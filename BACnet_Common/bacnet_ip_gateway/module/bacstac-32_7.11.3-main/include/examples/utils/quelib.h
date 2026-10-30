/* BACstac (TM) Example Server Application
 * all operations with Abstract Queue
 *
 * Copyright (c) 1998-2018 by Cimetrics Inc.  All rights reserved.
 * Author: Vlad Kaplan
 */

#ifndef quelib_h
#define quelib_h

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

	typedef struct tagITEM
		{
			struct tagITEM *prev;
			struct tagITEM *next;
			void *pElement;
		} ITEM;

	typedef struct tagHEADER
		{
			ITEM *firstItem;
			ITEM *lastItem;
			BACSTAC_UINT sizeElem;
			BACSTAC_UINT numberOfItems;
		} HEADER;

	void InitQueue(HEADER *queue, BACSTAC_UINT itemSize);
	void CloseQueue(HEADER *queue);

/* add pElem to queue, do not copy or alter Elem in any way       */
/* pointer to the Item, holding Elem is returned, NULL on failure */
	ITEM *AddToQueue (
		HEADER * pHeader,
		void *pElem
	);

/* add copy of Elem to Queue                                      */
/* copies the Elem to a new location and adds copy to queue       */
	BACSTAC_BOOLEAN AddLastElem (
		HEADER * pHeader,
		const void *pElem
	);

/* removes 1st Elem from queue and returns pointer to it          */
	void *RemoveFirstElem (
		HEADER * pHeader
	);

/* removes an Item from queue and returns pointer to the Element  */
	void *RemoveElem (
		HEADER * pHeader,
		ITEM * pRemItem
	);

/* removes an Item from queue and deletes the associated Element  */
	ITEM *DeleteItem (
		HEADER * pHeader,
		ITEM * pDelItem
	);	/* r.v. = pPrevItem */

	ITEM *ReadFirstItem (
		HEADER * pHeader
	);
	ITEM *ReadNextItem (
		HEADER * pHeader,
		ITEM * pPrevItem
	);

	BACSTAC_UINT NumberOfItems (
		HEADER * pHeader
	);

	void *GetElementFromItem (
		ITEM * pItem
	);

/* indicates if current Item is last in the queque */
	BACSTAC_BOOLEAN ItemIsLast (
		ITEM * pCurItem
	);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* quelib_h */
