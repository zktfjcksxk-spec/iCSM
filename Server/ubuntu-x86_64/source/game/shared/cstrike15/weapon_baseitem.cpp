//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: base class for belt items, eg pills and adrenaline
//
// $NoKeywords: $
//=====================================================================================//

#include "cbase.h"
#include "weapon_baseitem.h"
#include "cs_gamerules.h"

#if defined( CLIENT_DLL )
#include "c_cs_player.h"

#ifdef INCLUDE_SCALEFORM
#include "HUD/sfweaponselection.h"
#endif	// INCLUDE_SCALEFORM

#include "gameui_interface.h"
#else
#include "cs_player.h"
#endif // CLIENT_DLL

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponBaseItem, DT_WeaponBaseItem )

BEGIN_NETWORK_TABLE( CWeaponBaseItem, DT_WeaponBaseItem )
#ifndef CLIENT_DLL
SendPropBool( SENDINFO( m_bRedraw ) ),
#else
RecvPropBool( RECVINFO( m_bRedraw ) ),
#endif

END_NETWORK_TABLE()

#if defined CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeaponBaseItem )
DEFINE_PRED_FIELD( m_bRedraw, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
END_PREDICTION_DATA()
#endif


#ifndef CLIENT_DLL
BEGIN_DATADESC( CWeaponBaseItem )
DEFINE_FIELD( m_bRedraw, FIELD_BOOLEAN ),
END_DATADESC()
#endif

CWeaponBaseItem::CWeaponBaseItem()
{
	m_bRedraw = false;
}

//--------------------------------------------------------------------------------------------------------
void CWeaponBaseItem::Spawn( void )
{
	m_SequenceCompleteTimer.Invalidate();
	BaseClass::Spawn();
	SetCollisionGroup( COLLISION_GROUP_WEAPON );
}


//--------------------------------------------------------------------------------------------------------
bool CWeaponBaseItem::Deploy( void )
{
	m_bRedraw = false;
	m_SequenceCompleteTimer.Invalidate();
	return BaseClass::Deploy();
}


//--------------------------------------------------------------------------------------------------------
bool CWeaponBaseItem::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	m_bRedraw = false;
	m_SequenceCompleteTimer.Invalidate();
	return BaseClass::Holster( pSwitchingTo );
}

//--------------------------------------------------------------------------------------------------
// bool CWeaponBaseItem::CanExtendHelpingHand( void ) const
// {
// 	return !m_SequenceCompleteTimer.HasStarted() && BaseClass::CanExtendHelpingHand();
// }


//--------------------------------------------------------------------------------------------------------
void CWeaponBaseItem::PrimaryAttack( void )
{
	CCSPlayer *pPlayer = ToCSPlayer( GetPlayerOwner() );
	if (pPlayer == NULL)
		return;

	if ( m_SequenceCompleteTimer.HasStarted() )
	{
		return;
	}

// 	if ( HelpingHandPrimaryAttack() )
// 	{
// 		return;
// 	}

	if ( !CanUseOnSelf( pPlayer ) )
		return;

	SendWeaponAnim( ACT_VM_PRIMARYATTACK );
	pPlayer->DoAnimationEvent( PLAYERANIMEVENT_FIRE_GUN_PRIMARY );

	OnStartUse( pPlayer );

	m_SequenceCompleteTimer.Start( SequenceDuration() );
}

extern ConVar z_use_belt_item_tolerance;

//--------------------------------------------------------------------------------------------------------
void CWeaponBaseItem::SecondaryAttack( void )
{
	CCSPlayer *pPlayer = ToCSPlayer( GetPlayerOwner() );
	if (pPlayer == NULL)
		return;

	if ( m_SequenceCompleteTimer.HasStarted() )
		return;

// 	static const float GiveRange = 256.0f;
// 	CCSPlayer *target = ToCSPlayer( pPlayer->FindUseEntity( GiveRange, 0.0f, z_use_belt_item_tolerance.GetFloat(), NULL, true ) ); // Prefer to hit players
// 	if ( target && target->IsOnASurvivorTeam() && !target->IsIncapacitated() )
// 	{
// #ifdef GAME_DLL
// 		pPlayer->GiveActiveWeapon( target );
// #endif
// 		return;
// 	}

	BaseClass::SecondaryAttack();
}


