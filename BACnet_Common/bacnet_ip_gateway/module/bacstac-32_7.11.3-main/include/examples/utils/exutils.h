/*
 * BACstac example utilities
 *
 * Copyright (c) 2002-2025 by Cimetrics Inc. All rights reserved.
 *
 * Purpose: Declarations of different global utilities for BACstac examples
 *
 * Author: Al Markov
 *
 */

#ifndef EXUTILS_H
#define EXUTILS_H

#include <bacbase.h>

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef _MSC_VER
/* disable C4127: conditional expression is constant */
#pragma warning(disable : 4127)
#endif

/* Define SIZE_MAX if it is not defined */
#ifndef SIZE_MAX
#  if defined(__SIZE_MAX__)
#    define SIZE_MAX __SIZE_MAX__
#  elif defined(_MSC_VER) || defined(__BORLANDC__)
#    if defined(_WIN64)
#        define BACSTAC_SIZE_MAX (18446744073709551615UL)
#    else
#        define BACSTAC_SIZE_MAX (4294967295U)
#    endif
#  elif defined(__SIZEOF_SIZE_T__)
#    if __SIZEOF_SIZE_T__ == 8
#      define BACSTAC_SIZE_MAX (18446744073709551615UL)
#    elif __SIZEOF_SIZE_T__ == 4
#      define BACSTAC_SIZE_MAX (4294967295U)
#    else
#      error "Unsupported platform"
#    endif
#  else
#    error "Unsupported platform"
#  endif
#endif

#ifdef _MSC_VER
#define strdup _strdup
#pragma warning(disable : 4127) /* conditional expression is constant */
#endif

#ifdef _MSC_VER
#define DECL_NORETURN __declspec(noreturn)
#endif

#ifdef __GNUC__
#if __GNUC__ >= 2
#if __GNUC__ > 2 || __GNUC_MINOR__ >= 7
#define DECL_NORETURN __attribute__ ((noreturn))
#endif
#endif
#endif

#ifndef DECL_NORETURN
#define DECL_NORETURN
#endif

#if defined(__clang__) && (__clang_major__ >= 6)
#  define GNUC_HAS_DIAGNOSTIC_PUSH_POP
#elif defined(__GNUC__) && (__GNUC__ >= 4)
#  if (__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
#    define GNUC_HAS_DIAGNOSTIC_PUSH_POP
#  endif
#endif

/*
 * Macro calculates count of elements on static array <arr>.
 *
 * Example:
 *	int a[] = { 1, 2, 3, 4 };
 *	SLPrintf ("Array <a> consists of %u cells", ARRAY_SIZE (a));
 *
 * NOTE:
 *	This macro defined on this place additionaly to BACstac like macro.
 *	BACstac macro is inaccessible for examples and other external
 *	applications.
 *
 */

#if defined(__GNUC__) && (__GNUC__ >= 4)
#define ARRAY_SIZE(a)     (sizeof(a)/sizeof((a)[0]) + 0 * \
	(sizeof(struct { int i:((__builtin_types_compatible_p(__typeof__(a), __typeof__((a)+0))) ? -1 : 1);})))
#else
#define ARRAY_SIZE(a)     (sizeof(a)/sizeof((a)[0]))
#endif

/* DECL_BUFFER(var, size) is intended for declaration of a buffer of
 * the specified size. It is similar to declaring 'var' as
 *    BACSTAC_BYTE var[size];
 * except that it is guaranteed that 'var' is property aligned to hold
 * any value that BACstac can return.
 *
 * DECL_BUFFER_ARRAY allows to declare array of buffers
 */
#define DECL_BUFFER_ARRAY(var, num, size) union { \
	BACSTAC_UNSIGNED v_uint; \
	BACSTAC_ULONG v_ulong; \
	BACSTAC_UI64 v_ui64; \
	void *v_ptr; \
	BACSTAC_REAL v_real; \
	BACSTAC_DOUBLE v_double; \
	BACSTAC_BYTE buffer[size]; \
} var[num]

#define DECL_BUFFER(var, size) DECL_BUFFER_ARRAY(var, 1, size)

/* REQUIRED_ALIGNED defines the maximum alignment that is sufficient
 * for all BACstac data types. It is used to define ADJUST_SIZE_UP.
 */
struct required_alignment {
   BACSTAC_BYTE unused;
   DECL_BUFFER(var, 1);
};
#define REQUIRED_ALIGNED (offsetof(struct required_alignment, var))

/*
 * ADJUST_SIZE_UP rounds the integer to the nearest integer that is
 * not less than given and satisfies the alignment requirement for
 * this platform
 */
