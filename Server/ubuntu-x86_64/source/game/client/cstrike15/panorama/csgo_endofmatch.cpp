//========= Copyright (C) Valve Corporation, All rights reserved. ============//
//
// Component for EndOfMatch access. Only used by panorama and mirrors the EndOfMatch_scaleform singleton in SF.
//
//=============================================================================//


#include "cbase.h"

#include "csgo_endofmatch.h"

#include "c_cs_player.h"
#include "cs_gamerules.h"
#include "c_cs_playerresource.h"
#include "weapon_selection.h"
#include "IGameUIFuncs.h"
#include "inputsystem/iinputsystem.h"
#include "econ/econ_item_description.h"
#include "gametypes.h"
#include "panorama/controls/contextmenu.h"
#include "panorama/csgo_scoreboard.h"
#include "panorama/hud/csgo_hudvoicestatus.h"
#include "panorama/hud/csgo_hudhealtharmor.h"
#include "panorama/hud/csgo_hudchat.h"
#include "panorama/csgo_survival_endofmatch.h"
#include "gameui_hudinterfaces.h"
#include "hltvreplaysystem.h"

#include "uicomponents/uicomponent_gamestate.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

DEFINE_PANORAMA_EVENT( EndOfMatch_Show );
DEFINE_PANORAMA_EVENT( EndOfMatch_Shutdown );
DEFINE_PANORAMA_EVENT( EndOfMatch_Survival_Spectate_Clicked );

DEFINE_PANORAMA_EVENT( Scoreboard_OnEndOfMatch );

DECLARE_PANORAMA_EVENT0( EndOfMatch_Survival_Death );
DEFINE_PANORAMA_EVENT( EndOfMatch_Survival_Death );

REGISTER_PANEL2D_FACTORY( CCSGO_EndOfMatch, CSGOEndOfMatch )

CCSGO_EndOfMatch *CCSGO_EndOfMatch::s_pEndOfMatch = NULL;

CCSGO_EndOfMatch::CCSGO_EndOfMatch( CPanel2D *pParent, const char *pchID )
	: panorama::CPanel2D( pParent, pchID )
	, m_bOpen( false )
	, m_bMatchOver( false )
	, m_bInReplay( false )
	, m_bSurvivalSpectateMode( false )
	, m_Capture( this, "EndOfMatch", panorama::k_EGameInputShareMouse | panorama::k_EGameInputUIEnableKeyInput, false )
{
	
	Assert( s_pEndOfMatch == NULL );
	s_pEndOfMatch = this;

	// Make sure EndOfMatch panel has its own input hierarchy and therefore will not lose focus
	// when pushing another input context (peer panels such as scoreboard)
	SetTopOfInputContext( true );
	
	SetInputNamespace( "EndOfMatch" );

	SetAcceptsInput( true );
	SetAcceptsFocus( true );

	RequireLoadLayout( "file://{resources}/layout/endofmatch.xml" );

	SetVisible( false );

	ListenForGameEvent( "cs_win_panel_match" );
	ListenForGameEvent( "cs_game_disconnected" );
	ListenForGameEvent( "cs_match_end_restart" );
	ListenForGameEvent( "cs_intermission" );
	ListenForGameEvent( "nextlevel_changed" );
	ListenForGameEvent( "hltv_replay" );
	ListenForGameEvent( "begin_new_match" );
	ListenForGameEvent( "player_death" );

	RegisterEventHandler( EndOfMatch_Survival_Spectate_Clicked(), this, &CCSGO_EndOfMatch::OnEndOfMatchSurvivalSpectateClicked );
	RegisterEventHandler( EndOfMatch_Survival_Death(), this, &CCSGO_EndOfMatch::OnEndOfMatchSurvivalDeath );
}

CCSGO_EndOfMatch::~CCSGO_EndOfMatch()
{
	SetMouseCapture( false );

	Assert( s_pEndOfMatch == this );
	s_pEndOfMatch = NULL;
}

bool CCSGO_EndOfMatch::OnEndOfMatchSurvivalSpectateClicked()
{
	// Pretend match is not over and just let the user spectate normally.
	m_bSurvivalSpectateMode = true;
	UpdateVisibleState();
	return true;
}

bool CCSGO_EndOfMatch::OnEndOfMatchSurvivalDeath()
{
	m_bMatchOver = true;
	UpdateVisibleState();
	return true;
}

void CCSGO_EndOfMatch::UpdateVisibleState( bool bHardCut )
{
	if ( m_bMatchOver && !m_bInReplay && !m_bSurvivalSpectateMode )
		OpenEndOfMatch( bHardCut );
	else
		CloseEndOfMatch();
}

void CCSGO_EndOfMatch::SetupJavascriptObjectTemplate()
{
	BaseClass::SetupJavascriptObjectTemplate();

	panorama::RegisterJSMethod( "CloseEndOfMatch", PANORAMA_DELEGATE( &CCSGO_EndOfMatch::CloseEndOfMatch ) );
	panorama::RegisterJSMethod( "SetMouseCapture", PANORAMA_DELEGATE( &CCSGO_EndOfMatch::SetMouseCapture ) );
	//panorama::RegisterJSMethod( "LatchPlayerData", PANORAMA_DELEGATE( &CCSGO_EndOfMatch::LatchPlayerData ) );

}

