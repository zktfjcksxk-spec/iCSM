//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef PANORAMA_IMAGEPANEL_H
#define PANORAMA_IMAGEPANEL_H
#pragma once

#include "panel2d.h"
#include "../data/iimagesource.h"

namespace panorama
{

DECLARE_PANEL_EVENT1( SetImageSource, const char * );
DECLARE_PANEL_EVENT0( ClearImageSource );
enum EImageScaling
{
	k_EImageScalingNone,
	k_EImageScalingStretchBoth,
	k_EImageScalingStretchX,
	k_EImageScalingStretchY,
	k_EImageScalingStretchBothToFitPreserveAspectRatio,
	k_EImageScalingStretchXToFitPreserveAspectRatio,
	k_EImageScalingStretchYToFitPreserveAspectRatio,
	k_EImageScalingStretchBothToCoverPreserveAspectRatio
};

enum EImageHorizontalAlignment
{
	k_EImageHorizontalAlignmentCenter,
	k_EImageHorizontalAlignmentLeft,
	k_EImageHorizontalAlignmentRight,
};

enum EImageVerticalAlignment
{
	k_EImageVerticalAlignmentCenter,
	k_EImageVerticalAlignmentTop,
	k_EImageVerticalAlignmentBottom,
};

enum EImageOrientation
{
	K_EImageOrientation_0,
	K_EImageOrientation_90,
	K_EImageOrientation_180,
	K_EImageOrientation_270,
};

EImageScaling EImageScalingFromName( const char *pchEnumName );

//-----------------------------------------------------------------------------
// Purpose: ImagePanel
//-----------------------------------------------------------------------------
class CImagePanel : public CPanel2D
{
	DECLARE_PANEL2D( CImagePanel, CPanel2D );

public:
	CImagePanel( CPanel2D *parent, const char * pchPanelID );
	virtual ~CImagePanel();

	virtual void Paint() OVERRIDE;
	virtual bool BSetProperties( const CUtlVector< ParsedPanelProperty_t > &vecProperties ) OVERRIDE;

	IImageSource *GetImage() { return m_pImage; }

	// Set an image from a URL (file://, http://), if pchDefaultImage is specified it must be a file:// url and will be
	// used while the actual image is loaded asynchronously, it will also remain in use if the actual image fails to load
	void SetImage( const char *pchImageURL, const char *pchDefaultImageURL, bool bPrioritizeLoad, const UIImageLoadParams_t &loadParams );

	void SetImage( const char *pchImageURL, const char *pchDefaultImageURL = NULL, bool bPrioritizeLoad = false, int nResizeWidth = -1, int nResizeHeight = -1, float fScaleFactor = -1.0f, SvgAttributeOverrides_t* pSvgAttributeOverrides = NULL )
	{
		UIImageLoadParams_t params;
		if( pSvgAttributeOverrides )
		{
			params.m_svgAttributeOverrides = *pSvgAttributeOverrides;
		}
		params.m_nResizeWidth = nResizeWidth;
		params.m_nResizeHeight = nResizeHeight;
		params.m_bAllowAnimation = m_bAnimate;
		params.m_fScaleFactor = fScaleFactor;
		SetImage( pchImageURL, pchDefaultImageURL, bPrioritizeLoad, params );
	}

	// Set an image from a RGBA buffer.
	void SetImage( const CUtlBuffer& bufRGBA, int nWide, int nTall, const char* pchDefaultImage, EImageFormat eImageFormatIn, const UIImageLoadParams_t &loadParams );

	void SetImage( const CUtlBuffer& bufRGBA, int nWide, int nTall, const char* pchDefaultImage = NULL, EImageFormat eImageFormatIn = k_EImageFormatR8G8B8A8, int nResizeWidth = -1, int nResizeHeight = -1  )
	{
		UIImageLoadParams_t params;
		params.m_nResizeWidth = nResizeWidth;
		params.m_nResizeHeight = nResizeHeight;
		SetImage( bufRGBA, nWide, nTall, pchDefaultImage, eImageFormatIn, params );
	}

	void SetImage( IImageSource *pImage );

	// Set an image from file buffer
	void SetImageFromFileBuffer( const CUtlBuffer& bufRGBA, const UIImageLoadParams_t &loadParams );

