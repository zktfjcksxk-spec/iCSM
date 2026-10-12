//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef UIRENDERDEVICESOURCE2_H
#define UIRENDERDEVICESOURCE2_H
#pragma once

#include "iuirenderdevice.h"

namespace panorama
{

//
// Base instance of UI engine, with non platform specific functionality
//
class CUIRenderDeviceSource2 : public IUIRenderDevice
{
public:
	CUIRenderDeviceSource2();
	virtual ~CUIRenderDeviceSource2();

	// Called to create a texture, you call this directly on the main thread and the returned texture interface is thread safe,
	// so you can access its id/size and delete it from the main thread as well.  Drawing calls are not synchronized with texture creation,
	// but the contract is you must create the texture before attempting to draw for it's id.
	virtual bool BCreateTexture( IUITexture **pTextureOutput, void *pubTextureData, uint32 unWidth, uint32 unHeight, uint32 unStride, E2DTextureFormat eFormat, EAlphaChannelType eAlphaChannelType ) OVERRIDE;
	virtual bool BCreateTexture( IUITexture **pTextureOutput, const char *pResourceFile ) OVERRIDE;

	// Called to create a double buffered texture, you call this directly on the main thread
	// and the returned texture interface is thread safe, so you can update the texture data directly.  The textures are 
	// double buffered, so it should be hard to block the render thread, but some locking does occur.  Unlike normal texture
	// drawing your draw calls are not synchronized with texture data updates, so you could end up skipping frames or such.
	virtual bool BCreateDoubleBufferedTexture( IUIDoubleBufferedTexture **pDoubleBufferedOutput, uint32 unWidth, uint32 unHeight, uint32 unStride, E2DTextureFormat eFormat, EAlphaChannelType eAlphaChannelType, bool bSerializedUploads ) OVERRIDE;

	// Called to create a double buffered YUV420 texture (for movie rendering), you call this directly on the main thread
	// and the returned texture interface is thread safe, so you can update the texture data directly.  The textures are 
	// double buffered, so it should be hard to block the render thread, but some locking does occur.  Unlike normal texture
	// drawing your draw calls are not synchronized with texture data updates, so you could end up skipping frames or such.
	virtual bool BCreateDoubleBufferedYUV420Texture( IUIDoubleBufferedYUV420Texture **pDoubleBufferedYUV420Output ) OVERRIDE;

	// Called to create a texture that will reference an engine render target
	virtual bool BCreateTextureEngineRTRef( IUITexture **pTextureOutput, const char *pchEngineRTname ) OVERRIDE;

	virtual bool BCreateRenderTargetTexture( IUITexture **pTextureOutput, uint32 unWidth, uint32 unHeight ) OVERRIDE;
	virtual void PushPanelRT( const CRefPtr< IUITexture > &pRT ) OVERRIDE;
	virtual void PopPanelRT( const CRefPtr< IUITexture > &pRT ) OVERRIDE;

};


class CSource2UITexture : public CRefCounted1< IUITexture, CRefCountServiceMT >
{
public:
	CSource2UITexture();

	// IUITexture interface
	virtual uint32 GetOriginalWidth() OVERRIDE { return m_nOriginalWidth; }
	virtual uint32 GetOriginalHeight() OVERRIDE { return m_nOriginalHeight; }

	virtual uint32 GetTextureWidth() OVERRIDE { return m_nTextureWidth; }
	virtual uint32 GetTextureHeight() OVERRIDE { return m_nTextureHeight; }

	virtual uint32 GetStride() OVERRIDE { return m_nTextureStride; }

	virtual E2DTextureFormat GetFormat() OVERRIDE { return m_eFormat; }
	virtual EAlphaChannelType GetAlphaChannelType() OVERRIDE { return m_eAlphaChannelType; }

	virtual bool BIsReady() OVERRIDE { return m_bTextureUploaded; }

	bool IsValid() { return m_hRenderTexture != RENDER_TEXTURE_HANDLE_INVALID && m_hRenderTexture.IsValid() && m_bTextureUploaded; }
	HRenderTexture GetTextureHandle() { return m_hRenderTexture; }

	void SetTextureData( void *pubTextureData, uint32 unWidth, uint32 unHeight, uint32 unStride, E2DTextureFormat eFormat, EAlphaChannelType eAlphaChannelType );
	void SetTextureData( const char *pResourceFile );

	float GetOriginalWidthScale() { return m_flOriginalWidthScale; }
	float GetOriginalHeightScale() { return m_flOriginalHeightScale; }

protected:
	virtual ~CSource2UITexture();

private:
	bool m_bTextureUploaded;

	uint32 m_nTextureStride;
	uint32 m_nTextureWidth;
	uint32 m_nTextureHeight;

	E2DTextureFormat m_eFormat;
	EAlphaChannelType m_eAlphaChannelType;

	HRenderTextureStrong m_hRenderTexture;

	uint32 m_nOriginalWidth;
	uint32 m_nOriginalHeight;

