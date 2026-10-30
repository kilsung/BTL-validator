/*
 * BACstac(TM) BACCOMM.H
 * Copyright (c) 1996-2026 by Cimetrics Inc. All rights reserved.
 *
 * Purpose: define BACstac Common API routines, types and constants
 *
 */

#ifndef BACCOMM_H
#define BACCOMM_H

#include "bactypes.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*
 * BACstacDecodePropertyValue
 *
 * This function decodes an ASN.1 value of any property. The function relies
 * on objectType, propertyID, and arrayIndex to determine the desired type.
 * On success, cValue will contain the decoded C value. The 'tag' field of
 * cValue indicates the data type, nElements contains the number of elements
 * of that type, and the buffer field contains the decoded value. If iAlloc
 * is NULL then the callee should initialize the 'buffer' field in cValue to
 * a buffer that has sufficient space to contain the decoded value. If the
 * the allocator is provided (iAlloc != NULL) then the decoder will allocate
 * the buffer of the required size using this interface. The callee should
 * free the allocated buffer later using the BACstacFreeContents() routine.
 */
BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacDecodePropertyValue (
  BACSTAC_OBJECT_TYPE                   objectType,
  BACSTAC_PROPERTY_ID                   propertyID,
  BACSTAC_ARRAY_INDEX                   arrayIndex,
  const BACSTAC_BYTE                    *asnValue,
  BACSTAC_VALUE_SIZE                    asnLength,
  BACSTAC_PROPERTY_CONTENTS             *cValue,
  BACSTAC_IALLOC                        *iAlloc
) BACSTAC_NOTHROW;


/*
 * BACstacDecodeAsnValue decodes the given ASN.1 value as the specified
 * BACstac data type.
 *
 * Parameters:
 *
 * @asnValue is a pointer to the ASN.1 value to decode
 *
 * @asnLength is the length of the ASN.1 value
 *
 * @cValue is a pointer to the property contents, which must be properly
 *         initialized. The 'tag' field must specify the BACstac data type
 *         for the decoded value. If the decoded value is a scalar then the
 *         'nElements' field must 1. If the decoded value is an array or list
 *         then on input, this field must the maximum number of elements to
 *         decode, and, on successful return, it contains actually decoded
 *         items (which does not exceed the specified maximum). The 'buffer'
 *         field will contain the decoded value on successful return. If the
 *         function is invoked with iAlloc = NULL, then the buffer must be
 *         initialized to a variable or array that will contain the result.
 *         If iAlloc is not NULL then the buffer of the required size will
 *         be allocated by the function, which must be freed later by the
 *         callee using the BACstacFreeContents() function.
 *
 * @decodedLength is a pointer to the variable that on successful completion
 *         contains the actually decoded ASN.1 length. This allows to decode
 *         only the beginning of the ASN.1 value as the specified data type
 *         without decoding the rest. If this pointer is NULL then either
 *         the whole value is decoded or BACSTAC_STATUS_INVALID_DATA_TYPE is
 *         returned.
 *
 * @iAlloc is a pointer to the allocator interface to be used to allocate the
 *         buffer to the decoded value. If this pointer is NULL then the buffer
 *         in the property contents must be initialized to content
 *
 * NOTE: Normally, BACstacInitContentsToVar or BACstacInitContentsToArray should
 * be used to initialize the property contents. If the buffer was allocated by
 * the decoder (i.e. when iAlloc != NULL), the callee should free the allocated
 * buffer when it is no longer necessary using using the BACstacFreeContents()
 * routine.
 */

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacDecodeAsnValue (
   const BACSTAC_BYTE *asnValue,
   BACSTAC_VALUE_SIZE asnLength,
   BACSTAC_PROPERTY_CONTENTS *cValue,
   BACSTAC_VALUE_SIZE *decodedLength, /* optional */
   BACSTAC_IALLOC *iAlloc /* optional */
) BACSTAC_NOTHROW;

