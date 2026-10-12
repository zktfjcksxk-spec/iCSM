//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: Display money balance and buy zone 
//
// SF differences :
//		* nothing working yet
//=============================================================================//

#include "cbase.h"
#include "csgo_hudautodisconnect.h"

#include "c_cs_player.h"

#include "panorama/uievents.h"
#include "panorama/controls/label.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


REGISTER_PANEL2D_FACTORY( CCSGO_HudAutoDisconnect, CSGOHudAutoDisconnect );

CCSGO_HudAutoDisconnect::CCSGO_HudAutoDisconnect( panorama::CPanel2D *pParent, const char *pchID ) :
	panorama::CPanel2D( pParent, pchID ),
	CPanoramaHudElement( "CCSGO_HudAutoDisconnect", this )
{
	RequireLoadLayout( "file://{resources}/layout/hud/hudautodisconnect.xml" );

	m_pTopLabel = panorama::panel_cast<panorama::CLabel *>( RequireChildInLayoutFile( "TopLabel" ) );
	m_pBottomLabel = panorama::panel_cast<panorama::CLabel *>( RequireChildInLayoutFile( "BottomLabel" ) );
	m_pTimerIcon = panorama::panel_cast<panorama::CImagePanel *>( RequireChildInLayoutFile( "TimerIcon" ) );

	SetVisible( false );
}


void CCSGO_HudAutoDisconnect::ProcessInput()
{
	static const panorama::CPanoramaSymbol k_symHidden( "hidden" );

	static ConVarRef cl_connection_trouble_info( "cl_connection_trouble_info" );
	if ( m_bActive && cl_connection_trouble_info.IsValid() && cl_connection_trouble_info.GetString()[0] )
	{
		//
		// See cl_main.cpp CL_Move
		// bool hasProblem = cl.m_NetChannel->IsTimingOut() && !demoplayer->IsPlayingBack() &&	cl.IsActive();
		//

		// hide these by default
		if ( m_pBottomLabel ) { m_pBottomLabel->SetHasClass( k_symHidden, true ); }
		if ( m_pTimerIcon ) { m_pTimerIcon->SetHasClass( k_symHidden, true ); }

		// Check for disconnect?
		float TimeoutValue = -1.0f, Percentage = -1.0f;

		if ( 1 == sscanf( cl_connection_trouble_info.GetString(), "disconnect(%f)", &TimeoutValue ) )
		{
			if ( TimeoutValue < 0 )
				TimeoutValue = 0;

			char cTimerStr[ 128 ];
			V_snprintf( cTimerStr, sizeof(cTimerStr), "%02d:%02d", Floor2Int( TimeoutValue / 60.f ), ( Floor2Int(TimeoutValue) % 60 ) );

			if ( m_pTopLabel ) { m_pTopLabel->SetText( "#SFUI_CONNWARNING_HEADER" ); }

			if ( m_pBottomLabel )
			{
				m_pBottomLabel->SetText( "#PANORAMA_CONNWARNING_BODY" );
				m_pBottomLabel->SetDialogVariable( "timer", cTimerStr );
				m_pBottomLabel->SetHasClass( k_symHidden, false );
			}

			if ( m_pTimerIcon ) { m_pTimerIcon->SetHasClass( k_symHidden, false ); }
		}
		else if ( 2 == sscanf( cl_connection_trouble_info.GetString(), "@%f:loss(%f)", &TimeoutValue, &Percentage ) )
		{
			if ( m_pTopLabel ) { m_pTopLabel->SetText( "#SFUI_CONNWARNING_Bandwidth_PacketLoss" ); }
		}
		else if ( 2 == sscanf( cl_connection_trouble_info.GetString(), "@%f:choke(%f)", &TimeoutValue, &Percentage ) )
		{
			if ( m_pTopLabel ) { m_pTopLabel->SetText( "#SFUI_CONNWARNING_Bandwidth_Choking" ); }
		}
		else
		{
			if ( m_pTopLabel ) { m_pTopLabel->SetText( "#SFUI_CONNWARNING_HEADER" ); }
		}
	}
}


bool CCSGO_HudAutoDisconnect::ShouldDraw()
{
	return Helper_ShouldInformPlayerAboutConnectionLossChoke();
}


void CCSGO_HudAutoDisconnect::SetActive( bool bActive )
{
	if ( m_bActive != bActive )
	{
		SetVisible( bActive );
	}

	CPanoramaHudElement::SetActive( bActive );
}

