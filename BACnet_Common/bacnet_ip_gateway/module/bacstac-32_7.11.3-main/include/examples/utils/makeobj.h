/*
 * MAKEOBJ.H - BACstac(TM) Dynamic Object Creation Macros
 *
 * Copyright (c) 2019-2022 by Cimetrics Inc.  All rights reserved.
 *
 * Author: Ilya Boguslavsky
 *
 * CREATE_OBJ_BEGIN begins an object definition. Three standard properties that
 *     exists in all object types: object-identifier, object-type, and object-name
 *     are added automatically to the list of properties.
 *
 * INIT_PROPV defines a scalar property with regular flags (BACSTAC_REG_PROP).
 * INIT_PROPVF is same as INIT_PROPV but also specifies property flags explicitly.
 *
 * INIT_PROPS defines a UTF-8 string property with regular flags (BACSTAC_REG_PROP).
 * INIT_PROPSF is same as INIT_PROPS but also specifies property flags explicitly.
 *
 * INIT_PROPA defines an array or list property with regular flags (BACSTAC_REG_PROP).
 * INIT_PROPAF is same as INIT_PROPA but also specifies property flags explicitly.
 *
 * CREATE_OBJ_END ends the object definition and creates the object in the
 *   BACstac database. Once the object is created, property values used in its
 *   definition is no longer used.
 */

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#include <assert.h>
#include <bactypes.h>
#include <bacdb.h>
#include "exutils.h"   /* for ARRAY_SIZE */

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
	BACSTAC_DB_OBJECT dbObject_;                                                   \
	BACSTAC_DB_PROPERTY propList_[(prop_count)];                                   \
	BACSTAC_PROPERTY_CONTENTS pc_;                                                 \
	BACSTAC_PROPERTY_COUNT i_ = 0;                                                 \
	BACSTAC_OBJECT_ID objectID_;                                                   \
	static const BACSTAC_OBJECT_TYPE objectType_ = (obj_type);                     \
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
