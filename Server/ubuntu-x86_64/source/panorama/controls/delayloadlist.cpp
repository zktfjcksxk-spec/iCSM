//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx_client.h"
#include "panorama/controls/delayloadlist.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

/* ------------------------------------------------------------------------- */

using namespace panorama;

REGISTER_PANEL2D_FACTORY( CDelayLoadList, DelayLoadList );

DECLARE_PANORAMA_EVENT3( DelayLoadListLoadVisiblePanels, CDelayLoadList::ListLayoutID, int, int );

DEFINE_PANORAMA_EVENT( DelayLoadListLoadVisiblePanels );
DEFINE_PANORAMA_EVENT( ScrollToDelayLoadListItem );

static const char k_szItemIndexAttribute[] = "item_index";

/* ------------------------------------------------------------------------- */

CDelayLoadList::CDelayLoadList( CPanel2D *pParent, const char *pchPanelID )
	: CPanel2D( pParent, pchPanelID )
	, m_nItemCount( 0 )
	, m_nSpacerPeriod( 0 )
	, m_bReuseItemPanels( true )
	, m_nVisibleStartIndex( 0 )
	, m_nVisibleCount( 0 )
	, m_unLayoutID( 0 )
	, m_pEventAfterLayout( nullptr )
{
	UIPanel()->SetNeedsPaintArea( true );

	m_pReusePanelCache = new CPanel2D( this, nullptr, ePanelFlags_DontAddAsChild );
	m_pReusePanelCache->SetVisible( false );

	RegisterForReadyEvents( true );

	if ( !UIEngine()->BHaveEventHandlersRegisteredForType( CDelayLoadList::GetPanelSymbol() ) )
	{
		RegisterEventHandlerOnPanelType( ReadyForDisplay(), &CDelayLoadList::EventReadyForDisplay );
		RegisterEventHandlerOnPanelType( UnreadyForDisplay(), &CDelayLoadList::EventUnreadyForDisplay );
		RegisterEventHandlerOnPanelType( DelayLoadListLoadVisiblePanels(), &CDelayLoadList::EventLoadVisiblePanels );
		RegisterEventHandlerOnPanelType( ScrollToDelayLoadListItem(), &CDelayLoadList::EventScrollToListItem );
	}

#if defined( SOURCE2_PANORAMA )
	SetAttribute( "AllowOversized", static_cast< int >( 1 ) );
#endif
}

CDelayLoadList::~CDelayLoadList()
{
	delete m_pEventAfterLayout;
}

void CDelayLoadList::UpdateListItems( int nItemCount )
{
	m_nItemCount = nItemCount;

	ClearLoadedListItems( true );
}

void CDelayLoadList::ReloadListItems()
{
	ClearLoadedListItems( true );
}

void CDelayLoadList::SetListItemSize( const CUILength &lenWidth, const CUILength &lenHeight )
{
	if ( m_lenItemWidth == lenWidth && m_lenItemHeight == lenHeight )
		return;

	m_lenItemWidth = lenWidth;
	m_lenItemHeight = lenHeight;

	InvalidateSizeAndPosition();
}

void CDelayLoadList::SetSpacerPeriodAndSize( int nSpacerPeriod, const CUILength& lenSpacerSize )
{
	if ( m_nSpacerPeriod == nSpacerPeriod && m_lenSpacerSize == lenSpacerSize )
		return;
	
	m_nSpacerPeriod = Max( 0, nSpacerPeriod );
	m_lenSpacerSize = lenSpacerSize;

	InvalidateSizeAndPosition();
}


void CDelayLoadList::SetReuseListItemPanels( bool bReuseItemPanels )
{
	m_bReuseItemPanels = bReuseItemPanels;

	if ( !m_bReuseItemPanels )
	{
		m_pReusePanelCache->RemoveAndDeleteChildren();
	}

	// don't need to reload - this will just affect future loads
}

void CDelayLoadList::SetLoadListItemFunction( std::function< CPanel2D *( CPanel2D *pContainer, int nItemIndex, CPanel2D *pReuseItemPanel ) > fnLoadItem )
{
	m_fnLoadItem = fnLoadItem;

	ClearLoadedListItems( false );
}

CPanel2D *CDelayLoadList::GetListItemAtIndex( int nItemIndex )
{
	Assert( GetChildCount() == m_nVisibleCount );

	int iChild = nItemIndex - m_nVisibleStartIndex;
	CPanel2D *pChild = GetChild( iChild );
	Assert( !pChild || pChild->GetAttribute( k_szItemIndexAttribute, -1 ) == nItemIndex );
	return pChild;
}

