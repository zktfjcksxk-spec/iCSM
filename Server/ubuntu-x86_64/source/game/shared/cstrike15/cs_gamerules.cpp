//========= Copyright  1996-2005, Valve Corporation, All rights reserved. ============//
// 
// Purpose: The TF Game rules 
//
// $NoKeywords: $
//=============================================================================//

#ifdef POSIX
//
// Crazy compiler problems fix -- deque must be included first before any of the Valve game headers, which introduce
// macro defines that completely break STL compile process. STL include dependencies happen as part of RSA crypto
// library dependencies included at the bottom of this file.
//
#include <deque>
#endif

#include "cbase.h"
#include "cs_gamerules.h"
#include "cs_ammodef.h"
#include "weapon_csbase.h"
#include "basecsgrenade_projectile.h"
#include "cs_shareddefs.h"
#include "keyvalues.h"
#include "cs_achievement_constants.h"
#include "iachievementmgr.h"
#include "matchmaking/imatchframework.h"
#include "inputsystem/iinputsystem.h"
#include "platforminputdevice.h"
#include "engine/inetsupport.h"
#include "usermessages.h"
#include "hegrenade_projectile.h"
#ifndef CLIENT_DLL
#include "Effects/inferno.h"
#endif
#include "econ_item_view_helpers.h"

#ifdef CLIENT_DLL
    #include "networkstringtable_clientdll.h"
    #include "c_cs_player.h"
    #include "fmtstr.h"
    #include "vgui/ILocalize.h"		// temp - needed for GetFriendlyMapName()
	#include "c_team.h"
	#include "weapon_selection.h"
	#include "hud_macros.h"
	#include "c_cs_playerresource.h"

#ifdef PANORAMA_ENABLE
#include "panorama/csgo_buymenu.h"
#include "panorama/csgo_endofmatch.h"
#endif

#else
	#include "baseentity.h"
    #include "vote_controller.h"
    #include "cs_voteissues.h"
    #include "bot.h"
    #include "utldict.h"
    #include "cs_player.h"
    #include "cs_team.h"
    #include "cs_gamerules.h"
    #include "voice_gamemgr.h"
    #include "igamesystem.h"
    #include "weapon_c4.h"
    #include "mapinfo.h"
    #include "shake.h"
    #include "mapentities.h"
    #include "game.h"
    #include "cs_simple_hostage.h"
    #include "cs_gameinterface.h"
    #include "player_resource.h"
    #include "info_view_parameters.h"
    #include "cs_bot_manager.h"
    #include "cs_bot.h"
    #include "eventqueue.h"
    #include "fmtstr.h"
    #include "teamplayroundbased_gamerules.h"
    #include "gameweaponmanager.h"
    #include "cs_gamestats.h"
    #include "cs_urlretrieveprices.h"
    #include "networkstringtable_gamedll.h"
    #include "func_bomb_target.h"
    #include "func_hostage_rescue.h"
	#include "dedicated_server_ugc_manager.h"
    #include "vstdlib/vstrtools.h"
    #include "platforminputdevice.h"
    #include "cs_entity_spotting.h"
    #include "props.h"
	#include "hltvdirector.h"
	#include "econ_gcmessages.h"
	#include "env_cascade_light.h"
	#include "world.h"
	#include "items.h"
	#include "Effects/chicken.h"
	#include "item_dogtags.h"
	#include "global_chatter.h"
	#include "particle_system.h"
	#include "snowball_pile.h"
#endif

#include "gametypes.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"
#ifdef GAME_DLL
//--------------------------------------------------------------------------------------------------------------
const CSchemaItemDefHandle gGungame_Pistols[] = {
	CSchemaItemDefHandle( "weapon_deagle" ),
	CSchemaItemDefHandle( "weapon_revolver" ),
	CSchemaItemDefHandle( "weapon_elite" ),
	CSchemaItemDefHandle( "weapon_fiveseven" ),
	CSchemaItemDefHandle( "weapon_glock" ),
	CSchemaItemDefHandle( "weapon_tec9" ),
	CSchemaItemDefHandle( "weapon_hkp2000" ),
	CSchemaItemDefHandle( "weapon_usp_silencer" ),
	CSchemaItemDefHandle( "weapon_p250" ),
};

const CSchemaItemDefHandle gGungame_Rifles[] = {
	CSchemaItemDefHandle( "weapon_ak47" ),
	CSchemaItemDefHandle( "weapon_aug" ),
	CSchemaItemDefHandle( "weaopn_famas" ),
	CSchemaItemDefHandle( "weapon_galilar" ),
	CSchemaItemDefHandle( "weapon_m4a1" ),
	CSchemaItemDefHandle( "weapon_m4a1_silencer" ),
	CSchemaItemDefHandle( "weapon_sg556" ),
};

const CSchemaItemDefHandle gGungame_Machineguns[] = {
	CSchemaItemDefHandle( "weapon_m249" ),
	CSchemaItemDefHandle( "weapon_negev" ),
};

const CSchemaItemDefHandle gGungame_Shotguns[] = {
	CSchemaItemDefHandle( "weapon_xm1014" ),
	CSchemaItemDefHandle( "weapon_mag7" ),
	CSchemaItemDefHandle( "weapon_sawedoff" ),
	CSchemaItemDefHandle( "weapon_nova" ),
};

const CSchemaItemDefHandle gGungame_Smgs[] = {
	CSchemaItemDefHandle( "weapon_mac10" ),
	CSchemaItemDefHandle( "weapon_p90" ),
	CSchemaItemDefHandle( "weapon_ump45" ),
	CSchemaItemDefHandle( "weapon_bizon" ),
	CSchemaItemDefHandle( "weapon_mp7" ),
	CSchemaItemDefHandle( "weapon_mp5sd" ),
	CSchemaItemDefHandle( "weapon_mp9" ),
};

const CSchemaItemDefHandle gGungame_Snipers[] = {
	CSchemaItemDefHandle( "weapon_scar20" ),
	CSchemaItemDefHandle( "weapon_g3sg1" ),
	CSchemaItemDefHandle( "weapon_awp" ),
};
#endif

#ifndef CLIENT_DLL

#define CS_GAME_STATS_UPDATE 79200 //22 hours
#define CS_GAME_STATS_UPDATE_PERIOD 7200 // 2 hours

#define ROUND_END_WARNING_TIME 10.0f
static const float MAX_TIME_TO_WAIT_BEFORE_ENTERING = 5.0f;

// # seconds to delay before displaying autobalance text to clients
static const float AUTOBALANCE_TEXT_DELAY = 3.0f;

extern IUploadGameStats *gamestatsuploader;
extern bool Commentary_IsCommentaryEntity( CBaseEntity *pEntity );
void FootstepsServersideModified(IConVar *var, const char *pOldValue, float flOldValue)
{
	extern ConVar mp_footsteps_serverside;

	if (CSGameRules() != nullptr)
	{
		CSGameRules()->SetPlayAllStepSoundsOnServer( mp_footsteps_serverside.GetBool() );
	}
}

ConVar spec_replay_round_delay( "spec_replay_round_delay", "0", FCVAR_RELEASE, "Round can be delayed by this much due to someone watching a replay; must be at least 3-4 seconds, otherwise the last replay will always be interrupted by round start, assuming normal pause between round_end and round_start events (7 seconds) and freezecam delay (2 seconds) and 7.4 second full replay (5.4 second pre-death and ~2 seconds post-death) and replay in/out switching (up to a second)" );
ConVar mp_footsteps_serverside( "mp_footsteps_serverside", "1", FCVAR_RELEASE, "Makes the server always play footstep sounds. Clients never calculate footstep sounds locally, instead relying on the server.", FootstepsServersideModified );


#endif // !CLIENT_DLL

const float g_flWarmupToFreezetimeDelay = 4.0f;

ConVar sv_server_graphic1( "sv_server_graphic1", "", FCVAR_REPLICATED | FCVAR_RELEASE,	"A 360x60 (<16kb) image file in /csgo/ that will be displayed to spectators." );
ConVar sv_server_graphic2( "sv_server_graphic2", "", FCVAR_REPLICATED | FCVAR_RELEASE,	"A 220x45 (<16kb) image file in /csgo/ that will be displayed to spectators." );

ConVar sv_disable_observer_interpolation( "sv_disable_observer_interpolation", "0", FCVAR_REPLICATED | FCVAR_RELEASE, "Disallow interpolating between observer targets on this server." );

ConVar sv_reward_drop_delay( "sv_reward_drop_delay", "3.0", FCVAR_REPLICATED, "Delay between the end match scoreboard being shown and the beginning of item drops." );

// to define which holiday it is
ConVar sv_holiday_mode( "sv_holiday_mode", "0", FCVAR_REPLICATED | FCVAR_RELEASE, "0 = OFF, 1 = Halloween, 2 = Winter" );

ConVar sv_teamid_overhead_always_prohibit( "sv_teamid_overhead_always_prohibit", "0", FCVAR_RELEASE | FCVAR_REPLICATED | FCVAR_NOTIFY, "Determines whether cl_teamid_overhead_always is prohibited." );

ConVar sv_show_team_equipment_prohibit( "sv_show_team_equipment_prohibit", "0", FCVAR_RELEASE | FCVAR_REPLICATED | FCVAR_NOTIFY, "Determines whether +cl_show_team_equipment is prohibited." );
ConVar sv_show_team_equipment_force_on( "sv_show_team_equipment_force_on", "0", FCVAR_RELEASE | FCVAR_REPLICATED, "Force on if not prohibited" );
ConVar sv_teamid_overhead_maxdist_spec( "sv_teamid_overhead_maxdist_spec", "0", FCVAR_RELEASE | FCVAR_REPLICATED, "If >0, server will override cl_teamid_overhead_maxdist_spec" );
ConVar sv_teamid_overhead_maxdist( "sv_teamid_overhead_maxdist", "0", FCVAR_RELEASE | FCVAR_REPLICATED, "If >0, server will override cl_teamid_overhead_maxdist" );

ConVar sv_teamid_overhead( "sv_teamid_overhead", "1", FCVAR_RELEASE | FCVAR_REPLICATED | FCVAR_NOTIFY, "Shows teamID over player's heads.  0 = off, 1 = on" );

ConVar sv_disable_radar( "sv_disable_radar", "0", FCVAR_RELEASE | FCVAR_REPLICATED );

ConVar sv_outofammo_indicator( "sv_outofammo_indicator", "0", FCVAR_RELEASE | FCVAR_REPLICATED );

ConVar sv_falldamage_scale( "sv_falldamage_scale", "1", FCVAR_REPLICATED | FCVAR_RELEASE );

ConVar sv_falldamage_to_below_player_ratio( "sv_falldamage_to_below_player_ratio", "0", FCVAR_REPLICATED | FCVAR_RELEASE, "Landing on a another player's head gives them this ratio of the damage." );
ConVar sv_falldamage_to_below_player_multiplier( "sv_falldamage_to_below_player_multiplier", "1", FCVAR_REPLICATED | FCVAR_RELEASE, "Scale damage when distributed across two players" );

ConVar sv_chat_proximity( "sv_chat_proximity", "-1", FCVAR_RELEASE | FCVAR_REPLICATED );

#if defined( GAME_DLL )
ConVar sv_kick_players_with_cooldown( "sv_kick_players_with_cooldown", "1", FCVAR_RELEASE | FCVAR_GAMEDLL | FCVAR_REPLICATED,
	"(0: do not kick on insecure servers; 1: kick players with Untrusted status or convicted by Overwatch; 2: kick players with any cooldown)" );

ConVar sv_matchend_drops_enabled( "sv_matchend_drops_enabled", "1", FCVAR_RELEASE | FCVAR_GAMEDLL,
	"Rewards gameplay time is always accumulated for players, but drops at the end of the match can be prevented" );

ConVar sv_buy_status_override( "sv_buy_status_override", "-1", FCVAR_RELEASE | FCVAR_GAMEDLL | FCVAR_REPLICATED, "Override for buy status map info. 0 = everyone can buy, 1 = ct only, 2 = t only 3 = nobody" );
ConVar sv_ct_spawn_on_bombsite( "sv_ct_spawn_on_bombsite", "-1", FCVAR_RELEASE | FCVAR_GAMEDLL, "Force cts to spawn on a bombsite" );

ConVar sv_auto_adjust_bot_difficulty("sv_auto_adjust_bot_difficulty", "1", FCVAR_RELEASE | FCVAR_GAMEDLL, "Adjust the difficulty of bots each round based on contribution score." );
ConVar sv_bots_get_easier_each_win("sv_bots_get_easier_each_win", "0", FCVAR_RELEASE | FCVAR_GAMEDLL, "If > 0, some # of bots will lower thier difficulty each time they win. The argument defines how many will lower their difficulty each time." );
ConVar sv_bots_get_harder_after_each_wave( "sv_bots_get_harder_after_each_wave", "0", FCVAR_RELEASE | FCVAR_GAMEDLL, "If > 0, some # of bots will raise thier difficulty each time CTs beat a Guardian wave. The argument defines how many will raise their difficulty each time" );
ConVar sv_bots_force_rebuy_every_round( "sv_bots_force_rebuy_every_round", "0", FCVAR_RELEASE | FCVAR_GAMEDLL, "If set, this strips the bots of their weapons every round and forces them to rebuy." );

#endif

ConVar sv_skirmish_id( "sv_skirmish_id", "0", FCVAR_RELEASE | FCVAR_REPLICATED, "Dedicated server skirmish id to run" );


ConVar mp_team_timeout_time( "mp_team_timeout_time", "60", FCVAR_RELEASE | FCVAR_GAMEDLL | FCVAR_REPLICATED, "Duration of each timeout." );
ConVar mp_team_timeout_max( "mp_team_timeout_max", "1", FCVAR_RELEASE | FCVAR_GAMEDLL | FCVAR_REPLICATED, "Number of timeouts each team gets per match." );

ConVar mp_weapons_max_gun_purchases_per_weapon_per_match( "mp_weapons_max_gun_purchases_per_weapon_per_match", "-1", FCVAR_RELEASE | FCVAR_GAMEDLL | FCVAR_REPLICATED, "Max number of times a player may purchase any weapon per match", true, -1, true, (float)WEAPON_PURCHASE_PER_MATCH_SUPPORTED_LIMIT );

void MaxAllowedNetGraphCallback( IConVar *var, const char *pOldValue, float flOldValue );
ConVar sv_max_allowed_net_graph( "sv_max_allowed_net_graph", "1", FCVAR_NOTIFY | FCVAR_RELEASE | FCVAR_REPLICATED, "Determines max allowed net_graph value for clients.", MaxAllowedNetGraphCallback );

void MaxAllowedNetGraphCallback( IConVar *var, const char *pOldValue, float flOldValue )
{
#ifdef CLIENT_DLL
	extern ConVar net_graph;

	if ( net_graph.GetInt() > sv_max_allowed_net_graph.GetInt() )
	{
		net_graph.SetValue( sv_max_allowed_net_graph.GetInt() );
		Msg( "Server does not allow net_graph values above %d\n", sv_max_allowed_net_graph.GetInt() );
	}
#endif
}

#define SV_QMM_MIN_PLAYERS_FOR_CANCEL_MATCH 0

extern ConVar mp_maxrounds;
extern ConVar mp_match_restart_delay;

ConVar sv_disable_motd( "sv_disable_motd", "1", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT, "Prevent the motd from showing." );

#ifdef CLIENT_DLL
CON_COMMAND_F( print_mapgroup, "Prints the current mapgroup and the contained maps", FCVAR_RELEASE )
#else
CON_COMMAND_F( print_mapgroup_sv, "Prints the current mapgroup and the contained maps", FCVAR_RELEASE )
#endif
{
#if defined ( CLIENT_DLL )
	const char* szMapGroup = engine->GetMapGroupName();
#else
	const char* szMapGroup = STRING( gpGlobals->mapGroupName );
#endif
	const CUtlStringList *pMaps = g_pGameTypes->GetMapGroupMapList( szMapGroup );
	Msg( "Map group: %s\n", szMapGroup );
	if ( pMaps )
	{
		FOR_EACH_VEC( *pMaps, i )
		{
			const char* szMap = (*pMaps)[i];
			Msg( "     %s\n", szMap );
		}
	}
	else
	{
		Msg( "No maps in mapgroup map list!\n");
	}
}

const float cInitialRestartRoundTime = 0.0f;


/**
 * Player hull & eye position for standing, ducking, etc.  This version has a taller
 * player height, but goldsrc-compatible collision bounds.
 */
static CViewVectors g_CSViewVectors(
    Vector( 0, 0, 64 ),		// eye position

    Vector(-16, -16, 0 ),	// hull min
    Vector( 16,  16, 72 ),	// hull max

    Vector(-16, -16, 0 ),	// duck hull min
    Vector( 16,  16, 54 ),	// duck hull max
    Vector( 0, 0, 46 ),		// duck view

    Vector(-10, -10, -10 ),	// observer hull min
    Vector( 10,  10,  10 ),	// observer hull max

    Vector( 0, 0, 14 )		// dead view height
);

#ifndef CLIENT_DLL

extern ConVar spec_replay_bot;

BEGIN_DATADESC( SpawnPoint )
    // Keyfields
    DEFINE_KEYFIELD( m_iPriority,	FIELD_INTEGER,	"priority" ),
	DEFINE_KEYFIELD( m_bEnabled,	FIELD_BOOLEAN,	"enabled" ),

	DEFINE_INPUTFUNC( FIELD_VOID,				"SetEnabled",	InputSetEnabled ),
	DEFINE_INPUTFUNC( FIELD_VOID,				"SetDisabled",	InputSetDisabled ),
	DEFINE_INPUTFUNC( FIELD_VOID,				"ToggleEnabled",	InputToggleEnabled ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( info_player_terrorist, SpawnPoint );
LINK_ENTITY_TO_CLASS( info_player_counterterrorist, SpawnPoint );
LINK_ENTITY_TO_CLASS( info_player_logo, CPointEntity );
LINK_ENTITY_TO_CLASS( info_deathmatch_spawn, SpawnPoint );
LINK_ENTITY_TO_CLASS( info_armsrace_counterterrorist, SpawnPoint );
LINK_ENTITY_TO_CLASS( info_armsrace_terrorist, SpawnPoint );

template< typename TIssue >
void NewTeamIssue()
{
	new TIssue( g_voteControllerT );
	new TIssue( g_voteControllerCT );
}

template< typename TIssue >
void NewGlobalIssue()
{
	new TIssue( g_voteControllerGlobal );
}


SpawnPoint::SpawnPoint() : m_bEnabled( true ), m_nType( 0 )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void SpawnPoint::Spawn( void )
{
	BaseClass::Spawn();

	if ( CSGameRules() )
		CSGameRules()->AddSpawnPointToMasterList( this );
}

void SpawnPoint::InputSetEnabled( inputdata_t &inputdata )
{
	SetSpawnEnabled( true );
}

void SpawnPoint::InputSetDisabled( inputdata_t &inputdata )
{
	SetSpawnEnabled( false );
}

void SpawnPoint::InputToggleEnabled( inputdata_t &inputdata )
{
	m_bEnabled = !m_bEnabled;

	if ( CSGameRules() &&
		 !CSGameRules()->IsPlayingCoopMission() )
	{
		CSGameRules()->RefreshCurrentSpawnPointLists();
	}
}

void SpawnPoint::SetSpawnEnabled( bool bEnabled )
{
	bool bChanged = (m_bEnabled != bEnabled);

	m_bEnabled = bEnabled;

	if ( CSGameRules() && bChanged &&
		 !CSGameRules()->IsPlayingCoopMission() )
	{
		CSGameRules()->RefreshCurrentSpawnPointLists();
	}
}

LINK_ENTITY_TO_CLASS( info_enemy_terrorist_spawn, SpawnPointCoopEnemy );

BEGIN_DATADESC( SpawnPointCoopEnemy )
DEFINE_KEYFIELD( m_szWeaponsToGive, FIELD_STRING, "weapons_to_give" ),
DEFINE_KEYFIELD( m_szPlayerModelToUse, FIELD_STRING, "model_to_use" ),
DEFINE_KEYFIELD( m_nArmorToSpawnWith, FIELD_INTEGER, "armor_to_give" ),
DEFINE_KEYFIELD( m_nDefaultBehavior, FIELD_INTEGER, "default_behavior" ),
DEFINE_KEYFIELD( m_nBotDifficulty, FIELD_INTEGER, "bot_difficulty" ),
DEFINE_KEYFIELD( m_bIsAgressive, FIELD_BOOLEAN, "is_agressive" ),
DEFINE_KEYFIELD( m_bStartAsleep, FIELD_BOOLEAN, "start_asleep" ),
DEFINE_KEYFIELD( m_flHideRadius, FIELD_FLOAT, "hide_radius" ),
END_DATADESC()

SpawnPointCoopEnemy::SpawnPointCoopEnemy( void )
{
	//m_szWeaponsToGive = NULL_STRING;
	Assert( CSGameRules()->IsPlayingCoopMission() );
	m_pMyArea = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void SpawnPointCoopEnemy::Spawn( void )
{
	BaseClass::Spawn();
	Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void SpawnPointCoopEnemy::Precache( void )
{
	if ( V_stricmp( GetPlayerModelToUse(), "" ) != 0 )
	{
		PrecacheModel( GetPlayerModelToUse() );
	}
	BaseClass::Precache();
}

CNavArea * SpawnPointCoopEnemy::FindNearestArea( void )
{
	// These don't move so only do the expensive find the first time
	if ( !m_pMyArea )
		m_pMyArea = TheNavMesh->GetNearestNavArea( this );

	return m_pMyArea;
}


class CPointGiveAmmo : public CPointEntity
{
	DECLARE_CLASS( CPointGiveAmmo, CPointEntity );

	public:
	void	Spawn( void );
	void	Precache( void );

	// Input handlers
	void InputGiveAmmo( inputdata_t &inputdata );

	DECLARE_DATADESC();

	// 		int			m_nDamage;
	// 		int			m_bitsDamageType;
	// 		float		m_flRadius;
	// 		float		m_flDelay;
	// 		string_t	m_strTarget;
	EHANDLE		m_pActivator;
};

BEGIN_DATADESC( CPointGiveAmmo )

// 		DEFINE_KEYFIELD( m_flRadius, FIELD_FLOAT, "DamageRadius" ),
// 		DEFINE_KEYFIELD( m_nDamage, FIELD_INTEGER, "Damage" ),
// 		DEFINE_KEYFIELD( m_flDelay, FIELD_FLOAT, "DamageDelay" ),
// 		DEFINE_KEYFIELD( m_bitsDamageType, FIELD_INTEGER, "DamageType" ),
// 		DEFINE_KEYFIELD( m_strTarget, FIELD_STRING, "DamageTarget" ),

// Function Pointers

// Inputs
DEFINE_INPUTFUNC( FIELD_VOID, "GiveAmmo", InputGiveAmmo ),

DEFINE_FIELD( m_pActivator, FIELD_EHANDLE ),

END_DATADESC()

LINK_ENTITY_TO_CLASS( point_give_ammo, CPointGiveAmmo );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointGiveAmmo::Spawn( void )
{
	SetThink( NULL );
	SetUse( NULL );

	m_pActivator = NULL;

	Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointGiveAmmo::Precache( void )
{
	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for instantaneously hurting whatever is near us.
//-----------------------------------------------------------------------------
void CPointGiveAmmo::InputGiveAmmo( inputdata_t &data )
{
	m_pActivator = data.pActivator;

	CBasePlayer *pPlayer = dynamic_cast< CBasePlayer* >( m_pActivator.Get() );

	if ( pPlayer )
	{
		for ( int i = 0; i < 2; ++i )
		{
			CWeaponCSBase *pWeapon = dynamic_cast< CWeaponCSBase* >( pPlayer->Weapon_GetSlot( GEAR_SLOT_RIFLE ) );
			if ( i == 1 )
				pWeapon = dynamic_cast< CWeaponCSBase* >( pPlayer->Weapon_GetSlot( GEAR_SLOT_PISTOL ) );

			if ( pWeapon )
			{
				if ( pWeapon->UsesPrimaryAmmo() )
				{
					int ammoIndex = pWeapon->GetPrimaryAmmoType();

					if ( ammoIndex != -1 )
					{
						int giveAmount;
						giveAmount = GetAmmoDef()->MaxCarry( ammoIndex, pPlayer );
						pPlayer->GiveAmmo( giveAmount, GetAmmoDef()->GetAmmoOfIndex( ammoIndex )->pName );
					}
				}
				if ( pWeapon->UsesSecondaryAmmo() && pWeapon->HasSecondaryAmmo() )
				{
					// Give secondary ammo out, as long as the player already has some
					// from a presumeably natural source. This prevents players on XBox
					// having Combine Balls and so forth in areas of the game that
					// were not tested with these items.
					int ammoIndex = pWeapon->GetSecondaryAmmoType();

					if ( ammoIndex != -1 )
					{
						int giveAmount;
						giveAmount = GetAmmoDef()->MaxCarry( ammoIndex, pPlayer );
						pPlayer->GiveAmmo( giveAmount, GetAmmoDef()->GetAmmoOfIndex( ammoIndex )->pName );
					}
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
class CCoopBonusCoin : public CDynamicProp
{
	public:
	DECLARE_CLASS( CCoopBonusCoin, CDynamicProp );
	DECLARE_DATADESC();

	void CoinTouch( CBaseEntity *pOther );
	void CoinFadeOut( void );
	//void CoinThink( void );
	void Precache( void );
	void Spawn( void ) OVERRIDE; 
	void StartFadeOut( float delay );
};

LINK_ENTITY_TO_CLASS( item_coop_coin, CCoopBonusCoin );
PRECACHE_REGISTER( item_coop_coin );

BEGIN_DATADESC( CCoopBonusCoin )
// Function Pointers
DEFINE_ENTITYFUNC( CoinTouch ),
DEFINE_THINKFUNC( CoinFadeOut ),
END_DATADESC()

void CCoopBonusCoin::CoinTouch( CBaseEntity *pOther )
{
	CCSPlayer *pPlayer = dynamic_cast< CCSPlayer* >( pOther );
	if ( !pPlayer || !CSGameRules() )
		return;

	CSGameRules()->CoopCollectBonusCoin();
	EmitSound( "CollectableCoin.Collect" );

	ResetSequence( LookupSequence( "challenge_coin_collect" ) );

	SetTouch( NULL );

	StartFadeOut( 0.125 );
}

// void CCoopBonusCoin::CoinThink( void )
// {
// 	StudioFrameAdvance();
// }

void CCoopBonusCoin::Precache( void )
{
	PrecacheModel( "models/coop/challenge_coin.mdl" );
	PrecacheScriptSound( "CollectableCoin.Collect" );
}

void CCoopBonusCoin::Spawn( void )
{
	Precache();
	SetModel( "models/coop/challenge_coin.mdl" );
	SetModelName( MAKE_STRING( "models/coop/challenge_coin.mdl" ) );

	BaseClass::Spawn();

	//m_bShouldGlow = true;

	SetMoveType( MOVETYPE_NONE );
	SetSolid( SOLID_BBOX );
	SetBlocksLOS( false );
	AddSolidFlags( FSOLID_TRIGGER );
	AddEFlags( EFL_NO_ROTORWASH_PUSH );

	SetCollisionGroup( COLLISION_GROUP_WEAPON );
	//if ( HasBloatedCollision() )
	//{
		//CollisionProp()->UseTriggerBounds( true, ITEM_PICKUP_BOX_BLOAT );
	//}
	SetTouch( &CCoopBonusCoin::CoinTouch );

	ResetSequence( LookupSequence( "idle" ) );

	//SetThink( &CCoopBonusCoin::CoinThink );
}

void CCoopBonusCoin::StartFadeOut( float delay )
{
	SetThink( &CCoopBonusCoin::CoinFadeOut );
	SetNextThink( gpGlobals->curtime + delay );
	SetRenderAlpha( 255 );
	m_nRenderMode = kRenderNormal;
}

//-----------------------------------------------------------------------------
// Purpose: Fade out slowly
//-----------------------------------------------------------------------------
void CCoopBonusCoin::CoinFadeOut( void )
{
	float dt = gpGlobals->frametime;
	if ( dt > 0.1f )
		dt = 0.1f;

	m_nRenderMode = kRenderTransTexture;
	int speed = MAX( 0.35, 256 * dt ); // fade out
	SetRenderAlpha( UTIL_Approach( 0, m_clrRender->a, speed ) );

	if ( m_clrRender->a == 0 )
	{
		UTIL_Remove( this );
	}
	else
	{
		SetNextThink( gpGlobals->curtime );
	}
}

#endif

bool IsAssassinationQuest( const CEconQuestDefinition *pQuest )
{
	if ( !pQuest )
		return false;

	if ( !V_stristr( pQuest->GetQuestExpression(), "act_kill_target" )
		&& !V_stristr( pQuest->GetQuestExpression(), "act_pick_up_trophy" ) )
		return false;

	if ( pQuest->GetTargetTeam() != TEAM_TERRORIST && pQuest->GetTargetTeam() != TEAM_CT )
		return false;

	return true;
}

bool IsAssassinationQuest( uint32 questID )
{
	const CEconQuestDefinition *pQuest = GetItemSchema()->GetQuestDefinition( questID );
	return IsAssassinationQuest( pQuest );
}



#if defined ( CLIENT_DLL )
	bool __MsgFunc_SendPlayerItemDrops( const CCSUsrMsg_SendPlayerItemDrops &msg );
	bool __MsgFunc_SendPlayerItemFound( const CCSUsrMsg_SendPlayerItemFound &msg );
#endif

#ifdef GAME_DLL
void* SendProxy_SurvivalGameRules( const SendProp *pProp, const void *pStruct, const void *pVarData, CSendProxyRecipients *pRecipients, int objectID )
{
	CCSGameRules* pGameRules = ( CCSGameRules* )pStruct;

	if ( pGameRules && pGameRules->IsPlayingSurvival() )
		return ( void* )pVarData;

	return nullptr;
}
REGISTER_SEND_PROXY_NON_MODIFIED_POINTER( SendProxy_SurvivalGameRules );
#endif

REGISTER_GAMERULES_CLASS( CCSGameRules );

BEGIN_NETWORK_TABLE_NOBASE( CCSGameRules, DT_CSGameRules )


#ifdef CLIENT_DLL
        RecvPropBool( RECVINFO( m_bFreezePeriod ) ),
		RecvPropBool( RECVINFO( m_bMatchWaitingForResume ) ),
        RecvPropBool( RECVINFO( m_bWarmupPeriod ) ),
        RecvPropFloat( RECVINFO( m_fWarmupPeriodEnd ) ), // ONLY INTENDED FOR CLIENTS
        RecvPropFloat( RECVINFO( m_fWarmupPeriodStart ) ),	

		RecvPropBool( RECVINFO( m_bTerroristTimeOutActive ) ),
		RecvPropBool( RECVINFO( m_bCTTimeOutActive ) ),
		RecvPropFloat( RECVINFO( m_flTerroristTimeOutRemaining ) ),
		RecvPropFloat( RECVINFO( m_flCTTimeOutRemaining ) ),
		RecvPropInt( RECVINFO( m_nTerroristTimeOuts ) ),
		RecvPropInt( RECVINFO( m_nCTTimeOuts ) ),

        RecvPropInt( RECVINFO( m_iRoundTime ) ),
        RecvPropInt( RECVINFO( m_gamePhase ) ),
        RecvPropInt( RECVINFO( m_totalRoundsPlayed ) ),
		RecvPropInt( RECVINFO( m_nOvertimePlaying ) ),
        RecvPropFloat( RECVINFO( m_timeUntilNextPhaseStarts ) ),
		RecvPropFloat( RECVINFO( m_flCMMItemDropRevealStartTime ) ),
		RecvPropFloat( RECVINFO( m_flCMMItemDropRevealEndTime ) ),
        RecvPropFloat( RECVINFO( m_fRoundStartTime ) ),
        RecvPropBool( RECVINFO( m_bGameRestart ) ),	
        RecvPropFloat( RECVINFO( m_flRestartRoundTime ) ),	
        RecvPropFloat( RECVINFO( m_flGameStartTime ) ),
        RecvPropInt( RECVINFO( m_iHostagesRemaining ) ),
		RecvPropBool( RECVINFO( m_bAnyHostageReached ) ),
        RecvPropBool( RECVINFO( m_bMapHasBombTarget ) ),
        RecvPropBool( RECVINFO( m_bMapHasRescueZone ) ),
        RecvPropBool( RECVINFO( m_bMapHasBuyZone ) ),
		RecvPropBool( RECVINFO( m_bIsQueuedMatchmaking ) ),
		RecvPropBool( RECVINFO( m_bIsValveDS ) ),
		RecvPropBool( RECVINFO( m_bIsQuestEligible ) ),
        RecvPropBool( RECVINFO( m_bLogoMap ) ),
		RecvPropBool( RECVINFO( m_bPlayAllStepSoundsOnServer ) ),
        RecvPropInt( RECVINFO( m_iNumGunGameProgressiveWeaponsCT ) ),
        RecvPropInt( RECVINFO( m_iNumGunGameProgressiveWeaponsT ) ),
        RecvPropInt( RECVINFO( m_iSpectatorSlotCount ) ),
        RecvPropBool( RECVINFO( m_bBombDropped ) ),
        RecvPropBool( RECVINFO( m_bBombPlanted ) ),
        RecvPropInt( RECVINFO( m_iRoundWinStatus ) ),
		RecvPropInt( RECVINFO( m_eRoundWinReason ) ),
		RecvPropFloat( RECVINFO( m_flDMBonusStartTime ) ),
		RecvPropFloat( RECVINFO( m_flDMBonusTimeLength ) ),
		RecvPropInt( RECVINFO( m_unDMBonusWeaponLoadoutSlot ) ),
		RecvPropBool( RECVINFO( m_bDMBonusActive ) ),
		RecvPropBool( RECVINFO( m_bTCantBuy ) ),
		RecvPropBool( RECVINFO( m_bCTCantBuy ) ),
		RecvPropFloat( RECVINFO( m_flGuardianBuyUntilTime ) ),	
		RecvPropArray3( RECVINFO_ARRAY( m_iMatchStats_RoundResults ), RecvPropInt( RECVINFO( m_iMatchStats_RoundResults[0] ) ) ),
		RecvPropArray3( RECVINFO_ARRAY( m_iMatchStats_PlayersAlive_T ), RecvPropInt( RECVINFO( m_iMatchStats_PlayersAlive_T[0] ) ) ),
		RecvPropArray3( RECVINFO_ARRAY( m_iMatchStats_PlayersAlive_CT ), RecvPropInt( RECVINFO( m_iMatchStats_PlayersAlive_CT[0] ) ) ),
		RecvPropArray3( RECVINFO_ARRAY( m_GGProgressiveWeaponOrderCT ), RecvPropInt( RECVINFO( m_GGProgressiveWeaponOrderCT[0] ) ) ),
        RecvPropArray3( RECVINFO_ARRAY( m_GGProgressiveWeaponOrderT ), RecvPropInt( RECVINFO( m_GGProgressiveWeaponOrderT[0] ) ) ),
        RecvPropArray3( RECVINFO_ARRAY( m_GGProgressiveWeaponKillUpgradeOrderCT ), RecvPropInt( RECVINFO( m_GGProgressiveWeaponKillUpgradeOrderCT[0] ) ) ),
        RecvPropArray3( RECVINFO_ARRAY( m_GGProgressiveWeaponKillUpgradeOrderT ), RecvPropInt( RECVINFO( m_GGProgressiveWeaponKillUpgradeOrderT[0] ) ) ),
		RecvPropInt( RECVINFO( m_MatchDevice ) ),
        RecvPropBool( RECVINFO( m_bHasMatchStarted ) ),
		RecvPropArray3( RECVINFO_ARRAY(m_TeamRespawnWaveTimes), RecvPropFloat( RECVINFO(m_TeamRespawnWaveTimes[0]) ) ),
		RecvPropArray3( RECVINFO_ARRAY(m_flNextRespawnWave), RecvPropTime( RECVINFO(m_flNextRespawnWave[0]) ) ),
		RecvPropInt( RECVINFO( m_nNextMapInMapgroup ) ),
		RecvPropArray3( RECVINFO_ARRAY( m_nEndMatchMapGroupVoteTypes ), RecvPropInt( RECVINFO( m_nEndMatchMapGroupVoteTypes[0] ) ) ),
		RecvPropArray3( RECVINFO_ARRAY( m_nEndMatchMapGroupVoteOptions ), RecvPropInt( RECVINFO( m_nEndMatchMapGroupVoteOptions[0] ) ) ),
		RecvPropInt( RECVINFO( m_nEndMatchMapVoteWinner ) ),
		RecvPropBool( RECVINFO( m_bIsDroppingItems ) ),
		RecvPropInt( RECVINFO( m_iActiveAssassinationTargetMissionID ) ),
		RecvPropFloat( RECVINFO( m_fMatchStartTime ) ),
		RecvPropString( RECVINFO( m_szTournamentEventName ) ),
		RecvPropString( RECVINFO( m_szTournamentEventStage ) ),
		RecvPropString( RECVINFO( m_szTournamentPredictionsTxt ) ),
		RecvPropInt( RECVINFO( m_nTournamentPredictionsPct ) ),
		RecvPropString( RECVINFO( m_szMatchStatTxt ) ),

		// guardian mode	
		RecvPropInt( RECVINFO( m_nGuardianModeWaveNumber ) ),
		RecvPropInt( RECVINFO( m_nGuardianModeSpecialKillsRemaining ) ),
		RecvPropInt( RECVINFO( m_nGuardianModeSpecialWeaponNeeded ) ),

		// halloween	
		RecvPropInt( RECVINFO( m_nHalloweenMaskListSeed ) ),

		// Gifts global info
		RecvPropInt( RECVINFO( m_numGlobalGiftsGiven ) ),
		RecvPropInt( RECVINFO( m_numGlobalGifters ) ),
		RecvPropInt( RECVINFO( m_numGlobalGiftsPeriodSeconds ) ),
		RecvPropArray3( RECVINFO_ARRAY( m_arrFeaturedGiftersAccounts ), RecvPropInt (RECVINFO( m_arrFeaturedGiftersAccounts[0] ) ) ),
		RecvPropArray3( RECVINFO_ARRAY( m_arrFeaturedGiftersGifts ), RecvPropInt (RECVINFO( m_arrFeaturedGiftersGifts[0] ) ) ),

		RecvPropArray3( RECVINFO_ARRAY( m_arrProhibitedItemIndices ), RecvPropInt( RECVINFO( m_arrProhibitedItemIndices[ 0 ] ) ) ),

		// Tournament Casters
		RecvPropInt( RECVINFO( m_numBestOfMaps ) ),
		RecvPropArray3( RECVINFO_ARRAY( m_arrTournamentActiveCasterAccounts ), RecvPropInt ( RECVINFO( m_arrTournamentActiveCasterAccounts[0] ), 0, CCSGameRules::RecvProxy_TournamentActiveCasterAccounts ) ),

		// Survival
		RecvPropDataTable( RECVINFO_DT( m_SurvivalRules ), 0, &REFERENCE_RECV_TABLE( DT_SurvivalGameRules ) ),

    #else
        SendPropBool( SENDINFO( m_bFreezePeriod ) ),
		SendPropBool( SENDINFO( m_bMatchWaitingForResume ) ),
        SendPropBool( SENDINFO( m_bWarmupPeriod ) ),
        SendPropFloat( SENDINFO( m_fWarmupPeriodEnd ) ), // ONLY INTENDED FOR CLIENTS
        SendPropFloat( SENDINFO( m_fWarmupPeriodStart ) ),	

		SendPropBool( SENDINFO( m_bTerroristTimeOutActive ) ),
		SendPropBool( SENDINFO( m_bCTTimeOutActive ) ),
		SendPropFloat( SENDINFO( m_flTerroristTimeOutRemaining ) ),
		SendPropFloat( SENDINFO( m_flCTTimeOutRemaining ) ),
		SendPropInt( SENDINFO( m_nTerroristTimeOuts ) ),
		SendPropInt( SENDINFO( m_nCTTimeOuts ) ),

        SendPropInt( SENDINFO( m_iRoundTime ), 16 ),
        SendPropInt( SENDINFO( m_gamePhase ), 4, SPROP_UNSIGNED ),
        SendPropInt( SENDINFO( m_totalRoundsPlayed ), 16 ),
		SendPropInt( SENDINFO( m_nOvertimePlaying ), 16 ),
        SendPropFloat( SENDINFO( m_timeUntilNextPhaseStarts ), 32, SPROP_NOSCALE ),
		SendPropFloat( SENDINFO( m_flCMMItemDropRevealStartTime ), 32, SPROP_NOSCALE ),
		SendPropFloat( SENDINFO( m_flCMMItemDropRevealEndTime ), 32, SPROP_NOSCALE ),
        SendPropFloat( SENDINFO( m_fRoundStartTime ), 32, SPROP_NOSCALE ),
        SendPropFloat( SENDINFO( m_flRestartRoundTime ) ),	
        SendPropBool( SENDINFO( m_bGameRestart ) ),
        SendPropFloat( SENDINFO( m_flGameStartTime ), 32, SPROP_NOSCALE ),
        SendPropInt( SENDINFO( m_iHostagesRemaining ), 4 ),
		SendPropBool( SENDINFO( m_bAnyHostageReached ) ),
        SendPropBool( SENDINFO( m_bMapHasBombTarget ) ),
        SendPropBool( SENDINFO( m_bMapHasRescueZone ) ),
        SendPropBool( SENDINFO( m_bMapHasBuyZone ) ),
		SendPropBool( SENDINFO( m_bIsQueuedMatchmaking ) ),
		SendPropBool( SENDINFO( m_bIsValveDS ) ),
		SendPropBool( SENDINFO( m_bIsQuestEligible ) ),
        SendPropBool( SENDINFO( m_bLogoMap ) ),
		SendPropBool( SENDINFO( m_bPlayAllStepSoundsOnServer ) ),
        SendPropInt( SENDINFO( m_iNumGunGameProgressiveWeaponsCT ) ),
        SendPropInt( SENDINFO( m_iNumGunGameProgressiveWeaponsT ) ),
        SendPropInt( SENDINFO( m_iSpectatorSlotCount ) ),
        SendPropBool( SENDINFO( m_bBombDropped ) ),
        SendPropBool( SENDINFO( m_bBombPlanted ) ),
        SendPropInt( SENDINFO( m_iRoundWinStatus ) ),
		SendPropInt( SENDINFO( m_eRoundWinReason ) ),
		SendPropFloat( SENDINFO( m_flDMBonusStartTime ) ),
		SendPropFloat( SENDINFO( m_flDMBonusTimeLength ) ),
		SendPropInt( SENDINFO( m_unDMBonusWeaponLoadoutSlot ) ),
		SendPropBool( SENDINFO( m_bDMBonusActive ) ),
		SendPropBool( SENDINFO( m_bTCantBuy ) ),
		SendPropBool( SENDINFO( m_bCTCantBuy ) ),
		SendPropFloat( SENDINFO( m_flGuardianBuyUntilTime ) ),	
		SendPropArray3( SENDINFO_ARRAY3( m_iMatchStats_RoundResults ), SendPropInt (SENDINFO_ARRAY( m_iMatchStats_RoundResults ), 8, SPROP_UNSIGNED ) ),
		SendPropArray3( SENDINFO_ARRAY3( m_iMatchStats_PlayersAlive_T ), SendPropInt (SENDINFO_ARRAY( m_iMatchStats_PlayersAlive_T ), 6, SPROP_UNSIGNED ) ),
		SendPropArray3( SENDINFO_ARRAY3( m_iMatchStats_PlayersAlive_CT ), SendPropInt (SENDINFO_ARRAY( m_iMatchStats_PlayersAlive_CT ), 6, SPROP_UNSIGNED ) ),
		
        SendPropArray3( SENDINFO_ARRAY3( m_GGProgressiveWeaponOrderCT ), SendPropInt (SENDINFO_ARRAY( m_GGProgressiveWeaponOrderCT ), 0, SPROP_UNSIGNED ) ),
        SendPropArray3( SENDINFO_ARRAY3( m_GGProgressiveWeaponOrderT ), SendPropInt( SENDINFO_ARRAY( m_GGProgressiveWeaponOrderT ), 0, SPROP_UNSIGNED ) ),
        SendPropArray3( SENDINFO_ARRAY3( m_GGProgressiveWeaponKillUpgradeOrderCT ), SendPropInt( SENDINFO_ARRAY( m_GGProgressiveWeaponKillUpgradeOrderCT ), 0, SPROP_UNSIGNED ) ),
        SendPropArray3( SENDINFO_ARRAY3( m_GGProgressiveWeaponKillUpgradeOrderT ), SendPropInt( SENDINFO_ARRAY( m_GGProgressiveWeaponKillUpgradeOrderT ), 0, SPROP_UNSIGNED ) ),
		SendPropInt( SENDINFO( m_MatchDevice ) ),
        SendPropBool( SENDINFO( m_bHasMatchStarted ) ),
		SendPropArray3( SENDINFO_ARRAY3(m_TeamRespawnWaveTimes), SendPropFloat( SENDINFO_ARRAY(m_TeamRespawnWaveTimes) ) ),
		SendPropArray3( SENDINFO_ARRAY3(m_flNextRespawnWave), SendPropTime( SENDINFO_ARRAY(m_flNextRespawnWave) ) ),
		SendPropInt( SENDINFO( m_nNextMapInMapgroup ) ),
		SendPropArray3( SENDINFO_ARRAY3( m_nEndMatchMapGroupVoteTypes ), SendPropInt (SENDINFO_ARRAY( m_nEndMatchMapGroupVoteTypes ) ) ),
		SendPropArray3( SENDINFO_ARRAY3( m_nEndMatchMapGroupVoteOptions ), SendPropInt (SENDINFO_ARRAY( m_nEndMatchMapGroupVoteOptions ) ) ),
		SendPropInt( SENDINFO( m_nEndMatchMapVoteWinner ) ),
		SendPropBool( SENDINFO( m_bIsDroppingItems ) ),
		SendPropInt( SENDINFO( m_iActiveAssassinationTargetMissionID ) ),
		SendPropFloat( SENDINFO( m_fMatchStartTime ), 32, SPROP_NOSCALE ),
		SendPropString( SENDINFO( m_szTournamentEventName ) ),
		SendPropString( SENDINFO( m_szTournamentEventStage ) ),
		SendPropString( SENDINFO( m_szTournamentPredictionsTxt ) ),
		SendPropInt( SENDINFO( m_nTournamentPredictionsPct ) ),
		SendPropString( SENDINFO( m_szMatchStatTxt ) ),

		// guardian mode	
		SendPropInt( SENDINFO( m_nGuardianModeWaveNumber ) ),
		SendPropInt( SENDINFO( m_nGuardianModeSpecialKillsRemaining ) ),
		SendPropInt( SENDINFO( m_nGuardianModeSpecialWeaponNeeded ) ),

		// Halloween	
		SendPropInt( SENDINFO( m_nHalloweenMaskListSeed ) ),

		// Gifts global info
		SendPropInt( SENDINFO( m_numGlobalGiftsGiven ), 0, SPROP_UNSIGNED ),
		SendPropInt( SENDINFO( m_numGlobalGifters ), 0, SPROP_UNSIGNED ),
		SendPropInt( SENDINFO( m_numGlobalGiftsPeriodSeconds ), 0, SPROP_UNSIGNED ),
		SendPropArray3( SENDINFO_ARRAY3( m_arrFeaturedGiftersAccounts ), SendPropInt (SENDINFO_ARRAY( m_arrFeaturedGiftersAccounts ), 0, SPROP_UNSIGNED ) ),
		SendPropArray3( SENDINFO_ARRAY3( m_arrFeaturedGiftersGifts ), SendPropInt (SENDINFO_ARRAY( m_arrFeaturedGiftersGifts ), 0, SPROP_UNSIGNED ) ),

		SendPropArray3( SENDINFO_ARRAY3( m_arrProhibitedItemIndices ), SendPropInt( SENDINFO_ARRAY( m_arrProhibitedItemIndices ), 0, SPROP_UNSIGNED ) ),

		// Tournament Casters
		SendPropInt( SENDINFO( m_numBestOfMaps ), 4, SPROP_UNSIGNED ), // supporting no more than best-of-7 (1+2+4)
		SendPropArray3( SENDINFO_ARRAY3( m_arrTournamentActiveCasterAccounts ), SendPropInt (SENDINFO_ARRAY( m_arrTournamentActiveCasterAccounts ), 0, SPROP_UNSIGNED ) ),	

		// Survival
		SendPropDataTable( SENDINFO_DT( m_SurvivalRules ), &REFERENCE_SEND_TABLE( DT_SurvivalGameRules ), SendProxy_SurvivalGameRules ),
    #endif
END_NETWORK_TABLE()


IMPLEMENT_NETWORKCLASS_ALIASED( CSGameRulesProxy, DT_CSGameRulesProxy )
LINK_ENTITY_TO_CLASS_ALIASED( cs_gamerules, CSGameRulesProxy );

#ifdef CLIENT_DLL
void CCSGameRulesProxy::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( CSGameRules() )
		CSGameRules()->OnDataChanged( updateType );
}

void CCSGameRules::OnDataChanged( DataUpdateType_t updateType )
{
	if ( GetSurvivalRules() )
		GetSurvivalRules()->OnDataChanged( updateType );
}
#endif

#ifdef GAME_DLL
ConVar mp_teamname_1( "mp_teamname_1", "", FCVAR_RELEASE, "A non-empty string overrides the first team's name." );
ConVar mp_teamname_2( "mp_teamname_2", "", FCVAR_RELEASE, "A non-empty string overrides the second team's name." );

ConVar mp_teamflag_1( "mp_teamflag_1", "", FCVAR_RELEASE, "Enter a country's alpha 2 code to show that flag next to team 1's name in the spectator scoreboard." );
ConVar mp_teamflag_2( "mp_teamflag_2", "", FCVAR_RELEASE, "Enter a country's alpha 2 code to show that flag next to team 2's name in the spectator scoreboard." );

ConVar mp_teamlogo_1( "mp_teamlogo_1", "", FCVAR_RELEASE, "Enter a team's shorthand image name to display their logo. Images can be found here: 'resource/flash/econ/tournaments/teams'" );
ConVar mp_teamlogo_2( "mp_teamlogo_2", "", FCVAR_RELEASE, "Enter a team's shorthand image name to display their logo. Images can be found here: 'resource/flash/econ/tournaments/teams'" );

ConVar mp_teamprediction_txt( "mp_teamprediction_txt", "#SFUIHUD_Spectate_Predictions", FCVAR_RELEASE, "A value between 1 and 99 will set predictions in favor of first team." );
ConVar mp_teamprediction_pct( "mp_teamprediction_pct", "0", FCVAR_RELEASE, "A value between 1 and 99 will show predictions in favor of CT team." );

ConVar mp_teammatchstat_txt( "mp_teammatchstat_txt", "", FCVAR_RELEASE, "A non-empty string sets the match stat description, e.g. 'Match 2 of 3'." );
ConVar mp_teammatchstat_1( "mp_teammatchstat_1", "", FCVAR_RELEASE, "A non-empty string sets first team's match stat." );
ConVar mp_teammatchstat_2( "mp_teammatchstat_2", "", FCVAR_RELEASE, "A non-empty string sets second team's match stat." );

ConVar mp_teamscore_1( "mp_teamscore_1", "", FCVAR_RELEASE, "A non-empty string for best-of-N maps won by the first team." );
ConVar mp_teamscore_2( "mp_teamscore_2", "", FCVAR_RELEASE, "A non-empty string for best-of-N maps won by the second team." );
void FnChangeCallback_mp_teamscore_max( IConVar *var, const char *pOldValue, float flOldValue )
{
	ConVarRef ref( var );
	int nVal = ref.GetInt();
	if ( CCSGameRules *pCSR = CSGameRules() )
	{
		if ( nVal != pCSR->m_numBestOfMaps )
			pCSR->m_numBestOfMaps = nVal;
	}
}
ConVar mp_teamscore_max( "mp_teamscore_max", "0", FCVAR_RELEASE, "How many maps to win the series (bo3 max=2; bo5 max=3; bo7 max=4)", true, 0, true, 7, FnChangeCallback_mp_teamscore_max );

ConVar mp_teammatchstat_holdtime( "mp_teammatchstat_holdtime", "5", FCVAR_RELEASE, "Decide on a match stat and hold it additionally for at least so many seconds" );
ConVar mp_teammatchstat_cycletime( "mp_teammatchstat_cycletime", "45", FCVAR_RELEASE, "Cycle match stats after so many seconds" );
#endif

// Heavy Assault Suit convars
ConVar mp_heavyassaultsuit_speed( "mp_heavyassaultsuit_speed", "130", FCVAR_REPLICATED | FCVAR_RELEASE, "The max speed of a player when they are wearing the heavy assault suit" );
ConVar mp_heavybot_damage_reduction_scale( "mp_heavybot_damage_reduction_scale", "1.0", FCVAR_REPLICATED | FCVAR_RELEASE, "How much damage should scale when the player wearing the heavy assault suit is shot (1.0 = no change, 0.5 = half damage)" );
ConVar mp_heavyassaultsuit_deploy_timescale( "mp_heavyassaultsuit_deploy_timescale", "0.8", FCVAR_REPLICATED | FCVAR_RELEASE, "How fast a player wearing the heavy assault suit will draw their weapon (1.0 = normal speed, 0.5 = half speed)" );
ConVar mp_heavyassaultsuit_aimpunch( "mp_heavyassaultsuit_aimpunch", "1.0", FCVAR_REPLICATED | FCVAR_RELEASE, "How much EXTRA aim punch will happen when a player wearing the assault suit gets when shot" );

// COOP
#define COOPMISSION_SCORE_MULTIPLIER_TIMELEFT 40
#define COOPMISSION_SCORE_MULTIPLIER_DAMTAKEN -10
#define COOPMISSION_SCORE_MULTIPLIER_ROUNDSFAILED -1000

static uint32 Helper_ScoreLeaderboardData_FindEntryValue( uint32 nTag, const ::google::protobuf::RepeatedPtrField< ::ScoreLeaderboardData_Entry >&arr )
{
	for ( int i = 0; i < arr.size(); ++ i )
	{
		if ( arr.Get( i ).tag() == nTag )
			return arr.Get( i ).val();
	}
	return 0;
}
static uint32 Helper_ScoreLeaderboardData_FindEntryValueSum( uint32 nTag, const ::google::protobuf::RepeatedPtrField< ::ScoreLeaderboardData_AccountEntries >&arr )
{
	uint32 val = 0;
	for ( int i = 0; i < arr.size(); ++i )
	{
		val += Helper_ScoreLeaderboardData_FindEntryValue( nTag, arr.Get( i ).entries() );
	}
	return val;
}

int32 CoopScoreGetRatingEntryFromLeaderboardData( ScoreLeaderboardData &sld, bool bBonus, int nIndex, bool bAsScore )
{	// Note: this function should not be referencing gamerules because we can be looking at scores from Steam friends leaderboards
	bool bGuardian = false;
	bool bCoopMission = true;
	if ( sld.quest_id() )
	{
		const CEconQuestDefinition *pQuest = GetItemSchema()->GetQuestDefinition( sld.quest_id() );
		if ( !pQuest )
			return 0.0f;
		if ( !V_stricmp( pQuest->GetGameMode(), "cooperative" ) )
		{
			bGuardian = true;
			bCoopMission = false;
		}
		// otherwise assume coopmission
	}
	else if ( CSGameRules() )
	{	// keeping this around for local compatibility testing with listenservers
		bGuardian = CSGameRules()->IsPlayingCoopGuardian();
		bCoopMission = CSGameRules()->IsPlayingCoopMission();
		// otherwise assume coopmission
	}
	else
		return 0.0f;

	int32 nResult = 0;

	//
	// Base scorechart
	//
	if ( !bBonus )
	{

		if ( bGuardian )
		{
			if ( nIndex == 0 )
			{	// Damage to enemies ratio
				uint32 numDmgInflicted = Helper_ScoreLeaderboardData_FindEntryValueSum( k_EScoreLeaderboardDataEntryTag_HpDmgInflicted, sld.accountentries() );
				uint32 numDmgSuffered = Helper_ScoreLeaderboardData_FindEntryValueSum( k_EScoreLeaderboardDataEntryTag_HpDmgSuffered, sld.accountentries() );
				if ( !numDmgInflicted )
					nResult = 0;
				else if ( numDmgSuffered )
				{
					nResult = int( 10000.0f * float( numDmgInflicted ) / float( numDmgInflicted + numDmgSuffered ) );
					if ( numDmgInflicted && ( nResult <= 0 ) )
						nResult = 1;
				}
				else
					nResult = 10000;
				//if ( bAsScore )
				//{
				//	nResult = nResult;
				//}
			}
			else if ( nIndex == 3 )
			{	// Rounds failed penalty
				nResult = Helper_ScoreLeaderboardData_FindEntryValue( k_EScoreLeaderboardDataEntryTag_RoundsPlayed, sld.matchentries() );
				nResult = ( nResult > 1 ) ? ( nResult - 1 ) : 0;
				if ( bAsScore )
				{
					nResult = COOPMISSION_SCORE_MULTIPLIER_ROUNDSFAILED*nResult;
				}
			}
		}
		else
		{
			if ( nIndex == 0 )
			{	// Time Remaining on the clock
				nResult = Helper_ScoreLeaderboardData_FindEntryValue( k_EScoreLeaderboardDataEntryTag_TimeRemaining, sld.matchentries() );
				if ( bAsScore )
				{
					nResult = COOPMISSION_SCORE_MULTIPLIER_TIMELEFT*nResult;
				}
			}
			else if ( nIndex == 3 )
			{	// Total damage taken
				nResult = Helper_ScoreLeaderboardData_FindEntryValueSum( k_EScoreLeaderboardDataEntryTag_HpDmgSuffered, sld.accountentries() );
				if ( bAsScore )
				{
					nResult = COOPMISSION_SCORE_MULTIPLIER_DAMTAKEN*nResult;
				}
			}
		}

		if ( bGuardian || bCoopMission )
		{	// Shared categories for Guardian and Coop Mission
			if ( nIndex == 1 )
			{	// Bullets Accuracy
				uint32 numBulletsFired = Helper_ScoreLeaderboardData_FindEntryValueSum( k_EScoreLeaderboardDataEntryTag_ShotsFired, sld.accountentries() );
				uint32 numBulletsOnTarget = Helper_ScoreLeaderboardData_FindEntryValueSum( k_EScoreLeaderboardDataEntryTag_ShotsOnTarget, sld.accountentries() );
				if ( !numBulletsFired )
					nResult = 0;
				else if ( numBulletsFired > 0 && numBulletsOnTarget < numBulletsFired )
				{
					nResult = int( 10000.0f * float( numBulletsOnTarget ) / float( numBulletsFired ) );
					if ( numBulletsOnTarget && ( nResult <= 0 ) )
						nResult = 1;
				}
				else
					nResult = 10000;
				//if ( bAsScore )
				//{
				//	nResult = nResult;
				//}
			}
			else if ( nIndex == 2 )
			{	// Headshots kill percentage
				uint32 numHeadshots = Helper_ScoreLeaderboardData_FindEntryValueSum( k_EScoreLeaderboardDataEntryTag_Headshots, sld.accountentries() );
				uint32 numKills = Helper_ScoreLeaderboardData_FindEntryValueSum( k_EScoreLeaderboardDataEntryTag_Kills, sld.accountentries() );
				if ( !numKills )
					nResult = 0;
				else if ( numKills > 0 && numHeadshots < numKills )
				{
					nResult = int( 10000.0f * float( numHeadshots ) / float( numKills ) );
					if ( numHeadshots && ( nResult <= 0 ) )
						nResult = 1;
				}
				else
					nResult = 10000;
				//if ( bAsScore )
				//{
				//	nResult = nResult;
				//}
			}
		}

	}
	//
	// Scorechart for bonuses!
	//
	else
	{

		if ( bGuardian )
		{
			if ( nIndex == 0 )
			{	// Under 3 rounds?
				uint32 numRoundsPlayed = Helper_ScoreLeaderboardData_FindEntryValue( k_EScoreLeaderboardDataEntryTag_RoundsPlayed, sld.matchentries() );
				nResult = numRoundsPlayed;
				if ( bAsScore )
				{
					nResult = ( numRoundsPlayed <= 3 ) ? 5000 : 0;
				}
			}
		}

		if ( bCoopMission )
		{
			if ( nIndex == 0 )
			{	// No Deaths?
				uint32 numDeaths = Helper_ScoreLeaderboardData_FindEntryValueSum( k_EScoreLeaderboardDataEntryTag_Deaths, sld.accountentries() );
				nResult = numDeaths;
				if ( bAsScore )
				{
					nResult = ( numDeaths == 0 ) ? 5000 : 0;
				}
			}
			else if ( nIndex == 1 )
			{	// All Challenge Coins?
				uint32 numChallenge = Helper_ScoreLeaderboardData_FindEntryValue( k_EScoreLeaderboardDataEntryTag_BonusChallenge, sld.matchentries() );
				nResult = numChallenge;
				if ( bAsScore )
				{
					nResult = ( numChallenge ) ? 5000 : 0;
				}
			}
			else if ( nIndex == 2 )
			{	// Pistols Only?
				uint32 numPistolsOnly = Helper_ScoreLeaderboardData_FindEntryValue( k_EScoreLeaderboardDataEntryTag_BonusPistolOnly, sld.matchentries() );
				nResult = numPistolsOnly;
				if ( bAsScore )
				{
					nResult = ( numPistolsOnly ) ? 10000 : 0;
				}
			}
			else if ( nIndex == 3 )
			{	// Hard Mode?
				uint32 numHardMode = Helper_ScoreLeaderboardData_FindEntryValue( k_EScoreLeaderboardDataEntryTag_BonusHardMode, sld.matchentries() );
				nResult = numHardMode;
				if ( bAsScore )
				{
					nResult = ( numHardMode ) ? 25000 : 0;
				}
			}
		}

	}

	return nResult;
}



#ifndef CLIENT_DLL
ConVar mp_backup_round_auto( "mp_backup_round_auto", "1", FCVAR_RELEASE, "If enabled will keep in-memory backups to handle reconnecting players even if the backup files aren't written to disk" );
ConVar mp_backup_round_file( "mp_backup_round_file", "backup", FCVAR_RELEASE, "If set then server will save all played rounds information to files filename_date_time_team1_team2_mapname_roundnum_score1_score2.txt" );
ConVar mp_backup_round_file_pattern( "mp_backup_round_file_pattern", "%prefix%_round%round%.txt", FCVAR_RELEASE, "If set then server will save all played rounds information to files named by this pattern, e.g.'%prefix%_%date%_%time%_%team1%_%team2%_%map%_round%round%_score_%score1%_%score2%.txt'" );
ConVar mp_backup_round_file_last( "mp_backup_round_file_last", "", FCVAR_RELEASE, "Every time a backup file is written the value of this convar gets updated to hold the name of the backup file." );

static bool Helper_mp_backup_round_IsEnabled()
{
	if ( g_pGameTypes->GetCurrentGameType() == CS_GameType_GunGame )
	{
		switch ( g_pGameTypes->GetCurrentGameMode() )
		{
		case CS_GameMode::GunGame_Progressive:
		case CS_GameMode::GunGame_Bomb:
			return !!*mp_backup_round_file.GetString();	// round backups aren't supported in 'auto' mode in ArmsRace/Demolition, but we'll honor server config to write backup file
		}
	}
	return mp_backup_round_auto.GetBool() || *mp_backup_round_file.GetString();
}

static int Helper_mp_backup_restore_from_file_sorter( char const * const *a, char const * const *b )
{
	return -Q_stricmp( *a, *b ); // sort more recent files earlier
}


static void Helper_FilenameTokenReplace( char* pchBufFilename, int nSize, const char *aReplacePattern, const char *theActualValue )
{
	char const *pchReplacePattern, *pchActualValue;

	char const *pchEndFilename = ( char * ) pchBufFilename + nSize;

	pchReplacePattern = aReplacePattern;
	pchActualValue = theActualValue;

	while ( char *pchReplace = Q_strstr( pchBufFilename, pchReplacePattern ) )
	{
		int lenReplace = Q_strlen( pchReplacePattern ), lenValue = Q_strlen( pchActualValue );

		if ( pchReplace + lenValue >= pchEndFilename )
		{
			break;
		}
	
		Q_memmove( pchReplace + lenValue, pchReplace + lenReplace, pchEndFilename - pchReplace - MAX( lenReplace, lenValue ) );
		Q_memmove( pchReplace, pchActualValue, lenValue );

	}
}

static bool Helper_ShouldBroadcastCoopScoreLeaderboardData()
{
	if ( !CSGameRules() )
		return false;
	if ( CSGameRules()->IsPlayingCoopGuardian() || CSGameRules()->IsPlayingCoopMission() )
	{
		// Check if the humans won the match
		int numHumanRoundsWon = CSGameRules()->m_match.GetTeamScore( CSGameRules()->GetCoopHumanTeamNumber() );
		if ( numHumanRoundsWon > 0 )
			return true;
	}
	return false;
}

static void Helper_FillScoreLeaderboardData( ScoreLeaderboardData &sld )
{
	//
	// This function is used in both official and community server build
	// In official build it will deliver the leaderboard data to GC
	// in both official and community build this data is also replicated to clients for the end of match scoreboard
	//

	if ( !Helper_ShouldBroadcastCoopScoreLeaderboardData() )
		return;

	//
	// Per player stats
	//
	FOR_EACH_MAP( CSGameRules()->m_mapQueuedMatchmakingPlayersData, i )
	{
		CCSGameRules::CQMMPlayerData_t const &qmm = *CSGameRules()->m_mapQueuedMatchmakingPlayersData.Element( i );
		ScoreLeaderboardData_AccountEntries *pAcc = sld.add_accountentries();
		pAcc->set_accountid( qmm.m_uiPlayerAccountId );
		if ( int n = qmm.m_numEnemyKills )
		{
			ScoreLeaderboardData_Entry *pEnt = pAcc->add_entries();
			pEnt->set_tag( k_EScoreLeaderboardDataEntryTag_Kills );
			pEnt->set_val( n );
		}
		if ( int n = qmm.m_numEnemyKillHeadshots )
		{
			ScoreLeaderboardData_Entry *pEnt = pAcc->add_entries();
			pEnt->set_tag( k_EScoreLeaderboardDataEntryTag_Headshots );
			pEnt->set_val( n );
		}
		if ( int n = qmm.m_numDeaths )
		{
			ScoreLeaderboardData_Entry *pEnt = pAcc->add_entries();
			pEnt->set_tag( k_EScoreLeaderboardDataEntryTag_Deaths );
			pEnt->set_val( n );
		}
		if ( int n = qmm.m_numHealthPointsRemovedTotal )
		{
			ScoreLeaderboardData_Entry *pEnt = pAcc->add_entries();
			pEnt->set_tag( k_EScoreLeaderboardDataEntryTag_HpDmgSuffered );
			pEnt->set_val( n );
		}
		if ( int n = qmm.m_numHealthPointsDealtTotal )
		{
			ScoreLeaderboardData_Entry *pEnt = pAcc->add_entries();
			pEnt->set_tag( k_EScoreLeaderboardDataEntryTag_HpDmgInflicted );
			pEnt->set_val( n );
		}
		if ( int n = qmm.m_numShotsFiredTotal )
		{
			ScoreLeaderboardData_Entry *pEnt = pAcc->add_entries();
			pEnt->set_tag( k_EScoreLeaderboardDataEntryTag_ShotsFired );
			pEnt->set_val( n );
		}
		if ( int n = qmm.m_numShotsOnTargetTotal )
		{
			ScoreLeaderboardData_Entry *pEnt = pAcc->add_entries();
			pEnt->set_tag( k_EScoreLeaderboardDataEntryTag_ShotsOnTarget );
			pEnt->set_val( n );
		}
	}

	//
	// Match stats
	//
	if ( int n = CSGameRules()->GetTotalRoundsPlayed() )
	{
		ScoreLeaderboardData_Entry *pEnt = sld.add_matchentries();
		pEnt->set_tag( k_EScoreLeaderboardDataEntryTag_RoundsPlayed );
		pEnt->set_val( n );
	}

	if ( CSGameRules()->IsPlayingCoopMission() )
	{
		int nRemainingTime = CSGameRules()->GetRoundRemainingTime();
		if ( nRemainingTime < 0 )
			nRemainingTime = 0;
		if ( int n = nRemainingTime )
		{
			ScoreLeaderboardData_Entry *pEnt = sld.add_matchentries();
			pEnt->set_tag( k_EScoreLeaderboardDataEntryTag_TimeRemaining );
			pEnt->set_val( n );
		}

		static ConVarRef mp_coopmission_bot_difficulty_offset( "mp_coopmission_bot_difficulty_offset" );
		int nHardMode = ( mp_coopmission_bot_difficulty_offset.GetInt() >= 3 ) ? 1 : 0;
		if ( int n = nHardMode )
		{
			ScoreLeaderboardData_Entry *pEnt = sld.add_matchentries();
			pEnt->set_tag( k_EScoreLeaderboardDataEntryTag_BonusHardMode );
			pEnt->set_val( n );
		}

		if ( int n = CSGameRules()->m_coopBonusPistolsOnly ? 1 : 0 )
		{
			ScoreLeaderboardData_Entry *pEnt = sld.add_matchentries();
			pEnt->set_tag( k_EScoreLeaderboardDataEntryTag_BonusPistolOnly );
			pEnt->set_val( n );
		}

		if ( int n = ( CSGameRules()->m_coopBonusCoinsFound == 3 ) ? 1 : 0 )
		{
			ScoreLeaderboardData_Entry *pEnt = sld.add_matchentries();
			pEnt->set_tag( k_EScoreLeaderboardDataEntryTag_BonusChallenge );
			pEnt->set_val( n );
		}
	}


	//
	// Set the final score and the questid if applicable
	//
	int32 nTotalScore = 0;
	for ( int nBonus = 0; nBonus <= 1; ++ nBonus )
	{
		int32 nScoreTier = 0;
		for ( int iCategory = 0; iCategory < 5; ++ iCategory )
		{
			nScoreTier += CoopScoreGetRatingEntryFromLeaderboardData( sld, !!nBonus, iCategory, true );
		}
		if ( nScoreTier < 0 )
			nScoreTier = 0;
		nTotalScore += nScoreTier;
	}
	sld.set_score( nTotalScore );
}

// Checks basic conditions for a quest (mapgroup, mode, etc) to see if a quest is possible to complete
bool Helper_CheckQuestMapAndMode( const CEconQuestDefinition *pQuest )
{
	const char *szMapName = NULL;
	const char *szMapGroupName = NULL;
#if defined ( CLIENT_DLL )
	szMapName = engine->GetLevelNameShort();
	szMapGroupName = engine->GetMapGroupName();
#else
	szMapName = V_UnqualifiedFileName( STRING( gpGlobals->mapname ) );
	szMapGroupName = STRING( gpGlobals->mapGroupName );
#endif
	// Wrong map
	if ( !StringIsEmpty( pQuest->GetMap() ) && V_strcmp( szMapName, pQuest->GetMap() ) )
		return false;

	// Unless the map group is named after our map (so queued for a single map) also confirm we're using the right map group
	if ( V_strcmp( szMapGroupName, CFmtStr( "mg_%s", szMapName ) ) )
	{
		if ( !StringIsEmpty( pQuest->GetMapGroup() ) && V_strcmp( szMapGroupName, pQuest->GetMapGroup() ) )
		{
			return false;
		}
	}

	const char *szCurrentModeAsString = g_pGameTypes->GetGameModeFromInt( g_pGameTypes->GetCurrentGameType(), g_pGameTypes->GetCurrentGameMode() );
	// Mode doesn't match
	if ( V_strcmp( pQuest->GetGameMode(), szCurrentModeAsString ) )
		return false;

	return true;
}


bool IsAssassinationQuestActive( const CEconQuestDefinition *pQuest )
{
	if ( CSGameRules() && CSGameRules()->IsWarmupPeriod() )
		return false;

	// We need to have an active quest with the 'act_kill_target' requirement
	if ( !pQuest || !IsAssassinationQuest( pQuest ) )
		return false;

	if ( !Helper_CheckQuestMapAndMode( pQuest ) )
		return false;

	return true;
}



#if BACKUPSUPPORTZEROZERO
static char const * const g_szRoundBackupZeroZero = "0:0";
static char const * const g_szRoundBackupZeroZeroFileName = "00restart";
#endif
class CBackupFilesEnumerator : public CUtlVector< char const * >
{
public:
	CBackupFilesEnumerator()
	{
		if ( !mp_backup_round_file.GetString()[0] )
		{
			Warning( "Current backup file prefix is not set, use mp_backup_round_file to set it.\n" );
			return;
		}

		Warning( "Listing backup files with prefix: %s\n", mp_backup_round_file.GetString() );

		FileFindHandle_t hFind = NULL;

		CUtlBuffer bufFilename;

		int nBufSize = 1024;

		bufFilename.EnsureCapacity( nBufSize );
		char *pchBufFilename = ( char * ) bufFilename.Base();

		Q_memset( pchBufFilename, 0, nBufSize );
		Q_snprintf( pchBufFilename, nBufSize, "%s", mp_backup_round_file_pattern.GetString() );

		Helper_FilenameTokenReplace( pchBufFilename, nBufSize, "%prefix%", mp_backup_round_file.GetString() );
		Helper_FilenameTokenReplace( pchBufFilename, nBufSize, "%date%", "*" );
		Helper_FilenameTokenReplace( pchBufFilename, nBufSize, "%time%", "*" );
		Helper_FilenameTokenReplace( pchBufFilename, nBufSize, "%round%", "*" );
		Helper_FilenameTokenReplace( pchBufFilename, nBufSize, "%score1%", "*" );
		Helper_FilenameTokenReplace( pchBufFilename, nBufSize, "%score2%", "*" );
		

		char chTeam1[64] = {0};
		Q_snprintf( chTeam1, sizeof( chTeam1 ), "%s", GetGlobalTeam( CSGameRules()->AreTeamsPlayingSwitchedSides() ? TEAM_TERRORIST : TEAM_CT )->GetClanName() );
		for ( char *pch = chTeam1; *pch; ++ pch )
		{
			if (
				( ( pch[0] >= 'a' ) && ( pch[0] <= 'z' ) ) ||
				( ( pch[0] >= 'A' ) && ( pch[0] <= 'Z' ) ) ||
				( ( pch[0] >= '0' ) && ( pch[0] <= '9' ) )
				)
				;
			else
			{
				Q_memmove( pch, pch + 1, sizeof( chTeam1 ) - ( pch - chTeam1 ) - 1 );
				-- pch;
			}
		}
		char chTeam2[64] = {0};
		Q_snprintf( chTeam2, sizeof( chTeam2 ), "%s", GetGlobalTeam( CSGameRules()->AreTeamsPlayingSwitchedSides() ? TEAM_CT : TEAM_TERRORIST )->GetClanName() );
		for ( char *pch = chTeam2; *pch; ++ pch )
		{
			if (
				( ( pch[0] >= 'a' ) && ( pch[0] <= 'z' ) ) ||
				( ( pch[0] >= 'A' ) && ( pch[0] <= 'Z' ) ) ||
				( ( pch[0] >= '0' ) && ( pch[0] <= '9' ) )
				)
				;
			else
			{
				Q_memmove( pch, pch + 1, sizeof( chTeam2 ) - ( pch - chTeam2 ) - 1 );
				-- pch;
			}
		}

		Helper_FilenameTokenReplace( pchBufFilename, nBufSize, "%team1%", chTeam1 );
		Helper_FilenameTokenReplace( pchBufFilename, nBufSize, "%team2%", chTeam2 );
		{	// Map might have a path in it, make sure we strip it
			const char *szMapNameToken = STRING( gpGlobals->mapname );
			if ( const char *pchSlash = strrchr( szMapNameToken, '/' ) )
				szMapNameToken = pchSlash + 1;
			if ( const char *pchSlash2 = strrchr( szMapNameToken, '\\' ) )
				szMapNameToken = pchSlash2 + 1;
			Helper_FilenameTokenReplace( pchBufFilename, nBufSize, "%map%", szMapNameToken );
		}

		for ( char const *szFileName = filesystem->FindFirst( pchBufFilename, &hFind );
			szFileName && *szFileName; szFileName = filesystem->FindNext( hFind ) )
		{
			m_mapFiles.AddString( szFileName );
		}

		filesystem->FindClose( hFind );

		for ( int iStr = 0; iStr < m_mapFiles.GetNumStrings(); ++ iStr )
		{
			AddToTail( m_mapFiles.String( iStr ) );
		}

		Sort( Helper_mp_backup_restore_from_file_sorter );

		#if BACKUPSUPPORTZEROZERO
		// add 0:0 backup
		static char const * s_pchTournamentServer = CommandLine()->ParmValue( "-tournament", ( char const * ) NULL );
		if ( s_pchTournamentServer )
			AddToTail( g_szRoundBackupZeroZeroFileName );
		#endif
	}

private:
	CUtlStringMap< bool > m_mapFiles;
};


#if defined( _DEBUG )
	#define ROUND_BACKUP_REQUEST_RATE_LIMIT 5
#else
	#define ROUND_BACKUP_REQUEST_RATE_LIMIT 10
#endif

// concommand that requests a user message with the round backup filenames in it.
CON_COMMAND_F ( send_round_backup_file_list, "", FCVAR_GAMEDLL | FCVAR_RELEASE |FCVAR_HIDDEN|FCVAR_GAMEDLL_FOR_REMOTE_CLIENTS )
{

	CBasePlayer* pPlayer = UTIL_GetCommandClient();
	if ( !pPlayer )
		return;

	static CUtlMap<int, int > s_mapLastRequestTime;
	if ( s_mapLastRequestTime.Count() == 0 )
	{
		s_mapLastRequestTime.SetLessFunc( DefLessFunc( int ) );
	}

	// prevent spamming of the command. store each user's previous request time.

	int nIndex = s_mapLastRequestTime.Find( pPlayer->entindex() );

	if 	( ( s_mapLastRequestTime.IsValidIndex( nIndex ) ) &&
		( gpGlobals->curtime > s_mapLastRequestTime[ nIndex ] ) &&
		( ( gpGlobals->curtime - s_mapLastRequestTime[ nIndex ] ) <= ROUND_BACKUP_REQUEST_RATE_LIMIT ) )
		return;

	s_mapLastRequestTime.InsertOrReplace( pPlayer->entindex(), ( int )gpGlobals->curtime );

	CBackupFilesEnumerator arrStrings;

	// to avoid the proto message limit we're sending these in individual messages rather than one.
	for ( int idx = 0; idx < Min( 10, arrStrings.Count() ); idx++ )
	{
		CCSUsrMsg_RoundBackupFilenames msg;

		msg.set_count( Min( 10, arrStrings.Count() ) );

		msg.set_index( idx );

		msg.set_filename( arrStrings[ idx ] );

		// create human readable name
		KeyValues *kvSaveFile = new KeyValues( "" );
		KeyValues::AutoDelete autodelete_kvSaveFile( kvSaveFile );
		autodelete_kvSaveFile->UsesEscapeSequences( true );

		CUtlString nicefilename = arrStrings[ idx ];

		#if BACKUPSUPPORTZEROZERO
		if ( !V_strcmp( arrStrings[ idx ], g_szRoundBackupZeroZeroFileName ) )
		{
			nicefilename = g_szRoundBackupZeroZero;
		}
		else
		#endif
		if ( kvSaveFile->LoadFromFile( filesystem, arrStrings[ idx ] ) )
		{
			nicefilename = CFmtStr( "%s score %d:%d",
				kvSaveFile->GetString( "timestamp" ),
				kvSaveFile->GetInt( "FirstHalfScore/team1" ) + kvSaveFile->GetInt( "SecondHalfScore/team1" ) + kvSaveFile->GetInt( "OvertimeScore/team1" ),
				kvSaveFile->GetInt( "FirstHalfScore/team2" ) + kvSaveFile->GetInt( "SecondHalfScore/team2" ) + kvSaveFile->GetInt( "OvertimeScore/team2" ));
		}

		msg.set_nicename( nicefilename );

		CSingleUserRecipientFilter filter( pPlayer );

		SendUserMessage( filter, CS_UM_RoundBackupFilenames, msg );
	}


}

CON_COMMAND_F ( mp_backup_restore_list_files, "Lists recent backup round files matching the prefix, most recent files first, accepts a numeric parameter to limit the number of files displayed", FCVAR_GAMEDLL | FCVAR_RELEASE )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	int numFilesToList = 5;
	if ( args.ArgC() >= 2 )
	{
		numFilesToList = Q_atoi( args.Arg( 1 ) );
	}

	CBackupFilesEnumerator arrStrings;

	FOR_EACH_VEC( arrStrings, idxString )
	{
		if ( idxString >= numFilesToList )
			break;
		Warning( " %s\n", arrStrings[idxString] );
	}
	if ( !arrStrings.Count() )
	{
		Warning( "No matching backup files found.\n" );
	}
	else if ( numFilesToList < arrStrings.Count() )
	{
		Warning( "%d backup files found, %d most recent listed, use 'mp_backup_restore_list_files %d' to list all.\n", arrStrings.Count(), numFilesToList, arrStrings.Count() );
	}
	else
	{
		Warning( "%d backup files listed.\n", arrStrings.Count() );
	}

}


ConVar mp_backup_restore_load_autopause( "mp_backup_restore_load_autopause", "1", FCVAR_RELEASE, "Whether to automatically pause the match after restoring round data from backup" );
CON_COMMAND_F ( mp_backup_restore_load_file, "Loads player cash, KDA, scores and team scores; resets to the next round after the backup", FCVAR_GAMEDLL | FCVAR_RELEASE )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	if ( args.ArgC() < 2 )
	{
		Warning( "Usage: mp_backup_restore_load_file [filename], use 'mp_backup_restore_list_files' to list matching backup files.\n" );
		return;
	}
	CSGameRules()->LoadRoundDataInformation( args.Arg( 1 ) );
}

CMsgGCCStrike15_v2_MatchmakingGC2ServerReserve CCSGameRules::sm_QueuedServerReservation;
#endif

#ifdef CLIENT_DLL
    void RecvProxy_CSGameRules( const RecvProp *pProp, void **pOut, void *pData, int objectID )
    {
        CCSGameRules *pRules = CSGameRules();
        Assert( pRules );
        *pOut = pRules;
    }

    BEGIN_RECV_TABLE( CCSGameRulesProxy, DT_CSGameRulesProxy )
        RecvPropDataTable( "cs_gamerules_data", 0, 0, &REFERENCE_RECV_TABLE( DT_CSGameRules ), RecvProxy_CSGameRules )
    END_RECV_TABLE()
#else
    void* SendProxy_CSGameRules( const SendProp *pProp, const void *pStructBase, const void *pData, CSendProxyRecipients *pRecipients, int objectID )
    {
        CCSGameRules *pRules = CSGameRules();
        Assert( pRules );
        return pRules;
    }

    BEGIN_SEND_TABLE( CCSGameRulesProxy, DT_CSGameRulesProxy )
        SendPropDataTable( "cs_gamerules_data", 0, &REFERENCE_SEND_TABLE( DT_CSGameRules ), SendProxy_CSGameRules )
    END_SEND_TABLE()
#endif


// MOSTLY DECPRECATED.
// All weapons ( except exhaustibles, ITEM_FLAG_EXHAUSTIBLE, such as grenades ) store their reserve ammo on the weapon entity
// and no longer on the player. The reserve amount is specified in schema as attributes "primary reserve ammo max" and "secondary reserve ammo max"

ConVar ammo_50AE_max( "ammo_50AE_max", "35", FCVAR_REPLICATED | FCVAR_RELEASE );
ConVar ammo_762mm_max( "ammo_762mm_max", "90", FCVAR_REPLICATED | FCVAR_RELEASE );
ConVar ammo_556mm_max( "ammo_556mm_max", "90", FCVAR_REPLICATED | FCVAR_RELEASE );
ConVar ammo_556mm_small_max( "ammo_556mm_small_max", "40", FCVAR_REPLICATED | FCVAR_RELEASE );
ConVar ammo_556mm_box_max( "ammo_556mm_box_max", "200", FCVAR_REPLICATED | FCVAR_RELEASE );
ConVar ammo_338mag_max( "ammo_338mag_max", "30", FCVAR_REPLICATED | FCVAR_RELEASE );
ConVar ammo_9mm_max( "ammo_9mm_max", "120", FCVAR_REPLICATED | FCVAR_RELEASE );
ConVar ammo_buckshot_max( "ammo_buckshot_max", "32", FCVAR_REPLICATED | FCVAR_RELEASE );
ConVar ammo_45acp_max( "ammo_45acp_max", "100", FCVAR_REPLICATED | FCVAR_RELEASE );
ConVar ammo_357sig_max( "ammo_357sig_max", "52", FCVAR_REPLICATED | FCVAR_RELEASE );
ConVar ammo_357sig_p250_max( "ammo_357sig_p250_max", "26", FCVAR_REPLICATED | FCVAR_RELEASE );
ConVar ammo_357sig_small_max( "ammo_357sig_small_max", "24", FCVAR_REPLICATED | FCVAR_RELEASE );
ConVar ammo_357sig_min_max( "ammo_357sig_min_max", "12", FCVAR_REPLICATED | FCVAR_RELEASE );
ConVar ammo_57mm_max( "ammo_57mm_max", "100", FCVAR_REPLICATED | FCVAR_RELEASE );

ConVar ammo_grenade_limit_default( "ammo_grenade_limit_default", "1", FCVAR_REPLICATED | FCVAR_RELEASE );
ConVar ammo_grenade_limit_flashbang( "ammo_grenade_limit_flashbang", "1", FCVAR_REPLICATED | FCVAR_RELEASE );
ConVar ammo_grenade_limit_total( "ammo_grenade_limit_total", "3", FCVAR_REPLICATED | FCVAR_RELEASE );
ConVar ammo_grenade_limit_snowballs( "ammo_grenade_limit_snowballs", "3", FCVAR_REPLICATED | FCVAR_RELEASE );

ConVar ammo_item_limit_healthshot( "ammo_item_limit_healthshot", "4", FCVAR_REPLICATED | FCVAR_RELEASE );
ConVar ammo_grenade_limit_breachcharge( "ammo_grenade_limit_breachcharge", "3", FCVAR_REPLICATED | FCVAR_RELEASE );

ConVar ammo_50AE_impulse( "ammo_50AE_impulse", "2400", FCVAR_REPLICATED, "You must enable tweaking via tweak_ammo_impulses to use this value." );
ConVar ammo_762mm_impulse( "ammo_762mm_impulse", "2400", FCVAR_REPLICATED, "You must enable tweaking via tweak_ammo_impulses to use this value." );
ConVar ammo_556mm_impulse( "ammo_556mm_impulse", "2400", FCVAR_REPLICATED, "You must enable tweaking via tweak_ammo_impulses to use this value." );
ConVar ammo_556mm_box_impulse( "ammo_556mm_box_impulse", "2400", FCVAR_REPLICATED, "You must enable tweaking via tweak_ammo_impulses to use this value." );
ConVar ammo_338mag_impulse( "ammo_338mag_impulse", "2800", FCVAR_REPLICATED, "You must enable tweaking via tweak_ammo_impulses to use this value." );
ConVar ammo_9mm_impulse( "ammo_9mm_impulse", "2000", FCVAR_REPLICATED, "You must enable tweaking via tweak_ammo_impulses to use this value." );
ConVar ammo_buckshot_impulse( "ammo_buckshot_impulse", "600", FCVAR_REPLICATED, "You must enable tweaking via tweak_ammo_impulses to use this value." );
ConVar ammo_45acp_impulse( "ammo_45acp_impulse", "2100", FCVAR_REPLICATED, "You must enable tweaking via tweak_ammo_impulses to use this value." );
ConVar ammo_357sig_impulse( "ammo_357sig_impulse", "2000", FCVAR_REPLICATED, "You must enable tweaking via tweak_ammo_impulses to use this value." );
ConVar ammo_57mm_impulse( "ammo_57mm_impulse", "2000", FCVAR_REPLICATED, "You must enable tweaking via tweak_ammo_impulses to use this value." );

ConVar ammo_50AE_headshot_mult( "ammo_50AE_headshot_mult", "1.0", FCVAR_REPLICATED, "You must enable tweaking via tweak_ammo_impulses to use this value." );
ConVar ammo_762mm_headshot_mult( "ammo_762mm_headshot_mult", "1.0", FCVAR_REPLICATED, "You must enable tweaking via tweak_ammo_impulses to use this value." );
ConVar ammo_556mm_headshot_mult( "ammo_556mm_headshot_mult", "1.0", FCVAR_REPLICATED, "You must enable tweaking via tweak_ammo_impulses to use this value." );
ConVar ammo_556mm_box_headshot_mult( "ammo_556mm_box_headshot_mult", "1.0", FCVAR_REPLICATED, "You must enable tweaking via tweak_ammo_impulses to use this value." );
ConVar ammo_338mag_headshot_mult( "ammo_338mag_headshot_mult", "1.0", FCVAR_REPLICATED, "You must enable tweaking via tweak_ammo_impulses to use this value." );
ConVar ammo_9mm_headshot_mult( "ammo_9mm_headshot_mult", "1.0", FCVAR_REPLICATED, "You must enable tweaking via tweak_ammo_impulses to use this value." );
ConVar ammo_buckshot_headshot_mult( "ammo_buckshot_headshot_mult", "1.0", FCVAR_REPLICATED, "You must enable tweaking via tweak_ammo_impulses to use this value." );
ConVar ammo_45acp_headshot_mult( "ammo_45acp_headshot_mult", "1.0", FCVAR_REPLICATED, "You must enable tweaking via tweak_ammo_impulses to use this value." );
ConVar ammo_357sig_headshot_mult( "ammo_357sig_headshot_mult", "1.0", FCVAR_REPLICATED, "You must enable tweaking via tweak_ammo_impulses to use this value." );
ConVar ammo_57mm_headshot_mult( "ammo_57mm_headshot_mult", "1.0", FCVAR_REPLICATED, "You must enable tweaking via tweak_ammo_impulses to use this value." );

//ConVar mp_dynamicpricing( "mp_dynamicpricing", "0", FCVAR_REPLICATED, "Enables or Disables the dynamic weapon prices" );

ConVar mp_spec_swapplayersides( 
	"mp_spec_swapplayersides", 
	"0",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Toggle set the player names and team names to the opposite side in which they are are on the spectator panel.",
	true, 0,
	true, 1
	);

ConVar mp_force_assign_teams(
	"mp_force_assign_teams",
	"0",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Players don't get to choose what team they are on, it is auto assinged.",
	true, 0,
	true, 1
	);

#if defined( GAME_DLL )
// need this defined for the server so it can load from game.cfg
ConVar sv_gameinstructor_disable( "sv_gameinstructor_disable", "0", FCVAR_REPLICATED | FCVAR_RELEASE, "Force all clients to disable their game instructors." );

ConVar cs_AssistDamageThreshold( "cs_AssistDamageThreshold", "40.0", FCVAR_DEVELOPMENTONLY, "cs_AssistDamageThreshold defines the amount of damage needed to score an assist" );
#endif

ConVar sv_matchpause_auto_5v5( "sv_matchpause_auto_5v5", "0", FCVAR_REPLICATED | FCVAR_RELEASE, "When enabled will automatically pause the match at next freeze time if less than 5 players are connected on each team." );


extern ConVar sv_stopspeed;
extern ConVar mp_randomspawn;
extern ConVar mp_randomspawn_los;
extern ConVar mp_teammates_are_enemies;
extern ConVar mp_respawnwavetime;
extern ConVar mp_hostages_max;
extern ConVar mp_hostages_spawn_farthest;
extern ConVar mp_hostages_spawn_force_positions;
extern ConVar mp_hostages_spawn_same_every_round;
extern ConVar weapon_max_before_cleanup;

ConVar mp_spectators_max( 
	"mp_spectators_max", 
	"2",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"How many spectators are allowed in a match.",
	true, 0,
	false, 0 );

ConVar mp_buytime( 
    "mp_buytime", 
    "90",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "How many seconds after round start players can buy items for.",
    true, 0,
    false, 0 );

ConVar mp_buy_allow_grenades( 
	"mp_buy_allow_grenades", 
	"1",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Whether players can purchase grenades from the buy menu or not.",
	true, 0,
	true, 1 );

ConVar mp_buy_allow_guns(
	"mp_buy_allow_guns",
	"255",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Whether players can purchase guns: pistols (1), SMGs (2), rifles (4), shotguns (8), sniper rifles (16), heavy MGs (32).",
	true, 0,
	true, 255 );

ConVar mp_do_warmup_period( 
    "mp_do_warmup_period", 
    "1",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "Whether or not to do a warmup period at the start of a match.",
    true, 0,
    true, 1 );

ConVar mp_do_warmup_offine( 
    "mp_do_warmup_offine", 
    "0",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "Whether or not to do a warmup period at the start of a match in an offline (bot) match.",
    true, 0,
    true, 1 );

ConVar mp_startmoney( 
	"mp_startmoney", 
	"800", 
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"amount of money each player gets when they reset",
	true, 0,
	false, 0 );

ConVar mp_maxmoney(
	"mp_maxmoney",
	"16000",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"maximum amount of money allowed in a player's account",
	true, 0,
	false, 0 );

ConVar mp_afterroundmoney(
	"mp_afterroundmoney",
	"0",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"amount of money awared to every player after each round" );

ConVar mp_playercashawards(
	"mp_playercashawards",
	"1",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Players can earn money by performing in-game actions" );

ConVar mp_teamcashawards(
	"mp_teamcashawards",
	"1",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Teams can earn money by performing in-game actions" );

ConVar mp_overtime_enable(
	"mp_overtime_enable",
	"0",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"If a match ends in a tie, use overtime rules to determine winner" );

ConVar mp_overtime_maxrounds(
	"mp_overtime_maxrounds",
	"6",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"When overtime is enabled play additional rounds to determine winner" );

ConVar mp_overtime_startmoney(
	"mp_overtime_startmoney",
	"10000",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Money assigned to all players at start of every overtime half" );

ConVar mp_hostages_takedamage( 
	"mp_hostages_takedamage", 
	"0", 
	FCVAR_REPLICATED | FCVAR_RELEASE, 
	"Whether or not hostages can be hurt." );

ConVar mp_hostages_rescuetowin( 
	"mp_hostages_rescuetowin", 
	"1", 
	FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, 
	"0 == all alive, any other number is the number the CT's need to rescue to win the round." );

ConVar mp_hostages_rescuetime( 
	"mp_hostages_rescuetime", 
	"1", 
	FCVAR_REPLICATED | FCVAR_RELEASE, 
	"Additional time added to round time if a hostage is reached by a CT." );

ConVar mp_anyone_can_pickup_c4(
	"mp_anyone_can_pickup_c4",
	"0",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"If set, everyone can pick up the c4, not just Ts." );

ConVar mp_c4_cannot_be_defused(
	"mp_c4_cannot_be_defused",
	"0",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"If set, the planted c4 cannot be defused." );

ConVar sv_coaching_enabled(
	"sv_coaching_enabled",
	"0",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Allows spectating and communicating with a team ( 'coach t' or 'coach ct' )"  );

ConVar sv_coach_comm_unrestricted(
	"sv_coach_comm_unrestricted",
	"0",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"When set, ignores coach communication restrictions." );

ConVar sv_allow_thirdperson(
	"sv_allow_thirdperson",
	"0",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Allows the server set players in third person mode without the client slamming it back (if cheats are on, all clients can set thirdperson without this convar being set)" );

ConVar sv_party_mode(
	"sv_party_mode",
	"0",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Party!!" );

ConVar sv_dz_hostage_rescue_reward( "sv_dz_hostage_rescue_reward",
	"10",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Number of cash bundles to award for rescuing a hostage" );

// ConVar mp_hostages_canpickup( 
// 	"mp_hostages_canpickup", 
// 	"1", 
// 	FCVAR_REPLICATED | FCVAR_RELEASE, 
// 	"1 = hostages are picked up when used, 0 == hostages uyse the old method of following behind players." );

#ifdef GAME_DLL
ConVar mp_disconnect_kills_players( "mp_disconnect_kills_players",
	"1",
	FCVAR_RELEASE,
	"When a player disconnects, kill them first (triggering item drops, stats, etc.)" );

ConVar mp_disconnect_kills_bots( "mp_disconnect_kills_bots",
	"0",
	FCVAR_RELEASE,
	"When a bot disconnects, kill them first.  Requires mp_disconnect_kills_players." );
#endif

#ifndef CLIENT_DLL
CON_COMMAND( timeout_terrorist_start, "" )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	CSGameRules()->StartTerroristTimeOut();
}

CON_COMMAND( timeout_ct_start, "" )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	CSGameRules()->StartCTTimeOut();
}
CON_COMMAND( mp_warmup_start, "Start warmup." )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	if ( CSGameRules() )
	{
		if ( CSGameRules()->IsQueuedMatchmaking() )
		{
			Msg( "mp_warmup_start cannot be used on official servers, use a longer mp_warmuptime instead!\n" );
		}
		else
		{
			CSGameRules()->StartWarmup();
		}
	}
}

CON_COMMAND( mp_warmup_end, "End warmup immediately." )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
	{
// 		// Special handling here allowing players to unpause warmup timer
// 		// if it is in mode #2 where it is paused until getting unpaused by players
// 		extern ConVar mp_warmup_pausetimer;
// 		extern ConVar mp_warmuptime;
// 		if ( CSGameRules() && CSGameRules()->IsWarmupPeriod() && ( mp_warmup_pausetimer.GetInt() == 2 ) )
// 		{
// 			int nNewWarmupTime = mp_warmuptime.GetInt();
// 			if ( nNewWarmupTime < 15 )
// 			{
// 				mp_warmuptime.SetValue( 15 );
// 			}
// 			mp_warmup_pausetimer.SetValue( 0 );
// 
// 			CSGameRules()->SetWarmupPeriodStartTime( gpGlobals->curtime );
// 			CSGameRules()->m_fWarmupNextChatNoticeTime = gpGlobals->curtime + 10;
// 
// 			CBroadcastRecipientFilter filter;
// 			int issuingPlayerIndex = UTIL_GetCommandClientIndex();
// 			if ( CCSPlayer *pPlayer = ToCSPlayer( UTIL_EntityByIndex( issuingPlayerIndex ) ) )
// 			{
// 				Warning( "mp_warmup_end issued by player#%d(%s), unpausing warmup...\n", issuingPlayerIndex, pPlayer->GetPlayerName() );
// 				UTIL_ClientPrintFilter( filter, HUD_PRINTTALK, "#SFUI_Player_Wants_Restart", pPlayer->GetPlayerName() );
// 			}
// 			else
// 			{
// 				Warning( "mp_warmup_end issued by player#%d with no player info, unpausing warmup...\n", issuingPlayerIndex );
// 			}
// 			UTIL_ClientPrintFilter( filter, HUD_PRINTTALK, "#SFUI_Notice_Match_Will_Start_Chat" );
// 		}

		return;
	}

	if ( CSGameRules() )
	{ 
		if ( CSGameRules()->IsQueuedMatchmaking() )
		{
			Msg( "mp_warmup_end cannot be used on official servers, use a shorter mp_warmuptime instead!\n" );
		}
		else
		{
			CSGameRules()->EndWarmup();
		}
	}
}
#endif

static void mpwarmuptime_f( IConVar *pConVar, const char *pOldString, float flOldValue )
{
	if ( CSGameRules() )
	{ 
		CSGameRules()->SetWarmupPeriodStartTime( gpGlobals->curtime );
	}
}


ConVar mp_verbose_changelevel_spew( "mp_verbose_changelevel_spew", "1", FCVAR_RELEASE );

ConVar mp_warmuptime( 
    "mp_warmuptime", 
    "30",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "How long the warmup period lasts. Changing this value resets warmup.",
    true, 5,
    false, 0,
	mpwarmuptime_f );

ConVar mp_warmuptime_all_players_connected(
	"mp_warmuptime_all_players_connected",
	"0",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Warmup time to use when all players have connected. 0 to disable." );

ConVar mp_endwarmup_player_count(
	"mp_endwarmup_player_count",
	"0",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Number of players required to be connected to end warmup early. 0 to require maximum players for mode." );

ConVar mp_warmup_pausetimer( 
	"mp_warmup_pausetimer", 
	"0",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Set to 1 to stay in warmup indefinitely. Set to 0 to resume the timer." );

ConVar mp_halftime_pausetimer( 
	"mp_halftime_pausetimer", 
	"0",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Set to 1 to stay in halftime indefinitely. Set to 0 to resume the timer." );

ConVar mp_halftime_pausematch(
	"mp_halftime_pausematch",
	"0",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Set to 1 to pause match after halftime countdown elapses. Match must be resumed by vote or admin." );

ConVar mp_overtime_halftime_pausetimer( 
	"mp_overtime_halftime_pausetimer", 
	"0",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"If set to 1 will set mp_halftime_pausetimer to 1 before every half of overtime. Set mp_halftime_pausetimer to 0 to resume the timer." );

ConVar mp_respawn_immunitytime("mp_respawn_immunitytime", "4.0", FCVAR_RELEASE | FCVAR_REPLICATED, "How many seconds after respawn immunity lasts." );

ConVar mp_playerid(
    "mp_playerid",
    "0",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "Controls what information player see in the status bar: 0 all names; 1 team names; 2 no names",
    true, 0,
    true, 2 );

ConVar mp_playerid_delay(
    "mp_playerid_delay",
    "0.4",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "Number of seconds to delay showing information in the status bar",
    true, 0,
    true, 1 );

ConVar mp_playerid_hold(
    "mp_playerid_hold",
    "0.2",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "Number of seconds to keep showing old information in the status bar",
    true, 0,
    true, 1 );

ConVar mp_round_restart_delay(
    "mp_round_restart_delay",
    "7.0",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "Number of seconds to delay before restarting a round after a win",
    true, 0.0f,
	true, 14.0f ); // 10 - 11 seconds is a comfortable minimum to fit the end of round replay

ConVar mp_halftime_duration(
    "mp_halftime_duration",
    "15.0",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "Number of seconds that halftime lasts",
    true, 0.0f,
    true, 300.0f );

ConVar mp_match_can_clinch(
    "mp_match_can_clinch",
    "1",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "Can a team clinch and end the match by being so far ahead that the other team has no way to catching up?" );

DEVELOPMENT_ONLY_CONVAR( mp_overtime_force, 0 );

bool MatchCanClinch()
{
	return mp_match_can_clinch.GetBool() && !mp_overtime_force.GetBool();
}

ConVar mp_ggtr_always_upgrade(
	"mp_ggtr_always_upgrade",
	"0",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Award this many upgrade points every round in demolition mode" );

ConVar mp_ggtr_end_round_kill_bonus(
    "mp_ggtr_end_round_kill_bonus",
    "1",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "Number of bonus points awarded in Demolition Mode when knife kill ends round",
    true, 0,
    true, 10 );

ConVar mp_ggtr_last_weapon_kill_ends_half(
    "mp_ggtr_last_weapon_kill_ends_half",
    "0",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "End the half and give a team round point when a player makes a kill using the final weapon",
    true, 0,
    true, 1 );

ConVar mp_ggprogressive_round_restart_delay(
    "mp_ggprogressive_round_restart_delay",
    "15.0",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "Number of seconds to delay before restarting a round after a win in gungame progessive",
    true, 0.0f,
    true, 90.0f );

ConVar mp_ggprogressive_use_random_weapons(
	"mp_ggprogressive_use_random_weapons",
	"1",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"If set, selects random weapons from set categories for the progression order" );

ConVar mp_ggprogressive_random_weapon_kills_needed(
	"mp_ggprogressive_random_weapon_kills_needed",
	"2",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"If mp_ggprogressive_use_random_weapons is set, this is the number of kills needed with each weapon" );

ConVar mp_ggtr_num_rounds_autoprogress(
	"mp_ggtr_num_rounds_autoprogress",
	"3",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Upgrade the player's weapon after this number of rounds without upgrading" );

ConVar mp_ct_default_melee(
	"mp_ct_default_melee",
	"weapon_knife",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"The default melee weapon that the CTs will spawn with.  Even if this is blank, a knife will be given.  To give a taser, it should look like this: 'weapon_knife weapon_taser'.  Remember to set mp_weapons_allow_zeus to 1 if you want to give a taser!" );

ConVar mp_ct_default_secondary(
	"mp_ct_default_secondary",
	"weapon_hkp2000",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"The default secondary (pistol) weapon that the CTs will spawn with" );

ConVar mp_ct_default_primary(
	"mp_ct_default_primary",
	"",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"The default primary (rifle) weapon that the CTs will spawn with" );

ConVar mp_ct_default_grenades(
	"mp_ct_default_grenades",
	"",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"The default grenades that the CTs will spawn with.  To give multiple grenades, separate each weapon class with a space like this: 'weapon_molotov weapon_hegrenade'" );

ConVar mp_t_default_melee(
	"mp_t_default_melee",
	"weapon_knife",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"The default melee weapon that the Ts will spawn with" );

ConVar mp_t_default_secondary(
	"mp_t_default_secondary",
	"weapon_glock",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"The default secondary (pistol) weapon that the Ts will spawn with" );

ConVar mp_t_default_primary(
	"mp_t_default_primary",
	"",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"The default primary (rifle) weapon that the Ts will spawn with" );

ConVar mp_t_default_grenades(
	"mp_t_default_grenades",
	"",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"The default grenades that the Ts will spawn with.  To give multiple grenades, separate each weapon class with a space like this: 'weapon_molotov weapon_hegrenade'" );

ConVar mp_join_grace_time(
    "mp_join_grace_time",
    "0.0",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "Number of seconds after round start to allow a player to join a game",
    true, 0.0f,
    true, 30.0f );

ConVar mp_win_panel_display_time(
    "mp_win_panel_display_time",
    "3",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "The amount of time to show the win panel between matches / halfs" );

ConVar mp_ggtr_bomb_pts_for_upgrade(
    "mp_ggtr_bomb_pts_for_upgrade",
    "2.0",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "Kill points required to upgrade a player's weapon",
    true, 1.0f,
    true, 10.0f );

ConVar mp_ggtr_bomb_pts_for_he(
    "mp_ggtr_bomb_pts_for_he",
    "3",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "Kill points required in a round to get a bonus HE grenade",
    true, 1,
    true, 5);

ConVar mp_ggtr_bomb_pts_for_flash(
    "mp_ggtr_bomb_pts_for_flash",
    "4",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "Kill points required in a round to get a bonus flash grenade",
    true, 1,
    true, 5);

ConVar mp_ggtr_bomb_pts_for_molotov(
    "mp_ggtr_bomb_pts_for_molotov",
    "5",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "Kill points required in a round to get a bonus molotov cocktail",
    true, 1,
    true, 5);

ConVar mp_molotovusedelay( 
    "mp_molotovusedelay",
    "15.0",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "Number of seconds to delay before the molotov can be used after acquiring it",
    true, 0.0f,
    true, 30.0f );

ConVar mp_ggtr_halftime_delay(
    "mp_ggtr_halftime_delay",
    "0.0",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "Number of seconds to delay during TR Mode halftime",
    true, 0.0f,
    true, 30.0f );

ConVar mp_ggtr_bomb_respawn_delay(
    "mp_ggtr_bomb_respawn_delay",
    "0.0",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "Number of seconds to delay before making the bomb available to a respawner in gun game",
    true, 0.0f,
    true, 30.0f );

ConVar mp_ggtr_bomb_defuse_bonus(
    "mp_ggtr_bomb_defuse_bonus",
    "1",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "Number of bonus upgrades to award the CTs when they defuse a gun game bomb",
    true, 1,
    true, 10 );

ConVar mp_ggtr_bomb_detonation_bonus(
    "mp_ggtr_bomb_detonation_bonus",
    "1",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "Number of bonus upgrades to award the Ts when they detonate a gun game bomb",
    true, 1,
    true, 10 );

ConVar mp_dm_kill_base_score(
	"mp_dm_kill_base_score",
	"10",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Number of base points to award for a kill in deathmatch.  Cheaper weapons award 1 or 2 additional points.",
	true, 0,
	false, 0 );

ConVar mp_dm_bonus_percent(
	"mp_dm_bonus_percent",
	"50",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Percent of points additionally awarded when someone gets a kill with the bonus weapon during the bonus period.",
	true, 0,
	false, 0 );

ConVar mp_dm_bonus_respawn(
	"mp_dm_bonus_respawn",
	"0",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"When attempting to get the bonus weapon in deathmatch, whether we should respawn you with it or just give it to you directly" );

ConVar mp_dm_dogtag_score(
	"mp_dm_dogtag_score",
	"0",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Points to award for picking up a dogtag in deathmatch.",
	true, 0,
	false, 0 );

ConVar mp_dm_teammode(
	"mp_dm_teammode",
	"0",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"In deathmatch, enables team DM visuals & scoring" );

ConVar mp_dm_teammode_kill_score(
	"mp_dm_teammode_kill_score",
	"1",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Team deathmatch victory points to award for enemy kill" );

ConVar mp_dm_teammode_bonus_score(
	"mp_dm_teammode_bonus_score",
	"1",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Team deathmatch victory points to award for kill with bonus weapon" );

ConVar mp_dm_teammode_dogtag_score(
	"mp_dm_teammode_dogtag_score",
	"0",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Team deathmatch victory points to award for collecting enemy dogtags" );

ConVar mp_dm_bonusweapon_dogtags(
	"mp_dm_bonusweapon_dogtags",
	"0",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Additional dogtags to drop when making a kill with the bonus weapon" );

ConVar mp_display_kill_assists(
	"mp_display_kill_assists",
	"1",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Whether to display and score player assists",
	true, 0,
	true, 1 );

ConVar mp_match_end_restart(
    "mp_match_end_restart",
#if defined (CSTRIKE15)
    "0",
#else
    "0",
#endif
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "At the end of the match, perform a restart instead of loading a new map",
    true, 0,
    true, 1 );

ConVar mp_match_end_changelevel(
	"mp_match_end_changelevel",
	"0",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"At the end of the match, perform a changelevel even if next map is the same",
	true, 0,
	true, 1 );

ConVar mp_defuser_allocation(
    "mp_defuser_allocation",
    "0",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "How to allocate defusers to CTs at start or round: 0=none, 1=random, 2=everyone",
    true, 0,
    true, 2 );

ConVar mp_give_player_c4(
	"mp_give_player_c4",
	"1",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Whether this map should spawn a c4 bomb for a player or not.",
	true, 0,
	true, 1 );

#ifndef CLIENT_DLL
ConVar mp_require_gun_use_to_acquire(
	"mp_require_gun_use_to_acquire",
	"0",
	FCVAR_RELEASE,
	"Whether guns must be +used to acquire or default is touch-to-pickup",
	true, 0,
	true, 1 );
#endif

ConVar mp_death_drop_gun(
    "mp_death_drop_gun",
    "1",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "Which gun to drop on player death: 0=none, 1=best, 2=current or best",
    true, 0,
    true, 2 );

ConVar mp_death_drop_c4(
	"mp_death_drop_c4",
	"1",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Whether c4 is droppable" );

ConVar mp_death_drop_grenade(
    "mp_death_drop_grenade",
    "2",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "Which grenade to drop on player death: 0=none, 1=best, 2=current or best, 3=all grenades",
    true, 0,
    true, 3 );

ConVar mp_death_drop_defuser(
    "mp_death_drop_defuser",
    "1",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "Drop defuser on player death",
    true, 0,
    true, 1 );

ConVar mp_death_drop_taser(
	"mp_death_drop_taser",
	"1",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Drop taser on player death",
	true, 0,
	true, 1 );

ConVar mp_death_drop_breachcharge(
	"mp_death_drop_breachcharge",
	"1",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Drop breachcharge on player death",
	true, 0,
	true, 1 );

ConVar mp_death_drop_healthshot(
	"mp_death_drop_healthshot",
	"1",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Drop healthshot on player death",
	true, 0,
	true, 1 );

ConVar mp_coop_force_join_ct(
	"mp_coop_force_join_ct",
	"0",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"If set, real players will auto join CT on join." );

ConVar mp_coopmission_mission_number(
	"mp_coopmission_mission_number",
	"0",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Which mission the map should run after it loads." );

ConVar mp_force_pick_time(
    "mp_force_pick_time",
    "15",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "The amount of time a player has on the team screen to make a selection before being auto-teamed" );

ConVar bot_autodifficulty_threshold_low(
    "bot_autodifficulty_threshold_low",
    "-2.0",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "Lower bound below Average Human Contribution Score that a bot must be below to change its difficulty",
    true, -20.0,
    true, 20.0 );

ConVar bot_autodifficulty_threshold_high(
    "bot_autodifficulty_threshold_high",
    "5.0",
    FCVAR_REPLICATED | FCVAR_RELEASE,
    "Upper bound above Average Human Contribution Score that a bot must be above to change its difficulty",
    true, -20.0,
    true, 20.0 );

ConVar mp_equipment_reset_rounds(
	"mp_equipment_reset_rounds",
	"0",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Reset all player equipment every N rounds (0 for never)" );

ConVar mp_economy_reset_rounds(
	"mp_economy_reset_rounds",
	"0",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Reset all player money every N rounds (0 for never)" );

ConVar mp_weapons_allow_zeus(
	"mp_weapons_allow_zeus", 
	"1", 
	FCVAR_REPLICATED | FCVAR_RELEASE, 
	"Determines how many Zeus purchases a player can make per round (0 to disallow, -1 to have no limit)." );

ConVar mp_weapons_allow_pistols(
	"mp_weapons_allow_pistols",
	"-1",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Determines which team, if any, can purchase Pistols. -1 = any; 0 = non; 2 = Ts; 3 = CTs." );

ConVar mp_weapons_allow_smgs(
	"mp_weapons_allow_smgs",
	"-1",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Determines which team, if any, can purchase SMGs. -1 = any; 0 = non; 2 = Ts; 3 = CTs." );

ConVar mp_weapons_allow_heavy(
	"mp_weapons_allow_heavy",
	"-1",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Determines which team, if any, can purchase Heavy guns. -1 = any; 0 = non; 2 = Ts; 3 = CTs." );

ConVar mp_weapons_allow_rifles(
	"mp_weapons_allow_rifles",
	"-1",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Determines which team, if any, can purchase Rifles. -1 = any; 0 = non; 2 = Ts; 3 = CTs." );

const char *g_pszHeavyPhoenixModel = "models/player/custom_player/legacy/tm_phoenix_heavy.mdl";
const char *g_pszHeavyPhoenixSleeves = "models/weapons/v_models/arms/phoenix_heavy/v_sleeve_phoenix_heavy.mdl";
const char *g_pszHeavyPhoenixDefaultGloves = "models/weapons/v_models/arms/glove_hardknuckle/v_glove_hardknuckle_black.mdl";

const char *g_pszHeavyAssaultSuitCT = "models/player/custom_player/legacy/ctm_heavy.mdl";
const char *g_pszHeavyAssaultSuitCTSleeves = "models/weapons/v_models/arms/ctm_heavy/v_sleeve_ctm_heavy.mdl";
const char *g_pszHeavyAssaultSuitCTDefaultGloves = "models/weapons/v_models/arms/glove_hardknuckle/v_glove_hardknuckle_black.mdl";

void fnHeavyAssaultSuitCallback( IConVar *var, const char *pOldValue, float flOldValue )
{
#ifdef GAME_DLL
	ConVar *pCvar = static_cast< ConVar* >( var );

	if ( CSGameRules() && pCvar->GetBool() == true )
	{
		bool allowPrecache = CBaseEntity::IsPrecacheAllowed();
		CBaseEntity::SetAllowPrecache( true );

		if ( !engine->IsModelPrecached( g_pszHeavyPhoenixModel ) )
		{
			CBaseEntity::PrecacheModel( g_pszHeavyPhoenixModel );
			CBaseEntity::PrecacheModel( g_pszHeavyPhoenixSleeves );
			CBaseEntity::PrecacheModel( g_pszHeavyPhoenixDefaultGloves );
		}

		if ( !engine->IsModelPrecached( g_pszHeavyAssaultSuitCT ) )
		{
			CBaseEntity::PrecacheModel( g_pszHeavyAssaultSuitCT );
			CBaseEntity::PrecacheModel( g_pszHeavyAssaultSuitCTSleeves );
			CBaseEntity::PrecacheModel( g_pszHeavyAssaultSuitCTDefaultGloves );
		}

		CBaseEntity::SetAllowPrecache( allowPrecache );
	}

#endif
}

ConVar mp_weapons_allow_heavyassaultsuit(
	"mp_weapons_allow_heavyassaultsuit",
	"0",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Determines whether heavyassaultsuit is permitted.", 
	fnHeavyAssaultSuitCallback );

ConVar mp_heavyassaultsuit_cooldown(
	"mp_heavyassaultsuit_cooldown",
	"5",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Determines cooldown of purchase." );

ConVar mp_weapons_allow_typecount(
	"mp_weapons_allow_typecount",
	"5",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Determines how many purchases of each weapon type allowed per player per round (0 to disallow purchasing, -1 to have no limit)." );

ConVar mp_weapons_allow_map_placed(
	"mp_weapons_allow_map_placed", 
	"0", 
	FCVAR_REPLICATED | FCVAR_RELEASE, 
	"If this convar is set, when a match starts, the game will not delete weapons placed in the map." );

ConVar mp_default_team_winner_no_objective(
	"mp_default_team_winner_no_objective", 
	"-1", 
	FCVAR_REPLICATED | FCVAR_RELEASE, 
	"If the map doesn't define an objective (bomb, hostage, etc), the value of this convar will declare the winner when the time runs out in the round." );

ConVar mp_weapons_glow_on_ground(
	"mp_weapons_glow_on_ground", 
	"0", 
	FCVAR_REPLICATED | FCVAR_RELEASE, 
	"If this convar is set, weapons on the ground will have a glow around them." );

ConVar mp_respawn_on_death_t(
	"mp_respawn_on_death_t", 
	"0", 
	FCVAR_REPLICATED | FCVAR_RELEASE, 
	"When set to 1, terrorists will respawn after dying." );

ConVar mp_respawn_on_death_ct(
	"mp_respawn_on_death_ct", 
	"0", 
	FCVAR_REPLICATED | FCVAR_RELEASE, 
	"When set to 1, counter-terrorists will respawn after dying." );

ConVar mp_use_respawn_waves(
	"mp_use_respawn_waves", 
	"0", 
	FCVAR_REPLICATED | FCVAR_RELEASE, 
	"When set to 1, and that player's team is set to respawn, they will respawn in waves. If set to 2, teams will respawn when the whole team is dead." );

void ProhibitedItemsCallback( IConVar *var, const char *pOldValue, float flOldValue )
{
#ifdef GAME_DLL

	if ( !CSGameRules() )
		return;

	ConVar *pCvar = static_cast<ConVar*>(var);

	CUtlStringList pProhibitedWeapons( pCvar->GetString(), "," );
	int prohibitedCount = 0;

	for ( int i = 0; i < pProhibitedWeapons.Count() && prohibitedCount < MAX_PROHIBITED_ITEMS; i++ )
	{
		int nDefIndex = V_atoi( pProhibitedWeapons[i] );
		const CEconItemDefinition* pItemDef = GetItemSchema()->GetItemDefinition( nDefIndex, true );

		if ( pItemDef && pItemDef->GetDefinitionIndex() == nDefIndex )
		{
			DevMsg( "Prohibiting %s\n", pItemDef->GetDefinitionName() );
			CSGameRules()->m_arrProhibitedItemIndices.Set( prohibitedCount, nDefIndex );
			prohibitedCount++;
		}
	}
	
	for ( int i = prohibitedCount; i < MAX_PROHIBITED_ITEMS; ++i )
	{
		CSGameRules()->m_arrProhibitedItemIndices.Set( i, 0 );
	}
#endif // GAME_DLL
}

ConVar mp_items_prohibited(
	"mp_items_prohibited",
	"",
	FCVAR_REPLICATED | FCVAR_RELEASE,
	"Set this convar to a comma-delimited list of definition indices of weapons that should be prohibited from use.",
	ProhibitedItemsCallback );



void RespawnWaveTimeCTCallback( IConVar *var, const char *pOldValue, float flOldValue );
void RespawnWaveTimeTCallback( IConVar *var, const char *pOldValue, float flOldValue );

void RespawnWaveTimeCTCallback( IConVar *var, const char *pOldValue, float flOldValue )
{
#ifdef CLIENT_DLL
	extern ConVar mp_respawnwavetime_ct;
	float flTime = mp_respawnwavetime_ct.GetFloat();
	if ( CSGameRules() )
	{
		float flNextRespawn = gpGlobals->curtime + flTime;
		CSGameRules()->SetNextTeamRespawnWaveDelay( TEAM_CT, flNextRespawn );
	}
#endif
}

void RespawnWaveTimeTCallback( IConVar *var, const char *pOldValue, float flOldValue )
{
#ifdef CLIENT_DLL
	extern ConVar mp_respawnwavetime_t;
	float flTime = mp_respawnwavetime_t.GetFloat();
	if ( CSGameRules() )
	{
		float flNextRespawn = gpGlobals->curtime + flTime;
		CSGameRules()->SetNextTeamRespawnWaveDelay( TEAM_TERRORIST, flNextRespawn );
	}
#endif
}

ConVar mp_respawnwavetime_ct( "mp_respawnwavetime_ct", "10.0", FCVAR_REPLICATED | FCVAR_RELEASE, "Time between respawn waves for CTs.", RespawnWaveTimeCTCallback );
ConVar mp_respawnwavetime_t( "mp_respawnwavetime_t", "10.0", FCVAR_REPLICATED | FCVAR_RELEASE, "Time between respawn waves for Terrorists.", RespawnWaveTimeTCallback );

#ifndef CLIENT_DLL
// Announcing is always on in REL build

// training
ConVar tr_completed_training(
    "tr_completed_training",
    "0",
    FCVAR_DEVELOPMENTONLY | FCVAR_ARCHIVE | FCVAR_ARCHIVE_GAMECONSOLE | FCVAR_SS,
    "Whether the local player has completed the initial training portion of the training map" );

ConVar tr_best_course_time(
    "tr_best_course_time",
    "0",
    FCVAR_DEVELOPMENTONLY | FCVAR_ARCHIVE | FCVAR_ARCHIVE_GAMECONSOLE | FCVAR_SS,
    "The player's best time for the timed obstacle course" );

ConVar tr_valve_course_time(
    "tr_valve_course_time",
    "352",
    FCVAR_DEVELOPMENTONLY,
    "Valve's best time for the timed obstacle course" );

ConVar mp_competitive_endofmatch_extra_time(
	"mp_competitive_endofmatch_extra_time",
	"15",
	FCVAR_RELEASE,
	"After a competitive match finishes rematch voting extra time is given for rankings." );
#endif

ConVar mp_endmatch_votenextmap( 
		"mp_endmatch_votenextmap", 
		"1", 
		FCVAR_REPLICATED | FCVAR_RELEASE, 
		"Whether or not players vote for the next map at the end of the match when the final scoreboard comes up" );

ConVar mp_endmatch_votenextmap_keepcurrent( 
	"mp_endmatch_votenextmap_keepcurrent", 
	"1", 
	FCVAR_REPLICATED | FCVAR_RELEASE, 
	"If set, keeps the current map in the list of voting options.  If not set, the current map will not appear in the list of voting options." );

ConVar mp_endmatch_votenextleveltime(
	"mp_endmatch_votenextleveltime",
	"20",
	FCVAR_RELEASE,
	"If mp_endmatch_votenextmap is set, players have this much time to vote on the next map at match end." );
	
// music controls

ConVar snd_music_boost(
    "snd_music_boost",
    "0",
    FCVAR_REPLICATED,
    "Specifies an amount to boost music volume by" );

#ifdef CLIENT_DLL
ConVar snd_music_selection(
    "snd_music_selection", 
    "1", 
    FCVAR_ARCHIVE,
    "Tracking rotating music for players with no music packs equipped.");

extern ConVar cl_borrow_music_from_player_index;
#endif

ConVar sv_endmatch_item_drop_interval(
	"sv_endmatch_item_drop_interval",
	"1.0",
	FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY,
	"The time between drops on the end match scoreboard " );

ConVar sv_endmatch_item_drop_interval_rare(
	"sv_endmatch_item_drop_interval_rare",
	"1.0",
	FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY,
	"The time between drops on the end match scoreboard for rare items " );

ConVar sv_endmatch_item_drop_interval_mythical(
	"sv_endmatch_item_drop_interval_mythical",
	"1.25",
	FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY,
	"The time between drops on the end match scoreboard for mythical items " );

ConVar sv_endmatch_item_drop_interval_legendary(
	"sv_endmatch_item_drop_interval_legendary",
	"2.0",
	FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY,
	"The time between drops on the end match scoreboard for legendary items " );

ConVar sv_endmatch_item_drop_interval_ancient(
	"sv_endmatch_item_drop_interval_ancient",
	"3.5",
	FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY,
	"The time between drops on the end match scoreboard for ancient items " );

// bot difficulty tracking per user input device
ConVar sv_compute_per_bot_difficulty(
    "sv_compute_per_bot_difficulty",
    "0",
    FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY,
    "0 = compute all bot difficulties equally, 1 = compute unique bot difficulty for each bot " );

ConVar sv_show_bot_difficulty_in_name(
    "sv_show_bot_difficulty_in_name",
    "0",
    FCVAR_REPLICATED,
    "0 = hide bot difficulty in bot name, 1 = show bot difficulty in bot name" );

ConVar sv_bot_difficulty_kbm(
    "sv_bot_difficulty_kbm",
    "0",
    FCVAR_REPLICATED | FCVAR_HIDDEN,
    "Bot difficulty while playing with Keyboard/Mouse device" );

ConVar sv_bot_difficulty_gamepad(
    "sv_bot_difficulty_gamepad",
    "0",
    FCVAR_REPLICATED | FCVAR_HIDDEN,
    "Bot difficulty while playing with Gamepad device" );

ConVar sv_bot_difficulty_ps3move(
    "sv_bot_difficulty_ps3move",
    "0",
    FCVAR_REPLICATED | FCVAR_HIDDEN,
    "Bot difficulty while playing with PS3Move device" );

ConVar sv_bot_difficulty_hydra(
    "sv_bot_difficulty_hydra",
    "0",
    FCVAR_REPLICATED | FCVAR_HIDDEN,
    "Bot difficulty while playing with Hydra device" );

ConVar sv_bot_difficulty_sharpshooter(
    "sv_bot_difficulty_sharpshooter",
    "0",
    FCVAR_REPLICATED | FCVAR_HIDDEN,
    "Bot difficulty while playing with SharpShooter device" );

ConVar sv_competitive_official_5v5( "sv_competitive_official_5v5",
	"0",
	FCVAR_REPLICATED | FCVAR_NOTIFY | FCVAR_RELEASE,
	"Enable to force the server to show 5v5 scoreboards and allows spectators to see characters through walls." );

ConVar sv_kick_ban_duration( "sv_kick_ban_duration",
	"15",
	FCVAR_REPLICATED | FCVAR_NOTIFY | FCVAR_RELEASE,
	"How long should a kick ban from the server should last (in minutes)" );

ConVar sv_disable_immunity_alpha( "sv_disable_immunity_alpha",
							 "0",
							 FCVAR_REPLICATED | FCVAR_RELEASE,
							 "If set, clients won't slam the player model render settings each frame for immunity [mod authors use this]" );

#ifdef CLIENT_DLL
ConVar cl_bot_difficulty_kbm(
    "cl_bot_difficulty_kbm",
    "0",
    FCVAR_HIDDEN,
    "Bot difficulty while playing with Keyboard/Mouse device" );

ConVar cl_bot_difficulty_gamepad(
    "cl_bot_difficulty_gamepad",
    "0",
    FCVAR_HIDDEN,
    "Bot difficulty while playing with Gamepad device" );

ConVar cl_bot_difficulty_ps3move(
    "cl_bot_difficulty_ps3move",
    "0",
    FCVAR_HIDDEN,
    "Bot difficulty while playing with PS3Move device" );

ConVar cl_bot_difficulty_hydra(
    "cl_bot_difficulty_hydra",
    "0",
    FCVAR_HIDDEN,
    "Bot difficulty while playing with Hydra device" );

ConVar cl_bot_difficulty_sharpshooter(
    "cl_bot_difficulty_sharpshooter",
    "0",
    FCVAR_HIDDEN,
    "Bot difficulty while playing with SharpShooter device" );
#endif

// Set game rules to allow all clients to talk to each other.
// Muted players still can't talk to each other.
ConVar sv_alltalk( "sv_alltalk", "0", FCVAR_REPLICATED | FCVAR_NOTIFY | FCVAR_RELEASE, "Deprecated. Replaced with sv_talk_enemy_dead and sv_talk_enemy_living." );

// [jason] Can the dead speak to the living?
ConVar sv_deadtalk( "sv_deadtalk", "0",	FCVAR_REPLICATED | FCVAR_NOTIFY | FCVAR_RELEASE, "Dead players can speak (voice, text) to the living" );

// [jason] Override that removes all chat restrictions, including those for spectators
ConVar sv_full_alltalk( "sv_full_alltalk", "0", FCVAR_REPLICATED | FCVAR_RELEASE, "Any player (including Spectator team) can speak to any other player" );

ConVar sv_talk_enemy_dead( "sv_talk_enemy_dead", "0", FCVAR_REPLICATED | FCVAR_RELEASE, "Dead players can hear all dead enemy communication (voice, chat)" );
ConVar sv_talk_enemy_living( "sv_talk_enemy_living", "0", FCVAR_REPLICATED | FCVAR_RELEASE, "Living players can hear all living enemy communication (voice, chat)" );
ConVar sv_talk_after_dying_time( "sv_talk_after_dying_time", "0", FCVAR_REPLICATED | FCVAR_RELEASE, "The number of seconds a player can continue talking after dying as if they were still alive" );

#ifdef GAME_DLL
ConVar sv_auto_full_alltalk_during_warmup_half_end( "sv_auto_full_alltalk_during_warmup_half_end", "1", FCVAR_RELEASE, "When enabled will automatically turn on full all talk mode in warmup, at halftime and at the end of the match" );
#endif

ConVar sv_spec_hear( "sv_spec_hear", "1", FCVAR_REPLICATED | FCVAR_NOTIFY | FCVAR_RELEASE, "Determines who spectators can hear: 0: only spectators; 1: all players; 2: spectated team; 3: self only; 4: nobody" );

ConVar mp_c4timer( 
	"mp_c4timer", 
	"40", 
	FCVAR_REPLICATED | FCVAR_NOTIFY | FCVAR_RELEASE,
	"how long from when the C4 is armed until it blows",
	true, 10,	// min value
	false, 0	// max value
	);

namespace SpecHear
{
    enum Type
    {
        OnlySpectators = 0,
        AllPlayers = 1,
        SpectatedTeam = 2,
		Self = 3,
		Nobody = 4,
    };
}

// NOTE: the indices here must match TEAM_TERRORIST, TEAM_CT, TEAM_SPECTATOR, etc.
char *sTeamNames[] =
{
	"Unassigned",
	"Spectator",
	"TERRORIST",
	"CT"
};



#ifdef CLIENT_DLL

ConVar cl_autowepswitch(
    "cl_autowepswitch",
    "1",
    FCVAR_ARCHIVE | FCVAR_ARCHIVE_GAMECONSOLE| FCVAR_SS | FCVAR_USERINFO,
    "Automatically switch to picked up weapons (if more powerful)" );

ConVar cl_use_opens_buy_menu(
	"cl_use_opens_buy_menu",
	"1",
	FCVAR_ARCHIVE | FCVAR_ARCHIVE_GAMECONSOLE| FCVAR_SS | FCVAR_USERINFO,
	"Pressing the +use key will open the buy menu if in a buy zone (just as if you pressed the 'buy' key)." );

ConVar cl_autohelp(
    "cl_autohelp",
    "1",
    FCVAR_ARCHIVE | FCVAR_USERINFO,
    "Auto-help" );

ConVar closeonbuy(
	"closeonbuy",
	"0",
	FCVAR_ARCHIVE | FCVAR_SS,
	"Set non-zero to close the buy menu after buying something" );


#else

    // longest the intermission can last, in seconds
    #define MAX_INTERMISSION_TIME 120

    // Falling damage stuff.
    #define CS_PLAYER_FATAL_FALL_SPEED		1000	// approx 60 feet
    #define CS_PLAYER_MAX_SAFE_FALL_SPEED	580		// approx 20 feet
    #define CS_DAMAGE_FOR_FALL_SPEED		((float)100 / ( CS_PLAYER_FATAL_FALL_SPEED - CS_PLAYER_MAX_SAFE_FALL_SPEED )) // damage per unit per second.

    // These entities are preserved each round restart. The rest are removed and recreated.
    static const char *s_PreserveEnts[] =
    {
        "ai_network",
        "ai_hint",
        "cs_gamerules",
        "cs_team_manager",
        "cs_player_manager",
        "env_soundscape",
        "env_soundscape_proxy",
        "env_soundscape_triggerable",
        "env_sun",
        "env_wind",
        "env_fog_controller",
        "env_tonemap_controller",
        "env_cascade_light",
        "func_brush",
        "func_wall",
        "func_buyzone",
        "func_illusionary",
        "func_hostage_rescue",
        "func_bomb_target",
        "infodecal",
        "info_projecteddecal",
        "info_node",
        "info_target",
        "info_node_hint",
        "info_player_counterterrorist",
        "info_player_terrorist",
		"info_enemy_terrorist_spawn",
		"info_deathmatch_spawn",
		"info_armsrace_counterterrorist",
		"info_armsrace_terrorist",
        "info_map_parameters",
        "keyframe_rope",
        "move_rope",
        "info_ladder",
        "player",
        "point_viewcontrol",
        "point_viewcontrol_multiplayer",
        "scene_manager",
        "shadow_control",
        "sky_camera",
        "soundent",
        "trigger_soundscape",
        "viewmodel",
        "predicted_viewmodel",
        "worldspawn",
        "point_devshot_camera",
        "logic_choreographed_scene",
		"cfe_player_decal",				// persistent player spray decals must be preserved
		//"logic_auto",					// preserving this will break all of the maps who currently rely on it getting destroyed each time the map entities are recreated
        "info_bomb_target_hint_A",
        "info_bomb_target_hint_B",
        "info_hostage_rescue_zone_hint",
        // for the training map
        "generic_actor",
        "vote_controller",
		"wearable_item",
		"point_hiding_spot",
		"game_coopmission_manager",
		"chicken",
		"global_chatter", // keep chatter entity around, it refers to the generic_actor it creates that doesn't get removed.
        "", // END Marker
    };


    // --------------------------------------------------------------------------------------------------- //
    // Voice helper
    // --------------------------------------------------------------------------------------------------- //

    class CVoiceGameMgrHelper : public IVoiceGameMgrHelper
    {
    public:
        virtual bool		CanPlayerHearPlayer( CBasePlayer *pListener, CBasePlayer *pTalker, bool &bProximity )
        {
            if ( pListener == NULL || pTalker == NULL )
                return false;

            if ( !CSGameRules() )
                return false;

            return CSGameRules()->CanPlayerHearTalker( pListener, pTalker, false );
        }
    };
    CVoiceGameMgrHelper g_VoiceGameMgrHelper;
    IVoiceGameMgrHelper *g_pVoiceGameMgrHelper = &g_VoiceGameMgrHelper;



    // --------------------------------------------------------------------------------------------------- //
    // Globals.
    // --------------------------------------------------------------------------------------------------- //

	ConVar dev_reportmoneychanges( 
		"dev_reportmoneychanges", 
		"0", 
		FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY,
		"Displays money account changes for players in the console" );

    ConVar mp_roundtime( 
        "mp_roundtime",
        "5",
        FCVAR_REPLICATED | FCVAR_NOTIFY | FCVAR_RELEASE,
        "How many minutes each round takes.",
        true, 1,	// min value
        true, 60	// max value
        );

    ConVar mp_roundtime_deployment( 
        "mp_roundtime_deployment",
        "5",
        FCVAR_RELEASE,
        "How many minutes deployment for coop mission takes.",
        true, 1,	// min value
        true, 15	// max value
        );

	ConVar mp_roundtime_hostage( 
		"mp_roundtime_hostage",
		"0",
		FCVAR_REPLICATED | FCVAR_NOTIFY | FCVAR_RELEASE,
		"How many minutes each round of Hostage Rescue takes. If 0 then use mp_roundtime instead.",
		true, 0,	// min value
		true, 60	// max value
		);

	ConVar mp_roundtime_defuse( 
		"mp_roundtime_defuse",
		"0",
		FCVAR_REPLICATED | FCVAR_NOTIFY | FCVAR_RELEASE,
		"How many minutes each round of Bomb Defuse takes. If 0 then use mp_roundtime instead.",
		true, 0,	// min value
		true, 60	// max value
		);

    ConVar mp_freezetime( 
        "mp_freezetime",
        "6",
        FCVAR_REPLICATED | FCVAR_NOTIFY | FCVAR_RELEASE,
        "how many seconds to keep players frozen when the round starts",
        true, 0,	// min value
        true, 60	// max value
        );

    ConVar mp_limitteams( 
        "mp_limitteams", 
        "2", 
        FCVAR_REPLICATED | FCVAR_NOTIFY | FCVAR_RELEASE,
        "Max # of players 1 team can have over another (0 disables check)",
        true, 0,	// min value
        true, 30	// max value
        );

    ConVar mp_tkpunish( 
        "mp_tkpunish", 
        "0", 
        FCVAR_REPLICATED | FCVAR_RELEASE,
        "Will TK'ers and team damagers be punished in the next round?  {0=no,  1=yes}" );

    ConVar mp_autokick(
        "mp_autokick",
        "1",
        FCVAR_REPLICATED | FCVAR_RELEASE,
        "Kick idle/team-killing/team-damaging players" );

    ConVar mp_spawnprotectiontime(
        "mp_spawnprotectiontime",
        "5",
        FCVAR_REPLICATED | FCVAR_RELEASE,
        "Kick players who team-kill within this many seconds of a round restart." );

	ConVar mp_td_spawndmgthreshold(
		"mp_td_spawndmgthreshold",
		"50",
		FCVAR_REPLICATED | FCVAR_RELEASE,
		"The damage threshold players have to exceed at the start of the round to be warned/kick." );

	ConVar mp_td_dmgtowarn(
		"mp_td_dmgtowarn",
		"200",
		FCVAR_REPLICATED | FCVAR_RELEASE,
		"The damage threshhold players have to exceed in a match to get warned that they are about to be kicked." );

	ConVar mp_td_dmgtokick(
		"mp_td_dmgtokick",
		"300",
		FCVAR_REPLICATED | FCVAR_RELEASE,
		"The damage threshhold players have to exceed in a match to get kicked." );

    ConVar mp_humanteam( 
        "mp_humanteam", 
        "any", 
        FCVAR_REPLICATED | FCVAR_RELEASE,
        "Restricts human players to a single team {any, CT, T}" );

	ConVar mp_guardian_special_kills_needed( 
		"mp_guardian_special_kills_needed", 
		"10", 
		FCVAR_REPLICATED | FCVAR_RELEASE,
		"The number of kills needed with a specific weapon." );

	ConVar mp_guardian_special_weapon_needed( 
		"mp_guardian_special_weapon_needed", 
		"awp", 
		FCVAR_REPLICATED | FCVAR_RELEASE,
		"The weapon that needs to be used to increment the kills needed to complete the mission." );

	ConVar mp_guardian_player_dist_min( 
		"mp_guardian_player_dist_min", 
		"1300", 
		FCVAR_REPLICATED | FCVAR_RELEASE,
		"The distance at which we start to warn a player when they are too far from the guarded bombsite." );

	ConVar mp_guardian_player_dist_max( 
		"mp_guardian_player_dist_max", 
		"2000", 
		FCVAR_REPLICATED | FCVAR_RELEASE,
		"The maximum distance a player is allowed to get from the bombsite before they're killed." );

	ConVar mp_guardian_bot_money_per_wave(
		"mp_guardian_bot_money_per_wave",
		"800",
		FCVAR_REPLICATED | FCVAR_RELEASE,
		"The amount of money bots get time each wave the players complete.  This # is absolute and not additive, the money is set to (this)x(wave#) for each bot on each wave." );

	ConVar mp_guardian_loc_override(
		"mp_guardian_loc_override",
		"",
		FCVAR_REPLICATED | FCVAR_RELEASE,
		"Token to use to display guardian mission (#quest_hud_guardian_override_<name>) (if exists)"
		);

	ConVar mp_guardian_loc_mission(
		"mp_guardian_loc_mission",
		"default",
		FCVAR_REPLICATED | FCVAR_RELEASE,
		"Token to use to generate guardian mission (#quest_hud_guardian_mission_<name>)"
		);

	ConVar mp_guardian_loc_weapon(
		"mp_guardian_loc_weapon",
		"",
		FCVAR_REPLICATED | FCVAR_RELEASE,
		"(If set) weapon name token (#SFUI_WPNHUD_<name>)"
		);

	ConVar mp_guardian_loc_adjective(
		"mp_guardian_loc_adjective",
		"",
		FCVAR_REPLICATED | FCVAR_RELEASE,
		"(If set) kill condition token (#quest_hud_guardian_adjective_<name>)"
		);

	ConVar mp_guardian_loc_condition(
		"mp_guardian_loc_condition",
		"",
		FCVAR_REPLICATED | FCVAR_RELEASE,
		"(If set) kill condition token (#quest_hud_guardian_condition_<name>)"
		);

	ConVar mp_guardian_loc_icon(
		"mp_guardian_loc_icon",
		"",
		FCVAR_REPLICATED | FCVAR_RELEASE,
		"(If set) icon to override for guardian mission"
		);

    ConVar mp_ignore_round_win_conditions(
        "mp_ignore_round_win_conditions",
        "0",
        FCVAR_REPLICATED | FCVAR_RELEASE,
        "Ignore conditions which would end the current round" );

	ConVar mp_dm_time_between_bonus_min(
		"mp_dm_time_between_bonus_min",
		"30",
		FCVAR_REPLICATED | FCVAR_RELEASE,
		"Minimum time a bonus time will start after the round start or after the last bonus (in seconds)" );

	ConVar mp_dm_time_between_bonus_max(
		"mp_dm_time_between_bonus_max",
		"40",
		FCVAR_REPLICATED | FCVAR_RELEASE,
		"Maximum time a bonus time will start after the round start or after the last bonus (in seconds)" );

	ConVar mp_dm_bonus_length_min(
		"mp_dm_bonus_length_min",
		"30",
		FCVAR_REPLICATED | FCVAR_RELEASE,
		"Minimum time the bonus time will last (in seconds)" );

	ConVar mp_dm_bonus_length_max(
		"mp_dm_bonus_length_max",
		"30",
		FCVAR_REPLICATED | FCVAR_RELEASE,
		"Maximum time the bonus time will last (in seconds)" );

	ConVar mp_dogtag_pickup_rule(
		"mp_dogtag_pickup_rule",
		"0",
		FCVAR_REPLICATED | FCVAR_RELEASE,
		"Who is eligible to pick up a dogtag (0 = killer only, 1 = killer's team, 2 = victim's team, 3 = killer & victim's team, 4 = anyone)" );

	ConVar mp_dogtag_despawn_on_killer_death(
		"mp_dogtag_despawn_on_killer_death",
		"1",
		FCVAR_REPLICATED | FCVAR_RELEASE,
		"Whether dogtags should despawn when their killer dies" );

	ConVar mp_dogtag_despawn_time(
		"mp_dogtag_despawn_time",
		"120",
		FCVAR_REPLICATED | FCVAR_RELEASE,
		"How many seconds dogtags should stay around before despawning automatically (0 = infinite)",
		true, 0,
		false, -1 );

	ConVar mp_damage_scale_ct_body(
		"mp_damage_scale_ct_body",
		"1.0",
		FCVAR_REPLICATED | FCVAR_RELEASE,
		"Scales the damage a CT player takes by this much when they take damage in the body. (1 == 100%, 0.5 == 50%)" );

	ConVar mp_damage_scale_ct_head(
		"mp_damage_scale_ct_head",
		"1.0",
		FCVAR_REPLICATED | FCVAR_RELEASE,
		"Scales the damage a CT player takes by this much when they take damage in the head (1 == 100%, 0.5 == 50%).  REMEMBER! headshots do 4x the damage of the body before this scaler is applied." );

	ConVar mp_damage_scale_t_body(
		"mp_damage_scale_t_body",
		"1.0",
		FCVAR_REPLICATED | FCVAR_RELEASE,
		"Scales the damage a T player takes by this much when they take damage in the body. (1 == 100%, 0.5 == 50%)" );

	ConVar mp_damage_scale_t_head(
		"mp_damage_scale_t_head",
		"1.0",
		FCVAR_REPLICATED | FCVAR_RELEASE,
		"Scales the damage a T player takes by this much when they take damage in the head (1 == 100%, 0.5 == 50%).  REMEMBER! headshots do 4x the damage of the body before this scaler is applied." );

	ConVar mp_player_healthbuffer_decay_rate(
		"mp_player_healthbuffer_decay_rate",
		"0",
		FCVAR_REPLICATED,
		"When a player has buffer health, this is how fast it ticks down." );

	ConVar mp_damage_headshot_only(
		"mp_damage_headshot_only",
		"0",
		FCVAR_REPLICATED | FCVAR_RELEASE,
		"Determines whether non-headshot hits do any damage." );

	ConVar mp_weapon_self_inflict_amount(
		"mp_weapon_self_inflict_amount",
		"0",
		FCVAR_REPLICATED | FCVAR_RELEASE,
		"If Set to non-0, will hurt the attacker by the specified fraction of max damage if they miss.");

	ConVar mp_damage_vampiric_amount(
		"mp_damage_vampiric_amount",
		"0",
		FCVAR_REPLICATED | FCVAR_RELEASE,
		"If Set to non-0, will determine the fraction of damage dealt that will be given to attacker.");
	
	ConVar mp_global_damage_per_second(
		"mp_global_damage_per_second",
		"0.0",
		FCVAR_REPLICATED | FCVAR_RELEASE,
		"If above 0, deal non-lethal damage to players over time.",
		true, 0.0f,
		false, 0.0f );

	ConVar mp_starting_losses(
		"mp_starting_losses",
		"0",
		FCVAR_REPLICATED | FCVAR_RELEASE,
		"Determines what the initial loss streak is.", true, 0, false, -1 );

	ConVar mp_consecutive_loss_max(
		"mp_consecutive_loss_max",
		"4",
		FCVAR_REPLICATED | FCVAR_RELEASE,
		"", true, 0, false, -1 );




	ConCommand EndRound( "endround", &CCSGameRules::EndRound, "End the current round.", FCVAR_CHEAT );

	void cc_ReportEntitiesInEntList( const CCommand& args )
	{
		bool bShort = true, bPosition = false;
		const char *pOnlyClass = nullptr;
		const char *pOnlyName = nullptr;

		for(  int i= 1; i < args.ArgC(); i++ )
		{
			if ( !V_stricmp( args[ i ], "-?" ) )
			{
				Msg( "Usage: ent_list_report [-a] [-class name] [-name name] [-pos]\n" );
			}
			else if ( !V_stricmp( args[ i ], "-a" ) )
			{
				bShort = false;
			}
			else if ( !V_stricmp( args[ i ], "-pos" ) )
			{
				bShort = false;
				bPosition = true;
			}
			else if ( !V_stricmp( args[ i ], "-class" ) )
			{
				bShort = false;
				if ( ++i < args.ArgC() )
				{
					pOnlyClass = args[ i ];
				}
			}
			else if ( !V_stricmp( args[ i ], "-name" ) )
			{
				pOnlyName = nullptr;
				if ( ++i < args.ArgC() )
				{
					pOnlyName = args[ i ];
				}
			}
		}

		//int nNumEnts = gEntList.NumberOfEntities();
		for ( CBaseEntity *pEntity = gEntList.FirstEnt(); pEntity != NULL; pEntity = gEntList.NextEnt( pEntity ) )
		{
			if ( pEntity )
			{
				if ( bShort )
				{
					Msg( "%s\n", pEntity->GetClassname() );
				}
				else
				{
					Msg( "%d\t%s\t%s", pEntity->entindex(), pEntity->GetClassname(), pEntity->GetEntityNameAsCStr() );
					if ( pEntity->IsDormant() )
						Msg( "\tdormant" );
					if ( bPosition )
					{
						Vector pos = pEntity->GetAbsOrigin();
						Msg( "\t{%g,%g,%g}", pos.x, pos.y, pos.z );
					}
					Msg( "\n" );
				}
			}
		}
	}

	static ConCommand ent_list_report( "ent_list_report", cc_ReportEntitiesInEntList, "Reports all list of all entities in a map, one by one" );


		//number_of_entities = gEntList.NumberOfEntities();

	CON_COMMAND_F ( tv_time_remaining, "Print remaining tv broadcast time", FCVAR_RELEASE | FCVAR_GAMEDLL | FCVAR_GAMEDLL_FOR_REMOTE_CLIENTS )
	{
#ifdef GAME_DLL
		if ( HLTVDirector() && HLTVDirector()->IsActive() )
		{
			CEngineHltvInfo_t engineHltv;
			if ( engine->GetEngineHltvInfo( engineHltv ) &&
				engineHltv.m_bBroadcastActive && ( engineHltv.m_numClients > 0 ) )
			{
				if ( CSGameRules()->GetMatch()->GetPhase() != GAMEPHASE_MATCH_ENDED )
				{
					ConMsg( "GOTV spectators are attached. Match is still in progress.\n" );
				}
				else
				{
					float flTimeRemaining = ( CSGameRules()->GetIntermissionStartTime() + HLTVDirector()->GetDelay() + 5.0 ) - gpGlobals->curtime;

					if ( flTimeRemaining > 0 )
					{
						ConMsg("GOTV spectators are attached. %f seconds remaining to broadcast.\n", ( CSGameRules()->GetIntermissionStartTime() + HLTVDirector()->GetDelay() + 5.0 ) - gpGlobals->curtime );
					}
					else
					{
						ConMsg( "GOTV spectators are attached. GOTV Broadcast is complete.\n" );
					}
				}
			}
			else
			{
				ConMsg( "There are no GOTV spectators attached.\n" );
			}
		}
		else
#endif
			ConMsg( "GOTV is not active.\n" );
	}

    CON_COMMAND_F ( reset_expo, "Reset player scores, player controls, team scores, and end the round", FCVAR_CHEAT | FCVAR_GAMEDLL )
    {
        CSGameRules()->ResetForTradeshow();
    }
    
    CON_COMMAND_F ( tweak_ammo_impulses, "Allow real-time tweaking of the ammo impulse values.", FCVAR_CHEAT | FCVAR_GAMEDLL)
    {
        for  (int ii=0; ii<MAX_AMMO_TYPES; ++ii )
        {
            GetCSAmmoDef()->m_AmmoType[ii].pPhysicsForceImpulse = USE_CVAR;
        }
    }

    // --------------------------------------------------------------------------------------------------- //
    // Contribution score control values
    // --------------------------------------------------------------------------------------------------- //

    ConVar score_default(
        "score_default",
        "1000",
        FCVAR_NONE,
        "Default points for a new user" );

    ConVar score_kill_enemy_bonus(
        "score_kill_enemy_bonus",
        "0",
        FCVAR_NONE,
        "Points awarded for killing an enemy" );

    ConVar score_damage(
        "score_damage",
        "1",
        FCVAR_NONE,
        "Points awarded for each point of damage to an enemy" );

    ConVar score_ff_damage(
        "score_ff_damage",
        "1",
        FCVAR_NONE,
        "Penalty awarded for each point of damage to a teammate" );

    ConVar score_team_damage_bonus(
        "score_team_damage_bonus",
        "1",
        FCVAR_NONE,
        "Points awarded for each point of damage a nearby (in same zone) teammate does to enemies" );

    ConVar score_planted_bomb_proximity_damage_bonus(
        "score_planted_bomb_proximity_damage_bonus",
        "1",
        FCVAR_NONE,
        "Points awarded for damaging enemy near planted bomb" );

    ConVar score_planted_bomb_proximity_damage_radius_inner(
        "score_planted_bomb_proximity_damage_radius_inner",
        "120",
        FCVAR_NONE,
        "Inner radius (full bonus) for doing damage near planted bomb" );

    ConVar score_planted_bomb_proximity_damage_radius_outer(
        "score_planted_bomb_proximity_damage_radius_outer",
        "600",
        FCVAR_NONE,
        "Outer radius (zero bonus) for doing damage near planted bomb" );

    ConVar score_hostage_proximity_damage_bonus(
        "score_hostage_proximity_damage_bonus",
        "1",
        FCVAR_NONE,
        "Points awarded for damaging enemy near live hostage" );

    ConVar score_hostage_proximity_damage_radius_inner(
        "score_hostage_proximity_damage_radius_inner",
        "120",
        FCVAR_NONE,
        "Inner radius (full bonus) for doing damage near hostage" );

    ConVar score_hostage_proximity_damage_radius_outer(
        "score_hostage_proximity_damage_radius_outer",
        "600",
        FCVAR_NONE,
        "Outer radius (zero bonus) for doing damage near hostage" );

    ConVar score_dropped_bomb_proximity_damage_bonus(
        "score_dropped_bomb_proximity_damage_bonus",
        "1",
        FCVAR_NONE,
        "Points awarded for damaging enemy near dropped bomb" );

    ConVar score_dropped_bomb_proximity_damage_bonus_radius_inner(
        "score_dropped_bomb_proximity_damage_bonus_radius_inner",
        "120",
        FCVAR_NONE,
        "Inner radius (full bonus) for doing damage near dropped bomb" );

    ConVar score_dropped_bomb_proximity_damage_bonus_radius_outer(
        "score_dropped_bomb_proximity_damage_bonus_radius_outer",
        "600",
        FCVAR_NONE,
        "Outer radius (zero bonus) for doing damage near dropped bomb" );

    ConVar score_dropped_defuser_proximity_damage_bonus(
        "score_dropped_defuser_proximity_damage_bonus",
        "1",
        FCVAR_NONE,
        "Points awarded for damaging enemy near dropped defuser" );

    ConVar score_dropped_defuser_proximity_damage_radius_inner(
        "score_dropped_defuser_proximity_damage_radius_inner",
        "120",
        FCVAR_NONE,
        "Inner radius (full bonus) for doing damage near dropped defuser" );

    ConVar score_dropped_defuser_proximity_damage_radius_outer(
        "score_dropped_defuser_proximity_damage_radius_outer",
        "600",
        FCVAR_NONE,
        "Outer radius (zero bonus) for doing damage near dropped defuser" );

    ConVar score_bomb_plant_bonus(
        "score_bomb_plant_bonus",
        "200",
        FCVAR_NONE,
        "Points awarded for planting or assisting with planting the bomb" );

    ConVar score_bomb_plant_radius_inner(
        "score_bomb_plant_radius_inner",
        "120",
        FCVAR_NONE,
        "Inner radius (full bonus) for planting or assisting with planting the bomb" );

    ConVar score_bomb_plant_radius_outer(
        "score_bomb_plant_radius_outer",
        "600",
        FCVAR_NONE,
        "Outer radius (zero bonus) for planting or assisting with planting the bomb" );

    ConVar score_bomb_defuse_bonus(
        "score_bomb_defuse_bonus",
        "400",
        FCVAR_NONE,
        "Points awarded for defusing or assisting with defuse of bomb" );

    ConVar score_bomb_defuse_radius_inner(
        "score_bomb_defuse_radius_inner",
        "120",
        FCVAR_NONE,
        "Inner radius (full bonus) for defusing or assisting with defusing the bomb" );

    ConVar score_bomb_defuse_radius_outer(
        "score_bomb_defuse_radius_outer",
        "600",
        FCVAR_NONE,
        "Outer radius (zero bonus) for defusing or assisting with defseing the bomb" );

    ConVar score_hostage_rescue_bonus(
        "score_hostage_rescue_bonus",
        "100",
        FCVAR_NONE,
        "Points awarded for rescuing a hostage" );

    ConVar score_hostage_rescue_radius_inner(
        "score_hostage_rescue_radius_inner",
        "120",
        FCVAR_NONE,
        "Inner radius (full bonus) for rescuing hostage" );

    ConVar score_hostage_rescue_radius_outer(
        "score_hostage_rescue_radius_outer",
        "600",
        FCVAR_NONE,
        "Outer radius (zero bonus) for rescuing hostage" );

    ConVar score_hostage_damage_penalty(
        "score_hostage_damage_penalty",
        "2",
        FCVAR_NONE,
        "Penalty for damaging a hostage" );

    ConVar score_blind_enemy_bonus(
        "score_blind_enemy_bonus",
        "10",
        FCVAR_NONE,
        "Bonus for blinding enemy players" );

    ConVar score_blind_friendly_penalty(
        "score_blind_friendly_penalty",
        "10",
        FCVAR_NONE,
        "Penalty for blinding friendly players" );

    ConVar score_typical_good_score(
        "score_typical_good_score",
        "5",
        FCVAR_NONE,
        "An average good score for use in funfacts" );

    ConVar contributionscore_assist(
        "contributionscore_assist",
        "1",
        FCVAR_RELEASE | FCVAR_GAMEDLL,
        "amount of contribution score added for an assist" );

    ConVar contributionscore_kill(
        "contributionscore_kill",
        "2",
		FCVAR_RELEASE | FCVAR_GAMEDLL,
        "amount of contribution score added for a kill" );

    ConVar contributionscore_objective_kill(
        "contributionscore_objective_kill",
        "3",
		FCVAR_RELEASE | FCVAR_GAMEDLL,
        "amount of contribution score added for an objective related kill" );

	ConVar contributionscore_hostage_rescue_minor(
		"contributionscore_hostage_rescue_minor",
		"1",
		FCVAR_RELEASE | FCVAR_GAMEDLL,
		"amount of contribution score added to all alive CTs per hostage rescued" );

    ConVar contributionscore_hostage_rescue_major(
        "contributionscore_hostage_rescue_major",
        "3",
		FCVAR_RELEASE | FCVAR_GAMEDLL,
        "amount of contribution score added to rescuer per hostage rescued" );

    ConVar contributionscore_bomb_defuse_minor(
        "contributionscore_bomb_defuse_minor",
        "1",
		FCVAR_RELEASE | FCVAR_GAMEDLL,
        "amount of contribution score for defusing a bomb after eliminating enemy team" );

	ConVar contributionscore_bomb_defuse_major(
		"contributionscore_bomb_defuse_major",
		"3",
		FCVAR_RELEASE | FCVAR_GAMEDLL,
		"amount of contribution score for defusing a bomb while at least one enemy remains alive" );


    ConVar contributionscore_bomb_planted(
        "contributionscore_bomb_planted",
        "2",
		FCVAR_RELEASE | FCVAR_GAMEDLL,
        "amount of contribution score for planting a bomb" );

	ConVar contributionscore_bomb_exploded(
		"contributionscore_bomb_exploded",
		"1",
		FCVAR_RELEASE | FCVAR_GAMEDLL,
		"amount of contribution score awarded to bomb planter and terrorists remaining alive if bomb explosion wins the round" );


    ConVar contributionscore_suicide(
        "contributionscore_suicide",
        "-2",
		FCVAR_RELEASE | FCVAR_GAMEDLL,
        "amount of contribution score for a suicide, normally negative" );

    ConVar contributionscore_team_kill(
        "contributionscore_team_kill",
        "-2",
		FCVAR_RELEASE | FCVAR_GAMEDLL,
        "amount of contribution score for a team kill, normally negative" );

    ConVar contributionscore_hostage_kill(
        "contributionscore_hostage_kill",
        "-2",
		FCVAR_RELEASE | FCVAR_GAMEDLL,
        "amount of contribution score for killing a hostage, normally negative" );

	ConVar contributionscore_cash_bundle(
		"contributionscore_cash_bundle",
		"0",
		FCVAR_RELEASE | FCVAR_GAMEDLL,
		"amount of contribution score for picking up a cash bundle" );

	ConVar contributionscore_crate_break(
		"contributionscore_crate_break",
		"0",
		FCVAR_RELEASE | FCVAR_GAMEDLL,
		"amount of contribution score for breaking an item crate" );

	ConVar contributionscore_kill_factor(
		"contributionscore_kill_factor",
		"0",
		FCVAR_RELEASE | FCVAR_GAMEDLL,
		"percentage of victim's contribution score to award to their killer as a bonus" );

    // --------------------------------------------------------------------------------------------------- //
    // Global helper functions.
    // --------------------------------------------------------------------------------------------------- //

    void InitBodyQue(void)
    {
        // FIXME: Make this work
    }


    Vector DropToGround( 
        CBaseEntity *pMainEnt, 
        const Vector &vPos, 
        const Vector &vMins, 
        const Vector &vMaxs )
    {
        trace_t trace;
        UTIL_TraceHull( vPos, vPos + Vector( 0, 0, -500 ), vMins, vMaxs, MASK_SOLID, pMainEnt, COLLISION_GROUP_NONE, &trace );

		// lift the endpos off the ground a bit so the physics system doesn't have a chance to think a pos is underground
		trace.endpos.z += 0.1f;

        return trace.endpos;
    }


    //-----------------------------------------------------------------------------
    // Purpose: This function can be used to find a valid placement location for an entity.
    //			Given an origin to start looking from and a minimum radius to place the entity at,
    //			it will sweep out a circle around vOrigin and try to find a valid spot (on the ground)
    //			where mins and maxs will fit.
    // Input  : *pMainEnt - Entity to place
    //			&vOrigin - Point to search around
    //			fRadius - Radius to search within
    //			nTries - Number of tries to attempt
    //			&mins - mins of the Entity
    //			&maxs - maxs of the Entity
    //			&outPos - Return point
    // Output : Returns true and fills in outPos if it found a spot.
    //-----------------------------------------------------------------------------
    bool EntityPlacementTest( CBaseEntity *pMainEnt, const Vector &vOrigin, Vector &outPos, bool bDropToGround, unsigned int mask, ITraceFilter *pFilter )
    {
        // This function moves the box out in each dimension in each step trying to find empty space like this:
        //
        //											  X  
        //							   X			  X  
        // Step 1:   X     Step 2:    XXX   Step 3: XXXXX
        //							   X 			  X  
        //											  X  
        //

        CTraceFilterSimple defaultFilter( pMainEnt, COLLISION_GROUP_NONE );
        if ( !pFilter )
        {
            pFilter = &defaultFilter;
        }

        Vector mins, maxs;
        if ( pMainEnt )
        {
            pMainEnt->CollisionProp()->WorldSpaceAABB( &mins, &maxs );
            mins -= pMainEnt->GetAbsOrigin();
            maxs -= pMainEnt->GetAbsOrigin();
        }
        else
        {
            mins = VEC_HULL_MIN;
            maxs = VEC_HULL_MAX;
        }

        // Put some padding on their bbox.
        float flPadSize = 5;
        Vector vTestMins = mins - Vector( flPadSize, flPadSize, flPadSize );
        Vector vTestMaxs = maxs + Vector( flPadSize, flPadSize, flPadSize );

        // First test the starting origin.
        if ( UTIL_IsSpaceEmpty( pMainEnt, vOrigin + vTestMins, vOrigin + vTestMaxs, mask, pFilter ) )
        {
            if ( bDropToGround )
            {
                outPos = DropToGround( pMainEnt, vOrigin, vTestMins, vTestMaxs );
            }
            else
            {
                outPos = vOrigin;
            }
            return true;
        }

        Vector vDims = vTestMaxs - vTestMins;

        // Keep branching out until we get too far.
        int iCurIteration = 0;
        int nMaxIterations = 15;
        
        int offset = 0;
        do
        {
            for ( int iDim=0; iDim < 3; iDim++ )
            {
                float flCurOffset = offset * vDims[iDim];

                for ( int iSign=0; iSign < 2; iSign++ )
                {
                    Vector vBase = vOrigin;
                    vBase[iDim] += (iSign*2-1) * flCurOffset;
                
                    if ( UTIL_IsSpaceEmpty( pMainEnt, vBase + vTestMins, vBase + vTestMaxs, mask, pFilter ) )
                    {
                        // Ensure that there is a clear line of sight from the spawnpoint entity to the actual spawn point.
                        // (Useful for keeping things from spawning behind walls near a spawn point)
                        trace_t tr;
                        UTIL_TraceLine( vOrigin, vBase, mask, pFilter, &tr );

                        if ( tr.fraction != 1.0 )
                        {
                            continue;
                        }
                        
                        if ( bDropToGround )
                            outPos = DropToGround( pMainEnt, vBase, vTestMins, vTestMaxs );
                        else
                            outPos = vBase;

                        return true;
                    }
                }
            }

            ++offset;
        } while ( iCurIteration++ < nMaxIterations );

    //	Warning( "EntityPlacementTest for ent %d:%s failed!\n", pMainEnt->entindex(), pMainEnt->GetClassname() );
        return false;
    }

    // Returns the number of human spectators in the game
    int UTIL_SpectatorsInGame( void )
    {
        int iCount = 0;

        for ( int i = 1; i <= gpGlobals->maxClients; i++ )
        {
            CCSPlayer *entity = CCSPlayer::Instance( i );

            if ( entity && !FNullEnt( entity->edict() ) )
            {
                if ( FStrEq( entity->GetPlayerName(), "" ) )
                    continue;

                if ( FBitSet( entity->GetFlags(), FL_FAKECLIENT ) )
                    continue;

                if ( entity->IsBot() )
                    continue;

                if ( entity->GetTeamNumber() == TEAM_SPECTATOR )
                {
                    iCount++;
                }
            }
        }

        return iCount;
    }

    int UTIL_HumansInGame( bool ignoreSpectators, bool ignoreUnassigned )
    {
        int iCount = 0;

        for ( int i = 1; i <= gpGlobals->maxClients; i++ )
        {
            CCSPlayer *entity = CCSPlayer::Instance( i );

            if ( entity && !FNullEnt( entity->edict() ) )
            {
                if ( FStrEq( entity->GetPlayerName(), "" ) )
                    continue;

                if ( FBitSet( entity->GetFlags(), FL_FAKECLIENT ) )
                    continue;

                if ( ignoreSpectators && entity->GetTeamNumber() != TEAM_TERRORIST && entity->GetTeamNumber() != TEAM_CT )
                    continue;

                if ( ignoreSpectators && entity->State_Get() == STATE_PICKINGCLASS )
                    continue;

                if ( ignoreUnassigned && entity->GetTeamNumber() == TEAM_UNASSIGNED )
                    continue;

                iCount++;
            }
        }

        return iCount;
    }


#if defined ( GAME_DLL )
	void UTIL_RadioFromRandomPlayer( int teamNumber, const char* pszRadioSound, const char* pszRadioText )
	{
		CCSPlayer* options[MAX_PLAYERS];
		int numOptions = 0;
		for ( int i = 1; i <= MAX_PLAYERS; ++i )
		{
			CCSPlayer* pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );
			
			if ( !pPlayer )
				continue;

			if ( pPlayer->GetTeamNumber() != teamNumber )
				continue;

			if ( pPlayer->State_Get() != STATE_ACTIVE )
				continue;

			if ( !pPlayer->IsAlive() )
				continue;

			options[numOptions++] = pPlayer;
		}

		if ( numOptions == 0 )
			return;

		int choice = RandomInt( 0, numOptions - 1 );
		CCSPlayer* playerChoice = options[choice];
		playerChoice->Radio( pszRadioSound, pszRadioText, true );
	}

	bool CCSGameRules::CheckGotGuardianModeSpecialKill( CCSPlayer* pAttacker, CCSPlayer* pVictim, const CTakeDamageInfo& damageInfo )
	{
		if ( IsPlayingCoopGuardian() == false )
			return false;

		if ( IsWarmupPeriod() )
			return false;

		const char* specialWeapon = mp_guardian_special_weapon_needed.GetString();
		if ( specialWeapon && strchr( specialWeapon, '%' ) )
		{
			CExpressionCalculator expr( specialWeapon );
//			pAttacker->Helper_SetCommonQuestVariables( expr, &damageInfo, pVictim );
			float result = 0;
			if ( !expr.Evaluate( result ) )
				return false;

			if ( result <= 0.0f )
				return false;
		}
		else if ( m_nGuardianModeSpecialWeaponNeeded != 0 )
		{
			CWeaponCSBase* pAttackerWeapon = dynamic_cast< CWeaponCSBase* >( damageInfo.GetWeapon() );

			if ( !pAttackerWeapon )
				return false;

			const CEconItemView *pEconItemViewWeapon = pAttackerWeapon->GetEconItemView();
			if ( !pEconItemViewWeapon || !pEconItemViewWeapon->GetItemDefinition() )
				return false;

			if ( m_nGuardianModeSpecialWeaponNeeded != pEconItemViewWeapon->GetItemDefinition()->GetDefinitionIndex() )
				return false;
		}

		// reduce # of kills needed
		int totalKills = mp_guardian_special_kills_needed.GetInt();
		float oldPercent = ( totalKills > 0 ) ? ( m_nGuardianModeSpecialKillsRemaining / ( float )totalKills ) : 0.0f;
		m_nGuardianModeSpecialKillsRemaining = MAX( m_nGuardianModeSpecialKillsRemaining - 1, 0 );
		float newPercent = ( totalKills > 0 ) ? ( m_nGuardianModeSpecialKillsRemaining / ( float )totalKills ) : 0.0f;

		// Radio message when halfway done
		if ( oldPercent > 0.5f && newPercent <= 0.5f && m_nGuardianModeSpecialKillsRemaining > 0 )
		{
			UTIL_GlobalChatter( "Guardian.HalfwayThere" );
		}

		// REI: Should we send a game event message here?  Right now we rely on network synchronization of
		//      m_nGuardianModeSpecialKillsRemaining and notice when it changes on the client to update
		//      their UI.

		if ( m_nGuardianModeSpecialKillsRemaining <= 0 )
			GuardianAllKillsAchievedCheck();

		return true;
	}
#endif

#if CS_CONTROLLABLE_BOTS_ENABLED
 // DK TODO: Make a similar method run AFTER all loops of this to look for orphaned bots that think they are still player controlled
    class RevertBotsFunctor
    {
    public:
        bool operator()( CBasePlayer *basePlayer )
        {
            CCSPlayer *pPlayer = ToCSPlayer( basePlayer );
            if ( !pPlayer )
                return true;

            if ( !pPlayer->IsControllingBot() )
                return true;

            // this will properly handle restoring money, frag counts, etc
            pPlayer->ReleaseControlOfBot();	

            return true;
        }
    };
#endif


    CCSMatch::CCSMatch()
    {
        Reset();
    }

    void CCSMatch::Reset( void )
    {
        m_actualRoundsPlayed = 0;
        CSGameRules()->SetTotalRoundsPlayed( 0 );
		m_nOvertimePlaying = 0;
		CSGameRules()->SetOvertimePlaying( 0 );

        m_ctScoreFirstHalf = 0;
        m_ctScoreSecondHalf = 0;
		m_ctScoreOvertime = 0;
        m_ctScoreTotal = 0;

        m_terroristScoreFirstHalf = 0;
        m_terroristScoreSecondHalf = 0;
		m_terroristScoreOvertime = 0;
        m_terroristScoreTotal = 0;
        
        if ( CSGameRules()->HasHalfTime() )
        {
            SetPhase( GAMEPHASE_PLAYING_FIRST_HALF );
        }
        else
        {
            SetPhase( GAMEPHASE_PLAYING_STANDARD );
        }		
        UpdateTeamScores();
    }

    void CCSMatch::SetPhase( GamePhase phase )
    {
		CCSGameRules *pRules = CSGameRules();
		if ( ( m_phase == GAMEPHASE_HALFTIME ) && mp_halftime_pausematch.GetInt() && pRules )
		{	// when halftime is over, we pause the match if needed
			if ( !pRules->IsMatchWaitingForResume() )
			{
				UTIL_ClientPrintAll( HUD_PRINTCENTER, "#SFUI_Notice_Match_Will_Pause" );
			}
			pRules->SetMatchWaitingForResume( true, "mp_halftime_pausematch" );
		}

        m_phase = phase;

		// When going to overtime halftime pause the timer if requested
		if ( ( m_phase == GAMEPHASE_HALFTIME ) && m_nOvertimePlaying && mp_overtime_halftime_pausetimer.GetInt() )
			mp_halftime_pausetimer.SetValue( mp_overtime_halftime_pausetimer.GetInt() );

        EnableFullAlltalk( CSGameRules()->IsWarmupPeriod() || m_phase == GAMEPHASE_HALFTIME || m_phase == GAMEPHASE_MATCH_ENDED );

        CSGameRules()->SetGamePhase( phase );
    }

    void CCSMatch::AddTerroristWins( int numWins )
    {
        m_actualRoundsPlayed += numWins;
        CSGameRules()->SetTotalRoundsPlayed( m_actualRoundsPlayed );
        AddTerroristScore( numWins );
    }
    
    void CCSMatch::AddCTWins( int numWins )
    {
        m_actualRoundsPlayed += numWins;
        CSGameRules()->SetTotalRoundsPlayed( m_actualRoundsPlayed );
        AddCTScore( numWins );
    }

	void CCSMatch::IncrementRound( int nNumRounds )
	{
		m_actualRoundsPlayed += nNumRounds;
		CSGameRules()->SetTotalRoundsPlayed( m_actualRoundsPlayed );
	}

    void CCSMatch::AddTerroristBonusPoints( int points )
    {
        AddTerroristScore( points );
    }

    void CCSMatch::AddCTBonusPoints( int points)
    {
        AddCTScore( points );
    }		

    void CCSMatch::AddTerroristScore( int score )
    {
        m_terroristScoreTotal += score;

		if ( m_nOvertimePlaying > 0 )
		{
			m_terroristScoreOvertime += score;
		}
        else if ( m_phase == GAMEPHASE_PLAYING_FIRST_HALF )
        {
            m_terroristScoreFirstHalf += score;
        }
        else if ( m_phase == GAMEPHASE_PLAYING_SECOND_HALF )
        {
            m_terroristScoreSecondHalf += score;
        }
        UpdateTeamScores();
    }
    
    void CCSMatch::AddCTScore( int score )
    {
        m_ctScoreTotal += score;		

		if ( m_nOvertimePlaying > 0 )
		{
			m_ctScoreOvertime += score;
		}
		else if ( m_phase == GAMEPHASE_PLAYING_FIRST_HALF )
        {
            m_ctScoreFirstHalf += score;
        }
        else if ( m_phase == GAMEPHASE_PLAYING_SECOND_HALF )
        {
            m_ctScoreSecondHalf += score;
        }
        UpdateTeamScores();
    }

	void CCSMatch::GoToOvertime( int numOvertimesToAdd )
	{
		m_nOvertimePlaying += numOvertimesToAdd;
		CSGameRules()->SetOvertimePlaying( m_nOvertimePlaying );
	}

    void CCSMatch::SwapTeamScores( void )
    {
        short temp = m_terroristScoreFirstHalf;
        m_terroristScoreFirstHalf = m_ctScoreFirstHalf;
        m_ctScoreFirstHalf = temp;

        temp = m_terroristScoreSecondHalf;
        m_terroristScoreSecondHalf = m_ctScoreSecondHalf;
        m_ctScoreSecondHalf = temp;

		temp = m_terroristScoreOvertime;
		m_terroristScoreOvertime = m_ctScoreOvertime;
		m_ctScoreOvertime = temp;

        temp = m_terroristScoreTotal;
        m_terroristScoreTotal = m_ctScoreTotal;
        m_ctScoreTotal = temp;
        
        UpdateTeamScores();
    }

    void CCSMatch::UpdateTeamScores( void )
    {
        CTeam *pTerrorists = GetGlobalTeam( TEAM_TERRORIST );
        CTeam *pCTs = GetGlobalTeam( TEAM_CT );

        if ( pTerrorists )
        {
            pTerrorists->SetScore( m_terroristScoreTotal );
            pTerrorists->SetScoreFirstHalf( m_terroristScoreFirstHalf );
            pTerrorists->SetScoreSecondHalf( m_terroristScoreSecondHalf );
			pTerrorists->SetScoreOvertime( m_terroristScoreOvertime );
        }

        if ( pCTs )
        {
            pCTs->SetScore( m_ctScoreTotal);
            pCTs->SetScoreFirstHalf( m_ctScoreFirstHalf );
            pCTs->SetScoreSecondHalf( m_ctScoreSecondHalf );
			pCTs->SetScoreOvertime( m_ctScoreOvertime );
        }
    }

    void CCSMatch::EnableFullAlltalk( bool bEnable )
    {
		if ( !sv_auto_full_alltalk_during_warmup_half_end.GetBool() )
			bEnable = false;

        //static ConVarRef sv_full_alltalk( "sv_full_alltalk" ); // already defined in this file
        sv_full_alltalk.SetValue( bEnable );
    }

    int CCSMatch::GetWinningTeam( void )
    {
		CTeam *pTerrorists = GetGlobalTeam( TEAM_TERRORIST );
		CTeam *pCTs = GetGlobalTeam( TEAM_CT );

		if ( pTerrorists && pTerrorists->m_bSurrendered )
		{
			return TEAM_CT;
		}
		else if ( pCTs && pCTs->m_bSurrendered )
		{
			 return TEAM_TERRORIST;
		}
        else if ( m_terroristScoreTotal > m_ctScoreTotal )
        {
            return TEAM_TERRORIST;
        }
        else if ( m_terroristScoreTotal < m_ctScoreTotal )
        {
            return TEAM_CT;
        }
        else
        {
            return WINNER_NONE;
        }
    }

    template < class T > void VectorShuffle( CUtlVector< T > &arrayToShuffle )
    {
        int numEntries = arrayToShuffle.Count();

        // Shuffle entries
        for ( int i = 0; i < numEntries - 1; ++i )
        {
            int randVal = RandomInt( i, numEntries - 1 );

            if ( randVal != i )
            {
                // Swap values
                V_swap( arrayToShuffle[ i ], arrayToShuffle[ randVal ] );
            }
        }
    }


	// --------------------------------------------------------------------------------------------------- //
	// CCSGameRules implementation.
	// --------------------------------------------------------------------------------------------------- //
	CCSGameRules::GcBanInformationMap_t CCSGameRules::sm_mapGcBanInformation;

	CCSGameRules::CCSGameRules()
	{
		m_flLastThinkTime = gpGlobals->curtime;

		m_iRoundTime = 0;
		m_gamePhase = GAMEPHASE_PLAYING_STANDARD;
		m_iRoundWinStatus = WINNER_NONE;
		m_eRoundWinReason = RoundEndReason_StillInProgress;
		m_iFreezeTime = 0;
		m_totalRoundsPlayed = 0;
		m_nOvertimePlaying = 0;

		m_fMatchStartTime = gpGlobals->curtime;
		m_fRoundStartTime = 0;
		m_bAllowWeaponSwitch = true;
		m_bFreezePeriod = true;
		m_bMatchWaitingForResume = false;


		m_nTerroristTimeOuts = mp_team_timeout_max.GetInt();
		m_nCTTimeOuts = mp_team_timeout_max.GetInt();

		m_flTerroristTimeOutRemaining = mp_team_timeout_time.GetInt();
		m_flCTTimeOutRemaining = mp_team_timeout_time.GetInt();

		m_bTerroristTimeOutActive = false;
		m_bCTTimeOutActive = false;

		m_iNumTerrorist = m_iNumCT = 0;	// number of players per team
		m_flRestartRoundTime = cInitialRestartRoundTime; // restart first round as soon as possible
		m_timeUntilNextPhaseStarts = 0.0f;
		m_iNumSpawnableTerrorist = m_iNumSpawnableCT = 0;
		m_bFirstConnected = false;
		m_bCompleteReset = true;
		m_bPickNewTeamsOnReset = true;
		m_bScrambleTeamsOnRestart = false;
		m_bSwapTeamsOnRestart = false;
		m_iAccountTerrorist = m_iAccountCT = 0;		
		m_iNumConsecutiveCTLoses = 0;
		m_iNumConsecutiveTerroristLoses = 0;
		m_bTargetBombed = false;
		m_bBombDefused = false;
		m_iTotalRoundsPlayed = -1;
		m_endMatchOnRoundReset = false;
		m_endMatchOnThink = false;
		m_iUnBalancedRounds = 0;
		m_flGameStartTime = 0;
		m_iHostagesRemaining = 0;
		m_bAnyHostageReached = false;
		m_bLevelInitialized = false;
		m_flCoopRespawnAndHealTime = -1;

		m_bLogoMap = false;

		m_bPlayAllStepSoundsOnServer = mp_footsteps_serverside.GetBool();
		m_tmNextPeriodicThink = 0;

		m_flDMBonusStartTime = 0;
		m_flDMBonusTimeLength = 0;
		m_unDMBonusWeaponLoadoutSlot = 0;
		m_bDMBonusActive = false;

		m_bIsDroppingItems = false;
		m_iActiveAssassinationTargetMissionID = 0;

		m_flGuardianBuyUntilTime = -1;
		m_bCTCantBuy = false;
		m_bTCantBuy = false;
		m_bForceTeamChangeSilent = false;
		m_bLoadingRoundBackupData = false;
		m_pfnCalculateEndOfRoundMVPHook = NULL;

		m_bMapHasBombTarget = false;
		m_bMapHasRescueZone = false;

		m_iSpawnPointCount_Terrorist = 0;
		m_iSpawnPointCount_CT = 0;

		m_bBuyTimeEnded = false;

		m_bHasMatchStarted = false;

		m_nLastFreezeEndBeep = -1;

		m_iMaxNumTerrorists = 0;
		m_iMaxNumCTs = 0;

		m_bMapHasBuyZone = false;

		m_nNextMapInMapgroup = -1;

		m_bVoiceWonMatchBragFired = false;

		m_iLoserBonus = 0;

		m_iHostagesRescued = 0;
		m_iHostagesTouched = 0;
		m_flNextHostageAnnouncement = 0.0f;

		m_MatchDevice = BotProfileInputDevice::KB_MOUSE;

		// map vote for "season" map groups
		m_eEndMatchMapVoteState = k_EEndMatchMapVoteState_MatchInProgress;
		// clear the next level
		nextlevel.SetValue( "" );
		nextmode.SetValue( "" );

		// Set the bestof maps state
		m_numBestOfMaps = mp_teamscore_max.GetInt();

		// Set global gifts state
		m_numGlobalGiftsGiven = 0;
		m_numGlobalGifters = 0;
		m_numGlobalGiftsPeriodSeconds = 0;
		for ( int j = 0; j < MAX_GIFT_GIVERS_FEATURED_COUNT; ++ j )
		{
			m_arrFeaturedGiftersAccounts.Set( j, 0 );
			m_arrFeaturedGiftersGifts.Set( j, 0 );
		}
		CheckForGiftsLeaderboardUpdate();

		for ( int j = 0; j < MAX_TOURNAMENT_ACTIVE_CASTER_COUNT; ++ j )
		{
			m_arrTournamentActiveCasterAccounts.Set( j, 0 );
		}

		// Configure QMM settings
		m_bIsQuestEligible = IsQuestEligible();
		m_bIsQueuedMatchmaking = IsQueuedMatchmaking();
		m_bIsValveDS = IsValveDS();
		m_pQueuedMatchmakingReservationString = NULL;
		m_eQueuedMatchmakingRematchState = k_EQueuedMatchmakingRematchState_MatchInProgress;
		m_bNeedToAskPlayersForContinueVote = false;
		m_pQueuedMatchmakingReportedRoundStats = NULL;
		m_numTotalTournamentDrops = 0;
		m_numSpectatorsCountMax = 0;
		m_numSpectatorsCountMaxTV = 0;
		m_numSpectatorsCountMaxLnk = 0;
		m_numQueuedMatchmakingAccounts = 0;

		m_szTournamentEventName.GetForModify()[0] = 0;
		m_szTournamentEventStage.GetForModify()[0] = 0;
		Q_strncpy( m_szTournamentPredictionsTxt.GetForModify(), mp_teamprediction_txt.GetString(), MAX_PATH );
		Q_strncpy( m_szMatchStatTxt.GetForModify(), mp_teammatchstat_txt.GetString(), MAX_PATH );
		m_nTournamentPredictionsPct = 0;
		m_nMatchInfoShowType = k_MapMatchInfoShownCounts_None;
		m_flMatchInfoDecidedTime = gpGlobals->curtime;
		
		m_mapQueuedMatchmakingPlayersData.PurgeAndDeleteElements();

		for ( int j = 0; j < MAX_MATCH_STATS_ROUNDS; ++ j )
		{
			m_iMatchStats_PlayersAlive_T.GetForModify(j) = 0x3F;
			m_iMatchStats_PlayersAlive_CT.GetForModify(j) = 0x3F;
		}

		// Halloween
		m_nHalloweenMaskListSeed = RandomInt( 0, 30 );
#if 0
		//
		// Setting a bunch of overrides
		//
		sm_QueuedServerReservation.mutable_tournament_event()->set_event_name( "ESL One Katowice 2015 Vitaliy Test Championship" );
		sm_QueuedServerReservation.mutable_tournament_event()->set_event_stage_name( "Group Stage | Decider Match" );
		TournamentTeam *pTeam;
		pTeam = sm_QueuedServerReservation.add_tournament_teams();
		pTeam->set_team_tag( "NiP" );
		pTeam->set_team_flag( "SE" );
		pTeam->set_team_name( "Ninjas in Pyjamas" );
		// pTeam->set_team_clantag( "NiP.Trig" );
		if ( TournamentPlayer *pPlayer = pTeam->add_players() )
		{
			pPlayer->set_account_id( 102003 );
			pPlayer->set_player_nick( "GeT_RiGhT" );
			pPlayer->set_player_name( "Vitaliy Genkin" );
		}
		pTeam = sm_QueuedServerReservation.add_tournament_teams();
		pTeam->set_team_tag( "NAVI" );
		pTeam->set_team_flag( "UA" );
		pTeam->set_team_name( "Natus Vincere" );
		// pTeam->set_team_clantag( "NA'VI" );
		if ( TournamentPlayer *pPlayer = pTeam->add_players() )
		{
			pPlayer->set_account_id( 102003 );
			pPlayer->set_player_nick( "GeT_RiGhT" );
			pPlayer->set_player_name( "Vitaliy Genkin" );
		}
		sm_QueuedServerReservation.mutable_pre_match_data()->set_predictions_pct( 72 );
		CPreMatchInfoData_TeamStats *pTS;
#if 0 // Group A | Decider Match
		pTS = sm_QueuedServerReservation.mutable_pre_match_data()->add_stats();
		pTS->set_match_info_txt( "#CSGO_MatchInfoTxt_PreviouslyIn{name=#CSGO_MatchInfo_Stage_GroupA}" );
		pTS->add_match_info_teams()->assign( "#CSGO_MatchInfoTeam_WinVs{team=#CSGO_TeamID_37}" );
		pTS->add_match_info_teams()->assign( "#CSGO_MatchInfoTeam_WinVs{team=#CSGO_TeamID_26}" );
		pTS = sm_QueuedServerReservation.mutable_pre_match_data()->add_stats();
		pTS->set_match_info_txt( "#CSGO_MatchInfoTxt_PreviouslyIn{name=#CSGO_MatchInfo_Stage_GroupA}" );
		pTS->add_match_info_teams()->assign( "#CSGO_MatchInfoTeam_LossVs{team=#CSGO_TeamID_24}" );
		pTS->add_match_info_teams()->assign( "#CSGO_MatchInfoTeam_LossVs{team=#CSGO_TeamID_31}" );
		pTS = sm_QueuedServerReservation.mutable_pre_match_data()->add_stats();
		pTS->set_match_info_txt( "#CSGO_MatchInfoTxt_Group2{name=#CSGO_MatchInfo_Stage_GroupA}" );
		pTS->add_match_info_teams()->assign( "#CSGO_MatchInfoTeam_WinAdvan" );
		pTS->add_match_info_teams()->assign( "#CSGO_MatchInfoTeam_LossElim" );
		pTS = sm_QueuedServerReservation.mutable_pre_match_data()->add_stats();
		pTS->set_match_info_txt( "#CSGO_MatchInfoTxt_Group2{name=#CSGO_MatchInfo_Stage_GroupA}" );
		pTS->add_match_info_teams()->assign( "#CSGO_MatchInfoTeam_LossElim" );
		pTS->add_match_info_teams()->assign( "#CSGO_MatchInfoTeam_WinAdvan" );
#endif
#if 0 // Quarterfinal | Match 1 of 3
		pTS = sm_QueuedServerReservation.mutable_pre_match_data()->add_stats();
		pTS->set_match_info_txt( "#CSGO_MatchInfoTxt_Series2{name=#CSGO_MatchInfo_Stage_Quarterfinal}{idx=1}{count=3}" );
		pTS->add_match_info_teams()->assign( "0" );
		pTS->add_match_info_teams()->assign( "0" );
		pTS = sm_QueuedServerReservation.mutable_pre_match_data()->add_stats();
		pTS->set_match_info_txt( "#CSGO_MatchInfoTxt_PreviouslyIn{name=#CSGO_MatchInfo_Stage_Groups}" );
		pTS->add_match_info_teams()->assign( "#CSGO_MatchInfoTeam_QualPos1{name=#CSGO_MatchInfo_Stage_GroupA}" );
		pTS->add_match_info_teams()->assign( "#CSGO_MatchInfoTeam_QualPos2{name=#CSGO_MatchInfo_Stage_GroupB}" );
		pTS = sm_QueuedServerReservation.mutable_pre_match_data()->add_stats();
		pTS->set_match_info_txt( "#CSGO_MatchInfoTxt_PreviouslyIn{name=#CSGO_MatchInfo_Stage_Groups}" );
		pTS->add_match_info_teams()->assign( "#CSGO_MatchInfoTeam_WinVs{team=#CSGO_TeamID_37}" );
		pTS->add_match_info_teams()->assign( "#CSGO_MatchInfoTeam_WinVs{team=#CSGO_TeamID_26}" );
#endif
#if 0 // Quarterfinal | Match 2 of 3
		pTS = sm_QueuedServerReservation.mutable_pre_match_data()->add_stats();
		pTS->set_match_info_txt( "#CSGO_MatchInfoTxt_Series2{name=#CSGO_MatchInfo_Stage_Quarterfinal}{idx=2}{count=3}" );
		pTS->add_match_info_teams()->assign( "1" );
		pTS->add_match_info_teams()->assign( "0" );
		pTS = sm_QueuedServerReservation.mutable_pre_match_data()->add_stats();
		pTS->set_match_info_txt( "#CSGO_MatchInfoTxt_JustPlayedMap{map=#SFUI_Map_de_cbble}" );
		pTS->add_match_info_teams()->assign( "#CSGO_MatchInfoTeam_WinScoreMap{map=#SFUI_Map_de_cbble}{high=16}{low=3}" );
		pTS->add_match_info_teams()->assign( "#CSGO_MatchInfoTeam_Loss{map=#SFUI_Map_de_cbble}{high=16}{low=3}" );
		pTS = sm_QueuedServerReservation.mutable_pre_match_data()->add_stats();
		pTS->set_match_info_txt( "#CSGO_MatchInfoTxt_PreviouslyIn{name=#CSGO_MatchInfo_Stage_Groups}" );
		pTS->add_match_info_teams()->assign( "#CSGO_MatchInfoTeam_QualPos1{name=#CSGO_MatchInfo_Stage_GroupA}" );
		pTS->add_match_info_teams()->assign( "#CSGO_MatchInfoTeam_QualPos2{name=#CSGO_MatchInfo_Stage_GroupB}" );
		pTS = sm_QueuedServerReservation.mutable_pre_match_data()->add_stats();
		pTS->set_match_info_txt( "#CSGO_MatchInfoTxt_PreviouslyIn{name=#CSGO_MatchInfo_Stage_Groups}" );
		pTS->add_match_info_teams()->assign( "#CSGO_MatchInfoTeam_WinVs{team=#CSGO_TeamID_37}" );
		pTS->add_match_info_teams()->assign( "#CSGO_MatchInfoTeam_WinVs{team=#CSGO_TeamID_26}" );
		pTS = sm_QueuedServerReservation.mutable_pre_match_data()->add_stats();
		pTS->set_match_info_txt( "#CSGO_MatchInfoTxt_Series2{name=#CSGO_MatchInfo_Stage_Quarterfinal}{idx=2}{count=3}" );
		pTS->add_match_info_teams()->assign( "#CSGO_MatchInfoTeam_WinAdvan" );
		pTS->add_match_info_teams()->assign( "#CSGO_MatchInfoTeam_LossElim" );
#endif
#if 1 // Quarterfinal | Match 3 of 3
		pTS = sm_QueuedServerReservation.mutable_pre_match_data()->add_stats();
		pTS->set_match_info_txt( "#CSGO_MatchInfoTxt_BracketDecider{name=#CSGO_MatchInfo_Stage_Quarterfinal}" );
		pTS->add_match_info_teams()->assign( "1" );
		pTS->add_match_info_teams()->assign( "1" );
		pTS = sm_QueuedServerReservation.mutable_pre_match_data()->add_stats();
		pTS->set_match_info_txt( "#CSGO_MatchInfoTxt_JustPlayedMaps" );
		pTS->add_match_info_teams()->assign( "#CSGO_MatchInfoTeam_WinScoreMap{map=#SFUI_Map_de_cbble}{high=16}{low=3}" );
		pTS->add_match_info_teams()->assign( "#CSGO_MatchInfoTeam_WinScoreMap{map=#SFUI_Map_de_mirage}{high=23}{low=21}" );
		pTS = sm_QueuedServerReservation.mutable_pre_match_data()->add_stats();
		pTS->set_match_info_txt( "#CSGO_MatchInfoTxt_PreviouslyIn{name=#CSGO_MatchInfo_Stage_Groups}" );
		pTS->add_match_info_teams()->assign( "#CSGO_MatchInfoTeam_QualPos1{name=#CSGO_MatchInfo_Stage_GroupA}" );
		pTS->add_match_info_teams()->assign( "#CSGO_MatchInfoTeam_QualPos2{name=#CSGO_MatchInfo_Stage_GroupB}" );
		pTS = sm_QueuedServerReservation.mutable_pre_match_data()->add_stats();
		pTS->set_match_info_txt( "#CSGO_MatchInfoTxt_PreviouslyIn{name=#CSGO_MatchInfo_Stage_Groups}" );
		pTS->add_match_info_teams()->assign( "#CSGO_MatchInfoTeam_WinVs{team=#CSGO_TeamID_37}" );
		pTS->add_match_info_teams()->assign( "#CSGO_MatchInfoTeam_WinVs{team=#CSGO_TeamID_26}" );
		pTS = sm_QueuedServerReservation.mutable_pre_match_data()->add_stats();
		pTS->set_match_info_txt( "#CSGO_MatchInfoTxt_BracketDecider{name=#CSGO_MatchInfo_Stage_Quarterfinal}" );
		pTS->add_match_info_teams()->assign( "#CSGO_MatchInfoTeam_WinAdvan" );
		pTS->add_match_info_teams()->assign( "#CSGO_MatchInfoTeam_LossElim" );
		pTS = sm_QueuedServerReservation.mutable_pre_match_data()->add_stats();
		pTS->set_match_info_txt( "#CSGO_MatchInfoTxt_BracketDecider{name=#CSGO_MatchInfo_Stage_Quarterfinal}" );
		pTS->add_match_info_teams()->assign( "#CSGO_MatchInfoTeam_LossElim" );
		pTS->add_match_info_teams()->assign( "#CSGO_MatchInfoTeam_WinAdvan" );
#endif
#endif

		if ( m_bIsQueuedMatchmaking )
		{
			static ConVarRef sv_mmqueue_reservation( "sv_mmqueue_reservation" );
			m_pQueuedMatchmakingReservationString = new char[ Q_strlen( sv_mmqueue_reservation.GetString() ) + 1 ];
			Q_strcpy( m_pQueuedMatchmakingReservationString, sv_mmqueue_reservation.GetString() );
			int iDraftIndex = 0;
			for ( char const *pszPrev = sv_mmqueue_reservation.GetString(), *pszNext = pszPrev;
				( pszNext = strchr( pszPrev, '[' ) ) != NULL; ( pszPrev = pszNext + 1 ), ( ++ iDraftIndex ) )
			{
				uint32 uiAccountId = 0;
				sscanf( pszNext, "[%x]", &uiAccountId );
				if ( uiAccountId )
				{
					++ m_numQueuedMatchmakingAccounts;

					CQMMPlayerData_t *pqmmPlayerData = new CQMMPlayerData_t;
					pqmmPlayerData->m_uiPlayerAccountId = uiAccountId;
					pqmmPlayerData->m_iDraftIndex = iDraftIndex;
					if ( CCSGameRules::CQMMPlayerData_t *pQMM = CSGameRules()->QueuedMatchmakingPlayersDataFind( uiAccountId ) )
						delete pQMM;
					m_mapQueuedMatchmakingPlayersData.InsertOrReplace( uiAccountId, pqmmPlayerData );
				}
			}

			// Set next level to the current level for potential rematch
			nextlevel.SetValue( STRING( gpGlobals->mapname ) );

			// Set the tournament settings
			if ( sm_QueuedServerReservation.has_tournament_event() &&
				sm_QueuedServerReservation.tournament_event().has_event_name() )
				Q_strncpy( m_szTournamentEventName.GetForModify(), sm_QueuedServerReservation.tournament_event().event_name().c_str(), MAX_PATH );
			if ( sm_QueuedServerReservation.has_tournament_event() &&
				sm_QueuedServerReservation.tournament_event().has_event_stage_name() )
				Q_strncpy( m_szTournamentEventStage.GetForModify(), sm_QueuedServerReservation.tournament_event().event_stage_name().c_str(), MAX_PATH );

			// Tournament Best-of-N settings go here
			if ( sm_QueuedServerReservation.has_pre_match_data() &&
				sm_QueuedServerReservation.pre_match_data().has_draft() &&
				sm_QueuedServerReservation.pre_match_data().draft().maps_count() > 1 )
			{
				m_numBestOfMaps = ( sm_QueuedServerReservation.pre_match_data().draft().maps_count() + 1 ) / 2; // BO3 = 2 wins
			}
		}

		// [tj] reset flawless and lossless round related flags
		m_bNoTerroristsKilled = true;
		m_bNoCTsKilled = true;
		m_bNoTerroristsDamaged = true;
		m_bNoCTsDamaged = true;
		m_bNoEnemiesKilled = true;
		m_pFirstKill = NULL;
		m_firstKillTime = 0;

		// [menglish] Reset fun fact values
		m_pFirstBlood = NULL;
		m_firstBloodTime = 0;

		m_pMVP = NULL;

		m_bCanDonateWeapons = true;

		// [dwenger] Reset rescue-related achievement values
		m_arrRescuers.RemoveAll();

		m_hostageWasInjured = false;
		m_hostageWasKilled = false;

		m_pFunFactManager = new CCSFunFactMgr();
		m_pFunFactManager->Init();

		m_iHaveEscaped = 0;
		m_bMapHasEscapeZone = false;
		m_iNumEscapers = 0;
		m_iNumEscapeRounds = 0;

		m_bMapHasBombZone = false;
		m_bBombDropped = false;
		m_bBombPlanted = false;
		m_bHasHostageBeenTouched = false;
		m_bDontIncrementCoopWave = false;

		m_bGunGameRespawnWithBomb = false;
		m_fGunGameBombRespawnTimer = 0.0f;		
		m_bRoundTimeWarningTriggered = false;

		m_iNumGunGameProgressiveWeaponsCT = 0;
		m_iNumGunGameProgressiveWeaponsT = 0;		
		m_bAllowWeaponSwitch = true;

		m_iSpectatorSlotCount = 0;

		m_nGuardianModeWaveNumber = 1;
		m_nGuardianModeSpecialKillsRemaining = -1;
		m_nGuardianModeSpecialWeaponNeeded = -1;
		m_nGuardianGrenadesToGiveBots = 0;
		m_nNumHeaviesToSpawn = 0;

		m_flNextHostageAnnouncement = gpGlobals->curtime;	// asap.

		m_phaseChangeAnnouncementTime = 0.0f;
		m_fNextUpdateTeamClanNamesTime = 0.0f;

		// Create the team managers
		for ( int i = 0; i < ARRAYSIZE( sTeamNames ); i++ )
		{
			CTeam *pTeam = static_cast<CTeam*>(CreateEntityByName( "cs_team_manager" ));
			pTeam->Init( sTeamNames[i], i );

			g_Teams.AddToTail( pTeam );
		}

		m_bHasTriggeredRoundStartMusic = false;
		m_bHasTriggeredCoopSpawnReset = false;

		InitializeGameTypeAndMode();

		if ( const char* szMapNameBase = V_GetFileName( STRING(gpGlobals->mapname) ) )
		{
			if ( (IsPlayingCustomGametype() ) 
				&& filesystem->FileExists( UTIL_VarArgs( "maps/cfg/%s.cfg", szMapNameBase ) ) )
			{
				// Execute a map specific cfg file to define the rules
				engine->ServerCommand( UTIL_VarArgs( "execwithwhitelist %s.cfg */maps\n", szMapNameBase ) );
				engine->ServerExecute();
			}
			else if ( IsPlayingCoopMission() )
			{
				// Execute a map specific cfg file to define the rules
				int nMissionNumber = 1;
				for ( KeyValues *kvLaunchOptions = engine->GetLaunchOptions()->GetFirstSubKey(); kvLaunchOptions; kvLaunchOptions = kvLaunchOptions->GetNextKey() )
				{
					if ( char const *szValue = StringAfterPrefix( kvLaunchOptions->GetString(), "mission" ) )
					{
						nMissionNumber = V_atoi( szValue );
						DevMsg( "Coop mission number = %d (parsed from %s)\n", nMissionNumber, kvLaunchOptions->GetString() );
						break;
					}
				}
				engine->ServerCommand( UTIL_VarArgs( "execwithwhitelist %s_%d.cfg */maps\n", szMapNameBase, nMissionNumber ) );
				engine->ServerExecute();

				// set coop games to always be online
				extern ConVar game_online;
				game_online.SetValue( 1 );
			}
			else if ( IsPlayingCoopGuardian() )
			{
				char szCfgName[ MAX_PATH ];
				if ( filesystem->FileExists( UTIL_VarArgs( "maps/cfg/guardian_%s.cfg", szMapNameBase ) ) )
					V_snprintf( szCfgName, sizeof( szCfgName ), "guardian_%s", szMapNameBase );
				else
					V_snprintf( szCfgName, sizeof( szCfgName ), "guardian_defaultmap" );

				// Execute a map specific cfg file to define the rules
				engine->ServerCommand( UTIL_VarArgs( "execwithwhitelist %s.cfg */maps\n", szCfgName ) );
				engine->ServerExecute();

				// set coop games to always be online
				extern ConVar game_online;
				game_online.SetValue( 1 );
			}
		}

		ReadMultiplayCvars();

		m_bVoteCalled = false;
		m_bServerVoteOnReset = false;
		m_flVoteCheckThrottle = 0;

		m_bGameRestart = false;

		m_bSwitchingTeamsAtRoundReset = false;

		m_fAutobalanceDisplayTime = 0.0f;
		m_AutobalanceStatus = AutobalanceStatus::NONE;

		m_iNextCTSpawnPoint = 0;
		m_iNextTerroristSpawnPoint = 0;

		m_iMaxGunGameProgressiveWeaponIndex = 0;
		// m_flDeferredCallDispatchTime = 0.0f;

		m_bWarmupPeriod = mp_do_warmup_period.GetBool();
		m_fWarmupNextChatNoticeTime = 0;
		m_fWarmupPeriodStart = gpGlobals->curtime;
		m_fWarmupPeriodEnd = GetWarmupPeriodEndTime();
		m_coopMissionManager = NULL;

		for ( int i = 0; i < MAX_TEAMS; i++ )
		{
			m_flNextRespawnWave.Set( i, 0 );
			m_TeamRespawnWaveTimes.Set( i, -1.0f );
		}

		for ( int iVoteOption = 0; iVoteOption < MAX_ENDMATCH_VOTE_PANELS; ++iVoteOption )
		{
			m_nEndMatchMapGroupVoteTypes.Set( iVoteOption, kEndMatchVoteType_Invalid );
			m_nEndMatchMapGroupVoteOptions.Set( iVoteOption, -1 );
		}
		m_nEndMatchMapVoteWinner = -1;

		m_coopBonusCoinsFound = 0;
		m_coopBonusPistolsOnly = true;
		m_coopPlayersInDeploymentZone = false;

		//m_pSun = NULL;
	}

    //-----------------------------------------------------------------------------
    // Purpose: 
    //-----------------------------------------------------------------------------
    CCSGameRules::~CCSGameRules()
    {
        // Note, don't delete each team since they are in the gEntList and will 
        // automatically be deleted from there, instead.
        g_Teams.Purge();
        
		delete m_pFunFactManager;
		m_pFunFactManager = NULL;

		delete m_pQueuedMatchmakingReportedRoundStats;
		m_pQueuedMatchmakingReportedRoundStats = NULL;

		delete m_pQueuedMatchmakingReservationString;
		m_pQueuedMatchmakingReservationString = NULL;

		m_mapQueuedMatchmakingPlayersData.PurgeAndDeleteElements();

		ClearItemsDroppedDuringMatch();
    }

    void CCSGameRules::RefreshSkillData( bool /* forceUpdate */ )
    {
        // NOTE[pmf]: The base class loads skill configuration files which we
        // do not want for CS:GO
    }


    //-----------------------------------------------------------------------------
    // Purpose: 
    //-----------------------------------------------------------------------------
    void CCSGameRules::UpdateClientData( CBasePlayer *player )
    {
    }

    int CCSGameRules::GetMaxHumanPlayers() const
    {
        int iGameType = g_pGameTypes->GetCurrentGameType();
        int iGameMode = g_pGameTypes->GetCurrentGameMode();

		// Cooperative game modes have maxplayers 10 so that 5 bots appear on the enemy team
		if ( iGameType == CS_GameType_Cooperative )
			return 2;
		
        return g_pGameTypes->GetMaxPlayersForTypeAndMode( iGameType, iGameMode, sv_skirmish_id.GetInt() );
    }

    void CCSGameRules::ClientCommandKeyValues( edict_t *pEntity, KeyValues *pKeyValues )
    {
        CCSPlayer *pPlayer = ToCSPlayer( GetContainingEntity( pEntity ) );
        if ( !pPlayer )
            return;

		if ( ( 0 == Q_strcmp( pKeyValues->GetName(), "ClanTagChanged" ) ) &&
			// When we have tournament system enabled then players cannot change their clan tags from client
			CanClientCustomizeOwnIdentity() )
        {
            pPlayer->SetClanTag( pKeyValues->GetString( "tag", "" ) );
			const char *szClanName = pKeyValues->GetString( "name", "" );
			pPlayer->SetClanName( szClanName );

			UpdateTeamClanNames( TEAM_TERRORIST ); 
			UpdateTeamClanNames( TEAM_CT );
        }
		else if ( 0 == Q_strcmp( pKeyValues->GetName(), "InvalidSteamLogon" ) )
		{
			if ( IsWarmupPeriod() || IsFreezePeriod() || !pPlayer->IsAlive() )
				pKeyValues->SetBool( "disconnect", true );
			else
				pPlayer->m_bInvalidSteamLogonDelayed = true;
		}
    }

static bool Helper_CheckFieldAppliesToTeam( char const *szField, int nTeam )
{
	if ( StringHasPrefix( szField, "#CSGO_MatchInfoTeam_WinAdvan" ) )
	{
		// Make sure that this team is winning
		int nOtherTeam = ( ( nTeam == TEAM_CT ) ? TEAM_TERRORIST : ( ( nTeam == TEAM_TERRORIST ) ? TEAM_CT : nTeam ) );
		return ( GetGlobalTeam( nTeam ) ? GetGlobalTeam( nTeam )->GetScore() : 0 ) > ( GetGlobalTeam( nOtherTeam ) ? GetGlobalTeam( nOtherTeam )->GetScore() : 0 );
	}
	else if ( StringHasPrefix( szField, "#CSGO_MatchInfoTeam_LossElim" ) )
	{
		// Make sure that this team is losing
		int nOtherTeam = ( ( nTeam == TEAM_CT ) ? TEAM_TERRORIST : ( ( nTeam == TEAM_TERRORIST ) ? TEAM_CT : nTeam ) );
		return ( GetGlobalTeam( nTeam ) ? GetGlobalTeam( nTeam )->GetScore() : 0 ) < ( GetGlobalTeam( nOtherTeam ) ? GetGlobalTeam( nOtherTeam )->GetScore() : 0 );
	}
	else if ( ( szField[0] >= '0' ) && ( szField[0] <= '9' ) && ( szField[1] == 0 ) &&
		CSGameRules() && ( ( CSGameRules()->m_match.GetPhase() == GAMEPHASE_HALFTIME ) || ( CSGameRules()->m_match.GetPhase() == GAMEPHASE_MATCH_ENDED ) ) )
		return false;
	else
		return true;
}

void CCSGameRules::NotifySpawn( CBaseEntity* pEnt )
{
	// let gamerules do something when an entity spawns
	if ( CSGameRules() && CSGameRules()->IsPlayingSurvival() )
	{
		CSurvivalGameRules* pBRrules = CSGameRules()->GetSurvivalRules();
		if ( pBRrules )
		{
			if ( pEnt->IsBaseCombatWeapon() )
				pEnt->m_bEligibleForScreenHighlight = true;

			CBaseAnimating *pEntAnim = pEnt->GetBaseAnimating();
			if ( pEntAnim )
				pBRrules->ApplyHighlightColorToBaseAnimatingViaClassname( pEntAnim );
		}
	}
}

	void CCSGameRules::UpdateTeamPredictions()
	{
		int nWantPrediction = 0;
		if ( ( sm_QueuedServerReservation.pre_match_data().predictions_pct() >= 1 ) &&
			( sm_QueuedServerReservation.pre_match_data().predictions_pct() <= 99 ) )
			nWantPrediction = int( sm_QueuedServerReservation.pre_match_data().predictions_pct() ); // but convar can override
		if ( ( mp_teamprediction_pct.GetInt() >= 1 ) &&
			( mp_teamprediction_pct.GetInt() <= 99 ) )
			nWantPrediction = mp_teamprediction_pct.GetInt();
		
		// Prediction UI component cannot be displayed at certain times
		if ( ( m_match.GetPhase() == GAMEPHASE_HALFTIME ) || ( m_match.GetPhase() == GAMEPHASE_MATCH_ENDED ) )
			nWantPrediction = 0;

		if ( Q_strncmp( m_szTournamentPredictionsTxt, mp_teamprediction_txt.GetString(), MAX_PATH - 1 ) )
			Q_strncpy( m_szTournamentPredictionsTxt.GetForModify(), mp_teamprediction_txt.GetString(), MAX_PATH );

		char const *szWantMatchStatTxt = mp_teammatchstat_txt.GetString(); // can override from reservation later
		if ( sm_QueuedServerReservation.pre_match_data().stats().size() )
			szWantMatchStatTxt = sm_QueuedServerReservation.pre_match_data().stats( 0 ).match_info_txt().c_str(); // to ensure that it is eligible for a pick

		//
		// Here we must determine which statistics we are going to be showing
		//
		if ( IsWarmupPeriod() )
		{
			// we are going to show the draft here
			if ( sm_QueuedServerReservation.pre_match_data().stats().size() )
				m_nMatchInfoShowType = 0;
			else if ( *szWantMatchStatTxt )
				m_nMatchInfoShowType = 0;
			else if ( nWantPrediction )
				m_nMatchInfoShowType = k_MapMatchInfoShownCounts_Predictions;
			else
				m_nMatchInfoShowType = k_MapMatchInfoShownCounts_None;

			m_flMatchInfoDecidedTime = gpGlobals->curtime;
		}
		else if ( IsFreezePeriod() || ( m_iRoundWinStatus != WINNER_NONE ) )
		{
		if ( ( m_nMatchInfoShowType == k_MapMatchInfoShownCounts_None ) ||
			( gpGlobals->curtime - m_flMatchInfoDecidedTime > mp_teammatchstat_cycletime.GetFloat() ) )
		{
			m_nMatchInfoShowType = k_MapMatchInfoShownCounts_None;
			CUtlVectorFixedGrowable< int32, 10 > arrOptions;
			bool bTeamsAreSwitched = AreTeamsPlayingSwitchedSides();
			if ( nWantPrediction )
				arrOptions.AddToTail( k_MapMatchInfoShownCounts_Predictions );
			for ( int j = 0; j < sm_QueuedServerReservation.pre_match_data().stats().size(); ++ j )
			{
				if ( sm_QueuedServerReservation.pre_match_data().stats( j ).match_info_teams().size() < 2 ) continue;
				if ( ( m_match.GetPhase() == GAMEPHASE_MATCH_ENDED ) && ( m_nMatchInfoShowType == k_MapMatchInfoShownCounts_None ) )
				{
					if (
						( ( StringHasPrefix( sm_QueuedServerReservation.pre_match_data().stats( j ).match_info_teams(0).c_str(), "#CSGO_MatchInfoTeam_WinAdvan" ) ||
						StringHasPrefix( sm_QueuedServerReservation.pre_match_data().stats( j ).match_info_teams(0).c_str(), "#CSGO_MatchInfoTeam_LossElim" ) ) &&
						Helper_CheckFieldAppliesToTeam( sm_QueuedServerReservation.pre_match_data().stats( j ).match_info_teams(0).c_str(), bTeamsAreSwitched ? TEAM_TERRORIST : TEAM_CT ) )
						||
						( ( StringHasPrefix( sm_QueuedServerReservation.pre_match_data().stats( j ).match_info_teams(1).c_str(), "#CSGO_MatchInfoTeam_WinAdvan" ) ||
						StringHasPrefix( sm_QueuedServerReservation.pre_match_data().stats( j ).match_info_teams(1).c_str(), "#CSGO_MatchInfoTeam_LossElim" ) ) &&
						Helper_CheckFieldAppliesToTeam( sm_QueuedServerReservation.pre_match_data().stats( j ).match_info_teams(1).c_str(), bTeamsAreSwitched ? TEAM_CT : TEAM_TERRORIST ) )
						)
					{	// always conclude the match with advances/eliminated notification if such is applicable
						m_nMatchInfoShowType = j;
					}
				}

				if ( Helper_CheckFieldAppliesToTeam( sm_QueuedServerReservation.pre_match_data().stats( j ).match_info_teams(0).c_str(), bTeamsAreSwitched ? TEAM_TERRORIST : TEAM_CT ) ||
					Helper_CheckFieldAppliesToTeam( sm_QueuedServerReservation.pre_match_data().stats( j ).match_info_teams(1).c_str(), bTeamsAreSwitched ? TEAM_CT : TEAM_TERRORIST ) )
				{
					arrOptions.AddToTail( j );
				}
			}
			if ( ( !arrOptions.Count() || !sm_QueuedServerReservation.pre_match_data().stats().size() ) && *szWantMatchStatTxt )
				arrOptions.AddToTail( 0 );
			if ( arrOptions.Count() && ( m_nMatchInfoShowType == k_MapMatchInfoShownCounts_None ) )
			{
				// Pick the option that was shown the least number of times
				uint32 nLeastNumberOfTimesShown = ~uint32( 0 );
				FOR_EACH_VEC( arrOptions, iOption )
				{
					MapMatchInfoShownCounts::IndexType_t lookupIdx = m_mapMatchInfoShownCounts.Find( arrOptions[iOption] );
					uint32 nThisNumberOfTimesShown = 0;
					if ( lookupIdx != m_mapMatchInfoShownCounts.InvalidIndex() )
						nThisNumberOfTimesShown = m_mapMatchInfoShownCounts.Element( lookupIdx );
					if ( nThisNumberOfTimesShown < nLeastNumberOfTimesShown )
					{
						nLeastNumberOfTimesShown = nThisNumberOfTimesShown;
						m_nMatchInfoShowType = arrOptions[iOption];
					}
				}
			}
			if ( m_nMatchInfoShowType != k_MapMatchInfoShownCounts_None )
			{
				// Even if some stat was never picked prevent it from showing more than twice in a row
				uint32 numShown = 1;
				MapMatchInfoShownCounts::IndexType_t lookupIdx = m_mapMatchInfoShownCounts.Find( m_nMatchInfoShowType );
				if ( lookupIdx != m_mapMatchInfoShownCounts.InvalidIndex() )
					numShown = m_mapMatchInfoShownCounts.Element( lookupIdx ) + 1;
				FOR_EACH_MAP_FAST( m_mapMatchInfoShownCounts, iSCFast )
				{
					uint32 numOtherShown = m_mapMatchInfoShownCounts.Element( iSCFast );
					if ( numOtherShown > numShown + 1 )
						numShown = numOtherShown - 1;
				}

				// Track the option that was picked
				m_mapMatchInfoShownCounts.InsertOrReplace( m_nMatchInfoShowType, numShown );
			}
			m_flMatchInfoDecidedTime = gpGlobals->curtime;
		}
		}
		else if ( m_nMatchInfoShowType != k_MapMatchInfoShownCounts_None )
		{
			if ( m_flMatchInfoDecidedTime <= gpGlobals->curtime )
			{	// hold it a little longer to cover brief intermittent gamestate blips
				m_flMatchInfoDecidedTime = gpGlobals->curtime + mp_teammatchstat_holdtime.GetFloat();
			}
			else if ( m_flMatchInfoDecidedTime <= gpGlobals->curtime + 1.0f )
			{	// reset when < 1 sec remaining on hold timer
				m_nMatchInfoShowType = k_MapMatchInfoShownCounts_None;
			}
		}

		//
		// Kill the desire for things that weren't picked
		//
		if ( m_nMatchInfoShowType != k_MapMatchInfoShownCounts_Predictions )
			nWantPrediction = 0;

		if ( m_nMatchInfoShowType >= k_MapMatchInfoShownCounts_None )
			szWantMatchStatTxt = "";
		
		//
		// Set desired values
		//
		if ( nWantPrediction != m_nTournamentPredictionsPct )
			m_nTournamentPredictionsPct = nWantPrediction;

		if ( m_nMatchInfoShowType < k_MapMatchInfoShownCounts_None )
		{
			if ( sm_QueuedServerReservation.pre_match_data().stats().size() > m_nMatchInfoShowType )
			{
				int idxMatchTxt = m_nMatchInfoShowType;
				if ( sm_QueuedServerReservation.pre_match_data().stats( m_nMatchInfoShowType ).has_match_info_idxtxt() )
					idxMatchTxt = sm_QueuedServerReservation.pre_match_data().stats( m_nMatchInfoShowType ).match_info_idxtxt();
				szWantMatchStatTxt = sm_QueuedServerReservation.pre_match_data().stats( idxMatchTxt ).match_info_txt().c_str();
			}
		}

		if ( Q_strncmp( m_szMatchStatTxt, szWantMatchStatTxt, MAX_PATH - 1 ) )
			Q_strncpy( m_szMatchStatTxt.GetForModify(), szWantMatchStatTxt, MAX_PATH );
	}

	void CCSGameRules::UpdateTeamClanNames( int nTeam )
	{
		Assert( ( nTeam == TEAM_CT ) || ( nTeam == TEAM_TERRORIST ) );

		CTeam *pTeam = GetGlobalTeam( nTeam );
		//pTeam->SetName( GetDefaultTeamName(nTeam) );

		bool bTeamsAreSwitched = AreTeamsPlayingSwitchedSides();

		const char *(pTeamNames[ 2 ]) = { mp_teamname_2.GetString(), mp_teamname_1.GetString() };

		// If we have a competitive reservation then override team names from it
		if ( ( sm_QueuedServerReservation.tournament_teams().size() > 0 ) &&
			sm_QueuedServerReservation.tournament_teams(0).has_team_name() &&
			* sm_QueuedServerReservation.tournament_teams(0).team_name().c_str() )
			pTeamNames[1] = sm_QueuedServerReservation.tournament_teams(0).team_name().c_str();
		if ( ( sm_QueuedServerReservation.tournament_teams().size() > 1 ) &&
			sm_QueuedServerReservation.tournament_teams(1).has_team_name() &&
			* sm_QueuedServerReservation.tournament_teams(1).team_name().c_str() )
			pTeamNames[0] = sm_QueuedServerReservation.tournament_teams(1).team_name().c_str();

		int nTeamIndex = ( nTeam - TEAM_TERRORIST ); //  nTeamIndex == 0 if Terrorist, 1 if CT

		const char *pClanName = "";
		uint32 uiClanID = 0;
		

		// Set the team names to the convars depending on what half phase it is.
		if ( !bTeamsAreSwitched )
			pClanName = pTeamNames[ nTeamIndex ];
		else
			pClanName = pTeamNames[ 1 - nTeamIndex ];

		// The teamname convar was empty so differ to the team's clan name, if it exists.
		if ( StringIsEmpty( pClanName ) && IsClanTeam( pTeam ) )
		{
			for ( int iPlayer = 0; iPlayer < pTeam->GetNumPlayers(); iPlayer++ )
			{
				CCSPlayer *pPlayer = ToCSPlayer( pTeam->GetPlayer( iPlayer ) );
				if ( pPlayer && !pPlayer->IsBot() )
				{
					pClanName = pPlayer->GetClanName();

					const char *pClanID = engine->GetClientConVarValue( pPlayer->entindex(), "cl_clanid" );
					uiClanID = Q_atoi( pClanID );

					break;
				}
			}	
		}

		pTeam->SetClanName( pClanName );
		pTeam->SetClanID( uiClanID );

		//
		// Team flags processing
		//
		const char *pFlag = "";	
		const char *(pTeamFlags[ 2 ]) = { mp_teamflag_2.GetString(), mp_teamflag_1.GetString() };
		const char *pLogo = "";	
		const char *(pTeamLogos[ 2 ]) = { mp_teamlogo_2.GetString(), mp_teamlogo_1.GetString() };
		int numMapsWon = 0;
		int arrMapsWon[ 2 ] = { mp_teamscore_2.GetInt(), mp_teamscore_1.GetInt() };

		// If we have a competitive reservation then override team flags from it
		if ( ( sm_QueuedServerReservation.tournament_teams().size() > 0 ) &&
			sm_QueuedServerReservation.tournament_teams(0).has_team_flag() &&
			* sm_QueuedServerReservation.tournament_teams(0).team_flag().c_str() )
			pTeamFlags[1] = sm_QueuedServerReservation.tournament_teams(0).team_flag().c_str();

		if ( ( sm_QueuedServerReservation.tournament_teams().size() > 1 ) &&
			sm_QueuedServerReservation.tournament_teams(1).has_team_flag() &&
			* sm_QueuedServerReservation.tournament_teams(1).team_flag().c_str() )
			pTeamFlags[0] = sm_QueuedServerReservation.tournament_teams(1).team_flag().c_str();

		// get the logos
		if ( ( sm_QueuedServerReservation.tournament_teams().size() > 0 ) &&
			 sm_QueuedServerReservation.tournament_teams( 0 ).has_team_tag() &&
			 * sm_QueuedServerReservation.tournament_teams( 0 ).team_tag().c_str() )
			 pTeamLogos[1] = sm_QueuedServerReservation.tournament_teams( 0 ).team_tag().c_str() ;

		if ( ( sm_QueuedServerReservation.tournament_teams().size() > 01 ) &&
			 sm_QueuedServerReservation.tournament_teams( 1 ).has_team_tag() &&
			 * sm_QueuedServerReservation.tournament_teams( 1 ).team_tag().c_str() )
			 pTeamLogos[0] = sm_QueuedServerReservation.tournament_teams( 1 ).team_tag().c_str();

		if ( sm_QueuedServerReservation.pre_match_data().wins_size() >= 2 )
		{
			arrMapsWon[ 0 ] = sm_QueuedServerReservation.pre_match_data().wins( 1 );
			arrMapsWon[ 1 ] = sm_QueuedServerReservation.pre_match_data().wins( 0 );
		}

		// Set the team names to the convars depending on what half phase it is.
		if ( !bTeamsAreSwitched )
		{
			pFlag = pTeamFlags[ nTeamIndex ];
			pLogo = pTeamLogos[ nTeamIndex ];
			numMapsWon = arrMapsWon[ nTeamIndex ];
		}
		else
		{
			pFlag = pTeamFlags[ 1 - nTeamIndex ];
			pLogo = pTeamLogos[ 1 - nTeamIndex ];
			numMapsWon = arrMapsWon[ 1 - nTeamIndex ];
		}

		pTeam->SetFlagImageString( pFlag );
		pTeam->SetLogoImageString( pLogo );
		pTeam->SetNumMapVictories( numMapsWon );

		//
		// Team match stat processing
		//
		const char *pMatchStat = "";	
		const char *(pTeamMatchStats[ 2 ]) = { mp_teammatchstat_2.GetString(), mp_teammatchstat_1.GetString() };

		// If we have a competitive reservation then override team match stats from it
		bool bUsingReservation = false;
		if ( m_nMatchInfoShowType < k_MapMatchInfoShownCounts_None )
		{
			if ( ( sm_QueuedServerReservation.pre_match_data().stats().size() > m_nMatchInfoShowType ) &&
				( sm_QueuedServerReservation.pre_match_data().stats( m_nMatchInfoShowType ).match_info_teams().size() >= 2 ) )
			{
				bUsingReservation = true;
				pTeamMatchStats[1] = sm_QueuedServerReservation.pre_match_data().stats( m_nMatchInfoShowType ).match_info_teams(0).c_str();
				pTeamMatchStats[0] = sm_QueuedServerReservation.pre_match_data().stats( m_nMatchInfoShowType ).match_info_teams(1).c_str();
			}
		}
		else
		{
			pTeamMatchStats[0] = pTeamMatchStats[1] = "";
		}

		// Set the team match stats to the convars depending on what half phase it is.
		if ( !bTeamsAreSwitched )
			pMatchStat = pTeamMatchStats[ nTeamIndex ];
		else
			pMatchStat = pTeamMatchStats[ 1 - nTeamIndex ];

		if ( bUsingReservation && !Helper_CheckFieldAppliesToTeam( pMatchStat, nTeam ) )
			pMatchStat = "";

		Q_strncpy( pTeam->m_szTeamMatchStat.GetForModify(), pMatchStat, MAX_PATH );
	}

    // registered VSCRIPT functions
    void CCSGameRules::SetPlayerCompletedTraining( bool bCompleted )
    {

#ifndef CLIENT_DLL
        int nComplete = 0;
        if ( bCompleted )
            nComplete = 1;

        tr_completed_training.SetValue( nComplete );
#endif
    }

    bool CCSGameRules::GetPlayerCompletedTraining( void )
    {
#ifndef CLIENT_DLL
        return tr_completed_training.GetBool();
#else	
        return 0;
#endif
    }

    void CCSGameRules::SetBestTrainingCourseTime( int nTime )
    {
#ifndef CLIENT_DLL
        tr_best_course_time.SetValue( nTime );
#endif	
    }

    int CCSGameRules::GetBestTrainingCourseTime( void )
    {
#ifndef CLIENT_DLL
        return tr_best_course_time.GetInt();
#else	
        return 0;
#endif
    }

    int CCSGameRules::GetValveTrainingCourseTime( void )
    {
#ifndef CLIENT_DLL
        return tr_valve_course_time.GetInt();
#else	
        return 0;
#endif
    }

    bool CCSGameRules::IsLocalPlayerUsingController( void )
    {
        CCSPlayer *pPlayer = static_cast<CCSPlayer*>( UTIL_PlayerByIndex(1) );

        if ( pPlayer )
        {
            if ( pPlayer->GetPlayerInputDevice() == INPUT_DEVICE_GAMEPAD )
            {
                return true;
            }
        }

        return false;
    }

    void CCSGameRules::TrainingGivePlayerAmmo( void )
    {
        CBasePlayer *pPlayer = UTIL_PlayerByIndex(1);

        if ( pPlayer )
        {
            CBaseCombatWeapon *pWeapon = pPlayer->GetActiveWeapon();

            if( pWeapon && pWeapon->UsesPrimaryAmmo() )
            {
				pWeapon->SetReserveAmmoCount( AMMO_POSITION_PRIMARY, pWeapon->GetReserveAmmoMax( AMMO_POSITION_PRIMARY ) );
            }
        }
    }

    void CCSGameRules::TrainingSetRadarHidden( bool bHide )
    {
        CBasePlayer *pPlayer = UTIL_PlayerByIndex(1);

        if ( pPlayer )
        {
            CCSPlayer *pCSPlayer = static_cast<CCSPlayer *>( pPlayer );
            if ( pCSPlayer )
                pCSPlayer->SetRadarHidden( bHide );
        }
    }

    void CCSGameRules::TrainingSetMiniScoreHidden(  bool bHide )
    {
        CBasePlayer *pPlayer = UTIL_PlayerByIndex(1);

        if ( pPlayer )
        {
            CCSPlayer *pCSPlayer = static_cast<CCSPlayer *>( pPlayer );
            if ( pCSPlayer )
                pCSPlayer->SetMiniScoreHidden( bHide );
        }
    }

    // not used yet because this relied on vgui panels, scaleform isn't supported yet
    void CCSGameRules::TrainingHighlightAmmoCounter( void )
    {
        CBasePlayer *pPlayer = UTIL_PlayerByIndex(1);

        if ( pPlayer )
        {

#ifndef CLIENT_DLL
            IGameEvent * event = gameeventmanager->CreateEvent( "tr_highlight_ammo", true );
            if ( event )
            {
                event->SetInt( "userid", pPlayer->GetUserID() );
                gameeventmanager->FireEvent( event );
            }
#endif
        }
    }

    void CCSGameRules::TrainingShowFinishMsgBox( void )
    {
        CBasePlayer *pPlayer = UTIL_PlayerByIndex(1);

        if ( pPlayer )
        {
            IGameEvent * event = gameeventmanager->CreateEvent( "tr_show_finish_msgbox", true );
            if ( event )
            {
                gameeventmanager->FireEvent( event );
            }
        }
    }

    void CCSGameRules::TrainingShowExitDoorMsg( void )
    {
        CBasePlayer *pPlayer = UTIL_PlayerByIndex(1);

        if ( pPlayer )
        {
            IGameEvent * event = gameeventmanager->CreateEvent( "tr_show_exit_msgbox", true );
            if ( event )
            {
                gameeventmanager->FireEvent( event );
            }
        }
    }

	void ScriptCoopResetRoundStartTime( void )
	{
		CCSGameRules *pRules = CSGameRules();
		if ( !pRules )
			return;

		pRules->CoopResetRoundStartTime();
	}

	void ScriptCoopGiveC4sToCTs( int nNumC4ToGive )
	{
		CCSGameRules *pRules = CSGameRules();
		if ( !pRules )
			return;

		pRules->CoopGiveC4sToCTs( nNumC4ToGive );
	}

    void ScriptSetPlayerCompletedTraining( bool bCompleted )
    {
        CCSGameRules *pRules = CSGameRules();
        if ( !pRules )
            return;

        pRules->SetPlayerCompletedTraining( bCompleted );
    }

    bool ScriptGetPlayerCompletedTraining( void )
    {
        CCSGameRules *pRules = CSGameRules();
        if ( !pRules )
            return false;

        return pRules->GetPlayerCompletedTraining();
    }

    void ScriptSetBestTrainingCourseTime( int nTime )
    {
        CCSGameRules *pRules = CSGameRules();
        if ( !pRules )
            return;

        pRules->SetBestTrainingCourseTime( nTime );
    }

    int ScriptGetBestTrainingCourseTime( void )
    {
        CCSGameRules *pRules = CSGameRules();
        if ( !pRules )
            return false;

        return pRules->GetBestTrainingCourseTime();
    }

    int ScriptGetValveTrainingCourseTime( void )
    {
        CCSGameRules *pRules = CSGameRules();
        if ( !pRules )
            return false;

        return pRules->GetValveTrainingCourseTime();
    }

    void ScriptTrainingGivePlayerAmmo( void )
    {
        CCSGameRules *pRules = CSGameRules();
        if ( !pRules || !pRules->IsPlayingTraining() )
            return;

        pRules->TrainingGivePlayerAmmo();
    }

    void ScriptSetMiniScoreHidden( bool nHide )
    {
        CCSGameRules *pRules = CSGameRules();
        if ( !pRules || !pRules->IsPlayingTraining() )
            return;

        pRules->TrainingSetMiniScoreHidden( nHide );
    }

    void ScriptSetRadarHidden( bool nHide )
    {
        CCSGameRules *pRules = CSGameRules();
        if ( !pRules || !pRules->IsPlayingTraining() )
            return;

        pRules->TrainingSetRadarHidden( nHide );
    }

    void ScriptHighlightAmmoCounter( void )
    {
        CCSGameRules *pRules = CSGameRules();
        if ( !pRules || !pRules->IsPlayingTraining() )
            return;

        pRules->TrainingHighlightAmmoCounter();
    }

    void ScriptShowFinishMsgBox( void )
    {
        CCSGameRules *pRules = CSGameRules();
        if ( !pRules || !pRules->IsPlayingTraining() )
            return;

        pRules->TrainingShowFinishMsgBox();
    }

    void ScriptShowExitDoorMsg( void )
    {
        CCSGameRules *pRules = CSGameRules();
        if ( !pRules || !pRules->IsPlayingTraining() )
            return;

        pRules->TrainingShowExitDoorMsg();
    }

    bool ScriptIsLocalPlayerUsingController( void )
    {
        CCSGameRules *pRules = CSGameRules();
        if ( !pRules )
            return false;

        return pRules->IsLocalPlayerUsingController();
    }

	void ScriptPrintMessageCenterAll( const char *pszMessage )
	{
		UTIL_ClientPrintAll( HUD_PRINTCENTER, pszMessage );
	}

	void ScriptPrintMessageCenterAllWithParams( const char *pszMessage, const char *param1 = NULL, const char *param2 = NULL, const char *param3 = NULL )
	{
		UTIL_ClientPrintAll( HUD_PRINTCENTER, pszMessage, param1, param2, param3 );
	}

	void ScriptPrintMessageChatAll( const char *pszMessage )
	{
		UTIL_ClientPrintAll( HUD_PRINTTALK, pszMessage );
	}

	void ScriptPrintMessageCenterTeam( int nTeamNumber, const char *pszMessage )
	{
		CRecipientFilter filter;
		filter.MakeReliable();
		filter.AddRecipientsByTeam( GetGlobalTeam(nTeamNumber) );
		UTIL_ClientPrintFilter( filter, HUD_PRINTCENTER, pszMessage );
	}

	void ScriptPrintMessageChatTeam( int nTeamNumber, const char *pszMessage )
	{
		CRecipientFilter filter;
		filter.MakeReliable();
		filter.AddRecipientsByTeam( GetGlobalTeam(nTeamNumber) );
		UTIL_ClientPrintFilter( filter, HUD_PRINTTALK, pszMessage );
	}

	int ScriptGetGameMode( void )
	{
		return g_pGameTypes->GetCurrentGameMode();
	}

	int ScriptGetGameType( void )
	{
		return g_pGameTypes->GetCurrentGameType();
	}

	
	int ScriptGetRoundsPlayed( void )
	{
		CCSGameRules *pRules = CSGameRules();
		if ( !pRules )
			return false;

		return pRules->GetRoundsPlayed();
	}

	bool ScriptIsWarmupPeriod( void )
	{
		CCSGameRules *pRules = CSGameRules();
		if ( !pRules )
			return false;

		return pRules->IsWarmupPeriod();
	}

	void ScriptCoopSetBotQuotaAndRefreshSpawns( int nMaxEnemiesToSpawn )
	{
		CCSGameRules *pRules = CSGameRules();
		if ( !pRules )
			return;

		return pRules->CoopSetBotQuotaAndRefreshSpawns( nMaxEnemiesToSpawn );
	}

	void ScriptCoopMissionSpawnFirstEnemies( int nMaxEnemiesToSpawn )
	{
		CCSGameRules *pRules = CSGameRules();
		if ( !pRules )
			return;

		return pRules->CoopMissionSpawnFirstEnemies( nMaxEnemiesToSpawn );
	}

	void ScriptCoopMissionSetNextRespawnIn( float flSeconds, bool bIncrementWaveNumber )
	{
		CCSGameRules *pRules = CSGameRules();
		if ( !pRules )
			return;

		return pRules->CoopMissionSetNextRespawnIn( flSeconds, bIncrementWaveNumber );
	}

	void ScriptCoopMissionSpawnNextWave( int nMaxEnemiesToSpawn )
	{
		CCSGameRules *pRules = CSGameRules();
		if ( !pRules )
			return;

		return pRules->CoopMissionSpawnNextWave( nMaxEnemiesToSpawn );
	}

	void ScriptCoopMissionRespawnDeadPlayers( void )
	{
		CCSGameRules *pRules = CSGameRules();
		if ( !pRules )
			return;

		return pRules->CoopMissionRespawnDeadPlayers();
	}

	int ScriptCoopMissionGetMissionNumber( void )
	{
		return mp_coopmission_mission_number.GetInt();
	}

	void ScriptCoopCollectBonusCoin( void )
	{
		CCSGameRules *pRules = CSGameRules();
		if ( !pRules )
			return;

		return pRules->CoopCollectBonusCoin();
	}

    void CCSGameRules::RegisterScriptFunctions( void )
    {
        ScriptRegisterFunction( g_pScriptVM, ScriptSetPlayerCompletedTraining, "Sets whether the player has completed the initial portion of the training map." );
        ScriptRegisterFunction( g_pScriptVM, ScriptGetPlayerCompletedTraining, "Returns true if the player has completed the initial portion of the training map." );
        ScriptRegisterFunction( g_pScriptVM, ScriptSetBestTrainingCourseTime, "Sets the player's best time for completing the timed course." );
        ScriptRegisterFunction( g_pScriptVM, ScriptGetBestTrainingCourseTime, "Gets the player's best time for completing the timed course." );
        ScriptRegisterFunction( g_pScriptVM, ScriptGetValveTrainingCourseTime, "Gets Valve's best time for completing the timed course." );
        ScriptRegisterFunction( g_pScriptVM, ScriptTrainingGivePlayerAmmo, "Refills ammo to max for all weapons the player has (only works in training)." );
        ScriptRegisterFunction( g_pScriptVM, ScriptSetMiniScoreHidden, "Toggles the visibility of the miniscoreboard hud element." );
        ScriptRegisterFunction( g_pScriptVM, ScriptSetRadarHidden, "Toggles the visibility of the radar hud element." );
        ScriptRegisterFunction( g_pScriptVM, ScriptHighlightAmmoCounter, "Sends an event that is just used by the instructor system to show a hint highlighting the ammo counter." );
        ScriptRegisterFunction( g_pScriptVM, ScriptShowFinishMsgBox, "Shows a message box to let players know what to do next after finishing the training course." );
        ScriptRegisterFunction( g_pScriptVM, ScriptShowExitDoorMsg, "Shows a message box in trainign when the player exits through the exit door" );
        ScriptRegisterFunction( g_pScriptVM, ScriptIsLocalPlayerUsingController, "Returns whether the player is playing with a controller or not." );	

		ScriptRegisterFunction( g_pScriptVM, ScriptPrintMessageCenterAll, "Prints an alert message in the center print method to all players." );
		ScriptRegisterFunction( g_pScriptVM, ScriptPrintMessageCenterAllWithParams, "Prints an alert message in the center print method to all players. Needs to pass token/message. param1, param2, param3. Can pass null if you need less than 3." );
		ScriptRegisterFunction( g_pScriptVM, ScriptPrintMessageChatAll, "Prints a message in chat to all players." );	
		ScriptRegisterFunction( g_pScriptVM, ScriptPrintMessageCenterTeam, "Prints an alert message in the center print method to the specified team." );	
		ScriptRegisterFunction( g_pScriptVM, ScriptPrintMessageChatTeam, "Prints a message in chat to the specified team." );	

		ScriptRegisterFunction( g_pScriptVM, ScriptGetGameMode, "Gets the current game mode." );
		ScriptRegisterFunction( g_pScriptVM, ScriptGetGameType, "Gets the current game type." );
		ScriptRegisterFunction( g_pScriptVM, ScriptGetRoundsPlayed, "Get the number of rounds played so far." );	
		ScriptRegisterFunction( g_pScriptVM, ScriptIsWarmupPeriod, "Is it warmup or not." );
			
		ScriptRegisterFunction( g_pScriptVM, ScriptCoopSetBotQuotaAndRefreshSpawns, "Sets the bot quota considering the # of players connected and refreshes the spawns." );
		ScriptRegisterFunction( g_pScriptVM, ScriptCoopMissionSpawnFirstEnemies, "Spawns the first wave of enemies in coop." );
		ScriptRegisterFunction( g_pScriptVM, ScriptCoopMissionSetNextRespawnIn, "Set the next respawn wave to happen in this many seconds." );	
		ScriptRegisterFunction( g_pScriptVM, ScriptCoopMissionSpawnNextWave, "Tells the next wave of enemies to spawn in coop.  Also respawns player." );	
		ScriptRegisterFunction( g_pScriptVM, ScriptCoopMissionRespawnDeadPlayers, "Respawns players only." );	
		ScriptRegisterFunction( g_pScriptVM, ScriptCoopMissionGetMissionNumber, "Gets the mission number for the current map - maps can have multiple missions on them." );
		ScriptRegisterFunction( g_pScriptVM, ScriptCoopResetRoundStartTime, "Resets the round time and starts the mission." );
		ScriptRegisterFunction( g_pScriptVM, ScriptCoopGiveC4sToCTs, "Will give the number of specified C4s to all alive CT players." );
		ScriptRegisterFunction( g_pScriptVM, ScriptCoopCollectBonusCoin, "Marks one of the bonus coins as collected." );
		
	}

    //-----------------------------------------------------------------------------
    // Purpose: TF2 Specific Client Commands
    // Input  :
    // Output :
    //-----------------------------------------------------------------------------
    bool CCSGameRules::ClientCommand( CBaseEntity *pEdict, const CCommand &args )
    {
        CCSPlayer *pPlayer = ToCSPlayer( pEdict );

        if ( FStrEq( args[0], "changeteam" ) )
        {
            return true;
        }
        else if ( FStrEq( args[0], "nextmap" ) )
        {
            // catch corrupted command
            if ( pPlayer == NULL )
                return true;

			extern ConVar nextmap_print_enabled;
			if ( !nextmap_print_enabled.GetBool() )
				return true;

            if ( pPlayer->m_iNextTimeCheck < gpGlobals->curtime )
            {
                char szNextMap[MAX_PATH];
                if ( nextlevel.GetString() && *nextlevel.GetString() && engine->IsMapValid( nextlevel.GetString() ) )
                {
                    Q_strncpy( szNextMap, nextlevel.GetString(), sizeof( szNextMap ) );
                }
                else
                {
                    GetNextLevelName( szNextMap, sizeof( szNextMap ) );
                }

                if ( IsGameConsole() || engine->IsDedicatedServerForXbox() || engine->IsDedicatedServerForPS3() )
                {
                    // we know all the map names on console so we can safely assume we have a token for all maps
                    const int nMaxLength = MAX_MAP_NAME+10+1;
                    char szToken[nMaxLength];
                    CreateFriendlyMapNameToken(szNextMap, szToken, nMaxLength);
                    ClientPrint( pPlayer, HUD_PRINTTALK, "#game_nextmap", szToken );
                }
                else
                {
                    ClientPrint( pPlayer, HUD_PRINTTALK, "#game_nextmap", V_GetFileName( szNextMap ) );
                }
        
                pPlayer->m_iNextTimeCheck = gpGlobals->curtime + 1;
            }
            return true;
        }
        else if ( FStrEq( args[0], "tr_map_show_exit_door_msg" ) )
        {
            IGameEvent * event = gameeventmanager->CreateEvent( "tr_exit_hint_trigger" );
            if ( event )
            {
                gameeventmanager->FireEvent( event );
            }
            //engine->ServerCommand( "ent_fire @tr_exit_hint trigger\n" );
            return true;
        }
        else if( pPlayer != NULL && pPlayer->ClientCommand( args ) )
        {
            return true;
        }
        else if( BaseClass::ClientCommand( pEdict, args ) )
        {
            return true;
        }
        else if ( TheBots->ServerCommand( args.GetCommandString() ) )
        {
            return true;
        }
        else
        {
            return TheBots->ClientCommand( pPlayer, args );
        }
    }


    //-----------------------------------------------------------------------------
    // Purpose: Player has just spawned. Equip them.
    //-----------------------------------------------------------------------------
    void CCSGameRules::PlayerSpawn( CBasePlayer *pBasePlayer )
    {
        CCSPlayer *pPlayer = ToCSPlayer( pBasePlayer );
        if ( !pPlayer )
            Error( "PlayerSpawn" );

        if ( pPlayer->State_Get() != STATE_ACTIVE )
            return;

		// If the round number is a multiple of the requested number of rounds, reset equipment/economy
		if ( mp_equipment_reset_rounds.GetInt() > 0 && ( m_totalRoundsPlayed % mp_equipment_reset_rounds.GetInt() ) == 0 )
		{
			pPlayer->RemoveAllItems( true );
		}

		if ( mp_economy_reset_rounds.GetInt() > 0 && ( m_totalRoundsPlayed % mp_economy_reset_rounds.GetInt() ) == 0 )
		{
			pPlayer->ResetAccount();
			pPlayer->AddAccount( mp_startmoney.GetInt() );
		}

		if ( CSGameRules()->IsPlayingCoopGuardian()/* || CSGameRules()->IsPlayingCoopMission()*/ )
			CSGameRules()->GuardianUpdateBotAccountAndWeapons( pPlayer );

        pPlayer->EquipSuit();

        // remove any defusers left over from previous random if there is just one random one
        if ( mp_defuser_allocation.GetInt() == DefuserAllocation::Random )
            pPlayer->RemoveDefuser();

        bool addDefault = (pPlayer->GetTeamNumber() > TEAM_SPECTATOR);

        CBaseEntity	*pWeaponEntity = NULL;
        while ( ( pWeaponEntity = gEntList.FindEntityByClassname( pWeaponEntity, "game_player_equip" )) != NULL )
        {
			CGamePlayerEquip *pEquip = dynamic_cast<CGamePlayerEquip*>( pWeaponEntity );
			if ( pEquip && !pEquip->UseOnly() )
			{
				if ( addDefault && pEquip->StripFirst() )
				{
					// remove all our weapons and armor before touching the first game_player_equip
					pPlayer->RemoveAllItems( true );
				}
				pWeaponEntity->Touch( pPlayer );
				addDefault = false;
			}
        }

        if ( addDefault )
            pPlayer->GiveDefaultItems();
		if ( !IsWarmupPeriod() && IsPlayingCooperativeGametype() && !IsQueuedMatchmaking() )
		{
			switch ( pBasePlayer->GetTeamNumber() )
			{
			case TEAM_CT:
			case TEAM_TERRORIST:
				// Ensure that all QMM structures are always initialized for players in coop game type
				( void ) QueuedMatchmakingPlayersDataFindOrCreate( pPlayer );
				break;
			}
		}
    }

    void CCSGameRules::BroadcastSound( const char *sound, int team )
    {
        CBroadcastRecipientFilter filter;
        filter.MakeReliable();

        if( team != -1 )
        {
            filter.RemoveAllRecipients();
            filter.AddRecipientsByTeam( GetGlobalTeam(team) );
        }

		CCSUsrMsg_SendAudio msg;
		msg.set_radio_sound( sound ) ;
        SendUserMessage( filter, CS_UM_SendAudio, msg );
    }


    //-----------------------------------------------------------------------------
    // Purpose: Player has just spawned. Equip them.
    //-----------------------------------------------------------------------------

    // return a multiplier that should adjust the damage done by a blast at position vecSrc to something at the position
    // vecEnd.  This will take into account the density of an entity that blocks the line of sight from one position to
    // the other.
    //
    // this algorithm was taken from the HL2 version of RadiusDamage.
    float CCSGameRules::GetExplosionDamageAdjustment(Vector & vecSrc, Vector & vecEnd, CBaseEntity *pEntityToIgnore)
    {
        float retval = 0.0;
        trace_t tr;

        UTIL_TraceLine(vecSrc, vecEnd, MASK_SHOT, pEntityToIgnore, COLLISION_GROUP_NONE, &tr);
        if (tr.fraction == 1.0)
        {
            retval = 1.0;
        }
        else if (!(tr.DidHitWorld()) && (tr.m_pEnt != NULL) && (tr.m_pEnt != pEntityToIgnore) && (tr.m_pEnt->GetOwnerEntity() != pEntityToIgnore))
        {
            // if we didn't hit world geometry perhaps there's still damage to be done here.

            CBaseEntity *blockingEntity = tr.m_pEnt;

            // check to see if this part of the player is visible if entities are ignored.
            UTIL_TraceLine(vecSrc, vecEnd, CONTENTS_SOLID, NULL, COLLISION_GROUP_NONE, &tr);

            if (tr.fraction == 1.0)
            {
                if ((blockingEntity != NULL) && (blockingEntity->VPhysicsGetObject() != NULL))
                {
                    int nMaterialIndex = blockingEntity->VPhysicsGetObject()->GetMaterialIndex();

                    float flDensity;
                    float flThickness;
                    float flFriction;
                    float flElasticity;

                    physprops->GetPhysicsProperties( nMaterialIndex, &flDensity,
                        &flThickness, &flFriction, &flElasticity );

                    const float DENSITY_ABSORB_ALL_DAMAGE = 3000.0;
                    float scale = flDensity / DENSITY_ABSORB_ALL_DAMAGE;
                    if ((scale >= 0.0) && (scale < 1.0))
                    {
                        retval = 1.0 - scale;
                    }
                    else if (scale < 0.0)
                    {
                        // should never happen, but just in case.
                        retval = 1.0;
                    }
                }
                else
                {
                    retval = 0.75; // we're blocked by something that isn't an entity with a physics module or world geometry, just cut damage in half for now.
                }
            }
        }

        return retval;
    }

    // returns the percentage of the player that is visible from the given point in the world.
    // return value is between 0 and 1.
    float CCSGameRules::GetAmountOfEntityVisible(Vector & vecSrc, CBaseEntity *entity)
    {
        float retval = 0.0;

        const float damagePercentageChest = 0.40;
        const float damagePercentageHead = 0.20;
        const float damagePercentageFeet = 0.20;
        const float damagePercentageRightSide = 0.10;
        const float damagePercentageLeftSide = 0.10;

        if (!(entity->IsPlayer()))
        {
            // the entity is not a player, so the damage is all or nothing.
            Vector vecTarget;
            vecTarget = entity->BodyTarget(vecSrc, false);

            return GetExplosionDamageAdjustment(vecSrc, vecTarget, entity);
        }

        CCSPlayer *player = (CCSPlayer *)entity;

        // check what parts of the player we can see from this point and modify the return value accordingly.
        float chestHeightFromFeet;

        float armDistanceFromChest = HalfHumanWidth;

        // calculate positions of various points on the target player's body
        Vector vecFeet = player->GetAbsOrigin();

        Vector vecChest = player->BodyTarget(vecSrc, false);
        chestHeightFromFeet = vecChest.z - vecFeet.z;  // compute the distance from the chest to the feet. (this accounts for ducking and the like)

        Vector vecHead = player->GetAbsOrigin();
        vecHead.z += HumanHeight;

        Vector vecRightFacing;
        AngleVectors(player->GetAbsAngles(), NULL, &vecRightFacing, NULL);

        vecRightFacing.NormalizeInPlace();
        vecRightFacing = vecRightFacing * armDistanceFromChest;

        Vector vecLeftSide = player->GetAbsOrigin();
        vecLeftSide.x -= vecRightFacing.x;
        vecLeftSide.y -= vecRightFacing.y;
        vecLeftSide.z += chestHeightFromFeet;

        Vector vecRightSide = player->GetAbsOrigin();
        vecRightSide.x += vecRightFacing.x;
        vecRightSide.y += vecRightFacing.y;
        vecRightSide.z += chestHeightFromFeet;

        // check chest
        float damageAdjustment = GetExplosionDamageAdjustment(vecSrc, vecChest, entity);
        retval += (damagePercentageChest * damageAdjustment);

        // check top of head
        damageAdjustment = GetExplosionDamageAdjustment(vecSrc, vecHead, entity);
        retval += (damagePercentageHead * damageAdjustment);

        // check feet
        damageAdjustment = GetExplosionDamageAdjustment(vecSrc, vecFeet, entity);
        retval += (damagePercentageFeet * damageAdjustment);

        // check left "edge"
        damageAdjustment = GetExplosionDamageAdjustment(vecSrc, vecLeftSide, entity);
        retval += (damagePercentageLeftSide * damageAdjustment);

        // check right "edge"
        damageAdjustment = GetExplosionDamageAdjustment(vecSrc, vecRightSide, entity);
        retval += (damagePercentageRightSide * damageAdjustment);

        return retval;
    }

    void CCSGameRules::RadiusDamage( const CTakeDamageInfo &info, const Vector &vecSrcIn, float flRadius, int iClassIgnore, CBaseEntity * pEntityIgnore )
    {
        RadiusDamage( info, vecSrcIn, flRadius, iClassIgnore, false );
    }

    // Add the ability to ignore the world trace
    void CCSGameRules::RadiusDamage( const CTakeDamageInfo &info, const Vector &vecSrcIn, float flRadius, int iClassIgnore, bool bIgnoreWorld )
    {
        trace_t		tr;
        float		falloff, damagePercentage;
        Vector		vecSpot;
        Vector		vecToTarget;
        Vector		vecEndPos;

        // [tj] The number of enemy players this explosion killed
        int numberOfEnemyPlayersKilledByThisExplosion = 0;
        
        // [tj] who we award the achievement to if enough players are killed
        CCSPlayer* pCSExplosionAttacker = ToCSPlayer(info.GetAttacker());

        // [tj] used to determine which achievement to award for sufficient kills
        CBaseEntity* pInflictor = info.GetInflictor();
        bool isGrenade = dynamic_cast< CHEGrenadeProjectile* >( pInflictor ) != NULL;
        bool isBomb = pInflictor && V_strcmp(pInflictor->GetClassname(), "planted_c4") == 0;

        vecEndPos.Init();

        Vector vecSrc = vecSrcIn;

        damagePercentage = 1.0;

        if ( flRadius )
            falloff = info.GetDamage() / flRadius;
        else
            falloff = 1.0;

        //int bInWater = (UTIL_PointContents ( vecSrc, MASK_WATER ) & MASK_WATER) ? true : false;
        
        vecSrc.z += 1;// in case grenade is lying on the ground

		struct CEntityDistance {
			CBaseEntity* pEntity;
			float fDistanceSqr;
		};

		CUtlVectorFixed< CEntityDistance, MAX_SPHERE_QUERY > vecTargetedObjects;

        // iterate on all entities in the vicinity.
		CBaseEntity* pSphereQueryEntity = nullptr;
		for ( CEntitySphereQuery sphere( vecSrc, flRadius ); ( pSphereQueryEntity = sphere.GetCurrentEntity() ) != NULL; sphere.NextEntity() )
		{
			float fDistanceSquared = pSphereQueryEntity->GetAbsOrigin().DistToSqr( vecSrc );
			vecTargetedObjects.AddToTail( CEntityDistance{ pSphereQueryEntity, fDistanceSquared } );
		}

		// We do damage in order of distance from the explosion, so that players closer to a blast die first.
		// This can affect who wins a round and ranking in survival mode.
		vecTargetedObjects.Sort( []( const CEntityDistance* lhs, const CEntityDistance* rhs ) {
			float diff = lhs->fDistanceSqr - rhs->fDistanceSqr;
			if ( diff < 0.0f )
				return -1;
			else if ( diff > 0.0f )
				return 1;
			else
				return 0;
		} );

		FOR_EACH_VEC( vecTargetedObjects, iTargetedObject )
		{
			CBaseEntity* pEntity = vecTargetedObjects[iTargetedObject].pEntity;

            // [tj] We have to save whether or not the player is killed so we don't give credit 
            //		for pre-dead players.
            bool wasAliveBeforeExplosion = false;
            CCSPlayer* pCSExplosionVictim = ToCSPlayer(pEntity);
            if (pCSExplosionVictim)
            {
                wasAliveBeforeExplosion = pCSExplosionVictim->IsAlive();
            }

			bool bIgnoreDamageToThisEntity = ( pEntity->m_takedamage == DAMAGE_NO ) ||
				( pCSExplosionVictim && !wasAliveBeforeExplosion );

            if ( !bIgnoreDamageToThisEntity )
            {
                // UNDONE: this should check a damage mask, not an ignore
                if ( iClassIgnore != CLASS_NONE && pEntity->Classify() == iClassIgnore )
                {// houndeyes don't hurt other houndeyes with their attack
                    continue;
                }

                //// blasts used to not travel into or out of water, users assumed it was a bug. Fix is not to run this check -wills
                //if ( !bIgnoreWorld )
                //{
                //    if (bInWater && pEntity->GetWaterLevel() == WL_NotInWater)
                //        continue;
                //    if (!bInWater && pEntity->GetWaterLevel() == WL_Eyes)
                //        continue;
                //}

                // radius damage can only be blocked by the world
                vecSpot = pEntity->BodyTarget( vecSrc );

                bool bHit = false;

                if( bIgnoreWorld )
                {
                    vecEndPos = vecSpot;
                    bHit = true;
                }
                else
                {
                    // get the percentage of the target entity that is visible from the
                    // explosion position.
                    damagePercentage = GetAmountOfEntityVisible(vecSrc, pEntity);
                    if (damagePercentage > 0.0)
                    {
                        vecEndPos = vecSpot;

                        bHit = true;
                    }
                }

                if ( bHit )
                {
                    // the explosion can 'see' this entity, so hurt them!
                    //vecToTarget = ( vecSrc - vecEndPos );
                    vecToTarget = ( vecEndPos - vecSrc );

                    // use a Gaussian function to describe the damage falloff over distance, with flRadius equal to 3 * sigma
                    // this results in the following values:
                    // 
                    // Range Fraction  Damage
                    //		0.0			100%
                    // 		0.1			96%
                    // 		0.2			84%
                    // 		0.3			67%
                    // 		0.4			49%
                    // 		0.5			32%
                    // 		0.6			20%
                    // 		0.7			11%
                    // 		0.8			 6%
                    // 		0.9			 3%
                    // 		1.0			 1%

                    float fDist = vecToTarget.Length();
                    float fSigma = flRadius / 3.0f; // flRadius specifies 3rd standard deviation (0.0111 damage at this range)
                    float fGaussianFalloff = exp(-fDist * fDist / (2.0f * fSigma * fSigma));
                    float flAdjustedDamage = info.GetDamage() * fGaussianFalloff * damagePercentage;
                
                    if ( flAdjustedDamage > 0 )
                    {
                        CTakeDamageInfo adjustedInfo = info;
                        adjustedInfo.SetRadius( flRadius );
                        adjustedInfo.SetDamage( flAdjustedDamage );

                        Vector dir = vecToTarget;
                        VectorNormalize( dir );

                        // If we don't have a damage force, manufacture one
                        if ( adjustedInfo.GetDamagePosition() == vec3_origin || adjustedInfo.GetDamageForce() == vec3_origin )
                        {
                            CalculateExplosiveDamageForce( &adjustedInfo, dir, vecSrc, 1.5	/* explosion scale! */ );
                        }
                        else
                        {
                            // Assume the force passed in is the maximum force. Decay it based on falloff.
                            float flForce = adjustedInfo.GetDamageForce().Length() * falloff;
                            adjustedInfo.SetDamageForce( dir * flForce );
                            adjustedInfo.SetDamagePosition( vecSrc );
                        }

						// this entity may scale its response to damage force within a certain time window
						if ( pEntity->m_flDamageForceOverrideStartTime > gpGlobals->curtime )
						{
							float flMultiplier = RemapValClamped( gpGlobals->curtime,
								pEntity->m_flDamageForceOverrideStartTime,
								pEntity->m_flDamageForceOverrideEndTime,
								pEntity->m_flDamageForceOverrideMultiplier,
								1.0f );

							if ( flMultiplier != 1.0f )
								adjustedInfo.SetDamageForce( adjustedInfo.GetDamageForce() * flMultiplier );
						}

                        Vector vecTarget;
                        vecTarget = pEntity->BodyTarget(vecSrc, false);

                        UTIL_TraceLine(vecSrc, vecTarget, MASK_SHOT, NULL, COLLISION_GROUP_NONE, &tr);

                        // blasts always hit chest
                        tr.hitgroup = HITGROUP_GENERIC;

                        if (tr.fraction != 1.0)
                        {
                            // this has to be done to make breakable glass work.
                            ClearMultiDamage( );
                            pEntity->DispatchTraceAttack( adjustedInfo, dir, &tr );
                            ApplyMultiDamage();
                        }
                        else
                        {
                            pEntity->TakeDamage( adjustedInfo );
                        }
            
                        // Now hit all triggers along the way that respond to damage... 
                        pEntity->TraceAttackToTriggers( adjustedInfo, vecSrc, vecEndPos, dir );

                        // [sbodenbender] Increment grenade damage stat
                        if (pCSExplosionVictim && 
                            pCSExplosionAttacker && 
                            isGrenade && 
                            pCSExplosionVictim->GetTeamNumber() != pCSExplosionAttacker->GetTeamNumber() )
                        {
                            CCS_GameStats.IncrementStat(pCSExplosionAttacker, CSSTAT_GRENADE_DAMAGE, static_cast<int>(adjustedInfo.GetDamage()));
                        }
                    }
                }
            }

            // [tj] Count up victims of area of effect damage for achievement purposes
            if ( pCSExplosionVictim )
            {
                //If the bomb is exploding, set the attacker to the planter (we can't put this in the CTakeDamageInfo, since
                //players aren't supposed to get credit for bomb kills)
                if ( isBomb )
                {
                    CPlantedC4* bomb = static_cast<CPlantedC4*>( pInflictor );
                    if ( bomb )
                    {
                        pCSExplosionAttacker = bomb->GetPlanter();
                    }
                }

                //Count check to make sure we killed an enemy player
                if(	pCSExplosionAttacker &&
                    !pCSExplosionVictim->IsAlive() && 
                    wasAliveBeforeExplosion &&
                    pCSExplosionVictim->GetTeamNumber() != pCSExplosionAttacker->GetTeamNumber())
                {
                    numberOfEnemyPlayersKilledByThisExplosion++;
                }
            }
        }

        // [tj] Depending on which type of explosion it was, award the appropriate achievement.
        if ( pCSExplosionAttacker && isGrenade && numberOfEnemyPlayersKilledByThisExplosion >= AchievementConsts::GrenadeMultiKill_MinKills )
        {
            pCSExplosionAttacker->AwardAchievement( CSGrenadeMultikill );
            pCSExplosionAttacker->CheckMaxGrenadeKills( numberOfEnemyPlayersKilledByThisExplosion );
        }
        if ( pCSExplosionAttacker && isBomb && numberOfEnemyPlayersKilledByThisExplosion >= AchievementConsts::BombMultiKill_MinKills )
        {
            pCSExplosionAttacker->AwardAchievement( CSBombMultikill );
        }
    }

    CCSPlayer* CCSGameRules::CheckAndAwardAssists( CCSPlayer* pCSVictim, CCSPlayer* pKiller )
    {  
        CUtlLinkedList< CDamageRecord *, int >& victimDamageTakenList = pCSVictim->GetDamageList();
        float maxDamage = 0.0f;
        CCSPlayer* maxDamagePlayer = NULL;

        FOR_EACH_LL( victimDamageTakenList, ii )
        {
			if ( victimDamageTakenList[ii]->GetPlayerRecipientPtr() == pCSVictim )
			{
				CCSPlayer* pAttackerPlayer = victimDamageTakenList[ii]->GetPlayerDamagerPtr();
				if ( pAttackerPlayer )
				{
					if ( (victimDamageTakenList[ii]->GetDamage() > maxDamage) && (pAttackerPlayer != pKiller) && ( pAttackerPlayer != pCSVictim ) )
					{
						maxDamage = victimDamageTakenList[ii]->GetDamage();
						maxDamagePlayer = pAttackerPlayer;
					}
				}
			}            
        }

        // note, only the highest damaging player can be awarded an assist
        if ( maxDamagePlayer && (maxDamage > cs_AssistDamageThreshold.GetFloat( )) )
        {
            ScorePlayerAssist( maxDamagePlayer, pCSVictim ); 
            return maxDamagePlayer;
        }

        return NULL;
    }

	//-----------------------------------------------------------------------------
	// Purpose: 
	// Input  : *pVictim - 
	//			*pKiller - 
	//			*pInflictor - 
	//-----------------------------------------------------------------------------
	IGameEvent * CCSGameRules::CreateWeaponKillGameEvent( char const *szEventName, const CTakeDamageInfo &info )
	{
		IGameEvent * event = gameeventmanager->CreateEvent( szEventName );
		if ( !event )
			return NULL;

		// Work out what killed the player, and prepare to send a message to all clients about it
		const char *killer_weapon_name = "world";		// by default, the player is killed by the world
		int killer_ID = 0;
		char killer_XUID[64] = { '\0' };
		char killer_weapon_itemid[64] = { '\0' };
		char killer_weapon_fauxitemid[64] = { '\0' };
		char kill_weapon_originalowner_xuid[64] = { '\0' };


		// Find the killer & the scorer
		CBaseEntity *pInflictor = info.GetInflictor();
		CBaseEntity *pKiller = info.GetAttacker();
		CBasePlayer *pScorer = GetDeathScorer( pKiller, pInflictor );

		bool bHeadshot = false;

		const CEconItemView* pInflictorView = nullptr;

		/* Removed for partner depot */
		
		// Is the killer a client?
		if ( pScorer )
		{
			killer_ID = pScorer->GetUserID();
			V_sprintf_safe( killer_XUID, "%llu", pScorer->GetSteamIDAsUInt64() );

			if( info.GetDamageType() & DMG_HEADSHOT )
			{
				//to enable drawing the headshot icon as well as the weapon icon, 
				bHeadshot = true;
			}

			if ( pInflictor == pScorer )
			{
				// If the inflictor is the killer,  then it must be their current weapon doing the damage
				if ( CBaseCombatWeapon* pInflictorWeapon = pScorer->GetActiveWeapon() )
				{
					killer_weapon_name = pInflictorWeapon->GetClassname();
					pInflictorView = pInflictorWeapon->GetEconItemView();
				}
			}
		}

		// strip the NPC_* or weapon_* from the inflictor's classname
		if ( IsWeaponClassname( killer_weapon_name ) )
		{
			killer_weapon_name += WEAPON_CLASSNAME_PREFIX_LENGTH;
		}
		else if ( StringHasPrefix( killer_weapon_name, "NPC_" ) )
		{
			killer_weapon_name += V_strlen( "NPC_" );
		}
		else if ( StringHasPrefixCaseSensitive( killer_weapon_name, "func_" ) )
		{
			killer_weapon_name += V_strlen( "func_" );
		}

		event->SetInt("attacker", killer_ID );
		event->SetString("weapon", killer_weapon_name );
		event->SetString("weapon_itemid", killer_weapon_itemid );
		event->SetString("weapon_fauxitemid", killer_weapon_fauxitemid );
		event->SetBool("headshot", bHeadshot );
		event->SetString("weapon_originalowner_xuid", kill_weapon_originalowner_xuid );

		event->SetInt( "penetrated", info.GetObjectsPenetrated() );

		return event;
	}

    //-----------------------------------------------------------------------------
    // Purpose: 
    // Input  : *pVictim - 
    //			*pKiller - 
    //			*pInflictor - 
    //-----------------------------------------------------------------------------
    void CCSGameRules::DeathNotice( CBasePlayer *pVictim, const CTakeDamageInfo &info )
    {
        // Find the killer & the scorer
        CBaseEntity *pInflictor = info.GetInflictor();
        CBaseEntity *pKiller = info.GetAttacker();
        CBasePlayer *pScorer = GetDeathScorer( pKiller, pInflictor );
        CCSPlayer *pCSVictim = (CCSPlayer*)(pVictim);
        CCSPlayer *pAssiter = CheckAndAwardAssists( pCSVictim, (CCSPlayer *)pKiller );

        if ( IGameEvent * event = CreateWeaponKillGameEvent( "player_death", info ) )
        {
            event->SetInt("userid", pVictim->GetUserID() );
            event->SetInt("assister", pAssiter ? pAssiter->GetUserID() : 0 );
			if( !OnReplayPrompt( pVictim, pScorer ) )
				event->SetBool( "noreplay", true ); // prevent UI display of "press to replay" message if HLTV data is unavailable

			bool bHeadshot = ( pScorer ) && ( info.GetDamageType() & DMG_HEADSHOT );
			int priority = bHeadshot ? 8 : 7;
#ifdef GAME_DLL
			CCSPlayer* player = ( dynamic_cast< CCSPlayer* >(pScorer) );

			if ( player )
			{
				priority += player->GetKillStreak();
			}
#endif
            event->SetInt( "priority", priority );	// player_death

			if ( pCSVictim->GetDeathFlags() & CS_DEATH_DOMINATION )
            {
                event->SetInt( "dominated", 1 );
            }
            else if ( pCSVictim->GetDeathFlags() & CS_DEATH_REVENGE )
            {
                event->SetInt( "revenge", 1 );
            }
            
            gameeventmanager->FireEvent( event );
        }

    }
  
	bool EconEntity_OnOwnerKillEaterEvent( CEconItemView *pEconItemView, CCSPlayer *pOwner, CCSPlayer *pVictim, kill_eater_event_t eEventType, int iAmount /*= 1*/, uint32 *pNewValue /* = NULL */ )
	{
		if ( pNewValue )
			*pNewValue = 0;

		// Kill-eater weapons.
		if ( !pEconItemView )
			return false;

		if ( !pOwner )
			return false;

		// Ignore events where we're affecting ourself.
		if ( pOwner == pVictim )
			return false;

		// Always require that we have at least the base kill eater attribute before sending any messages
		// to the GC.
		static CSchemaAttributeDefHandle pAttr_KillEater( "kill eater" );
		if ( !pEconItemView->FindAttribute( pAttr_KillEater ) )
			return false;

		// Don't bother sending a message to the GC if either party is a bot, unless we're tracking events against
		// bots specifically.
		CSteamID KillerSteamID, VictimSteamID;
		if ( !pOwner->GetSteamID( &KillerSteamID ) )
			return false;

		// comment this out to have StatTrak count bot kills
		if ( pVictim && !pVictim->GetSteamID( &VictimSteamID ) )
			return false;

		// we don't want to increment the killeater if the weapon owner is not the killer
		if ( pEconItemView->GetAccountID() != KillerSteamID.GetAccountID() )
			return false;

		// comment this out to have StatTrak count bot kills
 		if ( pVictim && pVictim->IsFakeClient() )
			return false;

		// Also require that we have whatever event type we're looking for, unless we're looking for regular
		// player kills in which case we may or may not have a field to describe that.
		
		AssertMsg( GetKillEaterAttrPairCount() == 1, "This function is now assuming there is only one stattrak value being updated when called. If we're adding multiple killeaters per item, this needs to be revisited." );
		const CEconItemAttributeDefinition *pScoreAttribDef = GetKillEaterAttrPair_Score(0);
		if ( !pScoreAttribDef )
			return false;

		// If we don't have this attribute, move on. It's possible to be missing this attribute but still
		// have the next one in the list if we have user-customized tracking types.
		uint32 unCurrent;
		if ( !FindAttribute_UnsafeBitwiseCast<attrib_value_t>( pEconItemView, pScoreAttribDef, &unCurrent ) )
			return false;

		const CEconItemAttributeDefinition *pScoreTypeAttribDef = GetKillEaterAttrPair_Type(0);
		if ( !pScoreTypeAttribDef )
			return false;

		unCurrent += iAmount;
		pEconItemView->UpdateNetworkedDynamicAttributesForDemos( pScoreAttribDef->GetDefinitionIndex(), *(float*)&unCurrent );

		if ( pNewValue ) 
			*pNewValue = unCurrent;

		return true;
	}

    //=========================================================
    //=========================================================



	void CCSGameRules::PlayerKilled( CBasePlayer *pVictim, const CTakeDamageInfo &info )
    {
        CBaseEntity *pInflictor = info.GetInflictor();
        CBaseEntity *pKiller = info.GetAttacker();
        CBasePlayer *pScorer = GetDeathScorer( pKiller, pInflictor );

		bool bKillerIsBomb = !V_strcmp( pKiller->GetClassname(), "planted_c4" );

		if ( IsPlayingSurvival() )
		{
			if ( bKillerIsBomb )
			{
				CPlantedC4* bomb = static_cast< CPlantedC4* >( pInflictor );
				if ( bomb )
				{
					pScorer = bomb->GetPlanter();
				}
			}

			if ( !CSGameRules()->IsWarmupPeriod() )
			{
				m_SurvivalRules.OnPlayerKilled( pVictim, pScorer );
			}
		}

        CCSPlayer *pCSVictim = (CCSPlayer *)pVictim;
        CCSPlayer *pCSScorer = (CCSPlayer *)pScorer;

		// determine whether this is a positive or negative kill
		int numPointsForPlayerKill = IPointsForKill( pScorer, pVictim );
		CWeaponCSBase* pWeapon = dynamic_cast<CWeaponCSBase *>( info.GetWeapon() );

        // [tj] Flag the round as non-lossless for the appropriate team.
        // [menglish] Set the death flags depending on a nemesis system
        if ( pVictim->GetTeamNumber() == TEAM_TERRORIST )
        {
            m_bNoTerroristsKilled = false;
            m_bNoTerroristsDamaged = false;
        }
        if ( pVictim->GetTeamNumber() == TEAM_CT )
        {
            m_bNoCTsKilled = false;
            m_bNoCTsDamaged = false;
        }

        m_bCanDonateWeapons = false;

        if ( m_pFirstKill == NULL && pCSScorer != pVictim )
        {
            m_pFirstKill = pCSScorer;
            m_firstKillTime = gpGlobals->curtime - m_fRoundStartTime;
        }

		// In survival death flags are set differently from contract kills
		// we will not reset them at this point
		// see: survival game rules -- pAttackersContractKillTargetPlayer->SetDeathFlags( CS_DEATH_DOMINATION );
		if ( !IsPlayingSurvival() )
		{
			// determine if this kill affected a nemesis relationship
			int iDeathFlags = 0;
			if ( pScorer )
			{
				CCS_GameStats.CalculateOverkill( pCSScorer, pCSVictim );
				CCS_GameStats.CalcDominationAndRevenge( pCSScorer, pCSVictim, &iDeathFlags );
			}
			pCSVictim->SetDeathFlags( iDeathFlags );
		}

        pCSVictim->SetLastKillerIndex( pCSScorer ? pCSScorer->entindex() : 0 );

        // If we're killed by the C4, we do a subset of BaseClass::PlayerKilled()
        // Specifically, we shouldn't lose any points or show death notices, to match goldsrc
        if ( bKillerIsBomb && !IsPlayingSurvival() )
        {
            // dvsents2: uncomment when removing all FireTargets
            // variant_t value;
            // g_EventQueue.AddEvent( "game_playerdie", "Use", value, 0, pVictim, pVictim );
            FireTargets( "game_playerdie", pVictim, pVictim, USE_TOGGLE, 0 );

			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" [%.0f %.0f %.0f] was killed by the bomb.\n",
				pCSVictim->GetPlayerName(),
				pCSVictim->GetUserID(),
				pCSVictim->GetNetworkIDString(),
				pCSVictim->GetTeam()->GetName(),
				pCSVictim->GetAbsOrigin().x,
				pCSVictim->GetAbsOrigin().y,
				pCSVictim->GetAbsOrigin().z );
        }
        else
        {
			if ( ( !pCSScorer && !dynamic_cast< CBaseGrenade * >( pInflictor ) && !dynamic_cast< CInferno * >( pInflictor ) ) ||	// special case for suicide tracking: exclude grenade exploding when thrower disconnected
				( pCSVictim == pCSScorer ) )
			{
				if ( UseSuicidePenalty() && 
					!pCSVictim->IsControllingBot() &&
					mp_autokick.GetBool() && 
					!CSGameRules()->IsPlayingOffline() &&
					!CSGameRules()->IsWarmupPeriod() &&
					( pVictim->GetPendingTeamNumber() == pVictim->GetTeamNumber() ) ) // don't punish suicides due to team change 
				{
					pVictim->m_nSuicides++;
						
					if ( pVictim->m_nSuicides >= 5 )
					{
						if ( sv_kick_ban_duration.GetInt() > 0 )
						{
							engine->ServerCommand( CFmtStr( "banid %d %d;", sv_kick_ban_duration.GetInt(), pVictim->GetUserID() ) );
						}

						engine->ServerCommand( UTIL_VarArgs( "kickid_ex %d %d For suiciding too many times\n", pVictim->GetUserID(), 1 ) );
					}
				}	
			}

            if( pCSVictim == pCSScorer )
			{
                CSGameRules()->ScorePlayerSuicide( pCSVictim );
			}
			else if ( numPointsForPlayerKill >= 0 )
			{
				// Track kills per weapon before increment frag count call
				if ( pWeapon && pCSScorer )
				{
					switch ( pWeapon->GetWeaponType() )
					{
					case WEAPONTYPE_PISTOL:
					{
						int numPistolKills, numSniperKills;
						pCSScorer->GetEnemyWeaponKills( numPistolKills, numSniperKills );
						pCSScorer->SetEnemyWeaponKills( numPistolKills + 1, numSniperKills );
					}
					break;
					case WEAPONTYPE_SNIPER_RIFLE:
					{
						int numPistolKills, numSniperKills;
						pCSScorer->GetEnemyWeaponKills( numPistolKills, numSniperKills );
						pCSScorer->SetEnemyWeaponKills( numPistolKills, numSniperKills + 1 );
					}
					break;
					}
				}
			}

            BaseClass::PlayerKilled( pVictim, info );
        }

        // check for team-killing, and give monetary rewards/penalties
        // Find the killer & the scorer
        if ( !pScorer )
        {
            pCSVictim->SetLastConcurrentKilled( 0 );
            return;
        }

        if ( numPointsForPlayerKill < 0 )
        {
            // team-killer!
            pCSScorer->AddAccountAward( PlayerCashAward::KILL_TEAMMATE );
            // m_DeferredCallQueue.QueueCall( pCSScorer, &CCSPlayer::AddAccountAward, PlayerCashAward::KILL_TEAMMATE );
            pCSScorer->IncrementTeamKillsCount( 1 );
            pCSScorer->m_bJustKilledTeammate = true;

            ClientPrint( pCSScorer, HUD_PRINTCENTER, "#SFUI_Notice_Killed_Teammate" );
            if ( mp_autokick.GetBool() )
            {
				if ( !IsPlayingOffline() )
				{
					char strTeamKills[64];
					Q_snprintf( strTeamKills, sizeof( strTeamKills ), "%d", (3 - pCSScorer->m_iTeamKills) );
					ClientPrint( pCSScorer, HUD_PRINTTALK, "#SFUI_Notice_Game_teammate_kills", strTeamKills ); // this includes a " of 3" in it
				}

                if ( pCSScorer->m_iTeamKills >= 3 )
                {
					if ( !IsPlayingOffline() )
                    {
                        ClientPrint( pCSScorer, HUD_PRINTTALK, "#SFUI_Notice_Banned_For_Killing_Teammates" );
						if ( sv_kick_ban_duration.GetInt() > 0 )
						{
							SendKickBanToGC( pCSScorer, k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_TKLimit );
							// don't roll the kick command into this, it will fail on a lan, where kickid will go through
							engine->ServerCommand( CFmtStr( "banid %d %d;", sv_kick_ban_duration.GetInt(), pCSScorer->GetUserID() ) );
						}
                    }

                    engine->ServerCommand( UTIL_VarArgs( "kickid_ex %d %d For killing too many teammates\n", pCSScorer->GetUserID(), IsPlayingOffline() ? 0 : 1 ) );
                }
                else if ( ( mp_spawnprotectiontime.GetInt() > 0 ) && ( GetRoundElapsedTime() < mp_spawnprotectiontime.GetInt() ) )
                {
					const CCSWeaponInfo* pWeaponInfo = CCSPlayer::GetWeaponInfoFromDamageInfo( info );

					// Special case here: TK at round start using some weapons* only counts towards team damage
					// and team kills amount, but doesn't instantly ban the player due to common occurrence
					// of teammates crossing in front of AWP'er on de_dust2
					// Weapons that will not instantly ban for TK at round start must have zoom levels and
					// team damage from headshot must kill with a single kill
					extern ConVar ff_damage_reduction_bullets;
					if ( !pWeaponInfo || !pWeapon ||
						( pWeapon->GetZoomLevels() < 2 ) ||	// weapon doesn't have 2 zoom levels, worth the ban
						( float( pWeapon->GetDamage() ) * 4.0f * ff_damage_reduction_bullets.GetFloat() <= 99.0f ) || // single friendly headshot will not kill, worth the ban
						!pCSVictim || ( pCSVictim->GetNumAttackersFromDamageList() > 1 ) || // victim already took damage from multiple players, TK should be banned
						( pCSVictim->GetMostNumHitsDamageRecordFrom(pCSScorer) > 1 ) // TK'er shot the victim multiple times, worth the ban
						)
					{
						if ( !IsPlayingOffline() )
						{
							ClientPrint( pCSScorer, HUD_PRINTTALK, "#SFUI_Notice_Banned_For_TK_Start" );
							if ( sv_kick_ban_duration.GetInt() > 0 )
							{
								SendKickBanToGC( pCSScorer, k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_TKSpawn );
								// don't roll the kick command into this, it will fail on a lan, where kickid will go through
								engine->ServerCommand( CFmtStr( "banid %d %d;", sv_kick_ban_duration.GetInt(), pCSScorer->GetUserID() ) );
							}
						}

						engine->ServerCommand( UTIL_VarArgs( "kickid_ex %d %d For killing a teammate at round start\n", pCSScorer->GetUserID(), IsPlayingOffline() ? 0 : 1 ) );
					}
                }
            }

            if ( !(pCSScorer->m_iDisplayHistoryBits & DHF_FRIEND_KILLED) )
            {
                pCSScorer->m_iDisplayHistoryBits |= DHF_FRIEND_KILLED;
                pCSScorer->HintMessage( "#SFUI_Notice_Hint_careful_around_teammates", false );
            }
        }
        else
        {
			// [tj] Added a check to make sure we don't get money for suicides.
			if (pCSScorer != pCSVictim)
			{
				if ( pCSScorer->m_nTimeToKill < 0 )
				{
					pCSScorer->m_nTimeToKill = gpGlobals->curtime - pCSScorer->m_flLifeStartTime;
				}

				// Get the cash award from the weapon and scale it by the gamemode factor
				// const CCSWeaponInfo* pWeaponInfo = CCSPlayer::GetWeaponInfoFromDamageInfo(info);
				if ( pWeapon )
				{
					pCSScorer->AddAccountAward( PlayerCashAward::KILLED_ENEMY, pWeapon->GetKillAward(), pWeapon );
					// m_DeferredCallQueue.QueueCall( pCSScorer, &CCSPlayer::AddAccountAward,  PlayerCashAward::KILLED_ENEMY, award, pWeaponInfo );
				}
				else
				{
					pCSScorer->AddAccountAward( PlayerCashAward::KILLED_ENEMY );
					// m_DeferredCallQueue.QueueCall( pCSScorer, &CCSPlayer::AddAccountAward,  PlayerCashAward::KILLED_ENEMY );
				}
			}

			CWeaponCSBase* pInfoWeapon = dynamic_cast<CWeaponCSBase *>( info.GetWeapon() );
			CEconItemView *pEconWeapon = pInfoWeapon ? pInfoWeapon->GetEconItemView() : NULL ;

			EconEntity_OnOwnerKillEaterEvent( pEconWeapon, pCSScorer, pCSVictim, kKillEaterEvent_PlayerKill );

			/* 
            if ( !(pCSScorer->m_iDisplayHistoryBits & DHF_ENEMY_KILLED) )
            {
                pCSScorer->m_iDisplayHistoryBits |= DHF_ENEMY_KILLED;
                pCSScorer->HintMessage( "#SFUI_Notice_Hint_win_round_by_killing_enemy", false );
            }
            */
        }

		if ( CSGameRules()->IsPlayingCoopMission() && mp_use_respawn_waves.GetInt() == 2 )
		{
			int enemyTeam = CSGameRules()->GetCoopEnemyTeamNumber();

			if ( pCSVictim->IsBot() && pCSVictim->GetTeamNumber() == enemyTeam )
			{
				// get the number of enemies remaining and have one of the surviving human players call it out
				int count = 0;

				for ( int i = 1; i <= gpGlobals->maxClients; ++i )
				{
					CBaseEntity *player = UTIL_PlayerByIndex( i );
					if ( player == NULL || player->GetTeamNumber() != enemyTeam || !player->IsAlive() )
						continue;

					count++;
				}

				const char* radioMsg = nullptr;
				switch ( count )
				{
				case 3: radioMsg = "ThreeEnemiesLeft"; break;
				case 2: radioMsg = "TwoEnemiesLeft"; break;
				case 1: radioMsg = "OneEnemyLeft"; break;
				}

				if ( radioMsg )
				{
					UTIL_RadioFromRandomPlayer( CSGameRules()->GetCoopHumanTeamNumber(), radioMsg, "" );
				}
			}
		}
    }

	void CCSGameRules::SendKickBanToGC( CCSPlayer *pPlayer, EMsgGCCStrike15_v2_MatchmakingKickBanReason_t eReason )
	{
		/** Removed for partner depot **/
	}

	void CCSGameRules::SendKickBanToGCforAccountId( uint32 uiAccountId, EMsgGCCStrike15_v2_MatchmakingKickBanReason_t eReason )
	{
		/** Removed for partner depot **/
	}

    void CCSGameRules::InitDefaultAIRelationships()
    {
        //  Allocate memory for default relationships
        CBaseCombatCharacter::AllocateDefaultRelationships();

        // --------------------------------------------------------------
        // First initialize table so we can report missing relationships
        // --------------------------------------------------------------
        int i, j;
        int iNumClasses = GameRules() ? GameRules()->NumEntityClasses() : LAST_SHARED_ENTITY_CLASS;
        for (i=0;i<iNumClasses;i++)
        {
            for (j=0;j<iNumClasses;j++)
            {
                // By default all relationships are neutral of priority zero
                CBaseCombatCharacter::SetDefaultRelationship( (Class_T)i, (Class_T)j, D_NU, 0 );
            }
        }
    }

    //------------------------------------------------------------------------------
    // Purpose : Return classify text for classify type
    //------------------------------------------------------------------------------
    const char *CCSGameRules::AIClassText(int classType)
    {
        switch (classType)
        {
            case CLASS_NONE:			return "CLASS_NONE";
            case CLASS_PLAYER:			return "CLASS_PLAYER";
            default:					return "MISSING CLASS in ClassifyText()";
        }
    }

    //-----------------------------------------------------------------------------
    // Purpose: When gaining new technologies in TF, prevent auto switching if we
    //  receive a weapon during the switch
    // Input  : *pPlayer - 
    //			*pWeapon - 
    // Output : Returns true on success, false on failure.
    //-----------------------------------------------------------------------------
    bool CCSGameRules::FShouldSwitchWeapon( CBasePlayer *pPlayer, CBaseCombatWeapon *pWeapon )
    {
        bool bIsBeingGivenItem = false;
        CCSPlayer *pCSPlayer = ToCSPlayer( pPlayer );
        if ( pCSPlayer && pCSPlayer->IsBeingGivenItem() )
            bIsBeingGivenItem = true;

        if ( pPlayer->GetActiveWeapon() && pPlayer->IsNetClient() && !bIsBeingGivenItem )
        {
            // Player has an active item, so let's check cl_autowepswitch.
            const char *cl_autowepswitch = engine->GetClientConVarValue( ENTINDEX( pPlayer->edict() ), "cl_autowepswitch" );
            if ( cl_autowepswitch && atoi( cl_autowepswitch ) <= 0 )
            {
                return false;
            }
        }

        if ( pPlayer->IsBot() && !bIsBeingGivenItem )
        {
            return false;
        }

        if ( !GetAllowWeaponSwitch() )
        {
            return false;
        }

        return BaseClass::FShouldSwitchWeapon( pPlayer, pWeapon );
    }

    //-----------------------------------------------------------------------------
    // Purpose: 
    // Input  : allow - 
    //-----------------------------------------------------------------------------
    void CCSGameRules::SetAllowWeaponSwitch( bool allow )
    {
        m_bAllowWeaponSwitch = allow;
    }

    //-----------------------------------------------------------------------------
    // Purpose: 
    // Output : Returns true on success, false on failure.
    //-----------------------------------------------------------------------------
    bool CCSGameRules::GetAllowWeaponSwitch()
    {
        return m_bAllowWeaponSwitch;
    }

    //-----------------------------------------------------------------------------
    // Purpose: 
    // Input  : *pPlayer - 
    // Output : const char
    //-----------------------------------------------------------------------------
    const char *CCSGameRules::SetDefaultPlayerTeam( CBasePlayer *pPlayer )
    {
        Assert( pPlayer );
        return BaseClass::SetDefaultPlayerTeam( pPlayer );
    }


    void CCSGameRules::LevelInitPreEntity()
    {
        BaseClass::LevelInitPreEntity();

        // TODO for CZ-style hostages: TheHostageChatter->Precache();
		m_SurvivalRules.OnLevelInitPreEntity();
    }


    void CCSGameRules::LevelInitPostEntity()
    {
        BaseClass::LevelInitPostEntity();

        m_bLevelInitialized = false; // re-count CT and T start spots now that they exist

        // Figure out from the entities in the map what kind of map this is (bomb run, prison escape, etc).
        CheckMapConditions();

		m_SurvivalRules.OnLevelInitPostEntity();
    }
    
    float CCSGameRules::FlPlayerFallDamage( CBasePlayer *pPlayer )
    {
        float fFallVelocity = pPlayer->m_Local.m_flFallVelocity - CS_PLAYER_MAX_SAFE_FALL_SPEED;
        float fallDamage = fFallVelocity * CS_DAMAGE_FOR_FALL_SPEED;

		fallDamage *= sv_falldamage_scale.GetFloat();

		CCSPlayer *pCSPlayer = ToCSPlayer( pPlayer );
		if ( pCSPlayer && pCSPlayer->m_nParachuteState == PLAYER_PARACHUTE_DEPLOYED )
		{
			// Players expect that parachuting will save them from fall damage. In practice however, if their parachute has just deployed,
			// naturally the parachute still needs time to reduce their velocity to non-lethal speed. This means that it's entirely
			// possible to deploy your chute too late and still die from the fall. This makes perfect sense, but players don't like it
			// and it's a crappy experience. So we're intentionally zeroing fall damage with a deployed chute.
			return 0.0f;
		}

        return fallDamage;
    } 


	bool CCSGameRules::ClientConnected( edict_t *pEntity, const char *pszName, const char *pszAddress, char *reject, int maxrejectlen )
	{
		if ( !BaseClass::ClientConnected( pEntity, pszName, pszAddress, reject, maxrejectlen ) )
			return false;

		return true;
	}
    
    void CCSGameRules::ClientDisconnected( edict_t *pClient )
    {
		// [vitaliy] Players who disconnect while alive and reconnect in competitive mode before the end of the round will not receive win money just like suicide
		if ( CCSPlayer *pPlayer = ToCSPlayer( GetContainingEntity( pClient ) ) )
		{
			if ( pPlayer->IsAlive() )
			{
				if ( mp_disconnect_kills_players.GetBool() && ( mp_disconnect_kills_bots.GetBool() || !pPlayer->IsBot() ) )
				{
					CTakeDamageInfo info( pPlayer, pPlayer, 0.0f, DMG_NEVERGIB );
					pPlayer->SetHealth( 0 );
					pPlayer->Event_Killed( info );
					pPlayer->Event_Dying();
				}
				else
				{
					// old behavior.
					pPlayer->ProcessSuicideAsKillReward();
				}
			}
		}

        BaseClass::ClientDisconnected( pClient );

        // [tj] Clear domination data when a player disconnects
        if ( CCSPlayer *pPlayer = ToCSPlayer( GetContainingEntity( pClient ) ) )
        {
            pPlayer->RemoveNemesisRelationships();
        }

		UpdateTeamClanNames( TEAM_TERRORIST ); 
		UpdateTeamClanNames( TEAM_CT ); 

		if ( !IsPlayingCoopMission() )
			CheckWinConditions();
    }


    // Called when game rules are destroyed by CWorld
    void CCSGameRules::LevelShutdown()
    {
        BaseClass::LevelShutdown();
    }

    //---------------------------------------------------------------------------------------------------
    /**
     * Check if the scenario has been won/lost.
     * Return true if the scenario is over, false if the scenario is still in progress
     */
    bool CCSGameRules::CheckWinConditions( void )
    {
		if ( mp_ignore_round_win_conditions.GetBool() || m_bLoadingRoundBackupData )
			return false;

        // If a winner has already been determined.. then get the heck out of here
        if ( IsWarmupPeriod() || ( m_iRoundWinStatus != WINNER_NONE ) )
        {
            // still check if we lost players to where we need to do a full reset next round...
            int NumDeadCT, NumDeadTerrorist, NumAliveTerrorist, NumAliveCT;
            InitializePlayerCounts( NumAliveTerrorist, NumAliveCT, NumDeadTerrorist, NumDeadCT );

            bool bNeededPlayers = false;
            NeededPlayersCheck( bNeededPlayers );

            return true;
        }

        // Initialize the player counts..
        int NumDeadCT, NumDeadTerrorist, NumAliveTerrorist, NumAliveCT;
        InitializePlayerCounts( NumAliveTerrorist, NumAliveCT, NumDeadTerrorist, NumDeadCT );

		if ( IsPlayingSurvival() )
		{
			bool bSurvivalGameInProgress = m_SurvivalRules.CheckWinConditions();
			if ( !bSurvivalGameInProgress && ( m_match.GetPhase() != GAMEPHASE_MATCH_ENDED ) )
			{
				// Terminate the game server correctly
				m_match.SetPhase( GAMEPHASE_MATCH_ENDED );
				m_phaseChangeAnnouncementTime = gpGlobals->curtime + 1.5;
				GoToIntermission();
			}
			return bSurvivalGameInProgress;
			// survival doesn't care about any of the win conditions below
		}

        /*********************************** GUN GAME PROGRESSIVE CHECK *******************************************************/
        if ( IsPlayingGunGame() )
        {
            if ( GunGameProgressiveEndCheck( ) )
            {
                return true;
            }

            if ( IsPlayingGunGameProgressive() )
            {
                return false;
            }
        }

        /***************************** OTHER PLAYER's CHECK *********************************************************/
        bool bNeededPlayers = false;
        if ( NeededPlayersCheck( bNeededPlayers ) )
            return false;

        /****************************** PRISON ESCAPE CHECK *******************************************************/
        if ( PrisonRoundEndCheck() )
            return true;


        /****************************** BOMB CHECK ********************************************************/
        if ( BombRoundEndCheck( bNeededPlayers ) )
            return true;


        /***************************** TEAM EXTERMINATION CHECK!! *********************************************************/
        // CounterTerrorists won by virture of elimination
        if ( TeamExterminationCheck( NumAliveTerrorist, NumAliveCT, NumDeadTerrorist, NumDeadCT, bNeededPlayers ) )
            return true;

        
        /******************************** HOSTAGE RESCUE CHECK ******************************************************/
        if ( HostageRescueRoundEndCheck( bNeededPlayers ) )
			return true;

		/******************************** GUARDIAN MODE CHECK ******************************************************/
		if ( IsPlayingCoopGuardian() && (
			BombPlantedRoundEndCheck()
			|| CTsReachedHostageRoundEndCheck()
			) )
			return true;

        // scenario not won - still in progress
        return false;
    }


    bool CCSGameRules::NeededPlayersCheck( bool &bNeededPlayers )
    {
        if ( IsPlayingTraining() )
            return false;

		// When we silently are performing team manipulations then
		// don't run this players check logic which might cause
		// round to restart ignoring all team scores and player stats
		// assignments.
		if ( m_bLoadingRoundBackupData )
			return false;

		// Run this check differently in queue matchmaking mode
		if ( IsQueuedMatchmaking() )
		{
			if ( !m_bFirstConnected &&
				( m_iNumSpawnableTerrorist || m_iNumSpawnableCT ) )
			{
				m_bFreezePeriod  = false; //Make sure we are not on the FreezePeriod.
				m_bCompleteReset = true;

				TerminateRound( 0.5f, Game_Commencing );
				m_bFirstConnected = true;
				return true;
			}
			return false;
		}

        // We needed players to start scoring
        // Do we have them now?
        if( !m_iNumSpawnableTerrorist && !m_iNumSpawnableCT )
        {
            DevMsg( "Scoring will not start until both teams have players.\n" );
            //UTIL_ClientPrintAll( HUD_PRINTCONSOLE, "#SFUI_Notice_Game_scoring" );
            bNeededPlayers = true;

            m_bFirstConnected = false;
        }

		if ( !m_bFirstConnected &&
			( m_iNumSpawnableTerrorist || m_iNumSpawnableCT ) )
        {
            // Start the round immediately when the first person joins
            // UTIL_LogPrintf( "World triggered \"Game_Commencing\"\n" );

            m_bFreezePeriod  = false; //Make sure we are not on the FreezePeriod.
            m_bCompleteReset = true;

            TerminateRound( 0.5f, Game_Commencing );
            m_bFirstConnected = true;
            return true;
        }

        return false;
    }


    void CCSGameRules::InitializePlayerCounts(
        int &NumAliveTerrorist,
        int &NumAliveCT,
        int &NumDeadTerrorist,
        int &NumDeadCT
        )
    {
        NumAliveTerrorist = NumAliveCT = NumDeadCT = NumDeadTerrorist = 0;
        m_iNumTerrorist = m_iNumCT = m_iNumSpawnableTerrorist = m_iNumSpawnableCT = 0;
        m_iHaveEscaped = 0;

        // Set team filters

        m_filterCT.Reset();
        m_filterCT.MakeReliable();
        m_filterCT.AddRecipientsByTeam( GetGlobalTeam(TEAM_CT) );

        m_filterTerrorist.Reset();
        m_filterTerrorist.MakeReliable();
        m_filterTerrorist.AddRecipientsByTeam( GetGlobalTeam(TEAM_TERRORIST) );


        // Count how many dead players there are on each team.
        for ( int iTeam=0; iTeam < GetNumberOfTeams(); iTeam++ )
        {
            CTeam *pTeam = GetGlobalTeam( iTeam );

            for ( int iPlayer=0; iPlayer < pTeam->GetNumPlayers(); iPlayer++ )
            {
                CCSPlayer *pPlayer = ToCSPlayer( pTeam->GetPlayer( iPlayer ) );
                Assert( pPlayer );
                if ( !pPlayer )
                    continue;

                Assert( pPlayer->GetTeamNumber() == pTeam->GetTeamNumber() );

                switch ( pTeam->GetTeamNumber() )
                {
                case TEAM_CT:
                    m_iNumCT++;

                    if ( pPlayer->State_Get() != STATE_PICKINGCLASS )
                        m_iNumSpawnableCT++;

                    if ( pPlayer->m_lifeState != LIFE_ALIVE )
                        NumDeadCT++;
                    else
                        NumAliveCT++;

                    break;

                case TEAM_TERRORIST:
                    m_iNumTerrorist++;

                    if ( pPlayer->State_Get() != STATE_PICKINGCLASS )
                        m_iNumSpawnableTerrorist++;

                    if ( pPlayer->m_lifeState != LIFE_ALIVE )
                        NumDeadTerrorist++;
                    else
                        NumAliveTerrorist++;

                    // Check to see if this guy escaped.
                    if ( pPlayer->m_bEscaped == true )
                        m_iHaveEscaped++;

                    break;
                }
            }
        }
    }

	void CCSGameRules::AddHostageRescueTime( void )
	{
		if ( m_bAnyHostageReached )
			return;

		m_bAnyHostageReached = true;
		
		// If the round is already over don't add additional time
		bool roundIsAlreadyOver = (CSGameRules()->m_iRoundWinStatus != WINNER_NONE);
		if ( roundIsAlreadyOver )
			return;

		m_iRoundTime += (int)( mp_hostages_rescuetime.GetFloat() * 60 );

		UTIL_ClientPrintAll( HUD_PRINTTALK, "#hostagerescuetime" );
	}

    bool CCSGameRules::HostageRescueRoundEndCheck( bool bNeededPlayers )
    {
        // Check to see if 50% of the hostages have been rescued.
        CHostage* hostage = NULL;

        int iNumHostages = g_Hostages.Count();
        int iNumLeftToRescue = 0;
        int i;

        for ( i=0; i<iNumHostages; i++ )
        {
            hostage = g_Hostages[i];

            if ( hostage->m_iHealth > 0 && !hostage->IsRescued() ) // We've found a live hostage. don't end the round
                iNumLeftToRescue++;
        }

		// the number of hostages that can be left un rescued, but still win
		int iNumRescuedToWin = mp_hostages_rescuetowin.GetInt() == 0 ? iNumHostages : MIN( iNumHostages, mp_hostages_rescuetowin.GetInt() );
		int iNumLeftCanWin = MAX( 0, iNumHostages - iNumRescuedToWin );

        m_iHostagesRemaining = iNumLeftToRescue;

        if ( (iNumLeftToRescue >= iNumLeftCanWin) && (iNumHostages > 0) )
        {
            if ( m_iHostagesRescued >= iNumRescuedToWin	)
            {
                if ( !bNeededPlayers )
                {
                    m_match.AddCTWins( 1 );					
                }

                CCS_GameStats.Event_AllHostagesRescued();
                // tell the bots all the hostages have been rescued
                IGameEvent * event = gameeventmanager->CreateEvent( "hostage_rescued_all" );
                if ( event )
                {
                    gameeventmanager->FireEvent( event );
                }

				CTeam *pTeam = GetGlobalTeam( TEAM_TERRORIST );
				if ( IsPlayingCoopMission() && pTeam )
					pTeam->MarkSurrendered();

				TerminateRoundDelayedTeamCashAwards_t award = { TEAM_CT, TeamCashAward::WIN_BY_HOSTAGE_RESCUE };

                TerminateRound( mp_round_restart_delay.GetFloat(), All_Hostages_Rescued, &award, 1 );
                return true;
            }
        }

        return false;
    }


    bool CCSGameRules::PrisonRoundEndCheck()
    {
        //MIKETODO: get this working when working on prison escape
        /*
        if (m_bMapHasEscapeZone == true)
        {
            float flEscapeRatio;

            flEscapeRatio = (float) m_iHaveEscaped / (float) m_iNumEscapers;

            if (flEscapeRatio >= m_flRequiredEscapeRatio)
            {
                BroadcastSound( "Event.TERWin" );
                m_iAccountTerrorist += 3150;

                if ( !bNeededPlayers )
                {
                    m_iNumTerroristWins ++;
                    // Update the clients team score
                    UpdateTeamScores();
                }
                EndRoundMessage( "#Terrorists_Escaped", Terrorists_Escaped );
                TerminateRound( mp_round_restart_delay.GetFloat(), WINNER_TER );
                return;
            }
            else if ( NumAliveTerrorist == 0 && flEscapeRatio < m_flRequiredEscapeRatio)
            {
                BroadcastSound( "Event.CTWin" );
                m_iAccountCT += (1 - flEscapeRatio) * 3500; // CTs are rewarded based on how many terrorists have escaped...
                
                if ( !bNeededPlayers )
                {
                    m_iNumCTWins++;
                    // Update the clients team score
                    UpdateTeamScores();
                }
                EndRoundMessage( "#CTs_PreventEscape", CTs_PreventEscape );
                TerminateRound( mp_round_restart_delay.GetFloat(), WINNER_CT );
                return;
            }

            else if ( NumAliveTerrorist == 0 && NumDeadTerrorist != 0 && m_iNumSpawnableCT > 0 )
            {
                BroadcastSound( "Event.CTWin" );
                m_iAccountCT += (1 - flEscapeRatio) * 3250; // CTs are rewarded based on how many terrorists have escaped...
                
                if ( !bNeededPlayers )
                {
                    m_iNumCTWins++;
                    // Update the clients team score
                    UpdateTeamScores();
                }
                EndRoundMessage( "#Escaping_Terrorists_Neutralized", Escaping_Terrorists_Neutralized );
                TerminateRound( mp_round_restart_delay.GetFloat(), WINNER_CT );
                return;
            }
            // else return;    
        }
        */

        return false;
    }

	bool CCSGameRules::BombPlantedRoundEndCheck( void )
	{
		if ( !IsPlayingCoopGuardian() )
			return false;

		bool roundIsAlreadyOver = ( CSGameRules()->m_iRoundWinStatus != WINNER_NONE );
		if ( roundIsAlreadyOver )
			return false;

		if ( m_bBombPlanted )
		{
			m_match.AddTerroristWins( 1 );

			TerminateRound( mp_round_restart_delay.GetFloat(), Terrorists_Planted );

			return true;
		}

		return false;
	}

	bool CCSGameRules::CTsReachedHostageRoundEndCheck( void )
	{
		if ( !IsPlayingCoopGuardian() )
			return false;

		bool roundIsAlreadyOver = ( CSGameRules()->m_iRoundWinStatus != WINNER_NONE );
		if ( roundIsAlreadyOver )
			return false;

		if ( m_bHasHostageBeenTouched )
		{
			m_match.AddCTWins( 1 );

			TerminateRound( mp_round_restart_delay.GetFloat(), CTs_ReachedHostage );

			return true;
		}

		return false;
	}

	bool CCSGameRules::GuardianAllKillsAchievedCheck( void )
	{
		if ( !IsPlayingCoopGuardian() )
			return false;

		bool roundIsAlreadyOver = ( CSGameRules()->m_iRoundWinStatus != WINNER_NONE );
		if ( roundIsAlreadyOver )
			return false;

		if ( m_nGuardianModeSpecialKillsRemaining <= 0 )
		{
			// Let the operator comment about player victory
			UTIL_GlobalChatter( "Guardian.Victory" );

			int teamNumber = GetCoopHumanTeamNumber();

			if ( teamNumber == TEAM_TERRORIST )
			{
				m_match.AddTerroristWins( 1 );
				if ( CTeam *pTeam = GetGlobalTeam( TEAM_CT ) )
					pTeam->MarkSurrendered();
				TerminateRound( mp_round_restart_delay.GetFloat(), Terrorists_Win );
			}
			else if ( teamNumber == TEAM_CT )
			{
				m_match.AddCTWins( 1 );
				if ( CTeam *pTeam = GetGlobalTeam( TEAM_TERRORIST ) )
					pTeam->MarkSurrendered();
				TerminateRound( mp_round_restart_delay.GetFloat(), CTs_Win );
			}
			else
			{
				Assert( false );
			}
			
			return true;
		}

		return false;
	}

    void CCSGameRules::IncrementGunGameTerroristWeapons( void )
    {
        if ( IsPlayingGunGameTRBomb() )
        {
            for ( int iTeam=0; iTeam < GetNumberOfTeams(); iTeam++ )
            {
                CTeam *pTeam = GetGlobalTeam( iTeam );

                for ( int iPlayer=0; iPlayer < pTeam->GetNumPlayers(); iPlayer++ )
                {
                    CCSPlayer *pPlayer = ToCSPlayer( pTeam->GetPlayer( iPlayer ) );
                    Assert( pPlayer );
                    if ( !pPlayer )
                        continue;

                    if ( pTeam->GetTeamNumber() == TEAM_TERRORIST )
                    {
                        // Increment the player's gun game progressive weapon
                        pPlayer->IncrementGunGameProgressiveWeapon( mp_ggtr_bomb_detonation_bonus.GetInt() );
                    }
                }
            }
        }
    }

    void CCSGameRules::IncrementGunGameCTWeapons( void )
    {
        if ( IsPlayingGunGameTRBomb() )
        {
            for ( int iTeam=0; iTeam < GetNumberOfTeams(); iTeam++ )
            {
                CTeam *pTeam = GetGlobalTeam( iTeam );

                for ( int iPlayer=0; iPlayer < pTeam->GetNumPlayers(); iPlayer++ )
                {
                    CCSPlayer *pPlayer = ToCSPlayer( pTeam->GetPlayer( iPlayer ) );
                    Assert( pPlayer );
                    if ( !pPlayer )
                        continue;

                    if ( pTeam->GetTeamNumber() == TEAM_CT )
                    {
                        // Increment the player's gun game progressive weapon
                        pPlayer->IncrementGunGameProgressiveWeapon( mp_ggtr_bomb_defuse_bonus.GetInt() );
                    }
                }
            }
        }
    }

    bool CCSGameRules::IsBotOnlyTeam( int nTeamNumber )
    {
        CTeam *pTeam = GetGlobalTeam( nTeamNumber );

        if ( pTeam )
        {
            // Loop through all players on team and determine if they are all bots
            for ( int iPlayer=0; iPlayer < pTeam->GetNumPlayers(); iPlayer++ )
            {
                CCSPlayer *pPlayer = ToCSPlayer( pTeam->GetPlayer( iPlayer ) );

                if ( !pPlayer )
                {
                    continue;
                }

                if ( !pPlayer->IsBot() )
                {
                    return false;
                }
            }
        }

        return true;
    }

    bool CCSGameRules::GunGameProgressiveEndCheck( void )
    {
        bool	bDidEnd = false;

        if ( m_match.GetPhase() == GAMEPHASE_MATCH_ENDED )
        {
            // No need to perform the check if the match has ended
            return false;
        }
        
        if ( IsPlayingGunGameTRBomb() && !mp_ggtr_last_weapon_kill_ends_half.GetBool() )
        {
            // ignore kills made with the final weapon and let the round end naturally.
            return false;
        }

        if ( !IsPlayingGunGame() )
            return false;

        CCSPlayer *pWinner = NULL;

        // Test if a player made a kill with the final gun game weapon
        for ( int iTeam=0; iTeam < GetNumberOfTeams(); iTeam++ )
        {
            CTeam *pTeam = GetGlobalTeam( iTeam );

            for ( int iPlayer=0; iPlayer < pTeam->GetNumPlayers(); iPlayer++ )
            {
                CCSPlayer *pPlayer = ToCSPlayer( pTeam->GetPlayer( iPlayer ) );
                Assert( pPlayer );
                if ( !pPlayer )
                    continue;

                if ( pPlayer->MadeFinalGunGameProgressiveKill() )
                {
                    pWinner = pPlayer;

                    float delayTime;

                    if ( IsPlayingGunGameProgressive() || IsPlayingGunGameTRBomb() )
                    {
                        delayTime = mp_ggprogressive_round_restart_delay.GetFloat();
                    }
                    else
                    {
                        delayTime = mp_round_restart_delay.GetFloat();
                    }

                    // Award bonus points because the kill was made with the final progression weapon
                    if ( IsPlayingGunGameTRBomb() )
                    {
                        if ( pTeam->GetTeamNumber() == TEAM_CT )
                        {
                            m_match.AddCTWins( 1 );
                            m_match.AddCTBonusPoints( mp_ggtr_end_round_kill_bonus.GetInt() );								
                        }
                        else if ( pTeam->GetTeamNumber() == TEAM_TERRORIST )
                        {
                            m_match.AddTerroristWins( 1 );
                            m_match.AddTerroristBonusPoints( mp_ggtr_end_round_kill_bonus.GetInt() );								
                        }
                    }

                    if ( IsPlayingGunGameProgressive() )
                    {
                        m_bCompleteReset = true;

                        bDidEnd = true;

                        m_match.SetPhase( GAMEPHASE_MATCH_ENDED );

                        m_phaseChangeAnnouncementTime = gpGlobals->curtime + mp_win_panel_display_time.GetInt();

                        GoToIntermission();
                    }

                    TerminateRound( delayTime, ( pTeam->GetTeamNumber() == TEAM_CT ) ? CTs_Win : Terrorists_Win );

                    break;
                }
            }

            if ( bDidEnd )
                break;
        }

        if ( bDidEnd && IsPlayingGunGameProgressive() )
        {
            for ( int i = 1; i <= MAX_PLAYERS; i++ )
            {
                CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );

                if ( pPlayer )
                {
                    pPlayer->AddFlag( FL_FROZEN );
                    pPlayer->Unblind();
                }
            }
        }

        return bDidEnd;
    }


    bool CCSGameRules::BombRoundEndCheck( bool bNeededPlayers )
    {
        // Check to see if the bomb target was hit or the bomb defused.. if so, then let's end the round!
        if ( ( m_bTargetBombed == true ) && ( m_bMapHasBombTarget == true ) )
        {
            if ( !bNeededPlayers )
            {
                m_match.AddTerroristWins( 1 );
            }

			TerminateRoundDelayedTeamCashAwards_t award = { TEAM_TERRORIST, TeamCashAward::TERRORIST_WIN_BOMB };

            TerminateRound( mp_round_restart_delay.GetFloat(), Target_Bombed, &award, 1 );
            return true;
        }
        else
        if ( ( m_bBombDefused == true ) && ( m_bMapHasBombTarget == true ) )
        {
            if ( !bNeededPlayers )
            {
                m_match.AddCTWins( 1 );
            }

			TerminateRoundDelayedTeamCashAwards_t awards[2] = {
				{ TEAM_CT, TeamCashAward::WIN_BY_DEFUSING_BOMB },
				{ TEAM_TERRORIST, TeamCashAward::PLANTED_BOMB_BUT_DEFUSED }
			}; // give the T's a little bonus for planting the bomb even though it was defused.

            TerminateRound( mp_round_restart_delay.GetFloat(), Bomb_Defused, awards, 2 );
            return true;
        }

        return false;
    }

    // [dkorus] note, this is the standard "end of round mvp" for the case where a more specific MVP condition has not been met
    //			examples of more specific conditions:  Planting the bomb, defusing the bomb, rescuing the hostages, escaping as the VIP, etc
    CCSPlayer * CCSGameRules::CalculateEndOfRoundMVP()
    {
        CCSPlayer* pMVP = NULL;
        int maxKills = 0;
        int maxDamage = 0;
        CSMvpReason_t mvpReason = CSMVP_ELIMINATION;

		if ( CSGameRules()->IsPlayingSurvival() )
		{	// Survival game rules have a different winners panel without MVPs
			return NULL;
		}
        else if ( CSGameRules()->IsPlayingGunGameProgressive() )
        {
            // Handle winner of gun game progressive
            for ( int i = 1; i <= gpGlobals->maxClients; i++ )
            {
                CCSPlayer* pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );
                if ( pPlayer )
                {
                    if ( pPlayer->MadeFinalGunGameProgressiveKill() )
                    {
                        pMVP = pPlayer;
                        mvpReason = CSMVP_GUNGAMEWINNER;
                        break;
                    }
                }
            }
        }
		else
        {
			int nBestScore = 0;
            // Handle non-gun game progressive mvp
            for ( int i = 1; i <= gpGlobals->maxClients; i++ )
            {
                CCSPlayer* pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );
                if ( pPlayer )
                {
					if ( pPlayer->HasBeenControlledThisRound() )
						continue;

					if ( CSGameRules()->IsPlayingGunGameDeathmatch() )
					{
						// in team mode, only consider players on the winning team
						if ( CSGameRules()->IsPlayingTeamDM() && CSGameRules()->m_iRoundWinStatus != WINNER_DRAW && pPlayer->GetTeamNumber() != CSGameRules()->m_iRoundWinStatus )
							continue;

						int nScore = pPlayer->GetScore();
						int nEntindex = pPlayer->entindex();
						if ( nScore > nBestScore || (nScore == nBestScore && nEntindex < maxKills) )
						{
							pMVP = pPlayer;
							maxKills = nEntindex;
							nBestScore = nScore;
							mvpReason = CSGameRules()->IsPlayingTeamDM() ? CSMVP_TEAMDMSCORE : CSMVP_GUNGAMEWINNER;
						}
					}
					else
					{
						// only consider players on the winning team
						if ( CSGameRules()->m_iRoundWinStatus != WINNER_DRAW && pPlayer->GetTeamNumber() != CSGameRules()->m_iRoundWinStatus )
							continue;

						int nKills = pPlayer->GetNumRoundKills(); // - pPlayer->m_iNumRoundTKs; - for most eliminations count only enemies killed
						int nDamage = pPlayer->GetTotalActualHealthRemovedFromEnemies();
						if ( nKills > maxKills || ( nKills == maxKills && nDamage > maxDamage ) )
						{
							pMVP = pPlayer;
							maxKills = nKills;
							maxDamage = nDamage;
						}
					}
                }
            }
        }

        if ( pMVP )
        {
            pMVP->IncrementNumMVPs( mvpReason );
        }

		return pMVP;
    }


    bool CCSGameRules::TeamExterminationCheck(
        int NumAliveTerrorist,
        int NumAliveCT,
        int NumDeadTerrorist,
        int NumDeadCT,
        bool bNeededPlayers
    )
    {
		bool bCTsRespawn = mp_respawn_on_death_ct.GetBool();
		bool bTsRespawn = mp_respawn_on_death_t.GetBool();

        if ( ( m_iNumCT > 0 && m_iNumSpawnableCT > 0 ) && ( m_iNumTerrorist > 0 && m_iNumSpawnableTerrorist > 0 ) )
        {
			// this checks for last man standing rules
			if ( mp_teammates_are_enemies.GetBool() )
			{
				// last CT alive
				if ( NumAliveTerrorist == 0 && NumDeadTerrorist != 0 && !bTsRespawn && NumAliveCT == 1 )
				{
					m_match.AddCTWins( 1 );
					TerminateRound( mp_round_restart_delay.GetFloat(), CTs_Win );
					return true;
				}

				if ( NumAliveCT == 0 && NumDeadCT != 0 && !bCTsRespawn && NumAliveTerrorist == 1 )
				{
					m_match.AddTerroristWins( 1 );
					TerminateRound( mp_round_restart_delay.GetFloat(), Terrorists_Win );
					return true;
				}

				if ( NumAliveCT == 0 && !bCTsRespawn && NumAliveTerrorist == 0 && !bTsRespawn && ( m_iNumTerrorist > 0 || m_iNumCT > 0 ) )
				{
					TerminateRound( mp_round_restart_delay.GetFloat(), Round_Draw );
					return true;
				}
			}
			else
			{
				// CTs WON (if they don't respawn)
				if ( NumAliveTerrorist == 0 && NumDeadTerrorist != 0 && !bTsRespawn && m_iNumSpawnableCT > 0 )
				{
					bool nowin = false;

					for ( int iGrenade=0; iGrenade < g_PlantedC4s.Count(); iGrenade++ )
					{
						CPlantedC4 *pC4 = g_PlantedC4s[iGrenade];

						if ( pC4->IsBombActive() )
							nowin = true;
					}

					if ( !nowin )
					{
						if ( !bNeededPlayers )
						{
							m_match.AddCTWins( 1 );
						}

						TerminateRoundDelayedTeamCashAwards_t award = { TEAM_CT,
							( m_bMapHasBombTarget ) ? TeamCashAward::ELIMINATION_BOMB_MAP : TeamCashAward::ELIMINATION_HOSTAGE_MAP_CT }; 

						TerminateRound( mp_round_restart_delay.GetFloat(), CTs_Win, &award, 1 );
						return true;
					}
				}
				else if ( mp_use_respawn_waves.GetInt() == 2 && (IsPlayingCoopGuardian() || IsPlayingCoopMission()) )
				{
					int nTeam = GetCoopHumanTeamNumber();
					int nOtherTeam = GetCoopEnemyTeamNumber();

					if ( ( nOtherTeam == TEAM_CT && NumAliveCT == 0 && NumDeadCT != 0 && bCTsRespawn ) || ( nOtherTeam == TEAM_TERRORIST && NumAliveTerrorist == 0 && NumDeadTerrorist != 0 && bTsRespawn ) )
					{
						// HACK
						//play a sound here for all players on other team
						for ( int j = 1; j <= MAX_PLAYERS; j++ )
						{
							CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( j ) );
							if ( pPlayer && pPlayer->GetTeamNumber() == nTeam )
							{
								CSingleUserRecipientFilter filter( pPlayer );
								pPlayer->EmitSound( filter, pPlayer->entindex(), "UI.ArmsRace.BecomeMatchLeader" );
							
								ConVarRef mp_buy_anywhere( "mp_buy_anywhere" );
								if ( IsPlayingCoopGuardian() && pPlayer->IsAlive() && mp_buy_anywhere.GetBool() )
								{
									// force the player to get more ammo
									pPlayer->GuardianForceFillAmmo();
								}
							}
						}
	
						if ( IsPlayingCoopGuardian() )
						{
							// if T's are supposed to respawn
							float flNextRespawn = gpGlobals->curtime + GetRespawnWaveMaxLength( nOtherTeam );
							m_flNextRespawnWave.Set( nOtherTeam, flNextRespawn );

							m_flCoopRespawnAndHealTime = gpGlobals->curtime + MIN( 2.0f, GetRespawnWaveMaxLength( nOtherTeam ) );
										
							// decide if the players can buy now
							int iBuyStatus = sv_buy_status_override.GetInt();
							if ( iBuyStatus > 0 && ((nTeam == TEAM_CT && iBuyStatus != 1) || (nTeam == TEAM_TERRORIST && iBuyStatus != 2)) )
							{
								m_flGuardianBuyUntilTime = -1;
							}
							else
							{
								m_flGuardianBuyUntilTime = flNextRespawn + 7.0f;
							}

							char szSec[32];
							Q_snprintf(szSec, sizeof(szSec), "%d", (int)GetRespawnWaveMaxLength( nOtherTeam ));

							CBroadcastRecipientFilter filter;
							UTIL_ClientPrintFilter(filter, HUD_PRINTCENTER, "#SFUI_Notice_NextWaveIn", szSec);

							// give CTs a bonus for surving the round
							AddTeamAccount( nTeam, TeamCashAward::SURVIVE_GUARDIAN_WAVE );
						}

						// COOP MISSION
						if ( IsPlayingCoopMission() )
						{
							// output to the game entity here that says, we completed the room 
							// and open the door to the next room
							CGameCoopMissionManager *pManager = GetCoopMissionManager();
							if ( !pManager )
							{
								DevMsg( "Coop mission map is missing a game_coopmission_manager entity. You can't keep track of completed waves without it!" );
							}
							else
							{
								pManager->SetWaveCompleted();
								m_flCoopRespawnAndHealTime = gpGlobals->curtime + 3.0f;
							}
						}

						// give Ts something so they aren't always using pistols
						//AddTeamAccount( nOtherTeam, TeamCashAward::CUSTOM_AWARD, 1800, "" );

						// Super-specific hack for coop. After CTs clear a round, the easiest bot on that team
						// gets one level harder.
						CTeam *pTeam = GetGlobalTeam( nOtherTeam );
						if ( pTeam && sv_bots_get_harder_after_each_wave.GetInt() > 0 )
						{
							// increase difficulty of 2 bots
							for ( int iDiffIncrease=0; iDiffIncrease < sv_bots_get_harder_after_each_wave.GetInt(); iDiffIncrease++ )
							{
								CUtlVector < CCSBot* > vecBotsOnTeam;
								if ( pTeam->GetBotMembers( &vecBotsOnTeam ) )
								{
									CCSBot *pLeadBot = NULL;
									BotDifficultyType botHighDifficulty = BOT_EXPERT;
									FOR_EACH_VEC( vecBotsOnTeam, iBot )
									{
										const BotProfile *pProfile = vecBotsOnTeam[iBot]->GetProfile();
										if ( pProfile->GetMaxDifficulty() < botHighDifficulty )
										{
											pLeadBot = vecBotsOnTeam[iBot];
											botHighDifficulty = ( BotDifficultyType )pProfile->GetMaxDifficulty();
										}
									}
									if ( pLeadBot )
									{
										const BotProfile* pNewProfileData = TheBotProfiles->GetRandomProfile( MIN( ( BotDifficultyType )( botHighDifficulty + 1 ), BOT_EXPERT ), pLeadBot->GetTeamNumber(), WEAPONTYPE_UNKNOWN, true );
										if ( pNewProfileData )
										{
											pLeadBot->Initialize( pNewProfileData, pLeadBot->GetTeamNumber() );
										}
									}
								}
							}

							// now just randomize all of the profiles
							CUtlVector < CCSBot* > vecBotsOnTeam;
							if ( pTeam->GetBotMembers( &vecBotsOnTeam ) )
							{
								FOR_EACH_VEC( vecBotsOnTeam, i )
								{
									const BotProfile *pProfile = vecBotsOnTeam[i]->GetProfile();
									const BotProfile* pNewProfileData = TheBotProfiles->GetRandomProfile( ( BotDifficultyType )( pProfile->GetMaxDifficulty() ), vecBotsOnTeam[i]->GetTeamNumber(), WEAPONTYPE_UNKNOWN, true );
									if ( pNewProfileData )
									{
										vecBotsOnTeam[i]->Initialize( pNewProfileData, vecBotsOnTeam[i]->GetTeamNumber() );
									}
								}
							}
						}

// 						// let players buy
// 						int iBuyStatus = -1;
// 						if ( sv_buy_status_override.GetInt() >= 0 )
// 						{
// 							iBuyStatus = sv_buy_status_override.GetInt();
// 						}
// 						else if ( g_pMapInfo )
// 						{
// 							// Check to see if there's a mapping info parameter entity
// 							iBuyStatus = g_pMapInfo->m_iBuyingStatus;
// 						}
// 
// 						if ( iBuyStatus >= 0 )
// 						{
// 							switch ( iBuyStatus )
// 							{
// 								case 1:
// 									m_bCTCantBuy = false;
// 									m_bTCantBuy = true;
// 									Msg( "Only CT's can buy!!\n" );
// 									break;
// 
// 								case 2:
// 									m_bCTCantBuy = true;
// 									m_bTCantBuy = false;
// 									Msg( "Only T's can buy!!\n" );
// 									break;
// 
// 								case 3:
// 									m_bCTCantBuy = true;
// 									m_bTCantBuy = true;
// 									Msg( "No one can buy!!\n" );
// 									break;
// 
// 								default:
// 									m_bCTCantBuy = false;
// 									m_bTCantBuy = false;
// 									break;
// 							}
// 						}
// 
// 						// 				if ( nTeam == TEAM_CT )
// 						// 					m_bCTCantBuy = false;
// 						// 				else
// 						// 					m_bTCantBuy = false;
					}
				}

				// Terrorists WON (if they don't respawn)
				if ( NumAliveCT == 0 && NumDeadCT != 0 && !bCTsRespawn && m_iNumSpawnableTerrorist > 0 )
				{
					if ( !bNeededPlayers )
					{
						m_match.AddTerroristWins( 1 );
					}

					TerminateRoundDelayedTeamCashAwards_t award = { TEAM_TERRORIST,
						( m_bMapHasBombTarget ) ? TeamCashAward::ELIMINATION_BOMB_MAP : TeamCashAward::ELIMINATION_HOSTAGE_MAP_T };

					TerminateRound( mp_round_restart_delay.GetFloat(), Terrorists_Win, &award, 1 );
					return true;
				}
				else if ( mp_use_respawn_waves.GetInt() == 2 && NumAliveCT == 0 && NumDeadCT != 0 && bCTsRespawn )
				{
					// if T's are supposed to respawn
					m_flNextRespawnWave.Set( TEAM_CT, gpGlobals->curtime + GetRespawnWaveMaxLength( TEAM_CT ) );
				}
			}
        }
        else if ( NumAliveCT == 0 && !bCTsRespawn && NumAliveTerrorist == 0 && !bTsRespawn && ( m_iNumTerrorist > 0 || m_iNumCT > 0 ) )
        {
            TerminateRound( mp_round_restart_delay.GetFloat(), Round_Draw );
            return true;
        }

        return false;
    }


    void CCSGameRules::ReadMultiplayCvars()
    {
		float flRoundTime = 0;
		
		if ( IsPlayingClassic() && IsHostageRescueMap() && ( mp_roundtime_hostage.GetFloat() > 0 ) )
		{
			flRoundTime = mp_roundtime_hostage.GetFloat();
		}
		else if ( IsPlayingClassic() && IsBombDefuseMap() && ( mp_roundtime_defuse.GetFloat() > 0 ) )
		{
			flRoundTime = mp_roundtime_defuse.GetFloat();
		}
		else if ( IsPlayingCoopMission() )
		{
			flRoundTime = mp_roundtime_deployment.GetFloat();
		}
		else
		{
			flRoundTime = mp_roundtime.GetFloat();
		}

        m_iRoundTime = IsWarmupPeriod() ? 999 : (int)( flRoundTime * 60 );
        m_iFreezeTime = IsWarmupPeriod() ? 2 : mp_freezetime.GetInt();
    }

	static int BombSortPredicate(CCSPlayer * const *left, CCSPlayer * const *right) 
	{
		// should we prioritize humans over bots?
		if (cv_bot_defer_to_human_items.GetBool() )
		{
			if ( (*left)->IsBot() && !(*right)->IsBot() )
				return 1;

			if ( !(*left)->IsBot() && (*right)->IsBot() )
				return -1;
		}

		if ( (*left)->m_fLastGivenBombTime < (*right)->m_fLastGivenBombTime )
			return -1;

		if ( (*left)->m_fLastGivenBombTime > (*right)->m_fLastGivenBombTime )
			return +1;

		return 0;
	}

	static int SpawnPointSortFunction( SpawnPoint* const *left, SpawnPoint* const *right )
	{
		// Sort 2 spawn points against each other using their priority values
		return ( *left )->m_iPriority - ( *right )->m_iPriority;
	}

	static int CoopEnemySpawnSortFunction( SpawnPoint* const *left, SpawnPoint* const *right )
	{
		if ( !CSGameRules() )
			return 0;

		// Sort the spawn point lists
		// just use the first overlapping nav area as a reasonable approximation
		ShortestPathCost cost = ShortestPathCost();
		float dist_left = NavAreaTravelDistance( TheNavMesh->GetNearestNavArea( ( *left )->GetAbsOrigin() ),
												 TheNavMesh->GetNearestNavArea( CSGameRules()->m_vecMainCTSpawnPos ),
												 cost );

		float dist_right = NavAreaTravelDistance( TheNavMesh->GetNearestNavArea( ( *right )->GetAbsOrigin() ),
												  TheNavMesh->GetNearestNavArea( CSGameRules()->m_vecMainCTSpawnPos ),
												  cost );

		return dist_left - dist_right;
	}

	static int ArmsRaceSpawnPointSortFunction( SpawnPoint* const *left, SpawnPoint* const *right )
	{
		if ( ( *left )->m_nType != SpawnPoint::ArmsRace && ( *right )->m_nType == SpawnPoint::ArmsRace )
			return 1;

		if ( ( *left )->m_nType == SpawnPoint::ArmsRace && ( *right )->m_nType != SpawnPoint::ArmsRace )
			return -1;

		return 0;
	}

	void CCSGameRules::ResetMasterSpawnPointsForCoop( void )
	{
			RefreshCurrentSpawnPointLists();
	}

    // Perform round-related processing at the point when there is less than 1 second of "free play" to go before the round officially ends
    // At this point the round winner has been determined and displayed to the players
    void CCSGameRules::PreRestartRound( void )
    {
        IGameEvent *restartEvent = gameeventmanager->CreateEvent( "cs_pre_restart" );
        gameeventmanager->FireEvent( restartEvent );
        m_bHasTriggeredRoundStartMusic = true;

		if ( IsPlayingCoopMission() )
		{
			// make sure all active spawn points are enabled again
			ResetMasterSpawnPointsForCoop();
		}
		else
		{
			// reshuffle spawns and then sort by priority for the next round.
			ShuffleSpawnPointLists();
			SortSpawnPointLists();
		}
    }

    // Perform round-related processing at the point where the round winner has been determined, and free-play has begun
    void CCSGameRules::RoundWin( TerminateRoundDelayedTeamCashAwards_t *pCashAwards, int numCashAwards )
    {
        // Update accounts based on number of hostages remaining.. 
        int iRescuedHostageBonus = 0;

        for ( int iHostage=0; iHostage < g_Hostages.Count(); iHostage++ )
        {
            CHostage *pHostage = g_Hostages[iHostage];

            if( pHostage->IsRescuable() )	//Alive and not rescued
            {
                iRescuedHostageBonus += TeamCashAwardValue( TeamCashAward::HOSTAGE_ALIVE );
            }
            
            if ( iRescuedHostageBonus >= 2000 )
                break;
        }


		// Super-specific hack for coop. After a team wins, the hardest bot on that team
		// gets easier.
		if ( m_iRoundWinStatus == TEAM_TERRORIST || m_iRoundWinStatus == TEAM_CT )
		{
			CTeam *pTeam = GetGlobalTeam( m_iRoundWinStatus );
			if ( pTeam && IsPlayingCoopGuardian() )
			{
				for ( int j = 0; j < m_nGuardianModeWaveNumber; j++ )
				{
					for ( int iEasier = 0; iEasier < sv_bots_get_easier_each_win.GetInt(); iEasier++ )
					{
						CUtlVector < CCSBot* > vecBotsOnTeam;
						if ( pTeam->GetBotMembers( &vecBotsOnTeam ) )
						{
							CCSBot *pLeadBot = NULL;
							BotDifficultyType botHighDifficulty = BOT_EASY;
							FOR_EACH_VEC( vecBotsOnTeam, iBot )
							{
								const BotProfile *pProfile = vecBotsOnTeam[iBot]->GetProfile();
								if ( pProfile->GetMaxDifficulty() > botHighDifficulty )
								{
									pLeadBot = vecBotsOnTeam[iBot];
									botHighDifficulty = ( BotDifficultyType )pProfile->GetMaxDifficulty();
								}
							}
							if ( pLeadBot )
							{
								// dont go lower than the map set bot difficulty
								ConVarRef bot_difficulty( "bot_difficulty" );
								const BotProfile* pNewProfileData = TheBotProfiles->GetRandomProfile( MAX( ( BotDifficultyType )( botHighDifficulty - 1 ), ( BotDifficultyType )bot_difficulty.GetInt() ), pLeadBot->GetTeamNumber(), WEAPONTYPE_UNKNOWN, true );
								if ( pNewProfileData )
								{
									pLeadBot->Initialize( pNewProfileData, pLeadBot->GetTeamNumber() );
								}
							}
						}
					}
				}

				int nRoundsPlayed = GetTotalRoundsPlayed();
				if ( nRoundsPlayed > 3 )
				{
					int nShouldReport = nRoundsPlayed % 2;

					if ( nShouldReport == 0 )
					{
						int nHumanGuardianTeam = GetCoopHumanTeamNumber();
						if ( m_iRoundWinStatus != nHumanGuardianTeam )
						{	// Only print the message if the humans didn't complete the mission yet
							CBroadcastRecipientFilter filter;
							UTIL_ClientPrintFilter( filter, HUD_PRINTTALK, "#SFUI_Notice_GuardianModeLowerDifficultyNextRound" );
						}

						// ConVarRef sv_bots_get_harder_after_each_wave( "sv_bots_get_harder_after_each_wave" ); // already defined in this file
						sv_bots_get_harder_after_each_wave.SetValue( MAX( 1, sv_bots_get_harder_after_each_wave.GetInt()-1) );

						ConVarRef bot_difficulty( "bot_difficulty" );
						int nBotDifficulty = bot_difficulty.GetInt();
						if ( nRoundsPlayed > 9 )
							nBotDifficulty = 0;
						else if ( nRoundsPlayed > 5 )
							nBotDifficulty = MIN( nBotDifficulty, 1 );
						else if ( nRoundsPlayed > 3 )
							nBotDifficulty = MIN( nBotDifficulty, 2 );

						bot_difficulty.SetValue( MAX( 0, sv_bots_get_harder_after_each_wave.GetInt()-1) );
					}
				}
			}
		}

        //*******Catch up code by SupraFiend. Scale up the loser bonus when teams fall into losing streaks
		int nTeamCashAwardLoserBonus = TeamCashAwardValue( TeamCashAward::LOSER_BONUS );
		int nTeamCashAwardLoserConsecutiveRoundsBonus = TeamCashAwardValue( TeamCashAward::LOSER_BONUS_CONSECUTIVE_ROUNDS );
        if (m_iRoundWinStatus == WINNER_TER) // terrorists won
        {
            m_iNumConsecutiveTerroristLoses = 0;	// reset their streak

			m_iLoserBonus = nTeamCashAwardLoserBonus +
				m_iNumConsecutiveCTLoses * nTeamCashAwardLoserConsecutiveRoundsBonus;

			m_iNumConsecutiveCTLoses = Min( m_iNumConsecutiveCTLoses + 1, mp_consecutive_loss_max.GetInt() );

        }
        else if (m_iRoundWinStatus == WINNER_CT) // CT Won
        {
            m_iNumConsecutiveCTLoses = 0;	// reset their streak
            
			m_iLoserBonus = nTeamCashAwardLoserBonus +
				m_iNumConsecutiveTerroristLoses * nTeamCashAwardLoserConsecutiveRoundsBonus;

			m_iNumConsecutiveTerroristLoses = Min( m_iNumConsecutiveTerroristLoses + 1, mp_consecutive_loss_max.GetInt() );
        }

		// assign the delayed cash awards now that we have adjusted consecutive wins/losses and bonuses
		int nTeamCashAwardWinnersBonus = ( nTeamCashAwardLoserBonus > 0 && nTeamCashAwardLoserConsecutiveRoundsBonus > 0 ) ? TeamCashAwardValue( TeamCashAward::WINNER_BONUS_CONSECUTIVE_ROUNDS ) : 0;
		for ( int k = 0; k < numCashAwards; ++ k )
		{
			int amount = TeamCashAwardValue( pCashAwards[ k ].eReason );
			// Check if the win cash award is taxed/bonused
			switch ( pCashAwards[ k ].eReason )
			{
				case TeamCashAward::TERRORIST_WIN_BOMB					:
				case TeamCashAward::ELIMINATION_HOSTAGE_MAP_T			:
				case TeamCashAward::ELIMINATION_HOSTAGE_MAP_CT			:
				case TeamCashAward::ELIMINATION_BOMB_MAP				:
				case TeamCashAward::WIN_BY_TIME_RUNNING_OUT_HOSTAGE		:
				case TeamCashAward::WIN_BY_TIME_RUNNING_OUT_BOMB		:
				case TeamCashAward::WIN_BY_DEFUSING_BOMB				:
				case TeamCashAward::WIN_BY_HOSTAGE_RESCUE				:
					if ( nTeamCashAwardWinnersBonus != 0 )	// negative value means tax, positive value means bonus
					{	// Compute the tax based on loser bonus, both loser bonus and tax must be enabled
						int numConsecutiveLossesBakedIntoBonus = ( m_iLoserBonus - nTeamCashAwardLoserBonus ) / nTeamCashAwardLoserConsecutiveRoundsBonus;
						amount = Max( 0, amount + numConsecutiveLossesBakedIntoBonus * nTeamCashAwardWinnersBonus );
					}
					break;
			}
			AddTeamAccount( pCashAwards[k].iTeam, pCashAwards[k].eReason, amount );
		}

        // assign the wining and losing bonuses
        if (m_iRoundWinStatus == WINNER_TER) // terrorists won
        {
            AddTeamAccount( TEAM_TERRORIST, TeamCashAward::HOSTAGE_ALIVE, iRescuedHostageBonus );
            AddTeamAccount( TEAM_CT, TeamCashAward::LOSER_BONUS, m_iLoserBonus );
        }
        else if (m_iRoundWinStatus == WINNER_CT) // CT Won
        {
            AddTeamAccount( TEAM_CT, TeamCashAward::HOSTAGE_ALIVE, iRescuedHostageBonus);
			AddTeamAccount( TEAM_TERRORIST, TeamCashAward::LOSER_BONUS, m_iLoserBonus );
        }

        //Update CT account based on number of hostages rescued
        AddTeamAccount( TEAM_CT, TeamCashAward::RESCUED_HOSTAGE, m_iHostagesRescued * TeamCashAwardValue( TeamCashAward::RESCUED_HOSTAGE ));

		// store win reason in match stats

		int nStatToModify = GetCurrentRoundIndexForMatchStats();

		// If we reset the stats at the start of overtime then it's too late to modify the previous period's final round. index will be -1 and we abort.

		if ( nStatToModify >= 0 && ShouldRecordMatchStats( nStatToModify ) )
		{
			int NumDeadCT, NumDeadTerrorist, NumAliveTerrorist, NumAliveCT;
			InitializePlayerCounts( NumAliveTerrorist, NumAliveCT, NumDeadTerrorist, NumDeadCT );

			bool bCTsRespawn = mp_respawn_on_death_ct.GetBool();
			bool bTsRespawn = mp_respawn_on_death_t.GetBool();

			int iNumRescuedToWin = mp_hostages_rescuetowin.GetInt() == 0 ? g_Hostages.Count() : MIN( g_Hostages.Count(), mp_hostages_rescuetowin.GetInt() );

			m_iMatchStats_PlayersAlive_T.GetForModify( nStatToModify ) = NumAliveTerrorist;
			DevMsg( "NumAliveTerrorist( rnd %d ) = %d\n", nStatToModify, NumAliveTerrorist );

			m_iMatchStats_PlayersAlive_CT.GetForModify( nStatToModify ) = NumAliveCT;
			DevMsg( "NumAliveCT( rnd %d ) = %d\n", nStatToModify, NumAliveCT );

			if (m_iRoundWinStatus == WINNER_TER) // terrorists won
			{
				// T_WIN_BOMB
				if ( m_bTargetBombed == true )
				{
					m_iMatchStats_RoundResults.GetForModify( nStatToModify ) = RoundResult::T_WIN_BOMB;
					DevMsg( "Ts won by BOMB\n" );
				}
				// T_WIN_ELIMINATION
				else if ( NumAliveCT == 0 && NumDeadCT != 0 && !bCTsRespawn && m_iNumSpawnableTerrorist > 0 )
				{
					m_iMatchStats_RoundResults.GetForModify( nStatToModify ) = RoundResult::T_WIN_ELIMINATION;
					DevMsg( "Ts won by ELIMINATION\n" );
				}
				// T_WIN_TIME
				else if ( GetRoundRemainingTime() <= 0 )
				{
					m_iMatchStats_RoundResults.GetForModify( nStatToModify ) = RoundResult::T_WIN_TIME;
					DevMsg( "Ts won by TIME\n" );
				}
				else
				{	
					m_iMatchStats_RoundResults.GetForModify( nStatToModify ) = RoundResult::UNKNOWN;
					DevMsg( "Ts won, but the reason is UNKNOWN!!!\n" );
				}
			}
			else if (m_iRoundWinStatus == WINNER_CT) // cts won
			{
				// CT_WIN_DEFUSE
				if ( m_bBombDefused == true )
				{
					m_iMatchStats_RoundResults.GetForModify( nStatToModify ) = RoundResult::CT_WIN_DEFUSE;
					DevMsg( "CTs won by DEFUSE\n" );
				}
				// CT_WIN_ELIMINATION
				else if ( NumAliveTerrorist == 0 && NumDeadTerrorist != 0 && !bTsRespawn && m_iNumSpawnableCT > 0 )
				{
					m_iMatchStats_RoundResults.GetForModify( nStatToModify ) = RoundResult::CT_WIN_ELIMINATION;
					DevMsg( "CTs won by ELIMINATION\n" );
				}
				// CT_WIN_TIME
				else if ( GetRoundRemainingTime() <= 0 )
				{
					m_iMatchStats_RoundResults.GetForModify( nStatToModify ) = RoundResult::CT_WIN_TIME;
					DevMsg( "CTs won by TIME\n" );
				}
				// CT_WIN_RESCUE
				else if ( iNumRescuedToWin > 0 && m_iHostagesRescued >= iNumRescuedToWin )
				{
					m_iMatchStats_RoundResults.GetForModify( nStatToModify ) = RoundResult::CT_WIN_RESCUE;
					DevMsg( "CTs won by RESCUE\n" );
				}
				else
				{	
					m_iMatchStats_RoundResults.GetForModify( nStatToModify ) = RoundResult::UNKNOWN;
					DevMsg( "CTs won, but the reason is UNKNOWN!!!\n" );
				}
			}
			else
			{
				m_iMatchStats_RoundResults.GetForModify( nStatToModify ) = RoundResult::UNKNOWN;
			}
			
		}
    }

    // Perform round-related processing at the official end of round
    void CCSGameRules::RoundEnd( void )
    {
        IGameEvent * event = gameeventmanager->CreateEvent( "round_officially_ended" );
        if( event )
        {
            gameeventmanager->FireEvent( event );
        }
    }

    // Perform round-related processing at the point when the next round is beginning
    void CCSGameRules::RestartRound()
    {
		int nStatToModify = GetCurrentRoundIndexForMatchStats();

		// save off how many players were alive at the start of the new round
		if ( nStatToModify >= 0 && ShouldRecordMatchStats( nStatToModify ) )
		{
			if ( ShouldRecordMatchStats( nStatToModify ) )
			{
				int NumDeadCT, NumDeadTerrorist, NumAliveTerrorist, NumAliveCT;
				InitializePlayerCounts( NumAliveTerrorist, NumAliveCT, NumDeadTerrorist, NumDeadCT );

				m_iMatchStats_PlayersAlive_T.GetForModify( nStatToModify ) = NumAliveTerrorist;
				DevMsg( "NumAliveTerrorist( rnd %d ) = %d\n", nStatToModify, NumAliveTerrorist );

				m_iMatchStats_PlayersAlive_CT.GetForModify( nStatToModify ) = NumAliveCT;
				DevMsg( "NumAliveCT( rnd %d ) = %d\n", nStatToModify, NumAliveCT );

			}
		}

        m_iNextCTSpawnPoint = 0;
        m_iNextTerroristSpawnPoint = 0;

        // fire a round_prestart event
        if( IGameEvent * roundPrestartEvent = gameeventmanager->CreateEvent( "round_prestart" ) )
        {
            gameeventmanager->FireEvent( roundPrestartEvent );
        }
        // FIXME[pmf]: move this up to the pre-restart?
        // [tj] Notify players that the round is about to be reset
        int playerCount = 0;
        for ( int clientIndex = 1; clientIndex <= gpGlobals->maxClients; clientIndex++ )
        {
            CCSPlayer *pPlayer = (CCSPlayer*)UTIL_PlayerByIndex( clientIndex );
            if ( pPlayer )
            {
                playerCount++;
                pPlayer->OnPreResetRound();

            }
        }

		GetGlobalTeam( TEAM_CT )->ResetTeamLeaders();
		GetGlobalTeam( TEAM_TERRORIST )->ResetTeamLeaders();

		// If we are playing team deathmatch, reset state of which team is winning.
		// TODO: Should we reset the match scores here?  Right now TeamDM is always 1 round.
		if ( IsPlayingTeamDM() )
		{
			mTeamDMLastWinningTeamNumber = TEAM_INVALID;
			mTeamDMLastThinkTime = gpGlobals->curtime;
		}

		// If the pre-reset round was causing team changes then make sure we stop silencing them from now on
		m_bForceTeamChangeSilent = false;

		// Also after the round restarts we mark ourselves as no longer loading round backup data
		m_bLoadingRoundBackupData = false;

        // Kick any bots flagged for removal
        for ( int clientIndex = 1; clientIndex <= gpGlobals->maxClients; clientIndex++ )
        {
            CCSPlayer *pPlayer = (CCSPlayer*)UTIL_PlayerByIndex( clientIndex );
            if ( pPlayer && pPlayer->IsBot() && ( pPlayer->GetTeamNumber() == TEAM_SPECTATOR || pPlayer->GetPendingTeamNumber() == TEAM_SPECTATOR ) )
            {
                engine->ServerCommand( UTIL_VarArgs( "kickid %d\n", engine->GetPlayerUserId( pPlayer->edict() ) ) );
            }
        }

        // Perform any queued team moves
        for ( int clientIndex = 1; clientIndex <= gpGlobals->maxClients; clientIndex++ )
        {
            CCSPlayer *pPlayer = (CCSPlayer*)UTIL_PlayerByIndex( clientIndex );
            if ( pPlayer && ( pPlayer->GetPendingTeamNumber() != pPlayer->GetTeamNumber() ) )
            {
                pPlayer->HandleCommand_JoinTeam( pPlayer->GetPendingTeamNumber() ) ;
            }
        }

		// Team pre-round setup. Do this after players have finished switching teams.
		GetGlobalCSTeam( TEAM_CT )->OnRoundPreStart();
		GetGlobalCSTeam( TEAM_TERRORIST )->OnRoundPreStart();
        
        if ( m_endMatchOnRoundReset )
        {
            m_endMatchOnRoundReset = false;
            m_endMatchOnThink = true;
        }

#if defined ( _GAMECONSOLE )
        bool isReallyEndOfRound = false;
        if ((m_iRoundWinStatus == WINNER_TER) || (m_iRoundWinStatus == WINNER_CT))
            isReallyEndOfRound = true;

        if (playerCount > 1 && isReallyEndOfRound)
        {
            IGameEvent * updateMatchStatsEvent = gameeventmanager->CreateEvent( "update_matchmaking_stats" );
            if (updateMatchStatsEvent)
            {
                gameeventmanager->FireEvent( updateMatchStatsEvent);
            }

            IGameEvent * writeProfileEvent = gameeventmanager->CreateEvent( "write_profile_data" );
            if ( writeProfileEvent )
            {
                gameeventmanager->FireEvent( writeProfileEvent );
            }
        }		
#endif

        if ( !IsFinite( gpGlobals->curtime ) )
        {
            Warning( "NaN curtime in RestartRound\n" );
            gpGlobals->curtime = 0.0f;
        }


        // Brock H. - TR - 03/31/09
        // Revert any player controlled bots
#if CS_CONTROLLABLE_BOTS_ENABLED
        RevertBotsFunctor revertBots;
        ForEachPlayer( revertBots );
#endif

        m_iTotalRoundsPlayed++;
        
        //ClearBodyQue();

        // Tabulate the number of players on each team.
        int NumDeadCT, NumDeadTerrorist, NumAliveTerrorist, NumAliveCT;
        InitializePlayerCounts( NumAliveTerrorist, NumAliveCT, NumDeadTerrorist, NumDeadCT );
        
        m_bBombDropped = false;
        m_bBombPlanted = false;
		m_bHasHostageBeenTouched = false;
        
        if ( GetHumanTeam() != TEAM_UNASSIGNED )
        {
            MoveHumansToHumanTeam();
        }


#if !defined ( _GAMECONSOLE )
        ProcessAutoBalance();
#endif

        //If this is the first restart since halftime, do the appropriate bookkeeping.
        bool bClearAccountsAfterHalftime = false;
        if ( IsPlayingGunGameProgressive() )
        {
            ClearGunGameData();
        }
        else if ( m_match.GetPhase() == GAMEPHASE_HALFTIME )
        {
			if ( GetOvertimePlaying() && ( m_match.GetRoundsPlayed() <= ( mp_maxrounds.GetInt() + ( GetOvertimePlaying() - 1 )*mp_overtime_maxrounds.GetInt() ) ) )
			{
				// This is the overtime halftime at the end of a tied regulation time or at the end of a previous overtime that
				// failed to determine the winner, we will not be switching teams at this time and we proceed into first half
				// of next overtime period
				m_match.SetPhase( GAMEPHASE_PLAYING_FIRST_HALF );
			}
			else
			{
				// Regulation halftime or 1st half of overtime finished, swap the CT and T scores so the scoreboard will be correct
				m_match.SwapTeamScores();
				m_match.SetPhase( GAMEPHASE_PLAYING_SECOND_HALF ); 
			}
            
            if ( IsPlayingGunGameTRBomb() )
            {
                ClearGunGameData();
            }
            else
            {
                // Ensure everyone is given only the starting money
                bClearAccountsAfterHalftime = true;
            }

			// Remove all items at halftime or before overtime when teams aren't switching sides
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CCSPlayer *pPlayer = (CCSPlayer*) UTIL_PlayerByIndex( i );
				if ( !pPlayer )
					continue;

				pPlayer->RemoveAllItems( true );
			}
        }
        // Check to see if TR Gun Game match has ended, and player data should be reset
        else if ( m_match.GetPhase() == GAMEPHASE_MATCH_ENDED )
        {
            if ( IsPlayingGunGameTRBomb() )
            {
                ClearGunGameData();
            }
        }

        if ( m_bCompleteReset )
        {
			// reset timeouts
			EndTerroristTimeOut();
			EndCTTimeOut();

			m_nTerroristTimeOuts = mp_team_timeout_max.GetInt();
			m_nCTTimeOuts = mp_team_timeout_max.GetInt();

			m_flTerroristTimeOutRemaining = mp_team_timeout_time.GetInt();
			m_flCTTimeOutRemaining = mp_team_timeout_time.GetInt();

            // bounds check
            if ( mp_timelimit.GetInt() < 0 )
            {
                mp_timelimit.SetValue( 0 );
            }

            if ( m_bScrambleTeamsOnRestart )
            {
                HandleScrambleTeams();
                m_bScrambleTeamsOnRestart = false;

                if ( IsPlayingGunGameTRBomb() )
                {
                    ClearGunGameData();
                }
            }

            if ( m_bSwapTeamsOnRestart )
            {
                HandleSwapTeams();
                m_bSwapTeamsOnRestart = false;
            }


            m_flGameStartTime = gpGlobals->curtime;
            if ( !IsFinite( m_flGameStartTime.Get() ) )
            {
                Warning( "Trying to set a NaN game start time\n" );
                m_flGameStartTime.GetForModify() = 0.0f;
            }
            m_iTotalRoundsPlayed = 0;

            // Reset score info
            m_match.Reset();

			int nStartLossStreak = Min( mp_starting_losses.GetInt(), mp_consecutive_loss_max.GetInt() );
			m_iNumConsecutiveTerroristLoses	= nStartLossStreak;
			m_iNumConsecutiveCTLoses = nStartLossStreak;

            m_iAccountTerrorist = m_iAccountCT = 0; //No extra cash!.

            //We are starting fresh. So it's like no one has ever won or lost.
			m_iLoserBonus					= TeamCashAwardValue( TeamCashAward::LOSER_BONUS ) + 
												( nStartLossStreak * TeamCashAwardValue( TeamCashAward::LOSER_BONUS_CONSECUTIVE_ROUNDS ) ) ;
		}

		if ( m_bCompleteReset ||	// player scores and QMM fully reset when players lost coop mission round!
			IsPlayingCoopMission() )
		{
            // Reset the player stats
            for ( int i = 1; i <= gpGlobals->maxClients; i++ )
            {
                CCSPlayer *pPlayer = CCSPlayer::Instance( i );

                if ( pPlayer && !FNullEnt( pPlayer->edict() ) )
                    pPlayer->Reset( true );
            }

			// For queued matchmaking mode reset KDA/MVPs as well and initialize money for start
			if ( IsQueuedMatchmaking() )
			{
				FOR_EACH_MAP( m_mapQueuedMatchmakingPlayersData, idxQueuedPlayer )
				{
					m_mapQueuedMatchmakingPlayersData.Element( idxQueuedPlayer )->Reset();
					m_mapQueuedMatchmakingPlayersData.Element( idxQueuedPlayer )->m_cash = GetStartMoney();
				}
			}
			else
			{
				m_mapQueuedMatchmakingPlayersData.PurgeAndDeleteElements();
			}

			// Save a zeroeth round backup
			if ( Helper_mp_backup_round_IsEnabled() && !IsWarmupPeriod() )
			{
				SaveRoundDataInformation();
			}
        }

        m_bFreezePeriod = true;
        m_bGameRestart = false;

		UTIL_LogPrintf( "Starting Freeze period\n" );

        ReadMultiplayCvars();

		int iBuyStatus = -1;
		if ( sv_buy_status_override.GetInt() >= 0 )
		{
			iBuyStatus = sv_buy_status_override.GetInt();
		}
		else if ( g_pMapInfo )
		{
			// Check to see if there's a mapping info parameter entity
			iBuyStatus = g_pMapInfo->m_iBuyingStatus;
		}

		if ( iBuyStatus >= 0 )
        {
			switch ( iBuyStatus )
            {
                case 0: 
                    m_bCTCantBuy = false; 
                    m_bTCantBuy = false; 
                    Msg( "EVERYONE CAN BUY!\n" );
                    break;
                
                case 1: 
                    m_bCTCantBuy = false; 
                    m_bTCantBuy = true; 
                    Msg( "Only CT's can buy!!\n" );
                    break;

                case 2: 
                    m_bCTCantBuy = true; 
                    m_bTCantBuy = false; 
                    Msg( "Only T's can buy!!\n" );
                    break;
                
                case 3: 
                    m_bCTCantBuy = true; 
                    m_bTCantBuy = true; 
                    Msg( "No one can buy!!\n" );
                    break;

                default: 
                    m_bCTCantBuy = false; 
                    m_bTCantBuy = false; 
                    break;
            }
        }
        else
        {
            // by default everyone can buy
            m_bCTCantBuy = false; 
            m_bTCantBuy = false; 
        }
        
        // Check to see if this map has a bomb target in it

        if ( gEntList.FindEntityByClassname( NULL, "func_bomb_target" ) )
        {
			// this is a bit hacky, but it makes it so the bomb stuff only shows up on mission 3 of the coop mission
			if ( IsPlayingCoopMission() && mp_anyone_can_pickup_c4.GetBool() == false )
			{
				m_bMapHasBombTarget = false;
				m_bMapHasBombZone = false;
			}
			else
			{
				m_bMapHasBombTarget = true;
				m_bMapHasBombZone = true;
			}
        }
        else if ( gEntList.FindEntityByClassname( NULL, "info_bomb_target" ) )
        {
            m_bMapHasBombTarget		= true;
            m_bMapHasBombZone		= false;
        }
        else
        {
            m_bMapHasBombTarget		= false;
            m_bMapHasBombZone		= false;
        }

        // Check to see if this map has hostage rescue zones
		m_bMapHasRescueZone = IHostageRescueZone::AutoList().Count() > 0;


        // See if the map has func_buyzone entities
        // Used by CBasePlayer::HandleSignals() to support maps without these entities
        
        if ( gEntList.FindEntityByClassname( NULL, "func_buyzone" ) )
            m_bMapHasBuyZone = true;
        else
            m_bMapHasBuyZone = false;


        // GOOSEMAN : See if this map has func_escapezone entities
        if ( gEntList.FindEntityByClassname( NULL, "func_escapezone" ) )
        {
            m_bMapHasEscapeZone = true;
            m_iHaveEscaped = 0;
            m_iNumEscapers = 0; // Will increase this later when we count how many Ts are starting
            if (m_iNumEscapeRounds >= 3)
            {
                SwapAllPlayers();
                m_iNumEscapeRounds = 0;
            }

            m_iNumEscapeRounds++;  // Increment the number of rounds played... After 8 rounds, the players will do a whole sale switch..
        }
        else
            m_bMapHasEscapeZone = false;

        /*
        // Update accounts based on number of hostages remaining.. 
        int iRescuedHostageBonus = 0;

        for ( int iHostage=0; iHostage < g_Hostages.Count(); iHostage++ )
        {
            CHostage *pHostage = g_Hostages[iHostage];

            if( pHostage->IsRescuable() )	//Alive and not rescued
            {
                iRescuedHostageBonus += TeamCashAwardValue( TeamCashAward::HOSTAGE_ALIVE );
            }
            
            if ( iRescuedHostageBonus >= 2000 )
                break;
        }

        // *******Catch up code by SupraFiend. Scale up the loser bonus when teams fall into losing streaks
        if (m_iRoundWinStatus == WINNER_TER) // terrorists won
        {
            //check to see if they just broke a losing streak
            if ( m_iNumConsecutiveTerroristLoses > 0 )
            {
                // reset the loser bonus
                m_iLoserBonus = TeamCashAwardValue( TeamCashAward::LOSER_BONUS );
                m_iNumConsecutiveTerroristLoses = 0;
            }
            m_iNumConsecutiveCTLoses++;//increment the number of wins the CTs have had
        }
        else if (m_iRoundWinStatus == WINNER_CT) // CT Won
        {
            //check to see if they just broke a losing streak
            if ( m_iNumConsecutiveCTLoses > 0 )
            {
                // reset the loser bonus
                m_iLoserBonus = TeamCashAwardValue( TeamCashAward::LOSER_BONUS );
                m_iNumConsecutiveCTLoses = 0;
            }
            m_iNumConsecutiveTerroristLoses++;//increment the number of wins the Terrorists have had
        }

        //check if the losing team is in a losing streak & that the loser bonus hasn't maxed out.
        if((m_iNumConsecutiveTerroristLoses > 1) && (m_iLoserBonus < 3000))
            m_iLoserBonus += TeamCashAwardValue( TeamCashAward::LOSER_BONUS_CONSECUTIVE_ROUNDS );//help out the team in the losing streak
        else
        if((m_iNumConsecutiveCTLoses > 1) && (m_iLoserBonus < 3000))
            m_iLoserBonus += TeamCashAwardValue( TeamCashAward::LOSER_BONUS_CONSECUTIVE_ROUNDS );//help out the team in the losing streak

        // assign the wining and losing bonuses
        if (m_iRoundWinStatus == WINNER_TER) // terrorists won
        {
            AddTeamAccount( TEAM_TERRORIST, TeamCashAward::HOSTAGE_ALIVE, iRescuedHostageBonus );
            AddTeamAccount( TEAM_CT, TeamCashAward::LOSER_BONUS, m_iLoserBonus );
        }
        else if (m_iRoundWinStatus == WINNER_CT) // CT Won
        {
            AddTeamAccount( TEAM_CT, TeamCashAward::HOSTAGE_ALIVE, iRescuedHostageBonus);
            if (m_bMapHasEscapeZone == false)	// only give them the bonus if this isn't an escape map
                AddTeamAccount( TEAM_TERRORIST, TeamCashAward::LOSER_BONUS, m_iLoserBonus);
        }


        //Update CT account based on number of hostages rescued
        AddTeamAccount( TEAM_CT, TeamCashAward::RESCUED_HOSTAGE, m_iHostagesRescued * TeamCashAwardValue( TeamCashAward::RESCUED_HOSTAGE ));
        */

        // Update individual players accounts and respawn players

        //**********new code by SupraFiend
        //##########code changed by MartinO 
        //the round time stamp must be set before players are spawned
        m_fRoundStartTime = gpGlobals->curtime + m_iFreezeTime;

        if ( !IsFinite( m_fRoundStartTime.Get() ) )
        {
            Warning( "Trying to set a NaN round start time\n" );
            m_fRoundStartTime.GetForModify() = 0.0f;
        }

        m_bRoundTimeWarningTriggered = false;
        
		if ( IsPlayingCoopGuardian() )
		{
			if ( CanSpendMoneyInMap() )
				m_flGuardianBuyUntilTime = GetRoundStartTime();
		}

		if ( IsPlayingSurvival() )
		{
			// HACK: Mode-specific UI read these vars, and Survival doesn't want them, so they get stomped
			m_bMapHasBombTarget = false;
			m_bMapHasRescueZone = false;
			m_bMapHasBuyZone = false;
			m_bMapHasEscapeZone = false;
			m_bMapHasBombZone = false;
		}

        //Adrian - No cash for anyone at first rounds! ( well, only the default. )
        // Get the cash bonus awarded for completing the round
        int RoundBonus = m_bCompleteReset ? 0 : mp_afterroundmoney.GetInt();

        for ( int i = 1; i <= gpGlobals->maxClients; i++ )
        {
            CCSPlayer *pPlayer = (CCSPlayer*) UTIL_PlayerByIndex( i );

            if ( !pPlayer )
                continue;

            pPlayer->m_iNumSpawns	= 0;
            pPlayer->m_bTeamChanged	= false;

            // Award between round auto bonuses
            if ( RoundBonus > 0 )
            {
                pPlayer->AddAccount( RoundBonus, false );
            }

            // tricky, make players non solid while moving to their spawn points
            if ( (pPlayer->GetTeamNumber() == TEAM_CT) || (pPlayer->GetTeamNumber() == TEAM_TERRORIST) )
            {
                pPlayer->AddSolidFlags( FSOLID_NOT_SOLID );
            }
        }

        // Respawn entities (glass, doors, etc..). NOTE this also KILLS any map entities that the players happen to be pointing to.
        // For this reason, there is a UpdateMapEntityPointers() call to each player after the restart_round event is issued and processed.
        CleanUpMap();

		if ( IsPlayingCoopMission() )
		{
			m_coopPlayersInDeploymentZone = true;

			// output to the game entity here that says, we completed the room 
			// and open the door to the next room
			CGameCoopMissionManager *pManager = GetCoopMissionManager();
			// we really dont have one
			if ( !pManager )
			{
				DevMsg( "Coop mission map is missing a game_coopmission_manager entity. You can't keep track of completed waves without it!\n" );
			}
			else
			{
				pManager->SetRoundReset();
				// we do it here after we set a bunch or things for the map, 
				// because coop might want a certain map type even if they aren't spawned right now
			}
		}

		// Reduce hostage count to desired number

		int iHostageCount = mp_hostages_max.GetInt();

		if (	( mp_hostages_max.GetInt() != atoi( mp_hostages_max.GetDefault() ) ) &&
				( g_pMapInfo ) &&
				( g_pMapInfo->m_iHostageCount != 0 ) )
		{
			iHostageCount = g_pMapInfo->m_iHostageCount;
		}

		if ( g_Hostages.Count() > iHostageCount )
		{
			CUtlVector< CHostage * > arrCopyOfOriginalHostageIndices;
			arrCopyOfOriginalHostageIndices.AddMultipleToTail( g_Hostages.Count(), g_Hostages.Base() );

			if ( !mp_hostages_spawn_same_every_round.GetBool() )
				m_arrSelectedHostageSpawnIndices.RemoveAll();

			if ( m_arrSelectedHostageSpawnIndices.Count() )
			{
				// We have pre-selected hostage indices, keep only them
				FOR_EACH_VEC_BACK( g_Hostages, idxGlobalHostage )
				{
					if ( m_arrSelectedHostageSpawnIndices.Find( idxGlobalHostage ) != m_arrSelectedHostageSpawnIndices.InvalidIndex() )
						continue;
					CHostage *pHostage = g_Hostages[ idxGlobalHostage ];
					UTIL_Remove( pHostage );
					g_Hostages.Remove( idxGlobalHostage );

				}
			}
			else if ( mp_hostages_spawn_force_positions.GetString()[0] )
			{
				CUtlVector< int > arrBestHostageIdx;
				CUtlVector< char* > tagStrings;
				V_SplitString( mp_hostages_spawn_force_positions.GetString(), ",", tagStrings );
				arrBestHostageIdx.EnsureCapacity( tagStrings.Count() );
				FOR_EACH_VEC( tagStrings, iTagString )
				{
					arrBestHostageIdx.AddToTail( Q_atoi( tagStrings[iTagString] ) );
				}
				tagStrings.PurgeAndDeleteElements();

				// Now we have selected best hostage indices, keep only them
				FOR_EACH_VEC_BACK( g_Hostages, idxGlobalHostage )
				{
					if ( arrBestHostageIdx.Find( idxGlobalHostage ) != arrBestHostageIdx.InvalidIndex() )
						continue;
					CHostage *pHostage = g_Hostages[ idxGlobalHostage ];
					UTIL_Remove( pHostage );
					g_Hostages.Remove( idxGlobalHostage );
				}
			}
			else if ( mp_hostages_spawn_farthest.GetBool() )
			{
				CUtlVector< int > arrBestHostageIdx;
				vec_t bestMetric = 0;
				CUtlVector< int > arrTryHostageIdx;
				for ( int iStartIdx = 0; iStartIdx < mp_hostages_max.GetInt(); ++ iStartIdx )
					arrTryHostageIdx.AddToTail( iStartIdx );
				arrBestHostageIdx.AddMultipleToTail( arrTryHostageIdx.Count(), arrTryHostageIdx.Base() );
				while ( 1 )
				{
					vec_t metricThisCombo = 0;
					for ( int iFirstHostage = 0; iFirstHostage < arrTryHostageIdx.Count(); ++ iFirstHostage )
					{
						for ( int iSecondHostage = iFirstHostage + 1; iSecondHostage < arrTryHostageIdx.Count(); ++ iSecondHostage )
						{
							vec_t len2Dsq = ( g_Hostages[ arrTryHostageIdx[iFirstHostage] ]->GetAbsOrigin() - g_Hostages[ arrTryHostageIdx[iSecondHostage] ]->GetAbsOrigin() ).Length2DSqr();
							metricThisCombo += len2Dsq;
						}
					}

					if ( metricThisCombo > bestMetric )
					{
						arrBestHostageIdx.RemoveAll();
						arrBestHostageIdx.AddMultipleToTail( arrTryHostageIdx.Count(), arrTryHostageIdx.Base() );
						bestMetric = metricThisCombo;
					}

					// Advance to next permutation
					int iAdvanceIdx = 0;
					while ( ( iAdvanceIdx < arrTryHostageIdx.Count() ) && ( arrTryHostageIdx[ arrTryHostageIdx.Count() - 1 - iAdvanceIdx ] >= g_Hostages.Count() - 1 - iAdvanceIdx ) )
						iAdvanceIdx ++;
					if ( iAdvanceIdx >= arrTryHostageIdx.Count() )
						break;	// Cannot set a valid permutation
					// Increment the index 
					arrTryHostageIdx[ arrTryHostageIdx.Count() - 1 - iAdvanceIdx ] ++;
					// Set all the following indices
					for ( int iFollowingIdx = arrTryHostageIdx.Count() - iAdvanceIdx; iFollowingIdx < arrTryHostageIdx.Count(); ++ iFollowingIdx )
						arrTryHostageIdx[iFollowingIdx] = arrTryHostageIdx[ arrTryHostageIdx.Count() - 1 - iAdvanceIdx ] + ( iFollowingIdx - ( arrTryHostageIdx.Count() - iAdvanceIdx ) + 1 );
				}

				// Now we have selected best hostage indices, keep only them
				FOR_EACH_VEC_BACK( g_Hostages, idxGlobalHostage )
				{
					if ( arrBestHostageIdx.Find( idxGlobalHostage ) != arrBestHostageIdx.InvalidIndex() )
						continue;
					CHostage *pHostage = g_Hostages[ idxGlobalHostage ];
					UTIL_Remove( pHostage );
					g_Hostages.Remove( idxGlobalHostage );
				}
			}
			else
			{
				// Enforce spawn exclusion groups
				CUtlVector< CHostage * > arrSelectedSpawns;
				while ( ( arrSelectedSpawns.Count() < mp_hostages_max.GetInt() ) && g_Hostages.Count() )
				{
					uint32 uiTotalSpawnWeightFactor = 0;
					FOR_EACH_VEC( g_Hostages, idxGlobalHostage )
					{
						if ( CHostage *pCheckHostage = g_Hostages[idxGlobalHostage] )
							uiTotalSpawnWeightFactor += pCheckHostage->GetHostageSpawnRandomFactor();
					}
					if ( !uiTotalSpawnWeightFactor )
						break;

					uint32 iKeepHostage = ( uint32 ) RandomInt( 0, uiTotalSpawnWeightFactor - 1 );
					CHostage *pKeepHostage = NULL;
					FOR_EACH_VEC( g_Hostages, idxGlobalHostage )
					{
						if ( CHostage *pCheckHostage = g_Hostages[idxGlobalHostage] )
						{
							uint32 uiThisFactor = pCheckHostage->GetHostageSpawnRandomFactor();
							if ( iKeepHostage < uiThisFactor )
							{
								pKeepHostage = pCheckHostage;
								g_Hostages.Remove( idxGlobalHostage );
								break;
							}
							else
							{
								iKeepHostage -= uiThisFactor;
							}
						}
					}
					if ( !pKeepHostage )
						break;

					uint32 uiHostageSpawnExclusionGroup = pKeepHostage->GetHostageSpawnExclusionGroup();
					arrSelectedSpawns.AddToTail( pKeepHostage );

					if ( uiHostageSpawnExclusionGroup )
					{
						FOR_EACH_VEC_BACK( g_Hostages, idxGlobalHostage )
						{
							CHostage *pCheckHostage = g_Hostages[ idxGlobalHostage ];
							if ( ( pCheckHostage != pKeepHostage ) && !!( pCheckHostage->GetHostageSpawnExclusionGroup() & uiHostageSpawnExclusionGroup ) )
							{	// They share the same exclusion group
								UTIL_Remove( pCheckHostage );
								g_Hostages.Remove( idxGlobalHostage );
							}
						}
					}
				}
				// Remove all the remaining hostages that we didn't pick
				while ( g_Hostages.Count() )
				{
					CHostage *pHostage = g_Hostages.Tail();
					UTIL_Remove( pHostage );
					g_Hostages.RemoveMultipleFromTail( 1 );
				}
				// Add back the hostages that we decided to keep
				g_Hostages.AddMultipleToTail( arrSelectedSpawns.Count(), arrSelectedSpawns.Base() );
			}

			// Keep removing randomly now until we reach needed number of hostages remaining
			while ( g_Hostages.Count() > mp_hostages_max.GetInt() )
			{
				int randHostage = RandomInt( 0, g_Hostages.Count() - 1 );

				CHostage *pHostage = g_Hostages[ randHostage ];
				UTIL_Remove( pHostage );
				g_Hostages.Remove( randHostage );
			}

			// Remember which spots ended up picked, so that players could disable randomization and keep the spots
			if ( !m_arrSelectedHostageSpawnIndices.Count() )
			{
				FOR_EACH_VEC( g_Hostages, iPickedHostage )
				{
					int idxOriginalSpawnPoint = arrCopyOfOriginalHostageIndices.Find( g_Hostages[iPickedHostage] );
					Assert( idxOriginalSpawnPoint != arrCopyOfOriginalHostageIndices.InvalidIndex() );
					m_arrSelectedHostageSpawnIndices.AddToTail( idxOriginalSpawnPoint );
				}
			}

			// Show information about which hostage positions were selected for the round
			CFmtStr fmtHostagePositions;
			fmtHostagePositions.AppendFormat( "Selected %d hostage positions '", g_Hostages.Count() );
			FOR_EACH_VEC( g_Hostages, iPickedHostage )
			{
				int idxOriginalSpawnPoint = arrCopyOfOriginalHostageIndices.Find( g_Hostages[iPickedHostage] );
				Assert( idxOriginalSpawnPoint != arrCopyOfOriginalHostageIndices.InvalidIndex() );
				fmtHostagePositions.AppendFormat( "%d,", idxOriginalSpawnPoint );
			}
			fmtHostagePositions.Access()[ fmtHostagePositions.Length() - 1 ] = '\'';
			fmtHostagePositions.AppendFormat( "\n" );
			ConMsg( "%s", fmtHostagePositions.Access() );
		}

		// this needs to happen before we cleanup the map or we won't have a mission manager!
		if ( IsPlayingCoopMission() )
		{
			m_nGuardianModeWaveNumber = 1;
			m_nGuardianGrenadesToGiveBots = 0;
			m_coopPlayersInDeploymentZone = true;

			m_coopBonusCoinsFound = 0;
			m_coopBonusPistolsOnly = true;
		}

		if ( IsPlayingCoopGuardian() )
		{
			m_nGuardianModeWaveNumber = 1;
			m_nGuardianGrenadesToGiveBots = 0;

			extern ConVar sv_guardian_heavy_count;
			m_nNumHeaviesToSpawn = sv_guardian_heavy_count.GetInt();

			m_nGuardianModeSpecialKillsRemaining = mp_guardian_special_kills_needed.GetInt();
			m_nGuardianModeSpecialWeaponNeeded = 0;

			const char* szWeaponNeeded = mp_guardian_special_weapon_needed.GetString();
			if( strchr(szWeaponNeeded, '%') ) // New 'quest expression' syntax for mp_guardian_special_weapon_needed
			{
				// Get the first required weapon from the list
				const char* firstWeapon = V_strstr( szWeaponNeeded, "%weapon_" );
				if ( firstWeapon )
				{
					firstWeapon++; // skip '%'
					char szWeaponClassName[MAX_PATH];
					V_strcpy_safe( szWeaponClassName, firstWeapon );
					char* wepEnd = strchr( szWeaponClassName, '%' );
					if ( wepEnd != nullptr )
						*wepEnd = '\0';

					CEconItemDefinition* pItemDef = GetItemSchema()->GetItemDefinitionByName( szWeaponClassName );
					if ( pItemDef )
						m_nGuardianModeSpecialWeaponNeeded = pItemDef->GetDefinitionIndex();
				}
			}
			else if ( V_strcmp( szWeaponNeeded, "any" ) == 0 )
			{
				m_nGuardianModeSpecialWeaponNeeded = 0;
			}
			else
			{
				char pszWeaponClassname[MAX_PATH];
				V_sprintf_safe( pszWeaponClassname, "weapon_%s", szWeaponNeeded );
				CEconItemDefinition *pItemDef = GetItemSchema()->GetItemDefinitionByName( pszWeaponClassname );
				if ( pItemDef && pItemDef->GetDefinitionIndex() != 0 )
				{
					m_nGuardianModeSpecialWeaponNeeded = pItemDef->GetDefinitionIndex();
				}
				else
				{
					// REI: This code-path doesn't seem to be used in the latest operation, and I'm not sure this is the behavior we want.
					//      The quest HUD doesn't handle this path, so leave a message in chat for it in case it is used.
					//      But I suggest that maybe in this case we should just fall back on the 'any weapon' code.

					// we didn't find the weapon specified or it was intentially left blank
					// send a message instead that says we need to survive the round
					CBroadcastRecipientFilter filter;
					if ( IsHostageRescueMap() )
						UTIL_ClientPrintFilter( filter, HUD_PRINTTALK, "#SFUI_Notice_GuardianModeSurviveRoundHostage" );
					else
						UTIL_ClientPrintFilter( filter, HUD_PRINTTALK, "#SFUI_Notice_GuardianModeSurviveRound" );
				}
			}
		}

        // [tj] Keep track of number of players per side and if they have the same uniform
        int terroristUniform = -1;
        bool allTerroristsWearingSameUniform = true;
        int numberOfTerrorists = 0;
        int ctUniform = -1;
        bool allCtsWearingSameUniform = true;
        int numberOfCts = 0;

        // Now respawn all players
        CUtlVector< CCSPlayer* > respawningPlayersList;
        for ( int i = 1; i <= gpGlobals->maxClients; i++ )
        {
            CCSPlayer *pPlayer = (CCSPlayer*) UTIL_PlayerByIndex( i );

            if ( !pPlayer )
                continue;

            if ( pPlayer->GetTeamNumber() == TEAM_CT && PlayerModelInfo::GetPtr()->IsCTClass( pPlayer->PlayerClass() ) )
            {
                // [tj] Increment CT count and check CT uniforms.
                numberOfCts++;
                if ( ctUniform == -1 )
                {
                    ctUniform = pPlayer->PlayerClass();
                }
                else if ( pPlayer->PlayerClass() != ctUniform )
                {
                    allCtsWearingSameUniform = false;
                }

                respawningPlayersList.AddToTail( pPlayer );
            }
            else if ( pPlayer->GetTeamNumber() == TEAM_TERRORIST && PlayerModelInfo::GetPtr()->IsTClass( pPlayer->PlayerClass() ) )
            {
                // [tj] Increment terrorist count and check terrorist uniforms
                numberOfTerrorists++;
                if ( terroristUniform == -1 )
                {
                    terroristUniform = pPlayer->PlayerClass();
                }
                else if ( pPlayer->PlayerClass() != terroristUniform )
                {
                    allTerroristsWearingSameUniform = false;
                }

                respawningPlayersList.AddToTail( pPlayer );
            }
            else
            {
                pPlayer->ObserverRoundRespawn();
            }
        }

        // Shuffle spawning players list (this is done to ensure that spawning players don't always spawn in at the same spawn point)
        ShufflePlayerList( respawningPlayersList );

		// [menglish] reset per-round achievement variables for each player
		// -- And everything else in Survival
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CCSPlayer *pPlayer = (CCSPlayer*) UTIL_PlayerByIndex( i );
			if( pPlayer )
			{
				pPlayer->ResetRoundBasedAchievementVariables();

				// in survival a round is a match.
				if ( IsPlayingSurvival() )
				{
					pPlayer->Reset( true );
				}
			}
		}

		// Spawn the players
        for ( int i = 0; i < respawningPlayersList.Count(); ++i )
        {
            respawningPlayersList[ i ]->RoundRespawn();
        }

		// move follower chickens
		CBaseEntity *pNextChicken = NULL;

		while ( ( pNextChicken = gEntList.FindEntityByClassname( pNextChicken, "chicken" ) ) != NULL )
		{
			CChicken * pChicken = dynamic_cast< CChicken* >( pNextChicken );
			if ( pChicken && pChicken->GetLeader( ) )
			{
				if ( TheNavMesh )
				{
					CNavArea *pPlayerNav = TheNavMesh->GetNearestNavArea( pChicken->GetLeader( ) );

					const float tooSmall = 15.0f;

					if ( pPlayerNav && pPlayerNav->GetSizeX() > tooSmall && pPlayerNav->GetSizeY() > tooSmall )
					{
						{
							pChicken->SetAbsOrigin( pPlayerNav->GetRandomPoint() );
						}
					}
				}
			}
		}

		// Mark all QMM records as eligible for receiving money next round
		FOR_EACH_MAP_FAST( m_mapQueuedMatchmakingPlayersData, idxQMMEntry )
		{
			m_mapQueuedMatchmakingPlayersData.Element( idxQMMEntry )->m_bReceiveNoMoneyNextRound = false;
		}


        // [tj] Award same uniform achievement for qualifying teams
        for ( int i = 1; i <= gpGlobals->maxClients; i++ )
        {
            CCSPlayer *pPlayer = (CCSPlayer*) UTIL_PlayerByIndex( i );

            if ( !pPlayer )
                continue;
#if(ALL_WEARING_SAME_UNIFORM_ACHIEVEMENT)
            if ( pPlayer->GetTeamNumber() == TEAM_CT && allCtsWearingSameUniform && numberOfCts >= AchievementConsts::SameUniform_MinPlayers)
            {
                pPlayer->AwardAchievement(CSSameUniform);
            }

            if ( pPlayer->GetTeamNumber() == TEAM_TERRORIST && allTerroristsWearingSameUniform && numberOfTerrorists >= AchievementConsts::SameUniform_MinPlayers)
            {
                pPlayer->AwardAchievement(CSSameUniform);
            }
#endif
        }

        // [pfreese] Reset all round or match stats, depending on type of restart
        if ( m_bCompleteReset )
        {
            CCS_GameStats.ResetAllStats();
            CCS_GameStats.ResetPlayerClassMatchStats();
        }
        else
        {
            CCS_GameStats.ResetRoundStats();
        }

		// reset Match Stats
		if ( m_bCompleteReset )
		{
			ResetMatchStats();

			ResetMatchStatsTotals();


			// Log the match starting event into server log
			UTIL_LogPrintf( "World triggered \"Match_Start\" on \"%s\"\n", STRING( gpGlobals->mapname ) );
			extern void Helper_LogPrintPlayingTeamNames();
			Helper_LogPrintPlayingTeamNames();
		}

        // now run a tkpunish check, after the map has been cleaned up
        for ( int i = 1; i <= gpGlobals->maxClients; i++ )
        {
            CCSPlayer *pPlayer = (CCSPlayer*) UTIL_PlayerByIndex( i );

            if ( !pPlayer )
                continue;

            if ( ( pPlayer->GetTeamNumber() == TEAM_CT && PlayerModelInfo::GetPtr()->IsCTClass( pPlayer->PlayerClass() )) ||
                 ( pPlayer->GetTeamNumber() == TEAM_TERRORIST && PlayerModelInfo::GetPtr()->IsTClass( pPlayer->PlayerClass() ) ) )
            {
                pPlayer->CheckTKPunishment();
            }
        }

        if (m_bMapHasBombTarget == true	)
        {
            if ( !IsPlayingTraining() && !IsWarmupPeriod() && !IsPlayingGunGameProgressive() 
				 && !IsPlayingGunGameDeathmatch() && !CSGameRules()->IsPlayingCoopMission() )
            {
                GiveC4ToRandomPlayer();				// Give C4 to the terrorists

                if ( mp_defuser_allocation.GetInt() == DefuserAllocation::Random )
                    GiveDefuserToRandomPlayer();
            }
        }

        // Reset game variables
        m_flIntermissionStartTime = 0;
        m_flRestartRoundTime = 0.0;
        m_timeUntilNextPhaseStarts = 0.0f;
        m_iAccountTerrorist = m_iAccountCT = 0;
        m_iHostagesRescued = 0;
        m_iHostagesTouched = 0;
		m_flCMMItemDropRevealStartTime = 0;
		m_flCMMItemDropRevealEndTime = 0;

        // [tj] reset flawless and lossless round related flags
        m_bNoTerroristsKilled = true;
        m_bNoCTsKilled = true;
        m_bNoTerroristsDamaged = true;
        m_bNoCTsDamaged = true;
		m_bNoEnemiesKilled = true;
        m_pFirstKill = NULL;
        m_pFirstBlood = NULL;

		m_pMVP = NULL;

        m_bCanDonateWeapons = true;

        // [dwenger] Reset rescue-related achievement values
        m_iHostagesRemaining = 0;
		m_bAnyHostageReached = false;

        m_arrRescuers.RemoveAll();

        m_hostageWasInjured = false;
        m_hostageWasKilled = false;

        m_iRoundWinStatus = WINNER_NONE;
		m_eRoundWinReason = RoundEndReason_StillInProgress;
        m_bTargetBombed = m_bBombDefused = false;
        m_bCompleteReset = false;
        m_flNextHostageAnnouncement = gpGlobals->curtime;

        m_iHostagesRemaining = g_Hostages.Count();

		m_flDMBonusStartTime = gpGlobals->curtime + random->RandomFloat( mp_dm_time_between_bonus_min.GetFloat(), mp_dm_time_between_bonus_max.GetFloat() );
		m_flDMBonusTimeLength = random->RandomFloat( mp_dm_bonus_length_min.GetFloat(), mp_dm_bonus_length_max.GetFloat() );
		m_unDMBonusWeaponLoadoutSlot = PickRandomWeaponForDMBonus();
		m_bDMBonusActive = false;
		m_bIsDroppingItems = false;

        // fire global game event
        if ( IGameEvent* event = gameeventmanager->CreateEvent( "round_start" ) )
        {
            event->SetInt("timelimit", m_iRoundTime );
            event->SetInt("fraglimit", 0 );
            event->SetInt( "priority", 6 ); // round_start
        
            if ( m_bMapHasRescueZone )
            {
                event->SetString("objective","HOSTAGE RESCUE");
            }
            else if ( m_bMapHasEscapeZone )
            {
                event->SetString("objective","PRISON ESCAPE");
            }
            else if ( m_bMapHasBombTarget || m_bMapHasBombZone )
            {
                event->SetString("objective","BOMB TARGET");
            }
            else
            {
                event->SetString("objective","DEATHMATCH");
            }

            gameeventmanager->FireEvent( event );
        }

		// clear out hits data
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CCSPlayer *pPlayer = (CCSPlayer*) UTIL_PlayerByIndex( i );

			if ( pPlayer )
				pPlayer->m_totalHitsOnServer = 0;
		}

        if ( IsWarmupPeriod() )
        {
            if ( IGameEvent* roundAnnounceEvent = gameeventmanager->CreateEvent( "round_announce_warmup" ) )
                gameeventmanager->FireEvent( roundAnnounceEvent );

			m_SurvivalRules.OnWarmupStart();

			#ifndef CLIENT_DLL
			CheckForGiftsLeaderboardUpdate();
			#endif
        }

		#ifndef CLIENT_DLL
		if ( m_match.GetRoundsPlayed() <= 0 )
		{
			CheckForGiftsLeaderboardUpdate();
		}
		#endif

        // We need to reassign the player's pointers to entities that were killed during the map clean up but have been recreated since the round_start event was called.
        for ( int i = 1; i <= gpGlobals->maxClients; i++ )
        {
            CCSPlayer *pPlayer = (CCSPlayer*) UTIL_PlayerByIndex( i );
            if ( !pPlayer )
                continue;
            pPlayer->UpdateMapEntityPointers();
        }

		m_iActiveAssassinationTargetMissionID = 0;

        // [pfreese] I commented out this call to CreateWeaponManager, as the 
        // CGameWeaponManager object doesn't appear to be actually used by the CSS
        // code, and in any case, the weapon manager does not support wildcards in 
        // entity names (as seemingly indicated) below. When the manager fails to 
        // create its factory, it removes itself in any case.
        // CreateWeaponManager( "weapon_*", gpGlobals->maxClients * 2 );

        g_pPlayerResource->UpdatePlayerData();
        g_EntitySpotting->UpdateSpottedEntities();


        if ( bClearAccountsAfterHalftime && IsPlayingClassic() && HasHalfTime() )
        {
            AssignStartingMoneyToAllPlayers();

			int nStartLossStreak = Min( mp_starting_losses.GetInt(), mp_consecutive_loss_max.GetInt() );
			m_iNumConsecutiveTerroristLoses	= nStartLossStreak;
			m_iNumConsecutiveCTLoses = nStartLossStreak;

            m_iLoserBonus = TeamCashAwardValue( TeamCashAward::LOSER_BONUS ) +
				( nStartLossStreak * TeamCashAwardValue( TeamCashAward::LOSER_BONUS_CONSECUTIVE_ROUNDS ) ) ;;
        }	

        if ( !IsPlayingTraining() && !IsPlayingSurvival())
        {
            // should we show an announcement to declare that this round might be the last round?
            if ( IsLastRoundBeforeHalfTime() )
            {
                if ( IGameEvent * event = gameeventmanager->CreateEvent( "round_announce_last_round_half" ) )
                    gameeventmanager->FireEvent( event );
            }
            else if ( IsLastRoundOfMatch() )
            {
                // don't send the final round event if one of the teams just won the round by clinching
				int iNumWinsToClinch = GetNumWinsToClinch();
                if ( m_match.GetCTScore() != iNumWinsToClinch && m_match.GetTerroristScore() != iNumWinsToClinch )
                {
                    if ( IGameEvent * event = gameeventmanager->CreateEvent( "round_announce_final" ) )
                        gameeventmanager->FireEvent( event );
                }
            }
            else if ( IsMatchPoint() )
            {
                if ( IGameEvent * event = gameeventmanager->CreateEvent( "round_announce_match_point" ) )
                    gameeventmanager->FireEvent( event );
            }
        }

		// if a team voted to surrender and it passed at the end of a round and we went into halftime,
		// switch the teams that need to surrender
		if ( m_bSwitchingTeamsAtRoundReset )
		{
			OnTeamsSwappedAtRoundReset();
		}

        m_bSwitchingTeamsAtRoundReset = false;

        // Unfreeze all players now that the round is starting
        UnfreezeAllPlayers();

		m_fAccumulatedRoundOffDamage = 0.0f;

		m_SurvivalRules.OnRoundRestart();


        // Perform round-related processing at the point when the next round has just restarted
        // (This line should be last in this function)
        PostRestartRound();
    }	

    // Perform round-related processing at the point when the next round has just restarted
    void CCSGameRules::PostRestartRound( void )
    {
        if ( m_match.GetRoundsPlayed() < 1 )
        {
            // Ensure all spectating players are in correct mode at the beginning of the match
            for ( int i = 1; i <= MAX_PLAYERS; i++ )
            {
                CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );
                if ( pPlayer && pPlayer->IsObserver() )
                {
                    // Only process players in observer mode
                    int nMode = pPlayer->GetObserverMode();
        
                    if ( nMode != OBS_MODE_CHASE && nMode != OBS_MODE_IN_EYE )
                    {
                        // If the player is not in the chase or in-eye mode then force them to chase mode
                        nMode = OBS_MODE_CHASE;
                    }

                    // Build and send the command to ensure player is in a valid observer mode
                    char szCommand[ 32 ];
                    V_snprintf( szCommand, sizeof( szCommand ), "spec_mode %i", nMode );

                    CCommand cmd;
                    cmd.Tokenize( szCommand, kCommandSrcCode );
                    ClientCommand( pPlayer, cmd );
                }
            }
        }

		// if this is the first round of regulation time or an overtime, reset matchstats.
		if ( GetCurrentRoundIndexForMatchStats() == 0 )
		{
			CSGameRules()->ResetMatchStats();
		}

        IGameEvent * event = gameeventmanager->CreateEvent( "round_poststart" );
        if ( event )
        {
            gameeventmanager->FireEvent( event );
        }

		if ( IsPlayingCoopGuardian() && !IsWarmupPeriod() )
		{
			// Play a radio message from the mission operator at the start of the round
			UTIL_GlobalChatter( "Guardian.RoundStart" );
		}

		CCSPlayerResource *pResource = dynamic_cast< CCSPlayerResource * >( g_pPlayerResource );
		if ( pResource )
		{
			pResource->UpdatePlayerDataRoundStart();
		}

		// a hacky fix for the particle systems that are set to start active, getting out of order events and don't actually start active
		CBaseEntity *pParticleSystem = NULL;
		while ( ( pParticleSystem = gEntList.FindEntityByClassname( pParticleSystem, "info_particle_system" ) ) != NULL )
		{
			CParticleSystem * pParticle = dynamic_cast< CParticleSystem* >( pParticleSystem );
			if ( pParticle && (pParticle->ShouldStartActive() || pParticle->ShouldStartActive()) )
			{
				pParticle->StopParticleSystem();
				pParticle->StartParticleSystem();
			}
		}
	}


    void CCSGameRules::UnfreezeAllPlayers( void )
    {
        for ( int i = 1; i <= MAX_PLAYERS; i++ )
        {
            CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );
            if ( pPlayer )
            {
                pPlayer->RemoveFlag( FL_FROZEN );
            }
        }
    }

	loadout_positions_t CCSGameRules::PickRandomWeaponForDMBonus( void )
	{
		return LOADOUT_POSITION_INVALID;
	}


    void CCSGameRules::AssignStartingMoneyToAllPlayers( void )
    {
        // Loop through all players and give them only the starting money
        for ( int i = 1; i <= gpGlobals->maxClients; i++ )
        {
            CCSPlayer *pPlayer = (CCSPlayer*) UTIL_PlayerByIndex( i );
            if ( !pPlayer )
                continue;

            if ( pPlayer->GetTeamNumber() == TEAM_CT || pPlayer->GetTeamNumber() == TEAM_TERRORIST )
            {
                int amount_to_assign = -pPlayer->GetAccountBalance() + GetStartMoney();

                pPlayer->AddAccount( amount_to_assign, false );
            }
        }
    }

    void CCSGameRules::GiveC4ToRandomPlayer()
    {
        // Don't give C4 if not everyone is in the game, we are going to restart or if the convar says we should not
        bool bNeeded = false;
        NeededPlayersCheck( bNeeded );
        float timeToRestart = GetRoundRestartTime() - gpGlobals->curtime;
        if ( !mp_give_player_c4.GetBool() || timeToRestart > 0.001f  )
        {
            return;
        }

        CUtlVector<CCSPlayer*> candidates;
        candidates.EnsureCapacity(MAX_PLAYERS);

        // add all eligible terrorist candidates to a list
        for ( int i = 1; i <= gpGlobals->maxClients; i++ )
        {
            CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );
            if ( pPlayer && pPlayer->IsAlive() && pPlayer->GetTeamNumber() == TEAM_TERRORIST )
            {
                candidates.AddToTail(pPlayer);
            }
        }

        // randomly shuffle the list; this will keep the selection random in case of ties
        FOR_EACH_VEC(candidates, i)
        {
            V_swap(candidates[i], candidates[random->RandomInt( 0, candidates.Count() - 1)] );
        }

        // now sort the list
        candidates.Sort(BombSortPredicate);

        // give bomb to the first candidate
        if ( candidates.Count() > 0 )
        {
            CCSPlayer *pPlayer = candidates[0];
            Assert( pPlayer && pPlayer->GetTeamNumber() == TEAM_TERRORIST && pPlayer->IsAlive() );

            pPlayer->GiveNamedItem( WEAPON_C4_CLASSNAME );
            pPlayer->SelectItem( "weapon_c4" );
            pPlayer->m_fLastGivenBombTime = gpGlobals->curtime;

            //ClientPrint( pPlayer, HUD_PRINTCENTER, "#SFUI_Notice_Have_Bomb" );
        }

        m_bBombDropped = false;
    }

    static int DefuserSortPredicate(CCSPlayer * const *left, CCSPlayer * const *right) 
    {
        // should we prioritize humans over bots?
        if (cv_bot_defer_to_human_items.GetBool() )
        {
            if ( (*left)->IsBot() && !(*right)->IsBot() )
                return 1;

            if ( !(*left)->IsBot() && (*right)->IsBot() )
                return -1;
        }

        if ( (*left)->m_fLastGivenDefuserTime < (*right)->m_fLastGivenDefuserTime )
            return -1;

        if ( (*left)->m_fLastGivenDefuserTime > (*right)->m_fLastGivenDefuserTime )
            return +1;

        return 0;
    }

    void CCSGameRules::GiveDefuserToRandomPlayer()
    {
        int iDefusersToGive = 2;
        CUtlVector<CCSPlayer*> candidates;
        candidates.EnsureCapacity(MAX_PLAYERS);

        // add all CT candidates to a list
        for ( int i = 1; i <= gpGlobals->maxClients; i++ )
        {
            CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );
            if ( pPlayer && pPlayer->IsAlive() && pPlayer->GetTeamNumber() == TEAM_CT )
            {
                candidates.AddToTail(pPlayer);
            }
        }

        // randomly shuffle the list; this will keep the selection random in case of ties
        FOR_EACH_VEC(candidates, i)
        {
            V_swap(candidates[i], candidates[random->RandomInt( 0, candidates.Count() - 1)] );
        }

        // now sort the shuffled list into subgroups
        candidates.Sort(DefuserSortPredicate);

        // give defusers to the first N candidates
        for ( int i = 0; i < iDefusersToGive && i < candidates.Count(); ++i )
        {
            CCSPlayer *pPlayer = candidates[i];
            Assert( pPlayer && pPlayer->GetTeamNumber() == TEAM_CT && pPlayer->IsAlive() );
            pPlayer->GiveDefuser(false);
            pPlayer->HintMessage( "#Hint_you_have_the_defuser", false, true );
        }
    }

	int CCSGameRules::NumberOfPlayersRequiredToEndWarmupEarly() const
	{
		if ( mp_endwarmup_player_count.GetInt() > 0 )
		{
			return MIN( mp_endwarmup_player_count.GetInt(), GetMaxHumanPlayers() );
		}
		else
		{
			return GetMaxHumanPlayers();
		}
	}


    void CCSGameRules::Think()
    {
#ifndef GAME_DLL
		COMPILE_TIME_ASSERT( false );
#endif
        // NOTE: We are skipping calling CTeamplayRules::Think() and  CMultiplayRules::Think()
        // by calling CGameRules directly.   Be aware of this.
        CGameRules::Think();
// 		if ( m_DeferredCallQueue.Count() > 0 && gpGlobals->curtime > m_flDeferredCallDispatchTime )
// 		{
// 			m_DeferredCallQueue.CallQueued();
// 		}

		//
		// Check if connected players have bans on record
		//
		int nCooldownMode = sv_kick_players_with_cooldown.GetInt();
		if ( ( nCooldownMode <= 0 ) && steamgameserverapicontext && steamgameserverapicontext->SteamGameServer() && steamgameserverapicontext->SteamGameServer()->BSecure() )
			nCooldownMode = 1; // On VAC secure servers enforce global cooldowns
		if ( ( nCooldownMode > 0 ) && CCSGameRules::sm_mapGcBanInformation.Count() )
		{
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer *pBasePlayer = UTIL_PlayerByIndex( i );
				if ( !pBasePlayer )
					continue;

				CSteamID steamID;
				if ( pBasePlayer->GetSteamID( &steamID ) && steamID.IsValid() &&
					steamID.GetAccountID() )
				{
					CCSGameRules::GcBanInformationMap_t::IndexType_t idx = CCSGameRules::sm_mapGcBanInformation.Find( steamID.GetAccountID() );
					if ( idx != CCSGameRules::sm_mapGcBanInformation.InvalidIndex() )
					{
						CCSGameRules::CGcBanInformation_t &banInfo = CCSGameRules::sm_mapGcBanInformation.Element( idx );
						if ((banInfo.m_dblExpiration > Plat_FloatTime()) && !EMsgGCCStrike15_v2_MatchmakingKickBanReason_IsGreen(banInfo.m_uiReason) &&
							( ( nCooldownMode > 1 ) || EMsgGCCStrike15_v2_MatchmakingKickBanReason_IsGlobal( banInfo.m_uiReason ) ) )
						{
							// Kick this guy
							Msg( "Kicking user %s (sv_kick_players_with_cooldown=%d)\n", pBasePlayer->GetPlayerName(), nCooldownMode );

							if ( sv_kick_ban_duration.GetInt() > 0 )
							{
								// don't roll the kick command into this, it will fail on a lan, where kickid will go through
								engine->ServerCommand( CFmtStr( "banid %d %d;", sv_kick_ban_duration.GetInt(), pBasePlayer->GetUserID() ) );
							}
							char const *szReasonForKick = "Player has competitive matchmaking cooldown";
							switch ( banInfo.m_uiReason )
							{
							case k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_OfficialBan:
							case k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_ChallengeNotification:
								szReasonForKick = "Account is Untrusted";
								break;
							case k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_GsltViolation:
							case k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_ConvictedForBehavior:
							case k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_ConvictedForCheating:
								szReasonForKick = "Account is Convicted";
								break;
							case k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_NoUserSession:
								szReasonForKick = INVALID_STEAM_TICKET;
								break;
							}
							engine->ServerCommand( UTIL_VarArgs( "kickid_ex %d %d %s\n", pBasePlayer->GetUserID(), ( g_pGameRules && ((CCSGameRules *) g_pGameRules)->IsPlayingOffline() ) ? 0 : 1,
								szReasonForKick ) );
						}
					}
				}
			}
		}

		extern void ServerThinkReplayUploader();
		ServerThinkReplayUploader();

		if ( IsQueuedMatchmaking() )
		{
			CEngineHltvInfo_t engineHltv;
			if ( engine->GetEngineHltvInfo( engineHltv ) && engineHltv.m_bBroadcastActive && engineHltv.m_bMasterProxy )
			{
				int numCurrentSpectators = engineHltv.m_numClients - engineHltv.m_numProxies + engineHltv.m_numExternalTotalViewers;
				int numCurrentSpectatorsTV = engineHltv.m_numClients - engineHltv.m_numProxies;
				int numCurrentSpectatorsLnk = engineHltv.m_numExternalLinkedViewers;
				
				if ( numCurrentSpectators > int( m_numSpectatorsCountMax ) )
					m_numSpectatorsCountMax = numCurrentSpectators;
				if ( numCurrentSpectatorsTV > int( m_numSpectatorsCountMaxTV ) )
					m_numSpectatorsCountMaxTV = numCurrentSpectatorsTV;
				if ( numCurrentSpectatorsLnk > int( m_numSpectatorsCountMaxLnk ) )
					m_numSpectatorsCountMaxLnk = numCurrentSpectatorsLnk;
			}
		}

		// This fires begin_new_match once when a new match starts... there are other similar game events
		// but they all get fired multiple times between ending and starting a new match. Since we're using
		// this event to restart an OGS session we can't let it spam like that. 
		if ( !m_bHasMatchStarted && // Turned off when we go to intermission, turned on once we fire the even
			m_match.GetPhase() != GAMEPHASE_MATCH_ENDED && // Don't try to run this while we're still in intermission
			!IsWarmupPeriod() && // Don't start the match until the first round after warmup
			m_flRestartRoundTime == 0.0f && // This awkwardly ensures we've done a RestartRound() and are currently still waiting on one
			UTIL_HumansInGame( true, true ) > 0 ) // don't start until there is somebody on a team
		{

			IGameEvent * newMatch = gameeventmanager->CreateEvent( "begin_new_match" );
			if( newMatch )
			{
				gameeventmanager->FireEvent( newMatch );
			}
			m_bHasMatchStarted = true;
			m_fMatchStartTime = gpGlobals->curtime;

			CCSPlayerResource *pResource = dynamic_cast< CCSPlayerResource * >( g_pPlayerResource );
			if ( pResource )
				pResource->ForcePlayersPickColors();
		}

        // Display autobalance messages if necessary
        if ( m_fAutobalanceDisplayTime > 0.0f  && gpGlobals->curtime > m_fAutobalanceDisplayTime )
        {
            if ( m_AutobalanceStatus == AutobalanceStatus::THIS_ROUND )
            {
                UTIL_ClientPrintFilter( m_AutoBalanceTraitors, HUD_PRINTCENTER, "#SFUI_Notice_Player_Balanced" );
                UTIL_ClientPrintFilter( m_AutoBalanceLoyalists, HUD_PRINTCENTER, "#SFUI_Notice_Teams_Balanced" );

                m_AutoBalanceTraitors.RemoveAllRecipients();
                m_AutoBalanceLoyalists.RemoveAllRecipients();
            }
            else if ( m_AutobalanceStatus == AutobalanceStatus::NEXT_ROUND )
            {
                UTIL_ClientPrintAll( HUD_PRINTCENTER,"#SFUI_Notice_Auto_Team_Balance_Next_Round");
            }
            
            m_fAutobalanceDisplayTime = 0.0f;
            m_AutobalanceStatus = AutobalanceStatus::NONE;
        }

        //Update replicated variable for time till next match or half
        if ( m_match.GetPhase() == GAMEPHASE_HALFTIME )
        {
			if ( mp_halftime_pausetimer.GetBool() )
			{
				//Delay m_flRestartRoundTime for as long as we're paused.
				m_flRestartRoundTime += gpGlobals->curtime - m_flLastThinkTime;
			}

			m_timeUntilNextPhaseStarts = m_flRestartRoundTime - gpGlobals->curtime;

			m_bIsDroppingItems = false;
			
			// Can also implement mp_halftime_pausematch here
        }
        else if ( m_match.GetPhase() == GAMEPHASE_MATCH_ENDED )
        {     
			float flIntermissionDuration = IsQueuedMatchmaking() ? MIN( mp_competitive_endofmatch_extra_time.GetFloat(), GetIntermissionDuration() ) : GetIntermissionDuration();

			if ( m_ItemsPtrDroppedDuringMatch.Count() > 0 && m_phaseChangeAnnouncementTime > 0 && gpGlobals->curtime > m_phaseChangeAnnouncementTime )
			{
				// synch up the server's list of items recieved during this match to the ones on every client
				float duration = SendPlayerItemDropsToClient();

				if ( duration > 0.0f )
				{
					IGameEvent * event = gameeventmanager->CreateEvent( "endmatch_cmm_start_reveal_items" );
					if ( event )
					{
						gameeventmanager->FireEvent( event );
					}

					m_flCMMItemDropRevealStartTime = gpGlobals->curtime;
					m_flCMMItemDropRevealEndTime = m_flCMMItemDropRevealStartTime + duration;

					// make sure that the intermission time accounts for the number of items we're giving out		
					if ( m_flIntermissionStartTime )
					{
					//  The original implementation, commented out here, ensured that intermission was long enough to display all of the drops, but didn't necessarily
					// add any time because in the old version revelation happened in parallel. 
					// The change was to add the drop reveal time to the intermission start because the events now happen serially.)
					//
					//m_flIntermissionStartTime = MAX( m_flIntermissionStartTime, m_flCMMItemDropRevealEndTime + 4.0f - flIntermissionDuration );
					//
					//
						m_flIntermissionStartTime = MAX( m_flIntermissionStartTime, m_flCMMItemDropRevealStartTime + duration );
					}
				}
			}

			if (m_bIsDroppingItems && m_flCMMItemDropRevealEndTime < gpGlobals->curtime)

			{
				m_bIsDroppingItems = false;

				CheckSetVoteTime();
			}

			if ( IsPlayingGunGameProgressive() )
            {
                m_timeUntilNextPhaseStarts = (m_flRestartRoundTime + flIntermissionDuration + GetCMMItemDropRevealDuration()) - gpGlobals->curtime;
            }
            else
            {
                m_timeUntilNextPhaseStarts = m_flIntermissionStartTime + flIntermissionDuration - gpGlobals->curtime;
            }

            if ( m_flRestartRoundTime > 0.0f && ( ( m_flRestartRoundTime - 3.0 ) <= gpGlobals->curtime ) && !m_bVoiceWonMatchBragFired )
            {
                // [tj] Inform players that the round is over
				UTIL_RadioFromRandomPlayer( m_match.GetWinningTeam(), "WonRound", "" );
                m_bVoiceWonMatchBragFired = true;
            }
        }
        else 
        {
            m_timeUntilNextPhaseStarts = 0.0f;
            m_bVoiceWonMatchBragFired = false;
			m_bIsDroppingItems = false;
        }

        //Check if it is time to make the phase change announcement
        if ( m_phaseChangeAnnouncementTime > 0 && gpGlobals->curtime > m_phaseChangeAnnouncementTime )
        {
            IGameEvent * event = gameeventmanager->CreateEvent( "announce_phase_end" );
            if ( event )
            {
                gameeventmanager->FireEvent( event );
            }
            m_phaseChangeAnnouncementTime = 0.0f;
        }

        
        //Let all teams process		
        for ( int i = 0; i < GetNumberOfTeams(); i++ )
        {
            GetGlobalTeam( i )->Think();
        }

		// Update Team Clan Names periodically
		if ( m_fNextUpdateTeamClanNamesTime <= gpGlobals->curtime )
		{
			m_fNextUpdateTeamClanNamesTime = gpGlobals->curtime + 2;
			UpdateTeamPredictions();
			UpdateTeamClanNames( TEAM_CT );
			UpdateTeamClanNames( TEAM_TERRORIST );
		}

		m_SurvivalRules.OnEachTickThink();

        ///// Check game rules /////
        if ( CheckGameOver() )
        {
            return;
        }

        // did somebody hit the fraglimit ?
        if ( CheckFragLimit() )
        {
            return;
        }

        //Restart if we were flagged to do so
        if ( m_endMatchOnThink )
        {
            m_endMatchOnThink = false;
            GoToIntermission();
            return;
        }

        if ( !m_bBuyTimeEnded && IsBuyTimeElapsed() )
        {
            m_bBuyTimeEnded = true;
            IGameEvent * event = gameeventmanager->CreateEvent( "buytime_ended" );
            if ( event )
            {
                gameeventmanager->FireEvent( event );
            }
        }

        //Check for clinch
        int iNumWinsToClinch = GetNumWinsToClinch();

        bool bTeamHasClinchedVictory = false;
        bool bMatchHasEnded = false;

        //Check for halftime switching
        if ( m_match.GetPhase() == GAMEPHASE_PLAYING_FIRST_HALF )
        {
            //The number of rounds before halftime depends on the mode and the associated convar
            int numRoundsBeforeHalftime =
				GetOvertimePlaying()
				? ( mp_maxrounds.GetInt() + ( 2*GetOvertimePlaying() - 1 )*( mp_overtime_maxrounds.GetInt() / 2 ) )
				: ( mp_maxrounds.GetInt() / 2 );

            //Finally, check for halftime

			bool bhalftime = false;
			if ( numRoundsBeforeHalftime > 0 )
			{
				if ( m_match.GetRoundsPlayed() >= numRoundsBeforeHalftime )
				{
					bhalftime = true;
				}
			}
			// if maxrounds is 0 then the server is relying on mp_timelimit rather than mp_maxrounds.
			else if ( ( GetMapRemainingTime() <= ( ( mp_timelimit.GetInt() * 60 ) / 2 ) ) && IsRoundOver() )
			{
				bhalftime = true;
			}

			if ( bhalftime )
			{
				m_match.SetPhase( GAMEPHASE_HALFTIME );
                m_phaseChangeAnnouncementTime = gpGlobals->curtime + mp_win_panel_display_time.GetInt();
                m_flRestartRoundTime = gpGlobals->curtime + mp_halftime_duration.GetFloat();
                SwitchTeamsAtRoundReset();
                FreezePlayers();
            }
        }
        //Check for end of half-time match
        else if ( m_match.GetPhase() == GAMEPHASE_PLAYING_SECOND_HALF )
        {
            //Check for clinch
            if ( iNumWinsToClinch > 0 && ( HasHalfTime() && !IsPlayingTraining()) )
            {
                bTeamHasClinchedVictory = ( m_match.GetCTScore() >= iNumWinsToClinch ) || ( m_match.GetTerroristScore() >= iNumWinsToClinch );
            }
            
            //Finally, if there have enough rounds played, end the match
			bool bEndMatch = false;

			int numRoundToEndMatch = mp_maxrounds.GetInt() + GetOvertimePlaying()*mp_overtime_maxrounds.GetInt();
			if ( numRoundToEndMatch > 0 )
			{
				if ( m_match.GetRoundsPlayed() >= numRoundToEndMatch || bTeamHasClinchedVictory )
				{	
					bEndMatch = true;
				}
			}
			else if ( GetMapRemainingTime() <= 0 && IsRoundOver() )
			{
				bEndMatch = true;
			}

			// Check if the match ended in a tie and needs overtime
			if ( bEndMatch && 
				( mp_overtime_force.GetBool() || ( mp_overtime_enable.GetBool() && !bTeamHasClinchedVictory ) ) )
			{
				bEndMatch = false;

				m_match.GoToOvertime( 1 );
				m_match.SetPhase( GAMEPHASE_HALFTIME );
				m_phaseChangeAnnouncementTime = gpGlobals->curtime + mp_win_panel_display_time.GetInt();
				m_flRestartRoundTime = gpGlobals->curtime + mp_halftime_duration.GetFloat();
				// SwitchTeamsAtRoundReset(); -- don't switch teams, only switch at true halftimes
				FreezePlayers();
			}

			if ( bEndMatch )
			{
                m_phaseChangeAnnouncementTime = gpGlobals->curtime + mp_win_panel_display_time.GetInt();
                GoToIntermission();			
                bMatchHasEnded = true;

                if ( bTeamHasClinchedVictory && m_match.GetRoundsPlayed() < numRoundToEndMatch )
                {
                    // Send chat message to let players know why match is ending early
                    CRecipientFilter filter;

                    for ( int i = 1; i <= gpGlobals->maxClients; i++ )
                    {
                        CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );

                        if ( pPlayer && ( pPlayer->GetTeamNumber() == TEAM_SPECTATOR || pPlayer->GetTeamNumber() == TEAM_CT || pPlayer->GetTeamNumber() == TEAM_TERRORIST ) )
                        {
                            filter.AddRecipient( pPlayer );
                        }
                    }

                    filter.MakeReliable();

                    if ( m_match.GetCTScore() > m_match.GetTerroristScore() )
                    {
                        // CTs have clinched the match
                        UTIL_ClientPrintFilter( filter, HUD_PRINTTALK, "#SFUI_Notice_CTs_Clinched_Match" );
                    }
                    else
                    {
                        // Ts have clinched the match
                        UTIL_ClientPrintFilter( filter, HUD_PRINTTALK, "#SFUI_Notice_Ts_Clinched_Match" );
                    }
                }
            }
        }

        //If playing a non-halftime game, check the max rounds
        else if ( m_match.GetPhase() == GAMEPHASE_PLAYING_STANDARD )
        {
			// Check for a clinch

			if ( mp_maxrounds.GetInt() > 0 && !IsPlayingTraining() && MatchCanClinch() )
			{
				bTeamHasClinchedVictory = ( m_match.GetCTScore() >= iNumWinsToClinch ) || ( m_match.GetTerroristScore() >= iNumWinsToClinch );
			}
			
			
			// End the match if ( ( maxrounds are used ) and ( we've reached maxrounds or clinched the game ) ) or ( we've exceeded timelimit )
            if ( mp_maxrounds.GetInt() > 0 && !IsPlayingTraining() )
			{
				if ( m_match.GetRoundsPlayed() >= mp_maxrounds.GetInt() || bTeamHasClinchedVictory )
				{
					bMatchHasEnded = true;
				}
			}
			else if ( GetMapRemainingTime() <= 0 && !IsPlayingTraining() && IsRoundOver() )
			{
				bMatchHasEnded = true;
			}

			if ( bMatchHasEnded && IsPlayingSurvival() && IsRoundOver() && m_match.GetRoundsPlayed() >= mp_maxrounds.GetInt() )
			{
				bMatchHasEnded = m_flRestartRoundTime < gpGlobals->curtime;
			}

			if ( bMatchHasEnded == true )
            {
                m_phaseChangeAnnouncementTime = gpGlobals->curtime + mp_win_panel_display_time.GetInt();
                GoToIntermission();
            }				
        }	

        if ( IsWarmupPeriod() )
        {
			// if all humans are present and warmup time left is greater than mp_warmuptime_all_players_connected, reduce warmup time to mp_warmuptime_all_players_connected
			if ( ( mp_warmuptime_all_players_connected.GetFloat() > 0 )
				&& ( UTIL_HumansInGame( true, false ) >= NumberOfPlayersRequiredToEndWarmupEarly() )
				&& ( GetWarmupPeriodEndTime() - mp_warmuptime_all_players_connected.GetFloat() >= gpGlobals->curtime )
				&& ( mp_warmuptime.GetFloat() > mp_warmuptime_all_players_connected.GetFloat() ) // avoid spamming this code if warmup is paused
				)
			{
				SetWarmupPeriodStartTime( gpGlobals->curtime );
				mp_warmuptime.SetValue( mp_warmuptime_all_players_connected.GetFloat() );
				m_fWarmupPeriodEnd = GetWarmupPeriodEndTime();
				
				// also unpause warmup?
				//mp_warmup_pausetimer.SetValue( 0 );

				// notify players
				CBroadcastRecipientFilter filter;
				UTIL_ClientPrintFilter(filter, HUD_PRINTTALK, "#SFUI_Notice_All_Players_Connected", mp_warmuptime_all_players_connected.GetString());
			}

			if ( IsWarmupPeriodPaused() && ( GetWarmupPeriodEndTime() - 6 >= gpGlobals->curtime) ) // Ignore warmup pause if within 6s of end.
			{
				// push out the timers indefinitely.
				m_fWarmupPeriodStart += gpGlobals->curtime - m_flLastThinkTime;
				m_fWarmupPeriodEnd = GetWarmupPeriodEndTime();

				m_fWarmupNextChatNoticeTime += gpGlobals->curtime - m_flLastThinkTime;
			}
			
			if ( m_fWarmupNextChatNoticeTime < gpGlobals->curtime )
            {
                m_fWarmupNextChatNoticeTime = gpGlobals->curtime + 10;


//					// Commenting this out in an attempt to reduce chat pollution
//
//                 CBroadcastRecipientFilter filter;
// 
// 				if ( ( mp_warmuptime_all_players_connected.GetFloat() > 0 )
// 					&& ( UTIL_HumansInGame( true, false ) < GetMaxHumanPlayers() ) )
// 				{
// 					UTIL_ClientPrintFilter( filter, HUD_PRINTTALK, "#SFUI_Notice_Match_Will_Start_Waiting_Chat" );
// 				}
// 				else
// 				{
// 					UTIL_ClientPrintFilter( filter, HUD_PRINTTALK, "#SFUI_Notice_Match_Will_Start_Chat" );
// 				}

				if ( !m_numGlobalGiftsGiven )
					CheckForGiftsLeaderboardUpdate();
            }
            //bool bIsPlayingProgressive = CSGameRules() && CSGameRules()->IsPlayingGunGameProgressive();
			
			extern ConVar mp_do_warmup_period;

            if ( ( UTIL_HumansInGame( true, true ) > 0 && ( GetWarmupPeriodEndTime() - 5 < gpGlobals->curtime) ) || !mp_do_warmup_period.GetBool() )
            {
				extern ConVar mp_warmup_pausetimer;
				mp_warmup_pausetimer.SetValue( 0 ); // Timer is unpausable within 5 seconds of its end.

				if (GetWarmupPeriodEndTime() <= gpGlobals->curtime)
				{
					// when the warmup period ends, set the round to restart in 3 seconds
					if (!m_bCompleteReset && !m_bGameRestart)
					{
						GetGlobalTeam( TEAM_CT )->ResetTeamLeaders();
						GetGlobalTeam( TEAM_TERRORIST )->ResetTeamLeaders();

						m_flRestartRoundTime = gpGlobals->curtime + g_flWarmupToFreezetimeDelay;/*4.0*/;
						m_bCompleteReset = true;
						m_bGameRestart = true;
						FreezePlayers();

						if ( IsPlayingSurvival() )
						{
							m_SurvivalRules.DoPlayerExitWarmupTransition( g_flWarmupToFreezetimeDelay );
						}

						CCSPlayerResource *pResource = dynamic_cast< CCSPlayerResource * >( g_pPlayerResource );
						if ( pResource )
							pResource->ForcePlayersPickColors();

						{
//					// Commenting this out in an attempt to reduce chat pollution
//
// 							CReliableBroadcastRecipientFilter filter;
// 							UTIL_ClientPrintFilter( filter, HUD_PRINTTALK, "#SFUI_Notice_Warmup_Has_Ended" );

							m_fWarmupNextChatNoticeTime = gpGlobals->curtime + 10;
						}

						{
							CReliableBroadcastRecipientFilter filter;
							CCSUsrMsg_WarmupHasEnded msg;
							SendUserMessage( filter, CS_UM_WarmupHasEnded, msg );
						}
                    }

                    // when the round resets, turn off the warmup period
                    if ( m_flRestartRoundTime <= gpGlobals->curtime )
                    {
                        m_bWarmupPeriod = false;
                    }
                }
            }
        }

		// Check for change in lead
		if ( IsPlayingTeamDM() && gpGlobals->curtime >= mTeamDMLastThinkTime + 0.25f )
		{
			mTeamDMLastThinkTime = gpGlobals->curtime;

			// Check if the lead has changed
			int winningTeam = TEAM_INVALID;

			if ( m_match.GetTerroristScore() > m_match.GetCTScore() )
				winningTeam = TEAM_TERRORIST;
			else if ( m_match.GetCTScore() > m_match.GetTerroristScore() )
				winningTeam = TEAM_CT;

			if ( winningTeam != TEAM_INVALID && winningTeam != mTeamDMLastWinningTeamNumber )
			{
				int losingTeam = mTeamDMLastWinningTeamNumber;
				mTeamDMLastWinningTeamNumber = winningTeam;

				// Make a random player on each team yell about it in the radio
				UTIL_RadioFromRandomPlayer( winningTeam, "Radio.TeamDM.LeadGained", nullptr );
				if ( losingTeam != TEAM_INVALID )
					UTIL_RadioFromRandomPlayer( losingTeam, "Radio.TeamDM.LeadLost", nullptr );

				// Play a sound effect for each player
				for ( int i = 1; i <= MAX_PLAYERS; ++i )
				{
					CCSPlayer* pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );
					if ( !pPlayer )
						continue;

					CRecipientFilter filter;
					filter.MakeReliable();
					filter.AddRecipient( pPlayer );

					int team = pPlayer->GetTeamNumber();
					if ( team == TEAM_INVALID )
					{
						// nothing
					}
					else if ( pPlayer->GetTeamNumber() == winningTeam )
					{
						CBaseEntity::EmitSound( filter, pPlayer->entindex(), "UI.ArmsRace.BecomeMatchLeader" );
					}
					else if ( pPlayer->GetTeamNumber() == losingTeam )
					{
						CBaseEntity::EmitSound( filter, pPlayer->entindex(), "UI.ArmsRace.Demoted" );
					}
				}
			}
		}

        // Check for the end of the round.
        if ( IsFreezePeriod() )
        {
            CheckFreezePeriodExpired();
        }
        else 
        {
            CheckRoundTimeExpired();
        }

        CheckLevelInitialized();

        if ( !m_bRoundTimeWarningTriggered && GetRoundRemainingTime() < ROUND_END_WARNING_TIME && !IsPlayingTraining( ) )
        {
            m_bRoundTimeWarningTriggered = true;
            IGameEvent * event = gameeventmanager->CreateEvent( "round_time_warning" );
            if ( event )
            {
                gameeventmanager->FireEvent( event );
            }
        }

		if ( IsPlayingCoopMission() && 
			 m_flRestartRoundTime > 0.0f && ( ( m_flRestartRoundTime - 0.5 ) <= gpGlobals->curtime ) && 
			 !IsWarmupPeriod() && !m_bHasTriggeredCoopSpawnReset )
		{
			// we have to reset the spawns, BEFORE the PreRestartRound because that's where the spawns are initially set up
			// and we have to let anought time for map logic to set them all up
			CGameCoopMissionManager *pManager = GetCoopMissionManager();
			// we really dont have one
			if ( !pManager )
			{
				DevMsg( "Coop mission map is missing a game_coopmission_manager entity. You can't keep track of completed waves without it!\n" );
			}
			else
			{
				pManager->SetSpawnsReset();
			}

			m_bHasTriggeredCoopSpawnReset = true;
		}

		if (m_flRestartRoundTime > 0.0f && ((m_flRestartRoundTime - 0.3) <= gpGlobals->curtime) && !m_bHasTriggeredRoundStartMusic)
		{
			// Perform round-related processing at the point when there is less than 1 second of "free play" to go before the round officially ends
			// At this point the round winner has been determined and displayed to the players
			PreRestartRound();
		}

        if ( m_flRestartRoundTime > 0.0f && m_flRestartRoundTime <= gpGlobals->curtime )
        {
            if ( IsWarmupPeriod() && m_match.GetPhase() != GAMEPHASE_MATCH_ENDED && GetWarmupPeriodEndTime() <= gpGlobals->curtime && UTIL_HumansInGame( false, true ) && m_flGameStartTime != 0 )
            {
                m_bCompleteReset = true;
                m_flRestartRoundTime = gpGlobals->curtime + 1;
                mp_restartgame.SetValue( 5 );
                
                m_bWarmupPeriod = false;
            }
            else
            {
                bool botSpeaking = false;
                for ( int i=1; i <= gpGlobals->maxClients; ++i )
                {
                    CBasePlayer *player = UTIL_PlayerByIndex( i );
                    if (player == NULL)
                        continue;

                    if (!player->IsBot())
                        continue;

                    CCSBot *bot = dynamic_cast< CCSBot * >(player);
                    if ( !bot )
                        continue;

                    if ( bot->IsUsingVoice() )
                    {
                        if ( gpGlobals->curtime > m_flRestartRoundTime + 10.0f )
                        {
                            Msg( "Ignoring speaking bot %s at round end\n", bot->GetPlayerName() );
                        }
                        else
                        {
                            botSpeaking = true;
                            break;
                        }
                    }
                }

				// restart only if no bots are speaking, and if no people are watching replay. 
				// But still restart even when people are watching replay, if they've been delaying restart for over 10 seconds; we don't want a bug where someone can indefinitely delay a round by watching replays over and over
				float flMaxRoundDelayDueToReplay = spec_replay_round_delay.GetFloat();

				// don't start a new round at the end of the last round of a survival game.
				bool bSurvivalMatchPlayedToMaxRounds = ( IsPlayingSurvival() && IsRoundOver() && m_match.GetRoundsPlayed() >= mp_maxrounds.GetInt() );

				if ( !botSpeaking && ( gpGlobals->curtime > m_flRestartRoundTime + flMaxRoundDelayDueToReplay || !engine->AnyClientsInHltvReplayMode() ) && !bSurvivalMatchPlayedToMaxRounds )
				{
					m_bHasTriggeredRoundStartMusic = false;
					m_bHasTriggeredCoopSpawnReset = false;

					// Don't call RoundEnd() before the first round of a match
					if (m_match.GetRoundsPlayed() > 0)
					{
						if (IsWarmupPeriod() &&
							(GetWarmupPeriodEndTime() <= gpGlobals->curtime) &&
							UTIL_HumansInGame(false, true))
						{
							m_bCompleteReset = true;
							m_flRestartRoundTime = gpGlobals->curtime + 1;
							mp_restartgame.SetValue(5);

							m_bWarmupPeriod = false;
							return;
						}
						else
						{
							// Perform round-related processing at the official end of round
							RoundEnd();
						}
					}

					if (IsPlayingGunGameTRBomb() && m_bGameRestart)
					{
						m_bGameRestart = false;

						// Reset all the gun game demolition mode data
						ClearGunGameData();

						// Destroy primary and secondary weapons, as well as grenades
						for (int i = 1; i <= gpGlobals->maxClients; i++)
						{
							CCSPlayer *pPlayer = (CCSPlayer*)UTIL_PlayerByIndex(i);

							if (pPlayer)
							{
								pPlayer->DestroyWeapons(false);
								pPlayer->GiveDefaultItems();
							}
						}
					}

					if ( IsPlayingCoopMission() )
					{
						// Destroy primary and secondary weapons, as well as grenades
						for ( int i = 1; i <= gpGlobals->maxClients; i++ )
						{
							CCSPlayer *pPlayer = ( CCSPlayer* )UTIL_PlayerByIndex( i );

							if ( pPlayer )
							{
								pPlayer->DestroyWeapons( true );
								pPlayer->GiveDefaultItems();
								pPlayer->ResetAccount();
								pPlayer->AddAccount( mp_startmoney.GetInt(), false );
							}
						}
					}

					IGameEvent * leaderboardEvent = gameeventmanager->CreateEvent("round_end_upload_stats");
					if (leaderboardEvent)
					{
						gameeventmanager->FireEvent(leaderboardEvent);
					}

					// Save the round data information one more time when the round officially ends (overwriting the previously saved file)
					if ( ( ( m_iRoundWinStatus == WINNER_TER ) || ( m_iRoundWinStatus == WINNER_CT ) )
						&& Helper_mp_backup_round_IsEnabled() )
					{
						SaveRoundDataInformation( mp_backup_round_file_last.GetString() );
					}
					else
					{
						mp_backup_round_file_last.SetValue( "" );
					}

                    // Perform round-related processing at the point when the next round is beginning
                    RestartRound();
                }
            }
        }
        
        if ( gpGlobals->curtime > m_tmNextPeriodicThink )
        {
            CheckRestartRound();
			CheckRespawnWaves();
            m_tmNextPeriodicThink = gpGlobals->curtime + 1.0;
		}

        if ( IsPlayingGunGameProgressive() )
        {
            // Perform any queued team moves
            for ( int clientIndex = 1; clientIndex <= gpGlobals->maxClients; clientIndex++ )
            {
                CCSPlayer *pPlayer = (CCSPlayer*)UTIL_PlayerByIndex( clientIndex );
                if ( pPlayer )
                {
                    if ( ( pPlayer->GetTeamNumber() != TEAM_SPECTATOR ) &&
                          ( pPlayer->GetTeamNumber() != TEAM_UNASSIGNED ) &&
                          ( pPlayer->GetPendingTeamNumber() != pPlayer->GetTeamNumber() ) )
                    {
                        pPlayer->HandleCommand_JoinTeam( pPlayer->GetPendingTeamNumber() ) ;
                    }
                }
            }
        }

		if ( IsPlayingCoopGuardian() )
		{
			if ( m_bBombPlanted )
			{
				//AddTeamAccount( TEAM_CT, TeamCashAward::WIN_BY_TIME_RUNNING_OUT_BOMB );
				//TerminateRound( mp_round_restart_delay.GetFloat(), Terrorists_Win );
			}
			else if ( m_flCoopRespawnAndHealTime > -1 && m_flCoopRespawnAndHealTime <= gpGlobals->curtime )
			{
				// HACK
				int nTeam = GetCoopHumanTeamNumber();
				//int nOtherTeam = GetCoopEnemyTeamNumber();

				// give health to all of the players on the other team
				for ( int j = 1; j <= MAX_PLAYERS; j++ )
				{
					CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( j ) );
					if ( pPlayer && pPlayer->GetTeamNumber() == nTeam )
					{
						if ( pPlayer->IsAlive() )
						{
							pPlayer->GiveHealthAndArmorForGuardianMode( true );
						}
						else
						{
							// respawn any dead CTs as well
							if ( pPlayer->IsAlive() == false )
							{
								pPlayer->State_Transition( STATE_GUNGAME_RESPAWN );
							}
						}
					}
				}

				m_flCoopRespawnAndHealTime = -1;
			}
		}

// 		if ( IsPlayingCoopMission() )
// 		{
// 			if ( m_flCoopRespawnAndHealTime > -1 && m_flCoopRespawnAndHealTime <= gpGlobals->curtime )
// 			{
// 				int nTeam = TEAM_CT;
// 				// give health to all of the players on the other team
// 				for ( int j = 1; j <= MAX_PLAYERS; j++ )
// 				{
// 					CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( j ) );
// 					if ( pPlayer && pPlayer->GetTeamNumber() == nTeam )
// 					{
// 						// respawn any dead CTs as well
// 						if ( pPlayer->IsAlive() == false )
// 						{
// 
// 							pPlayer->State_Transition( STATE_GUNGAME_RESPAWN );
// 						}
// 					}
// 				}
// 
// 				m_flCoopRespawnAndHealTime = -1;
// 			}
// 		}

		// Handle deathmatch bonus weapons if they are worth any points.  Can disable 
		if ( ( IsPlayingGunGameDeathmatch() && mp_dm_bonus_percent.GetInt() > 0 )
			|| ( IsPlayingTeamDM() && mp_dm_teammode_bonus_score.GetInt() > 0 )
			|| ( IsPlayingGunGameDeathmatch() && mp_dm_bonusweapon_dogtags.GetInt() > 0 ) )
		{
			if ( m_flDMBonusStartTime != -1 && gpGlobals->curtime >= m_flDMBonusStartTime  )
			{
				if ( m_bDMBonusActive && gpGlobals->curtime > (m_flDMBonusStartTime + m_flDMBonusTimeLength)  )
				{
					// bonus time ended.....
					m_bDMBonusActive = false;
					m_unDMBonusWeaponLoadoutSlot = PickRandomWeaponForDMBonus();
					// pick the new one if we have enough time in the round
					if ( GetRoundRemainingTime() > (mp_dm_time_between_bonus_max.GetFloat() + mp_dm_bonus_length_max.GetFloat()) )
					{
						m_flDMBonusStartTime = gpGlobals->curtime + random->RandomFloat( mp_dm_time_between_bonus_min.GetFloat(), mp_dm_time_between_bonus_max.GetFloat() );
						m_flDMBonusTimeLength = random->RandomFloat( mp_dm_bonus_length_min.GetFloat(), mp_dm_bonus_length_max.GetFloat() );
					}
					else if ( GetRoundRemainingTime() > (mp_dm_time_between_bonus_min.GetFloat() + mp_dm_bonus_length_min.GetFloat()) )
					{
						m_flDMBonusTimeLength = mp_dm_bonus_length_min.GetFloat();
						m_flDMBonusStartTime = gpGlobals->curtime + (GetRoundRemainingTime() - m_flDMBonusTimeLength);					
					}
					else
					{
						// we're not going to hit a bonus, just disable it
						m_flDMBonusStartTime = -1;		
					}
				}

				// BONUS TIME!!!!
				else if ( !m_bDMBonusActive && m_flDMBonusStartTime > 0 )
				{
					m_bDMBonusActive = true;

					// reset bonus dm suicide limiter.
					for ( int i = 1; i <= gpGlobals->maxClients; i++ )
					{
						CCSPlayer *pPlayer = ToCSPlayer(UTIL_PlayerByIndex( i ));

						if( pPlayer )
							pPlayer->m_bHasUsedDMBonusRespawn = false;
					}

					IGameEvent *bonusWeaponEvent = gameeventmanager->CreateEvent( "dm_bonus_weapon_start" );
					if( bonusWeaponEvent )
					{
						bonusWeaponEvent->SetInt("time", (int)m_flDMBonusTimeLength);
						bonusWeaponEvent->SetInt("Pos", m_unDMBonusWeaponLoadoutSlot );
						gameeventmanager->FireEvent(bonusWeaponEvent);
					}
				}
			}
		}

		// Deal damage to players if that setting is enabled
		if ( mp_global_damage_per_second.GetFloat() > 0.0f && !IsFreezePeriod() )
		{
			float fDamageToPlayers = m_fAccumulatedRoundOffDamage;
			fDamageToPlayers += mp_global_damage_per_second.GetFloat() * ( gpGlobals->curtime - m_flLastThinkTime );
			if ( fDamageToPlayers >= 1.0f )
			{
				int nDamage = ( int )fDamageToPlayers;
				fDamageToPlayers -= nDamage;
				Assert( fDamageToPlayers >= 0 && fDamageToPlayers < 1 );

				// deal damage to each player
				for ( int iPlayer = 1; iPlayer <= MAX_PLAYERS; ++iPlayer )
				{
					CCSPlayer* pPlayer = ToCSPlayer( UTIL_PlayerByIndex( iPlayer ) );
					if ( !pPlayer || !pPlayer->IsAlive() || pPlayer->m_iHealth <= 1 )
						continue;

					int newHealth = MAX( pPlayer->m_iHealth - nDamage, 1 );
					pPlayer->m_iHealth = newHealth;
				}
			}
			m_fAccumulatedRoundOffDamage = fDamageToPlayers;
		}

		m_flLastThinkTime = gpGlobals->curtime;
    }

    // The bots do their processing after physics simulation etc so their visibility checks don't recompute
    // bone positions multiple times a frame.
    void CCSGameRules::EndGameFrame( void )
    {
        TheBots->StartFrame();

        BaseClass::EndGameFrame();
    }


void ServerThinkReplayUploader()
{
}


    bool CCSGameRules::CheckGameOver()
    {
        if ( g_fGameOver )   // someone else quit the game already
        {
            // [Forrest] Calling ChangeLevel multiple times was causing IncrementMapCycleIndex
            // to skip over maps in the list.  Avoid this using a technique from CTeamplayRoundBasedRules::Think.

			//
			// In queue matchmaking mode we let users vote for rematch
			//
			if ( IsQueuedMatchmaking() )
			{
				extern ConVar mp_match_restart_delay;
				switch ( m_eQueuedMatchmakingRematchState )
				{
				case k_EQueuedMatchmakingRematchState_MatchInProgress:
					// give the clients several seconds for the scoreboard to be displayed
					if ( gpGlobals->curtime < m_flIntermissionStartTime + mp_match_restart_delay.GetInt() )
						return true;
					if ( m_flCMMItemDropRevealEndTime > 0.0f && gpGlobals->curtime < m_flCMMItemDropRevealEndTime + 4.0f )
						return true;
					// else fallthrough

				case k_EQueuedMatchmakingRematchState_VoteStarting:
					{

#if VALVE_COMPETITIVE_REMATCH_FEATURE_ENABLED
						char const *szCannotRematchExplanation = "#SFUI_vote_failed_rematch_explain";
						static char const * s_pchTournamentServer = CommandLine()->ParmValue( "-tournament", ( char const * ) NULL );
						if ( m_pQueuedMatchmakingReportedRoundStats )
						{
							uint32 numHumansPresent = 0;
							for ( int k = 0; k < m_pQueuedMatchmakingReportedRoundStats->pings().size(); ++ k )
							{
								if ( m_pQueuedMatchmakingReportedRoundStats->pings(k) > 0 )
									++ numHumansPresent;
							}
							if ( numHumansPresent == m_numQueuedMatchmakingAccounts )
								szCannotRematchExplanation = NULL;
						}
						szCannotRematchExplanation = "#SFUI_vote_failed_rematch_mmsvr";
						if ( !s_pchTournamentServer && !szCannotRematchExplanation )
						{
							m_eQueuedMatchmakingRematchState = k_EQueuedMatchmakingRematchState_VoteStarting;
							engine->ServerCommand( "callvote rematch;\n" );
						}
						else
						{
							m_eQueuedMatchmakingRematchState = k_EQueuedMatchmakingRematchState_VoteToRematchFailed;

							if ( !s_pchTournamentServer )
							{
								// Send chat message to let players know why match cannot proceed
								CRecipientFilter filter;

								for ( int i = 1; i <= gpGlobals->maxClients; i++ )
								{
									CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );

									if ( pPlayer && ( pPlayer->GetTeamNumber() == TEAM_SPECTATOR || pPlayer->GetTeamNumber() == TEAM_CT || pPlayer->GetTeamNumber() == TEAM_TERRORIST ) )
									{
										filter.AddRecipient( pPlayer );
									}
								}

								filter.MakeReliable();

								// CTs have surrendered
								UTIL_ClientPrintFilter( filter, HUD_PRINTTALK, szCannotRematchExplanation );
							}
						}
#else  // VALVE_COMPETITIVE_REMATCH_FEATURE_ENABLED
						m_eQueuedMatchmakingRematchState = k_EQueuedMatchmakingRematchState_VoteToRematchFailed;
#endif // VALVE_COMPETITIVE_REMATCH_FEATURE_ENABLED
					}
					return true;

				case k_EQueuedMatchmakingRematchState_VoteToRematchInProgress:
					// waiting for the vote outcome
					return true;

				case k_EQueuedMatchmakingRematchState_VoteToRematch_CT_Surrender:
				case k_EQueuedMatchmakingRematchState_VoteToRematch_T_Surrender:
				case k_EQueuedMatchmakingRematchState_VoteToRematch_Aborted:
					m_eQueuedMatchmakingRematchState = k_EQueuedMatchmakingRematchState_VoteToRematchFailed;
					// fall through:
				case k_EQueuedMatchmakingRematchState_VoteToRematchFailed:
					// This condition will then cause all people to disconnect
					if ( m_flIntermissionStartTime + MIN( mp_competitive_endofmatch_extra_time.GetFloat(), GetIntermissionDuration() ) < gpGlobals->curtime + mp_competitive_endofmatch_extra_time.GetFloat() )
						m_flIntermissionStartTime = ( gpGlobals->curtime + mp_competitive_endofmatch_extra_time.GetFloat() ) - MIN( mp_competitive_endofmatch_extra_time.GetFloat(), GetIntermissionDuration() );

					m_eQueuedMatchmakingRematchState = k_EQueuedMatchmakingRematchState_VoteToRematchFailed_Done;

					{
						CReliableBroadcastRecipientFilter filter;
						CCSUsrMsg_ServerRankRevealAll msg;
						static char const * s_pchTournamentServer = CommandLine()->ParmValue( "-tournament", ( char const * ) NULL );
						if ( s_pchTournamentServer )
							msg.set_seconds_till_shutdown( MAX( 0, mp_competitive_endofmatch_extra_time.GetInt() ) );
						SendUserMessage( filter, CS_UM_ServerRankRevealAll, msg );
					}
					break;

				case k_EQueuedMatchmakingRematchState_VoteToRematchSucceeded:
#if VALVE_COMPETITIVE_REMATCH_FEATURE_ENABLED
					m_eQueuedMatchmakingRematchState = k_EQueuedMatchmakingRematchState_VoteToRematch_Done;
#else  // VALVE_COMPETITIVE_REMATCH_FEATURE_ENABLED
					m_eQueuedMatchmakingRematchState = k_EQueuedMatchmakingRematchState_VoteToRematchFailed;
#endif // VALVE_COMPETITIVE_REMATCH_FEATURE_ENABLED
					break;	// let the normal logic proceed and change level
					
				case k_EQueuedMatchmakingRematchState_VoteToRematch_Done:
					break;	// let the normal logic proceed and change level
				case k_EQueuedMatchmakingRematchState_VoteToRematchFailed_Done:
					if ( IsQueuedMatchmaking() && ( m_eQueuedMatchmakingRematchState == k_EQueuedMatchmakingRematchState_VoteToRematchFailed_Done )
						&& m_flIntermissionStartTime && ( m_flIntermissionStartTime + MIN( mp_competitive_endofmatch_extra_time.GetFloat(), GetIntermissionDuration() ) < gpGlobals->curtime ) )
					{
						// Tell all clients to return to the lobby
						CReliableBroadcastRecipientFilter filter;
						CCSUsrMsg_DisconnectToLobby msg;
						SendUserMessage( filter, CS_UM_DisconnectToLobby, msg );
					}
					break;	// let the normal logic proceed and change level
				}
			}

			// if we're in a "season" map group, let players vote to pick the next map at the end of the match
			if ( IsEndMatchVotingForNextMap() )
			{
				extern ConVar mp_match_restart_delay;
				switch ( m_eEndMatchMapVoteState )
				{
				case k_EEndMatchMapVoteState_MatchInProgress:
					// give the clients several seconds for the scoreboard to be displayed
					if ( gpGlobals->curtime < m_flIntermissionStartTime + 5.0f )
						return true;
					// else fallthrough

				case k_EEndMatchMapVoteState_VoteInProgress:
					{
						// waiting for the vote outcome
						CCSPlayerResource *pResource = dynamic_cast< CCSPlayerResource * >( g_pPlayerResource );
						if ( pResource && pResource->EndMatchNextMapAllVoted() )
						{
							m_eEndMatchMapVoteState = k_EEndMatchMapVoteState_AllPlayersVoted;
						}

						if ( (m_flIntermissionStartTime + GetIntermissionDuration()) < gpGlobals->curtime )
						{
							m_eEndMatchMapVoteState = k_EEndMatchMapVoteState_VoteTimeEnded;
						}

						return true;
					}

				case k_EEndMatchMapVoteState_VoteTimeEnded:
				case k_EEndMatchMapVoteState_AllPlayersVoted:
					{
						// first find out if we have a tie in the vote
						// this is done in three different places
						// TODO: make a function out of this
						typedef int NumberOfVotes_t;
						typedef int VoteOptionIndex_t;
						CUtlMap< VoteOptionIndex_t, NumberOfVotes_t > mapOption2Votes;
						mapOption2Votes.SetLessFunc( DefLessFunc( int ) );

						// ask all players what they voted for and store it
						// someone voted for a map in the mapgroup, see if we've recorded a vote for this map yet
						for ( int i = 1; i <= MAX_PLAYERS; i++ )
						{
							CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );
#if 0 // #ifdef _DEBUG	// cause everybody even bots that didn't vote to random vote in debug
							if ( pPlayer )
							{
								int nVoteNum = pPlayer->GetEndMatchNextMapVote();
								if ( nVoteNum < 0 )
									nVoteNum = RandomInt( 0, MAX_ENDMATCH_VOTE_PANELS - 1 );
#else
							if ( pPlayer && !pPlayer->IsBot() && ( pPlayer->GetTeamNumber() != TEAM_SPECTATOR ) )
							{
								// players store the keybind list index that displays on the client
								int nVoteNum = pPlayer->GetEndMatchNextMapVote();
#endif
								if ( ( nVoteNum < 0 ) || ( nVoteNum >= MAX_ENDMATCH_VOTE_PANELS )
									|| ( m_nEndMatchMapGroupVoteOptions[nVoteNum] < 0 )
									|| ( m_nEndMatchMapGroupVoteTypes[nVoteNum] < 0 ) )
								{
									if ( mp_verbose_changelevel_spew.GetInt() >= 2 )
									{
										Msg( "NEXTLEVELVOTE: player#%d (%s) voted %d which is ignored\n", pPlayer->entindex(), pPlayer->GetPlayerName(), nVoteNum );
									}
									continue;
								}

								if ( mp_verbose_changelevel_spew.GetInt() >= 2 )
								{
									Msg( "NEXTLEVELVOTE: player#%d (%s) voted %d (type=%d, option=%d)\n", pPlayer->entindex(), pPlayer->GetPlayerName(), nVoteNum,
										m_nEndMatchMapGroupVoteTypes[nVoteNum], m_nEndMatchMapGroupVoteOptions[nVoteNum] );
								}

								// store the number of votes sorted by the map index
								// someone voted for a map in the mapgroup, see if we've recorded a vote for this map yet
								CUtlMap< VoteOptionIndex_t, NumberOfVotes_t >::IndexType_t iMapEntry = mapOption2Votes.Find( nVoteNum );
								if ( iMapEntry == mapOption2Votes.InvalidIndex() )
									mapOption2Votes.Insert( nVoteNum, 1 ); // we don't have a vote for this map, so insert one
								else
									mapOption2Votes.Element( iMapEntry ) ++; // someone voted for this map already so increment it by one
							}
						}

						// now takes the votes and do this so we can sort them my the total number of votes for each map in ascending order
						CUtlMap< NumberOfVotes_t, VoteOptionIndex_t > mapVotesInOrder;
						mapVotesInOrder.SetLessFunc( DefLessFunc( int ) );
						FOR_EACH_MAP( mapOption2Votes, itMap2Vote )
						{
							mapVotesInOrder.Insert( mapOption2Votes.Element( itMap2Vote ), mapOption2Votes.Key( itMap2Vote ) );
							if ( mp_verbose_changelevel_spew.GetInt() >= 2 )
							{
								Msg( "NEXTLEVELVOTE: Total %d votes for choice %d\n", mapOption2Votes.Element( itMap2Vote ), mapOption2Votes.Key( itMap2Vote ) );
							}
						}

						// clear the tie votes in our global list
						m_nEndMatchTiedVotes.RemoveAll();
						for ( CUtlMap< NumberOfVotes_t, VoteOptionIndex_t >::IndexType_t idxVotesInOrder = mapVotesInOrder.LastInorder();
							idxVotesInOrder != mapVotesInOrder.InvalidIndex(); idxVotesInOrder = mapVotesInOrder.PrevInorder( idxVotesInOrder ) )
						{
							// compare the first to the last in the list and add any to it that matches because we have a tie
							if ( mapVotesInOrder.Key( idxVotesInOrder ) == mapVotesInOrder.Key( mapVotesInOrder.LastInorder() ) )
							{
								m_nEndMatchTiedVotes.AddToTail( mapVotesInOrder.Element( idxVotesInOrder ) );
								if ( mp_verbose_changelevel_spew.GetInt() >= 2 )
								{
									Msg( "NEXTLEVELVOTE: Considering best choice %d with %d votes\n", mapVotesInOrder.Element( idxVotesInOrder ), mapVotesInOrder.Key( idxVotesInOrder ) );
								}
							}
							else
								break;
						}

						m_nEndMatchMapVoteWinner = -1;
						// if we don't have any ties at all, it means no one voted, so add all of the maps to the "tie" list
						if ( !m_nEndMatchTiedVotes.Count() )
						{
							for ( int iVoteOption = 0; iVoteOption < MAX_ENDMATCH_VOTE_PANELS; ++ iVoteOption )
							{
								if ( m_nEndMatchMapGroupVoteOptions[iVoteOption] >= 0 && m_nEndMatchMapGroupVoteTypes[iVoteOption] >= 0 )
								{
									m_nEndMatchTiedVotes.AddToTail( iVoteOption );
									if ( mp_verbose_changelevel_spew.GetInt() >= 2 )
									{
										Msg( "NEXTLEVELVOTE: No votes considering choice %d (type=%d, option=%d)\n", iVoteOption,
											m_nEndMatchMapGroupVoteTypes[iVoteOption], m_nEndMatchMapGroupVoteOptions[iVoteOption] );
									}
								}
							}
						}
						// if there is only one in the list, it means at least one player votes, but one of the maps is a clear winner
						if ( m_nEndMatchTiedVotes.Count() == 1 )
						{
							m_nEndMatchMapVoteWinner = m_nEndMatchTiedVotes.Head();
							if ( mp_verbose_changelevel_spew.GetInt() >= 2 )
							{
								Msg( "NEXTLEVELVOTE: Absolute winner choice %d\n", m_nEndMatchMapVoteWinner.Get() );
							}
						}

						if ( m_nEndMatchMapVoteWinner == -1 )
						{
							// if we have a tie, goto SelectingWinner and tell the client to do something
							m_flIntermissionStartTime = MAX( m_flIntermissionStartTime, ( (gpGlobals->curtime + 4.0f) - (GetIntermissionDuration()) ) );
							m_eEndMatchMapVoteState = k_EEndMatchMapVoteState_SelectingWinner;

							IGameEvent * event = gameeventmanager->CreateEvent( "endmatch_mapvote_selecting_map" );
							if( event )
							{
								// we want to send all of the "ties" to the client.  We send ten because technically we could have a 10 person server with each player voting for different maps
								event->SetInt( "count", m_nEndMatchTiedVotes.Count() );
								char sz[16];
								for ( int i = 0 ; i < m_nEndMatchTiedVotes.Count() ; ++i) 
								{
									V_sprintf_safe( sz, "slot%d", i+1 );
									event->SetInt( sz, m_nEndMatchTiedVotes[i] );
								}
							
								gameeventmanager->FireEvent( event );
							}
						}
						else
						{
							// else just pick the winner and move on
							m_eEndMatchMapVoteState = k_EEndMatchMapVoteState_SettingNextLevel;
						}
					}
					return true;

				case k_EEndMatchMapVoteState_SelectingWinner:
					{
						if ( m_flIntermissionStartTime + GetIntermissionDuration() < gpGlobals->curtime )
							m_eEndMatchMapVoteState = k_EEndMatchMapVoteState_SettingNextLevel;
					}
					return true;

				case k_EEndMatchMapVoteState_SettingNextLevel:
					{
						m_flIntermissionStartTime = MAX( m_flIntermissionStartTime, ( (gpGlobals->curtime + 5.0f) - GetIntermissionDuration() ) );
						m_eEndMatchMapVoteState = k_EEndMatchMapVoteState_VoteAllDone;


						// we don't have a clear winner, here is where we break the tie randomly
						if ( m_nEndMatchMapVoteWinner == -1 && m_nEndMatchTiedVotes.Count() > 0 )
						{
							// if we have a tie for first place, randomly pick between the ones in the lead
							m_nEndMatchMapVoteWinner = m_nEndMatchTiedVotes[RandomInt( 0, ( m_nEndMatchTiedVotes.Count() - 1 ) )];
							if ( mp_verbose_changelevel_spew.GetInt() >= 2 )
							{
								Msg( "NEXTLEVELVOTE: Randomly picked winner choice=%d from %d tied for nextlevel\n", m_nEndMatchMapVoteWinner.Get(), m_nEndMatchTiedVotes.Count() );
							}
						}

						if ( m_nEndMatchMapVoteWinner < 0 || m_nEndMatchMapVoteWinner >= MAX_ENDMATCH_VOTE_PANELS
							|| m_nEndMatchMapGroupVoteOptions[m_nEndMatchMapVoteWinner] < 0
							|| m_nEndMatchMapGroupVoteTypes[m_nEndMatchMapVoteWinner] < 0 )
							return true;

						// Get the map and mode selected
						int nWinningOption = m_nEndMatchMapGroupVoteOptions[m_nEndMatchMapVoteWinner];
						switch ( m_nEndMatchMapGroupVoteTypes[m_nEndMatchMapVoteWinner] )
						{
						case kEndMatchVoteType_MapIndexInMapGroup:
						{
							if ( !g_pGameTypes )
								return true;

							const char* mapGroupName = gpGlobals->mapGroupName.ToCStr();
							const CUtlStringList* mapsInGroup = g_pGameTypes->GetMapGroupMapList( mapGroupName );
							if ( !mapsInGroup )
								return true;

							int nNumMaps = mapsInGroup->Count();
							if ( nNumMaps <= 0 )
								return true;

							// If we somehow failed here, pick a random map
							if ( nWinningOption < 0 || nWinningOption >= nNumMaps )
								nWinningOption = RandomInt( 0, nNumMaps - 1 );

							const char* internalMapName = mapsInGroup->Element( nWinningOption );
							if ( !internalMapName || !*internalMapName || !V_strcmp( "undefined", internalMapName ) )
								return true;

							engine->ServerCommand( CFmtStr( "nextlevel %s;", internalMapName ) );
							break;
						}
						case kEndMatchVoteType_ChangeSkirmish:
						{
							const CSkirmishModeDefinition* pWinningSkirmishMode = GetItemSchema()->GetSkirmishModeDefinition( nWinningOption );
							if ( !pWinningSkirmishMode )
								return true;

							if ( !g_pGameTypes )
								return true;

							CFmtStr mapGroupName( "mg_skirmish_%s", pWinningSkirmishMode->GetName() );
							if ( !g_pGameTypes->IsValidMapGroupName( mapGroupName ) )
								return true;

							const char* internalMapName = g_pGameTypes->GetRandomMap( mapGroupName );
							if ( !internalMapName )
								return true;

							engine->ServerCommand( CFmtStr( "mapgroup %s;", mapGroupName.Get() ) );
							engine->ServerCommand( CFmtStr( "nextlevel %s;", internalMapName ) );
							engine->ServerCommand( CFmtStr( "nextmode %s;", pWinningSkirmishMode->GetGameMode() ) );
							break;
						}
						default:
							return true;
						}

						extern ConVar nextmap_print_enabled;
						if ( nextmap_print_enabled.GetBool() )
						{
							UTIL_ClientPrintAll( HUD_PRINTTALK, "#game_nextmap", V_GetFileName( nextlevel.GetString() ) );
						}
						return true;
					}

				case k_EEndMatchMapVoteState_VoteAllDone:
					{

					}
					break;

				}
			}

            // check to see if we should change levels now
            if ( m_flIntermissionStartTime && ( m_flIntermissionStartTime + GetIntermissionDuration() < gpGlobals->curtime ) )
            {
                IGameEvent * leaderboardEvent = gameeventmanager->CreateEvent( "round_end_upload_stats" );
                if( leaderboardEvent )
                {
                    gameeventmanager->FireEvent( leaderboardEvent );
                }

                if ( !IsPlayingTraining() )
                {
                    // Perform round-related processing at the official end of round
                    RoundEnd();
                }

                // get the next map name
                char szNextMap[MAX_PATH];
                if ( nextlevel.GetString() && *nextlevel.GetString() && engine->IsMapValid( nextlevel.GetString() ) )
				{
					if ( mp_verbose_changelevel_spew.GetBool() )
					{
						Msg( "CHANGELEVEL: ConVar '%s' is set, next map will be '%s'\n", nextlevel.GetName(), nextlevel.GetString() );
					}

                    Q_strncpy( szNextMap, nextlevel.GetString(), sizeof( szNextMap ) );
				}
                else
				{
                    GetNextLevelName( szNextMap, sizeof(szNextMap) );
				}

                // intermission is over
                // check to see if we should restart or change maps
				// mp_match_end_restart trumps everything and just restarts the current map regardless of next map
				// mp_match_end_changelevel will force a changelevel even if next map matches current map
                // otherwise if the next map is the same one we are on, don't reload it, just restart the map instead
				// if the next map is different then changelevel
				bool bNextLevelDiffers = !!Q_strcmp( szNextMap, STRING( gpGlobals->mapname ) );
				bool bWantsChangeLevel = !mp_match_end_restart.GetBool() && ( mp_match_end_changelevel.GetBool() || bNextLevelDiffers );
				// if we're hosting a coop game on a listen server, go ahead and allow a change level 
				// because otherwise we'll sit in a black screen when the players complete the mission
				if ( bWantsChangeLevel == false && !engine->IsDedicatedServer() && IsPlayingCooperativeGametype() )
				{
					bWantsChangeLevel = true;
				}

				if ( bWantsChangeLevel == false && mp_verbose_changelevel_spew.GetBool() )
				{
					Msg( "CHANGELEVEL: Not changing level, %s is false, %s is %s %s\n", mp_match_end_restart.GetName(), mp_match_end_changelevel.GetName(), mp_match_end_changelevel.GetBool() ? "true" : "false", !bNextLevelDiffers ? "and next is the same" : "" );
				}
#if defined GAME_DLL 
				if ( engine->IsDedicatedServer() && DedicatedServerWorkshop().CurrentLevelNeedsUpdate() )
				{
					bWantsChangeLevel = true;	// if current level is out of date, we need to change level no matter the settings.
				}
#endif 

				if ( IsQueuedMatchmaking() && ( m_eQueuedMatchmakingRematchState == k_EQueuedMatchmakingRematchState_VoteToRematchFailed_Done ) )
				{
					// Tell all clients to return to the lobby
					CReliableBroadcastRecipientFilter filter;
					CCSUsrMsg_DisconnectToLobby msg;
					SendUserMessage( filter, CS_UM_DisconnectToLobby, msg );
				}
                else if ( bWantsChangeLevel )
                {
					ChangeLevel();
                }
                else
                {
                    //Sometimes we don't want the new match the allow players to pick teams, such as in the case of a vote to scramble or swap teams.
                    if ( m_bPickNewTeamsOnReset )
                    {
                        // Clear the players to unassigned teams.
                        for ( int clientIndex = 1; clientIndex <= gpGlobals->maxClients; clientIndex++ )
                        {
                            CCSPlayer *pPlayer = (CCSPlayer*) UTIL_PlayerByIndex( clientIndex );
                            if(pPlayer)
                            {
                                pPlayer->ChangeTeam( TEAM_UNASSIGNED );
                            }
                        }
                    }					

                    // Restart the round with a complete reset that will clear all the match status such as team scores.
                    m_bCompleteReset = true;
					// clear the next level
					nextlevel.SetValue( "" );
					nextmode.SetValue( "" );
                    RestartRound();
                    g_fGameOver = false;

                    // Send an event that clients can key off of to update their UI state.
                    IGameEvent *restartEvent = gameeventmanager->CreateEvent( "cs_match_end_restart" );
                    if( m_bPickNewTeamsOnReset  && restartEvent )
                    {
                        gameeventmanager->FireEvent( restartEvent );
                    }

                    m_bPickNewTeamsOnReset = true;



#if defined ( GAME_DLL )
					// If we're taking this path, tell the server ugc manager to check for updates now and force a real change level should we need one.
					if ( engine->IsDedicatedServer() )
					{
						DedicatedServerWorkshop().CheckIfCurrentLevelNeedsUpdate();
					}
#endif
                }

                // Don't run this code again
                m_flIntermissionStartTime = 0.f;
            }

            return true;
        }

        return false;
    }

    bool CCSGameRules::CheckFragLimit()
    {
        if ( fraglimit.GetInt() <= 0 )
            return false;

        for ( int i = 1; i <= gpGlobals->maxClients; i++ )
        {
            CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );

            if ( pPlayer && pPlayer->FragCount() >= fraglimit.GetInt() )
            {
                const char *teamName = "UNKNOWN";
                if ( pPlayer->GetTeam() )
                {
                    teamName = pPlayer->GetTeam()->GetName();
                }
                UTIL_LogPrintf("\"%s<%i><%s><%s>\" triggered \"Intermission_Kill_Limit\"\n", 
                    pPlayer->GetPlayerName(),
                    pPlayer->GetUserID(),
                    pPlayer->GetNetworkIDString(),
                    teamName
                    );
                GoToIntermission();
                return true;
            }
        }

        return false;
    }



    void CCSGameRules::CheckFreezePeriodExpired()
    {
        float startTime = m_fRoundStartTime;
        if ( !IsFinite( startTime ) )
        {
            Warning( "Infinite round start time!\n" );
            m_fRoundStartTime.GetForModify() = gpGlobals->curtime;
        }


        float flStartTime = m_fRoundStartTime;

        if( IsFinite( startTime ) && ( gpGlobals->curtime < flStartTime ) )
        {

			if ( CSGameRules() && CSGameRules()->IsMatchWaitingForResume() )
			{
				m_fRoundStartTime = gpGlobals->curtime + m_iFreezeTime;
			}

			// TIMEOUTS
			if ( m_bTerroristTimeOutActive )
			{
				m_fRoundStartTime = gpGlobals->curtime + m_iFreezeTime;

				m_flTerroristTimeOutRemaining -= ( gpGlobals->curtime - m_flLastThinkTime );

				if ( m_flTerroristTimeOutRemaining <= 0 )
				{
					EndTerroristTimeOut();
				}
			}
			else if ( m_bCTTimeOutActive )
			{
				m_fRoundStartTime = gpGlobals->curtime + m_iFreezeTime;

				m_flCTTimeOutRemaining -= ( gpGlobals->curtime - m_flLastThinkTime );

				if ( m_flCTTimeOutRemaining <= 0 )
				{
					EndCTTimeOut();
				}
			}


#ifndef CLIENT_DLL
			else 
			{
				int nTimeToStart = (int) ((flStartTime - gpGlobals->curtime) + 1.0);
				if( nTimeToStart <= 3 && m_nLastFreezeEndBeep != nTimeToStart )
				{
					m_nLastFreezeEndBeep = nTimeToStart;
					IGameEvent *pRoundStartBeepEvent = gameeventmanager->CreateEvent( "cs_round_start_beep" );
					if( pRoundStartBeepEvent )
					{
						gameeventmanager->FireEvent( pRoundStartBeepEvent );
					}
				}
			}
#endif

			return; // not time yet to start round

        }


		


#ifndef CLIENT_DLL
        if( IGameEvent *pRoundStartBeepEvent = gameeventmanager->CreateEvent( "cs_round_final_beep" ) )
        {
            gameeventmanager->FireEvent( pRoundStartBeepEvent );
        }

		// Mark all alive players as receiving money, they could join/reconnect at freezetime
		for ( int iAlivePlayer = 1; iAlivePlayer <= MAX_PLAYERS; ++ iAlivePlayer )
		{
			CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( iAlivePlayer ) );
			
			if ( pPlayer )	// mark all players to have all cash available
				pPlayer->m_iAccountMoneyEarnedForNextRound = 0;

			if ( pPlayer && pPlayer->IsConnected() && !pPlayer->IsBot() && pPlayer->IsAlive() && !pPlayer->IsControllingBot() )
			{
				switch ( pPlayer->GetTeamNumber() )
				{
				case TEAM_TERRORIST:
				case TEAM_CT:
					pPlayer->MarkAsNotReceivingMoneyNextRound( true ); // allow money next round
					break;
				}
			}
		}
#endif

        // Log this information
        UTIL_LogPrintf("World triggered \"Round_Start\"\n");

        char CT_sentence[40];
        char T_sentence[40];
        
        switch ( random->RandomInt( 0, 3 ) )
        {
        case 0:
            Q_strncpy(CT_sentence,"radio.moveout", sizeof( CT_sentence ) ); 
            Q_strncpy(T_sentence ,"radio.moveout", sizeof( T_sentence ) ); 
            break;

        case 1:
            Q_strncpy(CT_sentence, "radio.letsgo", sizeof( CT_sentence ) ); 
            Q_strncpy(T_sentence , "radio.letsgo", sizeof( T_sentence ) ); 
            break;

        case 2:
            Q_strncpy(CT_sentence , "radio.locknload", sizeof( CT_sentence ) );
            Q_strncpy(T_sentence , "radio.locknload", sizeof( T_sentence ) );
            break;

        default:
            Q_strncpy(CT_sentence , "radio.go", sizeof( CT_sentence ) );
            Q_strncpy(T_sentence , "radio.go", sizeof( T_sentence ) );
            break;
        }

        // More specific radio commands for the new scenarios : Prison & Assasination
        if (m_bMapHasEscapeZone == TRUE)
        {
            Q_strncpy(CT_sentence , "radio.elim", sizeof( CT_sentence ) );
            Q_strncpy(T_sentence , "radio.getout", sizeof( T_sentence ) );
        }

        // Freeze period expired: kill the flag
        m_bFreezePeriod = false;

        if ( IGameEvent * event = gameeventmanager->CreateEvent( "round_freeze_end" ) )
        {
            gameeventmanager->FireEvent( event );
        }

        if ( m_match.GetRoundsPlayed() == 0 && !IsWarmupPeriod() )
        {
            if ( IGameEvent * event = gameeventmanager->CreateEvent( "round_announce_match_start" ) )
                gameeventmanager->FireEvent( event );
        }

        if ( !IsPlayingGunGameTRBomb() || ( IsPlayingGunGameTRBomb() && m_match.GetPhase() != GAMEPHASE_MATCH_ENDED ) )
        {
            // Update the timers for all clients and play a sound
            bool bCTPlayed = false;
            bool bTPlayed = false;

			// Don't play start round VO for terrorists in coop
			if ( IsPlayingCoopMission() )
				bTPlayed = true;

			// We're going to play a global chat instead, so skip the 'go go go' here
			if ( IsPlayingCoopGuardian() )
			{
				bCTPlayed = true;
				bTPlayed = true;
			}

			if ( !bCTPlayed )
				UTIL_RadioFromRandomPlayer( TEAM_CT, CT_sentence, nullptr );
			if ( !bTPlayed )
				UTIL_RadioFromRandomPlayer( TEAM_TERRORIST, T_sentence, nullptr );

			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
            {
                CCSPlayer *pPlayer = CCSPlayer::Instance( i );
                if ( pPlayer && !FNullEnt( pPlayer->edict() ) )
                {
                    if ( pPlayer->State_Get() == STATE_ACTIVE )
                    {
						pPlayer->UpdateFreezetimeEndEquipmentValue();
                    }
                
                    //pPlayer->SyncRoundTimer();
                }
            }
        }

		if ( mp_use_respawn_waves.GetInt() == 2 && IsPlayingCoopGuardian() && !IsWarmupPeriod() )
		{
			int nTeam = GetCoopHumanTeamNumber();
			//int nOtherTeam = GetCoopEnemyTeamNumber();

			// Remind the players of their objective
			for ( int i = 1; i <= MAX_PLAYERS; i++ )
			{
				CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );
				if ( pPlayer && pPlayer->GetTeamNumber() == nTeam )
				{
					CRecipientFilter filter;
					filter.AddRecipient( pPlayer );

					const char *szTeam = (nTeam == TEAM_CT) ? "CT" : "T";
					char szNotice[512];
					Q_snprintf( szNotice, sizeof( szNotice ), "#SFUI_Notice_NewWaveBegun_%s0", szTeam );

					char szWave[32];
					Q_snprintf( szWave, sizeof( szWave ), "%d", ( int )m_nGuardianModeWaveNumber );
					UTIL_ClientPrintFilter( filter, HUD_PRINTCENTER, szNotice, szWave );
				}
			}
		}

		m_SurvivalRules.OnFreezePeriodExpired();
    }


    void CCSGameRules::CheckRoundTimeExpired()
    {
        if ( mp_ignore_round_win_conditions.GetBool() || IsWarmupPeriod() )
            return;

        if ( GetRoundRemainingTime() > 0 || m_iRoundWinStatus != WINNER_NONE ) 
            return; //We haven't completed other objectives, so go for this!.

        if( !m_bFirstConnected )
            return;

        // New code to get rid of round draws!!
		if ( IsPlayingSurvival() )
		{
			// survival games continue past the time limit and wait for players to either die or kill each other
			return;
		}
		else if ( IsPlayingTeamDM() )
		{
			if ( m_match.GetTerroristScore() > m_match.GetCTScore() )
				TerminateRound( mp_round_restart_delay.GetFloat(), Terrorists_Win );
			else if ( m_match.GetCTScore() > m_match.GetTerroristScore() )
				TerminateRound( mp_round_restart_delay.GetFloat(), CTs_Win );
			else
				TerminateRound( mp_round_restart_delay.GetFloat(), Round_Draw );
		}
		else if ( IsPlayingGunGameDeathmatch() )
		{
			// TODO: make this a shared function so hud playercount runs the same code
			CCSPlayer *pWinner = NULL;
			for ( int i = 1; i <= MAX_PLAYERS; i++ )
			{
				CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );
				if ( pPlayer )
				{
					if ( pWinner == NULL )
						pWinner = pPlayer; 

					if ( pWinner != pPlayer ) 
					{
						if ( pWinner->GetScore() > pPlayer->GetScore() )
							continue;
						else if ( pWinner->GetScore() < pPlayer->GetScore() )
							pWinner = pPlayer;
						else
							pWinner = (pWinner->entindex() > pPlayer->entindex()) ? pWinner : pPlayer;
					}
				}
			}

			if ( pWinner )
			{
				if ( pWinner->GetTeamNumber() == TEAM_CT )
					TerminateRound( mp_round_restart_delay.GetFloat(), CTs_Win );
				else
					TerminateRound( mp_round_restart_delay.GetFloat(), Terrorists_Win );
			}
			else
			{
				TerminateRound( mp_round_restart_delay.GetFloat(), Round_Draw );
			}
		}
		else if ( IsPlayingCoopMission() )
		{
			// When playing coop mission Terrorists win if timer runs out
			bool bCTsFailedToDeploy = m_coopPlayersInDeploymentZone;
			// However if CTs even failed to deploy in time kick them from the server
			m_match.AddTerroristWins( 1 );
			if ( bCTsFailedToDeploy )
			{
				if ( CTeam *pTeam = GetGlobalTeam( TEAM_CT ) )
					pTeam->MarkSurrendered();

				CReliableBroadcastRecipientFilter filter;
				UTIL_ClientPrintFilter(filter, HUD_PRINTCENTER, "#SFUIHUD_InfoPanel_Coop_DeployMissionBust" );
				UTIL_ClientPrintAll( HUD_PRINTTALK, "#SFUIHUD_InfoPanel_Coop_DeployMissionBust" );
			}
			
			TerminateRound( mp_round_restart_delay.GetFloat(), Terrorists_Win );

			if ( bCTsFailedToDeploy )
			{	// Mission is a bust
				m_match.SetPhase( GAMEPHASE_MATCH_ENDED );
				m_phaseChangeAnnouncementTime = gpGlobals->curtime + 1.5;
				GoToIntermission();
			}
		}
		else if ( mp_default_team_winner_no_objective.GetInt() != -1 )
		{
			int nTeam = mp_default_team_winner_no_objective.GetInt();

			if ( nTeam == TEAM_CT )
			{
				m_match.AddCTWins( 1 );
				TerminateRoundDelayedTeamCashAwards_t award = { TEAM_CT, TeamCashAward::WIN_BY_TIME_RUNNING_OUT_BOMB };
				int numCashAwards = 0;
				if ( IsPlayingCoopGuardian() && GetCoopHumanTeamNumber() == TEAM_CT )
				{
					if ( CTeam *pTeam = GetGlobalTeam( TEAM_TERRORIST ) )
						pTeam->MarkSurrendered();
				}
				else
				{
					MarkLivingPlayersOnTeamAsNotReceivingMoneyNextRound( TEAM_TERRORIST );
					numCashAwards = 1;
				}
				TerminateRound( mp_round_restart_delay.GetFloat(), CTs_Win, &award, numCashAwards );
			}
			else if ( nTeam == TEAM_TERRORIST )
			{
				m_match.AddTerroristWins( 1 );
				TerminateRoundDelayedTeamCashAwards_t award = { TEAM_TERRORIST, TeamCashAward::WIN_BY_TIME_RUNNING_OUT_HOSTAGE };
				int numCashAwards = 0;
				if ( IsPlayingCoopGuardian() && GetCoopHumanTeamNumber() == TEAM_TERRORIST )
				{
					if ( CTeam *pTeam = GetGlobalTeam( TEAM_CT ) )
						pTeam->MarkSurrendered();
				}
				else
				{
					MarkLivingPlayersOnTeamAsNotReceivingMoneyNextRound( TEAM_CT );
					numCashAwards = 1;
				}
				TerminateRound( mp_round_restart_delay.GetFloat(), Terrorists_Win, &award, numCashAwards );
			}
			else
			{
				TerminateRound( mp_round_restart_delay.GetFloat(), Round_Draw );
			}
		}
        else if ( m_bMapHasBombTarget )
        {
            //If the bomb is planted, don't let the round timer end the round.
            //keep going until the bomb explodes or is defused
            if( !m_bBombPlanted )
            {
                m_match.AddCTWins( 1 );	
                MarkLivingPlayersOnTeamAsNotReceivingMoneyNextRound(TEAM_TERRORIST);
				TerminateRoundDelayedTeamCashAwards_t award = { TEAM_CT, TeamCashAward::WIN_BY_TIME_RUNNING_OUT_BOMB };
                TerminateRound( mp_round_restart_delay.GetFloat(), Target_Saved, &award, 1 );
            }
        }
        else if ( m_bMapHasRescueZone )
        {
			MarkLivingPlayersOnTeamAsNotReceivingMoneyNextRound(TEAM_CT);
            m_match.AddTerroristWins( 1 );
			TerminateRoundDelayedTeamCashAwards_t award = { TEAM_TERRORIST, TeamCashAward::WIN_BY_TIME_RUNNING_OUT_HOSTAGE };
            TerminateRound( mp_round_restart_delay.GetFloat(), Hostages_Not_Rescued, &award, 1 );

        }
        else if ( m_bMapHasEscapeZone )
        {
            m_match.AddCTWins( 1 );
            TerminateRound( mp_round_restart_delay.GetFloat(), Terrorists_Not_Escaped );
        }
		//If there is no scenario-specific winner when the time runs out, just declare a draw
        else
        {
            TerminateRound( mp_round_restart_delay.GetFloat(), Round_Draw );
        }
    }

    bool CCSGameRules::ShouldGunGameSpawnBomb( void )
    {
        return ( m_bGunGameRespawnWithBomb && ( m_fGunGameBombRespawnTimer <= gpGlobals->curtime ) );
    }

    void CCSGameRules::SetGunGameSpawnBomb( bool allow )
    {
        m_bGunGameRespawnWithBomb = allow;
        m_fGunGameBombRespawnTimer = gpGlobals->curtime + mp_ggtr_bomb_respawn_delay.GetFloat();
    }

	void CCSGameRules::RewardMatchEndDrops( bool bAbortedMatch )
	{
	}

	void CCSGameRules::GoToIntermission( bool bAbortedMatch )
	{
		Msg( "Going to intermission...\n" );
		// stopping replay on all clients in anticipation of UI showing rank and xp updates. 
		// if CS_UM_ServerRankUpdate and CS_UM_XpUpdate can be passed through in replay, and UI works with that, this Stop can be removed
		
		static ConVarRef spec_replay_enable( "spec_replay_enable" );
		if ( spec_replay_enable.GetInt() == 2 )
		{
			; // Permanent replay is not stopped here, client will have to deal with all delayed and realtime passthrough messages in the mix
		}
		else
		{
			engine->StopClientsHltvReplay();
		}

		bool bAnnounceNextMap = true;
		bool bDoGenericRewardMatchEndDrops = true;

		// Do old style match end drops (community and official non-competitive)
		if ( bDoGenericRewardMatchEndDrops )
		{
			RewardMatchEndDrops( bAbortedMatch );
		}
		m_bIsDroppingItems = true; // Always wait in case items drop

		// generate the map list that players will be voting on
		CreateEndMatchMapGroupVoteOptions();

		m_match.SetPhase( GAMEPHASE_MATCH_ENDED );

		m_bHasMatchStarted = false;

		IGameEvent *winEvent = gameeventmanager->CreateEvent( "cs_win_panel_match" );
		if ( winEvent )
		{
			gameeventmanager->FireEvent( winEvent );
		}

		BaseClass::GoToIntermission();

		//
		// Tell everyone what the next map is
		//
		bool bNextMapAlreadySet = false;
		char szNextMap[ MAX_PATH ] = { 0 };
		if ( nextlevel.GetString() && *nextlevel.GetString() && engine->IsMapValid( nextlevel.GetString() ) )
		{
			bNextMapAlreadySet = true;
			Q_strncpy( szNextMap, nextlevel.GetString(), sizeof( szNextMap ) );
		}
		else
		{
			GetNextLevelName( szNextMap, sizeof( szNextMap ) );
		}

		// if we're running map group, let players vote to pick the next map at the end of the match
		if ( IsEndMatchVotingForNextMap() && !bNextMapAlreadySet && GetCMMItemDropRevealEndTime() < gpGlobals->curtime )
		{
			bAnnounceNextMap = !CheckSetVoteTime();
		}
		else if ( !m_bIsDroppingItems )
		{
			m_eEndMatchMapVoteState = k_EEndMatchMapVoteState_VoteAllDone;
		}

		// In queued matchmaking or during a "season" map group, don't message the next map
		if ( IsQueuedMatchmaking() || bAnnounceNextMap == false )
		{
			szNextMap[ 0 ] = 0;
		}

		//Clear various states from all players and freeze them in place
		for ( int i = 1; i <= MAX_PLAYERS; i++ )
		{
			CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );

			if ( pPlayer )
			{
				pPlayer->Unblind();
				pPlayer->AddFlag( FL_FROZEN );
				pPlayer->SetPendingTeamNum( TEAM_UNASSIGNED );
				UpdateMatchStats( pPlayer, m_match.GetWinningTeam() );

				extern ConVar nextmap_print_enabled;
				if ( szNextMap[ 0 ] && nextmap_print_enabled.GetBool() )
				{
					if ( IsGameConsole() || engine->IsDedicatedServerForXbox() || engine->IsDedicatedServerForPS3() )
					{
						// we know all the map names on console so we can safely assume we have a token for all maps
						const int nMaxLength = MAX_MAP_NAME + 10 + 1;
						char szToken[ nMaxLength ];
						CreateFriendlyMapNameToken( szNextMap, szToken, nMaxLength );
						ClientPrint( pPlayer, HUD_PRINTTALK, "#game_nextmap", szToken );
					}
					else
					{
						ClientPrint( pPlayer, HUD_PRINTTALK, "#game_nextmap", V_GetFileName( szNextMap ) );
					}
				}
			}
		}

		// freeze players while in intermission
		m_bFreezePeriod = true;

		// When not queued matchmaking that is driven by rematch endmatch vote machine reveal all ranks now
		if ( !IsQueuedMatchmaking() )
		{
			CReliableBroadcastRecipientFilter filter;
			CCSUsrMsg_ServerRankRevealAll msg;
			SendUserMessage( filter, CS_UM_ServerRankRevealAll, msg );
		}
    }

	bool CCSGameRules::CheckSetVoteTime()
	{
		int numMaps = 0;
		if ( g_pGameTypes )
		{
			const char* mapGroupName = gpGlobals->mapGroupName.ToCStr();
			const CUtlStringList* mapsInGroup = g_pGameTypes->GetMapGroupMapList( mapGroupName );
			if ( mapsInGroup )
				numMaps = mapsInGroup->Count();
		}

		if ( numMaps > 1 )
		{
			m_eEndMatchMapVoteState = k_EEndMatchMapVoteState_VoteInProgress;

			m_nEndMatchTiedVotes.RemoveAll();

			// artificially set the m_flIntermissionStartTime
			if ( m_flIntermissionStartTime &&
				( gpGlobals->curtime + mp_endmatch_votenextleveltime.GetFloat() > m_flIntermissionStartTime + GetIntermissionDuration() ) )
			{
				m_flIntermissionStartTime = gpGlobals->curtime + mp_endmatch_votenextleveltime.GetFloat() - GetIntermissionDuration();
			}

			return true;
		}

		m_eEndMatchMapVoteState = k_EEndMatchMapVoteState_VoteAllDone;
		return false;
	}

    void CCSGameRules::GoToMatchRestartIntermission()
    {

        //We use our own intermission steps here to bypass the match win panel (AKA scoreboard)
        BaseClass::GoToIntermission();

        for ( int i = 1; i <= MAX_PLAYERS; i++ )
        {
            CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );

            if ( pPlayer )
            {
                pPlayer->Unblind();

                // set all players to FL_FROZEN
                pPlayer->AddFlag( FL_FROZEN );					
            }
        }

        // freeze players while in intermission
        m_bFreezePeriod = true;
    }

	bool CCSGameRules::GameModeSupportsHealthBuffer( void )
	{

		return false;
	}

	void CCSGameRules::SpeechAppendGlobalCriteria( AI_CriteriaSet* pCriteriaSet )
	{
		BaseClass::SpeechAppendGlobalCriteria( pCriteriaSet );

		pCriteriaSet->AppendCriteria( "RoundLength", GetRoundLength() );
		pCriteriaSet->AppendCriteria( "RoundElapsedTime", GetRoundElapsedTime() );
		pCriteriaSet->AppendCriteria( "RoundNumber", GetMatch()->GetRoundsPlayed() + 1 );

		pCriteriaSet->AppendCriteria( "Score.T", GetMatch()->GetCTScore() );
		pCriteriaSet->AppendCriteria( "Score.CT", GetMatch()->GetTerroristScore() );

		if ( IsPlayingCooperativeGametype() )
		{
			int playerTeam = GetCoopHumanTeamNumber();
			int enemyTeam = GetCoopEnemyTeamNumber();

			// Count players
			int playersAlive = 0;
			int enemiesAlive = 0;
			int enemyHeavies = 0;

			for ( int iPlayer = 1; iPlayer <= MAX_PLAYERS; ++iPlayer )
			{
				CCSPlayer* pPlayer = ToCSPlayer( UTIL_PlayerByIndex( iPlayer ) );
				if ( !pPlayer )
					continue;

				if ( !pPlayer->IsAlive() )
					continue;

				if ( pPlayer->GetTeamNumber() == playerTeam )
				{
					playersAlive++;
				}
				else if ( pPlayer->GetTeamNumber() == enemyTeam )
				{
					enemiesAlive++;
					if ( pPlayer->HasHeavyAssaultSuit() )
						enemyHeavies++;
				}
			}

			pCriteriaSet->AppendCriteria( "Coop.PlayersAlive", playersAlive );
			pCriteriaSet->AppendCriteria( "Coop.EnemiesAlive", enemiesAlive );
			pCriteriaSet->AppendCriteria( "Coop.EnemyHeavies", enemyHeavies );
		}

		if ( IsPlayingCoopGuardian() && ( mp_guardian_special_kills_needed.GetInt() > 0 ) )
		{
			float fracRemaining = m_nGuardianModeSpecialKillsRemaining / ( float )( mp_guardian_special_kills_needed.GetInt() );
			pCriteriaSet->AppendCriteria( "Coop.PercentComplete", ( 1.0f - fracRemaining ) * 100.0f );
		}

		if ( GetActiveServerQuestID() > 0 )
		{
			pCriteriaSet->AppendCriteria( "Server.Quest", GetActiveServerQuestID() );
		}
	}

    void CCSGameRules::UpdateMatchStats( CCSPlayer* pPlayer, int winnerIndex )
    {

        if( pPlayer->GetTeamNumber() == winnerIndex)
        {
            CCS_GameStats.IncrementStat( pPlayer, CSSTAT_MATCHES_WON, 1, false, true );
            if( IsPlayingGunGame() )
            {

                if( ( IsPlayingGunGameProgressive() && pPlayer->MadeFinalGunGameProgressiveKill() )  ||
                    ( IsPlayingGunGameTRBomb() ) )
                {
                    CCS_GameStats.IncrementStat(pPlayer,CSSTAT_GUN_GAME_MATCHES_WON,1);
                }

                if( IsPlayingGunGameProgressive() )
                {
                    CCS_GameStats.IncrementStat(pPlayer,CSSTAT_GUN_GAME_PROGRESSIVE_MATCHES_WON,1);
                    
                    // check for a rampage:  See if we made it thru gun game progressive without dying once
                    if( pPlayer->WasKilledThisRound() == false && pPlayer->MadeFinalGunGameProgressiveKill() )
                    {
                        pPlayer->AwardAchievement(CSGunGameProgressiveRampage); 
                    }

                    // see if we won a progressive match without reloading
                    if( !pPlayer->HasReloaded()  )
                    {
                        pPlayer->AwardAchievement(CSGunGameConservationist);
                    }
                }
                else if( IsPlayingGunGameTRBomb())
                {
                    CCS_GameStats.IncrementStat(pPlayer,CSSTAT_GUN_GAME_TRBOMB_MATCHES_WON,1);
                }
            }

            int mapIndex = GetCSLevelIndex(gpGlobals->mapname.ToCStr());
            if ( mapIndex != -1 )
            {
                CSStatType_t matchStatId = MapName_StatId_Table[mapIndex].matchesWonId;
                // look up our map information and increment the matches won stat.  Rounds and other stats are done elsewhere
                if(matchStatId != CSSTAT_UNDEFINED)
                    CCS_GameStats.IncrementStat( pPlayer,matchStatId, 1, false, true );
            }

        }
        else if( winnerIndex == WINNER_DRAW )
        {
            CCS_GameStats.IncrementStat( pPlayer, CSSTAT_MATCHES_DRAW, 1, false, true );
        }
        CCS_GameStats.IncrementStat( pPlayer, CSSTAT_MATCHES_PLAYED, 1, false, true );

        if( IsPlayingGunGame() )
        {
            CCS_GameStats.IncrementStat( pPlayer, CSSTAT_GUN_GAME_MATCHES_PLAYED, 1, false, true );
        }

        // CSN-8618 and others - Make sure these get sent before stats are reset. 
        // End rounds stats are sent BEFORE this call, and the match ends before the next send happens
        // (resetting what we're recording above). Send these now so they don't get lost.
        CCS_GameStats.SendStatsToPlayer( pPlayer, CSSTAT_PRIORITY_ENDROUND );
    }

    // --------------------------------------------------------------------------------------------------- //
    // Contribution score helper functions
    // --------------------------------------------------------------------------------------------------- //

    inline float FalloffWeight( float fDistance, float fRangeInner, float fRangeOuter )
    {
        if ( fDistance > fRangeInner )
            return (fRangeOuter - fDistance) / (fRangeOuter - fRangeInner);
        else
            return 1.0f;
    }

    // splits point amongst all players in zone
    void CCSGameRules::SplitScoreAmongPlayersInZone( int iPoints, int iTeam, CCSPlayer* pExcludePlayer, uint iPlace )
    {
        float fWeights[MAX_PLAYERS+1];
        memset( fWeights, 0, sizeof( fWeights ) );
        float fWeightSum = 0.0f;

        for ( int i = 1; i <= MAX_PLAYERS; i++ )
        {
            CCSPlayer* pPlayer = (CCSPlayer*)UTIL_PlayerByIndex( i );
            if ( !pPlayer )
                continue;

            if ( pPlayer == pExcludePlayer )
                continue;

            if ( pPlayer->GetTeamNumber() != iTeam )
                continue;

            if ( pPlayer->GetLastKnownArea() && pPlayer->GetLastKnownArea()->GetPlace() == iPlace )
            {
                fWeightSum += 1.0f;
                fWeights[i] = 1.0f;
            }
        }

        if ( fWeightSum == 0.0f )
            return;

        for ( int i = 1; i <= gpGlobals->maxClients; i++ )
        {
            if ( fWeights[i] > 0.0f )
            {
                CCSPlayer* pPlayer = (CCSPlayer*)UTIL_PlayerByIndex( i );
                ASSERT( pPlayer );
                int score = RoundFloatToInt(iPoints * fWeights[i] / fWeightSum );
                pPlayer->AddScore( score );
                pPlayer->AddRoundProximityScore( score );
            }
        }
    }

    void CCSGameRules::SplitScoreAmongPlayersInRange( int iPoints, int iTeam, CCSPlayer* pExcludePlayer, const Vector& center, float fRangeInner, float fRangeOuter )
    {
        float fMaxRangeSquared = fRangeOuter * fRangeOuter;
        float fWeights[MAX_PLAYERS+1];
        memset( fWeights, 0, sizeof( fWeights ) );
        float fWeightSum = 0.0f;

        for ( int i = 1; i <= MAX_PLAYERS; i++ )
        {
            CCSPlayer* pPlayer = (CCSPlayer*)UTIL_PlayerByIndex( i );
            if ( !pPlayer )
                continue;

            if ( pPlayer == pExcludePlayer )
                continue;

            if ( pPlayer->GetTeamNumber() != iTeam )
                continue;

            if ( center.DistToSqr( pPlayer->GetAbsOrigin() ) <= fMaxRangeSquared )
            {
                float fWeight = FalloffWeight( center.DistTo( pPlayer->GetAbsOrigin()), fRangeInner, fRangeOuter );
                fWeightSum += fWeight;
                fWeights[i] = fWeight;
            }
        }

        if ( fWeightSum == 0.0f )
            return;

        for ( int i = 1; i <= gpGlobals->maxClients; i++ )
        {
            if ( fWeights[i] > 0.0f )
            {
                CCSPlayer* pPlayer = (CCSPlayer*)UTIL_PlayerByIndex( i );
                int score = RoundFloatToInt( iPoints * fWeights[i] / fWeightSum );
                pPlayer->AddScore( score );
                pPlayer->AddRoundProximityScore( score );
            }
        }
    }

    void CCSGameRules::ScorePlayerKill( CCSPlayer* pPlayer, CCSPlayer* pVictim, bool bIsObjectiveKill )
    {
        ASSERT( pPlayer != NULL );
        if ( pPlayer )
        {
			if ( IsPlayingGunGameDeathmatch() && pPlayer->GetActiveCSWeapon() )
			{
				CEconItemView* pItem = pPlayer->GetActiveCSWeapon()->GetEconItemView();
				loadout_positions_t iWepSlot = ( pItem && pItem->GetItemDefinition() ) ? pItem->GetItemDefinition()->GetDefaultLoadoutSlot() : LOADOUT_POSITION_INVALID;

				int nBonusScore;
				int nWeaponScore = GetWeaponScoreForDeathmatch( iWepSlot, &nBonusScore );
				if ( pPlayer->AddDeathmatchKillScore( nWeaponScore, nBonusScore, iWepSlot ) <= 0 )
				{
					pPlayer->AddContributionScore( contributionscore_kill.GetInt() );
				}

				// Give team points for enemy kill
				if ( IsPlayingTeamDM() )
				{
					int nScore = MAX( 0, mp_dm_teammode_kill_score.GetInt() );
					int nBonus = 0;

					// check for using current bonus weapon
					if ( IsDMBonusActive() && GetDMBonusWeaponLoadoutSlot() == iWepSlot )
						nBonus = MAX( 0, mp_dm_teammode_bonus_score.GetInt() );

					if ( nScore + nBonus > 0 )
					{
						// Score points
						if ( pPlayer->GetTeamNumber() == TEAM_TERRORIST )
							m_match.AddTerroristBonusPoints( nScore + nBonus );
						else if ( pPlayer->GetTeamNumber() == TEAM_CT )
							m_match.AddCTBonusPoints( nScore + nBonus );
						
						const char* szVictimName = pVictim ? pVictim->GetPlayerName() : "unknown";

						// Print a message to the player
						char szScore[64];
						if ( nBonus == 0 || nScore == 0 )
						{
							V_sprintf_safe( szScore, "%d", nScore + nBonus );
						}
						else
						{
							V_sprintf_safe( szScore, "%d (+%d)", nScore, nBonus );
						}

						if ( nBonus > 0 )
						{
							int pluralScore = ( nScore == 0 ) ? nBonus : nScore;
							const char* szToken = ( pluralScore == 1 ) ? "#Player_Team_Award_Bonus_Weapon" : "#Player_Team_Award_Bonus_Weapon_Plural";

							// Get weapon name for bonus token
							const char* szWeaponName = "unknown";
							if ( pItem )
								szWeaponName = pItem->GetItemDefinition()->GetItemBaseName();

							ClientPrint( pPlayer, HUD_PRINTTALK, szToken, szScore, szVictimName, szWeaponName );
						}
						else
						{
							const char* szToken = ( nScore == 1 ) ? "#Player_Team_Award_Killed_Enemy" : "#Player_Team_Award_Killed_Enemy_Plural";
							ClientPrint( pPlayer, HUD_PRINTTALK, szToken, szScore, szVictimName );
						}
					}
				}
			}
			else
			{
				if ( bIsObjectiveKill )
					pPlayer->AddContributionScore( contributionscore_objective_kill.GetInt() );
				else
					pPlayer->AddContributionScore( contributionscore_kill.GetInt() );
				
				// In survival you get a % of your target's score when you kill them
				Assert( pPlayer != pVictim ); // should go through suicide scoring instead.
				if ( contributionscore_kill_factor.GetFloat() > 0.0f && pVictim->GetRoundContributionScore() > 0 )
					pPlayer->AddContributionScore( Floor2Int( pVictim->GetRoundContributionScore() * contributionscore_kill_factor.GetFloat() ) );

				pPlayer->AddScore( RoundFloatToInt( score_kill_enemy_bonus.GetFloat() ) );
			}
        }
    }

    void CCSGameRules::ScorePlayerAssist( CCSPlayer* pPlayer, CCSPlayer* pVictim )
    {
        ASSERT( pPlayer != NULL );
        if ( pPlayer )
        {
			if ( IsPlayingGunGameDeathmatch() )
			{
				// we don't store what weapon the player did the assist damage with and we can't guarantee the player has a weapon 
				// when the assist is awarded, so we just give them and average of half the points awarded
				int nScore = (mp_dm_kill_base_score.GetInt() / 2) + 1;
				pPlayer->AddDeathmatchKillScore( nScore, 0, LOADOUT_POSITION_INVALID, true, pVictim->GetPlayerName() );
				pPlayer->IncrementAssistsCount( 1 );
			}
			else if ( IPointsForKill( pPlayer, pVictim ) > 0 ) // this ensures that only assists on enemies are recorded, but "assists" for teammate kills are not
			{
				pPlayer->AddContributionScore( contributionscore_assist.GetInt() );
				pPlayer->IncrementAssistsCount( 1 );
			}
        }
    }	

    void CCSGameRules::ScorePlayerTeamKill( CCSPlayer* pPlayer, CCSPlayer* pVictim )
    {
        ASSERT( pPlayer != NULL );
        if ( pPlayer )
        {
            pPlayer->AddContributionScore( contributionscore_team_kill.GetInt( ) );
        }
    }

    void CCSGameRules::ScorePlayerSuicide( CCSPlayer* pPlayer )
    {
        ASSERT( pPlayer != NULL );
        if ( pPlayer )
        {
            pPlayer->AddContributionScore( contributionscore_suicide.GetInt( ) );

			if ( !m_bLoadingRoundBackupData )
				pPlayer->ProcessSuicideAsKillReward();
        }
    }

    void CCSGameRules::ScorePlayerDamage( CCSPlayer* pPlayer, CCSPlayer* pVictim, float fDamage )
    {
        pPlayer->AddScore( RoundFloatToInt( fDamage * score_damage.GetFloat() ) );

        Place iPlace = pPlayer->GetLastKnownArea() ? pPlayer->GetLastKnownArea()->GetPlace() : UNDEFINED_PLACE;
        if ( iPlace != UNDEFINED_PLACE )
            SplitScoreAmongPlayersInZone( RoundFloatToInt( fDamage * score_team_damage_bonus.GetFloat() ), pPlayer->GetTeamNumber(), pPlayer, iPlace );

        // award bonus for being near planted bomb
        {
            const float fRadiusInner = score_planted_bomb_proximity_damage_radius_inner.GetFloat();
            const float fRadiusOuter = score_planted_bomb_proximity_damage_radius_outer.GetFloat();
            const float fRadiusOuterSquared = fRadiusOuter * fRadiusOuter;
            FOR_EACH_VEC( g_PlantedC4s, iBomb )
            {
                CPlantedC4 *pC4 = g_PlantedC4s[iBomb];
                if ( pC4 && pC4->IsBombActive() )
                {
                    Vector bombPos = pC4->GetAbsOrigin();
                    Vector playerToBomb = pPlayer->GetAbsOrigin() - bombPos;
                    if ( playerToBomb.LengthSqr() < fRadiusOuterSquared )
                    {
                        float fWeight = FalloffWeight( playerToBomb.Length(), fRadiusInner, fRadiusOuter );
                        int score = RoundFloatToInt( fWeight * score_planted_bomb_proximity_damage_bonus.GetFloat() );
                        pPlayer->AddScore( score  );
                        pPlayer->AddRoundProximityScore( score );
                        break;
                    }
                }
            }
        }

        // bonus for being near hostage
		{
			const float fRadiusInner = score_hostage_proximity_damage_radius_inner.GetFloat();
			const float fRadiusOuter = score_hostage_proximity_damage_radius_outer.GetFloat();
			float fClosestSquared = FLT_MAX;
			FOR_EACH_VEC( g_Hostages, iHostage )
			{
				CHostage* pHostage = g_Hostages[iHostage];
				if ( pHostage->m_iHealth > 0 && !pHostage->IsRescued() )
				{
					float fDistanceSquared = pHostage->GetAbsOrigin().DistToSqr( pPlayer->GetAbsOrigin() );
					if ( fDistanceSquared < fClosestSquared )
					{
						fClosestSquared = fDistanceSquared;
					}
				}
			}
			if ( fClosestSquared < fRadiusOuter * fRadiusOuter )
			{
				float fWeight = FalloffWeight( sqrtf( fClosestSquared ), fRadiusInner, fRadiusOuter );
				int score = RoundFloatToInt( fDamage * fWeight * score_hostage_proximity_damage_bonus.GetFloat() );
				pPlayer->AddScore( score );
				pPlayer->AddRoundProximityScore( score );
			}
		}

        // damage near dropped bomb bonus
        {
            const float fRadiusInner = score_dropped_bomb_proximity_damage_bonus_radius_inner.GetFloat();
            const float fRadiusOuter = score_dropped_bomb_proximity_damage_bonus_radius_outer.GetFloat();
            CBaseEntity* pC4 = gEntList.FindEntityByClassnameNearest( "weapon_c4", pPlayer->GetAbsOrigin(), fRadiusOuter );
            if ( pC4 && pC4->GetOwnerEntity() == NULL )
            {
                float fWeight = FalloffWeight( pC4->GetAbsOrigin().DistTo( pPlayer->GetAbsOrigin()), fRadiusInner, fRadiusOuter );
                int score = RoundFloatToInt( fDamage * fWeight * score_dropped_bomb_proximity_damage_bonus.GetFloat() );
                pPlayer->AddScore( score );
                pPlayer->AddRoundProximityScore( score );
            }
        }

        // damage near dropped defuser
        {
            const float fRadiusInner = score_dropped_defuser_proximity_damage_radius_inner.GetFloat();
            const float fRadiusOuter = score_dropped_defuser_proximity_damage_radius_outer.GetFloat();
            CBaseEntity* pDefuser = gEntList.FindEntityByClassnameNearest( "item_defuser", pPlayer->GetAbsOrigin(), fRadiusOuter );
            if ( pDefuser && pDefuser->GetOwnerEntity() == NULL )
            {
                float fWeight = FalloffWeight( pDefuser->GetAbsOrigin().DistTo( pPlayer->GetAbsOrigin() ), fRadiusInner, fRadiusOuter );
                int score = RoundFloatToInt( fDamage * fWeight * score_dropped_defuser_proximity_damage_bonus.GetFloat() );
                pPlayer->AddScore( score );
                pPlayer->AddRoundProximityScore( score );
            }
        }
    }

    void CCSGameRules::ScoreBombPlant( CCSPlayer* pPlayer )
    {
        ASSERT( pPlayer != NULL );
        if ( pPlayer )
        {
            pPlayer->AddContributionScore( contributionscore_bomb_planted.GetInt() );
        }

        CPlantedC4 *pC4 = g_PlantedC4s[0];
        ASSERT( pC4 != NULL );
        if ( pC4 )
        {
            SplitScoreAmongPlayersInRange( score_bomb_plant_bonus.GetInt(), TEAM_TERRORIST, NULL, pC4->GetAbsOrigin(), 
                score_bomb_plant_radius_inner.GetFloat(), score_bomb_plant_radius_outer.GetFloat() );
        }
    }

	void CCSGameRules::ScoreBombExploded( CCSPlayer* pPlayer )
	{
		ASSERT( pPlayer != NULL );
		if ( pPlayer )
		{
			pPlayer->AddContributionScore( contributionscore_bomb_exploded.GetInt() );
		}
	}

    void CCSGameRules::ScoreBombDefuse( CCSPlayer* pPlayer, bool bMajorEvent )
    {
        ASSERT( pPlayer != NULL );
        if ( pPlayer )
        {
            pPlayer->AddContributionScore( bMajorEvent ? contributionscore_bomb_defuse_major.GetInt() : contributionscore_bomb_defuse_minor.GetInt() );
        }

		m_bBombDefused = true;

        CPlantedC4 *pC4 = g_PlantedC4s[0];
        ASSERT( pC4 != NULL );
        if ( pC4 && bMajorEvent )
        {
            SplitScoreAmongPlayersInRange( score_bomb_defuse_bonus.GetInt(), TEAM_CT, NULL, pC4->GetAbsOrigin(), 
                score_bomb_defuse_radius_inner.GetFloat(), score_bomb_defuse_radius_outer.GetFloat() );
        }
    }

    void CCSGameRules::ScoreHostageRescue( CCSPlayer* pPlayer, CHostage* pHostage, bool bMajorEvent )
    {
        ASSERT( pPlayer != NULL );
        if ( pPlayer )
        {
            pPlayer->AddContributionScore( bMajorEvent ? contributionscore_hostage_rescue_major.GetInt() : contributionscore_hostage_rescue_minor.GetInt() );
        }

        ASSERT( pHostage != NULL );
        if ( pHostage && bMajorEvent )
        {
            SplitScoreAmongPlayersInRange( score_hostage_rescue_bonus.GetInt(), TEAM_CT, NULL, pHostage->GetAbsOrigin(), 
                score_hostage_rescue_radius_inner.GetFloat(), score_hostage_rescue_radius_outer.GetFloat() );
        }
    }

    void CCSGameRules::ScoreHostageKilled( CCSPlayer* pPlayer )
    {
        ASSERT( pPlayer != NULL );
        if ( pPlayer )
        {
            pPlayer->AddContributionScore( contributionscore_hostage_kill.GetInt() );
        }
    }


    void CCSGameRules::ScoreHostageDamage( CCSPlayer* pPlayer, float fDamage )
    {
        ASSERT( pPlayer != NULL );
        if ( pPlayer )
        {
            pPlayer->AddScore( -RoundFloatToInt( fDamage * score_hostage_damage_penalty.GetInt() ) );
        }
    }


    void CCSGameRules::ScoreFriendlyFire( CCSPlayer* pPlayer, CCSPlayer* pVictim, float fDamage )
    {
        ASSERT( pPlayer != NULL );
        if ( pPlayer )
        {
            pPlayer->AddScore( -RoundFloatToInt( fDamage * score_ff_damage.GetFloat() ) );
        }
    }

    void CCSGameRules::ScoreBlindEnemy( CCSPlayer* pPlayer, CCSPlayer* pVictim )
    {
        ASSERT( pPlayer != NULL );
        if ( pPlayer )
        {
            pPlayer->AddScore( score_blind_enemy_bonus.GetInt() );


			/// ROUND BASED MATCH STATS
			int nRound = GetCurrentRoundIndexForMatchStats();

			if ( ShouldRecordMatchStats( nRound ) )
			{
				// match stats for blinding enemy
				pPlayer->m_iMatchStats_EnemiesFlashed.GetForModify( nRound ) ++;	// record in MatchStats'

				// Keep track in QMM data
				if ( pPlayer->GetHumanPlayerAccountID() )
				{
					if ( CCSGameRules::CQMMPlayerData_t *pQMM = QueuedMatchmakingPlayersDataFind( pPlayer->GetHumanPlayerAccountID() ) )
					{
						pQMM->m_iMatchStats_EnemiesFlashed[ nRound ] = pPlayer->m_iMatchStats_EnemiesFlashed.Get( nRound );
					}
				}
			}
		}
    }

    void CCSGameRules::ScoreBlindFriendly( CCSPlayer* pPlayer, CCSPlayer* pVictim )
    {
        ASSERT( pPlayer != NULL );
        if ( pPlayer )
        {
            pPlayer->AddScore( -score_blind_friendly_penalty.GetInt() );
        }
    }


	bool CCSGameRules::GetDogTag( CCSPlayer* pPlayer, CItemDogtags* pDogtags, bool *pDeny )
	{
		CCSPlayer* pTarget = pDogtags->GetOwner();
		const char* szVictimName = "unknown";
		if ( pTarget != nullptr )
			szVictimName = pTarget->GetPlayerName();

		// Score points for picking up dogtags in deathmatch
		int dogtagPoints = mp_dm_dogtag_score.GetInt();
		if ( IsPlayingGunGameDeathmatch() && dogtagPoints > 0 )
		{
			// get points for picking up dog tag of someone you killed
			pPlayer->AddContributionScore( dogtagPoints );

			// Print a message to the player
			if ( !IsPlayingTeamDM() )
			{
				char szPoints[20];
				V_sprintf_safe( szPoints, "%d", dogtagPoints );

				ClientPrint( pPlayer, HUD_PRINTTALK,
					(dogtagPoints == 1) ? "#Player_Point_Award_Picked_Up_Dogtag" : "#Player_Point_Award_Picked_Up_Dogtag_Plural",
					szPoints, szVictimName );
			}
		}

		// In team mode, score points for picking up dogtags from the enemy team
		int teamScore = mp_dm_teammode_dogtag_score.GetInt();
		if ( IsPlayingTeamDM() && teamScore > 0 && pPlayer && pTarget )
		{
			if ( pPlayer->GetTeamNumber() == pTarget->GetTeamNumber() )
			{
				if ( pDeny )
					*pDeny = true;

				ClientPrint( pPlayer, HUD_PRINTTALK, "#Player_Team_Award_Picked_Up_Dogtag_Friendly", szVictimName );
			}
			else
			{
				if ( pPlayer->GetTeamNumber() == TEAM_CT )
				{
					m_match.AddCTBonusPoints( 1 );
				}
				else if ( pPlayer->GetTeamNumber() == TEAM_TERRORIST )
				{
					m_match.AddTerroristBonusPoints( 1 );
				}

				// Print a message to the player
				char szPoints[20];
				V_sprintf_safe( szPoints, "%d", teamScore );
				ClientPrint( pPlayer, HUD_PRINTTALK,
					( teamScore == 1 ) ? "#Player_Team_Award_Picked_Up_Dogtag" : "#Player_Team_Award_Picked_Up_Dogtag_Plural",
					szPoints, szVictimName );
			}
		}

		// tell dogtags they should be picked up
		return true;
	}

	void CCSGameRules::DropDogtagIfNeeded( CCSPlayer* pKiller, CCSPlayer* pTarget, const CTakeDamageInfo& damageInfo )
	{
		if ( pKiller == nullptr || pTarget == nullptr )
			return;

		// If you suicide or are teamkilled, no dogtag
		if ( !pTarget->IsOtherEnemy( pKiller ) )
			return;

		bool hasQuestProgress = false;

		if ( hasQuestProgress // Drop dogtag if they are worth quest progress
			|| ( IsPlayingGunGameDeathmatch() && mp_dm_dogtag_score.GetInt() > 0 ) // Drop dogtags in deathmatch if they are worth points
			|| ( IsPlayingTeamDM() && mp_dm_teammode_dogtag_score.GetInt() > 0 ) // Drop dogtags in team mode if they give victory points
			)
		{
			int dogtagsToDrop = 1; // sometimes we drop more than 1 dogtag

			if ( IsPlayingGunGameDeathmatch() && mp_dm_bonusweapon_dogtags.GetInt() > 0 )
			{
				// Kills with the bonus weapon drop additional dogtags.
				// Check if they were using the bonus weapon
				CWeaponCSBase* pActiveWeapon = pKiller->GetActiveCSWeapon();
				if ( pActiveWeapon && IsDMBonusActive() && GetDMBonusWeaponLoadoutSlot() == pActiveWeapon->GetDefaultLoadoutSlot() )
				{
					dogtagsToDrop += mp_dm_bonusweapon_dogtags.GetInt();
				}
			}

			for ( int i = 0; i < dogtagsToDrop; ++i )
			{
				CItemDogtags* pDogtags = ( CItemDogtags* )CreateEntityByName( "item_dogtags" );
				Vector vecDir = Vector( RandomFloat( -125.0f, 125.0f ), RandomFloat( -125.0f, 125.0f ), RandomFloat( 0.0f, 250.0f ) );
				pDogtags->SetAbsOrigin( pTarget->GetAbsOrigin() + Vector( 0.f, 0.f, 25.f ) );
				pDogtags->SetAbsVelocity( vecDir );
				pDogtags->SetLocalAngularVelocity( QAngle( RandomFloat( -800, 800 ), RandomFloat( -800, 800 ), RandomFloat( -800, 800 ) ) );
				pDogtags->SetDogtagInfo( pKiller, pTarget );
				pDogtags->Spawn();
			}
		}

		return;
	}

    // --------------------------------------------------------------------------------------------------- //
    // End Contribution score functions
    // --------------------------------------------------------------------------------------------------- //

    static void PrintToConsole( CBasePlayer *player, const char *text )
    {
        if ( player )
        {
            ClientPrint( player, HUD_PRINTCONSOLE, text );
        }
        else
        {
            Msg( "%s", text );
        }
    }

    void CCSGameRules::DumpTimers( void ) const
    {
        extern ConVar bot_join_delay;
        CBasePlayer *player = UTIL_GetCommandClient();
        CFmtStr str;

        PrintToConsole( player, str.sprintf( "Timers and related info at %f:\n", gpGlobals->curtime ) );
        PrintToConsole( player, str.sprintf( "m_bCompleteReset: %d\n", m_bCompleteReset ) );
        PrintToConsole( player, str.sprintf( "m_iTotalRoundsPlayed: %d\n", m_iTotalRoundsPlayed ) );
        PrintToConsole( player, str.sprintf( "m_iRoundTime: %d\n", m_iRoundTime.Get() ) );
        PrintToConsole( player, str.sprintf( "m_iRoundWinStatus: %d\n", m_iRoundWinStatus.Get() ) );

        PrintToConsole( player, str.sprintf( "first connected: %d\n", m_bFirstConnected ) );
        PrintToConsole( player, str.sprintf( "intermission start time: %f\n", m_flIntermissionStartTime ) );
		PrintToConsole( player, str.sprintf( "intermission duration: %f\n", GetIntermissionDuration() ) );
        PrintToConsole( player, str.sprintf( "freeze period: %d\n", m_bFreezePeriod.Get() ) );
        PrintToConsole( player, str.sprintf( "round restart time: %f\n", m_flRestartRoundTime.Get() ) );
        PrintToConsole( player, str.sprintf( "game start time: %f\n", m_flGameStartTime.Get() ) );
		PrintToConsole( player, str.sprintf( "m_fMatchStartTime: %f\n", m_fMatchStartTime.Get() ) );
		PrintToConsole( player, str.sprintf( "m_fRoundStartTime: %f\n", m_fRoundStartTime.Get() ) );
        PrintToConsole( player, str.sprintf( "freeze time: %d\n", m_iFreezeTime ) );
        PrintToConsole( player, str.sprintf( "next think: %f\n", m_tmNextPeriodicThink ) );

        PrintToConsole( player, str.sprintf( "fraglimit: %d\n", fraglimit.GetInt() ) );
        PrintToConsole( player, str.sprintf( "mp_maxrounds: %d\n", mp_maxrounds.GetInt() ) );
        PrintToConsole( player, str.sprintf( "mp_winlimit: %d\n", mp_winlimit.GetInt() ) );
        PrintToConsole( player, str.sprintf( "bot_quota: %d\n", cv_bot_quota.GetInt() ) );
        PrintToConsole( player, str.sprintf( "bot_quota_mode: %s\n", cv_bot_quota_mode.GetString() ) );
        PrintToConsole( player, str.sprintf( "bot_join_after_player: %d\n", cv_bot_join_after_player.GetInt() ) );
        PrintToConsole( player, str.sprintf( "bot_join_delay: %d\n", bot_join_delay.GetInt() ) );
        PrintToConsole( player, str.sprintf( "nextlevel: %s\n", nextlevel.GetString() ) );
		PrintToConsole( player, str.sprintf( "nextmode: %s\n", nextmode.GetString() ) );

        int humansInGame = UTIL_HumansInGame( true );
        int botsInGame = UTIL_BotsInGame();
        PrintToConsole( player, str.sprintf( "%d humans and %d bots in game\n", humansInGame, botsInGame ) );

        PrintToConsole( player, str.sprintf( "num CTs (spawnable): %d (%d)\n", m_iNumCT, m_iNumSpawnableCT ) );
        PrintToConsole( player, str.sprintf( "num Ts (spawnable): %d (%d)\n", m_iNumTerrorist, m_iNumSpawnableTerrorist ) );

        if ( g_fGameOver )
        {
            PrintToConsole( player, str.sprintf( "Game is over!\n" ) );
        }
        PrintToConsole( player, str.sprintf( "\n" ) );
    }

    CON_COMMAND( mp_dump_timers, "Prints round timers to the console for debugging" )
    {
        if ( !UTIL_IsCommandIssuedByServerAdmin() )
            return;

        if ( CSGameRules() )
        {
            CSGameRules()->DumpTimers();
        }
    }


    // living players on the given team need to be marked as not receiving any money
    // next round.
    void CCSGameRules::MarkLivingPlayersOnTeamAsNotReceivingMoneyNextRound(int team)
    {
        int playerNum;
        for (playerNum = 1; playerNum <= gpGlobals->maxClients; ++playerNum)
        {
            CCSPlayer *player = (CCSPlayer *)UTIL_PlayerByIndex(playerNum);
            if (player == NULL)
            {
                continue;
            }

            if ((player->GetTeamNumber() == team) && (player->IsAlive()))
            {
				// Exception here: only here and not inside "MarkAsNotReceivingMoneyNextRound"
				// to not affect team-wide money management, round backups, etc.

				// When a player is alive and controlling a bot then the monetary punishment should go
				// to the bot being controlled
				if ( player->IsControllingBot() )
				{
					if ( CCSPlayer* controlledBotPlayer = player->GetControlledBot() )
						player = controlledBotPlayer;
				}

                player->MarkAsNotReceivingMoneyNextRound();
            }
        }
    }


    void CCSGameRules::CheckLevelInitialized( void )
    {
        if ( !m_bLevelInitialized )
        {
            // Count the number of spawn points for each team
            // This determines the maximum number of players allowed on each
            
            m_iSpawnPointCount_Terrorist	= 0;
            m_iSpawnPointCount_CT			= 0;

            m_iMaxNumTerrorists = 0;
            m_iMaxNumCTs = 0;

			m_flCoopRespawnAndHealTime = -1;

            const char * szMapName = STRING( gpGlobals->mapname );
            int nNumSlots = 2;
            uint32 dwRichPresenceContext = 0xFFFF;
            if ( szMapName )
            {
                g_pGameTypes->GetMapInfo( szMapName, dwRichPresenceContext );
            }

            int iGameType = g_pGameTypes->GetCurrentGameType();
            int iGameMode = g_pGameTypes->GetCurrentGameMode();


            nNumSlots = g_pGameTypes->GetMaxPlayersForTypeAndMode( iGameType, iGameMode, sv_skirmish_id.GetInt() );

            // for the training map, our max is 1 CT and 0 T's, diving 1 in half doesn't work for us in this case, so make sure our min is 1
            m_iMaxNumTerrorists = MAX( nNumSlots / 2, 1 );
            m_iMaxNumCTs = MAX( nNumSlots / 2, 1 );

            m_iSpectatorSlotCount = mp_spectators_max.GetInt();


			m_nGuardianModeWaveNumber = 1;
			m_nGuardianModeSpecialKillsRemaining = mp_guardian_special_kills_needed.GetInt();

			// create the spawn point lists here
			GenerateSpawnPointListsFirstTime();

            // Is this a logo map?
            if ( gEntList.FindEntityByClassname( NULL, "info_player_logo" ) )
                m_bLogoMap = true;

            m_bLevelInitialized = true;
        }
    }

	void CCSGameRules::DoCoopSpawnAndNavInit( void )
	{
		// TODO: here is where we should pick the spawn points we will use for coop and delete the rest

		if ( m_vecMainCTSpawnPos == Vector( 0, 0, 0 ) )
		{
			CBaseEntity* ent = NULL;
			// we need to find at least one CT spawn point to figure out where the "start" of the map is
			while ( ( ent = gEntList.FindEntityByClassname( ent, "info_player_counterterrorist" ) ) != NULL )
			{
				if ( IsSpawnPointValid( ent, NULL ) )
				{
					SpawnPoint* pSpawnPoint = assert_cast< SpawnPoint* >( ent );
					if ( pSpawnPoint )
					{
						m_vecMainCTSpawnPos = pSpawnPoint->GetAbsOrigin();
						break;
					}
				}
			}
		}

		if ( TheNavMesh && !TheNavMesh->IsLoaded() && !TheNavMesh->IsAnalyzed() && !TheNavMesh->IsGenerating() )
		{
			// If there isn't a Navigation Mesh in memory, create one
			// we do this here because we need to generate the nav each time we create a new map
			// and generated it when a bot join is too late
			ConVarRef nav_generate_fast_for_tilegen( "nav_generate_fast_for_tilegen" );
			if ( nav_generate_fast_for_tilegen.IsValid() )
				nav_generate_fast_for_tilegen.SetValue( 1 );

			TheNavMesh->BeginGeneration();
		}
		else
		{
			ConVarRef bot_quota( "bot_quota" );
			bot_quota.SetValue( 20 );
		}
	}

	void CCSGameRules::AddSpawnPointToMasterList( SpawnPoint* pSpawnPoint )
	{
		//if classname == T
		if ( FClassnameIs( pSpawnPoint, "info_player_terrorist" ) || 
			 FClassnameIs( pSpawnPoint, "info_enemy_terrorist_spawn" ) ||
			 FClassnameIs( pSpawnPoint, "info_armsrace_terrorist" )  )
		{
			// check to make sure it isn't already in the list
			if ( m_TerroristSpawnPointsMasterList.Find( pSpawnPoint ) != m_TerroristSpawnPointsMasterList.InvalidIndex() )
			{
				AssertMsg( false, "AddSpawnPointToMasterList tried to add a spawn point to the list, but it already exists in the list!" );
				return;
			}

			m_TerroristSpawnPointsMasterList.AddToTail( pSpawnPoint );
		}
		else if ( FClassnameIs( pSpawnPoint, "info_player_counterterrorist" ) ||
				  FClassnameIs( pSpawnPoint, "info_armsrace_counterterrorist" ) )
		{
			if ( m_CTSpawnPointsMasterList.Find( pSpawnPoint ) != m_CTSpawnPointsMasterList.InvalidIndex() )
			{
				AssertMsg( false, "AddSpawnPointToMasterList tried to add a spawn point to the list, but it already exists in the list!" );
				return;
			}

			m_CTSpawnPointsMasterList.AddToTail( pSpawnPoint );
		}
		else if ( FClassnameIs( pSpawnPoint, "info_deathmatch_spawn" ) )
		{
			// No team specific spawns in this mode
		}
		else
		{
			// doesn't match any classes we are aware of!
			AssertMsg( false, "AddSpawnPointToMasterList is looking to adda  class to the master spawn list, but it doesn't recognize the class type!" );
		}

		RefreshCurrentSpawnPointLists();
	}

	void CCSGameRules::GenerateSpawnPointListsFirstTime( void )
	{
		//CUtlVector< SpawnPoint* >	m_CTSpawnPointsMasterList;			// The master list of CT spawn points (contains all points whether enabled or disabled)
		//CUtlVector< SpawnPoint* >	m_TerroristSpawnPointsMasterList;	// The master list of Terrorist spawn points (contains all points whether enabled or disabled)

		// Clear out existing spawn point lists
		m_TerroristSpawnPointsMasterList.RemoveAll();
		m_CTSpawnPointsMasterList.RemoveAll();

		const char* szTSpawnEntName = "info_player_terrorist";
		CBaseEntity* ent = NULL; 
		if ( IsPlayingCoopMission() )
			szTSpawnEntName = "info_enemy_terrorist_spawn";

		while ( ( ent = gEntList.FindEntityByClassname( ent, szTSpawnEntName ) ) != NULL )
		{
			if ( IsSpawnPointValid( ent, NULL ) )
			{
				SpawnPoint* pSpawnPoint = assert_cast< SpawnPoint* >( ent );
				if ( pSpawnPoint )
				{
					// Store off the terrorist spawn point
					m_TerroristSpawnPointsMasterList.AddToTail( pSpawnPoint );
				}
			}
			else
			{
				Warning("Invalid terrorist spawnpoint at (%.1f,%.1f,%.1f)\n",
					ent->GetAbsOrigin()[0],ent->GetAbsOrigin()[1],ent->GetAbsOrigin()[2] );
			}
		}

		while ( ( ent = gEntList.FindEntityByClassname( ent, "info_player_counterterrorist" ) ) != NULL )
		{
			if ( IsSpawnPointValid( ent, NULL ) ) 
			{
				SpawnPoint* pSpawnPoint = assert_cast< SpawnPoint* >( ent );
				if ( pSpawnPoint )
				{
					// Store off the CT spawn point
					m_CTSpawnPointsMasterList.AddToTail( pSpawnPoint );
				}
			}
			else
			{
				Warning("Invalid counterterrorist spawnpoint at (%.1f,%.1f,%.1f)\n",
					ent->GetAbsOrigin()[0],ent->GetAbsOrigin()[1],ent->GetAbsOrigin()[2] );
			}
		}

		ent = NULL; 
		// if we're playing armsrace, add the armsrace spawns to the list as well
		if ( CSGameRules()->IsPlayingGunGameProgressive() )
		{
			while ( ( ent = gEntList.FindEntityByClassname( ent, "info_armsrace_terrorist" ) ) != NULL )
			{
				if ( IsSpawnPointValid( ent, NULL ) )
				{
					SpawnPoint* pSpawnPoint = assert_cast< SpawnPoint* >( ent );
					if ( pSpawnPoint )
					{
						pSpawnPoint->m_nType = SpawnPoint::ArmsRace;
						// Store off the terrorist spawn point
						m_TerroristSpawnPointsMasterList.AddToTail( pSpawnPoint );
					}
				}
				else
				{
					Warning( "Invalid terrorist spawnpoint at (%.1f,%.1f,%.1f)\n",
							 ent->GetAbsOrigin()[0], ent->GetAbsOrigin()[1], ent->GetAbsOrigin()[2] );
				}
			}

			while ( ( ent = gEntList.FindEntityByClassname( ent, "info_armsrace_counterterrorist" ) ) != NULL )
			{
				if ( IsSpawnPointValid( ent, NULL ) )
				{
					SpawnPoint* pSpawnPoint = assert_cast< SpawnPoint* >( ent );
					if ( pSpawnPoint )
					{
						pSpawnPoint->m_nType = SpawnPoint::ArmsRace;
						// Store off the CT spawn point
						m_CTSpawnPointsMasterList.AddToTail( pSpawnPoint );
					}
				}
				else
				{
					Warning( "Invalid counterterrorist spawnpoint at (%.1f,%.1f,%.1f)\n",
							 ent->GetAbsOrigin()[0], ent->GetAbsOrigin()[1], ent->GetAbsOrigin()[2] );
				}
			}
		}

		// we want the arms race spawns to shuffle with the regular spawns
		if ( CSGameRules()->IsPlayingGunGameProgressive() )
			ShuffleMasterSpawnPointLists();

		// sort them to ensure the priority ones are up front
		SortMasterSpawnPointLists();

		RefreshCurrentSpawnPointLists();
	}

	void CCSGameRules::RefreshCurrentSpawnPointLists( void )
	{
		// Clear out existing spawn point lists
		m_TerroristSpawnPoints.RemoveAll();
		m_CTSpawnPoints.RemoveAll();

		m_iSpawnPointCount_Terrorist = 0;
		m_iSpawnPointCount_CT = 0;

		FOR_EACH_VEC( m_TerroristSpawnPointsMasterList, i )
		{
			SpawnPoint* pSpawnPoint = m_TerroristSpawnPointsMasterList[i];
			if ( pSpawnPoint && pSpawnPoint->IsEnabled() )
			{
				m_iSpawnPointCount_Terrorist++;

				// Store off the terrorist spawn point
				m_TerroristSpawnPoints.AddToTail( pSpawnPoint );
			}
		}

		FOR_EACH_VEC( m_CTSpawnPointsMasterList, i )
		{
			SpawnPoint* pSpawnPoint = m_CTSpawnPointsMasterList[i];
			if ( pSpawnPoint && pSpawnPoint->IsEnabled() )
			{
				m_iSpawnPointCount_CT++;

				// Store off the CT spawn point
				m_CTSpawnPoints.AddToTail( pSpawnPoint );
			}
		}

		if ( IsPlayingCoopMission() )
		{
			// reset so that T spawns always try to start from 0
			m_iNextTerroristSpawnPoint = 0;

			// make sure that there are always enough bots
			ConVarRef bot_quota( "bot_quota" );
			int nQuota = UTIL_HumansInGame( true, true ) + MIN( m_iMaxNumTerrorists, m_iSpawnPointCount_Terrorist );
			bot_quota.SetValue( nQuota );

			//DevMsg( ">> Setting bot_quota to %d\n", nQuota );

		}
		else
		{
			// Shuffle the spawn points
			ShuffleSpawnPointLists();

			// Sort the list now that the spawn points have been shuffled
			SortSpawnPointLists();
		}
	}

    void CCSGameRules::SortSpawnPointLists( void )
    {
		if ( CSGameRules()->IsPlayingGunGameProgressive() )
		{
			// Sort the spawn point lists
			m_TerroristSpawnPoints.Sort( ArmsRaceSpawnPointSortFunction );
			m_CTSpawnPoints.Sort( ArmsRaceSpawnPointSortFunction );
		}
		else
		{
			// Sort the spawn point lists
			m_TerroristSpawnPoints.Sort( SpawnPointSortFunction );
			m_CTSpawnPoints.Sort( SpawnPointSortFunction );
		}
    }

    void CCSGameRules::ShuffleSpawnPointLists( void )
    {
        // Shuffle terrorist spawn points
        VectorShuffle( m_TerroristSpawnPoints );

        // Shuffle CT spawn points
        VectorShuffle( m_CTSpawnPoints );
    }

	void CCSGameRules::ShuffleMasterSpawnPointLists( void )
	{
		// Shuffle terrorist spawn points
		VectorShuffle( m_TerroristSpawnPointsMasterList );

		// Shuffle CT spawn points
		VectorShuffle( m_CTSpawnPointsMasterList );
	}

	void CCSGameRules::SortMasterSpawnPointLists( void )
	{
		if ( CSGameRules()->IsPlayingGunGameProgressive() )
		{
			// Sort the spawn point lists
			m_TerroristSpawnPointsMasterList.Sort( ArmsRaceSpawnPointSortFunction );
			m_CTSpawnPointsMasterList.Sort( ArmsRaceSpawnPointSortFunction );

			int nSpots = 0;
			// disable all spawns over 10 because we already shoved teh arms race ones up front and
			// if there are enough, disable the ones that aren't flagged as arms race
			FOR_EACH_VEC( m_TerroristSpawnPointsMasterList, i )
			{
				SpawnPoint* pSpawnPoint = m_TerroristSpawnPointsMasterList[i];
				if ( pSpawnPoint && pSpawnPoint->IsEnabled() )
				{
					if ( IsSpawnPointValid( pSpawnPoint, NULL ) == false )
					{
						pSpawnPoint->m_bEnabled = false;
						continue;
					}

					nSpots++;
					if ( nSpots > 12 )
						pSpawnPoint->m_bEnabled = false;
				}
			}

			nSpots = 0;
			// disable for the CTs as well
			FOR_EACH_VEC( m_CTSpawnPointsMasterList, i )
			{
				SpawnPoint* pSpawnPoint = m_CTSpawnPointsMasterList[i];
				if ( pSpawnPoint && pSpawnPoint->IsEnabled() )
				{
					if ( IsSpawnPointValid( pSpawnPoint, NULL ) == false )
					{
						pSpawnPoint->m_bEnabled = false;
						continue;
					}

					nSpots++;
					if ( nSpots > 12 )
						pSpawnPoint->m_bEnabled = false;
				}
			}
		}
		else
		{

			if ( !IsPlayingCoopMission() )
				m_TerroristSpawnPointsMasterList.Sort( SpawnPointSortFunction );


			m_CTSpawnPointsMasterList.Sort( SpawnPointSortFunction );
		}
	}

    void CCSGameRules::ShufflePlayerList( CUtlVector< CCSPlayer* > &playersList )
    {
        // Shuffle players
        VectorShuffle( playersList );

		// shuffle and sort the T spawn points here too
		if ( IsPlayingCoopMission() )
		{
			ShuffleSpawnPointLists();
			SortSpawnPointLists();
		}
    }


    CBaseEntity*CCSGameRules::GetNextSpawnpoint( int teamNumber )
    {
        CBaseEntity* pRetVal = NULL;

        if ( teamNumber == TEAM_CT )
        {
            if ( m_iNextCTSpawnPoint >= m_CTSpawnPoints.Count() )
            {
                m_iNextCTSpawnPoint = 0;
            }

            if ( m_iNextCTSpawnPoint < m_CTSpawnPoints.Count() )
            {
                pRetVal = m_CTSpawnPoints[ m_iNextCTSpawnPoint ];
                m_iNextCTSpawnPoint++;
            }
        }
        else if ( teamNumber == TEAM_TERRORIST )
        {
            if ( m_iNextTerroristSpawnPoint >= m_TerroristSpawnPoints.Count() )
            {
                m_iNextTerroristSpawnPoint = 0;
            }

            if ( m_iNextTerroristSpawnPoint < m_TerroristSpawnPoints.Count() )
            {
                pRetVal = m_TerroristSpawnPoints[ m_iNextTerroristSpawnPoint ];
                m_iNextTerroristSpawnPoint++;
            }
        }

        return pRetVal;
    }

    void CCSGameRules::ShowSpawnPoints( int duration )
    {
        CBaseEntity* ent = NULL;
        
        while ( ( ent = gEntList.FindEntityByClassname( ent, "info_player_terrorist" ) ) != NULL )
        {
            if ( IsSpawnPointValid( ent, NULL ) == false )
            {
				NDebugOverlay::Box( ent->GetAbsOrigin(), VEC_HULL_MIN, VEC_HULL_MAX, 255, 0, 0, 200, duration );
            }
            else if ( !( assert_cast< SpawnPoint* >( ent )->IsEnabled() ) )
            {
                NDebugOverlay::Box( ent->GetAbsOrigin(), VEC_HULL_MIN, VEC_HULL_MAX, 255, 0, 255, 200, duration );
            }
			else
			{
				NDebugOverlay::Box( ent->GetAbsOrigin(), VEC_HULL_MIN, VEC_HULL_MAX, 0, 255, 0, 200, duration );
			}
        }

        while ( ( ent = gEntList.FindEntityByClassname( ent, "info_player_counterterrorist" ) ) != NULL )
        {
            if ( IsSpawnPointValid( ent, NULL ) == false ) 
            {
				NDebugOverlay::Box( ent->GetAbsOrigin(), VEC_HULL_MIN, VEC_HULL_MAX, 255, 0, 0, 200, duration );
			}
			else if ( !( assert_cast< SpawnPoint* >( ent )->IsEnabled() ) )
			{
				NDebugOverlay::Box( ent->GetAbsOrigin(), VEC_HULL_MIN, VEC_HULL_MAX, 255, 0, 255, 200, duration );
			}
			else
			{
				NDebugOverlay::Box( ent->GetAbsOrigin(), VEC_HULL_MIN, VEC_HULL_MAX, 0, 255, 0, 200, duration );
			}
        }

		while ( ( ent = gEntList.FindEntityByClassname( ent, "info_deathmatch_spawn" ) ) != NULL )
		{
			if ( IsSpawnPointValid( ent, NULL ) )
			{
				if ( !( assert_cast< SpawnPoint* >( ent )->IsEnabled() ) )
				{
					NDebugOverlay::Box( ent->GetAbsOrigin(), VEC_HULL_MIN, VEC_HULL_MAX, 255, 0, 255, 200, duration );
				}
				else if ( IsSpawnPointHiddenFromOtherPlayers( ent, NULL ) )
				{
					NDebugOverlay::Box( ent->GetAbsOrigin(), VEC_HULL_MIN, VEC_HULL_MAX, 0, 255, 0, 200, duration );
				}
				else
				{
					NDebugOverlay::Box( ent->GetAbsOrigin(), VEC_HULL_MIN, VEC_HULL_MAX, 255, 0, 0, 200, duration );
				}
			}
			else
			{
				NDebugOverlay::Box( ent->GetAbsOrigin(), VEC_HULL_MIN, VEC_HULL_MAX, 255, 0, 0, 200, duration );
			}
		}

		if ( CSGameRules()->IsPlayingGunGameProgressive() )
		{
			while ( ( ent = gEntList.FindEntityByClassname( ent, "info_armsrace_terrorist" ) ) != NULL )
			{
				if ( IsSpawnPointValid( ent, NULL ) == false )
				{
					NDebugOverlay::Box( ent->GetAbsOrigin(), VEC_HULL_MIN, VEC_HULL_MAX, 255, 0, 0, 200, duration );
				}
				else if ( !( assert_cast< SpawnPoint* >( ent )->IsEnabled() ) )
				{
					NDebugOverlay::Box( ent->GetAbsOrigin(), VEC_HULL_MIN, VEC_HULL_MAX, 255, 0, 255, 200, duration );
				}
				else
				{
					NDebugOverlay::Box( ent->GetAbsOrigin(), VEC_HULL_MIN, VEC_HULL_MAX, 0, 255, 0, 200, duration );
				}
			}

			while ( ( ent = gEntList.FindEntityByClassname( ent, "info_armsrace_counterterrorist" ) ) != NULL )
			{
				if ( IsSpawnPointValid( ent, NULL ) == false )
				{
					NDebugOverlay::Box( ent->GetAbsOrigin(), VEC_HULL_MIN, VEC_HULL_MAX, 255, 0, 0, 200, duration );
				}
				else if ( !( assert_cast< SpawnPoint* >( ent )->IsEnabled() ) )
				{
					NDebugOverlay::Box( ent->GetAbsOrigin(), VEC_HULL_MIN, VEC_HULL_MAX, 255, 0, 255, 200, duration );
				}
				else
				{
					NDebugOverlay::Box( ent->GetAbsOrigin(), VEC_HULL_MIN, VEC_HULL_MAX, 0, 255, 0, 200, duration );
				}
			}
		}
    }

    void CCSGameRules::CheckRestartRound( void )
    {
        // Restart the game if specified by the server
        int iRestartDelay = mp_restartgame.GetInt();

        if ( iRestartDelay > 0 )
        {
            if ( iRestartDelay > 60 )
                iRestartDelay = 60;

            // log the restart
            UTIL_LogPrintf( "World triggered \"Restart_Round_(%i_%s)\"\n", iRestartDelay, iRestartDelay == 1 ? "second" : "seconds" );

            UTIL_LogPrintf( "Team \"CT\" scored \"%i\" with \"%i\" players\n", m_match.GetCTScore(), m_iNumCT );
            UTIL_LogPrintf( "Team \"TERRORIST\" scored \"%i\" with \"%i\" players\n", m_match.GetTerroristScore(), m_iNumTerrorist );
			
#if ITEM_TIME_DATA
			CCS_GameStats.m_ItemTimeData.PrintAllAndReset();
#endif			

            // let the players know
            char strRestartDelay[64];
            Q_snprintf( strRestartDelay, sizeof( strRestartDelay ), "%d", iRestartDelay );
            UTIL_ClientPrintAll( HUD_PRINTCENTER, "#SFUI_Notice_Game_will_restart_in", strRestartDelay, iRestartDelay == 1 ? "#SFUI_Second" : "#SFUI_Seconds" );
            UTIL_ClientPrintAll( HUD_PRINTCONSOLE, "#SFUI_Notice_Game_will_restart_in", strRestartDelay, iRestartDelay == 1 ? "#SFUI_Second" : "#SFUI_Seconds" );

            m_flRestartRoundTime = gpGlobals->curtime + iRestartDelay;
            m_bCompleteReset = true;
            m_bGameRestart = true;
			m_bHasMatchStarted = false; 
            mp_restartgame.SetValue( 0 );
        }
    }

    void cc_ScrambleTeams( const CCommand& args )
    {
        if ( UTIL_IsCommandIssuedByServerAdmin() )
        {
            CCSGameRules *pRules = dynamic_cast<CCSGameRules*>( GameRules() );

            if ( pRules )
            {
                pRules->SetScrambleTeamsOnRestart( true );
                mp_restartgame.SetValue( 1 );
            }
        }
    }

    static ConCommand mp_scrambleteams( "mp_scrambleteams", cc_ScrambleTeams, "Scramble the teams and restart the game" );

    void cc_SwapTeams( const CCommand& args )
    {
        if ( UTIL_IsCommandIssuedByServerAdmin() )
        {
            CCSGameRules *pRules = dynamic_cast<CCSGameRules*>( GameRules() );

            if ( pRules )
            {
                pRules->SetSwapTeamsOnRestart( true );
                mp_restartgame.SetValue( 1 );
            }
        }
    }

    static ConCommand mp_swapteams( "mp_swapteams", cc_SwapTeams, "Swap the teams and restart the game" );	


    // sort function for the list of players that we're going to use to scramble the teams
    int ScramblePlayersSort( CCSPlayer* const *p1, CCSPlayer* const *p2 )
    {
        CCSPlayerResource *pResource = dynamic_cast< CCSPlayerResource * >( g_pPlayerResource );

        if ( pResource )
        {
            // check the priority
            if ( p1 && p2 && (*p1) && (*p2) && (*p1)->GetScore() > (*p2)->GetScore()  ) 
            {
                return 1;
            }
        }

        return -1;
    }

	//////// PAUSE
	void cc_PauseMatch( const CCommand& args )
	{
		if ( UTIL_IsCommandIssuedByServerAdmin() )
		{
			CCSGameRules *pRules = dynamic_cast<CCSGameRules*>( GameRules() );

			if ( pRules )
			{
				if ( !pRules->IsMatchWaitingForResume() )
				{
					UTIL_ClientPrintAll( HUD_PRINTCENTER, "#SFUI_Notice_Match_Will_Pause" );
				}
				pRules->SetMatchWaitingForResume( true, "mp_pause_match" );
			}
		}
	}

	static ConCommand mp_pause_match( "mp_pause_match", cc_PauseMatch, "Pause the match in the next freeze time" );	

	//////// RESUME
	void cc_ResumeMatch( const CCommand& args )
	{
		if ( UTIL_IsCommandIssuedByServerAdmin() )
		{
			CCSGameRules *pRules = dynamic_cast<CCSGameRules*>( GameRules() );

			if ( pRules && pRules->IsMatchWaitingForResume() )
			{
				pRules->SetMatchWaitingForResume( false, "mp_unpause_match" );
				if ( !pRules->IsMatchWaitingForResume() )
				{
					UTIL_ClientPrintAll( HUD_PRINTCENTER, "#SFUI_Notice_Match_Will_Resume" );
				}
				else
				{
					UTIL_ClientPrintAll( HUD_PRINTCENTER, "#SFUI_Notice_Match_Will_Pause" );
				}
			}
		}
	}

	static ConCommand mp_unpause_match( "mp_unpause_match", cc_ResumeMatch, "Resume the match" );	
	///////////


    class SetHumanTeamFunctor
    {
    public:
        SetHumanTeamFunctor( int targetTeam )
        {
            m_targetTeam = targetTeam;
            m_sourceTeam = ( m_targetTeam == TEAM_CT ) ? TEAM_TERRORIST : TEAM_CT;

            m_traitors.MakeReliable();
            m_loyalists.MakeReliable();
            m_loyalists.AddAllPlayers();
        }

        bool operator()( CBasePlayer *basePlayer )
        {
            CCSPlayer *player = ToCSPlayer( basePlayer );
            if ( !player )
                return true;

            if ( player->IsBot() )
                return true;

            if ( player->GetTeamNumber() != m_sourceTeam )
                return true;

            if ( player->State_Get() == STATE_PICKINGCLASS )
                return true;

            if ( CSGameRules()->TeamFull( m_targetTeam ) )
                return false;

            if ( CSGameRules()->TeamStacked( m_targetTeam, m_sourceTeam ) )
                return false;

            player->SwitchTeam( m_targetTeam );
            m_traitors.AddRecipient( player );
            m_loyalists.RemoveRecipient( player );

            return true;
        }

        void SendNotice( void )
        {
            if ( m_traitors.GetRecipientCount() > 0 )
            {
                UTIL_ClientPrintFilter( m_traitors, HUD_PRINTCENTER, "#SFUI_Notice_Player_Balanced" );
                UTIL_ClientPrintFilter( m_loyalists, HUD_PRINTCENTER, "#SFUI_Notice_Teams_Balanced" );
            }
        }

    private:
        int m_targetTeam;
        int m_sourceTeam;

        CRecipientFilter m_traitors;
        CRecipientFilter m_loyalists;
    };


    void CCSGameRules::MoveHumansToHumanTeam( void )
    {
        int targetTeam = GetHumanTeam();
        if ( targetTeam != TEAM_TERRORIST && targetTeam != TEAM_CT )
            return;

        SetHumanTeamFunctor setTeam( targetTeam );
        ForEachPlayer( setTeam );

        setTeam.SendNotice();
    }

	void CCSGameRules::AddDroppedWeaponToList( CWeaponCSBase *pWeapon )
	{
		//static ConVarRef weapon_max_before_cleanup( "weapon_max_before_cleanup" ); // already defined in this file
		// NOTE: dont check is removeable here because weapons just thrown aren't removeable, just clean up when we actually do the removals
		if ( pWeapon /*&& pWeapon->IsRemoveable()*/ && weapon_max_before_cleanup.GetInt() > 0 )
		{
			for ( int i = 0; i < m_weaponsDroppedInWorld.Count(); i++ )
			{
				if ( pWeapon == m_weaponsDroppedInWorld[i] )
					return;
			}

			m_weaponsDroppedInWorld.AddToTail( pWeapon );
		}
		
		while ( weapon_max_before_cleanup.GetInt() > 0 && m_weaponsDroppedInWorld.Count() > weapon_max_before_cleanup.GetInt() )
		{
			//CUtlVector< float> m_weaponsDist;
			// get the oldest 5 (or max) weapons
			int nMaxIndex = Min( 5, m_weaponsDroppedInWorld.Count() );
// 				for ( int i = 0; i < nMaxIndex; i++ )
// 				{	
// 					m_weaponsDist.AddToTail( 0 );
// 				}

			int index = 0;
			for ( int i = 0; i < nMaxIndex; i++ )  
			{
				CWeaponCSBase *pWepTemp = m_weaponsDroppedInWorld[i].Get();
				if ( pWepTemp && pWepTemp->IsRemoveable() )
				{
					// find the first weapon to be far enough away
					for ( int j = 1; j <= MAX_PLAYERS; j++ )
					{
						CBasePlayer *pPlayer = UTIL_PlayerByIndex( j );
						if ( !pPlayer )
							continue;

						float flDist = ( pWepTemp->GetAbsOrigin() - pPlayer->GetAbsOrigin() ).Length();	
						if ( flDist > 1200 )
						{
							index = i;
							break;
						}
					}

					if ( index > 0 )
						break;
				}
			}		

			CWeaponCSBase *pWep = m_weaponsDroppedInWorld[index].Get();
			if ( pWep && pWep->IsRemoveable() )
			{
				UTIL_Remove( pWep );
			}
			m_weaponsDroppedInWorld.Remove( index );
		}
	}

	void CCSGameRules::RemoveDroppedWeaponFromList( CWeaponCSBase *pWeapon )
	{
		if ( !pWeapon )
			return;

		for ( int i = 0; i < m_weaponsDroppedInWorld.Count(); i++ )
		{
			if ( pWeapon == m_weaponsDroppedInWorld[i] )
			{
				m_weaponsDroppedInWorld.Remove( i );
				return;
			}
		}
	}

    void CCSGameRules::ProcessAutoBalance( void )
    {
		// No autobalancing for people who abandon games in queued matchmaking
		if ( IsQueuedMatchmaking() )
			return;

        /*************** AUTO-BALANCE CODE *************/
        if ( mp_autoteambalance.GetInt() != 0 &&
            (m_iUnBalancedRounds >= 1) )
        {
            if ( GetHumanTeam() == TEAM_UNASSIGNED )
            {
                BalanceTeams();
            }
        }

        if ( ((m_iNumSpawnableCT - m_iNumSpawnableTerrorist) >= 2) ||
            ((m_iNumSpawnableTerrorist - m_iNumSpawnableCT) >= 2)	)
        {
            m_iUnBalancedRounds++;
        }
        else
        {
            m_iUnBalancedRounds = 0;
        }

        // Warn the players of an impending auto-balance next round...
        if ( mp_autoteambalance.GetInt() != 0 &&
            (m_iUnBalancedRounds == 1)	)
        {
            if ( GetHumanTeam() == TEAM_UNASSIGNED )
            {
                // Queue up impending auto-balance display text
                m_fAutobalanceDisplayTime = gpGlobals->curtime + AUTOBALANCE_TEXT_DELAY;
                m_AutobalanceStatus = AutobalanceStatus::NEXT_ROUND;
            }
        }
    }

    void CCSGameRules::BalanceTeams( void )
    {
		// No autobalancing for people who abandon games in queued matchmaking
		if ( IsQueuedMatchmaking() )
			return;

        int iTeamToSwap = TEAM_UNASSIGNED;
        int iNumToSwap;

        if (m_iNumCT > m_iNumTerrorist)
        {
            iTeamToSwap = TEAM_CT;
            iNumToSwap = (m_iNumCT - m_iNumTerrorist)/2;
                
        }
        else if (m_iNumTerrorist > m_iNumCT)
        {
            iTeamToSwap = TEAM_TERRORIST;
            iNumToSwap = (m_iNumTerrorist - m_iNumCT)/2;
        }
        else
        {
            return;	// Teams are even.. Get out of here.
        }

        if (iNumToSwap > 3) // Don't swap more than 3 players at a time.. This is a naive method of avoiding infinite loops.
            iNumToSwap = 3;

        int iTragetTeam = TEAM_UNASSIGNED;

        if ( iTeamToSwap == TEAM_CT )
        {
            iTragetTeam = TEAM_TERRORIST;
        }
        else if ( iTeamToSwap == TEAM_TERRORIST )
        {
            iTragetTeam = TEAM_CT;
        }
        else
        {
            // no valid team to swap
            return;
        }

        m_AutoBalanceTraitors.MakeReliable();
        m_AutoBalanceLoyalists.MakeReliable();
        m_AutoBalanceLoyalists.AddAllPlayers();

        for (int i = 0; i < iNumToSwap; i++)
        {
            // last person to join the server
            int iHighestUserID = -1;
            CCSPlayer *pPlayerToSwap = NULL;

            // check if target team is full, exit if so
            if ( TeamFull(iTragetTeam) )
                break;

            // search for player with highest UserID = most recently joined to switch over
            for ( int j = 1; j <= gpGlobals->maxClients; j++ )
            {
                CCSPlayer *pPlayer = (CCSPlayer *)UTIL_PlayerByIndex( j );

                if ( !pPlayer )
                    continue;

                CCSBot *bot = dynamic_cast< CCSBot * >(pPlayer);
                if ( bot )
                    continue; // don't swap bots - the bot system will handle that

                if ( pPlayer &&
                     ( pPlayer->GetTeamNumber() == iTeamToSwap ) && 
                     ( engine->GetPlayerUserId( pPlayer->edict() ) > iHighestUserID ) &&
                     ( pPlayer->State_Get() != STATE_PICKINGCLASS ) )
                    {
                        iHighestUserID = engine->GetPlayerUserId( pPlayer->edict() );
                        pPlayerToSwap = pPlayer;
                    }
            }

            if ( pPlayerToSwap != NULL )
            {
                m_AutoBalanceTraitors.AddRecipient( pPlayerToSwap );
                m_AutoBalanceLoyalists.RemoveRecipient( pPlayerToSwap );
                pPlayerToSwap->SwitchTeam( iTragetTeam );
            }
        }

        if ( m_AutoBalanceTraitors.GetRecipientCount() > 0 )
        {
            // Queue up traitor and loyalist display text
            m_fAutobalanceDisplayTime = gpGlobals->curtime + AUTOBALANCE_TEXT_DELAY;
            m_AutobalanceStatus = AutobalanceStatus::THIS_ROUND;
        }
    }


    void CCSGameRules::HandleScrambleTeams( void )
    {
        CCSPlayer *pCSPlayer = NULL;
        CUtlVector<CCSPlayer *> pListPlayers;

        // add all the players (that are on CT or Terrorist) to our temp list
        for ( int i = 1 ; i <= gpGlobals->maxClients ; i++ )
        {
            pCSPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );
            if ( pCSPlayer && ( pCSPlayer->GetTeamNumber() == TEAM_TERRORIST || pCSPlayer->GetTeamNumber() == TEAM_CT ) )
            {
                pListPlayers.AddToHead( pCSPlayer );
            }
        }

        // sort the list
        pListPlayers.Sort( ScramblePlayersSort );

        int team = TEAM_INVALID;
        bool assignToOpposingTeam = false;
        for ( int i = 0 ; i < pListPlayers.Count() ; i++ )
        {
            pCSPlayer = pListPlayers[i];

            if ( pCSPlayer )
            {
                //First assignment goes to random team
                //Second assignment goes to the opposite
                //Keep alternating until out of players.
                if ( !assignToOpposingTeam )
                {
                    team = ( rand() % 2 ) ? TEAM_TERRORIST : TEAM_CT;
                }
                else
                {
                    team = ( team == TEAM_TERRORIST ) ? TEAM_CT : TEAM_TERRORIST;
                }

                pCSPlayer->SwitchTeam( team );
                assignToOpposingTeam = !assignToOpposingTeam;
            }
        }	
    }

	void CCSGameRules::OnTeamsSwappedAtRoundReset()
	{
		//
		// This function is called both when the halftime ends and teams swap
		// and when cc_SwapTeams was called on the server and m_bCompleteReset is being performed
		//

		if ( m_eQueuedMatchmakingRematchState == k_EQueuedMatchmakingRematchState_VoteToRematch_CT_Surrender ||
			m_eQueuedMatchmakingRematchState == k_EQueuedMatchmakingRematchState_VoteToRematch_T_Surrender )
		{
			if ( m_eQueuedMatchmakingRematchState == k_EQueuedMatchmakingRematchState_VoteToRematch_CT_Surrender )
				m_eQueuedMatchmakingRematchState = k_EQueuedMatchmakingRematchState_VoteToRematch_T_Surrender;
			else if ( m_eQueuedMatchmakingRematchState == k_EQueuedMatchmakingRematchState_VoteToRematch_T_Surrender )
				m_eQueuedMatchmakingRematchState = k_EQueuedMatchmakingRematchState_VoteToRematch_CT_Surrender;
		}

		//
		// Flip the timeouts as well
		//
		bool bTemp;
		bTemp = m_bTerroristTimeOutActive;
		m_bTerroristTimeOutActive = m_bCTTimeOutActive;
		m_bCTTimeOutActive = bTemp;

		float flTemp;
		flTemp = m_flTerroristTimeOutRemaining;
		m_flTerroristTimeOutRemaining = m_flCTTimeOutRemaining;
		m_flCTTimeOutRemaining = flTemp;

		int nTemp = m_nTerroristTimeOuts;
		m_nTerroristTimeOuts = m_nCTTimeOuts;
		m_nCTTimeOuts = nTemp;

		g_voteControllerT->EndVoteImmediately();
		g_voteControllerCT->EndVoteImmediately();
	}

    void CCSGameRules::HandleSwapTeams( void )
    {
        CCSPlayer *pCSPlayer = NULL;
        CUtlVector<CCSPlayer *> pListPlayers;
		CUtlVector<CCSPlayer *> pListCoaches;

        // add all the players (that are on CT or Terrorist) to our temp list
        for ( int i = 1 ; i <= gpGlobals->maxClients ; i++ )
        {
            pCSPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );
			if ( pCSPlayer && ( pCSPlayer->GetTeamNumber() == TEAM_TERRORIST || pCSPlayer->GetTeamNumber() == TEAM_CT ) )
            {
                pListPlayers.AddToHead( pCSPlayer );
            }
			else if ( pCSPlayer && pCSPlayer->IsCoach() )
			{
				pListCoaches.AddToHead( pCSPlayer );
			}

        }
        
        for ( int i = 0 ; i < pListPlayers.Count() ; i++ )
        {
            pCSPlayer = pListPlayers[i];

            if ( pCSPlayer )
            {
				int currentTeam = pCSPlayer->GetTeamNumber();
                int newTeam = ( currentTeam == TEAM_TERRORIST ) ? TEAM_CT : TEAM_TERRORIST;
                pCSPlayer->SwitchTeam( newTeam );				
			}
        }

		//coaches last
		for ( int i = 0 ; i < pListCoaches.Count() ; i++ )
		{
			pCSPlayer = pListCoaches[i];

			if ( pCSPlayer )
			{
				int currentTeam = pCSPlayer->GetAssociatedTeamNumber();
				int newTeam = ( currentTeam == TEAM_TERRORIST ) ? TEAM_CT : TEAM_TERRORIST;
				pCSPlayer->m_iCoachingTeam = newTeam;		
			}
		}

		//
		// Flip the convars for custom team names and flags as well
		//

		CUtlString sTemp;
		sTemp = mp_teamname_1.GetString();
		mp_teamname_1.SetValue( mp_teamname_2.GetString() );
		mp_teamname_2.SetValue( sTemp.Get() );

		sTemp = mp_teamflag_1.GetString();
		mp_teamflag_1.SetValue( mp_teamflag_2.GetString() );
		mp_teamflag_2.SetValue( sTemp.Get() );

		sTemp = mp_teamlogo_1.GetString();
		mp_teamlogo_1.SetValue( mp_teamlogo_2.GetString() );
		mp_teamlogo_2.SetValue( sTemp.Get() );

		sTemp = mp_teammatchstat_1.GetString();
		mp_teammatchstat_1.SetValue( mp_teammatchstat_2.GetString() );
		mp_teammatchstat_2.SetValue( sTemp.Get() );

		sTemp = mp_teamscore_1.GetString();
		mp_teamscore_1.SetValue( mp_teamscore_2.GetString() );
		mp_teamscore_2.SetValue( sTemp.Get() );

		if ( ( mp_teamprediction_pct.GetInt() >= 1 ) && ( mp_teamprediction_pct.GetInt() <= 99 ) )
			mp_teamprediction_pct.SetValue( 100 - mp_teamprediction_pct.GetInt() );

		OnTeamsSwappedAtRoundReset();
    }

    
    // the following two functions cap the number of players on a team to five instead of basing it on the number of spawn points
    int CCSGameRules::MaxNumPlayersOnTerrTeam()
    {
		if ( IsPlayingCoopMission() )
			return m_iSpawnPointCount_Terrorist;

		if ( IsPlayingSurvival() )
			return MAX_PLAYERS;

		bool bRandomTSpawn = mp_randomspawn.GetInt() == 1 || mp_randomspawn.GetInt() == TEAM_TERRORIST;
        return MIN(m_iMaxNumTerrorists, bRandomTSpawn ? MAX_PLAYERS : m_iSpawnPointCount_Terrorist);
    }

    int CCSGameRules::MaxNumPlayersOnCTTeam()
    {
		bool bRandomCTSpawn = mp_randomspawn.GetInt() == 1 || mp_randomspawn.GetInt() == TEAM_CT;
        return MIN(m_iMaxNumCTs, bRandomCTSpawn ? MAX_PLAYERS : m_iSpawnPointCount_CT);
    }

    bool CCSGameRules::TeamFull( int team_id )
    {
        CheckLevelInitialized();

        switch ( team_id )
        {
        case TEAM_TERRORIST:
            return m_iNumTerrorist >= MaxNumPlayersOnTerrTeam();

        case TEAM_CT:
            return m_iNumCT >= MaxNumPlayersOnCTTeam();

        case TEAM_SPECTATOR:
            return GetGlobalTeam( TEAM_SPECTATOR )->GetNumPlayers() >= m_iSpectatorSlotCount;
        }

        return false;
    }

    bool CCSGameRules::WillTeamHaveRoomForPlayer( CCSPlayer* pThePlayer, int newTeam )
    {
        int teamSize = 0;

        for ( int clientIndex = 1; clientIndex <= gpGlobals->maxClients; clientIndex++ )
        {
            CCSPlayer *pPlayer = ( CCSPlayer* )UTIL_PlayerByIndex( clientIndex );
            if ( pPlayer && ( pPlayer != pThePlayer ) )
            {
                    if ( pPlayer->GetPendingTeamNumber() == newTeam )
                        teamSize++;
            }
        }

        bool result = false;

        switch( newTeam )
        {
            case TEAM_TERRORIST:
                result = ( teamSize < MaxNumPlayersOnTerrTeam() );
                break;

            case TEAM_CT:
                result = ( teamSize < MaxNumPlayersOnCTTeam() );
                break;

            case TEAM_SPECTATOR:
                result = ( teamSize < m_iSpectatorSlotCount );
                break;
        }

        return result;

    }

    int CCSGameRules::GetHumanTeam()
    {
        if ( FStrEq( "CT", mp_humanteam.GetString() ) )
        {
            return TEAM_CT;
        }
        else if ( FStrEq( "T", mp_humanteam.GetString() ) )
        {
            return TEAM_TERRORIST;
        }
        
        return TEAM_UNASSIGNED;
    }

    int CCSGameRules::SelectDefaultTeam( bool ignoreBots /*= false*/ )
    {
        if ( ignoreBots && ( FStrEq( cv_bot_join_team.GetString(), "T" ) || FStrEq( cv_bot_join_team.GetString(), "CT" ) ) )
        {
            ignoreBots = false;	// don't ignore bots when they can't switch teams
        }

        if ( ignoreBots && !mp_autoteambalance.GetBool() )
        {
            ignoreBots = false;	// don't ignore bots when they can't switch teams
        }

        int team = TEAM_UNASSIGNED;
        int numTerrorists = m_iNumTerrorist;
        int numCTs = m_iNumCT;
        if ( ignoreBots )
        {
            numTerrorists = UTIL_HumansOnTeam( TEAM_TERRORIST );
            numCTs = UTIL_HumansOnTeam( TEAM_CT );
        }

        // Choose the team that's lacking players
        if ( numTerrorists < numCTs )
        {
            team = TEAM_TERRORIST;
        }
        else if ( numTerrorists > numCTs )
        {
            team = TEAM_CT;
        }
        // Choose the team that's losing
        else if ( m_match.GetTerroristScore() < m_match.GetCTScore() )
        {
            team = TEAM_TERRORIST;
        }
        else if ( m_match.GetCTScore() < m_match.GetTerroristScore() )
        {
            team = TEAM_CT;
        }
        else
        {
            // Teams and scores are equal, pick a random team
            if ( random->RandomInt( 0, 1 ) == 0 )
            {
                team = TEAM_CT;
            }
            else
            {
                team = TEAM_TERRORIST;
            }
        }

        if ( TeamFull( team ) )
        {
            // Pick the opposite team
            if ( team == TEAM_TERRORIST )
            {
                team = TEAM_CT;
            }
            else
            {
                team = TEAM_TERRORIST;
            }

            // No choices left
            if ( TeamFull( team ) )
                return TEAM_UNASSIGNED;
        }

        return team;
    }

    //checks to see if the desired team is stacked, returns true if it is
    bool CCSGameRules::TeamStacked( int newTeam_id, int curTeam_id  )
    {
        //players are allowed to change to their own team
        if(newTeam_id == curTeam_id)
            return false;

        // if mp_limitteams is 0, don't check
        if ( mp_limitteams.GetInt() == 0 )
            return false;

		// Queued matchmaking code forces correct teams already, don't check stacked teams here
		if ( IsQueuedMatchmaking() )
			return false;

        switch ( newTeam_id )
        {
        case TEAM_TERRORIST:
            if(curTeam_id != TEAM_UNASSIGNED && curTeam_id != TEAM_SPECTATOR)
            {
                if((m_iNumTerrorist + 1) > (m_iNumCT + mp_limitteams.GetInt() - 1))
                    return true;
                else
                    return false;
            }
            else
            {
                if((m_iNumTerrorist + 1) > (m_iNumCT + mp_limitteams.GetInt()))
                    return true;
                else
                    return false;
            }
            break;
        case TEAM_CT:
            if(curTeam_id != TEAM_UNASSIGNED && curTeam_id != TEAM_SPECTATOR)
            {
                if((m_iNumCT + 1) > (m_iNumTerrorist + mp_limitteams.GetInt() - 1))
                    return true;
                else
                    return false;
            }
            else
            {
                if((m_iNumCT + 1) > (m_iNumTerrorist + mp_limitteams.GetInt()))
                    return true;
                else
                    return false;
            }
            break;
        }

        return false;
    }


    //=========================================================
    //=========================================================
    bool CCSGameRules::FPlayerCanRespawn( CBasePlayer *pBasePlayer )
    {
        CCSPlayer *pPlayer = ToCSPlayer( pBasePlayer );
        if ( !pPlayer )
            Error( "FPlayerCanRespawn: pPlayer=0" );

		int nTeamNum = pPlayer->GetTeamNumber();

        // Player cannot respawn twice in a round
		if ( !pPlayer->IsAbleToInstantRespawn() && !IsWarmupPeriod() )
        {
            if ( pPlayer->m_iNumSpawns > 0 && m_bFirstConnected )
                return false;
        }

        // If they're dead after the map has ended, and it's about to start the next round,
        // wait for the round restart to respawn them.
        if ( ( m_flRestartRoundTime - gpGlobals->curtime ) > MAX_TIME_TO_WAIT_BEFORE_ENTERING )
            return false;

        // Only valid team members can spawn
        if ( nTeamNum != TEAM_CT && nTeamNum != TEAM_TERRORIST )
            return false;

        // Only players with a valid class can spawn
        if ( pPlayer->GetClass() == CS_CLASS_NONE )
            return false;

        //if ( !IsPlayingGunGameProgressive() && !IsPlayingGunGameDeathmatch() && !IsWarmupPeriod() )
        if ( !pPlayer->IsAbleToInstantRespawn() && !IsWarmupPeriod() )
		{
            // Player cannot respawn until next round if more than 20 seconds in

            // Tabulate the number of players on each team.
            m_iNumCT = GetGlobalTeam( TEAM_CT )->GetNumPlayers();
            m_iNumTerrorist = GetGlobalTeam( TEAM_TERRORIST )->GetNumPlayers();

            if ( m_iNumTerrorist > 0 && m_iNumCT > 0 )
            {
                // gurjeets - Between rounds m_fRoundStartTime gets set to cur_time + freeze_time ie
                // sometime in the future. Thus, when we get here, players are still within the 
                // round restart time
                // First time into a game, however, m_fRoundStartTime is 0. On this code path,
                // the code that sets this properly only kicks in *after* players are spawned
                // This results in the msg "player_spawned" not getting sent. 
                // Hence, added the check for m_fRoundStartTime being 0
                if ( (m_fRoundStartTime != 0) && 
                    (gpGlobals->curtime > (m_fRoundStartTime + mp_join_grace_time.GetFloat() )) )
                {
                    //If this player just connected and fadetoblack is on, then maybe
                    //the server admin doesn't want him peeking around.
                    color32_s clr = {0,0,0,255};
                    if ( mp_forcecamera.GetInt() == OBS_ALLOW_NONE )
                    {
                        UTIL_ScreenFade( pPlayer, clr, 3, 3, FFADE_OUT | FFADE_STAYOUT );
                    }

                    return false;
                }
            }
        }

		if ( IsPlayingSurvival() && !IsWarmupPeriod() )
		{
			// outside of warmup, players cannot respawn in survival
			return false;
		}

        // Player cannot respawn while in the Choose Appearance menu
        //if ( pPlayer->m_iMenu == Menu_ChooseAppearance )
        //	return false;

        return true;
    }

	void CCSGameRules::LoadRoundDataInformation( char const *szFilename )
	{
		#if BACKUPSUPPORTZEROZERO
		//
		// Custom handling for loading 0:0 backup
		//
		if ( !V_strcmp( szFilename, g_szRoundBackupZeroZeroFileName ) )
		{	// Ensure that the warmup starts and stays paused
			extern ConVar mp_warmup_pausetimer;
			mp_warmup_pausetimer.SetValue( 1 );
			StartWarmup();
			return;
		}
		#endif

		KeyValues *kvSaveFile = new KeyValues( "" );
		KeyValues::AutoDelete autodelete_kvSaveFile( kvSaveFile );
		autodelete_kvSaveFile->UsesEscapeSequences( true );

		if ( !kvSaveFile->LoadFromFile( filesystem, szFilename ) )
		{
			Warning( "Failed to load file: %s\n", szFilename );
			return;
		}

		UTIL_ClientPrintAll( HUD_PRINTTALK, CFmtStr( "Restoring match backup %s, score %d:%d after round %d\n",
			kvSaveFile->GetString( "timestamp" ),
			kvSaveFile->GetInt( "FirstHalfScore/team1" ) + kvSaveFile->GetInt( "SecondHalfScore/team1" ) + kvSaveFile->GetInt( "OvertimeScore/team1" ),
			kvSaveFile->GetInt( "FirstHalfScore/team2" ) + kvSaveFile->GetInt( "SecondHalfScore/team2" ) + kvSaveFile->GetInt( "OvertimeScore/team2" ),
			kvSaveFile->GetInt( "round" ) ) );

		int numRoundsPlayed = kvSaveFile->GetInt( "round" );
		int numOvertimePlaying = kvSaveFile->GetInt( "OvertimeScore/OvertimeID" );
		bool bResetPlayerAccounts =
			( ( numRoundsPlayed < mp_maxrounds.GetInt() ) || !mp_overtime_maxrounds.GetInt() )
			? ( numRoundsPlayed == ( mp_maxrounds.GetInt() / 2 ) )
			: !( ( numRoundsPlayed - mp_maxrounds.GetInt() ) % ( mp_overtime_maxrounds.GetInt() / 2 ) );

		// Send all CT and T players to spectators for now
		int nSavedSpectatorSlotsCount = m_iSpectatorSlotCount;
		m_iSpectatorSlotCount = INT_MAX;
		m_bForceTeamChangeSilent = true;	// silence all the messages about players changing teams
		m_bLoadingRoundBackupData = true;	// make sure the game rules don't do any additional round restarts
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CCSPlayer *pPlayer = (CCSPlayer*) UTIL_PlayerByIndex( i );
			if ( pPlayer && pPlayer->IsConnected() && !pPlayer->IsBot() &&
				( ( pPlayer->GetTeamNumber() == TEAM_CT ) || ( pPlayer->GetTeamNumber() == TEAM_TERRORIST ) ||
				  ( pPlayer->GetPendingTeamNumber() == TEAM_CT ) || ( pPlayer->GetPendingTeamNumber() == TEAM_TERRORIST ) ) )
			{
				pPlayer->HandleCommand_JoinTeam( TEAM_SPECTATOR );
				pPlayer->SetPendingTeamNum( TEAM_UNASSIGNED );
			}
		}

		// Wipe cached accounts information and team assignments
		if ( !IsQueuedMatchmaking() )
		{
			m_mapQueuedMatchmakingPlayersData.PurgeAndDeleteElements();
		}

		//
		// Reset match and streak data
		//
		{
			m_iTotalRoundsPlayed = 0;

			// Reset score info
			m_match.Reset();

			int nStartLossStreak = Min( mp_starting_losses.GetInt(), mp_consecutive_loss_max.GetInt() );

			m_iNumConsecutiveTerroristLoses	= bResetPlayerAccounts ? nStartLossStreak  : kvSaveFile->GetInt( "History/NumConsecutiveTerroristLoses" );
			m_iNumConsecutiveCTLoses		= bResetPlayerAccounts ? nStartLossStreak : kvSaveFile->GetInt( "History/NumConsecutiveCTLoses" );

			m_iAccountTerrorist = bResetPlayerAccounts ? 0 : kvSaveFile->GetInt( "History/ExtraAccountTerrorist" );
			m_iAccountCT = bResetPlayerAccounts ? 0 : kvSaveFile->GetInt( "History/ExtraAccountCT" );

			m_iLoserBonus					= bResetPlayerAccounts ? 
																	( TeamCashAwardValue( TeamCashAward::LOSER_BONUS ) +
																	( nStartLossStreak * TeamCashAwardValue( TeamCashAward::LOSER_BONUS_CONSECUTIVE_ROUNDS ) ) )
												: kvSaveFile->GetInt( "History/LoserBonus" );

			// Reset hostage spawn indices
			m_arrSelectedHostageSpawnIndices.RemoveAll();
			if ( char const *szHostageSpawnIndices = kvSaveFile->GetString( "History/HostageSpawnIndices", NULL ) )
			{
				CUtlVector< char* > tagStrings;
				V_SplitString( szHostageSpawnIndices, ",", tagStrings );
				m_arrSelectedHostageSpawnIndices.EnsureCapacity( tagStrings.Count() );
				FOR_EACH_VEC( tagStrings, iTagString )
				{
					m_arrSelectedHostageSpawnIndices.AddToTail( Q_atoi( tagStrings[iTagString] ) );
				}
				tagStrings.PurgeAndDeleteElements();
			}
		}

		//
		// Process save data, disregarding team assignment
		// we'll figure out which team players are supposed to be on later
		//
		m_match.AddCTWins( kvSaveFile->GetInt( "FirstHalfScore/team1" ) );
		m_match.AddTerroristWins( kvSaveFile->GetInt( "FirstHalfScore/team2" ) );
		if ( numRoundsPlayed >= ( mp_maxrounds.GetInt() / 2 ) )
		{
			m_match.SetPhase( GAMEPHASE_PLAYING_SECOND_HALF );
			m_match.AddCTWins( kvSaveFile->GetInt( "SecondHalfScore/team1" ) );
			m_match.AddTerroristWins( kvSaveFile->GetInt( "SecondHalfScore/team2" ) );
		}
		if ( ( numRoundsPlayed >= mp_maxrounds.GetInt() ) || numOvertimePlaying )
		{
			int nOvertimeBasedOnRoundsPlayed = mp_overtime_maxrounds.GetInt() ? ( numRoundsPlayed - mp_maxrounds.GetInt() ) / mp_overtime_maxrounds.GetInt() : 0;
			numOvertimePlaying = MAX( numOvertimePlaying, nOvertimeBasedOnRoundsPlayed + 1 );
			m_match.GoToOvertime( numOvertimePlaying );

			if ( mp_overtime_maxrounds.GetInt() &&
				( ( ( numRoundsPlayed - mp_maxrounds.GetInt() ) % mp_overtime_maxrounds.GetInt() ) < ( mp_overtime_maxrounds.GetInt() / 2 ) ) )
				m_match.SetPhase( GAMEPHASE_PLAYING_FIRST_HALF );

			m_match.AddCTWins( kvSaveFile->GetInt( "OvertimeScore/team1" ) );
			m_match.AddTerroristWins( kvSaveFile->GetInt( "OvertimeScore/team2" ) );
		}

		// Switch team scores if they are supposed to be switched now
		bool bAreTeamsPlayingSwitchedSides = AreTeamsPlayingSwitchedSides();
		if ( bAreTeamsPlayingSwitchedSides )
		{
			m_match.SwapTeamScores();
		}

		for ( int r = 0; r < m_iMatchStats_RoundResults.Count(); r++ )
		{
			char szKey[30];

			V_sprintf_safe( szKey, "RoundResults/round%d", r + 1 );

			m_iMatchStats_RoundResults.GetForModify( r ) = kvSaveFile->GetInt( szKey );
		}
		for ( int r = 0; r < m_iMatchStats_PlayersAlive_T.Count(); r++ )
		{
			char szKey[30];

			V_sprintf_safe( szKey, "PlayersAliveT/round%d", r + 1 );

			m_iMatchStats_PlayersAlive_T.GetForModify( r ) = kvSaveFile->GetInt( szKey );
		}
		for ( int r = 0; r < m_iMatchStats_PlayersAlive_CT.Count(); r++ )
		{
			char szKey[30];

			V_sprintf_safe( szKey, "PlayersAliveCT/round%d", r + 1 );

			m_iMatchStats_PlayersAlive_CT.GetForModify( r ) = kvSaveFile->GetInt( szKey );
		}

		m_nTerroristTimeOuts = kvSaveFile->GetInt( "Timeouts/Terrorist" );
		m_nCTTimeOuts = kvSaveFile->GetInt( "Timeouts/CT" );

		// Now assign the players to correct teams with correct stats
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CCSPlayer *pPlayer = (CCSPlayer*) UTIL_PlayerByIndex( i );
			if ( pPlayer && pPlayer->IsConnected() && !pPlayer->IsBot() )
			{
				CSteamID steamIdHumanPlayer;
				if ( pPlayer->GetSteamID( &steamIdHumanPlayer ) )
				{
					int iTeamOrder = 0;
					KeyValues *kvPlayerData = NULL;
					for ( ; iTeamOrder < 2; ++ iTeamOrder )
					{
						kvPlayerData = kvSaveFile->FindKey( CFmtStr( "PlayersOnTeam%d/%u", iTeamOrder + 1, steamIdHumanPlayer.GetAccountID() ), false );
						if ( kvPlayerData )
							break;
					}
					if ( !kvPlayerData )
					{
						Warning( "Server checkpoint has no information for player %u: %s\n", steamIdHumanPlayer.GetAccountID(), pPlayer->GetPlayerName() );
					}
					else
					{
						int iRulesTeam = bAreTeamsPlayingSwitchedSides ? ( (iTeamOrder == 0)?TEAM_TERRORIST:TEAM_CT ) : ( (iTeamOrder == 0)?TEAM_CT:TEAM_TERRORIST );
						pPlayer->HandleCommand_JoinTeam( iRulesTeam ); // join the correct team
						pPlayer->HandleCommand_JoinClass(); // force join class now
						pPlayer->MarkAsNotReceivingMoneyNextRound(); // after loading backup we shouldn't give out extra end round money

						pPlayer->ResetFragCount();
						pPlayer->SetEnemyKillTrackInfo( kvPlayerData->GetInt( "enemyKs" ), kvPlayerData->GetInt( "enemyHSs" ), kvPlayerData->GetInt( "enemy3Ks" ), kvPlayerData->GetInt( "enemy4Ks" ), kvPlayerData->GetInt( "enemy5Ks" ), kvPlayerData->GetInt( "enemyKAg" ) );
						pPlayer->m_unTotalRoundDamageDealt = kvPlayerData->GetInt( "enemyDamageDealt" );
						pPlayer->SetEnemyFirstKills( kvPlayerData->GetInt( "firstKs" ), kvPlayerData->GetInt( "clutchKs" ) );
						pPlayer->SetEnemyWeaponKills( kvPlayerData->GetInt( "kills_weapon_pistol" ), kvPlayerData->GetInt( "kills_weapon_sniper" ) );

						pPlayer->IncrementFragCount( kvPlayerData->GetInt( "kills" ), -1 ); // -1 headshots is a special flag indicating that we are faking the call here, but it will still record kill info in QMM
						pPlayer->ResetAssistsCount();
						pPlayer->IncrementAssistsCount( kvPlayerData->GetInt( "assists" ) );
						pPlayer->ResetDeathCount();
						pPlayer->IncrementDeathCount( kvPlayerData->GetInt( "deaths" ) );

						pPlayer->SetNumMVPs( kvPlayerData->GetInt( "mvps" ) );

						pPlayer->ClearContributionScore();
						pPlayer->AddContributionScore( kvPlayerData->GetInt( "score" ) );

						pPlayer->ResetNumRoundKills();

						int iCashAccount = kvPlayerData->GetInt( "cash" );
						if ( bResetPlayerAccounts )
						{
							iCashAccount = GetOvertimePlaying() ? mp_overtime_startmoney.GetInt() : mp_startmoney.GetInt();
						}
						pPlayer->AddAccount( iCashAccount - pPlayer->GetAccountBalance(), true );

						pPlayer->RemoveAllItems( true );
						if ( !bResetPlayerAccounts )
						{
							for ( KeyValues *kvItem = kvPlayerData->FindKey( "Items" )->GetFirstValue(); kvItem; kvItem = kvItem->GetNextValue() )
							{
								pPlayer->GiveNamedItem( kvItem->GetString() );
							}

							pPlayer->SetArmorValue( kvPlayerData->GetInt( "armor" ) );
							pPlayer->m_bHasHelmet = kvPlayerData->GetBool( "helmet" );
							pPlayer->m_bHasHeavyArmor = kvPlayerData->GetBool( "heavyassaultsuit", false ); // m_bHasHeavyAssaultSuit

							if ( kvPlayerData->GetBool( "defusekit" ) && ( pPlayer->GetTeamNumber() == TEAM_CT ) )
								pPlayer->GiveDefuser();
						}

						for ( int r = 0; r < MAX_MATCH_STATS_ROUNDS; r++ )
						{
							char szKey[64] = {};

							V_snprintf( szKey, ARRAYSIZE( szKey ), "MatchStats/Kills/round%d", r + 1 );
							pPlayer->m_iMatchStats_Kills.GetForModify( r ) = ( r < numRoundsPlayed ) ? kvPlayerData->GetInt( szKey ) : 0;

							V_snprintf( szKey, ARRAYSIZE( szKey ), "MatchStats/Damage/round%d", r + 1 );
							pPlayer->m_iMatchStats_Damage.GetForModify( r ) = ( r < numRoundsPlayed ) ? kvPlayerData->GetInt( szKey ) : 0;

							V_snprintf( szKey, ARRAYSIZE( szKey ), "MatchStats/EquipmentValue/round%d", r + 1 );
							pPlayer->m_iMatchStats_EquipmentValue.GetForModify( r ) = ( r < numRoundsPlayed ) ? kvPlayerData->GetInt( szKey ) : 0;

							V_snprintf( szKey, ARRAYSIZE( szKey ), "MatchStats/MoneySaved/round%d", r + 1 );
							pPlayer->m_iMatchStats_MoneySaved.GetForModify( r ) = ( r < numRoundsPlayed ) ? kvPlayerData->GetInt( szKey ) : 0;

							V_snprintf( szKey, ARRAYSIZE( szKey ), "MatchStats/KillReward/round%d", r + 1 );
							pPlayer->m_iMatchStats_KillReward.GetForModify( r ) = ( r < numRoundsPlayed ) ? kvPlayerData->GetInt( szKey ) : 0;

							V_snprintf( szKey, ARRAYSIZE( szKey ), "MatchStats/LiveTime/round%d", r + 1 );
							pPlayer->m_iMatchStats_LiveTime.GetForModify( r ) = ( r < numRoundsPlayed ) ? kvPlayerData->GetInt( szKey ) : 0;

							V_snprintf( szKey, ARRAYSIZE( szKey ), "MatchStats/Deaths/round%d", r + 1 );
							pPlayer->m_iMatchStats_Deaths.GetForModify( r ) = ( r < numRoundsPlayed ) ? kvPlayerData->GetInt( szKey ) : 0;

							V_snprintf( szKey, ARRAYSIZE( szKey ), "MatchStats/Assists/round%d", r + 1 );
							pPlayer->m_iMatchStats_Assists.GetForModify( r ) = ( r < numRoundsPlayed ) ? kvPlayerData->GetInt( szKey ) : 0;

							V_snprintf( szKey, ARRAYSIZE( szKey ), "MatchStats/HeadShotKills/round%d", r + 1 );
							pPlayer->m_iMatchStats_HeadShotKills.GetForModify( r ) = ( r < numRoundsPlayed ) ? kvPlayerData->GetInt( szKey ) : 0;

							V_snprintf( szKey, ARRAYSIZE( szKey ), "MatchStats/Objective/round%d", r + 1 );
							pPlayer->m_iMatchStats_Objective.GetForModify( r ) = ( r < numRoundsPlayed ) ? kvPlayerData->GetInt( szKey ) : 0;

							V_snprintf( szKey, ARRAYSIZE( szKey ), "MatchStats/CashEarned/round%d", r + 1 );
							pPlayer->m_iMatchStats_CashEarned.GetForModify( r ) = ( r < numRoundsPlayed ) ? kvPlayerData->GetInt( szKey ) : 0;

							V_snprintf( szKey, ARRAYSIZE( szKey ), "MatchStats/UtilityDamage/round%d", r + 1 );
							pPlayer->m_iMatchStats_UtilityDamage.GetForModify( r ) = ( r < numRoundsPlayed ) ? kvPlayerData->GetInt( szKey ) : 0;

							V_snprintf( szKey, ARRAYSIZE( szKey ), "MatchStats/EnemiesFlashed/round%d", r + 1 );
							pPlayer->m_iMatchStats_EnemiesFlashed.GetForModify( r ) = ( r < numRoundsPlayed ) ? kvPlayerData->GetInt( szKey ) : 0;

						}

						pPlayer->m_iMatchStats_Kills_Total =                    kvPlayerData->GetInt( "MatchStats/Totals/Kills" );
						pPlayer->m_iMatchStats_Damage_Total =					kvPlayerData->GetInt( "MatchStats/Totals/Damage" );
						pPlayer->m_iMatchStats_EquipmentValue_Total =			kvPlayerData->GetInt( "MatchStats/Totals/EquipmentValue" );
						pPlayer->m_iMatchStats_KillReward_Total =				kvPlayerData->GetInt( "MatchStats/Totals/KillReward" );
						pPlayer->m_iMatchStats_LiveTime_Total =                 kvPlayerData->GetInt( "MatchStats/Totals/LiveTime" );
						pPlayer->m_iMatchStats_Deaths_Total =					kvPlayerData->GetInt( "MatchStats/Totals/Deaths" );
						pPlayer->m_iMatchStats_Assists_Total =				    kvPlayerData->GetInt( "MatchStats/Totals/Assists" );
						pPlayer->m_iMatchStats_HeadShotKills_Total =			kvPlayerData->GetInt( "MatchStats/Totals/HeadshotKills" );
						pPlayer->m_iMatchStats_Objective_Total =				kvPlayerData->GetInt( "MatchStats/Totals/Objective" );
						pPlayer->m_iMatchStats_CashEarned_Total =				kvPlayerData->GetInt( "MatchStats/Totals/CashEarned" );
						pPlayer->m_iMatchStats_UtilityDamage_Total =			kvPlayerData->GetInt( "MatchStats/Totals/UtilityDamage" );
						pPlayer->m_iMatchStats_EnemiesFlashed_Total =			kvPlayerData->GetInt( "MatchStats/Totals/EnemiesFlashed" );

						for ( int r = 0; r < SCHEMA_BASE_ITEM_MAX; r++ )
						{
							char szKey[64] = {};

							V_snprintf( szKey, ARRAYSIZE( szKey ), "WeaponPurchases/DefIndex_%d", r );
							pPlayer->m_iWeaponPurchasesThisMatch.GetForModify( r ) = kvPlayerData->GetInt( szKey, 0 );
						}

						pPlayer->m_nHeavyAssaultSuitCooldownRemaining = kvPlayerData->GetInt( "ItemCooldown/heavyassaultsuit" );

						// We've set up the player's armor properly, don't reset it
						pPlayer->m_bResetArmorNextSpawn = false;
					}
				}
			}
		}

		// Restore spectators
		m_iSpectatorSlotCount = nSavedSpectatorSlotsCount;
		m_bForceTeamChangeSilent = false;

		// Tell that we have loaded the checkpoint
		Warning( "Loaded server checkpoint %s, starting match with score %d:%d after round %d\n",
			kvSaveFile->GetString( "timestamp" ),
			kvSaveFile->GetInt( "FirstHalfScore/team1" ) + kvSaveFile->GetInt( "SecondHalfScore/team1" ),
			kvSaveFile->GetInt( "FirstHalfScore/team2" ) + kvSaveFile->GetInt( "SecondHalfScore/team2" ),
			kvSaveFile->GetInt( "round" ) );

		// Make sure we restart without warmup if warmup was active
		m_bWarmupPeriod = false;
		m_bCompleteReset = false;
		m_fWarmupPeriodStart = -1;
		m_fWarmupPeriodEnd = 0;
		m_flRestartRoundTime = 0;

		// Notify clients of potential phase change
		if ( IGameEvent * event = gameeventmanager->CreateEvent( "announce_phase_end" ) )
		{
			gameeventmanager->FireEvent( event );
		}
		m_phaseChangeAnnouncementTime = 0.0f;

		UTIL_LogPrintf( "World triggered \"Match_Reloaded\" on \"%s\"\n", STRING( gpGlobals->mapname ) );
		extern void Helper_LogPrintPlayingTeamNames();
		Helper_LogPrintPlayingTeamNames();

		if ( mp_backup_restore_load_autopause.GetBool() )
		{
			// pause the match
			SetMatchWaitingForResume( true, "mp_backup_restore_load_autopause" );
		}

		// Restart round
		EndRound();
	}

	static void Helper_CleanupStringForSaveRoundDataInformation( char *pch )
	{
		int nLength = Q_strlen( pch );
		char *pchStart = pch;
		for ( ; *pch; ++pch )
		{
			if (
				( ( pch[ 0 ] >= 'a' ) && ( pch[ 0 ] <= 'z' ) ) ||
				( ( pch[ 0 ] >= 'A' ) && ( pch[ 0 ] <= 'Z' ) ) ||
				( ( pch[ 0 ] >= '0' ) && ( pch[ 0 ] <= '9' ) )
				)
				;
			else
			{
				Q_memmove( pch, pch + 1, nLength - ( pch - pchStart ) );
				--pch;
			}
		}
	}

	void CCSGameRules::SaveRoundDataInformation( char const *szFilenameOverride )
	{
	/** Removed for partner depot **/
	}

	CCSGameRules::CQMMPlayerData_t * CCSGameRules::QueuedMatchmakingPlayersDataFindOrCreate( CCSPlayer *pPlayer )
	{
		CSteamID steamIdHumanPlayer;
		if ( !pPlayer->GetSteamID( &steamIdHumanPlayer ) )
			return NULL;
		if ( !steamIdHumanPlayer.IsValid() || !steamIdHumanPlayer.BIndividualAccount() )
			return NULL;
		if ( steamIdHumanPlayer.GetAccountID() != pPlayer->GetHumanPlayerAccountID() )
			return NULL;

		CCSGameRules::CQMMPlayerData_t *pQMM = QueuedMatchmakingPlayersDataFind( steamIdHumanPlayer.GetAccountID() );
		if ( pQMM )	// Already exists
			return pQMM;

		// There are some cases where we cannot create the player data
		// or are supposed to have all entries previously created upon GC request
		if ( IsQueuedMatchmaking() )
			return NULL;

		// Prepare for creating the entry
		bool bControllingBot = pPlayer->IsControllingBot();
		bool bTeamsArePlayingSwitchedSides = AreTeamsPlayingSwitchedSides();
		int iTeamOrder = 0;	// see "CCSGameRules::SaveRoundDataInformation" processing
		switch ( pPlayer->GetTeamNumber() )
		{
		case TEAM_CT:
		case TEAM_TERRORIST:
			iTeamOrder = ( ( pPlayer->GetTeamNumber() == TEAM_CT ) == bTeamsArePlayingSwitchedSides ) ? 1 : 0;
			break;
		default:
			return NULL;
		}

		//
		// Otherwise it is valid to create a new entry
		//
		CQMMPlayerData_t &qmmPlayerData = *new CQMMPlayerData_t;
		qmmPlayerData.m_uiPlayerAccountId = steamIdHumanPlayer.GetAccountID();
		const uint32 kNumPlayersPerSide = 5;
		qmmPlayerData.m_iDraftIndex = iTeamOrder * kNumPlayersPerSide;
		Q_strncpy( qmmPlayerData.m_chPlayerName, pPlayer->GetPlayerName(), sizeof( qmmPlayerData.m_chPlayerName ) );

		qmmPlayerData.m_numKills = bControllingBot ? pPlayer->GetBotPreControlData().m_iFrags : pPlayer->FragCount();
		qmmPlayerData.m_numAssists = bControllingBot ? pPlayer->GetBotPreControlData().m_iAssists : pPlayer->AssistsCount();
		qmmPlayerData.m_numDeaths = bControllingBot ? pPlayer->GetBotPreControlData().m_iDeaths : pPlayer->DeathCount();
		qmmPlayerData.m_numScorePoints = pPlayer->GetScore();
		qmmPlayerData.m_numMVPs = pPlayer->GetNumMVPs();
		qmmPlayerData.m_cash = bControllingBot ? pPlayer->GetBotPreControlData().m_iAccount : pPlayer->GetAccountBalance();
		qmmPlayerData.m_bReceiveNoMoneyNextRound = !pPlayer->DoesPlayerGetRoundStartMoney();

		int nKills, iEnemyKillHeadshots, iEnemy3Ks, iEnemy4Ks, iEnemy5Ks, iEnemyKillsAgg;
		pPlayer->GetEnemyKillTrackInfo( nKills, iEnemyKillHeadshots, iEnemy3Ks, iEnemy4Ks, iEnemy5Ks, iEnemyKillsAgg );
		qmmPlayerData.m_numEnemyKills = nKills;
		qmmPlayerData.m_numEnemyKillHeadshots = iEnemyKillHeadshots;
		qmmPlayerData.m_numEnemy3Ks = iEnemy3Ks;
		qmmPlayerData.m_numEnemy4Ks = iEnemy4Ks;
		qmmPlayerData.m_numEnemy5Ks = iEnemy5Ks;
		qmmPlayerData.m_numEnemyKillsAgg = iEnemyKillsAgg;

		qmmPlayerData.m_numRoundsWon = pPlayer->m_iRoundsWon;

		for ( int i = 0; i < MAX_MATCH_STATS_ROUNDS; i++ )
		{
			qmmPlayerData.m_iMatchStats_Kills[ i ] = pPlayer->m_iMatchStats_Kills.Get( i );
			qmmPlayerData.m_iMatchStats_Damage[ i ] = pPlayer->m_iMatchStats_Damage.Get( i );
			qmmPlayerData.m_iMatchStats_MoneySaved[ i ] = pPlayer->m_iMatchStats_MoneySaved.Get( i );
			qmmPlayerData.m_iMatchStats_EquipmentValue[ i ] = pPlayer->m_iMatchStats_EquipmentValue.Get( i );
			qmmPlayerData.m_iMatchStats_KillReward[ i ] = pPlayer->m_iMatchStats_KillReward.Get( i );
			qmmPlayerData.m_iMatchStats_LiveTime[ i ] = pPlayer->m_iMatchStats_LiveTime.Get( i );
			qmmPlayerData.m_iMatchStats_Deaths[ i ] = pPlayer->m_iMatchStats_Deaths.Get( i );
			qmmPlayerData.m_iMatchStats_Assists[ i ] = pPlayer->m_iMatchStats_Assists.Get( i );
			qmmPlayerData.m_iMatchStats_HeadShotKills[ i ] = pPlayer->m_iMatchStats_HeadShotKills.Get( i );
			qmmPlayerData.m_iMatchStats_Objective[ i ] = pPlayer->m_iMatchStats_Objective.Get( i );
			qmmPlayerData.m_iMatchStats_CashEarned[ i ] = pPlayer->m_iMatchStats_CashEarned.Get( i );
			qmmPlayerData.m_iMatchStats_UtilityDamage[ i ] = pPlayer->m_iMatchStats_UtilityDamage.Get( i );
			qmmPlayerData.m_iMatchStats_EnemiesFlashed[ i ] = pPlayer->m_iMatchStats_EnemiesFlashed.Get( i );
		}

		m_mapQueuedMatchmakingPlayersData.InsertOrReplace( qmmPlayerData.m_uiPlayerAccountId, &qmmPlayerData );

		return &qmmPlayerData;
	}

	void CCSGameRules::IncrementAndTerminateRound( float tmDelay, int reason )
	{
		bool roundIsAlreadyOver = ( CSGameRules()->m_iRoundWinStatus != WINNER_NONE );
		if ( roundIsAlreadyOver )
			return;

		if ( reason == Round_Draw )
		{
			m_match.IncrementRound( 1 );
		}
		else if ( reason == Terrorists_Win )
		{
			m_match.AddTerroristWins( 1 );
		}
		else if ( reason == CTs_Win )
		{
			m_match.AddCTWins( 1 );
			if ( IsPlayingCoopMission() )
			{
				if ( CTeam *pTeam = GetGlobalTeam( TEAM_TERRORIST ) )
					pTeam->MarkSurrendered();
			}
		}
		else
		{
			Assert( !"IncrementAndTerminateRound not implemented for the reason passed" );
			return;
		}

		TerminateRound( tmDelay, reason );
	}

	static inline int GetNumPlayers( CTeam *pTeam )
	{
		return pTeam ? pTeam->GetNumPlayers() : 0;
	}

    void CCSGameRules::TerminateRound( float tmDelay, int iReason, TerminateRoundDelayedTeamCashAwards_t *pCashAwards /*= NULL*/, int numCashAwards /*= 0*/ )
    {
		if ( m_iRoundWinStatus != WINNER_NONE )
			return;

		VPROF_BUDGET( "CCSGameRules::TerminateRound", VPROF_BUDGETGROUP_GAME );
		variant_t emptyVariant;
        int iWinnerTeam = WINNER_NONE;
        const char *text = "UNKNOWN";

		// extend the time a bit if we have items to display
		float flItemDelay = tmDelay;
// 		if ( m_ItemsPtrDroppedDuringMatch.Count() > 0 && !IsPlayingAnyCompetitiveStrictRuleset() )
// 			flItemDelay += MIN(5, m_ItemsPtrDroppedDuringMatch.Count())*0.75;
                
        // UTIL_ClientPrintAll( HUD_PRINTCENTER, sentence );

		if ( m_eQueuedMatchmakingRematchState == k_EQueuedMatchmakingRematchState_VoteToRematch_T_Surrender )
		{
			iReason = Terrorists_Surrender;
			if ( CTeam *pTeam = GetGlobalTeam( TEAM_TERRORIST ) )
				pTeam->MarkSurrendered();
		}
		else if ( m_eQueuedMatchmakingRematchState == k_EQueuedMatchmakingRematchState_VoteToRematch_CT_Surrender )
		{
			iReason = CTs_Surrender;
			if ( CTeam *pTeam = GetGlobalTeam( TEAM_CT ) )
				pTeam->MarkSurrendered();
		}

        switch ( iReason )
        {
// Terror wins:
            case Target_Bombed:	
                text = "#SFUI_Notice_Target_Bombed";
                iWinnerTeam = WINNER_TER;
                break;

            case Terrorists_Escaped:
                text = "#SFUI_Notice_Terrorists_Escaped";
                iWinnerTeam = WINNER_TER;
                break;

            case Terrorists_Win:
                text = "#SFUI_Notice_Terrorists_Win";
                iWinnerTeam = WINNER_TER;
                break;

            case Hostages_Not_Rescued:
                text = "#SFUI_Notice_Hostages_Not_Rescued";
                iWinnerTeam = WINNER_TER;
                break;

			case Terrorists_Planted:
				text = "#SFUI_Notice_Terrorists_Planted";
				iWinnerTeam = WINNER_TER;
				break;

			case CTs_ReachedHostage:
				text = "#SFUI_Notice_CTs_ReachedHostage";
				iWinnerTeam = WINNER_CT;
				break;

			case CTs_PreventEscape:
                text = "#SFUI_Notice_CTs_PreventEscape";
                iWinnerTeam = WINNER_CT;
                break;

            case Escaping_Terrorists_Neutralized:
                text = "#SFUI_Notice_Escaping_Terrorists_Neutralized";
                iWinnerTeam = WINNER_CT;
                break;

            case Bomb_Defused:
                text = "#SFUI_Notice_Bomb_Defused";
                iWinnerTeam = WINNER_CT;
                break;

            case CTs_Win:
                text = "#SFUI_Notice_CTs_Win";
                iWinnerTeam = WINNER_CT;
                break;

            case All_Hostages_Rescued:
                text = "#SFUI_Notice_All_Hostages_Rescued";
                iWinnerTeam = WINNER_CT;
                break;

            case Target_Saved:
                text = "#SFUI_Notice_Target_Saved";
                iWinnerTeam = WINNER_CT;
                break;

            case Terrorists_Not_Escaped:
                text = "#SFUI_Notice_Terrorists_Not_Escaped";
                iWinnerTeam = WINNER_CT;
                break;
// no winners:
            case Game_Commencing:
                text = "#SFUI_Notice_Game_Commencing";
                iWinnerTeam = WINNER_DRAW;
                break;

            case Round_Draw:
                text = "#SFUI_Notice_Round_Draw";
                iWinnerTeam = WINNER_DRAW;
                break;

            case Terrorists_Surrender:
                text = "#SFUI_Notice_Terrorists_Surrender";
                iWinnerTeam = WINNER_CT;
                break;

            case CTs_Surrender:
                text = "#SFUI_Notice_CTs_Surrender";
                iWinnerTeam = WINNER_TER;
                break;

			case Survival_Win:
				text = "#SFUI_Notice_Survival_Win";
                iWinnerTeam = WINNER_TER;
				break;

			case Survival_Draw:
				text = "#SFUI_Notice_Survival_Draw";
                iWinnerTeam = WINNER_DRAW;
				break;

            default:
                DevMsg("TerminateRound: unknown round end ID %i\n", iReason );
                break;
        }

        m_iRoundWinStatus = iWinnerTeam;
		m_eRoundWinReason = iReason;
        m_flRestartRoundTime = gpGlobals->curtime + flItemDelay;

		if ( ( m_iRoundWinStatus == WINNER_TER ) || ( m_iRoundWinStatus == WINNER_CT ) )
		{	// If the round is ending and there's a pending hook for MVP calculation
			// and player scoring then run these calculations here before the round
			// officially ends and the game goes on to save round stats or goes to
			// intermission and submits match outcome
			if ( m_pfnCalculateEndOfRoundMVPHook )
				m_pfnCalculateEndOfRoundMVPHook->CalculateEndOfRoundMVP();
			else	// run default MVP rules without any special hooks
				CalculateEndOfRoundMVP();
		}

		if ( ( ( m_iRoundWinStatus == WINNER_TER ) || ( m_iRoundWinStatus == WINNER_CT ) )
			&& Helper_mp_backup_round_IsEnabled() )
		{
			SaveRoundDataInformation();
		}

        if ( iWinnerTeam == WINNER_CT )
        {
            for( int i=0;i<g_Hostages.Count();i++ )
                g_Hostages[i]->AcceptInput( "CTsWin", NULL, NULL, emptyVariant, 0 );
        }
        else if ( iWinnerTeam == WINNER_TER )
        {
            for( int i=0;i<g_Hostages.Count();i++ )
                g_Hostages[i]->AcceptInput( "TerroristsWin", NULL, NULL, emptyVariant, 0 );
        }
        else
        {
            Assert( iWinnerTeam == WINNER_NONE || iWinnerTeam == WINNER_DRAW );
        }

        for ( int i = 1; i <= gpGlobals->maxClients; i++ )
        {
            CCSPlayer* pPlayer = (CCSPlayer*)UTIL_PlayerByIndex( i );
            if (pPlayer)
            {
                // have all players do any end of round bookkeeping
                pPlayer->HandleEndOfRound();
            }
        }

        // [tj] Check for any non-player-specific achievements.
        ProcessEndOfRoundAchievements(iWinnerTeam, iReason);

        if( iReason != Game_Commencing )
        {
            // [pfreese] Setup and send win panel event (primarily funfact data)

            FunFact funfact;
            funfact.szLocalizationToken = "";
            funfact.iPlayer = 0;
            funfact.iData1 = 0;
            funfact.iData2 = 0;
            funfact.iData3 = 0;

            m_pFunFactManager->GetRoundEndFunFact( iWinnerTeam, (e_RoundEndReason)iReason, funfact);

            //Send all the info needed for the win panel
            IGameEvent *winEvent = gameeventmanager->CreateEvent( "cs_win_panel_round" );

            if ( winEvent )
            {
                // determine what categories to send
                if ( GetRoundRemainingTime() <= 0 )
                {
                    // timer expired, defenders win
                    // show total time that was defended
                    winEvent->SetBool( "show_timer_defend", true );
                    winEvent->SetInt( "timer_time", m_iRoundTime );
                }
                else
                {
                    // attackers win
                    // show time it took for them to win
                    winEvent->SetBool( "show_timer_attack", true );

                    int iTimeElapsed = GetRoundElapsedTime();
                    winEvent->SetInt( "timer_time", iTimeElapsed );
                }

                winEvent->SetInt( "final_event", iReason );

                // Set the fun fact data in the event
                winEvent->SetString( "funfact_token", funfact.szLocalizationToken);
                winEvent->SetInt( "funfact_player", funfact.iPlayer );
                winEvent->SetInt( "funfact_data1", funfact.iData1 );
                winEvent->SetInt( "funfact_data2", funfact.iData2 );
                winEvent->SetInt( "funfact_data3", funfact.iData3 );
                gameeventmanager->FireEvent( winEvent );
            }
        }

        // [tj] Inform players that the round is over
        for ( int i = 1; i <= gpGlobals->maxClients; i++ )
        {
            CCSPlayer *pPlayer = (CCSPlayer*) UTIL_PlayerByIndex( i );
            if(pPlayer)
            {
                pPlayer->OnRoundEnd(iWinnerTeam, iReason);

				if ( ( pPlayer->GetTeamNumber() == iWinnerTeam ) && ( IsPlayingClassic() ) )
				{
					++ pPlayer->m_iRoundsWon;

					// Keep track in QMM data
					if ( pPlayer->GetHumanPlayerAccountID() )
					{
						if ( CCSGameRules::CQMMPlayerData_t *pQMM = CSGameRules()->QueuedMatchmakingPlayersDataFind( pPlayer->GetHumanPlayerAccountID() ) )
						{
							pQMM->m_numRoundsWon = pPlayer->m_iRoundsWon;
						}
					}
				}

				if ( IsPlayingCoopGuardian() && 
					 (iReason == CTs_ReachedHostage || iReason == Terrorists_Planted) &&
					 (pPlayer->GetTeamNumber() == TEAM_CT || pPlayer->GetTeamNumber() == TEAM_TERRORIST) )
				{
					color32_s clr = {0,0,0,255};
					UTIL_ScreenFade( pPlayer, clr, 3, 3, FFADE_OUT | FFADE_STAYOUT );
				}

				if ( IsPlayingCoopMission() )
				{
					color32_s clr = { 0, 0, 0, 255 };
					UTIL_ScreenFade( pPlayer, clr, 3, 3, FFADE_OUT | FFADE_STAYOUT );
				}
			}
        }

		if ( IsPlayingCoopGuardian() && ( iReason == CTs_ReachedHostage || iReason == Terrorists_Planted ) )
			FreezePlayers();

		if ( IsPlayingCoopMission() )
		{
			CGameCoopMissionManager *pManager = GetCoopMissionManager();
			// we really dont have one
			if ( !pManager )
				DevMsg( "Coop mission map is missing a game_coopmission_manager entity. You can't fire round ended outputs without it!\n" );
			else if ( iReason == Terrorists_Win )
			{
				if ( GetRoundRemainingTime() <= 0 )
					pManager->SetRoundLostTime();
				else
					pManager->SetRoundLostKilled();
			}
		}

        IGameEvent * event = gameeventmanager->CreateEvent( "round_end" );
        if ( event )
        {
            event->SetInt( "winner", iWinnerTeam );
            event->SetInt( "reason", iReason );
            event->SetString( "message", text );
            event->SetInt( "priority", 6 ); // round_end
			event->SetInt( "player_count", GetNumPlayers( GetGlobalTeam( TEAM_CT ) ) + GetNumPlayers( GetGlobalTeam( TEAM_TERRORIST ) ) );

			// Check if there are Terrorists alive
			bool bTerroristsAlive = false;
			bool bCTsAlive = false;
			for ( int i = 1; i <= MAX_PLAYERS; i++ )
			{
				CCSPlayer* pCheckPlayer = (CCSPlayer*)UTIL_PlayerByIndex(i);
				if ( !pCheckPlayer || pCheckPlayer->IsAlive() == false )
					continue;

				if ( pCheckPlayer->GetTeamNumber() == TEAM_TERRORIST )
				{
					bTerroristsAlive = true;
				}
				else if ( pCheckPlayer->GetTeamNumber() == TEAM_CT )
				{
					bCTsAlive = true;
				}

				if ( bCTsAlive && bTerroristsAlive && !IsLastRoundBeforeHalfTime() )
				{
					// If there are still players on both teams alive, don't play end of round music,
					// as it distracts from the action.
					event->SetInt("nomusic", 1);
					break;
				}
			}

			if ( ( iWinnerTeam == TEAM_CT ) && ( iReason == Bomb_Defused ) )
			{
				float flSecondsTillDetonationRemaining = -1.0f;
				for ( int i = 1; i <= MAX_PLAYERS; i++ )
				{
					CCSPlayer* pCheckPlayer = (CCSPlayer*)UTIL_PlayerByIndex( i );
					if ( !pCheckPlayer )
						continue;
					if ( ( pCheckPlayer->GetTeamNumber() == TEAM_TERRORIST )
						&& pCheckPlayer->IsAlive() )
					{
						break;
					}
					if ( pCheckPlayer->GetTeamNumber() == TEAM_CT )
					{
						float flTimeTillDetonation = pCheckPlayer->GetDefusedBombWithThisTimeRemaining();
						if ( flTimeTillDetonation > flSecondsTillDetonationRemaining )
							flSecondsTillDetonationRemaining = flTimeTillDetonation;
					}
				}


				if ( !bTerroristsAlive && ( flSecondsTillDetonationRemaining >= 0 ) )
				{	// Can only play legacy radio if no terrorists are alive (so all players are focused on the defuse)
					if ( flSecondsTillDetonationRemaining < 1.0f )
					{	// Always play the coolest brag for a close defuse
						event->SetInt( "legacy", 3 );
					}
					else if ( flSecondsTillDetonationRemaining < 3.0f )
					{
						if ( RandomFloat() < 0.75f )	// 75% chance for a medium tier brag
							event->SetInt( "legacy", 2 );
					}
					else if ( flSecondsTillDetonationRemaining < 7.0f )
					{
						if ( RandomFloat() < 0.5f )	// 50% chance for a low tier brag
							event->SetInt( "legacy", 1 );
					}
				}
			}
            gameeventmanager->FireEvent( event );
        }

        if ( ( iReason == CTs_Surrender || iReason == Terrorists_Surrender ) && !IsQueuedMatchmaking() )
        {
            m_phaseChangeAnnouncementTime = gpGlobals->curtime + mp_win_panel_display_time.GetInt();
            GoToIntermission();
        }

        if ( ( GetMapRemainingTime() == 0.0f ) && !IsQueuedMatchmaking() )
        {
            UTIL_LogPrintf("World triggered \"Intermission_Time_Limit\"\n");
            m_phaseChangeAnnouncementTime = gpGlobals->curtime + mp_win_panel_display_time.GetInt();
            GoToIntermission();
        }

        // Only update bot difficulty if playing in an Online-game (In offline mode, bot difficulty is set by user)
        if ( !IsPlayingOffline() )
        {
            // Determine the difficulty level that the bots should be at to compete
            ModifyRealtimeBotDifficulty();
        }

        if ( ( static_cast< e_RoundEndReason > ( iReason ) != Game_Commencing ) && !m_bLoadingRoundBackupData )
        {
            // Perform round-related processing at the point when a round winner has been determined
            RoundWin( pCashAwards, numCashAwards );
        }

		// This is a fantastic opportunity to submit round results to GC
		if ( iReason != Game_Commencing )
		{
			ReportRoundEndStatsToGC();

			if ( Helper_ShouldBroadcastCoopScoreLeaderboardData() )
			{
				CReliableBroadcastRecipientFilter filter;
				CCSUsrMsg_ScoreLeaderboardData msg;
				Helper_FillScoreLeaderboardData( *msg.mutable_data() );
				SendUserMessage( filter, CS_UM_ScoreLeaderboardData, msg );
			}

			if ( IsPlayingCoopGuardian()
				&& ( iWinnerTeam == GetCoopHumanTeamNumber() ) )
			{
				// If human team wins then end the match right now!
				m_match.SetPhase( GAMEPHASE_MATCH_ENDED );
				m_phaseChangeAnnouncementTime = gpGlobals->curtime + mp_win_panel_display_time.GetInt();
				GoToIntermission();
			}
			else if ( IsPlayingCoopMission() && iWinnerTeam == TEAM_CT )
			{
				if ( IsPlayingCoopMission() )
				{
					CGameCoopMissionManager *pManager = GetCoopMissionManager();
					// we really dont have one
					if ( !pManager )
						DevMsg( "Coop mission map is missing a game_coopmission_manager entity. You can't fire MissionCompleted outputs without it!\n" );
					else
						pManager->SetMissionCompleted();
				}

				// If human team wins then end the match right now!
				m_match.SetPhase( GAMEPHASE_MATCH_ENDED );
				m_phaseChangeAnnouncementTime = gpGlobals->curtime + 1.5;
				GoToIntermission();
			}
		}

		if ( iReason == Game_Commencing )
		{
			m_bWarmupPeriod = true;
		}
    }

	ConVar mp_endmatch_votenextmap_wargames_modes( "mp_endmatch_votenextmap_wargames_modes", "armsrace demolition flyingscoutsman", FCVAR_RELEASE, "Modes available for endmatch voting during War Games. Separate names with spaces." );
	ConVar mp_endmatch_votenextmap_wargames_nummaps( "mp_endmatch_votenextmap_wargames_nummaps", "3", FCVAR_RELEASE, "Maximum number of maps to include in endmatch voting during War Games" );
	ConVar mp_endmatch_votenextmap_wargames_nummodes( "mp_endmatch_votenextmap_wargames_nummodes", "1", FCVAR_RELEASE, "Maximum number of other War Games to include in endmatch voting during War Games" );

	void CCSGameRules::CreateEndMatchMapGroupVoteOptions( void )
	{
		VPROF_BUDGET( "CCSGameRules::CreateEndMatchMapGroupVoteOptions", VPROF_BUDGETGROUP_GAME );
		if ( sv_skirmish_id.GetInt() != 0 )
		{
			// use mp_endmatch_votenextmap_wargames_* convars to build voting panel

			const char* mapGroupName = STRING( gpGlobals->mapGroupName );
			int nOptions = 0;

			if ( g_pGameTypes && mapGroupName && g_pGameTypes->IsValidMapGroupName( mapGroupName ) )
			{
				const CUtlStringList* mapsInGroup = g_pGameTypes->GetMapGroupMapList( mapGroupName );
				int numMapsInGroup = mapsInGroup ? mapsInGroup->Count() : 0;
				int numMapsToInclude = mp_endmatch_votenextmap_wargames_nummaps.GetInt();
				if ( numMapsInGroup >= 1 && numMapsToInclude > 0 )
				{
					const char* szCurMapName = STRING( gpGlobals->mapname );
					int curMapIndex = -1;

					// Find our current map in the group
					FOR_EACH_VEC( *mapsInGroup, i )
					{
						if ( !V_stricmp( mapsInGroup->Element( i ), szCurMapName ) )
						{
							curMapIndex = i;
							break;
						}
					}

					// If we are somehow playing a map that isn't in the mapgroup,
					// choose a random map to start from.
					if ( curMapIndex < 0 )
						curMapIndex = RandomInt( 0, numMapsInGroup - 1 );

					// List the next N maps in the mapgroup
					if ( nOptions < MAX_ENDMATCH_VOTE_PANELS && mp_endmatch_votenextmap_keepcurrent.GetBool() )
					{
						m_nEndMatchMapGroupVoteTypes.Set( nOptions, kEndMatchVoteType_MapIndexInMapGroup );
						m_nEndMatchMapGroupVoteOptions.Set( nOptions, curMapIndex );
						nOptions++;
						numMapsToInclude--;
					}

					int nextMapIndex = ( curMapIndex + 1 ) % numMapsInGroup;
					while( nOptions < MAX_ENDMATCH_VOTE_PANELS
							&& nextMapIndex != curMapIndex
							&& numMapsToInclude > 0 )
					{
						m_nEndMatchMapGroupVoteTypes.Set( nOptions, kEndMatchVoteType_MapIndexInMapGroup );
						m_nEndMatchMapGroupVoteOptions.Set( nOptions, nextMapIndex );
						nOptions++;
						nextMapIndex = ( nextMapIndex + 1 ) % numMapsInGroup;
						numMapsToInclude--;
					}
				}
			}

			// Also add mode change options if we have space for at least 1
			if ( nOptions < MAX_ENDMATCH_VOTE_PANELS - 1 )
			{
				// Add separator
				m_nEndMatchMapGroupVoteTypes.Set( nOptions, kEndMatchVoteType_Separator );
				m_nEndMatchMapGroupVoteOptions.Set( nOptions, kEndMatchVoteSeparator_ChangeMode );
				nOptions++;

				// Build valid skirmish list
				CUtlVector<int> validSkirmishIds;
				const char* modeList = mp_endmatch_votenextmap_wargames_modes.GetString();
				if ( modeList && *modeList )
				{
					char szName[256];

					while( ( modeList = engine->ParseFile( modeList, szName, sizeof( szName ) ) ) != nullptr )
					{
						const CSkirmishModeDefinition* pSkirmishDef = GetItemSchema()->GetSkirmishModeDefinitionByName( szName );
						if ( !pSkirmishDef )
						{
							Warning( "Invalid skirmish '%s' in mp_endmatch_votenextmap_wargames_modes\n", szName );
							continue;
						}

						// Don't include the current skirmish, that's handled above by just changing maps
						if ( (int)pSkirmishDef->GetID() == sv_skirmish_id.GetInt() )
							continue;

						validSkirmishIds.AddToTail( pSkirmishDef->GetID() );
					}
				}


				// Choose a random set of skirmishes from the valid list up to mp_endmatch_votenextmap_wargames_nummodes
				int numSkirmishesToInclude = mp_endmatch_votenextmap_wargames_nummodes.GetInt();
				for( ; validSkirmishIds.Count() > 0	&& numSkirmishesToInclude > 0 && nOptions < MAX_ENDMATCH_VOTE_PANELS; numSkirmishesToInclude-- )
				{
					int validSkirmishIndex = RandomInt( 0, validSkirmishIds.Count() - 1 );
					int skirmishId = validSkirmishIds[validSkirmishIndex];
					validSkirmishIds.FastRemove( validSkirmishIndex );

					m_nEndMatchMapGroupVoteTypes.Set( nOptions, kEndMatchVoteType_ChangeSkirmish );
					m_nEndMatchMapGroupVoteOptions.Set( nOptions, skirmishId );
					nOptions++;
				}
			}

			// clear other options
			for ( ; nOptions < MAX_ENDMATCH_VOTE_PANELS; ++nOptions )
			{
				m_nEndMatchMapGroupVoteTypes.Set( nOptions, kEndMatchVoteType_Invalid );
				m_nEndMatchMapGroupVoteOptions.Set( nOptions, -1 );
			}
			return;
		}

		CUtlVector<int> arrVoteCandidates;
		if ( g_pGameTypes )
		{
			const char* mapGroupName = STRING( gpGlobals->mapGroupName );
#if 0 // debug for map voting
			mapGroupName = "mg_active";
#endif
			const CUtlStringList* mapsInGroup = g_pGameTypes->GetMapGroupMapList( mapGroupName );

			if ( mapsInGroup )
			{
				int nCount = mapsInGroup->Count();
				int nCurrentMapIndex = -1;
				for ( int iMap = 0; iMap < nCount; ++ iMap )
				{
					arrVoteCandidates.AddToTail( iMap );
					if ( !V_stricmp( mapsInGroup->Element( iMap ), STRING( gpGlobals->mapname ) ) )
						nCurrentMapIndex = iMap;
				}

				// Remove current map from pool if mp_endmatch_votenextmap_keepcurrent is set to 0.
				if ( !mp_endmatch_votenextmap_keepcurrent.GetBool() && nCurrentMapIndex >= 0 )
					arrVoteCandidates.Remove( nCurrentMapIndex );
				
				while ( arrVoteCandidates.Count() > 10 )
				{
					int nRemoveIndex = RandomInt( 0, arrVoteCandidates.Count() - 1 );
					if ( mp_endmatch_votenextmap_keepcurrent.GetBool() && ( arrVoteCandidates[nRemoveIndex] == nCurrentMapIndex ) )
					{
						nRemoveIndex ++;
						nRemoveIndex %= arrVoteCandidates.Count();
					}
					arrVoteCandidates.Remove( nRemoveIndex );
				}
			}
		}

		for ( int iVoteOption = 0; iVoteOption < MAX_ENDMATCH_VOTE_PANELS; ++iVoteOption )
		{
			if ( arrVoteCandidates.IsValidIndex( iVoteOption ) )
			{
				m_nEndMatchMapGroupVoteTypes.Set( iVoteOption, kEndMatchVoteType_MapIndexInMapGroup );
				m_nEndMatchMapGroupVoteOptions.Set( iVoteOption, arrVoteCandidates[iVoteOption] );
			}
			else
			{
				m_nEndMatchMapGroupVoteTypes.Set( iVoteOption, kEndMatchVoteType_Invalid );
				m_nEndMatchMapGroupVoteOptions.Set( iVoteOption, -1 );
			}
		}
	}

	void CCSGameRules::ReportRoundEndStatsToGC( CMsgGCCStrike15_v2_MatchmakingServerRoundStats **ppAllocateStats )
	{
	/** Removed for partner depot **/
	}

    // Helper to determine if all players on a team are playing for the same clan
    bool CCSGameRules::IsClanTeam( CTeam *pTeam )
    {
        uint32 iTeamClan = 0;
		bool bTeamInitialized = false;

        for ( int iPlayer = 0; iPlayer < pTeam->GetNumPlayers(); iPlayer++ )
        {
            CBasePlayer *pPlayer = pTeam->GetPlayer( iPlayer );
            if ( !pPlayer )
                return false;

			if ( pPlayer->IsBot() )
				continue;

            const char *pClanID = engine->GetClientConVarValue( pPlayer->entindex(), "cl_clanid" );
            uint32 iPlayerClan = atoi( pClanID );

			// Initialize the team clan
			if ( !bTeamInitialized )
			{
				iTeamClan = iPlayerClan;
				bTeamInitialized = true;
			}

            if ( iPlayerClan != iTeamClan || iPlayerClan == 0 )
				return false;

        }
        return iTeamClan != 0;
    }
    void CCSGameRules::ModifyRealtimeBotDifficulty( CCSPlayer* pOnlyBotToProcess /* = NULL */ )
    {
		if ( !sv_auto_adjust_bot_difficulty.GetBool() )
			return;

        float fAvgPlayerCS = CalculateAveragePlayerContributionScore();
        float fAvgBotCS;
        
        if ( !sv_compute_per_bot_difficulty.GetBool() )
        {
            fAvgBotCS = CalculateAverageBotContributionScore();
        }
        else
        {
            fAvgBotCS = 0.0f;
        }

        float fLowWindowExtent = fAvgPlayerCS + bot_autodifficulty_threshold_low.GetFloat();
        float fHighWindowExtent = fAvgPlayerCS + bot_autodifficulty_threshold_high.GetFloat();

        // Ensure the high/low window extents are accurate
        if ( fLowWindowExtent > fHighWindowExtent )
        {
            float swapval = fLowWindowExtent;
            fLowWindowExtent = fHighWindowExtent;
            fHighWindowExtent = swapval;
        }

        int nextBotDifficulty = -1;

        if ( sv_compute_per_bot_difficulty.GetBool() )
        {
            // Compare incoming target contribution score to levels of difficulty, and choose a level for each bot
            for ( int i = 1; i <= gpGlobals->maxClients; i++ )
            {
                CCSPlayer* pPlayer = ( CCSPlayer* )UTIL_PlayerByIndex( i );
                if ( !pPlayer )
                {
                    continue;
                }

                if ( pOnlyBotToProcess && pPlayer != pOnlyBotToProcess )
                {
                    continue;
                }

                if ( pPlayer->IsBot() )
                {
                    CCSBot* pBot = dynamic_cast< CCSBot* >( pPlayer );

                    if ( pBot )
                    {
                        const BotProfile* pProfile = pBot->GetProfile();

                        if ( pProfile  && TheBotProfiles )
                        {
                            float cScore = ( float )pPlayer->GetContributionScore();

                            nextBotDifficulty = -1;
                            DevMsg( "+++++Bot %s has CS = %f. Low = %f, High = %f\n", pPlayer->GetPlayerName(), cScore, fLowWindowExtent, fHighWindowExtent );

                            if ( cScore < fLowWindowExtent )
                            {
                                // Bot should have higher difficulty
                                DevMsg( "+++++Bot %s with CS = %f, should increase its difficulty\n", pPlayer->GetPlayerName(), cScore );

                                nextBotDifficulty = pProfile->GetMaxDifficulty() + 1;

                                // Ensure new difficulty level is valid
                                if ( nextBotDifficulty >= NUM_DIFFICULTY_LEVELS )
                                {
                                    nextBotDifficulty = NUM_DIFFICULTY_LEVELS - 1;
                                }
                            }
                            else if ( cScore > fHighWindowExtent )
                            {
                                // Bot should have lower difficulty
                                DevMsg( "+++++Bot %s with CS = %f, should decrease its difficulty\n", pPlayer->GetPlayerName(), cScore );

                                nextBotDifficulty = pProfile->GetMaxDifficulty() - 1;

                                // Ensure new difficulty level is valid
                                if ( nextBotDifficulty < BOT_EASY )
                                {
                                    nextBotDifficulty = BOT_EASY;
                                }
                            }

							// In queue matchmaking mode bots are always obeying bot manager difficulty
							if ( IsQueuedMatchmaking() )
								nextBotDifficulty = CCSBotManager::GetDifficultyLevel();

                            if ( nextBotDifficulty >= BOT_EASY )
                            {
                                // Change the bot's new profile based on desired difficulty
                                const BotProfile* pNewProfileData = TheBotProfiles->GetRandomProfile( ( BotDifficultyType )nextBotDifficulty, pBot->GetTeamNumber(), WEAPONTYPE_UNKNOWN, true );

                                if ( NULL == pNewProfileData )
                                {
                                    Warning( "-----No profile found to match search criteria.  Not updating this bot's difficulty.");
                                }
                                else
                                {
                                    // Change the parameters of the bot's profile to match the new difficulty settings
                                    DevMsg( "+++++Bot %s with Max Difficulty %d will become Bot %s with Max Difficulty %d\n", pProfile->GetName(), pProfile->GetMaxDifficulty(), pNewProfileData->GetName(), pNewProfileData->GetMaxDifficulty() );
                                    pBot->Initialize( pNewProfileData, pBot->GetTeamNumber() );
                                }
                            }
                        }
                    }
                }
            }
        }
        else
        {
            // Handle the alternate case for all bots changing difficulty as a group (all bots will have identical difficulty)
            // DevMsg( "+++++Average Bot Contribution Score = %f\n", fAvgBotCS );

            if ( fAvgBotCS < fLowWindowExtent )
            {
                // Bots should have higher difficulty
                nextBotDifficulty = -1;
                for ( int i = 1; i <= gpGlobals->maxClients; i++ )
                {
                    CCSPlayer* pPlayer = ( CCSPlayer* )UTIL_PlayerByIndex( i );

                    if ( pOnlyBotToProcess && pPlayer != pOnlyBotToProcess )
                    {
                        continue;
                    }

                    if ( pPlayer && pPlayer->IsBot() )
                    {
                        CCSBot* pBot = dynamic_cast< CCSBot* >( pPlayer );

                        if ( pBot )
                        {
                            const BotProfile* pProfile = pBot->GetProfile();

                            if ( pProfile  && TheBotProfiles && nextBotDifficulty == -1 )
                            {
                                nextBotDifficulty = pProfile->GetMaxDifficulty() + 1;

                                if ( nextBotDifficulty > BOT_EXPERT )
                                {
                                    nextBotDifficulty = BOT_EXPERT;
                                }
                            }

							// In queue matchmaking mode bots are always obeying bot manager difficulty
							if ( IsQueuedMatchmaking() )
								nextBotDifficulty = CCSBotManager::GetDifficultyLevel();

                            if ( nextBotDifficulty > -1 )
                            {
                                // Have a valid difficulty level, so apply it to each bot
                                const BotProfile* pNewProfileData = TheBotProfiles->GetRandomProfile( ( BotDifficultyType )nextBotDifficulty, pBot->GetTeamNumber(), WEAPONTYPE_UNKNOWN, true );

                                if ( NULL == pNewProfileData )
                                {
                                    Warning( "-----No profile found to match search criteria.  Not updating this bot's difficulty.");
                                }
                                else
                                {
                                    // Change the parameters of the bot's profile to match the new difficulty settings
                                    DevMsg( "+++++Bot %s with Max Difficulty %d will become Bot %s with Max Difficulty %d\n", pProfile->GetName(), pProfile->GetMaxDifficulty(), pNewProfileData->GetName(), pNewProfileData->GetMaxDifficulty() );
                                    pBot->Initialize( pNewProfileData, pBot->GetTeamNumber() );
                                }
                            }
                        }
                    }
                }
            }
            else if ( fAvgBotCS > fHighWindowExtent )
            {
                // Bots should have lower difficulty
                nextBotDifficulty = -1;
                for ( int i = 1; i <= gpGlobals->maxClients; i++ )
                {
                    CCSPlayer* pPlayer = ( CCSPlayer* )UTIL_PlayerByIndex( i );

                    if ( pOnlyBotToProcess && pPlayer != pOnlyBotToProcess )
                    {
                        continue;
                    }

                    if ( pPlayer && pPlayer->IsBot() )
                    {
                        CCSBot* pBot = dynamic_cast< CCSBot* >( pPlayer );

                        if ( pBot )
                        {
                            const BotProfile* pProfile = pBot->GetProfile();

                            if ( pProfile  && TheBotProfiles && nextBotDifficulty == -1 )
                            {
                                nextBotDifficulty = pProfile->GetMaxDifficulty() - 1;

                                // Ensure new difficulty level is valid
                                if ( nextBotDifficulty < BOT_EASY )
                                {
                                    nextBotDifficulty = BOT_EASY;
                                }
                            }
							
							// In queue matchmaking mode bots are always obeying bot manager difficulty
							if ( IsQueuedMatchmaking() )
								nextBotDifficulty = CCSBotManager::GetDifficultyLevel();

                            if ( nextBotDifficulty > -1 && ( pProfile->GetMaxDifficulty() != nextBotDifficulty ) )
                            {
                                // Have a valid difficulty level, so apply it to each bot
                                const BotProfile* pNewProfileData = TheBotProfiles->GetRandomProfile( ( BotDifficultyType )nextBotDifficulty, pBot->GetTeamNumber(), WEAPONTYPE_UNKNOWN, true );

                                if ( NULL == pNewProfileData )
                                {
                                    Warning( "-----No profile found to match search criteria.  Not updating this bot's difficulty.");
                                }
                                else
                                {
                                    // Change the parameters of the bot's profile to match the new difficulty settings
                                    DevMsg( "+++++Bot %s with Max Difficulty %d will become Bot %s with Max Difficulty %d\n", pProfile->GetName(), pProfile->GetMaxDifficulty( ), pNewProfileData->GetName(), pNewProfileData->GetMaxDifficulty() );
                                    pBot->Initialize( pNewProfileData, pBot->GetTeamNumber() );
                                }
                            }
                        }
                    }
                }
            }
        }

        if ( !IsPlayingOffline() )
        {
            fAvgBotCS = CalculateAverageBotContributionScore();
            //DevMsg( "Average Bot Difficulty = %f\n", fAvgBotCS );

            // Store max bot difficulty in the convar representing the player's input device
            sv_bot_difficulty_kbm.SetValue( fAvgBotCS );
        }
    }

    float CCSGameRules::CalculateAveragePlayerContributionScore( void )
    {
        // Loop through all players and get average human contribution score
        int cscoreTotal = 0;
        int numHumanPlayers = 0;
        float avgHumanContributionScore = 0.0f;

        for ( int i = 1; i <= gpGlobals->maxClients; i++ )
        {
            CCSPlayer* pPlayer = ( CCSPlayer* )UTIL_PlayerByIndex( i );

            if ( !pPlayer )
            {
                continue;
            }

            if ( !pPlayer->IsBot() )
            {
                cscoreTotal += pPlayer->GetContributionScore();
                numHumanPlayers++;
            }
        }

        if ( numHumanPlayers > 0 )
        {
            avgHumanContributionScore = ( float )cscoreTotal / ( float )numHumanPlayers;
        }

        return avgHumanContributionScore;
    }

    float CCSGameRules::CalculateAverageBotContributionScore( void )
    {
        // Loop through all players and get average bot contribution score
        int cscoreTotal = 0;
        int numBots = 0;
        float avgBotContributionScore = 0.0f;

        for ( int i = 1; i <= gpGlobals->maxClients; i++ )
        {
            CCSPlayer* pPlayer = ( CCSPlayer* )UTIL_PlayerByIndex( i );

            if ( !pPlayer )
            {
                continue;
            }

            if ( pPlayer->IsBot() )
            {
                cscoreTotal += pPlayer->GetContributionScore();
                numBots++;
            }
        }

        if ( numBots > 0 )
        {
            avgBotContributionScore = ( float )cscoreTotal / ( float )numBots;
        }

        return avgBotContributionScore;
    }

    // [tj] This is where we check non-player-specific that occur at the end of the round
    void CCSGameRules::ProcessEndOfRoundAchievements(int iWinnerTeam, int iReason)
    {			
        if (iWinnerTeam == WINNER_CT || iWinnerTeam == WINNER_TER)
        {
            int losingTeamId = (iWinnerTeam == TEAM_CT) ? TEAM_TERRORIST : TEAM_CT;
            CTeam* losingTeam = GetGlobalTeam(losingTeamId);

            
            //Check for players we should ignore when checking team size.
            int ignoreCount = 0;
            
            for ( int i = 1; i <= gpGlobals->maxClients; i++ )
            {
                CCSPlayer* pPlayer = (CCSPlayer*)UTIL_PlayerByIndex( i );
                if (pPlayer)
                {
                    if( IsPlayingGunGameProgressive() )
                        UpdateMatchStats(pPlayer,iWinnerTeam);

                    int teamNum = pPlayer->GetTeamNumber();
                    if ( teamNum == losingTeamId )
                    {
                        if (pPlayer->WasNotKilledNaturally())
                        {
                            ignoreCount++;
                        }
                    }
                }
            }


            // [tj] Check extermination with no losses achievement
            if ((iReason == CTs_Win && m_bNoCTsKilled || iReason == Terrorists_Win && m_bNoTerroristsKilled) 
                && losingTeam && losingTeam->GetNumPlayers() - ignoreCount >= AchievementConsts::DefaultMinOpponentsForAchievement)
            {
                CTeam *pTeam = GetGlobalTeam( iWinnerTeam );

                for ( int iPlayer=0; iPlayer < pTeam->GetNumPlayers(); iPlayer++ )
                {
                    CCSPlayer *pPlayer = ToCSPlayer( pTeam->GetPlayer( iPlayer ) );
                    Assert( pPlayer );
                    if ( !pPlayer )
                        continue;

                    pPlayer->AwardAchievement(CSLosslessExtermination);
                }
            }

            // [tj] Check flawless victory achievement - currently requiring extermination
            if ((iReason == CTs_Win && m_bNoCTsDamaged || iReason == Terrorists_Win && m_bNoTerroristsDamaged)
                && losingTeam && losingTeam->GetNumPlayers() - ignoreCount >= AchievementConsts::DefaultMinOpponentsForAchievement
                && !CSGameRules()->IsPlayingGunGameProgressive() )
            {
                CTeam *pTeam = GetGlobalTeam( iWinnerTeam );

                for ( int iPlayer=0; iPlayer < pTeam->GetNumPlayers(); iPlayer++ )
                {
                    CCSPlayer *pPlayer = ToCSPlayer( pTeam->GetPlayer( iPlayer ) );
                    Assert( pPlayer );
                    if ( !pPlayer )
                        continue;

                    pPlayer->AwardAchievement(CSFlawlessVictory);
                }
            }

            // [tj] Check bloodless victory achievement
            if ((iWinnerTeam == TEAM_TERRORIST && m_bNoCTsKilled || iWinnerTeam == TEAM_CT && m_bNoTerroristsKilled)
                && losingTeam && losingTeam->GetNumPlayers() >= AchievementConsts::DefaultMinOpponentsForAchievement)
            {
                CTeam *pTeam = GetGlobalTeam( iWinnerTeam );

                for ( int iPlayer=0; iPlayer < pTeam->GetNumPlayers(); iPlayer++ )
                {
                    CCSPlayer *pPlayer = ToCSPlayer( pTeam->GetPlayer( iPlayer ) );
                    Assert( pPlayer );
                    if ( !pPlayer )
                        continue;

                    pPlayer->AwardAchievement(CSBloodlessVictory);
                }
            }
        }
    }



    //[tj] Counts the number of players in each category in the struct (dead, alive, etc...)
    void CCSGameRules::GetPlayerCounts(TeamPlayerCounts teamCounts[TEAM_MAXCOUNT])
    {
        memset(teamCounts, 0, sizeof(TeamPlayerCounts) * TEAM_MAXCOUNT);

        for ( int i = 1; i <= gpGlobals->maxClients; i++ )
        {
            CCSPlayer* pPlayer = (CCSPlayer*)UTIL_PlayerByIndex( i );
            if (pPlayer)
            {
                int iTeam = pPlayer->GetTeamNumber();

                if (iTeam >= 0 && iTeam < TEAM_MAXCOUNT)
                {
                    ++teamCounts[iTeam].totalPlayers;
                    if (pPlayer->IsAlive())
                    {
                        ++teamCounts[iTeam].totalAlivePlayers;
                    }
                    else
                    {
                        ++teamCounts[iTeam].totalDeadPlayers;

                        //If the player has joined a team bit isn't in the game yet
                        if (pPlayer->State_Get() == STATE_PICKINGCLASS)
                        {
                            ++teamCounts[iTeam].unenteredPlayers;
                        }
                        else if (pPlayer->WasNotKilledNaturally())
                        {
                            ++teamCounts[iTeam].suicidedPlayers;
                        }
                        else
                        {
                            ++teamCounts[iTeam].killedPlayers;
                        }						
                    }
                }
            }
        }
    }

    void CCSGameRules::CheckMapConditions()
    {
        // Check to see if this map has a bomb target in it
        if ( gEntList.FindEntityByClassname( NULL, "func_bomb_target" ) )
        {
			// this is a bit hacky, but it makes it so the bomb stuff only shows up on mission 3 of the coop mission
			if ( IsPlayingCoopMission() && mp_anyone_can_pickup_c4.GetBool() == false )
			{
				m_bMapHasBombTarget = false;
				m_bMapHasBombZone = false;
			}
			else
			{
				m_bMapHasBombTarget = true;
				m_bMapHasBombZone = true;
			}
        }
        else if ( gEntList.FindEntityByClassname( NULL, "info_bomb_target" ) )
        {
            m_bMapHasBombTarget		= true;
            m_bMapHasBombZone		= false;
        }
        else
        {
            m_bMapHasBombTarget		= false;
            m_bMapHasBombZone		= false;
        }

        // See if the map has func_buyzone entities
        // Used by CBasePlayer::HandleSignals() to support maps without these entities
        if ( gEntList.FindEntityByClassname( NULL, "func_buyzone" ) )
        {
            m_bMapHasBuyZone = true;
        }
        else
        {
            m_bMapHasBuyZone = false;
        }

        // Check to see if this map has hostage rescue zones
		m_bMapHasRescueZone = IHostageRescueZone::AutoList().Count() > 0;

        // GOOSEMAN : See if this map has func_escapezone entities
        if ( gEntList.FindEntityByClassname( NULL, "func_escapezone" ) )
        {
            m_bMapHasEscapeZone = true;
        }
        else
        {
            m_bMapHasEscapeZone = false;
        }
    }


    void CCSGameRules::SwapAllPlayers()
    {
        // MOTODO we have to make sure that enought spaning points exits
        Assert ( 0 );
        for ( int i = 1; i <= gpGlobals->maxClients; i++ )
        {
            /* CCSPlayer *pPlayer = CCSPlayer::Instance( i );
            if ( pPlayer && !FNullEnt( pPlayer->edict() ) )
                pPlayer->SwitchTeam(); */
        }

        m_match.SwapTeamScores();
    }

	void CCSGameRules::ResetMatchStats()
	{

		for ( int r = 0; r < MAX_MATCH_STATS_ROUNDS; r++ )
		{
			m_iMatchStats_RoundResults.GetForModify( r ) = 0;
			m_iMatchStats_PlayersAlive_T.GetForModify( r ) = 0x3F;
			m_iMatchStats_PlayersAlive_CT.GetForModify( r ) = 0x3F;
		}

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CCSPlayer *pPlayer = ( CCSPlayer* )UTIL_PlayerByIndex( i );
			if ( pPlayer )
			{
				for ( int r = 0; r < MAX_MATCH_STATS_ROUNDS; r++ )
				{
					pPlayer->m_iMatchStats_Kills.GetForModify( r ) = 0;
					pPlayer->m_iMatchStats_Damage.GetForModify( r ) = 0;
					pPlayer->m_iMatchStats_EquipmentValue.GetForModify( r ) = 0;
					pPlayer->m_iMatchStats_MoneySaved.GetForModify( r ) = 0;
					pPlayer->m_iMatchStats_KillReward.GetForModify( r ) = 0;
					pPlayer->m_iMatchStats_LiveTime.GetForModify( r ) = 0;
					pPlayer->m_iMatchStats_Deaths.GetForModify( r ) = 0;
					pPlayer->m_iMatchStats_Assists.GetForModify( r ) = 0;
					pPlayer->m_iMatchStats_HeadShotKills.GetForModify( r ) = 0;
					pPlayer->m_iMatchStats_Objective.GetForModify( r ) = 0;
					pPlayer->m_iMatchStats_CashEarned.GetForModify( r ) = 0;
					pPlayer->m_iMatchStats_UtilityDamage.GetForModify( r ) = 0;
					pPlayer->m_iMatchStats_EnemiesFlashed.GetForModify( r ) = 0;
				}
			}
		}
	}

	void CCSGameRules::ResetMatchStatsTotals()
	{
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CCSPlayer *pPlayer = ( CCSPlayer* )UTIL_PlayerByIndex( i );
			if ( pPlayer )
			{
				pPlayer->m_iMatchStats_Kills_Total = 0;
				pPlayer->m_iMatchStats_Damage_Total = 0;
				pPlayer->m_iMatchStats_EquipmentValue_Total = 0;
				pPlayer->m_iMatchStats_KillReward_Total = 0;
				pPlayer->m_iMatchStats_LiveTime_Total = 0;
				pPlayer->m_iMatchStats_Deaths_Total = 0;
				pPlayer->m_iMatchStats_Assists_Total = 0;
				pPlayer->m_iMatchStats_HeadShotKills_Total = 0;
				pPlayer->m_iMatchStats_Objective_Total = 0;
				pPlayer->m_iMatchStats_CashEarned_Total = 0;
				pPlayer->m_iMatchStats_UtilityDamage_Total = 0;
				pPlayer->m_iMatchStats_EnemiesFlashed_Total = 0;
			}
		}
	}


	// reset player scores, team scores, and player controls, restart round; useful for running a demo
    void CCSGameRules::ResetForTradeshow( void )
    {
        for ( int i = 1; i <= gpGlobals->maxClients; i++ )
        {
            CCSPlayer *pPlayer = CCSPlayer::Instance( i );
            if ( pPlayer )
            {
                pPlayer->Reset( true );
            }
        }

        m_match.Reset();

        IGameEvent * event = gameeventmanager->CreateEvent( "reset_player_controls" );
        if ( event )
        {
            gameeventmanager->FireEvent( event );
        }

        ClearGunGameData();

        EndRound();
    }


    bool CS_FindInList( const char **pStrings, const char *pToFind )
    {
        return FindInList( pStrings, pToFind );
    }

    void CCSGameRules::CleanUpMap()
    {
        if (IsLogoMap())
            return;

		VPROF_BUDGET( "CCSGameRules::CleanUpMap", VPROF_BUDGETGROUP_GAME );
		// Recreate all the map entities from the map data (preserving their indices),
        // then remove everything else except the players.

		// first go through the current players and see if they are parented to something
		// if so, unparent them before we might remove their parent and cause a crash
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CCSPlayer *pPlayer = CCSPlayer::Instance( i );

			if ( pPlayer && !FNullEnt( pPlayer->edict() ) )
			{
				if ( pPlayer->GetParent() )
					pPlayer->SetParent( NULL );
			}
		}

        // Get rid of all entities except players.
        CBaseEntity *pCur = gEntList.FirstEnt();
        while ( pCur )
        {
            CWeaponCSBase *pWeapon = dynamic_cast< CWeaponCSBase* >( pCur );
            CBombTarget *pBombTarg = dynamic_cast< CBombTarget* >( pCur );
            CHostageRescueZone *pRescueZone = dynamic_cast< CHostageRescueZone* >( pCur );

            // Weapons with owners don't want to be removed..
            if ( pWeapon )
            {
                // [dwenger] Handle round restart processing for the weapon.
                pWeapon->OnRoundRestart();

                if ( m_bCompleteReset || pWeapon->ShouldRemoveOnRoundRestart() )
                {
                    UTIL_Remove( pCur );
                }
            }
            // bomb targets have a re-init function to call
            else if ( pBombTarg )
            {
                pBombTarg->ReInitOnRoundStart();
            }
            else if ( pRescueZone )
            {
                pRescueZone->ReInitOnRoundStart();
            }
            // remove entities that has to be restored on roundrestart (breakables etc)
            else if ( !CS_FindInList( s_PreserveEnts, pCur->GetClassname() ) )
            {
                if( !Commentary_IsCommentaryEntity( pCur ) ) //leave commentary alone
                {
                    UTIL_Remove( pCur );
                }
            }
            
            pCur = gEntList.NextEnt( pCur );
        }
        
        // Really remove the entities so we can have access to their slots below.
        gEntList.CleanupDeleteList();

        // Cancel all queued events, in case a func_bomb_target fired some delayed outputs that
        // could kill respawning CTs
        g_EventQueue.Clear();

        // Now reload the map entities.
        class CCSMapEntityFilter : public IMapEntityFilter
        {
        public:
            virtual bool ShouldCreateEntity( const char *pClassname )
            {
                // Don't recreate the preserved entities.
                if ( !CS_FindInList( s_PreserveEnts, pClassname ) )
                {
                    return true;
                }
                else
                {
                    // Increment our iterator since it's not going to call CreateNextEntity for this ent.
                    if ( m_iIterator != g_MapEntityRefs.InvalidIndex() )
                        m_iIterator = g_MapEntityRefs.Next( m_iIterator );
                
                    return false;
                }
            }


            virtual CBaseEntity* CreateNextEntity( const char *pClassname )
            {
                if ( m_iIterator == g_MapEntityRefs.InvalidIndex() )
                {
                    // This shouldn't be possible. When we loaded the map, it should have used 
                    // CCSMapLoadEntityFilter, which should have built the g_MapEntityRefs list
                    // with the same list of entities we're referring to here.
                    Assert( false );
                    return NULL;
                }
                else
                {
                    CMapEntityRef &ref = g_MapEntityRefs[m_iIterator];
                    m_iIterator = g_MapEntityRefs.Next( m_iIterator );	// Seek to the next entity.

                    if ( ref.m_iEdict == -1 || INDEXENT( ref.m_iEdict ) )
                    {
                        // Doh! The entity was delete and its slot was reused.
                        // Just use any old edict slot. This case sucks because we lose the baseline.
                        return CreateEntityByName( pClassname );
                    }
                    else
                    {
                        // Cool, the slot where this entity was is free again (most likely, the entity was 
                        // freed above). Now create an entity with this specific index.
                        return CreateEntityByName( pClassname, ref.m_iEdict );
                    }
                }
            }

        public:
            int m_iIterator; // Iterator into g_MapEntityRefs.
        };
        CCSMapEntityFilter filter;
        filter.m_iIterator = g_MapEntityRefs.Head();

        // DO NOT CALL SPAWN ON info_node ENTITIES!

        MapEntity_ParseAllEntities( engine->GetMapEntitiesString(), &filter, true );


        // this kind of sucks.  on ps3, we need to delete all physics props that are not set to debris to fix a crash, 
        // but we don't know if the prop has the debris flag until after it's been removed and then recreated again.
        // that's why we have to loop through all of the entities again here.
        if ( IsPS3() || engine->IsDedicatedServerForPS3() )
        {
            int nNumPhysPropsDeleted = 0;
            CBaseEntity *pPhysCur = gEntList.FirstEnt();
            while ( pPhysCur )
            {
                CPhysicsProp *pPhysProp = dynamic_cast< CPhysicsProp* >( pPhysCur );
                if ( pPhysProp && !pPhysProp->HasSpawnFlags( SF_PHYSPROP_DEBRIS ) )
                {
                    UTIL_Remove( pPhysCur );
                    nNumPhysPropsDeleted++;
                }

                pPhysCur = gEntList.NextEnt( pPhysCur );
            }

            if ( nNumPhysPropsDeleted > 0 )
                Msg( "DELETED %d prop_physics or prop_physics_multiplayer that were not set to debris!!\n", nNumPhysPropsDeleted );
        }


    }


    CCSPlayer *CCSGameRules::IsThereABomber()
    {
        for ( int i = 1; i <= gpGlobals->maxClients; i++ )
        {
            CCSPlayer *pPlayer = CCSPlayer::Instance( i );

            if ( pPlayer && !FNullEnt( pPlayer->edict() ) )
            {
                if ( pPlayer->GetTeamNumber() == TEAM_CT )
                    continue;

                if ( pPlayer->HasC4() )
                     return pPlayer; //There you are.
            }
        }

        //Didn't find a bomber.
        return NULL;
    }


    void CCSGameRules::EndRound()
    {
        // fake a round end
        CSGameRules()->TerminateRound( 0.0f, Round_Draw );
    }

    CBaseEntity *CCSGameRules::GetPlayerSpawnSpot( CBasePlayer *pPlayer )
    {
		// we don't want to get a spawn if we are already in the process of spawning with one because
		// it increments the spawn number without actually spawning and messes up the spawn priority
		CCSPlayer* pCSPlayer = ToCSPlayer( pPlayer );
		if ( !pCSPlayer || pCSPlayer->IsPlayerSpawning() )
			return NULL;

		pCSPlayer->SetPlayerSpawning( true );
		if ( IsPlayingSurvival() ) // Survival players spawn differently
		{
			return NULL; // this return type is unused?
		}

        // get valid spawn point
		CBaseEntity *pSpawnSpot = pPlayer->EntSelectSpawnPoint();

		if ( pPlayer->IsBot() && pPlayer->GetTeamNumber() == TEAM_TERRORIST )
		{
// 			// disable the spawn after someone spawn here in coop
			SpawnPointCoopEnemy *pEnemySpawnSpot = dynamic_cast< SpawnPointCoopEnemy* >( pSpawnSpot );
			if ( pEnemySpawnSpot )
			{
				 if ( IsPlayingCoopMission() )
				{
					CCSBot* pBot = static_cast< CCSBot* >( pPlayer );
					if ( pBot )
					{
						pBot->SetLastCoopSpawnPoint( pEnemySpawnSpot );
					}
				}
 			}
		}

	    // drop down to ground
		Vector GroundPos = DropToGround( pPlayer, pSpawnSpot->GetAbsOrigin() + Vector( 0, 0, 16 ), VEC_HULL_MIN + Vector( -4, -4, 0 ), VEC_HULL_MAX + Vector( 4, 4, 0 ) );

	    // Move the player to the place it said.
		pPlayer->Teleport( &GroundPos, &pSpawnSpot->GetLocalAngles(), &vec3_origin );
		pPlayer->m_Local.m_viewPunchAngle = vec3_angle;
		return pSpawnSpot;
    }
    
    // checks if the spot is clear of players
    bool CCSGameRules::IsSpawnPointValid( CBaseEntity *pSpot, CBasePlayer *pPlayer )
    {
		if ( !pSpot )
			return false;

        if ( !pSpot->IsTriggered( pPlayer ) )
            return false;

        Vector mins = GetViewVectors()->m_vHullMin;
        Vector maxs = GetViewVectors()->m_vHullMax;

        Vector vTestMins = pSpot->GetAbsOrigin() + mins;
        Vector vTestMaxs = pSpot->GetAbsOrigin() + maxs;
        
        // First test the starting origin.
        CTraceFilterSimple traceFilter( pPlayer, COLLISION_GROUP_PLAYER  );
        if ( !UTIL_IsSpaceEmpty( pPlayer, vTestMins, vTestMaxs, MASK_SOLID, &traceFilter ) )
			return false;

		// Test against other players potentially occupying this spot
		for ( int k = 1; k <= gpGlobals->maxClients; ++ k )
		{
			CBasePlayer *pOther = UTIL_PlayerByIndex( k );
			if ( !pOther ) continue;
			if ( pOther == pPlayer ) continue;
			if ( ( pOther->GetTeamNumber() != TEAM_TERRORIST )
				&& ( pOther->GetTeamNumber() != TEAM_CT ) )
				continue;

			if ( ( pOther->GetAbsOrigin().AsVector2D() - pSpot->GetAbsOrigin().AsVector2D() ).IsZero() )
				return false;
		}

		return true;
    }

	bool CCSGameRules::IsSpawnPointHiddenFromOtherPlayers( CBaseEntity *pSpot, CBasePlayer *pPlayer, int nHideFromTeam )
	{
		Vector vecSpot = pSpot->GetAbsOrigin() + Vector( 0, 0, 32 );
		if ( nHideFromTeam > 0 )
		{
			if ( nHideFromTeam == TEAM_CT && UTIL_IsVisibleToTeam( vecSpot, TEAM_CT ) )
				return false;
			else if ( nHideFromTeam == TEAM_TERRORIST && UTIL_IsVisibleToTeam( vecSpot, TEAM_TERRORIST ) )
				return false;
		}
		else if ( nHideFromTeam == 0 && ( UTIL_IsVisibleToTeam( vecSpot, TEAM_CT ) ) || 
			( UTIL_IsVisibleToTeam( vecSpot, TEAM_TERRORIST ) ) )
			return false;

		return true;
	}


    bool CCSGameRules::IsThereABomb()
    {
        bool bBombFound = false;

        /* are there any bombs, either laying around, or in someone's inventory? */
        if( gEntList.FindEntityByClassname( NULL, WEAPON_C4_CLASSNAME ) != 0 )
        {
            bBombFound = true;
        }
        /* what about planted bombs!? */
        else if( gEntList.FindEntityByClassname( NULL, PLANTED_C4_CLASSNAME ) != 0 )
        {
            bBombFound = true;
        }
        
        return bBombFound;
    }

    void CCSGameRules::HostageTouched()
    {
        if( gpGlobals->curtime > m_flNextHostageAnnouncement && m_iRoundWinStatus == WINNER_NONE )
        {
            //BroadcastSound( "Event.HostageTouched" );
            m_flNextHostageAnnouncement = gpGlobals->curtime + 60.0;
        }	

		m_bHasHostageBeenTouched = true;

		if ( IsPlayingCoopGuardian() )
			CTsReachedHostageRoundEndCheck();
    }

    void CCSGameRules::CreateStandardEntities()
    {
        // Create the player resource
        g_pPlayerResource = (CPlayerResource*)CBaseEntity::Create( "cs_player_manager", vec3_origin, vec3_angle );
    
        // Create the entity that will send our data to the client.
#ifdef DBGFLAG_ASSERT
        CBaseEntity *pEnt = 
#endif
            CBaseEntity::Create( "cs_gamerules", vec3_origin, vec3_angle );
        Assert( pEnt );

		g_voteControllerGlobal =	static_cast< CVoteController *>( CBaseEntity::Create("vote_controller", vec3_origin, vec3_angle) );
		g_voteControllerCT =		static_cast< CVoteController *>( CBaseEntity::Create("vote_controller", vec3_origin, vec3_angle) );
		g_voteControllerT =			static_cast< CVoteController *>( CBaseEntity::Create("vote_controller", vec3_origin, vec3_angle) );	
        // Vote Issue classes are handled/cleaned-up by g_voteControllers
		bool bKickVoteIssueAllowed = true;
		if ( IsQueuedMatchmaking() &&
			( MatchmakingGameTypeGameMaxPlayers( MatchmakingGameTypeToGame( sm_QueuedServerReservation.game_type() ) ) <= 5 ) )
			bKickVoteIssueAllowed = false;
		if ( bKickVoteIssueAllowed )
		{
			NewTeamIssue< CKickIssue >();
		}
        NewGlobalIssue< CRestartGameIssue >();
		NewGlobalIssue< CChangeLevelIssue >();
		NewGlobalIssue< CNextLevelIssue >();

		if ( IsPlayingAnyCompetitiveStrictRuleset() )
		{
			NewTeamIssue< CStartTimeOutIssue >();
		}

		static char const * s_pchTournamentServer = CommandLine()->ParmValue( "-tournament", ( char const * ) NULL );
		if ( s_pchTournamentServer && IsQueuedMatchmaking() )
		{
			NewGlobalIssue< CPauseMatchIssue >();
			NewGlobalIssue< CUnpauseMatchIssue >();
			NewGlobalIssue< CLoadBackupIssue >();
			NewGlobalIssue< CReadyForMatchIssue >();
			NewGlobalIssue< CNotReadyForMatchIssue >();
		}

		if ( IsQueuedMatchmaking() )
		{
			// new CQueuedMatchmakingRematch;
			// new CQueuedMatchmakingContinue;
			NewTeamIssue< CSurrender >();
		}
		else
		{
			NewGlobalIssue< CScrambleTeams >();
			NewGlobalIssue< CSwapTeams >();
			// new CSurrender;
		}
    }



#endif	// CLIENT_DLL

    ConVar cash_team_terrorist_win_bomb(
        "cash_team_terrorist_win_bomb",
        "3500",
		FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_NOTIFY);

    ConVar cash_team_elimination_hostage_map_t(
        "cash_team_elimination_hostage_map_t",
        "1000",
        FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_NOTIFY);

	ConVar cash_team_elimination_hostage_map_ct(
		"cash_team_elimination_hostage_map_ct",
		"2000",
		FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_NOTIFY);

    ConVar cash_team_elimination_bomb_map(
        "cash_team_elimination_bomb_map",
        "3250",
        FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_NOTIFY);

	ConVar cash_team_survive_guardian_wave(
		"cash_team_survive_guardian_wave",
		"1000",
		FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_NOTIFY );

    ConVar cash_team_win_by_time_running_out_hostage(
        "cash_team_win_by_time_running_out_hostage",
        "3250",
        FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_NOTIFY);

	ConVar cash_team_win_by_time_running_out_bomb(
		"cash_team_win_by_time_running_out_bomb",
		"3250",
		FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_NOTIFY);

    ConVar cash_team_win_by_defusing_bomb(
        "cash_team_win_by_defusing_bomb",
        "3250",
        FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_NOTIFY);

    ConVar cash_team_win_by_hostage_rescue(
        "cash_team_win_by_hostage_rescue",
        "3500",
        FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_NOTIFY);

    ConVar cash_team_loser_bonus(
        "cash_team_loser_bonus",
        "1400",
        FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_NOTIFY);

    ConVar cash_team_loser_bonus_consecutive_rounds(
        "cash_team_loser_bonus_consecutive_rounds",
        "500",
        FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_NOTIFY);

	ConVar cash_team_winner_bonus_consecutive_rounds(
		"cash_team_winner_bonus_consecutive_rounds",
		"0",
		FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_NOTIFY );

	ConVar cash_team_rescued_hostage(
        "cash_team_rescued_hostage",
        "0",
        FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_NOTIFY);

    ConVar cash_team_hostage_alive(
        "cash_team_hostage_alive",
        "0",
        FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_NOTIFY);

    ConVar cash_team_planted_bomb_but_defused(
        "cash_team_planted_bomb_but_defused",
        "800",
        FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_NOTIFY);

    ConVar cash_team_hostage_interaction(
        "cash_team_hostage_interaction",
        "500",
        FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_NOTIFY);

    ConVar cash_player_killed_teammate(
        "cash_player_killed_teammate",
        "-300",
        FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_NOTIFY);

    ConVar cash_player_killed_enemy_factor(
        "cash_player_killed_enemy_factor",
        "1",
        FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_NOTIFY);

    ConVar cash_player_killed_enemy_default(
        "cash_player_killed_enemy_default",
        "300",
        FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_NOTIFY);

    ConVar cash_player_bomb_planted(
        "cash_player_bomb_planted",
        "300",
        FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_NOTIFY);

    ConVar cash_player_bomb_defused(
        "cash_player_bomb_defused",
        "300",
        FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_NOTIFY);

    ConVar cash_player_rescued_hostage(
        "cash_player_rescued_hostage",
        "1000",
        FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_NOTIFY);

    ConVar cash_player_interact_with_hostage(
        "cash_player_interact_with_hostage",
        "150",
        FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_NOTIFY);

    ConVar cash_player_damage_hostage(
        "cash_player_damage_hostage",
        "-30",
        FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_NOTIFY);

    ConVar cash_player_killed_hostage(
        "cash_player_killed_hostage",
        "-1000",
        FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_NOTIFY);

	ConVar cash_player_respawn_amount(
		"cash_player_respawn_amount",
		"0",
		FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_NOTIFY);

	ConVar cash_player_get_killed(
		"cash_player_get_killed",
		"0",
		FCVAR_REPLICATED | FCVAR_RELEASE | FCVAR_NOTIFY);
	

int CCSGameRules::TeamCashAwardValue( TeamCashAward::Type reason)
{
    switch( reason )
    {
    case TeamCashAward::TERRORIST_WIN_BOMB:				return cash_team_terrorist_win_bomb.GetInt();
    case TeamCashAward::ELIMINATION_HOSTAGE_MAP_T:		return cash_team_elimination_hostage_map_t.GetInt();
    case TeamCashAward::ELIMINATION_HOSTAGE_MAP_CT:		return cash_team_elimination_hostage_map_ct.GetInt();
    case TeamCashAward::ELIMINATION_BOMB_MAP:			return cash_team_elimination_bomb_map.GetInt();
    case TeamCashAward::WIN_BY_TIME_RUNNING_OUT_HOSTAGE:return cash_team_win_by_time_running_out_hostage.GetInt();
    case TeamCashAward::WIN_BY_TIME_RUNNING_OUT_BOMB:	return cash_team_win_by_time_running_out_bomb.GetInt();
    case TeamCashAward::WIN_BY_DEFUSING_BOMB:			return cash_team_win_by_defusing_bomb.GetInt();
    case TeamCashAward::WIN_BY_HOSTAGE_RESCUE:			return cash_team_win_by_hostage_rescue.GetInt();
    case TeamCashAward::LOSER_BONUS:					return cash_team_loser_bonus.GetInt();
    case TeamCashAward::LOSER_BONUS_CONSECUTIVE_ROUNDS:	return cash_team_loser_bonus_consecutive_rounds.GetInt();
    case TeamCashAward::RESCUED_HOSTAGE:				return cash_team_rescued_hostage.GetInt();
    case TeamCashAward::HOSTAGE_ALIVE:					return cash_team_hostage_alive.GetInt();
    case TeamCashAward::PLANTED_BOMB_BUT_DEFUSED:		return cash_team_planted_bomb_but_defused.GetInt();
    case TeamCashAward::HOSTAGE_INTERACTION:			return cash_team_hostage_interaction.GetInt();
	case TeamCashAward::SURVIVE_GUARDIAN_WAVE:			return cash_team_survive_guardian_wave.GetInt();
	case TeamCashAward::WINNER_BONUS_CONSECUTIVE_ROUNDS: return cash_team_winner_bonus_consecutive_rounds.GetInt();
    default:
        AssertMsg( false, "Unhandled TeamCashAwardReason" );
        return 0;
    };
}

int CCSGameRules::PlayerCashAwardValue( PlayerCashAward::Type reason)
{
    switch( reason )
    {
    case PlayerCashAward::NONE:						return 0;
    case PlayerCashAward::KILL_TEAMMATE:			return cash_player_killed_teammate.GetInt();
    case PlayerCashAward::KILLED_ENEMY:				return cash_player_killed_enemy_default.GetInt();
    case PlayerCashAward::BOMB_PLANTED:				return cash_player_bomb_planted.GetInt();
    case PlayerCashAward::BOMB_DEFUSED:				return cash_player_bomb_defused.GetInt();
    case PlayerCashAward::RESCUED_HOSTAGE:			return cash_player_rescued_hostage.GetInt();
    case PlayerCashAward::INTERACT_WITH_HOSTAGE:	return cash_player_interact_with_hostage.GetInt();
    case PlayerCashAward::DAMAGE_HOSTAGE:			return cash_player_damage_hostage.GetInt();
    case PlayerCashAward::KILL_HOSTAGE:				return cash_player_killed_hostage.GetInt();
	case PlayerCashAward::RESPAWN:					return cash_player_respawn_amount.GetInt();
	case PlayerCashAward::GET_KILLED:				return cash_player_get_killed.GetInt();
    default:
        AssertMsg( false, "Unhandled PlayerCashAwardReason" );
        return 0;
    };
}

//////////////////////////////////////////////////////////////////////////
// Guardian mode getter functions for UI
//////////////////////////////////////////////////////////////////////////
int CCSGameRules::GetGuardianRequiredKills() const
{
#ifdef CLIENT_DLL
	static ConVarRef mp_guardian_special_kills_needed( "mp_guardian_special_kills_needed" );
#endif

	return mp_guardian_special_kills_needed.GetInt();
}

int CCSGameRules::GetGuardianKillsRemaining() const
{
	return m_nGuardianModeSpecialKillsRemaining;
}

int CCSGameRules::GetGuardianSpecialWeapon() const
{
	return m_nGuardianModeSpecialWeaponNeeded;
}

int CCSGameRules::GetCoopHumanTeamNumber() const
{
	if ( IsPlayingCoopGuardian() )
	{
		return 	( IsHostageRescueMap() ? TEAM_TERRORIST : TEAM_CT );
	}
	else if ( IsPlayingCoopMission() )
	{
		return TEAM_CT; // all coop missions play as CT right now
	}
	else
	{
		return TEAM_INVALID;
	}
}

int CCSGameRules::GetCoopEnemyTeamNumber() const
{
	if ( IsPlayingCoopGuardian() )
	{
		return 	( IsHostageRescueMap() ? TEAM_CT : TEAM_TERRORIST  );
	}
	else if ( IsPlayingCoopMission() )
	{
		return TEAM_TERRORIST; // all coop missions play as CT right now
	}
	else
	{
		return TEAM_INVALID;
	}
}

//////////////////////////////////////////////////////////////////////////
// Gun game getter functions
//////////////////////////////////////////////////////////////////////////

// Determines the highest weapon index of all players in the arms race match
void CCSGameRules::CalculateMaxGunGameProgressiveWeaponIndex( void )
{
    m_iMaxGunGameProgressiveWeaponIndex = 0;

    if ( IsPlayingGunGameProgressive() )
    {
        // Loop through all players and find the max progressive weapon index
        for ( int i = 1; i <= gpGlobals->maxClients; i++ )
        {
            CCSPlayer* pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );
            if ( pPlayer )
            {
                if ( pPlayer->GetPlayerGunGameWeaponIndex() > m_iMaxGunGameProgressiveWeaponIndex )
                {
                    m_iMaxGunGameProgressiveWeaponIndex = pPlayer->GetPlayerGunGameWeaponIndex();
                }
            }
        }
    }
}

const CEconItemDefinition* CCSGameRules::GetCurrentGunGameWeapon ( int nCurrentWeaponIndex, int nTeamID )
{
    if ( GetNumProgressiveGunGameWeapons( nTeamID ) == 0 )
    {
        // Don't process if weapon array is empty
        return nullptr;
    }

    if ( nCurrentWeaponIndex < 0 || nCurrentWeaponIndex >= GetNumProgressiveGunGameWeapons( nTeamID ) )
    {
        // Don't process if out of the array bounds
        return nullptr;
    }

    return GetProgressiveGunGameWeapon( nCurrentWeaponIndex, nTeamID );
}

const CEconItemDefinition* CCSGameRules::GetNextGunGameWeapon( int nCurrentWeaponIndex, int nTeamID )
{
    if ( GetNumProgressiveGunGameWeapons( nTeamID ) == 0 )
    {
        // Don't process if weapon array is empty
        return nullptr;
    }

    if ( nCurrentWeaponIndex < 0 || nCurrentWeaponIndex >= GetNumProgressiveGunGameWeapons( nTeamID ) - 1 )
    {
        // Don't process if already at last weapon or out of the array bounds
        return nullptr;
    }

    return GetProgressiveGunGameWeapon( nCurrentWeaponIndex + 1, nTeamID );
}

const CEconItemDefinition* CCSGameRules::GetPreviousGunGameWeapon( int nCurrentWeaponIndex, int nTeamID )
{
    if ( GetNumProgressiveGunGameWeapons( nTeamID ) == 0 )
    {
        // Don't process if weapon array is empty
		return nullptr;
    }

    if ( nCurrentWeaponIndex <= 0 || nCurrentWeaponIndex >= GetNumProgressiveGunGameWeapons( nTeamID ) )
    {
        // Don't process if already at first weapon or out of the array bounds
        return nullptr;
    }

    return GetProgressiveGunGameWeapon( nCurrentWeaponIndex - 1, nTeamID );
}

DEVELOPMENT_ONLY_CONVAR( debug_armsrace_end_on_next_upgrade, 0 );

bool CCSGameRules::IsFinalGunGameProgressiveWeapon( int nCurrentWeaponIndex, int nTeamID )
{
	if ( debug_armsrace_end_on_next_upgrade.GetBool() )
		return true;

    // Determine if the current weapon is the last in the list of gun game weapons
    if ( nCurrentWeaponIndex == GetNumProgressiveGunGameWeapons( nTeamID ) - 1 )
    {
        return true;
    }

    return false;
}

int CCSGameRules::GetGunGameNumKillsRequiredForWeapon( int nCurrentWeaponIndex, int nTeamID )
{
    if ( nCurrentWeaponIndex < 0 || nCurrentWeaponIndex > GetNumProgressiveGunGameWeapons( nTeamID ) - 1 )
    {
        // Don't process if out of the array bounds
        return -1;
    }

    return GetProgressiveGunGameWeaponKillRequirement( nCurrentWeaponIndex, nTeamID );
}

CBaseCombatWeapon *CCSGameRules::GetNextBestWeapon( CBaseCombatCharacter *pPlayer, CBaseCombatWeapon *pCurrentWeapon )
{
    CBaseCombatWeapon *bestWeapon = NULL;

    // search all the weapons looking for the closest next
    for ( int i = 0; i < MAX_WEAPONS; i++ )
    {
        CBaseCombatWeapon *weapon = pPlayer->GetWeapon(i);
        if ( !weapon )
            continue;

        if ( !weapon->CanBeSelected() || weapon == pCurrentWeapon )
            continue;

#ifndef CLIENT_DLL
        CCSPlayer *csPlayer = ToCSPlayer(pPlayer);
        CWeaponCSBase *csWeapon = static_cast< CWeaponCSBase * >(weapon);
        if ( csPlayer && csPlayer->IsBot() && !TheCSBots()->IsWeaponUseable( csWeapon ) )
            continue;
#endif // CLIENT_DLL

        if ( bestWeapon )
        {
            if ( weapon->GetGearSlot() < bestWeapon->GetGearSlot() )
            {
				int nAmmo = 0;
				if ( weapon->UsesClipsForAmmo1() )
				{
					nAmmo = weapon->Clip1();
				}
				else
				{
					if ( pPlayer )
					{
						nAmmo = weapon->GetReserveAmmoCount( AMMO_POSITION_PRIMARY );
					}
					else
					{
						// No owner, so return how much primary ammo I have along with me.
						nAmmo =  weapon->GetPrimaryAmmoCount();
					}
				}

				if ( nAmmo > 0 )
				{
					bestWeapon = weapon;
				}
            }
            else if ( weapon->GetGearSlot() == bestWeapon->GetGearSlot() && weapon->GetGearSlotPosition() < bestWeapon->GetGearSlotPosition() )
            {
                bestWeapon = weapon;
            }
        }
        else
        {
            bestWeapon = weapon;
        }
    }

    return bestWeapon;
}

float CCSGameRules::GetMapRemainingTime()
{
    // if timelimit is disabled, return -1
    if ( mp_timelimit.GetInt() <= 0 )
        return -1;

    // timelimit is in minutes
    float flTimeLeft =  ( m_flGameStartTime + mp_timelimit.GetInt() * 60 ) - gpGlobals->curtime;

    // never return a negative value
    if ( flTimeLeft < 0 )
        flTimeLeft = 0;

    return flTimeLeft;
}

float CCSGameRules::GetMapElapsedTime( void )
{
    return gpGlobals->curtime;
}

float CCSGameRules::GetRoundRemainingTime() const
{
    return (float) (m_fRoundStartTime + m_iRoundTime) - gpGlobals->curtime; 
}

float CCSGameRules::GetRoundStartTime()
{
    return m_fRoundStartTime;
}


float CCSGameRules::GetRoundElapsedTime()
{
    if ( IsPlayingTraining() )
        return 0.0f;

    float remainingTime = m_iRoundTime - GetRoundRemainingTime();
    if ( remainingTime >= 0.0f )
        return remainingTime;
    else
        return 0.0f;
}


bool CCSGameRules::ShouldCollide( int collisionGroup0, int collisionGroup1 )
{
    if ( collisionGroup0 > collisionGroup1 )
    {
        // swap so that lowest is always first
        V_swap(collisionGroup0,collisionGroup1);
    }
    
    //Don't stand on COLLISION_GROUP_WEAPONs
    if( collisionGroup0 == COLLISION_GROUP_PLAYER_MOVEMENT &&
        collisionGroup1 == COLLISION_GROUP_WEAPON )
    {
        return false;
    }

    // TODO: make a CS-SPECIFIC COLLISION GROUP FOR PHYSICS PROPS THAT USE THIS COLLISION BEHAVIOR.

    
    if ( (collisionGroup0 == COLLISION_GROUP_PLAYER || collisionGroup0 == COLLISION_GROUP_PLAYER_MOVEMENT) &&
        collisionGroup1 == COLLISION_GROUP_PUSHAWAY )
    {
        return false;
    }

    if ( collisionGroup0 == COLLISION_GROUP_DEBRIS && collisionGroup1 == COLLISION_GROUP_PUSHAWAY )
    {
        // let debris and multiplayer objects collide
        return true;
    }

    return BaseClass::ShouldCollide( collisionGroup0, collisionGroup1 ); 
}


bool CCSGameRules::IsFreezePeriod()
{
    return m_bFreezePeriod;
}

bool CCSGameRules::IsWarmupPeriod() const
{
    if ( IsPlayingTraining() )
        return false;

    if ( IsPlayingOffline() && !mp_do_warmup_offine.GetBool() )
        return false;

    return m_bWarmupPeriod;
}

bool CCSGameRules::AllowTaunts( void )
{
	/** Removed for partner depot **/
	return false;
}

#ifdef CLIENT_DLL

bool CCSGameRules::AllowThirdPersonCamera() 
{ 
	return sv_allow_thirdperson.GetBool(); 
}

bool CCSGameRules::IsGoodDownTime( void )
{
	if ( CSGameRules()->IsGameRestarting() )
		return true;

	if ( CSGameRules()->IsIntermission() )
		return true;

	if ( CSGameRules()->InRoundRestart() )
		return true;

	if ( ( ( CSGameRules()->GetRoundRestartTime() - gpGlobals->curtime ) < 0.5f
		&& ( CSGameRules()->GetRoundRestartTime() - gpGlobals->curtime ) > 0.0f ) )
		return true;

	return false;
}

bool CCSGameRules::IsLoadoutAllowed( void )
{
	// main menu
	if ( !engine->IsConnected() )
		return true;
		
	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();

	if ( !pLocalPlayer )
		return false;

	// non players
	if ( 	( pLocalPlayer->IsSpectator() )		||
			( pLocalPlayer->IsHLTV() )			||
			( engine->IsPlayingDemo() )	 )
	{
		return true;
	}

	// Only time when we should block loadout is in COMPETITIVE mode when warmup has elapsed
	if ( IsPlayingAnyCompetitiveStrictRuleset() )
	{
		return ( IsWarmupPeriod() );
	}

	return true;
}
#endif


void CCSGameRules::SetWarmupPeriodStartTime( float fl )
{
#ifndef CLIENT_DLL
	m_fWarmupPeriodStart = fl;
	m_fWarmupPeriodEnd = GetWarmupPeriodEndTime();
#endif
}

float CCSGameRules::GetWarmupPeriodEndTime() const
{
#ifdef CLIENT_DLL
	if ( m_fWarmupPeriodEnd > 0 )
		return m_fWarmupPeriodEnd;
#endif
    return m_fWarmupPeriodStart + mp_warmuptime.GetFloat();
}

bool CCSGameRules::IsWarmupPeriodPaused()
{
	return mp_warmup_pausetimer.GetBool();
}

bool CCSGameRules::IsConnectedUserInfoChangeAllowed( CBasePlayer *pPlayer )
{
#ifndef CLIENT_DLL
	if ( pPlayer )
	{
		int iPlayerTeam = pPlayer->GetTeamNumber();
		if ( ( iPlayerTeam == TEAM_TERRORIST ) || ( iPlayerTeam == TEAM_CT ) )
			return false;
	}
#else
	int iLocalPlayerTeam = GetLocalPlayerTeam();
	if ( ( iLocalPlayerTeam == TEAM_TERRORIST ) || ( iLocalPlayerTeam == TEAM_CT ) )
		return false;
#endif
	return true;
}

#ifndef CLIENT_DLL
void CCSGameRules::StartWarmup( void )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	m_bWarmupPeriod = true;
	m_bCompleteReset = true;
	m_fWarmupPeriodStart = gpGlobals->curtime;
	m_fWarmupPeriodEnd = GetWarmupPeriodEndTime();

	RestartRound();
}

void CCSGameRules::EndWarmup( void )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	if ( !m_bWarmupPeriod )
		return;

	m_bWarmupPeriod = false;
	m_bCompleteReset = true;
	m_fWarmupPeriodStart = -1;
	m_fWarmupPeriodEnd = 0;

	CCSPlayerResource *pResource = dynamic_cast< CCSPlayerResource * >( g_pPlayerResource );
	if ( pResource )
		pResource->ForcePlayersPickColors();
		
	RestartRound();
}

void CCSGameRules::StartTerroristTimeOut( void )
{
	if ( m_bTerroristTimeOutActive || m_bCTTimeOutActive )
		return;

	if ( m_nTerroristTimeOuts <= 0 )
		return;

	m_bTerroristTimeOutActive = true;
	m_flTerroristTimeOutRemaining = mp_team_timeout_time.GetInt();
	m_nTerroristTimeOuts--;

	UTIL_ClientPrintAll( HUD_PRINTCENTER, "#SFUI_Notice_Match_Will_Pause" );
	SetMatchWaitingForResume( true, "TimeOutTs" );
}

void CCSGameRules::EndTerroristTimeOut( void )
{
	if ( !m_bTerroristTimeOutActive )
		return;

	m_bTerroristTimeOutActive = false;
	SetMatchWaitingForResume( false, "TimeOutTs" );
}

void CCSGameRules::StartCTTimeOut( void )
{
	if ( m_bCTTimeOutActive || m_bTerroristTimeOutActive )
		return;

	if ( m_nCTTimeOuts <= 0 )
		return;

	m_bCTTimeOutActive = true;
	m_flCTTimeOutRemaining = mp_team_timeout_time.GetInt();
	m_nCTTimeOuts--;

	UTIL_ClientPrintAll( HUD_PRINTCENTER, "#SFUI_Notice_Match_Will_Pause" );
	SetMatchWaitingForResume( true, "TimeOutCTs" );
}

void CCSGameRules::EndCTTimeOut( void )
{
	if ( !m_bCTTimeOutActive )
		return;

	m_bCTTimeOutActive = false;
	SetMatchWaitingForResume( false, "TimeOutCTs" );
}


#endif

AcquireResult::Type CCSGameRules::IsWeaponAllowed( const CEconItemView *pItem, int nTeamNumber )
{
	// Should always have an itemdef here
	if ( !pItem || !pItem->IsValid() )
		return AcquireResult::NotAllowedForPurchase;

	CSWeaponID	weaponId = GetWeaponIDFromItem( pItem );
	const CCSWeaponInfo *pWeaponInfo = GetWeaponInfoFromItem( pItem );
	if ( !pWeaponInfo )
		return AcquireResult::NotAllowedForPurchase;

	CSWeaponType weaponType = pWeaponInfo->GetWeaponType( pItem );

	// prohibited items
	if ( ( CSGameRules()->m_arrProhibitedItemIndices[ 0 ] != 0 ) )
	{
		int nPosition = pItem->GetItemDefinition()->GetLoadoutSlot( nTeamNumber );
		const CEconItemView* pBaseItem = CSInventoryManager()->GetBaseItemForTeam( nTeamNumber, nPosition );

		if ( pBaseItem && pBaseItem->IsValid() )
		{
			for ( int j = 0; j < MAX_PROHIBITED_ITEMS; j++ )
			{
				// if the base item is prohibited then the slot is prohibited
				if ( CSGameRules()->m_arrProhibitedItemIndices[ j ] == pBaseItem->GetItemDefinition()->GetDefinitionIndex() )
				{
					return AcquireResult::NotAllowedByProhibition;
				}

				// if the base item is not prohibited then the alternate item might still be prohibited
				if ( CSGameRules()->m_arrProhibitedItemIndices[ j ] == pItem->GetItemDefinition()->GetDefinitionIndex() )
				{
					return AcquireResult::NotAllowedByProhibition;
				}
			}
		}
	}
	
	switch ( weaponType )
	{
	case WEAPONTYPE_PISTOL:
		if ( mp_weapons_allow_pistols.GetInt() != -1 )
		{
			if ( mp_weapons_allow_pistols.GetInt( ) == 0 )
				return AcquireResult::NotAllowedForPurchase;
			else if ( mp_weapons_allow_pistols.GetInt( ) != nTeamNumber )
				return AcquireResult::NotAllowedByTeam;
		}
		break;
	case WEAPONTYPE_SUBMACHINEGUN:
		if ( mp_weapons_allow_smgs.GetInt( ) != -1 )
		{
			if ( mp_weapons_allow_smgs.GetInt( ) == 0 )
				return AcquireResult::NotAllowedForPurchase;
			else if ( mp_weapons_allow_smgs.GetInt( ) != nTeamNumber )
				return AcquireResult::NotAllowedByTeam;
		}
		break;
	case WEAPONTYPE_RIFLE:
	case WEAPONTYPE_SNIPER_RIFLE:
		if ( mp_weapons_allow_rifles.GetInt( ) != -1 )
		{
			if ( mp_weapons_allow_rifles.GetInt( ) == 0 )
				return AcquireResult::NotAllowedForPurchase;
			else if ( mp_weapons_allow_rifles.GetInt( ) != nTeamNumber )
				return AcquireResult::NotAllowedByTeam;
		}
		break;
	case WEAPONTYPE_SHOTGUN:
	case WEAPONTYPE_MACHINEGUN:
		if ( mp_weapons_allow_heavy.GetInt( ) != -1 )
		{
			if ( mp_weapons_allow_heavy.GetInt( ) == 0 )
				return AcquireResult::NotAllowedForPurchase;
			else if ( mp_weapons_allow_heavy.GetInt( ) != nTeamNumber )
				return AcquireResult::NotAllowedByTeam;
		}
		break;
	}

	if ( weaponId == ITEM_DEFUSER )
	{
		if ( nTeamNumber == TEAM_TERRORIST )
		{
			return AcquireResult::NotAllowedByTeam;
		}
		if ( !IsBombDefuseMap() && !IsHostageRescueMap() )
		{
			return AcquireResult::NotAllowedByMap;
		}
	}
	else if ( weaponId == WEAPON_TASER )
	{
		// special case for limiting taser to classic casual; data drive this if it becomes more complex
		if ( !mp_weapons_allow_zeus.GetBool() )
		{
			return AcquireResult::NotAllowedForPurchase;
		}
	}
	else if ( weaponId == WEAPON_C4 )
	{
		if ( nTeamNumber != TEAM_TERRORIST && !IsPlayingTraining() && mp_anyone_can_pickup_c4.GetBool() == false )
		{
			return AcquireResult::NotAllowedByTeam;
		}
	}

	return AcquireResult::Allowed;
}

bool CCSGameRules::IsBombDefuseMap() const
{
    return m_bMapHasBombTarget;
}

bool CCSGameRules::IsHostageRescueMap() const
{
    return m_bMapHasRescueZone;
}

bool CCSGameRules::MapHasBuyZone() const
{
    return m_bMapHasBuyZone;
}

bool CCSGameRules::CanSpendMoneyInMap()
{
	if ( GetMaxMoney() <= 0 )
		return false;

    if ( PlayerCashAwardsEnabled() == false && TeamCashAwardsEnabled() == false )
    	return false;

	if ( IsPlayingCoopMission() )
		return false;

	if ( IsPlayingCoopGuardian() )
	{
		int nTeam = GetCoopHumanTeamNumber();
		ConVarRef cvref_sv_buy_status_override( "sv_buy_status_override" );
		int iBuyStatus = cvref_sv_buy_status_override.GetInt();
		if ( iBuyStatus > 0 && (( nTeam == TEAM_CT && iBuyStatus != 1 ) || ( nTeam == TEAM_TERRORIST && iBuyStatus != 2 )) )
			return false;
	}

    return true;
}

bool CCSGameRules::IsLogoMap() const
{
    return m_bLogoMap;
}

float CCSGameRules::GetBuyTimeLength()
{
	if ( IsWarmupPeriod() )
	{
		if ( IsWarmupPeriodPaused( ) )
			return GetWarmupPeriodEndTime( )-m_fWarmupPeriodStart;

		if ( mp_buytime.GetFloat() < GetWarmupPeriodEndTime() )
			return GetWarmupPeriodEndTime();
	}

    return mp_buytime.GetFloat();
}

bool CCSGameRules::IsBuyTimeElapsed()
{
	if ( IsWarmupPeriod() && IsWarmupPeriodPaused() )
		return false;

    return ( GetRoundElapsedTime() > GetBuyTimeLength() );
}

void CCSGameRules::SetMatchWaitingForResume( bool bPauseMatchNow, char const *szReason )
{
	if ( bPauseMatchNow != m_bMatchWaitingForResume )
	{
		m_bMatchWaitingForResume = bPauseMatchNow;

#ifndef CLIENT_DLL
		UTIL_LogPrintf( "Match pause is %s - %s\n", bPauseMatchNow ? "enabled" : "disabled", szReason );
#endif
	}
}

bool CCSGameRules::IsMatchWaitingForResume()
{
	if ( m_bMatchWaitingForResume )
		return true;

	if ( sv_matchpause_auto_5v5.GetBool() && !IsWarmupPeriod() )
	{
		const uint32 kNumPlayersPerSide = ( IsPlayingClassic() && ( g_pGameTypes->GetCurrentGameMode() == CS_GameMode::Classic_ScrimComp2v2 ) ) ? 2
			: 5;

		#ifdef CLIENT_DLL
		C_Team
		#else
		CTeam
		#endif
			*arrTeams[2] = { GetGlobalTeam( TEAM_CT ), GetGlobalTeam( TEAM_TERRORIST ) };
		for ( int iTeam = 0; iTeam < Q_ARRAYSIZE( arrTeams ); ++ iTeam )
		{
			if ( !arrTeams[iTeam] ) continue;
			int numPlayers = arrTeams[iTeam]->GetNumPlayers();
			if ( numPlayers < ( int ) kNumPlayersPerSide )
			{
				SetMatchWaitingForResume( true, "sv_matchpause_auto_5v5" ) ;
				return true;
			}
			int numHumans = 0;
			for ( int iPlayer = 0; iPlayer < numPlayers; ++ iPlayer )
			{
				CBasePlayer *pPlayer = arrTeams[iTeam]->GetPlayer( iPlayer );
				if ( !pPlayer ) continue;
				#ifndef CLIENT_DLL
				if ( !pPlayer->IsConnected() ) continue;
				#endif
				if ( pPlayer->IsBot() ) continue;
				++ numHumans;
			}
			if ( numHumans < ( int ) kNumPlayersPerSide )
			{
				SetMatchWaitingForResume( true, "sv_matchpause_auto_5v5" );
				return true;
			}
		}
	}

	return false;
}

int CCSGameRules::DefaultFOV()
{
    return 90;
}

const CViewVectors* CCSGameRules::GetViewVectors() const
{
    return &g_CSViewVectors;
}


#if defined( GAME_DLL) 
//=========================================================
//=========================================================
bool CCSGameRules::FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker )
{
	CCSPlayer *pCSAttacker = ToCSPlayer( pAttacker );
	if ( pCSAttacker && PlayerRelationship( pPlayer, pCSAttacker ) == GR_TEAMMATE && !pCSAttacker->IsOtherEnemy( pPlayer->entindex() ) )
	{
		// my teammate hit me.
		if ( ( mp_friendlyfire.GetInt() == 0 ) && ( pCSAttacker != pPlayer ) )
		{
			// friendly fire is off, and this hit came from someone other than myself,  then don't get hurt
			return false;
		}
	}

	return BaseClass::FPlayerCanTakeDamage( pPlayer, pCSAttacker );
}

//=========================================================
//=========================================================
int CCSGameRules::IPointsForKill( CBasePlayer *pAttacker, CBasePlayer *pKilled )
{
	CCSPlayer *pCSAttacker = ToCSPlayer( pAttacker );

	if ( !pKilled )
		return 0;

	if ( !pCSAttacker )
		return 1;

	if ( pCSAttacker != pKilled && PlayerRelationship( pCSAttacker, pKilled ) == GR_TEAMMATE && !pCSAttacker->IsOtherEnemy( pKilled->entindex() ) )
		return -1;

	return 1;
}

/*
	Helper function which handles both voice and chat. The only difference is which convar to use
	to determine whether enemies can be heard (sv_alltalk or sv_allchat).
*/
bool CanPlayerHear( CBasePlayer* pListener, CBasePlayer *pSpeaker, bool bTeamOnly, bool bHearEnemies )
{
	Assert(pListener != NULL && pSpeaker != NULL);
	if ( pListener == NULL || pSpeaker == NULL )
		return false;

	// sv_full_alltalk lets everyone can talk to everyone else, except comms specifically flagged as team-only
	if ( !bTeamOnly && sv_full_alltalk.GetBool() )
		return true;
	
	bool bSpeakerIsSpectator = pSpeaker->IsSpectator();
	bool bSpeakerAlive = pSpeaker->IsAlive();

	if ( pSpeaker->IsDead() && pSpeaker->HasDeathInfo() && gpGlobals->curtime < pSpeaker->GetDeathInfoTime() + sv_talk_after_dying_time.GetFloat() )
	{
		bSpeakerIsSpectator = false;
		bSpeakerAlive = true;
	}

	// if either speaker or listener are coaching then for intents and purposes treat them as teammates.
	int iListenerTeam = pListener->GetAssociatedTeamNumber();
	int iSpeakerTeam = pSpeaker->GetTeamNumber();

	// use the observed target's team when sv_spec_hear is mode 2
	if ( pListener->IsSpectator() && sv_spec_hear.GetInt() == SpecHear::SpectatedTeam && 
		( pListener->GetObserverMode() == OBS_MODE_IN_EYE || pListener->GetObserverMode() == OBS_MODE_CHASE ) )
	{
		CBaseEntity *pTarget = pListener->GetObserverTarget();
		CBasePlayer* pTargetPlayer = dynamic_cast< CBasePlayer* >( pTarget );
		if ( pTargetPlayer )
		{
			iListenerTeam = pTargetPlayer->GetTeamNumber();
			pListener = pTargetPlayer;
		}
	}

	if ( pListener->IsSpectator() )
	{
		if ( sv_spec_hear.GetInt() == SpecHear::Nobody )
			return false; // spectators are selected to not hear other spectators

		if ( sv_spec_hear.GetInt() == SpecHear::Self )
			return ( pListener == pSpeaker ); // spectators are selected to not hear other spectators

		// spectators can always hear other spectators
		if ( bSpeakerIsSpectator )
			return true;

		return !bTeamOnly && sv_spec_hear.GetInt() == SpecHear::AllPlayers;
	}

	// no one else can hear spectators, except if they're coaches at certain times
	if (pSpeaker->IsCoach() )
	{
		if (pSpeaker->GetAssociatedTeamNumber() == iListenerTeam)
		{
			if ( sv_coach_comm_unrestricted.GetBool() ||
				CSGameRules()->IsWarmupPeriod() ||
				CSGameRules()->IsTimeOutActive() ||
				(CSGameRules()->GetGamePhase() == GAMEPHASE_HALFTIME))
				return true;
			else
				return false;
		}
		else
			return false;
	}
	else if ( bSpeakerIsSpectator )
		return false;

	// are enemy teams prevented from hearing each other by sv_alltalk/sv_allchat?
	if ( (bTeamOnly || !bHearEnemies) && iSpeakerTeam != iListenerTeam )
		return false;

	// living players can only hear dead players if sv_deadtalk is enabled
	if ( pListener->IsAlive() && !bSpeakerAlive )
	{
		return sv_deadtalk.GetBool();
	}

	return true;
}

static bool OverrideCanPlayerHearInSurvival( CBasePlayer* pListener, CBasePlayer *pSpeaker, bool bTeamOnly, bool bTextChat )
{
	//
	// Text chat: team = never to enemies; all = allowed in warmup/freeze/end-of-match, otherwise follows voice rules during live round
	// Voice chat: solo = alive>no, bothdead>yes; duos+ = squadmates>yes, alive>no, bothsquadsdead>yes
	//

	bool bCanHear = true;
	CCSPlayer *pCSPlayerListener = ToCSPlayer( pListener );
	CCSPlayer *pCSPlayerSpeaker = ToCSPlayer( pSpeaker );
	if ( pCSPlayerListener && pCSPlayerSpeaker )
	{
		if ( bTextChat && bTeamOnly )
			return !pCSPlayerSpeaker->IsOtherEnemy( pCSPlayerListener );

		if ( bTextChat && !bTeamOnly &&
			CSGameRules() && ( CSGameRules()->IsWarmupPeriod() || CSGameRules()->IsFreezePeriod() || CSGameRules()->IsRoundOver() ) )
			return true;

		bCanHear = // in solo mode nobody can hear each other by default
			( pCSPlayerListener->m_nSurvivalTeam >= 0 && pCSPlayerSpeaker->m_nSurvivalTeam >= 0 ) && // playing team mode on same team
			( pCSPlayerListener->m_nSurvivalTeam == pCSPlayerSpeaker->m_nSurvivalTeam ); // note in solo mode everyone is on team -1

																						 // entirely dead survival teams can hear other entirely dead survival teams. Makes playtests better.
		if ( !bCanHear && !pCSPlayerListener->IsAlive() && !pCSPlayerSpeaker->IsAlive() // both are dead...
			&& CSGameRules() && !CSGameRules()->IsWarmupPeriod() && !CSGameRules()->IsFreezePeriod() ) // and the game is in progress
		{
			if ( pCSPlayerListener->m_nSurvivalTeam >= 0 || pCSPlayerSpeaker->m_nSurvivalTeam >= 0 )
				bCanHear = false;	// In team mode voice chat can never go across teams
			else
				bCanHear = true;	// In solo mode this enables dead talk
			/*
			// Code allowing dead talk when all teammates are dead across with other fully dead teams
			if ( pCSPlayerListener->m_nSurvivalTeam >= 0 || pCSPlayerSpeaker->m_nSurvivalTeam >= 0 )
			{
				// In team mode look for alive teammates of either of the players
				for ( int i = 1; i <= gpGlobals->maxClients; ++i )
				{
					CCSPlayer* pOtherPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );
					if ( pOtherPlayer && pOtherPlayer->IsAlive() )
					{
						if ( pOtherPlayer->m_nSurvivalTeam == pCSPlayerListener->m_nSurvivalTeam ||
							pOtherPlayer->m_nSurvivalTeam == pCSPlayerSpeaker->m_nSurvivalTeam )
						{
							bCanHear = false; // listener or speaker have an alive teammate, do not let them chat!
							break;
						}
					}
				}
			}
			*/
		}
	}

	return bCanHear;
}

bool CCSGameRules::CanPlayerHearTalker( CBasePlayer* pListener, CBasePlayer *pSpeaker, bool bTeamOnly  )
{
	bool bHearEnemy = false;

	bool bSpeakerAlive = pSpeaker->IsAlive();
	bool bSpeakerDying = false;

	if ( !bSpeakerAlive && pSpeaker->HasDeathInfo() && gpGlobals->curtime < pSpeaker->GetDeathInfoTime() + sv_talk_after_dying_time.GetFloat() )
	{
		bSpeakerAlive = true;
		bSpeakerDying = true;
	}
	
	if ( sv_talk_enemy_living.GetBool() && sv_talk_enemy_dead.GetBool() )
	{
		bHearEnemy = true;
	}
	else if ( !pListener->IsAlive() && !bSpeakerAlive )
	{
		bHearEnemy = sv_talk_enemy_dead.GetBool();
	}
	else if ( pListener->IsAlive() && bSpeakerAlive )
	{
		bHearEnemy = sv_talk_enemy_living.GetBool();
	}

	bool bCanHear = CanPlayerHear( pListener, pSpeaker, bTeamOnly, bHearEnemy );

	static ConVarRef sv_voice_proximity_minvolume( "sv_voice_proximity_minvolume" );
	extern ConVar sv_voice_proximity;
	if ( bCanHear && pListener->IsAlive() && sv_voice_proximity.GetFloat() > 0 && sv_voice_proximity_minvolume.GetFloat() <= 0.0f )
	{
		Vector vecSpeakerOrigin = pSpeaker->GetAbsOrigin();
		if ( bSpeakerDying && pSpeaker->HasDeathInfo() )
		{
			vecSpeakerOrigin = pSpeaker->GetDeathOrigin();
		}

		bCanHear = (vecSpeakerOrigin.DistTo( pListener->GetAbsOrigin() ) < sv_voice_proximity.GetFloat());
	}

	// duo mode voice comms rules
	if ( bCanHear && IsPlayingSurvival() )
	{
		bCanHear = OverrideCanPlayerHearInSurvival( pListener, pSpeaker, bTeamOnly, false );
	}

	return bCanHear;
}

extern ConVar sv_allchat;
bool CCSGameRules::PlayerCanHearChat( CBasePlayer *pListener, CBasePlayer *pSpeaker, bool bTeamOnly )
{
	bool bCanHear = CanPlayerHear( pListener, pSpeaker, bTeamOnly, sv_allchat.GetBool() );

	if ( bCanHear && pListener->IsAlive() && sv_chat_proximity.GetFloat() > 0 )
	{
		Vector vecSpeakerOrigin = pSpeaker->GetAbsOrigin();
		bCanHear = (vecSpeakerOrigin.DistTo( pListener->GetAbsOrigin() ) < sv_chat_proximity.GetFloat());
	}

	if ( bCanHear && IsPlayingSurvival() )
	{
		bCanHear = OverrideCanPlayerHearInSurvival( pListener, pSpeaker, bTeamOnly, true );
	}

	return bCanHear;
}

int CCSGameRules::PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget )
{

	// half life multiplay has a simple concept of Player Relationships.
	// you are either on another player's team, or you are not.
	if ( !pPlayer || !pTarget || !pTarget->IsPlayer() )
		return GR_NOTTEAMMATE;

	// don't do string compares, just compare the team number
	// 	if ( (*GetTeamID(pPlayer) != '\0') && (*GetTeamID(pTarget) != '\0') && !stricmp( GetTeamID(pPlayer), GetTeamID(pTarget) ) )
	// 	{
	// 		return GR_TEAMMATE;
	// 	}

	CCSPlayer *pCSPlayer = ToCSPlayer( pPlayer );
	CCSPlayer *pCSTarget = ToCSPlayer( pTarget );

	if ( pCSPlayer->GetAssociatedTeamNumber() != TEAM_INVALID && pCSTarget->GetAssociatedTeamNumber() != TEAM_INVALID && pCSPlayer->GetAssociatedTeamNumber() == pCSTarget->GetAssociatedTeamNumber() )
	{
		return GR_TEAMMATE;
	}

	return GR_NOTTEAMMATE;

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGameRules::SetTeamRespawnWaveTime( int iTeam, float flValue ) 
{ 
	if ( flValue < 0 )
	{
		flValue = 0;
	}

	// initialized to -1 so we can try to determine if this is the first spawn time we have received for this team
// 	if ( m_flOriginalTeamRespawnWaveTime[iTeam] < 0 )
// 	{
// 		m_flOriginalTeamRespawnWaveTime[iTeam] = flValue;
// 	}

	m_TeamRespawnWaveTimes.Set( iTeam, flValue );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGameRules::CheckRespawnWaves( void )
{
	bool bGuardianWaveStarted = false;

	for ( int team = LAST_SHARED_TEAM+1; team < GetNumberOfTeams(); team++ )
	{
		if ( m_flNextRespawnWave[team] > gpGlobals->curtime )
			continue;

		if ( mp_use_respawn_waves.GetInt() == 2 )
		{
			if ( IsWarmupPeriod() == false && ( IsPlayingCoopGuardian() || IsPlayingCoopMission() ) )
			{
				// we sometimes don't want to increment the wav number
				if ( m_bDontIncrementCoopWave == false )
					m_nGuardianModeWaveNumber++;

				m_bDontIncrementCoopWave = false;

				if ( IsPlayingCoopGuardian() )
					bGuardianWaveStarted = true;
			}

			// if this mode is set, we don't want to set the respawn time to anythign other than infinity, pretty much
			m_flNextRespawnWave.Set( team, gpGlobals->curtime + 99999 );
		}
		else
			m_flNextRespawnWave.Set( team, gpGlobals->curtime + GetRespawnWaveMaxLength( team ) );

		// respawn all players who are able to respawn here
		for ( int i = 1; i <= MAX_PLAYERS; i++ )
		{
			CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );
			if ( pPlayer && pPlayer->PlayerClass() != 0 && pPlayer->GetTeamNumber() > TEAM_SPECTATOR && pPlayer->GetTeamNumber() == team && !pPlayer->IsAlive() )
			{
				bool bMatchRespawnGamePhase = false;
				switch ( m_match.GetPhase() )
				{
					case GAMEPHASE_WARMUP_ROUND:
					case GAMEPHASE_PLAYING_STANDARD:
					case GAMEPHASE_PLAYING_FIRST_HALF:
					case GAMEPHASE_PLAYING_SECOND_HALF:
						bMatchRespawnGamePhase = true;
						break;
				}

				if ( ( IsPlayingCoopGuardian() || IsPlayingCoopMission() ) && pPlayer->IsBot() && pPlayer->IsAbleToInstantRespawn() )
				{
					GuardianUpdateBotAccountAndWeapons( pPlayer );

					// set the bot's name
					CCSBot *bot = dynamic_cast< CCSBot * >( pPlayer );
					if ( bot )
					{
						char botName[MAX_PLAYER_NAME_LENGTH];
						UTIL_ConstructBotNetName( botName, MAX_PLAYER_NAME_LENGTH, bot->GetProfile() );

						engine->SetFakeClientConVarValue( bot->edict(), "name", botName );
					}
					m_nGuardianGrenadesToGiveBots = RandomInt( 2, MIN( 5, m_nGuardianModeWaveNumber ) );

					// respawn
					pPlayer->State_Transition( STATE_GUNGAME_RESPAWN );
				}
				else if ( bMatchRespawnGamePhase && pPlayer->IsAbleToInstantRespawn() && pPlayer->GetObserverMode() > OBS_MODE_FREEZECAM )
				{
					// respawn
					pPlayer->State_Transition( STATE_GUNGAME_RESPAWN );
				}
			}
		}
	}

	// Handle chatter and player info after we have respawned everyone
	if ( bGuardianWaveStarted )
	{
		UTIL_GlobalChatter( "Guardian.WaveStart" );

		int nTeam = GetCoopHumanTeamNumber();
		for ( int i = 1; i <= MAX_PLAYERS; i++ )
		{
			CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );
			if ( pPlayer && pPlayer->GetTeamNumber() == nTeam )
			{
				//CBroadcastRecipientFilter filter;

				CRecipientFilter filter;
				filter.AddRecipient( pPlayer );
				pPlayer->EmitSound( filter, pPlayer->entindex(), "UI.DeathMatchBonusAlertEnd" );

				char szWave[32];
				Q_snprintf( szWave, sizeof( szWave ), "%d", ( int )m_nGuardianModeWaveNumber );

				const char *szTeam = ( nTeam == TEAM_CT ) ? "CT" : "T";
				int nMsg = RandomInt( 1, 8 );
				char szNotice[512];
				Q_snprintf( szNotice, sizeof( szNotice ), "#SFUI_Notice_NewWaveBegun_%s%d", szTeam, ( int )nMsg );

				// send the message
				UTIL_ClientPrintFilter( filter, HUD_PRINTCENTER, szNotice, szWave );
			}
		}
	}
}

void CCSGameRules::GuardianUpdateBotAccountAndWeapons( CCSPlayer *pBot )
{
	if ( pBot->IsBot() && mp_use_respawn_waves.GetInt() == 2
		 && IsWarmupPeriod() == false && (IsPlayingCoopGuardian() || IsPlayingCoopMission()) )
	{
		pBot->RemoveAllItems( true );
		pBot->ResetAccount();
		pBot->AddAccount( ( mp_guardian_bot_money_per_wave.GetInt()
			* m_nGuardianModeWaveNumber ), false );
	}
}

extern const char* Helper_PickBotGrenade();
void CCSGameRules::GiveGuardianBotGrenades( CCSPlayer *pBot )
{
	if ( m_nGuardianGrenadesToGiveBots > 0 && pBot->IsBot() && mp_use_respawn_waves.GetInt() == 2
		 && IsWarmupPeriod() == false && (IsPlayingCoopGuardian() || IsPlayingCoopMission()) )
	{
		const char* szGrenade = Helper_PickBotGrenade();
		if ( szGrenade && pBot->GiveNamedItem( CFmtStr( "weapon_%s", szGrenade ).Access() ) != NULL )
			m_nGuardianGrenadesToGiveBots--;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Is the player past the required delays for spawning
//-----------------------------------------------------------------------------
bool CCSGameRules::HasPassedMinRespawnTime( CBasePlayer *pPlayer )
{
	float flMinSpawnTime = GetMinTimeWhenPlayerMaySpawn( pPlayer ); 

	return ( gpGlobals->curtime > flMinSpawnTime );
}

#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CCSGameRules::GetRespawnTimeScalar( int iTeam )
{
	if ( mp_use_respawn_waves.GetInt() == 2 )
		return 1;

	// For long respawn times, scale the time as the number of players drops
	int iOptimalPlayers = 8;	// 16 players total, 8 per team

	int iNumPlayers = GetGlobalTeam(iTeam)->GetNumPlayers();

	float flScale = RemapValClamped( iNumPlayers, 1, iOptimalPlayers, 0.25, 1.0 );
	return flScale;
}

bool CCSGameRules::IsEndMatchVotingForNextMapEnabled()
{
	if ( mp_endmatch_votenextmap.GetBool() )
	{
		//int numMaps = 0;
		if ( g_pGameTypes )
		{
#ifndef CLIENT_DLL
			const char* mapGroupName = gpGlobals->mapGroupName.ToCStr();
#else
			const char* mapGroupName = engine->GetMapGroupName();
#endif
			const CUtlStringList* mapsInGroup = g_pGameTypes->GetMapGroupMapList( mapGroupName );
			if ( mapsInGroup )
				return (mapsInGroup->Count() > 1);
		}
	}

	return false;
}

bool CCSGameRules::IsEndMatchVotingForNextMap()
{
	if ( !IsEndMatchVotingForNextMapEnabled() || GetGamePhase() != GAMEPHASE_MATCH_ENDED )
		return false;

	if ( GetCMMItemDropRevealEndTime() > gpGlobals->curtime )
		return false;

 	if ( m_bIsDroppingItems )
 		return false;

// 	// TODO: add a check to make sure items aren't dropping
// 	if ( IsEndMatchVotingForNextMapEnabled() && ( GetGamePhase() == GAMEPHASE_MATCH_ENDED ) && 
// 		((gpGlobals->curtime+m_timeUntilNextPhaseStarts) >= gpGlobals->curtime+mp_win_panel_display_time.GetFloat()) && gpGlobals->curtime+GetCMMItemDropRevealEndTime() < gpGlobals->curtime )
// 	{
// 		return true;
// 	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: How long are the respawn waves for this team currently?
//-----------------------------------------------------------------------------
float CCSGameRules::GetRespawnWaveMaxLength( int iTeam, bool bScaleWithNumPlayers /* = true */ )
{
	//Let's just turn off respawn times while players are messing around waiting for the tournament to start
	if ( IsWarmupPeriod() || m_bFreezePeriod )
		return 0.0f;

	if ( mp_use_respawn_waves.GetInt() == 0 )
		return 0.0f;

	float flWaveTime = 0;
	if ( iTeam == TEAM_TERRORIST )
		flWaveTime = mp_respawnwavetime_t.GetFloat();
	else if ( iTeam == TEAM_CT )
		flWaveTime = mp_respawnwavetime_ct.GetFloat();

	//float flTime = ( ( m_TeamRespawnWaveTimes[iTeam] >= 0 ) ? m_TeamRespawnWaveTimes[iTeam] : flWaveTime );
	float flTime = flWaveTime;

	// For long respawn times, scale the time as the number of players drops
	if ( bScaleWithNumPlayers && flTime > 5 )
	{
		flTime = MAX( 5, flTime * GetRespawnTimeScalar(iTeam) );
	}

	return flTime;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
float CCSGameRules::GetMinTimeWhenPlayerMaySpawn( CBasePlayer *pPlayer )
{
	// Min respawn time is the sum of
	//
	// a) the length of one full *unscaled* respawn wave for their team
	//		and
	// b) death anim length + freeze panel length

	float fMinDelay = 2.0f + GetRespawnWaveMaxLength( pPlayer->GetTeamNumber(), false );

	return pPlayer->GetDeathTime() + fMinDelay;
}

void CCSGameRules::SetNextTeamRespawnWaveDelay( int iTeam, float flDelay )
{
	float flNextRespawn = gpGlobals->curtime + flDelay;
	m_flNextRespawnWave.Set( iTeam, flNextRespawn );
}

//-----------------------------------------------------------------------------
// Purpose: don't let us spawn before our freezepanel time would have ended, even if we skip it
//-----------------------------------------------------------------------------
float CCSGameRules::GetNextRespawnWave( int iTeam, CBasePlayer *pPlayer ) 
{ 
	// 	if ( State_Get() == GR_STATE_STALEMATE )
	// 		return 0;

	// If we are purely checking when the next respawn wave is for this team
	if ( pPlayer == NULL )
	{
		return m_flNextRespawnWave[iTeam];
	}

	// The soonest this player may spawn
	float flMinSpawnTime = GetMinTimeWhenPlayerMaySpawn( pPlayer );

	// the next scheduled respawn wave time
	float flNextRespawnTime = m_flNextRespawnWave[iTeam];

	// the length of one respawn wave. We'll check in increments of this
	float flRespawnWaveMaxLen = GetRespawnWaveMaxLength( iTeam );

	if ( flRespawnWaveMaxLen <= 0 )
	{
		return flNextRespawnTime;
	}

	// Keep adding the length of one respawn until we find a wave that
	// this player will be eligible to spawn in.
	while ( flNextRespawnTime < flMinSpawnTime )
	{
		flNextRespawnTime += flRespawnWaveMaxLen; 
	}

	return flNextRespawnTime; 
}
//-----------------------------------------------------------------------------
// Purpose: Init CS ammo definitions
//-----------------------------------------------------------------------------

// shared ammo definition
// JAY: Trying to make a more physical bullet response
#define BULLET_MASS_GRAINS_TO_LB(grains)	(0.002285*(grains)/16.0f)
#define BULLET_MASS_GRAINS_TO_KG(grains)	lbs2kg(BULLET_MASS_GRAINS_TO_LB(grains))

static CCSAmmoDef ammoDef;
CCSAmmoDef* GetCSAmmoDef()
{
    GetAmmoDef(); // to initialize the ammo info
    return &ammoDef;
}

CAmmoDef* GetAmmoDef()
{
    static bool bInitted = false;

    if ( !bInitted )
    {
        bInitted = true;
        
        ammoDef.AddAmmoType( BULLET_PLAYER_50AE,		DMG_BULLET, TRACER_LINE, 0, 0, "ammo_50AE_max",		"ammo_50AE_impulse", 0, 10, 14 );
        ammoDef.AddAmmoType( BULLET_PLAYER_762MM,		DMG_BULLET, TRACER_LINE, 0, 0, "ammo_762mm_max",	"ammo_762mm_impulse", 0, 10, 14 );//Sniper ammo has little force since it just goes through the target.
        ammoDef.AddAmmoType( BULLET_PLAYER_556MM,		DMG_BULLET, TRACER_LINE, 0, 0, "ammo_556mm_max",	"ammo_556mm_impulse", 0, 10, 14 );
		ammoDef.AddAmmoType( BULLET_PLAYER_556MM_SMALL,	DMG_BULLET, TRACER_LINE, 0, 0, "ammo_556mm_small_max","ammo_556mm_impulse", 0, 10, 14 ); // 15 round clip
        ammoDef.AddAmmoType( BULLET_PLAYER_556MM_BOX,	DMG_BULLET, TRACER_LINE, 0, 0, "ammo_556mm_box_max","ammo_556mm_box_impulse", 0, 10, 14 );
        ammoDef.AddAmmoType( BULLET_PLAYER_338MAG,		DMG_BULLET, TRACER_LINE, 0, 0, "ammo_338mag_max",	"ammo_338mag_impulse", 0, 12, 16 );
        ammoDef.AddAmmoType( BULLET_PLAYER_9MM,			DMG_BULLET, TRACER_LINE, 0, 0, "ammo_9mm_max",		"ammo_9mm_impulse", 0, 5, 10 );
        ammoDef.AddAmmoType( BULLET_PLAYER_BUCKSHOT,	DMG_BULLET, TRACER_LINE, 0, 0, "ammo_buckshot_max", "ammo_buckshot_impulse", 0, 3, 6 );
        ammoDef.AddAmmoType( BULLET_PLAYER_45ACP,		DMG_BULLET, TRACER_LINE, 0, 0, "ammo_45acp_max",	"ammo_45acp_impulse", 0, 6, 10 );
        ammoDef.AddAmmoType( BULLET_PLAYER_357SIG,		DMG_BULLET, TRACER_LINE, 0, 0, "ammo_357sig_max",	"ammo_357sig_impulse", 0, 4, 8 );
        ammoDef.AddAmmoType( BULLET_PLAYER_357SIG_SMALL,DMG_BULLET, TRACER_LINE, 0, 0, "ammo_357sig_small_max",	"ammo_357sig_impulse", 0, 4, 8 );
        ammoDef.AddAmmoType( BULLET_PLAYER_357SIG_MIN,	DMG_BULLET, TRACER_LINE, 0, 0, "ammo_357sig_min_max",	"ammo_357sig_impulse", 0, 4, 8 );
        ammoDef.AddAmmoType( BULLET_PLAYER_57MM,		DMG_BULLET, TRACER_LINE, 0, 0, "ammo_57mm_max",		"ammo_57mm_impulse", 0, 4, 8 );
        ammoDef.AddAmmoType( AMMO_TYPE_HEGRENADE,		DMG_BLAST,	TRACER_LINE, 0, 0, "ammo_grenade_limit_default", 0, 0, 0 );
        ammoDef.AddAmmoType( AMMO_TYPE_FLASHBANG,		0,			TRACER_LINE, 0,	0, "ammo_grenade_limit_flashbang", 0, 0, 0 );
        ammoDef.AddAmmoType( AMMO_TYPE_SMOKEGRENADE,	0,			TRACER_LINE, 0, 0, "ammo_grenade_limit_default", 0, 0, 0 );
        ammoDef.AddAmmoType( AMMO_TYPE_MOLOTOV,			DMG_BURN,	TRACER_NONE, 0, 0, "ammo_grenade_limit_default", 0, 0, 0 );
        ammoDef.AddAmmoType( AMMO_TYPE_DECOY,			0,			TRACER_NONE, 0, 0, "ammo_grenade_limit_default", 0, 0, 0 );
        ammoDef.AddAmmoType( AMMO_TYPE_TASERCHARGE,		DMG_SHOCK,	TRACER_BEAM, 0, 0, 0, 0, 0, 0 );
		ammoDef.AddAmmoType( BULLET_PLAYER_357SIG_P250,	DMG_BULLET, TRACER_LINE, 0, 0, "ammo_357sig_p250_max",	"ammo_357sig_impulse", 0, 4, 8 );
		ammoDef.AddAmmoType( AMMO_TYPE_HEALTHSHOT,		0,			TRACER_LINE, 0, 0, "ammo_item_limit_healthshot", 0, 0, 0 );
		ammoDef.AddAmmoType( AMMO_TYPE_TAGRENADE,		0,			TRACER_NONE, 0, 0, "ammo_grenade_limit_default", 0, 0, 0 );
		ammoDef.AddAmmoType( AMMO_TYPE_BREACHCHARGE,	0,			TRACER_NONE, 0, 0, "ammo_grenade_limit_breachcharge", 0, 0, 0 );
		ammoDef.AddAmmoType( AMMO_TYPE_SNOWBALL,		DMG_NEVERGIB,	TRACER_NONE, 0, 0, "ammo_grenade_limit_snowballs", 0, 0, 0 );
		

        //Adrian: I set all the prices to 0 just so the rest of the buy code works
        //This should be revisited.
        ammoDef.AddAmmoCost( BULLET_PLAYER_50AE, 0, 7 );
        ammoDef.AddAmmoCost( BULLET_PLAYER_762MM, 0, 30 );
        ammoDef.AddAmmoCost( BULLET_PLAYER_556MM, 0, 30 );
        ammoDef.AddAmmoCost( BULLET_PLAYER_556MM_SMALL, 0, 15 );
        ammoDef.AddAmmoCost( BULLET_PLAYER_556MM_BOX, 0, 30 );
        ammoDef.AddAmmoCost( BULLET_PLAYER_338MAG, 0, 10 );
        ammoDef.AddAmmoCost( BULLET_PLAYER_9MM, 0, 30 );
        ammoDef.AddAmmoCost( BULLET_PLAYER_BUCKSHOT, 0, 8 );
        ammoDef.AddAmmoCost( BULLET_PLAYER_45ACP, 0, 25 );
        ammoDef.AddAmmoCost( BULLET_PLAYER_357SIG, 0, 13 );
		ammoDef.AddAmmoCost( BULLET_PLAYER_357SIG_P250, 0, 13 );
        ammoDef.AddAmmoCost( BULLET_PLAYER_357SIG_SMALL, 0, 13 );
		ammoDef.AddAmmoCost( BULLET_PLAYER_357SIG_MIN, 0, 12 );
        ammoDef.AddAmmoCost( BULLET_PLAYER_57MM, 0, 50 );
    }

    return &ammoDef;
}

bool CCSGameRules::IsRoundOver() const
{
    return m_iRoundWinStatus != WINNER_NONE;
}

void CCSGameRules::ClearItemsDroppedDuringMatch( void )
{ 
	m_ItemsPtrDroppedDuringMatch.PurgeAndDeleteElements(); 
}

void CCSGameRules::RecordPlayerItemDrop( const CEconItemPreviewDataBlock &iteminfo )
{
	for ( int i = 0; i < m_ItemsPtrDroppedDuringMatch.Count(); i++ )
	{
		// if we've recorded this item already, don't record it again
		if ( m_ItemsPtrDroppedDuringMatch[i]->itemid() == iteminfo.itemid() &&
			m_ItemsPtrDroppedDuringMatch[i]->accountid() == iteminfo.accountid() )
			return;
	}

#if defined( CLIENT_DLL )
	// When playing back Overwatch with anonymous player identities don't collect drops on client
	if ( CDemoPlaybackParameters_t const *pParams = engine->GetDemoPlaybackParameters() )
	{
		if ( pParams->m_bAnonymousPlayerIdentity )
			return;
	}
#endif

	m_ItemsPtrDroppedDuringMatch.AddToTail( new CEconItemPreviewDataBlock( iteminfo ) );

#ifndef CLIENT_DLL
	// Send drops immediately in survival mode
	if ( IsPlayingSurvival() )
		SendPlayerItemDropsToClient();
#endif
}

#ifndef CLIENT_DLL
const char *CCSGameRules::GetChatPrefix( bool bTeamOnly, CBasePlayer *pPlayer )
{
    char *pszPrefix = NULL;

    if ( !pPlayer )  // dedicated server output
    {
        pszPrefix = "";
    }
    else
    {
        // team only
        if ( bTeamOnly == TRUE )
        {
            if ( pPlayer->GetTeamNumber() == TEAM_CT )
            {
                if ( pPlayer->m_lifeState == LIFE_ALIVE )
                {
                    pszPrefix = "(Counter-Terrorist)";
                }
                else 
                {
                    pszPrefix = "*DEAD*(Counter-Terrorist)";
                }
            }
            else if ( pPlayer->GetTeamNumber() == TEAM_TERRORIST )
            {
                if ( pPlayer->m_lifeState == LIFE_ALIVE )
                {
                    pszPrefix = "(Terrorist)";
                }
                else
                {
                    pszPrefix = "*DEAD*(Terrorist)";
                }
            }
            else if ( pPlayer->GetTeamNumber() == TEAM_SPECTATOR )
            {
                pszPrefix = "(Spectator)";
            }
        }
        // everyone
        else
        {
            if ( pPlayer->m_lifeState == LIFE_ALIVE )
            {
                pszPrefix = "";
            }
            else
            {
                if ( pPlayer->GetTeamNumber() != TEAM_SPECTATOR )
                {
                    pszPrefix = "*DEAD*";	
                }
                else
                {
                    pszPrefix = "*SPEC*";
                }
            }
        }
    }

    return pszPrefix;
}

const char *CCSGameRules::GetChatLocation( bool bTeamOnly, CBasePlayer *pPlayer )
{
    if ( !pPlayer )  // dedicated server output
    {
        return NULL;
    }

    // only teammates see locations
    if ( !bTeamOnly )
        return NULL;

    // only living players have locations
    if ( pPlayer->GetTeamNumber() != TEAM_CT && pPlayer->GetTeamNumber() != TEAM_TERRORIST )
        return NULL;

    if ( !pPlayer->IsAlive() )
        return NULL;

    return pPlayer->GetLastKnownPlaceName();
}

const char *CCSGameRules::GetChatFormat( bool bTeamOnly, CBasePlayer *pPlayer )
{
    if ( !pPlayer )  // dedicated server output
    {
        return NULL;
    }

    const char *pszFormat = NULL;

    // team only
    if ( bTeamOnly == TRUE )
    {
        if ( pPlayer->GetAssociatedTeamNumber() == TEAM_CT )
        {
            if ( ( pPlayer->m_lifeState == LIFE_ALIVE ) )
            {
                const char *chatLocation = GetChatLocation( bTeamOnly, pPlayer );
                if ( chatLocation && *chatLocation )
                {
                    pszFormat = "Cstrike_Chat_CT_Loc";
                }
                else
                {
                    pszFormat = "Cstrike_Chat_CT";
                }
            }
            else 
            {
                pszFormat = "Cstrike_Chat_CT_Dead";
            }
        }
        else if ( pPlayer->GetAssociatedTeamNumber() == TEAM_TERRORIST )
        {
            if ( ( pPlayer->m_lifeState == LIFE_ALIVE ) )
            {
                const char *chatLocation = GetChatLocation( bTeamOnly, pPlayer );
                if ( chatLocation && *chatLocation )
                {
                    pszFormat = "Cstrike_Chat_T_Loc";
                }
                else
                {
                    pszFormat = "Cstrike_Chat_T";
                }
            }
            else
            {
                pszFormat = "Cstrike_Chat_T_Dead";
            }
        }
        else if ( pPlayer->IsSpectator() )
        {
            pszFormat = "Cstrike_Chat_Spec";
        }
    }
    // everyone
    else
    {
        if ( pPlayer->m_lifeState == LIFE_ALIVE )
        {
            pszFormat = "Cstrike_Chat_All";
        }
        else
        {
            if ( !pPlayer->IsSpectator() )
            {
                pszFormat = "Cstrike_Chat_AllDead";	
            }
            else
            {
                pszFormat = "Cstrike_Chat_AllSpec";
            }
        }
    }

    return pszFormat;
}

void CCSGameRules::ClientSettingsChanged( CBasePlayer *pPlayer )
{
    CCSPlayer *pCSPlayer = dynamic_cast<CCSPlayer*>( pPlayer );
    if ( pCSPlayer )
    {
        // NOTE[pmf]: Before testing if the name has changed, we first copy it into a buffer of MAX_PLAYER_NAME_LENGTH length,
        // trimming malformed utf8 (truncated) characters at the end (which we get from Steam); otherwise, when this
        // correction happens as a result of CBasePlayer::SetPlayerName(), we'll always see the name as different,
        // and continue to spam name change attempts.
		if ( CSGameRules()->IsPlayingCoopMission() && pCSPlayer->IsBot() )
		{
			pCSPlayer->ChangeName( pCSPlayer->GetPlayerName() );
		}
		else if ( CanClientCustomizeOwnIdentity() )
		{
			char szNewName[MAX_PLAYER_NAME_LENGTH];
			V_UTF8_strncpy( szNewName, engine->GetClientConVarValue( pCSPlayer->entindex(), "name" ), sizeof(szNewName) );
			const char *pszOldName = pCSPlayer->GetPlayerName();
			if ( pszOldName[0] != 0 && Q_strcmp( pszOldName, szNewName ) )		
			{
				pCSPlayer->ChangeName( szNewName );		
			}
		}

        pCSPlayer->m_bShowHints = true;
        if ( pCSPlayer->IsNetClient() )
        {
            const char *pShowHints = engine->GetClientConVarValue( ENTINDEX( pCSPlayer->edict() ), "cl_autohelp" );
            if ( pShowHints && atoi( pShowHints ) <= 0 )
            {
                pCSPlayer->m_bShowHints = false;
            }
        }

		//pCSPlayer->UpdateAppearanceIndex();
		
		pCSPlayer->InitTeammatePreferredColor();
	}
}

bool CCSGameRules::CanClientCustomizeOwnIdentity()
{
	extern bool BForcedClientNamesOverridesLoaded();
	return !CCSGameRules::sm_QueuedServerReservation.tournament_teams().size()
		&& !BForcedClientNamesOverridesLoaded();
}

bool CCSGameRules::FAllowNPCs( void )
{
    if ( IsPlayingTraining() )
        return true;

    return true;
}

bool CCSGameRules::IsFriendlyFireOn( void ) const
{
    return mp_friendlyfire.GetBool();
}

bool CCSGameRules::IsLastRoundBeforeHalfTime( void )
{
    if ( HasHalfTime() )
    {
		int numRoundsBeforeHalftime = -1;
		if ( m_match.GetPhase() == GAMEPHASE_PLAYING_FIRST_HALF )
			numRoundsBeforeHalftime = GetOvertimePlaying()
				? ( mp_maxrounds.GetInt() + ( 2 * GetOvertimePlaying() - 1 ) * ( mp_overtime_maxrounds.GetInt() / 2 ) )
				: ( mp_maxrounds.GetInt() / 2 );

        if ( ( numRoundsBeforeHalftime > 0 ) && ( m_match.GetRoundsPlayed() == (numRoundsBeforeHalftime-1) ) )
        {
            return true;
        }
    }

    return false;
}

CON_COMMAND( map_showspawnpoints, "Shows player spawn points (red=invalid). Optionally pass in the duration." )
{
	int duration = 600;

	if ( args.ArgC() == 2 )
	{
		duration = Q_atoi( args[1] );
	}

	CSGameRules()->ShowSpawnPoints( duration );
    
}

void DrawSphere( const Vector& pos, float radius, int r, int g, int b, float lifetime )
{
    Vector edge, lastEdge;
    NDebugOverlay::Line( pos, pos + Vector( 0, 0, 50 ), r, g, b, true, lifetime );

    lastEdge = Vector( radius + pos.x, pos.y, pos.z );
    float angle;
    for( angle=0.0f; angle <= 360.0f; angle += 22.5f )
    {
        edge.x = radius * BotCOS( angle ) + pos.x;
        edge.y = pos.y;
        edge.z = radius * BotSIN( angle ) + pos.z;

        NDebugOverlay::Line( edge, lastEdge, r, g, b, true, lifetime );

        lastEdge = edge;
    }

    lastEdge = Vector( pos.x, radius + pos.y, pos.z );
    for( angle=0.0f; angle <= 360.0f; angle += 22.5f )
    {
        edge.x = pos.x;
        edge.y = radius * BotCOS( angle ) + pos.y;
        edge.z = radius * BotSIN( angle ) + pos.z;

        NDebugOverlay::Line( edge, lastEdge, r, g, b, true, lifetime );

        lastEdge = edge;
    }

    lastEdge = Vector( pos.x, radius + pos.y, pos.z );
    for( angle=0.0f; angle <= 360.0f; angle += 22.5f )
    {
        edge.x = radius * BotCOS( angle ) + pos.x;
        edge.y = radius * BotSIN( angle ) + pos.y;
        edge.z = pos.z;

        NDebugOverlay::Line( edge, lastEdge, r, g, b, true, lifetime );

        lastEdge = edge;
    }
}

CON_COMMAND_F( map_showbombradius, "Shows bomb radius from the center of each bomb site and planted bomb.", FCVAR_CHEAT )
{
    float flBombDamage = 500.0f;
    if ( g_pMapInfo )
        flBombDamage = g_pMapInfo->m_flBombRadius;
    float flBombRadius = flBombDamage * 3.5f;
    Msg( "Bomb Damage is %.0f, Radius is %.0f\n", flBombDamage, flBombRadius );

    CBaseEntity* ent = NULL;
    while ( ( ent = gEntList.FindEntityByClassname( ent, "func_bomb_target" ) ) != NULL )
    {
        const Vector &pos = ent->WorldSpaceCenter();
        DrawSphere( pos, flBombRadius, 255, 255, 0, 10 );
    }

    ent = NULL;
    while ( ( ent = gEntList.FindEntityByClassname( ent, "planted_c4" ) ) != NULL )
    {
        const Vector &pos = ent->WorldSpaceCenter();
        DrawSphere( pos, flBombRadius, 255, 0, 0, 10 );
    }
}

CON_COMMAND_F( map_setbombradius, "Sets the bomb radius for the map.", FCVAR_CHEAT )
{
    if ( args.ArgC() != 2 )
        return;

    if ( !UTIL_IsCommandIssuedByServerAdmin() )
        return;

    if ( !g_pMapInfo )
        CBaseEntity::Create( "info_map_parameters", vec3_origin, vec3_angle );

    if ( !g_pMapInfo )
        return;

    g_pMapInfo->m_flBombRadius = atof( args[1] );
    map_showbombradius( args );
}

//-----------------------------------------------------------------------------
// Purpose: Called when a player joins the game after it's started yet can still spawn in
//-----------------------------------------------------------------------------
void CCSGameRules::SpawningLatePlayer( CCSPlayer* pLatePlayer )
{
    //Reset the round kills number of enemies for the opposite team
    for ( int i = 1; i <= gpGlobals->maxClients; i++ )
    {
        CCSPlayer *pPlayer = (CCSPlayer*) UTIL_PlayerByIndex( i );
        if(pPlayer)
        {
            if(pPlayer->GetTeamNumber() == pLatePlayer->GetTeamNumber())
            {
                continue;
            }
            pPlayer->m_NumEnemiesAtRoundStart++;
        }
    }
}

//-----------------------------------------------------------------------------
// Purpose: Test for "pistol" round, defined as the default starting round
// when players cannot purchase anything primary weapons
//-----------------------------------------------------------------------------
bool CCSGameRules::IsPistolRound()
{
    if ( !IsPlayingClassic() )
        return false;

    return ( ( m_iTotalRoundsPlayed == 0 ) ||
		( HasHalfTime() && ( m_iTotalRoundsPlayed > 0 ) && ( m_iTotalRoundsPlayed == ( mp_maxrounds.GetInt() / 2 ) ) ) )
		&& ( GetStartMoney() <= 800 );
}

void CCSGameRules::PlayerTookDamage(CCSPlayer* player, const CTakeDamageInfo &damageInfo)
{
    CBaseEntity *pInflictor = damageInfo.GetInflictor();
    CBaseEntity *pAttacker = damageInfo.GetAttacker();
    CCSPlayer *pCSScorer = (CCSPlayer *)(GetDeathScorer( pAttacker, pInflictor ));

    if ( player && pCSScorer )
    {
        if (player->GetTeamNumber() == TEAM_CT)
        {
            m_bNoCTsDamaged = false;
        }

        if (player->GetTeamNumber() == TEAM_TERRORIST)
        {
            m_bNoTerroristsDamaged = false;
        }
        // set the first blood if this is the first and the victim is on a different team then the player
        if ( m_pFirstBlood == NULL && pCSScorer != player && pCSScorer->GetTeamNumber() != player->GetTeamNumber() )
        {
            m_pFirstBlood = pCSScorer;
            m_firstBloodTime = gpGlobals->curtime - m_fRoundStartTime;
        }
    }
}

CCSMatch* CCSGameRules::GetMatch( void )
{
    return &m_match;
}

float CCSGameRules::SendPlayerItemDropsToClient()
{
	CReliableBroadcastRecipientFilter broadcastFilter;
	CCSUsrMsg_SendPlayerItemDrops msg;
	bool bAnyDrops = false;

	// first randomize the list before we send it
	VectorShuffle( m_ItemsPtrDroppedDuringMatch );

	float duration = 0.0f;

	msg.mutable_entity_updates()->Reserve( m_ItemsPtrDroppedDuringMatch.Count() );
	for ( int i = 0; i < m_ItemsPtrDroppedDuringMatch.Count(); i++ )
	{
		const CEconItemPreviewDataBlock& iteminfo = *m_ItemsPtrDroppedDuringMatch[i];
		uint32 accountId = iteminfo.accountid();

		// Get player ent index if they are connected
		// For development, we 'trust' the entindex on items dropped to accountid 0;
		// these are items we dropped to bots for testing.
		int nEntIndex = -1;
		if ( accountId )
		{
			for ( int j = 1; j <= MAX_PLAYERS; j++ )
			{
				if ( CBasePlayer *pPlayer = UTIL_PlayerByIndex( j ) )
				{
					CSteamID steamID;
					pPlayer->GetSteamID( &steamID );
					if ( steamID.GetAccountID() == accountId )
					{
						nEntIndex = pPlayer->entindex();
						break;
					}
				}
			}

			// player disconnected after receiving drop, don't send to client
			if ( nEntIndex < 0 )
				continue;
		}

		// We are adding this item, so we know at least one drop is being sent.
		bAnyDrops = true;
		CEconItemPreviewDataBlock* pItemInfo = msg.add_entity_updates();
		*pItemInfo = iteminfo;
		if ( nEntIndex >= 0 )
			pItemInfo->set_entindex( nEntIndex );

		// We've added this item to the list to send to clients, so calculate the time
		// the client will display this item
		int rarity = iteminfo.rarity();
		static const ConVar* rarity_duration_convars[] = {
			&sv_endmatch_item_drop_interval,
			&sv_endmatch_item_drop_interval,
			&sv_endmatch_item_drop_interval_rare,
			&sv_endmatch_item_drop_interval_mythical,
			&sv_endmatch_item_drop_interval_legendary,
			&sv_endmatch_item_drop_interval_ancient,
			&sv_endmatch_item_drop_interval_ancient,
		};

		if ( rarity >= 0 && rarity < V_ARRAYSIZE( rarity_duration_convars ) && rarity_duration_convars[ rarity ] != nullptr )
		{
			duration += rarity_duration_convars[ rarity ]->GetFloat();
		}
		else
		{
			duration += sv_endmatch_item_drop_interval.GetFloat();
		}
	}

	ClearItemsDroppedDuringMatch();

	if ( bAnyDrops )
	{
		SendUserMessage( broadcastFilter, CS_UM_SendPlayerItemDrops, msg );

		// always show for at least 2 seconds
		return MAX( 2.0f, duration );
	}

	// no drops, don't wait for them
	return 0.0f;
}

void CCSGameRules::FreezePlayers( void )
{	
    for ( int i = 1; i <= MAX_PLAYERS; i++ )
    {
        CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );

        if ( pPlayer )
        {
            pPlayer->AddFlag( FL_FROZEN );
        }
    }
}

void CCSGameRules::ClearGunGameData( void )
{
    for ( int i = 1; i <= MAX_PLAYERS; i++ )
    {
        CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );
        if ( pPlayer )
        {
            // Reset all players' progressive weapon index values
            pPlayer->ResetTRBombModeData();
            pPlayer->ClearGunGameProgressiveWeaponIndex();
            pPlayer->ResetTRBombModeWeaponProgressFlag();
            pPlayer->ResetTRBombModeKillPoints();
        }
    }

    m_iMaxGunGameProgressiveWeaponIndex = 0;
}


void CCSGameRules::SwitchTeamsAtRoundReset( void )
{
	m_bForceTeamChangeSilent = true;
    m_bSwitchingTeamsAtRoundReset = true;

    for ( int i = 1; i <= MAX_PLAYERS; i++ )
    {
        CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );
        if ( pPlayer )
        {
            pPlayer->SetPendingTeamNum( pPlayer->GetTeamNumber() ) ;
            if ( pPlayer->GetAssociatedTeamNumber() == TEAM_CT || pPlayer->GetAssociatedTeamNumber() == TEAM_TERRORIST )
            {
                pPlayer->SwitchTeamsAtRoundReset();
            }
        }
    }

	// Swap custom teamnames
}

void CCSGameRules::AddTeamAccount( int team, TeamCashAward::Type reason )
{
    int amount = TeamCashAwardValue( reason );

    AddTeamAccount( team, reason, amount );
}

void CCSGameRules::AddTeamAccount( int team, TeamCashAward::Type reason, int amount, const char* szAwardText )
{
    if ( !TeamCashAwardsEnabled() )
	{
        DevMsg( "WARNING: Trying to award %d to team %d, but mp_teamcashawards is 0!\n", amount, team );
		return;
	}

	if ( amount == 0 )
		return;

	if ( IsPlayingCoopGuardian() || IsPlayingCoopMission() )
	{
		int nOtherTeam = GetCoopEnemyTeamNumber();
		// in this mode, bots are on the other side and we handle their money ourselves
		if ( team == nOtherTeam )
			return;
	}

//	float flDisplayedBonus = 1.0f;
//
//	if ( CSGameRules() )
//	{
//		// Modify with slow money multiplier rules
//		amount = CSGameRules()->ModifyMoneyRewardByMultiplier( true, amount, &flDisplayedBonus );
//	}

	char customAwardText[256];
	bool bUsingCustom = false;
    const char* awardReasonToken = NULL;
    
    switch ( reason )
    {
    case TeamCashAward::TERRORIST_WIN_BOMB:
        awardReasonToken = "#Team_Cash_Award_T_Win_Bomb";
        break;
    case TeamCashAward::ELIMINATION_HOSTAGE_MAP_T:
    case TeamCashAward::ELIMINATION_HOSTAGE_MAP_CT:
        awardReasonToken = "#Team_Cash_Award_Elim_Hostage";
        break;
    case TeamCashAward::ELIMINATION_BOMB_MAP:
        awardReasonToken = "#Team_Cash_Award_Elim_Bomb";
        break;
    case TeamCashAward::WIN_BY_TIME_RUNNING_OUT_HOSTAGE:
    case TeamCashAward::WIN_BY_TIME_RUNNING_OUT_BOMB:
        awardReasonToken = "#Team_Cash_Award_Win_Time";
        break;
    case TeamCashAward::WIN_BY_DEFUSING_BOMB:
        awardReasonToken = "#Team_Cash_Award_Win_Defuse_Bomb";
        break;
    case TeamCashAward::WIN_BY_HOSTAGE_RESCUE:
		if ( mp_hostages_rescuetowin.GetInt() == 1 ) 
			awardReasonToken = "#Team_Cash_Award_Win_Hostage_Rescue";
		else
			awardReasonToken = "#Team_Cash_Award_Win_Hostages_Rescue";
        break;
    case TeamCashAward::LOSER_BONUS:
		if ( amount > 0 )
			awardReasonToken = "#Team_Cash_Award_Loser_Bonus";
		else
			awardReasonToken = "#Team_Cash_Award_Loser_Bonus_Neg";


        break;
    case TeamCashAward::RESCUED_HOSTAGE:
        awardReasonToken = "#Team_Cash_Award_Rescued_Hostage";
        break;
	case TeamCashAward::HOSTAGE_INTERACTION:
		awardReasonToken = "#Team_Cash_Award_Hostage_Interaction";
		break;
    case TeamCashAward::HOSTAGE_ALIVE:
        awardReasonToken = "#Team_Cash_Award_Hostage_Alive";
        break;
    case TeamCashAward::PLANTED_BOMB_BUT_DEFUSED:
        awardReasonToken = "#Team_Cash_Award_Planted_Bomb_But_Defused";
        break;
	case TeamCashAward::SURVIVE_GUARDIAN_WAVE:
		awardReasonToken = "#Team_Cash_Award_Survive_GuardianMode_Wave";
		break;
		
	case TeamCashAward::CUSTOM_AWARD:
		{
			if ( szAwardText && szAwardText[0] != 0 )
			{
				awardReasonToken = "#Team_Cash_Award_Custom";
				Q_snprintf( customAwardText, sizeof( customAwardText ), "%s", szAwardText );
				bUsingCustom = true;
			}
			else
			{
				awardReasonToken = "#Team_Cash_Award_Generic";
			}
			
			break;
		}
    default:
        break;
    }

	bool bTeamHasClinchedVictory = false;
	if ( IsPlayingClassic() && !IsPlayingTraining() )
	{
		int iNumWinsToClinch = ( mp_maxrounds.GetInt() / 2 ) + 1 + GetOvertimePlaying() * ( mp_overtime_maxrounds.GetInt() / 2 );
		bTeamHasClinchedVictory = MatchCanClinch() && ( m_match.GetCTScore() >= iNumWinsToClinch ) || ( m_match.GetTerroristScore() >= iNumWinsToClinch );
	}

	char strAmount[64];
	Q_snprintf( strAmount, sizeof( strAmount ), "%s$%d", amount >= 0 ? "+" : "-", abs( amount ));

	// Update all QMM records for the teams
	bool bTeamsArePlayingSwitchedSides = AreTeamsPlayingSwitchedSides();
	const uint32 kNumPlayersPerSide = 5;
	FOR_EACH_MAP_FAST( m_mapQueuedMatchmakingPlayersData, idxQMMEntry )
	{
		CQMMPlayerData_t &qmmData = * m_mapQueuedMatchmakingPlayersData.Element( idxQMMEntry );
		int iRulesTeam = bTeamsArePlayingSwitchedSides
			? ( (qmmData.m_iDraftIndex < ( int ) kNumPlayersPerSide)?TEAM_TERRORIST:TEAM_CT )
			: ( (qmmData.m_iDraftIndex < ( int ) kNumPlayersPerSide)?TEAM_CT:TEAM_TERRORIST );
		if ( ( iRulesTeam == team ) && !qmmData.m_bReceiveNoMoneyNextRound )
		{	// we set all records even though alive players will stomp them when receiving money in the loop right below
			qmmData.m_cash = clamp( (int)( qmmData.m_cash + amount ), 0, GetMaxMoney() );
		}
	}

    for ( int i = 1; i <= gpGlobals->maxClients; i++ )
    {
        CCSPlayer *pPlayer = (CCSPlayer*) UTIL_PlayerByIndex( i );

        if ( !pPlayer || pPlayer->GetTeamNumber() != team )
			continue;

        // hand out team cash awards from previous round
		if ( pPlayer->DoesPlayerGetRoundStartMoney() )
		{
			pPlayer->AddAccountFromTeam( amount, true, reason );
			if ( dev_reportmoneychanges.GetBool() )
				Msg( "%s	%d		(total: %d)	AddAccountFromTeam: REASON: %s\n", pPlayer->GetPlayerName(), amount, pPlayer->GetAccountBalance(), awardReasonToken );

			if ( bUsingCustom )
			{
				ClientPrint( pPlayer, HUD_PRINTTALK, awardReasonToken, strAmount, customAwardText );
			}
			else if ( !IsLastRoundBeforeHalfTime() && ( m_match.GetPhase() != GAMEPHASE_HALFTIME ) &&
				( m_match.GetRoundsPlayed() != mp_maxrounds.GetInt() + GetOvertimePlaying() * mp_overtime_maxrounds.GetInt() ) && !bTeamHasClinchedVictory )
			{
				ClientPrint( pPlayer, HUD_PRINTTALK, awardReasonToken, strAmount );
			}
		}
		else
		{
			if ( !IsLastRoundBeforeHalfTime() && ( m_match.GetPhase() != GAMEPHASE_HALFTIME ) &&
				( m_match.GetRoundsPlayed() != mp_maxrounds.GetInt() + GetOvertimePlaying() * mp_overtime_maxrounds.GetInt() ) && !bTeamHasClinchedVictory )
			{
				// TODO: This code assumes on there only being 2 possible reasons for DoesPlayerGetRoundStartMoney returning false: Suicide or Running down the clock as T.
				// This code should not make that assumption and the awardReasonToken should probably be plumbed to express those properly.
				if ( pPlayer->GetHealth() > 0 )
				{
					ClientPrint( pPlayer, HUD_PRINTTALK, "#Team_Cash_Award_No_Income", strAmount );
				}
				else
				{
					ClientPrint( pPlayer, HUD_PRINTTALK, "#Team_Cash_Award_No_Income_Suicide", strAmount );
				}
			}



		}
    }
}

#endif // !CLIENT_DLL

bool CCSGameRules::IsCSGOBirthday( void )
{
	return sv_party_mode.GetBool();
}

int CCSGameRules::GetStartMoney( void )
{
	return IsWarmupPeriod() ? GetMaxMoney() : ( GetOvertimePlaying() ? mp_overtime_startmoney.GetInt() : mp_startmoney.GetInt() );
}


int	 CCSGameRules::GetMaxMoney( void )
{
	return mp_maxmoney.GetInt();
}


int  CCSGameRules::GetBetweenRoundMoney( void )
{
	return mp_afterroundmoney.GetInt();
}


bool CCSGameRules::PlayerCashAwardsEnabled( void )
{
	return mp_playercashawards.GetBool();
}


bool CCSGameRules::TeamCashAwardsEnabled( void )
{
	return mp_teamcashawards.GetBool();
}

bool CCSGameRules::IsPlayingGunGameProgressive( void ) const
{
    return ( IsPlayingGunGame() &&
             g_pGameTypes->GetCurrentGameMode() == CS_GameMode::GunGame_Progressive );
}

bool CCSGameRules::IsPlayingGunGameDeathmatch( void ) const
{
	return ( IsPlayingGunGame() &&
		g_pGameTypes->GetCurrentGameMode() == CS_GameMode::GunGame_Deathmatch );
}

bool CCSGameRules::IsPlayingTeamDM( void ) const
{
	return IsPlayingGunGameDeathmatch() && mp_dm_teammode.GetBool();
}

bool CCSGameRules::IsPlayingGunGameTRBomb( void ) const
{
    return ( IsPlayingGunGame() &&
             g_pGameTypes->GetCurrentGameMode() == CS_GameMode::GunGame_Bomb );
}

bool CCSGameRules::IsPlayingClassicCasual( void ) const
{
    return ( IsPlayingClassic() &&
        g_pGameTypes->GetCurrentGameMode() == CS_GameMode::Classic_Casual);
}

bool CCSGameRules::IsPlayingAnyCompetitiveStrictRuleset( void ) const
{
    return ( IsPlayingClassic() && (
		( g_pGameTypes->GetCurrentGameMode() == CS_GameMode::Classic_Competitive ) ||
		( g_pGameTypes->GetCurrentGameMode() == CS_GameMode::Classic_ScrimComp5v5 ) ||
		( g_pGameTypes->GetCurrentGameMode() == CS_GameMode::Classic_ScrimComp2v2 )
		) );
}

bool CCSGameRules::IsPlayingCoopGuardian( void ) const
{
	return ( IsPlayingCooperativeGametype() &&
			 g_pGameTypes->GetCurrentGameMode() == CS_GameMode::Cooperative_Guardian );
}

bool CCSGameRules::IsPlayingCoopMission( void ) const
{
	return ( IsPlayingCooperativeGametype() &&
			 g_pGameTypes->GetCurrentGameMode() == CS_GameMode::Cooperative_Mission );
}

bool CCSGameRules::IsPlayingSurvival( void ) const
{
	return ( IsPlayingFreeForAllGametype() &&
		g_pGameTypes->GetCurrentGameMode() == CS_GameMode::FreeForAll_Survival );
}

// optionally pass in a round that we will use as index into stats arrays to bounds check it
bool CCSGameRules::ShouldRecordMatchStats( int round ) const
{
	if ( IsWarmupPeriod() )
		return false;

	if ( !IsPlayingCoopMission() && !IsPlayingClassic() && !IsPlayingGunGameTRBomb() )
		return false;

//	Assert( round >= 0 && round < MAX_MATCH_STATS_ROUNDS );
	return ( round >= 0 && round < MAX_MATCH_STATS_ROUNDS );
}




int CCSGameRules::GetTotalRoundsPlayedThisPeriod() const
{
	int nResult;

	// regulation time
	if ( !GetOvertimePlaying() )
	{
		nResult = GetTotalRoundsPlayed();
	}
	else // overtime
	{
		nResult = ( GetTotalRoundsPlayed() - mp_maxrounds.GetInt() ) % mp_overtime_maxrounds.GetInt();
	}

	return nResult;
}


// returns rounds played since start of match or start of current overtime
// since we reset matchstats every overtime
// NOTE: this does not roll over until the start of the next round

int CCSGameRules::GetCurrentRoundIndexForMatchStats() const
{
	if ( CSGameRules()->IsWarmupPeriod() )
		return 0;

	int nResult = GetTotalRoundsPlayedThisPeriod();

	// if the round is over then GetTotalRoundsPlayed was incremented but we don't consider it over for stats purposes.
	if ( CSGameRules()->m_iRoundWinStatus != WINNER_NONE && 
		CSGameRules()->m_eRoundWinReason != Game_Commencing )
	{
		nResult--;
	}

	return nResult;
}

// returns rounds played since start of match or start of current overtime
// since we reset matchstats every overtime
int CCSGameRules::GetMaxRoundsThisPeriod() const
{
	// regulation time
	if ( !GetOvertimePlaying() )
	{
		return mp_maxrounds.GetInt();
	}
	else // overtime
	{
		return mp_overtime_maxrounds.GetInt();
	}
}


bool CCSGameRules::IsQueuedMatchmaking() const
{
#ifndef CLIENT_DLL
	static ConVarRef sv_mmqueue_reservation( "sv_mmqueue_reservation" );
	return sv_mmqueue_reservation.GetString()[0] == 'Q';
#else
	return m_bIsQueuedMatchmaking.Get();
#endif
}

bool CCSGameRules::IsValveDS() const
{
#ifndef CLIENT_DLL
		return false;
#else
	return m_bIsValveDS.Get();
#endif
}

bool CCSGameRules::IsQuestEligible() const
{
#ifndef CLIENT_DLL
		return false;
#else
	return m_bIsQuestEligible.Get();
#endif
}

bool CCSGameRules::IsPlayingOffline( void ) const
{
#ifndef CLIENT_DLL
    if ( engine->IsDedicatedServer() )
        return false;
#endif
//	int humansInGame = UTIL_HumansInGame( true );
//	if ( humansInGame > 1 )
//		return false;

    extern ConVar game_online;
    return !game_online.GetBool();
}

bool CCSGameRules::IsAwardsProgressAllowedForBotDifficulty( void ) const
{
    return ( !IsPlayingOffline() || ( GetCustomBotDifficulty() >= CUSTOM_BOT_MIN_DIFFICULTY_FOR_AWARDS_PROGRESS ) );
}

bool CCSGameRules::IsPlayingCustomGametype( void ) const
{
	return ( g_pGameTypes->GetCurrentGameType() == CS_GameType_Custom );
}

bool CCSGameRules::IsPlayingTraining( void ) const
{
    return ( g_pGameTypes->GetCurrentGameType() == CS_GameType_Training );
}

bool CCSGameRules::IsPlayingClassic( void ) const
{
    return ( g_pGameTypes->GetCurrentGameType() == CS_GameType_Classic );
}

bool CCSGameRules::IsPlayingGunGame( void ) const
{
    return ( g_pGameTypes->GetCurrentGameType() == CS_GameType_GunGame );
}

bool CCSGameRules::IsPlayingCooperativeGametype( void ) const
{
	return ( g_pGameTypes->GetCurrentGameType() == CS_GameType_Cooperative );
}

bool CCSGameRules::IsPlayingFreeForAllGametype( void ) const
{
	return ( g_pGameTypes->GetCurrentGameType() == CS_GameType_FreeForAll );
}

int	CCSGameRules::GetCustomBotDifficulty( void ) const
{
    return ( g_pGameTypes->GetCustomBotDifficulty() );
}

bool CCSGameRules::ForceSplitScreenPlayersOnToSameTeam( void )
{
    extern ConVar game_online;
    return game_online.GetBool();
}

ConVar mp_solid_teammates("mp_solid_teammates", "1", FCVAR_REPLICATED | FCVAR_RELEASE, "How solid are teammates: 0 = transparent; 1 = fully solid; 2 = can stand on top of heads" );
ConVar mp_free_armor("mp_free_armor", "0", FCVAR_REPLICATED | FCVAR_RELEASE, "Determines whether kevlar (1+) and/or helmet (2+) are given automatically." );
ConVar mp_max_armor( "mp_max_armor", "2", FCVAR_REPLICATED | FCVAR_RELEASE, "Determines the highest level of armor allowed to be purchased. (0) None, (1) Kevlar, (2) Helmet" );
ConVar mp_halftime("mp_halftime", "0", FCVAR_REPLICATED | FCVAR_RELEASE, "Determines whether the match switches sides in a halftime event.");
ConVar mp_randomspawn("mp_randomspawn", "0", FCVAR_REPLICATED | FCVAR_RELEASE, "Determines whether players are to spawn. 0 = default; 1 = both teams; 2 = Terrorists; 3 = CTs." );
ConVar mp_randomspawn_los("mp_randomspawn_los", "1", FCVAR_REPLICATED | FCVAR_RELEASE, "If using mp_randomspawn, determines whether to test Line of Sight when spawning." );
ConVar mp_randomspawn_dist( "mp_randomspawn_dist", "0", FCVAR_REPLICATED | FCVAR_RELEASE, "If using mp_randomspawn, determines whether to test distance when selecting this spot." );

// Returns true if teammates are solid obstacles in the current game mode
bool CCSGameRules::IsTeammateSolid( void ) const
{
    switch( mp_solid_teammates.GetInt() )
	{
	case 0:
	case 2:
		return false;
	default:
		return true;
	}
}

// Returns true if armor is given automatically.
bool CCSGameRules::IsArmorFree( void ) const
{
    return mp_free_armor.GetInt() >= 1;
}

// Returns true if helmets are given automatically
bool CCSGameRules::IsHelmetFree( void ) const
{
	return mp_free_armor.GetInt() >= 2;
}

// Returns true if the game is to be split into two halves.
bool CCSGameRules::HasHalfTime( void ) const
{
	return mp_halftime.GetBool();
}

int CCSGameRules::GetWeaponScoreForDeathmatch( int nPos, int* pBonusScore )
{
	CSWeaponID wepID = WEAPON_NONE;

	// have to set this to something because the compiler can't figure out that this is always
	// initialized on any codepath where wepId != WEAPON_NONE
	const CEconItemView *pGenericWeaponView = nullptr;

	// $$$REI TODO use the player's actual weapon loadout here as opposed to a random weapon
	// $$$REI      that shares its loadout slot

	for ( int i = 0; i < GetItemSchema()->GetItemDefinitionCount(); i++ )
	{
		const CCStrike15ItemDefinition* pItemDef = assert_cast<const CCStrike15ItemDefinition*>( GetItemSchema()->GetItemDefinitionByMapIndex( i ) );

		// not a base item
		if ( pItemDef->GetDefinitionIndex() >= 256 )
			continue;

		// not in the correct loadout slot
		if ( pItemDef->GetDefaultLoadoutSlot() != nPos )
			continue;

		// found one!
		// NOTE: This only works because we assume the first defined item that uses a loadout slot is the 'base' item for that slot.
		wepID = WeaponIDFromDefinition( pItemDef );
		pGenericWeaponView = CSInventoryManager()->GetReferenceEconItem( pItemDef->GetDefinitionIndex(), 0 );
		break;
	}

	int nScore;

	if ( wepID == WEAPON_NONE )
	{
		nScore = 0;
	}
	else if ( wepID == WEAPON_KNIFE )
	{
		nScore = 10 + mp_dm_kill_base_score.GetInt();
	}
	else if ( const CCSWeaponInfo* pWeaponInfo = GetWeaponInfoFromItem( pGenericWeaponView ) )
	{
		nScore = mp_dm_kill_base_score.GetInt();

		// Score is based on weapon price, with more expensive (more effective) weapons worth less points.
		int nPrice = pWeaponInfo->GetWeaponPrice( pGenericWeaponView );

		if ( nPrice < 1400 )
			nScore += 2;
		else if ( nPrice < 3500 )
			nScore += 1;
	}
	else
	{
		nScore = 1;
	}

	if ( pBonusScore )
	{
		*pBonusScore = ( ( float )( mp_dm_bonus_percent.GetInt() ) / 100.0f ) * nScore;
	}

	return nScore;
}



float CCSGameRules::GetRestartRoundTime( void ) const
{
	return m_fRoundStartTime;
}

#if !defined( CLIENT_DLL )
CGameCoopMissionManager *CCSGameRules::GetCoopMissionManager( void )
{
	CGameCoopMissionManager *pManager = static_cast< CGameCoopMissionManager* >( m_coopMissionManager.Get() );
	if ( !pManager )
	{
		CBaseEntity *ent = NULL;
		do
		{
			ent = gEntList.FindEntityByClassname( ent, "game_coopmission_manager" );

			if ( ent )
			{
				CSGameRules()->SetCoopMissionManager( ent );
				pManager = GetCoopMissionManager();
			}

		} while ( ent );
	}

	return pManager;
}

void CCSGameRules::CoopSetBotQuotaAndRefreshSpawns( int nMaxEnemiesToSpawn )
{
	RefreshCurrentSpawnPointLists();

	m_iMaxNumTerrorists = nMaxEnemiesToSpawn;

	ConVarRef bot_quota( "bot_quota" );
	int nQuota = UTIL_HumansInGame( true, true ) + MIN( m_iMaxNumTerrorists, m_iSpawnPointCount_Terrorist );
	bot_quota.SetValue( nQuota );

	ShuffleSpawnPointLists();
	SortSpawnPointLists();
}

void CCSGameRules::CoopMissionSpawnFirstEnemies( int nMaxEnemiesToSpawn )
{
	// set to 0 so we start in wave 1
	m_nGuardianModeWaveNumber = 0;

	CoopMissionSpawnNextWave( nMaxEnemiesToSpawn );
}

void CCSGameRules::CoopMissionSetNextRespawnIn( float flSeconds, bool bIncrementWaveNumber )
{
	m_bDontIncrementCoopWave = !bIncrementWaveNumber;

	// if T's are supposed to respawn
	float flNextRespawn = gpGlobals->curtime + flSeconds;
	m_flNextRespawnWave.Set( TEAM_TERRORIST, flNextRespawn );
}

void CCSGameRules::CoopMissionSpawnNextWave( int nMaxEnemiesToSpawn )
{
	// the CT spawn points for respawn are enabled, respawn now - we can work on the appropriate timings later
	int nTeam = TEAM_CT;
	// give health to all of the players on the other team
	for ( int j = 1; j <= MAX_PLAYERS; j++ )
	{
		CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( j ) );
		if ( pPlayer && pPlayer->GetTeamNumber() == nTeam )
		{
			// respawn any dead CTs as well
			if ( pPlayer->IsAlive() == false )
			{
				pPlayer->State_Transition( STATE_GUNGAME_RESPAWN );
			}
		}
	}

	// make sure this is 0 so we start from the beginning
	m_iNextTerroristSpawnPoint = 0;

	// do a refresh here, but we don't refresh as frequently in coop mission
	// we would normally refresh every time a spot enabled or disabled and a few other places
	// just do it here once before we spawn new guys
	RefreshCurrentSpawnPointLists();

	m_iMaxNumTerrorists = nMaxEnemiesToSpawn;

	ShuffleSpawnPointLists();
	SortSpawnPointLists();

	// make sure that there are always enough bots
	ConVarRef bot_quota( "bot_quota" );
	int nQuota = UTIL_HumansInGame( true, true ) + MIN( m_iMaxNumTerrorists, m_iSpawnPointCount_Terrorist );
	bot_quota.SetValue( nQuota );

	//DevMsg( ">> Setting bot_quota to %d\n", nQuota );

	// if T's are supposed to respawn
	CoopMissionSetNextRespawnIn( -1, true );
}

void CCSGameRules::CoopMissionRespawnDeadPlayers( void )
{
	// do a refresh here, but we don't refresh as frequently in coop mission
	// we would normally refresh every time a spot enabled or disabled and a few other places
	// just do it here once before we spawn new guys

	if ( !IsWarmupPeriod() )
		RefreshCurrentSpawnPointLists();

	// the CT spawn points for respawn are enabled, respawn now - we can work on the appropriate timings later
	CTeam *pTeam = GetGlobalTeam( TEAM_CT );
	CUtlVector < CCSPlayer* > vecPlayers;
	if ( pTeam->GetHumanMembers( &vecPlayers ) )
	{
		FOR_EACH_VEC( vecPlayers, i )
		{
			// respawn any dead CTs as well
			if ( vecPlayers[i]->IsAlive() == false )
			{
				vecPlayers[i]->State_Transition( STATE_GUNGAME_RESPAWN );
			}
		}
	}
}

void CCSGameRules::CoopCollectBonusCoin( void )
{
	m_coopBonusCoinsFound++;
	CRecipientFilter filter;
	filter.MakeReliable();
	filter.AddRecipientsByTeam( GetGlobalTeam( TEAM_CT ) );
	char szCoins[32];
	Q_snprintf( szCoins, sizeof( szCoins ), "%d", ( int )m_coopBonusCoinsFound );
	UTIL_ClientPrintFilter( filter, HUD_PRINTCENTER, "#SFUIHUD_InfoPanel_Coop_CollectCoin", szCoins );
}

#endif

#ifdef CLIENT_DLL

CCSGameRules::CCSGameRules()
{
	// Reset borrow music convar because player indices scramble after level transition.
	cl_borrow_music_from_player_index.SetValue( cl_borrow_music_from_player_index.GetDefault() );

    InitializeGameTypeAndMode();

	// Set the bestof maps
	m_numBestOfMaps = 0;

	m_fWarmupPeriodStart = gpGlobals->curtime;
	m_fWarmupPeriodEnd = 0;

	// Set global gifts state
	m_numGlobalGiftsGiven = 0;
	m_numGlobalGifters = 0;
	m_numGlobalGiftsPeriodSeconds = 0;
	for ( int j = 0; j < MAX_GIFT_GIVERS_FEATURED_COUNT; ++ j )
	{
		m_arrFeaturedGiftersAccounts.Set( j, 0 );
		m_arrFeaturedGiftersGifts.Set( j, 0 );
	}

	for ( int j = 0; j < MAX_TOURNAMENT_ACTIVE_CASTER_COUNT; ++ j )
	{
		m_arrTournamentActiveCasterAccounts.Set( j, 0 );
	}

	for ( int j = 0; j < MAX_MATCH_STATS_ROUNDS; ++j )
	{
		m_iMatchStats_PlayersAlive_T.GetForModify( j ) = 0x3F;
		m_iMatchStats_PlayersAlive_CT.GetForModify( j ) = 0x3F;
	}

	ResetCasterConvars();

	m_szTournamentEventName.GetForModify()[0] = 0;
	m_szTournamentEventStage.GetForModify()[0] = 0;
	m_szTournamentPredictionsTxt.GetForModify()[0] = 0;
	m_szMatchStatTxt.GetForModify()[ 0 ] = 0;
	m_nTournamentPredictionsPct = 0;

	m_bPlayAllStepSoundsOnServer = false;

	HOOK_MESSAGE( SendPlayerItemDrops );
	HOOK_MESSAGE( SendPlayerItemFound );
	m_bMarkClientStopRecordAtRoundEnd = false;

}

CCSGameRules::~CCSGameRules()
{
	ResetCasterConvars();

	for ( int j = 0; j < MAX_TOURNAMENT_ACTIVE_CASTER_COUNT; ++ j )
	{
		m_arrTournamentActiveCasterAccounts.Set( j, 0 );
	}

}

// CLIENT
bool __MsgFunc_SendPlayerItemDrops( const CCSUsrMsg_SendPlayerItemDrops &msg )
{
	if ( CSGameRules() )
	{
		//CSGameRules()->ClearItemsDroppedDuringMatch();
		for ( int i = 0; i < msg.entity_updates_size(); i++ )
		{
			const CEconItemPreviewDataBlock &update = msg.entity_updates( i );
			CSGameRules()->RecordPlayerItemDrop( update );
		}
	}

	return true;
}

void CCSGameRules::MarkClientStopRecordAtRoundEnd( bool bStop )
{
	m_bMarkClientStopRecordAtRoundEnd = bStop;
}

void CCSGameRules::OpenBuyMenu( int nPlayerID )
{
	// skip buy menu during demo playback
	if ( engine->IsPlayingDemo() || engine->IsPlayingTimeDemo() )
		return;

#ifdef PANORAMA_ENABLE
	if( GameUI().IsPanoramaEnabled() )
	{
		if ( !CSGameRules()->IsPlayingGunGameTRBomb() && !CSGameRules()->IsPlayingGunGameProgressive() )
			panorama::DispatchEvent( EventOpenBuyMenu(), nullptr );
	}
	else
#endif
	{
		GetViewPortInterface()->ShowPanel( PANEL_BUY, true );
	}

	IGameEvent *pEvent = gameeventmanager->CreateEvent( "buymenu_open" );
	if ( pEvent )
	{
		pEvent->SetInt("userid", nPlayerID );
		gameeventmanager->FireEventClientSide( pEvent );
	}
}

void CCSGameRules::CloseBuyMenu( int nPlayerID )
{
	// skip buy menu during demo playback
	if ( engine->IsPlayingDemo() || engine->IsPlayingTimeDemo() )
		return;

#ifdef PANORAMA_ENABLE
	if( GameUI().IsPanoramaEnabled() )
	{
		panorama::DispatchEvent( EventCloseBuyMenu(), nullptr );
	}
	else
#endif
	{
		GetViewPortInterface()->ShowPanel( PANEL_BUY, false );
	}

	IGameEvent *pEvent = gameeventmanager->CreateEvent( "buymenu_close" );
	if ( pEvent )
	{
		pEvent->SetInt("userid", nPlayerID );
		gameeventmanager->FireEventClientSide( pEvent );
	}

	// update the inventory
	CCSPlayer *pPlayer = static_cast<CCSPlayer*>( UTIL_PlayerByIndex(engine->GetPlayerForUserID( nPlayerID )) );
	C_WeaponCSBase *pWeapon = (pPlayer && pPlayer->IsLocalPlayer()) ? pPlayer->GetActiveCSWeapon() : NULL ;
	if ( pWeapon )
	{
		if ( IGameEvent* pSwitchEvent = gameeventmanager->CreateEvent( "weaponhud_selection" ) )
		{
			pSwitchEvent->SetInt( "userid", pPlayer->GetUserID() );
			pSwitchEvent->SetInt( "mode", kWeaponHud_Select_Switch );
			pSwitchEvent->SetInt( "entindex", pWeapon->entindex() );
			gameeventmanager->FireEventClientSide( pSwitchEvent );
		}
	}
}

// Is this workshop id one of the official maps we keep on the workshop for demo playback
bool Helper_IsOfficialCompatibilityVersionWorkshopMap( PublishedFileId_t ullID )
{
	return false;
}

/*static*/ const wchar_t* CCSGameRules::GetFriendlyMapName( const char* szShortName )
{
    // Look up the nice version of the name.
    char szToken[MAX_MAP_NAME+10+1]; // includes prefix size
    szToken[0] = '\0';
    V_snprintf( szToken, ARRAYSIZE( szToken ), "#SFUI_Map_%s", szShortName );
    wchar_t* szTranslated = g_pVGuiLocalize->Find( szToken );
    if ( szTranslated )
        return szTranslated;

    static wchar_t wszMapName[128];
    g_pVGuiLocalize->ConvertANSIToUnicode(szShortName, wszMapName, sizeof(wszMapName));
    return wszMapName;
}

/*static*/ bool CCSGameRules::GetFriendlyMapNameToken( const char* szShortName, char* szOutBuffer, int nBuffSize )
{
    // Create the nice version of the name.
    CreateFriendlyMapNameToken( szShortName, szOutBuffer, nBuffSize );

    if ( g_pVGuiLocalize->Find( szOutBuffer ) )
        return true;

    return false;
}

char const * CCSGameRules::GetTournamentEventName() const
{
	const char *sz = m_szTournamentEventName.Get();
	if ( sz && *sz && ( sz[0] != '#' ) )
	{
		// Localize it:
		static char const * const arrEvents[] = { ""
,"The 2013 DreamHack SteelSeries CS:GO Championship"
,"The Valve DPR Championship"
,"The 2014 EMS One Katowice CS:GO Championship"
,"ESL One Cologne 2014 CS:GO Championship"
,"The 2014 DreamHack CS:GO Championship"
,"ESL One Katowice 2015 CS:GO Championship"
,"ESL One Cologne 2015 CS:GO Championship"
,"DreamHack Cluj-Napoca 2015 CS:GO Championship"
,"MLG Columbus 2016 CS:GO Championship"
,"ESL One Cologne 2016 CS:GO Championship"
,"ELEAGUE Atlanta 2017 CS:GO Championship"
,"PGL 2017 Krakow CS:GO Major Championship"
,"ELEAGUE 2018 Boston CS:GO Major Championship"
,"FACEIT 2018 London CS:GO Major Championship"
,"IEM 2019 Katowice CS:GO Major Championship"
		};
		for ( int k = Q_ARRAYSIZE( arrEvents ) - 1; k > 0; -- k )
		{
			if ( !V_strcmp( sz, arrEvents[k] ) )
			{
				V_snprintf( const_cast< char * >( sz ), MAX_PATH, "#CSGO_Tournament_Event_Name_%d", k );
				return sz;
			}
		}
	}
	return m_szTournamentEventName;
}

char const * CCSGameRules::GetTournamentEventStage() const
{
	const char *sz = m_szTournamentEventStage.Get();
	if ( sz && *sz && ( sz[ 0 ] != '#' ) )
	{
		// Localize it:
		static char const * const arrEvents[] = { ""
,"Exhibition"
,"Group Stage | First Stage"
,"BYOC"
,"Valve Pre-Event Test"
,"Match 1 of 3 | Quarterfinal"
,"Match 2 of 3 | Quarterfinal"
,"Match 3 of 3 | Quarterfinal"
,"Match 1 of 3 | Semifinal"
,"Match 2 of 3 | Semifinal"
,"Match 3 of 3 | Semifinal"
,"Match 1 of 3 | Grand Final"
,"Match 2 of 3 | Grand Final"
,"Match 3 of 3 | Grand Final"
,"All-Star"
,"Group Stage | Winners Match"
,"Group Stage | Elimination Match"
,"Group Stage | Decider Match"
,"Qualification"
,"Match 2 of 3 | Qualification"
,"Match 3 of 3 | Qualification"
,"Group Stage | Decider Match 1 of 3"
,"Group Stage | Decider Match 2 of 3"
,"Group Stage | Decider Match 3 of 3"
,"Group Stage | Second Stage (Upper Pool)"
,"Group Stage | Second Stage (Lower Pool)"
,"Group Stage | Third Stage"
,"Preliminary Stage | First Stage"
,"Preliminary Stage | Winners Match"
,"Preliminary Stage | Elimination Match"
,"Preliminary Stage | Decider Match"
,"Preliminary Stage | Second Stage (Upper Pool)"
,"Preliminary Stage | Second Stage (Lower Pool)"
,"Preliminary Stage | Third Stage"
,"Preliminary Stage | Decider Match 1 of 3"
,"Preliminary Stage | Decider Match 2 of 3"
,"Preliminary Stage | Decider Match 3 of 3"
,"Preliminary Stage | Winners Match 1 of 3"
,"Preliminary Stage | Winners Match 2 of 3"
,"Preliminary Stage | Winners Match 3 of 3"
,"Preliminary Stage | Elimination Match 1 of 3"
,"Preliminary Stage | Elimination Match 2 of 3"
,"Preliminary Stage | Elimination Match 3 of 3"
,"Group Stage | Winners Match 1 of 3"
,"Group Stage | Winners Match 2 of 3"
,"Group Stage | Winners Match 3 of 3"
,"Group Stage | Elimination Match 1 of 3"
,"Group Stage | Elimination Match 2 of 3"
,"Group Stage | Elimination Match 3 of 3"
		};
		for ( int k = Q_ARRAYSIZE( arrEvents ) - 1; k > 0; --k )
		{
			if ( !V_strcmp( sz, arrEvents[ k ] ) )
			{
				V_snprintf( const_cast< char * >( sz ), MAX_PATH, "#CSGO_Tournament_Event_Stage_Display_%d", k );
				return sz;
			}
		}
	}
	return m_szTournamentEventStage;
}

//
// all of this is here to pick the first caster in the active caster list by default
//  later when we have a "pick caster" dialog then this can go away
//
extern ConVar spec_autodirector_cameraman;
extern ConVar spec_cameraman_ui;
extern ConVar spec_cameraman_xray;

void CCSGameRules::RecvProxy_TournamentActiveCasterAccounts( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	RecvProxy_Int32ToInt32( pData, pStruct, pOut );

	// set convars here
	if ( spec_autodirector_cameraman.GetInt() == -1 && CSGameRules() && CSGameRules()->m_arrTournamentActiveCasterAccounts[ 0 ] != 0 )
	{
		int casterID = ( int ) ( CSGameRules()->m_arrTournamentActiveCasterAccounts[ 0 ] );
		spec_autodirector_cameraman.SetValue( casterID );
		spec_cameraman_ui.SetValue( casterID );
		spec_cameraman_xray.SetValue( casterID );
		engine->SetVoiceCasterID( CSGameRules()->m_arrTournamentActiveCasterAccounts[ 0 ] );
	}
}

void CCSGameRules::ResetCasterConvars()
{
		spec_autodirector_cameraman.SetValue( -1 );
		spec_cameraman_ui.SetValue( 0 );
		spec_cameraman_xray.SetValue( 0 );
		engine->SetVoiceCasterID( 0 );
}

IMaterial *CCSGameRules::GetGameRuleScreenOverlayOverride( void )
{
	if ( CSurvivalGameRules *pSGR = IsPlayingSurvival() ? GetSurvivalRules() : NULL )
	{
		return pSGR->GetGameRuleScreenOverlayOverride();
	}

	return NULL;
}

#endif

CEconQuestDefinition* CCSGameRules::GetActiveAssassinationQuest( void ) const
{
	// Co-op quests are never assassination
	// TODO: Is there a way to early-out here for event quests as well?
	if ( IsPlayingCooperativeGametype() )
		return nullptr;

	if ( !m_iActiveAssassinationTargetMissionID )
		return nullptr;

	// Get quest
	CEconQuestDefinition* pQuest = GetItemSchema()->GetQuestDefinition( m_iActiveAssassinationTargetMissionID );
	if ( !pQuest )
		return nullptr;

	// Check if quest is an assassination quest.  Event quests will fail here.
	if ( !IsAssassinationQuest( pQuest ) )
		return nullptr;

	return pQuest;
}


/*static*/ void CCSGameRules::CreateFriendlyMapNameToken( const char* szShortName, char* szOutBuffer, int nBuffSize )
{
    // must be able to fit the token below
    Assert( nBuffSize >= MAX_MAP_NAME+10+1 );

    // Create the nice version of the name.
    // right now we blindly create the token because we can assume (on consoles) that we don't have any maps
    // that we don't know about.  For PC, we'll need to fix this so all checks will happen on the client
    V_snprintf( szOutBuffer, nBuffSize, "#SFUI_Map_%s", szShortName );
}

const char *CCSGameRules::GetDefaultTeamName( int nTeam )
{
	Assert( nTeam >= 0 && nTeam < ARRAYSIZE( sTeamNames ) );

	return sTeamNames[nTeam];
}

void CCSGameRules::AddGunGameWeapon( const CEconItemDefinition* pDef, int nNumKillsToUpgrade, int nTeamID )
{
    if ( !pDef )
        return;

    Assert( nNumKillsToUpgrade > 0 );
    if ( nNumKillsToUpgrade <= 0 )
    {
		Warning( "CCSGameRules: Invalid number of kills-to-upgrade (%d) for weapon %s.\n", nNumKillsToUpgrade, pDef->GetDefinitionName() );
        nNumKillsToUpgrade = 1;
    }

    if ( nTeamID == TEAM_CT )
    {
        m_GGProgressiveWeaponOrderCT.Set( m_iNumGunGameProgressiveWeaponsCT, pDef->GetDefinitionIndex() ); // $$$REI make sure this works during loadout setup?
        m_GGProgressiveWeaponKillUpgradeOrderCT.Set( m_iNumGunGameProgressiveWeaponsCT, nNumKillsToUpgrade );
        m_iNumGunGameProgressiveWeaponsCT++;
    }
    else if ( nTeamID == TEAM_TERRORIST )
    {
        m_GGProgressiveWeaponOrderT.Set( m_iNumGunGameProgressiveWeaponsT, pDef->GetDefinitionIndex() ); // $$$REI make sure this works during loadout setup?
        m_GGProgressiveWeaponKillUpgradeOrderT.Set( m_iNumGunGameProgressiveWeaponsT, nNumKillsToUpgrade );
        m_iNumGunGameProgressiveWeaponsT++;
    }

#if defined ( GAME_DLL )
    // PERF: Force creation of instance baselines so we don't take the hit upon creation.
    // We're doing this because we get a huge network spike upon player death which causes hitches on PS3.
    // Pre-creating baselines offloads a bit of that spike.
    UTIL_EnsureInstanceBaseline( pDef->GetItemClass() );	
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Set up the convars and data associated with the current game type and mode
//-----------------------------------------------------------------------------
void CCSGameRules::InitializeGameTypeAndMode( void )
{
#if !defined( CLIENT_DLL )
    bool isMultiplayer = !IsPlayingOffline();

	const char* szMapNameFull = STRING(gpGlobals->mapname);

	
	/////////////  load kv values from map sidecar file ( map.kv )
	////////

	char filename[ MAX_PATH ];
	V_StripExtension( V_UnqualifiedFileName( STRING( gpGlobals->mapname ) ), filename, MAX_PATH );

	uint32 dwRichPresenceContext = 0xFFFF;
	if ( !g_pGameTypes->GetMapInfo( filename, dwRichPresenceContext ) )
	{
		
		char kvFilename[ MAX_PATH ];
	
		bool bLoadSideCar = true;

		V_snprintf( kvFilename, sizeof( kvFilename ), "maps/%s.kv", filename );

		if ( !g_pFullFileSystem->FileExists( kvFilename ) ) 
			bLoadSideCar = false;

		// Load the Map sidecar entry

		if ( bLoadSideCar )
		{
			KeyValues *pkvMap = new KeyValues( "Map" );
			KeyValues::AutoDelete autodelete_kvMap( pkvMap );

			if ( pkvMap->LoadFromFile( g_pFullFileSystem, kvFilename ) )
			{
				g_pGameTypes->LoadMapEntry( pkvMap );
			}
			else
			{
				Warning( "Failed to load %s\n", kvFilename );
			}
		}
	}
	/////
	///////////////////

	g_pGameTypes->CheckShouldSetDefaultGameModeAndType( szMapNameFull );

    // Set the mode convars
    if ( g_pGameTypes->ApplyConvarsForCurrentMode( isMultiplayer ) )
    {
		// Add the gun game weapons.
		if ( IsPlayingGunGameProgressive() && mp_ggprogressive_use_random_weapons.GetBool() )
		{
			// this is where we build the list of GG progressive weapons
			// collect all of the weapons here
			CUtlVector< const CEconItemDefinition* > pSMGs;
			for ( int i = 0; i < V_ARRAYSIZE( gGungame_Smgs ); ++i )
				pSMGs.AddToTail( gGungame_Smgs[i] );
			CUtlVector< const CEconItemDefinition* > pShotguns;
			for ( int i = 0; i < V_ARRAYSIZE( gGungame_Shotguns ); ++i )
				pShotguns.AddToTail( gGungame_Shotguns[i] );
			CUtlVector< const CEconItemDefinition* > pRifles;
			for ( int i = 0; i < V_ARRAYSIZE( gGungame_Rifles ); ++i )
				pRifles.AddToTail( gGungame_Rifles[i] );
			CUtlVector< const CEconItemDefinition* > pSnipers;
			for ( int i = 0; i < V_ARRAYSIZE( gGungame_Snipers ); ++i )
				pSnipers.AddToTail( gGungame_Snipers[i] );
			CUtlVector< const CEconItemDefinition* > pMGs;
			for ( int i = 0; i < V_ARRAYSIZE( gGungame_Machineguns ); ++i )
				pMGs.AddToTail( gGungame_Machineguns[i] );
			CUtlVector< const CEconItemDefinition* > pPistols;
			for ( int i = 0; i < V_ARRAYSIZE( gGungame_Pistols ); ++i )
				pPistols.AddToTail( gGungame_Pistols[i] );

			int nNumSMGs = 3;
			int nNumRifles = 4;
			int nNumShotguns = 2;
			int nNumSnipers = 2;
			int nNumMGs = 1;
			int nNumPistols = 4;
			// this should total 16 weapons

			int nKillsNeeded = MAX( 1, mp_ggprogressive_random_weapon_kills_needed.GetInt() );

			// now pick a random one from the list we created above for each category
			CUtlVector< const CEconItemDefinition* > pWeaponProgression;
			for ( int i=0; i<nNumSMGs; i++ )
			{
				int nPick = RandomInt( 0, pSMGs.Count()-1 );
				pWeaponProgression.AddToTail(pSMGs[nPick]);
				pSMGs.FastRemove( nPick );
			}
			for ( int i=0; i<nNumRifles; i++ )
			{
				int nPick = RandomInt( 0, pRifles.Count()-1 );
				pWeaponProgression.AddToTail(pRifles[nPick]);
				pRifles.FastRemove( nPick );
			}
			for ( int i=0; i<nNumShotguns; i++ )
			{
				int nPick = RandomInt( 0, pShotguns.Count()-1 );
				pWeaponProgression.AddToTail(pShotguns[nPick]);
				pShotguns.FastRemove( nPick );
			}
			for ( int i=0; i<nNumSnipers; i++ )
			{
				int nPick = RandomInt( 0, pSnipers.Count()-1 );
				pWeaponProgression.AddToTail(pSnipers[nPick]);
				pSnipers.FastRemove( nPick );
			}
			for ( int i=0; i<nNumMGs; i++ )
			{
				int nPick = RandomInt( 0, pMGs.Count()-1 );
				pWeaponProgression.AddToTail(pMGs[nPick]);
				pMGs.FastRemove( nPick );
			}
			for ( int i=0; i<nNumPistols; i++ )
			{
				int nPick = RandomInt( 0, pPistols.Count()-1 );
				pWeaponProgression.AddToTail(pPistols[nPick]);
				pPistols.FastRemove( nPick );
			}

			// go through the list we build and add them to the final list that will get used in the game
			FOR_EACH_VEC( pWeaponProgression, iWeaponProgression )
			{
				AddGunGameWeapon( pWeaponProgression[iWeaponProgression], nKillsNeeded, TEAM_CT );
				AddGunGameWeapon( pWeaponProgression[iWeaponProgression], nKillsNeeded, TEAM_TERRORIST );
			}

			// add the knife manually
			static const CSchemaItemDefHandle knifegg( "weapon_knifegg" );
			AddGunGameWeapon( knifegg, 1, TEAM_CT );
			AddGunGameWeapon( knifegg, 1, TEAM_TERRORIST );
		}
		// Add the gun game weapons.
        else if ( IsPlayingGunGameTRBomb() || IsPlayingGunGameProgressive() )
        {
            const CUtlVector< IGameTypes::WeaponProgression > *pWeaponProgressionCT = g_pGameTypes->GetWeaponProgressionForCurrentModeCT();
            if ( pWeaponProgressionCT )
            {
                FOR_EACH_VEC( *pWeaponProgressionCT, iWeaponProgression )
                {
                    const IGameTypes::WeaponProgression &wp = (*pWeaponProgressionCT)[iWeaponProgression];
					const CEconItemDefinition* pWeapon = GetItemSchema()->GetItemDefinitionByName( CFmtStr( "weapon_%s", wp.m_Name.Get() ) );
					AddGunGameWeapon( pWeapon, wp.m_Kills, TEAM_CT );
                }
            }

            const CUtlVector< IGameTypes::WeaponProgression > *pWeaponProgressionT = g_pGameTypes->GetWeaponProgressionForCurrentModeT();
            if ( pWeaponProgressionT )
            {
                FOR_EACH_VEC( * pWeaponProgressionT, iWeaponProgression )
                {
                    const IGameTypes::WeaponProgression &wp = (* pWeaponProgressionT )[iWeaponProgression];
					const CEconItemDefinition* pWeapon = GetItemSchema()->GetItemDefinitionByName( CFmtStr( "weapon_%s", wp.m_Name.Get() ) );
					AddGunGameWeapon( pWeapon, wp.m_Kills, TEAM_TERRORIST );
                }
            }
        }

        if ( CSGameRules()->HasHalfTime() )
        {
            m_match.SetPhase( GAMEPHASE_PLAYING_FIRST_HALF );
        }
        else
        {
            m_match.SetPhase( GAMEPHASE_PLAYING_STANDARD );
        }

        // Set the map convars
        ConVarRef host_map( "host_map" );
        g_pGameTypes->ApplyConvarsForMap( host_map.GetString(), engine->IsDedicatedServer() || isMultiplayer );

		EMsgGCCStrike15_v2_MatchmakingGame_t eGameType = MatchmakingGameTypeToGame( CCSGameRules::sm_QueuedServerReservation.game_type() );
		EMsgGCCStrike15_v2_MatchmakingMapGroup_t eMapGroup = MatchmakingGameTypeToMapGroup( CCSGameRules::sm_QueuedServerReservation.game_type() );
		uint32 unQuestID = 0;

		if ( eGameType == k_EMsgGCCStrike15_v2_MatchmakingGame_Cooperative )
		{
			// Exec event-specific convars (if any)
			// These modes smuggle the quest ID into the map group
			unQuestID = ( uint32 )eMapGroup;
		}

		if(unQuestID != 0)
		{
			const CEconQuestDefinition *pQuest = GetItemSchema()->GetQuestDefinition( unQuestID );
			if ( pQuest )
			{
				DevMsg( "GameTypes: Event setup for '%s' (%d)\n", pQuest->GetName(), unQuestID );
				m_iActiveAssassinationTargetMissionID = unQuestID;

				if ( !StringIsEmpty( pQuest->GetQuestConVars() ) )
				{
					engine->ServerCommand( CFmtStr( "%s\n", pQuest->GetQuestConVars() ) );
					engine->ServerExecute();
				}
			}
		}

		if ( sv_skirmish_id.GetInt() != 0 )
		{
			const CSkirmishModeDefinition* pSkirmishDefinition = GetItemSchema()->GetSkirmishModeDefinition( sv_skirmish_id.GetInt() );
			if ( pSkirmishDefinition )
			{
				if ( !StringIsEmpty( pSkirmishDefinition->GetServerExec() ) )
				{
					engine->ServerCommand( CFmtStr( "%s\n", pSkirmishDefinition->GetServerExec() ) );
					engine->ServerExecute();
				}
			}
		}

    }
#endif

#if ( CSTRIKE_DEMO_PRESSBUILD || CSTRIKE_E3_BUILD )

    ConVarRef bot_quota( "bot_quota" );
    bot_quota.SetValue( 8 );

    ConVarRef bot_difficulty( "bot_difficulty" );
    bot_difficulty.SetValue( 0 );

#endif

    // this allows the weapon recoil tables to incorporate changes made to convars before the game launches
	g_pWeaponSystem->RefreshForGame();
}

int CCSGameRules::GetNumProgressiveGunGameWeapons( int nTeamID ) const
{
    if ( nTeamID == TEAM_CT )
    {
        return m_iNumGunGameProgressiveWeaponsCT;
    }
    else
    {
        return m_iNumGunGameProgressiveWeaponsT;
    }
}

const CEconItemDefinition* CCSGameRules::GetGunGameTRBonusGrenade( CCSPlayer *pPlayer )
{
	if ( !pPlayer )
		return nullptr;

    int nNumTRKillPoints = pPlayer->GetNumGunGameTRKillPoints();

    if ( nNumTRKillPoints >= mp_ggtr_bomb_pts_for_molotov.GetInt() )
    {
		if ( pPlayer->GetTeamNumber() == TEAM_TERRORIST )
			return WEAPON_MOLOTOV;
        else
            return WEAPON_INCGRENADE;
    }
    else if ( nNumTRKillPoints >= mp_ggtr_bomb_pts_for_flash.GetInt() )
    {
        return WEAPON_FLASHBANG;
    }
    else if ( nNumTRKillPoints >= mp_ggtr_bomb_pts_for_he.GetInt() )
    {
        return WEAPON_HEGRENADE;
    }

    return nullptr;
}

bool CCSGameRules::IsIntermission( void ) const
{
#ifndef CLIENT_DLL
    return m_flIntermissionStartTime + GetIntermissionDuration() > gpGlobals->curtime;
#endif

    return false;
}

int CCSGameRules::GetMaxSpectatorSlots( void ) const
{
    return m_iSpectatorSlotCount;
}

int CCSGameRules::GetMaxPlayers()
{
	if ( sv_competitive_official_5v5.GetInt() )
		return 10;

#ifdef CLIENT_DLL
	if ( engine->IsPlayingDemo() || !engine->IsConnected() )
	{
		return 0;
	}
#endif

	return MIN( g_pGameTypes->GetCurrentServerNumSlots(), 24 );
}

void CCSGameRules::CoopResetRoundStartTime( void )
{
	m_fRoundStartTime.GetForModify() = gpGlobals->curtime;

#ifdef GAME_DLL
	m_coopPlayersInDeploymentZone = false;

	if ( IsPlayingCoopMission() )
	{
		// Reset certain player stats when this happens
		FOR_EACH_MAP( m_mapQueuedMatchmakingPlayersData, idxQueuedPlayer )
		{
			CQMMPlayerData_t &qmm = *m_mapQueuedMatchmakingPlayersData.Element( idxQueuedPlayer );
			qmm.m_numHealthPointsRemovedTotal = 0;
			qmm.m_numHealthPointsDealtTotal = 0;
			qmm.m_numShotsFiredTotal = 0;
			qmm.m_numShotsOnTargetTotal = 0;
		}

		// now the actual round is starting, adjust the duration here
		m_iRoundTime.GetForModify() = int( mp_roundtime.GetFloat() * 60 );
	}

	// we also want to refresh the difficulty of the bots here
	CTeam *pTeam = GetGlobalTeam( TEAM_TERRORIST );
	CUtlVector < CCSBot* > vecBotsOnTeam;
	if ( pTeam->GetBotMembers( &vecBotsOnTeam ) )
	{
		FOR_EACH_VEC( vecBotsOnTeam, i )
		{
			vecBotsOnTeam[i]->CoopInitialize();
		}
	}
#endif
}

void CCSGameRules::CoopGiveC4sToCTs( int nC4sToGive )
{
#ifdef GAME_DLL
	CTeam *pTeam = GetGlobalTeam( TEAM_CT );
	CUtlVector < CCSPlayer* > vecPlayers;
	int nC4s = nC4sToGive;
	if ( pTeam->GetHumanMembers( &vecPlayers ) )
	{
		FOR_EACH_VEC( vecPlayers, i )
		{
			if ( vecPlayers[i]->Weapon_OwnsThisType( WEAPON_C4_CLASSNAME ) )
				nC4s--;
		}

		// don't give more C4s than players
		if ( nC4s == 0 )
			return;

		FOR_EACH_VEC( vecPlayers, i )
		{
			if ( vecPlayers[i]->IsAlive() && !vecPlayers[i]->Weapon_OwnsThisType( WEAPON_C4_CLASSNAME ) )
				vecPlayers[i]->GiveNamedItem( WEAPON_C4_CLASSNAME );
		}
	}
#endif
}

int CCSGameRules::GetNumWinsToClinch() const
{
	int iNumWinsToClinch = (mp_maxrounds.GetInt() > 0 && MatchCanClinch() ) ? ( mp_maxrounds.GetInt() / 2 ) + 1 + GetOvertimePlaying() * ( mp_overtime_maxrounds.GetInt() / 2 ) : -1;
	return iNumWinsToClinch;
}

bool CCSGameRules::IsLastRoundOfMatch() const
{
	bool bLastRound = mp_maxrounds.GetInt() > 0 ? ( GetTotalRoundsPlayed() == ( mp_maxrounds.GetInt()-1 + GetOvertimePlaying()*mp_overtime_maxrounds.GetInt() ) ) : false;
	return bLastRound;
}

bool CCSGameRules::IsMatchPoint() const
{
	int iNumWinsToClinch = GetNumWinsToClinch();
	bool bMatchPoint = false;
#ifdef CLIENT_DLL
	if ( GetGamePhase() != GAMEPHASE_PLAYING_FIRST_HALF )
	{
		C_Team *pTerrorists = GetGlobalTeam( TEAM_TERRORIST );
		C_Team *pCTs = GetGlobalTeam( TEAM_CT );
		bMatchPoint = ( pCTs && ( pCTs->Get_Score() == iNumWinsToClinch-1 ) ) || ( pTerrorists && ( pTerrorists->Get_Score() == iNumWinsToClinch-1 ) );
	}
#else
	if ( m_match.GetPhase() != GAMEPHASE_PLAYING_FIRST_HALF )
	{
		bMatchPoint = ( m_match.GetCTScore() == iNumWinsToClinch-1 || m_match.GetTerroristScore() == iNumWinsToClinch-1);
	}
#endif
	return bMatchPoint;
}

// AreTeamsPlayingSwitchedSides() -- will return true when match is in second half, or in the half of overtime period where teams are switched.
// Overtime logic is as follows: TeamA plays CTs as first half of regulation, then Ts as second half of regulation,
//				then if tied in regulation continues to play Ts as first half of 1st overtime, then switches to CTs for second half of 1st overtime,
//				then if still tied after 1st OT they continue to play CTs as first half of 2nd overtime, then switch to Ts for second half of 2nd overtime,
//				then if still tied after 2nd OT they continue to play Ts as first half of 3rd overtime, then switch to CTs for second half of 3rd overtime,
//				and so on until the match determines a winner.
// So AreTeamsPlayingSwitchedSides will return true when TeamA is playing T-side and will return false when TeamA plays CT-side as they started match on CT
// in scenario outlined above.
bool CCSGameRules::AreTeamsPlayingSwitchedSides() const
{
	if ( !GetOvertimePlaying() )
	{
		switch ( GetGamePhase() )
		{
		case GAMEPHASE_PLAYING_SECOND_HALF:
			return true;
		case GAMEPHASE_MATCH_ENDED:
			return HasHalfTime() && ( GetTotalRoundsPlayed() > ( mp_maxrounds.GetInt() / 2 ) );
		default:
			return false;
		}
	}
	else
	{
		switch ( GetGamePhase() )
		{
		case GAMEPHASE_PLAYING_SECOND_HALF:
			// Playing 2nd half of 2nd half of every even OT, e.g. second OT, will result in switched teams
			return ( GetOvertimePlaying() % 2 ) ? false : true;
		case GAMEPHASE_MATCH_ENDED:
			{
				bool bEndedInSecondHalfOfOvertime = HasHalfTime() &&
					( GetTotalRoundsPlayed() > mp_maxrounds.GetInt() + ( 2*GetOvertimePlaying() - 1 ) * ( mp_overtime_maxrounds.GetInt() / 2 ) );
				if ( GetOvertimePlaying() % 2 )
					bEndedInSecondHalfOfOvertime = !bEndedInSecondHalfOfOvertime;
				return bEndedInSecondHalfOfOvertime;
			}
		case GAMEPHASE_HALFTIME:
			{
				// halftime can also be at the end of regulation or at the end of both OT halves, in this case the overtime number has
				// already been incremented into the next overtime
				bool bSecondHalfOfOvertime = HasHalfTime() &&
					( GetTotalRoundsPlayed() <= ( mp_maxrounds.GetInt() + ( GetOvertimePlaying() - 1 )*mp_overtime_maxrounds.GetInt() ) );
				int nOvertimeInWhichHalftimeIsActuallyReached = GetOvertimePlaying();
				if ( bSecondHalfOfOvertime )
					-- nOvertimeInWhichHalftimeIsActuallyReached;	// this is the case when we already advanced the OT index and wait in intermission
				if ( nOvertimeInWhichHalftimeIsActuallyReached % 2 )
					bSecondHalfOfOvertime = !bSecondHalfOfOvertime;
				return bSecondHalfOfOvertime;
			}
			break;
		default:
			// Playing 1st half, opposite of GAMEPHASE_PLAYING_SECOND_HALF state coded above
			return ( GetOvertimePlaying() % 2 ) ? true : false;
		}
	}
}

int CCSGameRules::GetOvertimeInRound( int nRound ) const
{
	return Ceil2Int( ( float )( nRound - mp_maxrounds.GetInt() ) / ( float )mp_overtime_maxrounds.GetInt() );
}


bool CCSGameRules::AreTeamsPlayingSwitchedSidesInRound( int nRound ) const
{
	if ( !HasHalfTime() )
		return false;

	if (  nRound > ( mp_maxrounds.GetInt() / 2 ) && nRound <= mp_maxrounds.GetInt() )
	{
		return true;
	}
	else if ( nRound > mp_maxrounds.GetInt() )
	{
		int nOverTime = GetOvertimeInRound( nRound );

		bool bRetVal = nRound <= ( mp_maxrounds.GetInt() + ( 2 * nOverTime - 1 ) * ( mp_overtime_maxrounds.GetInt() / 2 ) );
		
		// Teams continue on whatever team they were on so we have to account for alternating overtimes.
		// See AreTeamsPlayingSwitchedSides for OT sequence.
		if ( nOverTime % 2 == 0 )
			bRetVal = !bRetVal;

		return bRetVal;
	}

	return false;
}

// music selection
#ifdef CLIENT_DLL

const char *musicTypeStrings[] =
{
	"NONE",
	"Music.StartRound_GG",
	"Music.StartRound",
	"Music.StartAction",
	"Music.DeathCam",
	"Music.BombPlanted",
	"Music.BombTenSecCount",
	"Music.TenSecCount",
	"Music.WonRound",
	"Music.LostRound",
	"Music.GotHostage",
	"Music.MVPAnthem",
	"Music.Selection",
	"Musix.HalfTime",
};

void PlayMusicSelection( IRecipientFilter& filter, CsMusicType_t nMusicType , int nPlayerEntIndex /* = 0*/ , float flPreElapsedTime  /*= 0.0*/  )
{
	//////////////////////////////////////////////////////////////////////////////////////////
	// test for between rounds and block incoming events until in round
	//
	static bool bBetweenRound = false;
	if( nMusicType == CSMUSIC_LOSTROUND || nMusicType == CSMUSIC_WONROUND || nMusicType == CSMUSIC_MVP || nMusicType == CSMUSIC_HALFTIME )
	{
		bBetweenRound = true;
	}
	else if( nMusicType == CSMUSIC_START || nMusicType == CSMUSIC_ACTION || nMusicType == CSMUSIC_STARTGG )
	{
		bBetweenRound = false;
	}
	if( bBetweenRound && ( nMusicType == CSMUSIC_BOMB || nMusicType == CSMUSIC_BOMBTEN || nMusicType == CSMUSIC_ROUNDTEN ))
	{
		return;
	}

	const char *pEntry = musicTypeStrings[ nMusicType ];

	////////////////////////////////////////////////////////////////////////////////////////////
	// this is to be used in the case that no music pack is equipped, mimicing original csgo behavior
	// music selection switches on team select screen and halftime
	static int nUpdatedMusic = snd_music_selection.GetInt();
	// hack cause halftime music is getting called twice for some reason
	if( nMusicType == CSMUSIC_SELECTION || nMusicType == CSMUSIC_HALFTIME )
	{
		nUpdatedMusic = (nUpdatedMusic == 2 ) ? 1 : 2;		
	}
	if(  nMusicType == CSMUSIC_SELECTION || nMusicType == CSMUSIC_START )
	{
		snd_music_selection.SetValue( nUpdatedMusic );
	}
	char pMusicExtensionBuf[64];
	V_sprintf_safe( pMusicExtensionBuf, "%s_%02i", "valve_csgo", snd_music_selection.GetInt() );
	const char *pMusicExtension = pMusicExtensionBuf;

	/////////////////////////////////////////////////////////////////////////
	// no halftime music in overwatch
	if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
	{
		if(pParameters->m_bAnonymousPlayerIdentity && nMusicType == CSMUSIC_HALFTIME )
			return; // or whatever is appropriate to not play the halftime music
	}

	/////////////////////////////////////////////////////////////////////////
	// is there delay from switching spectated player?
	// ********* not currently used ******
// 	engine->SOSSetOpvarFloat( "spectateMusicDelay", flPreElapsedTime );
// 	if( nMusicType == CSMUSIC_START )
// 	{
// 		engine->SOSSetOpvarFloat("csgo_roundstart_time", gpGlobals->curtime );
// 	}

	///////////////////////////////////////////////////////////////////////////////
	// music source priority:
	//	passed in arg (if has music pack)> borrowed music > own music

	int nPlayerIndex = 0;

	// use a specified playerindex if passed in ( MVP )
	if( ( nPlayerEntIndex != 0 ) && g_PR )
	{
		if (  g_PR->IsConnected( nPlayerEntIndex ) )
		{
			if ( C_CS_PlayerResource *cs_PR = dynamic_cast< C_CS_PlayerResource * >( g_PR ) )
			{
				uint32 unMusicID = cs_PR->GetMusicID( nPlayerEntIndex );
				if( unMusicID > 1 )
					nPlayerIndex = nPlayerEntIndex;
			}
		}
	}

	// if the passed in player didn't give us a music id, check for a borrowed kit.
	if ( !nPlayerIndex )
	{
		int nBorrowPlIndex = cl_borrow_music_from_player_index.GetInt();

		if ( nBorrowPlIndex )
		{	
			if ( g_PR->IsConnected( nBorrowPlIndex ) && !g_PR->IsFakePlayer( nBorrowPlIndex ) )
			{
				nPlayerIndex = nBorrowPlIndex;
			}
			else
			{
				cl_borrow_music_from_player_index.SetValue( cl_borrow_music_from_player_index.GetDefault() );
			}
		}
	}

	bool bLocalPlayer = false;

	// otherwise use our own
	if ( !nPlayerIndex ) {
		nPlayerIndex = GetLocalPlayerIndex();
		bLocalPlayer = true;
	}
			
	{
		uint32 unMusicID = 0;

		C_CS_PlayerResource *cs_PR = dynamic_cast< C_CS_PlayerResource * >( g_PR );
		if ( cs_PR != nullptr )
		{
			unMusicID = cs_PR->GetMusicID( nPlayerIndex );
		}
		if( unMusicID > 1 )
		{
			const CEconMusicDefinition *pMusicDef = GetItemSchema()->GetMusicDefinition(unMusicID);
			if(pMusicDef)
				pMusicExtension = pMusicDef->GetName();
		}
		else if( bLocalPlayer && steamapicontext->SteamUser() )
		{
			CSteamID steamIDForPlayer = steamapicontext->SteamUser()->GetSteamID();

			const CEconItemView *pItemData = CSInventoryManager()->GetItemInLoadoutForTeam(0, LOADOUT_POSITION_MUSICKIT, &steamIDForPlayer);
			if(pItemData && pItemData->IsValid())
			{
				const CEconItemAttributeDefinition *pAttr_MusicID = GetItemSchema()->GetAttributeDefinitionByName("music id");

				if(pItemData->FindAttribute(pAttr_MusicID, &unMusicID))
				{
					const CEconMusicDefinition *pMusicDef = GetItemSchema()->GetMusicDefinition(unMusicID);
					if(pMusicDef)
						pMusicExtension = pMusicDef->GetName();
				}
			}
		}

	}
	
	if( pEntry )
	{
		char musicSelection[128];
		int nExtLen = V_strlen( pMusicExtension );
		int nStrLen = V_strlen( pEntry );
		V_snprintf( musicSelection, nExtLen + nStrLen+2, "%s.%s", pEntry, pMusicExtension );
		C_BaseEntity::EmitSound( filter, -1, musicSelection );
	}
}
void PlayCustomDeathCamSelection( IRecipientFilter& filter, int nEntIndex, const char *pSoundEntry, int nDeathCamIndex )
{
    char musicSelection[128];
    V_snprintf( musicSelection, 128, "%s_%03i", pSoundEntry, nDeathCamIndex );
    C_BaseEntity::EmitSound( filter, nEntIndex, musicSelection );

}



float Helper_GetBuyTimeRemaining()
{
	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();

	float flEndTime;

	extern ConVar mp_buy_during_immunity;

	if ( CSGameRules()->IsPlayingCoopMission() )
	{
		flEndTime = gpGlobals->curtime + 9999.0f;
	}
	else if ( CSGameRules()->IsWarmupPeriod() == false && CSGameRules()->IsPlayingCoopGuardian() )
	{
		if ( CSGameRules()->IsFreezePeriod() )
			flEndTime = CSGameRules()->GetRoundStartTime();
		else
			flEndTime = CSGameRules()->m_flGuardianBuyUntilTime;

		if ( flEndTime - gpGlobals->curtime > 9999.0f )
			flEndTime = gpGlobals->curtime - 1.0f;
	}
	else if ( pLocalPlayer && pLocalPlayer->CanBuyDuringImmunity() )
	{
		flEndTime = pLocalPlayer->m_fImmuneToGunGameDamageTime;
	}
	else
	{
		if ( CSGameRules()->IsWarmupPeriod() )
		{
			flEndTime = CSGameRules()->GetWarmupPeriodEndTime();
		}
		else
		{
			flEndTime = CSGameRules()->GetRoundStartTime() + CSGameRules()->GetBuyTimeLength();
		}
	}

	return flEndTime - gpGlobals->curtime;
}

#endif

float CCSGameRules::CheckTotalSmokedLength( float flSmokeRadiusSq, Vector vecGrenadePos, Vector from, Vector to )
{
	Vector sightDir = to - from;
	float sightLength = sightDir.NormalizeInPlace();

	// the detonation position is the actual position of the smoke grenade, but the smoke volume center is actually some number of units above that
	Vector vecSmokeCenterOffset = Vector( 0, 0, 60 );
	const Vector &smokeOrigin = vecGrenadePos + vecSmokeCenterOffset;

	float flSmokeRadius = sqrt(flSmokeRadiusSq);
	// if the start point or the end point is inside the radius of the smoke, then the line goes through the smoke
	if ( (smokeOrigin - from).IsLengthLessThan( flSmokeRadius*0.95f ) || (smokeOrigin - to).IsLengthLessThan( flSmokeRadius ) )
		return -1;

	Vector toGrenade = smokeOrigin - from;

	float alongDist = DotProduct( toGrenade, sightDir );

	// compute closest point to grenade along line of sight ray
	Vector close;

	// constrain closest point to line segment
	if (alongDist < 0.0f)
		close = from;
	else if (alongDist >= sightLength)
		close = to;
	else
		close = from + sightDir * alongDist;

	// if closest point is within smoke radius, the line overlaps the smoke cloud
	Vector toClose = close - smokeOrigin;
	float lengthSq = toClose.LengthSqr();

	//float smokeRadius = (float)sqrt( flSmokeRadiusSq );
	//NDebugOverlay::Sphere( smokeOrigin, smokeRadius, 0, 255, 0, true, 2.0f);
	if (lengthSq < flSmokeRadiusSq)
	{
		// some portion of the ray intersects the cloud
			
		// 'from' and 'to' lie outside of the cloud - the line of sight completely crosses it
		// determine the length of the chord that crosses the cloud
		float smokedLength = 2.0f * (float)sqrt( flSmokeRadiusSq - lengthSq );
		return smokedLength;
	}
	
	return 0;
}

#if defined( CLIENT_DLL )

//-----------------------------------------------------------------------------
// Enforce certain values on the specified convar.
//-----------------------------------------------------------------------------
void EnforceCompetitiveCVar( const char *szCvarName, float fMinValue, float fMaxValue = FLT_MAX, int iArgs = 0, ... )
{
    // Doing this check first because OK values might be outside the min/max range
    ConVarRef competitiveConvar(szCvarName);
    float fValue = competitiveConvar.GetFloat();
    va_list vl;
    va_start(vl, iArgs);
    for( int i=0; i< iArgs; ++i )
    {
        if( (int)fValue == va_arg(vl,int) )
            return;
    }
    va_end(vl);

    if ( fValue < fMinValue || fValue > fMaxValue )
    {
        float fNewValue = MAX( MIN( fValue, fMaxValue ), fMinValue );
        competitiveConvar.SetValue( fNewValue );
        DevMsg( "Convar %s was out of range and forced to %.2f. Valid values are between %.2f and %.2f. To remove the restriction set sv_competitive_minspec 0 on the server.\n", szCvarName, fNewValue, fMinValue, fMaxValue );
    }
	else
	{
		competitiveConvar.SetValue( fValue );
	}
}

//-----------------------------------------------------------------------------
// An interface used by ENABLE_COMPETITIVE_CONVAR macro that lets the classes
// defined in the macro to be stored and acted on.
//-----------------------------------------------------------------------------
class ICompetitiveConvar
{
public:
    virtual void BackupConvar() = 0;
    virtual void EnforceRestrictions() = 0;
    virtual void RestoreOriginalValue() = 0;
    virtual void InstallChangeCallback() = 0;
};

//-----------------------------------------------------------------------------
// A manager for all enforced competitive convars.
//-----------------------------------------------------------------------------
class CCompetitiveCvarManager : public CAutoGameSystem
{
public:
    typedef CUtlVector<ICompetitiveConvar*> CompetitiveConvarList_t;
    static void AddConvarToList( ICompetitiveConvar* pCVar )
    {
        GetConvarList()->AddToTail( pCVar );
    }

    static void BackupAllConvars()
    {
        FOR_EACH_VEC( *GetConvarList(), i )
        {
            (*GetConvarList())[i]->BackupConvar();
        }
    }

    static void EnforceRestrictionsOnAllConvars()
    {
        FOR_EACH_VEC( *GetConvarList(), i )
        {
            (*GetConvarList())[i]->EnforceRestrictions();
        }
    }

    static void RestoreAllOriginalValues()
    {
        FOR_EACH_VEC( *GetConvarList(), i )
        {
            (*GetConvarList())[i]->RestoreOriginalValue();
        }
    }

    static CompetitiveConvarList_t* GetConvarList()
    {
        if( !s_pCompetitiveConvars )
        {
            s_pCompetitiveConvars = new CompetitiveConvarList_t();
        }
        return s_pCompetitiveConvars;
    }

    static KeyValues* GetConVarBackupKV()
    {
        if( !s_pConVarBackups )
        {
            s_pConVarBackups = new KeyValues("ConVarBackups");
        }
        return s_pConVarBackups;
    }

    virtual bool Init() 
    { 
        FOR_EACH_VEC( *GetConvarList(), i )
        {
            (*GetConvarList())[i]->InstallChangeCallback();
        }
        return true;
    }

    virtual void Shutdown()
    {
        FOR_EACH_VEC( *GetConvarList(), i )
        {
            delete (*GetConvarList())[i];
        }
        delete s_pCompetitiveConvars; 
        s_pCompetitiveConvars = 0;
        s_pConVarBackups->deleteThis(); 
        s_pConVarBackups = 0;
    }

    CCompetitiveCvarManager()
    {
        s_pCompetitiveConvars = 0;
        s_pConVarBackups = 0;
    }
private:
    static CompetitiveConvarList_t* s_pCompetitiveConvars;
    static KeyValues* s_pConVarBackups;
};
static CCompetitiveCvarManager *s_pCompetitiveCvarManager = new CCompetitiveCvarManager();
CCompetitiveCvarManager::CompetitiveConvarList_t* CCompetitiveCvarManager::s_pCompetitiveConvars = 0;
KeyValues* CCompetitiveCvarManager::s_pConVarBackups = 0;

//-----------------------------------------------------------------------------
// Macro to define restrictions on convars with "sv_competitive_minspec 1"
// Usage: ENABLE_COMPETITIVE_CONVAR( convarName, minValue, maxValue, optionalValues, opVal1, opVal2, ...
//-----------------------------------------------------------------------------
#define ENABLE_COMPETITIVE_CONVAR( convarName, ... ) \
class CCompetitiveMinspecConvar##convarName : public ICompetitiveConvar { \
public: \
    CCompetitiveMinspecConvar##convarName(){ CCompetitiveCvarManager::AddConvarToList(this);} \
    static void on_changed_##convarName( IConVar *var, const char *pOldValue, float flOldValue ){ \
        if( sv_competitive_minspec.GetBool() ) { \
            EnforceCompetitiveCVar( #convarName , __VA_ARGS__  ); }\
        else {\
            CCompetitiveCvarManager::GetConVarBackupKV()->SetFloat( #convarName, ConVarRef( #convarName ).GetFloat() ); } } \
    virtual void BackupConvar() { CCompetitiveCvarManager::GetConVarBackupKV()->SetFloat( #convarName, ConVarRef( #convarName ).GetFloat() ); } \
    virtual void EnforceRestrictions() { EnforceCompetitiveCVar( #convarName , __VA_ARGS__  ); } \
    virtual void RestoreOriginalValue() { ConVarRef(#convarName).SetValue(CCompetitiveCvarManager::GetConVarBackupKV()->GetFloat( #convarName ) ); } \
    virtual void InstallChangeCallback() { static_cast<ConVar*>(ConVarRef( #convarName ).GetLinkedConVar())->InstallChangeCallback( CCompetitiveMinspecConvar##convarName::on_changed_##convarName); } \
}; \
static CCompetitiveMinspecConvar##convarName *s_pCompetitiveConvar##convarName = new CCompetitiveMinspecConvar##convarName();

//-----------------------------------------------------------------------------
// Callback function for sv_competitive_minspec convar value change.
//-----------------------------------------------------------------------------
void sv_competitive_minspec_changed_f( IConVar *var, const char *pOldValue, float flOldValue )
{
    ConVar *pCvar = static_cast<ConVar*>(var);

    if( pCvar->GetBool() == true && flOldValue == 0.0f )
    {
        // Backup the values of each cvar and enforce new ones
        CCompetitiveCvarManager::BackupAllConvars();
        CCompetitiveCvarManager::EnforceRestrictionsOnAllConvars();
    }
    else if( pCvar->GetBool() == false && flOldValue != 0.0f )
    {
        // If sv_competitive_minspec is disabled, restore old client values
        CCompetitiveCvarManager::RestoreAllOriginalValues();
    }
}

#endif

static ConVar sv_competitive_minspec( "sv_competitive_minspec",
                                     "1",
                                     FCVAR_REPLICATED | FCVAR_NOTIFY | FCVAR_RELEASE,
                                     "Enable to force certain client convars to minimum/maximum values to help prevent competitive advantages."
#ifdef CLIENT_DLL
                                     ,sv_competitive_minspec_changed_f
#endif
                                     );

#ifdef CLIENT_DLL

#if defined( _GAMECONSOLE )

// ENABLE_COMPETITIVE_CONVAR( convar, range minimum, range maximum, number of additional distinct valid values, distinct valid values... );

ENABLE_COMPETITIVE_CONVAR( fps_max, 29, FLT_MAX, 1, 0 );	// force fps_max above 59. One additional value (0) works
#else
ENABLE_COMPETITIVE_CONVAR( fps_max, 59, FLT_MAX, 1, 0 );	// force fps_max above 59. One additional value (0) works
#endif
ENABLE_COMPETITIVE_CONVAR( cl_interp_ratio, 1, 2 );			// force cl_interp_ratio from 1 to 2
ENABLE_COMPETITIVE_CONVAR( cl_interp, 0, 0.031 );			// force cl_interp from 0.0152 to 0.031
ENABLE_COMPETITIVE_CONVAR( cl_updaterate, 10, 150 );		// force cl_updaterate from 10 to 150
ENABLE_COMPETITIVE_CONVAR( cl_cmdrate, 10, 150 );			// force cl_cmdrate from 10 to 150
ENABLE_COMPETITIVE_CONVAR( rate, 20480, 786432 );			// force rate above min rate and below max rate
ENABLE_COMPETITIVE_CONVAR( viewmodel_fov, 54, 68 );			// force viewmodel fov to be between 54 and 68
ENABLE_COMPETITIVE_CONVAR( viewmodel_offset_x, -2, 2.5 );		// restrict viewmodel positioning
ENABLE_COMPETITIVE_CONVAR( viewmodel_offset_y, -2, 2 );
ENABLE_COMPETITIVE_CONVAR( viewmodel_offset_z, -2, 2 );
ENABLE_COMPETITIVE_CONVAR( cl_bobcycle, 0.98, 0.98 );		// tournament standard

// replaced with sv_max_allowed_net_graph convar
//ENABLE_COMPETITIVE_CONVAR( net_graph, 0, 1 )				// tournament standard

#endif


#ifdef GAME_DLL
    CCSPlayer* FindPlayerFromAccountID( uint32 account_id )
    {
        for ( int i = 1; i <= gpGlobals->maxClients; i++ )
        {
            CBasePlayer *pBasePlayer = UTIL_PlayerByIndex( i );
            if ( !pBasePlayer )
                continue;

            CCSPlayer *pPlayer = dynamic_cast< CCSPlayer* >( pBasePlayer );
            if ( !pPlayer || pPlayer->IsBot() || !pPlayer->IsConnected() )
                continue;

            CSteamID steamID;
            pPlayer->GetSteamID( &steamID );

            if ( steamID.GetAccountID() == account_id )
                return pPlayer;
        }
        return NULL;
    }
#endif


#ifdef GAME_DLL

#ifndef NO_STEAM_GAMECOORDINATOR
	class ClientJob_EMsgGCCStrike15_v2_ServerNotificationForUserPenalty : public GCSDK::CGCClientJob
	{
	public:
		ClientJob_EMsgGCCStrike15_v2_ServerNotificationForUserPenalty( GCSDK::CGCClient *pGCClient ) : GCSDK::CGCClientJob( pGCClient )
		{
		}

		virtual bool BYieldingRunJobFromMsg( GCSDK::IMsgNetPacket *pNetPacket )
		{
			GCSDK::CProtoBufMsg<CMsgGCCStrike15_v2_ServerNotificationForUserPenalty> msg( pNetPacket );
			DevMsg( "Notification about user penalty: %u/%u (%u sec)\n", msg.Body().account_id(), msg.Body().reason(), msg.Body().seconds() );
			if ( !engine->IsDedicatedServer() || !msg.Body().account_id() )
				return true;

			if ( !CCSGameRules::sm_mapGcBanInformation.Count() )
				SetDefLessFunc( CCSGameRules::sm_mapGcBanInformation );
			
			{
				CCSGameRules::CGcBanInformation_t baninfo = { msg.Body().reason(), Plat_FloatTime() + msg.Body().seconds() };
				CCSGameRules::sm_mapGcBanInformation.InsertOrReplace( msg.Body().account_id(), baninfo );
			}

			return true;
		}
	};
	GC_REG_CLIENT_JOB( ClientJob_EMsgGCCStrike15_v2_ServerNotificationForUserPenalty, k_EMsgGCCStrike15_v2_ServerNotificationForUserPenalty );

	class ClientJob_EMsgGCCStrike15_v2_MatchEndRewardDropsNotification : public GCSDK::CGCClientJob
	{
	public:
		ClientJob_EMsgGCCStrike15_v2_MatchEndRewardDropsNotification( GCSDK::CGCClient *pGCClient ) : GCSDK::CGCClientJob( pGCClient )
		{
		}

		virtual bool BYieldingRunJobFromMsg( GCSDK::IMsgNetPacket *pNetPacket )
		{
			GCSDK::CProtoBufMsg<CMsgGCCStrike15_v2_MatchEndRewardDropsNotification> msg( pNetPacket );
			if ( !msg.Body().has_iteminfo() )
				return true;

			DevMsg( "Notification about user drop: %u %llu (%u-%u-%u)\n", msg.Body().iteminfo().accountid(), msg.Body().iteminfo().itemid(),
				msg.Body().iteminfo().defindex(), msg.Body().iteminfo().paintindex(), msg.Body().iteminfo().rarity() );

			if ( msg.Body().iteminfo().accountid() && msg.Body().iteminfo().itemid() && CSGameRules() )
			{
				CSGameRules()->RecordPlayerItemDrop( msg.Body().iteminfo() );
			}

			return true;
		}
	};
	GC_REG_CLIENT_JOB( ClientJob_EMsgGCCStrike15_v2_MatchEndRewardDropsNotification, k_EMsgGCCStrike15_v2_MatchEndRewardDropsNotification );

#endif // !NO_STEAM_GAMECOORDINATOR

#ifndef NO_STEAM_GAMECOORDINATOR
	static CMsgGCCStrike15_v2_GiftsLeaderboardResponse g_dataGiftsLeaderboard;
	static double g_dblGiftsLeaderboardReceived = 0;
#endif // !NO_STEAM_GAMECOORDINATOR
	void CCSGameRules::CheckForGiftsLeaderboardUpdate()
	{
	}

#ifndef NO_STEAM_GAMECOORDINATOR
	class ClientJob_EMsgGCCStrike15_v2_GiftsLeaderboardResponse : public GCSDK::CGCClientJob
	{
	public:
		ClientJob_EMsgGCCStrike15_v2_GiftsLeaderboardResponse( GCSDK::CGCClient *pGCClient ) : GCSDK::CGCClientJob( pGCClient )
		{
		}

		virtual bool BYieldingRunJobFromMsg( GCSDK::IMsgNetPacket *pNetPacket )
		{
			GCSDK::CProtoBufMsg<CMsgGCCStrike15_v2_GiftsLeaderboardResponse> msg( pNetPacket );
			if ( !msg.Body().has_servertime() )
				return true;

			// Set our cached structure
			g_dataGiftsLeaderboard = msg.Body();
			g_dblGiftsLeaderboardReceived = Plat_FloatTime();

			if ( CCSGameRules *pCSGR = CSGameRules() )
			{	// Copy gifts
				pCSGR->m_numGlobalGiftsGiven = g_dataGiftsLeaderboard.total_gifts_given();
				pCSGR->m_numGlobalGifters = g_dataGiftsLeaderboard.total_givers();
				pCSGR->m_numGlobalGiftsPeriodSeconds = g_dataGiftsLeaderboard.time_period_seconds();

				for ( int j = 0; j < MAX_GIFT_GIVERS_FEATURED_COUNT; ++ j )
				{
					pCSGR->m_arrFeaturedGiftersAccounts.Set( j, ( j < g_dataGiftsLeaderboard.entries().size() ) ? g_dataGiftsLeaderboard.entries( j ).accountid() : 0 );
					pCSGR->m_arrFeaturedGiftersGifts.Set( j, ( j < g_dataGiftsLeaderboard.entries().size() ) ? g_dataGiftsLeaderboard.entries( j ).gifts() : 0 );
				}
			}

			return true;
		}
	};
	GC_REG_CLIENT_JOB( ClientJob_EMsgGCCStrike15_v2_GiftsLeaderboardResponse, k_EMsgGCCStrike15_v2_GiftsLeaderboardResponse );
#endif // !NO_STEAM_GAMECOORDINATOR

	

#endif // GAME_DLL

#ifndef CLIENT_DLL
bool CCSGameRules::OnReplayPrompt( CBasePlayer *pVictim, CBasePlayer *pScorer )
{
	if ( m_iRoundWinStatus != WINNER_NONE )
	{
		// victim killed after the end of round: do not replay
		return false;
	}

	if ( CSurvivalGameRules *pSGR = IsPlayingSurvival() ? GetSurvivalRules() : NULL )
	{
		if ( pSGR->IsPlayingTeamMode() )
		{	// Consider pVictim dead, check if they have alive teammates remaining?
			CUtlVector< CCSPlayer * > arrTeammates;
			pSGR->GetPlayerTeammates( ToCSPlayer( pVictim ), arrTeammates, true );
			if ( arrTeammates.Count() )
				// If there are alive teammates remaining then this death cannot be replayed immediately
				return false;
		}
	}

	return CTeamplayRules::OnReplayPrompt( pVictim, pScorer ); // delegate to the base class
}

void CCSGameRules::ChangeLevel( void )
{

	BaseClass::ChangeLevel();

	// If the map we are changing to has a skirmish, set the convar for it (early) so that clients will get
	// the correct value on the loading screen.
	//
	// We also set it correctly on map load, but this usually lets clients see the update on their
	// load screens before the extended server info gets sent.
	//
	// $$REI Sometimes clients don't get this update before the change level happens.  I can't reliably
	//       reproduce it, not sure why that happens.
	const char* szMap = nextlevel.GetString();
	const char* szMapGroup = STRING( gpGlobals->mapGroupName );
	const char* szSkirmishMode = "";


	if ( szMap && *szMap && szMapGroup && *szMapGroup )
	{
		szSkirmishMode = g_pGameTypes->GetSkirmishModeForMap( szMapGroup, szMap );

		if ( szSkirmishMode && *szSkirmishMode )
		{
			if ( const CSkirmishModeDefinition* pSkirmishDef = GetItemSchema()->GetSkirmishModeDefinitionByName( szSkirmishMode ) )
			{
				sv_skirmish_id.SetValue( ( int )pSkirmishDef->GetID() );
			}
		}
	}

	IGameEvent * event = gameeventmanager->CreateEvent( "nextlevel_changed" );
	if ( event )
	{
		event->SetString( "nextlevel", szMap );
		event->SetString( "mapgroup", szMapGroup );
		event->SetString( "skirmish_mode", szSkirmishMode );
		gameeventmanager->FireEvent( event );
	}

}

void CCSGameRules::ReportSinglePlayerScore( CCSPlayer* pPlayer, bool bEnqueue )
{
	/* Removed for partner depot */
}
#endif

