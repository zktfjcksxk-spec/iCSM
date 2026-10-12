//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: mini-scoreboard/playercount
//
// refer to \content\csgo\flash\MainUI\Hud\PlayerCount\scene_f1.as
//
//=============================================================================//

#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "hud_element_helper.h"
#include "csgo_hudteamcounter.h"
#include "csgo_hudspectator.h"
#include "c_team.h"
#include "c_cs_playerresource.h"
#include "c_plantedc4.h"
#include "matchmaking/imatchframework.h"
#include "voice_status.h"
#include "vstdlib/vstrtools.h"
#include "gameui_util.h"
#if defined( INCLUDE_SCALEFORM )
#include "HUD/sfhudfreezepanel.h"
#endif
#include "gametypes.h"
#include "cs_gamerules.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern IGameTypes *g_pGameTypes;
extern ConVar cl_spec_swapplayersides;
extern ConVar cl_draw_only_deathnotices;
extern ConVar mp_forcecamera;
extern ConVar cl_hud_playercount_showcount;
extern ConVar cl_show_enemy_avatar_colors;

REGISTER_PANEL2D_FACTORY( CCSGO_HudTeamCounter, CSGOHudTeamCounter );
REGISTER_PANEL2D_FACTORY( CCSGO_AvatarHealthBar, CSGOAvatarHealthBar );

// When somebody surpasses the current GG Prog leader, save off their player index
static int g_GGProgLeaderPlayerIdx = -1;

