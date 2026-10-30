/*
 * BACstac(TM) BNETERR.H
 * Copyright (c) 1996-2025 by Cimetrics Inc.  All rights reserved.
 *
 * Purpose: define BACnet error-indicating enumerations and constants
 *
 */


#ifndef BNETERR_H
#define BNETERR_H

#ifndef BACBASE_H
#include "bacbase.h"
#endif

typedef enum BACSTAC_ERROR_CLASS
{
    ERR_CLASS_DEVICE            =    0,
    ERR_CLASS_OBJECT            =    1,
    ERR_CLASS_PROPERTY          =    2,
    ERR_CLASS_RESOURCES         =    3,
    ERR_CLASS_SECURITY          =    4,
    ERR_CLASS_SERVICES          =    5,
    ERR_CLASS_VT                =    6,
    ERR_CLASS_COMMUNICATION     =    7,
    ERR_CLASS_LAST_KNOWN        =    7,
    ERR_CLASS_ASHRAE_MAX        =    63,
    ERR_CLASS_FIRST_PROPRIETARY =    64,
    ERR_CLASS_MAX               = 65535
    BACSTAC_OTHER_ENUM(ERROR_CLASS)
} BACSTAC_ERROR_CLASS;

typedef enum
{
    ERR_CODE_OTHER = 0,
    ERR_CODE_AUTHENTICATION_FAILED =    1,
    ERR_CODE_CONFIG_IN_PROGRESS =       2,
    ERR_CODE_DEVICE_BUSY =              3,
    ERR_CODE_DYN_CREATION_NOT_SUPP =    4,
    ERR_CODE_FILE_ACCESS_DENIED =       5,
    ERR_CODE_WRONG_SECURITY_LEVELS =    6,
    ERR_CODE_INCONSISTENT_PARAMS =      7,
    ERR_CODE_INVALID_PARAMETERS =       ERR_CODE_INCONSISTENT_PARAMS,
        /* bad name; leave in for compatibility reasons */
    ERR_CODE_INVALID_SELECTION_CRIT =   8,
    ERR_CODE_INVALID_DATA_TYPE =        9,
    ERR_CODE_INVALID_FILE_ACCESS =      10,
    ERR_CODE_INVALID_FILE_START_POS =   11,
    ERR_CODE_INVALID_OPERATOR_NAME =    12,
    ERR_CODE_INVALID_PARAMETER_TYPE =   13,
    ERR_CODE_INVALID_TIME_STAMP =       14,
    ERR_CODE_KEY_GENERATION_ERROR =     15,
    ERR_CODE_MISSING_PARAMETER =        16,
    ERR_CODE_NO_OBJS_OF_SPEC_TYPE =     17,
    ERR_CODE_OBJECT_NOT_FOUND =         17,
        /* obsolete; use ERR_CODE_UNKNOWN_OBJECT (maps to BACnet unknown-object)
         * or ERR_CODE_NO_OBJS_OF_SPEC_TYPE (maps to BACnet no-objects-of-
         * specified-type)
         */
    ERR_CODE_NO_SPACE_FOR_OBJECT =      18,
    ERR_CODE_NO_SPACE_FOR_ELEMENT =     19,
    ERR_CODE_NO_SPACE_TO_WRITE_PROP =   20,
    ERR_CODE_NO_VT_SESSIONS_AVAIL =     21,
    ERR_CODE_PROP_IS_NOT_A_LIST =       22,
    ERR_CODE_OBJ_DELETION_DENIED =      23,
    ERR_CODE_OBJ_ID_ALREADY_EXISTS =    24,
    ERR_CODE_OPERATIONAL_PROBLEM =      25,
    ERR_CODE_PASSWORD_FAILURE =         26,
    ERR_CODE_READ_ACCESS_DENIED =       27,
    ERR_CODE_SECURITY_NOT_SUPPORTED =   28,
    ERR_CODE_SERVICE_REQUEST_DENIED =   29,
    ERR_CODE_TIMEOUT =                  30,
    ERR_CODE_UNKNOWN_OBJECT =           31,
    ERR_CODE_UNKNOWN_PROPERTY =         32,
    ERR_CODE_UNKNOWN_VT_CLASS =         34,
    ERR_CODE_UNKNOWN_VT_SESSION =       35,
    ERR_CODE_UNSUPPORTED_OBJ_TYPE =     36,
    ERR_CODE_VALUE_OUT_OF_RANGE =       37,
    ERR_CODE_VT_SESSION_CLOSED =        38,
    ERR_CODE_VT_SESSION_TERM_FAILED =   39,
    ERR_CODE_WRITE_ACCESS_DENIED =      40,
    ERR_CODE_CHAR_SET_NOT_SUPPORTED =   41,
    ERR_CODE_INVALID_ARRAY_INDEX =      42,
    ERR_CODE_COV_SUBS_FAILED =          43,
    ERR_CODE_NOT_COV_PROPERTY =         44,
    ERR_CODE_OPTIONAL_FUNC_NOT_SUPP =   45,
    ERR_CODE_INVALID_CONFIG_DATA =      46,
    ERR_CODE_DATATYPE_NOT_SUPPORTED =   47,    /* Add-135-2001a */
    ERR_CODE_DUPLICATE_NAME =           48,    /* Add-135-2001c */
    ERR_CODE_DUPLICATE_OBJECT_ID =      49,    /* Add-135-2001c */
    ERR_CODE_PROPERTY_IS_NOT_AN_ARRAY = 50,    /* Add-135-2001c */
    ERR_CODE_ABT_BUFFER_OVERFLOW =      51,    /* Add-135-2004b-11 */
    ERR_CODE_ABT_INVALID_APDU =         52,    /* Add-135-2004b-11 */
    ERR_CODE_ABT_PREEMPTED =            53,    /* Add-135-2004b-11 */
    ERR_CODE_ABT_SEG_NOT_SUPP =         54,    /* Add-135-2004b-11 */
    ERR_CODE_ABT_PROPRIETARY =          55,    /* Add-135-2004b-11 */
    ERR_CODE_ABT_OTHER =                56,    /* Add-135-2004b-11 */
    ERR_CODE_INVALID_TAG =              57,    /* Add-135-2004b-11 */
    ERR_CODE_NETWORK_DOWN =             58,    /* Add-135-2004b-11 */
    ERR_CODE_REJ_BUFFER_OVERFLOW =      59,    /* Add-135-2004b-11 */
    ERR_CODE_REJ_INCONSISTENT_PARAMS =  60,    /* Add-135-2004b-11 */
    ERR_CODE_REJ_INVALID_DATA_TYPE =    61,    /* Add-135-2004b-11 */
    ERR_CODE_REJ_INVALID_TAG =          62,    /* Add-135-2004b-11 */
    ERR_CODE_REJ_MISSING_PARAM =        63,    /* Add-135-2004b-11 */
    ERR_CODE_REJ_PARAM_OUT_OF_RANGE =   64,    /* Add-135-2004b-11 */
    ERR_CODE_REJ_TOO_MANY_ARGUMENTS =   65,    /* Add-135-2004b-11 */
    ERR_CODE_REJ_UNDEFINED_ENUM =       66,    /* Add-135-2004b-11 */
    ERR_CODE_REJ_UNRECOGNIZED_SERVICE = 67,    /* Add-135-2004b-11 */
    ERR_CODE_REJ_PROPRIETARY =          68,    /* Add-135-2004b-11 */
    ERR_CODE_REJ_OTHER =                69,    /* Add-135-2004b-11 */
    ERR_CODE_UNKNOWN_DEVICE =           70,    /* Add-135-2004b-11 */
    ERR_CODE_UNKNOWN_ROUTE =            71,    /* Add-135-2004b-11 */
    ERR_CODE_VALUE_NOT_INITIALIZED =    72,    /* Add-135-2004b-11 */
    ERR_CODE_INVALID_EVENT_STATE =      73,    /* Add-135-2004d-11 */
    ERR_CODE_NO_ALARM_CONFIGURED =      74,    /* Add-135-2004d-11 */
    ERR_CODE_LOG_BUFFER_FULL =          75,    /* Add-135-2004b-4 */
    ERR_CODE_LOGGED_VALUE_PURGED =      76,    /* Add-135-2004b-4 */
    ERR_CODE_NO_PROPERTY_SPECIFIED =    77,    /* Add-135-2004b-4 */
    ERR_CODE_NOT_CFG_TRIGGERED_LOG =    78,    /* Add-135-2004b-4 */
    ERR_CODE_UNKNOWN_SUBS =             79,    /* Add-135-2008h-5 */
    ERR_CODE_PARAM_OUT_OF_RANGE =       80,    /* Add-135-2008r-4 */
    ERR_CODE_LIST_ELEMENT_NOT_FOUND =   81,    /* Add-135-2004b-11 */
    ERR_CODE_BUSY =                     82,    /* Add-135-2004h-1  */
    ERR_CODE_COMMUNICATION_DISABLED =   83,    /* Add-135-2004m-8  */
    ERR_CODE_SUCCESS =                  84,    /* Add-135-2008g */
    ERR_CODE_ACCESS_DENIED =            85,    /* Add-135-2008g */
    ERR_CODE_BAD_DEST_ADDRESS =         86,    /* Add-135-2008g */
    ERR_CODE_BAD_DEST_DEVICE_ID =       87,    /* Add-135-2008g */
    ERR_CODE_BAD_SIGNATURE =            88,    /* Add-135-2008g */
    ERR_CODE_BAD_SRC_ADDRESS =          89,    /* Add-135-2008g */
    ERR_CODE_DUPLICATE_MESSAGE =        95,    /* Add-135-2008g */
    ERR_CODE_ENCRYPTION_NOT_CFG =       96,    /* Add-135-2008g */
    ERR_CODE_ENCRYPTION_REQUIRED =      97,    /* Add-135-2008g */
    ERR_CODE_MALFORMED_MSG =           101,    /* Add-135-2008g */
    ERR_CODE_SECURITY_NOT_CONFIGURED = 103,    /* Add-135-2008g */
    ERR_CODE_SRC_SECURITY_REQUIRED =   104,    /* Add-135-2008g */
    ERR_CODE_UNKNOWN_AUTHN_TYPE =      106,    /* Add-135-2008g */
    ERR_CODE_NOT_ROUTER_TO_DNET =      110,    /* Add-135-2008g */
    ERR_CODE_ROUTER_BUSY =             111,    /* Add-135-2008g */
    ERR_CODE_UNKNOWN_NETWORK_MSG =     112,    /* Add-135-2008g */
    ERR_CODE_MSG_TOO_LONG =            113,    /* Add-135-2008g */
    ERR_CODE_SECURITY_ERROR =          114,    /* Add-135-2008g */
    ERR_CODE_ADDRESSING_ERROR =        115,    /* Add-135-2008g */
    ERR_CODE_WRITE_BDT_FAILED =        116,    /* Add-135-2008g */
    ERR_CODE_READ_BDT_FAILED =         117,    /* Add-135-2008g */
    ERR_CODE_REGISTER_FD_FAILED =      118,    /* Add-135-2008g */
    ERR_CODE_READ_FDT_FAILED =         119,    /* Add-135-2008g */
    ERR_CODE_DELETE_FDT_ENTRY_FAILED = 120,    /* Add-135-2008g */
    ERR_CODE_DISTR_BROADCAST_FAILED =  121,    /* Add-135-2008g */
    ERR_CODE_UNKNOWN_FILE_SIZE =       122,    /* Add-135-2008n-1 */
    ERR_CODE_ABT_APDU_TOO_LONG =       123,    /* Add-135-2008u-3 */
    ERR_CODE_ABT_APP_EXC_REPLY_TIME =  124,    /* Add-135-2008u-3 */
    ERR_CODE_ABT_OUT_OF_RESOURCES =    125,    /* Add-135-2008u-3 */
    ERR_CODE_ABT_TSM_TIMEOUT =         126,    /* Add-135-2008u-3 */
    ERR_CODE_ABT_WIN_SIZE_OUT_OF_RANGE = 127,  /* Add-135-2008u-3 */
    ERR_CODE_FILE_FULL =               128,    /* Add-135-2008u-3 */
    ERR_CODE_INCONSISTENT_CFG =        129,    /* Add-135-2008u-3 */
    ERR_CODE_INCONSISTENT_OBJ_TYPE =   130,    /* Add-135-2008u-3 */
    ERR_CODE_INTERNAL_ERROR =          131,    /* Add-135-2008u-3 */
    ERR_CODE_NOT_CONFIGURED =          132,    /* Add-135-2008u-3 */
    ERR_CODE_OUT_OF_MEMORY =           133,    /* Add-135-2008u-3 */
    ERR_CODE_VALUE_TOO_LONG =          134,    /* Add-135-2008p */
    ERR_CODE_ABT_INSUFFICIENT_SECURITY = 135,  /* Add-135-2008g */
    ERR_CODE_ABT_SECURITY_ERROR =      136,    /* Add-135-2008g */
    ERR_CODE_DUPLICATE_ENTRY =         137,    /* Add-135-2012ax-4 */
    ERR_CODE_INVALID_VALUE_IN_THIS_STATE = 138, /* Add-135-2012ai-1 */
    ERR_CODE_INVALID_OPERATION_IN_THIS_STATE = 139, /* Add-135-2016bt */
    ERR_CODE_LIST_ITEM_NOT_NUMBERED =  140,    /* Add-135-2016bu */
    ERR_CODE_LIST_ITEM_NOT_TIMESTAMPED = 141,  /* Add-135-2016bu */
    ERR_CODE_INVALID_DATA_ENCODING =   142,    /* Add-135-2016bu */
    ERR_CODE_BVLC_FUNC_UNKNOWN =       143,    /* Add-135-2016bj-6 */
    ERR_CODE_BVLC_PTY_FUNC_UNKNOWN =   144,    /* Add-135-2016bj-6 */
    ERR_CODE_HEADER_ENCODING_ERROR =   145,    /* Add-135-2016bj-6 */
    ERR_CODE_HEADER_NOT_UNDERSTOOD =   146,    /* Add-135-2016bj-6 */
    ERR_CODE_MSG_INCOMPLETE =          147,    /* Add-135-2016bj-6 */
    ERR_CODE_NOT_A_BACNET_SC_HUB =     148,    /* Add-135-2016bj-6 */
    ERR_CODE_PAYLOAD_EXPECTED =        149,    /* Add-135-2016bj-6 */
    ERR_CODE_UNEXPECTED_DATA =         150,    /* Add-135-2016bj-6 */
    ERR_CODE_NODE_DUPLICATE_VMAC =     151,    /* Add-135-2016bj-6 */
    ERR_CODE_HTTP_UNEXPECTED_RESP_CODE = 152,  /* Add-135-2016bj-6 */
    ERR_CODE_HTTP_NO_UPGRADE =         153,    /* Add-135-2016bj-6 */
    ERR_CODE_HTTP_RESOURCE_NOT_LOCAL = 154,    /* Add-135-2016bj-6 */
    ERR_CODE_HTTP_PROXY_AUTHN_FAILED = 155,    /* Add-135-2016bj-6 */
    ERR_CODE_HTTP_RESP_TIMEOUT =       156,    /* Add-135-2016bj-6 */
    ERR_CODE_HTTP_RESP_SYNTAX_ERROR =  157,    /* Add-135-2016bj-6 */
    ERR_CODE_HTTP_RESP_VALUE_ERROR =   158,    /* Add-135-2016bj-6 */
    ERR_CODE_HTTP_RESP_MISSING_HEADER  = 159,  /* Add-135-2016bj-6 */
    ERR_CODE_HTTP_WS_HEADER_ERROR =    160,    /* Add-135-2016bj-6 */
    ERR_CODE_HTTP_UPGRADE_REQUIRED =   161,    /* Add-135-2016bj-6 */
    ERR_CODE_HTTP_UPGRADE_ERROR =      162,    /* Add-135-2016bj-6 */
    ERR_CODE_HTTP_TEMP_UNAVAILABLE =   163,    /* Add-135-2016bj-6 */
    ERR_CODE_HTTP_NOT_A_SERVER =       164,    /* Add-135-2016bj-6 */
    ERR_CODE_HTTP_ERROR =              165,    /* Add-135-2016bj-6 */
    ERR_CODE_WS_SCHEME_NOT_SUPPORTED = 166,    /* Add-135-2016bj-6 */
    ERR_CODE_WS_UNKNOWN_CONTROL_MSG =  167,    /* Add-135-2016bj-6 */
    ERR_CODE_WS_CLOSE_ERROR =          168,    /* Add-135-2016bj-6 */
    ERR_CODE_WS_CLOSED_BY_PEER =       169,    /* Add-135-2016bj-6 */
    ERR_CODE_WS_ENDPOINT_LEAVES =      170,    /* Add-135-2016bj-6 */
    ERR_CODE_WS_PROTOCOL_ERROR =       171,    /* Add-135-2016bj-6 */
    ERR_CODE_WS_DATA_NOT_ACCEPTED =    172,    /* Add-135-2016bj-6 */
    ERR_CODE_WS_CLOSED_ABNORMALLY =    173,    /* Add-135-2016bj-6 */
    ERR_CODE_WS_DATA_INCONSISTENT =    174,    /* Add-135-2016bj-6 */
    ERR_CODE_WS_DATA_AGAINST_POLICY =  175,    /* Add-135-2016bj-6 */
    ERR_CODE_WS_FRAME_TOO_LONG =       176,    /* Add-135-2016bj-6 */
    ERR_CODE_WS_EXTENSION_MISSING =    177,    /* Add-135-2016bj-6 */
    ERR_CODE_WS_REQUEST_UNAVAILABLE =  178,    /* Add-135-2016bj-6 */
    ERR_CODE_WS_ERROR =                179,    /* Add-135-2016bj-6 */
    ERR_CODE_TLS_CLIENT_CERT_ERROR =   180,    /* Add-135-2016bj-6 */
    ERR_CODE_TLS_SERVER_CERT_ERROR =   181,    /* Add-135-2016bj-6 */
    ERR_CODE_TLS_CLIENT_AUTHN_FAILED = 182,    /* Add-135-2016bj-6 */
    ERR_CODE_TLS_SERVER_AUTHN_FAILED = 183,    /* Add-135-2016bj-6 */
    ERR_CODE_TLS_CLIENT_CERT_EXPIRED = 184,    /* Add-135-2016bj-6 */
    ERR_CODE_TLS_SERVER_CERT_EXPIRED = 185,    /* Add-135-2016bj-6 */
    ERR_CODE_TLS_CLIENT_CERT_REVOKED = 186,    /* Add-135-2016bj-6 */
    ERR_CODE_TLS_SERVER_CERT_REVOKED = 187,    /* Add-135-2016bj-6 */
    ERR_CODE_TLS_ERROR =               188,    /* Add-135-2016bj-6 */
    ERR_CODE_DNS_UNAVAILABLE =         189,    /* Add-135-2016bj-6 */
    ERR_CODE_DNS_NAME_RES_FAILED =     190,    /* Add-135-2016bj-6 */
    ERR_CODE_DNS_RESOLVER_FAILURE =    191,    /* Add-135-2016bj-6 */
    ERR_CODE_DNS_ERROR =               192,    /* Add-135-2016bj-6 */
    ERR_CODE_TCP_CONNECT_TIMEOUT =     193,    /* Add-135-2016bj-6 */
    ERR_CODE_TCP_CONNECTION_REFUSED =  194,    /* Add-135-2016bj-6 */
    ERR_CODE_TCP_CLOSED_BY_LOCAL =     195,    /* Add-135-2016bj-6 */
    ERR_CODE_TCP_CLOSED_OTHER =        196,    /* Add-135-2016bj-6 */
    ERR_CODE_TCP_ERROR =               197,    /* Add-135-2016bj-6 */
    ERR_CODE_IP_ADDRESS_NOT_REACHABLE = 198,   /* Add-135-2016bj-6 */
    ERR_CODE_IP_ERROR =                199,    /* Add-135-2016bj-6 */
    ERR_CODE_CERTIFICATE_EXPIRED =     200,    /* Add-135-2020cc */
    ERR_CODE_CERTIFICATE_INVALID =     201,    /* Add-135-2020cc */
    ERR_CODE_CERTIFICATE_MALFORMED =   202,    /* Add-135-2020cc */
    ERR_CODE_CERTIFICATE_REVOKED =     203,    /* Add-135-2020cc */
    ERR_CODE_UNKNOWN_CERTIFICATE_KEY = 204,    /* Add-135-2020cc */
    ERR_CODE_REFERENCED_PORT_IN_ERROR = 205,   /* Add-135-2020cc */
    ERR_CODE_NOT_ENABLED =             206,    /* Add-135-2020ci-3 */
    ERR_CODE_INVALID_ARRAY_SIZE =      227,    /* Add-135-2020ci-6 */
    ERR_CODE_LAST_KNOWN =              ERR_CODE_INVALID_ARRAY_SIZE,
    ERR_CODE_ASHRAE_MAX =              256,
    ERR_CODE_FIRST_PROPRIETARY =       257,
    ERR_CODE_MAX =                   65535
    BACSTAC_OTHER_ENUM(ERROR_CODE)

    /* for backward compatibility only */
#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 8))
    #define ERR_CODE_INVALID_VALUE _Pragma("GCC warning \"ERR_CODE_INVALID_VALUE is deprecated\"") ERR_CODE_INVALID_VALUE_IN_THIS_STATE
    #define ERR_CODE_UNKNOWN_KEY _Pragma("GCC warning \"ERR_CODE_UNKNOWN_KEY is deprecated\"") ERR_CODE_UNKNOWN_CERTIFICATE_KEY
#else
    ,
    ERR_CODE_INVALID_VALUE = ERR_CODE_INVALID_VALUE_IN_THIS_STATE,
    ERR_CODE_UNKNOWN_KEY   = ERR_CODE_UNKNOWN_CERTIFICATE_KEY
#endif
#ifdef _MSC_VER
#pragma deprecated(ERR_CODE_INVALID_VALUE)
#endif /* _MSC_VER */
} BACSTAC_ERROR_CODE;

