//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=============================================================================//

#include "cbase.h"

#include "cs_gamerules_survival.h"
#include "cs_gamerules.h"
#include "dangerzone_controller.h"

#include "mathlib/hexes.h"

#ifdef GAME_DLL

#include "gametypes.h"
#include "br_items.h"
#include "point_template.h"
#include "econ/econ_entity_creation.h"
#include "nav_mesh.h"
#include "cs_player.h"
#include "props.h"
#include "util.h"
#include "func_hostage_rescue.h"
#include "item_cash.h"
#include "drone.h"
#include "weapon_tablet.h"
#include "collisionutils.h"
#include "team.h"
#include "grassburn.h"
#include "prop_counter.h"
#include "soundenvelope.h"
#include "game_timescale_shared.h"
#include "in_buttons.h"
#include "func_break.h"
#include "weapon_c4.h"
#include "imageutils.h"
#include "bitmap/bitmap.h"
#include "dt_utlvector_send.h"
#include "world.h"
#include "survival_spawn_point.h"
#include "cs_survival_funfacts.h"

#else // CLIENT_DLL

#ifdef DEBUG
#include "drone.h"
#endif

#include "bitmap/tgawriter.h"
#include "c_impact_effects.h"

#include "hud_macros.h" // HOOK_MESSAGE
#include "dt_utlvector_recv.h"
#include "c_cs_player.h"
#include "c_world.h"
#include "materialsystem/imaterialvar.h"

#endif // CLIENT_DLL

#if defined(CLIENT_DLL) && defined(PANORAMA_ENABLE)
#include "panorama/csgo_survival_spawnselect.h"
#include "panorama/csgo_survival_endofmatch.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define INVALID_SPAWN_HEX	-1

//////////////////////////////////////////////////////////////////////////
// Config stuff
#define SURVIVAL_CONFIG_FILE "cfg/survival/survival_config.kv3"
// TODO: these should probably get moved into the config
//       also perhaps tunable per-map?
DEVELOPMENT_ONLY_CONVAR( dev_dz_exploding_barrel_count_min, 5 );
DEVELOPMENT_ONLY_CONVAR( dev_dz_exploding_barrel_count_max, 7 ); // number of exploding barrels to allow in the map
DEVELOPMENT_ONLY_CONVAR( dev_dz_security_door_weight_scale, 2 ); // ? amount to count stuff behind security doors towards area loot ?
DEVELOPMENT_ONLY_CONVAR( dev_dz_bomb_quest_reward_detonate, 10 ); // number of cash bundles to spawn from exploding safes
DEVELOPMENT_ONLY_CONVAR( dev_dz_hostage_min_dist, 4000 );
DEVELOPMENT_ONLY_CONVAR( dev_dz_spawn_selection_time, 25 );
DEVELOPMENT_ONLY_CONVAR( dev_dz_spawn_selection_lock_time, 2 );
DEVELOPMENT_ONLY_CONVAR( dev_dz_spawn_selection_ready_time, 5 );
DEVELOPMENT_ONLY_CONVAR( dev_dz_random_safe_locations, 1 );
#ifdef GAME_DLL
ConVar sv_dz_player_spawn_health( "sv_dz_player_spawn_health", "120", FCVAR_RELEASE );
ConVar sv_dz_player_spawn_armor( "sv_dz_player_spawn_armor", "0", FCVAR_RELEASE );
ConVar sv_dz_player_max_health( "sv_dz_player_max_health", "120", FCVAR_RELEASE );
ConVar sv_dz_jointeam_allowed( "sv_dz_jointeam_allowed", "0", FCVAR_RELEASE, "Whether non-server admins are allowed to use the dz_jointeam command" );
ConVar sv_dz_autojointeam( "sv_dz_autojointeam", "1", FCVAR_RELEASE, "Whether players are automatically assigned a DZ team" );
#endif

//////////////////////////////////////////////////////////////////////////

ConVar sv_force_reflections( "sv_force_reflections", "0", FCVAR_RELEASE | FCVAR_REPLICATED );
ConVar sv_dz_team_count( "sv_dz_team_count", "1", FCVAR_RELEASE | FCVAR_REPLICATED, "Number of players per team" );

DEVELOPMENT_ONLY_CONVAR( dev_dz_win_condition_duration, 0.1 ); // Duration required for win condition to hold for a player to win
DEVELOPMENT_ONLY_CONVAR( dev_dz_win_condition_post, 1.5 ); // Duration required for transition from win condition to end of match
DEVELOPMENT_ONLY_CONVAR( dev_dz_win_condition_zonecheck, 0 ); // Whether winner player(s) must be in the safe zone to end the match
DEVELOPMENT_ONLY_CONVAR( dev_dz_warmup_spawn_mode, 1 ); // 0: use spawn points, 1: parachute onto map
DEVELOPMENT_ONLY_CONVAR( dev_dz_force_paradrops_count, 4 );
DEVELOPMENT_ONLY_CONVAR( dev_dz_paradrops_arrival_range_min, 30 );
DEVELOPMENT_ONLY_CONVAR( dev_dz_paradrops_arrival_range_max, 60 );
DEVELOPMENT_ONLY_CONVAR( dev_dz_chopper_arrive_time, 7 );
DEVELOPMENT_ONLY_CONVAR( dev_dz_keep_bots_in_zone, 0 ); // debug feature to stop bots from dying to danger zone damage
DEVELOPMENT_ONLY_CONVAR( dev_dz_spawn_chopper_speed, 50 );
DEVELOPMENT_ONLY_CONVAR( dev_dz_spawn_height_ground_offset, 2000 );
DEVELOPMENT_ONLY_CONVAR( dev_dz_econ_enable, 1 );
DEVELOPMENT_ONLY_CONVAR( dev_dz_spawn_height_ground_offset_warmup, 700 );
DEVELOPMENT_ONLY_CONVAR( dev_dz_drone_inflight_delivery_limit, 4 );
DEVELOPMENT_ONLY_CONVAR( dev_dz_spawn_camera_distance, 625 );
DEVELOPMENT_ONLY_CONVAR( dev_dz_spawn_camera_yaw, 12 );
DEVELOPMENT_ONLY_CONVAR( dev_dz_spawn_camera_height, 65 );

#ifdef GAME_DLL

#ifdef DEVELOPMENT_ONLY
ConVar debug_dz_show( "debug_dz_show", "", FCVAR_RELEASE );
#endif

ConVar sv_dz_warmup_weapon( "sv_dz_warmup_weapon", "weapon_glock", FCVAR_RELEASE, "" );
ConVar sv_dz_warmup_tablet( "sv_dz_warmup_tablet", "1", FCVAR_RELEASE, "" );
ConVar sv_dz_show_enemy_name_scope_range( "sv_dz_show_enemy_name_scope_range", "800", FCVAR_RELEASE );
extern ConVar sv_dz_contractkill_reward;
extern ConVar sv_dz_cash_bundle_size;
extern ConVar contributionscore_cash_bundle;

#endif // GAME_DLL

#ifdef DEBUG
ConVar follow_drone( "follow_drone", "0" );
#endif

static const char* s_szTeamColorLoc[] = {
	"#Survival_TeamColor_Blue",
	"#Survival_TeamColor_Aqua",
	"#Survival_TeamColor_Banana",
	"#Survival_TeamColor_Magenta",
	"#Survival_TeamColor_LtBlue",
	"#Survival_TeamColor_Lime",
	"#Survival_TeamColor_Orange",
	"#Survival_TeamColor_Pink",
	"#Survival_TeamColor_Purple",
	"#Survival_TeamColor_White",
	"#Survival_TeamColor_Peach",
	"#Survival_TeamColor_Mint",
	"#Survival_TeamColor_Fuchsia",
	"#Survival_TeamColor_Green",
	"#Survival_TeamColor_Lemon",
	"#Survival_TeamColor_Red",
	"#Survival_TeamColor_Grey",
};

COMPILE_TIME_ASSERT( WORLD_HEX_NUM == WORLD_HEX_WIDTH * WORLD_HEX_HEIGHT );

BEGIN_NETWORK_TABLE_NOBASE( CSurvivalGameRules, DT_SurvivalGameRules )
#ifdef CLIENT_DLL
	RecvPropVector( RECVINFO( m_vecPlayAreaMins ) ),
	RecvPropVector( RECVINFO( m_vecPlayAreaMaxs ) ),
	RecvPropArray3( RECVINFO_ARRAY( m_iPlayerSpawnHexIndices ), RecvPropInt( RECVINFO( m_iPlayerSpawnHexIndices[0] ) ) ),
	RecvPropArray3( RECVINFO_ARRAY( m_SpawnTileState ), RecvPropInt( RECVINFO( m_SpawnTileState[0] ) ) ),
	RecvPropFloat( RECVINFO( m_flSpawnSelectionTimeStart ) ),
	RecvPropFloat( RECVINFO( m_flSpawnSelectionTimeEnd ) ),
	RecvPropFloat( RECVINFO( m_flSpawnSelectionTimeLoadout ) ),
	RecvPropInt( RECVINFO( m_spawnStage ) ),
	RecvPropFloat( RECVINFO( m_flTabletHexOriginX ) ),
	RecvPropFloat( RECVINFO( m_flTabletHexOriginY ) ),
	RecvPropFloat( RECVINFO( m_flTabletHexSize ) ),
	RecvPropArray3( RECVINFO_ARRAY( m_roundData_playerXuids ), RecvPropInt( RECVINFO( m_roundData_playerXuids[0] ) ) ),
	RecvPropArray3( RECVINFO_ARRAY( m_roundData_playerPositions ), RecvPropInt( RECVINFO( m_roundData_playerPositions[0] ) ) ),
	RecvPropArray3( RECVINFO_ARRAY( m_roundData_playerTeams ), RecvPropInt( RECVINFO( m_roundData_playerTeams[0] ) ) ),
	RecvPropArray3( RECVINFO_ARRAY( m_SurvivalGameRuleDecisionTypes ), RecvPropInt( RECVINFO( m_SurvivalGameRuleDecisionTypes[ 0 ] ) ) ),
	RecvPropArray3( RECVINFO_ARRAY( m_SurvivalGameRuleDecisionValues ), RecvPropInt( RECVINFO( m_SurvivalGameRuleDecisionValues[ 0 ] ) ) ),
	RecvPropFloat( RECVINFO( m_flSurvivalStartTime ) ),
#else // GAME_DLL
	SendPropVector( SENDINFO( m_vecPlayAreaMins ), -1, SPROP_COORD ),
	SendPropVector( SENDINFO( m_vecPlayAreaMaxs ), -1, SPROP_COORD ),
	SendPropArray3( SENDINFO_ARRAY3( m_iPlayerSpawnHexIndices ), SendPropInt( SENDINFO_ARRAY( m_iPlayerSpawnHexIndices ), 10 ) ),
	SendPropArray3( SENDINFO_ARRAY3( m_SpawnTileState ), SendPropInt( SENDINFO_ARRAY( m_SpawnTileState ), 3, SPROP_UNSIGNED ) ),
	SendPropFloat( SENDINFO( m_flSpawnSelectionTimeStart ) ),
	SendPropFloat( SENDINFO( m_flSpawnSelectionTimeEnd ) ),
	SendPropFloat( SENDINFO( m_flSpawnSelectionTimeLoadout ) ),
	SendPropInt( SENDINFO( m_spawnStage ), CountEnumBits( CSurvivalGameRules::SPAWN_STAGE_COUNT ), SPROP_UNSIGNED ),
	SendPropFloat( SENDINFO( m_flTabletHexOriginX ) ),
	SendPropFloat( SENDINFO( m_flTabletHexOriginY ) ),
	SendPropFloat( SENDINFO( m_flTabletHexSize ) ),
	SendPropArray3( SENDINFO_ARRAY3( m_roundData_playerXuids ), SendPropInt( SENDINFO_ARRAY( m_roundData_playerXuids ), 64, SPROP_UNSIGNED ) ),
	SendPropArray3( SENDINFO_ARRAY3( m_roundData_playerPositions ), SendPropInt( SENDINFO_ARRAY( m_roundData_playerPositions ) ) ),
	SendPropArray3( SENDINFO_ARRAY3( m_roundData_playerTeams ), SendPropInt( SENDINFO_ARRAY( m_roundData_playerTeams ) ) ),
	SendPropArray3( SENDINFO_ARRAY3( m_SurvivalGameRuleDecisionTypes ), SendPropInt( SENDINFO_ARRAY( m_SurvivalGameRuleDecisionTypes ), 3, SPROP_UNSIGNED ) ),
	SendPropArray3( SENDINFO_ARRAY3( m_SurvivalGameRuleDecisionValues ), SendPropInt( SENDINFO_ARRAY( m_SurvivalGameRuleDecisionValues ) ) ),
	SendPropFloat( SENDINFO( m_flSurvivalStartTime ) ),
#endif // GAME_DLL
END_NETWORK_TABLE()

#define HELICOPTER_MODEL_CARGO "models/props_vehicles/helicopter_rescue.mdl"
#define HELICOPTER_MODEL_BLACKHAWK "models/props_survival/helicopter/blackhawk.mdl"

#define C4TARGET_MODEL "models/props_survival/safe/safe.mdl"
#define C4TARGET_DOOR_MODEL "models/props_survival/safe/safe_door.mdl"

#ifdef GAME_DLL
class CTriggerSafeMoneyGather : public CBaseTrigger
{
public:
	DECLARE_CLASS( CTriggerSafeMoneyGather, CBaseTrigger );
	DECLARE_DATADESC();

	void Spawn( void );
	void SafeMoneyTriggerTouch( CBaseEntity *pOther );

	CUtlVector< EHANDLE > m_vecCashBundles;
};
LINK_ENTITY_TO_CLASS( trigger_safemoneygather, CTriggerSafeMoneyGather );
BEGIN_DATADESC( CTriggerSafeMoneyGather )
DEFINE_FUNCTION( SafeMoneyTriggerTouch ),
END_DATADESC()

void CTriggerSafeMoneyGather::Spawn( void )
{
	BaseClass::Spawn();
	InitTrigger();

	// fixme: are these all necessary?
	SetMoveType( MOVETYPE_NONE );
	SetSolid( SOLID_OBB );
	SetSolidFlags( FSOLID_NOT_SOLID | FSOLID_TRIGGER );
	AddSpawnFlags( SF_TRIGGER_ALLOW_CLIENTS | SF_TRIGGER_ALLOW_NPCS );
	CollisionProp()->AddSolidFlags( FSOLID_TRIGGER_TOUCH_PLAYER );

	SetTouch( &CTriggerSafeMoneyGather::SafeMoneyTriggerTouch );
}
void CTriggerSafeMoneyGather::SafeMoneyTriggerTouch( CBaseEntity *pOther )
{
	CBasePlayer *pPlayer = ToBasePlayer( pOther );
	if ( !pPlayer || !pPlayer->IsAlive() )
		return;  // only alive players plz

	FOR_EACH_VEC_BACK( m_vecCashBundles, n )
	{
		if ( m_vecCashBundles[n].Get() )
		{
			CItemCash *pCash = dynamic_cast<CItemCash*>(m_vecCashBundles[n].Get());
			Assert( pCash );

			if ( pCash )
			{
				pCash->MyTouch( pPlayer );
			}
		}
		
		m_vecCashBundles.Remove( n );
	}

	//NDebugOverlay::EntityBounds( this, 0, 250, 0, 63, 20 );

	UTIL_Remove( this );
}
#endif

DECLARE_AUTO_LIST( IBRC4Target );
#ifdef CLIENT_DLL
#define CBRC4Target C_BRC4Target
#endif
DEVELOPMENT_ONLY_CONVAR( dev_dz_safe_arming_time, 6 );
DEVELOPMENT_ONLY_CONVAR( dev_dz_safe_warning_distance, 1300 );
class CBRC4Target : public CBaseAnimating, public IBRC4Target
{
public:
	DECLARE_CLASS( CBRC4Target, CBaseAnimating );
	DECLARE_DATADESC();
	DECLARE_NETWORKCLASS();
	IMPLEMENT_AUTO_LIST_GET();

	virtual int	ObjectCaps() { return BaseClass::ObjectCaps() | FCAP_IMPULSE_USE; }

#ifdef GAME_DLL
	virtual int ShouldTransmit( const CCheckTransmitInfo *pInfo ) { return FL_EDICT_ALWAYS; }
	virtual int UpdateTransmitState() { return SetTransmitState( FL_EDICT_ALWAYS ); }

	CBRC4Target()
	{
		m_prolongeduse.ConfigSetPlayerBlockingUseAction( k_CSPlayerBlockingUseAction_OpeningSafe );
		m_prolongeduse.ConfigSetUseDurationToCompletion( dev_dz_safe_arming_time.GetFloat() );
	}

	CEntitySupportForProlongedUse_t m_prolongeduse;

	void OnUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
	{
		if ( m_bBrokenOpen )
			return;

		CCSPlayer* player = pActivator ? ToCSPlayer( pActivator ) : NULL;
		if ( !player )
			return;

		switch ( m_prolongeduse.ETryToBeginUse( this, player ) )
		{
		case CEntitySupportForProlongedUse_t::k_EBeginUseResult_BeingUsedByOther:
			ClientPrint( player, HUD_PRINTCENTER, "#SFUIHUD_InfoPanel_OpeningSafe_Other" );
			return;
		case CEntitySupportForProlongedUse_t::k_EBeginUseResult_Started:
			EmitSound( "c4.disarmstart" );
			SetThink( &CBRC4Target::UseThink );
			SetNextThink( gpGlobals->curtime );
			return;
		default:
			return;
		}
	}

	EHANDLE m_hPlayerThatActivatedMe;
	void WarnPlayerThink( void )
	{
		if ( m_bBrokenOpen )
			return;

		SetNextThink( gpGlobals->curtime + 0.5f );
		SetThink( &CBRC4Target::WarnPlayerThink );

		CCSPlayer *pActivator = ToCSPlayer( m_hPlayerThatActivatedMe.Get() );
		if ( pActivator && pActivator->IsAlive() && pActivator->GetAbsOrigin().DistTo( GetAbsOrigin() ) < dev_dz_safe_warning_distance.GetFloat() )
		{
			ClientPrint( pActivator, HUD_PRINTCENTER, "#SFUIHUD_InfoPanel_TooCloseToSafe" );
		}
	}

	void UseThink( void )
	{
		CCSPlayer *pActivator = NULL;
		switch ( m_prolongeduse.ESupportUseThink( this, &pActivator ) )
		{
		case CEntitySupportForProlongedUse_t::k_EUseInProgressOutcome_NotInUse:
			SetThink( NULL );
			return;

		default:
		case CEntitySupportForProlongedUse_t::k_EUseInProgressOutcome_StillUsing:
			SetNextThink( gpGlobals->curtime + 0.1f );
			return;

		case CEntitySupportForProlongedUse_t::k_EUseInProgressOutcome_Aborted:
			SetThink( NULL );
			return;

		case CEntitySupportForProlongedUse_t::k_EUseInProgressOutcome_Completed:
			SetUse( NULL );
			SetThink( NULL );
			SetArmed( pActivator );
			return;
		}
	}

	void SetArmed( CCSPlayer *pActivator )
	{
		//create c4
		CPlantedC4 *pPlantedC4 = dynamic_cast< CPlantedC4* >( CreateEntityByName( "planted_c4_survival" ) );
		Assert( pPlantedC4 );
		if ( !pPlantedC4 ) return;

		EmitSound( "Survival.BreachUse" );

		pPlantedC4->m_bCannotBeDefused = true;
		pPlantedC4->SetParent( this );
		pPlantedC4->SetAbsOrigin( GetAbsOrigin() );
		pPlantedC4->SetOwnerEntity( pActivator ); // ensure that the kill that might happen is credited correctly (see: CPlantedC4::Explode)

		static const float kSafeBombTimer = 25.0f;
		pPlantedC4->ActivateSetTimerLength( kSafeBombTimer );

		UTIL_PushGlobalTabletNotification( TABLET_NOTIFICATION_BOMB_PLANTED_ON_TARGET );

		SetBodygroupPreset( "armed" );

		SetHighlightColor( 255, 30, 0 );

		m_hPlayerThatActivatedMe = pActivator;
		SetThink( &CBRC4Target::WarnPlayerThink );
		SetNextThink( gpGlobals->curtime + 0.5f );
	}

#endif

	bool IsInRange( const Vector &vecPos )
	{
		return vecPos.DistToSqr( GetAbsOrigin() ) <= Sqr( m_flRadius );
	}

	virtual void Precache()
	{
		BaseClass::Precache();
		PrecacheModel( C4TARGET_MODEL );
		PrecacheModel( C4TARGET_DOOR_MODEL );
		PrecacheScriptSound( "Survival.BreachUse" );
	}

#ifdef GAME_DLL
	void TraceAlignToGround()
	{
		SetThink( NULL );

		// try and face 'outwards' so the interesting part of the safe isn't looking at a wall
		{
			Vector vecAvgHit;
			Vector vecCenter = WorldSpaceCenter();
			for ( int i = 0; i < 14; i++ )
			{
				Vector vecRandom = RandomVectorOnUnitSphere();
				vecRandom.z = 0;
				vecRandom.NormalizeInPlace();

				trace_t tr;
				UTIL_TraceLine( vecCenter, vecCenter + vecRandom * 300.0f, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );
				vecAvgHit += tr.endpos;
				UTIL_TraceLine( vecCenter, vecCenter - vecRandom * 300.0f, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );
				vecAvgHit += tr.endpos;
			}
			vecAvgHit /= 16.0f;
			vecAvgHit -= vecCenter;
			vecAvgHit.z = 0;
			vecAvgHit.NormalizeInPlace();

			//debugoverlay->AddLineOverlay( vecCenter, vecCenter + vecAvgHit * 300.0f, 0, 255, 0, true, 40.0f );

			QAngle angTemp;
			VectorAngles( vecAvgHit, angTemp );
			angTemp[PITCH] = 0.0f;
			angTemp[ROLL] = 0.0f;
			SetAbsAngles( angTemp );
		}

		// set on the ground
		{
			Vector vecCenter = WorldSpaceCenter();
			trace_t tr;
			UTIL_TraceLine( vecCenter, vecCenter - Vector( 0, 0, 45 ), MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );
			SetAbsOrigin( tr.endpos );

			//debugoverlay->AddLineOverlay( tr.startpos, tr.endpos, 0, 255, 0, true, 40.0f );
		}
	}
#endif

	virtual void Spawn()
	{
		Precache();

#ifdef GAME_DLL
		m_flRadius = 500.0f; // fixme: is this needed anymore?

		SetModel( C4TARGET_MODEL );
		SetBodygroupPreset( "prearmed" );

		SetSolid( SOLID_VPHYSICS );
		SetMoveType( MOVETYPE_NONE );
		SetCollisionGroup( COLLISION_GROUP_NONE );

		m_bBrokenOpen = false;

		SetUse( &CBRC4Target::OnUse );

		// green highlight
		m_bEligibleForScreenHighlight = true;
		SetHighlightColor( 0, 255, 0 );

		SetThink( &CBRC4Target::TraceAlignToGround );
		SetNextThink( gpGlobals->curtime + 1.0f );
#endif

		AddFlag( FL_OBJECT ); // have this entity participate in high-priority entity pickup rules

		m_takedamage = DAMAGE_YES;

		BaseClass::Spawn();
	}

#ifdef GAME_DLL
	virtual int OnTakeDamage( const CTakeDamageInfo &info ) OVERRIDE
	{
		if ( m_bBrokenOpen )
			return 0;

		// if damage dealer 
		CPlantedC4 *pPlantedC4 = dynamic_cast< CPlantedC4* >( info.GetInflictor() );
		if ( !pPlantedC4 || !IsInRange( pPlantedC4->GetAbsOrigin() ) )
		{
			return 0; // ignore any non-c4 damage
		}

		m_bBrokenOpen = true;

		// remove highlight
		m_bEligibleForScreenHighlight = false;
		SetHighlightColor( 0, 0, 0 );

		UTIL_PushGlobalTabletNotification( TABLET_NOTIFICATION_BOMB_DETONATED_ON_TARGET );

		SetBodygroupPreset( "exploded" );

		CPhysicsProp *pProp = dynamic_cast<CPhysicsProp *>(CreateEntityByName( "prop_physics" ));
		if ( pProp )
		{
			pProp->SetAbsOrigin( GetAbsOrigin() );
			pProp->SetAbsAngles( GetAbsAngles() );
			pProp->KeyValue( "model", C4TARGET_DOOR_MODEL );
			pProp->Spawn();
			pProp->SetCollisionGroup( COLLISION_GROUP_DEBRIS );
		}

		extern ConVar sv_dz_cash_bundle_size;

		// create a trigger helper for cash pickup
		CTriggerSafeMoneyGather *pMoneyTrigger = (CTriggerSafeMoneyGather *)CreateEntityByName( "trigger_safemoneygather" );
		if ( pMoneyTrigger )
		{
			pMoneyTrigger->SetAbsAngles( GetAbsAngles() );
			pMoneyTrigger->SetAbsOrigin( GetAbsOrigin() );

			DispatchSpawn( pMoneyTrigger );

			pMoneyTrigger->SetParent( this ); // so the trigger is removed on parent deletion too
			pMoneyTrigger->SetCollisionBounds( Vector( -30, -30, 0 ), Vector( 30, 30, 55 ) ); // fixme, use 110% parent model bounds or something instead of hardcode

			//NDebugOverlay::EntityBounds( pMoneyTrigger, 200, 180, 63, 63, 10 );

			CUtlVector<CBaseEntity*> pVecCashEnts;
			UTIL_SpawnPhysicalCash( GetAbsOrigin() + Vector( 0, 0, 50 ), dev_dz_bomb_quest_reward_detonate.GetInt() * sv_dz_cash_bundle_size.GetInt(), 0, "c4_exploded", CASH_SPAWN_WITH_RANDOM_VELOCITY, &pVecCashEnts );

			FOR_EACH_VEC( pVecCashEnts, n )
			{
				pMoneyTrigger->m_vecCashBundles.AddToTail( pVecCashEnts[n] );
			}
		}
		else
		{
			Assert( false ); // we failed to create the trigger
			UTIL_SpawnPhysicalCash( GetAbsOrigin() + Vector( 0, 0, 50 ), dev_dz_bomb_quest_reward_detonate.GetInt() * sv_dz_cash_bundle_size.GetInt(), 0, "c4_exploded" );
		}
		
		return 0; // don't actually take damage, we manually set our model to the 'broken' version.
	}
#endif // GAME_DLL

	CNetworkVar( bool, m_bBrokenOpen );
private:
	CNetworkVar( float, m_flRadius );
};

BEGIN_DATADESC( CBRC4Target )
	DEFINE_KEYFIELD( m_flRadius, FIELD_FLOAT, "radius" ),
END_DATADESC()

IMPLEMENT_NETWORKCLASS_ALIASED( BRC4Target, DT_BRC4Target )
LINK_ENTITY_TO_CLASS_ALIASED( func_survival_c4_target, BRC4Target );
BEGIN_NETWORK_TABLE( CBRC4Target, DT_BRC4Target )
#ifdef GAME_DLL
	SendPropBool( SENDINFO( m_bBrokenOpen ) ),
	SendPropFloat( SENDINFO( m_flRadius ) ),
#else
	RecvPropBool( RECVINFO( m_bBrokenOpen ) ),
	RecvPropFloat( RECVINFO( m_flRadius ) ),
#endif
END_NETWORK_TABLE()

IMPLEMENT_AUTO_LIST( IBRC4Target )

#ifndef CLIENT_DLL
#define FOR_EACH_PLAYER( _iPlayer ) for ( int _iPlayer = 1; _iPlayer <= MAX_PLAYERS; ++_iPlayer )
CCSPlayer* UTIL_ValidatePlayer( int i, bool bIgnoreDead, bool bIgnoreSpectators, bool bIgnoreUnassigned, bool bIgnoreBots )
{
	CCSPlayer *entity = CCSPlayer::Instance( i );

	if ( !entity || FNullEnt( entity->edict() ) )
		return NULL;

	// HLTV observer should never affect the game
	if ( entity->IsHLTV() )
		return NULL;

	if ( bIgnoreDead && !entity->IsAlive() )
		return NULL;

	if ( FStrEq( entity->GetPlayerName(), "" ) )
		return NULL;

	if ( FBitSet( entity->GetFlags(), FL_FAKECLIENT ) && (bIgnoreBots || !entity->IsBot()) )
		return NULL;

	if ( bIgnoreSpectators && entity->GetTeamNumber() != TEAM_TERRORIST && entity->GetTeamNumber() != TEAM_CT )
		return NULL;

	if ( bIgnoreSpectators && entity->State_Get() == STATE_PICKINGCLASS )
		return NULL;

	if ( bIgnoreUnassigned && entity->GetTeamNumber() == TEAM_UNASSIGNED )
		return NULL;
	
	return entity;
}
#endif

#ifdef CLIENT_DLL

#include "view_shared.h"
#include "viewrender.h"
#include "cdll_client_int.h"

#ifdef DEVELOPMENT_ONLY
ConVar render_radar_tile( "render_radar_tile", "128", FCVAR_RELEASE );
ConVar render_radar_res( "render_radar_res", "4096", FCVAR_RELEASE );
ConVar render_radar_padding( "render_radar_padding", "16", FCVAR_RELEASE );
ConVar render_radar_pitch( "render_radar_pitch", "90", FCVAR_RELEASE );
ConVar render_radar_trace_ratio( "render_radar_trace_ratio", "0.9", FCVAR_RELEASE );

