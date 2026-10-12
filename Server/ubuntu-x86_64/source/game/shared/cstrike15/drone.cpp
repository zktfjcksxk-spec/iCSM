//========= Copyright © 1996-2012, Valve Corporation, All rights reserved. ============//
//
//=============================================================================//

#include "cbase.h"
#include "drone.h"
#include "cs_gamerules.h"

#ifndef CLIENT_DLL
#include "particle_parse.h"		// for DispatchParticleEffect
#include "IEffects.h"			// for sparks
#include "soundenvelope.h"								// for sound
#include "SoundEmitterSystem/isoundemittersystembase.h"	// for sound
#include "collisionutils.h"		// for intersection tests
#include "weapon_molotov.h"		// for dropping firebombs
#include "molotov_projectile.h"	// for dropping firebombs
#include "world.h"				// for taking damage from the world
#include "util.h"
#include "dangerzone_controller.h" // for asking about zone play area size
#include "br_items.h"
#include "cs_player.h"
#include "vphysics/constraints.h"
#include "item_cash.h"
#endif

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

IMPLEMENT_NETWORKCLASS_ALIASED( Drone, DT_Drone )

#ifdef GAME_DLL
BEGIN_NETWORK_TABLE( CDrone, DT_Drone )
SendPropEHandle( SENDINFO( m_hMoveToThisEntity ) ),
SendPropEHandle( SENDINFO( m_hDeliveryCargo ) ),
END_NETWORK_TABLE()
#else
BEGIN_NETWORK_TABLE( CDrone, DT_Drone )
RecvPropEHandle( RECVINFO( m_hMoveToThisEntity ) ),
RecvPropEHandle( RECVINFO( m_hDeliveryCargo ) ),
END_NETWORK_TABLE()
#endif

IMPLEMENT_AUTO_LIST( IDrone )

LINK_ENTITY_TO_CLASS_ALIASED( drone, Drone )

#ifdef GAME_DLL
DEVELOPMENT_ONLY_CONVAR( dev_drone_drop_cargo, 0 );
DEVELOPMENT_ONLY_CONVAR( dev_drone_default_health, 70 );
DEVELOPMENT_ONLY_CONVAR( dev_drone_cargodrop_moving_timeout, 10 ); // Drones wait this long before delivering to a moving target
DEVELOPMENT_ONLY_CONVAR( dev_drone_cargodrop_within_play_area_buffer, 6 );
DEVELOPMENT_ONLY_CONVAR( dev_drone_upgrade_speed_multiplier, 1.3 );
DEVELOPMENT_ONLY_CONVAR( dev_drone_drop_target_speed, 80 );
DEVELOPMENT_ONLY_CONVAR( dev_drone_zone_speed_multiplier, 1.2 );
DEVELOPMENT_ONLY_CONVAR( dev_drone_debug, 0 );
DEVELOPMENT_ONLY_CONVAR( dev_drone_pitch_min, 170 );
DEVELOPMENT_ONLY_CONVAR( dev_drone_pitch_max, 250 );
#endif // GAME_DLL


#define DRONE_MODEL "models/props_survival/drone/br_drone.mdl"

#ifndef CLIENT_DLL
#if DEVELOPMENT_ONLY
CON_COMMAND_F( sv_drone_state, "", FCVAR_CHEAT )
{
	CBasePlayer *pPlayer = UTIL_GetCommandClient();
	if ( !pPlayer )
		return;

	CBaseEntity *pEntity = NULL;
	while ( (pEntity = gEntList.FindEntityInSphere( pEntity, pPlayer->GetAbsOrigin(), 50000 )) != NULL )
	{
		CDrone *pDrone = dynamic_cast<CDrone*>(pEntity);
		if ( pDrone )
		{
			pDrone->PushNewOrders( droneOrders_t( (droneState_t)V_atoi( args[1] ) ) );
		}
	}

}

CON_COMMAND_F( sv_drone_waypoint, "", FCVAR_CHEAT )
{
	CBasePlayer *pPlayer = UTIL_GetCommandClient();
	if ( !pPlayer )
		return;

	CBaseEntity *pEntity = NULL;
	while ( (pEntity = gEntList.FindEntityInSphere( pEntity, pPlayer->GetAbsOrigin(), 50000 )) != NULL )
	{
		CDrone *pDrone = dynamic_cast<CDrone*>(pEntity);
		if ( pDrone )
		{
			Vector vecPlayerEyeForward;
			pPlayer->EyeVectors( &vecPlayerEyeForward );

			trace_t tr;
			UTIL_TraceLine( pPlayer->Weapon_ShootPosition(), pPlayer->Weapon_ShootPosition() + vecPlayerEyeForward * 5000, MASK_SOLID | CONTENTS_DRONECLIP, pPlayer, COLLISION_GROUP_NONE, &tr );

			pDrone->AddWayPoint( tr.endpos );
			pDrone->PushNewOrders( droneOrders_t( DRONE_STATE_EAT_WAYPOINTS ) );
		}
	}
}

CON_COMMAND_F( sv_drone_destroy, "", FCVAR_CHEAT )
{
	FOR_EACH_VEC( IDrone::AutoList(), nDrone )
	{
		CDrone *pDrone = static_cast< CDrone* >( IDrone::AutoList()[nDrone] );
		pDrone->TakeDamage( CTakeDamageInfo( NULL, NULL, 9999, DMG_GENERIC ) );
	}
}
#endif
#endif



CDrone::CDrone()
{
#ifndef CLIENT_DLL
	m_vecGroundOffset = Vector( 0, 0, 240 );

	m_flLastOrdersChangeTimestamp = 0;

	m_vecLastKnownVelocity = vec3_origin;
	m_vecLastKnownAngImpulse = vec3_origin;
	m_flLastKnownSpeed = 0;

	m_flMaxSpeed = 0;
	m_vecCurrentDestination = vec3_origin;
	m_flLastKnownDistanceToDestination = 0;
	m_flLastKnownWaypointAccuracy = 0;
	
	m_flLastKnownGroundHeight = 0;
	m_flPowerCutUntil = 0;

	m_hAttackTarget = INVALID_EHANDLE;
	m_flLastTimeSawAttackTarget = 0;
	m_vecLastKnownAttackTargetPosition = vec3_origin;
	m_bDoIntervalThink = false;

	m_flLastDroppedGrenadeAt = 0;
	m_flSpawnTimeStamp = 0;
	m_bInPlayArea = true;

	m_hMoveToThisEntity = INVALID_EHANDLE;
	m_vecLastKnownMoveToEntityPosition = vec3_origin;
	m_flTimeArrivedAtMoveToEntity = 0;

	m_hDeliveryCargo = INVALID_EHANDLE;

	m_vecSpawnPosition = vec3_origin;
	m_bQueuingOrders = false;

	m_flAvoidanceTime = 0;

	m_bUpgraded = false;

	m_pDroneRopeConstraint = NULL;
#endif
}

CDrone::~CDrone()
{
#ifndef CLIENT_DLL
	StopDroneSound();
	VPhysicsDestroyObject();
#endif
}

void CDrone::Spawn( void )
{
#ifdef CLIENT_DLL
	BaseClass::Spawn();
	m_flLastTimeCargoWasAttached = 0;
	m_vecClientSideTrailPositions.RemoveAll();
#else
	m_flSpawnTimeStamp = gpGlobals->curtime;

	SetModel( DRONE_MODEL );
	BaseClass::Spawn();

	SetNextThink( gpGlobals->curtime );
	SetThink( &CDrone::DroneThink );
	SetTouch( &CDrone::DroneTouch );

	const model_t *pModel = modelinfo->GetModel( GetModelIndex() );
	if ( pModel )
	{
		Vector mins, maxs;
		modelinfo->GetModelBounds( pModel, mins, maxs );
		SetCollisionBounds( mins, maxs );
	}

	m_vecCurrentDestination = GetAbsOrigin();

	SetHealth( dev_drone_default_health.GetInt() );
	SetMaxHealth( dev_drone_default_health.GetInt() );
	m_takedamage = DAMAGE_YES;

	//ListenForGameEvent( "weapon_fire" );

	StartDroneSound();

	SetCollisionGroup( COLLISION_GROUP_NPC );
#endif
}

