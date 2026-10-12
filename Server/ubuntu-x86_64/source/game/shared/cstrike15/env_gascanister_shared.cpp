//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "env_gascanister_shared.h"
#include "mapdata_shared.h"
#include "sharedInterface.h"
#include "mathlib/vmatrix.h"
#include "particle_parse.h"
#include "dangerzone_controller.h"
#include "decals.h"
#include "physics_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define ROTATION_SPEED 90.0f

//-----------------------------------------------------------------------------
// Models!
//-----------------------------------------------------------------------------
#define ENV_GASCANISTER_MODEL	"models/Shells/shell_57.mdl"
#define ENV_GASCANISTER_SKYBOX_MODEL	"models/Shells/shell_57.mdl"
#define ENV_GASCANISTER_INCOMING_SOUND_TIME	3.5f
#define ENV_GASCANISTER_KILL_PARTICLES_TIME 0.075f // how much time bfore impact we should kill the skybox particle trails
#define ENV_GASCANISTER_PARTICLE_TRAIL	"gas_canister_trail"
#define ENV_GASCANISTER_PARTICLE_IDLE	"gas_canister_idle"
#define ENV_GASCANISTER_PARTICLE_IMPACT	"gas_cannister_impact"

DEVELOPMENT_ONLY_CONVAR( dev_dz_gascanister_launch_height, 10000 );
DEVELOPMENT_ONLY_CONVAR( dev_dz_gascanister_flight_speed, 60 );
DEVELOPMENT_ONLY_CONVAR( dev_dz_gascanister_flight_time, 10 );
DEVELOPMENT_ONLY_CONVAR( dev_dz_gascanister_shake_amplitude, 50 );
DEVELOPMENT_ONLY_CONVAR( dev_dz_gascanister_shake_radius, 1024 );

#define ENV_GASCANISTER_TRAIL_TIME	3.0f

#ifndef CLIENT_DLL
LINK_ENTITY_TO_CLASS( info_gascanister_launchpoint, CPointEntity );
#endif

//-----------------------------------------------------------------------------
// Spawn flags
//-----------------------------------------------------------------------------
enum
{
	SF_NO_IMPACT_SOUND = 0x1,
	SF_NO_LAUNCH_SOUND = 0x2,
	SF_START_IMPACTED = 0x1000,
	SF_LAND_AT_INITIAL_POSITION = 0x2000,
	SF_NO_SMOKE = 0x4000,
	SF_NO_SHAKE = 0x8000,
	SF_REMOVE_ON_IMPACT = 0x10000,
};

#ifndef CLIENT_DLL
BEGIN_DATADESC( CEnvGasCanister )
	DEFINE_FIELD( m_vecStartPosition,			FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_vecEnterWorldPosition,		FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_vecDirection,				FIELD_VECTOR ),
	DEFINE_FIELD( m_vecStartAngles,				FIELD_VECTOR ),
	DEFINE_FIELD( m_flLaunchTime,				FIELD_TIME ),
	DEFINE_FIELD( m_flWorldEnterTime,			FIELD_FLOAT ),
	DEFINE_FIELD( m_flInitialZSpeed,			FIELD_FLOAT ),
	DEFINE_FIELD( m_flZAcceleration,			FIELD_FLOAT ),
	DEFINE_FIELD( m_flHorizSpeed,				FIELD_FLOAT ),
	DEFINE_FIELD( m_bLaunchedFromWithinWorld,	FIELD_BOOLEAN ),
	DEFINE_FIELD( m_vecSkyboxOrigin,			FIELD_VECTOR ),
	DEFINE_FIELD( m_vecParabolaDirection,		FIELD_VECTOR ),
	DEFINE_FIELD( m_flSkyboxScale,				FIELD_FLOAT ),
	DEFINE_FIELD( m_bInSkybox,					FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bDoImpactEffects,			FIELD_BOOLEAN ),

	DEFINE_FIELD( m_bLanded, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_vecImpactPosition, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_bIncomingSoundStarted, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bHasDetonated, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bLaunched, FIELD_BOOLEAN ),
	DEFINE_KEYFIELD( m_flDamageRadius, FIELD_FLOAT, "DamageRadius" ),
	DEFINE_KEYFIELD( m_flDamage, FIELD_FLOAT, "Damage" ),

	// Function Pointers.
	DEFINE_FUNCTION( GasCanisterWorldThink ),

	// Inputs
	DEFINE_INPUTFUNC( FIELD_VOID, "FireCanister", InputFireCanister ),

	// Outputs
	DEFINE_OUTPUT( m_OnLaunched, "OnLaunched" ),
	DEFINE_OUTPUT( m_OnImpacted, "OnImpacted" ),

END_DATADESC()
#endif

IMPLEMENT_NETWORKCLASS_ALIASED( EnvGasCanister, DT_EnvGasCanister )

LINK_ENTITY_TO_CLASS_ALIASED( env_gascanister, EnvGasCanister );
PRECACHE_REGISTER( env_gascanister )