#ifndef ADJUST_SIZE_UP
#define ADJUST_SIZE_UP(n)   (((n)+REQUIRED_ALIGNED-1) & ~(REQUIRED_ALIGNED-1))
#endif


/* Object descriptions are in dummydev.c */
struct BACSTAC_DB_DEVICE;
extern const struct BACSTAC_DB_DEVICE dummyDevice;

/*
 * BACSTAC_SET_HOOK is a wrapper to the BACstacSetHook() function, which
 * allows to avoid dangerous type cast of the pointer to the hook procedure,
 * and make sure that its type is correct for the given service.
 *
 * For example, instead of writing
 *    status = BACstacSetHook (HOOK_I_AM, (BACSTAC_HOOK_PROC)IAmHook);
 * you can write
 *    BACSTAC_SET_HOOK (status, I_AM, IAmHook);
 *
 * and the compiler will verify that the type of the IAmHook function
 * is correct. In this case, it should be BACSTAC_I_AM_HOOK_PROC.
 *
 * Note: If the compiler generates a warning, it means that the desciption
 * of your hook procedure does not match to required for this service.
 * You need to check the type of all parameters and make sure that the
 * description of the hook ptocedure has the word BACstac_hook in it.
 *
 *BACSTAC_HOOK_STATUS BACstac_hook IAmHook (
 *  BACSTAC_HTSM hTransaction,
 *  BACSTAC_ADDRESS *sourceAddress,
 *  BACSTAC_ADDRESS *destinationAddress,
 *  const BACSTAC_I_AM_INFO *pServiceInfo
 *  );
 *
 */
#define BACSTAC_SET_HOOK(status, name, fn) \
	do {	BACSTAC_##name##_HOOK_PROC fn_ = (fn); /* check type */ \
		(status) = BACstacSetHook (HOOK_##name, (BACSTAC_HOOK_PROC)fn_); \
	} while(0)

/* The following lines are workaround for irregularities in names of
 * hook types and the corresponding functions */
#define BACSTAC_GET_ALARM_SUMMARY_HOOK_PROC BACSTAC_ALARM_SUMM_HOOK_PROC
#define BACSTAC_GET_ENROLLMENT_SUMMARY_HOOK_PROC BACSTAC_ENRL_SUMM_HOOK_PROC
#define BACSTAC_SUBSCRIBE_COV_PROP_HOOK_PROC BACSTAC_SUBSCRIBE_COVP_HOOK_PROC
#define BACSTAC_CONF_COV_NOTIFICATION_HOOK_PROC BACSTAC_CONF_COV_HOOK_PROC
#define BACSTAC_UNCONF_COV_NOTIFICATION_HOOK_PROC BACSTAC_UNCONF_COV_HOOK_PROC
#define BACSTAC_READ_PROP_MULTIPLE_HOOK_PROC BACSTAC_READ_PROP_MUL_HOOK_PROC
#define BACSTAC_WRITE_PROP_MULTIPLE_HOOK_PROC BACSTAC_WRITE_PROP_MUL_HOOK_PROC
#define BACSTAC_CONF_TEXT_MESSAGE_HOOK_PROC BACSTAC_CONF_TEXT_MSG_HOOK_PROC
#define BACSTAC_UNCONF_TEXT_MESSAGE_HOOK_PROC BACSTAC_UNCONF_TEXT_MSG_HOOK_PROC
#define BACSTAC_CONF_PRIVATE_TRANSFER_HOOK_PROC BACSTAC_CONF_PRIVATE_XFER_HOOK_PROC
#define BACSTAC_UNCONF_PRIVATE_TRANSFER_HOOK_PROC BACSTAC_UNCONF_PRIVATE_XFER_HOOK_PROC
#define BACSTAC_READ_PROP_CONDITIONAL_HOOK_PROC BACSTAC_READ_PROP_CON_HOOK_PROC
#define BACSTAC_LIFE_SAFETY_OPERATION_HOOK_PROC BACSTAC_LIFE_SAFETY_OP_HOOK_PROC


/* If USE_BACAPP_MAIN is defined, it is assumed that the real main() function
 * is located somewhere else. This real main() performs all initialization and
 * then invokes BacAppMain. So InitExample() is defined as void.
 *
 * If USE_BACAPP_MAIN is not defined, then BacAppMain is main(), which is
 * invoked by the C start-up code. To perform additional initialization that
 * is common for all examples, InitExample() is invoked, which is defined in
 * exinit.c.
 *
 * NOTE: USE_BACAPP_MAIN is usually defined on small embedded platforms, such
 * as STM32Cube.
 */
#ifdef USE_BACAPP_MAIN
int BacAppMain(int argc, const char *argv[]);
#define InitExample() (void)0
#else
#define BacAppMain main
void InitExample(void);
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* EXUTILS_H */