/*
 * BACstacParseAsnList parses the given ASN.1 value as the specified
 * BACstac data type.
 *
 * Return value: number of successfully parsed list elements
 *
 * Parameters:
 *
 * @asnValue is a pointer to the ASN.1 value to decode
 *
 * @asnLength is the length of the ASN.1 value
 *
 * @parsedLength is a pointer to the variable that on completion
 *         contains the actually parsed ASN.1 length.
 */

BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_ELEMENT_COUNT BACstac_api BACstacParseAsnList (
   const BACSTAC_BYTE *asnValue,
   BACSTAC_VALUE_SIZE asnLength,
   BACSTAC_DATA_TYPE type,
   BACSTAC_VALUE_SIZE *parsedLength /* optional */
) BACSTAC_NOTHROW;

/*
 * BACstacEncodePropertyContents
 *
 * This function encodes any Property Contents to ASN.
 * The application must provide buffer with enough space
 * to store ASN in it. On success, the variable pointed
 * by actualBACnetLength will contain actual length of
 * the property contents in ASN. If the buffer does not
 * have enough space then BACSTAC_STATUS_VAL_OUT_OF_SPACE
 * is returned. If the function fails, the context of the
 * supplied buffer and the value of the variable pointed
 * by the actualBACnetLength parameter is undefined.
 */

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacEncodePropertyContents (
   const BACSTAC_PROPERTY_CONTENTS *cValue,
   BACSTAC_BYTE *bACnetValue,
   BACSTAC_VALUE_SIZE maxBACnetLength,
   BACSTAC_VALUE_SIZE *actualBACnetLength /* optional */
) BACSTAC_NOTHROW;

/*
 * BACstacProcess
 */


BACSTACAPI BACSTAC_STATUS BACstac_api BACstacProcess (void) BACSTAC_NOTHROW;


/*
 * BACstacClose
 */


BACSTACAPI void BACstac_api BACstacClose (void) BACSTAC_NOTHROW;


/*
 * BACstacGetDeviceList
 */


#define Query_Dev_List_Length(p)            BACstacGetDeviceList(NULL,0,p)
#define Devices_Copied(buff_len,available)  \
    (((buff_len) < (available)) ? (buff_len) : (available))

BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacGetDeviceList (
    BACSTAC_HDEVICE *deviceHandles,
    BACSTAC_DEVICE_COUNT deviceHandlesCount,
    BACSTAC_DEVICE_COUNT *deviceHandlesAvailable
) BACSTAC_NOTHROW;


/*
 * BACstacGetObjectList
 */


BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacGetObjectList (
    BACSTAC_HOBJECT *pObjectListBuffer,
    BACSTAC_OBJECT_COUNT nObjectListBufferCount,
    const BACSTAC_WHO_HAS_PARAM *pObjectSpec, /* or NULL */
    BACSTAC_OBJECT_COUNT *pObjectsAvailable
) BACSTAC_NOTHROW;


/*
 * BACstacSelectObjects
 */


BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacSelectObjects (
    BACSTAC_HOBJECT *pObjectListBuffer,
    BACSTAC_OBJECT_COUNT nObjectListBufferCount,
    const BACSTAC_WHO_HAS_HOOK_INFO *pObjectSpec, /* or NULL */
    BACSTAC_OBJECT_COUNT *pObjectsAvailable
) BACSTAC_NOTHROW;



/*
 * Set/query APDU parameters
 */


BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacSetAPDURetry (BACSTAC_UINT nCount) BACSTAC_NOTHROW;
BACSTACAPI BACSTAC_UINT BACstac_api BACstacGetAPDURetry (void) BACSTAC_NOTHROW;

BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacSetAPDUTimeout (BACSTAC_UINT nMillisec) BACSTAC_NOTHROW;
BACSTACAPI BACSTAC_UINT BACstac_api BACstacGetAPDUTimeout (void) BACSTAC_NOTHROW;

BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacSetMaxAPDULength (BACSTAC_UINT nBytes) BACSTAC_NOTHROW;
BACSTACAPI BACSTAC_UINT BACstac_api BACstacGetMaxAPDULength (void) BACSTAC_NOTHROW;

BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacSetDefaultAPDUProperties (
    const BACSTAC_APDU_PROPERTIES *pAPDUProps
) BACSTAC_NOTHROW;
BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacGetDefaultAPDUProperties (
    BACSTAC_APDU_PROPERTIES *pAPDUProps
) BACSTAC_NOTHROW;



/*
 * BACstacLockData
 */


BACSTACAPI void BACstac_api BACstacLockData (void) BACSTAC_NOTHROW;


/*
 * BACstacUnlockData
 */


BACSTACAPI void BACstac_api BACstacUnlockData (void) BACSTAC_NOTHROW;


/*
 * BACstacGetDeviceObjectHandle
 */


BACSTACAPI BACSTAC_HOBJECT BACstac_api BACstacGetDeviceObjectHandle (
    BACSTAC_HDEVICE hDevice
) BACSTAC_NOTHROW;

#define BACstacGetDeviceObjHandle(h)    BACstacGetDeviceObjectHandle (h)


/*
 * BACstacDeviceInstNumToHandle
 */


BACSTACAPI BACSTAC_HDEVICE BACstac_api BACstacDeviceInstNumToHandle (
    BACSTAC_INST_NUMBER instNumber
) BACSTAC_NOTHROW;


/*
 * BACstacDeviceAddressToHandle
 */


BACSTACAPI BACSTAC_HDEVICE BACstac_api BACstacDeviceAddressToHandle (
    const BACSTAC_ADDRESS *address
) BACSTAC_NOTHROW;


/*
 * BACstacIsDeviceLocal
 */


BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacIsDeviceLocal (
    BACSTAC_HDEVICE hDevice
) BACSTAC_NOTHROW;


/*
 * BACstacGetDeviceInstNum
 */


BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacGetDeviceInstNum (
    BACSTAC_HDEVICE hDevice,
    BACSTAC_INST_NUMBER *pInstNumber
) BACSTAC_NOTHROW;


/*
 * BACstacGetDeviceAddress
 */


BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacGetDeviceAddress (
    BACSTAC_HDEVICE hDevice,
    BACSTAC_ADDRESS *address
) BACSTAC_NOTHROW;


/*
 * BACstacGetObjectDeviceHandle
 */


BACSTACAPI BACSTAC_HDEVICE BACstac_api BACstacGetObjectDeviceHandle (
    BACSTAC_HOBJECT hObject
) BACSTAC_NOTHROW;

#define BACstacFindDevice(h)    BACstacGetObjectDeviceHandle (h)


/*
 * BACstacFindObjectByID
 */

BACSTACAPI BACSTAC_HOBJECT BACstac_api BACstacFindObjectByID (
    BACSTAC_HDEVICE hDevice,
    BACSTAC_OBJECT_ID objectID
) BACSTAC_NOTHROW;

/* for backward compatibility */
#define BACstacObjectIDToHandle(dev, pobj) \
	BACstacFindObjectByID((dev), *(pobj))

/*
 * BACstacFindObjectByName
 */

BACSTACAPI BACSTAC_HOBJECT BACstac_api BACstacFindObjectByName (
    BACSTAC_HDEVICE hDevice,
    const BACSTAC_CHAR_STRING *objectName
) BACSTAC_NOTHROW;


/*
 * BACstacObjectNameToHandle
 *
 * NOTE: for UTF-8 strings only!
 */

BACSTACAPI BACSTAC_HOBJECT BACstac_api BACstacObjectNameToHandle (
    BACSTAC_HDEVICE hDevice,
    const BACSTAC_CHAR *objectName
) BACSTAC_NOTHROW;


/*
 * BACstacIsObjectLocal
 */


#define BACstacIsObjectLocal(h)     \
    BACstacIsDeviceLocal (BACstacGetObjectDeviceHandle (h))


/*
 * BACstacGetObjectID
 */

BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacGetObjectID (
    BACSTAC_HOBJECT hObject,
    BACSTAC_OBJECT_ID *pObjectID
) BACSTAC_NOTHROW;


