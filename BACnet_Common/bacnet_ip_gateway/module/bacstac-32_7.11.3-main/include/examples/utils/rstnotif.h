/*
 * Project: BACstac (TM) examples
 *
 * Copyright (c) 2011-2023 by Cimetrics Inc. All rights reserved.
 *
 * Purpose: When a BACnet device restarts, it could lose some of its subscriptions.
 * Other devices may depend on these subscriptions. This restart procedure send
 * Unconfirmed event notifications to pre-configured BACnet devices as it is
 * prescribed by BACnet Standard Clause 19.3.
 *
 * Author: Dmitry Potapov
 */

#ifndef BACSTAC_RSTNOTIF_H
#define BACSTAC_RSTNOTIF_H

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/* RestartNotifIAmHook is used by SendRestartNotif() to determine addresses based
 * on device instance numbers. This procedure can be call directly from another
 * hook, or installed directly after InitRestartNotif().
 */
BACSTAC_HOOK_STATUS BACstac_hook RestartNotifIAmHook(
	BACSTAC_HTSM hTransaction, /* not used */
	BACSTAC_ADDRESS *pSourceAddress,
	BACSTAC_ADDRESS *pDestinationAddress, /* not used */
	const BACSTAC_I_AM_INFO *pInfo);

/*
 * InitRestartNotif must be called before RestartNotifIAmHook. It is possible to
 * call it before BACstacInit().
 *
 * CloseRestartNotif must be called after BACstacClose(), otherwise there will be a
 * race condition in your code.
 */
BACSTAC_STATUS InitRestartNotif(void);
void CloseRestartNotif(void);

BACSTAC_STATUS SendRestartNotif(BACSTAC_HDEVICE hDev);
void CancelRestartNotif(BACSTAC_HDEVICE hDev);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* BACSTAC_RSTNOTIF_H */
