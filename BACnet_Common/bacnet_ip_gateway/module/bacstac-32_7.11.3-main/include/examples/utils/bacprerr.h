/*
 * BACstac (TM) Status & Error Print Routines
 *
 * Provide strings for the BACSTAC_STATUS and BACSTAC_ERROR data types
 *
 * Copyright (c) 1998-2018 by Cimetrics Inc.  All rights reserved.
 */

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */
#include <bactypes.h>

void BACstacPrintError(BACSTAC_STATUS Code, const BACSTAC_ERROR *pErr,
	const char *comment);

const char *BACstacStatusString(BACSTAC_STATUS Code);

#ifdef __cplusplus
}
#endif /* __cplusplus */
