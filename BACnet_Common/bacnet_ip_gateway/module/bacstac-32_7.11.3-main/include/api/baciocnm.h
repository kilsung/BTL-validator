/*
 * BACstac(TM) BACIOCTL.H
 * Copyright (c) 2003-2025 by Cimetrics Inc.  All rights reserved.
 *
 * Purpose: define IOCTL number and status codes
 */
#ifndef BACIOCNM_H
#define BACIOCNM_H

#include "bacbase.h"

typedef BACSTAC_ENUM BACSTAC_IOCTL_STATUS;

/*
 * The list of known ioctl status
 */
#define BACSTAC_IOCTL_OK                  0
#define BACSTAC_IOCTL_NO_PERMISSION       1
#define BACSTAC_IOCTL_INVALID_NUM_ARG     2 /* Invalid number of agruments */
#define BACSTAC_IOCTL_INVALID_APP_ID      3 /* No such application */
#define BACSTAC_IOCTL_INVALID_PORT_ID     4 /* No device with such port ID */
#define BACSTAC_IOCTL_INVALID_PORT_TYPE   5 /* Not allowed ioctl request for this port */
#define BACSTAC_IOCTL_INVALID_PARAM       6 /* Invalid arguments in ioctl request */
#define BACSTAC_IOCTL_NOT_SUPPORTED       7 /* Ioctl request is not supported */
#define BACSTAC_IOCTL_BUSY                8 /* The resource is in use (blocked) */
#define BACSTAC_IOCTL_ALREADY_EXISTS      9 /* Attempt to create already existed resource */
#define BACSTAC_IOCTL_OUT_OF_MEMORY      10
#define BACSTAC_IOCTL_TOO_MANY_PORTS     11
#define BACSTAC_IOCTL_NO_PDU             12
#define BACSTAC_IOCTL_TOO_BIG            13
#define BACSTAC_IOCTL_NOT_FOUND          14 /* The specified resource is not found */
#define BACSTAC_IOCTL_UNKNOWN_ERROR      15 /* Internal error */

typedef BACSTAC_ENUM BACSTAC_IOCTL_REQUESTID;

/*
 * The following constant is for internal purpose only
 * Please, don't use them
 * FIXME: move them to another header
 */
#define BACSTAC_IOCTL_TSML_REQUEST      0x0000
#define BACSTAC_IOCTL_NL_REQUEST        0x0040
#define BACSTAC_IOCTL_DL_REQUEST        0x0080
#define BACSTAC_IOCTL_MSTP_REQUEST      0x00C0
#define BACSTAC_IOCTL_BIP_REQUEST       0x0100
#define BACSTAC_IOCTL_MAX_REQUEST       0x0FFF

/*
 * The list of known IOCTL requests
 */
#define BACSTAC_IOCTL_DAEMON_INFO           0
#define BACSTAC_IOCTL_DAEMON_STATUS         1
#define BACSTAC_IOCTL_PDU_STATISTICS        2
#define BACSTAC_IOCTL_PDU_STATUS            3
#define BACSTAC_IOCTL_IPC_INFO              4
#define BACSTAC_IOCTL_IPC_STATISTICS        5
#define BACSTAC_IOCTL_IPC_STATUS            6
#define BACSTAC_IOCTL_IPC_APP_STATISTICS    7
#define BACSTAC_IOCTL_IPC_APP_DISCONNECT    9
#define BACSTAC_IOCTL_APP_LIST_GET         10
#define BACSTAC_IOCTL_APP_INFO             11
#define BACSTAC_IOCTL_APP_STATISTICS       12
#define BACSTAC_IOCTL_APP_PID_LIST_GET     13
#define BACSTAC_IOCTL_APP_TSM_STATUS       14

#define BACSTAC_IOCTL_PORT_LIST_GET        64
#define BACSTAC_IOCTL_PORT_TO_NETS         65

#define BACSTAC_IOCTL_PORT_STATISTICS      68
#define BACSTAC_IOCTL_PORT_STATUS          69
#define BACSTAC_IOCTL_NL_TIMEOUTS_GET      72
#define BACSTAC_IOCTL_BUSY_NETS_GET        78
#define BACSTAC_IOCTL_PORT_ADDR_GET        79

/*
 * The list of MSTP IOCTL requests
 */
#define BACSTAC_IOCTL_MSTP_MAXINFOFRAMES_GET 192
#define BACSTAC_IOCTL_MSTP_MAXINFOFRAMES_SET 193
#define BACSTAC_IOCTL_MSTP_NMAXMASTER_GET    194
#define BACSTAC_IOCTL_MSTP_NMAXMASTER_SET    195


/*
 * The list of BIB IOCTL requests
 */
#define BACSTAC_IOCTL_BDT_READ               256
#define BACSTAC_IOCTL_BDT_WRITE              257
#define BACSTAC_IOCTL_BDT_SEND               258
#define BACSTAC_IOCTL_FDT_READ               259


/*
 * The list of SC IOCTL requests
 */
#define BACSTAC_IOCTL_SC_CONNECT             272
#define BACSTAC_IOCTL_SC_DISCONNECT          273
#define BACSTAC_IOCTL_SC_STATUS              274
#define BACSTAC_IOCTL_SC_DC_STATUS           275


/*
 * For backward compatibility
 * Don't use these names, they will be removed in the future!
 */
#define BACSTAC_IOCTL_GET_BUSY_NETS        78

#endif /* BACIOCNM_H */
