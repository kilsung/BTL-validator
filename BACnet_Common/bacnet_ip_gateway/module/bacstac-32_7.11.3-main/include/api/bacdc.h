/*
 * BACstac(TM) BACDC.H
 *
 * Copyright (c) 2024 by Cimetrics Inc. All rights reserved.
 *
 * Purpose: define BACstac Direct Connect API
 */
#ifndef BACDC_H
#define BACDC_H

#include "bactypes.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
/*
 * Parameters to initiate a direct connection
 */
struct BACSTAC_DIRECT_CONNECT_INIT;
typedef struct BACSTAC_DIRECT_CONNECT_INIT
{
    /* IMPORTANT: All fields are part of the current implementation.
     * The application should avoid relying on specific fields and
     * instead use access routines defined below. */
    BACSTAC_BYTE flags;
    BACSTAC_BYTE portID;
    BACSTAC_BYTE vmac[6];
    const char *uri;
} BACSTAC_DIRECT_CONNECT_INIT;

/*
 * Access routines to BACSTAC_DIRECT_CONNECT_INIT
 */

/* Initialize BACSTAC_DIRECT_CONNECT_INIT */
BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacInitDirectConnectInit(
    BACSTAC_DIRECT_CONNECT_INIT *dc,
    BACSTAC_BYTE portID);

/* Set attributes */
BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacSetDirectConnectInitVMAC(
    BACSTAC_DIRECT_CONNECT_INIT *dc,
    const BACSTAC_BYTE vmac[6]);

BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacSetDirectConnectInitURI(
    BACSTAC_DIRECT_CONNECT_INIT *dc,
    const char *uri);

/*
 * The function initiates a direct connection to the specified BACnet/SC node.
 *
 * The function returns success if either the specified direct connection has
 * already been established or the process of establishing of a direct
 * connection has been started. The application can read the SC_Direct_Connect_
 * Connection_Status property of the Network Port object to obtain the current
 * status of direct connection.
 *
 * NOTE: Usually, SC_Direct_Connect_Connection_Status should contain an item
 * element with the specified VMAC or URI. However, if too many outgoing direct
 * connections failed at the same time (currently, more than 5) then the
 * specified direct connection may be already expelled from the property.
 */
BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacEstablishDirectConnect(
    BACSTAC_DIRECT_CONNECT_INIT *dc);

/*
 * The function initiates the local disconnection procedure for an initiated or
 * accepted direct connection. The direct connection is identified by portID
 * and VMAC. The local disconnect procedure is described in Clause AB.6.2.2 and
 * AB.6.2.3 of the BACnet standard for an initiated and accepted direct
 * connection correspondingly.
 *
 * The function does not guarantee that the connection is closed on successful
 * return, however, it is possible to initiate a new direct connection with the
 * same PortID and VMAC immediately without waiting for the specified
 * connection being closed.
 *
 * NOTE: The function returns success if the given PortID specifies an existing
 * BACnet/SC port and either the disconnect procedure has been started or the
 * direct connection has not been found (it could have been closed before or
 * did not exist at all).
 */
BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacDisconnectDirectConnect(
    BACSTAC_BYTE portID, const BACSTAC_BYTE vmac[6]);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* BACDC_H */