BEGIN_NETWORK_TABLE( CEnvGasCanister, DT_EnvGasCanister )
#if !defined( CLIENT_DLL )
	SendPropFloat	( SENDINFO( m_flFlightSpeed ),			0, SPROP_NOSCALE ),	
	SendPropTime	( SENDINFO( m_flLaunchTime ) ),
	SendPropVector	( SENDINFO( m_vecParabolaDirection ),	0, SPROP_NOSCALE ),	

	SendPropFloat	( SENDINFO( m_flFlightTime ),			0, SPROP_NOSCALE ),
	SendPropFloat	( SENDINFO( m_flWorldEnterTime ),		0, SPROP_NOSCALE ),

	SendPropFloat	( SENDINFO( m_flInitialZSpeed ),		0, SPROP_NOSCALE ),
	SendPropFloat	( SENDINFO( m_flZAcceleration ),		0, SPROP_NOSCALE ),
	SendPropFloat	( SENDINFO( m_flHorizSpeed ),			0, SPROP_NOSCALE ),
	SendPropBool	( SENDINFO( m_bLaunchedFromWithinWorld ) ),
	
	SendPropVector	( SENDINFO( m_vecImpactPosition ),       0, SPROP_NOSCALE ),	
	SendPropVector	( SENDINFO( m_vecStartPosition ),       0, SPROP_NOSCALE ),	
	SendPropVector	( SENDINFO( m_vecEnterWorldPosition ),  0, SPROP_NOSCALE ),	
	SendPropVector	( SENDINFO( m_vecDirection ),			0, SPROP_NOSCALE ),	
	SendPropVector	( SENDINFO( m_vecStartAngles ),			0, SPROP_NOSCALE ),	

	SendPropVector	( SENDINFO( m_vecSkyboxOrigin ),		0, SPROP_NOSCALE ),	
	SendPropFloat	( SENDINFO( m_flSkyboxScale ),			0, SPROP_NOSCALE ),
	SendPropBool	( SENDINFO( m_bInSkybox ) ),
	SendPropBool	( SENDINFO( m_bDoImpactEffects ) ),
	
	SendPropBool( SENDINFO( m_bLanded ) ),
	SendPropEHandle(SENDINFO(m_hSkyboxCopy)),
	SendPropInt( SENDINFO( m_nMyZoneIndex ) ),

	// MAD HACK TO ALLOW THINGS TO GO OUTSIDE MAP BOUNDS
	SendPropExclude( "DT_BaseEntity", "m_vecOrigin" ),
	SendPropExclude( "DT_BaseEntity", "m_cellbits" ),
	SendPropExclude( "DT_BaseEntity", "m_cellX" ),
	SendPropExclude( "DT_BaseEntity", "m_cellY" ),
	SendPropExclude( "DT_BaseEntity", "m_cellZ" ),
	SendPropVectorXY( SENDINFO( m_vecOrigin ), -1, SPROP_NOSCALE | SPROP_CHANGES_OFTEN, 0.0f, HIGH_DEFAULT, SendProxy_OriginXY ),
	SendPropFloat( SENDINFO_VECTORELEM( m_vecOrigin, 2 ), -1, SPROP_NOSCALE | SPROP_CHANGES_OFTEN, 0.0f, HIGH_DEFAULT, SendProxy_OriginZ ),
#else
	RecvPropFloat	( RECVINFO( m_flFlightSpeed ) ),	
	RecvPropTime	( RECVINFO( m_flLaunchTime ) ),
	RecvPropVector	( RECVINFO( m_vecParabolaDirection ) ),	

	RecvPropFloat	( RECVINFO( m_flFlightTime ) ),	
	RecvPropFloat	( RECVINFO( m_flWorldEnterTime ) ),	

	RecvPropFloat	( RECVINFO( m_flInitialZSpeed ) ),	
	RecvPropFloat	( RECVINFO( m_flZAcceleration ) ),	
	RecvPropFloat	( RECVINFO( m_flHorizSpeed ) ),	
	RecvPropBool	( RECVINFO( m_bLaunchedFromWithinWorld ) ),	

	RecvPropVector	( RECVINFO( m_vecImpactPosition ) ),	
	RecvPropVector	( RECVINFO( m_vecStartPosition ) ),	
	RecvPropVector	( RECVINFO( m_vecEnterWorldPosition ) ),	
	RecvPropVector	( RECVINFO( m_vecDirection ) ),	
	RecvPropVector	( RECVINFO( m_vecStartAngles ) ),	

	RecvPropVector	( RECVINFO( m_vecSkyboxOrigin ) ),	
	RecvPropFloat	( RECVINFO( m_flSkyboxScale ) ),	
	RecvPropBool	( RECVINFO( m_bInSkybox ) ),	
	RecvPropBool	( RECVINFO( m_bDoImpactEffects ) ),	
	
	RecvPropBool( RECVINFO( m_bLanded ) ),
	RecvPropEHandle( RECVINFO(m_hSkyboxCopy) ),
	RecvPropInt( RECVINFO( m_nMyZoneIndex ) ),
	
	// MAD HACK TO ALLOW THINGS TO GO OUTSIDE MAP BOUNDS
	RecvPropVectorXY( RECVINFO_NAME( m_vecNetworkOrigin, m_vecOrigin ) ),
	RecvPropFloat( RECVINFO_NAME( m_vecNetworkOrigin[2], m_vecOrigin[2] ) ),
#endif

END_NETWORK_TABLE()



//=============================================================================
//
// GasCanister Functions.
//

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CEnvGasCanister::CEnvGasCanister()
{
	m_vecStartPosition.Init();
	m_vecDirection.Init();
	m_flFlightSpeed = dev_dz_gascanister_flight_speed.GetFloat();
	m_flLaunchHeight = dev_dz_gascanister_launch_height.GetFloat();

	// This tells the client DLL to not draw trails, etc.
	m_flLaunchTime = -1.0f;

	m_flWorldEnterTime = 0.0f;
	m_flFlightTime = dev_dz_gascanister_flight_time.GetFloat();
	m_bInSkybox = false;
	m_nMyZoneIndex = 0;
	m_bDoImpactEffects = true;
	//m_flMinRefireTime = -1.0f;
	//m_flMaxRefireTime = -1.0f;
#ifdef CLIENT_DLL
	m_bSpawnedSkyboxParticles = false;
	m_flKillImpactParticlesTime = -1.0f;
#endif
}

#ifdef CLIENT_DLL

C_EnvGasCanister::~C_EnvGasCanister()
{
	// Shut down our effect if we have it
	if ( m_trailParticleEffect )
	{
		m_trailParticleEffect->StopEmission( false, false, true );
		m_trailParticleEffect = NULL;
	}
	if ( m_impactParticleEffect )
	{
		m_impactParticleEffect->StopEmission( false, false, true );
		m_impactParticleEffect = NULL;
	}
}