#ifdef CLIENT_DLL
void CDrone::PostBuildTransformations( CStudioHdr *pStudioHdr, BoneVector *pos, BoneQuaternion q[] )
{
	BaseClass::PostBuildTransformations( pStudioHdr, pos, q );

	C_BaseAnimating *pDroneAnimating = GetBaseAnimating();
	if ( pDroneAnimating )
	{
		float flTimeSinceCargoDetach = gpGlobals->curtime - m_flLastTimeCargoWasAttached;

		CBaseEntity* pCargo = m_hDeliveryCargo.Get();
		if ( pCargo || flTimeSinceCargoDetach < 2.0f )
		{
			int oldWritableBones = m_BoneAccessor.GetReadableBones();
			m_BoneAccessor.SetWritableBones( BONE_USED_BY_ANYTHING );

			struct DroneCargoAttachMapping
			{
				char *szDroneCableNameA;
				char *szDroneCableNameB;
				char *szCargoRingName;
			};

			static DroneCargoAttachMapping s_CargoAttachNames[4] =
			{
				{ "drone_cable_1a", "drone_cable_1b", "cargo_ring1" },
				{ "drone_cable_2a", "drone_cable_2b", "cargo_ring2" },
				{ "drone_cable_3a", "drone_cable_3b", "cargo_ring3" },
				{ "drone_cable_4a", "drone_cable_4b", "cargo_ring4" },
			};

			for ( int i = 0; i < 4; i++ )
			{
				int nCableA = LookupBone( s_CargoAttachNames[i].szDroneCableNameA ); // hook cable root
				int nCableB = LookupBone( s_CargoAttachNames[i].szDroneCableNameB ); // hook itself

				if ( nCableA < 0 || nCableB < 0 )
				{
					Assert( false );
					continue;
				}
				
				if ( pCargo )
				{
					m_flLastTimeCargoWasAttached = gpGlobals->curtime;

					m_vecLastKnownCargoAttachPositions[i] = pCargo->WorldSpaceCenter();

					int nCargoAttach = pCargo->LookupAttachment( s_CargoAttachNames[i].szCargoRingName );
					if ( nCargoAttach >= 0 )
					{
						pCargo->GetAttachment( nCargoAttach, m_vecLastKnownCargoAttachPositions[i] );

						// place hook on cargo
						m_BoneAccessor.GetBoneForWrite( nCableB ).SetOrigin( m_vecLastKnownCargoAttachPositions[i] );
					}
				}
				else
				{
					// waggle and slerp in on release

					Vector vecCableRootPos = m_BoneAccessor.GetBone( nCableA ).GetOrigin();

					float flLerp = RemapValClamped( flTimeSinceCargoDetach, 2.0f, 0.0f, 0.0f, 1.0f );
					flLerp = Bias( flLerp, 0.05f );

					vecCableRootPos = Lerp( flLerp, vecCableRootPos, m_vecLastKnownCargoAttachPositions[i] );
					//Vector vecWaggle = (m_vecLastKnownCargoAttachPositions[i] - vecCableRootPos).Normalized() * sin( gpGlobals->curtime * (15.0f + i) ) * 5.0f;
					//float flSinWaveAmt = (4.0 * flLerp) * exp( 1.0 - (4.0 * flLerp) );
					//vecCableRootPos += (vecWaggle * flSinWaveAmt);

					m_BoneAccessor.GetBoneForWrite( nCableB ).SetOrigin( vecCableRootPos );
				}
			}

			m_BoneAccessor.SetWritableBones( oldWritableBones );
		}

	}

}

void CDrone::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );
	
	EHANDLE temp = m_hDeliveryCargo.Get();
	if ( temp.IsValid() ) // update clientside trail only when we have cargo (we don't need to resolve the handle)
	{
		Vector vecPt = GetAbsOrigin();
		if ( m_vecClientSideTrailPositions.Count() )
		{
			const Vector2D& vecHeadPos = m_vecClientSideTrailPositions.Tail().AsVector2D();
			if ( vecHeadPos.DistToSqr( vecPt.AsVector2D() ) > (350 * 350) )
			{
				vecPt.z = gpGlobals->curtime; // pack timestamp in z

				m_vecClientSideTrailPositions.AddToTail( vecPt );

				if ( m_vecClientSideTrailPositions.Count() > 32 )
					m_vecClientSideTrailPositions.Remove( 0 );
			}
		}
		else
		{
			vecPt.z = gpGlobals->curtime; // pack timestamp in z
			m_vecClientSideTrailPositions.AddToTail( vecPt );
		}
	}

}

#endif

#ifndef CLIENT_DLL
void CDrone::Precache( void )
{
	SetModelName( MAKE_STRING( DRONE_MODEL ) );
	BaseClass::Precache();
	PrecacheModel( DRONE_MODEL );
}

int	CDrone::UpdateTransmitState()
{
	return SetTransmitState( FL_EDICT_ALWAYS );
}

int CDrone::ShouldTransmit( const CCheckTransmitInfo *pInfo )
{
	return FL_EDICT_ALWAYS;
}

void CDrone::DroneThink( void )
{
	SetNextThink( gpGlobals->curtime ); // TODO: this was tuned at 64 tick, it probably fails at any other frequency.

	IPhysicsObject *pPhysicsObject = VPhysicsGetObject();

	if ( !pPhysicsObject )
		return;

	UpdateDroneSound();

	DebugDraw();

	UpdateCargoConstraint();

	if ( IsPowerOn() )
	{
		StartPropellers();
	}
	else
	{
		StopPropellers();
		return; // power is cut, don't do any more thinking.
	}

	UpdateIntervalThinkTimer();
	
	ComputeCurrentVelocityAndSpeed(); // populate the member variables for velocity and speed.

	CheckPlayArea();
	
	CarryOutOrders();
	CheckStuck();

	// update thrusters!
	HoverThrust(); // apply enough thrust to hover in place
	CollisionAvoidanceThrust(); // apply thrust to stay off the ground and away from walls
	MoveThrust(); // apply thrust to move to destination
	AmbientThrust(); // bob about and lose power if injured


	// update a record of past positions that we can use to re-trace if we get stuck
	UpdatePositionHistory();

	if ( !m_vecSpawnPosition.LengthSqr() )
	{
		m_vecSpawnPosition = GetAbsOrigin();
	}

}

const char * CDrone::GetDroneStatePrintName( droneState_t state )
{
	switch ( state ) {
	case DRONE_STATE_IDLE: return "Idling";
	case DRONE_STATE_EAT_WAYPOINTS: return "Eating waypoints";
	case DRONE_STATE_CYCLE_WAYPOINTS: return "Cycling waypoints";
	case DRONE_STATE_GET_UNSTUCK: return "Getting unstuck";
	case DRONE_STATE_FLY_UP: return "Flying up";
	case DRONE_STATE_WANDER: return "Wandering";
	case DRONE_STATE_INVESTIGATE: return "Investigating";
	case DRONE_STATE_ATTACK: return "Attacking";
	case DRONE_STATE_RETREAT: return "Retreating";
	case DRONE_STATE_MOVE_TO_ENTITY: return "Moving to entity";
	case DRONE_STATE_DROP_CARGO: return "Dropping cargo";
	case DRONE_STATE_RETURN_TO_DESPAWN: return "Returning to despawn";
	case DRONE_STATE_GET_TO_GOOD_CARGO_DROP_POS: return "Finding valid cargo drop pos";
	}
	Assert( false );
	return "UNKNOWN STATE";
}

void CDrone::DroneTouch( CBaseEntity *pOther )
{
	//if ( pOther->IsWorld() )
	//{
	//}
}

int CDrone::OnTakeDamage( const CTakeDamageInfo &info )
{
	
	if ( info.GetDamageType() == DMG_BURN )
	{
		PushNewOrders( droneOrders_t( DRONE_STATE_GET_UNSTUCK, 7 ) );
		return 0;
	}

	if ( IsUpgraded() && m_hDeliveryCargo.Get() )
	{
		return 0; // upgraded drones are invulnerable, at Bank's request
	}

	// cut power when taking damage
	m_flPowerCutUntil = gpGlobals->curtime + RandomFloat(0.5f, 1.5f);

	IPhysicsObject *pPhysicsObject = VPhysicsGetObject();
	if ( pPhysicsObject )
	{
		AngularImpulse angSpin = RandomVectorInUnitSphere() * info.GetDamage() * RandomFloat( 300, 700 );
		pPhysicsObject->ApplyTorqueCenter( angSpin );
	}

	CFmtStr fmtTypeNameCargo;
	if ( CBaseEntity *pCargoCarried = m_hDeliveryCargo.Get() )
	{
		fmtTypeNameCargo = pCargoCarried->GetClassname();

		if ( CPhysPropLootCrate *pCrateProp = dynamic_cast< CPhysPropLootCrate * >( pCargoCarried ) )
			pCrateProp->FormatTypeNameForLogging( fmtTypeNameCargo );
	}

	CFmtStr fmtDroneTypeName( "%s*%s%s*", GetClassname(), fmtTypeNameCargo.Access(), IsUpgraded() ? "+fast" : "" );
	CCSPlayer::UtilLogPrintfTakeDamageLine( info, this, fmtDroneTypeName );

	return BaseClass::OnTakeDamage( info );
}

void CDrone::Event_Killed( const CTakeDamageInfo &info )
{
	if ( info.GetInflictor() && !info.GetInflictor()->IsWorld() )
		EmitSound( "ambient.electrical_zap_9" ); // die loudly from non-world zone damage

	DispatchParticleEffect( "explosion_hegrenade_interior", GetAbsOrigin(), QAngle( 0, 0, 0 ) );

	//CSurvivalGameRules* pBRrules = CSGameRules()->GetSurvivalRules();
	//if ( pBRrules )
	//{
	//	pBRrules->SpawnWeapon( GetAbsOrigin(), "weapon_incgrenade", 1, this );
	//	pBRrules->SpawnWeapon( GetAbsOrigin(), "prop_ammo_box_generic", -1, this );
	//}

	DetachCargo( false, ToBasePlayer( info.GetInflictor() ) );

	StopDroneSound();

	UTIL_SpawnPhysicalCash( GetAbsOrigin(), 0, 1, "killed_deliverydrone" );

	// create a corpse model
	CPhysicsProp *pProp = dynamic_cast<CPhysicsProp *>(CreateEntityByName( "prop_physics" ));
	if ( pProp )
	{
		pProp->SetAbsOrigin( GetAbsOrigin() );
		pProp->SetAbsAngles( GetAbsAngles() );
		pProp->KeyValue( "model", DRONE_MODEL );
		
		pProp->Spawn();

		pProp->SetBodygroup( 0, 1 ); // propellers stopped
		pProp->SetCollisionGroup( COLLISION_GROUP_DEBRIS );
		
		IPhysicsObject *pPhysicsObject = VPhysicsGetObject();
		if ( pPhysicsObject )
		{
			Vector vecVelocity = vec3_origin;
			AngularImpulse angAngularVelocity;
			pPhysicsObject->GetVelocity( &vecVelocity, &angAngularVelocity );

			pProp->SetAbsVelocity( vecVelocity );
		}
	}

	BaseClass::Event_Killed( info );
}

