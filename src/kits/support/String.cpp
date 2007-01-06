/*
 * Copyright 2001-2007, Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Marc Flerackers (mflerackers@androme.be)
 *		Stefano Ceccherini (burton666@libero.it)
 *		Oliver Tappe (openbeos@hirschkaefer.de)
 *		Axel Dörfler, axeld@pinc-software.de
 */

/* String class supporting common string operations. */


#include <Debug.h>
#include <String.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>


// Set this to 1 to make some private methods inline
#define ENABLE_INLINES 0

// define proper names for case-option of _DoReplace()
#define KEEP_CASE false
#define IGNORE_CASE true

// define proper names for count-option of _DoReplace()
#define REPLACE_ALL 0x7FFFFFFF


// helper macro that is used to fall into debugger if a given param check fails:
#ifdef DEBUG
	#define CHECK_PARAM( expr, msg) \
	if (!(expr)) \
		debugger( msg)

	#define CHECK_PARAM_RET( expr, msg, retval) \
	if (!(expr)) \
		debugger( msg)

	#define CHECK_PARAM_VOID( expr, msg) \
	if (!(expr)) \
		debugger( msg)
#else
	#define CHECK_PARAM( expr, msg) \
	if (!(expr)) \
		return *this

	#define CHECK_PARAM_RET( expr, msg, retval) \
	if (!(expr)) \
		return (retval);

	#define CHECK_PARAM_VOID( expr, msg) 
#endif


//! helper class for BString::_ReplaceAtPositions():
class BString::PosVect {
	public:
		PosVect();
		~PosVect();

		bool Add(int32 pos);

		inline int32 ItemAt(int32 index) const
			{ return fBuffer[index]; }
		inline int32 CountItems() const
			{ return fSize; }

	private:
		int32	fSize;
		int32	fBufferSize;
		int32*	fBuffer;
};


const char *B_EMPTY_STRING = "";


// helper function, returns minimum of two given values (but clamps to 0):
static inline int32
min_clamp0(int32 num1, int32 num2) 
{ 
	if (num1 < num2)
		return num1 > 0 ? num1 : 0;

	return num2 > 0 ? num2 : 0;
}


//! helper function, returns length of given string (but clamps to given maximum):
static inline int32
strlen_clamp(const char* str, int32 max) 
{
	// this should yield 0 for max<0:
	int32 length = 0;
	while (length < max && *str++) {
		length++;
	}
	return length;
}


//! helper function, massages given pointer into a legal c-string:
static inline const char * 
safestr(const char* str) 
{
	return str ? str : "";
}


//	#pragma mark -


BString::PosVect::PosVect()
	:
	fSize(0),
	fBufferSize(20),
	fBuffer(NULL)
{
}


BString::PosVect::~PosVect()
{
	free(fBuffer);
}


bool
BString::PosVect::Add(int32 pos)
{
	if (fBuffer == NULL || fSize == fBufferSize) {
		if (fBuffer != NULL)
			fBufferSize *= 2;

		int32* newBuffer = (int32 *)realloc(fBuffer, fBufferSize * sizeof(int32));
		if (newBuffer == NULL)
			return false;

		fBuffer = newBuffer;
	}

	fBuffer[fSize++] = pos;
	return true;
}


//	#pragma mark - BString


BString::BString()
	: fPrivateData(NULL)	
{
}


BString::BString(const char* string)
	: fPrivateData(NULL)
{
	if (string != NULL)
		_Init(string, strlen(string));
}


BString::BString(const BString &string)
	: fPrivateData(NULL)			
{
	_Init(string.String(), string.Length());
}


BString::BString(const char *string, int32 maxLength)
	: fPrivateData(NULL)		
{
	if (string != NULL)
		_Init(string, strlen_clamp(string, maxLength));
}


BString::~BString()
{
	if (fPrivateData)
		free(fPrivateData - sizeof(int32));
}


//	#pragma mark - Access

		
int32
BString::CountChars() const
{
	int32 count = 0;

	const char *start = fPrivateData;
	const char *end = fPrivateData + Length();

	while (start++ != end) {
		count++;

		// Jump to next UTF8 character
		for (; (*start & 0xc0) == 0x80; start++);
	}

	return count;
}


//	#pragma mark - Assignment


BString&
BString::operator=(const BString &string)
{
	if (&string != this) // Avoid auto-assignment
		_DoAssign(string.String(), string.Length());
	return *this;
}


BString&
BString::operator=(const char *str)
{
	if (str != NULL)
		_DoAssign(str, strlen(str));	
	else
		_Alloc(0);

	return *this;
}


BString&
BString::operator=(char c)
{
	_DoAssign(&c, 1);
	return *this;
}


BString&
BString::SetTo(const char *str, int32 maxLength)
{
	if (str != NULL)
		_DoAssign(str, strlen_clamp(str, maxLength));
	else
		_Alloc(0);

	return *this;
}


BString&
BString::SetTo(const BString &from)
{
	if (&from != this) {
		// Avoid auto-assignment
		_DoAssign(from.String(), from.Length());
	}
	return *this;
}


BString&
BString::Adopt(BString &from)
{
	if (&from == this) {
		// Avoid auto-adoption
		return *this;
	}

	if (fPrivateData)
		free(fPrivateData - sizeof(int32));

	/* "steal" the data from the given BString */
	fPrivateData = from.fPrivateData;
	from.fPrivateData = NULL;

	return *this;
}


