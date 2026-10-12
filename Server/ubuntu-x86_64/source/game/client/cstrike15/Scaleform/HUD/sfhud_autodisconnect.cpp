//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:  Displays HUD element to show we are having connectivity trouble
//
//=====================================================================================//

#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "hud_element_helper.h"
#include "scaleformui/scaleformui.h"
#include "sfhud_autodisconnect.h"
#include "hud_macros.h"
#include "view.h"

#include "inetchannelinfo.h"

#include "c_cs_player.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

DECLARE_HUDELEMENT( SFHudAutodisconnect );

SFUI_BEGIN_GAME_API_DEF
SFUI_END_GAME_API_DEF( SFHudAutodisconnect, Autodisconnect ); 

SFHudAutodisconnect::SFHudAutodisconnect( const char *value ) : SFHudFlashInterface( value ),
	m_sfuiControlBg( NULL ), m_sfuiControlTopLabel( NULL ), m_sfuiControlBottomLabel( NULL ), m_sfuiControlTimerLabel( NULL ), m_sfuiControlTimerIcon( NULL )
{
	// This HUD element should never be hidden, so do not call SetHiddenBits
}

SFHudAutodisconnect::~SFHudAutodisconnect()
{
}

void SFHudAutodisconnect::ShowPanel( bool bShow )
{
	if ( m_FlashAPI )
	{
		WITH_SLOT_LOCKED
		{
			if ( bShow )
				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "ShowPanel", NULL, 0 );
			else
				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "HidePanel", NULL, 0 );
		}
	}
}

void SFHudAutodisconnect::ProcessInput( void )
{
	static ConVarRef cl_connection_trouble_info( "cl_connection_trouble_info" );
	if ( FlashAPIIsValid() && m_bActive && cl_connection_trouble_info.IsValid() && cl_connection_trouble_info.GetString()[0] )
	{
		//
		// See cl_main.cpp CL_Move
		// bool hasProblem = cl.m_NetChannel->IsTimingOut() && !demoplayer->IsPlayingBack() &&	cl.IsActive();
		//

		// Check for disconnect?
		float TimeoutValue = -1.0f, Percentage = -1.0f;

		if ( 1 == sscanf( cl_connection_trouble_info.GetString(), "disconnect(%f)", &TimeoutValue ) )
		{
			if ( TimeoutValue < 0 )
				TimeoutValue = 0;

			char cTimerStr[ 128 ];
			V_snprintf( cTimerStr, sizeof(cTimerStr), "%02d:%02d", Floor2Int( TimeoutValue / 60.f ), ( Floor2Int(TimeoutValue) % 60 ) );

			WITH_SLOT_LOCKED
			{
				if ( m_sfuiControlTopLabel )		m_pScaleformUI->Value_SetText( m_sfuiControlTopLabel, "#SFUI_CONNWARNING_HEADER" );
				if ( m_sfuiControlBottomLabel )		m_pScaleformUI->Value_SetText( m_sfuiControlBottomLabel, "#SFUI_CONNWARNING_BODY" );
				if ( m_sfuiControlTimerLabel )		m_pScaleformUI->Value_SetText( m_sfuiControlTimerLabel, cTimerStr );

				if ( m_sfuiControlBg )				m_pScaleformUI->Value_SetVisible( m_sfuiControlBg, true );
				if ( m_sfuiControlTopLabel )		m_pScaleformUI->Value_SetVisible( m_sfuiControlTopLabel, true );
				if ( m_sfuiControlBottomLabel )		m_pScaleformUI->Value_SetVisible( m_sfuiControlBottomLabel, true );
				if ( m_sfuiControlTimerLabel )		m_pScaleformUI->Value_SetVisible( m_sfuiControlTimerLabel, true );
				if ( m_sfuiControlTimerIcon )		m_pScaleformUI->Value_SetVisible( m_sfuiControlTimerIcon, true );
			}
		}
		else if ( 2 == sscanf( cl_connection_trouble_info.GetString(), "@%f:loss(%f)", &TimeoutValue, &Percentage ) )
		{
			WITH_SLOT_LOCKED
			{
				if ( m_sfuiControlTopLabel )		m_pScaleformUI->Value_SetText( m_sfuiControlTopLabel, "#SFUI_CONNWARNING_Bandwidth_PacketLoss" );

				if ( m_sfuiControlBg )				m_pScaleformUI->Value_SetVisible( m_sfuiControlBg, false );
				if ( m_sfuiControlTopLabel )		m_pScaleformUI->Value_SetVisible( m_sfuiControlTopLabel, true );
				if ( m_sfuiControlBottomLabel )		m_pScaleformUI->Value_SetVisible( m_sfuiControlBottomLabel, false );
				if ( m_sfuiControlTimerLabel )		m_pScaleformUI->Value_SetVisible( m_sfuiControlTimerLabel, false );
				if ( m_sfuiControlTimerIcon )		m_pScaleformUI->Value_SetVisible( m_sfuiControlTimerIcon, false );
			}
		}
		else if ( 2 == sscanf( cl_connection_trouble_info.GetString(), "@%f:choke(%f)", &TimeoutValue, &Percentage ) )
		{
			WITH_SLOT_LOCKED
			{
				if ( m_sfuiControlTopLabel )		m_pScaleformUI->Value_SetText( m_sfuiControlTopLabel, "#SFUI_CONNWARNING_Bandwidth_Choking" );

				if ( m_sfuiControlBg )				m_pScaleformUI->Value_SetVisible( m_sfuiControlBg, false );
				if ( m_sfuiControlTopLabel )		m_pScaleformUI->Value_SetVisible( m_sfuiControlTopLabel, true );
				if ( m_sfuiControlBottomLabel )		m_pScaleformUI->Value_SetVisible( m_sfuiControlBottomLabel, false );
				if ( m_sfuiControlTimerLabel )		m_pScaleformUI->Value_SetVisible( m_sfuiControlTimerLabel, false );
				if ( m_sfuiControlTimerIcon )		m_pScaleformUI->Value_SetVisible( m_sfuiControlTimerIcon, false );
			}
		}
		else
		{
			WITH_SLOT_LOCKED
			{
				if ( m_sfuiControlTopLabel )		m_pScaleformUI->Value_SetText( m_sfuiControlTopLabel, "#SFUI_CONNWARNING_HEADER" );

				if ( m_sfuiControlBg )				m_pScaleformUI->Value_SetVisible( m_sfuiControlBg, false );
				if ( m_sfuiControlTopLabel )		m_pScaleformUI->Value_SetVisible( m_sfuiControlTopLabel, true );
				if ( m_sfuiControlBottomLabel )		m_pScaleformUI->Value_SetVisible( m_sfuiControlBottomLabel, false );
				if ( m_sfuiControlTimerLabel )		m_pScaleformUI->Value_SetVisible( m_sfuiControlTimerLabel, false );
				if ( m_sfuiControlTimerIcon )		m_pScaleformUI->Value_SetVisible( m_sfuiControlTimerIcon, false );
			}
		}
	}
}

