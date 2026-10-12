//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#ifndef ENV_GASCANISTER_SHARED_H
#define ENV_GASCANISTER_SHARED_H

#ifdef _WIN32
#pragma once
#endif

#include "vstdlib/random.h"
#include "mathlib/vector.h"
#include "utlvector.h"
#include "networkvar.h"
#if !defined( CLIENT_DLL )
#include "explode.h"
#include "beam_shared.h"
#include "SpriteTrail.h"
//#include "ar2_explosion.h"
#include "SkyCamera.h"
#include "smoke_trail.h"
//#include "ai_basenpc.h"
//#include "npc_gas.h"
//#include "ai_motor.h"
#endif

#ifdef CLIENT_DLL
#define CEnvGasCanister C_EnvGasCanister
#endif

//-----------------------------------------------------------------------------
// Context think
//-----------------------------------------------------------------------------
static const char *s_pOpenThinkContext = "OpenThink";
static const char *s_pGasThinkContext = "GasThink";

//=============================================================================
//
// Shared GasCanister Class
//
class CEnvGasCanister : public CBaseAnimating
{
	DECLARE_CLASS( CEnvGasCanister, CBaseAnimating );

	DECLARE_NETWORKCLASS();
#ifndef CLIENT_DLL
	DECLARE_DATADESC();
#else
	//DECLARE_CLASS( C_EnvGasCanister, C_BaseAnimating );
	//DECLARE_CLIENTCLASS();

public:

	virtual void OnDataChanged( DataUpdateType_t updateType );
	virtual void ClientThink();

	C_EnvGasCanister( const C_EnvGasCanister & );
	~C_EnvGasCanister();

	CNetworkVar( bool, m_bLanded );

	CUtlReference<CNewParticleEffect> m_trailParticleEffect;
	CUtlReference<CNewParticleEffect> m_trailParticleEffectSkybox;
	CUtlReference<CNewParticleEffect> m_impactParticleEffect;
#endif

public: // static accessors for config data
	static float GetGasCanisterFlightTime();

public:
	// Initialization
	CEnvGasCanister();

	// Initialization.
	void InitInWorld( float flLaunchTime, const Vector &vecStartPosition, const QAngle &vecStartAngles, const Vector &vecDirection, const Vector &vecImpactPosition, bool bLaunchedFromWithinWorld = false );
	void InitInSkybox( float flLaunchTime, const Vector &vecStartPosition, const QAngle &vecStartAngles, const Vector &vecDirection, const Vector &vecImpactPosition, const Vector &vecSkyboxOrigin, float flSkyboxScale );
	void InitInSkyboxCopy( void );

	// Returns the position of the object at a given time.
	void GetPositionAtTime( float flTime, Vector &vecPosition, QAngle &vecAngles, bool bIsSkyboxCopy = false );

	// Returns the time at which it enters the world
	float GetEnterWorldTime() const;

	// Convert from skybox to world
	void ConvertFromSkyboxToWorld();

	// Did we impact?
	bool DidImpact( float flTime ) const;

	void	DisableImpactEffects( void ) { m_bDoImpactEffects = false; }

#ifndef CLIENT_DLL
	public:

	virtual void		Precache( void );
	virtual void		Spawn( void );
	virtual void		UpdateOnRemove();

	virtual void		SetTransmit( CCheckTransmitInfo *pInfo, bool bAlways );
	virtual int			UpdateTransmitState() { return SetTransmitState( FL_EDICT_ALWAYS ); }

	private:
	void				InputFireCanister( inputdata_t &inputdata );

	// Think(s)
	void				GasCanisterWorldThink( void );

	// Place the canister in the world
	void				PlaceCanisterInWorld();

	// Check for impacts
	void				TestForCollisionsAgainstEntities( const Vector &vecEndPosition );
	void				TestForCollisionsAgainstWorld( const Vector &vecEndPosition );

	// Blows up!
	void				Detonate( void );
	const char*			GetExplosionParticleSystemName( int pointContents, surfacedata_t *pdata );

	// Landed!
	void				SetLanded( void );
	void				Landed( void );

	// Set up the world model
	void				SetupWorldModel();

	//public:
	//string_t m_iszLaunchPositionName;

	private:
	CNetworkVar( bool, m_bLanded );

	//CNetworkVarEmbedded( CEnvGasCanisterShared, m_Shared );
	//CHandle<CSpriteTrail> m_hTrail;
	//CHandle<SmokeTrail>	m_hSmokeTrail;
	float m_flDamageRadius;
	float m_flDamage;
	bool m_bIncomingSoundStarted;
	bool m_bHasDetonated;
	bool m_bLaunched;

	COutputEHANDLE m_OnLaunched;
	COutputEvent m_OnImpacted;
	COutputEvent m_OnOpened;

	CNetworkHandle( CBaseEntity, m_hSkyboxCopy );
#else
	EHANDLE			m_hSkyboxCopy;
	bool			m_bSpawnedSkyboxParticles;

	float			m_flKillImpactParticlesTime;
#endif

	public:
	// The objects initial parametric conditions.
	CNetworkVector( m_vecImpactPosition );
	CNetworkVector( m_vecStartPosition );
	CNetworkVector( m_vecEnterWorldPosition );
	CNetworkVector( m_vecDirection );
	CNetworkQAngle( m_vecStartAngles );

	CNetworkVar( float, m_flFlightTime );
	CNetworkVar( float, m_flFlightSpeed );
	CNetworkVar( float, m_flLaunchTime );

	CNetworkVar( float, m_flInitialZSpeed );
	CNetworkVar( float, m_flZAcceleration );
	CNetworkVar( float, m_flHorizSpeed );

	CNetworkVar( bool, m_bLaunchedFromWithinWorld );

	CNetworkVector( m_vecParabolaDirection );

	// The time at which the canister enters the skybox
	CNetworkVar( float, m_flWorldEnterTime );

	// Skybox data
	CNetworkVector( m_vecSkyboxOrigin );
	CNetworkVar( float, m_flSkyboxScale );
	CNetworkVar( bool, m_bInSkybox );
	CNetworkVar( bool, m_bDoImpactEffects );
	
	CNetworkVar( int, m_nMyZoneIndex );

	private:
	float	m_flLaunchHeight;
};

#endif // ENV_GAS_CANISTER_SHARED_H
