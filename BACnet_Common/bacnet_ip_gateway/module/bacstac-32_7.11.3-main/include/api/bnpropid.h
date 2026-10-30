/*
 * BACstac(TM) BNPROPID.H
 *
 * Copyright (c) 2016-2024 by Cimetrics Inc. All rights reserved.
 *
 * Purpose: define BACnet Property Identifiers
 */

#ifndef BNPROPID_H
#define BNPROPID_H

#ifndef BACBASE_H
#include "bacbase.h"
#endif

#if UINT_MAX >= BACSTAC_UI32_MAX
typedef unsigned BACSTAC_PROPERTY_ID;
#else
typedef BACSTAC_UI32 BACSTAC_PROPERTY_ID;
#endif

#define PROP_ABSENTEE_LIMIT                     244 /* Add-135-2008j-1 */
#define PROP_ACCEPTED_MODES                     175 /* Add-135-2001c */
#define PROP_ACCESS_ALARM_EVENTS                245 /* Add-135-2008j-1 */
#define PROP_ACCESS_DOORS                       246 /* Add-135-2008j-1 */
#define PROP_ACCESS_EVENT                       247 /* Add-135-2008j-1 */
#define PROP_ACCESS_EVENT_AUTHN_FACTOR          248 /* Add-135-2008j-1 */
#define PROP_ACCESS_EVENT_CRED                  249 /* Add-135-2008j-1 */
#define PROP_ACCESS_EVENT_TAG                   322 /* Add-135-2008j-1 */
#define PROP_ACCESS_EVENT_TIME                  250 /* Add-135-2008j-1 */
#define PROP_ACCESS_TRANSACTION_EVENTS          251 /* Add-135-2008j-1 */
#define PROP_ACCOMPANIMENT                      252 /* Add-135-2008j-1 */
#define PROP_ACCOMPANIMENT_TIME                 253 /* Add-135-2008j-1 */
#define PROP_ACK_REQUIRED                         1
#define PROP_ACKED_TRANSITIONS                    0
#define PROP_ACTION                               2
#define PROP_ACTION_TEXT                          3
#define PROP_ACTIVATION_TIME                    254 /* Add-135-2008j-1 */
#define PROP_ACTIVE_AUTHN_POLICY                255 /* Add-135-2008j-1 */
#define PROP_ACTIVE_COV_MULTI_SUBS              481 /* Add-135-2012aq */
#define PROP_ACTIVE_COV_SUBSCRIPTIONS           152
#define PROP_ACTIVE_TEXT                          4
#define PROP_ACTIVE_VT_SESSIONS                   5
#define PROP_ACTUAL_SHED_LEVEL                  212 /* Add-135-2004e */
#define PROP_ADDITIONAL_REF_PORTS               508 /* Add-135-2020cc */
#define PROP_ADJUST_VALUE                       176 /* Add-135-2001c-4,5 */
#define PROP_ALARM_VALUE                          6
#define PROP_ALARM_VALUES                         7
#define PROP_ALIGN_INTERVALS                    193 /* Add-135-2004b-3 */
#define PROP_ALL                                  8
#define PROP_ALL_WRITES_SUCCESSFUL                9
#define PROP_ALLOW_GROUP_DELAY_INHIBIT          365 /* Add-135-2010aa-1 */
#define PROP_APDU_LENGTH                        399 /* Add-135-2012ai-1 */
#define PROP_APDU_SEGMENT_TIMEOUT                10
#define PROP_APDU_TIMEOUT                        11
#define PROP_APP_SOFTWARE_VERSION                12
#define PROP_ARCHIVE                             13
#define PROP_ASSIGNED_ACCESS_RIGHTS             256 /* Add-135-2008j-1 */
#define PROP_ASSIGNED_LANDING_CALLS             447 /* Add-135-2012aq */
#define PROP_ATTEMPTED_SAMPLES                  124
#define PROP_AUDITABLE_OPERATIONS               501 /* Add-135-2016bi */
#define PROP_AUDIT_LEVEL                        498 /* Add-135-2016bi */
#define PROP_AUDIT_NOTIF_RECIPIENT              499 /* Add-135-2016bi */
#define PROP_AUDIT_PRI_FILTER                   500 /* Add-135-2016bi */
#define PROP_AUDIT_SRC_REPORTER                 497 /* Add-135-2016bi */
#define PROP_AUTHN_FACTORS                      257 /* Add-135-2008j-1 */
#define PROP_AUTHN_POLICY_LIST                  258 /* Add-135-2008j-1 */
#define PROP_AUTHN_POLICY_NAMES                 259 /* Add-135-2008j-1 */
#define PROP_AUTHN_STATUS                       260 /* Add-135-2008j-1 */
#define PROP_AUTHZ_EXEMPTIONS                   364 /* Add-135-2010ae-3 */
#define PROP_AUTHZ_MODE                         261 /* Add-135-2008j-1 */
#define PROP_AUTO_SUBORDINATE_DISCOVERY         169 /* Add-135-2020ce */
#define PROP_AUTO_SLAVE_DISCOVERY               PROP_AUTO_SUBORDINATE_DISCOVERY /* Add-135-2001a */ /* was renamed in Add 2020ce */
#define PROP_AVERAGE_VALUE                      125
#define PROP_BACKUP_AND_RESTORE_STATE           338 /* Add-135-2008n-1 */
#define PROP_BACKUP_FAILURE_TIMEOUT             153
#define PROP_BACKUP_PREPARATION_TIME            339 /* Add-135-2008n-1 */
#define PROP_BASE_DEVICE_SECURITY_POLICY        327 /* This property was deleted in BACnet Protocol Revision 22 */
#define PROP_BIP_GLOBAL_ADDRESS                 407 /* Add-135-2012ai-1 */
#define PROP_BIP_MODE                           408 /* Add-135-2012ai-1 */
#define PROP_BIP_MULTICAST_ADDRESS              409 /* Add-135-2012ai-1 */
#define PROP_BIP_NAT_TRAVERSAL                  410 /* Add-135-2012ai-1 */
#define PROP_BIP_UDP_PORT                       412 /* Add-135-2012ai-1 */
#define PROP_BIPV6_MODE                         435 /* Add-135-2012aj-1 */
#define PROP_BIPV6_UDP_PORT                     438 /* Add-135-2012aj-1 */
#define PROP_BIPV6_MULTICAST_ADDRESS            440 /* Add-135-2012aj-1 */
#define PROP_BBMD_ACCEPT_FD_REG                 413 /* Add-135-2012ai-1 */
#define PROP_BBMD_BD_TABLE                      414 /* Add-135-2012ai-1 */
#define PROP_BBMD_FD_TABLE                      415 /* Add-135-2012ai-1 */
#define PROP_BELONGS_TO                         262
#define PROP_BIAS                                14
#define PROP_BIT_MASK                           342 /* Add-135-2008w */
#define PROP_BIT_TEXT                           343 /* Add-135-2008w */
#define PROP_BLINK_WARN_ENABLE                  373 /* Add-135-2010i-1 */
#define PROP_BUFFER_SIZE                        126
#define PROP_CAR_ASSIGNED_DIR                   448 /* Add-135-2012aq */
#define PROP_CAR_DOOR_COMMAND                   449 /* Add-135-2012aq */
#define PROP_CAR_DOOR_STATUS                    450 /* Add-135-2012aq */
#define PROP_CAR_DOOR_TEXT                      451 /* Add-135-2012aq */
#define PROP_CAR_DOOR_ZONE                      452 /* Add-135-2012aq */
#define PROP_CAR_DRIVE_STATUS                   453 /* Add-135-2012aq */
#define PROP_CAR_LOAD                           454 /* Add-135-2012aq */
#define PROP_CAR_LOAD_UNITS                     455 /* Add-135-2012aq */
#define PROP_CAR_MODE                           456 /* Add-135-2012aq */
#define PROP_CAR_MOVING_DIR                     457 /* Add-135-2012aq */
#define PROP_CAR_POSITION                       458 /* Add-135-2012aq */
#define PROP_CERT_SIGNING_REQ_FILE              509 /* Add-135-2020cc */
#define PROP_CHANGE_OF_STATE_COUNT               15
#define PROP_CHANGE_OF_STATE_TIME                16
#define PROP_CHANGES_PENDING                    416 /* Add-135-2012ai-1 */
#define PROP_CHANNEL_NUMBER                     366 /* Add-135-2010aa-1 */
#define PROP_CLIENT_COV_INCREMENT               127
#define PROP_COLOR_CMD                      4194334 /* Add-135-2020ca Errata */
#define PROP_COLOR_OVERRIDE                 4194328 /* Add-135-2020ca */
#define PROP_COLOR_REFERENCE                4194329 /* Add-135-2020ca */
#define PROP_DEFAULT_COLOR                  4194330 /* Add-135-2020ca */
#define PROP_DEFAULT_COLOR_TEMPERATURE      4194331 /* Add-135-2020ca */
#define PROP_COMMAND_VALIDATION_RESULT          510 /* Add-135-2020cc */
#define PROP_COMMAND                            417 /* Add-135-2012ai-1 */
#define PROP_COMMAND_TIME_ARRAY                 430 /* Add-135-2012as */
#define PROP_CONFIGURATION_FILES                154
#define PROP_CONTROL_GROUPS                     367 /* Add-135-2010aa-1 */
#define PROP_CONTROLLED_VAR_REFERENCE            19
#define PROP_CONTROLLED_VAR_UNITS                20
#define PROP_CONTROLLED_VAR_VALUE                21
#define PROP_COUNT                              177 /* Add-135-2001c-4,5 */
#define PROP_COUNT_BEFORE_CHANGE                178 /* Add-135-2001c-4,5 */
#define PROP_COUNT_CHANGE_TIME                  179 /* Add-135-2001c-4,5 */
#define PROP_COV_INCREMENT                       22
#define PROP_COV_PERIOD                         180 /* Add-135-2001c-4,5 */
#define PROP_COV_RESUBSCRIBE_INTERVAL           128
#define PROP_COVU_PERIOD                        349 /* Add-135-2008p */
#define PROP_COVU_RECIPIENTS                    350 /* Add-135-2008p */
#define PROP_CRED_DISABLE                       263 /* Add-135-2008j-1 */
#define PROP_CRED_STATUS                        264 /* Add-135-2008j-1 */
#define PROP_CREDENTIALS                        265 /* Add-135-2008j-1 */
#define PROP_CREDS_IN_ZONE                      266 /* Add-135-2008j-1 */
#define PROP_CURRENT_COMMAND_PRIORITY           431 /* Add-135-2012as */
#define PROP_CURRENT_HEALTH                 4194307 /* Add-135-2020cc */
#define PROP_CURRENT_NOTIFY_TIME                129 /* This property was deleted in BACnet 1.3 */
#define PROP_DATABASE_REVISION                  155
#define PROP_DATELIST                            23
#define PROP_DAYLIGHT_SAVINGS_STATUS             24
#define PROP_DAYS_REMAINING                     267 /* Add-135-2008j-1 */
#define PROP_DEADBAND                            25
#define PROP_DEFAULT_FADE_TIME                  374 /* Add-135-2010i-1 */
#define PROP_DEFAULT_ON_VALUE               4194341 /* Add-135-2020cj */
#define PROP_DEFAULT_PRESENT_VALUE              492 /* Add-135-2016bd */
#define PROP_DEFAULT_RAMP_RATE                  375 /* Add-135-2010i-1 */
#define PROP_DEFAULT_STEP_INCREMENT             376 /* Add-135-2010i-1 */
#define PROP_DEF_SUBORDINATE_RELATION           490 /* Add-135-2012ba */
#define PROP_DEFAULT_TIMEOUT                    393 /* Add-135-2012ay-1 */
#define PROP_DELETE_ON_FORWARD                  502 /* Add-135-2016bi */
#define PROP_DEPLOYED_PROFILE_LOCATION          484 /* Add-135-2012ba */
#define PROP_DERIVATIVE_CONSTANT                 26
#define PROP_DERIVATIVE_CONSTANT_UNITS           27
#define PROP_DESCRIPTION                         28
#define PROP_DESCRIPTION_OF_HALT                 29
#define PROP_DEVICE_ADDRESS_BINDING              30
#define PROP_DEVICE_ADDRESS_PROXY_ENABLE    4194338 /* Add-135-2020bx */
#define PROP_DEVICE_ADDRESS_PROXY_TABLE     4194339 /* Add-135-2020bx */
#define PROP_DEVICE_ADDRESS_PROXY_TIMEOUT   4194340 /* Add-135-2020bx */
#define PROP_DEVICE_TYPE                         31
#define PROP_DEVICE_UUID                        507 /* Add-135-2016bj-4 */
#define PROP_DIRECT_READING                     156 /* Add-135c-1,2 */
#define PROP_DISTRIBUTION_KEY_REVISION          328 /* This property was deleted in BACnet Protocol Revision 22 */
#define PROP_DO_NOT_HIDE                        329 /* This property was deleted in BACnet Protocol Revision 22 */
#define PROP_DOOR_ALARM_STATE                   226 /* Add-135-2004f */
#define PROP_DOOR_EXTENDED_PULSE_TIME           227 /* Add-135-2004f */
#define PROP_DOOR_MEMBERS                       228 /* Add-135-2004f */
#define PROP_DOOR_OPEN_TOO_LONG_TIME            229 /* Add-135-2004f */
#define PROP_DOOR_PULSE_TIME                    230 /* Add-135-2004f */
#define PROP_DOOR_STATUS                        231 /* Add-135-2004f */
#define PROP_DOOR_UNLOCK_DELAY_TIME             232 /* Add-135-2004f */
#define PROP_DUTY_WINDOW                        213 /* Add-135-2004e */
#define PROP_EFFECTIVE_PERIOD                    32
#define PROP_EGRESS_ACTIVE                      386 /* Add-135-2010i-1 */
#define PROP_EGRESS_TIME                        377 /* Add-135-2010i-1 */
#define PROP_ELAPSED_ACTIVE_TIME                 33
#define PROP_ELEVATOR_GROUP                     459 /* Add-135-2012aq */
#define PROP_ENABLE                             133 /* Add-135-2004b-1 */
#define PROP_ENERGY_METER                       460 /* Add-135-2012aq */
#define PROP_ENERGY_METER_REF                   461 /* Add-135-2012aq */
#define PROP_ENTRY_POINTS                       268 /* Add-135-2008j-1 */
#define PROP_ERROR_LIMIT                         34
#define PROP_ESCALATOR_MODE                     462 /* Add-135-2012aq */
#define PROP_EVENT_ALG_INHIBIT                  354 /* Add-135-2010af-18 */
#define PROP_EVENT_ALG_INHIBIT_REF              355 /* Add-135-2010af-18 */
#define PROP_EVENT_DETECTION_ENABLE             353 /* Add-135-2010af-17 */
#define PROP_EVENT_ENABLE                        35
#define PROP_EVENT_MESSAGE_TEXTS                351 /* Add-135-2008z-1 */
#define PROP_EVENT_MESSAGE_TEXTS_CFG            352 /* Add-135-2010af-16 */
#define PROP_EVENT_PARAMETERS                    83
#define PROP_EVENT_STATE                         36
#define PROP_EVENT_TIME_STAMPS                  130
#define PROP_EVENT_TYPE                          37
#define PROP_EXCEPTION_SCHEDULE                  38
#define PROP_EXECUTION_DELAY                    368 /* Add-135-2010aa-1 */
#define PROP_EXIT_POINTS                        269 /* Add-135-2008j-1 */
#define PROP_EXPECTED_SHED_LEVEL                214 /* Add-135-2004e */
#define PROP_EXPIRATION_TIME                    270 /* Add-135-2012ay-2 */
#define PROP_EXTENDED_TIME_ENABLE               271 /* Add-135-2008j-1 */
#define PROP_FAILED_ATTEMPT_EVENTS              272 /* Add-135-2008j-1 */
#define PROP_FAILED_ATTEMPTS                    273 /* Add-135-2008j-1 */
#define PROP_FAILED_ATTEMPTS_TIME               274 /* Add-135-2008j-1 */
#define PROP_FAULT_HIGH_LIMIT                   388 /* Add-135-2012aw-3 */
#define PROP_FAULT_LOW_LIMIT                    389 /* Add-135-2012aw-3 */
#define PROP_FAULT_PARAMETERS                   358 /* add-135-2010af-27 */
#define PROP_FAULT_SIGNALS                      463 /* Add-135-2012aq */
#define PROP_FAULT_TYPE                         359 /* add-135-2010af-27 */
#define PROP_FAULT_VALUES                        39
#define PROP_FD_BBMD_ADDRESS                    418 /* Add-135-2012ai-1 */
#define PROP_FD_SUBSCRIPTION_LIFETIME           419 /* Add-135-2012ai-1 */
#define PROP_FEEDBACK_VALUE                      40
#define PROP_FILE_ACCESS_METHOD                  41
#define PROP_FILE_SIZE                           42
#define PROP_FILE_TYPE                           43
#define PROP_FIRMWARE_REVISION                   44
#define PROP_FLOOR_NUMBER                       506 /* Add-135-2016bt */
#define PROP_FLOOR_TEXT                         464 /* Add-135-2012aq */
#define PROP_FULL_DUTY_BASELINE                 215 /* Add-135-2004e */
#define PROP_GLOBAL_ID                          323 /* Add-135-2008j-1 */
#define PROP_GROUP_ID                           465 /* Add-135-2012aq */
#define PROP_GROUP_MEMBER_NAMES                 346 /* Add-135-2008p */
#define PROP_GROUP_MEMBERS                      345 /* Add-135-2008p */
#define PROP_GROUP_MODE                         467 /* Add-135-2012aq */
#define PROP_HIGH_END_TRIM                  4194335 /* Add-135-2020ca Errata */
#define PROP_HIGH_LIMIT                          45
#define PROP_HIGHER_DECK                        468 /* Add-135-2012aq */
#define PROP_IN_PROCESS                          47
#define PROP_IN_PROGRESS                        378 /* Add-135-2010i-1 */
#define PROP_INACTIVE_TEXT                       46
#define PROP_INITIAL_TIMEOUT                    394 /* Add-135-2012ay-1 */
#define PROP_INPUT_REFERENCE                    181 /* Add-135-2001c-4,5 */
#define PROP_INSTALLATION_ID                    469 /* Add-135-2012aq */
#define PROP_INSTANCE_OF                         48
#define PROP_INSTANTANEOUS_POWER                379 /* Add-135-2010i-1 */
#define PROP_INTEGRAL_CONSTANT                   49
#define PROP_INTEGRAL_CONSTANT_UNITS             50
#define PROP_INTERFACE_VALUE                    387 /* Add-135-2012at-1 */
#define PROP_INTERVAL_OFFSET                    195 /* Add-135-2004b-3 */
#define PROP_IP_ADDRESS                         400 /* Add-135-2012ai-1 */
#define PROP_IP_DEFAULT_GATEWAY                 401 /* Add-135-2012ai-1 */
#define PROP_IP_DHCP_ENABLE                     402 /* Add-135-2012ai-1 */
#define PROP_IP_DHCP_LTIME                      403 /* Add-135-2012ai-1 */
#define PROP_IP_DHCP_LTIME_REMAINING            404 /* Add-135-2012ai-1 */
#define PROP_IP_DHCP_SERVER                     405 /* Add-135-2012ai-1 */
#define PROP_IP_DNS_SERVER                      406 /* Add-135-2012ai-1 */
#define PROP_IP_SUBNET_MASK                     411 /* Add-135-2012ai-1 */
#define PROP_IPV6_ADDRESS                       436 /* Add-135-2012aj-1 */
#define PROP_IPV6_AUTO_ADDRESS_ENABLE           442 /* Add-135-2012aj-1 */
#define PROP_IPV6_DEFAULT_GATEWAY               439 /* Add-135-2012aj-1 */
#define PROP_IPV6_DHCP_LTIME                    443 /* Add-135-2012aj-1 */
#define PROP_IPV6_DHCP_LTIME_REMAINING          444 /* Add-135-2012aj-1 */
#define PROP_IPV6_DHCP_SERVER                   445 /* Add-135-2012aj-1 */
#define PROP_IPV6_DNS_SERVER                    441 /* Add-135-2012aj-1 */
#define PROP_IPV6_PREFIX_LENGTH                 437 /* Add-135-2012aj-1 */
#define PROP_IPV6_ZONE_INDEX                    446 /* Add-135-2012aj-1 */
#define PROP_IS_UTC                             344 /* Add-135-2008w */
#define PROP_ISSUE_CONF_NOTIFICATIONS            51 /* This property was deleted in BACnet 1.4 */
#define PROP_ISSUER_CERT_FILES                  511 /* Add-135-2020cc */
#define PROP_KEY_SETS                           330 /* This property was deleted in BACnet Protocol Revision 22 */
#define PROP_LANDING_CALL_CONTROL               471 /* Add-135-2012aq */
#define PROP_LANDING_CALLS                      470 /* Add-135-2012aq */
#define PROP_LANDING_DOOR_STATUS                472 /* Add-135-2012aq */
#define PROP_LAST_ACCESS_EVENT                  275 /* Add-135-2008j-1 */
#define PROP_LAST_ACCESS_POINT                  276 /* Add-135-2008j-1 */
#define PROP_LAST_COMMAND_TIME                  432 /* Add-135-2012as */
#define PROP_LAST_CRED_ADDED                    277 /* Add-135-2008j-1 */
#define PROP_LAST_CRED_ADDED_TIME               278 /* Add-135-2008j-1 */
#define PROP_LAST_CRED_REMOVED                  279 /* Add-135-2008j-1 */
#define PROP_LAST_CRED_REMOVED_TIME             280 /* Add-135-2008j-1 */
#define PROP_LAST_KEY_SERVER                    331 /* This property was deleted in BACnet Protocol Revision 22 */
#define PROP_LAST_NOTIFY_RECORD                 173 /* Add-135-2001b */
#define PROP_LAST_ON_VALUE                  4194342 /* Add-135-2020cj */
#define PROP_LAST_PRIORITY                      369 /* Add-135-2010aa-1 */
#define PROP_LAST_RESTART_REASON                196 /* Add-135-2004b-5 */
#define PROP_LAST_RESTORE_TIME                  157
#define PROP_LAST_STATE_CHANGE                  395 /* Add-135-2012ay-1 */
#define PROP_LAST_USE_TIME                      281 /* Add-135-2008j-1 */
#define PROP_LIFE_SAFETY_ALARM_VALUES           166 /* Add-135c-1,2 */
#define PROP_LIGHTING_CMD                       380 /* Add-135-2010i-1 */
#define PROP_LIGHTING_CMD_DEF_PRIORITY          381 /* Add-135-2010i-1 */
#define PROP_LIMIT_ENABLE                       52
#define PROP_LIMIT_MONITORING_INTERVAL          182 /* Add-135-2001c-4,5 */
#define PROP_LINK_SPEED                         420 /* Add-135-2012ai-1 */
#define PROP_LINK_SPEED_AUTONEGOTIATE           422 /* Add-135-2012ai-1 */
#define PROP_LINK_SPEEDS                        421 /* Add-135-2012ai-1 */
#define PROP_LIST_OF_GROUP_MEMBERS               53
#define PROP_LIST_OF_OBJ_PROP_REF                54
#define PROP_LIST_OF_SESSION_KEYS                55
#define PROP_LOCAL_DATE                          56
#define PROP_LOCAL_FORWARDING_ONLY              360 /* Add-135-2010af-29 */
#define PROP_LOCAL_TIME                          57
#define PROP_LOCATION                            58
#define PROP_LOCK_STATUS                        233 /* Add-135-2004f */
#define PROP_LOCKOUT                            282 /* Add-135-2008j-1 */
#define PROP_LOCKOUT_RELINQUISH_TIME            283 /* Add-135-2008j-1 */
#define PROP_LOG_BUFFER                         131
#define PROP_LOG_DEVICE_OBJ_PROP                132
#define PROP_LOG_INTERVAL                       134
#define PROP_LOGGING_OBJECT                     183 /* Add-135-2001c-4,5 */
#define PROP_LOGGING_RECORD                     184 /* Add-135-2001c-4,5 */
#define PROP_LOGGING_TYPE                       197 /* Add-135-2004b-3 */
#define PROP_LOW_DIFF_LIMIT                     390 /* Add-135-2012aw-4 */
#define PROP_LOW_END_TRIM                   4194336 /* Add-135-2020ca Errata */
#define PROP_LOW_LIMIT                           59
#define PROP_LOWER_DECK                         473 /* Add-135-2012aq */
#define PROP_MAC_ADDRESS                        423 /* Add-135-2012ai-1 */
#define PROP_MACHINE_ROOM_ID                    474 /* Add-135-2012aq */
#define PROP_MAINTENANCE_REQUIRED               158 /* Add-135c-1,2 */
#define PROP_MAKING_CAR_CALL                    475 /* Add-135-2012aq */
#define PROP_MANIPULATED_VAR_REFERENCE           60
#define PROP_MANUAL_SUBORDINATE_ADDRESS_BINDING 170 /* Add-135-2020ce */
#define PROP_MANUAL_SLAVE_ADDRESS_BINDING       PROP_MANUAL_SUBORDINATE_ADDRESS_BINDING /* Add-135-2001a */ /* renamed in 2020ce */
#define PROP_MASKED_ALARM_VALUES                234 /* Add-135-2004f */
#define PROP_MASTER_EXEMPTION                   284 /* This property was deleted in BACnet 1.13 */
#define PROP_MAX_ACTUAL_VALUE                   382 /* Add-135-2010i-1 */
#define PROP_MAX_BVLC_LENGTH_ACCEPTED       4194304 /* Add-135-2020cc */
#define PROP_MAX_APDU_LENGTH_ACCEPTED            62
#define PROP_MAX_FAILED_ATTEMPTS                285 /* Add-135-2008j-1 */
#define PROP_MAX_INFO_FRAMES                     63
#define PROP_MAX_MANAGER                         64 /* Add-135-2020ce */
#define PROP_MAX_MASTER                          PROP_MAX_MANAGER /* renamed in 2020ce */
#define PROP_MAX_NPDU_LENGTH_ACCEPTED       4194305 /* Add-135-2020cc */
#define PROP_MAX_PRES_VALUE                      65
#define PROP_MAX_PROXIED_IAMS_PER_SECOND    4194349 /* Add-135-2020bx */
#define PROP_MAX_SEGMENTS_ACCEPTED              167 /* Add-135e-3 */
#define PROP_MAXIMUM_OUTPUT                      61
#define PROP_MAXIMUM_SEND_DELAY                 503 /* Add-135-2016bi */
#define PROP_MAXIMUM_VALUE                      135
#define PROP_MAXIMUM_VALUE_TIMESTAMP            149
#define PROP_MEMBER_OF                          159 /* Add-135c-1,2 */
#define PROP_MEMBER_STATUS_FLAGS                347 /* Add-135-2008p */
#define PROP_MEMBERS                            286 /* Add-135-2008j-1 */
#define PROP_MIN_ACTUAL_VALUE                   383 /* Add-135-2010i-1 */
#define PROP_MIN_PRES_VALUE                      69
#define PROP_MINIMUM_OFF_TIME                    66
#define PROP_MINIMUM_ON_TIME                     67
#define PROP_MINIMUM_OUTPUT                      68
#define PROP_MINIMUM_VALUE                      136
#define PROP_MINIMUM_VALUE_TIMESTAMP            150
#define PROP_MODE                               160 /* Add-135c-1,2 */
#define PROP_MODEL_NAME                          70
#define PROP_MODIFICATION_DATE                   71
#define PROP_MONITORED_OBJECTS                  504 /* Add-135-2016bi */
#define PROP_MUSTER_POINT                       287 /* Add-135-2008j-1 */
#define PROP_NEGATIVE_ACCESS_RULES              288 /* Add-135-2008j-1 */
#define PROP_NET_ACCESS_SECURITY_POLICIES       332 /* This property was deleted in BACnet Protocol Revision 22 */
#define PROP_NETWORK_INTERFACE_NAME             424 /* Add-135-2012ai-1 */
#define PROP_NETWORK_NUMBER                     425 /* Add-135-2012ai-1 */
#define PROP_NETWORK_NUMBER_QUALITY             426 /* Add-135-2012ai-1 */
#define PROP_NETWORK_TYPE                       427 /* Add-135-2012ai-1 */
#define PROP_NEXT_STOPPING_FLOOR                476 /* Add-135-2012aq */
#define PROP_NODE_SUBTYPE                       207 /* Add-135-2004d-1 */
#define PROP_NODE_TYPE                          208 /* Add-135-2004d-1 */
#define PROP_NOTIFICATION_CLASS                  17
#define PROP_NOTIFICATION_THRESHOLD             137
#define PROP_NOTIFY_TYPE                         72
#define PROP_NUMBER_OF_APDU_RETRIES              73
#define PROP_NUMBER_OF_AUTHN_POLICIES           289 /* Add-135-2008j-1 */
#define PROP_NUMBER_OF_STATES                    74
#define PROP_OBJECT_IDENTIFIER                   75
#define PROP_OBJECT_LIST                         76
#define PROP_OBJECT_NAME                         77
#define PROP_OBJECT_PROPERTY_REFERENCE           78
#define PROP_OBJECT_TYPE                         79
#define PROP_OCCUPANCY_COUNT                    290 /* Add-135-2008j-1 */
#define PROP_OCCUPANCY_COUNT_ADJUST             291 /* Add-135-2008j-1 */
#define PROP_OCCUPANCY_COUNT_ENABLE             292 /* Add-135-2008j-1 */
#define PROP_OCCUPANCY_EXEMPTION                293 /* This property was deleted in BACnet 1.13 */
#define PROP_OCCUPANCY_LOLIMIT                  294 /* Add-135-2008j-1 */
#define PROP_OCCUPANCY_LOLIMIT_ENFORCED         295 /* Add-135-2008j-1 */
#define PROP_OCCUPANCY_STATE                    296 /* Add-135-2008j-1 */
#define PROP_OCCUPANCY_UPLIMIT                  297 /* Add-135-2008j-1 */
#define PROP_OCCUPANCY_UPLIMIT_ENFORCED         298 /* Add-135-2008j-1 */
#define PROP_OPERATIONAL_CERT_FILE          4194306 /* Add-135-2020cc */
#define PROP_OPERATION_DIR                      477 /* Add-135-2012aq */
#define PROP_OPERATION_EXPECTED                 161 /* Add-135c-1,2 */
#define PROP_OPTIONAL                            80
#define PROP_OUT_OF_SERVICE                      81
#define PROP_OUTPUT_UNITS                        82
#define PROP_OVERRIDE_COLOR_REFERENCE       4194332 /* Add-135-2020ca */
#define PROP_PACKET_REORDER_TIME                333 /* This property was deleted in BACnet Protocol Revision 22 */
#define PROP_PASSBACK_EXEMPTION                 299 /* This property was deleted in BACnet 1.13 */
#define PROP_PASSBACK_MODE                      300 /* Add-135-2008j-1 */
#define PROP_PASSBACK_TIMEOUT                   301 /* Add-135-2008j-1 */
#define PROP_PASSENGER_ALARM                    478 /* Add-135-2012aq */
#define PROP_POLARITY                            84
#define PROP_PORT_FILTER                        363 /* Add-135-2010af-29 */
#define PROP_POSITIVE_ACCESS_RULES              302 /* Add-135-2008j-1 */
#define PROP_POWER                              384 /* Add-135-2010i-1 */
#define PROP_POWER_MODE                         479 /* Add-135-2012aq */
#define PROP_PRESCALE                           185 /* Add-135-2001c-4,5 */
#define PROP_PRESENT_STAGE                      493 /* Add-135-2016bd */
#define PROP_PRESENT_VALUE                       85
#define PROP_PREVIOUS_NOTIFY_TIME               138 /* This property was deleted in BACnet 1.3 */
#define PROP_PRIORITY                            86
#define PROP_PRIORITY_ARRAY                      87
#define PROP_PRIORITY_FOR_WRITING                88
#define PROP_PROCESS_IDENTIFIER                  89
#define PROP_PROCESS_IDENTIFIER_FILTER          361 /* Add-135-2010af-29 */
#define PROP_PROFILE_LOCATION                   485 /* Add-135-2012ba */
#define PROP_PROFILE_NAME                       168 /* Add-135e-2 */
#define PROP_PROGRAM_CHANGE                      90
#define PROP_PROGRAM_LOCATION                    91
#define PROP_PROGRAM_STATE                       92
#define PROP_PROPERTY_LIST                      371 /* Add-135-2010ao-5 */
#define PROP_PROPORTIONAL_CONST                  93
#define PROP_PROPORTIONAL_CONST_UNITS            94
#define PROP_PROT_CONFORMANCE_CLASS              95 /* This property was deleted in BACnet 1.2 */
#define PROP_PROTOCOL_LEVEL                     482 /* Add-135-2012bf-1 */
#define PROP_PROT_OBJ_TYPES_SUPPORTED            96
#define PROP_PROTOCOL_REVISION                   139
#define PROP_PROT_SERVICES_SUPPORTED             97
#define PROP_PROT_VERSION                        98
#define PROP_PULSE_RATE                         186 /* Add-135-2001c-4,5 */
#define PROP_READ_ONLY                           99
#define PROP_REASON_FOR_DISABLE                 303 /* Add-135-2008j-1 */
#define PROP_REASON_FOR_HALT                    100
#define PROP_RECIPIENT                          101 /* This property was deleted in BACnet 1.4 */
#define PROP_RECIPIENT_LIST                     102
#define PROP_RECORD_COUNT                       141
#define PROP_RECORDS_SINCE_NOTIFICATION         140
#define PROP_REFERENCE_PORT                     483 /* Add-135-2012bf-1 */
#define PROP_REGISTERED_CAR_CALL                480 /* Add-135-2012aq */
#define PROP_RELIABILITY                        103
#define PROP_RELIABILITY_EVAL_INHIBIT           357 /* Add-135-2010af-20 */
#define PROP_RELINQUISH_DEFAULT                 104
#define PROP_REPRESENTS                         491 /* Add-135-2012ba */
#define PROP_REQUESTED_SHED_LEVEL               218 /* Add-135-2004e */
#define PROP_REQUESTED_UPDATE_INTERVAL          348 /* Add-135-2008p */
#define PROP_REQUIRED                           105
#define PROP_RESERVED_18                         18 /* the property in this place was deleted */
#define PROP_RESOLUTION                         106
#define PROP_RESTART_NOTIF_RECIPIENTS           202 /* Add-135-2004b-5 */
#define PROP_RESTORE_COMPLETION_TIME            340 /* Add-135-2008n-1 */
#define PROP_RESTORE_PREPARATION_TIME           341 /* Add-135-2008n-1 */
#define PROP_ROUTING_TABLE                      428 /* Add-135-2012ai-1 */
#define PROP_SC_CONN_WAIT_TIMEOUT           4194308 /* Add-135-2020cc */
#define PROP_SC_DC_ACCEPT_ENABLE            4194309 /* Add-135-2020cc */
#define PROP_SC_DC_ACCEPT_URIS              4194310 /* Add-135-2020cc */
#define PROP_SC_DC_BINDING                  4194311 /* Add-135-2020cc */
#define PROP_SC_DC_CONN_STATUS              4194312 /* Add-135-2020cc */
#define PROP_SC_DC_INITIATE_ENABLE          4194313 /* Add-135-2020cc */
#define PROP_SC_DISCONN_WAIT_TIMEOUT        4194314 /* Add-135-2020cc */
#define PROP_SC_FAILED_CONN_REQS            4194315 /* Add-135-2020cc */
#define PROP_SC_FAILOVER_HUB_CONN_STATUS    4194316 /* Add-135-2020cc */
#define PROP_SC_FAILOVER_HUB_URI            4194317 /* Add-135-2020cc */
#define PROP_SC_HUB_CONNECTOR_STATE         4194318 /* Add-135-2020cc */
#define PROP_SC_HUB_FUNC_ACCEPT_URIS        4194319 /* Add-135-2020cc */
#define PROP_SC_HUB_FUNC_BINDING            4194320 /* Add-135-2020cc */
#define PROP_SC_HUB_FUNC_CONN_STATUS        4194321 /* Add-135-2020cc */
#define PROP_SC_HUB_FUNC_ENABLE             4194322 /* Add-135-2020cc */
#define PROP_SC_HEARTBEAT_TIMEOUT           4194323 /* Add-135-2020cc */
#define PROP_SC_PRIMARY_HUB_CONN_STATUS     4194324 /* Add-135-2020cc */
#define PROP_SC_PRIMARY_HUB_URI             4194325 /* Add-135-2020cc */
#define PROP_SC_MAXIMUM_RECONN_TIME         4194326 /* Add-135-2020cc */
#define PROP_SC_MINIMUM_RECONN_TIME         4194327 /* Add-135-2020cc */
#define PROP_SCALE                              187 /* Add-135-2001c-4,5 */
#define PROP_SCALE_FACTOR                       188 /* Add-135-2001c-4,5 */
#define PROP_SCHEDULE_DEFAULT                   174 /* Add-135-2001a */
#define PROP_SECURED_STATUS                     235 /* Add-135-2004f */
#define PROP_SECURITY_PDU_TIMEOUT               334 /* This property was deleted in BACnet Protocol Revision 22 */
#define PROP_SECURITY_TIME_WINDOW               335 /* This property was deleted in BACnet Protocol Revision 22 */
#define PROP_SEGMENTATION_SUPPORTED             107
#define PROP_SEND_NOW                           505 /* Add-135-2016bi */
#define PROP_SERIAL_NUMBER                      372 /* Add-135-2010ak-3 */
#define PROP_SETPOINT                           108
#define PROP_SETPOINT_REFERENCE                 109
#define PROP_SETTING                            162 /* Add-135c-1,2 */
#define PROP_SHED_DURATION                      219 /* Add-135-2004e */
#define PROP_SHED_LEVEL_DESCRIPTIONS            220 /* Add-135-2004e */
#define PROP_SHED_LEVELS                        221 /* Add-135-2004e */
#define PROP_SILENCED                           163 /* Add-135c-1,2 */
#define PROP_SUBORDINATE_ADDRESS_BINDING        171 /* Add-135-2020ce */
#define PROP_SLAVE_ADDRESS_BINDING              PROP_SUBORDINATE_ADDRESS_BINDING /* Add-135-2001a */ /* renamed in 2020ce */
#define PROP_SUBORDINATE_PROXY_ENABLE           172 /* Add-135-2020ce */
#define PROP_SLAVE_PROXY_ENABLE                 PROP_SUBORDINATE_PROXY_ENABLE /* Add-135-2001a */ /* renamed in 2020ce */
#define PROP_STAGES                             494 /* Add-135-2016bd */
#define PROP_STAGE_NAMES                        495 /* Add-135-2016bd */
#define PROP_START_TIME                         142
#define PROP_STATE_CHANGE_VALUES                396 /* Add-135-2012ay-1 */
#define PROP_STATE_DESCRIPTION                  222 /* Add-135-2004e */
#define PROP_STATE_TEXT                         110
#define PROP_STATUS_FLAGS                       111
#define PROP_STOP_TIME                          143
#define PROP_STOP_WHEN_FULL                     144
#define PROP_STRUCTURED_OBJECT_LIST             209 /* Add-135-2004d-1 */
#define PROP_STRIKE_COUNT                       391 /* Add-135-2012az-1 */
#define PROP_SUBORDINATE_ANNOTATIONS            210 /* Add-135-2004d-1 */
#define PROP_SUBORDINATE_LIST                   211 /* Add-135-2004d-1 */
#define PROP_SUBORDINATE_NODE_TYPES             487 /* Add-135-2012ba */
#define PROP_SUBORDINATE_RELATIONSHIPS          489 /* Add-135-2012ba */
#define PROP_SUBORDINATE_TAGS                   488 /* Add-135-2012ba */
#define PROP_SUBSCRIBED_RECIPIENTS              362 /* Add-135-2010af-29 */
#define PROP_SUPPORTED_FORMAT_CLASSES           305 /* Add-135-2008j-1 */
#define PROP_SUPPORTED_FORMATS                  304 /* Add-135-2008j-1 */
#define PROP_SUPPORTED_SECURITY_ALGORITHMS      336 /* This property was deleted in BACnet Protocol Revision 22 */
#define PROP_SYSTEM_STATUS                      112
#define PROP_TAGS                               486 /* Add-135-2012ba */
#define PROP_TARGET_REFERENCES                  496 /* Add-135-2016bd */
#define PROP_THREAT_AUTHORITY                   306 /* Add-135-2008j-1 */
#define PROP_THREAT_LEVEL                       307 /* Add-135-2008j-1 */
#define PROP_TIME_DELAY                         113
#define PROP_TIME_DELAY_NORMAL                  356 /* Add-135-2010af-19 */
#define PROP_TIME_OF_ACTIVE_TIME_RESET          114
#define PROP_TIME_OF_DEVICE_RESTART             203 /* Add-135-2004b-5 */
#define PROP_TIME_OF_STATE_COUNT_RESET          115
#define PROP_TIME_OF_STRIKE_COUNT_RESET         392 /* Add-135-2012az-1 */
#define PROP_TIME_SYNC_INTERVAL                 204 /* Add-135-2004b-6 */
#define PROP_TIME_SYNC_RECIPIENTS               116
#define PROP_TIMER_RUNNING                      397 /* Add-135-2012ay-1 */
#define PROP_TIMER_STATE                        398 /* Add-135-2012ay-1 */
#define PROP_TOTAL_RECORD_COUNT                 145
#define PROP_TRACE_FLAG                         308 /* Add-135-2008j-1 */
#define PROP_TRACKING_VALUE                     164 /* Add-135c-1,2 */
#define PROP_TRANSACTION_NOTIF_CLASS            309 /* Add-135-2008j-1 */
#define PROP_TRANSITION                         385 /* Add-135-2010i-1 */
#define PROP_TRIGGER                            205 /* Add-135-2004b-3 */
#define PROP_TRIM_FADE_TIME                 4194337 /* Add-135-2020ca-Errate */
#define PROP_UNITS                              117
#define PROP_UPDATE_INTERVAL                    118
#define PROP_UPDATE_KEY_SET_TIMEOUT             337 /* This property was deleted in BACnet Protocol Revision 22 */
#define PROP_UPDATE_TIME                        189 /* Add-135-2001c-4,5 */
#define PROP_USER_EXTERNAL_ID                   310 /* Add-135-2008j-1 */
#define PROP_USER_INFO_REF                      311 /* Add-135-2008j-1 */
#define PROP_USER_NAME                          317 /* Add-135-2008j-1 */
#define PROP_USER_TYPE                          318 /* Add-135-2008j-1 */
#define PROP_USES_REMAINING                     319 /* Add-135-2008j-1 */
#define PROP_UTC_OFFSET                         119
#define PROP_UTC_TIME_SYNC_RECIPIENTS           206 /* Add-135-2004b-6 */
#define PROP_VALID_SAMPLES                      146
#define PROP_VALUE_BEFORE_CHANGE                190 /* Add-135-2001c-4,5 */
#define PROP_VALUE_CHANGE_TIME                  192 /* Add-135-2001c-4,5 */
#define PROP_VALUE_SET                          191 /* Add-135-2001c-4,5 */
#define PROP_VALUE_SOURCE                       433 /* Add-135-2012as */
#define PROP_VALUE_SOURCE_ARRAY                 434 /* Add-135-2012as */
#define PROP_VARIANCE_VALUE                     151
#define PROP_VENDOR_IDENTIFIER                  120
#define PROP_VENDOR_NAME                        121
#define PROP_VERIFICATION_TIME                  326 /* Add-135-2008j-1 */
#define PROP_VIRTUAL_MAC_ADDRESS_TABLE          429 /* Add-135-2012ai-1 */
#define PROP_VT_CLASSES_SUPPORTED               122
#define PROP_WEEKLY_SCHEDULE                    123
#define PROP_WINDOW_INTERVAL                    147
#define PROP_WINDOW_SAMPLES                     148
#define PROP_WRITE_EVERY_SCHEDULED_ACTION   4194333 /* Add-135-2020bv */
#define PROP_WRITE_STATUS                       370 /* Add-135-2010aa-1 */
#define PROP_ZONE_FROM                          320 /* Add-135-2008j-1 */
#define PROP_ZONE_MEMBERS                       165 /* Add-135c-1,2 */
#define PROP_ZONE_TO                            321 /* Add-135-2008j-1 */
#define PROP_END_TRIM            PROP_HIGH_END_TRIM /* for backward compatibility only */
#define PROP_LAST_KNOWN_ID                  4194349
#define PROP_FIRST_PROPRIETARY                  512
#define PROP_PRIVATE_KEY_FILE_NAME            14030 /* SC DL NPO proprietary */
#define PROP_OPERATIONAL_CERT_FILE_NAME       14031 /* SC DL NPO proprietary */
#define PROP_ISSUER_CERT_FILE_NAMES           14032 /* SC DL NPO proprietary */
#define PROP_CERT_SIGNING_REQ_FILE_NAME       14033 /* SC DL NPO proprietary */
#define PROP_FIRST_EXTENDED                 4194304
#define PROP_MAX_ID                BACSTAC_UI32_MAX
#define BACSTAC_PROPERTY_ID_MAX    BACSTAC_UI32_MAX

#endif /* BNPROPID_H */
