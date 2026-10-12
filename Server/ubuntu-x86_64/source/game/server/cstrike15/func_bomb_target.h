//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Bomb Target Area ent
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "triggers.h"
#include "cvisibilitymonitor.h"
//#include "cs_player_resource.h"

class CBombTargetShim : public CBaseTrigger
{
public:
	void Touch( CBaseEntity *pOther ) { return BombTargetTouch( pOther ); }
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value ) { return BombTargetUse( pActivator, pCaller, useType, value ); }

	virtual void BombTargetTouch( CBaseEntity* pOther ) = 0;
	virtual void BombTargetUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value ) = 0;
};

DECLARE_AUTO_LIST( IBombTarget );
class CBombTarget : public CBombTargetShim, public IBombTarget
{
public:
	DECLARE_CLASS( CBombTarget, CBaseTrigger );
	DECLARE_DATADESC();

	IMPLEMENT_AUTO_LIST_GET();

	CBombTarget();

	void Spawn();
	virtual void ReInitOnRoundStart( void );
	void EXPORT BombTargetTouch( CBaseEntity* pOther );
	void EXPORT BombTargetUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	void OnBombExplode( inputdata_t &inputdata );
	void OnBombPlanted( inputdata_t &inputdata );
	void OnBombDefused( inputdata_t &inputdata );

	bool	IsHeistBombTarget( void ) { return m_bIsHeistBombTarget; }
	const char *GetBombMountTarget( void ){ return STRING( m_szMountTarget ); }

	bool IsBombSiteA() const { return !m_bIsBombSiteB; }
	bool IsBombSiteB() const { return m_bIsBombSiteB; }


private:
	COutputEvent m_OnBombExplode;	//Fired when the bomb explodes
	COutputEvent m_OnBombPlanted;	//Fired when the bomb is planted
	COutputEvent m_OnBombDefused;	//Fired when the bomb is defused

	bool		m_bIsBombSiteB; //if true, is bomb site B. Otherwise is bomb site A.

	bool		m_bIsHeistBombTarget;
	bool		m_bBombPlantedHere;
	string_t	m_szMountTarget;
	EHANDLE		m_hInstructorHint;		// Hint that's used by the instructor system
};

//-----------------------------------------------------------------------------
// Purpose: A generic target entity that gets replicated to the client for displaying a hint for the CS bomb targets
//-----------------------------------------------------------------------------
class CInfoInstructorHintBombTargetA : public CPointEntity
{
public:
	DECLARE_CLASS( CInfoInstructorHintBombTargetA, CPointEntity );

	void Spawn( void );
	virtual int UpdateTransmitState( void )	// set transmit filter to transmit always
	{
		return SetTransmitState( FL_EDICT_ALWAYS );
	}

	DECLARE_DATADESC();
};

//-----------------------------------------------------------------------------
// Purpose: A generic target entity that gets replicated to the client for displaying a hint for the CS bomb targets
//-----------------------------------------------------------------------------
class CInfoInstructorHintBombTargetB : public CPointEntity
{
public:
	DECLARE_CLASS( CInfoInstructorHintBombTargetB, CPointEntity );

	void Spawn( void );
	virtual int UpdateTransmitState( void )	// set transmit filter to transmit always
	{
		return SetTransmitState( FL_EDICT_ALWAYS );
	}

	DECLARE_DATADESC();
};