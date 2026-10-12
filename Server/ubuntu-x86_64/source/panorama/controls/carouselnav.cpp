//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx_client.h"
#include "panorama/controls/carouselnav.h"
#include "panorama/controls/carousel.h"
#include "panorama/controls/button.h"
#include "panorama/iuisoundsystem.h"


// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

REGISTER_PANEL2D_FACTORY( CCarouselNav, CarouselNav );

DECLARE_PANORAMA_EVENT1( CarouselNavSetSelectedIndex, int );
DECLARE_PANORAMA_EVENT1( CarouselNavIncrementSelectedIndex, int );

DEFINE_PANORAMA_EVENT( CarouselNavSetSelectedIndex );
DEFINE_PANORAMA_EVENT( CarouselNavIncrementSelectedIndex );

static const char k_szCarouselNavEnabledAttribute[] = "carousel_nav_enabled";

CCarouselNav::CCarouselNav( CPanel2D *parent, const char * pchPanelID )
	: CPanel2D( parent, pchPanelID )
	, m_nMaxPips( 10 )
	, m_nSteps( 1 )
	, m_bWrapAround( false )
	, m_nSelectedIndex( -1 )
	, m_bIgnoreDisabledChildren( false )
{
	DbgVerify( BLoadLayout( "file://{resources}/layout/carousel_nav.xml" ) );

	m_pItemPipsPanel = FindChildInLayoutFile( "ItemPips" );
	m_pPreviousItemButton = assert_cast< CButton * >( FindChildInLayoutFile( "PreviousItemButton" ) );
	m_pNextItemButton = assert_cast< CButton * >( FindChildInLayoutFile( "NextItemButton" ) );

	// Register for events
	if ( !UIEngine()->BHaveEventHandlersRegisteredForType( CCarouselNav::GetPanelSymbol() ) )
	{
		RegisterEventHandlerOnPanelType( CarouselNavSetSelectedIndex(), &CCarouselNav::EventSetCarouselSelectedIndex );
		RegisterEventHandlerOnPanelType( CarouselNavIncrementSelectedIndex(), &CCarouselNav::EventIncrementCarouselSelectedIndex );
		RegisterEventHandlerOnPanelType( PanelLoaded(), &CCarouselNav::EventPanelLoaded );
	}

	UpdateControls();
}

CCarouselNav::~CCarouselNav()
{
	if ( m_pCarousel.Get() )
	{
		UnregisterEventHandlerOnPanel( SetCarouselSelectedChild(), m_pCarousel->UIPanel(), this, &CCarouselNav::EventCarouselSelectionChanged );
		UnregisterEventHandlerOnPanel( CarouselChildrenChanged(), m_pCarousel->UIPanel(), this, &CCarouselNav::EventCarouselChildrenChanged );
		UnregisterEventHandlerOnPanel( StyleFlagsChanged(), m_pCarousel->UIPanel(), this, &CCarouselNav::EventStyleFlagsChanged );
	}
}

bool CCarouselNav::BSetProperty( CPanoramaSymbol symName, const char *pchValue )
{
	static const CPanoramaSymbol k_symCarouselID( "carouselid" );
	static const CPanoramaSymbol k_symIncrementSound("incrementsound");
	static const CPanoramaSymbol k_symMaxPips( "maxpips" );
	static const CPanoramaSymbol k_symWrapAround( "wraparound" );
	static const CPanoramaSymbol k_symSteps( "steps" );
	static const CPanoramaSymbol k_symIgnoreDisabledChildren( "ignoredisabledchildren" );
	static const CPanoramaSymbol k_symDisableCarouselNav( "disablenav" );

	if ( symName == k_symCarouselID )
	{
		m_strCarouselID = pchValue;
		return true;
	}
	else if ( symName == k_symIncrementSound )
	{
		m_strIncrementSound = pchValue;
		return true;
	}
	else if ( symName == k_symMaxPips )
	{
		m_nMaxPips = V_atoi( pchValue );
		return true;
	}
	else if ( symName == k_symWrapAround )
	{
		return CSSHelpers::BParseTrueFalse( pchValue, &m_bWrapAround );
	}
	else if ( symName == k_symSteps )
	{
		m_nSteps = V_atoi( pchValue );
	}
	else if ( symName == k_symIgnoreDisabledChildren )
	{
		return CSSHelpers::BParseTrueFalse( pchValue, &m_bIgnoreDisabledChildren );
	}
	else if( symName == k_symDisableCarouselNav )
	{
		AddClass( "DisableNav" );
		return true;
	}

	return BaseClass::BSetProperty( symName, pchValue );
}

