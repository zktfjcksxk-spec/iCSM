//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef D3D10SURFACE_H
#define D3D10SURFACE_H

#ifdef _WIN32
#pragma once
#endif

#include <WinSock2.h>
#include <windows.h>
#include "iui3dsurface.h"
#include "mathlib/vmatrix.h"
#include "tier1/utlsymbol.h"
#include "tier1/utlmap.h"
#include "tier1/utllinkedlist.h"
#include "../text/uitextlayoutwin32.h"
#include "../input/mousecursor.h"

#include "tier0/memdbgoff.h"
#include <d3dcommon.h>
#include <d3d10_1.h>
#include <d2d1.h>
#include <DWrite.h>
#include "tier0/memdbgon.h"

#include "dwritetextrenderer.h"
#include "uitoplevelwindowoverlay.h"

//#define _NVPERFKIT 1
#ifdef _NVPERFKIT

/// Generic unsigned data types, 8-64 bits
#include <stdint.h>
typedef uint64_t NVPMUINT64;

/// Context from NVPMAPI mapping back to the original API specific device/context
typedef NVPMUINT64 NVPMContext;

#endif

namespace panorama
{

#define VERTEX_BUFFER_FLUSH_SIZE 60*4
#define TOTAL_VERTEX_BUFFER_SIZE 360*4

// Vertex struct format
struct VertexTextured_t
{
	float x, y, z, rhw;
	float r, g, b, a;
	float u, v; // texture coordinates
	float masku1, maskv1;
	float masku2, maskv2;
};

// Forward declaration
class CD3D10D2DSurface;
class CDWriteTextRenderer;

class CCompositionLayer
{
public:

	// Constructor 
	CCompositionLayer( CD3D10D2DSurface *pParentSurface, ID3D10Texture2D *pD3DSurface, ID2D1RenderTarget *pD2DRenderTarget, ID3D10RenderTargetView *pRenderTargetView, ID3D10ShaderResourceView *pShaderResourceView,
					   float width, float height );
	~CCompositionLayer();

	// Is the layer in drawing mode?
	bool BIsDrawing() { return m_bIsDrawing; }

	// Set a context id for the layer
	void SetContextID( uint64 ulContextID ) { m_ulContextID = ulContextID; }

	// Get the context id for the layer
	uint64 GetContextID() { return m_ulContextID; }

	// Get width
	float GetWidth() { return m_flLayerWidth; }
	
	// Get height
	float GetHeight() { return m_flLayerHeight; }

	// Used to tell the layer it's surfaces width/height have changed underneath it, only occurs for backbuffers
	void ModifyWidthAndHeight( float flWidth, float flHeight )
	{
		m_flLayerWidth = flWidth;
		m_flLayerHeight = flHeight;
	}

	// Access D3D backing texture, no addref
	ID3D10Texture2D * AccessSurface() { Assert( !m_bIsDrawing && !m_bBeginDrawDone ); return m_pOffscreenSurface; }

	// Access D2D render target, no addref
	ID2D1RenderTarget * AccessRenderTarget() { DoDelayedBeginDrawIfNeeded(); return m_pD2DRenderTarget; }

	// Access D2D render target, but promise you won't draw, no addref
	ID2D1RenderTarget * AccessRenderTargetNoDrawing() { return m_pD2DRenderTarget; }

	// Access render target view, no addref
	ID3D10RenderTargetView * AccessRenderTargetView() { Assert( !m_bIsDrawing && !m_bBeginDrawDone ); return m_pRenderTargetView; }

	// Access shader resource view, no addref
	ID3D10ShaderResourceView * AccessShaderResourceView() { Assert( !m_bIsDrawing && !m_bBeginDrawDone ); return m_pShaderResourceView; }

	// Get last time used
	double GetTimeLastUsed() { return m_flLastTimeUsed; }

	// Get gaussian blur std deviation
	void GetBlurValues( float &flPasses, float &flStdDevHor, float &flStdDevVer ) 
	{ 
		flPasses = m_flBlurPasses;
		flStdDevHor = m_flBlurStdDevHor;
		flStdDevVer = m_flBlurStdDevVer;
	}

	// Set gaussian blur std deviation
	void SetBlurValues( float flPasses, float flStdDevHor, float flStdDevVer ) 
	{ 
		m_flBlurPasses = flPasses; 
		m_flBlurStdDevHor = flStdDevHor;
		m_flBlurStdDevVer = flStdDevVer;
	}

	// Update LRU/time last used on access
	void UpdateTimeLastUsedAndLRUListForAccess( double flTime, CUtlLinkedList< int, int > &list );

	// Check clip layer vec is empty, clear if needed
	void CheckAndClearClipLayers();

	// Push clip layers and begin drawing for layer
	void PushCliplayersAndBeginDraw( float flScaleX, float flScaleY, float flTranslateX, float flTranslateY );

	// Pop clip layers and end drawing for layer
	void PopClipLayersAndFlush();

	// Draw the border for the layer
	void DrawBorder( CD3D10D2DSurface *pBaseSurface );

	// Access render quad info
	VertexTextured_t *AccessRenderQuad() { return m_RenderQuad; }

	// Access matrix data
	float *AccessMatrix() { return m_flMatrix; }

	// Set saturation
	void SetSaturation( float saturation ) { m_flSaturation = saturation; }

	// Get desaturation value
	float GetSaturation() { return m_flSaturation; }

	// Set brightness
	void SetBrightness( float brightness ) { m_flBrightness = brightness; }

	// Get brightness value
	float GetBrightness() { return m_flBrightness; }

	// Set HueShift
	void SetHueShift( float hueshift ) { m_flHueShift = hueshift; }

	// Get HueShift value
	float GetHueShift() { return m_flHueShift; }

	// Set contrast
	void SetContrast( float contrast ) { m_flContrast = contrast; }

	// Get contrast value
	float GetContrast() { return m_flContrast; }

	// Set opacity mask textureid
	void SetOpacityMaskTextureID( uint32 unTextureID, float flOpacity ) { m_unOpacityMaskTextureID = unTextureID; m_flOpacityMaskOpacity = flOpacity; }

	// Access opacity mask textureid
	uint32 GetOpacityMaskTextureID() { return m_unOpacityMaskTextureID; }
	float GetOpacityMaskOpacity() { return m_flOpacityMaskOpacity; }

	// Set corner rounding data for the layer
	void SetCornerRadii( float flTopLeftHorizontal, float flTopLeftVertical, float flTopRightHorizontal, float flTopRightVertical,
		float flBottomRightHorizontal, float flBottomRightVertical, float flBottomLeftHorizontal, float flBottomLeftVertical )
	{
		m_rgCornerRadii[0] = flTopLeftHorizontal;
		m_rgCornerRadii[1] = flTopLeftVertical;
		m_rgCornerRadii[2] = flTopRightHorizontal;
		m_rgCornerRadii[3] = flTopRightVertical;
		m_rgCornerRadii[4] = flBottomRightHorizontal;
		m_rgCornerRadii[5] = flBottomRightVertical;
		m_rgCornerRadii[6] = flBottomLeftHorizontal;
		m_rgCornerRadii[7] = flBottomLeftVertical;
	}

