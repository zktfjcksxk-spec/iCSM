//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Custom panorama panel for a page of inventory items
//
//=============================================================================//

#include "cbase.h"
#include "csgo_inventory_item_list.h"
#include "cstrike15_item_inventory.h"
#include "uicomponents/uicomponent_inventory.h"
#include "econ_item_inventory.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

REGISTER_PANEL2D_FACTORY( CCSGO_InventoryItemList, InventoryItemList );

DEFINE_PANORAMA_EVENT( CSGOInventoryItemLoaded );
DEFINE_PANORAMA_EVENT( SetInventoryFilter );
DEFINE_PANORAMA_EVENT( UpdateItemTile );

using namespace panorama;

namespace
{
	CPanoramaSymbolLazyInit k_symItemID( "itemid" );
	CPanoramaSymbolLazyInit k_symContextMenuFilter( "context_menu_filter" );
	CPanoramaSymbolLazyInit k_symFilterCategory( "filter_category" );
	CPanoramaSymbolLazyInit k_symItemImageFallback( "item_fallback_image" );
}

CCSGO_InventoryItemList::CCSGO_InventoryItemList(CPanel2D *pParent, const char *pchID) : CDelayLoadList(pParent, pchID)
{
	DbgVerify( BLoadLayout( "file://{resources}/layout/inventory_item_list.xml" ) );
	m_strTileLayoutFile.Set( "file://{resources}/layout/itemtile.xml" );
	SetAcceptsInput( true );

	SetLoadListItemFunction( [ this ]( panorama::CPanel2D *pParent, int nItemIndex, panorama::CPanel2D *pReuseItemPanel ) -> CPanel2D*
	{
		itemid_t unItemId = m_vecItems[ nItemIndex ];
		CEconItemView* pItem = const_cast< CEconItemView* > ( CEconItemView::FindOrCreateEconItemViewForItemID( unItemId ) );

		// JS uses 0 as an error value for item ids: if the object has been removed set the attribute used by UI script to 0. 
		if ( !pItem || !pItem->IsValid() )
			unItemId = 0;
		
		CPanelPtr< CPanel2D > pItemPanel;
		if ( pReuseItemPanel )
		{
			pItemPanel = pReuseItemPanel;
		}
		else
		{
			pItemPanel = new CPanel2D( pParent, nullptr );
			pItemPanel->BLoadLayout( m_strTileLayoutFile.Get() );
		}
		
		// For real econ items we own set the item id, for preview items combine def index and paint int a faux id
		// UI code uses these interchangeably as itemid. 
		pItemPanel->SetAttribute( k_symItemID, unItemId );
		pItemPanel->SetAttribute( k_symContextMenuFilter, m_strTileContextMenuFilter );
		pItemPanel->SetAttribute( k_symFilterCategory, m_strPresentationList );

		CPanelPtr< panorama::CImagePanel > pImage = panorama::panel_cast< panorama::CImagePanel* >( pItemPanel->FindChild( "ItemImage" ) );
		if ( pImage.Get() )
		{
			if ( !pReuseItemPanel )
			{
				// created a new panel -> register ImageLoaded / ImageFailedLoad panorama events on the image panel
				RegisterEventHandlerOnPanel( ImageLoaded(), pImage->UIPanel(), this, &CCSGO_InventoryItemList::OnImageLoaded );
				RegisterEventHandlerOnPanel( ImageFailedLoad(), pImage->UIPanel(), this, &CCSGO_InventoryItemList::OnImageFailedLoad );
			}

			pImage->SetAttribute( k_symItemImageFallback, "" );
			
			// GenerateCachedInventoryImageName caches the name internally, otherwise this could be const. 
			if ( pItem && pItem->IsValid() )
			{
				EEconItemInventoryImagePurpose_t eImagePurpose = k_EEconItemInventoryImagePurpose_Generic;
				if ( !V_strcmp( "inv_graphic_art", m_strPresentationList.String() ) )
					eImagePurpose = k_EEconItemInventoryImagePurpose_Graffiti;
				CUtlString strDefaultImagePath;
				strDefaultImagePath.Format( "file://{images_econ}/%s.png", pItem->GetInventoryImage( eImagePurpose ) );
				if ( pItem->HasGeneratedInventoryImage() )
				{
					const char* szCachedImage = pItem->GenerateCachedInventoryImageName();
					if ( g_pFullFileSystem->FileExists( szCachedImage ) )
					{
						pImage->SetAttribute( k_symItemImageFallback, strDefaultImagePath.Get() );
						pImage->SetImage( CFmtStr( "file://{images_econ}/../../%s", szCachedImage ).Get() );
					}
					else
					{
						pItem->GenerateInventoryImage( [pItemPanel, pImage]( const CEconItemView * pItem, CUtlBuffer &rawImageRgba, int nWidth, int nHeight, itemid_t unItemId ) -> void
						{
							if ( pItemPanel.Get() && pItemPanel->GetAttribute( k_symItemID, 0llu ) == unItemId )
							{
								pImage->SetImage( rawImageRgba, nWidth, nHeight, nullptr, k_EImageFormatB8G8R8A8 );
							}
						} );
					}
				}
				else
				{
					// Unpainted items just use image specified in schema
					pImage->SetImage( strDefaultImagePath.Get() );
				}
			}
		}

		DispatchEvent( CSGOInventoryItemLoaded(), pItemPanel.Get() );

		return pItemPanel.Get();

	} );

	RegisterEventHandler( SetInventoryFilter(), this, &CCSGO_InventoryItemList::EventSetCategorySortAndFilters );
	RegisterForUnhandledEvent( PanoramaComponent_Inventory_PlayerEquipSlotChanged(), this, &CCSGO_InventoryItemList::EventEconItemEquipStateChange );
}

