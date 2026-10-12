
#include "cbase.h"
#include "dronegun.h"
#include "cs_gamerules.h"


#ifdef GAME_DLL
#include "cs_player.h"
#include "team.h"
#include "in_buttons.h"
#include "particle_parse.h"		// for DispatchParticleEffect
#include "weapon_csbase.h"
#include "collisionutils.h"
#include "dangerzone_controller.h"
#include "br_items.h"
#endif

IMPLEMENT_NETWORKCLASS_ALIASED( Dronegun, DT_Dronegun )

#ifdef CLIENT_DLL

BEGIN_NETWORK_TABLE( CDronegun, DT_Dronegun )
RecvPropVector( RECVINFO( m_vecAttentionTarget ) ),
RecvPropVector( RECVINFO( m_vecTargetOffset ) ),
RecvPropInt( RECVINFO( m_iHealth ) ),
RecvPropBool( RECVINFO( m_bHasTarget ) ),
END_NETWORK_TABLE()

#else

BEGIN_NETWORK_TABLE( CDronegun, DT_Dronegun )
SendPropVector( SENDINFO( m_vecAttentionTarget ) ),
SendPropVector( SENDINFO( m_vecTargetOffset ) ),
SendPropInt( SENDINFO( m_iHealth ), 10 ),
SendPropBool( SENDINFO( m_bHasTarget ) ),

END_NETWORK_TABLE()

#endif

LINK_ENTITY_TO_CLASS_ALIASED( dronegun, Dronegun )

DEVELOPMENT_ONLY_CONVAR( dev_dronegun_health, 220 );
DEVELOPMENT_ONLY_CONVAR( dev_dronegun_range, 1500 );
DEVELOPMENT_ONLY_CONVAR( dev_dronegun_burst_min, 8 );
DEVELOPMENT_ONLY_CONVAR( dev_dronegun_burst_max, 12 );
DEVELOPMENT_ONLY_CONVAR( dev_dronegun_damage_scale, 0.2 );
DEVELOPMENT_ONLY_CONVAR( dev_dronegun_spread, 0.02 );
DEVELOPMENT_ONLY_CONVAR( dev_dronegun_tracking_speed, 180 );
DEVELOPMENT_ONLY_CONVAR( dev_dronegun_recovery_speed, 72 );
DEVELOPMENT_ONLY_CONVAR( dev_dronegun_outside_zone_damage_rate, 3 );

#define DRONEGUN_MDL "models/props_survival/dronegun/dronegun.mdl"

const char *sDronegunGibs[] = {
	"models/props_survival/dronegun/dronegun_gib1.mdl",
	"models/props_survival/dronegun/dronegun_gib2.mdl",
	"models/props_survival/dronegun/dronegun_gib3.mdl",
	"models/props_survival/dronegun/dronegun_gib4.mdl",
	"models/props_survival/dronegun/dronegun_gib5.mdl",
	"models/props_survival/dronegun/dronegun_gib6.mdl",
	"models/props_survival/dronegun/dronegun_gib7.mdl",
	"models/props_survival/dronegun/dronegun_gib8.mdl"
};

CDronegun::CDronegun()
{
	m_nPoseParamPitch = -1;
	m_nPoseParamYaw = -1;
	m_nAttachMuzzle = -1;

	m_vecAttentionTarget = vec3_origin;
	m_vecTargetOffset = vec3_origin;
	m_vecAttentionCurrent = vec3_origin;

	m_bVarInit = false;
	m_bHasTarget = false;

#ifdef GAME_DLL
	m_flLastShootTime = 0;
	m_flLastSound1 = 0;
	m_flLastSound2 = 0;
	m_flLastSound3 = 0;
	m_flDisorientEndTime = 0;
#endif

	m_takedamage = DAMAGE_YES;
	m_iHealth = dev_dronegun_health.GetInt();
}

CDronegun::~CDronegun()
{
#ifndef CLIENT_DLL
	if ( m_hEnvGunfire.Get() )
	{
		UTIL_Remove( m_hEnvGunfire );
	}
#endif
}

void CDronegun::Precache()
{
	BaseClass::Precache();
	PrecacheModel( DRONEGUN_MDL );

	for ( int i = 0; i < ARRAYSIZE( sDronegunGibs ); i++ )
	{
		PrecacheModel( sDronegunGibs[i] );
	}

	PrecacheParticleSystem( "env_sparks_directional" );

	PrecacheScriptSound( "Survival.DroneGunSpark" );
	PrecacheScriptSound( "Survival.DroneGunBreakApart" );
	PrecacheScriptSound( "Survival.DroneGunBreakApartElectricalFailure" );
	PrecacheScriptSound( "Survival.DroneGunFocusPlayer" );
	PrecacheScriptSound( "Survival.DroneGunScanForPlayer" );
	PrecacheScriptSound( "Survival.DroneGunScanForPlayerOutOfRange" );
}

