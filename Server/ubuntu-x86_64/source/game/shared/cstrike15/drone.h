//========= Copyright © 1996-2012, Valve Corporation, All rights reserved. ============//
//
//=============================================================================//

#ifndef DRONE_H
#define DRONE_H

#ifdef _WIN32
#pragma once
#endif

#ifdef CLIENT_DLL
#include "c_props.h"
#include "c_physicsprop.h"
#else
#include "props.h"
#include "GameEventListener.h"
#endif

#ifdef CLIENT_DLL
#define CDrone C_Drone
#define CPhysicsProp C_PhysicsProp
#endif


#ifndef CLIENT_DLL

class CSoundPatch;

enum droneState_t
{
	DRONE_STATE_IDLE,
	DRONE_STATE_EAT_WAYPOINTS,
	DRONE_STATE_CYCLE_WAYPOINTS,
	DRONE_STATE_GET_UNSTUCK,
	DRONE_STATE_FLY_UP,
	DRONE_STATE_WANDER,
	DRONE_STATE_GET_TO_GOOD_CARGO_DROP_POS,
	DRONE_STATE_INVESTIGATE,
	DRONE_STATE_ATTACK,
	DRONE_STATE_RETREAT,
	DRONE_STATE_MOVE_TO_ENTITY,
	DRONE_STATE_DROP_CARGO,
	DRONE_STATE_RETURN_TO_DESPAWN,
};



struct droneOrders_t
{
	droneState_t	m_DroneState;
	float			m_flDuration;
	
	droneOrders_t( droneState_t droneState = DRONE_STATE_IDLE, float flDuration = -1 )
	{
		m_DroneState = droneState;
		m_flDuration = flDuration;
	}
};

static const Vector s_vecDroneOffsetDirs[18] =
{
	Vector( 1, 0, 0 ),
	Vector( -1, 0, 0 ),

	Vector( 0, 1, 0 ),
	Vector( 0, -1, 0 ),

	Vector( 0, 0, 1 ),
	Vector( 0, 0, -1 ),

	Vector( 1, 1, 0 ),
	Vector( -1, -1, 0 ),
	Vector( 1, -1, 0 ),
	Vector( -1, 1, 0 ),

	Vector( 0, 1, 1 ),
	Vector( 0, -1, -1 ),
	Vector( 0, 1, -1 ),
	Vector( 0, -1, 1 ),

	Vector( 1, 0, 1 ),
	Vector( -1, 0, -1 ),
	Vector( 1, 0, -1 ),
	Vector( -1, 0, 1 ),
};

#endif

DECLARE_AUTO_LIST( IDrone )

class CDrone : public CPhysicsProp, public IDrone
#ifndef CLIENT_DLL
	, public CGameEventListener
