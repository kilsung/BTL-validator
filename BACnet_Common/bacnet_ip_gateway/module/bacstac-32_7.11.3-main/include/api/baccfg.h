/*
 * BACstac(TM)
 *
 * Copyright (c) 2004-2025 by Cimetrics Inc.  All rights reserved.
 *
 * Purpose: This file contains BACstac configuration
 */

#ifndef BACCFG_H
#define BACCFG_H

/* BACstac configuration */
#undef  CONFIG_NL_ROUTER
#define CONFIG_NL_CONNECT 1
#define CONFIG_ARCNET 1
#define CONFIG_ETHERNET 1
#define CONFIG_BIP 1
#define CONFIG_BIP_BBMD 1
#define CONFIG_BIPV6 1
#define CONFIG_BIPV6_BBMD 1
#undef  CONFIG_SC
#define CONFIG_MSTP 1
#undef  CONFIG_MSTP_EXT
#define CONFIG_NULL 1
/* End of configuration */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <bacbase.h>
#include "baclogml.h"
#include <stdarg.h> /* for va_list */

/*
 * There are three versions of network configuration:
 * 1. Without routing and without establishing remote PTP connection.
 * 2. Without routing but with automatic establishing remote PTP connection.
 * 3. With routing and automatic establishing remote PTP connection.
 */
#ifdef CONFIG_NL_CONNECT
#ifdef CONFIG_NL_ROUTER
#define BACstacCfgLoadNl BACstacCfgLoadNl3
#define BACstacCfgFreeNl BACstacCfgFreeNl3
#define BACstacCfgSaveDlInfo BACstacCfgSaveDlInfo3
#define BACstacCfgSaveNlPortTable BACstacCfgSaveNlPortTable3
#define tagBACSTAC_NL_CFG tagBACSTAC_NL3_CFG
#define BACSTAC_NL_CFG tagBACSTAC_NL3_CFG
#else /* CONFIG_NL_ROUTER */
#define BACstacCfgLoadNl BACstacCfgLoadNl2
#define BACstacCfgFreeNl BACstacCfgFreeNl2
#define BACstacCfgSaveDlInfo BACstacCfgSaveDlInfo2
#define BACstacCfgSaveNlPortTable BACstacCfgSaveNlPortTable2
#define tagBACSTAC_NL_CFG tagBACSTAC_NL2_CFG
#define BACSTAC_NL_CFG tagBACSTAC_NL2_CFG
#endif /* CONFIG_NL_ROUTER */
#else /* CONFIG_NL_CONNECT */
#ifdef CONFIG_NL_ROUTER
#error "Inconsistent configuration"
#endif /* CONFIG_NL_ROUTER */
#define BACstacCfgLoadNl BACstacCfgLoadNl1
#define BACstacCfgFreeNl BACstacCfgFreeNl1
#define BACstacCfgSaveDlInfo BACstacCfgSaveDlInfo1
#define BACstacCfgSaveNlPortTable BACstacCfgSaveNlPortTable1
#define tagBACSTAC_NL_CFG tagBACSTAC_NL1_CFG
#define BACSTAC_NL_CFG tagBACSTAC_NL1_CFG
#endif /* CONFIG_NL_CONNECT */

struct tagBACSTAC_DL_EXECUTIVE;
typedef struct tagBACSTAC_DL_EXECUTIVE BACSTAC_DL_EXECUTIVE;

#ifndef BACSTAC_SEGMENTATION_DEFINED
#define BACSTAC_SEGMENTATION_DEFINED
typedef enum BACSTAC_SEGMENTATION
{
	SEGMENTED_BOTH                 = 0, /* send and receive */
	SEGMENTED_TRANSMIT             = 1, /* send only */
	SEGMENTED_RECEIVE              = 2, /* receive only */
	NO_SEGMENTATION                = 3  /* no segmentation */
	BACSTAC_ENUM_COMPATIBLE(SEGMENTATION)
} BACSTAC_SEGMENTATION;
#endif /* BACSTAC_SEGMENTATION_DEFINED */

#ifdef CONFIG_OSA_WCHAR
typedef wchar_t BACSTAC_OSA_CHAR;
#define BACSTAC_OSA_TEXT_(str) L ## str
#define BACSTAC_OSA_TEXT(str) BACSTAC_OSA_TEXT_(str)
#else /* CONFIG_OSA_WCHAR */
typedef char BACSTAC_OSA_CHAR;
#define BACSTAC_OSA_TEXT(str) str
#endif /* CONFIG_OSA_WCHAR */

typedef const BACSTAC_OSA_CHAR *BACSTAC_OSA_CSTR;

/*
 * BACSTAC_DEVICE_UUID stores 16 bytes of GUID in the big-endian order.
 * For example, if UUID is 11223344-5566-7788-9900-aabbccddeeff, it is
 * stored as:
 *   { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
 *     0x99, 0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF }
 */