//--------------------------------------------------------------------------------------------------------
/**
* Called when no buttons are pressed
*/
void CWeaponBaseItem::WeaponIdle( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponBaseItem::Reload()
{
	if ( ( m_bRedraw ) && ( m_flNextPrimaryAttack <= gpGlobals->curtime ) && ( m_flNextSecondaryAttack <= gpGlobals->curtime ) )
	{
		//Redraw the weapon
		SendWeaponAnim( ACT_VM_DRAW );

		//Update our times
		m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
		m_flNextSecondaryAttack = gpGlobals->curtime + SequenceDuration();

		SetWeaponIdleTime( gpGlobals->curtime + SequenceDuration() );

		//Mark this as done
		//	m_bRedraw = false;
	}

	return true;
}

//--------------------------------------------------------------------------------------------------------
/**
* Called each frame by the player PostThink
*/
void CWeaponBaseItem::ItemPostFrame( void )
{
	CCSPlayer *pPlayer = ToCSPlayer( GetPlayerOwner() );
	if ( !pPlayer || !pPlayer->IsAlive() )
	{
		m_SequenceCompleteTimer.Invalidate();
		return;
	}


	BaseClass::ItemPostFrame();
	
	if ( m_SequenceCompleteTimer.HasStarted() && m_SequenceCompleteTimer.IsElapsed() )
	{
		m_SequenceCompleteTimer.Invalidate();

		if ( pPlayer->GetAmmoCount(m_iPrimaryAmmoType) <= 0 )
		{
			pPlayer->Weapon_Drop( this, NULL, NULL );
#ifndef CLIENT_DLL	
			UTIL_Remove( this );
#endif
		}
		else
		{
			Deploy();
		}

#if defined (CLIENT_DLL)
#ifdef INCLUDE_SCALEFORM
		if ( !GameUI().IsPanoramaEnabled() )
		{
			// when an item is removed, force the local player to update their inventory screen
			C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
			if ( pLocalPlayer && pLocalPlayer == pPlayer )
			{
				SFWeaponSelection *pHudWS = GET_HUDELEMENT( SFWeaponSelection );
				if ( pHudWS )
				{
					int nAmmoCount = pPlayer->GetAmmoCount( m_iPrimaryAmmoType );
					if ( nAmmoCount <= 0 )
					{
						pHudWS->ShowAndUpdateSelection( WEPSELECT_DROP, this );
					}
					else
					{
						// we need to tell the hud that this weapon still exists and then update the selected weapon
						pHudWS->ShowAndUpdateSelection( WEPSELECT_PICKUP, this );
					}
				}
			}
		}
#endif	// INCLUDE_SCALEFORM
#endif
	}

}

// //--------------------------------------------------------------------------------------------------------
// bool CWeaponBaseItem::OnHit( trace_t &trace, const Vector &swingVector, bool firstTime ) 
// {
// 	if ( trace.m_pEnt && trace.m_pEnt->IsPlayer() && IsASurvivorTeam( trace.m_pEnt->GetTeamNumber() ) )
// 		return false;	// don't hit survivors who are outside of heal range if we're trying to get close and heal them.
// 
// 	return BaseClass::OnHit( trace, swingVector, firstTime );
// }

//--------------------------------------------------------------------------------------------------------
bool CWeaponBaseItem::SendWeaponAnim( int iActivity )
{
	//iActivity = TranslateViewmodelActivity( (Activity)iActivity );
	return BaseClass::SendWeaponAnim( iActivity );
}
#ifndef CLIENT_DLL
void CWeaponBaseItem::Operator_HandleAnimEvent( animevent_t * pEvent, CBaseCombatCharacter * pOperator )
{
	int nEvent = pEvent->Event();
	if ( (pEvent->type & AE_TYPE_NEWEVENTSYSTEM) && (pEvent->type & AE_TYPE_SERVER) )
	{

		if ( nEvent == AE_WPN_HEALTHSHOT_INJECT )
		{
			// apply the effect
			CCSPlayer *pPlayer = ToCSPlayer( GetPlayerOwner() );
			if ( pPlayer && pPlayer->IsAlive() && pPlayer->GetAmmoCount( m_iPrimaryAmmoType ) > 0 )
			{
				// remove the ammo
				pPlayer->RemoveAmmo( 1, m_iPrimaryAmmoType );
	
				CompleteUse( pPlayer );

				m_bRedraw = true;
			}
		}

	}

	BaseClass::Operator_HandleAnimEvent( pEvent, pOperator );
}
#endif

//--------------------------------------------------------------------------------------------------------
bool CWeaponBaseItem::CanFidget( void )
{
	return false;
}


//--------------------------------------------------------------------------------------------------------
