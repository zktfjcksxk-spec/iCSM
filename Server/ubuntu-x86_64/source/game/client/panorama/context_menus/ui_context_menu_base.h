//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//
#pragma once

#include "fileio.h"
#include "panorama/controls/panel2d.h"
#include "panorama/controls/contextmenu.h"

#ifndef CSGO_PORT
// In Source1 the pointers_to_members pragma is used within Scaleform and VGUI headers to ensure member function pointers
// are maximum size. This affects the panorama javascript bindings (see uijsregistration.h).
// We also add the pragma here so that we always assume the larger size pointers in source 1 for consistency, regardless
// of the order of includes. 
#define MEMBER_FUNCPTRS_MAXSIZE
#pragma pointers_to_members( full_generality, virtual_inheritance )
#endif

namespace panorama
{
	class CTopLevelWindow;
}

//-----------------------------------------------------------------------------
// Purpose: Base class for dota context menus
//-----------------------------------------------------------------------------
class CUI_ContextMenu_Base : public panorama::CContextMenu
{
	DECLARE_PANEL2D( CUI_ContextMenu_Base, panorama::CContextMenu );

public:
	CUI_ContextMenu_Base( panorama::CPanel2D *pParent, const char *pchName, CPanel2D *pEventParent );
	CUI_ContextMenu_Base( panorama::IUIWindow *pParent, const char *pchName, CPanel2D *pEventParent );
	virtual ~CUI_ContextMenu_Base();

	virtual void SetupJavascriptObjectTemplate() OVERRIDE;


protected:
	panorama::CPanel2D *GetContentsPanel() { return m_pContentsPanel; }

private:
	void Initialize();

	panorama::CPanel2D *m_pContentsPanel;
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
class CUI_ContextMenu_Script : public CUI_ContextMenu_Base
{
	DECLARE_PANEL2D( CUI_ContextMenu_Script, CUI_ContextMenu_Base );

public:
	CUI_ContextMenu_Script( CPanel2D *pParent, const char *pchName ); // NOTE: The provided parent doesn't become the actual parent panel!
};