BString&
BString::SetTo(const BString &string, int32 length)
{
	if (&string != this) // Avoid auto-assignment
		_DoAssign(string.String(), min_clamp0(length, string.Length()));
	return *this;
}


BString&
BString::Adopt(BString &from, int32 length)
{
	if (&from == this) // Avoid auto-adoption
		return *this;

	int32 len = min_clamp0(length, from.Length());

	if (fPrivateData)
		free(fPrivateData - sizeof(int32));

	/* "steal" the data from the given BString */
	fPrivateData = from.fPrivateData;
	from.fPrivateData = NULL;

	if (len < Length())
		_Alloc(len);

	return *this;
}


BString&
BString::SetTo(char c, int32 count)
{
	if (count < 0)
		count = 0;
	int32 curLen = Length();
	
	if (curLen == count || _GrowBy(count - curLen)) 
		memset(fPrivateData, c, count);
	return *this;	
}


//	#pragma mark - Substring copying


BString &
BString::CopyInto(BString &into, int32 fromOffset, int32 length) const
{
	if (&into != this) {
		CHECK_PARAM_RET(fromOffset >= 0, "'fromOffset' must not be negative!", 
						into);
		CHECK_PARAM_RET(fromOffset <= Length(), "'fromOffset' exceeds length!", 
						into);
		into.SetTo(String() + fromOffset, length);
	}
	return into;
}


void
BString::CopyInto(char *into, int32 fromOffset, int32 length) const
{
	if (into != NULL) {
		CHECK_PARAM_VOID(fromOffset >= 0, "'fromOffset' must not be negative!");
		CHECK_PARAM_VOID(fromOffset <= Length(), "'fromOffset' exceeds length!");
		int32 len = min_clamp0(length, Length() - fromOffset);
		memcpy(into, fPrivateData + fromOffset, len);
	}
}


//	#pragma mark - Appending


BString&
BString::operator+=(const char *str)
{
	if (str != NULL)
		_DoAppend(str, strlen(str));
	return *this;
}


BString&
BString::operator+=(char c)
{
	_DoAppend(&c, 1);
	return *this;
}


BString&
BString::Append(const BString &string, int32 length)
{
	_DoAppend(string.String(), min_clamp0(length, string.Length()));
	return *this;
}


BString&
BString::Append(const char *str, int32 length)
{
	if (str != NULL) {
		int32 len = strlen_clamp(str, length);
		_DoAppend(str, len);
	}	
	return *this;
}


BString&
BString::Append(char c, int32 count)
{
	int32 len = Length();
	if (count > 0 && _GrowBy(count))
		memset(fPrivateData + len, c, count);

	return *this;
}


//	#pragma mark - Prepending


BString&
BString::Prepend(const char *str)
{
	if (str != NULL)
		_DoPrepend(str, strlen(str));
	return *this;
}


// Prepend
BString&
BString::Prepend(const BString &string)
{
	if (&string != this)
		_DoPrepend(string.String(), string.Length());
	return *this;
}


// Prepend
BString&
BString::Prepend(const char *str, int32 length)
{
	if (str != NULL) {
		int32 len = strlen_clamp(str, length);
		_DoPrepend(str, len);
	}
	return *this;
}


// Prepend
BString&
BString::Prepend(const BString &string, int32 len)
{
	if (&string != this)
		_DoPrepend(string.String(), min_clamp0(len, string.Length()));
	return *this;
}


// Prepend
BString&
BString::Prepend(char c, int32 count)
{
	if (count > 0 && _OpenAtBy(0, count))
		memset(fPrivateData, c, count);
	
	return *this;
}


//	#pragma mark - Inserting


BString&
BString::Insert(const char *str, int32 pos)
{
	if (str != NULL) {
		CHECK_PARAM(pos <= Length(), "'pos' exceeds length!");
		int32 len = (int32)strlen(str);
		if (pos < 0) {
			int32 skipLen = min_clamp0(-1 * pos, len);
			str += skipLen;
			len -= skipLen;
			pos = 0;
		} else
			pos = min_clamp0(pos, Length());
		if (_OpenAtBy(pos, len))
			memcpy(fPrivateData + pos, str, len);
	}
	return *this;
}


// Insert
BString&
BString::Insert(const char *str, int32 length, int32 pos)
{
	if (str != NULL) {
		CHECK_PARAM(pos <= Length(), "'pos' exceeds length!");
		int32 len = strlen_clamp(str, length);
		if (pos < 0) {
			int32 skipLen = min_clamp0(-1 * pos, len);
			str += skipLen;
			len -= skipLen;
			pos = 0;
		} else
			pos = min_clamp0(pos, Length());
		if (_OpenAtBy(pos, len))
			memcpy(fPrivateData + pos, str, len);
	}
	return *this;
}


// Insert
BString&
BString::Insert(const char *str, int32 fromOffset, int32 length, int32 pos)
{
	CHECK_PARAM(fromOffset >= 0, "'fromOffset' must not be negative!");
	return Insert(str + fromOffset, length, pos);
}


// Insert
BString&
BString::Insert(const BString &string, int32 pos)
{
	if (&string != this)
		Insert(string.String(), pos); //TODO: Optimize
	return *this;				  
}


// Insert
BString&
BString::Insert(const BString &string, int32 length, int32 pos)
{
	if (&string != this)
		Insert(string.String(), length, pos); //TODO: Optimize
	return *this;
}