int CDelayLoadList::GetIndexForListItem( CPanel2D *pItemPanel ) const
{
	if ( !pItemPanel )
		return -1;

	return pItemPanel->GetAttribute( k_szItemIndexAttribute, -1 );
}

/*static*/ CDelayLoadList *CDelayLoadList::GetListFromListItem( CPanel2D *pItemPanel )
{
	if ( !pItemPanel )
		return nullptr;

	CPanel2D *pParent = pItemPanel->GetParent();
	if ( !pParent )
		return nullptr;

	CDelayLoadList *pList = dynamic_cast< CDelayLoadList * >( pParent );
	if ( pList )
		return pList;

	CPanel2D *pGrandParent = pParent->GetParent();
	if ( !pGrandParent )
		return nullptr;

	pList = dynamic_cast< CDelayLoadList * >( pGrandParent );
	if ( !pList )
		return nullptr;

	if ( pList->m_pReusePanelCache != pParent )
		return nullptr;

	return pList;
}

void CDelayLoadList::ClearLoadedListItems( bool bSaveForReuse )
{
	int nPreviousVisibleCount = m_nVisibleCount;

	m_nVisibleStartIndex = 0;
	m_nVisibleCount = 0;
	m_unLayoutID++;

	if ( bSaveForReuse )
	{
		// Save away some panels to reuse on the next load
		int nSavePanelCount = Min( GetChildCount(), nPreviousVisibleCount );
		for ( int i = nSavePanelCount - 1; i >= 0; --i )
		{
			SavePanelForReuse( GetChild( i ) );
		}
	}

	// Remove and delete any remaining children
	RemoveAndDeleteChildren();

	SetRepaint( k_EPanelRepaintFull );
}

void CDelayLoadList::ScrollListItemIntoView( int nItemIndex, ScrollBehavior_t behavior, bool bImmediateScroll )
{
	CPanel2D *pItemPanel = GetListItemAtIndex( nItemIndex );
	if ( pItemPanel )
	{
		pItemPanel->ScrollParentToMakePanelFit( behavior, bImmediateScroll );
		return;
	}

	// If we haven't been laid out, try again once we have been
	if ( !UIPanel()->BHasBeenLayedOut() )
	{
		delete m_pEventAfterLayout;
		m_pEventAfterLayout = ScrollToDelayLoadListItem::MakeEvent( this, nItemIndex, behavior, bImmediateScroll );
		return;
	}

	float flItemX, flItemY, flItemWidth, flItemHeight;
	if ( !BGetListItemBounds( nItemIndex, flItemX, flItemY, flItemWidth, flItemHeight ) )
		return;

	ScrollToFitRegion( flItemX, flItemX + flItemWidth, flItemY, flItemY + flItemHeight, behavior, false, bImmediateScroll );
}

bool CDelayLoadList::EventScrollToListItem( const CPanelPtr< IUIPanel > &panelPtr, int nItemIndex, ScrollBehavior_t behavior, bool bImmediateScroll )
{
	ScrollListItemIntoView( nItemIndex, behavior, bImmediateScroll );
	return true;
}

void CDelayLoadList::SavePanelForReuse( CPanel2D *pItemPanel )
{
	if ( !m_bReuseItemPanels )
	{
		delete pItemPanel;
		return;
	}

	// This is a pretty arbitrary amount to save, but basically whenever we scroll
	// we end up briefly having more than what's normally visible show up. So save
	// a few extra to make it so we don't end up allocating/deleting in those cases.
	int nMaxSavedCount = m_nVisibleCount / 4;
	if ( m_pReusePanelCache->GetChildCount() >= nMaxSavedCount )
	{
		delete pItemPanel;
		return;
	}

	pItemPanel->SetParent( m_pReusePanelCache );
	pItemPanel->SetAttribute( k_szItemIndexAttribute, -1 );
}

