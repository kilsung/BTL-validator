/*
 * BACstac(TM) BACCLI.H
 * Copyright (c) 1996-2026 by Cimetrics Inc. All rights reserved.
 *
 * Purpose: define BACstac Client API routines, types and constants
 *
 */


#ifndef BACCLI_H
#define BACCLI_H

#include <bnetdef.h>
#include <bneterr.h>
#include <bacbase.h>
#include <bactypes.h>
#include <baccomm.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */



/*
 * BACstacClientInit
 */

#define BACSTAC_IF_USE_CHAR_STRING 0x01

/*
 * This flag indicates whether the pServiceInfo parameter of the BACstacReadPropertyEx2
 * function is input-output or input-only.i If this flag is set then pServiceInfo is
 * input-output, and it receives Object Identifier from ReadProperty-ACK if
 * the instance number in the request was unknown (i.e. 4194304). If this flag is not set
 * then this pServiceInfo is input only and Object Identifier value received in
 * ReadProperty-ACK is not visible to the application.
 */
#define BACSTAC_IF_RP_IN_OUT_INFO  0x08

typedef struct tagBACSTAC_CLI_INIT
{
    BACSTAC_UI8    BACstacVersionMajor;    /* BACSTAC_VERSION_MAJOR for this version */
    BACSTAC_UI8    BACstacVersionMinor;    /* BACSTAC_VERSION_MINOR for this version */
    BACSTAC_UI16   wPortID;                /* 0 means first physical port */
    BACSTAC_DEVICE_COUNT    maxDevices;
    BACSTAC_OBJECT_COUNT    maxObjects;
    BACSTAC_UINT   flags;
} BACSTAC_CLI_INIT;

BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacInitCliInit (
    BACSTAC_CLI_INIT *pInit,
    BACSTAC_UI8 BACstacVersionMajor,
    BACSTAC_UI8 BACstacVersionMinor
) BACSTAC_NOTHROW;

BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacSetCliInitPortID (
   BACSTAC_CLI_INIT *pInit,
   BACSTAC_UI16 wPortID
) BACSTAC_NOTHROW;

BACSTACAPI BACSTAC_UI16 BACstac_api BACstacGetCliInitPortID (
   const BACSTAC_CLI_INIT *pInit
) BACSTAC_NOTHROW;

BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacSetCliInitMaxDevices (
    BACSTAC_CLI_INIT *pInit,
    BACSTAC_DEVICE_COUNT nMaxDevices
) BACSTAC_NOTHROW;

BACSTACAPI BACSTAC_DEVICE_COUNT BACstac_api BACstacGetCliInitMaxDevices (
    const BACSTAC_CLI_INIT *pInit
) BACSTAC_NOTHROW;

BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacSetCliInitMaxObjects (
    BACSTAC_CLI_INIT *pInit,
    BACSTAC_OBJECT_COUNT nMaxObjects
) BACSTAC_NOTHROW;

BACSTACAPI BACSTAC_OBJECT_COUNT BACstac_api BACstacGetCliInitMaxObjects (
    const BACSTAC_CLI_INIT *pInit
) BACSTAC_NOTHROW;

BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacSetCliInitFlags (
    BACSTAC_CLI_INIT *pInit,
    BACSTAC_UINT flags
) BACSTAC_NOTHROW;

BACSTACAPI BACSTAC_UINT BACstac_api BACstacGetCliInitFlags (
   const BACSTAC_CLI_INIT *pInit
) BACSTAC_NOTHROW;


BACSTACAPI BACSTAC_STATUS BACstac_api BACstacClientInit (
    const BACSTAC_CLI_INIT *pInitData,
    const void *reserved
) BACSTAC_NOTHROW;


/*
 * BACstacCreateDeviceImage
 */

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacCreateDeviceImage (
    const BACSTAC_ADDRESS *pAddress,
    BACSTAC_INST_NUMBER instNumber,
    BACSTAC_HDEVICE *deviceHandle
) BACSTAC_NOTHROW;


/*
 * BACstacDeleteDeviceImage
 */


BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacDeleteDeviceImage (BACSTAC_HDEVICE hDevice);


/*
 * BACstacDeleteDeviceImages
 */


typedef enum BACSTAC_DEVICE_IMG_FILTER
{
    BDIF_ALL,     /* All devices */
    BDIF_AUTO,    /* Automatically created by default action */
    BDIF_MANUAL   /* Created by BACstacCreateDeviceImage */
} BACSTAC_DEVICE_IMG_FILTER;

BACSTACAPI BACSTAC_DEVICE_COUNT BACstac_api BACstacDeleteDeviceImages (
    BACSTAC_DEVICE_IMG_FILTER filter,
    BACSTAC_INST_NUMBER filterMin,
    BACSTAC_INST_NUMBER filterMax,
    const BACSTAC_ADDRESS *address /* in, optional */
) BACSTAC_NOTHROW;


/*
 * BACstacCreateObjectImage
 */

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacCreateObjectImage (
    BACSTAC_HDEVICE hDevice,
    const BACSTAC_OBJECT_ID *objectID,
    BACSTAC_HOBJECT *objectHandle
) BACSTAC_NOTHROW;


/*
 * BACstacDeleteObjectImage
 */


BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacDeleteObjectImage (BACSTAC_HOBJECT hObject);


/*
 * BACstacGetLocalAddress()
 */


BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacGetLocalAddress (
    BACSTAC_ADDRESS *pAddress
) BACSTAC_NOTHROW;




/*
 * DCC "shut-off" valve
 */


BACSTACAPI BACSTAC_STATUS BACstac_api BACstacSetClientCommControl (
    BACSTAC_DCC_VALUE dccValue
) BACSTAC_NOTHROW;

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacGetClientCommControl (
    BACSTAC_DCC_VALUE *pDccValue
) BACSTAC_NOTHROW;


/*
 * BACstacSetDeviceAPDUProperties sets the APDU properties to be used in
 * client requests directed to the given device.
 *
 * Initially APDU properties for a newly created device are set accordingly
 * to the default APDU properties, and then modified by BACstac automatically
 * based on property values received in I-Am and other requests.
 *
 * BACstacSetDeviceAPDUProperties sets the specified APDU properties (those
 * that have non-zero bit in their mask) to the given value and makes them
 * "fixed", so BACstac no longer changes them automatically.
 *
 * Note: Raw API functions (which uses device addresses instead of handles)
 * do not use these APDU properties, but only the default APDU properties if
 * others are not provided explicitly by the corresponding request parameter.
 */
BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacSetDeviceAPDUProperties (
    BACSTAC_HDEVICE hDevice,
    const BACSTAC_APDU_PROPERTIES *pAPDUProps
) BACSTAC_NOTHROW;

/*
 * BACstacGetDeviceAPDUProperties returns the current APDU properties for
 * the given device. All fields in the BACSTAC_APDU_PROPERTIES structure
 * are set to the currently used values, but only those fields that have
 * their bits set to non-zero in the APDU mask, are considered "fixed"
 * while the rest determine by BACstac automatically, based on default
 * values and values received in I-Am and other requests.
 */
BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacGetDeviceAPDUProperties (
    BACSTAC_HDEVICE hDevice,
    BACSTAC_APDU_PROPERTIES *pAPDUProps
) BACSTAC_NOTHROW;

/*
 * BACstacSetHook
 */


BACSTACAPI BACSTAC_STATUS BACstac_api BACstacSetHook (
    BACSTAC_HOOK_TYPE eHookType,
    BACSTAC_HOOK_PROC pfnHookProc
) BACSTAC_NOTHROW;

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacGetHook (
    BACSTAC_HOOK_TYPE eHookType,
    BACSTAC_HOOK_PROC *pfnHookProc /* out */
) BACSTAC_NOTHROW;

/*
 * BACstacGetRawAsn1Request returns the variable part of the APDU
 * specified by its handle.
 *
 * This function takes a handle of a server TSM as input parameter
 * and stores pointer to raw ASN.1 and its length to the variables
 * pointed by `rawAsn' and `rawAsnSize' parameters correspondingly.
 *
 * The pointer to raw ASN.1 obtained by this function is read-only
 * and will remain valid as long as the handle remains valid. Thus
 * after sending a response of any kind (normal response, error,
 * reject, or abort) or after returning from the hook with the
 * HOOK_STATUS_DEFAULT value, the value stored at this pointer is
 * undefined and must not be accessed. If an application needs the
 * data after complition of the transaction, it should copy them
 * into a separate.
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacGetRawAsn1Request (
   BACSTAC_HTSM       hTSM,
   const BACSTAC_BYTE **rawAsn,
   BACSTAC_VALUE_SIZE *rawAsnSize
) BACSTAC_NOTHROW;

/*
 * BACstacGetSourcePortID returns the identifier of the port on
 * which the request arrived or 0 if TSM handle is invalid.
 */

BACSTACAPI BACSTAC_UI8 BACstac_api BACstacGetSourcePortID (
   BACSTAC_HTSM hTSM
) BACSTAC_NOTHROW;


