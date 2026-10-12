//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx_client.h"
#include "panorama/controls/source2/renderpanel.h"
#include "panorama/layout/csshelpers.h"

#include "rendersystem/irenderdevice.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

REGISTER_PANEL2D( CRenderPanel, RenderPanel );


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CRenderPanel::CRenderPanel( CPanel2D *parent, const char * pchPanelID, uint32 ePanelFlags /* = 0 */ )
	: CPanel2D( parent, pchPanelID, ePanelFlags )
{
	m_pRenderCallback = NULL;
	m_eFlags = k_ERenderCallbackFlagsDefault;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CRenderPanel::~CRenderPanel()
{
	if ( m_pRenderCallback )
	{
        CRenderThreadCallback *pOldRenderCallback = m_pRenderCallback;
		m_pRenderCallback = NULL;
		m_eFlags = k_ERenderCallbackFlagsDefault;
		pOldRenderCallback->Release();
	}
}


void CRenderPanel::SetRenderThreadCallback( CRenderThreadCallback *pRenderCallback, ERenderCallbackFlags eFlags ) 
{
	if ( pRenderCallback )
		pRenderCallback->AddRef();

    CRenderThreadCallback *pOldRenderCallback = m_pRenderCallback;
	ERenderCallbackFlags eOldFlags = m_eFlags;
	m_pRenderCallback = pRenderCallback;
	m_eFlags = eFlags;

	if ( pOldRenderCallback )
		pOldRenderCallback->Release();

	if ( pOldRenderCallback != m_pRenderCallback ||
		 eOldFlags != m_eFlags )
	{
		SetRepaint( k_EPanelRepaintFull );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Paint
//-----------------------------------------------------------------------------
void CRenderPanel::Paint()
{
	BaseClass::Paint();

	if ( !m_pRenderCallback )
		return;

	if( !BIsVisible() || BIsTransparent() )
		return;

	ERenderCallbackFlags eFlags = m_eFlags;
	if ( ( eFlags & ( k_ERenderCallbackFlagsAlwaysRepaint | k_ERenderCallbackFlagsManualRepaint ) ) == 0 )
	{
		if ( BShouldAlwaysRepaint() )
		{
			eFlags = (ERenderCallbackFlags)( eFlags | k_ERenderCallbackFlagsAlwaysRepaint );
		}
	}
	
	float flWidth = UIPanel()->GetActualRenderWidth();
	float flHeight = UIPanel()->GetActualRenderHeight();

	float flLeft, flTop, flRight, flBottom;
	AccessStyle()->GetContentInset( flWidth, flHeight, true, flLeft, flTop, flRight, flBottom );
	UIPanel()->UIRenderEngine()->RequestRenderCallback( m_pRenderCallback,
		UIPanel()->GetActualXOffset(), 
		UIPanel()->GetActualYOffset(), 
		UIPanel()->GetActualXOffset() + flWidth, 
		UIPanel()->GetActualYOffset() + flHeight,
		flLeft, flRight, flTop, flBottom, eFlags, m_pPanelRT );

	if ( ( eFlags & k_ERenderCallbackFlagsAlwaysRepaint ) != 0 )
	{
		SetRepaint( k_EPanelRepaintFull );
	}
}
