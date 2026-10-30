/*
 *  BACstac, event queue implementation header
 *
 *  Copyright (c) 2018 by Cimetrics Inc. All rights reserved.
 *
 *  Author: Dmitry Potapov
 */

#ifndef BACEVQ_H
#error "This file should never included directy"
#endif

#include <poll.h>

#define EVENT_IN POLLIN
#define EVENT_PRI POLLPRI
#define EVENT_OUT POLLOUT
#define EVENT_ERR POLLERR
#define EVENT_HUP POLLHUP
#define EVENT_INVAL POLLNVAL

struct EVQ_POLL_EVENT
{
	int index;
	EVQ_POLL_EVENT_CB callback;
};

inline static void EvqPollEventInit(EVQ_POLL_EVENT *poll, EVQ_POLL_EVENT_CB callback)
{
	poll->index = -1;
	poll->callback = callback;
}

struct EVQ_EVENT
{
	EVQ_EVENT_CB callback;
	EVQ_EVENT *next;
	OSAAtomic signal;
#ifdef DEBUG
	bool busy;
#endif
};
