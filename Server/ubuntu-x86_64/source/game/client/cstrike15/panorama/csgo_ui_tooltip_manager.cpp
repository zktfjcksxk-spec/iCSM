//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "cbase.h"
#include "csgo_ui_tooltip_manager.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


//-----------------------------------------------------------------------------
//
// CCSGO_UI_TooltipManager Panel / events
//
//-----------------------------------------------------------------------------

REGISTER_PANEL2D_FACTORY( CCSGO_UI_TooltipManager, CSGOTooltipManager )


//-----------------------------------------------------------------------------
//
// CCSGO_UI_TooltipManager Methods
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
CCSGO_UI_TooltipManager::CCSGO_UI_TooltipManager( panorama::CPanel2D *pParent, const char *pchID )
:
	CUI_TooltipManager( pParent, pchID )
{
	
}

//-----------------------------------------------------------------------------
CCSGO_UI_TooltipManager::~CCSGO_UI_TooltipManager()
{

}