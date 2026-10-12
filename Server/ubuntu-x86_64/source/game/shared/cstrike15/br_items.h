//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
//=============================================================================//

#ifndef _BR_ITEMS_H_
#define _BR_ITEMS_H_

#ifndef CLIENT_DLL

#include "props.h"

class CCSPlayer;
#include "cs_player_shared.h"

#else
#include "c_props.h"
#endif

DECLARE_AUTO_LIST( IPhysPropRadarJammer );

#ifdef CLIENT_DLL

DECLARE_AUTO_LIST( ITabletRenderedLootCrate );
class C_PhysPropLootCrate : public C_PhysicsPropMultiplayer, public ITabletRenderedLootCrate
{
	DECLARE_CLASS( C_PhysPropLootCrate, C_PhysicsPropMultiplayer );
	DECLARE_CLIENTCLASS();
	IMPLEMENT_AUTO_LIST_GET();

public:

	C_PhysPropLootCrate();

	bool m_bRenderInPSPM;
	bool m_bRenderInTablet;
	
	int GetHealth( void ) const { return m_iHealth; }
	int GetMaxHealth( void ) const { return m_iMaxHealth; }
	int  m_iMaxHealth;

	virtual void OnDataChanged( DataUpdateType_t updateType );

	virtual bool IsWaterStandable( void ) const { return false; }
};

// -------------------
class C_PhysPropAmmoBox : public C_PhysicsPropMultiplayer
{
	DECLARE_CLASS( C_PhysPropAmmoBox, C_PhysicsPropMultiplayer );

	DECLARE_CLIENTCLASS();
};

// -------------------
class C_PhysPropWeaponUpgrade : public C_PhysicsPropMultiplayer
{
	DECLARE_CLASS( C_PhysPropWeaponUpgrade, C_PhysicsPropMultiplayer );

	DECLARE_CLIENTCLASS();
};

// -------------------
class C_PhysPropRadarJammer : public C_PhysicsPropMultiplayer, public IPhysPropRadarJammer
{
	DECLARE_CLASS( C_PhysPropRadarJammer, C_PhysicsPropMultiplayer );

	DECLARE_CLIENTCLASS();
	IMPLEMENT_AUTO_LIST_GET();
};

#endif


#ifndef CLIENT_DLL
class CCSPlayer;

class CBrBaseItem : public CPhysicsPropMultiplayer
{
public:
	DECLARE_CLASS( CBrBaseItem, CPhysicsPropMultiplayer );

	void SetOriginalSource( const char *pszOriginalSource ) { m_strOriginalSource = pszOriginalSource; }

	// Overrides for derived classes
	virtual CConfigurationForHighPriorityUseEntity_t::EPriority_t GetProlongedUsePriority( CCSPlayer *pPlayer ) { return CConfigurationForHighPriorityUseEntity_t::k_EPriority_Default; }
	virtual char const * GetProlongedUsedByOtherMessage( CCSPlayer *pPlayer ) { return "Already being used by other player"; }
	virtual void OnProlongedUseStarted( CCSPlayer *pPlayer ) {}
	virtual void OnProlongedUseSucceeded( CCSPlayer *pPlayer ) {}

	// Internal implementation
	void ProlongedUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void ProlongedUseThink( void );
	
protected:
	CUtlString m_strOriginalSource;
	CEntitySupportForProlongedUse_t m_prolongeduse;
};

// --------------------------------------------------------------
class CPhysPropLootCrate : public CBrBaseItem
{
public:
	DECLARE_CLASS( CPhysPropLootCrate, CBrBaseItem );

	DECLARE_SERVERCLASS();

	CPhysPropLootCrate();
	~CPhysPropLootCrate();

	virtual bool IsWaterStandable( void ) const { return false; }

	virtual void OnEntityEvent( EntityEvent_t event, void *pEventData ) OVERRIDE;

	virtual void Precache();
	virtual void Spawn( void );
	virtual void OnBreak( const Vector &vecVelocity, const AngularImpulse &angVel, CBaseEntity *pBreaker );
	virtual void SpawnCrateItems( CCSPlayer *pBreaker );

	virtual float GetDmgModBullet( void );
	virtual float GetDmgModClub( void );
	virtual float GetDmgModExplosive( void );
	virtual float GetDmgModFire( void );

	virtual int OnTakeDamage( const CTakeDamageInfo &info );

	int ObjectCaps()
	{
		return (BaseClass::ObjectCaps() | FCAP_IMPULSE_USE | FCAP_USE_IN_RADIUS);
	}

	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value ) OVERRIDE;

	virtual bool SetupCrate( const char *pszCrateName );

	const char *GetCrateName() const { return m_pszCrateName; }

	void FormatTypeNameForLogging( CFmtStr &fmt );

	virtual const char *GetTypeName() const { return "wood"; }

	virtual void SetOwnedByPlayer( bool bOwnedByPlayer ) { m_bOwnedByPlayer = bOwnedByPlayer; }
	bool IsOwnedByPlayer() const { return m_bOwnedByPlayer; }
	void SetCrateOwner( CBasePlayer *pPlayer ) { m_hCrateOwner = pPlayer; }
	CBasePlayer *GetCrateOwner() const { return m_hCrateOwner; }

	enum CrateType_t
	{
		WOOD_CRATE = 0,
		METAL_CRATE,
		PARADROP_CRATE,
		MONEY_CRATE,
	};
	virtual CrateType_t GetCrateType() const { return WOOD_CRATE; }

	void GetSpawnPositionForItemNumber( int nItem, int nTotalItemCount, Vector &posOut, QAngle &angOut );

	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_iMaxHealth );
	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_iHealth );