void CDronegun::Spawn()
{
	m_iHealth = dev_dronegun_health.GetInt();

#ifdef CLIENT_DLL
	SetNextClientThink( CLIENT_THINK_ALWAYS );
	m_flNextSpark = 0;
#else
	Precache();
	SetModel( DRONEGUN_MDL );
	BaseClass::Spawn();

	SetSolid( SOLID_BBOX );
	SetMoveType( MOVETYPE_NONE );
	SetCollisionGroup( COLLISION_GROUP_NONE );

	SetThink( &CDronegun::TraceToGround );
	SetNextThink( gpGlobals->curtime + 0.5f );

	if ( !IsKillable() )
	{
		m_nSkin = 1;
	}

#endif
	
}

void CDronegun::VarInit( void )
{
	if ( m_bVarInit )
		return;

	m_bVarInit = true;
	m_vecAttentionTarget = GetAbsOrigin();
	m_vecAttentionCurrent = GetAbsOrigin();
}

#ifdef CLIENT_DLL

void CDronegun::PreDataUpdate( DataUpdateType_t updateType )
{
	if ( CachePoseParamIndices() )
	{
		m_flClientPoseParamYaw = GetPoseParameter( m_nPoseParamYaw );
		m_flClientPoseParamPitch = GetPoseParameter( m_nPoseParamPitch );
	}
	BaseClass::PreDataUpdate( updateType );
}

void CDronegun::PostDataUpdate( DataUpdateType_t updateType )
{
	BaseClass::PostDataUpdate( updateType );
	if ( CachePoseParamIndices() )
	{
		SetPoseParameter( m_nPoseParamYaw, m_flClientPoseParamYaw );
		SetPoseParameter( m_nPoseParamPitch, m_flClientPoseParamPitch );
	}
	SetAllowFastPath( false );
}

void CDronegun::ClientThink()
{
	float flClientThinkInterval = gpGlobals->curtime - m_flLastClientThinkTime;
	m_flLastClientThinkTime = gpGlobals->curtime;

	VarInit();

	ApproachTarget( flClientThinkInterval );

	ConvertTargetPosToPoseParams( m_vecAttentionCurrent );

	matrix3x4_t matMuzzle;
	GetAttachment( m_nAttachMuzzle, matMuzzle );

	float flRatioHealth = ( float )m_iHealth / dev_dronegun_health.GetFloat();
	if ( flRatioHealth <= 0.75f && m_flNextSpark <= gpGlobals->curtime )	
	{
		DoSpark();
		m_flNextSpark = gpGlobals->curtime + 0.1 + random->RandomFloat( 1, flRatioHealth * 6.0f );
	}

	if ( m_bHasTarget )
	{
		trace_t tr;
		UTIL_TraceLine( matMuzzle.GetOrigin(), RandomVectorOnUnitSphere() + matMuzzle.GetOrigin() + matMuzzle.GetForward() * dev_dronegun_range.GetFloat(), MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );
		
		ClientRenderHandle_t hRenderHandle = GetRenderHandle();
		if ( hRenderHandle != INVALID_CLIENT_RENDER_HANDLE && m_vecLaserTracePos != tr.endpos )
		{
			ClientLeafSystem()->RenderableChanged( GetRenderHandle() );
		}
		m_vecLaserTracePos = tr.endpos;
	}
	else
	{
		ClientRenderHandle_t hRenderHandle = GetRenderHandle();
		if ( hRenderHandle != INVALID_CLIENT_RENDER_HANDLE && m_vecLaserTracePos != GetAbsOrigin() )
		{
			ClientLeafSystem()->RenderableChanged( GetRenderHandle() );
		}
		m_vecLaserTracePos = GetAbsOrigin();
	}

}

void CDronegun::PostBuildTransformations( CStudioHdr *pStudioHdr, BoneVector *pos, BoneQuaternion q[] )
{
	BaseClass::PostBuildTransformations( pStudioHdr, pos, q );

	if ( m_bHasTarget )
	{
		C_BaseAnimating *pAnimating = GetBaseAnimating();
		if ( pAnimating )
		{
			int nIdx = LookupBone( "laser_end" );
			Assert( nIdx >= 0 );

			int oldWritableBones = m_BoneAccessor.GetReadableBones();
			m_BoneAccessor.SetWritableBones( BONE_USED_BY_ANYTHING );
			m_BoneAccessor.GetBoneForWrite( nIdx ).SetOrigin( m_vecLaserTracePos );
			m_BoneAccessor.SetWritableBones( oldWritableBones );
		}
	}
}

