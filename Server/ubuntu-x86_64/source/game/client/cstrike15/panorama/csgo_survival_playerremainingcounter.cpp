//========= Copyright © Valve Corporation, All rights reserved. ============//
//
//=====================================================================================//

#include "cbase.h"
#include "csgo_survival_playerremainingcounter.h"
#include "IGameUIFuncs.h"
#include <engine/IEngineSound.h>
#include "panorama/ui_root.h"
#include "panorama/uijsregistration.h"
#include "c_cs_player.h"
#include "c_cs_playerresource.h"
#include "clientsteamcontext.h"
#include "cs_gamerules_survival.h"
#include "hudelement.h"
#include "panorama/controls/movieplayer.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace panorama;

REGISTER_PANEL2D_FACTORY( CCSGO_SurvivalPlayerRemainingCounter, CSGOSurvivalPlayerRemainingCounter );

DEFINE_PANORAMA_EVENT_DOC( SurvivalShowPlayerRemainingCounter, "bool", "Show or hide the spawn panel." );
DEFINE_PANORAMA_EVENT_DOC( SurvivalPlayerRemainingCounterUpdate, "", "Update state from survival gamerules" );

extern ConVar cl_draw_only_deathnotices;
extern ConVar cl_drawhud;

CCSGO_SurvivalPlayerRemainingCounter::CCSGO_SurvivalPlayerRemainingCounter( panorama::CPanel2D *pParent, const char *pchID )
	: CPanoramaHudElement( "CCSGO_SurvivalPlayerRemainingCounter", this )
	, panorama::CPanel2D( pParent, pchID )
{
	RequireLoadLayout( "file://{resources}/layout/survival/survival_playerremainingcounter.xml" );

	UpdatePlayerCount( true );

	m_bShowingAsSpectator = true;
	m_nAlivePlayers = -1;
	m_nAliveTeams = -1;

	m_flLastVocalizedTime = 0;
	m_numLastVocalizedEnemies = 0;
}

void CCSGO_SurvivalPlayerRemainingCounter::UpdatePlayerCount( bool bVisible )
{
	if ( !bVisible )
	{
		SetVisible( false );
		return;
	}

	// How many enemies remaining?
	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	int numEnemiesRemaining = 0;	// Total number of enemies or humans remaining
	bool bJustCountRemainingHumans = ( pLocalPlayer && pLocalPlayer->GetTeamNumber() != TEAM_TERRORIST );	// spectators or GOTV
	if ( pLocalPlayer && CSGameRules() && CSGameRules()->GetSurvivalRules() && !CSGameRules()->IsWarmupPeriod()
		&& !CSGameRules()->IsFreezePeriod() && !CSGameRules()->IsRoundOver() )
	{
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CCSPlayer* pAlivePlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) ); // is the client allowed to do this? Probably ok for alive survival players?
			if ( !pAlivePlayer ) continue;
			if ( pAlivePlayer->IsHLTV() ) continue;
			if ( pAlivePlayer->GetTeamNumber() != TEAM_TERRORIST ) continue;
			if ( !pAlivePlayer->IsAlive() ) continue;

			if ( bJustCountRemainingHumans || pLocalPlayer->IsOtherEnemy( pAlivePlayer ) )
				++numEnemiesRemaining;
		}

		if ( pLocalPlayer->IsAlive() && ( pLocalPlayer->GetTeamNumber() == TEAM_TERRORIST ) && !g_bEngineIsHLTV )
		{	// Vocalize if local player is still alive and actively playing
			if ( numEnemiesRemaining >= m_numLastVocalizedEnemies )
			{
				m_numLastVocalizedEnemies = numEnemiesRemaining;
				m_flLastVocalizedTime = gpGlobals->curtime;
			}
			else if ( gpGlobals->curtime > m_flLastVocalizedTime + 5.0f )
			{
				m_flLastVocalizedTime = gpGlobals->curtime;
				m_numLastVocalizedEnemies = numEnemiesRemaining;

				if ( m_numLastVocalizedEnemies >= 1 && m_numLastVocalizedEnemies <= 6 )
				{
					CLocalPlayerFilter filter;
					C_BaseEntity::EmitSound( filter, SOUND_FROM_LOCAL_PLAYER, CFmtStr( "Survival.EnemiesLeft%d", m_numLastVocalizedEnemies ) );
				}
			}
		}
	}

	int* pCurrentVal = &numEnemiesRemaining;
	int* pPreviousVal = &m_nAlivePlayers;

	// Do we have teammates alive? If we are already in replay then show "Players" in HUD
	if ( !bJustCountRemainingHumans && g_bEngineIsHLTV )
		bJustCountRemainingHumans = true;

	bool bDataChanged = ( bJustCountRemainingHumans != m_bShowingAsSpectator ) || ( *pCurrentVal != *pPreviousVal );

	if ( bDataChanged && ShouldDraw() )
	{
		SetDialogVariable( "players-remaining", *pCurrentVal );
		*pPreviousVal = *pCurrentVal;
		m_bShowingAsSpectator = bJustCountRemainingHumans;

		CLabel *pLabel = panel_cast<CLabel*> (FindChildInLayoutFile( "RemainText" ));
		if ( pLabel )
		{
			numEnemiesRemaining = MAX( numEnemiesRemaining, 1 );
			numEnemiesRemaining = MIN( numEnemiesRemaining, 16 );
			CFmtStr fmtLabel( "#Survival_Remaining%s_%d", bJustCountRemainingHumans ? "Players" : "Enemies", numEnemiesRemaining );
			pLabel->SetText( fmtLabel.Access() );
		}

		CMoviePlayer *pMovie = panorama::panel_cast<CMoviePlayer *>(FindChildInLayoutFile( "SplatMovie" ));
		if ( pMovie )
		{
			pMovie->Play();
		}

		// Hacky hook: update rich presence in survival when enemies remaining counter changes
		extern IBaseClientDLL *clientdll;
		if ( clientdll )
			( void ) clientdll->GetRichPresenceStatusString();
	}

	SetVisible( *pCurrentVal >= 1 );
}

void CCSGO_SurvivalPlayerRemainingCounter::SetActive( bool bActive )
{
	UpdatePlayerCount( bActive );

	CPanoramaHudElement::SetActive( bActive );
}

bool CCSGO_SurvivalPlayerRemainingCounter::ShouldDraw( void )
{
	if ( !CSGameRules() || !CSGameRules()->IsPlayingSurvival() || CSGameRules()->IsWarmupPeriod() )
	{
		return false;
	}

	CSurvivalGameRules* pBRrules = CSGameRules()->GetSurvivalRules();
	if ( pBRrules && gpGlobals->curtime < pBRrules->GetSpawnSelectTimeEnd() )
	{
		return false;
	}

	return cl_drawhud.GetBool() && cl_draw_only_deathnotices.GetBool() == false && CPanoramaHudElement::ShouldDraw();
}

void CCSGO_SurvivalPlayerRemainingCounter::Think( void )
{
	UpdatePlayerCount( ShouldDraw() );
}

void CCSGO_SurvivalPlayerRemainingCounter::OnLayoutReloaded()
{
	BaseClass::OnLayoutReloaded();

	m_nAlivePlayers = -1;

	UpdatePlayerCount( false );
}