void CCSGO_EndOfMatch::LevelShutdownPreEntity()
{
	CloseEndOfMatch();
}

// void CCSGO_EndOfMatch::LatchPlayerData( void )
// {
// 	CUiComponent_GameState::GetInstance()->Latch();
// }

void CCSGO_EndOfMatch::SetMouseCapture( bool bVal)
{
	if ( bVal )
	{
		DevAssertMsg( !m_Capture.BEnabled(), "EndOfMatch - Make sure to call PanoramaReleaseDenyAllInputToGame before calling PanoramaAddDenyAllInputToGame again\n" );
		m_Capture.Enable();
	}
	else
	{
		m_Capture.Disable();
	}
}

void CCSGO_EndOfMatch::FireGameEvent( IGameEvent *event )
{

	if ( !CSGameRules() )
		return;

	const char *type = event->GetName();

	if ( !V_strcmp( type, "cs_win_panel_match" ) ||
		!V_strcmp( type, "cs_intermission" ) )
	{
		m_bMatchOver = true;
		m_bSurvivalSpectateMode = false; // TODO: add some way to not re-do the whole flow when we already saw it during survival
		UpdateVisibleState();
	}
	else if ( !V_strcmp( type, "player_death" ) )
	{
		// In survival mode, when the local player dies, transition to end-of-match state automatically
		// unless replay feature is enabled
		if ( CSGameRules()->IsPlayingSurvival() &&
			 CSGameRules()->GetSurvivalRules() &&
			 CSGameRules()->GetSurvivalRules()->IsPlayingSoloMode() && // kludge: not supporting community servers that play team mode survival for now
			( !g_HltvReplaySystem.IsHltvReplayFeatureEnabled() || event->GetBool( "noreplay" ) ) )
		{
			int victim = engine->GetPlayerForUserID( event->GetInt( "userid" ) );
			if ( victim && victim == GetLocalPlayerIndex() && !CSGameRules()->IsWarmupPeriod() )
			{
				// end match in a few seconds
				//
				// this gives time for the hltv replay to start as well as make sure we get responses
				// from the GC for xp / drops earned.
				DispatchEventAsync( 3.0f, EndOfMatch_Survival_Death(), this );
			}
		}
	}
	else if ( !V_strcmp( type, "cs_game_disconnected" ) ||
		!V_strcmp( type, "cs_match_end_restart" ) ||
		!V_strcmp( type, "nextlevel_changed" ) )
	{
		m_bMatchOver = false;
		m_bInReplay = false; // don't leak replay state to next level
		m_bSurvivalSpectateMode = false;
		UpdateVisibleState();
	}
	else if ( !V_strcmp( type, "hltv_replay" ) )
	{
		// If our hltv delay becomes nonzero, we must be in a killer replay.
		// Make sure we don't switch to end-of-match state while the
		// replay is active.
		int numSecondsInKillerReplayDelay = event->GetInt( "delay" );
		m_bInReplay = ( numSecondsInKillerReplayDelay != 0 );

		// When killer replay finishes, if the match is over, hard-cut to it
		// instead of a slow transition.
		UpdateVisibleState();

		//
		// Survival has custom mode where replay is kicked off always when you die
		//
		// In survival mode, when the local player dies, transition to end-of-match state
		// (because replay is enabled it will actually do it at the end of killer replay)
		if ( CSGameRules()->IsPlayingSurvival() && g_HltvReplaySystem.IsHltvReplayFeatureEnabled() && m_bInReplay )
		{
			// DispatchEventAsync( numSecondsInKillerReplayDelay + 2.0f, EndOfMatch_Survival_Death(), this );
			OnEndOfMatchSurvivalDeath();
		}
	}
}

void CCSGO_EndOfMatch::CloseEndOfMatch( void )
{
	if ( !m_bOpen )
		return;

	m_bOpen = false;

	// don't latch gamestate any more ? $$$REI this is private but latch isn't?
	// CUiComponent_GameState::GetInstance()->Unlatch();

	panorama::DispatchEvent( DismissAllContextMenus(), nullptr );
	panorama::DispatchEvent( EndOfMatch_Shutdown(), nullptr );

	CCSGO_HudVoiceStatus* pHudVoiceStatus = (CCSGO_HudVoiceStatus *)GetHud().FindElement( "CCSGO_HudVoiceStatus" );
	CCSGO_HudHealthArmor* pHudHealthArmor = (CCSGO_HudHealthArmor *)GetHud().FindElement( "CCSGO_HudHealthArmor" );
	if( pHudVoiceStatus && pHudHealthArmor )
	{
		// Move HudVoiceStatus panel back to its normal position in the Hud 
		CPanel2D* pHudParent = pHudHealthArmor->GetParent();
		pHudVoiceStatus->SetParent( pHudParent );

		pHudParent->MoveChildBefore( pHudVoiceStatus, pHudHealthArmor );
	}

	static const panorama::CPanoramaSymbol k_symEndOfMatchChat( "EndOfMatchChat" );
	CCSGO_HudChat* pHudChat = (CCSGO_HudChat*)GetHud().FindElement( "CCSGO_HudChat" );
	pHudChat->RemoveClass( k_symEndOfMatchChat );

	if ( CCSGO_SurvivalEndOfMatch* pSurvivalEndOfMatch = m_pSurvivalEndOfMatch.Get() )
	{
		m_pSurvivalEndOfMatch = nullptr;
		delete pSurvivalEndOfMatch;
	}

	// Move the scoreboard back to the Hud
	if ( CCSGO_Scoreboard* pScoreboard = m_pScoreboard.Get() )
	{
		if ( panorama::CPanel2D* pScoreboardHolder = m_pScoreboardHolder.Get() )
		{
			pScoreboard->SetParent( pScoreboardHolder );
			pScoreboard->CloseScoreboard();
		}
		else
		{
			Msg( "Scoreboard holder panel was destroyed while we were holding onto scoreboard?" );
		}

		m_pScoreboardHolder = nullptr;
		m_pScoreboard = nullptr;
	}

	SetVisible( false );

	// Removing chat panel from the input context stack
	GetParentWindow()->UIWindowInput()->RemoveInputContext( this->UIPanel() );

	SetMouseCapture( false );

	return;

}

