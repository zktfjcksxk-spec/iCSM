//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "decoy_projectile.h"
#include "engine/IEngineSound.h"
#include "keyvalues.h"
#include "weapon_csbase.h"
#include "particle_parse.h"

#if defined( CLIENT_DLL )

	#include "c_cs_player.h"
#else
	#include "sendproxy.h"
	#include "cs_player.h"
	#include "bot_manager.h"
#endif

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

#if defined( CLIENT_DLL )

IMPLEMENT_CLIENTCLASS_DT( C_DecoyProjectile, DT_DecoyProjectile, CDecoyProjectile )
END_RECV_TABLE()


//--------------------------------------------------------------------------------------------------------
void C_DecoyProjectile::OnNewParticleEffect( const char *pszParticleName, CNewParticleEffect *pNewParticleEffect )
{
	if ( FStrEq( pszParticleName, "weapon_decoy_ground_effect" ) )
	{
		m_decoyParticleEffect = pNewParticleEffect;
	}
}


//--------------------------------------------------------------------------------------------------------
void C_DecoyProjectile::OnParticleEffectDeleted( CNewParticleEffect *pParticleEffect )
{
	if ( m_decoyParticleEffect == pParticleEffect )
	{
		m_decoyParticleEffect = NULL;
	}
}

float C_DecoyProjectile::GetParticleEffectTimeSinceSpawn() const
{
	if ( m_decoyParticleEffect.IsValid() && m_flTimeParticleEffectSpawn )
		return gpGlobals->curtime - m_flTimeParticleEffectSpawn;
	else
		return 0.0f;
}

//--------------------------------------------------------------------------------------------------------
bool C_DecoyProjectile::Simulate( void )
{
	// we are still moving
	if ( GetAbsVelocity().Length() > 0.1f )
	{
		return true;
	}

	if ( !m_decoyParticleEffect.IsValid() )
	{
		DispatchParticleEffect( "weapon_decoy_ground_effect", PATTACH_POINT_FOLLOW, this, "Wick" );
		m_flTimeParticleEffectSpawn = gpGlobals->curtime;
	}
	else
	{
		m_decoyParticleEffect->SetSortOrigin( GetAbsOrigin() );
		m_decoyParticleEffect->SetNeedsBBoxUpdate( true );
	}

	BaseClass::Simulate();
	return true;
}

#else // GAME_DLL

#define GRENADE_MODEL "models/Weapons/w_eq_decoy_dropped.mdl"

LINK_ENTITY_TO_CLASS( decoy_projectile, CDecoyProjectile );
PRECACHE_REGISTER( decoy_projectile );

IMPLEMENT_SERVERCLASS_ST( CDecoyProjectile, DT_DecoyProjectile )
END_SEND_TABLE()

BEGIN_DATADESC( CDecoyProjectile )
	DEFINE_THINKFUNC( Think_Detonate ),
	DEFINE_THINKFUNC( GunfireThink )
END_DATADESC()

IMPLEMENT_AUTO_LIST( IDecoyProjectile )


struct DecoyWeaponProfile
{
	CSWeaponType	weaponType;
	int				minShots;
	int				maxShots;
	float			extraDelay;
	float			pauseMin;
	float			pauseMax;
};

DecoyWeaponProfile gDecoyWeaponProfiles[] = 
{
	//	CSWeaponType		minShots, maxShots, extraDelay, pauseMin, pauseMax
	{	WEAPONTYPE_PISTOL,			1,	3,		0.3f,		0.5f,	4.0f	},
	{	WEAPONTYPE_SUBMACHINEGUN,	1,	5,		0.0f,		0.5f,	4.0f	},
	{	WEAPONTYPE_RIFLE,			1,	3,		0.5f,		0.5f,	4.0f	},
	{	WEAPONTYPE_SHOTGUN,			1,	3,		0.0f,		0.5f,	4.0f	},
	{	WEAPONTYPE_SNIPER_RIFLE,	1,	3,		0.5f,		0.5f,	4.0f	},
#if DECOY_ALWAYS_NEGEV
	{	WEAPONTYPE_MACHINEGUN,		50,	150,	0.0f,		0.5f,	4.0f	},
#else
	{	WEAPONTYPE_MACHINEGUN,		6,	20,		0.0f,		0.5f,	4.0f	},
#endif
};