CON_COMMAND( render_radar, "" )
{
	// Ensure that mat_queue_mode is zero...this ConVarRef lookup isn't cheap, but this is rarely-run debug code
	ConVarRef mat_queue_mode( "mat_queue_mode" );
	if ( mat_queue_mode.GetInt() != 0 )
	{
		DevMsg( "Error: mat_queue_mode must be 0.\n" );
		return;
	}

	int nOutTargaRes = render_radar_res.GetInt();
	int nRenderTileSize = render_radar_tile.GetInt();
	int nRenderTilePadding = render_radar_padding.GetInt();
	int nRenderTileSizePadded = nRenderTileSize + nRenderTilePadding + nRenderTilePadding;

	float flPaddingRatio = (nRenderTileSizePadded > 0) ? (float)nRenderTileSizePadded / (float)nRenderTileSize : 1.0f;

	unsigned char *pImageOutput = new unsigned char[nOutTargaRes * nOutTargaRes * 4];
	unsigned char *pImageRender = new unsigned char[nRenderTileSizePadded * nRenderTileSizePadded * 4];

	#define SET_RADAR_PIXEL( x, y, r, g, b, a ) \
	pImageOutput[ (4 * ((y * nOutTargaRes) + x)) + 0 ] = ((int)r); \
	pImageOutput[ (4 * ((y * nOutTargaRes) + x)) + 1 ] = ((int)g); \
	pImageOutput[ (4 * ((y * nOutTargaRes) + x)) + 2 ] = ((int)b); \
	pImageOutput[ (4 * ((y * nOutTargaRes) + x)) + 3 ] = ((int)a); \
	
	Vector vecColor = vec3_origin;

	int nWorldWidth = 16384;
	int nHalfWorldWidth = nWorldWidth / 2;
	
	int nWorldTileSize = nWorldWidth / (nOutTargaRes / nRenderTileSize);
	int nHalfWorldTileSize = nWorldTileSize / 2;

	int nWorldTileSizePadded = nWorldTileSize * flPaddingRatio;
	int nHalfWorldTileSizePadded = nWorldTileSizePadded / 2;

	QAngle angCamera = QAngle( render_radar_pitch.GetFloat(), 90, 0 );
	Vector vecCamera;
	AngleVectors( angCamera, &vecCamera );

	for ( int y = 0; y < nOutTargaRes; y += nRenderTileSize )
	{
		for ( int x = 0; x < nOutTargaRes; x += nRenderTileSize )
		{
			int nWorldX = RemapValClamped( x, 0, nOutTargaRes, -nHalfWorldWidth, nHalfWorldWidth ) + nHalfWorldTileSize;
			int nWorldY = RemapValClamped( y, 0, nOutTargaRes, nHalfWorldWidth, -nHalfWorldWidth ) - nHalfWorldTileSize;

			Vector vecTraceEnd = Vector( nWorldX, nWorldY, 0 );
			Vector vecTraceStart = vecTraceEnd - vecCamera * 5250;

			trace_t tr;
			UTIL_TraceHull( vecTraceStart, vecTraceEnd, Vector( -1, -1, 0 ) * nHalfWorldTileSizePadded, Vector( 1, 1, 0 ) * nHalfWorldTileSizePadded, MASK_SHOT, NULL, COLLISION_GROUP_NONE, &tr );

			if ( tr.DidHit() )
			{
				Vector vecPt = Lerp( tr.fraction * render_radar_trace_ratio.GetFloat(), vecTraceStart, vecTraceEnd );

				CMatRenderContextPtr pRenderContext( materials );

				CViewSetup	viewTemp;
				memset( &viewTemp, 0, sizeof( viewTemp ) );

				viewTemp.origin = vecPt;
				viewTemp.m_flAspectRatio = 1.0f;
				viewTemp.m_bRenderToSubrectOfLargerScreen = true;
				viewTemp.zNear = 8.0f;
				viewTemp.zFar = 28400.0f;
				viewTemp.x = 0;
				viewTemp.y = 0;

				viewTemp.m_bCustomProjMatrix = true;
				MatrixBuildOrtho( viewTemp.m_matCustomProjMatrix, -nHalfWorldTileSizePadded, -nHalfWorldTileSizePadded, nHalfWorldTileSizePadded, nHalfWorldTileSizePadded, 1, 20000 );

				viewTemp.width = (float)nRenderTileSizePadded;
				viewTemp.height = (float)nRenderTileSizePadded;

				int backbufferWidth, backbufferHeight;
				materials->GetBackBufferDimensions( backbufferWidth, backbufferHeight );
				pRenderContext->Viewport( 0, 0, backbufferWidth, backbufferHeight );

				viewTemp.angles = angCamera;
				viewTemp.fov = 90;
				viewTemp.origin = vecPt;

				extern IViewRender *GetViewRenderInstance();
				if ( CViewRender *pViewRenderInstance = dynamic_cast<CViewRender *>(GetViewRenderInstance()) )
					pViewRenderInstance->RenderView( viewTemp, viewTemp, 0, 0 );

				materials->SwapBuffers();

				pRenderContext->ReadPixels( 0, 0, nRenderTileSizePadded, nRenderTileSizePadded, pImageRender, IMAGE_FORMAT_RGBA8888 );

				for ( int nPixelY = 0; nPixelY < nRenderTileSize; nPixelY++ )
				{
					for ( int nPixelX = 0; nPixelX < nRenderTileSize; nPixelX++ )
					{
						int nOutputIndex = ((y + nPixelY) * nOutTargaRes * 4) + ((x + nPixelX) * 4);
						int nRenderTileIndex = ((nRenderTilePadding + nPixelY) * nRenderTileSizePadded * 4) + ((nRenderTilePadding + nPixelX) * 4);

						pImageOutput[nOutputIndex + 0] = pImageRender[nRenderTileIndex + 0];
						pImageOutput[nOutputIndex + 1] = pImageRender[nRenderTileIndex + 1];
						pImageOutput[nOutputIndex + 2] = pImageRender[nRenderTileIndex + 2];
						pImageOutput[nOutputIndex + 3] = pImageRender[nRenderTileIndex + 3];
					}
				}

			}
		}
	}

	delete[] pImageRender;

	// allocate a buffer to write the tga into
	int iMaxTGASize = nOutTargaRes + (nOutTargaRes * nOutTargaRes * 4);
	void *pTGA = malloc( iMaxTGASize );
	CUtlBuffer buffer( pTGA, iMaxTGASize );

	if ( !TGAWriter::WriteToBuffer( pImageOutput, buffer, nOutTargaRes, nOutTargaRes, IMAGE_FORMAT_RGBA8888, IMAGE_FORMAT_RGBA8888 ) )
	{
		Error( "Couldn't write tga.\n" );
	}

	delete[] pImageOutput;

	// async write to disk (this will take ownership of the memory)
	char szPathedFileName[_MAX_PATH];
	Q_snprintf( szPathedFileName, sizeof( szPathedFileName ), "//MOD/render_radar.tga" );

	FileHandle_t fileTGA = filesystem->Open( szPathedFileName, "wb" );
	filesystem->Write( buffer.Base(), buffer.TellPut(), fileTGA );
	filesystem->Close( fileTGA );

	free( pTGA );

	Msg( "Done!" );

}
#endif
#endif

#ifdef GAME_DLL

#ifdef DEBUG
CON_COMMAND( debug_dz_config_test, "" )
{
	const char* filename = SURVIVAL_CONFIG_FILE;
	if ( args.ArgC() > 1 )
		filename = args[1];

	CBrConfig* pConfig = CBrConfig::Load( filename, NULL );
	CBrConfig::Free( pConfig );
}
#endif

CON_COMMAND( sv_dz_reset_danger_zone, "" )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
	{
		Assert( false );
		return;
	}

	CSurvivalGameRules* pBRrules = CSGameRules()->GetSurvivalRules();
	if ( pBRrules )
	{
		pBRrules->ResetDangerZone();
	}
}


#endif

uint64 UTIL_GetPlayerXUID( CCSPlayer* pPlayer )
{
	CSteamID steamId;
	if ( pPlayer->GetSteamID( &steamId ) )
		return steamId.ConvertToUint64();

	Assert( pPlayer->IsBot() );
	return pPlayer->entindex();
}



#ifdef CLIENT_DLL
bool __MsgFunc_SSUI( const CCSUsrMsg_SSUI & msg )
{
	// Show/hide the spawn select UI
#ifdef PANORAMA_ENABLE
	// When we show the UI, we need to wait until we've gotten the network update containing the
	// new tablet offset values -- so wait for the frame to end instead of doing it immediately here.
	panorama::DispatchEventAsync( SurvivalShowSpawnSelect(), ( panorama::IUIPanelClient* )nullptr, msg.show() );
#else
	AssertMsg( false, "Survival spawn UI only implemented in Panorama" );
#endif

	return true;
}

bool __MsgFunc_SurvivalStats( const CCSUsrMsg_SurvivalStats& msg )
{
	CSurvivalGameRules* pSurvivalGameRules = CSGameRules() ? CSGameRules()->GetSurvivalRules() : nullptr;
	if ( pSurvivalGameRules )
		pSurvivalGameRules->HandleMsgSurvivalStats( msg );

	return true;
}

void CSurvivalGameRules::HandleMsgSurvivalStats( const CCSUsrMsg_SurvivalStats& msg )
{
	uint64 xuid = msg.xuid();
	if ( xuid == 0 )
		return;

	uint64 localPlayerXuid = 0;
	C_CSPlayer* pPlayer = C_CSPlayer::GetLocalCSPlayer();

	// when watching GOTV replays, use steamid of current client
	CSteamID localSteamId;
	if ( pPlayer->IsHLTV() && engine->GetSteamAPIContext() && engine->GetSteamAPIContext()->SteamUser() )
	{
		localPlayerXuid = engine->GetSteamAPIContext()->SteamUser()->GetSteamID().ConvertToUint64();
	}
	else if ( pPlayer->GetSteamID( &localSteamId ) )
	{
		localPlayerXuid = localSteamId.ConvertToUint64();
	}
	else
	{
		localPlayerXuid = 0;
	}

	// save stats for local player
	if ( xuid == localPlayerXuid )
	{
		m_localSurvivalStatsMsg = MakeOwned( msg );
	}

	// save latest message so we can use it to get player positions
	if ( !m_latestSurvivalStatsMsg || msg.ticknumber() >= m_latestSurvivalStatsMsg->ticknumber() )
	{
		m_latestSurvivalStatsMsg = MakeOwned( msg );
	}

#ifdef PANORAMA_ENABLE
	// Let any screens that care about this data know that it has been updated
	panorama::DispatchEventAsync( CSGOSurvivalStatsUpdated(), ( panorama::IUIPanelClient* )nullptr );
#endif
}

// only works for getting the local player's stats
const CCSUsrMsg_SurvivalStats* CSurvivalGameRules::GetLocalPlayerStatMsg()
{
	if ( !m_localSurvivalStatsMsg )
		return nullptr;

	return &*m_localSurvivalStatsMsg;
}

const CCSUsrMsg_SurvivalStats* CSurvivalGameRules::GetLatestStatMsg()
{
	if ( !m_latestSurvivalStatsMsg )
		return nullptr;

	return &*m_latestSurvivalStatsMsg;
}

int CSurvivalGameRules::GetPlacementFromStatMsgs( uint64 xuid, bool bIncludeTeam, int* pTeamNumber, bool* pbCanImprove )
{
	if ( xuid == 0 )
		return -1;

	int nPlacement = 0;
	int nTeamNumber = -1;

	if ( m_latestSurvivalStatsMsg )
	{
		const CCSUsrMsg_SurvivalStats& msg = *m_latestSurvivalStatsMsg;

		for ( int i = 0; i < msg.users().size(); ++i )
		{
			if ( msg.users( i ).xuid() == xuid )
			{
				nPlacement = msg.users( i ).placement();
				if ( msg.users( i ).has_teamnumber() )
					nTeamNumber = msg.users( i ).teamnumber();
			}
		}
	}
	else
	{
		// if we haven't gotten a message from the server, use latest data from gamerules replicated variables
		for ( int i = 1; i <= MAX_PLAYERS; ++i )
		{
			if ( m_roundData_playerXuids.Get(i) == xuid )
			{
				nPlacement = m_roundData_playerPositions.Get( i );
				nTeamNumber = m_roundData_playerTeams.Get( i );
			}
		}
	}

	bool bCanImprove = false;
	if( nTeamNumber >= 0 && bIncludeTeam )
	{
		// check for better placement from teammates

		if ( m_latestSurvivalStatsMsg )
		{
			const CCSUsrMsg_SurvivalStats& msg = *m_latestSurvivalStatsMsg;

			for ( int i = 0; i < msg.users().size(); ++i )
			{
				if ( msg.users( i ).xuid() != xuid && msg.users( i ).has_teamnumber() && msg.users( i ).teamnumber() == nTeamNumber )
				{
					int nTeammatesPlacement = msg.users( i ).placement();
					if ( nTeammatesPlacement <= 0 )
						bCanImprove = true;
					else if ( nTeammatesPlacement < nPlacement )
						nPlacement = nTeammatesPlacement;
				}
			}
		}
		else
		{
			// again, if no message from server, use latest data from gamerules replicated variables
			for ( int i = 1; i <= MAX_PLAYERS; ++i )
			{
				if ( m_roundData_playerXuids.Get( i ) != xuid && m_roundData_playerTeams.Get( i ) == nTeamNumber )
				{
					int nTeammatesPlacement = m_roundData_playerPositions.Get( i );
					if ( nTeammatesPlacement <= 0 )
						bCanImprove = true;
					else if ( nTeammatesPlacement < nPlacement )
						nPlacement = nTeammatesPlacement;
				}
			}
		}
	}

	// return values
	if ( pTeamNumber )
		*pTeamNumber = nTeamNumber;
	if ( pbCanImprove )
		*pbCanImprove = bCanImprove;

	return nPlacement;
}
#endif // CLIENT_DLL


#define EarlyOutIfNotPlayingSurvival if ( CSGameRules() == NULL || !CSGameRules()->IsPlayingSurvival() ) { return; }

CSurvivalGameRules::CSurvivalGameRules()
{
#ifdef GAME_DLL
	ResetLocalEventVars();

	m_vecPlayerSpawnLocations.RemoveAll();

	m_pBrConfig = NULL;

	m_bBoundsInitialized = false;

	m_flSpawnSelectionTimeStart = -1.f;
	m_flSpawnSelectionTimeEnd = -1.f;
	m_flSpawnSelectionTimeLoadout = -1.f;
	m_spawnStage = SPAWN_STAGE_NONE;

	m_nPlayersOnNextTeam = 0;
	m_nTotalNumSurvivalTeams = 0;

	m_flTabletHexOriginX = 0.0f;
	m_flTabletHexOriginY = 0.0f;
	m_flTabletHexSize = 100.0f;

#endif // GAME_DLL

#ifdef CLIENT_DLL
	HOOK_MESSAGE( SSUI );
	HOOK_MESSAGE( SurvivalStats );

	m_localSurvivalStatsMsg = nullptr;
	m_latestSurvivalStatsMsg = nullptr;
#endif // CLIENT_DLL

	m_flLastThinkTime = 0;

	m_flSurvivalStartTime = 0;

	m_vecPlayAreaMins = vec3_origin;
	m_vecPlayAreaMaxs = vec3_origin;

	for ( int i = 0; i < SURVIVAL_GAME_RULES_DECISION_TYPES_NETWORK_TOTAL_MAX; ++ i )
	{
		m_SurvivalGameRuleDecisionTypes.Set( i, k_ESurvivalGameRuleDecision_Unknown );
		m_SurvivalGameRuleDecisionValues.Set( i, 0 );
	}

#ifdef GAME_DLL
	bool bServerIsLoadingSurvival = ( g_pGameTypes->GetCurrentGameType() == CS_GameType_FreeForAll ) &&
		( g_pGameTypes->GetCurrentGameMode() == CS_GameMode::FreeForAll_Survival );
	if ( !bServerIsLoadingSurvival )
		return;

	ReloadConfigFile();

	// Set up valid area on map

	// Initialize teams if we are queued
	if ( CSGameRules()->IsQueuedMatchmaking() )
	{
		InitializeTeamsFromMatchmakingReservation();
	}
#endif
}

CSurvivalGameRules::~CSurvivalGameRules()
{
#ifdef GAME_DLL
	CBrConfig::Free( m_pBrConfig );
	m_pBrConfig = NULL;
#endif
}


bool CSurvivalGameRules::IsPlayingSoloMode( void )
{
	return (sv_dz_team_count.GetInt() <= 1);
}

bool CSurvivalGameRules::IsPlayingTeamMode( void )
{
	return (!IsPlayingSoloMode());
}

IMPLEMENT_NETWORKCLASS_ALIASED( SurvivalSpawnChopper, DT_SurvivalSpawnChopper )
LINK_ENTITY_TO_CLASS_ALIASED( survival_spawn_chopper, SurvivalSpawnChopper );
BEGIN_NETWORK_TABLE( CSurvivalSpawnChopper, DT_SurvivalSpawnChopper )
// MAD HACK TO ALLOW THINGS TO GO OUTSIDE MAP BOUNDS
#ifndef CLIENT_DLL
SendPropExclude( "DT_BaseEntity", "m_vecOrigin" ),
SendPropExclude( "DT_BaseEntity", "m_cellbits" ),
SendPropExclude( "DT_BaseEntity", "m_cellX" ),
SendPropExclude( "DT_BaseEntity", "m_cellY" ),
SendPropExclude( "DT_BaseEntity", "m_cellZ" ),
SendPropVectorXY( SENDINFO( m_vecOrigin ), -1, SPROP_NOSCALE | SPROP_CHANGES_OFTEN, 0.0f, HIGH_DEFAULT, SendProxy_OriginXY ),
SendPropFloat( SENDINFO_VECTORELEM( m_vecOrigin, 2 ), -1, SPROP_NOSCALE | SPROP_CHANGES_OFTEN, 0.0f, HIGH_DEFAULT, SendProxy_OriginZ ),
#else
RecvPropVectorXY( RECVINFO_NAME( m_vecNetworkOrigin, m_vecOrigin ) ),
RecvPropFloat( RECVINFO_NAME( m_vecNetworkOrigin[2], m_vecOrigin[2] ) ),
#endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
void CSurvivalSpawnChopper::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		RenderPostSkyboxPreMainScene( true ); // draw after the skybox but before the main scene in a special pass
	}

	SetAllowFastPath( false ); // so it can opt out of rendering in the CSM depth pass
}

int	CSurvivalSpawnChopper::DrawModel( int flags, const RenderableInstance_t &instance )
{
	// the spawn chopper's CSM shadows are strobe-y and distracting. Disabled here:
	if ( flags & DF_SHADOW_DEPTH_MAP )
		return 0;

	return BaseClass::DrawModel( flags, instance );
}

#endif

#ifndef CLIENT_DLL

Vector CSurvivalSpawnChopper::ComputeFlightPositionForTime( float flTime, float flSpeed )
{
	CSurvivalGameRules* pBRrules = CSGameRules()->GetSurvivalRules();
	AABB_t playArea = pBRrules->GetPlayAreaBounds();

	float flTimeInAir = flTime - m_flSpawnTimeStamp;
	float flSupposedDistanceTraveled = flTimeInAir * flSpeed;

	float flRadiusX = 0.9f * (playArea.m_vMaxBounds.x - playArea.m_vMinBounds.x) / 2.0f;
	float flRadiusY = 0.9f * (playArea.m_vMaxBounds.y - playArea.m_vMinBounds.y) / 2.0f;

	float flRadianAngle = m_flFlightPathRotationOffset + (flSupposedDistanceTraveled / flRadiusX);

	float flX = cos( flRadianAngle ) * flRadiusX;
	float flY = sin( flRadianAngle ) * flRadiusY;
	float flZ = m_bCircling ? playArea.m_vMaxBounds.z / 3.0f : playArea.m_vMaxBounds.z;

	Vector vecTarget = Vector( m_bFlipX ? -flX : flX, m_bFlipY ? -flY : flY, flZ );

	//debugoverlay->AddBoxOverlay( vecTarget, Vector( -10, -10, -10 ), Vector( 10, 10, 10 ), vec3_angle, 255, 0, 0, 0, 60.0f );
	return vecTarget;
}

void CSurvivalSpawnChopper::Precache( void )
{
	BaseClass::Precache();
	PrecacheModel( HELICOPTER_MODEL_CARGO );
	PrecacheModel( HELICOPTER_MODEL_BLACKHAWK );
	PrecacheScriptSound( "helicopter_main" );
}

void CSurvivalSpawnChopper::Spawn( void )
{
	BaseClass::Spawn();

	m_bCircling = true;

	m_flFlightPathRotationOffset = RandomFloat( 0, 2.0f * M_PI );
	m_bFlipX = RandomInt( 0, 1 ) == 0;
	m_bFlipY = RandomInt( 0, 1 ) == 0;

	m_flSpawnTimeStamp = gpGlobals->curtime;

	CSurvivalGameRules* pBRrules = CSGameRules()->GetSurvivalRules();
	if ( !pBRrules )
	{
		Assert( false );
		UTIL_Remove( this );
		return;
	}

	SetModel( HELICOPTER_MODEL_BLACKHAWK );

	SetSequence( LookupSequence( GetModelPtr(), "fly_generic" ) );
	SetCycle( 0 );
	SetPlaybackRate( 1.0f );
	ResetSequenceInfo();

	SetMoveType( MOVETYPE_NOCLIP ); // we don't brake for nobody

	Vector vecSpawnPos = ComputeFlightPositionForTime( gpGlobals->curtime, dev_dz_spawn_chopper_speed.GetFloat() );

	SetOutOfBoundsLocalOriginAllowed( true );

	SetAbsOrigin( vecSpawnPos );

	CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
	CReliableBroadcastRecipientFilter filter;
	m_pSoundLoop = controller.SoundCreate( filter, entindex(), "helicopter_main" );
	controller.Play( m_pSoundLoop, 0.3f, PITCH_NORM );

	SetThink( &CSurvivalSpawnChopper::ChopperFly );
	SetNextThink( gpGlobals->curtime );
}

void CSurvivalSpawnChopper::UpdateOnRemove( void )
{
	BaseClass::UpdateOnRemove();

	if ( m_pSoundLoop )
	{
		CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
		controller.SoundDestroy( m_pSoundLoop );
	}
}

void CSurvivalSpawnChopper::ChopperFly( void )
{
	CSurvivalGameRules* pBRrules = CSGameRules()->GetSurvivalRules();
	if ( !pBRrules )
	{
		Assert( false );
		UTIL_Remove( this );
		return;
	}

	if ( m_bCircling )
	{
		Vector vecPosCurrent = ComputeFlightPositionForTime( gpGlobals->curtime, dev_dz_spawn_chopper_speed.GetFloat() );
		SetAbsVelocity( (vecPosCurrent - GetAbsOrigin()).Normalized() * dev_dz_spawn_chopper_speed.GetFloat() );

		Vector vecPosPrevious = ComputeFlightPositionForTime( gpGlobals->curtime - 2.0f, dev_dz_spawn_chopper_speed.GetFloat() );
		Vector vecPosFuture = ComputeFlightPositionForTime( gpGlobals->curtime + 5.0f, dev_dz_spawn_chopper_speed.GetFloat() );

		QAngle angTemp;
		VectorAngles( (vecPosFuture - vecPosPrevious).Normalized(), angTemp );
		angTemp[PITCH] = 0;
		angTemp[ROLL] = 0;
		SetAbsAngles( angTemp );
	}
	else
	{
		AABB_t playArea = pBRrules->GetPlayAreaBounds();
		playArea.m_vMinBounds.z = 0;

		if ( !playArea.ContainsPoint2D( GetAbsOrigin() * 0.5f ) )
		{
			UTIL_Remove( this );
			return;
		}
	}

	SetNextThink( gpGlobals->curtime );

	StudioFrameAdvance();
}

void CSurvivalSpawnChopper::AddPassenger( CBasePlayer* pPlayer )
{
	if ( !pPlayer || !pPlayer->IsAlive() )
		return;

	if ( CCSPlayer *pCSPlayer = ToCSPlayer( pPlayer ) )
	{
		pCSPlayer->EnableTrackingDistanceTraveled( false );
	}
		
	pPlayer->SetAbsVelocity( vec3_origin );
	pPlayer->SetMoveType( MOVETYPE_NONE );
	pPlayer->AddSolidFlags( FSOLID_NOT_SOLID );

	Vector vecTelePos = GetAbsOrigin();
	QAngle angTeleAng = GetAbsAngles();
	pPlayer->Teleport( &vecTelePos, &angTeleAng, &vec3_origin );

	pPlayer->SetParent( this );
	pPlayer->SetCollisionGroup( COLLISION_GROUP_IN_VEHICLE );

	pPlayer->ViewPunchReset();

	QAngle angTemp;
	VectorAngles( -pPlayer->GetAbsOrigin().Normalized(), angTemp );
	QAngle angEyeAngles = pPlayer->EyeAngles();
	angEyeAngles[YAW] = angTemp[YAW];
	angEyeAngles[PITCH] = MAX( angTemp[PITCH], 0 ); // don't look up, only down
	pPlayer->SnapEyeAngles( angEyeAngles );

	pPlayer->m_Local.m_iHideHUD |= HIDEHUD_PLAYERDEAD;

	// fade in from blue-white
	color32_s clr = { 200, 230, 255, 255 };
	UTIL_ScreenFade( pPlayer, clr, 0.6f, 0.6f, FFADE_IN | FFADE_PURGE | FFADE_SOFTCURVE );
}

void CSurvivalGameRules::AddToSpawnChopper( CBasePlayer *pPlayer )
{
	if ( !pPlayer || !pPlayer->IsAlive() )
		return;

	// send message to client to make them bring up spawn select ui
	{
		CSingleUserRecipientFilter filter( pPlayer );
		filter.MakeReliable();

		CCSUsrMsg_SSUI msg;
		msg.set_show( true );
		SendUserMessage( filter, CS_UM_SSUI, msg );
	}

	CSurvivalSpawnChopper *pSpawnChopper = dynamic_cast<CSurvivalSpawnChopper*>(m_hSpawnChopper.Get());
	if ( !pSpawnChopper )
	{
		pSpawnChopper = dynamic_cast<CSurvivalSpawnChopper *>(CreateEntityByName( "survival_spawn_chopper" ));
		if ( pSpawnChopper )
		{
			DispatchSpawn( pSpawnChopper );
			m_hSpawnChopper = pSpawnChopper;
		}
	}

	pSpawnChopper->AddPassenger( pPlayer );
}

#endif // GAME_DLL

void CSurvivalGameRules::GetWorldWidthAndHeight( float &flWidth, float &flHeight, Vector &vecWorldCenter )
{
	Vector vMin, vMax;
	CWorld *pWorld = CWorld::GetWorldEntity();
	if ( !pWorld )
	{
		flWidth = 1.0f;
		flHeight = 1.0f;
		vecWorldCenter = vec3_origin;
		return;
	}

	pWorld->GetWorldBounds( vMin, vMax );
	flWidth = fabsf( vMax.x - vMin.x );
	flHeight = fabsf( vMax.y - vMin.y );
	vecWorldCenter = ( vMin + vMax ) * 0.5f;
}

int CSurvivalGameRules::FindGridCenterIndexClosestToWorldPos( const Vector& vecWorldPos, int *pHighResPlayerLocation /*= NULL*/ )
{
	Vector2D vecClosestGridCenter;
	const Vector2D vecWorldPos2D = vecWorldPos.AsVector2D();
	
	Vector2D vecHexPos = vecWorldPos2D - Vector2D( m_flTabletHexOriginX, m_flTabletHexOriginY );
	CHexCoordF hc_hires = CHexCoordF( vecHexPos, m_flTabletHexSize );

	CHexCoord hc = hc_hires.Round();
	int x, y;
	hc.ToOffset( x, y );

	if ( x < 0 || x >= WORLD_HEX_WIDTH || y < 0 || y >= WORLD_HEX_HEIGHT )
	{
		x = Clamp( x, 0, WORLD_HEX_WIDTH - 1 );
		y = Clamp( y, 0, WORLD_HEX_HEIGHT - 1 );
		hc = CHexCoord::FromOffset( x, y );
	}

	if ( pHighResPlayerLocation )
	{
		CHexCoordF fromCenter = hc_hires - hc;
		*pHighResPlayerLocation = fromCenter.NearestEdgeIndex();
	}

	return y * WORLD_HEX_WIDTH + x;
}

struct IndexDist_t
{
	int m_nIdx;
	float m_flDist;
	IndexDist_t( int nIdx, float flDist )
	{
		m_nIdx = nIdx;
		m_flDist = flDist;
	}
};

void CSurvivalGameRules::FindSortedGridCenterIndicesClosestToPos( const Vector& vecWorldPos, CUtlVector<int>& vecIndicesOut )
{
	const int nHexes = WORLD_HEX_NUM;
	const Vector2D vecWorldPos2D = vecWorldPos.AsVector2D();

	CUtlVector<IndexDist_t> vecDistIndices;
	for ( int i = 0; i < nHexes; i++ )
	{
		const Vector2D& vecGridCenter = GetHexCenter( i );
		float flDist = vecWorldPos2D.DistToSqr( vecGridCenter );
		int nInsertIdx = 0;
		FOR_EACH_VEC( vecDistIndices, n )
		{
			nInsertIdx = n;
			if ( vecDistIndices[n].m_flDist > flDist )
				break;
		}
		vecDistIndices.InsertBefore( nInsertIdx, IndexDist_t( i, flDist ) );
	}

	FOR_EACH_VEC( vecDistIndices, n )
	{
		vecIndicesOut.AddToTail( vecDistIndices[n].m_nIdx );
	}
}

