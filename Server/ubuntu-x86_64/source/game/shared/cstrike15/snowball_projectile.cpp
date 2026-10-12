//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "snowball_projectile.h"
#include "shake.h"
#include "engine/IEngineSound.h"
#include "dlight.h"
#include "keyvalues.h"
#include "weapon_csbase.h"
#include "cs_gamerules.h"
#include "animation.h"
#include "particle_parse.h"		// for DispatchParticleEffect

#if !defined( CLIENT_DLL )
#include "cs_player.h"
#endif

#define GRENADE_MODEL "models/weapons/w_snowball.mdl"
#define SNOWBALL_IMPACT_EFFECT "weapon_snowball_impact"
#define SNOWBALL_TRAIL_EFFECT "weapon_snowball_trail"
#define SNOWBALL_IMPACT_SPLAT_EFFECT "weapon_snowball_impact_splat"


#if defined( CLIENT_DLL )
IMPLEMENT_CLIENTCLASS_DT( C_SnowballProjectile, DT_SnowballProjectile, CSnowballProjectile )
END_RECV_TABLE()

//--------------------------------------------------------------------------------------------------------
void C_SnowballProjectile::OnNewParticleEffect( const char *pszParticleName, CNewParticleEffect *pNewParticleEffect )
{
	if ( FStrEq( pszParticleName, SNOWBALL_TRAIL_EFFECT ) )
	{
		m_snowballParticleEffect = pNewParticleEffect;
	}
}


//--------------------------------------------------------------------------------------------------------
void C_SnowballProjectile::OnParticleEffectDeleted( CNewParticleEffect *pParticleEffect )
{
	if ( m_snowballParticleEffect == pParticleEffect )
	{
		m_snowballParticleEffect = NULL;
	}
}


//--------------------------------------------------------------------------------------------------------
bool C_SnowballProjectile::Simulate( void )
{
	if ( !m_snowballParticleEffect.IsValid() )
	{
		DispatchParticleEffect( SNOWBALL_TRAIL_EFFECT, PATTACH_ABSORIGIN_FOLLOW, this );
	}
	else
	{
		m_snowballParticleEffect->SetSortOrigin( GetAbsOrigin() );
		m_snowballParticleEffect->SetNeedsBBoxUpdate( true );
	}

	BaseClass::Simulate();
	return true;
}

#else // GAME_DLL

// Allow objects to occlude snowballs in only particular anim states
// e.g. de_nuke vents only occlude snowballs when closed
class CTraceFilterNoPlayersAndSnowballPassableAnims : public CTraceFilterNoPlayers
{
	public:
	const IHandleEntity* mTargetEntity;

	CTraceFilterNoPlayersAndSnowballPassableAnims( const IHandleEntity *snowballEntity, const IHandleEntity* targetEntity, int collisionGroup )
		: CTraceFilterNoPlayers( snowballEntity, collisionGroup )
		, mTargetEntity( targetEntity )
	{
	}

	virtual bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
	{
		if ( pHandleEntity == mTargetEntity )
			return false;

		if ( !CTraceFilterNoPlayers::ShouldHitEntity( pHandleEntity, contentsMask ) )
			return false;

		CBaseEntity *pEnt = EntityFromEntityHandle( pHandleEntity );
		if ( !pEnt )
			return false;

		// Weapons don't block snowballs
		CWeaponCSBase* pWeapon = dynamic_cast< CWeaponCSBase* >( pEnt );
		CBaseGrenade* pGrenade = dynamic_cast< CBaseGrenade* > ( pEnt );
		if ( pWeapon || pGrenade )
			return false;

		// Objects can block snowballs based on their animstate
		CBaseAnimating* pAnimating = dynamic_cast< CBaseAnimating* >( pEnt );
		if ( pAnimating )
		{
			// look for the snowball passable animtag
			float flSnowballPassable = pAnimating->GetAnySequenceAnimTag( pAnimating->GetSequence(), ANIMTAG_FLASHBANG_PASSABLE, -1 );

			if ( flSnowballPassable != -1 )
				return false; // model animation is tagged to allow snowballs through
		}

		// Block
		return true;
	}
};

// --------------------------------------------------------------------------------------------------- //
//
// RadiusDamage - this entity is exploding, or otherwise needs to inflict damage upon entities within a certain range.
// 
// only damage ents that can clearly be seen by the explosion!
// --------------------------------------------------------------------------------------------------- //