/*
 * BACstacSrvcError
 */

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacSrvcError (
    BACSTAC_HTSM hTransaction,
    BACSTAC_ERROR_CLASS errClass,
    BACSTAC_ERROR_CODE errCode
) BACSTAC_NOTHROW;


/*
 * BACstacSrvcAbort
 */

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacSrvcAbort (
    BACSTAC_HTSM hTransaction,
    BACSTAC_ABORT_REASON eAbortReason
) BACSTAC_NOTHROW;


/*
 * BACstacSrvcReject
 */

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacSrvcReject (
    BACSTAC_HTSM hTransaction,
    BACSTAC_REJECT_REASON rejectReason
) BACSTAC_NOTHROW;


/*
 * BACstacSrvcIgnore
 */


BACSTAC_DEPRECATED /* for backward compatibility only */
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacSrvcIgnore (
    BACSTAC_HTSM hTransaction
) BACSTAC_NOTHROW;


/*
 * BACstacDestroyTSM
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacDestroyTSM(
    BACSTAC_HTSM hTransaction
) BACSTAC_NOTHROW;


/*
 * BACstacSrvcResponse
 */

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacSrvcResponse (
    BACSTAC_HTSM hTransaction
) BACSTAC_NOTHROW;


/*
 * BACstacActivateNpoChanges
 */


BACSTACAPI BACSTAC_STATUS BACstac_api BACstacActivateNpoChanges (
    BACSTAC_HTSM hTransaction,  /* or BACSTAC_INVALID_HANDLE */
    BACSTAC_ERROR_TYPE *pError, /* or NULL */
    BACSTAC_BOOLEAN restart
) BACSTAC_NOTHROW;


/*
 * BACstacGetMaxRespSize returns the maximum size of the variable
 * part of the response in bytes.
 *
 * This function can be used to determine how large request can be
 * sent in the response by the ReadRange service.
 */

BACSTACAPI BACSTAC_VALUE_SIZE BACstac_api BACstacGetMaxRespSize (
    BACSTAC_HTSM hTransaction
) BACSTAC_NOTHROW;


/*                       ALARM AND EVENT SERVICES                        */

/*
 * Client routines: ProcessID operations
 */

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacRegisterProcessID (
    BACSTAC_UI32 processID
) BACSTAC_NOTHROW;

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacRegisterUniqueProcessID (
    BACSTAC_UI32 *processID
) BACSTAC_NOTHROW;

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacUnregisterProcessID (
    BACSTAC_UI32 processID
) BACSTAC_NOTHROW;

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacGetProcessIDList (
    BACSTAC_UI32 *processIDList,    /* optional */
    BACSTAC_UINT processIDBufferCount,
    BACSTAC_UINT *processIDAvailable
) BACSTAC_NOTHROW;



/*
 * Confirmed COV notification hook
 */

typedef BACSTAC_HOOK_STATUS (BACstac_hook *BACSTAC_CONF_COV_HOOK_PROC) (
    BACSTAC_HTSM hTransaction,
    BACSTAC_ADDRESS *sourceAddress,
    BACSTAC_ADDRESS *destinationAddress,
    const BACSTAC_COV_NOTIF_INFO *pServiceInfo
);

/*
 * Unconfirmed COV notification hook
 */

typedef BACSTAC_HOOK_STATUS (BACstac_hook *BACSTAC_UNCONF_COV_HOOK_PROC) (
    BACSTAC_HTSM hTransaction,  /* INVALID_HANDLE for unconfirmed services */
    BACSTAC_ADDRESS *sourceAddress,
    BACSTAC_ADDRESS *destinationAddress,
    const BACSTAC_COV_NOTIF_INFO *pServiceInfo
);

/*
 * Confirmed event notification hook
 */

typedef BACSTAC_HOOK_STATUS (BACstac_hook *BACSTAC_CONF_EVENT_NOTIF_HOOK_PROC) (
    BACSTAC_HTSM hTransaction,
    BACSTAC_ADDRESS *sourceAddress,
    BACSTAC_ADDRESS *destinationAddress,
    const BACSTAC_EVENT_NOTIF *pServiceInfo
);


/*
 * Unconfirmed event notification hook
 */

typedef BACSTAC_HOOK_STATUS (BACstac_hook *BACSTAC_UNCONF_EVENT_NOTIF_HOOK_PROC) (
    BACSTAC_HTSM hTransaction,  /* INVALID_HANDLE for unconfirmed services */
    BACSTAC_ADDRESS *sourceAddress,
    BACSTAC_ADDRESS *destinationAddress,
    const BACSTAC_EVENT_NOTIF *pServiceInfo
);


/*                         FILE ACCESS SERVICES                            */

/*                        OBJECT ACCESS SERVICES                           */
#ifdef BACSTAC_USE_64_BIT_READ_RANGE
   #define BACSTAC_READ_RANGE_HOOK_PROC BACSTAC_READ_RANGE_64_HOOK_PROC
   #define BACstacReadRangeResponse     BACstacReadRange64Response
   #define BACstacReadRangeAutoResponse BACstacReadRange64AutoResponse
#else  /* BACSTAC_USE_64_BIT_READ_RANGE */
   #define BACSTAC_READ_RANGE_HOOK_PROC BACSTAC_READ_RANGE_32_HOOK_PROC
   #define BACstacReadRangeResponse     BACstacReadRange32Response
   #define BACstacReadRangeAutoResponse BACstacReadRange32AutoResponse
#endif /* BACSTAC_USE_64_BIT_READ_RANGE */

/*                 REMOTE DEVICE MANAGEMENT SERVICES                       */

/*
 * UnconfirmedPrivateTransfer hook
 */

typedef BACSTAC_HOOK_STATUS (BACstac_hook *BACSTAC_UNCONF_PRIVATE_XFER_HOOK_PROC) (
    BACSTAC_HTSM hTransaction,
    BACSTAC_ADDRESS *sourceAddress,
    BACSTAC_ADDRESS *destinationAddress,
    const BACSTAC_PRIVATE_TRANSFER_INFO *pServiceInfo
);

/*
 * BACSTAC_I_HAVE_HOOK_PROC
 */

typedef BACSTAC_HOOK_STATUS (BACstac_hook *BACSTAC_I_HAVE_HOOK_PROC) (
    BACSTAC_HTSM hTransaction,
        /* INVALID_HANDLE since this is unconfirmed service */
    BACSTAC_ADDRESS *sourceAddress,
    BACSTAC_ADDRESS *destinationAddress,
    const BACSTAC_I_HAVE_INFO *pServiceInfo
);

/*
 * BACSTAC_I_AM_HOOK_PROC
 */

typedef BACSTAC_HOOK_STATUS (BACstac_hook *BACSTAC_I_AM_HOOK_PROC) (
    BACSTAC_HTSM hTransaction,
        /* INVALID_HANDLE since this is unconfirmed service */
    BACSTAC_ADDRESS *sourceAddress,
    BACSTAC_ADDRESS *destinationAddress,
    const BACSTAC_I_AM_INFO *pServiceInfo
);



/*
 * Confirmed COV notification multiple hook
 */

typedef BACSTAC_HOOK_STATUS(BACstac_hook *BACSTAC_CONF_COV_NOTIF_MULTI_HOOK_PROC) (
    BACSTAC_HTSM hTransaction,
    BACSTAC_ADDRESS *sourceAddress,
    BACSTAC_ADDRESS *destinationAddress,
    const BACSTAC_COV_NOTIF_MULTI_INFO *pServiceInfo
);

/*
 * Unconfirmed COV notification multiple hook
 */

typedef BACSTAC_HOOK_STATUS(BACstac_hook *BACSTAC_UNCONF_COV_NOTIF_MULTI_HOOK_PROC) (
    BACSTAC_HTSM hTransaction,  /* INVALID_HANDLE for unconfirmed services */
    BACSTAC_ADDRESS *sourceAddress,
    BACSTAC_ADDRESS *destinationAddress,
    const BACSTAC_COV_NOTIF_MULTI_INFO *pServiceInfo
);


/*
 * Unconfirmed audit notification hook
 */

typedef BACSTAC_HOOK_STATUS(BACstac_hook *BACSTAC_UNCONF_AUDIT_NOTIF_HOOK_PROC) (
    BACSTAC_HTSM hTransaction,  /* INVALID_HANDLE for unconfirmed services */
    BACSTAC_ADDRESS *sourceAddress,
    BACSTAC_ADDRESS *destinationAddress,
    const BACSTAC_AUDIT_NOTIF_INFO *pServiceInfo
);
/* Define BACSTAC_UNCONF_AUDIT_NOTIF_RAW_HOOK_PROC as alias to
 * BACSTAC_UNCONF_AUDIT_NOTIF_HOOK_PROC just to have the HOOK_PROC
 * name that matches its hook type: HOOK_UNCONF_AUDIT_NOTIF_RAW. */
typedef BACSTAC_UNCONF_AUDIT_NOTIF_HOOK_PROC BACSTAC_UNCONF_AUDIT_NOTIF_RAW_HOOK_PROC;


