/*
 * BACstac(TM) dhcpinfo.h
 *
 * Copyright (c) 2017-2023 by Cimetrics Inc. All rights reserved.
 *
 * Purpose: Define the interface to obtain DHCP info.
 *
 * NOTE: dhcpinfo.c should be compiled with DHCP_HELPER_MT to make it thread
 * safe. Otherwise it should be used only from the BACstac Main Thread.
 *
 * Author: Dmitry Potapov
 */

#ifndef DHCPINFO_H
#define DHCPINFO_H 1

#include <time.h>
#include <bacbase.h>
#include <netinet/in.h>
#include <status.h>
#include <npoerror.h>

typedef struct DHCP_INFO
{
	BACSTAC_BOOLEAN enabled;
	time_t lease;
	time_t expiry;
	union u_addr
	{
		struct in_addr  in;
		struct in6_addr in6;
	} server;
} DHCP_INFO;

/* DhcpHelperInit initializes DHCP helper.
 *
 * NOTE: DHCP helper is optional. If it does not present on the system, we
 * assume that DHCP is not supported, and the function returns STATUS_OK.
 */
CORE_STATUS DhcpHelperInit(void);

/*
 * DhcpHelperClose releases the resources allocated by DhcpHelperInit.
 */
void DhcpHelperClose(void);

/*
 * DhcpHelperGet gets DHCP information for the specified address family.
 *
 * NOTE: 'af' can be either AF_INET or AF_INET6.
 */
NPO_ERROR DhcpHelperGet(int af, const char *ifname, DHCP_INFO *info);

#endif /* DHCPINFO_H */
