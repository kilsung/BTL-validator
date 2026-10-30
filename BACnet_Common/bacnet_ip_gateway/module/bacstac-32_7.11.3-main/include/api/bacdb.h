/*
 * BACstac(TM) BACDB.H
 *
 * Copyright (c) 2005-2024 by Cimetrics Inc. All rights reserved.
 *
 * Purpose: define BACstac database API
 *
 */
#ifndef BACDB_H
#define BACDB_H

#include "bactypes.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*
 * Database Property Template
 */

/* Regular property flags */
#define BACSTAC_WRITABLE_FLAG           0x01
#define BACSTAC_READ_CB_FLAG            0x02
#define BACSTAC_WRITE_CB_FLAG           0x04
#define BACSTAC_CHANGE_LIST_CB_FLAG     0x08
#define BACSTAC_SUBSCRIBABLE_FLAG       0x10

/* Flags determining property types */
/* Regular property, which are stored in the database in the ASN.1 format */
#define BACSTAC_REG_PROP                0x00
/* automatic property, which are calculated based on some other properties */
#define BACSTAC_AUTO_PROP               0x20

typedef struct BACSTAC_DB_PROPERTY {
    BACSTAC_PROPERTY_ID       propertyID;
    BACSTAC_UINT              flags;
    BACSTAC_PROPERTY_CONTENTS contents;
} BACSTAC_DB_PROPERTY;

#define BACSTAC_DB_PROPERTY_INIT(propID, contents, flags) \
   {(propID), (flags), contents}

BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacInitDbProperty(
    BACSTAC_DB_PROPERTY *dbProperty,
    BACSTAC_PROPERTY_ID propertyID,
    const BACSTAC_PROPERTY_CONTENTS *contents,
    BACSTAC_UINT flags);


/*
 * Database Object Template
 */

/* Object flags */
#define BACSTAC_REG_OBJECT 0

typedef struct BACSTAC_DB_OBJECT {
    BACSTAC_UINT                          flags; /* must be BACSTAC_REG_OBJECT */
    BACSTAC_PROPERTY_COUNT                propCount;
    const BACSTAC_DB_PROPERTY *propList;
} BACSTAC_DB_OBJECT;

#define BACSTAC_DB_OBJECT_INIT(propCount, propList) \
    {BACSTAC_REG_OBJECT, (propCount), (propList)}

BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacInitDbObject(
    BACSTAC_DB_OBJECT *dbObject,
    const BACSTAC_DB_PROPERTY *propList,
    BACSTAC_PROPERTY_COUNT propCount);

/*
 * Database Device Template
 */

typedef struct BACSTAC_DB_DEVICE {
    BACSTAC_UI32                        signature;
    BACSTAC_OBJECT_COUNT                objCount;
    const BACSTAC_DB_OBJECT *objList;
} BACSTAC_DB_DEVICE;

#define BACSTAC_DB_DEVICE_SIG       0x44624476
#define BACSTAC_DB_DEVICE_INIT(objCount, objList) \
    {BACSTAC_DB_DEVICE_SIG, (objCount), (objList)}

BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacInitDbDevice(
    BACSTAC_DB_DEVICE *dbDevice,
    const BACSTAC_DB_OBJECT *objList,
    BACSTAC_OBJECT_COUNT objCount);


/*
 * BACstacCreateDBObject
 */

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacCreateDBObject (
    BACSTAC_HDEVICE hDevice,            /* in            */
    const BACSTAC_DB_OBJECT *pDBObj,    /* in            */
    BACSTAC_HOBJECT *pObjectHandle,     /* out, optional */
    BACSTAC_PROPERTY_COUNT *firstFailed /* out, optional */
) BACSTAC_NOTHROW;


BACSTAC_DEPRECATED /* for backward compatibility only */
BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacConstructObject (
    BACSTAC_HDEVICE hDevice,
    const BACSTAC_OBJECT_ID *pObjID,
    const BACSTAC_DEPRECATED_PROPERTY_INSTANCE *propList,
    BACSTAC_PROPERTY_COUNT propCount,
    BACSTAC_HOBJECT *pObjectHandle
) BACSTAC_NOTHROW;


/*
 * BACstacDestroyObject
 */


BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacDestroyObject (
    BACSTAC_HOBJECT hObject
) BACSTAC_NOTHROW;


/*
 * BACstacStorePropertyInstance
 */


