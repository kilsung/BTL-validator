/*
	BACstac/32, OSA port on Linux

	Copyright (c) 1998-2025 by Cimetrics Inc. All rights reserved.

	Purpose: OSA for Linux API declarations

*/


#ifndef osa_h_included
#define osa_h_included

#ifdef HAVE_BAC_AUTOCONF
#include "autoconf.h"
#endif

#if defined(__i386__) || defined(__amd64__) || defined(__x86_64__)
#ifndef _REENTRANT
#error Define _REENTRANT before any #include in your module!
#endif
#endif

#include <limits.h>			/* for ULONG_MAX               */
#include <stddef.h>			/* for NULL, ptrdiff_t, size_t */
#include <pthread.h>

#ifdef __USE_BSD
#include <sys/types.h>			/* for u_char, u_int, u_long   */
#else
#ifndef _BSDTYPES_DEFINED
typedef unsigned char  u_char;
typedef unsigned short u_short;
typedef unsigned int   u_int;
typedef unsigned long  u_long;
#define _BSDTYPES_DEFINED
#endif
#endif

#include <compiler.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OSA_STATUS

/*
 * General
 */

#define osa_inline bac_inline
#define osa_inline_def bac_inline_def

typedef enum OSAWaitStatus		/* results of awaiting something */
	{
	osaWaitOK,
	osaWaitTimeout,
	osaWaitFailure
	}
OSAWaitStatus;

/* interval time, in milleseconds */
typedef unsigned OSAInterval;
#define OSA_INFINITE		(~0u)
#define OSA_INTERVAL_MAX	INT_MAX /* it must be >= 21 day */


/*
 * OSA Atomics
 */

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L) && !defined(__STDC_NO_ATOMICS__)
#define HAVE_STDC_ATOMICS 1
#endif

#ifdef HAVE_STDC_ATOMICS
#include <osastdatomic.h>
#else /* HAVE_STDC_ATOMICS */
typedef struct OSAAtomicFlag {
	volatile int counter;
} OSAAtomicFlag;

#define OSA_ATOMIC_FLAG_INIT { 0 }

osa_inline bool OSAAtomicFlagSet(OSAAtomicFlag *v);
osa_inline void OSAAtomicFlagClear(OSAAtomicFlag *v);

typedef struct OSAAtomic {
	volatile int counter;
} OSAAtomic;

/*
 * Initializes the default-constructed atomic object with the desired value.
 * The function is not atomic: concurrent access from another thread, even
 * through an atomic operation, is a data race.
 */
osa_inline void OSAAtomicInit(OSAAtomic *v, int value);

/* Atomic operations */
osa_inline void OSAAtomicInc(OSAAtomic *v);
osa_inline bool OSAAtomicIncTest(OSAAtomic *v);
osa_inline void OSAAtomicDec(OSAAtomic *v);
osa_inline bool OSAAtomicDecTest(OSAAtomic *v);
osa_inline void OSAAtomicSet(OSAAtomic *v, int i);
osa_inline int  OSAAtomicGet(OSAAtomic *v);
osa_inline void OSAAtomicAdd(OSAAtomic *v, int i);
osa_inline void OSAAtomicSub(OSAAtomic *v, int i);
#endif /* HAVE_STDC_ATOMICS */


/*
 * Threads
 */

typedef void (*OSAThreadProc)(void*);
typedef void* OSAThreadHandle;

#define OSA_INVALID_THREAD	NULL

OSAThreadHandle OSACreateThread (OSAThreadProc start, void* parameter);
bool OSATerminateThread (OSAThreadHandle thread);
OSAWaitStatus OSAWaitForThread (OSAThreadHandle thread, OSAInterval timeout);

/* Thread ID */
typedef pthread_t OSAThreadId;
/* Obtain Thread ID of the calling thread */
#define OSAGetCurrentThreadId() pthread_self()
/* Compare two thread IDs */
#define OSAThreadEqual(t1, t2) ((bool)(pthread_equal((t1), (t2)) != 0))


/*
 * Semaphores
 */

/*
 * To use sem_t, we need sem_clockwait, which is available only in GLibc 2.30
 * or later. Otherwise we will use our own implementation using pthread_cond_t.
 */
