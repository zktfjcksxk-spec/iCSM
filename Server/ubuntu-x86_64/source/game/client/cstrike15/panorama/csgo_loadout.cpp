//========= Copyright (C) Valve Corporation, All rights reserved. ============//
//
// Menu to change player loadout
//
//=============================================================================//

#include "cbase.h"

#include "csgo_loadout.h"

#include "weapon_selection.h"
#include "econ/econ_item_description.h"
#include "uicomponents/uicomponent_inventory.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

DEFINE_PANORAMA_EVENT( ShowLoadout );

DECLARE_PANORAMA_EVENT2( Loadout_FilterForPosition, int, int )
DEFINE_PANORAMA_EVENT( Loadout_FilterForPosition )

REGISTER_PANEL2D_FACTORY( CCSGO_Loadout, CSGOLoadout )

using namespace panorama;

CCSGO_Loadout::CCSGO_Loadout( CPanel2D *pParent, const char *pchID )
	: CPanel2D( pParent, pchID )
{
	SetInputNamespace( "loadout" );

	SetAcceptsInput( true );
	SetAcceptsFocus( true );

	BLoadLayout( "file://{resources}/layout/loadout.xml" );
	m_pTeamLogo = RequireChildInLayoutFile( "TeamLogo" );
	m_pItemWheel = RequireChildInLayoutFile( "ItemWheel" );
	m_pItemList = RequireChildInLayoutFile( "LoadoutItemList" );
	m_pItemWheel->IterateChildren( [&]( CPanel2D *pPanel ) -> bool
	{
		CPanel2D* pContents = pPanel->RequireChild( "Contents" );
		pContents->RequireLoadLayoutSnippet( "ItemWedge" );
		return true;
	} );

	UpdatePanelsForLoadout( TEAM_CT, LOADOUT_POSITION_SECONDARY0, LOADOUT_POSITION_SECONDARY4 );
	m_pItemWheel->Enable();

	RegisterForUnhandledEvent( PanoramaComponent_Inventory_PlayerEquipSlotChanged(), this, &CCSGO_Loadout::EventOnLoadoutChanged );
	RegisterForUnhandledEvent( ShowLoadout(), this, &CCSGO_Loadout::EventShowLoadout );
	RegisterEventHandler( Loadout_FilterForPosition(), this, &CCSGO_Loadout::EventFilterForPosition );
}

bool GetLoadoutPositionRangeForSlot( const char* szSlot, loadout_positions_t& nOutStart, loadout_positions_t& nOutEnd )
{
	if ( StringHasPrefix( szSlot, "secondary" ) )
	{
		nOutStart = LOADOUT_POSITION_SECONDARY0;
		nOutEnd = LOADOUT_POSITION_SECONDARY4;
		return true;
	}
	else if ( StringHasPrefix( szSlot, "smg" ) )
	{
		nOutStart = LOADOUT_POSITION_SMG0;
		nOutEnd = LOADOUT_POSITION_SMG4;
		return true;
	}
	else if ( StringHasPrefix( szSlot, "rifle" ) )
	{
		nOutStart = LOADOUT_POSITION_RIFLE0;
		nOutEnd = LOADOUT_POSITION_RIFLE5;
		return true;
	}
	else if ( StringHasPrefix( szSlot, "heavy" ) )
	{
		nOutStart = LOADOUT_POSITION_HEAVY0;
		nOutEnd = LOADOUT_POSITION_HEAVY4;
		return true;
	}

	return false;
}

bool CCSGO_Loadout::EventOnLoadoutChanged( int nPosition, const char* szSlotName, itemid_t oldItem, itemid_t newItem )
{
	loadout_positions_t nStart, nEnd;
	if ( GetLoadoutPositionRangeForSlot( g_szLoadoutStrings[nPosition], nStart, nEnd ) )
	{
		int nTeam = GetAttribute( "team", TEAM_CT );
		UpdatePanelsForLoadout( nTeam, nStart, nEnd );
		EventFilterForPosition( nTeam, nPosition );
	}
	return true;
}

