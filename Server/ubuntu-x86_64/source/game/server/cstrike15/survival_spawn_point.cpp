//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//
#include "cbase.h"
#include "survival_spawn_point.h"
#include "prop_counter.h"
#include "cs_player.h"

BEGIN_DATADESC( CPointDZWeaponSpawn )
DEFINE_KEYFIELD( m_nGroupID,		FIELD_INTEGER,	"groupID" ),
DEFINE_KEYFIELD( m_flDefaultWeight,		FIELD_FLOAT,	"default_weight" ),
DEFINE_KEYFIELD( m_iszDoorName,			FIELD_STRING,	"door_name" ),
END_DATADESC()
IMPLEMENT_AUTO_LIST( IPointDZWeaponSpawn )
LINK_ENTITY_TO_CLASS( point_dz_weaponspawn, CPointDZWeaponSpawn );
#ifdef DEVELOPMENT_ONLY
LINK_ENTITY_TO_CLASS( point_br_weaponspawn, CPointDZWeaponSpawn );
#endif

CPointDZWeaponSpawn::CPointDZWeaponSpawn()
{
	m_nGroupID = 0;

	m_flDefaultWeight = 100.f;
	m_flCurrentWeight = m_flDefaultWeight;

	m_nPrice = 0;
}


void CPointDZWeaponSpawn::Spawn()
{
	BaseClass::Spawn();

	// make sure current weight is the same as default
	Reset();
}

void CPointDZWeaponSpawn::AssignItem( CBaseEntity *pEnt, int nPrice )
{
	Assert( !HasItem() );
	Assert( m_nPrice == 0 );
	m_hItem = pEnt;
	m_nPrice = nPrice;
}

void CPointDZWeaponSpawn::Reset()
{
	m_flCurrentWeight = m_flDefaultWeight;
	m_hDoor = NULL;
}


int CPointDZWeaponSpawn::DrawDebugTextOverlays(void)
{
	int offset = BaseClass::DrawDebugTextOverlays();
	
	if ( m_debugOverlays & OVERLAY_TEXT_BIT ) 
	{
		char tempstr[512];
		V_snprintf( tempstr, sizeof(tempstr), "Door Name: %s", STRING( m_iszDoorName ) );
		EntityText( offset, tempstr, 0 );
		offset++;
	}

	return offset;
}


BEGIN_DATADESC( CPointDZWeaponSpawnGroup )
DEFINE_KEYFIELD( m_flRadius, FIELD_FLOAT, "radius" ),
END_DATADESC()
IMPLEMENT_AUTO_LIST( IPointDZWeaponSpawnGroup )
LINK_ENTITY_TO_CLASS( point_dz_weaponspawn_group, CPointDZWeaponSpawnGroup );
#ifdef DEVELOPMENT_ONLY
LINK_ENTITY_TO_CLASS( point_br_weaponspawn_group, CPointDZWeaponSpawnGroup );
#endif

void CPointDZWeaponSpawnGroup::AddSpawnPoint( CPointDZWeaponSpawn *pPoint )
{
	Assert( m_vecSpawnPoints.Find( pPoint ) == m_vecSpawnPoints.InvalidIndex() );
	m_vecSpawnPoints.AddToTail( pPoint );
}


BEGIN_DATADESC( CPointDZDroneGunSpawn )
DEFINE_KEYFIELD( m_bSpawnAutomatically, FIELD_BOOLEAN, "spawnAutomatically" ),
DEFINE_INPUTFUNC( FIELD_VOID, "spawn", SpawnDroneGun ),
END_DATADESC()
IMPLEMENT_AUTO_LIST( IPointDZDroneGunSpawn )
LINK_ENTITY_TO_CLASS( point_dz_dronegun, CPointDZDroneGunSpawn );
#ifdef DEVELOPMENT_ONLY
LINK_ENTITY_TO_CLASS( point_br_dronegun, CPointDZDroneGunSpawn );
#endif

CPointDZDroneGunSpawn::CPointDZDroneGunSpawn()
{
	m_hSpawnedDroneGun = INVALID_EHANDLE;
}