// Insert
BString&
BString::Insert(const BString &string, int32 fromOffset, int32 length, int32 pos)
{
	if (&string != this) {
		CHECK_PARAM(fromOffset >= 0, "'fromOffset' must not be negative!");
		Insert(string.String() + fromOffset, length, pos);
	}
	return *this;
}


// Insert
BString&
BString::Insert(char c, int32 count, int32 pos)
{
	CHECK_PARAM(pos <= Length(), "'pos' exceeds length!");
	if (pos < 0) {
		count = max_c(count + pos, 0);
		pos = 0;
	} else
		pos = min_clamp0(pos, Length());
	
	if (count > 0 && _OpenAtBy(pos, count))
		memset(fPrivateData + pos, c, count);
	
	return *this;
}


//	#pragma mark - Removing


BString&
BString::Truncate(int32 newLength, bool lazy)
{
	if (newLength < 0)
		newLength = 0;

	int32 curLen = Length();

	if (newLength < curLen) {
		if (lazy) {
			// don't free memory yet, just set new length
			_SetLength(newLength);
			fPrivateData[newLength] = '\0';
		} else
			_Alloc(newLength);
	}

	return *this;
}


// Remove
BString&
BString::Remove(int32 from, int32 length)
{
	int32 len = Length();
	if (from < 0) {
		int32 skipLen = min_clamp0(from, len);
		len -= skipLen;
		from = 0;
	} else
		from = min_clamp0(from, len);
	_ShrinkAtBy(from, min_clamp0(length, len - from));
	return *this;
}


// Remove
BString&
BString::RemoveFirst(const BString &string)
{
	if (string.Length() > 0) {
		int32 pos = _ShortFindAfter(string.String(), string.Length());
		if (pos >= 0)
			_ShrinkAtBy(pos, string.Length());
	}
	return *this;
}


// Remove
BString&
BString::RemoveLast(const BString &string)
{
	int32 pos = _FindBefore(string.String(), Length(), string.Length());
	if (pos >= 0)
		_ShrinkAtBy(pos, string.Length());

	return *this;
}


// Remove
BString&
BString::RemoveAll(const BString &string)
{
	return _DoReplace(string.String(), "", REPLACE_ALL, 0, KEEP_CASE);
}


// Remove
BString&
BString::RemoveFirst(const char *string)
{
	int32 length = string ? strlen(string) : 0;
	if (length > 0) {
		int32 pos = _ShortFindAfter(string, length);
		if (pos >= 0)
			_ShrinkAtBy(pos, length);
	}
	return *this;
}


// Remove
BString&
BString::RemoveLast(const char *string)
{
	int32 length = string ? strlen(string) : 0;
	if (length > 0) {
		int32 pos = _FindBefore(string, Length(), length);
		if (pos >= 0)
			_ShrinkAtBy(pos, length);
	}
	return *this;
}


// Remove
BString&
BString::RemoveAll(const char *str)
{
	return _DoReplace(str, "", REPLACE_ALL, 0, KEEP_CASE);
}


// Remove
BString&
BString::RemoveSet(const char *setOfCharsToRemove)
{
	return ReplaceSet(setOfCharsToRemove, "");
}


// MoveInto
BString&
BString::MoveInto(BString &into, int32 from, int32 length)
{
	CHECK_PARAM_RET(from >= 0, "'from' must not be negative!", into);
	CHECK_PARAM_RET(from <= Length(), "'from' exceeds length!", into);
	int32 len = min_clamp0(length, Length() - from);
	if (&into == this) {
		/* TODO: [zooey]: to be activated later (>R1):
		// strings are identical, just move the data:
		if (from>0 && fPrivateData)
			memmove( fPrivateData, fPrivateData+from, len);
		Truncate( len);
		*/
		return *this;
	}
	into.SetTo(String() + from, len);
	_ShrinkAtBy(from, len);

	return into;
}


// MoveInto
void
BString::MoveInto(char *into, int32 from, int32 length)
{
	if (into != NULL) {
		CHECK_PARAM_VOID(from >= 0, "'from' must not be negative!");
		CHECK_PARAM_VOID(from <= Length(), "'from' exceeds length!");
		int32 len = min_clamp0(length, Length() - from);
		memcpy(into, String() + from, len);
		into[len] = '\0';
		_ShrinkAtBy(from, len);
	}
}


/*---- Compare functions ---------------------------------------------------*/
bool
BString::operator<(const char *string) const
{
	return strcmp(String(), safestr(string)) < 0;
}


bool
BString::operator<=(const char *string) const
{
	return strcmp(String(), safestr(string)) <= 0;
}


bool
BString::operator==(const char *string) const
{
	return strcmp(String(), safestr(string)) == 0;
}


bool
BString::operator>=(const char *string) const
{
	return strcmp(String(), safestr(string)) >= 0;
}


bool
BString::operator>(const char *string) const
{
	return strcmp(String(), safestr(string)) > 0;
}


//	#pragma mark - Comparison


int
BString::Compare(const BString &string) const
{
	return strcmp(String(), string.String());
}


int
BString::Compare(const char *string) const
{
	return strcmp(String(), safestr(string));
}


int
BString::Compare(const BString &string, int32 n) const
{
	return strncmp(String(), string.String(), n);
}


int
BString::Compare(const char *string, int32 n) const
{
	return strncmp(String(), safestr(string), n);
}


int
BString::ICompare(const BString &string) const
{
	return strcasecmp(String(), string.String());
}


int
BString::ICompare(const char *str) const
{
	return strcasecmp(String(), safestr(str));
}