//TODO - temp location/nameclash workaround until we find a common place to put all these helpers 
extern ConVar mp_spec_swapplayersides;
bool Helper_DisplayTeamsOnOppositeSides_Panorama( void )
{
	bool bSwitched = CSGameRules()->AreTeamsPlayingSwitchedSides();

	// swap sides again if we have the either swap convar set
	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
	bool bLocalObserverWantsSideSwap = pPlayer && ( pPlayer->IsSpectator() || pPlayer->IsHLTV() );
	bool bServerWantsSideSwap = ( mp_spec_swapplayersides.GetBool() );
	if ( bLocalObserverWantsSideSwap != bServerWantsSideSwap )
		bSwitched = !bSwitched;

	return bSwitched;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CAvatarPanelData::CAvatarPanelData()
{
	V_memset( this, 0, sizeof( CAvatarPanelData ) );
}

CAvatarPanelData::~CAvatarPanelData()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAvatarPanelData::SetBGForTeam( bool bCT, bool bSmall )
{
	static const panorama::CPanoramaSymbol k_symAvatarBGCT( "AvatarImageCT__Color" );
	static const panorama::CPanoramaSymbol k_symAvatarBGT( "AvatarImageT__Color" );

	static const panorama::CPanoramaSymbol k_symAvatarSDefaultBGCT( "AvatarS__Default--CT" );
	static const panorama::CPanoramaSymbol k_symAvatarSDefaultBGT( "AvatarS__Default--T" );
	static const panorama::CPanoramaSymbol k_symAvatarLDefaultBGCT( "AvatarL__Default--CT" );
	static const panorama::CPanoramaSymbol k_symAvatarLDefaultBGT( "AvatarL__Default--T" );


	if ( !m_pAvatarBG )
		return;

	if ( bCT )
	{
		m_pAvatarBG->SetHasClass( k_symAvatarBGCT, true );
		m_pAvatarBG->SetHasClass( k_symAvatarBGT, false );

		if ( bSmall )
		{
			m_pAvatarBG->SetHasClass( k_symAvatarSDefaultBGCT, true );
			m_pAvatarBG->SetHasClass( k_symAvatarSDefaultBGT, false );
		}
		else
		{
			m_pAvatarBG->SetHasClass( k_symAvatarLDefaultBGCT, true );
			m_pAvatarBG->SetHasClass( k_symAvatarLDefaultBGT, false );
		}
	}
	else
	{
		m_pAvatarBG->SetHasClass( k_symAvatarBGCT, false );
		m_pAvatarBG->SetHasClass( k_symAvatarBGT, true );

		if ( bSmall )
		{
			m_pAvatarBG->SetHasClass( k_symAvatarSDefaultBGCT, false );
			m_pAvatarBG->SetHasClass( k_symAvatarSDefaultBGT, true );
		}
		else
		{
			m_pAvatarBG->SetHasClass( k_symAvatarLDefaultBGCT, false );
			m_pAvatarBG->SetHasClass( k_symAvatarLDefaultBGT, true );
		}
	}

}

void CAvatarPanelData::SetOutlineVisible( bool bOutlineVisible )
{
	static const panorama::CPanoramaSymbol k_symBorderTypeAttribute( "bordertype" );

	if ( !m_pAvatarBG )
		return;

	m_pAvatarBG->SetHasClass( k_symBorderTypeAttribute, bOutlineVisible );
}

void CAvatarPanelData::SetBorderType( panorama::CPanoramaSymbol symName )
{
	static const panorama::CPanoramaSymbol k_symBorderTypeAttribute( "bordertype" );

	if ( !m_pAvatarBG )
		return;

	m_pAvatarBG->SwitchClass( k_symBorderTypeAttribute, symName );
}

void CAvatarPanelData::SetSkullType( panorama::CPanoramaSymbol symName )
{
	static const panorama::CPanoramaSymbol k_symSkullTypeAttribute( "skulltype" );

	if ( !m_pSkull )
		return;

	m_pSkull->SwitchClass( k_symSkullTypeAttribute, symName );
}

void CAvatarPanelData::SetBotType( panorama::CPanoramaSymbol symName )
{
	static const panorama::CPanoramaSymbol k_symBotTypeAttribute( "bottype" );

	if ( !m_pBot )
		return;

	m_pBot->SwitchClass( k_symBotTypeAttribute, symName );
}

void CAvatarPanelData::SetHealthBarType( panorama::CPanoramaSymbol symName )
{
	static const panorama::CPanoramaSymbol k_symHealthBarTypeAttribute( "healthbartype" );

	if ( !m_pHealth )
		return;

	m_pHealth->SwitchClass( k_symHealthBarTypeAttribute, symName );
}

//-----------------------------------------------------------------------------
// Re-parent pChild to visible or invisible avatar panels
// return true if changed parent, false otherwise
//-----------------------------------------------------------------------------
bool CAvatarPanelData::SetChildVisible( panorama::CPanel2D *pChild, bool bVisible )
{
	if ( !pChild ) 
		return false;

	if ( bVisible )
	{
		if ( pChild->GetParent() != m_pVisible )
		{
			pChild->SetParent( m_pVisible );
			return true;
		}
		return false;
	}
	else
	{
		if ( pChild->GetParent() != m_pInvisible )
		{
			pChild->SetParent( m_pInvisible );
			return true;
		}
		return false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCSGO_HudTeamCounter::CCSGO_HudTeamCounter( panorama::CPanel2D *pParent, const char *pchID )
	:
	panorama::CPanel2D( pParent, pchID ),
	CPanoramaHudElement( "CCSGO_HudTeamCounter", this ),
	m_pGameTimeContainer( NULL ),
	m_pBombContainer( NULL ),
	m_pGameScoreContainer( NULL ),
	m_pTime( NULL ),
	m_pCTScore( NULL ),
	m_pTScore( NULL ),
	m_bTimerAlertTriggered( false ),
	m_bRoundStarted( true ),
	m_bIsBombDefused( false ),
	m_bTimerHidden( false ),
	m_nTScoreLastUpdate( -1 ),
	m_nCTScoreLastUpdate( -1 ),
	m_Mode( VIEW_MODE_NORMAL ),
	m_nLeaderWeaponRank( -1 ),
	m_nTerroristTeamCount( -1 ),
	m_nCTTeamCount( -1 ),
	m_nPreviousGGProgressiveTotalPlayers( -1 ),
	m_bForceAvatarRefresh( false ),
	m_flPlayingTeamFadeoutTime( -1 ),
	m_flLastSpecListUpdate( -1 ),
	m_nHudBGAlpha( -1 ),
	m_nLastGGPlayerCount( 0 ),
	m_nMaxPlayers( 24 ),
	m_nPlayersAlive_CT( 0 ),
	m_nPlayersAlive_T( 0 ),
	m_bIsGunGame( false ),
	m_bIsShowingTimer( false ),
	m_bPositionInitialised( false ),
	m_bPositionIsBottom( false ),
	m_bShowOnlyPlayerCount( false )
{
	ListenForGameEvent( "round_start" );
	ListenForGameEvent( "round_announce_warmup" );
	ListenForGameEvent( "round_end" );
	ListenForGameEvent( "cs_match_end_restart" );
	ListenForGameEvent( "bomb_planted" );
	ListenForGameEvent( "bomb_defused" );
	ListenForGameEvent( "player_spawn" );
	ListenForGameEvent( "player_death" );
	ListenForGameEvent( "bot_takeover" );
	ListenForGameEvent( "player_team" );

	RequireLoadLayout( "file://{resources}/layout/hud/hudteamcounter.xml" );

	m_pTeamCounterPanel = RequireChildInLayoutFile( "TeamCounterBG" );

	// score and time panel
	m_pGameTimeContainer = RequireChildInLayoutFile( "GameTime" );
	m_pGameTimeContainer->SetVisible( false );
	m_pBombContainer = RequireChildInLayoutFile( "BombStatus" );
	m_pBombContainer->SetVisible( false );
	m_pGameScoreContainer = RequireChildInLayoutFile( "GameScore" );
	m_pGameScoreContainer->SetVisible( false );

	// init time
	m_pTime = panorama::panel_cast< panorama::CLabel * >( RequireChildInLayoutFile( "TimerText" ) );
	m_nLastTimeSet = TIMER_STATE_UNSET;

	// init bomb
	m_pBombPlanted = panorama::panel_cast< panorama::CImagePanel * >( RequireChildInLayoutFile( "BombPlanted" ) );
	m_pBombPlantedLines = panorama::panel_cast< panorama::CImagePanel * >( RequireChildInLayoutFile( "BombPlantedLines" ) );
	m_pBombDefused = panorama::panel_cast< panorama::CImagePanel * >( RequireChildInLayoutFile( "BombDefused" ) );
	m_pBombPlanted->SetVisible( false );
	m_pBombPlantedLines->SetVisible( false );
	m_pBombDefused->SetVisible( false );

	// init score
	m_pCTScore = panorama::panel_cast< panorama::CLabel * >( RequireChildInLayoutFile( "ScoreCT" ) );
	m_pTScore = panorama::panel_cast< panorama::CLabel * >( RequireChildInLayoutFile( "ScoreT" ) );

	// alive count panels
	m_pAliveBGCT = RequireChildInLayoutFile( "AliveBackgroundCT" );
	m_pAliveCountCT = panorama::panel_cast< panorama::CLabel * >( RequireChildInLayoutFile( "AliveCountCT" ) );
	m_pAliveTextCT = panorama::panel_cast< panorama::CLabel * >( RequireChildInLayoutFile( "AliveTextCT" ) );
	m_pAliveSkullCT = panorama::panel_cast< panorama::CImagePanel * >( RequireChildInLayoutFile( "AliveSkullCT" ) );
	m_pAliveBGT = RequireChildInLayoutFile( "AliveBackgroundT" );
	m_pAliveCountT = panorama::panel_cast< panorama::CLabel * >( RequireChildInLayoutFile( "AliveCountT" ) );
	m_pAliveTextT = panorama::panel_cast< panorama::CLabel * >( RequireChildInLayoutFile( "AliveTextT" ) );
	m_pAliveSkullT = panorama::panel_cast< panorama::CImagePanel * >( RequireChildInLayoutFile( "AliveSkullT" ) );
	m_pAliveBGCT->SetVisible( false );
	m_pAliveBGT->SetVisible( false );

	// join panels
	m_pJoinPanelBot = RequireChildInLayoutFile( "JoinPanelBot" );
	m_pJoinPanelCT = RequireChildInLayoutFile( "JoinPanelCT" );
	m_pJoinPanelT = RequireChildInLayoutFile( "JoinPanelT" );
	m_pJoinTextBot = panorama::panel_cast< panorama::CLabel * >( RequireChildInLayoutFile( "JoinTextBot" ) );
	m_pJoinPanel = RequireChildInLayoutFile( "JoinPanelParent" );

	m_pTeamAll = RequireChildInLayoutFile( "TeamAll" );
	m_pTeamLargeCT = RequireChildInLayoutFile( "TeamLargeCT" );
	m_pTeamLargeT = RequireChildInLayoutFile( "TeamLargeT" );
	m_pTeamSmallCTContainer = RequireChildInLayoutFile( "TeamSmallContainerCT" );
	m_pTeamSmallTContainer = RequireChildInLayoutFile( "TeamSmallContainerT" );
	m_pTeamSmallCTRow1 = RequireChildInLayoutFile( "TeamSmallCT__Row1" );
	m_pTeamSmallCTRow2 = RequireChildInLayoutFile( "TeamSmallCT__Row2" );
	m_pTeamSmallTRow1 = RequireChildInLayoutFile( "TeamSmallT__Row1" );
	m_pTeamSmallTRow2 = RequireChildInLayoutFile( "TeamSmallT__Row2" );


	m_pTeamAll->SetVisible( false );
	m_pTeamLargeCT->SetVisible( false );
	m_pTeamLargeT->SetVisible( false );
	m_pTeamSmallCTContainer->SetVisible( false );
	m_pTeamSmallTContainer->SetVisible( false );

	m_GGProgRankingTimer.Invalidate();

	SetAcceptsInput( false );
	SetAcceptsFocus( false );
	ShowPanel( false );

	m_aAvatarPanels.SetSize( MAX_AVATAR_PANELS );
	m_aPAvatarPanels.SetSize( MAX_PAVATAR_PANELS );

}

CCSGO_HudTeamCounter::~CCSGO_HudTeamCounter()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudTeamCounter::BeginTimerAlert( void )
{
	m_pTime->SetVisible( true );
	SetTimerRedColor( true );

	// TODO
/*	PTime.GlowPulse._visible = true;*/
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudTeamCounter::BeginTimerNormal( void )
{
	m_pTime->SetVisible( true );
	SetTimerRedColor( false );

	m_pBombPlanted->SetVisible( false );
	m_pBombPlantedLines->SetVisible( false );
	m_pBombDefused->SetVisible( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudTeamCounter::DisablePlayerIcon( bool bCT, int nSlot )
{
	int nHalfPlayers = m_nMaxPlayers / 2;
	int idx;

	CAvatarPanelData *pAvatarPanelData;

	if ( bCT )
	{
		idx = ( nHalfPlayers - 1 ) - nSlot;
	}
	else
	{
		idx = nHalfPlayers + nSlot;
	}

	if ( m_nMaxPlayers > 10 )
	{
		if ( !m_aPAvatarPanels.IsValidIndex( idx ) )
			return;

		pAvatarPanelData = &m_aPAvatarPanels[ idx ];
	}
	else
	{
		if ( !m_aAvatarPanels.IsValidIndex( idx ) )
			return;

		pAvatarPanelData = &m_aAvatarPanels[ idx ];
	}

	pAvatarPanelData->m_savedXUID = 0;

	if ( pAvatarPanelData->m_pPanel )
	{
		pAvatarPanelData->m_pPanel->SetVisible( false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudTeamCounter::DisableRemainingPlayerIcons( bool bCT, int nStartSlot )
{
	if ( CSGameRules() && CSGameRules()->IsPlayingSurvival() )
		return;

	int nHalfPlayers = m_nMaxPlayers / 2;

	for ( int idx = nStartSlot; idx < nHalfPlayers; idx++ )
	{
		DisablePlayerIcon( bCT, idx );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudTeamCounter::FireGameEvent( IGameEvent *event )
{
	const char *type = event->GetName();
	CBasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	int EventUserID = event->GetInt( "userid", -1 );
	int LocalPlayerID = ( pLocalPlayer != NULL ) ? pLocalPlayer->GetUserID() : -2;

	if ( !V_strcmp( type, "round_start" ) )
	{
		m_bRoundStarted = true;
		m_bIsBombDefused = false;

		BeginTimerNormal();

		if ( m_Mode == VIEW_MODE_GUN_GAME_PROGRESSIVE )
		{
			ResetLeader();
			m_GGProgRankingTimer.Invalidate();
			m_GGProgRankingTimer.Start( 0.5f );
		}
	}
	else if ( !V_strcmp( type, "round_announce_warmup" ) )
	{
		if ( m_pTime )
		{
			m_pTime->SetText( "" );
			m_nLastTimeSet = TIMER_STATE_UNSET;
		}
	}
	else if ( !V_strcmp( type, "round_end" ) )
	{
		// Update the team timer one last time at round end, so we properly show "0:00" if the round timed out
		C_CSGameRules *pRules = CSGameRules();
		if ( pRules && m_pTime )
		{
			int nTimer = static_cast<int>( floor( pRules->GetRoundRemainingTime() ) );
			if ( nTimer < 0 )
				nTimer = 0;

			char szTime[ 32 ];
			V_snprintf( szTime, ARRAYSIZE( szTime ), "%d:%.2d", ( nTimer / 60 ), ( nTimer % 60 ) );

			m_pTime->SetText( szTime );

			int iReason = event->GetInt( "reason", -1 );

			switch ( iReason )
			{
				// 			case Target_Bombed:
				// 				break;

			case Bomb_Defused:

				m_bIsBombDefused = true;

			default:
				UpdatePlantedBombState( 0.0f );
			}
		}

		m_bTimerAlertTriggered = false;
		SetTimerRedColor( false );
		m_nLastTimeSet = TIMER_STATE_UNSET;

		m_bRoundStarted = false;

		HideDisplayTeamPanels();
	}
	else if ( !V_strcmp( type, "cs_match_end_restart" ) )
	{
		if ( m_Mode == VIEW_MODE_GUN_GAME_PROGRESSIVE || m_Mode == VIEW_MODE_GUN_GAME_BOMB )
		{
			ResetLeader();
		}
	}
	else if ( !V_strcmp( type, "bomb_planted" ) )
	{
		if ( !CSGameRules()->IsPlayingCoopMission() && !CSGameRules()->IsPlayingSurvival() )
		{
			HideTimer();
			UpdatePlantedBombState( 100.0f );
		}
	}

	else if ( !V_strcmp( type, "player_spawn" ) )
	{
		UpdateMiniScoreboard();

		if ( EventUserID == LocalPlayerID )
		{
			HideDisplayTeamPanels();

			int nTeam = event->GetInt( "teamnum", -1 );

			if ( nTeam > 0
				&& !CSGameRules()->IsPlayingGunGameProgressive()
				&& !CSGameRules()->IsPlayingGunGameDeathmatch() 
				&& !CSGameRules()->IsPlayingSurvival() 
				)
			{
				SpawnDisplaySelectedTeam( nTeam );
				m_flPlayingTeamFadeoutTime = gpGlobals->curtime + 10.0f;
			}

			if ( m_Mode == VIEW_MODE_GUN_GAME_PROGRESSIVE || m_Mode == VIEW_MODE_GUN_GAME_BOMB )
			{
				// only need to update this once when the local player first spawns in
				if ( m_nLeaderWeaponRank == -1 )
				{
					ResetLeader();
				}
			}

			// Sanity check: round should have already started once we are spawned
			m_bRoundStarted = true;
		}
	}
	else if ( !V_strcmp( type, "player_death" ) && EventUserID == LocalPlayerID )
	{
		HideDisplayTeamPanels();
	}
	else if ( !V_strcmp( type, "player_team" ) && ( EventUserID == LocalPlayerID ) )
	{
		m_bForceAvatarRefresh = true;

		UpdateMiniScoreboard();

		// 		int playerIndex = GetPlayerIndexFromUserID(event->GetInt("userid"));
		// 
		// 		if (playerIndex == INVALID_INDEX)
		// 		{
		// 			return;
		// 		}
		// 
		// 		SFHudRadarIconPackage* pPackage = GetRadarPlayer(playerIndex);
		// 		pPackage->SetPlayerTeam(event->GetInt("team"));
		// 		UpdatePlayerNumber(pPackage);
	}
	else if ( !V_strcmp( type, "bot_takeover" ) && ( EventUserID == LocalPlayerID ) )
	{
		C_BasePlayer *pBot = UTIL_PlayerByUserId( event->GetInt( "botid" ) );
		if ( pBot )
		{
			/*wchar_t wszLocalized[ 100 ];
			wchar_t wszPlayerName[ MAX_PLAYER_NAME_LENGTH ];
			g_pVGuiLocalize->ConvertANSIToUnicode( pBot->GetPlayerName(), wszPlayerName, sizeof( wszPlayerName ) );
			g_pVGuiLocalize->ConstructString( wszLocalized, sizeof( wszLocalized ), g_pVGuiLocalize->Find( "#SFUI_Notice_Hint_Bot_Takeover" ), 1, wszPlayerName );
			*/
			TakeOverBot( pBot->GetPlayerName() );

			m_flPlayingTeamFadeoutTime = -1;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CCSGO_HudTeamCounter::GGProgSortFunction( TeamCounterMiniStatus_t* const* entry1, TeamCounterMiniStatus_t* const* entry2 )
{
	if ( entry1 == NULL || ( *entry1 ) == NULL )
		return 1;

	if ( entry2 == NULL || ( *entry2 ) == NULL )
		return -1;

	// Higher GG Progressive weapon ranks higher.  In case of ties for that, we rank according to player index so 
	//   we don't overly shuffle the ordering
	if ( ( *entry1 )->nGunGameLevel > ( *entry2 )->nGunGameLevel )
		return -1;
	else if ( ( *entry1 )->nGunGameLevel < ( *entry2 )->nGunGameLevel )
		return 1;
	else
	{
		// always put the current leader up top
		if ( ( *entry1 )->bTeamLeader && ( *entry2 )->bTeamLeader == false )
			return -1;
		else if ( ( *entry2 )->bTeamLeader && ( *entry1 )->bTeamLeader == false )
			return 1;

		// Current GG leader always sorts in front in the case of a tie
		if ( ( *entry1 )->nPlayerIdx == g_GGProgLeaderPlayerIdx )
			return -1;
		else if ( ( *entry2 )->nPlayerIdx == g_GGProgLeaderPlayerIdx )
			return 1;
		else
			return ( *entry1 )->nPlayerIdx - ( *entry2 )->nPlayerIdx;
	}
}

int CCSGO_HudTeamCounter::DMSortFunction( TeamCounterMiniStatus_t* const* entry1, TeamCounterMiniStatus_t* const* entry2 )
{
	if ( entry1 == NULL || ( *entry1 ) == NULL )
		return 1;

	if ( entry2 == NULL || ( *entry2 ) == NULL )
		return -1;

	// Higher GG Progressive weapon ranks higher.  In case of ties for that, we rank according to player index so 
	//   we don't overly shuffle the ordering
	if ( ( *entry1 )->nPoints > ( *entry2 )->nPoints )
		return -1;
	else if ( ( *entry1 )->nPoints < ( *entry2 )->nPoints )
		return 1;
	else
		return ( *entry1 )->nPlayerIdx - ( *entry2 )->nPlayerIdx;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const TeamCounterMiniStatus_t* CCSGO_HudTeamCounter::GetPlayerStatus( int index )
{
	// check to see if we haven't been updated in the last second
	if ( m_bActive == false && ( m_flLastSpecListUpdate + 1.0f < gpGlobals->curtime ) )
	{
		// the list hasn;t been updated recently (probably because the hud element is hidden) so update it now
		UpdateMiniScoreboard();
	}

	if ( !CSGameRules() )
		return NULL;

	//	bool bIsCompetitive = sv_competitive_official_5v5.GetInt() || CSGameRules()->IsPlayingAnyCompetitiveStrictRuleset();
	return const_cast<const CCSGO_HudTeamCounter*> ( this )->GetPlayerStatus( index );
}

const TeamCounterMiniStatus_t* CCSGO_HudTeamCounter::GetPlayerStatus( int index ) const
{
	if ( m_Mode == VIEW_MODE_GUN_GAME_PROGRESSIVE )
	{
		if ( index < 0 )
		{
			return NULL;
		}
		else if ( index >= m_ggSortedList.Count() )
		{
			return NULL;
		}
		else
		{
			return m_ggSortedList[ index ];
		}

	}
	else
	{
		//TODO - rename once nameclash with SF resolved
		bool bAreTeamsSwitched = Helper_DisplayTeamsOnOppositeSides_Panorama();

		if ( index < 0 )
			return NULL;

		if ( bAreTeamsSwitched )
		{
			int i = index;

			if ( i < m_nTerroristTeamCount )
			{
				return &m_TerroristTeam[ i ];
			}

			i -= m_nTerroristTeamCount;

			if ( i < m_nCTTeamCount )
			{
				return &m_CTTeam[ i ];
			}
		}
		else
		{
			int i = index;

			if ( i < m_nCTTeamCount )
			{
				return &m_CTTeam[ i ];
			}

			i -= m_nCTTeamCount;

			if ( i < m_nTerroristTeamCount )
			{
				return &m_TerroristTeam[ i ];
			}
		}

		return NULL;
	}
}

int CCSGO_HudTeamCounter::GetPlayerSlotIndex( int playerEntIndex )
{
	// check to see if we haven't been updated in the last second
	if ( m_bActive == false && ( m_flLastSpecListUpdate + 1.0f < gpGlobals->curtime ) )
	{
		// the list hasn't been updated recently (probably because the hud element is hidden) so update it now
		UpdateMiniScoreboard();
	}

	return const_cast<const CCSGO_HudTeamCounter*> ( this )->GetPlayerSlotIndex( playerEntIndex );

}

int CCSGO_HudTeamCounter::GetPlayerSlotIndex( int playerEntIndex ) const
{
	for ( int i = 0; i < MAX_TEAM_SIZE * 2; i++ )
	{
		const TeamCounterMiniStatus_t* pMS = GetPlayerStatus( i );
		if ( pMS && ( pMS->nPlayerIdx == playerEntIndex ) )
		{
			return i;
		}
	}

	return -1;
}

// Internally this class keeps observer slots 0 based but has a similar check scattered in other files to bump up to one based whenever it displays... Making method for that.
// These are used for the 1 thorugh 0 keyboard keys to select an observer, so return an error value if slot is out of range. 
// This hack will die with scaleform.
int CCSGO_HudTeamCounter::GetPlayerSlotIndexForDisplay( int playerEntIndex ) const
{
	int iObserverDisplaySlot = GetPlayerSlotIndex( playerEntIndex );
	if ( iObserverDisplaySlot >= 0 && iObserverDisplaySlot <= 9 )
		return ( iObserverDisplaySlot + 1 ) % 10;
	else
		return -1;
}

int CCSGO_HudTeamCounter::GetPlayerEntIndexInSlot( int nIndex )
{
	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pLocalPlayer )
	{
		return -1;
	}

	const TeamCounterMiniStatus_t* pMS = GetPlayerStatus( nIndex );
	if ( pMS )
	{
		return pMS->nPlayerIdx;
	}

	return -1;
}

int CCSGO_HudTeamCounter::GetSpectatorTargetFromSlot( int idx )
{
	// check to see if we haven't been updated in the last second
	if ( m_bActive == false && m_flLastSpecListUpdate + 1.0f < gpGlobals->curtime )
	{
		// the list hasn;t been updated recently (probably because the hud element is hidden) so update it now
		UpdateMiniScoreboard();
	}

	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pLocalPlayer )
	{
		return -1;
	}

	//	int localPlayerIndex = GetLocalPlayerIndex();
	// 
	C_CS_PlayerResource* pCSPR = (C_CS_PlayerResource*)g_PR;
	// 
	int spectatedTargetIndex = -1;
	// 	//if ( GetSpectatorMode() == OBS_MODE_IN_EYE || GetSpectatorMode() == OBS_MODE_CHASE )
	// 	{
	spectatedTargetIndex = GetSpectatorTarget();
	// 
	// 		if ( !spectatedTargetIndex )
	// 		{
	// 			spectatedTargetIndex = localPlayerIndex;
	// 		}
	// 	}
	// 
	// 	if ( !spectatedTargetIndex )
	// 	{
	// 		return -1;
	// 	}
	// 
	// 	int controlledPlayer = pCSPR->GetControlledPlayer( spectatedTargetIndex );
	// 
	// 	if ( controlledPlayer != 0 )
	// 	{
	// 		spectatedTargetIndex = controlledPlayer;
	// 	}
	// 
	// 	int originalSlotIndex = GetPlayerSlotIndex( spectatedTargetIndex );
	// 
	// 	if ( originalSlotIndex == -1 || originalSlotIndex == idx )
	// 	{
	// 		return -1;
	// 	}

	bool showEnemy = mp_forcecamera.GetInt() == OBS_ALLOW_ALL;

	if ( pLocalPlayer->GetTeamNumber() < TEAM_TERRORIST )
	{
		showEnemy = true;
	}

	bool bWantCT = spectatedTargetIndex > 0 ? ( pCSPR->GetTeam( spectatedTargetIndex ) == TEAM_CT ) : false;

	const TeamCounterMiniStatus_t* pStatus = GetPlayerStatus( idx );

	if ( pStatus && !pStatus->bDead && ( showEnemy || ( pStatus->bIsCT == bWantCT ) ) )
	{
		int result = pCSPR->GetControlledByPlayer( pStatus->nPlayerIdx );
		if ( !result )
		{
			result = pStatus->nPlayerIdx;
		}

		return result;
	}

	return -1;
}


static int Helper_FindSurvivalNextSpecTarget( int originalSlotIndex, bool reverse )
{
	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pLocalPlayer )
		return originalSlotIndex;

	C_CSPlayer *pOriginalPlayer = ToCSPlayer( UTIL_PlayerByIndex( originalSlotIndex ) );
	if ( !pOriginalPlayer || !pOriginalPlayer->IsAlive() )
		return -1;

	bool bFoundAliveTeammateOfLocalPlayer = false;

	// we are going to produce a deterministic list bucketed by survival team index.
	CUtlVector<C_CSPlayer*> vecSortedPlayers;

	{
		// first gather all players
		CUtlVector<C_CSPlayer*> vecAllPlayers;
		for ( int i = 0; i <= gpGlobals->maxClients; i++ )
		{
			C_CSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );
			if ( pPlayer && pPlayer->IsAlive() )
			{
				vecAllPlayers.AddToTail( pPlayer );

				// if the local player is on a survival team, check if this alive player is a teammate
				if ( pLocalPlayer->m_nSurvivalTeam >= 0 && pLocalPlayer->m_nSurvivalTeam == pPlayer->m_nSurvivalTeam )
				{
					bFoundAliveTeammateOfLocalPlayer = true;
				}
			}
		}

		if ( vecAllPlayers.Count() <= 1 ) //early out
			return originalSlotIndex;

		if ( bFoundAliveTeammateOfLocalPlayer ) // if the local player has an alive teammate, restrict sorted list to only those teammates
		{
			FOR_EACH_VEC( vecAllPlayers, i )
			{
				if ( vecAllPlayers[i]->m_nSurvivalTeam == pLocalPlayer->m_nSurvivalTeam )
				{
					vecSortedPlayers.AddToTail( vecAllPlayers[i] );
				}
			}
		}
		else // otherwise bucket the list by team index. In solo mode this is a no-op.
		{
			FOR_EACH_VEC( vecAllPlayers, i )
			{
				if ( vecAllPlayers[i]->m_nSurvivalTeam < 0 )
				{
					vecSortedPlayers.AddToTail( vecAllPlayers[i] );
				}
				else
				{
					int nInsertBeforeIndex = vecSortedPlayers.Count();
					FOR_EACH_VEC( vecSortedPlayers, j )
					{
						if ( vecSortedPlayers[j]->m_nSurvivalTeam == vecAllPlayers[i]->m_nSurvivalTeam )
						{
							nInsertBeforeIndex = j + 1;
						}
					}
					vecSortedPlayers.InsertBefore( nInsertBeforeIndex, vecAllPlayers[i] );
				}
			}
		}

		if ( vecSortedPlayers.Count() <= 1 ) //early out
			return originalSlotIndex;
	}

	// start with the original slot index player
	int nIndex = vecSortedPlayers.Find( pOriginalPlayer );
	if ( !vecSortedPlayers.IsValidIndex( nIndex ) )
	{
		Assert( false );
		return originalSlotIndex;
	}

	nIndex += ( reverse ? -1 : 1 );
	if ( nIndex < 0 )
	{
		nIndex = MAX( 0, vecSortedPlayers.Count() - 1 );
	}
	else if ( nIndex >= vecSortedPlayers.Count() )
	{
		nIndex = 0;
	}

	C_CSPlayer *pTarget = vecSortedPlayers.Element( nIndex );
	if ( pTarget )
	{
		return pTarget->entindex();
	}

	return originalSlotIndex;
}



int CCSGO_HudTeamCounter::FindNextObserverTargetIndex( bool reverse )
{
	// check to see if we haven't been updated in the last second
	if ( m_flLastSpecListUpdate + 1.0f < gpGlobals->curtime )
	{
		// the list hasn;t been updated recently (probably because the hud element is hidden) so update it now
		UpdateMiniScoreboard();
	}

	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pLocalPlayer )
	{
		return -1;
	}

	int localPlayerIndex = GetLocalPlayerIndex();

	C_CS_PlayerResource* pCSPR = (C_CS_PlayerResource*)g_PR;

	int spectatedTargetIndex = -1;
	if ( GetSpectatorMode() == OBS_MODE_IN_EYE || GetSpectatorMode() == OBS_MODE_CHASE )
	{
		spectatedTargetIndex = GetSpectatorTarget();

		if ( !spectatedTargetIndex )
		{
			spectatedTargetIndex = localPlayerIndex;
		}
	}

	if ( !spectatedTargetIndex )
	{
		return -1;
	}

	if ( CSGameRules() && CSGameRules()->IsPlayingSurvival() )
		return Helper_FindSurvivalNextSpecTarget( spectatedTargetIndex, reverse );

	int controlledPlayer = pCSPR->GetControlledPlayer( spectatedTargetIndex );

	if ( controlledPlayer != 0 )
	{
		spectatedTargetIndex = controlledPlayer;
	}

	int originalSlotIndex = GetPlayerSlotIndex( spectatedTargetIndex );

	if ( originalSlotIndex == -1 )
	{
		return -1;
	}

	bool showEnemy = mp_forcecamera.GetInt() == OBS_ALLOW_ALL;

	if ( ( pLocalPlayer->GetTeamNumber() < TEAM_TERRORIST ) && ( !pLocalPlayer->IsCoach() ) )
	{
		showEnemy = true;
	}

	bool bWantCT = ( pCSPR->GetTeam( spectatedTargetIndex ) == TEAM_CT );

	int result = -1;
	int currentSlotIndex = originalSlotIndex;

	int nMaxPlayers = MAX_TEAM_SIZE * 2;//MIN( g_pGameTypes->GetCurrentServerNumSlots( ), 24 );

	while ( result == -1 )
	{
		if ( reverse )
		{
			currentSlotIndex--;
		}
		else
		{
			currentSlotIndex++;
		}

		if ( currentSlotIndex >= nMaxPlayers )
		{
			currentSlotIndex -= nMaxPlayers;
		}
		else if ( currentSlotIndex < 0 )
		{
			currentSlotIndex += nMaxPlayers;
		}

		if ( currentSlotIndex == originalSlotIndex )
		{
			break;
		}

		const TeamCounterMiniStatus_t *pStatus = GetPlayerStatus( currentSlotIndex );

		if ( pStatus && !pStatus->bDead && ( showEnemy || ( pStatus->bIsCT == bWantCT ) ) )
		{
			result = pCSPR->GetControlledByPlayer( pStatus->nPlayerIdx );
			if ( !result )
			{
				result = pStatus->nPlayerIdx;
			}
		}
	}

	return result;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudTeamCounter::FadeOutSelectedTeam( void )
{
	static const panorama::CPanoramaSymbol k_symFadeIn( "JoinPanel--Animate-FadeIn" );
	static const panorama::CPanoramaSymbol k_symFadeOut( "JoinPanel--Animate-FadeOut" );

	m_pJoinPanel->SetVisible( true );
	m_pJoinPanel->SetHasClass( k_symFadeIn, false );
	m_pJoinPanel->TriggerClass( k_symFadeOut );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudTeamCounter::InitAvatarSnippetSmall( panorama::CPanel2D *pParent, int idx, bool bIsCT )
{
	static const panorama::CPanoramaSymbol k_symAvatarDefaultBGCT( "AvatarS__Default--CT" );
	static const panorama::CPanoramaSymbol k_symAvatarDefaultBGT( "AvatarS__Default--T" );

	char szTmp[ 128 ] = {};

	if ( !m_aPAvatarPanels.IsValidIndex( idx ) )
		return;

	CAvatarPanelData *pAvatar = &m_aPAvatarPanels[ idx ];

	V_sprintf_safe( szTmp, "PAvatar%d", idx );

	pAvatar->m_pPanel = new panorama::CPanel2D( pParent, szTmp );
	pAvatar->m_pPanel->RequireLoadLayoutSnippet( "AvatarSmallSnippet" );
	pAvatar->m_pPanel->SetVisible( false );

	pAvatar->m_pVisible = pAvatar->m_pPanel->RequireChildInLayoutFile( "Visible" );
	pAvatar->m_pInvisible = pAvatar->m_pPanel->RequireChildInLayoutFile( "Invisible" );

	pAvatar->m_pAvatarBG = pAvatar->m_pPanel->RequireChildInLayoutFile( "AvatarS__ImageBG" );
	pAvatar->m_pDynamicAvatar = panorama::panel_cast< CCSGO_AvatarImage * >( pAvatar->m_pPanel->RequireChildInLayoutFile( "AvatarImage" ) );

	pAvatar->m_pHealth = panorama::panel_cast<CCSGO_AvatarHealthBar *>( pAvatar->m_pPanel->RequireChildInLayoutFile( "HealthBar" ) );
	pAvatar->m_pBot = panorama::panel_cast<panorama::CImagePanel *>( pAvatar->m_pPanel->RequireChildInLayoutFile( "Bot" ) );
	pAvatar->m_pSound = pAvatar->m_pPanel->RequireChildInLayoutFile( "Sound" );
	pAvatar->m_pSkull = panorama::panel_cast< panorama::CImagePanel * >( pAvatar->m_pPanel->RequireChildInLayoutFile( "Skull" ) );
	pAvatar->m_pDominated = panorama::panel_cast< panorama::CImagePanel * >( pAvatar->m_pPanel->RequireChildInLayoutFile( "Dominated" ) );
	pAvatar->m_pNemesis = panorama::panel_cast< panorama::CImagePanel * >( pAvatar->m_pPanel->RequireChildInLayoutFile( "Nemesis" ) );

	pAvatar->m_pArsenalProgress = nullptr;
	pAvatar->m_pArsenalProgressText = nullptr;
	pAvatar->m_pArsenalProgressWeapon = nullptr;
	pAvatar->m_pArsenalProgressWeaponIcon = nullptr;

	pAvatar->m_pHealth->SetMin( 0.0f );
	pAvatar->m_pHealth->SetMax( 100.0f );

	// set default background image
	if ( pAvatar->m_pAvatarBG )
	{
		if ( bIsCT )
		{
			pAvatar->m_pAvatarBG->SetHasClass( k_symAvatarDefaultBGCT, true );
		}
		else
		{
			pAvatar->m_pAvatarBG->SetHasClass( k_symAvatarDefaultBGT, true );
		}
	}

	pAvatar->m_bInitialized = true;
}

void CCSGO_HudTeamCounter::InitAvatarSnippetLarge( panorama::CPanel2D *pParent, int idx, bool bIsCT, bool bIsGGProgressive, bool bScaleSmaller )
{
	static const panorama::CPanoramaSymbol k_symAvatarDefaultBGCT( "AvatarL__Default--CT" );
	static const panorama::CPanoramaSymbol k_symAvatarDefaultBGT( "AvatarL__Default--T" );

	static const panorama::CPanoramaSymbol k_symScaleSmaller( "AvatarLargeSnippet--ScaleSmaller" );

	char szTmp[ 128 ] = {};

	if ( !m_aAvatarPanels.IsValidIndex( idx ) )
		return;

	CAvatarPanelData *pAvatar = &m_aAvatarPanels[ idx ];

	V_sprintf_safe( szTmp, "Avatar%d", idx );

	pAvatar->m_pPanel = new panorama::CPanel2D( pParent, szTmp );
	pAvatar->m_pPanel->RequireLoadLayoutSnippet( "AvatarLargeSnippet" );
	pAvatar->m_pPanel->SetVisible( false );

	pAvatar->m_pVisible = pAvatar->m_pPanel->RequireChildInLayoutFile( "Visible" );
	pAvatar->m_pInvisible = pAvatar->m_pPanel->RequireChildInLayoutFile( "Invisible" );

	pAvatar->m_pAvatarBG = pAvatar->m_pPanel->RequireChildInLayoutFile( "AvatarL__ImageBG" );
	pAvatar->m_pDynamicAvatar = panorama::panel_cast<CCSGO_AvatarImage *>( pAvatar->m_pPanel->RequireChildInLayoutFile( "AvatarImage" ) );

	pAvatar->m_pHealth = panorama::panel_cast<CCSGO_AvatarHealthBar *>( pAvatar->m_pPanel->RequireChildInLayoutFile( "HealthBar" ) );
	pAvatar->m_pBot = panorama::panel_cast<panorama::CImagePanel *>( pAvatar->m_pPanel->RequireChildInLayoutFile( "Bot" ) );
	pAvatar->m_pSkull = panorama::panel_cast< panorama::CImagePanel * >( pAvatar->m_pPanel->RequireChildInLayoutFile( "Skull" ) );
	pAvatar->m_pPlayerColor = panorama::panel_cast<panorama::CImagePanel *>( pAvatar->m_pPanel->RequireChildInLayoutFile( "PlayerColor" ) );
	pAvatar->m_pSound = pAvatar->m_pPanel->RequireChildInLayoutFile( "Sound" );
	pAvatar->m_pPlayerLetter = panorama::panel_cast<panorama::CLabel *>( pAvatar->m_pPanel->RequireChildInLayoutFile( "PlayerLetter" ) );
	pAvatar->m_pDominated = panorama::panel_cast<panorama::CImagePanel *>( pAvatar->m_pPanel->RequireChildInLayoutFile( "Dominated" ) );
	pAvatar->m_pNemesis = panorama::panel_cast<panorama::CImagePanel *>( pAvatar->m_pPanel->RequireChildInLayoutFile( "Nemesis" ) );
	pAvatar->m_pArsenalProgress = pAvatar->m_pPanel->RequireChildInLayoutFile( "ArsenalProgress" );

	pAvatar->m_pArsenalProgressText = panorama::panel_cast<panorama::CLabel *>( m_aAvatarPanels[idx].m_pArsenalProgress->RequireChildInLayoutFile( "ArsenalProgressText" ) );

	pAvatar->m_pArsenalProgressWeapon = m_aAvatarPanels[idx].m_pArsenalProgress->RequireChildInLayoutFile( "ArsenalProgressWeapon" );
	pAvatar->m_pArsenalProgressWeaponIcon = panorama::panel_cast<panorama::CImagePanel *>( m_aAvatarPanels[idx].m_pArsenalProgress->RequireChildInLayoutFile( "ArsenalProgressWeaponIcon" ) );

	pAvatar->m_pHealth->SetMin( 0.0f );
	pAvatar->m_pHealth->SetMax( 100.0f );

	pAvatar->m_pPanel->SetHasClass( k_symScaleSmaller, bScaleSmaller );

	// set default background image
	if ( pAvatar->m_pAvatarBG )
	{
		if ( bIsCT )
		{
			pAvatar->m_pAvatarBG->SetHasClass( k_symAvatarDefaultBGCT, true );
		}
		else
		{
			pAvatar->m_pAvatarBG->SetHasClass( k_symAvatarDefaultBGT, true );
		}
	}

	pAvatar->m_bInitialized = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CCSGO_HudTeamCounter::GetPlayerColorLetter( int nSymbolStyle, int nSlot )
{
	if ( nSymbolStyle == 0 )
	{
		if ( nSlot == -1 )
			return "";
		else if ( nSlot == 0 )
			return "Y";
		else if ( nSlot == 1 )
			return "P";
		else if ( nSlot == 2 )
			return "G";
		else if ( nSlot == 3 )
			return "B";
		else if ( nSlot == 4 )
			return "O";
		else if ( nSlot == 10 )
			return "<img src='target_skull.png' height='8'/>";
	}
	else if ( nSymbolStyle == 1 )
	{
		if ( nSlot == -1 )
			return "";
		else if ( nSlot == 0 )
			return "Ⓨ";
		else if ( nSlot == 1 )
			return "Ⓟ";
		else if ( nSlot == 2 )
			return "Ⓖ";
		else if ( nSlot == 3 )
			return "Ⓑ";
		else if ( nSlot == 4 )
			return "Ⓞ";
	}
	else
	{
		if ( nSlot == -1 )
			return "";
		else if ( nSlot == 0 )
			return "★";
		else if ( nSlot == 1 )
			return "✚";
		else if ( nSlot == 2 )
			return "●";
		else if ( nSlot == 3 )
			return "♦";
		else if ( nSlot == 4 )
			return "■";
	}

	return "";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudTeamCounter::HideDisplayTeamPanels( void )
{
	m_pJoinPanel->SetVisible( false );

	m_pJoinPanelBot->SetVisible( false );
	m_pJoinPanelCT->SetVisible( false );
	m_pJoinPanelT->SetVisible( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudTeamCounter::HideTimer( void )
{
	m_pTime->SetVisible( false );

	// TODO
/*	PTime.GlowPulse._visible = false;*/
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudTeamCounter::InvokeAvatarSlotUpdate( sAvatarInitData &avatarData, const TeamCounterMiniStatus_t* ms, int slotNumber )
{
	if ( !ms )
		return;

	avatarData.m_nSlot = slotNumber;

	avatarData.m_XUID = ms->nXUID;

	bool bGGProgressive = ( ms->nGGProgressiveRank >= 0 );
	bool bDeathmatch = CSGameRules() && CSGameRules()->IsPlayingGunGameDeathmatch() && !CSGameRules()->IsPlayingTeamDM();

	int nTeam = ms->bIsCT ? TEAM_CT : TEAM_TERRORIST;
	bool bIsLeader = false;
	if ( CSGameRules()->IsPlayingGunGameProgressive() )
		bIsLeader = ( ms->nEntIdx == GetGlobalTeam( nTeam )->GetGGLeader( nTeam ) );

	// Pack all the flags as a bitfield to send to Scaleform
	int flags = 0;
	flags |= ( ( 0x1 & ms->bIsCT ) << 0 );
	flags |= ( ( 0x1 & ms->bLocalPlayer ) << 1 );
	flags |= ( ( 0x1 & ms->bDead ) << 2 );
	flags |= ( ( 0x1 & ms->bDominated ) << 3 );
	flags |= ( ( 0x1 & ms->bDominating ) << 4 );
	flags |= ( ( 0x1 & ms->bSpeaking ) << 5 );
	flags |= ( ( 0x1 & ms->bPlayerBot ) << 6 );
	flags |= ( ( 0x1 & ms->bSpectated ) << 7 );
	flags |= ( ( 0x1 & bGGProgressive ) << 8 );
	flags |= ( ( 0x1 & bIsLeader ) << 9 );

	avatarData.m_nFlags = flags;

	avatarData.m_pszPlayerName = g_PR->GetPlayerName( ms->nPlayerIdx );

	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	// set health for all spectators and gotv clients
	bool bShowHealth = CanSeeSpectatorOnlyTools();
	if ( bShowHealth == false && pLocalPlayer )
	{
		C_CSPlayer *pOtherPlayer = ToCSPlayer( UTIL_PlayerByIndex( ms->nPlayerIdx ) );
		if ( pOtherPlayer && !pOtherPlayer->IsDormant() )
		{
			if ( !pLocalPlayer->IsOtherEnemy( ms->nPlayerIdx ) || ( !pLocalPlayer->IsAlive() && mp_forcecamera.GetInt() == OBS_ALLOW_ALL ) )
				bShowHealth = true;
		}
	}

	avatarData.m_nHealth = bShowHealth ? ms->nHealth : 0;
	avatarData.m_nArmor = bShowHealth ? ms->nArmor : 0;

	//int nReverseLevel = CSGameRules()->GetNumProgressiveGunGameWeapons( ms->bIsCT ? TEAM_CT : TEAM_TERRORIST ) - ms->nGunGameLevel;
	int nLevel = 0;
	if ( bDeathmatch )
		nLevel = ms->nPoints;
	else if ( bGGProgressive )
		nLevel = ms->nGunGameLevel;

	avatarData.m_nLevel = nLevel;
	avatarData.m_szWeaponURL[0] = '\0'; // default to ""

	if ( CSGameRules() && CSGameRules()->IsPlayingGunGameProgressive() )
	{
		const CEconItemDefinition* pWeaponDef = CSGameRules()->GetCurrentGunGameWeapon( ms->nGunGameLevel, ms->bIsCT ? TEAM_CT : TEAM_TERRORIST );
		if ( pWeaponDef && pWeaponDef->GetDefinitionIndex() != 0 )
		{
			if ( const char *szWeaponName = pWeaponDef->GetDefinitionName() )
			{
				// strip the weapon_* from the weapon name
				if ( IsWeaponClassname( szWeaponName ) )
				{
					szWeaponName += WEAPON_CLASSNAME_PREFIX_LENGTH;
				}
				if ( StringHasPrefix( szWeaponName, "knifegg" ) )
				{
					szWeaponName = "knife";
				}

				V_sprintf_safe( avatarData.m_szWeaponURL, "file://{images}/icons/equipment/%s.svg", szWeaponName );
			}
		}
	}

	avatarData.m_nTeammateColor = ms->nTeammateColor;

	bool bShowLetter = false;
	if ( pLocalPlayer && pLocalPlayer->ShouldShowTeamPlayerColors( ms->bIsCT ? TEAM_CT : TEAM_TERRORIST ) )
		bShowLetter = pLocalPlayer->ShouldShowTeamPlayerColorLetters();

	avatarData.m_bShowLetter = bShowLetter;

	UpdateAvatarSlot( avatarData );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCSGO_HudTeamCounter::IsPlayerCountVisible()
{
	m_bShowOnlyPlayerCount = cl_hud_playercount_showcount.GetBool();

	return ( ( m_bIsGunGame == false ) && ( m_bShowOnlyPlayerCount == true ) );
}

bool CCSGO_HudTeamCounter::IsPlayerCountVisibleForCT()
{
	return IsPlayerCountVisible();
}

bool CCSGO_HudTeamCounter::IsPlayerCountVisibleForT()
{
	if ( CSGameRules() && CSGameRules()->IsPlayingCoopMission() )
	{
		return true;
	}
	else
	{
		return IsPlayerCountVisible();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudTeamCounter::LevelInit()
{
	ShowPanel( false );

	m_ggSortedList.RemoveAll();

	m_bTimerAlertTriggered = false;
	SetTimerRedColor( false );
	m_nLastTimeSet = TIMER_STATE_UNSET;

	m_bTimerHidden = false;
	m_nTScoreLastUpdate = -1;
	m_nCTScoreLastUpdate = -1;

	m_bRoundStarted = false;

	m_bPositionInitialised = false;

	// check if the round is already in progress when we first join it
	C_CSGameRules *pRules = CSGameRules();
	if ( pRules )
	{
		m_bRoundStarted = ( pRules->GetRoundStartTime() < gpGlobals->curtime );
	}

	// Reset all player tracking
	for ( int idx = 0; idx < MAX_TEAM_SIZE; ++idx )
	{
		m_CTTeam[ idx ].Reset();
		m_TerroristTeam[ idx ].Reset();
	}
	for ( int idx = 0; idx < MAX_GGPROG_PLAYERS; ++idx )
	{
		m_GGProgressivePlayers[ idx ].Reset();
	}
	m_nTerroristTeamCount = 0;
	m_nCTTeamCount = 0;
	m_nPreviousGGProgressiveTotalPlayers = 0;

	// game mode
	if ( CSGameRules() && ( CSGameRules()->IsPlayingGunGameProgressive() || CSGameRules()->IsPlayingGunGameDeathmatch() ) && !CSGameRules()->IsPlayingTeamDM() )
	{
		SetViewMode( VIEW_MODE_GUN_GAME_PROGRESSIVE );
	}
	else if ( CSGameRules() && CSGameRules()->IsPlayingGunGameTRBomb() )
	{
		SetViewMode( VIEW_MODE_GUN_GAME_BOMB );
		m_nLeaderWeaponRank = -1;
		g_GGProgLeaderPlayerIdx = -1;
	}
	else
	{
		SetViewMode( VIEW_MODE_NORMAL );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudTeamCounter::LevelShutdown( void )
{
	PurgeAvatars();

	ShowPanel( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudTeamCounter::ProcessInput( void )
{
	// update elements affected by position (based on cl_count_playercount_pos)
	UpdatePosition();

	// update game clock
	UpdateTimer();

	// update scores
	UpdateScore();

	// update team list (mini-scoreboard)
	UpdateMiniScoreboard();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudTeamCounter::PurgeAvatars()
{
	for ( int idx = 0; idx < MAX_AVATAR_PANELS; idx++ )
	{
		m_aAvatarPanels[ idx ].m_savedXUID = 0;

		if ( m_aAvatarPanels[ idx ].m_pPanel )
		{
			m_aAvatarPanels[ idx ].m_pPanel->DeleteAsync( 0.0f );
			m_aAvatarPanels[ idx ].m_pPanel = NULL;

			V_memset( &m_aAvatarPanels[idx], 0, sizeof( CAvatarPanelData ) );
		}
	}

	for ( int idx = 0; idx < MAX_PAVATAR_PANELS; idx++ )
	{
		m_aPAvatarPanels[ idx ].m_savedXUID = 0;

		if( m_aPAvatarPanels[idx].m_pPanel )
		{
			m_aPAvatarPanels[ idx ].m_pPanel->DeleteAsync( 0.0f );
			m_aPAvatarPanels[ idx ].m_pPanel = NULL;

			V_memset( &m_aPAvatarPanels[idx], 0, sizeof( CAvatarPanelData ) );
		}
	}

	m_nMaxPlayers = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudTeamCounter::ResetLeader()
{
	m_nLeaderWeaponRank = -1;
	g_GGProgLeaderPlayerIdx = -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudTeamCounter::SetActive( bool bActive )
{
	if ( bActive != m_bActive )
	{
		ShowPanel( bActive );
	}

	CPanoramaHudElement::SetActive( bActive );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudTeamCounter::SetBombDefused()
{
	m_pTime->SetVisible( false );

	m_pBombPlanted->SetVisible( false );
	m_pBombPlantedLines->SetVisible( false );
	m_pBombDefused->SetVisible( true );
}

//-----------------------------------------------------------------------------
// Purpose: Setup small avatars 
// 24 avatars visible across two teams of 12 in two rows of 6, split either side of the score/time panel
//-----------------------------------------------------------------------------
void CCSGO_HudTeamCounter::SetModeGunGameBomb()
{
	m_nMaxPlayers = 24;
	m_bIsGunGame = false;

	int idxTOffset = MAX_PAVATAR_PANELS / 2;

	// small avatar panels
	for ( int idx = 0; idx < idxTOffset; idx++ )
	{
		// CT
		InitAvatarSnippetSmall( ( idx & 1 ) ? m_pTeamSmallCTRow1 : m_pTeamSmallCTRow2, idx, true ); // CT filled opposite way around to T

		// T
		InitAvatarSnippetSmall( ( idx & 1 ) ? m_pTeamSmallTRow2 : m_pTeamSmallTRow1, idx + idxTOffset, false );
	}

	SetTimerVisibility( true );
	m_pGameScoreContainer->SetVisible( true );
	m_pTeamSmallCTContainer->SetVisible( true );
	m_pTeamSmallTContainer->SetVisible( true );

	// TODO
/*	Panel.PanelStroke._visible = true;	 */
}

//-----------------------------------------------------------------------------
// Purpose:  Setup large avatars 
// 10 avatars visible across two teams of 5 in a single row, split either side of the score/time panel
//-----------------------------------------------------------------------------
void CCSGO_HudTeamCounter::SetModeGunGameBombTen()
{
	m_nMaxPlayers = 10;
	m_bIsGunGame = false;

	int idxTOffset = MAX_AVATAR_PANELS / 2;

	// large avatar panels
	for ( int idx = 0; idx < idxTOffset; idx++ )
	{
		// CT
		InitAvatarSnippetLarge( m_pTeamLargeCT, idx, true, false, true );

		// T
		InitAvatarSnippetLarge( m_pTeamLargeT, idx + idxTOffset, false, false, true );
	}

	SetTimerVisibility( true );
	m_pGameScoreContainer->SetVisible( true );
	m_pTeamLargeCT->SetVisible( true );
	m_pTeamLargeT->SetVisible( true );

	// TODO
/*	Panel.PanelStroke._visible = true;	 */
}

//-----------------------------------------------------------------------------
// Purpose: Setup GGProg10
// 10 avatars visible as a single row (time/score panel not visible in this mode)
//-----------------------------------------------------------------------------
void CCSGO_HudTeamCounter::SetModeGunGameProgressive()
{
	m_nMaxPlayers = 10;
	m_bIsGunGame = true;


	// large avatar panels
	for ( int idx = 0; idx < MAX_AVATAR_PANELS; idx++ )
	{
		InitAvatarSnippetLarge( m_pTeamAll, idx, true, true, false );
	}

	m_pTeamAll->SetVisible( true );

	if ( CSGameRules()->IsPlayingGunGameDeathmatch() )
	{
		SetTimerVisibility( true, false );
	}

	// TODO
/*	Panel.PanelStroke._visible = false;*/
}

//-----------------------------------------------------------------------------
// Purpose: Setup small avatars
// 24 avatars visible across two teams of 12 in two rows of 6, split either side of the score/time panel
//-----------------------------------------------------------------------------
void CCSGO_HudTeamCounter::SetModeNormal()
{
	m_nMaxPlayers = 24;
	m_bIsGunGame = false;

	int idxTOffset = MAX_PAVATAR_PANELS / 2;

	// small avatar panels
	for ( int idx = 0; idx < idxTOffset; idx++ )
	{
		// CT
		InitAvatarSnippetSmall( ( idx & 1 ) ? m_pTeamSmallCTRow1 : m_pTeamSmallCTRow2, idx, true );  // CT filled opposite way around to T

		// T
		InitAvatarSnippetSmall( ( idx & 1 ) ? m_pTeamSmallTRow2 : m_pTeamSmallTRow1, idx + idxTOffset, false );
	}

	SetTimerVisibility( true );
	m_pGameScoreContainer->SetVisible( true );
	m_pTeamSmallCTContainer->SetVisible( true );
	m_pTeamSmallTContainer->SetVisible( true );

	// TODO
/*	Panel.PanelStroke._visible = true;*/
}

//-----------------------------------------------------------------------------
// Purpose: Setup large avatars
// 10 avatars visible across two teams of 5 in a single row, split either side of the score/time panel
//-----------------------------------------------------------------------------
void CCSGO_HudTeamCounter::SetModeNormalTen()
{
	m_nMaxPlayers = 10;
	m_bIsGunGame = false;

	int idxTOffset = MAX_AVATAR_PANELS / 2;

	// large avatar panels
	for ( int idx = 0; idx < idxTOffset; idx++ )
	{
		// CT
		InitAvatarSnippetLarge( m_pTeamLargeCT, idx, true, false, true );

		// T
		InitAvatarSnippetLarge( m_pTeamLargeT, idx + idxTOffset, false, false, true );
	}

	SetTimerVisibility( true );
	m_pGameScoreContainer->SetVisible( true );
	m_pTeamLargeCT->SetVisible( true );
	m_pTeamLargeT->SetVisible( true );

	// TODO
/*	Panel.PanelStroke._visible = true;*/
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudTeamCounter::SetNumPlayersAlive( int nPlayersAlive_CT, int nPlayersAlive_T )
{
	m_nPlayersAlive_CT = nPlayersAlive_CT;
	m_nPlayersAlive_T = nPlayersAlive_T;

	UpdateNumberCount();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudTeamCounter::SetViewMode( VIEW_MODE mode )
{
	Assert( mode >= VIEW_MODE_NORMAL && mode < VIEW_MODE_NUM );

	m_Mode = mode;

	static const panorama::CPanoramaSymbol k_symGameModeGunGameProgressive( "GameModeGunGameProgressive" );
	bool bGunGameProgressive = m_Mode == VIEW_MODE_GUN_GAME_PROGRESSIVE;
	m_pTeamCounterPanel->SetHasClass( k_symGameModeGunGameProgressive, bGunGameProgressive );

	SetTimerVisibility( false );
	m_pGameScoreContainer->SetVisible( false );
	m_pTeamAll->SetVisible( false );
	m_pTeamLargeCT->SetVisible( false );
	m_pTeamLargeT->SetVisible( false );
	m_pTeamSmallCTContainer->SetVisible( false );
	m_pTeamSmallTContainer->SetVisible( false );

	// show ten if we are forcing this on the server
	int nMaxPlayers = CCSGameRules::GetMaxPlayers();

	if ( bGunGameProgressive )
	{
		SetModeGunGameProgressive();
	}
	else if ( m_Mode == VIEW_MODE_GUN_GAME_BOMB )
	{
		if ( nMaxPlayers > 10 )
			SetModeGunGameBomb();
		else
			SetModeGunGameBombTen();
	}
	else
	{
		if ( nMaxPlayers > 10 )
			SetModeNormal();
		else
			SetModeNormalTen();


		UpdateScore();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------

extern bool IsTakingAFreezecamScreenshot();
extern ConVar cl_drawhud;

bool CCSGO_HudTeamCounter::ShouldDraw( void )
{
	if ( IsTakingAFreezecamScreenshot()
		|| ( CSGameRules() && CSGameRules()->IsPlayingTraining() ) 
		|| ( CSGameRules() && CSGameRules()->IsPlayingSurvival() ) 
		)
		return false;

	return cl_drawhud.GetBool() && cl_draw_only_deathnotices.GetBool() == false && CHudElement::ShouldDraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudTeamCounter::ShowPanel( const bool bShow )
{
	static const panorama::CPanoramaSymbol k_symFadeIn( "JoinPanel--Animate-FadeIn" );
	static const panorama::CPanoramaSymbol k_symFadeOut( "JoinPanel--Animate-FadeOut" );

	if ( m_bIsGunGame && m_bIsShowingTimer )
		UpdateTotalProgressivePlayers( m_nLastGGPlayerCount, m_bIsShowingTimer );

	//TODO - HudBackgroundUpdate();

	SetVisible( bShow );

	if ( bShow )
	{
	}
	else
	{
		m_pJoinPanel->SetVisible( false );
		m_pJoinPanel->SetHasClass( k_symFadeIn, false );
		m_pJoinPanel->SetHasClass( k_symFadeOut, false );

		m_pJoinPanelBot->SetVisible( false );
		m_pJoinPanelCT->SetVisible( false );
		m_pJoinPanelT->SetVisible( false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudTeamCounter::SpawnDisplaySelectedTeam( int team )
{
	static const panorama::CPanoramaSymbol k_symFadeIn( "JoinPanel--Animate-FadeIn" );
	static const panorama::CPanoramaSymbol k_symFadeOut( "JoinPanel--Animate-FadeOut" );

	m_pJoinPanelBot->SetVisible( false );
	m_pJoinPanel->SetVisible( false );

	if ( team == 3 )
	{
		m_pJoinPanel->SetVisible( true );
		m_pJoinPanel->TriggerClass( k_symFadeIn );
		m_pJoinPanel->SetHasClass( k_symFadeOut, false );

		m_pJoinPanelCT->SetVisible( true );
		m_pJoinPanelT->SetVisible( false );
	}
	else if ( team == 2 )
	{
		m_pJoinPanel->SetVisible( true );
		m_pJoinPanel->TriggerClass( k_symFadeIn );
		m_pJoinPanel->SetHasClass( k_symFadeOut, false );

		m_pJoinPanelCT->SetVisible( false );
		m_pJoinPanelT->SetVisible( true );
	}
	else
	{
		m_pJoinPanelCT->SetVisible( false );
		m_pJoinPanelT->SetVisible( false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudTeamCounter::TakeOverBot( const char *szBotName )
{
	static const panorama::CPanoramaSymbol k_symFadeIn( "JoinPanel--Animate-FadeIn" );
	static const panorama::CPanoramaSymbol k_symFadeOut( "JoinPanel--Animate-FadeOut" );

	m_pJoinPanel->SetVisible( true );
	m_pJoinPanel->TriggerClass( k_symFadeIn );
	m_pJoinPanel->SetHasClass( k_symFadeOut, false );

	m_pJoinPanelCT->SetVisible( false );
	m_pJoinPanelT->SetVisible( false );
	m_pJoinPanelBot->SetVisible( true );

	m_pJoinTextBot->SetDialogVariable( "s1", szBotName );
	m_pJoinTextBot->SetText( CFmtStr( "#SFUI_Notice_Hint_Bot_Takeover" ).String(), panorama::CLabel::k_ETextTypeHTML );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudTeamCounter::UpdateAvatarSlot( sAvatarInitData &avatarData )
{
	static const panorama::CPanoramaSymbol k_symFadeIn( "JoinPanel--Animate-FadeIn" );
	static const panorama::CPanoramaSymbol k_symFadeOut( "JoinPanel--Animate-FadeOut" );

	static const panorama::CPanoramaSymbol k_symGGTextPlayer( "AvatarL__GGText--Player" );
	static const panorama::CPanoramaSymbol k_symGGTextBot( "AvatarL__GGText--Bot" );

	static const panorama::CPanoramaSymbol k_symAvatarBorderCT( "Avatar__PlayerBorder--CT" );
	static const panorama::CPanoramaSymbol k_symAvatarBorderT( "Avatar__PlayerBorder--T" );

	static const panorama::CPanoramaSymbol k_symAvatarOutlineWhite( "Avatar__PlayerOutline--White" );
	static const panorama::CPanoramaSymbol k_symAvatarOutlineSpectate( "Avatar__PlayerOutline--Spectate" );
	static const panorama::CPanoramaSymbol k_symAvatarOutlineCT( "Avatar__PlayerOutline--CT" );
	static const panorama::CPanoramaSymbol k_symAvatarOutlineT( "Avatar__PlayerOutline--T" );

	static const panorama::CPanoramaSymbol k_symAvatarHealthNormal( "Avatar__HealthBar--Normal" );
	static const panorama::CPanoramaSymbol k_symAvatarHealthRed( "Avatar__HealthBar--Red" );

	static const panorama::CPanoramaSymbol k_symAvatarSkullRed( "Avatar__Skull--Red" );
	static const panorama::CPanoramaSymbol k_symAvatarSkullCT( "Avatar__Skull--CT" );
	static const panorama::CPanoramaSymbol k_symAvatarSkullT( "Avatar__Skull--T" );

	static const panorama::CPanoramaSymbol k_symAvatarBotCT( "Avatar__Bot--CT" );
	static const panorama::CPanoramaSymbol k_symAvatarBotT( "Avatar__Bot--T" );

 	static const panorama::CPanoramaSymbol k_symAvatarBGHidden( "Avatar__BG--Hidden" );


	int slot = avatarData.m_nSlot;

	if ( ( slot < 0 ) || ( slot > ( m_nMaxPlayers - 1 ) ) )
	{
		DebugMsg( "INVALID SLOT# : %d Passed to UpdateAvatarSlot.  Aborting.", slot );
		return;
	}

	int flags = avatarData.m_nFlags;

	// Unpack the bitfield
	bool bCT		= ( 0x1 & ( flags >> 0 ) ) != 0;
	bool bIsLocalPlayer	= ( 0x1 & ( flags >> 1 ) ) != 0;
	bool bDead		= ( 0x1 & ( flags >> 2 ) ) != 0;
	bool bDominated	= ( 0x1 & ( flags >> 3 ) ) != 0;
	bool bNemesis 	= ( 0x1 & ( flags >> 4 ) ) != 0;
	bool bSpeaking 	= ( 0x1 & ( flags >> 5 ) ) != 0;
	bool bPlayerBot	= ( 0x1 & ( flags >> 6 ) ) != 0;
	bool bSpectate	= ( 0x1 & ( flags >> 7 ) ) != 0;
	bool bGGProg	= ( 0x1 & ( flags >> 8 ) ) != 0;
	bool bTeamLeader = ( 0x1 & ( flags >> 9 ) ) != 0;

	static bool bPlayerIsCT = false;

	// Remember whether the player was a CT or not, to use later on for tinting Skulls
	if ( bIsLocalPlayer && !bDead )
	{
		bPlayerIsCT = bCT;
	}

	int nHalfPlayers = m_nMaxPlayers / 2;

	CAvatarPanelData *pAvatarPanelData;

	int nAvatarNameSlot = -1;
	int idx;

	if ( bGGProg )
	{
		idx = slot;
	}
	else
	{
		// Offset to the next available team-based slot
		if ( bCT )
		{
			nAvatarNameSlot = ( ( nHalfPlayers - 1 ) - slot );
		}
		else
		{
			nAvatarNameSlot = ( nHalfPlayers + slot );
		}
		idx = nAvatarNameSlot;
	}

	bool bLargeAvatar;
	if ( m_nMaxPlayers > MAX_AVATAR_PANELS )
	{
		if ( !m_aPAvatarPanels.IsValidIndex( idx ) )
			return;

		bLargeAvatar = false;
		pAvatarPanelData = &m_aPAvatarPanels[ idx ];
	}
	else
	{
		if ( !m_aAvatarPanels.IsValidIndex( idx ) )
			return;

		bLargeAvatar = true;
		pAvatarPanelData = &m_aAvatarPanels[ idx ];
	}

	if ( !pAvatarPanelData->m_pPanel || !pAvatarPanelData->m_bInitialized )
		return;

	bool bWasAlreadyDead = pAvatarPanelData->m_bDead;

	pAvatarPanelData->m_pPanel->SetVisible( true );

	if ( m_bForceAvatarRefresh )
	{
		pAvatarPanelData->m_savedXUID = 0;
	}

	bool bVisible = false;
	
	if ( bLargeAvatar )
	{
		if ( cl_show_enemy_avatar_colors.GetBool() || bPlayerIsCT == bCT )
		{
			int nColorID = avatarData.m_nTeammateColor;

			if ( avatarData.m_nTeammateColor > -1 )
				nColorID = ( avatarData.m_nTeammateColor % 5 );

			if ( ( nColorID != -1 ) && ( pAvatarPanelData->m_bDead == false ) )
				bVisible = true;

			if ( bVisible )
			{
				C_CS_PlayerResource* pCSPR = ( C_CS_PlayerResource* )g_PR;
				if ( pCSPR )
				{
					panorama::IUIPanelStyle *pPanelStyle = pAvatarPanelData->m_pPlayerColor->AccessStyle();
					pPanelStyle->SetSimpleWashColor( pCSPR->GetCompPlayerColorByID( nColorID ) );
				}

				if ( avatarData.m_bShowLetter )
				{
					pAvatarPanelData->m_pPlayerLetter->SetText( GetPlayerColorLetter( 0, nColorID ), panorama::CLabel::k_ETextTypeHTML );
				}
			}
		}

		if ( m_bShowOnlyPlayerCount && !m_bIsGunGame )
		{
			pAvatarPanelData->SetChildVisible( pAvatarPanelData->m_pPlayerColor, false );
			pAvatarPanelData->SetChildVisible( pAvatarPanelData->m_pPlayerLetter, false );
			m_pTeamLargeCT->SetVisible( false );
			m_pTeamLargeT->SetVisible( false );
		}
		else
		{
			pAvatarPanelData->SetChildVisible( pAvatarPanelData->m_pPlayerColor, bVisible );
			pAvatarPanelData->SetChildVisible( pAvatarPanelData->m_pPlayerLetter, avatarData.m_bShowLetter && bVisible );
			m_pTeamLargeCT->SetVisible( true );
			m_pTeamLargeT->SetVisible( true );
		}
	}

	bool bOnlyShowNumbers = bCT ? IsPlayerCountVisibleForCT() : IsPlayerCountVisibleForT();

	if ( bOnlyShowNumbers )
	{
		if ( ( m_nMaxPlayers > 10 ) || (nAvatarNameSlot != -1 && nAvatarNameSlot != 4 && nAvatarNameSlot != 5) )
		{
			pAvatarPanelData->m_pPanel->SetVisible( false );
			return;
		}

		bIsLocalPlayer = false;
		bDead		= false;
		bDominated 	= false;
		bNemesis 	= false;
		bSpeaking 	= false;
		bPlayerBot	= false;
		bSpectate	= false;
		bGGProg		= false;
	}

	// background & border (just background for now - but to be removed entirely soon
	pAvatarPanelData->SetBGForTeam( bCT, !bLargeAvatar );
	panorama::CPanoramaSymbol symAvatarBorder = bCT ? k_symAvatarBorderCT : k_symAvatarBorderT;

	pAvatarPanelData->m_pAvatarBG->SetVisible( !bDead );

	// Outline the local player while alive, or while we are playing as a bot

	bool bPlayerOutlineVisible = ( bIsLocalPlayer && !bDead ) || ( bPlayerBot );
	bool bOutlineColorSet = false;

	if ( !bDead )
	{
		if ( bTeamLeader && ( bPlayerOutlineVisible == false ) )
		{
			bPlayerOutlineVisible = true;
			symAvatarBorder = k_symAvatarOutlineWhite;

			if ( bCT )
			{
				symAvatarBorder = k_symAvatarOutlineCT;
			}
			else
			{
				symAvatarBorder = k_symAvatarOutlineT;
			}
			bOutlineColorSet = true;
		}

		if ( bPlayerOutlineVisible )
		{
			if ( !bOutlineColorSet )
			{
				symAvatarBorder = k_symAvatarOutlineWhite;
			}

			if ( bIsLocalPlayer && bTeamLeader && bGGProg )
			{
				if ( !m_pJoinPanelBot->BIsVisible() )
				{
					//.as TopPanel.Panel.PlayerJoinPanel.gotoAndPlay( "FadeIn" );
					m_pJoinPanelBot->SetVisible( true );
					m_pJoinPanel->SetVisible( true );
					m_pJoinPanel->TriggerClass( k_symFadeIn );
					m_pJoinPanel->SetHasClass( k_symFadeOut, false );
				}

				m_pJoinPanelCT->SetVisible( false );
				m_pJoinPanelT->SetVisible( false );

				m_pJoinTextBot->SetText( "#SFUI_Player_Is_Leader", panorama::CLabel::k_ETextTypeHTML );
			}

			if ( bIsLocalPlayer )
			{
				m_pJoinPanelBot->SetVisible( bTeamLeader && bGGProg );
			}
		}

		if ( bSpectate )
		{
			// add spectate border style
			symAvatarBorder = k_symAvatarOutlineSpectate;
		}
	}

	pAvatarPanelData->SetBorderType( symAvatarBorder );

	pAvatarPanelData->m_pHealth->SetVisible( !bOnlyShowNumbers );
	pAvatarPanelData->m_pHealth->SetValue( (float)( avatarData.m_nHealth ) );
	pAvatarPanelData->SetHealthBarType( ( avatarData.m_nHealth > 20 ) ? k_symAvatarHealthNormal : k_symAvatarHealthRed );

	// Save this to determine if this is the first frame with the skulls on
	bool bSkullAlreadyOn = !pAvatarPanelData->SetChildVisible( pAvatarPanelData->m_pSkull, bDead && !bPlayerBot );

	// Domination/nemesis status
	// When we're dead, that trumps the dominated/nemesis flags
	pAvatarPanelData->SetChildVisible( pAvatarPanelData->m_pDominated, !bDead && bDominated );
	pAvatarPanelData->SetChildVisible( pAvatarPanelData->m_pNemesis, !bDead && !bDominated && bNemesis );

	if ( ( bDead && !bPlayerBot ) && !bSkullAlreadyOn ) 
	{
		// Skulls: display based on team and whether they are local player's teammate

		if ( bPlayerIsCT == bCT )
		{
			// teammates get a bright red icon
			pAvatarPanelData->SetSkullType( k_symAvatarSkullRed );
		}
		else
		{
			pAvatarPanelData->SetSkullType( bCT ? k_symAvatarSkullCT : k_symAvatarSkullT );
		}

		if ( !bSkullAlreadyOn || !bWasAlreadyDead )
		{
			// TODO - fade in
			//Avatar.Skull.gotoAndPlay( "StartFade" );
		}
	}

	// Indicates a player playing as a bot (bot-takeover)
	pAvatarPanelData->SetChildVisible( pAvatarPanelData->m_pBot, bPlayerBot );
	if ( bPlayerBot )
	{
		pAvatarPanelData->SetBotType( bCT ? k_symAvatarBotCT : k_symAvatarBotT );
	}

	// Indicates speaking status
	if ( pAvatarPanelData->m_pSound )
	{
		//pAvatarPanelData->m_pSound->SetVisible( bSpeaking );
		pAvatarPanelData->SetChildVisible( pAvatarPanelData->m_pSound, bSpeaking );
	}

	// avatar image
	if ( avatarData.m_XUID != 0 )
	{
		// If this is a new XUID, we need to load the new avatar image now
		if (pAvatarPanelData->m_savedXUID != avatarData.m_XUID )
		{
			pAvatarPanelData->m_pDynamicAvatar->SetSteamID( avatarData.m_XUID );

			// disable default background image
			pAvatarPanelData->m_pAvatarBG->SetHasClass( k_symAvatarBGHidden, true );
		}

		pAvatarPanelData->m_pDynamicAvatar->SetVisible( !bOnlyShowNumbers );
	}
	else
	{
		pAvatarPanelData->m_pDynamicAvatar->SetVisible( false );
		pAvatarPanelData->m_pAvatarBG->SetHasClass( k_symAvatarBGHidden, false );
	}

	//if ( pAvatarPanelData->m_pDefaultAvatarCT->BIsVisible() )
	{
		// TODO
		/*if ( Avatar.DefaultAvatarCT["transformData"] == undefined )
		{
			var avatarTransform = new Color( Avatar.DefaultAvatarCT );
			Avatar.DefaultAvatarCT["transformData"] = avatarTransform;
		}

		Avatar.DefaultAvatarCT["transformData"].setTransform( ( bDead && !bPlayerBot) ? avatarDeadXform : avatarAliveXform );
		*/
	}

	//if ( pAvatarPanelData->m_pDefaultAvatarT->BIsVisible() )
	{
		// TODO
		/*if ( Avatar.DefaultAvatarT["transformData"] == undefined )
		{
			var avatarTransform = new Color( Avatar.DefaultAvatarT );
			Avatar.DefaultAvatarT["transformData"] = avatarTransform;
		}

		Avatar.DefaultAvatarT["transformData"].setTransform( ( bDead && !bPlayerBot) ? avatarDeadXform : avatarAliveXform );
		*/
	}

	if ( pAvatarPanelData->m_pDynamicAvatar->BIsVisible() )
	{
		// TODO
		/*if ( Avatar.DynamicAvatar["transformData"] == undefined )
		{
			var avatarTransform = new Color( Avatar.DynamicAvatar );
			Avatar.DynamicAvatar["transformData"] = avatarTransform;
		}

		if ( bCT )
			Avatar.DynamicAvatar["transformData"].setTransform( ( bDead && !bPlayerBot) ? avatarDeadXform : avatarAliveXform_CT );
		else
			Avatar.DynamicAvatar["transformData"].setTransform( ( bDead && !bPlayerBot) ? avatarDeadXform : avatarAliveXform_T );
		*/
	}

	if ( avatarData.m_nLevel == 0 )
	{
		if ( pAvatarPanelData->m_pArsenalProgress )
			pAvatarPanelData->m_pArsenalProgress->SetVisible( false );
	}
	else if ( pAvatarPanelData->m_pArsenalProgress )
	{
		pAvatarPanelData->m_pArsenalProgress->SetVisible( true );

		// default 5c5c5c
		// local player ffffff
		// winner fff000

		pAvatarPanelData->m_pArsenalProgressText->SetText( CFmtStr( "%d", avatarData.m_nLevel ), panorama::CLabel::k_ETextTypeHTML );
		if ( bIsLocalPlayer )
		{
			pAvatarPanelData->m_pArsenalProgressText->SetHasClass( k_symGGTextPlayer, true );
			pAvatarPanelData->m_pArsenalProgressText->SetHasClass( k_symGGTextBot, false );
		}
		else
		{
			pAvatarPanelData->m_pArsenalProgressText->SetHasClass( k_symGGTextPlayer, false );
			pAvatarPanelData->m_pArsenalProgressText->SetHasClass( k_symGGTextBot, true );
		}

		if ( avatarData.m_szWeaponURL[0] == 0 ) // no url
		{
			pAvatarPanelData->m_pArsenalProgressWeapon->SetVisible( false );
		}
		else
		{
			pAvatarPanelData->m_pArsenalProgressWeapon->SetVisible( true );

			// JS version avoids resetting reload params
			pAvatarPanelData->m_pArsenalProgressWeaponIcon->SetImageJS( avatarData.m_szWeaponURL );
		}
	}

	// Remember what XUID we used on this slot
	pAvatarPanelData->m_savedXUID = avatarData.m_XUID;

	pAvatarPanelData->m_bDead = bDead;
	pAvatarPanelData->m_bWasPlayerBot = bPlayerBot;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudTeamCounter::UpdateLeaderWeaponVisibility( int nSlot, bool bShowWeapon )
{
	// TODO
	/*	if ( Slot < 0 || Slot > 9 )
	{
	trace( "INVALID SLOT# :" + Slot + " Passed to UpdateAvatarSlot.  Aborting." );
	return;
	}

	var AvatarName = "Avatar" + Slot;
	var Avatar = TopPanel.Panel[ AvatarName ];
	if ( Avatar == undefined )
	{
	// Error! We don't have a slot for this avatar name
	trace( "Error! We don't have a slot for this avatar name.  Aborting for slot = " + Slot );
	return;
	}

	//trace( "slot =" + Slot + ", show weapon = " + bShowWeapon );
	Avatar.ArsenalProgress.WeaponIcon._visible = bShowWeapon;*/
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudTeamCounter::UpdateMiniScoreboard( void )
{
	if ( !CSGameRules() )
		return;

	if ( m_nMaxPlayers == 0 )
		return;

	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pLocalPlayer )
		return;

	// set the time when we updated
	m_flLastSpecListUpdate = gpGlobals->curtime;

	C_CS_PlayerResource* pCSPR = (C_CS_PlayerResource*)g_PR;

	int localPlayerIndex = GetLocalPlayerIndex();

	m_nTeamSelectionLastUpdate = pLocalPlayer->GetTeamNumber();

	int spectatedTargetIndex = -1;
	if ( GetSpectatorMode() == OBS_MODE_IN_EYE || GetSpectatorMode() == OBS_MODE_CHASE )
	{
		spectatedTargetIndex = GetSpectatorTarget();
	}

	bool bGunGameProgressive = CSGameRules()->IsPlayingGunGameProgressive();
	bool bDeathmatch = CSGameRules()->IsPlayingGunGameDeathmatch() && !CSGameRules()->IsPlayingTeamDM();
	bool bSurvival = CSGameRules()->IsPlayingSurvival();

	int	nTerroristTeamCount = 0;
	int	nCTTeamCount = 0;

	sAvatarInitData avatarData;

	//	bool bIsCompetitive = sv_competitive_official_5v5.GetInt( ) || CSGameRules( )->IsPlayingAnyCompetitiveStrictRuleset( );

	if ( pLocalPlayer )
	{
		int LocalBotControlledIdx = -1;
		if ( pLocalPlayer->IsControllingBot() )
		{
			LocalBotControlledIdx = pLocalPlayer->GetControlledBotIndex();
		}

		int nPlayersAlive_T = 0;
		int nPlayersAlive_CT = 0;
		bool bUpdatePlayerNumbers = false;

		for ( int playerIndex = 1; playerIndex <= MAX_PLAYERS; playerIndex++ )
		{
			bool bIsConnected = g_PR->IsConnected( playerIndex );

			if ( !bIsConnected )
				continue;

			int TeamId = g_PR->GetTeam( playerIndex );

			// Mini-Scoreboard only reflects the active players, not spectators or those who haven't selected a team
			if ( TeamId != TEAM_CT && TeamId != TEAM_TERRORIST )
				continue;

			int gunGameLevel = -1;
			if ( bGunGameProgressive )
			{
				gunGameLevel = pCSPR->GetGunGameLevel( playerIndex );
			}

			int nEntIdx = 0;
			C_CSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( playerIndex ) );
			if ( pPlayer )
			{
				nEntIdx = pPlayer->entindex();

				if ( pPlayer->MadeFinalGunGameProgressiveKill() )
					gunGameLevel++;
			}

			bool bIsLocalPlayer = ( localPlayerIndex == playerIndex );

			bool bSpeaking = GetClientVoiceMgr()->IsPlayerSpeaking( playerIndex );
			if ( bSpeaking )
			{
				if ( !g_PR->IsFakePlayer( playerIndex ) && !GetClientVoiceMgr()->IsPlayerAudible( playerIndex ) )
					bSpeaking = false;
			}

			bool bIsCT = ( TeamId == TEAM_CT );

			bool bShowHealth = CanSeeSpectatorOnlyTools();
			if ( bShowHealth == false && pLocalPlayer )
			{
				if ( !pLocalPlayer->IsOtherEnemy( playerIndex ) || ( !pLocalPlayer->IsAlive() && mp_forcecamera.GetInt() == OBS_ALLOW_ALL ) )
					bShowHealth = true;
			}

			XUID playerXuid = g_PR->GetXuid( playerIndex );

			bool bDead = false;

			int ControlledByIdx = pCSPR->GetControlledByPlayer( playerIndex );

			if ( ControlledByIdx != 0 )
			{
				// If we're a bot currently controlled by a player, we defer to their alive state
				bDead = !g_PR->IsAlive( ControlledByIdx );
			}
			else
			{
				// We are dead if we are not alive, or whenever we're controlling a bot (in this case, we want to show the bot as alive instead)
				bDead = !g_PR->IsAlive( playerIndex ) || pCSPR->IsControllingBot( playerIndex );

				//If a bot is kicked while we control it, just show us as alive.
				int controlledBot = pCSPR->GetControlledPlayer( playerIndex );
				if ( pCSPR->IsControllingBot( playerIndex ) && ( !pCSPR->IsConnected( controlledBot ) || !pCSPR->IsFakePlayer( controlledBot ) ) )
				{
					bDead = false;
				}
			}

			if ( TeamId == TEAM_CT && !bDead )
				nPlayersAlive_CT++;
			else if ( TeamId == TEAM_TERRORIST && !bDead )
				nPlayersAlive_T++;

			bool bIsSpectating = false;

			int nHealth = 0;
			int nArmor = 0;
			int nPoints = 0;

			if ( pCSPR->GetControlledPlayer( playerIndex ) == 0 )
			{
				int playerHealthIndex = playerIndex;
				int controlledByIndex = pCSPR->GetControlledByPlayer( playerIndex );

				if ( controlledByIndex != 0 )
				{
					playerHealthIndex = controlledByIndex;
				}

				bIsSpectating = ( playerHealthIndex == spectatedTargetIndex );

				if ( g_PR->IsAlive( playerHealthIndex ) && bShowHealth )
				{
					nHealth = pCSPR->GetHealth( playerHealthIndex );
					nArmor = pCSPR->GetArmor( playerHealthIndex );
				}

				nPoints = pCSPR->GetScore( playerHealthIndex );
			}

			TeamCounterMiniStatus_t *ms = NULL;

			int slotIdx = -1;
			if ( bIsCT )
			{
				// this team is full - we have no more space for players in our roster
				if ( !bGunGameProgressive && !bDeathmatch )
				{
					if ( nCTTeamCount == ( MAX_GGPROG_PLAYERS / 2 ) )
						continue;
				}
				else
				{
					if ( nCTTeamCount == MAX_TEAM_SIZE )
						continue;
				}

				slotIdx = nCTTeamCount++;
			}
			else
			{
				// this team is full - we have no more space for players in our roster
				if ( !bGunGameProgressive && !bDeathmatch )
				{
					if ( nTerroristTeamCount == ( MAX_GGPROG_PLAYERS / 2 ) )
						continue;
				}
				else
				{
					if ( nTerroristTeamCount == MAX_TEAM_SIZE )
						continue;
				}

				slotIdx = nTerroristTeamCount++;
			}

			if ( !bGunGameProgressive && !bDeathmatch )
			{
				// Grab from the team container
				TeamCounterMiniStatus_t *pTeamStatuses = bIsCT ? m_CTTeam : m_TerroristTeam;
				ms = &pTeamStatuses[ slotIdx ];
			}
			else
			{
				slotIdx = nCTTeamCount + nTerroristTeamCount - 1;

				// we only support up to this number of players in GG Progressive
				if ( slotIdx >= MAX_GGPROG_PLAYERS )
				{
					continue;
				}

				ms = &m_GGProgressivePlayers[ slotIdx ];
			}

			int nPlayerIdxForColor = -1;
			if ( pLocalPlayer && pLocalPlayer->ShouldShowTeamPlayerColors( ms->bIsCT ? TEAM_CT : TEAM_TERRORIST ) )
				nPlayerIdxForColor = pCSPR->GetCompTeammateColor( playerIndex );
			// 
			// 			C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
			// 			if ( pLocalPlayer && pLocalPlayer->ShouldShowTeamPlayerColors( ms->bIsCT ? TEAM_CT : TEAM_TERRORIST ) )
			// 			{
			// 				//m_bColorTabsInitialized = false;
			// 				bool bShowLetter = pLocalPlayer->ShouldShowTeamPlayerColorLetters();
			// 
			// 				C_CS_PlayerResource* pCSPR = ( C_CS_PlayerResource* )g_PR;
			// 				if ( pCSPR )
			// 				{
			// 					nPlayerIdxForColor = pCSPR->GetCompTeammateColor( ms->nPlayerIdx );
			// 					if ( nPlayerIdxForColor != -1 )
			// 						ms->bNeedsColorUpdate = false;
			// 				}
			// 			}
			// 			//else
			// 			//	m_bColorTabsInitialized = true;

			int nTeam = bIsCT ? TEAM_CT : TEAM_TERRORIST;
			bool bTeamLeader = ( nEntIdx == GetGlobalTeam( nTeam )->GetGGLeader( nTeam ) );

			bool bRefresh = ms->Update(
				playerXuid,
				nEntIdx,
				playerIndex,
				gunGameLevel,
				nHealth,
				nArmor,
				bIsCT,
				bIsLocalPlayer,
				bDead,
				pLocalPlayer->IsPlayerDominated( playerIndex ),
				pLocalPlayer->IsPlayerDominatingMe( playerIndex ),
				bTeamLeader,
				bSpeaking,
				( LocalBotControlledIdx == playerIndex ),
				bIsSpectating,
				nPoints,
				TeamId,
				nPlayerIdxForColor,
				gpGlobals->curtime
			);

			// 			if ( CSGameRules( )->IsPlayingAnyCompetitiveStrictRuleset( ) && nPlayerIdxForColor == -1 && TeamId == pLocalPlayer->GetTeamNumber( ) )
			// 				ms->bForceRefreshColor = true;

			if ( !bIsConnected )
				bRefresh = true;

			if ( m_bForceAvatarRefresh || bRefresh )
			{
				bUpdatePlayerNumbers = true;

				// If we're in Gun Game progressive and this player isn't ranked yet, do not update their slot until the next re-ranking
				if ( ( !bDeathmatch && !bGunGameProgressive ) || ms->nGGProgressiveRank != -1 )
				{
					InvokeAvatarSlotUpdate( avatarData, ms, ( bGunGameProgressive || bDeathmatch ) ? ms->nGGProgressiveRank : slotIdx );
				}
			}
		}

		if ( bUpdatePlayerNumbers )
		{
			SetNumPlayersAlive( nPlayersAlive_CT, nPlayersAlive_T );
		}
	}

	if ( bGunGameProgressive || bDeathmatch )
	{
		bool bPlayerCountChange = false;

		if ( ( nCTTeamCount + nTerroristTeamCount ) != m_nPreviousGGProgressiveTotalPlayers )
		{
			m_nPreviousGGProgressiveTotalPlayers = ( nCTTeamCount + nTerroristTeamCount );

			UpdateTotalProgressivePlayers( m_nPreviousGGProgressiveTotalPlayers, bDeathmatch );

			bPlayerCountChange = true;
		}

		if ( bPlayerCountChange || !m_GGProgRankingTimer.HasStarted() || m_GGProgRankingTimer.IsElapsed() )
		{
			static const float kRankingUpdateInterval = 0.5f;
			m_GGProgRankingTimer.Start( kRankingUpdateInterval );

			m_ggSortedList.RemoveAll();

			for ( int NewIdx = 0; NewIdx < MIN( MAX_GGPROG_PLAYERS, m_nPreviousGGProgressiveTotalPlayers ); NewIdx++ )
			{
				TeamCounterMiniStatus_t *ms = &m_GGProgressivePlayers[ NewIdx ];

				m_ggSortedList.AddToTail( ms );
			}

			if ( bDeathmatch )
				m_ggSortedList.Sort( DMSortFunction );
			else
				m_ggSortedList.Sort( GGProgSortFunction );

			for ( int NewIdx = 0; NewIdx < MIN( MAX_GGPROG_PLAYERS, m_nPreviousGGProgressiveTotalPlayers ); NewIdx++ )
			{
				TeamCounterMiniStatus_t *ms = m_ggSortedList[ NewIdx ];
				if ( ms && ms->nPlayerIdx != -1 )
				{
					ms->nGGProgressiveRank = NewIdx;

					InvokeAvatarSlotUpdate( avatarData, ms, NewIdx );
				}
			}

			// Update the current leader text every time we sort the list
			if ( m_ggSortedList.Count() > 0 )
			{
				TeamCounterMiniStatus_t *ms = m_ggSortedList[ 0 ];
				if ( ms )
				{
					int nPrevGGLevel = 0;
					bool bNoMoreLeads = false;
					for ( int i = 0; i < m_ggSortedList.Count(); i++ )
					{
						int nGGLevel = m_ggSortedList[ i ]->nGunGameLevel;
						if ( nGGLevel < nPrevGGLevel || nGGLevel == 0 )
							bNoMoreLeads = true;

						UpdateLeaderWeaponVisibility( i, !bNoMoreLeads );

						nPrevGGLevel = nGGLevel;
					}
				}
			}
		}
	}
	else
	{
		m_ggSortedList.RemoveAll();

		// Hide remaining unused T avatars
		if ( nTerroristTeamCount != m_nTerroristTeamCount && nTerroristTeamCount < MAX_TEAM_SIZE )
		{
			int nCount = nTerroristTeamCount;

			// Clear the player index
			for ( int Idx = nCount; Idx < MAX_TEAM_SIZE; ++Idx )
			{
				m_TerroristTeam[ Idx ].nPlayerIdx = -1;
			}

			DisableRemainingPlayerIcons( false, nCount );
		}

		// Hide remaining unused CT avatars
		if ( nCTTeamCount != m_nCTTeamCount && nCTTeamCount < MAX_TEAM_SIZE )
		{
			int nCount = nCTTeamCount;

			// Clear the player index
			for ( int Idx = nCount; Idx < MAX_TEAM_SIZE; ++Idx )
			{
				m_CTTeam[ Idx ].nPlayerIdx = -1;
			}

			DisableRemainingPlayerIcons( true, nCount );
		}
	}

	m_bForceAvatarRefresh = false;

	m_nTerroristTeamCount = nTerroristTeamCount;
	m_nCTTeamCount = nCTTeamCount;

	if ( m_flPlayingTeamFadeoutTime > -1 )
	{
		// if we've surpassed the fadeout time and it hasn't been surpassed by a full second, fade it out
		if ( m_flPlayingTeamFadeoutTime <= gpGlobals->curtime )
		{
			FadeOutSelectedTeam();
			m_flPlayingTeamFadeoutTime = -1;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudTeamCounter::UpdateNumberCount()
{
	if ( IsPlayerCountVisibleForT() )
	{
		m_pAliveBGT->SetVisible( true );

		if ( m_nPlayersAlive_T == 0 )
		{
			m_pAliveSkullT->SetVisible( true );

			m_pAliveCountT->SetVisible( false );
			m_pAliveTextT->SetVisible( false );
		}
		else
		{
			m_pAliveCountT->SetVisible( true );
			m_pAliveCountT->SetText( CFmtStr( "%d", m_nPlayersAlive_T ) );
			m_pAliveTextT->SetVisible( true );

			m_pAliveSkullT->SetVisible( false );
		}
	}
	else
	{
		m_pAliveBGT->SetVisible( false );
	}

	if ( IsPlayerCountVisibleForCT() )
	{
		m_pAliveBGCT->SetVisible( true );

		if ( m_nPlayersAlive_CT == 0 )
		{
			m_pAliveSkullCT->SetVisible( true );

			m_pAliveCountCT->SetVisible( false );
			m_pAliveTextCT->SetVisible( false );
		}
		else
		{
			m_pAliveCountCT->SetVisible( true );
			m_pAliveCountCT->SetText( CFmtStr( "%d", m_nPlayersAlive_CT ) );
			m_pAliveTextCT->SetVisible( true );

			m_pAliveSkullCT->SetVisible( false );
		}
	}
	else
	{
		m_pAliveBGCT->SetVisible( false );
	}

	// TODO
/*	if ( IsPlayerCountVisibleForT() )
	{
		var Talive = TopPanel.Panel.alivet.count;
		Talive._alpha = 100;
		Talive.box.htmlText = m_nPlayersAlive_T;
		TopPanel.Panel.alivet._visible = true;
		TopPanel.Panel.alivebgT._visible = true;
		TopPanel.Panel.Alive_T._visible = true;
		TopPanel.Panel.alivet.skullanim.gotoAndStop("Init");

		if ( m_nPlayersAlive_T == 0 )
		{
			TopPanel.Panel.alivet.skullanim.gotoAndStop("dead");
			TopPanel.Panel.Alive_T._visible = false;
			Talive._alpha = 0;
		}
	}
	else
	{
		TopPanel.Panel.alivet._visible = false;
		TopPanel.Panel.alivebgT._visible = false;
		TopPanel.Panel.aliveinstructor._visible = false;
		TopPanel.Panel.Alive_T._visible = false;
	}

	if ( IsPlayerCountVisibleForCT() )
	{
		var CTalive = TopPanel.Panel.alivect.count;
		CTalive._alpha = 100;
		CTalive.box.htmlText = m_nPlayersAlive_CT;
		TopPanel.Panel.alivect._visible = true;
		TopPanel.Panel.alivebgCT._visible = true;
		TopPanel.Panel.Alive_CT._visible = true;
		TopPanel.Panel.alivect.skullanim.gotoAndStop("Init");

		if ( m_nPlayersAlive_CT == 0 )
		{
			TopPanel.Panel.alivect.skullanim.gotoAndStop("dead");
			TopPanel.Panel.Alive_CT._visible = false;
			CTalive._alpha = 0;
		}
	}
	else
	{
		TopPanel.Panel.alivect._visible = false;
		TopPanel.Panel.alivebgCT._visible = false;
		TopPanel.Panel.aliveinstructor._visible = false;
		TopPanel.Panel.Alive_CT._visible = false;
	}*/
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudTeamCounter::UpdatePlantedBombState( float flDetProgress )
{
	static panorama::CPanoramaSymbol k_symBombPulseAnimSlow( "BombPlantedPulse__Slow" );
	static panorama::CPanoramaSymbol k_symBombPulseAnimMedium( "BombPlantedPulse__Medium" );
	static panorama::CPanoramaSymbol k_symBombPulseAnimFast( "BombPlantedPulse__Fast" );

	static panorama::CPanoramaSymbol k_symBombLinesAnimSlow( "BombPlantedLines__Slow" );
	static panorama::CPanoramaSymbol k_symBombLinesAnimMedium( "BombPlantedLines__Medium" );
	static panorama::CPanoramaSymbol k_symBombLinesAnimFast( "BombPlantedLines__Fast" );

	m_pTime->SetVisible( false );

	if ( flDetProgress > 60.0f )
	{
		m_pBombDefused->SetVisible( false );

		m_pBombPlanted->SetVisible( true );
		m_pBombPlantedLines->SetVisible( true );

		m_pBombPlanted->SetHasClass( k_symBombPulseAnimSlow, true );
		m_pBombPlanted->SetHasClass( k_symBombPulseAnimMedium, false );
		m_pBombPlanted->SetHasClass( k_symBombPulseAnimFast, false );

		m_pBombPlantedLines->SetHasClass( k_symBombLinesAnimSlow, true );
		m_pBombPlantedLines->SetHasClass( k_symBombLinesAnimMedium, false );
		m_pBombPlantedLines->SetHasClass( k_symBombLinesAnimFast, false );
	}
	else if ( flDetProgress > 30.0f )
	{
		m_pBombDefused->SetVisible( false );

		m_pBombPlanted->SetVisible( true );
		m_pBombPlantedLines->SetVisible( true );

		m_pBombPlanted->SetHasClass( k_symBombPulseAnimSlow, false );
		m_pBombPlanted->SetHasClass( k_symBombPulseAnimMedium, true );
		m_pBombPlanted->SetHasClass( k_symBombPulseAnimFast, false );

		m_pBombPlantedLines->SetHasClass( k_symBombLinesAnimSlow, false );
		m_pBombPlantedLines->SetHasClass( k_symBombLinesAnimMedium, true );
		m_pBombPlantedLines->SetHasClass( k_symBombLinesAnimFast, false );
	}
	else if ( flDetProgress > 0.0f )
	{
		m_pBombDefused->SetVisible( false );

		m_pBombPlanted->SetVisible( true );
		m_pBombPlantedLines->SetVisible( true );

		m_pBombPlanted->SetHasClass( k_symBombPulseAnimSlow, false );
		m_pBombPlanted->SetHasClass( k_symBombPulseAnimMedium, false );
		m_pBombPlanted->SetHasClass( k_symBombPulseAnimFast, true );

		m_pBombPlantedLines->SetHasClass( k_symBombLinesAnimSlow, false );
		m_pBombPlantedLines->SetHasClass( k_symBombLinesAnimMedium, false );
		m_pBombPlantedLines->SetHasClass( k_symBombLinesAnimFast, true );
	}
	else
	{
		m_pBombDefused->SetVisible( false );

		m_pBombPlanted->SetVisible( false );
		m_pBombPlantedLines->SetVisible( false );

		m_pBombPlanted->SetHasClass( k_symBombPulseAnimSlow, false );
		m_pBombPlanted->SetHasClass( k_symBombPulseAnimMedium, false );
		m_pBombPlanted->SetHasClass( k_symBombPulseAnimFast, false );

		m_pBombPlantedLines->SetHasClass( k_symBombLinesAnimSlow, true );
		m_pBombPlantedLines->SetHasClass( k_symBombLinesAnimMedium, false );
		m_pBombPlantedLines->SetHasClass( k_symBombLinesAnimFast, false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudTeamCounter::UpdateScore( void )
{
	int nCTScore = 0;
	int nTScore = 0;

	if ( !g_PR || !CSGameRules() || !m_bActive )
	{
		return;
	}

	if ( m_Mode == VIEW_MODE_NORMAL || m_Mode == VIEW_MODE_GUN_GAME_BOMB )
	{
		// [jason] The numbers now reflect rounds won, instead of remaining players
		C_Team *CT_team = GetGlobalTeam( TEAM_CT );
		if ( CT_team )
		{
			nCTScore = CT_team->Get_Score();
		}

		C_Team *T_team = GetGlobalTeam( TEAM_TERRORIST );
		if ( T_team )
		{
			nTScore = T_team->Get_Score();
		}
	}

	if ( m_pTScore && ( m_nTScoreLastUpdate != nTScore ) )
	{
		m_nTScoreLastUpdate = nTScore;
		m_pTScore->SetText( CFmtStr( "%d", m_nTScoreLastUpdate ) );
	}

	if ( m_pCTScore && ( m_nCTScoreLastUpdate != nCTScore ) )
	{
		m_nCTScoreLastUpdate = nCTScore;
		m_pCTScore->SetText( CFmtStr( "%d", m_nCTScoreLastUpdate ) );
	}

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudTeamCounter::UpdateTeamSelection()
{
	// TODO, required??
/*	var Panel = TopPanel.Panel;

	if ( team == 3 )
	{
		Panel.CTPlayer._visible = true;
		Panel.TPlayer._visible = false;
	}
	else if ( team == 2 )
	{
		Panel.TPlayer._visible = true;
		Panel.CTPlayer._visible = false;
	}
	else
	{
		Panel.CTPlayer._visible = false;
		Panel.TPlayer._visible = false;
	}*/
}

//-----------------------------------------------------------------------------
// update elements affected by position (based on cl_count_playercount_pos)
//-----------------------------------------------------------------------------
void CCSGO_HudTeamCounter::UpdatePosition( void )
{
	static const panorama::CPanoramaSymbol k_symAlignTop( "TeamCounter__Align--Top" );
	static const panorama::CPanoramaSymbol k_symAlignBottom( "TeamCounter__Align--Bottom" );

	static const panorama::CPanoramaSymbol k_symFlowUp( "TeamCounter__Flow--Up" );
	static const panorama::CPanoramaSymbol k_symFlowDown( "TeamCounter__Flow--Down" );


	extern ConVar cl_hud_playercount_pos;
	static int nLastPosition = -1;

	if ( m_nMaxPlayers == 0 )
		return;

	int nCurrentPosition = cl_hud_playercount_pos.GetInt();
	m_bPositionIsBottom = ( nCurrentPosition == 1 );

	if ( m_bPositionInitialised && ( nCurrentPosition == nLastPosition ) )
		return;

	m_bPositionInitialised = true;
	nLastPosition = nCurrentPosition;

	if ( m_bPositionIsBottom )
	{
		for ( int idx = 0; idx < MAX_AVATAR_PANELS; idx++ )
		{
			CAvatarPanelData *pAvatarPanelData = &m_aAvatarPanels[ idx ];

			if ( pAvatarPanelData->m_pPanel )
			{
				pAvatarPanelData->m_pPanel->SetHasClass( k_symFlowUp, true );
				pAvatarPanelData->m_pPanel->SetHasClass( k_symFlowDown, false );

				pAvatarPanelData->m_pPanel->SetHasClass( k_symAlignTop, false );
				pAvatarPanelData->m_pPanel->SetHasClass( k_symAlignBottom, true );
			}
		}

		SetHasClass( k_symAlignTop, false );
		SetHasClass( k_symAlignBottom, true );
	}
	else
	{
 		for ( int idx = 0; idx < MAX_AVATAR_PANELS; idx++ )
 		{
			CAvatarPanelData *pAvatarPanelData = &m_aAvatarPanels[ idx ];

			if ( pAvatarPanelData->m_pPanel )
			{
				pAvatarPanelData->m_pPanel->SetHasClass( k_symFlowUp, false );
				pAvatarPanelData->m_pPanel->SetHasClass( k_symFlowDown, true );

				pAvatarPanelData->m_pPanel->SetHasClass( k_symAlignTop, true );
				pAvatarPanelData->m_pPanel->SetHasClass( k_symAlignBottom, false );
			}
 		}

		SetHasClass( k_symAlignTop, true );
		SetHasClass( k_symAlignBottom, false );
	}

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudTeamCounter::UpdateTimer( void )
{
	C_CSGameRules *pRules = CSGameRules();
	bool bBeginTimerAlert = false;
	bool bCancelTimerAlert = false;
	bool bSetTime = false;

	if ( !pRules )
	{
		return;
	}

	// timer is hidden when bomb planted, so break out of update
	if ( g_PlantedC4s.Count() > 0 && !CSGameRules()->IsPlayingCoopMission() && !CSGameRules()->IsPlayingSurvival() )
	{
		if ( !m_bTimerHidden )
		{
			m_bTimerHidden = true;

			HideTimer();
		}

		if ( m_bIsBombDefused )
		{
			SetBombDefused();
		}
		else
		{
			UpdatePlantedBombState( g_PlantedC4s[ 0 ]->GetDetonationProgress() * 100.0f );
		}

		return;
	}

	if ( CSGameRules() && CSGameRules()->IsWarmupPeriod() && m_pTime )
	{
		if ( m_nLastTimeSet != TIMER_STATE_WARMUP )
		{
			m_pTime->SetText( "" );
			m_nLastTimeSet = TIMER_STATE_WARMUP;
		}
		return;
	}
	else if ( CSGameRules() && CSGameRules()->IsFreezePeriod() &&
		( CSGameRules()->IsMatchWaitingForResume() ) )
	{
		if ( m_nLastTimeSet != TIMER_STATE_FREEZE )
		{
			m_pTime->SetText( "❚❚" );
			m_nLastTimeSet = TIMER_STATE_FREEZE;
		}

		BeginTimerAlert();
		return;
	}

	int nTimer = static_cast<int>( ceil( pRules->GetRoundRemainingTime() ) );

	bool bFreezePeriod = pRules->IsFreezePeriod();
	if ( bFreezePeriod )
	{
		// countdown to the start of the round while we're in freeze period
		nTimer = static_cast<int>( ceil( pRules->GetRoundStartTime() - gpGlobals->curtime ) );
	}

	const int kTimeRemainingToDisplayRed = 11;

	if ( m_bRoundStarted )
	{
		if ( !m_bTimerAlertTriggered && ( nTimer < kTimeRemainingToDisplayRed ) )
		{
			// when time is low switch to red text
			m_bTimerAlertTriggered = true;
			SetTimerRedColor( true );
			m_nLastTimeSet = TIMER_STATE_UNSET;

			bBeginTimerAlert = true;
		}
		else if ( m_bTimerAlertTriggered && ( nTimer >= kTimeRemainingToDisplayRed ) )
		{
			// revert to normal timer color
			m_bTimerAlertTriggered = false;
			SetTimerRedColor( false );
			m_nLastTimeSet = TIMER_STATE_UNSET;

			bCancelTimerAlert = true;
		}
	}

	if ( nTimer < 0 )
	{
		nTimer = 0;
	}

	int nMinutes = nTimer / 60;
	int nSeconds = nTimer % 60;

	//wchar_t szTime[ 32 ];
	char szTime[ 32 ];
	szTime[ 0 ] = 0;

	if ( m_pTime && m_bRoundStarted && ( m_nLastTimeSet != nTimer ) )
	{
		//V_snwprintf( szTime, ARRAYSIZE( szTime ), L"%d:%.2d", nMinutes, nSeconds );
		V_snprintf( szTime, ARRAYSIZE( szTime ), "%d:%.2d", nMinutes, nSeconds );

		bSetTime = true;
		m_nLastTimeSet = nTimer;
	}

	if ( ( bSetTime || bBeginTimerAlert || bCancelTimerAlert ) )
	{
		if ( bCancelTimerAlert )
		{
			BeginTimerNormal();
		}

		if ( bBeginTimerAlert )
		{
			BeginTimerAlert();
		}

		if ( ( bSetTime ) && ( m_pTime ) )
		{
			m_pTime->SetText( szTime );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudTeamCounter::SetTimerVisibility( bool bVisible, bool bHasBomb /*= true*/ )
{
	m_pGameTimeContainer->SetVisible( bVisible );

	// bomb container visibility is the same as gametime container
	// it's a seperate panel because it needs to render image outside GameTimeContainer panel
	m_pBombContainer->SetVisible( bVisible && bHasBomb );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudTeamCounter::SetTimerRedColor( bool bRed )
{
	static const panorama::CPanoramaSymbol k_symRedTimer( "teamcounter_red_timer" );
	if ( m_pTime )
	{
		m_pTime->SetHasClass( k_symRedTimer, bRed );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudTeamCounter::UpdateTotalProgressivePlayers( int nNewCount, bool bShowTimer )
{
	// TODO
/*  if(!m_bIsGunGame)
		return;

	var Panel = TopPanel.Panel;

	// Force every avatar to be visible
	for ( var Idx = 0; Idx < 10; Idx++ )
	{
		var AvatarName = "Avatar" + Idx;

		var Avatar = Panel[ AvatarName ];

		Avatar._visible = true;
	}

	// Snap to the proper frame to recenter the mini-scoreboard
	if ( NewCount < 0 )
		NewCount = 0;
	else if ( NewCount > 10 )
		NewCount = 10;

	var sTimerStr = "";
	if ( bShowTimer )
		sTimerStr = "T";

	var sBottom = "";
	if ( m_bPositionIsBottom )
		sBottom = "_B_";

	m_nLastGGPlayerCount = NewCount;
	m_bIsShowingTimer = bShowTimer;

	var anim = "GGProg" + sTimerStr + sBottom + NewCount;
	trace( "UpdateTotalProgressivePlayers: anim = "+anim );
	Panel.gotoAndStop( "GGProg" + sTimerStr + sBottom + NewCount );*/
}


//-----------------------------------------------------------------------------
// Purpose: ctor; sets up default ranges
// CCSGO_AvatarHealthBar was going to be customized, but right now it acts 
// like a regular CPanel2D (we only set the width) so could be reverted to one.
//-----------------------------------------------------------------------------
CCSGO_AvatarHealthBar::CCSGO_AvatarHealthBar( panorama::CPanel2D *pParent, const char *pchID )
	: CPanel2D( pParent, pchID )
	, m_flMin( 0.0f )
	, m_flMax( 1.0f )
	, m_flCur( 0.0f )
{
}


//-----------------------------------------------------------------------------
// Purpose: dtor
//-----------------------------------------------------------------------------
CCSGO_AvatarHealthBar::~CCSGO_AvatarHealthBar()
{
}


//-----------------------------------------------------------------------------
// Purpose: layout - sets sizes of child panels
//-----------------------------------------------------------------------------
void CCSGO_AvatarHealthBar::OnLayoutTraverse( float flFinalWidth, float flFinalHeight )
{
	BaseClass::OnLayoutTraverse( flFinalWidth, flFinalHeight );
}


//-----------------------------------------------------------------------------
// Purpose: set property from xml
//-----------------------------------------------------------------------------
bool CCSGO_AvatarHealthBar::BSetProperty( panorama::CPanoramaSymbol symName, const char *pchValue )
{
	static const panorama::CPanoramaSymbol k_symMin( "min" );
	static const panorama::CPanoramaSymbol k_symMax( "max" );
	static const panorama::CPanoramaSymbol k_symValue( "value" );

	if ( symName == k_symMin )
	{
		SetMin( V_atof( pchValue ) );
		return true;
	}
	else if ( symName == k_symMax )
	{
		SetMax( V_atof( pchValue ) );
		return true;
	}
	else if ( symName == k_symValue )
	{
		SetValue( V_atof( pchValue ) );
		return true;
	}

	return BaseClass::BSetProperty( symName, pchValue );
}