//-----------------------------------------------------------------------------
// On data update
//-----------------------------------------------------------------------------
void C_EnvGasCanister::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );
	if ( updateType == DATA_UPDATE_CREATED )
	{
		SetNextClientThink( CLIENT_THINK_ALWAYS );

		// Create the effect of the correct size
		m_trailParticleEffect = ParticleProp()->Create( ENV_GASCANISTER_PARTICLE_TRAIL, PATTACH_ABSORIGIN_FOLLOW );
		//CreateParticleCollection( const char *pParticleSystemName, float flDelay = 0.0f, int nRandomSeed = 0 );
		if ( m_trailParticleEffect)
		{
			m_trailParticleEffect->SetControlPoint( 1, Vector( 1, 1, 1 ) );
		}

		//RenderPostSkyboxPreMainScene( true ); // draw after the skybox but before the main scene in a special pass

	}

	// Stop client-side simulation on landing
	if ( m_bLanded )
	{
		if ( m_trailParticleEffect )
		{
			m_trailParticleEffect->StopEmission( false, false, false, false );
			m_trailParticleEffect = NULL;
		}

		if ( m_bDoImpactEffects && !m_impactParticleEffect.IsValid() && m_flKillImpactParticlesTime <= 0 )
		{
			//DispatchParticleEffect( ENV_GASCANISTER_PARTICLE_IMPACT, m_vecImpactPosition + Vector( 0, 0, 8 ), vecAngles );	
			m_impactParticleEffect = ParticleProp()->Create( ENV_GASCANISTER_PARTICLE_IMPACT, PATTACH_CUSTOMORIGIN );
			if ( m_impactParticleEffect )
			{
				m_impactParticleEffect->SetControlPoint( 0, m_vecImpactPosition + Vector( 0, 0, 8 ) );
				m_impactParticleEffect->SetControlPointOrientation( 0, Vector( 1, 0, 0 ), Vector( 0, 1, 0 ), Vector( 0, 0, 1 ) );
			}		

			m_flKillImpactParticlesTime = gpGlobals->curtime + 15;
		}

		//SetNextClientThink( CLIENT_THINK_NEVER );
	}
}

//-----------------------------------------------------------------------------
// Compute position
//-----------------------------------------------------------------------------
void C_EnvGasCanister::ClientThink()
{
	Vector vecEndPosition;
	QAngle vecEndAngles;
	GetPositionAtTime( gpGlobals->curtime, vecEndPosition, vecEndAngles );
	SetAbsOrigin( vecEndPosition );
	if ( m_bLanded )
		SetAbsAngles( vec3_angle );
	else
		SetAbsAngles( vecEndAngles );

	if ( m_hSkyboxCopy.Get() )
	{
		Vector vecEndPositionS;
		QAngle vecEndAnglesS;
		GetPositionAtTime( gpGlobals->curtime, vecEndPositionS, vecEndAnglesS, true );
		m_hSkyboxCopy.Get()->SetAbsOrigin( vecEndPositionS );
		m_hSkyboxCopy.Get()->SetAbsAngles( vecEndAngles );

		if ( m_bSpawnedSkyboxParticles == false )
		{
			m_trailParticleEffectSkybox = m_hSkyboxCopy.Get()->ParticleProp()->Create( ENV_GASCANISTER_PARTICLE_TRAIL, PATTACH_ABSORIGIN_FOLLOW );
			if ( m_trailParticleEffectSkybox )
			{
				m_trailParticleEffectSkybox->SetControlPoint( 1, Vector( 1.0f/m_flSkyboxScale, 1.0f/m_flSkyboxScale, 1.0f/m_flSkyboxScale ) );
			}

			m_bSpawnedSkyboxParticles = true;
		}
	
		float flDistSq = ENV_GASCANISTER_KILL_PARTICLES_TIME * m_flFlightSpeed;
		flDistSq *= flDistSq;
		if ( vecEndPosition.DistToSqr( m_vecImpactPosition ) <= flDistSq )
		{
			// stop the trail particle effects a bit sooner
			if ( m_trailParticleEffect && m_trailParticleEffect.IsValid() )
			{
				m_trailParticleEffect->StopEmission( false, false, false, false );
				m_trailParticleEffect = NULL;
			}

			if ( m_trailParticleEffectSkybox && m_trailParticleEffectSkybox.IsValid() )
			{
				m_trailParticleEffectSkybox->StopEmission( false, false, false, false );
				m_trailParticleEffectSkybox = NULL;
			}
		}
	}

	if ( m_bLanded && m_trailParticleEffect )
	{
		m_trailParticleEffect->StopEmission( false, false, false, false );
		m_trailParticleEffect = NULL;

		m_trailParticleEffectSkybox->StopEmission( false, false, false, false );
		m_trailParticleEffectSkybox = NULL;		
	}
	
	CDangerZone *pZone = GetDangerZoneController()->GetDangerZone( m_nMyZoneIndex );
	if ( m_bDoImpactEffects && m_impactParticleEffect && m_impactParticleEffect.IsValid() && pZone )
	{
		float flRadius = (pZone->GetDangerZoneRadius()*2);	
		float flPerc = MIN( flRadius/3000.0f, 1.0f );
		float flPercSpawn = MAX( flPerc, 0.5f ) * 2;
		float flPercAlpha = 1.0f-flPerc;
		m_impactParticleEffect->SetControlPointEntity( 21, this );
		m_impactParticleEffect->SetControlPoint( 21, Vector( flRadius, flPercSpawn, flPercAlpha ) );

		//DevMsg( "[%d] Danger zone effect radius = ", flRadius );

		if ( m_flKillImpactParticlesTime > 0 && m_flKillImpactParticlesTime < gpGlobals->curtime )
		{
			m_impactParticleEffect->StopEmission( false, true, false, false );
			m_impactParticleEffect = NULL;
		}
	}
}
#else

