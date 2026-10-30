/*
 * BACstac(TM) BACstac module TIMERQ.H
 *
 * Copyright (c) 2014-2023 by Cimetrics Inc. All rights reserved.
 *
 * Purpose: Interface of the timer queue.
 *
 * Author: Dmitry Potapov
 *
 * The following API defines a timer queue that can be used to schedule
 * many timer events that are processed in one thread. Typically, an
 * application will have just one timer queue to process all timer events.
 * However, many timer queues may be created if timer events are processed
 * independently in different threads.
 *
 * The API assumes that starting and stopping of all timers done in the
 * thread that process the timer queue. Therefore, no synchronization is
 * required. To use this API from different threads, the application has
 * to use a mutex or another synchronization primitive to exclude concurrent
 * access to the timer queue. Timer functions that initialize, start, stop,
 * or restart a timer require access to the timer queue, and therefore they
 * require synchronization.
 *
 * TIMER_CLASS defines a group of timers that share the same timer queue
 * and timer callback.
 *
 * A typical use of the API (assuming one thread) includes following steps:
 *  1. Initialize the timer queue using TimerQueueInit.
 *  2. Initialize one or more timer types with the reference to the timer
 *     queue and the timer callback.
 *  3. Initialize as many timers as necessary using TimerInit.
 *  4. Process external events. During processing, some old timers can be
 *     stopped (using TimerStop) and others can be start using TimerStart
 *     or TimerStartWithTimeout. To restart a running timer, TimerRestart
 *     can be used.
 *  5. If the application does not have anything else to do, it invokes
 *     TimerQueueProcess, which processes expired timers and returns the
 *     timeout of next timer event.
 *  6. The thread is suspended until an external event is received or the
 *     timeout for the timer is expired.
 *  6. Unless termination event is received, repeat with step 4.
 *  8. Stop all running timers using TimerStop.
 *  9. Destroy all initialized timers using TimerDestroy.
 * 10. Destroy the timer queue using TimerQueueDestroy.
 */

#ifndef EXAMPLES_TIMERS
#define EXAMPLES_TIMERS
#include <oslib.h>

/* Date types */
struct tagTIMER_CLASS;
typedef struct tagTIMER_CLASS TIMER_CLASS;

struct tagTIMER;
typedef struct tagTIMER TIMER;

typedef void (*TIMER_CALLBACK)(TIMER*);

struct tagTIMER_QUEUE;
typedef struct tagTIMER_QUEUE TIMER_QUEUE;

/*
 * The Release and Debug versions have a different definitions of TIMER_QUEUE.
 * To avoid memory corruption when the timer library is built as Debug while
 * the application uses it as Release (i.e. without Debug specific fields),
 * TimerQueueInit is redefined here, so the linker will catch this problem.
 */
#ifndef NDEBUG
#define TimerQueueInit TimerQueueInit_Dbg
#endif


/*
 * Any timer class should be initialized before it can be used.
 *
 * @timerQueue is a reference to the timer queue to which timers of this
 *             class will be attached.
 * @timerCallback is the callback invoked for timers of this class when
 *             a timer has been expired.
 *
 * NOTE: The initialization should be performed only once.
 */
void TimerClassInit(TIMER_CLASS *timerClass, TIMER_QUEUE *timerQueue,
	TIMER_CALLBACK timerCallback);

/*
 * TIMER_CLASS_INIT can be used to initialize TIMER_CLASS during declaration.
 *
 * For example, the following statement declares myTimerClass and initialize
 * it with myTimerQueue and myTimerCb:
 *
 * const TIMER_CLASS myTimerClass = TIMER_CLASS_INIT(myTimerQueue, myTimerCb);
 *
 * NOTE: myTimerClass is declared as 'const' because it should never changed.
 */
#define TIMER_CLASS_INIT(queue, callback) { (queue), (callback) }

/*
 * Any timer should be successfully initialized before it can be used.
 *
 * The function returns true if the timer has been successfully initialized.
 * The function can fail if it cannot allocate memory.
 *
 * NOTE: When the timer is no longer needed, TimerDestroy should be used to
 * free resources used by the timer. An attempt to use TimerInit on a timer
 * that was already initialized can lead to unpredictable results.
 */
bool TimerInit(TIMER *timer, const TIMER_CLASS *timerClass);

/*
 * To free internal resources that are used by timer, the destroy function
 * should be invoked. An attempt to destroy a running timer can lead to
 * unpredictable results (such as memory corruption and crash).
 */
void TimerDestroy(TIMER *timer);

/*
 * Returns true if the given timer is attached to a timer queue.
 */
bool TimerIsRunning(const TIMER *timer);

/*
 * Returns true if the given timer is first in a timer queue.
 * IMPORTANT: The function can be used only for a running timer.
 */
bool TimerIsFirst(const TIMER *timer);

/*
 * Gets the timer expiration time as determined by OSLGetTickCount().
 *
 * NOTE: The expiration time is measured by OSLGetTickCount(), which
 * returns the number milliseconds since some implementation specific
 * time point (such as system start). If OSLInterval is 32-bit integer
 * then this value can wrap around zero approximately every 49.7 days.
 */
