//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "cbase.h"
#include "weapon_fists.h"
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
	#include "props.h"
#endif

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

#define	FISTS_BODYHIT_VOLUME 128
#define	FISTS_WALLHIT_VOLUME 512

#define FISTS_RANGE_SOFT 68
#define FISTS_RANGE_HARD 78

#define FISTS_DEPLOY_TIME 0.3f

#define FISTS_PUNCH_TIME_SOFT 0.5f
#define FISTS_PUNCH_TIME_HARD 1.34f

#define FISTS_PUNCH_HARD_DELAY 0.71f

#define ACT_VM_HARDPUNCH ACT_VM_RELOAD
#define ACT_VM_NORMALPUNCH ACT_VM_HITCENTER
#define ACT_VM_ROPERAPPEL ACT_VM_FIDGET
#define ACT_VM_LETGOROPE ACT_VM_RELEASE

ConVar sv_fistpunch_damage( "sv_fistpunch_damage", "10", FCVAR_REPLICATED );
ConVar sv_fistpunch_damage_to_player_multiplier( "sv_fistpunch_damage_to_player_multiplier", "1.5", FCVAR_REPLICATED );
ConVar sv_fistpunch_damage_hard( "sv_fistpunch_damage_hard", "20", FCVAR_REPLICATED );
ConVar sv_fistpunch_viewmove( "sv_fistpunch_viewmove", "40", FCVAR_REPLICATED );
ConVar sv_fistpunch_blocked_damage( "sv_fistpunch_blocked_damage", "25", FCVAR_REPLICATED );

ConVar sv_fistpoint_delay( "sv_fistpoint_delay", "1.8", FCVAR_REPLICATED );

ConVar sv_fistpunch_surface_impact_sounds( "sv_fistpunch_impact_sounds", "1", FCVAR_REPLICATED );

// ----------------------------------------------------------------------------- //
// CFists tables.
// ----------------------------------------------------------------------------- //

IMPLEMENT_NETWORKCLASS_ALIASED( Fists, DT_WeaponFists )

BEGIN_NETWORK_TABLE( CFists, DT_WeaponFists )
#ifdef GAME_DLL
SendPropBool( SENDINFO( m_bPlayingUninterruptableAct ) ),
#else
RecvPropBool( RECVINFO( m_bPlayingUninterruptableAct ) ),
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CFists )
END_PREDICTION_DATA()


LINK_ENTITY_TO_CLASS_ALIASED( weapon_fists, Fists );
PRECACHE_REGISTER( weapon_fists );

// ----------------------------------------------------------------------------- //
// CFists implementation.
// ----------------------------------------------------------------------------- //

static const Activity s_vecFistUninterruptableActs[] = { ACT_VM_PICKUP };

CFists::CFists()
{
	m_bPlayingUninterruptableAct = false;

#ifndef CLIENT_DLL
	m_flNextHardPunchHitTime = 0;
	m_bRestorePrevWep = false;
#endif
}

#ifndef CLIENT_DLL
void CFists::Touch( CBaseEntity *pOther )
{
	// don't let disembodied fists fall on the ground. Only valid case is touching a player when we give them the fists weapon.
	if ( !pOther->IsPlayer() )
	{
		UTIL_Remove( this );
	}
	else
	{
		BaseClass::Touch( pOther );
	}
}
#endif

bool CFists::HasPrimaryAmmo()
{
	return true;
}

bool CFists::CanBeSelected()
{
	return true;
}

void CFists::Precache()
{
	BaseClass::Precache();

	PrecacheScriptSound( "Flesh.ImpactSoft" );
	PrecacheScriptSound( "Flesh.ImpactHard" );
	PrecacheScriptSound( "Flesh.BulletImpact" );
	PrecacheScriptSound( "Gloves.Swish" );
}

void CFists::Spawn()
{
	m_iClip1 = -1;
	BaseClass::Spawn();
}

bool CFists::Deploy()
{
	m_flNextPrimaryAttack = gpGlobals->curtime + FISTS_DEPLOY_TIME;
	m_flNextSecondaryAttack = gpGlobals->curtime + FISTS_DEPLOY_TIME;
	SetWeaponIdleTime( gpGlobals->curtime + 2 );
	SendWeaponAnim( ACT_VM_DRAW );

#ifndef CLIENT_DLL
	m_flNextHardPunchHitTime = 0;
	m_bRestorePrevWep = false;
#endif

	return BaseClass::Deploy();
}

bool CFists::IsUninterruptableAct( Activity nAct )
{
	for ( int i = 0; i < ARRAYSIZE( s_vecFistUninterruptableActs ); i++ )
	{
		if ( s_vecFistUninterruptableActs[i] == nAct )
		{
			return true;
		}
	}
	return false;
}

