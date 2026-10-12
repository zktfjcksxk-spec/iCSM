//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef IIMAGESOURCE_H
#define IIMAGESOURCE_H
#pragma once

#include "svg/svgloader.h"

namespace panorama
{

enum EImageFormat
{
	k_EImageFormatUnknown,
	k_EImageFormatR8G8B8A8,
	k_EImageFormatB8G8R8A8_PreMultiplied,
	k_EImageFormatB8G8R8A8,
	k_EImageFormatA8
};

class CPanel2D;
class IUITexture;

//
// Data source for image data used to render an asset
//
class IImageSource: public panorama::IUIJSObject
{
public:
	virtual bool BIsValid() = 0;
	virtual bool BIsLoaded() = 0;
	virtual bool BFailedToLoad() = 0;
	virtual IUITexture *GetTexture() = 0;
	virtual int GetWidth() = 0;
	virtual int GetHeight() = 0;
	virtual EImageFormat ImageFormat() = 0;
	virtual bool BIsAnimating() = 0;

	// Ref counting
	virtual int GetRefCount() = 0;
	virtual int AddRef() = 0;
	virtual int Release() = 0;
	
	virtual const char *GetJSTypeName() { return "IImageSource"; }

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName ) = 0;
#endif
protected:

	friend class CImageResourceManager;
};


//
// main interface to load images for display in the ui
//
const int k_ResizeNone = -1;

struct UIImageLoadParams_t
{
#if defined( SOURCE2_PANORAMA )
	CUtlString m_origin;
#endif

	// If max width or height >= 0 the load will fail if
	// the image loaded is larger than that dimension.
	int32 m_nMaxWidth;
	int32 m_nMaxHeight;

	// If resize width or height >= 0 the load will resize
	// the loaded image to the given size. If only one of
	// width or height >= 0 the other dimension will be
	// computed to retain the original image aspect ratio.
	int32 m_nResizeWidth;
	int32 m_nResizeHeight;

	// If scale factor is > 0 then the load will resize the 
	// loaded image. 
	// If the resize dimensions are also set, then the scale factor 
	// will be applied to those.
	// E.g. for use with .svg files to ensure they are regenerated
	// to suitable size for the screen resolution.
	float m_fScaleFactor;

	SvgAttributeOverrides_t m_svgAttributeOverrides;

	bool m_bAllowAnimation;

	UIImageLoadParams_t()
	{
		m_nMaxWidth = k_ResizeNone;
		m_nMaxHeight = k_ResizeNone;
		m_nResizeWidth = k_ResizeNone;
		m_nResizeHeight = k_ResizeNone;
		m_bAllowAnimation = false;
		m_fScaleFactor = (float)k_ResizeNone;
		m_svgAttributeOverrides.m_nFlags = 0;
	}

	bool IsWithinMaxSize( int nWidth, int nHeight ) const
	{
		if ( m_nMaxWidth != k_ResizeNone && nWidth > m_nMaxWidth )
		{
			return false;
		}
		if ( m_nMaxHeight != k_ResizeNone && nHeight > m_nMaxHeight )
		{
			return false;
		}

		return true;
	}

	bool ValidateMaxSize( int nWidth, int nHeight ) const;
	
	bool HasResize() const
	{
		return m_nResizeWidth != k_ResizeNone || m_nResizeHeight != k_ResizeNone;
	}
};

class IUIImageManager
{
public:

    // load image data from a URL (file://blah, http://blah/bob.tga, etc), pchDefaultResourceURL may be null, and is what will be used while the real resource is loaded
	// if it is set.  As such it must be a local file.  bPrioritizeLoad will make your request jump to the head of the queue if it is an image over HTTP, use sparingly!
	// nResizeWidth and nResizeHeight do nothing if -1, if one is set and the other -1 the image is resized before being made into a texture, but aspect ratio is maintained
	// with the specified dimension at the set size, if both are set the image will be stretched if needed.
    IImageSource *LoadImageFromURL( const IUIPanel *pPanel, const char *pchDefaultResourceURL, const char *pchResourceURL, bool bPrioritizeLoad, EImageFormat imgFormatOut, int32 nResizeWidth = k_ResizeNone, int32 nResizeHeight = k_ResizeNone, bool bAllowAnimation = true )
	{
		UIImageLoadParams_t params;
		params.m_nResizeWidth = nResizeWidth;
		params.m_nResizeHeight = nResizeHeight;
		params.m_bAllowAnimation = bAllowAnimation;
		return LoadImageFromURL( pPanel, pchDefaultResourceURL, pchResourceURL, bPrioritizeLoad, imgFormatOut, params );
	}