typedef enum BACSTAC_ABORT_REASON
{
    ABORT_OTHER =                       0,
    ABORT_BUFFER_OVERFLOW =             1,
    ABORT_INVALID_APDU =                2,
    ABORT_PREEMPTED =                   3,
    ABORT_SEGMENTATION_NOT_SUPP =       4,
    ABORT_SECURITY_ERROR =              5,
    ABORT_INSUFFICIENT_SECURITY =       6,
    ABORT_WIN_SIZE_OUT_OF_RANGE =       7,
    ABORT_APP_EXC_REPLY_TIME =          8,
    ABORT_OUT_OF_RESOURCES =            9,
    ABORT_TSM_TIMEOUT =                10,
    ABORT_APDU_TOO_LONG =              11,
    ABORT_INCONSISTENT_ATTRIBUTES =    12, /* Add-135-2020cf */
    ABORT_LAST_KNOWN =                 12,
    ABORT_ASHRAE_MAX =                 63,
    ABORT_FIRST_PROPRIETARY =          64,
    ABORT_MAX =                       255
    BACSTAC_OTHER_ENUM(ABORT_REASON)
} BACSTAC_ABORT_REASON;

typedef enum BACSTAC_REJECT_REASON
{
    REJECT_OTHER =                      0,
    REJECT_BUFFER_OVERFLOW =            1,
    REJECT_INCONSISTENT_PARAMS =        2,
    REJECT_INVALID_DATA_TYPE =          3,
    REJECT_INVALID_TAG =                4,
    REJECT_MISSING_REQUIRED_PARAM =     5,
    REJECT_PARAMETER_OUT_OF_RANGE =     6,
    REJECT_TOO_MANY_ARGUMENTS =         7,
    REJECT_UNDEFINED_ENUMERATION =      8,
    REJECT_UNRECOGNIZED_SERVICE =       9,
    REJECT_INVALID_DATA_ENCODING =     10,
    REJECT_LAST_KNOWN =                10,
    REJECT_ASHRAE_MAX =                63,
    REJECT_FIRST_PROPRIETARY =         64,
    REJECT_MAX =                      255
    BACSTAC_OTHER_ENUM(REJECT_REASON)
} BACSTAC_REJECT_REASON;


#endif /* BNETERR_H */