typedef BACSTAC_HOOK_STATUS(BACstac_hook *BACSTAC_WHO_AM_I_HOOK_PROC) (
    BACSTAC_HTSM hTransaction,
    BACSTAC_ADDRESS *sourceAddress,
    BACSTAC_ADDRESS *destinationAddress,
    const BACSTAC_WHO_AM_I_INFO *pServiceInfo
);


typedef void (BACstac_callback *BACSTAC_SIMPLE_ACK_ACR_PROC) (
    BACSTAC_HTSM hTransaction,
    const BACSTAC_ADDRESS *pSourceAddress,
    const BACSTAC_ADDRESS *pDestinationAddress,
    BACSTAC_STATUS status,
    BACSTAC_ERROR *pError
);

/*                       ALARM AND EVENT SERVICES                        */

/*
 * Acknowledge Alarm
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacAcknowledgeAlarm (
    BACSTAC_HDEVICE hInitDevice,
    const BACSTAC_ACK_ALARM_INFO *pAckAlarmParams,
    BACSTAC_ERROR *pError                          /* optional */
) BACSTAC_NOTHROW;

/*
 * AcknowledgeAlarmEx
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacAcknowledgeAlarmEx (
    const BACSTAC_ADDRESS         *pSourceAddress,      /* in, optional  */
    const BACSTAC_ADDRESS         *pDestinationAddress, /* in            */
    const BACSTAC_ACK_ALARM_INFO  *pAckAlarmParams,     /* in            */
    BACSTAC_SIMPLE_ACK_ACR_PROC   pfACR,                /* in, optional  */
    const BACSTAC_APDU_PROPERTIES *pAPDUParams,         /* in, optional  */
    BACSTAC_HTSM                  *phTransaction,       /* out, optional */
    BACSTAC_ERROR                 *pError               /* optional      */
) BACSTAC_NOTHROW;




typedef void (BACstac_callback *BACSTAC_GLOBAL_GROUP_UCOV_NOTIF_ACR) (
    const BACSTAC_ADDRESS *pSourceAddress,
    const BACSTAC_ADDRESS *pDestinationAddress,
    BACSTAC_STATUS status,
    const BACSTAC_GLOBAL_GROUP_UCOV_RESULT *result
);






/*
 * Get Alarm Summary
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacGetAlarmSummary (
    BACSTAC_HDEVICE hDevice,
    BACSTAC_ALARM_SUMMARY *pSummaryBuffer,  /* Optional */
    BACSTAC_ELEMENT_COUNT nBufferCount,
    BACSTAC_ELEMENT_COUNT *pSummaryAvailable,
    BACSTAC_ERROR *pError                   /* Optional */
) BACSTAC_NOTHROW;

/*
 * GetAlarmSummaryEx
 */

typedef void (BACstac_callback *BACSTAC_ALARM_SUMM_ACR_PROC) (
    BACSTAC_HTSM hTransaction,
    const BACSTAC_ADDRESS *pSourceAddress,
    const BACSTAC_ADDRESS *pDestinationAddress,
    BACSTAC_STATUS status,
    BACSTAC_ALARM_SUMMARY *pSummaryBuffer,
    BACSTAC_ELEMENT_COUNT nBufferCount,
    BACSTAC_ELEMENT_COUNT nSummariesAvailable,
    BACSTAC_ERROR *pError
);

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacGetAlarmSummaryEx (
    const BACSTAC_ADDRESS *pSourceAddress,      /* in            */
    const BACSTAC_ADDRESS *pDestinationAddress, /* in            */
    BACSTAC_ALARM_SUMM_ACR_PROC pfACR,          /* optional      */
    const BACSTAC_APDU_PROPERTIES *pAPDUParams, /* in, optional  */
    BACSTAC_HTSM *phTransaction,                /* out, optional */
    BACSTAC_ALARM_SUMMARY *pSummaryBuffer,      /* out, optional */
    BACSTAC_ELEMENT_COUNT nBufferCount,         /* in, optional  */
    BACSTAC_ELEMENT_COUNT *pSummaryAvailable,   /* out           */
    BACSTAC_ERROR *pError                       /* out, optional */
) BACSTAC_NOTHROW;

/*
 * Get Enrollment Summary
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacGetEnrollmentSummary (
    BACSTAC_HDEVICE hDevice,
    const BACSTAC_ENROLLMENT_FILTER *pFilter,
    BACSTAC_ENROLLMENT_SUMMARY *pSummaryBuffer, /* Optional */
    BACSTAC_ELEMENT_COUNT nBufferCount,
    BACSTAC_ELEMENT_COUNT *pSummaryAvailable,
    BACSTAC_ERROR *pError                       /* Optional */
) BACSTAC_NOTHROW;

/*
 * GetEnrollmentSummaryEx
 */

typedef void (BACstac_callback *BACSTAC_ENRL_SUMM_ACR_PROC) (
    BACSTAC_HTSM hTransaction,
    const BACSTAC_ADDRESS *pSourceAddress,
    const BACSTAC_ADDRESS *pDestinationAddress,
    BACSTAC_STATUS status,
    BACSTAC_ENROLLMENT_SUMMARY *pSummaryBuffer,
    BACSTAC_ELEMENT_COUNT nBufferCount,
    BACSTAC_ELEMENT_COUNT nSummaryAvailable,
    BACSTAC_ERROR *pError
);

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacGetEnrollmentSummaryEx (
    const BACSTAC_ADDRESS *pSourceAddress,      /* in, optional  */
    const BACSTAC_ADDRESS *pDestinationAddress, /* in            */
    const BACSTAC_ENROLLMENT_FILTER *pFilter,   /* in            */
    BACSTAC_ENRL_SUMM_ACR_PROC pfACR,           /* in, optional  */
    const BACSTAC_APDU_PROPERTIES *pAPDUParams, /* in, optional  */
    BACSTAC_HTSM *phTransaction,                /* out, optional */
    BACSTAC_ENROLLMENT_SUMMARY *pSummaryBuffer, /* out, optional */
    BACSTAC_ELEMENT_COUNT nBufferCount,         /* in, optional  */
    BACSTAC_ELEMENT_COUNT *pSummaryAvailable,   /* out           */
    BACSTAC_ERROR *pError                       /* out, optional */
) BACSTAC_NOTHROW;

/*
 * Get Event Information
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacGetEventInfo2 (
    BACSTAC_HDEVICE hDevice,
    const BACSTAC_EVENT_INFO_INFO *pServiceInfo, /* in,  optional */
    BACSTAC_EVENT_INFO *pInfoBuffer,             /* out           */
    BACSTAC_ERROR *pError,                       /* out, optional */
    BACSTAC_IALLOC *iAlloc
) BACSTAC_NOTHROW;

BACSTAC_DEPRECATED /* for backward compatibility only */
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacGetEventInfo (
    BACSTAC_HDEVICE hDevice,
    const BACSTAC_EVENT_INFO_INFO *pServiceInfo, /* In, optional */
    BACSTAC_EVENT_INFO *pInfoBuffer,             /* Out */
    BACSTAC_ERROR *pError                        /* Optional */
) BACSTAC_NOTHROW;

/*
 * BACstacGetEventInfoEx2
 */

typedef void (BACstac_callback *BACSTAC_EVENT_INFO_ACR_PROC) (
    BACSTAC_HTSM hTransaction,
    const BACSTAC_ADDRESS *pSourceAddress,
    const BACSTAC_ADDRESS *pDestinationAddress,
    BACSTAC_STATUS status,
    BACSTAC_EVENT_INFO *pInfoBuffer,
    BACSTAC_ERROR *pError
);

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacGetEventInfoEx2 (
    const BACSTAC_ADDRESS *pSourceAddress,       /* in            */
    const BACSTAC_ADDRESS *pDestinationAddress,  /* in            */
    const BACSTAC_EVENT_INFO_INFO *pServiceInfo, /* in,           */
    BACSTAC_EVENT_INFO_ACR_PROC pfACR,           /* in, optional  */
    const BACSTAC_APDU_PROPERTIES *pAPDUParams,  /* in, optional  */
    BACSTAC_HTSM *phTransaction,                 /* out, optional */
    BACSTAC_EVENT_INFO *pInfoBuffer,             /* out           */
    BACSTAC_ERROR *pError,                       /* out, optional */
    BACSTAC_IALLOC *iAlloc                       /* in, optional  */
) BACSTAC_NOTHROW;

BACSTAC_DEPRECATED /* for backward compatibility only */
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacGetEventInfoEx (
    const BACSTAC_ADDRESS *pSourceAddress,       /* in            */
    const BACSTAC_ADDRESS *pDestinationAddress,  /* in            */
    const BACSTAC_EVENT_INFO_INFO *pServiceInfo, /* in,           */
    BACSTAC_EVENT_INFO_ACR_PROC pfACR,           /* in, optional  */
    const BACSTAC_APDU_PROPERTIES *pAPDUParams,  /* in, optional  */
    BACSTAC_HTSM *phTransaction,                 /* out, optional */
    BACSTAC_EVENT_INFO *pInfoBuffer,             /* out           */
    BACSTAC_ERROR *pError                        /* out, optional */
) BACSTAC_NOTHROW;

