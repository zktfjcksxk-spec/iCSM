//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "cbase.h"
#include "ui_context_menu_base.h"
#include "panorama/ui_context_menu_manager.h"
#include "panorama/uijsregistration.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

REGISTER_PANEL2D( CUI_ContextMenu_Base, ContextMenuBase )

using namespace panorama;

CUI_ContextMenu_Base::CUI_ContextMenu_Base( CPanel2D *pParent, const char *pchName, CPanel2D *pEventParent )
	: CContextMenu( pParent, pchName, pEventParent )
{
	Initialize();
}

CUI_ContextMenu_Base::CUI_ContextMenu_Base( IUIWindow *pParent, const char *pchName, CPanel2D *pEventParent )
	: CContextMenu( pParent, pchName, pEventParent )
{
	Initialize();
}

void CUI_ContextMenu_Base::Initialize()
{
	DbgVerify( BLoadLayout( "file://{resources}/layout/context_menus/context_menu_base.xml" ) );
	m_pContentsPanel = FindChildInLayoutFile( "Contents" );
}

CUI_ContextMenu_Base::~CUI_ContextMenu_Base()
{
}

void CUI_ContextMenu_Base::SetupJavascriptObjectTemplate()
{
	BaseClass::SetupJavascriptObjectTemplate();

	panorama::RegisterJSMethod( "GetContentsPanel", PANORAMA_DELEGATE( &CUI_ContextMenu_Base::GetContentsPanel ) );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
REGISTER_PANEL2D_FACTORY( CUI_ContextMenu_Script, ContextMenuScript )

CUI_ContextMenu_Script::CUI_ContextMenu_Script( CPanel2D *pParent, const char *pchName )
	: // NOTE: the parent isn't ACTUALLY the parent
	CUI_ContextMenu_Base( CUI_ContextMenuManager::GetPanelContextMenuManager( pParent ), pchName, pParent )
{
}

