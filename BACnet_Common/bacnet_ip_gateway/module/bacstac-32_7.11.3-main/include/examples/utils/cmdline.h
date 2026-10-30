/*
 * CMDLINE.H - BACstac(TM) Command Line Utilities
 *
 * Copyright (c) 1998-2023 by Cimetrics Inc.  All rights reserved.
 * Author: Keith Corbett
 */

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#include <bnetdef.h>
#include <bneterr.h>
#include <bacbase.h>
#include <bactypes.h>
#include <bacacc.h>
#include "bacenum.h"

	typedef enum
		{
			ARG_ADDRESS,
			ARG_BOOL,
			ARG_REAL,
			ARG_INTEGER,
			ARG_UNSIGNED,
			ARG_UI64,
			ARG_STRING,
			ARG_ENUM,
			ARG_BIT_STRING,
			ARG_OBJECT_ID,
			LAST_KNOWN_ARG_TYPE = ARG_OBJECT_ID
		} BACSTAC_ARG_TYPE;

	typedef struct tagBACSTAC_ARG_DESC
		{
			BACSTAC_ARG_TYPE argType;
			void *pVar;
			BACSTAC_UINT size;       /* or bit count */
			ENUM_TYPE_ID enumTypeID;
			char flag;
			const char *longFlag;
			const char *helpString;
		} BACSTAC_ARG_DESC;

	#define ARG_ADDRESS_INIT(address, flag, helpString) \
		{ARG_ADDRESS, (address), sizeof(BACSTAC_ADDRESS), ENUM_END, (flag), NULL, helpString}
	#define ARG_BOOL_INIT(boolean, flag, helpString) \
		{ARG_BOOL, (boolean), sizeof(BACSTAC_BOOLEAN), ENUM_END, (flag), NULL, helpString}
	#define ARG_REAL_INIT(real, flag, helpString) \
		{ARG_REAL, (real), sizeof(BACSTAC_REAL), ENUM_END, (flag), NULL, helpString}
	#define ARG_INTEGER_INIT(integer, flag, helpString) \
		{ ARG_INTEGER, (integer), sizeof(BACSTAC_INTEGER), ENUM_END, (flag), NULL, helpString}
	#define ARG_UNSIGNED_INIT(uint, flag, helpString) \
		{ARG_UNSIGNED, (uint), sizeof(BACSTAC_UNSIGNED), ENUM_END, (flag), NULL, helpString}
	#define ARG_UI64_INIT(uint, flag, helpString) \
		{ARG_UI64, (uint), sizeof(BACSTAC_UI64), ENUM_END, (flag), NULL, helpString}
	#define ARG_STRING_INIT(string, flag, helpString) \
		{ARG_STRING, (void*)(string), sizeof(char*), ENUM_END, (flag), NULL, helpString}
	#define ARG_ENUM_INIT(e, enumTypeID, flag, helpString) \
		{ARG_ENUM, (e), sizeof(BACSTAC_ENUM), (enumTypeID), (flag), NULL, helpString}
	#define ARG_BIT_STRING_INIT(bit_str, size, flag, helpString) \
		{ARG_BIT_STRING, (bit_str), (size), ENUM_END, (flag), NULL, helpString}
	#define ARG_OBJECT_ID_INIT(obj_id, flag, helpString) \
		{ARG_OBJECT_ID, (obj_id), sizeof(BACSTAC_OBJECT_ID), ENUM_END, (flag), NULL, helpString}

	#define ARG_ADDRESS_INIT_EX(address, flag, long_flag, helpString) \
		{ARG_ADDRESS, (address), sizeof(BACSTAC_ADDRESS), ENUM_END, (flag), long_flag, helpString}
	#define ARG_BOOL_INIT_EX(boolean, flag, long_flag, helpString) \
		{ARG_BOOL, (boolean), sizeof(BACSTAC_BOOLEAN), ENUM_END, (flag), long_flag, helpString}
	#define ARG_REAL_INIT_EX(real, flag, long_flag, helpString) \
		{ARG_REAL, (real), sizeof(BACSTAC_REAL), ENUM_END, (flag), long_flag, helpString}
	#define ARG_INTEGER_INIT_EX(integer, flag, long_flag, helpString) \
		{ ARG_INTEGER, (integer), sizeof(BACSTAC_INTEGER), ENUM_END, (flag), long_flag, helpString}
	#define ARG_UNSIGNED_INIT_EX(uint, flag, long_flag, helpString) \
		{ARG_UNSIGNED, (uint), sizeof(BACSTAC_UNSIGNED), ENUM_END, (flag), long_flag, helpString}
	#define ARG_UI64_INIT_EX(uint, flag, long_flag, helpString) \
		{ARG_UI64, (uint), sizeof(BACSTAC_UI64), ENUM_END, (flag), long_flag, helpString}
	#define ARG_STRING_INIT_EX(string, flag, long_flag, helpString) \
		{ARG_STRING, (void*)(string), sizeof(char*), ENUM_END, (flag), long_flag, helpString}
	#define ARG_ENUM_INIT_EX(e, enumTypeID, flag, long_flag, helpString) \
		{ARG_ENUM, (e), sizeof(BACSTAC_ENUM), (enumTypeID), (flag), long_flag, helpString}
	#define ARG_BIT_STRING_INIT_EX(bit_str, size, flag, long_flag, helpString) \
		{ARG_BIT_STRING, (bit_str), (size), ENUM_END, (flag), long_flag, helpString}
	#define ARG_OBJECT_ID_INIT_EX(obj_id, flag, long_flag, helpString) \
		{ARG_OBJECT_ID, (obj_id), sizeof(BACSTAC_OBJECT_ID), ENUM_END, (flag), long_flag, helpString}