BACSTACAPI BACSTAC_STATUS BACstac_api BACstacStorePropertyInstance (
    BACSTAC_HOBJECT hObject,
    BACSTAC_PROPERTY_ID ePropertyID,
    BACSTAC_ARRAY_INDEX nIndex,
    const BACSTAC_PROPERTY_CONTENTS *pValue
) BACSTAC_NOTHROW;


/*
 * BACstacRetrievePropertyInstance
 */


BACSTACAPI BACSTAC_STATUS BACstac_api BACstacRetrievePropertyInstance2 (
   BACSTAC_HOBJECT           hObject,
   BACSTAC_PROPERTY_ID       ePropertyID,
   BACSTAC_ARRAY_INDEX       nIndex,
   BACSTAC_PROPERTY_CONTENTS *pValue,
   BACSTAC_IALLOC            *iAlloc
) BACSTAC_NOTHROW;

BACSTAC_DEPRECATED /* for backward compatibility only */
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacRetrievePropertyInstance (
   BACSTAC_HOBJECT           hObject,
   BACSTAC_PROPERTY_ID       ePropertyID,
   BACSTAC_ARRAY_INDEX       nIndex,
   BACSTAC_PROPERTY_CONTENTS *pValue
) BACSTAC_NOTHROW;


/*
 * BACstacRetrieveListItems
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacRetrieveListItems (
   BACSTAC_HOBJECT                       hObject,
   BACSTAC_PROPERTY_ID                   ePropertyID,
   BACSTAC_ARRAY_INDEX                   nIndex,
   BACSTAC_UI64                          position,
   BACSTAC_INTEGER                       count,
   BACSTAC_PROPERTY_CONTENTS *pValue,
   BACSTAC_IALLOC                        *iAlloc,
   BACSTAC_BIT_STRING                    *pResultFlags
) BACSTAC_NOTHROW;

/*
 * BACstacRetrieveFaultParameters
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacRetrieveFaultParameters (
   BACSTAC_HOBJECT hObject,
   BACSTAC_BUFFER  *buffer,
   BACSTAC_IALLOC  *iAlloc
) BACSTAC_NOTHROW;


/*
 * BACstacRetrieveEventParameters
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacRetrieveEventParameters (
   BACSTAC_HOBJECT hObject,
   BACSTAC_BUFFER  *buffer,
   BACSTAC_IALLOC  *iAlloc
) BACSTAC_NOTHROW;


/*
 * BACstacStoreRawAsnProperty
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacStoreRawAsnProperty (
   BACSTAC_HOBJECT                      hObject,
   BACSTAC_PROPERTY_ID                  ePropertyID,
   BACSTAC_ARRAY_INDEX                  nIndex,
   const BACSTAC_BYTE                   *rawAsn,
   BACSTAC_VALUE_SIZE                   length
) BACSTAC_NOTHROW;

/*
 * BACstacRetrieveRawAsnProperty
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacRetrieveRawAsnProperty (
   BACSTAC_HOBJECT                      hObject,
   BACSTAC_PROPERTY_ID                  ePropertyID,
   BACSTAC_ARRAY_INDEX                  nIndex,
   BACSTAC_BYTE                         *buffer,
   BACSTAC_VALUE_SIZE                   bufSize,
   BACSTAC_VALUE_SIZE                   *pLength /* out */
) BACSTAC_NOTHROW;


/*
 * BACstacDoesPropertyExist
 */


BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacDoesPropertyExist (
    BACSTAC_HOBJECT hObject,
    BACSTAC_PROPERTY_ID ePropertyID
) BACSTAC_NOTHROW;


/*
 * BACstacCheckPropertyExistence
 *
 * If you rely on this function in your hook, it is most likely that your
 * hook is broken! Don't use this function!
 */

BACSTAC_DEPRECATED
BACSTACAPI BACSTAC_UTIL_STATUS BACstac_api BACstacCheckPropertyExistence (
    const BACSTAC_ADDRESS *deviceAddress,
    const BACSTAC_OBJECT_ID *objectID,
    BACSTAC_PROPERTY_ID ePropertyID,
    BACSTAC_ERROR_CLASS *errClass,
    BACSTAC_ERROR_CODE *errCode
) BACSTAC_NOTHROW;


/*
 * BACstacIsPropertyWritable
 */


BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacIsPropertyWritable (
    BACSTAC_HOBJECT hObject,
    BACSTAC_PROPERTY_ID ePropertyID
) BACSTAC_NOTHROW;


/*
 * BACstacIsArrayIndexValid
 */


BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacIsArrayIndexValid (
    BACSTAC_HOBJECT hObject,
    BACSTAC_PROPERTY_ID ePropertyID,
    BACSTAC_ARRAY_INDEX nIndex
) BACSTAC_NOTHROW;


/*
 * BACstacIsWritePriorityValid
 */


BACSTAC_DEPRECATED /* for backward compatibility only */
BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacIsWritePriorityValid (
    BACSTAC_HOBJECT hObject,
    BACSTAC_PROPERTY_ID ePropertyID,
    BACSTAC_PRIORITY_LEVEL nWritePriority
) BACSTAC_NOTHROW;


/*
 * BACstacIsPropertyCommandable
 */


BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacIsPropertyCommandable (
    BACSTAC_HOBJECT hObject,
    BACSTAC_PROPERTY_ID ePropertyID
) BACSTAC_NOTHROW;


/*
 * BACstacReadCommandableValue
 */


BACSTAC_DEPRECATED /* for backward compatibility only */
BACSTACAPI BACSTAC_UTIL_STATUS BACstac_api BACstacReadCommandableValue2 (
    BACSTAC_HOBJECT hObject,
    BACSTAC_PROPERTY_ID ePriorityArrayID,
    BACSTAC_PROPERTY_ID eRelinquishDefaultID,
    BACSTAC_PROPERTY_CONTENTS *pValue,
    BACSTAC_ERROR_CLASS *pErrClass,
    BACSTAC_ERROR_CODE *pErrCode,
    BACSTAC_IALLOC *iAlloc
) BACSTAC_NOTHROW;

BACSTAC_DEPRECATED /* for backward compatibility only */
BACSTACAPI BACSTAC_UTIL_STATUS BACstac_api BACstacReadCommandableValue (
    BACSTAC_HOBJECT hObject,
    BACSTAC_PROPERTY_ID ePriorityArrayID,
    BACSTAC_PROPERTY_ID eRelinquishDefaultID,
    BACSTAC_PROPERTY_CONTENTS *pValue,
    BACSTAC_ERROR_CLASS *pErrClass,
    BACSTAC_ERROR_CODE *pErrCode
) BACSTAC_NOTHROW;

/*
 * BACstacWriteCommandableValue
 */


BACSTAC_DEPRECATED /* for backward compatibility only */
BACSTACAPI BACSTAC_UTIL_STATUS BACstac_api BACstacWriteCommandableValue (
    BACSTAC_HOBJECT hObject,
    BACSTAC_PROPERTY_ID eCommandableID,
    BACSTAC_PROPERTY_ID ePriorityArrayID,
    BACSTAC_PROPERTY_ID eRelinquishDefaultID,
    const BACSTAC_PROPERTY_CONTENTS *pValue,
    BACSTAC_PRIORITY_LEVEL nWritePriority,
    BACSTAC_ERROR_CLASS *pErrClass,
    BACSTAC_ERROR_CODE *pErrCode
) BACSTAC_NOTHROW;


/*
 * BACstacAddElement
 */


BACSTACAPI BACSTAC_UTIL_STATUS BACstac_api BACstacAddElement (
    BACSTAC_HOBJECT hObject,
    BACSTAC_PROPERTY_ID ePropertyID,
    BACSTAC_ARRAY_INDEX nIndex,                 /* or VOID */
    const BACSTAC_PROPERTY_CONTENTS *pElements,
    BACSTAC_ERROR_CLASS *errClass,              /* or NULL */
    BACSTAC_ERROR_CODE *errCode,                /* or NULL */
    BACSTAC_ELEMENT_COUNT *pFirstFailed         /* or NULL */
) BACSTAC_NOTHROW;


/*
 * BACstacRemoveElement
 */


BACSTACAPI BACSTAC_UTIL_STATUS BACstac_api BACstacRemoveElement (
    BACSTAC_HOBJECT hObject,
    BACSTAC_PROPERTY_ID ePropertyID,
    BACSTAC_ARRAY_INDEX nIndex,                 /* or VOID */
    const BACSTAC_PROPERTY_CONTENTS *pElements,
    BACSTAC_ERROR_CLASS *errClass,              /* or NULL */
    BACSTAC_ERROR_CODE *errCode,                /* or NULL */
    BACSTAC_ELEMENT_COUNT *pFirstFailed         /* or NULL */
) BACSTAC_NOTHROW;


