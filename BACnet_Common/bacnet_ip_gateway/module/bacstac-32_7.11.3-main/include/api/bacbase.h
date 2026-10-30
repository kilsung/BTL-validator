/*
 * BACstac(TM)  BACBASE.H
 *
 * Copyright (c) 1997-2026 by Cimetrics Inc. All rights reserved.
 *
 * Purpose:  Defines basic BACstac data types
 */

#ifndef BACBASE_H
#define BACBASE_H
#define bacbase_h_included

#ifdef HAVE_BAC_AUTOCONF
#include "autoconf.h"
#endif

/*
 * BACstac version
 */


#define BACSTAC_VERSION_MAJOR      7
#define BACSTAC_VERSION_MINOR      11
#define BACSTAC_VERSION_RELEASE    3


#ifdef __cplusplus
# ifndef BACSTAC_NOTHROW
# define BACSTAC_NOTHROW throw()
# endif
#else
# ifndef BACSTAC_NOTHROW
# define BACSTAC_NOTHROW
# endif
#endif /* __cplusplus */

#ifndef BACSTAC_DEPRECATED
# if defined(_MSC_VER) && (_MSC_VER >= 1300) && !defined(MIDL_PASS)
#  define BACSTAC_DEPRECATED __declspec(deprecated)
# elif defined(__GNUC__) && ((__GNUC__ > 3) || ((__GNUC__ == 3) && (__GNUC_MINOR__ >= 1)))
#  define BACSTAC_DEPRECATED __attribute__((deprecated))
# else
#  define BACSTAC_DEPRECATED
# endif
#endif

#if defined(_MSC_VER) && (_MSC_VER >= 1700)
# define BACSTAC_CHECK_RETURN _Check_return_
#elif defined(__GNUC__) && ((__GNUC__ > 3) || ((__GNUC__ == 3) && (__GNUC_MINOR__ >= 4)))
# define BACSTAC_CHECK_RETURN __attribute__((warn_unused_result))
#else
# define BACSTAC_CHECK_RETURN
#endif

#if defined(__GNUC__) && ((__GNUC__ > 3) || ((__GNUC__ == 3) && (__GNUC_MINOR__ >= 3)))
# define BACSTAC_NONNULL_ARGS(args) __attribute__ ((__nonnull__ args))
#else
#define BACSTAC_NONNULL_ARGS(args)
#endif

#ifndef BACSTACAPI
#define BACSTACAPI
#endif

#if defined(_WIN32) || defined(__CYGWIN32__)
#define BACstac_api         __stdcall
#define BACstac_hook        __stdcall
#define BACstac_callback    __stdcall
#define BACstac_interface   __stdcall
#else
#define BACstac_api
#define BACstac_hook
#define BACstac_callback
#define BACstac_interface
#endif
#define BACstac_ptr

/*
 * BACSTAC_DECLARE_HANDLE macro that defines a new BACstac type
 * of handles.
 */
#define BACSTAC_DECLARE_HANDLE(name) \
	typedef size_t name
#define BACSTAC_INVALID_HANDLE      0

/* Define source and binary backward compatibility */
/* Note: backward compatibility on the source level is preserved
 * with BACstac 6.0 wherever it was possible, but there are a few
 * exceptions. See REAME.TXT for details. */
#define BACSTAC_SRC_COMP 0x0600 /* 6.0 */
#define BACSTAC_BIN_COMP 0x070b /* 7.11 */

/* Definition of byte order macros */
#include <stddef.h>

#define BACSTAC_ORDER_LITTLE_ENDIAN 1234
#define BACSTAC_ORDER_BIG_ENDIAN    4321
/* BACSTAC_BYTE_ORDER - host byte order */
#if defined(__BYTE_ORDER__)
#  if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#    define BACSTAC_BYTE_ORDER BACSTAC_ORDER_LITTLE_ENDIAN
#  elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#    define BACSTAC_BYTE_ORDER BACSTAC_ORDER_BIG_ENDIAN
#  else
#    error "Unsupported byte order"
#  endif
#elif defined(__i386) || defined(_M_IX86) || \
      defined(__x86_64__) || defined(_M_X64) || \
      defined(__ARMEL__) || defined(__aarch64__)
#  define BACSTAC_BYTE_ORDER BACSTAC_ORDER_LITTLE_ENDIAN
#elif defined(__MIPSEB__)
#  define BACSTAC_BYTE_ORDER BACSTAC_ORDER_BIG_ENDIAN
#else
#  error "Unknown byte order"
#endif