	void SetBorder( float flTopWidth, float flRightWidth, float flBottomWidth, float flLeftWidth, uint32 rgbaTop, uint32 rgbaRight, uint32 rgbaBottom, uint32 rgbaLeft )
	{
		m_rgBorderWidths[0] = flTopWidth;
		m_rgBorderWidths[1] = flRightWidth;
		m_rgBorderWidths[2] = flBottomWidth;
		m_rgBorderWidths[3] = flLeftWidth;

		m_rgbaBorderColors[0] = rgbaTop;
		m_rgbaBorderColors[1] = rgbaRight;
		m_rgbaBorderColors[2] = rgbaBottom;
		m_rgbaBorderColors[3] = rgbaLeft;
	}

	void GetBorderWidths( float &flTopWidth, float &flRightWidth, float &flBottomWidth, float &flLeftWidth )
	{
		flTopWidth = m_rgBorderWidths[0];
		flRightWidth = m_rgBorderWidths[1];
		flBottomWidth = m_rgBorderWidths[2];
		flLeftWidth = m_rgBorderWidths[3];
	}

	void SetBoxShadow( bool bInset, bool bFill, float flHorOffset, float flVerOffset, float flBlurRadius, float flSpreadDistance, uint32 rgbaShadowColor, bool bAnimatingBoxShadow )
	{
		m_bBoxShadowInset = bInset;
		m_bBoxShadowFill = bFill;
		m_flBoxShadowHorOffset = flHorOffset;
		m_flBoxShadowVerOffset = flVerOffset;
		m_flBoxShadowBlurRadius = flBlurRadius;
		m_flBoxShadowSpreadDistance = flSpreadDistance;
		m_rgbaBoxShadowColor = rgbaShadowColor;
		m_bAnimatingBoxShadow = bAnimatingBoxShadow;
	}

	void GetBoxShadow( bool &bInset, bool &bFill, float &flHorOffset, float &flVerOffset, float &flBlurRadius, float &flSpreadDistance, uint32 &rgbaShadowColor, bool &bAnimatingBoxShadow )
	{
		bInset = m_bBoxShadowInset;
		bFill = m_bBoxShadowFill;
		flHorOffset = m_flBoxShadowHorOffset;
		flVerOffset = m_flBoxShadowVerOffset;
		flBlurRadius = m_flBoxShadowBlurRadius;
		flSpreadDistance = m_flBoxShadowSpreadDistance;
		rgbaShadowColor = m_rgbaBoxShadowColor;
		bAnimatingBoxShadow = m_bAnimatingBoxShadow;
	}

	void Set2DScaleFactors( float x, float y )
	{
		m_flScale2D[0] = x;
		m_flScale2D[1] = y;
	}

	void Get2DScaleFactors( float &x, float &y )
	{
		x = m_flScale2D[0];
		y = m_flScale2D[1];
	}

	void Get2DRotate( float &fl2DRotate )
	{
		fl2DRotate = m_flRotate2D;
	}

	void Set2DRotate( float flRotate2D )
	{
		m_flRotate2D = flRotate2D;
	}

	// Access corner rounding data for the layer
	float *AccessCornerRadii() { return m_rgCornerRadii; }

	// Access border width data for the layer
	float *AccessBorderWidths() { return m_rgBorderWidths; }

	// Push a new clip layer into this composition layer
	void PushClipLayer( const CMsgPushClipLayer &msg );

	// Pop a clip layer out of the composition layer
	void PopClipLayer();

	// Get the current clip rect
	void GetCurrentClipRect( D3D10_RECT &r );

	// Get current clip layer count for composition layer
	uint32 GetClipLayerCount();

	// Set LRU list position
	void SetLRUPos( int iListIndex ) { m_iLRUPos = iListIndex; }

	// Draws an inset shadow (unblurred) into the layer
	void DrawInsetShadowIntoLayer( CD3D10D2DSurface *pBaseSurface, float flPadding, float flWidth, float flHeight, float flHorOffset, float flVerOffset, float flSpreadDistance, uint32 shadowColor, float *pflInnerRadii );

	// Get the d2d render target for the layer
	ID2D1RenderTarget *GetD2DRenderTarget() { return m_pD2DRenderTarget; }

	// Sort on size only
	bool operator <( const CCompositionLayer &l ) const
	{
		if ( m_flLayerWidth < l.m_flLayerWidth )
			return true;
		else if ( m_flLayerWidth > l.m_flLayerWidth )
			return false;

		return m_flLayerHeight < l.m_flLayerHeight;
	}

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName );
#endif
private:

	void DoDelayedBeginDrawIfNeeded();

	struct ClipLayerData_t
	{
		float x0, x1;
		float y0, y1;
	};

	// Helpers for actual push/pop of d2d layers
	void PushLayerNow( const ClipLayerData_t &clipLayer );
	void PopLayerNow( const ClipLayerData_t &clipLayer );

	// Ptr to parent surface
	CD3D10D2DSurface *m_pParentSurface;

	// Do we think we should be allowed to draw now?
	bool m_bIsDrawing;

	// Have we actually called BeginDraw?
	bool m_bBeginDrawDone;

	// context id for the layer
	uint64 m_ulContextID;

	// Size of the layer
	float m_flLayerWidth;
	float m_flLayerHeight;

	// Transform matrix for the layer
	float m_flMatrix[16];

	// Corner rounding data for the layer
	float m_rgCornerRadii[8];

	// Border data
	float m_rgBorderWidths[4];
	uint32 m_rgbaBorderColors[4];

	// Desaturation for the layer
	float m_flSaturation;
	float m_flHueShift;
	float m_flContrast;
	float m_flBrightness;

	// Gaussian blur std deviation
	float m_flBlurPasses;
	float m_flBlurStdDevHor;
	float m_flBlurStdDevVer;

	// box shadow data
	bool m_bBoxShadowInset;
	bool m_bBoxShadowFill;
	float m_flBoxShadowHorOffset;
	float m_flBoxShadowVerOffset;
	float m_flBoxShadowBlurRadius;
	float m_flBoxShadowSpreadDistance;
	uint32 m_rgbaBoxShadowColor;
	bool m_bAnimatingBoxShadow;


	// Scale/translate for the enitre layer
	float m_flScaleLayerX;
	float m_flScaleLayerY;
	float m_flTranslateLayerX;
	float m_flTranslateLayerY;

