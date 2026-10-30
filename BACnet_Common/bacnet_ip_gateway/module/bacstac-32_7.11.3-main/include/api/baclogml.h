/*
 * BACstac(TM) module baclogml.h
 *
 * Copyright (c) 2024 by Cimetrics Inc. All rights reserved.
 *
 * Purpose: Define Log message priorities used by BACstac.
 */

#ifndef BACLOGML_H
#define BACLOGML_H 1

/* Based on log message levels defined in <syslog.h> */
typedef unsigned char BACSTAC_LOG_MSG_LEVEL;

#define BLML_CRITICAL 2  /* critical conditions */
#define BLML_ERROR    3  /* error conditions */
#define BLML_WARNING  4  /* warning conditions */
#define BLML_NOTICE   5  /* normal but significant condition */
#define BLML_INFO     6  /* informational */
#define BLML_DEBUG    7  /* debug-level messages */

#endif /* BACLOGML_H */