DECLARE_PANORAMA_EVENT0( Scoreboard_OnMouseActive );

void CCSGO_EndOfMatch::OpenEndOfMatch( bool bHardCut )
{
	if ( m_bOpen )
		return;

	m_bOpen = true;

	// Latch scores
	CUiComponent_GameState::GetInstance()->Latch();

// 		We want chat during end of match
//	GetHud().DisableHud();

	SetVisible( true );

	// If the chat panel is up, do not switch input context (chat panel should have focus) as scoreboard
	// being opened is not in response to the player direct input (eg. scoreboard at half time)
	// (JIRA CSGO-1610)
	panorama::IUIPanel *pRestoreContext = nullptr;
	IHudChat* pHudChat = GetHudChat();
	if ( pHudChat && pHudChat->ChatRaised() )
	{
		pRestoreContext = GetParentWindow()->UIWindowInput()->GetInputFocusContext();
	}

	// EndOfMatch panel has its own input context (cf SetTopOfInputContext( true ) in constructor)
	// Therefore calling SetFocus will switch input context. Make sure to remove it from the stack
	// when hiding the EndOfMatch panel
	SetFocus();

	if ( pRestoreContext )
	{
		// Restore input context to the chat panel if necessary
		GetParentWindow()->UIWindowInput()->SetInputFocusContext( pRestoreContext );
	}

//	SetMouseCapture( true );

	// force close the scoreboard
	CCSGO_Scoreboard::GetInstance()->CloseScoreboard();

	// Move HudVoiceStatus panel to be part of endofmatch screen so it shows on top of blur
	CCSGO_HudVoiceStatus* pHudVoiceStatus = (CCSGO_HudVoiceStatus*)GetHud().FindElement( "CCSGO_HudVoiceStatus" );
	CPanel2D* pEomLayout = FindChildTraverse( "id-eom-chat-container" );
	if( !pEomLayout )
	{
		Msg( "Error locating panel id-eom-layout, unable to move HudVoiceStatus to endofmatch screen\n" );
	}
	if( pHudVoiceStatus && pEomLayout )
	{
		pHudVoiceStatus->SetParent( pEomLayout );
	}

	// Set chat panel class so styling can be tailored for endofmatch
	static const panorama::CPanoramaSymbol k_symEndOfMatchChat( "EndOfMatchChat" );
	CCSGO_HudChat* pPanHudChat = (CCSGO_HudChat*)GetHud().FindElement( "CCSGO_HudChat" );
	pPanHudChat->AddClass( k_symEndOfMatchChat );

	// Move the scoreboard into the End of Match screen
	CPanel2D* pEomScoreboard = RequireChildTraverse( "id-eom-scoreboard-container" );
	if ( CSGameRules()->IsPlayingSurvival() )
	{
		m_pSurvivalEndOfMatch = new CCSGO_SurvivalEndOfMatch( pEomScoreboard, "id-eom-survival-panel" );
	}
	else
	{
		CCSGO_Hud* pHud = CCSGO_Hud::GetInstance();
		CCSGO_Scoreboard* pScoreboard = panorama::panel_cast< CCSGO_Scoreboard* >( pHud->FindChildTraverse( "Scoreboard" ) );
		if ( pScoreboard )
		{
			m_pScoreboard = pScoreboard;
			m_pScoreboardHolder = pScoreboard->GetParent();
			pScoreboard->SetParent( pEomScoreboard );
			pScoreboard->OpenScoreboard();
			panorama::DispatchEvent( Scoreboard_OnEndOfMatch(), nullptr );
			// enable mouse interactivity on the scoreboard without enabling the mouse yet.
			// that will happen later.
			pScoreboard->SetHitTestChildrenEnabled( true );
			panorama::DispatchEvent( Scoreboard_OnMouseActive(), NULL );
		}
	}

	// Start end of match sequence
	panorama::DispatchEvent( EndOfMatch_Show(), NULL, bHardCut );
}


/////////////////////////////////////

REGISTER_PANEL2D_FACTORY( CCSGO_EndOfMatchItemDropsPanel, CSGOEndOfMatchItemDropsPanel )

DEFINE_PANORAMA_EVENT( ItemsDroppedDuringMatchReady );

// CCSGO_EndOfMatchItemDropsPanel *CCSGO_EndOfMatchItemDropsPanel::s_pItemDrops = NULL;