typedef BACSTAC_BYTE BACSTAC_DEVICE_UUID[16];
#define BACSTAC_DEVICE_UUID_NONE {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}

typedef struct tagBACSTAC_DL_CFG {
	BACSTAC_UINT pduPoolSize;
	BACSTAC_UINT socketBufferSize;
	BACSTAC_DEVICE_UUID deviceUUID;
} BACSTAC_DL_CFG;
#if defined(CONFIG_BIP) || defined(CONFIG_BIP_MODULE)
/*
 * B/IP configuration
 */
#ifdef CONFIG_BIP_BBMD
typedef struct tagBACSTAC_BIP_BBMD_CFG {
	/*
	 * The 'bdt' field stores a space-separated list of BDT entries.
	 *
	 * Each entry has the followng format:
	 *    <ip>[/<subnet>][:<port>]
	 * where
	 *    <ip> is the IP address, e.g.: 192.168.1.1.
	 *    <subnet> is the sumbet mask, which can be written either as four
	 *         octets or the number of bits, e.g.: 255.255.255.0 or 24. If
	 *         it is omitted then it is assumed to be 255.255.255.255.
	 *    <port> is the UDP port number. If it is omitted then it is
	 *         assumed to be 47808.
	 *
	 * Example:
	 *   "192.168.1.1 192.168.10.1/24 192.168.11.1:47809"
	 */
	BACSTAC_OSA_CSTR bdt;
	BACSTAC_BOOLEAN fdtPresent;
	/*
	 * natSupported indicates whether or not BACnet/IP NAT functionality
	 * as described in Clause J.7.8 is supported by this port. If it is
	 * is TRUE, the Network Port object has the BACnet_IP_NAT_Traversal
	 * and BACnet_IP_Global_Address properties, which has values defined
	 * below. If it is FALSE then all following NAT related parameters are
	 * ignored.
	 */
	BACSTAC_BOOLEAN natSupported;
	/*
	 * natEnable indicates whether or not this port is configured to
	 * operate in a NAT environment. Its value can be changed by writing
	 * the BACnet_IP_NAT_Traversal property in the Network port object.
	 */
	BACSTAC_BOOLEAN natEnabled;
	/*
	 * If support of Foreign Device is enabled, maxFdtEntries defines the
	 * maximum size of the Foreign Device Table, i.e. the maximum number
	 * of foreign devices that can connect to this port.
	 */
	BACSTAC_UINT maxFdtEntries;
	/*
	 * natIp and natUdp indicate the global address and UDP port from which
	 * the network port can be accessed from the global side of a NAT router.
	 * Their values can be changed by writing the BACnet_IP_Global_Address
	 * property in the Network port object.
	 */
	BACSTAC_OSA_CSTR natIp;
	BACSTAC_UINT natUdp;
	/*
	 * Expiration timeout for resolver in seconds; zero means infinite. The
	 * maximum possible value is about 1 million seconds (12 days).
	 */
	BACSTAC_UINT resolvTimeout;
} BACSTAC_BIP_BBMD_CFG;
#endif /* CONFIG_BIP_BBMD */

#define INFINITY_FD_REG_ATTEMPTS 0

typedef struct tagBACSTAC_BIP_FD_CFG { /* This structure described a BIP port
				     * that is a foreign device */
	BACSTAC_BOOLEAN foreignDevice; /* always BACSTAC_TRUE */
	BACSTAC_OSA_CSTR ip; /* local ip address to bind */
	BACSTAC_UI16 udp; /* local udp port to bind */
	BACSTAC_OSA_CSTR fdBbmdIp; /* IP address to connect */
	BACSTAC_UI16 fdBbmdUdp; /* UDP port of to connect */
	BACSTAC_UI16 fdTimeToLive; /* Time to live for registration */
	BACSTAC_UINT fdRegTimeout; /* Time to wait before retry the registration request */
	BACSTAC_UINT numFdRegAttempts; /* number of registration retries, zero means infinity */
} BACSTAC_BIP_FD_CFG;

typedef struct tagBACSTAC_BIP_CFG { /* This structure described a Bip port
				   * that is not a foreign device */
	BACSTAC_BOOLEAN foreignDevice; /* always BACSTAC_FALSE */
	BACSTAC_OSA_CSTR ip;
	BACSTAC_UI16 udp;
	BACSTAC_OSA_CSTR subnet;
#ifdef CONFIG_BIP_BBMD
	const BACSTAC_BIP_BBMD_CFG* bbmd;
#endif /* CONFIG_BIP_BBMD */
} BACSTAC_BIP_CFG;

