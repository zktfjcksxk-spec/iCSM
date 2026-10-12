//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================
#include "glow_outline_effect.h"

#ifndef C_PROPS_H
#define C_PROPS_H
#ifdef _WIN32
#pragma once
#endif

#include "c_breakableprop.h"
#include "props_shared.h"
#include "c_physicsprop.h"
#include "c_physbox.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class C_DynamicProp : public C_BreakableProp
{
	DECLARE_CLASS( C_DynamicProp, C_BreakableProp );
public:
	DECLARE_NETWORKCLASS();

	// constructor, destructor
	C_DynamicProp( void );
	~C_DynamicProp( void );

	virtual void	ClientThink( void );
	virtual void OnDataChanged( DataUpdateType_t type );

	void GetRenderBounds( Vector& theMins, Vector& theMaxs );
	unsigned int ComputeClientSideAnimationFlags();
	bool TestBoneFollowers( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr );
	bool TestCollision( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr );

	void ForceTurnOffGlow( void );
	void UpdateGlow( void );
	CGlowObject m_GlowObject;

private:
	C_DynamicProp( const C_DynamicProp & );

	bool	m_bUseHitboxesForRenderBox;
	int		m_iCachedFrameCount;
	Vector	m_vecCachedRenderMins;
	Vector	m_vecCachedRenderMaxs;

	float		m_flGlowMaxDist;
	bool		m_bShouldGlow;
	color32		m_clrGlow;
	int			m_nGlowStyle;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class C_BasePropDoor : public C_DynamicProp
{
	DECLARE_CLASS( C_BasePropDoor, C_DynamicProp );
public:
	DECLARE_CLIENTCLASS();

	// constructor, destructor
	C_BasePropDoor( void );
	virtual ~C_BasePropDoor( void );

	virtual void OnDataChanged( DataUpdateType_t type );
	virtual void PostDataUpdate( DataUpdateType_t updateType );

	virtual bool TestCollision( const Ray_t &ray, unsigned int mask, trace_t& trace );

private:
	C_BasePropDoor( const C_BasePropDoor & );
	bool m_modelChanged;
};


// ------------------------------------------------------------------------------------------ //
// Special version of func_physbox.
// ------------------------------------------------------------------------------------------ //
class C_PhysBoxMultiplayer : public C_PhysBox, public IMultiplayerPhysics
{
public:
	DECLARE_CLASS( C_PhysBoxMultiplayer, C_PhysBox );

	virtual int GetMultiplayerPhysicsMode()
	{
		return m_iPhysicsMode;
	}

	virtual float GetMass()
	{
		return m_fMass;
	}

	virtual bool IsAsleep()
	{
		Assert( 0 );
		return true;
	}

	CNetworkVar( int, m_iPhysicsMode );	// One of the PHYSICS_MULTIPLAYER_ defines.	
	CNetworkVar( float, m_fMass );

	DECLARE_CLIENTCLASS();
};

class C_PhysicsPropMultiplayer : public C_PhysicsProp, public IMultiplayerPhysics
{
	DECLARE_CLASS( C_PhysicsPropMultiplayer, C_PhysicsProp );

	virtual int GetMultiplayerPhysicsMode()
	{
		Assert( m_iPhysicsMode != PHYSICS_MULTIPLAYER_CLIENTSIDE );
		Assert( m_iPhysicsMode != PHYSICS_MULTIPLAYER_AUTODETECT );
		return m_iPhysicsMode;
	}

	virtual float GetMass()
	{
		return m_fMass;
	}

	virtual bool IsAsleep()
	{
		return !m_bAwake;
	}

	virtual void ComputeWorldSpaceSurroundingBox( Vector *mins, Vector *maxs )
	{
		Assert( mins != NULL && maxs != NULL );
		if ( !mins || !maxs )
			return;

		// Take our saved collision bounds, and transform into world space
		TransformAABB( EntityToWorldTransform(), m_collisionMins, m_collisionMaxs, *mins, *maxs );
	}

	CNetworkVar( int, m_iPhysicsMode );	// One of the PHYSICS_MULTIPLAYER_ defines.	
	CNetworkVar( float, m_fMass );
	CNetworkVector( m_collisionMins );
	CNetworkVector( m_collisionMaxs );

	DECLARE_CLIENTCLASS();
};


#endif // C_PROPS_H
