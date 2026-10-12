//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Custom panorama panel for a page of inventory items
//
//=============================================================================//

#include "cbase.h"
#include "csgo_item_image_panel.h"
#include "cstrike15_item_inventory.h"
//#include "uicomponents/uicomponent_inventory.h"
#include "panorama/uimetaprogramming.h"
#include "panorama/uijsregistration.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


using namespace panorama;

REGISTER_PANEL2D_FACTORY( CItemImagePanel, ItemImage );

CItemImagePanel::CItemImagePanel( panorama::CPanel2D *pParent, const char* pchID ) :
	CImagePanel( pParent, pchID )
	, m_ullItemID( 0 )
{
	RegisterEventHandler( ImageLoaded(), this, &CItemImagePanel::OnImageLoaded );
	RegisterEventHandler( ImageFailedLoad(), this, &CItemImagePanel::OnImageFailedLoad );
}

void CItemImagePanel::SetupJavascriptObjectTemplate()
{
	BaseClass::SetupJavascriptObjectTemplate();

	RegisterJSAccessor( "itemid", PANORAMA_DELEGATE( &CItemImagePanel::GetItemID ), PANORAMA_DELEGATE( &CItemImagePanel::SetItemID ) );
	RegisterJSAccessor( "small", PANORAMA_DELEGATE( &CItemImagePanel::BUsingSmallImage ), PANORAMA_DELEGATE( &CItemImagePanel::SetUseSmallImage ) );
	RegisterJSAccessor( "large", PANORAMA_DELEGATE( &CItemImagePanel::BUsingLargeImage ), PANORAMA_DELEGATE( &CItemImagePanel::SetUseLargeImage ) );
}

bool CItemImagePanel::BSetProperty( CPanoramaSymbol symName, const char *pchValue )
{
	static CPanoramaSymbol symItemID( "itemid" );
	static CPanoramaSymbol symSmallImage( "small" );
	static CPanoramaSymbol symLargeImage( "large" );

	if ( symName == symItemID )
	{
		itemid_t id = V_atoui64( pchValue );
		if ( id == 0 ) // cant set to invalid from properties, catch numbers that don't parse. 
			return false;

		if ( !BSetFromItemID( id ) )
			return false;
	}
	else if ( symName == symSmallImage )
	{
		bool bVal = false;
		if ( !CSSHelpers::BParseTrueFalse( pchValue, &bVal ) )
			return false;

		SetUseSmallImage( bVal );
	}
	else if ( symName == symLargeImage )
	{
		bool bVal = false;
		if ( !CSSHelpers::BParseTrueFalse( pchValue, &bVal ) )
			return false;

		SetUseLargeImage( bVal );
	}

	return BaseClass::BSetProperty( symName, pchValue );
}

bool CItemImagePanel::BSetFromItemID( itemid_t ullItemID )
{
	if ( CombinedItemIdIsDefIndexAndPaint( ullItemID ) )
	{
		// Fake items: Just set to image on disk and be done. 
		m_ullItemID = ullItemID; // Set member itemid to the fake item id... CEconItemView doesn't have this value stored. 
		return BSetFromEconItem( InventoryManager()->GetReferenceEconItem( ullItemID ) );
	}
	else
	{
		// Real item IDs: Find in the inventory and start image generation, set to default 
		CCSPlayerInventory* pMyInv = CSInventoryManager()->GetLocalCSInventory();
		if ( pMyInv )
		{
			CEconItemView *pItem = pMyInv->GetInventoryItemByItemID( ullItemID );
			if ( pItem )
			{
				return BSetFromEconItem( pItem );
			}
		}
	}

	return false;
}