/* Definition of the order of allocation of bit-fields.
 *
 * Currently, for all supported platforms, it is same as byte order */
#define BACSTAC_BIT_ORDER BACSTAC_BYTE_ORDER

/* Definition of floating point related macros
 *
 * BACSTAC_FLOAT_IEEE_FORMAT defines whether the processor uses single and
 * double floating point format to represent 'float' and 'double' types
 * correspondingly.
 *
 * NOTE: This macro should be always defined as either 1 (yes) or 0 (no).
 *
 * Currently, all supported platforms uses the IEEE representation. */
#define BACSTAC_FLOAT_IEEE_FORMAT 1

/* If the IEEE format is used by processor, two more macros should be
 * defined:
 *   BACSTAC_FLOAT_BYTE_ORDER defines byte order within 32-bit words.
 *   BACSTAC_FLOAT_UI32_ORDER defines the order of 32-bit words within
 *   a Double-precision floating-point number.
 * Usually, both macros have the same value as BACSTAC_BYTE_ORDER,
 * except on some little-endian ARM platforms. */
#if BACSTAC_FLOAT_IEEE_FORMAT
#  define BACSTAC_FLOAT_BYTE_ORDER BACSTAC_BYTE_ORDER
#  if defined(__ARMEL__) && !defined(__MAVERICK__) && !defined(__VFP_FP__)
#    define BACSTAC_FLOAT_UI32_ORDER BACSTAC_ORDER_BIG_ENDIAN
#  else
#    define BACSTAC_FLOAT_UI32_ORDER BACSTAC_FLOAT_BYTE_ORDER
#  endif
#endif

/* Definition of fixed-width integer types */
#include <limits.h>
#include <stdint.h>
typedef uint8_t  BACSTAC_UI8;
typedef uint16_t BACSTAC_UI16;
typedef uint32_t BACSTAC_UI32;
typedef uint64_t BACSTAC_UI64;
typedef int8_t   BACSTAC_SI8;
typedef int16_t  BACSTAC_SI16;
typedef int32_t  BACSTAC_SI32;
typedef int64_t  BACSTAC_SI64;

#define BACSTAC_UI8_C  UINT8_C
#define BACSTAC_UI16_C UINT16_C
#define BACSTAC_UI32_C UINT32_C
#define BACSTAC_UI64_C UINT64_C
#define BACSTAC_SI8_C  INT8_C
#define BACSTAC_SI16_C INT16_C
#define BACSTAC_SI32_C INT32_C
#define BACSTAC_SI64_C INT64_C

#define BACSTAC_UI8_MAX  UINT8_MAX
#define BACSTAC_UI16_MAX UINT16_MAX
#define BACSTAC_UI32_MAX UINT32_MAX
#define BACSTAC_UI64_MAX UINT64_MAX
#define BACSTAC_SI8_MIN  INT8_MIN
#define BACSTAC_SI16_MIN INT16_MIN
#define BACSTAC_SI32_MIN INT32_MIN
#define BACSTAC_SI64_MIN INT64_MIN
#define BACSTAC_SI8_MAX  INT8_MAX
#define BACSTAC_SI16_MAX INT16_MAX
#define BACSTAC_SI32_MAX INT32_MAX
#define BACSTAC_SI64_MAX INT64_MAX

/* End definition of fixed-width integer types */

typedef BACSTAC_UI8 BACSTAC_BYTE, *PBACSTAC_BYTE;

/* Boolean */
#ifdef __cplusplus
typedef bool BACSTAC_BOOLEAN, *PBACSTAC_BOOLEAN;
#define BACSTAC_TRUE        true
#define BACSTAC_FALSE       false
#else /* __cplusplus */
typedef _Bool BACSTAC_BOOLEAN, *PBACSTAC_BOOLEAN;
#define BACSTAC_TRUE        1
#define BACSTAC_FALSE       0
#endif /* __cplusplus */

typedef char BACSTAC_CHAR, *PBACSTAC_CHAR;
typedef unsigned char BACSTAC_UCHAR, *PBACSTAC_UCHAR;
typedef BACSTAC_UI16 BACSTAC_UCS2_CHAR, *PBACSTAC_UCS2_CHAR;
typedef BACSTAC_UI32 BACSTAC_UCS4_CHAR, *PBACSTAC_UCS4_CHAR;

