/* GNU test program (ksb and mjb) */

/* Modified to run with the GNU shell by bfox. */

/* Copyright (C) 1987-2004 Free Software Foundation, Inc.

   This file is part of GNU Bash, the Bourne Again SHell.

   Bash is free software; you can redistribute it and/or modify it under
   the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 2, or (at your option) any later
   version.

   Bash is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
   for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* Define TEST_STANDALONE to get the /bin/test version.  Otherwise, you get
   the shell builtin version. */

#include <config.h>
#include <stdio.h>
#include <sys/types.h>

#define TEST_STANDALONE 1

#ifndef LBRACKET
# define LBRACKET 0
#endif

/* The official name of this program (e.g., no `g' prefix).  */
#if LBRACKET
# define PROGRAM_NAME "["
#else
# define PROGRAM_NAME "test"
#endif

#include "system.h"
#include "error.h"
#include "euidaccess.h"
#include "quote.h"

#ifndef _POSIX_VERSION
# include <sys/param.h>
#endif /* _POSIX_VERSION */
#define whitespace(c) (((c) == ' ') || ((c) == '\t'))
#define digit(c)  ((c) >= '0' && (c) <= '9')
#define digit_value(c) ((c) - '0')

char *program_name;

#if !defined (_POSIX_VERSION)
# include <sys/file.h>
#endif /* !_POSIX_VERSION */

extern gid_t getegid ();
extern uid_t geteuid ();

/* Exit status for syntax errors, etc.  */
enum { TEST_TRUE, TEST_FALSE, TEST_FAILURE };

#if defined (TEST_STANDALONE)
# define test_exit(val) exit (val)
#else
   static jmp_buf test_exit_buf;
   static int test_error_return = 0;
# define test_exit(val) test_error_return = val, longjmp (test_exit_buf, 1)
#endif /* !TEST_STANDALONE */

static int pos;		/* The offset of the current argument in ARGV. */
static int argc;	/* The number of arguments present in ARGV. */
static char **argv;	/* The argument list. */

static bool test_unop (char const *s);
static bool binop (char *s);
static bool unary_operator (void);
static bool binary_operator (bool);
static bool two_arguments (void);
static bool three_arguments (void);
static bool posixtest (int);

static bool expr (void);
static bool term (void);
static bool and (void);
static bool or (void);

static void test_syntax_error (char const *format, char const *arg)
     ATTRIBUTE_NORETURN;
static void beyond (void) ATTRIBUTE_NORETURN;

static void
test_syntax_error (char const *format, char const *arg)
{
  fprintf (stderr, "%s: ", argv[0]);
  fprintf (stderr, format, arg);
  fflush (stderr);
  test_exit (TEST_FAILURE);
}

/* Increment our position in the argument list.  Check that we're not
   past the end of the argument list.  This check is supressed if the
   argument is false.  */

static inline void
advance (bool f)
{
  ++pos;

  if (f && pos >= argc)
    beyond ();
}

static inline void
unary_advance (void)
{
  advance (true);
  ++pos;
}

/*
 * beyond - call when we're beyond the end of the argument list (an
 *	error condition)
 */
static void
beyond (void)
{
  test_syntax_error (_("missing argument after %s"), quote (argv[argc - 1]));
}

/* Syntax error for when an integer argument was expected, but
   something else was found. */
static void
integer_expected_error (char const *pch)
{
  test_syntax_error (_("%s: integer expression expected\n"), pch);
}

/* Return true if the characters pointed to by STRING constitute a
   valid number.  Stuff the converted number into RESULT if RESULT is
   not null.  */
static bool
is_int (register char *string, intmax_t *result)
{
  int sign;
  intmax_t value;

  sign = 1;
  value = 0;

  if (result)
    *result = 0;

  /* Skip leading whitespace characters. */
  while (whitespace (*string))
    string++;

  if (!*string)
    return false;

  /* We allow leading `-' or `+'. */
  if (*string == '-' || *string == '+')
    {
      if (!digit (string[1]))
	return false;

      if (*string == '-')
	sign = -1;

      string++;
    }

  while (digit (*string))
    {
      if (result)
	value = (value * 10) + digit_value (*string);
      string++;
    }

  /* Skip trailing whitespace, if any. */
  while (whitespace (*string))
    string++;

  /* Error if not at end of string. */
  if (*string)
    return false;

  if (result)
    {
      value *= sign;
      *result = value;
    }

  return true;
}

/* Find the modification time of FILE, and stuff it into *AGE.
   Return true if successful.  */
