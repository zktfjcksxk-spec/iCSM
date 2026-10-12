//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "cbase.h"
#include "weapon_breachcharge.h"
#include "cs_gamerules.h"
#include "datacache/imdlcache.h"
#include "dangerzone_controller.h"

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
	#include "world.h"
	#include "econ/econ_entity_creation.h"
	#include "takedamageinfo.h"
	#include "survival_spawn_point.h"
#endif

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

ConVar sv_breachcharge_distance_min( "sv_breachcharge_distance_min", "600", FCVAR_RELEASE | FCVAR_GAMEDLL );
ConVar sv_breachcharge_distance_max( "sv_breachcharge_distance_max", "1200", FCVAR_RELEASE | FCVAR_GAMEDLL );

ConVar sv_breachcharge_delay_min( "sv_breachcharge_delay_min", "0", FCVAR_RELEASE | FCVAR_GAMEDLL );
ConVar sv_breachcharge_delay_max( "sv_breachcharge_delay_max", "0.8", FCVAR_RELEASE | FCVAR_GAMEDLL );

ConVar sv_breachcharge_fuse_min( "sv_breachcharge_fuse_min", "0.7", FCVAR_RELEASE | FCVAR_GAMEDLL );
ConVar sv_breachcharge_fuse_max( "sv_breachcharge_fuse_max", "1.0", FCVAR_RELEASE | FCVAR_GAMEDLL );

ConVar sv_breachcharge_arm_delay( "sv_breachcharge_arm_delay", "0.3", FCVAR_RELEASE | FCVAR_GAMEDLL );

#define BREACHCHARGE_MODEL "models/Weapons/w_eq_charge_dropped.mdl"

IMPLEMENT_NETWORKCLASS_ALIASED( BreachChargeProjectile, DT_BreachChargeProjectile )

BEGIN_NETWORK_TABLE( CBreachChargeProjectile, DT_BreachChargeProjectile )
	#ifdef CLIENT_DLL
		RecvPropBool( RECVINFO( m_bShouldExplode ) ),
		RecvPropEHandle( RECVINFO( m_weaponThatThrewMe ) ),
		RecvPropInt( RECVINFO( m_nParentBoneIndex ) ),
		RecvPropVector( RECVINFO( m_vecParentBonePos ) )
	#else
		SendPropBool( SENDINFO( m_bShouldExplode ) ),
		SendPropEHandle( SENDINFO( m_weaponThatThrewMe ) ),
		SendPropInt( SENDINFO( m_nParentBoneIndex ), MAXSTUDIOBONEBITS+1 ),
		SendPropVector(	SENDINFO( m_vecParentBonePos ) )
	#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CBreachChargeProjectile )
END_PREDICTION_DATA()


LINK_ENTITY_TO_CLASS_ALIASED( breachcharge_projectile, BreachChargeProjectile );

static CUtlVector<EHANDLE> g_ActiveBreachCharges;

CBreachChargeProjectile::CBreachChargeProjectile()
{
#ifndef CLIENT_DLL
	m_nParentBoneIndex = -1;
	m_vecParentBonePos.GetForModify().Init();
	m_pDesiredParent = NULL;
	m_bDefused = false;
	m_bUnstuckFromPlayer = false;
#endif
}

#ifndef CLIENT_DLL

CBreachChargeProjectile* CBreachChargeProjectile::Create( const Vector &position, const Vector &velocity, CBaseCombatCharacter *pOwner, CBreachCharge *pDetonator )
{
	Vector vecSrc = position + Vector(0,0,-7);

	CBreachChargeProjectile *pCharge = (CBreachChargeProjectile*)CBaseEntity::Create( "breachcharge_projectile", vecSrc, QAngle(0,0,0), pOwner );
	
	pCharge->ChangeTeam( pOwner->GetTeamNumber() );
	pCharge->GiveAnyKillCreditTo( pOwner );
	pCharge->SetCanBeDetonatedBy( pDetonator );

	pCharge->SetGravity( 1.0f );
	pCharge->SetFriction( 0.8f );
	pCharge->SetElasticity( 0.45f );

	pCharge->SetAbsAngles( pOwner->EyeAngles() + QAngle( -70, -20, 0 ) );
	pCharge->SetAbsVelocity( pOwner->GetAbsVelocity() + (velocity * 500) );
	QAngle angRotationVel = QAngle( RandomFloat(50,200), RandomFloat(-50,200), RandomFloat(-50,200) );
	pCharge->SetLocalAngularVelocity( angRotationVel );

	// make NPCs afraid of it while in the air
	//pCharge->SetThink( &CHEGrenadeProjectile::DangerSoundThink );
	//pCharge->SetNextThink( gpGlobals->curtime );

	pCharge->m_flDamage = 1000;
	pCharge->m_DmgRadius = 200;
	pCharge->SetCollisionGroup( COLLISION_GROUP_PROJECTILE );
	pCharge->SetTouch( &CBreachChargeProjectile::BounceTouch );

	return pCharge;
}