void CDrone::DebugDraw( void )
{
	if ( !dev_drone_debug.GetBool() )
		return;

	static const float flDbgDuration = 0.05f;

	// debug draw waypoints, even when not in waypoint mode
	debugoverlay->AddLineOverlay( GetAbsOrigin(), m_vecCurrentDestination, 255, 255, 0, true, flDbgDuration );

	FOR_EACH_VEC( m_vecWaypointQueue, n )
	{
		debugoverlay->AddSphereOverlay( m_vecWaypointQueue[n], 10.0f, 6, 6, 255, 255, 0, 0, flDbgDuration );
		if ( m_vecWaypointQueue.IsValidIndex( n + 1 ) )
		{
			debugoverlay->AddLineOverlay( m_vecWaypointQueue[n], m_vecWaypointQueue[n + 1], 255, 255, 0, true, flDbgDuration );
		}
	}

	FOR_EACH_VEC( m_vecUnstuckQueue, n )
	{
		debugoverlay->AddSphereOverlay( m_vecUnstuckQueue[n], 10.0f, 6, 6, 255, 0, 0, 0, flDbgDuration );
		if ( m_vecUnstuckQueue.IsValidIndex( n + 1 ) )
		{
			debugoverlay->AddLineOverlay( m_vecUnstuckQueue[n], m_vecUnstuckQueue[n + 1], 255, 0, 0, true, flDbgDuration );
		}
	}

	FOR_EACH_VEC( m_vecPositionHistory, n )
	{
		debugoverlay->AddSphereOverlay( m_vecPositionHistory[n], 2.0f, 6, 6, 0, 150, 240, 0, flDbgDuration );
	}

	FOR_EACH_VEC( m_vecSparsePositionHistory, n )
	{
		debugoverlay->AddSphereOverlay( m_vecSparsePositionHistory[n], 3.0f, 6, 6, 0, 0, 240, 0, flDbgDuration );
	}

	Vector vecTemp = GetAbsOrigin();
	FOR_EACH_VEC( m_Orders, n )
	{
		debugoverlay->AddTextOverlay( vecTemp, flDbgDuration, "%s", GetDroneStatePrintName( m_Orders[n].m_DroneState ) );
		vecTemp.z -= 10.0f;
	}
	
}

void CDrone::SetCurrentDestination( Vector vecPos )
{
	// TODO: smoothly lerp
	//Vector vecOldPosition = m_vecCurrentDestination;

	m_vecCurrentDestination = vecPos;
	ComputeDistanceToCurrentDestination();
	ComputeCurrentWaypointAccuracy();

	//m_vecCurrentDestination = Lerp( 0.1f, vecOldPosition, vecPos );
}

bool CDrone::ShouldIgnorePlayers( void )
{
	// if we're delivering, that takes priority. If we're not in the play area, don't get distracted

	if ( m_hDeliveryCargo.Get() || !m_bInPlayArea )
		return true;

	return false;
}

void CDrone::AttachCargo( CBaseEntity * pCargo )
{
	if ( !pCargo )
		return;

	m_hDeliveryCargo.Set( pCargo );

	m_tCargoCollisionGroup = (Collision_Group_t)pCargo->GetCollisionGroup();
	pCargo->SetCollisionGroup( COLLISION_GROUP_DEBRIS );
	
	// destroy any existing constraint
	if ( m_pDroneRopeConstraint )
	{
		physenv->DestroyConstraint( m_pDroneRopeConstraint );
		m_pDroneRopeConstraint = NULL;
	}

	// constrain the cargo
	{
		IPhysicsObject *pReferenceObject = VPhysicsGetObject();
		IPhysicsObject *pAttachedObject = pCargo->VPhysicsGetObject();

		if ( pReferenceObject && pAttachedObject )
		{
			constraint_lengthparams_t conLength;
			conLength.Defaults();
			conLength.Init( pReferenceObject, pAttachedObject, 35, false );
			conLength.constraint.bodyMassScale[0] = 100.0f; // so the drone is minimally affected by the cargo weight. The drone can get bogged down if it can't lift cargo that's too heavy!
			conLength.constraint.bodyMassScale[1] = 1.0f;
			conLength.objectPosition[1].z += 10;

			m_pDroneRopeConstraint = physenv->CreateLengthConstraint( pReferenceObject, pAttachedObject, NULL, conLength );
		}
	}

}

void CDrone::DetachCargo( bool bDelivered, CBasePlayer *pPlayerGettingCargoOverride )
{
	CBaseEntity *pCargo = m_hDeliveryCargo.Get();
	if ( !pCargo )
		return;

	if ( m_pDroneRopeConstraint )
	{
		physenv->DestroyConstraint( m_pDroneRopeConstraint );
		m_pDroneRopeConstraint = NULL;
	}

	EmitSound( "SolidMetal.ImpactHard" );

	pCargo->SetCollisionGroup( (int)m_tCargoCollisionGroup );

	IPhysicsObject *pPhys = pCargo->VPhysicsGetObject();
	if ( pPhys )
	{
		// dampen velocity on drop

		Vector vecVelocity = vec3_origin;
		AngularImpulse angAngularVelocity;
		pPhys->GetVelocity( &vecVelocity, &angAngularVelocity );

		vecVelocity *= 0.3f;

		pPhys->SetVelocity( &vecVelocity, NULL );

		pPhys->Wake();
	}

	CSurvivalGameRules* pBRrules = CSGameRules()->GetSurvivalRules();
	if ( pBRrules )
	{
		// msg owner
		if ( m_hCargoOwner && m_hCargoOwner->IsAlive() )
		{
			CSingleUserAndReplayRecipientFilter filter( m_hCargoOwner );
			filter.MakeReliable();

			CCSUsrMsg_EntityOutlineHighlight msg;
			msg.set_entidx( pCargo->entindex() );
			SendUserMessage( filter, CS_UM_EntityOutlineHighlight, msg );
		}
	}

	if ( !pPlayerGettingCargoOverride && m_hCargoOwner && m_hCargoOwner->IsAlive() )
		pPlayerGettingCargoOverride = m_hCargoOwner;

	if ( pPlayerGettingCargoOverride )
	{
		IGameEvent *event = gameeventmanager->CreateEvent( "drone_cargo_detached" );
		if ( event )
		{
			event->SetInt( "userid", pPlayerGettingCargoOverride->GetUserID() );
			event->SetInt( "cargo", pCargo->entindex() );
			event->SetBool( "delivered", bDelivered );
			event->SetInt( "priority", 2 );
			gameeventmanager->FireEvent( event );
		}
	}

	m_hDeliveryCargo = INVALID_EHANDLE;
}

void CDrone::ComputeCurrentWaypointAccuracy( void )
{
	// we can be a bit sloppy about reaching waypoints if we're moving fast.
	m_flLastKnownWaypointAccuracy = RemapValClamped( m_flLastKnownSpeed, 0.0f, 50.0f, 10.0f, 64.0f ); // magic numbers!
}

void CDrone::ComputeCurrentVelocityAndSpeed( void )
{
	// find current velocity and speed using physics object
	IPhysicsObject *pPhysicsObject = VPhysicsGetObject();
	if ( !pPhysicsObject )
		return;

	Vector vecVelPrevious = m_vecLastKnownVelocity;

	pPhysicsObject->GetVelocity( &m_vecLastKnownVelocity, &m_vecLastKnownAngImpulse );

	m_flLastKnownSpeed = m_vecLastKnownVelocity.Length();
	m_vecLastKnownAcceleration = m_vecLastKnownVelocity - vecVelPrevious;
}

void CDrone::ComputeDistanceToCurrentDestination( void )
{
	// populate a member var for how far we are from our immediate current destination.

	// FIXME: is the drone truly 2d?

	if ( m_flLastKnownSpeed > 30.0f )
	{
		m_flLastKnownDistanceToDestination = CalcDistanceToLineSegment( m_vecCurrentDestination, GetAbsOrigin(), GetAbsOrigin() - m_vecGroundOffset );
	}
	else
	{
		m_flLastKnownDistanceToDestination = m_vecCurrentDestination.AsVector2D().DistTo( GetAbsOrigin().AsVector2D() );
	}

}

void CDrone::UpdatePositionHistory( void )
{
	// update the record of past positions
	Vector vecDroneOrigin = GetAbsOrigin();

	static const float flDistanceBetweenHistoryPositionsSqr = 20 * 20;
	static const int nMaxNumHistoryPositions = 25;

	if ( !m_vecPositionHistory.Count() || m_vecPositionHistory.Tail().DistToSqr( vecDroneOrigin ) > flDistanceBetweenHistoryPositionsSqr )
	{
		m_vecPositionHistory.AddToTail( vecDroneOrigin );

		if ( m_vecPositionHistory.Count() > nMaxNumHistoryPositions )
		{
			m_vecPositionHistory.RemoveMultipleFromHead( 1 );
		}
	}

	static const float flDistanceBetweenSparseHistoryPositionsSqr = 200 * 200;
	static const int nMaxNumSparseHistoryPositions = 25;

	if ( !m_vecSparsePositionHistory.Count() || m_vecSparsePositionHistory.Tail().DistToSqr( vecDroneOrigin ) > flDistanceBetweenSparseHistoryPositionsSqr )
	{
		m_vecSparsePositionHistory.AddToTail( vecDroneOrigin );

		if ( m_vecSparsePositionHistory.Count() > nMaxNumSparseHistoryPositions )
		{
			m_vecSparsePositionHistory.RemoveMultipleFromHead( 1 );
		}
	}
}

