//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//
#pragma once

#include "panorama/controls/panel2d.h"

//-----------------------------------------------------------------------------
// Purpose: Manager for Dota context menus
//-----------------------------------------------------------------------------
class CUI_ContextMenuManager : public panorama::CPanel2D
{
	DECLARE_PANEL2D( CUI_ContextMenuManager, panorama::CPanel2D );

public:
	CUI_ContextMenuManager( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CUI_ContextMenuManager();

	static CUI_ContextMenuManager *GetWindowContextMenuManager( panorama::IUIWindow *pWindow );
	static CUI_ContextMenuManager *GetPanelContextMenuManager( panorama::CPanel2D *pPanel );
	static CUI_ContextMenuManager *GetTopmostContextMenuManager();

	// events
	bool EventShowPlayerContextMenu( const panorama::CPanelPtr< panorama::IUIPanel > &panelPtr, uint64 unSteamID );

private:
	bool ShouldShowContextMenu( const panorama::CPanelPtr< panorama::IUIPanel > &panelPtr );
};