void CDelayLoadList::GetItemPixelSize( float flContainerWidth, float flContainerHeight, float &flItemPixelWidth, float &flItemPixelHeight )
{
	flItemPixelWidth = 0.0f;
	flItemPixelHeight = 0.0f;

	if ( m_lenItemWidth.GetType() == CUILength::k_EUILengthUnset || m_lenItemHeight.GetType() == CUILength::k_EUILengthUnset )
		return;

	if ( m_lenItemWidth.IsWidthPercentage() || m_lenItemHeight.IsHeightPercentage() )
	{
		Assert( false );
		return;
	}

	if ( m_lenItemWidth.IsHeightPercentage() && m_lenItemHeight.IsWidthPercentage() )
	{
		Assert( false );
		return;
	}

	if ( m_lenItemWidth.IsFitChildren() || m_lenItemWidth.IsFillParentFlow() ||
		m_lenItemHeight.IsFitChildren() || m_lenItemHeight.IsFillParentFlow() )
	{
		Assert( false );
		return;
	}

	if ( m_lenItemWidth.IsHeightPercentage() )
	{
		flItemPixelHeight = m_lenItemHeight.GetValueAsLength( flContainerHeight );
		flItemPixelWidth = flItemPixelHeight * m_lenItemWidth.GetValue() / 100.0f;
	}
	else if ( m_lenItemHeight.IsWidthPercentage() )
	{
		flItemPixelWidth = m_lenItemWidth.GetValueAsLength( flContainerWidth );
		flItemPixelHeight = flItemPixelWidth * m_lenItemHeight.GetValue() / 100.0f;
	}
	else
	{
		flItemPixelWidth = m_lenItemWidth.GetValueAsLength( flContainerWidth );
		flItemPixelHeight = m_lenItemHeight.GetValueAsLength( flContainerHeight );
	}

	flItemPixelWidth *= GetActualUIScaleX();
	flItemPixelHeight *= GetActualUIScaleY();
}

float CDelayLoadList::GetSpacerPixelSize( float flContainerWidth, float flContainerHeight )
{
	if ( m_lenSpacerSize.GetType() == CUILength::k_EUILengthUnset )
		return 0;

	if ( m_lenSpacerSize.IsFitChildren() || m_lenSpacerSize.IsFillParentFlow() )
	{
		Assert( false );
		return 0;
	}

	if ( m_lenSpacerSize.IsHeightPercentage() )
	{
		return GetActualUIScaleY() * m_lenSpacerSize.GetValueAsLength( flContainerHeight );
	}
	else
	{
		return GetActualUIScaleX() * m_lenSpacerSize.GetValueAsLength( flContainerWidth );
	}
}

void CDelayLoadList::OnContentSizeTraverse( float *pflContentWidth, float *pflContentHeight, float flMaxWidth, float flMaxHeight, bool bFinalDimensions )
{
	// Base class handles traversing our children
	BaseClass::OnContentSizeTraverse( pflContentWidth, pflContentHeight, flMaxWidth, flMaxHeight, bFinalDimensions );

	float flInsetLeft, flInsetTop, flInsetRight, flInsetBottom;
	AccessStyle()->GetContentInset( flMaxWidth, flMaxHeight, bFinalDimensions, flInsetLeft, flInsetTop, flInsetRight, flInsetBottom );

	*pflContentWidth = flInsetLeft + flInsetRight;
	*pflContentHeight = flInsetTop + flInsetBottom;

	if ( m_nItemCount == 0 )
		return;

	float flItemWidthPixels = 0.0f;
	float flItemHeightPixels = 0.0f;
	GetItemPixelSize( flMaxWidth, flMaxHeight, flItemWidthPixels, flItemHeightPixels );
	if ( flItemWidthPixels == 0.0f || flItemHeightPixels == 0.0f )
		return;
	const float flSpacerSize = GetSpacerPixelSize( flMaxWidth, flMaxHeight );

	float flContainerWidth = flMaxWidth - flInsetLeft - flInsetRight;
	float flContainerHeight = flMaxHeight - flInsetTop - flInsetBottom;

	EFlowDirection eFlowDirection = k_EFlowNone;
	AccessStyle()->GetFlowChildren( eFlowDirection );

	switch ( eFlowDirection )
	{
		case k_EFlowNone:
			// Super weird, but just stack all the items on top of each other
			*pflContentWidth += flItemWidthPixels;
			*pflContentHeight += flItemHeightPixels;
			break;

		case k_EFlowDown:
			*pflContentWidth += flItemWidthPixels;
			*pflContentHeight += flItemHeightPixels * m_nItemCount + flSpacerSize * ( m_nItemCount / Max( 1, m_nSpacerPeriod ) );
			break;

		case k_EFlowRight:
			*pflContentWidth += flItemWidthPixels * m_nItemCount + flSpacerSize * ( m_nItemCount / Max( 1, m_nSpacerPeriod ) );
			*pflContentHeight += flItemHeightPixels;
			break;

		case k_EFlowDownWrap:
		{			
			const int nRows = Min( m_nItemCount, Max( 1, static_cast< int >( flContainerHeight / flItemHeightPixels ) ) );
			const int nColumns = ( m_nItemCount + nRows - 1 ) / nRows;

			*pflContentWidth += flItemWidthPixels * nColumns;
			if ( m_nSpacerPeriod > 0 )
				*pflContentWidth += flSpacerSize * ( nColumns / m_nSpacerPeriod );
			*pflContentHeight += flItemHeightPixels * nRows;
			break;
		}

		case k_EFlowRightWrap:
		{
			const int nColumns = Min( m_nItemCount, Max( 1, static_cast< int >( flContainerWidth / flItemWidthPixels ) ) );
			int nRows = ( m_nItemCount + nColumns - 1 ) / nColumns;

			*pflContentWidth += flItemWidthPixels * nColumns;
			*pflContentHeight += flItemHeightPixels * nRows;
			if ( m_nSpacerPeriod > 0 )
				*pflContentHeight += flSpacerSize * ( nRows / m_nSpacerPeriod );
			break;
		}

		default:
			AssertMsg( false, "Unknown flow direction!" );
			break;
	}
}

