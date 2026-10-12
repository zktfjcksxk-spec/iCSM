//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//
// SF Differences:
//		* Team win panel title: additive blending
//
//=============================================================================//

#include "cbase.h"
#include "csgo_hudwinpanel.h"

#include "c_cs_player.h"
#include "c_team.h"
#include "c_playerresource.h"
#include "c_cs_playerresource.h"
#include "../csgo_avatarimage.h"
#include "csgo_hudteamcounter.h"
#include "cs_player_rank_mgr.h"
#include "achievements_cs.h"
#include <engine/IEngineSound.h>

//#include "uicomponents/uicomponent_inventory.h"
//#include "uicomponents/uicomponent_medals.h"
//#include "uicomponents/uicomponent_mypersona.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


REGISTER_PANEL2D_FACTORY( CCSGO_HudWinPanel, CSGOHudWinPanel );

DEFINE_PANORAMA_EVENT_DOC( HudWinPanelShowEvent, "reason for which the panel is shown", "Fired when hud win panel is shown." );


extern bool IsTakingAFreezecamScreenshot();
extern ConVar cl_draw_only_deathnotices;
extern ConVar cl_drawhud;


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCSGO_HudWinPanel::CCSGO_HudWinPanel( panorama::CPanel2D *pParent, const char *pchID )
:
	panorama::CPanel2D( pParent, pchID ),
	CPanoramaHudElement( "CCSGO_HudWinPanel", this ),
	m_iMVP( 0 ),
	m_bShouldSetWinPanelExtraData( false ),
	m_fSetWinPanelExtraDataTime( 0.0 ),
	m_nWinEventTriggered( 0 ),
	m_nFunFactPlayer( 0 ),
	m_nFunfactToken( NULL ),
	m_nFunFactParam1( 0 ),
	m_nFunFactParam2( 0 ),
	m_nFunFactParam3( 0 )
{
	SetHiddenBits( HIDEHUD_MISCSTATUS );

	RequireLoadLayout( "file://{resources}/layout/hud/hudwinpanel.xml" );
	AddClass( "WinPanelRoot" );		// Not getting read from the xml for some reason

	m_pTitlePanel = panorama::panel_cast< panorama::CLabel * >(RequireChildInLayoutFile( "Title" ) );
	m_pSurrenderPanel = panorama::panel_cast< panorama::CLabel * >(RequireChildInLayoutFile( "Surrender" ) );
	m_pTeamLogoPanel = panorama::panel_cast< panorama::CImagePanel * >(RequireChildInLayoutFile( "TeamLogo" ) );

	m_pMVPPanel = RequireChildInLayoutFile( "MVP" );
	m_pMVPWinnerNamePanel = panorama::panel_cast< panorama::CLabel * >(RequireChildInLayoutFile( "MVPWinnerName" ) );
	m_pMVPAvatarPanel = panorama::panel_cast< CCSGO_AvatarImage * >(RequireChildInLayoutFile( "MVPAvatar" ) );
	m_pSurvivalPlacement = panorama::panel_cast< panorama::CLabel * >( RequireChildInLayoutFile( "id-survival-placement" ) );
	m_pMVPSurvivalAvatarPanel = panorama::panel_cast< CCSGO_AvatarImage * >( RequireChildInLayoutFile( "id-survival-avatar" ) );
	m_pMVPGunGameAvatarsPanel = RequireChildInLayoutFile( "MVPGunGameAvatars" );
	m_pMVPAvatar2ndPanel = panorama::panel_cast< CCSGO_AvatarImage * >(RequireChildInLayoutFile( "MVPAvatar2nd" ) );
	m_pMVPAvatar3rdPanel = panorama::panel_cast< CCSGO_AvatarImage * >(RequireChildInLayoutFile( "MVPAvatar3rd" ) );
	m_pMVPMusicKitPanel = RequireChildInLayoutFile( "MVPMusicKit" );
	m_pMVPMusicKitAnimPanel = RequireChildInLayoutFile( "MVPMusicKitAnim" );
	m_pMVPMusicKitStatTrackPanel = panorama::panel_cast< panorama::CLabel * >(RequireChildInLayoutFile( "MVPMusicKitStatTrak" ) );
	m_pMVPMusicKitNamePanel = panorama::panel_cast< panorama::CLabel * >(RequireChildInLayoutFile( "MVPMusicKitName" ) );
	m_pMVPMusicKitInfoPanel = panorama::panel_cast< panorama::CLabel * >(RequireChildInLayoutFile( "MVPMusicKitInfo" ) );
	m_pMVPMusicKitIconPanel = panorama::panel_cast< panorama::CImagePanel * >(RequireChildInLayoutFile( "MVPMusicKitIcon" ) );

	m_pFunFactPanel = RequireChildInLayoutFile( "Funfact" );
	m_pFunFactTextPanel = panorama::panel_cast<panorama::CLabel *>( RequireChildInLayoutFile( "FunFactText" ) );

	m_pMedalsPanel = RequireChildInLayoutFile( "Medals" );
	m_pMedalsContainerPanel = RequireChildInLayoutFile( "MedalsContainer" );

	m_pMedalStatsPanel = RequireChildInLayoutFile( "MedalStats" );
	m_pMedalStatsTextPanel = panorama::panel_cast<panorama::CLabel *>( RequireChildInLayoutFile( "MedalStatsText" ) );
	m_pMedalStatsIconPanel = panorama::panel_cast<panorama::CImagePanel *>( RequireChildInLayoutFile( "MedalStatsIcon" ) );

	m_pRankUpPanel = RequireChildInLayoutFile( "RankUp" );

	m_pGunGameExtraPanel = RequireChildInLayoutFile( "GunGameExtra" );
	m_pGunGameExtraTitlePanel = panorama::panel_cast<panorama::CLabel *>( RequireChildInLayoutFile( "GunGameExtraTitle" ) );
	m_pGunGameExtraWeaponNamePanel = panorama::panel_cast<panorama::CLabel *>( RequireChildInLayoutFile( "GunGameExtraWeaponName" ) );
	m_pGunGameExtraWeaponIconPanel = panorama::panel_cast<panorama::CImagePanel *>( RequireChildInLayoutFile( "GunGameExtraWeaponIcon" ) );
	m_pGunGameExtraGrenadeIconPanel = panorama::panel_cast<panorama::CImagePanel *>( RequireChildInLayoutFile( "GunGameExtraGrenadeIcon" ) );

	m_pProgressPanel = RequireChildInLayoutFile( "Progress" );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCSGO_HudWinPanel::~CCSGO_HudWinPanel()
{

}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudWinPanel::LevelInit( void )
{
	// listen for events	
	ListenForGameEvent( "round_start" );
	ListenForGameEvent( "cs_win_panel_round" );
	ListenForGameEvent( "round_mvp" );
	ListenForGameEvent( "hltv_replay" );

	ForceHide();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudWinPanel::LevelShutdown( void )
{
	StopListeningForAllEvents();

	ForceHide();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCSGO_HudWinPanel::ShouldDraw( void )
{
	if ( IsTakingAFreezecamScreenshot() )
		return false;

	if ( cl_draw_only_deathnotices.GetBool() )
		return false;

	return cl_drawhud.GetBool() && CHudElement::ShouldDraw();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudWinPanel::SetActive( bool bActive )
{
	// 	if ( !bActive && m_bVisible )
	// 	{
	// 		Hide();
	// 	}

	CHudElement::SetActive( bActive );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudWinPanel::ProcessInput( void )
{
	if ( m_bShouldSetWinPanelExtraData && m_fSetWinPanelExtraDataTime <= gpGlobals->curtime )
	{
		m_bShouldSetWinPanelExtraData = false;
		ShowWinPanelExtraData();
	}
}


//-----------------------------------------------------------------------------
// Purpose: determine if our player is an actual player (not an observer or demo playback etc.)
//-----------------------------------------------------------------------------
static bool Helper_DetermineIfLocalPlayerIsPlayingSurvival()
{
	C_CSPlayer *pLocalCsPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pLocalCsPlayer )
		return false;
	if ( pLocalCsPlayer->GetTeamNumber() != TEAM_TERRORIST )
		return false;
	
	if ( !CSGameRules() )
		return false;
	if ( !CSGameRules()->IsPlayingSurvival() )
		return false;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEG_NOINLINE void CCSGO_HudWinPanel::FireGameEvent( IGameEvent* event )
{
	const char *pEventName = event->GetName();

	if ( V_strcmp( "round_start", pEventName ) == 0 )
	{
		// Reset MVP info when round starts
		ShowMVP( NULL, CSMVP_UNDEFINED );

		ShowPanel( false );
		GetViewPortInterface()->UpdateAllPanels();
	}
	else if ( V_strcmp( "hltv_replay", pEventName ) == 0 )
	{
		if ( !event->GetInt( "delay" ) && CSGameRules() && CSGameRules()->IsPlayingSurvival() && ( m_nWinEventTriggered == Survival_Draw ) )
		{	// HLTV replay finished, in Survival we need to hide the win panel unless it is the victory panel
			ShowPanel( false );
			GetViewPortInterface()->UpdateAllPanels();
		}
	}
	else if ( V_strcmp( "round_mvp", pEventName ) == 0 )
	{
		if ( Helper_DetermineIfLocalPlayerIsPlayingSurvival() )
			return;	// we ignore this event, actual players will show their own placement

		C_BasePlayer *basePlayer = UTIL_PlayerByUserId( event->GetInt( "userid" ) );

		if ( basePlayer )
		{
			CSMvpReason_t mvpReason = (CSMvpReason_t)event->GetInt( "reason" );
			int32 nMusicKitMVPs = event->GetInt( "musickitmvps" );

			ShowMVP( ToCSPlayer( basePlayer ), mvpReason, nMusicKitMVPs );
		}

		if ( CSGameRules() && CSGameRules()->IsPlayingSurvival() )
		{	// Make sure we display placement as #1
			int nPlacement = 1;
			m_pSurvivalPlacement->SetText( CFmtStr( "#EOM_PositionPlace_%d", nPlacement ) );
			m_pSurvivalPlacement->SetVisible( true );
		}
	}
	else if ( V_strcmp( "cs_win_panel_round", pEventName ) == 0 )
	{
		if ( Helper_DetermineIfLocalPlayerIsPlayingSurvival() )
			return;	// we ignore this event, actual players will show their own placement

		m_bShouldSetWinPanelExtraData = true;
		m_fSetWinPanelExtraDataTime = gpGlobals->curtime + 1.0f;

		m_nFunFactPlayer = event->GetInt( "funfact_player" );
		m_nFunfactToken = AllocPooledString( event->GetString( "funfact_token", "" ) );
		m_nFunFactParam1 = event->GetInt( "funfact_data1" );
		m_nFunFactParam2 = event->GetInt( "funfact_data2" );
		m_nFunFactParam3 = event->GetInt( "funfact_data3" );
		
		if ( CSGameRules() && ( CSGameRules()->IsPlayingGunGameProgressive() || CSGameRules()->IsPlayingGunGameDeathmatch() ) && !CSGameRules()->IsPlayingTeamDM() )
		{
			ShowGunGameWinPanel();
		}
		else
		{
			int nConnectionProtocol = engine->GetConnectionDataProtocol();
			int iEndEvent = event->GetInt( "final_event" );
			if ( nConnectionProtocol &&
				( iEndEvent >= 0 ) &&					// backwards compatibility: we switched to consistent numbering in the enum
				( nConnectionProtocol < 13500 ) )		// and older demos have one-less numbers in "final_event" before 1.35.0.0 (Sep 15, 2015 3:20 PM release BuildID 776203)
				++iEndEvent;

			ShowTeamWinPanel( iEndEvent, 0 );
		}
	}
}

void CCSGO_HudWinPanel::ForceDisplayWinPanel( int iEndEvent )
{
	m_bShouldSetWinPanelExtraData = true;
	m_fSetWinPanelExtraDataTime = gpGlobals->curtime - 999.0f;
	int nPlacement = 0;

	if ( CSGameRules() && CSGameRules()->IsPlayingSurvival() )
	{
		//
		// Determine our placement
		//
		C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
		if ( pLocalPlayer )
		{
			ShowMVP( pLocalPlayer, CSMVP_SURVIVALSURVIVOR, 0 );

			// Determine placement
			extern uint64 UTIL_GetPlayerXUID( C_CSPlayer* );
			uint64 xuid = UTIL_GetPlayerXUID( pLocalPlayer );
			nPlacement = CSGameRules()->GetSurvivalRules()->GetPlacementFromStatMsgs( xuid, true );

			// Set the placement into the panel
			if ( nPlacement >= 1 && nPlacement <= 16 )
			{
				m_pSurvivalPlacement->SetText( CFmtStr( "#EOM_PositionPlace_%d", nPlacement ) );
				m_pSurvivalPlacement->SetVisible( true );
			}
			else if ( nPlacement >= 17 )
			{
				m_pSurvivalPlacement->SetText( CFmtStr( "%d", nPlacement ) );
				m_pSurvivalPlacement->SetVisible( true );
			}
			else
			{
				m_pSurvivalPlacement->SetVisible( false );
			}
		}
	}

	ShowTeamWinPanel( iEndEvent, nPlacement );
}

#if DEVELOPMENT_ONLY
DEVELOPMENT_ONLY_CONVAR( debug_hud_winpanel_type, 21 /*=Survival_Win*/ );
CON_COMMAND( debug_hud_winpanel_show, "Show the HUD win panel" )
{
	CCSGO_HudWinPanel * pWinPanel = GET_HUDELEMENT( CCSGO_HudWinPanel );
	if ( pWinPanel )
		pWinPanel->ForceDisplayWinPanel( debug_hud_winpanel_type.GetInt() );
}
#endif


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudWinPanel::ShowTeamWinPanel( int iEndEvent, int nPlacement )
{
	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pLocalPlayer )
		return;

	if ( pLocalPlayer->GetTeamNumber() == TEAM_UNASSIGNED && !pLocalPlayer->IsHLTV() )
		return;
	
	if ( cl_draw_only_deathnotices.GetBool() )
		return;

	static const panorama::CPanoramaSymbol k_symCTWin( "WinPanelRoot--CTWin" );
	static const panorama::CPanoramaSymbol k_symTWin( "WinPanelRoot--TWin" );
	static const panorama::CPanoramaSymbol k_symDraw( "WinPanelRoot--Draw" );
	static const panorama::CPanoramaSymbol k_symGunGame( "WinPanelRoot--GunGame" );
	static const panorama::CPanoramaSymbol k_symShowTeamLogo( "WinPanelRoot--ShowTeamLogo" );
	static const panorama::CPanoramaSymbol k_symSurvivalVictory( "WinPanelRoot--SurvivalVictory" );
	static const panorama::CPanoramaSymbol k_symSurvivalDeath( "WinPanelRoot--SurvivalDeath" );
	static const panorama::CPanoramaSymbol k_classesToRemove[] = { k_symCTWin, k_symTWin, k_symDraw, k_symGunGame, k_symSurvivalVictory, k_symSurvivalDeath };

	static const panorama::CPanoramaSymbol k_symSurvivalPlacement1( "survival-winner--placement-1" );
	static const panorama::CPanoramaSymbol k_symSurvivalPlacement2( "survival-winner--placement-2" );
	static const panorama::CPanoramaSymbol k_symSurvivalPlacement3( "survival-winner--placement-3" );
	static const panorama::CPanoramaSymbol k_symSurvivalPlacement4( "survival-winner--placement-4" );
	static const panorama::CPanoramaSymbol k_symSurvivalPlacements[] = { k_symSurvivalPlacement1, k_symSurvivalPlacement2, k_symSurvivalPlacement3, k_symSurvivalPlacement4 };

	// Fade in WinPanel
	ShowPanel( true );
	m_nWinEventTriggered = iEndEvent;

	RemoveClasses( k_classesToRemove, V_ARRAYSIZE( k_classesToRemove ) );
	RemoveClasses( k_symSurvivalPlacements, V_ARRAYSIZE( k_symSurvivalPlacements ) );

	const char *szTitle = nullptr;
	int nWinningTeam = WINNER_NONE;
	switch ( iEndEvent )
	{
	case Target_Bombed:
	case VIP_Assassinated:
	case Terrorists_Escaped:
	case Terrorists_Win:
	case Hostages_Not_Rescued:
	case VIP_Not_Escaped:
	case CTs_Surrender:
	case Terrorists_Planted:
		nWinningTeam = WINNER_TER;
		AddClass( k_symTWin );
		szTitle = "#SFUI_WinPanel_T_Win";
		break;

	case VIP_Escaped:
	case CTs_PreventEscape:
	case Escaping_Terrorists_Neutralized:
	case Bomb_Defused:
	case CTs_Win:
	case All_Hostages_Rescued:
	case Target_Saved:
	case Terrorists_Not_Escaped:
	case Terrorists_Surrender:
	case CTs_ReachedHostage:
		nWinningTeam = WINNER_CT;
		AddClass( k_symCTWin );
		szTitle = "#SFUI_WinPanel_CT_Win";
		break;

	case Round_Draw:
		nWinningTeam = WINNER_DRAW;
		AddClass( k_symDraw );
		szTitle = "#SFUI_WinPanel_Round_Draw";
		break;

	case Survival_Win:
		nWinningTeam = WINNER_DRAW;
		AddClass( k_symDraw );
		AddClass( k_symSurvivalVictory );
		nPlacement = 1;
		szTitle = "#SFUI_Notice_Survival_Win";
		break;

	case Survival_Draw:
		nWinningTeam = WINNER_DRAW;
		AddClass( k_symDraw );
		AddClass( k_symSurvivalDeath );
		nPlacement = clamp< int >( nPlacement, ( int ) 1, ( int ) V_ARRAYSIZE( k_symSurvivalPlacements ) );
		szTitle = "#SFUI_Notice_Survival_Draw";
		break;

	default:
		Assert( 0 );
		break;
	}

	// Add placement style
	if ( nPlacement > 0 && nPlacement <= V_ARRAYSIZE( k_symSurvivalPlacements ) )
		AddClass( k_symSurvivalPlacements[ nPlacement - 1 ] );

	C_Team *pTeam = GetGlobalTeam( nWinningTeam );

	//
	// Set title
	//
	
	if ( CSGameRules() && CSGameRules()->IsPlayingCoopMission() )
	{
		if ( nWinningTeam == WINNER_CT )
		{
			szTitle = "#SFUI_WinPanel_Coop_Mission_Win";
		}
		else
		{
			szTitle = "#SFUI_WinPanel_Coop_Mission_Lose";
		}
	}
	else if ( ( pTeam != NULL ) && !StringIsEmpty( pTeam->Get_ClanName() ) )
	{
		m_pTitlePanel->SetDialogVariable( "s1", pTeam->Get_ClanName() );
		szTitle = "#SFUI_WinPanel_Team_Win_Team";
	}
	
	m_pTitlePanel->SetText( szTitle );

	//
	// Surrender
	//

	if ( iEndEvent == CTs_Surrender )
	{
		m_pSurrenderPanel->SetText( "#winpanel_end_cts_surrender" );
	}
	else if ( iEndEvent == Terrorists_Surrender )
	{
		m_pSurrenderPanel->SetText( "#winpanel_end_terrorists_surrender" );
	}
	else
	{
		m_pSurrenderPanel->SetText( "" );
	}

	//
	// Team Logo
	//

	if ( pTeam && !StringIsEmpty( pTeam->Get_LogoImageString() ) )
	{
		AddClass( k_symShowTeamLogo );

		CUtlString strImagePath;
		strImagePath.Format( "file://{images}/tournaments/teams/%s.svg", pTeam->Get_LogoImageString() );
		//pLayer->SetPath( strImagePath );
		m_pTeamLogoPanel->SetImage( strImagePath );

		//CFmtStr iconPath( "file://{images_econ}/econ/tournaments/teams/%s.png", pTeam->Get_LogoImageString() );
		//m_pTeamLogoPanel->SetImage( iconPath.String() );
	}
	else
	{
		RemoveClass( k_symShowTeamLogo );
	}

	if ( GetViewPortInterface() && !pLocalPlayer->IsSpectator() && !engine->IsHLTV() )
	{
		GetViewPortInterface()->ShowPanel( PANEL_ALL, false );
	}

	panorama::DispatchEvent( HudWinPanelShowEvent(), this, iEndEvent );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudWinPanel::ShowGunGameWinPanel()
{
	static const panorama::CPanoramaSymbol k_symCTWin( "WinPanelRoot--CTWin" );
	static const panorama::CPanoramaSymbol k_symTWin( "WinPanelRoot--TWin" );
	static const panorama::CPanoramaSymbol k_symDraw( "WinPanelRoot--Draw" );
	static const panorama::CPanoramaSymbol k_symGunGame( "WinPanelRoot--GunGame" );
	static const panorama::CPanoramaSymbol k_classesToRemove[] = { k_symCTWin, k_symTWin, k_symDraw };

	// Fade in WinPanel
	ShowPanel( true );

	RemoveClasses( k_classesToRemove, V_ARRAYSIZE( k_classesToRemove ) );
	AddClass( k_symGunGame );

	if ( GetViewPortInterface() )
	{
		GetViewPortInterface()->ShowPanel( PANEL_ALL, false );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudWinPanel::ShowMVP( C_CSPlayer* pPlayer, CSMvpReason_t reason, int32 nMusicKitStatTrak /*= 0*/ )
{
	static const panorama::CPanoramaSymbol k_symMVPHidden( "MVP--Hidden" );
	static const panorama::CPanoramaSymbol k_symMVPGunGameAvatarsHidden( "MVP__GunGameAvatars--Hidden" );
	static const panorama::CPanoramaSymbol k_symMVPGunGameAvatarsScrollAnim( "MVP__GunGameAvatars--ScrollAnim" );
	
	if ( pPlayer )
	{
		m_iMVP = pPlayer->entindex();

		if ( g_PR )
		{
			m_pMVPPanel->RemoveClass( k_symMVPHidden );

			XUID xuidPlayer = g_PR->GetXuid( pPlayer->entindex() );

			//
			// Fill "Winner Name" label
			//

			wchar_t wszPlayerName[MAX_DECORATED_PLAYER_NAME_LENGTH];
			( (C_CS_PlayerResource*)g_PR )->GetDecoratedPlayerName( pPlayer->entindex(), wszPlayerName, sizeof( wszPlayerName ), k_EDecoratedPlayerNameFlag_Simple | k_EDecoratedPlayerNameFlag_DontUseNameOfControllingPlayer );
			// truncate the player name so it doesn't get too long
			bool bTruncatePlayerName = true;
			if ( reason == CSMVP_SURVIVALSURVIVOR )
				bTruncatePlayerName = false;
			if ( bTruncatePlayerName )
				TruncatePlayerName( wszPlayerName, ARRAYSIZE( wszPlayerName ), 16 );
			// Convert player's name to UTF-8 strings
			// Each Unicode code point can expand to as many as four bytes in UTF-8
			char szPlayerNameUTF8[4 * MAX_DECORATED_PLAYER_NAME_LENGTH];
			V_UnicodeToUTF8( wszPlayerName, szPlayerNameUTF8, ARRAYSIZE( szPlayerNameUTF8 ) );

			const char* mvpReasonToken = NULL;
			switch ( reason )
			{
			case CSMVP_ELIMINATION:
				mvpReasonToken = "#Panorama_winpanel_mvp_award_kills";
				break;
			case CSMVP_BOMBPLANT:
				mvpReasonToken = "#Panorama_winpanel_mvp_award_bombplant";
				break;
			case CSMVP_BOMBDEFUSE:
				mvpReasonToken = "#Panorama_winpanel_mvp_award_bombdefuse";
				break;
			case CSMVP_HOSTAGERESCUE:
				mvpReasonToken = "#Panorama_winpanel_mvp_award_rescue";
				break;
			case CSMVP_GUNGAMEWINNER:
				mvpReasonToken = "#Panorama_winpanel_mvp_award_gungame";
				break;
			case CSMVP_TEAMDMSCORE:
				mvpReasonToken = "#Panorama_winpanel_mvp_award_score";
				break;
			case CSMVP_SURVIVALSURVIVOR:
				mvpReasonToken = "#Panorama_winpanel_mvp_winner";
				break;
			default:
				mvpReasonToken = "#Panorama_winpanel_mvp_award";
				break;
			}

			this->SetDialogVariable( "mvp", szPlayerNameUTF8 );
			m_pMVPWinnerNamePanel->SetText( mvpReasonToken );

			//
			//	Avatar
			//

			const char *pszDefaultAvatartImage = "avatar-CT";
			if ( pPlayer->GetTeamNumber() == TEAM_TERRORIST )
			{
				pszDefaultAvatartImage = "avatar-TERRORIST";
			}
			m_pMVPAvatarPanel->SetDefaultImage( CFmtStr( "file://{images}/icons/scoreboard/%s.png", pszDefaultAvatartImage ) );
			m_pMVPAvatarPanel->SetSteamID( xuidPlayer );
			m_pMVPSurvivalAvatarPanel->SetDefaultImage( CFmtStr( "file://{images}/icons/scoreboard/%s.png", pszDefaultAvatartImage ) );
			m_pMVPSurvivalAvatarPanel->SetSteamID( xuidPlayer );

			//
			// Gun Game runner up avatars
			//

			m_pMVPGunGameAvatarsPanel->SetHasClass( k_symMVPGunGameAvatarsHidden, ( reason != CSMVP_GUNGAMEWINNER ) );
			if ( reason == CSMVP_GUNGAMEWINNER )
			{
				int nSecond = -1;
				int nThird = -1;

				CCSGO_HudTeamCounter *pTeamCounter = GET_HUDELEMENT( CCSGO_HudTeamCounter );
				if ( pTeamCounter )
				{
					for ( int i = 0; i < MAX_PLAYERS; ++i )
					{
						int indx = pTeamCounter->GetPlayerEntIndexInSlot( i );
						if ( pPlayer->entindex() != indx )
						{
							if ( nSecond == -1 )
							{
								nSecond = indx;
							}
							else
							{
								nThird = indx;
								break;
							}
						}
					}
				}

				CBasePlayer* pBasePlayer1 = UTIL_PlayerByIndex( nSecond );
				C_CSPlayer* pPlayer1 = ToCSPlayer( pBasePlayer1 );
				if ( pPlayer1 )
				{
					pszDefaultAvatartImage = "avatar-CT";
					if ( pPlayer1->GetTeamNumber() == TEAM_TERRORIST )
					{
						pszDefaultAvatartImage = "avatar-TERRORIST";
					}
					m_pMVPAvatar2ndPanel->SetDefaultImage( CFmtStr( "file://{images}/icons/scoreboard/%s.png", pszDefaultAvatartImage ) );
				}
				m_pMVPAvatar2ndPanel->SetSteamID( g_PR->GetXuid( nSecond ) );

				CBasePlayer* pBasePlayer2 = UTIL_PlayerByIndex( nThird );
				C_CSPlayer* pPlayer2 = ToCSPlayer( pBasePlayer2 );
				if ( pPlayer2 )
				{
					pszDefaultAvatartImage = "avatar-CT";
					if ( pPlayer2->GetTeamNumber() == TEAM_TERRORIST )
					{
						pszDefaultAvatartImage = "avatar-TERRORIST";
					}
					m_pMVPAvatar3rdPanel->SetDefaultImage( CFmtStr( "file://{images}/icons/scoreboard/%s.png", pszDefaultAvatartImage ) );
				}
				
				m_pMVPAvatar3rdPanel->SetSteamID( g_PR->GetXuid( nThird ) );

				m_pMVPGunGameAvatarsPanel->TriggerClass( k_symMVPGunGameAvatarsScrollAnim );
			}

			//
			// Music Kit
			//

			ShowMusicKit( xuidPlayer, szPlayerNameUTF8, nMusicKitStatTrak );
		}
	}
	else
	{
		m_pMVPPanel->AddClass( k_symMVPHidden );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudWinPanel::ShowMusicKit( XUID xuidPlayer, const char *szPlayerNameUTF8, int32 nMusicKitStatTrak )
{
	/* removed for partner depot */
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudWinPanel::ShowWinPanelExtraData()
{	
	if ( !CSGameRules() )
		return;

	const bool bIsWarmup = CSGameRules()->IsWarmupPeriod();

	const CUtlVector<RankIncreasedEvent_t> &medalRankIncreases = g_PlayerRankManager.GetRankIncreasesThisRound();
	const CUtlVector<MedalEarnedEvent_t> &medalsAwarded = g_PlayerRankManager.GetMedalsEarnedThisRound();
	CUtlVector<MedalStatEvent_t> medalStatsAwarded;
	g_PlayerRankManager.GetMedalStatsEarnedThisRound( medalStatsAwarded );
	
#if 0
	/* ##########  ACHIEMENTS TEST DATA ########## */
	CBaseAchievement achievement1, achievement2;
	achievement1.SetName( "base_scamper" );
	achievement2.SetName( "kill_bomb_pickup" );
	CUtlVector<MedalEarnedEvent_t> medalsAwarded;
	medalsAwarded.AddToTail( MedalEarnedEvent_t( &achievement1, MEDAL_CATEGORY_COMBAT ) );
	medalsAwarded.AddToTail( MedalEarnedEvent_t( &achievement2, MEDAL_CATEGORY_COMBAT ) );
	/* ########## END ACHIEMENTS TEST DATA ########## */
#endif

#if 0
	/* ########## MEDAL STATS TEST DATA ########## */
	CBaseAchievement achievement1;
	achievement1.SetName( "win_map_de_dust" );
	achievement1.SetCount( 1 );
	achievement1.SetGoal( 2 );
	medalStatsAwarded.AddToTail( MedalStatEvent_t( &achievement1, MEDAL_CATEGORY_COMBAT, CSSTAT_KILLS_HEADSHOT ) );
	/* ########## END TEST DATA ########## */
#endif

#if 0
	/* ########## MEDAL RANK INCREASE TEST DATA ########## */
	CBaseAchievement achievement1, achievement2;
	achievement1.SetName( "base_scamper" );
	achievement2.SetName( "base_scamper" );
	CUtlVector<MedalEarnedEvent_t> medalRankIncreases;
	medalRankIncreases.AddToTail( MedalEarnedEvent_t( &achievement1, MEDAL_CATEGORY_COMBAT ) );
	medalRankIncreases.AddToTail( MedalEarnedEvent_t( &achievement2, MEDAL_CATEGORY_WEAPON ) );
	//medalRankIncreases.AddToTail( MedalEarnedEvent_t( &achievement2, MEDAL_CATEGORY_SEASON_COIN ) );
	/* ########## END TEST DATA ########## */
#endif


	// CSGO-1618: Don't show achievements for panorama
	bool bShowAchievements = !GameUI().IsPanoramaEnabled();

	panorama::CPanel2D *pPanelToShow = nullptr;
	bool bShowProgress = false;
	bool bHideProgressAndFunFact = false;
	int nIdealProgressCatagory = MEDAL_CATEGORY_NONE;

	if ( !bIsWarmup && medalRankIncreases.Count() > 0 && bShowAchievements )
	{
		int nTotalRankIncreases = medalRankIncreases.Count();
		int nBestIndex = 0;
		int nHighestRank = 0;
		CUtlVector< int > tieList;

		// find the place where we earned the highest rank
		for ( int i = 0; i < nTotalRankIncreases; i++ )
		{
			nHighestRank = g_PlayerRankManager.CalculateRankForCategory( medalRankIncreases[nBestIndex].m_category );
			int nCurRank = g_PlayerRankManager.CalculateRankForCategory( medalRankIncreases[i].m_category );
			int nDelta = nCurRank - nHighestRank;

			if ( medalRankIncreases[i].m_category >= MEDAL_CATEGORY_ACHIEVEMENTS_END )
			{
				nBestIndex = i;
				tieList.RemoveAll();
			}
			else if ( nDelta == 0 )
			{
				// keep track of the ranks of the same value
				nBestIndex = i;
				tieList.AddToTail( i );
			}
			else if ( nDelta > 0 )
			{
				nBestIndex = i;
				tieList.RemoveAll();
			}
		}

		if ( tieList.Count() > 0 )
		{
			// break any ties by picking on randomly
			nBestIndex = tieList[RandomInt( 0, tieList.Count() - 1 )];
		}

		bool bIsCoinLevelUp = false;

		MedalCategory_t nCurrentBestCatagory = medalRankIncreases[nBestIndex].m_category;
		int nCurrentRankForCatagory =  g_PlayerRankManager.CalculateRankForCategory( medalRankIncreases[nBestIndex].m_category );

		if ( nHighestRank < ( MEDAL_CATEGORY_COUNT - 1 ) )
		{
			nIdealProgressCatagory = nCurrentBestCatagory;
		}

		// Set dialog variables used by the "rank up" panel

		panorama::CLocStringSafePointer pchCurrentBestCat = panorama::UILocalize()->PchFindToken(
			nullptr,
			g_PlayerRankManager.GetMedalCatagoryName( nCurrentBestCatagory ),
			panorama::k_nLocalizeMaxChars,
			panorama::k_eStringTruncationStyle_None,
			panorama::k_eStringTransformStyle_None,
			panorama::k_eStringEscapeStyle_None );
		
		panorama::CLocStringSafePointer pchCurrentRankCat = nullptr;
		int nDiagVarCurrentRank = ( bIsCoinLevelUp ? ( nCurrentRankForCatagory - 1 ) : nCurrentRankForCatagory );
		if ( nDiagVarCurrentRank >= 0 )
		{
			pchCurrentRankCat = panorama::UILocalize()->PchFindToken(
				nullptr,
				g_PlayerRankManager.GetMedalCatagoryRankName( nDiagVarCurrentRank ),
				panorama::k_nLocalizeMaxChars,
				panorama::k_eStringTruncationStyle_None,
				panorama::k_eStringTransformStyle_None,
				panorama::k_eStringEscapeStyle_None );
		}

		m_pRankUpPanel->SetDialogVariable( "rank_increase", nTotalRankIncreases );
		m_pRankUpPanel->SetDialogVariable( "current_best_cat", pchCurrentBestCat->String() );
		m_pRankUpPanel->SetDialogVariable( "current_rank_cat", ( pchCurrentRankCat ? pchCurrentRankCat->String() : "" ) );

		// Set the state (single rank increase, multi rank increase, coin) of the "rank up" panel
		// used to determine which label to display

		static const panorama::CPanoramaSymbol k_symRankUpSingleRank( "RankUp--SingleRank" );
		static const panorama::CPanoramaSymbol k_symRankUpCoin( "RankUp--Coin" );
		static const panorama::CPanoramaSymbol k_symRankUpMultiRank( "RankUp--MultiRank" );

		m_pRankUpPanel->SetHasClass( k_symRankUpSingleRank, ( !bIsCoinLevelUp && ( nTotalRankIncreases <= 1 ) ) );
		m_pRankUpPanel->SetHasClass( k_symRankUpCoin, bIsCoinLevelUp );
		m_pRankUpPanel->SetHasClass( k_symRankUpMultiRank, ( !bIsCoinLevelUp && ( nTotalRankIncreases > 1 ) ) );
		
		pPanelToShow = m_pRankUpPanel;
		bShowProgress = true;
	}
	else if ( !bIsWarmup && medalsAwarded.Count() > 0 && bShowAchievements )
	{
		static const panorama::CPanoramaSymbol k_symMedalIconHidden( "Medals__Icon--Hidden" );
		static const panorama::CPanoramaSymbol k_symMedalIconAnim( "Medals__Icon--Anim" );
		
		for ( int nIcon = 0; nIcon < m_pMedalsContainerPanel->GetChildCount(); ++nIcon )
		{
			panorama::CImagePanel *pIconPanel = panorama::panel_cast< panorama::CImagePanel * >( m_pMedalsContainerPanel->GetChild( nIcon ) );
			CBaseAchievement *pAchievement = medalsAwarded[nIcon].m_pAchievement;

			if ( nIcon < medalsAwarded.Count() && pAchievement )
			{
				pIconPanel->SetImage( CFmtStr( "file://{images}/icons/achievements/%s.jpg", pAchievement->GetName() ).String() );

				pIconPanel->RemoveClass( k_symMedalIconHidden );
				pIconPanel->TriggerClass( k_symMedalIconAnim );
			}
			else
			{
				pIconPanel->AddClass( k_symMedalIconHidden );
			}
		}
		
		pPanelToShow = m_pMedalsPanel;
		bShowProgress = true;
	}
	else if ( !bIsWarmup && medalStatsAwarded.Count() > 0 && bShowAchievements  )
	{
		MedalStatEvent_t *pBestStat = NULL;
		float flHighestCompletionPct = -1.0f;
		FOR_EACH_VEC( medalStatsAwarded, i )
		{
			MedalStatEvent_t&stat = medalStatsAwarded[i];
			float pct = (float)( stat.m_pAchievement->GetCount() ) / (float)( stat.m_pAchievement->GetGoal() );
			if ( pct > flHighestCompletionPct )
			{
				flHighestCompletionPct = pct;
				pBestStat = &medalStatsAwarded[i];
			}
		}

		if ( pBestStat )
		{
			const char* pszLocToken = GetLocTokenForStatId( pBestStat->m_StatType );
			if ( pszLocToken && pBestStat->m_pAchievement )
			{
				panorama::CLocStringSafePointer pchAchievementName = panorama::UILocalize()->PchFindToken( nullptr,
					CFmtStr( "#%s_NAME", pBestStat->m_pAchievement->GetName() ).String(),
					panorama::k_nLocalizeMaxChars, panorama::k_eStringTruncationStyle_None,
					panorama::k_eStringTransformStyle_None,
					panorama::k_eStringEscapeStyle_None );
				
				m_pMedalStatsIconPanel->SetImage( CFmtStr( "file://{images}/icons/achievements/%s.jpg", pBestStat->m_pAchievement->GetName() ).String() );
				
				m_pMedalStatsTextPanel->SetDialogVariable( "s1", CNumStr( pBestStat->m_pAchievement->GetCount() ).String() );
				m_pMedalStatsTextPanel->SetDialogVariable( "s2", CNumStr( pBestStat->m_pAchievement->GetGoal() ).String() );
				m_pMedalStatsTextPanel->SetDialogVariable( "s3", pchAchievementName->String() );
				m_pMedalStatsTextPanel->SetText( CFmtStr( "#%s", pszLocToken ).String() );
				
				pPanelToShow = m_pMedalStatsPanel;
			}
		}
	}
	else if ( !bIsWarmup && CSGameRules() && CSGameRules()->IsPlayingGunGame() )
	{
		C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
		if ( pPlayer )
		{
			const char *pchClassName = "";		// "weapon_m249";
			const char *pchPrintName = "";		// "#SFUI_WPNHUD_M249";
			const char *pchTitle = "";			// "#SFUI_WS_GG_YourNextWeaponIs";
			const char *pchGrenade = "";		// "weapon_molotov";
			bool bShowGunGameExtra = false;

			//
			// Collect data
			//

			const CEconItemDefinition* pDef = CSGameRules()->GetNextGunGameWeapon( pPlayer->GetPlayerGunGameWeaponIndex(), pPlayer->GetTeamNumber() );
			// This can fail when the user has already won (index >= number of weapons)
			if ( pDef && pDef->GetDefinitionIndex() != 0 )
			{
				pchClassName = pDef->GetDefinitionName();
				pchPrintName = pDef->GetItemBaseName();
			}

			int nKillValue = pPlayer->GetNumGunGameTRKillPoints();
			int nCurIndex = (float)pPlayer->GetPlayerGunGameWeaponIndex();
			int nMaxIndex = CSGameRules()->GetNumProgressiveGunGameWeapons( pPlayer->GetTeamNumber() );
			if ( nCurIndex != nMaxIndex && nKillValue > 0 )
			{
				bShowGunGameExtra = true;

				if ( const CEconItemDefinition* pBonusGrenade = CSGameRules()->GetGunGameTRBonusGrenade( pPlayer ) )
				{
					pchGrenade = pBonusGrenade->GetDefinitionName();
				}

				if ( CSGameRules()->IsPlayingGunGameTRBomb() )
				{
					pchTitle = "#SFUI_WS_GG_YourNextWeaponIs";
				}
				else
				{
					pchTitle = "#SFUI_WS_GG_NextWep";
				}
			}

			extern ConVar mp_maxrounds;
			int nextRound = CSGameRules()->GetTotalRoundsPlayed();
			if ( nextRound >= mp_maxrounds.GetInt()
				|| ( CSGameRules()->HasHalfTime() && nextRound == mp_maxrounds.GetInt() / 2 ) )
			{
				bShowGunGameExtra = false;
			}



			//
			// Setup Gun Game Extra Data panel
			//

			if ( bShowGunGameExtra )
			{
				// strip the weapon_* from the weapon name and grenade name
				if ( IsWeaponClassname( pchClassName ) )
				{
					pchClassName += WEAPON_CLASSNAME_PREFIX_LENGTH;
				}
				if ( IsWeaponClassname( pchGrenade ) )
				{
					pchGrenade += WEAPON_CLASSNAME_PREFIX_LENGTH;
				}

				if ( V_strcmp( "knife", pchClassName ) == 0 )
				{
					if ( pPlayer->GetTeamNumber() == TEAM_CT )
					{
						pchClassName = "knife_ct";
					}
					else
					{
						pchClassName = "knife_t";
					}
				}

				m_pGunGameExtraTitlePanel->SetText( pchTitle );
				m_pGunGameExtraWeaponNamePanel->SetText( pchPrintName );
				if ( pchClassName && pchClassName[0] != '\0' )
				{
					m_pGunGameExtraWeaponIconPanel->SetImage( CFmtStr( "file://{images}/icons/equipment/%s.svg", pchClassName ).String(), nullptr, false, -1, 45 );
					m_pGunGameExtraWeaponIconPanel->SetVisible( true );
				}
				else
				{
					m_pGunGameExtraWeaponIconPanel->SetVisible( false );
				}
				if ( pchGrenade && pchGrenade[0] != '\0' )
				{
					m_pGunGameExtraGrenadeIconPanel->SetImage( CFmtStr( "file://{images}/icons/equipment/%s.svg", pchGrenade ).String(), nullptr, false, -1, 45 );
					m_pGunGameExtraGrenadeIconPanel->SetVisible( true );
				}
				else
				{
					m_pGunGameExtraGrenadeIconPanel->SetVisible( false );
				}

				pPanelToShow = m_pGunGameExtraPanel;
			}
		}
	}

	if ( !bHideProgressAndFunFact )
	{
		if ( bShowProgress )
		{
			ShowProgress( nIdealProgressCatagory );
		}
		else if ( !bIsWarmup )
		{
			// Otherwise we show a FUN FACT
			ShowFunFact();
		}
	}

	if ( pPanelToShow )
	{
		C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
		if ( pLocalPlayer )
		{
			C_RecipientFilter filter;
			filter.AddRecipient( pLocalPlayer );
			C_BaseEntity::EmitSound( filter, SOUND_FROM_WORLD, "Player.InfoPanel" );
		}
		
		ShowWinPanelExtra( pPanelToShow );
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCSGO_HudWinPanel::ShowProgress( int nIdealProgressCatagory )
{
	bool bShowProgress = true;
	
	if ( nIdealProgressCatagory == -1 )
	{
		CUtlVector< int > nProgressCatagories;

		for ( int i = 0; i < MEDAL_CATEGORY_ACHIEVEMENTS_END; i++ )
		{
			int nTempRank = g_PlayerRankManager.CalculateRankForCategory( (MedalCategory_t)i );
			if ( nTempRank < ( MEDAL_RANK_COUNT - 1 ) )
				nProgressCatagories.AddToTail( i );
		}

		if ( nProgressCatagories.Count() == 0 )
		{
			// we maxed out all of our ranks, congrats!!!!
			bShowProgress = false;
		}
		else
		{
			// we don't have an ideal category to show a hint, so pick one to display
			nIdealProgressCatagory = nProgressCatagories[RandomInt( 0, nProgressCatagories.Count() - 1 )];
			nProgressCatagories.RemoveAll();
		}
	}

	if ( bShowProgress )
	{
		int nCurrentRank = g_PlayerRankManager.CalculateRankForCategory( (MedalCategory_t)nIdealProgressCatagory );
		int nMinMedalsNeeded = g_PlayerRankManager.GetMinMedalsForRank( (MedalCategory_t)nIdealProgressCatagory, (MedalRank_t)( MIN( nCurrentRank + 1, (int)( MEDAL_RANK_COUNT - 1 ) ) ) );
		int nMedalsAchieved = g_PlayerRankManager.CountAchievedInCategory( (MedalCategory_t)nIdealProgressCatagory );

		panorama::CLocStringSafePointer pchMedalCatName = panorama::UILocalize()->PchFindToken( nullptr,
			g_PlayerRankManager.GetMedalCatagoryName( (MedalCategory_t)nIdealProgressCatagory ),
			panorama::k_nLocalizeMaxChars,
			panorama::k_eStringTruncationStyle_None,
			panorama::k_eStringTransformStyle_None,
			panorama::k_eStringEscapeStyle_None );

		m_pProgressPanel->SetDialogVariable( "s1", CNumStr( nMinMedalsNeeded-nMedalsAchieved ).String() );
		m_pProgressPanel->SetDialogVariable( "s2", ( pchMedalCatName ? pchMedalCatName->String() : "" ) );

		ShowWinPanelExtra( m_pProgressPanel );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCSGO_HudWinPanel::ShowFunFact()
{
	if ( m_nFunFactPlayer == GetLocalPlayerIndex() )
	{
		CEG_PROTECT_VIRTUAL_FUNCTION( SFHudWinPanel_FireGameEvent );
	}

	const char *pFunFact = STRING( m_nFunfactToken );
	if ( pFunFact && V_strlen( pFunFact ) > 0 )
	{
		wchar_t playerText[MAX_DECORATED_PLAYER_NAME_LENGTH];
		if ( m_nFunFactPlayer >= 1 && m_nFunFactPlayer <= MAX_PLAYERS )
		{
			playerText[0] = L'\0';

			C_CS_PlayerResource *cs_PR = dynamic_cast<C_CS_PlayerResource *>( g_PR );
			if ( !cs_PR )
				return;

			cs_PR->GetDecoratedPlayerName( m_nFunFactPlayer, playerText, sizeof( playerText ), k_EDecoratedPlayerNameFlag_Simple );

			if ( playerText[0] == L'\0' )
			{
				// TODO Replace with panorama::UILocalize ?
				V_snwprintf( playerText, ARRAYSIZE( playerText ), PRI_WS_FOR_WS, g_pVGuiLocalize->Find( "#winpanel_former_player" ) );
			}
		}
		else
		{
			V_snwprintf( playerText, ARRAYSIZE( playerText ), L"" );
		}
		// Convert player's name to UTF-8 strings
		// Each Unicode code point can expand to as many as four bytes in UTF-8
		char szPlayerNameUTF8[4 * MAX_DECORATED_PLAYER_NAME_LENGTH];
		V_UnicodeToUTF8( playerText, szPlayerNameUTF8, ARRAYSIZE( szPlayerNameUTF8 ) );

		m_pFunFactTextPanel->SetDialogVariable( "s1", szPlayerNameUTF8 );
		m_pFunFactTextPanel->SetDialogVariable( "s2", CNumStr( m_nFunFactParam1 ).String() );
		m_pFunFactTextPanel->SetDialogVariable( "s3", CNumStr( m_nFunFactParam2 ).String() );
		m_pFunFactTextPanel->SetDialogVariable( "s4", CNumStr( m_nFunFactParam3 ).String() );
		
		m_pFunFactTextPanel->SetText( pFunFact );

		ShowWinPanelExtra( m_pFunFactPanel );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Fade win panel in / out
//-----------------------------------------------------------------------------
void CCSGO_HudWinPanel::ShowPanel( bool bShow )
{
	static const panorama::CPanoramaSymbol k_symForceHide( "WinPanelRoot--ForceHide" );
	static const panorama::CPanoramaSymbol k_symShowAnim( "WinPanelRoot--ShowAnim" );
	static const panorama::CPanoramaSymbol k_symHideAnim( "WinPanelRoot--HideAnim" );

	if ( bShow )
	{
		RemoveClass( k_symForceHide );
		RemoveClass( k_symHideAnim );
		AddClass( k_symShowAnim );
	}
	else
	{
		if ( BHasClass( k_symShowAnim ) )
		{
			RemoveClass( k_symForceHide );
			RemoveClass( k_symShowAnim );
			AddClass( k_symHideAnim );
		}
	}

	m_bVisible = bShow;

	// Always hide extra data panels
	HideAllWinPanelExtra();
}


//-----------------------------------------------------------------------------
// Purpose: Force hide the panel without playing any animations
//-----------------------------------------------------------------------------
void CCSGO_HudWinPanel::ForceHide()
{
	static const panorama::CPanoramaSymbol k_symForceHide( "WinPanelRoot--ForceHide" );
	static const panorama::CPanoramaSymbol k_symShowAnim( "WinPanelRoot--ShowAnim" );
	static const panorama::CPanoramaSymbol k_symHideAnim( "WinPanelRoot--HideAnim" );

	RemoveClass( k_symHideAnim );
	RemoveClass( k_symShowAnim );
	AddClass( k_symForceHide );

	m_bVisible = false;
}


//-----------------------------------------------------------------------------
// Purpose: Hide all extra data panels (fun facts, medals, ...)
//-----------------------------------------------------------------------------
void CCSGO_HudWinPanel::HideAllWinPanelExtra()
{
	static const panorama::CPanoramaSymbol k_symHidden( "WinPanelRow--Hidden" );
	
	m_pFunFactPanel->AddClass( k_symHidden );
	m_pProgressPanel->AddClass( k_symHidden );
	m_pMedalsPanel->AddClass( k_symHidden );
	m_pMedalStatsPanel->AddClass( k_symHidden );
	m_pRankUpPanel->AddClass( k_symHidden );
	m_pGunGameExtraPanel->AddClass( k_symHidden );
}


//-----------------------------------------------------------------------------
// Purpose: Show the given "extra data" panel (fun facts, medals awarded ...)
//-----------------------------------------------------------------------------
void CCSGO_HudWinPanel::ShowWinPanelExtra( panorama::CPanel2D *pPanel )
{
	static const panorama::CPanoramaSymbol k_symRowHidden( "WinPanelRow--Hidden" );
	static const panorama::CPanoramaSymbol k_symRowAnim( "WinPanelRow--Anim" );

	pPanel->RemoveClass( k_symRowHidden );
	pPanel->TriggerClass( k_symRowAnim );
}