//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: Healthshot Item, belt item
//
// $NoKeywords: $
//=====================================================================================//

#include "cbase.h"
#include "item_healthshot.h"
#include "cs_gamerules.h"
#include "econ_entity_creation.h"

#if defined( CLIENT_DLL )
#include "c_cs_player.h"
#else
#include "cs_player.h"
#endif // CLIENT_DLL

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

IMPLEMENT_NETWORKCLASS_ALIASED( Item_Healthshot, DT_Item_Healthshot )

BEGIN_NETWORK_TABLE( CItem_Healthshot, DT_Item_Healthshot )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CItem_Healthshot )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS_ALIASED( weapon_healthshot, Item_Healthshot );
PRECACHE_REGISTER( weapon_healthshot );

// #ifndef CLIENT_DLL
// BEGIN_DATADESC( CItem_Healthshot )
// END_DATADESC()
// #endif

ConVar healthshot_health( "healthshot_health", "50", FCVAR_REPLICATED | FCVAR_RELEASE );
ConVar healthshot_healthboost_time( "healthshot_healthboost_time", "0", FCVAR_REPLICATED | FCVAR_RELEASE );
ConVar healthshot_healthboost_speed_multiplier( "healthshot_healthboost_speed_multiplier", "1", FCVAR_REPLICATED | FCVAR_RELEASE );
ConVar healthshot_allow_use_at_full( "healthshot_allow_use_at_full", "1", FCVAR_REPLICATED | FCVAR_RELEASE );

void CItem_Healthshot::WeaponIdle()
{
	if (m_flTimeWeaponIdle > gpGlobals->curtime)
		return;
	
	SendWeaponAnim( ACT_VM_IDLE );

	SetWeaponIdleTime( gpGlobals->curtime + 20 );
}

void CItem_Healthshot::Precache( void )
{
	//PrecacheScriptSound( "Healthshot.Shot" );

	BaseClass::Precache();
}

bool CItem_Healthshot::CanUseOnSelf( CCSPlayer *pPlayer )	
{ 
	if ( !pPlayer )
		return false;

	if ( !healthshot_allow_use_at_full.GetBool() && pPlayer->GetHealth() >= pPlayer->GetMaxHealth() )
	{
#ifndef CLIENT_DLL
		ClientPrint( pPlayer, HUD_PRINTCENTER, "#SFUI_Healthshot_AlreadyAtMax" );
#endif
		return false;
	}

	return true;
}

void CItem_Healthshot::DropHealthshot( void )
{
#ifndef CLIENT_DLL
	CCSPlayer *pPlayer = ToCSPlayer( GetPlayerOwner() );
	if ( !pPlayer )
		return;

	int iAmount = pPlayer->GetAmmoCount( GetPrimaryAmmoType() );
	if ( iAmount <= 0 )
	{
		return; // don't drop, item will self-destruct almost immediately anyway (we're in healthshot post-stab)
	}
	else if ( iAmount == 1 )
	{
		pPlayer->CSWeaponDrop( this, false, true );
		return;
	}
	else	
	{
		pPlayer->RemoveAmmo( 1, AMMO_TYPE_HEALTHSHOT );

#if !defined( NO_STEAM ) && !defined( NO_STEAM_GAMECOORDINATOR )
		CBaseEntity *pEnt = NULL;
		CItemSelectionCriteria criteria;
		criteria.SetQuality( AE_NORMAL );
		criteria.BAddCondition( "name", k_EOperator_String_EQ, "weapon_healthshot", true );
		pEnt = ItemGeneration()->GenerateRandomItem( &criteria, pPlayer->EyePosition(), vec3_angle );

		CItem_Healthshot *pHealth = dynamic_cast<CItem_Healthshot*>(pEnt);
		if ( pHealth )
		{
			Vector vecWeaponThrowFromPos = pPlayer->EyePosition();
			QAngle angWeaponThrowFromAngle = pPlayer->EyeAngles();

 			Vector vForward;
 			AngleVectors(angWeaponThrowFromAngle, &vForward, NULL, NULL);
 			vecWeaponThrowFromPos = vecWeaponThrowFromPos + (vForward * 100);
			//NDebugOverlay::Box( vecWeaponThrowFromPos, Vector( 10, 10, 10 ), Vector( -10, -10, -10 ), 255, 0, 0, 200, 3 );

			DispatchSpawn( pHealth );

			// set it non-solid because it hits itself during the trace when trying to throw it
			pHealth->SetSolidFlags( FSOLID_NOT_SOLID );
			pHealth->SetMoveCollide( MOVECOLLIDE_FLY_BOUNCE );

			pPlayer->Weapon_Drop( pHealth, &vecWeaponThrowFromPos, NULL );			

			pHealth->SetSolidFlags( FSOLID_NOT_STANDABLE | FSOLID_TRIGGER | FSOLID_USE_TRIGGER_BOUNDS );
			pHealth->SetPreviousOwner( pPlayer );
			pHealth->m_flDroppedAtTime = gpGlobals->curtime;
			pHealth->AddPriorOwner( pPlayer );
		}
#endif
	}


#endif
}

//--------------------------------------------------------------------------------------------------------
void CItem_Healthshot::OnStartUse( CCSPlayer *pPlayer )
{
	SetWeaponIdleTime( gpGlobals->curtime + 20 ); // don't fidget for a bit
	BaseClass::OnStartUse( pPlayer );
}

#ifndef CLIENT_DLL
//--------------------------------------------------------------------------------------------------------
extern ConVar sv_health_approach_enabled;
void CItem_Healthshot::CompleteUse( CCSPlayer *pPlayer )
{
	pPlayer->OnHealthshotUsed();

	// Give health
	if ( sv_health_approach_enabled.GetBool() )
	{
		pPlayer->AddHealthApproach( healthshot_health.GetInt() );
	}
	else
	{
		pPlayer->SetHealth( Min( pPlayer->GetHealth() + healthshot_health.GetInt(), pPlayer->GetMaxHealth() ) );
	}

	// add boost
	if ( healthshot_healthboost_time.GetFloat() > 0 )
	{
		pPlayer->SetCurrentHealthShotBoostAmount( healthshot_healthboost_time.GetFloat() );
	}

	// emit event	
// 	IGameEvent *event = gameeventmanager->CreateEvent( "healthshot_used" );
// 	if( event )
// 	{
// 		int userID = pPlayer->GetUserID();
// 		event->SetInt( "userid", userID );
// 		event->SetInt( "subject", userID );
// 		gameeventmanager->FireEvent( event );
// 	}

	BaseClass::CompleteUse( pPlayer );
}
#endif	//CLIENT_DLL
