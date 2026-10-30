/*
 * Project: BACstac (tm) examples
 *
 * Copyright (c) 2005-2023 by Cimetrics Inc. All rights reserved.
 *
 * Purpose: Example of implementation of BACSTAC_IALLOC interface
 *
 * Author: Dmitry Potapov
 */

extern BACSTAC_IALLOC iMalloc_;

/* iMalloc interface for BACstac */
#define iMalloc (&iMalloc_)