	// Opacity mask texture id for the layer
	uint32 m_unOpacityMaskTextureID;
	float m_flOpacityMaskOpacity;

	// Clip layers for the layer
	CUtlVector< ClipLayerData_t > *m_pVecClipLayers;

	// index into LRU list position
	int m_iLRUPos;

	// D3D surface for the layer
	ID3D10Texture2D *m_pOffscreenSurface;

	// D2D render target for the layer
	ID2D1RenderTarget *m_pD2DRenderTarget;

	// Render target view for the layer
	ID3D10RenderTargetView *m_pRenderTargetView;

	// Shader resource view for the texture
	ID3D10ShaderResourceView *m_pShaderResourceView;

	// Last time this layer was used, so we can cleanup stale ones that are not getting used.
	double m_flLastTimeUsed;

	// Quad to use when rendering to parent layer
	VertexTextured_t m_RenderQuad[4];

	// 2D scaling factors to apply centered around untransformed object
	float m_flScale2D[2];

	// 2d rotation to apply cenetered around untransformed object
	float m_flRotate2D;

};

class ID3D10UITexture
{
public:
	virtual ~ID3D10UITexture() {}

	// These may or may not do locking, depending on the implementation (they only do for double buffered 
	// textures currently), but they must be implemented to provide safety such that while locked the 
	// caller on the render thread can call the various access functions and not worry about data changing 
	// underneath them due to actions of the owning thread.
	virtual int LockAndGetCurrentTexture( ID3D10Texture2D **ppTexture, ID3D10ShaderResourceView**ppResourceView, int32 nSerial  ) = 0;
	virtual void Unlock( int iLockHandle ) = 0;

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName ) = 0;
#endif

};

class CD3D10Texture : public IUITexture, public ID3D10UITexture
{
public:

	// Constructor
	CD3D10Texture( ID3D10Device *pDevice, uint32 unTextureID, void *pubTextureData, uint32 unWidth, uint32 unHeight, uint32 unStride, E2DTextureFormat eFormat, EAlphaChannelType eAlphaChannelType );

	// Destructor
	virtual ~CD3D10Texture();

	// IUITexture interface
	virtual uint32 GetTextureID() { return m_unTextureID; }
	virtual uint32 GetTextureWidth() { return m_unWidth; }
	virtual uint32 GetTextureHeight() { return m_unHeight; }
	virtual uint32 GetStride() { return m_unStride; }
	virtual E2DTextureFormat GetFormat() { return m_eFormat; }
	virtual EAlphaChannelType GetAlphaChannelType() { return m_eAlphaChannelType; }
	virtual bool BIsReady() { return true; }

	// ID3D10UITexture interface
	virtual int LockAndGetCurrentTexture( ID3D10Texture2D **ppTexture, ID3D10ShaderResourceView **ppResourceView, int nSerial )
	{
		REFERENCE( nSerial ); // ignore serial numbers and do immediate uploads for normal textures
		if ( ppTexture )
			*ppTexture = m_pTexture;
		if ( ppResourceView )
			*ppResourceView = m_pTextureView;
		return -1;
	}
	virtual void Unlock( int iLockHandle ) { }


#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName );
#endif

private:

	uint32 m_unStride;
	uint32 m_unTextureID;
	uint32 m_unWidth;
	uint32 m_unHeight;
	E2DTextureFormat m_eFormat;
	EAlphaChannelType m_eAlphaChannelType;

	ID3D10Texture2D *m_pTexture;
	ID3D10ShaderResourceView *m_pTextureView;
};

//
// Implementation of a double buffered texture for efficient hardware 
// accelerated handling of frequently updated textures via double buffering.
//
class CD3D10DoubleBufferedTexture : public IUIDoubleBufferedTexture, public ID3D10UITexture
{
public:

	// Constructor
	CD3D10DoubleBufferedTexture( ID3D10Device *pDevice, uint32 unTextureID, uint32 unTextureWidth, uint32 unTextureHeight, uint32 unStride, E2DTextureFormat eFormat, EAlphaChannelType eAlphaChannelType, bool bSerializedUploads );

	// Destructor
	virtual ~CD3D10DoubleBufferedTexture();

	// IUIDoubleBufferedTexture interface
	virtual int32 UpdateTextureData( void *pBuffer );
	virtual uint32 GetTextureID() { return m_unTextureID; }
	virtual uint32 GetTextureWidth() { return m_unWidth; }
	virtual uint32 GetTextureHeight() { return m_unHeight; }
	virtual uint32 GetStride() { return m_unStride; }
	virtual E2DTextureFormat GetFormat() { return m_eFormat; }
	virtual EAlphaChannelType GetAlphaChannelType() { return m_eAlphaChannelType; }
	virtual bool BIsReady() { return true; }

	// ID3D10UITexture interface
	virtual int LockAndGetCurrentTexture( ID3D10Texture2D **ppTexture, ID3D10ShaderResourceView **ppResourceView, int nSerial );
	virtual void Unlock( int iLockHandle );

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName );
#endif

private:

	uint32 m_unStride;
	uint32 m_unTextureID;
	uint32 m_unWidth;
	uint32 m_unHeight;
	int32 m_nSerial; // when not using immediate mode the unique value of this uploaded texture, ties back to drawrect calls with a serial
	int32 m_nDrawSerial;
	E2DTextureFormat m_eFormat;
	EAlphaChannelType m_eAlphaChannelType;
	int32 m_iPendingUpload; // the texture index of the next slot to upload into
	CThreadEvent m_DrawEvent; // used to wake the main thread if its blocking for an upload
	bool m_bSerializedUploads;

	struct Texture_t
	{
		// Not a spin lock, because if the provider thread (movies) renders
		// faster than we render in the render thread it will block on this,
		// which is fine and essentially sleep time, but we shouldn't spin.
		CThreadMutex m_Lock;
		int32 m_nSerial;

		ID3D10Texture2D *m_pTexture;
		ID3D10ShaderResourceView *m_pTextureView;
	};

	Texture_t m_rgTextures[2];

	// Tracking and locking for which texture should currently be used for rendering
	CThreadSpinLock m_IndexLock;
	int32 m_iCurRenderTexture;
};


//
// Implementation of a double buffered YUV420 texture for efficient hardware 
// accelerated handling of this common format for movies.  This actually creates
// 6 dxgi textures, 2 pairs of Y, U, and V.  A pixel shader is then needed to specially
// render with the 3 component textures and convert YUV to RGBA.
//
class CD3D10DoubleBufferedYUV420Texture : public IUIDoubleBufferedYUV420Texture, public ID3D10UITexture
{
public:

	// Constructor
	CD3D10DoubleBufferedYUV420Texture( CD3D10D2DSurface *pSurface, ID3D10Device *pDevice, uint32 unTextureID, uint32 unTextureWidth, uint32 unTextureHeight );