#endif
{
public:
	DECLARE_CLASS( CDrone, CPhysicsProp );
	DECLARE_NETWORKCLASS();
	IMPLEMENT_AUTO_LIST_GET();

	CDrone();
	~CDrone();

	virtual void Spawn( void );

#ifndef CLIENT_DLL
	virtual void VPhysicsDestroyObject( void );

	virtual void Precache( void );
	void DroneThink( void );
	const char *GetDroneStatePrintName( droneState_t state );

	virtual int UpdateTransmitState();
	virtual int ShouldTransmit( const CCheckTransmitInfo *pInfo );

	bool IsPowerOn( void )							{ return (m_flPowerCutUntil <= gpGlobals->curtime); }
	void SetCurrentDestination( Vector vecPos );
	
	bool ShouldIgnorePlayers( void );
	void AttachCargo( CBaseEntity* pCargo );
	void DetachCargo( bool bDelivered, CBasePlayer *pPlayerGettingCargoOverride = NULL );

	void SetCargoOwner( CBasePlayer *pPlayer ) { m_hCargoOwner = pPlayer; }

	void ComputeCurrentWaypointAccuracy( void );
	void ComputeCurrentVelocityAndSpeed( void );
	void ComputeDistanceToCurrentDestination( void );
	void UpdatePositionHistory( void );

	void HoverThrust( void );
	void CollisionAvoidanceThrust( void );
	void MoveThrust( void );
	void AmbientThrust( void );

	void CheckStuck( void );

	void StartQueuingOrders( void ) { m_bQueuingOrders = true; }
	void StopQueuingOrders( void ) { m_bQueuingOrders = false; OnOrdersChanged(); }

	void PushNewOrders( droneOrders_t newOrders );
	droneState_t GetCurrentOrderState( void );
	droneOrders_t GetCurrentOrders( void );
	void PopCompletedOrders( void );
	float GetTimeSinceLastOrderChange( void );
	void OnOrdersChanged( void );

	void UpdateIntervalThinkTimer( void );
	void LookForTargets( void );
	void CarryOutOrders( bool bEnteredState = false );
	void SetAttackTargetEntity( CBaseEntity* pEnt );
	void CheckPlayArea( void );

	void StartPropellers( void );
	void StopPropellers( void );

	void AddWayPoint( Vector vecPt );

	void StartDroneSound( void );
	void UpdateDroneSound( void );
	void StopDroneSound( void );

	virtual void FireGameEvent( IGameEvent *event );
	virtual void DroneTouch( CBaseEntity *pOther );
	virtual int OnTakeDamage( const CTakeDamageInfo &info );
	virtual void Event_Killed( const CTakeDamageInfo &info );

	void DebugDraw( void );

	void DoState_Idle			( bool bEntered = false );
	void DoState_Waypoints		( bool bEntered = false );
	void DoState_Unstuck		( bool bEntered = false );
	void DoState_FlyUp			( bool bEntered = false );
	void DoState_Wander			( bool bEntered = false );
	void DoState_GetToGoodCargoDropPos( bool bEntered = false );
	void DoState_Investigate	( bool bEntered = false );
	void DoState_Attack			( bool bEntered = false );
	void DoState_Retreat		( bool bEntered = false );
	void DoState_MoveToEntity	( bool bEntered = false );
	void DoState_DropCargo		( bool bEntered = false );
	void DoState_ReturnToDespawn( bool bEntered = false );

	void SetUpgraded( bool bUpgraded );
	bool IsUpgraded( void ) { return m_bUpgraded; }

	void UpdateCargoConstraint( void );

	void SetLastKnownMoveToEntityPosition( const Vector& vecPos ) { m_vecLastKnownMoveToEntityPosition = vecPos; }
#endif // !CLIENT_DLL

	CBaseEntity *GetMoveToEnt() const { return m_hMoveToThisEntity.Get(); }
	void SetMoveToEnt( CBaseEntity *pEnt ) { m_hMoveToThisEntity.Set( pEnt ); }

#ifdef CLIENT_DLL
	virtual void PostBuildTransformations( CStudioHdr *pStudioHdr, BoneVector *pos, BoneQuaternion q[] ) OVERRIDE;
	virtual void OnDataChanged( DataUpdateType_t updateType );
	CUtlVector<Vector> m_vecClientSideTrailPositions;
#endif

private:
	CNetworkVar( EHANDLE, m_hMoveToThisEntity );
	CNetworkVar( EHANDLE, m_hDeliveryCargo );

#ifdef CLIENT_DLL
	float m_flLastTimeCargoWasAttached;
	Vector m_vecLastKnownCargoAttachPositions[4];
#endif

#ifndef CLIENT_DLL
	CSoundPatch			*m_pStateSound;

	Vector				m_vecGroundOffset;

	CUtlVector<droneOrders_t> m_Orders;
	bool				m_bQueuingOrders;

	float				m_flLastOrdersChangeTimestamp;

	Vector				m_vecLastKnownAcceleration;
	Vector				m_vecLastKnownVelocity;
	AngularImpulse		m_vecLastKnownAngImpulse;
	float				m_flLastKnownSpeed;

	float				m_flMaxSpeed;
	Vector				m_vecCurrentDestination;
	float				m_flLastKnownDistanceToDestination;
	float				m_flLastKnownWaypointAccuracy;
	CUtlVector<Vector>	m_vecWaypointQueue;
	
	CUtlVector<Vector>	m_vecSparsePositionHistory;
	CUtlVector<Vector>	m_vecPositionHistory;
	CUtlVector<Vector>	m_vecUnstuckQueue;

	float				m_flLastKnownGroundHeight;
	CountdownTimer		m_GroundCheckTimer;
	CountdownTimer		m_ActInjuredTimer;
	float				m_flPowerCutUntil;

	CountdownTimer		m_WanderTimer;

	float				m_flLastTimeSawAttackTarget;
	Vector				m_vecLastKnownAttackTargetPosition;
	EHANDLE				m_hAttackTarget;

	CountdownTimer		m_IntervalThinkTimer;
	bool				m_bDoIntervalThink;

	float				m_flLastDroppedGrenadeAt;

	float				m_flSpawnTimeStamp;

	bool				m_bInPlayArea;


	Vector				m_vecSpawnPosition;

	Vector				m_vecLastKnownMoveToEntityPosition;

	Collision_Group_t	m_tCargoCollisionGroup;

	float				m_flTimeArrivedAtMoveToEntity;

	Vector				m_vecAvoidanceDir;
	float				m_flAvoidanceTime;

	bool				m_bUpgraded;

	CHandle< CBasePlayer > m_hCargoOwner;

	IPhysicsConstraint	*m_pDroneRopeConstraint;

#endif

};

#endif // DRONE_H
