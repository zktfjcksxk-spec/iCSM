//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "cbase.h"
#include "weapon_melee.h"
#include "cs_gamerules.h"
#include "datacache/imdlcache.h"

#if defined( CLIENT_DLL )
	#include "c_cs_player.h"
	#include "c_te_effect_dispatch.h"
	#include "c_rumble.h"
	#include "rumble_shared.h"
#else
	#include "cs_player.h"
	#include "ilagcompensationmanager.h"
	#include "te_effect_dispatch.h"
#endif

#include "in_buttons.h"

#include "weapon_knife.h"
#include "collisionutils.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

#define DZ_MELEE_RANGE 48

IMPLEMENT_NETWORKCLASS_ALIASED( Melee, DT_WeaponMelee )

#ifdef GAME_DLL
BEGIN_DATADESC( CMelee )
END_DATADESC()
#endif // GAME_DLL

BEGIN_NETWORK_TABLE( CMelee, DT_WeaponMelee )
#ifdef GAME_DLL
	SendPropFloat( SENDINFO( m_flThrowAt ) ),
#else
	RecvPropFloat( RECVINFO( m_flThrowAt ) ),
#endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CMelee )
	DEFINE_PRED_FIELD( m_flThrowAt, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
END_PREDICTION_DATA()
#endif // CLIENT_DLL


LINK_ENTITY_TO_CLASS_ALIASED( weapon_melee, Melee );
PRECACHE_REGISTER( weapon_melee );

CMelee::CMelee()
{
#ifdef GAME_DLL
	m_swingLeft = true;
#endif
	m_flThrowAt = 0;
}

void CMelee::Precache()
{
	BaseClass::Precache();
	PrecacheScriptSound( "Weapon_Knife.Deploy" );
	PrecacheScriptSound( "Weapon_Knife.Slash" );
	PrecacheScriptSound( "Weapon_Knife.Stab" );
	PrecacheScriptSound( "Weapon_Knife.Hit" );
	PrecacheEffect( "KnifeSlash" );
}

void CMelee::Spawn()
{
	m_iClip1 = -1;
	BaseClass::Spawn();
}

bool CMelee::Deploy()
{
	m_flThrowAt = 0;
	return BaseClass::Deploy();
}

extern void FindHullIntersection( const Vector &vecSrc, trace_t &tr, const Vector &mins, const Vector &maxs, CBaseEntity *pEntity );

void CMelee::PrimaryAttack()
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;
	
#if !defined (CLIENT_DLL)
	// Move other players back to history positions based on local player's lag
	lagcompensation->StartLagCompensation( pPlayer, LAG_COMPENSATE_HITBOXES_ALONG_RAY, pPlayer->EyePosition(), pPlayer->EyeAngles(), DZ_MELEE_RANGE );
#endif

	Vector vForward; AngleVectors( pPlayer->EyeAngles(), &vForward );
	Vector vecSrc = pPlayer->Weapon_ShootPosition();
	Vector vecEnd = vecSrc + vForward * DZ_MELEE_RANGE;

	CKnife::PushMeleeAttackVectorEndOutsidePlayerAABB( pPlayer, vecSrc, vecEnd, vForward, DZ_MELEE_RANGE, 128.0f );

	trace_t tr;
	UTIL_TraceLine( vecSrc, vecEnd, MASK_SOLID, pPlayer, COLLISION_GROUP_NONE, &tr );

	if ( tr.fraction >= 1.0 )
	{
		Vector head_hull_mins( -16, -16, -18 );
		Vector head_hull_maxs( 16, 16, 18 );

		UTIL_TraceHull( vecSrc, vecEnd, head_hull_mins, head_hull_maxs, MASK_SOLID, pPlayer, COLLISION_GROUP_NONE, &tr );
		if ( tr.fraction < 1.0 )
		{
			// Calculate the point of intersection of the line (or hull) and the object we hit
			// This is and approximation of the "best" intersection
			CBaseEntity *pHit = tr.m_pEnt;
			if ( !pHit || pHit->IsBSPModel() )
				FindHullIntersection( vecSrc, tr, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX, pPlayer );
			vecEnd = tr.endpos;	// This is the point on the actual surface (the hull could have hit space)
	}
}

	bool bDidHit = tr.fraction < 1.0f;

#ifndef CLIENT_DLL
	bool bFirstSwing = (m_flNextPrimaryAttack + 0.4) < gpGlobals->curtime;
	if ( bFirstSwing )
	{
		m_swingLeft = true;
	}
#endif

	float fPrimDelay = bDidHit ? 0.5f : 0.4f;
	float fSecDelay = bDidHit ? 0.5f : 0.5f;

	m_flNextPrimaryAttack = gpGlobals->curtime + fPrimDelay;
	m_flNextSecondaryAttack = gpGlobals->curtime + fSecDelay;

	SetWeaponIdleTime( gpGlobals->curtime + 2 );

	bool bBackStab = false;

	if ( bDidHit )
	{
		// server side damage calculations
		CBaseEntity *pEntity = tr.m_pEnt;

#ifndef CLIENT_DLL
		// player "shoot" animation
		pPlayer->SetAnimation( PLAYER_ATTACK1 );

		ClearMultiDamage();

		float flDamage = 0.f;	// set below
#endif
		if ( pEntity && pEntity->IsPlayer() )
		{
			Vector vTragetForward;

			AngleVectors( pEntity->GetAbsAngles(), &vTragetForward );

			Vector2D vecLOS = (pEntity->GetAbsOrigin() - pPlayer->GetAbsOrigin()).AsVector2D();
			Vector2DNormalize( vecLOS );

			float flDot = vecLOS.Dot( vTragetForward.AsVector2D() );

			//more damage if we are stabbing them in the back.
			if ( flDot > 0.475f )
				bBackStab = true;
		}

#ifndef CLIENT_DLL
		if ( bBackStab )
		{
			flDamage = GetDamage() * 2;
		}
		else if ( bFirstSwing )
		{
			flDamage = GetDamage() * 1.2f;
		}
		else
		{
			flDamage = GetDamage();
		}
#endif

		SendWeaponAnim( ACT_VM_HITCENTER );

#ifndef CLIENT_DLL
		CCSPlayer::StartNewBulletGroup();
		CTakeDamageInfo info( pPlayer, pPlayer, this, flDamage, DMG_SLASH | DMG_NEVERGIB );

		CalculateMeleeDamageForce( &info, vForward, tr.endpos, 1.0f / flDamage );
		pEntity->DispatchTraceAttack( info, vForward, &tr );
		ApplyMultiDamage();
#endif

		if ( tr.m_pEnt )
		{
			CPASAttenuationFilter filter( this );
			filter.UsePredictionRules();

			if ( tr.m_pEnt->IsPlayer() )
			{
				EmitSound( filter, entindex(), "Weapon_Knife.Stab" );
			}
			else
			{
				EmitSound( filter, entindex(), "Weapon_Knife.HitWall" );
			}
		}

		CEffectData data;
		data.m_vOrigin = tr.endpos;
		data.m_vStart = tr.startpos;
		data.m_nSurfaceProp = tr.surface.surfaceProps;
		data.m_nDamageType = DMG_SLASH;
		data.m_nHitBox = tr.hitbox;
#ifdef CLIENT_DLL
		data.m_hEntity = tr.m_pEnt->GetRefEHandle();
#else
		data.m_nEntIndex = tr.m_pEnt->entindex();
#endif

		CPASFilter filter( data.m_vOrigin );

#ifndef CLIENT_DLL
		filter.RemoveRecipient( pPlayer );
#endif

		data.m_vAngles = pPlayer->GetAbsAngles();
		data.m_fFlags = 0x1;	//IMPACT_NODECAL;
		DispatchEffect( filter, 0.0, "KnifeSlash", data );
	}
	else
	{
		// play whiff or swish sound
		CPASAttenuationFilter filter( this );
		filter.UsePredictionRules();
		EmitSound( filter, entindex(), "Weapon_Knife.Slash" );

		SendWeaponAnim( ACT_VM_MISSCENTER );
	}

#ifndef CLIENT_DLL
	{
		// See if we are back stabbing and if we're swinging left (opt) or right.
		pPlayer->DoAnimationEvent( bBackStab ? (m_swingLeft ? PLAYERANIMEVENT_FIRE_GUN_PRIMARY_OPT_SPECIAL1 : PLAYERANIMEVENT_FIRE_GUN_PRIMARY_SPECIAL1) : (m_swingLeft ? PLAYERANIMEVENT_FIRE_GUN_PRIMARY_OPT : PLAYERANIMEVENT_FIRE_GUN_PRIMARY) );
		m_swingLeft = !m_swingLeft;
	}
#else
	RumbleEffect( XBX_GetUserId( pPlayer->GetSplitScreenPlayerSlot() ), !bDidHit ? RUMBLE_CROWBAR_SWING : RUMBLE_AR2, 0, RUMBLE_FLAG_RESTART );
#endif



#if !defined (CLIENT_DLL)
	lagcompensation->FinishLagCompensation( pPlayer );
#endif
	
}

