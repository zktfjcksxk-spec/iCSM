
#include "cbase.h"

#include "item_cash.h"
#include "particle_parse.h"		// for DispatchParticleEffect
#include "weapon_fists.h"

IMPLEMENT_NETWORKCLASS_ALIASED( ItemCash, DT_ItemCash )

#ifdef GAME_DLL
BEGIN_NETWORK_TABLE( CItemCash, DT_ItemCash )
END_NETWORK_TABLE()
#else
BEGIN_NETWORK_TABLE( CItemCash, DT_ItemCash )
END_NETWORK_TABLE()
#endif

LINK_ENTITY_TO_CLASS_ALIASED( item_cash, ItemCash )

#define PARTICLE_CASH_BURST_SINGLE "money_burst_single"
#define PROP_CASH_MODEL "models/props_survival/cash/prop_cash_stack.mdl"
#define CASH_SOUND_PICKUP "Survival.ItemPickup"

#ifdef GAME_DLL

extern ConVar sv_dz_cash_bundle_size;

void CItemCash::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( pActivator->IsPlayer() )
	{
		CBasePlayer *pPlayer = ToBasePlayer( pActivator );
		if ( pPlayer )
		{

			{ // test AABBs
				const CCollisionProperty *pCollisionPlayer = pPlayer->CollisionProp();
				const CCollisionProperty *pCollisionMoney = CollisionProp();
				if ( pCollisionPlayer && pCollisionMoney )
				{
					// extract player AABB
					Vector vecAABBMins, vecAABBMaxs;
					pCollisionPlayer->WorldSpaceAABB( &vecAABBMins, &vecAABBMaxs );
					AABB_t aabbPlayer = AABB_t( vecAABBMins, vecAABBMaxs );

					// extract money AABB
					pCollisionMoney->WorldSpaceAABB( &vecAABBMins, &vecAABBMaxs );
					AABB_t aabbMoney = AABB_t( vecAABBMins, vecAABBMaxs );

					if ( aabbPlayer.Overlaps( aabbMoney ) )
					{
						m_nCashPickupAnimTryType = CASH_PICKUP_ANIM_ALWAYS;
						MyTouch( pPlayer );
						return;
					}
				}
			}

			trace_t tr;
			UTIL_TraceLine( pPlayer->EyePosition(), WorldSpaceCenter(), MASK_SOLID, pPlayer, COLLISION_GROUP_NONE, &tr );

			if ( tr.DidHit() && tr.m_pEnt == this )
			{
				m_nCashPickupAnimTryType = CASH_PICKUP_ANIM_ALWAYS;
				MyTouch( pPlayer );
				return;
			}
		}
	}

	return;
}

bool CItemCash::MyTouch( CBasePlayer* pPlayer )
{
	// Should always be a player
	CCSPlayer* pGetter = dynamic_cast< CCSPlayer* >( pPlayer );
	if ( !pGetter )
		return false;

	if ( gpGlobals->curtime < m_flAllowPickupTime )
		return false;

	pGetter->EmitSound( CASH_SOUND_PICKUP );

	CFmtStr fmtInfoData( "item_cash_%s", m_bufCashOriginalSource );
	pGetter->AddAccount( sv_dz_cash_bundle_size.GetInt(), true, m_bufCashOriginalSource[0] ? m_bufCashOriginalSource : "item_cash" );
	extern ConVar contributionscore_cash_bundle;
	pGetter->AddContributionScore( contributionscore_cash_bundle.GetInt() );

	IGameEvent *event = gameeventmanager->CreateEvent( "dz_item_interaction" );
	if ( event )
	{
		event->SetInt( "userid", pGetter->GetUserID() );
		event->SetInt( "subject", this->entindex() );
		event->SetString( "type", GetClassname() );
		event->SetInt( "priority", pGetter->GetPickupPriority( 1 ) );
		gameeventmanager->FireEvent( event );
	}

	UTIL_Remove( this );

	if ( m_nCashPickupAnimTryType != CASH_PICKUP_ANIM_NEVER )
	{
		CBaseCombatWeapon* pActiveWep = NULL;

		if ( m_nCashPickupAnimTryType == CASH_PICKUP_ANIM_ONLY_IF_FISTS_ACTIVE )
		{
			pActiveWep = pGetter->GetActiveWeapon();
		}
		else // CASH_PICKUP_ANIM_ALWAYS
		{
			pActiveWep = pPlayer->Weapon_OwnsThisType( "weapon_fists" );
		}

		if ( pActiveWep )
		{
			CFists* pFists = dynamic_cast<CFists*>(pActiveWep);
			if ( pFists )
			{
				pFists->PlayUninterruptableActivity( ACT_VM_PICKUP );
			}
		}
	}

	return true;
}