//-----------------------------------------------------------------------------
// Precache!
//-----------------------------------------------------------------------------
void CEnvGasCanister::Precache( void )
{
	BaseClass::Precache();
	PrecacheModel( ENV_GASCANISTER_MODEL );
	PrecacheModel( ENV_GASCANISTER_SKYBOX_MODEL );
	PrecacheModel("sprites/smoke.vmt");

	PrecacheScriptSound( "GasCanister.LaunchSound" );
	PrecacheScriptSound( "GasCanister.AfterLanding" );
	PrecacheScriptSound( "GasCanister.Explosion" );
	PrecacheScriptSound( "GasCanister.IncomingSound" );
	PrecacheScriptSound( "GasCanister.IncomingSoundClose" );	
	PrecacheScriptSound( "GasCanister.GasIdle" );

	PrecacheParticleSystem( ENV_GASCANISTER_PARTICLE_TRAIL );
	PrecacheParticleSystem( ENV_GASCANISTER_PARTICLE_IDLE );
	PrecacheParticleSystem( ENV_GASCANISTER_PARTICLE_IMPACT );

	//UTIL_PrecacheOther( s_pGasClass[m_nGasType] );
}


//-----------------------------------------------------------------------------
// Spawn!
//-----------------------------------------------------------------------------
void CEnvGasCanister::Spawn( void )
{
	BaseClass::Spawn();

	// It doesn't have any real presence at first.
	SetSolid( SOLID_NONE );

	m_vecImpactPosition = GetAbsOrigin();
	m_bIncomingSoundStarted = false;
	m_bLanded = false;
	m_bHasDetonated = false;
}


//-----------------------------------------------------------------------------
// On remove!
//-----------------------------------------------------------------------------
void CEnvGasCanister::UpdateOnRemove()
{
	BaseClass::UpdateOnRemove();
	StopSound( "GasCanister.AfterLanding" );
}


//-----------------------------------------------------------------------------
// Set up the world model
//-----------------------------------------------------------------------------
void CEnvGasCanister::SetupWorldModel()
{
	SetModel( ENV_GASCANISTER_MODEL );
	SetSolid( SOLID_BBOX );

	float flRadius = CollisionProp()->BoundingRadius();
	Vector vecMins( -flRadius, -flRadius, -flRadius );
	Vector vecMaxs( flRadius, flRadius, flRadius );
	SetSize( vecMins, vecMaxs );

}

//-----------------------------------------------------------------------------
// Place the canister in the world
//-----------------------------------------------------------------------------
void CEnvGasCanister::PlaceCanisterInWorld()
{
	// Are we launching from a point? If so, use that point.
	CPointEntity *pLaunchPos = GetDangerZoneController()->GetGasCanLaunchPosition( GetAbsOrigin() );
	//CPointEntity *pLaunchPos = dynamic_cast< CPointEntity* >( gEntList.FindEntityByClassname( NULL, "info_gascanister_launchpoint" ) );
	if ( pLaunchPos != NULL )
	{
		// create a skybox version as well to fly at the same time
		InitInSkyboxCopy();

		Vector vecStartPos = pLaunchPos->GetAbsOrigin();
		if ( pLaunchPos->DetectInSkybox() )
		{
			VectorMA( Vector(0, 0, 0), m_flSkyboxScale, pLaunchPos->GetAbsOrigin() - m_vecSkyboxOrigin, vecStartPos );
		}

		SetupWorldModel();

		Vector vecForward, vecImpactDirection;
		GetVectors( &vecForward, NULL, NULL );
		VectorMultiply( vecForward, -1.0f, vecImpactDirection );

		InitInWorld( gpGlobals->curtime, vecStartPos, GetAbsAngles(), 
			vecImpactDirection, m_vecImpactPosition, true );
		SetThink( &CEnvGasCanister::GasCanisterWorldThink );
		SetNextThink( gpGlobals->curtime );
	}

	Vector vecEndPosition;
	QAngle vecEndAngles;
	GetPositionAtTime( gpGlobals->curtime, vecEndPosition, vecEndAngles );
	SetAbsOrigin( vecEndPosition );
	SetAbsAngles( vecEndAngles );

	if ( m_hSkyboxCopy.Get() )
	{
		Vector vecEndPositionS;
		QAngle vecEndAnglesS;
		GetPositionAtTime( gpGlobals->curtime, vecEndPositionS, vecEndAnglesS, true );
		m_hSkyboxCopy.Get()->SetAbsOrigin( vecEndPositionS );
		m_hSkyboxCopy.Get()->SetAbsAngles( vecEndAnglesS );
	}
}

	
//-----------------------------------------------------------------------------
// Fires the canister!
//-----------------------------------------------------------------------------
void CEnvGasCanister::InputFireCanister( inputdata_t &inputdata )
{
	if (m_bLaunched)
		return;

	m_bLaunched = true;

	if ( HasSpawnFlags( SF_START_IMPACTED ) )
	{
		//StartSpawningGas( 0.01f );
		return;
	}

	// Play a firing sound
	CPASAttenuationFilter filter( this, ATTN_NONE );

	if ( !HasSpawnFlags( SF_NO_LAUNCH_SOUND ) )
	{
		EmitSound( filter, entindex(), "GasCanister.LaunchSound" );
		EmitSound( filter, entindex(), "GasCanister.IncomingSound" );
	}

	// Place the canister
	PlaceCanisterInWorld();

	// Fire that output!
	m_OnLaunched.Set( this, this, this );
}