	// Destructor
	virtual ~CD3D10DoubleBufferedYUV420Texture();

	// IUIDoubleBufferedYUV420Texture interface
	virtual bool BUpdateTextureData( void *pYBuffer, void *pUBuffer, void *pVBuffer, uint unStrideY, uint unStrideU, uint unStrideV );
	virtual uint32 GetTextureID() { return m_unTextureID; }
	virtual uint32 GetTextureWidth() { return m_unWidth; }
	virtual uint32 GetTextureHeight() { return m_unHeight; }
	virtual uint32 GetStride() { return m_unTextureStride; }
	virtual E2DTextureFormat GetFormat() { return k_EFormatYUV420; }
	virtual EAlphaChannelType GetAlphaChannelType() { return k_EAlphaChannelType_None; }
	virtual bool BIsReady() { return true; }

	// ID3D10UITexture interface
	virtual int LockAndGetCurrentTexture( ID3D10Texture2D **ppTexture, ID3D10ShaderResourceView **ppResourceView, int nSerial ) 
	{ 
		AssertMsg( false, "Must use LockAndGetCurrentTextures (plural) on YUV textures" ); 
		return -1; 
	}
	virtual void Unlock( int iLockHandle );

	// Special lock function for YUV textures
	int LockAndGetCurrentTextures( ID3D10ShaderResourceView **ppYView, ID3D10ShaderResourceView **ppUView, ID3D10ShaderResourceView **ppVView );

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName ) OVERRIDE;
#endif

private:

	uint32 m_unTextureStride;
	uint32 m_unTextureID;
	uint32 m_unWidth;
	uint32 m_unHeight;

	struct YUV420Texture_t
	{
		// Not a spin lock, because if the provider thread (movies) renders
		// faster than we render in the render thread it will block on this,
		// which is fine and essentially sleep time, but we shouldn't spin.
		CThreadMutex m_Lock;

		ID3D10Texture2D *m_pYTexture;
		ID3D10Texture2D *m_pUTexture;
		ID3D10Texture2D *m_pVTexture;

		ID3D10ShaderResourceView *m_pYTextureView;
		ID3D10ShaderResourceView *m_pUTextureView;
		ID3D10ShaderResourceView *m_pVTextureView;
	};

	YUV420Texture_t m_rgTextures[2];

	// Tracking and locking for which texture should currently be used for rendering
	CThreadSpinLock m_IndexLock;
	int m_iCurRenderTexture;

	double m_flLastRenderThreadFrameTimeOnUpdate;
	CD3D10D2DSurface *m_pSurface;

	friend class CD3D10D2DSurface;
};


//
// Implementation of IUI3DSurface in D3D10 + D2D.  D2D is used for most 2d drawing
// and gives us good text rendering, elliptical paths/corners, etc.  D3D is used for
// compositing of 2D panels in 3D space and gives us shaders for effects (ie, gaussian blur, 
// desaturation, etc)  efficiently on the GPU.
//
class CD3D10D2DSurface : public IUI3DSurface, public IUITextTextureStorage, public IUITextTextureProvider
{
public:

	CD3D10D2DSurface();
	~CD3D10D2DSurface();

	bool BInitialize( HWND hWnd, int nSurfaceWidth, int nSurfaceHeight, int nWindowWidth, int nWindowHeight, IUIEngine::ERenderTarget eRenderType, bool bEnforceAspectRatio, bool bFixedSurfaceSize, CMouseCursorRender *pCursorRender );

	bool BUpdateRenderStateIfNeeded( IUIEngine::ERenderTarget eMsgRenderTarget );
	bool BUpdateWindowSizeIfNeeded( uint32 nWidth, uint32 nHeight );
	bool BBackBufferScalingNeeded() const { return m_unWindowHeight != m_unSurfaceHeight || m_unWindowWidth != m_unSurfaceWidth;}

	uint32 GetSurfaceWidth() const { return m_unSurfaceWidth; }
	uint32 GetSurfaceHeight() const { return m_unSurfaceHeight; }
	uint32 GetWindowWidth() const { return m_unWindowWidth; }
	uint32 GetWindowHeight() const { return m_unWindowHeight; }
	IUIEngine::ERenderTarget GetRenderTarget() const { return m_eRenderTarget; }

	IDXGISwapChain *AccessSwapChain() const { return m_pDXGISwapChain; }
	ID3D10Device1 *AccessDevice() const { return m_pD3D10Device; }
	ID3D10Texture2D *AccessBackBuffer() const { return m_pBackBuffer; }
	ID2D1RenderTarget *AccessD2DRenderTarget() const { return m_pD2DRenderTarget; }
	ID3D10RenderTargetView *AccessD3DRenderTargetView() const { return m_pRenderTargetView; }

	ID2D1Factory *AccessD2D1Factory() const { return m_pD2DFactory; }
	IDXGIFactory1 *AccessDXGIFactory() const { return m_pDXGIFactory; }

	IDWriteRenderingParams *AccessDWriteRenderingParams() const { return m_pDWriteRenderingParams; }

	HWND GetHWND() const { return m_hWnd; }
	void SetHWND( HWND hwnd );

#ifdef PANORAMA_STEAMUI_STREAMING_CAPTURE_WIN32
	void SetSteamUIStreamingCaptureCallback( SteamUIStreamingCaptureCallback_t pCallback ) { m_pSteamUIStreamingCallback = pCallback; }
#endif

	vr::VROverlayHandle_t GetVROverlayHandle() const { return m_ulVROverlayHandle;  }
	void SetVROverlayHandle( vr::VROverlayHandle_t ulVROverlayHandle );

	int GetCompositionLayerStackSize() { return m_stackCompositionLayers.Count(); }

	// IUITextTextureStorage
	virtual uint32 GetMaximumTextureWidth() OVERRIDE;
	virtual uint32 GetMaximumTextureHeight() OVERRIDE;
	virtual UITextTextureRegion_t GetTextureRegion( int32 iWidth, int32 iHeight ) OVERRIDE;
	virtual void StartUpdateFontGlyphTexture( UITextTextureHandle_t hTexture ) OVERRIDE;
	virtual void UpdateFontGlyphTexture( UITextTextureHandle_t hTexture, int xOffset, int yOffset, int width, int height, void *pSourceData ) OVERRIDE;
	virtual void EndUpdateFontGlyphTexture( UITextTextureHandle_t hTexture ) OVERRIDE;

	// IUITextTextureProvider.
	virtual UITextTextureHandle_t AllocAlphaTexture( int32 iWidth, int32 iHeight ) OVERRIDE;
	virtual void FreeTexture( UITextTextureHandle_t hTexture ) OVERRIDE;

	// IUI3DSurface implementation
	virtual double GetLastFramePaintTime() { return m_flLastPaintFrameTime; }

