//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "cbase.h"
#include "csgo_huduniquealerts.h"
#include "localize/ilocalize.h"
#include "cs_gamerules.h"
#include "c_cs_player.h"
#include <engine/IEngineSound.h>
#include "clientsteamcontext.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

extern ConVar cl_draw_only_deathnotices;
extern ConVar item_debug_give_fake_random_tourney_awards;
extern ConVar cl_drawhud;
extern ConVar cl_draw_only_deathnotices;

extern bool IsTakingAFreezecamScreenshot();

REGISTER_PANEL2D_FACTORY(CCSGO_HudUniqueAlerts, CSGOHudAlerts);

enum HudAlertType_t
{
	HUD_ALERT_BASIC,
	HUD_ALERT_FLASHING,
	HUD_ALERT_FLASH_ANIM, // Play animation to flash up the alert, display for a few seconds, then flash and fade out
};

using namespace panorama;

CCSGO_HudUniqueAlerts::CCSGO_HudUniqueAlerts(CPanel2D *pParent, const char *pchID)
	: CPanoramaHudElement( "CCSGO_HudUniqueAlerts", this )
	, panorama::CPanel2D( pParent, pchID )
	, m_bVisible( false )
	, m_bShowedFirstMsg( false )
	, m_flNextWarmupNoticeTick( -1 )
	, m_bAlertStripVisible( false )
	, m_bWaitingForResume( false )
{		
	SetHiddenBits( HIDEHUD_PLAYERDEAD | HIDEHUD_MISCSTATUS );
	SetInputNamespace( "csgo_hudalerts" );

	DbgVerify(BLoadLayout("file://{resources}/layout/hud/hudalerts.xml"));
	m_pAlertText = panorama::panel_cast< CLabel * >( FindChildInLayoutFile( "AlertText" ) );

	ListenForGameEvent("game_newmap");
	ListenForGameEvent("player_spawn");
	ListenForGameEvent("round_start");
	ListenForGameEvent("round_announce_final");
	ListenForGameEvent("round_announce_match_point");
	ListenForGameEvent("round_announce_last_round_half");
	ListenForGameEvent("round_announce_match_start");
	ListenForGameEvent("round_announce_warmup");

	RegisterEventHandler(AnimationEnd(), this, &CCSGO_HudUniqueAlerts::EventAnimationEnd);
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CCSGO_HudUniqueAlerts::~CCSGO_HudUniqueAlerts()
{
}

void CCSGO_HudUniqueAlerts::ProcessInput()
{
	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pPlayer || !CSGameRules() )
		return;

	// get the round restart time first
	float flEndTime = CSGameRules()->GetRoundRestartTime() - 0.5f;
	bool bIsRestarting = CSGameRules()->IsGameRestarting();

	if (CSGameRules()->IsWarmupPeriod() && !m_bVisible)
	{
		m_bShowedFirstMsg = true;
		ShowPanel( true );
	}

	if (CSGameRules()->IsFreezePeriod() && (CSGameRules()->IsTimeOutActive()) && !m_bVisible)
	{
		m_bShowedFirstMsg = true;
		ShowPanel( true );
	}

	if (CSGameRules()->IsFreezePeriod() && (CSGameRules()->IsMatchWaitingForResume()) && !m_bVisible)
	{
		m_bShowedFirstMsg = true;
		ShowPanel( true );
	}

	if (m_bVisible && m_flNextWarmupNoticeTick <= gpGlobals->curtime)
	{
		m_flNextWarmupNoticeTick = gpGlobals->curtime + 1;

		if ( bIsRestarting )
		{
			int nLeft = (int)flEndTime - (int)gpGlobals->curtime;
			if ( nLeft >= 0 )
			{
				char szSecs[16] = "";
				V_snprintf(szSecs, 16, "%d", nLeft);
				SetDialogVariable("time_remaining", szSecs);

				if ( CSGameRules()->IsWarmupPeriod() )
				{
					if ( nLeft == 0 )
						ShowAlert("#CSGO_Notice_Alert_Match_Starting", HUD_ALERT_FLASHING);
					else
						ShowAlert("#CSGO_Notice_Alert_Match_Starting_In", HUD_ALERT_FLASHING);
				}
				else
				{
					if ( nLeft == 0 )
						ShowAlert("#CSGO_Notice_Alert_Match_Restarting", HUD_ALERT_FLASHING);
					else
						ShowAlert("#CSGO_Notice_Alert_Match_Restarting_In", HUD_ALERT_FLASHING);
				}
			}
		}
		else if ( CSGameRules()->IsWarmupPeriod() )
		{
			ShowWarmupAlertPanel();
		}
		else if ( CSGameRules()->IsFreezePeriod() )// we're paused
		{
			if ( CSGameRules()->IsTimeOutActive() )
			{
				m_bWaitingForResume = true;

				int nTimeLeftInSec;
				const char *szTeam;
				int nTimeOut;

				if ( CSGameRules()->IsTerroristTimeOutActive() )
				{
					nTimeLeftInSec = ( int )CSGameRules()->GetTerroristTimeOutRemaining();
					szTeam = g_pVGuiLocalize->FindAsUTF8("#CSGOEcon_SelectTerrorist");
					nTimeOut = CSGameRules()->GetTerroristTimeOuts();

				}
				else if ( CSGameRules()->IsCTTimeOutActive() )
				{
					nTimeLeftInSec = ( int )CSGameRules()->GetCTTimeOutRemaining();
					szTeam = g_pVGuiLocalize->FindAsUTF8("#CSGOEcon_SelectCT");
					nTimeOut = CSGameRules()->GetCTTimeOuts();
				}
				else
				{
					return;
				}

				static ConVarRef mp_team_timeout_max("mp_team_timeout_max");

				SetDialogVariable("team_name", szTeam);
				SetDialogVariable( "timeouts_max", mp_team_timeout_max.GetInt() );
				SetDialogVariable( "timeouts_remaining", mp_team_timeout_max.GetInt() - nTimeOut );

				if ( nTimeLeftInSec > 0 )
				{
					int nMinLeft = nTimeLeftInSec / 60;
					int nSecLeft = nTimeLeftInSec - ( nMinLeft * 60 );

					char szTime[ 8 ] = "";

					V_snprintf( szTime, ARRAYSIZE( szTime ), "%d:%02d", nMinLeft, nSecLeft );
					SetDialogVariable( "time_remaining", szTime );

					if ( ( mp_team_timeout_max.GetInt() > 1 ) && ( mp_team_timeout_max.GetInt() < 100 ) )
					{
						ShowAlert( "#CSGO_Notice_Alert_Timeout_Multi" );
					}
					else
					{
						ShowAlert( "#CSGO_Notice_Alert_Timeout" );
					}
				}
			}
			else if ( CSGameRules()->IsMatchWaitingForResume() )
			{
				m_bWaitingForResume = true;

				ShowAlert("#SFUI_Notice_Alert_Freeze_Pause");
			}
			else if ( m_bWaitingForResume )
			{
				m_bWaitingForResume = false;
				ShowPanel( false );
			}
			else
			{
				// CSGO-1684: This bug is in public w scaleform as well... 
				// Since the animations used to flash the panel will hide when completed it seems safe to just leave
				// it up during freezetime rather than spam hide. Alternatively we could prevent hide from canceling the 
				// flash animations but this is a simpler change and is probably fine.  
				// ShowPanel( false );
			}
		}
		else
		{
			ShowPanel( false );
		}
	}
}