/*
 * Life Safety Operation
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacLifeSafetyOperation (
    BACSTAC_HDEVICE hDevice,
    const BACSTAC_LIFE_SAFETY_INFO *pServiceInfo, /* In */
    BACSTAC_ERROR *pError                         /* Optional */
) BACSTAC_NOTHROW;

/*
 * LifeSafetyOperationEx
 */

/* use BACSTAC_SIMPLE_ACK_ACR_PROC for async notification */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacLifeSafetyOperationEx (
    const BACSTAC_ADDRESS *pSourceAddress,        /* in            */
    const BACSTAC_ADDRESS *pDestinationAddress,   /* in            */
    const BACSTAC_LIFE_SAFETY_INFO *pServiceInfo, /* in            */
    BACSTAC_SIMPLE_ACK_ACR_PROC pfACR,            /* in, optional  */
    const BACSTAC_APDU_PROPERTIES *pAPDUParams,   /* in, optional  */
    BACSTAC_HTSM *phTransaction,                  /* out, optional */
    BACSTAC_ERROR *pError                         /* out, optional */
) BACSTAC_NOTHROW;

/*
 * Subscribe COV
 */

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacSubscribeCOV (
    BACSTAC_HDEVICE hDevice,
    const BACSTAC_SUBSCRIBE_COV_INFO *pSubscriber,
    BACSTAC_ERROR *pError                          /* Optional */
) BACSTAC_NOTHROW;

/*
 * SubscribeCOVEx
 */

/* use BACSTAC_SIMPLE_ACK_ACR_PROC for async notification */

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacSubscribeCOVEx (
    const BACSTAC_ADDRESS *pSourceAddress,         /* in            */
    const BACSTAC_ADDRESS *pDestinationAddress,    /* in            */
    const BACSTAC_SUBSCRIBE_COV_INFO *pSubscriber, /* in            */
    BACSTAC_SIMPLE_ACK_ACR_PROC pfACR,             /* in, optional  */
    const BACSTAC_APDU_PROPERTIES *pAPDUParams,    /* in, optional  */
    BACSTAC_HTSM *phTransaction,                   /* out, optional */
    BACSTAC_ERROR *pError                          /* out, optional */
) BACSTAC_NOTHROW;

/*
 * Subscribe COV Property
 */

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacSubscribeCOVProp (
    BACSTAC_HDEVICE hDevice,
    const BACSTAC_SUBSCRIBE_COVP_INFO *pSubscriber,
    BACSTAC_ERROR *pError                           /* Optional */
) BACSTAC_NOTHROW;

/*
 * BACstacSubscribeCOVPropEx
 */

/* use BACSTAC_SIMPLE_ACK_ACR_PROC for async notification */

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacSubscribeCOVPropEx (
    const BACSTAC_ADDRESS *pSourceAddress,          /* in, optional  */
    const BACSTAC_ADDRESS *pDestinationAddress,     /* in            */
    const BACSTAC_SUBSCRIBE_COVP_INFO *pSubscriber, /* in            */
    BACSTAC_SIMPLE_ACK_ACR_PROC pfACR,              /* in, optional  */
    const BACSTAC_APDU_PROPERTIES *pAPDUParams,     /* in, optional  */
    BACSTAC_HTSM *phTransaction,                    /* out, optional */
    BACSTAC_ERROR *pError                           /* out, optional */
) BACSTAC_NOTHROW;


/*                         FILE ACCESS SERVICES                            */

/*
 * Atomic Read File
 */

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacReadFile2 (
    BACSTAC_HOBJECT hObject,
    const BACSTAC_READ_FILE_SPEC *pRequest,
    BACSTAC_READ_FILE_RESULT *pResult,
    BACSTAC_ERROR *pError,
    BACSTAC_IALLOC *iAlloc
) BACSTAC_NOTHROW;

BACSTAC_DEPRECATED /* for backward compatibility only */
BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacReadFile (
    BACSTAC_HOBJECT hObject,
    const BACSTAC_READ_FILE_SPEC *pRequest,
    BACSTAC_READ_FILE_RESULT *pResult,
    BACSTAC_ERROR *pError
) BACSTAC_NOTHROW;

/*
 * BACstacReadFileEx
 */

typedef void (BACstac_callback *BACSTAC_READ_FILE_ACR_PROC) (
    BACSTAC_HTSM hTransaction,
    const BACSTAC_ADDRESS *pSourceAddress,
    const BACSTAC_ADDRESS *pDestinationAddress,
    BACSTAC_STATUS status,
    BACSTAC_READ_FILE_RESULT *pResult,
    BACSTAC_ERROR *pError
);

BACSTAC_DEPRECATED /* for backward compatibility only */
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacReadFileEx (
   const BACSTAC_ADDRESS *pSourceAddress,      /* in, optional */
   const BACSTAC_ADDRESS *pDestinationAddress, /* in           */
   const BACSTAC_READ_FILE_INFO *pReadInfo,    /* in           */
   BACSTAC_READ_FILE_ACR_PROC pfACR,           /* in, optional */
   const BACSTAC_APDU_PROPERTIES *pAPDUParams, /* in, optional */
   BACSTAC_HTSM *phTransaction,                /* out,optional */
   BACSTAC_READ_FILE_RESULT *pResult,          /* out          */
   BACSTAC_ERROR *pError                       /* out,optional */
   );

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacReadFileEx2 (
    const BACSTAC_ADDRESS *pSourceAddress,      /* in           */
    const BACSTAC_ADDRESS *pDestinationAddress, /* in           */
    const BACSTAC_READ_FILE_INFO *pReadInfo,    /* in           */
    BACSTAC_READ_FILE_ACR_PROC pfACR,           /* in, optional */
    const BACSTAC_APDU_PROPERTIES *pAPDUParams, /* in, optional */
    BACSTAC_HTSM *phTransaction,                /* out,optional */
    BACSTAC_READ_FILE_RESULT *pResult,          /* out          */
    BACSTAC_ERROR *pError,                      /* out,optional */
    BACSTAC_IALLOC *iAlloc                      /* in, optional */
) BACSTAC_NOTHROW;

/*
 * BACstacWriteFile
 */

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacWriteFile (
    BACSTAC_HOBJECT hObject,
    const BACSTAC_WRITE_FILE_SPEC *pRequest,
    BACSTAC_WRITE_FILE_RESULT *pResult,
    BACSTAC_ERROR *pError
) BACSTAC_NOTHROW;

/*
 * BACstacWriteFileEx
 */

typedef void (BACstac_callback *BACSTAC_WRITE_FILE_ACR_PROC) (
    BACSTAC_HTSM hTransaction,
    const BACSTAC_ADDRESS *pSourceAddress,
    const BACSTAC_ADDRESS *pDestinationAddress,
    BACSTAC_STATUS status,
    BACSTAC_WRITE_FILE_RESULT *pResult,
    BACSTAC_ERROR *pError
);

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacWriteFileEx (
    const BACSTAC_ADDRESS *pSourceAddress,      /* in           */
    const BACSTAC_ADDRESS *pDestinationAddress, /* in           */
    const BACSTAC_WRITE_FILE_INFO *pWriteInfo,  /* in           */
    BACSTAC_WRITE_FILE_ACR_PROC pfACR,          /* in,optional  */
    const BACSTAC_APDU_PROPERTIES *pAPDUParams, /* in, optional */
    BACSTAC_HTSM *phTransaction,                /* out,optional */
    BACSTAC_WRITE_FILE_RESULT *pResult,         /* out          */
    BACSTAC_ERROR *pError                       /* out,optional */
) BACSTAC_NOTHROW;

/*                         OBJECT ACCESS SERVICES                          */

/*
 * BACstacAddListElement
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacAddListElement (
    BACSTAC_HOBJECT hObject,
    BACSTAC_PROPERTY_ID ePropertyID,
    BACSTAC_ARRAY_INDEX nIndex,                   /* or VOID */
    const BACSTAC_PROPERTY_CONTENTS *pElements,
    BACSTAC_ERROR *pError,                        /* or NULL */
    BACSTAC_ELEMENT_COUNT *pFirstFailed           /* or NULL */
) BACSTAC_NOTHROW;

typedef void (BACstac_callback *BACSTAC_CHANGE_LIST_ACR_PROC) (
    BACSTAC_HTSM hTransaction,
    const BACSTAC_ADDRESS *pSourceAddress,
    const BACSTAC_ADDRESS *pDestinationAddress,
    BACSTAC_STATUS status,
    BACSTAC_ERROR *pError,
    BACSTAC_ELEMENT_COUNT nFirstFailed
);