void CMelee::UpdateShieldState()
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	if ( !(pPlayer->m_nButtons & IN_ATTACK2) && m_flThrowAt > 0 ) // pending throw, not holding secondary fire
	{
		if ( m_flThrowAt > gpGlobals->curtime ) // not wound up yet
		{
			// cancel throw
			m_flThrowAt = 0;
			SetContextThink( NULL, 0, "CMelee::ThrowThink" );
			SendWeaponAnim( ACT_VM_IDLE );
		}
		else
		{
			SendWeaponAnim( ACT_VM_RELEASE );
#ifndef CLIENT_DLL
			ThrowWeapon();
#endif
		}
	}
}

void CMelee::SecondaryAttack()
{
	if ( m_flThrowAt == 0 )
	{
		SendWeaponAnim( ACT_VM_SWINGHARD );
		m_flThrowAt = gpGlobals->curtime + 0.2f;
	}

	m_flNextPrimaryAttack = gpGlobals->curtime + 0.4f;
	m_flNextSecondaryAttack = gpGlobals->curtime + 0.4f;
	SetWeaponIdleTime( gpGlobals->curtime + 1 );
}

#ifndef CLIENT_DLL
void CMelee::ThrowWeapon()
{
	if ( m_flThrowAt <= 0 )
	{
		Assert( false );
		return;
	}

	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( pPlayer )
	{
		m_hThrower = pPlayer;

		CPASAttenuationFilter filter( this );
		filter.UsePredictionRules();
		EmitSound( filter, entindex(), "Weapon_Knife.Slash" );

		Vector vThrowEmitPos = pPlayer->WorldSpaceCenter() + Vector( 0, 0, 24 );

		Vector vForward;
		Vector vRight;
		AngleVectors( pPlayer->EyeAngles(), &vForward, &vRight, NULL );

		vThrowEmitPos += vRight * 5;

		pPlayer->CSWeaponDrop( this, vThrowEmitPos + vForward * 100, false );

		IPhysicsObject *pWeaponPhys = VPhysicsGetObject();
		if ( pWeaponPhys )
		{
			SetAbsOrigin( vThrowEmitPos );

			Vector vPos;
			QAngle vAngles;

			pWeaponPhys->GetPosition( &vPos, &vAngles );
			pWeaponPhys->SetPosition( vThrowEmitPos, vAngles, true );

			Vector vecVel = ( pPlayer->GetAbsVelocity() * 0.5f ) + ( vForward * 1000 );
			vecVel.z += 100;
			AngularImpulse angImp( 0, 300, 0 );

			pWeaponPhys->SetVelocity( &vecVel, &angImp );
		}

		m_flThrowAt = 0;
	}
}