void CBreachChargeProjectile::AlignAnglesTo( const Vector vecTargetDir )
{
	// align the model up Z to the given dir via the least possible rotation on every other axis
	
	QAngle angSurface;
	MatrixAngles( 
		ConcatTransforms( 
			QuaternionMatrix( 
				RotateBetween( 
					EntityToWorldTransform().GetColumn(Z_AXIS),
					vecTargetDir.Normalized() ) ), 
			EntityToWorldTransform() ),
		angSurface );

	SetAbsAngles( angSurface );
}

inline void UTIL_TraceLineIgnoreTwoEntities( const Vector& vecAbsStart, const Vector& vecAbsEnd, unsigned int mask,
	const IHandleEntity *ignore, const IHandleEntity *ignore2, int collisionGroup, trace_t *ptr )
{
	Ray_t ray;
	ray.Init( vecAbsStart, vecAbsEnd );
	CTraceFilterSkipTwoEntities traceFilter( ignore, ignore2, collisionGroup );
	enginetrace->TraceRay( ray, mask, &traceFilter, ptr );
	if ( r_visualizetraces.GetBool() )
	{
		DebugDrawLine( ptr->startpos, ptr->endpos, 255, 0, 0, true, -1.0f );
	}
}

void CBreachChargeProjectile::BounceTouch( CBaseEntity *other )
{
	const trace_t &hitTrace = GetTouchTrace();

	if ( other->IsSolidFlagSet( FSOLID_TRIGGER | FSOLID_VOLUME_CONTENTS ) )
		return;

	// don't hit the guy that launched this grenade or ourselves
	if ( other == GetThrower() || other == this )
		return;

	// Don't stick to skybox or other non-drawn surfaces
	// TODO: Is this the right flag set?
	if ( hitTrace.surface.flags & ( SURF_SKY | SURF_HINT | SURF_SKIP ) )
	{
		return;
	}

	if ( m_bUnstuckFromPlayer && other->IsPlayer() )
	{
		return; // don't stick to player again if just unstuck
	}

	//if ( FClassnameIs( other, "func_breakable" ) )
	//{
	//	return;
	//}

	//if ( FClassnameIs( other, "func_breakable_surf" ) )
	//{
	//	return;
	//}

	// don't stick to ladder brushes, it looks silly
	//if ( FClassnameIs( other, "func_ladder" ) )
	//{
	//	return;
	//}

	if ( hitTrace.DidHit() )
	{
		Vector vecStickPos = hitTrace.endpos - hitTrace.plane.normal * 1.3f;
		SetAbsOrigin( vecStickPos );
		
		// if we hit a player, stick to them for laughter and sport
		CBaseAnimating* pAnimatingTarget = hitTrace.m_pEnt ? hitTrace.m_pEnt->GetBaseAnimating() : nullptr;

		CStudioHdr *pStudioHdr = NULL;
		mstudiohitboxset_t *set = NULL;

		if ( pAnimatingTarget )
		{
			// get the model pointer, and use it to get the hitbox set
			pStudioHdr = pAnimatingTarget->GetModelPtr();
			if ( pStudioHdr )
				set = pStudioHdr->pHitboxSet( pAnimatingTarget->m_nHitboxSet );
		}

		if ( hitTrace.m_pEnt && pStudioHdr && set && (hitTrace.m_pEnt->IsPlayer() || hitTrace.m_pEnt->ClassMatches( "chicken" )) ) // stick carefully onto players and chickens
		{
			// The initial trace just hit the player's abs box. Try and hit an actual hitbox along our flightpath.
			trace_t tr;
			Vector vecFlightDir = ( hitTrace.endpos - hitTrace.startpos ).Normalized();
			UTIL_TraceLineIgnoreTwoEntities( vecStickPos, vecStickPos + vecFlightDir * 32, MASK_VISIBLE_AND_NPCS|CONTENTS_HITBOX, GetThrower(), this, COLLISION_GROUP_NONE, &tr );
			
			if ( !tr.DidHit() || !tr.m_pEnt || tr.m_pEnt != hitTrace.m_pEnt )
			{
				// The hitbox trace failed. Let's find the closest hitbox and trace directly to it.

				float flClosestHitboxDistance = FLT_MAX;
				Vector vecClosestHitboxPos = pAnimatingTarget->GetAbsOrigin(); // gross fallback

				Vector vecPos;
				QAngle angAng;
				for ( int n = 0; n < set->numhitboxes; n++ )
				{
					mstudiobbox_t *pbox = set->pHitbox( n );
					pAnimatingTarget->GetHitboxBonePosition( pbox->bone, vecPos, angAng, pbox->angOffsetOrientation );

					float flDist = vecPos.DistToSqr( vecStickPos );

					if ( flDist < flClosestHitboxDistance )
					{
						flClosestHitboxDistance = flDist;
						vecClosestHitboxPos = vecPos;
					}
				}

				// trace to try and hit this hitbox
				UTIL_ClearTrace( tr );
				UTIL_TraceLineIgnoreTwoEntities( vecStickPos, vecClosestHitboxPos, MASK_VISIBLE_AND_NPCS|CONTENTS_HITBOX, GetThrower(), this, COLLISION_GROUP_NONE, &tr );
			}

			// We should have a valid trace now
			if ( tr.m_pEnt && tr.m_pEnt == hitTrace.m_pEnt )
			{
				AlignAnglesTo( -vecFlightDir );
				vecStickPos = tr.endpos - vecFlightDir * 2;
				SetAbsOrigin( vecStickPos );

				// save the position offset from the bone, we'll network that down to the client
				mstudiobbox_t *pbox = set->pHitbox( tr.hitbox );
				m_nParentBoneIndex = pbox->bone;
				
				matrix3x4_t matBoneToWorldTransform;
				pAnimatingTarget->GetBoneTransform( m_nParentBoneIndex, matBoneToWorldTransform );
				m_vecParentBonePos = VectorTransform( vecStickPos, matBoneToWorldTransform.InverseTR() );
			}	
		}
		else
		{
			// stick to non-player
			AlignAnglesTo( hitTrace.plane.normal );
			m_nParentBoneIndex = -1;
		}
		
		m_vecLastKnownValidPos = GetAbsOrigin();

		// can't just set the parent here, we're technically in the middle of physics flying, and it breaks stuff
		// if we suddenly become the child of the world in the middle of that.
		m_pDesiredParent = hitTrace.m_pEnt ? hitTrace.m_pEnt : GetWorldEntity();
		
		m_bUnstuckFromPlayer = false;

		SetArmed();
	}
}