/* This integer types that are used to define other BACstac types */

typedef unsigned int  BACSTAC_UINT;
#define BACSTAC_UINT_MAX UINT_MAX
typedef signed int    BACSTAC_INT;
#define BACSTAC_INT_MIN INT_MIN
#define BACSTAC_INT_MAX INT_MAX
typedef unsigned long BACSTAC_ULONG;
#define BACSTAC_ULONG_MAX ULONG_MAX
typedef signed long   BACSTAC_LONG;
#define BACSTAC_LONG_MIN LONG_MIN
#define BACSTAC_LONG_MAX LONG_MAX

/* The following types are used to represent BACnet values */

typedef unsigned BACSTAC_UNSIGNED, *PBACSTAC_UNSIGNED;
#define BACSTAC_UNSIGNED_MAX UINT_MAX
typedef int      BACSTAC_INTEGER, *PBACSTAC_INTEGER;
#define BACSTAC_INTEGER_MIN INT_MIN
#define BACSTAC_INTEGER_MAX INT_MAX
#if UINT_MAX >= BACSTAC_UI32_MAX
typedef unsigned BACSTAC_ENUM, *PBACSTAC_ENUM;
#define BACSTAC_ENUM_MAX UINT_MAX
#else
typedef BACSTAC_UI32 BACSTAC_ENUM, *PBACSTAC_ENUM;
#define BACSTAC_ENUM_MAX BACSTAC_UI32_MAX
#endif
typedef float    BACSTAC_REAL, *PBACSTAC_REAL;
#define BACSTAC_REAL_C(constant) constant##f
typedef double   BACSTAC_DOUBLE, *PBACSTAC_DOUBLE;
#define BACSTAC_DOUBLE_C(constant) constant

typedef size_t BACSTAC_SIZE_T;
#ifdef SIZE_MAX
#define BACSTAC_SIZE_MAX SIZE_MAX
#else
#  if defined(_MSC_VER) || defined(__BORLANDC__)
#    ifdef _WIN64
#      define BACSTAC_SIZE_MAX (18446744073709551615UL)
#    else
#      define BACSTAC_SIZE_MAX (4294967295U)
#    endif
#  elif defined(__SIZEOF_SIZE_T__)
#    if __SIZEOF_SIZE_T__ == 8
#      define BACSTAC_SIZE_MAX (18446744073709551615UL)
#    elif __SIZEOF_SIZE_T__ == 4
#      define BACSTAC_SIZE_MAX (4294967295U)
#    else
#      error "Unsupported platform"
#    endif
#  elif defined(__i386__) || defined(__i386) || defined(_X86) || \
        defined(_M_IX86) || defined(__I86__) || defined(__arm__) || \
        defined(__thumb__) || defined(__TARGET_ARCH_ARM) || \
        defined(__TARGET_ARCH_THUMB)
#    define BACSTAC_SIZE_MAX (4294967295U)
#  elif defined(__x86_64) || defined(_M_AMD64)
#    define BACSTAC_SIZE_MAX (18446744073709551615UL)
#  else
#    error "Unsupported platform"
#  endif
#endif

/* Maximum length of a BACnet address supported by BACstac */
#define BACSTAC_MAX_MAC_ADDRESS         7

#define BACSTAC_ENUM_COMPATIBLE(name) \
	, BACSTAC_##name##_RESERVE = INT_MAX

#ifdef BACSTAC_NO_SHORT_ENUM
#define BACSTAC_OTHER_ENUM(name) \
	, BACSTAC_##name##_RESERVE = INT_MAX
#else
#define BACSTAC_OTHER_ENUM(name)
#endif

/*
 * BACnet Object Instance number
 *
 * The instance number is 22-bit unsigned integer that takes values from 0 up
 * to 4194303. No object should have an object identifier with an instance
 * number of 4194303, because this value is used to indicate that the instance
 * number is unknown or not initialized.
 */
typedef BACSTAC_UI32 BACSTAC_INST_NUMBER;
#define BACSTAC_INST_NUMBER_MIN     ((BACSTAC_INST_NUMBER) 0)
#define BACSTAC_INST_NUMBER_MAX     ((BACSTAC_INST_NUMBER) 0x3FFFFFL)
#define BACSTAC_INST_NUMBER_UNKNOWN BACSTAC_INST_NUMBER_MAX