CCSGO_InventoryItemList::~CCSGO_InventoryItemList()
{
}

void CCSGO_InventoryItemList::SetupJavascriptObjectTemplate()
{
	BaseClass::SetupJavascriptObjectTemplate();
	RegisterJSAccessorReadOnly( "count", PANORAMA_DELEGATE( &CCSGO_InventoryItemList::GetItemCount ) );
}

bool CCSGO_InventoryItemList::EventSetCategorySortAndFilters( const panorama::CPanelPtr< panorama::IUIPanel > &ptrPanel, const char* szCategory, const char* szSubCategory /*= "any"*/, const char* szGroup /*= "any"*/, const char* szSort /*= nullptr*/, const char* szFilterString /*= nullptr*/, const char* szSubStringFilter /*=nullptr*/ )
{
	SetCategorySortAndFilters( szCategory, szSubCategory, szGroup, szSort, szFilterString, szSubStringFilter );
	return true;
}

// When UI relevant state changes on an econ item this event should fire to update the UI side. This isn't needed for 
// newly created or deleted items, as we tear down the whole list through SetCategorySortAndFilters but for equip state changes
// we don't want to force a full refresh like that, just update the item here.
bool CCSGO_InventoryItemList::EventEconItemEquipStateChange( int nSlot, const char* szSubPosition, itemid_t ullOldItem, itemid_t ullNewItem )
{
	for ( int i = 0; i < GetListItemCount(); ++i )
	{
		CPanel2D *pCurItemPanel = GetListItemAtIndex( i );
		if ( !pCurItemPanel )
			continue;
		itemid_t ullItemId = pCurItemPanel->GetAttribute( k_symItemID, 0llu );
		if ( ullItemId == ullNewItem )
		{
			DispatchEvent( UpdateItemTile(), pCurItemPanel, ullNewItem );
		}
		else if ( ullItemId == ullOldItem )
		{
			DispatchEvent( UpdateItemTile(), pCurItemPanel, ullOldItem );
		}
	}
	return false;
}