	float m_flOriginalWidthScale;
	float m_flOriginalHeightScale;
};


class CSource2DoubleBufferedTexture : public CRefCounted1< IUIDoubleBufferedTexture, CRefCountServiceMT >
{
public:
	// Constructor
	CSource2DoubleBufferedTexture( uint32 unWidth, uint32 unHeight, uint32 unStride, E2DTextureFormat eFormat, EAlphaChannelType eAlphaChannelType );

	// Destructor
	virtual ~CSource2DoubleBufferedTexture();

	// IUIDoubleBufferedTexture interface
	virtual int32 UpdateTextureData( void *pTextureData );

	// IUITexture interface
	virtual uint32 GetTextureWidth() { return m_pTexture->GetTextureWidth(); }
	virtual uint32 GetTextureHeight() { return m_pTexture->GetTextureHeight(); }
	virtual uint32 GetStride() { return m_pTexture->GetStride(); }
	virtual E2DTextureFormat GetFormat() { return m_pTexture->GetFormat(); }
	virtual EAlphaChannelType GetAlphaChannelType() { return m_pTexture->GetAlphaChannelType(); }
	virtual bool BIsReady() { return m_pTexture->BIsReady(); }

	HRenderTexture GetTextureHandle() { return m_pTexture->GetTextureHandle(); }

private:
	CRefPtr< CSource2UITexture > m_pTexture;
	int32 m_nSerial;
};


class CSource2DoubleBufferedYUV420Texture : public CRefCounted1< IUIDoubleBufferedYUV420Texture, CRefCountServiceMT >
{
public:
	// Constructor
	CSource2DoubleBufferedYUV420Texture();

	// Destructor
	virtual ~CSource2DoubleBufferedYUV420Texture();

	// IUIDoubleBufferedYUV420Texture interface
	virtual void GetTextureSize( uint32 &unTextureWidth, uint32 &unTextureHeight ) OVERRIDE;
	virtual bool BUpdateTextureData( uint unWidth, uint unHeight, void *pYBuffer, void *pUBuffer, void *pVBuffer, uint unStrideY, uint unStrideU, uint unStrideV ) OVERRIDE;

	// IUITexture interface
	virtual uint32 GetTextureWidth() OVERRIDE;
	virtual uint32 GetTextureHeight() OVERRIDE;
	virtual uint32 GetStride() OVERRIDE;
	virtual E2DTextureFormat GetFormat() OVERRIDE { return k_EFormatYUV420; }
	virtual EAlphaChannelType GetAlphaChannelType() OVERRIDE { return k_EAlphaChannelType_None; }
	virtual bool BIsReady() OVERRIDE;

	virtual void ClearCurrentTextureHandles() OVERRIDE;

	void GetCurrentTextureHandles( HRenderTexture &hRenderTextureY, HRenderTexture &hRenderTextureU, HRenderTexture &hRenderTextureV );

#ifdef PANORAMA_USE_S1WRAPPER
	float GetOriginalWidthScale() { return m_currentTexture.flOriginalWidthScale; }
	float GetOriginalHeightScale() { return m_currentTexture.flOriginalHeightScale; }
#endif

private:
	static void CopyTextureData( void *pDestData, uint32 unWidth, uint32 unHeight, const void *pSourceData, uint unStride );

	// Callbacks from AsyncSetTextureData
	void RecycleYBuffer( const void* pData );
	void RecycleUVBuffer( const void *pData );

	struct STexture
	{
		STexture()
		{
			Clear();
		}

		~STexture()
		{
			Clear();
		}

		void Clear()
		{
			unTextureWidth = 0;
			unTextureHeight = 0;

			hRenderTextureY = RENDER_TEXTURE_HANDLE_INVALID;
			hRenderTextureU = RENDER_TEXTURE_HANDLE_INVALID;
			hRenderTextureV = RENDER_TEXTURE_HANDLE_INVALID;

#ifdef PANORAMA_USE_S1WRAPPER
			flOriginalWidthScale = 1.0f;
			flOriginalHeightScale = 1.0f;
#endif
		}

		void Swap( STexture &other )
		{
			std::swap( unTextureWidth, other.unTextureWidth );
			std::swap( unTextureHeight, other.unTextureHeight );

			SwapRenderTexture( hRenderTextureY, other.hRenderTextureY );
			SwapRenderTexture( hRenderTextureU, other.hRenderTextureU );
			SwapRenderTexture( hRenderTextureV, other.hRenderTextureV );

#ifdef PANORAMA_USE_S1WRAPPER
			flOriginalWidthScale = other.flOriginalWidthScale;
			flOriginalHeightScale = other.flOriginalHeightScale;
#endif
		}

		static void SwapRenderTexture( HRenderTextureStrong &hLeft, HRenderTextureStrong &hRight )
		{
#ifdef PANORAMA_USE_S1WRAPPER
			HRenderTextureStrong hTemp( hLeft.GetResourceHandle() );
#else
			HRenderTexture hTemp = hLeft;
#endif
			hLeft = hRight;
			hRight = hTemp;
		}

