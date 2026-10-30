/*
 * BACstac(TM) BACstac module ARGS.H
 * Copyright (c) 1997-2023 by Cimetrics Inc. All rights reserved.
 *
 * Purpose: command-line args parsing utilities
 *
 * Author: Ilya Levinson
 *
 */

#include <bacbase.h>

BACSTAC_BOOLEAN ArgsIsKeyPresent (
	const char *argv[],
	int argc,
	const char *keyName
);

BACSTAC_BOOLEAN ArgsIsKeysPresent (
	const char *argv[],
	int argc,
	const char *keyNames
);

BACSTAC_BOOLEAN ArgsGetInt (
	const char *argv[],
	int argc,
	const char *keyName,
	int *pValue
);

/*
 * Function search key <keyName> on list <argv> (size of list is <argc>).
 * If key is present then in <pBuffer> puts value of this key.
 * Value must be after colon and if it contains not alpha-numeric chars
 * (and underscore) then must be in quotation '"'. Shell may "eat" char '"'
 * and may be nead double quotation -
 * Example:
 * abc.exe -t:ab.cd ef.gh
 *		get value <ab>
 *
 * abc.exe -t:"ab.cd ef.gh"
 *		get value <ab>
 *
 * abc.exe -t:\"ab.cd ef.gh\"
 *		get value <ab.cd>
 *
 * abc.exe -t:"\"ab.cd ef.gh\""
 *		get value <ab.cd ef.gh>
 *
 * On success, the value is put in <pBuffer> and TRUE is returned. If key
 * was not found or it does not have any  then FALSE is returned.
 *
 * NOTE 1: If size of key value is more then buffer size (<nBufferSize>) then
 * only the beginning of the string is put into <pBuffer>.
 *
 * NOTE 2: Function search value only for first key <keyName> in list <argv>.
 */

BACSTAC_BOOLEAN ArgsGetString (
	const char *argv[],
	int argc,
	const char *keyName,
	char *pBuffer,
	size_t nBufferSize
);

BACSTAC_BOOLEAN ArgsGetOctetString (
	const char *argv[],
	int argc,
	const char *keyName,
	BACSTAC_BYTE * pBuffer,
	size_t nBufferSize,
	size_t * byteCnt
);