extern const char *g_szLoadoutStringsSubPositions[];
int GetLoadoutPositionForString( const char* szLoadoutPosition )
{
	for ( int i = 0; i < LOADOUT_POSITION_COUNT; ++i )
	{
		if ( FStrEq( szLoadoutPosition, g_szLoadoutStringsSubPositions[ i ] ) ||
			 FStrEq( szLoadoutPosition, g_szLoadoutStrings[ i ] ) )
			return i;
	}

	Assert( 0 );
	return -1;
}

bool CCSGO_Loadout::EventShowLoadout( const char* szLoadoutPosition, int nTeam )
{
	loadout_positions_t nStart, nEnd;
	if ( GetLoadoutPositionRangeForSlot( szLoadoutPosition, nStart, nEnd ) )
	{
		UpdatePanelsForLoadout( nTeam, nStart, nEnd );
		SetHasClass( "loadout--visible", true );
		EventFilterForPosition( nTeam, GetLoadoutPositionForString( szLoadoutPosition ) );
		SetFocus();
	}

	return false;
}

bool CCSGO_Loadout::EventFilterForPosition( int nTeam, int nPosition )
{
	if ( nPosition >= 0 && nPosition < LOADOUT_POSITION_COUNT )
	{
		CFmtStr strFilters( "%s,%s", ( nTeam == TEAM_CT ? "ct" : "t" ), g_szLoadoutStringsSubPositions[ nPosition ] );
		CDropDown* pSort = panel_cast< CDropDown* >( RequireChildInLayoutFile( "LoadoutSortDropdown" ), true );
		const char* szSort = pSort->GetSelected() ? pSort->GetSelected()->GetID() : "rarity";
		char const *szDefaultFilter = "inv_group_equipment";
		char const *szSubCategory = g_szLoadoutStrings[ nPosition ];
		switch ( nPosition )
		{
		case LOADOUT_POSITION_FLAIR0:
			szDefaultFilter = "inv_display_slot";
			break;
		case LOADOUT_POSITION_SPRAY0:
			szDefaultFilter = "inv_graphic_art";
			szSubCategory = "graffiti";
			break;
		}
		m_pItemList->SetCategorySortAndFilters( szDefaultFilter, szSubCategory, "any", szSort, strFilters.Get() );
	}
	else
	{
		Assert( 0 );
	}
	return true;
}

void CCSGO_Loadout::UpdatePanelsForLoadout( int nTeam, loadout_positions_t nStart, loadout_positions_t nEnd )
{
	static CPanoramaSymbol k_symTeam( "team" );
	SetAttribute( k_symTeam, nTeam ); // Save off what team we're equipping to show the right weapons on refresh
	m_pTeamLogo->SetImage( CFmtStr( "file://{images}/icons/%s.svg", nTeam == TEAM_TERRORIST ? "t_logo" : "ct_logo" ).Get() );

	int nCurPos = nStart;
	m_pItemWheel->IterateChildren( [ & ] ( CPanel2D *pPanel ) -> bool 
	{
		static CPanoramaSymbol k_symLoadoutNoItem( "loadout--no-item" );
		static CPanoramaSymbol k_symEquipSlot( "equip_slot" );
		CItemImagePanel* pImage = panel_cast< CItemImagePanel* > ( pPanel->RequireChildTraverse( "ItemImage" ) );
		if ( nCurPos <= nEnd )
		{
			pImage->BSetFromEconItem( CSInventoryManager()->GetLocalCSInventory()->GetItemInLoadout( nTeam, nCurPos ) );
			pPanel->SetEnabled( true );
			pPanel->SetOnActivateEvent( Loadout_FilterForPosition::MakeEvent( this, nTeam, nCurPos ) );
			pPanel->SetHasClass( k_symLoadoutNoItem, false );
			pPanel->SetAttribute( k_symEquipSlot, g_szLoadoutStringsSubPositions[nCurPos] );
		}
		else
		{
			pImage->Clear();
			pPanel->SetEnabled( false );
			pPanel->SetHasClass( k_symLoadoutNoItem, true );
			pPanel->RemoveAttribute( k_symEquipSlot );
		}
		nCurPos++;
		return true;
	} );

}