void CDronegun::GetRenderBounds( Vector& theMins, Vector& theMaxs )
{
	BaseClass::GetRenderBounds( theMins, theMaxs );

	if ( m_bHasTarget ) // include laser end
	{
		theMins.x = MIN( theMins.x, m_vecLaserTracePos.x );
		theMins.y = MIN( theMins.y, m_vecLaserTracePos.y );
		theMins.z = MIN( theMins.z, m_vecLaserTracePos.z );
		theMaxs.x = MAX( theMaxs.x, m_vecLaserTracePos.x );
		theMaxs.y = MAX( theMaxs.y, m_vecLaserTracePos.y );
		theMaxs.z = MAX( theMaxs.z, m_vecLaserTracePos.z );
	}
}

#endif

//-----------------------------------------------------------------------------
// Purpose: Emits sparks when the turret is damaged
//-----------------------------------------------------------------------------
void CDronegun::DoSpark( void )
{
	float flRatioHealth = ( float )m_iHealth / dev_dronegun_health.GetFloat();

	EmitSound( "Survival.DroneGunSpark" );

	Vector			vel = RandomVector( -128.0f, 128.0f );
	QAngle			angSpark;
	VectorAngles( vel.Normalized(), angSpark );
	DispatchParticleEffect( "env_sparks_directional", WorldSpaceCenter(), Vector( ((1.0f - flRatioHealth) * 1.5f), 1, 1 ), angSpark );
	//DispatchParticleEffect( "env_sparks_omni", WorldSpaceCenter(), Vector( m_nMagnitude, m_nTrailLength, FBitSet( m_spawnflags, SF_SPARK_GLOW ) ), GetAbsAngles() );
}

bool CDronegun::CachePoseParamIndices( void )
{
	if ( m_nPoseParamPitch == -1 )
		m_nPoseParamPitch = LookupPoseParameter( "pitch" );

	if ( m_nPoseParamYaw == -1 )
		m_nPoseParamYaw = LookupPoseParameter( "yaw" );

	if ( m_nAttachMuzzle == -1 )
		m_nAttachMuzzle = LookupAttachment( "muzzle" );

	return m_nPoseParamPitch != -1 && m_nPoseParamYaw != -1;
}

void CDronegun::ApproachTarget( float flInterval )
{
	if ( flInterval <= 0 )
		return;

	Vector vecCurrentToTarget = m_vecAttentionTarget.Get() - m_vecAttentionCurrent;

	Vector vecDelta = vecCurrentToTarget.Normalized() * flInterval * dev_dronegun_tracking_speed.GetFloat();
	if ( vecDelta.LengthSqr() > vecCurrentToTarget.LengthSqr() )
	{
		m_vecAttentionCurrent = m_vecAttentionTarget.Get();
	}
	else
	{
		m_vecAttentionCurrent += vecDelta;
	}

	if ( m_vecTargetOffset.Get().LengthSqr() )
	{
		float flOffsetLength = MIN( m_vecTargetOffset.Get().Length(), 128 );
		flOffsetLength -= (flInterval * dev_dronegun_recovery_speed.GetFloat());
		m_vecTargetOffset = m_vecTargetOffset.Get().Normalized() * flOffsetLength;
	}

}

void CDronegun::ConvertTargetPosToPoseParams( Vector vecTarget )
{
	if ( !CachePoseParamIndices() )
		return;

	Vector vecGunPivot = GetAbsOrigin() + Vector( 0, 0, 35.0f );
	Vector vecToTarget = ((vecTarget + m_vecTargetOffset.Get() - Vector(0,0,10)) - vecGunPivot);

	QAngle angTemp;
	VectorAngles( vecToTarget.Normalized(), Vector( 0, 0, 1 ), angTemp );

	float flNewYaw = clamp( angTemp[YAW], -180, 180 );
	SetPoseParameter( m_nPoseParamYaw, flNewYaw );

	float flNewPitch = clamp( angTemp[PITCH], -45, 45 );
	SetPoseParameter( m_nPoseParamPitch, flNewPitch );

#ifndef CLIENT_DLL
	CEnvGunfire* pEnvGunfire = m_hEnvGunfire.Get();
	if ( pEnvGunfire && CachePoseParamIndices() )
	{
		matrix3x4_t matMuzzle;
		GetAttachment( m_nAttachMuzzle, matMuzzle );

		pEnvGunfire->SetAbsAngles( matMuzzle.ToQAngle() );
		pEnvGunfire->SetAbsOrigin( matMuzzle.GetOrigin() );

		pEnvGunfire->m_vecTargetPosition = matMuzzle.GetOrigin() + matMuzzle.GetForward() * 1024.0f;
	}
#endif

}

