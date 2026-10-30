/*
 * BACstac(TM) BACTYPES.H
 * Copyright (c) 1996-2025 by Cimetrics Inc. All rights reserved.
 *
 * Purpose: define BACstac constants, enumerations,
 *          data types and data access routines
 *
 */


#ifndef BACTYPES_H
#define BACTYPES_H

#include <stddef.h>     /* for NULL */

#include <bnetdef.h>
#include <bneterr.h>
#include <bacbase.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/*
 * General Declarations
 */


struct tagBACSTAC_HDEVICE; /* internal BACstac structure */
struct tagBACSTAC_HOBJECT; /* internal BACstac structure */
struct tagBACSTAC_HTSM;    /* internal BACstac structure */
struct tagBACSTAC_IOREQ;   /* internal BACstac structure */

/* Define BACstac handles */
typedef struct tagBACSTAC_HDEVICE *BACSTAC_HDEVICE;
typedef struct tagBACSTAC_HOBJECT *BACSTAC_HOBJECT;
typedef struct tagBACSTAC_HTSM *BACSTAC_HTSM;
typedef struct tagBACSTAC_IOREQ *BACSTAC_HIOREQ;
#define BACSTAC_INVALID_HANDLE      0

#define BACSTAC_FILTER_MIN          BACSTAC_INST_NUMBER_MIN
#define BACSTAC_FILTER_MAX          BACSTAC_INST_NUMBER_MAX
#define BACSTAC_NO_FILTER           ((BACSTAC_INST_NUMBER) ~0)

#define BACSTAC_ENTIRE_ARRAY        BACSTAC_VOID_INDEX
#define BACSTAC_ARRAY_COUNT         ((BACSTAC_ARRAY_INDEX) 0)

/*
 * Support for floating-point special cases: INF, -INF and NaN
 */
typedef enum BACSTAC_FP_CLASS
{
    FP_CLASS_INF_POS,
    FP_CLASS_INF_NEG,
    FP_CLASS_NAN,
    FP_CLASS_NORMAL
    BACSTAC_OTHER_ENUM(FP_CLASS)
} BACSTAC_FP_CLASS;


typedef BACSTAC_VALUE_SIZE     *PBACSTAC_VALUE_SIZE;
typedef BACSTAC_DEVICE_COUNT   *PBACSTAC_DEVICE_COUNT;
typedef BACSTAC_PROPERTY_COUNT *PBACSTAC_PROPERTY_COUNT;
typedef BACSTAC_ELEMENT_COUNT  *PBACSTAC_ELEMENT_COUNT;
typedef BACSTAC_OBJECT_COUNT   *PBACSTAC_OBJECT_COUNT;

typedef BACSTAC_PRIORITY_LEVEL *PBACSTAC_PRIORITY_LEVEL;

#define BACSTAC_PRI_MANUAL_LIFE_SAFETY       1
#define BACSTAC_PRI_AUTO_LIFE_SAFETY         2
#define BACSTAC_PRI_AVAILABLE_3              3
#define BACSTAC_PRI_AVAILABLE_4              4
#define BACSTAC_PRI_CRITICAL_EQUIP_CTRL      5
#define BACSTAC_PRI_MINIMUM_ON_OFF           6
#define BACSTAC_PRI_AVAILABLE_7              7
#define BACSTAC_PRI_MANUAL_OPERATOR          8
#define BACSTAC_PRI_AVAILABLE_9              9
#define BACSTAC_PRI_AVAILABLE_10             10
#define BACSTAC_PRI_AVAILABLE_11             11
#define BACSTAC_PRI_AVAILABLE_12             12
#define BACSTAC_PRI_AVAILABLE_13             13
#define BACSTAC_PRI_AVAILABLE_14             14
#define BACSTAC_PRI_AVAILABLE_15             15
#define BACSTAC_PRI_AVAILABLE_16             16

#define BACSTAC_DEFAULT_PRI      BACSTAC_PRI_AVAILABLE_16
#define BACSTAC_VOID_PRI         ((BACSTAC_PRIORITY_LEVEL)(-1))


/*
 * Status codes
 */


typedef enum BACSTAC_STATUS
{
    BACSTAC_STATUS_OK =                 0x00,
    BACSTAC_STATUS_UNKNOWN_ERROR =      0x01,
    BACSTAC_STATUS_SERVICE_NOT_FOUND =  0x02,
    BACSTAC_STATUS_NOT_INITIALIZED =    0x03,

    /*
     * BACSTAC_STATUS_NOT_SUPPORTED is removed in BACstac 6.5. This status was
     * used in different meanings, which could lead to confusion in some cases.
     * Therefore it is been replaced with following status codes:
     *
     * BACSTAC_STATUS_WRITE_ACCESS_DENIED - returned by BACstacStorePropertyInstance
     *     and other functions when an attempt is made to change a read-only property.
     *
     * BACSTAC_STATUS_UNSUPPORTED_OBJ_TYPE - returned by BACstacCreateDBObject when
     *     the given object type is unknown. Potentially, BACstacCreateDBObject can
     *     return also BACSTAC_STATUS_WRITE_ACCESS_DENIED if an attempt is made to
     *     initialize an automatic property that does not support initialization.
     *
     * BACSTAC_STATUS_UNSUPPORTED_SERVICE - returned by BACstacSetHook if the given
     *     service is not supported in the current version.
     */

    BACSTAC_STATUS_REJECT =             0x05,
    BACSTAC_STATUS_DCC_DISABLED =       0x06,
    BACSTAC_STATUS_SERVICE_TIMEOUT =    0x07, /* deprecated in BACstac 4.0 */
    BACSTAC_STATUS_INVALID_DEVICE_HANDLE = 0x08,
    BACSTAC_STATUS_INVALID_OBJECT_HANDLE = 0x09,
    BACSTAC_STATUS_UNKNOWN_OBJECT =     0x0a,
    BACSTAC_STATUS_UNKNOWN_PROPERTY =   0x0b,
    BACSTAC_STATUS_NOT_A_LIST =         0x0c,
    BACSTAC_STATUS_NOT_AN_ARRAY =       0x0d,
    BACSTAC_STATUS_INVALID_ARRAY_INDEX= 0x0e,
    BACSTAC_STATUS_INVALID_PFLAGS =     0x0f,
    BACSTAC_STATUS_INVALID_PARAM =      0x10,
    BACSTAC_STATUS_BUSY =               0x11,
    BACSTAC_STATUS_ALREADY_EXISTS =     0x12,
    BACSTAC_STATUS_BAD_CONFIG =         0x13,
    BACSTAC_STATUS_DATATYPE_MISALIGNMENT = 0x14,
    BACSTAC_STATUS_NOT_COV_PROPERTY =   0x15,
    BACSTAC_STATUS_COV_SUBS_FAILED =    0x16,
    BACSTAC_STATUS_CHARSET_NOT_SUPPORTED = 0x17,
    BACSTAC_STATUS_DUPLICATE_OBJECT_ID = 0x18,
    BACSTAC_STATUS_DUPLICATE_NAME =     0x19,
    BACSTAC_STATUS_DUPLICATE_PROPERTY_ID = 0x1a,
    BACSTAC_STATUS_ADDRESS_IN_USE =     0x1b,
    BACSTAC_STATUS_INVALID_BUFFER =     0x1c,
    BACSTAC_STATUS_MISSING_PROPERTY =   0x1d,
    BACSTAC_STATUS_MISSING_DEV_OBJECT = 0x1e,
    BACSTAC_STATUS_DUPLICATE_DEV_OBJECT = 0x1f,
    BACSTAC_STATUS_OUT_OF_MEMORY =      0x20,
    BACSTAC_STATUS_OUT_OF_RESOURCES =   0x21,
    BACSTAC_STATUS_XMT_OVERFLOW =       0x22,
    BACSTAC_STATUS_RCV_OVERFLOW =       0x23,
    BACSTAC_STATUS_STACK_OVERFLOW =     0x24,
    BACSTAC_STATUS_INVALID_ADDRESS =    0x25,
    BACSTAC_STATUS_INVALID_DB_TYPE_INFO = 0x26,
    BACSTAC_STATUS_WRITE_ACCESS_DENIED =  0x27,
    BACSTAC_STATUS_UNSUPPORTED_OBJ_TYPE = 0x28,
    BACSTAC_STATUS_UNSUPPORTED_SERVICE =  0x29,
    BACSTAC_STATUS_TIMEOUT =            0x30,
    BACSTAC_STATUS_REQUEST_TOO_LONG =   0x31,
    BACSTAC_STATUS_ILLEGAL_RESPONSE =   0x32,
    BACSTAC_STATUS_RESPONSE_TOO_LONG =  0x33,
    BACSTAC_STATUS_INVALID_DATA_TYPE =  0x34,
    BACSTAC_STATUS_VAL_OUT_OF_RANGE =   0x35,
    BACSTAC_STATUS_VAL_OUT_OF_SPACE =   0x36,
    BACSTAC_STATUS_SEG_NOT_SUPPORTED =  0x37,
    BACSTAC_STATUS_TRANSACTION_ABORTED =0x38,
    BACSTAC_STATUS_BACNET_ERROR =       0x40,
    BACSTAC_STATUS_BACNET_REJECT =      0x41,
    BACSTAC_STATUS_BACNET_ABORT =       0x42,
    BACSTAC_STATUS_INVALID_PACKET =     0x43,
    BACSTAC_STATUS_SEG_ERROR =          0x44,
    BACSTAC_STATUS_INCONSISTENT_TAGS =  0x45,
    BACSTAC_STATUS_MISSING_REQUIRED_PARAM = 0x46,
    BACSTAC_STATUS_CANNOT_BIND_TO_PORT =0x49,
    BACSTAC_STATUS_FD_NOT_REGISTERED =  0x4A,
    BACSTAC_STATUS_DRV_NOT_FOUND =      0x50,
    BACSTAC_STATUS_DRV_NOT_SUPPORTED =  0x51,
    BACSTAC_STATUS_DRV_CANT_INIT =      0x52,
    BACSTAC_STATUS_DRV_CANT_SEND =      0x53,
    BACSTAC_STATUS_DNET_UNREACHABLE =   0x54,
    BACSTAC_STATUS_INCONSISTENT_VERSIONS =  0x55,
    BACSTAC_STATUS_DEVICE_UNREACHABLE = 0x56,
    BACSTAC_STATUS_DNET_IS_LOCAL =      0x57,
    BACSTAC_STATUS_DNET_BUSY =          0x58,
    BACSTAC_STATUS_WIN_SIZE_OUT_OF_RANGE = 0x59,
    BACSTAC_STATUS_SUBS_NOT_FOUND =     0x5A,
    BACSTAC_STATUS_OPTIONAL_FUNC_NOT_SUPP = 0x5B,
    BACSTAC_STATUS_DUPLICATE_ENTRY =    0x5C,
    BACSTAC_STATUS_NO_SPACE_FOR_ELEMENT = 0x5D,
    BACSTAC_STATUS_NO_SPACE_FOR_OBJECT = 0x5E,
    BACSTAC_STATUS_NO_SPACE_FOR_DEVICE = 0x5F,
    BACSTAC_STATUS_DYN_CREATION_NOT_SUPP = 0x60,
    BACSTAC_STATUS_READ_ACCESS_DENIED = 0x61,
    BACSTAC_STATUS_RESTART_REQUIRED = 0x62,
    BACSTAC_STATUS_INVALID_OPERATION = 0x63,
    BACSTAC_STATUS_INVALID_DATA_ENCODING = 0x64,
    BACSTAC_STATUS_INVALID_ARRAY_SIZE = 0x65,
    BACSTAC_STATUS_INVALID_DATA_OPTS = 0x66,
    BACSTAC_STATUS_INCONSISTENT_ATTRIBUTES = 0x67
    BACSTAC_OTHER_ENUM(STATUS)
} BACSTAC_STATUS;


/*
 * Error description
 */


typedef enum BACSTAC_FAILURE_TYPE
{
    FAILURE_ABORT,
    FAILURE_REJECT,
    FAILURE_ERROR
    BACSTAC_OTHER_ENUM(FAILURE_TYPE)
} BACSTAC_FAILURE_TYPE;

typedef struct tagBACSTAC_ERROR_TYPE
{
    BACSTAC_ERROR_CLASS errClass;
    BACSTAC_ERROR_CODE  errCode;
} BACSTAC_ERROR_TYPE, *PBACSTAC_ERROR_TYPE;

typedef struct tagBACSTAC_ERROR
{
    BACSTAC_FAILURE_TYPE    tag;
    union {
        BACSTAC_ABORT_REASON    abortReason; /*= tag:FAILURE_ABORT */
        BACSTAC_REJECT_REASON   rejectReason; /*= tag:FAILURE_REJECT */
        BACSTAC_ERROR_TYPE      errorSpec; /*= tag:FAILURE_ERROR */
    } failure;
} BACSTAC_ERROR, *PBACSTAC_ERROR;



/*
 * Hook declarations
 */


typedef enum BACSTAC_HOOK_STATUS
{
    HOOK_STATUS_OK,         /* Indicates success. */
    HOOK_STATUS_DEFAULT     /* Indicates that the BACstac shall take the default action as
                               the hook were not executed at all. */
    BACSTAC_OTHER_ENUM(HOOK_STATUS)
} BACSTAC_HOOK_STATUS;

typedef enum BACSTAC_HOOK_TYPE
{
    HOOK_WHO_IS,
    HOOK_WHO_HAS,
    HOOK_READ_PROP,
    HOOK_WRITE_PROP,
    HOOK_TIME_SYNC,
    HOOK_ADD_ELEMENT,
    HOOK_REMOVE_ELEMENT,
    HOOK_REINIT_DEV,
    HOOK_I_AM,
    HOOK_I_HAVE,
    HOOK_SUBSCRIBE_COV,
    HOOK_CONF_COV_NOTIFICATION,
    HOOK_UNCONF_COV_NOTIFICATION,
    HOOK_ACK_ALARM,
    HOOK_GET_ALARM_SUMMARY,
    HOOK_GET_ENROLLMENT_SUMMARY,
    HOOK_READ_PROP_MULTIPLE,
    HOOK_WRITE_PROP_MULTIPLE,
    HOOK_DCC,
    HOOK_READ_RANGE_32,
    HOOK_READ_RANGE_64,
    HOOK_READ_FILE,
    HOOK_WRITE_FILE,
    HOOK_CONF_PRIVATE_TRANSFER,
    HOOK_UNCONF_PRIVATE_TRANSFER,
    HOOK_CONF_TEXT_MESSAGE,
    HOOK_UNCONF_TEXT_MESSAGE,
    HOOK_CREATE_OBJECT,
    HOOK_DELETE_OBJECT,
    HOOK_UTC_TIME_SYNC,
    HOOK_READ_PROP_CONDITIONAL,
    HOOK_SUBSCRIBE_COV_PROP,
    HOOK_GET_EVENT_INFO,
    HOOK_LIFE_SAFETY_OPERATION,
    HOOK_CONF_EVENT_NOTIF,
    HOOK_UNCONF_EVENT_NOTIF,
    HOOK_SUBS_COVP_MULTI,
    HOOK_CONF_COV_NOTIF_MULTI,
    HOOK_UNCONF_COV_NOTIF_MULTI,
    HOOK_WRITE_GROUP,
    HOOK_CONF_AUDIT_NOTIF,
    HOOK_CONF_AUDIT_NOTIF_RAW,
    HOOK_UNCONF_AUDIT_NOTIF,
    HOOK_UNCONF_AUDIT_NOTIF_RAW,
    HOOK_AUDIT_LOG_QUERY,
    HOOK_WHO_AM_I,
    HOOK_YOU_ARE
    BACSTAC_OTHER_ENUM(HOOK_TYPE)
} BACSTAC_HOOK_TYPE;

#ifdef BACSTAC_USE_64_BIT_READ_RANGE
#define HOOK_READ_RANGE      HOOK_READ_RANGE_64
#else
#define HOOK_READ_RANGE      HOOK_READ_RANGE_32
#endif


typedef enum BACSTAC_UTIL_STATUS
{
    UTIL_STATUS_OK,
    UTIL_STATUS_INVALID_PARAM,
    UTIL_STATUS_UNKNOWN_DEVICE,
    UTIL_STATUS_DEVICE_SHUT_DOWN,
    UTIL_STATUS_BACNET_ERROR,
    UTIL_STATUS_OTHER
    BACSTAC_OTHER_ENUM(UTIL_STATUS)
} BACSTAC_UTIL_STATUS;

/*
 * Generic Buffer
 */


typedef struct tagBACSTAC_BUFFER {
    void                *pBuffer;
    BACSTAC_VALUE_SIZE  nBufferSize;
} BACSTAC_BUFFER, *PBACSTAC_BUFFER;


/*
 * String types
 */

typedef enum BACSTAC_STRING_TYPE
{
    /* STRING_NONE does not present in the BACnet standard,
     * but some strings may be optional, and STRING_NONE is
     * used to indicate that the optional string was omitted. */
    STRING_NONE = -1,
    /* the following charset are defined in the BACnet standard */
    STRING_UTF_8 = 0,
    STRING_DBCS = 1,
    STRING_JIS_X_0208 = 2,
    STRING_UCS_4 = 3,
    STRING_UCS_2 = 4,
    STRING_ISO_8859_1 = 5,
    /* BACnet standard reserves values up to 255. */
    BACSTAC_STRING_TYPE_MAX = 255
    BACSTAC_OTHER_ENUM(STRING_TYPE),

    /* for backward compatibility only */
    STRING_ANSI_X34 = STRING_UTF_8,
    STRING_JIS_C_6226 = STRING_JIS_X_0208
#ifdef _MSC_VER
#pragma deprecated(STRING_ANSI_X34)
#pragma deprecated(STRING_JIS_C_6226)
#endif /* _MSC_VER */
} BACSTAC_STRING_TYPE;

/*
 * BACnet data types
 */


typedef enum BACSTAC_DATA_TYPE
{
    DATA_TYPE_NEW_STANDARD = -5, /*= BACSTAC_RAW_ASN1_VALUE */
    BACSTAC_DATA_TYPE_MIN = DATA_TYPE_NEW_STANDARD,
    DATA_TYPE_PROPRIETARY = -4,	/*= BACSTAC_RAW_ASN1_VALUE */
    DATA_TYPE_EXTENDED = -3,	/*= BACSTAC_RAW_ASN1_VALUE */

    DATA_TYPE_ANY_PRIMITIVE = -2,
    DATA_TYPE_INVALID = -1,

    DATA_TYPE_NULL = 0,		/*= BACSTAC_NULL */
    DATA_TYPE_BOOLEAN = 1,	/*= BACSTAC_BOOLEAN */
    DATA_TYPE_UNSIGNED = 2,	/*= BACSTAC_UNSIGNED */
    DATA_TYPE_INTEGER = 3,	/*= BACSTAC_INTEGER */
    DATA_TYPE_REAL = 4,		/*= BACSTAC_REAL */
    DATA_TYPE_DOUBLE = 5,	/*= BACSTAC_DOUBLE */
    DATA_TYPE_OCTET_STRING = 6,	/*= BACSTAC_OCTET_STRING */
    DATA_TYPE_ANY_CHAR_STRING = 7, /*= BACSTAC_CHAR_STRING */
    DATA_TYPE_BIT_STRING = 8,	/*= BACSTAC_BIT_STRING */
    DATA_TYPE_ENUM = 9,	/*= BACSTAC_ENUM */
    DATA_TYPE_DATE = 10,	/*= BACSTAC_DATE */
    DATA_TYPE_TIME = 11,	/*= BACSTAC_TIME */
    DATA_TYPE_OBJECT_ID = 12,	/*= BACSTAC_OBJECT_ID */

    DATA_TYPE_UTF_8_STRING, /*= STRING_OF BACSTAC_CHAR */
    DATA_TYPE_DBCS_STRING,	/*= BACSTAC_DBCS_STRING */
    DATA_TYPE_JIS_X_0208_STRING, /*= STRING_OF BACSTAC_CHAR (#2) */
    DATA_TYPE_UCS_4_STRING,	/*= STRING_OF BACSTAC_UCS4_CHAR */
    DATA_TYPE_UCS_2_STRING,	/*= STRING_OF BACSTAC_UCS2_CHAR */
    DATA_TYPE_ISO_8859_1_STRING, /*= STRING_OF BACSTAC_CHAR (#3) */

    DATA_TYPE_UI64,      /*= BACSTAC_UI64 */

    DATA_TYPE_PRI_ARRAY_ITEM,	/*= BACSTAC_PRI_ARRAY_ITEM */
    DATA_TYPE_ADDRESS,		/*= BACSTAC_ADDRESS */
    DATA_TYPE_DATE_TIME,	/*= BACSTAC_DATE_TIME */
    DATA_TYPE_OBJ_PROP_REF,	/*= BACSTAC_OBJ_PROP_REF */
    DATA_TYPE_PROPERTY_STATES,	/*= BACSTAC_PROPERTY_STATES */
    DATA_TYPE_DATE_RANGE,	/*= BACSTAC_DATE_RANGE */
    DATA_TYPE_WEEK_N_DAY,	/*= BACSTAC_WEEK_N_DAY */
    DATA_TYPE_CALENDAR_ENTRY,	/*= BACSTAC_CALENDAR_ENTRY */
    DATA_TYPE_TIME_VALUE,	/*= BACSTAC_TIME_VALUE */
    DATA_TYPE_SPECIAL_EVENT,	/*= BACSTAC_SPECIAL_EVENT */
    DATA_TYPE_PROPERTY_VALUE,	/*= BACSTAC_PROPERTY_VALUE */
    DATA_TYPE_RECIPIENT,	/*= BACSTAC_RECIPIENT */
    DATA_TYPE_SETPOINT_REF,	/*= BACSTAC_SETPOINT_REF */
    DATA_TYPE_DESTINATION,	/*= BACSTAC_DESTINATION */
    DATA_TYPE_DAILY_SCHEDULE,	/*= BACSTAC_DAILY_SCHEDULE */
    DATA_TYPE_TIME_STAMP,	/*= BACSTAC_TIME_STAMP */
    DATA_TYPE_PROP_REF,	/*= BACSTAC_PROP_REF */
    DATA_TYPE_ADDRESS_BINDING,	/*= BACSTAC_ADDRESS_BINDING */
    DATA_TYPE_DEV_OBJ_PROP_REF, /*= BACSTAC_DEV_OBJ_PROP_REF */
    DATA_TYPE_CLIENT_COV,	/*= BACSTAC_CLIENT_COV */
    DATA_TYPE_LOG_RECORD,	/*= BACSTAC_LOG_RECORD */
    DATA_TYPE_ACTION_LIST,	/*= BACSTAC_ACTION_LIST */
    DATA_TYPE_READ_ACCESS_SPEC,	/*= BACSTAC_READ_ACCESS_SPEC */
    DATA_TYPE_READ_ACCESS_RESULT,	/*= BACSTAC_READ_ACCESS_RESULT */
    DATA_TYPE_EVENT_PARAMETER2,	/*= BACSTAC_EVENT_PARAMETER2 */
    DATA_TYPE_COV_SUBSCRIPTION,	/*= BACSTAC_COV_SUBSCRIPTION */
    DATA_TYPE_DEV_OBJ_REF,	/*= BACSTAC_DEV_OBJ_REF */
    DATA_TYPE_DEV_OBJ_PROP_VALUE, /*= BACSTAC_DEV_OBJ_PROP_VALUE */ /* Add-135-2001a */
    DATA_TYPE_ACCUMULATOR_RECORD, /*= BACSTAC_ACCUMULATOR_RECORD */ /* Add-135-2001c-4 */
    DATA_TYPE_PRESCALE,		/*= BACSTAC_PRESCALE */ /* Add-135-2001c-4 */
    DATA_TYPE_SCALE,		/*= BACSTAC_SCALE */ /* Add-135-2001c-4 */
    DATA_TYPE_ACTION_COMMAND,	/*= BACSTAC_ACTION_COMMAND */
    DATA_TYPE_SHED_LEVEL,	/*= BACSTAC_SHED_LEVEL */
    DATA_TYPE_VT_SESSION, /*= BACSTAC_VT_SESSION */
    DATA_TYPE_SESSION_KEY, /*= BACSTAC_SESSION_KEY */
    DATA_TYPE_EVENT_LOG_RECORD,  /*= BACSTAC_EVENT_LOG_RECORD */
    DATA_TYPE_LOG_MULTI_RECORD,  /*= BACSTAC_LOG_MULTI_RECORD */
    DATA_TYPE_AUTHN_POLICY,  /*= BACSTAC_AUTHN_POLICY */
    DATA_TYPE_AUTHN_FACTOR,  /*= BACSTAC_AUTHN_FACTOR */
    DATA_TYPE_ACCESS_RULE,  /*= BACSTAC_ACCESS_RULE */
    DATA_TYPE_ACCESS_RIGHTS, /*= BACSTAC_ACCESS_RIGHTS */
    DATA_TYPE_AUTHN_FACTOR_FORMAT,  /*= BACSTAC_AUTHN_FACTOR_FORMAT */
    DATA_TYPE_CRED_AUTHN_FACTOR,  /*= BACSTAC_CRED_AUTHN_FACTOR */
    DATA_TYPE_OPT_CHAR_STRING,  /*= BACSTAC_OPT_CHAR_STRING */
    DATA_TYPE_PROP_ACCESS_RESULT, /*= BACSTAC_PROP_ACCESS_RESULT */
    DATA_TYPE_FAULT_PARAMETER,  /*= BACSTAC_FAULT_PARAMETER */
    DATA_TYPE_EVENT_NOTIF_SUBS, /*= BACSTAC_EVENT_NOTIF_SUBS */
    DATA_TYPE_PORT_PERMISSION,   /* BACSTAC_PORT_PERMISSION */
    DATA_TYPE_PROC_ID_SELECTION, /* BACSTAC_PROC_ID_SELECTION */
    DATA_TYPE_OPT_CMD_UNSIGNED, /*= BACSTAC_OPT_CMD_UNSIGNED */
    DATA_TYPE_HOST_ADDRESS,  /*= BACSTAC_HOST_ADDRESS */
    DATA_TYPE_HOST_NPORT,  /*= BACSTAC_HOST_NPORT */
    DATA_TYPE_BDT_ENTRY,   /*= BACSTAC_BDT_ENTRY */
    DATA_TYPE_FDT_ENTRY,   /*= BACSTAC_FDT_ENTRY */
    DATA_TYPE_VMAC_ENTRY,  /*= BACSTAC_VMAC_ENTRY */
    DATA_TYPE_ROUTER_ENTRY, /*= BACSTAC_ROUTER_ENTRY */
    DATA_TYPE_VALUE_SOURCE,  /*= BACSTAC_VALUE_SOURCE */
    DATA_TYPE_FLOOR_NUMBER,  /*= BACSTAC_FLOOR_NUMBER */
    DATA_TYPE_LANDING_CALL,  /*= BACSTAC_LANDING_CALL */
    DATA_TYPE_ASSIGNED_LANDING_CALLS,  /*= BACSTAC_ASSIGNED_LANDING_CALLS */
    DATA_TYPE_LANDING_CALL_STATUS,  /*= BACSTAC_LANDING_CALL_STATUS */
    DATA_TYPE_LANDING_DOOR,  /*= BACSTAC_LANDING_DOOR */
    DATA_TYPE_LANDING_DOOR_STATUS,  /*= BACSTAC_LANDING_DOOR_STATUS */
    DATA_TYPE_LIFT_CAR_CALL_LIST,  /*= BACSTAC_LIFT_CAR_CALL_LIST */
    DATA_TYPE_COV_MULTI_SUBSCRIPTION, /*= BACSTAC_COV_MULTI_SUBSCRIPTION */
    DATA_TYPE_LIGHTING_CMD, /*= BACSTAC_LIGHTING_CMD */
    DATA_TYPE_CHANNEL_VALUE, /*= BACSTAC_CHANNEL_VALUE */
    DATA_TYPE_OPT_REAL, /*= BACSTAC_OPT_REAL */
    DATA_TYPE_OPT_BINARY_PV, /*= BACSTAC_OPT_BINARY_PV */
    DATA_TYPE_OPT_UNSIGNED, /*= BACSTAC_OPT_UNSIGNED */
    DATA_TYPE_NET_SECURITY_POLICY, /*= BACSTAC_NET_SECURITY_POLICY */
    DATA_TYPE_SECURITY_KEY_SET, /*= BACSTAC_SECURITY_KEY_SET */
    DATA_TYPE_TS_CHANGE_VALUE, /*= BACSTAC_TS_CHANGE_VALUE */
    DATA_TYPE_NAME_VALUE, /*= BACSTAC_NAME_VALUE */
    DATA_TYPE_NAME_VALUE_COLLECTION, /*= BACSTAC_NAME_VALUE_COLLECTION */
    DATA_TYPE_HEALTH, /*= BACSTAC_HEALTH */
    DATA_TYPE_SC_DIRECT_CONNECTION, /*= BACSTAC_SC_DIRECT_CONNECTION */
    DATA_TYPE_SC_HUB_CONNECTION, /*= BACSTAC_SC_HUB_CONNECTION */
    DATA_TYPE_SC_FAILED_CONN_REQ, /*= BACSTAC_SC_FAILED_CONN_REQ */
    DATA_TYPE_SC_HUB_FUNC_CONN, /*= BACSTAC_SC_HUB_FUNC_CONN */
    DATA_TYPE_OBJECT_SELECTOR, /*= BACSTAC_OBJECT_SELECTOR */
    DATA_TYPE_OPT_PRI_FILTER, /*= BACSTAC_OPT_PRI_FILTER */
    DATA_TYPE_AUDIT_LOG_RECORD, /*= BACSTAC_AUDIT_LOG_RECORD */
    DATA_TYPE_ACK_ALARM_INFO_TV, /*= BACSTAC_ACK_ALARM_INFO_TV */
    DATA_TYPE_LIFE_SAFETY_INFO_TV, /* BACSTAC_LIFE_SAFETY_INFO_TV */
    DATA_TYPE_STAGE_LIMIT_VALUE, /*= BACSTAC_STAGE_LIMIT_VALUE */
    DATA_TYPE_XY_COLOR, /* = BACSTAC_XY_COLOR */
    DATA_TYPE_COLOR_CMD, /* = BACSTAC_COLOR_CMD */
    DATA_TYPE_DEV_ADDR_PROXY_TABLE_ENTRY, /* = BACSTAC_DEV_ADDR_PROXY_TABLE_ENTRY */
    BACSTAC_DATA_TYPE_MAX = DATA_TYPE_DEV_ADDR_PROXY_TABLE_ENTRY
    BACSTAC_OTHER_ENUM(DATA_TYPE)

    /* for backward compatibility only */
#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 8))
    #define DATA_TYPE_SIGNED                 _Pragma("GCC warning \"DATA_TYPE_SIGNED is deprecated\"") DATA_TYPE_INTEGER
    #define DATA_TYPE_ENUMERATED             _Pragma("GCC warning \"DATA_TYPE_ENUMERATED is deprecated\"") DATA_TYPE_ENUM
    #define DATA_TYPE_OBJECT_IDENTIFIER      _Pragma("GCC warning \"DATA_TYPE_OBJECT_IDENTIFIER is deprecated\"") DATA_TYPE_OBJECT_ID
    #define DATA_TYPE_CHAR_STRING            _Pragma("GCC warning \"DATA_TYPE_CHAR_STRING is deprecated\"") DATA_TYPE_UTF_8_STRING
    #define DATA_TYPE_DEV_OBJ_PROP_REFERENCE _Pragma("GCC warning \"DATA_TYPE_DEV_OBJ_PROP_REFERENCE is deprecated\"") DATA_TYPE_DEV_OBJ_PROP_REF
    #define DATA_TYPE_DEV_OBJ_REFERENCE      _Pragma("GCC warning \"DATA_TYPE_DEV_OBJ_REFERENCE is deprecated\"") DATA_TYPE_DEV_OBJ_REF
    #define DATA_TYPE_OBJ_PROP_REFERENCE     _Pragma("GCC warning \"DATA_TYPE_OBJ_PROP_REFERENCE is deprecated\"") DATA_TYPE_OBJ_PROP_REF
    #define DATA_TYPE_PROPERTY_REFERENCE     _Pragma("GCC warning \"DATA_TYPE_PROPERTY_REFERENCE is deprecated\"") DATA_TYPE_PROP_REF
    #define DATA_TYPE_SETPOINT_REFERENCE     _Pragma("GCC warning \"DATA_TYPE_SETPOINT_REFERENCE is deprecated\"") DATA_TYPE_SETPOINT_REF
    #define DATA_TYPE_ANSI_X34_STRING        _Pragma("GCC warning \"DATA_TYPE_ANSI_X34_STRING is deprecated\"") DATA_TYPE_UTF_8_STRING
    #define DATA_TYPE_JIS_C_6226_STRING      _Pragma("GCC warning \"DATA_TYPE_JIS_C_6226_STRING is deprecated\"") DATA_TYPE_JIS_X_0208_STRING
#else
    ,
    DATA_TYPE_SIGNED = DATA_TYPE_INTEGER,
    DATA_TYPE_ENUMERATED = DATA_TYPE_ENUM,
    DATA_TYPE_OBJECT_IDENTIFIER = DATA_TYPE_OBJECT_ID,
    DATA_TYPE_CHAR_STRING = DATA_TYPE_UTF_8_STRING,
    DATA_TYPE_DEV_OBJ_PROP_REFERENCE = DATA_TYPE_DEV_OBJ_PROP_REF,
    DATA_TYPE_DEV_OBJ_REFERENCE = DATA_TYPE_DEV_OBJ_REF,
    DATA_TYPE_OBJ_PROP_REFERENCE = DATA_TYPE_OBJ_PROP_REF,
    DATA_TYPE_PROPERTY_REFERENCE = DATA_TYPE_PROP_REF,
    DATA_TYPE_SETPOINT_REFERENCE = DATA_TYPE_SETPOINT_REF,
    DATA_TYPE_ANSI_X34_STRING = DATA_TYPE_UTF_8_STRING,
    DATA_TYPE_JIS_C_6226_STRING = DATA_TYPE_JIS_X_0208_STRING
#endif
#ifdef _MSC_VER
#pragma deprecated(DATA_TYPE_SIGNED)
#pragma deprecated(DATA_TYPE_ENUMERATED)
#pragma deprecated(DATA_TYPE_OBJECT_IDENTIFIER)
#pragma deprecated(DATA_TYPE_CHAR_STRING)
#pragma deprecated(DATA_TYPE_DEV_OBJ_PROP_REFERENCE)
#pragma deprecated(DATA_TYPE_DEV_OBJ_REFERENCE)
#pragma deprecated(DATA_TYPE_OBJ_PROP_REFERENCE)
#pragma deprecated(DATA_TYPE_PROPERTY_REFERENCE)
#pragma deprecated(DATA_TYPE_SETPOINT_REFERENCE)
#pragma deprecated(DATA_TYPE_ANSI_X34_STRING)
#pragma deprecated(DATA_TYPE_JIS_C_6226_STRING)
#endif /* _MSC_VER */
} BACSTAC_DATA_TYPE;