static bool
age_of (char *filename, time_t *age)
{
  struct stat finfo;
  bool ok = (stat (filename, &finfo) == 0);
  if (ok)
    *age = finfo.st_mtime;
  return ok;
}

/*
 * term - parse a term and return 1 or 0 depending on whether the term
 *	evaluates to true or false, respectively.
 *
 * term ::=
 *	'-'('h'|'d'|'f'|'r'|'s'|'w'|'c'|'b'|'p'|'u'|'g'|'k') filename
 *	'-'('L'|'x') filename
 *	'-t' int
 *	'-'('z'|'n') string
 *	string
 *	string ('!='|'=') string
 *	<int> '-'(eq|ne|le|lt|ge|gt) <int>
 *	file '-'(nt|ot|ef) file
 *	'(' <expr> ')'
 * int ::=
 *	'-l' string
 *	positive and negative integers
 */
static bool
term (void)
{
  bool value;
  bool negated = false;

  /* Deal with leading `not's.  */
  while (pos < argc && argv[pos][0] == '!' && argv[pos][1] == '\0')
    {
      advance (true);
      negated = !negated;
    }

  if (pos >= argc)
    beyond ();

  /* A paren-bracketed argument. */
  if (argv[pos][0] == '(' && argv[pos][1] == '\0')
    {
      int nargs;

      advance (true);

      for (nargs = 1;
	   pos + nargs < argc && ! STREQ (argv[pos + nargs], ")");
	   nargs++)
	if (nargs == 4)
	  {
	    nargs = argc - pos;
	    break;
	  }

      value = posixtest (nargs);
      if (argv[pos] == 0)
	test_syntax_error (_("')' expected\n"), NULL);
      else
        if (argv[pos][0] != ')' || argv[pos][1])
	  test_syntax_error (_("')' expected, found %s\n"), argv[pos]);
      advance (false);
    }

  /* Are there enough arguments left that this could be dyadic?  */
  else if (4 <= argc - pos && STREQ (argv[pos], "-l") && binop (argv[pos + 2]))
    value = binary_operator (true);
  else if (3 <= argc - pos && binop (argv[pos + 1]))
    value = binary_operator (false);

  /* It might be a switch type argument.  */
  else if (argv[pos][0] == '-' && argv[pos][1] && argv[pos][2] == '\0')
    {
      if (test_unop (argv[pos]))
	value = unary_operator ();
      else
	test_syntax_error (_("%s: unary operator expected\n"), argv[pos]);
    }
  else
    {
      value = (argv[pos][0] != '\0');
      advance (false);
    }

  return negated ^ value;
}