void RadiusSnowball( 
	Vector vecSrc, 
	CBaseEntity *pevInflictor, 
	CBaseEntity *pevAttacker, 
	float flDamage, 
	int iClassIgnore, 
	int bitsDamageType, 
	uint8 *pOutNumOpponentsEffected = NULL, 
	uint8 *pOutNumTeammatesEffected = NULL )
{
	vecSrc.z += 1;// in case grenade is lying on the ground

	if ( !pevAttacker )
		pevAttacker = pevInflictor;

	if ( pOutNumOpponentsEffected )
		*pOutNumOpponentsEffected = 0;

	if ( pOutNumTeammatesEffected )
		*pOutNumTeammatesEffected = 0;
	
	trace_t		tr;
	float		flAdjustedDamage;
	variant_t	var;
	Vector		vecEyePos;
	float		fadeTime, fadeHold;
	Vector		vForward;
	Vector		vecLOS;
	float		flDot;
	
	CBaseEntity		*pEntity = NULL;
	static float	flRadius = 62;
	float			falloff = flDamage / flRadius;

	//bool bInWater = (UTIL_PointContents( vecSrc, MASK_WATER ) == CONTENTS_WATER);

	// iterate on all entities in the vicinity.
	while ((pEntity = gEntList.FindEntityInSphere( pEntity, vecSrc, flRadius )) != NULL)
	{	
		bool bPlayer = pEntity->IsPlayer();

		if( !bPlayer )
			continue;

		vecEyePos = pEntity->EyePosition();

		Ray_t ray;
		trace_t tr2;
		CTraceFilterLOS traceFilter( pevInflictor, COLLISION_GROUP_NONE );
		//CTraceFilterNoPlayersAndSnowballPassableAnims traceFilter( pevInflictor, bPlayer, COLLISION_GROUP_NONE );
		unsigned int SNOW_MASK = MASK_OPAQUE_AND_NPCS | CONTENTS_DEBRIS;
		SNOW_MASK &= ~CONTENTS_OPAQUE;

		ray.Init( pEntity->EyePosition(), vecSrc );
		enginetrace->TraceRay( ray, SNOW_MASK, &traceFilter, &tr2 );

		if ( tr2.fraction == 1.0f )
		{
			if ( pOutNumOpponentsEffected && pEntity->GetTeamNumber() != pevAttacker->GetTeamNumber() )
				(*pOutNumOpponentsEffected)++;
			if ( pOutNumTeammatesEffected && pEntity->GetTeamNumber() == pevAttacker->GetTeamNumber() )
				(*pOutNumTeammatesEffected)++;

			// decrease damage for an ent that's farther from the grenade
			flAdjustedDamage = flDamage - ( vecSrc - pEntity->EyePosition() ).Length() * falloff;

			if ( flAdjustedDamage > 0 )
			{
				// See if we were facing the flash
				AngleVectors( pEntity->EyeAngles(), &vForward );

				vecLOS = ( vecSrc - vecEyePos );

				float flDistance = vecLOS.Length();

				//DebugDrawLine( vecEyePos, vecEyePos + (100.0 * vecLOS), 0, 255, 0, true, 10.0 );
				//DebugDrawLine( vecEyePos, vecEyePos + (100.0 * vForward), 0, 0, 255, true, 10.0 );

				// Normalize both vectors so the dotproduct is in the range -1.0 <= x <= 1.0 
				vecLOS.NormalizeInPlace();

				flDot = DotProduct (vecLOS, vForward);

				float startingAlpha = 110;
				bool bHitFace = (flDistance<36);

				// if target is facing the bomb, the effect lasts longer
				if( flDot >= 0.6 )
				{
					// looking at the snowball
					fadeTime = flAdjustedDamage * 1.0f;
					fadeHold = flAdjustedDamage * 0.1f;
				}
				else if( flDot >= 0.3 )
				{
					// looking to the side
					fadeTime = flAdjustedDamage * 0.8f;
					fadeHold = flAdjustedDamage * 0.1f;
				}
				else if( flDot >= 0.0 )
				{
					// looking to the side
					fadeTime = flAdjustedDamage * 0.45f;
					fadeHold = flAdjustedDamage * 0.1f;
					bHitFace = false;
				}
				else
				{
					// facing away
					fadeTime = flAdjustedDamage * 0.2f;
					fadeHold = flAdjustedDamage * 0.01f;
					bHitFace = false;
				//	startingAlpha = 200;
				}

				fadeTime *= falloff;
				fadeHold *= falloff;

				if ( bPlayer )
				{
					// blind players and bots
					CCSPlayer *player = static_cast< CCSPlayer * >( pEntity );
					if ( player )
					{
						player->Snowballed( fadeHold, fadeTime, startingAlpha, bHitFace );
						player->EmitSound( "Snowball.HitPlayerFace" );	

						if ( bHitFace )
						{
							IGameEvent * event = gameeventmanager->CreateEvent( "snowball_hit_player_face" );
							if ( event )
							{
								event->SetInt( "userid", engine->GetPlayerUserId( player->edict() ) );
								gameeventmanager->FireEvent( event );
							}
						}
					}
				}
			}	
		}
	}
}

