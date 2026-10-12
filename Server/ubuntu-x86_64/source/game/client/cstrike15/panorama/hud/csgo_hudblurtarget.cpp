//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "cbase.h"
#include "csgo_hudblurtarget.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

REGISTER_PANEL2D_FACTORY(CCSGO_HudBlurTarget, CSGOHudBlurTarget);

using namespace panorama;

CCSGO_HudBlurTarget::CCSGO_HudBlurTarget(CPanel2D *pParent, const char *pchID)
	: CPanoramaHudElement("CCSGO_HudBlurTarget", this),
	CCSGO_BlurTarget( pParent, pchID )
{
	m_bWantLateUpdate = true;
	// No alternate ticks for the HUDBlurTarget to keep it in sync (same frame vis) with the blur rects
	SetForceBuildPaintCmdCache( false );
	SetAllowAlternateTicks( false );

}

CCSGO_HudBlurTarget::~CCSGO_HudBlurTarget()
{
}

void CCSGO_HudBlurTarget::Think(void)
{
	GrabRects();

	bool bVisible = false;

	FOR_EACH_VEC( m_vecBlurRects, i )
	{
		IUIPanel *pTarget = m_vecBlurRects[ i ].Get();
		if(pTarget && pTarget->BIsVisible())
		{
			bVisible = true;
			break;
		}
	}

	SetVisible( bVisible );
}

