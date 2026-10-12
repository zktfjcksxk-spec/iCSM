//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//
#pragma once

#include "panorama/ui_tooltip_manager.h"

//-----------------------------------------------------------------------------
// Purpose: CSGO tooltip manager used to handled csgo specific tooltip events
//-----------------------------------------------------------------------------
class CCSGO_UI_TooltipManager : public CUI_TooltipManager
{
	DECLARE_PANEL2D( CCSGO_UI_TooltipManager, CUI_TooltipManager );

public:
	CCSGO_UI_TooltipManager( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CCSGO_UI_TooltipManager();
};