ConVar mp_weapon_melee_touch_time_after_hit( "mp_weapon_melee_touch_time_after_hit", "5.0", FCVAR_CHEAT | FCVAR_RELEASE );
bool CMelee::DidThrowHitPlayer( CCSPlayer* pHitPlayer )
{
	if ( !pHitPlayer )
		return false;

	Vector vel;
	AngularImpulse angVel;
	GetVelocity( &vel, &angVel );

	if ( vel.Length() < 400 )
		return false;

	// play punch hit player sound
	CPASAttenuationFilter filter( pHitPlayer );
	EmitSound( filter, entindex(), "Flesh.BulletImpact" );

	pHitPlayer->EmitSound( "Player.HitByThrownWeapon" );

	CBaseEntity *pAttacker = m_hThrower.Get();
	if ( !pAttacker )
	{
		Assert( false );
		pAttacker = pHitPlayer;
	}

	CCSPlayer::StartNewBulletGroup();
	CTakeDamageInfo info;
	info.SetDamage( 60 );
	info.SetMaxDamage( 60 );
	info.SetDamageForce( vel );
	info.SetAttacker( pAttacker );
	info.SetInflictor( this );
	info.SetDamagePosition( GetAbsOrigin() );
	info.SetDamageType( DMG_CLUB | DMG_NEVERGIB );
	AddMultiDamage( info, pHitPlayer );
	ApplyMultiDamage();


	IPhysicsObject *pWeaponPhys = VPhysicsGetObject();
	if ( pWeaponPhys )
	{
		Vector vPos;
		QAngle vAngles;

		Vector vecVel = vec3_origin;
		AngularImpulse angImp( 0, 0, 0 );

		pWeaponPhys->SetVelocity( &vecVel, &angImp );
	}

	// Make sure that the weapon will not be automatically picked up via touch by the victim for a little bit
	SetNextOwnerTouchTime( gpGlobals->curtime + mp_weapon_melee_touch_time_after_hit.GetFloat(), true );

	return true;
}
#endif