void CDelayLoadList::PaintArea( const PanoramaRect_t &rectPaintArea )
{
	// Calculate which panels should be visible
	int nVisibleStartIndex = 0;
	int nVisibleCount = 0;

	if ( BReadyForDisplay() )
	{
		float flLayoutWidth = GetActualLayoutWidth();
		float flLayoutHeight = GetActualLayoutHeight();

		// Now position all panels that are currently loaded
		float flInsetLeft, flInsetTop, flInsetRight, flInsetBottom;
		AccessStyle()->GetContentInset( flLayoutWidth, flLayoutHeight, false, flInsetLeft, flInsetTop, flInsetRight, flInsetBottom );
		const float flContainerWidth = flLayoutWidth - flInsetLeft - flInsetRight;
		const float flContainerHeight = flLayoutHeight - flInsetTop - flInsetBottom;

		// Sort of unclear why this is necessary, but the paint area doesn't take into
		// account padding/borders. So adjust for that now.
		PanoramaRect_t rectArea = rectPaintArea;
		rectArea.flX -= flInsetLeft;
		rectArea.flY -= flInsetTop;

		CalculateAreaIndexRange( rectArea, flContainerWidth, flContainerHeight, nVisibleStartIndex, nVisibleCount );
	}

	if ( m_nVisibleStartIndex != nVisibleStartIndex || m_nVisibleCount != nVisibleCount )
	{
		// Dispatch an event to update/reload whatever panels are necessary asynchronously, since we can't create/destroy panels during paint
		DispatchEventAsync( DelayLoadListLoadVisiblePanels(), this, m_unLayoutID, nVisibleStartIndex, nVisibleCount );
	}
}

void CDelayLoadList::StoppedPainting()
{
	// Unload all our panels
	ClearLoadedListItems( false );
}

static bool BRangesOverlap( float flMin1, float flMax1, float flMin2, float flMax2 )
{
	return flMin1 <= flMax2 && flMin2 <= flMax1;
}

