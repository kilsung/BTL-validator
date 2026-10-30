/*
 * BACstac(TM) BACGTW.H
 * Copyright (c) 1996-2026 by Cimetrics Inc. All rights reserved.
 *
 * Purpose: define BACstac Gateway API routines, types and constants
 *
 */


#ifndef BACGTW_H
#define BACGTW_H

#include <bnetdef.h>
#include <bneterr.h>
#include <bacbase.h>
#include <bactypes.h>
#include <baccomm.h>
#include <bacdb.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/*
 * BACstacInit
 */

typedef struct BACSTAC_DEVICE_PARAMS
{
    const void *deviceTemplate;
    BACSTAC_ADDRESS address;
    BACSTAC_INST_NUMBER instNumber;
    BACSTAC_OBJECT_COUNT maxObjects;
    BACSTAC_ELEMENT_COUNT maxCovSubs;
} BACSTAC_DEVICE_PARAMS;


#define BACSTAC_IF_USE_CHAR_STRING 0x01

/*
 * If the Read-Property hook is installed then the BACstac default action
 * should convert any Read-Property-Multiple request to Read-Property requests
 * to itself. Note that all generated Read-Property requests have the source
 * address same as the destination of the RPM request.
 */
#define BACSTAC_IF_RPM_TO_RP_HOOK  0x02

/*
 * If the Write-Property hook is installed then the BACstac default action
 * should converts any Write-Property-Multiple request to Write-Property
 * requests to itself. Note that all generated Write-Property requests have
 * the source address same as the destination of the WPM request.
 *
 * WARNING: This flag is incompatible with the Value-Source mechanism, so
 * if the BACstac database contains the Value_Source property this flag
 * should not be set.
 */
#define BACSTAC_IF_WPM_TO_WP_HOOK  0x04

/*
 * This flag indicates whether the pServiceInfo parameter of the BACstacReadPropertyEx2
 * function is input-output or input-only.i If this flag is set then pServiceInfo is
 * input-output, and it receives Object Identifier from ReadProperty-ACK if
 * the instance number in the request was unknown (i.e. 4194304). If this flag is not set
 * then this pServiceInfo is input only and Object Identifier value received in
 * ReadProperty-ACK is not visible to the application.
 */
#define BACSTAC_IF_RP_IN_OUT_INFO  0x08

typedef struct tagBACSTAC_GTW_INIT
{
    BACSTAC_UI8     BACstacVersionMajor;    /* BACSTAC_VERSION_MAJOR for this version */
    BACSTAC_UI8     BACstacVersionMinor;    /* BACSTAC_VERSION_MINOR for this version */
    BACSTAC_UI16    wPortID;
    BACSTAC_DEVICE_COUNT    maxDevices;
    const BACSTAC_DB_TYPE_INFO *devTypeInfo;
    const BACSTAC_BIT_STRING *servicesSupported;
    const BACSTAC_DEVICE_PARAMS *gatewayDevice;
    BACSTAC_UINT    flags;
} BACSTAC_GTW_INIT;

BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacInitGtwInit (
    BACSTAC_GTW_INIT *pInit,
    BACSTAC_UI8 BACstacVersionMajor,
    BACSTAC_UI8 BACstacVersionMinor,
    BACSTAC_UI16 wPortID
) BACSTAC_NOTHROW;

BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacSetGtwInitPortID (
    BACSTAC_GTW_INIT *pInit,
    BACSTAC_UI16 wPortID
) BACSTAC_NOTHROW;

BACSTACAPI BACSTAC_UI16 BACstac_api BACstacGetGtwInitPortID (
    const BACSTAC_GTW_INIT *pInit
) BACSTAC_NOTHROW;

BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacSetGtwInitMaxDevices (
    BACSTAC_GTW_INIT *pInit,
    BACSTAC_DEVICE_COUNT nMaxDevices
) BACSTAC_NOTHROW;

BACSTACAPI BACSTAC_DEVICE_COUNT BACstac_api BACstacGetGtwInitMaxDevices (
    const BACSTAC_GTW_INIT *pInit
) BACSTAC_NOTHROW;

BACSTACAPI void BACstac_api BACstacSetGtwInitPropTypeInfo (
    BACSTAC_GTW_INIT *pInit,
    const BACSTAC_DB_TYPE_INFO *devTypeInfo
) BACSTAC_NOTHROW;

BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacSetGtwInitServicesSupported (
    BACSTAC_GTW_INIT *pInit,
    const BACSTAC_BIT_STRING *servicesSupported);

BACSTACAPI const BACSTAC_BIT_STRING * BACstac_api BACstacGetGtwInitServicesSupported (
    const BACSTAC_GTW_INIT *pInit);

BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacSetGtwInitFlags (
    BACSTAC_GTW_INIT *pInit,
    BACSTAC_UINT flags
) BACSTAC_NOTHROW;

BACSTACAPI BACSTAC_UINT BACstac_api BACstacGetGtwInitFlags (
   const BACSTAC_GTW_INIT *pInit
) BACSTAC_NOTHROW;

BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacSetGatewayDevice(
   BACSTAC_GTW_INIT *pInit,
   const BACSTAC_DEVICE_PARAMS *pDeviceParams
) BACSTAC_NOTHROW;


BACSTACAPI BACSTAC_STATUS BACstac_api BACstacGatewayInit (
    const BACSTAC_GTW_INIT *pInitData
) BACSTAC_NOTHROW;


/*
 * BACstacCreateDevice
 */

BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacInitDeviceParams(
    BACSTAC_DEVICE_PARAMS *pDeviceParams,
    const void *pDeviceTemplate,
    const BACSTAC_ADDRESS *pAddress,
    BACSTAC_INST_NUMBER instNumber);

BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacSetDeviceMaxObjects(
    BACSTAC_DEVICE_PARAMS *pDeviceParams,
    BACSTAC_OBJECT_COUNT maxObjects);

BACSTACAPI BACSTAC_OBJECT_COUNT BACstac_api BACstacGetDeviceMaxObjects(
    const BACSTAC_DEVICE_PARAMS *pDeviceParams);

BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacSetDeviceMaxCovSubs(
    BACSTAC_DEVICE_PARAMS *pDeviceParams,
    BACSTAC_ELEMENT_COUNT maxObjects);

BACSTACAPI BACSTAC_ELEMENT_COUNT BACstac_api BACstacGetDeviceMaxCovSubs(
    const BACSTAC_DEVICE_PARAMS *pDeviceParams);

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacCreateDevice (
    const BACSTAC_DEVICE_PARAMS *pDeviceParams,
    BACSTAC_HDEVICE *pDeviceHandle
) BACSTAC_NOTHROW;


/*
 * BACstacCreateDeviceFromTemplate is same as BACstacCreateDevice
 * with maxCovSubs = 20.
 */

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacCreateDeviceFromTemplate (
    const void *pDeviceDescription,
    const BACSTAC_ADDRESS *pAddress,
    BACSTAC_INST_NUMBER instNumber,
    BACSTAC_OBJECT_COUNT maxObjects,
    BACSTAC_HDEVICE *pDeviceHandle
) BACSTAC_NOTHROW;


/*
 * BACstacDeleteDevice
 */


BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacDeleteDevice (BACSTAC_HDEVICE hDevice);




/*
 * Device Comminication Control
 */

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacGetDCCValueByAddress (
    const BACSTAC_ADDRESS *pAddress,
    BACSTAC_DCC_VALUE *pValue
) BACSTAC_NOTHROW;

BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacSetDCCValueByAddress (
    const BACSTAC_ADDRESS *pAddress,
    BACSTAC_DCC_VALUE value
) BACSTAC_NOTHROW;

/*
 * BACstacGetPortIDList
 */

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacGetPortIDList (
    BACSTAC_UI8 *portIDList,      /* or NULL */
    BACSTAC_UINT portIDBufferCount,
    BACSTAC_UINT *portIDAvailable
) BACSTAC_NOTHROW;

/*
 * BACstacGetFirstNpoHandle
 */