/*
 * BACstacAddListElementEx
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacAddListElementEx (
    const BACSTAC_ADDRESS *pSourceAddress,      /* in            */
    const BACSTAC_ADDRESS *pDestinationAddress, /* in            */
    const BACSTAC_CHANGE_LIST_INFO *pAddInfo,   /* in            */
    BACSTAC_CHANGE_LIST_ACR_PROC pfACR,         /* in, optional  */
    const BACSTAC_APDU_PROPERTIES *pAPDUParams, /* in, optional  */
    BACSTAC_HTSM *phTransaction,                /* out, optional */
    BACSTAC_ERROR *pError,                      /* out, optional */
    BACSTAC_ELEMENT_COUNT *pFirstFailed         /* out, optional */
) BACSTAC_NOTHROW;

/*
 * BACstacRemoveListElement
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacRemoveListElement (
    BACSTAC_HOBJECT hObject,
    BACSTAC_PROPERTY_ID ePropertyID,
    BACSTAC_ARRAY_INDEX nIndex,                 /* or VOID */
    const BACSTAC_PROPERTY_CONTENTS *pElements,
    BACSTAC_ERROR *pError,                      /* or NULL */
    BACSTAC_ELEMENT_COUNT *pFirstFailed         /* or NULL */
) BACSTAC_NOTHROW;

/*
 * BACstacRemoveListElementEx
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacRemoveListElementEx (
    const BACSTAC_ADDRESS *pSourceAddress,       /* in            */
    const BACSTAC_ADDRESS *pDestinationAddress,  /* in            */
    const BACSTAC_CHANGE_LIST_INFO *pRemoveInfo, /* in            */
    BACSTAC_CHANGE_LIST_ACR_PROC pfACR,          /* in, optional  */
    const BACSTAC_APDU_PROPERTIES *pAPDUParams,  /* in, optional  */
    BACSTAC_HTSM *phTransaction,                 /* out, optional */
    BACSTAC_ERROR *pError,                       /* out, optional */
    BACSTAC_ELEMENT_COUNT *pFirstFailed          /* out, optional */
) BACSTAC_NOTHROW;

/*
 * BACstacCreateObject
 */

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacCreateObject (
    BACSTAC_HDEVICE hDevice,
    const BACSTAC_OBJECT_SPECIFIER *pObjectSpecifier,
    const BACSTAC_PROPERTY_VALUE *pValues,            /* in, optional */
    BACSTAC_PROPERTY_COUNT nValuesCount,
    BACSTAC_OBJECT_ID *pObjectID,                     /* out, optional */
    BACSTAC_ERROR *pError,                            /* out, optional */
    BACSTAC_PROPERTY_COUNT *pFirstFailed              /* out, optional */
) BACSTAC_NOTHROW;

/*
 * BACstacCreateObjectEx
 */


typedef void (BACstac_callback *BACSTAC_CREATE_OBJECT_ACR_PROC) (
    BACSTAC_HTSM hTransaction,
    const BACSTAC_ADDRESS *pSourceAddress,
    const BACSTAC_ADDRESS *pDestinationAddress,
    BACSTAC_STATUS status,
    BACSTAC_OBJECT_ID *pObjectID,
    BACSTAC_ERROR *pError,
    BACSTAC_PROPERTY_COUNT *pFirstFailed
);

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacCreateObjectEx (
    const BACSTAC_ADDRESS *pSourceAddress,          /* in, optional  */
    const BACSTAC_ADDRESS *pDestinationAddress,     /* in            */
    const BACSTAC_CREATE_OBJECT_INFO *pServiceInfo, /* in            */
    BACSTAC_CREATE_OBJECT_ACR_PROC pfACR,           /* in, optional  */
    const BACSTAC_APDU_PROPERTIES *pAPDUParams,     /* in, optional  */
    BACSTAC_HTSM *phTransaction,                    /* out, optional */
    BACSTAC_OBJECT_ID *pObjectID,                   /* out, optional */
    BACSTAC_ERROR *pError,                          /* out, optional */
    BACSTAC_PROPERTY_COUNT *pFirstFailed            /* out, optional */
) BACSTAC_NOTHROW;

/*
 * BACstacDeleteObject
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacDeleteObject (
    BACSTAC_HDEVICE hDevice,
    const BACSTAC_OBJECT_ID *pObjectID,
    BACSTAC_ERROR *pError                       /* out, optional */
) BACSTAC_NOTHROW;

/*
 * BACstacDeleteObjectEx
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacDeleteObjectEx (
    const BACSTAC_ADDRESS *pSourceAddress,      /* in            */
    const BACSTAC_ADDRESS *pDestinationAddress, /* in            */
    const BACSTAC_OBJECT_ID *pObjectID,         /* in            */
    BACSTAC_SIMPLE_ACK_ACR_PROC pfACR,          /* in, optional  */
    const BACSTAC_APDU_PROPERTIES *pAPDUParams, /* in, optional  */
    BACSTAC_HTSM *phTransaction,                /* out, optional */
    BACSTAC_ERROR *pError                       /* out, optional */
) BACSTAC_NOTHROW;

/*
 * BACstacReadProperty
 */

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacReadProperty2 (
    BACSTAC_HOBJECT           hObject,
    BACSTAC_PROPERTY_ID       ePropertyID,
    BACSTAC_ARRAY_INDEX       nIndex,
    BACSTAC_PROPERTY_CONTENTS *pContents,
    BACSTAC_ERROR             *pError,
    BACSTAC_IALLOC            *iAlloc
) BACSTAC_NOTHROW;

BACSTAC_DEPRECATED /* for backward compatibility only */
BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacReadProperty (
    BACSTAC_HOBJECT           hObject,
    BACSTAC_PROPERTY_ID       ePropertyID,
    BACSTAC_ARRAY_INDEX       nIndex,
    BACSTAC_PROPERTY_CONTENTS *pContents,
    BACSTAC_ERROR             *pError
) BACSTAC_NOTHROW;

/*
 * BACstacReadObjectList
 */


#define Query_Obj_List_Length(hDev,p,err)           \
    BACstacReadObjectList(hDev,NULL,0,p,err)
#define Objs_Copied(buff_len,available)             \
    (((buff_len) < (available)) ? (buff_len) : (available))

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacReadObjectList (
    BACSTAC_HDEVICE hDevice,
    BACSTAC_OBJECT_ID *pObjectListBuffer,
    BACSTAC_OBJECT_COUNT nObjectListBufferCount,
    BACSTAC_OBJECT_COUNT *pObjectsAvailable,
    BACSTAC_ERROR *pError
) BACSTAC_NOTHROW;

/*
 * ReadPropertyEx
 */

typedef void (BACstac_callback *BACSTAC_READ_PROP_ACR_PROC) (
    BACSTAC_HTSM hTransaction,
    const BACSTAC_ADDRESS *pSourceAddress,
    const BACSTAC_ADDRESS *pDestinationAddress,
    BACSTAC_STATUS status,
    BACSTAC_READ_INFO *pRequestInfo,
    BACSTAC_PROPERTY_CONTENTS *pContents,
    BACSTAC_ERROR *pError
);

/* IMPORTANT: In case of an asynchronous request (i.e. pfACR is not NULL), the
 * variable pointed by pServiceInfo should remain alive and not modified until
 * the completion routine is invoked. */
BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacReadPropertyEx2 (
    const BACSTAC_ADDRESS         *pSourceAddress,      /* in, optional  */
    const BACSTAC_ADDRESS         *pDestinationAddress, /* in            */
    BACSTAC_READ_INFO             *pServiceInfo,        /* see BACSTAC_IF_RP_IN_OUT_INFO */
    BACSTAC_READ_PROP_ACR_PROC    pfACR,                /* in, optional  */
    const BACSTAC_APDU_PROPERTIES *pAPDUParams,         /* in, optional  */
    BACSTAC_HTSM                  *phTransaction,       /* out, optional */
    BACSTAC_PROPERTY_CONTENTS     *pContents,           /* out           */
    BACSTAC_ERROR                 *pError,              /* out, optional */
    BACSTAC_IALLOC                *iAlloc               /* in, optional  */
) BACSTAC_NOTHROW;

BACSTAC_DEPRECATED /* for backward compatibility only */
BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacReadPropertyEx (
    const BACSTAC_ADDRESS         *pSourceAddress,      /* in, optional  */
    const BACSTAC_ADDRESS         *pDestinationAddress, /* in            */
    BACSTAC_READ_INFO             *pServiceInfo,        /* in            */
    BACSTAC_READ_PROP_ACR_PROC    pfACR,                /* in, optional  */
    const BACSTAC_APDU_PROPERTIES *pAPDUParams,         /* in, optional  */
    BACSTAC_HTSM                  *phTransaction,       /* out, optional */
    BACSTAC_PROPERTY_CONTENTS     *pContents,           /* out           */
    BACSTAC_ERROR                 *pError               /* out, optional */
) BACSTAC_NOTHROW;



/*
 * BACstacReadPropertyConditionalEx
 */