Vector2D CSurvivalGameRules::ComputeGridCenterFromGridIndex( int nIndex )
{
	int nStride = WORLD_HEX_WIDTH;
	int nX = nIndex % nStride;
	int nY = nIndex / nStride;
	CHexCoord hc = CHexCoord::FromOffset( nX, nY );

	Vector2D point = hc.ToPoint( m_flTabletHexSize );
	point.x += m_flTabletHexOriginX;
	point.y += m_flTabletHexOriginY;

	return point;
}

uint64 CSurvivalGameRules::GetPlayerXuid( int playerEntIndex )
{
	if ( playerEntIndex < 1 || playerEntIndex >= MAX_PLAYERS )
		return 0;

	return m_roundData_playerXuids[playerEntIndex];
}

int CSurvivalGameRules::GetPlayerPosition( int playerEntIndex )
{
	if ( playerEntIndex < 1 || playerEntIndex >= MAX_PLAYERS )
		return 0;

	return m_roundData_playerPositions[playerEntIndex];
}

int CSurvivalGameRules::GetSurvivalPositionByAccountID( AccountID_t unAccountID )
{
	for ( int i = 1; i < MAX_PLAYERS; ++ i )
	{
		CSteamID steamID( m_roundData_playerXuids[i] );
		if ( steamID.IsValid() && steamID.BIndividualAccount() && steamID.GetAccountID() == unAccountID )
			return m_roundData_playerPositions[i];
	}
	return -1;
}


int CSurvivalGameRules::GetPlayerTeamIndex( int playerEntIndex )
{
	if ( playerEntIndex < 1 || playerEntIndex >= MAX_PLAYERS )
		return -2;

	return m_roundData_playerTeams[playerEntIndex];
}

CBaseEntity *CSurvivalGameRules::GetNearestC4Target( const Vector &vecPosition ) const
{
	//fixme: this doesn't take c4 target radius into account, so potentially a farther target might have a closer border

	float flMinDist = FLT_MAX;
	CBaseEntity *pNearestTarget = NULL;
	FOR_EACH_VEC( IBRC4Target::AutoList(), i )
	{
		CBRC4Target *pTarget = static_cast<CBRC4Target*>(IBRC4Target::AutoList()[i]);
		if ( pTarget->m_bBrokenOpen )
			continue;

		const Vector& vecTargetPos = pTarget->GetAbsOrigin();
		float flDistSqr = vecPosition.DistToSqr( vecTargetPos );
		if ( flDistSqr < flMinDist )
		{
			pNearestTarget = pTarget->GetEntity();
			flMinDist = flDistSqr;
		}
	}

	return pNearestTarget;
}

bool CSurvivalGameRules::IsInRangeOfNearestC4Target( const Vector &vecPosition ) const
{
	CBaseEntity *pEnt = GetNearestC4Target( vecPosition );
	if ( !pEnt )
		return false;

	CBRC4Target *pTarget = assert_cast< CBRC4Target* >(pEnt);
	return pTarget->IsInRange( vecPosition );
}


void CSurvivalGameRules::GetPlayerTeammates( CCSPlayer* pPlayer, CUtlVector<CCSPlayer*>& teammates, bool bAliveTeammateOnly /*= false*/ )
{
	if ( !pPlayer || pPlayer->m_nSurvivalTeam < 0 )
		return;

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CCSPlayer* pOtherPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );
		if ( pOtherPlayer && pOtherPlayer != pPlayer && pPlayer->m_nSurvivalTeam == pOtherPlayer->m_nSurvivalTeam )
		{
			if ( bAliveTeammateOnly && !pOtherPlayer->IsAlive() )
				continue;

			teammates.AddToTail( pOtherPlayer );
		}
	}
}

int CSurvivalGameRules::GetSurvivalGameRuleDecisionValue( ESurvivalGameRuleDecision_t eType, int nDefault ) const
{
	for ( int i = 0; i < SURVIVAL_GAME_RULES_DECISION_TYPES_NETWORK_TOTAL_MAX; ++ i )
	{
		if ( m_SurvivalGameRuleDecisionTypes[i] == eType )
			return m_SurvivalGameRuleDecisionValues[i];
	}

	return nDefault;
}

// -------------------------

#ifdef CLIENT_DLL
#define CParadropChopper C_ParadropChopper
#endif

enum chopperType_t
{
	CHOPPER_TYPE_PARADROP = 0,
	CHOPPER_TYPE_RAPPEL,
};

class CParadropChopper : public CBaseAnimating
{
	DECLARE_CLASS( CParadropChopper, CBaseAnimating );
	DECLARE_NETWORKCLASS();

public:

	CParadropChopper();

#ifdef CLIENT_DLL
	virtual void OnDataChanged( DataUpdateType_t updateType );
	virtual bool ShouldRegenerateOriginFromCellBits() const { return false; }
	virtual void PostBuildTransformations( CStudioHdr *pStudioHdr, BoneVector *pos, BoneQuaternion q[] ) OVERRIDE;
	virtual void GetRenderBounds( Vector& theMins, Vector& theMaxs );
	Vector m_vecLastRopeTargetPos;
	float m_flLastRopeTime;
#endif

#ifndef CLIENT_DLL

	virtual void Precache( void );
	virtual void Spawn( void );
	virtual void UpdateOnRemove( void );

	void ChopperFly( void );
	void ChopperFadeOut( void );
	void SetCallingPlayer( CBasePlayer *pPlayer, bool bCalledByPlayer ) { m_hCallingPlayer = pPlayer; m_bCalledByPlayer = bCalledByPlayer; }
	void SetDropPos( const Vector& vecPos ) { m_vecDropPos = vecPos; }
	void SetFlightTime( float flFlightTime ) { m_flFlightTime = flFlightTime; }

	virtual int UpdateTransmitState() { return SetTransmitState( FL_EDICT_ALWAYS ); }

	chopperType_t m_nChopperType;

private:
	Vector m_vecDropPos;
	float m_flLastDistFromDrop;
	bool m_bDroppedParadrop;
	CSoundPatch	*m_pSoundLoop;
	float m_flSpawnTime;
	float m_flFlightTime;
	bool m_bPlayerIsFinishedRappelling;
	float m_flPlayerFinishedRappellingTime;

#endif

	CNetworkVar( EHANDLE, m_hCallingPlayer );
	bool m_bCalledByPlayer;

};

IMPLEMENT_NETWORKCLASS_ALIASED( ParadropChopper, DT_ParadropChopper )
LINK_ENTITY_TO_CLASS_ALIASED( paradrop_chopper, ParadropChopper );
BEGIN_NETWORK_TABLE( CParadropChopper, DT_ParadropChopper )
// MAD HACK TO ALLOW THINGS TO GO OUTSIDE MAP BOUNDS
#ifndef CLIENT_DLL
SendPropExclude( "DT_BaseEntity", "m_vecOrigin" ),
SendPropExclude( "DT_BaseEntity", "m_cellbits" ),
SendPropExclude( "DT_BaseEntity", "m_cellX" ),
SendPropExclude( "DT_BaseEntity", "m_cellY" ),
SendPropExclude( "DT_BaseEntity", "m_cellZ" ),
SendPropVectorXY( SENDINFO( m_vecOrigin ), -1, SPROP_NOSCALE | SPROP_CHANGES_OFTEN, 0.0f, HIGH_DEFAULT, SendProxy_OriginXY ),
SendPropFloat( SENDINFO_VECTORELEM( m_vecOrigin, 2 ), -1, SPROP_NOSCALE | SPROP_CHANGES_OFTEN, 0.0f, HIGH_DEFAULT, SendProxy_OriginZ ),
SendPropEHandle( SENDINFO( m_hCallingPlayer ) ),
#else
RecvPropVectorXY( RECVINFO_NAME( m_vecNetworkOrigin, m_vecOrigin ) ),
RecvPropFloat( RECVINFO_NAME( m_vecNetworkOrigin[2], m_vecOrigin[2] ) ),
RecvPropEHandle( RECVINFO( m_hCallingPlayer ) ),
#endif
END_NETWORK_TABLE()

CParadropChopper::CParadropChopper()
{
#ifndef CLIENT_DLL
	m_nChopperType = CHOPPER_TYPE_PARADROP;
	m_flFlightTime = dev_dz_chopper_arrive_time.GetFloat();
#else
	m_vecLastRopeTargetPos = vec3_origin;
	m_flLastRopeTime = 0.0f;
#endif
	m_bCalledByPlayer = false;
}

#ifdef CLIENT_DLL
void CParadropChopper::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		RenderPostSkyboxPreMainScene( true ); // draw after the skybox but before the main scene in a special pass
	}
}

void CParadropChopper::GetRenderBounds( Vector& theMins, Vector& theMaxs )
{
	BaseClass::GetRenderBounds( theMins, theMaxs );

	// include a rappelling player if necessary
	CCSPlayer *pTargetPlayer = ToCSPlayer( m_hCallingPlayer.Get() );
	if ( pTargetPlayer && pTargetPlayer->IsAlive() && pTargetPlayer->m_bIsSpawnRappelling && pTargetPlayer->ShouldDraw() )
	{
		theMins.z += GetAbsOrigin().z;
		theMins.z = MIN( theMins.z, pTargetPlayer->GetAbsOrigin().z );
		theMins.z -= GetAbsOrigin().z;

		//ClientLeafSystem()->RenderableChanged( GetRenderHandle() );
	}
}

void CParadropChopper::PostBuildTransformations( CStudioHdr *pStudioHdr, BoneVector *pos, BoneQuaternion q[] )
{
	BaseClass::PostBuildTransformations( pStudioHdr, pos, q );

	C_BaseAnimating *pAnimating = GetBaseAnimating();
	if ( pAnimating )
	{
		CCSPlayer *pTargetPlayer = ToCSPlayer( m_hCallingPlayer.Get() );
		if ( pTargetPlayer && pTargetPlayer->IsAlive() && pTargetPlayer->m_bIsSpawnRappelling && pTargetPlayer->ShouldDraw() )
		{
			int oldWritableBones = m_BoneAccessor.GetReadableBones();
			m_BoneAccessor.SetWritableBones( BONE_USED_BY_ANYTHING );

			int nBoneA = pTargetPlayer->LookupBone( "weapon_hand_L" ); // left hand
			if ( nBoneA >= 0 )
			{
				Vector vecTemp;
				pTargetPlayer->GetBonePosition( nBoneA, vecTemp );
				m_BoneAccessor.GetBoneForWrite( LookupBone( "rappel_rope_a" ) ).SetOrigin( vecTemp );

				m_vecLastRopeTargetPos = vecTemp;
				m_flLastRopeTime = gpGlobals->curtime;
			}

			int nBoneB = pTargetPlayer->LookupBone( "ball_R" ); // right foot
			if ( nBoneB >= 0 )
			{
				Vector vecTemp;
				pTargetPlayer->GetBonePosition( nBoneB, vecTemp );
				m_BoneAccessor.GetBoneForWrite( LookupBone( "rappel_rope_b" ) ).SetOrigin( vecTemp );
			}

			m_BoneAccessor.SetWritableBones( oldWritableBones );

			ClientLeafSystem()->RenderableChanged( GetRenderHandle() );
		}
		else if ( m_flLastRopeTime != 0 ) // did we ever modify the rope?
		{
			float flRopeLerp = RemapValClamped( gpGlobals->curtime, m_flLastRopeTime, m_flLastRopeTime + 8.0f, 1.0f, 0.0f );
			if ( flRopeLerp > 0.0f ) // retracting the rope
			{
				flRopeLerp = Gain( flRopeLerp, 0.8f );

				int oldWritableBones = m_BoneAccessor.GetReadableBones();
				m_BoneAccessor.SetWritableBones( BONE_USED_BY_ANYTHING );

				int nBoneB = LookupBone( "rappel_rope_b" );
				Vector vecRetracted = m_BoneAccessor.GetBone( nBoneB ).GetOrigin();
				m_BoneAccessor.GetBoneForWrite( nBoneB ).SetOrigin( Lerp( flRopeLerp, vecRetracted, m_vecLastRopeTargetPos ) ); // slerp up the rope

				m_BoneAccessor.SetWritableBones( oldWritableBones );
			}
		}
	}
}

#endif // CLIENT_DLL

#ifndef CLIENT_DLL

void CParadropChopper::Precache( void )
{
	BaseClass::Precache();
	PrecacheModel( HELICOPTER_MODEL_CARGO );
	PrecacheModel( HELICOPTER_MODEL_BLACKHAWK );
	PrecacheScriptSound( "helicopter_main" );
}

void CParadropChopper::Spawn( void )
{
	BaseClass::Spawn();

	CSurvivalGameRules* pBRrules = CSGameRules()->GetSurvivalRules();
	if ( !pBRrules )
	{
		Assert( false );
		UTIL_Remove( this );
		return;
	}

	if ( m_nChopperType == CHOPPER_TYPE_PARADROP )
	{
		SetModel( HELICOPTER_MODEL_CARGO );
	}
	else
	{
		SetModel( HELICOPTER_MODEL_BLACKHAWK );
	}

	SetSequence( LookupSequence( GetModelPtr(), "fly_generic" ) );
	SetCycle( 0 );
	SetPlaybackRate( 1.0f );
	ResetSequenceInfo();

	SetMoveType( MOVETYPE_NOCLIP ); // we don't brake for nobody
	SetOutOfBoundsLocalOriginAllowed( true );

	if ( m_nChopperType == CHOPPER_TYPE_PARADROP )
	{
		AABB_t playArea = pBRrules->GetPlayAreaBounds();

		m_vecDropPos.z = playArea.m_vMaxBounds.z;

		trace_t tr;
		UTIL_TraceLine( m_vecDropPos, m_vecDropPos + RandomVector( -300, 300 ), MASK_SOLID, NULL, COLLISION_GROUP_NONE, &tr );
		if ( !tr.DidHit() )
			m_vecDropPos = tr.endpos;

		m_vecDropPos.z = playArea.m_vMaxBounds.z;

		// pick a random point on the outskirts of the map to spawn the chopper
		Vector vecSpawnPos = Vector( RandomFloat( -1, 1 ), RandomFloat( -1, 1 ), 0 ).Normalized() * playArea.LengthOfLargestDimension();
		vecSpawnPos.z = playArea.m_vMaxBounds.z;
		SetAbsOrigin( vecSpawnPos );

		Vector vecSpawnToDrop = m_vecDropPos - vecSpawnPos;

		QAngle angTemp;
		VectorAngles( vecSpawnToDrop.Normalized(), angTemp );
		SetAbsAngles( angTemp );

		float flDesiredVelocity = ( vecSpawnToDrop.Length() / m_flFlightTime ); // we want to reach the drop position in dev_dz_chopper_arrive_time seconds
		SetAbsVelocity( vecSpawnToDrop.Normalized() * flDesiredVelocity );
#ifdef _DEBUG
		DevMsg( "PARADROP: Chopper %p spawned at %.1f velocity %.1f\n", this, gpGlobals->curtime, flDesiredVelocity );
#endif

		m_flLastDistFromDrop = FLT_MAX;
		m_bDroppedParadrop = false;
	}
	else if ( m_nChopperType == CHOPPER_TYPE_RAPPEL )
	{
		CCSPlayer *pTargetPlayer = ToCSPlayer( m_hCallingPlayer.Get() );
		if ( !pTargetPlayer || !pTargetPlayer->IsAlive() )
		{
			Assert( false );
			UTIL_Remove( this );
			return;
		}

		m_bPlayerIsFinishedRappelling = false;
		m_flPlayerFinishedRappellingTime = 0;

		Vector vecFromOrigin = pTargetPlayer->GetAbsOrigin();
		vecFromOrigin.z = 0;
		vecFromOrigin.NormalizeInPlace();

		QAngle angTemp;
		VectorAngles( vecFromOrigin, angTemp );
		SetAbsAngles( angTemp );
		SetAbsOrigin( pTargetPlayer->GetAbsOrigin() + Vector( 0, 0, 500 ) );

		SetAbsVelocity( vec3_origin );
	}
	else
	{
		Assert( false );
	}

	CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
	CReliableBroadcastRecipientFilter filter;
	m_pSoundLoop = controller.SoundCreate( filter, entindex(), "helicopter_main" );

	if ( m_nChopperType == CHOPPER_TYPE_RAPPEL )
	{
		controller.Play( m_pSoundLoop, 0.4f, PITCH_NORM );
	}
	else
	{
		controller.Play( m_pSoundLoop, 1.f, PITCH_NORM );
	}

	SetThink( &CParadropChopper::ChopperFly );
	SetNextThink( gpGlobals->curtime );

	SetRenderAlpha( 0 ); // it will fade in as it approaches
}

void CParadropChopper::UpdateOnRemove( void )
{
	BaseClass::UpdateOnRemove();

	if ( m_pSoundLoop )
	{
		CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
		controller.SoundDestroy( m_pSoundLoop );
	}
}

void CParadropChopper::ChopperFly( void )
{
	CSurvivalGameRules* pBRrules = CSGameRules()->GetSurvivalRules();
	if ( !pBRrules )
	{
		Assert( false );
		UTIL_Remove( this );
		return;
	}

	SetNextThink( gpGlobals->curtime );

	StudioFrameAdvance();

	byte a = GetRenderAlpha();
	if ( a < 255 )
		SetRenderAlpha( a + 1 );

	bool bDoFadeTest = true;

	if ( m_nChopperType == CHOPPER_TYPE_PARADROP && !m_bDroppedParadrop )
	{
		bDoFadeTest = false;

		float flDistFromDrop = GetAbsOrigin().DistTo( m_vecDropPos );
		float flDelta = m_flLastDistFromDrop - flDistFromDrop;
		m_flLastDistFromDrop = flDistFromDrop;

		if ( flDelta < 0 ) // as soon as we are moving away from the drop point
		{
			m_bDroppedParadrop = true;

			CBasePlayer *pPlayer = ToBasePlayer( m_hCallingPlayer.Get() );
			
			// drop paradrop
			CUtlVector< CBaseEntity* > vecItems;
			pBRrules->SpawnItemOnEvent( m_vecDropPos, QAngle( 0, RandomFloat( 0, 360 ), 0 ), m_bCalledByPlayer ? "paradrop_crate_event" : "paradrop_random_crate_event", NULL, &vecItems );
			Assert( vecItems.Count() == 1 );

			if ( vecItems.Count() && vecItems[ 0 ] )
			{
				if ( !m_bCalledByPlayer )
				{
					// Game event notification for each paradrop
					UTIL_PushGlobalTabletNotification( TABLET_NOTIFICATION_PARADROPS_ALLOWED );

					if ( pBRrules->BFirstParadropParachutingDown() )
					{
						// First paradrop makes a "big deal" announcement
						UTIL_ClientPrintAll( HUD_PRINTCENTER, "#SFUI_SmokeBeaconReady_Others" );
						if ( IGameEvent *event = gameeventmanager->CreateEvent( "choppers_incoming_warning" ) )
						{
							event->SetBool( "global", true ); // don't really need the userid - the event itself triggers the sound
							gameeventmanager->FireEvent( event );
						}
					}
				}

				CPhysPropLootCrate *pCrate = assert_cast< CPhysPropLootCrate* >( vecItems[0] );
				if ( pCrate )
				{
					pCrate->SetOwnedByPlayer( m_bCalledByPlayer );
					pCrate->SetCrateOwner( pPlayer );
				}

				if ( IGameEvent *event = pPlayer ? gameeventmanager->CreateEvent( "smoke_beacon_paradrop" ) : NULL )
				{
					event->SetInt( "userid", pPlayer->GetUserID() );
					event->SetInt( "paradrop", vecItems[0]->entindex() );
					event->SetInt( "priority", 6 );
					gameeventmanager->FireEvent( event );
				}
			}
		}
		
	}
	else if ( m_nChopperType == CHOPPER_TYPE_RAPPEL )
	{
		if ( !m_bPlayerIsFinishedRappelling )
		{
			bDoFadeTest = false;

			CCSPlayer *pTargetPlayer = ToCSPlayer( m_hCallingPlayer.Get() );
			if ( !pTargetPlayer || !pTargetPlayer->IsAlive() || !pTargetPlayer->m_bIsSpawnRappelling )
			{
				m_bPlayerIsFinishedRappelling = true;
				m_flPlayerFinishedRappellingTime = gpGlobals->curtime;
			}
		}
		else
		{
			Vector vecVel = GetAbsOrigin();
			vecVel.z = 0;
			vecVel.NormalizeInPlace();

			SetAbsVelocity( vecVel * RemapValClamped( gpGlobals->curtime, m_flPlayerFinishedRappellingTime, m_flPlayerFinishedRappellingTime + 10.0f, 0.0f, 1000.0f ) );
		}
	}

	if ( bDoFadeTest )
	{
		AABB_t playArea = pBRrules->GetPlayAreaBounds();
		if ( GetAbsOrigin().Length() > playArea.LengthOfLargestDimension() * 1.5f )
		{
			SetThink( &CParadropChopper::ChopperFadeOut );
			SetNextThink( gpGlobals->curtime );
			return;
		}
	}
}

void CParadropChopper::ChopperFadeOut( void )
{
	SetNextThink( gpGlobals->curtime );

	StudioFrameAdvance();

	byte a = GetRenderAlpha();
	if ( a > 0 )
	{
		SetRenderAlpha( a - 1 );
#ifdef _DEBUG
		if ( a == 1 )
		{
			DevMsg( "PARADROP: Chopper %p fading out at %.1f\n", this, gpGlobals->curtime );
		}
#endif
	}

	if ( !a )
	{
		UTIL_Remove( this );
	}
}

void CSurvivalGameRules::DispatchParadropChopperTo( Vector vecDestinationDropPos, CBasePlayer *pCallingPlayer, bool bCalledByPlayer, float flFlightTime )
{
	CParadropChopper *pChopper = dynamic_cast<CParadropChopper *>(CreateEntityByName( "paradrop_chopper" ));
	Assert( pChopper );
	if ( pChopper )
	{
		pChopper->SetCallingPlayer( pCallingPlayer, bCalledByPlayer );
		pChopper->SetDropPos( vecDestinationDropPos );
		if ( flFlightTime > 0 )
			pChopper->SetFlightTime( flFlightTime );
		DispatchSpawn( pChopper );
	}

	m_flTimeOfLastParadrop = gpGlobals->curtime;
}

#endif

#ifdef CLIENT_DLL
ConVar cl_rappel_tilt( "cl_rappel_tilt", "0.35", FCVAR_RELEASE );
void CSurvivalGameRules::CalcCustomBRView( CBasePlayer *pPlayer, Vector &eyeOrigin, QAngle &eyeAngles, float &zNear, float &zFar, float &fov )
{

	if ( !pPlayer )
		return;

	if ( pPlayer->GetMoveParent() )
	{
		C_BaseAnimating *pParentAnimating = pPlayer->GetMoveParent()->GetBaseAnimating();
		if ( pParentAnimating )
		{
			eyeAngles[YAW] -= pParentAnimating->GetAbsAngles()[YAW];

			Vector vecEyeForward;
			AngleVectors( eyeAngles, &vecEyeForward );
			Vector vecHeliCamPos = pParentAnimating->GetAbsOrigin() - (dev_dz_spawn_camera_distance.GetFloat() * vecEyeForward);
			
			eyeAngles[YAW] -= dev_dz_spawn_camera_yaw.GetFloat() * 2.0f;
			eyeAngles[YAW] -= dev_dz_spawn_camera_yaw.GetFloat() * Gain( RemapValClamped( GetSpawnSelectTimeEnd() - gpGlobals->curtime, 5.0f, 0.0f, 1.0f, 0.0f ), 0.8f );;
			eyeOrigin = vecHeliCamPos;
			eyeOrigin.z -= dev_dz_spawn_camera_height.GetFloat();
		}
	}

	// tilt the player's view to make it seem more like swinging from a rope
	CCSPlayer *pCSPlayer = ToCSPlayer( pPlayer );
	if ( pCSPlayer )
	{
		pCSPlayer->m_flSpawnRappellingFadeOutForClientViewOffset = pCSPlayer->m_bIsSpawnRappelling ? 1.0f :
			Approach( 0.0f, pCSPlayer->m_flSpawnRappellingFadeOutForClientViewOffset, gpGlobals->frametime * 2.0f );

		if ( pCSPlayer->m_bIsSpawnRappelling || pCSPlayer->m_flSpawnRappellingFadeOutForClientViewOffset > 0.0f )
		{
			float flSpawnRappelFadeOut = Gain( pCSPlayer->m_flSpawnRappellingFadeOutForClientViewOffset, 0.8f );

			Vector vecEyeToRappellingOrigin = (pCSPlayer->m_vecSpawnRappellingRopeOrigin.Get() - eyeOrigin);
			float flTiltScale = clamp( cl_rappel_tilt.GetFloat(), 0.0f, 1.0f );
			vecEyeToRappellingOrigin.x *= flTiltScale * flSpawnRappelFadeOut;
			vecEyeToRappellingOrigin.y *= flTiltScale * flSpawnRappelFadeOut;
			vecEyeToRappellingOrigin.NormalizeInPlace();

			VMatrix matRot;
			MatrixBuildRotationAboutAxis( matRot, CrossProduct( Vector( 0, 0, 1 ), vecEyeToRappellingOrigin ).Normalized(), acos( DotProduct( Vector( 0, 0, 1 ), vecEyeToRappellingOrigin ) ) * 180 / M_PI );

			matrix3x4_t matEye;
			AngleMatrix( eyeAngles, matEye );

			matrix3x4_t matEyeRotated;
			ConcatTransforms( matRot.As3x4(), matEye, matEyeRotated );

			MatrixAngles( matEyeRotated, eyeAngles );
		}
	}


#ifdef DEBUG
	if ( follow_drone.GetBool() )
	{
		FOR_EACH_VEC_BACK( IDrone::AutoList(), nDrone )
		{
			CDrone *pDrone = static_cast<CDrone*>(IDrone::AutoList()[nDrone]);
			if ( pDrone && !pDrone->IsDormant() )
			{
				if ( follow_drone.GetInt() > 1 && !pDrone->GetMoveToEnt() )
					continue;

				Vector vecEyeForward;
				AngleVectors( eyeAngles, &vecEyeForward );
				eyeOrigin = pDrone->GetAbsOrigin() - (200.0f * vecEyeForward);

				return;
			}
		}
	}
#endif

}

void CSurvivalGameRules::OnDataChanged( DataUpdateType_t updateType )
{
#ifdef PANORAMA_ENABLE
	panorama::DispatchEvent( SurvivalSpawnSelectUpdate(), ( panorama::IUIPanelClient* )nullptr );
#endif
}

static Color s_teamColors[] =
{
	Color( 86, 134, 255, 255 ),		// blue
	Color( 49, 255, 203, 255 ),		// aqua
	Color( 255, 253, 93, 255 ),		// yellow
	Color( 208,	46, 255, 255 ),		// magenta
	Color( 33, 200, 255, 255 ),		// ltblue
	Color( 60, 255, 45, 255 ),		// green
	Color( 255, 195, 12, 255 ),		// orange
	Color( 255, 124, 250, 255 ),		// pink
	Color( 152, 120, 255, 255 ),		// purple
	Color( 255, 255, 255, 255 ),	// white
	Color( 255, 119, 87, 255 ),	// peach
	Color( 186, 255, 179, 255 ),	// mint
	Color( 255, 0, 180, 255 ),	// fuscia
	Color( 0, 176, 112, 255 ),	// pine
	Color( 216, 255, 0, 255 ),	// lime
	Color( 255, 46, 76, 255 ),	// red
	Color( 141, 141, 141, 255 ),	// grey
};

COMPILE_TIME_ASSERT( V_ARRAYSIZE( s_szTeamColorLoc ) == V_ARRAYSIZE( s_teamColors ) );

// generic 'enemy red' for glows
static Color s_noteamColor = Color( 244, 117, 117, 255 );

Color CSurvivalGameRules::GetTeamColor( int iTeam )
{
	if ( iTeam < 0 )
		return s_noteamColor;
	
	const int iTeamColorCount = ARRAYSIZE( s_teamColors );
	Assert( iTeam  < iTeamColorCount ); // if you hit this, maybe we need to add more team colors
	int iColorIndex = iTeam % iTeamColorCount;
	return s_teamColors[iColorIndex];
}

Color CSurvivalGameRules::GetTeamColorByUserID( int iUserID )
{
	C_CSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByUserId( iUserID ) );
	if ( !pPlayer )
		return s_noteamColor;

	return GetTeamColor( pPlayer->m_nSurvivalTeam );
}

IMaterial *CSurvivalGameRules::GetGameRuleScreenOverlayOverride( void )
{
	if ( !CSGameRules() )
		return NULL;

	if ( CSGameRules()->IsWarmupPeriod() && !g_bEngineIsHLTV )
	{
		// fade out at warmup end
		float flWarmupEnd = CSGameRules()->GetWarmupPeriodEndTime(); // fixme: why is warmup end time wrong in demo playback?
		float flFadeOut = RemapValClamped( gpGlobals->curtime, flWarmupEnd - 3.0f, flWarmupEnd, 0.0f, 1.05f );
		
		if ( flFadeOut <= 0 )
			return NULL;

		IMaterial *pMat = materials->FindMaterial( "dev/hextransition", TEXTURE_GROUP_OTHER, true );
		if ( pMat )
		{
			IMaterialVar* pVar = pMat->FindVar( "$c0_w", NULL );
			pVar->SetFloatValue( flFadeOut );

			return pMat;
		}
	}

	return NULL;
}