CCSGO_EndOfMatchItemDropsPanel::CCSGO_EndOfMatchItemDropsPanel( CPanel2D *pParent, const char *pchID )
	: panorama::CPanel2D( pParent, pchID )
	, m_pKVDropList( nullptr )
{
	RequireLoadLayout( "file://{resources}/layout/endofmatch-drops.xml" );

// 	Assert( s_pItemDrops == NULL );
// 	s_pItemDrops = this;
}

CCSGO_EndOfMatchItemDropsPanel::~CCSGO_EndOfMatchItemDropsPanel()
{
}

void CCSGO_EndOfMatchItemDropsPanel::SetupJavascriptObjectTemplate()
{
	BaseClass::SetupJavascriptObjectTemplate();

	panorama::RegisterJSAccessorReadOnly( "bDropListIsReady", PANORAMA_DELEGATE( &CCSGO_EndOfMatchItemDropsPanel::IsDropListReady ) );
	panorama::RegisterJSAccessorReadOnly( "DropListJSO", PANORAMA_DELEGATE( &CCSGO_EndOfMatchItemDropsPanel::GetDropListJSO ) );
	panorama::RegisterJSAccessorReadOnly( "bDropTimeHasEnded", PANORAMA_DELEGATE( &CCSGO_EndOfMatchItemDropsPanel::HasDropTimeEnded ) );
	panorama::RegisterJSAccessorReadOnly( "DropTimeRemaining", PANORAMA_DELEGATE( &CCSGO_EndOfMatchItemDropsPanel::GetDropTimeRemaining ) );
}


v8::Local< v8::Object > CCSGO_EndOfMatchItemDropsPanel::GetDropListJSO()
{
	ProcessDropList();

	return PANORAMA_FETCH_EXPOSED_KEYVALUE_MEMBER( m_pKVDropList );
}


void CCSGO_EndOfMatchItemDropsPanel::ProcessDropList()
{

	if ( !CSGameRules() )
		return;

	PANORAMA_CLEAR_EXPOSED_KEYVALUES_MEMBER( m_pKVDropList );
	m_pKVDropList.Attach( new KeyValues( "droplist" ) );

	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	C_CS_PlayerResource *cs_PR = static_cast< C_CS_PlayerResource * >( g_PR );
	if ( !pLocalPlayer || !cs_PR )
		return;

	const CUtlVector< CEconItemPreviewDataBlock * > &itemsPtrDroppedDuringMatch = CSGameRules()->GetItemsDroppedDuringMatch();
	CSteamID localXuid = cs_PR->GetXuid( pLocalPlayer->entindex() );

	// send all items to the scoreboard script and store them in the script
	for ( int i = 0; i < itemsPtrDroppedDuringMatch.Count(); i++ )
	{
		CSteamID steamOwnerID;
		// double check to see if this player is still connected when we try to display the item, if they aren't connected, just skip showing their items
		int nPlayerTeam = TEAM_UNASSIGNED;
		for ( int j = 1; j <= MAX_PLAYERS; j++ )
		{
			CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( j ) );
			if ( pPlayer )
			{
				CSteamID steamID;
				if ( pPlayer->GetSteamID( &steamID ) && steamID.IsValid() &&
					( steamID.GetAccountID() == itemsPtrDroppedDuringMatch[ i ]->accountid() ) )
				{
					nPlayerTeam = pPlayer->GetTeamNumber();
					steamOwnerID = steamID;
					break;
				}
			}
		}

		// the player is connected, include this item
		if ( steamOwnerID.IsValid() )
		{
			UtlOwnedPtr<KeyValues> pKVDropItem( PTR_CONSTRUCT, CFmtStr( "%d", i ) );

			bool bIsLocalPlayerItem = ( localXuid.ConvertToUint64() == steamOwnerID.ConvertToUint64() );

			// if the item isn't ours, send the definition index in place of the itemid
			//			char itemIDStr[ 255 ] = { '0' };
			//			V_snprintf( itemIDStr, ARRAYSIZE( itemIDStr ), "%llu", itemsPtrDroppedDuringMatch[ i ]->itemid() );

			pKVDropItem->SetUint64( "item_id", itemsPtrDroppedDuringMatch[ i ]->itemid() );


			//			char ownerXuidStr[ 255 ] = { '0' };
			//			V_snprintf( ownerXuidStr, ARRAYSIZE( ownerXuidStr ), "%llu", steamOwnerID.ConvertToUint64() );

			pKVDropItem->SetUint64( "owner_xuid", steamOwnerID.ConvertToUint64() );

			//	how long do we pause on this item?
			//
			extern ConVar sv_endmatch_item_drop_interval;
			extern ConVar sv_endmatch_item_drop_interval_ancient;
			extern ConVar sv_endmatch_item_drop_interval_legendary;
			extern ConVar sv_endmatch_item_drop_interval_mythical;
			extern ConVar sv_endmatch_item_drop_interval_rare;

			float flItemDisplayTime = sv_endmatch_item_drop_interval.GetFloat();
			switch ( itemsPtrDroppedDuringMatch[ i ]->rarity() )
			{
				case 6:
				case 5:
				{
					flItemDisplayTime = sv_endmatch_item_drop_interval_ancient.GetFloat();
					break;
				}
				case 4:
				{
					flItemDisplayTime = sv_endmatch_item_drop_interval_legendary.GetFloat();
					break;
				}
				case 3:
				{
					flItemDisplayTime = sv_endmatch_item_drop_interval_mythical.GetFloat();
					break;
				}
				case 2:
				{
					flItemDisplayTime = sv_endmatch_item_drop_interval_rare.GetFloat();
					break;
				}
			}

			pKVDropItem->SetFloat( "display_time", flItemDisplayTime );

			// item id
			int iFauxItemAttrParam = ( int )( uint )itemsPtrDroppedDuringMatch[ i ]->paintindex();

			uint8 ub1 = 0;
			const CEconItemDefinition *pItemDefDropped = GEconItemSchema().GetItemDefinition( itemsPtrDroppedDuringMatch[ i ]->defindex(), true );
			extern bool Helper_IsSpray( const CEconItemDefinition * pEconItemDefinition );
			if ( Helper_IsSpray( pItemDefDropped ) && itemsPtrDroppedDuringMatch[ i ]->stickers().size() )
			{
				iFauxItemAttrParam = ( int )( uint )itemsPtrDroppedDuringMatch[ i ]->stickers( 0 ).sticker_id();
				ub1 = itemsPtrDroppedDuringMatch[ i ]->stickers( 0 ).tint_id();
			}

			itemid_t ullFauxItemId = CombinedItemIdMakeFromDefIndexAndPaint( ( int )( uint )itemsPtrDroppedDuringMatch[ i ]->defindex(), iFauxItemAttrParam, ub1 );
			// 			char szFauxItemId[ 255 ] = { '0' };
			// 			V_snprintf( szFauxItemId, ARRAYSIZE( szFauxItemId ), "%llu", ullFauxItemId );

			pKVDropItem->SetUint64( "faux_item_id", ullFauxItemId );
			pKVDropItem->SetInt( "owner_team", nPlayerTeam );
			pKVDropItem->SetInt( "rarity", ( uint )itemsPtrDroppedDuringMatch[ i ]->rarity() );
			pKVDropItem->SetBool( "is_local", bIsLocalPlayerItem );
			pKVDropItem->SetInt( "reason", ( uint )itemsPtrDroppedDuringMatch[ i ]->dropreason() );

			m_pKVDropList->AddSubKey( pKVDropItem.Detach() );
		}
	}
}

