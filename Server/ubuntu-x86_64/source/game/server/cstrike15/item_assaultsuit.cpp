//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "cbase.h"
#include "items.h"
#include "cs_player.h"

extern const char *g_pszHeavyPhoenixModel;
extern const char *g_pszHeavyAssaultSuitCT;


class CItemAssaultSuit : public CItem
{
	void Spawn( void )
	{ 
		Precache( );
		CItem::Spawn( );
	}
	
	
	bool MyTouch( CBasePlayer *pBasePlayer )
	{
		CCSPlayer *pPlayer = dynamic_cast< CCSPlayer* >( pBasePlayer );
		if ( !pPlayer )
		{
			Assert( false );
			return false;
		}

		pPlayer->m_bHasHelmet = true;
		pPlayer->SetArmorValue( 100 );
		pPlayer->RecalculateCurrentEquipmentValue();

		if ( pPlayer->IsAlive() )
		{
			CBroadcastRecipientFilter filter;

			if (pPlayer->GetTeamNumber() == TEAM_CT)
			{
				// Play the CT Suit sound
				EmitSound(filter, entindex(), "Player.EquipArmor_CT");

			}
			else
			{
				// Play the T Suit sound
				EmitSound(filter, entindex(), "Player.EquipArmor_T");
			}

		}

		return true;		
	}
};

LINK_ENTITY_TO_CLASS( item_assaultsuit, CItemAssaultSuit );

class CItemHeavyAssaultSuit : public CItemAssaultSuit
{


	bool MyTouch( CBasePlayer *pBasePlayer )
	{
		CCSPlayer *pPlayer = dynamic_cast< CCSPlayer* >( pBasePlayer );
		if ( !pPlayer )
		{
			Assert( false );
			return false;
		}

		pPlayer->m_bHasHelmet = true;
		pPlayer->SetArmorValue( 200 );
		pPlayer->RecalculateCurrentEquipmentValue();
		pPlayer->m_bHasHeavyArmor = true; // m_bHasHeavyAssaultSuit

		if (pPlayer->GetTeamNumber() == TEAM_CT)
		{
			pPlayer->SetModel( g_pszHeavyAssaultSuitCT );
		}
		else
		{
			pPlayer->SetModel( g_pszHeavyPhoenixModel );
		}

		pPlayer->UpdateVoicePitch();
			
		if ( pPlayer->IsAlive() )
		{
			CBroadcastRecipientFilter filter;

			if (pPlayer->GetTeamNumber() == TEAM_CT)
			{
				// Play the CT Suit sound
				pPlayer->EmitSound("Player.EquipArmor_CT");
			}
			else
			{
				// Play the T Suit sound
				pPlayer->EmitSound("Player.EquipArmor_T");
			}
		
		}

		IGameEvent * event = gameeventmanager->CreateEvent( "item_pickup" );
		if ( event )
		{
			event->SetInt( "userid", pPlayer->GetUserID() );
			event->SetString( "item", "heavyassaultsuit" );
			event->SetBool( "silent", false );
			gameeventmanager->FireEvent( event );
		}

		if ( pPlayer->m_nTimeToHeavyAssaultSuit < 0 )
		{
			pPlayer->m_nTimeToHeavyAssaultSuit = gpGlobals->curtime - pPlayer->m_flLifeStartTime;
		}

		pPlayer->EmitSound( "Player.PickupWeapon" );
		return true;
	}
};

LINK_ENTITY_TO_CLASS( item_heavyassaultsuit, CItemHeavyAssaultSuit );