/* B/IP configuration is either BACSTAC_BIP_CFG or BACSTAC_BIP_FD_CFG
 * and that determines by the flag field at the beginning of these
 * structure
 */
typedef union tagBACSTAC_BIP_CFG_U
{
	BACSTAC_BOOLEAN foreignDevice;
	BACSTAC_BIP_FD_CFG fd; /* if foreignDevice is BACSTAC_TRUE  */
	BACSTAC_BIP_CFG bip;   /* if foreignDevice is BACSTAC_FALSE */
} BACSTAC_BIP_CFG_U;

extern const BACSTAC_DL_EXECUTIVE BACstacSubDlBip;

#endif /* CONFIG_BIP */
#if defined(CONFIG_BIPV6) || defined(CONFIG_BIPV6_MODULE)
/*
 * B/IP configuration
 */
#ifdef CONFIG_BIPV6_BBMD
typedef struct tagBACSTAC_BIPV6_BBMD_CFG {
	/*
	 * The 'bdt' field stores a space-separated list of BDT entries.
	 *
	 * Each entry has the followng format:
	 *    <host> | '[' <host> ']' ':' <port>
	 * where
	 *    <host> is the IPv6 address or the host name of a BBMD entry.
	 *    <port> is the UDP port number. If it is not specified then
	 *           it is assumed to be 47808.
	 *
	 * Example:
	 *   "fe80::42b0:76ff:fe47:ed79 [fe80::42b0:76ff:fe47:ed7a]:47809"
	 */
	BACSTAC_OSA_CSTR bdt;
	BACSTAC_BOOLEAN fdtPresent;
	BACSTAC_UINT maxFdtEntries;
	BACSTAC_UINT resolvTimeout; /* Expiration timeout for resolver in seconds;
	                               0 means infinite. The maximum possible value
	                               is about 1 million seconds (12 days) */
} BACSTAC_BIPV6_BBMD_CFG;
#endif /* CONFIG_BIPV6_BBMD */

typedef struct tagBACSTAC_BIPV6_VMAC_CFG {

	BACSTAC_UI16 cacheSize;     /* The size of the VMAC cache table, currently
	                               it should be between 1 and 255. */
	BACSTAC_UI8  numRetries;    /* Resolution retries (between 0 and 7). */
	BACSTAC_UINT resolvTimeout; /* Resolution timeout in milliseconds. */
	BACSTAC_UINT staleTimeout;  /* Stale timeout in milliseconds */
} BACSTAC_BIPV6_VMAC_CFG;

#define INFINITY_FD_REG_ATTEMPTS 0

typedef struct tagBACSTAC_BIPV6_FD_CFG { /* This structure described a BACnet/IPv6 port
	                                  * that is a foreign device */
	BACSTAC_BOOLEAN foreignDevice; /* always BACSTAC_TRUE */
	BACSTAC_UI16 udpPort; /* local udp port to bind */
	BACSTAC_INST_NUMBER devInstNum; /* device instance number, which is used for VMAC */
	BACSTAC_OSA_CSTR ip; /* local ip address to bind */
	BACSTAC_OSA_CSTR fdBbmdIp; /* IP address to connect */
	BACSTAC_UI16 fdBbmdUdp; /* UDP port of to connect */
	BACSTAC_UI16 fdTimeToLive; /* Time to live for registration */
	BACSTAC_UINT fdRegTimeout; /* Time to wait before retry the registration request */
	BACSTAC_UINT numFdRegAttempts; /* number of registration retries, zero means infinity */
	BACSTAC_BIPV6_VMAC_CFG vmac;
} BACSTAC_BIPV6_FD_CFG;

typedef struct tagBACSTAC_BIPV6_CFG { /* This structure described a BACnet/IPv6 port
	                               * that is not a foreign device */
	BACSTAC_BOOLEAN foreignDevice; /* always BACSTAC_FALSE */
	BACSTAC_UI16 udpPort;
	BACSTAC_INST_NUMBER devInstNum; /* device instance number, which is used for VMAC */
	BACSTAC_OSA_CSTR ip;
	BACSTAC_OSA_CSTR mcast;
	BACSTAC_BIPV6_VMAC_CFG vmac;
#ifdef CONFIG_BIPV6_BBMD
	const BACSTAC_BIPV6_BBMD_CFG *bbmd;
#endif /* CONFIG_BIPV6_BBMD */
} BACSTAC_BIPV6_CFG;

/* B/IP configuration is either BACSTAC_BIPV6_CFG or BACSTAC_BIPV6_FD_CFG
 * and that determines by the flag field at the beginning of these
 * structure
 */
typedef union tagBACSTAC_BIPV6_CFG_U
{
	BACSTAC_BOOLEAN foreignDevice;
	BACSTAC_BIPV6_FD_CFG fd; /* if foreignDevice is BACSTAC_TRUE  */
	BACSTAC_BIPV6_CFG bip;   /* if foreignDevice is BACSTAC_FALSE */
} BACSTAC_BIPV6_CFG_U;