#ifdef GAME_DLL

void CDronegun::FireGameEvent( IGameEvent *event )
{
	if ( event->GetBool( "silenced" ) )
	{
		return;
	}

	// Heard an unsilenced gunshot

	CBasePlayer *pPlayer = UTIL_PlayerByUserId( event->GetInt( "userid" ) );
	if ( pPlayer )
	{

		CWeaponCSBase *pWeapon = dynamic_cast<CWeaponCSBase*>(pPlayer->GetActiveWeapon());
		if ( pWeapon && !WeaponIsBallistic( pWeapon->GetWeaponType() ) )
			return; // not a weapon that we care about

		// trace the line for this shot
		Vector vecPlayerEyeForward;
		pPlayer->EyeVectors( &vecPlayerEyeForward );
		trace_t tr;
		UTIL_TraceLine( pPlayer->Weapon_ShootPosition(), pPlayer->Weapon_ShootPosition() + vecPlayerEyeForward * 5000, MASK_SOLID, pPlayer, COLLISION_GROUP_NONE, &tr );

		if ( (tr.DidHit() && tr.m_pEnt == this) || IsSphereIntersectingCone( GetAbsOrigin(), 20.0f, pPlayer->Weapon_ShootPosition(), vecPlayerEyeForward, 0.087f, 0.996f ) )
		{
			// they're shooting at us!!

			bool bUpdatedExistingTarget = false;

			FOR_EACH_VEC( m_vecTargets, n )
			{
				droneguntarget_t* pThisTarget = &m_vecTargets[n];
				if ( pThisTarget->m_nEntIndex == pPlayer->entindex() )
				{
					bUpdatedExistingTarget = true;

					pThisTarget->m_vecLastKnownPos = pPlayer->GetAbsOrigin();
					pThisTarget->m_angLastKnownAngle = pPlayer->GetAbsAngles();

					pThisTarget->m_nThreat = clamp( pThisTarget->m_nThreat + 1, 0, 4 );
					break;
				}
			}

			if ( !bUpdatedExistingTarget )
			{
				droneguntarget_t* pNewTarget = &m_vecTargets[m_vecTargets.AddToTail()];
				pNewTarget->m_nEntIndex = pPlayer->entindex();
				pNewTarget->m_vecLastKnownPos = pPlayer->GetAbsOrigin();
				pNewTarget->m_angLastKnownAngle = pPlayer->GetAbsAngles();

				pNewTarget->m_hEnt = pPlayer;
			}

		}

	}

}

void CDronegun::DisorientForDuration( float flDuration )
{
	if ( m_flDisorientEndTime < gpGlobals->curtime )
	{
		m_flDisorientEndTime = gpGlobals->curtime;
	}

	m_flDisorientEndTime += flDuration;
}

int CDronegun::OnTakeDamage( const CTakeDamageInfo &info )
{

	// droneguns with >= 999 health are unkillable
	if ( !IsKillable() )
		return 0;

	EmitSound("Survival.DroneGunBreakApartElectricalFailure");

	CTakeDamageInfo dmgInfoLocal = info;

	float flDamageScale = 1.f;
	if ( dmgInfoLocal.GetDamageType() & DMG_BURN )
	{
		// fire hurts drone guns more
		flDamageScale = 2.f;
	}
	else if ( dmgInfoLocal.GetDamageType() & DMG_BULLET )
	{
		CWeaponCSBase* pWeapon = dynamic_cast<CWeaponCSBase *>( info.GetWeapon() );
		if ( pWeapon )
		{
			switch( pWeapon->GetWeaponType() )
			{
			case WEAPONTYPE_SNIPER_RIFLE: // sniper rifle should kill dronegun in 2 shots
				flDamageScale = 2.f;
				break;
			}
		}
	}
	else if ( dmgInfoLocal.GetDamageType() & DMG_BLAST )
	{
		// droneguns are weak to all explosions
		flDamageScale = 2.2f;
	}
	else
	{
		DoSpark();
	}

	// scale damage if needed
	if ( flDamageScale != 1.f )
	{
		float flDamage = dmgInfoLocal.GetDamage();
		dmgInfoLocal.SetDamage( flDamage * flDamageScale );
		dmgInfoLocal.SetMaxDamage( flDamage * flDamageScale );
	}

	bool bAttackerIsPlayer = dmgInfoLocal.GetAttacker() != NULL && dmgInfoLocal.GetAttacker()->IsPlayer();

	Vector vecDamagePos = dmgInfoLocal.GetDamagePosition();
	if ( vecDamagePos.LengthSqr() )
	{
		Vector vecGunPivot = GetAbsOrigin() + Vector( 0, 0, 35.0f );
		Vector vecToDmg = (vecDamagePos - vecGunPivot).Normalized();

		matrix3x4_t matMuzzle;
		GetAttachment( m_nAttachMuzzle, matMuzzle );
		Vector vecMuzzleForward = matMuzzle.GetForward();

		Vector vecAwayFromDamage = (vecMuzzleForward - vecToDmg);
		vecAwayFromDamage.z *= 0.1f;

		m_vecTargetOffset += vecAwayFromDamage.Normalized() * 128.0f;
	}
	else
	{
		m_vecTargetOffset += RandomVectorInUnitSphere() * 128.0f;
	}

	float flRatio_old = (float)GetHealth() / dev_dronegun_health.GetFloat();

	if ( bAttackerIsPlayer )
		CCSPlayer::UtilLogPrintfTakeDamageLine( info, this, GetClassname() );

	int nRet = BaseClass::OnTakeDamage( dmgInfoLocal );

	float flRatio_new = (float)GetHealth() / dev_dronegun_health.GetFloat();

	if ( bAttackerIsPlayer )
	{
		CSingleUserRecipientFilter filter( ToBasePlayer( dmgInfoLocal.GetAttacker() ) );
		filter.MakeReliable();

		CCSUsrMsg_UpdateScreenHealthBar msg;
		msg.set_entidx( entindex() );
		msg.set_healthratio_old( flRatio_old );
		msg.set_healthratio_new( flRatio_new );
		msg.set_style( 0 ); // green/red health style
		SendUserMessage( filter, CS_UM_UpdateScreenHealthBar, msg );
	}

	return nRet;
}