bool CCSGO_EndOfMatchItemDropsPanel::IsDropListReady()
{
	return ( CSGameRules() && !CSGameRules()->GetItemsDroppedDuringMatch().IsEmpty() );
}



bool CCSGO_EndOfMatchItemDropsPanel::HasDropTimeEnded()
{
	if ( !CSGameRules() )
		return false;

	return ( CSGameRules()->GetCMMItemDropRevealEndTime() < gpGlobals->curtime );
}

float CCSGO_EndOfMatchItemDropsPanel::GetDropTimeRemaining()
{
	if ( !CSGameRules() )
		return 0.0f;

	return ( CSGameRules()->GetCMMItemDropRevealEndTime() - gpGlobals->curtime );
}



/////////////////////////////////////////////////////////////////////////////////////////////////

REGISTER_PANEL2D_FACTORY( CCSGO_EndOfMatchSkillgroupPanel, CSGOEndOfMatchSkillgroupPanel )

//DEFINE_PANORAMA_EVENT( XPProgressReady );


CCSGO_EndOfMatchSkillgroupPanel::CCSGO_EndOfMatchSkillgroupPanel( CPanel2D *pParent, const char *pchID )
	: panorama::CPanel2D( pParent, pchID )
	, m_pKVSkillgroupUpdateData( nullptr )
	, m_bSkillgroupUpdated( false )
{
	RequireLoadLayout( "file://{resources}/layout/endofmatch-skillgroup.xml" );
}

CCSGO_EndOfMatchSkillgroupPanel::~CCSGO_EndOfMatchSkillgroupPanel()
{
}


void CCSGO_EndOfMatchSkillgroupPanel::ProcessServerUpdateMsg()
{

	if ( !CSGameRules() )
		return;

	PANORAMA_CLEAR_EXPOSED_KEYVALUES_MEMBER( m_pKVSkillgroupUpdateData );
	m_pKVSkillgroupUpdateData.Attach( new KeyValues( "skillgroup_update" ) );

	CCSUsrMsg_ServerRankUpdate_RankUpdate const &msg = CSGameRules()->GetEndMatchClientData()->m_skillgroupUpdateMsg;

	m_pKVSkillgroupUpdateData->SetInt( "old_rank", ( int )msg.rank_old() );
	m_pKVSkillgroupUpdateData->SetInt( "new_rank", ( int )msg.rank_new() );
	m_pKVSkillgroupUpdateData->SetInt( "num_wins", ( int )msg.num_wins() );
// 	m_pKVSkillgroupUpdateData->SetFloat( "rank_change", ( float )msg.rank_change() );
// 	m_pKVSkillgroupUpdateData->SetInt( "rank_type", ( int )msg.rank_type_id() );
}

bool CCSGO_EndOfMatchSkillgroupPanel::IsSkillgroupReady()
{
	return CSGameRules() && CSGameRules()->GetEndMatchClientData()->m_skillgroupUpdateMsg.has_account_id();
}