#endif // CLIENT_DLL

#ifdef GAME_DLL
struct dz_player_index_position_t
{
	int m_nIndex;
	Vector m_vecPosition;
	float m_flDist;
};
int __cdecl BRPlayerSortDist( const dz_player_index_position_t *s1, const dz_player_index_position_t *s2 )
{
	return Sign(s1->m_flDist - s2->m_flDist);
}
#endif

AABB_t CSurvivalGameRules::GetPlayAreaBounds( void )
{
#ifdef GAME_DLL
	InitBounds();
#endif
	Assert( m_vecPlayAreaMins.Get() != vec3_origin );
	Assert( m_vecPlayAreaMaxs.Get() != vec3_origin );
	return AABB_t( m_vecPlayAreaMins.Get(), m_vecPlayAreaMaxs.Get() );
}

#ifdef GAME_DLL
void CSurvivalGameRules::InitBounds( void )
{
	EarlyOutIfNotPlayingSurvival;

	if ( m_bBoundsInitialized )
		return;

	m_bBoundsInitialized = true;

	CBaseEntity *pTriggerSurvivalPlayArea = gEntList.FindEntityByClassname( NULL, "trigger_survival_playarea" );
	Assert( pTriggerSurvivalPlayArea );
	if ( pTriggerSurvivalPlayArea )
	{
		pTriggerSurvivalPlayArea->CollisionProp()->WorldSpaceSurroundingBounds( &m_vecPlayAreaMins.GetForModify(), &m_vecPlayAreaMaxs.GetForModify() );
		DZ_ConsoleMsg( "Play area bounds: %.2f x %.2f\n", abs( GetPlayAreaBounds().GetSize().x ), abs( GetPlayAreaBounds().GetSize().y ) );
	}
}
#endif

#ifdef GAME_DLL

void CSurvivalGameRules::SpawnSurvivalPlayer( CCSPlayer * pPlayer )
{
	if ( !pPlayer || !pPlayer->IsAlive() )
		return;

	if ( CSGameRules()->IsWarmupPeriod() )
	{
		CBaseEntity *pSpot = (CSGameRules()->GetNextSpawnpoint( TEAM_TERRORIST ));
		Assert( pSpot );

		bool bGiveChute = false;

		Vector vecSpawnPos = vec3_origin;
		if ( pSpot && dev_dz_warmup_spawn_mode.GetInt() == 0 )
		{
			vecSpawnPos = pSpot->GetAbsOrigin();
			ValidateAndAdjustPlayerSpawnPosition( vecSpawnPos );
		}
		else
		{
			AABB_t playArea = GetPlayAreaBounds();
			vecSpawnPos = Vector( RandomInt( playArea.m_vMinBounds.x, playArea.m_vMaxBounds.x ), RandomInt( playArea.m_vMinBounds.y, playArea.m_vMaxBounds.y ), 0 );

			// use closest item spawn location to ensure players don't spawn over the middle of an ocean
			GetClosestItemSpawnPositionTo( vecSpawnPos, &vecSpawnPos, NULL );
			vecSpawnPos.z = playArea.m_vMaxBounds.z;
			bGiveChute = true;

			ValidateAndAdjustPlayerSpawnPosition( vecSpawnPos, dev_dz_spawn_height_ground_offset_warmup.GetFloat() );
		}

		QAngle angTemp;
		VectorAngles( -vecSpawnPos.Normalized(), angTemp ); // spawn warmup players looking in at map origin
		angTemp[PITCH] = 0;
		angTemp[ROLL] = 0;

		pPlayer->Teleport( &vecSpawnPos, &angTemp, &vec3_origin );
		pPlayer->m_Local.m_viewPunchAngle = vec3_angle;

		if ( pSpot && dev_dz_warmup_spawn_mode.GetInt() == 0 )
		{
			angTemp[PITCH] = 0.0f;
		}
		else
		{
			angTemp[PITCH] = 48.5f;
		}
		pPlayer->SnapEyeAngles( angTemp );

		CCSPlayer *pCSPlayer = ToCSPlayer( pPlayer );
		if ( pCSPlayer )
		{
			pCSPlayer->EnableTrackingDistanceTraveled( true );

			if ( bGiveChute )
			{
				pPlayer->SetGroundEntity( NULL );

				//pCSPlayer->m_bIsSpawnRappelling = true;
				//pCSPlayer->m_vecSpawnRappellingRopeOrigin = pCSPlayer->GetAbsOrigin() + Vector( 0, 0, 100 );
				//pCSPlayer->SetMaxFallVelocity( -180 );

				// players spawn in the air, so set them to fall slowly (they're parachuting)
				pCSPlayer->GiveParachute();
				pCSPlayer->DeployParachute( PLAYER_PARACHUTE_DEPLOY_REASON_SPAWN ); // deploy it right away, don't wait for fall velocity to trigger it
			}
		}

		if ( dev_dz_warmup_spawn_mode.GetInt() == 0 )
		{
			color32_s clr = { 0, 0, 0, 255 };
			UTIL_ScreenFade( pPlayer, clr, 1.0f, 0.5f, FFADE_IN | FFADE_PURGE | FFADE_SOFTCURVE );
		}
		else
		{
			// fade in from blue-white
			color32_s clr = { 200, 230, 255, 255 };
			UTIL_ScreenFade( pPlayer, clr, 1.0f, 0.5f, FFADE_IN | FFADE_PURGE | FFADE_SOFTCURVE );
		}

		// give us a weapon
		pPlayer->GiveNamedItem( sv_dz_warmup_weapon.GetString() );

		if ( sv_dz_warmup_tablet.GetBool() )
		{
			pPlayer->GiveNamedItem( "weapon_tablet" );
		}

		pPlayer->m_iAccount = 0; // no money in warmup thanks

	}
	else if ( pPlayer->State_Get() == STATE_ACTIVE )
	{
		AddToSpawnChopper( pPlayer );
	}
}

void CSurvivalGameRules::EquipSurvivalPlayer( CBasePlayer * pPlayer )
{
	Assert( pPlayer );

	pPlayer->GiveNamedItem( "weapon_fists" );

	pPlayer->SetHealth( sv_dz_player_spawn_health.GetInt() );
	pPlayer->SetMaxHealth( sv_dz_player_max_health.GetInt() );
	pPlayer->SetArmorValue( sv_dz_player_spawn_armor.GetInt() );

	CCSPlayer *pCSPlayer = ToCSPlayer( pPlayer );
	if ( pCSPlayer )
	{
		pCSPlayer->ResetAccount();
		pCSPlayer->AddAccount( mp_startmoney.GetInt(), true, "DZ Player Spawn" );
		//pCSPlayer->GiveDefuser( false );
		AssignTeam( pCSPlayer, false );
	}

	MDLCACHE_CRITICAL_SECTION(); // I hate this thing

	pPlayer->SetBodygroupPreset( "hide_vest" );
	pPlayer->SetBodygroupPreset( "hide_helmet" );
}

void CSurvivalGameRules::SpawnItemOnEvent( const Vector &vecPosition, const QAngle& angAngle, const char *pszEvent, CBasePlayer *pPlayer /*= NULL*/, CUtlVector< CBaseEntity * > *pOutputEnts /*= NULL*/ )
{
	const CBrConfig::Event_t *pEvent = m_pBrConfig ? m_pBrConfig->GetEvent( pszEvent ) : NULL;
	if ( pEvent )
	{
		return SpawnItemFromContent( vecPosition, angAngle, &pEvent->m_content, pPlayer, pOutputEnts );
	}
}

int CSurvivalGameRules::GetTeamCount( int nTeam )
{
	if ( nTeam < 0 )
		return 0;

	int teamcount = 0;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CCSPlayer* pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer )
			continue;

		if ( pPlayer->m_nSurvivalTeam == nTeam )
			++teamcount;
	}

	return teamcount;
}

void CSurvivalGameRules::SetTeam( CCSPlayer* pPlayer, int nTeam )
{
	// TODO: Add notifications for errors

	if ( nTeam < 0 )
		nTeam = -1; // no team

	// support up to 9 teams
	// >= 0 is required here because V_ARRAYSIZE comparison is done with unsigned integers
	if ( nTeam >= 0 && nTeam >= V_ARRAYSIZE( s_szTeamColorLoc ) )
		return;

	// no need to do anything if we aren't asking to change team
	if ( pPlayer->m_nSurvivalTeam == nTeam )
		return;

	// Assign teams based on steam id
	CSteamID steamId;
	uint32 nAccountId;
	if ( pPlayer->IsBot() )
	{
		// bots just use player index as account id
		nAccountId = pPlayer->entindex();
	}
	else if ( pPlayer->GetSteamID( &steamId ) )
	{
		nAccountId = steamId.GetAccountID();
	}
	else
	{
		AssertMsg1( false, "couldn't get steam id for %s\n", pPlayer->GetPlayerName() );
		return;
	}

	// clear old team assignment
	int iMapEntry = m_mapAccountIdToTeam.Find( nAccountId );
	if ( iMapEntry != m_mapAccountIdToTeam.InvalidIndex() )
		m_mapAccountIdToTeam.RemoveAt( iMapEntry );

	// add new team assignment
	m_mapAccountIdToTeam.Insert( nAccountId, nTeam );
	if ( nTeam >= 0 )
	{
		ClientPrint( pPlayer, HUD_PRINTTALK, "#Survival_Team_Joined", CFmtStr( "%d", nTeam + 1 ), s_szTeamColorLoc[nTeam] );
	}
	else
	{
		ClientPrint( pPlayer, HUD_PRINTTALK, "#Survival_Team_Left" );
	}

	// notify players of team change
	for ( int idxPlayer = 1; idxPlayer <= gpGlobals->maxClients; ++idxPlayer )
	{
		CCSPlayer* pTeammate = ToCSPlayer( UTIL_PlayerByIndex( idxPlayer ) );
		if ( !pTeammate || pTeammate == pPlayer )
			continue;

		if ( pTeammate->m_nSurvivalTeam >= 0 && pTeammate->m_nSurvivalTeam == pPlayer->m_nSurvivalTeam )
		{
			ClientPrint( pTeammate, HUD_PRINTTALK, "#Survival_Teammate_Left", CFmtStr( "#ENTNAME[%d]%s", pPlayer->entindex(), pPlayer->GetPlayerName() ) );
		}

		if ( pTeammate->m_nSurvivalTeam >= 0 && pTeammate->m_nSurvivalTeam == nTeam )
		{
			ClientPrint( pTeammate, HUD_PRINTTALK, "#Survival_Teammate_Joined", CFmtStr( "#ENTNAME[%d]%s", pPlayer->entindex(), pPlayer->GetPlayerName() ) );
			ClientPrint( pPlayer, HUD_PRINTTALK, "#Survival_Teammate_Joined", CFmtStr( "#ENTNAME[%d]%s", pTeammate->entindex(), pTeammate->GetPlayerName() ) );
		}
	}

	// assign team
	pPlayer->m_nSurvivalTeam = nTeam;
	if ( pPlayer->IsAlive() )
		m_roundData_playerTeams.Set( pPlayer->entindex(), pPlayer->m_nSurvivalTeam );
}

void CSurvivalGameRules::AssignTeam( CCSPlayer *pPlayer, bool bForceNewTeam )
{
	// default to 'solo' team
	pPlayer->m_nSurvivalTeam = -1;
	if ( pPlayer->IsAlive() )
		m_roundData_playerTeams.Set( pPlayer->entindex(), pPlayer->m_nSurvivalTeam );

	if ( sv_dz_team_count.GetInt() <= 1 )
	{
		// normal free-for-all mode
		return;
	}
	else if ( pPlayer->GetTeamNumber() != TEAM_TERRORIST )
	{
		// spectators don't get a team
		return;
	}

	uint32 nAccountId = 0;

	// Assign teams based on steam id
	CSteamID steamId;
	if ( pPlayer->IsBot() )
	{
		// bots just use player index as account id
		nAccountId = pPlayer->entindex();
	}
	else if ( pPlayer->GetSteamID( &steamId ) )
	{
		nAccountId = steamId.GetAccountID();
	}
	else
	{
		AssertMsg1( false, "couldn't get steam id for %s\n", pPlayer->GetPlayerName() );
		return;
	}

	int iMapEntry = m_mapAccountIdToTeam.Find( nAccountId );
	if ( iMapEntry == m_mapAccountIdToTeam.InvalidIndex() || bForceNewTeam )
	{
		if ( bForceNewTeam || sv_dz_autojointeam.GetBool() )
		{
			// Delete old team if any
			if ( iMapEntry != m_mapAccountIdToTeam.InvalidIndex() )
				m_mapAccountIdToTeam.RemoveAt( iMapEntry );

			// Give this player a new team
			if ( m_nPlayersOnNextTeam >= sv_dz_team_count.GetInt() )
			{
				// team is full, start a new team
				m_nPlayersOnNextTeam = 0;
				m_nTotalNumSurvivalTeams++;
			}

			int nPlayerTeam = m_nTotalNumSurvivalTeams;
			m_nPlayersOnNextTeam++;

			SetTeam( pPlayer, nPlayerTeam );
		}
	}
	else
	{
		// Player already assigned to a team -- make sure they are on that team
		pPlayer->m_nSurvivalTeam = m_mapAccountIdToTeam.Element( iMapEntry );
		if ( pPlayer->IsAlive() )
			m_roundData_playerTeams.Set( pPlayer->entindex(), pPlayer->m_nSurvivalTeam );
	}

}

void CSurvivalGameRules::ShuffleTeams()
{
	// Clear all team assignments
	m_mapAccountIdToTeam.Purge();
	m_nPlayersOnNextTeam = 0;
	m_nTotalNumSurvivalTeams = 0;

	CUtlVector<CCSPlayer*> vecPlayers;

	for ( int i = 0; i < gpGlobals->maxClients; ++i )
	{
		CCSPlayer* pPlayer = UTIL_ValidatePlayer( i, false, true, true, false );
		if ( !pPlayer )
			continue;

		pPlayer->m_nSurvivalTeam = -1;
		vecPlayers.AddToTail( pPlayer );
	}

	vecPlayers.Shuffle();

	FOR_EACH_VEC( vecPlayers, i )
	{
		AssignTeam( vecPlayers[i], true );
	}
}

CON_COMMAND_F( dz_shuffle_teams, "Shuffle all teams for Danger Zone", FCVAR_GAMEDLL )
{
	if ( !CSGameRules() || !CSGameRules()->GetSurvivalRules() )
		return;

	CSGameRules()->GetSurvivalRules()->ShuffleTeams();
}

CON_COMMAND_F( dz_jointeam, "dz_jointeam team# [userid#|name] - Join DZ team N (0 to leave your team).  Server admins can assign other players to teams.", FCVAR_GAMEDLL_FOR_REMOTE_CLIENTS )
{
	if ( args.ArgC() < 2 )
		return;

	// can't set team for other players if not a server admin
	if ( args.ArgC() >= 3 && !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	// only admins can set teams when jointeam is not enabled
	if ( !sv_dz_jointeam_allowed.GetBool() && !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	CCSPlayer* pPlayer = ToCSPlayer( UTIL_GetCommandClient() );
	if ( args.ArgC() >= 3 )
	{
		pPlayer = ToCSPlayer( UTIL_PlayerByUserId( V_atoi( args.Arg( 2 ) ) ) );
		if ( !pPlayer )
			pPlayer = ToCSPlayer( UTIL_PlayerByName( args.Arg( 2 ) ) );

		if ( !pPlayer )
			Msg( "No such player: %s\n", args.Arg( 2 ) );
	}

	if ( !pPlayer || pPlayer->GetTeamNumber() != TEAM_TERRORIST )
		return;

	if ( !CSGameRules() || !CSGameRules()->GetSurvivalRules() )
		return;

	if ( !CSGameRules()->IsWarmupPeriod() )
		return;

	if ( sv_dz_team_count.GetInt() <= 1 )
		return;

	int nTeam = V_atoi( args.Arg( 1 ) );

	// make input be 1-based (0 = none)
	nTeam--;
	if ( nTeam < 0 )
		nTeam = -1;

	// don't do anything if same team
	if ( pPlayer->m_nSurvivalTeam == nTeam )
		return;

	// can't join full team
	if ( nTeam >= 0 && CSGameRules()->GetSurvivalRules()->GetTeamCount( nTeam ) >= sv_dz_team_count.GetInt() )
		return;

	CSGameRules()->GetSurvivalRules()->SetTeam( pPlayer, nTeam );
}

CON_COMMAND_F( dz_clearteams, "Clear all DZ teams", FCVAR_GAMEDLL )
{
	if ( !CSGameRules() || !CSGameRules()->GetSurvivalRules() )
		return;

	if ( !CSGameRules()->IsWarmupPeriod() )
		return;

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CCSPlayer* pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer || pPlayer->GetTeamNumber() != TEAM_TERRORIST )
			continue;

		CSGameRules()->GetSurvivalRules()->SetTeam( pPlayer, -1 );
	}
}

void CSurvivalGameRules::InitializeTeamsFromMatchmakingReservation()
{
	/* Removed for partner depot */
}

void CSurvivalGameRules::SpawnItemFromContent( const Vector &vecPosition, const QAngle& angAngle, const CBrConfig::Content_t *pContent, CBasePlayer *pPlayer /*= NULL*/, CUtlVector< CBaseEntity * > *pOutputEnts /*= NULL*/ )
{
	if ( pContent->IsCrate() )
	{
		// spawn a crate
		CPhysPropLootCrate *pLootCrate = assert_cast<CPhysPropLootCrate*>( CreateEntityByName( pContent->m_pCrate->m_pszEntityName ) );
		if ( !pLootCrate )
		{
			return;
		}

		if ( !pLootCrate->SetupCrate( pContent->m_pCrate->m_pszName ) )	
		{
			UTIL_Remove( pLootCrate );
			return;
		}

		Vector vecSpawnPos = vecPosition;

		//debugoverlay->AddBoxOverlay( vecSpawnPos, Vector( -5, -5, -5 ), Vector( 5, 5, 5 ), angAngle, 0, 255, 0, 0, 10 );

		//pLootCrate->SetAbsAngles( angAngle );
		QAngle angTemp = QAngle( 0, angAngle[YAW], 0 ); // crates sometimes spawn on their side? Not sure if the incoming angle is good. Don't trust it - only use yaw.
		pLootCrate->SetAbsAngles( angTemp );

		pLootCrate->SetAbsOrigin( vecSpawnPos );
		
		DispatchSpawn( pLootCrate );
		
		//debugoverlay->AddBoxOverlay( pLootCrate->GetAbsOrigin(), Vector( -5, -5, -5 ), Vector( 5, 5, 5 ), pLootCrate->GetAbsAngles(), 255, 255, 0, 0, 10 );

		pLootCrate->SetHighlightColor( pContent->m_pCrate->m_cHighlightColor.r(), pContent->m_pCrate->m_cHighlightColor.g(), pContent->m_pCrate->m_cHighlightColor.b() );

		DZ_ConsoleMsg( "Spawned lootcrate (%s) at [%.1f,%.1f,%.1f]\n", pContent->m_pCrate->m_pszName, XYZ( vecSpawnPos ) );
		
		if ( pOutputEnts )
		{
			pOutputEnts->AddToTail( pLootCrate );
		}
	}
	else
	{
		SpawnItemFromLootList( vecPosition, angAngle, pContent->m_pLootList, pPlayer, pOutputEnts );
	}
}

void CSurvivalGameRules::SpawnItemFromLootList( const Vector &vecPosition, const QAngle& angAngle, const CBrConfig::LootList_t *pLootList, CBasePlayer *pPlayer /*= NULL*/, CUtlVector< CBaseEntity * > *pOutputEnts /*= NULL*/, CPhysPropLootCrate *pSrcCrate /*= NULL*/ )
{
	Assert( pLootList );
	if ( !pLootList )
		return;

	const CBrConfig::LootList_t::ItemList_t* pItemList = pLootList->RandomItemList();
	if ( pItemList )
	{
		FOR_EACH_VEC( pItemList->m_Items, iItem )
		{
			const auto &item = pItemList->m_Items[ iItem ];

			Vector vecTempPos = vecPosition;
			QAngle angTempAng = angAngle;

			if ( pSrcCrate )
			{
				pSrcCrate->GetSpawnPositionForItemNumber( iItem, pItemList->m_Items.Count(), vecTempPos, angTempAng );
			}

			const char *pszEntityName = item.m_pBaseItem->m_pszEntityName;
			int nAmmo = item.GetAmmo();

			if ( !V_strcmp( pszEntityName, "item_cash" ) )
			{
				UTIL_SpawnPhysicalCash( vecTempPos, 0, nAmmo, "lootbag", CASH_SPAWN_WITH_RANDOM_VELOCITY, pOutputEnts );
			}
			else
			{
				CBaseEntity *pEnt = SpawnItem( vecTempPos, angTempAng, pszEntityName, nAmmo, pPlayer );
				Assert( pEnt );

				if ( pEnt )
				{
					pEnt->m_flDamageForceOverrideStartTime = gpGlobals->curtime + 0.8f;
					pEnt->m_flDamageForceOverrideEndTime = gpGlobals->curtime + 1.4f;
					pEnt->m_flDamageForceOverrideMultiplier = 0.005f;

					CBaseAnimating *pEntAnim = pEnt->GetBaseAnimating();
					if ( pEntAnim )
						pEntAnim->SetHighlightColor( item.m_pBaseItem->m_cHighlightColor.r(), item.m_pBaseItem->m_cHighlightColor.g(), item.m_pBaseItem->m_cHighlightColor.b() );

					pEnt->Teleport( &vecTempPos, &angTempAng, NULL );
				}

				// add to output if needed
				if ( pOutputEnts )
				{
					pOutputEnts->AddToTail( pEnt );
				}
			}
		}

		return;
	}

	Assert( !"Didn't spawn anything!" );
}

CBaseEntity *CSurvivalGameRules::SpawnItem( const Vector &vecPosition, const QAngle& angAngle, const char *pszEntityName, int nAmmo, CBasePlayer *pPlayer /*= NULL*/ )
{
	Vector vecSpawnLocation = vecPosition;
	vecSpawnLocation.z += 10;

	CBaseEntity *pEnt = NULL;
	DZ_ConsoleMsg( "Spawning (%s[%i]) at [%.1f,%.1f,%.1f]\n", pszEntityName, nAmmo, XYZ( vecSpawnLocation ) );

	// MADHACK: pack the tablet type into the entity name
	const char* pszOriginalTabletName = pszEntityName;
	if ( V_stristr( pszOriginalTabletName, "weapon_tablet" ) )
	{
		pszEntityName = "weapon_tablet";
	}

	if ( pPlayer && dev_dz_econ_enable.GetBool() )
	{
		pEnt = pPlayer->GiveNamedItem( pszEntityName, 0, NULL, false, &vecSpawnLocation );
	}
	else
	{
#if !defined( NO_STEAM ) && !defined( NO_STEAM_GAMECOORDINATOR )
		// generate a base item of the given type
		CItemSelectionCriteria criteria;
		criteria.SetQuality( AE_NORMAL );
		criteria.BAddCondition( "name", k_EOperator_String_EQ, pszEntityName, true );
		pEnt = ItemGeneration()->GenerateRandomItem( &criteria, vecSpawnLocation, angAngle );

		if ( !pEnt )
		{
			criteria.SetQuality( AE_UNIQUE );
			pEnt = ItemGeneration()->GenerateRandomItem( &criteria, vecSpawnLocation, angAngle );
		}
#endif
	}
		
	if ( !pEnt )
	{
		pEnt = CBaseEntity::Create( pszEntityName, vecSpawnLocation, angAngle, NULL );
	}
		
	if ( pEnt )
	{
		if ( V_stristr( pszEntityName, "dronegun" ) )
		{
			pEnt->SetAbsAngles( vec3_angle );
			pEnt->m_bEligibleForScreenHighlight = false;
		}
		else if ( V_stristr( pszEntityName, "chicken" ) )
		{
			pEnt->m_bEligibleForScreenHighlight = false;
		}
		else if ( V_stristr( pszEntityName, "radar_jammer" ) )
		{
			pEnt->m_bEligibleForScreenHighlight = false;

			CCSPlayer *pCSPlayer = ToCSPlayer( pPlayer );
			if ( pCSPlayer )
			{
				pEnt->SetOwnerEntity( pCSPlayer->Weapon_OwnsThisType( "weapon_tablet" ) );
			}
		}
		//else if ( V_stristr( pszEntityName, "item_cash" ) )
		//{
		//	pEnt->m_bEligibleForScreenHighlight = false;
		//}
		else
		{
			pEnt->m_bEligibleForScreenHighlight = true;
		}
	}

	// set ammo
	if ( pEnt && pEnt->IsBaseCombatWeapon() )
	{
		CBaseCombatWeapon *pWeapon = static_cast<CBaseCombatWeapon*>( pEnt );
		if ( pWeapon )
		{
			pWeapon->m_bPlayerAmmoStockOnPickup = false;
		
			if ( pWeapon->GetReserveAmmoMax( AMMO_POSITION_PRIMARY ) > 0 )
			{
				pWeapon->m_iClip1 = 0;
				pWeapon->SetReserveAmmoCount( AMMO_POSITION_PRIMARY, MAX( 0, nAmmo ), true );
			}
		}
	}

	Assert( pEnt );
	return pEnt;
}

#include "weapon_tablet_buymenu.inc"
bool CSurvivalGameRules::TabletPurchase( CBasePlayer * pPlayer, const char *szCmd )
{
	if ( CSGameRules() && CSGameRules()->IsWarmupPeriod() )
		return false;

	if ( !pPlayer || !pPlayer->IsAlive() )
		return false;

	CCSPlayer *pCSPlayer = ToCSPlayer( pPlayer );
	if ( !pCSPlayer )
		return false;

	CBaseCombatWeapon* pCurrentWep = pCSPlayer->GetActiveWeapon();
	CTablet* pTablet = dynamic_cast<CTablet*>(pCSPlayer->Weapon_OwnsThisType( "weapon_tablet" ));
	if ( !pTablet || pTablet != pCurrentWep )
		return false;

	const char *pszBuyItem = szCmd + V_strlen( "tabletbuy_buy_" );
	
	const TabletBuyMenuEntry *pBuyMenuEntry = NULL;
	for ( int i = 0; i < ARRAYSIZE( s_TabletBuyMenu ); i++ )
	{
		if ( !s_TabletBuyMenu[ i ].szSpawnRuleGroupName	// ignore no longer purchase'able entries
			|| !*s_TabletBuyMenu[ i ].szSpawnRuleGroupName )
			continue;

		// Ensure that the conditions on item availability are satisfied
		if ( s_TabletBuyMenu[ i ].nGameRulesDecisionTypeID )
		{
			int nCurrentRulesValue = GetSurvivalGameRuleDecisionValue( ( ESurvivalGameRuleDecision_t ) s_TabletBuyMenu[ i ].nGameRulesDecisionTypeID );
			if ( nCurrentRulesValue != s_TabletBuyMenu[ i ].nGameRulesDecisionValue )
				continue;
		}

		if ( !Q_stricmp( pszBuyItem, s_TabletBuyMenu[i].szSpawnRuleGroupName ) )
		{
			pBuyMenuEntry = &s_TabletBuyMenu[i];
			break;
		}
	}

	if ( !pBuyMenuEntry )
	{
		Assert( false );
		return false;
	}

	extern ConVar sv_dz_cash_bundle_size;
	int nPrice = pBuyMenuEntry->nPrice * sv_dz_cash_bundle_size.GetInt();
	if ( pCSPlayer->GetAccountBalance() < nPrice )
	{
		pPlayer->EmitSound( "Survival.BuyItemFailed" );
		return false;
	}

	CFmtStr strAuditDronePurchase( "%s", pBuyMenuEntry->szSpawnRuleGroupName );
	pCSPlayer->AddAccount( -nPrice, true, strAuditDronePurchase );

	{
		pTablet->SetLastPurchaseIndex( pBuyMenuEntry->nUniqueIndex );
		pTablet->PushTabletNotification( TABLET_NOTIFICATION_PURCHASE_CONFIRMATION );
	}

	// hack - instantly apply tablet upgrades from buymenu, don't deliver by drone
	bool bDroneDispatched = false;
	if ( V_stristr( szCmd, "drone_drone_upgrade_purchase" ) )
	{
		pTablet->SetTabletUpgrade( TABLET_UPGRADE_DRONEINTEL, false );
	}
	else if ( V_stristr( szCmd, "drone_zone_upgrade_purchase" ) )
	{
		pTablet->SetTabletUpgrade( TABLET_UPGRADE_ZONEINTEL, false );
	}
	else if ( V_stristr( szCmd, "drone_highres_upgrade_purchase" ) )
	{
		pTablet->SetTabletUpgrade( TABLET_UPGRADE_HIGHRES, false );
	}
	else
	{
		// spawn the drone on the other side of the map so it has to fly over other players to arrive
		dz_queued_drone_purchase_t *pDronePurchase = &m_vecQueuedDronePurchases[m_vecQueuedDronePurchases.AddToTail()];
		pDronePurchase->m_vecDroneSpawnPos = ComputeDroneSpawnPosFromDeliveryPos( pPlayer->GetAbsOrigin() );
		pDronePurchase->m_hDestinationTablet = pTablet;
		pDronePurchase->m_bFastDelivery = pTablet->HasTabletUpgrade( TABLET_UPGRADE_DRONEINTEL );
		pDronePurchase->m_szBuyMenuEntry = pBuyMenuEntry->szSpawnRuleGroupName;
		pDronePurchase->m_hPlayer = pPlayer;
		pDronePurchase->m_vecPurchasePos = pPlayer->GetAbsOrigin();
		pDronePurchase->m_nOthersInFlight = 0; // computed later
		bDroneDispatched = true;
	}

	pPlayer->EmitSound( "Survival.BuyItem" );

	if ( IGameEvent *event = gameeventmanager->CreateEvent( "drone_dispatched" ) )
	{
		event->SetInt( "userid", pCSPlayer->GetUserID() );
		event->SetInt( "priority", 6 );
		event->SetBool( "drone_dispatched", bDroneDispatched );
		gameeventmanager->FireEvent( event );
	}

	return true;
}

