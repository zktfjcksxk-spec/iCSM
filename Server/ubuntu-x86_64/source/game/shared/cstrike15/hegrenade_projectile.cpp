//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "hegrenade_projectile.h"
#include "soundent.h"
#include "cs_player.h"
#include "keyvalues.h"
#include "weapon_csbase.h"
#include "decals.h"

#include "bot_manager.h"

#define GRENADE_MODEL "models/Weapons/w_eq_fraggrenade_dropped.mdl"

ConVar sv_hegrenade_damage_multiplier( "sv_hegrenade_damage_multiplier", "1", FCVAR_RELEASE | FCVAR_REPLICATED );
ConVar sv_hegrenade_radius_multiplier( "sv_hegrenade_radius_multiplier", "1", FCVAR_RELEASE | FCVAR_REPLICATED );

LINK_ENTITY_TO_CLASS( hegrenade_projectile, CHEGrenadeProjectile );
PRECACHE_REGISTER( hegrenade_projectile );

#if !defined( CLIENT_DLL )
BEGIN_DATADESC( CHEGrenadeProjectile )

// Inputs
DEFINE_INPUTFUNC( FIELD_VOID, "InitializeSpawnFromWorld", InitializeSpawnFromWorld ),

END_DATADESC()
#endif

CHEGrenadeProjectile* CHEGrenadeProjectile::Create( 
	const Vector &position, 
	const QAngle &angles, 
	const Vector &velocity, 
	const AngularImpulse &angVelocity, 
	CBaseCombatCharacter *pOwner, 
	item_definition_index_t nWeaponItem,
	float timer )
{
	CHEGrenadeProjectile *pGrenade = (CHEGrenadeProjectile*)CBaseEntity::Create( "hegrenade_projectile", position, angles, pOwner );
	
	// Set the timer for 1 second less than requested. We're going to issue a SOUND_DANGER
	// one second before detonation.

	pGrenade->SetDetonateTimerLength( 1.5 );
	pGrenade->SetAbsVelocity( velocity );
	pGrenade->SetupInitialTransmittedGrenadeVelocity( velocity );
	pGrenade->SetThrower( pOwner ); 

	pGrenade->SetGravity( BaseClass::GetGrenadeGravity() );
	pGrenade->SetFriction( BaseClass::GetGrenadeFriction() );
	pGrenade->SetElasticity( BaseClass::GetGrenadeElasticity() );

	pGrenade->ChangeTeam( pOwner->GetTeamNumber() );
	pGrenade->ApplyLocalAngularVelocityImpulse( angVelocity );	

	// make NPCs afaid of it while in the air
	pGrenade->SetThink( &CHEGrenadeProjectile::DangerSoundThink );
	pGrenade->SetNextThink( gpGlobals->curtime );

	pGrenade->SetSourceWeaponInfo( nWeaponItem );
	
	const CEconItemView* pGenericGrenade = CSInventoryManager()->GetReferenceEconItem( nWeaponItem, 0 );
	const CCSWeaponInfo* pWeaponInfo = GetWeaponInfoFromItem( pGenericGrenade );
	pGrenade->m_flDamage = ( float )pWeaponInfo->GetDamage( pGenericGrenade );		//	100;
	pGrenade->m_DmgRadius = pWeaponInfo->GetRange( pGenericGrenade );			// pGrenade->m_flDamage * 3.5f;

	pGrenade->m_flDamage *= sv_hegrenade_damage_multiplier.GetFloat();
	pGrenade->m_DmgRadius *= sv_hegrenade_radius_multiplier.GetFloat();

	pGrenade->SetCollisionGroup( COLLISION_GROUP_PROJECTILE );
	return pGrenade;
}

void CHEGrenadeProjectile::Spawn()
{
	SetModel( GRENADE_MODEL );
	BaseClass::Spawn();

	SetBodygroupPreset( "thrown" );
}

void CHEGrenadeProjectile::Precache()
{
	PrecacheModel( GRENADE_MODEL );

	PrecacheScriptSound( "HEGrenade.Bounce" );

	BaseClass::Precache();
}

void CHEGrenadeProjectile::BounceSound( void )
{
	EmitSound( "HEGrenade.Bounce" );
}

void CHEGrenadeProjectile::Detonate()
{
	// tell the bots an HE grenade has exploded (and record the event in the log)
	if ( CCSPlayer *player = ToCSPlayer( GetThrower() ) )
	{
		IGameEvent * event = gameeventmanager->CreateEvent( "hegrenade_detonate" );
		if ( event )
		{
			event->SetInt( "userid", player->GetUserID() );
			event->SetInt( "entityid", this->entindex() );
			event->SetFloat( "x", GetAbsOrigin().x );
			event->SetFloat( "y", GetAbsOrigin().y );
			event->SetFloat( "z", GetAbsOrigin().z );
			gameeventmanager->FireEvent( event );
		}
	}

	BaseClass::Detonate();
}

const char *CHEGrenadeProjectile::GetParticleSystemName( int pointContents, surfacedata_t *pdata )
{
	if ( pointContents & MASK_WATER )
		return "explosion_basic_water";
	
	// [msmith] If the grenade goes off near smoke, then we need to make sure that it doesn't
	// spawn any of it's own smoke (the explosion_hegrenade_brief effect has no smoke).
	// This fixes an exploit that allowed players to "see through" the smokegrenade smoke.
	const Vector *detonatePosition = &GetAbsOrigin();
	if ( TheBots->IsInsideSmokeCloud( detonatePosition, HEGrenadeRadius ) )
		return "explosion_hegrenade_brief";

	if ( pdata )
	{

		switch( pdata->game.material )
		{
		case CHAR_TEX_DIRT:
		case CHAR_TEX_SAND:
		case CHAR_TEX_GRASS:
		case CHAR_TEX_MUD:
		case CHAR_TEX_FOLIAGE:
			return "explosion_hegrenade_dirt";

		case CHAR_TEX_SNOW:
			return "explosion_hegrenade_snow";
		}
	}

	return "explosion_basic";
}

void CHEGrenadeProjectile::InitializeSpawnFromWorld( inputdata_t &inputdata )
{
	SetDetonateTimerLength( 1.5 );

	SetGravity( GetGrenadeGravity() );
	SetFriction( GetGrenadeFriction() );
	SetElasticity( GetGrenadeElasticity() );

	//pGrenade->ChangeTeam( pOwner->GetTeamNumber() );
	ApplyLocalAngularVelocityImpulse( AngularImpulse(600,random->RandomInt(-1200,1200),0) );

	// make NPCs afaid of it while in the air
	SetThink( &CHEGrenadeProjectile::DangerSoundThink );
	SetNextThink( gpGlobals->curtime );

	// Set default weapon info
	SetSourceWeaponInfo( WeaponIDToDefinition( WEAPON_HEGRENADE )->GetDefinitionIndex() );

	//m_flDamage = (float)m_pWeaponInfo.GetDamage();		//	100;
	//m_DmgRadius = m_pWeaponInfo.GetRange();			// pGrenade->m_flDamage * 3.5f;
	SetCollisionGroup( COLLISION_GROUP_PROJECTILE );
}