BACSTACAPI BACSTAC_HOBJECT BACstac_api BACstacGetFirstNpoHandle (
    BACSTAC_HDEVICE hDevice
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
 * Acknowledge Alarm hook
 */

typedef BACSTAC_HOOK_STATUS (BACstac_hook *BACSTAC_ACK_ALARM_HOOK_PROC) (
    BACSTAC_HTSM hTransaction,
    BACSTAC_ADDRESS *sourceAddress,
    BACSTAC_ADDRESS *destinationAddress,
    const BACSTAC_ACK_ALARM_INFO *pServiceInfo
);

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

BACSTACAPI void BACstac_api BACstacEnableAllProcessID (
   BACSTAC_BOOLEAN enable
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


/*
 * Get Alarm Summary hook
 */

typedef BACSTAC_HOOK_STATUS (BACstac_hook *BACSTAC_ALARM_SUMM_HOOK_PROC) (
    BACSTAC_HTSM hTransaction,
    BACSTAC_ADDRESS *sourceAddress,
    BACSTAC_ADDRESS *destinationAddress,
    const void *reserved /* NULL */
);

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacAlarmSummResponse (
    BACSTAC_HTSM hTransaction,
    const BACSTAC_ALARM_SUMMARY *pSummaries,    /* or NULL */
    BACSTAC_ELEMENT_COUNT nSummaryCount  /* or 0 */
) BACSTAC_NOTHROW;

/*
 * Get Enrollment Summary hook
 */

typedef BACSTAC_HOOK_STATUS (BACstac_hook *BACSTAC_ENRL_SUMM_HOOK_PROC) (
    BACSTAC_HTSM hTransaction,
    BACSTAC_ADDRESS *sourceAddress,
    BACSTAC_ADDRESS *destinationAddress,
    const BACSTAC_ENROLLMENT_FILTER *pServiceInfo
);

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacEnrollmentSummResponse (
    BACSTAC_HTSM hTransaction,
    const BACSTAC_ENROLLMENT_SUMMARY *pSummaries,   /* or NULL */
    BACSTAC_ELEMENT_COUNT nSummaryCount  /* or 0 */
) BACSTAC_NOTHROW;

/*
 * Get Event Information hook
 */

typedef BACSTAC_HOOK_STATUS (BACstac_hook *BACSTAC_GET_EVENT_INFO_HOOK_PROC) (
    BACSTAC_HTSM hTransaction,
    BACSTAC_ADDRESS *sourceAddress,
    BACSTAC_ADDRESS *destinationAddress,
    const BACSTAC_EVENT_INFO_INFO *pServiceInfo
);

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacEventInfoResponse (
    BACSTAC_HTSM hTransaction,
    const BACSTAC_EVENT_INFO *pEventInfo
) BACSTAC_NOTHROW;

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacEventInfoAutoResponse (
   BACSTAC_HTSM hTransaction,
   const BACSTAC_EVENT_INFO *pEventInfo,
   BACSTAC_ELEMENT_COUNT *pCount /* or NULL */
) BACSTAC_NOTHROW;

/*
 * Life Safety Operation hook
 */

typedef BACSTAC_HOOK_STATUS (BACstac_hook *BACSTAC_LIFE_SAFETY_OP_HOOK_PROC) (
    BACSTAC_HTSM hTransaction,
    BACSTAC_ADDRESS *sourceAddress,
    BACSTAC_ADDRESS *destinationAddress,
    const BACSTAC_LIFE_SAFETY_INFO *pServiceInfo
);

/*
 * Subscribe COV hook
 */

typedef BACSTAC_HOOK_STATUS (BACstac_hook *BACSTAC_SUBSCRIBE_COV_HOOK_PROC) (
    BACSTAC_HTSM hTransaction,
    BACSTAC_ADDRESS *sourceAddress,
    BACSTAC_ADDRESS *destinationAddress,
    const BACSTAC_SUBSCRIBE_COV_INFO *pServiceInfo
);

/*
 * Subscribe COV Property hook
 */

typedef BACSTAC_HOOK_STATUS (BACstac_hook *BACSTAC_SUBSCRIBE_COVP_HOOK_PROC) (
    BACSTAC_HTSM hTransaction,
    BACSTAC_ADDRESS *sourceAddress,
    BACSTAC_ADDRESS *destinationAddress,
    const BACSTAC_SUBSCRIBE_COVP_INFO *pServiceInfo
);

/*                         FILE ACCESS SERVICES                            */

/*
 * BACSTAC_READ_FILE_HOOK_PROC
 */

typedef BACSTAC_HOOK_STATUS (BACstac_hook *BACSTAC_READ_FILE_HOOK_PROC) (
    BACSTAC_HTSM hTransaction,
    BACSTAC_ADDRESS *sourceAddress,
    BACSTAC_ADDRESS *destinationAddress,
    const BACSTAC_READ_FILE_INFO *pServiceInfo
);

/*
 * BACstacReadFileResponse
 */

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacReadFileResponse (
    BACSTAC_HTSM hTransaction,
    const BACSTAC_READ_FILE_RESULT *pResult
) BACSTAC_NOTHROW;

/*
 * BACSTAC_WRITE_FILE_HOOK_PROC
 */

typedef BACSTAC_HOOK_STATUS (BACstac_hook *BACSTAC_WRITE_FILE_HOOK_PROC) (
    BACSTAC_HTSM hTransaction,
    BACSTAC_ADDRESS *sourceAddress,
    BACSTAC_ADDRESS *destinationAddress,
    const BACSTAC_WRITE_FILE_INFO *pServiceInfo
);
/*
 * BACstacWriteFileResponse
 */

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacWriteFileResponse (
    BACSTAC_HTSM hTransaction,
    const BACSTAC_WRITE_FILE_RESULT *pResult
) BACSTAC_NOTHROW;

/*                        OBJECT ACCESS SERVICES                           */

/*
 * BACSTAC_ADD_ELEMENT_HOOK_PROC
 */

typedef BACSTAC_HOOK_STATUS (BACstac_hook *BACSTAC_ADD_ELEMENT_HOOK_PROC) (
    BACSTAC_HTSM hTransaction,
    BACSTAC_ADDRESS *sourceAddress,
    BACSTAC_ADDRESS *destinationAddress,
    const BACSTAC_CHANGE_LIST_INFO *pServiceInfo
);

/*
 * BACSTAC_REMOVE_ELEMENT_HOOK_PROC
 */

typedef BACSTAC_HOOK_STATUS (BACstac_hook *BACSTAC_REMOVE_ELEMENT_HOOK_PROC) (
    BACSTAC_HTSM hTransaction,
    BACSTAC_ADDRESS *sourceAddress,
    BACSTAC_ADDRESS *destinationAddress,
    const BACSTAC_CHANGE_LIST_INFO *pServiceInfo
);

/*
 * BACstacChangeListError
 */

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacChangeListError (
    BACSTAC_HTSM hTransaction,
    BACSTAC_ERROR_CLASS errClass,
    BACSTAC_ERROR_CODE errCode,
    BACSTAC_ELEMENT_COUNT firstFailed
) BACSTAC_NOTHROW;

/*
 * CreateObject hook
 */

typedef BACSTAC_HOOK_STATUS (BACstac_hook *BACSTAC_CREATE_OBJECT_HOOK_PROC) (
    BACSTAC_HTSM hTransaction,
    BACSTAC_ADDRESS *sourceAddress,
    BACSTAC_ADDRESS *destinationAddress,
    const BACSTAC_CREATE_OBJECT_INFO *pServiceInfo
);

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacCreateObjectResponse (
    BACSTAC_HTSM hTSM,
    const BACSTAC_OBJECT_ID *pObjectID
) BACSTAC_NOTHROW;

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacCreateObjectError (
    BACSTAC_HTSM hTSM,
    BACSTAC_ERROR_CLASS errClass,
    BACSTAC_ERROR_CODE errCode,
    BACSTAC_PROPERTY_COUNT firstFailed
) BACSTAC_NOTHROW;

/*
 * DeleteObject hook
 */

typedef BACSTAC_HOOK_STATUS (BACstac_hook *BACSTAC_DELETE_OBJECT_HOOK_PROC) (
    BACSTAC_HTSM hTransaction,
    BACSTAC_ADDRESS *sourceAddress,
    BACSTAC_ADDRESS *destinationAddress,
    const BACSTAC_OBJECT_ID *pServiceInfo
);

/*
 * BACSTAC_READ_PROP_HOOK_PROC
 */

typedef BACSTAC_HOOK_STATUS (BACstac_hook *BACSTAC_READ_PROP_HOOK_PROC) (
    BACSTAC_HTSM hTransaction,
    BACSTAC_ADDRESS *sourceAddress,
    BACSTAC_ADDRESS *destinationAddress,
    const BACSTAC_READ_INFO *pServiceInfo
);

/*
 * BACstacReadPropResponse
 */

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacReadPropResponse (
    BACSTAC_HTSM hTransaction,
    const BACSTAC_READ_INFO *pServiceInfo,
    const BACSTAC_PROPERTY_CONTENTS *pValue
) BACSTAC_NOTHROW;

/*
 * ReadPropertyConditional hook & response
 */

typedef BACSTAC_HOOK_STATUS (BACstac_hook *BACSTAC_READ_PROP_CON_HOOK_PROC) (
    BACSTAC_HTSM hTransaction,
    BACSTAC_ADDRESS *sourceAddress,
    BACSTAC_ADDRESS *destinationAddress,
    const BACSTAC_READ_CON_INFO *pServiceInfo
);

/*
 * BACstacReadPropConResponse
 */

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacReadPropConResponse (
    BACSTAC_HTSM hTransaction,
    const BACSTAC_READ_ACCESS_RESULT *readAccessResult,
    BACSTAC_OBJECT_COUNT nResultListCount
) BACSTAC_NOTHROW;

/*
 * BACSTAC_READ_PROP_MUL_HOOK_PROC
 */

typedef BACSTAC_HOOK_STATUS (BACstac_hook *BACSTAC_READ_PROP_MUL_HOOK_PROC) (
    BACSTAC_HTSM hTransaction,
    BACSTAC_ADDRESS *sourceAddress,
    BACSTAC_ADDRESS *destinationAddress,
    const BACSTAC_READ_MUL_HOOK_INFO *pServiceInfo
);

/*
 * BACstacReadPropMultipleResponse
 */

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacReadPropMultipleResponse (
    BACSTAC_HTSM hTransaction,
    const BACSTAC_READ_ACCESS_RESULT *readAccessResult,
    BACSTAC_OBJECT_COUNT nResultListCount
) BACSTAC_NOTHROW;

/*
 * ReadRange hook
 */

#ifdef BACSTAC_USE_64_BIT_READ_RANGE
   #define BACSTAC_READ_RANGE_HOOK_PROC BACSTAC_READ_RANGE_64_HOOK_PROC
   #define BACstacReadRangeResponse     BACstacReadRange64Response
   #define BACstacReadRangeAutoResponse BACstacReadRange64AutoResponse
#else  /* BACSTAC_USE_64_BIT_READ_RANGE */
   #define BACSTAC_READ_RANGE_HOOK_PROC BACSTAC_READ_RANGE_32_HOOK_PROC
   #define BACstacReadRangeResponse     BACstacReadRange32Response
   #define BACstacReadRangeAutoResponse BACstacReadRange32AutoResponse
#endif /* BACSTAC_USE_64_BIT_READ_RANGE */

typedef BACSTAC_HOOK_STATUS (BACstac_hook *BACSTAC_READ_RANGE_64_HOOK_PROC) (
    BACSTAC_HTSM hTransaction,
    BACSTAC_ADDRESS *sourceAddress,
    BACSTAC_ADDRESS *destinationAddress,
    const BACSTAC_READ_RANGE_64_INFO *pServiceInfo
);

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacReadRange64Response (
    BACSTAC_HTSM hTransaction,
    BACSTAC_READ_RANGE_64_RAW_RESULT *pResult
) BACSTAC_NOTHROW;

typedef BACSTAC_HOOK_STATUS (BACstac_hook *BACSTAC_READ_RANGE_32_HOOK_PROC) (
    BACSTAC_HTSM hTransaction,
    BACSTAC_ADDRESS *sourceAddress,
    BACSTAC_ADDRESS *destinationAddress,
    const BACSTAC_READ_RANGE_32_INFO *pServiceInfo
);

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacReadRange32Response (
    BACSTAC_HTSM hTransaction,
    BACSTAC_READ_RANGE_32_RAW_RESULT *pResult
) BACSTAC_NOTHROW;

/*
 * This function is similar to BACstacReadRangeResponse but it automatically
 * truncates the number of records in the response based on APDU parameters.
 *
 * If all items from 'pResult' cannot be sent then the 'more-items' flag is
 * set and the 'first-item' or 'last-item' flag is cleaned in resultFlags.
 *
 * The 'last' parameter indicates whether first or last records should be
 * sent if all records do not fit into the response. It is determined by
 * the count value in the request.
 */
BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacReadRange64AutoResponse (
    BACSTAC_HTSM hTransaction,
    BACSTAC_READ_RANGE_64_RAW_RESULT *pResult,
    BACSTAC_BOOLEAN last
) BACSTAC_NOTHROW;

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacReadRange32AutoResponse (
    BACSTAC_HTSM hTransaction,
    BACSTAC_READ_RANGE_32_RAW_RESULT *pResult,
    BACSTAC_BOOLEAN last
) BACSTAC_NOTHROW;

/*
 * BACSTAC_WRITE_PROP_HOOK_PROC
 */

typedef BACSTAC_HOOK_STATUS (BACstac_hook *BACSTAC_WRITE_PROP_HOOK_PROC) (
    BACSTAC_HTSM hTransaction,
    BACSTAC_ADDRESS *sourceAddress,
    BACSTAC_ADDRESS *destinationAddress,
    const BACSTAC_WRITE_INFO *pServiceInfo
);

/*
 * BACstacWritePropResponse
 */

#define BACstacWritePropResponse(hTsm)      BACstacSrvcResponse(hTsm)

/*
 * BACSTAC_WRITE_PROP_MUL_HOOK_PROC
 */

typedef BACSTAC_HOOK_STATUS (BACstac_hook *BACSTAC_WRITE_PROP_MUL_HOOK_PROC) (
    BACSTAC_HTSM hTransaction,
    BACSTAC_ADDRESS *sourceAddress,
    BACSTAC_ADDRESS *destinationAddress,
    const BACSTAC_WRITE_PROP_MUL_INFO *pServiceInfo
);

/*
 * BACstacWritePropMultipleError
 */

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacWritePropMultipleError (
    BACSTAC_HTSM hTransaction,
    BACSTAC_ERROR_CLASS errClass,
    BACSTAC_ERROR_CODE errCode,
    const BACSTAC_OBJ_PROP_REF *pFirstFailed
) BACSTAC_NOTHROW;

/*                 REMOTE DEVICE MANAGEMENT SERVICES                       */

/*
 * Device Comminication Control hook
 */

typedef BACSTAC_HOOK_STATUS (BACstac_hook *BACSTAC_DCC_HOOK_PROC) (
    BACSTAC_HTSM hTransaction,
    BACSTAC_ADDRESS *sourceAddress,
    BACSTAC_ADDRESS *destinationAddress,
    const BACSTAC_DCC_INFO *pServiceInfo
);

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacGetDCCValue (
    BACSTAC_HDEVICE hDevice,
    BACSTAC_DCC_VALUE *pValue
) BACSTAC_NOTHROW;

BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacSetDCCValue (
    BACSTAC_HDEVICE hDevice,
    BACSTAC_DCC_VALUE value
) BACSTAC_NOTHROW;


/*
 * ConfirmedPrivateTransfer hook
 */

typedef BACSTAC_HOOK_STATUS (BACstac_hook *BACSTAC_CONF_PRIVATE_XFER_HOOK_PROC) (
    BACSTAC_HTSM hTransaction,
    BACSTAC_ADDRESS *sourceAddress,
    BACSTAC_ADDRESS *destinationAddress,
    const BACSTAC_PRIVATE_TRANSFER_INFO *pServiceInfo
);

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacPrivateTransferResponse (
    BACSTAC_HTSM hTSM,
    const BACSTAC_PRIVATE_TRANSFER_INFO *p
) BACSTAC_NOTHROW;

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacPrivateTransferError (
    BACSTAC_HTSM hTSM,
    BACSTAC_ERROR_CLASS errClass,
    BACSTAC_ERROR_CODE errCode,
    const BACSTAC_PRIVATE_TRANSFER_INFO *p
) BACSTAC_NOTHROW;

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
 * BACSTAC_REINIT_DEV_HOOK_PROC
 */

typedef BACSTAC_HOOK_STATUS (BACstac_hook *BACSTAC_REINIT_DEV_HOOK_PROC) (
    BACSTAC_HTSM hTransaction,
    BACSTAC_ADDRESS *sourceAddress,
    BACSTAC_ADDRESS *destinationAddress,
    const BACSTAC_REINIT_DEV_INFO *pServiceInfo
);

/*
 * ConfirmedTextMessage hook
 */

typedef BACSTAC_HOOK_STATUS (BACstac_hook *BACSTAC_CONF_TEXT_MSG_HOOK_PROC) (
    BACSTAC_HTSM hTransaction,
    BACSTAC_ADDRESS *sourceAddress,
    BACSTAC_ADDRESS *destinationAddress,
    const BACSTAC_TEXT_MESSAGE_INFO *pServiceInfo
);

/*
 * UnconfirmedTextMessage hook
 */

typedef BACSTAC_HOOK_STATUS (BACstac_hook *BACSTAC_UNCONF_TEXT_MSG_HOOK_PROC) (
    BACSTAC_HTSM hTransaction,
    BACSTAC_ADDRESS *sourceAddress,
    BACSTAC_ADDRESS *destinationAddress,
    const BACSTAC_TEXT_MESSAGE_INFO *pServiceInfo
);

/*
 * BACSTAC_TIME_SYNC_HOOK_PROC
 */

typedef BACSTAC_HOOK_STATUS (BACstac_hook *BACSTAC_TIME_SYNC_HOOK_PROC) (
    BACSTAC_HTSM hTransaction,  /* INVALID_HANDLE for unconfirmed services */
    BACSTAC_ADDRESS *sourceAddress,
    BACSTAC_ADDRESS *destinationAddress,
    const BACSTAC_TIME_SYNC_INFO *pServiceInfo
);

/*
 * BACSTAC_UTC_TIME_SYNC_HOOK_PROC
 */

typedef BACSTAC_HOOK_STATUS (BACstac_hook *BACSTAC_UTC_TIME_SYNC_HOOK_PROC) (
    BACSTAC_HTSM hTransaction,  /* INVALID_HANDLE for unconfirmed services */
    BACSTAC_ADDRESS *sourceAddress,
    BACSTAC_ADDRESS *destinationAddress,
    const BACSTAC_UTC_TIME_SYNC_INFO *pServiceInfo
);

/*
 * BACSTAC_WHO_HAS_HOOK_PROC
 */

typedef BACSTAC_HOOK_STATUS (BACstac_hook *BACSTAC_WHO_HAS_HOOK_PROC) (
    BACSTAC_HTSM hTransaction,  /* INVALID_HANDLE for unconfirmed services */
    BACSTAC_ADDRESS *sourceAddress,
    BACSTAC_ADDRESS *destinationAddress,
    const BACSTAC_WHO_HAS_HOOK_INFO *pServiceInfo
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
 * BACSTAC_WHO_IS_HOOK_PROC
 */

typedef BACSTAC_HOOK_STATUS (BACstac_hook *BACSTAC_WHO_IS_HOOK_PROC) (
    BACSTAC_HTSM hTransaction,  /* INVALID_HANDLE for unconfirmed services */
    BACSTAC_ADDRESS *sourceAddress,
    BACSTAC_ADDRESS *destinationAddress,
    const BACSTAC_WHO_IS_HOOK_INFO *pServiceInfo
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
 * BACstacWritePropMultipleError
 */

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacSubscribeCOVPMultiError (
    BACSTAC_HTSM hTransaction,
    const BACSTAC_SUBS_COVP_MULTI_ERROR *error
) BACSTAC_NOTHROW;

/*
 * Subscribe COV Property Multiple hook
 */

typedef BACSTAC_HOOK_STATUS (BACstac_hook *BACSTAC_SUBS_COVP_MULTI_HOOK_PROC) (
    BACSTAC_HTSM hTransaction,  /* INVALID_HANDLE for unconfirmed services */
    BACSTAC_ADDRESS *sourceAddress,
    BACSTAC_ADDRESS *destinationAddress,
    const BACSTAC_SUBS_COVP_MULTI_INFO *pServiceInfo
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
 * Write Group hook
 */

typedef BACSTAC_HOOK_STATUS(BACstac_hook *BACSTAC_WRITE_GROUP_HOOK_PROC) (
    BACSTAC_HTSM hTransaction,  /* INVALID_HANDLE for unconfirmed services */
    BACSTAC_ADDRESS *sourceAddress,
    BACSTAC_ADDRESS *destinationAddress,
    const BACSTAC_WRITE_GROUP_INFO *pServiceInfo
);

/*
 * Confirmed audit notification hook
 */

typedef BACSTAC_HOOK_STATUS(BACstac_hook *BACSTAC_CONF_AUDIT_NOTIF_HOOK_PROC) (
    BACSTAC_HTSM hTransaction,
    BACSTAC_ADDRESS *sourceAddress,
    BACSTAC_ADDRESS *destinationAddress,
    const BACSTAC_AUDIT_NOTIF_INFO *pServiceInfo
);
/* Define BACSTAC_CONF_AUDIT_NOTIF_RAW_HOOK_PROC as alias to
 * BACSTAC_CONF_AUDIT_NOTIF_HOOK_PROC just to have the HOOK_PROC
 * name that matches its hook type: HOOK_CONF_AUDIT_NOTIF_RAW. */
typedef BACSTAC_CONF_AUDIT_NOTIF_HOOK_PROC BACSTAC_CONF_AUDIT_NOTIF_RAW_HOOK_PROC;

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


/*
 * Audit log query hook
 */

typedef BACSTAC_HOOK_STATUS(BACstac_hook *BACSTAC_AUDIT_LOG_QUERY_HOOK_PROC) (
    BACSTAC_HTSM hTransaction,
    BACSTAC_ADDRESS *sourceAddress,
    BACSTAC_ADDRESS *destinationAddress,
    const BACSTAC_AUDIT_LOG_QUERY_INFO *pServiceInfo
);

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacAuditLogQueryResponse (
    BACSTAC_HTSM                               hTSM,
    BACSTAC_AUDIT_LOG_QUERY_RESULT *pResult
) BACSTAC_NOTHROW;

/*
 * This function is similar to BACstacAuditLogQueryResponse but it automatically
 * truncates the number of records in the response based on APDU parameters.
 *
 * If all items from 'pResult' cannot be sent then the 'no-more-items' flag is
 * cleared.
 *
 */
BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacAuditLogQueryAutoResponse (
    BACSTAC_HTSM                               hTSM,
    BACSTAC_AUDIT_LOG_QUERY_RESULT *pResult
) BACSTAC_NOTHROW;

typedef BACSTAC_HOOK_STATUS(BACstac_hook *BACSTAC_WHO_AM_I_HOOK_PROC) (
    BACSTAC_HTSM hTransaction,
    BACSTAC_ADDRESS *sourceAddress,
    BACSTAC_ADDRESS *destinationAddress,
    const BACSTAC_WHO_AM_I_INFO *pServiceInfo
);

typedef BACSTAC_HOOK_STATUS(BACstac_hook *BACSTAC_YOU_ARE_HOOK_PROC) (
    BACSTAC_HTSM hTransaction,
    BACSTAC_ADDRESS *sourceAddress,
    BACSTAC_ADDRESS *destinationAddress,
    const BACSTAC_YOU_ARE_INFO *pServiceInfo
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

/*
 * Confirmed COV notification
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacConfCOVNotification (
    BACSTAC_HDEVICE hInitDevice,
    BACSTAC_HOBJECT hMonitoredObject,
    BACSTAC_UI32    processID,
    BACSTAC_UNSIGNED timeRemaining,
    const BACSTAC_PROPERTY_VALUE *listOfValues,
    BACSTAC_PROPERTY_COUNT nValueCount,
    const BACSTAC_ADDRESS *pDestAddress,
    BACSTAC_ERROR *pError
) BACSTAC_NOTHROW;

/*
 * Confirmed COV notification EX
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacConfCOVNotificationEx (
    const BACSTAC_ADDRESS *pSourceAddress,      /* in, optional  */
    const BACSTAC_ADDRESS *pDestinationAddress, /* in            */
    const BACSTAC_COV_NOTIF_INFO *pCOVInfo,     /* in            */
    BACSTAC_SIMPLE_ACK_ACR_PROC pfACR,          /* in, optional  */
    const BACSTAC_APDU_PROPERTIES *pAPDUParams, /* in, optional  */
    BACSTAC_HTSM *phTransaction,                /* out, optional */
    BACSTAC_ERROR *pError                       /* out, optional */
) BACSTAC_NOTHROW;

/*
 * Unconfirmed COV notification
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacUnconfCOVNotification (
    BACSTAC_HDEVICE hInitDevice,
    BACSTAC_HOBJECT hMonitoredObject,
    BACSTAC_UI32    processID,
    BACSTAC_UNSIGNED timeRemaining,
    const BACSTAC_PROPERTY_VALUE *listOfValues,
    BACSTAC_PROPERTY_COUNT nValueCount,
    const BACSTAC_ADDRESS *pDestAddress
) BACSTAC_NOTHROW;

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacGlobalGroupUCOVNotif (
    BACSTAC_HOBJECT hMonitoredObject,
    BACSTAC_UI32    processID,
    BACSTAC_UNSIGNED timeRemaining,
    const BACSTAC_BIT_STRING *membStatusFlags,
    BACSTAC_ELEMENT_COUNT valueCount,
    const BACSTAC_PROP_ACCESS_RESULT *values,
    const BACSTAC_ADDRESS *destAddress,
    BACSTAC_GLOBAL_GROUP_UCOV_RESULT *result
) BACSTAC_NOTHROW;

typedef void (BACstac_callback *BACSTAC_GLOBAL_GROUP_UCOV_NOTIF_ACR) (
    const BACSTAC_ADDRESS *pSourceAddress,
    const BACSTAC_ADDRESS *pDestinationAddress,
    BACSTAC_STATUS status,
    const BACSTAC_GLOBAL_GROUP_UCOV_RESULT *result
);

/*
 * Unconfirmed COV notification EX
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacUnconfCOVNotificationEx (
    const BACSTAC_ADDRESS *pSourceAddress,      /* in            */
    const BACSTAC_ADDRESS *pDestinationAddress, /* in            */
    const BACSTAC_COV_NOTIF_INFO *pCOVInfo,     /* in            */
    const BACSTAC_APDU_PROPERTIES *pAPDUParams  /* in, optional  */
) BACSTAC_NOTHROW;

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacGlobalGroupUCOVNotifEx (
    const BACSTAC_ADDRESS *sourceAddress,        /* in, optional  */
    const BACSTAC_ADDRESS *destinationAddress,   /* in            */
    const BACSTAC_GLOBAL_GROUP_UCOV_NOTIF *info, /* in            */
    BACSTAC_GLOBAL_GROUP_UCOV_NOTIF_ACR fACR,    /* in, optional  */
    const BACSTAC_APDU_PROPERTIES *pAPDUParams,  /* in, optional  */
    BACSTAC_GLOBAL_GROUP_UCOV_RESULT *result     /* out, optional */
) BACSTAC_NOTHROW;

/*
 * Confirmed event notification
 */

/* BACstacConfEventNotification2 sends a confirmed event notification.
 *
 * Parameters:
 *
 *   hInitObject the handle of the initiating object
 *
 *   pEventNotif is a pointer to event notification request, which is used
 *        as an input/output parameter. All fields except the initiating
 *        device and object identifiers are input parameters. The initiating
 *        device and object identifiers are determined by the given object
 *        handle, and stored in this structure by this routine.
 *
 *   pDestAddress is a pointer to the address specifying device to which
 *        this notification must be sent.
 */
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacConfEventNotification2 (
    BACSTAC_HOBJECT hInitObject,
    BACSTAC_EVENT_NOTIF *pEventNotif,
    const BACSTAC_ADDRESS *pDestAddress,
    BACSTAC_ERROR *pError
) BACSTAC_NOTHROW;


/*
 * Confirmed event notification EX
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacConfEventNotificationEx2 (
    const BACSTAC_ADDRESS *pSourceAddress,      /* in            */
    const BACSTAC_ADDRESS *pDestinationAddress, /* in            */
    const BACSTAC_EVENT_NOTIF *pServiceInfo,    /* in            */
    BACSTAC_SIMPLE_ACK_ACR_PROC pfACR,          /* in, optional  */
    const BACSTAC_APDU_PROPERTIES *pAPDUParams, /* in, optional  */
    BACSTAC_HTSM *phTransaction,                /* out, optional */
    BACSTAC_ERROR *pError                       /* out, optional */
) BACSTAC_NOTHROW;

/*
 * Unconfirmed event notification
 */

/* BACstacUnconfEventNotification2 sends an unconfirmed event notification.
 *
 * Parameters:
 *
 *   hInitObject the handle of the initiating object
 *
 *   pEventNotif is a pointer to event notification request, which is used
 *        as an input/output parameter. All fields except the initiating
 *        device and object identifiers are input parameters. The initiating
 *        device and object identifiers are determined by the given object
 *        handle, and stored in this structure by this routine.
 *
 *   pDestAddress is a pointer to the address specifying device to which
 *        this notification must be sent.
 */
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacUnconfEventNotification2 (
    BACSTAC_HOBJECT hInitObject,
    BACSTAC_EVENT_NOTIF *pEventNotif,
    const BACSTAC_ADDRESS *pDestAddress
) BACSTAC_NOTHROW;


/*
 * Unconfirmed event notification EX
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacUnconfEventNotificationEx2 (
    const BACSTAC_ADDRESS *pSourceAddress,      /* in           */
    const BACSTAC_ADDRESS *pDestinationAddress, /* in           */
    const BACSTAC_EVENT_NOTIF *pServiceInfo,    /* in           */
    const BACSTAC_APDU_PROPERTIES *pAPDUParams  /* in, optional */
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
 * BACstacUnconfPrivateTransferEx
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacUnconfPrivateTransferEx (
    const BACSTAC_ADDRESS *pSourceAddress,             /* in            */
    const BACSTAC_ADDRESS *pDestinationAddress,        /* in            */
    const BACSTAC_PRIVATE_TRANSFER_INFO *pServiceInfo, /* in            */
    const BACSTAC_APDU_PROPERTIES *pAPDUParams         /* in, optional  */
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
 * BACstacConfTextMessageEx
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacConfTextMessageEx (
    const BACSTAC_ADDRESS *pSourceAddress,         /* in            */
    const BACSTAC_ADDRESS *pDestinationAddress,    /* in            */
    const BACSTAC_TEXT_MESSAGE_INFO *pServiceInfo, /* in            */
    BACSTAC_SIMPLE_ACK_ACR_PROC pfACR,             /* in, optional  */
    const BACSTAC_APDU_PROPERTIES *pAPDUParams,    /* in, optional  */
    BACSTAC_HTSM *phTransaction,                   /* out, optional */
    BACSTAC_ERROR *pError                          /* out, optional */
) BACSTAC_NOTHROW;


/*
 * BACstacUnconfTextMessageEx
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacUnconfTextMessageEx (
    const BACSTAC_ADDRESS *pSourceAddress,         /* in            */
    const BACSTAC_ADDRESS *pDestinationAddress,    /* in            */
    const BACSTAC_TEXT_MESSAGE_INFO *pServiceInfo, /* in            */
    const BACSTAC_APDU_PROPERTIES *pAPDUParams     /* in, optional  */
) BACSTAC_NOTHROW;


/*
 * BACstacTimeSynchronization
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacTimeSynchronizationEx (
    const BACSTAC_ADDRESS *pSourceAddress,      /* in           */
    const BACSTAC_ADDRESS *pDestinationAddress, /* in           */
    const BACSTAC_TIME_SYNC_INFO *pTimeSInfo,   /* in           */
    const BACSTAC_APDU_PROPERTIES *pAPDUParams  /* in, optional */
) BACSTAC_NOTHROW;


/*
 * BACstacUTCTimeSynchronizationEx
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacUTCTimeSynchronizationEx (
    const BACSTAC_ADDRESS *pSourceAddress,      /* in           */
    const BACSTAC_ADDRESS *pDestinationAddress, /* in           */
    const BACSTAC_TIME_SYNC_INFO *pTimeSInfo,   /* in           */
    const BACSTAC_APDU_PROPERTIES *pAPDUParams  /* in, optional */
) BACSTAC_NOTHROW;


/*
 * BACstacWhoHas
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacWhoHasEx (
    const BACSTAC_ADDRESS *pSourceAddress,       /* in            */
    const BACSTAC_ADDRESS *pDestinationAddress,  /* in            */
    const BACSTAC_WHO_HAS_HOOK_INFO *pWhoHasInfo,/* in            */
    const BACSTAC_APDU_PROPERTIES *pAPDUParams   /* in, optional  */
) BACSTAC_NOTHROW;

