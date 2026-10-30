/* BACstac (TM) Example Server Application
 * TSM handlers manipulating
 *
 * Copyright (c) 2000-2023 by Cimetrics Inc.  All rights reserved.
 *
 * Author: Marina Selissky
 */

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#include <bactypes.h>

/**********  Destroy TSM handle in case of failed transaction ******/
	void BACstacVerifyTSMCompletion (
		const char *functionName,
		BACSTAC_STATUS inStatus,
		BACSTAC_HTSM hTSM
	);

#ifdef __cplusplus
}
#endif /* __cplusplus */
