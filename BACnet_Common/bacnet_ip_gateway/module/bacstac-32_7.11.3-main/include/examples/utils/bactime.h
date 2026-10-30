/*
 * BACTIME.H - BACstac(TM) Time Utilities
 *
 * Copyright (c) 1998-2023 by Cimetrics Inc.  All rights reserved.
 * Author: Keith Corbett
 *
 */

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#include <bnetdef.h>
#include <bneterr.h>
#include <bacbase.h>
#include <bactypes.h>

	BACSTAC_BOOLEAN BACstacHasIntervalPassed (
		int interval
	);

	BACSTAC_BOOLEAN BACstacIsTimeLessThan (
		const BACSTAC_TIME *pTimeA,
		const BACSTAC_TIME *pTimeB
	);

	BACSTAC_UNSIGNED BACstacTimeDiffMsec (
		const BACSTAC_TIME *pStartTime,
		const BACSTAC_TIME *pStopTime
	);

	BACSTAC_BOOLEAN BACstacGetCurrentTime (
		BACSTAC_TIME *pTime
	);

	BACSTAC_BOOLEAN BACstacGetCurrentDate (
		BACSTAC_DATE *pDate
	);

	BACSTAC_BOOLEAN BACstacGetCurrentDateTime (
		BACSTAC_DATE_TIME *pDateTime
	);

	BACSTAC_BOOLEAN BACstacSetCurrentDateTime (
		const BACSTAC_DATE_TIME *pDateTime
	);

#ifdef __cplusplus
}
#endif /* __cplusplus */
