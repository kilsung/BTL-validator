/*
 * Project: BACstac (tm) examples
 *
 * Copyright (c) 2016-2018 by Cimetrics Inc. All rights reserved.
 *
 * Purpose: Define verify() and verify_ok() macros for DEBUG
 *          version.
 *
 * Author: Dmitry Potapov
 */

#ifndef verify
#ifdef NDEBUG
#define verify(x) (void)(x)
#else /* NDEBUG */
#include <assert.h>
#define verify(x) assert(x)
#endif /* NDEBUG */
#endif /* verify */

#ifndef verify_ok
#define verify_ok(x) verify((x) == BACSTAC_STATUS_OK)
#endif /* verify_ok */