void CPointDZDroneGunSpawn::SpawnDroneGun( bool bForce /* = false */ )
{
	if ( m_hSpawnedDroneGun.Get() )
		return;

	if ( m_bSpawnAutomatically == bForce )
		return;

	CBaseEntity *pEntDroneGun = CBaseEntity::CreateNoSpawn( "dronegun", GetAbsOrigin(), vec3_angle, NULL );
	Assert( pEntDroneGun );

	if ( pEntDroneGun )
	{
		DispatchSpawn( pEntDroneGun );
		m_hSpawnedDroneGun = pEntDroneGun;
	}
}


BEGIN_DATADESC( CPointDZParachuteSpawn )
END_DATADESC()
IMPLEMENT_AUTO_LIST( IPointDZParachuteSpawn )
LINK_ENTITY_TO_CLASS( point_dz_parachute, CPointDZParachuteSpawn );
#ifdef DEVELOPMENT_ONLY
LINK_ENTITY_TO_CLASS( point_br_parachute, CPointDZParachuteSpawn );
#endif

BEGIN_DATADESC( CDZDoor )
	DEFINE_KEYFIELD( m_bIsSecurityDoor, FIELD_BOOLEAN, "is_security_door" ),
	DEFINE_THINKFUNC( CDZDoor::SecurityDoorSoundThink ),
END_DATADESC()
IMPLEMENT_AUTO_LIST( IDZDoor )
LINK_ENTITY_TO_CLASS( dz_door, CDZDoor );
#ifdef DEVELOPMENT_ONLY
LINK_ENTITY_TO_CLASS( br_door, CDZDoor );
#endif

CDZDoor::CDZDoor() :
	m_bIsSecurityDoor( false ),
	m_bPaidToUnlock( false ),
	m_nPlayDoorOpenSound( 0 )
{
	m_nAttachmentIndex1 = -1;
	m_nAttachmentIndex2 = -1;
}

void CDZDoor::Precache()
{
	BaseClass::Precache();

	PrecacheScriptSound( "Survival.SecurityDoorOpen" );

	PrecacheScriptSound( "Survival.SecurityDoorPaymentStart" );
	PrecacheScriptSound( "Survival.SecurityDoorPaymentCounter" );

}

void CDZDoor::Spawn()
{
	BaseClass::Spawn();

	if ( m_bIsSecurityDoor )
	{
		m_nAttachmentIndex1 = LookupAttachment( "frame1" );
		m_nAttachmentIndex2 = LookupAttachment( "frame2" );
		if ( m_nAttachmentIndex1 <= 0 || m_nAttachmentIndex2 <= 0 )
		{
			DevWarning( "%s missing frame attachments!\n", GetEntityNameAsCStr() );
			Assert( false );
			return;
		}
	}

}

CPropCounter *CDZDoor::GetPropCounter()
{
	// find prop_counter
	CPropCounter *pPropCounter = NULL;

	CBaseEntity *pChild = FirstMoveChild();
	while ( pChild )
	{
		if ( FClassnameIs( pChild, "prop_counter" ) )
		{
			pPropCounter = static_cast<CPropCounter*>(pChild);
			break;
		}

		pChild = pChild->NextMovePeer();
	}

	if ( !pPropCounter )
	{
		CBaseEntity *pEnt = NULL;
		pEnt = gEntList.FindEntityByClassnameWithin( pEnt, "prop_counter", GetAbsOrigin(), 256.0f );
		if ( pEnt )
			pPropCounter = static_cast< CPropCounter* >(pEnt);
	}

	return pPropCounter;
}

