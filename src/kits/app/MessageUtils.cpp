//------------------------------------------------------------------------------
//	MessageUtils.cpp
//
//------------------------------------------------------------------------------

// Standard Includes -----------------------------------------------------------

// System Includes -------------------------------------------------------------

// Project Includes ------------------------------------------------------------
#include <MessageUtils.h>

// Local Includes --------------------------------------------------------------

// Local Defines ---------------------------------------------------------------

// Globals ---------------------------------------------------------------------

//------------------------------------------------------------------------------
uint32 _checksum_(const uchar* buf, int32 size)
{
	uint32 sum = 0;
	uint32 temp = 0;

	while (size > 3) {
#if defined(__INTEL__)
		sum += B_SWAP_INT32(*(int*)buf);
#else
		sum += *(int*)buf;
#endif

		buf += 4;
		size -= 4;
	}

	while (size > 0) {
		temp = (temp << 8) + *buf++;
		size -= 1;
		sum += temp;
	}

	return sum;
}
//------------------------------------------------------------------------------
int32 TChecksumHelper::CheckSum()
{
	 return _checksum_(fBuffer, fBufPtr - fBuffer);
}
//------------------------------------------------------------------------------

/*
 * $Log $
 *
 * $Id  $
 *
 */

