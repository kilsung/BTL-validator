/*
 * BACSCAN.H - BACstac (TM) Data Type Input Routines
 *
 * Print routines for Property Contents data types:
 *      BACSTAC_PROPERTY_CONTENTS, (+PROPERTY_ID, OBJECT_ID)
 *
 * Copyright (c) 1996-2023 by Cimetrics Inc.  All rights reserved.
 *
 */

#ifndef BACSCAN_H
#define BACSCAN_H

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#include "bacprint.h"

	void BACstacSScanContents (
		const char *pString,
		BACSTAC_PROPERTY_CONTENTS *pContents,
		BACSTAC_OBJECT_TYPE objectType,
		BACSTAC_PROPERTY_ID propertyID,
		BACSTAC_BOOLEAN bUseHint
	);

/*
 * in general, also want to return number of skipped positions, a la SPRINTF
 * to allow for nested calls.  If error return 0.  If end of string return -1?
 */

	BACSTAC_DATA_TYPE BACstacGetStringValueDataType (
		const char *pString
	);

	BACSTAC_BOOLEAN BACstacIsStringValueNull (
		const char *pString
	);	/* needed for present-value */
	BACSTAC_BOOLEAN BACstacIsStringValueBoolean (
		const char *pString
	);

	BACSTAC_REAL BACstacGetStringValueReal (
		const char *pString
	);
	BACSTAC_BOOLEAN BACstacGetStringValueBoolean (
		const char *pString
	);
	BACSTAC_UNSIGNED BACstacGetStringValueUnsigned (
		const char *pString
	);
	BACSTAC_INTEGER BACstacGetStringValueInteger (
		const char *pString
	);
	BACSTAC_BOOLEAN BACstacGetStringValueOctet (
		const char *pString,
		BACSTAC_OCTET_STRING *pOctet,
		BACSTAC_BYTE *buffer,
		BACSTAC_VALUE_SIZE size
	);
	void BACstacGetStringValueString (
		const char *pString,
		char *pOutput,
		BACSTAC_VALUE_SIZE nOutputLen
	);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* BACSCAN_H */