/*
 * BACstacExecuteGetAlarmSummary
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacExecuteGetAlarmSummary (
    BACSTAC_HTSM hTransaction
) BACSTAC_NOTHROW;


/*
 * Callback declarations
 */

typedef BACSTAC_UI32 BACSTAC_CALLBACK_STATUS;

/* CALLBACK_STATUS_ERROR can be used to construct a callback status that
 * contains any standard BACnet error. It takes two parameters: Error
 * Class and Error Code, which are represented by BACSTAC_ERROR_CLASS
 * and BACSTAC_ERROR_CODE enumerations.
 */
#define CALLBACK_STATUS_ERROR(errClass,errCode) \
   ((BACSTAC_CALLBACK_STATUS)(0x80000000 | (((BACSTAC_UI32)(errClass))<<16) | ((BACSTAC_UI32)(errCode))))

/* CALLBACK_STATUS_OK indicates success. The new value will be stored in
 * the database, and, for the read request, it will be returned as response.
 */
#define CALLBACK_STATUS_OK       0U

/* CALLBACK_STATUS_DEFAULT means that the property was not either written
 * to or read from the device, but the default actions should act if the
 * callback was not attached to this property. Thus, for the write request,
 * the new value will be placed in the database. For the read request, the
 * current value stored in the database will be returned in response.
 */
#define CALLBACK_STATUS_DEFAULT  1U

/* CALLBACK_STATUS_WRITTEN means that the new value of the property has been
 * written by the callback, so the default action does not need to write
 * anything to the property.
 */

#define CALLBACK_STATUS_WRITTEN  2U

/* CALLBACK_STATUS_FAILURE indicates a general failure for reasons any
 * other than defined in Chapter 18 of the BACnet standard. This code
 * should be used only when there is no more specific BACnet error for
 * this failure.
 *
 * The DEVICE:OTHER error is generated in response.
 */
#define CALLBACK_STATUS_FAILURE \
    CALLBACK_STATUS_ERROR(ERR_CLASS_DEVICE, ERR_CODE_OTHER)

/* CALLBACK_STATUS_DEV_SHUT_DOWN - DO NOT USE THIS STATUS CODE!
 * It does not really work and therefore is declared as deprecated.
 * It will be removed from the future version of BACstac. Consider
 * to use the BACstacSetDCCValue() function if you need to temporary
 * disable all communication with this BACnet device.
 */
#define CALLBACK_STATUS_DEV_SHUT_DOWN  3U

/* CALLBACK_STATUS_DEV_BUSY indicates that the device expects to be
 * involved in higher priority processing for a time in excess of the
 * usual request/confirm timeout period.
 *
 * The DEVICE:DEVICE_BUSY error is generated in response.
 */
#define CALLBACK_STATUS_DEV_BUSY \
    CALLBACK_STATUS_ERROR(ERR_CLASS_DEVICE, ERR_CODE_DEVICE_BUSY)

/* CALLBACK_STATUS_DEV_OP_PROBLEM means that the device has detected
 * an operational problem that prevents it from carrying out the requested
 * operation.
 *
 * The DEVICE:OPERATIONAL_PROBLEM error is generated in response.
 */
#define CALLBACK_STATUS_DEV_OP_PROBLEM \
    CALLBACK_STATUS_ERROR(ERR_CLASS_DEVICE, ERR_CODE_OPERATIONAL_PROBLEM)

/* CALLBACK_STATUS_PROP_OUT_OF_RANGE means that the value was not
 * written because it is outside of the allowed range.
 *
 * The PROPERTY:VALUE_OUT_OF_RANGE error is generated in response.
 */
#define CALLBACK_STATUS_PROP_OUT_OF_RANGE \
    CALLBACK_STATUS_ERROR(ERR_CLASS_PROPERTY, ERR_CODE_VALUE_OUT_OF_RANGE)

/* CALLBACK_STATUS_PROP_OUT_OF_SPACE means that the value was not
 * written because not enough dynamic memory space exists in the
 * device.
 *
 * The PROPERTY:NO_SPACE_TO_WRITE_PROPERTY error is generated in response.
 */
#define CALLBACK_STATUS_PROP_OUT_OF_SPACE \
    CALLBACK_STATUS_ERROR(ERR_CLASS_RESOURCES, ERR_CODE_NO_SPACE_TO_WRITE_PROP)