#ifdef __GLIBC__
#if __GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 30)
#define OSA_USE_SEM_T 1
#endif
#endif

typedef unsigned int OSASemValue;
#ifdef OSA_USE_SEM_T
#include <semaphore.h>
#define OSA_SEM_VALUE_MAX SEM_VALUE_MAX
#define OSA_INLINE_SEM_FUNC osa_inline
#else /* OSA_USE_SEM_T */
#define OSA_SEM_VALUE_MAX UINT_MAX
#define OSA_INLINE_SEM_FUNC /* non-inline */
#endif /* OSA_USE_SEM_T */

typedef struct OSASemaphore
{
#ifdef OSA_USE_SEM_T
	sem_t sem;
#else /* OSA_USE_SEM_T */
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	OSASemValue value;
#endif /* OSA_USE_SEM_T */
#ifdef CONFIG_WAIT_STAT
	OSAAtomic wait_cnt;
#endif /* CONFIG_WAIT_STAT */
#ifndef NDEBUG
	unsigned signature;
#endif
} OSASemaphore;

OSA_INLINE_SEM_FUNC bool OSAInitSemaphore(OSASemaphore *semaphore, OSASemValue value);

bool OSAWaitForSemaphore(OSASemaphore *semaphore, OSAInterval timeout);

OSA_INLINE_SEM_FUNC void OSAReleaseSemaphore(OSASemaphore *semaphore);

#ifdef CONFIG_WAIT_STAT
OSA_INLINE_SEM_FUNC void OSADestroySemaphore_(OSASemaphore *sem, const char *name);
#define OSADestroySemaphore(sem) OSADestroySemaphore_((sem), #sem)
#else /* CONFIG_WAIT_STAT */
OSA_INLINE_SEM_FUNC void OSADestroySemaphore(OSASemaphore *semaphore);
#endif /* CONFIG_WAIT_STAT */

#undef OSA_INLINE_SEM_FUNC

/*
 * Recursive mutex
 */

typedef struct OSARecursiveMutex
{
	pthread_mutex_t mut;
#ifdef CONFIG_WAIT_STAT
	OSAAtomic wait_cnt;
#endif /* CONFIG_WAIT_STAT */
#ifndef NDEBUG
	unsigned signature;
#endif
} OSARecursiveMutex;


osa_inline void OSAInitRecursiveMutex_ (OSARecursiveMutex *mutex);
#define OSAInitRecursiveMutex(mutex) (OSAInitRecursiveMutex_(mutex), true)
osa_inline void OSALockRecursiveMutex (OSARecursiveMutex *mutex);
osa_inline void OSAUnlockRecursiveMutex (OSARecursiveMutex *mutex);
#ifdef CONFIG_WAIT_STAT
osa_inline void OSADestroyRecursiveMutex_(OSARecursiveMutex *mut, const char *name);
#define OSADestroyRecursiveMutex(mut) OSADestroyRecursiveMutex_((mut), #mut)
#else /* CONFIG_WAIT_STAT */
osa_inline void OSADestroyRecursiveMutex(OSARecursiveMutex *mut);
#endif /* CONFIG_WAIT_STAT */

/*
 * Fast mutex
 */
typedef struct OSAFastMutex
{
	pthread_mutex_t mut;
#ifdef CONFIG_WAIT_STAT
	OSAAtomic wait_cnt;
#endif /* CONFIG_WAIT_STAT */
#ifndef NDEBUG
	unsigned signature;
#endif
} OSAFastMutex;


osa_inline void OSAInitFastMutex_ (OSAFastMutex *mutex);
#define OSAInitFastMutex(mutex) (OSAInitFastMutex_(mutex), true)
osa_inline void OSALockFastMutex (OSAFastMutex *mutex);
osa_inline void OSAUnlockFastMutex (OSAFastMutex *mutex);
#ifdef CONFIG_WAIT_STAT
osa_inline void OSADestroyFastMutex_(OSAFastMutex *mut, const char *name);
#define OSADestroyFastMutex(mut) OSADestroyFastMutex_((mut), #mut)
#else /* CONFIG_WAIT_STAT */
osa_inline void OSADestroyFastMutex(OSAFastMutex *mut);
#endif /* CONFIG_WAIT_STAT */


/*
 * Start / shut down
 */