protected:
	CNetworkVar( bool, m_bRenderInPSPM );
	CNetworkVar( bool, m_bRenderInTablet );
	float m_flSpawnTime;
	bool m_bTakeDamageFromDangerZone;

private:

	const char *m_pszCrateName;
	CHandle< CBasePlayer > m_hCrateOwner;
	bool m_bOwnedByPlayer;

	float m_flDampingOriginalSpeed;
	float m_flDampingOriginalRot;
};


class CPhysPropMetalCrate : public CPhysPropLootCrate
{
public:
	DECLARE_CLASS( CPhysPropMetalCrate, CPhysPropLootCrate );

	virtual CrateType_t GetCrateType() const OVERRIDE { return METAL_CRATE; }
	virtual const char *GetTypeName() const OVERRIDE { return "metal"; }
};


class CPhysPropMoneyCrate : public CPhysPropLootCrate
{
public:
	DECLARE_CLASS( CPhysPropMetalCrate, CPhysPropLootCrate );

	CPhysPropMoneyCrate();

	virtual void Spawn( void ) OVERRIDE;
	virtual CrateType_t GetCrateType() const OVERRIDE { return MONEY_CRATE; }
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value ) OVERRIDE;
	virtual void SpawnCrateItems( CCSPlayer *pBreaker ) OVERRIDE;

	virtual int ObjectCaps() OVERRIDE
	{
		return (BaseClass::ObjectCaps() | FCAP_IMPULSE_USE | FCAP_CONTINUOUS_USE | FCAP_USE_IN_RADIUS);
	}

private:
	int m_nCurrentCashCount;
	int m_nCashCount;
	float m_flTimeLastUsed;
};


class CPhysPropParadropCrate : public CPhysPropLootCrate
{
public:
	DECLARE_CLASS( CPhysPropParadropCrate, CPhysPropLootCrate );
	DECLARE_DATADESC();

	CPhysPropParadropCrate();

	virtual void Spawn( void ) OVERRIDE;
	virtual void VPhysicsCollision( int index, gamevcollisionevent_t *pEvent ) OVERRIDE;
	virtual bool SetupCrate( const char *pszCrateName ) OVERRIDE;
	virtual int UpdateTransmitState();

	virtual void SetOwnedByPlayer( bool bOwnedByPlayer );
	virtual void OnBreak( const Vector &vecVelocity, const AngularImpulse &angVel, CBaseEntity *pBreaker );
	virtual int OnTakeDamage( const CTakeDamageInfo &info ) OVERRIDE;

	void CrateThink();
	void BeepThink();

	void RemoveChuteFromParadrop( void );

	virtual CrateType_t GetCrateType() const { return PARADROP_CRATE; }
	virtual const char *GetTypeName() const OVERRIDE { return "paradrop"; }

private:
	bool m_bFalling;
	int m_nNumThinksAtZeroVerticalVelocity;
};


// --------------------------------------------------------------


class CPhysPropAmmoBox : public CBrBaseItem
{
public:
	DECLARE_CLASS( CPhysPropAmmoBox, CBrBaseItem );

	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CPhysPropAmmoBox();
	~CPhysPropAmmoBox();

	virtual void Precache();
	virtual void Spawn( void );

	virtual int OnTakeDamage( const CTakeDamageInfo &info );

	int ObjectCaps()
	{
		return (BaseClass::ObjectCaps() | FCAP_IMPULSE_USE | FCAP_CONTINUOUS_USE | FCAP_USE_IN_RADIUS);
	}

	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value ) OVERRIDE;

private:
	int m_nUsesRemaining;
	float m_flTimeLastUsed;
};


// --------------------------------------------------------------
// base class for Survival upgrade items
class CPhysPropWeaponUpgrade : public CBrBaseItem
{
public:
	DECLARE_CLASS( CPhysPropWeaponUpgrade, CBrBaseItem );

	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CPhysPropWeaponUpgrade();

	virtual void Precache();
	virtual void Spawn( void );

	virtual int OnTakeDamage( const CTakeDamageInfo &info );

	int ObjectCaps()
	{
		return (BaseClass::ObjectCaps() | FCAP_IMPULSE_USE | FCAP_USE_IN_RADIUS);
	}

	void ItemTouch( CBaseEntity *pOther );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value ) OVERRIDE;

	virtual void OnProlongedUseSucceeded( CCSPlayer *pPlayer ) OVERRIDE;

	// upgrade specific
	virtual const char *GetModelPath() const = 0;

protected:
	virtual bool CanUseUpgrade( CCSPlayer *pPlayer, bool bExplainErrorToUser ) { return true; }
	virtual void UseUpgrade( CCSPlayer *pPlayer ) = 0;
	int m_nEventPriority;

private:

	float m_flTimeLastUsed;
};



// --------------------------------------------------------------


class CPhysPropRadarJammer : public CBrBaseItem, public IPhysPropRadarJammer
{
public:
	DECLARE_CLASS( CPhysPropRadarJammer, CBrBaseItem );

	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();
	IMPLEMENT_AUTO_LIST_GET();

	CPhysPropRadarJammer();

	virtual void Precache();
	virtual void Spawn( void );

	virtual int OnTakeDamage( const CTakeDamageInfo &info );
	virtual void OnBreak( const Vector &vecVelocity, const AngularImpulse &angVel, CBaseEntity *pBreaker );
	virtual int ShouldTransmit( const CCheckTransmitInfo *pInfo ) { return FL_EDICT_ALWAYS; }
	virtual int UpdateTransmitState() { return SetTransmitState( FL_EDICT_ALWAYS ); }

	void JammerThink( void );
	void JammerDie( void );

	float m_flSpawnTime;
	float m_flLastSoundTime;
};

#endif

#endif // _BR_ITEMS_H_