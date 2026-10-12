//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "cbase.h"
#include "csgo_submenu.h"
#include "panorama/uievents.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

REGISTER_PANEL2D_FACTORY(CCSGO_SubMenu, CSGOSubMenu)
REGISTER_PANEL2D(CCSGO_SubMenuContextMenu, CSGOSubMenuContextMenu)

CCSGO_SubMenu::CCSGO_SubMenu(CPanel2D *pParent, const char *pchID) : CPanel2D(pParent, pchID)
{
	m_pButton = NULL;
	SetAcceptsFocus(true);

	// Locate the root panel for this layout file
	m_pRootPanel = pParent;
	CPanoramaSymbol symLayout = pParent->GetLayoutFile();
	while (m_pRootPanel->GetParent() && (m_pRootPanel->GetParent()->GetLayoutFile() == symLayout))
	{
		m_pRootPanel = m_pRootPanel->GetParent();
	}

	bool bHasID = (pchID && pchID[0] != '\0');
	m_menuID = bHasID ? CFmtStr("%sSubMenu", pchID) : "";

	m_pContextMenuBody = new CPanel2D(m_pRootPanel, "ContextMenuBody");
	m_pContextMenuBody->AddClass("SubMenuItems");
	CPanel2D* pArrow = new CPanel2D(m_pContextMenuBody, "TopArrow");
	pArrow->AddClass("SubMenuArrow");
	m_pContextMenuBody->SetVisible(false);

	// register for events
	RegisterEventHandler(Activated(), this, &CCSGO_SubMenu::EventPanelActivated);
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CCSGO_SubMenu::~CCSGO_SubMenu()
{
	delete m_pContextMenuBody;
	m_pContextMenuBody = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Called after our panel has been created (and children added from layout file)
//-----------------------------------------------------------------------------
void CCSGO_SubMenu::OnInitializedFromLayout()
{
	// First child is the button controlling the submenu
	m_pButton = GetChild(0);

	// Remaining children are the submenu items - move to our separate ContextMenuBody panel 	
	while (GetChildCount() > 1)
	{
		CPanel2D *pChild = GetChild(1);
		pChild->SetParent(m_pContextMenuBody);
		pChild->AddClass("SubMenuItem");
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handles the panel activated event
//-----------------------------------------------------------------------------
bool CCSGO_SubMenu::EventPanelActivated(const CPanelPtr< IUIPanel > &pPanel, EPanelEventSource_t eSource)
{
	if (ToPanel2D(pPanel.Get()) != m_pButton)
		return false;

	if (m_pButton && m_pContextMenuBody)
	{
		CCSGO_SubMenuContextMenu* pContextMenu = new CCSGO_SubMenuContextMenu(m_pRootPanel, m_menuID.String(), this);

		// Add contents
		m_pContextMenuBody->SetVisible(true);
		m_pContextMenuBody->SetAcceptsFocus(true);
		m_pContextMenuBody->SetParent(pContextMenu);

		pContextMenu->SetMenuTarget(m_pButton->UIPanel());
		pContextMenu->SetVisible(true);
		pContextMenu->SetFocus();

		m_pButton->SetSelected(true);

		return true;
	}

	return false;
}

void CCSGO_SubMenu::SetButtonSelected(bool bSelected)
{
	if (m_pButton)
	{
		m_pButton->SetSelected(bSelected);
	}
}

CCSGO_SubMenuContextMenu::CCSGO_SubMenuContextMenu(CPanel2D *pParent, const char *pchID, CCSGO_SubMenu *pSubMenu) : CContextMenu(pParent, pchID, pSubMenu)
{
	m_pSubMenu = pSubMenu;
}

CCSGO_SubMenuContextMenu::~CCSGO_SubMenuContextMenu()
{
}


void CCSGO_SubMenuContextMenu::Close()
{
	if (m_pSubMenu)
	{
		m_pSubMenu->SetButtonSelected(false);
	}

	CPanel2D *pContextMenuBody = GetChild(0);
	if (pContextMenuBody)
	{
		pContextMenuBody->SetParent(GetParent());
		pContextMenuBody->SetVisible(false);
	}
	BaseClass::Close();
}