void CCSGO_HudUniqueAlerts::OnTimeJump()
{
	m_flNextWarmupNoticeTick = -1; // refresh notices on next Think
}

bool CCSGO_HudUniqueAlerts::ShouldDraw( void )
{
	bool bTrainingMode = CSGameRules() && CSGameRules()->IsPlayingTraining();

	return
		cl_drawhud.GetBool()
		&& cl_draw_only_deathnotices.GetBool() == false
		&& !IsTakingAFreezecamScreenshot()
		&& !bTrainingMode
		&& CHudElement::ShouldDraw();
}

void CCSGO_HudUniqueAlerts::SetActive( bool bActive )
{
	if( bActive != BIsVisible() )
	{
		if( bActive )
		{
			Show();
		}
		else
		{
			Hide();
		}
	}

	CHudElement::SetActive( bActive );
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CCSGO_HudUniqueAlerts::FireGameEvent(IGameEvent *pEvent)
{
	const char *type = pEvent->GetName();

	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pLocalPlayer )
		return;

	int EventUserID = pEvent->GetInt("userid", -1);
	int LocalPlayerID = ( pLocalPlayer != NULL ) ? pLocalPlayer->GetUserID() : -2;

	if (Q_strcmp("game_newmap", type) == 0) // Equivalent of SFUniqueAlerts::LevelInit()
	{
		Hide();
		m_flNextWarmupNoticeTick = -1;
	}
	else if (Q_strcmp("round_announce_match_start", type) == 0)
	{
		m_bShowedFirstMsg = true;
		ShowAlert("#CSGO_Notice_Alert_Match_Start", HUD_ALERT_FLASH_ANIM);
		
		C_RecipientFilter filter;
		filter.AddRecipient( pLocalPlayer );
		C_BaseEntity::EmitSound( filter, SOUND_FROM_WORLD, "Music.Match_Start_Stinger" );
	}
	else if (Q_strcmp("round_start", type) == 0)
	{
		if ( m_bAlertStripVisible && pLocalPlayer->IsHLTV() )
			Hide();	

		m_flNextWarmupNoticeTick = -1;
	}
	else if (Q_strcmp("round_announce_final", type) == 0 ||
		Q_strcmp("round_announce_last_round_half", type) == 0 ||
		Q_strcmp("round_announce_match_point", type) == 0 ||
		Q_strcmp("player_spawn", type) == 0)
	{
		if (Q_strcmp("round_announce_final", type) == 0)
		{
			ShowPanel( true );
			m_bShowedFirstMsg = true;

			ShowAlert("#SFUI_Notice_Alert_Final_Round", HUD_ALERT_FLASH_ANIM);

			C_RecipientFilter filter;
			filter.AddRecipient( pLocalPlayer );
			C_BaseEntity::EmitSound( filter, SOUND_FROM_WORLD, "Music.Final_Round_Stinger" );
		}
		else if (Q_strcmp("round_announce_match_point", type) == 0)
		{
			ShowPanel( true );
			m_bShowedFirstMsg = true;

			ShowAlert("#SFUI_Notice_Alert_Match_Point", HUD_ALERT_FLASH_ANIM);

			C_RecipientFilter filter;
			filter.AddRecipient( pLocalPlayer );
			C_BaseEntity::EmitSound( filter, SOUND_FROM_WORLD, "Music.Match_Point_Stinger" );
		}
		else if (Q_strcmp("round_announce_last_round_half", type) == 0)
		{
			ShowPanel( true );
			m_bShowedFirstMsg = true;

			ShowAlert("#SFUI_Notice_Alert_Last_Round_Half", HUD_ALERT_FLASH_ANIM);
		}
		else if (Q_strcmp("player_spawn", type) == 0 && EventUserID == LocalPlayerID)
		{
			if ( m_bAlertStripVisible )
				Hide();	
		}
	}
	else if (Q_strcmp("round_announce_warmup", type) == 0)
	{
		m_bShowedFirstMsg = true;
		SetAlertStripVisible( false );
	}
}