static bool
binary_operator (bool l_is_l)
{
  register int op;
  struct stat stat_buf, stat_spare;
  intmax_t l, r;
  /* Is the right integer expression of the form '-l string'? */
  bool r_is_l;

  if (l_is_l)
    advance (false);
  op = pos + 1;

  if ((op < argc - 2) && STREQ (argv[op + 1], "-l"))
    {
      r_is_l = true;
      advance (false);
    }
  else
    r_is_l = false;

  if (argv[op][0] == '-')
    {
      /* check for eq, nt, and stuff */
      switch (argv[op][1])
	{
	default:
	  break;

	case 'l':
	  if (argv[op][2] == 't' && !argv[op][3])
	    {
	      /* lt */
	      if (l_is_l)
		l = strlen (argv[op - 1]);
	      else
		{
		  if (!is_int (argv[op - 1], &l))
		    integer_expected_error (_("before -lt"));
		}

	      if (r_is_l)
		r = strlen (argv[op + 2]);
	      else
		{
		  if (!is_int (argv[op + 1], &r))
		    integer_expected_error (_("after -lt"));
		}
	      pos += 3;
	      return l < r;
	    }

	  if (argv[op][2] == 'e' && !argv[op][3])
	    {
	      /* le */
	      if (l_is_l)
		l = strlen (argv[op - 1]);
	      else
		{
		  if (!is_int (argv[op - 1], &l))
		    integer_expected_error (_("before -le"));
		}
	      if (r_is_l)
		r = strlen (argv[op + 2]);
	      else
		{
		  if (!is_int (argv[op + 1], &r))
		    integer_expected_error (_("after -le"));
		}
	      pos += 3;
	      return l <= r;
	    }
	  break;

	case 'g':
	  if (argv[op][2] == 't' && !argv[op][3])
	    {
	      /* gt integer greater than */
	      if (l_is_l)
		l = strlen (argv[op - 1]);
	      else
		{
		  if (!is_int (argv[op - 1], &l))
		    integer_expected_error (_("before -gt"));
		}
	      if (r_is_l)
		r = strlen (argv[op + 2]);
	      else
		{
		  if (!is_int (argv[op + 1], &r))
		    integer_expected_error (_("after -gt"));
		}
	      pos += 3;
	      return l > r;
	    }

	  if (argv[op][2] == 'e' && !argv[op][3])
	    {
	      /* ge - integer greater than or equal to */
	      if (l_is_l)
		l = strlen (argv[op - 1]);
	      else
		{
		  if (!is_int (argv[op - 1], &l))
		    integer_expected_error (_("before -ge"));
		}
	      if (r_is_l)
		r = strlen (argv[op + 2]);
	      else
		{
		  if (!is_int (argv[op + 1], &r))
		    integer_expected_error (_("after -ge"));
		}
	      pos += 3;
	      return l >= r;
	    }
	  break;

	case 'n':
	  if (argv[op][2] == 't' && !argv[op][3])
	    {
	      /* nt - newer than */
	      time_t lt, rt;
	      bool le, re;
	      pos += 3;
	      if (l_is_l | r_is_l)
		test_syntax_error (_("-nt does not accept -l\n"), NULL);
	      le = age_of (argv[op - 1], &lt);
	      re = age_of (argv[op + 1], &rt);
	      return le > re || (le && lt > rt);
	    }

	  if (argv[op][2] == 'e' && !argv[op][3])
	    {
	      /* ne - integer not equal */
	      if (l_is_l)
		l = strlen (argv[op - 1]);
	      else
		{
		  if (!is_int (argv[op - 1], &l))
		    integer_expected_error (_("before -ne"));
		}
	      if (r_is_l)
		r = strlen (argv[op + 2]);
	      else
		{
		  if (!is_int (argv[op + 1], &r))
		    integer_expected_error (_("after -ne"));
		}
	      pos += 3;
	      return l != r;
	    }
	  break;

	case 'e':
	  if (argv[op][2] == 'q' && !argv[op][3])
	    {
	      /* eq - integer equal */
	      if (l_is_l)
		l = strlen (argv[op - 1]);
	      else
		{
		  if (!is_int (argv[op - 1], &l))
		    integer_expected_error (_("before -eq"));
		}
	      if (r_is_l)
		r = strlen (argv[op + 2]);
	      else
		{
		  if (!is_int (argv[op + 1], &r))
		    integer_expected_error (_("after -eq"));
		}
	      pos += 3;
	      return l == r;
	    }

	  if (argv[op][2] == 'f' && !argv[op][3])
	    {
	      /* ef - hard link? */
	      pos += 3;
	      if (l_is_l | r_is_l)
		test_syntax_error (_("-ef does not accept -l\n"), NULL);
	      return (stat (argv[op - 1], &stat_buf) == 0
		      && stat (argv[op + 1], &stat_spare) == 0
		      && stat_buf.st_dev == stat_spare.st_dev
		      && stat_buf.st_ino == stat_spare.st_ino);
	    }
	  break;

	case 'o':
	  if ('t' == argv[op][2] && '\000' == argv[op][3])
	    {
	      /* ot - older than */
	      time_t lt, rt;
	      bool le, re;
	      pos += 3;
	      if (l_is_l | r_is_l)
		test_syntax_error (_("-ot does not accept -l\n"), NULL);
	      le = age_of (argv[op - 1], &lt);
	      re = age_of (argv[op + 1], &rt);
	      return le < re || (re && lt < rt);
	    }
	  break;
	}

      /* FIXME: is this dead code? */
      test_syntax_error (_("unknown binary operator\n"), argv[op]);
    }

  if (argv[op][0] == '=' && !argv[op][1])
    {
      bool value = STREQ (argv[pos], argv[pos + 2]);
      pos += 3;
      return value;
    }

  if (STREQ (argv[op], "!="))
    {
      bool value = !STREQ (argv[pos], argv[pos + 2]);
      pos += 3;
      return value;
    }

  /* Not reached.  */
  abort ();
}