void CDelayLoadList::CalculateAreaIndexRange( const PanoramaRect_t &rectArea, float flContainerWidth, float flContainerHeight, int &nStartIndex, int &nIndexCount )
{
	EFlowDirection eFlowDirection = k_EFlowNone;
	AccessStyle()->GetFlowChildren( eFlowDirection );

	float flItemWidthPixels = 0.0f;
	float flItemHeightPixels = 0.0f;
	GetItemPixelSize( flContainerWidth, flContainerHeight, flItemWidthPixels, flItemHeightPixels );
	const float flSpacerSize = GetSpacerPixelSize( flContainerWidth, flContainerHeight );

	nStartIndex = 0;
	nIndexCount = 0;

	switch ( eFlowDirection )
	{
		case k_EFlowNone:
		{
			if ( BRangesOverlap( rectArea.flX, rectArea.flX + rectArea.flWidth, 0.0f, flItemWidthPixels ) &&
				BRangesOverlap( rectArea.flY, rectArea.flY + rectArea.flHeight, 0.0f, flItemHeightPixels ) )
			{
				nStartIndex = 0;
				nIndexCount = m_nItemCount;
			}
			break;
		}

		case k_EFlowDown:
		case k_EFlowRightWrap:
		{
			int nColumns = 1;
			if ( eFlowDirection == k_EFlowRightWrap )
			{
				nColumns = Min( m_nItemCount, Max( 1, static_cast< int >( flContainerWidth / flItemWidthPixels ) ) );
			}

			if ( BRangesOverlap( rectArea.flX, rectArea.flX + rectArea.flWidth, 0.0f, flItemWidthPixels * nColumns ) )
			{
				float flTop = rectArea.flY;
				float flBottom = rectArea.flY + rectArea.flHeight;
				const float flSpacerChunkSizePixels = flItemHeightPixels * m_nSpacerPeriod + flSpacerSize;
				if ( flSpacerChunkSizePixels > 0.f )
				{
					int nTopChunkIndex = static_cast<int>( flTop / flSpacerChunkSizePixels );
					float flTopChunkOffset = flTop - nTopChunkIndex * flSpacerChunkSizePixels;
					flTopChunkOffset = Min( flTopChunkOffset, flItemHeightPixels * m_nSpacerPeriod );
					flTop = flItemHeightPixels * m_nSpacerPeriod * nTopChunkIndex + flTopChunkOffset;

					int nBottomChunkIndex = static_cast<int>( flBottom / flSpacerChunkSizePixels );
					float flBottomChunkOffset = flBottom - nBottomChunkIndex * flSpacerChunkSizePixels;
					flBottomChunkOffset = Min( flBottomChunkOffset, flItemHeightPixels * m_nSpacerPeriod );
					flBottom = flItemHeightPixels * m_nSpacerPeriod * nBottomChunkIndex + flBottomChunkOffset;
				}		
				int nStartRowIndex = floorf( flTop / flItemHeightPixels );
				nStartRowIndex = Max( 0, nStartRowIndex );
				int nEndRowIndex = ceilf( flBottom / flItemHeightPixels );
				nStartIndex = nStartRowIndex * nColumns;
				nIndexCount = ( nEndRowIndex * nColumns ) - nStartIndex;
			}
			break;
		}
		
		case k_EFlowRight:
		case k_EFlowDownWrap:
		{
			int nRows = 1;
			if ( eFlowDirection == k_EFlowDownWrap )
			{
				nRows = ( int )( flContainerHeight / flItemHeightPixels );
				nRows = Max( 1, nRows );
			}

			if ( BRangesOverlap( rectArea.flY, rectArea.flY + rectArea.flHeight, 0.0f, flItemHeightPixels * nRows ) )
			{
				float flLeft = rectArea.flX;
				float flRight = rectArea.flX + rectArea.flWidth;
				const float flSpacerChunkSizePixels = flItemWidthPixels * m_nSpacerPeriod + flSpacerSize;
				if ( flSpacerChunkSizePixels > 0.f )
				{
					int nLeftChunkIndex = static_cast<int>( flLeft / flSpacerChunkSizePixels );
					float flLeftChunkOffset = flLeft - nLeftChunkIndex * flSpacerChunkSizePixels;
					flLeftChunkOffset = Min( flLeftChunkOffset, flItemWidthPixels * m_nSpacerPeriod );
					flLeft = flItemWidthPixels * m_nSpacerPeriod * nLeftChunkIndex + flLeftChunkOffset;

					int nRightChunkIndex = static_cast<int>( flRight / flSpacerChunkSizePixels );
					float flRightChunkOffset = flRight - nRightChunkIndex * flSpacerChunkSizePixels;
					flRightChunkOffset = Min( flRightChunkOffset, flItemWidthPixels * m_nSpacerPeriod );
					flRight = flItemWidthPixels * m_nSpacerPeriod * nRightChunkIndex + flRightChunkOffset;
				}
				int nStartColumnIndex = floorf( flLeft / flItemWidthPixels );
				nStartColumnIndex = Max( 0, nStartColumnIndex );
				int nEndColumnIndex = ceilf( flRight / flItemWidthPixels );
				nStartIndex = nStartColumnIndex * nRows;
				nIndexCount = ( nEndColumnIndex * nRows ) - nStartIndex;
			}
			break;
		}
	}

	// Make sure we're not trying to render more indices than we are given
	nIndexCount = Min( nIndexCount, m_nItemCount - nStartIndex );

	// Make sure we don't ever go negative
	nIndexCount = Max( nIndexCount, 0 );
}

void CDelayLoadList::GetIndexRangeForArea( const PanoramaRect_t &rectArea, int &nStartIndex, int &nIndexCount )
{
	float flLayoutWidth = GetActualLayoutWidth();
	float flLayoutHeight = GetActualLayoutHeight();

	// Now position all panels that are currently loaded
	float flInsetLeft, flInsetTop, flInsetRight, flInsetBottom;
	AccessStyle()->GetContentInset( flLayoutWidth, flLayoutHeight, false, flInsetLeft, flInsetTop, flInsetRight, flInsetBottom );
	const float flContainerWidth = flLayoutWidth - flInsetLeft - flInsetRight;
	const float flContainerHeight = flLayoutHeight - flInsetTop - flInsetBottom;

	CalculateAreaIndexRange( rectArea, flContainerWidth, flContainerHeight, nStartIndex, nIndexCount );
}

