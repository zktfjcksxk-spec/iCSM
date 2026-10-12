//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//
#pragma once

#include "panorama/controls/panel2d.h"
#include "panorama/iuipanel.h"
#include "panorama/controls/panelptr.h"

//-----------------------------------------------------------------------------
// Purpose: CSGO Blur Test
// Test background blur...
//-----------------------------------------------------------------------------

class CCSGO_BlurTarget : public panorama::CPanel2D
{
	DECLARE_PANEL2D( CCSGO_BlurTarget, panorama::CPanel2D );

public:
	CCSGO_BlurTarget( panorama::CPanel2D *pParent, const char *pchID );

	virtual void SetupJavascriptObjectTemplate() OVERRIDE;
	virtual void Paint( ) OVERRIDE;

	// Adding / Removing blur rectangles dynamically
	// (Note that deleted panels will automatically be removed from m_vecBlurRects in Paint())
	void AddBlurPanel( CPanel2D *pPanel );
	void RemoveBlurPanel( CPanel2D *pPanel );

protected:

	bool m_bBlurRectInitialized;
	void GrabRects();

	// List of panels to track. Each panel in that list will correspond to a blur rectangle
	// List populated on the first call to Paint (reading "blurrects" xml attribute) 
	// or dynamically by calling AddBlurPanel
	// Every call to CCSGO_BlurTarget::Paint() will check that panels have not been deleted. 
	// If a panel has been deleted, we will delete the corresponding entry in m_vecBlurRects.
	CUtlVector< panorama::CPanelPtr< panorama::IUIPanel > > m_vecBlurRects;
	
	// List of panels used to build the BlurPanelsCommand_t render command
	// Populated in Paint() from m_vecBlurRects (valid panels only)
	CUtlVector<uint64> m_vecPaintBlurRects;
};