static bool
unary_operator (void)
{
  struct stat stat_buf;

  switch (argv[pos][1])
    {
    default:
      return false;

      /* All of the following unary operators use unary_advance (), which
	 checks to make sure that there is an argument, and then advances
	 pos right past it.  This means that pos - 1 is the location of the
	 argument. */

    case 'a':			/* file exists in the file system? */
    case 'e':
      unary_advance ();
      return stat (argv[pos - 1], &stat_buf) == 0;

    case 'r':			/* file is readable? */
      unary_advance ();
      return euidaccess (argv[pos - 1], R_OK) == 0;

    case 'w':			/* File is writable? */
      unary_advance ();
      return euidaccess (argv[pos - 1], W_OK) == 0;

    case 'x':			/* File is executable? */
      unary_advance ();
      return euidaccess (argv[pos - 1], X_OK) == 0;

    case 'O':			/* File is owned by you? */
      unary_advance ();
      return (stat (argv[pos - 1], &stat_buf) == 0
	      && (geteuid () == stat_buf.st_uid));

    case 'G':			/* File is owned by your group? */
      unary_advance ();
      return (stat (argv[pos - 1], &stat_buf) == 0
	      && (getegid () == stat_buf.st_gid));

    case 'f':			/* File is a file? */
      unary_advance ();
      /* Under POSIX, -f is true if the given file exists
	 and is a regular file. */
      return (stat (argv[pos - 1], &stat_buf) == 0
	      && S_ISREG (stat_buf.st_mode));

    case 'd':			/* File is a directory? */
      unary_advance ();
      return (stat (argv[pos - 1], &stat_buf) == 0
	      && S_ISDIR (stat_buf.st_mode));

    case 's':			/* File has something in it? */
      unary_advance ();
      return (stat (argv[pos - 1], &stat_buf) == 0
	      && 0 < stat_buf.st_size);

    case 'S':			/* File is a socket? */
      unary_advance ();
      return (stat (argv[pos - 1], &stat_buf) == 0
	      && S_ISSOCK (stat_buf.st_mode));

    case 'c':			/* File is character special? */
      unary_advance ();
      return (stat (argv[pos - 1], &stat_buf) == 0
	      && S_ISCHR (stat_buf.st_mode));

    case 'b':			/* File is block special? */
      unary_advance ();
      return (stat (argv[pos - 1], &stat_buf) == 0
	      && S_ISBLK (stat_buf.st_mode));

    case 'p':			/* File is a named pipe? */
      unary_advance ();
      return (stat (argv[pos - 1], &stat_buf) == 0
	      && S_ISFIFO (stat_buf.st_mode));

    case 'L':			/* Same as -h  */
      /*FALLTHROUGH*/

    case 'h':			/* File is a symbolic link? */
      unary_advance ();
      return (lstat (argv[pos - 1], &stat_buf) == 0
	      && S_ISLNK (stat_buf.st_mode));

    case 'u':			/* File is setuid? */
      unary_advance ();
      return (stat (argv[pos - 1], &stat_buf) == 0
	      && (stat_buf.st_mode & S_ISUID));

    case 'g':			/* File is setgid? */
      unary_advance ();
      return (stat (argv[pos - 1], &stat_buf) == 0
	      && (stat_buf.st_mode & S_ISGID));

    case 'k':			/* File has sticky bit set? */
      unary_advance ();
      return (stat (argv[pos - 1], &stat_buf) == 0
	      && (stat_buf.st_mode & S_ISVTX));

    case 't':			/* File (fd) is a terminal? */
      {
	intmax_t fd;
	unary_advance ();
	if (!is_int (argv[pos - 1], &fd))
	  integer_expected_error (_("after -t"));
	return INT_MIN <= fd && fd <= INT_MAX && isatty (fd);
      }

    case 'n':			/* True if arg has some length. */
      unary_advance ();
      return argv[pos - 1][0] != 0;

    case 'z':			/* True if arg has no length. */
      unary_advance ();
      return argv[pos - 1][0] == '\0';
    }
}

/*
 * and:
 *	term
 *	term '-a' and
 */
static bool
and (void)
{
  bool value = true;

  for (;;)
    {
      value &= term ();
      if (! (pos < argc && STREQ (argv[pos], "-a")))
	return value;
      advance (false);
    }
}

/*
 * or:
 *	and
 *	and '-o' or
 */
static bool
or (void)
{
  bool value = false;

  for (;;)
    {
      value |= and ();
      if (! (pos < argc && STREQ (argv[pos], "-o")))
	return value;
      advance (false);
    }
}

/*
 * expr:
 *	or
 */
static bool
expr (void)
{
  if (pos >= argc)
    beyond ();

  return or ();		/* Same with this. */
}

