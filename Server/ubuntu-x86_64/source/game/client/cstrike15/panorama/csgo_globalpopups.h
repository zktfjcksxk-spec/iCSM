//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: Global Popups top level window
//			This top level window should always be visible so that popups can 
//			survive going from in-game to main menu for example.
//=============================================================================//
#pragma once

#include "panorama/ui_root.h"


class CCSGO_GlobalPopups : public CUI_Root
{
	DECLARE_PANEL2D( CCSGO_GlobalPopups, CUI_Root );

public:

	CCSGO_GlobalPopups( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CCSGO_GlobalPopups();

	static CCSGO_GlobalPopups *GetInstance() { return s_pPopupsUiRoot; }

private:

	static CCSGO_GlobalPopups *s_pPopupsUiRoot;

	bool EventUIPopupManagerVisibilityChanged( const panorama::CPanelPtr< panorama::IUIPanel > &panelPtr, bool bVisible );
};