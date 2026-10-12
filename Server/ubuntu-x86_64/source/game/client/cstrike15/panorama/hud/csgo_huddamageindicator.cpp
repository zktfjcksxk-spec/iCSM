//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//
// SF Differences:
//		* Missing additive blending 
//		* Damage segment currently png -> change to vtf or svg ?
//
//=============================================================================//

#include "cbase.h"
#include "csgo_huddamageindicator.h"

#include "hud_macros.h"
#include "hltvcamera.h"
#include "view.h"
#include "inputsystem/iinputsystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


REGISTER_PANEL2D_FACTORY( CCSGO_HudDamageIndicator, CSGOHudDamageIndicator );

DECLARE_HUD_MESSAGE( CCSGO_HudDamageIndicator, Damage );


extern bool IsTakingAFreezecamScreenshot();		// TODO Currently defined in sfhudfreezepanel.cpp
extern ConVar cl_draw_only_deathnotices;
extern ConVar cl_drawhud;


static float s_flCloseDamageDistance = 50.f;		// (in world units) - if damage received is closer than this to player, all directions light up
static float s_flDirectionDotTolerance = 0.3f;		// incoming dmg direction dot product must be > this value in order for damage to be "from" this direction
static float s_flStartFadeThreshold = 0.4f;			// scale at which the directional dmg indicator begins to auto-fade out
static float s_flFadeRateAboveThreshold = 1.f;	
static float s_flFadeRateBelowThreshold = 2.f;	


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCSGO_HudDamageIndicator::CCSGO_HudDamageIndicator( panorama::CPanel2D *pParent, const char *pchID )
:
	panorama::CPanel2D( pParent, pchID ),
	CPanoramaHudElement( "CCSGO_HudDamageIndicator", this ),
	m_flLastFrameTime( 0.0f )
{
	SetHiddenBits( HIDEHUD_HEALTH );

	RequireLoadLayout( "file://{resources}/layout/hud/huddamageindicator.xml" );

	m_pDamagePanel = RequireChildInLayoutFile( "Damage" );
	m_pDamageTopPanel = RequireChildInLayoutFile( "DamageTop" );
	m_pDamageRightPanel = RequireChildInLayoutFile( "DamageRight" );
	m_pDamageBottomPanel = RequireChildInLayoutFile( "DamageBottom" );
	m_pDamageLeftPanel = RequireChildInLayoutFile( "DamageLeft" );

	ResetData();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCSGO_HudDamageIndicator::~CCSGO_HudDamageIndicator()
{

}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudDamageIndicator::ResetData()
{
	m_flAttackFront = 0.0f;
	m_flAttackRear = 0.0f;
	m_flAttackLeft = 0.0f;
	m_flAttackRight = 0.0f;

	m_pDamageTopPanel->SetOpacity( 0.0f );
	m_pDamageRightPanel->SetOpacity( 0.0f );
	m_pDamageBottomPanel->SetOpacity( 0.0f );
	m_pDamageLeftPanel->SetOpacity( 0.0f );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudDamageIndicator::LevelInit( void )
{
	// When initially loaded, hide all indicators
	ShowPanel( false );

	HOOK_HUD_MESSAGE( CCSGO_HudDamageIndicator, Damage );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudDamageIndicator::LevelShutdown( void )
{
	m_UMCMsgDamage.Unbind();

	ShowPanel( false );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
static void UpdateDamageIndicator( panorama::CPanel2D *pPanel, float &flValue, float flFrameTime )
{
	if ( flValue > 0.0 )
	{
		float flRate = ( flValue > s_flStartFadeThreshold ) ? s_flFadeRateAboveThreshold : s_flFadeRateBelowThreshold;
		float flFade = flFrameTime * flRate;
		flValue = Max<float>( 0.0, flValue - flFade );
		flValue = Min<float>( 1.0, flValue );

		pPanel->SetOpacity( flValue );
	}
}

void CCSGO_HudDamageIndicator::ProcessInput( void )
{
	float flFrameTime = gpGlobals->curtime - m_flLastFrameTime;

	UpdateDamageIndicator( m_pDamageTopPanel, m_flAttackFront, flFrameTime );
	UpdateDamageIndicator( m_pDamageRightPanel, m_flAttackRight, flFrameTime );
	UpdateDamageIndicator( m_pDamageBottomPanel, m_flAttackRear, flFrameTime );
	UpdateDamageIndicator( m_pDamageLeftPanel, m_flAttackLeft, flFrameTime );

	m_flLastFrameTime = gpGlobals->curtime;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCSGO_HudDamageIndicator::ShouldDraw( void )
{
	if ( IsTakingAFreezecamScreenshot() )
		return false;

	return cl_drawhud.GetBool() && cl_draw_only_deathnotices.GetBool() == false && CHudElement::ShouldDraw();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudDamageIndicator::SetActive( bool bActive )
{
	if ( bActive != m_bActive )
	{
		ShowPanel( bActive );
	}

	CHudElement::SetActive( bActive );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudDamageIndicator::ShowPanel( bool bShow )
{
	static panorama::CPanoramaSymbol k_symDamageHidden( "Damage--Hidden" );

	m_pDamagePanel->SetHasClass( k_symDamageHidden, !bShow );

	ResetData();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCSGO_HudDamageIndicator::MsgFunc_Damage( const CCSUsrMsg_Damage &msg )
{
	C_BasePlayer *pVictimPlayer = NULL;
	if ( g_bEngineIsHLTV )
	{
		// Only show damage indicator for the player we are currently observing.
		if ( HLTVCamera()->GetMode() != OBS_MODE_IN_EYE )
			return true;

		C_BaseEntity* pTarget = HLTVCamera()->GetPrimaryTarget();
		if ( !pTarget || !pTarget->IsPlayer() || pTarget->entindex() != msg.victim_entindex() )
			return true;

		// This cast is safe because pTarget->IsPlayer() returned true above
		pVictimPlayer = static_cast<C_BasePlayer*>( pTarget );
	}
	else
	{
		Assert( C_BasePlayer::GetLocalPlayer()->entindex() == msg.victim_entindex() );
		pVictimPlayer = C_BasePlayer::GetLocalPlayer();
	}


	int damageTaken = msg.amount();

	if ( damageTaken > 0 )
	{
		Vector vecFrom;
		vecFrom.x = msg.inflictor_world_pos().x();
		vecFrom.y = msg.inflictor_world_pos().y();
		vecFrom.z = msg.inflictor_world_pos().z();

		CalcDamageDirection( vecFrom, pVictimPlayer );

		// If we are using a Steam Controller, do haptics on the Steam Controller
		// to indicate getting hit.
		if ( g_pInputSystem->IsSteamControllerActive() && steamapicontext->SteamController() )
		{
			static ConVarRef steam_controller_haptics( "steam_controller_haptics" );
			if ( steam_controller_haptics.GetBool() )
			{
				ControllerHandle_t handles[MAX_STEAM_CONTROLLERS];
				int nControllers = steamapicontext->SteamController()->GetConnectedControllers( handles );

				for ( int i = 0; i < nControllers; ++i )
				{
					float flLeft = m_flAttackLeft + m_flAttackFront*0.5 + m_flAttackRear*0.5;
					float flRight = m_flAttackRight + m_flAttackFront*0.5 + m_flAttackRear*0.5;
					float flTotal = flLeft + flRight;
					if ( flTotal > 0.0 )
					{
						flLeft /= flTotal;
						flRight /= flTotal;
						if ( flRight > 0 )
						{
							steamapicontext->SteamController()->TriggerHapticPulse( handles[i], k_ESteamControllerPad_Right, 2000 * flRight );
						}

						if ( flLeft > 0 )
						{
							steamapicontext->SteamController()->TriggerHapticPulse( handles[i], k_ESteamControllerPad_Left, 2000 * flLeft );
						}
					}
				}
			}

		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Called from the 'Damage' message handler to update 
//			m_flAttackFront / m_flAttackRear / m_flAttackLeft / m_flAttackRight
//-----------------------------------------------------------------------------
void CCSGO_HudDamageIndicator::CalcDamageDirection( const Vector &vecFrom, C_BasePlayer *pVictimPlayer )
{
	// I assume this is done to detect damage from world (falling) and not display
	// an indicator for this. Old code was zeroing all indicator values here which caused
	// a bug if we were currently in mid-fade for a previous damage source. 
	if ( vecFrom == vec3_origin )
	{
		return;
	}

	if ( !pVictimPlayer )
	{
		return;
	}

	Vector vecDelta = ( vecFrom - pVictimPlayer->GetRenderOrigin() );

	if ( vecDelta.Length() <= s_flCloseDamageDistance )
	{
		m_flAttackFront = 1.0f;
		m_flAttackRear = 1.0f;
		m_flAttackRight = 1.0f;
		m_flAttackLeft = 1.0f;

		return;
	}

	VectorNormalize( vecDelta );

	Vector forward;
	Vector right;
	AngleVectors( MainViewAngles( GET_ACTIVE_SPLITSCREEN_SLOT() ), &forward, &right, NULL );

	float flFront = DotProduct( vecDelta, forward );
	float flSide = DotProduct( vecDelta, right );

	if ( flFront > 0 )
	{
		if ( flFront > s_flDirectionDotTolerance )
			m_flAttackFront = Max( m_flAttackFront, flFront );
	}
	else
	{
		float f = fabs( flFront );
		if ( f > s_flDirectionDotTolerance )
			m_flAttackRear = Max( m_flAttackRear, f );
	}

	if ( flSide > 0 )
	{
		if ( flSide > s_flDirectionDotTolerance )
			m_flAttackRight = Max( m_flAttackRight, flSide );
	}
	else
	{
		float f = fabs( flSide );
		if ( f > s_flDirectionDotTolerance )
			m_flAttackLeft = Max( m_flAttackLeft, f );
	}
}
