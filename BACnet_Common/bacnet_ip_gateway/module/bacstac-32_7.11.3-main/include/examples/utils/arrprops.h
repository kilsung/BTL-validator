/*
 * BACstac(TM) Property Description List Utility Routines
 *
 *  Included client/server independent routines for property lists
 *
 * Copyright (c) 2000-2021 by Cimetrics Inc.  All rights reserved.
 * Author: Marina Selissky
 */

#include <bactypes.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

	BACSTAC_OBJECT_COUNT BACstacGetAllStandardObjectsNumber (
		void
	);

	BACSTAC_PROPERTY_COUNT BACstacGetAllObjectPropsNumber (
		BACSTAC_OBJECT_TYPE objType
	);

	const BACSTAC_PROPERTY_ID *BACstacGetObjectPropList (
		BACSTAC_OBJECT_TYPE objType
	);

	BACSTAC_PROPERTY_COUNT BACstacGetUnknownPropListSize (
		void
	);

	const BACSTAC_PROPERTY_ID *BACstacGetUnknownPropList (
		void
	);
#ifdef __cplusplus
}
#endif /* __cplusplus */