void CDZDoor::SetupDoor()
{
	if ( !m_bIsSecurityDoor )
		return;

	int nPrice = 0;
	bool bHasItem = false;
	if ( m_hSpawnPoint )
	{
		nPrice = m_hSpawnPoint->GetPrice();
		bHasItem = m_hSpawnPoint->HasItem();
	}
	else
	{
		DevWarning( "%s missing a spawn point at [%.2f %.2f %.2f]\n", GetEntityNameAsCStr(), XYZ( GetAbsOrigin() ) );
	}

	CPropCounter *pPropCounter = GetPropCounter();
	if ( pPropCounter )
	{
			pPropCounter->SetDisplayValue( nPrice );
	}
	else
	{
		Assert( !"Missing prop_counter" );
	}

	if ( pPropCounter && nPrice <= 0 )
	{
		UTIL_Remove( pPropCounter ); // delete $0 counters
	}

	// leave the door open if we don't have anything behind it
	if ( !bHasItem )
	{
		DoorOpen( NULL );
	}
	else if ( nPrice > 0 )
	{
		m_prolongeduse.ConfigSetPlayerBlockingUseAction( k_CSPlayerBlockingUseAction_PayingToOpenDoor );
		m_prolongeduse.ConfigSetUseDurationToCompletion( 2.5f ); // consistent use time (was: at least 1 second plus 1 additional second per each 1,000 -- [[[ 1.5f + float( nPrice )/1000.0f ]]] )
		m_prolongeduse.ConfigSetSingleUse( false ); // maybe by the time use bar timer elapses player doesn't have money? allow them to retry or other player to retry.
		AddFlag( FL_OBJECT ); // have this entity participate in high-priority entity pickup rules
	}
}

void CDZDoor::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( m_bIsSecurityDoor )
	{
		CCSPlayer* player = pActivator ? ToCSPlayer( pActivator ) : NULL;
		if ( !player )
			return;

		if ( m_prolongeduse.BIsConfigured() && CanOpenSecurityDoor( player ) )
		{
			switch ( m_prolongeduse.ETryToBeginUse( this, player ) )
			{
			case CEntitySupportForProlongedUse_t::k_EBeginUseResult_BeingUsedByOther:
				ClientPrint( player, HUD_PRINTCENTER, "#SFUIHUD_InfoPanel_OpeningSecurityDoor_Other" );
				return;
			case CEntitySupportForProlongedUse_t::k_EBeginUseResult_Started:
				SetThink( &CDZDoor::ProlongedUseThink );
				SetNextThink( gpGlobals->curtime );

				EmitSound( "Survival.SecurityDoorPaymentStart" );

				return;
			default:
				return;
			}
		}
		else
		{
			OpenSecurityDoor( ToCSPlayer( pActivator ) );
		}
	}
	else
	{
		BaseClass::Use( pActivator, pCaller, useType, value );
	}
}

void CDZDoor::ProlongedUseThink()
{
	CCSPlayer *pActivator = NULL;
	switch ( m_prolongeduse.ESupportUseThink( this, &pActivator ) )
	{
	case CEntitySupportForProlongedUse_t::k_EUseInProgressOutcome_NotInUse:
		SetThink( NULL );
		return;

	default:
	case CEntitySupportForProlongedUse_t::k_EUseInProgressOutcome_StillUsing:
		SetNextThink( gpGlobals->curtime + 0.1f );
		return;

	case CEntitySupportForProlongedUse_t::k_EUseInProgressOutcome_Aborted:
		StopSound("Survival.SecurityDoorPaymentStart");

		SetThink( NULL );
		return;

	case CEntitySupportForProlongedUse_t::k_EUseInProgressOutcome_Completed:
		StopSound("Survival.SecurityDoorPaymentStart");

		SetThink( NULL );
		OpenSecurityDoor( pActivator );
		return;
	}
}

bool CDZDoor::CanOpenSecurityDoor( CCSPlayer *pCSPlayer, bool* pCannotAfford /*=nullptr*/ )
{

	if ( pCannotAfford != nullptr )
	{
		*pCannotAfford = false;
	}

	if ( !m_bIsSecurityDoor )
		return false;

	if ( IsDoorOpen() )
		return false;

	if ( !pCSPlayer )
		return false;

	if ( m_bPaidToUnlock )
		return false;

	int nPrice = m_hSpawnPoint ? m_hSpawnPoint->GetPrice() : 0;

	if ( pCSPlayer->GetAccountBalance() < nPrice )
	{
		if(pCannotAfford != nullptr)
		{
			*pCannotAfford = true;
		}

		return false;
	}

	return true;
}