void CDronegun::BreakApart( void )
{
	EmitSound( "Survival.DroneGunBreakApart" );
	EmitSound( "Survival.DroneGunBreakApartElectricalFailure" );

	DispatchParticleEffect( "explosion_hegrenade_interior", GetAbsOrigin(), QAngle( 0, 0, 0 ) );

	m_bHasTarget = false;

	QAngle angAligned = GetAbsAngles();
	Vector vecToTarget = m_vecAttentionTarget.Get() + m_vecTargetOffset.Get() - GetAbsOrigin();
	if ( vecToTarget.Length() > 10.0f )
	{
		VectorAngles( vecToTarget.Normalized(), angAligned );
	}

	// gibs
	for ( int i = 0; i < ARRAYSIZE( sDronegunGibs ); i++ )
	{
		CPhysicsProp *pProp = dynamic_cast<CPhysicsProp *>(CreateEntityByName( "prop_physics" ));
		if ( pProp )
		{
			pProp->SetAbsOrigin( GetAbsOrigin() );

			if ( i < 4 ) // quick hack to make the drone 'head' gibs align better
			{
				pProp->SetAbsAngles( angAligned );
			}
			else
			{
				pProp->SetAbsAngles( GetAbsAngles() );
			}
			
			pProp->KeyValue( "model", sDronegunGibs[i] );
			pProp->Spawn();
			pProp->SetCollisionGroup( COLLISION_GROUP_DEBRIS );

			IPhysicsObject *pPhysicsObject = pProp->VPhysicsGetObject();
			if ( pPhysicsObject )
			{
				Vector vecVel = (RandomVectorInUnitSphere() + Vector( 0, 0, 1 )).Normalized() * RandomFloat( 50, 150 );
				pPhysicsObject->AddVelocity( &vecVel, NULL );

				AngularImpulse angSpin = RandomVectorInUnitSphere() * RandomFloat( 30, 60 );
				pPhysicsObject->ApplyTorqueCenter( angSpin );
			}

		}
	}
}

void CDronegun::Event_Killed( const CTakeDamageInfo &info )
{
	BreakApart();

	BaseClass::Event_Killed( info );

	CCSPlayer *pCSPlayer = ToCSPlayer( info.GetAttacker() );

	if ( pCSPlayer )
	{
		pCSPlayer->m_nSentriesDestroyed++;
	}

}