/* Object ID */

#if BACSTAC_BIT_ORDER == BACSTAC_ORDER_BIG_ENDIAN

typedef struct tagBACSTAC_OBJECT_ID
{
    BACSTAC_UI32 type : 10; /* BACSTAC_OBJECT_TYPE */
    BACSTAC_INST_NUMBER instNumber : 22;
} BACSTAC_OBJECT_ID, *PBACSTAC_OBJECT_ID;

#define BACSTAC_OBJECT_ID_INIT(objType, instNum) \
    {(objType), (instNum)}

#else /* BACSTAC_BIT_ORDER */

typedef struct tagBACSTAC_OBJECT_ID
{
    BACSTAC_INST_NUMBER instNumber : 22;
    BACSTAC_UI32 type : 10; /* BACSTAC_OBJECT_TYPE */
} BACSTAC_OBJECT_ID, *PBACSTAC_OBJECT_ID;

#define BACSTAC_OBJECT_ID_INIT(objType, instNum) \
    {(instNum), (objType)}

#define BACSTAC_OBJECT_ID_UNKNOWN \
    BACSTAC_OBJECT_ID_INIT(BACSTAC_OBJECT_TYPE_MAX, BACSTAC_INST_NUMBER_UNKNOWN)

#endif /* BACSTAC_BIT_ORDER */

/* Bit string */

#define BACSTAC_MAX_BIT_STRING_BUF      10
typedef struct tagBACSTAC_BIT_STRING
{
    BACSTAC_UI8     bitCount;
    BACSTAC_BYTE    data[BACSTAC_MAX_BIT_STRING_BUF];
} BACSTAC_BIT_STRING, *PBACSTAC_BIT_STRING;

/* empty bit string */
#define BACSTAC_BIT_STRING0_INIT \
    {0, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}}
/* for bitCount up to 8 */
#define BACSTAC_BIT_STRING1_INIT(bitCount, b0) \
    {(bitCount), {(b0), 0, 0, 0, 0, 0, 0, 0, 0, 0}}
/* for bitCount up to 16 */
#define BACSTAC_BIT_STRING2_INIT(bitCount, b0, b1) \
    {(bitCount), {(b0), (b1), 0, 0, 0, 0, 0, 0, 0, 0}}
/* for bitCount up to 24 */
#define BACSTAC_BIT_STRING3_INIT(bitCount, b0, b1, b2) \
    {(bitCount), {(b0), (b1), (b2), 0, 0, 0, 0, 0, 0, 0}}
/* for bitCount up to 32 */
#define BACSTAC_BIT_STRING4_INIT(bitCount, b0, b1, b2, b3) \
    {(bitCount), {(b0), (b1), (b2), (b3), 0, 0, 0, 0, 0, 0}}
/* for bitCount up to 40 */
#define BACSTAC_BIT_STRING5_INIT(bitCount, b0, b1, b2, b3, b4) \
    {(bitCount), {(b0), (b1), (b2), (b3), (b4), 0, 0, 0, 0, 0}}
/* for bitCount up to 48 */
#define BACSTAC_BIT_STRING6_INIT(bitCount, b0, b1, b2, b3, b4, b5) \
    {(bitCount), {(b0), (b1), (b2), (b3), (b4), (b5), 0, 0, 0, 0}}
/* for bitCount up to 56 */
#define BACSTAC_BIT_STRING7_INIT(bitCount, b0, b1, b2, b3, b4, b5, b6) \
    {(bitCount), {(b0), (b1), (b2), (b3), (b4), (b5), (b6), 0, 0, 0}}
/* for bitCount up to 64 */
#define BACSTAC_BIT_STRING8_INIT(bitCount, b0, b1, b2, b3, b4, b5, b6, b7) \
    {(bitCount), {(b0), (b1), (b2), (b3), (b4), (b5), (b6), (b7), 0, 0}}
/* for bitCount up to 72 */
#define BACSTAC_BIT_STRING9_INIT(bitCount, b0, b1, b2, b3, b4, b5, b6, b7, b8) \
    {(bitCount), {(b0), (b1), (b2), (b3), (b4), (b5), (b6), (b7), (b8), 0}}
/* for bitCount up to 80 */
#define BACSTAC_BIT_STRING10_INIT(bitCount, b0, b1, b2, b3, b4, b5, b6, b7, b8, b9) \
    {(bitCount), {(b0), (b1), (b2), (b3), (b4), (b5), (b6), (b7), (b8), (b9)}}


/* Octet String */

typedef struct tagBACSTAC_OCTET_STRING
{
    BACSTAC_BYTE        *octets;
    BACSTAC_VALUE_SIZE  count;
} BACSTAC_OCTET_STRING, *PBACSTAC_OCTET_STRING;

#define BACSTAC_OCTET_STRING_INIT(pOctets, octetCount) \
    {(pOctets), (octetCount)}

typedef struct tagBACSTAC_CHAR_STRING
{
    /* 'data' contains pointer to null-terminated string, when
     * it passed from BACstac decoder to user. BACstac encoder
     * does not need the string to be null-terminated, so user
     * may store not null-terminated string here and specify
     * the string using the 'size' field.
     */
    void               *data;

    /* 'size' determines the length of data in _bytes_.
     *
     * BACSTAC_NULL_TERM_STRING can be used instead of actual
     * size to indicate that BACstac encoder should determine
     * the actual string size of the string based on the null
     * character.
     *
     * BACstac decoder always sets the actual size and never
     * uses BACSTAC_NULL_TERM_STRING, though the null character
     * is routinely added to the BACnet string for convenience.
     */
    BACSTAC_VALUE_SIZE size;

    /* DBCS code page; it is used for DBCS strings only! */
    BACSTAC_UI16       codePage;

    /* 'charset' determines the character set used for encoding
     * of data. For DBCS, you need take also 'codePage' into
     * account to find the right data encoding.
     *
     * 'charset' can have any value defined by the BACnet standard.
     * See BACSTAC_STRING_TYPE.
     */
    short              charset;
} BACSTAC_CHAR_STRING, *PBACSTAC_CHAR_STRING;

/* BACSTAC_NULL_TERM_STRING can be used instead of size to tell that the
 * length of the string should be determined based on the null-character. */
#define BACSTAC_NULL_TERM_STRING BACSTAC_VALUE_SIZE_MAX


/* BACSTAC_CHAR_STRING initializers for null-terminated C strings */

#define BACSTAC_UTF8_CSTRING_INIT(string) \
   BACSTAC_UTF8_NSTRING_INIT((string), BACSTAC_NULL_TERM_STRING)

#define BACSTAC_DBCS_CSTRING_INIT(codePage, string) \
   BACSTAC_DBCS_NSTRING_INIT((codePage), (string), BACSTAC_NULL_TERM_STRING)

#define BACSTAC_JISX_CSTRING_INIT(string) \
   BACSTAC_JISX_NSTRING_INIT((string), BACSTAC_NULL_TERM_STRING)

#define BACSTAC_UCS4_CSTRING_INIT(string) \
   BACSTAC_UCS4_NSTRING_INIT((string), BACSTAC_NULL_TERM_STRING)

#define BACSTAC_UCS2_CSTRING_INIT(string) \
   BACSTAC_UCS2_NSTRING_INIT((string), BACSTAC_NULL_TERM_STRING)

#define BACSTAC_LATIN1_CSTRING_INIT(string) \
   BACSTAC_LATIN1_NSTRING_INIT((string), BACSTAC_NULL_TERM_STRING)

/* for backward compatibility only */
#define BACSTAC_ASCII_CSTRING_INIT BACSTAC_UTF8_CSTRING_INIT
#define BACSTAC_JISC_CSTRING_INIT BACSTAC_JISX_CSTRING_INIT

/* BACSTAC_CHAR_STRING initializers for strings whose length is specified
 * explicitly as a number of _bytes_. The string parameter does not have
 * to be NULL-terminated, and if a NULL-terminated string is provided, the
 * size parameter should indicate its length without the terminating NULL
 * character. For example:
 *
 * #define MY_ASTR "Hello world!"
 * BACSTAC_CHAR_STRING myAStr = BACSTAC_UTF8_NSTRING_INIT(MY_ASTR, sizeof(MY_ASTR)-1);
 *
 * #define MY_WSTR L"Hello world!"
 * // For platforms where wchar_t is 16-bit:
 * BACSTAC_CHAR_STRING myWStr = BACSTAC_UCS2_NSTRING_INIT(MY_WSTR, sizeof(MY_WSTR)-2);
 * // For platforms where wchar_t is 32-bit:
 * BACSTAC_CHAR_STRING myWStr = BACSTAC_UCS4_NSTRING_INIT(MY_WSTR, sizeof(MY_WSTR)-4);
 */

#define BACSTAC_UTF8_NSTRING_INIT(string, size) \
    {(string), (size), 0, STRING_UTF_8}

#define BACSTAC_DBCS_NSTRING_INIT(codePage, string, size) \
    {(string), (size), (codePage), STRING_DBCS}

#define BACSTAC_JISX_NSTRING_INIT(string, size) \
    {(string), (size), 0, STRING_JIS_X_0208}

#define BACSTAC_UCS4_NSTRING_INIT(string, size) \
    {(string), (size), 0, STRING_UCS_4}

#define BACSTAC_UCS2_NSTRING_INIT(string, size) \
    {(string), (size), 0, STRING_UCS_2}

#define BACSTAC_LATIN1_NSTRING_INIT(string, size) \
    {(string), (size), 0, STRING_ISO_8859_1}

/* backward compatibility with a typo */
#define BACSTAC_USC2_NSTRING_INIT BACSTAC_UCS2_NSTRING_INIT
/* for backward compatibility only */
#define BACSTAC_ASCII_NSTRING_INIT BACSTAC_UTF8_NSTRING_INIT
#define BACSTAC_JISC_NSTRING_INIT BACSTAC_JISX_NSTRING_INIT


/* because data type is called ANY_CHAR_STRING, we
 * add alias here for convenience */
typedef BACSTAC_CHAR_STRING BACSTAC_ANY_CHAR_STRING;

typedef union tagBACSTAC_OPT_CHAR_STRING
{
   BACSTAC_CHAR_STRING charStr;
} BACSTAC_OPT_CHAR_STRING, *PBACSTAC_OPT_CHAR_STRING;

/* Initialize BACSTAC_OPT_CHAR_STRING to NULL */

#define BACSTAC_OPT_STRING_INIT \
    {{NULL, BACSTAC_NULL_TERM_STRING, 0, STRING_NONE}}

/* Initialize BACSTAC_OPT_CHAR_STRING to null-terminated C strings */

#define BACSTAC_UTF8_OPT_CSTRING_INIT(string) \
   {BACSTAC_UTF8_CSTRING_INIT(string)}

#define BACSTAC_DBCS_OPT_CSTRING_INIT(codePage, string) \
   {BACSTAC_DBCS_CSTRING_INIT((codePage), (string))}

#define BACSTAC_JISX_OPT_CSTRING_INIT(string) \
   {BACSTAC_JISX_CSTRING_INIT(string)}

#define BACSTAC_UCS4_OPT_CSTRING_INIT(string) \
   {BACSTAC_UCS4_CSTRING_INIT(string)}

#define BACSTAC_UCS2_OPT_CSTRING_INIT(string) \
   {BACSTAC_UCS2_CSTRING_INIT(string)}

#define BACSTAC_LATIN1_OPT_CSTRING_INIT(string) \
   {BACSTAC_LATIN1_CSTRING_INIT(string)}

/* Initialize BACSTAC_OPT_CHAR_STRING to strings whose length is specified
 * explicitly as a number of _bytes_.
 */

#define BACSTAC_UTF8_OPT_NSTRING_INIT(string, size) \
    {BACSTAC_UTF8_NSTRING_INIT((string), (size))}

#define BACSTAC_DBCS_OPT_NSTRING_INIT(codePage, string, size) \
    {BACSTAC_DBCS_NSTRING_INIT((codePage), (string), (size))}

#define BACSTAC_JISX_OPT_NSTRING_INIT(string, size) \
    {BACSTAC_JISX_NSTRING_INIT((string), (size))}

#define BACSTAC_UCS4_OPT_NSTRING_INIT(string, size) \
    {BACSTAC_UCS4_NSTRING_INIT((string), (size))}
#define BACSTAC_UCS2_OPT_NSTRING_INIT(string, size) \
    {BACSTAC_UCS2_NSTRING_INIT((string), (size))}

#define BACSTAC_LATIN1_OPT_NSTRING_INIT(string, size) \
    {BACSTAC_LATIN1_NSTRING_INIT((string), (size))}

/* for backward compatibility only */
#define BACSTAC_ASCII_OPT_CSTRING_INIT BACSTAC_UCS2_OPT_CSTRING_INIT
#define BACSTAC_ASCII_OPT_NSTRING_INIT BACSTAC_UCS2_OPT_NSTRING_INIT
#define BACSTAC_JISC_OPT_CSTRING_INIT BACSTAC_JISX_OPT_CSTRING_INIT
#define BACSTAC_JISC_OPT_NSTRING_INIT BACSTAC_JISX_OPT_NSTRING_INIT

/* Date */

typedef enum BACSTAC_MONTH
{
    MONTH_UNSPECIFIED,
    MONTH_JANUARY,
    MONTH_FEBRUARY,
    MONTH_MARCH,
    MONTH_APRIL,
    MONTH_MAY,
    MONTH_JUNE,
    MONTH_JULY,
    MONTH_AUGUST,
    MONTH_SEPTEMBER,
    MONTH_OCTOBER,
    MONTH_NOVEMBER,
    MONTH_DECEMBER,
    MONTHS_ODD,                 /* Add-135-2001a */
    MONTHS_EVEN,                /* Add-135-2001a */
    BACSTAC_MONTH_MAX = MONTHS_EVEN
    BACSTAC_OTHER_ENUM(MONTH)
} BACSTAC_MONTH, *PBACSTAC_MONTH;

typedef enum BACSTAC_DAY_OF_WEEK
{
    DAY_OF_WEEK_UNSPECIFIED,
    DAY_OF_WEEK_MONDAY,
    DAY_OF_WEEK_TUESDAY,
    DAY_OF_WEEK_WEDNESDAY,
    DAY_OF_WEEK_THURSDAY,
    DAY_OF_WEEK_FRIDAY,
    DAY_OF_WEEK_SATURDAY,
    DAY_OF_WEEK_SUNDAY
    BACSTAC_OTHER_ENUM(DAY_OF_WEEK)
} BACSTAC_DAY_OF_WEEK, *PBACSTAC_DAY_OF_WEEK;

typedef BACSTAC_UI16 BACSTAC_YEAR, *PBACSTAC_YEAR;
#define YEAR_UNSPECIFIED            ((BACSTAC_YEAR) ~0)
#define BACSTAC_YEAR_MIN            ((BACSTAC_YEAR)1900)
#define BACSTAC_YEAR_MAX            ((BACSTAC_YEAR)(1900+254))

typedef BACSTAC_UI8  BACSTAC_DAY_OF_MONTH, *PBACSTAC_DAY_OF_MONTH;
#define DAY_OF_MONTH_UNSPECIFIED    ((BACSTAC_DAY_OF_MONTH) ~0)
#define LAST_DAY_OF_MONTH           ((BACSTAC_DAY_OF_MONTH) 32) /* Add-135-2001a */
#define ODD_DAYS_OF_MONTH           ((BACSTAC_DAY_OF_MONTH) 33) /* Add-135-2008h-8 */
#define EVEN_DAYS_OF_MONTH          ((BACSTAC_DAY_OF_MONTH) 34) /* Add-135-2008h-8 */
#define BACSTAC_DAY_OF_MONTH_MAX    EVEN_DAYS_OF_MONTH

typedef struct tagBACSTAC_DATE
{
    BACSTAC_YEAR        year;
    BACSTAC_UI8         month; /* BACSTAC_MONTH */
    BACSTAC_DAY_OF_MONTH    dayOfMonth;
    BACSTAC_DAY_OF_WEEK     dayOfWeek;
} BACSTAC_DATE, *PBACSTAC_DATE;

#define BACSTAC_DATE_INIT(year, month, dayOfMonth, dayOfWeek) \
    {(year), (month), (dayOfMonth), (dayOfWeek)}

#define BACSTAC_UNSPECIFIED_DATE_INIT \
    {YEAR_UNSPECIFIED, MONTH_UNSPECIFIED, DAY_OF_MONTH_UNSPECIFIED, DAY_OF_WEEK_UNSPECIFIED}


/* Time */

typedef BACSTAC_UI8 BACSTAC_HOUR, *PBACSTAC_HOUR;
#define HOUR_UNSPECIFIED            ((BACSTAC_HOUR) ~0)

typedef BACSTAC_UI8 BACSTAC_MINUTE, *PBACSTAC_MINUTE;
#define MINUTE_UNSPECIFIED          ((BACSTAC_MINUTE) ~0)

typedef BACSTAC_UI8 BACSTAC_SECOND, *PBACSTAC_SECOND;
#define SECOND_UNSPECIFIED          ((BACSTAC_SECOND) ~0)

typedef BACSTAC_UI8 BACSTAC_HUNDREDTHS, *PBACSTAC_HUNDREDTHS;
#define HUNDREDTHS_UNSPECIFIED      ((BACSTAC_HUNDREDTHS) ~0)

typedef struct tagBACSTAC_TIME
{
    BACSTAC_HOUR        hour;
    BACSTAC_MINUTE      minute;
    BACSTAC_SECOND      second;
    BACSTAC_HUNDREDTHS  hundredths;
} BACSTAC_TIME, *PBACSTAC_TIME;

#define BACSTAC_TIME_INIT(hour, minute, second, hundredths) \
    {(hour), (minute), (second), (hundredths)}

#define BACSTAC_UNSPECIFIED_TIME_INIT \
    {HOUR_UNSPECIFIED, MINUTE_UNSPECIFIED, SECOND_UNSPECIFIED, HUNDREDTHS_UNSPECIFIED}


/* Date Time */

typedef struct tagBACSTAC_DATE_TIME
{
    BACSTAC_DATE    date;
    BACSTAC_TIME    time;
} BACSTAC_DATE_TIME, *PBACSTAC_DATE_TIME;

#define BACSTAC_DATE_TIME_INIT(year, month, dayOfMonth, dayOfWeek, \
        hour, minute, second, hundredths) \
    {BACSTAC_DATE_INIT((year), (month), (dayOfMonth), (dayOfWeek)), \
     BACSTAC_TIME_INIT((hour), (minute), (second), (hundredths))}

#define BACSTAC_UNSPECIFIED_DATE_TIME_INIT \
    BACSTAC_DATE_TIME_INIT( \
       YEAR_UNSPECIFIED, MONTH_UNSPECIFIED, DAY_OF_MONTH_UNSPECIFIED, DAY_OF_WEEK_UNSPECIFIED, \
       HOUR_UNSPECIFIED, MINUTE_UNSPECIFIED, SECOND_UNSPECIFIED, HUNDREDTHS_UNSPECIFIED)


/* Priority array item */

typedef struct tagBACSTAC_PRI_ARRAY_ITEM
{
    BACSTAC_DATA_TYPE   tag;
    union {
        BACSTAC_ENUM         e; /*= tag:DATA_TYPE_ENUM */
        BACSTAC_REAL         r; /*= tag:DATA_TYPE_REAL */
        BACSTAC_UNSIGNED     u; /*= tag:DATA_TYPE_UNSIGNED */
        BACSTAC_BOOLEAN      b; /*= tag:DATA_TYPE_BOOLEAN */
        BACSTAC_INTEGER      i; /*= tag:DATA_TYPE_INTEGER */
        BACSTAC_DOUBLE       d; /*= tag:DATA_TYPE_DOUBLE */
        BACSTAC_TIME         time; /*= tag:DATA_TYPE_TIME */
        BACSTAC_CHAR_STRING  charStr; /*= tag:DATA_TYPE_ANY_CHAR_STRING */
        BACSTAC_OCTET_STRING octetStr; /*= tag:DATA_TYPE_OCTET_STRING */
        BACSTAC_BIT_STRING   bitStr; /*= tag:DATA_TYPE_BIT_STRING */
        BACSTAC_DATE         date; /*= tag:DATA_TYPE_DATE */
        BACSTAC_OBJECT_ID    objectID; /*= tag:DATA_TYPE_OBJECT_ID */
        BACSTAC_DATE_TIME    dateTime; /*= tag:DATA_TYPE_DATE_TIME */

        /* for backward compatibility only */
        BACSTAC_DEPRECATED BACSTAC_INTEGER s;
    } value;
} BACSTAC_PRI_ARRAY_ITEM, *PBACSTAC_PRI_ARRAY_ITEM;

/* for backward compatibility */
typedef BACSTAC_DEPRECATED BACSTAC_PRI_ARRAY_ITEM BACSTAC_PRIORITY_ARRAY_ITEM;
typedef BACSTAC_DEPRECATED PBACSTAC_PRI_ARRAY_ITEM PBACSTAC_PRIORITY_ARRAY_ITEM;

/* Priority Array */

#define BACSTAC_MAX_PRI_ARRAY           16
typedef BACSTAC_PRI_ARRAY_ITEM
BACSTAC_PRIORITY_ARRAY[BACSTAC_MAX_PRI_ARRAY];
typedef BACSTAC_PRI_ARRAY_ITEM *PBACSTAC_PRIORITY_ARRAY;


/*
 * Address
 */


#define BACSTAC_MAC_BROADCAST_LENGTH    0
#define BACSTAC_MAC_ETHERNET_LENGTH     6
#define BACSTAC_MAC_ARCNET_LENGTH       1
#define BACSTAC_MAC_BIP_LENGTH          6

#define BACSTAC_NETWORK_LOCAL           0
#define BACSTAC_NETWORK_GLOBAL          ((BACSTAC_UI16) ~0)


typedef struct tagBACSTAC_ADDRESS
{
    BACSTAC_BOOLEAN remote;
    BACSTAC_UI16    netNumber;
    BACSTAC_BYTE    address[BACSTAC_MAX_MAC_ADDRESS];
    BACSTAC_BYTE    addressLength;
} BACSTAC_ADDRESS, *PBACSTAC_ADDRESS;

/* broadcast address initializer */
#define BACSTAC_ADDRESS0_INIT(netNumber) \
    {(BACSTAC_BOOLEAN)(netNumber != BACSTAC_NETWORK_LOCAL), \
      (netNumber), {0, 0, 0, 0, 0, 0, 0}, 0}

#define BACSTAC_ADDRESS1_INIT(netNumber, mac0) \
    {(BACSTAC_BOOLEAN)(netNumber != BACSTAC_NETWORK_LOCAL), \
      (netNumber), {(mac0), 0, 0, 0, 0, 0, 0}, 1}

#define BACSTAC_ADDRESS2_INIT(netNumber, mac0, mac1) \
    {(BACSTAC_BOOLEAN)(netNumber != BACSTAC_NETWORK_LOCAL), \
      (netNumber), {(mac0), (mac1), 0, 0, 0, 0, 0}, 2}

#define BACSTAC_ADDRESS3_INIT(netNumber, mac0, mac1, mac2) \
    {(BACSTAC_BOOLEAN)(netNumber != BACSTAC_NETWORK_LOCAL), \
      (netNumber), {(mac0), (mac1), (mac2), 0, 0, 0, 0}, 3}

#define BACSTAC_ADDRESS4_INIT(netNumber, mac0, mac1, mac2, mac3) \
    {(BACSTAC_BOOLEAN)(netNumber != BACSTAC_NETWORK_LOCAL), \
      (netNumber), {(mac0), (mac1), (mac2), (mac3), 0, 0, 0}, 4}

#define BACSTAC_ADDRESS5_INIT(netNumber, mac0, mac1, mac2, mac3, mac4) \
    {(BACSTAC_BOOLEAN)(netNumber != BACSTAC_NETWORK_LOCAL), \
      (netNumber), {(mac0), (mac1), (mac2), (mac3), (mac4), 0, 0}, 5}

#define BACSTAC_ADDRESS6_INIT(netNumber, mac0, mac1, mac2, mac3, mac4, mac5) \
    {(BACSTAC_BOOLEAN)(netNumber != BACSTAC_NETWORK_LOCAL), \
      (netNumber), {(mac0), (mac1), (mac2), (mac3), (mac4), (mac5), 0}, 6}

#define BACSTAC_ADDRESS7_INIT(netNumber, mac0, mac1, mac2, mac3, mac4, mac5, mac6) \
    {(BACSTAC_BOOLEAN)(netNumber != BACSTAC_NETWORK_LOCAL), \
      (netNumber), {(mac0), (mac1), (mac2), (mac3), (mac4), (mac5), (mac6)}, 7}

/* Initializers BACSTAC_ADDRESS to local, global, and remote broadcast */
#define BACSTAC_LBC_ADDRESS_INIT BACSTAC_ADDRESS0_INIT(BACSTAC_NETWORK_LOCAL)
#define BACSTAC_GBC_ADDRESS_INIT BACSTAC_ADDRESS0_INIT(BACSTAC_NETWORK_GLOBAL)
#define BACSTAC_RBC_ADDRESS_INIT(netNumber) BACSTAC_ADDRESS0_INIT(netNumber)


/*
 * BACSTAC_DBCS_STRING
 */


typedef struct tagBACSTAC_DBCS_STRING {
    BACSTAC_UI16 codePage;
    BACSTAC_CHAR *stringData; /*= STRING */
} BACSTAC_DBCS_STRING, *PBACSTAC_DBCS_STRING;

#define BACSTAC_DBCS_STRING_INIT(codePage, string) \
    {(codePage), (string)}

/* Address Binding */

typedef struct tagBACSTAC_ADDRESS_BINDING
{
   BACSTAC_OBJECT_ID objectID;
   BACSTAC_ADDRESS   address;
} BACSTAC_ADDRESS_BINDING, *PBACSTAC_ADDRESS_BINDING;

#define BACSTAC_ADDRESS_BINDING_INIT(instNum, address) \
    {BACSTAC_OBJECT_ID_INIT(OBJ_DEVICE, instNum), address}

/*
 * The actual type of pServiceInfo parameter is service specific
 */
typedef BACSTAC_HOOK_STATUS (BACstac_hook *BACSTAC_HOOK_PROC) (
   BACSTAC_HTSM hTransaction,
   BACSTAC_ADDRESS *sourceAddress,
   BACSTAC_ADDRESS *destinationAddress,
   const void *pServiceInfo
   );



/*
 * PropertyContents
 */


typedef struct tagBACSTAC_PROPERTY_CONTENTS {
    BACSTAC_DATA_TYPE       tag;
    BACSTAC_ELEMENT_COUNT   nElements;
    BACSTAC_BUFFER          buffer;
} BACSTAC_PROPERTY_CONTENTS, *PBACSTAC_PROPERTY_CONTENTS;

/*
 * BACSTAC_CONTENTS_INIT is a property contents initializer, which initializes
 * the property contents to refer to a variable or array. The <dataType>
 * parameter is BACstac data type that corresponds to the C object type of the
 * variable or array element correspondingly. <code>bufSize</code> specifies the
 * total size of memory in bytes that is used or can be used to store data.
 * <nElements> specifies the number
 * of elements in an array or it must be 1 if the contents is initialized to refer
 * to a variable.

 * BACSTAC_CONTENTS_INIT is a property contents initializer, which initializes
 * the property contents to refer to a variable, array, or memory buffer pointed
 * by the <ptr> parameter. The <dataType> parameter is BACstac data type that
 * corresponds to the C object type of the variable or array element referred
 * by the property contents. <bufSize> specifies the total size of memory in
 * bytes that is used or can be used to store data. <nElements> specifies the
 * number of elements in an array or it must be 1 if the contents is
 * initialized to refer to a variable.

 * Examples:
 *     const BACSTAC_CHAR *str = "My String";
 *     BACSTAC_PROPERTY_CONTENTS pc1 = BACSTAC_CONTENTS_INIT(
 *             DATA_TYPE_UTF_8_STRING, str, strlen(str)+1, 1);
 *
 *     BACSTAC_CHAR multistr[] = "String 1\0String 2\0String 3\String 4";
 *     BACSTAC_PROPERTY_CONTENTS pc2 = BACSTAC_CONTENTS_INIT(
 *        DATA_TYPE_UTF_8_STRING, multistr, sizeof(multistr), 4);
 *
 *     BACSTAC_UNSIGNED *pMyArray;
 *     BACSTAC_ARRAY_SIZE nElementsInMyArray;
 *     BACSTAC_VALUE_SIZE sizeOfMyArray;
 *     BACSTAC_PROPERTY_CONTENTS pc2 = BACSTAC_CONTENTS_INIT(
 *        DATA_TYPE_UNSIGNED, pMyArray, sizeOfMyArray, nElementsInMyArray);
 */
#define BACSTAC_CONTENTS_INIT(tag, ptr, bufSize, nElements) \
    {(tag), (nElements), {(ptr), (bufSize)}}

/*
 * BACSTAC_CONTENTS_INIT_TO_VAR is a property contents initializer, which
 * initializes the property contents to refer to the given variable. The <var>
 * parameter is an expression of one of C data type specified in Appendix D,
 * which designates a variable that will be referred by the property contents,
 * and the dataType parameter is the corresponding BACSTAC_DATA_TYPE value. The
 * C data type is required to be complete, i.e. its size can be determined
 * using the sizeof operator.
 *
 * NOTE 1: Because this initializer relies on the sizeof operator, it works for
 * strings variables declared as an array of chars (i.e. using BACSTAC_CHAR[])
 * or as a string literal, but produces the wrong result for strings described
 * as a pointer to BACSTAC_CHAR (i.e. using BACSTAC_CHAR*). If you want to
 * initialize a property contents using a pointer to BACSTAC_CHAR then
 * BACSTAC_CONTENTS_INIT should be used instead.
 *
 * NOTE 2: Usually C compilers do not evaluate the operand of a sizeof
 * operator, but if your compiler does then the <var> expression must be a
 * constant expression. Also, some old compilers may not support non-constant
 * expressions in initializers.
 *
 * Examples:
 *     const BACSTAC_CHAR str[] = "My String";
 *     BACSTAC_UNSIGNED value = 5;
 *     BACSTAC_PROPERTY_CONTENTS pc1 = BACSTAC_CONTENTS_INIT_TO_VAR(
 *             DATA_TYPE_UTF_8_STRING, str);
 *     BACSTAC_PROPERTY_CONTENTS pc2 = BACSTAC_CONTENTS_INIT_TO_VAR(
 *             DATA_TYPE_UTF_8_STRING, "My String");
 *     BACSTAC_PROPERTY_CONTENTS pc3 = BACSTAC_CONTENTS_INIT_TO_VAR(
 *             DATA_TYPE_UNSIGNED, value);
 */
