//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Spectator hud panel
//
//=============================================================================//

#include "cbase.h"
#include "csgo_hudspectator.h"
#include "panorama/uievents.h"
#include "c_cs_playerresource.h"
#include "c_team.h"
#include "../csgo_scoreboard.h"
#include "c_plantedc4.h"
#include "gameui_util.h"
#include "gameui_interface.h"
//#include "cs_lobby_helpers.h"
//#include "uicomponents/uicomponent_matchstats.h"
#include "IGameUIFuncs.h"
#include "inputsystem/iinputsystem.h"
#include "cdll_util.h"
#include "clientmode_csnormal.h"
#include "panorama/csgo_endofmatch.h"
#include "panorama/csgo_scoreboard.h"

// To get team counter panel instance as it has some spectator related logic in it...
#include "hud.h"
#include "panorama/hud/csgo_hudteamcounter.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>



using namespace panorama;

extern ConVar mp_maxrounds;
extern ConVar mp_overtime_maxrounds;
extern ConVar cl_spec_swapplayersides;
extern ConVar mp_spec_swapplayersides;
extern ConVar spec_show_xray;
extern ConVar spec_hide_players;
extern ConVar item_debug_give_fake_random_tourney_awards;
extern ConVar cl_server_graphic2_enable;
extern ConVar sv_server_graphic2;
extern ConVar spec_autodirector;
extern ConVar cl_draw_only_deathnotices;
extern ConVar cl_drawhud;