typedef void (BACstac_callback *BACSTAC_READ_PROP_CON_ACR_PROC) (
    BACSTAC_HTSM hTransaction,
    const BACSTAC_ADDRESS *pSourceAddress,
    const BACSTAC_ADDRESS *pDestinationAddress,
    BACSTAC_STATUS status,
    BACSTAC_RPC_RESULT *pResult,
    BACSTAC_ERROR *pError
);

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacReadPropertyConditionalEx2 (
   const BACSTAC_ADDRESS *pSourceAddress,      /* in, optional  */
   const BACSTAC_ADDRESS *pDestinationAddress, /* in            */
   const BACSTAC_READ_CON_INFO *pServiceInfo,  /* in            */
   BACSTAC_READ_PROP_CON_ACR_PROC pfACR,       /* in, optional  */
   const BACSTAC_APDU_PROPERTIES *pAPDUParams, /* in, optional  */
   BACSTAC_HTSM *phTransaction,                /* out, optional */
   BACSTAC_RPC_RESULT *pResult,                /* out           */
   BACSTAC_ERROR *pError,                      /* out, optional */
   BACSTAC_IALLOC *iAlloc                      /* in, optional  */
) BACSTAC_NOTHROW;

BACSTAC_DEPRECATED /* for backward compatibility only */
BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacReadPropertyConditionalEx (
   const BACSTAC_ADDRESS *pSourceAddress,      /* in, optional  */
   const BACSTAC_ADDRESS *pDestinationAddress, /* in            */
   const BACSTAC_READ_CON_INFO *pServiceInfo,  /* in            */
   BACSTAC_READ_PROP_CON_ACR_PROC pfACR,       /* in, optional  */
   const BACSTAC_APDU_PROPERTIES *pAPDUParams, /* in, optional  */
   BACSTAC_HTSM *phTransaction,                /* out, optional */
   BACSTAC_RPC_RESULT *pResult,                /* out           */
   BACSTAC_ERROR *pError                       /* out, optional */
) BACSTAC_NOTHROW;


/*
 * BACstacReadPropertyMultiple
 */
BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacReadPropertyMultiple2 (
    BACSTAC_HDEVICE          hDevice,
    BACSTAC_READ_LIST        *readAccessSpec,
    BACSTAC_READ_RESULT_LIST *readAccessResult,
    BACSTAC_OBJECT_COUNT     nObjectCount,
    BACSTAC_ERROR            *pError,           /* or NULL */
    BACSTAC_IALLOC           *iAlloc            /* or NULL */
) BACSTAC_NOTHROW;

BACSTAC_DEPRECATED /* for backward compatibility only */
BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacReadPropertyMultiple (
    BACSTAC_HDEVICE          hDevice,
    BACSTAC_READ_LIST        *readAccessSpec,
    BACSTAC_READ_RESULT_LIST *readAccessResult,
    BACSTAC_OBJECT_COUNT     nObjectCount,
    BACSTAC_ERROR            *pError            /* or NULL */
) BACSTAC_NOTHROW;

/*
 * ReadPropertyMultipleEx
 */

typedef void (BACstac_callback *BACSTAC_READ_PROP_MUL_ACR_PROC) (
    BACSTAC_HTSM hTransaction,
    const BACSTAC_ADDRESS *pSourceAddress,
    const BACSTAC_ADDRESS *pDestinationAddress,
    BACSTAC_STATUS status,
    BACSTAC_READ_RAW_RESULT_LIST *readAccessResult,
    BACSTAC_OBJECT_COUNT nResultListCount,
    BACSTAC_ERROR *pError
);

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacReadPropertyMultipleEx2 (
    const BACSTAC_ADDRESS            *pSourceAddress,      /* in, optional  */
    const BACSTAC_ADDRESS            *pDestinationAddress, /* in            */
    const BACSTAC_READ_MUL_HOOK_INFO *pServiceInfo,        /* in            */
    BACSTAC_READ_PROP_MUL_ACR_PROC   pfACR,                /* in, optional  */
    const BACSTAC_APDU_PROPERTIES    *pAPDUParams,         /* in, optional  */
    BACSTAC_HTSM                     *phTransaction,       /* out, optional */
    BACSTAC_READ_RAW_RESULT_LIST     *readAccessResult,    /* out           */
    BACSTAC_ERROR                    *pError,              /* out, optional */
    BACSTAC_IALLOC                   *iAlloc               /* in, optional  */
) BACSTAC_NOTHROW;

BACSTAC_DEPRECATED /* for backward compatibility only */
BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacReadPropertyMultipleEx (
    const BACSTAC_ADDRESS            *pSourceAddress,      /* in, optional  */
    const BACSTAC_ADDRESS            *pDestinationAddress, /* in            */
    const BACSTAC_READ_MUL_HOOK_INFO *pServiceInfo,        /* in            */
    BACSTAC_READ_PROP_MUL_ACR_PROC   pfACR,                /* in, optional  */
    const BACSTAC_APDU_PROPERTIES    *pAPDUParams,         /* in, optional  */
    BACSTAC_HTSM                     *phTransaction,       /* out, optional */
    BACSTAC_READ_RAW_RESULT_LIST     *readAccessResult,    /* out           */
    BACSTAC_ERROR                    *pError               /* out, optional */
) BACSTAC_NOTHROW;


#ifdef BACSTAC_USE_64_BIT_READ_RANGE
   #define BACstacReadRange3             BACstacReadRange64v3
   #define BACSTAC_READ_RANGE_ACR_PROC   BACSTAC_READ_RANGE_64_ACR_PROC
   #define BACstacReadRangeEx2           BACstacReadRange64Ex2
#else  /* BACSTAC_USE_64_BIT_READ_RANGE */
   #define BACstacReadRange3             BACstacReadRange32v3
   #define BACSTAC_READ_RANGE_ACR_PROC   BACSTAC_READ_RANGE_32_ACR_PROC
   #define BACstacReadRangeEx2           BACstacReadRange32Ex2
#endif /* BACSTAC_USE_64_BIT_READ_RANGE */

/*
 * BACstacReadRange
 */

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacReadRange64v3 (            /* Add-135-2001b */
    BACSTAC_HOBJECT hObject,                  /* in */
    BACSTAC_PROPERTY_ID ePropertyID,          /* in */
    BACSTAC_ARRAY_INDEX nIndex,               /* in */
    const BACSTAC_READ_RANGE_64_SPEC *pRange, /* in, a tagged type */
    BACSTAC_BIT_STRING *pResultFlags,         /* out */
    BACSTAC_PROPERTY_CONTENTS *pContents,     /* out */
    BACSTAC_UI64 *pFirstSequenceNum,          /* out or NULL */
    BACSTAC_ERROR *pError,                    /* out or NULL */
    BACSTAC_IALLOC *iAlloc                    /* in or NULL */
) BACSTAC_NOTHROW;

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacReadRange32v3 (            /* Add-135-2001b */
    BACSTAC_HOBJECT hObject,                  /* in */
    BACSTAC_PROPERTY_ID ePropertyID,          /* in */
    BACSTAC_ARRAY_INDEX nIndex,               /* in */
    const BACSTAC_READ_RANGE_32_SPEC *pRange, /* in, a tagged type */
    BACSTAC_BIT_STRING *pResultFlags,         /* out */
    BACSTAC_PROPERTY_CONTENTS *pContents,     /* out */
    BACSTAC_UI32 *pFirstSequenceNum,          /* out or NULL */
    BACSTAC_ERROR *pError,                    /* out or NULL */
    BACSTAC_IALLOC *iAlloc                    /* in or NULL */
) BACSTAC_NOTHROW;

BACSTAC_DEPRECATED /* for backward compatibility only */
BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacReadRange2 (
    BACSTAC_HOBJECT hObject,                  /* in */
    BACSTAC_PROPERTY_ID ePropertyID,          /* in */
    BACSTAC_ARRAY_INDEX nIndex,               /* in */
    const BACSTAC_READ_RANGE_32_SPEC *pRange, /* in, a tagged type */
    BACSTAC_BIT_STRING *pResultFlags,         /* out */
    BACSTAC_PROPERTY_CONTENTS *pContents,     /* out */
    BACSTAC_UI32 *pFirstSequenceNum,          /* out or NULL */
    BACSTAC_ERROR *pError                     /* out or NULL */
) BACSTAC_NOTHROW;

/*
 * BACstacReadRangeEx
 */

typedef void (BACstac_callback *BACSTAC_READ_RANGE_64_ACR_PROC) (
    BACSTAC_HTSM hTransaction,
    const BACSTAC_ADDRESS *pSourceAddress,
    const BACSTAC_ADDRESS *pDestinationAddress,
    BACSTAC_STATUS status,
    BACSTAC_READ_RANGE_64_RAW_RESULT *pResult,
    BACSTAC_ERROR *pError
);

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacReadRange64Ex2 (
    const BACSTAC_ADDRESS            *pSourceAddress,      /* in, optional  */
    const BACSTAC_ADDRESS            *pDestinationAddress, /* in            */
    const BACSTAC_READ_RANGE_64_INFO *pReadInfo,           /* in            */
    BACSTAC_READ_RANGE_64_ACR_PROC   pfACR,                /* in, optional  */
    const BACSTAC_APDU_PROPERTIES    *pAPDUParams,         /* in, optional  */
    BACSTAC_HTSM                     *phTransaction,       /* out, optional */
    BACSTAC_READ_RANGE_64_RAW_RESULT *pResult,             /* out           */
    BACSTAC_ERROR                    *pError,              /* out, optional */
    BACSTAC_IALLOC                   *iAlloc               /* in or NULL    */
) BACSTAC_NOTHROW;

