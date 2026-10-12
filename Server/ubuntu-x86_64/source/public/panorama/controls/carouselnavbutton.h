//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef CAROUSEL_NAV_BUTTON_H
#define CAROUSEL_NAV_BUTTON_H

#ifdef _WIN32
#pragma once
#endif

#include "button.h"

namespace panorama
{

class CCarousel;

//-----------------------------------------------------------------------------
// Purpose: Carousel Navigation Button control that moves forward or backwards through a carousel.
// Automatically enables/disables itself based on the selected index.
//-----------------------------------------------------------------------------
class CCarouselNavButton : public CButton
{
	DECLARE_PANEL2D( CCarouselNavButton, CButton );

public:
	CCarouselNavButton( CPanel2D *parent, const char * pchPanelID );
	virtual ~CCarouselNavButton();

	void SetCarousel( CCarousel *pCarousel );

	virtual bool BSetProperty( CPanoramaSymbol symName, const char *pchValue ) OVERRIDE;

private:
	CCarousel *FindCarousel( const char *pchCarouselID );
	void UpdateControls();

	bool EventPanelLoaded( const CPanelPtr< IUIPanel > &panelPtr );
	bool EventCarouselSelectionChanged( CPanelPtr< CPanel2D > pSelectedChild );
	bool EventCarouselChildrenChanged( const CPanelPtr< IUIPanel > &panelPtr );
	bool EventActivated( const CPanelPtr< IUIPanel > &panelPtr, EPanelEventSource_t eSource );

	CUtlString m_strCarouselID;
	CPanelPtr< CCarousel > m_pCarousel;
};


} // namespace panorama

#endif // CAROUSEL_NAV_H