/*
 * BACstacGetObjectInstNum
 */


BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacGetObjectInstNum (
    BACSTAC_HOBJECT hObject,
    BACSTAC_INST_NUMBER *pInstNumber
) BACSTAC_NOTHROW;


/*
 * BACstacGetObjectType
 */


BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacGetObjectType (
    BACSTAC_HOBJECT hObject,
    BACSTAC_OBJECT_TYPE *type
) BACSTAC_NOTHROW;


/*
 * BACstacSetObjectUserPtr
 */


BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacSetObjectUserPtr (
    BACSTAC_HOBJECT hObject,
    void *userPtr
) BACSTAC_NOTHROW;


/*
 * BACstacGetObjectUserPtr
 */


BACSTACAPI void * BACstac_api BACstacGetObjectUserPtr (
    BACSTAC_HOBJECT hObject
) BACSTAC_NOTHROW;


/*
 * Handle validation
 */

BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacIsDeviceHandleValid (
    BACSTAC_HDEVICE h
) BACSTAC_NOTHROW;

BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacIsObjectHandleValid (
    BACSTAC_HOBJECT h
) BACSTAC_NOTHROW;


/*
 * Receiving a property data type
 */

#define BACSTAC_PROP_ARRAY     0x01
#define BACSTAC_PROP_LIST      0x02
#define BACSTAC_PROP_OPTIONAL  0x04
#define BACSTAC_PROP_ARRAY_OF_LISTS 0x03

BACSTACAPI BACSTAC_DATA_TYPE BACstac_api BACstacGetPropertyDataType (
    BACSTAC_OBJECT_TYPE objectType, /* in */
    BACSTAC_PROPERTY_ID propertyID, /* in */
    BACSTAC_UINT        *flags      /* out | NULL */
) BACSTAC_NOTHROW;

BACSTAC_DEPRECATED /* for backward compatibility only */
BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacGetPropertyInfo (
    BACSTAC_OBJECT_TYPE objectType, /* in */
    BACSTAC_PROPERTY_ID propertyID, /* in */
    BACSTAC_DATA_TYPE   *dataType,  /* out | NULL */
    BACSTAC_UINT        *flags      /* out | NULL */
) BACSTAC_NOTHROW;




/*
 * Support for floating-point special cases: INF, -INF and NaN
 */


/* Determines floating-point class to which the given value belongs */
BACSTACAPI BACSTAC_FP_CLASS BACstac_api BACstacFPClass (
    double value
) BACSTAC_NOTHROW;

#ifdef BACSTAC_GENERIC
#define BACstacGenerateSpecFPValue(fpClass, pValue) BACSTAC_GENERIC(*(pValue), \
		float : BACstacGenerateSpecFPValueFloat, \
		double : BACstacGenerateSpecFPValueDouble) \
		((fpClass), (pValue))
#else
/* Returns floating-point value of the specified class. FP_CLASS_NORMAL
   is not allowed on the input. */
BACSTAC_BOOLEAN BACstacGenerateSpecFPValueUnsupported (void);

#define BACstacGenerateSpecFPValue(fpClass, pValue) ( \
    (sizeof(*pValue) == sizeof(float)) ? BACstacGenerateSpecFPValueFloat((fpClass), (void*)(pValue)) : \
    (sizeof(*pValue) == sizeof(double)) ? BACstacGenerateSpecFPValueDouble((fpClass), (void*)(pValue)) : \
    BACstacGenerateSpecFPValueUnsupported() /* generate error */ )
#endif

BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacGenerateSpecFPValueFloat (
    BACSTAC_FP_CLASS fpClass,
    float *pValue
) BACSTAC_NOTHROW;

BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacGenerateSpecFPValueDouble (
    BACSTAC_FP_CLASS fpClass,
    double *pValue
) BACSTAC_NOTHROW;

BACSTAC_STATUS BACstac_api BACstacRestartCore(
    BACSTAC_UINT flags /* reserved for future */
) BACSTAC_NOTHROW;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* BACCOMM_H */