/* number of elements in an array */
typedef BACSTAC_UINT BACSTAC_ELEMENT_COUNT;
#define BACSTAC_ELEMENT_COUNT_MAX BACSTAC_UINT_MAX

/* index of array
 * it is defined as a signed integer now, but it can be changed
 * to unsigned integer in the future
 */
typedef BACSTAC_INT  BACSTAC_ARRAY_INDEX;
#define BACSTAC_ARRAY_INDEX_MAX BACSTAC_INT_MAX

/* BACSTAC_VOID_INDEX denotes absent of index */
#define BACSTAC_VOID_INDEX  ((BACSTAC_ARRAY_INDEX) (-1))

/* IS_VALID_BACSTAC_ARRAY_INDEX(index) verifies that the given
 * index is valid. For signed definition of BACSTAC_ARRAY_INDEX,
 * it is equivalent that the value is equal or greater than zero.
 * For unsigned definition of of BACSTAC_ARRAY_INDEX, it is
 * equivalent that the value is less than BACSTAC_VOID_INDEX.
 */
#define IS_VALID_BACSTAC_ARRAY_INDEX(ind) ((ind) >= 0)

/* IS_VALID_BACSTAC_ARRAY_INDEX_OR_VOID(index) verifies that
 * the given index is valid or BACSTAC_VOID_INDEX.
 */
#define IS_VALID_BACSTAC_ARRAY_INDEX_OR_VOID(ind) ((ind)>=-1)

typedef BACSTAC_SIZE_T BACSTAC_VALUE_SIZE;
#define BACSTAC_VALUE_SIZE_MAX BACSTAC_SIZE_MAX

typedef BACSTAC_UINT BACSTAC_DEVICE_COUNT;
#define BACSTAC_DEVICE_COUNT_MAX BACSTAC_UINT_MAX
typedef BACSTAC_UINT BACSTAC_PROPERTY_COUNT;
#define BACSTAC_PROPERTY_COUNT_MAX BACSTAC_UINT_MAX
typedef BACSTAC_UINT BACSTAC_OBJECT_COUNT;
#define BACSTAC_OBJECT_COUNT_MAX BACSTAC_UINT_MAX

typedef BACSTAC_INT BACSTAC_PRIORITY_LEVEL;
#define BACSTAC_PRIORITY_LEVEL_MIN 1
#define BACSTAC_PRIORITY_LEVEL_MAX 16

/* Support of _Generic from C11 */
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#  define BACSTAC_GENERIC _Generic
#elif defined(__GNUC__) && ((__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 9)))
#  define BACSTAC_GENERIC __extension__ _Generic
#else
#  undef BACSTAC_GENERIC
#endif

/* for backward compatibility */
typedef BACSTAC_DEPRECATED BACSTAC_INTEGER BACSTAC_SIGNED, *PBACSTAC_SIGNED;
#define BACSTAC_SIGNED_MIN BACSTAC_INTEGER_MIN
#define BACSTAC_SIGNED_MAX BACSTAC_INTEGER_MAX

#ifdef BACSTAC_USE_DEPRECATED_TYPES
#if !defined(_WINDEF_) && !defined(_WINDEF_H)
typedef BACSTAC_CHAR    CHAR, *PCHAR;
typedef BACSTAC_UI8     BYTE, *PBYTE;
typedef BACSTAC_UI16    WORD, *PWORD;
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN32__)
typedef unsigned long   DWORD, *PDWORD;
#else
typedef BACSTAC_UI32    DWORD, *PDWORD;
#endif
typedef BACSTAC_INT     INT, *PINT;
typedef BACSTAC_UINT    UINT, *PUINT;
#if !defined(__CYGWIN__) || defined(__CYGWIN32__)
typedef BACSTAC_LONG    LONG, *PLONG;
typedef BACSTAC_ULONG   ULONG, *PULONG;
#else
typedef BACSTAC_SI32    LONG, *PLONG;
typedef BACSTAC_UI32    ULONG, *PULONG;
#endif

typedef BACSTAC_BYTE BOOLEAN, *PBOOLEAN;

#ifndef TRUE
#define TRUE        1
#define FALSE       0
#endif

#endif /* _WINDEF_ */
#endif /* BACSTAC_USE_DEPRECATED_TYPES */

#endif /* BACBASE_H */