#define BACSTAC_CONTENTS_INIT_TO_VAR(tag, var) \
    BACSTAC_CONTENTS_INIT((tag), (void*)&(var), sizeof(var), 1)

/*
 * BACSTAC_CONTENTS_INIT_TO_ARRAY is a property contents initializer, which
 * initializes the property contents to refer to the given array. The <array>
 * parameter should be an expression that designates an object of a complete
 * array data type and each element of the array has any scalar C data type
 * specified in Appendix D. The dataType parameter is the corresponding
 * BACSTAC_DATA_TYPE value that describes the type of an array element.
 *
 * NOTE 1: This initializer determines the buffer size and number of elements
 * using the sizeof operator, so it works only with arrays whose type is
 * complete (i.e. its size is known) and where each element has a fixed size
 * (i.e. not an array itself). If you have a pointer to some array or want to
 * initialize an array of strings then BACSTAC_CONTENTS_INIT should be used
 * instead.
 *
 * NOTE 2: Usually C compilers do not evaluate the operand of a sizeof
 * operator, but if your compiler does then the <array> expression must be a
 * constant expression. Also, some old compilers may not support non-constant
 * expressions in initializers.
 *
 * Example:
 *     BACSTAC_UNSIGNED array[] = {1, 2, 3};
 *     BACSTAC_PROPERTY_CONTENTS pc = BACSTAC_CONTENTS_INIT_TO_ARRAY(
 *                 DATA_TYPE_UNSIGNED, array);
 */
#define BACSTAC_CONTENTS_INIT_TO_ARRAY(tag, array) \
    BACSTAC_CONTENTS_INIT((tag), (void*)(array), sizeof (array), \
          sizeof (array) / sizeof ((array)[0]))

/*
 * BACSTAC_EMPTY_ARRAY_CONTENTS_INIT  initializes a property contents to
 * represent an empty array of the given type.
 */
#define BACSTAC_EMPTY_ARRAY_CONTENTS_INIT(tag) \
    BACSTAC_CONTENTS_INIT((tag), NULL, 0, 0)

/*
 * BACSTAC_EMPTY_CONTENTS_INIT initializes a property contents to be empty
 * with the data type set to DATA_TYPE_INVALID, so the property contents
 * will not confused with a valid empty array.
 */
#define BACSTAC_EMPTY_CONTENTS_INIT \
    BACSTAC_EMPTY_ARRAY_CONTENTS_INIT(DATA_TYPE_INVALID)

/*
 * BACSTAC_NULL_CONTENTS_INIT initializes a property contents to
 * represent ASN NULL value.
 */
#define BACSTAC_NULL_CONTENTS_INIT \
    BACSTAC_CONTENTS_INIT(DATA_TYPE_NULL, NULL, 0, 1)

/*
 * BACSTAC_CONTENTS_INIT_TO_BUF initializes a prioperty contents to the given
 * buffer with the given buffer size
 */
#define BACSTAC_CONTENTS_INIT_TO_BUF(buf, size) \
   BACSTAC_CONTENTS_INIT(DATA_TYPE_INVALID, (buf), (size), 0)


/*
 * BACSTAC_OBJ_PROP_REF
 */


typedef struct tagBACSTAC_OBJ_PROP_REF
{
    BACSTAC_OBJECT_ID   objectID;
    BACSTAC_PROPERTY_ID propID;
    BACSTAC_ARRAY_INDEX index;          /* or UNSPECIFIED */
} BACSTAC_OBJ_PROP_REF, *PBACSTAC_OBJ_PROP_REF;

#define BACSTAC_OBJ_PROP_REF_INIT_WI(objType, insNum, propID, index) \
    {BACSTAC_OBJECT_ID_INIT((objType), (insNum)), (propID), (index)}

#define BACSTAC_OBJ_PROP_REF_INIT(objType, insNum, propID) \
    BACSTAC_OBJ_PROP_REF_INIT_WI((objType), (insNum), (propID), BACSTAC_VOID_INDEX)


/*
 * BACSTAC_DEV_OBJ_PROP_REF
 */


typedef struct tagBACSTAC_DEV_OBJ_PROP_REF
{
    BACSTAC_OBJECT_ID   objectID;
    BACSTAC_PROPERTY_ID propID;
    BACSTAC_ARRAY_INDEX index;          /* or UNSPECIFIED */
    BACSTAC_BOOLEAN     fDevicePresent;
    BACSTAC_OBJECT_ID   deviceID;       /* ignored if !fDevicePresent */
} BACSTAC_DEV_OBJ_PROP_REF, *PBACSTAC_DEV_OBJ_PROP_REF;

#define BACSTAC_DEV_OBJ_PROP_REF_INIT_WI(devInstNum, objType, instNum, propID, index) \
    {BACSTAC_OBJECT_ID_INIT((objType), (instNum)), (propID), (index), \
     BACSTAC_TRUE, BACSTAC_OBJECT_ID_INIT(OBJ_DEVICE, (devInstNum))}

#define BACSTAC_DEV_OBJ_PROP_REF_INIT(devInstNum, objType, instNum, propID) \
    BACSTAC_DEV_OBJ_PROP_REF_INIT_WI((devInstNum), (objType), (instNum), (propID), \
            BACSTAC_VOID_INDEX)

#define BACSTAC_DEV_OBJ_PROP_REF_INIT_NODEV_WI(objType, instNum, propID, index) \
    {BACSTAC_OBJECT_ID_INIT((objType), (instNum)), (propID), (index), \
     BACSTAC_FALSE, BACSTAC_OBJECT_ID_INIT(OBJ_DEVICE, BACSTAC_INST_NUMBER_MAX)}

#define BACSTAC_DEV_OBJ_PROP_REF_INIT_NODEV(objType, instNum, propID) \
    BACSTAC_DEV_OBJ_PROP_REF_INIT_NODEV_WI((objType), (instNum), (propID), \
            BACSTAC_VOID_INDEX)

/*
 * BACSTAC_DEV_OBJ_REF
 */


typedef struct tagBACSTAC_DEV_OBJ_REF
{
    BACSTAC_OBJECT_ID   objectID;
    BACSTAC_BOOLEAN     fDevicePresent;
    BACSTAC_OBJECT_ID   deviceID;       /* ignored if !fDevicePresent */
} BACSTAC_DEV_OBJ_REF, *PBACSTAC_DEV_OBJ_REF;

#define BACSTAC_DEV_OBJ_REF_INIT(devInstNum, objType, instNum) \
    {BACSTAC_OBJECT_ID_INIT((objType), (instNum)), \
     BACSTAC_TRUE, BACSTAC_OBJECT_ID_INIT(OBJ_DEVICE, (devInstNum))}

#define BACSTAC_DEV_OBJ_REF_INIT_NODEV(objType, instNum) \
    {BACSTAC_OBJECT_ID_INIT((objType), (instNum)), \
     BACSTAC_FALSE, BACSTAC_OBJECT_ID_INIT(OBJ_DEVICE, BACSTAC_INST_NUMBER_MAX)}



/*
 * BACSTAC_PROPERTY_STATES
 */


typedef enum BACSTAC_PROP_STATES_TYPE
{
    PROP_STATES_TYPE_BOOLEAN = 0,
    PROP_STATES_TYPE_BINARY = 1,
    PROP_STATES_TYPE_EVENT_TYPE = 2,
    PROP_STATES_TYPE_POLARITY = 3,
    PROP_STATES_TYPE_PROGRAM_CHANGE = 4,
    PROP_STATES_TYPE_PROGRAM_STATE = 5,
    PROP_STATES_TYPE_REASON_FOR_HALT = 6,
    PROP_STATES_TYPE_RELIABILITY = 7,
    PROP_STATES_TYPE_STATE = 8,
    PROP_STATES_TYPE_SYSTEM_STATUS = 9,
    PROP_STATES_TYPE_UNITS = 10,
    PROP_STATES_TYPE_UNSIGNED = 11,
    PROP_STATES_TYPE_SAFETY_MODE = 12,                         /* Add-135c */
    PROP_STATES_TYPE_SAFETY_STATE = 13,                        /* Add-135c */
    PROP_STATES_TYPE_RESTART_REASON = 14,                      /* Add-135-2004b-5 */
    PROP_STATES_TYPE_ALARM_STATE = 15,                         /* Add-135-2004f */
    PROP_STATES_TYPE_ACTION = 16,                              /* Add-135-2008h-3 */
    PROP_STATES_TYPE_DOOR_SECURED_STATUS = 17,                 /* Add-135-2008h-3 */
    PROP_STATES_TYPE_DOOR_STATUS = 18,                         /* Add-135-2008h-3 */
    PROP_STATES_TYPE_DOOR_VALUE = 19,                          /* Add-135-2008h-3 */
    PROP_STATES_TYPE_FILE_ACCESS_METHOD = 20,                  /* Add-135-2008h-3 */
    PROP_STATES_TYPE_LOCK_STATUS = 21,                         /* Add-135-2008h-3 */
    PROP_STATES_TYPE_SAFETY_OPERATION = 22,                    /* Add-135-2008h-3 */
    PROP_STATES_TYPE_MAINTENANCE = 23,                         /* Add-135-2008h-3 */
    PROP_STATES_TYPE_NODE_TYPE = 24,                           /* Add-135-2008h-3 */
    PROP_STATES_TYPE_NOTIFY_TYPE = 25,                         /* Add-135-2008h-3 */
    PROP_STATES_TYPE_SECURITY_LEVEL = 26,                      /* Add-135-2008g */
    PROP_STATES_TYPE_SHED_STATE = 27,                          /* Add-135-2008h-3 */
    PROP_STATES_TYPE_SILENCED_STATE = 28,                      /* Add-135-2008h-3 */
    /* context tag 29 reserved for future addenda */
    PROP_STATES_TYPE_ACCESS_EVENT = 30,                        /* Add-135-2008j-1 */
    PROP_STATES_TYPE_ZONE_OCCUPANCY_STATE = 31,                /* Add-135-2008j-1 */
    PROP_STATES_TYPE_ACCESS_CRED_DISABLE_REASON = 32,          /* Add-135-2008j-1 */
    PROP_STATES_TYPE_ACCESS_CRED_DISABLE = 33,                 /* Add-135-2008j-1 */
    PROP_STATES_TYPE_AUTHN_STATUS = 34,                        /* Add-135-2008j-1 */
    PROP_STATES_TYPE_BACKUP_STATE = 36,                        /* Add-135-2008n-1 */
    PROP_STATES_TYPE_WRITE_STATUS = 37,                        /* Add-135-2010aa-1 */
    PROP_STATES_TYPE_LIGHTING_IN_PROGRESS = 38,                /* Add-135-2010i-1 */
    PROP_STATES_TYPE_LIGHTING_OPERATION = 39,                  /* Add-135-2010i-1 */
    PROP_STATES_TYPE_LIGHTING_TRANSITION = 40,                 /* Add-135-2010i-1 */
    PROP_STATES_TYPE_INTEGER = 41,                             /* Add-135-2012aw-1 */
    PROP_STATES_TYPE_BINARY_LIGHTING_PV = 42,                  /* Add-135-2012az-1 */
    PROP_STATES_TYPE_TIMER_STATE = 43,                         /* Add-135-2012ay-1 */
    PROP_STATES_TYPE_TIMER_TRANSITION = 44,                    /* Add-135-2012ay-1 */
    PROP_STATES_TYPE_BIP_MODE = 45,                            /* Add-135-2012ai-1 */
    PROP_STATES_TYPE_NET_PORT_CMD = 46,                        /* Add-135-2012ai-1 */
    PROP_STATES_TYPE_NETWORK_TYPE = 47,                        /* Add-135-2012ai-1 */
    PROP_STATES_TYPE_NET_NUMBER_QUALITY = 48,                  /* Add-135-2012ai-1 */
    PROP_STATES_TYPE_ESCALATOR_OPER_DIR = 49,                  /* Add-135-2012aq */
    PROP_STATES_TYPE_ESCALATOR_FAULT = 50,                     /* Add-135-2012aq */
    PROP_STATES_TYPE_ESCALATOR_MODE = 51,                      /* Add-135-2012aq */
    PROP_STATES_TYPE_LIFT_CAR_DIR = 52,                        /* Add-135-2012aq */
    PROP_STATES_TYPE_LIFT_CAR_DOOR_COMMAND = 53,               /* Add-135-2012aq */
    PROP_STATES_TYPE_LIFT_CAR_DRIVE_STATUS = 54,               /* Add-135-2012aq */
    PROP_STATES_TYPE_LIFT_CAR_MODE = 55,                       /* Add-135-2012aq */
    PROP_STATES_TYPE_LIFT_GROUP_MODE = 56,                     /* Add-135-2012aq */
    PROP_STATES_TYPE_LIFT_FAULT = 57,                          /* Add-135-2012aq */
    PROP_STATES_TYPE_PROTOCOL_LEVEL = 58,                      /* Add-135-2012bf */
    PROP_STATES_TYPE_AUDIT_LEVEL = 59,                         /* Add-135-2016bi */
    PROP_STATES_TYPE_AUDIT_OPERATION = 60,                     /* Add-135-2016bi */
    PROP_STATES_TYPE_SC_CONNECT_STATE = 258,                   /* Add-135-2020cc */
    PROP_STATES_TYPE_SC_HUB_CONNECTOR_STATE = 259,             /* Add-135-2020cc */
    PROP_STATES_TYPE_LAST_KNOWN = 259,
    PROP_STATES_TYPE_FIRST_PROPRIETARY = 64,
    PROP_STATES_TYPE_LAST_PROPRIETARY = 254
    BACSTAC_OTHER_ENUM(PROP_STATES_TYPE)

    /* for backward compatibility only */
#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 8))
    #define PROP_STATES_TYPE_SIGNED _Pragma("GCC warning \"PROP_STATES_TYPE_SIGNED is deprecated\"") PROP_STATES_TYPE_INTEGER
#else
    ,
    PROP_STATES_TYPE_SIGNED = PROP_STATES_TYPE_INTEGER
#endif
#ifdef _MSC_VER
#   pragma deprecated(PROP_STATES_TYPE_SIGNED)
#endif /* _MSC_VER */
} BACSTAC_PROP_STATES_TYPE;

typedef struct tagBACSTAC_PROPERTY_STATES
{
    BACSTAC_PROP_STATES_TYPE tag;
    BACSTAC_UI32             value;
} BACSTAC_PROPERTY_STATES, *PBACSTAC_PROPERTY_STATES;


/*
 * BACSTAC_EVENT_PARAMETER2
 */


typedef struct tagBACSTAC_EP_COBS_PARAM         /* change-of-bit-string */
{
    BACSTAC_UNSIGNED        timeDelay;
    BACSTAC_BIT_STRING      bitMask;
    BACSTAC_BIT_STRING      *pValues; /*= size:valueCount */
    BACSTAC_ELEMENT_COUNT   valueCount;
} BACSTAC_EP_COBS_PARAM, *PBACSTAC_EP_COBS_PARAM;

typedef struct tagBACSTAC_EP_COS_PARAM          /* change-of-state */
{
    BACSTAC_UNSIGNED        timeDelay;
    BACSTAC_PROPERTY_STATES *pValues; /*= size:valueCount */
    BACSTAC_ELEMENT_COUNT   valueCount;
} BACSTAC_EP_COS_PARAM, *PBACSTAC_EP_COS_PARAM;

typedef enum BACSTAC_EP_COV_CRITERIA_TYPE
{
    EP_COV_CRITERIA_BITMASK,
    EP_COV_CRITERIA_INCREMENT
    BACSTAC_OTHER_ENUM(EP_COV_CRITERIA_TYPE),

    /* for backward compatibility only */
#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 8))
    #define EP_COV_CRITERIA_BIT_STRING _Pragma("GCC warning \"EP_COV_CRITERIA_BIT_STRING is deprecated\"") EP_COV_CRITERIA_BITMASK
#else
    EP_COV_CRITERIA_BIT_STRING = EP_COV_CRITERIA_BITMASK
#endif
} BACSTAC_EP_COV_CRITERIA_TYPE;
/* Generate warnings for deprecated properties */
#ifdef _MSC_VER
#   pragma deprecated(EP_COV_CRITERIA_BIT_STRING)
#endif /* _MSC_VER */

typedef struct tagBACSTAC_EP_COV_PARAM          /* change-of-value */
{
    BACSTAC_UNSIGNED                timeDelay;
    BACSTAC_EP_COV_CRITERIA_TYPE    tag;
    union
    {
        BACSTAC_BIT_STRING      bitMask;
        BACSTAC_REAL            propIncrement;
    } covCriteria;
} BACSTAC_EP_COV_PARAM, *PBACSTAC_EP_COV_PARAM;

typedef struct tagBACSTAC_EP_OOR_PARAM          /* out-of-range */
{
    BACSTAC_UNSIGNED            timeDelay;
    BACSTAC_REAL                lowLimit;
    BACSTAC_REAL                highLimit;
    BACSTAC_REAL                deadband;
} BACSTAC_EP_OOR_PARAM, *PBACSTAC_EP_OOR_PARAM;

typedef struct tagBACSTAC_EP_BUFR_PARAM     /* buffer-ready NOT SUPPORTED!!! */
{
    BACSTAC_UI32        notifyThreshold;
    BACSTAC_UI32        prevNotifyCount;
} BACSTAC_EP_BUFR_PARAM, *PBACSTAC_EP_BUFR_PARAM;


typedef struct tagBACSTAC_EP_CMDF_PARAM2         /* command-failure */
{
    BACSTAC_UNSIGNED                timeDelay;
    BACSTAC_DEV_OBJ_PROP_REF        feedbackProperty;
} BACSTAC_EP_CMDF_PARAM2, *PBACSTAC_EP_CMDF_PARAM2;

typedef struct tagBACSTAC_EP_FLIMIT_PARAM2       /* floating-limit */
{
    BACSTAC_UNSIGNED                timeDelay;
    BACSTAC_DEV_OBJ_PROP_REF        setpoint;
    BACSTAC_REAL                    lowDiffLimit;
    BACSTAC_REAL                    highDiffLimit;
    BACSTAC_REAL                    deadband;
} BACSTAC_EP_FLIMIT_PARAM2, *PBACSTAC_EP_FLIMIT_PARAM2;

/* Add-135c: */
typedef struct tagBACSTAC_EP_COLS_PARAM       /* change-of-life-safety */
{
    BACSTAC_UNSIGNED                      timeDelay;
    BACSTAC_LIFE_SAFETY_STATE *pLifeSafetyAlarmValues; /*= size:safetyAlarmValueCount */
    BACSTAC_ELEMENT_COUNT                 safetyAlarmValueCount;
    BACSTAC_LIFE_SAFETY_STATE *pAlarmValues; /*= size:alarmValueCount */
    BACSTAC_ELEMENT_COUNT                 alarmValueCount;
    BACSTAC_DEV_OBJ_PROP_REF              modePropertyReference;
} BACSTAC_EP_COLS_PARAM, *PBACSTAC_EP_COLS_PARAM;

typedef struct tagBACSTAC_EP_E_PARAMETER  /* Add-135-2001a */
{
    BACSTAC_DATA_TYPE tag;
    union {
             BACSTAC_REAL                   real;
             BACSTAC_UNSIGNED               nUnsigned;
             BACSTAC_BOOLEAN                boolean;
             BACSTAC_INTEGER                integer;
             BACSTAC_DOUBLE                 fDouble;
             BACSTAC_OCTET_STRING           octetString;
             BACSTAC_CHAR_STRING            charString;
             BACSTAC_BIT_STRING             bitString;
             BACSTAC_ENUM                   enumerated;
             BACSTAC_DATE                   date;
             BACSTAC_TIME                   time;
             BACSTAC_OBJECT_ID              objectID;
             BACSTAC_DEV_OBJ_PROP_REF       reference;
    } param;
} BACSTAC_EP_E_PARAMETER, *PBACSTAC_EP_E_PARAMETER;

typedef struct tagBACSTAC_EP_EXT_PARAM  /* Add-135-2001a */
{
    BACSTAC_UI16                       vendorID;
    BACSTAC_UNSIGNED                   extendedEventType;
    BACSTAC_EP_E_PARAMETER *pParameters; /*= size:parametersCount */
    BACSTAC_ELEMENT_COUNT              parametersCount;
} BACSTAC_EP_EXT_PARAM, *PBACSTAC_EP_EXT_PARAM;

typedef struct tagBACSTAC_EP_URANGE_PARAM  /* Add-135-2001c-4 */
{
    BACSTAC_UNSIGNED                   timeDelay;
    BACSTAC_UNSIGNED                   lowLimit;
    BACSTAC_UNSIGNED                   highLimit;
} BACSTAC_EP_URANGE_PARAM, *PBACSTAC_EP_URANGE_PARAM;

typedef struct tagBACSTAC_EP_AEVENT_PARAM  /* Add-135-2008j-7 */
{
    BACSTAC_ACCESS_EVENT *pAccessEvents; /*= size:accessEventsCount */
    BACSTAC_ELEMENT_COUNT             accessEventsCount;
    BACSTAC_DEV_OBJ_PROP_REF          accessEventTimeRef;
} BACSTAC_EP_AEVENT_PARAM, *PBACSTAC_EP_AEVENT_PARAM;

typedef struct tagBACSTAC_EP_DOUBLE_OOR_PARAM  /* double-out-of-range; Add-135-2008w */
{
    BACSTAC_UNSIGNED                  timeDelay;
    BACSTAC_DOUBLE                    lowLimit;
    BACSTAC_DOUBLE                    highLimit;
    BACSTAC_DOUBLE                    deadband;
} BACSTAC_EP_DOUBLE_OOR_PARAM, *PBACSTAC_EP_DOUBLE_OOR_PARAM;

typedef struct tagBACSTAC_EP_SIGNED_OOR_PARAM  /* signed-out-of-range; Add-135-2008w */
{
    BACSTAC_UNSIGNED                  timeDelay;
    BACSTAC_INTEGER                   lowLimit;
    BACSTAC_INTEGER                   highLimit;
    BACSTAC_UNSIGNED                  deadband;
} BACSTAC_EP_SIGNED_OOR_PARAM, *PBACSTAC_EP_SIGNED_OOR_PARAM;

typedef struct tagBACSTAC_EP_UNSIGNED_OOR_PARAM  /* unsigned-out-of-range; Add-135-2008w */
{
    BACSTAC_UNSIGNED                  timeDelay;
    BACSTAC_UNSIGNED                  lowLimit;
    BACSTAC_UNSIGNED                  highLimit;
    BACSTAC_UNSIGNED                  deadband;
} BACSTAC_EP_UNSIGNED_OOR_PARAM, *PBACSTAC_EP_UNSIGNED_OOR_PARAM;

typedef struct tagBACSTAC_EP_COCS_PARAM         /* change-of-characterstring; Add-135-2008w */
{
    BACSTAC_UNSIGNED                  timeDelay;
    BACSTAC_CHAR_STRING               *pValues; /*= size:valueCount */
    BACSTAC_ELEMENT_COUNT             valueCount;
} BACSTAC_EP_COCS_PARAM, *PBACSTAC_EP_COCS_PARAM;

typedef struct tagBACSTAC_EP_COSF_PARAM         /* change-of-status-flags; Add-135-2008p */
{
    BACSTAC_UNSIGNED   timeDelay;
    BACSTAC_BIT_STRING selectedFlags;
} BACSTAC_EP_COSF_PARAM, *PBACSTAC_EP_COSF_PARAM;

typedef struct tagBACSTAC_EP_CODV_PARAM         /* change-of-discrete-value; Add-135-2012aw-2 */
{
    BACSTAC_UNSIGNED timeDelay;
} BACSTAC_EP_CODV_PARAM, *PBACSTAC_EP_CODV_PARAM;

typedef struct tagBACSTAC_EP_COT_PARAM          /* change-of-timer; Add-135-2012ay-1 */
{
    BACSTAC_UNSIGNED         timeDelay;
    BACSTAC_TIMER_STATE      *alarmValues;
    BACSTAC_ELEMENT_COUNT    valueCount;
    BACSTAC_DEV_OBJ_PROP_REF updateTimeRef;
} BACSTAC_EP_COT_PARAM, *PBACSTAC_EP_COT_PARAM;

typedef struct tagBACSTAC_EVENT_PARAMETER2
{
    BACSTAC_EVENT_TYPE  tag;
    union
    {
        BACSTAC_EP_COBS_PARAM       coBitString; /*= tag:EVENT_TYPE_CHANGE_OF_BITSTRING */
        BACSTAC_EP_COS_PARAM        coState; /*= tag:EVENT_TYPE_CHANGE_OF_STATE */
        BACSTAC_EP_COV_PARAM        coValue; /*= tag:EVENT_TYPE_CHANGE_OF_VALUE */
        BACSTAC_EP_CMDF_PARAM2      cmdFailure; /*= tag:EVENT_TYPE_COMMAND_FAILURE */
        BACSTAC_EP_FLIMIT_PARAM2    floatingLimit; /*= tag:EVENT_TYPE_FLOATING_LIMIT */
        BACSTAC_EP_OOR_PARAM        outOfRange; /*= tag:EVENT_TYPE_OUT_OF_RANGE */
        BACSTAC_EP_BUFR_PARAM       bufferReady; /*= tag:EVENT_TYPE_BUFFER_READY2 */
        BACSTAC_EP_COLS_PARAM       coLifeSafety; /*= tag:EVENT_TYPE_CHANGE_OF_LIFE_SAFETY */  /* Add-135c */
        BACSTAC_EP_EXT_PARAM        extended; /*= tag:EVENT_TYPE_EXTENDED */ /* Add-135-2001a */
        BACSTAC_EP_URANGE_PARAM     unsignedRange; /*= tag:EVENT_TYPE_UNSIGNED_RANGE */ /* Add-135-2001c-4 */
        BACSTAC_EP_AEVENT_PARAM     accessEvent; /*= tag:EVENT_TYPE_ACCESS_EVENT */ /* Add-135-2008j-7 */
        BACSTAC_EP_DOUBLE_OOR_PARAM doubleOutOfRange; /*= tag:EVENT_TYPE_DOUBLE_OUT_OF_RANGE */ /* Add-135-2008w */
        BACSTAC_EP_SIGNED_OOR_PARAM signedOutOfRange; /*= tag:EVENT_TYPE_SIGNED_OUT_OF_RANGE */ /* Add-135-2008w */
        BACSTAC_EP_UNSIGNED_OOR_PARAM unsignedOutOfRange; /*= tag:EVENT_TYPE_UNSIGNED_OUT_OF_RANGE */ /* Add-135-2008w */
        BACSTAC_EP_COCS_PARAM       coCharString; /*= tag:EVENT_TYPE_CHANGE_OF_CHARSTRING */ /* Add-135-2008w */
        BACSTAC_EP_COSF_PARAM       coStatusFlags; /*= tag:EVENT_TYPE_CHANGE_OF_STATUS_FLAGS */ /* Add-135-2008p */
        BACSTAC_EP_CODV_PARAM       coDiscreteValue; /*= tag:EVENT_TYPE_CHANGE_OF_DISCRETE_VAL */ /* Add-135-2012aw-2*/
        BACSTAC_EP_COT_PARAM        coTimer; /*= tag:EVENT_TYPE_CHANGE_OF_TIMER */ /* Add-135-2012ay-1 */
    } parameter;
} BACSTAC_EVENT_PARAMETER2, *PBACSTAC_EVENT_PARAMETER2;




/*
 * BACSTAC_DATE_RANGE
 */


typedef struct tagBACSTAC_DATE_RANGE
{
    BACSTAC_DATE    startDate;
    BACSTAC_DATE    endDate;
} BACSTAC_DATE_RANGE, *PBACSTAC_DATE_RANGE;

#define BACSTAC_DATE_RANGE_INIT(startYear, startMonth, startDayOfMonth, \
      startDayOfWeek, endYear, endMonth, endDayOfMonth, endDayOfWeek) \
    {BACSTAC_DATE_INIT((startYear), (startMonth), (startDayOfMonth), (startDayOfWeek)), \
     BACSTAC_DATE_INIT((endYear), (endMonth), (endDayOfMonth), (endDayOfWeek))}


/*
 * BACSTAC_WEEK_N_DAY
 */


typedef enum BACSTAC_WEEK_OF_MONTH
{
    WEEK_UNSPECIFIED = -1,
    WEEK_1_TO_7 = 1,
    WEEK_8_TO_14 = 2,
    WEEK_15_TO_21 = 3,
    WEEK_22_TO_28 = 4,
    WEEK_29_TO_31 = 5,
    WEEK_LAST_7 = 6,         /* last 7 days of this month */
    WEEK_PRIOR_LAST_7 = 7,   /* any of the 7 days prior to the last 7 days of this month */
    WEEK_PRIOR_LAST_14 = 8,  /* any of the 7 days prior to the last 14 days of this month */
    WEEK_PRIOR_LAST_21 = 9   /* any of the 7 days prior to the last 21 days of this month */
    BACSTAC_OTHER_ENUM(WEEK_OF_MONTH)
} BACSTAC_WEEK_OF_MONTH, *PBACSTAC_WEEK_OF_MONTH;

typedef struct tagBACSTAC_WEEK_N_DAY
{
    BACSTAC_MONTH           month;          /* | UNSPECIFIED */
    BACSTAC_WEEK_OF_MONTH   weekOfMonth;    /* | UNSPECIFIED */
    BACSTAC_DAY_OF_WEEK     dayOfWeek;      /* | UNSPECIFIED */
} BACSTAC_WEEK_N_DAY, *PBACSTAC_WEEK_N_DAY;

#define BACSTAC_WEEK_N_DAY_INIT(month, weekOfMonth, dayOfWeek) \
    {(month), (weekOfMonth), (dayOfWeek)}


/*
 * BACSTAC_CALENDAR_ENTRY
 */


typedef enum BACSTAC_CALENDAR_ENTRY_TYPE
{
    CALENDAR_ENTRY_DATE,
    CALENDAR_ENTRY_DATE_RANGE,
    CALENDAR_ENTRY_WEEK_N_DAY
    BACSTAC_OTHER_ENUM(CALENDAR_ENTRY_TYPE)
} BACSTAC_CALENDAR_ENTRY_TYPE;

typedef struct tagBACSTAC_CALENDAR_ENTRY
{
    BACSTAC_CALENDAR_ENTRY_TYPE     tag;
    union
    {
        BACSTAC_DATE        date;
        BACSTAC_DATE_RANGE  dateRange;
        BACSTAC_WEEK_N_DAY  weekNDay;
    } entry;
} BACSTAC_CALENDAR_ENTRY, *PBACSTAC_CALENDAR_ENTRY;


/*
 * BACSTAC_TIME_VALUE
 */


typedef struct tagBACSTAC_TIME_VALUE
{
    BACSTAC_TIME                time;
    BACSTAC_PROPERTY_CONTENTS   value;
} BACSTAC_TIME_VALUE, *PBACSTAC_TIME_VALUE;

#define BACSTAC_TIME_VALUE_INIT(time, contents) \
    {time, contents}

#define BACSTAC_TIME_VALUE_INIT_TO_VAR(hour, minute, second, hundredths, dataType, var) \
    {BACSTAC_TIME_INIT((hour), (minute), (second), (hundredths)), \
     BACSTAC_CONTENTS_INIT_TO_VAR((dataType), (var))}

#define BACSTAC_UNSPECIFIED_TIME_VALUE_INIT \
    BACSTAC_TIME_VALUE_INIT(BACSTAC_UNSPECIFIED_TIME_INIT, BACSTAC_EMPTY_CONTENTS_INIT)