Vector CSurvivalGameRules::ComputeDroneSpawnPosFromDeliveryPos( Vector vecDeliveryPos )
{
	AABB_t playArea = GetPlayAreaBounds();
	playArea.m_vMinBounds.z = -1;
	playArea.m_vMaxBounds.z = 1;

	Vector vecPos = vecDeliveryPos;
	vecPos += RandomVector( -400, 400 );
	vecPos.z = 0.0f;
	vecPos.NormalizeInPlace();
	vecPos *= playArea.LengthOfLargestDimension();

	CBaseTrace trace;
	IntersectRayWithBox( -vecPos, vecPos, playArea.m_vMinBounds, playArea.m_vMaxBounds, 10.0f, &trace );

	Vector vecDroneSpawnPos = vec3_origin;
	if ( trace.fraction <= 0 || trace.fraction >= 1.0f )
	{
		Assert( false );
		vecDroneSpawnPos = RandomInt( 0, 1 ) == 0 ? playArea.m_vMinBounds : playArea.m_vMaxBounds;
	}
	else
	{
		vecDroneSpawnPos = trace.endpos;
	}

	vecDroneSpawnPos.z = 600; // fixme

	return vecDroneSpawnPos;
}

void CSurvivalGameRules::DispatchQueuedDrones( void )
{
	if ( m_vecQueuedDronePurchases.IsEmpty() )
		return;

	float flTimeSinceLastDroneSpawn = gpGlobals->curtime - m_flLastDroneSpawnTime;
	if ( flTimeSinceLastDroneSpawn < 1.0f )
		return;
	m_flLastDroneSpawnTime = gpGlobals->curtime;

	// find out and update how many existing in-flight deliveries there are for each queued purchase destination, and remember the fewest while we're at it
	int nFewest = INT_MAX;
	int nFewestIndex = 0;
	FOR_EACH_VEC( m_vecQueuedDronePurchases, n )
	{
		dz_queued_drone_purchase_t *pThisDronePurchase = &m_vecQueuedDronePurchases[n];
		pThisDronePurchase->m_nOthersInFlight = 0; // reset

		FOR_EACH_VEC( IDrone::AutoList(), nDrone )
		{
			CDrone *pDrone = static_cast<CDrone*>(IDrone::AutoList()[nDrone]);
			if ( pThisDronePurchase->m_hDestinationTablet == pDrone->GetMoveToEnt() )
			{
				// increment
				pThisDronePurchase->m_nOthersInFlight++;
			}
		}

		if ( pThisDronePurchase->m_nOthersInFlight < nFewest )
		{
			nFewest = pThisDronePurchase->m_nOthersInFlight;
			nFewestIndex = n;
		}
	}

	// first dispatch purchases that have the fewest existing in-flight deliveries, and only dispatch ones that have fewer than (limit) in-flight
	dz_queued_drone_purchase_t *pDronePurchase = &m_vecQueuedDronePurchases[nFewestIndex];

	if ( pDronePurchase->m_nOthersInFlight > dev_dz_drone_inflight_delivery_limit.GetInt() )
		return;

	CBaseEntity *pEnt = CBaseEntity::Create( "drone", pDronePurchase->m_vecDroneSpawnPos, vec3_angle, NULL );
	Assert( pEnt );

	CDrone *pDrone = dynamic_cast<CDrone*>(pEnt);
	if ( pDrone )
	{
		pDrone->SetUpgraded( pDronePurchase->m_bFastDelivery );

		pDrone->StartQueuingOrders();
		pDrone->PushNewOrders( droneOrders_t( DRONE_STATE_RETURN_TO_DESPAWN ) );
		pDrone->PushNewOrders( droneOrders_t( DRONE_STATE_FLY_UP, 2 ) );
		pDrone->PushNewOrders( droneOrders_t( DRONE_STATE_DROP_CARGO ) );
		pDrone->PushNewOrders( droneOrders_t( DRONE_STATE_GET_TO_GOOD_CARGO_DROP_POS, 4 ) );

		pDrone->SetLastKnownMoveToEntityPosition( pDronePurchase->m_vecPurchasePos );
		pDrone->SetMoveToEnt( pDronePurchase->m_hDestinationTablet );

		pDrone->PushNewOrders( droneOrders_t( DRONE_STATE_MOVE_TO_ENTITY ) );

		CCSPlayer *pOwner = ToCSPlayer( pDronePurchase->m_hPlayer );

		if ( pOwner )
			pOwner->m_nDronesOrdered++;

		pDrone->SetCargoOwner( pOwner );

		CUtlVector< CBaseEntity* > outputEnts;
		SpawnItemOnEvent( pDronePurchase->m_vecDroneSpawnPos - Vector( 0, 0, 30 ), vec3_angle, 
			pDronePurchase->m_szBuyMenuEntry, pOwner, &outputEnts );

		// there should only be 1 cargo spawned
		Assert( outputEnts.Count() == 1 );
		if ( !outputEnts.IsEmpty() )
		{
			CPhysPropLootCrate *pCrate = dynamic_cast< CPhysPropLootCrate* >( outputEnts[0] );
			if ( pCrate )
			{
				pCrate->SetCrateOwner( pOwner );

				pCrate->SetBodygroupPreset( "show_cargo_rings" );

				pCrate->SetOriginalSource( "drone" );
			}

			pDrone->AttachCargo( outputEnts[0] );
		}

		pDrone->StopQueuingOrders();
	}

	m_vecQueuedDronePurchases.Remove( nFewestIndex );
	
}

void CSurvivalGameRules::InitPlayerSpawnLocations( void )
{
	if ( m_vecPlayerSpawnLocations.Count() )
		return;

	int nNumPlayersAliveOrDead = UTIL_HumansInGame( true, true );

	nNumPlayersAliveOrDead = MAX( nNumPlayersAliveOrDead, 8 );

	int nRoll = RandomInt( 0, 3 );
	
	Vector vSpawnLineA = Vector( 1, -1, 0 );
	Vector vSpawnLineB = Vector( 1, 1, 0 );

	if ( nRoll == 0 )
	{
		vSpawnLineA = Vector( 1, -1, 0 );
		vSpawnLineB = Vector( 1, 1, 0 );
	}
	else if( nRoll == 1 )
	{
		vSpawnLineA = Vector( 1, 1, 0 );
		vSpawnLineB = Vector( -1, 1, 0 );
	}
	else if ( nRoll == 2 )
	{
		vSpawnLineA = Vector( -1, 1, 0 );
		vSpawnLineB = Vector( -1, -1, 0 );
	}
	else if ( nRoll == 3 )
	{
		vSpawnLineA = Vector( -1, -1, 0 );
		vSpawnLineB = Vector( 1, -1, 0 );
	}
	else
	{
		Assert( false );
	}

	vSpawnLineA *= 5000.0f;
	vSpawnLineB *= 5000.0f;

	for ( int nPlayer = 0; nPlayer < nNumPlayersAliveOrDead; nPlayer++ )
	{
		float flRatio = (float)nPlayer / (float)(nNumPlayersAliveOrDead-1);

		Vector vecPos = Lerp( flRatio, vSpawnLineA, vSpawnLineB );

		vecPos += ( vecPos.Normalized() * RandomFloat( -1200, 1200 ) );

		vecPos.z = RandomInt( 5800, 6000 );

		m_vecPlayerSpawnLocations.AddToTail( vecPos );
	}

	m_vecPlayerSpawnLocations.Shuffle();
}

void CSurvivalGameRules::DoPlayerExitWarmupTransition( float flDuration )
{
	FOR_EACH_PLAYER( iPlayer )
	{
		CCSPlayer* pPlayer = UTIL_ValidatePlayer( iPlayer, false, false, false, false );
		if ( !pPlayer )
			continue;

		// fade us out to blue-white
		color32_s clr = { 227, 242, 255, 255 };
		UTIL_ScreenFade( pPlayer, clr, flDuration, 3.0f, FFADE_OUT | FFADE_SOFTCURVE );
	}
}

extern ConVar mp_round_restart_delay;
bool CSurvivalGameRules::CheckWinConditions( void )
{
	// is the game still in progress?

	if ( !CSGameRules() )
		return false;

	if ( CSGameRules()->IsWarmupPeriod() || CSGameRules()->IsFreezePeriod() )
		return true;

	bool roundIsAlreadyOver = (CSGameRules()->m_iRoundWinStatus != WINNER_NONE);
	if ( roundIsAlreadyOver )
		return false;

	uint nAlivePlayers = 0;
	CCSPlayer* pAlivePlayer = NULL;

	CDangerZoneController *pZone = GetDangerZoneController();
	bool bAlivePlayerOutsideTheZone = false;

	uint nAliveTeams = 0;
	CUtlVector<int> vecAliveTeamIndices;
	vecAliveTeamIndices.RemoveAll();

	FOR_EACH_PLAYER( iPlayer )
	{
		CCSPlayer* pPlayer = UTIL_ValidatePlayer( iPlayer, true, true, true, false );
		if ( !pPlayer )
			continue;

		nAlivePlayers++;

		if ( vecAliveTeamIndices.Find( pPlayer->m_nSurvivalTeam ) == vecAliveTeamIndices.InvalidIndex() )
		{
			vecAliveTeamIndices.AddToTail( pPlayer->m_nSurvivalTeam );
		}

		// Check if this is a better alive player to be "featured"?
		if ( !pAlivePlayer )
		{
			pAlivePlayer = pPlayer;
		}
		else
		{
			float flExistingTimeStamp = pAlivePlayer->m_flDealtDamageToEnemyMostRecentTimestamp;
			if ( flExistingTimeStamp < gpGlobals->curtime - 10 ) flExistingTimeStamp = 0; // not recent
			float flNewTimeStamp = pPlayer->m_flDealtDamageToEnemyMostRecentTimestamp;
			if ( flNewTimeStamp < gpGlobals->curtime - 10 ) flNewTimeStamp = 0; // not recent
			if ( flExistingTimeStamp != flNewTimeStamp )
			{	// can resolve the "featured" player by how recent they dealt damage to an enemy
				if ( flNewTimeStamp > flExistingTimeStamp )
					pAlivePlayer = pPlayer;
			}
			else
			{	// should resolve the "featured" player by their contribution score
				int nExistingScore = pAlivePlayer->GetContributionScore();
				int nNewScore = pPlayer->GetContributionScore();
				if ( nExistingScore != nNewScore )
				{
					if ( nNewScore > nExistingScore )
						pAlivePlayer = pPlayer;
				}
				else
				{
					// resolve by the kills
					if ( pPlayer->FragCount() > pAlivePlayer->FragCount() )
						pAlivePlayer = pPlayer;
				}
			}
		}

		if ( nAlivePlayers > 1 )
			m_bWasThereEverMoreThanOnePlayerAlive = true;

		if ( dev_dz_keep_bots_in_zone.GetBool() && pPlayer->IsBot() && pPlayer->IsAlive() && GetDangerZoneController() && !GetDangerZoneController()->IsWithinPlayArea( pPlayer->GetAbsOrigin() ) )
		{
			Vector vecTraceFrom = GetDangerZoneController()->MovePointIntoPlayArea( pPlayer->GetAbsOrigin() );
			AABB_t playArea = GetPlayAreaBounds();
			vecTraceFrom.z = playArea.m_vMaxBounds.z;
			Vector vecTraceTo = vecTraceFrom;
			vecTraceTo.z = 0;
			trace_t tr;
			UTIL_TraceLine( vecTraceFrom, vecTraceTo, MASK_SOLID, NULL, COLLISION_GROUP_NONE, &tr );
			Vector vecTeleportPos = tr.endpos;
			vecTeleportPos.z += 100;
			QAngle angTemp = vec3_angle;
			pPlayer->Teleport( &vecTeleportPos, &angTemp, &vec3_origin );
		}

		// If we require that alive players must be in the safe zone then:
		// if there are alive players outside the zone, the game goes on. This is true in duo+ too
		if ( dev_dz_win_condition_zonecheck.GetBool() && !bAlivePlayerOutsideTheZone &&
			pZone && !pZone->IsWithinPlayArea( pPlayer->GetAbsOrigin() ) )
			bAlivePlayerOutsideTheZone = true;
	}

	nAliveTeams = vecAliveTeamIndices.Count();

	if ( m_nLastKnownTeamsRemainingCount > nAliveTeams ) // can only decrement
	{
		m_nLastKnownTeamsRemainingCount = nAliveTeams;
	}

	if ( m_nLastKnownPlayersAliveCount > nAlivePlayers ) // can only decrement
	{
		m_nLastKnownPlayersAliveCount = nAlivePlayers;
	}

	if ( IsPlayingSoloMode() )
	{
		// if there's more than one player alive in solo mode, the game goes on
		if ( m_nLastKnownPlayersAliveCount > 1 )
		{
			return true;
		}
	}
	else // team mode
	{
		// if more than one team remains in team mode, the game goes on
		if ( m_nLastKnownTeamsRemainingCount > 1 )
		{
			return true;
		}
	}

	// if there are alive players outside the zone, the game goes on, no matter the mode
	if ( bAlivePlayerOutsideTheZone )
	{
		return true;
	}

	// Prepare for end of match
	int nRoundWinReason = Survival_Win;

	if ( m_nLastKnownPlayersAliveCount == 1 || (IsPlayingTeamMode() && m_nLastKnownTeamsRemainingCount == 1) )
	{
		if ( !m_bWasThereEverMoreThanOnePlayerAlive )
		{
			return true;
		}
	}
	else
	{
		// We may still determine a player to proclaim #1 and will override this win reason
		nRoundWinReason = Survival_Draw;
	}

	// Looks like we have our winner winner chicken dinner
	// Hold the game in this state for a little bit to ensure
	// that all ragdolls settle and the player is not going to die imminently
	// like from a random massive explosion that happened to process players in order
	if ( !m_nWinConditionStageProgress )
	{
		if ( m_flLastWinConditionDetectedTime )
		{
			if ( gpGlobals->curtime - m_flLastWinConditionDetectedTime < dev_dz_win_condition_duration.GetFloat() )
				return true; // continue waiting
		}
		else
		{
			m_nEntIndexOfRunnerUpPlayer = m_nEntIndexOfKilledPlayerCheckingWinConditions;
			m_flLastWinConditionDetectedTime = gpGlobals->curtime;
			return true; // start waiting countdown
		}
		//
		// Win condition has been satisfied for long enough
		// proceed with the win-out sequence
		//
		m_nWinConditionStageProgress = nRoundWinReason;

		// If there are no alive players at the end of the match then determine who is the NUMBER 1?
		if ( !pAlivePlayer )
		{
			FOR_EACH_PLAYER( iPlayer )
			{
				CCSPlayer* pPlayer = UTIL_ValidatePlayer( iPlayer, false, true, true, false );
				if ( !pPlayer )
					continue;
				
				int nPlayerEnt = pPlayer->entindex();
				if ( nPlayerEnt >= 1 && nPlayerEnt <= MAX_PLAYERS )
				{
					// player is on winning team
					int nPosition = m_roundData_playerPositions[ nPlayerEnt ];
					if ( 1 == nPosition )
					{
						pAlivePlayer = pPlayer;
					}
				}
				else
					continue;
			}
		}

		if ( pAlivePlayer ) // todo: list whole team as winners. Should there be an MVP?
		{
			m_nWinConditionStageProgress = Survival_Win; // force the win condition if there's a connected player at position #1
			m_hWinnerPlayer = pAlivePlayer;

			pAlivePlayer->IncrementNumMVPs( CSMVP_SURVIVALSURVIVOR );

			DZ_ConsoleMsg( "GAME OVER - \"%s<%i><%s><%s>\" [%.0f %.0f %.0f] is the surviving winner!\n",
				pAlivePlayer->GetPlayerName(),
				pAlivePlayer->GetUserID(),
				pAlivePlayer->GetNetworkIDString(),
				pAlivePlayer->GetTeam()->GetName(),
				pAlivePlayer->GetAbsOrigin().x,
				pAlivePlayer->GetAbsOrigin().y,
				pAlivePlayer->GetAbsOrigin().z );

			//
			// Bonus points for live players on winning team
			// And also make sure that we send out most up to date standings to all remaining players
			//
			PlayerTeammateVector_t winningTeam;
			GetPlayerTeammates( pAlivePlayer, winningTeam );
			winningTeam.AddToTail( pAlivePlayer );
			FOR_EACH_VEC( winningTeam, i )
			{
				CCSPlayer* pPlayer = winningTeam[i];
				int nPlayerEnt = pPlayer->entindex();
				if ( pPlayer->IsAlive() )
				{
					pPlayer->AddContributionScore( 40 );

					// paranoia
					if ( nPlayerEnt >= 1 && nPlayerEnt <= MAX_PLAYERS )
					{
						pPlayer->CloseOutPlayerForTheRound();
						pPlayer->m_nTimeToWin = gpGlobals->curtime - pPlayer->m_flLifeStartTime;

						// player is on winning team
						m_roundData_playerPositions.Set( nPlayerEnt, 1 );
						FunFacts_EvaluateSurvivalStats( pPlayer, &m_survivalStats[nPlayerEnt] );
						FunFacts_SendSurvivalStats( pPlayer, m_roundData_playerXuids[nPlayerEnt], m_survivalStats[nPlayerEnt] );
					}
					else
					{
						Assert( false );
					}
				}
			}

			//
			// Now force all the replays
			//
			// Winner congratulatory replay parameters for the winning player and their squad
			ClientReplayEventParams_t reParams( REPLAY_EVENT_VICTORY );
			reParams.m_flEventTime = gpGlobals->curtime;
			reParams.m_bForceUseTheseSettings = true;
			FOR_EACH_VEC( winningTeam, i )
			{
				CCSPlayer* pPlayer = winningTeam[ i ];
				int nPlayerEnt = pPlayer->entindex();
				//
				// Force a replay onto the alive player to witness their glorious victory!
				// The way the "winningTeam" array is built ensures that the last teammate
				// will be alive when the first teammate is checked
				//
				if ( pPlayer->IsAlive() )
				{
					// Prepare the player for the replay
					pPlayer->m_takedamage = DAMAGE_NO;	// This disables all forms of damage, including radius damage from grenades..
					pPlayer->AddFlag( FL_FROZEN );		// Freeze the player in place
					pPlayer->AddFlag( FL_GODMODE );		// Prevent any damage
					pPlayer->AddFlag( FL_NOTARGET );	// Avoid any NPC targeting

														// Initiate the replay (watch yourself!)
					reParams.m_nPrimaryTargetEntIndex = m_nEntIndexOfRunnerUpPlayer ? m_nEntIndexOfRunnerUpPlayer : pPlayer->entindex();
				}
				else
				{
					// Initiate the replay (assume we will watch your living friend win it!)
					reParams.m_nPrimaryTargetEntIndex = pAlivePlayer->entindex();

					if ( nPlayerEnt >= 1 && nPlayerEnt <= MAX_PLAYERS )
					{
						// check if this player is also the "winner"
						// we rely on "m_roundData_playerPositions" snapping position of the player at their death moment
						// so that way if all our squad dies to a bomb explosion, but we are all #1 together then we will
						// watch our own self as the winner
						int nPosition = m_roundData_playerPositions[ nPlayerEnt ];
						if ( 1 == nPosition )
						{	// if we are also "the winner" then watch ourselves
							reParams.m_nPrimaryTargetEntIndex = pPlayer->entindex();
						}
					}

					if ( m_nEntIndexOfRunnerUpPlayer )
						reParams.m_nPrimaryTargetEntIndex = m_nEntIndexOfRunnerUpPlayer;
				}
				pPlayer->StartHltvReplayEvent( reParams );
			}
		}
		else
		{
			DZ_ConsoleMsg( "GAME OVER - There are no surviving winners!\n" );
		}

		return true; // we initiated final replays, wait for them to start playing
	}
	else
	{
		if ( gpGlobals->curtime - m_flLastWinConditionDetectedTime > dev_dz_win_condition_duration.GetFloat() + dev_dz_win_condition_post.GetFloat() )
		{
			// Now we can terminate round
			CSGameRules()->m_match.IncrementRound( 1 );
			CSGameRules()->TerminateRound( mp_round_restart_delay.GetFloat(), m_nWinConditionStageProgress );

			return false;
		}
		else
		{
			return true; // keep waiting
		}
	}
}

#if DEVELOPMENT_ONLY && defined( GAME_DLL )
DEVELOPMENT_ONLY_CONVAR( debug_mvp_assign_reason, 7 /*=CSMVP_SURVIVALSURVIVOR*/ );
CON_COMMAND( debug_mvp_assign, "Make the human player MVP" )
{
	FOR_EACH_PLAYER( iPlayer )
	{
		CCSPlayer* pPlayer = UTIL_ValidatePlayer( iPlayer, true, true, true, true );
		if ( !pPlayer )
			continue;
		
		pPlayer->IncrementNumMVPs( ( CSMvpReason_t ) debug_mvp_assign_reason.GetInt() );
		return;
	}
}
#endif

void CSurvivalGameRules::ResetDangerZone( void )
{
	CDangerZoneController *pZone = GetDangerZoneController();
	if ( pZone )
	{
		pZone->ResetMasterDangerZone();
	}
}
#endif

void CSurvivalGameRules::OnWarmupStart( void )
{
	EarlyOutIfNotPlayingSurvival;

#ifdef GAME_DLL
	DZ_ConsoleMsg( "Warmup started.\n" );

	ConvertSafeSpawnsToItemSpawns();
#endif
}

#ifdef GAME_DLL
void CSurvivalGameRules::ConvertSafeSpawnsToItemSpawns( void )
{
	// remove map-placed safes and turn them into regular item spawn points if needed
	if ( dev_dz_random_safe_locations.GetBool() )
	{
		FOR_EACH_VEC( IBRC4Target::AutoList(), i )
		{
			CBRC4Target *pTarget = static_cast<CBRC4Target*>(IBRC4Target::AutoList()[i]);
			CPointDZWeaponSpawn *pNewPoint = dynamic_cast<CPointDZWeaponSpawn *>(CreateEntityByName( "point_dz_weaponspawn" ));
			if ( pNewPoint )
			{
				pNewPoint->SetAbsAngles( pTarget->GetAbsAngles() );
				pNewPoint->SetAbsOrigin( pTarget->GetAbsOrigin() );
				UTIL_Remove( pTarget );
			}
		}
	}
}
#endif

void CSurvivalGameRules::OnRoundRestart( void )
{
	EarlyOutIfNotPlayingSurvival;

#ifdef GAME_DLL

	ConvertSafeSpawnsToItemSpawns();

	if ( GetGrassBurn() )
	{
		GetGrassBurn()->ResetGrassBurn();
	}

	ResetLocalEventVars();

	// We should not reload config file every round restart because it messes up state
	// due to random decision being different in warmup -vs- after deploy into the live game
	// ReloadConfigFile();

	ResetPlayerSpawnData();

	InitGlobalTablet();

	if ( !CSGameRules()->IsWarmupPeriod() )
	{
		m_spawnStage = SPAWN_STAGE_SELECTION;

		m_flSpawnSelectionTimeStart = gpGlobals->curtime;
		m_flSpawnSelectionTimeEnd = m_flSpawnSelectionTimeStart + dev_dz_spawn_selection_time.GetFloat();
		m_flSpawnSelectionTimeLoadout = m_flSpawnSelectionTimeEnd + dev_dz_spawn_selection_lock_time.GetFloat() + dev_dz_spawn_selection_ready_time.GetFloat();

		UTIL_ClientPrintAll( HUD_PRINTTALK, "#SFUI_SpawnSelectPrepare" );
		UTIL_ClientPrintAll( HUD_PRINTTALK, "#SFUI_SpawnSelectBegin" );
	}

	m_vecPlayerSpawnLocations.RemoveAll();

	m_flLastDroneSpawnTime = 0;
	m_vecQueuedDronePurchases.RemoveAll();

	if ( GetDangerZoneController() )
	{
		GetDangerZoneController()->DisableDangerZone();
	}

	FindGameSurvivalLogicEntity();

	DZ_ConsoleMsg( "Round restarted. Danger zone disabled.\n" );
#endif
}

void CSurvivalGameRules::OnLevelInitPostEntity( void )
{
#if defined ( GAME_DLL )
	InitBounds();
#endif
}



#ifdef GAME_DLL

CGameSurvivalLogic *CSurvivalGameRules::GetGameSurvivalLogicEntity( void )
{
	return static_cast< CGameSurvivalLogic* >( m_gameSurvivalLogicEntity.Get() );
}

void CSurvivalGameRules::FindGameSurvivalLogicEntity( void )
{
	if ( !static_cast< CGameSurvivalLogic* >( m_gameSurvivalLogicEntity.Get() ) )
	{
		CBaseEntity *ent = NULL;
		do
		{
			ent = gEntList.FindEntityByClassname( ent, "game_survival_logic" );

			if ( ent )
			{
				CSGameRules()->GetSurvivalRules()->SetGameSurvivalLogicEntity( ent );
				break;
			}

		} while ( ent );
	}
}

void CSurvivalGameRules::ResetPlayerSpawnData()
{
	COMPILE_TIME_ASSERT( SURVIVAL_SPAWN_TILE_NUM <= 1024 ); // Source1 Net code doesn't seem to handle arrays bigger than this
	Assert( SURVIVAL_SPAWN_TILE_NUM == SURVIVAL_SPAWN_TILE_WIDTH * SURVIVAL_SPAWN_TILE_HEIGHT );

	// Initialize spawn select to 'not running'
	for ( int i = 0; i < m_iPlayerSpawnHexIndices.Count(); ++i )
	{
		m_iPlayerSpawnHexIndices.Set( i, INVALID_SPAWN_HEX );
	}
	m_flSpawnSelectionTimeStart = -1;
	m_flSpawnSelectionTimeEnd = -1;
	m_flSpawnSelectionTimeLoadout = -1;
	m_spawnStage = SPAWN_STAGE_NONE;

	// load map mask & initialize networked copy
	ESurvivalSpawnTileState tileState[SURVIVAL_SPAWN_TILE_NUM];
	const char* szMapName = STRING( gpGlobals->mapname );
	ReadMapMask( CFmtStr( "maps/%s_spawnmask.png", szMapName ), tileState );
	for ( int iTile = 0, nTiles = GetNumSpawnHex(); iTile < nTiles; ++iTile )
		m_SpawnTileState.Set( iTile, tileState[iTile] );
}

void CSurvivalGameRules::ReadMapMask( const char* szFileName, ESurvivalSpawnTileState( &tileState )[SURVIVAL_SPAWN_TILE_NUM] )
{
	int nTiles = GetNumSpawnHex();
	Assert( nTiles <= SURVIVAL_SPAWN_TILE_NUM );

	// Default to all available
	COMPILE_TIME_ASSERT( kSurvivalSpawn_Available == 0 );
	memset( tileState, 0, sizeof( ESurvivalSpawnTileState ) * nTiles );

	FileHandle_t fh = g_pFullFileSystem->Open( szFileName, "rb", "GAME" );
	if ( !fh ) // No spawn mask, default to all available
		return;

	CUtlBuffer fileData;
	bool readOk = g_pFullFileSystem->ReadToBuffer( fh, fileData );
	g_pFullFileSystem->Close( fh );

	if ( !readOk )
		return;

	Bitmap_t bitmap;
	ConversionErrorType errorType = ImgUtl_LoadPNGBitmapFromBuffer( fileData, bitmap );
	if ( errorType != CE_SUCCESS )
		return;

	// don't need file data any more, free it
	fileData.Purge();

	int nWidth = bitmap.Width();
	int nHeight = bitmap.Height();

	// Sample bitmap at each tile center
	for ( int iTile = 0; iTile < nTiles; ++iTile )
	{
		Vector center = Get2DHexCenter( iTile ); // 0-1 range
		int x = Floor2Int( center.x * nWidth );
		int y = Floor2Int( center.y * nHeight );

		// handle 1.0 exactly
		if ( x == nWidth )
			x = nWidth - 1;
		if ( y == nHeight )
			y = nHeight - 1;

		// out of bounds (shouldn't really ever happen)
		if ( x < 0 || x >= nWidth || y < 0 || y >= nHeight )
		{
			tileState[iTile] = kSurvivalSpawn_GameBlocked;
			continue;
		}

		Color sample = bitmap.GetColor( x, y );
		if ( sample.r() < 128 )
		{
			tileState[iTile] = kSurvivalSpawn_MapBlocked;
		}
	}
}