void SFHudAutodisconnect::FlashReady( void )
{
	// Grab a pointer to the timeout text box
	if ( SFVALUE panelRoot = m_pScaleformUI->Value_GetMember( m_FlashAPI, "Panel" ) )
	{
		if ( SFVALUE panelAnim = m_pScaleformUI->Value_GetMember( panelRoot, "PanelAnim" ) )
		{
			m_sfuiControlBg = m_pScaleformUI->Value_GetMember( panelAnim, "AutoDisconnectBg" );

			if ( SFVALUE text = m_pScaleformUI->Value_GetMember( panelAnim, "Text" ) )
			{
				m_sfuiControlTopLabel = m_pScaleformUI->Value_GetMember( text, "AutoDisconnectText1" );
				m_sfuiControlBottomLabel = m_pScaleformUI->Value_GetMember( text, "AutoDisconnectText2" );
				m_sfuiControlTimerLabel = m_pScaleformUI->Value_GetMember( text, "TimerText" );
				m_sfuiControlTimerIcon = m_pScaleformUI->Value_GetMember( text, "TimerIcon" );

				SafeReleaseSFVALUE( text );
			}

			SafeReleaseSFVALUE( panelAnim );
		}

		SafeReleaseSFVALUE( panelRoot );
	}	
}

bool SFHudAutodisconnect::PreUnloadFlash( void )
{
	SafeReleaseSFVALUE( m_sfuiControlBg );
	SafeReleaseSFVALUE( m_sfuiControlTopLabel );
	SafeReleaseSFVALUE( m_sfuiControlBottomLabel );
	SafeReleaseSFVALUE( m_sfuiControlTimerLabel );
	SafeReleaseSFVALUE( m_sfuiControlTimerIcon );

	return true;
}

void SFHudAutodisconnect::LevelInit( void )
{
	if ( !FlashAPIIsValid() )
	{
		// We assume we are offline-only for splitscreen multiplayer, so we allow this to be displayed in the split screen slot like
		//	the rest of the HUD is.  If we support splitscreen online down the road, this should be moved into the fullscreen slot.
		SFUI_REQUEST_ELEMENT( SF_SS_SLOT( GET_ACTIVE_SPLITSCREEN_SLOT() ), g_pScaleformUI, SFHudAutodisconnect, this, Autodisconnect );
	}
	
	// When initially loaded, hide any previous message
	ShowPanel( false );
	m_bActive = false;
}

void SFHudAutodisconnect::LevelShutdown( void )
{
	if ( FlashAPIIsValid() )
	{
		RemoveFlashElement();
	}
}

void SFHudAutodisconnect::Reset( void )
{
	ShowPanel( false );
}

bool SFHudAutodisconnect::ShouldDraw( void )
{
	return Helper_ShouldInformPlayerAboutConnectionLossChoke();
}

void SFHudAutodisconnect::SetActive( bool bActive )
{
	if ( m_bActive != bActive )
	{
		ShowPanel( bActive );
	}

	CHudElement::SetActive( bActive );
}