/*
 * BACSTAC_SPECIAL_EVENT
 */


typedef enum BACSTAC_PERIOD_TYPE
{
    PERIOD_CALENDAR_ENTRY,
    PERIOD_CALENDAR_REFERENCE
    BACSTAC_OTHER_ENUM(PERIOD_TYPE)
} BACSTAC_PERIOD_TYPE;

typedef struct tagBACSTAC_SPECIAL_EVENT
{
    BACSTAC_PERIOD_TYPE     tag;
    union
    {
        BACSTAC_CALENDAR_ENTRY  calendarEntry;
        BACSTAC_OBJECT_ID       calendarReference;
    } period;
    BACSTAC_TIME_VALUE              *pEntries; /*= size:entryCount */
    BACSTAC_ELEMENT_COUNT           entryCount;
    BACSTAC_UI8                     eventPriority;  /* 1..16 */
} BACSTAC_SPECIAL_EVENT, *PBACSTAC_SPECIAL_EVENT;


/*
 * BACSTAC_PROPERTY_VALUE
 */


typedef struct tagBACSTAC_PROPERTY_VALUE
{
    BACSTAC_PROPERTY_ID         propID;
    BACSTAC_ARRAY_INDEX         index;          /* | VOID */
    BACSTAC_PROPERTY_CONTENTS   value;
    BACSTAC_PRIORITY_LEVEL      priority;       /* | VOID */
} BACSTAC_PROPERTY_VALUE, *PBACSTAC_PROPERTY_VALUE;


/*
 * BACSTAC_DEV_OBJ_PROP_VALUE
 */


typedef struct tagBACSTAC_DEV_OBJ_PROP_VALUE  /* Add-135-2001a */
{
    BACSTAC_OBJECT_ID           deviceID;
    BACSTAC_OBJECT_ID           objectID;
    BACSTAC_PROPERTY_ID         propID;
    BACSTAC_ARRAY_INDEX         index;          /* | VOID */
    BACSTAC_PROPERTY_CONTENTS   value;
} BACSTAC_DEV_OBJ_PROP_VALUE, *PBACSTAC_DEV_OBJ_PROP_VALUE;


/*
 * BACSTAC_RECIPIENT
 */


typedef enum BACSTAC_RECIPIENT_TYPE
{
    RECIPIENT_DEVICE,
    RECIPIENT_ADDRESS
    BACSTAC_OTHER_ENUM(RECIPIENT_TYPE)
} BACSTAC_RECIPIENT_TYPE;

typedef struct tagBACSTAC_RECIPIENT
{
    BACSTAC_RECIPIENT_TYPE  tag;
    union {
        BACSTAC_OBJECT_ID       device; /*= tag:RECIPIENT_DEVICE */
        BACSTAC_ADDRESS         address; /*= tag:RECIPIENT_ADDRESS */
    } recipient;
} BACSTAC_RECIPIENT, *PBACSTAC_RECIPIENT;


/*
 * BACSTAC_SETPOINT_REF
 */


typedef struct tagBACSTAC_SETPOINT_REF
{
    BACSTAC_BOOLEAN propPresent;
    BACSTAC_OBJ_PROP_REF property;
} BACSTAC_SETPOINT_REF, *PBACSTAC_SETPOINT_REF;

#define BACSTAC_SETPOINT_REF_INIT_WI(objType, insNum, propID, index) \
    {BACSTAC_TRUE, BACSTAC_OBJ_PROP_REF_INIT_WI((objType), (insNum), (propID), (index))}

#define BACSTAC_SETPOINT_REF_INIT(objType, insNum, propID) \
    BACSTAC_SETPOINT_REF_INIT_WI((objType), (insNum), (propID), BACSTAC_VOID_INDEX)

#define BACSTAC_EMPTY_SETPOINT_REF_INIT \
    {BACSTAC_FALSE, BACSTAC_OBJ_PROP_REF_INIT(BACSTAC_OBJECT_TYPE_MAX, BACSTAC_INST_NUMBER_MAX, BACSTAC_PROPERTY_ID_MAX)}


/*
 * BACSTAC_DESTINATION
 */


typedef struct tagBACSTAC_DESTINATION
{
    BACSTAC_BIT_STRING  validDays;      /* BACSTAC_DAY_OF_WEEK_BITS */
    BACSTAC_TIME        fromTime;
    BACSTAC_TIME        toTime;
    BACSTAC_RECIPIENT   recipient;
    BACSTAC_UI32        processID;
    BACSTAC_BOOLEAN     issueConfNotification;
    BACSTAC_BIT_STRING  transitions;    /* BACSTAC_EVENT_TRANSITION_BITS */
} BACSTAC_DESTINATION, *PBACSTAC_DESTINATION;


/*
 * BACSTAC_DAILY_SCHEDULE
 */


typedef struct tagBACSTAC_DAILY_SCHEDULE
{
    BACSTAC_TIME_VALUE              *pEntries; /*= size:entryCount */
    BACSTAC_ELEMENT_COUNT           entryCount;
} BACSTAC_DAILY_SCHEDULE, *PBACSTAC_DAILY_SCHEDULE;

#define BACSTAC_DAILY_SCHEDULE_INIT(pEntries, entryCount) \
    {(pEntries), (entryCount)}


/*
 * BACSTAC_TIME_STAMP
 */


typedef enum BACSTAC_TIME_STAMP_TYPE
{
    TIME_STAMP_TIME,
    TIME_STAMP_SEQ_NUMBER,
    TIME_STAMP_DATE_TIME
    BACSTAC_OTHER_ENUM(TIME_STAMP_TYPE)
} BACSTAC_TIME_STAMP_TYPE;

typedef struct tagBACSTAC_TIME_STAMP
{
    BACSTAC_TIME_STAMP_TYPE     tag;
    union
    {
        BACSTAC_TIME        time;
        BACSTAC_UI16        seqNumber;  /* 0..65535 */
        BACSTAC_DATE_TIME   dateTime;
    } stamp;
} BACSTAC_TIME_STAMP, *PBACSTAC_TIME_STAMP;


/*
 * BACSTAC_PROP_REF
 */


typedef struct tagBACSTAC_PROP_REF
{
    BACSTAC_PROPERTY_ID         propID;
    BACSTAC_ARRAY_INDEX         index;          /* | VOID */
} BACSTAC_PROP_REF, *PBACSTAC_PROP_REF;

#define BACSTAC_PROP_REF_INIT_WI(propID, index) \
    {(propID), (index)}

#define BACSTAC_PROP_REF_INIT(propID) \
    BACSTAC_PROP_REF_INIT_WI((propID), BACSTAC_VOID_INDEX)


/*
 * BACSTAC_AUTHN_FACTOR
 */


typedef struct tagBACSTAC_AUTHN_FACTOR {
    BACSTAC_AUTHN_FACTOR_TYPE formatType;
    BACSTAC_UNSIGNED         formatClass;
    BACSTAC_OCTET_STRING     value;
} BACSTAC_AUTHN_FACTOR, *PBACSTAC_AUTHN_FACTOR;

#define BACSTAC_AUTHN_FACTOR_INIT(formatType, formatClass, pOctets, octetCount) \
    {(formatType), (formatClass), BACSTAC_OCTET_STRING_INIT(pOctets, octetCount)}

/*
 * BACSTAC_NOTIFICATION_PARAMETERS
 */


typedef struct tagBACSTAC_NP_COBS_PARAM             /* change-of-bit-string */
{
    BACSTAC_BIT_STRING      refBitString;
    BACSTAC_BIT_STRING      statusFlags;        /* BACSTAC_STATUS_FLAG_BITS */
} BACSTAC_NP_COBS_PARAM, *PBACSTAC_NP_COBS_PARAM;

typedef struct tagBACSTAC_NP_COS_PARAM              /* change-of-state */
{
    BACSTAC_PROPERTY_STATES newState;
    BACSTAC_BIT_STRING      statusFlags;        /* BACSTAC_STATUS_FLAG_BITS */
} BACSTAC_NP_COS_PARAM, *PBACSTAC_NP_COS_PARAM;

typedef enum BACSTAC_NP_COV_TYPE
{
    NP_CHANGE_BIT_STRING,
    NP_CHANGE_VALUE
    BACSTAC_OTHER_ENUM(NP_COV_TYPE)
} BACSTAC_NP_COV_TYPE;

typedef struct tagBACSTAC_NP_COV_PARAM              /* change-of-value */
{
    BACSTAC_NP_COV_TYPE tag;
    union
    {
        BACSTAC_BIT_STRING  changedBits;
        BACSTAC_REAL        propIncrement;
    } newValue;
    BACSTAC_BIT_STRING      statusFlags;        /* BACSTAC_STATUS_FLAG_BITS */
} BACSTAC_NP_COV_PARAM, *PBACSTAC_NP_COV_PARAM;

typedef struct tagBACSTAC_NP_CMDF_PARAM             /* command-failure */
{
    BACSTAC_PROPERTY_CONTENTS   cmdValue;
    BACSTAC_BIT_STRING          statusFlags;    /* BACSTAC_STATUS_FLAG_BITS */
    BACSTAC_PROPERTY_CONTENTS   feedbackValue;
} BACSTAC_NP_CMDF_PARAM, *PBACSTAC_NP_CMDF_PARAM;

typedef struct tagBACSTAC_NP_FLIMIT_PARAM           /* floating-limit */
{
    BACSTAC_REAL        refValue;
    BACSTAC_BIT_STRING  statusFlags;            /* BACSTAC_STATUS_FLAG_BITS */
    BACSTAC_REAL        setpointValue;
    BACSTAC_REAL        errorLimit;
} BACSTAC_NP_FLIMIT_PARAM, *PBACSTAC_NP_FLIMIT_PARAM;

typedef struct tagBACSTAC_NP_OOR_PARAM              /* out-of-range */
{
    BACSTAC_REAL        exceedingValue;
    BACSTAC_BIT_STRING  statusFlags;            /* BACSTAC_STATUS_FLAG_BITS */
    BACSTAC_REAL        deadband;
    BACSTAC_REAL        exceededLimit;
} BACSTAC_NP_OOR_PARAM, *PBACSTAC_NP_OOR_PARAM;

typedef struct tagBACSTAC_NP_CET_PARAM              /* complex-event-type */
{
    BACSTAC_PROPERTY_VALUE  *pValues; /*= size:valueCount */
    BACSTAC_ELEMENT_COUNT   valueCount;
} BACSTAC_NP_CET_PARAM, *PBACSTAC_NP_CET_PARAM;

typedef struct tagBACSTAC_NP_BUFR_PARAM   /* buffer-ready             */
{                                         /* deprecated Add-135-2001b */
    BACSTAC_OBJECT_ID   bufferDevice;
    BACSTAC_OBJECT_ID   bufferObject;
    BACSTAC_DATE_TIME   prevNotification;
    BACSTAC_DATE_TIME   currNotification;
} BACSTAC_NP_BUFR_PARAM, *PBACSTAC_NP_BUFR_PARAM;

typedef struct tagBACSTAC_NP_COLS_PARAM  /* change-of-life-safety, Add-135c */
{
    BACSTAC_LIFE_SAFETY_STATE       newState;
    BACSTAC_LIFE_SAFETY_MODE        newMode;
    BACSTAC_BIT_STRING              statusFlags;
    BACSTAC_LIFE_SAFETY_OPERATION   operationExpected;
} BACSTAC_NP_COLS_PARAM, *PBACSTAC_NP_COLS_PARAM;

typedef struct tagBACSTAC_NP_E_PARAMETER  /* Add-135-2001a */
{
    BACSTAC_DATA_TYPE tag;
    union {
             BACSTAC_REAL               real;
             BACSTAC_UNSIGNED           nUnsigned;
             BACSTAC_BOOLEAN            boolean;
             BACSTAC_INTEGER            integer;
             BACSTAC_DOUBLE             fDouble;
             BACSTAC_OCTET_STRING       octetString;
             BACSTAC_CHAR_STRING        charString;
             BACSTAC_BIT_STRING         bitString;
             BACSTAC_ENUM               enumerated;
             BACSTAC_DATE               date;
             BACSTAC_TIME               time;
             BACSTAC_OBJECT_ID          objectID;
             BACSTAC_DEV_OBJ_PROP_VALUE propertyValue;
    } param;
} BACSTAC_NP_E_PARAMETER, *PBACSTAC_NP_E_PARAMETER;

typedef struct tagBACSTAC_NP_EXT_PARAM  /* Add-135-2001a */
{
    BACSTAC_UI16                       vendorID;
    BACSTAC_UNSIGNED                   extendedEventType;
    BACSTAC_NP_E_PARAMETER *pParameters; /*= size:parametersCount */
    BACSTAC_ELEMENT_COUNT              parametersCount;
} BACSTAC_NP_EXT_PARAM, *PBACSTAC_NP_EXT_PARAM;

typedef struct tagBACSTAC_NP_BUFR_PARAM2     /* buffer-ready, Add-135-2001b */
{
    BACSTAC_DEV_OBJ_PROP_REF bufferProperty;
    BACSTAC_UI32             prevNotification;
    BACSTAC_UI32             currNotification;
} BACSTAC_NP_BUFR_PARAM2, *PBACSTAC_NP_BUFR_PARAM2;

typedef struct tagBACSTAC_NP_URANGE_PARAM     /* unsigned-range, Add-135-2001c-4 */
{
    BACSTAC_UNSIGNED               exceedingValue;
    BACSTAC_BIT_STRING             statusFlags;
    BACSTAC_UNSIGNED               exceededLimit;
} BACSTAC_NP_URANGE_PARAM, *PBACSTAC_NP_URANGE_PARAM;

typedef struct tagBACSTAC_NP_AEVENT_PARAM     /* access-event, Add-135-2008j-7 */
{
    BACSTAC_ACCESS_EVENT accessEvent;
    BACSTAC_BIT_STRING   statusFlags;
    BACSTAC_UNSIGNED     accessEventTag;
    BACSTAC_TIME_STAMP   accessEventTime;
    BACSTAC_DEV_OBJ_REF  accessCredential;
    BACSTAC_BOOLEAN      fAuthnFactorPresent;
    BACSTAC_AUTHN_FACTOR authnFactor;
} BACSTAC_NP_AEVENT_PARAM, *PBACSTAC_NP_AEVENT_PARAM;

typedef struct tagBACSTAC_NP_DOUBLE_OOR_PARAM   /* double-out-of-range, Add-135-2008w */
{
    BACSTAC_DOUBLE     exceedingValue;
    BACSTAC_BIT_STRING statusFlags;             /* BACSTAC_STATUS_FLAG_BITS */
    BACSTAC_DOUBLE     deadband;
    BACSTAC_DOUBLE     exceededLimit;
} BACSTAC_NP_DOUBLE_OOR_PARAM, *PBACSTAC_NP_DOUBLE_OOR_PARAM;

typedef struct tagBACSTAC_NP_SIGNED_OOR_PARAM   /* signed-out-of-range, Add-135-2008w */
{
    BACSTAC_INTEGER    exceedingValue;
    BACSTAC_BIT_STRING statusFlags;             /* BACSTAC_STATUS_FLAG_BITS */
    BACSTAC_UNSIGNED   deadband;
    BACSTAC_INTEGER    exceededLimit;
} BACSTAC_NP_SIGNED_OOR_PARAM, *PBACSTAC_NP_SIGNED_OOR_PARAM;

typedef struct tagBACSTAC_NP_UNSIGNED_OOR_PARAM /* unsigned-out-of-range, Add-135-2008w */
{
    BACSTAC_UNSIGNED   exceedingValue;
    BACSTAC_BIT_STRING statusFlags;             /* BACSTAC_STATUS_FLAG_BITS */
    BACSTAC_UNSIGNED   deadband;
    BACSTAC_UNSIGNED   exceededLimit;
} BACSTAC_NP_UNSIGNED_OOR_PARAM, *PBACSTAC_NP_UNSIGNED_OOR_PARAM;

typedef struct tagBACSTAC_NP_COCS_PARAM         /* change-of-characterstring, Add-135-2008w */
{
    BACSTAC_CHAR_STRING changedValue;
    BACSTAC_BIT_STRING  statusFlags;            /* BACSTAC_STATUS_FLAG_BITS */
    BACSTAC_CHAR_STRING alarmValue;
} BACSTAC_NP_COCS_PARAM, *PBACSTAC_NP_COCS_PARAM;

typedef struct tagBACSTAC_NP_COSF_PARAM         /* change-of-status-flags, Add-135-2008p  */
{
    BACSTAC_PROPERTY_CONTENTS presentValue;     /* this field is OPTIONAL;
                                                 * if the field is omitted, then
                                                 * presentValue.tag == DATA_TYPE_INVALID */
    BACSTAC_BIT_STRING        referencedFlags;  /* BACSTAC_STATUS_FLAG_BITS */
} BACSTAC_NP_COSF_PARAM, *PBACSTAC_NP_COSF_PARAM;

typedef struct tagBACSTAC_NP_COR_PARAM          /* change-of-reliability, Add-135-2010af  */
{
    BACSTAC_RELIABILITY    reliability;
    BACSTAC_BIT_STRING     statusFlags;          /* BACSTAC_STATUS_FLAG_BITS */
    BACSTAC_PROPERTY_VALUE *pValues;  /*= size:valueCount */
    BACSTAC_ELEMENT_COUNT  valueCount;
} BACSTAC_NP_COR_PARAM, *PBACSTAC_NP_COR_PARAM;

typedef struct tagBACSTAC_NP_CODV_VALUE
{
    BACSTAC_DATA_TYPE tag;
    union {
             BACSTAC_BOOLEAN      b;
             BACSTAC_UNSIGNED     u;
             BACSTAC_INTEGER      i;
             BACSTAC_ENUM         e;
             BACSTAC_CHAR_STRING  charString;
             BACSTAC_OCTET_STRING octetString;
             BACSTAC_DATE         date;
             BACSTAC_TIME         time;
             BACSTAC_OBJECT_ID    objectID;
             BACSTAC_DATE_TIME    dateTime;

             /* for backward compatibility only */
             BACSTAC_DEPRECATED BACSTAC_INTEGER s;
    } value;
} BACSTAC_NP_CODV_VALUE, *PBACSTAC_NP_CODV_VALUE;

typedef struct tagBACSTAC_NP_CODV_PARAM          /* change-of-discrete-value, Add-135-2012aw-2  */
{
    BACSTAC_NP_CODV_VALUE  newValue;
    BACSTAC_BIT_STRING     statusFlags;          /* BACSTAC_STATUS_FLAG_BITS */
} BACSTAC_NP_CODV_PARAM, *PBACSTAC_NP_CODV_PARAM;

#define BACSTAC_NP_LAST_CHANGE       0x01
#define BACSTAC_NP_INIT_TIMEOUT      0x02
#define BACSTAC_NP_EXPIRATION_TIME   0x04

typedef struct tagBACSTAC_NP_COT_PARAM          /* change-of-timer, Add-135-2012ay-1  */
{
    BACSTAC_UINT             mask;
    BACSTAC_TIMER_STATE      newState;
    BACSTAC_BIT_STRING       statusFlags;          /* BACSTAC_STATUS_FLAG_BITS */
    BACSTAC_DATE_TIME        updateTime;
    BACSTAC_TIMER_TRANSITION lastStateChange;      /* optional */ /*= mask:BACSTAC_NP_LAST_CHANGE */
    BACSTAC_UNSIGNED         initialTimeout;       /* optional */ /*= mask:BACSTAC_NP_INIT_TIMEOUT */
    BACSTAC_DATE_TIME        expirationTime;       /* optional */ /*= mask:BACSTAC_NP_EXPIRATION_TIME */
} BACSTAC_NP_COT_PARAM, *PBACSTAC_NP_COT_PARAM;

typedef struct tagBACSTAC_NOTIFICATION_PARAMETERS
{
    BACSTAC_EVENT_TYPE  tag;
    union
    {
        BACSTAC_NP_COBS_PARAM         coBitString;
        BACSTAC_NP_COS_PARAM          coState;
        BACSTAC_NP_COV_PARAM          coValue;
        BACSTAC_NP_CMDF_PARAM         cmdFailure;
        BACSTAC_NP_FLIMIT_PARAM       floatingLimit;
        BACSTAC_NP_OOR_PARAM          outOfRange;
        BACSTAC_NP_CET_PARAM          complexEventType;
        BACSTAC_NP_BUFR_PARAM         bufferReady;  /* deprecated by Add-135-2001b */
        BACSTAC_NP_COLS_PARAM         coLifeSafety;       /* Add-135c */
        BACSTAC_NP_EXT_PARAM          extended;           /* Add-135-2001a */
        BACSTAC_NP_BUFR_PARAM2        bufferReady2;       /* Add-135-2001b */
        BACSTAC_NP_URANGE_PARAM       unsignedRange;      /* Add-135-2001c-4 */
        BACSTAC_NP_AEVENT_PARAM       accessEvent;        /* Add-135-2008j-7 */
        BACSTAC_NP_DOUBLE_OOR_PARAM   doubleOutOfRange;   /* Add-135-2008w */
        BACSTAC_NP_SIGNED_OOR_PARAM   signedOutOfRange;   /* Add-135-2008w */
        BACSTAC_NP_UNSIGNED_OOR_PARAM unsignedOutOfRange; /* Add-135-2008w */
        BACSTAC_NP_COCS_PARAM         coCharString;       /* Add-135-2008w */
        BACSTAC_NP_COSF_PARAM         coStatusFlags;      /* Add-135-2008p */
        BACSTAC_NP_COR_PARAM          coReliability;      /* Add-135-2010af */
        BACSTAC_NP_CODV_PARAM         coDiscreteValue;    /* Add-135-2012aw-2 */
        BACSTAC_NP_COT_PARAM          coTimer;            /* Add-135-2012ay-1 */
    } parameters;
} BACSTAC_NOTIFICATION_PARAMETERS, *PBACSTAC_NOTIFICATION_PARAMETERS;


/*
 * BACSTAC_SHED_LEVEL
 */

typedef enum BACSTAC_SHED_LEVEL_TYPE
{
    LEVEL_TYPE_PERCENT,
    LEVEL_TYPE_LEVEL,
    LEVEL_TYPE_AMOUNT
    BACSTAC_OTHER_ENUM(SHED_LEVEL_TYPE)
} BACSTAC_SHED_LEVEL_TYPE;

typedef struct tagBACSTAC_SHED_LEVEL
{
    BACSTAC_SHED_LEVEL_TYPE    tag;
    union
    {
        BACSTAC_UNSIGNED percent; /*= tag:LEVEL_TYPE_PERCENT */
        BACSTAC_UNSIGNED level;   /*= tag:LEVEL_TYPE_LEVEL */
        BACSTAC_REAL     amount;  /*= tag:LEVEL_TYPE_AMOUNT */
    } level;
} BACSTAC_SHED_LEVEL, *PBACSTAC_SHED_LEVEL;


/*
 * BACSTAC_VT_SESSION
 */

typedef struct tagBACSTAC_VT_SESSION
{
    BACSTAC_UI8     localVtSessionID;
    BACSTAC_UI8     remoteVtSessionID;
    BACSTAC_ADDRESS remoteVtAddress;
} BACSTAC_VT_SESSION, *PBACSTAC_VT_SESSION;

#define BACSTAC_VT_SESSION_INIT(localVtSessionID, remoteVtSessionID, remoteVtAddress) \
    {localVtSessionID, remoteVtSessionID, remoteVtAddress}


/*
 * BACSTAC_SESSION_KEY
 */

#define ENCRYPTION_KEY_SIZE 8 /* 56 bits for key, 8 bits for checksum */
typedef struct tagBACSTAC_SESSION_KEY
{
    BACSTAC_BYTE    sessionKey[ENCRYPTION_KEY_SIZE];
    BACSTAC_ADDRESS peerAddress;
} BACSTAC_SESSION_KEY, *PBACSTAC_SESSION_KEY;

#define BACSTAC_SESSION_KEY_INIT(key0, key1, key2, key3, key4, key5, key6, key7, address) \
    {{key0, key1, key2, key3, key4, key5, key6, key7}, address}


/*
 * BACSTAC_WHO_HAS_TYPE defines the object choice within a device.
 */

typedef enum BACSTAC_WHO_HAS_TYPE
{
    WHO_HAS_ID,   /* select object by its identifier */
    WHO_HAS_NAME  /* select object by its name */
    BACSTAC_OTHER_ENUM(WHO_HAS_TYPE)
} BACSTAC_WHO_HAS_TYPE;

typedef struct tagBACSTAC_WHO_HAS_PARAM {
    BACSTAC_WHO_HAS_TYPE    tag;
    union {
        BACSTAC_OBJECT_ID   objectID;   /*= tag:WHO_HAS_ID   */
        BACSTAC_CHAR_STRING objectName; /*= tag:WHO_HAS_NAME */
    } choice;
} BACSTAC_WHO_HAS_PARAM, *PBACSTAC_WHO_HAS_PARAM;


/*
 * Who-Has Request, which contains an optional instance range and
 * the object choice criterion.
 */

typedef struct tagBACSTAC_WHO_HAS_HOOK_INFO
{
    BACSTAC_INST_NUMBER         filterMin;  /* | BACSTAC_NO_FILTER */
    BACSTAC_INST_NUMBER         filterMax;  /* | BACSTAC_NO_FILTER */
    BACSTAC_WHO_HAS_PARAM       object;
} BACSTAC_WHO_HAS_HOOK_INFO, *PBACSTAC_WHO_HAS_HOOK_INFO;

/* for backward compatibility */
typedef BACSTAC_DEPRECATED BACSTAC_WHO_HAS_TYPE BACSTAC_WH_HI_OBJ_SPEC_TYPE;
#define WH_HI_OBJ_ID WHO_HAS_ID
#define WH_HI_OBJ_NAME WHO_HAS_NAME

/*
 * Who-Is helper datatypes (for SERVER and GATEWAY)
 */


typedef struct tagBACSTAC_WHO_IS_HOOK_INFO
{
    BACSTAC_INST_NUMBER     filterMin;  /* | BACSTAC_NO_FILTER */
    BACSTAC_INST_NUMBER     filterMax;  /* | BACSTAC_NO_FILTER */
} BACSTAC_WHO_IS_HOOK_INFO, *PBACSTAC_WHO_IS_HOOK_INFO;



/*
 * ReadProperty
 */


typedef struct tagBACSTAC_READ_INFO
{
    BACSTAC_OBJECT_ID   objectID;
    BACSTAC_PROPERTY_ID ePropertyID;
    BACSTAC_ARRAY_INDEX nIndex;
} BACSTAC_READ_INFO, *PBACSTAC_READ_INFO;

/* For backward compatibility */
typedef BACSTAC_DEPRECATED BACSTAC_READ_INFO  BACSTAC_READ_HOOK_INFO;
typedef BACSTAC_DEPRECATED PBACSTAC_READ_INFO PBACSTAC_READ_HOOK_INFO;


/*
 * WriteProperty
 */


typedef struct tagBACSTAC_WRITE_INFO
{
    BACSTAC_OBJECT_ID         objectID;
    BACSTAC_PROPERTY_ID       ePropertyID;
    BACSTAC_ARRAY_INDEX       nIndex;
    BACSTAC_PRIORITY_LEVEL    nWritePriority;
    BACSTAC_PROPERTY_CONTENTS *pValue;
} BACSTAC_WRITE_INFO, *PBACSTAC_WRITE_INFO;

/* For backward compatibility */
typedef BACSTAC_DEPRECATED BACSTAC_WRITE_INFO  BACSTAC_WRITE_HOOK_INFO;
typedef BACSTAC_DEPRECATED PBACSTAC_WRITE_INFO PBACSTAC_WRITE_HOOK_INFO;


/*
 * TimeSync & UTCTimeSync
 */


typedef struct tagBACSTAC_TIME_SYNC_INFO
{
    BACSTAC_DATE_TIME   dateTime;
} BACSTAC_TIME_SYNC_INFO, *PBACSTAC_TIME_SYNC_INFO;

/* For backward compatibility */
typedef BACSTAC_DEPRECATED BACSTAC_TIME_SYNC_INFO  BACSTAC_TIME_SYNC_HOOK_INFO;
typedef BACSTAC_DEPRECATED PBACSTAC_TIME_SYNC_INFO PBACSTAC_TIME_SYNC_HOOK_INFO;

/* UTC variant */

typedef BACSTAC_TIME_SYNC_INFO          BACSTAC_UTC_TIME_SYNC_INFO;
typedef PBACSTAC_TIME_SYNC_INFO         PBACSTAC_UTC_TIME_SYNC_INFO;


/*
 * COV notifications
 */


typedef struct tagBACSTAC_SUBSCRIBE_COV_INFO
{
    BACSTAC_BOOLEAN     fSubscribe; /* as opposite to cancel */
    BACSTAC_UI32        processID;
    BACSTAC_OBJECT_ID   monitoredObjectID;

    /* These two are not used if !fSubscribe */
    BACSTAC_BOOLEAN     issueConfirmedNotifications;
    BACSTAC_UNSIGNED    lifetime;   /* BACSTAC_INFINITE_LIFETIME for indefinite */
} BACSTAC_SUBSCRIBE_COV_INFO, *PBACSTAC_SUBSCRIBE_COV_INFO;

#define BACSTAC_INFINITE_LIFETIME   ((BACSTAC_UNSIGNED) 0)

/* For backward compatibility */
typedef BACSTAC_DEPRECATED BACSTAC_SUBSCRIBE_COV_INFO  BACSTAC_COV_CONTEXT;
typedef BACSTAC_DEPRECATED PBACSTAC_SUBSCRIBE_COV_INFO PBACSTAC_COV_CONTEXT;

typedef struct tagBACSTAC_COV_NOTIF_INFO
{
    BACSTAC_UI32        processID;
    BACSTAC_OBJECT_ID   initDeviceID;
    BACSTAC_OBJECT_ID   monitoredObjectID;
    BACSTAC_UNSIGNED    timeRemaining;
    BACSTAC_PROPERTY_VALUE *listOfValues; /*= size:nValueCount */
    BACSTAC_PROPERTY_COUNT nValueCount;
} BACSTAC_COV_NOTIF_INFO, *PBACSTAC_COV_NOTIF_INFO;


/*
 * Event notifications
 */

typedef struct tagBACSTAC_EVENT_NOTIF {
    BACSTAC_UI32                    processID;
    BACSTAC_OBJECT_ID               initDevice;
    BACSTAC_OBJECT_ID               initObject;
    BACSTAC_TIME_STAMP              timeStamp;
    BACSTAC_UNSIGNED                notifClass;
    BACSTAC_UI8                     priority;
    BACSTAC_EVENT_TYPE              eventType;
    BACSTAC_CHAR_STRING             msgText;     /* OPTIONAL */
    BACSTAC_NOTIFY_TYPE             notifyType;
    BACSTAC_BOOLEAN                 ackRequired; /* only for notify type alarm or event */
    BACSTAC_EVENT_STATE             fromState;   /* only for notify type alarm or event */
    BACSTAC_EVENT_STATE             toState;
    BACSTAC_NOTIFICATION_PARAMETERS eventValues; /* only for notify type alarm or event */
} BACSTAC_EVENT_NOTIF, *PBACSTAC_EVENT_NOTIF;


/*
 * AcknowledgeAlarm
 */


typedef struct tagBACSTAC_ACK_ALARM_INFO {
    BACSTAC_UI32        processID;
    BACSTAC_OBJECT_ID   eventObjectID;
    BACSTAC_EVENT_STATE eventState;
    BACSTAC_TIME_STAMP  timeStamp;
    BACSTAC_CHAR_STRING ackSource;
    BACSTAC_TIME_STAMP  ackTime;
} BACSTAC_ACK_ALARM_INFO, *PBACSTAC_ACK_ALARM_INFO;

/* For backward compatibility */
typedef BACSTAC_DEPRECATED BACSTAC_ACK_ALARM_INFO  BACSTAC_ACK_ALARM;
typedef BACSTAC_DEPRECATED PBACSTAC_ACK_ALARM_INFO PBACSTAC_ACK_ALARM;


/*
 * GetAlarmSummary
 */


typedef struct tagBACSTAC_ALARM_SUMMARY {
    BACSTAC_OBJECT_ID   objectID;
    BACSTAC_EVENT_STATE alarmState;
    BACSTAC_BIT_STRING  ackedTransitions;
} BACSTAC_ALARM_SUMMARY, *PBACSTAC_ALARM_SUMMARY;


/*
 * GetEnrollmentSummary
 */