void CBreachChargeProjectile::SetArmed( void )
{
	SetMoveType( MOVETYPE_NONE );
	SetSolidFlags( FSOLID_NOT_SOLID );

	SetTouch(NULL);

	EmitSound( "Survival.BreachChargeSetArmed" );
	EmitSound( "Survival.BreachChargeClick" );

	SetThink( &CBreachChargeProjectile::StuckToSurfaceThink );
	SetNextThink( gpGlobals->curtime );
}

void CBreachChargeProjectile::ExplodeThink( void )
{
	SetThink( NULL );
	Detonate();
}

bool CBreachChargeProjectile::ShouldRemoveOnParentRemoval( void )
{
	SetParent( NULL ); // next think should take care of what to do now our parent is gone
	m_pDesiredParent = NULL;

	return false; // that is, DON'T remove me if my parent is gone
}

void CBreachChargeProjectile::StuckToSurfaceThink( void )
{
	SetNextThink( gpGlobals->curtime + 0.1f );

	if ( m_pDesiredParent && !GetMoveParent() )
	{
		CCSPlayer *pParent = dynamic_cast<CCSPlayer*>(m_pDesiredParent);
		m_iParentClass = pParent ? pParent->PlayerClass() : 0;

		SetParent( m_pDesiredParent ); // we've been wanting to attach to this, so do it now
		m_pDesiredParent = NULL;
	}
	else
	{
		bool bShouldDetach = (!GetMoveParent());
		if ( !bShouldDetach && GetMoveParent()->IsPlayer() )
		{
			CCSPlayer *pParent = dynamic_cast<CCSPlayer*>(GetMoveParent());
			if ( !pParent || !pParent->IsAlive() || m_iParentClass != pParent->PlayerClass() )
			{
				bShouldDetach = true;
			}
		}

		if ( bShouldDetach )
		{
			// our parent broke, disappeared, or died! Fall off and stick to something else...

			SetParent( NULL );
			m_pDesiredParent = NULL;

			SetSolidFlags( FSOLID_NOT_STANDABLE );
			SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_BOUNCE );
			m_nParentBoneIndex = -1;

			SetThink( NULL );
			SetTouch( &CBreachChargeProjectile::BounceTouch );

			SetAbsOrigin( m_vecLastKnownValidPos );
			SetAbsVelocity( Vector( RandomFloat( -100, 100 ), RandomFloat( -100, 100 ), 200 ) );

			SetNextThink( gpGlobals->curtime );
		}
		else
		{
			m_vecLastKnownValidPos = GetAbsOrigin();
		}

	}

	if ( !m_bResolvedParent )
	{
		m_bResolvedParent = true;
		SetNextThink( gpGlobals->curtime + sv_breachcharge_arm_delay.GetFloat() );
		return;
	}

	if ( GetDangerZoneController() && !GetDangerZoneController()->IsWithinPlayArea( GetAbsOrigin() ) )
	{
		// charges blow up when outside the play area
		m_bShouldExplode = true;
	}

	if ( m_bShouldExplode )
	{
		SetThink( &CBreachChargeProjectile::ExplodeThink );

		float flDistancePenalty = 0;
		CCSPlayer *pThrower = ToCSPlayer( GetThrower() );
		if ( pThrower && pThrower->IsAlive() )
		{
			Vector vecOrigin = GetAbsOrigin();
			Vector vecDetonatorPos = pThrower->Weapon_ShootPosition();

			// start to add a delay beyond a given distance
			flDistancePenalty = RemapValClamped( vecOrigin.DistTo( vecDetonatorPos ),
				sv_breachcharge_distance_min.GetFloat(),
				sv_breachcharge_distance_max.GetFloat(),
				sv_breachcharge_delay_min.GetFloat(),
				sv_breachcharge_delay_max.GetFloat() );
		}

		SetNextThink( gpGlobals->curtime + flDistancePenalty + RandomFloat( sv_breachcharge_fuse_min.GetFloat(), sv_breachcharge_fuse_max.GetFloat() ) );
		EmitSound( "Survival.BreachSoundWarningBeep" );
	}

}