// --------------------------------------------------------------------------------------------------- //
// CFlashbangProjectile implementation.
// --------------------------------------------------------------------------------------------------- //

CDecoyProjectile* CDecoyProjectile::Create( 
	const Vector &position, 
	const QAngle &angles, 
	const Vector &velocity, 
	const AngularImpulse &angVelocity, 
	CBaseCombatCharacter *pOwner,
	item_definition_index_t weaponItem )
{
	CDecoyProjectile *pGrenade = ( CDecoyProjectile* )CBaseEntity::Create( "decoy_projectile", position, angles, pOwner );
	
	// Set the timer for 1 second less than requested. We're going to issue a SOUND_DANGER
	// one second before detonation.
	pGrenade->SetTimer( 2.0f );
	pGrenade->SetAbsVelocity( velocity );
	pGrenade->SetupInitialTransmittedGrenadeVelocity( velocity );
	pGrenade->SetThrower( pOwner );

	pGrenade->m_flDamage = 25.0f; // 25 = 1/4 of HEGrenade Damage
	pGrenade->m_DmgRadius = pGrenade->m_flDamage * 3.5f; // Changing damage will change the radius
	pGrenade->ChangeTeam( pOwner->GetTeamNumber() );
	pGrenade->ApplyLocalAngularVelocityImpulse( angVelocity );
	pGrenade->SetTouch( &CBaseGrenade::BounceTouch );

	pGrenade->SetGravity( BaseClass::GetGrenadeGravity() );
	pGrenade->SetFriction( BaseClass::GetGrenadeFriction() );
	pGrenade->SetElasticity( BaseClass::GetGrenadeElasticity() );

	pGrenade->SetSourceWeaponInfo( weaponItem );

	ASSERT(pOwner != NULL);

#if DECOY_ALWAYS_NEGEV
	const CSchemaItemDefHandle hNegev( "weapon_negev" );
	pGrenade->m_decoyWeaponDefIndex = hNegev->GetDefinitionIndex();
#else
	// pick a weapon based on what the player is carrying, falling back to default starting pistols
	CBaseCombatWeapon* pPrimaryWeapon = pOwner->Weapon_GetSlot( GEAR_SLOT_RIFLE );
	CBaseCombatWeapon* pSecondaryWeapon = pOwner->Weapon_GetSlot( GEAR_SLOT_PISTOL );
	pGrenade->m_decoyWeaponDefIndex = INVALID_ITEM_DEF_INDEX;
#endif
	pGrenade->m_decoyWeaponSoundType = SINGLE;

#if !DECOY_ALWAYS_NEGEV
	if ( pPrimaryWeapon != NULL )
	{
		CEconItemView* pItem = pPrimaryWeapon->GetEconItemView();
		if ( pItem && pItem->IsValid() )
		{
			pGrenade->m_decoyWeaponDefIndex = pItem->GetItemDefinition()->GetDefinitionIndex();
			CWeaponCSBase *pWeapon = static_cast<CWeaponCSBase *>( pPrimaryWeapon );
			if ( pWeapon )
			{
				pGrenade->m_decoyWeaponSoundType = ( pWeapon->HasSilencer() && pWeapon->IsSilenced() ) ? SPECIAL1 : SINGLE;
			}
		}
	}
	else if ( pSecondaryWeapon != NULL )
	{
		CEconItemView* pItem = pSecondaryWeapon->GetEconItemView();
		if ( pItem && pItem->IsValid() )
		{
			pGrenade->m_decoyWeaponDefIndex = pItem->GetItemDefinition()->GetDefinitionIndex();
			CWeaponCSBase *pWeapon = static_cast<CWeaponCSBase *>( pSecondaryWeapon );
			if ( pWeapon )
			{
				pGrenade->m_decoyWeaponSoundType = ( pWeapon->HasSilencer() && pWeapon->IsSilenced() ) ? SPECIAL1 : SINGLE;
			}
		}
	}
	else
	{
		if ( pOwner->GetTeamNumber() == TEAM_CT )
		{
			pGrenade->m_decoyWeaponDefIndex = WeaponIDToDefinition( WEAPON_HKP2000 )->GetDefinitionIndex();
		}
		else
		{
			pGrenade->m_decoyWeaponDefIndex = WeaponIDToDefinition( WEAPON_GLOCK )->GetDefinitionIndex();
		}
	}
#endif // DECOY_ALWAYS_NEGEV

#if DECOY_ALWAYS_NEGEV
	pGrenade->m_pProfile = &gDecoyWeaponProfiles[ 5 ]; // NEGEV
#else
	// find the corresponding decoy firing profile
	const CEconItemView* pDecoyWeaponView = CSInventoryManager()->GetReferenceEconItem( pGrenade->m_decoyWeaponDefIndex, 0 );
	const CCSWeaponInfo* pDecoyWeaponInfo = GetWeaponInfoFromItem( pDecoyWeaponView );
	pGrenade->m_pProfile = &gDecoyWeaponProfiles[0];
	for ( int i = 0; i < ARRAYSIZE( gDecoyWeaponProfiles ); ++i )
	{
		if ( gDecoyWeaponProfiles[i].weaponType == pDecoyWeaponInfo->GetWeaponType( pDecoyWeaponView ) ) 
		{
			pGrenade->m_pProfile = &gDecoyWeaponProfiles[i];
			break;
		}
	}
#endif // DECOY_ALWAYS_NEGEV

	pGrenade->SetCollisionGroup( COLLISION_GROUP_PROJECTILE );
	return pGrenade;
}

