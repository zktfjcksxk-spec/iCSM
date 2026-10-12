//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Hint text: panel for displaying hint text sent from the server (through CEnvHudHint or CHintMessageQueue systems) or client (CCenterPrint)
//
//=============================================================================//

#include "cbase.h"
#include "csgo_hudhinttext.h"
#include "panorama/uievents.h"
#include "hud_macros.h"
#include "text_message.h"
#include "uicomponents/uicomponent_gamestate.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

extern ConVar scr_centertime;
extern ConVar cl_draw_only_deathnotices;

REGISTER_PANEL2D_FACTORY( CCSGO_HudHintText, CSGOHudHintText );

DECLARE_HUD_MESSAGE( CCSGO_HudHintText, HintText );
DECLARE_HUD_MESSAGE( CCSGO_HudHintText, KeyHintText );

DEFINE_PANORAMA_EVENT_DOC( ShowCenterPrintText, "utf8 message string", "Display the string in the center of the hud." );

CCSGO_HudHintText::CCSGO_HudHintText( CPanel2D *pParent, const char *pchID )
	: CPanoramaHudElement( "CCSGO_HudHintText", this ),
	CPanel2D( pParent, pchID ),
	m_curHintType( CCenterPrint::k_EPriority_None )
{
	RequireLoadLayout( "file://{resources}/layout/hud/hudhinttext.xml" );
	m_pHintIcon			= panel_cast< CImagePanel*> ( FindChildInLayoutFile( "HintIcon" ) );
	m_pHintLabel		= panel_cast< CLabel* > ( FindChildInLayoutFile( "HintLabel" ) );

	if ( GameUI().IsPanoramaEnabled() )
	{
		HOOK_HUD_MESSAGE( CCSGO_HudHintText, HintText );
		HOOK_HUD_MESSAGE( CCSGO_HudHintText, KeyHintText );

		RegisterForUnhandledEvent( ShowCenterPrintText(), this, &CCSGO_HudHintText::SetHintText );
		RegisterForUnhandledEvent( GameState_OnMatchStart(), this, &CCSGO_HudHintText::EventOnMatchStart );
		RegisterForUnhandledEvent( PanoramaGameTimeJumpEvent(), this, &CCSGO_HudHintText::OnPanoramaGameTimeJumpEvent );
	}
}

CCSGO_HudHintText::~CCSGO_HudHintText()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCSGO_HudHintText::ShouldDraw( void )
{
	if ( cl_draw_only_deathnotices.GetBool() )
		return false;

	return CHudElement::ShouldDraw();
}

void CCSGO_HudHintText::Think( void )
{
	if ( m_hintDisplayTime.HasStarted() && m_hintDisplayTime.IsElapsed() )
	{
		SetShown( false );
	}
}

bool CCSGO_HudHintText::MsgFunc_HintText( const CCSUsrMsg_HintText &msg )
{
	// hudmessagetext system maps what up what the server sends to an actual loc token in csgo_<language>.txt... 
	SetHintText( hudtextmessage->LookupString( msg.text().c_str() ), CCenterPrint::k_EPriority_Low );
	return true;
}

bool CCSGO_HudHintText::MsgFunc_KeyHintText( const CCSUsrMsg_KeyHintText &msg )
{
	//NOTE: Only using first hint in the list which is the same behavior as sfhudinfopanel
	SetHintText( hudtextmessage->LookupString( msg.hints(0).c_str()), CCenterPrint::k_EPriority_Low );
	return true;
}

void CCSGO_HudHintText::SetShown( bool bShown )
{
	if ( bShown == true && cl_draw_only_deathnotices.GetBool() )
		return;

	SetHasClass( "hud-hint--visible", bShown );

	if ( !bShown )
	{
		m_curHintType = CCenterPrint::k_EPriority_None;
		m_hintDisplayTime.Invalidate();
	}

}

bool CCSGO_HudHintText::OnPanoramaGameTimeJumpEvent( float flTimeJump )
{
	SetShown( false );

	return false;
}

bool CCSGO_HudHintText::SetHintText( const char* szHintText, CCenterPrint::EPriority eType /*= CCenterPrint::k_EPriority_Low*/ )
{
	if ( !szHintText || StringIsEmpty( szHintText ) || cl_draw_only_deathnotices.GetBool() )
	{
		SetShown( false );
		return true;
	}

	// Ignore display requests from lower priority hint types. This is to match behavior of sfhudinfopanel. 
	if ( m_curHintType > eType )
	{
		return true;
	}

	SetShown( true );
	m_hintDisplayTime.Start( scr_centertime.GetFloat() );
	m_curHintType = eType;

	if ( m_pHintLabel.Get() )
	{
		m_pHintLabel->SetText( szHintText );
	}

	static const CPanoramaSymbol k_symPriorityHint( "hud-hint--priority" );
	SetHasClass( k_symPriorityHint, eType == CCenterPrint::k_EPriority_High );

	return true;
}

bool CCSGO_HudHintText::EventOnMatchStart()
{
	SetShown( false );
	return false;
}