LINK_ENTITY_TO_CLASS( snowball_projectile, CSnowballProjectile );
PRECACHE_REGISTER( snowball_projectile );

BEGIN_DATADESC( CSnowballProjectile )

// Fields
//DEFINE_KEYFIELD( m_flTimeToDetonate, FIELD_FLOAT, "TimeToDetonate" ),

// Inputs
DEFINE_INPUTFUNC( FIELD_FLOAT, "SetTimer", InputSetTimer ),

END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CSnowballProjectile, DT_SnowballProjectile )
END_SEND_TABLE()

// --------------------------------------------------------------------------------------------------- //
// CSnowballProjectile implementation.
// --------------------------------------------------------------------------------------------------- //

CSnowballProjectile* CSnowballProjectile::Create( 
	const Vector &position, 
	const QAngle &angles, 
	const Vector &velocity, 
	const AngularImpulse &angVelocity, 
	CBaseCombatCharacter *pOwner,
	item_definition_index_t weaponItem )
{
	CSnowballProjectile *pGrenade = (CSnowballProjectile*)CBaseEntity::Create( "snowball_projectile", position, angles, pOwner );
	
	// Set the timer for 1 second less than requested. We're going to issue a SOUND_DANGER
	// one second before detonation.
	pGrenade->SetAbsVelocity( velocity );
	pGrenade->SetupInitialTransmittedGrenadeVelocity( velocity );
	pGrenade->SetThrower( pOwner );
	pGrenade->SetSourceWeaponInfo( weaponItem );

	pGrenade->ChangeTeam( pOwner->GetTeamNumber() );

	pGrenade->ApplyLocalAngularVelocityImpulse( angVelocity );
	pGrenade->SetCollisionGroup( COLLISION_GROUP_PROJECTILE );

	// we have to reset these here because we set the model late and it resets the collision
	Vector min = Vector( -3, -3, -3 );
	Vector max = Vector( 3, 3, 3 );
	pGrenade->SetSize( min, max );
	if ( pGrenade->CollisionProp() )
		pGrenade->SetCollisionBounds( min, max );

	return pGrenade;
}

CSnowballProjectile::CSnowballProjectile()
{
	m_flDamage = 1;
	// default timer value for when a player throws it
	// can be overridden when spawned from an entity maker
	m_flTimeToDetonate = 10.0;
	m_numOpponentsHit = m_numTeammatesHit = 0;
}

void CSnowballProjectile::Spawn()
{
	SetModel( GRENADE_MODEL );

	SetDetonateTimerLength( m_flTimeToDetonate );

	SetTouch( &CSnowballProjectile::BounceTouch );

	SetThink( &CBaseCSGrenadeProjectile::DangerSoundThink );
	SetNextThink( gpGlobals->curtime );

	SetGravity( BaseClass::GetGrenadeGravity() );
	SetFriction( BaseClass::GetGrenadeFriction() );
	SetElasticity( BaseClass::GetGrenadeElasticity() );

	BaseClass::Spawn();

	SetBodygroupPreset( "thrown" );

	//DispatchParticleEffect( SNOWBALL_TRAIL_EFFECT, PATTACH_ABSORIGIN_FOLLOW, this );
}

void CSnowballProjectile::Precache()
{
	PrecacheModel( GRENADE_MODEL );

	PrecacheScriptSound( "Player.SnowballHit" );
	PrecacheScriptSound( "Snowball.HitPlayerFace" );
	PrecacheScriptSound( "Snowball.Bounce" );
	PrecacheParticleSystem( SNOWBALL_IMPACT_EFFECT );

	BaseClass::Precache();
}

ConVar sv_snowball_strength( "sv_snowball_strength", "12.0", FCVAR_REPLICATED, "Snowball strength", true, 2.0, true, 64.0 );