void CDrone::HoverThrust( void )
{
	IPhysicsObject *pPhysicsObject = VPhysicsGetObject();
	if ( !pPhysicsObject )
		return;

	//counteract velocity so the drone floats in the air
	float flDampening = RemapValClamped( m_flLastKnownDistanceToDestination, 0.0f, 500.0f, 0.05f, 0.1f ); // movement is looser closer to target
	Vector vecCounterVel = -m_vecLastKnownVelocity * flDampening;
	AngularImpulse angCounterAng = -m_vecLastKnownAngImpulse * flDampening * 2.0f;
	pPhysicsObject->AddVelocity( &vecCounterVel, &angCounterAng );

	// keep the drone upright, oriented to follow acceleration

	Vector vecUprightTarget = ( (0.01f * m_vecLastKnownVelocity) + Vector( m_vecLastKnownAcceleration.x, m_vecLastKnownAcceleration.y, 10.0f )).Normalized();

	AngularImpulse angKeepUpright = CrossProduct( Up().Normalized(), vecUprightTarget ) * 500.0f;
	pPhysicsObject->ApplyTorqueCenter( angKeepUpright );
}

void CDrone::CollisionAvoidanceThrust( void )
{
	IPhysicsObject *pPhysicsObject = VPhysicsGetObject();
	if ( !pPhysicsObject )
		return;
	
	if ( GetCurrentOrders().m_DroneState == DRONE_STATE_GET_UNSTUCK )
		return;

	Vector vecDroneOrigin = GetAbsOrigin();

	if ( m_hDeliveryCargo.Get() )
		vecDroneOrigin.z -= 40.0f;

	float flLastKnownSpeed = m_vecLastKnownVelocity.AsVector2D().Length();

	// re-evaluate ground height periodically, but not constantly
	float flGroundCheckSampleRate = RemapValClamped( m_flLastKnownSpeed, 5.0f, 40.0f, 0.5f, 0.08f );
	if ( m_GroundCheckTimer.Interval( flGroundCheckSampleRate ) )
	{
		float flTraceScale = RemapValClamped( flLastKnownSpeed, 10.0f, 600.0f, 1.05f, 1.2f );
		Vector vecMin = CollisionProp()->OBBMins() * flTraceScale;
		vecMin.z = -1;
		Vector vecMax = CollisionProp()->OBBMaxs() * flTraceScale;
		vecMax.z = 1;

		Vector vecTraceAhead = m_vecLastKnownVelocity.Normalized() * flLastKnownSpeed;
		vecTraceAhead.z = 0;

		trace_t tr;
		UTIL_TraceHull( vecDroneOrigin, vecDroneOrigin - m_vecGroundOffset + vecTraceAhead, vecMin, vecMax, (MASK_PLAYERSOLID & ~CONTENTS_MONSTER) | CONTENTS_DRONECLIP, this, COLLISION_GROUP_PLAYER_MOVEMENT, &tr );

		trace_t tr2;
		UTIL_TraceHull( vecDroneOrigin, vecDroneOrigin - m_vecGroundOffset, vecMin, vecMax, (MASK_PLAYERSOLID & ~CONTENTS_MONSTER) | CONTENTS_DRONECLIP, this, COLLISION_GROUP_PLAYER_MOVEMENT, &tr2 );

		m_flLastKnownGroundHeight = MAX( tr.endpos.z, tr2.endpos.z );

		// find water height
		trace_t trWater;
		UTIL_TraceLine( vecDroneOrigin, vecDroneOrigin - m_vecGroundOffset, MASK_WATER | CONTENTS_DRONECLIP, this, COLLISION_GROUP_NONE, &trWater );
		if ( trWater.DidHit() )
		{
			m_flLastKnownGroundHeight = MAX( m_flLastKnownGroundHeight, trWater.endpos.z );
		}

		if ( dev_drone_debug.GetBool() )
		{
			debugoverlay->AddLineOverlay( tr.startpos, tr.endpos, 255, 0, 0, 0, (vecMax - vecMin).Length(), 0.1f );
			debugoverlay->AddLineOverlay( tr2.startpos, tr2.endpos, 255, 0, 0, 0, (vecMax - vecMin).Length(), 0.1f );

			Vector vecGround = vecDroneOrigin;
			vecGround.z = m_flLastKnownGroundHeight;
			for ( int i = 0; i < 10; i++ )
			{
				debugoverlay->AddBoxOverlay( vecGround, Vector( -1, -1, 0 ) * i * 10, Vector( 1, 1, 0 ) * i * 10, vec3_angle, 255, 0, 0, 0, 0.1f );
			}
		}
	}

	// avoid poles and trees
	{
		float flTimeSinceLastAvoid = gpGlobals->curtime - m_flAvoidanceTime;
		
		if ( flLastKnownSpeed > 250 && flTimeSinceLastAvoid > 1.0f )
		{
			Vector vecAvoidPoleEnd = m_vecLastKnownVelocity.Normalized() * flLastKnownSpeed;// *1.5f;
			vecAvoidPoleEnd.z = 0;

			trace_t trAvoidPole;
			UTIL_TraceHull( vecDroneOrigin, vecDroneOrigin + vecAvoidPoleEnd, Vector( -30, -30, -20 ), Vector( 30, 30, 20 ), (MASK_PLAYERSOLID & ~CONTENTS_MONSTER) | CONTENTS_DRONECLIP, this, COLLISION_GROUP_PLAYER_MOVEMENT, &trAvoidPole );
			//UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() + vecAvoidPoleEnd, MASK_SOLID, this, COLLISION_GROUP_NONE, &trAvoidPole );

			if ( dev_drone_debug.GetBool() )
			{
				debugoverlay->AddLineOverlay( trAvoidPole.startpos, trAvoidPole.endpos, 255, 0, 255, 0, 40, 0.1f );
				//debugoverlay->AddLineOverlay( trAvoidPole.startpos, trAvoidPole.endpos, 255, 255, 0, true, 0.1f );
			}

			if ( trAvoidPole.DidHit() )
			{
				m_vecAvoidanceDir = RandomInt( 0, 1 ) == 0 ? Left() : -Left();
				m_flAvoidanceTime = gpGlobals->curtime;
			}
		}

		if ( flTimeSinceLastAvoid < 2.0f )
		{
			float flScale = RemapValClamped( flTimeSinceLastAvoid, 0.0f, 2.0f, 0.0f, 1.0f );
			//flScale = pow( 4.0f * flScale * (1.0f - flScale), 2.2f );
			flScale = (4.0 * flScale) * exp( 1.0 - (4.0 * flScale) );

			Vector vecAvoid = m_vecAvoidanceDir * flScale * 30.0f;
			pPhysicsObject->AddVelocity( &vecAvoid, NULL );

			if ( dev_drone_debug.GetBool() )
			{
				debugoverlay->AddLineOverlay( vecDroneOrigin, vecDroneOrigin + vecAvoid, 255, 255, 0, 255, 2.0f, 0.1f );
			}
		}
	}

	// keep the drone off the ground
	float flIdealHeight = m_flLastKnownGroundHeight + m_vecGroundOffset.z;

	if ( vecDroneOrigin.z < flIdealHeight )
	{
		float flLocalHeightOffset = flIdealHeight - vecDroneOrigin.z;
		Vector vecUpVel = Vector( 0, 0, 1 ) * flLocalHeightOffset * 0.75f;
		pPhysicsObject->AddVelocity( &vecUpVel, NULL );
	}
}

void CDrone::MoveThrust( void )
{
	IPhysicsObject *pPhysicsObject = VPhysicsGetObject();
	if ( !pPhysicsObject )
		return;

	Vector vecDroneOrigin = GetAbsOrigin();

	Vector vecToTarget = m_vecCurrentDestination - vecDroneOrigin;

	// apply torque to point nose toward destination
	float flLookForwardStrength = RemapValClamped( m_flLastKnownDistanceToDestination, 0.0f, 500.0f, 2.0f, 1.0f ); // turn speed increases as destination approaches
	AngularImpulse angKeepForward = CrossProduct( Forward().Normalized(), vecToTarget.Normalized() ) * m_flLastKnownSpeed * flLookForwardStrength;
	pPhysicsObject->ApplyTorqueCenter( angKeepForward );

	// apply some combination of local forward velocity and velocity directly toward the destination
	float flLerpBetweenDirectAndTraditionalThrust = RemapValClamped( m_flLastKnownDistanceToDestination, 200.0f, 1200.0f, 0.0f, 0.9f ); // wider turns when far away
	Vector vecFinal = Lerp( flLerpBetweenDirectAndTraditionalThrust, vecToTarget.Normalized(), Forward() );
	vecFinal = vecFinal.Normalized() * MIN( vecToTarget.Length(), m_flMaxSpeed );
	pPhysicsObject->AddVelocity( &vecFinal, NULL );

	if ( dev_drone_debug.GetBool() )
	{
		debugoverlay->AddLineOverlay( vecDroneOrigin, vecDroneOrigin + vecFinal, 0, 255, 0, 255, 1.0f, 0.1f );
	}
	
}

