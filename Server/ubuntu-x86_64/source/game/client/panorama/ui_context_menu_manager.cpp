//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "cbase.h"
#include "ui_context_menu_manager.h"
#include "panorama/ui_root.h"

#if DOTA_DLL
#include "context_menus/dota_db_context_menu_player.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

REGISTER_PANEL2D_FACTORY( CUI_ContextMenuManager, ContextMenuManager )

using namespace panorama;

CUI_ContextMenuManager::CUI_ContextMenuManager( CPanel2D *pParent, const char *pchID )
	: CPanel2D( pParent, pchID )
{
	SetHitTestEnabled( false );

#if DOTA_DLL
	RegisterForUnhandledEvent( DOTAShowPlayerContextMenu(), this, &CUI_ContextMenuManager::EventShowPlayerContextMenu );
#endif
}

CUI_ContextMenuManager::~CUI_ContextMenuManager()
{
#if DOTA_DLL
	UnregisterForUnhandledEvent( DOTAShowPlayerContextMenu(), this, &CUI_ContextMenuManager::EventShowPlayerContextMenu );
#endif
}

/*static*/ CUI_ContextMenuManager *CUI_ContextMenuManager::GetWindowContextMenuManager( IUIWindow *pWindow )
{
	CUI_Root *pRoot = CUI_Root::GetRootForWindow( pWindow );
	if ( !pRoot )
		return nullptr;

	return pRoot->GetContextMenuManager();
}

CUI_ContextMenuManager *CUI_ContextMenuManager::GetPanelContextMenuManager( panorama::CPanel2D *pPanel )
{
	if ( !pPanel )
		return nullptr;

	return GetWindowContextMenuManager( pPanel->GetParentWindow() );
}

CUI_ContextMenuManager *CUI_ContextMenuManager::GetTopmostContextMenuManager()
{
	int nRootCount = CUI_Root::GetRootCount();
	for ( int i = nRootCount - 1; i >= 0; --i )
	{
		CUI_Root *pRoot = CUI_Root::GetRootByIndex( i );
		if ( !pRoot->GetParentWindow()->BIsVisible() )
			continue;

		if ( !pRoot->GetContextMenuManager() )
			continue;

		return pRoot->GetContextMenuManager();
	}

	return nullptr;
}

bool CUI_ContextMenuManager::ShouldShowContextMenu( const panorama::CPanelPtr< panorama::IUIPanel > &panelPtr )
{
	IUIWindow *pWindow = GetParentWindow();
	if ( !pWindow )
		return false;

	if ( !pWindow->BIsVisible() )
		return false;

	if ( panelPtr.Get() && pWindow != panelPtr->GetParentWindow() )
		return false;

	return true;
}

bool CUI_ContextMenuManager::EventShowPlayerContextMenu( const CPanelPtr< IUIPanel > &panelPtr, uint64 unSteamID )
{
	if ( !ShouldShowContextMenu( panelPtr ) )
		return false;

	CSteamID steamID( unSteamID );
	if ( !steamID.IsValid() )
		return false;

#if DOTA_DLL

	CDOTA_DB_ContextMenu_Player *pContextMenu = new CDOTA_DB_ContextMenu_Player( this, "PersonaContextMenu", ToPanel2D( panelPtr.Get() ) );
	pContextMenu->SetPlayerOptions( steamID, DOTA_PLAYER_MENU_OPTIONS_ALL );
	pContextMenu->SetMenuTarget( panelPtr );
	pContextMenu->SetVisible( true );
	pContextMenu->SetFocus();

#endif

	return true;
}