bool CCarouselNav::EventPanelLoaded( const CPanelPtr< IUIPanel > &panelPtr )
{
	if ( m_strCarouselID.IsEmpty() )
		return false;

	CCarousel *pCarousel = FindCarousel( m_strCarouselID );
	if ( !pCarousel )
	{
		AssertMsg2( false, "CarouselNav '%s': unable to find a Carousel with ID '%s'", GetID(), m_strCarouselID.Get() );
		return false;
	}

	m_strCarouselID = "";
	SetCarousel( pCarousel );
	return false;
}

CCarousel *CCarouselNav::FindCarousel( const char *pchCarouselID )
{
	if ( !pchCarouselID || pchCarouselID[ 0 ] == '\0' )
		return nullptr;

	CPanel2D *pParent = GetParent();
	if ( !pParent )
		return nullptr;

	// Search for the carousel anywhere in our parent's layout file
	CPanel2D *pCarouselPanel = pParent->FindPanelInLayoutFile( pchCarouselID );
	if ( !pCarouselPanel )
		return nullptr;

	CCarousel *pCarousel = dynamic_cast< CCarousel * >( pCarouselPanel );
	if ( !pCarousel )
	{
		AssertMsg2( false, "Found a panel of type '%s' rather than a Carousel for id '%s'", pCarouselPanel->GetPanelType().String(), pCarouselPanel->GetID() );
		return nullptr;
	}

	return pCarousel;
}

void CCarouselNav::SetCarousel( CCarousel *pCarousel )
{
	if ( pCarousel == m_pCarousel.Get() )
		return;

	// Unregister event listeners from old panel
	if ( m_pCarousel.Get() )
	{
		UnregisterEventHandlerOnPanel( SetCarouselSelectedChild(), m_pCarousel->UIPanel(), this, &CCarouselNav::EventCarouselSelectionChanged );
		UnregisterEventHandlerOnPanel( CarouselChildrenChanged(), m_pCarousel->UIPanel(), this, &CCarouselNav::EventCarouselChildrenChanged );
		UnregisterEventHandlerOnPanel( StyleFlagsChanged(), m_pCarousel->UIPanel(), this, &CCarouselNav::EventStyleFlagsChanged );
	}

	m_pCarousel = pCarousel;

	// Register event listeners on new panel
	if ( m_pCarousel.Get() )
	{
		RegisterEventHandlerOnPanel( SetCarouselSelectedChild(), m_pCarousel->UIPanel(), this, &CCarouselNav::EventCarouselSelectionChanged );
		RegisterEventHandlerOnPanel( CarouselChildrenChanged(), m_pCarousel->UIPanel(), this, &CCarouselNav::EventCarouselChildrenChanged );
		RegisterEventHandlerOnPanel( StyleFlagsChanged(), m_pCarousel->UIPanel(), this, &CCarouselNav::EventStyleFlagsChanged );
	}

	UpdateControls();
}