void CDrone::AmbientThrust( void )
{
	IPhysicsObject *pPhysicsObject = VPhysicsGetObject();
	if ( !pPhysicsObject )
		return;

	// add some velocity noise to make the drone move more realistically
	Vector vecRandomVelNoise = RandomVectorInUnitSphere() * 2.0f;
	pPhysicsObject->AddVelocity( &vecRandomVelNoise, NULL );

	// act injured
	float flHealthRatio = (float)GetHealth() / dev_drone_default_health.GetFloat();
	if ( flHealthRatio < 0.5f )
	{
		if ( !m_ActInjuredTimer.HasStarted() || m_ActInjuredTimer.IsElapsed() )
		{
			// pretend to be injured by intermittently cutting power 
			Vector vecDir = Forward();
			g_pEffects->Sparks( GetAbsOrigin(), 1, 1, &vecDir );
			m_flPowerCutUntil = gpGlobals->curtime + RandomFloat( 0.2f, 0.5f );
			m_ActInjuredTimer.Start( RandomFloat( 1.5f, 2.5f ) );

			EmitSound( "Breakable.Spark" );
		}
	}
}

void CDrone::CheckStuck( void )
{
	// TODO: different stuck criteria depending on state?

	// here are some criteria for thinking we're stuck. TODO: make less iffy.

	if ( GetCurrentOrders().m_DroneState == DRONE_STATE_GET_UNSTUCK )
		return; // we're already stuck and trying to get out of this pickle

	bool bFarFromDestination = (m_flLastKnownDistanceToDestination > MAX( m_flLastKnownWaypointAccuracy, 20.0f ));

	IPhysicsObject *pPhysicsObject = VPhysicsGetObject();
	bool bTouchingSomething = pPhysicsObject ? pPhysicsObject->GetContactPoint( NULL, NULL ) : false;

	bool bNotMoving = m_flLastKnownSpeed < 16.0f;

	bool bDestinationIsDirectlyAboveOrBelow = m_vecCurrentDestination.AsVector2D().DistTo( GetAbsOrigin().AsVector2D() ) < m_flLastKnownWaypointAccuracy;


	bool bProbablyCaughtOnBuildingOrTree = (bFarFromDestination && bTouchingSomething && bNotMoving);
	bool bDestinationIsNextToWallOrInHole = (bFarFromDestination && bNotMoving && bDestinationIsDirectlyAboveOrBelow);

	if ( bProbablyCaughtOnBuildingOrTree || bDestinationIsNextToWallOrInHole )
	{
		// backtrack for no more than 7 seconds
		PushNewOrders( droneOrders_t( DRONE_STATE_GET_UNSTUCK, 7 ) );
	}

}

void CDrone::PushNewOrders( droneOrders_t newOrders )
{
	Assert( m_Orders.Count() < 16 ); // this isn't a limit, it's just to catch accidentally pushing too many orders. may just want to remove this check

	if ( m_Orders.Count() && m_Orders.Element( 0 ).m_DroneState == newOrders.m_DroneState )
	{
		// we're already in this state. Adopt the new duration only.
		m_Orders.Element( 0 ).m_flDuration = newOrders.m_flDuration;
		return;
	}

	m_Orders.AddToHead( newOrders );

	if ( !m_bQueuingOrders )
		OnOrdersChanged();
}

inline droneState_t CDrone::GetCurrentOrderState( void )
{
	return GetCurrentOrders().m_DroneState;
}

droneOrders_t CDrone::GetCurrentOrders( void )
{
	if ( m_Orders.Count() )
	{
		return m_Orders.Element( 0 );
	}

	return droneOrders_t(); // defaults to idle of infinite duration
}

void CDrone::PopCompletedOrders( void )
{
	if ( dev_drone_debug.GetBool() )
	{
		debugoverlay->AddTextOverlay( GetAbsOrigin() + Vector(0,0,10), 5.0f, "Ended %s", GetDroneStatePrintName( GetCurrentOrderState() ) );
	}

	if ( m_Orders.Count() )
	{
		m_Orders.RemoveMultipleFromHead( 1 );
	}
	
	OnOrdersChanged();
}

float CDrone::GetTimeSinceLastOrderChange( void )
{
	return gpGlobals->curtime - m_flLastOrdersChangeTimestamp;
}

void CDrone::UpdateIntervalThinkTimer( void )
{
	m_bDoIntervalThink = false;

	if ( m_IntervalThinkTimer.HasStarted() && !m_IntervalThinkTimer.IsElapsed() )
		return;

	m_bDoIntervalThink = true;

	m_IntervalThinkTimer.Start( 1.0f ); // do AI think every second, not constantly
}

void CDrone::LookForTargets( void )
{
	if ( !m_bDoIntervalThink )
		return; // this function only runs sporadically

	// if we aren't allowed to notice players, bail
	if ( ShouldIgnorePlayers() )
		return;

	// identify friend or foe

	// this is our current target. might be null
	CBasePlayer *pBestTargetPlayer = ToBasePlayer( m_hAttackTarget.Get() );

	if ( pBestTargetPlayer )
	{
		trace_t tr;
		UTIL_TraceLine( GetAbsOrigin(), pBestTargetPlayer->GetAbsOrigin() + Vector( 0, 0, 32 ), MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );

		if ( tr.DidHit() && tr.m_pEnt == pBestTargetPlayer )
		{
			if ( tr.startpos.DistTo( tr.endpos ) < 1200 )
			{
				// we can see this player!

				SetAttackTargetEntity( pBestTargetPlayer );
				PushNewOrders( droneOrders_t( DRONE_STATE_INVESTIGATE ) );
				return;
			}
		}
	}
	
	// we either don't have a target or we haven't seen our existing target for a while
	//if ( !pBestTargetPlayer || gpGlobals->curtime - m_flLastTimeSawAttackTarget > 15 )
	//{
	//	CBaseEntity *pEntity = NULL;
	//	while ( (pEntity = gEntList.FindEntityInSphere( pEntity, GetAbsOrigin(), 1000 )) != NULL )
	//	{
	//		if ( pEntity->IsPlayer() && pEntity != m_hDeliveryPlayer.Get() )
	//		{
	//			SetAttackTargetEntity( pEntity );
	//			PushNewOrders( droneOrders_t( DRONE_STATE_INVESTIGATE ) );
	//			return;
	//		}
	//	}
	//}

}

void CDrone::OnOrdersChanged( void )
{
	// runs when the current orders change (and a new order starts)

	m_flLastOrdersChangeTimestamp = gpGlobals->curtime;

	CarryOutOrders( true /*first entering this state*/ );
}

void CDrone::CarryOutOrders( bool bEnteredState /* = false */ )
{
	// runs each think and evaluates the current order

	if ( !bEnteredState )
	{
		// if the current orders had a time limit, check that now
		float flCurrentOrderTimeLimit = GetCurrentOrders().m_flDuration;
		if ( flCurrentOrderTimeLimit >= 0 && GetTimeSinceLastOrderChange() > flCurrentOrderTimeLimit )
		{
			PopCompletedOrders();
			return;
		}
	}
	else
	{

		if ( dev_drone_debug.GetBool() )
		{
			debugoverlay->AddTextOverlay( GetAbsOrigin(), 5.0f, "Started %s", GetDroneStatePrintName( GetCurrentOrderState() ) );
		}

	}
	
	if ( GetCurrentOrderState() == DRONE_STATE_IDLE )
	{
		DoState_Idle( bEnteredState );
	}
	else if( GetCurrentOrderState() == DRONE_STATE_EAT_WAYPOINTS || GetCurrentOrderState() == DRONE_STATE_CYCLE_WAYPOINTS )
	{
		DoState_Waypoints( bEnteredState );
	}	
	else if ( GetCurrentOrderState() == DRONE_STATE_GET_UNSTUCK )
	{
		DoState_Unstuck( bEnteredState );
	}
	else if ( GetCurrentOrderState() == DRONE_STATE_FLY_UP )
	{
		DoState_FlyUp( bEnteredState );
	}
	else if ( GetCurrentOrderState() == DRONE_STATE_WANDER )
	{
		DoState_Wander( bEnteredState );
	}
	else if ( GetCurrentOrderState() == DRONE_STATE_GET_TO_GOOD_CARGO_DROP_POS )
	{
		DoState_GetToGoodCargoDropPos( bEnteredState );
	}
	else if ( GetCurrentOrderState() == DRONE_STATE_INVESTIGATE )
	{
		DoState_Investigate( bEnteredState );
	}
	else if ( GetCurrentOrderState() == DRONE_STATE_ATTACK )
	{
		DoState_Attack( bEnteredState );
	}
	else if ( GetCurrentOrderState() == DRONE_STATE_RETREAT )
	{
		DoState_Retreat( bEnteredState );
	}
	else if ( GetCurrentOrderState() == DRONE_STATE_MOVE_TO_ENTITY )
	{
		DoState_MoveToEntity( bEnteredState );
	}
	else if ( GetCurrentOrderState() == DRONE_STATE_DROP_CARGO )
	{
		DoState_DropCargo( bEnteredState );
	}
	else if ( GetCurrentOrderState() == DRONE_STATE_RETURN_TO_DESPAWN )
	{
		DoState_ReturnToDespawn( bEnteredState );
	}

}

