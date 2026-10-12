//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef PANORAMA_RENDERPANEL_H
#define PANORAMA_RENDERPANEL_H
#pragma once

#include "panorama/controls/panel2d.h"

namespace panorama
{

//-----------------------------------------------------------------------------
// Purpose: Render panel, to use for raw source2 render operations directly in render thread
//-----------------------------------------------------------------------------
class CRenderPanel : public CPanel2D
{
	DECLARE_PANEL2D( CRenderPanel, CPanel2D );

public:
	CRenderPanel( CPanel2D *parent, const char * pchPanelID, uint32 ePanelFlags = 0 );
	virtual ~CRenderPanel();

	// Note that if k_ERenderCallbackFlagsAlwaysRepaint or k_ERenderCallbackFlagsManualRepaint is
	// given in the flags then BShouldAlwaysRepaint is not used.
	void SetRenderThreadCallback( CRenderThreadCallback *pRenderCallback, ERenderCallbackFlags eFlags = k_ERenderCallbackFlagsDefault );

	// Override and make return true if you need to paint every single frame, and will not manually call SetRepaint when you want to repaint
	virtual bool BShouldAlwaysRepaint() { return true; }

protected:
	// Override of Panel2D paint
	virtual void Paint() OVERRIDE;

	CRefPtr< IUITexture > m_pPanelRT;

private:
	CRenderThreadCallback *m_pRenderCallback;
	ERenderCallbackFlags m_eFlags;
};

} // namespace panorama

#endif // PANORAMA_RENDERPANEL_H