    virtual IImageSource *LoadImageFromURL( const IUIPanel *pPanel, const char *pchDefaultResourceURL, const char *pchResourceURL, bool bPrioritizeLoad, EImageFormat imgFormatOut, const UIImageLoadParams_t &loadParams ) = 0;

	// load image data from image file (png/jpg/tga) bytes you already have in memory, pchDefaultResourceURL may be null, and is what will be used while the real resource is loaded
	// if it is set.  As such it must be a local file.
	IImageSource *LoadImageFileFromMemory( const IUIPanel *pPanel, const char *pchResourceURLDefault, const CUtlBuffer &bufFile, int nResizeWidth = panorama::k_ResizeNone, int nResizeHeight = panorama::k_ResizeNone, bool bAllowAnimation = true, float fScaleFactor = (float)panorama::k_ResizeNone )
	{
		UIImageLoadParams_t params;
		params.m_nResizeWidth = nResizeWidth;
		params.m_nResizeHeight = nResizeHeight;
		params.m_bAllowAnimation = bAllowAnimation;
		params.m_fScaleFactor = fScaleFactor;
		return LoadImageFileFromMemory( pPanel, pchResourceURLDefault, bufFile, params );
	}

	virtual IImageSource *LoadImageFileFromMemory( const IUIPanel *pPanel, const char *pchResourceURLDefault, const CUtlBuffer &bufFile, const UIImageLoadParams_t &loadParams ) = 0;

	// load image data from RGBA bytes you already have in memory, pchDefaultResourceURL may be null, and is what will be used while the real resource is loaded
	// if it is set.  As such it must be a local file.
	IImageSource *LoadImageFromMemory( const IUIPanel *pPanel, const char *pchDefaultResourceURL, const CUtlBuffer &bufData, int nWide, int nTall, EImageFormat imgFormatIn = k_EImageFormatR8G8B8A8, int nResizeWidth = k_ResizeNone, int nResizeHeight = k_ResizeNone, bool bAllowAnimation = true )
	{
		UIImageLoadParams_t params;
		params.m_nResizeWidth = nResizeWidth;
		params.m_nResizeHeight = nResizeHeight;
		params.m_bAllowAnimation = bAllowAnimation;
		return LoadImageFromMemory( pPanel, pchDefaultResourceURL, bufData, nWide, nTall, imgFormatIn, params );
	}
    
	virtual IImageSource *LoadImageFromMemory( const IUIPanel *pPanel, const char *pchDefaultResourceURL, const CUtlBuffer &bufData, int nWide, int nTall, EImageFormat imgFormatIn, const UIImageLoadParams_t &loadParams ) = 0;

	// load image from an engine render target name
	IImageSource *LoadImageFromEngineRT( const IUIPanel *pPanel, const char *pchEngineRTName )
	{
		UIImageLoadParams_t params;
		return LoadImageFromEngineRT( pPanel, pchEngineRTName, params );
	}

	virtual IImageSource *LoadImageFromEngineRT( const IUIPanel *pPanel, const char *pchEngineRTName, const UIImageLoadParams_t &loadParams ) = 0;

	virtual CUtlString GetPchImageSourcePath( IImageSource *pImageSource ) = 0;

	virtual void ReloadChangedImage( IImageSource *pImageToReload ) = 0;

	virtual void ReloadChangedFile( const char *pchFile ) = 0;

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName ) = 0;
#endif
};

DECLARE_PANEL_EVENT1( ImageLoaded, IImageSource * );
DECLARE_PANEL_EVENT1( ImageFailedLoad, IImageSource * );

} // namespace panorama

#endif // IIMAGESOURCE_H