void CCSGO_HudUniqueAlerts::Show(void)
{
	if ( !m_bShowedFirstMsg )
		return;

	m_bVisible = true;
}

void CCSGO_HudUniqueAlerts::SetAlertStripVisible(bool bVisible)
{
	static const CPanoramaSymbol k_symHideFlash( "HideFlash" );

	if ( m_bAlertStripVisible && !bVisible )
	{
		SwitchClass( "alertstate", k_symHideFlash );
	}
	m_bAlertStripVisible = bVisible;
}

bool CCSGO_HudUniqueAlerts::EventAnimationEnd( const panorama::CPanelPtr< panorama::IUIPanel > &panelPtr, CPanoramaSymbol symAnimation )
{
	static const CPanoramaSymbol k_symHideFlash( "HideFlash" );
	static const CPanoramaSymbol k_symFlashAnim( "FlashAnim" );
	static const CPanoramaSymbol k_symAlertHidden( "AlertHidden" );

	if ( ( symAnimation == k_symHideFlash ) || ( symAnimation == k_symFlashAnim ) )
	{
		// End of a fade out animation, panel no longer visible
		m_bAlertStripVisible = false;
		Hide();
		SwitchClass( "alertstate", k_symAlertHidden );
	}
	return true;
}

void CCSGO_HudUniqueAlerts::Hide( void )
{
	m_bVisible = false;

	SetAlertStripVisible( false );

	//Msg( "------------------------ SFUniqueAlerts::Hide\n" ); 
}

