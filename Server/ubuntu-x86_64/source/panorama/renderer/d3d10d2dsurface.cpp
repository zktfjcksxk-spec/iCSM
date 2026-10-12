//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx.h"
#include "d3d10d2dsurface.h"
#include <dxgitype.h>
#include <dxgi1_2.h>
#include <d3d11_1.h>
#define INITGUID 1 
#include "D3Dcommon.h"
#include "panorama/layout/csshelpers.h"
#include "color.h"
#include "tier1/checksum_crc.h"
#include "uienginewin32.h"
#include "rendermessages.h"
#include "../../overlay/common/rendermessages.h"
#include <openvr.h>
#include <vrapi.h>

#if !defined( SOURCE2_PANORAMA )
#include <nvapi.h>
#endif


// The below DXGI errors are from dxgitype.h in an old dxsdk, they moved to winerror.h but we have an older windows sdk.. ugh.
#define DXGI_ERROR_DEVICE_HUNG           _HRESULT_TYPEDEF_(0x887A0006L)
#define DXGI_ERROR_DEVICE_REMOVED        _HRESULT_TYPEDEF_(0x887A0005L)
#define DXGI_ERROR_DEVICE_RESET          _HRESULT_TYPEDEF_(0x887A0007L)
#define DXGI_ERROR_DRIVER_INTERNAL_ERROR _HRESULT_TYPEDEF_(0x887A0020L)
#define DXGI_ERROR_INVALID_CALL          _HRESULT_TYPEDEF_(0x887A0001L)

#define DXGI_STATUS_OCCLUDED             _HRESULT_TYPEDEF_(0x087A0001L)

#define DWORD_ARGB(a,r,g,b) \
	((DWORD)((((a)&0xff)<<24)|(((r)&0xff)<<16)|(((g)&0xff)<<8)|((b)&0xff)))

// Needed for D3DPERF_ calls
#include "d3d9.h"

#ifdef _NVPERFKIT

// Set up NVPMAPI
#define NVPM_INITGUID
#include "C:\\PerfKit_2.2.0.12166\\inc\\NvPmApi.Manager.h"

// Simple singleton implementation for grabbing the NvPmApi
static NvPmApiManager S_NVPMManager;
extern NvPmApiManager *GetNvPmApiManager() {return &S_NVPMManager;}
const NvPmApi *GetNvPmApi() {return S_NVPMManager.Api();}

#ifdef _WIN64
#define PATH_TO_NVPMAPI_CORE L"C:\\PerfKit_2.2.0.12166\\bin\\win7_x64\\NvPmApi.Core.dll"
#else
#define PATH_TO_NVPMAPI_CORE L"C:\\PerfKit_2.2.0.12166\\bin\\win7_x86\\NvPmApi.Core.dll"
#endif

#endif

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

#define SAFE_LAYER_DELETE( pLayer ) \
	if ( pLayer ) \
	{ \
		Destruct( pLayer ); \
		m_CompositionLayerPool.Free( pLayer ); \
		pLayer = NULL; \
	} \


ConVar s_convarPanoramaVsync( "@panorama_vsync", "1" );
ConVar s_convarPanoramaGPUMemorySpew( "@panorama_spew_gpu_memory", "0" );

static const float k_flMinFPSForSlowReport = 40.0f; // increment the slow FPS counter if a 5 second window goes below this avg
static const int k_nSlowFPSSeconds = 5; // number of seconds to track the slow FPS window


// Keep this off all the time by default, generates noise/perf hit, but useful if you are really doing d3d/d2d level surface work
//#define D3DDEBUGRUNTIME 1

#ifdef D3DDEBUGRUNTIME 
#define PushPerfEvent( dwColor, pwchEvent ) g_D3DPERF_BeginEvent( dwColor, pwchEvent );
#define PopPerfEvent() g_D3DPERF_EndEvent();
#else
#define PushPerfEvent( dwColor, pwchEvent ) ((void)0)
#define PopPerfEvent() ((void)0)
#endif

// statics
CInterlockedInt CD3D10D2DSurface::s_nSurfaces;
CInterlockedInt CD3D10D2DSurface::s_unNextTextureID;

// 2 because 0 is treated as invalid and 1 is reserved for the magic white texture
CInterlockedInt CD3D10D2DSurface::s_unNextOverlayTextureID = 2;

// extern from uitoplevelwindowwin32.cpp, need to synchronize some fullscreen switching
extern CInterlockedInt g_nInFullscreenSwitch;

#if _MSC_VER < 1600
// Below is from windows.h
extern "C++"
{
	template<typename T> void** IID_PPV_ARGS_Helper(T** pp) 
	{
		static_cast<IUnknown*>(*pp);    // make sure everyone derives from IUnknown
		return reinterpret_cast<void**>(pp);
	}
}
#define IID_PPV_ARGS(ppType) __uuidof(**(ppType)), IID_PPV_ARGS_Helper(ppType)
#endif

void panorama::D3D_SetDebugName( ID3D10DeviceChild *pObject, const char *pchName )
{
#ifdef D3DDEBUGRUNTIME
	// Only works if device is created with the D3D10 or D3D10 debug layer, or when attached to PIX for Windows
	static const GUID guid = { 0x429b8c22, 0x9188, 0x4b0c, { 0x87, 0x42,  0xac,  0xb0,  0xbf,  0x85,  0xc2,  0x00 } };
	pObject->SetPrivateData( guid, V_strlen( pchName ), pchName );
#endif
}


static BOOL operator!=(const LUID& a, const LUID& b)
{
	return a.HighPart != b.HighPart || a.LowPart != b.LowPart;
}


//-----------------------------------------------------------------------------
// Purpose: Constructor 
//-----------------------------------------------------------------------------
CCompositionLayer::CCompositionLayer( CD3D10D2DSurface *pParentSurface, ID3D10Texture2D *pD3DSurface, ID2D1RenderTarget *pD2DRenderTarget, ID3D10RenderTargetView *pRenderTargetView, ID3D10ShaderResourceView *pShaderResourceView,
				  float width, float height )
{
	m_ulContextID = 0;
	m_bIsDrawing = false;
	m_bBeginDrawDone = false;

	m_pParentSurface = pParentSurface;

	m_pOffscreenSurface = pD3DSurface;
	if ( m_pOffscreenSurface )
		m_pOffscreenSurface->AddRef();

	m_pD2DRenderTarget = pD2DRenderTarget;
	if ( m_pD2DRenderTarget )
		m_pD2DRenderTarget->AddRef();

	m_pRenderTargetView = pRenderTargetView;
	if ( m_pRenderTargetView )
		m_pRenderTargetView->AddRef();

	m_pShaderResourceView = pShaderResourceView;
	if ( m_pShaderResourceView )
		m_pShaderResourceView->AddRef();


	m_pVecClipLayers = NULL; 
	m_flLayerWidth = ceil( width );
	m_flLayerHeight = ceil( height );

	m_flSaturation = 1.0f;
	m_flHueShift = 0.0f;
	m_flBrightness = 1.0f;
	m_flContrast = 1.0f;
	m_unOpacityMaskTextureID = 0;
	m_flOpacityMaskOpacity = 1.0f;
	m_flBlurPasses = 1.0f;
	m_flBlurStdDevHor = 0.0f;
	m_flBlurStdDevVer = 0.0f;
	m_iLRUPos = -1;
	m_flLastTimeUsed = 0;
	V_memset( m_RenderQuad, 0, sizeof( m_RenderQuad ) );

	for ( int i=0; i < V_ARRAYSIZE( m_rgbaBorderColors ); ++i )
	{
		m_rgbaBorderColors[i] = 0;
	}

	for ( int i=0; i < V_ARRAYSIZE( m_rgBorderWidths ); ++i )
	{
		m_rgBorderWidths[i] = 0.0f;
	}

	for( int i=0; i < V_ARRAYSIZE( m_rgCornerRadii ); ++i )
	{
		m_rgCornerRadii[i] = 0.0f;
	}

	m_bBoxShadowInset = false;
	m_bBoxShadowFill = false;
	m_flBoxShadowHorOffset = 0.0f;
	m_flBoxShadowVerOffset = 0.0f;
	m_flBoxShadowBlurRadius = 0.0f;
	m_flBoxShadowSpreadDistance = 0.0f;
	m_rgbaBoxShadowColor = 0x00000000;
	m_bAnimatingBoxShadow = false;

	m_flScaleLayerX = 1.0f;
	m_flScaleLayerY = 1.0f;
	m_flTranslateLayerX = 0.0f;
	m_flTranslateLayerY = 0.0f;

	m_flMatrix[0] = 1.0f;
	m_flMatrix[1] = 0.0f;
	m_flMatrix[2] = 0.0f;
	m_flMatrix[3] = 0.0f;

	m_flMatrix[4] = 0.0f;
	m_flMatrix[5] = 1.0f;
	m_flMatrix[6] = 0.0f;
	m_flMatrix[7] = 0.0f;

	m_flMatrix[8] = 0.0f;
	m_flMatrix[9] = 0.0f;
	m_flMatrix[10] = 1.0f;
	m_flMatrix[11] = 0.0f;

	m_flMatrix[12] = 0.0f;
	m_flMatrix[13] = 0.0f;
	m_flMatrix[14] = 0.0f;
	m_flMatrix[15] = 1.0f;

	m_flScale2D[0] = 1.0f;
	m_flScale2D[1] = 1.0f;

	m_flRotate2D = 0.0f;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CCompositionLayer::~CCompositionLayer()
{
	SAFE_DELETE( m_pVecClipLayers );
	SAFE_RELEASE( m_pOffscreenSurface );
	SAFE_RELEASE( m_pRenderTargetView );
	SAFE_RELEASE( m_pShaderResourceView );
	SAFE_RELEASE( m_pD2DRenderTarget );
}


//-----------------------------------------------------------------------------
// Purpose: Helper for pushing clip layers and beginning draw on d2d target
//-----------------------------------------------------------------------------
void CCompositionLayer::PushCliplayersAndBeginDraw( float flScaleX, float flScaleY, float flTranslateX, float flTranslateY )
{
	VPROF_BUDGET( "CCompositionLayer::PushCliplayersAndBeginDraw ", VPROF_BUDGETGROUP_TENFOOT );
	if ( !m_pD2DRenderTarget )
		return;

	m_flScaleLayerX = flScaleX;
	m_flScaleLayerY = flScaleY;
	m_flTranslateLayerX = flTranslateX;
	m_flTranslateLayerY = flTranslateY;
	m_bIsDrawing = true;
}


//-----------------------------------------------------------------------------
// Purpose: Helper for doing actual BeginDraw called as late as possible only when
// we really need to draw
//-----------------------------------------------------------------------------
void CCompositionLayer::DoDelayedBeginDrawIfNeeded()
{
	if ( m_bIsDrawing && !m_bBeginDrawDone )
	{
		m_pD2DRenderTarget->BeginDraw();
		m_pD2DRenderTarget->SetTransform( D2D1::Matrix3x2F::Scale( m_flScaleLayerX, m_flScaleLayerY )*D2D1::Matrix3x2F::Translation( m_flTranslateLayerX, m_flTranslateLayerY ) );
		m_pParentSurface->SetShaderVariablesDirty();
		m_bBeginDrawDone = true;

		if ( m_pVecClipLayers )
		{
			FOR_EACH_VEC( (*m_pVecClipLayers), i )
			{
				PushLayerNow( m_pVecClipLayers->Element(i) );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Helper for popping clip layers and ending draw on D2D Surface
//-----------------------------------------------------------------------------
void CCompositionLayer::PopClipLayersAndFlush()
{
	VPROF_BUDGET( "CD3D10D2DSurface::PopClipLayersAndFlush", VPROF_BUDGETGROUP_TENFOOT );

	if ( !m_pD2DRenderTarget )
		return;

	if( m_bIsDrawing && m_bBeginDrawDone )
	{
		if ( m_pVecClipLayers )
		{
			FOR_EACH_VEC( (*m_pVecClipLayers), i )
			{
				PopLayerNow( m_pVecClipLayers->Element(i) );
			}
		}
		{
			VPROF_BUDGET( "CD3D10D2DSurface::PopClipLayersAndFlush - end draw", VPROF_BUDGETGROUP_TENFOOT );
			if ( !SUCCEEDED( m_pD2DRenderTarget->EndDraw() ) )
			{
				AssertMsg( false, "D2D EndDraw failed -- it should never fail unless our code is broken" );
			}

			m_pParentSurface->SetShaderVariablesDirty();
		}
	}

	m_bBeginDrawDone = false;
	m_bIsDrawing = false;
}


//-----------------------------------------------------------------------------
// Purpose: Get current clip layer count for composition layer
//-----------------------------------------------------------------------------
uint32 CCompositionLayer::GetClipLayerCount()
{
	if ( m_pVecClipLayers )
	{
		return m_pVecClipLayers->Count();
	}

	return 0;
}


//-----------------------------------------------------------------------------
// Purpose: Check clip layer vec is empty, clear if needed
//-----------------------------------------------------------------------------
void CCompositionLayer::CheckAndClearClipLayers()
{
	if ( m_pVecClipLayers )
	{
		Assert( m_pVecClipLayers->Count() == 0 );
		m_pVecClipLayers->RemoveAll();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Draws a layer inset shadow image (unblurred) into this layer
//-----------------------------------------------------------------------------
void CCompositionLayer::DrawInsetShadowIntoLayer( CD3D10D2DSurface *pBaseSurface, float flPadding, float flWidth, float flHeight, float flHorOffset, float flVerOffset, float flSpreadDistance, uint32 shadowColor, float *pflInnerRadii )
{
	VPROF_BUDGET( "CCompositionLayer::DrawInsetShadowIntoLayer", VPROF_BUDGETGROUP_TENFOOT );
	bool bHasRounding = false;
	for( int i=0; i < 8; ++i )
	{
		if ( pflInnerRadii[i] != 0.0f )
		{
			bHasRounding = true;
			break;
		}
	}

	flWidth -= flPadding*2;
	flHeight -= flPadding*2;
	flHorOffset += flPadding;
	flVerOffset += flPadding;

	m_pD2DRenderTarget->BeginDraw();
	m_pD2DRenderTarget->Clear( D2D1::ColorF( 0.0f, 0.0f, 0.0f, 0.0f ) );

	ID2D1RectangleGeometry *pOuterGeometry = NULL;
	ID2D1Geometry *pInnerGeometry = NULL;

	D2D1_RECT_F rect;
	rect.left = 0;
	rect.top = 0;
	rect.right = m_flLayerWidth;
	rect.bottom = m_flLayerHeight;

	DbgVerify( SUCCEEDED( pBaseSurface->AccessD2D1Factory()->CreateRectangleGeometry( &rect, &pOuterGeometry ) ) );

	if ( !bHasRounding )
	{
		// Easy case for rectangular shadow
		D2D1_RECT_F rectInner;
		rectInner.left = flHorOffset;
		rectInner.top = flVerOffset;
		rectInner.right = flHorOffset+flWidth-flSpreadDistance;
		rectInner.bottom = flVerOffset+flHeight-flSpreadDistance;

		ID2D1RectangleGeometry *pGeometry = NULL;
		DbgVerify( SUCCEEDED( pBaseSurface->AccessD2D1Factory()->CreateRectangleGeometry( &rectInner, &pGeometry ) ) );
		pInnerGeometry = pGeometry;
	}
	else
	{
		// Need to build up path for geometry to draw
		ID2D1PathGeometry *pGeometry;
		if ( SUCCEEDED( pBaseSurface->AccessD2D1Factory()->CreatePathGeometry( &pGeometry ) ) )
		{
			pInnerGeometry = pGeometry;

			ID2D1GeometrySink *pSink;
			if ( SUCCEEDED( pGeometry->Open( &pSink ) ) )
			{
				D2D1_ARC_SEGMENT segment;
				pSink->BeginFigure( 
					D2D1::Point2F( flHorOffset, pflInnerRadii[1]+flVerOffset ), 
					D2D1_FIGURE_BEGIN_FILLED );

				segment.point.x = pflInnerRadii[0]+flHorOffset;
				segment.point.y = flVerOffset;
				segment.size.width = pflInnerRadii[0];
				segment.size.height = pflInnerRadii[1];
				segment.rotationAngle = 0.0f;
				segment.sweepDirection = D2D1_SWEEP_DIRECTION_CLOCKWISE;
				segment.arcSize = D2D1_ARC_SIZE_SMALL;
				pSink->AddArc( &segment );

				pSink->AddLine( D2D1::Point2F( flWidth-flSpreadDistance-pflInnerRadii[2]+flHorOffset, flVerOffset ) );

				segment.point.x = flHorOffset+flWidth-flSpreadDistance;
				segment.point.y = pflInnerRadii[3]+flVerOffset;
				segment.size.width = pflInnerRadii[2];
				segment.size.height = pflInnerRadii[3];
				pSink->AddArc( &segment );

				pSink->AddLine( D2D1::Point2F( flHorOffset+flWidth-flSpreadDistance, flVerOffset+flHeight-flSpreadDistance-pflInnerRadii[5] ) );

				segment.point.x = flHorOffset+flWidth-flSpreadDistance-pflInnerRadii[4];
				segment.point.y = flVerOffset+flHeight-flSpreadDistance;
				segment.size.width = pflInnerRadii[4];
				segment.size.height = pflInnerRadii[5];
				pSink->AddArc( &segment );

				pSink->AddLine( D2D1::Point2F( flHorOffset+pflInnerRadii[6], flVerOffset+flHeight-flSpreadDistance ) );

				segment.point.x = flHorOffset;
				segment.point.y = flVerOffset+flHeight-flSpreadDistance-pflInnerRadii[7];
				segment.size.width = pflInnerRadii[6];
				segment.size.height = pflInnerRadii[7];
				pSink->AddArc( &segment );

				pSink->EndFigure( D2D1_FIGURE_END_CLOSED );
				DbgVerify( SUCCEEDED( pSink->Close() ) );

				SAFE_RELEASE( pSink );
			}
		}
	}

	// Combine the geometries (xor), then draw
	ID2D1PathGeometry *pCombinedGeometry = NULL;
	if ( SUCCEEDED( pBaseSurface->AccessD2D1Factory()->CreatePathGeometry( &pCombinedGeometry ) ) )
	{
		ID2D1GeometrySink *pSink;
		if ( SUCCEEDED( pCombinedGeometry->Open( &pSink ) ) )
		{
			if ( SUCCEEDED( pOuterGeometry->CombineWithGeometry( pInnerGeometry, D2D1_COMBINE_MODE_XOR, NULL, 2.0f, pSink ) ) )
			{
				DbgVerify( SUCCEEDED( pSink->Close() ) );
			}
			SAFE_RELEASE( pSink );
		}
	}

	if ( pCombinedGeometry )
	{
		ID2D1SolidColorBrush *pBrush = pBaseSurface->GetSolidColorBrush( shadowColor );
		if ( pBrush )
		{
			m_pD2DRenderTarget->FillGeometry( pCombinedGeometry, pBrush );
			SAFE_RELEASE( pBrush );
		}
	}

	SAFE_RELEASE( pCombinedGeometry );
	SAFE_RELEASE( pInnerGeometry );
	SAFE_RELEASE( pOuterGeometry );

	m_pD2DRenderTarget->EndDraw();
}


//-----------------------------------------------------------------------------
// Purpose: Draw the border for the layer
//-----------------------------------------------------------------------------
void CCompositionLayer::DrawBorder( CD3D10D2DSurface *pBaseSurface )
{
	VPROF_BUDGET( "CCompositionLayer::DrawBorder", VPROF_BUDGETGROUP_TENFOOT );
	// Is there any border at all?
	if ( m_rgBorderWidths[0] == 0.0f && m_rgBorderWidths[1] == 0.0f && m_rgBorderWidths[2] == 0.0f && m_rgBorderWidths[3] == 0.0f )
		return;

	// Early out for transparent colors
	bool bHasColor = false;
	for ( int iCur = 0; iCur < V_ARRAYSIZE( m_rgbaBorderColors ); ++iCur )
	{
		if ( ((m_rgbaBorderColors[iCur]>>24)&0xff) != 0 )
		{
			bHasColor = true;
			break;
		}
	}

	if ( !bHasColor )
		return;

	bool bHasRounding = false;
	for( int i=0; i < V_ARRAYSIZE( m_rgCornerRadii ); ++i )
	{
		if ( m_rgCornerRadii[i] != 0.0f )
		{
			bHasRounding = true;
			break;
		}
	}


	DoDelayedBeginDrawIfNeeded();
	if ( !bHasRounding )
	{
		// Create path geometry to draw top border edge into
		ID2D1PathGeometry *rgGeometry[4] = { NULL, NULL, NULL, NULL };
		if ( SUCCEEDED( pBaseSurface->AccessD2D1Factory()->CreatePathGeometry( &rgGeometry[0] ) ) )
		{
			ID2D1GeometrySink *pSink;
			if ( SUCCEEDED( rgGeometry[0]->Open( &pSink ) ) )
			{
				pSink->BeginFigure( D2D1::Point2F( 0, 0 ), D2D1_FIGURE_BEGIN_FILLED );
				pSink->AddLine( D2D1::Point2F( m_rgBorderWidths[3], m_rgBorderWidths[0] ) );
				pSink->AddLine( D2D1::Point2F( m_flLayerWidth-m_rgBorderWidths[1], m_rgBorderWidths[0] ) );
				pSink->AddLine( D2D1::Point2F( m_flLayerWidth, 0 ) );
			
				pSink->EndFigure( D2D1_FIGURE_END_CLOSED );
				DbgVerify( SUCCEEDED( pSink->Close() ) );

				SAFE_RELEASE( pSink );
			}
		}

		// Create path geometry to draw right border edge into
		if ( SUCCEEDED( pBaseSurface->AccessD2D1Factory()->CreatePathGeometry( &rgGeometry[1] ) ) )
		{
			ID2D1GeometrySink *pSink;
			if ( SUCCEEDED( rgGeometry[1]->Open( &pSink ) ) )
			{
				pSink->BeginFigure( D2D1::Point2F( m_flLayerWidth-m_rgBorderWidths[1], m_rgBorderWidths[0] ), D2D1_FIGURE_BEGIN_FILLED );
				pSink->AddLine( D2D1::Point2F( m_flLayerWidth, 0 ) );
				pSink->AddLine( D2D1::Point2F( m_flLayerWidth, m_flLayerHeight ) );
				pSink->AddLine( D2D1::Point2F( m_flLayerWidth-m_rgBorderWidths[1], m_flLayerHeight-m_rgBorderWidths[2] ) );

				pSink->EndFigure( D2D1_FIGURE_END_CLOSED );
				DbgVerify( SUCCEEDED( pSink->Close() ) );

				SAFE_RELEASE( pSink );
			}
		}

		// Create path geometry to draw bottom border edge into
		if ( SUCCEEDED( pBaseSurface->AccessD2D1Factory()->CreatePathGeometry( &rgGeometry[2] ) ) )
		{
			ID2D1GeometrySink *pSink;
			if ( SUCCEEDED( rgGeometry[2]->Open( &pSink ) ) )
			{
				pSink->BeginFigure( D2D1::Point2F( m_flLayerWidth-m_rgBorderWidths[1], m_flLayerHeight-m_rgBorderWidths[2] ), D2D1_FIGURE_BEGIN_FILLED );
				pSink->AddLine( D2D1::Point2F( m_flLayerWidth, m_flLayerHeight) );
				pSink->AddLine( D2D1::Point2F( 0, m_flLayerHeight ) );
				pSink->AddLine( D2D1::Point2F( m_rgBorderWidths[3], m_flLayerHeight-m_rgBorderWidths[2] ) );

				pSink->EndFigure( D2D1_FIGURE_END_CLOSED );
				DbgVerify( SUCCEEDED( pSink->Close() ) );

				SAFE_RELEASE( pSink );
			}
		}

		// Create path geometry to draw left border edge into
		if ( SUCCEEDED( pBaseSurface->AccessD2D1Factory()->CreatePathGeometry( &rgGeometry[3] ) ) )
		{
			ID2D1GeometrySink *pSink;
			if ( SUCCEEDED( rgGeometry[3]->Open( &pSink ) ) )
			{
				pSink->BeginFigure( D2D1::Point2F( m_rgBorderWidths[3], m_flLayerHeight-m_rgBorderWidths[2] ), D2D1_FIGURE_BEGIN_FILLED );
				pSink->AddLine( D2D1::Point2F( 0, m_flLayerHeight ) );
				pSink->AddLine( D2D1::Point2F( 0, 0 ) );
				pSink->AddLine( D2D1::Point2F( m_rgBorderWidths[3], m_rgBorderWidths[0] ) );

				pSink->EndFigure( D2D1_FIGURE_END_CLOSED );
				DbgVerify( SUCCEEDED( pSink->Close() ) );

				SAFE_RELEASE( pSink );
			}
		}

		// Now fill the geometries, we need to combine those we can, to avoid rough corners due to antialiasing,
		// we don't want to turn off antialiasing so sub pixel interpolation of widths looks good.
		for( int iCur=0; iCur < 4; ++iCur )
		{
			// Skip NULL, probably already combined with previous pass
			if ( rgGeometry[iCur] == NULL )
				continue;

			int nCountCombined = 0;
			for( int iCombine=iCur+1; iCombine < 4; ++iCombine )
			{
				// Skip NULL, likely already combined in previous pass.
				if ( rgGeometry[iCombine] == NULL )
					continue;

				if ( m_rgbaBorderColors[iCur] == m_rgbaBorderColors[iCombine] )
				{
					ID2D1PathGeometry *pCombinedGeometry = NULL;
					if ( SUCCEEDED( pBaseSurface->AccessD2D1Factory()->CreatePathGeometry( &pCombinedGeometry ) ) )
					{
						ID2D1GeometrySink *pSink;
						if ( SUCCEEDED( pCombinedGeometry->Open( &pSink ) ) )
						{
							if ( SUCCEEDED( rgGeometry[iCur]->CombineWithGeometry( rgGeometry[iCombine], D2D1_COMBINE_MODE_UNION, NULL, 2.0f, pSink ) ) )
							{
								DbgVerify( SUCCEEDED( pSink->Close() ) );
								
								SAFE_RELEASE( rgGeometry[iCur] );
								SAFE_RELEASE( rgGeometry[iCombine] );

								rgGeometry[iCur] = pCombinedGeometry;
								pCombinedGeometry = NULL;
							}
							SAFE_RELEASE( pSink );
						}
						SAFE_RELEASE( pCombinedGeometry );
					}
					++nCountCombined;
				}
			}

			// We've combined all we can with this one, draw it
			ID2D1SolidColorBrush *pBrush = pBaseSurface->GetSolidColorBrush( m_rgbaBorderColors[iCur] );
			if ( pBrush )
			{
				m_pD2DRenderTarget->FillGeometry( rgGeometry[iCur], pBrush );
				SAFE_RELEASE( pBrush );
			}

			SAFE_RELEASE( rgGeometry[iCur] );
		}
	}
	else
	{
		// Rounded corners case, must build enclosed path with two different sets of radii for all corners
		// Create path geometry to draw top border edge into.  Outer geometry is just a rect, since we'll clip it
		// with the opacity mask when compositing.  We don't want it to be correct, as antialiasing differences will
		// lead to backgrounds bleeding through on alpha blended edges then.
		ID2D1RectangleGeometry *pOuterGeometry = NULL;
		D2D1_RECT_F rect;
		rect.left = 0;
		rect.top = 0;
		rect.right = m_flLayerWidth;
		rect.bottom = m_flLayerHeight;

		DbgVerify( SUCCEEDED( pBaseSurface->AccessD2D1Factory()->CreateRectangleGeometry( &rect, &pOuterGeometry ) ) );

		ID2D1PathGeometry *pInnerGeometry = NULL;
		if ( SUCCEEDED( pBaseSurface->AccessD2D1Factory()->CreatePathGeometry( &pInnerGeometry ) ) )
		{
			ID2D1GeometrySink *pSink;
			if ( SUCCEEDED( pInnerGeometry->Open( &pSink ) ) )
			{
				float rgInnerRaddi[8];
				rgInnerRaddi[0] = MAX( m_rgCornerRadii[0] - m_rgBorderWidths[3], 0.0f );
				rgInnerRaddi[1] =  MAX( m_rgCornerRadii[1] - m_rgBorderWidths[0], 0.0f );
				rgInnerRaddi[2] = MAX( m_rgCornerRadii[2] - m_rgBorderWidths[1], 0.0f );
				rgInnerRaddi[3] =  MAX( m_rgCornerRadii[3] - m_rgBorderWidths[0], 0.0f );
				rgInnerRaddi[4] = MAX( m_rgCornerRadii[4] - m_rgBorderWidths[1], 0.0f );
				rgInnerRaddi[5] =  MAX( m_rgCornerRadii[5] - m_rgBorderWidths[2], 0.0f );
				rgInnerRaddi[6] = MAX( m_rgCornerRadii[6] - m_rgBorderWidths[3], 0.0f );
				rgInnerRaddi[7] =  MAX( m_rgCornerRadii[7] - m_rgBorderWidths[2], 0.0f );

				D2D1_ARC_SEGMENT segment;
				pSink->BeginFigure( D2D1::Point2F( m_rgBorderWidths[3], rgInnerRaddi[1]+m_rgBorderWidths[0] ), D2D1_FIGURE_BEGIN_FILLED );

				segment.point.x = rgInnerRaddi[0]+m_rgBorderWidths[3];
				segment.point.y = m_rgBorderWidths[0];
				segment.size.width = rgInnerRaddi[0];
				segment.size.height = rgInnerRaddi[1];
				segment.rotationAngle = 0.0f;
				segment.sweepDirection = D2D1_SWEEP_DIRECTION_CLOCKWISE;
				segment.arcSize = D2D1_ARC_SIZE_SMALL;
				pSink->AddArc( &segment );

				pSink->AddLine( D2D1::Point2F( m_flLayerWidth-rgInnerRaddi[2]-m_rgBorderWidths[1], m_rgBorderWidths[0] ) );

				segment.point.x = m_flLayerWidth-m_rgBorderWidths[1];
				segment.point.y = rgInnerRaddi[3]+m_rgBorderWidths[0];
				segment.size.width = rgInnerRaddi[2];
				segment.size.height = rgInnerRaddi[3];
				pSink->AddArc( &segment );

				pSink->AddLine( D2D1::Point2F( m_flLayerWidth-m_rgBorderWidths[1], m_flLayerHeight-rgInnerRaddi[5]-m_rgBorderWidths[2] ) );

				segment.point.x = m_flLayerWidth - rgInnerRaddi[4]-m_rgBorderWidths[1];
				segment.point.y = m_flLayerHeight-m_rgBorderWidths[2];
				segment.size.width = rgInnerRaddi[4];
				segment.size.height = rgInnerRaddi[5];
				pSink->AddArc( &segment );

				pSink->AddLine( D2D1::Point2F( rgInnerRaddi[6]+m_rgBorderWidths[3], m_flLayerHeight-m_rgBorderWidths[2] ) );

				segment.point.x = m_rgBorderWidths[3];
				segment.point.y = m_flLayerHeight - rgInnerRaddi[7] - m_rgBorderWidths[2];
				segment.size.width = rgInnerRaddi[6];
				segment.size.height = rgInnerRaddi[7];
				pSink->AddArc( &segment );

				pSink->EndFigure( D2D1_FIGURE_END_CLOSED );
				DbgVerify( SUCCEEDED( pSink->Close() ) );

				SAFE_RELEASE( pSink );
			}
		}

		ID2D1PathGeometry *pCombinedGeometry = NULL;
		if ( SUCCEEDED( pBaseSurface->AccessD2D1Factory()->CreatePathGeometry( &pCombinedGeometry ) ) )
		{
			ID2D1GeometrySink *pSink;
			if ( SUCCEEDED( pCombinedGeometry->Open( &pSink ) ) )
			{
				if ( SUCCEEDED( pOuterGeometry->CombineWithGeometry( pInnerGeometry, D2D1_COMBINE_MODE_XOR, NULL, 2.0f, pSink ) ) )
				{
					DbgVerify( SUCCEEDED( pSink->Close() ) );
				}
				SAFE_RELEASE( pSink );
			}
		}

		// If we have a single color, we can draw in a single operation
		if ( m_rgbaBorderColors[0] == m_rgbaBorderColors[1] && m_rgbaBorderColors[1] == m_rgbaBorderColors[2] && m_rgbaBorderColors[2] == m_rgbaBorderColors[3] )
		{
			ID2D1SolidColorBrush *pBrush = pBaseSurface->GetSolidColorBrush( m_rgbaBorderColors[0] );
			if ( pBrush )
			{
				m_pD2DRenderTarget->FillGeometry( pCombinedGeometry, pBrush );
				SAFE_RELEASE( pBrush );
			}
		}
		else
		{
			// Have to draw each corner and fill section independently to get correct colors
			D2D1_RECT_F clipRect;
			clipRect.left = 0.0f;
			clipRect.top = 0.0f;
			clipRect.right = m_rgCornerRadii[0];
			clipRect.bottom = m_rgCornerRadii[1];

			m_pD2DRenderTarget->PushAxisAlignedClip( clipRect, D2D1_ANTIALIAS_MODE_ALIASED );

			CMsgLinearGradient msg;
			CMsgColorStop *pStopStart = msg.add_color_stop();
			CMsgColorStop *pStopSecond = msg.add_color_stop();
			pStopStart->set_position( 0.0f );
			pStopStart->set_color_rgba( m_rgbaBorderColors[3] );
			pStopSecond->set_position( 0.3f );
			pStopSecond->set_color_rgba( pStopStart->color_rgba() );
			CMsgColorStop *pStopBeforeEnd = msg.add_color_stop();
			pStopBeforeEnd->set_position( 0.7f );
			CMsgColorStop *pStopEnd = msg.add_color_stop();
			pStopEnd->set_position( 1.0f );
			pStopEnd->set_color_rgba( m_rgbaBorderColors[0] );
			pStopBeforeEnd->set_color_rgba( pStopEnd->color_rgba() );
			msg.mutable_start_position()->set_x( clipRect.left );
			msg.mutable_start_position()->set_y( clipRect.bottom );
			msg.mutable_end_position()->set_x( clipRect.right );
			msg.mutable_end_position()->set_y( clipRect.top );

			ID2D1LinearGradientBrush *pGradientBrush = pBaseSurface->GetLinearGradientBrush( msg );
			if ( pGradientBrush )
			{
				m_pD2DRenderTarget->FillGeometry( pCombinedGeometry, pGradientBrush );
				SAFE_RELEASE( pGradientBrush );
			}

			m_pD2DRenderTarget->PopAxisAlignedClip();

			clipRect.left = m_rgCornerRadii[0];
			clipRect.right = m_flLayerWidth - m_rgCornerRadii[2];
			clipRect.top = 0.0f;
			clipRect.bottom = m_rgBorderWidths[0]+1.0f;

			m_pD2DRenderTarget->PushAxisAlignedClip( clipRect, D2D1_ANTIALIAS_MODE_ALIASED );

			ID2D1SolidColorBrush *pSolidBrush = pBaseSurface->GetSolidColorBrush( m_rgbaBorderColors[0] );
			if ( pSolidBrush )
			{
				m_pD2DRenderTarget->FillGeometry( pCombinedGeometry, pSolidBrush );
				SAFE_RELEASE( pSolidBrush );
			}
			
			m_pD2DRenderTarget->PopAxisAlignedClip();
			
			clipRect.left = m_flLayerWidth-m_rgCornerRadii[2];
			clipRect.top = 0.0f;
			clipRect.right = m_flLayerWidth;
			clipRect.bottom = m_rgCornerRadii[3];

			m_pD2DRenderTarget->PushAxisAlignedClip( clipRect, D2D1_ANTIALIAS_MODE_ALIASED );

			pStopStart->set_color_rgba( m_rgbaBorderColors[0] );
			pStopSecond->set_color_rgba( m_rgbaBorderColors[0] );
			pStopBeforeEnd->set_color_rgba( m_rgbaBorderColors[1] );
			pStopEnd->set_color_rgba( m_rgbaBorderColors[1] );
			msg.mutable_start_position()->set_x( clipRect.left );
			msg.mutable_start_position()->set_y( clipRect.top );
			msg.mutable_end_position()->set_x( clipRect.right );
			msg.mutable_end_position()->set_y( clipRect.bottom );

			pGradientBrush = pBaseSurface->GetLinearGradientBrush( msg );
			if ( pGradientBrush )
			{
				m_pD2DRenderTarget->FillGeometry( pCombinedGeometry, pGradientBrush );
				SAFE_RELEASE( pGradientBrush );
			}

			m_pD2DRenderTarget->PopAxisAlignedClip();

			clipRect.left = m_flLayerWidth-m_rgBorderWidths[1]-1.0f;
			clipRect.right = m_flLayerWidth;
			clipRect.top = m_rgCornerRadii[3];
			clipRect.bottom = m_flLayerHeight - m_rgCornerRadii[5];

			m_pD2DRenderTarget->PushAxisAlignedClip( clipRect, D2D1_ANTIALIAS_MODE_ALIASED );

			pSolidBrush = pBaseSurface->GetSolidColorBrush( m_rgbaBorderColors[1] );
			if ( pSolidBrush )
			{
				m_pD2DRenderTarget->FillGeometry( pCombinedGeometry, pSolidBrush );
				SAFE_RELEASE( pSolidBrush );
			}

			m_pD2DRenderTarget->PopAxisAlignedClip();

			clipRect.left = m_flLayerWidth-m_rgCornerRadii[4];
			clipRect.top = m_flLayerHeight-m_rgCornerRadii[5];
			clipRect.right = m_flLayerWidth;
			clipRect.bottom = m_flLayerHeight;

			m_pD2DRenderTarget->PushAxisAlignedClip( clipRect, D2D1_ANTIALIAS_MODE_ALIASED );

			pStopStart->set_color_rgba( m_rgbaBorderColors[1] );
			pStopSecond->set_color_rgba( m_rgbaBorderColors[1] );
			pStopBeforeEnd->set_color_rgba( m_rgbaBorderColors[2] );
			pStopEnd->set_color_rgba( m_rgbaBorderColors[2] );
			msg.mutable_start_position()->set_x( clipRect.right );
			msg.mutable_start_position()->set_y( clipRect.top );
			msg.mutable_end_position()->set_x( clipRect.left );
			msg.mutable_end_position()->set_y( clipRect.bottom );

			pGradientBrush = pBaseSurface->GetLinearGradientBrush( msg );
			if ( pGradientBrush )
			{
				m_pD2DRenderTarget->FillGeometry( pCombinedGeometry, pGradientBrush );
				SAFE_RELEASE( pGradientBrush );
			}

			m_pD2DRenderTarget->PopAxisAlignedClip();
				
			clipRect.left = m_rgCornerRadii[6];
			clipRect.right = m_flLayerWidth-m_rgCornerRadii[4];
			clipRect.top = m_flLayerHeight-m_rgBorderWidths[2]-1.0f;
			clipRect.bottom = m_flLayerHeight;

			m_pD2DRenderTarget->PushAxisAlignedClip( clipRect, D2D1_ANTIALIAS_MODE_ALIASED );

			pSolidBrush = pBaseSurface->GetSolidColorBrush( m_rgbaBorderColors[2] );
			if ( pSolidBrush )
			{
				m_pD2DRenderTarget->FillGeometry( pCombinedGeometry, pSolidBrush );
				SAFE_RELEASE( pSolidBrush );
			}

			m_pD2DRenderTarget->PopAxisAlignedClip();

			clipRect.left = 0.0f;
			clipRect.top = m_flLayerHeight-m_rgCornerRadii[7];
			clipRect.right = m_rgCornerRadii[6];
			clipRect.bottom = m_flLayerHeight;

			m_pD2DRenderTarget->PushAxisAlignedClip( clipRect, D2D1_ANTIALIAS_MODE_ALIASED );

			pStopStart->set_color_rgba( m_rgbaBorderColors[2] );
			pStopSecond->set_color_rgba( m_rgbaBorderColors[2] );
			pStopBeforeEnd->set_color_rgba( m_rgbaBorderColors[3] );
			pStopEnd->set_color_rgba( m_rgbaBorderColors[3] );
			msg.mutable_start_position()->set_x( clipRect.right );
			msg.mutable_start_position()->set_y( clipRect.bottom );
			msg.mutable_end_position()->set_x( clipRect.left );
			msg.mutable_end_position()->set_y( clipRect.top );

			pGradientBrush = pBaseSurface->GetLinearGradientBrush( msg );
			if ( pGradientBrush )
			{
				m_pD2DRenderTarget->FillGeometry( pCombinedGeometry, pGradientBrush );
				SAFE_RELEASE( pGradientBrush );
			}

			m_pD2DRenderTarget->PopAxisAlignedClip();

			clipRect.left = 0.0f;
			clipRect.right = m_rgBorderWidths[3]+1.0f;
			clipRect.top = m_rgCornerRadii[1];
			clipRect.bottom = m_flLayerHeight-m_rgCornerRadii[7];

			m_pD2DRenderTarget->PushAxisAlignedClip( clipRect, D2D1_ANTIALIAS_MODE_ALIASED );

			pSolidBrush = pBaseSurface->GetSolidColorBrush( m_rgbaBorderColors[3] );
			if ( pSolidBrush )
			{
				m_pD2DRenderTarget->FillGeometry( pCombinedGeometry, pSolidBrush );
				SAFE_RELEASE( pSolidBrush );
			}

			m_pD2DRenderTarget->PopAxisAlignedClip();
		}

		SAFE_RELEASE( pOuterGeometry );
		SAFE_RELEASE( pInnerGeometry );
		SAFE_RELEASE( pCombinedGeometry );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Update LRU/time last used on access
//-----------------------------------------------------------------------------
void CCompositionLayer::UpdateTimeLastUsedAndLRUListForAccess( double flTime, CUtlLinkedList< int, int > &list )
{
	m_flLastTimeUsed = flTime;

	if ( m_iLRUPos != list.InvalidIndex() )
		list.Remove( m_iLRUPos );

	m_iLRUPos = list.InvalidIndex();
}


//-----------------------------------------------------------------------------
// Purpose: Helper to determine if a clip layer is an axis aligned rect
//-----------------------------------------------------------------------------
bool BHasNoRounding( const CRadiusData &msg )
{
	if ( msg.top_left().horizontal() != 0.0f )
		return false;
	if ( msg.top_left().vertical() != 0.0f )
		return false;
	if ( msg.top_right().horizontal() != 0.0f )
		return false;
	if ( msg.top_right().vertical() != 0.0f )
		return false;
	if ( msg.bottom_right().horizontal() != 0.0f )
		return false;
	if ( msg.bottom_right().vertical() != 0.0f )
		return false;
	if ( msg.bottom_left().horizontal() != 0.0f )
		return false;
	if ( msg.bottom_left().vertical() != 0.0f )
		return false;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Do actual d2d layer push
//-----------------------------------------------------------------------------
void CCompositionLayer::PushLayerNow( const ClipLayerData_t &clipLayer )
{
	D2D1_RECT_F rect;
	rect.left = (clipLayer.x0);
	rect.top = (clipLayer.y0);
	rect.right = (clipLayer.x1);
	rect.bottom = (clipLayer.y1);

	m_pD2DRenderTarget->PushAxisAlignedClip( rect, D2D1_ANTIALIAS_MODE_ALIASED );
}


//-----------------------------------------------------------------------------
// Purpose: Do actual d2d layer pop
//-----------------------------------------------------------------------------
void CCompositionLayer::PopLayerNow( const ClipLayerData_t &clipLayer )
{
	m_pD2DRenderTarget->PopAxisAlignedClip();
}


//-----------------------------------------------------------------------------
// Purpose: Push a new clip layer into the composition layer
//-----------------------------------------------------------------------------
void CCompositionLayer::PushClipLayer( const CMsgPushClipLayer &msg )
{
	if ( !m_pVecClipLayers )
	{
		m_pVecClipLayers = new CUtlVector<ClipLayerData_t>;
	}

	// If there is corner rounding, then we must create a composition layer rather than doing d2d axis aligned
	// clipping, so we shouldn't hit this.
	DbgAssert( BHasNoRounding( msg.border_radius() ) );

	ClipLayerData_t &data  = m_pVecClipLayers->Element( m_pVecClipLayers->AddToTail() );

	data.x0 = msg.top_left().x();
	data.x1 = msg.bottom_right().x();
	data.y0 = msg.top_left().y();
	data.y1 = msg.bottom_right().y();

	// If currently drawing to this layer, also update right away
	if ( m_bIsDrawing && m_bBeginDrawDone )
	{
		PushLayerNow( data );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Pop a clip layer out of the composition layer
//-----------------------------------------------------------------------------
void CCompositionLayer::PopClipLayer()
{
	if ( m_pVecClipLayers && m_pVecClipLayers->Count() > 0 )
	{
		ClipLayerData_t &clipLayer = m_pVecClipLayers->Element( m_pVecClipLayers->Count() -1 );
		if ( m_bIsDrawing && m_bBeginDrawDone )
		{
			PopLayerNow( clipLayer );
		}

		m_pVecClipLayers->Remove( m_pVecClipLayers->Count() - 1 );
	}
	else
	{
		AssertMsg( false, "Called CCompositionLayer::PopClipLayer with no clip layers pushed" );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get the current clip rect
//-----------------------------------------------------------------------------
void CCompositionLayer::GetCurrentClipRect( D3D10_RECT &r )
{
	if ( m_pVecClipLayers && m_pVecClipLayers->Count() > 0 )
	{
		ClipLayerData_t &data = m_pVecClipLayers->Element( m_pVecClipLayers->Count() -1 );
		r.left = data.x0;
		r.top = data.y0;
		r.right = data.x1;
		r.bottom = data.y1;
	}
	else
	{
		r.left = 0;
		r.top = 0;
		r.right = m_flLayerWidth;
		r.bottom = m_flLayerHeight;
	}
}


typedef CCompositionLayer * CCompositionLayerPtr;
bool CCompositionLayerLessThan( const CCompositionLayerPtr &lhs, const CCompositionLayerPtr &rhs )	
{
	return *lhs < *rhs;
}

typedef CRenderMsg<CMsgLinearGradient>::Node * CLinearGradientMapKey;
bool CLinearGradientLessThan( const CLinearGradientMapKey &lhsNode, const CLinearGradientMapKey &rhsNode )
{
	CMsgLinearGradient *lhs = lhsNode->elem;
	CMsgLinearGradient *rhs = rhsNode->elem;

	if ( lhs->start_position().x() < rhs->start_position().x() )
		return true;
	else if ( lhs->start_position().x() > rhs->start_position().x() )
		return false;

	if ( lhs->start_position().y() < rhs->start_position().y() )
		return true;
	else if ( lhs->start_position().y() > rhs->start_position().y() )
		return false;


	if ( lhs->end_position().x() < rhs->end_position().x() )
		return true;
	else if ( lhs->end_position().x() > rhs->end_position().x() )
		return false;

	if ( lhs->end_position().y() < rhs->end_position().y() )
		return true;
	else if ( lhs->end_position().y() > rhs->end_position().y() )
		return false;

	if ( lhs->color_stop_size() < rhs->color_stop_size() )
		return true;
	else if ( lhs->color_stop_size() > rhs->color_stop_size() )
		return false;


	Assert( lhs->color_stop_size() == rhs->color_stop_size() );
	for( int i=0; i < lhs->color_stop_size(); ++i )
	{
		if ( lhs->color_stop(i).color_rgba() < rhs->color_stop(i).color_rgba() )
			return true;
		else if( lhs->color_stop(i).color_rgba() > rhs->color_stop(i).color_rgba() )
			return false;

		if ( lhs->color_stop(i).color_rgba() < rhs->color_stop(i).color_rgba() )
			return true;
		else if( lhs->color_stop(i).color_rgba() > rhs->color_stop(i).color_rgba() )
			return false;
	}

	return false;
}


typedef const CMsgRadialGradient * CRadialGradientMapKey;
bool CRadialGradientLessThan( const CRadialGradientMapKey &lhs, const CRadialGradientMapKey &rhs )
{
	if ( lhs->center_position().x() < rhs->center_position().x() )
		return true;
	else if ( lhs->center_position().x() > rhs->center_position().x() )
		return false;

	if ( lhs->center_position().y() < rhs->center_position().y() )
		return true;
	else if ( lhs->center_position().y() > rhs->center_position().y() )
		return false;


	if ( lhs->offset_distance().x() < rhs->offset_distance().x() )
		return true;
	else if ( lhs->offset_distance().x() > rhs->offset_distance().x() )
		return false;

	if ( lhs->offset_distance().y() < rhs->offset_distance().y() )
		return true;
	else if ( lhs->offset_distance().y() > rhs->offset_distance().y() )
		return false;

	if ( lhs->radii().x() < rhs->radii().x() )
		return true;
	else if ( lhs->radii().x() > rhs->radii().x() )
		return false;

	if ( lhs->radii().y() < rhs->radii().y() )
		return true;
	else if ( lhs->radii().y() > rhs->radii().y() )
		return false;

	if ( lhs->color_stop_size() < rhs->color_stop_size() )
		return true;
	else if ( lhs->color_stop_size() > rhs->color_stop_size() )
		return false;


	Assert( lhs->color_stop_size() == rhs->color_stop_size() );
	for( int i=0; i < lhs->color_stop_size(); ++i )
	{
		if ( lhs->color_stop(i).color_rgba() < rhs->color_stop(i).color_rgba() )
			return true;
		else if( lhs->color_stop(i).color_rgba() > rhs->color_stop(i).color_rgba() )
			return false;

		if ( lhs->color_stop(i).color_rgba() < rhs->color_stop(i).color_rgba() )
			return true;
		else if( lhs->color_stop(i).color_rgba() > rhs->color_stop(i).color_rgba() )
			return false;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Helper for converting texture types for DXGI use
//-----------------------------------------------------------------------------
DXGI_FORMAT GetDXGIFormatForUI2DTextureFormat( E2DTextureFormat eFormat )
{
	switch( eFormat )
	{
	case k_EFormatRGBA8:
		return DXGI_FORMAT_R8G8B8A8_UNORM;
	case k_EFormatBGRA8:
		return DXGI_FORMAT_B8G8R8A8_UNORM;
	case k_EFormatBGR8:
		return DXGI_FORMAT_B8G8R8X8_UNORM;
	case k_EFormatA8:
		return DXGI_FORMAT_A8_UNORM;
	case k_EFormatYUV420:
		return DXGI_FORMAT_R8_UNORM;
	case k_EFormatR16G16B16A16:
		return DXGI_FORMAT_R16G16B16A16_UNORM;
	}

	return DXGI_FORMAT_UNKNOWN;
}


//-----------------------------------------------------------------------------
// Purpose: Helper for determining pixel size for textures in a given format
//-----------------------------------------------------------------------------
uint32 GetPixelBytesForUI2DTextureFormat( E2DTextureFormat eFormat )
{
	switch( eFormat )
	{
	case k_EFormatRGBA8:
		return 4;
	case k_EFormatBGRA8:
		return 4;
	case k_EFormatBGR8:
		return 4;
	case k_EFormatA8:
		return 1;
	case k_EFormatYUV420:
		return 1;
	case k_EFormatR16G16B16A16:
		return 8;
	}

	return 0;
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CD3D10Texture::CD3D10Texture( ID3D10Device *pDevice, uint32 unTextureID, void *pubTextureData, uint32 unWidth, uint32 unHeight, uint32 unStride, E2DTextureFormat eFormat, EAlphaChannelType eAlphaChannelType )
{
	VPROF_BUDGET( "CD3D10Texture - constructor", VPROF_BUDGETGROUP_TENFOOT );
	m_unTextureID = unTextureID;
	Assert( m_unTextureID != 0 );
	uint32 unPixelBytes = GetPixelBytesForUI2DTextureFormat( eFormat );
	m_unWidth = unWidth;
	m_unHeight = unHeight;
	m_unStride = unStride*unPixelBytes;
	m_eFormat = eFormat;
	m_eAlphaChannelType = eAlphaChannelType;
	m_pTexture = NULL;
	m_pTextureView = NULL;


	// Create texture resources
	D3D10_TEXTURE2D_DESC desc;
	desc.Width = m_unWidth;
	desc.Height = m_unHeight;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = GetDXGIFormatForUI2DTextureFormat( eFormat );
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D10_USAGE_DYNAMIC;
	desc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
	desc.MiscFlags = 0;

	D3D10_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;
	renderTargetViewDesc.Format = desc.Format;
	renderTargetViewDesc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2D;
	renderTargetViewDesc.Texture2D.MipSlice = 0;

	DbgVerify( SUCCEEDED( pDevice->CreateTexture2D( &desc, NULL, &m_pTexture ) ) );
	DbgVerify( SUCCEEDED( pDevice->CreateShaderResourceView( m_pTexture, NULL, &m_pTextureView ) ) );


	Assert( m_pTexture && m_pTextureView );
	if ( m_pTexture && m_pTextureView )
	{
		UINT subResource = D3D10CalcSubresource(0, 0, 1);

		uint32 unPixelsBytesTotal = m_unStride*m_unHeight;
		D3D10_MAPPED_TEXTURE2D mappedTex;
		HRESULT hres;
		{
			VPROF_BUDGET( "CD3D10Texture -- map", VPROF_BUDGETGROUP_TENFOOT );
			hres = m_pTexture->Map( subResource, D3D10_MAP_WRITE_DISCARD, 0, &mappedTex );
		}
		if ( SUCCEEDED( hres ) )
		{
			if ( m_unStride == mappedTex.RowPitch )
			{
				VPROF_BUDGET( "CD3D10Texture -- copy", VPROF_BUDGETGROUP_TENFOOT );
				V_memcpy_nocache( mappedTex.pData, pubTextureData, unPixelsBytesTotal );
			}
			else
			{
				VPROF_BUDGET( "CD3D10Texture -- copy", VPROF_BUDGETGROUP_TENFOOT );
				for( uint32 i=0; i < m_unHeight; ++i )
				{
					V_memcpy_nocache( (byte*)mappedTex.pData + ( i * mappedTex.RowPitch ), (byte*)pubTextureData+ ( i * m_unStride ), m_unStride );
				}
			}
			m_pTexture->Unmap( subResource );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CD3D10Texture::~CD3D10Texture()
{
	SAFE_RELEASE( m_pTextureView );
	SAFE_RELEASE( m_pTexture );
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CD3D10DoubleBufferedTexture::CD3D10DoubleBufferedTexture( ID3D10Device *pDevice, uint32 unTextureID, uint32 unTextureWidth, uint32 unTextureHeight, uint32 unStride, E2DTextureFormat eFormat, EAlphaChannelType eAlphaChannelType, bool bSerializedUploads )
{
	VPROF_BUDGET( "CD3D10DoubleBufferedTexture - constructor", VPROF_BUDGETGROUP_TENFOOT );
	m_unTextureID = unTextureID;
	Assert( m_unTextureID != 0 );
	m_unWidth = unTextureWidth;
	m_unHeight = unTextureHeight;
	m_eFormat = eFormat;
	uint32 unPixelBytes = GetPixelBytesForUI2DTextureFormat( m_eFormat );
	m_unStride = unStride*unPixelBytes;
	m_eAlphaChannelType = eAlphaChannelType;
	m_bSerializedUploads = bSerializedUploads;

	m_iCurRenderTexture = -1;
	m_iPendingUpload = -1;
	m_nSerial = 1; // 0 means ignore serial numbers so seed from 1
	m_nDrawSerial = 0;

	for( int i=0; i < V_ARRAYSIZE( m_rgTextures ); ++i )
	{
		// Create texture resources
		D3D10_TEXTURE2D_DESC desc;
		desc.Width = unTextureWidth;
		desc.Height = unTextureHeight;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = GetDXGIFormatForUI2DTextureFormat( m_eFormat );
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Usage = D3D10_USAGE_DYNAMIC;
		desc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
		desc.MiscFlags = 0;

		D3D10_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;
		renderTargetViewDesc.Format = desc.Format;
		renderTargetViewDesc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2D;
		renderTargetViewDesc.Texture2D.MipSlice = 0;

		DbgVerify( SUCCEEDED( pDevice->CreateTexture2D( &desc, NULL, &m_rgTextures[i].m_pTexture ) ) );
		DbgVerify( SUCCEEDED( pDevice->CreateShaderResourceView( m_rgTextures[i].m_pTexture, NULL, &m_rgTextures[i].m_pTextureView ) ) );

		Assert( m_rgTextures[i].m_pTexture && m_rgTextures[i].m_pTextureView );
		m_rgTextures[i].m_nSerial = -1;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CD3D10DoubleBufferedTexture::~CD3D10DoubleBufferedTexture()
{
	{
		AUTO_LOCK( m_IndexLock );
		m_iCurRenderTexture = -1;
	}

	for( int i=0; i < V_ARRAYSIZE( m_rgTextures ); ++i )
	{
		AUTO_LOCK( m_rgTextures[i].m_Lock );
		SAFE_RELEASE( m_rgTextures[i].m_pTextureView );
		SAFE_RELEASE( m_rgTextures[i].m_pTexture );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Call to update texture data for next rendering frame
//-----------------------------------------------------------------------------
int32 CD3D10DoubleBufferedTexture::UpdateTextureData( void *pBuffer )
{
	Assert( pBuffer );
	// Defensive programming -- we have hit this case at least once
	if ( !pBuffer )
		return m_nSerial;

	VPROF_BUDGET( "CD3D10DoubleBufferedTexture::UpdateTextureData", VPROF_BUDGETGROUP_TENFOOT );

	m_IndexLock.Lock();
	int iUpdateIndex = ( m_iPendingUpload + 1 ) % V_ARRAYSIZE(m_rgTextures);

	while ( m_bSerializedUploads )
	{
		if ( m_rgTextures[iUpdateIndex].m_nSerial < m_nDrawSerial )
		{
			break;
		}
		else
		{
			VPROF_BUDGET( "CD3D10DoubleBufferedTexture::UpdateTextureData - wait for free upload slot", VPROF_BUDGETGROUP_TENFOOT );
			m_IndexLock.Unlock();
			m_DrawEvent.Wait();
			m_IndexLock.Lock();
		}
		
		iUpdateIndex = ( m_iPendingUpload + 1 ) % V_ARRAYSIZE(m_rgTextures);
	}

	m_iPendingUpload++;

	{
		VPROF_BUDGET( "CD3D10DoubleBufferedTexture::UpdateTextureData - lock/sleep for double buffering", VPROF_BUDGETGROUP_TENFOOT );
		m_rgTextures[iUpdateIndex].m_Lock.Lock();
	}

	m_IndexLock.Unlock();

	UINT subResource = D3D10CalcSubresource(0, 0, 1);

	uint32 unPixelBytesTotal = m_unStride*m_unHeight;
	D3D10_MAPPED_TEXTURE2D mappedTex;
	HRESULT hres;
	{
		VPROF_BUDGET( "CD3D10DoubleBufferedTexture -- map", VPROF_BUDGETGROUP_TENFOOT );
		hres = m_rgTextures[iUpdateIndex].m_pTexture->Map( subResource, D3D10_MAP_WRITE_DISCARD, 0, &mappedTex );
	}
	if ( SUCCEEDED( hres ) )
	{
		if ( m_unStride == mappedTex.RowPitch )
		{
			VPROF_BUDGET( "CD3D10DoubleBufferedTexture -- copy", VPROF_BUDGETGROUP_TENFOOT );
			V_memcpy_nocache( mappedTex.pData, pBuffer, unPixelBytesTotal );
		}
		else
		{
			VPROF_BUDGET( "CD3D10DoubleBufferedTexture -- copy", VPROF_BUDGETGROUP_TENFOOT );
			for( uint32 i=0; i < m_unHeight; ++i )
			{
				V_memcpy_nocache( (byte*)mappedTex.pData + ( i * mappedTex.RowPitch ), (byte*)pBuffer+( i * m_unStride ), m_unStride );
			}
		}
		m_rgTextures[iUpdateIndex].m_pTexture->Unmap( subResource );
	}

	m_rgTextures[iUpdateIndex].m_Lock.Unlock();

	VPROF_BUDGET( "CD3D10DoubleBufferedTexture -- indexlock", VPROF_BUDGETGROUP_TENFOOT );
	AUTO_LOCK( m_IndexLock );

	m_nSerial++;
	m_rgTextures[iUpdateIndex].m_nSerial = m_nSerial;


	if ( !m_bSerializedUploads || m_iCurRenderTexture == -1 )
		m_iCurRenderTexture = iUpdateIndex; // commit the upload texture if its immediate mode or the first upload


	return m_nSerial;
}


//-----------------------------------------------------------------------------
// Purpose: Return texture data, locking it such that it won't be modified underneath caller (which must be render thread)
//-----------------------------------------------------------------------------
int CD3D10DoubleBufferedTexture::LockAndGetCurrentTexture( ID3D10Texture2D **ppTexture, ID3D10ShaderResourceView **ppResourceView, int nSerial )
{
	if ( ppTexture )
		*ppTexture = NULL;

	if ( ppResourceView )
		*ppResourceView = NULL;

	int iIndex = -1;
	m_IndexLock.Lock();
	iIndex = m_iCurRenderTexture;
	if ( iIndex == -1 )
	{
		// No texture actually ready to render
		m_IndexLock.Unlock();
		return -1;
	}

	if ( nSerial != 0 ) // if we want to track the actual texture page we render with
	{
		 int32 nTextureSerial = m_rgTextures[iIndex].m_nSerial;
		if ( nTextureSerial < nSerial ) // did we move to a new serial
		{
			Assert( m_iPendingUpload >= 0 );

			int iUpdateIndex = ( iIndex + 1 )% V_ARRAYSIZE( m_rgTextures ); // grab the next image
			Assert( nSerial == m_rgTextures[ iUpdateIndex ].m_nSerial );

			m_iCurRenderTexture = iUpdateIndex;
			iIndex = iUpdateIndex;
	
		}
	}

	CD3D10DoubleBufferedTexture::Texture_t &textureData = m_rgTextures[iIndex];

	m_nDrawSerial = textureData.m_nSerial;
	m_DrawEvent.Set(); // pulse the update thread as we have moved on

	// We return still holding this lock! Unlock call will unlock it.
	textureData.m_Lock.Lock();
	m_IndexLock.Unlock();

	Assert( !m_bSerializedUploads || nSerial == textureData.m_nSerial );

	if ( ppTexture )
		*ppTexture = textureData.m_pTexture;

	if ( ppResourceView )
		*ppResourceView = textureData.m_pTextureView;

	return iIndex;
}


//-----------------------------------------------------------------------------
// Purpose: Return texture data, locking it such that it won't be modified underneath caller (which must be render thread)
//-----------------------------------------------------------------------------
void CD3D10DoubleBufferedTexture::Unlock( int iLockHandle )
{
	if ( iLockHandle != -1 )
	{
		m_rgTextures[iLockHandle].m_Lock.Unlock();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CD3D10DoubleBufferedYUV420Texture::CD3D10DoubleBufferedYUV420Texture( CD3D10D2DSurface *pSurface, ID3D10Device *pDevice, uint32 unTextureID, uint32 unTextureWidth, uint32 unTextureHeight )
{
	VPROF_BUDGET( "CD3D10DoubleBufferedYUV420Texture - constructor", VPROF_BUDGETGROUP_TENFOOT );
	m_unTextureID = unTextureID;
	Assert( m_unTextureID != 0 );
	m_unWidth = unTextureWidth;
	m_unHeight = unTextureHeight;
	m_iCurRenderTexture = -1;
	m_flLastRenderThreadFrameTimeOnUpdate = -1.0f;
	m_pSurface = pSurface;

	for( int i=0; i < V_ARRAYSIZE( m_rgTextures ); ++i )
	{
		// Create texture resources
		D3D10_TEXTURE2D_DESC desc;
		desc.Width = unTextureWidth;
		desc.Height = unTextureHeight;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_R8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Usage = D3D10_USAGE_DYNAMIC;
		desc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
		desc.MiscFlags = 0;

		D3D10_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;
		renderTargetViewDesc.Format = desc.Format;
		renderTargetViewDesc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2D;
		renderTargetViewDesc.Texture2D.MipSlice = 0;

		DbgVerify( SUCCEEDED( pDevice->CreateTexture2D( &desc, NULL, &m_rgTextures[i].m_pYTexture ) ) );
		DbgVerify( SUCCEEDED( pDevice->CreateShaderResourceView( m_rgTextures[i].m_pYTexture, NULL, &m_rgTextures[i].m_pYTextureView ) ) );

		// Half size for U/V
		desc.Width = MAX( unTextureWidth/2, 1 );
		desc.Height = MAX( unTextureHeight/2, 1 );

		DbgVerify( SUCCEEDED( pDevice->CreateTexture2D( &desc, NULL, &m_rgTextures[i].m_pUTexture ) ) );
		DbgVerify( SUCCEEDED( pDevice->CreateShaderResourceView( m_rgTextures[i].m_pUTexture, NULL, &m_rgTextures[i].m_pUTextureView ) ) );
		DbgVerify( SUCCEEDED( pDevice->CreateTexture2D( &desc, NULL, &m_rgTextures[i].m_pVTexture ) ) );
		DbgVerify( SUCCEEDED( pDevice->CreateShaderResourceView( m_rgTextures[i].m_pVTexture, NULL, &m_rgTextures[i].m_pVTextureView ) ) );

		Assert( m_rgTextures[i].m_pYTexture && m_rgTextures[i].m_pYTextureView && m_rgTextures[i].m_pUTexture && m_rgTextures[i].m_pUTextureView && m_rgTextures[i].m_pVTexture && m_rgTextures[i].m_pVTextureView );
	}

	// need to retrieve texture stride
	m_unTextureStride = 0;
	if ( m_rgTextures[0].m_pYTexture )
	{
		// not seeing an easier way than to map this into memory
		UINT subResource = D3D10CalcSubresource(0, 0, 1);
		D3D10_MAPPED_TEXTURE2D mappedTex;
		if ( SUCCEEDED( m_rgTextures[0].m_pYTexture->Map( subResource, D3D10_MAP_WRITE_DISCARD, 0, &mappedTex ) ) )
		{
			m_unTextureStride = mappedTex.RowPitch;
			m_rgTextures[0].m_pYTexture->Unmap( subResource );
		}		
	}
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CD3D10DoubleBufferedYUV420Texture::~CD3D10DoubleBufferedYUV420Texture()
{
	{
		AUTO_LOCK( m_IndexLock );
		m_iCurRenderTexture = -1;
	}

	for( int i=0; i < V_ARRAYSIZE( m_rgTextures ); ++i )
	{
		AUTO_LOCK( m_rgTextures[i].m_Lock );
		SAFE_RELEASE( m_rgTextures[i].m_pYTextureView );
		SAFE_RELEASE( m_rgTextures[i].m_pYTexture );
		SAFE_RELEASE( m_rgTextures[i].m_pUTextureView );
		SAFE_RELEASE( m_rgTextures[i].m_pUTexture );
		SAFE_RELEASE( m_rgTextures[i].m_pVTextureView );
		SAFE_RELEASE( m_rgTextures[i].m_pVTexture );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Call to update YUV texture data for next rendering frame
//-----------------------------------------------------------------------------
bool CD3D10DoubleBufferedYUV420Texture::BUpdateTextureData( void *pYBuffer, void *pUBuffer, void *pVBuffer, uint unStrideY, uint unStrideU, uint unStrideV )
{
	VPROF_BUDGET( "CD3D10DoubleBufferedYUV420Texture::UpdateTextureData", VPROF_BUDGETGROUP_TENFOOT );
	int iUpdateIndex = m_iCurRenderTexture + 1;
	if ( iUpdateIndex > V_ARRAYSIZE( m_rgTextures )-1 )
		iUpdateIndex = 0;

	{
		{
			VPROF_BUDGET( "CD3D10DoubleBufferedYUV420Texture::UpdateTextureData - waiting on rendering", VPROF_BUDGETGROUP_TENFOOT );
			int nIterations = 0;
			while( m_pSurface->GetCurrentRenderThreadFrameTime() <= m_flLastRenderThreadFrameTimeOnUpdate  && nIterations < 5 )
			{
				ThreadSleep( 2 );
				++nIterations;
			}

			if ( m_pSurface->GetCurrentRenderThreadFrameTime() <= m_flLastRenderThreadFrameTimeOnUpdate )
			{
				// webmdecoder will handle this and retry, early outing here lets it continue to decode audio which may need to update sooner
				//Msg( "Waited longer than 10ms on render thread to move forward for YUV420 texture update, refusing update." );
				return false;
			}
		}

		{
			VPROF_BUDGET( "CD3D10DoubleBufferedYUV420Texture::UpdateTextureData - lock/sleep for double buffering", VPROF_BUDGETGROUP_TENFOOT );
			m_rgTextures[iUpdateIndex].m_Lock.Lock();
		}	


		m_flLastRenderThreadFrameTimeOnUpdate = m_pSurface->GetCurrentRenderThreadFrameTime();

		UINT subResource = D3D10CalcSubresource(0, 0, 1);

		D3D10_MAPPED_TEXTURE2D mappedTex;
		HRESULT hres;
		{
			VPROF_BUDGET( "CD3D10DoubleBufferedYUV420Texture -- map", VPROF_BUDGETGROUP_TENFOOT );
			hres = m_rgTextures[iUpdateIndex].m_pYTexture->Map( subResource, D3D10_MAP_WRITE_DISCARD, 0, &mappedTex );
		}
		if ( SUCCEEDED( hres ) )
		{
			if ( unStrideY == mappedTex.RowPitch )
			{
				VPROF_BUDGET( "CD3D10DoubleBufferedYUV420Texture -- copy", VPROF_BUDGETGROUP_TENFOOT );
				V_memcpy_nocache( mappedTex.pData, pYBuffer, unStrideY * m_unHeight );
			}
			else
			{
				VPROF_BUDGET( "CD3D10DoubleBufferedYUV420Texture -- copy", VPROF_BUDGETGROUP_TENFOOT );
				for( uint32 i=0; i < m_unHeight; ++i )
				{
					V_memcpy_nocache( (byte*)mappedTex.pData + ( i * mappedTex.RowPitch ), (byte*)pYBuffer+ ( i * unStrideY ), m_unWidth );
				}
			}
			m_rgTextures[iUpdateIndex].m_pYTexture->Unmap( subResource );
		}

		uint32 unHalfHeight = MAX( m_unHeight/2, 1 );
		uint32 unHalfWidth = MAX( m_unWidth/2, 1 );
		{
			VPROF_BUDGET( "CD3D10DoubleBufferedYUV420Texture -- map", VPROF_BUDGETGROUP_TENFOOT );
			hres = m_rgTextures[iUpdateIndex].m_pUTexture->Map( subResource, D3D10_MAP_WRITE_DISCARD, 0, &mappedTex );
		}
		if ( SUCCEEDED( hres ) )
		{
			if ( unStrideU == mappedTex.RowPitch )
			{
				VPROF_BUDGET( "CD3D10DoubleBufferedYUV420Texture -- copy", VPROF_BUDGETGROUP_TENFOOT );
				V_memcpy_nocache( mappedTex.pData, pUBuffer, unStrideU * unHalfHeight );
			}
			else
			{
				VPROF_BUDGET( "CD3D10DoubleBufferedYUV420Texture -- copy", VPROF_BUDGETGROUP_TENFOOT );
				for( uint32 i=0; i < unHalfHeight; ++i )
				{
					V_memcpy_nocache( (byte*)mappedTex.pData + ( i * mappedTex.RowPitch ), (byte*)pUBuffer + ( i * unStrideU ), unHalfWidth );
				}
			}
			m_rgTextures[iUpdateIndex].m_pUTexture->Unmap( subResource );
		}

		{
			VPROF_BUDGET( "CD3D10DoubleBufferedYUV420Texture -- map", VPROF_BUDGETGROUP_TENFOOT );
			hres = m_rgTextures[iUpdateIndex].m_pVTexture->Map( subResource, D3D10_MAP_WRITE_DISCARD, 0, &mappedTex );
		}
		if ( SUCCEEDED( hres ) ) 
		{
			if ( unStrideV == mappedTex.RowPitch )
			{
				VPROF_BUDGET( "CD3D10DoubleBufferedYUV420Texture -- copy", VPROF_BUDGETGROUP_TENFOOT );
				V_memcpy_nocache( mappedTex.pData, pVBuffer, unStrideV * unHalfHeight );
			}
			else
			{
				VPROF_BUDGET( "CD3D10DoubleBufferedYUV420Texture -- copy", VPROF_BUDGETGROUP_TENFOOT );
				for( uint32 i=0; i < unHalfHeight; ++i )
				{
					V_memcpy_nocache( (byte*)mappedTex.pData + ( i * mappedTex.RowPitch ), (byte*)pVBuffer + ( i * unStrideV ), unHalfWidth );
				}
			}
			m_rgTextures[iUpdateIndex].m_pVTexture->Unmap( subResource );
		}

		m_rgTextures[iUpdateIndex].m_Lock.Unlock();
	}

	{
		VPROF_BUDGET( "CD3D10DoubleBufferedYUV420Texture -- indexlock", VPROF_BUDGETGROUP_TENFOOT );
		AUTO_LOCK( m_IndexLock );
		m_iCurRenderTexture = iUpdateIndex;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Return texture data, locking it such that it won't be modified underneath caller (which must be render thread)
//-----------------------------------------------------------------------------
int CD3D10DoubleBufferedYUV420Texture::LockAndGetCurrentTextures( ID3D10ShaderResourceView **ppYView, ID3D10ShaderResourceView **ppUView, ID3D10ShaderResourceView **ppVView )
{
	*ppYView = NULL;
	*ppUView = NULL;
	*ppVView = NULL;

	int iIndex = -1;
	m_IndexLock.Lock();
	iIndex = m_iCurRenderTexture;

	if ( iIndex == -1 )
	{
		// No texture actually ready to render
		m_IndexLock.Unlock();
		return -1;
	}

	CD3D10DoubleBufferedYUV420Texture::YUV420Texture_t &textureData = m_rgTextures[iIndex];
	
	// We return still holding this lock! Unlock call will unlock it.
	textureData.m_Lock.Lock();
	
	m_IndexLock.Unlock();

	*ppYView = textureData.m_pYTextureView;
	*ppUView = textureData.m_pUTextureView;
	*ppVView = textureData.m_pVTextureView;

	return iIndex;
}


//-----------------------------------------------------------------------------
// Purpose: Return texture data, locking it such that it won't be modified underneath caller (which must be render thread)
//-----------------------------------------------------------------------------
void CD3D10DoubleBufferedYUV420Texture::Unlock( int iLockHandle )
{
	if ( iLockHandle != -1 )
	{
		m_rgTextures[iLockHandle].m_Lock.Unlock();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CD3D10D2DSurface::CD3D10D2DSurface() : m_CompositionLayerPool( sizeof( CCompositionLayer ), 64 )
{
	++s_nSurfaces;

#ifdef _NVPERFKIT
	m_hNVPMContext = 0;
	if ( s_nSurfaces == 1 )
	{
		if(GetNvPmApiManager()->Construct(PATH_TO_NVPMAPI_CORE) != S_OK)
		{
			Msg( "NVPerfKit Construct() Error\n" );
		}
		else
		{
			NVPMRESULT nvResult;
			if((nvResult = GetNvPmApi()->Init()) != NVPM_OK)
			{
				Msg( "NVPerfKit Init() Error\n" );
			}
		}
	}
#endif
	m_bCrashed = false;
	m_bShaderVarsDirty = true;
	m_ulDedicatedGPUMem = 0;
	m_ulDedicatedSysMem = 0;
	m_ulSharedSysMem = 0;
	m_flLastStatsDump = 0.0f;
	m_eRenderTarget = IUIEngine::k_ERenderTargetUnset;
	m_pDXGISwapChain = NULL;
	m_pD3D10Device = NULL;
	m_pDXGIFactory = NULL;
	m_pDWriteRenderingParams = NULL;
	m_pRasterizerState = NULL;
	m_bVsyncEnabled = true; // we don't actually know yet. We'll set the real value in BInitialize
	m_bHmdReady = false;
	m_ulVROverlayHandle = vr::k_ulOverlayHandleInvalid;
	m_unSurfaceWidth = 0;
	m_unSurfaceHeight = 0;
	m_unWindowWidth = 0;
	m_unWindowHeight = 0;
	m_nOverlayTextureID = 0;
	m_bFixedSurfaceSize = false;
	m_bSurfaceOccluded = false;
	m_flCurrentRenderFrameTime = 0.0f;
	m_pTechnique = NULL;
	m_pViewportHeight = NULL;
	m_pViewportWidth = NULL;
	m_pDiffuseTex = NULL;
	m_pTechniqueBlur = NULL;
	m_pIncrementalGaussian = NULL;
	m_pBlurDirectionVecPass1 = NULL;
	m_pBlurDirectionVecPass2 = NULL;
	m_pBlurDirectionVecPass3 = NULL;
	m_pBlurDirectionVecPass4 = NULL;
	m_pTechniqueYUV420 = NULL;
	m_pYTex = NULL;
	m_pUTex = NULL;
	m_pVTex = NULL;
	m_pOpacityMaskTex = NULL;
	m_pOpacityMaskTexTwo = NULL;
	m_pflOpacityMaskOneBase = NULL;
	m_pflOpacityMaskOneOpacity = NULL;
	m_pflOpacityMaskTwoBase = NULL;
	m_pmatTransform = NULL;
	m_pflSaturation = NULL;
	m_pflHueShift = NULL;
	m_pflBrightness = NULL;
	m_pflContrast = NULL;
	m_pRenderEffect = NULL;
	m_pLastDrawTechnique = NULL;
	m_pFlushTechnique = NULL;
	m_pUITextureOpaqueMask = NULL;
	m_ERenderState = k_ERenderStateUnset;
	m_pTextRenderer = NULL;
	m_bEnforceAspectRatio = false;
	m_flScaleBackbufferX = 1.0f;
	m_flTranslateBackbufferX = 0.0f;
	m_flScaleBackbufferY = 1.0f;
	m_flTranslateBackbufferY = 0.0f;
	m_pBackBuffer = NULL;
	m_pD2DRenderTarget = NULL;
	m_pRenderTargetView = NULL;
	m_pCompositionLayer = NULL;
	m_pBackBufferSharedMemStream = NULL;
	m_pBackBufferSharedMemEvent = NULL;
	m_pBackBufferSharedMemWriteEvent = NULL;
	m_unBackBufferSharedMenEventFails = 0;
	m_dwTargetOverlayPID = 0;
	m_flRenderFrameTime = 0.0f;
	m_nFramesRendered = 0;
	m_nSlowFPSPeriod = 0;
	m_nSessionFramesRendered = 0;
	m_flRenderSessionFrameTime = 0.0f;
	m_pSharedTexCopy = NULL;
	m_pKeyedMutex = NULL;
	m_hSharedText = NULL;
	m_pTextTextureCache = NULL;
	m_pTextLayoutDrawCache = NULL;

	m_rpDistortionMap[0] = NULL;
	m_rpDistortionMap[1] = NULL;

	m_nLastFrameMillisecondsIndex = -1;
	for( int i=0; i < V_ARRAYSIZE( m_rgflMillisecondsFrame ); ++i )
	{
		m_rgflMillisecondsFrame[i] = FLT_MAX;
	}

	m_treeFreeCompositionLayers.SetLessFunc( CCompositionLayerLessThan );
	m_mapLinearGradientBrushes.SetLessFunc( CLinearGradientLessThan );
	m_mapRadialGradientBrushes.SetLessFunc( CRadialGradientLessThan );
		
	m_pTextRenderer = new CDWriteTextRenderer( this );

	ZeroMemory( &m_leftSteamPadPointer, sizeof( m_leftSteamPadPointer ) );
	ZeroMemory( &m_rightSteamPadPointer, sizeof( m_rightSteamPadPointer ) );
	
	m_renderThreadID = (ThreadId_t)0;

#ifdef PANORAMA_STEAMUI_STREAMING_CAPTURE_WIN32
	m_pSteamUIStreamingCallback = NULL;
#endif

#ifdef PANORAMA_PUBLIC_STEAM_SDK
	// Need to make sure VR APIs available for trackpad controllers
	vrapi::EnsureOpenVRAPILoaded();
#endif // PANORAMA_PUBLIC_STEAM_SDK
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CD3D10D2DSurface::~CD3D10D2DSurface()
{
	--s_nSurfaces;
	FlushCurrentVertexBuffer( m_pTechnique );

	ClearShaderResourceVariables();

	if ( m_eRenderTarget == IUIEngine::k_ERenderToOpenVROverlay && m_ulVROverlayHandle != vr::k_ulOverlayHandleInvalid )
	{
		vrapi::VROverlay()->ClearOverlayTexture( m_ulVROverlayHandle );
	}

	// Also in the map, don't need to release directly
	m_pUITextureOpaqueMask = NULL;
	FOR_EACH_MAP_FAST( m_mapTextures, i )
	{
		delete m_mapTextures[i];
	}
	m_mapTextures.RemoveAll();

	FOR_EACH_MAP_FAST( m_mapBorderRadiusOpacityMasks, i )
	{
		SAFE_RELEASE( m_mapBorderRadiusOpacityMasks[i].m_pTexture2D );
		SAFE_RELEASE( m_mapBorderRadiusOpacityMasks[i].m_pShaderResourceView );
	}
	m_mapBorderRadiusOpacityMasks.RemoveAll();
	m_listBorderRadiusOpacityMaskLRU.RemoveAll();

		
	FOR_EACH_MAP_FAST( m_mapSolidColorBrushes, i )
	{
		SAFE_RELEASE( m_mapSolidColorBrushes[i].m_pBrush );
	}
	m_listSolidColorBrushLRU.RemoveAll();
	m_mapSolidColorBrushes.RemoveAll();

	FOR_EACH_MAP_FAST( m_mapLinearGradientBrushes, i )
	{
		SAFE_RELEASE( m_mapLinearGradientBrushes[i].m_pBrush );
		CRenderMsg<CMsgLinearGradient>::FreeProtoBufMsgObject( m_mapLinearGradientBrushes.Key(i) );
	}
	m_listLinearGradientBrushLRU.RemoveAll();
	m_mapLinearGradientBrushes.RemoveAll();

	FOR_EACH_MAP_FAST( m_mapRadialGradientBrushes, i )
	{
		SAFE_RELEASE( m_mapRadialGradientBrushes[i].m_pBrush );
		delete m_mapRadialGradientBrushes.Key(i);
	}
	m_listRadialGradientBrushLRU.RemoveAll();
	m_mapRadialGradientBrushes.RemoveAll();

	m_pTechnique = NULL;
	m_pViewportHeight = NULL;
	m_pViewportWidth = NULL;

	if ( m_pOpacityMaskTex )
	{
		m_pOpacityMaskTex->SetResource( NULL );
		m_pOpacityMaskTex = NULL;
	}

	if ( m_pOpacityMaskTexTwo )
	{
		m_pOpacityMaskTexTwo->SetResource( NULL );
		m_pOpacityMaskTexTwo = NULL;
	}

	if ( m_pDiffuseTex )
	{
		m_pDiffuseTex->SetResource( NULL );
		m_pDiffuseTex = NULL;
	}

	m_pmatTransform = NULL;
	m_pflSaturation = NULL;
	m_pflHueShift = NULL;
	m_pflContrast = NULL;
	m_pflBrightness = NULL;
	m_pTechniqueBlur = NULL;
	m_pIncrementalGaussian = NULL;
	m_pBlurDirectionVecPass1 = NULL;
	m_pBlurDirectionVecPass2 = NULL;
	m_pBlurDirectionVecPass3 = NULL;
	m_pBlurDirectionVecPass4 = NULL;
	m_pTechniqueYUV420 = NULL;
	m_pYTex = NULL;
	m_pUTex = NULL;
	m_pVTex = NULL;

	if ( m_pRenderEffect )
	{
		SAFE_RELEASE( m_pRenderEffect->m_pEffect );
		SAFE_RELEASE( m_pRenderEffect->m_pVertexBuffer );
		SAFE_RELEASE( m_pRenderEffect->m_pVertexLayout );
		delete m_pRenderEffect;
		m_pRenderEffect = NULL;
	}

	if ( m_stackCompositionLayers.Count() )
	{
		m_stackCompositionLayers[0] = NULL;
		FOR_EACH_VEC( m_stackCompositionLayers, i )
		{
			SAFE_DELETE( m_stackCompositionLayers[i] );
		}

		m_stackCompositionLayers.RemoveAll();
	}

	SAFE_LAYER_DELETE( m_pCompositionLayer );

	FOR_EACH_RBTREE_FAST( m_treeFreeCompositionLayers, i )
	{
		SAFE_LAYER_DELETE( m_treeFreeCompositionLayers[i] );
	}
	m_treeFreeCompositionLayers.RemoveAll();
	m_listCompositionLayersLRU.RemoveAll();

	{
		AUTO_LOCK( m_MutexReservedLayers );
		FOR_EACH_MAP_FAST( m_mapReservedCompositionLayers, i )
		{
			SAFE_LAYER_DELETE( m_mapReservedCompositionLayers[i].m_pLayer );
		}
		m_mapReservedCompositionLayers.RemoveAll();
		m_listReservedCompositionLayerLRU.RemoveAll();
	}

	FOR_EACH_MAP_FAST( m_mapShadowLayers, i )
	{
		SAFE_LAYER_DELETE( m_mapShadowLayers[i].m_pLayer );
	}
	m_mapShadowLayers.RemoveAll();
	m_listShadowLayerLRU.RemoveAll();

	SAFE_RELEASE( m_pTextRenderer );

	g_IUITextServices->FreeTextTextureCache( m_pTextTextureCache );
	g_IUITextServices->FreeTextLayoutDrawCache( m_pTextLayoutDrawCache );

	if ( m_pD3D10Device )
	{
		AccessDevice()->ClearState();
		AccessDevice()->Flush();
	}

	if ( IUIEngine::BIsRenderingToFullScreen( m_eRenderTarget ) )
	{
		g_nInFullscreenSwitch++;
		m_pDXGISwapChain->SetFullscreenState( FALSE, NULL );
		m_eRenderTarget = IUIEngine::k_ERenderToWindow;
		g_nInFullscreenSwitch--;
	}

#ifdef PANORAMA_STEAMUI_STREAMING_CAPTURE_WIN32
	SteamUIStreamingCaptureCallback_t pSteamUIStreamingCallback = m_pSteamUIStreamingCallback;
	if ( pSteamUIStreamingCallback )
	{
		pSteamUIStreamingCallback( m_pD3D10Device, NULL );
	}
#endif

	SAFE_RELEASE( m_pRenderTargetView );
	SAFE_RELEASE( m_pBackBuffer );
	SAFE_RELEASE( m_pD2DRenderTarget );
	SAFE_RELEASE( m_pRasterizerState );
	SAFE_RELEASE( m_pDXGISwapChain );
	SAFE_RELEASE( m_pD3D10Device );
	SAFE_RELEASE( m_pDWriteRenderingParams );
	SAFE_RELEASE( m_pDWriteFactory );
	SAFE_RELEASE( m_pD2DFactory );
	SAFE_RELEASE( m_pDXGIFactory );

	SAFE_DELETE( m_pBackBufferSharedMemStream );
	SAFE_DELETE( m_pBackBufferSharedMemEvent );
}


//-----------------------------------------------------------------------------
// Purpose: Initialization
//-----------------------------------------------------------------------------
bool CD3D10D2DSurface::BInitialize( HWND hWnd, int nSurfaceWidth, int nSurfaceHeight, int nWindowWidth, int nWindowHeight, IUIEngine::ERenderTarget eRenderType, bool bEnforceAspectRatio, bool bFixedSurfaceSize, CMouseCursorRender *pCursorRender )
{
	VPROF_BUDGET( "CD3D10D2DSurface::BInitialize", VPROF_BUDGETGROUP_TENFOOT );
	m_unSurfaceWidth = nSurfaceWidth;
	m_unSurfaceHeight = nSurfaceHeight;
	m_eRenderTarget = eRenderType;

	m_unWindowWidth = nWindowWidth;
	m_unWindowHeight = nWindowHeight;
	m_bFixedSurfaceSize = bFixedSurfaceSize;
	m_bSurfaceOccluded = false;

	if ( !g_IUITextServices )
		return false;

	m_pCursorRender = pCursorRender;

	m_hWnd = hWnd;

	m_bVsyncEnabled = s_convarPanoramaVsync.GetBool();

	CUITextLayoutWin32::BInitGlobals();

	if ( !BCreateDeviceIndependentResources() )
		return false;

	if ( !BCreateDeviceAndSwapChain() )
		return false;

	if ( !BCreateD3DDeviceResources() )
		return false;

	if ( !BRecreateSizedD3DResources() )
		return false;
	
	D3D10_TEXTURE2D_DESC desc;
	m_pBackBuffer->GetDesc( &desc );
	m_pCompositionLayer = (CCompositionLayer *)m_CompositionLayerPool.Alloc();
	ConstructSevenArg( m_pCompositionLayer, this, m_pBackBuffer, m_pD2DRenderTarget, m_pRenderTargetView, (ID3D10ShaderResourceView*)NULL, (float)desc.Width, (float)desc.Height );

	m_bEnforceAspectRatio = bEnforceAspectRatio;

	m_pTextTextureCache = g_IUITextServices->CreateTextTextureCache( this );
	m_pTextLayoutDrawCache = g_IUITextServices->CreateTextLayoutDrawCache( this );

	ComputeBackbufferScaling();

	// Setup top level of composition layers
	m_stackCompositionLayers.AddToTail( m_pCompositionLayer );

	LoadShaders();

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Compute back buffer scaling/translation factors
//-----------------------------------------------------------------------------
void CD3D10D2DSurface::ComputeBackbufferScaling()
{
	if ( !m_bEnforceAspectRatio )
	{
		m_flScaleBackbufferX = (float)GetWindowWidth() / (float)GetSurfaceWidth();
		m_flTranslateBackbufferX = 0.0f;
		m_flScaleBackbufferY = (float)GetWindowHeight() / (float)GetSurfaceHeight();
		m_flTranslateBackbufferY = 0.0f;
	}
	else
	{
		m_flScaleBackbufferX = (float)GetWindowWidth() / (float)GetSurfaceWidth();
		m_flScaleBackbufferY = (float)GetWindowHeight() / (float)GetSurfaceHeight();
		float flScaleToUse = MIN( m_flScaleBackbufferX, m_flScaleBackbufferY );
		m_flScaleBackbufferX = flScaleToUse;
		m_flScaleBackbufferY = flScaleToUse;

		m_flTranslateBackbufferX = ((float)GetWindowWidth() - (GetSurfaceWidth()*m_flScaleBackbufferX)) / 2.0f;
		m_flTranslateBackbufferY = ((float)GetWindowHeight() - (GetSurfaceHeight()*m_flScaleBackbufferY)) / 2.0f;

		// Want to be on pixel boundaries, or fonts will not be crisp!
		m_flTranslateBackbufferX = RoundFloatToInt( m_flTranslateBackbufferX );
		m_flTranslateBackbufferY = RoundFloatToInt( m_flTranslateBackbufferY );
	}
}


//-----------------------------------------------------------------------------
// Purpose: ReleaseDXResources
//			Drop outstanding references to raw DX resources like the back buffer
//-----------------------------------------------------------------------------
void CD3D10D2DSurface::ReleaseDXResources()
{
	Assert( m_stackCompositionLayers.Count() <= 1 &&  m_stackCompositionLayers.Count() > 0 );
	FOR_EACH_VEC( m_stackCompositionLayers, i )
	{
		SAFE_LAYER_DELETE( m_stackCompositionLayers[i] );
	}
	m_stackCompositionLayers.RemoveAll();
	m_pCompositionLayer = NULL;

#ifdef PANORAMA_STEAMUI_STREAMING_CAPTURE_WIN32
	SteamUIStreamingCaptureCallback_t pSteamUIStreamingCallback = m_pSteamUIStreamingCallback;
	if ( pSteamUIStreamingCallback )
	{
		pSteamUIStreamingCallback( m_pD3D10Device, NULL );
	}
#endif

	if ( m_eRenderTarget == IUIEngine::k_ERenderToOpenVROverlay && m_ulVROverlayHandle != vr::k_ulOverlayHandleInvalid)
	{
		vrapi::VROverlay()->ClearOverlayTexture( m_ulVROverlayHandle );
	}
}


//-----------------------------------------------------------------------------
// Purpose: BeginFrame
//-----------------------------------------------------------------------------
void CD3D10D2DSurface::BeginFrame( const CRenderMsg<CMsgBeginFrame> &renderMsg )
{
	VPROF_BUDGET( "CD3D10D2DSurface::BeginFrame", VPROF_BUDGETGROUP_TENFOOT );

	m_flCurrentRenderFrameTime = Plat_FloatTime();
	
	if ( m_renderThreadID == (ThreadId_t)0 )
	{
		m_renderThreadID = ThreadGetCurrentId();
	}

	if( renderMsg.BodyConst().clear_gpu_resources_before_frame() )
	{
		// Not actually implemented
		//ClearGPUResources();
	}

	BCheckForDeviceRemovedAndCrash();

	IUIEngine::ERenderTarget eMsgRenderTarget = (IUIEngine::ERenderTarget)renderMsg.BodyConst().render_target();
	uint32 nWidth = renderMsg.BodyConst().surface_width();
	uint32 nHeight = renderMsg.BodyConst().surface_height();

	m_bVsyncEnabled = s_convarPanoramaVsync.GetBool();

	// Did we reset devices?
	if ( BUpdateRenderStateIfNeeded( eMsgRenderTarget ) || BUpdateWindowSizeIfNeeded( nWidth, nHeight ) )
	{
		ComputeBackbufferScaling();
		
		Assert( !m_pCompositionLayer );

		D3D10_TEXTURE2D_DESC desc;
		m_pBackBuffer->GetDesc( &desc );

		m_pCompositionLayer = (CCompositionLayer *)m_CompositionLayerPool.Alloc();
		ConstructSevenArg( m_pCompositionLayer, this, m_pBackBuffer, m_pD2DRenderTarget, m_pRenderTargetView, (ID3D10ShaderResourceView*)NULL, (float)desc.Width, (float)desc.Height );

		// Setup top level of composition layers
		m_stackCompositionLayers.AddToTail( m_pCompositionLayer );
	}

	if ( m_stackCompositionLayers.Count() == 0 )
	{
		Assert( m_stackCompositionLayers.Count() != 0 );
	}
	else
	{
		CCompositionLayer *pLayer = m_stackCompositionLayers[ m_stackCompositionLayers.Count()-1 ];
		if ( m_stackCompositionLayers.Count() == 1 )
		{
			pLayer->PushCliplayersAndBeginDraw( m_flScaleBackbufferX, m_flScaleBackbufferY, m_flTranslateBackbufferX, m_flTranslateBackbufferY );
		}
		else
			pLayer->PushCliplayersAndBeginDraw( 1.0f, 1.0f, 0.0f, 0.0f );
	}

	m_flLastPaintFrameTime = renderMsg.BodyConst().frame_paint_time();
}



//-----------------------------------------------------------------------------
// Purpose: Clear back buffer
//-----------------------------------------------------------------------------
void CD3D10D2DSurface::ClearBackbuffer( const CRenderMsg<CMsgClearBackbuffer> &renderCommand )
{
	D2D1_COLOR_F colord2d;
	float color[4];
	const CMsgClearBackbuffer &msgBody = renderCommand.BodyConst();
	colord2d.r = color[0] = (msgBody.clear_color_rgba() & 0xFF)/255.0f;
	colord2d.g = color[1] = ((msgBody.clear_color_rgba() >> 8 ) & 0xFF)/255.0f;
	colord2d.b = color[2] = ((msgBody.clear_color_rgba() >> 16 ) & 0xFF)/255.0f;
	colord2d.a = color[3] = (msgBody.clear_color_rgba() >> 24) / 255.0f;

#if 0
	// lavender
	colord2d.r = color[0] = 0.70f;
	colord2d.g = color[1] = 0.1f;
	colord2d.b = color[2] = 0.75f;
	colord2d.a = color[3] = 0.9f;
#endif	

	if ( AccessD3DRenderTargetView() )
		AccessDevice()->ClearRenderTargetView( AccessD3DRenderTargetView(), color );
	if ( m_pCompositionLayer )
		m_pCompositionLayer->AccessRenderTarget()->Clear( &colord2d );
}


//-----------------------------------------------------------------------------
// Purpose: see if we should reload our shaders
//-----------------------------------------------------------------------------
void CD3D10D2DSurface::ReloadChangedFile( const char *pchFile )
{
	if  ( strstr( pchFile, "orthographic2d.fxo" ) != NULL )
	{
		// BUGBUG - Add reload of shaders, needs to be thread safe?
		/*
		 if ( m_pRenderEffect )
			m_pRenderEffect->Release();
		m_pRenderEffect = NULL;
		
		LoadShaders();*/
	}
}

void CD3D10D2DSurface::GetAndSetColorCorrectionShaderVarDefaults()
{
	if ( !m_pflSaturation )
	{
		m_pflSaturation = m_pRenderEffect->m_pEffect->GetVariableByName( "g_Saturation" )->AsScalar();
	}
	m_pflSaturation->SetFloat( 1.0f );

	if ( !m_pflHueShift )
	{
		m_pflHueShift = m_pRenderEffect->m_pEffect->GetVariableByName( "g_HueShift" )->AsScalar();
	}
	m_pflHueShift->SetFloat( 0.0f );

	if ( !m_pflContrast )
	{
		m_pflContrast = m_pRenderEffect->m_pEffect->GetVariableByName( "g_Contrast" )->AsScalar();
	}
	m_pflContrast->SetFloat( 1.0f );

	if ( !m_pflBrightness )
	{
		m_pflBrightness = m_pRenderEffect->m_pEffect->GetVariableByName( "g_Brightness" )->AsScalar();
	}
	m_pflBrightness->SetFloat( 1.0f );
}

//-----------------------------------------------------------------------------
// Purpose: Load shaders for the render layer
//-----------------------------------------------------------------------------
void CD3D10D2DSurface::LoadShaders()
{
	VPROF_BUDGET( "CD3D10D2DSurface::LoadShaders()", VPROF_BUDGETGROUP_TENFOOT );
	// Shouldn't already be loaded..
	Assert( !m_pRenderEffect );
	if ( m_pRenderEffect )
		return;

	m_pRenderEffect = new EffectData_t();

	CUtlString strUTF8 = UIEngine()->GetLocalPathForNamedPath( "{shaders}" );
	strUTF8 += "\\d3d10\\orthographic2d.fxo";
	CUtlBuffer bufFile;
	LoadFileIntoBuffer( strUTF8.String(), bufFile, false );

	if ( bufFile.TellPut() == 0 )
	{
		Msg( "Failed to read effect file %s", strUTF8.String() );
	}
	else
	{
		HRESULT hRes = g_D3D10CreateEffectFromMemory( bufFile.Base(), bufFile.TellPut(), 0, AccessDevice(), NULL, &m_pRenderEffect->m_pEffect );
		if ( SUCCEEDED( hRes ) )
		{
			Msg( "Created ID3DX11Effect for orthographic2d.fxo from memory ok!\n" );
		}
		else
		{
			Msg( "Creating ID3DX11Effect for orthographic2d.fxo  from memory failed!\n" );
		}
	}

	if ( !m_pRenderEffect->m_pEffect )
	{
		Msg( "Failed to find/create effect for orthographic2d.fxo\n" );
		return;
	}

	// Clear render state, so we'll reset for the new effect
	m_ERenderState = k_ERenderStateUnset;

	m_pTechnique = NULL;
	m_pViewportHeight = NULL;
	m_pViewportWidth = NULL;
	m_pDiffuseTex = NULL;
	m_pOpacityMaskTex = NULL;
	m_pOpacityMaskTexTwo = NULL;

	if ( !m_pRenderEffect->m_pVertexBuffer )
	{
		// Obtain the technique
		m_pTechnique = m_pRenderEffect->m_pEffect->GetTechniqueByName( "RenderComposition" );
		if ( !m_pTechnique )
		{
			Msg( "Couldn't get RenderComposition technique from effect\n" );
			return;
		}

		// Obtain the technique
		m_pTechniqueQuadNonPremultiplied = m_pRenderEffect->m_pEffect->GetTechniqueByName( "RenderQuad" );
		if ( !m_pTechniqueQuadNonPremultiplied )
		{
			Msg( "Couldn't get RenderQuad technique from effect\n" );
			return;
		}

		m_pTechniqueQuadAlphaOnly = m_pRenderEffect->m_pEffect->GetTechniqueByName( "RenderQuadAlphaOnly" );
		if ( !m_pTechniqueQuadAlphaOnly )
		{
			Msg( "Couldn't get RenderQuadAlphaOnly technique from effect\n" );
			return;
		}


		m_pTechniqueParticleSystem = m_pRenderEffect->m_pEffect->GetTechniqueByName( "RenderParticleSystem" );
		if ( !m_pTechniqueParticleSystem )
		{
			Msg( "Couldn't get RenderParticleSystem technique from effect\n" );
			return;
		}

		// Obtain the technique
		m_pTechniqueQuadPremultiplied = m_pRenderEffect->m_pEffect->GetTechniqueByName( "RenderQuadPreMultipled" );
		if ( !m_pTechniqueQuadPremultiplied )
		{
			Msg( "Couldn't get RenderQuad technique from effect\n" );
			return;
		}

		// Obtain the technique
		m_pTechniqueBlur = m_pRenderEffect->m_pEffect->GetTechniqueByName( "RenderBlur" );
		if ( !m_pTechniqueBlur )
		{
			Msg( "Couldn't get RenderBlur technique from effect\n" );
			return;
		}

		// Obtain the technique
		m_pTechniqueYUV420 = m_pRenderEffect->m_pEffect->GetTechniqueByName( "RenderYUV420" );
		if ( !m_pTechniqueYUV420 )
		{
			Msg( "Couldn't get RenderYUV420 technique from effect\n" );
			return;
		}

		static D3D10_INPUT_ELEMENT_DESC layout[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR",	0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0 }, 
			{ "TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0 }, 
			{ "TEXCOORD", 2, DXGI_FORMAT_R32G32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0 }, 
		};
		UINT numElements = sizeof(layout)/sizeof(layout[0]);

		// Create the input layout
		D3D10_PASS_DESC PassDesc;
		m_pTechnique->GetPassByIndex( 0 )->GetDesc( &PassDesc );
		HRESULT hRes = AccessDevice()->CreateInputLayout( layout, numElements, PassDesc.pIAInputSignature, PassDesc.IAInputSignatureSize, &m_pRenderEffect->m_pVertexLayout );
		if( FAILED( hRes ) )
		{
			Log ( "Failed creating input layout for Render/Blur technique\n" );
		}

		// Set the input layout
		AccessDevice()->IASetInputLayout( m_pRenderEffect->m_pVertexLayout );

		D3D10_BUFFER_DESC bd;
		bd.Usage = D3D10_USAGE_DYNAMIC;
		bd.ByteWidth = sizeof( VertexTextured_t )*TOTAL_VERTEX_BUFFER_SIZE;
		bd.BindFlags = D3D10_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
		bd.MiscFlags = 0;

		hRes = AccessDevice()->CreateBuffer( &bd, NULL, &m_pRenderEffect->m_pVertexBuffer );
		if( FAILED( hRes ) )
		{
			Msg( "Failed creating vertex buffer\n" );
		}
	}

	if ( !m_pDiffuseTex )
		m_pDiffuseTex = m_pRenderEffect->m_pEffect->GetVariableByName( "g_txDiffuse" )->AsShaderResource();

	if ( !m_pOpacityMaskTex )
		m_pOpacityMaskTex = m_pRenderEffect->m_pEffect->GetVariableByName( "g_txOpacityMask" )->AsShaderResource();

	if ( !m_pOpacityMaskTexTwo )
		m_pOpacityMaskTexTwo = m_pRenderEffect->m_pEffect->GetVariableByName( "g_txOpacityMaskTwo" )->AsShaderResource();

	if ( !m_pYTex )
		m_pYTex = m_pRenderEffect->m_pEffect->GetVariableByName( "g_txY" )->AsShaderResource();

	if ( !m_pUTex )
		m_pUTex = m_pRenderEffect->m_pEffect->GetVariableByName( "g_txU" )->AsShaderResource();

	if ( !m_pVTex )
		m_pVTex = m_pRenderEffect->m_pEffect->GetVariableByName( "g_txV" )->AsShaderResource();


	float flMatrixIdentity[16] = { 1.0f, 0.0f, 0.0f, 0.0f,
						   0.0f, 1.0f, 0.0f, 0.0f,
						   0.0f, 0.0f, 1.0f, 0.0f,
						   0.0f, 0.0f, 0.0f, 1.0f };
	if ( !m_pmatTransform )
	{
		m_pmatTransform = m_pRenderEffect->m_pEffect->GetVariableByName( "g_MatTransform" );
	}
	m_pmatTransform->AsMatrix()->SetMatrix( flMatrixIdentity );

	GetAndSetColorCorrectionShaderVarDefaults();

	if ( !m_pflOpacityMaskOneBase )
	{
		m_pflOpacityMaskOneBase = m_pRenderEffect->m_pEffect->GetVariableByName( "g_OpacityMaskOneBase" )->AsScalar();
	}
	m_pflOpacityMaskOneBase->SetFloat( 0.0f );

	if ( !m_pflOpacityMaskOneOpacity )
	{
		m_pflOpacityMaskOneOpacity = m_pRenderEffect->m_pEffect->GetVariableByName( "g_OpacityMaskOneOpacity" )->AsScalar();
	}
	m_pflOpacityMaskOneOpacity->SetFloat( 1.0f );

	if ( !m_pflOpacityMaskTwoBase )
	{
		m_pflOpacityMaskTwoBase = m_pRenderEffect->m_pEffect->GetVariableByName( "g_OpacityMaskTwoBase" )->AsScalar();
	}
	m_pflOpacityMaskTwoBase->SetFloat( 0.0f );

	UpdateViewPortSize();
}


//-----------------------------------------------------------------------------
// Purpose: Update viewport size for shaders
//-----------------------------------------------------------------------------
void CD3D10D2DSurface::UpdateBlurVariables( Vector2D vecBlurDirection, float flStdDeviation, float flStepSize )
{
	if ( !m_pIncrementalGaussian )
		m_pIncrementalGaussian = m_pRenderEffect->m_pEffect->GetVariableByName( "incrementalGaussian" )->AsVector();

	if ( !m_pBlurDirectionVecPass1 )
		m_pBlurDirectionVecPass1 = m_pRenderEffect->m_pEffect->GetVariableByName( "blurMultiplyVecPass1" )->AsVector();

	if ( !m_pBlurDirectionVecPass2 )
		m_pBlurDirectionVecPass2 = m_pRenderEffect->m_pEffect->GetVariableByName( "blurMultiplyVecPass2" )->AsVector();

	if ( !m_pBlurDirectionVecPass3 )
		m_pBlurDirectionVecPass3 = m_pRenderEffect->m_pEffect->GetVariableByName( "blurMultiplyVecPass3" )->AsVector();

	if ( !m_pBlurDirectionVecPass4 )
		m_pBlurDirectionVecPass4 = m_pRenderEffect->m_pEffect->GetVariableByName( "blurMultiplyVecPass4" )->AsVector();


	if ( m_pBlurDirectionVecPass1 )
	{
		float flValues[2] = { vecBlurDirection.x * flStepSize, vecBlurDirection.y * flStepSize };
		m_pBlurDirectionVecPass1->SetFloatVectorArray( flValues, 0, 2 );
	}

	if ( m_pBlurDirectionVecPass2 )
	{
		float flValues[2] = { vecBlurDirection.x * flStepSize * 2, vecBlurDirection.y * flStepSize * 2 };
		m_pBlurDirectionVecPass2->SetFloatVectorArray( flValues, 0, 2 );
	}

	if ( m_pBlurDirectionVecPass3 )
	{
		float flValues[2] = { vecBlurDirection.x * flStepSize * 3, vecBlurDirection.y * flStepSize * 3 };
		m_pBlurDirectionVecPass3->SetFloatVectorArray( flValues, 0, 2 );
	}

	if ( m_pBlurDirectionVecPass4 )
	{
		float flValues[2] = { vecBlurDirection.x * flStepSize * 4, vecBlurDirection.y * flStepSize * 4 };
		m_pBlurDirectionVecPass4->SetFloatVectorArray( flValues, 0, 2 );
	}


	if ( m_pIncrementalGaussian )
	{
		float flValues[3];

		// The magic 2.50... number here is sqrt( 2*pi )
		flValues[0] =  1.0f / (2.5066282746f * flStdDeviation);
		flValues[1] = exp( -0.5f / (flStdDeviation * flStdDeviation) );
		flValues[2] = flValues[1] * flValues[1];
		m_pIncrementalGaussian->SetFloatVectorArray( flValues, 0, 3 );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Update viewport size for shaders
//-----------------------------------------------------------------------------
void CD3D10D2DSurface::UpdateViewPortSize()
{
	if ( m_pRenderEffect )
	{
		if ( !m_pViewportHeight )
			m_pViewportHeight = m_pRenderEffect->m_pEffect->GetVariableByName( "g_viewportHeight" )->AsScalar();
		if ( !m_pViewportWidth )
			m_pViewportWidth = m_pRenderEffect->m_pEffect->GetVariableByName( "g_viewportWidth" )->AsScalar();

		if ( m_pViewportWidth && m_pViewportHeight )
		{
			Assert( m_stackCompositionLayers.Count() );
			if ( m_stackCompositionLayers.Count() )
			{
				CCompositionLayer *pLayer = m_stackCompositionLayers[ m_stackCompositionLayers.Count()-1 ];
				m_pViewportWidth->SetInt( pLayer->GetWidth() );
				m_pViewportHeight->SetInt( pLayer->GetHeight() );

				D3D10_VIEWPORT viewport;
				viewport.Width = pLayer->GetWidth();
				viewport.Height = pLayer->GetHeight();
				viewport.MinDepth = 0.0f;
				viewport.MaxDepth = 1.0f;
				viewport.TopLeftX = 0.0f;
				viewport.TopLeftY = 0.0f;
				AccessDevice()->RSSetViewports( 1, &viewport );

				D3D10_RECT rect;
				pLayer->GetCurrentClipRect( rect );

				if ( m_stackCompositionLayers.Count() == 1 ) 
				{
					// If we are drawing into the backbuffer there is scaling that is going to occur, we need to make that work.
					rect.left = (0.0f*m_flScaleBackbufferX) + m_flTranslateBackbufferX;
					rect.right = (m_unSurfaceWidth*m_flScaleBackbufferX) + m_flTranslateBackbufferX;
					rect.top = (0.0f*m_flScaleBackbufferY) + m_flTranslateBackbufferY;
					rect.bottom = (m_unSurfaceHeight*m_flScaleBackbufferY) + m_flTranslateBackbufferY;
				}
				AccessDevice()->RSSetScissorRects( 1, &rect );
			}
		}

		
	}
}


//-----------------------------------------------------------------------------
// Purpose: Create a texture object from off-thread
//-----------------------------------------------------------------------------
bool CD3D10D2DSurface::BCreateTexture( IUITexture **pTextureOutput, void *pubTextureData, uint32 unWidth, uint32 unHeight, uint32 unStride, E2DTextureFormat eFormat, EAlphaChannelType eAlphaChannelType )
{
	*pTextureOutput = NULL;

	CD3D10Texture *pTexture = new CD3D10Texture( AccessDevice(), ++s_unNextTextureID, pubTextureData, unWidth, unHeight, unStride, eFormat, eAlphaChannelType );
	if ( pTexture )
	{
		{
			AUTO_LOCK( m_lockTextureMap );
			m_mapTextures.Insert( pTexture->GetTextureID(), pTexture );
		}
		*pTextureOutput = pTexture;
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Create a double buffered texture object that can be directly updated off-thread
//-----------------------------------------------------------------------------
bool CD3D10D2DSurface::BCreateDoubleBufferedTexture( IUIDoubleBufferedTexture **pDoubleBufferedOutput, uint32 unWidth, uint32 unHeight, uint32 unStride, E2DTextureFormat eFormat, EAlphaChannelType eAlphaChannelType, bool bSerializedUploads )
{
	*pDoubleBufferedOutput = NULL;

	CD3D10DoubleBufferedTexture *pTexture = new CD3D10DoubleBufferedTexture( AccessDevice(), ++s_unNextTextureID, unWidth, unHeight, unStride, eFormat, eAlphaChannelType, bSerializedUploads );
	if ( pTexture )
	{
		{
			AUTO_LOCK( m_lockTextureMap );
			m_mapTextures.Insert( pTexture->GetTextureID(), pTexture );
		}
		*pDoubleBufferedOutput = pTexture;
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Create a double buffered YUV420 texture object that can be directly updated off-thread
//-----------------------------------------------------------------------------
bool CD3D10D2DSurface::BCreateDoubleBufferedYUV420Texture( IUIDoubleBufferedYUV420Texture **pDoubleBufferedYUV420Output, uint32 unWidth, uint32 unHeight )
{
	*pDoubleBufferedYUV420Output = NULL;

	CD3D10DoubleBufferedYUV420Texture *pTexture = new CD3D10DoubleBufferedYUV420Texture( this, AccessDevice(), ++s_unNextTextureID, unWidth, unHeight );
	if ( pTexture )
	{
		{
			AUTO_LOCK( m_lockTextureMap );
			m_mapTextures.Insert( pTexture->GetTextureID(), pTexture );
		}
		*pDoubleBufferedYUV420Output = pTexture;
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Delete a texture object 
//-----------------------------------------------------------------------------
bool CD3D10D2DSurface::BDeleteTexture( IUITexture *pTexture )
{
	// The texture should be in our map
	{
		AUTO_LOCK( m_lockTextureMap );
		short iMap = m_mapTextures.Find( pTexture->GetTextureID() );
		if ( iMap == m_mapTextures.InvalidIndex() )
			return false;

		Assert( m_mapTextures[iMap] == pTexture );
	}

	// Put the delete into a queue for deletion by texture id, we'll process it at the end of the next frame in the render thread
	// which ensures we don't delete it out from under drawing calls.
	m_tsQueueTextureDeletes.PushItem( pTexture->GetTextureID() );

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Lock texture, to increment it's draw serial probably even though it was culled
//-----------------------------------------------------------------------------
void CD3D10D2DSurface::LockTexture( const CRenderMsg<CMsgLockTexture> &renderCommand )
{
	const CMsgLockTexture &body = renderCommand.BodyConst();
	IUITexture *pTexture = NULL;
	{
		AUTO_LOCK( m_lockTextureMap );
		short iMap = m_mapTextures.Find( body.texture_id() );
		if ( iMap == m_mapTextures.InvalidIndex() )
			return;
		pTexture = m_mapTextures[iMap];
	}

	E2DTextureFormat eFormat = pTexture->GetFormat();

	// Special handling for YUV420
	if ( eFormat == k_EFormatYUV420 )
	{
		CD3D10DoubleBufferedYUV420Texture *pYUVTexture = (CD3D10DoubleBufferedYUV420Texture *)pTexture;

		ID3D10ShaderResourceView *resourceViews[3];
		int iLock = pYUVTexture->LockAndGetCurrentTextures( &resourceViews[0], &resourceViews[1], &resourceViews[2] );

		pYUVTexture->Unlock( iLock );
	}
	else
	{
		ID3D10UITexture *pD3DUITexture = (CD3D10Texture *)pTexture;

		ID3D10Texture2D *pD3DTexture;
		ID3D10ShaderResourceView *pShaderResourceView;
		int iLock = pD3DUITexture->LockAndGetCurrentTexture( &pD3DTexture, &pShaderResourceView, renderCommand.BodyConst().texture_serial() );

		pD3DUITexture->Unlock( iLock );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Draw a textured rect into current context surface
//-----------------------------------------------------------------------------
void CD3D10D2DSurface::DrawTexturedRect( const CRenderMsg<CMsgRenderTexturedRect> &renderCommand )
{
	if ( m_stackCompositionLayers.Count() == 0 )
	{
		Assert( m_stackCompositionLayers.Count() > 0 );
		return;
	}

	const CMsgRenderTexturedRect &body = renderCommand.BodyConst();
	IUITexture *pTexture = NULL;
	{
		AUTO_LOCK( m_lockTextureMap );
		short iMap = m_mapTextures.Find( body.texture_id() );
		if ( iMap == m_mapTextures.InvalidIndex() )
			return;
		pTexture = m_mapTextures[iMap];
	}

	if ( pTexture )
	{
		// Ok, we have the locked texture data, setup shader resource view variables, and draw into current composition layer.
		CCompositionLayer *pLayer = m_stackCompositionLayers[ m_stackCompositionLayers.Count()-1 ];
		pLayer->PopClipLayersAndFlush();

		// Clip if we need to, since we directly draw via D3D D2D doesn't take care of this for us...
		float flU0 = body.texture_top_left().x();
		float flV0 = body.texture_top_left().y();
		float flU1 = body.texture_bottom_right().x();
		float flV1 = body.texture_bottom_right().y();

		float flX0 = body.top_left().x();
		float flY0 = body.top_left().y();
		float flX1 = body.bottom_right().x();
		float flY1 = body.bottom_right().y();

		float flOriginalWidth = flX1 - flX0;
		float flOriginalHeight = flY1 - flY0;
		float flUWidth = flU1 - flU0;
		float flVWidth = flV1 - flV0;

		D3D10_RECT r;
		pLayer->GetCurrentClipRect( r );
		if ( r.left > flX0 )
		{
			flU0 = flU0 + ( ( r.left - flX0 ) / flOriginalWidth )*flUWidth;
			flX0 = r.left;
		}

		if ( flX1 > r.right )
		{
			flU1 = flU1 - ( ( flX1 - r.right ) / flOriginalWidth )*flUWidth;
			flX1 = r.right;
		}

		if ( r.top > flY0 )
		{
			flV0 = flV0 + ( ( r.top - flY0 ) / flOriginalHeight )*flVWidth;
			flY0 = r.top;
		}

		if ( flY1 > r.bottom )
		{
			flV1 = flV1 - ( ( flY1 - r.bottom ) / flOriginalHeight )*flVWidth;
			flY1 = r.bottom;
		}

		VertexTextured_t quad[4];
		quad[0].r = quad[0].g = quad[0].b = quad[0].a = 1.0f;
		quad[0].rhw = 1.0f;
		quad[0].masku1 = quad[0].masku2 = quad[0].u = flU0;
		quad[0].maskv1 = quad[0].maskv2 = quad[0].v = flV0;
		quad[0].x = flX0;
		quad[0].y = flY0;
		quad[0].z = 0.0f;

		quad[1].r = quad[1].g = quad[1].b = quad[1].a = 1.0f;
		quad[1].rhw = 1.0f;
		quad[1].masku1 = quad[1].masku2 = quad[1].u = flU1;
		quad[1].maskv1 = quad[1].maskv2 = quad[1].v = flV0;
		quad[1].x = flX1;
		quad[1].y = flY0;
		quad[1].z = 0.0f;

		quad[2].r = quad[2].g = quad[2].b = quad[2].a = 1.0f;
		quad[2].rhw = 1.0f;
		quad[2].masku1 = quad[2].masku2 = quad[2].u = flU0;
		quad[2].maskv1 = quad[2].maskv2 = quad[2].v = flV1;
		quad[2].x = flX0;
		quad[2].y = flY1;
		quad[2].z = 0.0f;

		quad[3].r = quad[3].g = quad[3].b = quad[3].a = 1.0f;
		quad[3].rhw = 1.0f;
		quad[3].masku1 = quad[3].masku2 = quad[3].u = flU1;
		quad[3].maskv1 = quad[3].maskv2 = quad[3].v = flV1;
		quad[3].x = flX1;
		quad[3].y = flY1;
		quad[3].z = 0.0f;

		ID3D10RenderTargetView *pRenderTargetView = pLayer->AccessRenderTargetView();
		AccessDevice()->OMSetRenderTargets( 1, &pRenderTargetView, NULL );

		UpdateViewPortSize();

		E2DTextureFormat eFormat = pTexture->GetFormat();

		// Special handling for YUV420
		if ( eFormat == k_EFormatYUV420 )
		{
			CD3D10DoubleBufferedYUV420Texture *pYUVTexture = (CD3D10DoubleBufferedYUV420Texture *)pTexture;

			ID3D10ShaderResourceView *resourceViews[3];
			int iLock = pYUVTexture->LockAndGetCurrentTextures( &resourceViews[0], &resourceViews[1], &resourceViews[2] );

			// Draw the movie with the appropriate technique, and flush immediately so we can flip back into d2d drawing mode
			DrawTexturedQuadInternal( m_pTechniqueYUV420, NULL, NULL, NULL, NULL, resourceViews[0], resourceViews[1], resourceViews[2], quad, 1.0f, 1.0f, 0.0f );
			FlushCurrentVertexBuffer( m_pTechniqueYUV420 );
			
			pYUVTexture->Unlock( iLock );
		}
		else
		{
			ID3D10UITexture *pD3DUITexture = (CD3D10Texture *)pTexture;

			ID3D10Texture2D *pD3DTexture;
			ID3D10ShaderResourceView *pShaderResourceView;
			int iLock = pD3DUITexture->LockAndGetCurrentTexture( &pD3DTexture, &pShaderResourceView, renderCommand.BodyConst().texture_serial() );

			ID3D10EffectTechnique *pTechnique = m_pTechniqueQuadNonPremultiplied;
			if ( pTexture->GetAlphaChannelType() == k_EAlphaChannelType_PreMultiplied )
				pTechnique = m_pTechniqueQuadPremultiplied;

			if ( (ETextureSampleMode)renderCommand.BodyConst().texture_sample_mode() == k_ETextureSampleModeAlphaOnly )
			{
				pTechnique = m_pTechniqueQuadAlphaOnly;
			}
			else
			{
				AssertMsg( (ETextureSampleMode)renderCommand.BodyConst().texture_sample_mode() == k_ETextureSampleModeNormal, "Unknown texture sampling type" );
			}

			// Draw the texture with the appropriate technique, and flush immediately so we can flip back into d2d drawing mode
			DrawTexturedQuadInternal( pTechnique, pD3DTexture, pShaderResourceView, NULL, NULL, NULL, NULL, NULL, quad, 1.0f, 1.0f, 0.0f );
			FlushCurrentVertexBuffer( pTechnique );

			pD3DUITexture->Unlock( iLock );
		}

		// Back into D2D drawing
		if ( m_stackCompositionLayers.Count() == 1 )
			pLayer->PushCliplayersAndBeginDraw( m_flScaleBackbufferX, m_flScaleBackbufferY, m_flTranslateBackbufferX, m_flTranslateBackbufferY );
		else
			pLayer->PushCliplayersAndBeginDraw( 1.0f, 1.0f, 0.0f, 0.0f );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Mark shader variables as dirty, called from code that knows it's changing all state (ie, using d2d)
//-----------------------------------------------------------------------------
void CD3D10D2DSurface::SetShaderVariablesDirty()
{
	m_bShaderVarsDirty = true;
}


//-----------------------------------------------------------------------------
// Purpose: Draw a textured quad
//-----------------------------------------------------------------------------
void CD3D10D2DSurface::DrawTexturedQuadInternal( ID3D10EffectTechnique *pTechnique, ID3D10Texture2D *pTexture, ID3D10ShaderResourceView *pShaderResourceView, ID3D10ShaderResourceView *pOpacityMaskResourceView, 
												ID3D10ShaderResourceView *pOpacityMaskTwoResourceView, ID3D10ShaderResourceView *pYTex, ID3D10ShaderResourceView *pUTex, ID3D10ShaderResourceView *pVTex, 
												const VertexTextured_t *points, float flScale2DX, float flScale2DY, float flRotate2D )
{
	VPROF_BUDGET( "CD3D10D2DSurface::DrawTexturedQuadInternal", VPROF_BUDGETGROUP_TENFOOT );
	if ( !AccessDevice() )
		return;

	if ( !m_pRenderEffect )
		return;

	// Can't render if we don't have a vertex buffer, should get recreated at effect creation time
	if ( !m_pRenderEffect->m_pVertexBuffer )
		return;

	bool bGotOpaqueMask = false;
	LockedOpacityMaskTextureShaderResourceView_t opacityMaskResourceViewOpaque; 
	opacityMaskResourceViewOpaque.m_iLockHandle = -1;
	opacityMaskResourceViewOpaque.m_pShaderResource = NULL;
	opacityMaskResourceViewOpaque.m_pTexture = NULL;

	if ( pOpacityMaskResourceView == NULL || pOpacityMaskTwoResourceView == NULL )
	{
		bGotOpaqueMask = true;
		opacityMaskResourceViewOpaque = GetOpacityMaskShaderResourceViewForTexture( 0 );
	}

	// See if we need to flush first
	if ( m_pRenderEffect->m_unVerticesInCurrentBatch*4 == VERTEX_BUFFER_FLUSH_SIZE || m_pRenderEffect->m_unVertexBufferPosition + m_pRenderEffect->m_unVerticesInCurrentBatch*4 >= TOTAL_VERTEX_BUFFER_SIZE || pTechnique != m_pLastDrawTechnique )
	{
		FlushCurrentVertexBuffer( m_pLastDrawTechnique );
		m_pLastDrawTechnique = pTechnique;
	}

	if ( m_bShaderVarsDirty || m_pRenderEffect->m_pCurrentTexture2D != pTexture || m_pRenderEffect->m_pOpacityMask != pOpacityMaskResourceView || m_pRenderEffect->m_pOpacityMaskTwo != pOpacityMaskTwoResourceView
		 || m_pRenderEffect->m_pCurrentTextureY != pYTex || m_pRenderEffect->m_pCurrentTextureU != pUTex || m_pRenderEffect->m_pCurrentTextureV != pVTex )
	{
		FlushCurrentVertexBuffer( m_pLastDrawTechnique );
		m_bShaderVarsDirty = false;
		m_pRenderEffect->m_pCurrentTexture2D = pTexture;
		m_pRenderEffect->m_pOpacityMask = pOpacityMaskResourceView;
		m_pRenderEffect->m_pOpacityMaskTwo = pOpacityMaskTwoResourceView;
		m_pRenderEffect->m_pCurrentTextureY = pYTex;
		m_pRenderEffect->m_pCurrentTextureU = pUTex;
		m_pRenderEffect->m_pCurrentTextureV = pVTex;

		// Set current shader resource view
		if ( m_pDiffuseTex )
			m_pDiffuseTex->SetResource( pShaderResourceView );

		if ( m_pOpacityMaskTex )
			m_pOpacityMaskTex->SetResource( pOpacityMaskResourceView ? pOpacityMaskResourceView : opacityMaskResourceViewOpaque.m_pShaderResource );

		if ( m_pOpacityMaskTexTwo )
			m_pOpacityMaskTexTwo->SetResource( pOpacityMaskTwoResourceView ? pOpacityMaskTwoResourceView : opacityMaskResourceViewOpaque.m_pShaderResource );

		if ( m_pYTex )
			m_pYTex->SetResource( pYTex );

		if ( m_pUTex )
			m_pUTex->SetResource( pUTex );

		if ( m_pVTex )
			m_pVTex->SetResource( pVTex );
	}

	bool bFailed = false;
	if ( !m_pRenderEffect->m_pCurrentVertexBatch )
	{
		if ( FAILED( m_pRenderEffect->m_pVertexBuffer->Map( m_pRenderEffect->m_unVertexBufferPosition ? D3D10_MAP_WRITE_NO_OVERWRITE : D3D10_MAP_WRITE_DISCARD, 0, (void**)&m_pRenderEffect->m_pCurrentVertexBatch ) ) )
		{
			Msg( "Failed mapping vertex buffer\n" );
			m_pRenderEffect->m_pCurrentVertexBatch = NULL;
			bFailed = true;
		}
	}

	if ( !bFailed )
	{
		VertexTextured_t *Quad = m_pRenderEffect->m_pCurrentVertexBatch + (m_pRenderEffect->m_unVertexBufferPosition + m_pRenderEffect->m_unVerticesInCurrentBatch * 4);		
		Quad[0] = points[0];
		Quad[1] = points[1];
		Quad[2] = points[2];
		Quad[3] = points[3];

		// If we are drawing into the backbuffer, we should scale to fit into it.		
		if ( m_stackCompositionLayers.Count() == 1 && BBackBufferScalingNeeded() )
		{
			for ( int i = 0; i < 4; i++ )
			{
				Quad[i].x = (Quad[i].x*m_flScaleBackbufferX) + m_flTranslateBackbufferX;
				Quad[i].y = (Quad[i].y*m_flScaleBackbufferY) + m_flTranslateBackbufferY;
			}
		}

		float flWidth = ( (Quad[1].x - Quad[0].x) + (Quad[3].x-Quad[2].x) )/2.0f;
		float flHeight = ( (Quad[3].y-Quad[0].y) + (Quad[2].y-Quad[1].y) )/2.0f;
		float flXOffset = (flWidth - (flScale2DX * flWidth) )/2.0f;
		float flYOffset = (flHeight - (flScale2DY * flHeight) )/2.0f;

		Quad[0].x += flXOffset;
		Quad[1].x -= flXOffset;
		Quad[2].x += flXOffset;
		Quad[3].x -= flXOffset;

		Quad[0].y += flYOffset;
		Quad[1].y += flYOffset;
		Quad[2].y -= flYOffset;
		Quad[3].y -= flYOffset;

		if ( flRotate2D > 0.00001f || flRotate2D < -0.00001f )
		{
			float flSine;
			float flCosine;
			float flRadians = DEG2RAD( flRotate2D );
			SinCos( flRadians, &flSine, &flCosine );

			float flXTranslate = Quad[0].x + ((Quad[1].x - Quad[0].x )/2.0f);
			float flYTranslate = Quad[0].y + ((Quad[3].y - Quad[0].y )/2.0f);

			for( int iCorner=0; iCorner < 4; ++iCorner )
			{
				float x = ( Quad[iCorner].x - flXTranslate );
				float y = ( Quad[iCorner].y - flYTranslate );

				Quad[iCorner].x = x * flCosine - y * flSine;
				Quad[iCorner].y = y * flCosine + x * flSine;

				Quad[iCorner].x += flXTranslate;
				Quad[iCorner].y += flYTranslate;
			}
		}

		m_pRenderEffect->m_unVerticesInCurrentBatch += 1;
	}

	if ( bGotOpaqueMask )
		ReleaseLockedOpacityMaskTextureShaderResourceView( opacityMaskResourceViewOpaque );
}


//-----------------------------------------------------------------------------
// Purpose: Helper for generating opacity masks for corner rounding
//-----------------------------------------------------------------------------
bool CD3D10D2DSurface::BDrawRoundedCornerMaskBitmapToRenderTarget( ID2D1RenderTarget *pOpacityRenderTarget, float flWidth, float flHeight, float flXInset, float flYInset, float flTopLeftHorizontal, float flTopLeftVertical, float flTopRightHorizontal, float flTopRightVertical, 
												float flBottomRightHorizontal, float flBottomRightVertical, float flBottomLeftHorizontal, float flBottomLeftVertical )
{
	VPROF_BUDGET( "CD3D10D2DSurface::BDrawRoundedCornerMaskBitmapToRenderTarget", VPROF_BUDGETGROUP_TENFOOT );
	pOpacityRenderTarget->BeginDraw();

	D2D1_COLOR_F color;
	color.r = 0.0f;
	color.g = 0.0f;
	color.b = 0.0f;
	color.a = 0.0f;

	pOpacityRenderTarget->Clear( color );

	// Fast case for all four corners having the same rounding
	if ( flTopLeftHorizontal == flTopRightHorizontal && flTopRightHorizontal == flBottomRightHorizontal && flBottomRightHorizontal == flBottomLeftHorizontal 
		&& flTopLeftVertical == flTopRightVertical && flTopRightVertical == flBottomRightVertical && flBottomRightVertical == flBottomLeftVertical )
	{

		D2D1_ROUNDED_RECT rect;
		rect.radiusX = flTopLeftHorizontal;
		rect.radiusY = flTopLeftVertical;
		rect.rect.left = flXInset;
		rect.rect.top = flYInset;
		rect.rect.right = flXInset+flWidth;
		rect.rect.bottom = flYInset+flHeight;

		ID2D1Brush *pBrush = NULL;
		pBrush = GetSolidColorBrush( 0xffffffff );

		pOpacityRenderTarget->FillRoundedRectangle( rect, pBrush );
		SAFE_RELEASE( pBrush );
	}
	else
	{
		// Have to draw each corner separately, with clipping, then fill primary rect region lame.
		ID2D1Brush *pBrush = NULL;
		pBrush = GetSolidColorBrush( 0xffffffff );

		// Note: the +/-flRoundingError here all deal with rounding error for scaling cases, without them -fulldesktopres may have
		// artifacts on outerbox shadows around irregularly rounded panels.

		const float flRoundingError = 0.01f;

		D2D1_RECT_F rectTopLeft;
		rectTopLeft.left = flXInset;
		rectTopLeft.top = flYInset;
		rectTopLeft.bottom = flYInset+flTopLeftVertical+flRoundingError;
		rectTopLeft.right = flXInset+flTopLeftHorizontal+flRoundingError;

		D2D1_ROUNDED_RECT rect;
		rect.radiusX = flTopLeftHorizontal;
		rect.radiusY = flTopLeftVertical;
		rect.rect.left = flXInset;
		rect.rect.top = flYInset;
		rect.rect.right = flXInset+flWidth;
		rect.rect.bottom = flYInset+flHeight;

		pOpacityRenderTarget->PushAxisAlignedClip( rectTopLeft, D2D1_ANTIALIAS_MODE_ALIASED );
		pOpacityRenderTarget->FillRoundedRectangle( rect, pBrush );
		pOpacityRenderTarget->PopAxisAlignedClip();

		D2D1_RECT_F rectTopRight;
		rectTopRight.left = flXInset + flWidth - flTopRightHorizontal - flRoundingError;
		rectTopRight.right = flXInset + flWidth;
		rectTopRight.top = flYInset;
		rectTopRight.bottom = flYInset + flTopRightVertical + flRoundingError;

		rect.radiusX = flTopRightHorizontal;
		rect.radiusY = flTopRightVertical;

		pOpacityRenderTarget->PushAxisAlignedClip( rectTopRight, D2D1_ANTIALIAS_MODE_ALIASED );
		pOpacityRenderTarget->FillRoundedRectangle( rect, pBrush );
		pOpacityRenderTarget->PopAxisAlignedClip();

		D2D1_RECT_F rectBottomRight;
		rectBottomRight.left = flXInset + flWidth - flBottomRightHorizontal - flRoundingError;
		rectBottomRight.right = flXInset + flWidth;
		rectBottomRight.top = flYInset + flHeight - flBottomRightVertical - flRoundingError;
		rectBottomRight.bottom = flYInset + flHeight;

		rect.radiusX = flBottomRightHorizontal;
		rect.radiusY = flBottomRightVertical;

		pOpacityRenderTarget->PushAxisAlignedClip( rectBottomRight, D2D1_ANTIALIAS_MODE_ALIASED );
		pOpacityRenderTarget->FillRoundedRectangle( rect, pBrush );
		pOpacityRenderTarget->PopAxisAlignedClip();

		D2D1_RECT_F rectBottomLeft;
		rectBottomLeft.left = flXInset;
		rectBottomLeft.right = flXInset + flBottomLeftHorizontal + flRoundingError;
		rectBottomLeft.top = flYInset + flHeight - flBottomLeftVertical - flRoundingError;
		rectBottomLeft.bottom = flYInset + flHeight;

		rect.radiusX = flBottomLeftHorizontal;
		rect.radiusY = flBottomLeftVertical;

		pOpacityRenderTarget->PushAxisAlignedClip( rectBottomLeft, D2D1_ANTIALIAS_MODE_ALIASED );
		pOpacityRenderTarget->FillRoundedRectangle( rect, pBrush );
		pOpacityRenderTarget->PopAxisAlignedClip();

		// Fill non corner regions

		// Create path geometry to build clip region into
		ID2D1PathGeometry *pPathGeometry = NULL;
		if ( SUCCEEDED( AccessD2D1Factory()->CreatePathGeometry( &pPathGeometry ) ) )
		{
			ID2D1GeometrySink *pSink;
			if ( SUCCEEDED( pPathGeometry->Open( &pSink ) ) )
			{
				pSink->BeginFigure( D2D1::Point2F( flXInset, flYInset+flTopLeftVertical-flRoundingError ), D2D1_FIGURE_BEGIN_FILLED );
				pSink->AddLine( D2D1::Point2F( flXInset+flTopLeftHorizontal-flRoundingError, flYInset+flTopLeftVertical-flRoundingError ) );
				pSink->AddLine( D2D1::Point2F( flXInset+flTopLeftHorizontal-flRoundingError, flYInset ) );
				pSink->AddLine( D2D1::Point2F( flXInset+flWidth-flTopRightHorizontal+flRoundingError, flYInset ) );
				pSink->AddLine( D2D1::Point2F( flXInset+flWidth-flTopRightHorizontal+flRoundingError, flYInset+flTopRightVertical-flRoundingError ) );
				pSink->AddLine( D2D1::Point2F( flXInset+flWidth, flYInset+flTopRightVertical-flRoundingError ) );
				pSink->AddLine( D2D1::Point2F( flXInset+flWidth, flYInset+flHeight-flBottomRightVertical+flRoundingError ) );
				pSink->AddLine( D2D1::Point2F( flXInset+flWidth-flBottomRightHorizontal-flRoundingError, flYInset+flHeight-flBottomRightVertical+flRoundingError ) );
				pSink->AddLine( D2D1::Point2F( flXInset+flWidth-flBottomRightHorizontal-flRoundingError, flYInset+flHeight ) );
				pSink->AddLine( D2D1::Point2F( flXInset+flBottomLeftHorizontal-flRoundingError, flYInset+flHeight ) );
				pSink->AddLine( D2D1::Point2F( flXInset+flBottomLeftHorizontal-flRoundingError, flYInset+flHeight-flBottomLeftVertical+flRoundingError ) );
				pSink->AddLine( D2D1::Point2F( flXInset, flYInset+flHeight-flBottomLeftVertical+flRoundingError ) );
				pSink->AddLine( D2D1::Point2F( flXInset, flYInset+flTopLeftVertical-flRoundingError ) );
				pSink->EndFigure( D2D1_FIGURE_END_CLOSED );
				DbgVerify( SUCCEEDED( pSink->Close() ) );
				SAFE_RELEASE( pSink );
			}

			D2D1_SIZE_F size;
			size.height = flHeight;
			size.width = flWidth;

			ID2D1Layer *pLayer;
			if ( SUCCEEDED( pOpacityRenderTarget->CreateLayer( size, &pLayer ) ) )
			{
				D2D1_LAYER_PARAMETERS params = D2D1::LayerParameters();
				params.geometricMask = pPathGeometry;
				params.maskAntialiasMode = D2D1_ANTIALIAS_MODE_ALIASED;
				pOpacityRenderTarget->PushLayer( &params, pLayer );
				pOpacityRenderTarget->FillRectangle( D2D1::RectF( flXInset, flYInset, flXInset+flWidth, flYInset+flHeight ), pBrush );
				pOpacityRenderTarget->PopLayer();
				SAFE_RELEASE( pLayer );
			}

			SAFE_RELEASE( pPathGeometry );
		}

		SAFE_RELEASE( pBrush );
	}

	pOpacityRenderTarget->EndDraw();

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Get/create a radial gradient brush
//-----------------------------------------------------------------------------
ID2D1RadialGradientBrush *CD3D10D2DSurface::GetRadialGradientBrush( const CMsgRadialGradient &msg )
{
	VPROF_BUDGET( "CD3D10D2DSurface::GetRadialGradientBrush", VPROF_BUDGETGROUP_TENFOOT );
	if ( msg.color_stop_size() < 1 )
		return NULL;

	int iMap = m_mapRadialGradientBrushes.Find( &msg );
	if ( iMap != m_mapRadialGradientBrushes.InvalidIndex() )
	{
		RadialGradientBrush_t &data = m_mapRadialGradientBrushes[iMap];
		m_listRadialGradientBrushLRU.LinkToTail( data.m_iLRUIndex );
		data.m_iLRUIndex = m_listRadialGradientBrushLRU.Tail();
		m_listRadialGradientBrushLRU[data.m_iLRUIndex].m_flLastUseTime = m_flCurrentRenderFrameTime;

		data.m_pBrush->AddRef();
		return data.m_pBrush;
	}
	else
	{
		ID2D1GradientStopCollection *pGradientStopCollection = NULL;
		D2D1_GRADIENT_STOP *pGradientStops = new D2D1_GRADIENT_STOP[ msg.color_stop_size() ];

		for( int i=0; i < msg.color_stop_size(); ++i )
		{
			uint32 rgba = msg.color_stop( i ).color_rgba();
			float r,g,b,a;
			r = (rgba&0xff) / 255.0f;
			g = ((rgba>>8)&0xff) / 255.0f;
			b = ((rgba>>16)&0xff) / 255.0f;
			a = ((rgba>>24)&0xff) / 255.0f;

			pGradientStops[i].color = D2D1::ColorF( r, g, b, a );
			pGradientStops[i].position = msg.color_stop( i ).position();
		}

		ID2D1RadialGradientBrush *pBrush = NULL;
		HRESULT hRes = AccessD2DRenderTarget()->CreateGradientStopCollection( pGradientStops, msg.color_stop_size(),
			D2D1_GAMMA_2_2,	D2D1_EXTEND_MODE_CLAMP,	&pGradientStopCollection );
		if ( SUCCEEDED( hRes ) )
		{
			hRes = AccessD2DRenderTarget()->CreateRadialGradientBrush( D2D1::RadialGradientBrushProperties(
				D2D1::Point2F( msg.center_position().x(), msg.center_position().y() ),
				D2D1::Point2F( msg.offset_distance().x(), msg.offset_distance().y() ),
				msg.radii().x(), msg.radii().y() ),
				pGradientStopCollection,
				&pBrush
				);

			if ( SUCCEEDED( hRes ) )
			{
				RadialGradientBrush_t brush;
				brush.m_pBrush = pBrush;
				pBrush->AddRef();

				RadialGradientBrushLRU_t lru;
				lru.m_flLastUseTime = m_flCurrentRenderFrameTime;

				CMsgRadialGradient *pKey = new CMsgRadialGradient( msg );

				lru.m_iMap = iMap = m_mapRadialGradientBrushes.Insert( pKey, brush );
				m_mapRadialGradientBrushes[iMap].m_iLRUIndex = m_listRadialGradientBrushLRU.AddToTail( lru );
			}

			SAFE_RELEASE( pGradientStopCollection );
		}

		delete[] pGradientStops;
		return pBrush;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get/create a linear gradient brush
//-----------------------------------------------------------------------------
ID2D1LinearGradientBrush *CD3D10D2DSurface::GetLinearGradientBrush( const CMsgLinearGradient &msg )
{
	VPROF_BUDGET( "CD3D10D2DSurface::GetLinearGradientBrush", VPROF_BUDGETGROUP_TENFOOT );
	if ( msg.color_stop_size() < 1 )
		return NULL;

	CRenderMsg<CMsgLinearGradient>::Node node;
	node.Next = NULL;
	node.elem = (CMsgLinearGradient *)&msg;

	int iMap = m_mapLinearGradientBrushes.Find( &node );
	if ( iMap != m_mapLinearGradientBrushes.InvalidIndex() )
	{
		LinearGradientBrush_t &data = m_mapLinearGradientBrushes[iMap];
		m_listLinearGradientBrushLRU.LinkToTail( data.m_iLRUIndex );
		data.m_iLRUIndex = m_listLinearGradientBrushLRU.Tail();
		m_listLinearGradientBrushLRU[data.m_iLRUIndex].m_flLastUseTime = m_flCurrentRenderFrameTime;
	
		data.m_pBrush->AddRef();
		return data.m_pBrush;
	}
	else
	{
		ID2D1GradientStopCollection *pGradientStopCollection = NULL;

		D2D1_GRADIENT_STOP *pGradientStops = new D2D1_GRADIENT_STOP[ msg.color_stop_size() ];
		
		for( int i=0; i < msg.color_stop_size(); ++i )
		{
			uint32 rgba = msg.color_stop( i ).color_rgba();
			float r,g,b,a;
			r = (rgba&0xff) / 255.0f;
			g = ((rgba>>8)&0xff) / 255.0f;
			b = ((rgba>>16)&0xff) / 255.0f;
			a = ((rgba>>24)&0xff) / 255.0f;

			pGradientStops[i].color = D2D1::ColorF( r, g, b, a );
			pGradientStops[i].position = msg.color_stop( i ).position();
		}

		ID2D1LinearGradientBrush *pBrush = NULL;
		HRESULT hRes = AccessD2DRenderTarget()->CreateGradientStopCollection( pGradientStops, msg.color_stop_size(),
			D2D1_GAMMA_2_2,	D2D1_EXTEND_MODE_CLAMP,	&pGradientStopCollection );
		if ( SUCCEEDED( hRes ) )
		{
			hRes = AccessD2DRenderTarget()->CreateLinearGradientBrush( D2D1::LinearGradientBrushProperties(
				D2D1::Point2F( msg.start_position().x(), msg.start_position().y() ),
				D2D1::Point2F( msg.end_position().x(), msg.end_position().y() ) ),
				pGradientStopCollection,
				&pBrush
				);

			if ( SUCCEEDED( hRes ) )
			{
				LinearGradientBrush_t brush;
				brush.m_pBrush = pBrush;
				pBrush->AddRef();

				LinearGradientBrushLRU_t lru;
				lru.m_flLastUseTime = m_flCurrentRenderFrameTime;


				CRenderMsg<CMsgLinearGradient>::Node *pNode = CRenderMsg<CMsgLinearGradient>::AllocProtoBufMsgObject();
				*(pNode->elem) = msg;

				lru.m_iMap = iMap = m_mapLinearGradientBrushes.Insert( pNode, brush );
				m_mapLinearGradientBrushes[iMap].m_iLRUIndex = m_listLinearGradientBrushLRU.AddToTail( lru );
			}
			else
			{
				BCheckForDeviceRemovedAndCrash();
			}

			SAFE_RELEASE( pGradientStopCollection );
		}

		delete[] pGradientStops;
		return pBrush;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Get/create a solid color brush
//-----------------------------------------------------------------------------
ID2D1SolidColorBrush *CD3D10D2DSurface::GetSolidColorBrush( uint32 unColor )
{
	VPROF_BUDGET( "CD3D10D2DSurface::GetSolidColorBrush", VPROF_BUDGETGROUP_TENFOOT );
	// Early out for transparent colors
	if ( ((unColor>>24)&0xff) == 0 )
		return NULL;

	int iMap = m_mapSolidColorBrushes.Find( unColor );
	if ( iMap == m_mapSolidColorBrushes.InvalidIndex() )
	{
		ID2D1SolidColorBrush *pBrush = NULL;

		float r,g,b,a;
		r = (unColor&0xff) / 255.0f;
		g = ((unColor>>8)&0xff) / 255.0f;
		b = ((unColor>>16)&0xff) / 255.0f;
		a = ((unColor>>24)&0xff) / 255.0f;

		HRESULT hRes = AccessD2DRenderTarget()->CreateSolidColorBrush( D2D1::ColorF( r, g, b, a ), &pBrush );
		if ( SUCCEEDED( hRes ) )
		{
			SolidBrush_t brush;
			brush.m_pBrush = pBrush;

			SolidBrushLRU_t lru;
			lru.m_flLastUseTime = m_flCurrentRenderFrameTime;

			lru.m_iMap = iMap = m_mapSolidColorBrushes.Insert( unColor, brush );
			m_mapSolidColorBrushes[iMap].m_iLRUIndex = m_listSolidColorBrushLRU.AddToTail( lru );
		}
	}
	else
	{
		SolidBrush_t &brush = m_mapSolidColorBrushes[iMap];

		m_listSolidColorBrushLRU.LinkToTail( brush.m_iLRUIndex );
		brush.m_iLRUIndex = m_listSolidColorBrushLRU.Tail();
		m_listSolidColorBrushLRU[brush.m_iLRUIndex].m_flLastUseTime = m_flCurrentRenderFrameTime;
	}

	// No brush
	if ( iMap == m_mapSolidColorBrushes.InvalidIndex() )
		return NULL;

	m_mapSolidColorBrushes[iMap].m_pBrush->AddRef();
	return m_mapSolidColorBrushes[iMap].m_pBrush;
}


//-----------------------------------------------------------------------------
// Purpose: Get an appropriate d2d fill brush for a given fill brush message
//-----------------------------------------------------------------------------
ID2D1Brush *CD3D10D2DSurface::GetD2DBrushForFillBrush( const CMsgFillBrush &brushMsg )
{
	VPROF_BUDGET( "CD3D10D2DSurface::GetD2DBrushForFillBrush", VPROF_BUDGETGROUP_TENFOOT );
	if ( brushMsg.has_linear_gradient() )
	{
		return GetLinearGradientBrush( brushMsg.linear_gradient() );
	}
	if ( brushMsg.has_radial_gradient() )
	{
		return GetRadialGradientBrush( brushMsg.radial_gradient() );
	}
	else
	{
		if ( brushMsg.has_color_rgba() )
		{
			return GetSolidColorBrush( brushMsg.color_rgba() );
		}
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Helper to push an opacity layer for a brush if needed
//-----------------------------------------------------------------------------
bool CD3D10D2DSurface::PushOpacityLayerIfNeeded( ID2D1RenderTarget *pRenderTarget, const D2D1_SIZE_F &rect, const CMsgFillBrush &msg )
{
	VPROF_BUDGET( "CD3D10D2DSurface::PushOpacityLayerIfNeeded", VPROF_BUDGETGROUP_TENFOOT );
	bool bLayer = false;
	if ( msg.opacity() < 1.0f )
	{
		ID2D1Layer *pLayer;
		if ( SUCCEEDED( pRenderTarget->CreateLayer( &rect, &pLayer ) ) )
		{
			D2D1_LAYER_PARAMETERS params = D2D1::LayerParameters();
			params.opacity = msg.opacity();
			pRenderTarget->PushLayer( &params, pLayer );
			bLayer = true;
			SAFE_RELEASE( pLayer );
		}
	}

	return bLayer;
}


//-----------------------------------------------------------------------------
// Purpose: Draw a filled quad
//-----------------------------------------------------------------------------
void CD3D10D2DSurface::DrawFilledRect( const CRenderMsg<CMsgRenderFilledRect> &renderCommand )
{
	VPROF_BUDGET( "CD3D10D2DSurface::DrawFilledRect", VPROF_BUDGETGROUP_TENFOOT );
	if ( m_stackCompositionLayers.Count() == 0 )
	{
		Assert( m_stackCompositionLayers.Count() > 0 );
		return;
	}

	const CMsgRenderFilledRect &msgBody = renderCommand.BodyConst();

	CCompositionLayer *pLayer = m_stackCompositionLayers[ m_stackCompositionLayers.Count()-1 ];
	if ( pLayer->BIsDrawing() )
	{
		D2D1_RECT_F rect;
		rect.left = msgBody.top_left().x();
		rect.top = msgBody.top_left().y();
		rect.right = msgBody.bottom_right().x();
		rect.bottom = msgBody.bottom_right().y();

		D2D1_SIZE_F size = D2D1::SizeF( rect.right - rect.left, rect.bottom - rect.top );

		const CMsgRenderFillBrushCollection &fill_brush_collection = msgBody.fill_brush_collection();
		int cBrushes = fill_brush_collection.fill_brush_size();
		for ( int i=0; i<cBrushes; ++i )
		{
			const CMsgFillBrush &brush = fill_brush_collection.fill_brush( i );

			if ( brush.has_particle_system() )
			{
				VPROF_BUDGET( "DrawFilledRect - ParticleSystem", VPROF_BUDGETGROUP_TENFOOT );

				// Stop D2D drawing
				CCompositionLayer *pLayer = m_stackCompositionLayers[ m_stackCompositionLayers.Count()-1 ];
				pLayer->PopClipLayersAndFlush();

				ID3D10RenderTargetView *pRenderTargetView = pLayer->AccessRenderTargetView();
				AccessDevice()->OMSetRenderTargets( 1, &pRenderTargetView, NULL );

				UpdateViewPortSize();

				ID3D10EffectTechnique *pTechnique = m_pTechniqueParticleSystem;

				// bugbug jmccaskey - should do clipping of partial particles, D2D would normally, but we avoid it for the particle system
				D3D10_RECT clipRect;
				pLayer->GetCurrentClipRect( clipRect );

				VertexTextured_t quad[4];
				
				const CMsgParticleSystem &system = brush.particle_system();
				int nParticles = system.particles_size();
				{
					VPROF_BUDGET( "DrawFilledRect - ParticleSystem Loop", VPROF_BUDGETGROUP_TENFOOT );
					for ( int iParticle = 0; iParticle < nParticles; ++iParticle )
					{
						const CMsgParticle &particle = system.particles( iParticle );

						uint32 rgba = particle.color_rgba();
						float r,g,b,a;

						r = (rgba&0xff) / 255.0f;
						g = ((rgba>>8)&0xff) / 255.0f;
						b = ((rgba>>16)&0xff) / 255.0f;
						a = ((rgba>>24)&0xff) / 255.0f;


						float flSharpness = particle.particle_sharpness();
						float flHalfSize = particle.particle_size() / 2.0f;

						// Clip particles partially outside clipRect, or fully outside...

						quad[0].r = r;
						quad[0].g = g;
						quad[0].b = b;
						quad[0].a = a * brush.opacity();
						quad[0].rhw = 1.0f;
						quad[0].u = 0.0f;
						quad[0].v = 0.0f;
						quad[0].masku1 = quad[0].masku2 = flSharpness;
						quad[0].maskv1 = quad[0].maskv2 = flSharpness;
						quad[0].x = particle.particle_position().x() - flHalfSize;
						quad[0].y = particle.particle_position().y() - flHalfSize;
						quad[0].z = particle.particle_position().z();

						quad[1].r = r;
						quad[1].g = g;
						quad[1].b = b;
						quad[1].a = a * brush.opacity();;
						quad[1].rhw = 1.0f;
						quad[1].u = 1.0f;
						quad[1].v = 0.0f;
						quad[1].masku1 = quad[1].masku2 = flSharpness;
						quad[1].maskv1 = quad[1].maskv2 = flSharpness;
						quad[1].x = particle.particle_position().x() + flHalfSize;
						quad[1].y = particle.particle_position().y() - flHalfSize;
						quad[1].z = particle.particle_position().z();

						quad[2].r = r;
						quad[2].g = g;
						quad[2].b = b;
						quad[2].a = a * brush.opacity();;
						quad[2].rhw = 1.0f;
						quad[2].u = 0.0f;
						quad[2].v = 1.0f;
						quad[2].masku1 = quad[2].masku2 = flSharpness;
						quad[2].maskv1 = quad[2].maskv2 = flSharpness;
						quad[2].x = particle.particle_position().x() - flHalfSize;
						quad[2].y = particle.particle_position().y() + flHalfSize;
						quad[2].z = particle.particle_position().z();

						quad[3].r = r;
						quad[3].g = g;
						quad[3].b = b;
						quad[3].a = a * brush.opacity();
						quad[3].rhw = 1.0f;
						quad[3].u = 1.0f;
						quad[3].v = 1.0f;
						quad[3].masku1 = quad[3].masku2 = flSharpness;
						quad[3].maskv1 = quad[3].maskv2 = flSharpness;
						quad[3].x = particle.particle_position().x() + flHalfSize;
						quad[3].y = particle.particle_position().y() + flHalfSize;
						quad[3].z = particle.particle_position().z();

						// Draw the texture with the appropriate technique, and flush immediately so we can flip back into d2d drawing mode
						DrawTexturedQuadInternal( pTechnique, NULL, NULL, NULL, NULL, NULL, NULL, NULL, quad, 1.0f, 1.0f, 0.0f );
					}
				}
				
				FlushCurrentVertexBuffer( pTechnique );
				
				// Back into D2D drawing
				if ( m_stackCompositionLayers.Count() == 1 )
					pLayer->PushCliplayersAndBeginDraw( m_flScaleBackbufferX, m_flScaleBackbufferY, m_flTranslateBackbufferX, m_flTranslateBackbufferY );
				else
					pLayer->PushCliplayersAndBeginDraw( 1.0f, 1.0f, 0.0f, 0.0f );
			}
			else if ( 1 /*|| brush.has_linear_gradient() || brush.has_radial_gradient() */ )
			{
				ID2D1RenderTarget *pRenderTarget = pLayer->AccessRenderTarget();
				EAntialiasing antialising = (EAntialiasing)msgBody.antialiasing();

				if ( antialising == k_EAntialiasingNone )
					pRenderTarget->SetAntialiasMode( D2D1_ANTIALIAS_MODE_ALIASED );

				bool bLayer = PushOpacityLayerIfNeeded( pRenderTarget, size, fill_brush_collection.fill_brush( i ) );

				ID2D1Brush *pBrush = GetD2DBrushForFillBrush( fill_brush_collection.fill_brush(i) );
				if ( pBrush )
				{
					pBrush->SetTransform( D2D1::Matrix3x2F::Translation( msgBody.top_left().x(), msgBody.top_left().y() ) );
					pRenderTarget->FillRectangle( &rect, pBrush );
					pBrush->SetTransform( D2D1::Matrix3x2F::Identity() );
					SAFE_RELEASE( pBrush );
				}

				if ( bLayer )
					pRenderTarget->PopLayer();


				if ( antialising == k_EAntialiasingNone )
					pRenderTarget->SetAntialiasMode( D2D1_ANTIALIAS_MODE_PER_PRIMITIVE );
			}
			//else
			{
				// Solid RGBA brush, we can just use D3D and draw a quad, which is faster
				//uint32 rgba = brush.color_rgba();
				
			}
		}
	}

}



//-----------------------------------------------------------------------------
// Purpose: Generate or return cached text opacity mask texture
//-----------------------------------------------------------------------------
UITextOpacityMaskData_t *CD3D10D2DSurface::GetCachedTextOpacityMask( const void *pRawText, int cbRawText, int cTextChars, EPanoramaTextEncoding eTextEncoding, float x0, float y0, float x1, float y1, float flLineHeight, ETextAlign align, bool bWrap, bool bEllipsis,
	const CMsgRenderTextFormat &defaultFormat, const ::google::protobuf::RepeatedPtrField< CMsgRenderTextRangeFormat > &rangeFormats )
{
	VPROF_BUDGET( "CD3D10D2DSurface::GetCachedTextOpacityMask", VPROF_BUDGETGROUP_TENFOOT );

	// Need text to do any work
	if ( !pRawText || cTextChars <= 0 )
		return NULL;

	UITextLayoutProperties_t *pKey = m_pTextLayoutDrawCache->AllocTextLayoutProperties( pRawText, cbRawText, cTextChars, eTextEncoding, x0, y0, x1, y1, flLineHeight, align, bWrap, bEllipsis, UIEngine()->GetDisplayLanguage() );

	RenderMsgToTextLayoutKey( defaultFormat, rangeFormats, pKey );

	CUtlVectorFixedGrowable<const char *, 4> vecRangeFontNames;
	vecRangeFontNames.SetCount( rangeFormats.size() );

	for ( int i = 0; i < rangeFormats.size(); i++ )
	{
		if ( rangeFormats.Get( i ).format().has_font_name() )
		{
			vecRangeFontNames[i] = rangeFormats.Get( i ).format().font_name().c_str();
		}
		else
		{
			vecRangeFontNames[i] = NULL;
		}
	}

	UITextOpacityMaskData_t *pData = m_pTextLayoutDrawCache->GetTextOpacityMask( pRawText, cbRawText, cTextChars, eTextEncoding, x0, y0, defaultFormat.font_name().c_str(), pKey, vecRangeFontNames.Base(), m_flCurrentRenderFrameTime, m_pTextRenderer );

	m_pTextLayoutDrawCache->FreeTextLayoutProperties( pKey );

	return pData;
}


//-----------------------------------------------------------------------------
// Purpose: Returns the maximum font glyph texture width
//-----------------------------------------------------------------------------
uint32 CD3D10D2DSurface::GetMaximumTextureWidth()
{
	return 1920;
}


//-----------------------------------------------------------------------------
// Purpose: Returns the maximum font glyph texture height
//-----------------------------------------------------------------------------
uint32 CD3D10D2DSurface::GetMaximumTextureHeight()
{
	return 1920;
}


//-----------------------------------------------------------------------------
// Purpose: Creates or finds a cached text alpha texture
//-----------------------------------------------------------------------------
UITextTextureRegion_t CD3D10D2DSurface::GetTextureRegion( int32 iWidth, int32 iHeight )
{
	return m_pTextTextureCache->GetTextureRegion( iWidth, iHeight );
}


//-----------------------------------------------------------------------------
// Purpose: Called at the start of updating a font texture
//-----------------------------------------------------------------------------
void CD3D10D2DSurface::StartUpdateFontGlyphTexture( UITextTextureHandle_t hTexture )
{
}


//-----------------------------------------------------------------------------
// Purpose: Called to update font texture
//-----------------------------------------------------------------------------
void CD3D10D2DSurface::UpdateFontGlyphTexture( UITextTextureHandle_t hTexture, int xOffset, int yOffset, int width, int height, void *pSourceData )
{
	AssertMsg( false, "Never called in d3dsurface, dwrite render handles texture updating directly as it draws natively to d3d textures" );
}


//-----------------------------------------------------------------------------
// Purpose: Called at the end of updating a font texture
//-----------------------------------------------------------------------------
void CD3D10D2DSurface::EndUpdateFontGlyphTexture( UITextTextureHandle_t hTexture )
{
}


//-----------------------------------------------------------------------------
// Purpose: Allocate a new alpha-only texture for text rendering
//-----------------------------------------------------------------------------
UITextTextureHandle_t CD3D10D2DSurface::AllocAlphaTexture( int32 iWidth, int32 iHeight )
{
	ID2D1BitmapRenderTarget *pAlphaOnlyTarget = NULL;

	D2D1_PIXEL_FORMAT alphaOnlyFormat = D2D1::PixelFormat(
		DXGI_FORMAT_A8_UNORM,
		D2D1_ALPHA_MODE_PREMULTIPLIED );

	D2D1_SIZE_U bitmapSize = D2D1::SizeU( iWidth, iHeight );
	ID2D1RenderTarget *pRenderTarget = m_stackCompositionLayers[m_stackCompositionLayers.Count() - 1]->AccessRenderTargetNoDrawing();
	if ( SUCCEEDED( pRenderTarget->CreateCompatibleRenderTarget( NULL, &bitmapSize, &alphaOnlyFormat, D2D1_COMPATIBLE_RENDER_TARGET_OPTIONS_NONE, &pAlphaOnlyTarget ) ) )
	{
		pAlphaOnlyTarget->BeginDraw();
		pAlphaOnlyTarget->Clear( D2D1::ColorF( D2D1::ColorF::Black, 0.0f ) );
		pAlphaOnlyTarget->EndDraw();
	}

	return pAlphaOnlyTarget;
}


//-----------------------------------------------------------------------------
// Purpose: Free a texture
//-----------------------------------------------------------------------------
void CD3D10D2DSurface::FreeTexture( UITextTextureHandle_t hTexture )
{
	ID2D1BitmapRenderTarget *pAlphaOnlyTarget = (ID2D1BitmapRenderTarget*)hTexture;
	SAFE_RELEASE( pAlphaOnlyTarget );
}


//-----------------------------------------------------------------------------
// Purpose: Internal helper to draw a range of text with a specified brush
//-----------------------------------------------------------------------------
void CD3D10D2DSurface::DrawTextRegionRange( ID2D1RenderTarget *pRenderTarget, float x0, float y0, float x1, float y1, UITextOpacityMaskDataRange_t &maskRange, const CMsgRenderFillBrushCollection &fill_brush_collection )
{	
	int cBrushes = fill_brush_collection.fill_brush_size();
	D2D1_SIZE_F size = D2D1::SizeF( x1-x0, y1-y0 );

	for ( int i = 0; i < cBrushes; ++i )
	{		
		bool bLayer = PushOpacityLayerIfNeeded( pRenderTarget, size, fill_brush_collection.fill_brush( i ) );

		ID2D1Brush *pBrush = GetD2DBrushForFillBrush( fill_brush_collection.fill_brush(i) );
		if ( pBrush )
		{
			pRenderTarget->SetAntialiasMode( D2D1_ANTIALIAS_MODE_ALIASED );

			D2D1_RECT_F destinationRect = D2D1::RectF(	x0 + maskRange.m_flStringOffsetX,
														y0 + maskRange.m_flStringOffsetY,
														x0 + maskRange.m_flStringOffsetX + (maskRange.m_x1 - maskRange.m_x0),
														y0 + maskRange.m_flStringOffsetY + (maskRange.m_y1 - maskRange.m_y0) );

			pBrush->SetTransform( D2D1::Matrix3x2F::Translation( x0, destinationRect.top ) );

			ID2D1Bitmap *pBitmap = NULL;
			ID2D1BitmapRenderTarget *pTarget = (ID2D1BitmapRenderTarget *)maskRange.m_hTexture;
			pTarget->GetBitmap( &pBitmap );
			if ( pBitmap )
			{
				D2D1_RECT_F rect;
				rect.left = maskRange.m_x0;
				rect.right = maskRange.m_x1;
				rect.top = maskRange.m_y0;
				rect.bottom = maskRange.m_y1;
				pRenderTarget->FillOpacityMask(	pBitmap, pBrush, D2D1_OPACITY_MASK_CONTENT_TEXT_NATURAL, &destinationRect, &rect );
			}

			SAFE_RELEASE( pBitmap );

			pRenderTarget->SetAntialiasMode( D2D1_ANTIALIAS_MODE_PER_PRIMITIVE );
			pBrush->SetTransform( D2D1::Matrix3x2F::Identity() );

			SAFE_RELEASE( pBrush );
		}

		if ( bLayer )
			pRenderTarget->PopLayer();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handle drawing text region
//-----------------------------------------------------------------------------
void CD3D10D2DSurface::DrawTextRegion( const CRenderMsg<CMsgRenderTextRegion> &renderCommand ) 
{
	VPROF_BUDGET( "CD3D10D2DSurface::DrawTextRegion", VPROF_BUDGETGROUP_TENFOOT );
	if ( m_stackCompositionLayers.Count() == 0 )
	{
		Assert( m_stackCompositionLayers.Count() > 0 );
		return;
	}

	CCompositionLayer *pLayer = m_stackCompositionLayers[ m_stackCompositionLayers.Count()-1 ];
	if ( pLayer->BIsDrawing() )
	{
		const CMsgRenderTextRegion &msgBody = renderCommand.BodyConst();

		float x0, y0, x1, y1;
		x0 = msgBody.top_left().x();
		y0 = msgBody.top_left().y();
		x1 = msgBody.bottom_right().x();
		y1 = msgBody.bottom_right().y();

		// Check rect has valid area
		if ( x1 <= x0 || y1 <= y0 )
			return;

		float flLineHeight = k_flFloatNotSet;
		if ( msgBody.has_line_height() )
			flLineHeight = msgBody.line_height();

		// get textures
		UITextOpacityMaskData_t *pResult = GetCachedTextOpacityMask( msgBody.raw_text().data(), msgBody.raw_text().size(), msgBody.text_chars(), (EPanoramaTextEncoding)msgBody.text_encoding(), x0, y0, x1, y1, flLineHeight, (ETextAlign)msgBody.text_align(), msgBody.wrapping(), msgBody.ellipsis(),
			msgBody.default_format(), msgBody.range_formats() );

		if ( pResult )
		{
			for ( int iTextMaskRegion = 0; iTextMaskRegion < pResult->m_cRangeData; iTextMaskRegion++ )
			{
				const CMsgRenderFillBrushCollection *pFillBrush = &msgBody.default_format().fill_brush_collection();
				int iColorRangeFormat = pResult->m_pRangeData[iTextMaskRegion].m_iColorIndex;
				if ( iColorRangeFormat != UITextOpacityMaskDataRange_t::k_iColorIndexUnset )
				{
					Assert( msgBody.range_formats().size() > iColorRangeFormat );
					pFillBrush = &msgBody.range_formats( iColorRangeFormat ).format().fill_brush_collection();
				}
				
				// check for an override for this range
				ID2D1RenderTarget *pRenderTarget = pLayer->AccessRenderTarget();
				DrawTextRegionRange( pRenderTarget, x0, y0, x1, y1, pResult->m_pRangeData[iTextMaskRegion], *pFillBrush );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Flush geometry for current effects vertex buffer
//-----------------------------------------------------------------------------
void CD3D10D2DSurface::FlushCurrentVertexBuffer( ID3D10EffectTechnique *pTechnique )
{
	VPROF_BUDGET( "CD3D10D2DSurface::FlushCurrentVertexBuffer", VPROF_BUDGETGROUP_TENFOOT );
	if ( !m_pRenderEffect )
		return;

	if ( m_pRenderEffect->m_pCurrentVertexBatch )
	{
		if ( !AccessDevice() )
			return;

		if ( m_pRenderEffect->m_pVertexBuffer )
		{
			m_pRenderEffect->m_pVertexBuffer->Unmap();
			m_pRenderEffect->m_pCurrentVertexBatch = NULL;
		}
		else
		{
			return;
		}

		if ( m_pRenderEffect->m_unVerticesInCurrentBatch == 0 )
			return;

		if ( m_ERenderState != k_ERenderStateDrawTexturedQuad )
		{
			// Set the input layout
			AccessDevice()->IASetInputLayout( m_pRenderEffect->m_pVertexLayout );

			// Set vertex buffer
			UINT stride = sizeof( VertexTextured_t );
			UINT offset = 0;
			AccessDevice()->IASetVertexBuffers( 0, 1, &m_pRenderEffect->m_pVertexBuffer, &stride, &offset );

			// Set primitive topology
			AccessDevice()->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );

			m_ERenderState = k_ERenderStateDrawTexturedQuad;
		}

		{
			D3D10_TECHNIQUE_DESC techDesc;
			pTechnique->GetDesc( &techDesc );
			for( UINT p = 0; p < techDesc.Passes; ++p )
			{
				pTechnique->GetPassByIndex( p )->Apply( 0 );
				for ( DWORD i=0; i < m_pRenderEffect->m_unVerticesInCurrentBatch*4; i += 4 )
				{
					AccessDevice()->Draw( 4, m_pRenderEffect->m_unVertexBufferPosition+i );
				}
			}
		}

		m_pRenderEffect->m_unVertexBufferPosition += m_pRenderEffect->m_unVerticesInCurrentBatch*4;
		m_pRenderEffect->m_unVerticesInCurrentBatch = 0;
		if ( m_pRenderEffect->m_unVertexBufferPosition >= TOTAL_VERTEX_BUFFER_SIZE || TOTAL_VERTEX_BUFFER_SIZE - m_pRenderEffect->m_unVertexBufferPosition < VERTEX_BUFFER_FLUSH_SIZE / 2 )
		{
			m_pRenderEffect->m_unVertexBufferPosition = 0;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: draw the mouse cursor on the screen
//-----------------------------------------------------------------------------
void CD3D10D2DSurface::DrawMouseCursor( CCompositionLayer *pLayer, uint32 nMouseTextureID, const Vector2D &ptHotspot )
{
	// draw the mouse cursor on the screen as our last act before present
	VPROF_BUDGET( "CD3D10D2DSurface::DrawMouseCursor", VPROF_BUDGETGROUP_TENFOOT );
	if( m_pCursorRender )
	{
		// pump the frame loop for the cursor
		m_pCursorRender->RunRenderFrame( GetHWND(), m_flCurrentRenderFrameTime, GetSurfaceWidth(), GetSurfaceHeight(), m_bEnforceAspectRatio );

		if ( m_pCursorRender->BCursorVisible() )
		{
			float flOpacity = m_pCursorRender->GetCursorOpacity();
			Vector2D pt = m_pCursorRender->GetRenderCursorPosition();

			IUITexture *pTexture = NULL;
			{
				AUTO_LOCK( m_lockTextureMap );
				short iMap = m_mapTextures.Find( nMouseTextureID );
				if ( iMap == m_mapTextures.InvalidIndex() )
					return;
				pTexture = m_mapTextures[iMap];
			}

			if ( pTexture )
			{
				E2DTextureFormat eFormat = pTexture->GetFormat();

				// Doesn't handle YUV420 yet
				Assert ( eFormat != k_EFormatYUV420 );
				ID3D10UITexture *pD3DUITexture = (CD3D10Texture*)pTexture;

				bool bPremultiplied = false;
				if ( pTexture->GetAlphaChannelType() == k_EAlphaChannelType_PreMultiplied )
					bPremultiplied = true;

				ID3D10Texture2D *pD3DTexture;
				ID3D10ShaderResourceView *pShaderResourceView;
				int iLock = pD3DUITexture->LockAndGetCurrentTexture( &pD3DTexture, &pShaderResourceView, 0 );

				D3D10_TEXTURE2D_DESC desc;
				pD3DTexture->GetDesc( &desc );

				float flScaledCursorWidth = desc.Width * GetWindowScaleFactor();
				float flScaledCursorHeight = desc.Height * GetWindowScaleFactor();

				// Offset for cursor hotspot
				pt.x -= ptHotspot.x * flScaledCursorWidth;
				pt.y -= ptHotspot.y * flScaledCursorHeight;

				VertexTextured_t quad[4];
				quad[0].r = quad[0].g = quad[0].b = quad[0].a = 1.0;
				quad[0].rhw = 1.0f;
				quad[0].masku1 = quad[0].masku2 = quad[0].u = 0.0f;
				quad[0].maskv1 = quad[0].maskv2 = quad[0].v = 0.0f;
				quad[0].x = pt.x;
				quad[0].y = pt.y;
				quad[0].z = 0.0f;

				quad[1].r = quad[1].g = quad[1].b = quad[1].a = 1.0;
				quad[1].rhw = 1.0f;
				quad[1].masku1 = quad[1].masku2 = quad[1].u = 1.0f;
				quad[1].maskv1 = quad[1].maskv2 = quad[1].v = 0.0f;
				quad[1].x = pt.x + flScaledCursorWidth;
				quad[1].y = pt.y;
				quad[1].z = 0.0f;

				quad[2].r = quad[2].g = quad[2].b = quad[2].a = 1.0;
				quad[2].rhw = 1.0f;
				quad[2].masku1 = quad[2].masku2 = quad[2].u = 0.0f;
				quad[2].maskv1 = quad[2].maskv2 = quad[2].v = 1.0f;
				quad[2].x = pt.x;
				quad[2].y = pt.y + flScaledCursorHeight;
				quad[2].z = 0.0f;

			
				quad[3].rhw = 1.0f;
				quad[3].r = quad[3].g = quad[3].b = quad[3].a = 1.0;
				quad[3].masku1 = quad[3].masku2 = quad[3].u = 1.0f;
				quad[3].maskv1 = quad[3].maskv2 = quad[3].v = 1.0f;
				quad[3].x = pt.x + flScaledCursorWidth;
				quad[3].y = pt.y + flScaledCursorHeight;
				quad[3].z = 0.0f;

				if ( bPremultiplied && flOpacity < 1.0f )
				{
					quad[0].r = quad[0].g = quad[0].b = quad[0].a = flOpacity;
					quad[1].r = quad[1].g = quad[1].b = quad[1].a = flOpacity;
					quad[2].r = quad[2].g = quad[2].b = quad[2].a = flOpacity;
					quad[3].r = quad[3].g = quad[3].b = quad[3].a = flOpacity;
				}
				else if ( flOpacity < 1.0f )
				{
					quad[0].a = flOpacity;
					quad[1].a = flOpacity;
					quad[2].a = flOpacity;
					quad[3].a = flOpacity;
				}

				ID3D10RenderTargetView *pRenderTargetView = pLayer->AccessRenderTargetView();
				AccessDevice()->OMSetRenderTargets( 1, &pRenderTargetView, NULL );

				UpdateViewPortSize();
		
				ID3D10EffectTechnique *pTechnique = m_pTechniqueQuadNonPremultiplied;
				if ( pTexture->GetAlphaChannelType() == k_EAlphaChannelType_PreMultiplied )
					pTechnique = m_pTechniqueQuadPremultiplied;

				// Draw the texture with the appropriate technique, and flush immediately so we can flip back into d2d drawing mode
				DrawTexturedQuadInternal( pTechnique, pD3DTexture, pShaderResourceView, NULL, NULL, NULL, NULL, NULL, quad, 1.0f, 1.0f, 0.0f );
				FlushCurrentVertexBuffer( pTechnique );

				pD3DUITexture->Unlock( iLock );
			}
		}
	}
}

void CD3D10D2DSurface::DrawSteamPadPointer( CCompositionLayer *pLayer, SteamPadPointer_t *pPointer, int padX, int padY )
{
	VPROF_BUDGET( "CD3D10D2DSurface::DrawSteamPadPointer", VPROF_BUDGETGROUP_TENFOOT );

	if ( pPointer->bVisible == false )
		return;

	IUITexture *pTexture = NULL;
	{
		AUTO_LOCK( m_lockTextureMap );
		short iMap = m_mapTextures.Find( pPointer->nTextureID );
		if ( iMap == m_mapTextures.InvalidIndex() )
			return;
		pTexture = m_mapTextures[iMap];
	}

	if ( !pTexture )
	{
		AssertMsgOnce( false, "Invalid textureid to CD3D10D2DSurface::DrawSteamPadPointer" );
		return;
	}

	float flOpacity = pPointer->flOpacity;

	struct Identity { static float I( float f, bool ) { return f; } };
	auto funcPreRenderCalculatePaddOffset = pPointer->funcPreRenderCalculatePadOffset ? pPointer->funcPreRenderCalculatePadOffset : &Identity::I;

	float controllerX = (*funcPreRenderCalculatePaddOffset)( padX / 32768.0, false ) * pPointer->flRadius + pPointer->vecCenter.x;
	float controllerY = (*funcPreRenderCalculatePaddOffset)( -padY / 32768.0, true ) * pPointer->flRadius + pPointer->vecCenter.y;

	E2DTextureFormat eFormat = pTexture->GetFormat();

	// Doesn't handle YUV420 yet
	Assert( eFormat != k_EFormatYUV420 );
	ID3D10UITexture *pD3DUITexture = (CD3D10Texture*)pTexture;

	bool bPremultiplied = false;
	if ( pTexture->GetAlphaChannelType() == k_EAlphaChannelType_PreMultiplied )
		bPremultiplied = true;

	ID3D10Texture2D *pD3DTexture;
	ID3D10ShaderResourceView *pShaderResourceView;
	int iLock = pD3DUITexture->LockAndGetCurrentTexture( &pD3DTexture, &pShaderResourceView, 0 );

	D3D10_TEXTURE2D_DESC desc;
	pD3DTexture->GetDesc( &desc );

	float flScaledCursorWidth = desc.Width * GetWindowScaleFactor();
	float flScaledCursorHeight = desc.Height * GetWindowScaleFactor();

	Vector2D pt;
	pt.x = controllerX - flScaledCursorWidth / 2.0;
	pt.y = controllerY - flScaledCursorHeight / 2.0;

	VertexTextured_t quad[4];
	quad[0].r = quad[0].g = quad[0].b = quad[0].a = 1.0;
	quad[0].rhw = 1.0f;
	quad[0].masku1 = quad[0].masku2 = quad[0].u = 0.0f;
	quad[0].maskv1 = quad[0].maskv2 = quad[0].v = 0.0f;
	quad[0].x = pt.x;
	quad[0].y = pt.y;
	quad[0].z = 0.0f;

	quad[1].r = quad[1].g = quad[1].b = quad[1].a = 1.0;
	quad[1].rhw = 1.0f;
	quad[1].masku1 = quad[1].masku2 = quad[1].u = 1.0f;
	quad[1].maskv1 = quad[1].maskv2 = quad[1].v = 0.0f;
	quad[1].x = pt.x + flScaledCursorWidth;
	quad[1].y = pt.y;
	quad[1].z = 0.0f;

	quad[2].r = quad[2].g = quad[2].b = quad[2].a = 1.0;
	quad[2].rhw = 1.0f;
	quad[2].masku1 = quad[2].masku2 = quad[2].u = 0.0f;
	quad[2].maskv1 = quad[2].maskv2 = quad[2].v = 1.0f;
	quad[2].x = pt.x;
	quad[2].y = pt.y + flScaledCursorHeight;
	quad[2].z = 0.0f;


	quad[3].rhw = 1.0f;
	quad[3].r = quad[3].g = quad[3].b = quad[3].a = 1.0;
	quad[3].masku1 = quad[3].masku2 = quad[3].u = 1.0f;
	quad[3].maskv1 = quad[3].maskv2 = quad[3].v = 1.0f;
	quad[3].x = pt.x + flScaledCursorWidth;
	quad[3].y = pt.y + flScaledCursorHeight;
	quad[3].z = 0.0f;

	if ( bPremultiplied && flOpacity < 1.0f )
	{
		quad[0].r = quad[0].g = quad[0].b = quad[0].a = flOpacity;
		quad[1].r = quad[1].g = quad[1].b = quad[1].a = flOpacity;
		quad[2].r = quad[2].g = quad[2].b = quad[2].a = flOpacity;
		quad[3].r = quad[3].g = quad[3].b = quad[3].a = flOpacity;
	}
	else if ( flOpacity < 1.0f )
	{
		quad[0].a = flOpacity;
		quad[1].a = flOpacity;
		quad[2].a = flOpacity;
		quad[3].a = flOpacity;
	}

	ID3D10RenderTargetView *pRenderTargetView = pLayer->AccessRenderTargetView();
	AccessDevice()->OMSetRenderTargets( 1, &pRenderTargetView, NULL );

	UpdateViewPortSize();

	ID3D10EffectTechnique *pTechnique = m_pTechniqueQuadNonPremultiplied;
	if ( pTexture->GetAlphaChannelType() == k_EAlphaChannelType_PreMultiplied )
		pTechnique = m_pTechniqueQuadPremultiplied;

	// Draw the texture with the appropriate technique, and flush immediately so we can flip back into d2d drawing mode
	DrawTexturedQuadInternal( pTechnique, pD3DTexture, pShaderResourceView, NULL, NULL, NULL, NULL, NULL, quad, 1.0f, 1.0f, 0.0f );
	FlushCurrentVertexBuffer( pTechnique );

	pD3DUITexture->Unlock( iLock );
}

//-----------------------------------------------------------------------------
// Purpose: EndFrame
//-----------------------------------------------------------------------------
void CD3D10D2DSurface::EndFrame( const CRenderMsg<CMsgEndFrame> &renderCommand )
{
	VPROF_BUDGET( "CD3D10D2DSurface::EndFrame", VPROF_BUDGETGROUP_TENFOOT );

	Assert( m_stackCompositionLayers.Count() == 1 );
	CCompositionLayer *pLayer = m_stackCompositionLayers[ m_stackCompositionLayers.Count() - 1];
	AssertMsg( pLayer->BIsDrawing(), "Should be drawing when we get here, we'll stop now." );
	pLayer->PopClipLayersAndFlush();	

	int32 nMouseTextureID = 0;
	if ( renderCommand.BodyConst().has_mouse_cursor_texture_id() )
		nMouseTextureID = renderCommand.BodyConst().mouse_cursor_texture_id();
	if ( nMouseTextureID != 0 )
	{
		Vector2D ptHotspot;
		ptHotspot.x = renderCommand.BodyConst().mouse_cursor_hotspot_x();
		ptHotspot.y = renderCommand.BodyConst().mouse_cursor_hotspot_y();
		DrawMouseCursor( pLayer, nMouseTextureID, ptHotspot );
	}

	// In the same fashion as the mouse cursor, draw any steam controller cursors
#if !defined( PANORAMA_PUBLIC_STEAM_SDK )
	{
		SteamControllerStateInternal_t controllerState;

		ClientControllerLocal()->GetControllerState( m_leftSteamPadPointer.iControllerID, &controllerState );
		
		if ( controllerState.ulButtons & STEAM_LEFTPAD_FINGERDOWN_MASK )
			DrawSteamPadPointer( pLayer, &m_leftSteamPadPointer, controllerState.sLeftPadX, controllerState.sLeftPadY );
		
		ClientControllerLocal()->GetControllerState( m_rightSteamPadPointer.iControllerID, &controllerState );
		
		if ( controllerState.ulButtons & STEAM_RIGHTPAD_FINGERDOWN_MASK )
			DrawSteamPadPointer( pLayer, &m_rightSteamPadPointer, controllerState.sRightPadX, controllerState.sRightPadY );
	}
#endif // !defined( PANORAMA_PUBLIC_STEAM_SDK )
#ifdef PANORAMA_PUBLIC_STEAM_SDK
/*	Disabled the touchpad display on the Panorama side for VR since the overlay renders it now
	{
		vr::VRControllerState_t controllerState;

		// Get mappings for which controller is left and right
		// For now we assume there are exactly two controllers. If there are more we will be getting the rightmost two which might not be right
		vr::TrackedDeviceIndex_t sortedDeviceIndexes[2];
		uint32_t unIndexCount = vrapi::VRSystem()->GetSortedTrackedDeviceIndicesOfClass( vr::TrackedDeviceClass::TrackedDeviceClass_Controller, sortedDeviceIndexes, 2, vr::k_unTrackedDeviceIndex_Hmd );

		if ( unIndexCount > 1 )
		{
			vrapi::VRSystem()->GetControllerState( sortedDeviceIndexes[0], &controllerState );

			if ( controllerState.ulButtonTouched & ButtonMaskFromId( vr::k_EButton_SteamVR_Touchpad ) )
			{
				DrawSteamPadPointer( pLayer, &m_rightSteamPadPointer, controllerState.rAxis[0].x * 32768.0, controllerState.rAxis[0].y * 32768.0 );
			}
			vrapi::VRSystem()->GetControllerState( sortedDeviceIndexes[1], &controllerState );
			if ( controllerState.ulButtonTouched & ButtonMaskFromId( vr::k_EButton_SteamVR_Touchpad ) )
			{
				DrawSteamPadPointer( pLayer, &m_leftSteamPadPointer, controllerState.rAxis[0].x * 32768.0, controllerState.rAxis[0].y * 32768.0 );
			}
		}
	}*/
#endif // PANORAMA_PUBLIC_STEAM_SDK

	PresentBackBuffer();

	// Debug spew for various GPU memory usage, useful when debugging...
	if ( s_convarPanoramaGPUMemorySpew.GetBool() && UIEngine()->GetCurrentFrameTime() > m_flLastStatsDump + 1.0f )
	{
		Msg( "!! GPU Memory for surface %ux%u (%s)\n", m_unSurfaceWidth, m_unSurfaceHeight, IUIEngine::BIsRenderingToTexture( m_eRenderTarget ) ? "overlay" : "mainwindow" );
		uint32 unTotalBytes = 0;
		uint32 unBytes = 0;
		FOR_EACH_MAP( m_mapTextures, i )
		{
			IUITexture *pTexture = m_mapTextures[i];
			unBytes += pTexture->GetStride() * pTexture->GetTextureHeight();
		}
		unTotalBytes += unBytes;
		Msg( "   Image/movie texture memory: %s\n", V_pretifymem( (float)unBytes ) );

		unBytes = 0;
		FOR_EACH_MAP( m_mapShadowLayers, i )
		{
			CCompositionLayer *pLayer = m_mapShadowLayers[i].m_pLayer;

			// Doesn't account for padding in stride... close enough
			D3D10_TEXTURE2D_DESC desc;
			pLayer->AccessSurface()->GetDesc( &desc );
			unBytes += 4 * desc.Height * desc.Width;
		}
		unTotalBytes += unBytes;
		Msg( "   Shadow Layer memory: %s\n", V_pretifymem( (float)unBytes ) );

		unBytes = 0;
		uint32 unCountReserved = 0;
		FOR_EACH_MAP( m_mapReservedCompositionLayers, i )
		{
			CCompositionLayer *pLayer = m_mapReservedCompositionLayers[i].m_pLayer;

			// Doesn't account for padding in stride... close enough
			D3D10_TEXTURE2D_DESC desc;
			pLayer->AccessSurface()->GetDesc( &desc );
			unBytes += 4 * desc.Height * desc.Width;		
			unCountReserved++;
		}
		unTotalBytes += unBytes;
		Msg( "   Reserved Composition Layer memory: %s, %u layers\n", V_pretifymem( (float)unBytes ), unCountReserved );

		unBytes = 0;
		uint32 unCountUnreserved = 0;
		FOR_EACH_MAP( m_treeFreeCompositionLayers, i )
		{
			CCompositionLayer *pLayer = m_treeFreeCompositionLayers[i];

			// Doesn't account for padding in stride... close enough
			D3D10_TEXTURE2D_DESC desc;
			pLayer->AccessSurface()->GetDesc( &desc );
			unBytes += 4 * desc.Height * desc.Width;
			unCountUnreserved++;
		}
		unTotalBytes += unBytes;
		Msg( "   Unreserved Composition Layer memory: %s, %u layers\n", V_pretifymem( (float)unBytes ), unCountUnreserved );

		unBytes = 0;
		FOR_EACH_MAP( m_mapBorderRadiusOpacityMasks, i )
		{
			// Doesn't account for padding in stride... close enough
			D3D10_TEXTURE2D_DESC desc;
			m_mapBorderRadiusOpacityMasks[i].m_pTexture2D->GetDesc( &desc );
			unBytes += desc.Height * desc.Width;		
		}
		unTotalBytes += unBytes;
		Msg( "   Border Radius Masks: %s\n", V_pretifymem( (float)unBytes ) );

		D3D10_TEXTURE2D_DESC desc;
		m_pBackBuffer->GetDesc( &desc );
		unTotalBytes += desc.Width*desc.Height*4;

		Msg( "   Back buffer: %s\n", V_pretifymem( (float)(desc.Width*desc.Height*4) ) );

		Msg( "!! Total Tracked: %s\n", V_pretifymem( (float)(unTotalBytes) ) );
		m_flLastStatsDump = UIEngine()->GetCurrentFrameTime();
	}

#ifdef _NVPERFKIT
	NVPMUINT unCount;
	GetNvPmApi()->Sample( m_hNVPMContext, NULL, &unCount );

	NVPMUINT64 value, cycle;
	Msg( "-------------------\n" );

	if ( NVPM_OK == GetNvPmApi()->GetCounterValueByName( m_hNVPMContext, "gpu_idle", 0, &value, &cycle) )
		Msg( "GPU Idle: %1.2f\n", ((float) value / (float) cycle * 100.0f) );

	if ( NVPM_OK == GetNvPmApi()->GetCounterValueByName( m_hNVPMContext, "shader_busy", 0, &value, &cycle) )
		Msg( "Shader Busy: %1.2f\n", ((float) value / (float) cycle * 100.0f) );

	if ( NVPM_OK == GetNvPmApi()->GetCounterValueByName( m_hNVPMContext, "texture_busy", 0, &value, &cycle) )
		Msg( "Texture Busy: %1.2f\n", ((float) value / (float) cycle * 100.0f) );

	if ( NVPM_OK == GetNvPmApi()->GetCounterValueByName( m_hNVPMContext, "vertex_shader_instruction_rate", 0, &value, &cycle) )
		Msg( "Vertex Shader Ins Rate: %1.2f\n", ((float) value ) );

	if ( NVPM_OK == GetNvPmApi()->GetCounterValueByName( m_hNVPMContext, "pixel_shader_instruction_rate", 0, &value, &cycle) )
		Msg( "Pixel Shader Ins Rate: %1.2f\n", ((float) value) );

	if ( NVPM_OK == GetNvPmApi()->GetCounterValueByName( m_hNVPMContext, "rop_busy", 0, &value, &cycle) )
		Msg( "ROP Busy: %1.2f\n", ((float) value / (float) cycle * 100.0f) );

	if ( NVPM_OK == GetNvPmApi()->GetCounterValueByName( m_hNVPMContext, "shader_waits_for_texture", 0, &value, &cycle) )
		Msg( "Shader waits on Tex: %1.2f\n", ((float) value / (float) cycle * 100.0f) );
	
	if ( NVPM_OK == GetNvPmApi()->GetCounterValueByName( m_hNVPMContext, "shader_waits_for_rop", 0, &value, &cycle) )
		Msg( "Shader waits on ROP: %1.2f\n", ((float) value / (float) cycle * 100.0f) );

	if ( NVPM_OK == GetNvPmApi()->GetCounterValueByName( m_hNVPMContext, "stream_out_busy", 0, &value, &cycle) )
		Msg( "Stream out Busy: %1.2f\n", ((float) value / (float) cycle * 100.0f) );

	if ( NVPM_OK == GetNvPmApi()->GetCounterValueByName( m_hNVPMContext, "shaded_pixel_count", 0, &value, &cycle) )
		Msg( "Shaded Pixels: %1.2f\n", ((float) value));


#endif

	// Delete any textures we've been told to free
	{
		AUTO_LOCK( m_lockTextureMap );
		while( m_tsQueueTextureDeletes.Count() )
		{
			uint32 unTexID = 0;
			if( m_tsQueueTextureDeletes.PopItem( &unTexID ) )
			{
				short iMap = m_mapTextures.Find( unTexID );
				if( iMap != m_mapTextures.InvalidIndex() )
				{
					delete m_mapTextures[iMap];
					m_mapTextures.RemoveAt( iMap );
				}
				else
				{
					AssertMsg( false, "Texture in delete list doesn't exist.  Bad delete call previously?" );
				}
			}
		}
	}

	double flNow = m_flCurrentRenderFrameTime;
	{
		while( m_listCompositionLayersLRU.Count() )
		{
			int iList = m_listCompositionLayersLRU.Head();
			int iTree = m_listCompositionLayersLRU[iList];
			CCompositionLayer *pLayer = m_treeFreeCompositionLayers[ iTree ];
			if ( flNow - pLayer->GetTimeLastUsed() > 1.5f )
			{
				m_listCompositionLayersLRU.Remove( iList );
				m_treeFreeCompositionLayers.RemoveAt( iTree );

				SAFE_LAYER_DELETE( pLayer );
			}
			else
			{
				break;
			}
		}
	}

	{
		AUTO_LOCK( m_MutexReservedLayers );
		while( m_listReservedCompositionLayerLRU.Count() )
		{
			int iList = m_listReservedCompositionLayerLRU.Head();
			ReservedLayerLRU_t &lru = m_listReservedCompositionLayerLRU[iList];
			if ( flNow - lru.m_flLastUseTime > 3.0f )
			{
				ReservedLayer_t &data = m_mapReservedCompositionLayers[ lru.m_iMap ];

				SAFE_LAYER_DELETE( data.m_pLayer );

				m_mapReservedCompositionLayers.RemoveAt( lru.m_iMap );
				m_listReservedCompositionLayerLRU.Remove( iList );
			}
			else
			{
				break;
			}
		}
	}

	{
		while( m_listShadowLayerLRU.Count() )
		{
			int iList = m_listShadowLayerLRU.Head();
			ShadowLayerLRU_t &lru = m_listShadowLayerLRU[iList];
			if ( flNow - lru.m_flLastUseTime > 2.0f )
			{
				ShadowLayer_t &data = m_mapShadowLayers[ lru.m_iMap ];
				SAFE_LAYER_DELETE( data.m_pLayer );
				m_mapShadowLayers.RemoveAt( lru.m_iMap );
				m_listShadowLayerLRU.Remove( iList );
			}
			else
			{
				break;
			}
		}
	}

	m_pTextLayoutDrawCache->DeleteOlderEntriesToTextureCache( flNow - 1.0f, m_pTextTextureCache );

	{
		while( m_listLinearGradientBrushLRU.Count() )
		{
			int iList = m_listLinearGradientBrushLRU.Head();
			LinearGradientBrushLRU_t &lru = m_listLinearGradientBrushLRU[iList];
			if ( flNow - lru.m_flLastUseTime > 2.0f )
			{
				LinearGradientBrush_t &data = m_mapLinearGradientBrushes[ lru.m_iMap ];
				SAFE_RELEASE( data.m_pBrush );
				CRenderMsg<CMsgLinearGradient>::FreeProtoBufMsgObject( m_mapLinearGradientBrushes.Key( lru.m_iMap ) );
				m_mapLinearGradientBrushes.RemoveAt( lru.m_iMap );
				m_listLinearGradientBrushLRU.Remove( iList );
			}
			else
			{
				break;
			}
		}
	}

	{
		while( m_listRadialGradientBrushLRU.Count() )
		{
			int iList = m_listRadialGradientBrushLRU.Head();
			RadialGradientBrushLRU_t &lru = m_listRadialGradientBrushLRU[iList];
			if ( flNow - lru.m_flLastUseTime > 2.0f )
			{
				RadialGradientBrush_t &data = m_mapRadialGradientBrushes[ lru.m_iMap ];
				SAFE_RELEASE( data.m_pBrush );
				delete m_mapRadialGradientBrushes.Key( lru.m_iMap );
				m_mapRadialGradientBrushes.RemoveAt( lru.m_iMap );
				m_listRadialGradientBrushLRU.Remove( iList );
			}
			else
			{
				break;
			}
		}
	}

	{
		while ( m_listSolidColorBrushLRU.Count() )
		{
			int iList = m_listSolidColorBrushLRU.Head();
			SolidBrushLRU_t &lru = m_listSolidColorBrushLRU[iList];
			if ( flNow - lru.m_flLastUseTime > 1.0f )
			{
				SolidBrush_t &brush = m_mapSolidColorBrushes[lru.m_iMap];
				SAFE_RELEASE( brush.m_pBrush );
				m_mapSolidColorBrushes.RemoveAt( lru.m_iMap );
				m_listSolidColorBrushLRU.Remove( iList );
			}
			else
			{
				break;
			}
		}
	}

	{
		while ( m_listBorderRadiusOpacityMaskLRU.Count() )
		{
			int iList = m_listBorderRadiusOpacityMaskLRU.Head();
			BorderRadiusOpacityMaskLRU_t &lru = m_listBorderRadiusOpacityMaskLRU[iList];
			if ( flNow - lru.m_flLastUseTime > 2.0f )
			{
				BorderRadiusOpacityMaskData_t &data = m_mapBorderRadiusOpacityMasks[lru.m_iMap];
				SAFE_RELEASE( data.m_pShaderResourceView );
				SAFE_RELEASE( data.m_pTexture2D );

				m_mapBorderRadiusOpacityMasks.RemoveAt( lru.m_iMap );
				m_listBorderRadiusOpacityMaskLRU.Remove( iList );
			}
			else
			{
				break;
			}
		}
	}

	m_FrameTimer.End();
	m_nLastFrameMillisecondsIndex++;
	if ( m_nLastFrameMillisecondsIndex >= V_ARRAYSIZE( m_rgflMillisecondsFrame ) )
	{
		// accumulate frame counts and times into the slow fps counters
		for ( int i=0; i < V_ARRAYSIZE( m_rgflMillisecondsFrame ); ++i )
		{
			if ( m_rgflMillisecondsFrame[i] < 1000.0f && ( m_nFramesRendered == 0 || Plat_IsInDebugSession() )  ) // ignore 1 second or longer frames, chances you were in the debugger or doing startup. If they really are running this slow all the time then the avg FPS measurement will tell us.
			{
				m_flRenderFrameTime +=  m_rgflMillisecondsFrame[i];
				m_flRenderSessionFrameTime += m_rgflMillisecondsFrame[i];
			}
		}
		m_nFramesRendered += m_nLastFrameMillisecondsIndex;
		m_nSessionFramesRendered += m_nLastFrameMillisecondsIndex;

		m_nLastFrameMillisecondsIndex = 0;

		if ( m_nFramesRendered >= k_nMillion/GetMinFrameTimeInMicroseconds()*k_nSlowFPSSeconds ) // keep track of 5 second windows, with our expected FPS of 120hz
		{
			float flFPSAvg = GetSessionFPSAverages();
			if ( flFPSAvg < k_flMinFPSForSlowReport ) // was this below our threshold?
			{
				m_nSlowFPSPeriod++;
			}
			m_nFramesRendered = 0; // reset the counts and track the next window
			m_flRenderFrameTime = 0.0f;
		}
	}

	m_rgflMillisecondsFrame[ m_nLastFrameMillisecondsIndex ] = m_FrameTimer.GetDuration().GetMillisecondsF();
	m_FrameTimer.Start();
}


//-----------------------------------------------------------------------------
// Purpose: Gets framerate average for the last few frames
//-----------------------------------------------------------------------------
float CD3D10D2DSurface::GetFPSAverage()
{
	double flSum = 0.0f;
	int nDivisor = 0;
	for ( int i=0; i < V_ARRAYSIZE( m_rgflMillisecondsFrame ); ++i )
	{
		if ( m_rgflMillisecondsFrame[i] != FLT_MAX )
		{
			++nDivisor;
			flSum += m_rgflMillisecondsFrame[i];
		}
	}

	return 1000.0f / ((float)((double)flSum / (double)nDivisor));
}


//-----------------------------------------------------------------------------
// Purpose: Gets framerate average since creation
//-----------------------------------------------------------------------------
float CD3D10D2DSurface::GetSessionFPSAverages()
{
	return (m_nSessionFramesRendered*1000.0f)/m_flRenderSessionFrameTime;
}


//-----------------------------------------------------------------------------
// Purpose: Check if our device was removed underneath us and crash cleanly if so
//-----------------------------------------------------------------------------
bool CD3D10D2DSurface::BCheckForDeviceRemovedAndCrash()
{
	if ( m_bCrashed )
		return true;


	// Check device is valid -- needed because AMD can't write code and their drivers like to crash under us a lot on Win 8
	HRESULT hDeviceRemoved = AccessDevice()->GetDeviceRemovedReason();
	if ( hDeviceRemoved != S_OK )
	{
		const char *pchErrorString = "D3D Device was removed, this usually indicates a bad device driver or may indicate bad hardware.  Big Picture must exit.";
		switch( hDeviceRemoved )
		{
		case DXGI_ERROR_DEVICE_HUNG:
			pchErrorString = "D3D Device was removed due to DXGI_ERROR_DEVICE_HUNG, this usually indicates a bad device driver or may indicate bad hardware.  Big Picture must exit.";
			break;
		case DXGI_ERROR_DEVICE_REMOVED:
			pchErrorString = "D3D Device was removed due to DXGI_ERROR_DEVICE_REMOVED, this usually indicates you removed the device at runtime or performed a driver upgrade while running.  Big Picture must exit.";
			break;
		case DXGI_ERROR_DEVICE_RESET:
			pchErrorString = "The D3D device was removed with DXGI_ERROR_DEVICE_RESET.  This typically indicates a driver bug, or GPU hardware failing/overheating.  If you were running a game at the time of this error then the game may have triggered a driver bug or caused your GPU to overheat.   Big Picture must exit.";
			break;
		case DXGI_ERROR_DRIVER_INTERNAL_ERROR:
			pchErrorString = "D3D Device was removed due to DXGI_ERROR_DEVICE_HUNG, this usually indicates a bad device driver or may indicate bad hardware.  Big Picture must exit.";
			break;
		case DXGI_ERROR_INVALID_CALL:
			pchErrorString = "D3D Device was removed due to DXGI_ERROR_INVALID_CALL, this usually indicates a bad device driver or may indicate bad hardware.  Big Picture must exit.";
			break;
		default:
			break;
		}

		AssertMsg( false, pchErrorString );

		// Calling this directly is thread safe, non method wrapper functions are not thread safe.
		UIEngine()->DispatchEventAsync( 0.0f, AsyncPanoramaQuitWithError::MakeEvent( NULL, pchErrorString ) );

		m_bCrashed = true;
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Find a matching free composition layer, or create a new one
//-----------------------------------------------------------------------------
CCompositionLayer * CD3D10D2DSurface::GetCompositionLayer( CCompositionLayer &search )
{
	int iTree = m_treeFreeCompositionLayers.Find( &search );
	if ( iTree == m_treeFreeCompositionLayers.InvalidIndex() )
	{
		VPROF_BUDGET( "CreateCompositionLayer", VPROF_BUDGETGROUP_TENFOOT );

		if ( BCheckForDeviceRemovedAndCrash() )
			return NULL;

		// Create a layer
		D3D10_TEXTURE2D_DESC desc;
		desc.Width = search.GetWidth();
		desc.Height = search.GetHeight();
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Usage = D3D10_USAGE_DEFAULT;
		desc.BindFlags = D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0 ;

		ID3D10Texture2D *pSurface = NULL;
		HRESULT hRes = AccessDevice()->CreateTexture2D( &desc, NULL, &pSurface );
		if ( SUCCEEDED( hRes ) )
		{
			D3D10_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;
			renderTargetViewDesc.Format = desc.Format;
			renderTargetViewDesc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2D;
			renderTargetViewDesc.Texture2D.MipSlice = 0;

			ID3D10RenderTargetView *pRenderTargetView = NULL;
			if ( SUCCEEDED( AccessDevice()->CreateRenderTargetView( pSurface, &renderTargetViewDesc, &pRenderTargetView ) ) )
			{
				ID3D10ShaderResourceView *pShaderResourceView = NULL;
				if ( SUCCEEDED( AccessDevice()->CreateShaderResourceView( pSurface, NULL, &pShaderResourceView ) ) )
				{
					IDXGISurface *pDXGISurface = NULL;
					if ( SUCCEEDED( pSurface->QueryInterface( &pDXGISurface ) ) )
					{
						ID2D1RenderTarget *pRenderTarget = NULL;

						D2D1_RENDER_TARGET_PROPERTIES props =
							D2D1::RenderTargetProperties(
							D2D1_RENDER_TARGET_TYPE_DEFAULT,
							D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED),
							96,
							96
							);

						HRESULT hr = AccessD2D1Factory()->CreateDxgiSurfaceRenderTarget(
							pDXGISurface,
							&props,
							&pRenderTarget
							);

						SAFE_RELEASE( pDXGISurface );

						if ( SUCCEEDED( hr ) )
						{
							VPROF_BUDGET( "ConstructCompositionLayer", VPROF_BUDGETGROUP_TENFOOT );
							CCompositionLayer *pLayer = (CCompositionLayer *)m_CompositionLayerPool.Alloc();
							ConstructSevenArg( pLayer, this, pSurface, pRenderTarget, pRenderTargetView, pShaderResourceView, search.GetWidth(), search.GetHeight() );
							iTree = m_treeFreeCompositionLayers.Insert( pLayer );

							SAFE_RELEASE( pRenderTarget );
						}
						else
						{
							BCheckForDeviceRemovedAndCrash();
						}
					}

					SAFE_RELEASE( pShaderResourceView );
				}
				else
				{
					BCheckForDeviceRemovedAndCrash();
				}
				
				SAFE_RELEASE( pRenderTargetView );
			}
			else
			{
				BCheckForDeviceRemovedAndCrash();
			}
		
			SAFE_RELEASE( pSurface );
		}
		else
		{
			BCheckForDeviceRemovedAndCrash();
		}
	}

	if ( iTree != m_treeFreeCompositionLayers.InvalidIndex() )
	{
		CCompositionLayer *pLayer = m_treeFreeCompositionLayers[iTree];
		pLayer->UpdateTimeLastUsedAndLRUListForAccess( m_flCurrentRenderFrameTime, m_listCompositionLayersLRU );
		pLayer->CheckAndClearClipLayers();
		m_treeFreeCompositionLayers.RemoveAt( iTree );
		return pLayer;
	}

	AssertMsg( false, "Failed to create/find composition layer" );
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Called to push a new compositing layer
//-----------------------------------------------------------------------------
void CD3D10D2DSurface::ClearShaderResourceVariables()
{
	VPROF_BUDGET( "CD3D10D2DSurface::ClearShaderResourceVariables", VPROF_BUDGETGROUP_TENFOOT );

	// Avoid dumb D3D info spew about the render target being bound but still referenced as a shader resource, this isn't a real
	// issue since it will always change before actually drawing more.  But D3D can't figure that out.
	if ( m_pRenderEffect )
		m_pRenderEffect->m_pCurrentTexture2D = NULL;
	if ( m_pDiffuseTex )
		m_pDiffuseTex->SetResource( NULL );
	
	if ( m_pRenderEffect )
		m_pRenderEffect->m_pOpacityMask = NULL;
	
	if ( m_pOpacityMaskTex )
		m_pOpacityMaskTex->SetResource( NULL );

	if ( m_pRenderEffect )
		m_pRenderEffect->m_pOpacityMaskTwo = NULL;
	if ( m_pOpacityMaskTexTwo )
		m_pOpacityMaskTexTwo->SetResource( NULL );

	if ( m_pRenderEffect )
		m_pRenderEffect->m_pCurrentTextureY = NULL;
	if ( m_pYTex )
		m_pYTex->SetResource( NULL );

	if ( m_pRenderEffect )
		m_pRenderEffect->m_pCurrentTextureU = NULL;
	if ( m_pUTex )
		m_pUTex->SetResource( NULL );

	if ( m_pRenderEffect )
		m_pRenderEffect->m_pCurrentTextureV = NULL;
	if ( m_pVTex )
		m_pVTex->SetResource( NULL );

	if ( AccessDevice() )
	{
		ID3D10ShaderResourceView *const pSRV[1] = {NULL};
		AccessDevice()->PSSetShaderResources( 0, 1, pSRV );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Explict free of a composition layer
//-----------------------------------------------------------------------------
void CD3D10D2DSurface::FreeCompositingLayer( const CRenderMsg<CMsgFreeCompositingLayer> &renderCommand )
{
	AUTO_LOCK( m_MutexReservedLayers );
	int iMap = m_mapReservedCompositionLayers.Find( renderCommand.BodyConst().layer_id() );
	if ( iMap != m_mapReservedCompositionLayers.InvalidIndex() )
	{
		ReservedLayer_t &layer = m_mapReservedCompositionLayers[iMap];

		int iTree = m_treeFreeCompositionLayers.Insert( layer.m_pLayer );
		m_treeFreeCompositionLayers[iTree]->SetLRUPos( m_listCompositionLayersLRU.AddToTail( iTree ) );
		m_listReservedCompositionLayerLRU.Remove( layer.m_iLRUIndex );
		m_mapReservedCompositionLayers.RemoveAt( iMap );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called to ping a composition layer, verifying it's reserved for re-use, and 
// increasing it's expiration so it doesn't LRU for a bit.
//-----------------------------------------------------------------------------
bool CD3D10D2DSurface::PingCompositingLayer( uint64 ulLayerID, float flWidth, float flHeight )
{
	// The below can be quite useful for perf debugging/debugging why panels get past
	// the layout thread simple heuristic cull, but it's not actually thread safe as the
	// panel can be deleted, hence the try catch as you will hit access violations on read, 
	// only use when you really need to debug this
#if 0
	try
	{
		CPanelPtr<CPanel2D> ptr;
		ptr.SetFromUInt64( ulLayerID );
		Msg( "Ping layer: %s (%s)\n", ptr->GetID(), ptr->GetPanelType().String() );
	}
	catch( ... )
	{
	}
#endif

	AUTO_LOCK( m_MutexReservedLayers );
	int iMap = m_mapReservedCompositionLayers.Find( ulLayerID );
	if ( iMap != m_mapReservedCompositionLayers.InvalidIndex() )
	{
		ReservedLayer_t &layer = m_mapReservedCompositionLayers[iMap];
		if ( layer.m_pLayer->GetWidth() == flWidth && layer.m_pLayer->GetHeight() == flHeight )
		{
			m_listReservedCompositionLayerLRU.LinkToTail( layer.m_iLRUIndex );
			m_listReservedCompositionLayerLRU[m_listReservedCompositionLayerLRU.Tail()].m_flLastUseTime = m_flCurrentRenderFrameTime;

			return true;
		}
		else
		{
			return false;
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Called to push a new compositing layer
//-----------------------------------------------------------------------------
void CD3D10D2DSurface::PushCompositingLayer( const CRenderMsg<CMsgPushCompositingLayer> &renderCommand )
{
	VPROF_BUDGET( "CD3D10D2DSurface::PushCompositingLayer", VPROF_BUDGETGROUP_TENFOOT );
	PushPerfEvent( D3DCOLOR_RGBA( 0, 0, 255, 255 ), L"PushCompositingLayer()" );

	if ( m_stackCompositionLayers.Count() )
	{
		CCompositionLayer *pLayer = m_stackCompositionLayers[m_stackCompositionLayers.Count()-1];
		pLayer->PopClipLayersAndFlush();
	}
	FlushCurrentVertexBuffer( m_pTechnique );

	const CMsgPushCompositingLayer &msgBody = renderCommand.BodyConst();

	CCompositionLayer *pLayer = NULL;
	{
		VPROF_BUDGET( "CD3D10D2DSurface::PushCompositingLayer - find layer", VPROF_BUDGETGROUP_TENFOOT );
		AUTO_LOCK( m_MutexReservedLayers );
		int iMap = m_mapReservedCompositionLayers.Find( msgBody.layer_id() );
		if ( iMap != m_mapReservedCompositionLayers.InvalidIndex() )
		{
			ReservedLayer_t &layer = m_mapReservedCompositionLayers[iMap];
			
			CCompositionLayer *pPotentialLayer = layer.m_pLayer;
			if ( pPotentialLayer->GetWidth() == ceil(msgBody.width()) && pPotentialLayer->GetHeight() == ceil(msgBody.height()) )
			{
				pLayer = pPotentialLayer;
				m_listReservedCompositionLayerLRU.LinkToTail( layer.m_iLRUIndex );
				m_listReservedCompositionLayerLRU[m_listReservedCompositionLayerLRU.Tail()].m_flLastUseTime = m_flCurrentRenderFrameTime;
			}
			else
			{
				// Free up the layer, since it no longer matches our size, should only happen if also actually redrawing
				Assert( msgBody.needs_clear() );

				// Below is not thread safe, can't use panelptr on this thread, BUT, super useful if debugging this assert being hit
#if 0
				if ( !msgBody.needs_clear() )
				{
					CPanelPtr< CPanel2D > ptr;
					ptr.SetFromUInt64( msgBody.layer_id() );
					AssertMsg1( msgBody.needs_clear(), "Panel %s has wrong size composition layer being re-used", ptr->GetID() );
				}
#endif 

				int iTree = m_treeFreeCompositionLayers.Insert( pPotentialLayer );
				m_treeFreeCompositionLayers[iTree]->SetLRUPos( m_listCompositionLayersLRU.AddToTail( iTree ) );
				m_listReservedCompositionLayerLRU.Remove( layer.m_iLRUIndex );
				m_mapReservedCompositionLayers.RemoveAt( iMap );
			}
		}
	}

	if ( !pLayer )
	{
		VPROF_BUDGET( "CD3D10D2DSurface::PushCompositingLayer - create new layer", VPROF_BUDGETGROUP_TENFOOT );


		// Better really be drawing the layer if we didn't find it..., currently, you may hit this assert if you pause
		// in the debugger too long because the mainthread/animation thread won't have pinged us to keep the layer and
		// a bad cleanup will occur.  We need a strategy to ensure we don't cleanup if we've been stalled in the debugger
		// to fix that, but it isn't an issue that will impact users.
		Assert( msgBody.needs_clear() );

		CCompositionLayer search( this, NULL, NULL, NULL, NULL, msgBody.width(), msgBody.height() );
		pLayer = GetCompositionLayer( search );
	}
	
	float flWidthDiscard = (ceil( msgBody.width() ) - msgBody.width() ) / ceil( msgBody.width() );
	float flHeightDiscard = (ceil( msgBody.height() ) - msgBody.height() ) / ceil( msgBody.height() );

	// Should now be valid, unless D3D actually failed...
	if ( pLayer )
	{
		VPROF_BUDGET( "CD3D10D2DSurface::PushCompositingLayer - parse msg", VPROF_BUDGETGROUP_TENFOOT );

		// Update layer data
		Color compositionColor;
		compositionColor.SetRawColor( msgBody.composition_color() );

		float r = compositionColor.r() / 255.0f;
		float g = compositionColor.g() / 255.0f;
		float b = compositionColor.b() / 255.0f;
		float a = compositionColor.a() / 255.0f;
		float flOpacity = msgBody.opacity();

		// Combine opacity and color, and convert to pre-multiplied alpha
		r = Lerp( a, 1.0f, r ) * flOpacity;
		g = Lerp( a, 1.0f, g ) * flOpacity;
		b = Lerp( a, 1.0f, b ) * flOpacity;

		// Remember, pre-multiplied alpha in use!
		VertexTextured_t *pQuad = pLayer->AccessRenderQuad();
		pQuad[0].r = r;
		pQuad[0].g = g;
		pQuad[0].b = b;
		pQuad[0].a = flOpacity;
		pQuad[0].rhw = 1.0f;
		pQuad[0].masku1 = pQuad[0].masku1 = pQuad[0].u = 0.0f;
		pQuad[0].maskv1 = pQuad[0].maskv1 = pQuad[0].v = 0.0f;
		pQuad[0].x = msgBody.layer_quad_top_left_x();
		pQuad[0].y = msgBody.layer_quad_top_left_y();
		pQuad[0].z = msgBody.layer_quad_top_left_z();

		pQuad[1].r = r;
		pQuad[1].g = g;
		pQuad[1].b = b;
		pQuad[1].a = flOpacity;
		pQuad[1].rhw = 1.0f;
		pQuad[1].masku1 = pQuad[1].masku1 = pQuad[1].u = 1.0f - flWidthDiscard;
		pQuad[1].maskv1 = pQuad[1].maskv1 = pQuad[1].v = 0.0f;
		pQuad[1].x = msgBody.layer_quad_top_right_x();
		pQuad[1].y = msgBody.layer_quad_top_right_y();
		pQuad[1].z = msgBody.layer_quad_top_right_z();

		pQuad[2].r = r;
		pQuad[2].g = g;
		pQuad[2].b = b;
		pQuad[2].a = flOpacity;
		pQuad[2].rhw = 1.0f;
		pQuad[2].masku1 = pQuad[2].masku1 = pQuad[2].u = 0.0f;
		pQuad[2].maskv1 = pQuad[2].maskv1 = pQuad[2].v = 1.0f - flHeightDiscard;
		pQuad[2].x = msgBody.layer_quad_bottom_left_x();
		pQuad[2].y = msgBody.layer_quad_bottom_left_y();
		pQuad[2].z = msgBody.layer_quad_bottom_left_z();

		pQuad[3].r = r;
		pQuad[3].g = g;
		pQuad[3].b = b;
		pQuad[3].a = flOpacity;
		pQuad[3].rhw = 1.0f;
		pQuad[3].masku1 = pQuad[3].masku1 = pQuad[3].u = 1.0f - flWidthDiscard;
		pQuad[3].maskv1 = pQuad[3].maskv1 = pQuad[3].v = 1.0f - flHeightDiscard;
		pQuad[3].x = msgBody.layer_quad_bottom_right_x();
		pQuad[3].y = msgBody.layer_quad_bottom_right_y();
		pQuad[3].z = msgBody.layer_quad_bottom_right_z();

		RenderMsgMatrixToFloatArray( pLayer->AccessMatrix(), msgBody );

		pLayer->SetContextID( msgBody.layer_id() );
		pLayer->SetSaturation( msgBody.saturation() );
		pLayer->SetHueShift( msgBody.hue_shift() );
		pLayer->SetBrightness( msgBody.brightness() );
		pLayer->SetContrast( msgBody.contrast() );
		pLayer->SetOpacityMaskTextureID( msgBody.opacity_mask_texture_id(), msgBody.opacity_mask_opacity() );

		if ( msgBody.has_border_radius() )
		{
			const CRadiusData &radius = msgBody.border_radius();
			pLayer->SetCornerRadii( radius.top_left().horizontal(), radius.top_left().vertical(),
				radius.top_right().horizontal(), radius.top_right().vertical(),
				radius.bottom_right().horizontal(), radius.bottom_right().vertical(),
				radius.bottom_left().horizontal(), radius.bottom_left().vertical() );
		}
		else
		{
			pLayer->SetCornerRadii( 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f );
		}

		if ( msgBody.has_border() )
		{
			const CBorderData &border = msgBody.border();
			pLayer->SetBorder( border.top().width(), border.right().width(), border.bottom().width(), border.left().width(),
				border.top().color(), border.right().color(), border.bottom().color(), border.left().color() );
		}
		else
		{
			pLayer->SetBorder( 0.0f, 0.0f, 0.0f, 0.0f, 0, 0, 0, 0 );
		}
		
		if ( msgBody.has_box_shadow() )
		{
			const CBoxShadowData &shadow = msgBody.box_shadow();
			pLayer->SetBoxShadow( shadow.inset(), shadow.fill(), shadow.horizontal_offset(), shadow.vertical_offset(), 
				shadow.blur_radius(), shadow.spread_distance(), shadow.color(), shadow.animating() );
		}
		else
		{
			pLayer->SetBoxShadow( false, false, 0.0f, 0.0f, 0.0f, 0.0f, 0, false );
		}

		pLayer->SetBlurValues( msgBody.gaussianblur_passes(), msgBody.gaussianblur_stddevhor(), msgBody.gaussianblur_stddevver() );

		pLayer->Set2DScaleFactors( msgBody.scale_2d_factors_x(), msgBody.scale_2d_factors_y() );

		pLayer->Set2DRotate( msgBody.rotate_2d() );

		ClearShaderResourceVariables();

		// Add to stack to keep track of this layer
		m_stackCompositionLayers.AddToTail( pLayer );

		// We don't actually clear and begin draw until we receive the clear message for the layer
		if ( msgBody.needs_clear() )
			ClearCompositingLayer( pLayer );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get shader resource view for an opacity mask specified by a set of rounded corner radii
//-----------------------------------------------------------------------------
ID3D10ShaderResourceView *CD3D10D2DSurface::GetOpacityMaskShaderResourceViewForCornerRadii( float flWidth, float flHeight, float flXInset, float flYInset, float *pflCornerRadii )
{
	VPROF_BUDGET( "CD3D10D2DSurface::GetOpacityMaskShaderResourceViewForCornerRadii", VPROF_BUDGETGROUP_TENFOOT );
	bool bHasRounding = false;
	for( int i=0; i < 8; ++i )
	{
		if ( pflCornerRadii[i] != 0.0f )
		{
			bHasRounding = true;
			break;
		}
	}

	// If we have no rounding, then use the noop 0 opacity mask texture which will apply no clipping
	if ( !bHasRounding )
		return NULL;

	// If we have rounding data, then we need to find or create a d3d texture to use as an opacity mask to
	// clip to that rounding.
	
	BorderRadiusOpacityMaskKey_t key;
	key.m_flWidth = flWidth;
	key.m_flHeight = flHeight;
	key.m_flXInset = flXInset;
	key.m_flYInset = flYInset;
	V_memcpy( key.m_rgRadii, pflCornerRadii, sizeof( key.m_rgRadii ) );

	// In order to avoid a huge number of masks with very very slightly different radius, round off
	// the values to a 1/2 a pixel.  That's plenty of precision here.
	for( int i=0; i < V_ARRAYSIZE( key.m_rgRadii ); ++i )
	{
		key.m_rgRadii[i] = ((int)floorf( key.m_rgRadii[i] * 2.0f +  0.5)) / 2.0f;
	}

	short i = m_mapBorderRadiusOpacityMasks.Find( key );
	if ( i != m_mapBorderRadiusOpacityMasks.InvalidIndex() )
	{
		// Update LRU position, then return 
		BorderRadiusOpacityMaskData_t &data = m_mapBorderRadiusOpacityMasks[i];
		ID3D10ShaderResourceView *pShaderResourceView = data.m_pShaderResourceView;
		m_listBorderRadiusOpacityMaskLRU[data.m_iLRUIndex].m_flLastUseTime = m_flCurrentRenderFrameTime;
		m_listBorderRadiusOpacityMaskLRU.LinkToTail( data.m_iLRUIndex );
		data.m_iLRUIndex = m_listBorderRadiusOpacityMaskLRU.Tail();
		pShaderResourceView->AddRef();
		return pShaderResourceView;
	}
	else
	{

		BorderRadiusOpacityMaskData_t data;

		UINT uNumQualityLevels = 1;
		AccessDevice()->CheckMultisampleQualityLevels( DXGI_FORMAT_A8_UNORM, 1, &uNumQualityLevels );

		D3D10_TEXTURE2D_DESC desc;
		desc.Width = flWidth+flXInset*2;
		desc.Height = flHeight+flYInset*2;
		desc.MipLevels = desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = uNumQualityLevels - 1;
		desc.Usage = D3D10_USAGE_DEFAULT;
		desc.BindFlags = D3D10_BIND_SHADER_RESOURCE | D3D10_BIND_RENDER_TARGET;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;
		HRESULT hRes = AccessDevice()->CreateTexture2D( &desc, NULL, &data.m_pTexture2D );
		if ( SUCCEEDED( hRes ) && data.m_pTexture2D ) 
		{
			IDXGISurface *pDXGISurface = NULL;
			if ( SUCCEEDED( data.m_pTexture2D->QueryInterface( &pDXGISurface ) ) )
			{
				ID2D1RenderTarget *pRenderTarget = NULL;

				D2D1_RENDER_TARGET_PROPERTIES props =
					D2D1::RenderTargetProperties(
					D2D1_RENDER_TARGET_TYPE_DEFAULT,
					D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED),
					96,
					96
					);

				HRESULT hr = AccessD2D1Factory()->CreateDxgiSurfaceRenderTarget(
					pDXGISurface,
					&props,
					&pRenderTarget
					);

				SAFE_RELEASE( pDXGISurface );

				if ( SUCCEEDED( hr ) )
				{
					if ( BDrawRoundedCornerMaskBitmapToRenderTarget( pRenderTarget, flWidth, flHeight, flXInset, flYInset, pflCornerRadii[0], pflCornerRadii[1], pflCornerRadii[2], pflCornerRadii[3],
						pflCornerRadii[4], pflCornerRadii[5], pflCornerRadii[6], pflCornerRadii[7] ) )
					{
						// Success add to map

						// Set shader resource view
						if ( !data.m_pShaderResourceView )
						{
							HRESULT hRes = AccessDevice()->CreateShaderResourceView( data.m_pTexture2D, NULL, &data.m_pShaderResourceView );
							if ( FAILED( hRes ) || !data.m_pShaderResourceView )
								Log( "Failed creating shader resource view\n" );
						}

						// Add to map and LRU
						BorderRadiusOpacityMaskLRU_t lru;
						lru.m_iMap = m_mapBorderRadiusOpacityMasks.Insert( key, data );
						lru.m_flLastUseTime = m_flCurrentRenderFrameTime;
						m_mapBorderRadiusOpacityMasks[lru.m_iMap].m_iLRUIndex = m_listBorderRadiusOpacityMaskLRU.AddToTail( lru );
					}
					SAFE_RELEASE( pRenderTarget );
				}
			}

			if ( data.m_pShaderResourceView )
			{
				data.m_pShaderResourceView->AddRef();
			}
			return data.m_pShaderResourceView;
		}
		else
		{
			// Failure, let's try to fail in a way we still draw something rather than nothing...
			return NULL;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get shader resource view for an opacity mask
//-----------------------------------------------------------------------------
CD3D10D2DSurface::LockedOpacityMaskTextureShaderResourceView_t CD3D10D2DSurface::GetOpacityMaskShaderResourceViewForTexture( uint32 unTextureID )
{
	VPROF_BUDGET( "CD3D10D2DSurface::GetOpacityMaskShaderResourceViewForTexture", VPROF_BUDGETGROUP_TENFOOT );

	LockedOpacityMaskTextureShaderResourceView_t result;
	result.m_iLockHandle = -1;
	result.m_pShaderResource = NULL;

	IUITexture *pTexture = NULL;
	{
		AUTO_LOCK( m_lockTextureMap );
		short iMap = m_mapTextures.Find( unTextureID );
		if ( iMap != m_mapTextures.InvalidIndex() )
			pTexture = m_mapTextures[iMap];
	}

	if ( !pTexture )
	{
		if ( !m_pUITextureOpaqueMask )
		{
			static unsigned char unRGBA = 0xFF;
			DbgVerify( BCreateTexture( &m_pUITextureOpaqueMask, &unRGBA, 1, 1, 1, k_EFormatA8, k_EAlphaChannelType_Normal ) );
		}

		pTexture = m_pUITextureOpaqueMask;
	}

	result.m_pTexture = (CD3D10Texture*)pTexture;
	result.m_iLockHandle = result.m_pTexture->LockAndGetCurrentTexture( NULL, &result.m_pShaderResource, 0 );
	return result;
}


//-----------------------------------------------------------------------------
// Purpose: Release locked data for opacity mask texture
//-----------------------------------------------------------------------------
void CD3D10D2DSurface::ReleaseLockedOpacityMaskTextureShaderResourceView( LockedOpacityMaskTextureShaderResourceView_t &data )
{
	data.m_pTexture->Unlock( data.m_iLockHandle );
}


//-----------------------------------------------------------------------------
// Purpose: Called to create outer shadow layer for a given layer
//-----------------------------------------------------------------------------
CCompositionLayer *CD3D10D2DSurface::GetOuterShadowLayer( CCompositionLayer *pSourceLayer, bool bFill, float flHorOffset, float flVerOffset, float flBlurRadius, float flSpreadDistance, uint32 shadowColor, bool bAnimating )
{
	VPROF_BUDGET( "CD3D10D2DSurface::GetOuterShadowLayer", VPROF_BUDGETGROUP_TENFOOT );


	ShadowLayerKey_t key;
	if ( !bAnimating )
	{
		key.m_flBaseWidth = pSourceLayer->GetWidth();
		key.m_flBaseHeight = pSourceLayer->GetHeight();
		key.m_bInset = false;
		key.m_bFill = bFill;
		key.m_flHorOffset = flHorOffset;
		key.m_flVerOffset = flVerOffset;
		key.m_flBlurRadius = flBlurRadius;
		key.m_flSpreadDistance = flSpreadDistance; 
		key.m_shadowColor = shadowColor;

		V_memcpy( key.m_rgBorderRadii, pSourceLayer->AccessCornerRadii(), sizeof( key.m_rgBorderRadii ) );

		int iMap = m_mapShadowLayers.Find( key );
		if ( iMap != m_mapShadowLayers.InvalidIndex() )
		{
			ShadowLayer_t &layer = m_mapShadowLayers[iMap];

			m_listShadowLayerLRU.LinkToTail( layer.m_iLRUIndex );
			layer.m_iLRUIndex = m_listShadowLayerLRU.Tail();
			m_listShadowLayerLRU[m_listShadowLayerLRU.Tail()].m_flLastUseTime = m_flCurrentRenderFrameTime;

			return layer.m_pLayer;
		}
	}

	CCompositionLayer *pShadowOutLayer = NULL;
	if ( shadowColor != 0x00000000 )
	{
		float flPadding = ceil( flBlurRadius );
		// Make sure to bloat the original layer size by even dimensions for the
		// glow/shadow layer, or we won't be able to render in the exact center
		// and will end up with a shadow that jitters around when animated.
		int iSpreadDistance = int( flSpreadDistance + 2 ) & ~1;
		float flWidth = pSourceLayer->GetWidth() + iSpreadDistance + (flPadding * 2.0f);
		float flHeight = pSourceLayer->GetHeight() + iSpreadDistance + (flPadding * 2.0f);

		float flNewWidth = (uint32)flWidth + (32 - (uint32)flWidth % 32);
		float flNewHeight = (uint32)flHeight + (32 - (uint32)flHeight % 32);

		float flPaddingHor = flPadding + (flNewWidth - flWidth) / 2;
		float flPaddingVer = flPadding + (flNewHeight - flHeight) / 2;

		flWidth = flNewWidth;
		flHeight = flNewHeight;

		// Note: Composition layer will ceil() width/height, that's ok, we'll just draw slightly stretched, but when we draw into
		// the parent context we'll squish back down appropriately, which will result in pretty good linearly interpolated results for
		// sub pixel shadow boundaries.
		CCompositionLayer *pColorLayer = NULL;
		CCompositionLayer *pHorizontalBlurLayer = NULL;
		{
			CCompositionLayer search( this, NULL, NULL, NULL, NULL, flWidth, flHeight );
			pShadowOutLayer = GetCompositionLayer( search );
			pHorizontalBlurLayer = GetCompositionLayer( search );
		}

		{
			CCompositionLayer search( this, NULL, NULL, NULL, NULL, flWidth, flHeight );
			pColorLayer = GetCompositionLayer( search );
		}

		if ( !pColorLayer || !pHorizontalBlurLayer || !pShadowOutLayer )
		{
			SAFE_LAYER_DELETE( pColorLayer );
			SAFE_LAYER_DELETE( pHorizontalBlurLayer );
			SAFE_LAYER_DELETE( pShadowOutLayer );

			return NULL;
		}

		m_stackCompositionLayers.AddToTail( pShadowOutLayer );

		float flshadowcolor[4];
		flshadowcolor[3] = (shadowColor >> 24) / 255.0f;
		flshadowcolor[0] = (shadowColor & 0xFF)/255.0f * flshadowcolor[3];
		flshadowcolor[1] = ((shadowColor >> 8 ) & 0xFF)/255.0f * flshadowcolor[3];
		flshadowcolor[2] = ((shadowColor >> 16 ) & 0xFF)/255.0f * flshadowcolor[3];


		ID3D10RenderTargetView *pRenderTargetView = pColorLayer->AccessRenderTargetView();
		AccessDevice()->OMSetRenderTargets( 1, &pRenderTargetView, NULL );
		AccessDevice()->ClearRenderTargetView( pRenderTargetView, flshadowcolor );

		pRenderTargetView = pShadowOutLayer->AccessRenderTargetView();
		AccessDevice()->OMSetRenderTargets( 1, &pRenderTargetView, NULL );

		float clearcolor[4] = { 0.0, 0.0, 0.0, 0.0 };
		AccessDevice()->ClearRenderTargetView( pRenderTargetView, clearcolor );

		// Draw color layer into shadow output layer, with opacity mask from original layer to get correct border box.
		VertexTextured_t quad[4];
		quad[0].r = quad[0].g = quad[0].b = quad[0].a = 1.0f;
		quad[0].rhw = 1.0f;
		quad[0].masku1 = quad[0].masku2 = quad[0].u = 0.0f;
		quad[0].maskv1 = quad[0].maskv2 = quad[0].v = 0.0f;
		quad[0].x = flPaddingHor;
		quad[0].y = flPaddingVer;
		quad[0].z = 0.0f;

		quad[1].r = quad[1].g = quad[1].b = quad[1].a = 1.0f;
		quad[1].rhw = 1.0f;
		quad[1].masku1 = quad[1].masku2 = quad[1].u = 1.0f;
		quad[1].maskv1 = quad[1].maskv2 = quad[1].v = 0.0f;
		quad[1].x = pShadowOutLayer->GetWidth()-flPaddingHor;
		quad[1].y = flPaddingVer;
		quad[1].z = 0.0f;

		quad[2].r = quad[2].g = quad[2].b = quad[2].a = 1.0f;
		quad[2].rhw = 1.0f;
		quad[2].masku1 = quad[2].masku2 = quad[2].u = 0.0f;
		quad[2].maskv1 = quad[2].maskv2 = quad[2].v = 1.0f;
		quad[2].x = flPaddingHor;
		quad[2].y = pShadowOutLayer->GetHeight()-flPaddingVer;
		quad[2].z = 0.0f;

		quad[3].r = quad[3].g = quad[3].b = quad[3].a = 1.0f;
		quad[3].rhw = 1.0f;
		quad[3].masku1 = quad[3].masku2 = quad[3].u = 1.0f;
		quad[3].maskv1 = quad[3].maskv2 = quad[3].v = 1.0f;
		quad[3].x = pShadowOutLayer->GetWidth()-flPaddingHor;
		quad[3].y = pShadowOutLayer->GetHeight()-flPaddingVer;
		quad[3].z = 0.0f;

		// Update viewport size for shaders
		UpdateViewPortSize();

		float flMatrixIdentity[16] = { 1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f };

		if ( !m_pmatTransform )
			m_pmatTransform = m_pRenderEffect->m_pEffect->GetVariableByName( "g_MatTransform" );

		m_pmatTransform->AsMatrix()->SetMatrix( flMatrixIdentity );

		GetAndSetColorCorrectionShaderVarDefaults();

		// Find shader resource view for rounded corners mask 
		ID3D10ShaderResourceView *pOpacityMaskTwoResourceView = GetOpacityMaskShaderResourceViewForCornerRadii( pSourceLayer->GetWidth(), pSourceLayer->GetHeight(), 0.0f, 0.0f, pSourceLayer->AccessCornerRadii() );

		// Draw the color layer into the shadow output layer
		DrawTexturedQuadInternal( m_pTechnique, pColorLayer->AccessSurface(), pColorLayer->AccessShaderResourceView(), NULL, pOpacityMaskTwoResourceView, NULL, NULL, NULL, quad, 1.0f, 1.0f, 0.0f );
		FlushCurrentVertexBuffer( m_pTechnique );

		SAFE_RELEASE( pOpacityMaskTwoResourceView );

		if( flBlurRadius > 0.0f )
		{
			// The shadow layer now contains the correctly sized and shaped shadow, blur needs to be applied before it is drawn into parent layer.
			m_stackCompositionLayers.Remove( m_stackCompositionLayers.Count()-1 );
			m_stackCompositionLayers.AddToTail( pHorizontalBlurLayer );

			VertexTextured_t blurquad[4];
			blurquad[0].r = blurquad[0].g = blurquad[0].b = blurquad[0].a = 1.0f;
			blurquad[0].rhw = 1.0f;
			blurquad[0].masku1 = blurquad[0].masku2 = blurquad[0].u = 0.0f;
			blurquad[0].maskv1 = blurquad[0].maskv2 = blurquad[0].v = 0.0f;
			blurquad[0].x = 0.0f;
			blurquad[0].y = 0.0f;
			blurquad[0].z = 0.0f;

			blurquad[1].r = blurquad[1].g = blurquad[1].b = blurquad[1].a = 1.0f;
			blurquad[1].rhw = 1.0f;
			blurquad[1].masku1 = blurquad[1].masku2 = blurquad[1].u = 1.0f;
			blurquad[1].maskv1 = blurquad[1].maskv2 = blurquad[1].v = 0.0f;
			blurquad[1].x = pHorizontalBlurLayer->GetWidth();
			blurquad[1].y = 0.0f;
			blurquad[1].z = 0.0f;

			blurquad[2].r = blurquad[2].g = blurquad[2].b = blurquad[2].a = 1.0f;
			blurquad[2].rhw = 1.0f;
			blurquad[2].masku1 = blurquad[2].masku2 = blurquad[2].u = 0.0f;
			blurquad[2].maskv1 = blurquad[2].maskv2 = blurquad[2].v = 1.0f;
			blurquad[2].x = 0.0f;
			blurquad[2].y = pHorizontalBlurLayer->GetHeight();
			blurquad[2].z = 0.0f;

			blurquad[3].r = blurquad[3].g = blurquad[3].b = blurquad[3].a = 1.0f;
			blurquad[3].rhw = 1.0f;
			blurquad[3].masku1 = blurquad[3].masku2 = blurquad[3].u = 1.0f;
			blurquad[3].maskv1 = blurquad[3].maskv2 = blurquad[3].v = 1.0f;
			blurquad[3].x = pHorizontalBlurLayer->GetWidth();
			blurquad[3].y = pHorizontalBlurLayer->GetHeight();
			blurquad[3].z = 0.0f;

			UpdateViewPortSize();

			pRenderTargetView = pHorizontalBlurLayer->AccessRenderTargetView();
			AccessDevice()->OMSetRenderTargets( 1, &pRenderTargetView, NULL );
			AccessDevice()->ClearRenderTargetView( pRenderTargetView, clearcolor );

			// Update viewport size for shaders
			UpdateBlurVariables( Vector2D( 1.0f, 0.0f ),  flBlurRadius/2.0f, 1.0f/pHorizontalBlurLayer->GetWidth() );

			// Draw the current layer into horizontal blur surface
			DrawTexturedQuadInternal( m_pTechniqueBlur, pShadowOutLayer->AccessSurface(), pShadowOutLayer->AccessShaderResourceView(), NULL, NULL, NULL, NULL, NULL, blurquad, 1.0f, 1.0f, 0.0f );
			FlushCurrentVertexBuffer( m_pTechniqueBlur );

			ClearShaderResourceVariables();

			// Now draw back into the original layer adding vertical blur
			UpdateBlurVariables( Vector2D( 0.0f, 1.0f), flBlurRadius/2.0f, 1.0f/pHorizontalBlurLayer->GetHeight() );

			ID3D10RenderTargetView *pRenderTargetViewLayer = pShadowOutLayer->AccessRenderTargetView();
			AccessDevice()->OMSetRenderTargets( 1, &pRenderTargetViewLayer, NULL );
			AccessDevice()->ClearRenderTargetView( pRenderTargetViewLayer, clearcolor );

			DrawTexturedQuadInternal( m_pTechniqueBlur, pHorizontalBlurLayer->AccessSurface(), pHorizontalBlurLayer->AccessShaderResourceView(), NULL, NULL, NULL, NULL, NULL, blurquad, 1.0f, 1.0f, 0.0f );
			FlushCurrentVertexBuffer( m_pTechniqueBlur );
		}

		// If we aren't filling the inside of the box with the shadow as well, then clip it
		if ( !bFill )
		{
			float *pflCornerRadii = pSourceLayer->AccessCornerRadii();
			bool bRounded = false;
			for ( int i=0; i < 8; ++i )
			{
				if ( pflCornerRadii[i] != 0.0f )
				{
					bRounded = true;
					break;
				}
			}

			ID2D1RenderTarget *pD2DTarget = pShadowOutLayer->GetD2DRenderTarget();
			pD2DTarget->BeginDraw();

			if ( !bRounded )
			{
				D2D1_RECT_F clipRect;
				clipRect.left = (flWidth - pSourceLayer->GetWidth() ) / 2.0f;
				clipRect.right = clipRect.left + pSourceLayer->GetWidth();
				clipRect.top = (flHeight - pSourceLayer->GetHeight() ) / 2.0f;
				clipRect.bottom = clipRect.top + pSourceLayer->GetHeight();


				pD2DTarget->PushAxisAlignedClip( clipRect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE );
				pD2DTarget->Clear( D2D1::ColorF( 0.0f, 0.0f, 0.0f, 0.0f ) );
				pD2DTarget->PopAxisAlignedClip();
			}

			pD2DTarget->EndDraw();
		}

		m_stackCompositionLayers.Remove( m_stackCompositionLayers.Count()-1 );

		// color layer as well, though it wasn't in the stack
		int iTree = m_treeFreeCompositionLayers.Insert( pColorLayer );
		m_treeFreeCompositionLayers[iTree]->SetLRUPos( m_listCompositionLayersLRU.AddToTail( iTree ) );

		// finally, horizontal blur layer as well
		iTree = m_treeFreeCompositionLayers.Insert( pHorizontalBlurLayer );
		m_treeFreeCompositionLayers[iTree]->SetLRUPos( m_listCompositionLayersLRU.AddToTail( iTree ) );

		// Add to list of cached shadow layers
		if ( !bAnimating )
		{
			ShadowLayerLRU_t lru;
			ShadowLayer_t layer;
			layer.m_pLayer = pShadowOutLayer;
			int iMap = m_mapShadowLayers.Insert( key, layer );
			lru.m_iMap = iMap;
			lru.m_flLastUseTime = m_flCurrentRenderFrameTime;
			m_mapShadowLayers[iMap].m_iLRUIndex = m_listShadowLayerLRU.AddToTail( lru );
		}
	}
	

	// Shadow layer is returned, and must be put back in free stack by caller
	return pShadowOutLayer;
}


//-----------------------------------------------------------------------------
// Purpose: Called to draw and free a composition layer created with CreateOuterShadowLayer
//-----------------------------------------------------------------------------
void CD3D10D2DSurface::DrawOuterShadowLayer( CCompositionLayer *pShadowLayer, CCompositionLayer *pSourceLayer, bool bFill, float flHorOffset, float flVerOffset, float flBlurRadius, float flSpreadDistance, uint32 shadowColor )
{
	VPROF_BUDGET( "CD3D10D2DSurface::DrawOuterShadowLayer", VPROF_BUDGETGROUP_TENFOOT );

	VertexTextured_t *pBoxQuad = pSourceLayer->AccessRenderQuad();
	VertexTextured_t shadowquad[4];

	float flScale2DX, flScale2DY;
	pSourceLayer->Get2DScaleFactors( flScale2DX, flScale2DY );
	float flRotate2D;
	pSourceLayer->Get2DRotate( flRotate2D );

	float flOpacity = pSourceLayer->AccessRenderQuad()[0].a;
	float flPadding = ceil( flBlurRadius );

	// Make sure to bloat the original layer size by even dimensions for the
	// glow/shadow layer, or we won't be able to render in the exact center
	// and will end up with a shadow that jitters around when animated.
	int iSpreadDistance = int( flSpreadDistance + 2 ) & ~1;
	float flWidth = pSourceLayer->GetWidth() + iSpreadDistance + (flPadding * 2.0f);
	float flHeight = pSourceLayer->GetHeight() + iSpreadDistance + (flPadding * 2.0f);

	float flNewWidth = (uint32)flWidth + (32 - (uint32)flWidth % 32);
	float flNewHeight = (uint32)flHeight + (32 - (uint32)flHeight % 32);

	float flPaddingHor = flPadding + (flNewWidth - flWidth) / 2;
	float flPaddingVer = flPadding + (flNewHeight - flHeight) / 2;

	shadowquad[0].r = shadowquad[0].g = shadowquad[0].b = shadowquad[0].a = flOpacity;
	shadowquad[0].rhw = 1.0f;
	shadowquad[0].masku1 = shadowquad[0].masku2 = shadowquad[0].u = 0.0f;
	shadowquad[0].maskv1 = shadowquad[0].maskv2 = shadowquad[0].v = 0.0f;
	shadowquad[0].x = pBoxQuad[0].x - flPaddingHor + flHorOffset;
	shadowquad[0].y = pBoxQuad[0].y - flPaddingVer + flVerOffset;
	shadowquad[0].z = pBoxQuad[0].z;

	shadowquad[1].r = shadowquad[1].g = shadowquad[1].b = shadowquad[1].a = flOpacity;
	shadowquad[1].rhw = 1.0f;
	shadowquad[1].masku1 = shadowquad[1].masku2 = shadowquad[1].u = 1.0f;
	shadowquad[1].maskv1 = shadowquad[1].maskv2 = shadowquad[1].v = 0.0f;
	shadowquad[1].x = pBoxQuad[1].x + flPaddingHor + flHorOffset + flSpreadDistance;
	shadowquad[1].y = pBoxQuad[1].y - flPaddingVer + flVerOffset;
	shadowquad[1].z = pBoxQuad[1].z;

	shadowquad[2].r = shadowquad[2].g = shadowquad[2].b = shadowquad[2].a = flOpacity;
	shadowquad[2].rhw = 1.0f;
	shadowquad[2].masku1 = shadowquad[2].masku2 = shadowquad[2].u = 0.0f;
	shadowquad[2].maskv1 = shadowquad[2].maskv2 = shadowquad[2].v = 1.0f;
	shadowquad[2].x = pBoxQuad[2].x - flPaddingHor + flHorOffset;
	shadowquad[2].y = pBoxQuad[2].y + flPaddingVer + flVerOffset + flSpreadDistance;
	shadowquad[2].z = pBoxQuad[2].z;

	shadowquad[3].r = shadowquad[3].g = shadowquad[3].b = shadowquad[3].a = flOpacity;
	shadowquad[3].rhw = 1.0f;
	shadowquad[3].masku1 = shadowquad[3].masku2 = shadowquad[3].u = 1.0f;
	shadowquad[3].maskv1 = shadowquad[3].maskv2 = shadowquad[3].v = 1.0f;
	shadowquad[3].x = pBoxQuad[3].x + flPaddingHor + flHorOffset + flSpreadDistance;
	shadowquad[3].y = pBoxQuad[3].y + flPaddingVer + flVerOffset + flSpreadDistance;
	shadowquad[3].z = pBoxQuad[3].z;

	// Find shader resource view for opacity mask 
	ID3D10ShaderResourceView *pClipMask = NULL;

	if ( !bFill )
	{
		float *pflCornerRadii = pSourceLayer->AccessCornerRadii();
		bool bHasRounding = false;
		for( int i=0; i < 8; ++i )
		{
			if ( pflCornerRadii[i] != 0.0f )
			{
				bHasRounding = true;
				break;
			}
		}

		if ( bHasRounding )
		{
			float flXInset = (pShadowLayer->GetWidth() - pSourceLayer->GetWidth() ) / 2.0f;
			float flYInset = (pShadowLayer->GetHeight() - pSourceLayer->GetHeight() ) / 2.0f;

			pClipMask = GetOpacityMaskShaderResourceViewForCornerRadii( pSourceLayer->GetWidth(), pSourceLayer->GetHeight(), flXInset, flYInset, pflCornerRadii );

			if ( !m_pflOpacityMaskOneBase )
				m_pflOpacityMaskOneBase = m_pRenderEffect->m_pEffect->GetVariableByName( "g_OpacityMaskOneBase" )->AsScalar();
			m_pflOpacityMaskOneBase->SetFloat( 1.0f );
			m_pflOpacityMaskOneOpacity->SetFloat( 1.0f );
		}
	}


	// Draw the current layer into it's parent
	DrawTexturedQuadInternal( m_pTechnique, pShadowLayer->AccessSurface(), pShadowLayer->AccessShaderResourceView(), pClipMask, NULL, NULL, NULL, NULL, shadowquad, flScale2DX, flScale2DY, flRotate2D );
	FlushCurrentVertexBuffer( m_pTechnique );

	if ( m_pflOpacityMaskOneBase )
		m_pflOpacityMaskOneBase->SetFloat( 0.0f );

	SAFE_RELEASE( pClipMask );
}


//-----------------------------------------------------------------------------
// Purpose: Called to create inset shadow layer for a given layer
//-----------------------------------------------------------------------------
CCompositionLayer *CD3D10D2DSurface::GetInsetShadowLayer( CCompositionLayer *pSourceLayer, float flHorOffset, float flVerOffset, float flBlurRadius, float flSpreadDistance, uint32 shadowColor, bool bAnimating )
{
	VPROF_BUDGET( "CD3D10D2DSurface::GetInsetShadowLayer", VPROF_BUDGETGROUP_TENFOOT );

	ShadowLayerKey_t key;
	if ( !bAnimating )
	{
		key.m_flBaseWidth = pSourceLayer->GetWidth();
		key.m_flBaseHeight = pSourceLayer->GetHeight();
		key.m_bInset = true;
		key.m_bFill = false;
		key.m_flHorOffset = flHorOffset;
		key.m_flVerOffset = flVerOffset;
		key.m_flBlurRadius = flBlurRadius;
		key.m_flSpreadDistance = flSpreadDistance;
		key.m_shadowColor = shadowColor;
		V_memcpy( key.m_rgBorderRadii, pSourceLayer->AccessCornerRadii(), sizeof( key.m_rgBorderRadii ) );

		int iMap = m_mapShadowLayers.Find( key );
		if ( iMap != m_mapShadowLayers.InvalidIndex() )
		{
			ShadowLayer_t &layer = m_mapShadowLayers[iMap];

			m_listShadowLayerLRU.LinkToTail( layer.m_iLRUIndex );
			layer.m_iLRUIndex = m_listShadowLayerLRU.Tail();
			m_listShadowLayerLRU[m_listShadowLayerLRU.Tail()].m_flLastUseTime = m_flCurrentRenderFrameTime;

			return layer.m_pLayer;
		}
	}

	CCompositionLayer *pShadowOutLayer = NULL;
	if ( shadowColor != 0x00000000 )
	{
		float *pflOuterRaddi = pSourceLayer->AccessCornerRadii();
		float *pflBorderWidths = pSourceLayer->AccessBorderWidths();

		float flPadding = ceil(flBlurRadius);
		float flWidth = ceil( flPadding*2.0f+pSourceLayer->GetWidth()-pflBorderWidths[1]-pflBorderWidths[3] );
		float flHeight = ceil( flPadding*2.0f+pSourceLayer->GetHeight()-pflBorderWidths[0]-pflBorderWidths[2] );

		float flHalfSpread = flSpreadDistance/2.0f;
		float rgInnerRaddi[8];
		rgInnerRaddi[0] = MAX( pflOuterRaddi[0] - pflBorderWidths[3] - flHalfSpread, 0.0f );
		rgInnerRaddi[1] =  MAX( pflOuterRaddi[1] - pflBorderWidths[0] - flHalfSpread, 0.0f );
		rgInnerRaddi[2] = MAX( pflOuterRaddi[2] - pflBorderWidths[1] - flHalfSpread, 0.0f );
		rgInnerRaddi[3] =  MAX( pflOuterRaddi[3] - pflBorderWidths[0] - flHalfSpread, 0.0f );
		rgInnerRaddi[4] = MAX( pflOuterRaddi[4] - pflBorderWidths[1] - flHalfSpread, 0.0f );
		rgInnerRaddi[5] =  MAX( pflOuterRaddi[5] - pflBorderWidths[2] - flHalfSpread, 0.0f );
		rgInnerRaddi[6] = MAX( pflOuterRaddi[6] - pflBorderWidths[3] - flHalfSpread, 0.0f );
		rgInnerRaddi[7] =  MAX( pflOuterRaddi[7] - pflBorderWidths[2] - flHalfSpread, 0.0f );

		// Note: Composition layer will ceil() width/height, that's ok, we'll just draw slightly stretched, but when we draw into
		// the parent context we'll squish back down appropriately, which will result in pretty good linearly interpolated results for
		// sub pixel shadow boundaries.
		CCompositionLayer *pHorizontalBlurLayer = NULL;
		{
			CCompositionLayer search( this, NULL, NULL, NULL, NULL, flWidth, flHeight );
			pShadowOutLayer = GetCompositionLayer( search );
			pHorizontalBlurLayer = GetCompositionLayer( search );
		}

		if ( !pShadowOutLayer || !pHorizontalBlurLayer )
		{
			SAFE_LAYER_DELETE( pShadowOutLayer );
			SAFE_LAYER_DELETE( pHorizontalBlurLayer );

			return NULL;
		}

		// Draw the unblurred shadow 
		pShadowOutLayer->DrawInsetShadowIntoLayer( this, flPadding, flWidth, flHeight, flHorOffset, flVerOffset, flSpreadDistance, shadowColor, rgInnerRaddi );

		m_stackCompositionLayers.AddToTail( pShadowOutLayer );

		if( flBlurRadius > 0.0f )
		{
			// The shadow layer now contains the correctly sized and shaped shadow, blur needs to be applied before it is drawn into parent layer.
			m_stackCompositionLayers.Remove( m_stackCompositionLayers.Count()-1 );
			m_stackCompositionLayers.AddToTail( pHorizontalBlurLayer );

			VertexTextured_t blurquad[4];
			blurquad[0].r = blurquad[0].g = blurquad[0].b = blurquad[0].a = 1.0f;
			blurquad[0].rhw = 1.0f;
			blurquad[0].masku1 = blurquad[0].masku2 = blurquad[0].u = 0.0f;
			blurquad[0].maskv1 = blurquad[0].maskv2 = blurquad[0].v = 0.0f;
			blurquad[0].x = 0.0f;
			blurquad[0].y = 0.0f;
			blurquad[0].z = 0.0f;

			blurquad[1].r = blurquad[1].g = blurquad[1].b = blurquad[1].a = 1.0f;
			blurquad[1].rhw = 1.0f;
			blurquad[1].masku1 = blurquad[1].masku2 = blurquad[1].u = 1.0f;
			blurquad[1].maskv1 = blurquad[1].maskv2 = blurquad[1].v = 0.0f;
			blurquad[1].x = pHorizontalBlurLayer->GetWidth();
			blurquad[1].y = 0.0f;
			blurquad[1].z = 0.0f;

			blurquad[2].r = blurquad[2].g = blurquad[2].b = blurquad[2].a = 1.0f;
			blurquad[2].rhw = 1.0f;
			blurquad[2].masku1 = blurquad[2].masku2 = blurquad[2].u = 0.0f;
			blurquad[2].maskv1 = blurquad[2].maskv2 = blurquad[2].v = 1.0f;
			blurquad[2].x = 0.0f;
			blurquad[2].y = pHorizontalBlurLayer->GetHeight();
			blurquad[2].z = 0.0f;

			blurquad[3].r = blurquad[3].g = blurquad[3].b = blurquad[3].a = 1.0f;
			blurquad[3].rhw = 1.0f;
			blurquad[3].masku1 = blurquad[3].masku2 = blurquad[3].u = 1.0f;
			blurquad[3].maskv1 = blurquad[3].maskv2 = blurquad[3].v = 1.0f;
			blurquad[3].x = pHorizontalBlurLayer->GetWidth();
			blurquad[3].y = pHorizontalBlurLayer->GetHeight();
			blurquad[3].z = 0.0f;

			UpdateViewPortSize();

			ID3D10RenderTargetView *pRenderTargetView = pHorizontalBlurLayer->AccessRenderTargetView();
			AccessDevice()->OMSetRenderTargets( 1, &pRenderTargetView, NULL );
			float clearcolor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			AccessDevice()->ClearRenderTargetView( pRenderTargetView, clearcolor );

			// Update viewport size for shaders
			UpdateBlurVariables( Vector2D( 1.0f, 0.0f ),  flBlurRadius/2.0f, 1.0f/pHorizontalBlurLayer->GetWidth() );

			// Draw the current layer into horizontal blur surface
			DrawTexturedQuadInternal( m_pTechniqueBlur, pShadowOutLayer->AccessSurface(), pShadowOutLayer->AccessShaderResourceView(), NULL, NULL, NULL, NULL, NULL, blurquad, 1.0f, 1.0f, 0.0f );
			FlushCurrentVertexBuffer( m_pTechniqueBlur );
			ClearShaderResourceVariables();

			// Now draw back into the original layer adding vertical blur
			UpdateBlurVariables( Vector2D( 0.0f, 1.0f), flBlurRadius/2.0f, 1.0f/pHorizontalBlurLayer->GetHeight() );

			ID3D10RenderTargetView *pRenderTargetViewLayer = pShadowOutLayer->AccessRenderTargetView();
			AccessDevice()->OMSetRenderTargets( 1, &pRenderTargetViewLayer, NULL );
			AccessDevice()->ClearRenderTargetView( pRenderTargetViewLayer, clearcolor );

			DrawTexturedQuadInternal( m_pTechniqueBlur, pHorizontalBlurLayer->AccessSurface(), pHorizontalBlurLayer->AccessShaderResourceView(), NULL, NULL, NULL, NULL, NULL, blurquad, 1.0f, 1.0f, 0.0f );
			FlushCurrentVertexBuffer( m_pTechniqueBlur );
		}

		m_stackCompositionLayers.Remove( m_stackCompositionLayers.Count()-1 );

		// finally, horizontal blur layer as well
		int iTree = m_treeFreeCompositionLayers.Insert( pHorizontalBlurLayer );
		m_treeFreeCompositionLayers[iTree]->SetLRUPos( m_listCompositionLayersLRU.AddToTail( iTree ) );

		// Add to list of cached shadow layers
		if( !bAnimating )
		{
			ShadowLayerLRU_t lru;
			ShadowLayer_t layer;
			layer.m_pLayer = pShadowOutLayer;
			int iMap = m_mapShadowLayers.Insert( key, layer );
			lru.m_iMap = iMap;
			lru.m_flLastUseTime = m_flCurrentRenderFrameTime;
			m_mapShadowLayers[iMap].m_iLRUIndex = m_listShadowLayerLRU.AddToTail( lru );
		}
	}

	// Shadow layer is returned, and must be put back in free stack by caller
	return pShadowOutLayer;
}


//-----------------------------------------------------------------------------
// Purpose: Called to draw and free a composition layer created with CreateInsetShadowLayer
//-----------------------------------------------------------------------------
void CD3D10D2DSurface::DrawInsetShadowLayer( CCompositionLayer *pShadowLayer, CCompositionLayer *pSourceLayer, float flHorOffset, float flVerOffset, float flBlurRadius, float flSpreadDistance, uint32 shadowColor )
{
	VPROF_BUDGET( "CD3D10D2DSurface::DrawInsetShadowLayer", VPROF_BUDGETGROUP_TENFOOT );

	float *pflOuterRaddi = pSourceLayer->AccessCornerRadii();
	float *pflBorderWidths = pSourceLayer->AccessBorderWidths();

	float flWidth = ceil( pSourceLayer->GetWidth()-pflBorderWidths[1]-pflBorderWidths[3] );
	float flHeight = ceil( pSourceLayer->GetHeight()-pflBorderWidths[0]-pflBorderWidths[2] );

	float rgInnerRaddi[8];
	rgInnerRaddi[0] = MAX( pflOuterRaddi[0] - pflBorderWidths[3], 0.0f );
	rgInnerRaddi[1] =  MAX( pflOuterRaddi[1] - pflBorderWidths[0], 0.0f );
	rgInnerRaddi[2] = MAX( pflOuterRaddi[2] - pflBorderWidths[1], 0.0f );
	rgInnerRaddi[3] =  MAX( pflOuterRaddi[3] - pflBorderWidths[0], 0.0f );
	rgInnerRaddi[4] = MAX( pflOuterRaddi[4] - pflBorderWidths[1], 0.0f );
	rgInnerRaddi[5] =  MAX( pflOuterRaddi[5] - pflBorderWidths[2], 0.0f );
	rgInnerRaddi[6] = MAX( pflOuterRaddi[6] - pflBorderWidths[3], 0.0f );
	rgInnerRaddi[7] =  MAX( pflOuterRaddi[7] - pflBorderWidths[2], 0.0f );

	VertexTextured_t *pBoxQuad = pSourceLayer->AccessRenderQuad();
	VertexTextured_t shadowquad[4];

	float flScale2DX, flScale2DY;
	pSourceLayer->Get2DScaleFactors( flScale2DX, flScale2DY );
	float flRotate2D;
	pSourceLayer->Get2DRotate( flRotate2D );
	float flPadding = ceil(flBlurRadius);
	float flUAdjustment = flPadding / pShadowLayer->GetWidth();
	float flVAdjustment = flPadding / pShadowLayer->GetHeight();

	shadowquad[0].r = shadowquad[0].g = shadowquad[0].b = shadowquad[0].a = 1.0f*pBoxQuad[0].a;
	shadowquad[0].rhw = 1.0f;
	shadowquad[0].u = flUAdjustment;
	shadowquad[0].v = flVAdjustment;
	shadowquad[0].masku1 = shadowquad[0].masku2 = 0.0f;
	shadowquad[0].maskv1 = shadowquad[0].maskv2 = 0.0f;
	shadowquad[0].x = (pBoxQuad[0].x + pflBorderWidths[3]);
	shadowquad[0].y = (pBoxQuad[0].y + pflBorderWidths[0]);
	shadowquad[0].z = pBoxQuad[0].z;

	shadowquad[1].r = shadowquad[1].g = shadowquad[1].b = shadowquad[1].a = 1.0f*pBoxQuad[1].a;
	shadowquad[1].rhw = 1.0f;
	shadowquad[1].u = 1.0f-flUAdjustment;
	shadowquad[1].v = flVAdjustment;
	shadowquad[1].masku1 = shadowquad[1].masku2 = 1.0f;
	shadowquad[1].maskv1 = shadowquad[1].maskv2 = 0.0f;
	shadowquad[1].x = (pBoxQuad[1].x - pflBorderWidths[1]);
	shadowquad[1].y = (pBoxQuad[1].y + pflBorderWidths[0]);
	shadowquad[1].z = pBoxQuad[1].z;

	shadowquad[2].r = shadowquad[2].g = shadowquad[2].b = shadowquad[2].a = 1.0f*pBoxQuad[2].a;
	shadowquad[2].rhw = 1.0f;
	shadowquad[2].u = flUAdjustment;
	shadowquad[2].v = 1.0f-flVAdjustment;
	shadowquad[2].masku1 = shadowquad[2].masku2 = 0.0f;
	shadowquad[2].maskv1 = shadowquad[2].maskv2 = 1.0f;
	shadowquad[2].x = (pBoxQuad[2].x + pflBorderWidths[3]);
	shadowquad[2].y = (pBoxQuad[2].y - pflBorderWidths[2]);
	shadowquad[2].z = pBoxQuad[2].z;

	shadowquad[3].r = shadowquad[3].g = shadowquad[3].b = shadowquad[3].a = 1.0f*pBoxQuad[3].a;
	shadowquad[3].rhw = 1.0f;
	shadowquad[3].u = 1.0f-flUAdjustment;
	shadowquad[3].v = 1.0f-flVAdjustment;
	shadowquad[3].masku1 = shadowquad[3].masku2 = 1.0f;
	shadowquad[3].maskv1 = shadowquad[3].maskv2 = 1.0f;
	shadowquad[3].x = (pBoxQuad[3].x - pflBorderWidths[1]);
	shadowquad[3].y = (pBoxQuad[3].y - pflBorderWidths[2]);
	shadowquad[3].z = pBoxQuad[3].z;

	// Find shader resource view for rounded corners mask 
	ID3D10ShaderResourceView *pMaskBorderEdge = GetOpacityMaskShaderResourceViewForCornerRadii( flWidth, flHeight, 0.0f, 0.0f, rgInnerRaddi );

	// Draw the current layer into it's parent
	DrawTexturedQuadInternal( m_pTechnique, pShadowLayer->AccessSurface(), pShadowLayer->AccessShaderResourceView(), pMaskBorderEdge, NULL, NULL, NULL, NULL, shadowquad, flScale2DX, flScale2DY, flRotate2D );
	FlushCurrentVertexBuffer( m_pTechnique );

	SAFE_RELEASE( pMaskBorderEdge );
}


//-----------------------------------------------------------------------------
// Purpose: Called to clear contents of current compositing layer
//-----------------------------------------------------------------------------
void CD3D10D2DSurface::ClearCompositingLayer( CCompositionLayer *pLayer )
{
	VPROF_BUDGET( "CD3D10D2DSurface::ClearCompositingLayer", VPROF_BUDGETGROUP_TENFOOT );

	// Bind the render target view and depth stencil buffer to the output render pipeline.
	ID3D10RenderTargetView *pRenderTargetView = pLayer->AccessRenderTargetView();
	AccessDevice()->OMSetRenderTargets( 1, &pRenderTargetView, NULL );

	// Update viewport size for shaders
	UpdateViewPortSize();

	float color[4] = { 0.0, 0.0, 0.0, 0.0 };
	AccessDevice()->ClearRenderTargetView( pRenderTargetView, color );

	// Begin draw for layer
	D2D1_COLOR_F colord2d;
	colord2d.r = colord2d.g = colord2d.b = colord2d.a = 0.0;

	if ( m_stackCompositionLayers.Count() == 1 )
		pLayer->PushCliplayersAndBeginDraw( m_flScaleBackbufferX, m_flScaleBackbufferY, m_flTranslateBackbufferX, m_flTranslateBackbufferY );
	else
		pLayer->PushCliplayersAndBeginDraw( 1.0f, 1.0f, 0.0f, 0.0f );

	pLayer->AccessRenderTarget()->Clear( &colord2d );

	Assert( pLayer->GetClipLayerCount() == 0 );
}

//-----------------------------------------------------------------------------
// Purpose: Called to pop a compositing layer
//-----------------------------------------------------------------------------
void CD3D10D2DSurface::PopCompositingLayer( const CRenderMsg<CMsgPopCompositingLayer> &renderCommand )
{
	VPROF_BUDGET( "CD3D10D2DSurface::PopCompositingLayer", VPROF_BUDGETGROUP_TENFOOT );
	if ( m_stackCompositionLayers.Count() < 2 )
	{
		AssertMsg( false, "CD3D10D2DSurface::PopCompositingLayer hit with no layers, mismatched push/pop?" );
		return;
	}

	// End draw for layer
	CCompositionLayer *pLayer = m_stackCompositionLayers[m_stackCompositionLayers.Count()-1];

	bool bLayerRedraw = pLayer->BIsDrawing();
	if ( bLayerRedraw )
	{
		pLayer->DrawBorder( this );
	}
		
	if ( bLayerRedraw )
		pLayer->PopClipLayersAndFlush();
	
	Assert( pLayer->GetClipLayerCount() == 0 );

	m_pRenderEffect->m_pCurrentTexture2D = NULL;
	m_pDiffuseTex->SetResource( NULL );
	m_pOpacityMaskTex->SetResource( NULL );
	m_pOpacityMaskTexTwo->SetResource( NULL );

	bool bInset;
	bool bFill;
	float flHorOffset;
	float flVerOffset;
	float flBlurRadius;
	float flSpreadDistance;
	uint32 shadowColor;
	bool bAnimatingBoxShadow;
	pLayer->GetBoxShadow( bInset, bFill, flHorOffset, flVerOffset, flBlurRadius, flSpreadDistance, shadowColor, bAnimatingBoxShadow );

	// If we have an outer box-shadow, need to create a layer for that first
	CCompositionLayer *pShadowOutLayer = NULL;
	CCompositionLayer *pShadowInsetLayer = NULL;

	bool bTransparent = ((shadowColor>>24)&0xff) == 0 ? true : false;

	if ( bInset == false && !bTransparent )
		pShadowOutLayer = GetOuterShadowLayer( pLayer, bFill, flHorOffset, flVerOffset, flBlurRadius, flSpreadDistance, shadowColor, bAnimatingBoxShadow );
	else if ( bInset == true && !bTransparent )
		pShadowInsetLayer = GetInsetShadowLayer( pLayer, flHorOffset, flVerOffset, flBlurRadius, flSpreadDistance, shadowColor, bAnimatingBoxShadow );

	float flBlurPasses = 0.0f;
	float flBlurStdDevHor = 0.0f;
	float flBlurStdDevVer = 0.0f;
	pLayer->GetBlurValues( flBlurPasses, flBlurStdDevHor , flBlurStdDevVer );

	// If we have blur (and we redrew), then draw into another layer for blur first
	if ( bLayerRedraw && flBlurPasses > 0.0f && ( flBlurStdDevHor > 0.0f || flBlurStdDevVer > 0.0f ) )
	{
		VPROF_BUDGET( "CD3D10D2DSurface::PopCompositingLayer - Draw Blur", VPROF_BUDGETGROUP_TENFOOT );
		CCompositionLayer search( this, NULL, NULL, NULL, NULL, pLayer->GetWidth(), pLayer->GetHeight() );
		CCompositionLayer *pHorizontalBlurLayer = GetCompositionLayer( search );
		if ( pHorizontalBlurLayer )
		{
			m_stackCompositionLayers.AddToTail( pHorizontalBlurLayer );

			VertexTextured_t quad[4];
			quad[0].r = quad[0].g = quad[0].b = quad[0].a = 1.0f;
			quad[0].rhw = 1.0f;
			quad[0].u = 0.0f;
			quad[0].v = 0.0f;
			quad[0].x = 0.0f;
			quad[0].y = 0.0f;
			quad[0].z = 0.0f;

			quad[1].r = quad[1].g = quad[1].b = quad[1].a = 1.0f;
			quad[1].rhw = 1.0f;
			quad[1].u = 1.0f;
			quad[1].v = 0.0f;
			quad[1].x = pLayer->GetWidth();
			quad[1].y = 0.0f;
			quad[1].z = 0.0f;

			quad[2].r = quad[2].g = quad[2].b = quad[2].a = 1.0f;
			quad[2].rhw = 1.0f;
			quad[2].u = 0.0f;
			quad[2].v = 1.0f;
			quad[2].x = 0.0f;
			quad[2].y = pLayer->GetHeight();
			quad[2].z = 0.0f;

			quad[3].r = quad[3].g = quad[3].b = quad[3].a = 1.0f;
			quad[3].rhw = 1.0f;
			quad[3].u = 1.0f;
			quad[3].v = 1.0f;
			quad[3].x = pLayer->GetWidth();
			quad[3].y = pLayer->GetHeight();
			quad[3].z = 0.0f;

			float clearcolor[4] = { 0.0, 0.0, 0.0, 0.0 };

			float flMatrixIdentity[16] = { 1.0f, 0.0f, 0.0f, 0.0f,
				0.0f, 1.0f, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f, 0.0f,
				0.0f, 0.0f, 0.0f, 1.0f };

			if ( !m_pmatTransform )
				m_pmatTransform = m_pRenderEffect->m_pEffect->GetVariableByName( "g_MatTransform" );

			m_pmatTransform->AsMatrix()->SetMatrix( flMatrixIdentity );

			UpdateViewPortSize();

			for( float i = 0.0f; i < flBlurPasses; i += 1.0f )
			{
				ClearShaderResourceVariables();

				ID3D10RenderTargetView *pRenderTargetView = pHorizontalBlurLayer->AccessRenderTargetView();
				AccessDevice()->OMSetRenderTargets( 1, &pRenderTargetView, NULL );
				AccessDevice()->ClearRenderTargetView( pRenderTargetView, clearcolor );

				// Update viewport size for shaders
				UpdateBlurVariables( Vector2D( 1.0, 0.0 ), flBlurStdDevHor, 1.0f/pLayer->GetWidth() );

				// Draw the current layer into horizontal blur surface
				DrawTexturedQuadInternal( m_pTechniqueBlur, pLayer->AccessSurface(), pLayer->AccessShaderResourceView(), NULL, NULL, NULL, NULL, NULL, quad, 1.0f, 1.0f, 0.0f );
				FlushCurrentVertexBuffer( m_pTechniqueBlur );

				ClearShaderResourceVariables();

				// Now draw back into the original layer adding vertical blur
				UpdateBlurVariables( Vector2D( 0.0f, 1.0f), flBlurStdDevVer, 1.0f/pLayer->GetHeight() );

				ID3D10RenderTargetView *pRenderTargetViewLayer = pLayer->AccessRenderTargetView();
				AccessDevice()->OMSetRenderTargets( 1, &pRenderTargetViewLayer, NULL );
				AccessDevice()->ClearRenderTargetView( pRenderTargetViewLayer, clearcolor );

				DrawTexturedQuadInternal( m_pTechniqueBlur, pHorizontalBlurLayer->AccessSurface(), pHorizontalBlurLayer->AccessShaderResourceView(), NULL, NULL, NULL, NULL, NULL, quad, 1.0f, 1.0f, 0.0f );
				FlushCurrentVertexBuffer( m_pTechniqueBlur );
			}

			// Add blur layer back to free layers list
			m_stackCompositionLayers.Remove( m_stackCompositionLayers.Count()-1 );

			int iTree = m_treeFreeCompositionLayers.Insert( pHorizontalBlurLayer );
			m_treeFreeCompositionLayers[iTree]->SetLRUPos( m_listCompositionLayersLRU.AddToTail( iTree ) );
		}
	}

	CCompositionLayer *pParent = m_stackCompositionLayers[m_stackCompositionLayers.Count()-2];
	ID3D10RenderTargetView *pParentRenderTargetView = pParent->AccessRenderTargetView();
	AccessDevice()->OMSetRenderTargets( 1, &pParentRenderTargetView, NULL );

	// Pop it off the stack and put it in the free list
	{
		AUTO_LOCK( m_MutexReservedLayers );
		int iMap = m_mapReservedCompositionLayers.Find( pLayer->GetContextID() );
		if ( iMap == m_mapReservedCompositionLayers.InvalidIndex() )
		{
			ReservedLayer_t layer;
			layer.m_pLayer = pLayer;
			layer.m_iLRUIndex = -1;
			iMap = m_mapReservedCompositionLayers.Insert( pLayer->GetContextID(), layer );

			ReservedLayerLRU_t lru;
			lru.m_iMap = iMap;
			lru.m_flLastUseTime = m_flCurrentRenderFrameTime;
			m_mapReservedCompositionLayers[iMap].m_iLRUIndex = m_listReservedCompositionLayerLRU.AddToTail( lru );
		}
		else
		{
	#ifdef _DEBUG
			Assert( m_mapReservedCompositionLayers[iMap].m_pLayer == pLayer );
	#endif
		}
	}
	m_stackCompositionLayers.Remove( m_stackCompositionLayers.Count()-1 );

	// Update viewport size for shaders
	UpdateViewPortSize();

	float flMatrixIdentity[16] = { 1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f };

	if ( !m_pmatTransform )
	{
		m_pmatTransform = m_pRenderEffect->m_pEffect->GetVariableByName( "g_MatTransform" );
	}

	if( BBackBufferScalingNeeded() && m_stackCompositionLayers.Count() == 1 )
	{
		float rgMatrix[16];
		V_memcpy( rgMatrix, pLayer->AccessMatrix(), sizeof( rgMatrix ) );

		if( fabs( rgMatrix[12] - 0.0f ) > 0.0001f )
		{
			rgMatrix[12] *= m_flScaleBackbufferX;
		}

		if( fabs( rgMatrix[13] - 0.0f ) > 0.0001f )
		{
			rgMatrix[13] *= m_flScaleBackbufferY;
		}

		m_pmatTransform->AsMatrix()->SetMatrix( rgMatrix );
	}
	else
	{
		m_pmatTransform->AsMatrix()->SetMatrix( pLayer->AccessMatrix() );
	}

	GetAndSetColorCorrectionShaderVarDefaults();

	float flScale2DX, flScale2DY;
	pLayer->Get2DScaleFactors( flScale2DX, flScale2DY );

	float flRotate2D;
	pLayer->Get2DRotate( flRotate2D );

	// If we had a shadow layer, then it's time to draw it now
	if ( pShadowOutLayer )
	{
		DrawOuterShadowLayer( pShadowOutLayer, pLayer, bFill, flHorOffset, flVerOffset, flBlurRadius, flSpreadDistance, shadowColor );

		if ( bAnimatingBoxShadow )
		{
			ClearShaderResourceVariables();
			int iTree = m_treeFreeCompositionLayers.Insert( pShadowOutLayer );
			m_treeFreeCompositionLayers[iTree]->SetLRUPos( m_listCompositionLayersLRU.AddToTail( iTree ) );
		}
	}

	m_pflSaturation->SetFloat( pLayer->GetSaturation() );
	m_pflHueShift->SetFloat( pLayer->GetHueShift() );
	m_pflContrast->SetFloat( pLayer->GetContrast() );
	m_pflBrightness->SetFloat( pLayer->GetBrightness() );

	// Find shader resource view for opacity mask 
	LockedOpacityMaskTextureShaderResourceView_t opacityMaskResourceView = GetOpacityMaskShaderResourceViewForTexture( pLayer->GetOpacityMaskTextureID() );
	bool bChangedOpacityMaskOpacity = false;

	if ( pLayer->GetOpacityMaskOpacity() < 1.0f && pLayer->GetOpacityMaskTextureID() != 0 )
	{
		m_pflOpacityMaskOneOpacity->SetFloat( pLayer->GetOpacityMaskOpacity() );
		bChangedOpacityMaskOpacity = true;
	}

	// Find shader resource view for rounded corners mask 
	ID3D10ShaderResourceView *pOpacityMaskTwoResourceView = GetOpacityMaskShaderResourceViewForCornerRadii( pLayer->GetWidth(), pLayer->GetHeight(), 0.0f, 0.0f, pLayer->AccessCornerRadii() );

	// Draw the current layer into it's parent
	DrawTexturedQuadInternal( m_pTechnique, pLayer->AccessSurface(), pLayer->AccessShaderResourceView(), opacityMaskResourceView.m_pShaderResource, pOpacityMaskTwoResourceView, NULL, NULL, NULL, pLayer->AccessRenderQuad(), flScale2DX, flScale2DY, flRotate2D );
	FlushCurrentVertexBuffer( m_pTechnique );

	if ( bChangedOpacityMaskOpacity )
		m_pflOpacityMaskOneOpacity->SetFloat( 1.0f );

	ReleaseLockedOpacityMaskTextureShaderResourceView( opacityMaskResourceView );
	SAFE_RELEASE( pOpacityMaskTwoResourceView );

	// If we had an inset shadow, then it's time to draw it now
	if ( pShadowInsetLayer )
	{
		DrawInsetShadowLayer( pShadowInsetLayer, pLayer, flHorOffset, flVerOffset, flBlurRadius, flSpreadDistance, shadowColor );

		if ( bAnimatingBoxShadow )
		{
			SAFE_LAYER_DELETE( pShadowInsetLayer );
		}
	}

	m_pmatTransform->AsMatrix()->SetMatrix( flMatrixIdentity );

	PopPerfEvent();

	if ( m_stackCompositionLayers.Count() == 0 )
	{
		Assert( m_stackCompositionLayers.Count() != 0 );
	}
	else
	{
		if ( m_stackCompositionLayers.Count() == 1 )
			pParent->PushCliplayersAndBeginDraw( m_flScaleBackbufferX, m_flScaleBackbufferY, m_flTranslateBackbufferX, m_flTranslateBackbufferY );
		else
			pParent->PushCliplayersAndBeginDraw( 1.0f, 1.0f, 0.0f, 0.0f );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called to push a clipping layer
//-----------------------------------------------------------------------------
void CD3D10D2DSurface::PushClipLayer( const CRenderMsg<CMsgPushClipLayer> &renderCommand )
{
	VPROF_BUDGET( "CD3D10D2DSurface::PushClipLayer", VPROF_BUDGETGROUP_TENFOOT );
	if ( m_stackCompositionLayers.Count() == 0 )
	{
		Assert( m_stackCompositionLayers.Count() != 0 );
	}
	else
	{
		CCompositionLayer *pLayer = m_stackCompositionLayers[ m_stackCompositionLayers.Count()-1 ];
		pLayer->PushClipLayer( renderCommand.BodyConst() );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called to pop a clipping layer
//-----------------------------------------------------------------------------
void CD3D10D2DSurface::PopClipLayer( const CRenderMsg<CMsgPopClipLayer> &renderCommand )
{
	VPROF_BUDGET( "CD3D10D2DSurface::PopClipLayer", VPROF_BUDGETGROUP_TENFOOT );
	if ( m_stackCompositionLayers.Count() == 0 )
	{
		Assert( m_stackCompositionLayers.Count() != 0 );
	}
	else
	{
		CCompositionLayer *pLayer = m_stackCompositionLayers[ m_stackCompositionLayers.Count()-1 ];
		pLayer->PopClipLayer();
	}
}

#ifdef DBGFLAG_VALIDATE

//-----------------------------------------------------------------------------
// Purpose: validate mem
//-----------------------------------------------------------------------------
void CCompositionLayer::Validate( CValidator &validator, const tchar *pchName )
{
	VALIDATE_SCOPE();

	ValidatePtr( m_pVecClipLayers )
}


//-----------------------------------------------------------------------------
// Purpose: validate mem
//-----------------------------------------------------------------------------
void CD3D10DoubleBufferedYUV420Texture::Validate( CValidator &validator, const tchar *pchName )
{
	VALIDATE_SCOPE();
}


//-----------------------------------------------------------------------------
// Purpose: validate mem
//-----------------------------------------------------------------------------
void CD3D10DoubleBufferedTexture::Validate( CValidator &validator, const tchar *pchName )
{
	VALIDATE_SCOPE();
}


//-----------------------------------------------------------------------------
// Purpose: validate mem
//-----------------------------------------------------------------------------
void CD3D10Texture::Validate( CValidator &validator, const tchar *pchName )
{
	VALIDATE_SCOPE();
}


//-----------------------------------------------------------------------------
// Purpose: validate mem
//-----------------------------------------------------------------------------
void CD3D10D2DSurface::Validate( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE();

	ValidateObj( m_bufTempScanline );
	ValidateObj( m_vecClipLayers );
	validator.ClaimMemory( m_pRenderEffect );
	ValidateObj( m_mapBorderRadiusOpacityMasks );
	ValidateObj( m_listBorderRadiusOpacityMaskLRU );
	ValidateObj( m_mapSolidColorBrushes );
	ValidateObj( m_listSolidColorBrushLRU );
	ValidateObj( m_treeFreeCompositionLayers );

	// bugbug jmccaskey - validate new text stuff

	ValidateObj( m_mapLinearGradientBrushes );
	FOR_EACH_MAP( m_mapLinearGradientBrushes, i )
	{
		validator.ClaimMemory( MemAlloc_Unalign( ( m_mapLinearGradientBrushes.Key( i ) ) ) );
	}
	ValidateObj( m_listLinearGradientBrushLRU );

	ValidateObj( m_mapRadialGradientBrushes );
	ValidateObj( m_listRadialGradientBrushLRU );
	FOR_EACH_MAP_FAST( m_mapRadialGradientBrushes, i )
	{
		validator.ClaimMemory( (void*)m_mapRadialGradientBrushes.Key( i ) );
	}

	ValidateObj( m_CompositionLayerPool );
	
	// Owned by m_CompositionLayerPool, so call validate on object, but don't claim object ptr itself
	FOR_EACH_RBTREE_FAST( m_treeFreeCompositionLayers, i )
	{
		m_treeFreeCompositionLayers[i]->Validate( validator, "CCompositionLayer" );
	}

	ValidateObj( m_listCompositionLayersLRU );
	ValidateObj( m_stackCompositionLayers ); 	

	
	// Owned by m_CompositionLayerPool
	FOR_EACH_VEC( m_stackCompositionLayers, i )
	{
		m_stackCompositionLayers[i]->Validate( validator, "CCompositionLayer" );
	}

	// Owned by m_CompositionLayerPool
	// The back buffer layer is inside the stack, unless it's empty...
	if ( m_stackCompositionLayers.Count() == 0 )
		m_pCompositionLayer->Validate( validator, "CCompositionLayer" );

	{
		AUTO_LOCK( m_MutexReservedLayers );
		ValidateObj( m_listReservedCompositionLayerLRU );
		ValidateObj( m_mapReservedCompositionLayers );

		// Owned by m_CompositionLayerPool
		FOR_EACH_MAP_FAST( m_mapReservedCompositionLayers, i )
		{
			m_mapReservedCompositionLayers[i].m_pLayer->Validate( validator, "CCompositionLayer" );
		}
	}

	ValidateObj( m_listShadowLayerLRU );
	ValidateObj( m_mapShadowLayers );

	// Owned by m_CompositionLayerPool
	FOR_EACH_MAP_FAST( m_mapShadowLayers, i )
	{
		m_mapShadowLayers[i].m_pLayer->Validate( validator, "CCompositionLayer" );
	}

	ValidateObj( m_mapTextures );
	FOR_EACH_MAP_FAST( m_mapTextures, i )
	{
		ValidatePtr( dynamic_cast<CD3D10Texture *>(m_mapTextures[i]) );
	}

	ValidatePtr( m_pTextRenderer );

	m_tsQueueTextureDeletes.ValidateDataStructureOnly( validator, "m_tsQueueTextureDeletes" );
}
#endif


//-----------------------------------------------------------------------------
// Purpose: Determine if we need a mode switch or state update of some kind and do it
//-----------------------------------------------------------------------------
bool CD3D10D2DSurface::BUpdateRenderStateIfNeeded( IUIEngine::ERenderTarget eMsgRenderTarget )
{
	if ( IUIEngine::BValidRenderStateChange( GetRenderTarget(), eMsgRenderTarget ) )
	{
		m_eRenderTarget = eMsgRenderTarget;
		g_nInFullscreenSwitch++;
		BOOL bSetFullScreen = FALSE;
		if ( IUIEngine::BIsRenderingToFullScreen( m_eRenderTarget ) )
			bSetFullScreen = TRUE;

		m_pDXGISwapChain->SetFullscreenState( bSetFullScreen, NULL );
		g_nInFullscreenSwitch--;
		ReleaseDXResources();
		DbgVerify( BRecreateSizedD3DResources() );
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Determine if we need to resize the backbuffer and do it
//-----------------------------------------------------------------------------
bool CD3D10D2DSurface::BUpdateWindowSizeIfNeeded( uint32 nWidth, uint32 nHeight )
{
	if ( nHeight != m_unWindowHeight || nWidth != m_unWindowWidth )
	{
		// Sizes outside these bounds are not valid for render target surfaces and will fail later, catch bugs with bad
		// values early when set by asserting here
		Assert( nWidth >= 1 && nWidth <= 8192 );
		Assert( nHeight >= 1 && nHeight <= 8192 );
		m_unWindowWidth = nWidth;
		m_unWindowHeight = nHeight;
		if ( !m_bFixedSurfaceSize )
		{
			m_unSurfaceWidth = nWidth;
			m_unSurfaceHeight = nHeight;
		}
		ReleaseDXResources();
		DbgVerify( BRecreateSizedD3DResources() );
		return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Present if we have a working swap chain
//-----------------------------------------------------------------------------
void CD3D10D2DSurface::PresentBackBuffer()
{
	HRESULT hRes = S_OK;
	if ( !IUIEngine::BIsRenderingToTexture( m_eRenderTarget ) )
	{
		UINT flags = 0;
		if ( m_bSurfaceOccluded )
			flags = DXGI_PRESENT_TEST;

#ifdef PANORAMA_STEAMUI_STREAMING_CAPTURE_WIN32
		// Call back into the SteamUI in-home streaming system, give it a chance
		// to do NV12 conversion and read the NV12 texture back asynchronously.
		SteamUIStreamingCaptureCallback_t pSteamUIStreamingCallback = m_pSteamUIStreamingCallback;
		if ( !m_bSurfaceOccluded && pSteamUIStreamingCallback )
		{
			pSteamUIStreamingCallback( m_pD3D10Device, static_cast<IUnknown*>( m_pBackBuffer ) );
		}
#endif

		if ( m_bVsyncEnabled )
		{
			VPROF_BUDGET( "DXGI::Present - vsync sleep", VPROF_BUDGETGROUP_TENFOOT );
			hRes = m_pDXGISwapChain->Present( 1, flags );
		}
		else
		{
			VPROF_BUDGET( "DXGI::Present", VPROF_BUDGETGROUP_TENFOOT );
			hRes = m_pDXGISwapChain->Present( 0, flags );
		}

		if ( hRes == DXGI_STATUS_OCCLUDED )
		{
			m_bSurfaceOccluded = true;
		}
	}
	else if ( m_eRenderTarget == IUIEngine::k_ERenderToOpenVROverlay && m_ulVROverlayHandle != vr::k_ulOverlayHandleInvalid )
	{
		//vr::Texture_t texture = { m_pBackBuffer, vr::API_DirectX, vr::ColorSpace_Gamma };
		vr::Texture_t texture = { m_pBackBuffer, vr::API_DirectX, vr::ColorSpace_Auto };
		vrapi::VROverlay()->SetOverlayTexture( m_ulVROverlayHandle, &texture );
		m_pD3D10Device->Flush();
	}
	else
	{
		if ( m_dwTargetOverlayPID == 0 )
			return;

		if ( m_nOverlayTextureID == 0 )
			m_nOverlayTextureID = ++s_unNextOverlayTextureID;

		if ( !m_pBackBufferSharedMemStream )
		{
			m_pBackBufferSharedMemStream = new CSharedMemStream( CFmtStr1024( "GameOverlayRender_SharedTex_%d_%d", m_nOverlayTextureID, m_dwTargetOverlayPID ).Access(), SHMEMSTREAM_SIZE_ONE_MBYTE * 16, 200 );
		}

		if ( !m_pBackBufferSharedMemEvent )
		{
			m_pBackBufferSharedMemEvent = IPC::CreateEvent( CFmtStr1024( "GameOverlayRender_SharedTexRead_%d_%d", m_nOverlayTextureID, m_dwTargetOverlayPID ).Access(), false, true, NULL );
		}

		if( !m_pBackBufferSharedMemWriteEvent )
		{
			m_pBackBufferSharedMemWriteEvent = IPC::CreateEvent( CFmtStr1024( "GameOverlayRender_SharedTexWrite_%d_%d", m_nOverlayTextureID, m_dwTargetOverlayPID ).Access(), false, false, NULL );
		}
	
		D3D10_TEXTURE2D_DESC descBackBuffer;
		m_pBackBuffer->GetDesc( &descBackBuffer );
		if ( m_bRenderSharedSurface == false )
		{
			// We aren't drawing, so clear the stream and set the event that the game read so we 
			// won't get blocked when we resume drawing and the game won't do useless work now
			m_pBackBufferSharedMemStream->Clear();
			m_pBackBufferSharedMemEvent->SetEvent();
			return;
		}

		// If there is already a texture 
		if( !m_pBackBufferSharedMemEvent->Wait( 300 ) )
		{
			m_unBackBufferSharedMenEventFails++;

			// Just set the event ourselves so we'll unblock next frame, the game may not have consumed the frame and may even
			// then get a partial/corrupt texture but we can't block forever.
			if( m_unBackBufferSharedMenEventFails > 4 )
				m_pBackBufferSharedMemEvent->SetEvent();
			return;
		}

		m_unBackBufferSharedMenEventFails = 0;

		m_pBackBufferSharedMemEvent->ResetEvent();

		if( m_eRenderTarget == IUIEngine::k_ERenderToOverlaySharedTexture )
		{
			if( !m_pSharedTexCopy )
			{
				VPROF_BUDGET( "Create GPU shared texture", VPROF_BUDGETGROUP_TENFOOT );

				D3D10_TEXTURE2D_DESC desc;
				ZeroMemory( &desc, sizeof(desc) );
				desc.Width = descBackBuffer.Width;
				desc.Height = descBackBuffer.Height;
				desc.MipLevels = 1;
				desc.ArraySize = 1;
				desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
				desc.SampleDesc.Count = 1;
				desc.Usage = D3D10_USAGE_DEFAULT;
				desc.MiscFlags = D3D10_RESOURCE_MISC_SHARED; // KEYED_MUTEX for 10/11?
				desc.BindFlags = D3D10_BIND_SHADER_RESOURCE | D3D10_BIND_RENDER_TARGET;
				desc.CPUAccessFlags = 0;

				HRESULT hRes = AccessDevice()->CreateTexture2D( &desc, NULL, &m_pSharedTexCopy );
				if( SUCCEEDED( hRes ) )
				{
					IDXGIResource* pDXGIResource = NULL;
					m_pSharedTexCopy->QueryInterface( __uuidof(IDXGIResource), (LPVOID*)&pDXGIResource );

					pDXGIResource->GetSharedHandle( &m_hSharedText );
					pDXGIResource->Release();
				}
			}


			m_pD3D10Device->Flush();
			D3D10_QUERY_DESC qdesc;
			qdesc.Query = D3D10_QUERY_EVENT;
			qdesc.MiscFlags = 0;

			ID3D10Query *pQuery = NULL;
			m_pD3D10Device->CreateQuery( &qdesc, &pQuery );
			pQuery->Begin();
			AccessDevice()->CopyResource( m_pSharedTexCopy, m_pBackBuffer );
			pQuery->End();

		
			while( S_OK != pQuery->GetData( NULL, 0, 0 ) )
			{
				ThreadSleep( 1 );
			}

			pQuery->Release();
			
			RenderUpdateSharedTextureHeader_t header;

			header.m_unMagic = k_unUpdateSharedTextureMagicNumberSharedHandle;
			header.m_unHeight = m_unWindowHeight;
			header.m_unWidth = m_unWindowWidth;
			header.m_ulSharedTexHandle = (uint64)m_hSharedText;
			header.m_format = (ETextureFormat)m_eOverlayTextureFormat;
			m_pBackBufferSharedMemStream->Put( &header, sizeof(RenderUpdateSharedTextureHeader_t) );

			m_pBackBufferSharedMemWriteEvent->SetEvent();
			//Msg( "Texture updated, will wait on game to update again\n" );
		}
		else
		{

			ID3D10Texture2D *pCopy = NULL;

			D3D10_TEXTURE2D_DESC desc;
			ZeroMemory( &desc, sizeof(desc) );
			desc.Width = descBackBuffer.Width;
			desc.Height = descBackBuffer.Height;
			desc.MipLevels = 1;
			desc.ArraySize = 1;
			desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
			desc.SampleDesc.Count = 1;
			desc.Usage = D3D10_USAGE_STAGING;
			desc.MiscFlags = 0;
			desc.BindFlags = 0;
			desc.CPUAccessFlags = D3D10_CPU_ACCESS_READ;

			{
				VPROF_BUDGET( "Copy Texture to CPU", VPROF_BUDGETGROUP_TENFOOT );
				HRESULT hRes = AccessDevice()->CreateTexture2D( &desc, NULL, &pCopy );
				if( SUCCEEDED( hRes ) )
				{
					AccessDevice()->CopyResource( pCopy, m_pBackBuffer );

					D3D10_MAPPED_TEXTURE2D mappedTex;
					hRes = pCopy->Map( D3D10CalcSubresource( 0, 0, 1 ), D3D10_MAP_READ, 0, &mappedTex );
					if( SUCCEEDED( hRes ) )
					{
						RenderUpdateSharedTextureHeader_t header;

						header.m_unMagic = k_unUpdateSharedTextureMagicNumber;
						header.m_unHeight = m_unWindowHeight;
						header.m_unWidth = m_unWindowWidth;
						header.m_unRowPitch = m_unWindowWidth * 4;
						header.m_format = (ETextureFormat)m_eOverlayTextureFormat;

						Assert( m_eOverlayTextureFormat == k_ETextureBGRA8 || m_eOverlayTextureFormat == k_ETextureRGBA8 );

						m_pBackBufferSharedMemStream->Put( &header, sizeof(RenderUpdateSharedTextureHeader_t) );

						m_bufTempScanline.EnsureCapacity( m_unWindowWidth * 4 );

						for( uint32 y = 0; y < m_unWindowHeight; ++y )
						{
							const byte *pScanlineToPut = (byte*)mappedTex.pData + (y*mappedTex.RowPitch);
							
							// The texture we read back is always in BGRA byte order. Must convert to desired format.
							if ( m_eOverlayTextureFormat == k_ETextureRGBA8 )
							{
								uint32 *pConvert = (uint32*)m_bufTempScanline.Base();
								for ( uint i = 0; i < m_unWindowWidth*4; i += 4 )
								{
									// BGRA = 0xAARRGGBB little-endian, RGBA = 0xAABBGGRR little-endian, byteswap and rotate.
									*pConvert++ = _rotr( _byteswap_ulong( *(uint32*)( pScanlineToPut + i ) ), 8 );
								}
								pScanlineToPut = m_bufTempScanline.Base();
							}

							if ( m_pBackBufferSharedMemStream->Put( pScanlineToPut, m_unWindowWidth * 4 ) < m_unWindowWidth * 4 )
							{
								// Serious error, sending the texture data failed. Game may not be reading stream any more
								break;
							}
						}
						
						//Msg( "Put %d,%d,%d: %d, %d\n", header.m_unMagic, header.m_unWidth, header.m_unHeight, sizeof( RenderUpdateSharedTextureHeader_t ), mappedTex.RowPitch * m_unSurfaceHeight );
						pCopy->Unmap( D3D10CalcSubresource( 0, 0, 1 ) );
					}

					pCopy->Release();
				}
			}
			m_pBackBufferSharedMemWriteEvent->SetEvent();
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Should only be called on overlay render-to-texture instances, tells 
// us to push the overlay render cmd stream necessary to draw our content.  This is 
// called from the main thread outside the render thread.
//-----------------------------------------------------------------------------
void CD3D10D2DSurface::PushOverlayRenderCmdStream( CSharedMemStream *pRenderStream, DWORD dwPID, float flOpacity, uint32 unGameWidth, uint32 unGameHeight, EOverlayWindowAlignment alignment )
{
	if ( !IUIEngine::BIsRenderingToTexture( m_eRenderTarget ) )
	{
		AssertMsg( false, "Shouldn't call PushOverlayRenderCmdSteram on non-render-to-texture targets" );
		return;
	}

	if ( flOpacity <= 0.000001f )
	{
		m_bRenderSharedSurface = false;
		return;
	}
	else 
	{
		if ( !m_bRenderSharedSurface )
		{
			// return so we get an extra frame to first render
			m_bRenderSharedSurface = true;
			return;
		}
	}

	m_dwTargetOverlayPID = dwPID;

	if ( m_nOverlayTextureID == 0 )
		return;

	CUtlBuffer bufData;
	bufData.EnsureCapacity( sizeof( ESurfaceCommand ) + sizeof( RenderDrawAndUpdateSharedTexture_t ) );

	RenderDrawAndUpdateSharedTexture_t data;
	data.m_iTextureID = m_nOverlayTextureID;
	data.hTexture = 0;
	data.width = 0;
	data.height = 0;

	if ( alignment == k_EOverlayWindowAlignment_FullscreenLetterboxed || alignment == k_EOverlayWindowAlignment_FullscreenNoLetterBox )
	{
		float flScale = 1.0f;
		uint32 unWidthTarget = unGameWidth;
		if ( m_unWindowWidth != unGameWidth )
		{
			flScale = (float)unGameWidth / (float)m_unWindowWidth;
		}

		uint32 unHeightTarget = (uint32)(flScale * m_unWindowHeight);

		if ( unHeightTarget > unGameHeight )
		{
			flScale = (float)unGameHeight / (float)m_unWindowHeight;
		}

		unHeightTarget = (uint32)(flScale * m_unWindowHeight);
		unWidthTarget = (uint32)(flScale *m_unWindowWidth);

		float flXOffset = 0.0f;
		if ( unWidthTarget < unGameWidth )
			flXOffset = ( unGameWidth - unWidthTarget ) / 2.0f;

		float flYOffset = 0.0f;
		if ( unHeightTarget < unGameHeight )
			flYOffset = ( unGameHeight - unHeightTarget ) / 2.0f;

		data.m_x0 = flXOffset;
		data.m_y0 = flYOffset;
		data.m_x1 = flXOffset + unWidthTarget;
		data.m_y1 = flYOffset + unHeightTarget;

		if ( alignment == k_EOverlayWindowAlignment_FullscreenLetterboxed )
		{
			if ( flXOffset > 0.0f )
			{
				ESurfaceCommand cmd = k_EDrawTexturedRect;
				RenderDrawTexturedRect_t rData;

				rData.m_iTextureID = k_nWhiteTextureID;
				rData.m_x0 = 0;
				rData.m_y0 = 0.0f;
				rData.m_x1 = flXOffset;
				rData.m_y1 = unGameHeight;
				rData.m_u0 = 0.0f;
				rData.m_v0 = 0.0f;
				rData.m_u1 = 1.0f;
				rData.m_v1 = 1.0f;
				rData.m_colorStart = rData.m_colorEnd = DWORD_ARGB( (int)(m_cLetterBoxColor.a() * flOpacity ), m_cLetterBoxColor.r(), m_cLetterBoxColor.g(), m_cLetterBoxColor.b() );
				rData.m_EDirection = k_EOverlayGradientNone;
				rData.m_flZPos = 0.0f;

				pRenderStream->Put( &cmd, sizeof( ESurfaceCommand ) );
				pRenderStream->Put( &rData, sizeof( RenderDrawTexturedRect_t ) );

				rData.m_x0 = unGameWidth - flXOffset;
				rData.m_y0 = 0.0f;
				rData.m_x1 = unGameWidth;
				rData.m_y1 = unGameHeight;

				pRenderStream->Put( &cmd, sizeof( ESurfaceCommand ) );
				pRenderStream->Put( &rData, sizeof( RenderDrawTexturedRect_t ) );
			}

			if ( flYOffset > 0.0f )
			{
				ESurfaceCommand cmd = k_EDrawTexturedRect;
				RenderDrawTexturedRect_t rData;

				rData.m_iTextureID = k_nWhiteTextureID;
				rData.m_x0 = 0.0f;
				rData.m_y0 = 0.0f;
				rData.m_x1 = unGameWidth;
				rData.m_y1 = flYOffset;
				rData.m_u0 = 0.0f;
				rData.m_v0 = 0.0f;
				rData.m_u1 = 1.0f;
				rData.m_v1 = 1.0f;
				rData.m_colorStart = rData.m_colorEnd = DWORD_ARGB( (int)(m_cLetterBoxColor.a() * flOpacity ), m_cLetterBoxColor.r(), m_cLetterBoxColor.g(), m_cLetterBoxColor.b() );
				rData.m_EDirection = k_EOverlayGradientNone;
				rData.m_flZPos = 0.0f;

				pRenderStream->Put( &cmd, sizeof( ESurfaceCommand ) );
				pRenderStream->Put( &rData, sizeof( RenderDrawTexturedRect_t ) );

				rData.m_x0 = 0.0f;
				rData.m_y0 = unGameHeight - flYOffset;
				rData.m_x1 = unGameWidth;
				rData.m_y1 = unGameHeight;

				pRenderStream->Put( &cmd, sizeof( ESurfaceCommand ) );
				pRenderStream->Put( &rData, sizeof( RenderDrawTexturedRect_t ) );
			}
		}
	}
	else if ( alignment == k_EOverlayWindowAlignment_BottomRight )
	{
		data.m_x0 = unGameWidth - m_unWindowWidth;
		data.m_y0 = unGameHeight - m_unWindowHeight;
		data.m_x1 = data.m_x0 + m_unWindowWidth;
		data.m_y1 = data.m_y0 + m_unWindowHeight;
	}

	data.m_u0 = 0.0f;
	data.m_v0 = 0.0f;
	data.m_u1 = 1.0f;
	data.m_v1 = 1.0f;
	data.m_flZPos = 0.0f;
	data.m_flOpacity = flOpacity;
	
	ESurfaceCommand cmd = k_EDrawAndUpdateSharedTexture;
	bufData.Put( &cmd, sizeof( ESurfaceCommand ) );
	bufData.Put( &data, sizeof( RenderDrawAndUpdateSharedTexture_t ) );

	pRenderStream->Put( bufData.Base(), bufData.TellPut() );

	
}


//-----------------------------------------------------------------------------
// Purpose: Create resources that have process lifetime
//-----------------------------------------------------------------------------
bool CD3D10D2DSurface::BCreateDeviceIndependentResources()
{
	VPROF_BUDGET( "CD3D10D2DSurface::BCreateDeviceIndependentResources()", VPROF_BUDGETGROUP_TENFOOT );
	D2D1_FACTORY_OPTIONS options;
	// Sometimes the D2D debug layer leaks memory, inexplicably.  So we only turn it on when
	// explicitly wanting to very info level spew from d2d.
#if D3DDEBUGRUNTIME
	options.debugLevel = D2D1_DEBUG_LEVEL_WARNING;
#else
	options.debugLevel = D2D1_DEBUG_LEVEL_NONE;
#endif

	HRESULT hRes;
	hRes = g_D2D1CreateFactory( D2D1_FACTORY_TYPE_MULTI_THREADED, __uuidof(ID2D1Factory), &options, (void**)&m_pD2DFactory );
	if ( FAILED( hRes ) )
	{
		AssertMsg( false, "D2D1CreateFactory failed, can't create surface" );
		UIEngine()->ShowNativeTopMostMessageBox( "Error: D2D1CreateFactory failed, can't create D3D Window", "Fatal Error", IUIEngine::k_ENativeMessageOk );
		return false;
	}


	hRes = g_DWriteCreateFactory( DWRITE_FACTORY_TYPE_SHARED, __uuidof( m_pDWriteFactory ), (IUnknown**)&m_pDWriteFactory );
	if ( FAILED( hRes ) )
	{
		AssertMsg( false, "DWriteCreateFactory failed, can't create surface" );
		UIEngine()->ShowNativeTopMostMessageBox( "Error: DWriteCreateFactory failed, can't create D3D Window", "Fatal Error", IUIEngine::k_ENativeMessageOk );
		SAFE_RELEASE( m_pD2DFactory );
		return false;
	}

	IDWriteRenderingParams *pParams;
	if ( SUCCEEDED( m_pDWriteFactory->CreateRenderingParams( &pParams ) ) )
	{
		// We always want outline mode to bypass the cleartype rasterizer, this may look worse at very 
		// small font sizes, but we don't use those much in 10foot, and outline produces way better accuracy
		// with some of the not-awesome-hinted custom fonts we use.
		DWRITE_RENDERING_MODE mode = DWRITE_RENDERING_MODE_OUTLINE;
		DWRITE_PIXEL_GEOMETRY geometry = DWRITE_PIXEL_GEOMETRY_FLAT;
		geometry = pParams->GetPixelGeometry();

		if ( !SUCCEEDED( m_pDWriteFactory->CreateCustomRenderingParams(
			pParams->GetGamma(), pParams->GetEnhancedContrast(), 
			mode == DWRITE_RENDERING_MODE_DEFAULT ? pParams->GetClearTypeLevel() : 0.0f,
			geometry,
			mode, &m_pDWriteRenderingParams ) ) )
		{
			AssertFatalMsg( false, "Failed creating dwrite rendering params" );
		}

		pParams->Release();
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Create d3d device and swap chain
//-----------------------------------------------------------------------------
bool CD3D10D2DSurface::BCreateDeviceAndSwapChain()
{
	VPROF_BUDGET( "CD3D10D2DSurface::BCreateDeviceAndSwapChain()", VPROF_BUDGETGROUP_TENFOOT );
	HRESULT hRes;

	if ( !m_pD3D10Device || !m_pDXGIFactory )
	{
		// Done with the DXGI device
		SAFE_RELEASE( m_pD3D10Device );
		SAFE_RELEASE( m_pDXGIFactory );

		hRes = g_CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)(&m_pDXGIFactory)); 
		if( FAILED (hRes) ) 
		{
			AssertMsg( false, "CreateDXGIFactory1 failed, can't create surface" );
			return false;
		}

		// Check if we have IDXGIFactory2 available, which would indicate we are on Win8 or Win7 with KB2670838.
		// Those OSs/updates ship newer D3D11/D2D DLLs which have a threading bug around sharing a D3D10 device
		// with D2D.  See the changenotes for the commit for this code for more details.  We need a semi-convoluted
		// work around creating a D3D11 device and setting it's context as D3D10 to avoid crashes due to that MS shipped
		// bug.
		bool bNeedD3D11Device = false;
		IDXGIFactory2 *pDXGIFactory2 = NULL;
		hRes = m_pDXGIFactory->QueryInterface( __uuidof( IDXGIFactory2 ), (void**)&pDXGIFactory2 );
		if ( SUCCEEDED( hRes ) && pDXGIFactory2 )
		{
			bNeedD3D11Device = true;
			SAFE_RELEASE( pDXGIFactory2 );
		}

		// The 1st adapter is the desktop, but might as well be a good citizen and grab them
		// all. This might be 'weird' if the non-primary adapter supports feature level 10.0
		// but the primary does not.
		const int nMaxAdapters = 8;
		int nAdapterCount = 0;
		IDXGIAdapter1* allAdapters[nMaxAdapters];	
		for ( int i = 0; i < nMaxAdapters; i++ )
		{
			hRes = m_pDXGIFactory->EnumAdapters1( i, &allAdapters[i] );
			if ( FAILED(hRes) )
				break;

			DXGI_ADAPTER_DESC desc;
			allAdapters[i]->GetDesc( &desc );
			Msg( "Found adapter %S\n", desc.Description );

			nAdapterCount++;
		}

		if ( nAdapterCount == 0 )
		{
			AssertMsg( false, "Found zero adapters with DXGI EnumAdapters1, can't create surface" );
			SAFE_RELEASE( m_pDXGIFactory );
			return false;
		}

		UINT nDeviceFlags = D3D10_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef D3DDEBUGRUNTIME
		nDeviceFlags |= D3D10_CREATE_DEVICE_DEBUG;
#endif

		UINT nDeviceFlags11 = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef D3DDEBUGRUNTIME
		nDeviceFlags11 |= D3D11_CREATE_DEVICE_DEBUG;
#endif

		static const D3D10_DRIVER_TYPE driverType[] = 
		{
			D3D10_DRIVER_TYPE_HARDWARE,
			D3D10_DRIVER_TYPE_WARP
		};

		static const D3D10_FEATURE_LEVEL1 levelAttempts[] = 
		{
			D3D10_FEATURE_LEVEL_10_0,
			D3D10_FEATURE_LEVEL_9_3,
			D3D10_FEATURE_LEVEL_9_2,
			D3D10_FEATURE_LEVEL_9_1,
		};

		static const D3D_FEATURE_LEVEL levelAttempts11[] = 
		{
			D3D_FEATURE_LEVEL_10_0,
			D3D_FEATURE_LEVEL_9_3,
			D3D_FEATURE_LEVEL_9_2,
			D3D_FEATURE_LEVEL_9_1,
		};

		Assert( V_ARRAYSIZE( levelAttempts11 ) == V_ARRAYSIZE( levelAttempts ) );

		int nGPUVendorID = 0;

		for ( int type = 0; type < V_ARRAYSIZE( driverType ) && !m_pD3D10Device; type++ )
		{
			for ( int level = 0; level < V_ARRAYSIZE(levelAttempts) && !m_pD3D10Device; level++ )
			{
				for ( int adapter = 0; adapter < nAdapterCount && !m_pD3D10Device; adapter++ )
				{
					// Crazy workaround/hack for Win8 or Win7 with KB2670838.
					// Those OSs/updates ship newer D3D11/D2D DLLs which have a threading bug around sharing a D3D10 device
					// with D2D.  See the changenotes for the commit for this code for more details.  We need a semi-convoluted
					// work around creating a D3D11 device and setting it's context as D3D10 to avoid crashes due to that MS shipped
					// bug.
					if ( bNeedD3D11Device )
					{
						AssertMsg( g_D3D11CreateDevice != NULL, "bNeedsD3D11Device true but no g_D3D11CreateDevice, shouldn't happen!" );
						D3D_FEATURE_LEVEL levelResult;
						ID3D11Device *pDevice = NULL;
						ID3D11Device1 *pDevice1 = NULL;
						ID3D11DeviceContext *pDeviceContext = NULL;

						hRes = g_D3D11CreateDevice( allAdapters[adapter], D3D_DRIVER_TYPE_UNKNOWN, NULL,
							nDeviceFlags11, levelAttempts11+level, 1, D3D11_SDK_VERSION, &pDevice, &levelResult, &pDeviceContext );

						if ( FAILED( hRes ) )
						{
							pDevice = NULL;
						}

						SAFE_RELEASE( pDeviceContext );

						if ( pDevice )
						{
							if ( SUCCEEDED( pDevice->QueryInterface( __uuidof( ID3D11Device1 ), (void**)&pDevice1 ) ) )
							{
								// Now setup the device to act like a D3D10 device, and then point the D3D10 device ptr at it...
								ID3D10Multithread *pMultithread = NULL;
								if ( SUCCEEDED( pDevice->QueryInterface( __uuidof( ID3D10Multithread ), (void**)&pMultithread ) ) )
								{
									pMultithread->SetMultithreadProtected( TRUE );

									ID3DDeviceContextState *pContextState = NULL;
									if ( SUCCEEDED( pDevice1->CreateDeviceContextState( 0, levelAttempts11, V_ARRAYSIZE( levelAttempts11 ), D3D11_SDK_VERSION, __uuidof( ID3D10Device1 ), &levelResult, &pContextState ) ) )
									{
										ID3DDeviceContextState *pStateLast = NULL;
										ID3D11DeviceContext1 *pContext1 = NULL;

										// Both of the below never fail and are void.
										pDevice1->GetImmediateContext1( &pContext1 );
										pContext1->SwapDeviceContextState( pContextState, &pStateLast );

										DbgVerify( pDevice1->QueryInterface( __uuidof( ID3D10Device1 ), (void**)&m_pD3D10Device ) == S_OK );
										
										SAFE_RELEASE( pStateLast );
										SAFE_RELEASE( pContext1 );
										SAFE_RELEASE( pContextState );
									}

									SAFE_RELEASE( pMultithread );
								}

								SAFE_RELEASE( pDevice1 );
							}
							SAFE_RELEASE( pDevice );
						}
					}
					else
					{
						hRes = g_D3D10CreateDevice1( allAdapters[adapter], driverType[type], NULL,
							nDeviceFlags, levelAttempts[level], D3D10_1_SDK_VERSION, &m_pD3D10Device );

						if ( FAILED( hRes ) )
						{
							m_pD3D10Device = NULL;
						}
					}

					if ( m_pD3D10Device )
					{
						DXGI_ADAPTER_DESC desc;
						if ( SUCCEEDED( allAdapters[adapter]->GetDesc( &desc ) ) )
							nGPUVendorID = desc.VendorId;
					}
				} // each adapter
			} // each level
		} // each type

		for ( int i = 0; i < nAdapterCount; i++ )
		{
			SAFE_RELEASE( allAdapters[i] );
		}

		if ( m_eRenderTarget == IUIEngine::k_ERenderToOverlaySharedTexture && nGPUVendorID == 0x10DE /*NVIDIA*/ )
		{
#if !defined( SOURCE2_PANORAMA )
			// Verify that we aren't trying to render a DXGI shared surface on an NVIDIA SLI system
			// which has alternate-frame rendering enabled. If we were to proceed, the overlay would
			// flicker rapidly, depending on which GPU is selected to render each frame of the game.
			static NvAPI_Status initOnce = NvAPI_Initialize();
			if ( initOnce == NVAPI_OK )
			{
				NV_GET_CURRENT_SLI_STATE sliState;
				sliState.version = NV_GET_CURRENT_SLI_STATE_VER;
				if ( NvAPI_D3D_GetCurrentSLIState( m_pD3D10Device, &sliState ) == NVAPI_OK 
					&& sliState.maxNumAFRGroups > 1 )
				{
					// ABORT - SLI enabled. Fail so outer logic can retry a non-shared overlay
					return false;
				}
			}
#endif
		}
	}

	if ( !m_pD3D10Device )
	{
		AssertMsg( false, "No D2D device created, possible no GPU supoprting D3D10_FEATURE_LEVEL_9_1 or higher was found? Can't create surface." );
		UIEngine()->ShowNativeTopMostMessageBox( "Error: Failed to create D3D Surface for Window.  No GPU supporting D3D10_FEATURE_LEVEL_9_1 or higher was found.", "Fatal Error", IUIEngine::k_ENativeMessageOk );
		return false;
	}

#ifdef D3DDEBUGRUNTIME

	// Filter out PSSETSHADERRESOURCES_UNBINDDELETINGOBJECT messages which are just annoying spam and not a real bug
	ID3D10InfoQueue * pInfoQueue;
	m_pD3D10Device->QueryInterface( __uuidof(ID3D10InfoQueue),  (void **)&pInfoQueue );
	if ( pInfoQueue != NULL )
	{
		// Set up the list of messages to filter
		D3D10_MESSAGE_ID messageIDs [] = { D3D10_MESSAGE_ID_PSSETSHADERRESOURCES_UNBINDDELETINGOBJECT };

		// Set the DenyList to use the list of messages
		D3D10_INFO_QUEUE_FILTER filter = { 0 };
		filter.DenyList.NumIDs = 1;
		filter.DenyList.pIDList = messageIDs;

		// Apply the filter to the info queue
		pInfoQueue->AddStorageFilterEntries( &filter );  

		// Set to break on warnings/errors
		//pInfoQueue->SetBreakOnSeverity( D3D10_MESSAGE_SEVERITY_WARNING, true );
		pInfoQueue->SetBreakOnSeverity( D3D10_MESSAGE_SEVERITY_INFO, FALSE );
		pInfoQueue->SetBreakOnSeverity( D3D10_MESSAGE_SEVERITY_ERROR, TRUE );

		SAFE_RELEASE( pInfoQueue );
	}

#endif

#ifdef _NVPERFKIT
	NVPMRESULT nvResult = GetNvPmApi()->CreateContextFromD3D10Device( m_pD3D10Device, &m_hNVPMContext );
	if( nvResult != NVPM_OK )
	{
		Msg( "NVPerfKit error CreateContextFromD3D10Device()\n" );
	}
	else
	{
		GetNvPmApi()->AddCounterByName( m_hNVPMContext, "gpu_idle" );
		GetNvPmApi()->AddCounterByName( m_hNVPMContext, "shader_busy" );
		GetNvPmApi()->AddCounterByName( m_hNVPMContext, "texture_busy" );
		GetNvPmApi()->AddCounterByName( m_hNVPMContext, "rop_busy" );
		GetNvPmApi()->AddCounterByName( m_hNVPMContext, "vertex_shader_instruction_rate" );
		GetNvPmApi()->AddCounterByName( m_hNVPMContext, "pixel_shader_instruction_rate" );
		GetNvPmApi()->AddCounterByName( m_hNVPMContext, "shader_waits_for_texture" );
		GetNvPmApi()->AddCounterByName( m_hNVPMContext, "shader_waits_for_rop" );
		GetNvPmApi()->AddCounterByName( m_hNVPMContext, "stream_out_busy" );
		GetNvPmApi()->AddCounterByName( m_hNVPMContext, "shaded_pixel_count" );
	}
#endif


	IDXGIDevice *pDXGIDevice = NULL;
	hRes = AccessDevice()->QueryInterface( &pDXGIDevice );
	if( FAILED( hRes ) )
	{
		AssertMsg( false, "QueryInterface on D3D device did not return IDXGIDevice, can't create surface." );
		UIEngine()->ShowNativeTopMostMessageBox( "Error: Failed to create D3D Surface for Window", "Fatal Error", IUIEngine::k_ENativeMessageOk );
		return false;
	}

	IDXGIAdapter *pDXGIAdapter = NULL;
	hRes = pDXGIDevice->GetAdapter( &pDXGIAdapter );
	// Done with the DXGI device
	SAFE_RELEASE( pDXGIDevice );

	if ( FAILED( hRes ) )
	{
		AssertMsg( false, "GetAdapter failed on IDXGIDevice, can't create surface." );
		UIEngine()->ShowNativeTopMostMessageBox( "Error: Failed to create D3D Surface for Window", "Fatal Error", IUIEngine::k_ENativeMessageOk );
		return false;
	}

	DXGI_ADAPTER_DESC desc;
	pDXGIAdapter->GetDesc( &desc );

	m_ulDedicatedGPUMem = desc.DedicatedVideoMemory;
	m_ulDedicatedSysMem = desc.DedicatedSystemMemory;
	m_ulSharedSysMem = desc.SharedSystemMemory;

	Msg( "!! Using adapter %S, GPU Mem: %s, Sys Mem: %s, Shared Mem: %s\n", desc.Description, V_pretifymem( (float)desc.DedicatedVideoMemory ), V_pretifymem( (float)desc.DedicatedSystemMemory ), V_pretifymem( (float)desc.SharedSystemMemory ) );

	
	// Enumerate the primary output adapter (monitor)
	IDXGIOutput *pDXGIOutput = NULL;
	hRes = pDXGIAdapter->EnumOutputs( 0, &pDXGIOutput );
	if ( FAILED( hRes ) )
	{
		SAFE_RELEASE( pDXGIAdapter ); // should still be NULL
		AssertMsg( false, "EnumOutputs failed on DXGIAdapter, blindly proceeding." );
		//UIEngine()->ShowNativeTopMostMessageBox( "Error: Failed to create D3D Surface for Window (EnumOutputs)", "Fatal Error", IUIEngine::k_ENativeMessageOk );
		//return false;
	}

	// Get the number of modes that fit our desired display format
	DXGI_MODE_DESC modeDesc;
	V_memset( &modeDesc, 0, sizeof( DXGI_MODE_DESC ) );

	modeDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	modeDesc.Scaling = DXGI_MODE_SCALING_CENTERED;
	modeDesc.Width = m_unWindowWidth;
	modeDesc.Height = m_unWindowHeight;
	modeDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;

	DXGI_MODE_DESC modeOut = modeDesc;


	// FindClosestMatchingMode does not work over RDP, just assume our desired mode desc will work
	bool bIsRemoteDesktop = ( ::GetSystemMetrics( SM_REMOTESESSION ) == 1 );
	if ( pDXGIOutput && !bIsRemoteDesktop )
	{
		hRes = pDXGIOutput->FindClosestMatchingMode( &modeDesc, &modeOut, m_pD3D10Device );
		if ( FAILED( hRes ) )
		{
			SAFE_RELEASE( pDXGIOutput );
			SAFE_RELEASE( pDXGIAdapter );
			AssertMsg( false, "Failed to find matching mode for DXGIOutput, can't create surface." );
			UIEngine()->ShowNativeTopMostMessageBox( "Error: Failed to find matching mode for DXGIOutput, can't create D3D Window", "Fatal Error", IUIEngine::k_ENativeMessageOk );
			return false;
		}
	}

	SAFE_RELEASE( pDXGIOutput );
	SAFE_RELEASE( pDXGIAdapter );

	if ( !IUIEngine::BIsRenderingToTexture( m_eRenderTarget ) )
	{
		// Setup swap chain
		DXGI_SWAP_CHAIN_DESC swapChainDesc;
		memset( &swapChainDesc, 0, sizeof ( DXGI_SWAP_CHAIN_DESC ) );
		swapChainDesc.BufferCount = 1;
		swapChainDesc.BufferDesc = modeOut;

		if ( !m_bVsyncEnabled )
		{
			swapChainDesc.BufferDesc.RefreshRate.Numerator = 0;
			swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
		}

		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.OutputWindow = m_hWnd;

		// No multi-sampling for now
		swapChainDesc.SampleDesc.Count = 1;
		swapChainDesc.SampleDesc.Quality = 0;

		swapChainDesc.Windowed = (!IUIEngine::BIsRenderingToFullScreen( m_eRenderTarget ));
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
		swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

		hRes = m_pDXGIFactory->CreateSwapChain( AccessDevice(), &swapChainDesc, &m_pDXGISwapChain );
		// Default handling is not ok, since it assumes rendering is on wndproc thread. Must do this post swap chain creation.
		m_pDXGIFactory->MakeWindowAssociation( m_hWnd, DXGI_MWA_NO_WINDOW_CHANGES | DXGI_MWA_NO_ALT_ENTER );

		if ( FAILED( hRes ) )
		{
			SAFE_RELEASE( m_pDXGIFactory );
			SAFE_RELEASE( m_pD3D10Device );

			AssertMsg( false, "Failed calling CreateSwapChain on DXGIFactory, can't create surface." );
			UIEngine()->ShowNativeTopMostMessageBox( "Error: Failed in CreateSwapChain, can't create D3D window", "Fatal Error", IUIEngine::k_ENativeMessageOk );
			return false;
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Create d3d device specific resources
//-----------------------------------------------------------------------------
bool CD3D10D2DSurface::BCreateD3DDeviceResources()
{
	VPROF_BUDGET( "CD3D10D2DSurface::BCreateD3DDeviceResources()", VPROF_BUDGETGROUP_TENFOOT );
	HRESULT hResult;

	// Setup rasterizer desc
	D3D10_RASTERIZER_DESC rasterizerDesc;
	memset( &rasterizerDesc, 0, sizeof( D3D10_RASTERIZER_DESC ) );
	rasterizerDesc.AntialiasedLineEnable = false;
	rasterizerDesc.CullMode = D3D10_CULL_NONE;
	rasterizerDesc.DepthBias = 0;
	rasterizerDesc.DepthBiasClamp = 0.0f;
	// NOTE: We don't use DepthClip, but don't be tempted to set disabled, as that breaks devices with capabilities < 9.3
	rasterizerDesc.DepthClipEnable = true;
	rasterizerDesc.FillMode = D3D10_FILL_SOLID;
	rasterizerDesc.FrontCounterClockwise = false;
	rasterizerDesc.MultisampleEnable = false;
	rasterizerDesc.ScissorEnable = true;
	rasterizerDesc.SlopeScaledDepthBias = 0.0f;

	hResult = AccessDevice()->CreateRasterizerState( &rasterizerDesc, &m_pRasterizerState );
	if ( FAILED( hResult ) )
	{
		AssertMsg( false, "CreateRasterizerState failed, can't create surface" );
		UIEngine()->ShowNativeTopMostMessageBox( CFmtStr1024( "Error: CreateRasterizerState failed (0x%X), can't create D3D Window", hResult ).String(), "Fatal Error", IUIEngine::k_ENativeMessageOk );
		return false;
	}

	AccessDevice()->RSSetState( m_pRasterizerState );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Create resources that are resizable
//-----------------------------------------------------------------------------
bool CD3D10D2DSurface::BRecreateSizedD3DResources()
{
	VPROF_BUDGET( "CD3D10D2DSurface::BRecreateSizedD3DResources", VPROF_BUDGETGROUP_TENFOOT );

	HRESULT hRes;
	ID3D10RenderTargetView *viewList[1] = {NULL};
	AccessDevice()->OMSetRenderTargets( 1, viewList, NULL );

	SAFE_RELEASE( m_pBackBuffer );
	SAFE_RELEASE( m_pD2DRenderTarget );
	SAFE_RELEASE( m_pRenderTargetView );
	SAFE_RELEASE( m_pSharedTexCopy );
	if ( m_pCompositionLayer )
	{
		SAFE_LAYER_DELETE( m_pCompositionLayer );
	}
	

	ID3D10Texture2D *pBackBuffer = NULL;

	if ( IUIEngine::BIsRenderingToTexture( m_eRenderTarget ) )
	{
		Assert( m_pDXGISwapChain == NULL );

		D3D10_TEXTURE2D_DESC desc;								
		ZeroMemory( &desc, sizeof(desc) );
		desc.Width = m_unWindowWidth;
		desc.Height = m_unWindowHeight;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D10_USAGE_DEFAULT;
		if ( m_eRenderTarget == IUIEngine::k_ERenderToOpenVROverlay )
			desc.MiscFlags = D3D10_RESOURCE_MISC_SHARED;
		else
			desc.MiscFlags = 0; 
		desc.BindFlags = D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE;

		// initializes sync shared surface texture
		hRes = AccessDevice()->CreateTexture2D( &desc, NULL, &pBackBuffer );
		if( FAILED( hRes ) || pBackBuffer == NULL )
		{
			AssertMsg( false, "CreateTexture2D failed, can't create render to texture surface" );
			return false;
		}
	}
	else
	{
		HRESULT hRes = m_pDXGISwapChain->ResizeBuffers( 1, m_unWindowWidth, m_unWindowHeight, DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH );
		if ( FAILED( hRes ) )
		{
			AssertMsg1( false, "ResizeBuffers failed, can't create surface: 0x%x\n", (int)hRes );
			UIEngine()->ShowNativeTopMostMessageBox( "Error: ResizeBuffers failed, can't create D3D Window", "Fatal Error", IUIEngine::k_ENativeMessageOk );
			return false;
		}

		hRes = m_pDXGISwapChain->GetBuffer( 0, __uuidof(ID3D10Texture2D), (LPVOID*)&pBackBuffer );
		if ( FAILED( hRes ) )
		{
			AssertMsg1( false, "GetBuffer failed, can't acquire surface: 0x%x\n", (int)hRes );
			UIEngine()->ShowNativeTopMostMessageBox( "Error: ResizeBuffers failed, can't create D3D Window", "Fatal Error", IUIEngine::k_ENativeMessageOk );
			return false;
		}

		if ( m_pCompositionLayer )
			m_pCompositionLayer->ModifyWidthAndHeight( m_unWindowWidth, m_unWindowHeight );
	}

	// create the render target view and set this as our "back buffer"
	if ( !BCreateRenderTarget( pBackBuffer, m_pD2DFactory ) )
		return false;

	// Bind the render target view and depth stencil buffer to the output render pipeline.
	ID3D10RenderTargetView *pRenderTargetView = AccessD3DRenderTargetView();
	AccessDevice()->OMSetRenderTargets( 1, &pRenderTargetView, NULL );

	// Setup viewport
	D3D10_VIEWPORT viewport;
	viewport.Width = (float)m_unWindowWidth;
	viewport.Height = (float)m_unWindowHeight;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	AccessDevice()->RSSetViewports( 1, &viewport );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Track which window we will ultimately render to, we wish we couldn't care
// inside the render layer, but windows has coupled these things.
//-----------------------------------------------------------------------------
void panorama::CD3D10D2DSurface::SetHWND( HWND hwnd )
{
	Assert( IUIEngine::BIsRenderingToTexture( m_eRenderTarget ) );
	if ( IUIEngine::BIsRenderingToTexture( m_eRenderTarget ) )
	{
		m_hWnd = hwnd;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Track which OpenVR Overlay we should send our output to
//-----------------------------------------------------------------------------
void panorama::CD3D10D2DSurface::SetVROverlayHandle( vr::VROverlayHandle_t ulVROverlayHandle )
{
	Assert( m_eRenderTarget == IUIEngine::k_ERenderToOpenVROverlay );
	if ( m_eRenderTarget == IUIEngine::k_ERenderToOpenVROverlay )
	{
		m_ulVROverlayHandle = ulVROverlayHandle;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Determine if any part of the window is visible and worth rendering.
//-----------------------------------------------------------------------------
bool CD3D10D2DSurface::BSurfaceOccluded()
{
	if ( m_bSurfaceOccluded == false )
		return false;
	
	// This should set in BeginFrame before the logic that can set m_bSurfaceOccluded
	Assert ( ThreadGetCurrentId() != (ThreadId_t)0 );

	// DXGI test presents can deadlock on the message queue being pumped, not thread safe
	if ( ThreadGetCurrentId() == m_renderThreadID )
	{
		// if we think we are occluded, we need to test again, because anyone
		// paying attention to this may not drive the loop that would hit
		// the Present() call the normal way.
		HRESULT hRes = m_pDXGISwapChain->Present( 0, DXGI_PRESENT_TEST );
		if ( hRes != DXGI_STATUS_OCCLUDED )
			m_bSurfaceOccluded = false;
	}

	return m_bSurfaceOccluded;
}


//-----------------------------------------------------------------------------
// Purpose: Add a backbuffer and related resources to manage.
//-----------------------------------------------------------------------------
bool CD3D10D2DSurface::BCreateRenderTarget( ID3D10Texture2D *pBackBuffer, ID2D1Factory *pD2DFactory )
{
	ID2D1RenderTarget *pD2DRenderTarget = NULL;
	ID3D10RenderTargetView *pD3DRenderTargetView = NULL;

	D3D_SetDebugName( pBackBuffer, "BackBuffer" );
	
	// Create render target view
	D3D10_RENDER_TARGET_VIEW_DESC renderDesc;
	renderDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	renderDesc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2D;
	renderDesc.Texture2D.MipSlice = 0;
	
	ID3D10Device *pDevice = NULL;
	pBackBuffer->GetDevice(&pDevice);

	HRESULT hRes = pDevice->CreateRenderTargetView( pBackBuffer, &renderDesc, &pD3DRenderTargetView );
	SAFE_RELEASE( pDevice );

	if ( FAILED( hRes ) )
	{
		AssertMsg( false, "CreateRenderTargetView failed, can't create surface" );
		UIEngine()->ShowNativeTopMostMessageBox( "Error: CreateRenderTargetView failed, can't create D3D Window", "Fatal Error", IUIEngine::k_ENativeMessageOk );
		return false;
	}

	D3D_SetDebugName( pD3DRenderTargetView, "RenderTargetView" );

	IDXGISurface *pDXGISurface = NULL;
	hRes = pBackBuffer->QueryInterface( &pDXGISurface );
	if ( SUCCEEDED( hRes ) )
	{
		D2D1_RENDER_TARGET_PROPERTIES props =
			D2D1::RenderTargetProperties(
			D2D1_RENDER_TARGET_TYPE_DEFAULT,
			D2D1::PixelFormat( DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED ),
			96,
			96
			);

		hRes = pD2DFactory->CreateDxgiSurfaceRenderTarget( pDXGISurface, &props, &pD2DRenderTarget );

		SAFE_RELEASE( pDXGISurface );
	}
	
	if ( FAILED( hRes ) )
	{
		AssertMsg( false, "QueryInterface failed to find IDXGISurface on backbuffer, can't create surface" );
		UIEngine()->ShowNativeTopMostMessageBox( "Error: CreateRenderTargetView failed, can't create D3D Window", "Fatal Error", IUIEngine::k_ENativeMessageOk );
		return false;
	}

	m_pBackBuffer = pBackBuffer;
	m_pD2DRenderTarget = pD2DRenderTarget;
	m_pRenderTargetView = pD3DRenderTargetView;

	return true;
}

