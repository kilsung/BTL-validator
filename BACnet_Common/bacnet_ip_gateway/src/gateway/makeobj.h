/*
 * MAKEOBJ.H - BACstac(TM) Dynamic Object Creation Macros
 * Adapted for Modbus-BACnet Gateway.
 */

#ifndef MAKEOBJ_H_GATEWAY
#define MAKEOBJ_H_GATEWAY

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#include <assert.h>
#include <string.h>
#include <bactypes.h>
#include <bacdb.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

#define INIT_PROPVF(prop_id, type, value, flags)                                      \
do {                                                                                  \
	assert(i_ < ARRAY_SIZE(propList_));                                           \
	BACstacInitContentsToVar(&pc_, DATA_TYPE_##type, (value), sizeof(*(value)));  \
	BACstacInitDbProperty(propList_ + i_, PROP_##prop_id, &pc_, (flags));         \
	i_++;                                                                         \
} while (0)

#define INIT_PROPV(prop_id, type, value) \
	INIT_PROPVF(prop_id, type, (value), BACSTAC_REG_PROP)

#define INIT_PROPSF(prop_id, str, flags)                                              \
do {                                                                                  \
	assert(i_ < ARRAY_SIZE(propList_));                                           \
	BACstacInitContentsToVar(&pc_, DATA_TYPE_UTF_8_STRING, str, strlen(str) + 1); \
	BACstacInitDbProperty(propList_ + i_, PROP_##prop_id, &pc_, (flags));         \
	i_++;                                                                         \
} while (0)

#define INIT_PROPS(prop_id, str) \
	INIT_PROPSF(prop_id, str, BACSTAC_REG_PROP)

#define INIT_PROPA(prop_id, type, array, item_count)                                   \
do {                                                                                   \
	assert(i_ < ARRAY_SIZE(propList_));                                            \
	BACstacInitContentsToArray(&pc_, DATA_TYPE_##type, array,                      \
		sizeof (array[0]) * item_count, item_count);                           \
	BACstacInitDbProperty(propList_ + i_, PROP_##prop_id, &pc_, BACSTAC_REG_PROP); \
	i_++;                                                                          \
} while (0)

/* IMPORTANT: prop_count is the maximum number of properties in the object definition,
 * including three properties that are added by CREATE_OBJ_BEGIN automatically. */
#define CREATE_OBJ_BEGIN(obj_type, inst_num, obj_name, prop_count)                     \
do {                                                                                   \
	(void)(prop_count);                                                            \
	BACSTAC_DB_OBJECT dbObject_;                                                   \
	BACSTAC_DB_PROPERTY propList_[32];                                             \
	memset(propList_, 0, sizeof(propList_));                                       \
	BACSTAC_PROPERTY_CONTENTS pc_;                                                 \
	BACSTAC_PROPERTY_COUNT i_ = 0;                                                 \
	BACSTAC_OBJECT_ID objectID_;                                                   \
	const BACSTAC_OBJECT_TYPE objectType_ = (obj_type);                            \
                                                                                       \
	BACstacInitObjectID(&objectID_, objectType_, (inst_num));                      \
                                                                                       \
	INIT_PROPV(OBJECT_IDENTIFIER, OBJECT_ID, &objectID_);                          \
	INIT_PROPS(OBJECT_NAME, (obj_name));                                           \
	INIT_PROPV(OBJECT_TYPE, ENUM, &objectType_);                                   \

#define CREATE_OBJ_END(status, phObj, pFirstFailed)                                    \
	BACstacInitDbObject(&dbObject_, propList_, i_);                                \
	(status) = BACstacCreateDBObject(hDev, &dbObject_, (phObj), (pFirstFailed));   \
} while (0)

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* MAKEOBJ_H_GATEWAY */