int
BString::ICompare(const BString &string, int32 n) const
{
	return strncasecmp(String(), string.String(), n);
}


int
BString::ICompare(const char *str, int32 n) const
{
	return strncasecmp(String(), safestr(str), n);
}


//	#pragma mark - Searching


int32
BString::FindFirst(const BString &string) const
{
	return _ShortFindAfter(string.String(), string.Length());
}


// FindFirst
int32
BString::FindFirst(const char *string) const
{
	if (string == NULL)
		return B_BAD_VALUE;

	return _ShortFindAfter(string, strlen(string));
}


// FindFirst
int32
BString::FindFirst(const BString &string, int32 fromOffset) const
{
	if (fromOffset < 0)
		return B_ERROR;

	return _FindAfter(string.String(), min_clamp0(fromOffset, Length()),
		string.Length());
}


// FindFirst
int32
BString::FindFirst(const char *string, int32 fromOffset) const
{
	if (string == NULL)
		return B_BAD_VALUE;
	if (fromOffset < 0)
		return B_ERROR;

	return _FindAfter(string, min_clamp0(fromOffset, Length()),
		strlen(string));
}


// FindFirst
int32
BString::FindFirst(char c) const
{	
	const char *start = String();
	const char *end = String() + Length();

	/* Scans the string until we find the character, */
	/* or we hit the string's end */
	while (start != end && *start != c) {
		start++;
	}

	if (start == end)
		return B_ERROR;

	return start - String();
}


// FindFirst
int32
BString::FindFirst(char c, int32 fromOffset) const
{
	if (fromOffset < 0)
		return B_ERROR;

	const char *start = String() + min_clamp0(fromOffset, Length());
	const char *end = String() + Length();

	/* Scans the string until we found the character, */
	/* or we hit the string's end */
	while (start < end && *start != c) {
		start++;
	}

	if (start >= end)
		return B_ERROR;

	return start - String();
}


// FindLast
int32
BString::FindLast(const BString &string) const
{
	return _FindBefore(string.String(), Length(), string.Length());
}


// FindLast
int32
BString::FindLast(const char *string) const
{
	if (string == NULL)
		return B_BAD_VALUE;

	return _FindBefore(string, Length(), strlen(string));
}


// FindLast
int32
BString::FindLast(const BString &string, int32 beforeOffset) const
{
	if (beforeOffset < 0)
		return B_ERROR;

	return _FindBefore(string.String(), min_clamp0(beforeOffset, Length()), 
		string.Length()); 
}


// FindLast
int32
BString::FindLast(const char *string, int32 beforeOffset) const
{
	if (string == NULL)
		return B_BAD_VALUE;
	if (beforeOffset < 0)
		return B_ERROR;

	return _FindBefore(string, min_clamp0(beforeOffset, Length()), strlen(string));
}


// FindLast
int32
BString::FindLast(char c) const
{
	const char *start = String();
	const char *end = String() + Length();

	/* Scans the string backwards until we found the character, */
	/* or we reach the string's start */
	while (end != start && *end != c) {
		end--;
	}

	if (end == start)
		return B_ERROR;

	return end - String();
}


// FindLast
int32
BString::FindLast(char c, int32 beforeOffset) const
{
	if (beforeOffset < 0)
		return B_ERROR;

	const char *start = String();
	const char *end = String() + min_clamp0(beforeOffset, Length());

	/* Scans the string backwards until we found the character, */
	/* or we reach the string's start */
	while (end > start && *end != c) {
		end--;
	}

	if (end <= start)
		return B_ERROR;

	return end - String();
}


int32
BString::IFindFirst(const BString &string) const
{
	return _IFindAfter(string.String(), 0, string.Length());
}


int32
BString::IFindFirst(const char *string) const
{
	if (string == NULL)
		return B_BAD_VALUE;

	return _IFindAfter(string, 0, strlen(string));
}


int32
BString::IFindFirst(const BString &string, int32 fromOffset) const
{
	if (fromOffset < 0)
		return B_ERROR;

	return _IFindAfter(string.String(), min_clamp0(fromOffset, Length()), 
		string.Length());
}


int32
BString::IFindFirst(const char *string, int32 fromOffset) const
{
	if (string == NULL)
		return B_BAD_VALUE;
	if (fromOffset < 0)
		return B_ERROR;

	return _IFindAfter(string, min_clamp0(fromOffset,Length()), strlen(string));
}


int32
BString::IFindLast(const BString &string) const
{
	return _IFindBefore(string.String(), Length(), string.Length());
}


int32
BString::IFindLast(const char *string) const
{
	if (string == NULL)
		return B_BAD_VALUE;

	return _IFindBefore(string, Length(), strlen(string));
}


int32
BString::IFindLast(const BString &string, int32 beforeOffset) const
{
	if (beforeOffset < 0)
		return B_ERROR;

	return _IFindBefore(string.String(), min_clamp0(beforeOffset, Length()), 
		string.Length());
}


int32
BString::IFindLast(const char *string, int32 beforeOffset) const
{
	if (string == NULL)
		return B_BAD_VALUE;
	if (beforeOffset < 0)
		return B_ERROR;

	return _IFindBefore(string, min_clamp0(beforeOffset, Length()),
		strlen(string));
}


//	#pragma mark - Replacing


BString&
BString::ReplaceFirst(char replaceThis, char withThis)
{
	int32 pos = FindFirst(replaceThis);
	if (pos >= 0)
		fPrivateData[pos] = withThis;

	return *this;
}