/*
 * Read Property Callback
 */

typedef BACSTAC_CALLBACK_STATUS (BACstac_callback *BACSTAC_PROP_READ_CB) (
    BACSTAC_HOBJECT hObject,
    BACSTAC_PROPERTY_ID propertyID,
    BACSTAC_ARRAY_INDEX arrayIndex
);


/*
 * Write Property Callback
 */


typedef BACSTAC_CALLBACK_STATUS (BACstac_callback *BACSTAC_PROP_WRITE_CB) (
    BACSTAC_HOBJECT hObject,
    BACSTAC_PROPERTY_ID propertyID,
    BACSTAC_ARRAY_INDEX arrayIndex,
    const BACSTAC_PROPERTY_CONTENTS *pPropertyContents
);

 /*
 * Change List Callback
 */


typedef enum BACSTAC_LIST_OPERATION
{
   LIST_ADD_OPERATION,
   LIST_REMOVE_OPERATION
   BACSTAC_OTHER_ENUM(LIST_OPERATION)
}  BACSTAC_LIST_OPERATION;


typedef BACSTAC_CALLBACK_STATUS (BACstac_callback *BACSTAC_CHANGE_LIST_CB) (
    BACSTAC_HOBJECT hObject,
    BACSTAC_PROPERTY_ID propertyID,
    BACSTAC_ARRAY_INDEX arrayIndex,
    BACSTAC_LIST_OPERATION operation,
    const BACSTAC_PROPERTY_CONTENTS *pChange,
    const BACSTAC_PROPERTY_CONTENTS *pResult,
    BACSTAC_ELEMENT_COUNT *pFirstFailed
);

/*
 * Commandable Callback
 */

typedef struct BACSTAC_CALLBACK_INFO
{
    const BACSTAC_ADDRESS *valueSource; /* or NULL */
} BACSTAC_CALLBACK_INFO;

BACSTACAPI const BACSTAC_ADDRESS * BACstac_api BACstacGetCBInfoValueSource(
    const BACSTAC_CALLBACK_INFO *callbackInfo);

typedef BACSTAC_CALLBACK_STATUS (BACstac_callback *BACSTAC_WRITE_CMD_CB) (
    BACSTAC_HOBJECT hObject,
    BACSTAC_PROPERTY_ID propertyID,
    BACSTAC_ARRAY_INDEX arrayIndex,
    const BACSTAC_PROPERTY_CONTENTS *writtenValue,
    const BACSTAC_PROPERTY_CONTENTS *newPresentValue, /* NULL if PV is not changed */
    const BACSTAC_CALLBACK_INFO *extraInfo
);

/*
 * Object Change Callback
 */


typedef void (BACstac_callback *BACSTAC_OBJECT_CHANGE_CB) (
    BACSTAC_HOBJECT hObject
);



/*
 * BACstacSetCallback
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacSetReadCb (
    BACSTAC_PROP_READ_CB pCallbackProc
) BACSTAC_NOTHROW;

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacSetWriteCb (
    BACSTAC_PROP_WRITE_CB pCallbackProc
) BACSTAC_NOTHROW;

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacSetChangeListCb (
    BACSTAC_CHANGE_LIST_CB pCallbackProc
) BACSTAC_NOTHROW;

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacSetWriteCmdCb (
    BACSTAC_WRITE_CMD_CB pCallbackProc
) BACSTAC_NOTHROW;

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacSetObjChangeCb (
    BACSTAC_OBJECT_CHANGE_CB pCallbackProc
) BACSTAC_NOTHROW;


/*
 * BACstacGetCallback
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacGetReadCb (
    BACSTAC_PROP_READ_CB *pCallbackProc
) BACSTAC_NOTHROW;

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacGetWriteCb (
    BACSTAC_PROP_WRITE_CB *pCallbackProc
) BACSTAC_NOTHROW;

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacGetChangeListCb (
    BACSTAC_CHANGE_LIST_CB *pCallbackProc
) BACSTAC_NOTHROW;

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacGetWriteCmdCb (
    BACSTAC_WRITE_CMD_CB *pCallbackProc
) BACSTAC_NOTHROW;

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacGetObjChangeCb (
    BACSTAC_OBJECT_CHANGE_CB *pCallbackProc
) BACSTAC_NOTHROW;


/* BACSTAC_CB_TYPE is used to set and get callback attachment */
typedef enum BACSTAC_CB_TYPE {
    CB_READ,
    CB_WRITE, /* attach both Write and WriteCmd callbacks */
    CB_CHANGE_LIST
    BACSTAC_OTHER_ENUM(CB_TYPE)
} BACSTAC_CB_TYPE;

