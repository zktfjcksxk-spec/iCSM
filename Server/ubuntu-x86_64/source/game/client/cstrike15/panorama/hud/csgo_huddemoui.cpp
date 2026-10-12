//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: Panorama replacement for vgui demo ui
//
//=============================================================================//

#include "cbase.h"
#include "csgo_huddemoui.h"
#include "IGameUIFuncs.h"
#include "clientmode_csnormal.h" // CSGOFrameUpdate

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

REGISTER_PANEL2D_FACTORY( CCSGO_HudDemoPlayback, CSGOHudDemoPlayback );

DEFINE_PANORAMA_EVENT( DemoToggleUI );
DEFINE_PANORAMA_EVENT_DOC( DemoPlaybackControl, "string,float", "Control demo playback" );


CCSGO_HudDemoPlayback::CCSGO_HudDemoPlayback( panorama::CPanel2D* pParent, const char* pchID )
	: BaseClass( pParent, pchID )
{
	RequireLoadLayout( "file://{resources}/layout/hud/huddemoplayback.xml" );

	RegisterEventHandler( DemoPlaybackControl(), this, &CCSGO_HudDemoPlayback::HandleDemoPlaybackControl );

	RegisterForUnhandledEvent( DemoToggleUI(), this, &CCSGO_HudDemoPlayback::HandleDemoToggleUI );
	RegisterForUnhandledEvent( CSGOFrameUpdate(), this, &CCSGO_HudDemoPlayback::HandleFrameUpdate );

	// NOTE: We need mouse input to work but we don't capture mouse input; for now we leave that to other systems (e.g. CCSGO_HudSpectator)
}

CCSGO_HudDemoPlayback::~CCSGO_HudDemoPlayback()
{
}

void CCSGO_HudDemoPlayback::OnLayoutReloaded()
{
	BaseClass::OnLayoutReloaded();
}

bool CCSGO_HudDemoPlayback::HandleDemoToggleUI()
{
	if ( !engine->IsPlayingDemo() )
		return true;

	ToggleClass( "demoplayback--active" );
	bool bActive = BHasClass( "demoplayback--active" );

	CCSGO_Hud* pHud = CCSGO_Hud::GetInstance();
	panorama::CPanel2D *pHudTopCenter = pHud ? pHud->FindChildTraverse( "HudTopCenter" ) : nullptr;

	// Offset HUD top center because we take up some space
	if ( pHudTopCenter )
	{
		panorama::CUILength x, y, z;
		pHudTopCenter->GetPosition( x, y, z );
		pHudTopCenter->SetPosition( x, panorama::CUILength( bActive ? 50 : 0, panorama::CUILength::k_EUILengthLength ), z );
	}

	return false;
}

bool CCSGO_HudDemoPlayback::HandleDemoPlaybackControl( const char* szControlType, float flControlAmount )
{
	if ( !engine->IsPlayingDemo() )
		return true;

	if ( !V_stricmp( szControlType, "Play" ) )
	{
		engine->ExecuteClientCmd( CFmtStr( "demo_timescale %f", flControlAmount ) );
	}
	else if ( !V_stricmp( szControlType, "Round" ) )
	{
		int nCount = ( int )flControlAmount;
		const char* pszEventName = "round_start";
		bool bForward = nCount >= 0;
		if ( nCount < 0 )
			nCount = -nCount;

		int nCurTick = engine->GetDemoPlaybackTick();
		int nGoalTick = nCurTick;
		for ( int iCount = 0; iCount < nCount; ++iCount )
		{
			int nTargetTick = bForward ? engine->DemoFindNextImportantTick( pszEventName, nGoalTick ) : engine->DemoFindPrevImportantTick( pszEventName, nGoalTick );
			if ( nTargetTick < 0 )
			{
				if ( !bForward )
					nGoalTick = 0;
				break;
			}

			nGoalTick = nTargetTick;
		}

		if ( nGoalTick != nCurTick )
			engine->ExecuteClientCmd( CFmtStr( "demo_gototick %d", nGoalTick ) );
	}

	return true;
}

bool CCSGO_HudDemoPlayback::HandleFrameUpdate()
{
	if ( !BHasClass( "demoplayback--active" ) )
		return false;

	int iTick = engine->GetDemoPlaybackTick();
	int nTicks = engine->GetDemoPlaybackTotalTicks();

	float curTime = ( iTick + gpGlobals->interpolation_amount ) * gpGlobals->interval_per_tick;
	float totalTime = nTicks * gpGlobals->interval_per_tick;

	int nMinutes, nSeconds, nThousandths;

	// Figure out number of digits required
	nMinutes = Floor2Int( totalTime / 60.0f );
	nSeconds = Floor2Int( totalTime - nMinutes * 60 );
	nThousandths = Floor2Int( 1000 * ( totalTime - nMinutes * 60 - nSeconds ) );

	int nDigits = 0;
	for ( int nMinutesTmp = nMinutes; nMinutesTmp != 0; nMinutesTmp /= 10 )
	{
		nDigits++;
	}
	nDigits = MAX( nDigits, 1 );

	SetDialogVariable( "demo-time-max", CFmtStr( "%0*d:%02d.%03d", nDigits, nMinutes, nSeconds, nThousandths ) );

	nMinutes = Floor2Int( curTime / 60.0f );
	nSeconds = Floor2Int( curTime - nMinutes * 60 );
	nThousandths = Floor2Int( 1000 * ( curTime - nMinutes * 60 - nSeconds ) );

	SetDialogVariable( "demo-time", CFmtStr( "%0*d:%02d.%03d", nDigits, nMinutes, nSeconds, nThousandths ) );

	return false;
}