		uint32 unTextureWidth;
		uint32 unTextureHeight;

		HRenderTextureStrong hRenderTextureY;
		HRenderTextureStrong hRenderTextureU;
		HRenderTextureStrong hRenderTextureV;

#ifdef PANORAMA_USE_S1WRAPPER
		float flOriginalWidthScale;
		float flOriginalHeightScale;
#endif
	};

	// You should hold the m_currentTextureMutex when accessing m_currentTexture
	CThreadFastMutex m_currentTextureMutex;
	STexture m_currentTexture;
	bool m_bTextureUploaded;

	bool BUpdateTextureDataInternal( STexture &texture, uint unWidth, uint unHeight, void *pYBuffer, void *pUBuffer, void *pVBuffer, uint unStrideY, uint unStrideU, uint unStrideV );

	struct SPixelDataBuffer
	{
		uint32 unByteCount;
		uint8 bufferData[ 1 ]; // actual size is unByteCount
	};

	static SPixelDataBuffer *CreatePixelDataBuffer( uint32 unByteCount )
	{
		uint8 *pMemory = new byte[ sizeof( SPixelDataBuffer ) - sizeof( uint8 ) + ( unByteCount * sizeof( uint8 ) ) ];
		SPixelDataBuffer *pBuffer = ( SPixelDataBuffer * )( pMemory );
		pBuffer->unByteCount = unByteCount;
		return pBuffer;
	}

	static void FreePixelDataBuffer( SPixelDataBuffer *pBuffer )
	{
		uint8 *pMemory = ( uint8 * )pBuffer;
		delete[] pMemory;
	}

	static SPixelDataBuffer *GetPixelDataBufferFromBufferData( const void *pData )
	{
		// pData is actually a pointer to bufferData, so we can just go backwards from there to find the real data buffer;
		return ( SPixelDataBuffer * )( ( uint8 *)pData - offsetof( SPixelDataBuffer, bufferData ) );
	}

	uint32 m_unYBuffersByteCount;
	CUtlVectorFixed< SPixelDataBuffer *, 2 > m_YBuffers;

	uint32 m_unUVBuffersByteCount;
	CUtlVectorFixed< SPixelDataBuffer *, 4 > m_UVBuffers;

	static CThreadMutex s_bufferPoolMutex;
};


class CSource2UIRenderTargetTexture : public CRefCounted1< IUITexture, CRefCountServiceMT >
{
public:
	// Constructor
	CSource2UIRenderTargetTexture( uint32 unWidth, uint32 unHeight );

	// Destructor
	~CSource2UIRenderTargetTexture();

	// IUITexture interface
	virtual uint32 GetTextureWidth() { return m_nTextureWidth; }
	virtual uint32 GetTextureHeight() { return m_nTextureHeight; }
	virtual uint32 GetStride() { return 4; }
	virtual E2DTextureFormat GetFormat() { return k_EFormatRGBA8; }
	virtual EAlphaChannelType GetAlphaChannelType() { return k_EAlphaChannelType_Normal; }
	virtual bool BIsReady() { return m_hRenderTarget != RENDER_TEXTURE_HANDLE_INVALID; }

	HRenderTexture GetTextureHandle() { return m_hRenderTarget; }

private:

	uint32 m_nTextureWidth;
	uint32 m_nTextureHeight;
	
	HRenderTextureStrong m_hRenderTarget;
};

// A CSource2UITexture that will reference an engine render target
class CSource2UITextureEngineRTRef : public CRefCounted1< IUITexture, CRefCountServiceMT >
{
public:
	// Constructor
	CSource2UITextureEngineRTRef();

	// Destructor
	~CSource2UITextureEngineRTRef();

	void SetFromEngineRT( const char *pchEngineRTName );

	// IUITexture interface
	virtual uint32 GetTextureWidth() { return m_nTextureWidth; }
	virtual uint32 GetTextureHeight() { return m_nTextureHeight; }
	virtual uint32 GetStride() { return m_nTextureStride; }
	virtual E2DTextureFormat GetFormat() { return m_eFormat; }
	virtual EAlphaChannelType GetAlphaChannelType() { return m_eAlphaChannelType; }
	virtual bool BIsReady() { return m_hRenderTarget != RENDER_TEXTURE_HANDLE_INVALID; }

	HRenderTexture GetTextureHandle() { return m_hRenderTarget; }

private:

	void Reset();

	uint32 m_nTextureStride;
	uint32 m_nTextureWidth;
	uint32 m_nTextureHeight;
	E2DTextureFormat m_eFormat;
	EAlphaChannelType m_eAlphaChannelType;

	// A reference to the engine render target
	// Do not change to a HRenderTextureStrong as we do not want to go via
	// the resource system when destroying the texture
	HRenderTexture m_hRenderTarget;
};


} // namespace panorama

#endif // UIRENDERDEVICESOURCE2_H