void CDrone::DoState_MoveToEntity( bool bEntered )
{
	if ( bEntered )
	{

	}
	else
	{
		m_flMaxSpeed = RemapValClamped( m_flLastKnownDistanceToDestination, 0.0f, 1500.0f, 20.0f, 70.0f ); // max velocity reduces closer to target

		if ( IsUpgraded() )
		{
			m_flMaxSpeed *= dev_drone_upgrade_speed_multiplier.GetFloat();
		}

		if ( !m_bInPlayArea && m_hDeliveryCargo.Get() )
		{
			// we are on our way to a delivery and are in the danger zone, apply boost
			m_flMaxSpeed *= dev_drone_zone_speed_multiplier.GetFloat();
		}

		bool bMoveToEntIsInPlayArea = true;

		CBaseEntity *pMoveToEnt = m_hMoveToThisEntity.Get();
		if ( pMoveToEnt && m_bDoIntervalThink )
		{
			m_vecLastKnownMoveToEntityPosition = pMoveToEnt->GetAbsOrigin();

			// try to drop the cargo NEXT to a player not ON them
			if ( pMoveToEnt->GetParent() && pMoveToEnt->GetParent()->IsPlayer() )
			{
				CBasePlayer *pPlayer = ToBasePlayer( pMoveToEnt->GetParent() );
				if ( pPlayer && pPlayer->IsAlive() )
				{
					Vector vecEyeForward;
					pPlayer->EyeVectors( &vecEyeForward );
					vecEyeForward.z = 0;
					vecEyeForward.NormalizeInPlace();
					vecEyeForward *= 40.0f;

					m_vecLastKnownMoveToEntityPosition += vecEyeForward;
				}
			}

			{ // bob and weave
				Vector vecRandomOffset = RandomVectorOnUnitSphere();
				vecRandomOffset.z = 0;
				vecRandomOffset.NormalizeInPlaceSafe( vec3_origin );
				vecRandomOffset *= RemapValClamped( m_flLastKnownDistanceToDestination, 10000.0f, 1000.0f, 2000.0f, 0.0f );
				m_vecLastKnownMoveToEntityPosition += vecRandomOffset;
			}

			CDangerZoneController *pZone = GetDangerZoneController();
			if ( pZone /*&& pZone->IsDangerZoneEnabled()*/ )
			{
				bMoveToEntIsInPlayArea = pZone->IsWithinPlayArea( m_vecLastKnownMoveToEntityPosition );

				if ( !pZone->IsWithinPlayArea( m_vecLastKnownMoveToEntityPosition, 1.0f, dev_drone_cargodrop_within_play_area_buffer.GetFloat() ) )
				{
					// the destination won't be in the play area dev_drone_cargodrop_within_play_area_buffer.GetFloat() seconds from now

					// if the zone radius at that time is small, give up, fine to drop it now
					if ( pZone->GetMyDangerZoneRadius( m_vecLastKnownMoveToEntityPosition, dev_drone_cargodrop_within_play_area_buffer.GetFloat() ) > 500 )
					{
						// move the drop destination into the play area by that amount
						m_vecLastKnownMoveToEntityPosition = pZone->MovePointIntoPlayArea( m_vecLastKnownMoveToEntityPosition, 1.0f, dev_drone_cargodrop_within_play_area_buffer.GetFloat() );
					}
				}
			}
		}

		SetCurrentDestination( m_vecLastKnownMoveToEntityPosition );

		if ( m_flLastKnownDistanceToDestination < m_flLastKnownWaypointAccuracy )
		{
			// Players wanted their drone to deliver only if they were standing still. 
			// That way if they were running somewhere, the drone wouldn't drop the cargo behind them.
			// Not sure if supporting this is a good gameplay idea. It might be more exciting to have to
			// stop and open your drone cargo, or choose to leave it behind if you really want to be 
			// somewhere else.

			bool bTargetIsMoving = false;

			// determine the velocity of the target
			Vector vecVelocity = vec3_origin;
			if ( pMoveToEnt )
			{
				vecVelocity = pMoveToEnt->GetSmoothedVelocity();

				if ( pMoveToEnt->GetMoveParent() )
				{
					Vector vecParentVelocity = pMoveToEnt->GetMoveParent()->GetSmoothedVelocity();
					if ( vecParentVelocity.LengthSqr() > vecVelocity.LengthSqr() )
					{
						vecVelocity = vecParentVelocity;
					}
				}
			}

			// the target is moving if its velocity is higher than this
			if ( vecVelocity.Length() > dev_drone_drop_target_speed.GetFloat() )
			{
				bTargetIsMoving = true;
			}

			// keep track of when we arrived, so we can give up and dump the cargo anyway after a timeout period
			if ( m_flTimeArrivedAtMoveToEntity == 0 )
				m_flTimeArrivedAtMoveToEntity = gpGlobals->curtime;

			// time out if we've been waiting for a moving entity to stop
			bool bTimeout = (gpGlobals->curtime - m_flTimeArrivedAtMoveToEntity) > dev_drone_cargodrop_moving_timeout.GetFloat();


			// Drop cargo if the destination is static, or we've been following for too long.
			// If the destination is moving outside the zone, don't time out the follow until they are inside.
			bool bDoCargoDrop = (!bTargetIsMoving) || (bTimeout && bMoveToEntIsInPlayArea);

			if ( bDoCargoDrop )
			{

				if ( !m_hDeliveryCargo.Get() )
				{
					// our cargo is missing? Inform the player about the refund policy.

					if ( pMoveToEnt && pMoveToEnt->GetMoveParent() && pMoveToEnt->GetMoveParent()->IsPlayer() )
					{
						CBasePlayer *pPlayer = ToBasePlayer( pMoveToEnt->GetMoveParent() );
						if ( pPlayer )
						{
							ClientPrint( pPlayer, HUD_PRINTCENTER, "#SFUI_DroneDeliveryStolen" );
						}
					}

				}


				m_flTimeArrivedAtMoveToEntity = 0;

				m_hMoveToThisEntity = INVALID_EHANDLE;

				PopCompletedOrders();
				return;
			}

		}

	}
}

void CDrone::DoState_DropCargo( bool bEntered )
{
	if ( bEntered )
	{
		DetachCargo( true );
	}
	else
	{
		PopCompletedOrders();
		return;
	}
}

void CDrone::DoState_ReturnToDespawn( bool bEntered )
{
	if ( bEntered )
	{

	}
	else
	{

		m_flMaxSpeed = 45.0f;

		SetCurrentDestination( m_vecSpawnPosition );

		LookForTargets();

		if ( m_flLastKnownDistanceToDestination < m_flLastKnownWaypointAccuracy )
		{
			UTIL_Remove( this );
			return;
		}

	}
}

void CDrone::SetUpgraded( bool bUpgraded )
{
	m_bUpgraded = bUpgraded;

	if ( m_bUpgraded )
	{
		SetRenderColor( 35, 185, 255 );
	}
	else
	{
		SetRenderColor( 255, 255, 255 );
	}
}

void CDrone::DoState_Idle( bool bEntered )
{
	if ( bEntered )
	{

	}
	else
	{
		m_flMaxSpeed = RemapValClamped( m_flLastKnownDistanceToDestination, 0.0f, 300.0f, 5.0f, 20.0f ); // max velocity reduces closer to target

		if ( m_bDoIntervalThink )
		{
			trace_t tr;
			UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() - m_vecGroundOffset, MASK_SOLID | CONTENTS_DRONECLIP, this, COLLISION_GROUP_NONE, &tr );

			if ( !tr.DidHit() )
			{
				SetCurrentDestination( tr.endpos );
			}
		}
		else
		{
			SetCurrentDestination( m_vecCurrentDestination );
		}

		LookForTargets();

	}
}

void CDrone::DoState_Waypoints( bool bEntered )
{
	if ( bEntered )
	{

	}
	else
	{
		m_flMaxSpeed = RemapValClamped( m_flLastKnownDistanceToDestination, 0.0f, 1500.0f, 20.0f, 70.0f ); // max velocity reduces closer to target

		if ( m_vecWaypointQueue.Count() )
		{
			SetCurrentDestination( m_vecWaypointQueue.Head() );

			int nLimit = (GetCurrentOrderState() == DRONE_STATE_EAT_WAYPOINTS) ? 0 : 1;

			// have we reached the current waypoint?
			while ( m_vecWaypointQueue.Count() > nLimit && m_flLastKnownDistanceToDestination < m_flLastKnownWaypointAccuracy )
			{
				Vector vecReached = m_vecWaypointQueue.Head();

				m_vecWaypointQueue.RemoveMultipleFromHead( 1 );

				if ( GetCurrentOrderState() == DRONE_STATE_CYCLE_WAYPOINTS )
				{
					AddWayPoint( vecReached );
				}

				if ( m_vecWaypointQueue.Count() )
				{
					// select the next waypoint
					SetCurrentDestination( m_vecWaypointQueue.Head() );
				}

			}

		}
		else
		{
			PopCompletedOrders();
			return;
		}

		LookForTargets();

	}
}

void CDrone::DoState_Unstuck( bool bEntered )
{
	if ( bEntered )
	{
		m_vecUnstuckQueue.RemoveAll();

		if ( m_vecPositionHistory.Count() )
		{

			while ( m_vecPositionHistory.Count() )
			{
				m_vecUnstuckQueue.AddToHead( m_vecPositionHistory.Head() );
				m_vecPositionHistory.RemoveMultipleFromHead( 1 );
			}
		}
		else
		{
			// no position history to get us unstuck!
			PopCompletedOrders();
			PushNewOrders( droneOrders_t( DRONE_STATE_WANDER, 5 ) );
			return;
		}
	}
	else
	{
		m_flMaxSpeed = RemapValClamped( m_flLastKnownDistanceToDestination, 0.0f, 500.0f, 16.0f, 50.0f ); // max velocity reduces closer to target

		if ( m_vecUnstuckQueue.Count() && GetTimeSinceLastOrderChange() < 10 ) // note getting unstuck has a hard limit of 10 seconds
		{
			SetCurrentDestination( m_vecUnstuckQueue.Head() );

			while ( m_vecUnstuckQueue.Count() && m_flLastKnownDistanceToDestination < m_flLastKnownWaypointAccuracy )
			{
				m_vecUnstuckQueue.RemoveMultipleFromHead( 1 );

				if ( m_vecUnstuckQueue.Count() )
				{
					SetCurrentDestination( m_vecUnstuckQueue.Head() );
				}

			}
		}
		else
		{
			PopCompletedOrders();

			// then fly up in the air
			PushNewOrders( droneOrders_t( DRONE_STATE_FLY_UP, 2 ) );
			return;
		}
	}
}