/*
 * BACstacSetCallbackAttachment
 */


BACSTACAPI BACSTAC_STATUS BACstac_api BACstacSetCallbackAttachment (
    BACSTAC_CB_TYPE type,
    BACSTAC_HOBJECT hObject,
    BACSTAC_PROPERTY_ID propertyID,
    BACSTAC_BOOLEAN bAttach
) BACSTAC_NOTHROW;


/*
 * BACstacSetObjChangeCbAttachment
 */


BACSTACAPI BACSTAC_STATUS BACstac_api BACstacSetObjChangeCbAttachment (
    BACSTAC_HOBJECT hObject,
    BACSTAC_BOOLEAN bAttach
) BACSTAC_NOTHROW;


/*
 * BACstacGetCallbackAttachment
 */


BACSTACAPI BACSTAC_STATUS BACstac_api BACstacGetCallbackAttachment (
    BACSTAC_CB_TYPE type,
    BACSTAC_HOBJECT hObject,
    BACSTAC_PROPERTY_ID propertyID,
    BACSTAC_BOOLEAN *pAttachment
) BACSTAC_NOTHROW;


/*
 * BACstacGetObjChangeCbAttachment
 */


BACSTACAPI BACSTAC_STATUS BACstac_api BACstacGetObjChangeCbAttachment (
    BACSTAC_HOBJECT hObject,
    BACSTAC_BOOLEAN *pAttachment
) BACSTAC_NOTHROW;


/*
 * BACstacSetPropertyAccess
 */


BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacSetPropertyAccess (
    BACSTAC_HOBJECT hObject,
    BACSTAC_PROPERTY_ID ID,
    BACSTAC_ACCESS fileAccess
) BACSTAC_NOTHROW;


/*
 * BACstacGetPropertyAccess
 */


BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacGetPropertyAccess (
    BACSTAC_HOBJECT hObject,
    BACSTAC_PROPERTY_ID ID,
    BACSTAC_ACCESS *fileAccess
) BACSTAC_NOTHROW;



BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacSynchDeviceTSMParams (
    BACSTAC_HDEVICE hDevice
) BACSTAC_NOTHROW;



#define BACSTAC_FORCE_FLAG      0x00001u
#define BACSTAC_INVOKE_CB_FLAG  0x10000u

/*
 * BACstacReadLocalProperties
 *
 * Parameters:
 *
 *   hDevice
 *     Handle of the device or BACSTAC_INVALID_HANDLE it if it is
 *     a server application.
 *
 *   pReadAccessSpec
 *     A pointer to a read access specification, which contains the object
 *     identifier and list of properties to read.
 *
 *   pReadAccessResult
 *     A pointer to the structure that contain the result of reading, which
 *     contains object identifier and list of BACSTAC_READ_RESULT_ITEM.
 *
 *   iAlloc
 *     A pointer to the allocator interface to be used to allocate an array
 *     of BACSTAC_READ_RESULT_ITEM and the property content of each item.
 *
 * Returned status:
 *
 *   BACSTAC_STATUS_OK
 *     The operation has been successful.
 *
 *   BACSTAC_STATUS_NOT_INITIALIZED
 *     BACstac was not initialized.
 *
 *   BACSTAC_STATUS_SERVICE_NOT_FOUND
 *     The BACstac service has been stopped or restarted.
 *
 *   BACSTAC_STATUS_INVALID_DEVICE_HANDLE
 *     Invalid device handle.
 *
 *   BACSTAC_STATUS_INVALID_PARAM.
 *     Some of parameters is incorrect:
 *     - pReadAccessSpec or pReadAccessResult is NULL;
 *     - iAlloc and pReadAccessResult->readResults are NULL;
 *     - iAlloc is NULL and some buffer in pReadAccessResult->readResults
 *       is not properly initialized;
 *     - flags has invalid bits.
 *
 *   BACSTAC_STATUS_DATATYPE_MISALIGNMENT
 *     One of the given buffer is not properly alignment.
 *
 *   BACSTAC_STATUS_OUT_OF_MEMORY
 *     Not enough memory to perform the operation.
 *
 *   BACSTAC_STATUS_UNKNOWN_ERROR
 *     Internal stack error.
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacReadLocalProperties (
    BACSTAC_HDEVICE hDevice,                          /* in, only for GTW */
    const BACSTAC_READ_ACCESS_SPEC *pReadAccessSpec,  /* in               */
    BACSTAC_READ_RAW_RESULT_LIST *pReadAccessResult,  /* out              */
    BACSTAC_IALLOC *iAlloc,                           /* in, optional     */
    BACSTAC_UINT flags                                /* in               */
) BACSTAC_NOTHROW;



