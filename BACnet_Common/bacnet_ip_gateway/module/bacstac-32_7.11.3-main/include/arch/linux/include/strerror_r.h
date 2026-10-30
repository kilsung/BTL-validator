/*
	BACstac/32, OSA port on Linux

	Copyright (c) 1998-2022 by Cimetrics Inc. All rights reserved.

	Purpose:	Short thread-safe implementation of strerror(errno)

	Platform:	Linux >= 2.0

	Author:		Sergey Clushin
*/


#ifndef strerror_r_h_included
#define strerror_r_h_included

#ifndef _REENTRANT
#error Define _REENTRANT before any #include in your module!
#endif

#include <errno.h>
#include <string.h>


// --------------------------------------------------------------------------
// You can use <STRERROR_R> family macros as in the following example:
//
// #include "strerror_r.h"
//
// if ((n = read(fd, buf, count)) < 0)
//	{
//	STRERROR_R_DECLARE;
//	OSALogMessage(osaLogError, "read error, %s", STRERROR_R);
//	}
//
// --------------------------------------------------------------------------

#define STRERROR_R_DECLARE	char strerror_r_buffer[64]

#define STRERROR_RE(errnum)	(bac_strerror_r((errnum), strerror_r_buffer, \
				 sizeof(strerror_r_buffer)))

#define STRERROR_R STRERROR_RE(errno)

#if (_POSIX_C_SOURCE >= 200112L) && !_GNU_SOURCE
/* The XSI-compliant version of strerror_r() is provided, so we need to wrap it
 * to return a pointer to a string containing the error message. */
char* bac_strerror_r(int errnum, char *buf, size_t buflen);
#else
/* The GNU-specific strerror_r() is provided, which returns pointer to a string
 * containing the error message. */
#  define bac_strerror_r strerror_r
#endif

#endif // strerror_r_h_included