typedef enum BACSTAC_ACK_FILTER
{
    ACK_FILTER_ALL = 0,
    ACK_FILTER_ACKED,
    ACK_FILTER_NOT_ACKED
    BACSTAC_OTHER_ENUM(ACK_FILTER)
} BACSTAC_ACK_FILTER;

typedef struct tagBACSTAC_RECIPIENT_PROCESS {
    BACSTAC_RECIPIENT   recipient;
    BACSTAC_UI32        processID;
} BACSTAC_RECIPIENT_PROCESS, *PBACSTAC_RECIPIENT_PROCESS;

typedef enum BACSTAC_STATE_FILTER
{
    STATE_FILTER_OFFNORMAL,
    STATE_FILTER_FAULT,
    STATE_FILTER_NORMAL,
    STATE_FILTER_ALL,
    STATE_FILTER_ACTIVE
    BACSTAC_OTHER_ENUM(STATE_FILTER)
} BACSTAC_STATE_FILTER;

#define BACSTAC_EF_ENROLLMENT       0x01
#define BACSTAC_EF_EVENT_STATE      0x02
#define BACSTAC_EF_EVENT_TYPE       0x04
#define BACSTAC_EF_PRIORITY         0x08
#define BACSTAC_EF_NOTIF_CLASS      0x10

typedef struct tagBACSTAC_ENROLLMENT_FILTER {
    BACSTAC_UINT                mask;
    BACSTAC_ACK_FILTER          acknowledgement;
    BACSTAC_RECIPIENT_PROCESS   enrollment;         /* optional */ /*= mask:BACSTAC_EF_ENROLLMENT  */
    BACSTAC_STATE_FILTER        eventState;         /* optional */ /*= mask:BACSTAC_EF_EVENT_STATE */
    BACSTAC_EVENT_TYPE          eventType;          /* optional */ /*= mask:BACSTAC_EF_EVENT_TYPE  */
    BACSTAC_UI8                 minPriority;        /* optional */ /*= mask:BACSTAC_EF_PRIORITY    */
    BACSTAC_UI8                 maxPriority;        /* optional */ /*= mask:BACSTAC_EF_PRIORITY    */
    BACSTAC_UNSIGNED            notificationClass;  /* optional */ /*= mask:BACSTAC_EF_NOTIF_CLASS */
} BACSTAC_ENROLLMENT_FILTER, *PBACSTAC_ENROLLMENT_FILTER;

typedef struct tagBACSTAC_ENROLLMENT_SUMMARY {
    BACSTAC_BOOLEAN         fNotClassPresent;
    BACSTAC_OBJECT_ID       objectID;
    BACSTAC_EVENT_TYPE      eventType;
    BACSTAC_EVENT_STATE     eventState;
    BACSTAC_UI8             priority;
    BACSTAC_UNSIGNED        notificationClass;
} BACSTAC_ENROLLMENT_SUMMARY, *PBACSTAC_ENROLLMENT_SUMMARY;


/*
 * IAm
 */


typedef struct tagBACSTAC_I_AM_INFO
{
    BACSTAC_INST_NUMBER     deviceNumber;
    BACSTAC_UNSIGNED        maxAPDULengthAccepted;
    BACSTAC_SEGMENTATION    segmentationSupported;
    BACSTAC_UI16            vendorID;
} BACSTAC_I_AM_INFO, *PBACSTAC_I_AM_INFO;

/* For backward compatibility */
typedef BACSTAC_DEPRECATED BACSTAC_I_AM_INFO  BACSTAC_I_AM_HOOK_INFO;
typedef BACSTAC_DEPRECATED PBACSTAC_I_AM_INFO PBACSTAC_I_AM_HOOK_INFO;


/*
 * IHave
 */


typedef struct tagBACSTAC_I_HAVE_INFO
{
    BACSTAC_INST_NUMBER deviceNumber;
    BACSTAC_OBJECT_ID   objectID;
    BACSTAC_CHAR_STRING objectName;
} BACSTAC_I_HAVE_INFO, *PBACSTAC_I_HAVE_INFO;

/* For backward compatibility */
typedef BACSTAC_DEPRECATED BACSTAC_I_HAVE_INFO  BACSTAC_I_HAVE_HOOK_INFO;
typedef BACSTAC_DEPRECATED PBACSTAC_I_HAVE_INFO PBACSTAC_I_HAVE_HOOK_INFO;


/*
 * AddListElement
 */


typedef struct tagBACSTAC_CHANGE_LIST_INFO
{
    BACSTAC_OBJECT_ID         objectID;
    BACSTAC_PROPERTY_ID       ePropertyID;
    BACSTAC_ARRAY_INDEX       nIndex;     /* | VOID */
    BACSTAC_PROPERTY_CONTENTS *pElements;
} BACSTAC_CHANGE_LIST_INFO, *PBACSTAC_CHANGE_LIST_INFO;

/* for backward compatibility */
typedef BACSTAC_DEPRECATED BACSTAC_CHANGE_LIST_INFO  BACSTAC_ADD_ELEMENT_INFO;
typedef BACSTAC_DEPRECATED PBACSTAC_CHANGE_LIST_INFO PBACSTAC_ADD_ELEMENT_INFO;
typedef BACSTAC_DEPRECATED BACSTAC_CHANGE_LIST_INFO  BACSTAC_REMOVE_ELEMENT_INFO;
typedef BACSTAC_DEPRECATED PBACSTAC_CHANGE_LIST_INFO PBACSTAC_REMOVE_ELEMENT_INFO;


/*
 * ReinitializeDevice
 */


#define BACSTAC_MAX_RD_PASSWORD_CHARS       20
/*
 * The BACnet standard says that the password is up to 20 characters,
 * but it does not specify what exactly is meant by characters. It
 * could be either 20 bytes, or 20 Unicode character points, or
 * composed characters. So, it is not clear how much space should be
 * reserved here. So we use some value should be enough in practice.
 */
#define BACSTAC_MAX_REINIT_DEV_PASSWORD_SIZE 120

typedef struct tagBACSTAC_REINIT_DEV_INFO
{
    BACSTAC_CHAR_STRING     password;
    BACSTAC_REINIT_TYPE     reinitType;
    /* For backward compatibility, we need to provide some buffer here. */
    union BACSTAC_REINIT_DEV_INFO_BUF
    {
       BACSTAC_CHAR         chars[(BACSTAC_MAX_REINIT_DEV_PASSWORD_SIZE)/1 + 1];
       BACSTAC_UCS2_CHAR    ucs2s[(BACSTAC_MAX_REINIT_DEV_PASSWORD_SIZE)/2 + 1];
       BACSTAC_UCS4_CHAR    ucs4s[(BACSTAC_MAX_REINIT_DEV_PASSWORD_SIZE)/4 + 1];
    } buffer;
} BACSTAC_REINIT_DEV_INFO, *PBACSTAC_REINIT_DEV_INFO;


/*
 * APDU properties
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


#define APDU_MAX_LENGTH_FLAG        	0x01u
#define APDU_SEGMENTATION_FLAG      	0x02u
#define APDU_MAX_SEGMENTS_FLAG      	0x04u
#define APDU_WINDOW_SIZE_FLAG       	0x08u
#define APDU_SEGMENT_TIMEOUT_FLAG   	0x10u
#define APDU_TIMEOUT_FLAG           	0x20u
#define APDU_NUM_RETRIES_FLAG       	0x40u
#define APDU_MAX_SEGMENTS_TO_SEND_FLAG	0x80u

#define APDU_ALL_FLAGS              	0xFFu

typedef struct tagBACSTAC_APDU_PROPERTIES {
    BACSTAC_UINT          mask;
    BACSTAC_UINT          nMaxLength;
    BACSTAC_UINT          nMaxSegments;
    BACSTAC_UINT          nWindowSize;
    BACSTAC_UINT          nSegmentTimeout;
    BACSTAC_UINT          nTimeout;
    BACSTAC_UINT          nRetryCount;
    BACSTAC_SEGMENTATION  eSegmentation;
    BACSTAC_UINT          nMaxSegmentsToSend;
} BACSTAC_APDU_PROPERTIES, *PBACSTAC_APDU_PROPERTIES;


/*
 * BACSTAC_READ_ACCESS_SPEC
 */


typedef struct tagBACSTAC_READ_ACCESS_SPEC
{
    BACSTAC_OBJECT_ID      objectID;
    BACSTAC_PROPERTY_COUNT nItemCount;
    BACSTAC_PROP_REF       *readItems; /*= size:nItemCount */
} BACSTAC_READ_ACCESS_SPEC, *PBACSTAC_READ_ACCESS_SPEC;

#define BACSTAC_READ_ACCESS_SPEC_INIT(objType, instNum, pItems, itemCount) \
    {BACSTAC_OBJECT_ID_INIT((objType), (instNum)), (itemCount), (pItems)}

/* for backward compatibility */
typedef BACSTAC_DEPRECATED BACSTAC_READ_ACCESS_SPEC  BACSTAC_READ_HOOK_LIST;
typedef BACSTAC_DEPRECATED PBACSTAC_READ_ACCESS_SPEC PBACSTAC_READ_HOOK_LIST;


typedef struct tagBACSTAC_READ_MUL_HOOK_INFO
{
    BACSTAC_OBJECT_COUNT     nListCount;
    BACSTAC_READ_ACCESS_SPEC *readSpec; /*= size:nListCount */
} BACSTAC_READ_MUL_HOOK_INFO, *PBACSTAC_READ_MUL_HOOK_INFO;


/*
 * RPM result (normal + hook + raw)
 */


typedef struct tagBACSTAC_READ_RESULT_ITEM
{
    BACSTAC_PROPERTY_ID         propID;
    BACSTAC_ARRAY_INDEX         index;      /* or VOID */
    BACSTAC_BOOLEAN             status;
    BACSTAC_PROPERTY_CONTENTS   value;      /* valid if status == BACSTAC_TRUE */
    BACSTAC_ERROR_TYPE          error;      /* valid if status == BACSTAC_FALSE */
} BACSTAC_READ_RESULT_ITEM, *PBACSTAC_READ_RESULT_ITEM;


typedef struct tagBACSTAC_READ_RAW_RESULT_LIST
{
    BACSTAC_OBJECT_ID        objectID;
    BACSTAC_PROPERTY_COUNT   nResultBufferCount;
    BACSTAC_PROPERTY_COUNT   nResultActualCount;
    BACSTAC_READ_RESULT_ITEM *readResults;
} BACSTAC_READ_RAW_RESULT_LIST, *PBACSTAC_READ_RAW_RESULT_LIST;


/*
 * WPM
 */


typedef struct tagBACSTAC_WRITE_ACCESS_SPEC
{
    BACSTAC_OBJECT_ID      objectID;
    BACSTAC_PROPERTY_COUNT nItemCount;
    BACSTAC_PROPERTY_VALUE *writeItems; /*= size:nItemCount */
} BACSTAC_WRITE_ACCESS_SPEC, *PBACSTAC_WRITE_ACCESS_SPEC;

typedef struct tagBACSTAC_WRITE_PROP_MUL_INFO
{
    BACSTAC_OBJECT_COUNT      nCount;
    BACSTAC_WRITE_ACCESS_SPEC *writeSpec; /*= size:nCount */
} BACSTAC_WRITE_PROP_MUL_INFO, *PBACSTAC_WRITE_PROP_MUL_INFO;


/* for backward compatibility */
typedef BACSTAC_DEPRECATED BACSTAC_WRITE_ACCESS_SPEC BACSTAC_WRITE_HOOK_LIST;
typedef BACSTAC_DEPRECATED PBACSTAC_WRITE_ACCESS_SPEC PBACSTAC_WRITE_HOOK_LIST;
typedef BACSTAC_DEPRECATED BACSTAC_PROPERTY_VALUE BACSTAC_WRITE_ITEM;
typedef BACSTAC_DEPRECATED PBACSTAC_PROPERTY_VALUE PBACSTAC_WRITE_ITEM;
typedef BACSTAC_DEPRECATED BACSTAC_WRITE_PROP_MUL_INFO BACSTAC_WRITE_MUL_HOOK_INFO;
typedef BACSTAC_DEPRECATED PBACSTAC_WRITE_PROP_MUL_INFO PBACSTAC_WRITE_MUL_HOOK_INFO;

/*
 * DCC
 */


#define BACSTAC_DCC_INDEFINITE          0

#define BACSTAC_MAX_DCC_PASSWORD_CHARS      20
/*
 * The BACnet standard says that the password is up to 20 characters,
 * but it does not specify what exactly is meant by characters. It
 * could be either 20 bytes, or 20 Unicode character points, or
 * composed characters. So, it is not clear how much space should be
 * reserved here. So we use some value should be enough in practice.
 */
#define BACSTAC_MAX_DCC_PASSWORD_SIZE      120

typedef struct tagBACSTAC_DCC_INFO {
    BACSTAC_UNSIGNED    nTimeDuration;  /* may be INDEFINITE */
    BACSTAC_DCC_VALUE   dccValue;
    BACSTAC_CHAR_STRING password;
    /* For backward compatibility, we need to provide some buffer here. */
    union BACSTAC_DCC_INFO_BUF
    {
       BACSTAC_CHAR         chars[(BACSTAC_MAX_DCC_PASSWORD_SIZE)/1 + 1];
       BACSTAC_UCS2_CHAR    ucs2s[(BACSTAC_MAX_DCC_PASSWORD_SIZE)/2 + 1];
       BACSTAC_UCS4_CHAR    ucs4s[(BACSTAC_MAX_DCC_PASSWORD_SIZE)/4 + 1];
    } buffer;
} BACSTAC_DCC_INFO, *PBACSTAC_DCC_INFO;


/*
 * "Proprietary" section
 */


typedef struct
{
    BACSTAC_VALUE_SIZE nLength;
    BACSTAC_BYTE       *rawData; /*= size:nLength */
} BACSTAC_RAW_ASN1_VALUE, *PBACSTAC_RAW_ASN1_VALUE;

#define BACSTAC_RAW_ASN1_VALUE_INIT(asnData, asnLength) \
    {(asnLength), (asnData)}

#define BACSTAC_EMPTY_RAW_ASN1_VALUE_INIT \
    BACSTAC_RAW_ASN1_VALUE_INIT(NULL, 0)


/*
 * ReadRange
 */

/* The 'Reference Index' parameter specifies the index of the first (if
 * 'Count' is positive) or last (if 'Count' is negative) item to be read. */

#ifdef BACSTAC_USE_64_BIT_READ_RANGE
   #define BACSTAC_READ_RANGE_RANGE      BACSTAC_READ_RANGE_64_RANGE
   #define BACSTAC_READ_RANGE_INFO       BACSTAC_READ_RANGE_64_INFO
   #define BACSTAC_READ_RANGE_RAW_RESULT BACSTAC_READ_RANGE_64_RAW_RESULT

   #define BACSTAC_REF_INDEX             BACSTAC_UI64
   #define BACSTAC_REF_INDEX_MAX         BACSTAC_UI64_MAX
   #define BACSTAC_REF_INDEX_C(c)        BACSTAC_UI64_C(c)

   #define BACSTAC_SEQ_NUM               BACSTAC_UI64
   #define BACSTAC_SEQ_NUM_MAX           BACSTAC_UI64_MAX
   #define BACSTAC_SEQ_NUM_C(c)          BACSTAC_UI64_C(c)
#else /* BACSTAC_USE_64_BIT_READ_RANGE */
   #define BACSTAC_READ_RANGE_RANGE      BACSTAC_READ_RANGE_32_RANGE
   #define BACSTAC_READ_RANGE_INFO       BACSTAC_READ_RANGE_32_INFO
   #define BACSTAC_READ_RANGE_RAW_RESULT BACSTAC_READ_RANGE_32_RAW_RESULT

   #define BACSTAC_REF_INDEX             BACSTAC_UI32
   #define BACSTAC_REF_INDEX_MAX         BACSTAC_UI32_MAX
   #define BACSTAC_REF_INDEX_C(c)        BACSTAC_UI32_C(c)

   #define BACSTAC_SEQ_NUM               BACSTAC_UI32
   #define BACSTAC_SEQ_NUM_MAX           BACSTAC_UI32_MAX
   #define BACSTAC_SEQ_NUM_C(c)          BACSTAC_UI32_C(c)
#endif /* BACSTAC_USE_64_BIT_READ_RANGE */

#define BACSTAC_INVALID_SEQ_NUM       BACSTAC_SEQ_NUM_C(0)
/* for backward compatibility */
#define BACSTAC_INVALID_SEQUENCE_NUM  BACSTAC_INVALID_SEQ_NUM

typedef enum BACSTAC_RANGE_TYPE
{
    BACSTAC_RANGE_NONE,
    BACSTAC_RANGE_BY_POSITION,
    BACSTAC_RANGE_BY_SEQUENCE_NUM, /* Add-135-2001b */
    BACSTAC_RANGE_BY_TIME2         /* Add-135-2001b */
    BACSTAC_OTHER_ENUM(RANGE_TYPE)
} BACSTAC_RANGE_TYPE;

typedef union tagBACSTAC_READ_RANGE_32_RANGE {
    struct tagBACSTAC_READ_RANGE_32_POSITION {
        BACSTAC_UI32       nRefIndex;
        BACSTAC_INTEGER    nCount;
    } position;              /*= tag:BACSTAC_RANGE_BY_POSITION */
    struct tagBACSTAC_READ_RANGE_32_TIME {
        BACSTAC_DATE_TIME   refTime;
        BACSTAC_INTEGER     nCount;
    } time;                  /*= tag:BACSTAC_RANGE_BY_TIME2 */
    struct tagBACSTAC_READ_RANGE_32_SEQ_NUM {
                            /* Add-135-2001b                      */
        BACSTAC_UI32        nRefSequenceNum;
        BACSTAC_INTEGER     nCount;
    } sequenceNum;          /*= tag:BACSTAC_RANGE_BY_SEQUENCE_NUM */
} BACSTAC_READ_RANGE_32_RANGE, *PBACSTAC_READ_RANGE_32_RANGE;

typedef struct tagBACSTAC_READ_RANGE_32_INFO {
    BACSTAC_OBJECT_ID           objectID;
    BACSTAC_PROPERTY_ID         ePropertyID;
    BACSTAC_ARRAY_INDEX         nIndex;
    BACSTAC_RANGE_TYPE          tag;
    BACSTAC_READ_RANGE_32_RANGE range;
} BACSTAC_READ_RANGE_32_INFO, *PBACSTAC_READ_RANGE_32_INFO;

typedef struct tagBACSTAC_READ_RANGE_32_RAW_RESULT {
    BACSTAC_OBJECT_ID           objectID;
    BACSTAC_PROPERTY_ID         ePropertyID;
    BACSTAC_ARRAY_INDEX         nIndex;
    BACSTAC_BIT_STRING          resultFlags;
    BACSTAC_PROPERTY_CONTENTS   itemData;
    /* Add-135-2001b: */
    BACSTAC_UI32                firstSequenceNum;    /* | BACSTAC_INVALID_SEQ_NUM */
} BACSTAC_READ_RANGE_32_RAW_RESULT, *PBACSTAC_READ_RANGE_32_RAW_RESULT;

typedef union tagBACSTAC_READ_RANGE_64_RANGE {
    struct tagBACSTAC_READ_RANGE_64_POSITION {
        BACSTAC_UI64       nRefIndex;
        BACSTAC_INTEGER    nCount;
    } position;              /*= tag:BACSTAC_RANGE_BY_POSITION */
    struct tagBACSTAC_READ_RANGE_64_TIME {
        BACSTAC_DATE_TIME   refTime;
        BACSTAC_INTEGER     nCount;
    } time;                  /*= tag:BACSTAC_RANGE_BY_TIME2 */
    struct tagBACSTAC_READ_RANGE_64_SEQ_NUM {
                            /* Add-135-2001b                      */
        BACSTAC_UI64        nRefSequenceNum;
        BACSTAC_INTEGER     nCount;
    } sequenceNum;          /*= tag:BACSTAC_RANGE_BY_SEQUENCE_NUM */
} BACSTAC_READ_RANGE_64_RANGE, *PBACSTAC_READ_RANGE_64_RANGE;

typedef struct tagBACSTAC_READ_RANGE_64_INFO {
    BACSTAC_OBJECT_ID           objectID;
    BACSTAC_PROPERTY_ID         ePropertyID;
    BACSTAC_ARRAY_INDEX         nIndex;
    BACSTAC_RANGE_TYPE          tag;
    BACSTAC_READ_RANGE_64_RANGE range;
} BACSTAC_READ_RANGE_64_INFO, *PBACSTAC_READ_RANGE_64_INFO;

typedef struct tagBACSTAC_READ_RANGE_64_RAW_RESULT {
    BACSTAC_OBJECT_ID           objectID;
    BACSTAC_PROPERTY_ID         ePropertyID;
    BACSTAC_ARRAY_INDEX         nIndex;
    BACSTAC_BIT_STRING          resultFlags;
    BACSTAC_PROPERTY_CONTENTS   itemData;
    /* Add-135-2001b: */
    BACSTAC_UI64                firstSequenceNum;    /* | BACSTAC_INVALID_SEQ_NUM */
} BACSTAC_READ_RANGE_64_RAW_RESULT, *PBACSTAC_READ_RANGE_64_RAW_RESULT;



/*
 * TrendLog
 */


typedef struct tagBACSTAC_CLIENT_COV {
    BACSTAC_DATA_TYPE   tag;            /* REAL or NULL */
    BACSTAC_REAL        realIncrement;  /* if selected by tag */
} BACSTAC_CLIENT_COV, *PBACSTAC_CLIENT_COV;

#define BACSTAC_NULL_CLIENT_COV_INIT \
    {DATA_TYPE_NULL, 0}

#define BACSTAC_REAL_CLIENT_COV_INIT(increment) \
    {DATA_TYPE_REAL, (increment)}

typedef enum BACSTAC_LOG_RECORD_TYPE
{
    LOG_RECORD_STATUS = 0,  /* assign explicitly to match BACnetLogRecord */
    LOG_RECORD_BOOL_VALUE = 1,
    LOG_RECORD_REAL_VALUE = 2,
    LOG_RECORD_ENUM_VALUE = 3,
    LOG_RECORD_UNSIGNED_VALUE = 4,
    LOG_RECORD_INTEGER_VALUE = 5,
    LOG_RECORD_BIT_STRING_VALUE = 6,
    LOG_RECORD_NULL_VALUE = 7,
    LOG_RECORD_FAILURE = 8,
    LOG_RECORD_TIME_CHANGE = 9,
    LOG_RECORD_ANY_VALUE = 10
    BACSTAC_OTHER_ENUM(LOG_RECORD_TYPE)

    /* for backward compatibility only */
#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 8))
    #define LOG_RECORD_SIGNED_VALUE _Pragma("GCC warning \"LOG_RECORD_SIGNED_VALUE is deprecated\"") LOG_RECORD_INTEGER_VALUE
#else
    ,
    LOG_RECORD_SIGNED_VALUE = LOG_RECORD_INTEGER_VALUE
#endif
#ifdef _MSC_VER
#   pragma deprecated(LOG_RECORD_SIGNED_VALUE)
#endif /* _MSC_VER */
} BACSTAC_LOG_RECORD_TYPE;

typedef struct tagBACSTAC_LOG_RECORD {
    BACSTAC_DATE_TIME           timeStamp;
    BACSTAC_LOG_RECORD_TYPE     type;
    union {
        BACSTAC_BIT_STRING      logStatus;
        BACSTAC_REAL            timeChange;
        BACSTAC_ERROR_TYPE      error;
        BACSTAC_BOOLEAN         boolValue;
        BACSTAC_REAL            realValue;
        BACSTAC_ENUM            enumValue;
        BACSTAC_UNSIGNED        unsignedValue;
        BACSTAC_INTEGER         integerValue;
        BACSTAC_BIT_STRING      bitStringValue;
        BACSTAC_PROPERTY_CONTENTS       anyValue;

        /* for backward compatibility only */
        BACSTAC_DEPRECATED BACSTAC_INTEGER signedValue;
    } record; /*= tag:type */
    BACSTAC_BOOLEAN         fStatusFlagsPresent;
    BACSTAC_BIT_STRING      statusFlags;
} BACSTAC_LOG_RECORD, *PBACSTAC_LOG_RECORD;


/*
 * EventLog
 */


typedef enum BACSTAC_LOG_DATUM_TYPE
{
    LOG_DATUM_LOG_STATUS = 0,  /* assign explicitly to match BACnetLogRecord */
    LOG_DATUM_NOTIFICATION = 1,
    LOG_DATUM_TIME_CHANGE = 2
    BACSTAC_OTHER_ENUM(LOG_DATUM_TYPE)
} BACSTAC_LOG_DATUM_TYPE;

typedef struct tagBACSTAC_EVENT_LOG_RECORD {
    BACSTAC_DATE_TIME      timeStamp;
    BACSTAC_LOG_DATUM_TYPE type;
    union {
        BACSTAC_BIT_STRING  logStatus;
        BACSTAC_EVENT_NOTIF notification;
        BACSTAC_REAL        timeChange;
    } logDatum; /*= tag:type */
} BACSTAC_EVENT_LOG_RECORD, *PBACSTAC_EVENT_LOG_RECORD;


/*
 * Trend Log Multiple
 */


typedef enum BACSTAC_LOG_DATA_ITEM_TYPE
{
    LOG_DATA_ITEM_BOOL = 0, /* assign explicitly to match BACnetLogData */
    LOG_DATA_ITEM_REAL = 1,
    LOG_DATA_ITEM_ENUM = 2,
    LOG_DATA_ITEM_UNSIGNED = 3,
    LOG_DATA_ITEM_INTEGER = 4,
    LOG_DATA_ITEM_BIT_STRING = 5,
    LOG_DATA_ITEM_NULL = 6,
    LOG_DATA_ITEM_FAILURE = 7,
    LOG_DATA_ITEM_ANY_VALUE = 8
    BACSTAC_OTHER_ENUM(LOG_DATA_ITEM_TYPE)
} BACSTAC_LOG_DATA_ITEM_TYPE;

typedef struct tagBACSTAC_LOG_DATA_ITEM {
    BACSTAC_LOG_DATA_ITEM_TYPE type;
    union {
        BACSTAC_BOOLEAN           boolValue;
        BACSTAC_REAL              realValue;
        BACSTAC_ENUM              enumValue;
        BACSTAC_UNSIGNED          unsignedValue;
        BACSTAC_INTEGER           integerValue;
        BACSTAC_BIT_STRING        bitStringValue;
        BACSTAC_ERROR_TYPE        error;
        BACSTAC_PROPERTY_CONTENTS anyValue;

        /* for backward compatibility only */
        BACSTAC_DEPRECATED BACSTAC_INTEGER signedValue;
    } item; /*= tag:type */
} BACSTAC_LOG_DATA_ITEM, *PBACSTAC_LOG_DATA_ITEM;

typedef struct tagBACSTAC_LOG_DATA_LIST {
    BACSTAC_ELEMENT_COUNT nCount;
    BACSTAC_LOG_DATA_ITEM *pItems;
} BACSTAC_LOG_DATA_LIST, *PBACSTAC_LOG_DATA_LIST;

typedef enum BACSTAC_LOG_DATA_TYPE
{
    LOG_DATA_LOG_STATUS = 0,  /* assign explicitly to match BACnetLogData */
    LOG_DATA_LOG_DATA = 1,
    LOG_DATA_TIME_CHANGE = 2
    BACSTAC_OTHER_ENUM(LOG_DATA_TYPE)
} BACSTAC_LOG_DATA_TYPE;

typedef struct tagBACSTAC_LOG_MULTI_RECORD {
    BACSTAC_DATE_TIME timeStamp;
    BACSTAC_LOG_DATA_TYPE type;
    union {
        BACSTAC_BIT_STRING    logStatus;
        BACSTAC_LOG_DATA_LIST logData;
        BACSTAC_REAL          timeChange;
    } logData; /*= tag:type */
} BACSTAC_LOG_MULTI_RECORD, *PBACSTAC_LOG_MULTI_RECORD;


/*
 * Access Point
 */


typedef struct tagBACSTAC_AUTHN_POLICY_ITEM {
    BACSTAC_DEV_OBJ_REF credDataInput;
    /* Index starts with the value 1 and continue in increasing sequence */
    BACSTAC_UNSIGNED    index;
} BACSTAC_AUTHN_POLICY_ITEM, *PBACSTAC_AUTHN_POLICY_ITEM;

#define BACSTAC_AUTHN_POLICY_ITEM_INIT(devObjRef, index) \
    {devObjRef, (index)}

typedef struct tagBACSTAC_AUTHN_POLICY {
    BACSTAC_AUTHN_POLICY_ITEM *pPolicyItems; /*= size: policyCount */
    BACSTAC_ELEMENT_COUNT                 policyCount;
    BACSTAC_UNSIGNED                      timeout; /* in seconds; 0 means infinity */
    BACSTAC_BOOLEAN                       orderEnforced;
} BACSTAC_AUTHN_POLICY, *PBACSTAC_AUTHN_POLICY;

#define BACSTAC_INFINITE_TIMEOUT ((BACSTAC_UNSIGNED) 0)

#define BACSTAC_AUTHN_POLICY_INIT(pItems, nCount, orderEnforced, timeout) \
    {(pItems), (nCount), (timeout), (orderEnforced)}

#define BACSTAC_AFF_VENDOR_ID       0x01u
#define BACSTAC_AFF_VENDOR_FORMAT   0x02u

typedef struct tagBACSTAC_AUTHN_FACTOR_FORMAT {
    BACSTAC_UINT              mask;
    BACSTAC_AUTHN_FACTOR_TYPE formatType;
    BACSTAC_UI16              vendorID;     /* optional */ /*= mask:BACSTAC_AFF_VENDOR_ID */
    BACSTAC_UI16              vendorFormat; /* optional */ /*= mask:BACSTAC_AFF_VENDOR_FORMAT */
} BACSTAC_AUTHN_FACTOR_FORMAT, *PBACSTAC_AUTHN_FACTOR_FORMAT;

/* Initialize Authentification Factor Format without vendorID and vendorFormat.
 * This is typical initializer for any format type except 'BAFT_CUSTOM'. The
 * custom format type should not be used, because it requires vendorID and
 * vendorFormat to be present. */
#define BACSTAC_AUTHN_FACTOR_FORMAT_INIT(formatType) \
   {0, (formatType), 0, 0}

/* Initialize Authentification Factor Format with optionally zero vendorID
 * and vendorFormat. If vendorIDPresent or vendorFormatPresent is TRUE then
 * the corresponding parameter is set to zero, otherwise it is absent. This
 * initializator should not be used for the custom format type. */
#define BACSTAC_AUTHN_FACTOR_FORMAT_INIT_EX(formatType, \
   vendorIDPresent, vendorFormatPresent) \
   {((vendorIDPresent) ? BACSTAC_AFF_VENDOR_ID : 0) | \
    ((vendorFormatPresent) ? BACSTAC_AFF_VENDOR_FORMAT : 0), \
    formatType, 0, 0}

/* Initialize Authentification Factor Format with the custom format type.
 * vendorID shall contain the BACnet vendor identifier of the vendor which
 * defined the custom format.
 * vendorFormat shall contain a unique identifier that identifies a specific
 * custom authentication factor format. */
#define BACSTAC_AUTHN_FACTOR_FORMAT_INIT_CUSTOM(vendorID, vendorFormat) \
   {BACSTAC_AFF_VENDOR_ID | BACSTAC_AFF_VENDOR_FORMAT, \
    BAFT_CUSTOM, (vendorID), (vendorFormat)}


/*
 * Access Rule
 */

typedef enum BACSTAC_AR_TIME_RANGE_SPECIFIER
{
    BACSTAC_AR_TIME_RANGE_SPECIFIED = 0,
    BACSTAC_AR_TIME_RANGE_ALWAYS    = 1
    BACSTAC_OTHER_ENUM(AR_TIME_RANGE_SPECIFIER)
} BACSTAC_AR_TIME_RANGE_SPECIFIER;

typedef enum BACSTAC_AR_LOCATION_SPECIFIER
{
    BACSTAC_AR_LOCATION_SPECIFIED = 0,
    BACSTAC_AR_LOCATION_ALL       = 1
    BACSTAC_OTHER_ENUM(AR_LOCATION_SPECIFIER)
} BACSTAC_AR_LOCATION_SPECIFIER;

#define BACSTAC_AR_TIME_RANGE    0x01u
#define BACSTAC_AR_LOCATION      0x02u

