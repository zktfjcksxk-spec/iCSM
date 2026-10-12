//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef UI_CONTEXT_MENU_CUSTOM_LAYOUT_H
#define UI_CONTEXT_MENU_CUSTOM_LAYOUT_H
#ifdef _WIN32
#pragma once
#endif

#include "ui_context_menu_base.h"
#include "panorama/ui_custom_layout.h"

//-----------------------------------------------------------------------------
// Purpose: Helper class for custom layout context menus 
//-----------------------------------------------------------------------------
class CUI_ContextMenu_CustomLayout : public CUI_ContextMenu_Base
{
	DECLARE_PANEL2D( CUI_ContextMenu_CustomLayout, CUI_ContextMenu_Base );

public:
	CUI_ContextMenu_CustomLayout( panorama::CPanel2D *pParent, const char *pchName, panorama::CPanel2D *pEventParent );
	virtual ~CUI_ContextMenu_CustomLayout();

	void Init( const char *pszLayout, const char *pszParams );

private:
	CUI_CustomLayoutHandler m_customLayoutHandler;
};

#endif	// UI_CONTEXT_MENU_CUSTOM_LAYOUT_H