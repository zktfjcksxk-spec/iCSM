//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef SURVIVAL_SPAWN_POINT_H
#define SURVIVAL_SPAWN_POINT_H
#ifdef _WIN32
#pragma once
#endif

#include "BasePropDoor.h"

class CCSPlayer;
#include "cs_player_shared.h"

class CPointDZWeaponSpawnGroup;
class CDZDoor;
class CPropCounter;

// stub entity to control weapon or item spawn locations
DECLARE_AUTO_LIST( IPointDZWeaponSpawn )
class CPointDZWeaponSpawn : public CServerOnlyPointEntity, public IPointDZWeaponSpawn
{
	DECLARE_CLASS( CPointDZWeaponSpawn, CServerOnlyPointEntity )
public:
	DECLARE_DATADESC();
	IMPLEMENT_AUTO_LIST_GET();

	CPointDZWeaponSpawn();

	virtual void Spawn() OVERRIDE;

	int GetGroupID() const { return m_nGroupID; }

	void AssignItem( CBaseEntity *pEnt, int nPrice );

	int GetPrice() const { return m_nPrice; }

	bool HasItem() const { return m_hItem != NULL; }
	CBaseEntity *GetItem() const { return m_hItem; }

	void AssignSpawnGroup( CPointDZWeaponSpawnGroup *pGroup ) { Assert( !HasSpawnGroup() ); m_hSpawnGroup = pGroup; }
	const CPointDZWeaponSpawnGroup *GetSpawnGroup() const { return m_hSpawnGroup; }
	bool HasSpawnGroup() const { return m_hSpawnGroup != NULL; }

	float GetCurrentWeight() const { return m_flCurrentWeight; }
	void ApplyWeightScale( float flScale ) { m_flCurrentWeight *= flScale; }
	void Reset();

	string_t GetDoorName() const { return m_iszDoorName; }
	void AssignDoor( CDZDoor *pDoor ) { Assert( m_hDoor == NULL ); m_hDoor = pDoor; }
	CDZDoor *GetDoor() { return m_hDoor; }

	virtual int DrawDebugTextOverlays(void) OVERRIDE;

private:
	int m_nGroupID;

	float m_flDefaultWeight;
	float m_flCurrentWeight;

	CHandle< CPointDZWeaponSpawnGroup > m_hSpawnGroup;

	EHANDLE m_hItem;

	string_t m_iszDoorName;
	CHandle< CDZDoor > m_hDoor;

	int m_nPrice;
};


// groups encapsulated locations
DECLARE_AUTO_LIST( IPointDZWeaponSpawnGroup )
class CPointDZWeaponSpawnGroup : public CServerOnlyPointEntity, public IPointDZWeaponSpawnGroup
{
	DECLARE_CLASS( CPointDZWeaponSpawnGroup, CServerOnlyPointEntity )
public:
	DECLARE_DATADESC();
	IMPLEMENT_AUTO_LIST_GET();

	float GetRadius() const { return m_flRadius; }
	const CUtlVector< CHandle< CPointDZWeaponSpawn > > &GetSpawnPoints() const { return m_vecSpawnPoints; }
	void AddSpawnPoint( CPointDZWeaponSpawn *pPoint );

private:
	float m_flRadius;

	CUtlVector< CHandle< CPointDZWeaponSpawn > > m_vecSpawnPoints;
};

// dronegun spawn
DECLARE_AUTO_LIST( IPointDZDroneGunSpawn )
class CPointDZDroneGunSpawn : public CServerOnlyPointEntity, public IPointDZDroneGunSpawn
{
	DECLARE_CLASS( CPointDZDroneGunSpawn, CServerOnlyPointEntity )
public:
	DECLARE_DATADESC();
	IMPLEMENT_AUTO_LIST_GET();

	CPointDZDroneGunSpawn();

	void SpawnDroneGun( bool bForce = false );
	void SpawnDroneGun( inputdata_t &inputdata ) { SpawnDroneGun( true ); }
private:
	bool m_bSpawnAutomatically;
	EHANDLE m_hSpawnedDroneGun;
};

// parachute spawn
DECLARE_AUTO_LIST( IPointDZParachuteSpawn )
class CPointDZParachuteSpawn : public CServerOnlyPointEntity, public IPointDZParachuteSpawn
{
	DECLARE_CLASS( CPointDZParachuteSpawn, CServerOnlyPointEntity )
public:
	DECLARE_DATADESC();
	IMPLEMENT_AUTO_LIST_GET();
};

// door
DECLARE_AUTO_LIST( IDZDoor )
class CDZDoor : public CPropDoorRotatingBreakable, public IDZDoor
{
public:
	DECLARE_CLASS( CDZDoor, CPropDoorRotatingBreakable );
	DECLARE_DATADESC();
	IMPLEMENT_AUTO_LIST_GET();

	CDZDoor();

	virtual void Precache() OVERRIDE;

	bool IsSecurityDoor() const { return m_bIsSecurityDoor; }

	virtual void Spawn() OVERRIDE;

	CPropCounter *GetPropCounter();

	void SetupDoor();

	void Reset() { m_hSpawnPoint = NULL; }
	void AssignSpawnPoint( CPointDZWeaponSpawn *pSpawnPoint ) { Assert( m_hSpawnPoint == NULL ); m_hSpawnPoint = pSpawnPoint; }
	CPointDZWeaponSpawn *GetSpawnPoint() { return m_hSpawnPoint; }

	bool BIsConfiguredForProlongedUse() const { return m_prolongeduse.BIsConfigured(); }
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value ) OVERRIDE;
	void ProlongedUseThink();

	bool CanOpenSecurityDoor( CCSPlayer *pCSPlayer, bool* pCannotAfford=nullptr );
	void OpenSecurityDoor( CCSPlayer *pCSPlayer );
	
	void SecurityDoorSoundThink();

private:
	bool m_bIsSecurityDoor;

	CHandle< CPointDZWeaponSpawn > m_hSpawnPoint;
	bool m_bPaidToUnlock;
	int m_nPlayDoorOpenSound;

	CEntitySupportForProlongedUse_t m_prolongeduse;

public:
	int m_nAttachmentIndex1;
	int m_nAttachmentIndex2;
};

bool GetClosestItemSpawnPositionTo( const Vector &vecPosition, Vector *vecSpawnPosFound, QAngle *angSpawnAngleFound );

#endif // SURVIVAL_SPAWN_POINT_H