typedef void (BACstac_callback *BACSTAC_READ_RANGE_32_ACR_PROC) (
    BACSTAC_HTSM hTransaction,
    const BACSTAC_ADDRESS *pSourceAddress,
    const BACSTAC_ADDRESS *pDestinationAddress,
    BACSTAC_STATUS status,
    BACSTAC_READ_RANGE_32_RAW_RESULT *pResult,
    BACSTAC_ERROR *pError
);

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacReadRange32Ex2 (
    const BACSTAC_ADDRESS            *pSourceAddress,      /* in, optional  */
    const BACSTAC_ADDRESS            *pDestinationAddress, /* in            */
    const BACSTAC_READ_RANGE_32_INFO *pReadInfo,           /* in            */
    BACSTAC_READ_RANGE_32_ACR_PROC   pfACR,                /* in, optional  */
    const BACSTAC_APDU_PROPERTIES    *pAPDUParams,         /* in, optional  */
    BACSTAC_HTSM                     *phTransaction,       /* out, optional */
    BACSTAC_READ_RANGE_32_RAW_RESULT *pResult,             /* out           */
    BACSTAC_ERROR                    *pError,              /* out, optional */
    BACSTAC_IALLOC                   *iAlloc               /* in or NULL */
) BACSTAC_NOTHROW;

BACSTAC_DEPRECATED /* for backward compatibility only */
BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacReadRangeEx (
    const BACSTAC_ADDRESS            *pSourceAddress,      /* in, optional  */
    const BACSTAC_ADDRESS            *pDestinationAddress, /* in            */
    const BACSTAC_READ_RANGE_32_INFO *pReadInfo,           /* in            */
    BACSTAC_READ_RANGE_32_ACR_PROC   pfACR,                /* in, optional  */
    const BACSTAC_APDU_PROPERTIES    *pAPDUParams,         /* in, optional  */
    BACSTAC_HTSM                     *phTransaction,       /* out, optional */
    BACSTAC_READ_RANGE_32_RAW_RESULT *pResult,             /* out           */
    BACSTAC_ERROR                    *pError               /* out, optional */
) BACSTAC_NOTHROW;

/*
 * BACstacWriteProperty
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacWriteProperty (
    BACSTAC_HOBJECT hObject,
    BACSTAC_PROPERTY_ID ePropertyID,
    BACSTAC_ARRAY_INDEX nIndex,
    BACSTAC_PRIORITY_LEVEL nPriority,
    const BACSTAC_PROPERTY_CONTENTS *pContents,
    BACSTAC_ERROR *pError
) BACSTAC_NOTHROW;

/*
 * WritePropertyEx
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacWritePropertyEx (
    const BACSTAC_ADDRESS *pSourceAddress,      /* in            */
    const BACSTAC_ADDRESS *pDestinationAddress, /* in            */
    const BACSTAC_WRITE_INFO *pWriteInfo,       /* in            */
    BACSTAC_SIMPLE_ACK_ACR_PROC pfACR,          /* in, optional  */
    const BACSTAC_APDU_PROPERTIES *pAPDUParams, /* in, optional  */
    BACSTAC_HTSM *phTransaction,                /* out, optional */
    BACSTAC_ERROR *pError                       /* out, optional */
) BACSTAC_NOTHROW;

/*
 * BACstacWritePropertyMultiple
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacWritePropertyMultiple (
    BACSTAC_HDEVICE hDevice,
    BACSTAC_WRITE_LIST *writeAccessSpec,
    BACSTAC_OBJECT_COUNT nAccessSpecCount,
    BACSTAC_ERROR *pError,                 /* or NULL */
    BACSTAC_OBJ_PROP_REF *pFirstFailed     /* or NULL */
) BACSTAC_NOTHROW;

/*
 * WritePropertyMultipleEx
 */


typedef void (BACstac_callback *BACSTAC_WRITE_PROP_MUL_ACR_PROC) (
    BACSTAC_HTSM hTransaction,
    const BACSTAC_ADDRESS *pSourceAddress,
    const BACSTAC_ADDRESS *pDestinationAddress,
    BACSTAC_STATUS status,
    BACSTAC_ERROR *pError,
    BACSTAC_OBJ_PROP_REF *pFirstFailed
);

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacWritePropertyMultipleEx (
    const BACSTAC_ADDRESS *pSourceAddress,           /* in            */
    const BACSTAC_ADDRESS *pDestinationAddress,      /* in            */
    const BACSTAC_WRITE_PROP_MUL_INFO *pServiceInfo, /* in            */
    BACSTAC_WRITE_PROP_MUL_ACR_PROC pfACR,           /* in, optional  */
    const BACSTAC_APDU_PROPERTIES *pAPDUParams,      /* in, optional  */
    BACSTAC_HTSM *phTransaction,                     /* out, optional */
    BACSTAC_ERROR *pError,                           /* out, optional */
    BACSTAC_OBJ_PROP_REF *pFirstFailed               /* out, optional */
) BACSTAC_NOTHROW;

/*                 REMOTE DEVICE MANAGEMENT SERVICES                       */

/*
 * Device Communication Control
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacDeviceCommControl (
    BACSTAC_HDEVICE hDevice,
    BACSTAC_UNSIGNED nTimeDuration,     /* or INDEFINITE */
    BACSTAC_DCC_VALUE dccValue,
    const BACSTAC_CHAR *pszPassword,/* optional, <= BACSTAC_MAX_DCC_PASSWORD_CHARS */
    BACSTAC_ERROR *pError   /* optional */
) BACSTAC_NOTHROW;

/*
 * BACstacDeviceCommControlEx
 */

/* use BACSTAC_SIMPLE_ACK_ACR_PROC for async notification */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacDeviceCommControlEx (
    const BACSTAC_ADDRESS *pSourceAddress,      /* in            */
    const BACSTAC_ADDRESS *pDestinationAddress, /* in            */
    const BACSTAC_DCC_INFO *pDccParams,         /* in            */
    BACSTAC_SIMPLE_ACK_ACR_PROC pfACR,          /* in, optional  */
    const BACSTAC_APDU_PROPERTIES *pAPDUParams, /* in, optional  */
    BACSTAC_HTSM *phTransaction,                /* out, optional */
    BACSTAC_ERROR *pError                       /* out, optional */
) BACSTAC_NOTHROW;

/*
 * BACstacConfPrivateTransfer
 */

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacConfPrivateTransfer2 (
    BACSTAC_HDEVICE hDevice,                           /* in            */
    const BACSTAC_PRIVATE_TRANSFER_INFO *pServiceInfo, /* in            */
    BACSTAC_PRIVATE_TRANSFER_INFO *pResult,            /* out           */
    BACSTAC_ERROR *pError,                             /* out, optional */
    BACSTAC_IALLOC *iAlloc                             /* in, optional  */
) BACSTAC_NOTHROW;

BACSTAC_DEPRECATED /* for backward compatibility only */
BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacConfPrivateTransfer (
    BACSTAC_HDEVICE hDevice,                           /* in            */
    const BACSTAC_PRIVATE_TRANSFER_INFO *pServiceInfo, /* in            */
    BACSTAC_PRIVATE_TRANSFER_INFO *pResult,            /* out           */
    BACSTAC_ERROR *pError                              /* out, optional */
) BACSTAC_NOTHROW;

/*
 * BACstacConfPrivateTransferEx
 */

typedef void (BACstac_callback *BACSTAC_PRIVATE_XFER_ACR_PROC) (
    BACSTAC_HTSM hTransaction,
    const BACSTAC_ADDRESS *pSourceAddress,
    const BACSTAC_ADDRESS *pDestinationAddress,
    BACSTAC_STATUS status,
    BACSTAC_PRIVATE_TRANSFER_INFO *pResult,
    BACSTAC_ERROR *pError
);

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacConfPrivateTransferEx2 (
    const BACSTAC_ADDRESS *pSourceAddress,             /* in            */
    const BACSTAC_ADDRESS *pDestinationAddress,        /* in            */
    const BACSTAC_PRIVATE_TRANSFER_INFO *pServiceInfo, /* in            */
    BACSTAC_PRIVATE_XFER_ACR_PROC pfACR,               /* in, optional  */
    const BACSTAC_APDU_PROPERTIES *pAPDUParams,        /* in, optional  */
    BACSTAC_HTSM *phTransaction,                       /* out, optional */
    BACSTAC_PRIVATE_TRANSFER_INFO *pResult,            /* out           */
    BACSTAC_ERROR *pError,                             /* out, optional */
    BACSTAC_IALLOC *iAlloc                             /* in, optional  */
) BACSTAC_NOTHROW;

