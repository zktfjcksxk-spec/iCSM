//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Display active Deathmatch bonus weapon -- split from sfhud_uniquealerts.cpp
//
//=============================================================================//

#include "cbase.h"
#include "csgo_huddmbonuspanel.h"
#include "cs_gamerules.h"
#include "c_cs_player.h"
#include "vguicenterprint.h"

REGISTER_PANEL2D_FACTORY( CCSGO_HudDMBonusPanel, CSGOHudDMBonusPanel );

extern ConVar mp_dm_bonus_percent;
extern ConVar mp_dm_dogtag_score;
extern ConVar mp_dm_bonusweapon_dogtags;
extern ConVar mp_dm_teammode_bonus_score;
extern ConVar mp_dm_bonus_respawn;
extern ConVar mp_dm_teammode_kill_score;

static const float kBonusPanel_TimeToDisplayBonusKill = 2.0f;

CCSGO_HudDMBonusPanel::CCSGO_HudDMBonusPanel( panorama::CPanel2D *pParent, const char *pchID )
	: BaseClass(pParent, pchID)
	, CPanoramaHudElement( "CCSGO_HudDMBonusPanel", this )
{
	// Get our layout
	RequireLoadLayout( "file://{resources}/layout/hud/huddmbonus.xml" );

	// Get references to fields we care about in layout
	m_pImageIcon = panorama::panel_cast< panorama::CImagePanel* >( FindChildTraverse( "Icon" ) );
	m_pPanelTimerBar = FindChildTraverse( "TimerBar" );
	m_pLabelPoints = panorama::panel_cast< panorama::CLabel* >( FindChildTraverse( "Points" ) );
	m_pPanelKillPoints = FindChildTraverse( "PointsAnim" );
	m_pLabelKillPoints = panorama::panel_cast< panorama::CLabel* >( FindChildTraverse( "KillPoints" ) );

	// Hide (panel should start hidden automatically by xml)
	m_bActive = false;

	// Update on 1st Think()
	ResetUpdateTime();

	// bonus kill panel is not displayed by default
	m_bBonusKillEnabled = false;

	// Default values for dialog variables
	SetDialogVariableLocString( "dm-bonus-weapon", "#SFUI_WPNHUD_AK47" );
	SetDialogVariable( "dm-bonus-time", "0" );
	SetDialogVariable( "dm-bonus-points", 5 );
	if ( m_pPanelKillPoints )
		m_pPanelKillPoints->SetDialogVariable( "dm-bonus-points", 16 );

	ListenForGameEvent( "gg_killed_enemy" );
}

CCSGO_HudDMBonusPanel::~CCSGO_HudDMBonusPanel() = default;