typedef struct tagBACSTAC_ACCESS_RULE {
    BACSTAC_UINT                    mask;
    /*
     * If 'timeRangeSpecifier' is set to 'SPECIFIED' then timeRange field shall
     * be present. Otherwise, 'timeRangeSpecifier' may be omitted, but if it is
     * present then it shall contain 4194303 (BACSTAC_INST_NUMBER_MAX) in the
     * instance part of the object identifier and in the instance part of the
     * device identifier, if present.
     */
    BACSTAC_AR_TIME_RANGE_SPECIFIER timeRangeSpecifier;
    BACSTAC_DEV_OBJ_PROP_REF        timeRange;  /* optional */ /*= mask:BACSTAC_AR_TIME_RANGE */

    /*
     * If 'locationSpecifier' is 'SPECIFIED' then 'location' shall be present.
     * Otherwise, 'location' may be omitted, but if it is present then it shall
     * contain 4194303 (BACSTAC_INST_NUMBER_MAX) in the instance part of the
     * object identifier and in the instance part of the device identifier,
     * if present.
     */
    BACSTAC_AR_LOCATION_SPECIFIER   locationSpecifier;
    BACSTAC_DEV_OBJ_REF             location;   /* optional */ /*= mask:BACSTAC_AR_LOCATION   */
    BACSTAC_BOOLEAN                 enable;
} BACSTAC_ACCESS_RULE, *PBACSTAC_ACCESS_RULE;

/* Initialize an Access Rule with the specified timeRange and location. */
#define BACSTAC_ACCESS_RULE_INIT(timeRange, location, enable) \
    {BACSTAC_AR_TIME_RANGE | BACSTAC_AR_LOCATION, \
     BACSTAC_AR_TIME_RANGE_SPECIFIED, timeRang, \
     BACSTAC_AR_LOCATION_SPECIFIED, location, (enable)}

/* Initialize an Access Rule with the specified timeRange. */
#define BACSTAC_ACCESS_RULE_INIT_TR(timeRange, enable) \
    {BACSTAC_AR_TIME_RANGE, \
     BACSTAC_AR_TIME_RANGE_SPECIFIED, timeRange, \
     BACSTAC_AR_LOCATION_ALL, \
     BACSTAC_DEV_OBJ_REF_INIT_NODEV(BACSTAC_OBJECT_TYPE_MAX, BACSTAC_INST_NUMBER_MAX), \
     (enable)}

/* Initialize an Access Rule with the specified location. */
#define BACSTAC_ACCESS_RULE_INIT_LOC(location, enable) \
    {BACSTAC_AR_LOCATION, \
     BACSTAC_AR_TIME_RANGE_ALWAYS, BACSTAC_DEV_OBJ_PROP_REF_INIT_NODEV( \
         BACSTAC_OBJECT_TYPE_MAX, BACSTAC_INST_NUMBER_MAX, BACSTAC_PROPERTY_ID_MAX), \
     BACSTAC_AR_LOCATION_SPECIFIED, location, (enable)}

/* Initialize an Access Rule that applies to all (i.e. without any specific
 * timeRange or location). */
#define BACSTAC_ACCESS_RULE_INIT_ALL(enable) \
    {0, \
     BACSTAC_AR_TIME_RANGE_ALWAYS, BACSTAC_DEV_OBJ_PROP_REF_INIT_NODEV( \
         BACSTAC_OBJECT_TYPE_MAX, BACSTAC_INST_NUMBER_MAX, BACSTAC_PROPERTY_ID_MAX), \
     BACSTAC_AR_LOCATION_ALL, BACSTAC_DEV_OBJ_REF_INIT_NODEV( \
         BACSTAC_OBJECT_TYPE_MAX, BACSTAC_INST_NUMBER_MAX), \
     (enable)}


typedef struct tagBACSTAC_ACCESS_RIGHTS {
    BACSTAC_DEV_OBJ_REF rights;
    BACSTAC_BOOLEAN     enable;
} BACSTAC_ACCESS_RIGHTS, *PBACSTAC_ACCESS_RIGHTS;

#define BACSTAC_ACCESS_RIGHTS_INIT(devInstNum, objType, instNum, enable) \
    {BACSTAC_DEV_OBJ_REF_INIT((devInstNum), (objType), (instNum)), (enable)}

#define BACSTAC_ACCESS_RIGHTS_INIT_NODEV(objType, instNum, enable) \
    {BACSTAC_DEV_OBJ_REF_INIT_NODEV((objType), (instNum)), (enable)}


typedef struct tagBACSTAC_CRED_AUTHN_FACTOR {
    BACSTAC_ACCESS_AUTHN_FACTOR_DISABLE disable;
    BACSTAC_AUTHN_FACTOR                authnFactor;
} BACSTAC_CRED_AUTHN_FACTOR, *PBACSTAC_CRED_AUTHN_FACTOR;

#define BACSTAC_CRED_AUTHN_FACTOR_INIT(disable, authnFactor) \
    {(disable), authnFactor}


typedef struct tagBACSTAC_PROP_ACCESS_RESULT {
    BACSTAC_OBJECT_ID   objectID;
    BACSTAC_PROPERTY_ID propID;
    BACSTAC_ARRAY_INDEX index;      /* or VOID */
    BACSTAC_OBJECT_ID   deviceID;   /* ignored if !fDevicePresent */
    BACSTAC_BOOLEAN     fDevicePresent;
    BACSTAC_BOOLEAN     status;
    union {
       BACSTAC_PROPERTY_CONTENTS value;      /* valid if status == BACSTAC_TRUE */
       BACSTAC_ERROR_TYPE        error;      /* valid if status == BACSTAC_FALSE */
    } accessResult;
} BACSTAC_PROP_ACCESS_RESULT, *PBACSTAC_PROP_ACCESS_RESULT;


/*
 * BACSTAC_FAULT_PARAMETER
 */

typedef struct BACSTAC_FAULT_CHARSTRING_PARAM
{
    BACSTAC_CHAR_STRING *faultValues; /*= size:count */
    BACSTAC_ELEMENT_COUNT           count;
} BACSTAC_FAULT_CHARSTRING_PARAM, *PBACSTAC_FAULT_CHARSTRING_PARAM;

typedef struct BACSTAC_FAULT_EXT_PARAMETER
{
   BACSTAC_DATA_TYPE tag;
   union {
       BACSTAC_REAL             real;
       BACSTAC_UNSIGNED         nUnsigned;
       BACSTAC_BOOLEAN          boolean;
       BACSTAC_INTEGER          integer;
       BACSTAC_DOUBLE           fDouble;
       BACSTAC_OCTET_STRING     octetString;
       BACSTAC_CHAR_STRING      charString;
       BACSTAC_BIT_STRING       bitString;
       BACSTAC_ENUM             enumerated;
       BACSTAC_DATE             date;
       BACSTAC_TIME             time;
       BACSTAC_OBJECT_ID        objectID;
       BACSTAC_DEV_OBJ_PROP_REF reference;
     } param;
} BACSTAC_FAULT_EXT_PARAMETER, *PBACSTAC_FAULT_EXT_PARAMETER;

typedef struct BACSTAC_FAULT_EXTENDED_PARAM
{
    BACSTAC_UI16                            vendorID;
    BACSTAC_UNSIGNED                        extendedFaultType;
    BACSTAC_FAULT_EXT_PARAMETER *parameters; /*= size:count */
    BACSTAC_ELEMENT_COUNT                   count;
} BACSTAC_FAULT_EXTENDED_PARAM, *PBACSTAC_FAULT_EXTENDED_PARAM;

typedef struct BACSTAC_FAULT_LIFE_SAFETY_PARAM
{
    BACSTAC_LIFE_SAFETY_STATE *faultValues; /*= size:count */
    BACSTAC_ELEMENT_COUNT                 count;
    BACSTAC_DEV_OBJ_PROP_REF              modePropReference;
} BACSTAC_FAULT_LIFE_SAFETY_PARAM, *PBACSTAC_FAULT_LIFE_SAFETY_PARAM;

typedef struct BACSTAC_FAULT_STATE_PARAM
{
    BACSTAC_PROPERTY_STATES *faultValues; /*= size:count */
    BACSTAC_ELEMENT_COUNT               count;
} BACSTAC_FAULT_STATE_PARAM, *PBACSTAC_FAULT_STATE_PARAM;

typedef struct BACSTAC_FAULT_OUT_OF_RANGE_PARAM
{
   BACSTAC_DATA_TYPE minTag;
   BACSTAC_DATA_TYPE maxTag;
   union BACSTAC_FAULT_OUT_OF_RANGE_U {
            BACSTAC_REAL     real;
            BACSTAC_UNSIGNED nUnsigned;
            BACSTAC_DOUBLE   fDouble;
            BACSTAC_INTEGER  integer;
   }min;
   union BACSTAC_FAULT_OUT_OF_RANGE_U max;
} BACSTAC_FAULT_OUT_OF_RANGE_PARAM, *PBACSTAC_FAULT_OUT_OF_RANGE_PARAM;

typedef BACSTAC_DEV_OBJ_PROP_REF BACSTAC_FAULT_STATUS_FLAGS_PARAM;
typedef BACSTAC_DEV_OBJ_PROP_REF BACSTAC_FAULT_LISTED_PARAM;

typedef struct BACSTAC_FAULT_PARAMETER
{
    BACSTAC_FAULT_TYPE tag;
    union
    {
       BACSTAC_FAULT_CHARSTRING_PARAM   charString; /*= tag:FAULT_CHARACTERSTRING */
       BACSTAC_FAULT_EXTENDED_PARAM     extended; /*= tag:FAULT_EXTENDED */
       BACSTAC_FAULT_LIFE_SAFETY_PARAM  lifeSafety; /*= tag:FAULT_LIFE_SAFETY */
       BACSTAC_FAULT_STATE_PARAM        state; /*= tag:FAULT_STATE */
       BACSTAC_FAULT_STATUS_FLAGS_PARAM statusFlags; /*= tag:FAULT_STATUS_FLAGS */
       BACSTAC_FAULT_OUT_OF_RANGE_PARAM outOfRange; /*= tag:FAULT_OUT_OF_RANGE */
       BACSTAC_FAULT_LISTED_PARAM       listed; /*= tag:FAULT_LISTED */
    } parameter;
} BACSTAC_FAULT_PARAMETER, *PBACSTAC_FAULT_PARAMETER;


/*
 * AtomicReadFile, AtomicWriteFile
 */


typedef enum BACSTAC_FILE_ACCESS_TYPE
{
    FILE_ACCESS_STREAM,
    FILE_ACCESS_RECORD
    BACSTAC_OTHER_ENUM(FILE_ACCESS_TYPE)
} BACSTAC_FILE_ACCESS_TYPE;

/* ReadFile */

typedef union tagBACSTAC_READ_FILE_RANGE {
    struct tagBACSTAC_READ_STREAM_REQUEST {
        BACSTAC_INTEGER         startPosition;  /* zero-based */
        BACSTAC_UNSIGNED        octetCount;
    } streamAccess;
    struct tagBACSTAC_READ_RECORD_REQUEST {
        BACSTAC_INTEGER         startRecord;    /* zero-based */
        BACSTAC_UNSIGNED        recordCount;
    } recordAccess;
} BACSTAC_READ_FILE_RANGE, *PBACSTAC_READ_FILE_RANGE;

typedef struct tagBACSTAC_READ_FILE_INFO {
    BACSTAC_OBJECT_ID           fileID;
    BACSTAC_FILE_ACCESS_TYPE    accessType;
    BACSTAC_READ_FILE_RANGE     request; /*= tag:accessType */
} BACSTAC_READ_FILE_INFO, *PBACSTAC_READ_FILE_INFO;

typedef struct tagBACSTAC_READ_STREAM_RESULT {
    BACSTAC_INTEGER         startPosition;  /* zero-based */
    BACSTAC_VALUE_SIZE      retOctetCount;
    BACSTAC_BUFFER          streamData;
        /* If size of the buffer specified by the 'streamData' field
         * is not sufficient to hold the entire result, ReadFile
         * will return BACSTAC_STATUS_VAL_OUT_OF_SPACE.
         */
} BACSTAC_READ_STREAM_RESULT, *PBACSTAC_READ_STREAM_RESULT;

typedef struct tagBACSTAC_READ_RECORD_RESULT {
    BACSTAC_INTEGER         startRecord;    /* zero-based */
    BACSTAC_UNSIGNED        retRecordCount;
    BACSTAC_BUFFER          recordData;
        /* A plain buffer is passed in. On return, it is formatted
         * as an array of BACSTAC_OCTET_STRING values. Access routines
         * should be used to obtain pointer(s) to those octet strings.
         * Number of octet strings in the array is stored in the
         * retRecordCount field.
         *
         * If size of the buffer specified by the 'recordData' field
         * is not sufficient to hold the entire result, ReadFile
         * will return BACSTAC_STATUS_INVALID_PARAM.
         */
} BACSTAC_READ_RECORD_RESULT, *PBACSTAC_READ_RECORD_RESULT;

typedef struct tagBACSTAC_READ_FILE_RESULT {
    BACSTAC_BOOLEAN         endOfFile;
    BACSTAC_FILE_ACCESS_TYPE accessType;
    union {
        BACSTAC_READ_STREAM_RESULT streamAccess; /*= tag:FILE_ACCESS_STREAM */
        BACSTAC_READ_RECORD_RESULT recordAccess; /*= tag:FILE_ACCESS_RECORD */
    } data;  /*= tag:accessType */
} BACSTAC_READ_FILE_RESULT, *PBACSTAC_READ_FILE_RESULT;

/* WriteFile */

typedef struct tagBACSTAC_WRITE_STREAM_REQUEST {
    BACSTAC_INTEGER         startPosition; /* zero-based */
    BACSTAC_OCTET_STRING    streamData;
} BACSTAC_WRITE_STREAM_REQUEST, *PBACSTAC_WRITE_STREAM_REQUEST;

typedef struct tagBACSTAC_WRITE_RECORD_REQUEST {
    BACSTAC_INTEGER         startRecord;    /* zero-based */
    BACSTAC_UNSIGNED        recordCount;
    BACSTAC_OCTET_STRING    *recordData; /*= size:recordCount */
} BACSTAC_WRITE_RECORD_REQUEST, *PBACSTAC_WRITE_RECORD_REQUEST;

typedef union tagBACSTAC_WRITE_FILE_DATA {
    BACSTAC_WRITE_STREAM_REQUEST streamAccess; /*= tag:FILE_ACCESS_STREAM */
    BACSTAC_WRITE_RECORD_REQUEST recordAccess; /*= tag:FILE_ACCESS_RECORD */
} BACSTAC_WRITE_FILE_DATA, *PBACSTAC_WRITE_FILE_DATA;

typedef struct tagBACSTAC_WRITE_FILE_INFO {
    BACSTAC_OBJECT_ID           fileID;
    BACSTAC_FILE_ACCESS_TYPE    accessType;
    BACSTAC_WRITE_FILE_DATA     request; /*= tag:accessType */
} BACSTAC_WRITE_FILE_INFO, *PBACSTAC_WRITE_FILE_INFO;

typedef struct tagBACSTAC_WRITE_FILE_RESULT {
        BACSTAC_FILE_ACCESS_TYPE    accessType;
        union {
            BACSTAC_INTEGER         startPosition;  /* zero-based */
            BACSTAC_INTEGER         startRecord;    /* zero-based */
        } data; /*= tag:accessType */
} BACSTAC_WRITE_FILE_RESULT, *PBACSTAC_WRITE_FILE_RESULT;


/*
 * BACSTAC_ACTION_COMMAND
 */


#define ACTION_DEVICE_ID_FLAG       0x01u
#define ACTION_POST_DELAY_FLAG      0x02u

typedef struct tagBACSTAC_ACTION_COMMAND {
    BACSTAC_UINT                mask;
    BACSTAC_OBJECT_ID           deviceID;         /* optional */ /*= mask:ACTION_DEVICE_ID_FLAG */
    BACSTAC_OBJECT_ID           objectID;
    BACSTAC_PROPERTY_ID         propertyID;
    BACSTAC_ARRAY_INDEX         index;            /* | VOID */
    BACSTAC_PROPERTY_CONTENTS   value;
    BACSTAC_PRIORITY_LEVEL      priority;         /* | VOID */
    BACSTAC_UNSIGNED            postDelay;        /* optional */ /*= mask:ACTION_POST_DELAY_FLAG */
    BACSTAC_BOOLEAN             quitOnFailure;
    BACSTAC_BOOLEAN             writeSuccessful;
} BACSTAC_ACTION_COMMAND, *PBACSTAC_ACTION_COMMAND;


/*
 * BACSTAC_ACTION_LIST
 */


typedef struct tagBACSTAC_ACTION_LIST {
    BACSTAC_ACTION_COMMAND *pAction; /*= size:actionCount */
    BACSTAC_ELEMENT_COUNT  actionCount;
} BACSTAC_ACTION_LIST, *PBACSTAC_ACTION_LIST;


/*
 * BACSTAC_READ_ACCESS_RESULT
 */


typedef struct tagBACSTAC_READ_ACCESS_RESULT
{
    BACSTAC_OBJECT_ID        objectID;
    /*
     * If listOfResults does not present in ASN.1 encoding then nItemCount == 0
     * and readResults == NULL. If the field presents but it is empty then
     * nItemCount == 0 and readResults != NULL.
     */
    BACSTAC_PROPERTY_COUNT   nItemCount;
    BACSTAC_READ_RESULT_ITEM *readResults; /*= size:nItemCount */
} BACSTAC_READ_ACCESS_RESULT, *PBACSTAC_READ_ACCESS_RESULT;


/* for backward compatibility */
typedef BACSTAC_DEPRECATED BACSTAC_READ_ACCESS_RESULT  BACSTAC_READ_HOOK_RESULT_LIST;
typedef BACSTAC_DEPRECATED PBACSTAC_READ_ACCESS_RESULT PBACSTAC_READ_HOOK_RESULT_LIST;


/*
 * PrivateTransfer
 */


typedef struct tagBACSTAC_PRIVATE_TRANSFER_INFO
{
    BACSTAC_UI16       nVendorID;
    BACSTAC_UINT       nServiceNumber;
    BACSTAC_BOOLEAN    fParametersPresent;
    BACSTAC_BUFFER     parameters;
    BACSTAC_VALUE_SIZE nByteCount;
} BACSTAC_PRIVATE_TRANSFER_INFO, *PBACSTAC_PRIVATE_TRANSFER_INFO;


/*
 * TextMessage
 */


typedef enum BACSTAC_MESSAGE_CLASS_TYPE
{
    MESSAGE_CLASS_NUMERIC,
    MESSAGE_CLASS_CHARACTER
    BACSTAC_OTHER_ENUM(MESSAGE_CLASS_TYPE)
} BACSTAC_MESSAGE_CLASS_TYPE;

typedef struct tagBACSTAC_MESSAGE_CLASS
{
    BACSTAC_MESSAGE_CLASS_TYPE tag;
    union {
        BACSTAC_UNSIGNED    numeric; /*= tag:MESSAGE_CLASS_NUMERIC */
        BACSTAC_CHAR_STRING character; /*= tag:MESSAGE_CLASS_CHARACTER */
    } mClass;
} BACSTAC_MESSAGE_CLASS, *PBACSTAC_MESSAGE_CLASS;

typedef enum BACSTAC_MESSAGE_PRIORITY
{
    MESSAGE_PRIORITY_NORMAL,
    MESSAGE_PRIORITY_URGENT
    BACSTAC_OTHER_ENUM(MESSAGE_PRIORITY)
} BACSTAC_MESSAGE_PRIORITY;

typedef struct tagBACSTAC_TEXT_MESSAGE_INFO
{
    BACSTAC_OBJECT_ID         initDevice;
    BACSTAC_BOOLEAN           fMessageClassPresent;
    BACSTAC_MESSAGE_CLASS     messageClass;
    BACSTAC_MESSAGE_PRIORITY  eMessagePriority;
    BACSTAC_CHAR_STRING       message;
} BACSTAC_TEXT_MESSAGE_INFO, *PBACSTAC_TEXT_MESSAGE_INFO;


/*
 * CreateObject
 */

typedef enum BACSTAC_CREATE_OBJECT_TYPE
{
    CREATE_OBJECT_TYPE,
    CREATE_OBJECT_ID
    BACSTAC_OTHER_ENUM(CREATE_OBJECT_TYPE)
} BACSTAC_CREATE_OBJECT_TYPE;

typedef struct tagBACSTAC_OBJECT_SPECIFIER
{
    BACSTAC_CREATE_OBJECT_TYPE    tag;
    union {
        BACSTAC_OBJECT_TYPE type; /*= tag:CREATE_OBJECT_TYPE */
        BACSTAC_OBJECT_ID   objectID; /*= tag:CREATE_OBJECT_ID */
    } objectSpec;
} BACSTAC_OBJECT_SPECIFIER, *PBACSTAC_OBJECT_SPECIFIER;

typedef struct tagBACSTAC_CREATE_OBJECT_INFO
{
    BACSTAC_OBJECT_SPECIFIER objectSpecifier;
    /* can't just use NULL for the pValues field - need to distinguish */
    /* "empty list" and "no list present" cases */
    BACSTAC_BOOLEAN          fValuesPresent;
    BACSTAC_PROPERTY_VALUE   *pValues; /*= size:nValuesCount */
    BACSTAC_PROPERTY_COUNT   nValuesCount;
} BACSTAC_CREATE_OBJECT_INFO, *PBACSTAC_CREATE_OBJECT_INFO;


/*
 * ReadPropertyConditional
 */


typedef enum BACSTAC_SELECTION_LOGIC
{
    SELECTION_AND = 0,  /* must match values defined in the Standard */
    SELECTION_OR = 1,
    SELECTION_ALL = 2
    BACSTAC_OTHER_ENUM(SELECTION_LOGIC)
} BACSTAC_SELECTION_LOGIC;

typedef enum BACSTAC_RELATION_TYPE
{
    RELATION_EQUAL = 0, /* must match values defined in the Standard */
    RELATION_NOT_EQUAL = 1,
    RELATION_LESS_THAN = 2,
    RELATION_GREATER_THAN = 3,
    RELATION_LESS_THAN_OR_EQUAL = 4,
    RELATION_GREATER_THAN_OR_EQUAL = 5
    BACSTAC_OTHER_ENUM(RELATION_TYPE)
} BACSTAC_RELATION_TYPE;

typedef struct tagBACSTAC_SELECTION_CRITERION {
    BACSTAC_PROPERTY_ID         ePropertyID;
    BACSTAC_ARRAY_INDEX         nIndex;
    BACSTAC_RELATION_TYPE       eRelationSpecifier;
    BACSTAC_PROPERTY_CONTENTS   comparisonValue;
} BACSTAC_SELECTION_CRITERION, *PBACSTAC_SELECTION_CRITERION;

typedef struct tagBACSTAC_SELECTION_CRITERIA {
    BACSTAC_SELECTION_LOGIC                 eSelectionLogic;
    BACSTAC_PROPERTY_COUNT                  nListCount;
    /*
     * optional;
     * If not present, nListCount == 0 and pListOfSelectionCriteria == NULL
     * If present but empty, nListCount == 0 and pListOfSelectionCriteria != NULL
     */
    BACSTAC_SELECTION_CRITERION *pListOfSelectionCriteria; /*= size:nListCount */
} BACSTAC_SELECTION_CRITERIA, *PBACSTAC_SELECTION_CRITERIA;

typedef struct tagBACSTAC_READ_CON_INFO
{
    BACSTAC_SELECTION_CRITERIA objectSelectionCriteria;
    BACSTAC_PROPERTY_COUNT     nItemCount;
    /*
     * Optional; if not present, nItemCount == 0 and readItems == NULL
     * If present but empty then nItemCount == 0 and readItems != 0
     * Pseudo-properties ALL, REQUIRED and OPTIONAL are allowed
     */
    BACSTAC_PROP_REF           *readItems; /*= size:nItemCount */
} BACSTAC_READ_CON_INFO, *PBACSTAC_READ_CON_INFO;

                                                              /*
 * listOfReadAccessResults is passed in as a raw buffer. On the output
 * it is formatted as an array of BACSTAC_READ_ACCESS_RESULT structures.
 * Use access routines to retrieve thsese values.
 *
 * If pointer to the buffer in side the listOfReadAccessResults field is NULL,
 * resultListCount will still contain correct value on return, which allows
 * application to find out how many objects of interest is in the device in
 * query, or calculate required buffer size (for data types having fixed size).
 */
typedef struct tagBACSTAC_RPC_RESULT {
    BACSTAC_BUFFER listOfReadAccessResults;    /* optional */
    BACSTAC_OBJECT_COUNT resultListCount;
} BACSTAC_RPC_RESULT, *PBACSTAC_RPC_RESULT;


/*
 * SubscribeCOVProperty
 */


#define COVP_INFO_CONF_FLAG_PRESENT     0x01u
#define COVP_INFO_LIFETIME_PRESENT      0x02u
#define COVP_INFO_INCREMENT_PRESENT     0x04u

/*
 *  According to Errata-9-7-99:29 service request shall be interpreted
 *  as follows:
 *
 *           field                                is present
 * ----------------------------------------------------------------------------
 * lifetime                          yes        no          no          yes
 * issueConfirmedNotifications       yes        yes         no          no
 * ----------------------------------------------------------------------------
 * request meaning                  subs.   lifetime=0  cancellation   invalid
 */

typedef struct tagBACSTAC_SUBSCRIBE_COVP_INFO
{
    BACSTAC_UINT                    mask;
    BACSTAC_UI32                    processID;
    BACSTAC_OBJECT_ID               monitoredObjectID;
    BACSTAC_BOOLEAN                 issueConfirmedNotifications;
    /* BACSTAC_INFINITE_LIFETIME for indefinite lifetime */
    BACSTAC_UNSIGNED                lifetime;   /* optional */ /*= mask:COVP_INFO_LIFETIME_PRESENT */
    BACSTAC_PROP_REF                monitoredProperty;
    BACSTAC_REAL                    covIncrement;   /* optional */ /*= mask:COVP_INFO_INCREMENT_PRESENT */
} BACSTAC_SUBSCRIBE_COVP_INFO, *PBACSTAC_SUBSCRIBE_COVP_INFO;


#define COV_SUB_INCREMENT_PRESENT       0x1

typedef struct tagBACSTAC_COV_SUBSCRIPTION {
    BACSTAC_UINT                mask;
    BACSTAC_RECIPIENT_PROCESS   recipient;
    BACSTAC_OBJ_PROP_REF        monitoredProperty;
    BACSTAC_BOOLEAN     issueConfirmedNotifications;
    BACSTAC_UNSIGNED    timeRemaining;  /* BACSTAC_INFINITE_LIFETIME if indefinite */
    BACSTAC_REAL        covIncrement;   /* optional */ /*= mask:COV_SUB_INCREMENT_PRESENT */
} BACSTAC_COV_SUBSCRIPTION, *PBACSTAC_COV_SUBSCRIPTION;


/*
 * GetEventInformation
 */


/* Request */

#define COVP_EVENT_INFO_LAST_RCV_PRESENT        0x1

typedef struct tagBACSTAC_EVENT_INFO_INFO
{
    BACSTAC_UINT        mask;
    BACSTAC_OBJECT_ID   lastReceived;
} BACSTAC_EVENT_INFO_INFO, *PBACSTAC_EVENT_INFO_INFO;


/* Response */

typedef struct tagBACSTAC_EVENT_SUMMARY
{
    BACSTAC_OBJECT_ID   objectID;
    BACSTAC_EVENT_STATE eventState;
    BACSTAC_BIT_STRING  ackedTransitions;    /* BACSTAC_EVENT_TRANSITION_BITS */
    BACSTAC_TIME_STAMP  eventStamps[3];
    BACSTAC_NOTIFY_TYPE notifyType;
    BACSTAC_BIT_STRING  eventEnable;        /* BACSTAC_EVENT_TRANSITION_BITS */
    BACSTAC_UNSIGNED    eventPriorities[3];
} BACSTAC_EVENT_SUMMARY, *PBACSTAC_EVENT_SUMMARY;

typedef struct tagBACSTAC_EVENT_INFO
{
    BACSTAC_EVENT_SUMMARY               *pSummaries;    /* optional */
    BACSTAC_ELEMENT_COUNT               bufferCount;
    BACSTAC_ELEMENT_COUNT               summaryCount;
    BACSTAC_BOOLEAN                     moreEvents;
} BACSTAC_EVENT_INFO, *PBACSTAC_EVENT_INFO;


/*
 * LifeSafetyOperation                      / Add-135c-4 /
 */


#define LIFE_SAFETY_OBJECT_ID_PRESENT        0x1

typedef struct tagBACSTAC_LIFE_SAFETY_INFO
{
    BACSTAC_UINT                    mask;
    BACSTAC_UI32                    processID;
    BACSTAC_CHAR_STRING             requestingSource;
    BACSTAC_LIFE_SAFETY_OPERATION   eRequest;
    BACSTAC_OBJECT_ID               objectID;           /* optional */ /*= mask:LIFE_SAFETY_OBJECT_ID_PRESENT */
} BACSTAC_LIFE_SAFETY_INFO, *PBACSTAC_LIFE_SAFETY_INFO;

/* Add-135-2001c-4 Begin */
typedef struct tagBACSTAC_ACCUMULATOR_RECORD
{
    BACSTAC_DATE_TIME           timestamp;
    BACSTAC_UNSIGNED            presentValue;
    BACSTAC_UNSIGNED            accumulatedValue;
    BACSTAC_ACCUMULATOR_STATUS  accumulatorStatus;
} BACSTAC_ACCUMULATOR_RECORD, *PBACSTAC_ACCUMULATOR_RECORD;

#define BACSTAC_ACCUMULATOR_RECORD_INIT(timestamp, presentValue, \
        accumulatedValue, accumulatorStatus) \
    {timestamp, (presentValue), (accumulatedValue), (accumulatorStatus)}

typedef struct tagBACSTAC_PRESCALE
{
    BACSTAC_UNSIGNED            multiplier;
    BACSTAC_UNSIGNED            moduloDivide;
} BACSTAC_PRESCALE, *PBACSTAC_PRESCALE;

#define BACSTAC_PRESCALE_INIT(multiplier, moduloDivide) \
    {(multiplier), (moduloDivide)}

typedef struct tagBACSTAC_SCALE
{
    BACSTAC_DATA_TYPE   tag;
    union {
        BACSTAC_REAL        floatScale; /*= tag:DATA_TYPE_REAL */
        BACSTAC_INTEGER     integerScale; /*= tag:DATA_TYPE_INTEGER */
    } value;
} BACSTAC_SCALE, *PBACSTAC_SCALE;

/* Add-135-2001c-4 End */


/*
 * for CLIENT only
 *
 */

/*
 * for AtomicReadFile
 */

typedef struct tagBACSTAC_READ_FILE_SPEC {
    BACSTAC_FILE_ACCESS_TYPE    accessType;
    BACSTAC_READ_FILE_RANGE     range;
} BACSTAC_READ_FILE_SPEC, *PBACSTAC_READ_FILE_SPEC;

/*
 * for AtomicWriteFile
 */

typedef struct tagBACSTAC_WRITE_FILE_SPEC {
    BACSTAC_FILE_ACCESS_TYPE    accessType;
    BACSTAC_WRITE_FILE_DATA     data;
} BACSTAC_WRITE_FILE_SPEC, *PBACSTAC_WRITE_FILE_SPEC;

/*
 * for ReadPropertyMultiple
 */

/* BACSTAC_READ_RESULT_LIST */

typedef struct tagBACSTAC_READ_RESULT_LIST
{
    BACSTAC_HOBJECT          hObject;    /* or BACSTAC_INVALID_HANDLE */
    BACSTAC_PROPERTY_COUNT   nResultBufferCount;
    BACSTAC_PROPERTY_COUNT   nResultActualCount;
    BACSTAC_READ_RESULT_ITEM *readResults;
} BACSTAC_READ_RESULT_LIST, *PBACSTAC_READ_RESULT_LIST;

/* BACSTAC_READ_LIST */

typedef struct tagBACSTAC_READ_LIST
{
    BACSTAC_HOBJECT         hObject;
    BACSTAC_PROPERTY_COUNT  nItemCount;
    BACSTAC_PROP_REF        *readItems;
} BACSTAC_READ_LIST, *PBACSTAC_READ_LIST;

/*
 * for ReadRange
 */

