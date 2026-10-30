/*
 * DIRCONN.H - BACstac(TM) SC Direct Connect API
 *
 * Establish and close SC Direct Connect
 *
 * Copyright (c) 2024 by Cimetrics Inc.  All rights reserved.
 * Author: Eugene Baranov
 */

#ifndef DIRCONN_H
#define DIRCONN_H

#include <bacbase.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

typedef struct DIR_CONN_ID
	{
	BACSTAC_BOOLEAN valid;
	BACSTAC_UI8 portID;
	BACSTAC_BYTE vmac[6];
	}
DIR_CONN_ID;

#define INVALID_DIR_CONN_ID { \
	BACSTAC_FALSE, 0, {0, 0, 0, 0, 0, 0}}


BACSTAC_BOOLEAN EstablishDirectConnect (const BACSTAC_ADDRESS *destAddress, DIR_CONN_ID *pDC);
void DisconnectDirectConnect (const DIR_CONN_ID *pDC);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DIRCONN_H */