	virtual void BeginFrame( const CRenderMsg<CMsgBeginFrame> &renderCommand );
	virtual void EndFrame( const CRenderMsg<CMsgEndFrame> &renderCommand );
	virtual void ClearBackbuffer( const CRenderMsg<CMsgClearBackbuffer> &renderCommand );
	virtual void DrawFilledRect( const CRenderMsg<CMsgRenderFilledRect> &renderCommand );
	virtual void DrawTextRegion( const CRenderMsg<CMsgRenderTextRegion> &renderCommand );
	virtual void PushCompositingLayer( const CRenderMsg<CMsgPushCompositingLayer> &renderCommand );
	virtual void PopCompositingLayer( const CRenderMsg<CMsgPopCompositingLayer> &renderCommand );
	virtual void PushClipLayer( const CRenderMsg<CMsgPushClipLayer> &renderCommand );
	virtual void PopClipLayer( const CRenderMsg<CMsgPopClipLayer> &renderCommand );
	virtual void PushPanelContextInLayer( const CRenderMsg<CMsgPushPanelContextInLayer> &renderCommand ) { Assert( false ); }
	virtual void PopPanelContextInLayer( const CRenderMsg<CMsgPopPanelContextInLayer> &renderCommand ) { Assert( false ); }

	virtual bool BCreateTexture( IUITexture **pTextureOutput, void *pubTextureData, uint32 unWidth, uint32 unHeight, uint32 unStride, E2DTextureFormat eFormat, EAlphaChannelType eAlphaChannelType );
	virtual bool BCreateDoubleBufferedTexture( IUIDoubleBufferedTexture **pDoubleBufferedOutput, uint32 unWidth, uint32 unHeight, uint32 unStride, E2DTextureFormat eFormat, EAlphaChannelType eAlphaChannelType,  bool bSerializedUploads );
	virtual bool BCreateDoubleBufferedYUV420Texture( IUIDoubleBufferedYUV420Texture **pDoubleBufferedYUV420Output, uint32 unWidth, uint32 unHeight );
	virtual bool BDeleteTexture( IUITexture *pTexture );
	virtual void DrawTexturedRect( const CRenderMsg<CMsgRenderTexturedRect> &renderCommand );
	virtual void LockTexture( const CRenderMsg<CMsgLockTexture> &renderCommand );
	virtual void FreeCompositingLayer( const CRenderMsg<CMsgFreeCompositingLayer> &renderCommand );

	virtual bool PingCompositingLayer( uint64 ulLayerID, float flWidth, float flHeight );

// Handle render callback request (basically calls back raw panel code to draw direct to surface, source 2 only noop elsewhere)
	virtual void RequestRenderCallback( const CRenderMsg<CMsgRequestRenderCallback> &renderCommand ) { }

	virtual float GetFPSAverage();
	virtual float GetSessionFPSAverages();

	virtual void UpdateSteamPadPointers( SteamPadPointer_t* leftPointer, SteamPadPointer_t* rightPointer )
	{
		if ( leftPointer )
			m_leftSteamPadPointer = *leftPointer;
		if ( rightPointer )
			m_rightSteamPadPointer = *rightPointer;
	}

	virtual bool BVsyncEnabled() { return m_bVsyncEnabled; }

	virtual bool BCursorVisible()
	{
		if ( m_pCursorRender )
			return m_pCursorRender->BCursorVisible();

		return true;
	}

	virtual void WakeupMouseCursor()
	{
		if ( m_pCursorRender )
			m_pCursorRender->WakeupMouseCursor();

		return;
	}

	virtual void FadeOutCursorNow()
	{
		if ( m_pCursorRender )
			m_pCursorRender->FadeOutCursorNow();

		return;
	}

	virtual void SetOverlayLetterboxColor( Color c ) { m_cLetterBoxColor = c; }

	// uiengine telling us a file on disk changed
	void ReloadChangedFile( const char *pchFile );

	void ReleaseDXResources();

	virtual uint64 GetMinFrameTimeInMicroseconds()
	{
		if ( !IUIEngine::BIsRenderingToTexture( GetRenderTarget() ) )
		{
			// 120fps for normal surfaces
			return 8333;
		}
		else
		{
			// 60fps for overlay to-texture surfaces
			return 16666.6666666;
		}
	}

	// If returns true, there's nothing to draw, there's no visibility to the frame
	// right now.
	virtual bool BSurfaceOccluded();

	// Should only be called on overlay render-to-texture instances, tells 
	// us to push the overlay render cmd stream necessary to draw our content.  This is 
	// called from the main thread outside the render thread.
	void PushOverlayRenderCmdStream( CSharedMemStream *pRenderStream, DWORD dwPID, float flOpacity, uint32 unGameWidth, uint32 unGameHeight, EOverlayWindowAlignment alignment );

	// Set desired output texture format for overlay surfaces
	void SetOverlayTextureFormat( int32 eTextureFormat ) { m_eOverlayTextureFormat = eTextureFormat; }

	// Get the scaling factor that is being applied to all UI by the framework
	virtual float GetWindowScaleFactor() { return m_flScaleFactor; }

	// Set the scaling factor that is being applied to all UI by the framework
	virtual void SetWindowScaleFactor( float flScaleFactor ) { m_flScaleFactor = flScaleFactor; }

	// Get the render thread frame time
	volatile double GetCurrentRenderThreadFrameTime() { return m_flCurrentRenderFrameTime; }

	// get the number of 5 second periods where the FPS dropped below our threshold (40hz)
	volatile int GetNumPeriodsBelowMinFPS() { return m_nSlowFPSPeriod; }


	// Helper for composition layers to let us know we need to treat our shader vars as dirty (because d2d has messed with state underneath us)
	void SetShaderVariablesDirty();

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName );
#endif