void CDecoyProjectile::SetTimer( float timer )
{
	SetThink( &CDecoyProjectile::Think_Detonate );
	SetNextThink( gpGlobals->curtime + timer );

	TheBots->SetGrenadeRadius( this, 0.0f );
}
#if DECOY_ALWAYS_NEGEV
#define DECOY_DURATION 8 // 11.3 is full clip (150) @ 0.075 cycle time
#endif

void CDecoyProjectile::Think_Detonate( void )
{
#if 1
	if ( GetAbsVelocity().Length() > 0.2f )
	{
		// Still moving. Don't detonate yet.
		SetNextThink( gpGlobals->curtime + 0.2f );
		return;
	}
#endif // 0

	CCSPlayer *player = ToCSPlayer( GetThrower() );
	if ( player )
	{
		IGameEvent * event = gameeventmanager->CreateEvent( "decoy_started" );
		if ( event )
		{
			event->SetInt( "userid", player->GetUserID() );
			event->SetInt( "entityid", this->entindex() );
			event->SetFloat( "x", GetAbsOrigin().x );
			event->SetFloat( "y", GetAbsOrigin().y );
			event->SetFloat( "z", GetAbsOrigin().z );
			gameeventmanager->FireEvent( event );
		}
	}

	m_shotsRemaining = 0;
	
	if ( CSGameRules() && CSGameRules()->IsPlayingSurvival() )
	{
		m_shotsRemaining = 40;
	}

#if DECOY_ALWAYS_NEGEV
	m_fExpireTime = gpGlobals->curtime + DECOY_DURATION;
#else	
	m_fExpireTime = gpGlobals->curtime + 14.0f; // TODO: Make this Data Driven
#endif

	SetThink( &CDecoyProjectile::GunfireThink );
	TheBots->SetGrenadeRadius( this, DecoyGrenadeRadius );
	GunfireThink(); // This will handling the 'Detonate'
}

void CDecoyProjectile::Detonate( void )
{
	// [mlowrance] The Decoy is handling it's own detonate.
	Assert(!"Decoy grenade handles its own detonation");
}