bool CCSGO_InventoryItemList::SetCategorySortAndFilters( const char* szCategory, const char* szSubCategory /*= "any"*/, const char* szGroup /*= "any"*/, const char* szSort /*= nullptr*/, const char* szFilterString /*= nullptr*/, const char* szSubStringFilter /*= nullptr */ )
{
	CPlayerInventory::CClientInventoryViewTier * pCategory = CSInventoryManager()->GetLocalCSInventory()->GetClientInventoryView()->Find( szCategory );
	if ( !pCategory )
	{
		Warning( "CCSGO_InventoryItemList: Failed to find category %s\n", szCategory );
		return false;
	}

	CPlayerInventory::CClientInventoryViewTier * pSubCategory = pCategory->Find( szSubCategory );
	if ( !pSubCategory )
	{
		Warning( "CCSGO_InventoryItemList: Failed to find subcategory %s\n", szSubCategory );
		return false;
	}

	CPlayerInventory::CClientInventoryViewTier * pGroup = V_strcmp( "any", szGroup ) ? NULL : pSubCategory;
	if ( !pGroup )
	{
		Warning( "CCSGO_InventoryItemList: Failed to find group %s\n", szGroup );
		return false;
	}

	CInventoryFilters filters( szFilterString );
	// Copy the selected view tier into the our internal list for display
	m_vecItems.RemoveAll();
	FOR_EACH_MAP( pGroup->m_Items, i )
	{
		const CEconItemView *pCurItem = pGroup->m_Items.Key( i );

		// Apply legacy custom filters, if any.
		if ( szFilterString && filters.BShouldFilterItem( pCurItem ) )
			continue;

		// Filter the old way for now, just searching item name
		// From uicomponent_inventory.cpp
		extern bool UTIL_FilterByItemName( const char * m_szSubStringFilter, const CEconItemView * pEconItem );
		if ( szSubStringFilter && !StringIsEmpty( szSubStringFilter ) && UTIL_FilterByItemName( szSubStringFilter, pCurItem ) )
			continue;

		m_vecItems.AddToTail( pCurItem->GetItemID() ? pCurItem->GetItemID() : pCurItem->GetFauxItemIDFromDefinitionIndex() );
	}
	if ( szSort )
	{
		m_vecItems.SortPredicate( [ szSort ]( itemid_t lhs, itemid_t rhs )
		{
			// Use old scaleform sort function
			const CEconItemView* pLHS = CEconItemView::FindOrCreateEconItemViewForItemID( lhs );
			const CEconItemView* pRHS = CEconItemView::FindOrCreateEconItemViewForItemID( rhs );
			return CUiComponent_Inventory::SortFunc_Helper( &pLHS, &pRHS, InventorySortFromName( szSort ) ) == -1;
		} );
	}

	// Reload the list with the new contained items, remember the filters to set attribute tags on the children during delayed lambda population
	UpdateListItems( m_vecItems.Count() );
	m_strPresentationList = szCategory;
	return m_vecItems.Count() > 0;
}

bool CCSGO_InventoryItemList::BSetProperties( const CUtlVector< ParsedPanelProperty_t > &vecProperties )
{
	static const CPanoramaSymbol k_symInventoryFilter( "inventoryfilter" );
	static const CPanoramaSymbol k_symItemLayoutFile( "itemlayout" );
	static const CPanoramaSymbol k_symItemContextMenuFilter( "item_context_menu_filter" );

	for ( const ParsedPanelProperty_t &prop : vecProperties )
	{
		if ( prop.m_symName == k_symInventoryFilter )
		{
			SetCategorySortAndFilters( prop.m_pchValue, "any", "any" );
		}
		else if ( prop.m_symName == k_symItemLayoutFile )
		{
			m_strTileLayoutFile.Set( prop.m_pchValue );
		}
		else if ( prop.m_symName == k_symItemContextMenuFilter )
		{
			m_strTileContextMenuFilter.Set( prop.m_pchValue );
		}
	}

	return BaseClass::BSetProperties( vecProperties );
}

bool CCSGO_InventoryItemList::OnImageLoaded( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, panorama::IImageSource *pImage )
{
	panorama::CImagePanel *pImagePanel = panorama::panel_cast< panorama::CImagePanel* >( (panorama::CPanel2D*)pPanel->ClientPtr() );
	if ( pImagePanel && pImage == pImagePanel->GetImage() )
	{
		// Successfully loaded the image, no need for the image fallback anymore
		pImagePanel->SetAttribute( k_symItemImageFallback, "" );
	}

	// Always return false so CImagePanel handler gets called next (that should return true)
	return false;
}

bool CCSGO_InventoryItemList::OnImageFailedLoad( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, panorama::IImageSource *pImage )
{
	panorama::CImagePanel *pImagePanel = panorama::panel_cast< panorama::CImagePanel* >( (panorama::CPanel2D*)pPanel->ClientPtr() );
	if ( pImagePanel && ( pImage == pImagePanel->GetImage() ) /*&& !m_strImageFallback.IsEmpty()*/ )
	{
		const char *pchFallbackImage = pImagePanel->GetAttribute( k_symItemImageFallback, "" );
		if ( pchFallbackImage && pchFallbackImage[0] != '\0' )
		{
			// Loading fallback image (asynchronously)
			pImagePanel->SetImage( pchFallbackImage );
			// Clearing image fallback to avoid trying to reload it in case of failure
			pImagePanel->SetAttribute( k_symItemImageFallback, "" );

			return true;
		}
	}

	return false;
}