bool CItemImagePanel::BSetFromEconItem( const CEconItemView* pItem )
{
	if ( !pItem || !pItem->GetInventoryImage() )
		return false;

	m_strImageFallback.Clear();

	// Set with _large or _small as requested, and fallback to the undecorated path in case this item has no special sized alternatives.
	CFmtStr strDefaultImage( "file://{images_econ}/%s%s.png", pItem->GetInventoryImage(), m_strDefaultImageAlternateSize.Get() );
	CFmtStr strDefaultImageFallback( "file://{images_econ}/%s.png", pItem->GetInventoryImage() );

	// HACK: Fake CEconItemViews have an itemid of 0, and changing this would be difficult because of old code using GetItemID()==0 to check if an item is real...
	// This makes it hard to treat CEconItemViews the same... This check is to save off the itemid if this is a real item, and we assume BSetFromItemID will set it to
	// the fake item id (combined defidx + paint) before calling here. Our itemid member needs to be either a real itemid or combined defidx+paint because that's what
	// UI code will use to interact with items. 
	if ( pItem->GetItemID() != 0 )
		m_ullItemID = pItem->GetItemID();
	//Assert( m_ullItemID != 0 );
	if ( pItem->HasGeneratedInventoryImage() )
	{
		CEconItemView *pNonConstHack = const_cast< CEconItemView* > ( pItem );
		const char* szCachedImage = pNonConstHack->GenerateCachedInventoryImageName();
		if ( g_pFullFileSystem->FileExists( szCachedImage ) )
		{
			m_strImageFallback.Set( strDefaultImage.Get() );
			SetImage( CFmtStr( "file://{images_econ}/../../%s", szCachedImage ).Get() );
		}
		else
		{
			CPanelPtr< CItemImagePanel > pImagePanel( this );
			pNonConstHack->GenerateInventoryImage( [pImagePanel]( const CEconItemView * pItem, CUtlBuffer &rawImageRgba, int nWidth, int nHeight, itemid_t unGeneratedItemId ) -> void
			{
				if ( pImagePanel.Get() && pImagePanel->GetItemID() == unGeneratedItemId )
				{
					pImagePanel->SetImage( rawImageRgba, nWidth, nHeight, NULL, k_EImageFormatB8G8R8A8 );
				}
			} );
		}
	}
	else
	{
		if ( !m_strDefaultImageAlternateSize.IsEmpty() )
		{
			// Fallback only needed if we specified a small/large alternate size
			m_strImageFallback.Set( strDefaultImageFallback.Get() );
		}
		SetImage( strDefaultImage.Get() );
	}
	
	BSetItemInfoOnPanel( pItem, this );

	return true;
}

bool CItemImagePanel::BSetItemInfoOnPanel( const CEconItemView* pItem, CPanel2D *pPanel )
{
	if ( !pItem->IsValid() )
		return false;

	if ( const CEconItemRarityDefinition *pRarity = GetItemSchema()->GetRarityDefinition( pItem->GetRarity() ) )
	{
		pPanel->SetDialogVariableLocString( "item_rarity", pRarity->GetWepLocKey() );
		static CPanoramaSymbol k_symRarityClass( "rarity_class" );
		pPanel->SwitchClass( k_symRarityClass, pRarity->GetRarityClass() );
	}

	char szNameBuff[ 256 ];
	pItem->GetItemDisplayNameUtf8( szNameBuff, sizeof( szNameBuff ) );
	pPanel->SetDialogVariable( "item_name", szNameBuff );

	return true;
}

itemid_t CItemImagePanel::GetItemID( void ) const
{
	return m_ullItemID;
}

bool CItemImagePanel::BUsingSmallImage() const
{
	return m_strDefaultImageAlternateSize == "_small";
}

void CItemImagePanel::SetUseSmallImage( bool bUse )
{
	if ( bUse )
		m_strDefaultImageAlternateSize = "_small";
	else
		m_strDefaultImageAlternateSize.Clear();
}

bool CItemImagePanel::BUsingLargeImage() const
{
	return m_strDefaultImageAlternateSize == "_large";
}

void CItemImagePanel::SetUseLargeImage( bool bUse )
{
	if ( bUse )
		m_strDefaultImageAlternateSize = "_large";
	else
		m_strDefaultImageAlternateSize.Clear();
}

bool CItemImagePanel::OnImageLoaded( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, panorama::IImageSource *pImage )
{
	if ( pImage == GetImage() )
	{
		// Successfully loaded the image, no need for the image fallback anymore
		m_strImageFallback.Clear();
	}
	
	// Always return false so CImagePanel handler gets called next (that should return true)
	return false;
}

bool CItemImagePanel::OnImageFailedLoad( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, panorama::IImageSource *pImage )
{
	if ( ( pImage == GetImage() ) && !m_strImageFallback.IsEmpty() )
	{
		// Loading fallback image (asynchronously)
		SetImage( m_strImageFallback.Get() );
		// Clearing image fallback to avoid trying to reload it in case of failure
		m_strImageFallback.Clear();

		return true;
	}

	return false;
}