void CDecoyProjectile::GunfireThink( void )
{
	ASSERT(m_pProfile != NULL);
	if ( !m_pProfile )
		return;

	const char *shootsound = NULL;
	const CEconItemView* pGenericWeaponView = NULL;
	const CCSWeaponInfo* pWeaponInfo = NULL;

	bool bBR = (CSGameRules() && CSGameRules()->IsPlayingSurvival());

	if ( bBR )
	{
		if ( m_shotsRemaining % 2 == 0 )
		{
			shootsound = "CT_Default.StepLeft";
		}
		else
		{
			shootsound = "CT_Default.StepRight";
		}
	}
	else
	{

		if ( m_shotsRemaining <= 0 )
		{
			// pick a new burst activity
			m_shotsRemaining = RandomInt( m_pProfile->minShots, m_pProfile->maxShots );
		}

		pGenericWeaponView = CSInventoryManager()->GetReferenceEconItem( m_decoyWeaponDefIndex, 0 );
		pWeaponInfo = GetWeaponInfoFromItem( pGenericWeaponView );

		CBroadcastRecipientFilter filter;

		// $$$REI TODO should fire non-weaponid weapons too, remove weaponid here and use defindex
		shootsound = pWeaponInfo->GetShootSound( pGenericWeaponView, m_decoyWeaponSoundType );

		if ( m_decoyWeaponDefIndex != INVALID_ITEM_DEF_INDEX )
		{
			// Get the item definition
			const CEconItemDefinition *pDef = (m_decoyWeaponDefIndex > 0) ? GetItemSchema()->GetItemDefinition( m_decoyWeaponDefIndex ) : NULL;
			if ( pDef )
			{
				const char *pszTempSound = pDef->GetWeaponReplacementSound( m_decoyWeaponSoundType );
				if ( pszTempSound )
				{
					shootsound = pszTempSound;
				}
			}
		}

	}

	Assert( shootsound );

	CSoundParameters params;
	if ( GetParametersForSound( shootsound, params, NULL ) )
	{
		CPASAttenuationFilter filter( this, params.soundlevel );
		EmitSound( filter, entindex(), shootsound, &GetLocalOrigin(), 0.0f ); 
		DispatchParticleEffect( "weapon_decoy_ground_effect_shot", GetAbsOrigin(), GetAbsAngles() );
	}

	// tell the bots about the gunfire
	CCSPlayer *pPlayer = ToCSPlayer( GetThrower() );
	if ( pPlayer )
	{
		// allow the bots to react to the "gunfire"

		// we need a custom event here, because the "weapon_fire" event assumes
		// the gunfire is coming from the player!
		IGameEvent * event = gameeventmanager->CreateEvent( "decoy_firing" );
		if ( event )
		{
			event->SetInt( "userid", pPlayer->GetUserID() );
			event->SetInt( "entityid", this->entindex() );
			event->SetFloat( "x", GetAbsOrigin().x );
			event->SetFloat( "y", GetAbsOrigin().y );
			event->SetFloat( "z", GetAbsOrigin().z );
			gameeventmanager->FireEvent( event );
		}
	}

	if ( --m_shotsRemaining > 0 )
	{
		if ( bBR )
		{
			SetNextThink( gpGlobals->curtime + RandomFloat( 0.3f, 0.4f ) );
		}
		else
		{
#if DECOY_ALWAYS_NEGEV
			SetNextThink( gpGlobals->curtime + pWeaponInfo->GetCycleTime( pGenericWeaponView ) );
#else	
			SetNextThink( gpGlobals->curtime + pWeaponInfo->GetCycleTime( pGenericWeaponView ) + RandomFloat( 0.0f, m_pProfile->extraDelay ) );
#endif
		}
		return;
	}

	if ( gpGlobals->curtime < m_fExpireTime 
			&& !bBR 
		)
	{
		SetNextThink( gpGlobals->curtime + pWeaponInfo->GetCycleTime( pGenericWeaponView ) + RandomFloat( m_pProfile->pauseMin, m_pProfile->pauseMax ) );
	}
	else
	{
		// [mlowrance] Do the damage on Despawn and post event
		CCSPlayer *player = ToCSPlayer( GetThrower() );
		if ( player )
		{
			IGameEvent * event = gameeventmanager->CreateEvent( "decoy_detonate" );
			if ( event )
			{
				event->SetInt( "userid", player->GetUserID() );
				event->SetInt( "entityid", this->entindex() );
				event->SetFloat( "x", GetAbsOrigin().x );
				event->SetFloat( "y", GetAbsOrigin().y );
				event->SetFloat( "z", GetAbsOrigin().z );
				gameeventmanager->FireEvent( event );
			}
		}

		BaseClass::Detonate();
		UTIL_Remove( this );
	}
}

void CDecoyProjectile::Spawn( void )
{
	SetModel( GRENADE_MODEL );
 	BaseClass::Spawn();
	
	SetBodygroupPreset( "thrown" );
}

void CDecoyProjectile::Precache( void )
{
	PrecacheModel( GRENADE_MODEL );

	PrecacheScriptSound( "Flashbang.Explode" );
	PrecacheScriptSound( "Flashbang.Bounce" );

	PrecacheParticleSystem( "weapon_decoy_ground_effect_shot" );

	BaseClass::Precache();
}

//TODO: Let physics handle the sound!
void CDecoyProjectile::BounceSound( void )
{
	EmitSound( "Flashbang.Bounce" );
}

#endif // GAME_DLL
