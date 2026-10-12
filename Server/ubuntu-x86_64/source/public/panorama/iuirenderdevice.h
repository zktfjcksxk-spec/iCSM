//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef IUIRENDERDEVICE_H
#define IUIRENDERDEVICE_H

#if defined(_WIN32) || defined(SOURCE2_PANORAMA)
#pragma once
#endif

#include "panoramatypes.h"
#include "panorama/renderer/rendercommands.h"
#include "tier1/refcount.h"
#if defined( SOURCE2_PANORAMA )
class IRenderContext;
#endif

namespace panorama
{

enum E2DTextureFormat
{
	k_EFormatRGBA8,
	k_EFormatBGRA8,
	k_EFormatRGB8,
	k_EFormatBGR8, // 8 bits per channel, last 8 bits in the dword are ignored
	k_EFormatA8,
	k_EFormatYUV420,
	k_EFormatR16G16B16A16,
	k_EFormatDXT1,
	k_EFormatDXT5,
};


enum EAlphaChannelType
{
	k_EAlphaChannelType_None,
	k_EAlphaChannelType_Normal,
	k_EAlphaChannelType_PreMultiplied,
};


//
// Class to represent all textures
//
class IUITexture : public IRefCounted
{
public:
	virtual uint32 GetOriginalWidth() { return GetTextureWidth(); }
	virtual uint32 GetOriginalHeight() { return GetTextureHeight(); }
	virtual uint32 GetTextureWidth() = 0;
	virtual uint32 GetTextureHeight() = 0;
	virtual uint32 GetStride() = 0;
	virtual E2DTextureFormat GetFormat() = 0;
	virtual EAlphaChannelType GetAlphaChannelType() = 0;
	virtual bool BIsReady() = 0;

protected:
	virtual ~IUITexture() { }
};


//
// Class to handle double buffered textures of various standard formats (RGBA8, BGRA8, alpha-premulitplied/or-not, etc)
//
class IUIDoubleBufferedTexture : public IUITexture
{
public:
	virtual ~IUIDoubleBufferedTexture() {}

	// Update the data for rendering next frame
	virtual int32 UpdateTextureData( void *pTextureData ) = 0;
};


//
// YUV420 textures are special, because they need 3 textures one for Y, U, and V, and then
// they need special rendering rules in a pixel shader to scale/color convert.
//
class IUIDoubleBufferedYUV420Texture : public IUITexture
{
public:
	virtual ~IUIDoubleBufferedYUV420Texture() {}

	// YUV420 texture can update their size internally whenever their texture data changes. Since this happens across multiple threads, calling
	// GetTextureWidth followed by GetTextureHeight could theoretically lead to an inconsistent view wehre you have the width from one frame
	// and the height from another. Calling GetTextureSize will always return values that are consistent for a full frame.
	virtual void GetTextureSize( uint32 &unTextureWidth, uint32 &unTextureHeight ) = 0;

	// Update the YUV420 data for rendering next frame
	virtual bool BUpdateTextureData( uint unWidth, uint unHeight, void *pYBuffer, void *pUBuffer, void *pVBuffer, uint unStrideY, uint unStrideU, uint unStrideV ) = 0;

	virtual void ClearCurrentTextureHandles() = 0;
};


//
// An IUIRenderDevice represents the 3d device used for creating textures.
//
class IUIRenderDevice
{
public:
	// Called to create a texture, you call this directly on the main thread and the returned texture interface is thread safe,
	// so you can access its id/size and delete it from the main thread as well.  Drawing calls are not synchronized with texture creation,
	// but the contract is you must create the texture before attempting to draw for it's id.
	virtual bool BCreateTexture( IUITexture **pTextureOutput, void *pubTextureData, uint32 unWidth, uint32 unHeight, uint32 unStride, E2DTextureFormat eFormat, EAlphaChannelType eAlphaChannelType ) = 0;

#if defined( SOURCE2_PANORAMA )
	// Called to create a texture from a source 2 resource file. Follows the same rules as above
	virtual bool BCreateTexture( IUITexture **pTextureOutput, const char *pResourceFile ) = 0;
#endif

	// Called to create a double buffered texture, you call this directly on the main thread
	// and the returned texture interface is thread safe, so you can update the texture data directly.  The textures are 
	// double buffered, so it should be hard to block the render thread, but some locking does occur.  Unlike normal texture
	// drawing your draw calls are not synchronized with texture data updates, so you could end up skipping frames or such.
	virtual bool BCreateDoubleBufferedTexture( IUIDoubleBufferedTexture **pDoubleBufferedOutput, uint32 unWidth, uint32 unHeight, uint32 unStride, E2DTextureFormat eFormat, EAlphaChannelType eAlphaChannelType, bool bSerializedUploads ) = 0;

	// Called to create a double buffered YUV420 texture (for movie rendering), you call this directly on the main thread
	// and the returned texture interface is thread safe, so you can update the texture data directly.  The textures are 
	// double buffered, so it should be hard to block the render thread, but some locking does occur.  Unlike normal texture
	// drawing your draw calls are not synchronized with texture data updates, so you could end up skipping frames or such.
	virtual bool BCreateDoubleBufferedYUV420Texture( IUIDoubleBufferedYUV420Texture **pDoubleBufferedYUV420Output ) = 0;

	// Called to create a texture that will reference an engine render target
	virtual bool BCreateTextureEngineRTRef( IUITexture **pTextureOutput, const char *pchEngineRTname ) = 0;

	virtual bool BCreateRenderTargetTexture( IUITexture **pTextureOutput, uint32 unWidth, uint32 unHeight ) = 0;
	virtual void PushPanelRT( const CRefPtr< IUITexture > &pRT ) = 0;
	virtual void PopPanelRT( const CRefPtr< IUITexture > &pRT ) = 0;

};


}
#endif // IUIRENDERDEVICE_H