void CCarouselNav::UpdateControls()
{
	static const CPanoramaSymbol k_symPaginationButton( "PaginationButton" );
	static const CPanoramaSymbol k_symOverMaxPips( "OverMaxPips" );

	int nCarouselItems = 0;
	if ( m_pCarousel.Get() )
	{
		if ( m_bIgnoreDisabledChildren )
		{
			for ( CPanel2D *pChild : m_pCarousel->Children() )
			{
				if ( !pChild->IsEnabled() )
					continue;

				++nCarouselItems;
			}
		}
		else
		{
			nCarouselItems = m_pCarousel->GetChildCount();
		}
	}

	int nSelectedIndex = -1;
	if ( m_pCarousel.Get() )
	{
		nSelectedIndex = m_pCarousel->GetFocusIndex();

		if ( m_bIgnoreDisabledChildren )
		{
			int nDisabledChildrenBeforeSelectedIndex = 0;
			for ( int i = 0; i < nSelectedIndex; ++i )
			{
				CPanel2D *pChild = m_pCarousel->GetChild( i );
				if ( pChild && !pChild->IsEnabled() )
				{
					++nDisabledChildrenBeforeSelectedIndex;
				}
			}

			nSelectedIndex -= nDisabledChildrenBeforeSelectedIndex;
		}

		if ( nSelectedIndex < 0 )
		{
			nSelectedIndex = 0;
		}
	}

	bool bOverMaxPips = nCarouselItems > m_nMaxPips;

	// Update the page button controls to match the number of children of the carousel
	int nExpectedCarouselButtons = bOverMaxPips ? 0 : nCarouselItems;
	while ( m_pItemPipsPanel->GetChildCount() != nExpectedCarouselButtons )
	{
		if ( m_pItemPipsPanel->GetChildCount() < nExpectedCarouselButtons )
		{
			CButton *pItemButton = new CButton( m_pItemPipsPanel, nullptr );
			pItemButton->AddClass( k_symPaginationButton );
			pItemButton->SetOnActivateEvent( CarouselNavSetSelectedIndex::MakeEvent( pItemButton, m_pItemPipsPanel->GetChildCount() - 1 ) );
		}
		else
		{
			delete m_pItemPipsPanel->GetChild( m_pItemPipsPanel->GetChildCount() - 1 );
		}
	}

	// If we just deleted the button we thought we had selected, then don't consider it selected anymore
	if ( m_nSelectedIndex >= nExpectedCarouselButtons )
	{
		m_nSelectedIndex = -1;
	}

	// Update the selected state
	if ( m_nSelectedIndex != nSelectedIndex )
	{
		if ( m_nSelectedIndex >= 0 && m_nSelectedIndex < m_pItemPipsPanel->GetChildCount() )
		{
			CButton *pOldSelectedButton = assert_cast< CButton * >( m_pItemPipsPanel->GetChild( m_nSelectedIndex ) );
			pOldSelectedButton->SetSelected( false );
			m_nSelectedIndex = -1;
		}

		if ( nSelectedIndex >= 0 && nSelectedIndex < m_pItemPipsPanel->GetChildCount() )
		{
			CButton *pNewSelectedButton = assert_cast< CButton * >( m_pItemPipsPanel->GetChild( nSelectedIndex ) );
			pNewSelectedButton->SetSelected( true );
			m_nSelectedIndex = nSelectedIndex;
		}
	}

	SetHasClass( k_symOverMaxPips, bOverMaxPips );
	SetEnabled( nCarouselItems > 1 );

	// Update the dialog variables for the label
	SetDialogVariable( "current_item", nSelectedIndex + 1 );
	SetDialogVariable( "total_items", nCarouselItems );

	// Update forward/back button enabled state
	m_pPreviousItemButton->SetEnabled( nSelectedIndex >= 0 && ( m_bWrapAround || nSelectedIndex != 0 ) );
	m_pNextItemButton->SetEnabled( nSelectedIndex >= 0 && ( m_bWrapAround || nSelectedIndex < nCarouselItems - m_nSteps ) );

	// If we're watching for children to become disabled/enabled, then set an attribute tracking our last known state.
	// This lets us early out when styles change if the enabled/state didn't change from our last update
	if ( m_bIgnoreDisabledChildren && m_pCarousel.Get() )
	{
		for ( CPanel2D *pChild : m_pCarousel->Children() )
		{
			pChild->SetAttribute( k_szCarouselNavEnabledAttribute, pChild->IsEnabled() ? 1 : 0 );
		}
	}
}

