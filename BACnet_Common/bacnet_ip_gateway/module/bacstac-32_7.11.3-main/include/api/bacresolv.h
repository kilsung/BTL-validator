/*
 * BACstac(TM) BACRESOLV.H
 *
 * Copyright (c) 2014-2023 by Cimetrics Inc. All rights reserved.
 *
 * Purpose: define BACstac API for DeviceID resolver
 *
 */

#ifndef BACRESOLV_H
#define BACRESOLV_H


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "bactypes.h"

/* Try to resolve the given DeviceID synchronously by sending Who-Is and
 * waiting for I-Am to arrive or the final timeout to expire. */
BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacResolveDeviceID(
    BACSTAC_INST_NUMBER deviceNumber,     /* in */
    BACSTAC_ADDRESS *deviceAddress,       /* out */
    BACSTAC_I_AM_INFO *iAmInfo            /* out, optional */
) BACSTAC_NOTHROW;

/* Obtain the device address and I-Am information from the resolver
 * cache. */
BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacGetDeviceAddressByID(
    BACSTAC_INST_NUMBER deviceNumber, /* in */
    BACSTAC_ADDRESS *deviceAddress,   /* out */
    BACSTAC_I_AM_INFO *iAmInfo        /* out, optional */
) BACSTAC_NOTHROW;

struct tagBACSTAC_RESOLVE_OPER; /* forward declaration */

/* The Asynchronous Completion Routine that is invoked when the address is
 * resolved or some error has happened. */
typedef void (BACstac_callback *BACSTAC_RESOLVE_ACR) (
    BACSTAC_STATUS status,
    BACSTAC_I_AM_INFO *iAmInfo,
    BACSTAC_ADDRESS *deviceAddress,
    struct tagBACSTAC_RESOLVE_OPER *oper
);

/* BACSTAC_RESOLVE_OPER_INTERNAL is for internal use only!!! */
typedef struct tagBACSTAC_RESOLVE_OPER_INTERNAL
{
    struct tagBACSTAC_RESOLVE_OPER *resolverInternalNext;
    struct tagBACSTAC_RESOLVE_OPER **resolverInternalPrev;
} BACSTAC_RESOLVE_OPER_INTERNAL;

/* The resolver area that is used by the ACR interface. This area contains
 * the ACR to be invoked and some implementation specific parameters. */
typedef struct tagBACSTAC_RESOLVE_OPER
{
    BACSTAC_RESOLVE_ACR acr;
    BACSTAC_RESOLVE_OPER_INTERNAL internal;
} BACSTAC_RESOLVE_OPER;

/* The access routine that initializes the resolver area to the given
 * asynchronous completion routine. */
BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacInitResolveOper(
    BACSTAC_RESOLVE_OPER *oper,
    BACSTAC_RESOLVE_ACR acr
) BACSTAC_NOTHROW;

/*
 * BACstacStartResolveOper - start an asynchronous resolver operation.
 *
 * NOTE: If operArea is NULL then the function works as Prefetch, i.e. it
 * tries to resolve the device instance number, but the result resides in
 * the cache and no callback is invoked.
 */
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacStartResolveOper(
    BACSTAC_INST_NUMBER deviceNumber,     /* in */
    BACSTAC_RESOLVE_OPER *operArea        /* in, optional */
) BACSTAC_NOTHROW;

/* BACstacCancelResolveOper - cancel an asynchronous resolver operation.
 *
 * If the operation is not running, then the function is successful, and it
 * returns TRUE and no callback will be invoked after that. If the callback
 * is running or has already been invoked then the function return FALSE.
 */
BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacCancelResolveOper(
    BACSTAC_RESOLVE_OPER *oper
) BACSTAC_NOTHROW;

/*
 * BACSTAC_WHO_IS_SENDER provides the interface to set a customer sender.
 *
 * The default implementation uses a global Who-Is broadcast and it always
 * returns STATUS_OK even if sending a global broadcast failed. It makes
 * sense to ignore the returned value, because normally the resolver will
 * retry again. So temporary lack of resources will not abort the operation.
 *
 * A custom Who-Is sender may return an error code if it wants to abort
 * the search operation without retry. Also, a custom sender may use
 * local broadcasts or remote broadcast for some range of device instances
 * if it is known that they are located on a specific network.
 */
typedef BACSTAC_STATUS (BACstac_callback *BACSTAC_WHO_IS_SENDER)(
    BACSTAC_INST_NUMBER /* deviceInstance */,
    void * /* userData */
    );


/* For gateways, Sets the Who-Is source address for default sender */
BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacSetResolverSrcAddress(
    const BACSTAC_ADDRESS *sourceAddress
) BACSTAC_NOTHROW;

/* Sets the Who-Is sender used by the resolver */
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacSetResolverWhoIsSender(
    BACSTAC_WHO_IS_SENDER sender,
    void *userData
) BACSTAC_NOTHROW;

/* Get Device Binding information
 *
 * The function retries device binding information currently stored
 * in the resolver cache. The resolver cache is updated asynchronously
 * by the default action of I-Am. Therefore, two consequent calls to
 * the cache can return different numbers of device binding elements.
 * The resolver cache does not intend to store information about all
 * discovered device on the BACnet internetwork but only those that are
 * currently used.
 *
 * Parameters:
 *
 * @buffer - buffer that receives the array of BACSTAC_ADDRESS_BINDING
 *           structures from the resolver cache.
 *
 * @size - size of the buffer in elements.
 *
 * @count - pointer to a variable that on return contains the actual
 *          number of elements stored in the buffer.
 *
 * If the buffer is null and size is 0 then the function returns OK and
 * the variable pointed by count contains the required size of the
 * buffer.
 *
 * If the buffer is null and size is not 0 then the function returns
 * INVALID_PARAM.
 *
 * If the buffer is not null and size is not sufficient to store all
 * elements then VAL_OUT_OF_SPACE is returned. On return, the variable
 * pointed by count contains the required size of the buffer.
 *
 * If the buffer is not null and size is sufficient to store all
 * elements then OK is returned. On return, the variable pointed by
 * count contains the number of elements copied to the buffer.
 */
BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacGetDeviceBinding (
    BACSTAC_ADDRESS_BINDING *buffer,  /* out, optional */
    BACSTAC_ELEMENT_COUNT size,       /* in */
    BACSTAC_ELEMENT_COUNT *count      /* out */
) BACSTAC_NOTHROW;

/* Get Device Binding information using an allocator
 *
 * The function retries device binding information currently stored
 * in the resolver cache and uses the given allocator to allocate
 * an array to store the binding information. The array should be
 * freed by application when it is no longer needed.
 *
 * Parameters:
 *
 * @binding - a pointer to variable that receives a pointer to an array
 *            of BACSTAC_ADDRESS_BINDING structures from the resolver
 *            cache. The array is allocated using the given allocator.
 *
 * @count - pointer to a variable that receives the number of elements
 *          in the binding array.
 *
 * @iAlloc - BACstac allocator interface.
 */
BACSTAC_CHECK_RETURN
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacGetDeviceBindingA (
    BACSTAC_ADDRESS_BINDING **binding, /* out */
    BACSTAC_ELEMENT_COUNT *count,      /* out */
    BACSTAC_IALLOC *iAlloc             /* in */
) BACSTAC_NOTHROW;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* BACRESOLV_H */
