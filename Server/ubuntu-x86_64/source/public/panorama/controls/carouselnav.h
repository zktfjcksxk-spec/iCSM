//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef CAROUSEL_NAV_H
#define CAROUSEL_NAV_H

#ifdef _WIN32
#pragma once
#endif

#include "panel2d.h"

namespace panorama
{

class CCarousel;
class CButton;

//-----------------------------------------------------------------------------
// Purpose: Carousel Navigation control. Shows arrows to move left/right on the
// carousel, as well as a set of buttons to jump to a specific spot in the carousel.
//-----------------------------------------------------------------------------
class CCarouselNav : public CPanel2D
{
	DECLARE_PANEL2D( CCarouselNav, CPanel2D );

public:
	CCarouselNav( CPanel2D *parent, const char * pchPanelID );
	virtual ~CCarouselNav();

	void SetCarousel( CCarousel *pCarousel );

	virtual bool BSetProperty( CPanoramaSymbol symName, const char *pchValue ) OVERRIDE;

private:
	CCarousel *FindCarousel( const char *pchCarouselID );
	void UpdateControls();

	bool EventPanelLoaded( const CPanelPtr< IUIPanel > &panelPtr );
	bool EventCarouselSelectionChanged( CPanelPtr< CPanel2D > pSelectedChild );
	bool EventCarouselChildrenChanged( const CPanelPtr< IUIPanel > &panelPtr );
	bool EventSetCarouselSelectedIndex( int nChildIndex );
	bool EventIncrementCarouselSelectedIndex( int nChildIndexOffset );
	bool EventStyleFlagsChanged( const CPanelPtr< IUIPanel > &panelPtr );

	CUtlString m_strCarouselID;
	CUtlString m_strIncrementSound;
	CPanelPtr< CCarousel > m_pCarousel;
	int m_nMaxPips;
	int m_nSteps;
	bool m_bWrapAround;
	int m_nSelectedIndex;
	bool m_bIgnoreDisabledChildren;

	CPanel2D *m_pItemPipsPanel;
	CButton *m_pPreviousItemButton;
	CButton *m_pNextItemButton;
};


} // namespace panorama

#endif // CAROUSEL_NAV_H