void CDrone::DoState_FlyUp( bool bEntered )
{
	if ( bEntered )
	{
		// pick a random point in the air above to fly to

		trace_t tr;
		UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() + Vector( RandomFloat( -300, 300 ), RandomFloat( -300, 300 ), RandomFloat( 300, 500 ) ), MASK_SOLID | CONTENTS_DRONECLIP, this, COLLISION_GROUP_NONE, &tr );

		SetCurrentDestination( tr.endpos );
	}
	else
	{
		// we picked a destination when the state entered. In case we didn't set a sane limit for this order,
		// enforce a hard-coded limit here.

		m_flMaxSpeed = RemapValClamped( m_flLastKnownDistanceToDestination, 0.0f, 100.0f, 1.0f, 30.0f );

		if ( GetTimeSinceLastOrderChange() > 5 )
		{
			PopCompletedOrders();
			return;
		}
	}
}

void CDrone::DoState_Wander( bool bEntered )
{
	if ( bEntered )
	{
		m_WanderTimer.Reset();
	}
	else
	{
		m_flMaxSpeed = RemapValClamped( m_flLastKnownDistanceToDestination, 0.0f, 500.0f, 5.0f, 20.0f );

		// wander about by picking a new random destination every so often
		if ( !m_WanderTimer.HasStarted() || m_WanderTimer.IsElapsed() )
		{
			m_WanderTimer.Start( RandomFloat( 1.5f, 3.5f ) );

			Vector vecRandom = RandomVectorInUnitSphere();
			vecRandom.z = 0;
			vecRandom.NormalizeInPlace();

			Vector vecNewDestination = GetAbsOrigin() + vecRandom * RandomFloat( 500, 800 );

			SetCurrentDestination( vecNewDestination );
		}

		LookForTargets();
	}
}

void CDrone::DoState_GetToGoodCargoDropPos( bool bEntered )
{
	if ( bEntered )
	{
		// Players hate it when drones drop cargo on nearby rooftops. This tends to happen because the cargo drop destination is next to a wall.
		// In order to prevent this, try to push the drop point out into open space a bit.

		Vector vecAverageOffset = vec3_origin;
		for ( int i = 0; i < ARRAYSIZE( s_vecDroneOffsetDirs ); i++ )
		{
			trace_t trX;
			UTIL_TraceLine( m_vecLastKnownMoveToEntityPosition, m_vecLastKnownMoveToEntityPosition + s_vecDroneOffsetDirs[i] * 255, CONTENTS_SOLID | CONTENTS_DRONECLIP, this, COLLISION_GROUP_DEBRIS, &trX );

			if ( dev_drone_debug.GetBool() )
				debugoverlay->AddLineOverlay( trX.startpos, trX.endpos, 255, 255, 0, true, 4.0f );

			vecAverageOffset += trX.endpos;
		}
		vecAverageOffset *= (1.0f / (float)ARRAYSIZE( s_vecDroneOffsetDirs ));
		vecAverageOffset -= m_vecLastKnownMoveToEntityPosition;

		if ( vecAverageOffset.LengthSqr() )
		{
			if ( dev_drone_debug.GetBool() )
				debugoverlay->AddBoxOverlay( m_vecLastKnownMoveToEntityPosition + vecAverageOffset, Vector( -5, -5, -5 ), Vector( 5, 5, 5 ), QAngle( 0, 0, 0 ), 255, 255, 0, 0, 4.0f );

			m_vecLastKnownMoveToEntityPosition += vecAverageOffset;
		}

	}
	else
	{
		m_flMaxSpeed = RemapValClamped( m_flLastKnownDistanceToDestination, 0.0f, 500.0f, 5.0f, 20.0f );

		SetCurrentDestination( m_vecLastKnownMoveToEntityPosition );

		if ( m_flLastKnownDistanceToDestination < m_flLastKnownWaypointAccuracy )
		{
			PopCompletedOrders();
			return;
		}
	}
}

void CDrone::DoState_Investigate( bool bEntered )
{
	if ( bEntered )
	{

	}
	else
	{
		m_flMaxSpeed = RemapValClamped( m_flLastKnownDistanceToDestination, 0.0f, 500.0f, 10.0f, 40.0f );

		CBasePlayer *pPlayerToAttack = ToBasePlayer( m_hAttackTarget.Get() );
		if ( pPlayerToAttack )
		{

			float flTimeSinceLastSawTarget = gpGlobals->curtime - m_flLastTimeSawAttackTarget;
			if ( flTimeSinceLastSawTarget > 15 )
			{
				PopCompletedOrders();
				return;
			}

			SetCurrentDestination( m_vecLastKnownAttackTargetPosition );

			if ( m_vecLastKnownAttackTargetPosition.AsVector2D().DistTo( GetAbsOrigin().AsVector2D() ) < 80 )
			{
				// we're right over the enemy
				PopCompletedOrders();
				PushNewOrders( droneOrders_t( DRONE_STATE_ATTACK ) );
				return;
			}

			LookForTargets();
		}
		else if ( GetTimeSinceLastOrderChange() > 3 )
		{
			// disconnected player?
			PopCompletedOrders();
			return;
		}

	}
}

void CDrone::DoState_Attack( bool bEntered )
{
	if ( bEntered )
	{

	}
	else
	{
		m_flMaxSpeed = RemapValClamped( m_flLastKnownDistanceToDestination, 0.0f, 500.0f, 10.0f, 40.0f );

		CBasePlayer *pPlayerToAttack = ToBasePlayer( m_hAttackTarget.Get() );
		if ( pPlayerToAttack )
		{

			if ( !pPlayerToAttack->IsAlive() )
			{
				PopCompletedOrders();
				return;
			}
			else
			{
				trace_t tr;
				UTIL_TraceLine( GetAbsOrigin(), pPlayerToAttack->GetAbsOrigin() + Vector( 0, 0, 32 ), MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );

				if ( tr.DidHit() && tr.m_pEnt == pPlayerToAttack )
				{
					m_vecLastKnownAttackTargetPosition = pPlayerToAttack->GetAbsOrigin();
					m_flLastTimeSawAttackTarget = gpGlobals->curtime;

					if ( gpGlobals->curtime - m_flLastDroppedGrenadeAt > 5.0f )
					{
						m_flLastDroppedGrenadeAt = gpGlobals->curtime;

						// drop a grenade on their head
						//const CSchemaItemDefHandle defWeaponIncGrenade( "weapon_incgrenade" );
						//CMolotovProjectile::Create( GetAbsOrigin(), GetAbsAngles(), m_vecLastKnownVelocity, m_vecLastKnownAngImpulse, pPlayerToAttack, defWeaponIncGrenade->GetDefinitionIndex() );

						SetAttackTargetEntity( NULL );

						PopCompletedOrders();

						PushNewOrders( droneOrders_t( DRONE_STATE_RETREAT ) );
						return;
					}

				}
			}

			if ( GetTimeSinceLastOrderChange() > 4 )
			{
				PopCompletedOrders();
				PushNewOrders( droneOrders_t( DRONE_STATE_RETREAT ) );
				return;
			}

			SetCurrentDestination( m_vecLastKnownAttackTargetPosition );

		}
		else if ( GetTimeSinceLastOrderChange() > 3 )
		{
			// disconnected player?
			PopCompletedOrders();
			return;
		}
	}
}

void CDrone::DoState_Retreat( bool bEntered )
{
	if ( bEntered )
	{
		// find the farthest sparse position

		if ( m_vecSparsePositionHistory.Count() )
		{

			Vector vecFarthest = m_vecSparsePositionHistory.Head();
			float flDistSqr = vecFarthest.DistToSqr( GetAbsOrigin() );

			FOR_EACH_VEC( m_vecSparsePositionHistory, n )
			{
				float flContenderDist = m_vecSparsePositionHistory[n].DistToSqr( GetAbsOrigin() );
				if ( flContenderDist > flDistSqr )
				{
					vecFarthest = m_vecSparsePositionHistory[n];
					flDistSqr = flContenderDist;
				}
			}

			SetCurrentDestination( vecFarthest );
		}
	}
	else
	{
		m_flMaxSpeed = RemapValClamped( m_flLastKnownDistanceToDestination, 0.0f, 500.0f, 20.0f, 40.0f );

		SetCurrentDestination( m_vecCurrentDestination );

		if ( m_flLastKnownDistanceToDestination < m_flLastKnownWaypointAccuracy )
		{
			PopCompletedOrders();
			return;

			//if ( m_bCargoDelivered )
			//{
			//	AddWayPoint( m_vecSpawnPosition );
			//	PushNewOrders( droneOrders_t( DRONE_STATE_EAT_WAYPOINTS ) );
			//}
			//else
			//{
			//	// TODO: move this into a dedicated state
			//	m_vecWaypointQueue.RemoveAll();
			//	FOR_EACH_VEC( m_vecSparsePositionHistory, n )
			//	{
			//		m_vecWaypointQueue.AddToHead( m_vecSparsePositionHistory[n] );
			//	}
			//	PushNewOrders( droneOrders_t( DRONE_STATE_EAT_WAYPOINTS ) );
			//}

		}
		
	}
}