	void SetImageFromFileBuffer( const CUtlBuffer& bufRGBA, int nResizeWidth = -1, int nResizeHeight = -1 )
	{
		UIImageLoadParams_t params;
		params.m_nResizeWidth = nResizeWidth;
		params.m_nResizeHeight = nResizeHeight;
		SetImageFromFileBuffer( bufRGBA, params );
	}

	// Set an image from an engine render target texture
	void SetImageFromEngineRT( const char *pchEngineRTName );

	void Clear( bool bDestroying = false );
	bool IsSet() { return (m_pImage != NULL); }

	void SetScaling( EImageScaling eScale );
	void SetScaling( CPanoramaSymbol symScale );
	void SetAlignment( EImageHorizontalAlignment horAlign, EImageVerticalAlignment verAlign );
	void SetVisibleImageSlice( int nX, int nY, int nWidth, int nHeight );

	virtual void  SetupJavascriptObjectTemplate() OVERRIDE;

	void GetDebugPropertyInfo( CUtlVector< DebugPropertyOutput_t *> *pvecProperties ) OVERRIDE;

	virtual bool BRequiresContentClipLayer() OVERRIDE;

	void SetImageJS( const char *pchImageURL );

	virtual bool IsClonable() OVERRIDE { return AreChildrenClonable(); }
	virtual CPanel2D *Clone() OVERRIDE;

	void UnloadImages();
	void ReloadImages();
	const char* GetImageURL();

#ifdef DBGFLAG_VALIDATE
	virtual void ValidateClientPanel( CValidator &validator, const tchar *pchName ) OVERRIDE;
#endif

protected:
	bool OnImageLoaded( const CPanelPtr< IUIPanel > &pPanel, IImageSource *pImage );
	bool OnSetImageSource( const CPanelPtr<IUIPanel> &pPanel, const char *pchImageSource );
	bool OnClearImageSource( const CPanelPtr<IUIPanel> &pPanel );
	bool OnReadyForDisplay( const CPanelPtr< IUIPanel > &pPanel );
	bool OnUnreadyForDisplay( const CPanelPtr< IUIPanel > &pPanel );

	virtual void OnContentSizeTraverse( float *pflContentWidth, float *pflContentHeight, float flMaxWidth, float flMaxHeight, bool bFinalDimensions ) OVERRIDE;
	virtual void OnLayoutTraverse( float flFinalWidth, float flFinalHeight ) OVERRIDE;

	virtual void InitClonedPanel( CPanel2D *pPanel ) OVERRIDE;
	void LoadImageFromSourceSet( float flWidth );	

private:
	struct Source_t;
	static bool BParseSourceSet( CUtlVector< Source_t > *pvecSet, const char *pchString );

	// Set an image from an already created IImageSource, you should almost always use the simpler SetImage( pchImageURL, pchDefaultImageURL ) call.
	void SetImageInternal( IImageSource *pImage );
	void SetScaleFactor( const char *pchImageURL ); //Set scale factor for svgs based on parent window
	void UpdateScaleFactor(); // Update scale factor based on parent window

	EImageScaling m_eScaling;
	EImageHorizontalAlignment m_eHorAlignment;
	EImageVerticalAlignment m_eVerAlignment;
	EImageOrientation m_eOrientation;
	int m_nVisibleSliceX;
	int m_nVisibleSliceY;
	int m_nVisibleSliceWidth;
	int m_nVisibleSliceHeight;
	UIImageLoadParams_t m_reloadParams;
	bool m_bImageUnloaded;
	CUtlString m_strSource;
	CUtlString m_strSourceDefault;
	bool m_bAnimate;

	// source set storage
	struct Source_t
	{
		CUtlString m_strSource;
		int m_nWidth;
	};
	CUtlVector< Source_t > *m_pvecSourceSet;
	int m_nLoadedSourceWidth;
	float m_flLastLayoutWidth;	
	IImageSource *m_pImageLoading;

	IImageSource *m_pImage;
	float m_flPrevAnimateWidth;
	float m_flPrevAnimateHeight;
};

} // namespace panorama

#endif // PANORAMA_IMAGEPANEL_H