void CBreachChargeProjectile::Detonate( void )
{

	if ( m_bDefused )
	{
		EmitSound( "Survival.BreachDefused" );
		UTIL_Remove( this );
		return;
	}

	// tell the bots an HE grenade has exploded (and record the event in the log)
	//if ( CCSPlayer *player = ToCSPlayer( GetThrower() ) )
	//{
	//	IGameEvent * event = gameeventmanager->CreateEvent( "hegrenade_detonate" );
	//	if ( event )
	//	{
	//		event->SetInt( "userid", player->GetUserID() );
	//		event->SetInt( "entityid", this->entindex() );
	//		event->SetFloat( "x", GetAbsOrigin().x );
	//		event->SetFloat( "y", GetAbsOrigin().y );
	//		event->SetFloat( "z", GetAbsOrigin().z );
	//		gameeventmanager->FireEvent( event );
	//	}
	//}

	Vector vecForward, vecRight, vecUp;
	MatrixVectors( EntityToWorldTransform(), &vecForward, &vecRight, &vecUp );

	trace_t tr;
	UTIL_TraceLine ( GetAbsOrigin(), GetAbsOrigin() + vecUp * 2, MASK_SHOT_HULL, this, COLLISION_GROUP_NONE, & tr);
	
	Explode( &tr, DMG_BLAST );
	
	UTIL_ScreenShake( GetAbsOrigin(), 50.0f, 150.0, 1.0, GetShakeRadius(), SHAKE_START );

	Vector vecFlashPos = tr.endpos;
	CPVSFilter flashFilter( vecFlashPos );
	te->DynamicLight( flashFilter, 0.0, &vecFlashPos, 255, 163, 55, 5, 150, 0.15, 100 );

	// forcibly destroy doors within radius, no matter what. These are breach charges, after all.
	Vector vecOrigin = GetAbsOrigin();
	FOR_EACH_VEC( IDZDoor::AutoList(), iDoor )
	{
		CDZDoor *pDoor = static_cast<CDZDoor*>(IDZDoor::AutoList()[iDoor]);
		
		//if ( !pDoor->IsSecurityDoor() ) // not just security doors
		//	continue;

		if ( pDoor->WorldSpaceCenter().DistToSqr( vecOrigin ) < ( 80.0f * 80.0f ) )
		{
			CTakeDamageInfo info( this, GetThrower(), pDoor->GetHealth(), DMG_BLAST );
			pDoor->TakeDamage( info );
		}
	}

}

