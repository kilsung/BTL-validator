/*
	BACstac/32, OSA port on Linux

	Copyright (c) 2001-2022 by Cimetrics Inc. All rights reserved.

	Purpose:	OSA for Linux API declarations

	Author:		Dmitry Potapov

*/

#ifndef OSA_TRACE_H
#define OSA_TRACE_H

#include "osa.h"

/*
OSAFastMutex *OSAInit(void);
*/
#define OSAInit() \
	dbg_OSAInit(__FILE__, __LINE__)
OSAFastMutex *dbg_OSAInit(const char* file, int line);

/*
void OSAClose(void);
*/
#define OSAClose() \
	dbg_OSAClose(__FILE__, __LINE__)
void dbg_OSAClose(const char* file, int line);

/*
OSAThreadHandle OSACreateThread (OSAThreadProc start, void* parameter);
*/
#define OSACreateThread(start, parameter) \
	dbg_OSACreateThread(__FILE__, __LINE__, start, parameter)
OSAThreadHandle dbg_OSACreateThread(const char* file, int line, OSAThreadProc start, void* parameter);

/*
bool OSATerminateThread (OSAThreadHandle thread);
*/
#define OSATerminateThread(thread) \
	dbg_OSATerminateThread(__FILE__, __LINE__, thread)
bool dbg_OSATerminateThread(const char* file, int line, OSAThreadHandle thread);

/*
OSAWaitStatus OSAWaitForThread (OSAThreadHandle thread, OSAInterval timeout);
*/
#define OSAWaitForThread(thread, timeout) \
	dbg_OSAWaitForThread(__FILE__, __LINE__, thread, timeout)
OSAWaitStatus dbg_OSAWaitForThread(const char* file, int line, OSAThreadHandle thread, OSAInterval timeout);

/*
 * bool OSAInitSemaphore(OSASemaphore *semaphore, OSASemValue value);
 */
#undef OSAInitSemaphore
#define OSAInitSemaphore(semaphore, value) \
	dbg_OSAInitSemaphore(__FILE__, __LINE__, (semaphore), (value))
bool dbg_OSAInitSemaphore(const char* file, int line,
	OSASemaphore *semaphore, OSASemValue value);

/*
 * bool OSAWaitForSemaphore(OSASemaphore *semaphore, OSAInterval timeout);
 */
#define OSAWaitForSemaphore(semaphore, timeout) \
	dbg_OSAWaitForSemaphore(__FILE__, __LINE__, (semaphore), (timeout))
bool dbg_OSAWaitForSemaphore(const char* file, int line, OSASemaphore *semaphore,
				OSAInterval timeout);

/*
 * void OSAReleaseSemaphore(OSASemaphore *semaphore);
 */
#define OSAReleaseSemaphore(semaphore) \
	dbg_OSAReleaseSemaphore(__FILE__, __LINE__, (semaphore))
void dbg_OSAReleaseSemaphore(const char* file, int line, OSASemaphore *semaphore);

/*
 * void OSADestroySemaphore(OSASemaphore *semaphore);
 */
#define OSADestroySemaphore(semaphore) \
	dbg_OSADestroySemaphore(__FILE__, __LINE__, semaphore)
void dbg_OSADestroySemaphore(const char* file, int line, OSASemaphore *semaphore);

/*
bool OSAInitFastMutex (OSAFastMutex *mutex);
*/
#undef OSAInitFastMutex
#define OSAInitFastMutex(mutex) \
	dbg_OSAInitFastMutex(__FILE__, __LINE__, mutex)
bool dbg_OSAInitFastMutex(const char* file, int line, OSAFastMutex *mutex);

/*
void OSALockFastMutex (OSAFastMutex *mutex);
*/
#define OSALockFastMutex(mutex) \
	dbg_OSALockFastMutex(__FILE__, __LINE__, mutex)
void dbg_OSALockFastMutex(const char* file, int line, OSAFastMutex *mutex);

/*
void OSAUnlockFastMutex (OSAFastMutex *mutex);
*/
#define OSAUnlockFastMutex(mutex) \
	dbg_OSAUnlockFastMutex(__FILE__, __LINE__, mutex)
void dbg_OSAUnlockFastMutex(const char* file, int line, OSAFastMutex *mutex);

/*
void OSADestroyFastMutex (OSAFastMutex *mutex);
*/
#define OSADestroyFastMutex(mutex) \
	dbg_OSADestroyFastMutex(__FILE__, __LINE__, mutex)
void dbg_OSADestroyFastMutex(const char* file, int line, OSAFastMutex *mutex);

/*
bool OSAInitRecursiveMutex (OSARecursiveMutex *mutex);
*/
#undef OSAInitRecursiveMutex
#define OSAInitRecursiveMutex(mutex) \
	dbg_OSAInitRecursiveMutex(__FILE__, __LINE__, mutex)
bool dbg_OSAInitRecursiveMutex(const char* file, int line, OSARecursiveMutex *mutex);

/*
void OSALockRecursiveMutex (OSARecursiveMutex *mutex);
*/
#define OSALockRecursiveMutex(mutex) \
	dbg_OSALockRecursiveMutex(__FILE__, __LINE__, mutex)
void dbg_OSALockRecursiveMutex(const char* file, int line, OSARecursiveMutex *mutex);