BString&
BString::ReplaceLast(char replaceThis, char withThis)
{
	int32 pos = FindLast(replaceThis);
	if (pos >= 0)
		fPrivateData[pos] = withThis;

	return *this;
}


BString&
BString::ReplaceAll(char replaceThis, char withThis, int32 fromOffset)
{
	CHECK_PARAM(fromOffset >= 0, "'fromOffset' must not be negative!");
	for (int32 pos = min_clamp0(fromOffset, Length());;) {
		pos = FindFirst(replaceThis, pos);
		if (pos < 0)
			break;
		fPrivateData[pos] = withThis;
	}

	return *this;
}


BString&
BString::Replace(char replaceThis, char withThis, int32 maxReplaceCount, int32 fromOffset)
{
	CHECK_PARAM(fromOffset >= 0, "'fromOffset' must not be negative!");
	if (maxReplaceCount > 0) {
		for (int32 pos = min_clamp0(fromOffset, Length()); 
			  		maxReplaceCount > 0; maxReplaceCount--) {
			pos = FindFirst(replaceThis, pos);
			if (pos < 0)
				break;
			fPrivateData[pos] = withThis;
		}
	}
	return *this;
}


BString&
BString::ReplaceFirst(const char *replaceThis, const char *withThis)
{
	return _DoReplace( replaceThis, withThis, 1, 0, KEEP_CASE);
}


BString&
BString::ReplaceLast(const char *replaceThis, const char *withThis)
{
	if (replaceThis == NULL)
		return *this;
		
	int32 firstStringLength = strlen(replaceThis);	
	int32 pos = _FindBefore(replaceThis, Length(), firstStringLength);
	
	if (pos >= 0) {
		int32 len = (withThis ? strlen(withThis) : 0);
		int32 difference = len - firstStringLength;
		
		if (difference > 0) {
			if (!_OpenAtBy(pos, difference))
				return *this;
		} else if (difference < 0) {
			if (!_ShrinkAtBy(pos, -difference))
				return *this;
		}
		memcpy(fPrivateData + pos, withThis, len);
	}
		
	return *this;
}


BString&
BString::ReplaceAll(const char *replaceThis, const char *withThis, int32 fromOffset)
{
	CHECK_PARAM(fromOffset >= 0, "'fromOffset' must not be negative!");
	return _DoReplace(replaceThis, withThis, REPLACE_ALL,
		min_clamp0(fromOffset,Length()), KEEP_CASE);
}


BString&
BString::Replace(const char *replaceThis, const char *withThis, int32 maxReplaceCount, int32 fromOffset)
{
	CHECK_PARAM(fromOffset >= 0, "'fromOffset' must not be negative!");
	return _DoReplace(replaceThis, withThis, maxReplaceCount,
		min_clamp0(fromOffset,Length()), KEEP_CASE);
}


BString&
BString::IReplaceFirst(char replaceThis, char withThis)
{
	char tmp[2] = { replaceThis, '\0' };

	int32 pos = _IFindAfter(tmp, 0, 1);
	if (pos >= 0)
		fPrivateData[pos] = withThis;

	return *this;
}


BString&
BString::IReplaceLast(char replaceThis, char withThis)
{
	char tmp[2] = { replaceThis, '\0' };

	int32 pos = _IFindBefore(tmp, Length(), 1);
	if (pos >= 0)
		fPrivateData[pos] = withThis;
	
	return *this;
}


BString&
BString::IReplaceAll(char replaceThis, char withThis, int32 fromOffset)
{
	CHECK_PARAM(fromOffset >= 0, "'fromOffset' must not be negative!");

	char tmp[2] = { replaceThis, '\0' };

	for (int32 pos = min_clamp0(fromOffset, Length());;) {
		pos = _IFindAfter(tmp, pos, 1);
		if (pos < 0)
			break;
		fPrivateData[pos] = withThis;
	}
	return *this;
}


BString&
BString::IReplace(char replaceThis, char withThis, int32 maxReplaceCount, int32 fromOffset)
{
	CHECK_PARAM(fromOffset >= 0, "'fromOffset' must not be negative!");

	char tmp[2] = { replaceThis, '\0' };
	
	if (fPrivateData == NULL)
		return *this;
		
	for (int32 pos = min_clamp0(fromOffset,Length()); 
		  	maxReplaceCount > 0;   maxReplaceCount--) {	
		pos = _IFindAfter(tmp, pos, 1);
		if (pos < 0)
			break;
		fPrivateData[pos] = withThis;
	}
	return *this;
}


BString&
BString::IReplaceFirst(const char *replaceThis, const char *withThis)
{
	return _DoReplace(replaceThis, withThis, 1, 0, IGNORE_CASE);
}


BString&
BString::IReplaceLast(const char *replaceThis, const char *withThis)
{
	if (replaceThis == NULL)
		return *this;

	int32 firstStringLength = strlen(replaceThis);		
	int32 pos = _IFindBefore(replaceThis, Length(), firstStringLength);

	if (pos >= 0) {
		int32 len = (withThis ? strlen(withThis) : 0);
		int32 difference = len - firstStringLength;

		if (difference > 0) {
			if (!_OpenAtBy(pos, difference))
				return *this;
		} else if (difference < 0) {
			if (!_ShrinkAtBy(pos, -difference))
				return *this;
		}
		memcpy(fPrivateData + pos, withThis, len);
	}

	return *this;
}