void CFists::PlayUninterruptableActivity( Activity nAct )
{
	if ( !IsUninterruptableAct( nAct ) )
	{
		Assert( false );
		return;
	}

	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

#ifndef CLIENT_DLL
	if ( !m_bPlayingUninterruptableAct && pPlayer->GetActiveWeapon() != this )
	{
		m_hWeaponPrevious = pPlayer->GetActiveWeapon();
		m_hWeaponBeforePrevious = pPlayer->Weapon_GetLast();
		pPlayer->Weapon_Switch( this );
		m_bRestorePrevWep = true;
	}
	m_bPlayingUninterruptableAct = true;

	m_flNextHardPunchHitTime = 0;
#endif

	SendWeaponAnim( nAct );
	
	float flSequenceDuration = SequenceDuration();
	m_flNextPrimaryAttack = gpGlobals->curtime + flSequenceDuration;
	m_flNextSecondaryAttack = gpGlobals->curtime + flSequenceDuration;
	SetWeaponIdleTime( gpGlobals->curtime + flSequenceDuration );
}

bool CFists::CanHolster( void )
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( pPlayer && pPlayer->m_bIsSpawnRappelling )
	{
		return false;
	}

	// fists can refuse to holster if they are playing a special animation, like money pick-up.
	return !m_bPlayingUninterruptableAct;
}

