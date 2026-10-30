/*
 * Project: BACstac (TM) examples
 *
 * Copyright (c) 2009-2022 by Cimetrics Inc.  All rights reserved.
 *
 * Purpose: Additional useful routines to access to the BACstac database
 *
 * Author: Dmitry Potapov
 */
#ifndef BACSTAC_UTILS_DBACC_H
#define BACSTAC_UTILS_DBACC_H

#include <bactypes.h>

BACSTAC_STATUS SetBoolProperty (
	BACSTAC_HOBJECT     hObject,
	BACSTAC_PROPERTY_ID propertyID,
	BACSTAC_BOOLEAN             value);

BACSTAC_STATUS SetUnsignedProperty (
	BACSTAC_HOBJECT     hObject,
	BACSTAC_PROPERTY_ID propertyID,
	BACSTAC_UNSIGNED    value);

BACSTAC_STATUS SetUInt64Property (
	BACSTAC_HOBJECT     hObject,
	BACSTAC_PROPERTY_ID propertyID,
	BACSTAC_UI64        value);

BACSTAC_STATUS SetRealProperty (
	BACSTAC_HOBJECT     hObject,
	BACSTAC_PROPERTY_ID propertyID,
	BACSTAC_REAL        value);

BACSTAC_STATUS SetEnumProperty (
	BACSTAC_HOBJECT     hObject,
	BACSTAC_PROPERTY_ID propertyID,
	BACSTAC_ENUM        value);

BACSTAC_STATUS GetBoolProperty (
	BACSTAC_HOBJECT     hObject,
	BACSTAC_PROPERTY_ID propertyID,
	BACSTAC_BOOLEAN             *pValue);

BACSTAC_STATUS GetUnsignedProperty (
	BACSTAC_HOBJECT     hObject,
	BACSTAC_PROPERTY_ID propertyID,
	BACSTAC_UNSIGNED    *pValue);

BACSTAC_STATUS GetUInt64Property (
	BACSTAC_HOBJECT     hObject,
	BACSTAC_PROPERTY_ID propertyID,
	BACSTAC_UI64        *pValue);

BACSTAC_STATUS GetRealProperty (
	BACSTAC_HOBJECT     hObject,
	BACSTAC_PROPERTY_ID propertyID,
	BACSTAC_REAL        *pValue);

BACSTAC_STATUS GetEnumProperty (
	BACSTAC_HOBJECT     hObject,
	BACSTAC_PROPERTY_ID propertyID,
	BACSTAC_ENUM        *pValue);


#endif /* BACSTAC_UTILS_DBACC_H */