BString&
BString::IReplaceAll(const char *replaceThis, const char *withThis, int32 fromOffset)
{
	CHECK_PARAM(fromOffset >= 0, "'fromOffset' must not be negative!");
	return _DoReplace(replaceThis, withThis, REPLACE_ALL,
		min_clamp0(fromOffset, Length()), IGNORE_CASE);
}


BString&
BString::IReplace(const char *replaceThis, const char *withThis,
	int32 maxReplaceCount, int32 fromOffset)
{
	CHECK_PARAM(fromOffset >= 0, "'fromOffset' must not be negative!");
	return _DoReplace(replaceThis, withThis, maxReplaceCount,
		min_clamp0(fromOffset, Length()), IGNORE_CASE);
}


BString&
BString::ReplaceSet(const char *setOfChars, char with)
{
	if (setOfChars == NULL)
		return *this;

	int32 offset = 0;
	int32 length = Length();

	for (int32 pos;;) {
		pos = strcspn(String() + offset, setOfChars);

		offset += pos;
		if (offset >= length)
			break;

		fPrivateData[offset] = with;
		offset++;
	}

	return *this;
}


BString&
BString::ReplaceSet(const char *setOfChars, const char *with)
{
	int32 withLen = with ? strlen(with) : 0;
	if (withLen == 1) {
		// delegate simple case:
		return ReplaceSet(setOfChars, *with);
	}

	if (setOfChars == NULL || fPrivateData == NULL)
		return *this;

	PosVect positions;

	int32 searchLen = 1;
	int32 len = Length();
	int32 pos = 0;
	for (int32 offset = 0; offset < len; offset += (pos+searchLen)) {
		pos = strcspn(fPrivateData + offset, setOfChars);
		if (pos + offset >= len)
			break;
		if (!positions.Add(offset + pos))
			return *this;
	}

	_ReplaceAtPositions(&positions, searchLen, with, withLen);	
	return *this;
}


/*---- Unchecked char access -----------------------------------------------*/

// operator[]
char &
BString::operator[](int32 index)
{
	return fPrivateData[index];
}


/*---- Fast low-level manipulation -----------------------------------------*/
char*
BString::LockBuffer(int32 maxLength)
{
	_SetUsingAsCString(true);

	int32 len = Length();

	if (maxLength > len) {
		if (!_GrowBy(maxLength - len))
			return NULL;
		if (!len && fPrivateData)
			// if string was empty before call to LockBuffer(), we make sure the
			// buffer represents an empty c-string:
			*fPrivateData = '\0';
	} else if (!maxLength && !len) {
		// special case for unallocated string, we return an empty c-string:
		return const_cast<char*>(String());
	}

	return fPrivateData;
}


BString&
BString::UnlockBuffer(int32 length)
{
	_SetUsingAsCString(false);

	if (length < 0)
		length = (fPrivateData == NULL) ? 0 : strlen(fPrivateData);

	if (length != Length())
		_GrowBy(length - Length());

	return *this;
}


/*---- Uppercase<->Lowercase ------------------------------------------------*/
// ToLower
BString&
BString::ToLower()
{
	int32 length = Length();
	for (int32 count = 0; count < length; count++) {
		fPrivateData[count] = tolower(fPrivateData[count]);
	}

	return *this;
}


// ToUpper
BString&
BString::ToUpper()
{			
	int32 length = Length();
	for (int32 count = 0; count < length; count++) {
		fPrivateData[count] = toupper(fPrivateData[count]);
	}

	return *this;
}


// Capitalize
BString&
BString::Capitalize()
{
	if (fPrivateData == NULL)
		return *this;

	fPrivateData[0] = toupper(fPrivateData[0]);
	int32 length = Length();

	for (int32 count = 1; count < length; count++) {
		fPrivateData[count] = tolower(fPrivateData[count]);
	}

	return *this;
}


// CapitalizeEachWord
BString&
BString::CapitalizeEachWord()
{
	if (fPrivateData == NULL)
		return *this;
		
	int32 count = 0;
	int32 length = Length();
		
	do {
		// Find the first alphabetical character...
		for (; count < length; count++) {
			if (isalpha(fPrivateData[count])) {
				// ...found! Convert it to uppercase.
				fPrivateData[count] = toupper(fPrivateData[count]);
				count++;
				break;
			}
		}

		// Now find the first non-alphabetical character,
		// and meanwhile, turn to lowercase all the alphabetical ones
		for (; count < length; count++) {
			if (isalpha(fPrivateData[count]))
				fPrivateData[count] = tolower(fPrivateData[count]);
			else
				break;
		}
	} while (count < length);

	return *this;
}


/*----- Escaping and Deescaping --------------------------------------------*/
BString&
BString::CharacterEscape(const char *original, const char *setOfCharsToEscape,
	char escapeWith)
{
	SetTo(original);
	CharacterEscape(setOfCharsToEscape, escapeWith);

	return *this;
}