void Spectator_SwapPlayerSides_Callback( IConVar *pConVar, const char *pOldString, float flOldValue )
{
	CCSGO_HudSpectator *pSpecPanels = CCSGO_HudSpectator::GetInstance();
	if ( pSpecPanels && pSpecPanels->BIsVisible() )
	{
		pSpecPanels->ForceRefreshPanels();
	}	
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CSpecPlayerPanelData::CSpecPlayerPanelData()
{
	V_memset( this, 0, sizeof( CSpecPlayerPanelData ) );
	m_flHealthFlashStartTime = 0;
}

CSpecPlayerPanelData::~CSpecPlayerPanelData()
{
}

bool CSpecPlayerPanelData::HasHealthFlashTimeElapsed()
{
	if ( gpGlobals->curtime > m_flHealthFlashStartTime + PLAYERPANEL_HEALTHFLASH_LEN )
		return true;

	return false;
}

void CSpecPlayerPanelData::StartHealthFlashTimer()
{
	m_flHealthFlashStartTime = gpGlobals->curtime;
}

void CSpecPlayerPanelData::InitRedHealthBar()
{
	m_flHealthFlashStartTime = 0;
	m_flPrevHealth = 100;
	m_flCurHealth = 100;
	m_pHealthBarRed->SetValue(0);
}

float CSpecPlayerPanelData::GetHealthFlashLerpFrac()
{
	return MIN( (gpGlobals->curtime - (m_flHealthFlashStartTime + PLAYERPANEL_HEALTHFLASH_LEN)) / 0.25f, 1.0f );
}

void CSpecPlayerPanelData::SetHealthBarType( panorama::CPanoramaSymbol symName )
{
	static const panorama::CPanoramaSymbol k_symHealthBarTypeAttribute( "healthbartype" );

	if ( !m_pHealthBar )
		return;

	m_pHealthBar->SwitchClass( k_symHealthBarTypeAttribute, symName );
}

DEFINE_PANORAMA_EVENT_DOC( SpectatorSelectClickedPlayer, "", "Spectate a particular player" );

REGISTER_PANEL2D_FACTORY( CCSGO_HudSpectator, CSGOHudSpectator );
CCSGO_HudSpectator *CCSGO_HudSpectator::s_pSpecPanels = NULL;

CCSGO_HudSpectator::CCSGO_HudSpectator( CPanel2D *pParent, const char *pchID )
	: CPanoramaHudElement( "CCSGO_HudSpectator", this ),
	CPanel2D( pParent, pchID ),
	m_bRoundStarted( true ),
	m_bForceAvatarRefresh( true ),
	m_bHideMiniScoreboard( false ),
	m_nCurrentRoundNumber( 0 ),
	m_bTimerAlertTriggered( false ),
	m_bIsBombDefused( false ),
	m_bDisplayMatchesWon( false ),
	m_nTourneyItemsDroppedThisRound( 0 ),
	m_nTourneyItemsDroppedTotal( 0 ),
	m_flLastTourneyItemPanelUpdate( 0 ),
	m_bIsFreezeTimeActive( false ),
	m_HLTVSpectators( 0 ),
	m_bRequestedMouseInput( false ),
	m_bForceRefresh( false ),
	m_Capture( this, "HudSpectator", k_EGameInputShareMouse )
{
	Assert( s_pSpecPanels == NULL );
	s_pSpecPanels = this;

	m_TournamentRewardTempList.RemoveAll();

	m_bSwapPlayerNames = false;

	/*
	SetAcceptsInput( true );
	SetAcceptsFocus( true );
	*/

	SetInputNamespace( "CSGOHudSpectator" );
	RequireLoadLayout( "file://{resources}/layout/hud/hudspectator.xml" );
	//m_pPlayerAvatar			= panel_cast< CCSGO_AvatarImage*> ( FindChildInLayoutFile( "PlayerAvatar" ) );
	//m_pPlayerNameLabel		= panel_cast< CLabel* > ( FindChildInLayoutFile( "PlayerName" ) );
	m_aPlayerPanels.SetSize( MAX_TEAM_SIZE * 2 );
	m_bValidPlayerPanels = false;

	m_pTeamSpec_L = RequireChildInLayoutFile( "TeamSpec_L" );
	m_pTeamSpec_R = RequireChildInLayoutFile( "TeamSpec_R" );

	m_pTeamSpecTeamMoney_L = RequireChildInLayoutFile( "TeamMoney_L" );
	m_pTeamSpecTeamMoney_R = RequireChildInLayoutFile( "TeamMoney_R" );

	m_pTeamMoneyText_L = panorama::panel_cast<panorama::CLabel *>( RequireChildInLayoutFile( "TeamMoneyText_L" ) );
	m_pTeamMoneyText_R = panorama::panel_cast<panorama::CLabel *>( RequireChildInLayoutFile( "TeamMoneyText_R" ) );
	m_pEQValueText_L = panorama::panel_cast<panorama::CLabel *>( RequireChildInLayoutFile( "TeamEQValueText_L" ) );
	m_pEQValueText_R = panorama::panel_cast<panorama::CLabel *>( RequireChildInLayoutFile( "TeamEQValueText_R" ) );
	m_pTimer = panorama::panel_cast<panorama::CLabel *>( RequireChildInLayoutFile( "Timer" ) );
	m_pRoundText = panorama::panel_cast<panorama::CLabel *>( RequireChildInLayoutFile( "RoundNumber" ) );

	// init bomb
	m_pBombPlanted = panorama::panel_cast< panorama::CImagePanel * >( RequireChildInLayoutFile( "BombPlanted" ) );
	m_pBombPlantedLines = panorama::panel_cast< panorama::CImagePanel * >( RequireChildInLayoutFile( "BombPlantedLines" ) );
	m_pBombDefused = panorama::panel_cast< panorama::CImagePanel * >( RequireChildInLayoutFile( "BombDefused" ) );
	
	m_pScore_L = panorama::panel_cast<panorama::CLabel *>( RequireChildInLayoutFile( "ScoreL" ) );
	m_pScore_R = panorama::panel_cast<panorama::CLabel *>( RequireChildInLayoutFile( "ScoreR" ) );

	m_pTeamLogoL = panorama::panel_cast< panorama::CImagePanel * >( RequireChildInLayoutFile( "TeamLogoL" ) );
	m_pTeamLogoR = panorama::panel_cast< panorama::CImagePanel * >( RequireChildInLayoutFile( "TeamLogoR" ) );

	m_pTeamName_L = panorama::panel_cast<panorama::CLabel *>( RequireChildInLayoutFile( "TeamNameL" ) );
	m_pTeamName_R = panorama::panel_cast<panorama::CLabel *>( RequireChildInLayoutFile( "TeamNameR" ) );

	m_pExtraMatchData = RequireChildInLayoutFile( "ExtraMatchData" );
	m_pExtraMatchDataText = panorama::panel_cast<panorama::CLabel *>( RequireChildInLayoutFile( "ExtraMatchDataText" ) );
	m_pExtraMatchDataTextL = panorama::panel_cast< panorama::CLabel * >( RequireChildInLayoutFile( "ExtraMatchDataTextL" ) );
	m_pExtraMatchDataTextR = panorama::panel_cast< panorama::CLabel * >( RequireChildInLayoutFile( "ExtraMatchDataTextR" ) );

	m_pBombPlanted->SetVisible( false );
	m_pBombPlantedLines->SetVisible( false );
	m_pBombDefused->SetVisible( false );

	m_pTeamSpec_L->SetVisible( false );
	m_pTeamSpec_R->SetVisible( false );

	// pickem
	m_pPickemPredictionsPanel = RequireChildInLayoutFile( "PickemPredictions" );
	m_pPickemBar = panorama::panel_cast< CProgressBar * >( RequireChildInLayoutFile( "PickemBar" ) );
	m_pPickemBar->SetMin( 0.0f );
	m_pPickemBar->SetMax( 100.0f );

	m_pPickemPercentL = panorama::panel_cast<panorama::CLabel *>( RequireChildInLayoutFile( "PickemPercentL" ) );
	m_pPickemPercentR = panorama::panel_cast<panorama::CLabel *>( RequireChildInLayoutFile( "PickemPercentR" ) );

	m_pBestOfPipsPanel = RequireChildInLayoutFile( "BestOfPips" );
	m_pBestOfPipsText = panorama::panel_cast<panorama::CLabel *>( RequireChildInLayoutFile( "BestOfPipsText" ) );
	m_pBestOfPipsL = RequireChildInLayoutFile( "BestOfPipsPanelL" );
	m_pBestOfPipsR = RequireChildInLayoutFile( "BestOfPipsPanelR" );
	m_pBestOfPipsIconsL.SetSize( 3 );
	m_pBestOfPipsIconsR.SetSize( 3 );
	char szWepTmp[64] = {};
	for ( int i = 0; i < 3; i++ )
	{
		V_sprintf_safe( szWepTmp, "BestOfPipL%d", i + 1 );
		m_pBestOfPipsIconsL[i] = panorama::panel_cast< panorama::CImagePanel * >( RequireChildInLayoutFile( szWepTmp ) );
		V_sprintf_safe( szWepTmp, "BestOfPipR%d", i + 1 );
		m_pBestOfPipsIconsR[i] = panorama::panel_cast< panorama::CImagePanel * >( RequireChildInLayoutFile( szWepTmp ) );
	}

	m_pItemDropRoot = RequireChildInLayoutFile( "ItemDropPanel" );

	m_pEventDropsViewersRoot = RequireChildInLayoutFile( "EventDropsViewersRoot" );
	m_pEventLogo = RequireChildInLayoutFile( "EventLogoPanel" );
	m_pEventLogoImage = panorama::panel_cast< panorama::CImagePanel * >( RequireChildInLayoutFile( "EventLogoImage" ) );
	m_pEventLogoRight = panorama::panel_cast< panorama::CImagePanel * >( RequireChildInLayoutFile( "EventLogo_R" ) );

//	m_pItemDropItemImage = panorama::panel_cast<panorama::CImagePanel*>( RequireChildInLayoutFile( "ItemDropPanel_Item" ) );
	m_pItemDropItemName = panorama::panel_cast<panorama::CLabel *>( RequireChildInLayoutFile( "ItemDropPanel_ItemName" ) );
	m_pItemDropTotalRound = panorama::panel_cast<panorama::CLabel *>( RequireChildInLayoutFile( "ItemDropPanel_DropsRound" ) );
	m_pItemDropTotalMatch = panorama::panel_cast<panorama::CLabel *>( RequireChildInLayoutFile( "ItemDropPanel_Footer" ) );

	m_pMatchViewerPanel = RequireChildInLayoutFile( "ViewersPanel" );
	m_pMatchViewerText = panorama::panel_cast<panorama::CLabel *>( RequireChildInLayoutFile( "ViewersText" ) );

	m_pHudBlurTargetPanel = CCSGO_Hud::GetInstance()->RequireChildInLayoutFile( "HudBlur" );

	ListenForGameEvent( "spec_target_updated" );
	ListenForGameEvent( "cs_prev_next_spectator" );
	//ListenForGameEvent( "hltv_changed_mode" );
	//ListenForGameEvent( "cs_game_disconnected" );
	ListenForGameEvent( "round_start" );
	ListenForGameEvent( "round_end" );
	ListenForGameEvent( "bomb_planted" );
	ListenForGameEvent( "bot_takeover" );
	ListenForGameEvent( "hltv_status" );
	ListenForGameEvent( "tournament_reward" );
	ListenForGameEvent( "player_spawn" );

	RegisterEventHandler( panorama::AnimationEnd(), this, &CCSGO_HudSpectator::EventAnimationEnd );
	RegisterEventHandler( SpectatorSelectClickedPlayer(), this, &CCSGO_HudSpectator::HandleSelectClickedPlayer );
}

CCSGO_HudSpectator::~CCSGO_HudSpectator()
{
	Assert( s_pSpecPanels == this );
	s_pSpecPanels = NULL;
}

void CCSGO_HudSpectator::FireGameEvent( IGameEvent *evt )
{
	const char* szEventName = evt->GetName();
	CBasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	int EventUserID = evt->GetInt( "userid", -1 );
	int LocalPlayerID = ( pLocalPlayer != NULL ) ? pLocalPlayer->GetUserID() : -2;
	if ( FStrEq( "spec_target_updated", szEventName ) ||
		 FStrEq( "spec_mode_updated", szEventName ) )
	{
		ForceRefreshPanels();
	}
	else if ( FStrEq( "cs_prev_next_spectator", szEventName ) )
	{
		ForceRefreshPanels();
	}
	else if ( FStrEq( "round_start", szEventName ) )
	{
		m_bRoundStarted = true;
		m_bIsBombDefused = false;
		m_flLastTourneyItemPanelUpdate = 0;
		//BeginTimerNormal();

		DisplayEventServerImage();

		ForceRefreshPanels();

		if ( m_bValidPlayerPanels )
		{
			CSpecPlayerPanelData *pPlayerPanelData;
			for ( int i = 0; i < m_aPlayerPanels.Count(); i++ )
			{
				// get the player panel
				pPlayerPanelData = &m_aPlayerPanels[i];
				pPlayerPanelData->m_flPrevHealth = pPlayerPanelData->m_pHealthBar->GetValue();
				pPlayerPanelData->m_flCurHealth = pPlayerPanelData->m_pHealthBar->GetValue();
			}
		}

	}
	else if ( FStrEq( "player_spawn", szEventName ) )
	{
		if ( EventUserID == LocalPlayerID )
		{
			// we make sure they are hidden because even when a parent is hidden, we pay the cost of them blurring
			InitPlayerBGPanels();
		}
	}

	else if ( FStrEq( "hltv_status", szEventName ) )
	{
		// spectators = clients - proxies + external viewers
		m_HLTVSpectators = evt->GetInt( "clients" );
		m_HLTVSpectators -= evt->GetInt( "proxies" );
		m_HLTVSpectators += evt->GetInt( "externaltotal" );
	}
	else if ( FStrEq( "round_end", szEventName ) )
	{
		// Update the team timer one last time at round end, so we properly show "0:00" if the round timed out
		C_CSGameRules *pRules = CSGameRules();
		if ( pRules && m_pTimer )
		{
			int nTimer = static_cast< int >( floor( pRules->GetRoundRemainingTime() ) );
			if ( nTimer < 0 )
				nTimer = 0;

			m_pTimer->SetText( CFmtStr( "%d:%.2d", ( nTimer / 60 ), ( nTimer % 60 ) ) );

			int iReason = evt->GetInt( "reason", -1 );

			switch ( iReason )
			{
				case Bomb_Defused:
					m_bIsBombDefused = true;

				default:
					UpdatePlantedBombState( 0.0f );
			}
		}

		m_bRoundStarted = false;

		//HideDisplayTeamPanels();
	}
	else if ( FStrEq( "bomb_planted", szEventName ) )
	{
		if ( !CSGameRules()->IsPlayingCoopMission() && !CSGameRules()->IsPlayingSurvival() )
		{
			//HideTimer();
			UpdatePlantedBombState( 100.0f );
		}
	}
	else if ( FStrEq( "tournament_reward", szEventName ) )
	{
		// the items's defindex
		int nDefindex = evt->GetInt( "defindex" );
		m_nTourneyItemsDroppedTotal = evt->GetInt( "totalrewards" );
		AccountID_t uiAccountID = evt->GetInt( "accountid" );

		AccountID_t uiLocalAccountID = steamapicontext->SteamUser()->GetSteamID().GetAccountID();
		bool bIsLocalPlayer = false;
		if ( pLocalPlayer && uiLocalAccountID == uiAccountID )
			bIsLocalPlayer = true;

		if ( m_TournamentRewardTempList.Count() < MAX_PLAYER_ITEM_DROP_DISPLAY )
		{
			TournamentRewardTempList_t drop;
			drop.nDefindex = nDefindex;
			drop.uiAccountID = uiAccountID;
			drop.bIsLocalPlayer = bIsLocalPlayer;

			m_TournamentRewardTempList.AddToTail( drop );
		}

		m_nTourneyItemsDroppedThisRound++;
	}

	if ( FStrEq( "bot_takeover", szEventName ) || FStrEq( "spec_target_updated", szEventName ) || FStrEq( "hltv_changed_mode", szEventName ) )
	{
		m_bForceAvatarRefresh = true;

		// update the server image here as well so it happens more often
		DisplayEventServerImage();
	}
}

void CCSGO_HudSpectator::ForceRefreshPanels( void )
{
	m_bForceRefresh = true;
	
	bool bShow = ShouldShowSpecPlayerPanels();
	UpdateSpectateMode( bShow );

	// we make sure they are hidden because even when a parent is hidden, we pay the cost of them blurring
	InitPlayerBGPanels();

	static CPanoramaSymbol k_symSpectatorVisible( "hud-spectator--visible" );
	if ( bShow || BHasClass( k_symSpectatorVisible ) )
	{
		UpdatePlayerPanels();
		UpdateTimer();
		UpdateRounds();
		UpdateScoreAndTeamNames();
		UpdateEventDropsAndViewers();
	}

	m_bForceRefresh = false;
}

void CCSGO_HudSpectator::InitPlayerBGPanels( void )
{
	// this could be a general Init function
	CSpecPlayerPanelData *pPlayerPanelData;
	for ( int i = 0; i < m_aPlayerPanels.Count(); i++ )
	{
		// get the player panel
		pPlayerPanelData = &m_aPlayerPanels[i];
		if ( pPlayerPanelData->m_pPlayerPanelBG )
			pPlayerPanelData->m_pPlayerPanelBG->SetVisible( false );
	}
}

void CCSGO_HudSpectator::UpdateSpectateMode( bool bShow )
{
	static CPanoramaSymbol k_symSpectatorVisible( "hud-spectator--visible" );

	if ( bShow && !BHasClass( k_symSpectatorVisible ) )
	{
		m_bForceRefresh = true;

		// Show spectator ui
		AddClass( k_symSpectatorVisible );

		panorama::CPanel2D* pVignetting = CCSGO_Hud::GetInstance()->FindChildInLayoutFile( "HudSpectatorVignetting" );
		if ( pVignetting )
			pVignetting->RequireLoadLayout( "file://{resources}/layout/hud/hudspectator__vignetting.xml" );

		// When start to show, the user hasn't requested mouse input yet
		// (Currently no way for them to do this, so in competitive while dead you just don't have mouse control)
		m_bRequestedMouseInput = false;

		// this could be a general Init function
		CSpecPlayerPanelData *pPlayerPanelData;
		Assert( m_bValidPlayerPanels );		// Spectator panel not visible if we haven't initialized player's panels
											// ShouldShowSpecPlayerPanels returning false and therefore bShow should be false
											// LevelInit is initializing player's panels
		for ( int i = 0; i < m_aPlayerPanels.Count(); i++ )
		{
			// get the player panel
			pPlayerPanelData = &m_aPlayerPanels[i];
			if ( pPlayerPanelData )
				pPlayerPanelData->InitRedHealthBar();
		}

		// need to move the money panels before the player panels
		//MoveChildBefore( m_pTeamSpecTeamMoney_L, m_aPlayerPanels[0].m_pPanel );
		//MoveChildBefore( m_pTeamSpecTeamMoney_R, m_aPlayerPanels[5].m_pPanel );
	}
	else if ( !bShow && BHasClass( k_symSpectatorVisible ) )
	{
		RemoveClass( k_symSpectatorVisible );

		panorama::CPanel2D* pVignetting = CCSGO_Hud::GetInstance()->FindChildInLayoutFile( "HudSpectatorVignetting" );
		if ( pVignetting )
			pVignetting->UnloadLayout();
	}

	extern bool Helper_DisplayTeamsOnOppositeSides_Panorama( void );
	if ( CSGameRules() )
	{
		m_bSwapPlayerNames = Helper_DisplayTeamsOnOppositeSides_Panorama();
	}
}

bool CCSGO_HudSpectator::ShouldShowSpecPlayerPanels( void )
{
	// Do not show the spectator panel until player panels have been initialized (in LevelInit)
	if ( !m_bValidPlayerPanels )
		return false;
	
	C_BasePlayer * player = C_BasePlayer::GetLocalPlayer();
	if ( !player )
		return false;

	if ( cl_draw_only_deathnotices.GetBool() == true )
		return false;

	if ( cl_drawhud.GetBool() == false || CHudElement::ShouldDraw() == false )
		return false;

	// During halftime scoreboard visibility, don't show spectator ui
	if ( CCSGO_Scoreboard::GetInstance() && CCSGO_Scoreboard::GetInstance()->IsLockedOpen() )
		return false;

	// During end-of-match sequence, don't show spectator ui
	if ( CCSGO_EndOfMatch::GetInstance() && CCSGO_EndOfMatch::GetInstance()->IsOpen() )
		return false;

	CBasePlayer *pSpectatorTarget = UTIL_PlayerByIndex( GetSpectatorTarget() );
	if (( player->IsHLTV() || engine->IsPlayingDemo() ) && CSGameRules()->IsPlayingAnyCompetitiveStrictRuleset() 
		 && GetSpectatorMode() != OBS_MODE_NONE && pSpectatorTarget )
		return true;

//	if ( /*( player->IsHLTV() || engine->IsPlayingDemo() ) &&*/ CSGameRules()->IsPlayingSurvival()
//		 && GetSpectatorMode() != OBS_MODE_NONE && pSpectatorTarget )
//		return true;
//
	if ( pSpectatorTarget && player->IsPlayerDead() && CSGameRules()->IsPlayingAnyCompetitiveStrictRuleset() )
		return true;

	return false;
}

void CCSGO_HudSpectator::Think()
{
	//VPROF_BUDGET( "CCSGO_HudSpectator::HandleFrameUpdate", VPROF_BUDGETGROUP_TENFOOT );

	bool bShow = ShouldShowSpecPlayerPanels();

	UpdateSpectateMode( bShow );

	if( bShow )
	{
		UpdatePlayerPanels();
		UpdateTimer();
		UpdateRounds();
		UpdateScoreAndTeamNames();
		UpdateEventDropsAndViewers();
	}

	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pPlayer )
		return;

	bool bCursorInput =
		// cursor input requires visible spectator ui
		BIsVisible()
		// all other modes use mouse input for camera, so don't enable cursor input
		&& ( GetSpectatorMode() == OBS_MODE_IN_EYE || spec_autodirector.GetBool() )
		// local player requested mouse input (currently no way to enable this, so, always false)
		&& m_bRequestedMouseInput;
	
	// control mouse pointer input
	EnableCursorInput( bCursorInput );

	// 	if ( pPlayer->IsHLTV() && spec_autodirector.GetBool() )
// 	{
// 		if ( spec_autodirector_cameraman.GetInt() > 0 )
// 		{
// 			pDirector = panorama::UILocalize()->PchFindRawString( "#PANOHUD_Spectate_Navigation_New_Cameraman_On" );
// 		}
// 		else
// 		{
// 			pDirector = panorama::UILocalize()->PchFindRawString( "#PANOHUD_Spectate_Navigation_New_Director_On" );
// 		}
// 		if ( !pDirector )
// 		{
// 			pDirector = "";
// 		}
// 	}

	m_bHideMiniScoreboard = ( CCSGameRules::GetMaxPlayers() <= 10 ) && !( CSGameRules()->IsPlayingGunGameDeathmatch() || CSGameRules()->IsPlayingGunGameProgressive() );

	m_bForceRefresh = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCSGO_HudSpectator::EventAnimationEnd( const panorama::CPanelPtr< panorama::IUIPanel > &panelPtr, panorama::CPanoramaSymbol symAnimation )
{
	static const panorama::CPanoramaSymbol k_symExtraDataShowL( "PlayerPanel_ExtraDataShow_L" );
	static const panorama::CPanoramaSymbol k_symExtraDataShowR( "PlayerPanel_ExtraDataShow_R" );
	static const panorama::CPanoramaSymbol k_symExtraDataHideL( "PlayerPanel_ExtraDataHide_L" );
	static const panorama::CPanoramaSymbol k_symExtraDataHideR( "PlayerPanel_ExtraDataHide_R" );
	
	static const panorama::CPanoramaSymbol k_symEventDropsViewersRootHide( "EventDropsViewersRoot_FadeOut" );
	static const panorama::CPanoramaSymbol k_symEventDropsViewersRootShow( "EventDropsViewersRoot_FadeIn" );
	static const panorama::CPanoramaSymbol k_symEventLogoFadeIn( "EventLogoFadeIn" );
	static const panorama::CPanoramaSymbol k_symEventLogoFadeOut( "EventLogoFadeOut" );

	static const panorama::CPanoramaSymbol k_symTeamMoneyHideL( "TeamSpecPanel_TeamMoneyHide_L" );
	static const panorama::CPanoramaSymbol k_symTeamMoneyHideR( "TeamSpecPanel_TeamMoneyHide_R" );
	static const panorama::CPanoramaSymbol k_symTeamMoneyShowL( "TeamSpecPanel_TeamMoneyShow_L" );
	static const panorama::CPanoramaSymbol k_symTeamMoneyShowR( "TeamSpecPanel_TeamMoneyShow_R" );

	if ( symAnimation == k_symExtraDataShowL || symAnimation == k_symExtraDataShowR || symAnimation == k_symEventLogoFadeIn || symAnimation == k_symEventDropsViewersRootShow
		 || symAnimation == k_symTeamMoneyShowL || symAnimation == k_symTeamMoneyShowR )
	{
		panelPtr->SetHasClass( symAnimation, false );
	}
	else if ( symAnimation == k_symTeamMoneyHideL || symAnimation == k_symTeamMoneyHideR )
	{
		panorama::CPanel2D *pPanel = ToPanel2D( panelPtr.Get() );
		if ( pPanel )
			pPanel->SetOpacity(0);

		panelPtr->SetHasClass( symAnimation, false );		
	}
	else if ( symAnimation == k_symEventDropsViewersRootHide || symAnimation == k_symEventLogoFadeOut || symAnimation == k_symExtraDataHideR || symAnimation == k_symExtraDataHideL )
	{
		panelPtr->SetHasClass( symAnimation, false );
		panelPtr->SetVisible( false );
	}


	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCSGO_HudSpectator::HandleSelectClickedPlayer( const panorama::CPanelPtr< panorama::IUIPanel > &clickedPanel )
{
	panorama::CPanel2D* pPanel = ToPanel2D( clickedPanel.Get() );
	if ( !pPanel )
		return true;

	// Find the panel that was clicked
	int iPanelIdx = -1;
	FOR_EACH_VEC( m_aPlayerPanels, i )
	{
		if ( pPanel == m_aPlayerPanels[i].m_pPlayerPanelBG )
		{
			iPanelIdx = i;
			break;
		}
	}

	if ( iPanelIdx < 0 )
		return true;

	const CSpecPlayerPanelData& panelData = m_aPlayerPanels[iPanelIdx];
	if ( panelData.m_bWasDead || panelData.m_nPlayerIdx < 0 )
		return true;

	bool bWasAutodirector = spec_autodirector.GetBool();

	// Spectate that player
	engine->ClientCmd( CFmtStr( "spec_player %d", panelData.m_nPlayerIdx ) );

	// If autodirector was enabled, selecting a player will disable it and might disable mouse input.
	// Force mouse input to stay enabled by switching to in-eye camera.
	if ( bWasAutodirector )
		engine->ClientCmd( CFmtStr( "spec_mode %d", OBS_MODE_IN_EYE ) );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudSpectator::LevelInit()
{
	// if we're in spectator mode
	static CPanoramaSymbol k_symSpectatorVisible( "hud-spectator--visible" );

	//m_nMaxPlayers = 10;
	//m_bIsGunGame = false;

	FOR_EACH_VEC( m_aPlayerPanels, i )
	{
		CSpecPlayerPanelData& playerData = m_aPlayerPanels[i];
		delete playerData.m_pPanel;
	}
	m_aPlayerPanels.RemoveAll();
	m_aPlayerPanels.SetSize( MAX_TEAM_SIZE * 2 );

	int idxTOffset = MAX_TEAM_SIZE;
	for ( int idx = 0; idx < idxTOffset; idx++ )
	{
		// CT
		//InitAvatarSnippet( m_pTeamSpec_L, idx, true );
		InitPlayerPanelSnippet( m_pTeamSpec_L, idx, true );

		// T
		//InitAvatarSnippet( m_pTeamSpec_R, idx + idxTOffset, false );
		InitPlayerPanelSnippet( m_pTeamSpec_R, idx + idxTOffset, false );
	}
	m_bValidPlayerPanels = true;

	m_pTeamSpec_L->SetVisible( true );
	m_pTeamSpec_R->SetVisible( true );
	m_pEventLogoRight->SetVisible( false );

	m_bRoundStarted = false;

	// check if the round is already in progress when we first join it
	C_CSGameRules *pRules = CSGameRules();
	if ( pRules )
	{
		m_bRoundStarted = ( pRules->GetRoundStartTime() < gpGlobals->curtime );
	}

	// Reset all player tracking
	for ( int idx = 0; idx < MAX_TEAM_SIZE; ++idx )
	{
		m_CTTeam[idx].Reset();
		m_TerroristTeam[idx].Reset();
	}

	// need to move the money panels before the player panels
	//MoveChildBefore(m_pTeamSpecTeamMoney_L, m_aPlayerPanels[0].m_pPanel );
	//MoveChildBefore(m_pTeamSpecTeamMoney_R, m_aPlayerPanels[5].m_pPanel );

// 	if ( cl_server_graphic2_enable.GetBool() || Helper_GraphicEnabled() )
// 	{
// 		Helper_SetServerGraphic( m_pEventLogoRight, sv_server_graphic2.GetString() );
// 	}

	m_nTerroristTeamCount = 0;
	m_nCTTeamCount = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudSpectator::LevelShutdown()
{
	m_bValidPlayerPanels = false;
}

void CCSGO_HudSpectator::InitPlayerPanelSnippet( panorama::CPanel2D *pParent, int idx, bool bIsLeftSide )
{
	static const panorama::CPanoramaSymbol k_symPlayerPanelL( "PlayerPanelSnippet_L" );
	static const panorama::CPanoramaSymbol k_symPlayerPanelR( "PlayerPanelSnippet_R" );
	static const panorama::CPanoramaSymbol k_symAlignR( "PlayerPanel__Align_R" );	

	char szTmp[ 128 ] = {};

	CSpecPlayerPanelData* pSpecPanel = &m_aPlayerPanels[ idx ];

	V_sprintf_safe( szTmp, "PlayerPanel%d", idx );

	pSpecPanel->m_pPanel = new panorama::CPanel2D( pParent, szTmp );
	if ( bIsLeftSide )
	{
		pSpecPanel->m_pPanel->RequireLoadLayoutSnippet( "PlayerPanelSnippet_L" );
	}
	else
	{
		pSpecPanel->m_pPanel->RequireLoadLayoutSnippet( "PlayerPanelSnippet_R" );
	}
	
	pSpecPanel->m_pPanel->SetVisible( true );
	pSpecPanel->m_bPlayerExtraDataVisible = false;

	//pSpecPanel->m_pVisible = pSpecPanel->m_pPanel->RequireChildInLayoutFile( "Visible" );
	//if ( !bIsLeftSide )
	//{
	//	pSpecPanel->m_pVisible->SetHasClass( k_symAlignR, true );
	//}

	//pSpecPanel->m_pInvisible = pSpecPanel->m_pPanel->RequireChildInLayoutFile( "Invisible" );
	
	pSpecPanel->m_pPlayerPanelParent = pSpecPanel->m_pPanel->RequireChildInLayoutFile( "PlayerPanelParent" );
	pSpecPanel->m_pPlayerPanelBG = pSpecPanel->m_pPanel->RequireChildInLayoutFile( "PlayerPanelBG" );

	pSpecPanel->m_pHighlight = pSpecPanel->m_pPanel->RequireChildInLayoutFile( "PlayerPanelHighlight" );
	pSpecPanel->m_pDeadBG = pSpecPanel->m_pPanel->RequireChildInLayoutFile( "PlayerPanelDeadBG" );
	pSpecPanel->m_pAvatarBucket = pSpecPanel->m_pPanel->RequireChildInLayoutFile( "PlayerPanelAvatarBucket" );
	pSpecPanel->m_pHealthBar = bIsLeftSide ? panorama::panel_cast<CProgressBar *>( pSpecPanel->m_pPanel->RequireChildInLayoutFile( "PlayerPanelHealthBar_Left" ) ) : 
		panorama::panel_cast<CProgressBar *>( pSpecPanel->m_pPanel->RequireChildInLayoutFile( "PlayerPanelHealthBar_Right" ) );
	pSpecPanel->m_pHealthBar->SetMin( 0.0f );
	pSpecPanel->m_pHealthBar->SetMax( 100.0f );

	pSpecPanel->m_flPrevHealth = 100;
	pSpecPanel->m_flCurHealth = 100;
	pSpecPanel->m_pHealthBarRed = panorama::panel_cast< CProgressBar * >( pSpecPanel->m_pPanel->RequireChildInLayoutFile( "PlayerPanelHealthBar_Red" ) );
	pSpecPanel->m_pHealthBarRed->SetMin( 0.0f );
	pSpecPanel->m_pHealthBarRed->SetMax( 100.0f );

	pSpecPanel->m_pHealthText = panorama::panel_cast<panorama::CLabel *>( pSpecPanel->m_pPanel->RequireChildInLayoutFile( "HealthText" ) );
	pSpecPanel->m_pPlayerName = panorama::panel_cast<panorama::CLabel *>( pSpecPanel->m_pPanel->RequireChildInLayoutFile( "PlayerName" ) );
	pSpecPanel->m_pMoneyText = panorama::panel_cast<panorama::CLabel *>( pSpecPanel->m_pPanel->RequireChildInLayoutFile( "MoneyText" ) );	

	pSpecPanel->m_pArmorPanel = pSpecPanel->m_pPanel->RequireChildInLayoutFile( "ArmorIcons" );
	pSpecPanel->m_pArmorIcon = panorama::panel_cast< panorama::CImagePanel * >( pSpecPanel->m_pPanel->RequireChildInLayoutFile( "ArmorIcon" ) );

	pSpecPanel->m_pC4DefuserIcon = panorama::panel_cast< panorama::CImagePanel * >( pSpecPanel->m_pPanel->RequireChildInLayoutFile( "C4DefuserIcon" ) );	

	pSpecPanel->m_pKillPanel = panorama::panel_cast< panorama::CImagePanel * >( pSpecPanel->m_pPanel->RequireChildInLayoutFile( "KillIcon" ) );
	pSpecPanel->m_pKillText = panorama::panel_cast<panorama::CLabel *>( pSpecPanel->m_pPanel->RequireChildInLayoutFile( "KillText" ) );

	pSpecPanel->m_pWeaponIcons.SetSize( MAX_PLAYERPANEL_WEAPONS );
	char szWepTmp[128] = {};
	for ( int i = 0; i < 8; i++ )
	{
		V_sprintf_safe( szWepTmp, "Weapon%d", i+1 );
		pSpecPanel->m_pWeaponIcons[ i ] = panorama::panel_cast< panorama::CImagePanel * >( pSpecPanel->m_pPanel->RequireChildInLayoutFile( szWepTmp ) );
	}

	pSpecPanel->m_pWeaponMain = panorama::panel_cast< panorama::CImagePanel* >( pSpecPanel->m_pPanel->FindChildInLayoutFile( "PlayerPanel_MainWeapon" ) );

	pSpecPanel->m_pExtraDataPanel = pSpecPanel->m_pPanel->RequireChildInLayoutFile( "PlayerPanelExtraDataPanel" );
	pSpecPanel->m_pExtraDataPanel_K = panorama::panel_cast<panorama::CLabel *>( pSpecPanel->m_pPanel->RequireChildInLayoutFile( "KAD_NumK" ) );
	pSpecPanel->m_pExtraDataPanel_A = panorama::panel_cast<panorama::CLabel *>( pSpecPanel->m_pPanel->RequireChildInLayoutFile( "KAD_NumA" ) );
	pSpecPanel->m_pExtraDataPanel_D = panorama::panel_cast<panorama::CLabel *>( pSpecPanel->m_pPanel->RequireChildInLayoutFile( "KAD_NumD" ) );
	pSpecPanel->m_pExtraDataPanel_MoneySpent = panorama::panel_cast<panorama::CLabel *>( pSpecPanel->m_pPanel->RequireChildInLayoutFile( "KAD_MoneySpent" ) );

	if ( bIsLeftSide )
	{
		pSpecPanel->m_pPanel->SetHasClass( k_symPlayerPanelL, true );
	}
	else
	{
		pSpecPanel->m_pPanel->SetHasClass( k_symPlayerPanelR, true );
	}

	pSpecPanel->m_pAvatar = InitAvatarSnippet( pSpecPanel, idx, bIsLeftSide );

	if ( m_pHudBlurTargetPanel && pSpecPanel->m_pPlayerPanelBG )
	{
		m_pHudBlurTargetPanel->AddBlurPanel( pSpecPanel->m_pPlayerPanelBG );
	}
	pSpecPanel->m_pPlayerPanelBG->SetVisible(false);
}

CAvatarPanelData *CCSGO_HudSpectator::InitAvatarSnippet( CSpecPlayerPanelData *pParent, int idx, bool bIsLeftSide )
{
	static const panorama::CPanoramaSymbol k_symAvatarDefaultBGCT( "AvatarSpec__Default--CT" );
	static const panorama::CPanoramaSymbol k_symAvatarDefaultBGT( "AvatarSpec__Default--T" );

	static const panorama::CPanoramaSymbol k_symScaleSmaller( "AvatarSpecSnippet" );

	char szTmp[ 128 ] = {};

	CAvatarPanelData *pAvatar = new CAvatarPanelData();
	//pParent->m_pAvatar = pAvatar;

	V_sprintf_safe( szTmp, "Avatar%d", idx );
	//pAvatar.Init( pParent->m_pAvatarBucket, szTmp );

	pAvatar->m_pPanel = new panorama::CPanel2D( pParent->m_pAvatarBucket, szTmp );
	pAvatar->m_pPanel->RequireLoadLayoutSnippet( "AvatarSpecSnippet" );
	pAvatar->m_pPanel->SetVisible( true );

	//pAvatar->m_pVisible = pAvatar->m_pPanel->RequireChildInLayoutFile( "Visible" );
	//pAvatar->m_pInvisible = pAvatar->m_pPanel->RequireChildInLayoutFile( "Invisible" );

	pAvatar->m_pAvatarBG = pAvatar->m_pPanel->RequireChildInLayoutFile( "AvatarSpec__ImageBG" );
	pAvatar->m_pDynamicAvatar = panorama::panel_cast<CCSGO_AvatarImage *>( pAvatar->m_pPanel->RequireChildInLayoutFile( "AvatarImage" ) );

	pAvatar->m_pHealth = panorama::panel_cast<CCSGO_AvatarHealthBar *>( pAvatar->m_pPanel->RequireChildInLayoutFile( "HealthBar" ) );
	pAvatar->m_pBot = panorama::panel_cast<panorama::CImagePanel *>( pAvatar->m_pPanel->RequireChildInLayoutFile( "Bot" ) );
	pAvatar->m_pSkull = panorama::panel_cast< panorama::CImagePanel * >( pAvatar->m_pPanel->RequireChildInLayoutFile( "Skull" ) );
	pAvatar->m_pPlayerColor = panorama::panel_cast<panorama::CImagePanel *>( pAvatar->m_pPanel->RequireChildInLayoutFile( "PlayerColor" ) );
	//pAvatar->m_pSound = pAvatar->m_pPanel->RequireChildInLayoutFile( "Sound" );
	pAvatar->m_pPlayerLetter = panorama::panel_cast<panorama::CLabel *>( pAvatar->m_pPanel->RequireChildInLayoutFile( "PlayerLetter" ) );
	pAvatar->m_pPlayerNumber = panorama::panel_cast<panorama::CLabel *>( pAvatar->m_pPanel->RequireChildInLayoutFile( "PlayerNumber" ) );
	//pAvatar->m_pDominated = panorama::panel_cast<panorama::CImagePanel *>( pAvatar->m_pPanel->RequireChildInLayoutFile( "Dominated" ) );
	//pAvatar->m_pNemesis = panorama::panel_cast<panorama::CImagePanel *>( pAvatar->m_pPanel->RequireChildInLayoutFile( "Nemesis" ) );
	//pAvatar->m_pArsenalProgressAbove = pAvatar->m_pPanel->RequireChildInLayoutFile( "ArsenalProgressAbove" );
	//pAvatar->m_pArsenalProgressBelow = pAvatar->m_pPanel->RequireChildInLayoutFile( "ArsenalProgressBelow" );
	pAvatar->m_pArsenalProgress = NULL;  // this will point to one of the above two when setting the hud position

	pAvatar->m_pHealth->SetMin( 0.0f );
	pAvatar->m_pHealth->SetMax( 100.0f );

	pAvatar->m_pPanel->SetHasClass( k_symScaleSmaller, true );

	// set default background image
	if ( pAvatar->m_pAvatarBG )
	{
		if ( bIsLeftSide )
		{
			pAvatar->m_pAvatarBG->SetHasClass( k_symAvatarDefaultBGCT, true );
		}
		else
		{
			pAvatar->m_pAvatarBG->SetHasClass( k_symAvatarDefaultBGT, true );
		}
	}

	if ( !bIsLeftSide )
	{
		// right side, right justify
		static const panorama::CPanoramaSymbol k_symAvatarPlayerNumberR( "AvatarSpec__PlayerNumber_R" );
		pAvatar->m_pPlayerNumber->SetHasClass( k_symAvatarPlayerNumberR, true );
	}

	pAvatar->m_bInitialized = true;
	
	return pAvatar;
}

int SpecPlayerWeaponSortFunction( C_BaseCombatWeapon* const* entry1, C_BaseCombatWeapon* const* entry2 )
{
	if ( entry1 == NULL || ( *entry1 ) == NULL )
		return -1;

	if ( entry2 == NULL || ( *entry2 ) == NULL )
		return 1;

	// Higher GG Progressive weapon ranks higher.  In case of ties for that, we rank according to player index so 
	//   we don't overly shuffle the ordering
	if ( ( *entry1 )->GetGearSlot() > ( *entry2 )->GetGearSlot() )
		return 1;
	else if ( ( *entry1 )->GetGearSlot() < ( *entry2 )->GetGearSlot() )
		return -1;
	else
	{
		if ( ( *entry1 )->GetGearSlotPosition() > ( *entry2 )->GetGearSlotPosition() )
			return 1;
		else// if ( ( *entry1 )->GetGearSlot() < ( *entry2 )->GetGearSlot() )
			return -1;
	}
}

void CCSGO_HudSpectator::UpdatePlayerPanels( void )
{
	if ( !CSGameRules() )
		return;

	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pLocalPlayer || !BIsVisible() )
	{
		return;
	}

	bool bHidePlayerPanels = spec_hide_players.GetBool();

	CCSGO_Scoreboard *pScoreboard = CCSGO_Scoreboard::GetInstance();
	if ( bHidePlayerPanels || (pScoreboard && pScoreboard->BIsVisible()) )
	{
		m_pTeamSpec_L->SetVisible(false);
		m_pTeamSpec_R->SetVisible(false);
		m_pTeamSpecTeamMoney_L->SetVisible(false);
		m_pTeamSpecTeamMoney_R->SetVisible( false );

		m_bWasHidden = true;
		return;
	}

	m_pTeamSpec_L->SetVisible( true );
	m_pTeamSpec_R->SetVisible( true );

	C_CS_PlayerResource* pCSPR = ( C_CS_PlayerResource* )g_PR;

	//m_nTeamSelectionLastUpdate = pLocalPlayer->GetTeamNumber();	

	int spectatedTargetIndex = -1;
	if ( GetSpectatorMode() == OBS_MODE_IN_EYE || GetSpectatorMode() == OBS_MODE_CHASE )
	{
		spectatedTargetIndex = GetSpectatorTarget();
	}

	int	nTerroristTeamCount = 0;
	int	nCTTeamCount = 0;

	sAvatarInitData avatarData;

	if ( pLocalPlayer )
	{
		int LocalBotControlledIdx = -1;
		if ( pLocalPlayer->IsControllingBot() )
		{
			LocalBotControlledIdx = pLocalPlayer->GetControlledBotIndex();
		}

		// WIP team's equipment value.
		int iCurrentEquipValue[ 2 ];
		memset( iCurrentEquipValue, 0, sizeof(iCurrentEquipValue) );
		int iCurrentTeamMoney[2];
		memset( iCurrentTeamMoney, 0, sizeof( iCurrentTeamMoney ) );

		for ( int playerIndex = 1; playerIndex <= MAX_PLAYERS; playerIndex++ )
		{
			if ( !g_PR )
				break;

			if ( g_PR->IsConnected( playerIndex ) )
			{
				int TeamId = g_PR->GetTeam( playerIndex );

				// Mini-Scoreboard only reflects the active players, not spectators or those who haven't selected a team
				if ( TeamId != TEAM_CT && TeamId != TEAM_TERRORIST )
					continue;

				// WIP team's equipment value.
				C_CSPlayer *player = ToCSPlayer(UTIL_PlayerByIndex( playerIndex ));
				if ( player && pLocalPlayer->GetTeamNumber() != TEAM_CT && pLocalPlayer->GetTeamNumber() != TEAM_TERRORIST )
				{
					iCurrentEquipValue[ player->GetTeamNumber() - 2 ] += player->IsAlive() ? player->GetCurrentEquipmentValue() : 0;
					iCurrentTeamMoney[ player->GetTeamNumber() - 2 ] += player->GetAccount();
					//iFreezetimeEndEquipValue[ player->GetTeamNumber() - 2 ] += player->GetFreezetimeEndEquipmentValue();
				}

				bool bSpeaking = false;

				bool bIsCT = ( TeamId == TEAM_CT );

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

				bool bIsSpectating = false; 

				int nHealth = 0; 
				int nArmor = 0;
				int nMoney = 0;
				bool bHasHelmet = false;
				bool bHasDefBomb = false;
				CSWeaponID nSelectedWeaponID = WEAPON_NONE;
				CSWeaponID nPrimaryWeaponID = WEAPON_NONE;
				const char *szPrimaryWeaponName = "";
				char szFinalWeaponString[512];
				szFinalWeaponString[0] = 0;
				int nSecondarySelectedSlot = -1;

				uint64 nFinalWeaponStringInt = 0;
				
				int nRoundKills = -1;

				PlayerSpecStatus_t *ms = NULL;

				int slotIdx = -1;
				int nPlayerPanelSlot = -1;

				if ( bIsCT )
				{
					// this team is full - we have no more space for players in our roster
					if ( nCTTeamCount == MAX_TEAM_SIZE )
						continue;

					nPlayerPanelSlot = nCTTeamCount;
					slotIdx = nCTTeamCount++;
				}
				else
				{
					// this team is full - we have no more space for players in our roster
					if ( nTerroristTeamCount == MAX_TEAM_SIZE )
						continue;

					nPlayerPanelSlot = nTerroristTeamCount + MAX_TEAM_SIZE;
					slotIdx = nTerroristTeamCount++;
				}

				if ( pCSPR->GetControlledPlayer( playerIndex ) == 0 )
				{
					int playerHealthIndex = playerIndex;
					int controlledByIndex = pCSPR->GetControlledByPlayer( playerIndex );

					if ( controlledByIndex != 0 )
					{
						playerHealthIndex = controlledByIndex;
					}

					bIsSpectating = ( playerHealthIndex == spectatedTargetIndex );
				
					C_CSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( playerHealthIndex ) );
					if ( pPlayer )
					{
						nRoundKills = pPlayer->GetNumRoundKills();
						nMoney = pPlayer->GetAccount();
					}

					if ( g_PR->IsAlive( playerHealthIndex ) )
					{
						nHealth = pCSPR->GetHealth( playerHealthIndex );
						nArmor = pCSPR->GetArmor( playerHealthIndex );						

						bHasHelmet = pCSPR->HasHelmet( playerHealthIndex );
						bHasDefBomb = (pCSPR->HasC4( playerHealthIndex ) || pCSPR->HasDefuser( playerHealthIndex ));

						if ( pPlayer )
						{
							if ( pPlayer->GetActiveCSWeapon() )
							{
								nSelectedWeaponID = pPlayer->GetActiveCSWeapon()->GetCSWeaponID();
								nPrimaryWeaponID = WEAPON_KNIFE;
								szPrimaryWeaponName = "knife";

// 									if ( nPrimaryWeaponID == nSelectedWeaponID )
// 									{
// 										C_WeaponCSBase *pWeapon = pPlayer->GetActiveCSWeapon();
// 										CEconItemView *pItem = pWeapon->GetEconItemView();
// 										szPrimaryWeaponName = ( ( pItem && pItem->IsValid() && pItem->GetItemIndex() && pItem->GetItemDefinition() )
// 																? pItem->GetItemDefinition()->GetDefinitionName()
// 																: pWeapon->GetClassname() );
// 									}
// 									else
								{
									// While in the freeze period we want to show their primary weapon if they have one
									for ( int i = 0; i < MAX_WEAPONS; i++ )
									{						
										C_WeaponCSBase *pWeapon = assert_cast< C_WeaponCSBase* >( pPlayer->GetWeapon( i ) );
										if ( !pWeapon )
											continue;

										const CEconItemView *pItem = pWeapon->GetEconItemView();
										CSWeaponID nIDTemp = pWeapon->GetCSWeaponID();

										if ( IsPrimaryWeapon( pItem ) )
										{
											// Found a primary
											nPrimaryWeaponID = nIDTemp;
											szPrimaryWeaponName = ( ( pItem && pItem->IsValid() && pItem->GetItemIndex() && pItem->GetItemDefinition() )
												? pItem->GetItemDefinition()->GetDefinitionName()
												: pWeapon->GetClassname() );
											break;
										}
										else if ( IsSecondaryWeapon( pItem ) )
										{
											// Fall back to secondary if non-primary and non-secondary is active
											nPrimaryWeaponID = nIDTemp;
											szPrimaryWeaponName = ( ( pItem && pItem->IsValid() && pItem->GetItemIndex() && pItem->GetItemDefinition() )
												? pItem->GetItemDefinition()->GetDefinitionName()
												: pWeapon->GetClassname() );
										}
									}

									if ( nPrimaryWeaponID == WEAPON_KNIFE && nPrimaryWeaponID == nSelectedWeaponID )
									{
										C_WeaponCSBase *pWeapon = pPlayer->GetActiveCSWeapon();
										CEconItemView *pItem = pWeapon->GetEconItemView();
										szPrimaryWeaponName = ( ( pItem && pItem->IsValid() && pItem->GetItemIndex() && pItem->GetItemDefinition() )
																? pItem->GetItemDefinition()->GetDefinitionName()
																: pWeapon->GetClassname() );
									}
								}

								CUtlVector<C_BaseCombatWeapon*> equippedWeaponList;
								// create a list of the player's weapons
								for ( int i = 0; i < pPlayer->WeaponCount(); i++ )
								{
									C_WeaponCSBase *pWeapon = dynamic_cast<C_WeaponCSBase*>( pPlayer->GetWeapon( i ) );
									// if we dont have a weapon OR if the found weapon is our selected weapon, skip it
									if ( !pWeapon || pWeapon->GetCSWeaponID() == nPrimaryWeaponID || pWeapon->GetCSWeaponID() == WEAPON_KNIFE || pWeapon->GetCSWeaponID() == WEAPON_C4 )
										continue;

 									int nCount = 1;
 									//CWeaponCSBase* pCSWeapon = static_cast< CWeaponCSBase * >( pWeapon );
 									if ( pWeapon && pWeapon->IsKindOf( WEAPONTYPE_GRENADE ) )
 										nCount = pWeapon->GetReserveAmmoCount( AMMO_POSITION_PRIMARY );
 
 									for ( int nGren = 0; nGren < nCount; nGren++ )
 										equippedWeaponList.AddToTail( pWeapon );
									
								}

								// sort them
								equippedWeaponList.Sort( SpecPlayerWeaponSortFunction );

								// now go through the list and build the string for the ui to parse and display
								for ( int i = 0; i < equippedWeaponList.Count(); i++ )
								{
									C_WeaponCSBase *pWeapon = dynamic_cast< C_WeaponCSBase* >( equippedWeaponList[i] );

									CEconItemView *pItem = pWeapon->GetEconItemView();
									const char *szWeapon = ( ( pItem && pItem->IsValid() && pItem->GetItemIndex() && pItem->GetItemDefinition() )
																? pItem->GetItemDefinition()->GetDefinitionName()
																: pWeapon->GetClassname() );

									if ( nSelectedWeaponID == pWeapon->GetCSWeaponID() )
										nSecondarySelectedSlot = i;

									// string the "weapon_"
									if ( IsWeaponClassname( szWeapon ) )
										szWeapon += WEAPON_CLASSNAME_PREFIX_LENGTH;

									if ( Q_strcmp( "knife", szWeapon ) == 0 )
									{
										if ( pPlayer && pPlayer->GetTeamNumber() == TEAM_CT )
										{
											szWeapon = "knife_default_ct";
										}
										else
										{
											szWeapon = "knife_default_t";
										}
									}

									// need to keep track of this
									//bool bSelected = ( pWeapon->GetCSWeaponID() == nSelectedWeaponID );

									// if we aren't on the first entry, add a comma
									if ( i > 0 )
										Q_strncat( szFinalWeaponString, ",", sizeof( szFinalWeaponString ), COPY_ALL_CHARACTERS );

									Q_strncat( szFinalWeaponString, szWeapon, sizeof( szFinalWeaponString ), COPY_ALL_CHARACTERS );

									// keep a string so we can easily see if our weapon list changed without comparing strings
									// make sure we don't overflow
									Assert( 0 == ( nFinalWeaponStringInt & 0xFF0000000000ull ) );
									nFinalWeaponStringInt = ( nFinalWeaponStringInt << 8 ) | ( uint64 )WeaponIDToByte( pWeapon->GetCSWeaponID() );
								}

								equippedWeaponList.RemoveAll();
							}
						}
					}
				}

				// Grab from the team container
				if ( bIsCT )
					ms = &m_CTTeam[slotIdx];
				else
					ms = &m_TerroristTeam[slotIdx];

				bool bLeftSide = bIsCT;
				if ( m_bSwapPlayerNames )
					bLeftSide = !bLeftSide;

				HandleExtraData( bLeftSide, ms, slotIdx );

				bool bIsPrimarySelected = (nSelectedWeaponID == nPrimaryWeaponID);

				bool bRefresh = ms->Update( 
					playerXuid,
					playerIndex,
					bIsPrimarySelected,
					nSecondarySelectedSlot,
					szPrimaryWeaponName,
					nHealth,
					nArmor,
					nMoney,
					bIsCT,
					bHasDefBomb,
					bHasHelmet,
					bDead,
					bSpeaking,
					( LocalBotControlledIdx == playerIndex ),
					bIsSpectating,
					nRoundKills,
					szFinalWeaponString,
					nFinalWeaponStringInt
				);

				if ( m_bForceRefresh || m_bWasHidden )
					bRefresh = true;

				// if something changed, update everything
				if ( bRefresh )
 					InvokePlayerSlotUpdate( avatarData, ms, nPlayerPanelSlot, slotIdx );
			}
		}

		// this is the MASTER BOOL
		bool bShowValue = ( CSGameRules()->IsPlayingClassic() && pLocalPlayer->IsSpectator() ) || engine->IsHLTV();

		int nValueT = iCurrentEquipValue[0];
		int nValueCT = iCurrentEquipValue[1];
		int nMoneyT = iCurrentTeamMoney[0];
		int nMoneyCT = iCurrentTeamMoney[1];

		if ( bShowValue && ShouldShowSpecPlayerPanels() )
		{
			m_pEQValueText_L->SetText( CFmtStr( "$%d", m_bSwapPlayerNames ? nValueT : nValueCT ) );
			m_pEQValueText_R->SetText( CFmtStr( "$%d", m_bSwapPlayerNames ? nValueCT : nValueT ) );

			m_pTeamMoneyText_L->SetText( CFmtStr( "$%d", m_bSwapPlayerNames ? nMoneyT : nMoneyCT ) );
			m_pTeamMoneyText_R->SetText( CFmtStr( "$%d", m_bSwapPlayerNames ? nMoneyCT : nMoneyT ) );
		}
		else
		{
			m_pTeamSpecTeamMoney_L->SetOpacity(0);
			m_pTeamSpecTeamMoney_R->SetOpacity(0);
		}

		// show all the extra match data under the top scoreboard
		ShowExtraMatchStats( bShowValue, iCurrentEquipValue[ 0 ], iCurrentEquipValue[ 1 ] );

		m_bForceAvatarRefresh = false;
	}

	//m_ggSortedList.RemoveAll();

	if ( ( nTerroristTeamCount == 0 ) ||
		 ( nTerroristTeamCount != m_nTerroristTeamCount && nTerroristTeamCount < MAX_TEAM_SIZE ) )
	{
		// Clear the player index
		for ( int Idx = 0; Idx < MAX_TEAM_SIZE; ++Idx )
		{
			m_TerroristTeam[Idx].nPlayerIdx = -1;

			bool bLeftSide = false;
			if ( m_bSwapPlayerNames )
				bLeftSide = !bLeftSide;

			int idx;
			// Offset to the next available team-based slot
			if ( bLeftSide )
				idx = Idx;
			else
				idx = Idx + MAX_TEAM_SIZE;

			CSpecPlayerPanelData* pSpecPanel = &m_aPlayerPanels[ idx ];
			if ( pSpecPanel )
				pSpecPanel->m_pPanel->SetVisible(false);
		}
	}

	if ( ( nCTTeamCount == 0 ) ||
		 ( nCTTeamCount != m_nCTTeamCount && nCTTeamCount < MAX_TEAM_SIZE ) )
	{
		// Clear the player index
		for ( int Idx = 0; Idx < MAX_TEAM_SIZE; ++Idx )
		{
			m_CTTeam[Idx].nPlayerIdx = -1;

			bool bLeftSide = true;
			if ( m_bSwapPlayerNames )
				bLeftSide = !bLeftSide;

			int idx;
			// Offset to the next available team-based slot
			if ( bLeftSide )
				idx = Idx;
			else
				idx = Idx + MAX_TEAM_SIZE;

			CSpecPlayerPanelData* pSpecPanel = &m_aPlayerPanels[idx];
			if ( pSpecPanel )
				pSpecPanel->m_pPanel->SetVisible( false );
		}
	}

	// show the money and extra data for players during the freeze period
	if ( CSGameRules()->IsFreezePeriod() && CSGameRules()->CanSpendMoneyInMap() )
	{
		bool bRenderForSpectator = CanSeeSpectatorOnlyTools();// && spec_show_xray.GetInt();
		if ( bRenderForSpectator )
		{
			if ( m_bTeamMoneyVisible == false )
			{
				//m_pTeamSpecTeamMoney_L->SetVisible( true );
				//m_pTeamSpecTeamMoney_R->SetVisible( true );
				m_pTeamSpecTeamMoney_L->SetOpacity( 1.0f );
				m_pTeamSpecTeamMoney_R->SetOpacity( 1.0f );

				// show the extra data
				static const panorama::CPanoramaSymbol k_symTeamMoneyShowL( "TeamSpecPanel_TeamMoneyShow_L" );
				static const panorama::CPanoramaSymbol k_symTeamMoneyShowR( "TeamSpecPanel_TeamMoneyShow_R" );
				m_pTeamSpecTeamMoney_L->TriggerClass( k_symTeamMoneyShowL );
				m_pTeamSpecTeamMoney_R->TriggerClass( k_symTeamMoneyShowR );

				// fade out the event logo
// 				static const panorama::CPanoramaSymbol k_symEventLogoFadeOut( "EventLogoFadeOut" );
// 				if ( V_strlen( sv_server_graphic2.GetString() ) > 0 && (cl_server_graphic2_enable.GetBool() /*|| Helper_GraphicEnabled()*/) )
// 				{
// 					m_pEventLogoRight->TriggerClass( k_symEventLogoFadeOut );
// 				}

				m_bTeamMoneyVisible = true;
			}
		}
	}
	else if ( m_bTeamMoneyVisible == true )
	{
		static const panorama::CPanoramaSymbol k_symExtraDataHideL( "TeamSpecPanel_TeamMoneyHide_L" );
		static const panorama::CPanoramaSymbol k_symExtraDataHideR( "TeamSpecPanel_TeamMoneyHide_R" );
		m_pTeamSpecTeamMoney_L->TriggerClass( k_symExtraDataHideL );
		m_pTeamSpecTeamMoney_R->TriggerClass( k_symExtraDataHideR );

		// fade out the event logo
// 		static const panorama::CPanoramaSymbol k_symEventLogoFadeIn( "EventLogoFadeIn" );
// 		if ( V_strlen( sv_server_graphic2.GetString() ) > 0 && (cl_server_graphic2_enable.GetBool() /*|| Helper_GraphicEnabled()*/) )
// 		{
// 			m_pEventLogoRight->SetVisible( true );
// 			m_pEventLogoRight->TriggerClass( k_symEventLogoFadeIn );
// 		}

		m_bTeamMoneyVisible = false;
	}	

	bool bShowTeamSpecMoney = ( !spec_hide_players.GetBool() && CSGameRules()->IsFreezePeriod() && CSGameRules()->CanSpendMoneyInMap() && CanSeeSpectatorOnlyTools() && m_bTeamMoneyVisible );
	m_pTeamSpecTeamMoney_L->SetVisible( bShowTeamSpecMoney );
	m_pTeamSpecTeamMoney_R->SetVisible( bShowTeamSpecMoney );

	m_nTerroristTeamCount = nTerroristTeamCount;
	m_nCTTeamCount = nCTTeamCount;

	m_bWasHidden = false;
}

void CCSGO_HudSpectator::ShowExtraMatchStats( bool bShowValue, int nEQValueT, int nEQValueCT )
{
	// stats
	char const *szMatchStatTxt = CSGameRules()->GetMatchStatTeamsTxt();
	bool bShowStats = bShowValue && szMatchStatTxt && *szMatchStatTxt;
	if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
	{
		if ( pParameters->m_bAnonymousPlayerIdentity )
			bShowStats = false;
	}

	// win percentage
	int nPct = bShowValue ? CSGameRules()->GetTournamentPredictionsPct() : 0;
	if ( nPct && CSGameRules()->AreTeamsPlayingSwitchedSides() )
		nPct = 100 - nPct;

	char const *szPredictionTxt = CSGameRules()->GetTournamentPredictionsTxt();
	bool bShowPct = bShowValue && !bShowStats && ( nPct != 0 ) && szPredictionTxt && *szPredictionTxt;
	if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
	{
		if ( pParameters->m_bAnonymousPlayerIdentity )
			bShowPct = false;
	}

	// team values
	bool bShowTeamValues = bShowValue;
	if ( bShowStats || bShowPct )
		bShowTeamValues = false;

	// set the NumberOfMatchesWonPips
	// if the # is > 0
	// show the matches won stats
	// else show stats
	// else show pct
	// else show team values?

	m_pExtraMatchData->SetVisible( false );
	m_pPickemPredictionsPanel->SetVisible( false );
	m_pBestOfPipsPanel->SetVisible(false);
	if ( bShowStats )
	{
		m_pExtraMatchData->SetVisible( true );

		char const *szStatR = GetGlobalTeam( TEAM_TERRORIST ) ? GetGlobalTeam( TEAM_TERRORIST )->m_szTeamMatchStat : "";
		char const *szStatL = GetGlobalTeam( TEAM_CT ) ? GetGlobalTeam( TEAM_CT )->m_szTeamMatchStat : "";

		static const panorama::CPanoramaSymbol k_symColorCT( "SpecColor_CT" );
		static const panorama::CPanoramaSymbol k_symColorT( "SpecColor_T" );
		if ( m_bSwapPlayerNames )
		{
			if ( m_pExtraMatchDataTextR->BHasClass( k_symColorCT ) == false )
				m_pExtraMatchDataTextR->AddClass( k_symColorCT );

			if ( m_pExtraMatchDataTextL->BHasClass( k_symColorT ) == false )
				m_pExtraMatchDataTextL->AddClass( k_symColorT );

			szStatL = GetGlobalTeam( TEAM_TERRORIST ) ? GetGlobalTeam( TEAM_TERRORIST )->m_szTeamMatchStat : "";
			szStatR = GetGlobalTeam( TEAM_CT ) ? GetGlobalTeam( TEAM_CT )->m_szTeamMatchStat : "";
		}
		else
		{
			if ( m_pExtraMatchDataTextR->BHasClass( k_symColorCT ) == true )
				m_pExtraMatchDataTextR->RemoveClass( k_symColorCT );

			if ( m_pExtraMatchDataTextL->BHasClass( k_symColorT ) == true )
				m_pExtraMatchDataTextL->RemoveClass( k_symColorT );
		}
/*
		extern wchar_t const * Helper_ResolveLocalizationStringWithFormattingParameters( wchar_t *wch, size_t wchsize, char const *pchToken );

		wchar_t wszMatchStatTxt[MAX_PATH] = {};
		if ( szMatchStatTxt && ( szMatchStatTxt[0] == '#' ) )
		{
			wchar_t const *wsz;

			// Special case for "Best of %count%" style strings, some languages require a ""%numtowin% To Win" alternative
			// Add {numtowin=%d} to formatting parameters
			const char* pCount = strstr( szMatchStatTxt, "{count=" );
			if ( pCount )
			{
				int nCount = V_atoi( pCount + V_strlen( "{count=" ) );
				int nNumToWin = ( nCount + 1) / 2;
				wsz = Helper_ResolveLocalizationStringWithFormattingParameters( wszMatchStatTxt, sizeof( wszMatchStatTxt ), CFmtStr("%s{numtowin=%d}", szMatchStatTxt , nNumToWin));
			}
			else
			{
				wsz = Helper_ResolveLocalizationStringWithFormattingParameters( wszMatchStatTxt, sizeof( wszMatchStatTxt ), szMatchStatTxt );
			}

			char szUTF8[4 * 512];
			V_UnicodeToUTF8( wsz, szUTF8, ARRAYSIZE( szUTF8 ) );

			m_pExtraMatchDataText->SetText( szUTF8 );
		}
		else
		{
			m_pExtraMatchDataText->SetText( szMatchStatTxt ? szMatchStatTxt : "" );
		}

		if ( szStatR && ( szStatR[0] == '#' ) )
		{
			wchar_t wszMatchTeamStatR[MAX_PATH] = {};
			wchar_t const *wszR = Helper_ResolveLocalizationStringWithFormattingParameters( wszMatchTeamStatR, sizeof( wszMatchTeamStatR ), szStatR );
			char szUTF8[4 * 512];
			V_UnicodeToUTF8( wszR, szUTF8, ARRAYSIZE( szUTF8 ) );
			m_pExtraMatchDataTextR->SetText( szUTF8 );
		}
		else
		{
			m_pExtraMatchDataTextR->SetText( szStatR ? szStatR : "" );
		}

		if ( szStatL && ( szStatL[0] == '#' ) )
		{
			wchar_t wszMatchTeamStatL[MAX_PATH] = {};
			wchar_t const *wszL = Helper_ResolveLocalizationStringWithFormattingParameters( wszMatchTeamStatL, sizeof( wszMatchTeamStatL ), szStatL );
			char szUTF8[4 * 512];
			V_UnicodeToUTF8( wszL, szUTF8, ARRAYSIZE( szUTF8 ) );
			m_pExtraMatchDataTextL->SetText( szUTF8 );
		}
		else
		{
			m_pExtraMatchDataTextL->SetText( szStatL ? szStatL : "" );
		}
*/
	}
	else if ( bShowPct )
	{
		m_pPickemPredictionsPanel->SetVisible( true );

		int nTValue = 100 - nPct;
		int nCTValue = nPct;

		static const panorama::CPanoramaSymbol k_symPickemProgressbarR( "PickemProgressbarR" );
		if ( m_bSwapPlayerNames )
		{
			if ( m_pPickemBar->BHasClass( k_symPickemProgressbarR ) == false )
				m_pPickemBar->AddClass( k_symPickemProgressbarR );

			nTValue = nPct;
			nCTValue = 100 - nPct;

			m_pPickemBar->SetValue( 100.0f - ( float )nPct );
		}
		else
		{
			if ( m_pPickemBar->BHasClass( k_symPickemProgressbarR ) == true )
				m_pPickemBar->RemoveClass( k_symPickemProgressbarR );

			m_pPickemBar->SetValue( ( float )nPct );
		}

		m_pPickemPercentL->SetText( CFmtStr( "%d%%", nCTValue ).Get() );
		m_pPickemPercentR->SetText( CFmtStr( "%d%%", nTValue ).Get() );
	}
	else if ( m_bDisplayMatchesWon )
	{
		m_pBestOfPipsPanel->SetVisible(true);
	}
}

void CCSGO_HudSpectator::SetNumberOfMatchesWonPips( int numBestOfPipsPerSide, int numPipsFilled, int nTeamID )
{
	//trace( "numBestOfPipsPerSide" + numBestOfPipsPerSide);
	if ( numBestOfPipsPerSide > 0 )
	{
		m_bDisplayMatchesWon = true;
		int numTotalPips = 3;

		int numBestOfSeries = ( numBestOfPipsPerSide * 2 ) - 1;

		m_pBestOfPipsText->SetDialogVariable("count", numBestOfSeries );
		m_pBestOfPipsText->SetDialogVariable("numtowin", numBestOfPipsPerSide );

		for ( int i = 0; i < numTotalPips; i++ )
		{
			panorama::CImagePanel *pPip = NULL;
			if ( m_bSwapPlayerNames )
				pPip = (nTeamID == TEAM_CT) ? m_pBestOfPipsIconsR[i] : m_pBestOfPipsIconsL[i];
			else
				pPip = (nTeamID == TEAM_CT) ? m_pBestOfPipsIconsL[i] : m_pBestOfPipsIconsR[i];

			if ( i >= numBestOfPipsPerSide )
			{
				pPip->SetVisible(false);
			}
			else
			{
				pPip->SetVisible( true );

				if ( i < numPipsFilled )
				{
					pPip->SetImage( "file://{images}/status_icons/icon_star.vtf" );
				}
				else
				{
					pPip->SetImage( "file://{images}/status_icons/icon_star_empty.vtf" );
				}
			}
		}
	}
	else
		m_bDisplayMatchesWon = false;

}

void CCSGO_HudSpectator::HandleExtraData( bool bLeftSide, const PlayerSpecStatus_t* ms, int nSlotIdx )
{
	int nHalfPlayers = MAX_TEAM_SIZE;
	int idx;
	// Offset to the next available team-based slot
	if ( bLeftSide )
	{
		idx = nSlotIdx;
	}
	else
	{
		idx = nSlotIdx + nHalfPlayers;
	}

	// get the player panel
	CSpecPlayerPanelData *pPlayerPanelData = &m_aPlayerPanels[idx];
	if ( !pPlayerPanelData->m_pPanel )
		return;

	float flHealth = bLeftSide ? ( float )ms->nHealth : 100.0f - ( float )ms->nHealth;
	if ( pPlayerPanelData->m_flCurHealth != flHealth )
	{
		// health changed from what we were at
		if ( pPlayerPanelData->HasHealthFlashTimeElapsed() )
		{
			pPlayerPanelData->m_flPrevHealth = pPlayerPanelData->m_flCurHealth;	
			pPlayerPanelData->m_pHealthBarRed->SetValue( pPlayerPanelData->m_flPrevHealth );
			pPlayerPanelData->StartHealthFlashTimer();
		}

		// set the real health to the new value
		pPlayerPanelData->m_flCurHealth = flHealth;
	}
	else if ( pPlayerPanelData->m_flPrevHealth != pPlayerPanelData->m_flCurHealth && pPlayerPanelData->HasHealthFlashTimeElapsed() )
	{
		float flFrac = pPlayerPanelData->GetHealthFlashLerpFrac();
		float flValue = Lerp(flFrac, pPlayerPanelData->m_flPrevHealth, flHealth );
		pPlayerPanelData->m_pHealthBarRed->SetValue( flValue );
		if ( flFrac >= 1 )
		{
			pPlayerPanelData->m_flPrevHealth = flHealth;
			pPlayerPanelData->m_flCurHealth = flHealth;
		}
	}

	// show the money and extra data for players during the freeze period
	if ( CSGameRules()->IsFreezePeriod() && CSGameRules()->CanSpendMoneyInMap() )
	{
		bool bRenderForSpectator = CanSeeSpectatorOnlyTools();// && spec_show_xray.GetInt();
		C_CSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( ms->nPlayerIdx ) );
		if ( bRenderForSpectator && pPlayer && pPlayerPanelData->m_pExtraDataPanel )
		{
			if ( pPlayerPanelData->m_bPlayerExtraDataVisible == false )
			{
				// show the extra data
				pPlayerPanelData->m_pExtraDataPanel->SetVisible( true );
				static const panorama::CPanoramaSymbol k_symExtraDataShowL( "PlayerPanel_ExtraDataShow_L" );
				static const panorama::CPanoramaSymbol k_symExtraDataShowR( "PlayerPanel_ExtraDataShow_R" );		

				if ( bLeftSide )
				{
					pPlayerPanelData->m_pExtraDataPanel->TriggerClass( k_symExtraDataShowL );
				}
				else
				{
					pPlayerPanelData->m_pExtraDataPanel->TriggerClass( k_symExtraDataShowR );
				}

				pPlayerPanelData->m_bPlayerExtraDataVisible = true;
			}

			C_CS_PlayerResource* pCSPR = ( C_CS_PlayerResource* )g_PR;
			int nCashSpent = pCSPR->GetCashSpentThisRound( ms->nPlayerIdx );
			// now fill out the data for this slot
			pPlayerPanelData->m_pExtraDataPanel_K->SetText( CFmtStr( "%d", pCSPR->GetKills( ms->nPlayerIdx ) ).Get() );
			pPlayerPanelData->m_pExtraDataPanel_A->SetText( CFmtStr( "%d", pCSPR->GetAssists( ms->nPlayerIdx ) ).Get() );
			pPlayerPanelData->m_pExtraDataPanel_D->SetText( CFmtStr( "%d", pCSPR->GetDeaths( ms->nPlayerIdx ) ).Get() );
			if ( nCashSpent == 0 )
				pPlayerPanelData->m_pExtraDataPanel_MoneySpent->SetText( "" );
			else
				pPlayerPanelData->m_pExtraDataPanel_MoneySpent->SetText( CFmtStr( "-$%d", pCSPR->GetCashSpentThisRound( ms->nPlayerIdx ) ).Get() );
		}
	}
	else if ( pPlayerPanelData->m_bPlayerExtraDataVisible == true )
	{
		// hide the extra data
		//if ( pPlayerPanelData->m_pExtraDataPanel && pPlayerPanelData->m_pExtraDataPanel->BIsVisible() )
		//	pPlayerPanelData->m_pExtraDataPanel->SetVisible( false );

		static const panorama::CPanoramaSymbol k_symExtraDataHideL( "PlayerPanel_ExtraDataHide_L" );
		static const panorama::CPanoramaSymbol k_symExtraDataHideR( "PlayerPanel_ExtraDataHide_R" );
		if ( bLeftSide )
			pPlayerPanelData->m_pExtraDataPanel->TriggerClass( k_symExtraDataHideL );
		else
			pPlayerPanelData->m_pExtraDataPanel->TriggerClass( k_symExtraDataHideR );

		pPlayerPanelData->m_bPlayerExtraDataVisible = false;
	}
}

void CCSGO_HudSpectator::InvokePlayerSlotUpdate( sAvatarInitData &avatarData, const PlayerSpecStatus_t* ms, int nPlayerPanel, int nSlotIdx )
{
	if ( !ms )
		return;

	IViewPortPanel* panel = GetViewPortInterface()->FindPanelByName( PANEL_SCOREBOARD );
	if ( panel && panel->IsVisible() )
		return;

	static const panorama::CPanoramaSymbol k_symHealthCT( "PlayerPanel__HealthBar--CT" );
	static const panorama::CPanoramaSymbol k_symHealthT( "PlayerPanel__HealthBar--T" );

	static char xuidAsText[256] = { 0 };
	g_PR->FillXuidText( ms->nPlayerIdx, xuidAsText, sizeof( xuidAsText ) );

	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	//g_PR->FillXuidText( pLocalPlayer ? pLocalPlayer->entindex() : ms->nPlayerIdx, xuidAsText, sizeof( xuidAsText ) );
	
	bool bShowOtherTeam = g_bEngineIsHLTV;
	if ( pLocalPlayer && !bShowOtherTeam )
	{
		if ( pLocalPlayer->IsSpectator() )
			bShowOtherTeam = true;

 		if ( pLocalPlayer->GetAssociatedTeamNumber() == g_PR->GetTeam( ms->nPlayerIdx ) )
 			bShowOtherTeam = true;
	}

	bool bLeftSide = ms->bIsCT;
	if ( m_bSwapPlayerNames )
		bLeftSide = !bLeftSide;

	//CAvatarPanelData *pAvatarPanelData;
	CSpecPlayerPanelData *pPlayerPanelData;

	int nHalfPlayers = MAX_TEAM_SIZE;
	int idx;
	// Offset to the next available team-based slot
	if ( bLeftSide )
	{
		idx = nSlotIdx;
	}
	else
	{
		idx = nSlotIdx + nHalfPlayers;
	}

	if ( idx < 0 || idx >= m_aPlayerPanels.Count() )
		return;

	// get the player panel
	pPlayerPanelData = &m_aPlayerPanels[ idx ];
	if ( !pPlayerPanelData->m_pPanel )
		return;

	// make sure we're visible
	pPlayerPanelData->m_pPanel->SetVisible(true);
	pPlayerPanelData->m_pPlayerPanelBG->SetVisible(true);

	pPlayerPanelData->m_nPlayerIdx = ms->nPlayerIdx;

	// Pack all the flags as a bitfield
	int Flags = 0;
	Flags |= ( (0x1 & bLeftSide)					<< 0 );
	Flags |= ( (0x1 & ms->bIsCT)					<< 1 );
	Flags |= ( (0x1 & ms->bHasDefBomb)				<< 2 );
	Flags |= ( (0x1 & ms->bDead)					<< 3 );
	Flags |= ( (0x1 & ms->bHasHelmet)				<< 4 );
	Flags |= ( (0x1 & ms->bSpeaking)				<< 5 );
	Flags |= ( (0x1 & ms->bPlayerBot)				<< 6 );
	Flags |= ( (0x1 & ms->bSpectated)				<< 7 );
	Flags |= ( (0x1 & ms->bForceAvatarRefresh)		<< 8 );
	Flags |= ( (0x1 & bShowOtherTeam)				<< 9 );

	avatarData.m_bShowLetter = false;
	avatarData.m_nArmor = ms->nArmor;
	avatarData.m_nHealth = ms->nHealth;
	avatarData.m_nLevel = 0;
	avatarData.m_nSlot = idx;
	avatarData.m_XUID = ms->nXUID;
	avatarData.m_nFlags = Flags;

	C_CS_PlayerResource* pCSPR = ( C_CS_PlayerResource* )g_PR;
	if ( pCSPR )
		avatarData.m_nTeammateColor = pCSPR->GetCompTeammateColor( ms->nPlayerIdx );

	UpdateAvatar( avatarData, ms, pPlayerPanelData->m_pAvatar );

	// if we're a player and we're dead, hide the other side
	if ( bShowOtherTeam == false )
	{
		if ( ( pLocalPlayer->GetAssociatedTeamNumber() == TEAM_CT && !ms->bIsCT ) || ( pLocalPlayer->GetAssociatedTeamNumber() == TEAM_TERRORIST && ms->bIsCT ) )
		{
			pPlayerPanelData->m_pPlayerPanelParent->SetVisible( false );
			pPlayerPanelData->m_pHighlight->SetVisible( false );
			return;
		}
	}

	pPlayerPanelData->m_pPlayerPanelParent->SetVisible( true );

	if ( ms->bSpectated )
		pPlayerPanelData->m_pHighlight->SetVisible(true);
	else
		pPlayerPanelData->m_pHighlight->SetVisible(false);

	static const panorama::CPanoramaSymbol k_symPanelLeftCT( "PlayerPanel__HealthBar_L_CT" );
	static const panorama::CPanoramaSymbol k_symPanelLeftT( "PlayerPanel__HealthBar_L_T" );
	static const panorama::CPanoramaSymbol k_symPanelRightCT( "PlayerPanel__HealthBar_R_CT" );
	static const panorama::CPanoramaSymbol k_symPanelRightT( "PlayerPanel__HealthBar_R_T" );

	if ( pPlayerPanelData->m_pHealthBar->BHasClass( k_symPanelLeftCT ) )
		pPlayerPanelData->m_pHealthBar->RemoveClass( k_symPanelLeftCT );
	if ( pPlayerPanelData->m_pHealthBar->BHasClass( k_symPanelLeftT ) )
		pPlayerPanelData->m_pHealthBar->RemoveClass( k_symPanelLeftT );
	if ( pPlayerPanelData->m_pHealthBar->BHasClass( k_symPanelRightCT ) )
		pPlayerPanelData->m_pHealthBar->RemoveClass( k_symPanelRightCT );
	if ( pPlayerPanelData->m_pHealthBar->BHasClass( k_symPanelRightT ) )
		pPlayerPanelData->m_pHealthBar->RemoveClass( k_symPanelRightT );

	if ( bLeftSide )
	{
		if ( ms->bIsCT)
			pPlayerPanelData->m_pHealthBar->AddClass( k_symPanelLeftCT );
		else
			pPlayerPanelData->m_pHealthBar->AddClass( k_symPanelLeftT );
	}
	else
	{
		if ( ms->bIsCT )
			pPlayerPanelData->m_pHealthBar->AddClass( k_symPanelRightCT );
		else
			pPlayerPanelData->m_pHealthBar->AddClass( k_symPanelRightT );
	}

//	CSWeaponID nWeaponID = ms->nWeaponID; //**
//	const CCSWeaponInfo* pWeaponInfo = GetWeaponInfo( nWeaponID ); //**
	
	bool bIsPrimarySelected = ms->bIsPrimarySelected;
	
	CUtlVector<char*> WeaponList;
	V_SplitString( ms->szFinalWeaponString, ",", WeaponList );
	// display the sorted list
	for ( int i = 0; i < MAX_PLAYERPANEL_WEAPONS; i++ )
	{
		if ( i >= WeaponList.Count() )
		{
			pPlayerPanelData->m_pWeaponIcons[i]->SetVisible( false );
			continue;
		}

		if ( pPlayerPanelData->m_pPanel && i < MAX_PLAYERPANEL_WEAPONS )
		{
			pPlayerPanelData->m_pWeaponIcons[i]->SetVisible( true );
			pPlayerPanelData->m_pWeaponIcons[i]->SetImageJS( CFmtStr( "file://{images}/icons/equipment/%s.svg", WeaponList[i] ) );

			panorama::IUIPanelStyle *pPanelStyle;
			pPanelStyle = pPlayerPanelData->m_pWeaponIcons[i]->AccessStyle();
			if ( ms->nSecondarySelectedSlot == i )
				pPanelStyle->SetSimpleWashColor( Color( 255, 255, 255, 255 ), true );
			else
				pPanelStyle->SetSimpleWashColor( Color( 180, 180, 180, 255 ), true );		
		}
	}

	static const panorama::CPanoramaSymbol k_symAvatarBGHidden( "PlayerPanelSnippet__BG_Hide" );
	static const panorama::CPanoramaSymbol k_symDeadBGAnimR( "PlayerPanel_DeadBG_R" );
	static const panorama::CPanoramaSymbol k_symDeadBGAnimL( "PlayerPanel_DeadBG_L" );
	if ( ms->bDead )
	{
		pPlayerPanelData->m_pWeaponMain->SetVisible(false);
		pPlayerPanelData->m_pHealthBar->SetVisible( false );
		pPlayerPanelData->m_pHealthBarRed->SetVisible( false );
		pPlayerPanelData->m_pHealthText->SetVisible( false );
		pPlayerPanelData->m_pArmorPanel->SetVisible( false );	
		pPlayerPanelData->m_pDeadBG->SetVisible( true );	
		pPlayerPanelData->m_pPlayerPanelBG->SetHasClass( k_symAvatarBGHidden, true );

		if ( pPlayerPanelData->m_bWasDead  == false )
		{
			if ( bLeftSide )
				pPlayerPanelData->m_pDeadBG->TriggerClass( k_symDeadBGAnimL );
			else
				pPlayerPanelData->m_pDeadBG->TriggerClass( k_symDeadBGAnimR );
		}
			
		pPlayerPanelData->m_bWasDead = true;

	}
	else
	{
		pPlayerPanelData->m_bWasDead = false;

		pPlayerPanelData->m_pDeadBG->SetVisible( false );	
		pPlayerPanelData->m_pPlayerPanelBG->SetHasClass( k_symAvatarBGHidden, false );

		pPlayerPanelData->m_pWeaponMain->SetVisible( true );
		pPlayerPanelData->m_pHealthBar->SetVisible( true );
		pPlayerPanelData->m_pHealthBarRed->SetVisible( true );
		pPlayerPanelData->m_pHealthText->SetVisible( true );
		pPlayerPanelData->m_pArmorPanel->SetVisible( true );

		// set the main weapon
		const char *szWeaponName = ms->szPrimaryWeaponName;

		// strip the weapon_* from the weapon name
		if ( IsWeaponClassname( szWeaponName ) )
		{
			szWeaponName += WEAPON_CLASSNAME_PREFIX_LENGTH;
		}
		if ( StringHasPrefixCaseSensitive( szWeaponName, "knifegg" ) )
		{
			szWeaponName = "knife";
		}

		pPlayerPanelData->m_pWeaponMain->SetImageJS( CFmtStr( "file://{images}/icons/equipment/%s.svg", szWeaponName ) );
		pPlayerPanelData->m_pWeaponMain->SetScaling( panorama::k_EImageScalingNone );
		
		panorama::IUIPanelStyle *pPanelStyle;
		pPanelStyle = pPlayerPanelData->m_pWeaponMain->AccessStyle();			
		if ( bIsPrimarySelected )
			pPanelStyle->SetSimpleWashColor( Color( 255, 255, 255, 255 ), true );
		else
			pPanelStyle->SetSimpleWashColor( Color( 180, 180, 180, 255 ), true );

		// health
		float flHealth = bLeftSide ? ( float )ms->nHealth : 100.0f - ( float )ms->nHealth;

		// set the real health to the new value
		pPlayerPanelData->m_pHealthBar->SetValue( flHealth );
		pPlayerPanelData->m_pHealthText->SetText( CNumStr( ms->nHealth ).String() );
	}

	// don't show the BOT for players, only the bot they took over
	EDecoratedPlayerNameFlag_t flags = k_EDecoratedPlayerNameFlag_Simple;
	if ( ms->bPlayerBot )
		flags |= k_EDecoratedPlayerNameFlag_AddBotToNameIfControllingBot;

	wchar_t wszPlayerName[MAX_PLAYER_NAME_LENGTH];
	pCSPR->GetDecoratedPlayerName( ms->nPlayerIdx, wszPlayerName, sizeof( wszPlayerName ), flags );

	// $$$REI Move to dialog variable instead of SetText() here to avoid loc tag translation
	char szPlayerNameUTF8[4 * MAX_DECORATED_PLAYER_NAME_LENGTH];
	V_UnicodeToUTF8( wszPlayerName, szPlayerNameUTF8, ARRAYSIZE( szPlayerNameUTF8 ) );
	pPlayerPanelData->m_pPlayerName->SetText( szPlayerNameUTF8 );

	// hide armor if we are dead or have no armor
	if ( ms->bDead || ms->nArmor <= 0 )
	{
		pPlayerPanelData->m_pArmorIcon->SetVisible( false );
	}
	else
	{
		pPlayerPanelData->m_pArmorIcon->SetVisible( true );
		if ( ms->bHasHelmet )
			pPlayerPanelData->m_pArmorIcon->SetImage( "file://{images}/icons/ui/armor.svg" );
		else
			pPlayerPanelData->m_pArmorIcon->SetImage( "file://{images}/icons/ui/shield.svg" );

		// todo this should only happen once
		panorama::IUIPanelStyle *pPanelStyle;
		pPanelStyle = pPlayerPanelData->m_pArmorPanel->AccessStyle();
		pPanelStyle->SetSimpleWashColor( Color( 220, 220, 220, 200 ), true );
		pPanelStyle->SetWashColor( "white" );
	}

	if ( ms->bHasDefBomb )
	{
		pPlayerPanelData->m_pC4DefuserIcon->SetVisible(true);
		panorama::IUIPanelStyle *pPanelStyle;
		pPanelStyle = pPlayerPanelData->m_pC4DefuserIcon->AccessStyle();
		if ( ms->bIsCT )
		{
			pPanelStyle->SetSimpleWashColor( Color( 160, 200, 230, 255 ), true );
			pPlayerPanelData->m_pC4DefuserIcon->SetImage( "file://{images}/icons/equipment/defuser.svg" );
		}
		else
		{
			pPanelStyle->SetSimpleWashColor( Color( 255, 180, 40, 255 ), true );
			pPlayerPanelData->m_pC4DefuserIcon->SetImage( "file://{images}/icons/equipment/c4.svg" );
		}
	}
	else
		pPlayerPanelData->m_pC4DefuserIcon->SetVisible(false);

	// money is always visible
	pPlayerPanelData->m_pMoneyText->SetVisible( true );
	pPlayerPanelData->m_pMoneyText->SetText( CFmtStr( "$%d", ms->nMoney ).Get() );

	// kills
	if ( ms->nRoundKills <= 0 )
	{
		pPlayerPanelData->m_pKillPanel->SetVisible(false);
		pPlayerPanelData->m_pKillText->SetVisible(false);
	}
	else
	{
		pPlayerPanelData->m_pKillPanel->SetVisible( true );
		pPlayerPanelData->m_pKillText->SetVisible( false );

		if ( ms->nRoundKills > 1 )
		{
			pPlayerPanelData->m_pKillText->SetVisible( true );

			if ( bLeftSide )
				pPlayerPanelData->m_pKillText->SetText( CFmtStr( "%dx", ms->nRoundKills ).Get() );
			else
				pPlayerPanelData->m_pKillText->SetText( CFmtStr( "x%d", ms->nRoundKills ).Get() );
		}
	}
}

void CCSGO_HudSpectator::UpdateAvatar( sAvatarInitData &avatarData, const PlayerSpecStatus_t* ms, CAvatarPanelData *pAvatarPanelData )
{
	static const panorama::CPanoramaSymbol k_symAvatarSkullCT( "Avatar__Skull--CT" );
	static const panorama::CPanoramaSymbol k_symAvatarSkullT( "Avatar__Skull--T" );
	static const panorama::CPanoramaSymbol k_symAvatarBGHidden( "Avatar__BG--Hidden" );

	if ( !pAvatarPanelData->m_pPanel || !pAvatarPanelData->m_bInitialized )
		return;

		// update the avatar info
	//bool bWasAlreadyDead = pAvatarPanelData->m_bDead;

	pAvatarPanelData->m_pPanel->SetVisible( true );

	if ( m_bForceAvatarRefresh || pAvatarPanelData->m_savedXUID != ms->nXUID )
	{
		pAvatarPanelData->m_savedXUID = 0;
	}

	// background (just background for now - but to be removed entirely soon
	pAvatarPanelData->SetBGForTeam( ms->bIsCT, false );

	pAvatarPanelData->m_pAvatarBG->SetVisible( !ms->bDead );

	// player color
	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	bool bShowColor = ( pLocalPlayer && pLocalPlayer->ShouldShowTeamPlayerColors( ms->bIsCT ? TEAM_CT : TEAM_TERRORIST ) );
	CCSGO_HudTeamCounter *pTeamCounter = GET_HUDELEMENT( CCSGO_HudTeamCounter );
	C_CS_PlayerResource* pCSPR = ( C_CS_PlayerResource* )g_PR;
	if ( bShowColor && pTeamCounter && pCSPR )
	{
		Color c = pCSPR->GetCompPlayerColorByID( avatarData.m_nTeammateColor );
		panorama::IUIPanelStyle *pPanelStyle = pAvatarPanelData->m_pPlayerColor->AccessStyle();
		pPanelStyle->SetSimpleWashColor( c );

		// TODO: this doesn't seem correct
		if ( avatarData.m_bShowLetter )
		{
			pAvatarPanelData->m_pPlayerLetter->SetText( pTeamCounter->GetPlayerColorLetter( 0, avatarData.m_nTeammateColor ), panorama::CLabel::k_ETextTypeHTML );
		}
	}
	pAvatarPanelData->m_pPlayerColor->SetVisible(bShowColor);
	pAvatarPanelData->m_pPlayerLetter->SetVisible(avatarData.m_bShowLetter);
	
	static const panorama::CPanoramaSymbol k_symAvatarOutlineBlack( "Avatar__PlayerOutline--None" );
	/*
	static const panorama::CPanoramaSymbol k_symAvatarOutlineSpectate( "Avatar__PlayerOutline--Spectate" );

	if ( ms->bSpectated )	
		pAvatarPanelData->SetBorderType( k_symAvatarOutlineSpectate );
	else
		pAvatarPanelData->SetBorderType( k_symAvatarOutlineBlack );
	*/

	pAvatarPanelData->SetOutlineVisible( true );

	// set default background image
	if ( pAvatarPanelData->m_pAvatarBG )
	{
		static const panorama::CPanoramaSymbol k_symAvatarDefaultBGCT( "AvatarSpec__Default--CT" );
		static const panorama::CPanoramaSymbol k_symAvatarDefaultBGT( "AvatarSpec__Default--T" );

		if ( ms->bIsCT )
		{
			if ( pAvatarPanelData->m_pAvatarBG->BHasClass( k_symAvatarDefaultBGT ) )
				pAvatarPanelData->m_pAvatarBG->RemoveClass( k_symAvatarDefaultBGT );

			if ( !pAvatarPanelData->m_pAvatarBG->BHasClass(k_symAvatarDefaultBGCT) )
				pAvatarPanelData->m_pAvatarBG->SetHasClass( k_symAvatarDefaultBGCT, true );
		}
		else
		{
			if ( pAvatarPanelData->m_pAvatarBG->BHasClass( k_symAvatarDefaultBGCT ) )
				pAvatarPanelData->m_pAvatarBG->RemoveClass( k_symAvatarDefaultBGCT );

			if ( !pAvatarPanelData->m_pAvatarBG->BHasClass(k_symAvatarDefaultBGT) )
				pAvatarPanelData->m_pAvatarBG->SetHasClass( k_symAvatarDefaultBGT, true );
		}
	}

	pAvatarPanelData->m_pHealth->SetVisible( false );

	// make the hotkey label show up
	bool bShowHotkey = g_bEngineIsHLTV;
	if ( pLocalPlayer && !bShowHotkey )
	{
		if ( pLocalPlayer->IsSpectator() || (  pLocalPlayer->GetAssociatedTeamNumber() == g_PR->GetTeam( ms->nPlayerIdx )) )
			bShowHotkey = true;

		if ( bShowHotkey == false && ( pLocalPlayer->GetTeamNumber() == TEAM_CT && ms->bIsCT ) || ( pLocalPlayer->GetTeamNumber() == TEAM_TERRORIST && !ms->bIsCT ) )
			bShowHotkey = true;
	}

	pAvatarPanelData->m_pPlayerNumber->SetVisible( !ms->bDead && bShowHotkey );
	pAvatarPanelData->m_pPlayerNumber->SetText( CNumStr( (avatarData.m_nSlot + 1) % 10 ).String() );

	if ( !ms->bDead )
		pAvatarPanelData->m_pSkull->SetVisible( false );
	
	if ( ms->bDead && !ms->bPlayerBot )
	{
		// Skulls: display based on team and whether they are local player's teammate
		pAvatarPanelData->SetSkullType( ms->bIsCT ? k_symAvatarSkullCT : k_symAvatarSkullT );
		pAvatarPanelData->m_pSkull->SetVisible( ms->bDead && !ms->bPlayerBot );
	}

	// Indicates a player playing as a bot (bot-takeover)
	pAvatarPanelData->m_pBot->SetVisible( ms->bPlayerBot );

	// avatar image
	if ( avatarData.m_XUID != 0 )
	{
		// If this is a new XUID, we need to load the new avatar image now
		if ( pAvatarPanelData->m_savedXUID != avatarData.m_XUID )
		{
			pAvatarPanelData->m_pDynamicAvatar->SetSteamID( avatarData.m_XUID );

			// disable default background image
			pAvatarPanelData->m_pAvatarBG->SetHasClass( k_symAvatarBGHidden, true );
		}

		pAvatarPanelData->m_pDynamicAvatar->SetVisible( true );
	}
	else
	{
		pAvatarPanelData->m_pDynamicAvatar->SetVisible( false );
		pAvatarPanelData->m_pAvatarBG->SetHasClass( k_symAvatarBGHidden, false );
	}

	// Remember what XUID we used on this slot
	pAvatarPanelData->m_savedXUID = avatarData.m_XUID;

	pAvatarPanelData->m_bDead = ms->bDead;
	pAvatarPanelData->m_bWasPlayerBot = false;
}

void CCSGO_HudSpectator::UpdateTimer( void )
{
	//m_pTimer

	C_CSGameRules *pRules = CSGameRules();
	if ( !pRules || !m_pTimer )
		return;

	if ( pRules->m_iRoundWinStatus == WINNER_NONE )
		m_nCurrentRoundNumber = CSGameRules()->GetTotalRoundsPlayed() + 1;

	// timer is hidden when bomb planted, so break out of update
	if ( g_PlantedC4s.Count() > 0 )
	{
		m_pTimer->SetVisible(false);

		if ( m_bIsBombDefused )
		{
			SetBombDefused();
		}
		else
		{
			UpdatePlantedBombState( g_PlantedC4s[0]->GetDetonationProgress() * 100.0f );
		}

		return;
	}

	m_pTimer->SetVisible( true );
	m_pBombPlanted->SetVisible( false );
	m_pBombPlantedLines->SetVisible( false );
	m_pBombDefused->SetVisible( false );

	int nTimer = static_cast< int >( ceil( pRules->GetRoundRemainingTime() ) );

	bool bFreezePeriod = pRules->IsFreezePeriod();
	if ( bFreezePeriod )
	{
		// countdown to the start of the round while we're in freeze period
		nTimer = static_cast< int >( ceil( pRules->GetRoundStartTime() - gpGlobals->curtime ) );
	}

	const int kTimeRemainingToDisplayRed = 11;
	static CPanoramaSymbol k_symTimerRed( "ScoreboardTimer_Red" );

	Assert( m_pTimer->BHasClass( k_symTimerRed ) == m_bTimerAlertTriggered );
	if ( !m_bTimerAlertTriggered && ( nTimer < kTimeRemainingToDisplayRed ) )
	{
		// when time is low switch to red text
		m_bTimerAlertTriggered = true;
		m_pTimer->AddClass( k_symTimerRed );
	}
	else if ( m_bTimerAlertTriggered && ( nTimer >= kTimeRemainingToDisplayRed ) )
	{
		// revert to normal timer color
		m_bTimerAlertTriggered = false;
		m_pTimer->RemoveClass( k_symTimerRed );
	}

	if ( nTimer < 0 )
		nTimer = 0;

	int nMinutes = nTimer / 60;
	int nSeconds = nTimer % 60;

	wchar_t szTime[32];
	szTime[0] = 0;

	//V_snwprintf( szTime, ARRAYSIZE( szTime ), L"%d:%.2d", nMinutes, nSeconds );
	if ( CSGameRules() && CSGameRules()->IsWarmupPeriod() )
		m_pTimer->SetText( "" );
	else
		m_pTimer->SetText( CFmtStr( "%d:%.2d", nMinutes, nSeconds ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudSpectator::SetBombDefused()
{
	m_pTimer->SetVisible( false );

	m_pBombPlanted->SetVisible( false );
	m_pBombPlantedLines->SetVisible( false );
	m_pBombDefused->SetVisible( true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudSpectator::UpdatePlantedBombState( float flDetProgress )
{
	static panorama::CPanoramaSymbol k_symBombPulseAnimSlow( "BombPlantedPulse__Slow" );
	static panorama::CPanoramaSymbol k_symBombPulseAnimMedium( "BombPlantedPulse__Medium" );
	static panorama::CPanoramaSymbol k_symBombPulseAnimFast( "BombPlantedPulse__Fast" );

	static panorama::CPanoramaSymbol k_symBombLinesAnimSlow( "BombPlantedLines__Slow" );
	static panorama::CPanoramaSymbol k_symBombLinesAnimMedium( "BombPlantedLines__Medium" );
	static panorama::CPanoramaSymbol k_symBombLinesAnimFast( "BombPlantedLines__Fast" );

	m_pTimer->SetVisible( false );

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


void CCSGO_HudSpectator::UpdateRounds( void )
{
	if ( !CSGameRules() )
		return;

	wchar_t wszMessage[128] = { 0 };
	if ( CSGameRules()->GetOvertimePlaying() && ( m_nCurrentRoundNumber > mp_maxrounds.GetInt() ) && mp_overtime_maxrounds.GetInt() )
	{
		int numOvertimeRounds = mp_overtime_maxrounds.GetInt();
		int numRoundsInThisOvertime = ( m_nCurrentRoundNumber - mp_maxrounds.GetInt() ) % numOvertimeRounds;
		int nOvertimeCounter = CSGameRules()->GetOvertimePlaying() - ( numRoundsInThisOvertime ? 0 : 1 );
		if ( !numRoundsInThisOvertime )
			numRoundsInThisOvertime = numOvertimeRounds;

		wchar_t wszOvertime[32], wszRounds[32];
		V_swprintf_safe( wszOvertime, L"%d", nOvertimeCounter );
		V_swprintf_safe( wszRounds, L"%d/%d", numRoundsInThisOvertime, numOvertimeRounds );
		g_pVGuiLocalize->ConstructString( wszMessage, sizeof( wszMessage ), g_pVGuiLocalize->Find( "#SFUIHUD_Spectate_RoundsLeftOvertime" ), 2, wszOvertime, wszRounds );
	}
	else
	{
		wchar_t wszRounds[32] = {};
		// infinity symbol doesn't look good
		//wchar_t wszMaxRounds[ 32 ] = { 0x221E, 0 };
		wchar_t wszMaxRounds[32] = { L"--" };
		if ( mp_maxrounds.GetInt() <= 999 )
			V_snwprintf( wszMaxRounds, ARRAYSIZE( wszMaxRounds ), L"%d", mp_maxrounds.GetInt() );

		V_swprintf_safe( wszRounds, L"%d/%s", m_nCurrentRoundNumber, wszMaxRounds );
		g_pVGuiLocalize->ConstructString( wszMessage, sizeof( wszMessage ), g_pVGuiLocalize->Find( "#SFUIHUD_Spectate_RoundsLeft" ), 1, wszRounds );
	}

	if ( m_pRoundText )
	{
		m_pRoundText->SetVisible( !CSGameRules()->IsWarmupPeriod() );

		char szRoundMsgUTF8[4 * 128];
		V_UnicodeToUTF8( wszMessage, szRoundMsgUTF8, ARRAYSIZE( szRoundMsgUTF8 ) );
		m_pRoundText->SetText( szRoundMsgUTF8 );
	}
}

void CCSGO_HudSpectator::UpdateEventDropsAndViewers( void )
{	
	bool bDoFakeTOurneyAwards = false;

	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( pLocalPlayer )
	{
		if ( CSGameRules()->IsFreezePeriod() )
		{
			char const *szTournamentEventName = CSGameRules() ? CSGameRules()->GetTournamentEventName() : NULL;
			bool bQ = ( szTournamentEventName && *szTournamentEventName );

			if ( CDemoPlaybackParameters_t const *pPlayback = engine->GetDemoPlaybackParameters() )
			{
				if ( pPlayback->m_bAnonymousPlayerIdentity )
					m_HLTVSpectators = 0; // don't show spectators count at freezetime in overwatch
			}

			if ( ( m_HLTVSpectators > 0 ) || bQ || bDoFakeTOurneyAwards )
			{
				// don't update every frame
				if ( (m_flLastTourneyItemPanelUpdate + 1.0f) <= gpGlobals->curtime )
				{
					m_flLastTourneyItemPanelUpdate = gpGlobals->curtime;

					// send the list of player ids - we'll show these names on screen
					/*
					if ( m_TournamentRewardTempList.Count() > 0 )
					{
						for ( int i = 0; i < m_TournamentRewardTempList.Count(); i++ )
						{
							CSteamID steamID( m_TournamentRewardTempList[i].uiAccountID, steamapicontext->SteamUser()->GetSteamID().GetEUniverse(), k_EAccountTypeIndividual );

							// only send the first 5
							if ( i < MAX_PLAYER_ITEM_DROP_DISPLAY )
							{

							}
						}
					}
					*/

					if ( bQ )
					{
						CUtlVector< char* > urlFragments;
						V_SplitString( szTournamentEventName, "_", urlFragments );
						int nEventTag = ( int )atoi( urlFragments[urlFragments.Count() - 1] );
						m_pEventLogoImage->SetImage( CFmtStr( "file://{images}/../../../resource/flash/econ/tournaments/tournament_logo_%d.png", nEventTag ) );
					}
					else
					{
						m_pEventLogoImage->SetImage("");
					}

					if ( m_HLTVSpectators > 0 )
					{
						m_pMatchViewerPanel->SetVisible(true);
						wchar_t wszMessage[128] = { 0 };

						const char *pchNumberWithCommas = CFmtStr( "%s", V_pretifynum( m_HLTVSpectators ) ).Get();

						wchar_t wszCount[128];
						V_swprintf_safe( wszCount, L"" PRI_S_FOR_WS, pchNumberWithCommas );
						g_pVGuiLocalize->ConstructString( wszMessage, sizeof( wszMessage ), g_pVGuiLocalize->Find( "#SFUI_Scoreboard_Viewers" ), 1, wszCount );
						char szUTF8[4 * 512];
						V_UnicodeToUTF8( wszMessage, szUTF8, ARRAYSIZE( szUTF8 ) );

						m_pMatchViewerText->SetText( szUTF8 );
					}
					else
					{
						m_pMatchViewerPanel->SetVisible(false);
					}
	
					if ( m_TournamentRewardTempList.Count() > 0 )
					{
						m_pItemDropRoot->SetVisible(true);
						uint64 itemId = CombinedItemIdMakeFromDefIndexAndPaint( m_TournamentRewardTempList[0].nDefindex, 0 );

						const CEconItemView * pEconItemView = CEconItemView::FindOrCreateEconItemViewForItemID( itemId );
						if ( pEconItemView )
						{
//							m_pItemDropItemImage->SetItemID( itemId );

							char szNameBuff[256];
							pEconItemView->GetItemDisplayNameUtf8( szNameBuff, sizeof( szNameBuff ) );
							m_pItemDropItemName->SetText( szNameBuff );
						}

						if ( m_nTourneyItemsDroppedThisRound > 0 )
						{
							wchar_t wszMessage[128] = { 0 };
							wchar_t wszDropsMatchCount[128];
							const char *pchNumberWithCommas = CFmtStr( "%s", V_pretifynum( m_nTourneyItemsDroppedThisRound ) ).Get();
							V_swprintf_safe( wszDropsMatchCount, L"" PRI_S_FOR_WS, pchNumberWithCommas );
							g_pVGuiLocalize->ConstructString( wszMessage, sizeof( wszMessage ), g_pVGuiLocalize->Find( "#SFUIHUD_Spec_Event_DroppingItemsRewarded" ), 1, wszDropsMatchCount );
							char szUTF8[4 * 512];
							V_UnicodeToUTF8( wszMessage, szUTF8, ARRAYSIZE( szUTF8 ) );
							m_pItemDropTotalRound->SetText( szUTF8 );
						}
						else
						{
							m_pItemDropTotalRound->SetText( "" );
						}

						wchar_t wszMessage[128] = { 0 };
						wchar_t wszDropsMatchCount[128];
						const char *pchNumberWithCommas = CFmtStr( "%s", V_pretifynum( m_nTourneyItemsDroppedTotal ) ).Get();
						V_swprintf_safe( wszDropsMatchCount, L"" PRI_S_FOR_WS, pchNumberWithCommas );
						g_pVGuiLocalize->ConstructString( wszMessage, sizeof( wszMessage ), g_pVGuiLocalize->Find( "#SFUIHUD_Spec_Event_DroppingItemsTotalDropped" ), 1, wszDropsMatchCount );
						char szUTF8[4 * 512];
						V_UnicodeToUTF8( wszMessage, szUTF8, ARRAYSIZE( szUTF8 ) );
						m_pItemDropTotalMatch->SetText( szUTF8 );
					}
					else
					{
						m_pItemDropRoot->SetVisible( false );
					}
				}

				if ( CSGameRules()->IsFreezePeriod() )
				{
					if ( m_bIsFreezeTimeActive == false )
					{
						static const panorama::CPanoramaSymbol k_symEventDropsViewersRootFadeIn( "EventDropsViewersRoot_FadeIn" );
						m_pEventDropsViewersRoot->SetVisible( true );
						m_pEventDropsViewersRoot->TriggerClass( k_symEventDropsViewersRootFadeIn );
					}

					m_bIsFreezeTimeActive = true;
				}
			}
		}
		else if ( m_bIsFreezeTimeActive == true )
		{
			m_bIsFreezeTimeActive = false;
			m_nTourneyItemsDroppedThisRound = 0;
			// remove our temp list
			m_TournamentRewardTempList.RemoveAll();
			m_flLastTourneyItemPanelUpdate = 0;

			if ( m_pEventDropsViewersRoot->BIsVisible() )
			{
				static const panorama::CPanoramaSymbol k_symEventDropsViewersRootFadeOut( "EventDropsViewersRoot_FadeOut" );
				m_pEventDropsViewersRoot->TriggerClass( k_symEventDropsViewersRootFadeOut );
			}
		}	
	}
}

void CCSGO_HudSpectator::DisplayEventServerImage( void )
{
	/*
	m_pEventLogoRight->SetVisible( false );

	// server_graphic 1 is for spectators only
	//extern bool Helper_GraphicEnabled();
	if ( cl_server_graphic2_enable.GetBool() || Helper_GraphicEnabled() )
	{
		if( m_pEventLogoRight->IsSet() )
		{
			m_pEventLogoRight->SetVisible( true );
		}
	}
	*/
}

void CCSGO_HudSpectator::UpdateScoreAndTeamNames( void )
{
	if ( !g_PR || !CSGameRules() )
		return;

	if ( !CSGameRules()->IsWarmupPeriod() && CSGameRules()->GetRoundRemainingTime() <= 0 && CSGameRules()->m_bBombPlanted == false )
		return;

	int nCTScore = 0;
	int nTScore = 0;

	C_Team *CT_team = GetGlobalTeam( TEAM_CT );
	if ( CT_team )
		nCTScore = CT_team->Get_Score();

	C_Team *T_team = GetGlobalTeam( TEAM_TERRORIST );
	if ( T_team )
		nTScore = T_team->Get_Score();

	static const panorama::CPanoramaSymbol k_symSpecColorCT( "SpecColor_CT" );
	static const panorama::CPanoramaSymbol k_symSpecColorT( "SpecColor_T" );

	if ( m_bSwapPlayerNames )
	{
		m_pScore_L->SetText( CFmtStr( "%d", nTScore ) );
		m_pScore_R->SetText( CFmtStr( "%d", nCTScore ) );

		if ( m_pScore_L->BHasClass( k_symSpecColorT ) == false )
			m_pScore_L->AddClass(k_symSpecColorT);

		if ( m_pScore_R->BHasClass( k_symSpecColorCT ) == false )
			m_pScore_R->AddClass( k_symSpecColorCT );

		if ( m_pTeamName_L->BHasClass( k_symSpecColorT ) == false )
			m_pTeamName_L->AddClass( k_symSpecColorT );

		if ( m_pTeamName_R->BHasClass( k_symSpecColorCT ) == false )
			m_pTeamName_R->AddClass( k_symSpecColorCT );
		
	}
	else
	{
		m_pScore_R->SetText( CFmtStr( "%d", nTScore ) );
		m_pScore_L->SetText( CFmtStr( "%d", nCTScore ) );

		if ( m_pScore_L->BHasClass( k_symSpecColorT ) == true )
			m_pScore_L->RemoveClass( k_symSpecColorT );

		if ( m_pScore_R->BHasClass( k_symSpecColorCT ) == true )
			m_pScore_R->RemoveClass( k_symSpecColorCT );

		if ( m_pTeamName_L->BHasClass( k_symSpecColorT ) == true )
			m_pTeamName_L->RemoveClass( k_symSpecColorT );

		if ( m_pTeamName_R->BHasClass( k_symSpecColorCT ) == true )
			m_pTeamName_R->RemoveClass( k_symSpecColorCT );
	}

	// set the names
	for ( int team = TEAM_TERRORIST; team <= TEAM_CT; team++ )
	{
		int iTeamIndex = team - TEAM_TERRORIST;

		// get the team
		C_Team *pTeam = GetGlobalTeam( team );

		// For the Best-of-N UI determine the number of pips to show per side and how many filled
		int numBestOfPipsPerSide = ( CSGameRules()->m_numBestOfMaps );
		if ( numBestOfPipsPerSide <= 1 )
			numBestOfPipsPerSide = 0;
		int numPipsFilled = pTeam ? pTeam->m_numMapVictories : 0;
		if ( numPipsFilled < 0 || numPipsFilled > numBestOfPipsPerSide )
			numPipsFilled = numBestOfPipsPerSide;

		SetNumberOfMatchesWonPips( numBestOfPipsPerSide, numPipsFilled, team );

		// se if we have a custom clan name
		wchar_t wszSafeName[MAX_TEAM_NAME_LENGTH];
		wszSafeName[0] = L'\0';

		static uint32 s_uiClanIDPrevious[2] = { 0 };
		static XUID s_xuidClan[2] = { 0 };
		uint32 uiClanID = 0 /*pTeam->GetClanID()*/;

		panorama::CImagePanel *pTeamLogo = NULL;
		panorama::CLabel *pTeamName = NULL;
		switch ( team )
		{
			case TEAM_TERRORIST:
			{
				if ( m_bSwapPlayerNames )
				{
					pTeamLogo = m_pTeamLogoL;
					pTeamName = m_pTeamName_L;
				}
				else
				{
					pTeamLogo = m_pTeamLogoR;
					pTeamName = m_pTeamName_R;
				}

				break;
			}

			case TEAM_CT:
			{
				if ( m_bSwapPlayerNames )
				{
					pTeamLogo = m_pTeamLogoR;
					pTeamName = m_pTeamName_R;
				}
				else
				{
					pTeamLogo = m_pTeamLogoL;
					pTeamName = m_pTeamName_L;
				}

				break;
			}
		}

		static const panorama::CPanoramaSymbol k_symTextUpper( "SpecTextUpper" );
		if ( ( pTeam == NULL ) || StringIsEmpty( pTeam->Get_ClanName() ) )
		{
			// if not, just use the default T or CT labels
			switch ( team )
			{
				case TEAM_TERRORIST:
					V_snwprintf( wszSafeName, ARRAYSIZE( wszSafeName ), g_pVGuiLocalize->Find( "#SFUI_T_Label" ) );
					break;

				case TEAM_CT:
					V_snwprintf( wszSafeName, ARRAYSIZE( wszSafeName ), g_pVGuiLocalize->Find( "#SFUI_CT_Label" ) );
					break;
			}

			// convert to upper case
			if ( pTeamName->BHasClass(k_symTextUpper) == false )
				pTeamName->AddClass(k_symTextUpper);
		}
		else
		{
			if ( pTeamName->BHasClass( k_symTextUpper ) == true )
				pTeamName->RemoveClass( k_symTextUpper );

			wchar_t wszName[MAX_TEAM_NAME_LENGTH];
			// we have a custom team name, convert to wide
			g_pVGuiLocalize->ConvertANSIToUnicode( pTeam->Get_ClanName(), wszName, sizeof( wszName ) );
			// now make the team name string safe
			GameUI_MakeStringSafe( wszName, wszSafeName, sizeof( wszName ) );

			// if we have a clan id and it's a new one then get it from Steam.
			if ( uiClanID && ( uiClanID != s_uiClanIDPrevious[iTeamIndex] ) )
			{
				s_xuidClan[iTeamIndex] = CSteamID( uiClanID, steamapicontext->SteamUtils()->GetConnectedUniverse(), k_EAccountTypeClan ).ConvertToUint64();
			}

			s_uiClanIDPrevious[iTeamIndex] = uiClanID;
		}

		static const panorama::CPanoramaSymbol k_symLogoFlag( "SpecTeamLogo_Flag" );

		// If there's no clan logo, check if there's a flag to use instead
		wchar_t wszFinalName[MAX_TEAM_NAME_LENGTH * 3];
		bool bHasLogoImage = false;
		if ( pTeam && !StringIsEmpty( pTeam->Get_LogoImageString() ) )
		{
			bHasLogoImage = true;

			char szBGImagePath[MAX_PATH];
			szBGImagePath[0] = '\0';

			//CUtlString strImagePath;
			//strImagePath.Format( "file://{images}/tournaments/teams/%s.svg", pTeam->Get_LogoImageString() );

			// try SVG
			//strImagePath.Format( "materials/panorama/images/tournaments/teams/%s.svg", pTeam->Get_LogoImageString() );
			V_snprintf( szBGImagePath, MAX_PATH, "materials/panorama/images/tournaments/teams/%s.svg", pTeam->Get_LogoImageString() );
			if ( !g_pFullFileSystem->FileExists( szBGImagePath ) )
			{
				// try PNG
				//strImagePath.Format( "materials/panorama/images/tournaments/teams/%s.png", pTeam->Get_LogoImageString() );
				V_snprintf( szBGImagePath, MAX_PATH, "materials/panorama/images/tournaments/teams/%s.png", pTeam->Get_LogoImageString() );
				if ( !g_pFullFileSystem->FileExists( szBGImagePath ) )
				{
					// let custom team logos have a crack at the old location
					//strImagePath.Format( "resource/flash/econ/tournaments/teams/%s.png", pTeam->Get_LogoImageString() );
					V_snprintf( szBGImagePath, MAX_PATH, "resource/flash/econ/tournaments/teams/%s.png", pTeam->Get_LogoImageString() );
					if ( !g_pFullFileSystem->FileExists( szBGImagePath ) )
					{
						bHasLogoImage = false;
					}
				}
			}

			pTeamLogo->SetVisible(bHasLogoImage);
			//pTeamLogo->SetImage(strImagePath);
			if ( bHasLogoImage)
				pTeamLogo->SetImage( CFmtStr( "s2r://%s", szBGImagePath ) );

			if ( pTeamLogo->BHasClass( k_symLogoFlag ) )
				pTeamLogo->RemoveClass( k_symLogoFlag );

			V_snwprintf( wszFinalName, ARRAYSIZE( wszFinalName ), L"%s",  wszSafeName );
		}
		else
		{
			if ( !StringIsEmpty( pTeam->Get_FlagImageString() ) )
			{
				pTeamLogo->SetVisible( true );
				if ( pTeamLogo->BHasClass( k_symLogoFlag ) == false )
					pTeamLogo->AddClass( k_symLogoFlag );

				CUtlString strImagePath;
				strImagePath.Format( "file://{images}/flags/%s.png", pTeam->Get_FlagImageString() );
				pTeamLogo->SetVisible( true );
				pTeamLogo->SetImage( strImagePath );

				V_snwprintf( wszFinalName, ARRAYSIZE( wszFinalName ), L"%s",  wszSafeName );

			}
			else
			{
				pTeamLogo->SetVisible( false );
				V_snwprintf( wszFinalName, ARRAYSIZE( wszFinalName ), L"%s",  wszSafeName );
			}
		}

		// set the text
		char szFinalNameUTF8[MAX_TEAM_NAME_LENGTH * 4];
		V_UnicodeToUTF8( wszFinalName, szFinalNameUTF8, ARRAYSIZE( szFinalNameUTF8 ) );
		pTeamName->SetText( szFinalNameUTF8, CLabel::k_ETextTypeHTML );
	}
}

void CCSGO_HudSpectator::EnableCursorInput( bool bEnable )
{
	m_Capture.SetEnabled( bEnable );
}