void CSnowballProjectile::Detonate()
{
	RadiusSnowball ( GetAbsOrigin(), this, GetThrower(), sv_snowball_strength.GetInt(), CLASS_NONE, DMG_BLAST, &m_numOpponentsHit, &m_numTeammatesHit );
	EmitSound( "Player.SnowballHit" );	

	//trace_t		tr;
	//Vector		vecSpot = GetAbsOrigin() + Vector ( 0 , 0 , 2 );
	//UTIL_TraceLine ( vecSpot, vecSpot + Vector ( 0, 0, -64 ), MASK_SHOT_HULL, this, COLLISION_GROUP_NONE, & tr);
	//UTIL_DecalTrace( &tr, "Scorch" );

	Vector vecDirection = GetAbsVelocity().Normalized();//GetThrower()->EyePosition() - GetAbsOrigin()).Normalized();
	trace_t		tr;
	Vector		vecSpot = GetAbsOrigin();
	Vector		vecEnd = vecSpot + (vecDirection * 16);
	UTIL_TraceLine ( vecSpot, vecEnd, MASK_SHOT_HULL, this, COLLISION_GROUP_NONE, & tr);
	
	// DEBUG
	//float length = tr.endpos.DistTo( vecSpot );
	//float length2 = vecEnd.DistTo( vecSpot );
	//NDebugOverlay::BoxDirection(vecSpot, Vector(-1,-1,-1), Vector(length,1,1), vecDirection, 0,255,255,255,8);
	//NDebugOverlay::BoxDirection(vecSpot, Vector(-4,-4,-4), Vector(length2,4,4), vecDirection, 255,255,0,40,8);

	bool bSpawnSplat = false;
	if (tr.DidHit())
	{
		vecDirection = tr.plane.normal;
		if ( tr.m_pEnt->IsWorld() )
			bSpawnSplat = true;
	}
	else
	{
		vecDirection = -vecDirection;
	}
	QAngle angles;
	VectorAngles( vecDirection, angles );
	DispatchParticleEffect( SNOWBALL_IMPACT_EFFECT, GetAbsOrigin(), angles );
	if ( bSpawnSplat )
	{
		Vector	vforward, vright, vup;
		AngleVectors( angles, &vforward, &vright, &vup );
		VectorAngles( vup, angles );

		DispatchParticleEffect( SNOWBALL_IMPACT_SPLAT_EFFECT, GetAbsOrigin(), angles );
	}

	//NDebugOverlay::Line(tr.endpos, tr.endpos + vecDirection*18, 255, 0, 255, true, 10 );
	UTIL_Remove( this );
}

void CSnowballProjectile::BounceTouch( CBaseEntity *other )
{
	// Deal the usual 1 dmg to people this grenade bounces off of
	// don't do any damage...
	//BaseClass::BounceTouch( other );

	if ( other->IsSolidFlagSet( FSOLID_TRIGGER | FSOLID_VOLUME_CONTENTS ) )
		return;

	// don't hit the guy that launched this grenade
	if ( other == GetThrower() )
		return;

	// only do damage if we're moving fairly fast
	if ( ( other->m_takedamage != DAMAGE_NO ) && ( m_flNextAttack < gpGlobals->curtime && GetAbsVelocity().Length() > 100 ) )
	{
		//if ( GetOwnerEntity() && !other->IsPlayer() )
		{
#if !defined( CLIENT_DLL )
			trace_t tr;
			tr = CBaseEntity::GetTouchTrace();
			ClearMultiDamage();
			Vector forward;
			AngleVectors( GetLocalAngles(), &forward, NULL, NULL );
			CTakeDamageInfo info( this, GetOwnerEntity(), 5, DMG_SONIC );
			CalculateMeleeDamageForce( &info, GetAbsVelocity(), GetAbsOrigin() );
			other->DispatchTraceAttack( info, forward, &tr );
			ApplyMultiDamage();
#endif
		}
		m_flNextAttack = gpGlobals->curtime + 1.0; // debounce
	}

	if ( FClassnameIs( other, "func_breakable" ) )
	{
		return;
	}

	if ( FClassnameIs( other, "func_breakable_surf" ) )
	{
		return;
	}

	// don't detonate on ladders
	if ( FClassnameIs( other, "func_ladder" ) )
	{
		return;
	}

	Detonate();
}

//TODO: Let physics handle the sound!
void CSnowballProjectile::BounceSound( void )
{
	//EmitSound( "Snowball.Bounce" );
}

void CSnowballProjectile::InputSetTimer( inputdata_t &inputdata )
{
	m_flTimeToDetonate = inputdata.value.Float();
	SetDetonateTimerLength( m_flTimeToDetonate );
}

#endif