bool CCSGO_HudDMBonusPanel::ShowDmBonusPanel()
{
	if ( !CSGameRules() )
		return false;

	SetHasClass( "bonuspanel--deathmatch", CSGameRules()->IsPlayingGunGameDeathmatch() );

	if ( !CSGameRules()->IsPlayingGunGameDeathmatch() )
		return false;

	float fStartTime = CSGameRules()->GetDMBonusStartTime();
	if ( gpGlobals->curtime < fStartTime )
	{
		SetUpdateTime( fStartTime );
		return false;
	}

	float fEndTime = fStartTime + CSGameRules()->GetDMBonusTimeLength();
	if ( gpGlobals->curtime > fEndTime )
		return false;

	loadout_positions_t nPos = CSGameRules()->GetDMBonusWeaponLoadoutSlot();
	if ( nPos == LOADOUT_POSITION_INVALID )
		return false;

	// Figure out the item in the local player's loadout is in this slot
	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pLocalPlayer )
		return false;

	CCSPlayerInventory* pPlayerInv = CSInventoryManager()->GetLocalCSInventory();
	if ( !pPlayerInv )
		return false;

	const CEconItemView* pItem = pPlayerInv->GetItemInLoadout( C_CSPlayer::GetLocalCSPlayer()->GetTeamNumber(), nPos );
	if ( !pItem || !pItem->IsValid() )
		return false;

	// Calculate points to award for bonus and what messaging to show the player
	int nPoints, nBonusPoints;
	const char* szSingularMessage;
	const char* szPluralMessage;

	if ( mp_dm_bonusweapon_dogtags.GetInt() > 0 )
	{
		nPoints = 0;
		nBonusPoints = mp_dm_bonusweapon_dogtags.GetInt();
		szSingularMessage = "#Panorama_DMBonusPanel_KillAward_Dogtag";
		szPluralMessage = "#Panorama_DMBonusPanel_KillAward_Dogtags";
	}
	else if ( CSGameRules()->IsPlayingTeamDM() && mp_dm_teammode_bonus_score.GetInt() > 0 )
	{
		nPoints = mp_dm_teammode_kill_score.GetInt();
		nBonusPoints = mp_dm_teammode_bonus_score.GetInt();
		szSingularMessage = "#Panorama_DMBonusPanel_KillAward_Team";
		szPluralMessage = "#Panorama_DMBonusPanel_KillAward_Teams";
	}
	else
	{
		nPoints = CSGameRules()->GetWeaponScoreForDeathmatch( nPos, &nBonusPoints );
		szSingularMessage = "#Panorama_DMBonusPanel_KillAward_Point";
		szPluralMessage = "#Panorama_DMBonusPanel_KillAward_Points";
	}

	const char* bonusMessage = ( nBonusPoints == 1 ) ? szSingularMessage : szPluralMessage;
	const char* scoreMessage = ( ( nPoints + nBonusPoints ) == 1 ) ? szSingularMessage : szPluralMessage;

	const CEconItemDefinition* pWeaponDef = pItem->GetStaticData();
	const char* szWeaponLocName = pWeaponDef->GetItemBaseName();
	const char* szWeaponIconName = pWeaponDef->GetDefinitionName();
	const char* skip_ = strchr( szWeaponIconName, '_' );
	if ( skip_ )
		szWeaponIconName = skip_ + 1;
	
	SetDialogVariableLocString( "dm-bonus-weapon", szWeaponLocName );
	SetDialogVariable( "dm-bonus-points", nBonusPoints );
	if ( m_pPanelKillPoints )
		m_pPanelKillPoints->SetDialogVariable( "dm-bonus-points", nPoints + nBonusPoints );

	if ( m_pImageIcon )
	{
		m_pImageIcon->SetImageJS( CFmtStr( "file://{images}/icons/equipment/%s.svg", szWeaponIconName ) );
	}

	if ( m_pLabelPoints )
		m_pLabelPoints->SetText( bonusMessage );

	if ( m_pLabelKillPoints )
		m_pLabelKillPoints->SetText( scoreMessage );

	// If we already have the bonus weapon, we can switch to it instead of respawning
	CBaseCombatWeapon *pOwnedWeapon = pLocalPlayer->Weapon_GetPosition( nPos );
	if ( pOwnedWeapon || pLocalPlayer->CanPlayerBuy( false ) || !mp_dm_bonus_respawn.GetBool() )
		GetCenterPrint()->Print( "#Panorama_Notice_DM_BonusSwitchTo" );
	else
		GetCenterPrint()->Print( "#Panorama_Notice_DM_BonusRespawn" );

	m_nActivePosition = nPos;
	return true;
}