OSAFastMutex *OSAInit(void);
void OSAClose(void);


/*
 * Dynamic memory (process scope)
 */

decl_malloc
#if (defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3)))
__attribute__ ((alloc_size(1)))
#endif
void* OSAAlloc (size_t size);
void OSAFree (void* ptr);


OSAInterval OSAGetTickCount (void);
void OSASleep (OSAInterval ms);

/*
 * OSAChar and stuff
 */

#include <osachar.h>

#ifdef USE_OSA_CONF
/*
 * Configuration data
 */

#define MAX_CONFIG_KEY			(32u + 1u)
#define MAX_CONFIG_LEVELS		5U
#define CONFIG_SUBLEVEL_CHAR OSA_TEXT('*') /* sub-level separator (char) */
#define CONFIG_SUBLEVEL_STR  OSA_TEXT("*") /* the same as string literal */

bool OSAReadLong (const OSAChar* key, long* value, long defaultValue);
bool OSAWriteLong (const OSAChar* key, long value);

bool OSAReadString (const OSAChar* key, OSAChar* value,
		size_t* valueSize, const OSAChar* defaultValue);
bool OSAWriteString (const OSAChar* key, const OSAChar* value);

bool OSAReadMultiString (const OSAChar* key, OSAChar* value,
		size_t* valueSize, const OSAChar* defaultValue);
bool OSAWriteMultiString (const OSAChar* key, const OSAChar* value);

bool OSADeleteKey (const OSAChar* key);
bool OSARenameKey (const OSAChar* oldKey, const OSAChar* newKey);

bool OSAConfigFlush(void);
#endif

#include <osalog.h>

#include <api/bacbase.h> /* for BACSTAC_FLOAT_IEEE_FORMAT */

/* If it is not the IEEE format, OSA must provide functions to determine
 * whether it is a normal or special value and how to generate them. */
#if !BACSTAC_FLOAT_IEEE_FORMAT
typedef enum OSAFPClass
	{
	osaFPInfPos,			/* infinity positive (+INF) */
	osaFPInfNeg,			/* infinity negative (-INF) */
	osaFPNAN,			/* not a number (NAN) */
	osaFPNormal			/* all other (regular number) */
	}
OSAFPClass;

OSAFPClass OSAFPClassify (double value);

bool OSAGenerateSpecFPValue (OSAFPClass fpClass, double* value);
#endif /* BACSTAC_FLOAT_IEEE_FORMAT */


/*
 * Miscellaneous
 */

#ifdef OSA_STATUS
typedef enum OSAStatusParam
{
	osaStatus_NumAllocChunks,
	osaStatus_AllocBytes,
	osaStatus_NumThreads,
	osaStatus_LastParam = osaStatus_NumThreads
} OSAStatusParam;

int OSAGetStatus(OSAStatusParam param);
#endif

#ifdef CONFIG_MODULES
/*
 * Dynamic Load Libraries
 */
typedef struct {
	unsigned *unused;
} *OSADlHandle;

#define OSA_INVALID_DL_HANDLE ((OSADlHandle)0)
OSADlHandle OSAOpenDl(const char* filename);
void *OSAGetProcAddress(OSADlHandle handle, const char* name);
bool OSACloseDl(OSADlHandle handle);
#endif

#ifdef HAVE_INLINE_FUNC
#include "osa.inl"
#endif

#ifdef __cplusplus
}
#endif

#ifdef CONFIG_TRACE_OSA_CALL
#ifndef internal_h_included
#include "osa-trace.h"
#endif
#endif /* CONFIG_TRACE_OSA_CALL */

/* For internal use inside of linux specific files */
struct timespec;
void osaAddTimeout(struct timespec* abstime, OSAInterval timeout);

/* To create condition variables that use CLOCK_MONOTONIC */
extern pthread_condattr_t osaMonotonicCondAttr;

// Return absolute time based on the system-wide real-time clock.
// This function is provided only for backward compatibility.
void osaAbsTimeFromTimeout(struct timespec* abstime, OSAInterval timeout);

/* These variables are used by the BACstac service start-up code */
extern const char* osaMainConfig;
extern const char* osaVarConfig;
extern int osaLogFacility;

#endif /* osa_h_included */

/* eof */