extern const BACSTAC_DL_EXECUTIVE BACstacSubDlBipv6;

#endif /* CONFIG_BIPV6 */
#ifdef CONFIG_MSTP
typedef struct tagBACSTAC_MSTP_CFG {
	/* main configuration parameters */
	BACSTAC_UINT TS;
	BACSTAC_UINT baudrate;
	/* additional configuration parameters */
	BACSTAC_UINT Nmax_info_frames;
	BACSTAC_UINT Nmax_manager;
#ifdef __linux__
	/* linux specific configuration parameters */
	BACSTAC_INT schedPriority;
	BACSTAC_INT rtsOnSend;
#endif
#ifdef CONFIG_MSTP_EXT
	/* full path name of external MS/TP DL binary */
	BACSTAC_OSA_CSTR extDlName;
#endif
} BACSTAC_MSTP_CFG;

extern const BACSTAC_DL_EXECUTIVE BACstacSubDlMstp;

#endif /* CONFIG_MSTP */
#if defined(CONFIG_NULL) || defined(CONFIG_NULL_MODULE)
typedef struct tagBACSTAC_NULL_CFG {
	BACSTAC_BYTE mac; /* one byte address */
} BACSTAC_NULL_CFG;

extern const BACSTAC_DL_EXECUTIVE BACstacSubDlNull;

#endif /* CONFIG_NULL */
#ifdef CONFIG_ARCNET
extern const BACSTAC_DL_EXECUTIVE BACstacSubDlArcnet;

#endif /* CONFIG_ARCNET */
#ifdef CONFIG_ETHERNET
extern const BACSTAC_DL_EXECUTIVE BACstacSubDlEthernet;

#endif /* CONFIG_ETHERNET */
#ifdef CONFIG_SC
typedef struct tagBACSTAC_SC_CFG {

	/*
	 * Initial Virtual MAC of the BACnet/SC node. The VMAC should have 6
	 * octets, which is written in the hex format and can be separated by
	 * either '-' or ':'.
	 * For example:
	 *   "010203040506"
	 *   "01-02-03-04-05-06"
	 *   "01:02:03:04:05:06"
	 * Value "FFFFFFFFFFFF" and "000000000000" are reserved and should not
	 * be used as VMAC. In case of VMAC collision the actual VMAC used by
	 * the device may differ from this parameter.
	 */
	BACSTAC_OSA_CSTR vmac;

	/*
	 * The WebSocket URI of the primary BACnet/SC hub. The URI should
	 * follow the "wss" URI scheme defined in in RFC 6455, Section 3, and
	 * it should contain at least the host name and port. For example:
	 * wss://server.example.com:4443/
	 */
	BACSTAC_OSA_CSTR primaryHubURI;

	/*
	 * The WebSocket URI of the failover BACnet/SC hub. It has the same
	 * format as Primary_Hub_URI.
	 */
	BACSTAC_OSA_CSTR failoverHubURI;

	/* The file that contains the private key of the node. */
	BACSTAC_OSA_CSTR privateKey;

	/* The file that contains the public certificate of the node */
	BACSTAC_OSA_CSTR operationalCertificate;

	/* The two files that contain the signing CA issuer certificate files
	 * the port accepts when it is establishing a secure connection. */
	BACSTAC_OSA_CSTR issuerCertificate[2];

	/* The file that contains the PKCS #10 certificate signing request */
	BACSTAC_OSA_CSTR certSigningRequest;

	/*
	 * Whether the node supports initiating WebSocket
	 * connections as BACnet/SC direct connections.
	 */
	BACSTAC_BOOLEAN directConnectInitiateEnable;

	/*
	 * Whether the node supports accepting WebSocket
	 * connections as BACnet/SC direct connections.
	 */
	BACSTAC_BOOLEAN directConnectAcceptEnable;

	/*
	 * directConnectAcceptURIs contains a list of WebSocket URIs that can be
	 * used by other devices to establish a direct connect to this device.
	 * The format of URIs is defined in RFC 3986, and they are separated by
	 * a single space (X'20'). This parameter is used only if accepting of
	 * direct connect is enabled.
	 */
	BACSTAC_OSA_CSTR directConnectAcceptURIs;

	/*
	 * directConnectBinding specifies the IP address and TCP port to bind
	 * to. This parameter is used only if accepting of direct connect is
	 * enabled.
	 *
	 * This parameter is a combination of the binding address and the
	 * binding port separated by column (':'). For example:
	 *   "127.0.0.1:4450"
	 *
	 * If the IP address and the nic parameter are empty, it means to
	 * listen on all IP addresses (i.e. bind to INADDR_ANY). If IP address
	 * is specified and the nic is parameter is empty, the data link binds
	 * to the specified address. If the IP address is empty and nic is
	 * specified, the data link binds to the first IP address assigned to
	 * that nic. On Linux, nic and IP address can not be specified at the
	 * same time. On Windows, if both parameters are non-empty then the IP
	 * address is only used if it is assigned to the nic interface,
	 * otherwise STATUS_BAD_CONFIG is returned.
	 */
	BACSTAC_OSA_CSTR directConnectBinding;

	/*
	 * Populate directConnectAcceptURIs with an automatically generated URIs
	 * using the IP addresses specified for the network interface.
	 */
	BACSTAC_BOOLEAN autoPopulationEnable;

	/*
	 * Max number of established direct connections (0 - unlimited).
	 */
	BACSTAC_UI16 maxDirectConnections;

	/*
	 * Minimum delay between attempts to re-establish WebSocket connection
	 * to the Hub, in seconds. The BACnet/SC data link uses this timeout
	 * before the first attempt to re-establish connection and then it
	 * doubles the current retry timeout before each next attempt until it
	 * reaches the maximumReconnectTime. All subsequent attempts are done
	 * with maximumReconnectTime.
	 */
	BACSTAC_UI16 minimumReconnectTime;

	/*
	 * Maximum delay between attempts to re-establish WebSocket connection
	 * to the Hub, in seconds. Standard recommends to set it to no more
	 * than 600 seconds (AB.6.1).
	 */
	BACSTAC_UI16 maximumReconnectTime;

	/*
	 * Timeout to wait for a response to the Connect-Request message,
	 * in seconds.
	 */
	BACSTAC_UI16 connectWaitTimeout;

	/*
	 * Timeout to wait for a response to the Disconnect-Request message,
	 * in seconds.
	 */
	BACSTAC_UI16 disconnectWaitTimeout;

	/*
	 * To keep the established connection alive, the initiating node sends
	 * Heartbeat-Request messages periodically with this period.
	 */
	BACSTAC_UI16 heartbeatTimeout;

	/*
	 * Timeout to wait for a Heartbeat-ACK message in seconds. If this
	 * parameter is not zero and Heartbeat-ACK was not received in response
	 * to the Heartbeat-Request request within this timeout then the
	 * connection is closed automatically. Zero means infinite timeout,
	 * i.e. absence of Heartbeat-ACK message does not result in connection
	 * termination.
	 */
	BACSTAC_UI16 heartbeatWaitTimeout;

	/*
	 * Timeout to wait for a response to an Address-Resolution message,
	 * in seconds.
	 */
	BACSTAC_UI16 addressResolutionTimeout;

	/*
	 * Delay between Address-Resolution attempts, in seconds.
	 */
	BACSTAC_UI16 addressResolutionDelay;

} BACSTAC_SC_CFG;