//=============================================================================
//
// Enumerator for swept bbox collision.
//
class CCollideList : public IEntityEnumerator
{
public:
	CCollideList( Ray_t *pRay, CBaseEntity* pIgnoreEntity, int nContentsMask ) : 
		m_Entities( 0, 32 ), m_pIgnoreEntity( pIgnoreEntity ),
		m_nContentsMask( nContentsMask ), m_pRay(pRay), m_bIsSkybox(false) {}

	virtual bool EnumEntity( IHandleEntity *pHandleEntity )
	{
		// Don't bother with the ignore entity.
		if ( pHandleEntity == m_pIgnoreEntity )
			return true;

		Assert( pHandleEntity );

		trace_t tr;
		enginetrace->ClipRayToEntity( *m_pRay, m_nContentsMask, pHandleEntity, &tr );
		if (( tr.fraction < 1.0f ) || (tr.startsolid) || (tr.allsolid))
		{
			if ( tr.DidHit() && tr.surface.flags & SURF_SKY )
				m_bIsSkybox = true;

			CBaseEntity *pEntity = gEntList.GetBaseEntity( pHandleEntity->GetRefEHandle() );
			m_Entities.AddToTail( pEntity );
		}

		return true;
	}

	CUtlVector<CBaseEntity*>	m_Entities;
	bool			m_bIsSkybox;

private:
	CBaseEntity		*m_pIgnoreEntity;
	int				m_nContentsMask;
	Ray_t			*m_pRay;
};


//-----------------------------------------------------------------------------
// Test for impact!
//-----------------------------------------------------------------------------
void CEnvGasCanister::TestForCollisionsAgainstEntities( const Vector &vecEndPosition )
{
	// Debugging!!
//	NDebugOverlay::Box( GetAbsOrigin(), m_vecMin * 0.5f, m_vecMax * 0.5f, 255, 255, 0, 0, 5 );
//	NDebugOverlay::Box( vecEndPosition, m_vecMin, m_vecMax, 255, 0, 0, 0, 5 );

	float flRadius = CollisionProp()->BoundingRadius();
	Vector vecMins( -flRadius, -flRadius, -flRadius );
	Vector vecMaxs( flRadius, flRadius, flRadius );

	Ray_t ray;
	ray.Init( GetAbsOrigin(), vecEndPosition, vecMins, vecMaxs );

	CCollideList collideList( &ray, this, MASK_SOLID );
	enginetrace->EnumerateEntities( ray, false, &collideList );

	float flDamage = m_flDamage;

	if ( collideList.m_bIsSkybox )
		m_bDoImpactEffects = false;

	// Now get each entity and react accordinly!
	for( int iEntity = collideList.m_Entities.Count(); --iEntity >= 0; )
	{
		CBaseEntity *pEntity = collideList.m_Entities[iEntity];
		Vector vecForceDir = m_vecDirection;

		// Check for a physics object and apply force!
		IPhysicsObject *pPhysObject = pEntity->VPhysicsGetObject();
		if ( pPhysObject )
		{
			float flMass = PhysGetEntityMass( pEntity );
			vecForceDir *= flMass * 750;
			pPhysObject->ApplyForceCenter( vecForceDir );
		}

		if ( pEntity->m_takedamage && ( m_flDamage != 0.0f ) )
		{
			CTakeDamageInfo info( this, this, flDamage, DMG_BLAST );
			CalculateExplosiveDamageForce( &info, vecForceDir, pEntity->GetAbsOrigin() );
			pEntity->TakeDamage( info );
		}
	}
}


//-----------------------------------------------------------------------------
// Test for impact!
//-----------------------------------------------------------------------------
#define INNER_RADIUS_FRACTION 0.25f