typedef struct tagBACSTAC_READ_RANGE_32_SPEC {
    BACSTAC_RANGE_TYPE          tag;
    BACSTAC_READ_RANGE_32_RANGE range;
} BACSTAC_READ_RANGE_32_SPEC, *PBACSTAC_READ_RANGE_32_SPEC;

typedef struct tagBACSTAC_READ_RANGE_64_SPEC {
    BACSTAC_RANGE_TYPE          tag;
    BACSTAC_READ_RANGE_64_RANGE range;
} BACSTAC_READ_RANGE_64_SPEC, *PBACSTAC_READ_RANGE_64_SPEC;

#ifdef BACSTAC_USE_64_BIT_READ_RANGE
#define BACSTAC_READ_RANGE_SPEC      BACSTAC_READ_RANGE_64_SPEC
#else
#define BACSTAC_READ_RANGE_SPEC      BACSTAC_READ_RANGE_32_SPEC
#endif

/*
 * for WritePropertyMultiple
 */

typedef struct tagBACSTAC_WRITE_LIST
{
    BACSTAC_HOBJECT        hObject;
    BACSTAC_PROPERTY_VALUE *writeItems;
    BACSTAC_PROPERTY_COUNT nItemCount;
} BACSTAC_WRITE_LIST, *PBACSTAC_WRITE_LIST;


/*
 * BACstac Alloc interface
 */

struct tagBACSTAC_IALLOC;

/* BACstac Alloc interface virtual table */
typedef struct tagBACSTAC_IALLOC_VTBL
{
   void * (BACstac_interface *alloc)(struct tagBACSTAC_IALLOC *, BACSTAC_VALUE_SIZE);
   void (BACstac_interface *free)(struct tagBACSTAC_IALLOC *, void *);
} BACSTAC_IALLOC_VTBL, *PBACSTAC_IALLOC_VTBL;

/* BACstac Alloc interface */
typedef struct tagBACSTAC_IALLOC
{
   const BACSTAC_IALLOC_VTBL *vTable;
} BACSTAC_IALLOC, *PBACSTAC_IALLOC;


/*
 * Data Base
 */


typedef enum BACSTAC_ACCESS
{  /* do not change these values */
    BACSTAC_ACCESS_READ_ONLY = 0,
    BACSTAC_ACCESS_READ_WRITE = 1
    BACSTAC_OTHER_ENUM(ACCESS)
} BACSTAC_ACCESS;

typedef struct tagBACSTAC_PROPERTY_INSTANCE {
    BACSTAC_PROPERTY_ID     ePropertyID;
    BACSTAC_ACCESS          ePropertyAccess;
    BACSTAC_BOOLEAN         bReadCallbackAttached;
    BACSTAC_BOOLEAN         bWriteCallbackAttached;
    BACSTAC_PROPERTY_CONTENTS   propertyContents;
} BACSTAC_DEPRECATED_PROPERTY_INSTANCE;

typedef BACSTAC_DEPRECATED BACSTAC_DEPRECATED_PROPERTY_INSTANCE BACSTAC_PROPERTY_INSTANCE;


/* for backward compatibility */
typedef BACSTAC_DEPRECATED PBACSTAC_PROP_REF PBACSTAC_PROPERTY_REF;
typedef BACSTAC_DEPRECATED BACSTAC_DEV_OBJ_PROP_REF BACSTAC_DEV_OBJ_PROP_REFERENCE;
typedef BACSTAC_DEPRECATED BACSTAC_DEV_OBJ_REF BACSTAC_DEV_OBJ_REFERENCE;
typedef BACSTAC_DEPRECATED BACSTAC_OBJ_PROP_REF BACSTAC_OBJ_PROP_REFERENCE;
typedef BACSTAC_DEPRECATED BACSTAC_PROP_REF BACSTAC_PROPERTY_REFERENCE;
typedef BACSTAC_DEPRECATED BACSTAC_SETPOINT_REF BACSTAC_SETPOINT_REFERENCE;

typedef struct tagBACSTAC_GLOBAL_GROUP_UCOV_NOTIF
{
    BACSTAC_UI32               processID;           /* Subscriber Process Identifier              */
    BACSTAC_OBJECT_ID          initDeviceID;        /* Initiating Device Identifier               */
    BACSTAC_OBJECT_ID          monitoredObjectID;   /* Monitored Object Identifier                */
    BACSTAC_UNSIGNED           timeRemaining;       /* Time Remaining, in seconds                 */
    BACSTAC_BIT_STRING         membStatusFlags;     /* The value of Member_Status_Flags           */
    BACSTAC_ELEMENT_COUNT      valueCount;          /* The number of elements of the values array */
    BACSTAC_PROP_ACCESS_RESULT *values;             /* Elements of Present_Value property         */
} BACSTAC_GLOBAL_GROUP_UCOV_NOTIF, *PBACSTAC_GLOBAL_GROUP_UCOV_NOTIF;

typedef struct tagBACSTAC_GLOBAL_GROUP_UCOV_NOTIF_RESULT
{
    BACSTAC_ELEMENT_COUNT requestCount;
    BACSTAC_ELEMENT_COUNT failureCount;
    BACSTAC_ELEMENT_COUNT valueTooLongCount;
    BACSTAC_ELEMENT_COUNT firstValueTooLong;
} BACSTAC_GLOBAL_GROUP_UCOV_RESULT, *PBACSTAC_GLOBAL_GROUP_UCOV_RESULT;

typedef struct tagBACSTAC_EVENT_NOTIF_SUBS {
   BACSTAC_RECIPIENT recipient;
   BACSTAC_UI32      processID;
   BACSTAC_BOOLEAN   issueConfirmedNotifications;
   BACSTAC_UNSIGNED  timeRemaining;
} BACSTAC_EVENT_NOTIF_SUBS, *PBACSTAC_EVENT_NOTIF_SUBS;

typedef struct tagBACSTAC_PORT_PERMISSION {
   BACSTAC_UI8     portID;
   BACSTAC_BOOLEAN enabled;
} BACSTAC_PORT_PERMISSION, *PBACSTAC_PORT_PERMISSION;

#define BACSTAC_PORT_PERMISSION_INIT(portID, enabled) \
    {(portID), (enabled)}


typedef struct tagBACSTAC_PROC_ID_SELECTION {
    BACSTAC_DATA_TYPE tag;        /* UNSIGNED or NULL */
    BACSTAC_UI32      processID;  /* if selected by tag */
} BACSTAC_PROC_ID_SELECTION, *PBACSTAC_PROC_ID_SELECTION;

#define BACSTAC_NULL_PROC_ID_SELECTION \
    {DATA_TYPE_NULL, 0}

#define BACSTAC_UNSIGNED_PROC_ID_SELECTION(processID) \
    {DATA_TYPE_UNSIGNED, (processID)}

typedef BACSTAC_PRIORITY_LEVEL BACSTAC_OPT_CMD_UNSIGNED;
typedef BACSTAC_OPT_CMD_UNSIGNED *PBACSTAC_OPT_CMD_UNSIGNED;

typedef enum BACSTAC_HOST_ADDR_TYPE
{
    HOST_ADDR_NULL,
    HOST_ADDR_ADDRESS,
    HOST_ADDR_NAME
    BACSTAC_OTHER_ENUM(HOST_ADDR_TYPE)
} BACSTAC_HOST_ADDR_TYPE;

#define BACSTAC_IPV4_ADDRESS_SIZE   4
#define BACSTAC_IPV6_ADDRESS_SIZE   16
#define BACSTAC_MIN_IP_ADDRESS_SIZE 4
#define BACSTAC_MAX_IP_ADDRESS_SIZE 16

typedef struct tagBACSTAC_IP_ADDRESS {
    BACSTAC_BYTE address[BACSTAC_MAX_IP_ADDRESS_SIZE];
    BACSTAC_BYTE addressLength; /* between MIN_IP_ADDRESS_SIZE and MAX_IP_ADDRESS_SIZE */
} BACSTAC_IP_ADDRESS, *PBACSTAC_IP_ADDRESS;

typedef struct tagBACSTAC_HOST_ADDRESS {
    BACSTAC_HOST_ADDR_TYPE tag;
    union {
        BACSTAC_IP_ADDRESS  ipAddress;
        BACSTAC_CHAR_STRING name;
    } address;
} BACSTAC_HOST_ADDRESS, *PBACSTAC_HOST_ADDRESS;

typedef struct tagBACSTAC_HOST_NPORT {
    BACSTAC_HOST_ADDRESS host;
    BACSTAC_UI16         port;
} BACSTAC_HOST_NPORT, *PBACSTAC_HOST_NPORT;

typedef struct tagBACSTAC_BDT_ENTRY {
    BACSTAC_HOST_NPORT bbmdAddress;
    BACSTAC_BOOLEAN    fMaskPresent;
    BACSTAC_BYTE       broadcastMask[BACSTAC_IPV4_ADDRESS_SIZE];
} BACSTAC_BDT_ENTRY, *PBACSTAC_BDT_ENTRY;

#define BACSTAC_MIN_BIP_ADDRESS_SIZE 6
#define BACSTAC_MAX_BIP_ADDRESS_SIZE 18

typedef struct tagBACSTAC_FDT_ENTRY {
    BACSTAC_BYTE bIpAddress[BACSTAC_MAX_BIP_ADDRESS_SIZE];
    BACSTAC_BYTE addressLength; /* between MIN_BIP_ADDRESS_SIZE and MAX_BIP_ADDRESS_SIZE */
    BACSTAC_UI16 timeToLive;
    BACSTAC_UI16 remainingTimeToLive;
} BACSTAC_FDT_ENTRY, *PBACSTAC_FDT_ENTRY;

#define BACSTAC_FDT_ENTRY_INIT(addr0, addr1, addr2, addr3, addr4, addr5, timeToLive, remainingTimeToLive) \
   {{addr0, addr1, addr2, addr3, addr4, addr5, 0,0,0,0,0,0,0,0,0,0,0,0}, 6, timeToLive, remainingTimeToLive};

#define BACSTAC_MAX_VMAC_ADDRESS 6
#define BACSTAC_MAX_NMAC_ADDRESS 18

typedef struct tagBACSTAC_VMAC_ENTRY {
    BACSTAC_BYTE virtualAddress[BACSTAC_MAX_VMAC_ADDRESS];
    BACSTAC_BYTE vaLength;
    BACSTAC_BYTE nativeAddress[BACSTAC_MAX_NMAC_ADDRESS];
    BACSTAC_BYTE naLength;
} BACSTAC_VMAC_ENTRY, *PBACSTAC_VMAC_ENTRY;

typedef struct tagBACSTAC_ROUTER_ENTRY {
    BACSTAC_UI16      netNumber;
    BACSTAC_BYTE      address[BACSTAC_MAX_MAC_ADDRESS];
    BACSTAC_BYTE      addressLength;
    BACSTAC_RE_STATUS status;
    BACSTAC_BOOLEAN   fIndexPresent;
    BACSTAC_UI8       performanceIndex;
} BACSTAC_ROUTER_ENTRY, *PBACSTAC_ROUTER_ENTRY;

#define BACSTAC_ROUTER_ENTRY0_INIT(netNumber, status) \
    {(netNumber), {0, 0, 0, 0, 0, 0, 0}, 0, status, BACSTAC_FALSE, 0}

#define BACSTAC_ROUTER_ENTRY1_INIT(netNumber, mac0, status) \
    {(netNumber), {(mac0), 0, 0, 0, 0, 0, 0}, 1, status, BACSTAC_FALSE, 0}

#define BACSTAC_ROUTER_ENTRY2_INIT(netNumber, mac0, mac1, status) \
    {(netNumber), {(mac0), (mac1), 0, 0, 0, 0, 0}, 2, status, BACSTAC_FALSE, 0}

#define BACSTAC_ROUTER3_ENTRY_INIT(netNumber, mac0, mac1, mac2, status) \
    {(netNumber), {(mac0), (mac1), (mac2), 0, 0, 0, 0}, 3, status, BACSTAC_FALSE, 0}

#define BACSTAC_ROUTER_ENTRY4_INIT(netNumber, mac0, mac1, mac2, mac3, status) \
    {(netNumber), {(mac0), (mac1), (mac2), (mac3), 0, 0, 0}, 4, status, BACSTAC_FALSE, 0}

#define BACSTAC_ROUTER_ENTRY5_INIT(netNumber, mac0, mac1, mac2, mac3, mac4, status) \
    {(netNumber), {(mac0), (mac1), (mac2), (mac3), (mac4), 0, 0}, 5, status, BACSTAC_FALSE, 0}

#define BACSTAC_ROUTER_ENTRY6_INIT(netNumber,mac0, mac1, mac2, mac3, mac4, mac5, status) \
    {(netNumber), {(mac0), (mac1), (mac2), (mac3), (mac4), (mac5), 0}, 6, status, BACSTAC_FALSE, 0}

#define BACSTAC_ROUTER_ENTRY7_INIT(netNumber, mac0, mac1, mac2, mac3, mac4, mac5, mac6, status) \
    {(netNumber), {(mac0), (mac1), (mac2), (mac3), (mac4), (mac5), (mac6)}, 7, status, BACSTAC_FALSE, 0}

#define BACSTAC_ROUTER_ENTRY0_WITH_INDEX_INIT(netNumber, status, index) \
    {(netNumber), {0, 0, 0, 0, 0, 0, 0}, 0, status, BACSTAC_TRUE, (index)}

#define BACSTAC_ROUTER_ENTRY1_WITH_INDEX_INIT(netNumber, mac0, status, index) \
    {(netNumber), {(mac0), 0, 0, 0, 0, 0, 0}, 1, status, BACSTAC_TRUE, (index)}

#define BACSTAC_ROUTER_ENTRY2_WITH_INDEX_INIT(netNumber, mac0, mac1, status, index) \
    {(netNumber), {(mac0), (mac1), 0, 0, 0, 0, 0}, 2, status, BACSTAC_TRUE, (index)}

#define BACSTAC_ROUTER3_ENTRY_WITH_INDEX_INIT(netNumber, mac0, mac1, mac2, status, index) \
    {(netNumber), {(mac0), (mac1), (mac2), 0, 0, 0, 0}, 3, status, BACSTAC_TRUE, (index)}

#define BACSTAC_ROUTER_ENTRY4_WITH_INDEX_INIT(netNumber, mac0, mac1, mac2, mac3, status, index) \
    {(netNumber), {(mac0), (mac1), (mac2), (mac3), 0, 0, 0}, 4, status, BACSTAC_TRUE, (index)}

#define BACSTAC_ROUTER_ENTRY5_WITH_INDEX_INIT(netNumber, mac0, mac1, mac2, mac3, mac4, status, index) \
    {(netNumber), {(mac0), (mac1), (mac2), (mac3), (mac4), 0, 0}, 5, status, BACSTAC_TRUE, (index)}

#define BACSTAC_ROUTER_ENTRY6_WITH_INDEX_INIT(netNumber, mac0, mac1, mac2, mac3, mac4, mac5, status, index) \
    {(netNumber), {(mac0), (mac1), (mac2), (mac3), (mac4), (mac5), 0}, 6, status, BACSTAC_TRUE, (index)}

#define BACSTAC_ROUTER_ENTRY7_WITH_INDEX_INIT(netNumber, mac0, mac1, mac2, mac3, mac4, mac5, mac6, status, index) \
    {(netNumber), {(mac0), (mac1), (mac2), (mac3), (mac4), (mac5), (mac6)}, 7, status, BACSTAC_TRUE, (index)}

typedef enum BACSTAC_VALUE_SOURCE_TYPE
{
    VALUE_SOURCE_NONE,
    VALUE_SOURCE_OBJECT,
    VALUE_SOURCE_ADDRESS
    BACSTAC_OTHER_ENUM(VALUE_SOURCE_TYPE)
} BACSTAC_VALUE_SOURCE_TYPE;

typedef struct tagBACSTAC_VALUE_SOURCE {
    BACSTAC_VALUE_SOURCE_TYPE tag;
    union {
        BACSTAC_DEV_OBJ_REF object;
        BACSTAC_ADDRESS     address;
    } source;
} BACSTAC_VALUE_SOURCE, *PBACSTAC_VALUE_SOURCE;

/* The universal floor number can take values from 1 up to 255. */
typedef BACSTAC_UI8 BACSTAC_FLOOR_NUMBER, *PBACSTAC_FLOOR_NUMBER;

typedef struct tagBACSTAC_LANDING_CALL {
    BACSTAC_FLOOR_NUMBER floorNumber;
    BACSTAC_LIFT_CAR_DIR direction;
} BACSTAC_LANDING_CALL, *PBACSTAC_LANDING_CALL;

#define BACSTAC_LANDING_CALL_INIT(floorNumber, direction) \
    {(floorNumber), (direction)}

typedef struct tagBACSTAC_ASSIGNED_LANDING_CALLS {
    BACSTAC_LANDING_CALL  *landingCalls; /*= size:count */
    BACSTAC_ELEMENT_COUNT count;
} BACSTAC_ASSIGNED_LANDING_CALLS, *PBACSTAC_ASSIGNED_LANDING_CALLS;

#define BACSTAC_ASSIGNED_LANDING_CALLS_INIT(landingCalls, count) \
    {(landingCalls), (count)}

typedef enum BACSTAC_LC_COMMAND_TYPE
{
    LC_COMMAND_DIRECTION   = 1,
    LC_COMMAND_DESTINATION = 2
    BACSTAC_OTHER_ENUM(LC_COMMAND_TYPE)
} BACSTAC_LC_COMMAND_TYPE;

typedef struct tagBACSTAC_LANDING_CALL_STATUS
{
    BACSTAC_FLOOR_NUMBER     floorNumber;
    BACSTAC_LC_COMMAND_TYPE  tag;
    union {
        BACSTAC_LIFT_CAR_DIR direction;
        BACSTAC_FLOOR_NUMBER destination;
    } command;
    BACSTAC_CHAR_STRING      floorText;  /* OPTIONAL */
} BACSTAC_LANDING_CALL_STATUS, *PBACSTAC_LANDING_CALL_STATUS;

typedef struct tagBACSTAC_LANDING_DOOR {
    BACSTAC_FLOOR_NUMBER floorNumber;
    BACSTAC_DOOR_STATUS  doorStatus;
} BACSTAC_LANDING_DOOR, *PBACSTAC_LANDING_DOOR;

#define BACSTAC_LANDING_DOOR_INIT(floorNumber, doorStatus) \
    {(floorNumber), (doorStatus)}

typedef struct tagBACSTAC_LANDING_DOOR_STATUS {
    BACSTAC_LANDING_DOOR  *landingDoors; /*= size:count */
    BACSTAC_ELEMENT_COUNT count;
} BACSTAC_LANDING_DOOR_STATUS, *PBACSTAC_LANDING_DOOR_STATUS;

/* for backward compatibility */
typedef BACSTAC_DEPRECATED PBACSTAC_LANDING_DOOR_STATUS PBACSTAC_LANDING__DOOR_STATUS;

#define BACSTAC_LANDING_DOOR_STATUS_INIT(landingDoors, count) \
    {(landingDoors), (count)}

typedef struct tagBACSTAC_LIFT_CAR_CALL_LIST {
    BACSTAC_FLOOR_NUMBER *floorNumbers; /*= size:count */
    BACSTAC_ELEMENT_COUNT   count;
} BACSTAC_LIFT_CAR_CALL_LIST, *PBACSTAC_LIFT_CAR_CALL_LIST;

#define BACSTAC_LIFT_CAR_CALL_LIST_INIT(floorNumbers, count) \
    {(floorNumbers), (count)}

/*
 * BACnetCOVMultipleSubscription
 */

typedef struct tagBACSTAC_COV_REFERENCE
{
    BACSTAC_PROP_REF monitoredProperty;
    BACSTAC_BOOLEAN  fCovIncrement;
    BACSTAC_REAL     covIncrement;   /* optional */
    BACSTAC_BOOLEAN  timestamped;
} BACSTAC_COV_REFERENCE, *PBACSTAC_COV_REFERENCE;

#define BACSTAC_COV_REFERENCE_INIT(monitoredProperty, timestamped) \
   {monitoredProperty, BACSTAC_FALSE, 0.0, (timestamped)}
#define BACSTAC_COV_REFERENCE_EX_INIT(monitoredProperty, covIncrement, timestamped) \
   {monitoredProperty, BACSTAC_TRUE, (covIncrement), (timestamped)}

typedef struct tagBACSTAC_COV_SUBSCRIPTION_SPEC
{
    BACSTAC_OBJECT_ID     monitoredObjectID;
    BACSTAC_COV_REFERENCE *listOfCOVRefs;
    BACSTAC_ELEMENT_COUNT count;
} BACSTAC_COV_SUBSCRIPTION_SPEC, *PBACSTAC_COV_SUBSCRIPTION_SPEC;

#define BACSTAC_SUBSCRIPTION_SPEC_INIT(monitoredObjectID, listOfCOVRefs, count) \
   {monitoredObjectID, (listOfCOVRefs), (count)}

typedef struct tagBACSTAC_COV_MULTI_SUBSCRIPTION {
    BACSTAC_RECIPIENT_PROCESS     recipient;
    BACSTAC_BOOLEAN               issueConfirmedNotifications;
    BACSTAC_UNSIGNED              timeRemaining;
    BACSTAC_UNSIGNED              maxNotificationDelay;
    BACSTAC_COV_SUBSCRIPTION_SPEC *listOfCOVSubsSpec;
    BACSTAC_ELEMENT_COUNT         count;
} BACSTAC_COV_MULTI_SUBSCRIPTION, *PBACSTAC_COV_MULTI_SUBSCRIPTION;

/*
 *  Concept of Color for BACnet
 */

typedef struct tagBACSTAC_XY_COLOR
{
    BACSTAC_REAL xCoordinate;
    BACSTAC_REAL yCoordinate;
} BACSTAC_XY_COLOR, *PBACSTAC_XY_COLOR;

#define BACSTAC_XY_COLOR_INIT(xCoordinate, yCoordinate) \
    {(xCoordinate), (yCoordinate)}

typedef struct tagBACSTAC_COLOR_CMD
{
    BACSTAC_COLOR_OPERATION  operation;
    BACSTAC_XY_COLOR         targetColor;            /* OPTIONAL */
    BACSTAC_UNSIGNED         targetColorTemp; /* OPTIONAL, 1000 to 30000 */
    BACSTAC_UNSIGNED         fadeTime;               /* OPTIONAL, 100 to 8640000 */
    BACSTAC_UNSIGNED         rampRate;               /* OPTIONAL, 1 to 30000 */
    BACSTAC_UNSIGNED         stepIncrement;          /* OPTIONAL, 1 to 30000 */
} BACSTAC_COLOR_CMD, *PBACSTAC_COLOR_CMD;

#define BACSTAC_COLOR_CMD_INIT(operation) \
    {(operation), {-1, -1}, 0, 0, 0, 0}

/*
 * BACnetLightingCommand
 */

typedef struct tagBACSTAC_LIGHTING_CMD {
    BACSTAC_LIGHTING_OPERATION operation;
    BACSTAC_REAL               targetLevel;     /* OPTIONAL, if omitted < 0 */
    BACSTAC_REAL               rampRate;        /* OPTIONAL, if omitted < 0 */
    BACSTAC_REAL               stepIncrement;   /* OPTIONAL, if omitted < 0 */
    BACSTAC_UNSIGNED           fadeTime;        /* OPTIONAL, if omitted == 0 */
    BACSTAC_PRIORITY_LEVEL     priority;        /* OPTIONAL, if omitted BACSTAC_VOID_PRI */
} BACSTAC_LIGHTING_CMD, *PBACSTAC_LIGHTING_CMD;

#define BACSTAC_LIGHTING_CMD_INIT(operation) \
   {(operation), -1.0, -1.0, -1.0, 0, BACSTAC_VOID_PRI}

#define BACSTAC_LIGHTING_CMD_NONE_INIT \
   {BLO_NONE, -1.0, -1.0, -1.0, 0, BACSTAC_VOID_PRI}

#define BACSTAC_LIGHTING_CMD_FADE_TO_INIT(targetLevel, fadeTime, priority) \
   {BLO_FADE_TO, (targetLevel), -1.0, -1.0, (fadeTime), (priority)}

#define BACSTAC_LIGHTING_CMD_RAMP_TO_INIT(targetLevel, rampRate, priority) \
   {BLO_RAMP_TO, (targetLevel), (rampRate), -1.0, 0, (priority)}

#define BACSTAC_LIGHTING_CMD_STEP_UP_INIT(stepIncrement, priority) \
   {BLO_STEP_UP, -1.0, -1.0, (stepIncrement), 0, (priority)}

#define BACSTAC_LIGHTING_CMD_STEP_DOWN_INIT(stepIncrement, priority) \
   {BLO_STEP_DOWN, -1.0, -1.0, (stepIncrement), 0, (priority)}

#define BACSTAC_LIGHTING_CMD_STEP_ON_INIT(stepIncrement, priority) \
   {BLO_STEP_ON, -1.0, -1.0, (stepIncrement), 0, (priority)}

#define BACSTAC_LIGHTING_CMD_STEP_OFF_INIT(stepIncrement, priority) \
   {BLO_STEP_OFF, -1.0, -1.0, (stepIncrement), 0, (priority)}

#define BACSTAC_LIGHTING_CMD_WARN_INIT(priority) \
   {BLO_WARN, -1.0, -1.0, -1.0, 0, (priority)}

#define BACSTAC_LIGHTING_CMD_WARN_RELINQUISH_INIT(priority) \
   {BLO_WARN_RELINQUISH, -1.0, -1.0, -1.0, 0, (priority)}

#define BACSTAC_LIGHTING_CMD_WARN_OFF_INIT(priority) \
   {BLO_WARN_OFF, -1.0, -1.0, -1.0, 0, (priority)}

#define BACSTAC_LIGHTING_CMD_STOP_INIT(priority) \
   {BLO_STOP, -1.0, -1.0, -1.0, 0, (priority)}

/*
 * BACnetChannelValue
 */

typedef struct tagBACSTAC_CHANNEL_VALUE
{
    BACSTAC_DATA_TYPE   tag;
    union {
        BACSTAC_REAL         r; /*= tag:DATA_TYPE_REAL */
        BACSTAC_ENUM         e; /*= tag:DATA_TYPE_ENUM */
        BACSTAC_UNSIGNED     u; /*= tag:DATA_TYPE_UNSIGNED */
        BACSTAC_BOOLEAN      b; /*= tag:DATA_TYPE_BOOLEAN */
        BACSTAC_INTEGER      i; /*= tag:DATA_TYPE_INTEGER */
        BACSTAC_DOUBLE       d; /*= tag:DATA_TYPE_DOUBLE */
        BACSTAC_TIME         time; /*= tag:DATA_TYPE_TIME */
        BACSTAC_CHAR_STRING  charStr; /*= tag:DATA_TYPE_ANY_CHAR_STRING */
        BACSTAC_OCTET_STRING octetStr; /*= tag:DATA_TYPE_OCTET_STRING */
        BACSTAC_BIT_STRING   bitStr; /*= tag:DATA_TYPE_BIT_STRING */
        BACSTAC_DATE         date; /*= tag:DATA_TYPE_DATE */
        BACSTAC_OBJECT_ID    objectID; /*= tag:DATA_TYPE_OBJECT_ID */
        BACSTAC_LIGHTING_CMD lightingCommand; /*= tag:DATA_TYPE_LIGHTING_CMD */
        BACSTAC_XY_COLOR     xyColor;         /*= tag:DATA_TYPE_XY_COLOR */      /* Add-135-2020ca */
        BACSTAC_COLOR_CMD colorCommand;   /*= tag:DATA_TYPE_COLOR_CMD */ /* Add-135-2020ca */

        /* for backward compatibility only */
        BACSTAC_DEPRECATED BACSTAC_INTEGER s;
    } value;
} BACSTAC_CHANNEL_VALUE, *PBACSTAC_CHANNEL_VALUE;

#define BACSTAC_NULL_CHANNEL_VALUE_INIT {DATA_TYPE_NULL, {0}}

/*
 * BACnetOptionalReal
 */

typedef struct tagBACSTAC_OPT_REAL
{
    BACSTAC_BOOLEAN fPresent;
    BACSTAC_REAL    r;
} BACSTAC_OPT_REAL, *PBACSTAC_OPT_REAL;

#define BACSTAC_NULL_OPT_REAL_INIT  {BACSTAC_FALSE, 0}
#define BACSTAC_OPT_REAL_INIT(real) {BACSTAC_TRUE, real}

/*
 * BACnetOptionalBinaryPV
 */

typedef struct tagBACSTAC_OPT_BINARY_PV
{
    BACSTAC_BOOLEAN   fPresent;
    BACSTAC_BINARY_PV e;
} BACSTAC_OPT_BINARY_PV, *PBACSTAC_OPT_BINARY_PV;

#define BACSTAC_NULL_OPT_BINARY_PV_INIT  {BACSTAC_FALSE, 0}
#define BACSTAC_OPT_BINARY_PV_INIT(binaryPV) {BACSTAC_TRUE, binaryPV}

/*
 * BACnetOptionalUnsigned
 */

typedef struct tagBACSTAC_OPT_UNSIGNED
{
    BACSTAC_BOOLEAN  fPresent;
    BACSTAC_UNSIGNED u;
} BACSTAC_OPT_UNSIGNED, *PBACSTAC_OPT_UNSIGNED;

#define BACSTAC_NULL_OPT_UNSIGNED_INIT  {BACSTAC_FALSE, 0}
#define BACSTAC_OPT_UNSIGNED_INIT(u) {BACSTAC_TRUE, u}

/*
 * NetworkSecurityPolicy
 */

typedef struct tagBACSTAC_NET_SECURITY_POLICY {
    BACSTAC_UI8             portID;
    BACSTAC_SECURITY_POLICY securityLevel;
} BACSTAC_NET_SECURITY_POLICY, *PBACSTAC_NET_SECURITY_POLICY;

#define BACSTAC_NET_SECURITY_POLICY_INIT(portID, securityLevel) \
   {(portID), (securityLevel)}

/*
 * KeyIdentifier
 */

typedef struct tagBACSTAC_KEY_ID {
    BACSTAC_UI8 algorithm;
    BACSTAC_UI8 keyID;
} BACSTAC_KEY_ID, *PBACSTAC_KEY_ID;

#define BACSTAC_KEY_ID_INIT(algorithm, keyID) \
   {(algorithm), (keyID)}

/*
 * SecurityKeySet
 */

typedef struct tagBACSTAC_SECURITY_KEY_SET {
    BACSTAC_UI8                keyRevision;
    BACSTAC_DATE_TIME          activationTime;
    BACSTAC_DATE_TIME          expirationTime;
    BACSTAC_KEY_ID *keyIds;
    BACSTAC_ELEMENT_COUNT      count;
} BACSTAC_SECURITY_KEY_SET, *PBACSTAC_SECURITY_KEY_SET;

#define BACSTAC_SECURITY_KEY_SET_INIT(keyRevision, activationTime, expirationTime, keyIds, count) \
   {(keyRevision), activationTime, expirationTime, (keyIds), (count)}

typedef enum BACSTAC_TSC_VALUE_TYPE
{
    TSC_VALUE_TYPE_NO_VALUE     = -1,
    TSC_VALUE_TYPE_NULL         = DATA_TYPE_NULL,
    TSC_VALUE_TYPE_BOOLEAN      = DATA_TYPE_BOOLEAN,
    TSC_VALUE_TYPE_UNSIGNED     = DATA_TYPE_UNSIGNED,
    TSC_VALUE_TYPE_INTEGER      = DATA_TYPE_INTEGER,
    TSC_VALUE_TYPE_REAL         = DATA_TYPE_REAL,
    TSC_VALUE_TYPE_DOUBLE       = DATA_TYPE_DOUBLE,
    TSC_VALUE_TYPE_OCTET_STRING = DATA_TYPE_OCTET_STRING,
    TSC_VALUE_TYPE_CHAR_STRING  = DATA_TYPE_ANY_CHAR_STRING,
    TSC_VALUE_TYPE_BIT_STRING   = DATA_TYPE_BIT_STRING,
    TSC_VALUE_TYPE_ENUM         = DATA_TYPE_ENUM,
    TSC_VALUE_TYPE_DATE         = DATA_TYPE_DATE,
    TSC_VALUE_TYPE_TIME         = DATA_TYPE_TIME,
    TSC_VALUE_TYPE_OBJECT_ID    = DATA_TYPE_OBJECT_ID,
    TSC_VALUE_TYPE_CONSTRUCTED  = DATA_TYPE_EXTENDED,
    TSC_VALUE_TYPE_DATE_TIME    = DATA_TYPE_DATE_TIME,
    TSC_VALUE_TYPE_LIGHTING_CMD = DATA_TYPE_LIGHTING_CMD
    BACSTAC_OTHER_ENUM(TSC_VALUE_TYPE)

    /* for backward compatibility only */
#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 8))
    #define TSC_VALUE_TYPE_SIGNED _Pragma("GCC warning \"TSC_VALUE_TYPE_SIGNED is deprecated\"") TSC_VALUE_TYPE_INTEGER
#else
    ,
    TSC_VALUE_TYPE_SIGNED       = TSC_VALUE_TYPE_INTEGER
#endif
#ifdef _MSC_VER
#   pragma deprecated(TSC_VALUE_TYPE_SIGNED)
#endif /* _MSC_VER */
} BACSTAC_TSC_VALUE_TYPE;

