/* BACstac (TM) Examples:
 * StrToBACstacAddress() declaration
 *
 * Copyright (c) 2005-2023 by Cimetrics Inc.  All rights reserved.
 *
 * Author: Al Markov
 *
 */

#ifndef strtoaddr_h
#define strtoaddr_h

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/*
 * Function converts <dNet> and <addrBuf> to BACstac address and writes it
 * to <addr>. <addrBuf> must contain char string with hex digits without
 * prefix '0x' (for example: 'C0A80113BAC9'). If <dNet> is 0 then <addrBuf>
 * is local, else is remote.
 *
 * Return TRUE on success. Else FALSE.
 */

	BACSTAC_BOOLEAN StrToBACstacAddress (
		BACSTAC_ADDRESS *addr,	/* out */
		BACSTAC_INTEGER dNet,	/* in */
		const char *addrBuf	/* in */
	);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* strtoaddr_h */

/* eof */