/*
 * IHave
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacIHave (
    void            *unused,
    BACSTAC_HOBJECT hObject,
    const BACSTAC_ADDRESS *pAddress
) BACSTAC_NOTHROW;

/*
 * IHaveEx
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacIHaveEx (
    const BACSTAC_ADDRESS *pSourceAddress,      /* in           */
    const BACSTAC_ADDRESS *pDestinationAddress, /* in           */
    const BACSTAC_I_HAVE_INFO *pIHaveInfo,      /* in           */
    const BACSTAC_APDU_PROPERTIES *pAPDUParams  /* in, optional */
) BACSTAC_NOTHROW;


/*
 * BACstacWhoIs
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacWhoIsEx (
    const BACSTAC_ADDRESS *pSourceAddress,      /* in            */
    const BACSTAC_ADDRESS *pDestinationAddress, /* in            */
    const BACSTAC_WHO_IS_HOOK_INFO *pWhoIsInfo, /* in            */
    const BACSTAC_APDU_PROPERTIES *pAPDUParams  /* in, optional  */
) BACSTAC_NOTHROW;

/*
 * BACstacIAm
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacIAm (
    BACSTAC_HDEVICE hDevice,
    const BACSTAC_ADDRESS *pAddress
) BACSTAC_NOTHROW;

/*
 * BACstacIAmEx
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacIAmEx (
    const BACSTAC_ADDRESS *pSourceAddress,      /* in           */
    const BACSTAC_ADDRESS *pDestinationAddress, /* in           */
    const BACSTAC_I_AM_INFO *pIAmInfo,          /* in           */
    const BACSTAC_APDU_PROPERTIES *pAPDUParams  /* in, optional */
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
 * Confirmed COV notification multiple EX
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacConfCOVNotifMultiEx (
    const BACSTAC_ADDRESS *pSourceAddress,        /* in, optional  */
    const BACSTAC_ADDRESS *pDestinationAddress,   /* in            */
    const BACSTAC_COV_NOTIF_MULTI_INFO *pCOVInfo, /* in            */
    BACSTAC_SIMPLE_ACK_ACR_PROC pfACR,            /* in, optional  */
    const BACSTAC_APDU_PROPERTIES *pAPDUParams,   /* in, optional  */
    BACSTAC_HTSM *phTransaction,                  /* out, optional */
    BACSTAC_ERROR *pError                         /* out, optional */
) BACSTAC_NOTHROW;

/*
 * Unconfirmed COV notification multiple EX
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacUnconfCOVNotifMultiEx (
    const BACSTAC_ADDRESS *pSourceAddress,        /* in            */
    const BACSTAC_ADDRESS *pDestinationAddress,   /* in            */
    const BACSTAC_COV_NOTIF_MULTI_INFO *pCOVInfo, /* in            */
    const BACSTAC_APDU_PROPERTIES *pAPDUParams    /* in, optional  */
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

/*
 * Who Am I Ex
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacWhoAmIEx (
   const BACSTAC_ADDRESS *pSourceAddress,      /* in, optional  */
   const BACSTAC_ADDRESS *pDestinationAddress, /* in            */
   const BACSTAC_WHO_AM_I_INFO *pServiceInfo,  /* in            */
   const BACSTAC_APDU_PROPERTIES *pAPDUParams  /* in, optional  */
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


#endif /* BACGTW_H */
