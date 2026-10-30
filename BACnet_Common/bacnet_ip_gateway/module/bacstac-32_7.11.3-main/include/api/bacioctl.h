/*
 * BACstac(TM) BACIOCTL.H
 * Copyright (c) 2003-2023 by Cimetrics Inc. All rights reserved.
 *
 * Purpose: define IOCTL user interface
 *
 */

#ifndef BACIOCTL_H
#define BACIOCTL_H

#include "bactypes.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* baciocnm.h defines IOCTL request numbers and status codes */
#include "baciocnm.h"

/*
 * BACstacCreateIoctl creates a new ioctl request and
 * returns a handle to it
 *
 * This ioctl request can be then executed by BACstacIoctlExec()
 * routine after supplying all needed parameters by calling to
 * a corresponding BACstacIoPutXXX() routines.
 *
 * On error, BACSTAC_INVALID_HANDLE is returned.
 */
BACSTACAPI BACSTAC_HIOREQ BACstac_api BACstacCreateIoctl (void);

/*
 * BACstacIoReqDelete destroyes ioctl request pointed by hIoReq
 * handle and invalidate this handle itself. The handle must be
 * created before by BACstacCreateIoctl routine.
 * After call to BACstacIoReqDelete, the handle must not be used
 * or it can lead to unpredictable behaviour.
 */
BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacIoReqDelete (BACSTAC_HIOREQ hIoReq);

/*
 * BACstacIoReqClone creates a new ioctl request that is the same as
 * pointed by hIoReq handle.
 */
BACSTACAPI BACSTAC_HIOREQ BACstac_api BACstacIoReqClone (BACSTAC_HIOREQ hIoReq);


/*
 * BACstacIoReqCancel cancels ioctl request pointed by hIoReq handle.
 *
 * This routine returns:
 *
 */
/* Not implemeted
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacIoReqCancel (BACSTAC_HIOREQ hIoReq);
*/

/*
 * BACstacIoSetACR sets pointers to ACR and user data for specified
 * ioctl request. If pfACR parameter is not NULL, then this
 * ioctl request will be executed asynchronously, and specified
 * routine will be called at its completition.
 */

typedef void (BACstac_callback *BACSTAC_IOREQ_ACR_PROC)
    (BACSTAC_HIOREQ, const BACSTAC_ADDRESS *, BACSTAC_STATUS);

BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacIoSetACR (
	BACSTAC_HIOREQ hIoReq,
	BACSTAC_IOREQ_ACR_PROC pfACR,
	void *userData
	);

/*
 * The BACstacIoctlExec function executes local ioctl request
 *  hIoReq is a handle to the ioctl request. The BACstacCreateIoctl
 *  function returns this handle
 */
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacIoctlExec (BACSTAC_HIOREQ hIoReq);

/*
 * The BACstacIoctlExec function executes remote ioctl request
 * hIoReq is a handle to the ioctl request. The BACstacCreateIoctl
 * function returns this handle
 * srcAddress is a source address; for client and server program it must be
 * NULL, and for gateway application real local device address (non-NULL value)
 * destAddress is a destination address for the ioctl request
 * pAPDUParams is a pointer to APDU parameters; if it is NULL, then default
 * APDU parameters are used
 */
BACSTACAPI BACSTAC_STATUS BACstac_api BACstacIoctlRemoteExec(
   BACSTAC_HIOREQ                            hIoReq,
   const BACSTAC_ADDRESS                     *srcAddress,
   const BACSTAC_ADDRESS                     *destAddress,
   const BACSTAC_APDU_PROPERTIES             *pAPDUParams
   );

#define BACSTAC_NULL_TERM_STR BACSTAC_VALUE_SIZE_MAX

BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacIoPutBoolean (BACSTAC_HIOREQ hIoReq, BACSTAC_BOOLEAN val);
BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacIoPutUnsigned (BACSTAC_HIOREQ hIoReq, BACSTAC_UNSIGNED val);
BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacIoPutUInt64 (BACSTAC_HIOREQ hIoReq, BACSTAC_UI64 val);
BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacIoPutInteger (BACSTAC_HIOREQ hIoReq, BACSTAC_INTEGER val);
BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacIoPutAsciiStr (BACSTAC_HIOREQ hIoReq, BACSTAC_VALUE_SIZE len, const BACSTAC_CHAR *ptr);
BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacIoPutOctetStr (BACSTAC_HIOREQ hIoReq, BACSTAC_VALUE_SIZE len, const BACSTAC_BYTE *ptr);
BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacIoPutBitStr (BACSTAC_HIOREQ hIoReq, BACSTAC_VALUE_SIZE len, BACSTAC_BYTE unusedBitsNum, const BACSTAC_BYTE *ptr);
BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacIoPutEnum (BACSTAC_HIOREQ hIoReq, BACSTAC_ENUM val);

BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacIoGetBoolean (BACSTAC_HIOREQ hIoReq, BACSTAC_BOOLEAN *pVal);
BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacIoGetUnsigned (BACSTAC_HIOREQ hIoReq, BACSTAC_UNSIGNED *pVal);
BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacIoGetUInt64 (BACSTAC_HIOREQ hIoReq, BACSTAC_UI64 *pVal);
BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacIoGetInteger (BACSTAC_HIOREQ hIoReq, BACSTAC_INTEGER *pVal);
BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacIoGetAsciiStr (BACSTAC_HIOREQ hIoReq, BACSTAC_VALUE_SIZE *pLen, const BACSTAC_CHAR **ptr);
BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacIoGetOctetStr (BACSTAC_HIOREQ hIoReq, BACSTAC_VALUE_SIZE *pLen, const BACSTAC_BYTE **ptr);
BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacIoGetBitStr (BACSTAC_HIOREQ hIoReq, BACSTAC_VALUE_SIZE *pLen, BACSTAC_BYTE *pUnusedBitsNum, const BACSTAC_BYTE **ptr);
BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacIoGetEnum (BACSTAC_HIOREQ hIoReq, BACSTAC_ENUM *val);

BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacIoGetStatus (BACSTAC_HIOREQ hIoReq, BACSTAC_STATUS *pStatus);
BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacIoGetErrorType (BACSTAC_HIOREQ hIoReq, BACSTAC_ERROR_TYPE *pError);
BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacIoGetRejectReason (BACSTAC_HIOREQ hIoReq, BACSTAC_REJECT_REASON *pVal);
BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacIoGetAbortReason (BACSTAC_HIOREQ hIoReq, BACSTAC_ABORT_REASON *pVal);
BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacIoGetFailureReason (BACSTAC_HIOREQ hIoReq, BACSTAC_ERROR *pError);

BACSTACAPI void *BACstac_api BACstacIoGetUserData (BACSTAC_HIOREQ hIoReq);


BACSTAC_DEPRECATED /* for backward compatibility only */
BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacIoPutSigned (BACSTAC_HIOREQ hIoReq, BACSTAC_INTEGER val);
BACSTAC_DEPRECATED /* for backward compatibility only */
BACSTACAPI BACSTAC_BOOLEAN BACstac_api BACstacIoGetSigned (BACSTAC_HIOREQ hIoReq, BACSTAC_INTEGER *pVal);

/*
 * BACstacIoctlFilter
 */

typedef enum BACSTAC_IOCTL_FILTER_STATUS
{
    FILTER_STATUS_ACCEPT,
    FILTER_STATUS_REJECT,
    FILTER_STATUS_IGNORE
    BACSTAC_OTHER_ENUM(IOCTL_FILTER_STATUS)
} BACSTAC_IOCTL_FILTER_STATUS;

typedef BACSTAC_IOCTL_FILTER_STATUS (
   BACstac_callback *BACSTAC_IOCTL_FILTER_PROC
   ) (
      BACSTAC_ADDRESS *pSourceAddress,
      BACSTAC_IOCTL_REQUESTID requestID,
      BACSTAC_HIOREQ hIoReq                 /* now BACSTAC_INVALID_HANDLE */
);

BACSTACAPI BACSTAC_STATUS BACstac_api BACstacSetIoctlAccessFilter (
    BACSTAC_IOCTL_FILTER_PROC pfIoctlFiterProc
);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* BACIOCTL_H */