/* Global configuration for all BACnet/SC ports */
typedef struct BACSTAC_SC_GLOBAL_CFG
{
	BACSTAC_BOOLEAN allowTLSv12;
	BACSTAC_BOOLEAN logDebug;
	/* logWebsockLevel is a bitmask from that contains LL_XXX flags from
	 * libwebsockets/include/libwebsockets/lws-logs.h */
	BACSTAC_UI32 logWebsockLevel;
} BACSTAC_SC_GLOBAL_CFG;

extern const BACSTAC_DL_EXECUTIVE BACstacSubDlSc;

#define BACSTAC_SC_DEF_MINIMUM_RECONNECT_TIME 10
#define BACSTAC_SC_DEF_MAXIMUM_RECONNECT_TIME 60
#define BACSTAC_SC_DEF_CONNECT_WAIT_TIMEOUT 10
#define BACSTAC_SC_DEF_DISCONNECT_WAIT_TIMEOUT 10
#define BACSTAC_SC_DEF_HEARTBEAT_TIMEOUT 300
#define BACSTAC_SC_DEF_HEARTBEAT_WAIT_TIMEOUT 0
#define BACSTAC_SC_DEF_ADDRESS_RESOLUTION_TIMEOUT 5
#define BACSTAC_SC_DEF_ADDRESS_RESOLUTION_DELAY 30
#define BACSTAC_SC_DEF_MAX_DIRECT_CONNECTIONS 32

#endif /* CONFIG_SC */

/*
 * NL port table
 */
typedef struct tagBACSTAC_PORT_TABLE_CFG {
	BACSTAC_UI8 portId; /* 1..255 */
	BACSTAC_UI16 dnet; /* 1..65534 */
	const BACSTAC_DL_EXECUTIVE *subDL;
	BACSTAC_OSA_CSTR label;
	BACSTAC_OSA_CSTR nic;
	const void* dlInfo;
	BACSTAC_BOOLEAN connected;
} BACSTAC_PORT_TABLE_CFG;

