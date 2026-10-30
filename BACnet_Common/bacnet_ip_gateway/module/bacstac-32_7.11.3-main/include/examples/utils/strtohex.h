/*
 * Project: BACstac(TM) examples
 *
 * Copyright (c) 2010-2023 by Cimetrics Inc. All rights reserved.
 *
 * Purpose: The StringToHex function converts a character string
 *          containing hex digits to its binary representation.
 *
 *          char[] = "B9a4" -> BACSTAC_BYTE[] = {0xb9, 0xa4}
 *          char[] = "9a4" -> BACSTAC_BYTE[] = {0x09, 0xa4}
 *
 */
#ifndef UTILS_STRTOHEX_H
#define UTILS_STRTOHEX_H

#include <bacbase.h>

/*
 * StringToHex converts the input <string> of characters, which contains hex
 * digits to an array of bytes stored at the buffer pointed by <buffer>.
 * The maximum size of the buffer is specified by <bufsize>. The number of bytes
 * stored in the output buffer is set to the variable pointed by <nBytes>.
 *
 * On success, the function returns TRUE. If error occurred (the buffer size is
 * too small, or the string has a non-hex digit), it returns FALSE.
 */

BACSTAC_BOOLEAN StringToHex(const char *string, BACSTAC_BYTE *buffer, size_t bufsize,
		size_t *nBytes);

#endif /* UTILS_STRTOHEX_H */