void CCSGO_EndOfMatchSkillgroupPanel::SetupJavascriptObjectTemplate()
{
	BaseClass::SetupJavascriptObjectTemplate();

	panorama::RegisterJSAccessorReadOnly( "bSkillgroupDataReady", PANORAMA_DELEGATE( &CCSGO_EndOfMatchSkillgroupPanel::IsSkillgroupReady ) );
	panorama::RegisterJSAccessorReadOnly( "SkillgroupDataJSO", PANORAMA_DELEGATE( &CCSGO_EndOfMatchSkillgroupPanel::GetSkillgroupDataJSO ) );

}

v8::Local< v8::Object > CCSGO_EndOfMatchSkillgroupPanel::GetSkillgroupDataJSO()
{
	ProcessServerUpdateMsg();
	return PANORAMA_FETCH_EXPOSED_KEYVALUE_MEMBER( m_pKVSkillgroupUpdateData );
}





/////////////////////////////////////////////////////////////////////////////////////////////////

REGISTER_PANEL2D_FACTORY( CCSGO_EndOfMatchXpPanel, CSGOEndOfMatchXpPanel )

//DEFINE_PANORAMA_EVENT( XPProgressReady );


CCSGO_EndOfMatchXpPanel::CCSGO_EndOfMatchXpPanel( CPanel2D *pParent, const char *pchID )
	: panorama::CPanel2D( pParent, pchID )
	, m_pKVXpData(nullptr)
{
	RequireLoadLayout( "file://{resources}/layout/endofmatch-rank.xml" );
}

CCSGO_EndOfMatchXpPanel::~CCSGO_EndOfMatchXpPanel()
{
}


void CCSGO_EndOfMatchXpPanel::ProcessXpMsg()
{
	if ( !CSGameRules() )
		return;

	PANORAMA_CLEAR_EXPOSED_KEYVALUES_MEMBER( m_pKVXpData );
	m_pKVXpData.Attach( new KeyValues( "xp_progress" ) );

	CUtlStringBuilder strReasonIDs;
	CMsgGCCstrike15_v2_GC2ServerNotifyXPRewarded const &data = CSGameRules()->GetEndMatchClientData()->m_xpMsg.data();

	UtlOwnedPtr<KeyValues> pKVXpEarned( PTR_CONSTRUCT, "xp_earned" );

	for ( int i = 0; i < data.xp_progress_data_size(); i++ )
	{
		pKVXpEarned->SetString( CFmtStr( "%d", data.xp_progress_data( i ).xp_category() ),
			CFmtStr( "%d", CSGOXpPointsFromXpWirePoints( data.xp_progress_data( i ).xp_points() ) ) );
	}

	m_pKVXpData->AddSubKey( pKVXpEarned.Detach() );

	m_pKVXpData->SetInt( "current_level", ( int )data.current_level() );
	m_pKVXpData->SetInt( "current_xp", ( int )CSGOXpPointsFromXpWirePoints( data.current_xp() ) );

//	m_pKVXpProgress->SetInt( "upgraded_defidx", ( int )data.upgraded_defidx() );
//	m_pKVXpProgress->SetInt( "operation_points_awarded", ( int )data.operation_points_awarded() );
}

bool CCSGO_EndOfMatchXpPanel::IsXpDataReady()
{
	return CSGameRules() && CSGameRules()->GetEndMatchClientData()->m_xpMsg.has_data();
}

void CCSGO_EndOfMatchXpPanel::SetupJavascriptObjectTemplate()
{
	BaseClass::SetupJavascriptObjectTemplate();

	panorama::RegisterJSAccessorReadOnly( "bXpDataReady", PANORAMA_DELEGATE( &CCSGO_EndOfMatchXpPanel::IsXpDataReady ) );
	panorama::RegisterJSAccessorReadOnly( "XpDataJSO", PANORAMA_DELEGATE( &CCSGO_EndOfMatchXpPanel::GetXpDataJSO ) );
}

v8::Local< v8::Object > CCSGO_EndOfMatchXpPanel::GetXpDataJSO()
{
	ProcessXpMsg();

	return PANORAMA_FETCH_EXPOSED_KEYVALUE_MEMBER( m_pKVXpData );
}



/////////////////////////////////////////////////////////////////////////////////////////////////

REGISTER_PANEL2D_FACTORY( CCSGO_EndOfMatchVotingPanel, CSGOEndOfMatchVotingPanel )

CCSGO_EndOfMatchVotingPanel::CCSGO_EndOfMatchVotingPanel( CPanel2D *pParent, const char *pchID )
	: panorama::CPanel2D( pParent, pchID )
	, m_pKVVotingData( nullptr )
{
	RequireLoadLayout( "file://{resources}/layout/endofmatch-voting.xml" );
}

CCSGO_EndOfMatchVotingPanel::~CCSGO_EndOfMatchVotingPanel()
{
}

void CCSGO_EndOfMatchVotingPanel::SetupJavascriptObjectTemplate()
{
	BaseClass::SetupJavascriptObjectTemplate();

	panorama::RegisterJSAccessorReadOnly( "NextMatchVotingData", PANORAMA_DELEGATE( &CCSGO_EndOfMatchVotingPanel::GetNextMatchVotingDataJSO ) );
}

