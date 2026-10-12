//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Custom panorama panel for a page of inventory items
//
//=============================================================================//
#pragma once

#include "panorama/csgo_panorama.h"
#include "panorama/controls/delayloadlist.h"
#include "econ_item_inventory.h"

DECLARE_PANEL_EVENT0( CSGOInventoryItemLoaded );
DECLARE_PANEL_EVENT6( SetInventoryFilter, const char *, const char *, const char *, const char*, const char*, const char* );
DECLARE_PANEL_EVENT1( UpdateItemTile, itemid_t );



class CCSGO_InventoryItemList : public panorama::CDelayLoadList
{
	DECLARE_PANEL2D( CCSGO_InventoryItemList, panorama::CDelayLoadList );
public:
	CCSGO_InventoryItemList( panorama::CPanel2D *pParent, const char* pchID );
	virtual ~CCSGO_InventoryItemList();

	virtual void SetupJavascriptObjectTemplate() OVERRIDE;

	bool SetCategorySortAndFilters( const char* szCategory, const char* szSubCategory = "any", const char* szGroup = "any", const char* szSort = nullptr, const char* szFilterString = nullptr, const char* szSubStringFilter = nullptr );
	int GetItemCount( void ) const { return m_vecItems.Count(); }

protected:
	// CPanel2D overrides
	virtual bool BSetProperties( const CUtlVector< panorama::ParsedPanelProperty_t > &vecProperties ) OVERRIDE;

	bool EventSetCategorySortAndFilters( const panorama::CPanelPtr< panorama::IUIPanel > &ptrPanel, const char* szCategory, const char* szSubCategory = "any", const char* szGroup = "any", const char* szSort = nullptr, const char* szFilterString = nullptr, const char* szSubStringFilter = nullptr );
	bool EventEconItemEquipStateChange( int nSlot, const char* szSubPosition, itemid_t ullOldItem, itemid_t ullNewItem );

	bool OnImageLoaded( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, panorama::IImageSource *pImage );
	bool OnImageFailedLoad( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, panorama::IImageSource *pImage );

	//const CPlayerInventory::CClientInventoryViewTier *m_pItemList;
//	CUtlMap< itemid_t, CUtlBuffer > m_mapItemToRGBABuffer;

	// FIXME: Would love to make this const but the old sort code doesn't operate on const econ item views...
	CUtlVector < itemid_t > m_vecItems;
	CUtlString m_strTileLayoutFile;
	CUtlString m_strTileContextMenuFilter;
	CUtlString m_strPresentationList;
};