#ifdef CONFIG_NL_ROUTER
#define BACSTAC_INIT_PHYSICAL_PORT(subDL, portID, net, label, nic, dlInfo, connected) \
	{portID, net, subDL, label, nic, dlInfo, connected}
#define BACSTAC_INIT_VIRTUAL_PORT(portID, net, label, connected) \
	{portID, net, NULL, label, BACSTAC_OSA_TEXT(""), NULL, connected}
#else /* CONFIG_NL_ROUTER */
/* `net' is not used in non-routing edition */
#define BACSTAC_INIT_PHYSICAL_PORT(subDL, portID, net, label, nic, dlInfo, connected) \
	{portID, net, subDL, label, nic, dlInfo, connected}
/* BACSTAC_INIT_VIRTUAL_PORT - only in routing edition */
#undef BACSTAC_INIT_VIRTUAL_PORT
#endif /* CONFIG_NL_ROUTER */

typedef struct tagBACSTAC_NL_CFG {
	BACSTAC_UINT portTableSize; /* must be between 1..255 */
	const BACSTAC_PORT_TABLE_CFG *portTable;

	/* maxPendingXmt: if a network is temporary unavailable,
	 * then BACstac defers all PDUs directed to it and put
	 * then into an internal queue. This parameter specifies
	 * the size of this queue */
	BACSTAC_UINT maxPendingXmt;

	/* maxKnownDnets: the maximal number of known nets
	 * It should be large enough to store information about
	 * each network in the BACnet internetwork */
	BACSTAC_UI16 maxKnownDnets;

	/* maxDnetsInSearch: the maximal number of networks
	 * that are BACstac is trying to find at one moment */
	BACSTAC_UI16 maxDnetsInSearch;

	/* findDnetTimeout: timeout used to find specified
	 * network, if the network is not found during this
	 * time all packets on it will be drop and it will
	 * be marked as unavailable */
	BACSTAC_UINT findDnetTimeout; /* ms */

#ifdef CONFIG_NL_ROUTER
	/* hopDecrement must be 1..255 */
	BACSTAC_BYTE hopDecrement;

#endif /* CONFIG_NL_ROUTER */
#ifdef CONFIG_NL_CONNECT
	/* connectTimeout is timeout for establishing a remote PTP
	 * connection by sending the Establish-Connection-To-Network
	 * message. */
	BACSTAC_UINT connectTimeout; /* ms */

	BACSTAC_UINT connectAckTimeout;

	/* ptpTermTimeValue specifies the termination time value field in
	 * the Establish-Connection-To-Network message. */
	BACSTAC_BYTE ptpTermTimeValue; /* sec, 0 means infinite */

#endif /* CONFIG_NL_CONNECT */
#ifdef CONFIG_NL_ROUTER
	/* ptpTermDalay is the connection termination delay.
	 *
	 * If one or more of the reachable networks listed in the
	 * I-Am-Router-To-Network message is reached through a directly
	 * connected PTP connection, transmitting the I-Am-Router-To-Network
	 * message shall start or restart a connection termination
	 * delay timer with this value.
	 *
	 * The default value specified for it in the BACnet standard
	 * is sixty second */
	BACSTAC_UINT ptpTermDelay; /* ms */

	/* ptpSafetyTimeout specifies PTP safety timeout, which is used
	 * to discover duplicated path by sending the  I-Am-Router-To-Network
	 * message. The value specified for it in the BACnet standard is 5 minutes.
	 *
	 * NOTE: Setting this value equal or less than ptpTermDelay
	 * will prevent termination of the connection by activity timer */
	BACSTAC_UINT ptpSafetyTimeout;

#endif /* CONFIG_NL_ROUTER */
	/* routerBusyTimeout: each Router-Busy-To-Network message curtails
	 * all traffic to the specified network until Router-Available-To-Network
	 * is received or this timeout is expired.
	 * The BACnet standard specifies its value as 30 sec, i.e. 30000 ms
	 * Do not change the default value unless you know what you do */
	BACSTAC_UINT routerBusyTimeout; /* ms */

#ifdef CONFIG_NL_ROUTER
	/* routerBroadcastPeriod specifies an interval used to broadcast
	 * the Who-Is-Router-To-Network message to all connected ports
	 * This timer is not required by the BACnet standard. If it is
	 * set to zero then no periodic broadcasts will be sent. */
	BACSTAC_UINT routerBroadcastPeriod; /* sec */
#endif /* CONFIG_NL_ROUTER */
	/* delayOnBusy specifies the maximum delay time in milliseconds for APDU
	 * to a busy network. If the network in the temporarily unreachable
	 * state for less than the specified value than packets from BACstac
	 * applications will be accepted and STATUS_OK is returned to the
	 * application. If the network remains in the busy state longer than
	 * this time, no new APDU to this network is accepted and the error
	 * DNET_BUSY is returned to the application.
	 *
	 * Zero value of this timeout has a special meaning: defer all packets
	 * to the busy network until the destination network will become
	 * available.
	 *
	 * The meaning of this option has been changed in BACstac 6.3. Now it
	 * does not affect routing. Starting with BACstac 6.3, upon receiving
	 * a message whose destination is one of the temporarily unreachable
	 * networks, BACstac sends a Reject-Message-To-Network message with a
	 * reject reason of 2 to the originating node.
	 */
	BACSTAC_UINT delayOnBusy; /* msec */
#ifdef CONFIG_NL_CONNECT
	BACSTAC_UINT ptpConnectDelay; /* msec */
#endif /* CONFIG_NL_CONNECT */
} BACSTAC_NL_CFG;