#endif

#ifdef CLIENT_DLL

void CBreachChargeProjectile::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );
	SetAllowFastPath( (m_nParentBoneIndex == -1) );
}

int CBreachChargeProjectile::DrawModel( int flags, const RenderableInstance_t &instance )
{
	// if we're stuck to a player
	if ( m_nParentBoneIndex != -1 && GetMoveParent() )
	{
		CBasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
		if ( pLocalPlayer && pLocalPlayer == GetMoveParent() )
		{
			// in first-person, stick under the camera so it never obscures the view
			SetAbsAngles( pLocalPlayer->EyeAngles() );
			SetAbsOrigin( pLocalPlayer->EyePosition() + Vector(0,0,-20) );
		}
		else
		{
			C_BaseAnimating *pBaseAnimating = GetMoveParent()->GetBaseAnimating();
			if ( pBaseAnimating )
			{
				matrix3x4_t matParentBoneToWorld;
				pBaseAnimating->GetBoneTransform( m_nParentBoneIndex, matParentBoneToWorld );
				SetAbsOrigin( VectorTransform( m_vecParentBonePos.Get(), matParentBoneToWorld ) );
			}
		}
	}

	return BaseClass::DrawModel(flags, instance);
}
#endif

#ifndef CLIENT_DLL
void CBreachChargeProjectile::Use( CBaseEntity * pActivator, CBaseEntity * pCaller, USE_TYPE useType, float value )
{

	if ( pActivator && pActivator->IsPlayer() )
	{
		CCSPlayer *pCSPlayer = ToCSPlayer( pActivator );
		if ( pCSPlayer )
		{

			if ( ToBaseCombatCharacter( pCSPlayer ) != GetThrower() )
			{
				if ( pActivator == GetMoveParent() ) // using a bomb that's stuck to ourselves
				{
					m_bUnstuckFromPlayer = true;
					SetParent( NULL );
					m_pDesiredParent = NULL;
				}
				else if ( !m_bDefused )
				{
					m_bDefused = true;
					SetRenderColor( 80, 80, 80 );
					EmitSound( "Survival.BreachDefused" );
					return;
				}
			}
			else
			{
				CBreachCharge *pDetonator = dynamic_cast<CBreachCharge*>(pCSPlayer->Weapon_OwnsThisType( "weapon_breachcharge" ));
				if ( pDetonator && GetCanBeDetonatedBy( pDetonator ) )
				{
					int nAmmoAvailable = pDetonator->GetMaxClip1() - pDetonator->Clip1();
					if ( nAmmoAvailable )
					{
						pDetonator->m_iClip1 += 1;
						EmitSound( "Survival.BreachUse" );
						UTIL_Remove( this );

						if ( pCSPlayer->GetActiveCSWeapon() == pDetonator )
							pDetonator->SendWeaponAnim( ACT_VM_IDLE );
					}
				}
			}
			
		}
	}

}
#endif