#ifdef BACSTAC_USE_64_BIT_READ_RANGE
	#define ARG_REF_INDEX_INIT ARG_UI64_INIT
	#define ARG_SEQ_NUM_INIT ARG_UI64_INIT
	#define ARG_REF_INDEX_INIT_EX ARG_UI64_INIT_EX
	#define ARG_SEQ_NUM_INIT_EX ARG_UI64_INIT_EX
#else /* BACSTAC_USE_64_BIT_READ_RANGE */
	#define ARG_REF_INDEX_INIT ARG_UNSIGNED_INIT
	#define ARG_SEQ_NUM_INIT ARG_UNSIGNED_INIT
	#define ARG_REF_INDEX_INIT_EX ARG_UNSIGNED_INIT_EX
	#define ARG_SEQ_NUM_INIT_EX ARG_UNSIGNED_INIT_EX
#endif /* BACSTAC_USE_64_BIT_READ_RANGE */


/*
 * BACstacParseOptions parses a list of options.
 *
 * Parameters:
 *  argc  - the number of options to parse.
 *  argv  - the array of options to parse.
 *  nArgs - the size of the description array.
 *  pArgs - the array that contains description of supported options.
 *
 * Return Value: the number of processed options or -1 on error.
 */
	int BACstacParseOptions (
		int argc,
		const char **argv,
		BACSTAC_ELEMENT_COUNT nArgs,
		const BACSTAC_ARG_DESC * pArgs
	);

	BACSTAC_BOOLEAN BACstacParseCommandLine (
		int argc,
		const char **argv,
		const BACSTAC_ARG_DESC *pArgs,
		BACSTAC_ELEMENT_COUNT nArgs
	);

	typedef struct tagPRINT_HELP
		{
			const char *pHelpStr;
			size_t helpStrLen;
		} PRINT_HELP;

	void SetPrintHelpContents (
		const char *pHelpStr,
		size_t helpStrLen
	);

	void PrintHelpContents (
		void
	);

	BACSTAC_BOOLEAN FindHelpFlag (
		int argc,
		const char **argv
	);

	void PrintStartupBanner(const char *name, int year);
#ifdef __cplusplus
}
#endif /* __cplusplus */