void CCSGO_HudUniqueAlerts::ShowPanel(const bool bShow)
{
	if ( bShow )
	{
		Show();
	}
	else
	{
		Hide();
	}
}

//-----------------------------------------------------------------------------
void CCSGO_HudUniqueAlerts::ShowAlert(char* szAlertText, int nAlertType /*=0*/)
{
	static const CPanoramaSymbol k_symFlashAnim("FlashAnim");
	static const CPanoramaSymbol k_symAlertVisible("AlertVisible");
	static const CPanoramaSymbol k_symAlertHidden("AlertHidden");

	SetAlertStripVisible(true);

	if (m_pAlertText)
	{
		m_pAlertText->SetTextWithDialogVariables(szAlertText, CLabel::k_ETextTypeHTML);

		switch (nAlertType)
		{
		case HUD_ALERT_FLASHING:
			// Remove then add class to reset animation to run from beginning
			SwitchClass("alertstate", k_symAlertHidden);
			SwitchClass("alertstate", k_symAlertVisible);
			break;
		case HUD_ALERT_FLASH_ANIM:
			SwitchClass("alertstate", k_symFlashAnim);
			break;
		default:
			SwitchClass("alertstate", k_symAlertVisible);
			break;
		}
	}
}

void CCSGO_HudUniqueAlerts::ShowWarmupAlertPanel(void)
{
	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
	if (!pPlayer || !CSGameRules())
		return;

	if ( !m_bVisible )
	{
		//DevMsg( "SFUniqueAlerts:ShowWarmupAlertPanel : RETURNING : m_bVisible == %s, m_bActive == %s\n", m_bVisible ? "true" : "false", m_bActive ? "true" : "false" );
		return;
	}

	float flEndTime = CSGameRules()->GetWarmupPeriodEndTime() - 0.5f;
	bool bIsRestarting = CSGameRules()->IsGameRestarting();

	int nTimeLeftInSec = (int)flEndTime - (int)gpGlobals->curtime;
	if ( nTimeLeftInSec > 0 )
	{
		int nMinLeft = nTimeLeftInSec / 60;
		int nSecLeft = nTimeLeftInSec - ( nMinLeft * 60 ); 

		char szTime[8] = "";
			
		if ( !CSGameRules()->IsWarmupPeriodPaused() )
		{
			V_snprintf(szTime, 8, "%d:%02d", nMinLeft, nSecLeft);
		}
		SetDialogVariable("time_remaining", szTime);

		if ( nTimeLeftInSec <= 5 )
		{
			// g_pVGuiLocalize->ConstructString( szNotice, sizeof( szNotice ), g_pVGuiLocalize->Find( "#SFUI_Notice_Alert_Warmup_Period_Ending" ), 1, wzTime );
			//V_snprintf(szNotice, 64, "%s %s\n", "#SFUI_Notice_Alert_Warmup_Period_Ending", wzTime);
			ShowAlert("#CSGO_Notice_Alert_Warmup_Period_Ending", HUD_ALERT_FLASHING);

			if ( !bIsRestarting )
			{
				pPlayer->EmitSound("Alert.WarmupTimeoutBeep");
			}
		}
		else
		{
			if ( CSGameRules()->IsQueuedMatchmaking() )
			{
				// client-side UTIL_HumansInGame
				int nTotalPlayers = 0;
				for ( int i = 1; i <= gpGlobals->maxClients; i++ )
				{
					pPlayer = ( CCSPlayer* )UTIL_PlayerByIndex( i );
					if ( pPlayer && pPlayer->GetTeamNumber() > TEAM_SPECTATOR && !pPlayer->IsBot() )
						nTotalPlayers++;
				}

				int numHumansNeeded = CCSGameRules::GetMaxPlayers();
				if ( CSGameRules()->IsPlayingCoopGuardian() )
					numHumansNeeded = 2;

				if ( nTotalPlayers < numHumansNeeded )
				{
					ShowAlert("#CSGO_Notice_Alert_Waiting_For_Players");
				}
				else
				{
					ShowAlert("#CSGO_Notice_Alert_Warmup_Period");
				}
			}
			else
			{
				ShowAlert("#CSGO_Notice_Alert_Warmup_Period");
			}
		}
	}
}