void CBreachChargeProjectile::Spawn()
{
#ifndef CLIENT_DLL
	m_bResolvedParent = false;
	m_takedamage = DAMAGE_YES;
#endif // !CLIENT_DLL

	Precache();

	SetModel( BREACHCHARGE_MODEL );
	BaseClass::Spawn();

	//SetBodygroupPreset( "thrown" );

	SetSolidFlags( FSOLID_NOT_STANDABLE );
	SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_BOUNCE );

	SetSolid( SOLID_BBOX );

	AddFlag( FL_GRENADE );
	
	//Vector min = Vector( -4.4, -2.6, -1.2 );
	//Vector max = Vector( 4.4, 2.6, 1.2 );

	Vector min = Vector( -2, -2, -2 );
	Vector max = Vector( 2, 2, 2 );

	SetSize( min, max );

 	if ( CollisionProp() )
 		CollisionProp()->SetCollisionBounds( min, max );

	g_ActiveBreachCharges.AddToTail( this );
}

void CBreachChargeProjectile::Precache()
{
	PrecacheModel( BREACHCHARGE_MODEL );

	PrecacheScriptSound( "HEGrenade.Bounce" );

	PrecacheScriptSound("Survival.BreachChargeSetArmed");
	PrecacheScriptSound("Survival.BreachChargeClick");
	PrecacheScriptSound("Survival.BreachSoundWarningBeep");
	PrecacheScriptSound("Survival.BreachSoundActivate");
	PrecacheScriptSound("Survival.BreachSoundActivateNoBombs");
	PrecacheScriptSound("Survival.BreachDefused");
	PrecacheScriptSound("Survival.BreachUse");
	PrecacheScriptSound("Survival.BreachThrow");

	BaseClass::Precache();
}

#ifndef CLIENT_DLL
int CBreachChargeProjectile::OnTakeDamage( const CTakeDamageInfo &info )
{
	if ( info.GetDamageType() & ( DMG_BLAST | DMG_BURN ) )
	{
		m_bShouldExplode = true;
	}
	else 
	{
		CBaseEntity *pInflictor = info.GetInflictor();
		if ( pInflictor && pInflictor->ClassMatches( "prop_exploding_barrel" ) ) // FIXME: why do explosive barrels not do DMG_BLAST damage?
		{
			m_bShouldExplode = true;
		}
	}
	return 0;
}
#endif

#define BREACHCHARGE_DEPLOY_TIME 0.3f

// ----------------------------------------------------------------------------- //
// CBreachCharge tables.
// ----------------------------------------------------------------------------- //

IMPLEMENT_NETWORKCLASS_ALIASED( BreachCharge, DT_WeaponBreachCharge )

BEGIN_NETWORK_TABLE( CBreachCharge, DT_WeaponBreachCharge )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CBreachCharge )
END_PREDICTION_DATA()


LINK_ENTITY_TO_CLASS_ALIASED( weapon_breachcharge, BreachCharge );
// PRECACHE_REGISTER( weapon_breachcharge ); // No Precache override, so not needed

#define BREACHCHARGE_ICON "models/weapons/v_models/breachcharge/breachcharge_icon"
PRECACHE_REGISTER_BEGIN( GLOBAL, BreachChargeMaterials )
PRECACHE( MATERIAL, BREACHCHARGE_ICON )
PRECACHE_REGISTER_END()

// ----------------------------------------------------------------------------- //
// CBreachCharge implementation.
// ----------------------------------------------------------------------------- //

CBreachCharge::CBreachCharge()
{
#ifdef CLIENT_DLL
	m_pIconMaterial = NULL;
#endif
}

bool CBreachCharge::HasPrimaryAmmo()
{
	return true;
}

bool CBreachCharge::CanBeSelected()
{
	return true;
}

void CBreachCharge::Spawn()
{
	BaseClass::Spawn();

#ifndef CLIENT_DLL
	m_iClip1 = GetMaxClip1();
#endif
}

