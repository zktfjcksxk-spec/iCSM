//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx.h"
#include "uirenderdevicesource2.h"
#include "uienginesource2.h"

#include "wrap_texture.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CUIRenderDeviceSource2::CUIRenderDeviceSource2()
{
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CUIRenderDeviceSource2::~CUIRenderDeviceSource2()
{
}

//-----------------------------------------------------------------------------
// Purpose: Called to create a texture, you call this directly on the main thread and the returned texture interface is thread safe,
// so you can access its id/size and delete it from the main thread as well.  Drawing calls are not synchronized with texture creation,
// but the contract is you must create the texture before attempting to draw for it's id.
//-----------------------------------------------------------------------------
bool CUIRenderDeviceSource2::BCreateTexture( IUITexture **pTextureOutput, void *pubTextureData, uint32 unWidth, uint32 unHeight, uint32 unStride, E2DTextureFormat eFormat, EAlphaChannelType eAlphaChannelType )
{
	CSource2UITexture *pTexture = *( CSource2UITexture** )pTextureOutput;
	if ( pTexture )
	{
		// We need data to update.
		if ( pubTextureData == NULL )
		{
			return false;
		}

		// update
		pTexture->SetTextureData( pubTextureData, unWidth, unHeight, unStride, eFormat, eAlphaChannelType );
		return pTexture->IsValid();
	}

	// create
	pTexture = new CSource2UITexture();
	// Data can be NULL when creating a new texture.
	pTexture->SetTextureData( pubTextureData, unWidth, unHeight, unStride, eFormat, eAlphaChannelType );
	if ( !pTexture->IsValid() )
	{
		pTexture->Release();
		return false;
	}

	*pTextureOutput = pTexture;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Called to create a texture
//-----------------------------------------------------------------------------
bool CUIRenderDeviceSource2::BCreateTexture( IUITexture **pTextureOutput, const char *pResourceFile )
{
	CSource2UITexture *pTexture = *( CSource2UITexture** )pTextureOutput;
	if ( pTexture )
	{
		// update
		pTexture->SetTextureData( pResourceFile );
		return pTexture->IsValid();
	}

	// create
	pTexture = new CSource2UITexture();
	pTexture->SetTextureData( pResourceFile );
	if ( !pTexture->IsValid() )
	{
		pTexture->Release();
		return false;
	}

	*pTextureOutput = pTexture;
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Called to create a double buffered texture, you call this directly on the man thread
// and the returned texture interface is thread safe, so you can update the texture data directly.  The textures are 
// double buffered, so it should be hard to block the render thread, but some locking does occur.  Unlike normal texture
// drawing your draw calls are not synchronized with texture data updates, so you could end up skipping frames or such.
//-----------------------------------------------------------------------------
bool CUIRenderDeviceSource2::BCreateDoubleBufferedTexture( IUIDoubleBufferedTexture **pDoubleBufferedTexture, uint32 unWidth, uint32 unHeight, uint32 unStride, E2DTextureFormat eFormat, EAlphaChannelType eAlphaChannelType, bool bSerializedUploads )
{
	// create
	CSource2DoubleBufferedTexture *pTexture = new CSource2DoubleBufferedTexture( unWidth, unHeight, unStride, eFormat, eAlphaChannelType );
	*pDoubleBufferedTexture = pTexture;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Called to create a double buffered YUV420 texture (for movie rendering), you call this directly on the main thread
// and the returned texture interface is thread safe, so you can update the texture data directly.  The textures are 
// double buffered, so it should be hard to block the render thread, but some locking does occur.  Unlike normal texture
// drawing your draw calls are not synchronized with texture data updates, so you could end up skipping frames or such.
//-----------------------------------------------------------------------------
bool CUIRenderDeviceSource2::BCreateDoubleBufferedYUV420Texture( IUIDoubleBufferedYUV420Texture **pDoubleBufferedYUV420Texture )
{
	// create
	*pDoubleBufferedYUV420Texture = new CSource2DoubleBufferedYUV420Texture();

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Called to create a texture that will reference an engine render target
//-----------------------------------------------------------------------------
bool CUIRenderDeviceSource2::BCreateTextureEngineRTRef( IUITexture **pTextureOutput, const char *pchEngineRTname )
{
	CSource2UITextureEngineRTRef *pTexture = *(CSource2UITextureEngineRTRef**)pTextureOutput;
	if ( pTexture )
	{
		// update
		pTexture->SetFromEngineRT( pchEngineRTname );
		return pTexture->BIsReady();
	}

	// create
	pTexture = new CSource2UITextureEngineRTRef();
	pTexture->SetFromEngineRT( pchEngineRTname );
	if ( !pTexture->BIsReady() )
	{
		pTexture->Release();
		return false;
	}

	*pTextureOutput = pTexture;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CUIRenderDeviceSource2::BCreateRenderTargetTexture( IUITexture **pTextureOutput, uint32 unWidth, uint32 unHeight )
{
	// create
	CSource2UIRenderTargetTexture *pTexture = new CSource2UIRenderTargetTexture( unWidth, unHeight );
	*pTextureOutput = pTexture;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CUIRenderDeviceSource2::PushPanelRT( const CRefPtr< IUITexture > &pRT )
{
	CSource2UIRenderTargetTexture *pSource2RT = (CSource2UIRenderTargetTexture *)(*&pRT);
	g_pRenderDevice->PushPanelRT( pSource2RT->GetTextureHandle() );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CUIRenderDeviceSource2::PopPanelRT( const CRefPtr< IUITexture > &pRT  )
{
	g_pRenderDevice->PopPanelRT();
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CSource2UIRenderTargetTexture::CSource2UIRenderTargetTexture( uint32 unWidth, uint32 unHeight )
:
	m_nTextureWidth( unWidth ),
	m_nTextureHeight( unHeight )
{
	CTextureCreationDesc specRT;
	specRT.m_nWidth = m_nTextureWidth;
	specRT.m_nHeight = m_nTextureHeight;
	specRT.m_nNumMipLevels = 1;
	specRT.m_nDepth = 1;
	specRT.m_nFlags = TSPEC_RENDER_TARGET | TSPEC_RENDER_TARGET_SAMPLEABLE | TSPEC_SUGGEST_CLAMPS | TSPEC_SUGGEST_CLAMPT;
	specRT.m_nImageFormat = IMAGE_FORMAT_RGBA8888;
	specRT.m_nMultisampleType = RENDER_MULTISAMPLE_NONE;
	specRT.m_Reflectivity.Init( 1, 1, 1 );
	specRT.m_nUsage = TEXTURE_USAGE_GPU_ONLY;

	specRT.m_nFlags |= TSPEC_RENDER_TARGET_WITHDS;

	m_hRenderTarget = g_pRenderDevice->FindOrCreateTexture( "panorama_rt_RENDERPANEL.vtex", true, &specRT );
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CSource2UIRenderTargetTexture::~CSource2UIRenderTargetTexture()
{
	Assert( GetRefCount() == 0 );

	if ( m_hRenderTarget != RENDER_TEXTURE_HANDLE_INVALID )
	{
		m_hRenderTarget.Shutdown();
		m_hRenderTarget = RENDER_TEXTURE_HANDLE_INVALID;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CSource2UITexture::CSource2UITexture()
{
	m_bTextureUploaded = false;

	m_nOriginalWidth = 0;
	m_nOriginalHeight = 0;

	m_nTextureWidth = 0;
	m_nTextureHeight = 0;

	m_nTextureStride = 0;
	m_eFormat = k_EFormatRGBA8;
	m_eAlphaChannelType = k_EAlphaChannelType_None;
	m_hRenderTexture = RENDER_TEXTURE_HANDLE_INVALID;

	m_flOriginalWidthScale = 0;
	m_flOriginalHeightScale = 0;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CSource2UITexture::~CSource2UITexture()
{
	Assert( GetRefCount() == 0 );

	if ( m_hRenderTexture != RENDER_TEXTURE_HANDLE_INVALID )
	{
		( ( CUIEngineSource2* )UIEngine() )->StopMonitoringResourceForChanges( m_hRenderTexture );

		m_hRenderTexture.Shutdown();
		m_hRenderTexture = RENDER_TEXTURE_HANDLE_INVALID;
	}

	m_bTextureUploaded = false;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CSource2UITexture::SetTextureData( void *pubTextureData, uint32 unWidth, uint32 unHeight, uint32 unStride, E2DTextureFormat eFormat, EAlphaChannelType eAlphaChannelType )
{
	m_nTextureWidth = m_nOriginalWidth = unWidth;
	m_nTextureHeight = m_nOriginalHeight = unHeight;
	m_nTextureStride = unStride;

	m_eFormat = eFormat;
	m_eAlphaChannelType = eAlphaChannelType;

	m_flOriginalWidthScale = 1.0f;
	m_flOriginalHeightScale = 1.0f;

#ifdef PANORAMA_USE_S1WRAPPER	

	// need a scaling fixup if we don't support POW2 textures
	// this needs to match the adjustment made to the S1 texture size - see  S1Wrapper_Texture_t::FillTextureDesc()
	if ( !( g_pMaterialSystemHardwareConfig->SupportsNPO2Textures() ) )
	{
		m_flOriginalWidthScale = m_nTextureWidth ? (float)m_nOriginalWidth / (float)SmallestPowerOfTwoGreaterOrEqual( unWidth ) : 1.0f;
		m_flOriginalHeightScale = m_nTextureHeight ? (float)m_nOriginalHeight / (float)SmallestPowerOfTwoGreaterOrEqual( unHeight ) : 1.0f;
	}
#endif

	int nDataStride = 0;
	ImageFormat imageFormat = IMAGE_FORMAT_UNKNOWN;
	switch ( m_eFormat )
	{
		case k_EFormatRGBA8:
			imageFormat = IMAGE_FORMAT_RGBA8888;
			nDataStride = m_nTextureWidth * 4;
			break;
		case k_EFormatBGRA8:
			imageFormat = IMAGE_FORMAT_BGRA8888;
			nDataStride = m_nTextureWidth * 4;
			break;
		case k_EFormatBGR8:
			imageFormat = IMAGE_FORMAT_BGRX8888;
			nDataStride = m_nTextureWidth * 4;
			break;
		case k_EFormatA8:
			imageFormat = IMAGE_FORMAT_A8;
			nDataStride = m_nTextureWidth;
			break;
		case k_EFormatYUV420:
			nDataStride = m_nTextureWidth;
			break;
		case k_EFormatR16G16B16A16:
			imageFormat = IMAGE_FORMAT_RGBA16161616;
			nDataStride = m_nTextureWidth * 8;
			break;
	}

	if ( imageFormat == IMAGE_FORMAT_UNKNOWN )
	{
		AssertMsg1( false, "CSource2UITexture(): Image format %d not supported.\n", eFormat );
		return;
	}

	CTextureCreationDesc textureDesc;
	textureDesc.m_nWidth = m_nTextureWidth;
	textureDesc.m_nHeight = m_nTextureHeight;
	textureDesc.m_nDepth = 1;
	textureDesc.m_nImageFormat = imageFormat;
	textureDesc.m_nNumMipLevels = 1;
	textureDesc.m_nMultisampleType = RENDER_MULTISAMPLE_NONE;
	textureDesc.m_nFlags = TSPEC_SUGGEST_CLAMPS | TSPEC_SUGGEST_CLAMPT | TSPEC_NO_LOD;
	textureDesc.m_Reflectivity.Init( 1.0f, 1.0f, 1.0f, 1.0f );
	textureDesc.m_nUsage = TEXTURE_USAGE_DYNAMIC;
	textureDesc.m_nScope = TEXTURE_SCOPE_GLOBAL;

	m_hRenderTexture = g_pRenderDevice->FindOrCreateTexture( "panorama_texture.vtex", true, &textureDesc, &textureDesc, pubTextureData, m_nTextureHeight * nDataStride );
	if ( m_hRenderTexture.IsValid() && m_hRenderTexture.IsLoaded() )
	{
		m_bTextureUploaded = true;
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CSource2UITexture::SetTextureData( const char *pResourceFile )
{
	m_bTextureUploaded = false;

	if ( !pResourceFile || !pResourceFile[ 0 ] )
		return;

	if ( m_hRenderTexture == RENDER_TEXTURE_HANDLE_INVALID )
	{
		m_hRenderTexture = g_pRenderDevice->FindOrCreateFileTexture( pResourceFile, LOADMODE_IMMEDIATE );
	}

	( ( CUIEngineSource2* )UIEngine() )->MonitorResourceForChanges( m_hRenderTexture );

	if ( m_hRenderTexture.IsValid() && m_hRenderTexture.IsLoaded() )
	{
		// has to be disk version since streamer will eventually provide that
		const CTextureDesc *pTextureDesc = g_pRenderDevice->GetOnDiskTextureDesc( m_hRenderTexture );

		m_nTextureWidth = m_nOriginalWidth = pTextureDesc->m_nWidth;
		m_nTextureHeight = m_nOriginalHeight = pTextureDesc->m_nHeight;
		m_nTextureStride = pTextureDesc->m_nWidth;

		if ( ( pTextureDesc->m_nDisplayRectWidth != 0 ) && ( pTextureDesc->m_nDisplayRectHeight != 0 ) )
		{
			m_nOriginalWidth = pTextureDesc->m_nDisplayRectWidth;
			m_nOriginalHeight = pTextureDesc->m_nDisplayRectHeight;
		}
		else
		{
			( ( CUIEngineSource2* )UIEngine() )->GetOriginalImageDimensions( pResourceFile, &m_nOriginalWidth, &m_nOriginalHeight );
		}

		// need a scaling fixup
		m_flOriginalWidthScale = m_nTextureWidth ? ( float )m_nOriginalWidth / ( float )m_nTextureWidth : 1.0f;
		m_flOriginalHeightScale = m_nTextureHeight ? ( float )m_nOriginalHeight / ( float )m_nTextureHeight : 1.0f;

		switch ( pTextureDesc->m_nImageFormat )
		{
			case IMAGE_FORMAT_RGBA8888:
				m_eFormat = k_EFormatRGBA8;
				break;
			case IMAGE_FORMAT_BGRA8888:
				m_eFormat = k_EFormatBGRA8;
				break;
			case IMAGE_FORMAT_BGRX8888:
				m_eFormat = k_EFormatBGR8;
				break;
			case IMAGE_FORMAT_A8:
				m_eFormat = k_EFormatA8;
				break;
			case IMAGE_FORMAT_RGBA16161616:
				m_eFormat = k_EFormatR16G16B16A16;
				break;
			case IMAGE_FORMAT_DXT1:
				m_eFormat = k_EFormatDXT1;
				break;
			case IMAGE_FORMAT_DXT5:
				m_eFormat = k_EFormatDXT5;
				break;
		}

		m_bTextureUploaded = true;
	}
	else
	{
		DbgAssertMsg( false, "Texture %s not already loaded/invalid", pResourceFile );
		Log_Warning( LOG_PANORAMA, "Panorama texture %s not already loaded/invalid.  Resource should have already loaded before this call.\n", pResourceFile );
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CSource2DoubleBufferedTexture::CSource2DoubleBufferedTexture( uint32 unWidth, uint32 unHeight, uint32 unStride, E2DTextureFormat eFormat, EAlphaChannelType eAlphaChannelType )
{
	m_nSerial = -1;

	m_pTexture = new CSource2UITexture();
	m_pTexture->SetTextureData( NULL, unWidth, unHeight, unStride, eFormat, eAlphaChannelType );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CSource2DoubleBufferedTexture::~CSource2DoubleBufferedTexture()
{
	m_nSerial = -1;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int32 CSource2DoubleBufferedTexture::UpdateTextureData( void *pTextureData )
{
	if ( pTextureData && m_pTexture )
	{
		m_pTexture->SetTextureData( pTextureData, GetTextureWidth(), GetTextureHeight(), GetStride(), GetFormat(), GetAlphaChannelType() );
		m_nSerial++;
	}

	return m_nSerial;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CSource2DoubleBufferedYUV420Texture::CSource2DoubleBufferedYUV420Texture()
	: m_unYBuffersByteCount( 0 )
	, m_unUVBuffersByteCount( 0 )
	, m_bTextureUploaded( false )
{
}

CThreadMutex CSource2DoubleBufferedYUV420Texture::s_bufferPoolMutex;

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CSource2DoubleBufferedYUV420Texture::~CSource2DoubleBufferedYUV420Texture()
{
	for ( SPixelDataBuffer *pBuffer : m_YBuffers )
		FreePixelDataBuffer( pBuffer );
	for ( SPixelDataBuffer *pBuffer : m_UVBuffers )
		FreePixelDataBuffer( pBuffer );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CSource2DoubleBufferedYUV420Texture::BUpdateTextureData( uint unWidth, uint unHeight, void *pYBuffer, void *pUBuffer, void *pVBuffer, uint unStrideY, uint unStrideU, uint unStrideV )
{
	STexture *pTextureToUpdate = nullptr;
	STexture tempTexture;

	// First, see if the size changed. If so, we need to update a new temp texture, and then swap it in at the end. 
	// If the size didn't change, we can just update in-place.
	{
		AUTO_LOCK( m_currentTextureMutex );
		if ( m_bTextureUploaded && m_currentTexture.unTextureWidth == unWidth && m_currentTexture.unTextureHeight == unHeight )
		{
			pTextureToUpdate = &m_currentTexture;
		}
		else
		{
			pTextureToUpdate = &tempTexture;
		}
	}

	// Do the real work of updating the texture without holding the lock the whole time
	bool bSuccess = BUpdateTextureDataInternal( *pTextureToUpdate, unWidth, unHeight, pYBuffer, pUBuffer, pVBuffer, unStrideY, unStrideU, unStrideV );

	// Now swap it back in if necessary
	if ( bSuccess && pTextureToUpdate == &tempTexture )
	{
		AUTO_LOCK( m_currentTextureMutex );
		m_currentTexture.Swap( tempTexture );
		m_bTextureUploaded = true;
	}

	return bSuccess;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CSource2DoubleBufferedYUV420Texture::BUpdateTextureDataInternal( STexture &texture, uint unWidth, uint unHeight, void *pYBuffer, void *pUBuffer, void *pVBuffer, uint unStrideY, uint unStrideU, uint unStrideV )
{
	texture.unTextureWidth = unWidth;
	texture.unTextureHeight = unHeight;

	// Y texture is full size
	int16 nYWidth = unWidth;
	int16 nYHeight = unHeight;
	uint32 unYBuffersByteCount = nYWidth * nYHeight;

	// UV texture is half size
	int16 nUVWidth = MAX( unWidth / 2, 1 );
	int16 nUVHeight = MAX( unHeight / 2, 1 );
	uint32 unUVBuffersByteCount = nUVWidth * nUVHeight;

	// If the size changed, then invalidate any buffers that we've stored
	{
		AUTO_LOCK( CSource2DoubleBufferedYUV420Texture::s_bufferPoolMutex );

		if ( m_unYBuffersByteCount != unYBuffersByteCount )
		{
			for ( SPixelDataBuffer *pBuffer : m_YBuffers )
				FreePixelDataBuffer( pBuffer );
			m_YBuffers.RemoveAll();
			m_unYBuffersByteCount = unYBuffersByteCount;
		}

		if ( m_unUVBuffersByteCount != unUVBuffersByteCount )
		{
			for ( SPixelDataBuffer *pBuffer : m_UVBuffers )
				FreePixelDataBuffer( pBuffer );
			m_UVBuffers.RemoveAll();
			m_unUVBuffersByteCount = unUVBuffersByteCount;
		}
	}

	if ( !texture.hRenderTextureY.IsValid() )
	{
		// Luminance component
		// Create as anonymous. Not on main thread during known resource system tolerant interval
		CTextureCreationDesc textureDesc;
		textureDesc.m_nWidth = nYWidth;
		textureDesc.m_nHeight = nYHeight;
		textureDesc.m_nDepth = 1;
		textureDesc.m_nImageFormat = IMAGE_FORMAT_I8;
		textureDesc.m_nNumMipLevels = 1;
		textureDesc.m_nMultisampleType = RENDER_MULTISAMPLE_NONE;
		textureDesc.m_nFlags = TSPEC_SUGGEST_CLAMPS | TSPEC_SUGGEST_CLAMPT | TSPEC_NO_LOD;
		textureDesc.m_Reflectivity.Init( 1.0f, 1.0f, 1.0f, 1.0f );
		textureDesc.m_nUsage = TEXTURE_USAGE_DYNAMIC;
		textureDesc.m_nScope = TEXTURE_SCOPE_GLOBAL;
		texture.hRenderTextureY = g_pRenderDevice->FindOrCreateTexture( "panorama_texture_y.vtex", true, &textureDesc );
	}

	if ( texture.hRenderTextureY.IsValid() )
	{
		// Luminance component
		// upload the procedural texture
		CTextureDesc dataDesc;
		dataDesc.m_nWidth = nYWidth;
		dataDesc.m_nHeight = nYHeight;
		dataDesc.m_nDepth = 1;
		dataDesc.m_nImageFormat = IMAGE_FORMAT_I8;
		dataDesc.m_nNumMipLevels = 1;

		SPixelDataBuffer *pDstBuffer = nullptr;
		{
			// hold this mutex when modifying m_YBuffers
			AUTO_LOCK( CSource2DoubleBufferedYUV420Texture::s_bufferPoolMutex );
			if ( m_YBuffers.IsEmpty() )
			{
				pDstBuffer = CreatePixelDataBuffer( unYBuffersByteCount );
			}
			else
			{
				pDstBuffer = m_YBuffers.Tail();
				m_YBuffers.Remove( m_YBuffers.Count() - 1 );
			}
		}
		CopyMemory3D( pDstBuffer->bufferData, pYBuffer,
			dataDesc.m_nWidth, dataDesc.m_nHeight, 1,
			unStrideY, unStrideY * dataDesc.m_nHeight,
			dataDesc.m_nWidth, dataDesc.m_nWidth * dataDesc.m_nHeight );

		AddRef(); // Corresponding release is Async in RecycleYBuffer
		DataRecycleDelegate_t recycleDelegate( this, &CSource2DoubleBufferedYUV420Texture::RecycleYBuffer );
		g_pRenderDevice->AsyncSetTextureData( texture.hRenderTextureY, &dataDesc, pDstBuffer->bufferData, unYBuffersByteCount, false, -1, NULL, 0, &recycleDelegate );
	}

	if ( !texture.hRenderTextureU.IsValid() )
	{
		// Chroma components half resolution
		CTextureCreationDesc textureDesc;
		textureDesc.m_nWidth = nUVWidth;
		textureDesc.m_nHeight = nUVHeight;
		textureDesc.m_nDepth = 1;
		textureDesc.m_nImageFormat = IMAGE_FORMAT_I8;
		textureDesc.m_nNumMipLevels = 1;
		textureDesc.m_nMultisampleType = RENDER_MULTISAMPLE_NONE;
		textureDesc.m_nFlags = TSPEC_SUGGEST_CLAMPS | TSPEC_SUGGEST_CLAMPT | TSPEC_NO_LOD;
		textureDesc.m_Reflectivity.Init( 1.0f, 1.0f, 1.0f, 1.0f );
		textureDesc.m_nUsage = TEXTURE_USAGE_DYNAMIC;
		textureDesc.m_nScope = TEXTURE_SCOPE_GLOBAL;

		texture.hRenderTextureU = g_pRenderDevice->FindOrCreateTexture( "panorama_texture_u.vtex", true, &textureDesc );
	}

	if ( texture.hRenderTextureU.IsValid() )
	{
		// Chroma components half resolution
		CTextureDesc dataDesc;
		dataDesc.m_nWidth = nUVWidth;
		dataDesc.m_nHeight = nUVHeight;
		dataDesc.m_nDepth = 1;
		dataDesc.m_nImageFormat = IMAGE_FORMAT_I8;
		dataDesc.m_nNumMipLevels = 1;

		SPixelDataBuffer *pDstBuffer = nullptr;
		{
			// hold this mutex when modifying m_UVBuffers
			AUTO_LOCK( CSource2DoubleBufferedYUV420Texture::s_bufferPoolMutex );
			if ( m_UVBuffers.IsEmpty() )
			{
				pDstBuffer = CreatePixelDataBuffer( unUVBuffersByteCount );
			}
			else
			{
				pDstBuffer = m_UVBuffers.Tail();
				m_UVBuffers.Remove( m_UVBuffers.Count() - 1 );
			}
		}
		CopyMemory3D( pDstBuffer->bufferData, pUBuffer,
			dataDesc.m_nWidth, dataDesc.m_nHeight, 1,
			unStrideU, unStrideU * dataDesc.m_nHeight,
			dataDesc.m_nWidth, dataDesc.m_nWidth * dataDesc.m_nHeight );

		AddRef(); // Corresponding release is Async in RecycleUVBuffer
		DataRecycleDelegate_t recycleDelegate( this, &CSource2DoubleBufferedYUV420Texture::RecycleUVBuffer );
		g_pRenderDevice->AsyncSetTextureData( texture.hRenderTextureU, &dataDesc, pDstBuffer->bufferData, unUVBuffersByteCount, false, -1, NULL, 0, &recycleDelegate );
	}

	if ( !texture.hRenderTextureV.IsValid() )
	{
		// Chroma components half resolution
		CTextureCreationDesc textureDesc;
		textureDesc.m_nWidth = nUVWidth;
		textureDesc.m_nHeight = nUVHeight;
		textureDesc.m_nDepth = 1;
		textureDesc.m_nImageFormat = IMAGE_FORMAT_I8;
		textureDesc.m_nNumMipLevels = 1;
		textureDesc.m_nMultisampleType = RENDER_MULTISAMPLE_NONE;
		textureDesc.m_nFlags = TSPEC_SUGGEST_CLAMPS | TSPEC_SUGGEST_CLAMPT | TSPEC_NO_LOD;
		textureDesc.m_Reflectivity.Init( 1.0f, 1.0f, 1.0f, 1.0f );
		textureDesc.m_nUsage = TEXTURE_USAGE_DYNAMIC;
		textureDesc.m_nScope = TEXTURE_SCOPE_GLOBAL;

		texture.hRenderTextureV = g_pRenderDevice->FindOrCreateTexture( "panorama_texture_v.vtex", true, &textureDesc );
	}

	if ( texture.hRenderTextureV.IsValid() )
	{
		// Chroma components half resolution
		CTextureDesc dataDesc;
		dataDesc.m_nWidth = nUVWidth;
		dataDesc.m_nHeight = nUVHeight;
		dataDesc.m_nDepth = 1;
		dataDesc.m_nImageFormat = IMAGE_FORMAT_I8;
		dataDesc.m_nNumMipLevels = 1;

		SPixelDataBuffer *pDstBuffer = nullptr;
		{
			// hold this mutex when modifying m_UVBuffers
			AUTO_LOCK( CSource2DoubleBufferedYUV420Texture::s_bufferPoolMutex );
			if ( m_UVBuffers.IsEmpty() )
			{
				pDstBuffer = CreatePixelDataBuffer( unUVBuffersByteCount );
			}
			else
			{
				pDstBuffer = m_UVBuffers.Tail();
				m_UVBuffers.Remove( m_UVBuffers.Count() - 1 );
			}
		}
		CopyMemory3D( pDstBuffer->bufferData, pVBuffer,
			dataDesc.m_nWidth, dataDesc.m_nHeight, 1,
			unStrideV, unStrideV * dataDesc.m_nHeight,
			dataDesc.m_nWidth, dataDesc.m_nWidth * dataDesc.m_nHeight );

		AddRef(); // Corresponding release is Async in RecycleUVBuffer
		DataRecycleDelegate_t recycleDelegate( this, &CSource2DoubleBufferedYUV420Texture::RecycleUVBuffer );
		g_pRenderDevice->AsyncSetTextureData( texture.hRenderTextureV, &dataDesc, pDstBuffer->bufferData, unUVBuffersByteCount, false, -1, NULL, 0, &recycleDelegate );
	}

	Assert( ( texture.hRenderTextureY.IsValid() && texture.hRenderTextureY.IsLoaded() ) && 
			( texture.hRenderTextureU.IsValid() && texture.hRenderTextureU.IsLoaded() ) && 
			( texture.hRenderTextureV.IsValid() && texture.hRenderTextureV.IsLoaded() ) );

	return true;
}

/*static*/ void CSource2DoubleBufferedYUV420Texture::CopyTextureData( void *pDestData, uint32 unWidth, uint32 unHeight, const void *pSourceData, uint unStride )
{
	// If the width matches the stride, we can do just a single memcpy for everything
	if ( unWidth == unStride )
	{
		V_memcpy( pDestData, pSourceData, unStride * unHeight );
		return;
	}

	// Width doesn't match the stride, so copy row by row
	const uint8 *pSourceBytes = ( const uint8 * )pSourceData;
	uint8 *pDestBytes = ( uint8 * )pDestData;

	for ( uint32 i = 0; i < unHeight; ++i )
	{
		V_memcpy( pDestBytes + ( i * unWidth ), pSourceBytes + ( i * unStride ), unWidth );
	}
}


void CSource2DoubleBufferedYUV420Texture::RecycleYBuffer( const void* pData )
{
	SPixelDataBuffer *pBuffer = GetPixelDataBufferFromBufferData( pData );

	{
		AUTO_LOCK( CSource2DoubleBufferedYUV420Texture::s_bufferPoolMutex );
		if ( m_YBuffers.Count() < m_YBuffers.NumAllocated() && pBuffer->unByteCount == m_unYBuffersByteCount )
		{
			m_YBuffers.AddToTail( pBuffer );
		}
		else
		{
			FreePixelDataBuffer( pBuffer );
		}
	}

	// We were holding a reference for the recycle delegate, so release that now
	Release();
}

void CSource2DoubleBufferedYUV420Texture::RecycleUVBuffer( const void *pData )
{
	SPixelDataBuffer *pBuffer = GetPixelDataBufferFromBufferData( pData );

	{
		AUTO_LOCK( CSource2DoubleBufferedYUV420Texture::s_bufferPoolMutex );
		if ( m_UVBuffers.Count() < m_UVBuffers.NumAllocated() && pBuffer->unByteCount == m_unUVBuffersByteCount )
		{
			m_UVBuffers.AddToTail( pBuffer );
		}
		else
		{
			FreePixelDataBuffer( pBuffer );
		}
	}

	// We were holding a reference for the recycle delegate, so release that now
	Release();
}

bool CSource2DoubleBufferedYUV420Texture::BIsReady()
{
	return m_bTextureUploaded;
}

uint32 CSource2DoubleBufferedYUV420Texture::GetTextureWidth()
{
	AUTO_LOCK( m_currentTextureMutex );
	return m_currentTexture.unTextureWidth;
}

uint32 CSource2DoubleBufferedYUV420Texture::GetTextureHeight()
{
	AUTO_LOCK( m_currentTextureMutex );
	return m_currentTexture.unTextureHeight;
}

void CSource2DoubleBufferedYUV420Texture::GetTextureSize( uint32 &unTextureWidth, uint32 &unTextureHeight )
{
	AUTO_LOCK( m_currentTextureMutex );
	unTextureWidth = m_currentTexture.unTextureWidth;
	unTextureHeight = m_currentTexture.unTextureHeight;
}

uint32 CSource2DoubleBufferedYUV420Texture::GetStride()
{
	return GetTextureWidth();
}

void CSource2DoubleBufferedYUV420Texture::GetCurrentTextureHandles( HRenderTexture &hRenderTextureY, HRenderTexture &hRenderTextureU, HRenderTexture &hRenderTextureV )
{
	AUTO_LOCK( m_currentTextureMutex );
	hRenderTextureY = m_currentTexture.hRenderTextureY;
	hRenderTextureU = m_currentTexture.hRenderTextureU;
	hRenderTextureV = m_currentTexture.hRenderTextureV;
}

void CSource2DoubleBufferedYUV420Texture::ClearCurrentTextureHandles()
{
	AUTO_LOCK( m_currentTextureMutex );
	m_currentTexture.Clear();
	m_bTextureUploaded = false;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CSource2UITextureEngineRTRef::CSource2UITextureEngineRTRef() : m_hRenderTarget( RENDER_TEXTURE_HANDLE_INVALID )
{
	Reset();
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CSource2UITextureEngineRTRef::~CSource2UITextureEngineRTRef()
{
	Reset();
}

//-----------------------------------------------------------------------------
// Purpose: Reference an engine render target
//-----------------------------------------------------------------------------
void CSource2UITextureEngineRTRef::SetFromEngineRT( const char *pchEngineRTName )
{
	Reset();

	if ( !pchEngineRTName || !pchEngineRTName[0] )
		return;

	m_hRenderTarget = g_pRenderDevice->CreateTextureFromEngineRT( pchEngineRTName );

	if ( m_hRenderTarget.IsValid() && m_hRenderTarget.IsLoaded() )
	{
		// has to be disk version since streamer will eventually provide that
		const CTextureDesc *pTextureDesc = g_pRenderDevice->GetTextureDesc( m_hRenderTarget );

		m_nTextureWidth = pTextureDesc->m_nWidth;
		m_nTextureHeight = pTextureDesc->m_nHeight;
		m_nTextureStride = pTextureDesc->m_nWidth;

		switch ( pTextureDesc->m_nImageFormat )
		{
		case IMAGE_FORMAT_RGBA8888:
			m_eFormat = k_EFormatRGBA8;
			break;
		case IMAGE_FORMAT_BGRA8888:
			m_eFormat = k_EFormatBGRA8;
			break;
		case IMAGE_FORMAT_RGB888:
			m_eFormat = k_EFormatRGB8;
			break;
		case IMAGE_FORMAT_BGRX8888:
			m_eFormat = k_EFormatBGR8;
			break;
		case IMAGE_FORMAT_A8:
			m_eFormat = k_EFormatA8;
			break;
		case IMAGE_FORMAT_RGBA16161616:
			m_eFormat = k_EFormatR16G16B16A16;
			break;
		case IMAGE_FORMAT_DXT1:
			m_eFormat = k_EFormatDXT1;
			break;
		case IMAGE_FORMAT_DXT5:
			m_eFormat = k_EFormatDXT5;
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Reference an engine render target
//-----------------------------------------------------------------------------
void CSource2UITextureEngineRTRef::Reset()
{
	if ( m_hRenderTarget != RENDER_TEXTURE_HANDLE_INVALID )
	{
		g_pRenderDevice->DestroyTextureFromEngineRT( m_hRenderTarget );
	}
	
	m_nTextureStride = 0;
	m_nTextureWidth = 0;
	m_nTextureHeight = 0;

	m_eFormat = k_EFormatRGBA8;
	m_eAlphaChannelType = k_EAlphaChannelType_None;
	m_hRenderTarget = RENDER_TEXTURE_HANDLE_INVALID;
}
