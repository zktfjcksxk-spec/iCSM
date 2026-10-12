//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//
#pragma once

#include "panorama/controls/contextmenu.h"

//-----------------------------------------------------------------------------
// Purpose: CSGO Sub Menu
// CSGO submenus are constructed in the XML layout as a simple list of buttons
// The first button is the button controlling the display of the submenu
// The remaining buttons are moved to a contextmenu panel which is displayed 
// when the control button is activated.
// When the submenu is displayed, the control button is set to a 'selected' state.
//-----------------------------------------------------------------------------
class CCSGO_SubMenu : public panorama::CPanel2D
{
	DECLARE_PANEL2D(CCSGO_SubMenu, panorama::CPanel2D);

public:
	CCSGO_SubMenu(panorama::CPanel2D *pParent, const char *pchID);
	virtual ~CCSGO_SubMenu();
	void SetButtonSelected(bool bSelected);

protected:
	bool EventPanelActivated(const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, panorama::EPanelEventSource_t eSource);
	virtual void OnInitializedFromLayout();

private:

	panorama::CPanel2D *m_pButton;
	panorama::CPanel2D * m_pRootPanel;
	CUtlString m_menuID;
	panorama::CPanel2D * m_pContextMenuBody;

};

class CCSGO_SubMenuContextMenu : public panorama::CContextMenu
{
	DECLARE_PANEL2D(CCSGO_SubMenuContextMenu, panorama::CContextMenu);

public:
	CCSGO_SubMenuContextMenu(panorama::CPanel2D *pParent, const char *pchName, CCSGO_SubMenu *pSubMenu);
	virtual ~CCSGO_SubMenuContextMenu();
	virtual void Close();

private:
	CCSGO_SubMenu* m_pSubMenu;
};