void CDrone::SetAttackTargetEntity( CBaseEntity * pEnt )
{
	// hack - don't acquire new targets for a few seconds if we just spawned
	float flTimeSinceSpawn = gpGlobals->curtime - m_flSpawnTimeStamp;
	if ( flTimeSinceSpawn < 5.0f )
		return;

	m_hAttackTarget = pEnt;
	if ( pEnt )
	{
		EmitSound( "Buttons.snd19" );

		m_vecLastKnownAttackTargetPosition = pEnt->GetAbsOrigin();
		m_flLastTimeSawAttackTarget = gpGlobals->curtime;
	}
}

void CDrone::CheckPlayArea( void )
{
	if ( !GetDangerZoneController() /*|| !GetDangerZoneController()->IsDangerZoneEnabled()*/ )
	{
		m_bInPlayArea = true;
		return;
	}

	m_bInPlayArea = GetDangerZoneController()->IsWithinPlayArea( GetAbsOrigin() );

	// if we're outside the play area and not delivering cargo, take damage
	if ( m_bDoIntervalThink ) // once a second
	{
		if ( !m_bInPlayArea && !m_hDeliveryCargo.Get() && GetWorldEntity() )
		{
			CTakeDamageInfo info( GetWorldEntity(), GetWorldEntity(), 10, DMG_GENERIC );
			info.SetDamagePosition( GetAbsOrigin() );
			info.SetDamageForce( vec3_origin );
			TakeDamage( info );
		}
	}
}

void CDrone::StartPropellers( void )
{
	//SetBodygroup( 0, 0 ); // FIXME: why does changing bodygroups break refract materials?
}

void CDrone::StopPropellers( void )
{
	//SetBodygroup( 0, 1 ); // FIXME: why does changing bodygroups break refract materials?
}

void CDrone::AddWayPoint( Vector vecPt )
{
	m_vecWaypointQueue.AddToTail( vecPt );
}

void CDrone::StartDroneSound( void )
{
	StopDroneSound();

	CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
	CReliableBroadcastRecipientFilter filter;
	m_pStateSound = controller.SoundCreate( filter, entindex(), CHAN_STATIC, "drone.engine", ATTN_NORM );
	controller.Play( m_pStateSound, 1.0f, 100 );
	controller.SoundChangeVolume( m_pStateSound, 0.5f, 1.0f );
}

void CDrone::UpdateDroneSound( void )
{
	CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
	if ( m_pStateSound )
	{
		float flTargetPitch = RemapValClamped( m_flLastKnownSpeed, 50.0f, 500.0f, dev_drone_pitch_min.GetFloat(), dev_drone_pitch_max.GetFloat() );

		if ( !IsPowerOn() )
			flTargetPitch = 10.0f;

		controller.SoundChangePitch( m_pStateSound, flTargetPitch, 0.1f );

		if ( GetCurrentOrderState() == DRONE_STATE_INVESTIGATE || GetCurrentOrderState() == DRONE_STATE_ATTACK )
		{
			controller.SoundChangeVolume( m_pStateSound, 1.5f, 1.0f );
		}
		else
		{
			controller.SoundChangeVolume( m_pStateSound, 0.5f, 3.0f );
		}

	}
}

void CDrone::StopDroneSound( void )
{
	CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
	if ( m_pStateSound )
	{
		controller.SoundDestroy( m_pStateSound );
		m_pStateSound = NULL;
	}
}

void CDrone::FireGameEvent( IGameEvent *event )
{
	return; // disabled behavior

	if ( event->GetBool( "silenced" ) )
	{
		return;
	}

	// Heard an unsilenced gunshot

	// todo: retreat and take an alternate route?
	if ( ShouldIgnorePlayers() )
		return;

	CBasePlayer *pPlayer = UTIL_PlayerByUserId( event->GetInt( "userid" ) );
	if ( pPlayer )
	{

		CWeaponCSBase *pWeapon = dynamic_cast< CWeaponCSBase* >(pPlayer->GetActiveWeapon());
		if ( pWeapon && !WeaponIsBallistic( pWeapon->GetWeaponType() ) )
			return; // not a weapon that we care about

		// trace the line for this shot
		Vector vecPlayerEyeForward;
		pPlayer->EyeVectors( &vecPlayerEyeForward );
		trace_t tr;
		UTIL_TraceLine( pPlayer->Weapon_ShootPosition(), pPlayer->Weapon_ShootPosition() + vecPlayerEyeForward * 5000, MASK_SOLID, pPlayer, COLLISION_GROUP_NONE, &tr );

		if ( tr.DidHit() && tr.m_pEnt == this )
		{
			// they're shooting at us!!
			SetAttackTargetEntity( pPlayer );
			PushNewOrders( droneOrders_t( DRONE_STATE_INVESTIGATE ) );
			return;
		}
		else if ( IsSphereIntersectingCone( GetAbsOrigin(), 20.0f, pPlayer->Weapon_ShootPosition(), vecPlayerEyeForward, 0.087f, 0.996f ) )
		{
			// if this shot was aimed at us, then we should still investigate.
			SetAttackTargetEntity( pPlayer );
			PushNewOrders( droneOrders_t( DRONE_STATE_INVESTIGATE ) );
			return;
		}
		//else if ( !m_hAttackTarget.Get() )
		//{
		//	Vector vecToNewTarget = pPlayer->GetAbsOrigin() - GetAbsOrigin();
		//	if ( vecToNewTarget.Length() > 2000 )
		//		return; // eh, too far away
		//
		//	// might as well investigate if we don't yet have an attack target
		//	SetAttackTargetEntity( pPlayer );
		//	PushNewOrders( droneOrders_t( DRONE_STATE_INVESTIGATE ) );
		//	return;
		//}
		//else
		//{
		//	// this shot probably wasn't meant for us
		//
		//	Vector vecToNewTarget = pPlayer->GetAbsOrigin() - GetAbsOrigin();
		//	if ( vecToNewTarget.Length() > 2000 )
		//		return; // eh, too far away
		//
		//	// since we already have a target, only change targets if this player is closer than our current target.
		//	Vector vecToOldTarget = m_hAttackTarget.Get()->GetAbsOrigin() - GetAbsOrigin();
		//
		//	if ( vecToNewTarget.LengthSqr() < vecToOldTarget.LengthSqr() )
		//	{
		//		SetAttackTargetEntity( pPlayer );
		//		PushNewOrders( droneOrders_t( DRONE_STATE_INVESTIGATE ) );
		//		return;
		//	}
		//
		//}

	}

}

void CDrone::VPhysicsDestroyObject( void )
{
	IPhysicsObject *pPhysicsObject = VPhysicsGetObject();
	if ( pPhysicsObject )
		pPhysicsObject->EnableCollisions( false );

	if ( m_pDroneRopeConstraint )
	{
		physenv->DestroyConstraint( m_pDroneRopeConstraint );
		m_pDroneRopeConstraint = NULL;
	}

	BaseClass::VPhysicsDestroyObject();
}

void CDrone::UpdateCargoConstraint( void )
{

	CBaseEntity *pCargo = m_hDeliveryCargo.Get();
	if ( !pCargo || !m_pDroneRopeConstraint )
		return;

	if ( dev_drone_drop_cargo.GetBool() )
	{
		DetachCargo( false );
		return;
	}

	// drop cargo when close no matter what
	if ( pCargo->GetAbsOrigin().DistTo( m_vecLastKnownMoveToEntityPosition ) < 100 )
	{
		DetachCargo( true );
		return;
	}

	IPhysicsObject *pCargoPhysicsObject = pCargo->VPhysicsGetObject();
	IPhysicsObject *pDronePhysicsObject = VPhysicsGetObject();
	if ( pCargoPhysicsObject && pDronePhysicsObject )
	{
		pCargoPhysicsObject->Wake();

		Vector vecCargoAnchorPosIdeal = GetAbsOrigin();
		Vector vecCargoAnchorPosCurrent = pCargo->GetAbsOrigin();

		Vector vecCargoAnchorDelta = vecCargoAnchorPosIdeal - vecCargoAnchorPosCurrent;
		float flDist = vecCargoAnchorDelta.Length();

		if ( flDist > 100 )
		{
			Assert( false ); // the cargo probably got stuck on something and is going to be left behind. Teleport it to the drone
			pCargo->Teleport( &vecCargoAnchorPosIdeal, NULL, &m_vecLastKnownVelocity );
		}
		else
		{
			// get existing values
			Vector vecCargoVelocity;
			AngularImpulse angCargoImpulse;
			pCargoPhysicsObject->GetVelocity( &vecCargoVelocity, &angCargoImpulse );
			
			Vector vecVelocity;
			AngularImpulse angImpulse;
			pDronePhysicsObject->GetVelocity( &vecVelocity, &angImpulse );

			// dampen
			Vector vecDamped = Lerp( 0.03f, vecCargoVelocity, vecVelocity ); // the crate dampens towards the drone velocity, not zero
			vecCargoVelocity.x = vecDamped.x;
			vecCargoVelocity.y = vecDamped.y;

			if ( vecCargoAnchorPosCurrent.z > vecCargoAnchorPosIdeal.z - 16 )
			{
				vecCargoVelocity.z = MIN( vecCargoVelocity.z, vecVelocity.z );
			}

			angCargoImpulse *= 0.95f;
			pCargoPhysicsObject->SetVelocity( &vecCargoVelocity, &angCargoImpulse );

			angCargoImpulse = CrossProduct( Forward().Normalized(), -pCargo->Forward().Normalized() ) * 150.0f;
			pCargoPhysicsObject->ApplyTorqueCenter( angCargoImpulse );
		}

	}
	
}

#endif // !CLIENT_DLL
