//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef PANORAMA_DELAYLOADLIST_H
#define PANORAMA_DELAYLOADLIST_H

#ifdef _WIN32
#pragma once
#endif

#include "panel2d.h"

namespace panorama
{

DECLARE_PANEL_EVENT3( ScrollToDelayLoadListItem, int, ScrollBehavior_t, bool );

//-----------------------------------------------------------------------------
// Purpose: A list of fixed size items where each item is initialized by a
// callback/lambda function.  This list can be an extremely large size, because
// it doesn't ever keep all of the panels in memory. Instead, it just keeps the
// ones that are visible.
//-----------------------------------------------------------------------------
class CDelayLoadList : public CPanel2D
{
	DECLARE_PANEL2D( CDelayLoadList, CPanel2D );

public:
	CDelayLoadList( CPanel2D *parent, const char * pchPanelID );
	virtual ~CDelayLoadList();

	// Set the number of items in the list and reload them
	void UpdateListItems( int nItemCount );

	// Reloads the existing list items without changing the count
	void ReloadListItems();

	// Set the dimensions of each item in the list.
	void SetListItemSize( const CUILength &lenWidth, const CUILength &lenHeight );

	// Set the spacer gap period and size
	void SetSpacerPeriodAndSize( int nSpacerPeriod, const CUILength & flSpacerSize );
	
	// Set a function to call when we need to load an item panel
	void SetLoadListItemFunction( std::function< CPanel2D * ( CPanel2D *pItemParent, int nItemIndex, CPanel2D *pReuseItemPanel ) > fnLoadItem );

	// Get the panel for the list item at the given index. Note that if the panel is
	// currently unloaded due to being out of view, then it will return nullptr.
	CPanel2D *GetListItemAtIndex( int nItemIndex );

	// Get the number of list items
	int GetListItemCount() const { return m_nItemCount; }

	// Get the index that was used to initialize the given item panel. This is the same as the nItemIndex that is passed into the load function
	int GetIndexForListItem( CPanel2D *pItemPanel ) const;

	// Set whether or not the list should attempt to reuse item panels. Default value is true.
	void SetReuseListItemPanels( bool bReuseItemPanels );

	// Given a list item, return the list containing it. This isn't as simple as just GetParent,
	// because the list item might be in the reuse cache.
	static CDelayLoadList *GetListFromListItem( CPanel2D *pItemPanel );

	// Scrolls the item at the given index into view
	void ScrollListItemIntoView( int nItemIndex, ScrollBehavior_t behavior = SCROLL_BEHAVIOR_DEFAULT, bool bImmediateScroll = false );

	// Get the raw position/size information about list items. Note that this will
	// return false if the list has not yet been laid out.
	bool BGetListItemBounds( int nItemIndex, float &flItemX, float &flItemY, float &flItemWidth, float &flItemHeight );

	// Gets a range of indices that spans the given rectangular area. Note that the index range might still contain some items
	// that are not actually in the area if the flow children is right-wrap or down-wrap and the area doesn't span the full width/height.
	void GetIndexRangeForArea( const PanoramaRect_t &rectArea, int &nStartIndex, int &nIndexCount );

	// Internal use only.
	typedef uint32 ListLayoutID;

protected:
	// CPanel2D overrides
	virtual void OnContentSizeTraverse( float *pflContentWidth, float *pflContentHeight, float flMaxWidth, float flMaxHeight, bool bFinalDimensions ) OVERRIDE;
	virtual void OnLayoutTraverse( float flFinalWidth, float flFinalHeight ) OVERRIDE;
	virtual bool BSetProperties( const CUtlVector< ParsedPanelProperty_t > &vecProperties ) OVERRIDE;
	virtual void PaintArea( const PanoramaRect_t &rectPaintArea ) OVERRIDE;
	virtual void StoppedPainting() OVERRIDE;

private:
	static void CalculateItemPosition( int nItemIndex, float flItemWidth, float flItemHeight, float flTotalWidth, float flTotalHeight, EFlowDirection eFlowDirection, int nSpacerPeriod, float flSpacerSize, float &flItemX, float &flItemY );
	static int ComparePanelByItemIndex( const IUIPanelClient *pPanel1, const IUIPanelClient *pPanel2 );
	void GetItemPixelSize( float flContainerWidth, float flContainerHeight, float &flItemPixelWidth, float &flItemPixelHeight );
	float GetSpacerPixelSize( float flContainerWidth, float flContainerHeight );

	void CalculateAreaIndexRange( const PanoramaRect_t &rectArea, float flContainerWidth, float flContainerHeight, int &nStartIndex, int &nIndexCount );
	void SavePanelForReuse( CPanel2D *pItemPanel );
	void ClearLoadedListItems( bool bSaveForReuse );

	bool EventLoadVisiblePanels( ListLayoutID unLayoutID, int nVisibleStartIndex, int nVisibleCount );
	bool EventReadyForDisplay( const CPanelPtr< IUIPanel > &panelPtr );
	bool EventUnreadyForDisplay( const CPanelPtr< IUIPanel > &panelPtr );
	bool EventScrollToListItem( const CPanelPtr< IUIPanel > &panelPtr, int nItemIndex, ScrollBehavior_t behavior, bool bImmediateScroll );

	CPanel2D *m_pReusePanelCache;

	int m_nItemCount;
	CUILength m_lenItemWidth;
	CUILength m_lenItemHeight;
	int m_nSpacerPeriod;
	CUILength m_lenSpacerSize;

	bool m_bReuseItemPanels;
	std::function< CPanel2D * ( CPanel2D *pItemParent, int nItemIndex, CPanel2D *pReuseItemPanel ) > m_fnLoadItem;

	int m_nVisibleStartIndex;
	int m_nVisibleCount;

	IUIEvent *m_pEventAfterLayout;

	ListLayoutID m_unLayoutID;
};

} // namespace panorama

#endif // PANORAMA_DELAYLOADLIST_H
