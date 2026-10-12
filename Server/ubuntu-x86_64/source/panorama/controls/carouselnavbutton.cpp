//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx_client.h"
#include "panorama/controls/carouselnavbutton.h"
#include "panorama/controls/carousel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

REGISTER_PANEL2D_FACTORY( CCarouselNavButton, CarouselNavButton );

CCarouselNavButton::CCarouselNavButton( CPanel2D *parent, const char * pchPanelID )
	: CButton( parent, pchPanelID )
{
	// Register for events
	if ( !UIEngine()->BHaveEventHandlersRegisteredForType( CCarouselNavButton::GetPanelSymbol() ) )
	{
		RegisterEventHandlerOnPanelType( PanelLoaded(), &CCarouselNavButton::EventPanelLoaded );
		RegisterEventHandlerOnPanelType( Activated(), &CCarouselNavButton::EventActivated );
	}

	UpdateControls();
}

CCarouselNavButton::~CCarouselNavButton()
{
	if ( m_pCarousel.Get() )
	{
		UnregisterEventHandlerOnPanel( SetCarouselSelectedChild(), m_pCarousel->UIPanel(), this, &CCarouselNavButton::EventCarouselSelectionChanged );
		UnregisterEventHandlerOnPanel( CarouselChildrenChanged(), m_pCarousel->UIPanel(), this, &CCarouselNavButton::EventCarouselChildrenChanged );
	}
}

bool CCarouselNavButton::BSetProperty( CPanoramaSymbol symName, const char *pchValue )
{
	static const CPanoramaSymbol k_symCarouselID( "carouselid" );

	if ( symName == k_symCarouselID )
	{
		m_strCarouselID = pchValue;
		return true;
	}

	return BaseClass::BSetProperty( symName, pchValue );
}

bool CCarouselNavButton::EventPanelLoaded( const CPanelPtr< IUIPanel > &panelPtr )
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

CCarousel *CCarouselNavButton::FindCarousel( const char *pchCarouselID )
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

void CCarouselNavButton::SetCarousel( CCarousel *pCarousel )
{
	if ( pCarousel == m_pCarousel.Get() )
		return;

	// Unregister event listeners from old panel
	if ( m_pCarousel.Get() )
	{
		UnregisterEventHandlerOnPanel( SetCarouselSelectedChild(), m_pCarousel->UIPanel(), this, &CCarouselNavButton::EventCarouselSelectionChanged );
		UnregisterEventHandlerOnPanel( CarouselChildrenChanged(), m_pCarousel->UIPanel(), this, &CCarouselNavButton::EventCarouselChildrenChanged );
	}

	m_pCarousel = pCarousel;

	// Register event listeners on new panel
	if ( m_pCarousel.Get() )
	{
		RegisterEventHandlerOnPanel( SetCarouselSelectedChild(), m_pCarousel->UIPanel(), this, &CCarouselNavButton::EventCarouselSelectionChanged );
		RegisterEventHandlerOnPanel( CarouselChildrenChanged(), m_pCarousel->UIPanel(), this, &CCarouselNavButton::EventCarouselChildrenChanged );
	}

	UpdateControls();
}

void CCarouselNavButton::UpdateControls()
{
	int nCarouselItems = m_pCarousel.Get() ? m_pCarousel->GetChildCount() : 0;

	int nSelectedIndex = -1;
	if ( m_pCarousel.Get() )
	{
		nSelectedIndex = m_pCarousel->GetFocusIndex();
		if ( nSelectedIndex < 0 )
		{
			nSelectedIndex = 0;
		}
	}

	static const CPanoramaSymbol k_symNavBackward( "NavBackward" );
	bool bNavigateForward = !BHasClass( k_symNavBackward );

	bool bEnabled = nCarouselItems > 0 && nSelectedIndex >= 0;
	if ( bEnabled )
	{
		if ( bNavigateForward )
		{
			bEnabled = nSelectedIndex != nCarouselItems - 1;
		}
		else
		{
			bEnabled = nSelectedIndex != 0;
		}
	}
	SetEnabled( bEnabled );
}

bool CCarouselNavButton::EventCarouselSelectionChanged( CPanelPtr< CPanel2D > pSelectedChild )
{
	UpdateControls();
	return false;
}

bool CCarouselNavButton::EventCarouselChildrenChanged( const CPanelPtr< IUIPanel > &panelPtr )
{
	UpdateControls();
	return false;
}

bool CCarouselNavButton::EventActivated( const CPanelPtr< IUIPanel > &panelPtr, EPanelEventSource_t eSource )
{
	if ( panelPtr.Get() != UIPanel() )
		return false;

	if ( !m_pCarousel.Get() )
		return false;

	int nChildCount = m_pCarousel->GetChildCount();
	if ( nChildCount == 0 )
		return false;

	static const CPanoramaSymbol k_symNavBackward( "NavBackward" );
	int nChildIndexOffset = BHasClass( k_symNavBackward ) ? -1 : 1;

	int nChildIndex = m_pCarousel->GetFocusIndex() + nChildIndexOffset;
	if ( nChildIndex < 0 || nChildIndex >= m_pCarousel->GetChildCount() )
	{
		Assert( false );
		return false;
	}

	CPanel2D *pChild = m_pCarousel->GetChild( nChildIndex );
	m_pCarousel->SetSelectedChild( pChild );

	return true;
}