OSLInterval TimerGetExpiry(const TIMER *timer);

/*
 * Gets the timer expiration relatively to the current time (as returned
 * by OSLGetTickCount). If the current timer has already expired then
 * zero is returned.
 */
OSLInterval TimerGetTimeout(const TIMER *timer);

/*
 * Starts the timer with the given specified expiration timer.
 *
 * NOTE: The function can be used only for a non-running timer that
 * is properly initialized and the expiration time is within one forth
 * of the maximum value of OSLInterval from the current timer. Any
 * attempt to use this function with a timer that does not satisfy
 * the above condition can lead to unpredictable behavior (such as
 * memory corruption and crash).
 */
void TimerStart(TIMER *timer, OSLInterval expiry);

/*
 * Starts the timer that expires after the specified timeout from the
 * current time.
 *
 * The function calculates the expiration time as OSLGetTickCount() plus
 * the timeout parameter and then starts the timer.
 *
 * NOTE: The function can be used only for a non-running timer that
 * is properly initialized and the timeout parameter should not exceed
 * one forth of the maximum value of OSLInterval from the current timer.
 * Any attempt to use this function with a timer that does not satisfy
 * the above condition can lead to unpredictable behavior (such as memory
 * corruption and crash).
 */
void TimerStartWithTimeout(TIMER *timer, OSLInterval timeout);

/*
 * Starts the timer that expires after the specified interval after
 * the current expiration time (as returned TimerGetExpiry()).
 *
 * The function calculates the expiration time as TimerGetExpiry() plus
 * the interval parameter and then starts the timer.
 *
 * This function is mostly useful to create a periodic timer. Every time
 * when the timer callback is invoked, this function starts a new period.
 *
 * NOTE: The function can be used only for a non-running timer that
 * is properly initialized and the interval parameter should not exceed
 * one forth of the maximum value of OSLInterval from the current timer.
 * Any attempt to use this function with a timer that does not satisfy
 * the above condition can lead to unpredictable behavior (such as memory
 * corruption and crash).
 */
void TimerStartNextInterval(TIMER *timer, OSLInterval interval);

/*
 * Restart the timer with the given specified expiration timer.
 *
 * This function is equivalent of stopping the timer and then starting
 * it again with the given expiration time. This function does not need
 * to allocate memory, so it never fails.
 *
 * NOTE: The function can be used only for a running timer.
 */
void TimerRestart(TIMER *timer, OSLInterval expiry);

/*
 * Stops the timer.
 *
 * This function detaches the timer from the timer queue. The timer
 * queue parameter should be same as was used in TimerStart. If the
 * timer queue parameter is different then behavior of this function
 * is unpredictable (such as memory corruption and crash).
 *
 * IMPORTANT: The function can be used only for a running timer. If it
 * is unknown whether the timer is running or not then TimerIsRunning
 * should be used. An attempt to stop a non-running timer can lead to
 * unpredictable behavior.
 */
void TimerStop(TIMER *timer);

/*
 * Initializes a timer queue.
 *
 * This function should be invoked before any other function that uses
 * the timer queue.
 */
void TimerQueueInit(TIMER_QUEUE *timerQueue);

/*
 * Deinitializes a previously initialized timer queue.
 *
 * This function frees resources allocated by the timer queue.
 */
void TimerQueueDestroy(TIMER_QUEUE *timerQueue);

/*
 * Processes the timer queue and invokes callbacks for all timers that
 * have been expired after that it returns the timeout to the next
 * expiration time.
 */
OSLInterval TimerQueueProcess(TIMER_QUEUE *timerQueue);


/*
 * DEFINITIONS OF INTERNAL STRUCTURES
 *
 * IMPORTANT: All fields of TIMER and TIMER_QUEUE should be treated as
 * 'private', so they may be accessed only through the API defined above.
 */
typedef unsigned TIMER_QUEUE_SIZE;
#define TIMER_QUEUE_SIZE_MAX UINT_MAX

struct tagTIMER_QUEUE
{
	TIMER_QUEUE_SIZE size; /* the allocated size of the heap */
	TIMER_QUEUE_SIZE count; /* the total number of timers */
	TIMER_QUEUE_SIZE active; /* the number of running timers */
	TIMER **heap; /* timer binary heap */
#ifndef NDEBUG
	/*
	 * These variables are not real locks, but they are used to catch
	 * incorrect use of the Timer Queue. The variables are declared as
	 * 'volatile' to prevent the compiler from re-ordering their use.
	 */
	volatile bool tryLock;
	volatile bool lock;
#endif
};

struct tagTIMER_CLASS
{
	TIMER_QUEUE *queue; /* timer queue */
	TIMER_CALLBACK callback; /* timer callback */
};

struct tagTIMER
{
	const TIMER_CLASS *cls; /* timer class */
	OSLInterval expiry; /* timer expiration time */
	TIMER_QUEUE_SIZE position; /* internal position */
};

#endif /* EXAMPLES_TIMERS */
