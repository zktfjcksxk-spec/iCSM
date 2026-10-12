//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Panel for econ item images which may have a generated image 
//
//=============================================================================//
#pragma once

#include "panorama/csgo_panorama.h"


// Image panel that will generate/load an item image from an itemid_t
class CItemImagePanel : public panorama::CImagePanel
{
	DECLARE_PANEL2D( CItemImagePanel, panorama::CImagePanel );
public:
	CItemImagePanel( panorama::CPanel2D *pParent, const char* pchID );

	virtual void SetupJavascriptObjectTemplate() OVERRIDE;
	virtual bool BSetProperty( panorama::CPanoramaSymbol symName, const char *pchValue ) OVERRIDE;

	bool BSetFromItemID( itemid_t ullItemID );
	bool BSetFromEconItem( const CEconItemView* pItem );
	static bool BSetItemInfoOnPanel( const CEconItemView* pItem, CPanel2D *pPanel );

	// JS Delegates
	itemid_t GetItemID( void ) const;
	void SetItemID( itemid_t ullItemID ) { BSetFromItemID( ullItemID ); }

	bool BUsingSmallImage() const;
	void SetUseSmallImage( bool bUse );
	bool BUsingLargeImage() const;
	void SetUseLargeImage( bool bUse );

private:

	bool OnImageLoaded( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, panorama::IImageSource *pImage );
	bool OnImageFailedLoad( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, panorama::IImageSource *pImage );

private:
	itemid_t m_ullItemID;

	// Fallback image to load in case the iic or default image failed to load
	CUtlString m_strImageFallback;

	// Hack to let users force the _small or _large versions of default pngs. No effect on generated images.
	CUtlString m_strDefaultImageAlternateSize;
};
