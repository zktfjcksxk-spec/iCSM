//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//
#pragma once

#include "panorama/controls/image.h"
#include "game/shared/econ/econ_item_constants.h"

//-----------------------------------------------------------------------------
// Purpose: This control is the image for a single econ item
//-----------------------------------------------------------------------------
class CUI_EconItemImage : public panorama::CImagePanel
{
	DECLARE_PANEL2D( CUI_EconItemImage, panorama::CImagePanel );

public:
	CUI_EconItemImage( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CUI_EconItemImage();

	virtual bool BSetProperties( const CUtlVector< panorama::ParsedPanelProperty_t > &vecProperties ) OVERRIDE;

	void SetItem( const EconItemIDs_t &id, style_index_t nStyleIndex = INVALID_STYLE_INDEX );
	void SetItemDef( item_definition_index_t nItemDef, style_index_t nStyleIndex = INVALID_STYLE_INDEX );

	// For displaying gems. Don't need to call SetItem/SetItemDef to use this. Either use this or those.
	bool SetImageFromSocket( const IItemSocket *pSocket );

	const EconItemIDs_t &GetItem() const { return m_item; }
	style_index_t GetStyleIndex() const { return m_nStyleIndex; }

	virtual bool IsClonable() OVERRIDE{ return AreChildrenClonable(); }
	virtual CPanel2D *Clone() OVERRIDE;

protected:
	virtual void InitClonedPanel( CPanel2D *pClone ) OVERRIDE;

	void UpdateImages( const char *pImage, const char *pOverlayImage, const IItemSocket *pSocket );

private:
	panorama::CImagePanel *m_pOverlayImage;
	EconItemIDs_t m_item;
	style_index_t m_nStyleIndex;
};