bool CSurvivalGameRules::IsUseEntBehindClosedSecurityDoor( CBaseEntity *pUseEnt, CBasePlayer *pPlayer )
{
	// an alternative to this code could be to change the door frame model collision to not have gaps.

	if ( !pUseEnt || !pPlayer || pUseEnt->IsPlayer() ) // add more early outs?
		return false;

	{
		CDZDoor *pUseEntDoor = dynamic_cast<CDZDoor*>(pUseEnt);
		if ( pUseEntDoor )
			return false; // doors aren't behind doors
	}

	Vector vecItemPos = pUseEnt->WorldSpaceCenter(); // should maybe be player eye dir?
	Vector vecPlayerEyePos = pPlayer->EyePosition();
	Vector vecMidPt = (vecItemPos + vecPlayerEyePos) * 0.5;

	// find closest closed security door
	CDZDoor *pClosestDoor = NULL;
	float flBestDist = 256.0f;

	FOR_EACH_VEC( IDZDoor::AutoList(), iDoor )
	{
		CDZDoor *pDoor = static_cast<CDZDoor*>(IDZDoor::AutoList()[iDoor]);
		if ( !pDoor->IsSecurityDoor() || pDoor->IsDoorOpen() )
			continue;

		float flContenderDist = pDoor->WorldSpaceCenter().DistTo( vecMidPt );

		if ( flContenderDist < flBestDist )
		{
			flBestDist = flContenderDist;
			pClosestDoor = pDoor;
		}
	}

	if ( !pClosestDoor )
		return false;

	Assert( pClosestDoor->m_nAttachmentIndex1 > 0 && pClosestDoor->m_nAttachmentIndex2 > 0 );

	matrix3x4_t matFrame1Local;
	pClosestDoor->GetAttachmentLocal( pClosestDoor->m_nAttachmentIndex1, matFrame1Local );
	Assert( matFrame1Local.IsValid() );
	matrix3x4_t matFrame2Local;
	pClosestDoor->GetAttachmentLocal( pClosestDoor->m_nAttachmentIndex2, matFrame2Local );
	Assert( matFrame2Local.IsValid() );
	Assert( matFrame1Local.GetOrigin().DistTo( matFrame2Local.GetOrigin() ) > 180 );

//#ifdef DEBUG
//	debugoverlay->AddBoxOverlay( pClosestDoor->GetAbsOrigin(), matFrame2Local.GetOrigin(), matFrame1Local.GetOrigin(), pClosestDoor->GetAbsAngles(), 255, 0, 0, 0, 5.0f );
//	debugoverlay->AddLineOverlay( vecPlayerEyePos, vecItemPos, 255, 0, 0, true, 5.0f );
//#endif

	Ray_t ray;
	ray.Init( vecPlayerEyePos, vecItemPos );

	trace_t tr;
	if ( IntersectRayWithOBB( ray, pClosestDoor->GetAbsOrigin(), pClosestDoor->GetAbsAngles(), matFrame2Local.GetOrigin(), matFrame1Local.GetOrigin(), 0.0f, &tr ) )
	{
		return true;
	}

	return false;
}

void CSurvivalGameRules::OnPlayerKilled( CBasePlayer *pVictimBase, CBasePlayer *pAttackerBase )
{
	CCSPlayer* pVictim = ToCSPlayer( pVictimBase );
	CCSPlayer* pAttacker = ToCSPlayer( pAttackerBase );
	if ( !pVictim )
		return;

	int nVictimEnt = pVictim->entindex();

	// paranoia about array bounds
	Assert( nVictimEnt >= 1 && nVictimEnt <= MAX_PLAYERS );
	if ( nVictimEnt < 1 || nVictimEnt > MAX_PLAYERS )
		return;

	// calculate position for this player
	int nLiveTeams = 0;
	int nLiveOpponents = 0;
	static CUtlVector<int> sLiveTeams;
	sLiveTeams.RemoveAll();

	for ( int iPlayer = 1; iPlayer <= MAX_PLAYERS; ++iPlayer )
	{
		CCSPlayer* pEachPlayer = ToCSPlayer( UTIL_PlayerByIndex( iPlayer ) );
		if ( !pEachPlayer || pEachPlayer == pVictim || !pEachPlayer->IsAlive() || pEachPlayer->IsHLTV() || pEachPlayer->GetTeamNumber() != TEAM_TERRORIST )
			continue;

		if ( pEachPlayer->m_nSurvivalTeam < 0 )
		{
			nLiveTeams++;
			nLiveOpponents++;
		}
		else if ( pEachPlayer->m_nSurvivalTeam == pVictim->m_nSurvivalTeam )
		{
			// nothing, current player's team doesn't count for score
		}
		else if ( sLiveTeams.Find( pEachPlayer->m_nSurvivalTeam ) == sLiveTeams.InvalidIndex() )
		{
			nLiveTeams++;
			nLiveOpponents++;
			sLiveTeams.AddToTail( pEachPlayer->m_nSurvivalTeam );
		}
		else
		{
			nLiveOpponents++;
		}
	}

	// Store off user's position
	int position = nLiveTeams + 1;
	m_roundData_playerPositions.Set( nVictimEnt, position );

	// give bonus score if you survive until close to the end
	static const int kPositionBonuses[] = { 40, 20, 10, 5 }; // we give 40 points for 1st place
	if ( nLiveOpponents >= 0 && nLiveOpponents < V_ARRAYSIZE( kPositionBonuses ) )
		pVictim->AddContributionScore( kPositionBonuses[nLiveOpponents] );

	// Calculate funfacts and send to user
	FunFacts_EvaluateSurvivalStats( pVictim, &m_survivalStats[nVictimEnt] );
	FunFacts_SendSurvivalStats( pVictim, m_roundData_playerXuids[nVictimEnt], m_survivalStats[nVictimEnt] );

	// Handle contract kills
	if ( pAttacker && pAttacker != pVictim && pAttacker->IsAlive() )
	{
		// CCSPlayer *pVictimsContractKillTarget = ToCSPlayer( pVictim->m_hSurvivalAssassinationTarget.Get() );
		CCSPlayer *pAttackersContractKillTarget = ToCSPlayer( pAttacker->m_hSurvivalAssassinationTarget.Get() );

		if ( pAttackersContractKillTarget && pAttackersContractKillTarget == pVictim )
		{
			// This player was killed by their contract killer. Give the cash award to the killer and notify them.
			int reward = sv_dz_contractkill_reward.GetInt() * sv_dz_cash_bundle_size.GetInt();
			pAttacker->AddAccount( reward, true, "DZ contract kill - succeeded" );
			pAttacker->AddContributionScore( sv_dz_contractkill_reward.GetInt() * contributionscore_cash_bundle.GetInt() );
			ClientPrint( pAttacker, HUD_PRINTCENTER, "#SFUI_ContractKillComplete", CFmtStr( "%d", reward ) );

			// tell everyone else (via domination kill feed icon)
			// UTIL_ClientPrintAll( HUD_PRINTTALK, "#SFUI_ContractKillTextMsg", pAttacker->GetPlayerName(), pVictim->GetPlayerName() );
			pVictim->SetDeathFlags( CS_DEATH_DOMINATION );

			pAttacker->m_hSurvivalAssassinationTarget = nullptr;
			pAttacker->m_nCompletedSurvivalAssassinations++;

			// remove all contract kills from the killer's teammates, otherwise the following code will assign teammates to kill each other!
			if ( IsPlayingTeamMode() )
			{
				PlayerTeammateVector_t vecTeammates;
				GetPlayerTeammates( pAttacker, vecTeammates );
				FOR_EACH_VEC( vecTeammates, iTeammate )
				{
					vecTeammates[iTeammate]->m_hSurvivalAssassinationTarget = nullptr;
				}
			}

		}
	}

	pVictim->m_hSurvivalAssassinationTarget = nullptr; // dead players lose their contract kill targets

    // reassign any contract kills that are targeting this player to the killer
	CCSPlayer* pNewAssassinationTarget = pAttacker;
	if ( pNewAssassinationTarget == pVictim || !pNewAssassinationTarget || !pNewAssassinationTarget->IsAlive() )
		pNewAssassinationTarget = nullptr;

	for ( int iPlayer = 1; iPlayer <= gpGlobals->maxClients; ++iPlayer )
	{
		CCSPlayer* pEachPlayer = ToCSPlayer( UTIL_PlayerByIndex( iPlayer ) );
		if ( pEachPlayer && pEachPlayer->m_hSurvivalAssassinationTarget.Get() == pVictim )
		{
			if ( pEachPlayer == pNewAssassinationTarget )
			{
				// Can't have a contract targeting yourself.
				//
				// Note: this case shouldn't happen since it means we killed our contract target
				// and therefore should have had our target reset above.
				pEachPlayer->m_hSurvivalAssassinationTarget = nullptr;
			}
			else
			{
				// make absolutely sure we don't assign contract kills to teammates
				if ( !IsPlayingTeamMode() )
				{
					pEachPlayer->m_hSurvivalAssassinationTarget = pNewAssassinationTarget;
				}
				else
				{
					if ( pNewAssassinationTarget && ( pEachPlayer->m_nSurvivalTeam == pNewAssassinationTarget->m_nSurvivalTeam ) )
						pEachPlayer->m_hSurvivalAssassinationTarget = NULL;
					else
						pEachPlayer->m_hSurvivalAssassinationTarget = pNewAssassinationTarget;
				}
			}
		}
	}
}

void CSurvivalGameRules::ApplyHighlightColorToBaseAnimatingViaClassname( CBaseAnimating *pEntAnim )
{
	if ( !pEntAnim || !m_pBrConfig )
		return;
	
	const CBrConfig::BaseItem_t *pItem = m_pBrConfig->GetFirstMatchingBaseItemFromClassname( pEntAnim->GetClassname() );
	
	if ( pItem )
	{
		pEntAnim->SetHighlightColor( pItem->m_cHighlightColor.r(), pItem->m_cHighlightColor.g(), pItem->m_cHighlightColor.b() );
	}
}

static const char *s_pszDroneDefaultTabletDeliveryEntry = "drone_default_tablet_delivery";
void CSurvivalGameRules::LoadoutPlayer( CBasePlayer *pPlayer )
{
	// give the player startup items, and keep a record of players that have been given items so they don't get them more than once.

	if ( !pPlayer || CSGameRules()->IsWarmupPeriod() )
		return;

	FOR_EACH_VEC( m_vecLoadedOutPlayers, i )
	{
		if ( m_vecLoadedOutPlayers[i].Get() == pPlayer )
			return;
	}

	m_vecLoadedOutPlayers.AddToTail( pPlayer );

	pPlayer->GiveNamedItem( "weapon_healthshot" );
	pPlayer->GiveNamedItem( "weapon_tablet" );

	CBaseCombatWeapon* pFists = pPlayer->Weapon_OwnsThisType( "weapon_fists" );
	if ( pFists )
		pPlayer->Weapon_Switch( pFists );
}

void CSurvivalGameRules::ResetLocalEventVars( void )
{
	m_nLastKnownPlayersAliveCount = MAX_PLAYERS;
	m_nLastKnownTeamsRemainingCount = MAX_PLAYERS;

	m_bWasThereEverMoreThanOnePlayerAlive = false;
	m_bSurvivalEventFired_FadeEveryoneOutFromMapSelection = false;
	m_bSurvivalEventFired_PlayedWinnerSurrenderAnim = false;
	m_bSurvivalEventFired_TimeForSmokeBeacons = false;
	m_bSurvivalEventFired_FirstParadropIncoming = false;
	m_flTimeOfLastParadrop = gpGlobals->curtime;
	m_flLastWinConditionDetectedTime = 0;
	m_nWinConditionStageProgress = 0;
	m_nEntIndexOfRunnerUpPlayer = 0;
	m_nEntIndexOfKilledPlayerCheckingWinConditions = 0;

	m_hWinnerPlayer = INVALID_EHANDLE;

	m_vecLoadedOutPlayers.RemoveAll();
	m_arrParadropSettings.RemoveAll();
}
#endif

void CSurvivalGameRules::OnFreezePeriodExpired( void )
{
	EarlyOutIfNotPlayingSurvival;

#ifdef GAME_DLL
	if ( !CSGameRules()->IsWarmupPeriod() )
	{
		DZ_ConsoleMsg( "Freeze period expired.\n" );		

		ResetLocalEventVars();
	}
#endif
}

#ifdef GAME_DLL

static void HelperParadropConvertDangerZonesToDropPositions( CUtlVector< CDangerZone* > const &arrZones, int iZone, CUtlVector<dz_player_index_position_t> &vecPositions )
{
	for ( ; iZone < arrZones.Count(); ++ iZone )
	{
		Vector vecCheck = arrZones[ iZone ]->GetAbsOrigin();
		bool bValidZone = false;
		if ( arrZones[ iZone ]->BCanBePotentialEndGameHex() )	// zone must have spawn points in it to be considered for paradrops
		{
			Vector vecStart( vecCheck );
			Vector vecEnd( vecCheck );
			vecStart.z += 3000;
			vecEnd.z -= 3000;

			trace_t trace;
			UTIL_TraceLine( vecStart, vecEnd, MASK_WATER | MASK_NPCWORLDSTATIC, arrZones[ iZone ], COLLISION_GROUP_NONE, &trace );

			if ( trace.DidHit() && !trace.startsolid && !trace.allsolid )
			{
				bValidZone = ( trace.contents & CONTENTS_WATER ) == 0;
			}
		}
		
		if ( bValidZone )
		{
			dz_player_index_position_t &el = vecPositions[ vecPositions.AddToTail() ];
			el.m_nIndex = iZone;
			el.m_vecPosition = vecCheck;
		}
	}
}

static int HelperFindMostRemotePosition( CUtlVector< Vector > const &arrPoints, CUtlVector<dz_player_index_position_t> const &vecPositions )
{
	Assert( arrPoints.Count() );
	Assert( vecPositions.Count() );
	CUtlVector<dz_player_index_position_t> vecCopy;
	vecCopy.AddMultipleToTail( vecPositions.Count(), vecPositions.Base() );
	FOR_EACH_VEC( vecCopy, i )
	{
		vecCopy[i].m_nIndex = i;
	}
	for ( int iCounter = 0; vecCopy.Count() > 1; ++ iCounter )
	{
		Vector const &vCheck = arrPoints[ iCounter % arrPoints.Count() ];
		FOR_EACH_VEC( vecCopy, i )
		{
			vecCopy[i].m_flDist = ( vecCopy[i].m_vecPosition - vCheck ).Length2DSqr();
			for ( int iOthers = iCounter + 1, iOthersEnd = iCounter + arrPoints.Count(); iOthers < iOthersEnd; ++ iOthers )
			{	// but prefer distance to others be larger
				vecCopy[ i ].m_flDist -= ( vecCopy[ i ].m_vecPosition - arrPoints[ iOthers % arrPoints.Count() ] ).Length2D() / ( iOthers - iCounter ) / 10;
			}
		}
		vecCopy.Sort( BRPlayerSortDist );
		vecCopy.RemoveMultipleFromHead( 1 );
	}
	return vecCopy.Head().m_nIndex;
}

void CSurvivalGameRules::AutoDropParadrops( void )
{
	if ( CSGameRules()->IsWarmupPeriod() || CSGameRules()->IsFreezePeriod() )
		return;

	CDangerZoneController *pDangerZoneController = GetDangerZoneController();
	if ( !pDangerZoneController )
		return;

	float flTimeWaveEnd = GetDangerZoneController()->GetGameTimeOfWaveEnd( 1 );
	if ( !flTimeWaveEnd )
		return;

	if ( !m_bSurvivalEventFired_TimeForSmokeBeacons )
	{
		// We support up to 4 paradrops
		int numGuaranteedParadrops = MAX( dev_dz_force_paradrops_count.GetInt(), 0 );
		if ( numGuaranteedParadrops && m_arrParadropSettings.IsEmpty() )
		{
			// Generate paradrop settings for the very first paradrop
			ParadropSettings_t ps;
			ps.m_flTimeToDispatch = flTimeWaveEnd - RandomFloat( dev_dz_paradrops_arrival_range_min.GetFloat(), dev_dz_paradrops_arrival_range_max.GetFloat() ); // 90 sec pause after zone finishes fill
			m_arrParadropSettings.AddToTail( ps );
		}

		if ( m_arrParadropSettings.Count() )
			flTimeWaveEnd = m_arrParadropSettings.Head().m_flTimeToDispatch;

		// tell everyone it's paradrop beacon time
		float flTimeUntilParadropsAllowed = MAX( 0, flTimeWaveEnd - gpGlobals->curtime );
		if ( flTimeUntilParadropsAllowed == 0 )
		{
			//
			// Build paradrop parameters now
			//
			m_bSurvivalEventFired_TimeForSmokeBeacons = true;
			m_arrParadropSettings.RemoveAll();

			if ( numGuaranteedParadrops > 0 )
			{
				//
				// First one is dropped into the crowd
				//
				CUtlVector< CDangerZone* > arrZones;
				CUtlVector<dz_player_index_position_t> vecPositions;
				pDangerZoneController->GetZonesFromWaveID( 2, arrZones );
				int numZone2s = arrZones.Count();
				pDangerZoneController->GetZonesFromWaveID( 3, arrZones );
				pDangerZoneController->GetZonesFromWaveID( 4, arrZones );
				arrZones.AddToTail( pDangerZoneController->GetFinalZone() );

				HelperParadropConvertDangerZonesToDropPositions( arrZones, 0, vecPositions );
				if ( vecPositions.Count() < 3 )
					return;

				// Compute each cell's proximity to other players who are still alive
				FOR_EACH_VEC( vecPositions, iPositionIterator )
				{
					vecPositions[ iPositionIterator ].m_flDist = 0.0f;
					FOR_EACH_PLAYER( iPlayer )
					{
						CCSPlayer* pPlayer = UTIL_ValidatePlayer( iPlayer, true, true, true, false );
						if ( !pPlayer )
							continue;
						vecPositions[ iPositionIterator ].m_flDist +=
							( pPlayer->GetAbsOrigin() - vecPositions[ iPositionIterator ].m_vecPosition ).Length2DSqr();
					}
				}
				vecPositions.Sort( BRPlayerSortDist );
				{	// First paradrop goes into the most crowded location
					ParadropSettings_t ps0;
					ps0.m_flTimeToDispatch = gpGlobals->curtime;
					ps0.m_flChopperFlightTime = dev_dz_chopper_arrive_time.GetFloat();
					ps0.m_vecLocation = vecPositions.Head().m_vecPosition;
					m_arrParadropSettings.AddToTail( ps0 );
					vecPositions.FastRemove( 0 );
				}

				//
				// Second one is dropped into ring #2 diameter away from first
				//
				FOR_EACH_VEC( vecPositions, iPositionIterator )
				{
					vecPositions[ iPositionIterator ].m_flDist = ( m_arrParadropSettings.Head().m_vecLocation - vecPositions[ iPositionIterator ].m_vecPosition ).Length2DSqr();
				}
				vecPositions.Sort( BRPlayerSortDist );
				{
					int iSecondDrop = -1;
					FOR_EACH_VEC_BACK( vecPositions, iPositionIterator )
					{
						if ( vecPositions[ iPositionIterator ].m_nIndex < numZone2s )
						{
							iSecondDrop = iPositionIterator;
							break;
						}
					}
					if ( iSecondDrop == -1 )
						iSecondDrop = vecPositions.Count() - 1;

					// Drop the second one at this location
					ParadropSettings_t ps0;
					ps0.m_flTimeToDispatch = GetDangerZoneController()->GetGameTimeOfWaveEnd( 1 ); // end of wave flood
					float flEarlierTime = ( m_arrParadropSettings.Tail().m_flTimeToDispatch + dev_dz_paradrops_arrival_range_min.GetFloat() );
					if ( flEarlierTime < ps0.m_flTimeToDispatch )
					{
						float flLaterTime = ( m_arrParadropSettings.Tail().m_flTimeToDispatch + dev_dz_paradrops_arrival_range_max.GetFloat() );
						ps0.m_flTimeToDispatch = RandomFloat( flEarlierTime, MIN( ps0.m_flTimeToDispatch, flLaterTime ) );
					}
					float flHalfInterval = ( ps0.m_flTimeToDispatch - m_arrParadropSettings.Tail().m_flTimeToDispatch ) / 2;
					ps0.m_flChopperFlightTime = MAX( flHalfInterval, dev_dz_chopper_arrive_time.GetFloat() );
					ps0.m_flTimeToDispatch -= ps0.m_flChopperFlightTime;
					ps0.m_vecLocation = vecPositions[ iSecondDrop ].m_vecPosition;
					m_arrParadropSettings.AddToTail( ps0 );
					vecPositions.FastRemove( iSecondDrop );
				}

				//
				// Cannot drop any more into zone #2 as it will flood
				FOR_EACH_VEC_BACK( vecPositions, iPositionIterator )
				{
					if ( vecPositions[iPositionIterator].m_nIndex < numZone2s )
						vecPositions.FastRemove( iPositionIterator );
				}

				//
				// Two last ones are most remote from the existing ones
				//
				while ( ( vecPositions.Count() > 0  ) && ( m_arrParadropSettings.Count() < numGuaranteedParadrops ) )
				{
					CUtlVector< Vector > vecPointsExisting;
					FOR_EACH_VEC_BACK( m_arrParadropSettings, iPS )
						vecPointsExisting.AddToTail( m_arrParadropSettings[iPS].m_vecLocation );
					
					int iNextDrop = HelperFindMostRemotePosition( vecPointsExisting, vecPositions );
					// Drop the next one at this location
					float flPreviousDropSpawnTime = m_arrParadropSettings.Tail().m_flTimeToDispatch + m_arrParadropSettings.Tail().m_flChopperFlightTime;
					float flEarliestTime = ( flPreviousDropSpawnTime + dev_dz_paradrops_arrival_range_min.GetFloat() );
					float flLatestTime = ( flPreviousDropSpawnTime + dev_dz_paradrops_arrival_range_max.GetFloat() );
					ParadropSettings_t ps0;
					ps0.m_flTimeToDispatch = RandomFloat( flEarliestTime, flLatestTime );
					float flHalfInterval = ( ps0.m_flTimeToDispatch - flPreviousDropSpawnTime ) / 2;
					ps0.m_flChopperFlightTime = MAX( flHalfInterval, dev_dz_chopper_arrive_time.GetFloat() );
					ps0.m_flTimeToDispatch -= ps0.m_flChopperFlightTime;
					ps0.m_vecLocation = vecPositions[ iNextDrop ].m_vecPosition;
					m_arrParadropSettings.AddToTail( ps0 );
					vecPositions.FastRemove( iNextDrop );
				}
			}
		}

		// If it's not time yet for paradrops then don't dispatch random paradrops
		return;
	}

	//
	// Dispatch choppers as scheduled
	//
	if ( m_arrParadropSettings.Count() && gpGlobals->curtime > m_arrParadropSettings.Head().m_flTimeToDispatch )
	{
		ParadropSettings_t const &ps0 = m_arrParadropSettings.Head();
		DispatchParadropChopperTo( ps0.m_vecLocation, NULL, false, ps0.m_flChopperFlightTime );
		m_arrParadropSettings.RemoveMultipleFromHead( 1 );
	}
}

ConVar sv_dz_show_weapon_spawns( "sv_dz_show_weapon_spawns", "0" );
ConVar sv_dz_show_security_door_item_price( "sv_dz_show_security_door_item_price", "0" );
void CSurvivalGameRules::ShowDebugOverlays( void )
{
	if ( sv_dz_show_security_door_item_price.GetBool() )
	{
		FOR_EACH_VEC( IPointDZWeaponSpawn::AutoList(), iPoint )
		{
			CPointDZWeaponSpawn *pSpawnPoint = static_cast<CPointDZWeaponSpawn*>(IPointDZWeaponSpawn::AutoList()[iPoint]);
			if ( pSpawnPoint->GetDoor() )
			{
				NDebugOverlay::Text( pSpawnPoint->GetAbsOrigin() + Vector( 0, 0, 10 ), CFmtStr( "%d", pSpawnPoint->GetPrice() ), false, 0.f );
			}
		}
	}

	if ( sv_dz_show_weapon_spawns.GetBool() )
	{
		float flPulse = 1.0f - fmod( gpGlobals->curtime, 1.0f );
		FOR_EACH_VEC( IPointDZWeaponSpawnGroup::AutoList(), nGroup )
		{
			CPointDZWeaponSpawnGroup *pGroup = static_cast<CPointDZWeaponSpawnGroup*>(IPointDZWeaponSpawnGroup::AutoList()[nGroup]);

			// count how many points have items assigned to them
			int nHasItem = 0;
			FOR_EACH_VEC( pGroup->GetSpawnPoints(), nItem )
			{
				CPointDZWeaponSpawn *pPoint = pGroup->GetSpawnPoints()[nItem];
				if ( pPoint->HasItem() )
				{
					nHasItem++;
				}
			}

			debugoverlay->AddTextOverlay( pGroup->GetAbsOrigin(), 0.1f, "%i", nHasItem );
			FOR_EACH_VEC( pGroup->GetSpawnPoints(), nItem )
			{
				CPointDZWeaponSpawn *pPoint = pGroup->GetSpawnPoints()[nItem];
				if ( pPoint->HasItem() )
				{
					debugoverlay->AddLineOverlay( pPoint->GetAbsOrigin(), pGroup->GetAbsOrigin(), 0, 255 * flPulse, 0, true, 0.1f );
				}
				else
				{
					debugoverlay->AddLineOverlay( pPoint->GetAbsOrigin(), pGroup->GetAbsOrigin(), 150, 150 * flPulse, 150, true, 0.1f );
				}
				NDebugOverlay::Axis( pPoint->GetAbsOrigin(), pPoint->GetAbsAngles(), 5, true, 0.f );
			}
		}
	}

#ifdef DEVELOPMENT_ONLY
	if ( debug_dz_show.GetString()[0] )
	{
		CBaseEntity *pEntity = NULL;
		while ( (pEntity = gEntList.FindEntityByModelPartial( pEntity, debug_dz_show.GetString() )) != NULL )
		{
			if ( pEntity )
			{
				debugoverlay->AddTextOverlay( pEntity->GetAbsOrigin(), 10.0f, "%s", STRING( pEntity->GetModelName() ) );
			}
		}
		debug_dz_show.SetValue( "" );
	}
#endif

}

#endif

void CSurvivalGameRules::OnEachTickThink( void )
{
	EarlyOutIfNotPlayingSurvival;

#ifdef GAME_DLL

	if ( !CSGameRules()->IsWarmupPeriod() && !CSGameRules()->IsFreezePeriod() && GetDangerZoneController() )
	{
		GetDangerZoneController()->UpdateDangerZoneController();
	}

	if ( CSGameRules()->IsWarmupPeriod() )
	{
		NotifySurvivalSquadPartners();
	}

	UpdateSpawnStage();

	DispatchQueuedDrones();

	AutoDropParadrops();

	ShowDebugOverlays();

	UpdateTablets();

	// Always check win conditions via main game rules object
	// which is the only one that can go to intermission correctly
	// and terminate the match and game server, demo file, etc.
	CSGameRules()->CheckWinConditions();

#endif // GAME_DLL

	m_flLastThinkTime = gpGlobals->curtime;
}

#ifdef GAME_DLL
void CSurvivalGameRules::UpdateTablets( void )
{
	FOR_EACH_VEC( ITablet::AutoList(), i )
	{
		CTablet *pTablet = static_cast< CTablet * >( ITablet::AutoList()[i]->GetEntity() );
		pTablet->UpdateTabletExplorationPayout();
		pTablet->UpdatePlayerPositionHistory();
	}
}

void CSurvivalGameRules::StartSurvival( void )
{
	InitItemSpawnLocations();
	GameStartSpawnAllItems();
	PostItemSpawn();
	ResetDangerZone();
	SpawnAllPlayers();

	m_flSurvivalStartTime = gpGlobals->curtime;
}

// only jitter hexes half as much in x direction to try to match width/height as much as possible
// (should help avoid some hexes being very far off of playable area)
DEVELOPMENT_ONLY_CONVAR( dz_dev_tablet_jitter_hexes_x, 0.5 );
DEVELOPMENT_ONLY_CONVAR( dz_dev_tablet_jitter_hexes_y, 1 );
DEVELOPMENT_ONLY_CONVAR( dz_dev_tablet_jitter_force_x, -1 );
DEVELOPMENT_ONLY_CONVAR( dz_dev_tablet_jitter_force_y, -1 );

DEVELOPMENT_ONLY_CONVAR( dz_hex_use_world_bounds, 0 );
DEVELOPMENT_ONLY_CONVAR( dz_hex_inflate_bounds, 1100 );