BString&
BString::CharacterEscape(const char *setOfCharsToEscape, char escapeWith)
{
	if (setOfCharsToEscape == NULL || fPrivateData == NULL)
		return *this;

	PosVect positions;
	int32 len = Length();
	int32 pos = 0;
	for (int32 offset = 0; offset < len; offset += pos + 1) {
		if ((pos = strcspn(fPrivateData + offset, setOfCharsToEscape)) < len - offset) {
			if (!positions.Add(offset + pos))
				return *this;
		}
	}

	uint32 count = positions.CountItems();
	int32 newLength = len + count;
	if (!newLength) {
		_Alloc(0);
		return *this;
	}

	int32 lastPos = 0;
	char* oldAdr = fPrivateData;
	char* newData = (char*)malloc(newLength + sizeof(int32) + 1);
	if (newData) {
		newData += sizeof(int32);
		char* newAdr = newData;
		for (uint32 i = 0; i < count; ++i) {
			pos = positions.ItemAt( i);
			len = pos-lastPos;
			if (len > 0) {
				memcpy(newAdr, oldAdr, len);
				oldAdr += len;
				newAdr += len;
			}
			*newAdr++ = escapeWith;
			*newAdr++ = *oldAdr++;
			lastPos = pos + 1;
		}
		len = Length() + 1 - lastPos;
		if (len > 0)
			memcpy(newAdr, oldAdr, len);

		free(fPrivateData - sizeof(int32));
		fPrivateData = newData;
		fPrivateData[newLength] = 0;
		_SetLength( newLength);
	}

	return *this;
}


BString&
BString::CharacterDeescape(const char *original, char escapeChar)
{
	SetTo(original);	
	CharacterDeescape(escapeChar);

	return *this;
}


BString&
BString::CharacterDeescape(char escapeChar)
{
	const char temp[2] = {escapeChar, 0};
	return _DoReplace(temp, "", REPLACE_ALL, 0, KEEP_CASE);
}


/*---- Simple sprintf replacement calls ------------------------------------*/
/*---- Slower than sprintf but type and overflow safe ----------------------*/
BString&
BString::operator<<(const char *str)
{
	if (str != NULL)
		_DoAppend(str, strlen(str));
	return *this;	
}


BString&
BString::operator<<(const BString &string)
{
	_DoAppend(string.String(), string.Length());
	return *this;
}


BString&
BString::operator<<(char c)
{
	_DoAppend(&c, 1);	
	return *this;
}


BString&
BString::operator<<(int i)
{
	char num[32];
	int32 length = snprintf(num, sizeof(num), "%d", i);

	_DoAppend(num, length);
	return *this;
}


BString&
BString::operator<<(unsigned int i)
{
	char num[32];
	int32 length = snprintf(num, sizeof(num), "%u", i);

	_DoAppend(num, length);
	return *this;
}


BString&
BString::operator<<(uint32 i)
{
	char num[32];
	int32 length = snprintf(num, sizeof(num), "%lu", i);

	_DoAppend(num, length);
	return *this;
}


BString&
BString::operator<<(int32 i)
{
	char num[32];
	int32 length = snprintf(num, sizeof(num), "%ld", i);

	_DoAppend(num, length);
	return *this;
}


BString&
BString::operator<<(uint64 i)
{
	char num[64];
	int32 length = snprintf(num, sizeof(num), "%llu", i);

	_DoAppend(num, length);
	return *this;
}


BString&
BString::operator<<(int64 i)
{
	char num[64];
	int32 length = snprintf(num, sizeof(num), "%lld", i);

	_DoAppend(num, length);
	return *this;
}


BString&
BString::operator<<(float f)
{
	char num[64];
	int32 length = snprintf(num, sizeof(num), "%.2f", f);

	_DoAppend(num, length);
	return *this;
}


//	#pragma mark - Private or reserved


char *
BString::_Alloc(int32 dataLength)
{
	char *dataPtr = fPrivateData ? fPrivateData - sizeof(int32) : NULL;
	if (dataLength <= 0) {
		// release buffer
#if 0
		free(dataPtr);
		fPrivateData = NULL;
		return NULL;
#else
		// TODO: think about removing this work-around again; it lets
		//	BeOS R5 NetPositive run on Haiku - this is obviously ignoring
		//	the fact, that fPrivateData could be NULL at one point
		//	(while building the menus from resources).
		dataLength = 0;
#endif
	}

	int32 allocLength = dataLength + sizeof(int32) + 1;
	dataPtr = (char *)realloc(dataPtr, allocLength);
	if (dataPtr) {
		dataPtr += sizeof(int32);
		fPrivateData = dataPtr;

		_SetLength(dataLength);
		fPrivateData[dataLength] = '\0';
	}

	return dataPtr;
}	

void
BString::_Init(const char *str, int32 len)
{
	if (_Alloc(len))
		memcpy(fPrivateData, str, len);
}


#if ENABLE_INLINES
inline
#endif
void
BString::_DoAssign(const char *str, int32 len)
{
	int32 curLen = Length();

	if (len == curLen || _GrowBy(len - curLen))
		memcpy(fPrivateData, str, len);
}


#if ENABLE_INLINES
inline
#endif
void
BString::_DoAppend(const char *str, int32 len)
{
	int32 length = Length();
	if (_GrowBy(len))
		memcpy(fPrivateData + length, str, len);
}


char*
BString::_GrowBy(int32 size)
{
	return _Alloc(Length() + size);
}


char *
BString::_OpenAtBy(int32 offset, int32 length)
{
	int32 oldLength = Length();

	char* newData = _Alloc(oldLength + length);
	if (newData != NULL) {
		memmove(fPrivateData + offset + length, fPrivateData + offset,
			oldLength - offset);
	}

	return newData;
}


char*
BString::_ShrinkAtBy(int32 offset, int32 length)
{	
	if (!fPrivateData)
		return NULL;

	int32 oldLength = Length();

	memmove(fPrivateData + offset, fPrivateData + offset + length,
		oldLength - offset - length);

	// the following actually should never fail, since we are reducing the size...
	return _Alloc(oldLength - length);
}


