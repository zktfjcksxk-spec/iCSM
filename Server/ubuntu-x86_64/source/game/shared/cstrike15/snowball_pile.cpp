//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "snowball_pile.h"
#include "cs_weapon_parse.h"
#include "particle_parse.h"		// for DispatchParticleEffect

#ifndef CLIENT_DLL
#include "weapon_csbase.h"
#include "cs_player.h"
#endif

#define SNOWPILE_MODEL "models/props_holidays/snowball/snowball_pile.mdl"
#define SNOWBALL_PILE_EFFECT "snowball_pile"//"weapon_confetti_cheap"//"snowball_pile"

IMPLEMENT_NETWORKCLASS_ALIASED( SnowballPile, DT_SnowballPile )

LINK_ENTITY_TO_CLASS_ALIASED( ent_snowball_pile, SnowballPile );
PRECACHE_REGISTER( ent_snowball_pile )

BEGIN_NETWORK_TABLE( CSnowballPile, DT_SnowballPile )
#if !defined( CLIENT_DLL )
#else
#endif
END_NETWORK_TABLE()

#if defined( CLIENT_DLL )
C_SnowballPile::~C_SnowballPile()
{
	// Shut down our effect if we have it
	if ( m_baseParticleEffect )
	{
		m_baseParticleEffect->StopEmission( false, false, true );
		m_baseParticleEffect = NULL;
	}
}

//-----------------------------------------------------------------------------
// On data update
//-----------------------------------------------------------------------------
void C_SnowballPile::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( !m_baseParticleEffect || !m_baseParticleEffect.IsValid() )
	{
		m_baseParticleEffect = ParticleProp()->Create( SNOWBALL_PILE_EFFECT, PATTACH_ABSORIGIN_FOLLOW );
	}

	//if ( updateType == DATA_UPDATE_CREATED )
	//{
	//	m_baseParticleEffect = ParticleProp()->Create( SNOWBALL_PILE_EFFECT, PATTACH_ABSORIGIN_FOLLOW );
	//}
}

#else

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CSnowballPile )
DEFINE_USEFUNC( SnowPileUse ),
END_DATADESC()

CSnowballPile::CSnowballPile()
{
}

void CSnowballPile::Spawn()
{
	Precache();

	SetModel( SNOWPILE_MODEL );

	SetUse( &CSnowballPile::SnowPileUse );

	//SetSolid( SOLID_BBOX );
	SetSolid( SOLID_VPHYSICS );
	SetMoveType( MOVETYPE_NONE );
	SetCollisionGroup( COLLISION_GROUP_INTERACTIVE_DEBRIS );

	AddFlag( FL_OBJECT );

	Vector min = Vector( -10, -10, 0 );
	Vector max = Vector( 10, 10, 32 );
	SetSize( min, max );
	if ( CollisionProp() )
		CollisionProp()->SetCollisionBounds( min, max );

	m_takedamage = DAMAGE_NO;

	BaseClass::Spawn();
}

void CSnowballPile::Precache()
{
	PrecacheModel( SNOWPILE_MODEL );

	PrecacheParticleSystem( SNOWBALL_PILE_EFFECT );

	BaseClass::Precache();
}

void CSnowballPile::EquipPlayer( CBaseEntity *pEntity, const char *szWeapon )
{
	if ( !pEntity )
		return;

	CBasePlayer *pPlayer = NULL;

	if ( pEntity->IsPlayer() )
	{
		pPlayer = ( CBasePlayer * )pEntity;
	}

	if ( !pPlayer )
		return;

	const char *weaponName = szWeapon;

	const CEconItemDefinition *pDef = GetItemSchema()->GetItemDefinitionByName( weaponName );
	CSWeaponID weaponID = GetWeaponIDFromItem( pDef );
	if ( weaponID != WEAPON_NONE )
	{
		CCSPlayer *pCSPlayer = ToCSPlayer( pPlayer );
		const CEconItemView* pGenericWeaponView = CSInventoryManager()->GetReferenceEconItem( pDef->GetDefinitionIndex(), 0 );
		if ( pCSPlayer && IsGrenadeWeapon( pGenericWeaponView ) )
		{
			// if it's a grenade and we could pick it up, give it
			AcquireResult::Type acquireResult = pCSPlayer->CanAcquire( pGenericWeaponView, AcquireMethod::PickUp );
			if ( acquireResult == AcquireResult::Allowed )
				pCSPlayer->GiveNamedItem( "weapon_snowball" );
		}
	}
}


void CSnowballPile::SnowPileUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	EquipPlayer( pActivator, "weapon_snowball" ); // note: pActivator may sometimes be NULL
}

#endif