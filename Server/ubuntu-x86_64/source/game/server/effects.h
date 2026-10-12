//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef EFFECTS_H
#define EFFECTS_H

#ifdef _WIN32
#pragma once
#endif


class CBaseEntity;
class Vector;


//-----------------------------------------------------------------------------
// The rotor wash shooter. It emits gibs when pushed by a rotor wash
//-----------------------------------------------------------------------------
abstract_class IRotorWashShooter
{
public:
	virtual CBaseEntity *DoWashPush( float flWashStartTime, const Vector &vecForce ) = 0;
};


//-----------------------------------------------------------------------------
// Gets at the interface if the entity supports it
//-----------------------------------------------------------------------------
IRotorWashShooter *GetRotorWashShooter( CBaseEntity *pEntity );

class CEnvQuadraticBeam : public CPointEntity
{
	DECLARE_CLASS( CEnvQuadraticBeam, CPointEntity );

public:
	void Spawn();
	void SetSpline( const Vector &control, const Vector &target )
	{
		m_targetPosition = target;
		m_controlPosition = control;
	}
	void SetScrollRate( float rate )
	{
		m_scrollRate = rate;
	}

	void SetWidth( float width )
	{
		m_flWidth = width;
	}

private:
	CNetworkVector( m_targetPosition );
	CNetworkVector( m_controlPosition );
	CNetworkVar( float, m_scrollRate );
	CNetworkVar( float, m_flWidth );

	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();
};
CEnvQuadraticBeam *CreateQuadraticBeam( const char *pSpriteName, const Vector &start, const Vector &control, const Vector &end, float width, CBaseEntity *pOwner );

class CEnvGunfire : public CPointEntity
{
public:
	DECLARE_CLASS( CEnvGunfire, CPointEntity );

	CEnvGunfire()
	{
		// !!!HACKHACK
		// These fields came along kind of late, so they get
		// initialized in the constructor for now. (sjb)
		m_flBias = 1.0f;
		m_bCollide = false;

		m_bAllowNullTarget = false;
		m_bAlwaysWallbangTracer = false;
		m_flDamageScaleValue = 1.0f;
		m_flAdditionalSpread = 0.0f;

	}

	void Precache();
	void Spawn();
	void Activate();
	void StartShooting();
	void StopShooting();
	void ShootThink();
	void UpdateTarget();
	void FireBullet(
		Vector vecSrc,	// shooting postion
		const QAngle &shootAngles,  //shooting angle
		float flDistance, // max distance 
		float flPenetration, // the power of the penetration
		int nPenetrationCount,
		int iBulletType, // ammo type
		int iDamage, // base damage
		float flRangeModifier, // damage range modifier
		CBaseEntity *pevAttacker, // shooter
		bool bDoEffects,
		float xSpread, float ySpread,
		const char *pszTracerName
	);

	void InputEnable( inputdata_t &inputdata );
	void InputDisable( inputdata_t &inputdata );

	int	m_iMinBurstSize;
	int m_iMaxBurstSize;

	float m_flMinBurstDelay;
	float m_flMaxBurstDelay;

	float m_flRateOfFire;

	string_t	m_iszShootSound;
	string_t	m_iszTracerType;
	string_t	m_iszWeaponName;

	bool m_bDisabled;

	int	m_iShotsRemaining;

	int		m_iSpread;
	Vector	m_vecSpread;
	Vector	m_vecTargetPosition;
	float	m_flTargetDist;

	float	m_flBias;
	bool	m_bCollide;

	EHANDLE m_hTarget;

	bool m_bAllowNullTarget;
	bool m_bAlwaysWallbangTracer;
	float m_flDamageScaleValue;
	float m_flAdditionalSpread;

	DECLARE_DATADESC();
};

#endif // EFFECTS_H