v8::Local< v8::Object > CCSGO_EndOfMatchVotingPanel::GetNextMatchVotingDataJSO()
{
	GetNextMatchVotingDataJSO_Internal();

	return PANORAMA_FETCH_EXPOSED_KEYVALUE_MEMBER( m_pKVVotingData );
}

static const char* sGetEndMatchVoteMapNameInMapgroupInternal( int nMapIndexInGroup )
{
	const char* internalMapName = nullptr;

	if ( g_pGameTypes )
	{
		const char* mapGroupName = engine->GetMapGroupName();
		const CUtlStringList* mapsInGroup = g_pGameTypes->GetMapGroupMapList( mapGroupName );

		if ( mapsInGroup )
		{
			int nNumMaps = mapsInGroup->Count();
			if ( nMapIndexInGroup >= 0 && nMapIndexInGroup < nNumMaps )
				internalMapName = mapsInGroup->Element( nMapIndexInGroup );
		}
	}

	return internalMapName;
}

void CCSGO_EndOfMatchVotingPanel::GetNextMatchVotingDataJSO_Internal()
{

	if ( !CSGameRules() )
		return;

	C_CS_PlayerResource *cs_PR = static_cast< C_CS_PlayerResource * >( g_PR );

	if ( !cs_PR )
		return;

	PANORAMA_CLEAR_EXPOSED_KEYVALUES_MEMBER( m_pKVVotingData );
	m_pKVVotingData.Attach( new KeyValues( "matchendvotedata" ) );

	// collect current state of player votes
	int nVotes[ MAX_ENDMATCH_VOTE_PANELS ];
	for ( int i = 0; i < MAX_ENDMATCH_VOTE_PANELS; i++ )
		nVotes[ i ] = 0;

	int nTotalPlayers = 0;

	for ( int playerIndex = 1; playerIndex <= MAX_PLAYERS; playerIndex++ )
	{
		if ( cs_PR->IsConnected( playerIndex ) )
		{
			int nVote = cs_PR->GetEndMatchNextMapVote( playerIndex );
			if ( nVote != -1 && nVote < MAX_ENDMATCH_VOTE_PANELS )
			{
				nVotes[ nVote ] ++;
			}

			if ( cs_PR->GetTeam( playerIndex ) != TEAM_SPECTATOR && !cs_PR->IsFakePlayer( playerIndex ) )
				nTotalPlayers++;
		}
	}

	float flnVotesToSucceed = ( float )nTotalPlayers * 0.501f;
	int nVotesToSucceed = ceil( flnVotesToSucceed );

	m_pKVVotingData->SetInt( "votes_to_succeed", nVotesToSucceed );

	m_pKVVotingData->SetBool( "voting_done", cs_PR->EndMatchNextMapAllVoted() );
	m_pKVVotingData->SetInt( "voting_winner", CSGameRules()->m_nEndMatchMapVoteWinner );

	UtlOwnedPtr<KeyValues> pKVVoteOptions( PTR_CONSTRUCT, "voting_options" );

	//bool bVotingOptions = false;

	int nValidOptions = 0;

	for ( int nIndex = 0; nIndex < MAX_ENDMATCH_VOTE_PANELS; ++nIndex )
	{
		if ( CSGameRules()->m_nEndMatchMapGroupVoteTypes[ nIndex ] != CCSGameRules::kEndMatchVoteType_Invalid )
		{
			CCSGameRules::EndMatchVoteType voteType = ( CCSGameRules::EndMatchVoteType )CSGameRules()->m_nEndMatchMapGroupVoteTypes.Get( nIndex ); // map or separator or skirmish
			int voteOption = CSGameRules()->m_nEndMatchMapGroupVoteOptions.Get( nIndex ); // the option index

			UtlOwnedPtr<KeyValues> pKVVoteItem( PTR_CONSTRUCT, CFmtStr( "%d", nIndex ) );

			switch ( voteType )
			{
				case CCSGameRules::kEndMatchVoteType_MapIndexInMapGroup:
				case CCSGameRules::kEndMatchVoteType_Old_MapIndexInMapGroup:
				{
					const char* internalMapName = sGetEndMatchVoteMapNameInMapgroupInternal( voteOption );
					if ( internalMapName && *internalMapName && V_strcmp( "undefined", internalMapName ) )
					{
						pKVVoteItem->SetString( "type", "map" );
						pKVVoteItem->SetString( "name", internalMapName );
						pKVVoteItem->SetInt( "votes", nVotes[ nIndex ] );
					}

					nValidOptions++;

					break;
				}

				case CCSGameRules::kEndMatchVoteType_ChangeSkirmish:
				{
					const CSkirmishModeDefinition* pSkirmishMode = GetItemSchema()->GetSkirmishModeDefinition( voteOption );

					pKVVoteItem->SetString( "type", "skirmish" );
					pKVVoteItem->SetString( "name", pSkirmishMode->GetLocNameToken() );
					pKVVoteItem->SetInt( "id", pSkirmishMode->GetID() );

					pKVVoteItem->SetInt( "votes", nVotes[ nIndex ] );

					nValidOptions++;

					break;
				}

				case CCSGameRules::kEndMatchVoteType_Separator:
				{
					switch ( voteOption ) {
						case CCSGameRules::kEndMatchVoteSeparator_ChangeMode:

							pKVVoteItem->SetString( "type", "separator" );
							pKVVoteItem->SetString( "name", "SFUI_Skirmishes_VoteSeparator_ChangeMode" );
							break;

						case CCSGameRules::kEndMatchVoteSeparator_Blank:
						default:
							pKVVoteItem->SetString( "", "" );
							break;
					}
					break;
				}

				case CCSGameRules::kEndMatchVoteType_Invalid:
				default:
					pKVVoteItem->SetString( "", "" );
					break;
			}

			pKVVoteOptions->AddSubKey( pKVVoteItem.Detach() );
		}
	}

	if ( nValidOptions > 1 )
		m_pKVVotingData->AddSubKey( pKVVoteOptions.Detach() );

}