bool CItemCash::ItemCanBeTouchedByPlayer( CBasePlayer *pPlayer )
{
	bool bRet = BaseClass::ItemCanBeTouchedByPlayer( pPlayer );

	if ( !bRet )
	{
		// it's ok to pick up cash through safes

		// Get our test positions
		Vector vecStartPos;
		IPhysicsObject *pPhysObj = VPhysicsGetObject();
		if ( pPhysObj != NULL )
		{
			QAngle vecAngles;
			pPhysObj->GetPosition( &vecStartPos, &vecAngles );
		}
		else
		{
			vecStartPos = CollisionProp()->WorldSpaceCenter();
		}

		Vector vecEndPos = pPlayer->EyePosition();

		trace_t tr;
		CTraceFilterSkipTwoEntities filter( pPlayer, this, COLLISION_GROUP_PLAYER_MOVEMENT );
		UTIL_TraceLine( vecStartPos, vecEndPos, MASK_SOLID, &filter, &tr );

		if ( tr.DidHit() && tr.m_pEnt && tr.m_pEnt->ClassMatches( "func_survival_c4_target" ) )
			return true;
	}

	return bRet;
}

#endif // GAME_DLL

void CItemCash::Spawn()
{
	Precache();

	SetSolid( SOLID_BBOX );
	RemoveEFlags( EFL_USE_PARTITION_WHEN_NOT_SOLID );

	SetModel( PROP_CASH_MODEL );

	BaseClass::Spawn();

#ifdef GAME_DLL
	m_nCashPickupAnimTryType = CASH_PICKUP_ANIM_ONLY_IF_FISTS_ACTIVE;

	m_bEligibleForScreenHighlight = true;

	// small delay after spawn so players get a chance to notice the money before it disappears
	m_flAllowPickupTime = gpGlobals->curtime + 1.f;

	float flMinVel = 100.f;
	float flMaxVel = 400.f;
	switch ( m_nSpawnType )
	{
	case CASH_SPAWN_AT_POS_INSTANT_PICKUP:
		flMinVel = flMaxVel = 0.f;
		m_flAllowPickupTime = 0.f;
		break;
	};

	float flRandomVel = RandomFloat( flMinVel, flMaxVel );
	IPhysicsObject *pPhysicsObject = VPhysicsGetObject();
	if ( pPhysicsObject && flRandomVel > 0.f )
	{
		Vector vecVel = ( RandomVectorInUnitSphere() + Vector( 0, 0, 1 ) ).Normalized() * flRandomVel;
		pPhysicsObject->AddVelocity( &vecVel, NULL );
	}
#endif

	CollisionProp()->UseTriggerBounds( true, 30 );
}

void CItemCash::Precache()
{
	BaseClass::Precache();

	PrecacheModel( PROP_CASH_MODEL );
	PrecacheScriptSound( CASH_SOUND_PICKUP );
	PrecacheParticleSystem( PARTICLE_CASH_BURST_SINGLE );
}

#ifdef CLIENT_DLL
void CItemCash::OnDataChanged( DataUpdateType_t changeType )
{
	BaseClass::OnDataChanged( changeType );
}
#endif

CItemCash::CItemCash()
	: CItem()
{
#ifdef GAME_DLL
	AddSpawnFlags( SF_NORESPAWN );
	V_memset( m_bufCashOriginalSource, 0, sizeof( m_bufCashOriginalSource ) );

	m_nSpawnType = CASH_SPAWN_WITH_RANDOM_VELOCITY;
#endif
}

#ifdef GAME_DLL
void UTIL_SpawnPhysicalCash( Vector vecPosition, int nDollarAmount /* = 0 */, int nNumberOfStacks /* = 0 */, char const *pszOrigin /* = NULL */, CashSpawnType_t spawnType /*= CASH_SPAWN_WITH_RANDOM_VELOCITY*/, CUtlVector< CBaseEntity* > *pOutputCash /*= NULL*/ )
{
	int nNumCashStacksToSpawn = MAX( nNumberOfStacks, nDollarAmount / sv_dz_cash_bundle_size.GetInt() );

	while ( nNumCashStacksToSpawn )
	{
		nNumCashStacksToSpawn--;

		QAngle angRandom = QAngle( RandomFloat( 0, 360 ), RandomFloat( 0, 360 ), RandomFloat( 0, 360 ) );
		CItemCash *pPropCash = dynamic_cast<CItemCash *>( CBaseEntity::CreateNoSpawn( "item_cash", vecPosition + Vector( 0, 0, 5 ), angRandom ) );
		if ( pPropCash )
		{
			pPropCash->SetSpawnType( spawnType );

			if ( pszOrigin )
			{
				V_strcpy_safe( pPropCash->m_bufCashOriginalSource, pszOrigin );
			}

			pPropCash->Spawn();
			DispatchParticleEffect( PARTICLE_CASH_BURST_SINGLE, pPropCash->GetAbsOrigin(), QAngle( 0, 0, 0 ) );

			if ( pOutputCash )
			{
				pOutputCash->AddToTail( pPropCash );
			}

			DispatchSpawn( pPropCash );
		}
	}
}
#endif
