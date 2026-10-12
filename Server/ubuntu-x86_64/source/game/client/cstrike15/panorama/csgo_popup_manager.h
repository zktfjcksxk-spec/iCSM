//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//
#pragma once

#include "panorama/ui_popup_manager.h"


//-----------------------------------------------------------------------------
// Purpose: CSGO popup manager
//-----------------------------------------------------------------------------
class CCSGO_PopupManager : public CUI_PopupManager
{
	DECLARE_PANEL2D( CCSGO_PopupManager, CUI_PopupManager );

public:
	CCSGO_PopupManager( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CCSGO_PopupManager();

private:

};