void CDZDoor::OpenSecurityDoor( CCSPlayer *pCSPlayer )
{
	bool bCannotAfford = false;
	if ( !CanOpenSecurityDoor( pCSPlayer, &bCannotAfford ) )
	{
		if ( bCannotAfford )
		{
			EmitSound("Survival.BuyItemFailed");
		}

		return;
	}

	int nPrice = m_hSpawnPoint ? m_hSpawnPoint->GetPrice() : 0;

	if ( pCSPlayer->GetAccountBalance() < nPrice )
	{
		EmitSound( "Survival.BuyItemFailed" );
		return;
	}
	else
	{
		m_bPaidToUnlock = true;

		float fDelayTime = 1.0f;

		if ( nPrice > 0 ) {
			EmitSound( "Survival.SecurityDoorPaymentCounter" );
		} else {
			//Skip straight to opening the door when we don't need to pay.
			m_nPlayDoorOpenSound = 1;
			fDelayTime = 0.01f;
		}

		// find prop_counter and set price to 0
		CPropCounter *pPropCounter = GetPropCounter();
		if ( pPropCounter )
		{
			pPropCounter->SetDisplayValue( 0 );
		}

		const char *pszItemName = "nothing";
		if ( m_hSpawnPoint && m_hSpawnPoint->HasItem() )
		{
			pszItemName = m_hSpawnPoint->GetItem()->GetClassname();
		}

		IGameEvent *event = gameeventmanager->CreateEvent( "dz_item_interaction" );
		if ( event )
		{
			event->SetInt( "userid", pCSPlayer->GetUserID() );
			event->SetInt( "subject", this->entindex() );
			event->SetString( "type", GetClassname() );
			event->SetInt( "priority", pCSPlayer->GetPickupPriority( 1 + ( nPrice / 750 ) ) );
			gameeventmanager->FireEvent( event );
		}

		pCSPlayer->AddAccount( -nPrice, true, CFmtStr( "%s to get %s inside", GetClassname(), pszItemName ) );

		SetContextThink( &CDZDoor::SecurityDoorSoundThink, gpGlobals->curtime + fDelayTime, "DOOR_SOUND_THINK" );
	}
}
	
void CDZDoor::SecurityDoorSoundThink()
{
	m_nPlayDoorOpenSound++;

	if ( m_nPlayDoorOpenSound == 2 )
	{
		StopSound( "Survival.SecurityDoorPaymentCounter" );

		EmitSound( "Survival.SecurityDoorOpen" );
	}

	// After 3 seconds events have processed and the door opens.
	if ( m_nPlayDoorOpenSound == 3 )
	{
		DoorOpen( NULL );
	}
	else
	{
		SetContextThink( &CDZDoor::SecurityDoorSoundThink, gpGlobals->curtime + 1.0f, "DOOR_SOUND_THINK" );
	}
}


bool GetClosestItemSpawnPositionTo( const Vector &vecPosition, Vector *vecSpawnPosFound, QAngle *angSpawnAngleFound )
{
	float flClosest = FLT_MAX;
	int nClosestIndex = -1;
	FOR_EACH_VEC( IPointDZWeaponSpawn::AutoList(), n )
	{
		float flContenderDist = IPointDZWeaponSpawn::AutoList()[n]->GetEntity()->GetAbsOrigin().DistToSqr( vecPosition );
		if ( flContenderDist < flClosest )
		{
			flClosest = flContenderDist;
			nClosestIndex = n;
		}
	}

	Assert( IPointDZWeaponSpawn::AutoList().IsValidIndex( nClosestIndex ) );
	if ( IPointDZWeaponSpawn::AutoList().IsValidIndex( nClosestIndex ) )
	{
		if ( vecSpawnPosFound )
			(*vecSpawnPosFound) = IPointDZWeaponSpawn::AutoList()[nClosestIndex]->GetEntity()->GetAbsOrigin();
		if ( angSpawnAngleFound )
			(*angSpawnAngleFound) = IPointDZWeaponSpawn::AutoList()[nClosestIndex]->GetEntity()->GetAbsAngles();
		return true;
	}

	if ( vecSpawnPosFound )
		(*vecSpawnPosFound) = vecPosition;
	if ( angSpawnAngleFound )
		(*angSpawnAngleFound) = vec3_angle;

	return false;
}