/*
 * BACstacReadLocalProperty
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacReadLocalProperty (
    BACSTAC_HDEVICE hDevice,            /* in, only for GTW */
    const BACSTAC_READ_INFO *readInfo,  /* in               */
    BACSTAC_PROPERTY_CONTENTS *pValue,  /* out              */
    BACSTAC_ERROR_TYPE *pError,         /* out, optional    */
    BACSTAC_IALLOC *iAlloc,             /* in, optional     */
    BACSTAC_UINT flags                  /* in, for future   */
) BACSTAC_NOTHROW;



/*
 * BACstacWriteLocalProperties
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacWriteLocalProperties (
    BACSTAC_HDEVICE hDevice,                           /* in, only for GTW */
    const BACSTAC_WRITE_ACCESS_SPEC *pWriteAccessSpec, /* in               */
    BACSTAC_ERROR_TYPE *pError,                        /* out, optional    */
    BACSTAC_OBJ_PROP_REF *pFirstFailed,                /* out, optional    */
    const BACSTAC_ADDRESS *pSourceAddress,             /* in, optional     */
    BACSTAC_UINT flags                                 /* in               */
) BACSTAC_NOTHROW;



/*
 * BACstacWriteLocalProperty
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacWriteLocalProperty (
    BACSTAC_HDEVICE hDevice,                 /* in, only for GTW */
    const BACSTAC_WRITE_INFO *pWriteInfo,    /* in               */
    BACSTAC_ERROR_TYPE *pError,              /* out, optional    */
    const BACSTAC_ADDRESS *pSourceAddress,   /* in, optional     */
    BACSTAC_UINT flags                       /* in               */
) BACSTAC_NOTHROW;


/*
 * BACstacReadRangeLocal
 */

#ifdef BACSTAC_USE_64_BIT_READ_RANGE
   #define BACstacReadRangeLocal BACstacReadRange64Local
#else
   #define BACstacReadRangeLocal BACstacReadRange32Local
#endif

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacReadRange64Local (
    BACSTAC_HDEVICE hDevice,                         /* in, only for GTW */
    const BACSTAC_READ_RANGE_64_INFO *readRangeInfo, /* in               */
    BACSTAC_PROPERTY_CONTENTS *pValue,               /* out              */
    BACSTAC_BIT_STRING *pResultFlags,                /* out              */
    BACSTAC_ERROR_TYPE *pError,                      /* out, optional    */
    BACSTAC_IALLOC *iAlloc,                          /* in, optional     */
    BACSTAC_UINT flags                               /* in, for future   */
) BACSTAC_NOTHROW;

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacReadRange32Local (
    BACSTAC_HDEVICE hDevice,                         /* in, only for GTW */
    const BACSTAC_READ_RANGE_32_INFO *readRangeInfo, /* in               */
    BACSTAC_PROPERTY_CONTENTS *pValue,               /* out              */
    BACSTAC_BIT_STRING *pResultFlags,                /* out              */
    BACSTAC_ERROR_TYPE *pError,                      /* out, optional    */
    BACSTAC_IALLOC *iAlloc,                          /* in, optional     */
    BACSTAC_UINT flags                               /* in, for future   */
) BACSTAC_NOTHROW;



/*
 * BACstacAddElementLocal
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacAddElementLocal (
    BACSTAC_HDEVICE hDevice,                        /* in, only for GTW */
    const BACSTAC_CHANGE_LIST_INFO *changeListInfo, /* in               */
    BACSTAC_ERROR_TYPE *pError,                     /* out, optional    */
    BACSTAC_ELEMENT_COUNT *pFirstFailed,            /* out, optional    */
    const BACSTAC_ADDRESS *pSourceAddress,          /* in, optional     */
    BACSTAC_UINT flags                              /* in               */
) BACSTAC_NOTHROW;