void CSurvivalGameRules::InitGlobalTablet( void )
{
	// Get size of map.
	float flWidth, flHeight;
	Vector vecWorldCenter;

	if ( dz_hex_use_world_bounds.GetBool() )
	{
		GetWorldWidthAndHeight( flWidth, flHeight, vecWorldCenter );
	}
	else
	{
		AABB_t aabb = GetPlayAreaBounds();
		flWidth = aabb.GetSize().x;
		flHeight = aabb.GetSize().y;
		vecWorldCenter = aabb.GetCenter();
	}

	flWidth += dz_hex_inflate_bounds.GetFloat();
	flHeight += dz_hex_inflate_bounds.GetFloat();

	// Get hex size (distance between hex centers)
	//
	// Deflate hex count to account for the fact that we don't want any part of the world to not be inside a hex
	// so we need to account for the half-hex horizontally / pointy tops/bottoms vertically which can leave some
	// space outside the playable area
	//
	// Also deflate to account by the maximum amount of shifting we may do
	float flHexCountX = WORLD_HEX_WIDTH - kHexInflateX - dz_dev_tablet_jitter_hexes_x.GetFloat();
	float flHexCountY = WORLD_HEX_HEIGHT - kHexInflateY - dz_dev_tablet_jitter_hexes_y.GetFloat();

	float flHexSizeFromWidth = flWidth / flHexCountX;
	float flHexSizeFromHeight = flHeight / ( kHexHeight * flHexCountY );
	float flHexSize = MAX( flHexSizeFromWidth, flHexSizeFromHeight );

	// Calculate origin of hex grid
	float flTotalWidth = flHexSize * flHexCountX;
	float flTotalHeight = flHexSize * kHexHeight * flHexCountY;

	float flOriginX = vecWorldCenter.x - flTotalWidth / 2;
	float flOriginY = vecWorldCenter.y - flTotalHeight / 2;

	// move hexes so the centers are inside the world
	// We offset less in Y direction to make sure that pointy-top/bottom areas don't allow players
	// to escape the hex grid inside the playable area.
	//flOriginX += flHexSize * 0.5f; // since even rows are offset by +1/2 hex, we actually don't want to move x here
	flOriginY += flHexSize * kHexHeight * (1.0f / 3.0f);

	// jitter the tablet grid
	float jitterX = ( dz_dev_tablet_jitter_force_x.GetFloat() >= 0 ) ? dz_dev_tablet_jitter_force_x.GetFloat() : RandomFloat( 0, 1 );
	float jitterY = ( dz_dev_tablet_jitter_force_y.GetFloat() >= 0 ) ? dz_dev_tablet_jitter_force_y.GetFloat() : RandomFloat( 0, 1 );

	flOriginX -= jitterX * dz_dev_tablet_jitter_hexes_x.GetFloat() * flHexSize;
	flOriginY -= jitterY * dz_dev_tablet_jitter_hexes_y.GetFloat() * flHexSize * kHexHeight;

	m_flTabletHexOriginX = flOriginX;
	m_flTabletHexOriginY = flOriginY;
	m_flTabletHexSize = flHexSize;
}

void CSurvivalGameRules::InitItemSpawnLocations( void )
{
	// remove some random exploding barrels
	if ( ICCSPropExplodingBarrel::AutoList().Count() > 1 )
	{
		CUtlVector<ICCSPropExplodingBarrel*> vecExplodingBarrels;
		vecExplodingBarrels.AddVectorToTail( ICCSPropExplodingBarrel::AutoList() );

		vecExplodingBarrels.Shuffle();

		int nDesiredBarrelCount = MAX( 0, RandomInt( dev_dz_exploding_barrel_count_min.GetInt(), dev_dz_exploding_barrel_count_max.GetInt() ) );

		int nNumBarrelsToRemove = vecExplodingBarrels.Count() - nDesiredBarrelCount;

		for ( int i = 0; i < nNumBarrelsToRemove; i++ )
		{
			UTIL_Remove( vecExplodingBarrels[i]->GetEntity() );	
			vecExplodingBarrels[i] = NULL;
		}
	}

	FOR_EACH_VEC( IPointDZDroneGunSpawn::AutoList(), i )
	{
		CPointDZDroneGunSpawn *pDroneGunSpawn = static_cast< CPointDZDroneGunSpawn* >(IPointDZDroneGunSpawn::AutoList()[i]);
		pDroneGunSpawn->SpawnDroneGun();
	}

	FOR_EACH_VEC( IPointDZParachuteSpawn::AutoList(), i )
	{
		CBaseEntity *pEntParachute = CBaseEntity::Create( "prop_weapon_upgrade_chute", IPointDZParachuteSpawn::AutoList()[i]->GetEntity()->GetAbsOrigin(), vec3_angle, NULL );
		if ( pEntParachute )
			pEntParachute->m_bEligibleForScreenHighlight = true;
		Assert( pEntParachute );
	}

	DZ_ConsoleMsg( "Map contains %i weapon spawn locations in %i groups.\n", IPointDZWeaponSpawn::AutoList().Count(), IPointDZWeaponSpawnGroup::AutoList().Count() );

	// associate spawns with groups
	FOR_EACH_VEC( IPointDZWeaponSpawnGroup::AutoList(), nGroup )
	{
		CPointDZWeaponSpawnGroup *pGroup = static_cast< CPointDZWeaponSpawnGroup* >( IPointDZWeaponSpawnGroup::AutoList()[nGroup] );

		float flGroupRadiusSqr = Square( pGroup->GetRadius() );

		FOR_EACH_VEC( IPointDZWeaponSpawn::AutoList(), nPoint )
		{
			CPointDZWeaponSpawn *pPoint = static_cast< CPointDZWeaponSpawn* >( IPointDZWeaponSpawn::AutoList()[ nPoint ] );

			// check for unassigned points
			if ( !pPoint->HasSpawnGroup() )
			{
				// is the point inside this sphere
				if ( pPoint->GetAbsOrigin().DistToSqr( pGroup->GetAbsOrigin() ) <= flGroupRadiusSqr )
				{
					pPoint->AssignSpawnGroup( pGroup );
					pGroup->AddSpawnPoint( pPoint );
				}
			}
			// point is already assigned to this group in a map, add this point to the group
			else if ( pPoint->GetSpawnGroup() == pGroup )
			{
				pGroup->AddSpawnPoint( pPoint );
			}
		}

		//DZ_ConsoleMsg( "Group %i contains %i spawns.\n", nGroup, pGroup->m_vecItemSpawns.Count() );
	}

	// associate isolated spawns with their nearest group
	FOR_EACH_VEC( IPointDZWeaponSpawn::AutoList(), nPoint )
	{
		CPointDZWeaponSpawn *pPoint = static_cast< CPointDZWeaponSpawn* >( IPointDZWeaponSpawn::AutoList()[ nPoint ] );

		if ( !pPoint->HasSpawnGroup() )
		{
			float flClosestDist = FLT_MAX;
			CPointDZWeaponSpawnGroup *pClosestGroup = NULL;

			FOR_EACH_VEC( IPointDZWeaponSpawnGroup::AutoList(), nGroup )
			{
				CPointDZWeaponSpawnGroup *pGroup = static_cast< CPointDZWeaponSpawnGroup* >( IPointDZWeaponSpawnGroup::AutoList()[nGroup] );

				float flContenderDist = pGroup->GetAbsOrigin().DistToSqr( pPoint->GetAbsOrigin() );

				if ( pClosestGroup == NULL || flContenderDist < flClosestDist )
				{
					flClosestDist = flContenderDist;
					pClosestGroup = pGroup;
				}
			}

			if ( pClosestGroup )
			{
				pPoint->AssignSpawnGroup( pClosestGroup );
				pClosestGroup->AddSpawnPoint( pPoint );
			}
		}
	}
}

DEVELOPMENT_ONLY_CONVAR( sv_survival_item_distribution_grid_size, 10 );
void CSurvivalGameRules::SetupSpawnPoints( void )
{
	
	ConvertSafeSpawnsToItemSpawns();

	// group points by sections of the map
	CUtlVector< int > vecSpawnPointGridIndex;
	CUtlVector< int > vecGridSpawnPointCount;
	bool bSubDiv = sv_survival_item_distribution_grid_size.GetInt() > 0;
	if ( bSubDiv )
	{
		AABB_t playArea = GetPlayAreaBounds();
		const int nGridSize = sv_survival_item_distribution_grid_size.GetInt();
		const int nTotalGrids = Square( nGridSize );
		vecGridSpawnPointCount.EnsureCount( nTotalGrids );
		vecGridSpawnPointCount.FillWithValue( 0 );
		
		vecSpawnPointGridIndex.EnsureCount( IPointDZWeaponSpawn::AutoList().Count() );
		vecSpawnPointGridIndex.FillWithValue( vecSpawnPointGridIndex.InvalidIndex() );

		Vector vecMapSize = playArea.GetSize();
		const float flRegionW = vecMapSize.x / nGridSize;
		const float flRegionH = vecMapSize.y / nGridSize;

		FOR_EACH_VEC( IPointDZWeaponSpawn::AutoList(), iPoint )
		{
			CBaseEntity *pEnt = IPointDZWeaponSpawn::AutoList()[iPoint]->GetEntity();
			int nGridX = int( ( pEnt->GetAbsOrigin().x - playArea.m_vMinBounds.x ) / flRegionW );
			int nGridY = int( ( pEnt->GetAbsOrigin().y - playArea.m_vMinBounds.y ) / flRegionH );
			
			int nRegionIndex = nGridY * nGridSize + nGridX;
			vecSpawnPointGridIndex[ iPoint ] = nRegionIndex;
			vecGridSpawnPointCount[ nRegionIndex ]++;
		}

#ifdef DEVELOPMENT_ONLY
		DevMsg( "SURVIVAL: item distribution grid\n" );
		for ( int y=0; y<nGridSize; ++y )
		{
			for ( int x=0; x<nGridSize; ++x )
			{
				int nGridIndex = y * nGridSize + x;
				DevMsg( "%d\t", vecGridSpawnPointCount[nGridIndex] );
			}
			DevMsg( "\n" );
		}
#endif // DEVELOPMENT_ONLY
	}

	// reset all doors
	FOR_EACH_VEC( IDZDoor::AutoList(), iDoor )
	{
		CDZDoor *pDoor = static_cast< CDZDoor* >( IDZDoor::AutoList()[ iDoor ] );
		pDoor->Reset();
	}
	
	// make sure all spawn points are on the ground
	FOR_EACH_VEC( IPointDZWeaponSpawn::AutoList(), iPoint )
	{
		CPointDZWeaponSpawn *pPoint = static_cast<CPointDZWeaponSpawn*>( IPointDZWeaponSpawn::AutoList()[ iPoint ] );

		pPoint->Reset();

		// apply grid scale if needed
		if ( bSubDiv )
		{
			int nGridIndex = vecSpawnPointGridIndex[ iPoint ];
			float flWeightScale = 1.f / vecGridSpawnPointCount[ nGridIndex ];
			pPoint->ApplyWeightScale( flWeightScale );
		}

		// trace to ground
		trace_t tr;
		UTIL_TraceLine( pPoint->GetAbsOrigin(), pPoint->GetAbsOrigin() - Vector( 0, 0, 64 ), MASK_SOLID, NULL, COLLISION_GROUP_NONE, &tr );
		if ( tr.DidHit() )
		{
			pPoint->SetAbsOrigin( tr.endpos );
		}

		// pairing with door if needed
		if ( pPoint->GetDoorName() != NULL_STRING )
		{
			const char *pszSearchDoorName = STRING( pPoint->GetDoorName() );
			if ( pszSearchDoorName && *pszSearchDoorName )
			{
				FOR_EACH_VEC( IDZDoor::AutoList(), iDoor )
				{
					CDZDoor *pDoor = static_cast< CDZDoor* >( IDZDoor::AutoList()[ iDoor ] );
					const char *pszEntName = pDoor->GetEntityNameAsCStr();
					if ( FStrEq( pszSearchDoorName, pszEntName ) )
					{
						pPoint->AssignDoor( pDoor );
						pPoint->ApplyWeightScale( dev_dz_security_door_weight_scale.GetFloat() ); // scale weight of points behind security door
						pDoor->AssignSpawnPoint( pPoint );
						break;
					}
				}
			}
		}
	}
}

void CSurvivalGameRules::ReloadConfigFile( void )
{
	CBrConfig::Free( m_pBrConfig );
	m_pBrConfig = CBrConfig::Load( SURVIVAL_CONFIG_FILE, this );
	AssertFatal( m_pBrConfig );
}

void CSurvivalGameRules::OnConfigRandomDecisionMade( int nType, int nValue )
{
	if ( nType <= 0 || nType >= k_ESurvivalGameRuleDecision_TotalCount )
		return;

	// Find the entry for this decision type
	int iVacantSlot = -1;
	for ( int jj = 0; jj < SURVIVAL_GAME_RULES_DECISION_TYPES_NETWORK_TOTAL_MAX; ++ jj )
	{
		if ( m_SurvivalGameRuleDecisionTypes[ jj ] == k_ESurvivalGameRuleDecision_Unknown )
		{
			if ( iVacantSlot < 0 )
				iVacantSlot = jj;
		}
		else if ( m_SurvivalGameRuleDecisionTypes[ jj ] == nType )
		{
			m_SurvivalGameRuleDecisionValues.Set( jj, nValue );
			return;
		}
	}

	// If we haven't found an entry then insert into an empty slot
	Assert( iVacantSlot >= 0 );
	if ( iVacantSlot >= 0 )
	{
		m_SurvivalGameRuleDecisionTypes.Set( iVacantSlot, ( ESurvivalGameRuleDecision_t ) nType );
		m_SurvivalGameRuleDecisionValues.Set( iVacantSlot, nValue );
	}
}

#ifdef DEVELOPMENT_ONLY
static bool s_bDevOnlyVarLoggingItemDistribution = false;
#define DEV_ONLY_IS_LOGGING_ITEM_DISTRIBUTION() s_bDevOnlyVarLoggingItemDistribution
CUtlBuffer g_bufSpawnLog( 0, 0, CUtlBuffer::TEXT_BUFFER );
CUtlVector< bool > g_vecLogHasItem;
CUtlVector< bool > g_vecLogHasItemEver;
void CleanUpItemLog()
{
	// clear everything
	g_bufSpawnLog.Clear();
	g_vecLogHasItem.RemoveAll();
	g_vecLogHasItemEver.RemoveAll();
}

void LogItemSpawn( CPointDZWeaponSpawn *pSpawnPoint, const char *pszItemName )
{
	int nPointIndex = IPointDZWeaponSpawn::AutoList().Find( pSpawnPoint );
	Assert( nPointIndex != IPointDZWeaponSpawn::AutoList().InvalidIndex() );
	Assert( !g_vecLogHasItem[ nPointIndex ] );
	g_vecLogHasItem[ nPointIndex ] = true;
	g_vecLogHasItemEver[ nPointIndex ] = true;
	g_bufSpawnLog.Printf( "Spawned: [%s] at [%.0f, %.0f, %.0f]\n", pszItemName, XYZ( pSpawnPoint->GetAbsOrigin() ) );
}

CON_COMMAND( survival_log_item_distributions, "log number of item distributions" )
{
	if ( IPointDZWeaponSpawn::AutoList().Count() == 0 )
	{
		Warning( "survival_log_item_distributions failed: make sure you're in a map with item spawn points\n" );
		return;
	}

	float flStart = Plat_FloatTime();

	// make sure we log at least 1
	int nLog = 1;
	if ( args.ArgC() > 1 )
		nLog = MAX( 1, atoi( args[1] ) );

	CleanUpItemLog();
	
	// alloc has item for points
	g_vecLogHasItem.EnsureCount( IPointDZWeaponSpawn::AutoList().Count() );
	g_vecLogHasItemEver.EnsureCount( IPointDZWeaponSpawn::AutoList().Count() );
	g_vecLogHasItemEver.FillWithValue( false );

	s_bDevOnlyVarLoggingItemDistribution = true;

	for ( int i=0; i<nLog; ++i )
	{
		g_vecLogHasItem.FillWithValue( false );
		CSGameRules()->GetSurvivalRules()->GameStartSpawnAllItems();
	}

	char logPath[MAX_PATH];
	g_pFullFileSystem->RelativePathToFullPath( CFmtStr( "survival_spawnlog_%d.txt", nLog ).Access(), "MOD", logPath, sizeof( logPath ) );
	g_pFullFileSystem->WriteFile( logPath, "MOD", g_bufSpawnLog );

	s_bDevOnlyVarLoggingItemDistribution = false;

	

	int nTotalPointUsed = 0;
	FOR_EACH_VEC( g_vecLogHasItemEver, i )
	{
		if ( g_vecLogHasItemEver[i] )
			nTotalPointUsed++;
	}

	float flEnd = Plat_FloatTime();
	DevMsg( "survival_log_item_distributions took %fs to generate %d item distribution with %d/%d points used\n", flEnd - flStart, nLog, nTotalPointUsed, IPointDZWeaponSpawn::AutoList().Count() );

	CleanUpItemLog();
}
#else
#define DEV_ONLY_IS_LOGGING_ITEM_DISTRIBUTION() false
#endif

void CSurvivalGameRules::GameStartSpawnAllItems( void )
{
	if ( !m_pBrConfig )
	{
		Assert( false );
		DZ_ConsoleMsg( "Can't spawn anything with no config loaded.\n" );
		return;
	}

	SetupSpawnPoints();

	float flStartTime = Plat_FloatTime();

	CUtlVector< IPointDZWeaponSpawn* > vecCurrentValidSpawnPoints;
	const auto &vecGameStartItems = m_pBrConfig->GetGameStartItems();

	ConvertSafeSpawnsToItemSpawns();

	int nNumTotalSpawnItems = 0;

	float flHostageMinDistSqr = Square( dev_dz_hostage_min_dist.GetFloat() );

	typedef CUtlVector< int > VecItemFromGroup_t;
	CUtlMap< int, VecItemFromGroup_t* > mapPriorityGroup;
	mapPriorityGroup.SetLessFunc( DefLessFunc( int ) );
	FOR_EACH_VEC( vecGameStartItems, iGroup )
	{
		const CBrConfig::GameStartItem_t& group = vecGameStartItems[ iGroup ];
		int iIndex = mapPriorityGroup.Find( group.m_nPriority );
		if ( iIndex == mapPriorityGroup.InvalidIndex() )
		{
			iIndex = mapPriorityGroup.Insert( group.m_nPriority, new VecItemFromGroup_t );
		}

		VecItemFromGroup_t vecSpawnGroup;
		vecSpawnGroup.EnsureCount( group.m_nQuantity );
		vecSpawnGroup.FillWithValue( iGroup );

		mapPriorityGroup[iIndex]->AddVectorToTail( vecSpawnGroup );
	}

	FOR_EACH_MAP( mapPriorityGroup, iMap )
	{
		int nPriority = mapPriorityGroup.Key( iMap );
		VecItemFromGroup_t &vecItemFromGroup = *mapPriorityGroup[ iMap ];
		Assert( vecItemFromGroup.Count() > 0 );
		
		if ( !dev_dz_random_safe_locations.GetBool() && FStrEq( vecGameStartItems[vecItemFromGroup[0]].m_pszName, "safes" ) )
		{
			continue; // don't spawn random safes if this feature is disabled
		}
		
		vecItemFromGroup.Shuffle();

		// find valid points to spawn next set of items
		auto lambdaIsPointTooCloseToResqZone = [&]( const CPointDZWeaponSpawn *pPoint ) -> bool
		{
			FOR_EACH_VEC( IHostageRescueZone::AutoList(), nHostageResq )
			{
				const Vector &vecHostageResqLocation = IHostageRescueZone::AutoList()[nHostageResq]->GetEntity()->WorldSpaceCenter();
				if ( pPoint->GetAbsOrigin().DistToSqr( vecHostageResqLocation ) < flHostageMinDistSqr )
				{
					return true;
				}
			}

			return false;
		};

		bool bHostage = FStrEq( vecGameStartItems[ vecItemFromGroup[0] ].m_pszName, "hostages" );
#ifdef DEVELOPMENT_ONLY
		if ( bHostage )
		{
			// we're assuming that hostage only spawns with other hostages
			FOR_EACH_VEC( vecItemFromGroup, iItem )
			{
				Assert( FStrEq( vecGameStartItems[ vecItemFromGroup[ iItem ] ].m_pszName, "hostages" ) );
			}
		}
#endif // DEVELOPMENT_ONLY

		// Spawns selected already for existing items
		CUtlVector< CPointDZWeaponSpawn * > arrSelectedPointSpawnLocations;

		auto lambdaGenerateValidSpawnPoints = [&]()
		{
			arrSelectedPointSpawnLocations.RemoveAll();
			vecCurrentValidSpawnPoints.RemoveAll();
			FOR_EACH_VEC( IPointDZWeaponSpawn::AutoList(), nPoint )
			{
				CPointDZWeaponSpawn *pPoint = static_cast< CPointDZWeaponSpawn* >( IPointDZWeaponSpawn::AutoList()[ nPoint ] );

				// already spawned an item
				if ( pPoint->HasItem() )
					continue;

				if ( DEV_ONLY_IS_LOGGING_ITEM_DISTRIBUTION() )
				{
#if DEVELOPMENT_ONLY
					if ( g_vecLogHasItem[ nPoint ] )
						continue;
#endif
				}

				// make sure we spawn hostages away from resq zone for some units
				// only add points that're outside resq zone area
				if ( bHostage && lambdaIsPointTooCloseToResqZone( pPoint ) )
					continue;

				vecCurrentValidSpawnPoints.AddToTail( pPoint );
			}
			Assert( vecCurrentValidSpawnPoints.Count() > 0 );
		};
		lambdaGenerateValidSpawnPoints();

		// abort if we can't spawn this many
		if ( vecItemFromGroup.Count() > vecCurrentValidSpawnPoints.Count() )
		{
			Warning( "Trying to spawn %d of items in priority %d, but there's only %d spawn points left\n", vecItemFromGroup.Count(), nPriority, vecCurrentValidSpawnPoints.Count() );
			Assert( !"Too many entities! Add more spawn points!" );
			return;
		}

		auto lambdaRemoveAllPointsInRadius = [&]( int nGroupID, const Vector &vecPos, float flWeightRadiusSqr )
		{
			FOR_EACH_VEC_BACK( vecCurrentValidSpawnPoints, nPoint )
			{
				CPointDZWeaponSpawn *pPoint = static_cast< CPointDZWeaponSpawn* >( vecCurrentValidSpawnPoints[ nPoint ] );
				
				// only apply weight scale to points in the same group ID
				if ( nGroupID != pPoint->GetGroupID() )
					continue;

				float flDistSqr = pPoint->GetAbsOrigin().DistToSqr( vecPos );
				if ( flDistSqr < flWeightRadiusSqr )
				{	
					vecCurrentValidSpawnPoints.Remove( nPoint );
				}
			}
		};

		auto lambdaGetSpawnLocation = [&]( const CBrConfig::GameStartItem_t& group ) -> CPointDZWeaponSpawn*
		{
			float flWeightRadiusSqr = Square( group.m_flWeightRadius );
			CUtlVector< IPointDZWeaponSpawn* > vecForThisTryValidSpawnPoints;
			vecForThisTryValidSpawnPoints.EnsureCapacity( vecCurrentValidSpawnPoints.Count() );

			float flTotalWeight = 0.f;
			FOR_EACH_VEC( vecCurrentValidSpawnPoints, nPoint )
			{
				CPointDZWeaponSpawn *pPoint = static_cast< CPointDZWeaponSpawn* >( vecCurrentValidSpawnPoints[ nPoint ] );
				bool bEnabled = true;
				FOR_EACH_VEC( arrSelectedPointSpawnLocations, iAlreadySelected )
				{
					if ( ( arrSelectedPointSpawnLocations[iAlreadySelected]->GetGroupID() == pPoint->GetGroupID() ) &&
						( pPoint->GetAbsOrigin().DistToSqr( arrSelectedPointSpawnLocations[ iAlreadySelected ]->GetAbsOrigin() ) < flWeightRadiusSqr ) )
					{
						bEnabled = false;
						break;
					}
				}
				if ( !bEnabled ) continue;

				vecForThisTryValidSpawnPoints.AddToTail( pPoint );
				flTotalWeight += pPoint->GetCurrentWeight();
			}

			// make sure we have valid points to spawn item
			if ( vecForThisTryValidSpawnPoints.Count() == 0 )
			{
				Warning( "Spawn group '%s' ran out of valid spawn points and has to reuse points within radius of some items spawning from this group\n", group.m_pszName );
				Assert( false );
			}

			float flRandomWeight = RandomFloat( 0.f, flTotalWeight );
			FOR_EACH_VEC( vecForThisTryValidSpawnPoints, nPoint )
			{
				CPointDZWeaponSpawn *pPoint = static_cast< CPointDZWeaponSpawn* >( vecForThisTryValidSpawnPoints[ nPoint ] );
				flRandomWeight -= pPoint->GetCurrentWeight();
				if ( flRandomWeight < 0.f )
				{
					// remove all points in radius
					// bring them back when we run out of points to spawn item from this group
					// this is to try to give item in the same group a min distance from each other
					lambdaRemoveAllPointsInRadius( pPoint->GetGroupID(), pPoint->GetAbsOrigin(), flWeightRadiusSqr );

					return pPoint;
				}
			}

			return NULL;
		};

		// keep track of entities spawned from this group
		CUtlVector< CBaseEntity* > vecSpawnEntities;
		FOR_EACH_VEC( vecItemFromGroup, iItem )
		{
			int iGroup = vecItemFromGroup[ iItem ];
			const CBrConfig::GameStartItem_t& group = vecGameStartItems[ iGroup ];

			CPointDZWeaponSpawn *pSpawnPoint = lambdaGetSpawnLocation( group );
			if ( pSpawnPoint )
			{
				arrSelectedPointSpawnLocations.AddToTail( pSpawnPoint );

				if ( DEV_ONLY_IS_LOGGING_ITEM_DISTRIBUTION() )
				{
#if DEVELOPMENT_ONLY
					const char *pszItemName = group.m_content.IsCrate() ? group.m_content.m_pCrate->m_pszName : group.m_content.m_pLootList->RandomItemList()->m_Items[0].m_pBaseItem->m_pszEntityName;
					LogItemSpawn( pSpawnPoint, pszItemName );
#endif
				}
				else
				{
					Assert( pSpawnPoint->GetAbsOrigin().IsValid() );

					CBaseEntity *pEnt = NULL;
					int nPrice = 0;
					if ( group.m_content.IsCrate() )
					{
						CUtlVector< CBaseEntity* > vecNewEntities;
						SpawnItemFromContent( pSpawnPoint->GetAbsOrigin(), pSpawnPoint->GetAbsAngles(), &group.m_content, NULL, &vecNewEntities );
						// there should only be one item
						Assert( vecNewEntities.Count() == 1 );
						pEnt = vecNewEntities[0];

						nPrice = group.m_content.GetPrice();
					}
					else
					{
						const CBrConfig::LootList_t::ItemList_t *pItemList = group.m_content.m_pLootList->RandomItemList();
						// if the item list has multiple items, we should put that in a crate
						Assert( pItemList->m_Items.Count() == 1 );

						const auto pItem = &pItemList->m_Items[0];
						const char *pszClassName = pItem->m_pBaseItem->m_pszEntityName;
						const int nAmmo = pItem->GetAmmo();

						pEnt = SpawnItem( pSpawnPoint->GetAbsOrigin(), pSpawnPoint->GetAbsAngles(), pszClassName, nAmmo );
						Assert( pEnt );
						if ( pEnt )
						{
							CBaseAnimating *pEntAnim = pEnt->GetBaseAnimating();
							if ( pEntAnim )
								pEntAnim->SetHighlightColor( pItem->m_pBaseItem->m_cHighlightColor.r(), pItem->m_pBaseItem->m_cHighlightColor.g(), pItem->m_pBaseItem->m_cHighlightColor.b() );
						}

						nPrice = pItem->m_pBaseItem->m_nSecurityDoorValue;
					}
					
					pSpawnPoint->AssignItem( pEnt, nPrice );
					vecSpawnEntities.AddToTail( pEnt );
				}
			}
			else
			{
				Assert( !"Failed to GetSpawnLocation" );
			}
		}

		if ( !DEV_ONLY_IS_LOGGING_ITEM_DISTRIBUTION() )
		{
			float flMinDistanceBetweenItemsFromThisGroup = FLT_MAX;
			FOR_EACH_VEC( vecSpawnEntities, i )
			{
				for ( int j=i+1; j<vecSpawnEntities.Count(); ++j )
				{
					float flDistToSqr = vecSpawnEntities[i]->GetAbsOrigin().DistToSqr( vecSpawnEntities[j]->GetAbsOrigin() );
					if ( flDistToSqr < flMinDistanceBetweenItemsFromThisGroup )
					{
						flMinDistanceBetweenItemsFromThisGroup = flDistToSqr;
					}
				}
			}
			nNumTotalSpawnItems += vecSpawnEntities.Count();

			DZ_ConsoleMsg( "Min dist for items in priority %d is %.2f\n", nPriority, sqrtf( flMinDistanceBetweenItemsFromThisGroup ) );
		}
	} // for priority

	if ( !DEV_ONLY_IS_LOGGING_ITEM_DISTRIBUTION() )
	{
		DZ_ConsoleMsg( "spawned %d entities total.\n", nNumTotalSpawnItems );
	}

	mapPriorityGroup.PurgeAndDeleteElements();

	DevMsg( "Survival: GameStartSpawnAllItems took %f\n", Plat_FloatTime() - flStartTime );
}


void CSurvivalGameRules::PostItemSpawn()
{
	// setup all doors
	FOR_EACH_VEC( IDZDoor::AutoList(), iDoor )
	{
		CDZDoor *pDoor = static_cast< CDZDoor* >( IDZDoor::AutoList()[ iDoor ] );
		pDoor->SetupDoor();
	}
}

#ifdef DEVELOPMENT_ONLY