typedef struct tagBACSTAC_TSML_CFG {
	/* Put value in max-segments-accepted in the APDU header
	   as specified in BACnet 2001 or later. If this field is
	   BACSTAC_FALSE then max-segments-accepted is not set,
	   i.e. it works as specified in behavior of BACnet 1995. */
	BACSTAC_BOOLEAN apduMaxSegsPresent;
} BACSTAC_TSML_CFG;

typedef struct tagBACSTAC_APIL_CFG {
	BACSTAC_UINT cliTsmPoolSize; /* size of client TSM pool */
	BACSTAC_UINT srvTsmPoolSize; /* size of server TSM pool */
	BACSTAC_UINT apduLength; /* maximum APDU length, in octets */
	BACSTAC_UINT apduTimeout; /* APDU timeout, in milliseconds */
	BACSTAC_UINT numRetries; /* number of retries */
	BACSTAC_SEGMENTATION segmentation; /* segmentation mode */
	BACSTAC_UINT maxSegAccepted; /* maximal number of segments in an incoming message */
	BACSTAC_UINT maxSegToSend; /* maximal number of segments in an outgoing message */
	BACSTAC_UINT segWindowSize; /* segmented window size, in packets 1..127 */
	BACSTAC_UINT segTimeout; /* segmentation timeout */
	BACSTAC_UINT maxProcessIDs; /* maximum number of allocated processIDs */
	BACSTAC_BOOLEAN allProcessIDs; /* pass all ProcessIDs to hook, not only
				  those that were registered */
    BACSTAC_UINT deviceIdCacheSize; /* Size of deviceId resolver cache */
} BACSTAC_APIL_CFG;

/* Limits for DeviceID resolver cache */
#define MIN_DEVICE_ID_CACHE_SIZE 1
#define MAX_DEVICE_ID_CACHE_SIZE 65535

/* Limits for different pools */
#define MIN_CLI_TSM_POOL_SIZE           1
#define MAX_CLI_TSM_POOL_SIZE (BACSTAC_UINT_MAX / 1024)
#define MIN_SRV_TSM_POOL_SIZE           1
#define MAX_SRV_TSM_POOL_SIZE (BACSTAC_UINT_MAX / 1024)

#define MIN_PROCESS_ID_POOL_SIZE        0
#define MAX_PROCESS_ID_POOL_SIZE    65535

/* Limits for different APDU values */

/* The application should not set APDU values outside of the specified
 * limits or undefined behavior may occur, i.e. BACstac can generate
 * wrong responses or even crash! Always check the user supplied input!
 */

#define MIN_APDU_LENGTH             50
/*
 * MAX_APDU_LENGTH is the maximum length for all BACnet data links
 * However, some data links may have the maximum APDU length smaller
 * than this number. When BACstac verifies APDU parameters, it uses
 * the actually physical APDU length for the port to which BACstac
 * application is bound.
 */
#define MAX_APDU_LENGTH             61327 /* maximum for BACnet/SC */

#define MIN_APDU_WINDOW_SIZE        1
#define MAX_APDU_WINDOW_SIZE        127

/* The following limits are used by BACstac for sanity check */
#define MIN_APDU_TIMEOUT            10 /* 10 milliseconds */
#define MAX_APDU_TIMEOUT            3600000 /* 1 hour */
#define MIN_APDU_SEG_TIMEOUT        10 /* 10 milliseconds */
#define MAX_APDU_SEG_TIMEOUT        3600000 /* 1 hour */
#define MAX_APDU_RETRIES            256
#define MIN_APDU_SEGMENTS           2
#define MAX_APDU_SEGMENTS           65535
/* End of APDU limits */

/*
 * Callbacks to the user code that loads and saves the configuration
 */