/*
void OSAUnlockRecursiveMutex (OSARecursiveMutex *mutex);
*/
#define OSAUnlockRecursiveMutex(mutex) \
	dbg_OSAUnlockRecursiveMutex(__FILE__, __LINE__, mutex)
void dbg_OSAUnlockRecursiveMutex(const char* file, int line, OSARecursiveMutex *mutex);

/*
void OSADestroyRecursiveMutex (OSARecursiveMutex *mutex);
*/
#define OSADestroyRecursiveMutex(mutex) \
	dbg_OSADestroyRecursiveMutex(__FILE__, __LINE__, mutex)
void dbg_OSADestroyRecursiveMutex(const char* file, int line, OSARecursiveMutex *mutex);

/*
void* OSAAlloc (size_t size);
*/
#define OSAAlloc(size) \
	dbg_OSAAlloc(__FILE__, __LINE__, size)
void* dbg_OSAAlloc(const char* file, int line, size_t size);

/*
 * void OSAFree (void* ptr);
 */
#define OSAFree(ptr) \
	dbg_OSAFree(__FILE__, __LINE__, ptr)
void dbg_OSAFree(const char* file, int line, void* ptr);

/*
OSAInterval OSAGetTickCount (void);
*/
#define OSAGetTickCount() \
	dbg_OSAGetTickCount(__FILE__, __LINE__)
OSAInterval dbg_OSAGetTickCount(const char* file, int line);

/*
void OSASleep (OSAInterval ms);
*/
#define OSASleep(ms) \
	dbg_OSASleep(__FILE__, __LINE__, ms)
void dbg_OSASleep(const char* file, int line, OSAInterval ms);

/*
bool OSAReadLong (const OSAChar* key, long* value, long defaultValue);
*/
#define OSAReadLong(key, value, defaultValue) \
	dbg_OSAReadLong(__FILE__, __LINE__, key, value, defaultValue)
bool dbg_OSAReadLong(const char* file, int line, const OSAChar* key, long* value, long defaultValue);

/*
bool OSAWriteLong (const OSAChar* key, long value);
*/
#define OSAWriteLong(key, value) \
	dbg_OSAWriteLong(__FILE__, __LINE__, key, value)
bool dbg_OSAWriteLong(const char* file, int line, const OSAChar* key, long value);

/*
bool OSAReadString (const OSAChar* key, OSAChar* value,
		size_t* valueSize, const OSAChar* defaultValue);
*/
#define OSAReadString(key, value, valueSize, defaultValue) \
	dbg_OSAReadString(__FILE__, __LINE__, key, value, valueSize, defaultValue)
bool dbg_OSAReadString(const char* file, int line, const OSAChar* key, OSAChar* value,
		size_t* valueSize, const OSAChar* defaultValue);

/*
bool OSAWriteString (const OSAChar* key, const OSAChar* value);
*/
#define OSAWriteString(key, value) \
	dbg_OSAWriteString(__FILE__, __LINE__, key, value)
bool dbg_OSAWriteString(const char* file, int line, const OSAChar* key, const OSAChar* value);

/*
bool OSAReadMultiString (const OSAChar* key, OSAChar* value,
		size_t* valueSize, const OSAChar* defaultValue);
*/
#define OSAReadMultiString(key, value, valueSize, defaultValue) \
	dbg_OSAReadMultiString(__FILE__, __LINE__, key, value, valueSize, defaultValue)
bool dbg_OSAReadMultiString(const char* file, int line, const OSAChar* key, OSAChar* value,
		size_t* valueSize, const OSAChar* defaultValue);

/*
bool OSAWriteMultiString (const OSAChar* key, const OSAChar* value);
*/
#define OSAWriteMultiString(key, value) \
	dbg_OSAWriteMultiString(__FILE__, __LINE__, key, value)
bool dbg_OSAWriteMultiString(const char* file, int line, const OSAChar* key, const OSAChar* value);

/*
bool OSADeleteKey (const OSAChar* key);
*/
#define OSADeleteKey(key) \
	dbg_OSADeleteKey(__FILE__, __LINE__, key)
bool dbg_OSADeleteKey(const char* file, int line, const OSAChar* key);

/*
bool OSARenameKey (const OSAChar* oldKey, const OSAChar* newKey);
*/
#define OSARenameKey(oldKey, newKey) \
	dbg_OSARenameKey(__FILE__, __LINE__, oldKey, newKey)
bool dbg_OSARenameKey(const char* file, int line, const OSAChar* oldKey, const OSAChar* newKey);

#if !BACSTAC_FLOAT_IEEE_FORMAT
/*
OSAFPClass OSAFPClassify (double value);
*/
#define OSAFPClassify(value) \
	dbg_OSAFPClassify(__FILE__, __LINE__, value)
OSAFPClass dbg_OSAFPClassify(const char* file, int line, double value);

/*
bool OSAGenerateSpecFPValue (OSAFPClass fpClass, double* value);
*/
#define OSAGenerateSpecFPValue(fpClass, value) \
	dbg_OSAGenerateSpecFPValue(__FILE__, __LINE__, fpClass, value)
bool dbg_OSAGenerateSpecFPValue(const char* file, int line, OSAFPClass fpClass, double* value);
#endif /* BACSTAC_FLOAT_IEEE_FORMAT */

#endif /* OSA_TRACE_H */