/* Return true if S is one of the test command's binary operators.  */
static bool
binop (char *s)
{
  return ((STREQ (s,   "=")) || (STREQ (s,  "!=")) || (STREQ (s, "-nt")) ||
	  (STREQ (s, "-ot")) || (STREQ (s, "-ef")) || (STREQ (s, "-eq")) ||
	  (STREQ (s, "-ne")) || (STREQ (s, "-lt")) || (STREQ (s, "-le")) ||
	  (STREQ (s, "-gt")) || (STREQ (s, "-ge")));
}

/* Return true if OP is one of the test command's unary operators. */
static bool
test_unop (char const *op)
{
  if (op[0] != '-')
    return false;

  switch (op[1])
    {
    case 'a': case 'b': case 'c': case 'd': case 'e':
    case 'f': case 'g': case 'h': case 'k': case 'n':
    case 'o': case 'p': case 'r': case 's': case 't':
    case 'u': case 'w': case 'x': case 'z':
    case 'G': case 'L': case 'O': case 'S': case 'N':
      return true;
    }

  return false;
}

static bool
one_argument (void)
{
  return argv[pos++][0] != '\0';
}

static bool
two_arguments (void)
{
  bool value;

  if (STREQ (argv[pos], "!"))
    {
      advance (false);
      value = ! one_argument ();
    }
  else if (argv[pos][0] == '-'
	   && argv[pos][1] != '\0'
	   && argv[pos][2] == '\0')
    {
      if (test_unop (argv[pos]))
	value = unary_operator ();
      else
	test_syntax_error (_("%s: unary operator expected\n"), argv[pos]);
    }
  else
    beyond ();
  return (value);
}

static bool
three_arguments (void)
{
  bool value;

  if (binop (argv[pos + 1]))
    value = binary_operator (false);
  else if (STREQ (argv[pos], "!"))
    {
      advance (true);
      value = !two_arguments ();
    }
  else if (STREQ (argv[pos], "(") && STREQ (argv[pos + 2], ")"))
    {
      advance (false);
      value = one_argument ();
      advance (false);
    }
  else if (STREQ (argv[pos + 1], "-a") || STREQ (argv[pos + 1], "-o"))
    value = expr ();
  else
    test_syntax_error (_("%s: binary operator expected\n"), argv[pos+1]);
  return (value);
}

/* This is an implementation of a Posix.2 proposal by David Korn. */
static bool
posixtest (int nargs)
{
  bool value;

  switch (nargs)
    {
      case 1:
	value = one_argument ();
	break;

      case 2:
	value = two_arguments ();
	break;

      case 3:
	value = three_arguments ();
	break;

      case 4:
	if (STREQ (argv[pos], "!"))
	  {
	    advance (true);
	    value = !three_arguments ();
	    break;
	  }
	if (STREQ (argv[pos], "(") && STREQ (argv[pos + 3], ")"))
	  {
	    advance (false);
	    value = two_arguments ();
	    advance (false);
	    break;
	  }
	/* FALLTHROUGH */
      case 5:
      default:
	if (nargs <= 0)
	  abort ();
	value = expr ();
    }

  return (value);
}

#if defined (TEST_STANDALONE)
# include "long-options.h"