void CDelayLoadList::OnLayoutTraverse( float flFinalWidth, float flFinalHeight )
{
	// The base class handles traversing our children to let them size themselves
	BaseClass::OnLayoutTraverse( flFinalWidth, flFinalHeight );

	// Now position all panels that are currently loaded
	float flInsetLeft, flInsetTop, flInsetRight, flInsetBottom;
	AccessStyle()->GetContentInset( flFinalWidth, flFinalWidth, false, flInsetLeft, flInsetTop, flInsetRight, flInsetBottom );
	const float flContainerWidth = flFinalWidth - flInsetLeft - flInsetRight;
	const float flContainerHeight = flFinalHeight - flInsetTop - flInsetBottom;

	EFlowDirection eFlowDirection = k_EFlowNone;
	AccessStyle()->GetFlowChildren( eFlowDirection );

	float flItemWidthPixels = 0.0f;
	float flItemHeightPixels = 0.0f;
	GetItemPixelSize( flContainerWidth, flContainerHeight, flItemWidthPixels, flItemHeightPixels );
	const float flSpacerSize = GetSpacerPixelSize( flContainerWidth, flContainerHeight );

	for ( CPanel2D *pItemPanel : Children() )
	{
		int nItemIndex = pItemPanel->GetAttribute( k_szItemIndexAttribute, -1 );
		if ( nItemIndex < 0 )
		{
			Assert( false );
			continue;
		}

		float flItemX = 0.0f;
		float flItemY = 0.0f;
		CalculateItemPosition( nItemIndex, flItemWidthPixels, flItemHeightPixels, flContainerWidth, flContainerHeight, eFlowDirection, m_nSpacerPeriod, flSpacerSize, flItemX, flItemY );

		CUILength lenX( flItemX, CUILength::k_EUILengthLength );
		CUILength lenY( flItemY, CUILength::k_EUILengthLength );
		pItemPanel->SetPositionWithoutTransition( lenX, lenY, CUILength::ZeroLength(), true );
	}

	if ( m_pEventAfterLayout )
	{
		UIEngine()->DispatchEventAsync( 0.0f, m_pEventAfterLayout );
		m_pEventAfterLayout = nullptr;
	}
}

/*static*/ void CDelayLoadList::CalculateItemPosition( int nItemIndex, float flItemWidth, float flItemHeight, float flMaxWidth, float flMaxHeight, EFlowDirection eFlowDirection, int nSpacerPeriod, float flSpacerSize, float &flItemX, float &flItemY )
{
	flItemX = 0.0f;
	flItemY = 0.0f;
	if ( nSpacerPeriod <= 0 )
	{
		nSpacerPeriod = 1;
		flSpacerSize = 0.f;
	}

	switch ( eFlowDirection )
	{
		case k_EFlowDown:
			flItemY = nItemIndex * flItemHeight + flSpacerSize * static_cast<int>( nItemIndex / nSpacerPeriod );
			break;

		case k_EFlowRight:
			flItemX = nItemIndex * flItemWidth + flSpacerSize * static_cast<int>( nItemIndex / nSpacerPeriod );
			break;

		case k_EFlowDownWrap:
		{
			const int nRows = static_cast< int >( Max( 1.f, flMaxHeight / flItemHeight ) );
			const int nColumnIndex = ( nItemIndex / nRows );
			flItemX = nColumnIndex * flItemWidth + flSpacerSize * static_cast<int>( nColumnIndex / nSpacerPeriod );
			flItemY = ( nItemIndex % nRows ) * flItemHeight;
			break;
		}

		case k_EFlowRightWrap:
		{
			const int nColumns = static_cast< int >( Max( 1.f, flMaxWidth / flItemWidth ) );
			const int nRowIndex = ( nItemIndex / nColumns );
			flItemX = ( nItemIndex % nColumns ) * flItemWidth + flSpacerSize * static_cast<int>( nRowIndex / nSpacerPeriod );
			flItemY = ( nItemIndex / nColumns ) * flItemHeight;
			break;
		}

		default:
			Assert( false );
		case k_EFlowNone:
			break;
	}
}

