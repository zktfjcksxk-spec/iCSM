//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "cbase.h"
#include "csgo_popup_manager.h"

#include "panorama/popups/ui_popup_generic.h"
#include "panorama/uijsregistration.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


//-----------------------------------------------------------------------------
//
// CCSGO_PopupManager Panel / events
//
//-----------------------------------------------------------------------------

REGISTER_PANEL2D_FACTORY( CCSGO_PopupManager, CSGOPopupManager )


//-----------------------------------------------------------------------------
//
// CCSGO_PopupManager Methods
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
CCSGO_PopupManager::CCSGO_PopupManager( panorama::CPanel2D *pParent, const char *pchID )
:
	CUI_PopupManager( pParent, pchID )
{
	//RegisterForUnhandledEvent( UIShowPopupMessage(), this, &CCSGO_PopupManager::EventShowPopupMessage );
}

//-----------------------------------------------------------------------------
CCSGO_PopupManager::~CCSGO_PopupManager()
{

}