void
usage (int status)
{
  if (status != EXIT_SUCCESS)
    fprintf (stderr, _("Try `%s --help' for more information.\n"),
	     program_name);
  else
    {
      fputs (_("\
Usage: test EXPRESSION\n\
  or:  test\n\
  or:  [ EXPRESSION ]\n\
  or:  [ ]\n\
  or:  [ OPTION\n\
Exit with the status determined by EXPRESSION.\n\
\n\
"), stdout);
      fputs (HELP_OPTION_DESCRIPTION, stdout);
      fputs (VERSION_OPTION_DESCRIPTION, stdout);
      fputs (_("\
\n\
An omitted EXPRESSION defaults to false.  Otherwise,\n\
EXPRESSION is true or false and sets exit status.  It is one of:\n\
"), stdout);
      fputs (_("\
\n\
  ( EXPRESSION )               EXPRESSION is true\n\
  ! EXPRESSION                 EXPRESSION is false\n\
  EXPRESSION1 -a EXPRESSION2   both EXPRESSION1 and EXPRESSION2 are true\n\
  EXPRESSION1 -o EXPRESSION2   either EXPRESSION1 or EXPRESSION2 is true\n\
"), stdout);
      fputs (_("\
\n\
  -n STRING            the length of STRING is nonzero\n\
  STRING               equivalent to -n STRING\n\
  -z STRING            the length of STRING is zero\n\
  STRING1 = STRING2    the strings are equal\n\
  STRING1 != STRING2   the strings are not equal\n\
"), stdout);
      fputs (_("\
\n\
  INTEGER1 -eq INTEGER2   INTEGER1 is equal to INTEGER2\n\
  INTEGER1 -ge INTEGER2   INTEGER1 is greater than or equal to INTEGER2\n\
  INTEGER1 -gt INTEGER2   INTEGER1 is greater than INTEGER2\n\
  INTEGER1 -le INTEGER2   INTEGER1 is less than or equal to INTEGER2\n\
  INTEGER1 -lt INTEGER2   INTEGER1 is less than INTEGER2\n\
  INTEGER1 -ne INTEGER2   INTEGER1 is not equal to INTEGER2\n\
"), stdout);
      fputs (_("\
\n\
  FILE1 -ef FILE2   FILE1 and FILE2 have the same device and inode numbers\n\
  FILE1 -nt FILE2   FILE1 is newer (modification date) than FILE2\n\
  FILE1 -ot FILE2   FILE1 is older than FILE2\n\
"), stdout);
      fputs (_("\
\n\
  -b FILE     FILE exists and is block special\n\
  -c FILE     FILE exists and is character special\n\
  -d FILE     FILE exists and is a directory\n\
  -e FILE     FILE exists\n\
"), stdout);
      fputs (_("\
  -f FILE     FILE exists and is a regular file\n\
  -g FILE     FILE exists and is set-group-ID\n\
  -G FILE     FILE exists and is owned by the effective group ID\n\
  -h FILE     FILE exists and is a symbolic link (same as -L)\n\
  -k FILE     FILE exists and has its sticky bit set\n\
"), stdout);
      fputs (_("\
  -L FILE     FILE exists and is a symbolic link (same as -h)\n\
  -O FILE     FILE exists and is owned by the effective user ID\n\
  -p FILE     FILE exists and is a named pipe\n\
  -r FILE     FILE exists and read permission is granted\n\
  -s FILE     FILE exists and has a size greater than zero\n\
"), stdout);
      fputs (_("\
  -S FILE     FILE exists and is a socket\n\
  -t FD       file descriptor FD is opened on a terminal\n\
  -u FILE     FILE exists and its set-user-ID bit is set\n\
  -w FILE     FILE exists and write permission is granted\n\
  -x FILE     FILE exists and execute (or search) permission is granted\n\
"), stdout);
      fputs (_("\
\n\
Except for -h and -L, all FILE-related tests dereference symbolic links.\n\
Beware that parentheses need to be escaped (e.g., by backslashes) for shells.\n\
INTEGER may also be -l STRING, which evaluates to the length of STRING.\n\
"), stdout);
      printf (_("\nReport bugs to <%s>.\n"), PACKAGE_BUGREPORT);
    }
  exit (status);
}
#endif /* TEST_STANDALONE */

#if !defined (TEST_STANDALONE)
# define main test_command
#endif

#define AUTHORS "Kevin Braunsdorf", "Matthew Bradburn"

/*
 * [:
 *	'[' expr ']'
 * test:
 *	test expr
 */
int
main (int margc, char **margv)
{
  bool value;

#if !defined (TEST_STANDALONE)
  int code;

  code = setjmp (test_exit_buf);

  if (code)
    return (test_error_return);
#else /* TEST_STANDALONE */
  initialize_main (&margc, &margv);
  program_name = margv[0];
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  initialize_exit_failure (TEST_FAILURE);
  atexit (close_stdout);
#endif /* TEST_STANDALONE */

  argv = margv;

  if (LBRACKET)
    {
      /* Recognize --help or --version, but only when invoked in the
	 "[" form, and when the last argument is not "]".  POSIX
	 allows "[ --help" and "[ --version" to have the usual GNU
	 behavior, but it requires "test --help" and "test --version"
	 to exit silently with status 1.  */
      if (margc < 2 || !STREQ (margv[margc - 1], "]"))
	{
	  parse_long_options (margc, margv, PROGRAM_NAME, GNU_PACKAGE, VERSION,
			      usage, AUTHORS, (char const *) NULL);
	  test_syntax_error (_("missing `]'\n"), NULL);
	}

      --margc;
    }

  argc = margc;
  pos = 1;

  if (pos >= argc)
    test_exit (TEST_FALSE);

  value = posixtest (argc - 1);

  if (pos != argc)
    test_syntax_error (_("extra argument %s"), quote (argv[pos]));

  test_exit (value ? TEST_TRUE : TEST_FALSE);
}
