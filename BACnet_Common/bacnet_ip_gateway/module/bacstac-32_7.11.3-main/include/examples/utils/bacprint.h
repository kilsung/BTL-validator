/*
 * BACPRINT.H - BACstac (TM) Data Type Print Routines
 *
 * Print routines for Property Contents data types and enumerations.
 *
 * Copyright (c) 1998-2025 by Cimetrics Inc.  All rights reserved.
 * Author: Keith Corbett
 *
 */

#ifndef BACPRINT_H
#define BACPRINT_H

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#include <bnetdef.h>
#include <bneterr.h>
#include <bacbase.h>
#include <bactypes.h>
#include <bacacc.h>
#include "bacenum.h"

/* Property Contents data types */

BACSTAC_NONNULL_ARGS(())
void BACstacPrintContents (
	const BACSTAC_PROPERTY_CONTENTS *pValue,
	BACSTAC_OBJECT_TYPE ObjType,
	BACSTAC_PROPERTY_ID PropID,
	BACSTAC_BOOLEAN bSymbolNames
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintContents (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_PROPERTY_CONTENTS *pValue,
	BACSTAC_OBJECT_TYPE ObjType,
	BACSTAC_PROPERTY_ID PropID,
	BACSTAC_BOOLEAN bSymbolNames
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintContentsWithIndex (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_PROPERTY_CONTENTS *pValue,
	BACSTAC_OBJECT_TYPE ObjType,
	BACSTAC_PROPERTY_ID PropID,
	BACSTAC_ARRAY_INDEX nIndex,
	BACSTAC_BOOLEAN bSymbolNames
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintContentsList (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_PROPERTY_CONTENTS *pValue,
	BACSTAC_OBJECT_TYPE objType,
	BACSTAC_PROPERTY_ID propID,
	BACSTAC_BOOLEAN bSymbolNames
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintActionList (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_ACTION_LIST *pActionList,
	BACSTAC_BOOLEAN bSymbolNames
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintActionCommand (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_ACTION_COMMAND *pActionCommand,
	BACSTAC_BOOLEAN bSymbolNames
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintAddress (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_ADDRESS *pAddress
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintAddressBinding (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_ADDRESS_BINDING *pAddressBinding
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintBitString (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_BIT_STRING *pBitString,
	ENUM_TYPE_ID enumTypeID,
	BACSTAC_BOOLEAN bSymbolNames
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintBoolean (
	char *pBuffer,
	size_t nBufferLength,
	BACSTAC_BOOLEAN Bool
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintCalendarEntry (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_CALENDAR_ENTRY *pCalEntry
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintDailySchedule (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_DAILY_SCHEDULE *pDailySched,
	BACSTAC_BOOLEAN bSymbolNames
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintDate (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_DATE *pDate
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintDateRange (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_DATE_RANGE *pDateRange
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintDateTime (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_DATE_TIME *pDateTime
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintDestination (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_DESTINATION *pDestination,
	BACSTAC_BOOLEAN bSymbolNames
);

BACSTAC_NONNULL_ARGS(())
void BACstacPrintEnum (
	BACSTAC_ENUM enumValue,
	ENUM_TYPE_ID enumTypeID,
	BACSTAC_BOOLEAN bSymbolNames
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintEnum (
	char *pBuffer,
	size_t nBufferLength,
	BACSTAC_ENUM Enum,
	ENUM_TYPE_ID enumTypeID,
	BACSTAC_BOOLEAN bSymbolNames
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintErrorType (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_ERROR_TYPE *pError
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintEventParameter2 (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_EVENT_PARAMETER2 *pEventParam,
	BACSTAC_BOOLEAN bSymbolNames
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintGroupMember (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_READ_ACCESS_SPEC *pGroupMember,
	BACSTAC_BOOLEAN bSymbolNames
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintGroupResLstMember (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_READ_ACCESS_RESULT *pGroupResLstMem,
	BACSTAC_BOOLEAN bSymbolNames
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintRawResult (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_READ_RESULT_ITEM *pResultItem,
	BACSTAC_OBJECT_TYPE ObjType,
	BACSTAC_BOOLEAN bSymbolNames
);

BACSTAC_NONNULL_ARGS((1))
size_t BACstacSPrintHex (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_BYTE * pBytes,
	BACSTAC_VALUE_SIZE Size
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintObjectID (
	char *pBuffer,
	size_t nBufferLength,
	BACSTAC_OBJECT_ID objID
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintObjPropReference (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_OBJ_PROP_REF *pObjPropRef,
	BACSTAC_BOOLEAN bSymbolNames
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintDevObjPropReference (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_DEV_OBJ_PROP_REF *pDevObjPropRef,
	BACSTAC_BOOLEAN bSymbolNames
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintDevObjReference (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_DEV_OBJ_REF *pDevObjRef
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintClientCOV (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_CLIENT_COV *pClientCOV
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintOctetString (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_OCTET_STRING *pOctetString
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintRawASN (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_RAW_ASN1_VALUE *pRawASNString
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintPriArrayItem (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_PRI_ARRAY_ITEM *pPriArrayItem,
	ENUM_TYPE_ID enumTypeID,
	BACSTAC_BOOLEAN bSymbolNames
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintPropertyReference (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_PROP_REF *pPropRef,
	BACSTAC_BOOLEAN bSymbolNames
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintPropertyStates (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_PROPERTY_STATES *pPropStates,
	BACSTAC_BOOLEAN bSymbolNames
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintPropertyValue (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_PROPERTY_VALUE *pPropValue,
	BACSTAC_OBJECT_TYPE ObjType,
	BACSTAC_BOOLEAN bSymbolNames
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintReal (
	char *pBuffer,
	size_t nBufferLength,
	BACSTAC_REAL real
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintDouble (
	char *pBuffer,
	size_t nBufferLength,
	BACSTAC_DOUBLE real
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintRecipient (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_RECIPIENT *pRecipient
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintSetpointReference (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_SETPOINT_REF *pSetpointRef,
	BACSTAC_BOOLEAN bSymbolNames
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintInteger (
	char *pBuffer,
	size_t nBufferLength,
	BACSTAC_INTEGER i
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintSpecialEvent (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_SPECIAL_EVENT *pSpecEvent,
	BACSTAC_BOOLEAN bSymbolNames
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintString (
	char *pBuffer,
	size_t nBufferLength,
	const char *pString
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintStringLen (
	char *pBuffer,
	size_t nBufferLength,
	const char *pString,
	size_t nStrLen
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintTag (
	char *pBuffer,
	size_t nBufferLength,
	BACSTAC_ENUM Tag
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintTime (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_TIME *pTime
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintTimeStamp (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_TIME_STAMP *pTimeStamp
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintTimeValue (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_TIME_VALUE *pTimeValue,
	BACSTAC_OBJECT_TYPE ObjType,
	BACSTAC_PROPERTY_ID PropID,
	BACSTAC_BOOLEAN bSymbolNames
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintUnsigned (
	char *pBuffer,
	size_t nBufferLength,
	BACSTAC_UNSIGNED uInt
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintUInt64 (
	char *pBuffer,
	size_t nBufferLength,
	BACSTAC_UI64 uInt
);

#ifdef BACSTAC_USE_64_BIT_READ_RANGE
#define BACstacSPrintSeqNum BACstacSPrintUInt64
#else
#define BACstacSPrintSeqNum BACstacSPrintUnsigned
#endif

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintWeekNDay (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_WEEK_N_DAY *pWeekNDay
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintDevObjPropValue (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_DEV_OBJ_PROP_VALUE *pDevObjPropValue,
	BACSTAC_BOOLEAN bSymbolNames
);

BACSTAC_NONNULL_ARGS(())

size_t BACstacSPrintStringUTF8 (
	char *pBuffer,
	size_t nBufferLength,
	const char *pString,
	BACSTAC_VALUE_SIZE strLen
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintStringAscii (
	char *pBuffer,
	size_t nBufferLength,
	const char *pString,
	BACSTAC_VALUE_SIZE strLen
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintStringDBCS (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_DBCS_STRING *pString,
	BACSTAC_VALUE_SIZE size
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintStringJISX (
	char *pBuffer,
	size_t nBufferLength,
	const char *pString,
	BACSTAC_VALUE_SIZE size
);

/* for backward compatibility only */
#define BACstacSPrintStringJISC BACstacSPrintStringJISX

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintStringUCS4 (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_UCS4_CHAR * pString,
	BACSTAC_VALUE_SIZE size
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintStringUCS2 (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_UCS2_CHAR * pString,
	BACSTAC_VALUE_SIZE size
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintStringLatin1 ( /* ISO 8859-1 */
	char *pBuffer,
	size_t nBufferLength,
	const char *pString,
	BACSTAC_VALUE_SIZE size
);

/* for backward compatibility */
#define BACstacSPrintStringISO8859 BACstacSPrintStringLatin1
#define BACstacSPrintStringISO_8859_1 BACstacSPrintStringLatin1

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintCharString (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_CHAR_STRING *pString
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintRecipientProc (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_RECIPIENT_PROCESS *pRecipientProc
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintCOVSubscription (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_COV_SUBSCRIPTION *pCOVSubscription,
	BACSTAC_BOOLEAN bSymbolNames
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintAccumulatorRecord (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_ACCUMULATOR_RECORD *pAccRecord,
	BACSTAC_BOOLEAN bSymbolNames
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintPrescale (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_PRESCALE *pPrescale
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintScale (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_SCALE *pScale
);

BACSTAC_NONNULL_ARGS(())
size_t  BACstacSPrintShedLevel (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_SHED_LEVEL *pShedLevel
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintLogRecord (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_LOG_RECORD *pInfo,
	BACSTAC_BOOLEAN bSymbolNames
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintVtSession (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_VT_SESSION *pVtSession
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintSessionKey (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_SESSION_KEY *pSessionKey
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintEventNotif (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_EVENT_NOTIF *pInfo,
	BACSTAC_BOOLEAN bSymbolNames
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintEventLogRecord (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_EVENT_LOG_RECORD *pInfo,
	BACSTAC_BOOLEAN bSymbolNames
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintLogMultiRecord (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_LOG_MULTI_RECORD *pInfo,
	BACSTAC_BOOLEAN bSymbolNames
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintAuthnPolicy (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_AUTHN_POLICY *pInfo
);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintAuthnFactor (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_AUTHN_FACTOR *pInfo,
	BACSTAC_BOOLEAN bSymbolNames
	);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintAccessRule (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_ACCESS_RULE *pInfo,
	BACSTAC_BOOLEAN bSymbolNames
	);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintAccessRights (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_ACCESS_RIGHTS *pInfo
	);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintAuthnFactorFormat (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_AUTHN_FACTOR_FORMAT *pInfo,
	BACSTAC_BOOLEAN bSymbolNames
	);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintCredAuthnFactor (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_CRED_AUTHN_FACTOR *pInfo,
	BACSTAC_BOOLEAN bSymbolNames
	);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintOptCharString (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_OPT_CHAR_STRING *pString
	);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintPropAccessResult (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_PROP_ACCESS_RESULT *pPropAccRes,
	BACSTAC_BOOLEAN bSymbolNames
	);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintFaultParameter (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_FAULT_PARAMETER *pFaultParam,
	BACSTAC_BOOLEAN bSymbolNames
	);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintENSubscription (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_EVENT_NOTIF_SUBS *pENSubscription
	);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintPortPermission (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_PORT_PERMISSION *pPortPermission
	);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintProcIdSelection (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_PROC_ID_SELECTION *pProcSel
	);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintOptCmdUnsigned (
	char *pBuffer,
	size_t nBufferLength,
	BACSTAC_OPT_CMD_UNSIGNED optCmdUnsigned
	);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintHostAddress (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_HOST_ADDRESS *pHostAddress
	);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintHostNPort (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_HOST_NPORT *pHostNPort
	);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintBDTEntry (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_BDT_ENTRY *pBDTEntry
	);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintFDTEntry (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_FDT_ENTRY *pFDTEntry
	);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintVMACEntry (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_VMAC_ENTRY *pVMACEntry
	);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintRouterEntry (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_ROUTER_ENTRY *pRouterEntry
	);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintValueSource (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_VALUE_SOURCE *pValueSource
	);

#define BACstacSPrintFloorNumber(pBuffer, nBufferLength, uFloorNumber) \
	BACstacSPrintUnsigned((pBuffer), (nBufferLength), (uFloorNumber))

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintLandingCall (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_LANDING_CALL *pLandingCall,
	BACSTAC_BOOLEAN bSymbolNames
	);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintAssignedCalls (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_ASSIGNED_LANDING_CALLS *pAssignedCalls,
	BACSTAC_BOOLEAN bSymbolNames
	);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintLandingCallStatus (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_LANDING_CALL_STATUS *pLCStatus,
	BACSTAC_BOOLEAN bSymbolNames
	);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintLandingDoor (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_LANDING_DOOR *pLandingDoor,
	BACSTAC_BOOLEAN bSymbolNames
	);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintLandingDoorStatus (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_LANDING_DOOR_STATUS *pLandingDoorStatus,
	BACSTAC_BOOLEAN bSymbolNames
	);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintLiftCarCallList (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_LIFT_CAR_CALL_LIST *pLiftCarCallList
	);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintCOVSudsSpec (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_COV_SUBSCRIPTION_SPEC *pCOVSudsSpec,
	BACSTAC_BOOLEAN bSymbolNames
	);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintCOVMultiSubs (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_COV_MULTI_SUBSCRIPTION *pSubscription,
	BACSTAC_BOOLEAN bSymbolNames
	);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintLightingCmd (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_LIGHTING_CMD *pCmd,
	BACSTAC_BOOLEAN bSymbolNames
	);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintChannelValue (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_CHANNEL_VALUE *pValue,
	BACSTAC_BOOLEAN bSymbolNames
	);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintOptReal (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_OPT_REAL *pOptReal
	);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintOptBinaryPV (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_OPT_BINARY_PV *pOptBinaryPV
	);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintOptUnsigned (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_OPT_UNSIGNED *pOptUnsigned
	);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintNetSecurityPolicy (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_NET_SECURITY_POLICY *pPolicy,
	BACSTAC_BOOLEAN bSymbolNames
	);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintKeyID (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_KEY_ID *pKeyID
	);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintSecurityKeySet (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_SECURITY_KEY_SET *pKeySet
	);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintTimerStateChangeValue (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_TS_CHANGE_VALUE *pValue,
	BACSTAC_BOOLEAN bSymbolNames
	);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintNameValue (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_NAME_VALUE *pNameValue,
	BACSTAC_BOOLEAN bSymbolNames
	);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintNameValueCollection (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_NAME_VALUE_COLLECTION *pCollection,
	BACSTAC_BOOLEAN bSymbolNames
	);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintHealth (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_HEALTH *pHealth,
	BACSTAC_BOOLEAN bSymbolNames
	);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintSCDirectConnection (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_SC_DIRECT_CONNECTION *pScdc,
	BACSTAC_BOOLEAN bSymbolNames
	);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintSCHubConnection (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_SC_HUB_CONNECTION *pSchc,
	BACSTAC_BOOLEAN bSymbolNames
	);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintSCFailedConnReq (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_SC_FAILED_CONN_REQ *pScfcr
	);

size_t BACstacSPrintSCHubFuncConn (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_SC_HUB_FUNC_CONN *pSchfc,
	BACSTAC_BOOLEAN bSymbolNames
	);

size_t BACstacSPrintObjectSelector (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_OBJECT_SELECTOR *pObjSelector
	);

size_t BACstacSPrintOptPriFilter (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_OPT_PRI_FILTER *pOptPryFilter,
	BACSTAC_BOOLEAN bSymbolNames
	);

size_t BACstacSPrintAuditNotif (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_AUDIT_NOTIF *pNotif,
	BACSTAC_BOOLEAN bSymbolNames
	);

size_t BACstacSPrintAuditLogRecord (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_AUDIT_LOG_RECORD *pRecord,
	BACSTAC_BOOLEAN bSymbolNames
	);

size_t BACstacSPrintAckAlarmInfo (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_ACK_ALARM_INFO_TV *pInfo,
	BACSTAC_BOOLEAN bSymbolNames
	);

size_t BACstacSPrintLifeSafetyOperationInfo (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_LIFE_SAFETY_INFO_TV *pInfo,
	BACSTAC_BOOLEAN bSymbolNames
	);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintStageLimitValue (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_STAGE_LIMIT_VALUE *pLimitValue
	);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintXYColor (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_XY_COLOR *xyColor
	);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintColorCmd (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_COLOR_CMD *colorCommand,
	BACSTAC_BOOLEAN bSymbolNames
	);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintIAmRequest (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_I_AM_INFO *iAmInfo,
	BACSTAC_BOOLEAN bSymbolNames
	);

BACSTAC_NONNULL_ARGS(())
size_t BACstacSPrintDevAddrProxyTableEntry (
	char *pBuffer,
	size_t nBufferLength,
	const BACSTAC_DEV_ADDR_PROXY_TABLE_ENTRY *devAddrProxyTableEntry,
	BACSTAC_BOOLEAN bSymbolNames
	);

#include "bacenum.h"

ENUM_TYPE_ID GetEnumTypeID (
	BACSTAC_OBJECT_TYPE objType,
	BACSTAC_PROPERTY_ID nPropID
);


/* some BACstac Enumerations */

#define BACstacDayOfWeekBitsString(DayOfWeekBits) \
	(BACstacEnumString (ENUM_DAY_OF_WEEK_BITS, DayOfWeekBits))

#define BACstacDayOfWeekString(DayOfWeek) \
	(BACstacEnumString (ENUM_DAY_OF_WEEK, DayOfWeek))

#define BACstacMonthString(Month) \
	(BACstacEnumString (ENUM_MONTH, Month))

#define BACstacObjTypeString(ObjType) \
	(BACstacEnumString (ENUM_OBJECT_TYPE, ObjType))

#define BACstacPropIDString(PropID) \
	(BACstacEnumString (ENUM_PROPERTY_ID, PropID))

/* Enumerated and Bit String Properties */

#define BACstacActionString(Action) \
	(BACstacEnumString (ENUM_ACTION, Action))

#define BACstacBinaryPVString(BinaryPV) \
	(BACstacEnumString (ENUM_BINARY_PV, BinaryPV))

#define BACstacDevStatusString(DeviceStatus) \
	(BACstacEnumString (ENUM_DEVICE_STATUS, DeviceStatus))

#define BACstacEventStateString(EventState) \
	(BACstacEnumString (ENUM_EVENT_STATE, EventState))

#define BACstacEventTransitionString(EventBits) \
	(BACstacEnumString (ENUM_EVENT_TRANSITION_BITS, EventBits))

#define BACstacEventTypeString(EventType) \
	(BACstacEnumString (ENUM_EVENT_TYPE, EventType))

#define BACstacLimitEnableString(LimitEnable) \
	(BACstacEnumString (ENUM_LIMIT_ENABLE, LimitEnable))

#define BACstacNotifyTypeString(NotifyType) \
	(BACstacEnumString (ENUM_NOTIFY_TYPE, NotifyType))

#define BACstacPolarityString(Polarity) \
	(BACstacEnumString (ENUM_POLARITY, Polarity))

#define BACstacProgramErrorString(ProgramError) \
	(BACstacEnumString (ENUM_PROGRAM_ERROR, ProgramError))

#define BACstacProgramRequestString(ProgramRequest) \
	(BACstacEnumString (ENUM_PROGRAM_REQUEST, ProgramRequest))

#define BACstacProgramStateString(ProgramState) \
	(BACstacEnumString (ENUM_PROGRAM_STATE, ProgramState))

#define BACstacReliabilityString(Reliability) \
	(BACstacEnumString (ENUM_RELIABILITY, Reliability))

#define BACstacSegmentString(Segmentation) \
	(BACstacEnumString (ENUM_SEGMENTATION, Segmentation))

#define BACstacServiceString(Service) \
	(BACstacEnumString (ENUM_SERVICES_BITS, Service))

#define BACstacStatusFlagString(StatusFlag) \
	(BACstacEnumString (ENUM_STATUS_FLAG_BITS, StatusFlag))

#define BACstacUnitsString(EngUnits) \
	(BACstacEnumString (ENUM_ENGINEERING_UNITS, EngUnits))

#define BACstacFileAccessString(FileAccess) \
	(BACstacEnumString (ENUM_FILE_ACCESS_METHOD, FileAccess))


#define BACstacLifeSafetyModeString(LifeSafetyMode) \
	(BACstacEnumString (ENUM_LIFE_SAFETY_MODE, LifeSafetyMode))


#define BACstacLifeSafetyOpString(LifeSafetyOperation) \
	(BACstacEnumString (ENUM_LIFE_SAFETY_OPERATION, LifeSafetyOperation))


#define BACstacLifeSafetyStateString(LifeSafetyState) \
	(BACstacEnumString (ENUM_LIFE_SAFETY_STATE, LifeSafetyState))


#define BACstacMaintenanceString(Maintenance) \
	(BACstacEnumString (ENUM_MAINTENANCE, Maintenance))


#define BACstacSilencedStateString(SilencedState) \
	(BACstacEnumString (ENUM_SILENCED_STATE, SilencedState))


#define BACstacAccumulatorStatusString(AccumulatorStatus) \
	(BACstacEnumString (ENUM_ACCUMULATOR_STATUS, AccumulatorStatus))

#define BACstacNodeTypeString(NodeType) \
	(BACstacEnumString (ENUM_NODE_TYPE, NodeType))

#define BACstacShedStateString(ShedState) \
	(BACstacEnumString (ENUM_SHED_STATE, ShedState))

#define BACstacDoorValueString(DoorValue) \
	(BACstacEnumString (ENUM_DOOR_VALUE, DoorValue))

#define BACstacLockStatusString(LockStatus) \
	(BACstacEnumString (ENUM_LOCK_STATUS, LockStatus))

#define BACstacDoorSecutedStatusString(DoorSecutedStatus) \
	(BACstacEnumString (ENUM_DOOR_SECURED_STATUS, DoorSecutedStatus))

#define BACstacDoorStatusString(DoorStatus) \
	(BACstacEnumString (ENUM_DOOR_STATUS, DoorStatus))

#define BACstacDoorAlarmStateString(DoorAlarmState) \
	(BACstacEnumString (ENUM_DOOR_ALARM_STATE, DoorAlarmState))

#define BACstacLoggingTypeString(LoggingType) \
	(BACstacEnumString (ENUM_LOGGING_TYPE, LoggingType))

#define BACstacRestartReasonString(LoggingType) \
	(BACstacEnumString (ENUM_RESTART_REASON, RestartReason))

#define BACstacAuthnFactorTypeString(AuthnFactorType) \
	(BACstacEnumString(ENUM_AUTHN_FACTOR_TYPE, AuthnFactorType))

#define BACstacAccessEventString(AccessEvent) \
	(BACstacEnumString(ENUM_ACCESS_EVENT, AccessEvent))

#define BACstacAuthnStatusString(AuthnStatus) \
	(BACstacEnumString(ENUM_AUTHN_STATUS, AuthnStatus))

#define BACstacAuthzModeString(AuthzMode) \
	(BACstacEnumString(ENUM_AUTHZ_MODE, AuthzMode))

#define BACstacZoneOccupancyStateString(ZoneOccupancyState) \
	(BACstacEnumString(ENUM_ZONE_OCCUPANCY_STATE, ZoneOccupancyState))

#define BACstacAccessCredDisableString(AccessCredDisable) \
	(BACstacEnumString(ENUM_ACCESS_CRED_DISABLE, AccessCredDisable))

#define BACstacAccessCredDisableReasonString(AccessCredDisableReason) \
	(BACstacEnumString(ENUM_ACCESS_CRED_DISABLE_REASON, AccessCredDisableReason))

#define BACstacAccessPassbackModeString(AccessPassbackMode) \
	(BACstacEnumString(ENUM_ACCESS_PASSBACK_MODE, AccessPassbackMode))

#define BACstacAccessUserTypeString(AccessUserType) \
	(BACstacEnumString(ENUM_ACCESS_USER_TYPE, AccessUserType))

#define BACstacAccessAuthnFactorDisableString(AccessAuthnFactorDisable) \
	(BACstacEnumString(ENUM_ACCESS_AUTHN_FACTOR_DISABLE, AccessAuthnFactorDisable))

#define BACstacBackupStateString(BackupState) \
	(BACstacEnumString(ENUM_BACKUP_STATE, BackupState))

#define BACstacFaultTypeString(FaultType) \
	(BACstacEnumString(ENUM_FAULT_TYPE, FaultType))

#define BACstacAuthorizationExemptionString(AuthorizationExemption) \
	(BACstacEnumString(ENUM_AUTHZ_EXEMPTION, AuthorizationExemption))

#define BACstacBIPModeString(BIPMode) \
	(BACstacEnumString(ENUM_BIP_MODE, BIPMode))

#define BACstacNetPortCmdString(NetPortCmd) \
	(BACstacEnumString(ENUM_NET_PORT_CMD, NetPortCmd))

#define BACstacNetNumberQualityString(NetNumberQuality) \
	(BACstacEnumString(ENUM_NET_NUMBER_QUALITY, NetNumberQuality))

#define BACstacNetworkTypeString(NetworkType) \
	(BACstacEnumString(ENUM_NETWORK_TYPE, NetworkType))

#define BACstacEscalatorFaultString(EscalatorFault) \
	(BACstacEnumString(ENUM_ESCALATOR_FAULT, EscalatorFault))

#define BACstacEscalatorModeString(EscalatorMode) \
	(BACstacEnumString(ENUM_ESCALATOR_MODE, EscalatorMode))

#define BACstacEscalatorOperDirString(EscalatorOperDir) \
	(BACstacEnumString(ENUM_ESCALATOR_OPER_DIR, EscalatorOperDir))

#define BACstacLiftCarDirString(LiftCarDir) \
	(BACstacEnumString(ENUM_LIFT_CAR_DIR, LiftCarDir))

#define BACstacLiftCarDoorCmdString(LiftCarDoorCmd) \
	(BACstacEnumString(ENUM_LIFT_CAR_DOOR_CMD, LiftCarDoorCmd))

#define BACstacLiftCarDriveStatusString(LiftCarDriveStatus) \
	(BACstacEnumString(ENUM_LIFT_CAR_DRIVE_STATUS, LiftCarDriveStatus))

#define BACstacLiftCarModeString(LiftCarMode) \
	(BACstacEnumString(ENUM_LIFT_CAR_MODE, LiftCarMode))

#define BACstacLiftFaultString(LiftFault) \
	(BACstacEnumString(ENUM_LIFT_FAULT, LiftFault))

#define BACstacLiftGroupModeString(LiftGroupMode) \
	(BACstacEnumString(ENUM_LIFT_GROUP_MODE, LiftGroupMode))

#define BACstacProtocolLevelString(ProtocolLevel) \
	(BACstacEnumString(ENUM_PROTOCOL_LEVEL, ProtocolLevel))

#define BACstacLightingOperationString(LightingOperation) \
	(BACstacEnumString(ENUM_LIGHTING_OPERATION, LightingOperation))

#define BACstacLightingInProgressString(LightingInProgress) \
	(BACstacEnumString(ENUM_LIGHTING_IN_PROGRESS, LightingInProgress))

#define BACstacLightingTransitionString(LightingTransition) \
	(BACstacEnumString(ENUM_LIGHTING_TRANSITION, LightingTransition))

#define BACstacWriteStatusString(WriteStatus) \
	(BACstacEnumString(ENUM_WRITE_STATUS, WriteStatus))

#define BACstacBinaryLightingPVString(BinaryLightingPV) \
	(BACstacEnumString(ENUM_ BINARY_LIGHTING_PV, BinaryLightingPV))

#define BACstacSecurityLevelString(SecurityLevel) \
	(BACstacEnumString(ENUM_SECURITY_LEVEL, SecurityLevel))

#define BACstacSecurityPolicyString(SecurityPolicy) \
	(BACstacEnumString(ENUM_SECURITY_POLICY, SecurityPolicy))

#define BACstacTimerStateString(TimerState) \
	(BACstacEnumString(ENUM_TIMER_STATE, TimerState))

#define BACstacTimerTransitionString(TimerTransition) \
	(BACstacEnumString(ENUM_TIMER_TRANSITION, TimerTransition))

#define BACstacRelationshipString(Relationship) \
	(BACstacEnumString(ENUM_RELATIONSHIP, Relationship))

#define BACstacVTClassString(VTClass) \
	(BACstacEnumString (ENUM_VT_CLASS, VTClass))

#define BACstacSCConnectionStateString(SCConnectionState) \
	(BACstacEnumString (ENUM_SC_CONNECT_STATE, SCConnectionState))

#define BACstacSCHubConnectorStateString(SCHubConnectorState) \
	(BACstacEnumString (ENUM_SC_HUB_CONNECTOR_STATE, SCHubConnectorState))

#define BACstacAuditLevelString(AuditLevel) \
	(BACstacEnumString(ENUM_AUDIT_LEVEL, AuditLevel))

#define BACstacAuditOperationString(AuditOperation) \
	(BACstacEnumString(ENUM_AUDIT_OPERATION, AuditOperation))

#define BACstacSuccessFilterString(SuccessFilter) \
	(BACstacEnumString(ENUM_SUCCESS_FILTER, SuccessFilter))

#define BACstacPriorityFilterString(PriorityFilter) \
	(BACstacEnumString(ENUM_PRIORITY_FILTER_BITS, PriorityFilter))

#define BACstacColorOperationString(ColorOperation) \
	(BACstacEnumString(COLOR_OPETATION, ColorOperation))

#define BACstacColorOperationInProgress(ColorOperationInProgress) \
	(BACstacEnumString(COLOR_OPERATION_IN_PROGRESS, ColorOperationInProgress))

#define BACstacColorTransition(ColorTransition) \
	(BACstacEnumString(COLOR_TRANSITION, ColorTransition))

extern const char UnknownString[];
extern const char UnspecifiedString[];
extern const char CommaString[];
extern const char QuoteString[];
extern const char SingleQuoteString[];
extern const char HexQuoteString[];
extern const char HexUnspecifiedString[];
extern const char SpaceString[];
extern const char NullString[];
extern const char TrueString[];
extern const char FalseString[];
extern const char LBraceString[];
extern const char RBraceString[];
extern const char LBracketString[];
extern const char RBracketString[];
extern const char HyphenString[];

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* BACPRINT_H */