/*
 * BACstacRemoveElementLocal
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacRemoveElementLocal (
    BACSTAC_HDEVICE hDevice,                        /* in, only for GTW */
    const BACSTAC_CHANGE_LIST_INFO *changeListInfo, /* in               */
    BACSTAC_ERROR_TYPE *pError,                     /* out, optional    */
    BACSTAC_ELEMENT_COUNT *pFirstFailed,            /* out, optional    */
    const BACSTAC_ADDRESS *pSourceAddress,          /* in, optional     */
    BACSTAC_UINT flags                              /* in               */
) BACSTAC_NOTHROW;



/*
 * Subscribe COV Property local
 */

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacChangeCovSubs (
    BACSTAC_HDEVICE hDevice,
    const BACSTAC_SUBSCRIBE_COVP_INFO *pSubscriber,
    const BACSTAC_ADDRESS *pRecipient
) BACSTAC_NOTHROW;


/*
 * BACstacGetLocalDeviceHandle
 */


BACSTACAPI BACSTAC_HDEVICE BACstac_api BACstacGetLocalDeviceHandle(void);


/*
 * BACstacGetLocalBACnetAddress
 */


BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacGetLocalBACnetAddress (
    BACSTAC_ADDRESS *pAddress
) BACSTAC_NOTHROW;

/*
 * BACstacGetLocalPortID returns the Port Identifier of the port to which the
 * application is bound.
 */

BACSTACAPI BACSTAC_UI8 BACstac_api BACstacGetLocalPortID(void) BACSTAC_NOTHROW;

/*
 * Property flags consists of four types and additional flags
 * that modifies the default BACstac behavior. See below:
 */
/* Each property can be one of the four types: */
#define BACSTAC_PF_SCALAR   0 /* scalar */
#define BACSTAC_PF_LIST     1 /* list */
#define BACSTAC_PF_FS_ARRAY 2 /* fixed size array */
#define BACSTAC_PF_VS_ARRAY 3 /* variable size array */

/* Each property can be either required or optional. If a property
 * is marked as required, it should present in all objects of the
 * given type. However, currently, BACstac does not enforce this
 * restriction and allows to create objects that does not have all
 * required properties. So, the only way in which it affects the
 * BACstac database is execution of ReadPropertyMultiple service
 * with PROP_REQUIRED and PROP_OPTIONAL in the access specification.
 *
 * If this flag is not set then the property is considered required.
 */
#define BACSTAC_PF_OPTIONAL 0x10

/* Conditional writable */
#define BACSTAC_PF_W_OOS  0x80 /* writable when out of service is true*/

typedef struct BACSTAC_PROPERTY_TYPE_INFO
{
    BACSTAC_PROPERTY_ID propertyID;
    BACSTAC_DATA_TYPE dataType;
    BACSTAC_ELEMENT_COUNT arraySize; /* number of elements */
    BACSTAC_VALUE_SIZE size; /* item size hint, 0 means default */
    BACSTAC_UINT flags;
} BACSTAC_PROPERTY_TYPE_INFO;

#define BACSTAC_PROPERTY_TYPE_INFO_INIT(propID, dataType, arraySize, \
                                        itemSizeHint, flags) \
    {(propID), (dataType), (arraySize), (itemSizeHint), (flags)}


typedef struct BACSTAC_OBJECT_TYPE_INFO
{
    BACSTAC_OBJECT_TYPE objectType;
    BACSTAC_PROPERTY_COUNT propCount;
    const BACSTAC_PROPERTY_TYPE_INFO *propTypeInfo;
    BACSTAC_UINT flags; /* reserved, must be 0 */
} BACSTAC_OBJECT_TYPE_INFO;

#define BACSTAC_OBJECT_TYPE_INFO_INIT(objType, propCount, propTypeInfo) \
    {(objType), (propCount), (propTypeInfo), 0}

typedef struct BACSTAC_DB_TYPE_INFO
{
    BACSTAC_OBJECT_COUNT objCount;
    const BACSTAC_OBJECT_TYPE_INFO *objTypeInfo;
} BACSTAC_DB_TYPE_INFO;

#define BACSTAC_DB_TYPE_INFO_INIT(objCount, objTypeInfo) \
    {(objCount), (objTypeInfo)}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* BACDB_H */
