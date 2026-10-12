//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "cbase.h"
#include "csgo_globalpopups.h"

#include "csgo_popup_manager.h"
#include "csgo_ui_tooltip_manager.h"
#include "panorama/ui_context_menu_manager.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


REGISTER_PANEL2D_FACTORY( CCSGO_GlobalPopups, CSGOGlobalPopups )


// ---------------------------------------------------------------------------- -
// Static data members
//-----------------------------------------------------------------------------
/*static*/ CCSGO_GlobalPopups *CCSGO_GlobalPopups::s_pPopupsUiRoot = nullptr;


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CCSGO_GlobalPopups::CCSGO_GlobalPopups( CPanel2D *pParent, const char *pchID )
:
	CUI_Root( pParent, pchID )
{
	Assert( s_pPopupsUiRoot == NULL );
	s_pPopupsUiRoot = this;

	RequireLoadLayout( "file://{resources}/layout/globalpopups.xml" );

	// Tell the root about these controls
	SetPopupManager( panorama::panel_cast< CCSGO_PopupManager * >( RequireChildInLayoutFile( "PopupManager" ), true ) );
	SetTooltipManager( panorama::panel_cast< CCSGO_UI_TooltipManager * >( RequireChildInLayoutFile( "TooltipManager" ), true ) );
	SetContextMenuManager( panorama::panel_cast< CUI_ContextMenuManager * >( RequireChildInLayoutFile( "ContextMenuManager" ), true ) );

	SetAcceptsInput( true );
	SetAcceptsFocus( true );
	SetInputNamespace( "CSGO_popups" );

	RegisterEventHandler( UIPopupManagerVisibilityChanged(), this, &CCSGO_GlobalPopups::EventUIPopupManagerVisibilityChanged );

	GetParentWindow()->SetForceConsumeKBAndMouseInputEvents( true );	// Stops input event propagating to another top level window
																		// Only occasionally do we have mote than one top level
																		// visible (eg main menu & global popups)
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CCSGO_GlobalPopups::~CCSGO_GlobalPopups()
{
	UnregisterEventHandler( UIPopupManagerVisibilityChanged(), this, &CCSGO_GlobalPopups::EventUIPopupManagerVisibilityChanged );

	Assert( s_pPopupsUiRoot == this );
	s_pPopupsUiRoot = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
bool CCSGO_GlobalPopups::EventUIPopupManagerVisibilityChanged( const panorama::CPanelPtr< panorama::IUIPanel > &panelPtr, bool bVisible )
{
	// Top level window only visible if a popup is visible
	GetParentWindow()->SetVisible( bVisible );

	return true;
}