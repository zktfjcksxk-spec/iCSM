//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Handling for the base world item. Most of this was moved from items.cpp.
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "c_items.h"
#include "c_cs_player.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


IMPLEMENT_CLIENTCLASS_DT( C_Item, DT_Item, CItem )
RecvPropBool( RECVINFO( m_bShouldGlow ) ),
END_RECV_TABLE()

// BEGIN_PREDICTION_DATA( C_Item )
// END_PREDICTION_DATA()

//-----------------------------------------------------------------------------
// Constructor 
//-----------------------------------------------------------------------------
C_Item::C_Item() : m_GlowObject( this, Vector( 1.0f, 1.0f, 1.0f ), 0.0f, false, false )
{
}

C_Item::~C_Item()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_Item::Spawn( void )
{

}

void C_Item::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	if ( type == DATA_UPDATE_CREATED )
	{
		if ( m_bShouldGlow )
		{
			UpdateOutlineGlow();
			SetNextClientThink( gpGlobals->curtime );
		}
	}
}

void C_Item::ClientThink( void )
{
	BaseClass::ClientThink();

	UpdateOutlineGlow();
}

void C_Item::UpdateOutlineGlow( void )
{
	if ( !m_bShouldGlow )
		return;

	Vector glowColor;
	bool bRender = false;
	{
		glowColor.x = ( 40.0f / 255.0f );
		glowColor.y = ( 245.0f / 255.0f );
		glowColor.z = ( 40.0f / 255.0f );
		bRender = true;
	}

	// fade the alpha based on distace
	float flAlpha = 0.0f;
	C_CSPlayer *pPlayer = GetLocalOrInEyeCSPlayer();
	if ( pPlayer )
	{
		float flDistance = 0;
		flDistance = ( pPlayer->GetAbsOrigin() - GetAbsOrigin() ).Length();
		flAlpha = clamp( 1.0 - ( flDistance / 512 ), 0.0, 0.9 );
	}

	// Start glowing
	m_GlowObject.SetRenderFlags( bRender, false );
	m_GlowObject.SetColor( glowColor );
	m_GlowObject.SetAlpha( bRender ? flAlpha : 0.0f );

	SetNextClientThink( gpGlobals->curtime + 0.1 );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
IMPLEMENT_CLIENTCLASS_DT( C_ItemAssaultSuitUseable, DT_ItemAssaultSuitUseable, CItemAssaultSuitUseable )
RecvPropInt( RECVINFO( m_nArmorValue ) ),
RecvPropBool( RECVINFO( m_bIsHeavyAssaultSuit ) ),
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_ItemAssaultSuitUseable::Spawn( void )
{

}

wchar_t* C_ItemAssaultSuitUseable::GetReticleHintText( void )
{
	wchar_t wszArmorText[10];
	V_snwprintf( wszArmorText, ARRAYSIZE( wszArmorText ) - 1, L"%d", m_nArmorValue.Get() );
	wszArmorText[ARRAYSIZE( wszArmorText ) - 1] = '\0';

	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	const char *printFormatString = NULL;
	if ( pLocalPlayer && pLocalPlayer->ArmorValue() > 0 )
	{
		if ( m_bIsHeavyAssaultSuit )
			printFormatString = "#SFUIHUD_heavyassaultsuitusable_swap";
		else
			printFormatString = "#SFUIHUD_assaultsuitusable_swap";
	}
	else
	{
		if ( m_bIsHeavyAssaultSuit )
			printFormatString = "#SFUIHUD_heavyassaultsuitusable_take";
		else
			printFormatString = "#SFUIHUD_assaultsuitusable_take";
	}

	g_pVGuiLocalize->ConstructString( m_pReticleHintTextName, sizeof( m_pReticleHintTextName ), g_pVGuiLocalize->Find( printFormatString ), 1, wszArmorText );

	return m_pReticleHintTextName;
}

