//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "cbase.h"
#include "hltvdirector.h"
#include "igameevents.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


class CCSHLTVDirector : public CHLTVDirector
{
public:
	DECLARE_CLASS( CCSHLTVDirector, CHLTVDirector );

	const char** GetModEvents();
	void AddHLTVServer( IHLTVServer *hltv );
	void CreateShotFromEvent( CHLTVGameEvent *event );

};

void CCSHLTVDirector::AddHLTVServer( IHLTVServer *hltv )
{
	BaseClass::AddHLTVServer( hltv );

	// mod specific events the director uses to find interesting shots
	ListenForGameEvent( "hostage_rescued" );
	ListenForGameEvent( "hostage_killed" );
	ListenForGameEvent( "hostage_hurt" );
	ListenForGameEvent( "hostage_follows" );
	ListenForGameEvent( "bomb_pickup" );
	ListenForGameEvent( "bomb_dropped" );
	ListenForGameEvent( "bomb_exploded" );
	ListenForGameEvent( "bomb_defused" );
	ListenForGameEvent( "bomb_planted" );
	ListenForGameEvent( "bomb_begindefuse" );
	ListenForGameEvent( "bomb_beginplant" );
	ListenForGameEvent( "vip_escaped" );
	ListenForGameEvent( "vip_killed" );
}


void CCSHLTVDirector::CreateShotFromEvent( CHLTVGameEvent *event )
{
	// show event at least for 2 more seconds after it occured
	const char *name = event->m_Event->GetName();

	CBaseEntity *player = NULL;

	if ( !Q_strcmp( "hostage_rescued", name ) ||
		 !Q_strcmp( "hostage_hurt", name ) ||
		 !Q_strcmp( "hostage_follows", name ) ||
		 !Q_strcmp( "hostage_killed", name ) )
	{
		player = UTIL_PlayerByUserId( event->m_Event->GetInt("userid") );

		if ( !player )
			return;

		// shot player as primary, hostage as secondary target
		StartChaseCameraShot( player->entindex(), event->m_Event->GetInt( "hostage" ), 96, 20, 40, false );
		// + 2 seconds
		m_nNextShotTick = event->m_Tick+TIME_TO_TICKS(2.0);
		return;
	}

	else if (	!Q_strcmp( "bomb_beginplant", name ) ||
				!Q_strcmp( "bomb_begindefuse", name ) )
	{

		player = UTIL_PlayerByUserId( event->m_Event->GetInt("userid") );

		if ( !player )
			return;

		StartChaseCameraShot( player->entindex(), 0, 500, 45, 180, true );
		// + 3 seconds		
		m_nNextShotTick = event->m_Tick + TIME_TO_TICKS( 3.0 );
		return;
	}
	
	// let baseclass create a shot
	BaseClass::CreateShotFromEvent( event );	
}

const char** CCSHLTVDirector::GetModEvents()
{
	// game events relayed to spectator clients
	static const char *s_modevents[] =
	{
		"achievement_earned",
		"announce_phase_end", 	
		"begin_new_match",
		"bomb_begindefuse",
		"bomb_beginplant",	
		"bomb_defused",
		"bomb_dropped",
		"bomb_exploded",
		"bomb_pickup",
		"bomb_planted",	
		"bot_takeover",
		"buytime_ended",
		"choppers_incoming_warning",
		"firstbombs_incoming_warning",
		"cs_game_disconnected",
		"cs_match_end_restart",
		"cs_pre_restart",
		"cs_round_final_beep",
		"cs_round_start_beep",
		"cs_win_panel_match",			
		"cs_win_panel_round",
		"decoy_detonate",
		"decoy_started",
		"defuser_dropped",
		"defuser_pickup",
		"dm_bonus_weapon_start",
		"endmatch_cmm_start_reveal_items",
		"endmatch_mapvote_selecting_map",
		"flashbang_detonate",
		"game_newmap",
		"hegrenade_detonate",
		"hltv_chat",
		"hltv_status",
		"hltv_status",
		"hostage_hurt",
		"hostage_killed",
		"hostage_rescued",
		"hostage_rescued_all",
		"inferno_expire",
		"inferno_startburn",
		"item_equip",
		"item_found",
		"item_pickup",
		"item_remove",
		"items_gifted",
		"other_death",
		"player_blind",
		"player_changename",
		"player_chat",
		"player_connect",
		"player_connect_full",
		"player_death",
		"player_disconnect",
		"player_falldamage",
		"player_footstep",
		"player_hurt",
		"player_info",
		"player_jump",
		"player_spawn",
		"player_team",
		"round_announce_final",
		"round_announce_last_round_half",
		"round_announce_match_point",
		"round_announce_match_start",
		"round_announce_warmup",
		"round_end",
		"round_freeze_end",
		"round_mvp",	
		"round_officially_ended",
		"round_poststart",
		"round_prestart",
		"round_start",
		"round_time_warning",
		"seasoncoin_levelup",
		"server_cvar",
		"server_spawn",
		"smokegrenade_detonate",
		"smokegrenade_expired",
		"sniper_warning",
		"survival_paradrop_spawn",
		"survival_paradrop_break",
		"teamplay_broadcast_audio",
		"tournament_reward",
		"weapon_fire",
		"weapon_fire_on_empty",
		"weapon_outofammo",
		"weapon_reload",
		"weapon_zoom",
		NULL
	};

	return s_modevents;
}

static CCSHLTVDirector s_HLTVDirector;	// singleton

EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CHLTVDirector, IHLTVDirector, INTERFACEVERSION_HLTVDIRECTOR, s_HLTVDirector );

CHLTVDirector* HLTVDirector()
{
	return &s_HLTVDirector;
}

IGameSystem* HLTVDirectorSystem()
{
	return &s_HLTVDirector;
}