bool CCarouselNav::EventCarouselSelectionChanged( CPanelPtr< CPanel2D > pSelectedChild )
{
	UpdateControls();
	return false;
}

bool CCarouselNav::EventCarouselChildrenChanged( const CPanelPtr< IUIPanel > &panelPtr )
{
	UpdateControls();
	return false;
}

bool CCarouselNav::EventSetCarouselSelectedIndex( int nChildIndex )
{
	if ( !m_pCarousel.Get() )
		return true;

	if ( nChildIndex < 0 || nChildIndex >= m_pCarousel->GetChildCount() )
		return true;

	// If we're supposed to ignore disabled children, convert the child index into a real child index
	if ( m_bIgnoreDisabledChildren )
	{
		int nRealChildIndex = 0;
		int nRealChildCount = m_pCarousel->GetChildCount();

		while ( nRealChildIndex < nRealChildCount )
		{
			CPanel2D *pChild = m_pCarousel->GetChild( nRealChildIndex );
			if ( pChild->IsEnabled() )
			{
				if ( nChildIndex == 0 )
					break;

				nChildIndex--;
			}

			nRealChildIndex++;
		}

		nChildIndex = nRealChildIndex;
	}

	CPanel2D *pChild = m_pCarousel->GetChild( nChildIndex );
	m_pCarousel->SetSelectedChild( pChild );
	return true;
}

bool CCarouselNav::EventIncrementCarouselSelectedIndex( int nChildIndexOffset )
{
	if ( !m_pCarousel.Get() )
		return true;

	int nChildCount = m_pCarousel->GetChildCount();
	if ( nChildCount == 0 )
		return true;

	if ( m_strIncrementSound.Length() > 0 )
	{
		UISoundSystem()->PlaySound( m_strIncrementSound.Get(), UIPanel(), k_ESoundType_Effects, 1.0f );
	}

	// Apply the offset. This is a loop because if we're ignoring disabled children we might need to walk through multiple of them
	CPanel2D *pChild = nullptr;
	int nIteration = 0;
	int nChildIndex = m_pCarousel->GetFocusIndex();
	while ( !pChild && nIteration < nChildCount )
	{
		nChildIndex = nChildIndex + ( nChildIndexOffset * m_nSteps );

		// Wrap around if requested
		if ( m_bWrapAround )
		{
			if ( nChildIndex < 0 )
				nChildIndex += nChildCount;

			nChildIndex = nChildIndex % nChildCount;
		}
		else
		{
			nChildIndex = Max( 0, nChildIndex );
		}

		pChild = m_pCarousel->GetChild( nChildIndex );
		if ( pChild->IsEnabled() || !m_bIgnoreDisabledChildren )
		{
			m_pCarousel->SetSelectedChild( pChild );
			return true;
		}

		nIteration++;
	}

	return true;
}

bool CCarouselNav::EventStyleFlagsChanged( const CPanelPtr< IUIPanel > &panelPtr )
{
	if ( !m_bIgnoreDisabledChildren )
		return false;

	if ( !m_pCarousel.Get() )
		return false;

	CPanel2D *pPanel = ToPanel2D( panelPtr.Get() );
	if ( !pPanel )
		return false;

	// Only care about direct children of the carousel
	if ( pPanel->GetParent() != m_pCarousel.Get() )
		return false;

	// We only want to do work if the enabled state of the child changed
	bool bCarouselNavThinksEnabled = pPanel->GetAttribute( k_szCarouselNavEnabledAttribute, 1 ) != 0;
	if ( bCarouselNavThinksEnabled != pPanel->IsEnabled() )
	{
		UpdateControls();
	}

	return false;
}