void CDronegun::TraceToGround()
{
	// places the sentry gun at a point where ideally no feet are floating. TODO: make feet actually conform to the surface
	Vector vecMoveTo = GetAbsOrigin();

	Vector vecTemp;
	QAngle angTemp;
	if ( GetAttachment( LookupAttachment( "foot1" ), vecTemp, angTemp ) )
	{
		trace_t tr;
		UTIL_TraceLine( vecTemp + Vector( 0, 0, 32 ), vecTemp - Vector( 0, 0, 32 ), MASK_FLOORTRACE, this, COLLISION_GROUP_NONE, &tr );
		if ( tr.DidHit() )
			vecMoveTo.z = MIN( vecMoveTo.z, tr.endpos.z );
	}
	if ( GetAttachment( LookupAttachment( "foot2" ), vecTemp, angTemp ) )
	{
		trace_t tr;
		UTIL_TraceLine( vecTemp + Vector( 0, 0, 32 ), vecTemp - Vector( 0, 0, 32 ), MASK_FLOORTRACE, this, COLLISION_GROUP_NONE, &tr );
		if ( tr.DidHit() )
			vecMoveTo.z = MIN( vecMoveTo.z, tr.endpos.z );
	}
	if ( GetAttachment( LookupAttachment( "foot3" ), vecTemp, angTemp ) )
	{
		trace_t tr;
		UTIL_TraceLine( vecTemp + Vector( 0, 0, 32 ), vecTemp - Vector( 0, 0, 32 ), MASK_FLOORTRACE, this, COLLISION_GROUP_NONE, &tr );
		if ( tr.DidHit() )
			vecMoveTo.z = MIN( vecMoveTo.z, tr.endpos.z );
	}

	SetAbsOrigin( vecMoveTo );
	SetAbsAngles( vec3_angle );

	CBaseEntity *pEnt = CBaseEntity::Create( "env_gunfire", vecMoveTo, vec3_angle, this );
	if ( pEnt )
	{
		CEnvGunfire* pEnvGunfire = dynamic_cast<CEnvGunfire*>(pEnt);
		if ( pEnvGunfire )
		{
			pEnvGunfire->StopShooting();

			m_hEnvGunfire = pEnvGunfire;

			pEnvGunfire->m_iMinBurstSize = dev_dronegun_burst_min.GetInt();
			pEnvGunfire->m_iMaxBurstSize = dev_dronegun_burst_max.GetInt();
			pEnvGunfire->m_flMinBurstDelay = 2;
			pEnvGunfire->m_flMaxBurstDelay = 3;
			//pEnvGunfire->m_flRateOfFire, FIELD_FLOAT, "rateoffire" ),
			//pEnvGunfire->m_iszShootSound, FIELD_STRING, "shootsound" ),
			//pEnvGunfire->m_iszTracerType, FIELD_STRING, "tracertype" ),
			pEnvGunfire->m_bDisabled = false;
			pEnvGunfire->m_iSpread = 1;
			pEnvGunfire->m_flBias = 0.5f;
			pEnvGunfire->m_bCollide = false;
			pEnvGunfire->m_iszWeaponName = MAKE_STRING("weapon_mac10");
			pEnvGunfire->m_iShotsRemaining = 1500;
			//pEnvGunfire->m_vecSpread, FIELD_VECTOR ),
			//pEnvGunfire->m_vecTargetPosition, FIELD_VECTOR ),
			//pEnvGunfire->m_flTargetDist, FIELD_FLOAT ),
			pEnvGunfire->m_bAllowNullTarget = true;
			pEnvGunfire->m_hTarget = NULL;
			pEnvGunfire->m_bAlwaysWallbangTracer = true;
			pEnvGunfire->m_flDamageScaleValue = dev_dronegun_damage_scale.GetFloat();
			pEnvGunfire->m_flAdditionalSpread = dev_dronegun_spread.GetFloat();

			pEnvGunfire->SetOwnerEntity( this );

			pEnvGunfire->StopShooting();
		}
	}

	// spawn ammo under the dronegun to make it look like it's guarding the ammo
	CSurvivalGameRules* pBRrules = CSGameRules()->GetSurvivalRules();
	if ( pBRrules && IsKillable() )
	{
		CPhysPropAmmoBox *pAmmo = assert_cast<CPhysPropAmmoBox*>(pBRrules->SpawnItem( GetAbsOrigin(), vec3_angle, "prop_ammo_box_generic", -1 ));
		if ( pAmmo )
		{
			pAmmo->SetOriginalSource( "dronegun" );
		}
	}

	SetThink( &CDronegun::ServerThink );
	SetNextThink( gpGlobals->curtime + 3.0f ); // wait a little bit to start thinking, to allow physics props to settle
}

