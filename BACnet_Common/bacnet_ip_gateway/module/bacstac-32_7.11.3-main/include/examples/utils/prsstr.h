/*
 * BACstac(TM) BACstac module PRSSTR.H
 *
 * Copyright (c) 2018-2022 by Cimetrics Inc. All rights reserved.
 *
 * Purpose: Interface to Parse<Type> functions.
 *
 * Author: Dmitry Potapov
 */

#ifndef EXAMPLES_PRSSTR_H
#define EXAMPLES_PRSSTR_H

#include <bactypes.h>
#include "bacenum.h" /* for ENUM_TYPE_ID */

typedef struct PARSER_BUFFER {
	BACSTAC_BYTE *cur;
	BACSTAC_BYTE *end;
} PARSER_BUFFER;
void ParserBufferInit(PARSER_BUFFER *buf, void *ptr, size_t size);
void *ParserBufferAlloc(PARSER_BUFFER *buf, size_t size);

char *SkipSpaces(const char *ptr);

int ParseBooleanValue(BACSTAC_BOOLEAN *pOut, const char *ptr);
int ParseUnsignedValue(BACSTAC_UNSIGNED *pOut, const char *ptr);
int ParseUInt64Value(BACSTAC_UI64 *pOut, const char *ptr);
int ParseIntegerValue(BACSTAC_INTEGER *pOut, const char *ptr);
int ParsePriorityLevel(BACSTAC_PRIORITY_LEVEL *pOut, const char *ptr);
int ParseRealValue(BACSTAC_REAL *pOut, const char *ptr);
int ParseDoubleValue(BACSTAC_DOUBLE *pOut, const char *ptr);
int ParseEnumValue(BACSTAC_ENUM* pOut, ENUM_TYPE_ID enumTypeID, const char *ptr);
int ParseDateValue(BACSTAC_DATE* pOut, const char *ptr);
int ParseTimeValue(BACSTAC_TIME* pOut, const char *ptr);
int ParseObjectIDValue(BACSTAC_OBJECT_ID* pOut, const char *ptr);
int ParseOctetStringValue(BACSTAC_OCTET_STRING *pOut, const char *ptr, PARSER_BUFFER *buf);
int ParseCharStringValue(BACSTAC_CHAR_STRING *pOut, const char *ptr, PARSER_BUFFER *buf);
int ParseBitStringValue(BACSTAC_BIT_STRING *pOut, const char *ptr, BACSTAC_UINT fixedLen);

#endif /* EXAMPLES_PRSSTR_H */
