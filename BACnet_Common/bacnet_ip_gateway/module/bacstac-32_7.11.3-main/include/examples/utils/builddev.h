/*
 * Project: BACstac (TM) examples
 *
 * Copyright (c) 2010-2023 by Cimetrics Inc. All rights reserved.
 *
 * Purpose: define macros to build a template device using C structures.
 *
 * Author: Dmitry Potapov
 *
 * Before including this file DEVICE_FILE should be defined, which should
 * contain the description of the device and optionally other objects.
 * The format of this file is described below.
 *
 * Also DEVICE_NAME should be defined. It can be defined either before
 * including this file or inside of DEVICE_FILE.
 *
 * This file defines of the following macros:
 *
 *   OBJ_BEGIN marks the beginning of the object list.
 *   OBJ_NEXT is used as a separator between objects.
 *   OBJ_END marks the end of the object list.
 *
 *   DB_CONST defines a scalar property of the given data type.
 *          (This macro is not suitable for UTF-8 and other character strings
 *           that have a variable length. It can be used for ANY_CHAR_STRING.)
 *   DB_UTF8_STR defines a property that has the UTF-8 data type.
 *   DB_EMPTY_ARRAY defines a property that is an empty array.
 *   DB_AUTO_PROP defines an automatic property.
 *
 * NOTE: DB_ macros provides only initial values for properties, which
 *       can be modified later by BACstacStorePropertyInstance or using
 *       other BACstac API to manipulate the BACstac database.
 *
 * USAGE: This file includes DEVICE_FILE, which contain the actual description
 *        of the BACstac template. This file can look like this:
 *
 * OBJ_BEGIN
 *     DB_CONST(OBJECT_IDENTIFIER, OBJECT_ID, BACSTAC_OBJECT_ID_INIT(OBJ_DEVICE, 0), 0)
 *     DB_UTF8_STR(OBJECT_NAME, "sample device", 0)
 *     ...
 * OBJ_NEXT
 *     DB_CONST(OBJECT_IDENTIFIER, OBJECT_ID, BACSTAC_OBJECT_ID_INIT(OBJ_ANALOG_INPUT, 1), 0)
 *     DB_UTF8_STR(OBJECT_NAME, "AI-1", 0)
 *     ...
 * OBJ_END
 */

#ifndef DEVICE_FILE
#error "DEVICE_FILE is not defined"
#endif

#include <bactypes.h>
#include <bacdb.h>

#ifndef CONCAT
#define CONCAT_(x, y) x ## y
#define CONCAT(x, y) CONCAT_(x, y)
#endif

#define OBJLST DEVICE_NAME##ObjList

/*
 * Define constants referred by the property contents inside of BACSTAC_DB_PROPERTY
 */

#define DB_CONST(propId,type,value,flags) \
	static const BACSTAC_##type CONCAT(prop, __LINE__) = value;
#define DB_VAR(propId,type,name,flags) \
	BACSTAC_##type name;
#define DB_UTF8_STR(propId,value,flags)
#define DB_EMPTY_ARRAY(propId,type,flags)
#define DB_ARRAY(propId,type,name,n,flags) \
	BACSTAC_##type name[(n)];
#define DB_AUTO_PROP(propId)
#define OBJ_BEGIN
#define OBJ_NEXT
#define OBJ_END

#include DEVICE_FILE

#undef DB_CONST
#undef DB_VAR
#undef DB_UTF8_STR
#undef DB_EMPTY_ARRAY
#undef DB_ARRAY
#undef DB_AUTO_PROP
#undef OBJ_BEGIN
#undef OBJ_NEXT
#undef OBJ_END

#ifndef DEVICE_NAME
#error "DEVICE_NAME is not defined"
#endif

/*
 * Initialize list of properties for each object
 */

#define DB_CONST(propId,type,value,flags) \
	BACSTAC_DB_PROPERTY_INIT(PROP_##propId, \
		BACSTAC_CONTENTS_INIT_TO_VAR(DATA_TYPE_##type, CONCAT(prop, __LINE__)), \
		BACSTAC_REG_PROP | (flags)),
#define DB_VAR(propId,type,name,flags) \
	BACSTAC_DB_PROPERTY_INIT(PROP_##propId, \
		BACSTAC_CONTENTS_INIT_TO_VAR(DATA_TYPE_##type, name), \
		BACSTAC_REG_PROP | (flags)),
#define DB_UTF8_STR(propId,value,flags) \
	BACSTAC_DB_PROPERTY_INIT(PROP_##propId, \
		BACSTAC_CONTENTS_INIT_TO_VAR(DATA_TYPE_UTF_8_STRING, value), \
		BACSTAC_REG_PROP | (flags)),
#define DB_EMPTY_ARRAY(propId,type,flags) \
	BACSTAC_DB_PROPERTY_INIT(PROP_##propId, \
		BACSTAC_EMPTY_ARRAY_CONTENTS_INIT(DATA_TYPE_##type), \
		BACSTAC_REG_PROP | (flags)),
#define DB_ARRAY(propId,type,name,n,flags) \
	BACSTAC_DB_PROPERTY_INIT(PROP_##propId, \
		BACSTAC_CONTENTS_INIT_TO_ARRAY(DATA_TYPE_##type, name), \
		BACSTAC_REG_PROP | (flags)),
#define DB_AUTO_PROP(propId) \
	BACSTAC_DB_PROPERTY_INIT(PROP_##propId, BACSTAC_EMPTY_CONTENTS_INIT, BACSTAC_AUTO_PROP),

#define OBJ_BEGIN \
	static const BACSTAC_DB_PROPERTY CONCAT(proplist, __LINE__)[] = {
#define OBJ_NEXT OBJ_END OBJ_BEGIN
#define OBJ_END \
	};

#include DEVICE_FILE

#undef DB_CONST
#undef DB_VAR
#undef DB_UTF8_STR
#undef DB_EMPTY_ARRAY
#undef DB_ARRAY
#undef DB_AUTO_PROP
#undef OBJ_BEGIN
#undef OBJ_NEXT
#undef OBJ_END


/*
 * Initialize the list of objects in the device
 */

#define DB_CONST(propId,type,value,flags)
#define DB_VAR(propId,type,name,flags)
#define DB_UTF8_STR(propId,value,flags)
#define DB_EMPTY_ARRAY(propId,type,flags)
#define DB_ARRAY(propId,type,name,n,flags)
#define DB_AUTO_PROP(propId)
#define OBJ_BEGIN \
	BACSTAC_DB_OBJECT_INIT( \
		sizeof(CONCAT(proplist, __LINE__))/sizeof(CONCAT(proplist, __LINE__)[0]), \
		CONCAT(proplist, __LINE__))
#define OBJ_NEXT , OBJ_BEGIN
#define OBJ_END

static const BACSTAC_DB_OBJECT OBJLST[] = {
#include DEVICE_FILE
};

#undef DB_CONST
#undef DB_VAR
#undef DB_UTF8_STR
#undef DB_EMPTY_ARRAY
#undef DB_ARRAY
#undef DB_AUTO_PROP
#undef OBJ_BEGIN
#undef OBJ_NEXT
#undef OBJ_END

/*
 * Initialize the device template
 */
const BACSTAC_DB_DEVICE DEVICE_NAME = BACSTAC_DB_DEVICE_INIT(
		sizeof (OBJLST)/sizeof(OBJLST[0]), OBJLST);