CON_COMMAND( sv_dz_spawn_item, "spawn a specific item at the nearest spawn point to local player's lookat position" )
{
	if ( args.ArgC() > 1 )
	{
		CBasePlayer *pPlayer = UTIL_GetCommandClient();
		if ( !pPlayer )
			return;

		CSurvivalGameRules* pBRrules = CSGameRules()->GetSurvivalRules();
		if ( !pBRrules )
			return;

		const CBrConfig::BaseItem_t *pItem = pBRrules->GetConfig()->GetBaseItem( args[1] );
		if ( !pItem )
		{
			Warning( "Invalid item '%s'", args[1] );
			return;
		}

		trace_t tr;

		Vector forward;
		pPlayer->EyeVectors( &forward );
		UTIL_TraceLine( pPlayer->EyePosition(), pPlayer->EyePosition() + forward * MAX_COORD_RANGE, MASK_SOLID, pPlayer, COLLISION_GROUP_NONE, &tr );
		if ( tr.DidHit() )
		{
			float flDebugDuration = 3.f;

			Vector vecSpawnLocation;
			QAngle angSpawnAngle;

			GetClosestItemSpawnPositionTo( tr.endpos, &vecSpawnLocation, &angSpawnAngle );

			NDebugOverlay::Axis( tr.endpos, vec3_angle, 2.f, true, flDebugDuration );
			NDebugOverlay::HorzArrow( tr.endpos, vecSpawnLocation, 2, 255, 0, 0, 0, true, flDebugDuration );
			NDebugOverlay::Axis( vecSpawnLocation, vec3_angle, 2.f, true, flDebugDuration );

			CBaseEntity *pEnt = pBRrules->SpawnItem( vecSpawnLocation, angSpawnAngle, pItem->m_pszEntityName, pItem->m_nDefaultAmmo );

			if ( pEnt )
			{
				CBaseAnimating *pEntAnim = pEnt->GetBaseAnimating();
				if ( pEntAnim )
					pEntAnim->SetHighlightColor( pItem->m_cHighlightColor.r(), pItem->m_cHighlightColor.g(), pItem->m_cHighlightColor.b() );
			}

			Assert( pEnt );
		}
	}
}

CON_COMMAND( sv_dz_spawn_content, "spawn a specific content at the nearest spawn point to local player's lookat position" )
{
	if ( args.ArgC() > 1 )
	{
		CBasePlayer *pPlayer = UTIL_GetCommandClient();
		if ( !pPlayer )
			return;

		CSurvivalGameRules* pBRrules = CSGameRules()->GetSurvivalRules();
		if ( !pBRrules )
			return;

		CBrConfig::Content_t content;
		if ( !pBRrules->GetConfig()->GetContent( &content, args[1] ) )
		{
			Warning( "Invalid item '%s'", args[1] );
			return;
		}

		trace_t tr;

		Vector forward;
		pPlayer->EyeVectors( &forward );
		UTIL_TraceLine( pPlayer->EyePosition(), pPlayer->EyePosition() + forward * MAX_COORD_RANGE, MASK_SOLID, pPlayer, COLLISION_GROUP_NONE, &tr );
		if ( tr.DidHit() )
		{
			float flDebugDuration = 3.f;

			Vector vecSpawnLocation;
			QAngle angSpawnAngle;

			GetClosestItemSpawnPositionTo( tr.endpos, &vecSpawnLocation, &angSpawnAngle );

			NDebugOverlay::Axis( tr.endpos, vec3_angle, 2.f, true, flDebugDuration );
			NDebugOverlay::HorzArrow( tr.endpos, vecSpawnLocation, 2, 255, 0, 0, 0, true, flDebugDuration );
			NDebugOverlay::Axis( vecSpawnLocation, vec3_angle, 2.f, true, flDebugDuration );

			CUtlVector< CBaseEntity* > vecOutput;
			pBRrules->SpawnItemFromContent( vecSpawnLocation, angSpawnAngle, &content, NULL, &vecOutput );
			Assert( vecOutput.Count() > 0 );
		}
	}
}

CON_COMMAND( sv_dz_check_spawn_points, "Check all spawn point positions and show ones that have invalid position" )
{
	int nBadSpawnPoints = 0;
	FOR_EACH_VEC( IPointDZWeaponSpawn::AutoList(), n )
	{
		const Vector& vecPos = IPointDZWeaponSpawn::AutoList()[n]->GetEntity()->GetAbsOrigin();
		trace_t tr;
		UTIL_TraceLine( vecPos, vecPos, MASK_SOLID, NULL, COLLISION_GROUP_NONE, &tr );
		if ( tr.startsolid )
		{
			Warning( "Bad item spawn point [%f %f %f]\n", XYZ( vecPos ) );
			NDebugOverlay::Text( vecPos + Vector( 0, 0, 10 ), "Bad item spawn point", true, 30.f );
			nBadSpawnPoints++;
		}
	}

	if ( nBadSpawnPoints > 0 )
	{
		Warning( "Found %d bad item spawn point(s)\n", nBadSpawnPoints );
	}
}

#endif // DEVELOPMENT_ONLY

bool CSurvivalGameRules::PlayerSelectHex( int playerEntIndex, int hexID )
{
	// returns true if select succeeded.

	int iArrayIndex = playerEntIndex - 1;
	Assert( iArrayIndex >= 0 && iArrayIndex < MAX_PLAYERS );
	
	// Check the player's current selected hex
	int selectedHex = m_iPlayerSpawnHexIndices[iArrayIndex];
	if ( selectedHex >= 0 )
	{
		Assert( selectedHex < GetNumSpawnHex() );

		switch ( m_SpawnTileState.Get( selectedHex ) )
		{
		case kSurvivalSpawn_Occupied:
			break;

		case kSurvivalSpawn_TempLocked:
		case kSurvivalSpawn_Locked:
			// player isn't allowed to change state
			return false;

		default:
			return false; // currently selected hex is invalid somehow?
		}
	}

	// Only let them select a new hex if they are allowed to do so
	if ( !CanSelectHex( hexID ) )
		return false;

	CBasePlayer *pPlayer = UTIL_PlayerByIndex( playerEntIndex );
	if ( pPlayer )
	{
		//Send three separate audio cues, to the player themselves, the player's team mates,
		//and enemy players.
		{
			CSingleUserRecipientFilter filter( pPlayer );
			pPlayer->EmitSound( filter, pPlayer->entindex(), "Survival.SelectDropLocation" );
		}

		CCSPlayer* pCSPlayer = dynamic_cast<CCSPlayer*>( pPlayer );


		CUtlVector<CCSPlayer*> playerTeamMates;

		if ( pCSPlayer != nullptr )
		{
			GetPlayerTeammates( pCSPlayer, playerTeamMates );
		}

		if ( playerTeamMates.Count() > 0 )
		{
			CRecipientFilter filter;
			FOR_EACH_VEC( playerTeamMates, i )
			{
				filter.AddRecipient( playerTeamMates[i] );
			}

			pPlayer->EmitSound(filter, pPlayer->entindex(), "Survival.ZoneChosenByFriend");
		}

		{
			CBroadcastRecipientFilter filter;
			filter.RemoveRecipient(pPlayer);
			FOR_EACH_VEC(playerTeamMates, i)
			{
				filter.RemoveRecipient(playerTeamMates[i]);
			}

			pPlayer->EmitSound( filter, pPlayer->entindex(), "Survival.ZoneChosenByOther" );
		}
	}

	// Clear old selection (if any)
	if ( selectedHex >= 0 )
		m_SpawnTileState.Set( selectedHex, kSurvivalSpawn_Available );

	// Update their selection
	m_iPlayerSpawnHexIndices.Set( iArrayIndex, hexID );

	// Currently, lock selection instantly.
	m_SpawnTileState.Set( hexID, kSurvivalSpawn_Locked );

	// Update adjacent hexes as locked by proximity
	for ( int i = 0; i < EHexEdge_Count; i++ )
	{
		int iNeighborIndex = Get2DHexNeighborTile( hexID, i );
		if ( iNeighborIndex >= 0 && m_SpawnTileState.Get( iNeighborIndex ) == kSurvivalSpawn_Available )
		{
			m_SpawnTileState.Set( iNeighborIndex, kSurvivalSpawn_ProximityBlocked );

			// Lock two additional tiles up and down the map
			EHexEdge nOneMoreDirection;
			switch ( i )
			{
			case EHexEdge_Northwest: nOneMoreDirection = EHexEdge_Northeast; break;
			case EHexEdge_Northeast: nOneMoreDirection = EHexEdge_Northwest; break;
			case EHexEdge_Southwest: nOneMoreDirection = EHexEdge_Southeast; break;
			case EHexEdge_Southeast: nOneMoreDirection = EHexEdge_Southwest; break;
			default: continue;
			}
			int iOneMoreNeighborIndex = Get2DHexNeighborTile( iNeighborIndex, nOneMoreDirection );
			if ( iOneMoreNeighborIndex >= 0 && m_SpawnTileState.Get( iOneMoreNeighborIndex ) == kSurvivalSpawn_Available )
			{
				m_SpawnTileState.Set( iOneMoreNeighborIndex, kSurvivalSpawn_ProximityBlocked );
			}
		}
	}

	return true;
}

bool CSurvivalGameRules::CheckAllPlayersSelectSpawnHex()
{
	FOR_EACH_PLAYER( iPlayer )
	{
		CCSPlayer* pPlayer = UTIL_ValidatePlayer( iPlayer, true, true, true, false );
		if ( !pPlayer )
			continue;

		// found a player who hasn't pick yet
		const int iSpawnHexArrayIndex = iPlayer - 1;
		const int iPlayerHexID = m_iPlayerSpawnHexIndices[ iSpawnHexArrayIndex ];
		if ( iPlayerHexID == INVALID_SPAWN_HEX )
			return false;
	}

	return true;
}

void CSurvivalGameRules::ForceSelectAllSpawnHexes( bool bBotsOnly /* = false */ )
{
	CUtlVector< int > vecPlayersWhoHaventChosenSpawn;
	FOR_EACH_PLAYER( iPlayer )
	{
		CCSPlayer* pPlayer = UTIL_ValidatePlayer( iPlayer, true, true, true, false );
		if ( !pPlayer )
			continue;

		if ( bBotsOnly && !pPlayer->IsBot() )
			continue;

		// found a player who hasn't pick yet
		const int iSpawnHexArrayIndex = iPlayer - 1;
		const int iPlayerHexID = m_iPlayerSpawnHexIndices[iSpawnHexArrayIndex];
		if ( iPlayerHexID == INVALID_SPAWN_HEX )
		{
			// store array index of the player
			vecPlayersWhoHaventChosenSpawn.AddToTail( iPlayer );
		}
	}

	// if all players have selected, don't need to do any more work
	if ( vecPlayersWhoHaventChosenSpawn.Count() == 0 )
		return;

	// Build set of available hexes so we can choose randomly.
	CUtlVector< int > vecAvailableHex;
	int nTotalHex = GetNumSpawnHex();
	vecAvailableHex.EnsureCapacity( nTotalHex );
	for ( int i = 0; i < nTotalHex; ++i )
	{
		ESurvivalSpawnTileState eState = m_SpawnTileState.Get( i );
		if ( eState == kSurvivalSpawn_Available )
			vecAvailableHex.AddToTail( i );
	}

	// randomly select a hex for each player who didn't make selection
	FOR_EACH_VEC( vecPlayersWhoHaventChosenSpawn, i )
	{
		// Selection of hexes for some players might make other hexes unavailable, so loop until successful.
		bool bRandomSelectionSucceeded = false;
		while ( !bRandomSelectionSucceeded && vecAvailableHex.Count() )
		{
			int iRandomHex = RandomInt( 0, vecAvailableHex.Count() - 1 );
			int iRandomHexIndex = vecAvailableHex.Element( iRandomHex );
			vecAvailableHex.FastRemove( iRandomHex );

			bRandomSelectionSucceeded = PlayerSelectHex( vecPlayersWhoHaventChosenSpawn[i], iRandomHexIndex );
		}

		if ( !bRandomSelectionSucceeded )
		{
			// no space to spawn
			Warning( "Couldn't spawn some players -- not enough hexes" );
			Assert( false );
		}
	}
}

void CSurvivalGameRules::LockSpawnHexChoices()
{
	int nTotalHex = GetNumSpawnHex();
	for ( int i = 0; i < nTotalHex; ++i )
	{
		ESurvivalSpawnTileState eState = m_SpawnTileState.Get( i );

		switch ( eState )
		{
		case kSurvivalSpawn_TempLocked:
		case kSurvivalSpawn_Occupied:
			// lock decisions in
			m_SpawnTileState.Set( i, kSurvivalSpawn_Locked );
			break;

		default:
			// nothing to do
			break;
		}
	}
}



void CSurvivalGameRules::ValidateAndAdjustPlayerSpawnPosition( Vector &pos, float flGroundOffset /* = 0.0f */ )
{
	// if we want a minimum height off the ground, trace and apply that here
	if ( flGroundOffset > 0 )
	{
		AABB_t playArea = GetPlayAreaBounds();

		Vector vecTraceFrom = Vector( pos.x, pos.y, playArea.m_vMaxBounds.z );
		Vector vecTraceTo = Vector( pos.x, pos.y, -playArea.m_vMaxBounds.z );

		trace_t tr;
		UTIL_TraceHull( vecTraceFrom, vecTraceTo, VEC_HULL_MIN, VEC_HULL_MAX, MASK_PLAYERSOLID, NULL, COLLISION_GROUP_PLAYER_MOVEMENT, &tr );

		pos.z = tr.endpos.z + flGroundOffset;
	}

	// make sure the destination is clear, if not, try space nearby
	{
		for ( int i = 0; i < 24; i++ )
		{
			int nScale = 48 * i;

			Vector vecTemp = pos;
			vecTemp.x += RandomInt( -nScale, nScale );
			vecTemp.y += RandomInt( -nScale, nScale );

			trace_t tr;
			UTIL_TraceHull( vecTemp, vecTemp, VEC_HULL_MIN, VEC_HULL_MAX, MASK_PLAYERSOLID, NULL, COLLISION_GROUP_PLAYER_MOVEMENT, &tr );
			if ( !tr.DidHit() )
			{
				pos = vecTemp;
				break;
			}
		}
	}

}

void CSurvivalGameRules::SpawnAllPlayers()
{
	DZ_ConsoleMsg( "DEPLOY PLAYERS START\n" );

	for ( int i = 1; i <= MAX_PLAYERS; ++i )
	{
		m_roundData_playerXuids.Set( i, 0 );
		m_roundData_playerPositions.Set( i, -1 );
	}

	// Print the log line that we are playing a solo game
	UTIL_LogPrintf( "SURVIVAL game start %s\n", IsPlayingTeamMode() ? "teams" : "solo" );

	FOR_EACH_PLAYER( iPlayer )
	{
		CCSPlayer* pPlayer = UTIL_ValidatePlayer( iPlayer, true, true, true, false );
		if ( !pPlayer )
			continue;

		// Add to set of players present in this round
		{
			int playerIndex = pPlayer->entindex();
			m_roundData_playerXuids.Set( playerIndex, UTIL_GetPlayerXUID( pPlayer ) );
			m_roundData_playerPositions.Set( playerIndex, 0 );
		}

		pPlayer->UnforceButtons( IN_JUMP );

		// disconnect from chopper
		pPlayer->SetParent( NULL );

		// MADHACK FIXME: why do player's econ gloves disappear when they spawn?
		{
			CUtlVector<CBaseEntity*> vecTemp;
			CBaseEntity *pChild = pPlayer->FirstMoveChild();
			while ( pChild )
			{
				vecTemp.AddToTail( pChild );
				pChild = pChild->NextMovePeer();
			}
			FOR_EACH_VEC( vecTemp, n )
			{
				vecTemp[n]->SetParent( NULL );
				vecTemp[n]->SetParent( pPlayer );
			}
		}

		pPlayer->SetMoveType( MOVETYPE_WALK );
		pPlayer->RemoveSolidFlags( FSOLID_NOT_SOLID );
		pPlayer->SetCollisionGroup( COLLISION_GROUP_PLAYER );
		pPlayer->SetWaterType( CONTENTS_EMPTY );
		pPlayer->SetGroundEntity( NULL );
		pPlayer->SetAbsVelocity( vec3_origin );
		pPlayer->m_Local.m_iHideHUD &= ~HIDEHUD_PLAYERDEAD;

		pPlayer->m_flLifeStartTime = gpGlobals->curtime;

		Vector vecTelePos = vec3_origin;

		int hexID = m_iPlayerSpawnHexIndices[iPlayer - 1];
		Assert( hexID != INVALID_SPAWN_HEX );
		vecTelePos = GetSpawnHexWorldPosition( hexID );
		ValidateAndAdjustPlayerSpawnPosition( vecTelePos, dev_dz_spawn_height_ground_offset.GetFloat() );

		QAngle angTeleAng = vec3_angle;
		pPlayer->Teleport( &vecTelePos, &angTeleAng, &vec3_origin );

		pPlayer->ViewPunchReset();
		
		QAngle angEyeAngles = pPlayer->EyeAngles();
		angEyeAngles[YAW] = 90.0f;
		angEyeAngles[PITCH] = 48.5f; // look down when coming in
		pPlayer->SnapEyeAngles( angEyeAngles );

		// fade in from blue-white
		color32_s clr = { 200, 230, 255, 255 };
		UTIL_ScreenFade( pPlayer, clr, 0.6f, 0.6f, FFADE_IN | FFADE_PURGE | FFADE_SOFTCURVE );

		pPlayer->EnableTrackingDistanceTraveled( true );

		// Set up rappel from chopper
		pPlayer->m_bIsSpawnRappelling = true;
		pPlayer->m_vecSpawnRappellingRopeOrigin = pPlayer->GetAbsOrigin() + Vector( 0, 0, 100 );
		pPlayer->SetMaxFallVelocity( -180 );

		CParadropChopper *pChopper = dynamic_cast<CParadropChopper *>(CreateEntityByName( "paradrop_chopper" ));
		Assert( pChopper );
		if ( pChopper )
		{
			pChopper->m_nChopperType = CHOPPER_TYPE_RAPPEL;
			pChopper->SetCallingPlayer( pPlayer, true );
			DispatchSpawn( pChopper );
			pChopper->EmitSound( "heli_pass2" );
		}

		LoadoutPlayer( pPlayer );

		if ( IsPlayingTeamMode() )
		{
			int nThisPlayerTeam = pPlayer->m_nSurvivalTeam.Get();
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" is on team %d\n",
				pPlayer->GetPlayerName(),
				pPlayer->entindex(),
				pPlayer->GetNetworkIDString(),
				pPlayer->GetTeam()->GetName(),
				nThisPlayerTeam );
		}
	}

	// tell the spawn chopper that it can fly away
	CSurvivalSpawnChopper *pSpawnChopper = dynamic_cast<CSurvivalSpawnChopper*>(m_hSpawnChopper.Get());
	if ( pSpawnChopper )
	{
		pSpawnChopper->StopCircling();
		UTIL_Remove( pSpawnChopper );
	}
}

void CSurvivalGameRules::NotifySurvivalSquadPartners()
{
	// The message about a partner is only simple in duo-squads
	if ( sv_dz_team_count.GetInt() != 2 ) return;

	FOR_EACH_PLAYER( iPlayer )
	{
		CCSPlayer* pPlayer = UTIL_ValidatePlayer( iPlayer, false, true, true, true );
		if ( !pPlayer )
			continue;

		// Who's your partner?
		CUtlVector< CCSPlayer * > arrTeammates;
		GetPlayerTeammates( pPlayer, arrTeammates );
		if ( !arrTeammates.IsEmpty() )
		{
			CCSPlayer *pPartner = arrTeammates.Head();
			ClientPrint( pPlayer, HUD_PRINTCENTER, "#Cstrike_TitlesTXT_Hint_DZPartner", CFmtStr( "#ENTNAME[%d]%s", pPartner->entindex(), pPartner->GetPlayerName() ) );
		}
	}
}

void CSurvivalGameRules::UpdateSpawnStage()
{
	if ( m_spawnStage == SPAWN_STAGE_NONE )
		return;

	switch ( m_spawnStage )
	{
		case SPAWN_STAGE_SELECTION:
		{
			ForceSelectAllSpawnHexes( true ); // bots pick right away

			bool bStageOver = gpGlobals->curtime >= m_flSpawnSelectionTimeEnd;
			if ( bStageOver || CheckAllPlayersSelectSpawnHex() )
			{
				// Lock in any decisions that are still marked as temporary
				LockSpawnHexChoices();

				m_flSpawnSelectionTimeStart = gpGlobals->curtime;
				m_flSpawnSelectionTimeEnd = gpGlobals->curtime + dev_dz_spawn_selection_lock_time.GetFloat();
				m_flSpawnSelectionTimeLoadout = m_flSpawnSelectionTimeEnd + dev_dz_spawn_selection_ready_time.GetFloat();
				m_spawnStage = SPAWN_STAGE_LOCKED;
			}
		}
		break;
		case SPAWN_STAGE_LOCKED:
		{
			if ( gpGlobals->curtime >= m_flSpawnSelectionTimeEnd )
			{
				// Select spawn hexes for anyone who hasn't selected a hex yet
				ForceSelectAllSpawnHexes();

				// Lock in those choices if they weren't for whatever reason
				LockSpawnHexChoices();

				m_flSpawnSelectionTimeStart = gpGlobals->curtime;
				m_flSpawnSelectionTimeEnd = gpGlobals->curtime + dev_dz_spawn_selection_ready_time.GetFloat();
				m_flSpawnSelectionTimeLoadout = m_flSpawnSelectionTimeEnd;
				m_spawnStage = SPAWN_STAGE_ALL_READY;
			}
		}
		break;
		case SPAWN_STAGE_ALL_READY:
		{
			if ( !m_bSurvivalEventFired_FadeEveryoneOutFromMapSelection && gpGlobals->curtime >= m_flSpawnSelectionTimeEnd - 1.0f )
			{
				m_bSurvivalEventFired_FadeEveryoneOutFromMapSelection = true;
				DoPlayerExitWarmupTransition( 1.0f );
			}

			if ( gpGlobals->curtime >= m_flSpawnSelectionTimeEnd )
			{
				m_flSpawnSelectionTimeStart = -1;
				m_flSpawnSelectionTimeEnd = -1;
				m_flSpawnSelectionTimeLoadout = -1;
				m_spawnStage = SPAWN_STAGE_NONE;

				StartSurvival();
			}
		}
		break;
	}
}

CON_COMMAND_F( dz_spawnselect_choose_hex, "", FCVAR_GAMEDLL | FCVAR_GAMEDLL_FOR_REMOTE_CLIENTS )
{
	CCSPlayer *player = ToCSPlayer( UTIL_GetCommandClient() );
	if ( !player || player->IsSpectator() )
		return;

	if ( !CSGameRules() )
		return;

	CSurvivalGameRules *pBRRules = CSGameRules()->GetSurvivalRules();
	if ( !pBRRules )
		return;

	// can only select during selection phase
	if ( pBRRules->GetSpawnStage() != CSurvivalGameRules::SPAWN_STAGE_SELECTION )
		return;

	int hexID = atoi( args[1] );
	pBRRules->PlayerSelectHex( player->entindex(), hexID );
}

#endif // GAME_DLL


int CSurvivalGameRules::GetNumSpawnHexHorizontal() const
{
	return SURVIVAL_SPAWN_TILE_WIDTH;
}

int CSurvivalGameRules::GetNumSpawnHexVertical() const
{
	return SURVIVAL_SPAWN_TILE_HEIGHT;
}

int CSurvivalGameRules::GetNumSpawnHex() const
{
	return GetNumSpawnHexHorizontal() * GetNumSpawnHexVertical();
}

float CSurvivalGameRules::GetSpawnHexWidth() const
{
	return 1.0f / ( GetNumSpawnHexHorizontal() + 0.5f ); // half a hex for the hex offsets on either side
}

float CSurvivalGameRules::GetSpawnHexHeight() const
{
	return 1.0f / ( GetNumSpawnHexVertical() + 0.25f ); // quarter of a hex for pointy tops/bottoms of hexes
} 

// Returns the top-left of the selectable area for a hex in the UI
Vector CSurvivalGameRules::Get2DHexPosition( int tileIdx ) const
{
	if ( tileIdx < 0 || tileIdx >= GetNumSpawnHex() )
		return Vector( -1.0f, -1.0f, 0.0f );

	const int nSpawnHexHori = GetNumSpawnHexHorizontal();
	int x = tileIdx % nSpawnHexHori;
	int y = tileIdx / nSpawnHexHori;

	const float flHexWidth = GetSpawnHexWidth();
	const float flHexHeight = GetSpawnHexHeight();
	const float flSpawnHexOddRowXOffset = 0.5f * flHexWidth; // Offset from left of map for hexes in odd rows
	const float flSpawnHexYOffset = 0.125f * flHexHeight;	// Offset from top of map for start of selectable area

	float xOffset = ( y % 2 ) ? flSpawnHexOddRowXOffset : 0.0f;
	float xPos = ( x * flHexWidth ) + xOffset;
	float yPos = ( y * flHexHeight ) + flSpawnHexYOffset;

	return Vector( xPos, yPos, 0.0f );
}

int CSurvivalGameRules::Get2DHexNeighborTile( int tileIdx, int iHexEdge ) const
{
	if ( tileIdx < 0 || tileIdx >= GetNumSpawnHex() )
		return INVALID_SPAWN_HEX;

	if ( iHexEdge < 0 || iHexEdge >= EHexEdge_Count )
		return INVALID_SPAWN_HEX;

	const int nSpawnHexHori = GetNumSpawnHexHorizontal();
	int x = tileIdx % nSpawnHexHori;
	int y = tileIdx / nSpawnHexHori;

	int xOffsetForLeftOnNextRow = ( y % 2 ) ? 0 : -1;

	switch ( iHexEdge )
	{
	case EHexEdge_Northwest:
		x = x + 0 + xOffsetForLeftOnNextRow;
		y = y - 1;
		break;
	case EHexEdge_Northeast:
		x = x + 1 + xOffsetForLeftOnNextRow;
		y = y - 1;
		break;
	case EHexEdge_West:
		x = x - 1;
		break;
	case EHexEdge_East:
		x = x + 1;
		break;
	case EHexEdge_Southwest:
		x = x + 0 + xOffsetForLeftOnNextRow;
		y = y + 1;
		break;
	case EHexEdge_Southeast:
		x = x + 1 + xOffsetForLeftOnNextRow;
		y = y + 1;
		break;
	}

	if ( x < 0 || x >= nSpawnHexHori )
		return INVALID_SPAWN_HEX;
	if ( y < 0 || y >= GetNumSpawnHexVertical() )
		return INVALID_SPAWN_HEX;

	return y * nSpawnHexHori + x;
}

// Returns the center point of a hex in the UI
Vector CSurvivalGameRules::Get2DHexCenter( int tileIdx ) const
{
	if ( tileIdx < 0 || tileIdx >= GetNumSpawnHex() )
		return Vector(-1.0f, -1.0f, 0.0f);

	Vector pos = Get2DHexPosition( tileIdx );

	float xCenter = ( 0.5f * GetSpawnHexWidth() ) + pos.x;
	float yCenter = ( 0.5f * GetSpawnHexHeight() ) + pos.y;

	return Vector( xCenter, yCenter, 0.0f );
}

Vector CSurvivalGameRules::GetSpawnHexWorldPosition( int hexID )
{
	Vector vec2DHexCenter = Get2DHexCenter( hexID );

	AABB_t playArea = GetPlayAreaBounds();
	Vector vecMapSize = playArea.GetSize();
	return Vector( playArea.m_vMinBounds.x + vec2DHexCenter.x * vecMapSize.x, playArea.m_vMinBounds.y + ( 1.f - vec2DHexCenter.y ) * vecMapSize.y, playArea.m_vMaxBounds.z );
}

bool CSurvivalGameRules::CanSelectHex( int hexID )
{
	return
		( hexID >= 0
			&& hexID < GetNumSpawnHex()
			&& m_SpawnTileState[hexID] == kSurvivalSpawn_Available
			);
}

int CSurvivalGameRules::GetTotalNumPlayers( void )
{
	int nTotalPlayers = 0;

	for ( int i = 0; i < MAX_PLAYERS + 1; i++ )
	{
		if ( m_roundData_playerXuids[ i ] > 0 )
			nTotalPlayers++;
	}

	return nTotalPlayers;
}

void CSurvivalGameRules::OnLevelInitPreEntity( void )
{
	EarlyOutIfNotPlayingSurvival;

	UTIL_PrecacheOther( "prop_counter" );
	UTIL_PrecacheOther( "dronegun" );
	UTIL_PrecacheOther( "trigger_safemoneygather" );
	UTIL_PrecacheOther( "survival_spawn_chopper" );
	UTIL_PrecacheOther( "paradrop_chopper" );
	UTIL_PrecacheOther( "weapon_tablet" );
	UTIL_PrecacheOther( "drone" );
	UTIL_PrecacheOther( "prop_loot_crate" );
	UTIL_PrecacheOther( "radar_jammer" );
	UTIL_PrecacheOther( "prop_ammo_box_generic" );
	UTIL_PrecacheOther( "prop_weapon_upgrade_armor" );
	UTIL_PrecacheOther( "prop_weapon_upgrade_armor_helmet" );
	UTIL_PrecacheOther( "prop_weapon_upgrade_helmet" );
	UTIL_PrecacheOther( "prop_weapon_upgrade_chute" );
	UTIL_PrecacheOther( "prop_weapon_upgrade_contractkill" );
	UTIL_PrecacheOther( "prop_weapon_upgrade_heavyarmor" );
	UTIL_PrecacheOther( "prop_weapon_upgrade_tablet_zoneintel" );
	UTIL_PrecacheOther( "prop_weapon_upgrade_tablet_droneintel" );
	UTIL_PrecacheOther( "item_cash" );
}