bool CBreachCharge::Deploy()
{
	bool bDeploy = BaseClass::Deploy();

	if ( Clip1() > 0 )
	{
		SendWeaponAnim( ACT_VM_DRAW );
	}
	else
	{
		SendWeaponAnim( ACT_VM_EMPTY_DRAW );
	}
	
	m_flNextPrimaryAttack = gpGlobals->curtime + BREACHCHARGE_DEPLOY_TIME;
	m_flNextSecondaryAttack = gpGlobals->curtime + BREACHCHARGE_DEPLOY_TIME;
	SetWeaponIdleTime( gpGlobals->curtime + 2 );

	return bDeploy;
}

void CBreachCharge::PrimaryAttack()
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	if ( gpGlobals->curtime > m_flNextPrimaryAttack && Clip1() > 0 )
	{
		m_flNextPrimaryAttack = gpGlobals->curtime + 0.5f;
		SetWeaponIdleTime( gpGlobals->curtime + 20 );

		if ( Clip1() == 1 )
		{
			SendWeaponAnim( ACT_VM_EMPTY_FIRE );
		}
		else
		{
			SendWeaponAnim( ACT_VM_PRIMARYATTACK );
		}

		m_iClip1--;

#ifndef CLIENT_DLL
		Vector vecThrowDir;
		AngleVectors( pPlayer->EyeAngles(), &vecThrowDir );

		CBreachChargeProjectile::Create( pPlayer->Weapon_ShootPosition(), vecThrowDir, pPlayer, this );

		EmitSound( "Survival.BreachThrow" );
#endif
	}
}

void CBreachCharge::SecondaryAttack()
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	if ( gpGlobals->curtime > m_flNextSecondaryAttack )
	{
		m_flNextSecondaryAttack = gpGlobals->curtime + 2.0f;
		SetWeaponIdleTime( gpGlobals->curtime + 20 );
		
		if ( Clip1() > 0 )
		{
			SendWeaponAnim( ACT_VM_HITRIGHT );
		}
		else
		{
			SendWeaponAnim( ACT_VM_HITRIGHT2 );
		}

#ifndef CLIENT_DLL
		EmitSound( "Survival.BreachSoundActivate" );

		FOR_EACH_VEC_BACK( g_ActiveBreachCharges, n )
		{
			CBreachChargeProjectile *pCharge = dynamic_cast< CBreachChargeProjectile* >( g_ActiveBreachCharges[n].Get() );
			if ( !pCharge )
			{
				g_ActiveBreachCharges.Remove( n );
			}
			else if ( pCharge->GetCanBeDetonatedBy( this ) )
			{
				pCharge->GiveAnyKillCreditTo( pPlayer );
				pCharge->m_bShouldExplode = true;
				g_ActiveBreachCharges.Remove( n );
			}
		}

		if ( Clip1() == 0 )
		{
			SetThink( &CBreachCharge::RemoveThink );
			SetNextThink( gpGlobals->curtime + 1.4f );
		}
#endif
	}
}

void CBreachCharge::WeaponIdle()
{
	if (m_flTimeWeaponIdle > gpGlobals->curtime)
		return;
	
	if ( Clip1() > 0 )
	{
		SendWeaponAnim( ACT_VM_IDLE );
	}
	else
	{
		SendWeaponAnim( ACT_VM_EMPTY_IDLE );
	}

	SetWeaponIdleTime( gpGlobals->curtime + 20 );
}

void CBreachCharge::SetModel( const char *szModelName )
{
	BaseClass::SetModel( szModelName );

	MDLCACHE_CRITICAL_SECTION();

	//set bodygroup to match ammo
	if ( V_stristr( szModelName, "dropped" ) )
	{
		if ( m_iClip1 == -1 )
		{
			SetBodygroup( 0, 4 ); // fully stocked
		}
		else
		{
			SetBodygroup( 0, m_iClip1 + 1 );
		}
	}
}

#ifndef CLIENT_DLL

void CBreachCharge::RemoveThink()
{
	if ( Clip1() > 0 )
	{
		SetThink(NULL);
		return;
	}

	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( pPlayer )
	{
		pPlayer->Weapon_Drop( this, NULL, NULL ); // so we switch
	}

	UTIL_Remove(this);
}

