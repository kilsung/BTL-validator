/*
 * BACstac (TM) BACnet Error Strings
 *
 * Provide strings for BACnet error class and error code.
 *
 * Copyright (c) 1998-2023 by Cimetrics Inc.  All rights reserved.
 */

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

	const char *BACstacErrorCodeString (
		BACSTAC_ERROR_CODE errCode
	);

	const char *BACstacErrorClassString (
		BACSTAC_ERROR_CLASS errClass
	);

	const char *BACstacAbortReasonString (
		BACSTAC_ABORT_REASON reason
	);

	const char *BACstacRejectReasonString (
		BACSTAC_REJECT_REASON reason
	);

#ifdef __cplusplus
}
#endif /* __cplusplus */
