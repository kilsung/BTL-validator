/*
 * Project: BACstac (tm) examples
 *
 * Copyright (c) 2003-2023 by Cimetrics Inc. All rights reserved.
 *
 * Purpose: Function for comparing BACstac addresses
 *
 * Author: Al Markov
 *
 */

#include <bactypes.h>

#ifndef addrcmp_h
#define addrcmp_h

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

BACSTAC_BOOLEAN BACstacAddressCmp (
	const BACSTAC_ADDRESS *p1,
	const BACSTAC_ADDRESS *p2
);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* addrcmp_h */

/* eof */