private:

	friend class CCompositionLayer;
	friend class CDWriteTextRenderer;

	void PresentBackBuffer();
	void ComputeBackbufferScaling();
	void LoadShaders();
	void GetAndSetColorCorrectionShaderVarDefaults();

	CCompositionLayer *GetCompositionLayer( CCompositionLayer &search );
	
	void ClearShaderResourceVariables();
	void ClearCompositingLayer( CCompositionLayer *pLayer );

	struct LockedOpacityMaskTextureShaderResourceView_t
	{
		CD3D10Texture *m_pTexture;
		int m_iLockHandle;
		ID3D10ShaderResourceView *m_pShaderResource;
	};

	bool PushOpacityLayerIfNeeded( ID2D1RenderTarget *pRenderTarget, const D2D1_SIZE_F &rect, const CMsgFillBrush &msg );
	ID2D1Brush *GetD2DBrushForFillBrush( const CMsgFillBrush &brushMsg );
	ID2D1RadialGradientBrush *GetRadialGradientBrush( const CMsgRadialGradient &msg );
	ID2D1LinearGradientBrush *GetLinearGradientBrush( const CMsgLinearGradient &msg );
	ID2D1SolidColorBrush *GetSolidColorBrush( uint32 unColor );
	LockedOpacityMaskTextureShaderResourceView_t GetOpacityMaskShaderResourceViewForTexture( uint32 unTextureID );
	void ReleaseLockedOpacityMaskTextureShaderResourceView( LockedOpacityMaskTextureShaderResourceView_t &data );
	ID3D10ShaderResourceView *GetOpacityMaskShaderResourceViewForCornerRadii( float flWidth, float flHeight, float flXInset, float flYInset, float *pflCornerRadii );
	CCompositionLayer *GetOuterShadowLayer( CCompositionLayer *pSourceLayer, bool bFill, float flHorOffset, float flVerOffset, float flBlurRadius, float flSpreadDistance, uint32 shadowColor, bool bAnimating );
	void DrawOuterShadowLayer( CCompositionLayer *pShadowLayer, CCompositionLayer *pSourceLayer, bool bFill, float flHorOffset, float flVerOffset, float flBlurRadius, float flSpreadDistance, uint32 shadowColor );
	CCompositionLayer *GetInsetShadowLayer( CCompositionLayer *pSourceLayer, float flHorOffset, float flVerOffset, float flBlurRadius, float flSpreadDistance, uint32 shadowColor, bool bAnimating );
	void DrawInsetShadowLayer( CCompositionLayer *pShadowLayer, CCompositionLayer *pSourceLayer, float flHorOffset, float flVerOffset, float flBlurRadius, float flSpreadDistance, uint32 shadowColor );
	void DrawMouseCursor( CCompositionLayer *pLayer, uint32 nMouseTextureID, const Vector2D &ptHotspot );
	void DrawSteamPadPointer( CCompositionLayer *pLayer, SteamPadPointer_t *pPointer, int padX, int padY );

	
	UITextOpacityMaskData_t *GetCachedTextOpacityMask( const void *pRawText, int cbRawText, int cTextChars, EPanoramaTextEncoding eTextEncoding, float x0, float y0, float x1, float y1, float flLineHeight, ETextAlign align, bool bWrap, bool bEllipsis, 
		const CMsgRenderTextFormat &defaultFormat, const ::google::protobuf::RepeatedPtrField< CMsgRenderTextRangeFormat > &rangeFormats );
	void DrawTextRegionRange( ID2D1RenderTarget *pRenderTarget, float x0, float y0, float x1, float y1, UITextOpacityMaskDataRange_t &maskRange, const CMsgRenderFillBrushCollection &fill_brush_collection );

	// Helper for drawing rounded corner mask bitmap into render target
	bool BDrawRoundedCornerMaskBitmapToRenderTarget( ID2D1RenderTarget *pOpacityRenderTarget, float flWidth, float flHeight, float flXInset, float flYInset, float flTopLeftHorizontal, float flTopLeftVertical, float flTopRightHorizontal, float flTopRightVertical, 
		float flBottomRightHorizontal, float flBottomRightVertical, float flBottomLeftHorizontal, float flBottomLeftVertical );


	void DrawTexturedQuadInternal( ID3D10EffectTechnique *pTechnique, ID3D10Texture2D *pTexture, 
		ID3D10ShaderResourceView *pShaderResourceView, ID3D10ShaderResourceView *pOpacityMaskResourceView, ID3D10ShaderResourceView *pOpacityMaskTwoResourceView, 
		ID3D10ShaderResourceView *pYTex, ID3D10ShaderResourceView *pUTex, ID3D10ShaderResourceView *pVTex, const VertexTextured_t *points, float flScale2DX, float flScale2DY, float flRotate2D );

	void FlushCurrentVertexBuffer( ID3D10EffectTechnique *pTechnique );
	void UpdateViewPortSize();
	bool BCreateDistortionMap();

	void UpdateBlurVariables( Vector2D vecBlurDirection, float flStdDeviation, float flStepSize );

	// Helper methods for creating resources
	bool BCreateDeviceIndependentResources();
	bool BCreateDeviceAndSwapChain();
	bool BCreateD3DDeviceResources();
	bool BRecreateSizedD3DResources();
	bool BCreateRenderTarget( ID3D10Texture2D *pBackBuffer, ID2D1Factory *pD2DFactory );


	bool BCheckForDeviceRemovedAndCrash();

	// have we already crashed and shouldn't dispatch another fatal error?
	bool m_bCrashed;

	double m_flLastPaintFrameTime;

	CUtlVector<CMsgPushClipLayer> m_vecClipLayers;

	enum ERenderState
	{
		k_ERenderStateUnset = 0,
		k_ERenderStateDrawTexturedQuad = 1,
	};

	ERenderState m_ERenderState;

	// Map of currently loaded effects
	struct EffectData_t
	{
		EffectData_t() 
		{ 
			m_pEffect = NULL; 
			m_pCurrentTexture2D = NULL; 
			m_pVertexBuffer = NULL; 
			m_pVertexLayout = NULL; 
			m_pCurrentVertexBatch = NULL;
			m_pCurrentTextureY = NULL;
			m_pCurrentTextureU = NULL;
			m_pCurrentTextureV = NULL;
			m_unVertexBufferPosition = 0;
			m_unVerticesInCurrentBatch = 0;
		}

		ID3D10Effect *m_pEffect;
		ID3D10Texture2D *m_pCurrentTexture2D;
		ID3D10ShaderResourceView *m_pOpacityMask;
		ID3D10ShaderResourceView *m_pOpacityMaskTwo;
		ID3D10ShaderResourceView *m_pCurrentTextureY;
		ID3D10ShaderResourceView *m_pCurrentTextureU;
		ID3D10ShaderResourceView *m_pCurrentTextureV;
		ID3D10Buffer *m_pVertexBuffer;
		ID3D10InputLayout *m_pVertexLayout;
		VertexTextured_t *m_pCurrentVertexBatch;
		uint32 m_unVertexBufferPosition;
		uint32 m_unVerticesInCurrentBatch;
	};

	// Data for currently set effect
	EffectData_t *m_pRenderEffect;
	ID3D10EffectTechnique *m_pTechnique;
	ID3D10EffectScalarVariable *m_pViewportHeight;
	ID3D10EffectScalarVariable *m_pViewportWidth;
	ID3D10EffectShaderResourceVariable *m_pDiffuseTex;
	ID3D10EffectShaderResourceVariable *m_pOpacityMaskTex;
	ID3D10EffectShaderResourceVariable *m_pOpacityMaskTexTwo;
	ID3D10EffectVariable *m_pmatTransform;
	ID3D10EffectScalarVariable *m_pflSaturation;
	ID3D10EffectScalarVariable *m_pflHueShift;
	ID3D10EffectScalarVariable *m_pflBrightness;
	ID3D10EffectScalarVariable *m_pflContrast;
	ID3D10EffectScalarVariable *m_pflOpacityMaskOneBase;
	ID3D10EffectScalarVariable *m_pflOpacityMaskOneOpacity;
	ID3D10EffectScalarVariable *m_pflOpacityMaskTwoBase;
	ID3D10Buffer  *m_pCurrentVertexBuffer;

	// Simple non-premultipled quad technique
	ID3D10EffectTechnique *m_pTechniqueQuadNonPremultiplied;

	// Simple alpha channel only quad technique
	ID3D10EffectTechnique *m_pTechniqueQuadAlphaOnly;

	// Simple premultiplied quad technique
	ID3D10EffectTechnique *m_pTechniqueQuadPremultiplied;

	// Blur technique
	ID3D10EffectTechnique *m_pTechniqueBlur;
	ID3D10EffectVectorVariable *m_pBlurDirectionVecPass1;
	ID3D10EffectVectorVariable *m_pBlurDirectionVecPass2;
	ID3D10EffectVectorVariable *m_pBlurDirectionVecPass3;
	ID3D10EffectVectorVariable *m_pBlurDirectionVecPass4;
	ID3D10EffectVectorVariable *m_pIncrementalGaussian;


	// YUV420 technique
	ID3D10EffectTechnique *m_pTechniqueYUV420;
	ID3D10EffectShaderResourceVariable *m_pYTex;
	ID3D10EffectShaderResourceVariable *m_pUTex;
	ID3D10EffectShaderResourceVariable *m_pVTex;

	// Particle system technique
	ID3D10EffectTechnique *m_pTechniqueParticleSystem;

	// Last technique we drew with, needed to know to flush if technique changes
	ID3D10EffectTechnique *m_pLastDrawTechnique;
	ID3D10EffectTechnique *m_pFlushTechnique;

	bool m_bShaderVarsDirty;

	struct BorderRadiusOpacityMaskKey_t
	{
		float m_flWidth;
		float m_flHeight;
		float m_flXInset;
		float m_flYInset;
		float m_rgRadii[8];

		// Sort on size only
		bool operator <( const BorderRadiusOpacityMaskKey_t &l ) const
		{
			if ( m_flXInset < l.m_flXInset ) 
				return true;
			else if ( m_flXInset > l.m_flXInset )
				return false;

			if ( m_flYInset < l.m_flYInset ) 
				return true;
			else if ( m_flYInset > l.m_flYInset )
				return false;

			if ( m_flWidth < l.m_flWidth ) 
				return true;
			else if ( m_flWidth > l.m_flWidth )
				return false;

			if ( m_flHeight < l.m_flHeight )
				return true;
			else if ( m_flHeight > l.m_flHeight )
				return false;

			for( int i=0; i<V_ARRAYSIZE(m_rgRadii); ++i )
			{
				if ( m_rgRadii[i] < l.m_rgRadii[i] )
					return true;
				else if ( m_rgRadii[i] > l.m_rgRadii[i] )
					return false;
			}

			return false;
		}
	};

	struct BorderRadiusOpacityMaskData_t
	{
		BorderRadiusOpacityMaskData_t() { m_pTexture2D = NULL; m_pShaderResourceView = NULL; }
		ID3D10Texture2D *m_pTexture2D;
		ID3D10ShaderResourceView *m_pShaderResourceView;
		int m_iLRUIndex;
	};
	struct BorderRadiusOpacityMaskLRU_t
	{
		double m_flLastUseTime;
		int m_iMap;
	};
	CUtlMap< BorderRadiusOpacityMaskKey_t, BorderRadiusOpacityMaskData_t, short, CDefLess<BorderRadiusOpacityMaskKey_t> > m_mapBorderRadiusOpacityMasks;
	CUtlLinkedList< BorderRadiusOpacityMaskLRU_t, int > m_listBorderRadiusOpacityMaskLRU;
	

	CDWriteTextRenderer *m_pTextRenderer;
	IUITextTextureCache *m_pTextTextureCache;
	IUITextLayoutDrawCache *m_pTextLayoutDrawCache;

	CMemoryPool m_CompositionLayerPool;

	struct SolidBrush_t
	{
		ID2D1SolidColorBrush *m_pBrush;
		int m_iLRUIndex;
	};
	struct SolidBrushLRU_t
	{
		double m_flLastUseTime;
		int m_iMap;
	};
	CUtlMap< DWORD, SolidBrush_t, int, CDefLess< DWORD > > m_mapSolidColorBrushes;
	CUtlLinkedList< SolidBrushLRU_t, int > m_listSolidColorBrushLRU;

	struct LinearGradientBrush_t
	{
		ID2D1LinearGradientBrush *m_pBrush;
		int m_iLRUIndex;
	};
	struct LinearGradientBrushLRU_t
	{
		double m_flLastUseTime;
		int m_iMap;
	};
	CUtlMap< CRenderMsg< CMsgLinearGradient >::Node *, LinearGradientBrush_t > m_mapLinearGradientBrushes;
	CUtlLinkedList< LinearGradientBrushLRU_t, int > m_listLinearGradientBrushLRU;

	struct RadialGradientBrush_t
	{
		ID2D1RadialGradientBrush *m_pBrush;
		int m_iLRUIndex;
	};
	struct RadialGradientBrushLRU_t
	{
		double m_flLastUseTime;
		int m_iMap;
	};
	CUtlMap< const CMsgRadialGradient *, RadialGradientBrush_t > m_mapRadialGradientBrushes;
	CUtlLinkedList< RadialGradientBrushLRU_t, int > m_listRadialGradientBrushLRU;

	static CInterlockedInt s_nSurfaces;

	// List of free composition layers for re-use
	CUtlRBTree< CCompositionLayer *, int > m_treeFreeCompositionLayers;
	CUtlLinkedList< int, int > m_listCompositionLayersLRU;

	struct ReservedLayer_t
	{
		CCompositionLayer *m_pLayer;
		int m_iLRUIndex;
	};
	struct ReservedLayerLRU_t
	{
		double m_flLastUseTime;
		int m_iMap;
	};
	CUtlMap< uint64, ReservedLayer_t, int, CDefLess<uint64> > m_mapReservedCompositionLayers;
	CUtlLinkedList< ReservedLayerLRU_t, int > m_listReservedCompositionLayerLRU;
	CThreadMutex m_MutexReservedLayers;

	struct ShadowLayerKey_t
	{
		float m_flBaseWidth;
		float m_flBaseHeight;
		bool m_bInset;
		bool m_bFill;
		float m_flHorOffset;
		float m_flVerOffset;
		float m_flBlurRadius;
		float m_flSpreadDistance;
		uint32 m_shadowColor;
		float m_rgBorderRadii[8];

		// Sort on size only
		bool operator <( const ShadowLayerKey_t &r ) const
		{
			if ( m_flBaseWidth < r.m_flBaseWidth )
				return true;
			else if ( m_flBaseWidth > r.m_flBaseWidth )
				return false;

			if ( m_flBaseHeight < r.m_flBaseHeight )
				return true;
			else if ( m_flBaseHeight > r.m_flBaseHeight )
				return false;

			if ( m_bInset != r.m_bInset )
				return m_bInset;

			if ( m_bFill != r.m_bFill )
				return m_bFill;

			if ( m_flHorOffset < r.m_flHorOffset )
				return true;
			else if ( m_flHorOffset > r.m_flHorOffset )
				return false;

			if ( m_flVerOffset < r.m_flVerOffset )
				return true;
			else if ( m_flVerOffset > r.m_flVerOffset )
				return false;

			if ( m_flBlurRadius < r.m_flBlurRadius )
				return true;
			else if ( m_flBlurRadius > r.m_flBlurRadius )
				return false;

			if ( m_flSpreadDistance < r.m_flSpreadDistance )
				return true;
			else if ( m_flSpreadDistance > r.m_flSpreadDistance )
				return false;

			for ( int i=0; i < V_ARRAYSIZE( m_rgBorderRadii ); ++i )
			{
				if ( m_rgBorderRadii[i] < r.m_rgBorderRadii[i] )
					return true;
				else if ( m_rgBorderRadii[i] > r.m_rgBorderRadii[i] )
					return false;
			}

			return m_shadowColor < r.m_shadowColor;
		}
	};

	struct ShadowLayer_t
	{
		CCompositionLayer *m_pLayer;
		int m_iLRUIndex;
	};
	struct ShadowLayerLRU_t
	{
		double m_flLastUseTime;
		int m_iMap;
	};
	CUtlMap< ShadowLayerKey_t, ShadowLayer_t, int, CDefLess<ShadowLayerKey_t> > m_mapShadowLayers;
	CUtlLinkedList< ShadowLayerLRU_t, int > m_listShadowLayerLRU;

	// Stack of composition layers currently pushed
	CUtlVector< CCompositionLayer * > m_stackCompositionLayers;

	// Texture data
	static CInterlockedInt s_unNextTextureID;
	CThreadSpinLock m_lockTextureMap;
	CUtlMap< uint32, IUITexture *, short, CDefLess< uint32 > > m_mapTextures;
	CTSQueue< uint32 > m_tsQueueTextureDeletes;

	static CInterlockedInt s_unNextOverlayTextureID;
	int m_nOverlayTextureID;

	IUITexture *m_pUITextureOpaqueMask;

	bool m_bEnforceAspectRatio;
	float m_flScaleBackbufferX;
	float m_flTranslateBackbufferX;
	float m_flScaleBackbufferY;
	float m_flTranslateBackbufferY;

	// FPS average data
	CFastTimer m_FrameTimer;
	int m_nLastFrameMillisecondsIndex;
	float m_rgflMillisecondsFrame[PANORAMA_FRAMES_FOR_FPS_AVERAGES];

	// Current render frame time
	volatile double m_flCurrentRenderFrameTime;

	// owner of details about the cursor state
	CMouseCursorRender *m_pCursorRender;

	// Surface type/size data
	IUIEngine::ERenderTarget m_eRenderTarget;
	HWND m_hWnd;
	uint32 m_unSurfaceWidth;
	uint32 m_unSurfaceHeight;
	uint32 m_unWindowWidth;
	uint32 m_unWindowHeight;
	bool m_bFixedSurfaceSize;
	bool m_bSurfaceOccluded; // Is the surface fully blocked from being seen?

	uint64 m_ulDedicatedGPUMem;
	uint64 m_ulDedicatedSysMem;
	uint64 m_ulSharedSysMem;

	Color m_cLetterBoxColor;

	// factories
	ID2D1Factory *m_pD2DFactory;
	IDWriteFactory *m_pDWriteFactory;

	// interfaces
	IDXGISwapChain *m_pDXGISwapChain;
	ID3D10Device1 *m_pD3D10Device;
	IDXGIFactory1 *m_pDXGIFactory;
	ID3D10RasterizerState *m_pRasterizerState;
	IDWriteRenderingParams *m_pDWriteRenderingParams;

	// Render targets
	ID3D10Texture2D *m_pBackBuffer; 
	ID2D1RenderTarget *m_pD2DRenderTarget; 
	ID3D10RenderTargetView *m_pRenderTargetView;

	// distortion textures
	IUITexture *m_rpDistortionMap[2];

	CSharedMemStream *m_pBackBufferSharedMemStream;
	IPC::IEvent *m_pBackBufferSharedMemEvent;
	IPC::IEvent *m_pBackBufferSharedMemWriteEvent;
	uint32 m_unBackBufferSharedMenEventFails;
	DWORD m_dwTargetOverlayPID;
	bool m_bRenderSharedSurface;
	float m_flTimeLastSharedTexUpdate;
	int32 m_eOverlayTextureFormat;
	CUtlMemory<uint8> m_bufTempScanline;

	// Composition layer
	CCompositionLayer *m_pCompositionLayer;

	// Is vsync enabled?
	bool m_bVsyncEnabled;

	// Has hmd finished calibrating and gotten a valid initial pose?
	bool m_bHmdReady;
	vr::VROverlayHandle_t m_ulVROverlayHandle;


	// What scaling factor should we use on drawing the mouse cursor?  All other input values
	// come in pre-scaled.
	float m_flScaleFactor;

	double m_flRenderFrameTime; // accumlated frame time since last slow fps check
	int m_nFramesRendered; // number of frames rendered since last slow fps check
	double m_flRenderSessionFrameTime; // accumlated frame time since last slow fps check
	int m_nSessionFramesRendered; // number of frames rendered since last slow fps check
	int m_nSlowFPSPeriod; // number of slow fps periods detected by slow fps check

	double m_flLastStatsDump;

	// Vars for texture sharing cross proc for overlay
	ID3D10Texture2D *m_pSharedTexCopy;
	IDXGIKeyedMutex *m_pKeyedMutex;
	HANDLE m_hSharedText;

	SteamPadPointer_t m_leftSteamPadPointer;
	SteamPadPointer_t m_rightSteamPadPointer;
	
	ThreadId_t m_renderThreadID;

#ifdef PANORAMA_STEAMUI_STREAMING_CAPTURE_WIN32
	SteamUIStreamingCaptureCallback_t volatile m_pSteamUIStreamingCallback;
#endif

#ifdef _NVPERFKIT
	NVPMContext m_hNVPMContext;
#endif
};

void D3D_SetDebugName( ID3D10DeviceChild *pObject, const char *pchName );

} // namespace panorama

#endif // D3D10SURFACE_H