bool CCSGO_HudDMBonusPanel::UpdateDmBonusPanel()
{
	if ( !CSGameRules() )
		return false;

	float fStartTime = CSGameRules()->GetDMBonusStartTime();
	if ( gpGlobals->curtime < fStartTime )
		return false;

	float fEndTime = fStartTime + CSGameRules()->GetDMBonusTimeLength();
	if ( gpGlobals->curtime > fEndTime )
		return false;

	// Check if something weird happened behind our back and we need to re-initialize icon, points, etc.
	if ( m_nActivePosition != CSGameRules()->GetDMBonusWeaponLoadoutSlot() )
	{
		bool shouldShow = ShowDmBonusPanel();
		Assert( shouldShow );
		if ( !shouldShow )
			return false;
	}

	// Update timer text
	int nSecondsLeft = ( int )( fEndTime - gpGlobals->curtime );
	SetDialogVariable( "dm-bonus-timer", CFmtStr( "%d", nSecondsLeft ) );

	// Handle bonus score animation
	if( m_bBonusKillEnabled )
	{
		if ( gpGlobals->curtime > m_fBonusKillClearTime )
			SetBonusKillEnabled( false );
		else
			SetUpdateTime( m_fBonusKillClearTime );
	}

	// Update timer bar with current time
	if ( m_pPanelTimerBar )
	{
		float fDuration = fEndTime - fStartTime;
		float fOffset = gpGlobals->curtime - fStartTime;

		float flTimeScale = engine->GetTimescale();
		if ( flTimeScale < 0.0001f )
		{
			// "pause" by running very slowly; we'll reset on next think
			flTimeScale = 0.0001f;
		}

		fDuration /= flTimeScale;
		fOffset /= flTimeScale;

		// For some reason CPanel2D has BSetProperty protected.  End-around by upcasting to the interface where it's public
		IUIPanelClient* pTimerPanel = m_pPanelTimerBar;
		pTimerPanel->BSetProperty( "style", CFmtStr( "animation-duration: %.2fs; animation-delay: %.2fs;", fDuration, -fOffset ) );
	}

	// Set next update  to exactly when will update to the next second
	// example:
	//		curtime = 10.5
	//		fEndTime = 13.2
	//		fEndtime - curtime = 2.7
	//		nSecondsleft = 2
	//		fEndTime - nSecondsLeft = 11.2
	// (Then the next update happens at curtime > 11.2)
	//		curtime = just over 11.2
	//      fEndTime - curtime = just under 2
	//      nSecondsLeft = 1 -> change text to 00:01
	SetUpdateTime( fEndTime - nSecondsLeft );

	return true;
}

void CCSGO_HudDMBonusPanel::FireGameEvent( IGameEvent *event )
{
	CPanoramaHudElement::FireGameEvent( event );

	if ( !m_bActive )
		return;

	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pLocalPlayer )
		return;

	const char *type = event->GetName();

	// Bonus weapon kill
	if ( !V_strcmp( "gg_killed_enemy", type )
		&& CSGameRules()->IsPlayingGunGame()
		&& event->GetInt( "bonus" )
		&& pLocalPlayer->GetUserID() == event->GetInt( "attackerid" )
		)
	{
		m_fBonusKillClearTime = gpGlobals->curtime + kBonusPanel_TimeToDisplayBonusKill;
		SetUpdateTime( m_fBonusKillClearTime );
		SetBonusKillEnabled( true );
	}
}

void CCSGO_HudDMBonusPanel::Think()
{
	static const panorama::CPanoramaSymbol k_symDMBonusActive{ "bonuspanel--active" };

	float updateTime = m_fNextUpdateTime;
	if ( gpGlobals->curtime <= updateTime )
		return;

	// Update at least 4 times per second; more if an event happens in the intervening time
	// Parts of this 'think' function might set the next update time to sometime in the past; that just means
	// that we will update again instantly on the next frame.
	m_fNextUpdateTime = updateTime + 0.25f;

	// Check if we need to show
	if ( !m_bActive && ShowDmBonusPanel() )
	{
		m_bActive = true;
		SetHasClass( k_symDMBonusActive, true );
	}

	// Check if we need to hide
	if ( m_bActive && !UpdateDmBonusPanel() )
	{
		m_bActive = false;
		SetHasClass( k_symDMBonusActive, false );
		SetBonusKillEnabled( false );
	}
}

void CCSGO_HudDMBonusPanel::SetBonusKillEnabled( bool bSet )
{
	static const panorama::CPanoramaSymbol kSymBonusPanelKill( "bonuspanel--bonuskill" );

	if ( bSet != m_bBonusKillEnabled )
	{
		m_bBonusKillEnabled = bSet;

		if ( bSet )
			TriggerClass( kSymBonusPanelKill );
		else
			RemoveClass( kSymBonusPanelKill );
	}
}

void CCSGO_HudDMBonusPanel::OnTimeTravel()
{
	// Hide window
	static const panorama::CPanoramaSymbol k_symDMBonusActive{ "bonuspanel--active" };
	if ( m_bActive )
	{
		m_bActive = false;
		SetHasClass( k_symDMBonusActive, false );
		SetBonusKillEnabled( false );
	}

	// Update instantly on next tick
	ResetUpdateTime();
}