void CEnvGasCanister::TestForCollisionsAgainstWorld( const Vector &vecEndPosition )
{
	// Splash damage!
	// Iterate on all entities in the vicinity.
	float flDamageRadius = m_flDamageRadius;
	float flDamage = m_flDamage;

	CBaseEntity *pEntity;
	for ( CEntitySphereQuery sphere( vecEndPosition, flDamageRadius ); ( pEntity = sphere.GetCurrentEntity() ) != NULL; sphere.NextEntity() )
	{
		if ( pEntity == this )
			continue;

		if ( !pEntity->IsSolid() )
			continue;

		// Get distance to object and use it as a scale value.
		Vector vecSegment;
		VectorSubtract( pEntity->GetAbsOrigin(), vecEndPosition, vecSegment ); 
		float flDistance = VectorNormalize( vecSegment );

		float flFactor = 1.0f / ( flDamageRadius * (INNER_RADIUS_FRACTION - 1) );
		flFactor *= flFactor;
		float flScale = flDistance - flDamageRadius;
		flScale *= flScale * flFactor;
		if ( flScale > 1.0f ) 
		{ 
			flScale = 1.0f; 
		}
		
		// Check for a physics object and apply force!
		Vector vecForceDir = vecSegment;
		IPhysicsObject *pPhysObject = pEntity->VPhysicsGetObject();
		if ( pPhysObject )
		{
			// Send it flying!!!
			float flMass = PhysGetEntityMass( pEntity );
			vecForceDir *= flMass * 750 * flScale;
			pPhysObject->ApplyForceCenter( vecForceDir );
		}

		if ( pEntity->m_takedamage && ( m_flDamage != 0.0f ) )
		{
			CTakeDamageInfo info( this, this, flDamage * flScale, DMG_BLAST );
			CalculateExplosiveDamageForce( &info, vecSegment, pEntity->GetAbsOrigin() );
			pEntity->TakeDamage( info );
		}

		if ( pEntity->IsPlayer() && !(static_cast<CBasePlayer*>(pEntity)->IsInAVehicle()) )
		{
			if (vecSegment.z < 0.1f)
			{
				vecSegment.z = 0.1f;
				VectorNormalize( vecSegment );					
			}
			float flAmount = SimpleSplineRemapVal( flScale, 0.0f, 1.0f, 250.0f, 1000.0f );
			pEntity->ApplyAbsVelocityImpulse( vecSegment * flAmount );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvGasCanister::SetLanded( void )
{
	SetAbsOrigin( m_vecImpactPosition );
	SetMoveType( MOVETYPE_NONE );
	SetSolid( SOLID_VPHYSICS );
	VPhysicsInitStatic();
	
	// resetting the angle so the particles orient the correct way, not a final solution!
	SetAbsAngles( vec3_angle );

	//IncrementInterpolationFrame();
	m_bLanded = true;

	EmitSound( "GasCanister.GasIdle" );

	if ( m_hSkyboxCopy.Get() )
	{
		UTIL_Remove( m_hSkyboxCopy );
	}
}

//-----------------------------------------------------------------------------
// Landed!
//-----------------------------------------------------------------------------
void CEnvGasCanister::Landed( void )
{
	// Lock us now that we've stopped
	SetLanded();

	SetThink( NULL );

}


//-----------------------------------------------------------------------------
// Creates the explosion effect
//-----------------------------------------------------------------------------
void CEnvGasCanister::Detonate( )
{
	// Send the impact output
	m_OnImpacted.FireOutput( this, this, 0 );

	StopSound( "GasCanister.IncomingSoundClose" );
	StopSound( "GasCanister.IncomingSound" );
	EmitSound( "GasCanister.Explosion" );

	// If we're supposed to be removed, do that now
	if ( HasSpawnFlags( SF_REMOVE_ON_IMPACT ) )
	{
		SetAbsOrigin( m_vecImpactPosition );
		SetMoveType( MOVETYPE_NONE );
		//IncrementInterpolationFrame();
		m_bLanded = true;
		
		// Become invisible so our trail can finish up
		AddEffects( EF_NODRAW );
		SetSolidFlags( FSOLID_NOT_SOLID );

		SetThink( &CEnvGasCanister::SUB_Remove );
		SetNextThink( gpGlobals->curtime + ENV_GASCANISTER_TRAIL_TIME );

		return;
	}

	// Test for damaging things
	TestForCollisionsAgainstWorld( m_vecImpactPosition );

	// Shake the screen
	float shakeRadius = dev_dz_gascanister_shake_radius.GetFloat();

	UTIL_ScreenShake( m_vecImpactPosition, dev_dz_gascanister_shake_amplitude.GetFloat(), 150.0, 1.0, shakeRadius, SHAKE_START );

	// Do explosion effects
	ExplosionCreate( m_vecImpactPosition, GetAbsAngles(), this, 50.0f, 500.0f, 
		SF_ENVEXPLOSION_NODLIGHTS | SF_ENVEXPLOSION_NOSPARKS | SF_ENVEXPLOSION_NODAMAGE | SF_ENVEXPLOSION_NOSOUND, 1300.0f );

	trace_t		tr;
	UTIL_TraceLine( m_vecImpactPosition + Vector( 0, 0, 12 ), m_vecImpactPosition + Vector( 0, 0, -64 ), MASK_SHOT_HULL, this, COLLISION_GROUP_NONE, &tr );
	surfacedata_t *pSurfaceData = physprops->GetSurfaceData( tr.surface.surfaceProps );
	int contents = UTIL_PointContents ( tr.endpos, MASK_ALL );
	const char *pEffectName = GetExplosionParticleSystemName( contents, pSurfaceData );
	QAngle	vecAngles( 0, 0, 0 );
	if ( pEffectName != NULL )
	{
		DispatchParticleEffect( pEffectName, m_vecImpactPosition + Vector( 0, 0, 8 ), vecAngles );
	}

	//DispatchParticleEffect( ENV_GASCANISTER_PARTICLE_IMPACT, m_vecImpactPosition + Vector( 0, 0, 8 ), vecAngles );	
}

const char *CEnvGasCanister::GetExplosionParticleSystemName( int pointContents, surfacedata_t *pdata )
{
	if ( pointContents & MASK_WATER )
		return "explosion_basic_water";

	if ( pdata )
	{
		switch ( pdata->game.material )
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

	return "explosion_hegrenade_dirt";
}

//-----------------------------------------------------------------------------
// Purpose: This think function simulates (moves/collides) the GasCanister while in
//          the world.
//-----------------------------------------------------------------------------
void CEnvGasCanister::GasCanisterWorldThink( void )
{
	// Get the current time.
	float flTime = gpGlobals->curtime;

	Vector vecStartPosition = GetAbsOrigin();

	// Update GasCanister position for swept collision test.
	Vector vecEndPosition;
	QAngle vecEndAngles;
	GetPositionAtTime( flTime, vecEndPosition, vecEndAngles );

	if ( !m_bIncomingSoundStarted && !HasSpawnFlags( SF_NO_IMPACT_SOUND ) )
	{
		float flDistSq = ENV_GASCANISTER_INCOMING_SOUND_TIME * m_flFlightSpeed;
		flDistSq *= flDistSq;
		if ( vecEndPosition.DistToSqr(m_vecImpactPosition) <= flDistSq )
		{
			// Figure out if we're close enough to play the incoming sound
			EmitSound( "GasCanister.IncomingSoundClose" );
			m_bIncomingSoundStarted = true;
		}
	}

	TestForCollisionsAgainstEntities( vecEndPosition );
	if ( DidImpact( flTime ) )
	{
		if ( !m_bHasDetonated )
		{
			Detonate();
			m_bHasDetonated = true;
		}
		
		if ( !HasSpawnFlags( SF_REMOVE_ON_IMPACT ) )
		{
			Landed();
		}

		return;
	}
		   
	// Always move full movement.
	SetAbsOrigin( vecEndPosition );

	// Touch triggers along the way
	PhysicsTouchTriggers( &vecStartPosition );

	SetNextThink( gpGlobals->curtime + 0.2f );
	SetAbsAngles( vecEndAngles );

	if ( m_hSkyboxCopy.Get() )
	{
		Vector vecEndPositionS;
		QAngle vecEndAnglesS;
		GetPositionAtTime( gpGlobals->curtime, vecEndPositionS, vecEndAnglesS, true );
		m_hSkyboxCopy.Get()->SetAbsOrigin( vecEndPositionS );
		m_hSkyboxCopy.Get()->SetAbsAngles( vecEndAnglesS );
	}

	if ( !m_bHasDetonated )
	{
		if ( vecEndPosition.DistToSqr( m_vecImpactPosition ) < BoundingRadius() * BoundingRadius() )
		{
			Detonate();
			m_bHasDetonated = true;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pInfo - 
//			bAlways - 
//-----------------------------------------------------------------------------
void CEnvGasCanister::SetTransmit( CCheckTransmitInfo *pInfo, bool bAlways )
{
	// Are we already marked for transmission?
	if ( pInfo->m_pTransmitEdict->Get( entindex() ) )
		return;

	BaseClass::SetTransmit( pInfo, bAlways );
}

#endif

//-----------------------------------------------------------------------------
// Creates a gas canister in the world
//-----------------------------------------------------------------------------
void CEnvGasCanister::InitInWorld( float flLaunchTime, 
	const Vector &vecStartPosition, const QAngle &vecStartAngles, 
	const Vector &vecDirection, const Vector &vecImpactPosition, bool bLaunchedFromWithinWorld )
{
	Vector vecActualStartPosition = vecStartPosition;
	if ( !bLaunchedFromWithinWorld )
	{
		// Move the start position inward if it's too close
		Vector vecDelta;
		VectorSubtract( vecStartPosition, vecImpactPosition, vecDelta );
		VectorNormalize( vecDelta );

		VectorMA( vecImpactPosition, m_flFlightTime * m_flFlightSpeed, vecDelta, vecActualStartPosition );
	}
 
	// Setup initial parametric state.
	m_flLaunchTime = flLaunchTime;
	m_vecStartPosition = vecActualStartPosition;
	m_vecEnterWorldPosition = vecActualStartPosition;
	m_vecDirection = vecDirection;
	m_vecStartAngles = vecStartAngles;
	m_flWorldEnterTime = 0.0f;
	m_bInSkybox = false;
	m_bLaunchedFromWithinWorld = bLaunchedFromWithinWorld;
 
	if ( m_bLaunchedFromWithinWorld )
	{
		//m_flSkyboxScale = 1;
		//m_vecSkyboxOrigin = vec3_origin;

		float flLength = m_vecDirection.Get().AsVector2D().Length();
		VectorSubtract(vecImpactPosition, vecStartPosition, m_vecParabolaDirection.GetForModify());
		m_vecParabolaDirection.GetForModify().z = 0;
		float flTotalDistance = VectorNormalize( m_vecParabolaDirection.GetForModify() );
		m_vecDirection.GetForModify().x = flLength * m_vecParabolaDirection.Get().x;
		m_vecDirection.GetForModify().y = flLength * m_vecParabolaDirection.Get().y;
 
		m_flHorizSpeed = flTotalDistance / m_flFlightTime;
		m_flWorldEnterTime = 0;
 
		float flFinalZSpeed = m_vecDirection.Get().z * m_flHorizSpeed;
		m_flFlightSpeed = sqrt( m_flHorizSpeed * m_flHorizSpeed + flFinalZSpeed * flFinalZSpeed );
		m_flInitialZSpeed = (2.0f * ( vecImpactPosition.z - vecStartPosition.z ) - flFinalZSpeed * m_flFlightTime) / m_flFlightTime;
		m_flZAcceleration = (flFinalZSpeed - m_flInitialZSpeed) / m_flFlightTime;
	}
}


//-----------------------------------------------------------------------------
// Creates a gas canister in the skybox
//-----------------------------------------------------------------------------
void CEnvGasCanister::InitInSkyboxCopy( )
{
#ifndef CLIENT_DLL
	CBaseAnimating *pSkyboxCopy = dynamic_cast< CBaseAnimating* >( CreateEntityByName( "prop_dynamic" ) );
	if ( pSkyboxCopy )
	{
		pSkyboxCopy->SetSolid( SOLID_NONE );
		pSkyboxCopy->AddEFlags( EFL_IN_SKYBOX );
		pSkyboxCopy->SetModel( ENV_GASCANISTER_MODEL );

		m_hSkyboxCopy = pSkyboxCopy;

		CSkyCamera *pCamera = GetCurrentSkyCamera();
		if ( pCamera )
		{
			m_flSkyboxScale = pCamera->m_skyboxData.scale;
			m_vecSkyboxOrigin = pCamera->m_skyboxData.origin;
		}
		
	}	
#endif
}

//-----------------------------------------------------------------------------
// Creates a gas canister in the skybox
//-----------------------------------------------------------------------------
void CEnvGasCanister::InitInSkybox( float flLaunchTime, 
	const Vector &vecStartPosition, const QAngle &vecStartAngles, const Vector &vecDirection,
	const Vector &vecImpactPosition, const Vector &vecSkyboxOrigin, float flSkyboxScale )
{
	// Compute a horizontal speed (constant)
	m_vecParabolaDirection.Init( vecDirection.x, vecDirection.y, 0.0f );
	float flLength = VectorNormalize( m_vecParabolaDirection.GetForModify() ); 
	m_flHorizSpeed = flLength * m_flFlightSpeed;

	// compute total distance to travel
	float flTotalDistance = m_flFlightTime * m_flHorizSpeed;
	flTotalDistance -= vecStartPosition.AsVector2D().DistTo( vecImpactPosition.AsVector2D() );
	if ( flTotalDistance <= 0.0f )
	{
		InitInWorld( flLaunchTime, vecStartPosition, vecStartAngles, vecDirection, vecImpactPosition );
		return;
	}

	// Setup initial parametric state.
	m_flLaunchTime = flLaunchTime;
	m_flWorldEnterTime = flTotalDistance / m_flHorizSpeed;
	m_vecSkyboxOrigin = vecSkyboxOrigin;
	m_flSkyboxScale = flSkyboxScale;

	m_vecEnterWorldPosition = vecStartPosition;
	m_vecDirection = vecDirection;
	m_vecStartAngles = vecStartAngles;
	m_bInSkybox = true;
	m_bLaunchedFromWithinWorld = false;

	// Compute parabolic course
	// Assume the x velocity remains constant.
	// Z moves ballistically, as if under gravity
	// zf + lh = zo
	// vf = vo + a*t
	// zf = zo + vo*t + 0.5 * a * t*t
	// a*t = vf - vo
	// zf = zo + vo*t + 0.5f * (vf - vo) * t
	// zf - zo = 0.5f *vo*t + 0.5f * vf * t
	// -lh - 0.5f * vf * t = 0.5f * vo * t
	// vo = -2.0f * lh / t - vf
	// a = (vf - vo) / t
	m_flHorizSpeed /= flSkyboxScale;

	VectorMA( vecSkyboxOrigin, 1.0f / m_flSkyboxScale, vecStartPosition, m_vecStartPosition.GetForModify() );
	VectorMA( m_vecStartPosition.Get(), -m_flHorizSpeed * m_flWorldEnterTime, m_vecParabolaDirection, m_vecStartPosition.GetForModify() );

	float flLaunchHeight = m_flLaunchHeight / flSkyboxScale;
	float flFinalZSpeed = m_vecDirection.Get().z * m_flFlightSpeed / flSkyboxScale;
	m_vecStartPosition.GetForModify().z += flLaunchHeight;
	m_flZAcceleration = 2.0f * ( flLaunchHeight + flFinalZSpeed * m_flWorldEnterTime ) / ( m_flWorldEnterTime * m_flWorldEnterTime );
	m_flInitialZSpeed = flFinalZSpeed - m_flZAcceleration * m_flWorldEnterTime;
}


//-----------------------------------------------------------------------------
// Convert from skybox to world
//-----------------------------------------------------------------------------
void CEnvGasCanister::ConvertFromSkyboxToWorld()
{
	Assert( m_bInSkybox );
	m_bInSkybox = false;
}


//-----------------------------------------------------------------------------
// Returns the time at which it enters the world
//-----------------------------------------------------------------------------
float CEnvGasCanister::GetEnterWorldTime() const
{
	return m_flWorldEnterTime;
}


//-----------------------------------------------------------------------------
// Did we impact?
//-----------------------------------------------------------------------------
bool CEnvGasCanister::DidImpact( float flTime ) const
{
	return (flTime - m_flLaunchTime) >= m_flFlightTime;
}


//-----------------------------------------------------------------------------
// Computes the position of the canister
//-----------------------------------------------------------------------------
void CEnvGasCanister::GetPositionAtTime( float flTime, Vector &vecPosition, QAngle &vecAngles, bool bIsSkyboxCopy )
{
	float flDeltaTime = flTime - m_flLaunchTime;
	if ( flDeltaTime > m_flFlightTime )
	{
		flDeltaTime = m_flFlightTime;
	}

	Vector vecStartPos = m_vecStartPosition;
	float flHorizSpeed = m_flHorizSpeed;
	float flInitialZSpeed = m_flInitialZSpeed;
	float flZAcceleration = m_flZAcceleration;
	if ( bIsSkyboxCopy )
	{
		Vector vecSkyboxOrigin = m_vecSkyboxOrigin;
		flHorizSpeed /= m_flSkyboxScale;
		flInitialZSpeed /= m_flSkyboxScale;
		flZAcceleration /= m_flSkyboxScale;
		VectorMA( vecSkyboxOrigin, 1.0f / m_flSkyboxScale, m_vecStartPosition, vecStartPos );	
	}

	VMatrix initToWorld;
	if ( m_bLaunchedFromWithinWorld || m_bInSkybox )
	{
		VectorMA( vecStartPos, flDeltaTime * flHorizSpeed, m_vecParabolaDirection, vecPosition );
		vecPosition.z += flInitialZSpeed * flDeltaTime + 0.5f * flZAcceleration * flDeltaTime * flDeltaTime;

		Vector vecLeft;
		CrossProduct( m_vecParabolaDirection, Vector( 0, 0, 1 ), vecLeft );

		Vector vecForward;
		VectorMultiply( m_vecParabolaDirection, -1.0f, vecForward );
		vecForward.z = -(flInitialZSpeed + flZAcceleration * flDeltaTime) / flHorizSpeed;	// This is -dz/dx.
		VectorNormalize( vecForward );

		Vector vecUp;
		CrossProduct( vecForward, vecLeft, vecUp );
 
		initToWorld.SetBasisVectors( vecForward, vecLeft, vecUp );
	}
	else
	{
		flDeltaTime -= m_flWorldEnterTime;
		Vector vecVelocity;
		VectorMultiply( m_vecDirection, m_flFlightSpeed, vecVelocity );
		VectorMA( m_vecEnterWorldPosition, flDeltaTime, vecVelocity, vecPosition );

		MatrixFromAngles( m_vecStartAngles.Get(), initToWorld );
	}

	VMatrix rotation;
	MatrixBuildRotationAboutAxis( rotation, Vector( 1, 0, 0 ), flDeltaTime * ROTATION_SPEED );

	VMatrix newAngles;
	MatrixMultiply( initToWorld, rotation, newAngles );
	MatrixToAngles( newAngles, vecAngles );
}

/*static*/ float CEnvGasCanister::GetGasCanisterFlightTime() { return dev_dz_gascanister_flight_time.GetFloat(); }