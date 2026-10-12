//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "cbase.h"
#include "csgo_intromovie.h"

#include "engine/IEngineSound.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

REGISTER_PANEL2D_FACTORY( CCSGO_IntroMovie, CSGOIntroMovie )

DEFINE_PANORAMA_EVENT( CSGOShowIntroMovie );
DEFINE_PANORAMA_EVENT( CSGOHideIntroMovie );

using namespace panorama;

//-----------------------------------------------------------------------------
// Static data members
//-----------------------------------------------------------------------------
/*static*/ CCSGO_IntroMovie *CCSGO_IntroMovie::s_pIntroMovie = NULL;

CCSGO_IntroMovie::CCSGO_IntroMovie( CPanel2D *pParent, const char *pchID )
	: CUI_Root( pParent, pchID )
{
	Assert( s_pIntroMovie == NULL );
	s_pIntroMovie = this;

	RequireLoadLayout( "file://{resources}/layout/intromovie.xml" );

	SetAcceptsInput( true );
	SetInputNamespace( "CSGO_intromovie" );

	m_pSteamNotificationsPlaceholderPanel = RequireChildInLayoutFile( "SteamNotificationsPlaceholder" );

	// set up events
	RegisterEventHandler( CSGOHideIntroMovie(), this, &CCSGO_IntroMovie::EventHideIntroMovie );

	GameUI().RegisterGameUIStateListener( this );
	OnCSGOGameUIStateChange( CSGO_GAME_UI_STATE_INVALID, GameUI().GetGameUIState() );

}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CCSGO_IntroMovie::~CCSGO_IntroMovie()
{
	GameUI().UnregisterGameUIStateListener( this );

	Assert( s_pIntroMovie == this );
	s_pIntroMovie = NULL;
}

bool CCSGO_IntroMovie::EventHideIntroMovie( void )
{
	GameUI().ChangeGameUIState( CSGO_GAME_UI_STATE_MAINMENU );
	return true;
}

void CCSGO_IntroMovie::OnCSGOGameUIStateChange( CSGOGameUIState_t nOldState, CSGOGameUIState_t nNewState )
{
	bool bVisible = false;
	if ( nNewState == CSGO_GAME_UI_STATE_INTROMOVIE )
	{
		DispatchEvent( CSGOShowIntroMovie(), NULL );
		bVisible = true;
	}

	GetParentWindow()->SetVisible( bVisible );
}