/* STATUS CODE returned by callbacks */
typedef enum BACSTAC_CFG_STATUS
{
	BCS_OK                  = 0x0000, /* = STATUS_OK */
	BCS_UNKNOWN_ERROR       = 0x0002, /* = STATUS_UNKNOWN_ERROR */
	BCS_OUT_OF_MEMORY       = 0x0005, /* = STATUS_OUT_OF_MEMORY */
	BCS_OUT_OF_RESOURCES    = 0x0006, /* = STATUS_OUT_OF_RESOURCES */
	BCS_BAD_CONFIG          = 0x000A  /* = STATUS_BAD_CONFIG */
} BACSTAC_CFG_STATUS;

/*
 * Load different parts of the BACstac configuration.
 *
 * NOTE: BACSTAC_NL_CFG contains pointers to other structures, which may be
 * dynamically allocated. All these pointers and referred by them data must
 * remain valid and not modified in any way until BACstacCfgFreeNl is invoked
 * to free previously allocated memory.
 */
BACSTAC_CFG_STATUS BACstacCfgLoadApil(BACSTAC_APIL_CFG *cfg);
BACSTAC_CFG_STATUS BACstacCfgLoadTsml(BACSTAC_TSML_CFG *cfg);
BACSTAC_CFG_STATUS BACstacCfgLoadNl(BACSTAC_NL_CFG *cfg);
BACSTAC_CFG_STATUS BACstacCfgLoadDl(BACSTAC_DL_CFG *cfg);

/* Free memory used by pointers in the configuration */
void BACstacCfgFreeNl(BACSTAC_NL_CFG *cfg);

/* Callback when Port table is changed.
 *
 * BACstacCfgSaveNlPortTable saves the configuration into a permanent storage.
 * The reference to the old configuration is provided to see what parameters are
 * changed. If the configuration is invalid it returns BACSTAC_FALSE.
 *
 * IMPORTANT: BACstacCfgSaveNlPortTable MUST NOT alter directly or indirectly
 * the previous configuration returned by BACstacCfgLoadNl.
 */
BACSTAC_BOOLEAN BACstacCfgSaveNlPortTable(BACSTAC_UINT size,
	const BACSTAC_PORT_TABLE_CFG *table, const BACSTAC_PORT_TABLE_CFG *oldTable);

/* callback when DL-specific configuration is changed */
BACSTAC_BOOLEAN BACstacCfgSaveDlInfo(BACSTAC_UINT portIndex, const void *dlInfo);

/* Global configuration for all ports the specific type */
#ifdef CONFIG_SC
BACSTAC_CFG_STATUS BACstacCfgLoadScGlobal(BACSTAC_SC_GLOBAL_CFG *cfg);
void BACstacCfgFreeScGlobal(BACSTAC_SC_GLOBAL_CFG *cfg);
#endif /* CONFIG_SC */

typedef struct tagBACSTAC_OSA_CFG {
	void (*logger) (int priority, BACSTAC_OSA_CSTR format, va_list va);
	void (*panic) (const char *prefix, BACSTAC_OSA_CSTR format, va_list va);
} BACSTAC_OSA_CFG;

extern BACSTAC_OSA_CFG BACstacOsaCfg;

/* Log message format */
typedef enum BACSTAC_LOG_MSG_TYPE {
	/* Log Event Extended Format (LEEF) Version 2 */
	BLMT_LEEF,
	/* Human-readable format */
	BLMT_TEXT
} BACSTAC_LOG_MSG_TYPE;

typedef struct tagBACSTAC_LOG_MSG_CFG {
	BACSTAC_LOG_MSG_TYPE type;
} BACSTAC_LOG_MSG_CFG;

typedef enum BACSTAC_LOG_MSG_SUBSYSTEM
{
	// BACstac Core SubSystems
	// BACstac sub-DLs
	BLMS_ETHERNET,
	BLMS_MSTP,
	BLMS_IPV4,
	BLMS_IPV6,
	BLMS_SC,
	// End of subDLs
	// Other BACstac Core SubSystems
	BLMS_DL,       // common DL code
	BLMS_NL,       // Network layer
	BLMS_TSML,     // TSM layer
	BLMS_IPC_CORE, // IPC inside of BACstac Core
	BLMS_LAST_CORE = BLMS_IPC_CORE,
	// End of BACstac Core SubSystems
	// APIL SubSystems
	BLMS_IPC_APIL, // IPC client in APIL
	BLMS_PARSER,
	BLMS_CODER,
	BLMS_SERVICES,
	BLMS_DATABASE,
	BLMS_LAST_APIL = BLMS_DATABASE
	// End of APIL
} BACSTAC_LOG_MSG_SUBSYSTEM;

BACSTAC_CFG_STATUS BACstacCfgLoadLogMsg(BACSTAC_LOG_MSG_CFG *cfg,
	BACSTAC_LOG_MSG_LEVEL *logLevels);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* BACCFG_H */