#if ENABLE_INLINES
inline
#endif
void
BString::_DoPrepend(const char *str, int32 count)
{
	if (_OpenAtBy(0, count))
		memcpy(fPrivateData, str, count);
}


/* XXX: These could be inlined too, if they are too slow */
int32
BString::_FindAfter(const char *str, int32 offset, int32 strlen) const
{	
	char *ptr = strstr(String() + offset, str);

	if (ptr != NULL)
		return ptr - String();
	
	return B_ERROR;
}


int32
BString::_IFindAfter(const char *str, int32 offset, int32 strlen) const
{
	char *ptr = strcasestr(String() + offset, str);

	if (ptr != NULL)
		return ptr - String();

	return B_ERROR;
}


int32
BString::_ShortFindAfter(const char *str, int32 len) const
{
	char *ptr = strstr(String(), str);
	
	if (ptr != NULL)
		return ptr - String();
		
	return B_ERROR;
}


int32
BString::_FindBefore(const char *str, int32 offset, int32 strlen) const
{
	if (fPrivateData) {
		const char *ptr = fPrivateData + offset - strlen;
		
		while (ptr >= fPrivateData) {	
			if (!memcmp(ptr, str, strlen))
				return ptr - fPrivateData; 
			ptr--;
		}
	}
	return B_ERROR;
}


int32
BString::_IFindBefore(const char *str, int32 offset, int32 strlen) const
{
	if (fPrivateData) {
		char *ptr1 = fPrivateData + offset - strlen;
		
		while (ptr1 >= fPrivateData) {
			if (!strncasecmp(ptr1, str, strlen))
				return ptr1 - fPrivateData; 
			ptr1--;
		}
	}
	return B_ERROR;
}


BString&
BString::_DoReplace(const char *findThis, const char *replaceWith,
	int32 maxReplaceCount, int32 fromOffset, bool ignoreCase)
{
	if (findThis == NULL || maxReplaceCount <= 0 
		|| fromOffset < 0 || fromOffset >= Length())
		return *this;

	typedef int32 (BString::*TFindMethod)(const char *, int32, int32) const;
	TFindMethod findMethod = ignoreCase ? &BString::_IFindAfter : &BString::_FindAfter;
	int32 findLen = strlen(findThis);

	if (!replaceWith)
		replaceWith = "";

	int32 replaceLen = strlen(replaceWith);
	int32 lastSrcPos = fromOffset;
	PosVect positions;
	for(int32 srcPos = 0; 
			maxReplaceCount > 0 
			&& (srcPos = (this->*findMethod)(findThis, lastSrcPos, findLen)) >= 0; 
			maxReplaceCount-- ) {
		positions.Add(srcPos);
		lastSrcPos = srcPos + findLen;
	}
	_ReplaceAtPositions(&positions, findLen, replaceWith, replaceLen);
	return *this;
}


void
BString::_ReplaceAtPositions(const PosVect* positions,
	int32 searchLen, const char* with, int32 withLen)
{
	int32 len = Length();
	uint32 count = positions->CountItems();
	int32 newLength = len + count * (withLen - searchLen);
	if (!newLength) {
		_Alloc(0);
		return;
	}

	int32 pos;
	int32 lastPos = 0;
	char *oldAdr = fPrivateData;
	char *newData = (char *)malloc(newLength + sizeof(int32) + 1);
	if (newData) {
		newData += sizeof(int32);
		char *newAdr = newData;
		for(uint32 i = 0; i < count; ++i) {
			pos = positions->ItemAt(i);
			len = pos - lastPos;
			if (len > 0) {
				memcpy(newAdr, oldAdr, len);
				oldAdr += len;
				newAdr += len;
			}
			memcpy(newAdr, with, withLen);
			oldAdr += searchLen;
			newAdr += withLen;
			lastPos = pos+searchLen;
		}
		len = Length() + 1 - lastPos;
		if (len > 0)
			memcpy(newAdr, oldAdr, len);

		free(fPrivateData - sizeof(int32));
		fPrivateData = newData;
		fPrivateData[newLength] = 0;
		_SetLength( newLength);
	}
}


#if ENABLE_INLINES
inline
#endif
void
BString::_SetLength(int32 length)
{
	*((int32*)fPrivateData - 1) = length & 0x7fffffff;
}


#if DEBUG
// AFAIK, these are not implemented in BeOS R5
// XXX : Test these puppies
void
BString::_SetUsingAsCString(bool state)
{	
	//TODO: Implement ?		
}


void
BString::_AssertNotUsingAsCString() const
{
	//TODO: Implement ?
}
#endif


//	#pragma mark - backwards compatibility


/*
	Translates to (missing const):
	BString& BString::operator<<(BString& string)
*/
extern "C" BString&
__ls__7BStringR7BString(BString* self, BString& string)
{
	return self->operator<<(string);
}


//	#pragma mark - Non-member compare for sorting, etc.


int
Compare(const BString &string1, const BString &string2)
{
	return strcmp(string1.String(), string2.String());
}


int
ICompare(const BString &string1, const BString &string2)
{
	return strcasecmp(string1.String(), string2.String());
}


int
Compare(const BString *string1, const BString *string2)
{
	return strcmp(string1->String(), string2->String());
}


int
ICompare(const BString *string1, const BString *string2)
{
	return strcasecmp(string1->String(), string2->String());
}