BACSTAC_DEPRECATED /* for backward compatibility only */
BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacConfPrivateTransferEx (
    const BACSTAC_ADDRESS *pSourceAddress,             /* in            */
    const BACSTAC_ADDRESS *pDestinationAddress,        /* in            */
    const BACSTAC_PRIVATE_TRANSFER_INFO *pServiceInfo, /* in            */
    BACSTAC_PRIVATE_XFER_ACR_PROC pfACR,               /* in, optional  */
    const BACSTAC_APDU_PROPERTIES *pAPDUParams,        /* in, optional  */
    BACSTAC_HTSM *phTransaction,                       /* out, optional */
    BACSTAC_PRIVATE_TRANSFER_INFO *pResult,            /* out           */
    BACSTAC_ERROR *pError                              /* out, optional */
) BACSTAC_NOTHROW;

/*
 * BACstacUnconfPrivateTransfer
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacUnconfPrivateTransfer (
    BACSTAC_HDEVICE hDevice,                          /* in            */
    const BACSTAC_PRIVATE_TRANSFER_INFO *pServiceInfo /* in            */
) BACSTAC_NOTHROW;

/*
 * BACstacUnconfPrivateTransferEx
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacUnconfPrivateTransferEx (
    const BACSTAC_ADDRESS *pSourceAddress,             /* in            */
    const BACSTAC_ADDRESS *pDestinationAddress,        /* in            */
    const BACSTAC_PRIVATE_TRANSFER_INFO *pServiceInfo, /* in            */
    const BACSTAC_APDU_PROPERTIES *pAPDUParams         /* in, optional  */
) BACSTAC_NOTHROW;

/*
 * BACstacReinitializeDevice
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacReinitializeDevice (
    BACSTAC_HDEVICE hDevice,
    BACSTAC_REINIT_TYPE state,
    const BACSTAC_CHAR *pszPassword,    /* or NULL */
    BACSTAC_ERROR *pError
) BACSTAC_NOTHROW;

/*
 * BACstacReinitializeDeviceEx
 */


/* use BACSTAC_SIMPLE_ACK_ACR_PROC for async notification */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacReinitializeDeviceEx (
    const BACSTAC_ADDRESS *pSourceAddress,       /* in            */
    const BACSTAC_ADDRESS *pDestinationAddress,  /* in            */
    const BACSTAC_REINIT_DEV_INFO *pServiceInfo, /* in            */
    BACSTAC_SIMPLE_ACK_ACR_PROC pfACR,           /* in, optional  */
    const BACSTAC_APDU_PROPERTIES *pAPDUParams,  /* in, optional  */
    BACSTAC_HTSM *phTransaction,                 /* out, optional */
    BACSTAC_ERROR *pError                        /* out, optional */
) BACSTAC_NOTHROW;





/*
 * BACstacTimeSynchronization
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacTimeSynchronization (
    const BACSTAC_ADDRESS *pAddress,
    const BACSTAC_DATE_TIME *dateTime
) BACSTAC_NOTHROW;


/*
 * BACstacUTCTimeSynchronization
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacUTCTimeSynchronization (
    const BACSTAC_ADDRESS *pAddress,
    const BACSTAC_DATE_TIME *dateTime
) BACSTAC_NOTHROW;


/*
 * BACstacWhoHas
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacWhoHas (
    const BACSTAC_ADDRESS *networkAddress,
    BACSTAC_INST_NUMBER filterMin,
    BACSTAC_INST_NUMBER filterMax,
    const BACSTAC_WHO_HAS_PARAM *objectSpec
) BACSTAC_NOTHROW;




/*
 * BACstacWhoIs
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacWhoIs (
    const BACSTAC_ADDRESS *networkAddress,
    BACSTAC_INST_NUMBER filterMin,
    BACSTAC_INST_NUMBER filterMax
) BACSTAC_NOTHROW;




/*
 * Subscribe COV Property Multiple
 */

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacSubscribeCOVPMulti (
    BACSTAC_HDEVICE hDevice,
    const BACSTAC_SUBS_COVP_MULTI_INFO *pSubscriber,
    BACSTAC_SUBS_COVP_MULTI_ERROR *pError,             /* Optional */
    BACSTAC_BYTE *pAbortOrRejectReason                 /* Optional */
) BACSTAC_NOTHROW;

/*
 * Subscribe COV Property Multiple Ex
 */

typedef void (BACstac_callback *BACSTAC_SUBS_COVP_MULTI_ACR_PROC) (
    BACSTAC_HTSM hTransaction,
    const BACSTAC_ADDRESS *pSourceAddress,
    const BACSTAC_ADDRESS *pDestinationAddress,
    BACSTAC_STATUS status,
    BACSTAC_SUBS_COVP_MULTI_ERROR *pError,
    BACSTAC_BYTE *pAbortOrRejectReason
);

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacSubscribeCOVPMultiEx (
    const BACSTAC_ADDRESS *pSourceAddress,           /* in, optional  */
    const BACSTAC_ADDRESS *pDestinationAddress,      /* in            */
    const BACSTAC_SUBS_COVP_MULTI_INFO *pSubscriber, /* in            */
    BACSTAC_SUBS_COVP_MULTI_ACR_PROC pfACR,          /* in, optional  */
    const BACSTAC_APDU_PROPERTIES *pAPDUParams,      /* in, optional  */
    BACSTAC_HTSM *phTransaction,                     /* out, optional */
    BACSTAC_SUBS_COVP_MULTI_ERROR *pError,           /* out, optional */
    BACSTAC_BYTE *pAbortOrRejectReason               /* out, optional */
) BACSTAC_NOTHROW;



/*
 * Write Group EX
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacWriteGroupEx (
    const BACSTAC_ADDRESS *pSourceAddress,       /* in            */
    const BACSTAC_ADDRESS *pDestinationAddress,  /* in            */
    const BACSTAC_WRITE_GROUP_INFO *pWGInfo,     /* in            */
    const BACSTAC_APDU_PROPERTIES *pAPDUParams   /* in, optional  */
) BACSTAC_NOTHROW;

/*
 * Confirmed Audit Notification Ex
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacConfAuditNotificationEx (
   const BACSTAC_ADDRESS *pSourceAddress,        /* in, optional  */
   const BACSTAC_ADDRESS *pDestinationAddress,   /* in            */
   const BACSTAC_AUDIT_NOTIF_INFO *pServiceInfo, /* in            */
   BACSTAC_SIMPLE_ACK_ACR_PROC pfACR,            /* in, optional  */
   const BACSTAC_APDU_PROPERTIES *pAPDUParams,   /* in, optional  */
   BACSTAC_HTSM *phTransaction,                  /* out, optional */
   BACSTAC_ERROR *pError                         /* out, optional */
) BACSTAC_NOTHROW;

/*
 * Unconfirmed Audit Notification Ex
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacUnconfAuditNotificationEx (
   const BACSTAC_ADDRESS *pSourceAddress,        /* in, optional  */
   const BACSTAC_ADDRESS *pDestinationAddress,   /* in            */
   const BACSTAC_AUDIT_NOTIF_INFO *pServiceInfo, /* in            */
   const BACSTAC_APDU_PROPERTIES *pAPDUParams    /* in, optional  */
) BACSTAC_NOTHROW;

/*
 * BACstacAuditLogQueryEx
 */

typedef void (BACstac_callback *BACSTAC_AUDIT_LOG_QUERY_ACR_PROC) (
    BACSTAC_HTSM hTransaction,
    const BACSTAC_ADDRESS *pSourceAddress,
    const BACSTAC_ADDRESS *pDestinationAddress,
    BACSTAC_STATUS status,
    BACSTAC_AUDIT_LOG_QUERY_RESULT *pResult,
    BACSTAC_ERROR *pError
);

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacAuditLogQueryEx (
   const BACSTAC_ADDRESS *pSourceAddress,            /* in, optional  */
   const BACSTAC_ADDRESS *pDestinationAddress,       /* in            */
   const BACSTAC_AUDIT_LOG_QUERY_INFO *pServiceInfo, /* in            */
   BACSTAC_AUDIT_LOG_QUERY_ACR_PROC pfACR,           /* in, optional  */
   const BACSTAC_APDU_PROPERTIES *pAPDUParams,       /* in, optional  */
   BACSTAC_HTSM *phTransaction,                      /* out, optional */
   BACSTAC_AUDIT_LOG_QUERY_RESULT *pResult,          /* out           */
   BACSTAC_ERROR *pError,                            /* out, optional */
   BACSTAC_IALLOC *iAlloc                            /* in, optional  */
) BACSTAC_NOTHROW;



BACSTACAPI BACSTAC_STATUS BACstac_api BACstacYouAreEx (
   const BACSTAC_ADDRESS *pSourceAddress,      /* in, optional  */
   const BACSTAC_ADDRESS *pDestinationAddress, /* in            */
   const BACSTAC_YOU_ARE_INFO *pServiceInfo,   /* in            */
   const BACSTAC_APDU_PROPERTIES *pAPDUParams  /* in, optional  */
) BACSTAC_NOTHROW;



#ifdef __cplusplus
}
#endif /* __cplusplus */


#ifndef NO_BACACC_H
#include <bacacc.h>
#endif


#endif /* BACCLI_H */