bool CDelayLoadList::BGetListItemBounds( int nItemIndex, float &flItemX, float &flItemY, float &flItemWidth, float &flItemHeight )
{
	if ( !UIPanel()->BHasBeenLayedOut() )
		return false;

	if ( nItemIndex < 0 || nItemIndex >= m_nItemCount )
		return false;

	float flLayoutWidth = GetActualLayoutWidth();
	float flLayoutHeight = GetActualLayoutHeight();

	// Now position all panels that are currently loaded
	float flInsetLeft, flInsetTop, flInsetRight, flInsetBottom;
	AccessStyle()->GetContentInset( flLayoutWidth, flLayoutHeight, false, flInsetLeft, flInsetTop, flInsetRight, flInsetBottom );
	const float flContainerWidth = flLayoutWidth - flInsetLeft - flInsetRight;
	const float flContainerHeight = flLayoutHeight - flInsetTop - flInsetBottom;

	GetItemPixelSize( flContainerWidth, flContainerHeight, flItemWidth, flItemHeight );
	const float flSpacerPixelSize = GetSpacerPixelSize( flContainerWidth, flContainerHeight );

	EFlowDirection eFlowDirection = k_EFlowNone;
	AccessStyle()->GetFlowChildren( eFlowDirection );

	CalculateItemPosition( nItemIndex, flItemWidth, flItemHeight, flContainerWidth, flContainerHeight, eFlowDirection, m_nSpacerPeriod, flSpacerPixelSize, flItemX, flItemY );

	return true;
}

bool CDelayLoadList::BSetProperties( const CUtlVector< ParsedPanelProperty_t > &vecProperties )
{
	static const CPanoramaSymbol k_symItemWidth( "itemwidth" );
	static const CPanoramaSymbol k_symItemHeight( "itemheight" );
	static const CPanoramaSymbol k_symSpacerSize( "spacersize" );
	static const CPanoramaSymbol k_symSpacerPeriod( "spacerperiod" );

	bool bSuccess = true;

	CUILength lenItemWidth, lenItemHeight, lenSpacerSize;
	int nSpacerPeriod = -1;
	for ( const ParsedPanelProperty_t &prop : vecProperties )
	{
		if ( prop.m_symName == k_symItemWidth )
		{
			if ( !CSSHelpers::BParseIntoUILength( &lenItemWidth, prop.m_pchValue ) )
				bSuccess = false;
		}
		else if ( prop.m_symName == k_symItemHeight )
		{
			if ( !CSSHelpers::BParseIntoUILength( &lenItemHeight, prop.m_pchValue ) )
				bSuccess = false;
		}
		else if ( prop.m_symName == k_symSpacerSize )
		{
			if ( !CSSHelpers::BParseIntoUILength( &lenSpacerSize, prop.m_pchValue ) )
				bSuccess = false;
		}
		else if ( prop.m_symName == k_symSpacerPeriod )
		{
			nSpacerPeriod = V_atoi( prop.m_pchValue );
		}
	}

	if ( lenItemWidth.IsSet() && lenItemHeight.IsSet() )
	{
		SetListItemSize( lenItemWidth, lenItemHeight );
	}
	else
	{
		AssertMsg1( !lenItemWidth.IsSet() && !lenItemHeight.IsSet(), "DelayLoadList (%s): both itemwidth and itemheight must be set", GetID() );
	}

	if ( lenSpacerSize.IsSet() && nSpacerPeriod > 0 )
	{
		SetSpacerPeriodAndSize( nSpacerPeriod, lenSpacerSize );
	}
	else if ( vecProperties.Count() > 0 ) // Delayed property pass means we'll always assert here, only bother if there was more than one property passed
	{
		AssertMsg1( lenSpacerSize.IsSet() && nSpacerPeriod != -1, "DelayLoadList (%s): both spacersize and spacerperiod must be set", GetID() );
	}

	if ( !BaseClass::BSetProperties( vecProperties ) )
		bSuccess = false;

	return bSuccess;
}