bool CBreachCharge::HasAnyLiveCharges( void )
{

	FOR_EACH_VEC_BACK( g_ActiveBreachCharges, n )
	{
		CBreachChargeProjectile *pCharge = dynamic_cast<CBreachChargeProjectile*>(g_ActiveBreachCharges[n].Get());
		if ( !pCharge )
		{
			g_ActiveBreachCharges.Remove( n );
		}
		else if ( pCharge->GetCanBeDetonatedBy( this ) )
		{
			return true;
		}
	}

	return false;
}

bool CBreachCharge::ExtractAmmoFromOther( CBreachCharge* pOther )
{
	bool bTookAnything = false;

	int nAmmoMissing = GetMaxClip1() - Clip1();

	bool bAllChargesThrown = (Clip1() == 0);

	if ( nAmmoMissing > 0 )
	{
		int nAmmoAvailable = MIN( pOther->m_iClip1, nAmmoMissing );

		if ( nAmmoAvailable > 0 )
		{
			pOther->m_iClip1 -= nAmmoAvailable;
			m_iClip1 += nAmmoAvailable;

			pOther->SetBodygroup( 0, pOther->m_iClip1 + 1 );

			bTookAnything = true;
		}

		if ( pOther->m_iClip1 <= 0 )
		{
			// take control of the other detonator's mines

			FOR_EACH_VEC_BACK( g_ActiveBreachCharges, n )
			{
				CBreachChargeProjectile *pCharge = dynamic_cast< CBreachChargeProjectile* >( g_ActiveBreachCharges[n].Get() );
				if ( !pCharge )
				{
					g_ActiveBreachCharges.Remove( n );
				}
				else if ( pCharge->GetCanBeDetonatedBy( pOther ) )
				{
					pCharge->GiveAnyKillCreditTo( GetOwner() );
					pCharge->SetCanBeDetonatedBy( this );
				}
			}

			UTIL_Remove( pOther );

			bTookAnything = true;
		}
		
	}

	if ( bAllChargesThrown && bTookAnything && Clip1() > 0 )
	{
		SendWeaponAnim( ACT_VM_IDLE );
	}

	return bTookAnything;
}

#else

void CBreachCharge::WeaponPreRender( void )
{
	CCSPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer || pPlayer->GetActiveCSWeapon() != this )
		return;

	if ( !g_ActiveBreachCharges.Count() )
		return;

	CMatRenderContextPtr pRenderContext(materials);
	
	if ( m_pIconMaterial == NULL )
		m_pIconMaterial = materials->FindMaterial( BREACHCHARGE_ICON, TEXTURE_GROUP_OTHER, false );
	
	
	if ( m_pIconMaterial != NULL )
	{
		Vector vecPlayerPos = pPlayer->Weapon_ShootPosition();

		FOR_EACH_VEC_BACK( g_ActiveBreachCharges, n )
		{
			CBreachChargeProjectile *pCharge = dynamic_cast< CBreachChargeProjectile* >( g_ActiveBreachCharges[n].Get() );
			if ( !pCharge )
			{
				g_ActiveBreachCharges.Remove( n );
			}
			else if ( pCharge->GetMoveType() == MOVETYPE_NONE && pCharge->GetCanBeDetonatedBy( this ) )
			{
				Vector vecChargePos = pCharge->GetAbsOrigin();
				
				float flDist = pCharge->m_bShouldExplode ? 0 : vecChargePos.DistToSqr( vecPlayerPos );

				if ( flDist > 250000 )
					continue;

				float flPulse = fmod( gpGlobals->curtime, 0.25f ) * 4.0f;
				float flSize = RemapValClamped( flDist, 90000, 250000, 32 + flPulse * 4, 8 );

				Vector vecOrigin;
				ScreenTransform( pCharge->GetAbsOrigin() + Vector(0,0,3), vecOrigin );
				ConvertNormalizedScreenSpaceToPixelScreenSpace( vecOrigin );

				vecOrigin.x -= flSize * 0.5f;
				vecOrigin.y -= flSize * 0.5f;
	
				pRenderContext->DrawScreenSpaceRectangle( m_pIconMaterial, 
										vecOrigin.x, vecOrigin.y,
										flSize, flSize,
										0, 0, 64, 64, 64, 64 );
			}
		}
	}

}

#endif
