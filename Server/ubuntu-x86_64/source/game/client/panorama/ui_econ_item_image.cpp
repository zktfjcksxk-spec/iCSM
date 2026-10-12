//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//
#include "cbase.h"
#include "ui_econ_item_image.h"
#include "econ/econ_item_sockets.h"
#include "econ/econ_item_inventory.h"
#include "panorama/ui_symbols.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

REGISTER_PANEL2D_FACTORY( CUI_EconItemImage, EconItemImage )

using namespace panorama;

CUI_EconItemImage::CUI_EconItemImage( CPanel2D *pParent, const char *pchID )
	: CImagePanel( pParent, pchID )
	, m_nStyleIndex( INVALID_STYLE_INDEX )
{
	m_pOverlayImage = nullptr;
}

CUI_EconItemImage::~CUI_EconItemImage()
{
}

void CUI_EconItemImage::UpdateImages( const char *pImage, const char *pOverlayImage, const IItemSocket *pSocket )
{
	// Juke the image to be the brand for spectator gems
	if ( pSocket && ( pSocket->GetSocketType() == SocketType_Spectator ) )
	{
		const CItemSocket_Spectator *pSpectatorSocket = static_cast< const CItemSocket_Spectator * >( pSocket );
		ItemPartner_t *pPartner = GetItemSchema()->GetItemPartner( pSpectatorSocket->GetPartnerType(), pSpectatorSocket->GetPartnerID() );
		if ( pPartner && !pPartner->m_symImage.IsEmpty() )
		{
			pImage = pPartner->m_symImage.Get();
		}
	}

	if ( !pImage || !pImage[0] )
	{
		Clear();
	}
	else
	{
		SetImage( InventoryManager()->GetInventoryImageURL( pImage ) );
	}

	if ( !pOverlayImage || !pOverlayImage[0] )
	{
		if ( m_pOverlayImage )
		{
			delete m_pOverlayImage;
			m_pOverlayImage = nullptr;
		}
	}
	else
	{
		m_pOverlayImage = new CImagePanel( this, "Overlay" );
		m_pOverlayImage->SetScaling( k_EImageScalingStretchBothToCoverPreserveAspectRatio );
		m_pOverlayImage->SetImage( InventoryManager()->GetInventoryImageURL( pOverlayImage ) );

		// Show the color of gems appropriately
		if ( pSocket && ( pSocket->GetSocketType() == SocketType_Color ) )
		{
			const CItemSocket_Color *pColorSocket = static_cast< const CItemSocket_Color * >( pSocket );
			m_pOverlayImage->AccessStyleDirty()->SetWashColor( pColorSocket->GetColor() );
		}
	}
}

void CUI_EconItemImage::SetItem( const EconItemIDs_t &id, style_index_t nStyleIndex /* = INVALID_STYLE_INDEX */ )
{
	if ( id == m_item && m_nStyleIndex == nStyleIndex )
		return;

	// Cache this off for cloning later
	m_item = id;
	m_nStyleIndex = nStyleIndex;

	item_definition_index_t nItemDefIndex = m_item.m_nItemDefIndex;

	CEconItemView *pEconItemView = nullptr;

	// Item ID has priority over item def index
	if ( m_item.m_nItemID != INVALID_ITEM_ID )
	{
		pEconItemView = InventoryManager()->GetLocalInventory()->GetInventoryItemByItemID( m_item.m_nItemID );
		if ( pEconItemView )
		{
			nItemDefIndex = pEconItemView->GetItemDefinition()->GetDefinitionIndex();
			if ( nStyleIndex == INVALID_STYLE_INDEX )
			{
				nStyleIndex = pEconItemView->GetStyle();
			}
		}
	}

	CEconItemView itemView;
	if ( !pEconItemView || ( nStyleIndex != pEconItemView->GetStyle() ) )
	{
		pEconItemView = &itemView;
		pEconItemView->Init( nItemDefIndex, AE_USE_SCRIPT_VALUE, AE_USE_SCRIPT_VALUE, 0, nullptr, nStyleIndex );
	}

	if ( m_pOverlayImage )
	{
		delete m_pOverlayImage;
		m_pOverlayImage = nullptr;
	}

	const char *pImage = nullptr;
	const char *pOverlayImage = nullptr;
	const IItemSocket *pSocket = nullptr;

	if ( pEconItemView->GetItemDefinition()->GetDefinitionIndex() != INVALID_ITEM_DEF_INDEX )
	{
		pImage = pEconItemView->GetInventoryImage();
		pOverlayImage = pEconItemView->GetInventoryOverlayImage();

		// Show the color of gems appropriately
		if ( pEconItemView->GetItemDefinition()->GetCapabilities() & ITEM_CAP_IS_GEM )
		{
			CSocketIterator socketIterator( ATTRIB_STORAGE_STATIC | ATTRIB_STORAGE_DYNAMIC );
			pEconItemView->IterateAttributes( &socketIterator );
			pSocket = socketIterator.GetSocketByAttributeDef( 1 );
		}
	}

	UpdateImages( pImage, pOverlayImage, pSocket );
}

bool CUI_EconItemImage::SetImageFromSocket( const IItemSocket *pSocket )
{
	CUtlConstString strSocketImage;
	CUtlConstString strSocketOverlayImage;
	if ( pSocket )
	{
		pSocket->GetImage( strSocketImage, strSocketOverlayImage );
	}

	UpdateImages( strSocketImage.Get(), strSocketOverlayImage.Get(), pSocket );
	return !strSocketImage.IsEmpty();
}

void CUI_EconItemImage::SetItemDef( item_definition_index_t nItemDef, style_index_t nStyleIndex )
{
	EconItemIDs_t ids;
	ids.m_nItemDefIndex = nItemDef;
	SetItem( ids, nStyleIndex );
}

bool CUI_EconItemImage::BSetProperties( const CUtlVector< ParsedPanelProperty_t > &vecProperties )
{
	EconItemIDs_t id;
	style_index_t nStyleIndex = INVALID_STYLE_INDEX;

	bool bSuccess = true;

	for ( const ParsedPanelProperty_t &prop : vecProperties )
	{
		if ( prop.m_symName == k_symItemDef )
		{
			id.m_nItemDefIndex = item_definition_index_t( V_atoi( prop.m_pchValue ) );
		}
		else if ( prop.m_symName == k_symStyleIndex )
		{
			nStyleIndex = style_index_t( V_atoi( prop.m_pchValue ) );
		}
	}

	if ( id.m_nItemDefIndex != INVALID_ITEM_DEF_INDEX )
	{
		SetItem( id, nStyleIndex );
	}

	if ( !BaseClass::BSetProperties( vecProperties ) )
	{
		bSuccess = false;
	}

	return bSuccess;
}

CPanel2D *CUI_EconItemImage::Clone()
{
	if ( !IsClonable() )
	{
		AssertMsg( false, "Panel can't be cloned (child type not clonable)" );
		return NULL;
	}

	CUI_EconItemImage *pImage = new CUI_EconItemImage( GetParent(), NULL );
	InitClonedPanel( pImage );

	return pImage;
}

void CUI_EconItemImage::InitClonedPanel( CPanel2D *pClone )
{
	BaseClass::InitClonedPanel( pClone );
	CUI_EconItemImage *pTarget = panel_cast< CUI_EconItemImage * >( pClone );
	pTarget->SetItem( m_item, m_nStyleIndex );
}
