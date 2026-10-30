/*
 *  BACstac, event queue
 *
 *  Copyright (c) 2009-2018 by Cimetrics Inc. All rights reserved.
 *
 *  Purpose: Interface to the main event queue
 *
 *  Author: Dmitry Potapov
 */

#ifndef BACEVQ_H
#define BACEVQ_H

#include <status.h> /* CORE_STATUS */
#include <osa.h> /* OSAInterval */

/*
 * There is only one event queue in BACstac, so the Event Queue structure is an
 * implementation details. This event queue contains completion notifications
 * for different system operations (such as asynchronous reading a UDP socket).
 *
 * Unless otherwise is specified explicitly, all functions below should be
 * invoked only from the BACstac main thread.
 */

/*
 * Initialize the event queue.
 */
CORE_STATUS EvqInit(void);

/*
 * Destroy the event queue.
 */
void EvqDestroy(void);

/*
 * Process the event queue.
 *
 * Parameters:
 *   ms - time in milliseconds during which the processing takes place
 *   after which the function must return.
 *
 * Returned value:
 *   'true' if one more event was processed, 'false' if timeout expired.
 */
bool EvqProcess(OSAInterval ms);


/*
 * Each asynchronous operation that can be associated with the event queue
 * has its own data structure. Fields of each structure are implementation
 * specific. Usually, this structure contains the callback that should be
 * invoked by EvqProcess, and some system specific fields.
 */


/*
 * EVQ_EVENT - Event-queue event.
 *
 * The primary purpose of this data structure is to notify the main BACstac
 * thread about completion of some operation.
 */
struct EVQ_EVENT;
typedef struct EVQ_EVENT EVQ_EVENT;

/* EVQ_EVENT callback */
typedef void (*EVQ_EVENT_CB)(EVQ_EVENT*);

/* EVQ_EVENT methods */

/*
 * Initialize a new event-queue event and associates it with the main BACstac
 * event queue.
 */
CORE_STATUS EvqEventInit(EVQ_EVENT* self, EVQ_EVENT_CB callback);

/*
 * Destroy an event-queue event.
 *
 * NOTE: EvqEventDestroy must not be invoked from the callback for that
 * particular event. However, any event can be destroyed from a callback
 * for another event.
 */
void EvqEventDestroy(EVQ_EVENT *self);

/*
 * Signals the event.
 *
 * This operation may be invoked from any thread. When an event is signalled,
 * the EvqProcess function will invoke the callback associated with this event.
 * If this event has been signalled before but the event has not processed yet,
 * then this operation has no effect.
 */
void EvqEventSignal(EVQ_EVENT *self);


/*
 * EVQ_POLL_EVENT - Poll events
 *
 * The 'events' parameter in the callback and EvqPollEventSetMod can be have the following bits:
 *
 * EVENT_IN   - There is data to read.
 *
 * EVENT_PRI  - There is urgent data to read (e.g., out-of-band data on TCP socket).
 *
 * EVENT_OUT  - Writing is now possible.
 *
 * EVENT_ERR  - Error condition (only returned in revents; ignored in events).
 *
 * EVENT_HUP  - Hang up (only returned in the callback; ignored in EvqPollEventSetMod).
 *
 * EVENT_NVAL - Invalid request: fd not open (only returned in the callback; ignored in EvqPollEventSetMod).
 *
 * NOTE: This interface is Linux specific, so it is only used in Linux-specific code.
 */

struct EVQ_POLL_EVENT;
typedef struct EVQ_POLL_EVENT EVQ_POLL_EVENT;

typedef void (*EVQ_POLL_EVENT_CB)(EVQ_POLL_EVENT* /* cb */, int /* events */);

/* EVQ_POLL_EVENT methods */

inline static void EvqPollEventInit(EVQ_POLL_EVENT *poll, EVQ_POLL_EVENT_CB callback);

#ifdef DEBUG
void EvqPollEventDestroy(EVQ_POLL_EVENT *poll);
#else
#define EvqPollEventDestroy(poll) (void)(poll)
#endif

CORE_STATUS EvqPollEventAdd(EVQ_POLL_EVENT *poll, int fd, int events);
void EvqPollEventSetMod(EVQ_POLL_EVENT *poll, int events);
void EvqPollEventDel(EVQ_POLL_EVENT *poll);


#include "bacevqimpl.h"

#endif /* BACEVQ_H */