static int GetGamePhase()
{
	// GAMEPHASE_WARMUP_ROUND			0
	// GAMEPHASE_PLAYING_STANDARD		1
	// GAMEPHASE_PLAYING_FIRST_HALF		2
	// GAMEPHASE_PLAYING_SECOND_HALF	3
	// GAMEPHASE_HALFTIME				4
	// GAMEPHASE_MATCH_ENDED			5

	int nGamePhase = -1;
	if ( CCSGameRules *mp = CSGameRules() )
	{
		if ( mp->IsWarmupPeriod() )
			nGamePhase = GAMEPHASE_WARMUP_ROUND;
		else
			nGamePhase = mp->GetGamePhase();
	}

	return nGamePhase;
}


REGISTER_PANEL2D_FACTORY( CCSGO_GameTimeLabel, CSGOGameTimeLabel )

CCSGO_GameTimeLabel::CCSGO_GameTimeLabel( CPanel2D *pParent, const char *pchID )
	: panorama::CLabel( pParent, pchID )
{
	// Register for events
	
// 	RegisterForReadyEvents( true );
// 
// 	if ( !panorama::UIEngine()->BHaveEventHandlersRegisteredForType( CCSGO_GameTimeLabel::GetPanelSymbol() ) )
// 	{
// 		RegisterEventHandlerOnPanelType( panorama::ReadyForDisplay(), &CCSGO_GameTimeLabel::EventReadyForDisplay );
// 		RegisterEventHandlerOnPanelType( panorama::UnreadyForDisplay(), &CCSGO_GameTimeLabel::EventUnreadyForDisplay );
// 
// 	}

	static const panorama::CPanoramaSymbol k_symTimer( "timer" );
	SetHasClass( k_symTimer, true );

	m_bActiveTimer = false;
	m_eType = TIMER_DEFAULT;

	SetVisible( false );
	SetOpacity( 0.f );

	SetParseAsHTML( true );

	CUiComponent_GameState::GetInstance()->m_vecGameTimeLabels.AddToTail( this );


}

CCSGO_GameTimeLabel::~CCSGO_GameTimeLabel()
{
	CUiComponent_GameState::GetInstance()->m_vecGameTimeLabels.FindAndRemove( this );
}


void CCSGO_GameTimeLabel::SetupJavascriptObjectTemplate()
{
	BaseClass::SetupJavascriptObjectTemplate();

	RegisterJSAccessor( "active", PANORAMA_DELEGATE( &CCSGO_GameTimeLabel::IsActive ), PANORAMA_DELEGATE( &CCSGO_GameTimeLabel::SetActive ) );
}

bool CCSGO_GameTimeLabel::BSetProperty( panorama::CPanoramaSymbol symName, const char *pchValue )
{
//	static const panorama::CPanoramaSymbol k_symType( "time-type" );

	if ( !V_strcmp( pchValue, "round-remaining" ) )
	{
		m_eType = TIMER_ROUND_REMAINING;
	}
	else if ( !V_strcmp( pchValue, "round-elapsed" ) )
	{
		m_eType = TIMER_ROUND_ELAPSED;
	}
	else if ( !V_strcmp( pchValue, "map-remaining" ) )
	{
		m_eType = TIMER_MAP_REMAINING;
	}
	else if ( !V_strcmp( pchValue, "map-elapsed" ) )
	{
		m_eType = TIMER_MAP_ELAPSED;
	}
	else // "remaining-time"
	{
		m_eType = TIMER_DEFAULT;
	}

	return BaseClass::BSetProperty( symName, pchValue );
}

// bool CCSGO_GameTimeLabel::EventReadyForDisplay( const panorama::CPanelPtr< panorama::IUIPanel > &panelPtr )
// {
// 	CUiComponent_GameState::GetInstance()->m_vecGameTimeLabels.AddToTail( this );
// 	return true;
// }
// 
// bool CCSGO_GameTimeLabel::EventUnreadyForDisplay( const panorama::CPanelPtr< panorama::IUIPanel > &panelPtr )
// {
// 	CUiComponent_GameState::GetInstance()->m_vecGameTimeLabels.FindAndRemove( this );
// 	return true;
// }

bool CCSGO_GameTimeLabel::IsActive()
{
	return m_bActiveTimer;
}

void CCSGO_GameTimeLabel::SetActive( bool bActive )
{
	m_bActiveTimer = bActive;

	SetVisible( m_bActiveTimer );
	SetOpacity( m_bActiveTimer ? 1.0f : .0f );
	SetDialogVariable( "s_gametime_time", "" );
	SetDialogVariable( "s_gametime_desc", "" );

}