#ifndef CLIENT_DLL
void CFists::FistsAttack( bool bHard )
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	pPlayer->DoAnimationEvent( RandomInt( 0, 1 ) ? PLAYERANIMEVENT_FIRE_GUN_PRIMARY_OPT : PLAYERANIMEVENT_FIRE_GUN_PRIMARY );

	Vector vForward;
	AngleVectors( pPlayer->EyeAngles(), &vForward );

	float flFistRange = (bHard ? FISTS_RANGE_HARD : FISTS_RANGE_SOFT);

	Vector vecSrc = pPlayer->Weapon_ShootPosition();
	Vector vecEnd = vecSrc + vForward * flFistRange;

	float flDamage = (bHard ? sv_fistpunch_damage_hard.GetFloat() : sv_fistpunch_damage.GetFloat());

	trace_t tr;

	// Just for the trace, move other players back to history positions based on local player's lag
	lagcompensation->StartLagCompensation( pPlayer, LAG_COMPENSATE_HITBOXES_ALONG_RAY, pPlayer->EyePosition(), pPlayer->EyeAngles(), flFistRange );
	UTIL_TraceLine( vecSrc, vecEnd, MASK_SOLID, pPlayer, COLLISION_GROUP_NONE, &tr );
	lagcompensation->FinishLagCompensation( pPlayer );

	if ( tr.DidHit() && tr.m_pEnt->IsPlayer() )
	{
		flDamage *= sv_fistpunch_damage_to_player_multiplier.GetFloat();

		CCSPlayer *pPlayerGettingPunched = ToCSPlayer( tr.m_pEnt );
		if ( pPlayerGettingPunched )
		{
			// give them a view punch
			pPlayerGettingPunched->ViewPunch( QAngle( RandomInt( 15, 20 ), RandomInt( 15, 20 ), RandomInt( 5, 10 ) ) );

			// emit a damage sound
			pPlayerGettingPunched->EmitSound( "Player.PunchDamage" );

			// splat them with a decal
			UTIL_ImpactTrace( &tr, DMG_BULLET );

			CWeaponCSBase *pPlayerGettingPunchedWeapon = pPlayerGettingPunched->GetActiveCSWeapon();

			// disarm them!
			if ( bHard && pPlayerGettingPunchedWeapon && !pPlayerGettingPunchedWeapon->IsA( WEAPON_FISTS ) )
			{
				// knock the weapon out of their hands						

				// if we're punching someone who is facing away, assume we're sneakily disarming and throw the weapon at the puncher
				Vector vPlayerGettingPunchedForward;
				AngleVectors( pPlayerGettingPunched->EyeAngles(), &vPlayerGettingPunchedForward );
				if ( DotProduct( vForward, vPlayerGettingPunchedForward ) > 0.45f )
				{
					pPlayerGettingPunched->CSWeaponDrop( pPlayerGettingPunchedWeapon, pPlayer->Weapon_ShootPosition(), false );
				}
				else
				{
					pPlayerGettingPunched->CSWeaponDrop( pPlayerGettingPunchedWeapon, pPlayerGettingPunched->Weapon_ShootPosition() + vForward * 100, false );
				}

				// highlight the stolen weapon
				pPlayerGettingPunchedWeapon->HightlightForPlayer( pPlayer );

				pPlayerGettingPunched->EmitSound("Player.PunchDisarm");

			}

			// move the victim's view angle
			QAngle angEyeAngles = pPlayerGettingPunched->EyeAngles();
			angEyeAngles.y += RandomFloat( -sv_fistpunch_viewmove.GetFloat(), sv_fistpunch_viewmove.GetFloat() );
			pPlayerGettingPunched->SnapEyeAngles( angEyeAngles );

			// give them some damage to make them flinch
			CCSPlayer::StartNewBulletGroup();
			CTakeDamageInfo info;
			info.SetDamage( flDamage );
			info.SetMaxDamage( flDamage );
			info.SetDamageForce( vForward );
			info.SetAttacker( pPlayer );
			info.SetInflictor( pPlayer );
			info.SetDamagePosition( vecSrc );
			info.SetDamageType( DMG_CLUB | DMG_NEVERGIB );
			AddMultiDamage( info, pPlayerGettingPunched );
			ApplyMultiDamage();

			pPlayer->m_nDamageDoneWithPunches += flDamage;

			// play punch hit player sound
			CPASAttenuationFilter filter( pPlayerGettingPunched );
			EmitSound( filter, entindex(), "Flesh.BulletImpact" );

		}
	}
	else if ( tr.DidHit() )
	{
		// ouch - view punch ourselves
		pPlayer->ViewPunch( QAngle( RandomInt( 5, 10 ), RandomInt( 5, 10 ), RandomInt( 5, 10 ) ) );

		// we punched a non-player thing
		CPASAttenuationFilter filter( pPlayer );

		CSoundParameters params;
		if ( GetParametersForSound( "Flesh.ImpactGloves", params, nullptr ) )
		{
			EmitSound_t ep(params);
			ep.m_pOrigin = &tr.endpos;

			EmitSound(filter, entindex(), ep);
		}

		// play a hit sound that matches the surface hit
		if ( sv_fistpunch_surface_impact_sounds.GetBool() )
		{
			const surfacedata_t *psurf = physprops->GetSurfaceData( tr.surface.surfaceProps );

			if ( psurf != nullptr )
			{
				const char* impactName = physprops->GetString( bHard ? psurf->sounds.impactHard : psurf->sounds.impactSoft );

				//When punching we see if there is a 'gloves' sound event for impact with gloves. Otherwise we'll fall back
				//to just the impact soft sound.
				char impactNameGloves[1024];
				V_strcpy(impactNameGloves, impactName);
				V_strcat(impactNameGloves, "gloves", sizeof(impactNameGloves));

				if ( GetParametersForSound( impactNameGloves, params, NULL ) || GetParametersForSound( impactName, params, NULL ) )
				{
					EmitSound_t ep( params );
					ep.m_pOrigin = &tr.endpos;

					EmitSound( filter, entindex(), ep );
				}
			}
		}

		// try and hurt the thing
		CBaseEntity *pEnt = tr.m_pEnt;
		if ( pEnt && pEnt->GetHealth() )
		{
			CTakeDamageInfo info;
			info.SetDamage( flDamage );
			info.SetAttacker( pPlayer );
			info.SetInflictor( pPlayer );
			info.SetDamagePosition( vecSrc );
			info.SetDamageType( DMG_CLUB );
			AddMultiDamage( info, pEnt );
			ApplyMultiDamage();
		}

	}
	else
	{
		// punch hit the air - play wiff sound
		CPASAttenuationFilter filter( pPlayer );
		EmitSound( filter, entindex(), "Gloves.Swish" );
	}
}
#endif

void CFists::FistsPunch( bool bHard )
{
	if ( CSGameRules()->IsFreezePeriod() )
		return;

	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	if ( pPlayer->m_bIsDefusing )
		return;

	if ( pPlayer->GetMoveParent() )
		return; // don't allow punching in the spawn chopper

	if ( pPlayer->m_bIsSpawnRappelling )
		return; // can't punch while rappelling

	if ( MAX( m_flNextPrimaryAttack.Get(), m_flNextSecondaryAttack.Get() ) <= gpGlobals->curtime )
	{

		m_flNextPrimaryAttack = gpGlobals->curtime + (bHard ? FISTS_PUNCH_TIME_HARD : FISTS_PUNCH_TIME_SOFT);
		m_flNextSecondaryAttack = gpGlobals->curtime + (bHard ? FISTS_PUNCH_TIME_HARD : FISTS_PUNCH_TIME_SOFT);

		SetWeaponIdleTime( gpGlobals->curtime + 2 );

		SendWeaponAnim( (bHard ? ACT_VM_HARDPUNCH : ACT_VM_NORMALPUNCH) );

#ifndef CLIENT_DLL
		if ( !bHard )
		{
			FistsAttack( false );
			m_flNextHardPunchHitTime = 0;
		}
		else
		{
			m_flNextHardPunchHitTime = gpGlobals->curtime + FISTS_PUNCH_HARD_DELAY;
		}
#endif

	}
}