int g_nLastDroneGunServerThinkTick = 0;
void CDronegun::ServerThink()
{
	if ( gpGlobals->tickcount == g_nLastDroneGunServerThinkTick )
	{
		SetNextThink( gpGlobals->curtime );
		return;
	}
	g_nLastDroneGunServerThinkTick = gpGlobals->tickcount;

	VarInit();

	static const float flServerThinkInterval = 0.1f;
	SetNextThink( gpGlobals->curtime + flServerThinkInterval );

	Vector vecGunPivot = GetAbsOrigin() + Vector( 0, 0, 35.0f );

	CDangerZoneController *pZone = GetDangerZoneController();
	if ( pZone /*&& pZone->IsDangerZoneEnabled()*/ )
	{
		if ( !pZone->IsWithinPlayArea( vecGunPivot, 1.0f, -1.0f ) )
		{
			CTakeDamageInfo info( this, this, dev_dronegun_outside_zone_damage_rate.GetInt(), DMG_GENERIC );
			OnTakeDamage( info );
			return;
		}
	}

	if ( IsDisoriented() )
	{
		m_vecAttentionTarget.GetForModify() += RandomVectorInUnitSphere() * 200.0f;
	}

	float flDistToClosestPlayer = dev_dronegun_range.GetFloat();

	CBaseEntity *pEnt = NULL;
	for ( CEntitySphereQuery sphere( GetAbsOrigin(), dev_dronegun_range.GetFloat() ); (pEnt = sphere.GetCurrentEntity()) != NULL; sphere.NextEntity() )
	{
		if ( pEnt && (pEnt->IsPlayer() || pEnt->GetMoveType() == MOVETYPE_VPHYSICS) || pEnt->ClassMatches("decoy_projectile") )
		{

			if ( pEnt->ClassMatches("item_cash") || pEnt->ClassMatches( "drone" ) )
				continue;

			Vector vecEntCenter = pEnt->WorldSpaceCenter();

			if ( !pEnt->IsPlayer() )
			{
				IPhysicsObject *pPhysicsObject = pEnt->VPhysicsGetObject();
				if ( pPhysicsObject )
				{
					QAngle angTemp;
					pPhysicsObject->GetPosition( &vecEntCenter, &angTemp );
				}
			}
			else
			{
				CCSPlayer *pCSPlayer = ToCSPlayer( pEnt );
				if ( pCSPlayer && pCSPlayer->m_bIsSpawnRappelling )
				{
					continue; // do not target spawning players until they are on the ground.
				}

				float flDistToThisPlayer = pEnt->GetAbsOrigin().DistTo( vecGunPivot );
				flDistToClosestPlayer = MIN( flDistToClosestPlayer, flDistToThisPlayer );
			}

			{
				trace_t tr;
				UTIL_TraceLine( vecGunPivot, vecEntCenter, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );

				if ( !tr.DidHit() || tr.m_pEnt != pEnt )
				{

					if ( pEnt->IsPlayer() )
					{
						vecEntCenter = pEnt->EyePosition();

						UTIL_TraceLine( vecGunPivot, vecEntCenter, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );
						if ( !tr.DidHit() || tr.m_pEnt != pEnt )
							continue;
					}
					else
					{
						continue;
					}
					
				}
			}

			bool bUpdatedExistingTarget = false;

			FOR_EACH_VEC( m_vecTargets, n )
			{
				droneguntarget_t* pThisTarget = &m_vecTargets[n];

				if ( pThisTarget->m_nEntIndex == pEnt->entindex() )
				{
					bUpdatedExistingTarget = true;

					if ( !VectorsAreEqual( pThisTarget->m_vecLastKnownPos, vecEntCenter, 5 ) ||
						AngleDiff( pEnt->GetAbsAngles().x, pThisTarget->m_angLastKnownAngle.x ) > 10 ||
						AngleDiff( pEnt->GetAbsAngles().y, pThisTarget->m_angLastKnownAngle.y ) > 10 ||
						AngleDiff( pEnt->GetAbsAngles().y, pThisTarget->m_angLastKnownAngle.y ) > 10 )
					{
						pThisTarget->m_nThreat = clamp( pThisTarget->m_nThreat + 1, 0, 4 );

						pThisTarget->m_vecLastKnownPos = vecEntCenter;
						pThisTarget->m_angLastKnownAngle = pEnt->GetAbsAngles();
					}

					break;
				}
			}

			if ( !bUpdatedExistingTarget )
			{
				droneguntarget_t* pNewTarget = &m_vecTargets[m_vecTargets.AddToTail()];
				pNewTarget->m_nEntIndex = pEnt->entindex();
				pNewTarget->m_vecLastKnownPos = vecEntCenter;
				pNewTarget->m_angLastKnownAngle = pEnt->GetAbsAngles();

				pNewTarget->m_hEnt = pEnt;
			}

		}
	}

	m_bHasTarget = false;

	if ( flDistToClosestPlayer >= dev_dronegun_range.GetFloat() )
	{
		static const float flServerThinkIntervalLong = 1.0f;
		SetNextThink( gpGlobals->curtime + flServerThinkIntervalLong );
	}

	droneguntarget_t* pClosestTarget = NULL;
	float flDist = FLT_MAX;

	FOR_EACH_VEC_BACK( m_vecTargets, n )
	{
		droneguntarget_t* pThisTarget = &m_vecTargets[n];

		if ( !pThisTarget )
		{
			m_vecTargets.Remove( n );
			continue;
		}

		if ( pThisTarget->m_nThreat <= 0 )
			continue;

		float flDistToThisTarget = GetAbsOrigin().DistTo( pThisTarget->m_vecLastKnownPos );

		if ( pThisTarget->m_hEnt.Get() )
		{
			if ( GetAbsOrigin().DistTo( pThisTarget->m_hEnt.Get()->GetAbsOrigin() ) > dev_dronegun_range.GetFloat() )
			{
				pThisTarget->m_nThreat = clamp( pThisTarget->m_nThreat - 1, 0, 4 );

				if ( pThisTarget->m_nThreat <= 0 )
				{
					m_vecTargets.Remove( n );
				}
				continue;
			}
			else
			{
				if ( pThisTarget->m_hEnt.Get()->ClassMatches( "decoy_projectile" ) )
				{
					// hack: treat decoys as the closest target, so the dronegun pays attention to them more than closer players
					flDistToThisTarget = 0;
				}
			}
		}

		if ( flDistToThisTarget < flDist )
		{
			flDist = flDistToThisTarget;
			pClosestTarget = pThisTarget;
		}
	}

	if ( pClosestTarget )
	{
		if ( !IsDisoriented() )
		{
			m_vecAttentionTarget = pClosestTarget->m_vecLastKnownPos;
		}

		if ( gpGlobals->curtime - m_flLastSound1 > 1 )
		{
			m_flLastSound1 = gpGlobals->curtime;
			EmitSound( "Survival.DroneGunFocusPlayer" );
		}
		
	}

	ApproachTarget( flServerThinkInterval );

	ConvertTargetPosToPoseParams( m_vecAttentionCurrent );

	if ( !pClosestTarget )
	{
		CEnvGunfire* pEnvGunfire = m_hEnvGunfire.Get();
		if ( pEnvGunfire )
		{
			pEnvGunfire->StopShooting();
		}

		if ( CachePoseParamIndices() )
		{
			matrix3x4_t matMuzzle;
			GetAttachment( m_nAttachMuzzle, matMuzzle );

			if ( !IsDisoriented() )
			{
				m_vecAttentionTarget = matMuzzle.GetOrigin() - Vector( 0, 0, 100 );
			}
		}

		return;
	}

	float flMaxDist = 512.0f;

	float flDistToTarget = m_vecAttentionCurrent.DistTo( m_vecAttentionTarget.Get() );

	if ( flDistToTarget < flMaxDist )
		m_bHasTarget = true;

	if ( flDistToTarget < 32 )
	{
		float flTimeSinceLastShoot = gpGlobals->curtime - m_flLastShootTime;
		if ( flTimeSinceLastShoot > 2 )
		{
			m_flLastShootTime = gpGlobals->curtime;

			CEnvGunfire* pEnvGunfire = m_hEnvGunfire.Get();
			if ( pEnvGunfire )
			{
				pEnvGunfire->StartShooting();

				if ( pClosestTarget->m_hEnt.Get() && pClosestTarget->m_hEnt.Get()->IsPlayer() )
				{
					IGameEvent * event = gameeventmanager->CreateEvent( "dronegun_attack" );
					if ( event )
					{
						CBasePlayer *pPlayer = ToBasePlayer( pClosestTarget->m_hEnt.Get() );
						event->SetInt( "userid", pPlayer->GetUserID() );
						event->SetInt( "priority", 1 );
						gameeventmanager->FireEvent( event );
					}
				}
			}

			if ( VectorsAreEqual( m_vecAttentionCurrent, m_vecAttentionTarget.Get(), 2 ) )
			{
				pClosestTarget->m_nThreat = clamp( pClosestTarget->m_nThreat - 1, 0, 4 );
			}

		}
	}
	else if ( flDistToTarget >= 32 && flDistToTarget < 128 )
	{
		if ( gpGlobals->curtime - m_flLastSound2 > 1 )
		{
			m_flLastSound2 = gpGlobals->curtime;
			EmitSound( "Survival.DroneGunScanForPlayer" );
		}
	}
	else if ( flDistToTarget > flMaxDist )
	{
		if ( gpGlobals->curtime - m_flLastSound3 > 1 )
		{
			m_flLastSound3 = gpGlobals->curtime;
			EmitSound( "Survival.DroneGunScanForPlayerOutOfRange" );
		}
	}

}
#endif