bool CDelayLoadList::EventLoadVisiblePanels( ListLayoutID unLayoutID, int nVisibleStartIndex, int nVisibleCount )
{
	// If something changed our layout while this event was in-flight, then just ignore it and assume the next version will take care of it.
	if ( m_unLayoutID != unLayoutID )
		return true;

	// If we have no load function, there's nothing we can do
	if ( !m_fnLoadItem )
		return true;

	// Anything to do?
	if ( m_nVisibleStartIndex == nVisibleStartIndex && m_nVisibleCount == nVisibleCount )
		return true;

	CUtlVector< CPanel2D * > vecItemPanels;
	vecItemPanels.SetCount( nVisibleCount );
	vecItemPanels.FillWithValue( nullptr );

	// Step 1) Find all the panels we can just use without reloading
	for ( CPanel2D *pChild : Children() )
	{
		int nItemIndex = pChild->GetAttribute( k_szItemIndexAttribute, -1 );
		if ( nItemIndex < nVisibleStartIndex || nItemIndex >= nVisibleStartIndex + nVisibleCount )
			continue;

		vecItemPanels[ nItemIndex - nVisibleStartIndex ] = pChild;
	}

	// Step 2) Move panels into slots so that we can reuse them. Delete any extras
	int iFreeSlot = m_bReuseItemPanels ? 0 : nVisibleCount;
	for ( int iChild = 0; iChild < GetChildCount(); ++iChild )
	{
		CPanel2D *pChild = GetChild( iChild );

		// Is this child already correct?
		int nItemIndex = pChild->GetAttribute( k_szItemIndexAttribute, -1 );
		if ( nItemIndex >= nVisibleStartIndex && nItemIndex < nVisibleStartIndex + nVisibleCount )
			continue;

		// Find the next free slot to place this child into
		for ( ; iFreeSlot < nVisibleCount; ++iFreeSlot )
		{
			if ( vecItemPanels[ iFreeSlot ] == nullptr )
				break;
		}

		if ( iFreeSlot < nVisibleCount )
		{
			vecItemPanels[ iFreeSlot ] = pChild;
			++iFreeSlot;
		}
		else
		{
			SavePanelForReuse( pChild );
			--iChild; // counteract the increment in the for loop
		}
	}

	// Step 3) If we still have empty slots, try to reuse panels we saved earlier
	while ( iFreeSlot < nVisibleCount && m_pReusePanelCache->GetChildCount() > 0 )
	{
		// Find the next free slot to place this child into
		for ( ; iFreeSlot < nVisibleCount; ++iFreeSlot )
		{
			if ( vecItemPanels[ iFreeSlot ] == nullptr )
				break;
		}

		if ( iFreeSlot >= nVisibleCount )
			break;

		CPanel2D *pReuseItem = m_pReusePanelCache->GetLastChild();
		pReuseItem->SetParent( this );
		vecItemPanels[ iFreeSlot ] = pReuseItem;
		++iFreeSlot;
	}

	// Step 4) Go through and actually load any panels necessary
	bool bAnyPanelsLoaded = false;
	for ( int iSlot = 0; iSlot < nVisibleCount; ++iSlot )
	{
		int nSlotItemIndex = nVisibleStartIndex + iSlot;

		CPanel2D *pExistingItem = vecItemPanels[ iSlot ];
		if ( pExistingItem )
		{
			// If it already matches, then there's no work to do
			int nExistingItemIndex = pExistingItem->GetAttribute( k_szItemIndexAttribute, -1 );
			if ( nExistingItemIndex == nSlotItemIndex )
				continue;
		}

		// Now actually do the load
		CPanel2D *pNewItem = m_fnLoadItem( this, nSlotItemIndex, pExistingItem );
		if ( pNewItem )
		{
			pNewItem->SetAttribute( k_szItemIndexAttribute, nSlotItemIndex );
			bAnyPanelsLoaded = true;
		}

		// See if they didn't actually reuse the provided panel
		if ( pExistingItem && pNewItem != pExistingItem )
		{
			// Don't bother trying to save for reuse, since it already failed once
			delete pExistingItem;
		}

		// Shouldn't actually matter, but helpful for debugging
		vecItemPanels[ iSlot ] = pNewItem;
	}

	// Step 5) Now sort all the panels by item index
	if ( bAnyPanelsLoaded )
	{
		SortChildren( ComparePanelByItemIndex );
	}

	// Step 6) And finally record the new values
	m_nVisibleStartIndex = nVisibleStartIndex;
	m_nVisibleCount = nVisibleCount;

	return true;
}

/*static*/ int CDelayLoadList::ComparePanelByItemIndex( const IUIPanelClient *pPanel1, const IUIPanelClient *pPanel2 )
{
	const CPanel2D *pLeftPanel = static_cast< const CPanel2D * >( pPanel1 );
	const CPanel2D *pRightPanel = static_cast< const CPanel2D * >( pPanel2 );

	int nLeftItemIndex = pLeftPanel->GetAttribute( k_szItemIndexAttribute, -1 );
	int nRightItemIndex = pRightPanel->GetAttribute( k_szItemIndexAttribute, -1 );

	if ( nLeftItemIndex == nRightItemIndex )
		return 0;

	return nLeftItemIndex < nRightItemIndex ? -1 : 1;
}

bool CDelayLoadList::EventReadyForDisplay( const CPanelPtr< IUIPanel > &panelPtr )
{
	SetRepaint( k_EPanelRepaintFull );
	return true;
}

bool CDelayLoadList::EventUnreadyForDisplay( const CPanelPtr< IUIPanel > &panelPtr )
{
	ClearLoadedListItems( false );
	return true;
}