typedef struct tagBACSTAC_TS_CHANGE_VALUE
{
    BACSTAC_TSC_VALUE_TYPE   tag;
    union {
        BACSTAC_BOOLEAN        b; /*= tag:TSC_VALUE_TYPE_BOOLEAN */
        BACSTAC_UNSIGNED       u; /*= tag:TSC_VALUE_TYPE_UNSIGNED */
        BACSTAC_INTEGER        i; /*= tag:TSC_VALUE_TYPE_INTEGER */
        BACSTAC_REAL           r; /*= tag:TSC_VALUE_TYPE_REAL */
        BACSTAC_DOUBLE         d; /*= tag:TSC_VALUE_TYPE_DOUBLE */
        BACSTAC_ENUM           e; /*= tag:TSC_VALUE_TYPE_ENUM */
        BACSTAC_OCTET_STRING   octetStr; /*= tag:TSC_VALUE_TYPE_OCTET_STRING */
        BACSTAC_CHAR_STRING    charStr; /*= tag:TSC_VALUE_TYPE_ANY_CHAR_STRING */
        BACSTAC_BIT_STRING     bitStr; /*= tag:TSC_VALUE_TYPE_BIT_STRING */
        BACSTAC_DATE           date; /*= tag:TSC_VALUE_TYPE_DATE */
        BACSTAC_TIME           time; /*= tag:TSC_VALUE_TYPE_TIME */
        BACSTAC_OBJECT_ID      objectID; /*= tag:TSC_VALUE_TYPE_OBJECT_ID */
        BACSTAC_RAW_ASN1_VALUE constructed;  /*= tag:TSC_VALUE_TYPE_EXTENDED */
        BACSTAC_DATE_TIME      dateTime; /*= tag:TSC_VALUE_TYPE_DATE_TIME */
        BACSTAC_LIGHTING_CMD   lightingCommand; /*= tag:TSC_VALUE_TYPE_LIGHTING_CMD */

        /* for backward compatibility only */
        BACSTAC_DEPRECATED BACSTAC_INTEGER s;
    } value;
} BACSTAC_TS_CHANGE_VALUE, *PBACSTAC_TS_CHANGE_VALUE;

typedef enum BACSTAC_NAME_VALUE_TYPE
{
    NAME_VALUE_TYPE_NO_VALUE     = -1,
    NAME_VALUE_TYPE_NULL         = DATA_TYPE_NULL,
    NAME_VALUE_TYPE_BOOLEAN      = DATA_TYPE_BOOLEAN,
    NAME_VALUE_TYPE_UNSIGNED     = DATA_TYPE_UNSIGNED,
    NAME_VALUE_TYPE_INTEGER      = DATA_TYPE_INTEGER,
    NAME_VALUE_TYPE_REAL         = DATA_TYPE_REAL,
    NAME_VALUE_TYPE_DOUBLE       = DATA_TYPE_DOUBLE,
    NAME_VALUE_TYPE_OCTET_STRING = DATA_TYPE_OCTET_STRING,
    NAME_VALUE_TYPE_CHAR_STRING  = DATA_TYPE_ANY_CHAR_STRING,
    NAME_VALUE_TYPE_BIT_STRING   = DATA_TYPE_BIT_STRING,
    NAME_VALUE_TYPE_ENUM         = DATA_TYPE_ENUM,
    NAME_VALUE_TYPE_DATE         = DATA_TYPE_DATE,
    NAME_VALUE_TYPE_TIME         = DATA_TYPE_TIME,
    NAME_VALUE_TYPE_OBJECT_ID    = DATA_TYPE_OBJECT_ID,
    NAME_VALUE_TYPE_DATE_TIME    = DATA_TYPE_DATE_TIME
    BACSTAC_OTHER_ENUM(NAME_VALUE_TYPE)

    /* for backward compatibility only */
#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 8))
#define NAME_VALUE_TYPE_SIGNED _Pragma("GCC warning \"NAME_VALUE_TYPE_SIGNED is deprecated\"") NAME_VALUE_TYPE_INTEGER
#else
    ,
    NAME_VALUE_TYPE_SIGNED       = NAME_VALUE_TYPE_INTEGER
#endif
#ifdef _MSC_VER
#pragma deprecated(NAME_VALUE_TYPE_SIGNED)
#endif /* _MSC_VER */
} BACSTAC_NAME_VALUE_TYPE;

typedef struct tagBACSTAC_NAME_VALUE
{
    BACSTAC_NAME_VALUE_TYPE tag;
    BACSTAC_CHAR_STRING     name;
    union {
        BACSTAC_BOOLEAN        b; /*= tag:NAME_VALUE_TYPE_BOOLEAN */
        BACSTAC_UNSIGNED       u; /*= tag:NAME_VALUE_TYPE_UNSIGNED */
        BACSTAC_INTEGER        i; /*= tag:NAME_VALUE_TYPE_INTEGER */
        BACSTAC_REAL           r; /*= tag:NAME_VALUE_TYPE_REAL */
        BACSTAC_DOUBLE         d; /*= tag:NAME_VALUE_TYPE_DOUBLE */
        BACSTAC_ENUM           e; /*= tag:NAME_VALUE_TYPE_ENUM */
        BACSTAC_OCTET_STRING   octetStr; /*= tag:NAME_VALUE_TYPE_OCTET_STRING */
        BACSTAC_CHAR_STRING    charStr; /*= tag:NAME_VALUE_TYPE_ANY_CHAR_STRING */
        BACSTAC_BIT_STRING     bitStr; /*= tag:NAME_VALUE_TYPE_BIT_STRING */
        BACSTAC_DATE           date; /*= tag:NAME_VALUE_TYPE_DATE */
        BACSTAC_TIME           time; /*= tag:NAME_VALUE_TYPE_TIME */
        BACSTAC_OBJECT_ID      objectID; /*= tag:NAME_VALUE_TYPE_OBJECT_ID */
        BACSTAC_DATE_TIME      dateTime; /*= tag:NAME_VALUE_TYPE_DATE_TIME */

        /* for backward compatibility only */
        BACSTAC_DEPRECATED BACSTAC_INTEGER s;
    } value;
} BACSTAC_NAME_VALUE, *PBACSTAC_NAME_VALUE;

typedef struct tagBACSTAC_NAME_VALUE_COLLECTION
{
    BACSTAC_NAME_VALUE *members;
    BACSTAC_ELEMENT_COUNT          count;
} BACSTAC_NAME_VALUE_COLLECTION, *PBACSTAC_NAME_VALUE_COLLECTION;

#define BACSTAC_NAME_VALUE_COLLECTION_INIT(members, count) \
    {(members), (count)}

typedef struct tagBACSTAC_HEALTH
{
    BACSTAC_DATE_TIME   timestamp;
    BACSTAC_ERROR_TYPE  result;
    BACSTAC_BOOLEAN     propPresent;
    BACSTAC_PROPERTY_ID property; /* OPTIONAL */
    BACSTAC_CHAR_STRING details;  /* OPTIONAL */
} BACSTAC_HEALTH, *PBACSTAC_HEALTH;

#define BACSTAC_HEALTH_INIT(timestamp, result) \
    {timestamp, result, BACSTAC_FALSE, (BACSTAC_PROPERTY_ID)(-1), {NULL, BACSTAC_NULL_TERM_STRING, 0, STRING_NONE}}
#define BACSTAC_HEALTH_INIT_ALL(timestamp, result, property, details) \
    {timestamp, result, BACSTAC_TRUE, (property), details}
#define BACSTAC_HEALTH_INIT_DETAILS(timestamp, result, details) \
    {timestamp, result, BACSTAC_FALSE, (BACSTAC_PROPERTY_ID)(-1), details}
#define BACSTAC_HEALTH_INIT_PROP(timestamp, result, property) \
    {timestamp, result, BACSTAC_TRUE, (property), {NULL, BACSTAC_NULL_TERM_STRING, 0, STRING_NONE}}

#define BACSTAC_SCDC_PEER_ADDRESS  0x01
#define BACSTAC_SCDC_PEER_VMAC     0x02
#define BACSTAC_SCDC_PEER_UUID     0x04
#define BACSTAC_SCDC_ERROR         0x08
#define BACSTAC_SCDC_ERROR_DETAILS 0x10

#define BACSTAC_SC_VMAC_SIZE          6
#define BACSTAC_SC_UUID_SIZE         16

typedef struct tagBACSTAC_SC_DIRECT_CONNECTION
{
    BACSTAC_UINT             mask;
    BACSTAC_CHAR_STRING      uri;
    BACSTAC_SC_CONNECT_STATE connectionState;
    BACSTAC_DATE_TIME        connectTimestamp;
    BACSTAC_DATE_TIME        disconnectTimestamp;
    BACSTAC_HOST_NPORT       peerAddress;                      /* optional */ /*= mask:BACSTAC_SCDC_PEER_ADDRESS  */
    BACSTAC_BYTE             peerVmac[BACSTAC_SC_VMAC_SIZE];   /* optional */ /*= mask:BACSTAC_SCDC_PEER_VMAC     */
    BACSTAC_BYTE             peerUuid[BACSTAC_SC_UUID_SIZE];   /* optional */ /*= mask:BACSTAC_SCDC_PEER_UUID     */
    BACSTAC_ERROR_TYPE       error;                            /* optional */ /*= mask:BACSTAC_SCDC_ERROR         */
    BACSTAC_CHAR_STRING      errorDetails;                     /* optional */ /*= mask:BACSTAC_SCDC_ERROR_DETAILS */
} BACSTAC_SC_DIRECT_CONNECTION, *PBACSTAC_SC_DIRECT_CONNECTION;

#define BACSTAC_SCHC_ERROR         0x01
#define BACSTAC_SCHC_ERROR_DETAILS 0x02

typedef struct tagBACSTAC_SC_HUB_CONNECTION
{
    BACSTAC_UINT             mask;
    BACSTAC_SC_CONNECT_STATE connectionState;
    BACSTAC_DATE_TIME        connectTimestamp;
    BACSTAC_DATE_TIME        disconnectTimestamp;
    BACSTAC_ERROR_TYPE       error;                            /* optional */ /*= mask:BACSTAC_SCHC_ERROR         */
    BACSTAC_CHAR_STRING      errorDetails;                     /* optional */ /*= mask:BACSTAC_SCHC_ERROR_DETAILS */
} BACSTAC_SC_HUB_CONNECTION, *PBACSTAC_SC_HUB_CONNECTION;

#define BACSTAC_SC_HC_INIT(conState, conTimestamp, disconTimestamp) \
    {0, (conState), conTimestamp, disconTimestamp, {0, 0}, {NULL, BACSTAC_NULL_TERM_STRING, 0, STRING_NONE}}
#define BACSTAC_SC_HC_INIT_ALL(conState, conTimestamp, disconTimestamp, error, errorDetails) \
    {(BACSTAC_SCHC_ERROR | BACSTAC_SCHC_ERROR_DETAILS), conState, conTimestamp, disconTimestamp, \
     error, errorDetails}
#define BACSTAC_SC_HC_INIT_ERROR(conState, conTimestamp, disconTimestamp, error) \
    {BACSTAC_SCHC_ERROR, conState, conTimestamp, disconTimestamp, \
     error, {NULL, BACSTAC_NULL_TERM_STRING, 0, STRING_NONE}}
#define BACSTAC_SC_HC_INIT_ERR_DETAILS(conState, conTimestamp, disconTimestamp, errorDetails) \
    {BACSTAC_SCHC_ERROR_DETAILS, conState, conTimestamp, disconTimestamp, \
     {0, 0}, errorDetails}

#define BACSTAC_SCFCR_PEER_VMAC     0x01
#define BACSTAC_SCFCR_PEER_UUID     0x02
#define BACSTAC_SCFCR_ERROR_DETAILS 0x04

typedef struct tagBACSTAC_SC_FAILED_CONN_REQ
{
    BACSTAC_UINT        mask;
    BACSTAC_DATE_TIME   timestamp;
    BACSTAC_HOST_NPORT  peerAddress;
    BACSTAC_BYTE        peerVmac[BACSTAC_SC_VMAC_SIZE];   /* optional */ /*= mask:BACSTAC_SCFCR_PEER_VMAC     */
    BACSTAC_BYTE        peerUuid[BACSTAC_SC_UUID_SIZE];   /* optional */ /*= mask:BACSTAC_SCFCR_PEER_UUID     */
    BACSTAC_ERROR_TYPE  error;
    BACSTAC_CHAR_STRING errorDetails;                     /* optional */ /*= mask:BACSTAC_SCFCR_ERROR_DETAILS */
} BACSTAC_SC_FAILED_CONN_REQ, *PBACSTAC_SC_FAILED_CONN_REQ;

#define BACSTAC_SCHFC_ERROR         0x01
#define BACSTAC_SCHFC_ERROR_DETAILS 0x02

typedef struct tagBACSTAC_SC_HUB_FUNC_CONN
{
    BACSTAC_UINT             mask;
    BACSTAC_SC_CONNECT_STATE connectionState;
    BACSTAC_DATE_TIME        connectTimestamp;
    BACSTAC_DATE_TIME        disconnectTimestamp;
    BACSTAC_HOST_NPORT       peerAddress;
    BACSTAC_BYTE             peerVmac[BACSTAC_SC_VMAC_SIZE];
    BACSTAC_BYTE             peerUuid[BACSTAC_SC_UUID_SIZE];
    BACSTAC_ERROR_TYPE       error;                            /* optional */ /*= mask:BACSTAC_SCHFC_ERROR         */
    BACSTAC_CHAR_STRING      errorDetails;                     /* optional */ /*= mask:BACSTAC_SCHFC_ERROR_DETAILS */
} BACSTAC_SC_HUB_FUNC_CONN, *PBACSTAC_SC_HUB_FUNC_CONN;

typedef struct tagBACSTAC_OBJECT_SELECTOR
{
    BACSTAC_DATA_TYPE tag;
    union {
        BACSTAC_OBJECT_ID   objectID;      /*= tag:DATA_TYPE_OBJECT_ID */
        BACSTAC_OBJECT_TYPE objectType;  /*= tag:DATA_TYPE_ENUM */
    } selector;
} BACSTAC_OBJECT_SELECTOR, *PBACSTAC_OBJECT_SELECTOR;

#define BACSTAC_NULL_OBJECT_SELECTOR_INIT {DATA_TYPE_NULL, {0}}

typedef struct tagBACSTAC_OPT_PRI_FILTER
{
    BACSTAC_BOOLEAN    fPresent;
    BACSTAC_BIT_STRING filter;
} BACSTAC_OPT_PRI_FILTER, *PBACSTAC_OPT_PRI_FILTER;

#define BACSTAC_DEF_OPT_PRI_FILTER_INIT {BACSTAC_FALSE, BACSTAC_BIT_STRING0_INIT}
#define BACSTAC_OPT_PRI_FILTER_INIT(b0, b1) {BACSTAC_TRUE, BACSTAC_BIT_STRING2_INIT(16, (b0), (b1))}

/*
 * BACnetAuditNotification
 */

#define BACSTAC_AN_SRC_TIME_STAMP     0x0001
#define BACSTAC_AN_TARGET_TIME_STAMP  0x0002
#define BACSTAC_AN_SRC_OBJECT_ID      0x0004
#define BACSTAC_AN_SRC_COMMENT        0x0008
#define BACSTAC_AN_TARGET_COMMENT     0x0010
#define BACSTAC_AN_INVOKE_ID          0x0020
#define BACSTAC_AN_SRC_USER_ID        0x0040
#define BACSTAC_AN_SRC_USER_ROLE      0x0080
#define BACSTAC_AN_TARGET_OBJ         0x0100
#define BACSTAC_AN_TARGET_PROP        0x0200 /* has the meaning if BACSTAC_AN_TARGET_OBJ */
#define BACSTAC_AN_TARGET_VALUE       0x0400 /* has the meaning if BACSTAC_AN_TARGET_PROP */
#define BACSTAC_AN_CURRENT_VALUE      0x0800 /* has the meaning if BACSTAC_AN_TARGET_VALUE */
#define BACSTAC_AN_RESULT             0x1000

typedef struct tagBACSTAC_AUDIT_NOTIF
{
    BACSTAC_UINT              mask;
    BACSTAC_TIME_STAMP        sourceTimeStamp;  /* optional */ /*= mask:BACSTAC_AN_SRC_TIME_STAMP */
    BACSTAC_TIME_STAMP        targetTimeStamp;  /* optional */ /*= mask:BACSTAC_AN_TARGET_TIME_STAMP */
    BACSTAC_RECIPIENT         sourceDevice;
    BACSTAC_OBJECT_ID         sourceObject;     /* optional */ /*= mask:BACSTAC_AN_SRC_OBJECT_ID */
    BACSTAC_CHAR_STRING       sourceComment;    /* optional */ /*= mask:BACSTAC_AN_SRC_COMMENT */
    BACSTAC_CHAR_STRING       targetComment;    /* optional */ /*= mask:BACSTAC_AN_TARGET_COMMENT */
    BACSTAC_AUDIT_OPERATION   operation;
    BACSTAC_UI16              sourceUserID;     /* optional */ /*= mask:BACSTAC_AN_SRC_USER_ID */
    BACSTAC_UI8               invokeID;         /* optional */ /*= mask:BACSTAC_AN_INVOKE_ID */
    BACSTAC_UI8               sourceUserRole;   /* optional */ /*= mask:BACSTAC_AN_SRC_USER_ROLE */
    BACSTAC_RECIPIENT         targetDevice;
    BACSTAC_OBJECT_ID         targetObject;     /* optional */ /*= mask:BACSTAC_AN_TARGET_OBJ */
    BACSTAC_PRIORITY_LEVEL    targetPriority;   /* | VOID */
    BACSTAC_PROP_REF          targetProperty;   /* optional */ /*= mask:BACSTAC_AN_TARGET_PROP */
    BACSTAC_PROPERTY_CONTENTS targetValue;      /* optional */ /*= mask:BACSTAC_AN_TARGET_VALUE */
    BACSTAC_PROPERTY_CONTENTS currentValue;     /* optional */ /*= mask:BACSTAC_AN_CURRENT_VALUE */
    BACSTAC_ERROR_TYPE        result;           /* optional */ /*= mask:BACSTAC_AN_RESULT */
} BACSTAC_AUDIT_NOTIF, *PBACSTAC_AUDIT_NOTIF;

typedef struct tagBACSTAC_AUDIT_LOG_RECORD
{
    BACSTAC_DATE_TIME      timestamp;
    BACSTAC_LOG_DATUM_TYPE tag;
    union {
        BACSTAC_BIT_STRING  logStatus;
        BACSTAC_AUDIT_NOTIF notification;
        BACSTAC_REAL        timeChange;
    } logDatum;
} BACSTAC_AUDIT_LOG_RECORD, *PBACSTAC_AUDIT_LOG_RECORD;

typedef struct tagBACSTAC_DEV_ADDR_PROXY_TAB_ENTRY
{
    BACSTAC_ADDRESS   address;
    BACSTAC_I_AM_INFO iAm;
    BACSTAC_DATE_TIME lastIAmTime;
} BACSTAC_DEV_ADDR_PROXY_TABLE_ENTRY, *PBACSTAC_DEV_ADDR_PROXY_TAB_ENTRY;

/*
 * BACnetAcknowledgeAlarmInfo
 *
 * BACnetAcknowledgeAlarmInfo is a subset of the AcknowledgeAlarm parameters
 * that is stored in the 'Target Value' field of BACnetAuditNotification.
 */

typedef struct tagBACSTAC_ACK_ALARM_INFO_TV
{
   BACSTAC_EVENT_STATE eventStateAck;
   BACSTAC_TIME_STAMP  timeStamp;
} BACSTAC_ACK_ALARM_INFO_TV, *PBACSTAC_ACK_ALARM_INFO_TV;

/*
 * BACnetLifeSafetyOperationInfo
 *
 * BACnetLifeSafetyOperationInfo is a subset of the LifeSafetyOperation parameters
 * that is stored in the 'Target Value' field of BACnetAuditNotification.
 */

typedef struct tagBACSTAC_LIFE_SAFETY_INFO_TV
{
   BACSTAC_UI32                  processID;
   BACSTAC_LIFE_SAFETY_OPERATION request;
} BACSTAC_LIFE_SAFETY_INFO_TV, *PBACSTAC_LIFE_SAFETY_INFO_TV;

#define BACSTAC_LIFE_SAFETY_INFO_INIT(processID, request) \
    {(processID), (request)}
typedef struct tagBACSTAC_STAGE_LIMIT_VALUE
{
    BACSTAC_REAL       limit;
    BACSTAC_BIT_STRING values;
    BACSTAC_REAL       deadband;
} BACSTAC_STAGE_LIMIT_VALUE, *PBACSTAC_STAGE_LIMIT_VALUE;

/*
 * SubscribeCOVPropertyMultiple-Error
 */

typedef enum BACSTAC_SUBS_COVP_MULTI_ERROR_TYPE
{
    SUBS_COVPM_SIMPLE_ERROR,
    SUBS_COVPM_FIRST_FAILED
    BACSTAC_OTHER_ENUM(SUBS_COVP_MULTI_ERROR_TYPE)
} BACSTAC_SUBS_COVP_MULTI_ERROR_TYPE;

typedef struct tagBACSTAC_SUBS_COVP_MULTI_ERROR {
   BACSTAC_SUBS_COVP_MULTI_ERROR_TYPE tag;
   union {
      BACSTAC_ERROR_TYPE errorType;
      struct tagBACSTAC_FIRST_FAILED_SUBS {
         BACSTAC_OBJECT_ID  monitoredObjectID;
         BACSTAC_PROP_REF   monitoredPropRef;
         BACSTAC_ERROR_TYPE errorType;
      } firstFailedSubs;
   } error;
} BACSTAC_SUBS_COVP_MULTI_ERROR, *PBACSTAC_SUBS_COVP_MULTI_ERROR;



/* SubscribeCOVPropertyMultiple-Request */

typedef struct tagBACSTAC_SUBS_COVP_MULTI_INFO
{
    BACSTAC_BOOLEAN               fSubscribe;
    BACSTAC_UI32                  processID;
    BACSTAC_BOOLEAN               issueConfirmedNotifications;
    BACSTAC_UNSIGNED              lifetime;             /* optional */ /*= fSubscribe:TRUE */
    BACSTAC_UNSIGNED              maxNotificationDelay; /* optional */ /*= fSubscribe:TRUE */
    BACSTAC_COV_SUBSCRIPTION_SPEC *listOfCOVSubsSpec;
    BACSTAC_ELEMENT_COUNT         count;
} BACSTAC_SUBS_COVP_MULTI_INFO, *PBACSTAC_SUBS_COVP_MULTI_INFO;


/* COVNotificationMultiple */

typedef struct tagBACSTAC_COV_NOTIF_PROP_VALUE {
    BACSTAC_PROPERTY_ID       propID;
    BACSTAC_ARRAY_INDEX       index;          /* | VOID */
    BACSTAC_PROPERTY_CONTENTS value;
    BACSTAC_BOOLEAN           fTimePresent;
    BACSTAC_TIME              timeOfChange;   /* optional */
} BACSTAC_COV_NOTIF_PROP_VALUE, *PBACSTAC_COV_NOTIF_PROP_VALUE;

typedef struct tagBACSTAC_COV_NOTIF_ITEM {
    BACSTAC_OBJECT_ID            monitoredObjectID;
    BACSTAC_COV_NOTIF_PROP_VALUE *listOfValues; /*= size:count */
    BACSTAC_ELEMENT_COUNT        count;
} BACSTAC_COV_NOTIF_ITEM, *PBACSTAC_COV_NOTIF_ITEM;

typedef struct tagBACSTAC_COV_NOTIF_MULTI_INFO
{
    BACSTAC_UI32           processID;
    BACSTAC_OBJECT_ID      initDeviceID;
    BACSTAC_UNSIGNED       timeRemaining;
    BACSTAC_BOOLEAN        fTimestampPresent;
    BACSTAC_DATE_TIME      timestamp; /* optional */
    BACSTAC_COV_NOTIF_ITEM *listOfCOVNotifications; /*= size:count */
    BACSTAC_ELEMENT_COUNT  count;
} BACSTAC_COV_NOTIF_MULTI_INFO, *PBACSTAC_COV_NOTIF_MULTI_INFO;


/* WriteGroup */

typedef struct tagBACSTAC_GROUP_CHANNEL_VALUE {
    BACSTAC_UI16           channel;
    BACSTAC_PRIORITY_LEVEL priority;        /* OPTIONAL, if omitted BACSTAC_VOID_PRI */
    BACSTAC_CHANNEL_VALUE  value;
} BACSTAC_GROUP_CHANNEL_VALUE, *PBACSTAC_GROUP_CHANNEL_VALUE;

typedef struct tagBACSTAC_WRITE_GROUP_INFO {
    BACSTAC_UI32                            groupNumber;
    BACSTAC_PRIORITY_LEVEL                  priority;
    BACSTAC_GROUP_CHANNEL_VALUE *changeList;
    BACSTAC_ELEMENT_COUNT                   count;
    BACSTAC_BOOLEAN                         fInhibitDelayPresent;
    BACSTAC_BOOLEAN                         inhibitDelay; /* OPTIONAL */
} BACSTAC_WRITE_GROUP_INFO, *PBACSTAC_WRITE_GROUP_INFO;

/* ConfirmedAuditNotification and UnconfirmedAuditNotification */
typedef struct tagAUDIT_NOTIF_INFO {
    BACSTAC_AUDIT_NOTIF   *notifications;
    BACSTAC_ELEMENT_COUNT count;
} BACSTAC_AUDIT_NOTIF_INFO, *PBACSTAC_AUDIT_NOTIF_INFO;

#define BACSTAC_AUDIT_NOTIF_INFO_INIT(notifications, count) \
    {(notifications), (count)}


/* AuditLogQuery-Request */

typedef enum BACSTAC_QUERY_PARAM_TYPE
{
    BACSTAC_ALQP_BY_TARGET,
    BACSTAC_ALQP_BY_SOURCE
    BACSTAC_OTHER_ENUM(QUERY_PARAM_TYPE)
} BACSTAC_QUERY_PARAM_TYPE;

#define BACSTAC_ALQP_ADDRESS         0x01
#define BACSTAC_ALQP_OBJECT_ID       0x02
#define BACSTAC_ALQP_PROPERTY_ID     0x04
#define BACSTAC_ALQP_OPERATIONS      0x08

typedef struct tagBACSTAC_AUDIT_LOG_QUERY_PARAM
{
   BACSTAC_QUERY_PARAM_TYPE tag;
      union {
         struct tagBACSTAC_AUDIT_LOG_QUERY_BY_TARGET {
            BACSTAC_UINT            mask;
            BACSTAC_OBJECT_ID       deviceID;
            BACSTAC_ADDRESS         address;        /* optional */ /*= mask:BACSTAC_ALQP_ADDRESS     */
            BACSTAC_OBJECT_ID       objectID;       /* optional */ /*= mask:BACSTAC_ALQP_OBJECT_ID   */
            BACSTAC_PROPERTY_ID     propertyID;     /* optional */ /*= mask:BACSTAC_ALQP_PROPERTY_ID */
            BACSTAC_ARRAY_INDEX     index;          /* optional */ /* | BACSTAC_VOID_INDEX           */
            BACSTAC_PRIORITY_LEVEL  priority;       /* optional */ /* | BACSTAC_VOID_PRI             */
            BACSTAC_BIT_STRING      operations;     /* optional */ /*= mask:BACSTAC_ALQP_OPERATIONS  */
            BACSTAC_SUCCESS_FILTER  resultFilter;
         } target;              /*= tag:BACSTAC_ALQP_BY_TARGET */
         struct tagBACSTAC_AUDIT_LOG_QUERY_BY_SOURCE {
            BACSTAC_UINT            mask;
            BACSTAC_OBJECT_ID       deviceID;
            BACSTAC_ADDRESS         address;        /* optional */ /*= mask:BACSTAC_ALQP_ADDRESS     */
            BACSTAC_OBJECT_ID       objectID;       /* optional */ /*= mask:BACSTAC_ALQP_OBJECT_ID   */
            BACSTAC_BIT_STRING      operations;     /* optional */ /*= mask:BACSTAC_ALQP_OPERATIONS  */
            BACSTAC_SUCCESS_FILTER  resultFilter;
         } source;              /*= tag:BACSTAC_ALQP_BY_SOURCE */
      } query;
} BACSTAC_AUDIT_LOG_QUERY_PARAM, *PBACSTAC_AUDIT_LOG_QUERY_PARAM;

typedef struct tagBACSTAC_AUDIT_LOG_QUERY_INFO
{
    BACSTAC_OBJECT_ID               auditLog;
    BACSTAC_AUDIT_LOG_QUERY_PARAM   parameters;
    BACSTAC_UI64                    startAtSeqNumber; /* | BACSTAC_INVALID_SEQ_NUM */
    BACSTAC_UI16                    requestedCount;
} BACSTAC_AUDIT_LOG_QUERY_INFO, *PBACSTAC_AUDIT_LOG_QUERY_INFO;


/* BACnetAuditLogRecordResult */

typedef struct tagAUDIT_LOG_RECORD_RESULT {
    BACSTAC_UI64             sequenceNumber;
    BACSTAC_AUDIT_LOG_RECORD logRecord;
} BACSTAC_AUDIT_LOG_RECORD_RESULT, *PBACSTAC_AUDIT_LOG_RECORD_RESULT;

/*
 * AuditLogQuery-Result
 */
/*
 * The returned records, of type BACSTAC_AUDIT_LOG_RECORD_RESULT, are placed in the buffer.
 * Use access routines to get these records.
 */
typedef struct tagAUDIT_LOG_QUERY_RESULT {
    BACSTAC_OBJECT_ID     auditLog;
    BACSTAC_BUFFER        records;
    BACSTAC_ELEMENT_COUNT count;
    BACSTAC_BOOLEAN       noMoreItems;
} BACSTAC_AUDIT_LOG_QUERY_RESULT, *PBACSTAC_AUDIT_LOG_QUERY_RESULT;

#define BACSTAC_AUDIT_LOG_QUERY_RECORDS_INIT(auditLog, records, count, noMoreItems) \
    {(auditLog), {(records), sizeof((records)[0]) * (count)}, (count), (noMoreItems)}
#define BACSTAC_AUDIT_LOG_QUERY_BUFFER_INIT(recordbuffer, recordCount) \
    {BACSTAC_OBJECT_ID_UNKNOWN, {(recordBuffer), (sizeof((recordBuffer)[0])*(recordCount))}, 0, BACSTAC_FALSE}

/*
 * Who-Am-I service
 */

typedef struct tagBACSTAC_WHO_AM_I_INFO
{
    BACSTAC_UI16         vendorID;
    BACSTAC_CHAR_STRING  modelName;
    BACSTAC_CHAR_STRING  serialNumber;
} BACSTAC_WHO_AM_I_INFO, *PBACSTAC_WHO_AM_I_INFO;

/*
 * You-Are service
 */

typedef struct tagBACSTAC_YOU_ARE_INFO
{
    BACSTAC_UI16         vendorID;
    BACSTAC_CHAR_STRING  modelName;
    BACSTAC_CHAR_STRING  serialNumber;
    BACSTAC_OBJECT_ID    deviceID;
    BACSTAC_BYTE         deviceMACAddress[BACSTAC_MAX_MAC_ADDRESS];
    BACSTAC_BYTE         addressLength;
} BACSTAC_YOU_ARE_INFO, *PBACSTAC_YOU_ARE_INFO;

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* BACTYPES_H */