void CFists::PrimaryAttack()
{
	FistsPunch( false );
}

void CFists::SecondaryAttack()
{
	FistsPunch( true );
}

bool CFists::SendWeaponAnim( int iActivity )
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( pPlayer )
	{
		MDLCACHE_CRITICAL_SECTION();

		CBaseViewModel *vm = pPlayer->GetViewModel( m_nViewModelIndex );
		if ( vm && vm->GetModelPtr() )
		{
			if ( iActivity == ACT_VM_ROPERAPPEL || iActivity == ACT_VM_LETGOROPE )
			{
				vm->SetBodygroupPreset( "show_rope" );
			}
			else
			{
				vm->SetBodygroupPreset( "hide_rope" );
			}
		}
	}

	return BaseClass::SendWeaponAnim( iActivity );
}

#ifdef CLIENT_DLL
void CFists::WeaponPreRender( void )
{
	BaseClass::WeaponPreRender();

	UpdatePoseParameter();
}
#endif

void CFists::WeaponIdle()
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	Activity nCurrentAct = GetActivity();

	if ( pPlayer->m_bIsSpawnRappelling )
	{
		if ( nCurrentAct != ACT_VM_ROPERAPPEL )
		{
			SendWeaponAnim( ACT_VM_ROPERAPPEL );
		}
		return;
	}

	if ( nCurrentAct == ACT_VM_ROPERAPPEL )
	{
		SendWeaponAnim( ACT_VM_LETGOROPE );
		return;
	}

	if ( nCurrentAct == ACT_VM_LETGOROPE && !IsViewModelSequenceFinished() )
	{
		return;
	}

	if ( m_bPlayingUninterruptableAct )
	{
#ifndef CLIENT_DLL
		if ( m_flNextPrimaryAttack < gpGlobals->curtime )
		{
			m_bPlayingUninterruptableAct = false;

			// switch back to our last weapon and restore lastinv
			if ( m_bRestorePrevWep && m_hWeaponPrevious.Get() && m_hWeaponPrevious.Get() != this )
			{
				pPlayer->Weapon_Switch( m_hWeaponPrevious.Get() );

				if ( m_hWeaponBeforePrevious.Get() )
				{
					pPlayer->Weapon_SetLast( m_hWeaponBeforePrevious.Get() );
				}
			}
			else
			{
				Deploy();
			}
		}
#endif
		return;
	}

	

	if (m_flTimeWeaponIdle > gpGlobals->curtime)
		return;

	if ( IsViewModelSequenceFinished() )
		SendWeaponAnim( ACT_VM_IDLE );
	
	SetWeaponIdleTime( gpGlobals->curtime + SequenceDuration() );
}

#ifndef CLIENT_DLL
void CFists::ItemPostFrame()
{
	if ( m_flNextHardPunchHitTime > 0 && m_flNextHardPunchHitTime < gpGlobals->curtime )
	{
		m_flNextHardPunchHitTime = 0;
		FistsAttack( true );
		return;
	}

	BaseClass::ItemPostFrame();
}
#endif

void CFists::UpdatePoseParameter( void )
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	if ( pPlayer->GetActiveCSWeapon() != this )
		return;

	CBaseViewModel *vm = pPlayer->GetViewModel( m_nViewModelIndex );
	if ( !vm )
		return;

	MDLCACHE_CRITICAL_SECTION();

	if ( pPlayer->m_bIsSpawnRappelling )
	{
		static int nPitchPoseParamIndex = vm->LookupPoseParameter( "pitch" );
		Assert( nPitchPoseParamIndex != -1 );

		float flTarget = RemapValClamped( pPlayer->EyeAngles()[PITCH], -90.0f, 90.0f, -1.0f, 1.0f );
		vm->SetPoseParameter( nPitchPoseParamIndex, flTarget );
	}

	{
		static int nRunningPoseParamIndex = vm->LookupPoseParameter( "running" );
		Assert( nRunningPoseParamIndex != -1 );

		Vector vecVel = pPlayer->GetAbsVelocity();
		float flHorizontalSpeed = vecVel.AsVector2D().Length();
		vm->SetPoseParameter( nRunningPoseParamIndex, RemapValClamped( flHorizontalSpeed, 20.0f, 150.0f, 0.0f, 1.0f ) );
	}
}

bool CFists::CanDrop()
{
	return false;
}
