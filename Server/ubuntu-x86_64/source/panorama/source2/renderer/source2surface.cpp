//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose:  
//=============================================================================//

#ifdef _WIN32
#define _WIN32_WINNT 0x0502
#ifndef DX_TO_GL_ABSTRACTION
#include <WinSock2.h>
#endif
#endif
#include "togl/rendermechanism.h"
#include "stdafx.h"
#include "source2surface.h"
#include "mathlib/vmatrix.h"
#include "tier1/fileio.h"
#include "tier0/vprof.h"
#include "fmtstr.h"
#include "panorama/panoramatypes.h"
#include "panorama/layout/csshelpers.h"
#include "color.h"
#include "tier1/checksum_crc.h"
#include "renderer/uirenderengine.h"

#include "rendersystem/irenderdevice.h"
#include "rendersystem/irendercontext.h"
#include "rendersystem/indexdata.h"
#include "rendersystem/vertexdata.h"
#include "resourcesystem/stronghandle.h"
#include "inputsystem/iinputsystem.h"
#include "resourcesystem/resourcemanifesthelpers.h"
#include "resourcesystem/iresourcesystem.h"
#include "materialsystem2/imaterialsystem2utils.h"
#include "scenesystem/sceneobject.h"
#include "scenesystem/iscenelayer.h"
#include "interfaces/interfaces.h"
#include "rendersystem/irenderhardwareconfig.h"

#include "source2/uienginesource2.h"
#include "source2/uirenderdevicesource2.h"

#if PANDX_DRAW
#include "../panoramauiengine.h"
#endif

#include "wrap_texture.h"

#if defined ( DX_TO_GL_ABSTRACTION )
// Placed here so inlines placed in dxabstract.h can access gGL
extern COpenGLEntryPoints *gGL;
#endif

static ConVar s_panorama_blur_ecomode( "@panorama_blur_ecomode", "0", FCVAR_DEVELOPMENTONLY );
static ConVar s_panorama_blur_ecomode_fps( "@panorama_blur_ecomode_fps", "16.0", FCVAR_DEVELOPMENTONLY );

ConVar s_convarUseAsyncTextlayoutGeneration( "pan_asynctext", "1", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );

#if PANDX_DRAW
bool g_bPanDx = 1;
ConVar s_convarPanDx( "pan_dx", "1", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );
bool g_pdxInit = false;

panorama::FancyQuad_t *PanDxGetFancyQuadPtr( int nNumFancyQuads );
panorama::BasicQuad_t *PanDxGetBasicQuadPtr();

//
// temp data for batching stats
//

//#define PANDX_LAZY_STATE_EVAL_CONSTS_ALL
//#define PANDX_TRACK_BATCHING

#ifdef PANDX_TRACK_BATCHING

bool g_bPanDxRenderStateChanged = true;

struct sPanDxBatching
{
	// tracking stats
	int m_nDrawCalls;					// total draw calls between own/disown dx
	int m_nBasicDrawCalls;				// totall basicquad calls
	int m_nFancyDrawCalls;				// total fancyquad draw calls
	int m_nBatchableBasicDrawCalls;		// how many basicquad could have been batched since no render states changed from the previous call
	int m_nBatchableFancyDrawCalls;		// how many fancyquad could have been batched since no render states changed from the previous call
	int m_nOptimalBasicDrawCalls;		// optimal number of basic draw calls if we batched (nBatchableFancyDrawCalls + nOptimalFancyDrawCalls = nFancyDrawCalls)
	int m_nOptimalFancyDrawCalls;		// optimal number of fancy draw calls if we batched (nBatchableFancyDrawCalls + nOptimalFancyDrawCalls = nFancyDrawCalls)

	// counts for what render state breaks potential batching
	int m_nSetViewport;
	int m_nClearRenderTarget;
	int m_nSetRenderTarget;
	int m_nSetScissorEnable;
	int m_nSetScissorRect;
	int m_nSampler;
	int m_nSamplerIsRenderTarget;
	int m_nSamplerState;
	int m_nRenderState;
	int m_nVSConst;
	int m_nPSConst;
	int m_nVS;
	int m_nPS;
	int m_nVertexDecl;
	int m_nVB;
	int m_nPrimType;
};

sPanDxBatching g_PanDxBatching;

#define PANDX_STATS_BATCHING( a ) g_PanDxBatching.m_n##a++

#define PANDX_BROKE_BATCHING( a )\
g_bPanDxRenderStateChanged = true;\
g_PanDxBatching.m_n##a++;

#define PANDX_STATS_BASIC() \
PANDX_STATS_BATCHING( DrawCalls );\
PANDX_STATS_BATCHING( BasicDrawCalls );\
if ( g_bPanDxRenderStateChanged == false )\
	PANDX_STATS_BATCHING( BatchableBasicDrawCalls );\
else\
	PANDX_STATS_BATCHING( OptimalBasicDrawCalls );\
g_bPanDxRenderStateChanged = false;

#define PANDX_STATS_FANCY( nNumQuads ) \
PANDX_STATS_BATCHING( DrawCalls );\
if ( g_bPanDxRenderStateChanged == false )\
	PANDX_STATS_BATCHING( BatchableFancyDrawCalls );\
else\
	PANDX_STATS_BATCHING( OptimalFancyDrawCalls );\
g_bPanDxRenderStateChanged = false;

#else

struct sPanDxBatching
{
	// tracking stats
	int m_nDrawCalls;					// total draw calls between own/disown dx
	int m_nBasicDrawCalls;				// totall basicquad calls
	int m_nFancyDrawCalls;				// total fancyquad draw calls
};

sPanDxBatching g_PanDxBatching;

#define PANDX_STATS_BATCHING( a ) g_PanDxBatching.m_n##a++

#define PANDX_BROKE_BATCHING( a )

#define PANDX_STATS_BASIC() \
PANDX_STATS_BATCHING( DrawCalls ); \
PANDX_STATS_BATCHING( BasicDrawCalls );

#define PANDX_STATS_FANCY( nNumQuads ) PANDX_STATS_BATCHING( DrawCalls );

#endif

#else

#define PANDX_STATS_BATCHING( a ) 
#define PANDX_BROKE_BATCHING( a )
#define PANDX_STATS_BASIC()
#define PANDX_STATS_FANCY( nNumQuads )

#endif

//
// Rounded corners. Notes on terminolgy.
//
// innercorners : Used to draw borders and inset shadows.
// When inner corner radii are applied by the shader the corners of the quad 
// are rounded and a border color is lerped towards. The border extends all around
// the quad ( outside the radii) because the pixel ss coords are clamped to 0 
// at the edge of the ellipse/circle
//
// outercorners : Used by everything else to round of the corners of the quad (blends out to nothing)
// When outer corner radii are applied the shader and the quad is blended to transparent black at the 
// outide of the quad
//
// We either have inner or outer coords, not both.
//
// When borders are applied to quads wit no rounding, it's simply another draw call which
// put for quads on top of our quad to for a border around it.
//

void HighlightCompositionLayersChanged( IConVar *pConVar, const char *pOldValue, float flOldValue );

ConVar s_convarPanoramaHighlightCompositionLayers( "@panorama_highlight_composition_layers", "0", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT, "", false, 0.0f, false, 0.0f, &HighlightCompositionLayersChanged );

ConVar s_convarPanoramaDisableOuterShadowLayerCache( "@panorama_disable_outershadow_layer_cache", "0", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );
ConVar s_convarPanoramaDisableImageShadowLayerCache( "@panorama_disable_imageshadow_layer_cache", "0", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );
ConVar s_convarPanoramaDisableLayerCache( "@panorama_disable_layer_cache", "0", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );
ConVar s_convarPanoramaMinCompLayerCacheCost( "@panorama_min_comp_layer_cache_cost", "0" ); // faster to always cache, trying 0 here. PREV: "@panorama_min_comp_layer_cache_cost", 4096" );
ConVar s_convarPanoramaDisableRTCache( "@panorama_disable_render_target_cache", "0", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );
ConVar s_convarPanoramaRTCacheMaxSize( "@panorama_render_target_cache_max_size", "31457280" );	// 30*1024*1024

ConVar s_convarPanoramaRenderStats( "@panorama_render_stats", "0", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );
ConVar s_convarPanoramaRenderStatsPosX( "@panorama_render_stats_posx", "50", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );
ConVar s_convarPanoramaRenderStatsPosY( "@panorama_render_stats_posy", "500", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );

ConVar s_convarPanoramaVsync( "@panorama_vsync", "1" );
ConVar s_convarPanoramaMaxFreeFBO( "@panorama_max_free_fbo", "1000");
ConVar s_convarPanoramaFBOAllocBatch( "@panorama_fbo_alloc_batch", "10");

ConVar s_convarPanoramaDisableBlur( "@panorama_disable_blur", "0", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );
ConVar s_convarPanoramaDisableBoxShadow( "@panorama_disable_box_shadow", "0", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );
ConVar s_convarPanoramaDisableRenderCallbacks( "@panorama_disable_render_callbacks", "0", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );
ConVar s_convarPanoramaDisableDrawFancyQuad( "@panorama_disable_draw_fancy_quad", "0", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );
ConVar s_convarPanoramaDisableLayerClear( "@panorama_disable_layer_clear", "0", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );
ConVar s_convarPanoramaDisableDrawText( "@panorama_disable_draw_text", "0", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );
ConVar s_convarPanoramaDisableDrawTextShadow( "@panorama_disable_draw_text_shadow", "0", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );
ConVar s_convarPanoramaForceFastTextShadow( "@panorama_force_fast_text_shadow", "0", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );
ConVar s_convarPanoramaExperimentalOverdrawPrevention( "@panorama_experimental_overdraw_prevention", "0" );
ConVar s_convarPanoramaUseBackbufferDirectly( "@panorama_use_backbuffer_directly", "1", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );

ConVar s_convarPanoramaEnableMotionBlur( "@panorama_enable_motion_blur", "0", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );
ConVar s_convarPanoramaMotionBlurVelocityScale( "@panorama_motion_blur_velocity_scale", "0.04", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );

static void DebugFontSelectionChanged( IConVar *var, const char *pOldValue, float flOldValue );
static ConVar s_convarDebugFontSelection( "@panorama_debug_font_selection", "0", FCVAR_DEVELOPMENTONLY, "", DebugFontSelectionChanged );
static bool s_bClearTextCacheBeforeNextFrame = false;

#ifdef PANORAMA_USE_S1WRAPPER
//ConVar s_convarPanoramaUseHWConfigForNPO2TextureSupport( "@panorama_use_hwconfig_for_NPO2_texture_support", "1", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT | FCVAR_HIDDEN, "1 - use hwconfig to determine NPO2 support, 0 - force disable native NPO2 support in panorama (for debugging)" );
//#define PANORAMA_S1_NPO2_NO_TEXCOORDSCALE
ConVar s_convarPanoramaClampFractionalPixelPositions( "@panorama_clamp_fractional_pixel_positions", "1" );
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

CInterlockedInt CSource2Surface::s_nCompositionRenderTargets = 0;
CInterlockedInt CSource2Surface::s_nTextureIDs = 0;

struct Source2FancyQuadVertex_t
{
	Vector4D m_vecPosition;
	Vector4D m_vecTexCoordGradientCoord;
	Vector4D m_vecColor0;
	Vector4D m_vecColor1;
	Vector4D m_vecOpacityTexCoord;
	Vector4D m_vecFragCoordWdHt;
};

struct Source2BasicQuadVertex_t
{
	Vector4D m_vecPosition;
	Vector4D m_vecTex;
	Vector4D m_vecTex1;
	Vector4D m_vecColor;
};

//-----------------------------------------------------------------------------
// Purpose: Decode our 0xAABBGGRR color constants to linear color (from sRGB or linear RGB)
//-----------------------------------------------------------------------------

inline Vector4D VecColorPreMulAlpha(Vector4D in)
{
	Vector4D out;
	out.x = in.x * in.w;
	out.y = in.y * in.w;
	out.z = in.z * in.w;
	out.w = in.w;
	return out;
}

inline Vector4D VecColorPreDivAlpha( Vector4D in )
{
	Vector4D out;
	out.x = in.x / in.w;
	out.y = in.y / in.w;
	out.z = in.z / in.w;
	out.w = in.w;
	return out;
}

inline Vector4D VecLinearToSrgb( Vector4D in )
{
	Vector4D out;
	out.x =  SrgbLinearToGamma( in.x );
	out.y =  SrgbLinearToGamma( in.y );
	out.z =  SrgbLinearToGamma( in.z );
	out.w =  in.w;
	return out;
}

inline Vector4D VecColorFromABGR( unsigned int c )
{
	Vector4D vec;

	vec.x = SrgbGammaToLinear( (c & 0xff) * (1.0f / 255.0f) );
	vec.y = SrgbGammaToLinear( ((c >> 8) & 0xff) * (1.0f / 255.0f) );
	vec.z = SrgbGammaToLinear( ((c >> 16) & 0xff) * (1.0f / 255.0f) );
	vec.w = ((c >> 24) & 0xff) * (1.0f / 255.0f);

	return vec;
}

inline Vector4D VecColorFromABGRPreMul( unsigned int c )
{
	Vector4D vec;

	vec.w = ( ( c >> 24 ) & 0xff ) * ( 1.0f / 255.0f );
	vec.x = SrgbGammaToLinear( ( c & 0xff ) * ( 1.0f / 255.0f ) ) * vec.w;
	vec.y = SrgbGammaToLinear( ( ( c >> 8 ) & 0xff ) * ( 1.0f / 255.0f ) )* vec.w;
	vec.z = SrgbGammaToLinear( ( ( c >> 16 ) & 0xff ) * ( 1.0f / 255.0f ) )* vec.w;

	return vec;
}


//-----------------------------------------------------------------------------
// Purpose: Decode our 0xAABBGGRR color constants to linear color 
//-----------------------------------------------------------------------------
void ColorFromABGR( float &r, float &g, float &b, float &a, unsigned int c )
{
	r = SrgbGammaToLinear( (c & 0xff) * (1.0f / 255.0f) );
	g = SrgbGammaToLinear( ((c >> 8) & 0xff) * (1.0f / 255.0f) );
	b = SrgbGammaToLinear( ((c >> 16) & 0xff) * (1.0f / 255.0f) );
	a = ((c >> 24) & 0xff) * (1.0f / 255.0f);
}


//-----------------------------------------------------------------------------
//Purpose: Round up size to a multiple of 32
//-----------------------------------------------------------------------------
static int16 RoundUpRenderTargetSize( int16 size )
{
	return Max( 32, ( size + 31 ) & ~31 );
}


//-----------------------------------------------------------------------------
//Purpose: Pre transform vertex positions (taking out of VS, mostly to remove all VS consts)
//-----------------------------------------------------------------------------
void PreXFormFancyQuadPositions( CRenderAttributes *pRenderAttributes, FancyQuad_t *q, const VMatrix *pMat, float flViewportW, float flViewportH, int nNumQuads )
{
	float flHalfTexelOffset = 0.5f;

	if ( pMat )
	{
		for ( int i = 0; i < nNumQuads; i++ )
		{
			FancyQuad_t *pFancyQuad = q + i;

			for ( int j = 0; j < 4; j++ )
			{
				Vector4D tmpV4;
				pRenderAttributes->GetValue( &pFancyQuad->m_Verts[j].m_vFragCoordWdHt, ATTR_TopLeftWdHt );

				// pre x-formed pos.xy - topleft goes here (used in D_USEOUTERCORNER/D_USEINNERCORNER combo)
				// i.e. replacing 	o.vfragCoord.xy = i.vPositionSs.xy - g_vTopLeftWdHt.xy;
				pFancyQuad->m_Verts[ j ].m_vFragCoordWdHt.x = pFancyQuad->m_Verts[ j ].m_vPosition.x - pFancyQuad->m_Verts[ j ].m_vFragCoordWdHt.x;
				pFancyQuad->m_Verts[ j ].m_vFragCoordWdHt.y = pFancyQuad->m_Verts[ j ].m_vPosition.y - pFancyQuad->m_Verts[ j ].m_vFragCoordWdHt.y;

				// assumes pos of form (x,y,0,1)
				Vector2D tmpV;
				tmpV.x = ( pMat->m[ 0 ][ 0 ] * pFancyQuad->m_Verts[ j ].m_vPosition.x ) + ( pMat->m[ 0 ][ 1 ] * pFancyQuad->m_Verts[ j ].m_vPosition.y ) + ( pMat->m[ 0 ][ 3 ] );
				tmpV.y = ( pMat->m[ 1 ][ 0 ] * pFancyQuad->m_Verts[ j ].m_vPosition.x ) + ( pMat->m[ 1 ][ 1 ] * pFancyQuad->m_Verts[ j ].m_vPosition.y ) + ( pMat->m[ 1 ][ 3 ] );

				pFancyQuad->m_Verts[ j ].m_vPosition.x = ( ( tmpV.x - flHalfTexelOffset ) / flViewportW ) * 2.0f - 1.0f;
				pFancyQuad->m_Verts[ j ].m_vPosition.y = -( ( tmpV.y - flHalfTexelOffset ) / flViewportH ) * 2.0f + 1.0f;
			}
		}
	}
	else
	{
		// identity transform
		for ( int i = 0; i < nNumQuads; i++ )
		{
			FancyQuad_t *pFancyQuad = q + i;

			for ( int j = 0; j < 4; j++ )
			{
				Vector4D tmpV4;
				pRenderAttributes->GetValue( &pFancyQuad->m_Verts[ j ].m_vFragCoordWdHt, ATTR_TopLeftWdHt );

				// pre x-formed pos.xy - topleft goes here (used in D_USEOUTERCORNER/D_USEINNERCORNER combo)
				// i.e. replacing 	o.vfragCoord.xy = i.vPositionSs.xy - g_vTopLeftWdHt.xy;
				pFancyQuad->m_Verts[ j ].m_vFragCoordWdHt.x = pFancyQuad->m_Verts[ j ].m_vPosition.x - pFancyQuad->m_Verts[ j ].m_vFragCoordWdHt.x;
				pFancyQuad->m_Verts[ j ].m_vFragCoordWdHt.y = pFancyQuad->m_Verts[ j ].m_vPosition.y - pFancyQuad->m_Verts[ j ].m_vFragCoordWdHt.y;

				// xform
				pFancyQuad->m_Verts[ j ].m_vPosition.x = ( ( pFancyQuad->m_Verts[ j ].m_vPosition.x - flHalfTexelOffset ) / flViewportW ) * 2.0f - 1.0f;
				pFancyQuad->m_Verts[ j ].m_vPosition.y = -( ( pFancyQuad->m_Verts[ j ].m_vPosition.y - flHalfTexelOffset ) / flViewportH ) * 2.0f + 1.0f;
			}
		}
	}
}

void PreXFormBasicQuadPositions( CRenderAttributes *pRenderAttributes, BasicQuad_t *q, float flViewportW, float flViewportH )
{
	float flHalfTexelOffset = 0.5f;

	// identity transform
	for ( int j = 0; j < 4; j++ )
	{
		q->m_vPosition[ j ].x = ( ( q->m_vPosition[ j ].x - flHalfTexelOffset ) / flViewportW ) * 2.0f - 1.0f;
		q->m_vPosition[ j ].y = -( ( q->m_vPosition[ j ].y - flHalfTexelOffset ) / flViewportH ) * 2.0f + 1.0f;
	}
}

//-----------------------------------------------------------------------------
//Purpose: 
//-----------------------------------------------------------------------------
void CSource2RenderTargetCache::Add( HRenderTexture hRenderTarget )
{
	if ( !hRenderTarget.IsValid() || s_convarPanoramaDisableRTCache.GetBool() )
		return;

	hRenderTarget.GetResourceHandle()->AddRef();

	const CTextureDesc *pDesc = g_pRenderDevice->GetTextureDesc( hRenderTarget );
	m_nSizeRTsInBytes += pDesc->m_nWidth * pDesc->m_nHeight * 4;	// Assuming IMAGE_FORMAT_RGBA8888

	const CacheEntry_t entry = { pDesc->m_nWidth, pDesc->m_nHeight, hRenderTarget };
	m_cachedRTs.AddToTail( entry );
}

//-----------------------------------------------------------------------------
//Purpose:	Try to find a compatible-size render target corresponding to the given 
//			width and height:
//				* try to find a exact match first
//				* otherwise try to return the smallest render target with a 
//				  bigger or equal size (closest match)
//			Returns RESOURCE_HANDLE_INVALID if no match found
//-----------------------------------------------------------------------------
HRenderTexture CSource2RenderTargetCache::GetMatching( int16 nWidth, int16 nHeight )
{
	HRenderTexture hRenderTarget = RESOURCE_HANDLE_INVALID;

	if ( s_convarPanoramaDisableRTCache.GetBool() )
	{
		return hRenderTarget;
	}

	const int32 nArea = nWidth * nHeight;
	int nBestMatchEntry = -1;
	int32 nBestMatchArea = INT32_MAX;
	FOR_EACH_LL( m_cachedRTs, nEntry )
	{
		const CacheEntry_t &entry = m_cachedRTs[nEntry];
		const int32 nEntryArea = entry.m_nWidth * entry.m_nHeight;

		if ( ( entry.m_nWidth == nWidth ) && ( entry.m_nHeight == nHeight ) )
		{
			// Exact match
			nBestMatchEntry = nEntry;
			break;
		}
		else if ( ( entry.m_nWidth >= nWidth ) && ( entry.m_nHeight >= nHeight ) && ( nEntryArea < nBestMatchArea ) )
		{
			// Closest render target of the desired size
			nBestMatchEntry = nEntry;
			nBestMatchArea = nEntryArea;
		}
	}

	if ( nBestMatchEntry != -1 )
	{
		const CacheEntry_t &entry = m_cachedRTs[nBestMatchEntry];

		hRenderTarget = entry.m_hRenderTarget;

		// Decrement ref count - might go to 0. It is the responsibility of the caller to increment
		// ref count if necessary. The resource system will not delete the render target if you increment 
		// the ref count in the same frame.
		hRenderTarget.GetResourceHandle()->Release();
		m_nSizeRTsInBytes -= entry.m_nWidth * entry.m_nHeight * 4;	// Assuming IMAGE_FORMAT_RGBA8888

		m_cachedRTs.Remove( nBestMatchEntry );
	}

	return hRenderTarget;
}

//-----------------------------------------------------------------------------
//Purpose: Removes render targets until cache size is under the nMaxCacheSizeInBytes limit.
//-----------------------------------------------------------------------------
void CSource2RenderTargetCache::PurgeOverSizeLimit( uint64 nMaxCacheSizeInBytes )
{
	while ( ( m_nSizeRTsInBytes > nMaxCacheSizeInBytes ) && !m_cachedRTs.IsEmpty() )
	{
		int nEntry = m_cachedRTs.Head();
		const CacheEntry_t &entry = m_cachedRTs[nEntry];

		// Delete render target.
		entry.m_hRenderTarget.GetResourceHandle()->Release();
		m_nSizeRTsInBytes -= entry.m_nWidth * entry.m_nHeight * 4;	// Assuming IMAGE_FORMAT_RGBA8888

		m_cachedRTs.Remove( nEntry );
	}
}


//-----------------------------------------------------------------------------
// Constructor for dummy composition layers used for searching
//-----------------------------------------------------------------------------
CSource2CompositionLayer::CSource2CompositionLayer( float flWidth, float flHeight, bool bNeedsDepth, bool bNeedsIntermediate ) : 
	m_flLayerWidth( ceil( flWidth ) ), 
	m_flLayerHeight( ceil( flHeight ) ), 
	m_flLayerCreationWidth( flWidth ),
	m_flLayerCreationHeight( flHeight ),
	m_bNeedsDepth( bNeedsDepth ),
	m_bNeedsIntermediate( bNeedsIntermediate ),
	m_bShouldCache( true ),
	m_bReusedFromCache( false ),
	m_bOffscreen( false ),
	m_hRenderTarget( RENDER_TEXTURE_HANDLE_INVALID ),
	m_hIntermediateRenderTarget( RENDER_TEXTURE_HANDLE_INVALID ),
	m_hDepthStencilRenderTarget( RENDER_TEXTURE_HANDLE_INVALID ),
	m_ePriorFractionalPixelPositions( k_EFractionalPixelPositionsDefault )
{
}

//-----------------------------------------------------------------------------
// Purpose: Constructor 
//-----------------------------------------------------------------------------
CSource2CompositionLayer::CSource2CompositionLayer( CSource2Surface *pParentSurface, float flWidth, float flHeight, bool bNeedsDepth, bool bNeedsIntermediate, bool bIsBackBuffer, bool bOffscreen, const char *pchType )
{
	ResetToDefault();
	
	m_pParentSurface = pParentSurface;
	pParentSurface->GetFrameStats().m_nCompositionLayersCreated++;

	m_ulContextID = 0;
	m_bIsDrawing = false;

	m_flLayerWidth = ceil( flWidth );
	m_flLayerHeight = ceil( flHeight );
	m_flLayerCreationWidth = flWidth;
	m_flLayerCreationHeight = flHeight;

	m_bNeedsDepth = bNeedsDepth;
	m_bNeedsIntermediate = bNeedsIntermediate;
	m_bOffscreen = bOffscreen;

	m_flHueShift = 0.0f;
	m_flSaturation = 1.0f;
	m_flBrightness = 1.0f;
	m_flContrast = 1.0f;

	m_flOpacityMaskOpacity = 1.0f;

	m_flBlurPasses = 1.0f;
	m_flBlurStdDevHor = 0.0f;
	m_flBlurStdDevVer = 0.0f;
	m_eBlendMode = k_EMixBlendModeNormal;

	m_rgBorderWidths[0] = 0.0f;
	m_rgBorderWidths[1] = 0.0f;
	m_rgBorderWidths[2] = 0.0f;
	m_rgBorderWidths[3] = 0.0f;

	m_bBoxShadowInset = false;
	m_bBoxShadowFill = false;
	m_flBoxShadowHorOffset = 0.0f;
	m_flBoxShadowVerOffset = 0.0f;
	m_flBoxShadowBlurRadius = 0.0f;
	m_flBoxShadowSpreadDistance = 0.0f;
	m_rgbaBoxShadowColor = 0x00000000;

	m_flScaleLayerX = 1.0f;
	m_flScaleLayerY = 1.0f;
	m_flTranslateLayerX = 0.0f;
	m_flTranslateLayerY = 0.0f;

	m_VMatrix.Identity();

	m_flScale2D[0] = 1.0f;
	m_flScale2D[1] = 1.0f;

	m_flMotionBlurVelocity = 0.0f;
	m_flMotionBlurDirection[0] = 0.0f;
	m_flMotionBlurDirection[1] = 0.0f;
	m_iMotionBlurSampleCount = 0;

	m_hRenderTarget = RENDER_TEXTURE_HANDLE_INVALID;
	m_hIntermediateRenderTarget = RENDER_TEXTURE_HANDLE_INVALID;
	m_hDepthStencilRenderTarget = RENDER_TEXTURE_HANDLE_INVALID;
	m_flRTOriginalWidthScale = 1.0f;
	m_flRTOriginalHeightScale = 1.0f;

	m_bIsBackBuffer = bIsBackBuffer;

	m_nRTSizeInBytes = 0;

	m_ePriorFractionalPixelPositions = k_EFractionalPixelPositionsDefault;

	AssertMsgOnce( !bNeedsIntermediate || !bIsBackBuffer, "Intermediate textures are only supported by non-backbuffer Panorama composition layers.\n" );

	if ( !bIsBackBuffer )
	{
		CTextureCreationDesc specRT;
		specRT.m_nWidth = m_flLayerWidth;
		specRT.m_nHeight = m_flLayerHeight;
		specRT.m_nNumMipLevels = 1;
		specRT.m_nDepth = 1;
		specRT.m_nFlags = TSPEC_RENDER_TARGET | TSPEC_RENDER_TARGET_SAMPLEABLE | TSPEC_SUGGEST_CLAMPS | TSPEC_SUGGEST_CLAMPT;
		specRT.m_nImageFormat = IMAGE_FORMAT_RGBA8888;
		specRT.m_nMultisampleType = RENDER_MULTISAMPLE_NONE;
		specRT.m_Reflectivity.Init( 1, 1, 1 );
		specRT.m_nUsage = TEXTURE_USAGE_GPU_ONLY;

#ifndef PANORAMA_USE_S1WRAPPER
		m_hRenderTarget = g_pRenderDevice->FindOrCreateTexture( CFmtStr( "panorama_rt_%s.vtex", pchType ).Get(), true, &specRT );

		m_hDepthStencilRenderTarget = RENDER_TEXTURE_HANDLE_INVALID;
		if ( m_bNeedsDepth )
		{
			// depth stencil MUST BE THE SAME SIZE
			CTextureCreationDesc specDepthStencil;
			specDepthStencil.m_nWidth = specRT.m_nWidth;
			specDepthStencil.m_nHeight = specRT.m_nHeight;
			specDepthStencil.m_nNumMipLevels = 1;
			specDepthStencil.m_nDepth = 1;
			specDepthStencil.m_nFlags = TSPEC_RENDER_TARGET;
			specDepthStencil.m_nImageFormat = IMAGE_FORMAT_D24S8;
			specDepthStencil.m_nMultisampleType = RENDER_MULTISAMPLE_NONE;
			specDepthStencil.m_Reflectivity.Init( 1, 1, 1 );
			specDepthStencil.m_nUsage = TEXTURE_USAGE_GPU_ONLY;
			m_hDepthStencilRenderTarget = g_pRenderDevice->FindOrCreateTexture( CFmtStr( "panorama_rt_ds_%s.vtex", pchType ).Get(), true, &specDepthStencil );
		}
#else
		if ( m_bNeedsDepth )
		{
			specRT.m_nFlags |= TSPEC_RENDER_TARGET_WITHDS;
			m_nRTSizeInBytes += specRT.m_nWidth * specRT.m_nHeight * 4;	// Assuming D24S8 image format
		}
		else if ( !m_bOffscreen )
		{
			// Round up RT size
			specRT.m_nWidth = RoundUpRenderTargetSize( specRT.m_nWidth );
			specRT.m_nHeight = RoundUpRenderTargetSize( specRT.m_nHeight );

			// Try to get render target from the cache
			m_hRenderTarget = pParentSurface->m_RenderTargetCache.GetMatching( specRT.m_nWidth, specRT.m_nHeight );

			if ( m_hRenderTarget.IsValid() )
			{
				// The render target cache can return a render target bigger than what was requested
				const CTextureDesc *pDesc = g_pRenderDevice->GetTextureDesc( m_hRenderTarget );
				specRT.m_nWidth = pDesc->m_nWidth;
				specRT.m_nHeight = pDesc->m_nHeight;
				
				pParentSurface->GetFrameStats().m_nRTCacheHits++;
			}
			else
			{
				pParentSurface->GetFrameStats().m_nRTCacheMisses++;
			}

			m_flRTOriginalWidthScale = m_flLayerWidth / (float)specRT.m_nWidth;
			m_flRTOriginalHeightScale = m_flLayerHeight / (float)specRT.m_nHeight;
		}

		if ( !m_hRenderTarget.IsValid() )
		{
			m_hRenderTarget = g_pRenderDevice->FindOrCreateTexture( CFmtStr( "panorama_rt_%s.vtex", pchType ).Get(), true, &specRT );
		}
		m_nRTSizeInBytes += specRT.m_nWidth * specRT.m_nHeight * 4;

		m_hDepthStencilRenderTarget = RENDER_TEXTURE_HANDLE_INVALID; // TODO - sanity check where this might break
#endif

		m_hIntermediateRenderTarget = RENDER_TEXTURE_HANDLE_INVALID;
		if ( bNeedsIntermediate )
		{
			// This uses the exact same spec render target as for the primary render target
			m_hIntermediateRenderTarget = g_pRenderDevice->FindOrCreateTexture( CFmtStr( "panorama_rt_pp_%s.vtex", pchType ).Get(), true, &specRT );
			m_nRTSizeInBytes += specRT.m_nWidth * specRT.m_nHeight * 4;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CSource2CompositionLayer::~CSource2CompositionLayer()
{
	VPROF( "CSource2CompositionLayer::~CSource2CompositionLayer");

	if ( m_hRenderTarget != RENDER_TEXTURE_HANDLE_INVALID )
	{
#ifdef PANORAMA_USE_S1WRAPPER
		if ( !m_bNeedsDepth )
		{
			m_pParentSurface->m_RenderTargetCache.Add( m_hRenderTarget );
		}
#endif
		m_hRenderTarget.Shutdown();
		m_hRenderTarget = RENDER_TEXTURE_HANDLE_INVALID;
	}

	if ( m_hDepthStencilRenderTarget != RENDER_TEXTURE_HANDLE_INVALID )
	{
		m_hDepthStencilRenderTarget.Shutdown();
		m_hDepthStencilRenderTarget = RENDER_TEXTURE_HANDLE_INVALID;
	}

	if ( m_hIntermediateRenderTarget != RENDER_TEXTURE_HANDLE_INVALID )
	{
		m_hIntermediateRenderTarget.Shutdown();
		m_hIntermediateRenderTarget = RENDER_TEXTURE_HANDLE_INVALID;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Restore the composition layer to neutral settings, comparable
// to a freshly-constructed layer.  Used to reset layers pulled
// from the reuse cache.
//-----------------------------------------------------------------------------
void CSource2CompositionLayer::ResetToDefault()
{
	m_bShouldCache = true; // by default we cache composition layers to avoid redoing rendering work
	m_bReusedFromCache = false;
	m_bOffscreen = false;

	CheckAndClearClipLayers();
}

//-----------------------------------------------------------------------------
// Purpose: Helper for pushing clip layers and beginning draw on d2d target
//-----------------------------------------------------------------------------



void CSource2CompositionLayer::ActivateRenderTarget()
{
	InternalActivateRenderTarget( false );
}

void CSource2CompositionLayer::ActivateRenderTargetAndClear()
{
	InternalActivateRenderTarget( true );
}

bool CSource2CompositionLayer::GetRenderTargetHandleAndDesc( HRenderTexture &hRT, RenderTargetDesc_t &rtDesc )
{
	if ( !m_hRenderTarget.IsValid() && !m_bIsBackBuffer )
		return false;

	if ( m_bIsBackBuffer )
	{
		m_pParentSurface->GetBackBufferRenderTarget(hRT, rtDesc);
	}
	else
	{
		if ( m_hIntermediateRenderTarget != RENDER_TEXTURE_HANDLE_INVALID )
		{
			rtDesc = RenderTargetDesc_t( m_hRenderTarget.GetResourceHandle(), m_hIntermediateRenderTarget.GetResourceHandle(), m_hDepthStencilRenderTarget.GetResourceHandle(), RENDER_SRGB );
			hRT = m_hRenderTarget;
		}
		else
		{
			rtDesc = RenderTargetDesc_t( m_hRenderTarget, m_hDepthStencilRenderTarget, RENDER_SRGB );
			hRT = m_hRenderTarget;
		}
	}
		return true;

}

void CSource2CompositionLayer::InternalActivateRenderTarget( bool bClearRenderTarget )
{
	VPROF( "CSource2CompositionLayer::ActivateRenderTarget ");

	if ( !m_hRenderTarget.IsValid() && !m_bIsBackBuffer )
		return;

	bool bSetViewport = false;
	bool bSetRenderTarget = false;

	if ( m_bIsBackBuffer )
	{
		m_pParentSurface->ActivateBackBufferRenderTarget();
	}
	else
	{
		if ( m_hIntermediateRenderTarget != RENDER_TEXTURE_HANDLE_INVALID )
		{
#ifdef PANORAMA_USE_S1WRAPPER
			RenderTargetDesc_t rtDesc( m_hRenderTarget.GetResourceHandle(), m_hIntermediateRenderTarget.GetResourceHandle(), m_hDepthStencilRenderTarget.GetResourceHandle(), RENDER_SRGB );
#else
			RenderTargetDesc_t rtDesc( m_hRenderTarget, m_hIntermediateRenderTarget, m_hDepthStencilRenderTarget, RENDER_SRGB );
#endif
			bSetRenderTarget = m_pParentSurface->m_pRenderContext->BindRenderTargets( rtDesc );
		}
		else
		{
			RenderTargetDesc_t rtDesc( m_hRenderTarget, m_hDepthStencilRenderTarget, RENDER_SRGB );
			bSetRenderTarget = m_pParentSurface->m_pRenderContext->BindRenderTargets( rtDesc );
		}

		if ( bSetRenderTarget )
		{
			bSetViewport = true;
		}
	}

	if ( bClearRenderTarget )
	{
		// Clear the render target before setting the viewport as the render target could be bigger than the layer

		int nClearFlags = RENDER_CLEAR_FLAGS_CLEAR_COLOR;
		if ( m_hDepthStencilRenderTarget != RENDER_TEXTURE_HANDLE_INVALID )
		{
			nClearFlags |= RENDER_CLEAR_FLAGS_CLEAR_DEPTH;
		}

		// Clear intermediate texture also if it is bound
		uint32 nNumClearColors = ( m_hIntermediateRenderTarget != RENDER_TEXTURE_HANDLE_INVALID ) ? 2 : 1;
		const Vector4D vecClearColors[ 2 ] = { Vector4D( 0, 0, 0, 0 ), Vector4D( 0, 0, 0, 0 ) };
		m_pParentSurface->m_pRenderContext->Clear( vecClearColors, nNumClearColors, nClearFlags );
	}

	if ( bSetViewport )
	{
		RenderViewport_t renderViewport;
		renderViewport.Init( 0, 0, m_flLayerWidth, m_flLayerHeight );
		m_pParentSurface->m_pRenderContext->SetViewports( 1, &renderViewport );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Helper for pushing clip layers and beginning draw on d2d target
//-----------------------------------------------------------------------------
void CSource2CompositionLayer::PushCliplayersAndBeginDraw( float flScaleX, float flScaleY, float flTranslateX, float flTranslateY )
{
	VPROF_BUDGET_THREAD( "CCompositionLayer::PushCliplayersAndBeginDraw ", VPROF_BUDGETGROUP_TENFOOT );

	if ( !m_hRenderTarget.IsValid() && !m_bIsBackBuffer )
		return;

	Assert( !m_bIsDrawing );
	if ( !m_bIsDrawing )
	{
		m_bIsDrawing = true;
		m_flScaleLayerX = flScaleX;
		m_flScaleLayerY = flScaleY;
		m_flTranslateLayerX = flTranslateX;
		m_flTranslateLayerY = flTranslateY;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Helper for popping clip layers and ending draw on D2D Surface
//-----------------------------------------------------------------------------
void CSource2CompositionLayer::PopClipLayersAndFlush()
{
	if ( m_bIsDrawing )
	{
		m_bIsDrawing = false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Get current clip layer count for composition layer
//-----------------------------------------------------------------------------
uint32 CSource2CompositionLayer::GetClipLayerCount()
{
    return m_vecClipLayers.Count();
}

//-----------------------------------------------------------------------------
// Purpose: Check clip layer vec is empty, clear if needed
//-----------------------------------------------------------------------------
void CSource2CompositionLayer::CheckAndClearClipLayers()
{
    m_vecClipLayers.RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose: clear the texture for this layer
//-----------------------------------------------------------------------------
void CSource2CompositionLayer::Clear()
{
	if (s_convarPanoramaDisableLayerClear.GetBool())
		return;

	ActivateRenderTargetAndClear();
}

//-----------------------------------------------------------------------------
// Purpose: Draws a layer inset shadow image (unblurred) into this layer
//-----------------------------------------------------------------------------
void CSource2CompositionLayer::DrawInsetShadowIntoLayer( CSource2Surface *pBaseSurface, float flPadding, float flWidth, float flHeight, float flHorOffset, float flVerOffset, float flSpreadDistance, uint32 shadowColor, float *pflInnerRadii )
{
	VPROF( "CCompositionLayer::DrawInsetShadowIntoLayer");

	float viewWidth = flWidth, viewHeight = flHeight;
	flWidth -= flPadding * 2;
	flHeight -= flPadding * 2;
	flHorOffset += flPadding;
	flVerOffset += flPadding;

	// this function renders a rounded corner alpha blended rectangle into the layer
	FancyQuadParameters_t FancyParam( FancyQuadFlag_InnerCorner | FancyQuadFlag_HasBorder);
	FancyParam.m_flZ = 0.0f;
	FancyParam.m_flVertexMin[0] = 0;
	FancyParam.m_flVertexMin[1] = 0;
	FancyParam.m_flVertexMax[0] = m_flLayerWidth;
	FancyParam.m_flVertexMax[1] = m_flLayerHeight;
	FancyParam.m_flTexCoordMin[0] = 0.0f;
	FancyParam.m_flTexCoordMin[1] = 0.0f;
	FancyParam.m_flTexCoordMax[0] = 1.0f;
	FancyParam.m_flTexCoordMax[1] = 1.0f;
	// A radius of 1.0 is also sharp to FancyQuad, but is the minimum required
	// to properly initialize all the inner radii data structures so that
	// BorderWidth has any effect, since it uses the same codepath in the shader
	FancyParam.m_flCornerRadii[0][0] = MAX( pflInnerRadii[0], 1.0f );  // inner
	FancyParam.m_flCornerRadii[0][1] = MAX( pflInnerRadii[1], 1.0f );
	FancyParam.m_flCornerRadii[1][0] = MAX( pflInnerRadii[2], 1.0f );
	FancyParam.m_flCornerRadii[1][1] = MAX( pflInnerRadii[3], 1.0f );
	FancyParam.m_flCornerRadii[2][0] = MAX( pflInnerRadii[4], 1.0f );
	FancyParam.m_flCornerRadii[2][1] = MAX( pflInnerRadii[5], 1.0f );
	FancyParam.m_flCornerRadii[3][0] = MAX( pflInnerRadii[6], 1.0f );
	FancyParam.m_flCornerRadii[3][1] = MAX( pflInnerRadii[7], 1.0f );
	FancyParam.m_flBorderWidth[0] = flVerOffset;
	FancyParam.m_flBorderWidth[1] = flHorOffset;
	FancyParam.m_flBorderWidth[2] = m_flLayerHeight - (flVerOffset + flHeight - flSpreadDistance);
	FancyParam.m_flBorderWidth[3] = m_flLayerWidth - (flHorOffset + flWidth - flSpreadDistance);
	Vector4D vec = VecColorFromABGR( shadowColor );
	FancyParam.m_flBorderColor[0] = vec.x * vec.w;
	FancyParam.m_flBorderColor[1] = vec.y * vec.w;
	FancyParam.m_flBorderColor[2] = vec.z * vec.w;
	FancyParam.m_flBorderColor[3] = vec.w;

	FancyQuadBrush_t FancyBrush(0.0, 0.0, 0.0, 0.0);

	FancyQuadDraw_t fancyQuadDraw;
	fancyQuadDraw.m_nWide = viewWidth;
	fancyQuadDraw.m_nTall = viewHeight;
	fancyQuadDraw.m_pQuadParameters = &FancyParam;
	fancyQuadDraw.m_pQuadBrush = &FancyBrush;

	pBaseSurface->DrawFancyQuad( &fancyQuadDraw );
}

//-----------------------------------------------------------------------------
// Purpose: Draw the border for the layer
//-----------------------------------------------------------------------------
void CSource2CompositionLayer::DrawBorder( CSource2Surface *pBaseSurface )
{
	VPROF( "CSource2CompositionLayer::DrawBorder");

	// Is there any border at all?
	if ( m_rgBorderWidths[0] == 0.0f && m_rgBorderWidths[1] == 0.0f && m_rgBorderWidths[2] == 0.0f && m_rgBorderWidths[3] == 0.0f )
		return;

	// Early out for transparent colors
	bool bHasColor = false;
	for ( int iCur = 0; iCur < V_ARRAYSIZE( m_rgbaBorderColors ); ++iCur )
	{
		if ( ((m_rgbaBorderColors[iCur] >> 24) & 0xff) != 0 )
		{
			bHasColor = true;
			break;
		}
	}

	if ( !bHasColor )
		return;

	bool bHasRounding = false;
	for ( int i = 0; i < V_ARRAYSIZE( m_rgCornerRadii ); ++i )
	{
		if ( m_rgCornerRadii[i] != 0.0f )
		{
			bHasRounding = true;
			break;
		}
	}

	if ( !bHasRounding )
	{
		// forestw: draw top border with diagonal edges to meet nicely with the other borders
		CRenderAttributes renderAttributes;
		renderAttributes.SetIntValue( ATTR_D_TEXTURETYPE_NONE, 1 );
		renderAttributes.SetVMatrixValue( ATTR_MatTransform, VMatrix::GetIdentityMatrix() );
		renderAttributes.SetFloatValue( ATTR_ViewportWidth, m_flLayerWidth );
		renderAttributes.SetFloatValue( ATTR_ViewportHeight, m_flLayerHeight );

#if PANDX_DRAW
		FancyQuad_t *q = PanDxGetFancyQuadPtr( 4 );
#else
		FancyQuad_t q[4];
#endif
		q[ 0 ].FqInit( Vector2D(0.0f, 0.0f),
					 Vector2D(m_rgBorderWidths[ 3 ], m_rgBorderWidths[ 0 ]),
					 Vector2D(m_flLayerWidth - m_rgBorderWidths[ 1 ], m_rgBorderWidths[ 0 ]),
					 Vector2D(m_flLayerWidth, 0.0f),
					 VecColorFromABGRPreMul( m_rgbaBorderColors[ 0 ] ) );

		q[ 1 ].FqInit( Vector2D(m_flLayerWidth - m_rgBorderWidths[ 1 ], m_rgBorderWidths[ 0 ]),
					 Vector2D(m_flLayerWidth, 0.0f),
					 Vector2D(m_flLayerWidth, m_flLayerHeight),
					 Vector2D(m_flLayerWidth - m_rgBorderWidths[ 1 ], m_flLayerHeight - m_rgBorderWidths[ 2 ]),
					 VecColorFromABGRPreMul( m_rgbaBorderColors[ 1 ] ) );

		q[ 2 ].FqInit( Vector2D(m_flLayerWidth - m_rgBorderWidths[ 1 ], m_flLayerHeight - m_rgBorderWidths[ 2 ]),
					 Vector2D(m_flLayerWidth, m_flLayerHeight),
					 Vector2D(0.0f, m_flLayerHeight),
					 Vector2D(m_rgBorderWidths[ 3 ], m_flLayerHeight - m_rgBorderWidths[ 2 ]),
					 VecColorFromABGRPreMul( m_rgbaBorderColors[ 2 ] ) );

		q[ 3 ].FqInit( Vector2D(m_rgBorderWidths[ 3 ], m_flLayerHeight - m_rgBorderWidths[ 2 ]),
					 Vector2D(0.0f, m_flLayerHeight),
					 Vector2D(0.0f, 0.0f),
					 Vector2D(m_rgBorderWidths[ 3 ], m_rgBorderWidths[ 0 ]),
					 VecColorFromABGRPreMul( m_rgbaBorderColors[ 3 ] ) );

		// pre transform (since we're optimising out VS consts)
		PreXFormFancyQuadPositions( &renderAttributes, q, nullptr, m_flLayerWidth, m_flLayerHeight, 4 );

		#if ( PANDX_DRAW )
		if ( !g_bPanDx )
		#endif
		{
			RenderViewport_t viewport;
			m_pParentSurface->m_pRenderContext->GetViewport( &viewport, 0 );
			if ( viewport.m_nHeight == 0 )
				return;

			const IMaterial2 *pMaterial = m_pParentSurface->m_hFancyQuadMaterial;
			IMaterialMode *pMode = pMaterial->GetMode();
			if ( !pMode )
			{
				Log_Warning( LOG_PANORAMA, "CSource2Surface::DrawFancyQuad() GetMode == NULL? Can't Render\n" );
				return;
			}

			int nVerts = 4 * 2 * 3;
			CDynamicVertexData< Source2FancyQuadVertex_t > triVB( m_pParentSurface->m_pRenderContext, nVerts, "PanoramaFancyQuad", "Panorama" );

			// Cut off anything outside the viewport
	//APS	Rect_t rectScissor( viewport.m_nTopLeftX, viewport.m_nTopLeftY, viewport.m_nWidth, viewport.m_nHeight );
	//APS	m_pRenderContext->SetScissorRect( rectScissor );

			int vertidx[] = { 0,2,1,0,3,2 };

			for ( int i = 0; i < 4; i++ )
			{
				FancyQuad_t *pFancyQuad = q + i;

				for ( int &j : vertidx )
				{
					triVB->m_vecPosition.Init( pFancyQuad->m_Verts[ j ].m_vPosition.x, pFancyQuad->m_Verts[ j ].m_vPosition.y, 0.0f, 1.0f );
					triVB->m_vecColor0 = pFancyQuad->m_vColor;
					triVB->m_vecColor1 = pFancyQuad->m_vColorStop;
					triVB->m_vecTexCoordGradientCoord.Init( pFancyQuad->m_Verts[ j ].m_vUV.x, pFancyQuad->m_Verts[ j ].m_vUV.y,
															pFancyQuad->m_Verts[ j ].m_vUVGradient.x, pFancyQuad->m_Verts[ j ].m_vUVGradient.y );
					triVB->m_vecOpacityTexCoord.Init( pFancyQuad->m_Verts[ j ].m_vUVOpacity.x, pFancyQuad->m_Verts[ j ].m_vUVOpacity.y, 0, 0 );
					triVB->m_vecFragCoordWdHt = pFancyQuad->m_Verts[ j ].m_vFragCoordWdHt;

					triVB.AdvanceVertex();
				}
			}

			triVB.UnlockAndBind( 0, 0 );

			// Draw batch
			MaterialRenderablePass_t passes[ MATERIAL_RENDERABLE_PASS_MAX ];
			int nPasses = pMode->ComputeRenderablePassesForContext( &renderAttributes, m_pParentSurface->m_pRenderContext, passes );
			for ( int i = 0; i < nPasses; i++ )
			{
				g_pMaterialSystem2->SetRenderStateForRenderablePass( &renderAttributes, m_pParentSurface->m_pRenderContext, m_pParentSurface->m_hSource2FancyQuadVertexLayout, passes[ i ] );

				m_pParentSurface->m_pRenderContext->SetCullMode( RENDER_CULLMODE_CULL_NONE );
				m_pParentSurface->m_pRenderContext->SetZBufferMode( RENDER_ZBUFFER_NONE );
				m_pParentSurface->m_pRenderContext->SetBlendState( m_pParentSurface->m_hCurrentBlendState );

				// PANDRAWCALL FQ -- CSource2CompositionLayer.DrawBorder
				m_pParentSurface->m_pRenderContext->CtxDraw( RENDER_PRIM_TRIANGLES, 0, nVerts );
				m_pParentSurface->GetFrameStats().m_nDrawBorderCalls++;
			}

		}
		#if ( PANDX_DRAW )
		else
		{
			m_pParentSurface->m_pRenderContext->SetCullMode( RENDER_CULLMODE_CULL_NONE );
			m_pParentSurface->m_pRenderContext->SetZBufferMode( RENDER_ZBUFFER_NONE );
			m_pParentSurface->m_pRenderContext->SetBlendState( m_pParentSurface->m_hCurrentBlendState );
			( (CRenderContext*)m_pParentSurface->m_pRenderContext )->m_pAttr = &renderAttributes;

			//PANDRAWCALL FQ
			PanDxDrawFancyQuads( (CRenderContext*)m_pParentSurface->m_pRenderContext, q, 4 );
		}
		#endif

	}
	else
	{
		// forestw: surrounded by a border color, with rounded inner corners
		AssertMsg( m_rgbaBorderColors[0] == m_rgbaBorderColors[1] && m_rgbaBorderColors[0] == m_rgbaBorderColors[2] && m_rgbaBorderColors[0] == m_rgbaBorderColors[3], "DrawBorder with rounded corners must use same color on all sides" );

		FancyQuadParameters_t FancyParam( FancyQuadFlag_InnerCorner | FancyQuadFlag_HasBorder );
		FancyParam.m_flZ = 0.0f;
		FancyParam.m_flVertexMin[0] = 0;
		FancyParam.m_flVertexMin[1] = 0;
		FancyParam.m_flVertexMax[0] = m_flLayerWidth;
		FancyParam.m_flVertexMax[1] = m_flLayerHeight;
		FancyParam.m_flTexCoordMin[0] = 0.0f;
		FancyParam.m_flTexCoordMin[1] = 0.0f;
		FancyParam.m_flTexCoordMax[0] = 1.0f;
		FancyParam.m_flTexCoordMax[1] = 1.0f;
		FancyParam.m_flCornerRadii[0][0] = MAX( 1.0f, m_rgCornerRadii[0] - m_rgBorderWidths[3] ); //inner
		FancyParam.m_flCornerRadii[0][1] = MAX( 1.0f, m_rgCornerRadii[1] - m_rgBorderWidths[0] );
		FancyParam.m_flCornerRadii[1][0] = MAX( 1.0f, m_rgCornerRadii[2] - m_rgBorderWidths[1] );
		FancyParam.m_flCornerRadii[1][1] = MAX( 1.0f, m_rgCornerRadii[3] - m_rgBorderWidths[0] );
		FancyParam.m_flCornerRadii[2][0] = MAX( 1.0f, m_rgCornerRadii[4] - m_rgBorderWidths[1] );
		FancyParam.m_flCornerRadii[2][1] = MAX( 1.0f, m_rgCornerRadii[5] - m_rgBorderWidths[2] );
		FancyParam.m_flCornerRadii[3][0] = MAX( 1.0f, m_rgCornerRadii[6] - m_rgBorderWidths[3] );
		FancyParam.m_flCornerRadii[3][1] = MAX( 1.0f, m_rgCornerRadii[7] - m_rgBorderWidths[2] );
		FancyParam.m_flBorderWidth[0] = m_rgBorderWidths[0];
		FancyParam.m_flBorderWidth[1] = m_rgBorderWidths[1];
		FancyParam.m_flBorderWidth[2] = m_rgBorderWidths[2];
		FancyParam.m_flBorderWidth[3] = m_rgBorderWidths[3];
		Vector4D vec = VecColorFromABGR( m_rgbaBorderColors[0] );
		FancyParam.m_flBorderColor[0] = vec.x * vec.w;
		FancyParam.m_flBorderColor[1] = vec.y * vec.w;
		FancyParam.m_flBorderColor[2] = vec.z * vec.w;
		FancyParam.m_flBorderColor[3] = vec.w;
		// color is transparent black but the FancyQuad.m_flBorderColor will override in the right areas

		FancyQuadBrush_t FancyBrush(0.0, 0.0, 0.0, 0.0);

		FancyQuadDraw_t fancyQuadDraw;
		fancyQuadDraw.m_nWide = m_flLayerWidth;
		fancyQuadDraw.m_nTall = m_flLayerHeight;
		fancyQuadDraw.m_pQuadParameters = &FancyParam;
		fancyQuadDraw.m_pQuadBrush = &FancyBrush;

		pBaseSurface->DrawFancyQuad( &fancyQuadDraw );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called to push a transform matrix
//-----------------------------------------------------------------------------
void CSource2CompositionLayer::PushPanelContextInLayer( const PushPanelContextInLayerRenderCommand_t &renderCommand )
{
	int iContext = m_vecPanelContext.AddToTail();
	PanelContext_t &context = m_vecPanelContext[iContext];

	if ( renderCommand.transform )
	{
		RenderMatrixToVMatrix( context.m_VMatrix, *renderCommand.transform, NULL );
	}
	else
	{
		context.m_VMatrix = VMatrix::GetIdentityMatrix();
	}

	if ( m_vecPanelContext.Count() > 1 )
		context.m_VMatrix = m_vecPanelContext[m_vecPanelContext.Count() - 2].m_VMatrix * context.m_VMatrix;

	context.m_flLayerWidth = renderCommand.width;
	context.m_flLayerHeight = renderCommand.height;
	context.m_flPositionX = renderCommand.position.x;
	context.m_flPositionY = renderCommand.position.y;
	context.m_flPositionZ = renderCommand.position.z;
	context.m_flScrollX = renderCommand.scroll_offset.x;
	context.m_flScrollY = renderCommand.scroll_offset.y;

	if ( renderCommand.box_shadow )
	{
		context.m_bBoxShadowInset = renderCommand.box_shadow->inset;
		context.m_bBoxShadowFill = renderCommand.box_shadow->fill;
		context.m_flBoxShadowHorOffset = renderCommand.box_shadow->horizontal_offset;
		context.m_flBoxShadowVerOffset = renderCommand.box_shadow->vertical_offset;
		context.m_flBoxShadowBlurRadius = renderCommand.box_shadow->blur_radius;
		context.m_flBoxShadowSpreadDistance = renderCommand.box_shadow->spread_distance;
		context.m_rgbaBoxShadowColor = renderCommand.box_shadow->color;
	}
	else
	{
		context.m_bBoxShadowInset = false;
		context.m_bBoxShadowFill = false;
		context.m_flBoxShadowHorOffset = 0.0f;
		context.m_flBoxShadowVerOffset = 0.0f;
		context.m_flBoxShadowBlurRadius = 0.0f;
		context.m_flBoxShadowSpreadDistance = 0.0f;
		context.m_rgbaBoxShadowColor = 0x00000000;
	}

	if ( renderCommand.border )
	{
		context.m_rgBorderWidths[ 0 ] = renderCommand.border->top.width;
		context.m_rgBorderWidths[ 1 ] = renderCommand.border->right.width;
		context.m_rgBorderWidths[ 2 ] = renderCommand.border->bottom.width;
		context.m_rgBorderWidths[ 3 ] = renderCommand.border->left.width;

		context.m_rgbaBorderColors[ 0 ] = renderCommand.border->top.color;
		context.m_rgbaBorderColors[ 1 ] = renderCommand.border->right.color;
		context.m_rgbaBorderColors[ 2 ] = renderCommand.border->bottom.color;
		context.m_rgbaBorderColors[ 3 ] = renderCommand.border->left.color;
	}
	else
	{
		V_memset( context.m_rgBorderWidths, 0, sizeof( context.m_rgBorderWidths ) );
		V_memset( context.m_rgbaBorderColors, 0, sizeof( context.m_rgbaBorderColors ) );
	}

	ColorFromABGR( context.m_multColor.x, context.m_multColor.y, context.m_multColor.z, context.m_multColor.w, renderCommand.composition_color );
}


//-----------------------------------------------------------------------------
// Purpose: Called to pop a transform matrix
//-----------------------------------------------------------------------------
void CSource2CompositionLayer::PopPanelContextInLayer( const PopPanelContextInLayerRenderCommand_t &renderCommand )
{
	if ( m_vecPanelContext.Count() > 0 )
	{
		m_vecPanelContext.Remove( m_vecPanelContext.Count() - 1 );
	}
	else
	{
		AssertMsg( false, "Called CCompositionLayer::PopTransformMatrix with no matrix pushed" );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Push a new clip layer into the composition layer
//-----------------------------------------------------------------------------
void CSource2CompositionLayer::PushClipLayer( const PushClipLayerRenderCommand_t &renderCommand )
{
	RectBounds_t &bounds = m_vecClipLayers[ m_vecClipLayers.AddToTail() ];
	bounds.left = renderCommand.top_left.x;
	bounds.top = renderCommand.top_left.y;
	bounds.right = renderCommand.bottom_right.x;
	bounds.bottom = renderCommand.bottom_right.y;

	// Check that we are constrained to parent layer if one has been pushed
	if ( m_vecClipLayers.Count() > 1 )
	{
		RectBounds_t &parentBounds = m_vecClipLayers.Element( m_vecClipLayers.Count() - 2 );

        bounds.left = clamp( bounds.left, parentBounds.left, parentBounds.right );
        bounds.top = clamp( bounds.top, parentBounds.top, parentBounds.bottom );
        bounds.right = clamp( bounds.right, parentBounds.left, parentBounds.right );
        bounds.bottom = clamp( bounds.bottom, parentBounds.top, parentBounds.bottom );
	}
	else
	{
		// Constrained by the composition layer dimension
		bounds.left = clamp( bounds.left, 0, m_flLayerWidth );
		bounds.top = clamp( bounds.top, 0, m_flLayerHeight );
		bounds.right = clamp( bounds.right, 0, m_flLayerWidth );
		bounds.bottom = clamp( bounds.bottom, 0, m_flLayerHeight );
	}
}


//-----------------------------------------------------------------------------
// Purpose:Pop a clip layer out of the composition layer
//-----------------------------------------------------------------------------
void CSource2CompositionLayer::PopClipLayer()
{
	if ( m_vecClipLayers.Count() > 0 )
	{		
		m_vecClipLayers.Remove( m_vecClipLayers.Count() - 1 );
	}
	else
	{
		AssertMsg( false, "Called CCompositionLayer::PopClipLayer with no clip layers pushed" );
	}
}


typedef const LinearGradient_t * CLinearGradientMapKey;
bool CLinearGradientLessThan( const CLinearGradientMapKey &lhs, const CLinearGradientMapKey &rhs )
{
	if ( lhs->start_position.x < rhs->start_position.x )
		return true;
	else if ( lhs->start_position.x > rhs->start_position.x )
		return false;

	if ( lhs->start_position.y < rhs->start_position.y )
		return true;
	else if ( lhs->start_position.y > rhs->start_position.y )
		return false;


	if ( lhs->end_position.x < rhs->end_position.x )
		return true;
	else if ( lhs->end_position.x > rhs->end_position.x )
		return false;

	if ( lhs->end_position.y < rhs->end_position.y )
		return true;
	else if ( lhs->end_position.y > rhs->end_position.y )
		return false;

	const ColorStop_t *pLeftStop = lhs->color_stop.GetFirst();
	const ColorStop_t *pRightStop = rhs->color_stop.GetFirst();

	while ( pLeftStop && pRightStop )
	{
		if ( pLeftStop->color_rgba < pRightStop->color_rgba )
			return true;
		else if ( pLeftStop->color_rgba > pRightStop->color_rgba )
			return false;

		if ( pLeftStop->color_rgba < pRightStop->color_rgba )
			return true;
		else if ( pLeftStop->color_rgba > pRightStop->color_rgba )
			return false;

		pLeftStop = lhs->color_stop.GetNext( pLeftStop );
		pRightStop = lhs->color_stop.GetNext( pRightStop );
	}

	return pRightStop != nullptr;
}



//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CSource2Surface::CSource2Surface( const char *pName )
{
	m_SurfaceName = pName;
	m_flScaleFactor = 1.0;
	m_pCursorRender = NULL;
	m_flCurrentRenderFrameTime = 0.0;
	m_flLastPaintFrameTime = 0.0;
	m_flScaleBackbufferX = 1.0f;
	m_flTranslateBackbufferX = 0.0f;
	m_flScaleBackbufferY = 1.0f;
	m_flTranslateBackbufferY = 0.0f;
	m_bEnforceAspectRatio = false;
	m_bUseBackBufferDirectly = s_convarPanoramaUseBackbufferDirectly.GetBool();

	m_hResourceManifest = RESOURCE_MANIFEST_HANDLE_INVALID;

	m_hMaterial = MATERIAL_HANDLE_INVALID;
	m_hFancyQuadMaterial = MATERIAL_HANDLE_INVALID;

	m_hSource2FancyQuadVertexLayout = RENDER_INPUT_LAYOUT_INVALID;
	m_hSource2VertexTexturedLayout = RENDER_INPUT_LAYOUT_INVALID;

	m_hAlphaBlendState = RENDER_BLEND_STATE_HANDLE_INVALID;
	m_hPremultipliedAlphaBlendState = RENDER_BLEND_STATE_HANDLE_INVALID;
	m_hAlphaOnlyBlendState = RENDER_BLEND_STATE_HANDLE_INVALID;
	m_hMixScreenState	= RENDER_BLEND_STATE_HANDLE_INVALID;
	m_hMixMultiplyState	= RENDER_BLEND_STATE_HANDLE_INVALID;

	m_hCurrentBlendState = RENDER_BLEND_STATE_HANDLE_INVALID;
	m_nBlendStateOverridden = 0;
	m_eCurrentFractionalPixelPositions = k_EFractionalPixelPositionsDefault;

	m_pSceneLayer = NULL;
	m_pRenderContext = NULL;
	m_pSceneView = NULL;

	m_pTextTextureCache = NULL;
    m_pTextLayoutDrawCache = NULL;

    m_nGradientTextures = 0;
	V_memset( m_GradientTextureBuffer, 0, sizeof( m_GradientTextureBuffer ) );

	if ( V_strstr( pName, "CSGOMainMenu" ) )
	{
		m_bMainMenu = true;
	}
	else
	{
		m_bMainMenu = false;
	}

	InitBlurLayerCache();
	
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CSource2Surface::~CSource2Surface()
{
	TermBlurLayerCache();

	DestroyDeviceResources( true );

	// Should only have the backbuffer layer in stack, or zero
	Assert( m_stackCompositionLayers.Count() <= 1 );
	FOR_EACH_VEC( m_stackCompositionLayers, i )
	{
		SAFE_DELETE( m_stackCompositionLayers[i] );
	}
	m_stackCompositionLayers.RemoveAll();

	g_IUITextServices->FreeTextTextureCache( m_pTextTextureCache );
	g_IUITextServices->FreeTextLayoutDrawCache( m_pTextLayoutDrawCache );

// WIP:
//	g_pRenderDevice->RemoveTextureResidencyListener( this );
//	

}

//-----------------------------------------------------------------------------
// Purpose: Initialization
//-----------------------------------------------------------------------------
bool CSource2Surface::BInitialize( int nSurfaceWidth, int nSurfaceHeight, int nWindowWidth, int nWindowHeight, bool bEnforceAspectRatio, bool bFixedSurfaceSize, CMouseCursorRender *pCursorRender )
{
	VPROF_BUDGET_THREAD( "CSource2Surface::BInitialize", VPROF_BUDGETGROUP_TENFOOT );

	m_unSurfaceWidth = nSurfaceWidth;
	m_unSurfaceHeight = nSurfaceHeight;
	m_unWindowWidth = nWindowWidth;
	m_unWindowHeight = nWindowHeight;
	m_bFixedSurfaceSize = bFixedSurfaceSize;
	m_bEnforceAspectRatio = bEnforceAspectRatio;

	m_pCursorRender = pCursorRender;

	if ( !g_IUITextServices )
	{
		return false;
	}

	m_pTextTextureCache = g_IUITextServices->CreateTextTextureCache( this );
	m_pTextLayoutDrawCache = g_IUITextServices->CreateTextLayoutDrawCache( this );
	g_IUITextServices->SetDebugFontSelection( s_convarDebugFontSelection.GetBool() );


// WIP:
//	g_pRenderDevice->AddTextureResidencyListener( this );

	ComputeBackbufferScaling();

	if ( !BCreateDeviceResources() )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Compute back buffer scaling/translation factors
//-----------------------------------------------------------------------------
void CSource2Surface::ComputeBackbufferScaling()
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
// Purpose: Destroy device resources
//-----------------------------------------------------------------------------
void CSource2Surface::DestroyDeviceResources( bool bShuttingDown )
{
	if ( m_hMaterial != MATERIAL_HANDLE_INVALID )
	{
		m_hMaterial.Shutdown();
		m_hMaterial = MATERIAL_HANDLE_INVALID;
	}

	if ( m_hFancyQuadMaterial != MATERIAL_HANDLE_INVALID )
	{
		m_hFancyQuadMaterial.Shutdown();
		m_hFancyQuadMaterial = MATERIAL_HANDLE_INVALID;
	}

    for ( int i = 0; i < m_nGradientTextures; i++ )
    {
		m_GradientTextures[i].m_hTexture.Shutdown();
	}
    m_nGradientTextures = 0;

	m_hSource2FancyQuadVertexLayout = RENDER_INPUT_LAYOUT_INVALID;
	m_hSource2VertexTexturedLayout = RENDER_INPUT_LAYOUT_INVALID;


	if ( m_hResourceManifest != RESOURCE_MANIFEST_HANDLE_INVALID )
	{
		g_pResourceSystem->DestroyResourceManifest( m_hResourceManifest );
		g_pResourceSystem->UpdateSimple();
		m_hResourceManifest = RESOURCE_MANIFEST_HANDLE_INVALID;
	}

	if ( m_hAlphaBlendState != RENDER_BLEND_STATE_HANDLE_INVALID )
	{
		m_hAlphaBlendState = RENDER_BLEND_STATE_HANDLE_INVALID;
	}

	if ( m_hPremultipliedAlphaBlendState != RENDER_BLEND_STATE_HANDLE_INVALID )
	{
		m_hPremultipliedAlphaBlendState = RENDER_BLEND_STATE_HANDLE_INVALID;
	}

	if ( m_hAlphaOnlyBlendState != RENDER_BLEND_STATE_HANDLE_INVALID )
	{
		m_hAlphaOnlyBlendState = RENDER_BLEND_STATE_HANDLE_INVALID;
	}


	{	
//		AUTO_LOCK( m_lockTextureMap );
//		m_mapTextures.PurgeAndDeleteElements();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Create device resources
//-----------------------------------------------------------------------------
bool CSource2Surface::BCreateDeviceResources()
{
	Log_Detailed( LOG_PANORAMA, "CSource2Surface::BCreateDeviceResources()\n" );

	bool bSuccess = true;

	const char *resources[] =
	{
		"materials/panorama/panorama.vmat",
		"materials/panorama/panorama_fancyquad.vmat",
	};

	m_hResourceManifest = g_pResourceSystem->CreateResourceManifest( ARRAYSIZE( resources ), resources, RESOURCE_MANIFEST_LOAD_STREAMING_DATA, "Panorama" );
	g_pResourceSystem->ForceSynchronizationAndBlockUntilManifestLoaded( m_hResourceManifest ); 

	m_hMaterial = g_pMaterialSystem2->FindOrCreateMaterialFromResource( "materials/panorama/panorama.vmat" ); 
	if ( !m_hMaterial.IsValid() || !m_hMaterial.IsLoaded() )
		bSuccess = false;

	m_hFancyQuadMaterial = g_pMaterialSystem2->FindOrCreateMaterialFromResource( "materials/panorama/panorama_fancyquad.vmat" );
	if ( !m_hFancyQuadMaterial.IsValid() || !m_hFancyQuadMaterial.IsLoaded() )
		bSuccess = false;

	m_hSource2FancyQuadVertexLayout = RIL_FANCY;
	m_hSource2VertexTexturedLayout = RIL_STDVERT;

	m_hAlphaBlendState = BLENDSTATE_ALPHA;
	m_hPremultipliedAlphaBlendState = BLENDSTATE_PREMULT_ALPHA;
	m_hAlphaOnlyBlendState = BLENDSTATE_ONLY_ALPHA;
	m_hMixScreenState = BLENDSTATE_MIX_SCREEN;
	m_hMixMultiplyState = BLENDSTATE_MIX_MULTIPLY;
	m_hMixAdditiveState = BLENDSTATE_MIX_ADDITIVE;
	m_hMixAdditiveSRGBState = BLENDSTATE_MIX_ADDITIVESRGB;
	m_hMixOpaqueState = BLENDSTATE_MIX_OPAQUE;

	return bSuccess;
}

//-----------------------------------------------------------------------------
// Purpose: Create resources that are resizable
//-----------------------------------------------------------------------------
bool CSource2Surface::BRecreateBaseCompositionLayer()
{
	VPROF_BUDGET_THREAD( "CSource2Surface::BRecreateBaseCompositionLayer", VPROF_BUDGETGROUP_TENFOOT );

	m_bUseBackBufferDirectly = s_convarPanoramaUseBackbufferDirectly.GetBool();

	// Should only have the backbuffer layer in stack, or zero
	Assert( m_stackCompositionLayers.Count() <= 1 );
	FOR_EACH_VEC( m_stackCompositionLayers, i )
	{
		SAFE_DELETE( m_stackCompositionLayers[i] );
	}
	m_stackCompositionLayers.RemoveAll();

	// Setup top level of composition layers
	Assert( m_stackCompositionLayers.Count() == 0 );
	
	CSource2CompositionLayer *pLayer = NULL;
	if ( BBackBufferScalingNeeded() || !m_bUseBackBufferDirectly )
		pLayer = new CSource2CompositionLayer( this, m_unSurfaceWidth, m_unSurfaceHeight, false, false, false, false, "base" );
	else
		pLayer = new CSource2CompositionLayer( this, m_unSurfaceWidth, m_unSurfaceHeight, false, false, true, false, "base" );

	m_stackCompositionLayers.AddToTail( pLayer );
	
	BasicQuad_t *pQuad = pLayer->AccessRenderQuad();

	pQuad->BqInit( Vector2D( 0.0, 0.0 ), Vector2D( m_unSurfaceWidth, m_unSurfaceHeight ) );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Determine if we need to resize the backbuffer and do it
//-----------------------------------------------------------------------------
bool CSource2Surface::BUpdateWindowSizeIfNeeded( uint32 nWidth, uint32 nHeight )
{
	if ( nHeight != m_unWindowHeight || nWidth != m_unWindowWidth )
	{
		m_unWindowWidth = nWidth;
		m_unWindowHeight = nHeight;
		if ( !m_bFixedSurfaceSize )
		{
			m_unSurfaceWidth = nWidth;
			m_unSurfaceHeight = nHeight;
		}

		return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Called at the start of every frame. Should setup general state for 
// the frame, call beginscene(), etc.
//-----------------------------------------------------------------------------
void CSource2Surface::BeginFrame( const BeginFrameRenderCommand_t &renderCommand )
{

	m_flCurrentRenderFrameTime = Plat_FloatTime();

	m_frameStats.InitZero();
	m_frameStats.m_nFramesRendered++;
	m_frameStats.m_flFrameTime = Plat_FloatTime();

	if ( renderCommand.clear_gpu_resources_before_frame )
	{
		ClearGPUResources();
	}

	if ( !renderCommand.empty_frame )
	{
#if ( PANDX_DRAW )
		g_bPanDx = s_convarPanDx.GetBool();

		if ( g_bPanDx ) PanDxOwnDx();
#endif

		if( s_bClearTextCacheBeforeNextFrame )
		{
			m_pTextLayoutDrawCache->Clear();
			s_bClearTextCacheBeforeNextFrame = false;
		}

		// If we don't have our base composition layer it needs to be recreated now
		if ( m_stackCompositionLayers.Count() == 0 )
			DbgVerify( BRecreateBaseCompositionLayer() );

		uint32 nWidth = renderCommand.surface_width;
		uint32 nHeight = renderCommand.surface_height;
		if ( BUpdateWindowSizeIfNeeded( nWidth, nHeight ) || m_bUseBackBufferDirectly != s_convarPanoramaUseBackbufferDirectly.GetBool() )
		{
			ComputeBackbufferScaling();
			DbgVerify( BRecreateBaseCompositionLayer() );
		}

		ActivateBackBufferRenderTarget();

		if ( m_stackCompositionLayers.Count() == 0 )
		{
			Assert( m_stackCompositionLayers.Count() != 0 );
		}
		else
		{
			CSource2CompositionLayer *pLayer = m_stackCompositionLayers[m_stackCompositionLayers.Count() - 1];
			if ( !pLayer->BIsBackbuffer() )
				pLayer->Clear();

			if ( m_stackCompositionLayers.Count() == 1 )
				pLayer->PushCliplayersAndBeginDraw( m_flScaleBackbufferX, m_flScaleBackbufferY, m_flTranslateBackbufferX, m_flTranslateBackbufferY );
			else
				pLayer->PushCliplayersAndBeginDraw( 1.0f, 1.0f, 0.0f, 0.0f );

		}

		m_eCurrentFractionalPixelPositions = k_EFractionalPixelPositionsDefault;
	}

	m_flLastPaintFrameTime = renderCommand.frame_paint_time;
}


//-----------------------------------------------------------------------------
// Purpose: Called to clear the backbuffer to a solid color.
//-----------------------------------------------------------------------------
void CSource2Surface::ClearBackbuffer( const ClearBackbufferRenderCommand_t &renderCommand )
{
	// EXPLICITLY NOT CLEARING THE PROVIDED ROOT BACK BUFFER.
	// Composition needs to be aware that S2 is providing a back buffer with existing data that
	// Panorama layers on top of. The EndFrame() final blit alpha blends onto the back buffer.

	//ActivateRenderTarget();
}


//-----------------------------------------------------------------------------
// Purpose: Draw a textured quad
//-----------------------------------------------------------------------------
void CSource2Surface::DrawTexturedQuadInternal( HMaterial hMaterial, CRenderAttributes *pRenderAttributes, HRenderTexture hTextureID, BasicQuad_t *pBasicQuad )
{
	// pre transform (since we're optimising out VS consts)
	Vector4D vWidth, vHeight;
	pRenderAttributes->GetValue( &vWidth, ATTR_ViewportWidth );
	pRenderAttributes->GetValue( &vHeight, ATTR_ViewportHeight );

	// use a local copy since we're transforming the positions on the CPU instead of GPU
	PreXFormBasicQuadPositions( pRenderAttributes, pBasicQuad, vWidth.x, vHeight.x );

#if ( PANDX_DRAW )
	if ( !g_bPanDx )
#endif
	{
		VPROF( "CSource2Surface::DrawTexturedQuadInternal" );

		RenderViewport_t viewport;
		m_pRenderContext->GetViewport( &viewport, 0 );
		if ( viewport.m_nHeight == 0 )
			return;

		const IMaterial2 *pMaterial = m_hMaterial;
		IMaterialMode *pMode = pMaterial->GetMode();
		if ( !pMode )
		{
			Log_Warning( LOG_PANORAMA, "CSource2Surface::DrawTexturedQuadInternal() GetMode == NULL? Can't Render\n" );
			return;
		}

		int nVerts = 2 * 3;
		CDynamicVertexData< Source2BasicQuadVertex_t > triVB( m_pRenderContext, nVerts, "PanoramaTexturedQuad", "Panorama" );

		// Top left -> bottom right -> top right 
		// top left -> bottom left -> bottom right 
		int vertidx[] = { 0,2,1,0,3,2 };
		for ( int &i : vertidx )
		{
			triVB->m_vecPosition.Init( pBasicQuad->m_vPosition[ i ].x, pBasicQuad->m_vPosition[ i ].y, 0.0f, 1.0f );
			triVB->m_vecTex.Init( pBasicQuad->m_vUV[ i ].x, pBasicQuad->m_vUV[ i ].y, 0, 0 );
			triVB->m_vecTex1.Init( 0, 0, 0, 0 );
			triVB->m_vecColor.Init( 0, 0, 0, 0 );
			triVB.AdvanceVertex();
		}

		triVB.UnlockAndBind( 0, 0 );

		pRenderAttributes->SetTextureValue( ATTR_Texture0, hTextureID );

		// Draw batch
		MaterialRenderablePass_t passes[ MATERIAL_RENDERABLE_PASS_MAX ];
		int nPasses = pMode->ComputeRenderablePassesForContext( pRenderAttributes, m_pRenderContext, passes );
		for ( int i = 0; i < nPasses; i++ )
		{
			g_pMaterialSystem2->SetRenderStateForRenderablePass( pRenderAttributes, m_pRenderContext, m_hSource2VertexTexturedLayout, passes[ i ] );

			m_pRenderContext->SetCullMode( RENDER_CULLMODE_CULL_NONE );
			m_pRenderContext->SetZBufferMode( RENDER_ZBUFFER_NONE );
			m_pRenderContext->SetBlendState( m_hCurrentBlendState );

			// PANDRAWCALL STD - CSource2Surface.DrawTexturedQuadInternal
			m_pRenderContext->CtxDraw( RENDER_PRIM_TRIANGLES, 0, nVerts );
			GetFrameStats().m_nDrawTexturedQuadInternalCalls++;
		}
	}

	#if ( PANDX_DRAW )
	else
	{

		RenderViewport_t viewport;
		m_pRenderContext->GetViewport( &viewport, 0 );
		if ( viewport.m_nHeight == 0 )
			return;

		const IMaterial2 *pMaterial = m_hMaterial;
		IMaterialMode *pMode = pMaterial->GetMode();
		if ( !pMode )
		{
			Log_Warning( LOG_PANORAMA, "CSource2Surface::DrawTexturedQuadInternal() GetMode == NULL? Can't Render\n" );
			return;
		}


		pRenderAttributes->SetTextureValue( ATTR_Texture0, hTextureID );

		m_pRenderContext->SetCullMode( RENDER_CULLMODE_CULL_NONE );
		m_pRenderContext->SetZBufferMode( RENDER_ZBUFFER_NONE );
		m_pRenderContext->SetBlendState( m_hCurrentBlendState );

		( (CRenderContext*)m_pRenderContext )->m_pAttr = pRenderAttributes;

		//PANDRAWCALL STD - CSource2Surface.DrawFancyQuad
		PanDxDrawBasicQuad( (CRenderContext*)m_pRenderContext, pBasicQuad );
	}

	#endif
}


bool CSource2Surface::FancyQuadGradientTexture_t::MatchesBrush( const FancyQuadBrush_t *pBrush ) const
{
    return m_nStops == pBrush->m_nGradientStops &&
        memcmp( m_flStops, pBrush->m_flGradientStops, m_nStops * sizeof( m_flStops[0] ) ) == 0 &&
        memcmp( m_flColor, pBrush->m_flColor, m_nStops * sizeof( m_flColor[0] ) ) == 0;
}

void CSource2Surface::FancyQuadGradientTexture_t::SetFromBrush( const FancyQuadBrush_t *pBrush, HRenderTexture hTexture )
{
    m_hTexture = hTexture;
    m_nStops = pBrush->m_nGradientStops;
    m_nAge = 0;
    memcpy( m_flStops, pBrush->m_flGradientStops, m_nStops * sizeof( m_flStops[0] ) );
    memcpy( m_flColor, pBrush->m_flColor, m_nStops * sizeof( m_flColor[0] ) );
}

//-----------------------------------------------------------------------------
// Purpose: Update gradient texture to match brush
//-----------------------------------------------------------------------------
HRenderTexture CSource2Surface::UpdateFancyQuadGradientTexture( const FancyQuadBrush_t *pFancyQuadBrush )
{
	int iCurrentTexel;
	int iCurrentStop = 0;
	int iLastStopUpdated = -1;
	float pCurrentColor[4] = { 0, 0, 0, 0 };
	float pNextColor[4] = { 0, 0, 0, 0 };
	float flCurrentStopSpacing = 0.0f;
	float flCurrentStopProgression;
    int nOldest = -1;
    int nOldestAge = 0;

    // Check the cache for an existing texture which matches the brush.
    for ( int i = 0; i < m_nGradientTextures; i++ )
    {
        if ( m_GradientTextures[i].MatchesBrush( pFancyQuadBrush ) )
        {
            // Reset the found entry's age for LRU purposes.
            m_GradientTextures[i].m_nAge = 0;

            // Increment the age of the remaining entries.
            for ( int j = i + 1; j < m_nGradientTextures; j++ )
            {
                if ( m_GradientTextures[j].m_nAge < INT_MAX )
                {
                    m_GradientTextures[j].m_nAge++;
                }
            }

            return m_GradientTextures[i].m_hTexture;
        }

        if ( m_GradientTextures[i].m_nAge < INT_MAX )
        {
            m_GradientTextures[i].m_nAge++;
        }
        if ( nOldest < 0 ||
             m_GradientTextures[i].m_nAge > nOldestAge )
        {
            nOldest = i;
            nOldestAge = m_GradientTextures[i].m_nAge;
        }
    }
    
	// Our gradient space goes from 0.0 to 1.0, this is the position difference
	// between each texel in our gradient texture in gradient space.
	const float flGradientTexelScale = 1.0f / (FANCYQUAD_GRADIENT_TEXTURE_SIZE - 1);
	
	float flCurrentGradientPosition;
	
	Assert( pFancyQuadBrush->m_nGradientStops <= FANCYQUAD_MAXSTOPS );
	
	iCurrentTexel = 0;
	
	// Iterate over each texel in our gradient texture and compute its color in
	// gradient space.
	while ( iCurrentTexel < FANCYQUAD_GRADIENT_TEXTURE_SIZE )
	{
		// How far this texel is into gradient space.
		flCurrentGradientPosition = iCurrentTexel * flGradientTexelScale;
		
		// Skip ahead if we passed stop(s).
		while ( pFancyQuadBrush->m_flGradientStops[iCurrentStop + 1] < flCurrentGradientPosition )
		{
			iCurrentStop++;
		}
		
		// Last stop ought to be 1.0, so we can't have passed it.
		Assert( iCurrentStop < pFancyQuadBrush->m_nGradientStops );
		
		// If we're between two stops we've never seen, stash their colors for
		// easy interpolation later.
		if ( iLastStopUpdated != iCurrentStop )
		{
			for ( int c = 0; c < 4; c++ )
			{
				pCurrentColor[c] = pFancyQuadBrush->m_flColor[iCurrentStop][c];
				pNextColor[c] = pFancyQuadBrush->m_flColor[iCurrentStop + 1][c];
			}

			flCurrentStopSpacing = 	pFancyQuadBrush->m_flGradientStops[iCurrentStop + 1] -
									pFancyQuadBrush->m_flGradientStops[iCurrentStop];

			iLastStopUpdated = iCurrentStop;
		}

		Assert( flCurrentGradientPosition >= pFancyQuadBrush->m_flGradientStops[iCurrentStop] );
		Assert( flCurrentGradientPosition <= pFancyQuadBrush->m_flGradientStops[iCurrentStop + 1] );

		// Now, figure out where we stand between our two bounding stops.
		flCurrentStopProgression = 	flCurrentGradientPosition -	pFancyQuadBrush->m_flGradientStops[iCurrentStop];
									
		// Normalize into current stop scale.
		flCurrentStopProgression /= flCurrentStopSpacing;
		
		// Interpolate each component between the two colors according to our
		// local progress in the current stop.
		for ( int c = 0; c < 4; c++ )
		{
			float flInterpolatedColor = pCurrentColor[c] * (1.0f - flCurrentStopProgression) + pNextColor[c] * flCurrentStopProgression;
			m_GradientTextureBuffer[iCurrentTexel][c] = (c < 3) ? ( 255.0f * SrgbLinearToGamma( flInterpolatedColor ) ) : ( 255.0f * flInterpolatedColor );
		}
		
		iCurrentTexel++;
	}

    // Add the new gradient to the cache.
    // If we have space we let the cache grow, otherwise
    // we reuse the oldest texture.
    HRenderTexture hTexture;
    if ( m_nGradientTextures < V_ARRAYSIZE( m_GradientTextures ) )
    {
        // Create a new 1D texture.
		CTextureCreationDesc textureDesc;
		textureDesc.m_nWidth = FANCYQUAD_GRADIENT_TEXTURE_SIZE;
		textureDesc.m_nHeight = 1;
		textureDesc.m_nDepth = 1;
		textureDesc.m_nImageFormat = IMAGE_FORMAT_RGBA8888;
		textureDesc.m_nNumMipLevels = 1;
		textureDesc.m_nMultisampleType = RENDER_MULTISAMPLE_NONE;
		textureDesc.m_nFlags = TSPEC_SUGGEST_CLAMPS | TSPEC_SUGGEST_CLAMPT | TSPEC_NO_LOD;
		textureDesc.m_Reflectivity.Init( 1.0f, 1.0f, 1.0f, 1.0f );
		textureDesc.m_nUsage = TEXTURE_USAGE_DYNAMIC;
		textureDesc.m_nScope = TEXTURE_SCOPE_GLOBAL;
		hTexture = g_pRenderDevice->FindOrCreateTexture( "panorama_fancyquad_gradienttexture.vtex", true, &textureDesc );
        nOldest = m_nGradientTextures++;
    }
    else
    {
        Assert( nOldest >= 0 && nOldest < m_nGradientTextures );
        hTexture = m_GradientTextures[nOldest].m_hTexture;
    }
    
    m_GradientTextures[nOldest].SetFromBrush( pFancyQuadBrush, hTexture );

    // upload the procedural texture
    CTextureCreationDesc dataDesc;
    dataDesc.m_nWidth = FANCYQUAD_GRADIENT_TEXTURE_SIZE;
    dataDesc.m_nHeight = 1;
    dataDesc.m_nNumMipLevels = 1;
    dataDesc.m_nDepth = 1;
    dataDesc.m_nImageFormat = IMAGE_FORMAT_RGBA8888;
    m_pRenderContext->SetTextureData( hTexture, &dataDesc, m_GradientTextureBuffer, sizeof( m_GradientTextureBuffer ) );

    return hTexture;
}


//-----------------------------------------------------------------------------
// Purpose: Draw a fancy quad (gradients, corners), optionally with texture
//-----------------------------------------------------------------------------
static CTSPool<CRenderAttributes> s_FancyQuadAttributesPool;

void CSource2Surface::DrawFancyQuad( const FancyQuadDraw_t *pFancyQuadDraw )
{
	VPROF( "CSource2Surface::DrawFancyQuad" );
	//VPROF_BUDGET( "Panorama DrawFancyQuad", VPROF_BUDGETGROUP_GAME );

	if ( s_convarPanoramaDisableDrawFancyQuad.GetBool() )
		return;

	Assert( m_stackCompositionLayers.Count() );
	if ( !m_stackCompositionLayers.Count() )
		return;

	const FancyQuadParameters_t *pFancyQuadParameters = pFancyQuadDraw->m_pQuadParameters;
	const FancyQuadBrush_t *pFancyQuadBrush = pFancyQuadDraw->m_pQuadBrush;

	CSource2CompositionLayer *pLayer = m_stackCompositionLayers[ m_stackCompositionLayers.Count()-1 ];

	float flWidth = 1.0f;
	float flHeight = 1.0f;
	float flXOffset = 0.0f;
	float flYOffset = 0.0f;
	float flScaledCornerRadii[4][2];
	
	if ( !pFancyQuadDraw->m_bRawCoords )
	{
		flWidth = ( pFancyQuadParameters->m_flVertexMax[0] - pFancyQuadParameters->m_flVertexMin[0] );
		flHeight = ( pFancyQuadParameters->m_flVertexMax[1] - pFancyQuadParameters->m_flVertexMin[1] );
		if ( pFancyQuadDraw->m_flTextureWidth > 0.0f )
		{
			flWidth = pFancyQuadDraw->m_flTextureWidth;
		}
		if ( pFancyQuadDraw->m_flTextureHeight > 0.0f )
		{
			flHeight = pFancyQuadDraw->m_flTextureHeight;
		}
		flXOffset = ( flWidth - ( pFancyQuadDraw->m_flScale2DX * flWidth ) ) / 2.0f;
		flYOffset = ( flHeight - ( pFancyQuadDraw->m_flScale2DY * flHeight ) ) / 2.0f;
		
		for ( int i = 0; i < 4; i++ )
		{
			flScaledCornerRadii[i][1] = pFancyQuadParameters->m_flCornerRadii[i][1] * pFancyQuadDraw->m_flScale2DY;
			flScaledCornerRadii[i][0] = pFancyQuadParameters->m_flCornerRadii[i][0] * pFancyQuadDraw->m_flScale2DX;
		}
	}
	else
	{
		for ( int i = 0; i < 4; i++ )
		{
			flScaledCornerRadii[i][0] = pFancyQuadParameters->m_flCornerRadii[i][0];
			flScaledCornerRadii[i][1] = pFancyQuadParameters->m_flCornerRadii[i][1];
		}
	}
	
	Vector2D vMin;
	Vector2D vMax;
	vMin.x = pFancyQuadParameters->m_flVertexMin[0] + flXOffset;
	vMin.y = pFancyQuadParameters->m_flVertexMin[1] + flYOffset;
	vMax.x = pFancyQuadParameters->m_flVertexMax[0] - flXOffset;
	vMax.y = pFancyQuadParameters->m_flVertexMax[1] - flYOffset;

	float flAdjustedTexCoordMin[2];
	float flAdjustedTexCoordMax[2];
	flAdjustedTexCoordMin[0] = pFancyQuadParameters->m_flTexCoordMin[0] * pFancyQuadDraw->m_flTexture0TexCoordScale[0];
	flAdjustedTexCoordMin[1] = pFancyQuadParameters->m_flTexCoordMin[1] * pFancyQuadDraw->m_flTexture0TexCoordScale[1];
	flAdjustedTexCoordMax[0] = pFancyQuadParameters->m_flTexCoordMax[0] * pFancyQuadDraw->m_flTexture0TexCoordScale[0];
	flAdjustedTexCoordMax[1] = pFancyQuadParameters->m_flTexCoordMax[1] * pFancyQuadDraw->m_flTexture0TexCoordScale[1];


	float flAdjustedOpacityTexCoordMax[ 2 ];
	flAdjustedOpacityTexCoordMax[ 0 ] = pFancyQuadParameters->m_flOpacityTexCoordMax[ 0 ] * pFancyQuadDraw->m_flTexture1TexCoordScale[ 0 ];
	flAdjustedOpacityTexCoordMax[ 1 ] = pFancyQuadParameters->m_flOpacityTexCoordMax[ 1 ] * pFancyQuadDraw->m_flTexture1TexCoordScale[ 1 ];

	CRenderAttributes &renderAttributes = *s_FancyQuadAttributesPool.GetObject();

#if PANDX_DRAW
	FancyQuad_t *pQuad = PanDxGetFancyQuadPtr( 1 );
#else
	FancyQuad_t pQuad[1];
#endif

	pQuad->FqInit( vMin, vMax,
			   *(Vector2D*)flAdjustedTexCoordMin, *(Vector2D*)flAdjustedTexCoordMax,
			   *(Vector2D*)pFancyQuadParameters->m_flOpacityTexCoordMin, *(Vector2D*)flAdjustedOpacityTexCoordMax);
	
	if ( s_convarPanoramaClampFractionalPixelPositions.GetBool() && ( m_eCurrentFractionalPixelPositions != k_EFractionalPixelPositionsNoClamp ) )
	{
		pQuad->RoundPosToInt();
	}

	//renderAttributes.SetVector4DValue( ATTR_TopLeftWdHt, Vector4D( vMin.x, vMin.y, vMax.x - vMin.x, vMax.y - vMin.y ) );

	renderAttributes.SetVector4DValue( ATTR_TopLeftWdHt, Vector4D( pQuad->m_Verts[ 0 ].m_vPosition.x,
																   pQuad->m_Verts[ 0 ].m_vPosition.y,
																   pQuad->m_Verts[ 1 ].m_vPosition.x - pQuad->m_Verts[ 0 ].m_vPosition.x,
																   pQuad->m_Verts[ 3 ].m_vPosition.y - pQuad->m_Verts[ 0 ].m_vPosition.y ) );

	pQuad->m_flZ = pFancyQuadParameters->m_flZ;


	CSource2CompositionLayer::PanelContext_t *pPanelContext = pLayer->AccessPanelContextInLayer();
	if ( !pPanelContext )
	{
		pQuad->m_vColor = *(Vector4D*)pFancyQuadBrush->m_flColor[0];
	}
	else
	{
		pQuad->m_vColor = *(Vector4D*)pFancyQuadBrush->m_flColor[0] * pPanelContext->m_multColor;
	}
	pQuad->m_vColorStop = *(Vector4D*)pFancyQuadBrush->m_flColor[ 1 ];

	// the gradient math needs the original positions
	float flOriginalPosition[ 4 ][ 2 ];
	flOriginalPosition[ 0 ][ 0 ] = pFancyQuadParameters->m_flVertexMin[ 0 ];
	flOriginalPosition[ 0 ][ 1 ] = pFancyQuadParameters->m_flVertexMin[ 1 ];
	flOriginalPosition[ 1 ][ 0 ] = pFancyQuadParameters->m_flVertexMax[ 0 ];
	flOriginalPosition[ 1 ][ 1 ] = pFancyQuadParameters->m_flVertexMin[ 1 ];
	flOriginalPosition[ 2 ][ 0 ] = pFancyQuadParameters->m_flVertexMax[ 0 ];
	flOriginalPosition[ 2 ][ 1 ] = pFancyQuadParameters->m_flVertexMax[ 1 ];
	flOriginalPosition[ 3 ][ 0 ] = pFancyQuadParameters->m_flVertexMin[ 0 ];
	flOriginalPosition[ 3 ][ 1 ] = pFancyQuadParameters->m_flVertexMax[ 1 ];

	float flGradientMatrix[ 3 ][ 2 ];
	if ( pFancyQuadBrush->m_bIsRadialGradient )
	{
		// radial gradient - we just need the inverse radius as a scaling factor around the start point
		flGradientMatrix[ 0 ][ 0 ] = 1.0f / pFancyQuadBrush->m_flGradientRadii[ 0 ];
		flGradientMatrix[ 0 ][ 1 ] = 0.0f;
		flGradientMatrix[ 1 ][ 0 ] = 0.0f;
		flGradientMatrix[ 1 ][ 1 ] = 1.0f / pFancyQuadBrush->m_flGradientRadii[ 1 ];
	}
	else if ( pFancyQuadBrush->m_bIsLinearGradient )
	{
		// linear gradient - we need a single vector along the desired direction, its length must be the inverse length, which is easily achieved with a divide by squared length ( 1 / sqrt without the sqrt )
		float flDelta[ 2 ], flInvDelta[ 2 ], flSqrLength, flInvSqrLength;
		flDelta[ 0 ] = pFancyQuadBrush->m_flGradientEndPoint[ 0 ] - pFancyQuadBrush->m_flGradientStartPoint[ 0 ];
		flDelta[ 1 ] = pFancyQuadBrush->m_flGradientEndPoint[ 1 ] - pFancyQuadBrush->m_flGradientStartPoint[ 1 ];
		flSqrLength = ( flDelta[ 0 ] * flDelta[ 0 ] + flDelta[ 1 ] * flDelta[ 1 ] );
		flInvSqrLength = (flSqrLength > 0.0001f ) ? 1.0f / flSqrLength : 0.0f;
		flInvDelta[ 0 ] = flDelta[ 0 ] * flInvSqrLength;
		flInvDelta[ 1 ] = flDelta[ 1 ] * flInvSqrLength;
		flGradientMatrix[ 0 ][ 0 ] = flInvDelta[ 0 ];
		flGradientMatrix[ 0 ][ 1 ] = 0.0f;
		flGradientMatrix[ 1 ][ 0 ] = flInvDelta[ 1 ];
		flGradientMatrix[ 1 ][ 1 ] = 0.0f;
	}
	else
	{
		flGradientMatrix[ 0 ][ 0 ] = 0.0f;
		flGradientMatrix[ 0 ][ 1 ] = 0.0f;
		flGradientMatrix[ 1 ][ 0 ] = 0.0f;
		flGradientMatrix[ 1 ][ 1 ] = 0.0f;
	}
	flGradientMatrix[ 2 ][ 0 ] = -( pFancyQuadBrush->m_flGradientStartPoint[ 0 ] * flGradientMatrix[ 0 ][ 0 ] + pFancyQuadBrush->m_flGradientStartPoint[ 1 ] * flGradientMatrix[ 1 ][ 0 ] );
	flGradientMatrix[ 2 ][ 1 ] = -( pFancyQuadBrush->m_flGradientStartPoint[ 0 ] * flGradientMatrix[ 0 ][ 1 ] + pFancyQuadBrush->m_flGradientStartPoint[ 1 ] * flGradientMatrix[ 1 ][ 1 ] );


	for ( int i = 0; i < 4; i++ )
	{
		pQuad->m_Verts[i].m_vUVGradient.x = flOriginalPosition[i][0] * flGradientMatrix[0][0] + flOriginalPosition[i][1] * flGradientMatrix[1][0] + flGradientMatrix[2][0];
		pQuad->m_Verts[i].m_vUVGradient.y = flOriginalPosition[i][0] * flGradientMatrix[0][1] + flOriginalPosition[i][1] * flGradientMatrix[1][1] + flGradientMatrix[2][1];
	}


	{
		VPROF( "CSource2Surface::DrawFancyQuad - Render");



		int nFlags = pFancyQuadParameters->m_nFlags;

		renderAttributes.SetVector4DValue( ATTR_BorderWd, *(Vector4D*)pFancyQuadParameters->m_flBorderWidth );

		if ( nFlags & (FancyQuadFlag_OuterCorner | FancyQuadFlag_InnerCorner ) )
		{
			renderAttributes.SetVector4DValue( ATTR_TopCornerRad, Vector4D( flScaledCornerRadii[ 0 ][ 0 ], flScaledCornerRadii[ 0 ][ 1 ], flScaledCornerRadii[ 1 ][ 0 ], flScaledCornerRadii[ 1 ][ 1 ] ) );
			renderAttributes.SetVector4DValue( ATTR_BtmCornerRad, Vector4D( flScaledCornerRadii[ 2 ][ 0 ], flScaledCornerRadii[ 2 ][ 1 ], flScaledCornerRadii[ 3 ][ 0 ], flScaledCornerRadii[ 3 ][ 1 ] ) );
		}

		if ( pFancyQuadBrush->m_nGradientStops > 2 )
			nFlags |= FancyQuadFlag_GradientComplex;
		else if ( pFancyQuadBrush->m_nGradientStops > 1 )
			nFlags |= FancyQuadFlag_GradientTwoStop;
		if ( pFancyQuadBrush->m_bIsRadialGradient )
			nFlags |= FancyQuadFlag_RadialGradient;
		if ( ( pFancyQuadDraw->m_flHueShift != 0.0f ) || ( pFancyQuadDraw->m_flSaturation != 1.0f ) || ( pFancyQuadDraw->m_flBrightness != 1.0f ) || ( pFancyQuadDraw->m_flContrast != 1.0f ) )
			nFlags |= FancyQuadFlag_ColorCorrection;
		if ( pFancyQuadDraw->m_hTexture1 != RENDER_TEXTURE_HANDLE_INVALID )
			nFlags |= FancyQuadFlag_OpacityMask;

		// the premul shader will multiply the texture's rgb by the texture's alpha, we don't need this if using an alpha texture or no texture, and if the texture is already premul then we don't need it there either...
		int nType = 0;
		if ( pFancyQuadDraw->m_hTexture0 == RENDER_TEXTURE_HANDLE_INVALID )
			nType = FancyQuadTextureType_None;
		else if ( pFancyQuadDraw->m_bIsYUVTexture )
			nType = FancyQuadTextureType_YUV;
		else if ( pFancyQuadDraw->m_bIsAlphaTexture || pFancyQuadBrush->m_bAlphaOnlyTexture )
			nType = FancyQuadTextureType_Alpha;
		else if ( pFancyQuadDraw->m_bIsYCoCgTexture )
			nType = FancyQuadTextureType_YCoCg;
		else
			nType = FancyQuadTextureType_RGBA;


		renderAttributes.SetIntValue( ATTR_D_USERADIALGRADIENT, ( nFlags & FancyQuadFlag_RadialGradient ) ? 1 : 0 );
		renderAttributes.SetIntValue( ATTR_D_USEOUTERCORNER, ( nFlags & FancyQuadFlag_OuterCorner ) ? 1 : 0 );
		renderAttributes.SetIntValue( ATTR_D_USEINNERCORNER, ( nFlags & FancyQuadFlag_InnerCorner ) ? 1 : 0 );
		renderAttributes.SetIntValue( ATTR_D_COLORCORRECTION, ( nFlags & FancyQuadFlag_ColorCorrection ) ? 1 : 0 );
		renderAttributes.SetIntValue( ATTR_D_USEOPACITYMASK, ( nFlags & FancyQuadFlag_OpacityMask ) ? 1 : 0 );
		renderAttributes.SetIntValue( ATTR_D_GRADIENT_TWOSTOP, ( nFlags & FancyQuadFlag_GradientTwoStop ) ? 1 : 0 );
		renderAttributes.SetIntValue( ATTR_D_GRADIENT_COMPLEX, ( nFlags & FancyQuadFlag_GradientComplex ) ? 1 : 0 );

		renderAttributes.SetIntValue( ATTR_D_USERADIALCLIP, (pFancyQuadDraw->m_flRadialClipSectorAngle != 0.0f) ? 1 : 0 );
		renderAttributes.SetFloatValue( ATTR_RadialClipStartAngle, DEG2RAD( pFancyQuadDraw->m_flRadialClipStartAngle ) );
		renderAttributes.SetFloatValue( ATTR_RadialClipSectorAngle, DEG2RAD( pFancyQuadDraw->m_flRadialClipSectorAngle ) );

		renderAttributes.SetIntValue( ATTR_D_TEXTURETYPE, nType );
		renderAttributes.SetIntValue( ATTR_D_PREMULTIPLY_ALPHA, pFancyQuadDraw->m_bTexIsNotPremul && !pFancyQuadBrush->m_bAlphaOnlyTexture );

		renderAttributes.SetVMatrixValue( ATTR_MatTransform, pFancyQuadDraw->m_pVMatrix ? pFancyQuadDraw->m_pVMatrix->Transpose() : VMatrix::GetIdentityMatrix().Transpose() );

		float flViewportWidth = pFancyQuadDraw->m_nWide == -1 ? pLayer->GetWidth() : pFancyQuadDraw->m_nWide;
		float flViewportHeight = pFancyQuadDraw->m_nTall == -1 ? pLayer->GetHeight() : pFancyQuadDraw->m_nTall;
		renderAttributes.SetFloatValue( ATTR_ViewportWidth, flViewportWidth );
		renderAttributes.SetFloatValue( ATTR_ViewportHeight, flViewportHeight );


		renderAttributes.SetVector4DValue( ATTR_Bordercolor, Vector4D( pFancyQuadParameters->m_flBorderColor[0], pFancyQuadParameters->m_flBorderColor[1], pFancyQuadParameters->m_flBorderColor[2], pFancyQuadParameters->m_flBorderColor[3] ) );
		renderAttributes.SetFloatValue( ATTR_HueShift, pFancyQuadDraw->m_flHueShift );
		renderAttributes.SetFloatValue( ATTR_Saturation, pFancyQuadDraw->m_flSaturation );
		renderAttributes.SetFloatValue( ATTR_Brightness, pFancyQuadDraw->m_flBrightness );
		renderAttributes.SetFloatValue( ATTR_Contrast, pFancyQuadDraw->m_flContrast );
		renderAttributes.SetFloatValue( ATTR_OpacityMaskOpacity, pFancyQuadDraw->m_flOpacityMaskOpacity );
	
		// forestw: using *2 on the gradientradialoffset makes the main menu background match the D2D version, I don't know if this is correct though
		if (pFancyQuadBrush->m_bIsRadialGradient)
		{
			Vector4D vecGradiantRadialOffset((pFancyQuadBrush->m_flGradientEndPoint[0] - pFancyQuadBrush->m_flGradientStartPoint[0]) / pFancyQuadBrush->m_flGradientRadii[0] * 2.0f, (pFancyQuadBrush->m_flGradientEndPoint[1] - pFancyQuadBrush->m_flGradientStartPoint[1]) / pFancyQuadBrush->m_flGradientRadii[1] * 2.0f, 0.0f, 0.0f);
			renderAttributes.SetVector4DValue(ATTR_Gradientradialoffset, vecGradiantRadialOffset);
		}


		if ( pFancyQuadDraw->m_hTexture0 != RENDER_TEXTURE_HANDLE_INVALID )
		{
			renderAttributes.SetTextureValue( ATTR_Texture0, pFancyQuadDraw->m_hTexture0 );
		}

		if ( pFancyQuadDraw->m_hTexture1 != RENDER_TEXTURE_HANDLE_INVALID )
		{
			renderAttributes.SetTextureValue( ATTR_Texture1, pFancyQuadDraw->m_hTexture1 );
		}

		if ( pFancyQuadDraw->m_hTexture2 != RENDER_TEXTURE_HANDLE_INVALID )
		{
			renderAttributes.SetTextureValue( ATTR_Texture2, pFancyQuadDraw->m_hTexture2 );
		}

		if ( nFlags & FancyQuadFlag_GradientComplex )
		{
			// generate the procedural texture bits
			HRenderTexture hGradientTexture = UpdateFancyQuadGradientTexture( pFancyQuadBrush );
			renderAttributes.SetTextureValue( ATTR_Texture3, hGradientTexture );
		}
			
		RenderViewport_t viewport;
		m_pRenderContext->GetViewport( &viewport, 0 );
		if ( viewport.m_nHeight == 0 || viewport.m_nWidth == 0 )
			return;

		if ( pFancyQuadDraw->m_bClipToLayer )
		{
			RectBounds_t r;
			pLayer->GetCurrentClipRect( r );
			Rect_t rScissor( RoundFloatToInt( r.left ), RoundFloatToInt( r.top ), RoundFloatToInt( r.right - r.left ), RoundFloatToInt( r.bottom - r.top ) );
			if( rScissor.x < 0 )
			{
				rScissor.width += rScissor.x;
				rScissor.x = 0;
			}

			if( rScissor.y < 0 )
			{
				rScissor.height += rScissor.y;
				rScissor.y = 0;
			}
			int nMaxWidth = viewport.m_nWidth - rScissor.x;
			rScissor.width = MIN( rScissor.width, nMaxWidth );
			int nMaxHeight = viewport.m_nHeight - rScissor.y;
			rScissor.height = MIN( rScissor.height, nMaxHeight );

			m_pRenderContext->SetScissorRect( rScissor );
		}
		else
		{
			// Cut off anything outside the viewport
			Rect_t rectScissor( viewport.m_nTopLeftX, viewport.m_nTopLeftY, viewport.m_nWidth, viewport.m_nHeight );
			m_pRenderContext->SetScissorRect( rectScissor );
		}

		const IMaterial2 *pMaterial = m_hFancyQuadMaterial;
		IMaterialMode *pMode = pMaterial->GetMode();
		if ( !pMode )
		{
			Log_Warning( LOG_PANORAMA, "CSource2Surface::DrawFancyQuad() GetMode == NULL? Can't Render\n" );
			return;
		}

		// pre transform (since we're optimising out VS consts)
		PreXFormFancyQuadPositions( &renderAttributes, pQuad, pFancyQuadDraw->m_pVMatrix, flViewportWidth, flViewportHeight, 1 );

		#if PANDX_DRAW
		if ( !g_bPanDx )
		#endif
		{
			//BROFILER_EVENT_START( OutVerts );

			int nVerts = 2 * 3;
			CDynamicVertexData< Source2FancyQuadVertex_t > triVB( m_pRenderContext, nVerts, "PanoramaFancyQuad", "Panorama" );
			triVB.Reset();

			int vertidx[] = { 0,2,1,0,3,2 };

			for ( int &j : vertidx )
			{
				triVB->m_vecPosition.Init( pQuad->m_Verts[ j ].m_vPosition.x, pQuad->m_Verts[ j ].m_vPosition.y, 0.0f, 1.0f );
				triVB->m_vecColor0 = pQuad->m_vColor;
				triVB->m_vecColor1 = pQuad->m_vColorStop;
				triVB->m_vecTexCoordGradientCoord.Init( pQuad->m_Verts[ j ].m_vUV.x, pQuad->m_Verts[ j ].m_vUV.y,
														pQuad->m_Verts[ j ].m_vUVGradient.x, pQuad->m_Verts[ j ].m_vUVGradient.y );
				triVB->m_vecOpacityTexCoord.Init( pQuad->m_Verts[ j ].m_vUVOpacity.x, pQuad->m_Verts[ j ].m_vUVOpacity.y, 0, 0 );
				triVB->m_vecFragCoordWdHt = pQuad->m_Verts[ j ].m_vFragCoordWdHt;

				triVB.AdvanceVertex();
			}


			triVB.UnlockAndBind( 0, 0 );

			// Draw batch
			MaterialRenderablePass_t passes[ MATERIAL_RENDERABLE_PASS_MAX ];
			int nPasses = pMode->ComputeRenderablePassesForContext( &renderAttributes, m_pRenderContext, passes );


			//BROFILER_EVENT_END( OutVerts );


			for ( int i = 0; i < nPasses; i++ )
			{
				VPROF( "DFQ - Draw" );

				g_pMaterialSystem2->SetRenderStateForRenderablePass( &renderAttributes, m_pRenderContext, m_hSource2FancyQuadVertexLayout, passes[ i ] );

				m_pRenderContext->SetCullMode( RENDER_CULLMODE_CULL_NONE );
				m_pRenderContext->SetZBufferMode( RENDER_ZBUFFER_NONE );
				m_pRenderContext->SetBlendState( m_hCurrentBlendState );

				//PANDRAWCALL FQ - CSource2Surface.DrawFancyQuad
				m_pRenderContext->CtxDraw( RENDER_PRIM_TRIANGLES, 0, nVerts );
				GetFrameStats().m_nDrawfancyQuadCalls++;
			}

		}
		#if PANDX_DRAW
		else
		{
			m_pRenderContext->SetCullMode( RENDER_CULLMODE_CULL_NONE );
			m_pRenderContext->SetZBufferMode( RENDER_ZBUFFER_NONE );
			m_pRenderContext->SetBlendState( m_hCurrentBlendState );
			( (CRenderContext*)m_pRenderContext )->m_pAttr = &renderAttributes;

			//PANDRAWCALL FQ - CSource2Surface.DrawFancyQuad
			PanDxDrawFancyQuads( (CRenderContext*)m_pRenderContext, pQuad );
		}
		#endif
		
		renderAttributes.Clear( false );
		s_FancyQuadAttributesPool.PutObject( &renderAttributes );	
	}
}


//-----------------------------------------------------------------------------
// Purpose: Handle render callback request (basically calls back raw panel code to draw direct to surface, source 2 only noop elsewhere)
//
// PANORAMA_USE_S1WRAPPER Notes:
// -----------------------------
// RequestRenderCallbackCommand_t supports 2 modes:
//		* RequestRenderCallbackCommand_t::pCallbackObj is not null - runs callback
//		  to draw directly to the layer's surface
//		* RequestRenderCallbackCommand_t::panelRT is not null - The user provided a render target
//		  texture that will be drawn to the layer's surface. It is the responsibility of the user
//		  to manage the render target ie drawn before panorama. This mode was added to support
//		  src1 queued material system.
//
//-----------------------------------------------------------------------------
void CSource2Surface::RequestRenderCallback( const RequestRenderCallbackCommand_t &renderCommand )
{
	VPROF_BUDGET_THREAD( "CSource2Surface::RequestRenderCallback", VPROF_BUDGETGROUP_TENFOOT );

	if ( m_stackCompositionLayers.Count() == 0 )
	{
		return;
	}

	if ( !s_convarPanoramaDisableRenderCallbacks.GetBool() )
	{
		if ( renderCommand.flags & k_ERenderCallbackFlagsRenderTargetMode )
		{
			//
			// ---------- Render target mode ----------
			//

			CSource2CompositionLayer *pLayer = m_stackCompositionLayers[ m_stackCompositionLayers.Count() - 1 ];
			pLayer->ActivateRenderTarget();

			int nClearFlags = RENDER_CLEAR_FLAGS_CLEAR_DEPTH | RENDER_CLEAR_FLAGS_CLEAR_STENCIL;
			m_pRenderContext->Clear( Vector4D( 0, 0, 0, 0 ), nClearFlags );

			RectBounds_t r;
			pLayer->GetCurrentClipRect( r );
			Rect_t rScissor( RoundFloatToInt( r.left ), RoundFloatToInt( r.top ), RoundFloatToInt( r.right - r.left ), RoundFloatToInt( r.bottom - r.top ) );
			if ( rScissor.x < 0 )
			{
				rScissor.width += rScissor.x;
				rScissor.x = 0;
			}

			if ( rScissor.y < 0 )
			{
				rScissor.height += rScissor.y;
				rScissor.y = 0;
			}

			IUITexture *pTexture = renderCommand.panelRT.GetTexture();
			if ( !pTexture )
			{
				return;
			}

			CSource2UIRenderTargetTexture *pSource2UITexture = dynamic_cast<CSource2UIRenderTargetTexture *>( pTexture );
			if ( !pSource2UITexture )
			{
				return;
			}

			m_pRenderContext->SetScissorRects( 1, &rScissor );

			HRenderTexture srcTex = pSource2UITexture->GetTextureHandle();

			CRenderAttributes renderAttributes;

			renderAttributes.SetFloatValue( ATTR_ViewportWidth, (float)pLayer->GetWidth() );
			renderAttributes.SetFloatValue( ATTR_ViewportHeight, (float)pLayer->GetHeight() );
			renderAttributes.SetVector2DValue( ATTR_UVClamp, Vector2D( 1.0, 1.0 ) );

			renderAttributes.SetIntValue( ATTR_D_TEX2DBLUR, 0 );
			renderAttributes.SetIntValue( ATTR_D_TEX2DDOWNSAMPLE, 1 ); // Up&Downsample both use a simple bilinear shader w/clamp

			renderAttributes.SetVMatrixValue( ATTR_MatTransform, VMatrix::GetIdentityMatrix() );

			RenderViewport_t renderViewport;
			renderViewport.Init( 0, 0, pLayer->GetWidth(), pLayer->GetHeight() );
			m_pRenderContext->SetViewports( 1, &renderViewport );

#if PANDX_DRAW
			BasicQuad_t *pQuad = PanDxGetBasicQuadPtr();
#else
			BasicQuad_t pQuad[1];
#endif

			pQuad->BqInit( Vector2D( renderCommand.top_left.x, renderCommand.top_left.y ), Vector2D( renderCommand.bottom_right.x, renderCommand.bottom_right.y ) );

			DrawTexturedQuadInternal( m_hMaterial, &renderAttributes, srcTex, pQuad );

			// Back to panorama renderer - Reset scissor 
			m_pRenderContext->SetScissorRects( 1, &rScissor );

		}
		else
		{

#if ( PANDX_DRAW )
			if ( g_bPanDx )
			{
				if ( !g_pdxInit ) return;

				CSource2CompositionLayer *pLayer = m_stackCompositionLayers[ m_stackCompositionLayers.Count() - 1 ];

				HRenderTexture hRT; 
				RenderTargetDesc_t rtDesc;

				pLayer->GetRenderTargetHandleAndDesc( hRT, rtDesc );

				S1Wrapper_Texture_t *pWrapperTexture = (S1Wrapper_Texture_t *)hRT.GetResourceHandle()->m_handle;
				ITexture* pTexture = pWrapperTexture->GetS1Texture();

				IDirect3DTexture9* pD3DTex = (IDirect3DTexture9*)g_pMaterialSystem->GetPanormaTexturePtr( pTexture );

				IDirect3DSurface9* pSurf = NULL;
				pD3DTex->GetSurfaceLevel( 0, &pSurf );
				PanDxCopyBackBuffer( (void*)pSurf, 0, 0, pLayer->GetWidth(), pLayer->GetHeight() );
				pSurf->Release();
				
				return;
			}
#endif

			//
			// ---------- callback mode using src1 matsys----------
			//


			CSource2CompositionLayer *pLayer = m_stackCompositionLayers[ m_stackCompositionLayers.Count() - 1 ];
			pLayer->ActivateRenderTarget();

			int nClearFlags = RENDER_CLEAR_FLAGS_CLEAR_DEPTH | RENDER_CLEAR_FLAGS_CLEAR_STENCIL;
			m_pRenderContext->Clear( Vector4D( 0, 0, 0, 0 ), nClearFlags );

			RectBounds_t r;
			pLayer->GetCurrentClipRect( r );
			Rect_t rScissor( RoundFloatToInt( r.left ), RoundFloatToInt( r.top ), RoundFloatToInt( r.right - r.left ), RoundFloatToInt( r.bottom - r.top ) );
			if ( rScissor.x < 0 )
			{
				rScissor.width += rScissor.x;
				rScissor.x = 0;
			}

			if ( rScissor.y < 0 )
			{
				rScissor.height += rScissor.y;
				rScissor.y = 0;
			}


			if ( !renderCommand.pCallbackObj )
			{
				return;
			}

			bool bEnableSSAA = false;
			if ( pLayer->GetTextureHandle() == RENDER_TEXTURE_HANDLE_INVALID )
			{
				// Prefer no SSAA if rendering to the scene layer RT (i.e. use MSAA if enabled)
				bEnableSSAA = false;
			}
			else
			{
				bEnableSSAA = true;
			}
			Vector4D vScissorAttribute( rScissor.x, rScissor.y, rScissor.width, rScissor.height );


			// Make sure no scissors are set before entering the callback
			// It is the callback responsibility to set the correct scissor
			m_pRenderContext->SetScissorRects( 0, NULL );

			renderCommand.pCallbackObj->RenderThreadCallback( &vScissorAttribute, renderCommand.top_left.x, renderCommand.top_left.y, renderCommand.bottom_right.x, renderCommand.bottom_right.y, bEnableSSAA );

			// Back to panorama renderer - Reset scissor 
			m_pRenderContext->SetScissorRects( 1, &rScissor );

		}

	}
}

//-----------------------------------------------------------------------------
// Purpose: Called to create an image shadow layer for a given image
//-----------------------------------------------------------------------------
CSource2CompositionLayer *CSource2Surface::GetImageShadowLayer( const ImageShadowLayerParams_t &params, IUITexture *pTexture, float flOriginalWidthScale, float flOriginalHeightScale )
{
	VPROF( "CSource2Surface::GetImageShadowLayer" );

	CSource2CompositionLayer *pImgShadowLayer = nullptr;

	// First try to find a matching outer shadow layer from the cache
	pImgShadowLayer = m_ImageShadowLayers.FindAndRemove( params, m_flCurrentRenderFrameTime );
	if ( pImgShadowLayer )
	{
		return pImgShadowLayer;
	}
	
	//--------------------------------------------------------------------------------------------------
	// Draw shadow in into padded RT
	// Uses strength property to dilate the image. 
	// TODO : optimisation opportuntites here.
	//--------------------------------------------------------------------------------------------------

	float flPadding = ceil( params.flBlurRadius );
	float flStrength = params.flStrength;
	float flWidth = params.flImgWidth + ( flStrength * 2.0f ) + ( flPadding * 2.0f );
	float flHeight = params.flImgHeight + ( flStrength * 2.0f ) + ( flPadding * 2.0f );

	pImgShadowLayer = GetCompositionLayer( flWidth, flHeight, false, false, "imgshadow_outset" );

	m_stackCompositionLayers.AddToTail( pImgShadowLayer );

	pImgShadowLayer->ActivateRenderTargetAndClear();

	FancyQuadParameters_t FancyParam( 0 );
	FancyParam.m_flZ = 0.0f;
	FancyParam.m_flTexCoordMin[0] = params.m_vUV[0].x;
	FancyParam.m_flTexCoordMin[1] = params.m_vUV[0].y;
	FancyParam.m_flTexCoordMax[0] = params.m_vUV[1].x;
	FancyParam.m_flTexCoordMax[1] = params.m_vUV[1].y;
	FancyParam.m_flOpacityTexCoordMin[0] = params.m_vUV[0].x;
	FancyParam.m_flOpacityTexCoordMin[1] = params.m_vUV[0].y;
	FancyParam.m_flOpacityTexCoordMax[0] = params.m_vUV[1].x;
	FancyParam.m_flOpacityTexCoordMax[1] = params.m_vUV[1].y;

	Vector4D vecCol = VecColorFromABGR( params.color );

	FancyQuadBrush_t FancyBrush( vecCol.x, vecCol.y, vecCol.z, 1.0f );
	if ( params.textureSampleMode == k_ETextureSampleModeAlphaOnly )
	{
		FancyBrush.m_bAlphaOnlyTexture = true;
	}

	
	if ( params.hRenderTexture != RENDER_TEXTURE_HANDLE_INVALID )
	{
		FancyQuadDraw_t fancyQuadDraw;
		fancyQuadDraw.m_hTexture0 = params.hRenderTexture;
		fancyQuadDraw.m_flTexture0TexCoordScale[0] = flOriginalWidthScale;
		fancyQuadDraw.m_flTexture0TexCoordScale[1] = flOriginalHeightScale;
		fancyQuadDraw.m_nWide = pImgShadowLayer->GetWidth();
		fancyQuadDraw.m_nTall = pImgShadowLayer->GetHeight();
		fancyQuadDraw.m_pQuadParameters = &FancyParam;
		fancyQuadDraw.m_pQuadBrush = &FancyBrush;
		fancyQuadDraw.m_flTextureWidth = pTexture->GetOriginalWidth();
		fancyQuadDraw.m_flTextureHeight = pTexture->GetOriginalHeight();
		fancyQuadDraw.m_bTexIsNotPremul = pTexture->GetAlphaChannelType() != k_EAlphaChannelType_PreMultiplied;
		fancyQuadDraw.m_bClipToLayer = true;
#ifdef PANORAMA_USE_S1WRAPPER	
		fancyQuadDraw.m_bIsYCoCgTexture = ( pTexture->GetFormat() == k_EFormatDXT5 ); // YCoCg only currently supported via DXT5
#else
		fancyQuadDraw.m_bIsYCoCgTexture = bIsDXTTexture;
#endif
		VMatrix mat = VMatrix::GetIdentityMatrix();
		fancyQuadDraw.m_pVMatrix = &mat;

		float flX = flPadding + flStrength;
		float flY = flPadding + flStrength;
		float flW = params.flImgWidth;
		float flH = params.flImgHeight;

		FancyParam.m_flVertexMin[0] = flX;
		FancyParam.m_flVertexMin[1] = flY;
		FancyParam.m_flVertexMax[0] = flX + flW;
		FancyParam.m_flVertexMax[1] = flY + flH;
		DrawFancyQuad( &fancyQuadDraw );

		for ( float flOffset = 1.0; flOffset < flStrength; flOffset += 1.0 )
		{
			FancyParam.m_flVertexMin[0] = flX + flOffset;
			FancyParam.m_flVertexMin[1] = flY;
			FancyParam.m_flVertexMax[0] = flX + flW + flOffset;
			FancyParam.m_flVertexMax[1] = flY + flH;
			DrawFancyQuad( &fancyQuadDraw );
			FancyParam.m_flVertexMin[0] = flX - flOffset;
			FancyParam.m_flVertexMin[1] = flY;
			FancyParam.m_flVertexMax[0] = flX + flW - flOffset;
			FancyParam.m_flVertexMax[1] = flY + flH;
			DrawFancyQuad( &fancyQuadDraw );
			FancyParam.m_flVertexMin[0] = flX;
			FancyParam.m_flVertexMin[1] = flY + flOffset;
			FancyParam.m_flVertexMax[0] = flX + flW;
			FancyParam.m_flVertexMax[1] = flY + flH + flOffset;
			DrawFancyQuad( &fancyQuadDraw );
			FancyParam.m_flVertexMin[0] = flX;
			FancyParam.m_flVertexMin[1] = flY - flOffset;
			FancyParam.m_flVertexMax[0] = flX + flW;
			FancyParam.m_flVertexMax[1] = flY + flH - flOffset;
			DrawFancyQuad( &fancyQuadDraw );
		}
	}

	//--------------------------------------------------------------------------------------------------
	// Blur shadow RT
	//--------------------------------------------------------------------------------------------------

	CRenderAttributes renderAttributes;

	// Apply gaussian blur

	ApplyGaussianBlur( renderAttributes, pImgShadowLayer, MAX( 1, int( params.flBlurRadius ) / 8 ), MIN( params.flBlurRadius, 8 ), MIN( params.flBlurRadius, 8 ) );

	m_stackCompositionLayers.Remove( m_stackCompositionLayers.Count() - 1 );


	return pImgShadowLayer;
}

//-----------------------------------------------------------------------------
// Purpose: Called to draw a textured quad with a texture object
//-----------------------------------------------------------------------------
void CSource2Surface::DrawTexturedRect( const RenderTexturedRectRenderCommand_t &renderCommand )
{
	VPROF( "CSource2Surface::DrawTexturedRect");
	//VPROF_BUDGET( "Panorama DrawTexturedRect", VPROF_BUDGETGROUP_GAME );

	IUITexture *pTexture = renderCommand.texture.GetTexture();
	if ( !pTexture )
		return;

	if ( m_stackCompositionLayers.Count() == 0 )
	{
		Assert( m_stackCompositionLayers.Count() > 0 );
		return;
	}

	CSource2CompositionLayer *pLayer = m_stackCompositionLayers[m_stackCompositionLayers.Count() - 1];
	if ( pLayer->BIsDrawing() )
	{
		// Ok, we have the locked texture data, setup shader resource view variables, and draw into current composition layer.
		pLayer->PopClipLayersAndFlush();
		pLayer->ActivateRenderTarget();

		float x0 = renderCommand.top_left.x;
		float x1 = renderCommand.bottom_right.x;

		float y0 = renderCommand.top_left.y;
		float y1 = renderCommand.bottom_right.y;

		float u0 = renderCommand.texture_top_left.x;
		float u1 = renderCommand.texture_bottom_right.x;

		float v0 = renderCommand.texture_top_left.y;
		float v1 = renderCommand.texture_bottom_right.y;

		const E2DTextureFormat eFormat = pTexture->GetFormat();
		float flOriginalWidthScale = 1.0f;
		float flOriginalHeightScale = 1.0f;
		const bool bIsDXTTexture = ( eFormat == k_EFormatDXT5 || eFormat == k_EFormatDXT1 );
		HRenderTexture hRenderTexture = RENDER_TEXTURE_HANDLE_INVALID;
		CSource2UITexture *pSource2UITexture = dynamic_cast<CSource2UITexture *>( pTexture );
		if ( pSource2UITexture )
		{
			hRenderTexture = pSource2UITexture->GetTextureHandle();
			flOriginalWidthScale = pSource2UITexture->GetOriginalWidthScale();
			flOriginalHeightScale = pSource2UITexture->GetOriginalHeightScale();

#ifdef PANORAMA_USE_S1WRAPPER
#ifdef PANORAMA_S1_NPO2_NO_TEXCOORDSCALE
			flOriginalWidthScale = 1.0f;
			flOriginalHeightScale = 1.0f;
#endif
#endif
		}
		else
		{
			CSource2DoubleBufferedTexture *pSource2DoubleBufferedTexture = dynamic_cast<CSource2DoubleBufferedTexture *>( pTexture );
			if ( pSource2DoubleBufferedTexture )
			{
				hRenderTexture = pSource2DoubleBufferedTexture->GetTextureHandle();
			}
			else
			{
				CSource2UITextureEngineRTRef *pSource2UITextureEngineRT = dynamic_cast<CSource2UITextureEngineRTRef *>( pTexture );
				if ( pSource2UITextureEngineRT )
				{
					hRenderTexture = pSource2UITextureEngineRT->GetTextureHandle();
				}
			}
		}

		// Draw Shadow

		if (renderCommand.img_shadow)
		{
			ImageShadowLayerParams_t imgShadowParams;
			imgShadowParams.flImgWidth = x1 - x0;
			imgShadowParams.flImgHeight = y1 - y0;
			imgShadowParams.m_vUV[0] = Vector2D( u0, v0 );
			imgShadowParams.m_vUV[1] = Vector2D( u1, v1 );
			imgShadowParams.flBlurRadius = renderCommand.img_shadow->blur_radius;
			imgShadowParams.flStrength = MIN ( renderCommand.img_shadow->strength, 256.0);
			imgShadowParams.color = renderCommand.img_shadow->color;
			imgShadowParams.textureSampleMode = renderCommand.texture_sample_mode;
			imgShadowParams.hRenderTexture = hRenderTexture;
			CSource2CompositionLayer* pShadowOutLayer = GetImageShadowLayer( imgShadowParams, pTexture, flOriginalWidthScale, flOriginalHeightScale );

			//--------------------------------------------------------------------------------------------------
			// Draw Back into Layer
			//--------------------------------------------------------------------------------------------------
			{
				pLayer->ActivateRenderTarget();

				float flPadding = ceil( imgShadowParams.flBlurRadius );
				float flStrength = imgShadowParams.flStrength;
				float flWidth = imgShadowParams.flImgWidth + ( flStrength * 2.0f ) + ( flPadding * 2.0f );
				float flHeight = imgShadowParams.flImgHeight + ( flStrength * 2.0f ) + ( flPadding * 2.0f );

				FancyQuadParameters_t FancyParam( 0 );
				FancyParam.m_flZ = 0.0f;
				FancyParam.m_flVertexMin[ 0 ] = x0 - flPadding - flStrength + renderCommand.img_shadow->horizontal_offset;
				FancyParam.m_flVertexMin[ 1 ] = y0 - flPadding - flStrength + renderCommand.img_shadow->vertical_offset;
				FancyParam.m_flVertexMax[ 0 ] = FancyParam.m_flVertexMin[ 0 ] + flWidth;
				FancyParam.m_flVertexMax[ 1 ] = FancyParam.m_flVertexMin[ 1 ] + flHeight;
				FancyParam.m_flTexCoordMin[ 0 ] = 0;
				FancyParam.m_flTexCoordMin[ 1 ] = 0;
				FancyParam.m_flTexCoordMax[ 0 ] = pShadowOutLayer->GetRTOriginalWidthScale();
				FancyParam.m_flTexCoordMax[ 1 ] = pShadowOutLayer->GetRTOriginalHeightScale();

				float flOpacity = 1.0f;
				Vector4D vecCol(1.0f,1.0f,1.0f,1.0f);

				FancyQuadBrush_t FancyBrush( vecCol.x, vecCol.y, vecCol.z, flOpacity );
				if ( renderCommand.texture_sample_mode == k_ETextureSampleModeAlphaOnly )
				{
					FancyBrush.m_bAlphaOnlyTexture = true;
				}

				//if ( pTexture->BIsReady() )
				{
					FancyQuadDraw_t fancyQuadDraw;
					fancyQuadDraw.m_hTexture0 = pShadowOutLayer->GetTextureHandle();
					fancyQuadDraw.m_flTexture0TexCoordScale[ 0 ] = 1.0;
					fancyQuadDraw.m_flTexture0TexCoordScale[ 1 ] = 1.0;
					fancyQuadDraw.m_nWide = pLayer->GetWidth();
					fancyQuadDraw.m_nTall = pLayer->GetHeight();
					fancyQuadDraw.m_pQuadParameters = &FancyParam;
					fancyQuadDraw.m_pQuadBrush = &FancyBrush;
					fancyQuadDraw.m_flTextureWidth = pShadowOutLayer->GetWidth();
					fancyQuadDraw.m_flTextureHeight = pShadowOutLayer->GetHeight();
					fancyQuadDraw.m_bTexIsNotPremul = false;
					fancyQuadDraw.m_bClipToLayer = false;
					fancyQuadDraw.m_bIsYCoCgTexture = false;
					VMatrix mat = VMatrix::GetIdentityMatrix();
					fancyQuadDraw.m_pVMatrix = &mat;

					DrawFancyQuad( &fancyQuadDraw );

				}

			}

			if ( !s_convarPanoramaDisableImageShadowLayerCache.GetBool() )
			{
				m_ImageShadowLayers.Insert( imgShadowParams, pShadowOutLayer, m_flCurrentRenderFrameTime );
			}
			else
			{
				m_FreeLayers.Insert( pShadowOutLayer, pShadowOutLayer, m_flCurrentRenderFrameTime );
			}

		}


		// prepare the fancy quad parameters and brush according to the message
		FancyQuadParameters_t FancyParam(0);
		FancyParam.m_flZ = 0.0f;
		FancyParam.m_flVertexMin[0] = x0;
		FancyParam.m_flVertexMin[1] = y0;
		FancyParam.m_flVertexMax[0] = x1;
		FancyParam.m_flVertexMax[1] = y1;
		FancyParam.m_flTexCoordMin[0] = u0;
		FancyParam.m_flTexCoordMin[1] = v0;
		FancyParam.m_flTexCoordMax[0] = u1;
		FancyParam.m_flTexCoordMax[1] = v1;
		FancyParam.m_flOpacityTexCoordMin[0] = u0;
		FancyParam.m_flOpacityTexCoordMin[1] = v0;
		FancyParam.m_flOpacityTexCoordMax[0] = u1;
		FancyParam.m_flOpacityTexCoordMax[1] = v1;

		float flOpacity = renderCommand.texture_opacity;
		FancyQuadBrush_t FancyBrush(flOpacity, flOpacity, flOpacity, flOpacity);

		if ( renderCommand.texture_sample_mode == k_ETextureSampleModeAlphaOnly )
		{
			FancyBrush.m_bAlphaOnlyTexture = true;
		}

		// Special handling for YUV420
		if ( eFormat == k_EFormatYUV420 )
		{
			CSource2DoubleBufferedYUV420Texture *pSource2DoubleBufferedYUV420Texture = dynamic_cast< CSource2DoubleBufferedYUV420Texture * >( pTexture );
			if ( pTexture->BIsReady() )
			{
				FancyQuadDraw_t fancyQuadDraw;
				pSource2DoubleBufferedYUV420Texture->GetCurrentTextureHandles( fancyQuadDraw.m_hTexture0, fancyQuadDraw.m_hTexture1, fancyQuadDraw.m_hTexture2 );

#ifdef PANORAMA_USE_S1WRAPPER
#ifndef PANORAMA_S1_NPO2_NO_TEXCOORDSCALE
					fancyQuadDraw.m_flTexture0TexCoordScale[ 0 ] = pSource2DoubleBufferedYUV420Texture->GetOriginalWidthScale();
					fancyQuadDraw.m_flTexture0TexCoordScale[ 1 ] = pSource2DoubleBufferedYUV420Texture->GetOriginalHeightScale();
#endif
#endif

				fancyQuadDraw.m_nWide = pLayer->GetWidth();
				fancyQuadDraw.m_nTall = pLayer->GetHeight();
				fancyQuadDraw.m_pQuadParameters = &FancyParam;
				fancyQuadDraw.m_pQuadBrush = &FancyBrush;
				fancyQuadDraw.m_flTextureWidth = pTexture->GetTextureWidth();
				fancyQuadDraw.m_flTextureHeight = pTexture->GetTextureHeight();
				fancyQuadDraw.m_bIsYUVTexture = true;
				fancyQuadDraw.m_bClipToLayer = true;
				fancyQuadDraw.m_pVMatrix = pLayer->AccessPushedMatrix();

				DrawFancyQuad( &fancyQuadDraw );
			}
		}
		else
		{
			if ( pTexture->BIsReady() )
			{
				FancyQuadDraw_t fancyQuadDraw;
				fancyQuadDraw.m_hTexture0 = hRenderTexture;
				fancyQuadDraw.m_flTexture0TexCoordScale[0] = flOriginalWidthScale;
				fancyQuadDraw.m_flTexture0TexCoordScale[1] = flOriginalHeightScale;
				fancyQuadDraw.m_nWide = pLayer->GetWidth();
				fancyQuadDraw.m_nTall = pLayer->GetHeight();
				fancyQuadDraw.m_pQuadParameters = &FancyParam;
				fancyQuadDraw.m_pQuadBrush = &FancyBrush;
				fancyQuadDraw.m_flTextureWidth = pTexture->GetOriginalWidth();
				fancyQuadDraw.m_flTextureHeight = pTexture->GetOriginalHeight();
				fancyQuadDraw.m_bTexIsNotPremul = pTexture->GetAlphaChannelType() != k_EAlphaChannelType_PreMultiplied;
				fancyQuadDraw.m_bClipToLayer = true;
#ifdef PANORAMA_USE_S1WRAPPER	
				fancyQuadDraw.m_bIsYCoCgTexture = (eFormat == k_EFormatDXT5); // YCoCg only currently supported via DXT5
#else
				fancyQuadDraw.m_bIsYCoCgTexture = bIsDXTTexture;
#endif
				fancyQuadDraw.m_pVMatrix = pLayer->AccessPushedMatrix();

				DrawFancyQuad( &fancyQuadDraw );
			}
		}
	}

	if ( m_stackCompositionLayers.Count() == 1 )
		pLayer->PushCliplayersAndBeginDraw( m_flScaleBackbufferX, m_flScaleBackbufferY, m_flTranslateBackbufferX, m_flTranslateBackbufferY );
	else
		pLayer->PushCliplayersAndBeginDraw( 1.0f, 1.0f, 0.0f, 0.0f );
}


//-----------------------------------------------------------------------------
// Purpose: Set up a FancyQuadBrush parameter structure from a FillBrush_t
//-----------------------------------------------------------------------------
void CSource2Surface::SetFancyQuadFillBrush( FancyQuadBrush_t &FancyBrush, const FillBrush_t &brush, float offsetx, float offsety )
{
	uint32 rgba;
	float r, g, b, a, p;
	float maxposition = 1.0f;
	bool bPrintGradient = false;

	V_memset( &FancyBrush, 0, sizeof( FancyBrush ) );

	if ( brush.eFillBrushType == k_EFillBrushType_LinearGradient )
	{
		const LinearGradient_t &gradient = *brush.linear_gradient;

		if ( bPrintGradient )
			Log_Msg( LOG_PANORAMA, "SetFancyQuadFillBrush: linear gradient:" );

		int iColorStop = 0;
		for ( const ColorStop_t *pColorStop : gradient.color_stop )
		{
			rgba = pColorStop->color_rgba;
			p = pColorStop->position;
			maxposition = p;
			ColorFromABGR( r, g, b, a, rgba );

			if ( bPrintGradient )
				Log_Msg( LOG_PANORAMA, " %08x@%.3f", rgba, p );

			a *= brush.opacity;
			FancyBrush.m_flColor[ iColorStop ][ 0 ] = r * a;
			FancyBrush.m_flColor[ iColorStop ][ 1 ] = g * a;
			FancyBrush.m_flColor[ iColorStop ][ 2 ] = b * a;
			FancyBrush.m_flColor[ iColorStop ][ 3 ] = a;
			FancyBrush.m_flGradientStops[ iColorStop ] = p;
			FancyBrush.m_nGradientStops = iColorStop + 1;

			Assert( iColorStop < FANCYQUAD_MAXSTOPS );

			++iColorStop;
		}
		if ( bPrintGradient )
			Log_Msg( LOG_PANORAMA, "\n" );

		// forestw: we have two gradient methods in the shader, one is fast (2 stops, normalized to position 1.0), the other is slow (up to 4 stops at configurable positions)
		// if we're using 2 stops we need to normalize the second position
		if ( FancyBrush.m_nGradientStops > 2 )
		{
			// if we're using the complex gradient method, skip the gradient collapse
			maxposition = 1.0f;
		}
		FancyBrush.m_flGradientStartPoint[ 0 ] = gradient.start_position.x + offsetx;
		FancyBrush.m_flGradientStartPoint[ 1 ] = gradient.start_position.y + offsety;
		FancyBrush.m_flGradientEndPoint[ 0 ] = gradient.start_position.x + ( gradient.end_position.x - gradient.start_position.x ) * maxposition + offsetx;
		FancyBrush.m_flGradientEndPoint[ 1 ] = gradient.start_position.y + ( gradient.end_position.y - gradient.start_position.y ) * maxposition + offsety;
		FancyBrush.m_flGradientRadii[ 0 ] = 0.0f;
		FancyBrush.m_flGradientRadii[ 1 ] = 0.0f;
		FancyBrush.m_bIsLinearGradient = true;
		FancyBrush.m_bIsRadialGradient = false;
	}
	else if ( brush.eFillBrushType == k_EFillBrushType_RadialGradient )
	{
		const RadialGradient_t &gradient = *brush.radial_gradient;

		if ( bPrintGradient )
			Log_Msg( LOG_PANORAMA, "SetFancyQuadFillBrush: radial gradient:" );

		int iColorStop = 0;
		for ( const ColorStop_t *pColorStop : gradient.color_stop )
		{
			rgba = pColorStop->color_rgba;
			p = pColorStop->position;
			maxposition = p;
			ColorFromABGR( r, g, b, a, rgba );

			if ( bPrintGradient )
				Log_Msg( LOG_PANORAMA, " %08x@%.3f", rgba, p );

			a *= brush.opacity;
			FancyBrush.m_flColor[ iColorStop ][ 0 ] = r * a;
			FancyBrush.m_flColor[ iColorStop ][ 1 ] = g * a;
			FancyBrush.m_flColor[ iColorStop ][ 2 ] = b * a;
			FancyBrush.m_flColor[ iColorStop ][ 3 ] = a;
			FancyBrush.m_flGradientStops[ iColorStop ] = p;
			FancyBrush.m_nGradientStops = iColorStop + 1;

			Assert( iColorStop < FANCYQUAD_MAXSTOPS );

			++iColorStop;
		}
		if ( bPrintGradient )
			Log_Msg( LOG_PANORAMA, "\n" );

		// pad the gradient to fill remaining stops
		// check for iColorStop > 0 required otherwise analyze build complains about possible invalid indexing
		if ( iColorStop > 0 )
		{
			while ( iColorStop < FANCYQUAD_MAXSTOPS )
			{
				FancyBrush.m_flColor[iColorStop][0] = FancyBrush.m_flColor[iColorStop - 1][0];
				FancyBrush.m_flColor[iColorStop][1] = FancyBrush.m_flColor[iColorStop - 1][1];
				FancyBrush.m_flColor[iColorStop][2] = FancyBrush.m_flColor[iColorStop - 1][2];
				FancyBrush.m_flColor[iColorStop][3] = FancyBrush.m_flColor[iColorStop - 1][3];
				FancyBrush.m_flGradientStops[iColorStop] = FancyBrush.m_flGradientStops[iColorStop - 1];
				iColorStop++;
			}
		}

		if ( ( gradient.radii.x > 0 ) && ( gradient.radii.y > 0 ) )
		{
			// forestw: we have two gradient methods in the shader, one is fast (2 stops, normalized to position 1.0), the other is slow (up to 4 stops at configurable positions)
			// if we're using 2 stops we need to normalize the second position
			if ( FancyBrush.m_nGradientStops > 2 )
			{
				// if we're using the complex gradient method, skip the gradient collapse
				maxposition = 1.0f;
			}
		FancyBrush.m_flGradientStartPoint[ 0 ] = gradient.center_position.x + offsetx;
		FancyBrush.m_flGradientStartPoint[ 1 ] = gradient.center_position.y + offsety;
		FancyBrush.m_flGradientEndPoint[ 0 ] = gradient.center_position.x + gradient.offset_distance.x + offsetx;
		FancyBrush.m_flGradientEndPoint[ 1 ] = gradient.center_position.y + gradient.offset_distance.y + offsety;
		FancyBrush.m_flGradientRadii[ 0 ] = gradient.radii.x * maxposition;
		FancyBrush.m_flGradientRadii[ 1 ] = gradient.radii.y * maxposition;
			FancyBrush.m_bIsLinearGradient = false;
			FancyBrush.m_bIsRadialGradient = true;
		}
		else
		{
			// radial gradient with a radius of 0. Convert to a fill color brush 
			// using color from the last stop which is equivalent in appearance

			FancyBrush.m_flColor[0][0] = FancyBrush.m_flColor[FANCYQUAD_MAXSTOPS - 1][0];
			FancyBrush.m_flColor[0][1] = FancyBrush.m_flColor[FANCYQUAD_MAXSTOPS - 1][1];
			FancyBrush.m_flColor[0][2] = FancyBrush.m_flColor[FANCYQUAD_MAXSTOPS - 1][2];
			FancyBrush.m_flColor[0][3] = FancyBrush.m_flColor[FANCYQUAD_MAXSTOPS - 1][3];
		}
	}
	else if ( brush.eFillBrushType == k_EFillBrushType_Color )
	{
		ColorFromABGR( r, g, b, a, brush.color_rgba );
		a *= brush.opacity;
		FancyBrush.m_flColor[0][0] = r * a;
		FancyBrush.m_flColor[0][1] = g * a;
		FancyBrush.m_flColor[0][2] = b * a;
		FancyBrush.m_flColor[0][3] = a;
	}
	else
	{
		FancyBrush.m_flColor[0][0] = 1.0f;
		FancyBrush.m_flColor[0][1] = 1.0f;
		FancyBrush.m_flColor[0][2] = 1.0f;
		FancyBrush.m_flColor[0][3] = 1.0f;
	}

	if ( FancyBrush.m_bIsLinearGradient || FancyBrush.m_bIsRadialGradient )
	{
		int iColorStop = FancyBrush.m_nGradientStops;

		// pad the gradient to fill remaining stops
		while ( iColorStop < FANCYQUAD_MAXSTOPS )
		{
			FancyBrush.m_flColor[iColorStop][0] = FancyBrush.m_flColor[iColorStop - 1][0];
			FancyBrush.m_flColor[iColorStop][1] = FancyBrush.m_flColor[iColorStop - 1][1];
			FancyBrush.m_flColor[iColorStop][2] = FancyBrush.m_flColor[iColorStop - 1][2];
			FancyBrush.m_flColor[iColorStop][3] = FancyBrush.m_flColor[iColorStop - 1][3];
			FancyBrush.m_flGradientStops[iColorStop] = 1.0f;
			iColorStop++;
		}

		// If the inbound gradient doesn't end at 1.0, promote one of our added padding
		// stops to the actual last stop of the gradient.
		if ( FancyBrush.m_flGradientStops[FancyBrush.m_nGradientStops - 1] < 1.0f )
		{
			FancyBrush.m_nGradientStops++;
			Assert( FancyBrush.m_nGradientStops < FANCYQUAD_MAXSTOPS );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called to draw a filled quad in current composition layer
//-----------------------------------------------------------------------------
void CSource2Surface::DrawFilledRect( const RenderFilledRectRenderCommand_t &renderCommand )
{
	VPROF( "CSource2Surface::DrawFilledRect");
	//VPROF_BUDGET( "Panorama DrawFilledRect", VPROF_BUDGETGROUP_GAME );

	if ( m_stackCompositionLayers.Count() == 0 )
	{
		Assert( m_stackCompositionLayers.Count() > 0 );
		return;
	}

	CSource2CompositionLayer *pLayer = m_stackCompositionLayers[ m_stackCompositionLayers.Count() - 1 ];
	if ( pLayer->BIsDrawing() )
	{
		const FillBrushCollection_t &fill_brush_collection = renderCommand.fill_brush_collection;

		float x0 = renderCommand.top_left.x;
		float y0 = renderCommand.top_left.y;
		float x1 = renderCommand.bottom_right.x;
		float y1 = renderCommand.bottom_right.y;
		pLayer->ActivateRenderTarget();

		CRenderAttributes renderAttributes;
		renderAttributes.SetIntValue( ATTR_D_TEX2DPARTICLE, 1 );
		renderAttributes.SetVMatrixValue( ATTR_MatTransform, VMatrix::GetIdentityMatrix() );
		renderAttributes.SetFloatValue( ATTR_ViewportWidth, (float)pLayer->GetWidth() );
		renderAttributes.SetFloatValue( ATTR_ViewportHeight, (float)pLayer->GetHeight() );

		for ( const FillBrush_t *pFillBrush : fill_brush_collection.fill_brush )
		{
			// prepare the fancy quad parameters and brush according to the message
			FancyQuadParameters_t FancyParam(0);
			FancyParam.m_flZ = renderCommand.top_left.z;
			FancyParam.m_flVertexMin[ 0 ] = x0;
			FancyParam.m_flVertexMin[ 1 ] = y0;
			FancyParam.m_flVertexMax[ 0 ] = x1;
			FancyParam.m_flVertexMax[ 1 ] = y1;
			FancyParam.m_flTexCoordMin[ 0 ] = 0.0f;
			FancyParam.m_flTexCoordMin[ 1 ] = 0.0f;
			FancyParam.m_flTexCoordMax[ 0 ] = 1.0f;
			FancyParam.m_flTexCoordMax[ 1 ] = 1.0f;

			FancyQuadBrush_t FancyBrush;
			SetFancyQuadFillBrush( FancyBrush, *pFillBrush, x0, y0 );

			FancyQuadDraw_t fancyQuadDraw;
			fancyQuadDraw.m_nWide = -1;
			fancyQuadDraw.m_nTall = -1;
			fancyQuadDraw.m_pQuadParameters = &FancyParam;
			fancyQuadDraw.m_pQuadBrush = &FancyBrush;
			fancyQuadDraw.m_bClipToLayer = true;
			fancyQuadDraw.m_pVMatrix = pLayer->AccessPushedMatrix();

			DrawFancyQuad( &fancyQuadDraw );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Generate or return cached text opacity mask texture
//-----------------------------------------------------------------------------
UITextOpacityMaskData_t *CSource2Surface::GetCachedTextOpacityMask( const void *pRawText, int cbRawText, int cTextChars, EPanoramaTextEncoding eTextEncoding, float x0, float y0, float x1, float y1, float flLineHeight, ETextAlign align, bool bWrap, bool bEllipsis, const RenderTextFormat_t &defaultFormat, const CRenderDataList< RenderTextRangeFormat_t > &rangeFormats )
{
	VPROF( "CSource2Surface::GetCachedTextOpacityMask");

	// Need text to do any work
	if ( !pRawText || cTextChars <= 0 )
		return NULL;

	UITextLayoutProperties_t key;
	m_pTextLayoutDrawCache->InitTextLayoutProperties(
		&key, pRawText, cbRawText, cTextChars, eTextEncoding, x0, y0, x1, y1, flLineHeight, align, bWrap, bEllipsis, UIEngine()->GetDisplayLanguage() );
	RenderCommandToTextLayoutKey( defaultFormat, rangeFormats, &key );

	CUtlVectorFixedGrowable<const char *, 4> vecRangeFontNames;
	for ( const RenderTextRangeFormat_t *pRangeFormat : rangeFormats )
	{
		vecRangeFontNames.AddToTail( pRangeFormat->format.font_name );
	}

	UITextOpacityMaskData_t *pData = m_pTextLayoutDrawCache->GetTextOpacityMask( pRawText, cbRawText, cTextChars, eTextEncoding, defaultFormat.font_name, &key, vecRangeFontNames.Base(), m_flCurrentRenderFrameTime, nullptr );

	return pData;
}


//-----------------------------------------------------------------------------
// Purpose: Returns the maximum font glyph texture width
//-----------------------------------------------------------------------------
uint32 CSource2Surface::GetMaximumTextureWidth()
{
	return GetSurfaceWidth();
}


//-----------------------------------------------------------------------------
// Purpose: Returns the maximum font glyph texture height
//-----------------------------------------------------------------------------
uint32 CSource2Surface::GetMaximumTextureHeight()
{
	return GetSurfaceHeight();
}


//-----------------------------------------------------------------------------
// Purpose: Creates or finds a cached text alpha texture
//-----------------------------------------------------------------------------
UITextTextureRegion_t CSource2Surface::GetTextureRegion( int32 iWidth, int32 iHeight )
{
	return m_pTextTextureCache->GetTextureRegion( iWidth, iHeight );
}


//-----------------------------------------------------------------------------
// Purpose: Called at the start of updating a font texture
//-----------------------------------------------------------------------------
void CSource2Surface::StartUpdateFontGlyphTexture( UITextTextureHandle_t hTexture )
{
}


//-----------------------------------------------------------------------------
// Purpose: Called to update font texture
//-----------------------------------------------------------------------------
void CSource2Surface::UpdateFontGlyphTexture( UITextTextureHandle_t hTexture, int xOffset, int yOffset, int width, int height, void *pSourceData )
{
#ifdef PANORAMA_USE_S1WRAPPER
	// Checking for devicce lost (ie CShaderDeviceDx8::IsDeactivated() exposed via CanDownloadTextures() on the material system)
	// (Note that it is possible for the device to be marked as lost (calling CShaderDeviceDx8::MarkDeviceLost)
	// before getting the "device lost" callback (done in CShaderDeviceDx8::CheckDeviceLost) )
	if ( g_pMaterialSystem->CanDownloadTextures() )
	{
		g_pRenderDevice->UpdateAlphaTexture( (ResourceHandle_t)hTexture, xOffset, yOffset, width, height, pSourceData );
	}
#else
	CSource2UITexture *pTexture = (CSource2UITexture*)hTexture;

	// subrect in the bits
	CTextureDesc dataDesc;
	dataDesc.m_nWidth = width;
	dataDesc.m_nHeight = height;
	dataDesc.m_nNumMipLevels = 1;
	dataDesc.m_nDepth = 1;
	dataDesc.m_nImageFormat = IMAGE_FORMAT_A8;

	Rect3D_t subRect;
	subRect.x = xOffset;
	subRect.y = yOffset;
	subRect.z = 0;
	subRect.width = width;
	subRect.height = height;
	subRect.depth = 1;

	m_pRenderContext->SetTextureData( pTexture->GetTextureHandle(), &dataDesc, pSourceData, width*height, false, -1, &subRect );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Called at the end of updating a font texture
//-----------------------------------------------------------------------------
void CSource2Surface::EndUpdateFontGlyphTexture( UITextTextureHandle_t hTexture )
{
}


//-----------------------------------------------------------------------------
// Purpose: Allocate a new alpha-only texture for text rendering
//-----------------------------------------------------------------------------
UITextTextureHandle_t CSource2Surface::AllocAlphaTexture( int32 iWidth, int32 iHeight )
{
#ifdef PANORAMA_USE_S1WRAPPER
	COMPILE_TIME_ASSERT( sizeof( UITextTextureHandle_t ) == sizeof( ResourceHandle_t ) );

	HRenderTexture hTexture = g_pRenderDevice->CreateAlphaTexture( iWidth, iHeight );

	return (UITextTextureHandle_t)hTexture.GetResourceHandle();
#else
	IUIRenderDevice *pRenderDevice = ( ( CUIEngineSource2 * )UIEngine() )->GetRenderDevice();
	IUITexture *pTexture = NULL;
	if ( !pRenderDevice->BCreateTexture( &pTexture, NULL, iWidth, iHeight, iWidth, k_EFormatA8, k_EAlphaChannelType_None ) )
	{
		return NULL;
	}

	return pTexture;
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Free a texture
//-----------------------------------------------------------------------------
void CSource2Surface::FreeTexture( UITextTextureHandle_t hTexture )
{
#ifdef PANORAMA_USE_S1WRAPPER
	g_pRenderDevice->DestroyAlphaTexture( (ResourceHandle_t)hTexture );
#else
	IUITexture *pTexture = (IUITexture*)hTexture;
	pTexture->Release();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Kick off async job to add text region
//			to the "text layout draw cache" (if it's not already there)
//-----------------------------------------------------------------------------
void CSource2Surface::AsyncAddTextRegionToCache( const DrawTextRegionRenderCommand_t &renderCommand )
{
	if( s_convarUseAsyncTextlayoutGeneration.GetBool() )
	{
		float x0, y0, x1, y1;
		x0 = renderCommand.top_left.x;
		y0 = renderCommand.top_left.y;
		x1 = renderCommand.bottom_right.x;
		y1 = renderCommand.bottom_right.y;
		void *pRawText = renderCommand.raw_text;
		int cbRawText = renderCommand.raw_text_bytes;
		int cTextChars = renderCommand.text_chars;
		EPanoramaTextEncoding eTextEncoding = renderCommand.text_encoding;

		// Check rect has valid area
		if( x1 <= x0 || y1 <= y0 )
			return;

		// Need text to do any work
		if( !renderCommand.raw_text || cTextChars <= 0 )
			return;

		float flLineHeight = k_flFloatNotSet;
		if( renderCommand.line_height > 0 )
			flLineHeight = renderCommand.line_height;

		UITextLayoutProperties_t key;
		m_pTextLayoutDrawCache->InitTextLayoutProperties(
			&key, pRawText, cbRawText, cTextChars, eTextEncoding, 
			x0, y0, x1, y1, flLineHeight, renderCommand.text_align, renderCommand.wrapping, renderCommand.ellipsis, UIEngine()->GetDisplayLanguage() );

		RenderCommandToTextLayoutKey( renderCommand.default_format, renderCommand.range_formats, &key );

		CUtlVectorFixedGrowable<const char *, 4> vecRangeFontNames;
		for( const TextRangeFormatData_t *pRangeFormat : renderCommand.range_formats )
		{
			vecRangeFontNames.AddToTail( pRangeFormat->format.font_name );
		}

		m_pTextLayoutDrawCache->GetTextOpacityMaskAsync( pRawText, cbRawText, cTextChars, eTextEncoding, 
			renderCommand.default_format.font_name, &key, vecRangeFontNames.Base(), m_flCurrentRenderFrameTime );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Make sure text (for the given rec region) stays in the 
//			"text layout draw cache" (if it is currently in the cache)
//-----------------------------------------------------------------------------
void CSource2Surface::TouchTextRegionInCache( const RenderTextRegionCommand_t &renderCommand )
{
	float x0, y0, x1, y1;
	x0 = renderCommand.top_left.x;
	y0 = renderCommand.top_left.y;
	x1 = renderCommand.bottom_right.x;
	y1 = renderCommand.bottom_right.y;

	// Check rect has valid area
	if ( x1 <= x0 || y1 <= y0 )
		return;

	float flLineHeight = k_flFloatNotSet;
	if ( renderCommand.line_height > 0 )
		flLineHeight = renderCommand.line_height;


	UITextLayoutProperties_t key; 
	m_pTextLayoutDrawCache->InitTextLayoutProperties(
		&key,
		renderCommand.raw_text, 
		renderCommand.raw_text_bytes, 
		renderCommand.text_chars, 
		renderCommand.text_encoding, 
		x0, 
		y0, 
		x1, 
		y1, 
		flLineHeight, 
		renderCommand.text_align, 
		renderCommand.wrapping, 
		renderCommand.ellipsis, 
		UIEngine()->GetDisplayLanguage() );
	RenderCommandToTextLayoutKey( renderCommand.default_format, renderCommand.range_formats, &key );

	m_pTextLayoutDrawCache->Touch( &key, m_flCurrentRenderFrameTime );

}


//-----------------------------------------------------------------------------
// Purpose: Internal helper to draw a range of text with a specified brush
//-----------------------------------------------------------------------------
void CSource2Surface::DrawTextRegionRange( CSource2CompositionLayer *pLayer, float x0, float y0, float x1, float y1, UITextOpacityMaskDataRange_t &maskRange, const FillBrushCollection_t &fill_brush_collection )
{
#ifdef PANORAMA_USE_S1WRAPPER
	HRenderTexture hTexture = (ResourceHandle_t)maskRange.m_hTexture;
	float flTextureOriginalWidthScale = 1.0f;
	float flTextureOriginalHeightScale = 1.0f;
#else
	CSource2UITexture *pSource2UITexture = (CSource2UITexture*)maskRange.m_hTexture;
	HRenderTexture hTexture = pSource2UITexture->GetTextureHandle();
	float flTextureOriginalWidthScale = pSource2UITexture->GetOriginalWidthScale();
	float flTextureOriginalHeightScale = pSource2UITexture->GetOriginalHeightScale();
#endif

	// x0,y0,x1,y1 is the rect encompassing the entire text layout (all lines)
	// maskRange.m_x0,x1,y0,y1 defines the rect of the run
	// flYTranslate is the offset of the first line within the layout
	x0 += maskRange.m_flStringOffsetX;
	x1 = x0 + maskRange.m_x1 - maskRange.m_x0;
	y0 += maskRange.m_flStringOffsetY;
	y1 = y0 + maskRange.m_y1 - maskRange.m_y0;

	float u0 = maskRange.m_x0 / maskRange.m_flTextureWidth;
	float u1 = maskRange.m_x1 / maskRange.m_flTextureWidth;

	float v0 = maskRange.m_y0 / maskRange.m_flTextureHeight;
	float v1 = maskRange.m_y1 / maskRange.m_flTextureHeight;

	FancyQuadParameters_t FancyParam(0);
	FancyParam.m_flZ = 0.0f;
	FancyParam.m_flVertexMin[0] = x0;
	FancyParam.m_flVertexMin[1] = y0;
	FancyParam.m_flVertexMax[0] = x1;
	FancyParam.m_flVertexMax[1] = y1;
	FancyParam.m_flTexCoordMin[0] = u0;
	FancyParam.m_flTexCoordMin[1] = v0;
	FancyParam.m_flTexCoordMax[0] = u1;
	FancyParam.m_flTexCoordMax[1] = v1;
	FancyParam.m_flOpacityTexCoordMin[0] = 0;
	FancyParam.m_flOpacityTexCoordMin[1] = 0;
	FancyParam.m_flOpacityTexCoordMax[0] = 1;
	FancyParam.m_flOpacityTexCoordMax[1] = 1;

	FancyQuadBrush_t FancyBrush;

	FancyQuadDraw_t fancyQuadDraw;
	fancyQuadDraw.m_hTexture0 = hTexture;
	fancyQuadDraw.m_flTexture0TexCoordScale[0] = flTextureOriginalWidthScale;
	fancyQuadDraw.m_flTexture0TexCoordScale[1] = flTextureOriginalHeightScale;
	fancyQuadDraw.m_nWide = -1;
	fancyQuadDraw.m_nTall = -1;
	fancyQuadDraw.m_pQuadParameters = &FancyParam;
	fancyQuadDraw.m_pQuadBrush = &FancyBrush;
	fancyQuadDraw.m_flTextureWidth = maskRange.m_flTextureWidth;
	fancyQuadDraw.m_flTextureHeight = maskRange.m_flTextureHeight;
	fancyQuadDraw.m_bIsAlphaTexture = true;
	fancyQuadDraw.m_bRawCoords = true;
	fancyQuadDraw.m_bClipToLayer = true;
	fancyQuadDraw.m_pVMatrix = pLayer->AccessPushedMatrix();
	//const E2DTextureFormat texFmt = pSource2UITexture->GetFormat();
	//fancyQuadDraw.m_bIsYCoCgTexture = ( texFmt == k_EFormatDXT5 || texFmt == k_EFormatDXT1 );

	for ( const FillBrush_t *pFillBrush : fill_brush_collection.fill_brush )
	{
		SetFancyQuadFillBrush( FancyBrush, *pFillBrush, x0, y0 );
		if ( hTexture.IsValid() )
		{
			DrawFancyQuad( &fancyQuadDraw );
		}
	}
}


static ConVar s_disabletextshadowcache( "pan_disabletextshadowcache", "0", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );

void CSource2Surface::DrawTextRegion( const RenderTextRegionCommand_t &renderCommand )
{
	if ( s_disabletextshadowcache.GetBool() ) return DrawTextRegionOriginal( renderCommand );

	if ( s_convarPanoramaDisableDrawText.GetBool() )
		return;

	VPROF( "CSource2Surface::DrawTextRegion" );
	//VPROF_BUDGET( "Panorama DrawTextRegion", VPROF_BUDGETGROUP_GAME );

	if ( m_stackCompositionLayers.Count() == 0 )
	{
		Assert( m_stackCompositionLayers.Count() > 0 );
		return;
	}

	CSource2CompositionLayer *pLayer = m_stackCompositionLayers[m_stackCompositionLayers.Count() - 1];
	if ( pLayer->BIsDrawing() )
	{
		pLayer->ActivateRenderTarget();

		float x0, y0, x1, y1;
		x0 = renderCommand.top_left.x;
		y0 = renderCommand.top_left.y;
		x1 = renderCommand.bottom_right.x;
		y1 = renderCommand.bottom_right.y;

		// Check rect has valid area
		if ( x1 <= x0 || y1 <= y0 )
			return;

		float flLineHeight = k_flFloatNotSet;
		if ( renderCommand.line_height > 0 )
			flLineHeight = renderCommand.line_height;

		// get textures
		UITextOpacityMaskData_t *pResult = GetCachedTextOpacityMask( renderCommand.raw_text, renderCommand.raw_text_bytes, renderCommand.text_chars, renderCommand.text_encoding, 
			x0, y0, x1, y1, flLineHeight, renderCommand.text_align, renderCommand.wrapping, renderCommand.ellipsis, renderCommand.default_format, renderCommand.range_formats );

		if( pResult && ( pResult->m_cRangeData !=0 ) )
		{
			// draw shadow if present first using shadow color
			if ( renderCommand.text_shadow && ( renderCommand.text_shadow->blur_radius != 0.0 ) &&
				 !s_convarPanoramaDisableDrawTextShadow.GetBool()
				)
			{
				float flXOffset = renderCommand.text_shadow->horizontal_offset;
				float flYOffset = renderCommand.text_shadow->vertical_offset;

				// Setup shadow fill brush
				// Setup shadow fill brush. Note that we handle the memory ourselves on the stack rather than letting it allocate normally.
				FillBrushCollection_t shadowBrushCollection;
				CRenderDataListBuilder< FillBrush_t > shadowBrushCollectionBuilder( shadowBrushCollection.fill_brush, nullptr );
				CRenderDataListBuilder< FillBrush_t >::DefaultListNode_t brushListNode;
				memset( &brushListNode, 0, sizeof( brushListNode ) );
				uint32 unShadowColor = renderCommand.text_shadow->color;

				brushListNode.entry.eFillBrushType = k_EFillBrushType_Color;
				brushListNode.entry.opacity = 1.0f;
				brushListNode.entry.color_rgba = unShadowColor;
				shadowBrushCollectionBuilder.AddNodeToTail( &brushListNode );

				// Basic fast text shadows (css property text-shadow-fast) are flagged using a value of -1.0 for blur radius and strength 
				if( s_convarPanoramaForceFastTextShadow.GetBool() || (renderCommand.text_shadow->blur_radius < 0.0f && renderCommand.text_shadow->strength < 0.0f) )
				{
					if( (flXOffset > 0.0f) || (flYOffset > 0.0f) )
					{
						for( int iTextMaskRegion = 0; iTextMaskRegion < pResult->m_cRangeData; iTextMaskRegion++ )
						{
							if( pResult->m_pRangeData[iTextMaskRegion].m_hTexture )
								DrawTextRegionRange( pLayer, x0 + flXOffset, y0 + flYOffset, x1 + flXOffset, y1 + flYOffset, pResult->m_pRangeData[iTextMaskRegion], shadowBrushCollection );
						}
					}
				}
				else
				{
					CRenderAttributes renderAttributes;

					int xPadding = clamp( (x1 - x0) * 0.1, 10, 100 );
					int yPadding = clamp( (y1 - y0) * 0.1, 10, 100 );

					int nWidth = x1 - x0 + (xPadding * 2);
					int nHeight = y1 - y0 + (yPadding * 2);

#if PANDX_DRAW
 					BasicQuad_t *pQuad = PanDxGetBasicQuadPtr();
#else
					BasicQuad_t pQuad[ 1 ];
#endif
 					pQuad->BqInit( 0, 0, nWidth, nHeight );

					// Here we check that whether we have the shadow in the m_TextShadowCache

					TextShadowKey_t textShadowKey;
					textShadowKey.data = *renderCommand.text_shadow;
					textShadowKey.uniqueID = pResult->m_nUniqueId;
					
					CSource2CompositionLayer *pVerticalBlurLayer = m_TextShadowCache.Find( textShadowKey, m_flCurrentRenderFrameTime );

					if ( !pVerticalBlurLayer )
					{

						int nVerticalRenderTargetWidth = nWidth;
						int nVerticalRenderTargetHeight = nHeight;

						// Create temporary render target to draw into and blur...
						pVerticalBlurLayer = GetCompositionLayer( nVerticalRenderTargetWidth, nVerticalRenderTargetHeight, false, false, "textshadow_blur" );
						pVerticalBlurLayer->Clear();


						m_stackCompositionLayers.AddToTail( pVerticalBlurLayer );
						pVerticalBlurLayer->ActivateRenderTargetAndClear();

						float x0Shadow = xPadding + flXOffset;
						float x1Shadow = x0Shadow + (x1 - x0);
						float y0Shadow = yPadding + flYOffset;
						float y1Shadow = y0Shadow + (y1 - y0);

						Rect_t rectScissor( 0, 0, nWidth, nHeight );
						m_pRenderContext->SetScissorRect( rectScissor );

						// draw background with our shadow fillbrush
						for ( int iTextMaskRegion = 0; iTextMaskRegion < pResult->m_cRangeData; iTextMaskRegion++ )
						{
							if ( pResult->m_pRangeData[ iTextMaskRegion ].m_hTexture )
							{
								DrawTextRegionRange( pVerticalBlurLayer, x0Shadow, y0Shadow, x1Shadow, y1Shadow, pResult->m_pRangeData[ iTextMaskRegion ], shadowBrushCollection );

								float flStrength = MIN( renderCommand.text_shadow->strength, 10.0f );
								while ( flStrength > 1.0f )
								{
									float flOffset = flStrength - 1.0f;

									DrawTextRegionRange( pVerticalBlurLayer, x0Shadow + flOffset, y0Shadow, x1Shadow + flOffset, y1Shadow, pResult->m_pRangeData[ iTextMaskRegion ], shadowBrushCollection );
									DrawTextRegionRange( pVerticalBlurLayer, x0Shadow - flOffset, y0Shadow, x1Shadow - flOffset, y1Shadow, pResult->m_pRangeData[ iTextMaskRegion ], shadowBrushCollection );
									DrawTextRegionRange( pVerticalBlurLayer, x0Shadow, y0Shadow + flOffset, x1Shadow, y1Shadow + flOffset, pResult->m_pRangeData[ iTextMaskRegion ], shadowBrushCollection );
									DrawTextRegionRange( pVerticalBlurLayer, x0Shadow, y0Shadow - flOffset, x1Shadow, y1Shadow - flOffset, pResult->m_pRangeData[ iTextMaskRegion ], shadowBrushCollection );

									flStrength -= 1.0f;
								}
							}
						}

						float flBlurRadius = renderCommand.text_shadow->blur_radius;
						float flBlurPasses = 1.0f;
						const float flMaxBlurPerPass = 12.0f;
						while ( flBlurRadius > flMaxBlurPerPass )
						{
							flBlurPasses++;
							flBlurRadius -= flMaxBlurPerPass;
						}

						int nMaxSamples = 1 + Float2Int( ceil( flMaxBlurPerPass / 1.5f ) );
						float *pSampleWeights = StackAlloc( float, nMaxSamples );
						float *pSampleOffsets = StackAlloc( float, nMaxSamples );

						HRenderTexture hBlurLayer = g_pSceneSystem->GetWellKnownRenderTarget( SCENE_RTGT_SCRATCH_TEXTURE_8888, SCENE_RTSIZE_FRAMEBUFFER, 0 );
						CTextureDesc const *dstDesc = g_pRenderDevice->GetTextureDesc( hBlurLayer );

						for ( float flBlurPass = 0.0f; flBlurPass < flBlurPasses; flBlurPass += 1.0f )
						{
							float flBlurThisPass = flBlurRadius;
							bool bLastPass = false;
							if ( flBlurPass + 1.0f < flBlurPasses )
								flBlurThisPass = flMaxBlurPerPass;
							else
								bLastPass = true;

							int nNumSamples = g_pSceneUtils->CalculateLinearWeightsForGaussianBlur( flBlurThisPass, pSampleWeights, pSampleOffsets, nMaxSamples );

							RenderTargetDesc_t rtDesc( hBlurLayer, RENDER_TEXTURE_HANDLE_INVALID, RENDER_SRGB );
							m_pRenderContext->BindRenderTargets( rtDesc );

							RenderViewport_t renderViewport;
							renderViewport.Init( 0, 0, Min( nWidth + Max( 10, Ceil2Int( flBlurRadius ) ), (int)dstDesc->m_nWidth ), Min( nHeight + Max( 10, Ceil2Int( flBlurRadius ) ), (int)dstDesc->m_nHeight ) );
							m_pRenderContext->SetViewports( 1, &renderViewport );

							const Vector4D vecClearColors[ 1 ] = { Vector4D( 0, 0, 0, 0 ) };
							m_pRenderContext->Clear( vecClearColors, 1, RENDER_CLEAR_FLAGS_CLEAR_COLOR );

							renderViewport.Init( 0, 0, nWidth, nHeight );
							m_pRenderContext->SetViewports( 1, &renderViewport );

							renderAttributes.SetIntValue( ATTR_D_TEX2DFASTBLUR, nNumSamples - 1 );
							renderAttributes.SetVMatrixValue( ATTR_MatTransform, VMatrix::GetIdentityMatrix() );
							renderAttributes.SetFloatValue( ATTR_ViewportWidth, (float)nWidth );
							renderAttributes.SetFloatValue( ATTR_ViewportHeight, (float)nHeight );
							renderAttributes.SetFloatValue( ATTR_centerWeight, pSampleWeights[ 0 ] );

							float flInvWidth = (1.0f / (float)nVerticalRenderTargetWidth) * pVerticalBlurLayer->GetRTOriginalWidthScale();
							for ( int j = 1; j < nNumSamples; ++j )
							{
								renderAttributes.SetVector4DValue( RenderAttrVector4D_t( ATTR_sample0 + j ), Vector4D( pSampleOffsets[ j ] * flInvWidth, 0.0f, pSampleWeights[ j ], 0.0f ) );
							}

							pQuad->m_vUV[ 1 ].x = ((float)nWidth / (float)nVerticalRenderTargetWidth) * pVerticalBlurLayer->GetRTOriginalWidthScale();
							pQuad->m_vUV[ 2 ].x = ((float)nWidth / (float)nVerticalRenderTargetWidth) * pVerticalBlurLayer->GetRTOriginalWidthScale();
							pQuad->m_vUV[ 2 ].y = ((float)nHeight / (float)nVerticalRenderTargetHeight)* pVerticalBlurLayer->GetRTOriginalHeightScale();
							pQuad->m_vUV[ 3 ].y = ((float)nHeight / (float)nVerticalRenderTargetHeight)* pVerticalBlurLayer->GetRTOriginalHeightScale();

#if PANDX_DRAW
 							BasicQuad_t *pQuadLpH = PanDxGetBasicQuadPtr();
#else
							BasicQuad_t pQuadLpH[ 1 ];
#endif

							memcpy( pQuadLpH, pQuad, sizeof( BasicQuad_t ) );

							// Draw the current layer into horizontal blur surface
							DrawTexturedQuadInternal( m_hMaterial, &renderAttributes, pVerticalBlurLayer->GetTextureHandle(), pQuadLpH );


							// Draw Back into vertical

							{
								RenderTargetDesc_t rtVerDesc( pVerticalBlurLayer->GetTextureHandle(), RENDER_TEXTURE_HANDLE_INVALID, RENDER_SRGB );
								m_pRenderContext->BindRenderTargets( rtVerDesc );

								renderViewport.Init( 0, 0, Min( nWidth + Max( 10, Ceil2Int( flBlurRadius ) ), (int)nVerticalRenderTargetWidth ), Min( nHeight + Max( 10, Ceil2Int( flBlurRadius ) ), (int)nVerticalRenderTargetHeight ) );
								m_pRenderContext->SetViewports( 1, &renderViewport );

								m_pRenderContext->Clear( vecClearColors, 1, RENDER_CLEAR_FLAGS_CLEAR_COLOR );

								renderViewport.Init( 0, 0, nWidth, nHeight );
								m_pRenderContext->SetViewports( 1, &renderViewport );

								renderAttributes.SetIntValue( ATTR_D_TEX2DFASTBLUR, nNumSamples - 1 );

								renderAttributes.SetVMatrixValue( ATTR_MatTransform, VMatrix::GetIdentityMatrix() );
								renderAttributes.SetFloatValue( ATTR_ViewportWidth, (float)nWidth );
								renderAttributes.SetFloatValue( ATTR_ViewportHeight, (float)nHeight );
								renderAttributes.SetFloatValue( ATTR_centerWeight, pSampleWeights[ 0 ] );

								float flInvHeight = 1.0f / dstDesc->m_nHeight;
								for ( int j = 1; j < nNumSamples; ++j )
								{
									renderAttributes.SetVector4DValue( RenderAttrVector4D_t( ATTR_sample0 + j ), Vector4D( 0.0f, pSampleOffsets[ j ] * flInvHeight, pSampleWeights[ j ], 0.0f ) );
								}

								pQuad->m_vUV[ 1 ].x = ((float)nWidth / (float)dstDesc->m_nWidth);
								pQuad->m_vUV[ 2 ].x = ((float)nWidth / (float)dstDesc->m_nWidth);
								pQuad->m_vUV[ 2 ].y = ((float)nHeight / (float)dstDesc->m_nHeight);
								pQuad->m_vUV[ 3 ].y = ((float)nHeight / (float)dstDesc->m_nHeight);

#if PANDX_DRAW
								BasicQuad_t *pQuadLpV = PanDxGetBasicQuadPtr();
#else
								BasicQuad_t pQuadLpV[ 1 ];
#endif

								memcpy( pQuadLpV, pQuad, sizeof( BasicQuad_t ) );

								// now draw back in using vertical blur
								DrawTexturedQuadInternal( m_hMaterial, &renderAttributes, hBlurLayer, pQuadLpV );
							}
						}

						// Take the layer off the layer stack
						m_stackCompositionLayers.Remove( m_stackCompositionLayers.Count() - 1 );


						// Cache shadow
						if ( !textShadowKey.data.animating )
						{
							m_TextShadowCache.Insert( textShadowKey, pVerticalBlurLayer, m_flCurrentRenderFrameTime );
						}
						else
						{
							m_FreeLayers.Insert( pVerticalBlurLayer, pVerticalBlurLayer, m_flCurrentRenderFrameTime );

						}

					}

					pQuad->m_vPosition[0].x = x0 - xPadding;
					pQuad->m_vPosition[0].y = y0 - yPadding;
					pQuad->m_vPosition[1].x = x1 + xPadding;
					pQuad->m_vPosition[1].y = y0 - yPadding;
					pQuad->m_vPosition[2].x = x1 + xPadding;
					pQuad->m_vPosition[2].y = y1 + yPadding;
					pQuad->m_vPosition[3].x = x0 - xPadding;
					pQuad->m_vPosition[3].y = y1 + yPadding;

					pLayer->ActivateRenderTarget();


					RectBounds_t r;
					pLayer->GetCurrentClipRect( r );
					Rect_t rScissor( RoundFloatToInt( r.left ), RoundFloatToInt( r.top ), RoundFloatToInt( r.right - r.left ), RoundFloatToInt( r.bottom - r.top ) );
					if( rScissor.x < 0 )
					{
						rScissor.width += rScissor.x;
						rScissor.x = 0;
					}

					if( rScissor.y < 0 )
					{
						rScissor.height += rScissor.y;
						rScissor.y = 0;
					}

					m_pRenderContext->SetScissorRect( rScissor );


					// Draw shadow to layer


					{
						renderAttributes.SetVector2DValue( ATTR_UVClamp, Vector2D( 10.0, 10.0 ) );
						renderAttributes.SetIntValue( ATTR_D_TEX2DBLUR, 0 );
						renderAttributes.SetIntValue( ATTR_D_TEX2DDOWNSAMPLE, 1 ); // Up&Downsample both use a simple bilinear shader w/clamp

						VMatrix *pMatrix = pLayer->AccessPushedMatrix();
						renderAttributes.SetVMatrixValue( ATTR_MatTransform, pMatrix ? pMatrix->Transpose() : VMatrix::GetIdentityMatrix() );
						renderAttributes.SetFloatValue( ATTR_BlurSigma, 0.0f );
						renderAttributes.SetFloatValue( ATTR_ViewportWidth, (float)pLayer->GetWidth() );
						renderAttributes.SetFloatValue( ATTR_ViewportHeight, (float)pLayer->GetHeight() );

						pQuad->m_vUV[0].x = 0.0f;
						pQuad->m_vUV[0].y = 0.0f;
						pQuad->m_vUV[1].x = 1.0f * pVerticalBlurLayer->GetRTOriginalWidthScale();
						pQuad->m_vUV[1].y = 0.0f;
						pQuad->m_vUV[2].x = 1.0f * pVerticalBlurLayer->GetRTOriginalWidthScale();
						pQuad->m_vUV[2].y = 1.0f * pVerticalBlurLayer->GetRTOriginalHeightScale();
						pQuad->m_vUV[3].x = 0.0f;
						pQuad->m_vUV[3].y = 1.0f * pVerticalBlurLayer->GetRTOriginalHeightScale();

						DrawTexturedQuadInternal( m_hMaterial, &renderAttributes, pVerticalBlurLayer->GetTextureHandle(), pQuad );
					}



					// Cache shadow

					m_TextShadowCache.Touch( textShadowKey, m_flCurrentRenderFrameTime );

				}
			}


			// draw foreground with actual fillbrush
			for ( int iTextMaskRegion = 0; iTextMaskRegion < pResult->m_cRangeData; iTextMaskRegion++ )
			{
				const FillBrushCollection_t *pFillBrushCollection = &renderCommand.default_format.fill_brush_collection;

				int iColorRangeFormat = pResult->m_pRangeData[ iTextMaskRegion ].m_iColorIndex;
				if ( iColorRangeFormat != UITextOpacityMaskDataRange_t::k_iColorIndexUnset )
				{
					int i = 0;
					for ( const RenderTextRangeFormat_t *pRangeFormat : renderCommand.range_formats )
					{
						if ( i == iColorRangeFormat )
						{
							pFillBrushCollection = &pRangeFormat->format.fill_brush_collection;
							break;
						}
						++i;
					}
				}

				if ( pResult->m_pRangeData[ iTextMaskRegion ].m_hTexture )
					DrawTextRegionRange( pLayer, x0, y0, x1, y1, pResult->m_pRangeData[ iTextMaskRegion ], *pFillBrushCollection );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Switch to the window framebuffer and reset related state
//-----------------------------------------------------------------------------

static ResourceData_t s_BackBuff { 0, RESOURCE_TYPE_BACKBUFFER };
static HRenderTexture s_hBackBuff = &s_BackBuff;

void CSource2Surface::GetBackBufferRenderTarget( HRenderTexture &hRT, RenderTargetDesc_t &rtDesc )
{
	rtDesc = m_pSceneLayer->GetRenderTargetDesc();
	hRT = s_hBackBuff;
}

void CSource2Surface::ActivateBackBufferRenderTarget()
{
	VPROF_BUDGET_THREAD( "CSource2Surface::ActivateRenderTarget ", VPROF_BUDGETGROUP_TENFOOT );

	RenderTargetDesc_t rtDesc = m_pSceneLayer->GetRenderTargetDesc();

	m_pRenderContext->BindRenderTargets( rtDesc );

	RenderViewport_t renderViewport;
	renderViewport.Init( 0, 0, m_unWindowWidth, m_unWindowHeight );

	m_pRenderContext->SetViewports( 1, &renderViewport );

	// Do not overwrite blend state if specified in PushPanelContextInLayer render commands
	if ( m_nBlendStateOverridden == 0 )
	{
		m_hCurrentBlendState = m_hPremultipliedAlphaBlendState;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called to tell us to free all cached GPU resources that we can
//-----------------------------------------------------------------------------
void CSource2Surface::ClearGPUResources()
{
	FOR_EACH_VEC( m_stackCompositionLayers, i )
	{
		SAFE_DELETE( m_stackCompositionLayers[i] );
	}
	m_stackCompositionLayers.RemoveAll();

	{
		AUTO_LOCK( m_MutexReservedLayers );
		m_ReservedLayers.DeleteAll();
	}

	m_FreeLayers.DeleteAll();

	m_OuterShadowLayers.DeleteAll();
	m_ImageShadowLayers.DeleteAll();
	m_TextShadowCache.DeleteAll();

	TermBlurLayerCache();

	m_RenderTargetCache.PurgeOverSizeLimit( 0 );

	m_pTextTextureCache->Purge();
    m_pTextLayoutDrawCache->Clear();
}


//-----------------------------------------------------------------------------
// Purpose: EndFrame
//-----------------------------------------------------------------------------
void CSource2Surface::EndFrame( const EndFrameRenderCommand_t &renderCommand )
{
	VPROF_BUDGET_THREAD( "CSource2Surface::EndFrame", VPROF_BUDGETGROUP_TENFOOT );

	if ( m_stackCompositionLayers.Count() > 0 )
	{
		Assert( m_stackCompositionLayers.Count() == 1 );
		CSource2CompositionLayer *pLayer = m_stackCompositionLayers[m_stackCompositionLayers.Count() - 1];

		AssertMsgOnce( pLayer->BIsDrawing(), "Need to still be drawing for the cursor" );

		pLayer->ActivateRenderTarget();

		// render the mouse cursor into the main FBO
#if 0
		int32 nMouseTextureID = 0;
		if ( renderMsg.BodyConst().has_mouse_cursor_texture_id() )
			nMouseTextureID = renderCommand.BodyConst().mouse_cursor_texture_id();
		if ( nMouseTextureID != 0 )
		{
			Vector2D ptHotspot;
			ptHotspot.x = renderCommand.BodyConst().mouse_cursor_hotspot_x();
			ptHotspot.y = renderCommand.BodyConst().mouse_cursor_hotspot_y();
			DrawMouseCursor( nMouseTextureID, ptHotspot );
		}
#endif

		if ( s_convarPanoramaRenderStats.GetBool() )
		{
			float64 flNow = Plat_FloatTime();
			m_frameStats.m_flFrameTime = flNow - m_frameStats.m_flFrameTime;
			m_frameStats.m_nCompositionLayersFreeCache = m_FreeLayers.Count();
			m_frameStats.m_nFreeCacheSizeInBytes = m_FreeLayers.RenderTargetSizeInBytes();
			{
				AUTO_LOCK( m_MutexReservedLayers );
				m_frameStats.m_nCompositionLayersReservedCache = m_ReservedLayers.Count();
				m_frameStats.m_nReservedCacheSizeInBytes = m_ReservedLayers.RenderTargetSizeInBytes();
			}
			m_frameStats.m_nCompositionLayersOuterShadowCache = m_OuterShadowLayers.Count();
			m_frameStats.m_nOuterShadowCacheSizeInBytes = m_OuterShadowLayers.RenderTargetSizeInBytes();
			m_frameStats.m_nCompositionLayersImageShadowCache = m_ImageShadowLayers.Count();
			m_frameStats.m_nImageShadowCacheSizeInBytes = m_ImageShadowLayers.RenderTargetSizeInBytes();

			m_frameStats.m_nTextShadowCache = m_TextShadowCache.Count();
			m_frameStats.m_nTextShadowCacheSizeInBytes = m_TextShadowCache.RenderTargetSizeInBytes();

			m_frameStats.m_nRTCacheCount = m_RenderTargetCache.Count();
			m_frameStats.m_nRTCacheSizeInBytes = m_RenderTargetCache.SizeInBytes();

			StatsBlurLayerCache( m_frameStats.m_nCompositionLayersBlurCache, m_frameStats.m_nBlurCacheSizeInBytes );

#if (PANDX_DRAW)
			if ( g_bPanDx )
			{
				m_frameStats.m_nPanDxDrawCalls = g_PanDxBatching.m_nDrawCalls;
				m_frameStats.m_nPanDxBasicDrawCalls = g_PanDxBatching.m_nBasicDrawCalls;
				m_frameStats.m_nPanDxFancyDrawCalls = g_PanDxBatching.m_nFancyDrawCalls;
			}
#endif
			
			LogStats( m_frameStats, s_convarPanoramaRenderStatsPosX.GetFloat(), s_convarPanoramaRenderStatsPosY.GetFloat() );
		}

		// pop MUST be after the cursor drawing above so we are still in a draw call
		pLayer->PopClipLayersAndFlush();

		if ( !pLayer->BIsBackbuffer() )
		{
			// now draw all our accrued fbo image to the back buffer
			ActivateBackBufferRenderTarget();

			FancyQuadParameters_t FancyParam(0);
			FancyParam.m_flZ = 0.0f;
			if ( BBackBufferScalingNeeded() )
			{
				FancyParam.m_flVertexMin[0] = m_flTranslateBackbufferX;
				FancyParam.m_flVertexMin[1] = m_flTranslateBackbufferY;
				FancyParam.m_flVertexMax[0] = RoundFloatToInt( m_unSurfaceWidth*m_flScaleBackbufferX + m_flTranslateBackbufferX );
				FancyParam.m_flVertexMax[1] = RoundFloatToInt( m_unSurfaceHeight*m_flScaleBackbufferY + m_flTranslateBackbufferY );
			}
			else
			{
				FancyParam.m_flVertexMin[0] = 0.0f;
				FancyParam.m_flVertexMin[1] = 0.0f;
				FancyParam.m_flVertexMax[0] = m_unSurfaceWidth;
				FancyParam.m_flVertexMax[1] = m_unSurfaceHeight;
			}

			FancyParam.m_flTexCoordMin[0] = 0.0f;
			FancyParam.m_flTexCoordMin[1] = 0.0f;
			FancyParam.m_flTexCoordMax[0] = 1.0f;
			FancyParam.m_flTexCoordMax[1] = 1.0f;

			FancyQuadBrush_t FancyBrush( 1.0, 1.0, 1.0, 1.0);

			m_hCurrentBlendState = m_hAlphaBlendState;

			FancyQuadDraw_t fancyQuadDraw;
			fancyQuadDraw.m_hTexture0 = pLayer->GetTextureHandle();
			fancyQuadDraw.m_flTexture0TexCoordScale[0] = pLayer->GetRTOriginalWidthScale();
			fancyQuadDraw.m_flTexture0TexCoordScale[1] = pLayer->GetRTOriginalHeightScale();
			fancyQuadDraw.m_nWide = m_unWindowWidth;
			fancyQuadDraw.m_nTall = m_unWindowHeight;
			fancyQuadDraw.m_pQuadParameters = &FancyParam;
			fancyQuadDraw.m_pQuadBrush = &FancyBrush;
			fancyQuadDraw.m_flTextureWidth = -1.0f;
			fancyQuadDraw.m_flTextureHeight = -1.0f;
			fancyQuadDraw.m_bRawCoords = true;

			DrawFancyQuad( &fancyQuadDraw );
		}
	}
	
	// Delete any textures we've been told to free
// 7LS -- where are the textures now deleted ????	
//	while ( m_tsQueueTextureDeletes.Count() )
//	{
//		VPROF_BUDGET_THREAD( "EndFrame - texture delete", VPROF_BUDGETGROUP_TENFOOT );
//
//		uint32 unTextureID = 0;
//		if ( m_tsQueueTextureDeletes.PopItem( &unTextureID ) )
//		{
//			AUTO_LOCK( m_lockTextureMap );
//
//			int iMap = m_mapTextures.Find( unTextureID );
//			if ( iMap != m_mapTextures.InvalidIndex() )
//			{
//				IUITexture *pTexture = m_mapTextures[iMap];
//				m_mapTextures.RemoveAt( iMap );
//
//				delete pTexture;
//			}
//		}
//	}

	float flNow = m_flCurrentRenderFrameTime;
	{
		VPROF_BUDGET_THREAD( "EndFrame - composition lru", VPROF_BUDGETGROUP_TENFOOT );
		m_FreeLayers.Purge( flNow - 1.0f );
	}

	{
		VPROF_BUDGET_THREAD( "EndFrame - reserved lru", VPROF_BUDGETGROUP_TENFOOT );
		AUTO_LOCK( m_MutexReservedLayers );
		m_ReservedLayers.Purge( flNow - 2.0f );
	}

	{
		VPROF_BUDGET_THREAD( "EndFrame - outer shadow layers lru", VPROF_BUDGETGROUP_TENFOOT );
		m_OuterShadowLayers.Purge( flNow - 1.0f );
	}

	{
		VPROF_BUDGET_THREAD( "EndFrame - image shadow layers lru", VPROF_BUDGETGROUP_TENFOOT );
		m_ImageShadowLayers.Purge( flNow - 1.0f );
	}

	{
		VPROF_BUDGET( "EndFrame - opacity lru", VPROF_BUDGETGROUP_TENFOOT );
		m_pTextLayoutDrawCache->DeleteOlderEntriesToTextureCache( flNow - 1.0f, m_pTextTextureCache );
	}

	{
		VPROF_BUDGET( "EndFrame - rt cache cleanup", VPROF_BUDGETGROUP_TENFOOT );
		m_RenderTargetCache.PurgeOverSizeLimit( s_convarPanoramaRTCacheMaxSize.GetInt() );
	}

	float flEcoInterval = 1.0f / MAX( s_panorama_blur_ecomode_fps.GetFloat(), 1.0f );
	PurgeBlurLayerCache( flNow - flEcoInterval );

	// Purge text shadows

	m_TextShadowCache.Purge( flNow - 1.0f );

	// Ensure we have no dangling scissor rects 
	m_pRenderContext->SetScissorRects( 0, NULL );

#if (PANDX_DRAW)
	if ( g_bPanDx && IsPanDxInsideOwnDx() ) PanDxDisownDx();
#endif

}


//-----------------------------------------------------------------------------
// Purpose: Find a matching free composition layer, or create a new one
//-----------------------------------------------------------------------------
CSource2CompositionLayer * CSource2Surface::GetCompositionLayer( float flWidth, float flHeight, bool bNeedsDepth, bool bNeedsIntermediate, const char *pchType )
{
	CSource2CompositionLayer search( flWidth, flHeight, bNeedsDepth, bNeedsIntermediate );
	CSource2CompositionLayer *pLayer = m_FreeLayers.FindAndRemove( &search, m_flCurrentRenderFrameTime );
	if ( pLayer == NULL )
	{
		VPROF_BUDGET_THREAD( "CSource2Surface::GetCompositionLayer - create new", VPROF_BUDGETGROUP_TENFOOT );

		pLayer = new CSource2CompositionLayer( this, flWidth, flHeight, bNeedsDepth, bNeedsIntermediate, false, false, pchType );
		pLayer->Clear(); // make sure we initialize the layer to empty, on OSX this isn't automatic
	}
	else
	{
		pLayer->ResetToDefault();
	}

	return pLayer;
}


//-----------------------------------------------------------------------------
// Purpose: Force throwing out of a compositing layer, called from animation thread.  
// This makes sure we stop re-using a layer which the animation thread knows it's culling 
// render operations to and thus knows is now dirty and not re-usuable
//-----------------------------------------------------------------------------
void CSource2Surface::FreeCompositingLayer( const FreeCompositingLayerRenderCommand_t &renderCommand )
{
	AUTO_LOCK( m_MutexReservedLayers );
	CSource2CompositionLayer * pLayer = m_ReservedLayers.Find( renderCommand.layer_id, m_flCurrentRenderFrameTime );
	if ( pLayer )
	{
		m_ReservedLayers.Remove( renderCommand.layer_id );
		m_FreeLayers.Insert( pLayer, pLayer, m_flCurrentRenderFrameTime );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Thread safe function which lets another thread verify we have a cached 
// representation of a given layer so it can be re-rendered without a full redraw, 
// and also makes sure we don't LRU it right away moving it to the end of our list 
// for throw out.
//-----------------------------------------------------------------------------
bool CSource2Surface::PingCompositingLayer( uint64 ulLayerID, float flWidth, float flHeight ) 
{ 
	AUTO_LOCK( m_MutexReservedLayers );
	return m_ReservedLayers.Touch( ulLayerID, flWidth, flHeight, m_flCurrentRenderFrameTime );
}


//-----------------------------------------------------------------------------
// Purpose: Thread safe function which lets another thread get the render target name of a given layer
// Returns nullptr if not found
//-----------------------------------------------------------------------------
const char *CSource2Surface::GetCompositionLayerRenderTargetName( uint64 ulLayerID )
{
	AUTO_LOCK( m_MutexReservedLayers );
	CSource2CompositionLayer * pLayer = m_ReservedLayers.Find( ulLayerID, m_flCurrentRenderFrameTime );
	if ( pLayer )
	{
		HRenderTexture hRenderTarget = pLayer->GetTextureHandle();
		if ( hRenderTarget.IsLoaded() )
		{
#ifdef PANORAMA_USE_S1WRAPPER
			S1Wrapper_Texture_t *pWrapperTexture = (S1Wrapper_Texture_t *)hRenderTarget.GetResourceHandle()->m_handle;
			ITexture* pTexture = pWrapperTexture->GetS1Texture();
			return pTexture->GetName();
#endif
		}
	}
	return nullptr;
}


//-----------------------------------------------------------------------------
// Purpose: Called to push a new compositing layer - this means we stop drawing into
// the current one for now, create/find a render target for the other one and draw into it 
// until a pop or push occurs again.
//-----------------------------------------------------------------------------
void CSource2Surface::PushCompositingLayer( const PushCompositingLayerRenderCommand_t &renderCommand )
{
	VPROF_BUDGET_THREAD( "CSource2Surface::PushCompositingLayer", VPROF_BUDGETGROUP_TENFOOT );
	//VPROF_BUDGET( "Panorama PushCompositingLayer", VPROF_BUDGETGROUP_GAME );

	// Flush current layer before moving to next
	if ( m_stackCompositionLayers.Count() )
	{
		CSource2CompositionLayer *pLayer = m_stackCompositionLayers[m_stackCompositionLayers.Count() - 1];
		pLayer->PopClipLayersAndFlush();
	}

	CSource2CompositionLayer *pLayer = NULL;
	{
		AUTO_LOCK( m_MutexReservedLayers );
		pLayer = m_ReservedLayers.Find( renderCommand.layer_id, m_flCurrentRenderFrameTime );
		if ( pLayer )
		{
			//
			// Check to make sure it matches our size and depth requirements
			// 
			bool bLayerHasDepthStencil = ( pLayer->GetDepthStencilTextureHandle() != RENDER_TEXTURE_HANDLE_INVALID );
			bool bLayerHasIntermediateTexture = ( pLayer->GetIntermediateTextureHandle() != RENDER_TEXTURE_HANDLE_INVALID );

			// The depth buffer (if present) is guaranteed to match the size of the layer, so a size disparity would already cause a mismatch (i.e. can't just use existing depth buffer)
			if ( ( pLayer->GetWidth() != ceil( renderCommand.width ) ) || ( pLayer->GetHeight() != ceil( renderCommand.height ) ) || ( bLayerHasDepthStencil != renderCommand.needs_depth ) || ( bLayerHasIntermediateTexture != renderCommand.needs_intermediate_texture ) )
			{
				m_ReservedLayers.Remove( renderCommand.layer_id );
				// Free up the layer, since it no longer matches our size/depth requirements, should only happen if also actually redrawing
				Assert( renderCommand.needs_clear );
				m_FreeLayers.Insert( pLayer, pLayer, m_flCurrentRenderFrameTime );
				pLayer = NULL;
			}
			else
			{
				pLayer->SetReusedFromCache( true );
			}
		}
	}

	if ( !pLayer )
	{
		if ( !renderCommand.offscreen_composition_layer )
		{
			pLayer = GetCompositionLayer( renderCommand.width, renderCommand.height, renderCommand.needs_depth, renderCommand.needs_intermediate_texture, "panel" );
		}
		else
		{
			// Panorama rendering to an offscreen render target that will be reused by the engine later (eg on model, ...)
			// Ensure the render target is the correct size (ie no extra padding)
			VPROF_BUDGET_THREAD( "CSource2Surface::GetCompositionLayer - create new (offscreen)", VPROF_BUDGETGROUP_TENFOOT );

			pLayer = new CSource2CompositionLayer( this, renderCommand.width, renderCommand.height, renderCommand.needs_depth, renderCommand.needs_intermediate_texture, false, true, "panel" );
			pLayer->Clear(); // make sure we initialize the layer to empty, on OSX this isn't automatic
		}
	}

	float flWidthDiscard = (ceil( renderCommand.width ) - renderCommand.width) / ceil( renderCommand.width );
	float flHeightDiscard = (ceil( renderCommand.height ) - renderCommand.height) / ceil( renderCommand.height );

	// Should now be valid, unless D3D actually failed...
	if ( pLayer )
	{
		uint32 rgba = renderCommand.composition_color;
		float r, g, b, a;
		ColorFromABGR( r, g, b, a, rgba );

		float flOpacity = renderCommand.opacity;

		// Combine opacity and color, and convert to pre-multiplied alpha
		r = Lerp( a, 1.0f, r ) * flOpacity;
		g = Lerp( a, 1.0f, g ) * flOpacity;
		b = Lerp( a, 1.0f, b ) * flOpacity;

		// Remember, pre-multiplied alpha in use!
		BasicQuad_t *pQuad = pLayer->AccessRenderQuad();

		Vector2D pos[] = {	Vector2D( renderCommand.layer_quad_top_left.x, renderCommand.layer_quad_top_left.y ),
							Vector2D( renderCommand.layer_quad_top_right.x, renderCommand.layer_quad_top_right.y ),
							Vector2D( renderCommand.layer_quad_bottom_right.x, renderCommand.layer_quad_bottom_right.y ),
							Vector2D( renderCommand.layer_quad_bottom_left.x, renderCommand.layer_quad_bottom_left.y ) };

		Vector2D uv[] = { Vector2D( 0.0, 0.0 ),
						  Vector2D( ( 1.0f - flWidthDiscard ), 0.0 ),
						  Vector2D( ( 1.0f - flWidthDiscard ), ( 1.0f - flHeightDiscard ) ),
						  Vector2D( 0.0, ( 1.0f - flHeightDiscard ) ) };

		pQuad->BqInit( Vector4D( r, g, b, flOpacity ), pos, uv, renderCommand.layer_quad_top_left.z);

		bool bMatrixIsIdentity;
		RenderMatrixToVMatrix( *pLayer->AccessMatrix(), renderCommand.transform, &bMatrixIsIdentity );

		if ( m_stackCompositionLayers.Count() > 0 )
		{
			// If we have a parent who was not a composition layer itself but pushed some panel context into it's own ancestor
			// composition layer than we need to accumulate the transform operations
			CSource2CompositionLayer *pParent = m_stackCompositionLayers.Tail();
			VMatrix *pParentTransforms = pParent->AccessPushedMatrix();
			if ( pParentTransforms )
				*pLayer->AccessMatrix() = (*pParentTransforms) * (*pLayer->AccessMatrix());
		}


		pLayer->SetContextID( renderCommand.layer_id );
		pLayer->SetHueShift( renderCommand.hue_shift );
		pLayer->SetSaturation( renderCommand.saturation );
		pLayer->SetBrightness( renderCommand.brightness );
		pLayer->SetContrast( renderCommand.contrast );
		pLayer->SetOffscreen( renderCommand.offscreen_composition_layer );

		pLayer->SetOpacityMaskTexture( renderCommand.opacity_mask_texture.GetTexture(), renderCommand.opacity_mask_opacity );

		pLayer->SetFullyOccluded( false );
		pLayer->ClearOccludedRegion();


		if ( renderCommand.occluded_left_edge <= renderCommand.layer_quad_top_left.x && renderCommand.occluded_top_edge <= renderCommand.layer_quad_top_left.y
			&& renderCommand.occluded_bottom_edge >= renderCommand.layer_quad_bottom_right.y && renderCommand.occluded_right_edge >= renderCommand.layer_quad_bottom_right.x )
		{
			pLayer->SetFullyOccluded( true );
		}
		else
		{
			// Check we aren't going to split up drawing into 4 ops just to avoid a tiny region, minumun area of 100 pixels to bother
			float flAreaOccluded = ( renderCommand.occluded_right_edge - renderCommand.occluded_left_edge ) * ( renderCommand.occluded_bottom_edge - renderCommand.occluded_top_edge );

			if ( flAreaOccluded > 100.0f )
			{
				pLayer->SetOccludedRegion( ceil( renderCommand.occluded_left_edge ), ceil( renderCommand.occluded_top_edge ), floor( renderCommand.occluded_right_edge ), floor( renderCommand.occluded_bottom_edge ) );
			}
		}

		if ( renderCommand.border_radius )
		{
			const RadiusData_t &radius = *renderCommand.border_radius;
			pLayer->SetCornerRadii( radius.top_left.horizontal, radius.top_left.vertical,
				radius.top_right.horizontal, radius.top_right.vertical,
				radius.bottom_right.horizontal, radius.bottom_right.vertical,
				radius.bottom_left.horizontal, radius.bottom_left.vertical );
		}
		else
		{
			pLayer->SetCornerRadii( 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f );
		}

		if ( renderCommand.border )
		{
			const BorderData_t &border = *renderCommand.border;
			pLayer->SetBorder( border.top.width, border.right.width, border.bottom.width, border.left.width,
				border.top.color, border.right.color, border.bottom.color, border.left.color );
		}
		else
		{
			pLayer->SetBorder( 0.0f, 0.0f, 0.0f, 0.0f, 0, 0, 0, 0 );
		}

		if ( renderCommand.box_shadow )
		{
			const BoxShadowData_t &shadow = *renderCommand.box_shadow;
			//Msg( "msg layer %p : %s %s %5.2f %5.2f %5.2f %5.2f %08x\n", pLayer, shadow.inset() ? "true " : "false", shadow.fill() ? "true " : "false", shadow.horizontal_offset(), shadow.vertical_offset(), shadow.blur_radius(), shadow.spread_distance(), shadow.color() );
			pLayer->SetBoxShadow( shadow.inset, shadow.fill, shadow.horizontal_offset, shadow.vertical_offset,
								  shadow.blur_radius, shadow.spread_distance, shadow.color );
		}
		else
		{
			pLayer->SetBoxShadow( false, false, 0.0f, 0.0f, 0.0f, 0.0f, 0 );
		}

		if ( renderCommand.radial_clip )
		{
			const RadialClipData_t &radial = *renderCommand.radial_clip;
			pLayer->SetRadialClip( true, radial.center_x, radial.center_y, radial.start_angle, radial.sector_angle );
		}
		else
		{
			pLayer->SetRadialClip( false, 0.0f, 0.0f, 0.0f, 0.0f );
		}

		pLayer->SetBlurValues( renderCommand.gaussian_blur.blurType, renderCommand.gaussian_blur.passes, renderCommand.gaussian_blur.stddev_hor, renderCommand.gaussian_blur.stddev_ver );

		pLayer->Set2DScaleFactors( renderCommand.scale2d_factors.x, renderCommand.scale2d_factors.y );

		pLayer->SetMixBlendMode( ( EMixBlendMode )renderCommand.mix_blend_mode );

		pLayer->SetPriorClampFractionalPixelPositions( m_eCurrentFractionalPixelPositions );
		if ( renderCommand.fractional_pixel_positions != k_EFractionalPixelPositionsDefault )
		{
			m_eCurrentFractionalPixelPositions = renderCommand.fractional_pixel_positions;
		}

		// Add to stack to keep track of this layer
		m_stackCompositionLayers.AddToTail( pLayer );

		// We don't actually clear and begin draw until we receive the clear message for the layer
		if ( renderCommand.needs_clear )
		{
#if 0
			Msg( "Clearing composition layer %s\n", CPanelIdentityString( pLayer->GetContextID() ).Get() );
#endif
			ClearCompositingLayer( pLayer );
		}

		// If we need to redraw every frame, then don't bother caching or letting any of our parents
		// cache. We can't reuse it anyways.
		if ( renderCommand.needs_redraw_every_frame )
		{
			FOR_EACH_VEC( m_stackCompositionLayers, i )
			{
				m_stackCompositionLayers[ i ]->SetShouldCache( false );
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Purpose: Get shader resource view for an opacity mask
//-----------------------------------------------------------------------------
HRenderTexture CSource2Surface::GetOpacityMaskShaderResourceViewForTexture( IUITexture *pTexture, float *pOriginalWidthScale, float *pOriginalHeightScale )
{
	VPROF_BUDGET_THREAD( "CSource2Surface::GetOpacityMaskShaderResourceViewForTexture", VPROF_BUDGETGROUP_TENFOOT );

	if ( pTexture )
	{
		CSource2UITexture *pSource2UITexture = (CSource2UITexture *)pTexture;
		if ( pOriginalWidthScale )
		{
			*pOriginalWidthScale = pSource2UITexture->GetOriginalWidthScale();
		}
		if ( pOriginalHeightScale )
		{
			*pOriginalHeightScale = pSource2UITexture->GetOriginalHeightScale();
		}

		return pSource2UITexture->GetTextureHandle();
	}

	return RENDER_TEXTURE_HANDLE_INVALID;
}


//-----------------------------------------------------------------------------
// Purpose: Called to create outer shadow layer for a given layer
//-----------------------------------------------------------------------------
CSource2CompositionLayer *CSource2Surface::GetOuterShadowLayer( const OuterShadowLayerParams_t &params )
{
	VPROF( "CSource2Surface::GetOuterShadowLayer");

	CSource2CompositionLayer *pShadowOutLayer = NULL;

	if ( params.shadowColor != 0x00000000 )
	{
		// First try to find a matching outer shadow layer from the cache
		pShadowOutLayer = m_OuterShadowLayers.FindAndRemove( params, m_flCurrentRenderFrameTime );
		if ( pShadowOutLayer )
		{
			return pShadowOutLayer;
		}

		
		float flPadding = ceil( params.flBlurRadius );

		// Make sure to bloat the original layer size by even dimensions for the
		// glow/shadow layer, or we won't be able to render in the exact center
		// and will end up with a shadow that jitters around when animated.
		int iSpreadDistance = int( params.flSpreadDistance + 2 ) & ~1;
		float flWidth = params.flWidthLayer + iSpreadDistance + (flPadding * 2.0f);
		float flHeight = params.flHeightLayer + iSpreadDistance + (flPadding * 2.0f);

		// Note: Composition layer will ceil() width/height, that's ok, we'll just draw slightly stretched, but when we draw into
		// the parent context we'll squish back down appropriately, which will result in pretty good linearly interpolated results for
		// sub pixel shadow boundaries.
		pShadowOutLayer = GetCompositionLayer( flWidth, flHeight, false, false, "shadow_outset" );
		pShadowOutLayer->Clear();

		m_stackCompositionLayers.AddToTail( pShadowOutLayer );

		float flshadowcolor[4];
		ColorFromABGR( flshadowcolor[0], flshadowcolor[1], flshadowcolor[2], flshadowcolor[3], params.shadowColor );
		flshadowcolor[0] *= flshadowcolor[3];
		flshadowcolor[1] *= flshadowcolor[3];
		flshadowcolor[2] *= flshadowcolor[3];

		pShadowOutLayer->ActivateRenderTargetAndClear();

		// We draw a bloated primitive around the original border; we want to
		// offset the corner radii by the size difference so that they still
		// align vertically and horizontally
		float flCornerOffset = ((pShadowOutLayer->GetWidth() - params.flWidthLayer) / 2) - flPadding;

		// set up to draw with corner rounding

		int nFlg = 0;

		if ( ( params.borderRadii[ 0 ] + params.borderRadii[ 1 ] + params.borderRadii[ 2 ] + params.borderRadii[ 3 ] 
			   + params.borderRadii[ 4 ] + params.borderRadii[ 5 ] + params.borderRadii[ 6 ] + params.borderRadii[ 7 ] ) > 0.01 )
		{
			nFlg = FancyQuadFlag_OuterCorner;
		}

		FancyQuadParameters_t FancyParam( nFlg);
		FancyParam.m_flZ = 0.0f;
		FancyParam.m_flVertexMin[0] = flPadding;
		FancyParam.m_flVertexMin[1] = flPadding;
		FancyParam.m_flVertexMax[0] = pShadowOutLayer->GetWidth() - flPadding;
		FancyParam.m_flVertexMax[1] = pShadowOutLayer->GetHeight() - flPadding;
		FancyParam.m_flTexCoordMin[0] = 0.0f;
		FancyParam.m_flTexCoordMin[1] = 0.0f; 
		FancyParam.m_flTexCoordMax[0] = 1.0f;
		FancyParam.m_flTexCoordMax[1] = 1.0f;
		FancyParam.m_flCornerRadii[0][0] = params.borderRadii[0] + flCornerOffset;		// outer
		FancyParam.m_flCornerRadii[0][1] = params.borderRadii[1] + flCornerOffset;
		FancyParam.m_flCornerRadii[1][0] = params.borderRadii[2] + flCornerOffset;
		FancyParam.m_flCornerRadii[1][1] = params.borderRadii[3] + flCornerOffset;
		FancyParam.m_flCornerRadii[2][0] = params.borderRadii[4] + flCornerOffset;
		FancyParam.m_flCornerRadii[2][1] = params.borderRadii[5] + flCornerOffset;
		FancyParam.m_flCornerRadii[3][0] = params.borderRadii[6] + flCornerOffset;
		FancyParam.m_flCornerRadii[3][1] = params.borderRadii[7] + flCornerOffset;

		FancyQuadBrush_t FancyBrush( flshadowcolor[ 0 ], flshadowcolor[ 1 ], flshadowcolor[ 2 ], flshadowcolor[ 3 ] );

		FancyQuadDraw_t fancyQuadDraw;
		fancyQuadDraw.m_nWide = pShadowOutLayer->GetWidth();
		fancyQuadDraw.m_nTall = pShadowOutLayer->GetHeight();
		fancyQuadDraw.m_pQuadParameters = &FancyParam;
		fancyQuadDraw.m_pQuadBrush = &FancyBrush;
		fancyQuadDraw.m_flTextureWidth = -1.0f;
		fancyQuadDraw.m_flTextureHeight = -1.0f;

		// Draw color layer into shadow output layer, with opacity mask from original layer to get correct border box.
		DrawFancyQuad( &fancyQuadDraw );

		if ( params.flBlurRadius > 0.0f )
		{
			HRenderTexture hBlurLayer = g_pSceneSystem->GetWellKnownRenderTarget( SCENE_RTGT_SCRATCH_TEXTURE_8888, SCENE_RTSIZE_FRAMEBUFFER, 0 );
		
#if PANDX_DRAW
			BasicQuad_t *pBlurQuad = PanDxGetBasicQuadPtr();
#else
			BasicQuad_t pBlurQuad[ 1 ];
#endif

			pBlurQuad->BqInit( 0, 0, flWidth, flHeight );
			for ( Vector2D& uv : pBlurQuad->m_vUV )
			{
				uv *= Vector2D( pShadowOutLayer->GetRTOriginalWidthScale(), pShadowOutLayer->GetRTOriginalHeightScale() );
			}


			enum { MAX_BLUR_RADIUS = 16 };
			float flClampedBlurRadius = MIN( params.flBlurRadius, float( MAX_BLUR_RADIUS ) );
			int nNumSamples = 1 + Float2Int( ceil( flClampedBlurRadius / 2.0f ) );
			float *pSampleWeights = StackAlloc( float, nNumSamples );
			float *pSampleOffsets = StackAlloc( float, nNumSamples );
			nNumSamples = g_pSceneUtils->CalculateLinearWeightsForGaussianBlur( flClampedBlurRadius, pSampleWeights, pSampleOffsets, nNumSamples );

			CRenderAttributes renderAttributes;
			renderAttributes.SetIntValue( ATTR_D_TEX2DFASTBLUR, nNumSamples - 1 );
			renderAttributes.SetVMatrixValue( ATTR_MatTransform, VMatrix::GetIdentityMatrix() );

			renderAttributes.SetFloatValue( ATTR_ViewportWidth, ( float )flWidth );
			renderAttributes.SetFloatValue( ATTR_ViewportHeight, ( float )flHeight );
			renderAttributes.SetFloatValue( ATTR_centerWeight, pSampleWeights[ 0 ] );

			RenderTargetDesc_t rtDesc( hBlurLayer, RENDER_TEXTURE_HANDLE_INVALID, RENDER_SRGB );
			m_pRenderContext->BindRenderTargets( rtDesc );

			CTextureDesc const *dstDesc = g_pRenderDevice->GetTextureDesc( hBlurLayer );

			RenderViewport_t renderViewport;
			renderViewport.Init( 0, 0, Min( flWidth + Max( 10.0f, flPadding ), (float)dstDesc->m_nWidth ), Min( flHeight + Max( 10.0f, flPadding ), (float)dstDesc->m_nHeight ) );
			m_pRenderContext->SetViewports( 1, &renderViewport );

			const Vector4D vecClearColors[1] = { Vector4D( 0, 0, 0, 0 ) };
			m_pRenderContext->Clear( vecClearColors, 1, RENDER_CLEAR_FLAGS_CLEAR_COLOR );

			renderViewport.Init( 0, 0, flWidth, flHeight );
			m_pRenderContext->SetViewports( 1, &renderViewport );

			float flInvWidth = ( 1.0f / flWidth ) * pShadowOutLayer->GetRTOriginalWidthScale();
			for ( int i = 1; i < nNumSamples; ++i )
			{
				renderAttributes.SetVector4DValue(RenderAttrVector4D_t(ATTR_sample0+i), Vector4D( pSampleOffsets[ i ] * flInvWidth, 0.0f, pSampleWeights[ i ], 0.0f ) );
			}

#if PANDX_DRAW
			BasicQuad_t *pBlurQuadH = PanDxGetBasicQuadPtr();
#else
			BasicQuad_t pBlurQuadH[ 1 ];
#endif

			memcpy( pBlurQuadH, pBlurQuad, sizeof( BasicQuad_t ) );

			// Draw the current layer into horizontal blur surface
			DrawTexturedQuadInternal( m_hMaterial, &renderAttributes, pShadowOutLayer->GetTextureHandle(), pBlurQuadH );

			// now render the blurred layer back onto the base surface
			pShadowOutLayer->ActivateRenderTargetAndClear();

			float flInvHeight = 1.0f / (float)m_unSurfaceHeight;
			for ( int i = 1; i < nNumSamples; ++i )
			{
				renderAttributes.SetVector4DValue( RenderAttrVector4D_t( ATTR_sample0 + i ), Vector4D( 0.0f, pSampleOffsets[ i ] * flInvHeight, pSampleWeights[ i ], 0.0f ) );
			}

			pBlurQuad->m_vUV[0].x = 0.0f;
			pBlurQuad->m_vUV[0].y = 0.0f;
			pBlurQuad->m_vUV[1].x = ( flWidth / (float)dstDesc->m_nWidth );
			pBlurQuad->m_vUV[1].y = 0.0f;
			pBlurQuad->m_vUV[2].x = ( flWidth / (float)dstDesc->m_nWidth );
			pBlurQuad->m_vUV[2].y = ( flHeight / (float)dstDesc->m_nHeight );
			pBlurQuad->m_vUV[3].x = 0.0f;
			pBlurQuad->m_vUV[3].y = ( flHeight / (float)dstDesc->m_nHeight );

			// now draw the horizontally blurred surface into the original surface using vertical blur
			DrawTexturedQuadInternal( m_hMaterial, &renderAttributes, hBlurLayer, pBlurQuad );
		}

		// If we aren't filling the inside of the box with the shadow as well, then clip it
		if ( !params.bFill )
		{
			int corners[2][2];

			// forestw: round toward inner rectangle, otherwise we get pixel gaps on scaling glowing main menu buttons
			corners[0][0] = ceil( (flWidth - params.flWidthLayer) * 0.5f );
			corners[0][1] = ceil( (flHeight - params.flHeightLayer) * 0.5f );
			corners[1][0] = floor( (flWidth - params.flWidthLayer) * 0.5f + params.flWidthLayer );
			corners[1][1] = floor( (flHeight - params.flHeightLayer) * 0.5f + params.flHeightLayer );

			// forestw: the D3D code uses an opacity mask to cut out the middle when rounding is used, here we just draw another fancyquad to do it
			// set up fancyquad to draw with corner rounding
			FancyParam.m_flZ = 0.0f;
			FancyParam.m_flVertexMin[0] = corners[0][0];
			FancyParam.m_flVertexMin[1] = corners[0][1];
			FancyParam.m_flVertexMax[0] = corners[1][0];
			FancyParam.m_flVertexMax[1] = corners[1][1];
			FancyParam.m_flTexCoordMin[0] = 0.0f;
			FancyParam.m_flTexCoordMin[1] = 0.0f;
			FancyParam.m_flTexCoordMax[0] = 1.0f;
			FancyParam.m_flTexCoordMax[1] = 1.0f;
			FancyParam.m_flCornerRadii[0][0] = params.borderRadii[0]; // outer
			FancyParam.m_flCornerRadii[0][1] = params.borderRadii[1];
			FancyParam.m_flCornerRadii[1][0] = params.borderRadii[2];
			FancyParam.m_flCornerRadii[1][1] = params.borderRadii[3];
			FancyParam.m_flCornerRadii[2][0] = params.borderRadii[4];
			FancyParam.m_flCornerRadii[2][1] = params.borderRadii[5];
			FancyParam.m_flCornerRadii[3][0] = params.borderRadii[6];
			FancyParam.m_flCornerRadii[3][1] = params.borderRadii[7];
			FancyParam.m_flBorderWidth[0] = 0.0f;
			FancyParam.m_flBorderWidth[1] = 0.0f;
			FancyParam.m_flBorderWidth[2] = 0.0f;
			FancyParam.m_flBorderWidth[3] = 0.0f;

			V_memset( &FancyBrush, 0, sizeof(FancyBrush) );
			FancyBrush.m_flColor[0][0] = 0.0f;
			FancyBrush.m_flColor[0][1] = 0.0f;
			FancyBrush.m_flColor[0][2] = 0.0f;
			FancyBrush.m_flColor[0][3] = 1.0f;
			FancyBrush.m_flGradientStartPoint[0] = 0.0f;
			FancyBrush.m_flGradientStartPoint[1] = 0.0f;
			FancyBrush.m_flGradientEndPoint[0] = 0.0f;
			FancyBrush.m_flGradientEndPoint[1] = 0.0f;
			FancyBrush.m_flGradientRadii[0] = 0.0f;
			FancyBrush.m_flGradientRadii[1] = 0.0f;
			FancyBrush.m_bIsLinearGradient = false;
			FancyBrush.m_bIsRadialGradient = false;

			// Draw color layer into shadow output layer, with opacity mask from original layer to get correct border box.
			// we need a special blendfunc to cut a hole into the alpha
			RsBlendStateHandle_t hPriorBlendState = m_hCurrentBlendState;
			m_hCurrentBlendState = m_hAlphaOnlyBlendState;

			fancyQuadDraw.Init();
			fancyQuadDraw.m_nWide = pShadowOutLayer->GetWidth();
			fancyQuadDraw.m_nTall = pShadowOutLayer->GetHeight();
			fancyQuadDraw.m_pQuadParameters = &FancyParam;
			fancyQuadDraw.m_pQuadBrush = &FancyBrush;
			fancyQuadDraw.m_flTextureWidth = -1.0f;
			fancyQuadDraw.m_flTextureHeight = -1.0f;

			DrawFancyQuad( &fancyQuadDraw );

			m_hCurrentBlendState = hPriorBlendState;
		}

		m_stackCompositionLayers.Remove( m_stackCompositionLayers.Count() - 1 );
	}

	// Shadow layer is returned, and must be put back in free stack by caller
	return pShadowOutLayer;
}


//-----------------------------------------------------------------------------
// Purpose: Called to draw and free a composition layer created with CreateOuterShadowLayer
//-----------------------------------------------------------------------------
void panorama::CSource2Surface::DrawOuterShadowLayer( float flWidthSourceLayer, float flHeightSourceLayer, float flXPosInParent, float flYPosInParent, float flScale2DX, float flScale2DY, float flOpacity, float *pflCornerRadii, VMatrix *pMatTransform, 
	float flWidthParentLayer, float flHeightParentLayer, CSource2CompositionLayer *pShadowLayer, bool bFill, float flHorOffset, float flVerOffset, float flBlurRadius, float flSpreadDistance, uint32 shadowColor, float flZ )
{
	VPROF_BUDGET_THREAD( "CSource2Surface::DrawOuterShadowLayer", VPROF_BUDGETGROUP_TENFOOT );

	float flPadding = ceil( flBlurRadius );

	// set up fancyquad to draw with corner rounding
	FancyQuadParameters_t FancyParam( FancyQuadFlag_OuterCorner);
	FancyParam.m_flZ = flZ;
	FancyParam.m_flVertexMin[0] = flXPosInParent - flPadding + flHorOffset;
	FancyParam.m_flVertexMin[1] = flYPosInParent - flPadding + flVerOffset;
	FancyParam.m_flVertexMax[0] = flXPosInParent + flWidthSourceLayer + flPadding + flHorOffset + flSpreadDistance;
	FancyParam.m_flVertexMax[1] = flYPosInParent + flHeightSourceLayer + flPadding + flVerOffset + flSpreadDistance;
	FancyParam.m_flTexCoordMin[0] = 0.0f;
	FancyParam.m_flTexCoordMin[1] = 0.0f;
	FancyParam.m_flTexCoordMax[0] = 1.0f;
	FancyParam.m_flTexCoordMax[1] = 1.0f;
	FancyParam.m_flCornerRadii[0][0] = pflCornerRadii[0]; //outer
	FancyParam.m_flCornerRadii[0][1] = pflCornerRadii[1];
	FancyParam.m_flCornerRadii[1][0] = pflCornerRadii[2];
	FancyParam.m_flCornerRadii[1][1] = pflCornerRadii[3];
	FancyParam.m_flCornerRadii[2][0] = pflCornerRadii[4];
	FancyParam.m_flCornerRadii[2][1] = pflCornerRadii[5];
	FancyParam.m_flCornerRadii[3][0] = pflCornerRadii[6];
	FancyParam.m_flCornerRadii[3][1] = pflCornerRadii[7];

	FancyQuadBrush_t FancyBrush( flOpacity, flOpacity, flOpacity, flOpacity);

	FancyQuadDraw_t fancyQuadDraw;
	fancyQuadDraw.m_hTexture0 = pShadowLayer->GetTextureHandle();
	fancyQuadDraw.m_flTexture0TexCoordScale[0] = pShadowLayer->GetRTOriginalWidthScale();
	fancyQuadDraw.m_flTexture0TexCoordScale[1] = pShadowLayer->GetRTOriginalHeightScale();
	fancyQuadDraw.m_nWide = flWidthParentLayer;
	fancyQuadDraw.m_nTall = flHeightParentLayer;
	fancyQuadDraw.m_pQuadParameters = &FancyParam;
	fancyQuadDraw.m_pQuadBrush = &FancyBrush;
	fancyQuadDraw.m_pVMatrix = pMatTransform;
	fancyQuadDraw.m_flScale2DX = flScale2DX;
	fancyQuadDraw.m_flScale2DY = flScale2DY;
	fancyQuadDraw.m_flTextureWidth = -1.0f;
	fancyQuadDraw.m_flTextureHeight = -1.0f;
	fancyQuadDraw.m_bClipToLayer = true;

	// Draw the current layer into its parent with corner rounding if needed
	DrawFancyQuad( &fancyQuadDraw );
}


//-----------------------------------------------------------------------------
// Purpose: Called to create inset shadow layer for a given layer
//-----------------------------------------------------------------------------
CSource2CompositionLayer *CSource2Surface::GetInsetShadowLayer( float flWidthLayer, float flHeightLayer, float *pBorderRadii, float *pBorderWidths, float flHorOffset, float flVerOffset, float flBlurRadius, float flSpreadDistance, uint32 shadowColor )
{
	VPROF( "CSource2Surface::GetInsetShadowLayer");
		
	CSource2CompositionLayer *pShadowOutLayer = NULL;
	if ( shadowColor != 0x00000000 )
	{
		float *pflOuterRaddi = pBorderRadii;
		float *pflBorderWidths = pBorderWidths;

		float flPadding = ceil( flBlurRadius );
		float flWidth = ceil( flPadding * 2.0f + flWidthLayer - pflBorderWidths[1] - pflBorderWidths[3] );
		float flHeight = ceil( flPadding * 2.0f + flHeightLayer - pflBorderWidths[0] - pflBorderWidths[2] );

		float flHalfSpread = flSpreadDistance / 2.0f;
		float rgInnerRaddi[8];
		rgInnerRaddi[0] = MAX( pflOuterRaddi[0] - pflBorderWidths[3] - flHalfSpread, 0.0f );
		rgInnerRaddi[1] = MAX( pflOuterRaddi[1] - pflBorderWidths[0] - flHalfSpread, 0.0f );
		rgInnerRaddi[2] = MAX( pflOuterRaddi[2] - pflBorderWidths[1] - flHalfSpread, 0.0f );
		rgInnerRaddi[3] = MAX( pflOuterRaddi[3] - pflBorderWidths[0] - flHalfSpread, 0.0f );
		rgInnerRaddi[4] = MAX( pflOuterRaddi[4] - pflBorderWidths[1] - flHalfSpread, 0.0f );
		rgInnerRaddi[5] = MAX( pflOuterRaddi[5] - pflBorderWidths[2] - flHalfSpread, 0.0f );
		rgInnerRaddi[6] = MAX( pflOuterRaddi[6] - pflBorderWidths[3] - flHalfSpread, 0.0f );
		rgInnerRaddi[7] = MAX( pflOuterRaddi[7] - pflBorderWidths[2] - flHalfSpread, 0.0f );

		// Note: Composition layer will ceil() width/height, that's ok, we'll just draw slightly stretched, but when we draw into
		// the parent context we'll squish back down appropriately, which will result in pretty good linearly interpolated results for
		// sub pixel shadow boundaries.
		pShadowOutLayer = GetCompositionLayer( flWidth, flHeight, false, false, "shadow_inset" );
		pShadowOutLayer->Clear();
		
		// Draw the unblurred shadow
		pShadowOutLayer->ActivateRenderTarget();
		pShadowOutLayer->DrawInsetShadowIntoLayer( this, flPadding, flWidth, flHeight, flHorOffset, flVerOffset, flSpreadDistance, shadowColor, rgInnerRaddi );

		m_stackCompositionLayers.AddToTail( pShadowOutLayer );

		if ( flBlurRadius > 0.0f )
		{
			HRenderTexture hBlurLayer = g_pSceneSystem->GetWellKnownRenderTarget( SCENE_RTGT_SCRATCH_TEXTURE_8888, SCENE_RTSIZE_FRAMEBUFFER, 0 );

#if PANDX_DRAW
			BasicQuad_t *pBlurQuad = PanDxGetBasicQuadPtr();
#else
			BasicQuad_t pBlurQuad[ 1 ];
#endif

			pBlurQuad->BqInit( 0, 0, flWidth, flHeight );
			for ( Vector2D& uv : pBlurQuad->m_vUV )
			{
				uv *= Vector2D( pShadowOutLayer->GetRTOriginalWidthScale(), pShadowOutLayer->GetRTOriginalHeightScale() );
			}

			RenderTargetDesc_t rtDesc( hBlurLayer, RENDER_TEXTURE_HANDLE_INVALID, RENDER_SRGB );
			m_pRenderContext->BindRenderTargets( rtDesc );

			CTextureDesc const *dstDesc = g_pRenderDevice->GetTextureDesc( hBlurLayer );

			RenderViewport_t renderViewport;
			renderViewport.Init( 0, 0, Min( flWidth + Max( 10.0f, flPadding ), (float)dstDesc->m_nWidth ), Min( flHeight + Max( 10.0f, flPadding ), (float)dstDesc->m_nHeight ) );
			m_pRenderContext->SetViewports( 1, &renderViewport );

			const Vector4D vecClearColors[1] = { Vector4D( 0, 0, 0, 0 ) };
			m_pRenderContext->Clear( vecClearColors, 1, RENDER_CLEAR_FLAGS_CLEAR_COLOR );

			renderViewport.Init( 0, 0, flWidth, flHeight );
			m_pRenderContext->SetViewports( 1, &renderViewport );

			CRenderAttributes renderAttributes;
			renderAttributes.SetIntValue( ATTR_D_TEX2DBLUR, 1 );
			renderAttributes.SetVMatrixValue( ATTR_MatTransform, VMatrix::GetIdentityMatrix() );
			renderAttributes.SetFloatValue( ATTR_ViewportWidth, (float)flWidth );
			renderAttributes.SetFloatValue( ATTR_ViewportHeight, (float)flHeight );
			renderAttributes.SetFloatValue( ATTR_BlurSigma, flBlurRadius / 2.0f );
			renderAttributes.SetVector2DValue( ATTR_BlurMultiplyVec, Vector2D( ( 1.0f / pShadowOutLayer->GetWidth() ) *  pShadowOutLayer->GetRTOriginalWidthScale(), 0.0f ) );

#if PANDX_DRAW
			BasicQuad_t *pBlurQuadH = PanDxGetBasicQuadPtr();
#else
			BasicQuad_t pBlurQuadH[ 1 ];
#endif

			memcpy( pBlurQuadH, pBlurQuad, sizeof( BasicQuad_t ) );

			// draw the current layer into horizontal blur surface
			DrawTexturedQuadInternal( m_hMaterial, &renderAttributes, pShadowOutLayer->GetTextureHandle(), pBlurQuadH );

			pShadowOutLayer->ActivateRenderTargetAndClear();

			renderAttributes.SetFloatValue( ATTR_ViewportWidth, (float)pShadowOutLayer->GetWidth() );
			renderAttributes.SetFloatValue( ATTR_ViewportHeight, (float)pShadowOutLayer->GetHeight() );
			renderAttributes.SetFloatValue( ATTR_BlurSigma, flBlurRadius / 2.0f );
			renderAttributes.SetVector2DValue( ATTR_BlurMultiplyVec, Vector2D( 0.0f, 1.0f / dstDesc->m_nHeight ) );

			pBlurQuad->m_vUV[0].x = 0.0f;
			pBlurQuad->m_vUV[0].y = 0.0f;
			pBlurQuad->m_vUV[1].x = (flWidth / (float)dstDesc->m_nWidth);
			pBlurQuad->m_vUV[1].y = 0.0f;
			pBlurQuad->m_vUV[2].x = (flWidth / (float)dstDesc->m_nWidth);
			pBlurQuad->m_vUV[2].y = (flHeight / (float)dstDesc->m_nHeight);
			pBlurQuad->m_vUV[3].x = 0.0f;
			pBlurQuad->m_vUV[3].y = (flHeight / (float)dstDesc->m_nHeight);

			// now draw back to the original surface using vertical blur
			DrawTexturedQuadInternal( m_hMaterial, &renderAttributes, hBlurLayer, pBlurQuad );
		}

		m_stackCompositionLayers.Remove( m_stackCompositionLayers.Count() - 1 );
	}

	// Shadow layer is returned, and must be put back in free stack by caller
	return pShadowOutLayer;
}


//-----------------------------------------------------------------------------
// Purpose: Called to draw and free a composition layer created with CreateInsetShadowLayer
//-----------------------------------------------------------------------------
void panorama::CSource2Surface::DrawInsetShadowLayer( float flWidthSourceLayer, float flHeightSourceLayer, float flXPosInParent, float flYPosInParent, float flScale2DX, float flScale2DY, float flOpacity, float *pflOuterRadii, float *pflBorderWidths, VMatrix *pMatTransform,
	float flWidthParentLayer, float flHeightParentLayer, CSource2CompositionLayer *pShadowLayer, float flHorOffset, float flVerOffset, float flBlurRadius, float flSpreadDistance, uint32 shadowColor )
{
	VPROF_BUDGET_THREAD( "CSource2Surface::DrawInsetShadowLayer", VPROF_BUDGETGROUP_TENFOOT );

	float flPadding = ceil( flBlurRadius );
	float flUAdjustment = flPadding / pShadowLayer->GetWidth();
	float flVAdjustment = flPadding / pShadowLayer->GetHeight();
	
	// set up fancyquad to draw with corner rounding
	FancyQuadParameters_t FancyParam( FancyQuadFlag_OuterCorner );
	FancyParam.m_flZ = 0.0f;
	FancyParam.m_flVertexMin[0] = (flXPosInParent + pflBorderWidths[3]);
	FancyParam.m_flVertexMin[1] = (flYPosInParent + pflBorderWidths[0]);
	FancyParam.m_flVertexMax[0] = (flXPosInParent + flWidthSourceLayer - pflBorderWidths[1]);
	FancyParam.m_flVertexMax[1] = (flYPosInParent + flHeightSourceLayer - pflBorderWidths[2]);
	FancyParam.m_flTexCoordMin[0] = flUAdjustment;
	FancyParam.m_flTexCoordMin[1] = flVAdjustment;
	FancyParam.m_flTexCoordMax[0] = 1.0f - flUAdjustment;
	FancyParam.m_flTexCoordMax[1] = 1.0f - flVAdjustment;
	FancyParam.m_flCornerRadii[0][0] = pflOuterRadii[0] - pflBorderWidths[3];		// outer
	FancyParam.m_flCornerRadii[0][1] = pflOuterRadii[1] - pflBorderWidths[0];
	FancyParam.m_flCornerRadii[1][0] = pflOuterRadii[2] - pflBorderWidths[1];
	FancyParam.m_flCornerRadii[1][1] = pflOuterRadii[3] - pflBorderWidths[0];
	FancyParam.m_flCornerRadii[2][0] = pflOuterRadii[4] - pflBorderWidths[1];
	FancyParam.m_flCornerRadii[2][1] = pflOuterRadii[5] - pflBorderWidths[2];
	FancyParam.m_flCornerRadii[3][0] = pflOuterRadii[6] - pflBorderWidths[3];
	FancyParam.m_flCornerRadii[3][1] = pflOuterRadii[7] - pflBorderWidths[2];

	FancyQuadBrush_t FancyBrush( flOpacity, flOpacity, flOpacity, flOpacity );

	FancyQuadDraw_t fancyQuadDraw;
	fancyQuadDraw.m_hTexture0 = pShadowLayer->GetTextureHandle();
	fancyQuadDraw.m_flTexture0TexCoordScale[0] = pShadowLayer->GetRTOriginalWidthScale();
	fancyQuadDraw.m_flTexture0TexCoordScale[1] = pShadowLayer->GetRTOriginalHeightScale();
	fancyQuadDraw.m_nWide = flWidthParentLayer;
	fancyQuadDraw.m_nTall = flHeightParentLayer;
	fancyQuadDraw.m_pQuadParameters = &FancyParam;
	fancyQuadDraw.m_pQuadBrush = &FancyBrush;
	fancyQuadDraw.m_pVMatrix = pMatTransform;
	fancyQuadDraw.m_flScale2DX = flScale2DX;
	fancyQuadDraw.m_flScale2DY = flScale2DY;
	fancyQuadDraw.m_flTextureWidth = -1.0f;
	fancyQuadDraw.m_flTextureHeight = -1.0f;
	fancyQuadDraw.m_bClipToLayer = true;

	// Draw the current layer into its parent with corner rounding if needed
	DrawFancyQuad( &fancyQuadDraw);
}

//-----------------------------------------------------------------------------
// Purpose: Called to clear contents of current compositing layer
//-----------------------------------------------------------------------------
void CSource2Surface::ClearCompositingLayer( CSource2CompositionLayer *pLayer )
{
	pLayer->Clear();

	if ( m_stackCompositionLayers.Count() == 1 )
		pLayer->PushCliplayersAndBeginDraw( m_flScaleBackbufferX, m_flScaleBackbufferY, m_flTranslateBackbufferX, m_flTranslateBackbufferY );
	else
		pLayer->PushCliplayersAndBeginDraw( 1.0f, 1.0f, 0.0f, 0.0f );

	Assert( pLayer->GetClipLayerCount() == 0 );
}


//--------------------------------------------------------------------------------------------------
// Util fns for PopCompositingLayer
//--------------------------------------------------------------------------------------------------

// Returns true if this is a blurtarget panel, false otherwise
bool CSource2Surface::SetupBlurPanelAttr( CRenderAttributes &renderAttributes, CSource2CompositionLayer *pLayer )
{
	int idx = FindBlurPanel( pLayer->GetContextID() );
	if ( idx != -1 )
	{
		//				CPanelPtr< CUIPanel > ptr;
		//				ptr.SetFromUInt64( pLayer->GetContextID() );

		int nRect = m_BlurPanels[ idx ].source_ids.Count();

		if ( nRect )
		{
			// Add target position and transform to renderAttr

			Vector4D targetRect( m_BlurPanels[ idx ].m_vTopLeft.x, m_BlurPanels[ idx ].m_vTopLeft.y,
								 m_BlurPanels[ idx ].m_vBottomRight.x, m_BlurPanels[ idx ].m_vBottomRight.y );

			// First blur rect added is the target
			renderAttributes.AddBlurRect( targetRect, m_BlurPanels[idx].m_Matrix );

			// Add source rectangles

			for ( int i = 0; i < nRect; i++ )
			{
				uint64 nSrcPanelID = m_BlurPanels[ idx ].source_ids[ i ];
				int srcIdx = m_TrackedSourcePanelIDs.Find( nSrcPanelID );

				Assert( srcIdx != -1 );

				CTrackedPanelData &panelData = m_TrackedSourcePanelData[ srcIdx ];

				Vector4D rect( panelData.m_vTopLeft.x, panelData.m_vTopLeft.y,
							   panelData.m_vBottomRight.x, panelData.m_vBottomRight.y );

				// temp sanity check, remove once we figure out why we're getting zero size rects here
				// TODO - sanity check
				if ( ( ( panelData.m_vBottomRight.x - panelData.m_vTopLeft.x ) < 1.0f ) ||
					 ( ( panelData.m_vBottomRight.y - panelData.m_vTopLeft.y ) < 1.0f ) )
					continue;

				renderAttributes.AddBlurRect( rect, panelData.m_Matrix );
			}
		}

		return true;

	}

	return false;

}

void CSource2Surface::ClearBlurRT( BlurRT_t &blurRT, float flW, float flH, Vector4D color )
{
	m_pRenderContext->BindRenderTargets( *blurRT.m_pRTDesc );
	RenderViewport_t renderViewport;
	renderViewport.Init( 0, 0, Min( flW, blurRT.m_flRTW ), Min( flH, blurRT.m_flRTH ) );
	m_pRenderContext->SetViewports( 1, &renderViewport );
	m_pRenderContext->Clear( &color, 1, RENDER_CLEAR_FLAGS_CLEAR_COLOR );
}


void CSource2Surface::DrawGaussianRTtoRT( CRenderAttributes &renderAttributes, BlurRT_t &dst, BlurRT_t &src, char cHorV, float flBlurStdDev )
{
	m_pRenderContext->BindRenderTargets( *dst.m_pRTDesc );

	// Set view port to the layer RT size
	RenderViewport_t renderViewport;
	renderViewport.Init( 0, 0, dst.m_flWindowW, dst.m_flWindowH );
	m_pRenderContext->SetViewports( 1, &renderViewport );

	ClearBlurRT( dst, dst.m_flWindowW, dst.m_flWindowH, Vector4D( 0, 0, 0, 0 ) );

	// shaders need the viewport set
	renderAttributes.SetFloatValue( ATTR_ViewportWidth, dst.m_flWindowW );
	renderAttributes.SetFloatValue( ATTR_ViewportHeight, dst.m_flWindowH );

	float halftexelU = ( 0.5f / src.m_flRTW );
	float halftexelV = ( 0.5f / src.m_flRTH );

	renderAttributes.SetVector2DValue( ATTR_UVClamp,
									   Vector2D( src.m_flWindowW / src.m_flRTW,
												 src.m_flWindowH / src.m_flRTH )
									   - ( 1.0 * Vector2D( halftexelU, halftexelV ) ) );
	
	renderAttributes.SetFloatValue( ATTR_BlurSigma, flBlurStdDev );

	if ( cHorV == 'V' )
	{
		renderAttributes.SetVector2DValue( ATTR_BlurMultiplyVec, Vector2D( 0.0f, 1.0f / src.m_flRTH ) );
	}
	else
	{
		renderAttributes.SetVector2DValue( ATTR_BlurMultiplyVec, Vector2D( 1.0f / src.m_flRTW, 0.0f ) );
	}

#if PANDX_DRAW
	BasicQuad_t *pQuad = PanDxGetBasicQuadPtr();
#else
	BasicQuad_t pQuad[ 1 ];
#endif

	pQuad->BqInit( 0, 0, dst.m_flWindowW, dst.m_flWindowH );
	
	pQuad->m_vUV[ 1 ].x = src.m_flWindowW / src.m_flRTW;
	pQuad->m_vUV[ 2 ].x = src.m_flWindowW / src.m_flRTW;
	pQuad->m_vUV[ 2 ].y = src.m_flWindowH / src.m_flRTH;
	pQuad->m_vUV[ 3 ].y = src.m_flWindowH / src.m_flRTH;

	DrawTexturedQuadInternal( m_hMaterial, &renderAttributes, *src.m_pRenderTex, pQuad );

}

void CSource2Surface::CopyBlurRectsToRT(CRenderAttributes &renderAttributes, BlurRT_t &dst, BlurRT_t &src, bool bUpsizeInCopy)
{
	int nRect = renderAttributes.GetNumSourceBlurRects();
	if ( nRect )
	{
		Vector4D offset;
		renderAttributes.GetTargetBlurRect( &offset, nullptr );

		for ( int i = 0; i < nRect; i++ )
		{
			Vector4D rect;
			VMatrix mat;
			Vector v[ 4 ];

			renderAttributes.GetSourceBlurRect( &rect, &mat, i );

			Vector topleft( rect.x, rect.y, 0.0f );
			Vector bottomright( rect.z, rect.w, 0.0f );

			v[ 0 ] = Vector( topleft.x, topleft.y, 0 );
			v[ 1 ] = Vector( topleft.x, bottomright.y, 0 );
			v[ 2 ] = Vector( bottomright.x, bottomright.y, 0 );
			v[ 3 ] = Vector( bottomright.x, topleft.y, 0 );

#if PANDX_DRAW
			BasicQuad_t *pQuad = PanDxGetBasicQuadPtr();
#else
			BasicQuad_t pQuad[ 1 ];
#endif

			pQuad->BqInit( 0, 0, dst.m_flWindowW, dst.m_flWindowH );

			for ( int j = 0; j < SIZE_OF_ARRAY( v ); j++ )
			{
				v[ j ] = mat * v[ j ];
				v[ j ].x -= offset.x;
				v[ j ].y -= offset.y;

				pQuad->m_vPosition[ j ].x = v[ j ].x;
				pQuad->m_vPosition[ j ].y = v[ j ].y;

				if ( bUpsizeInCopy)
				{
					pQuad->m_vUV[ j ].x = (src.m_flWindowW * pQuad->m_vPosition[ j ].x / dst.m_flWindowW) / src.m_flRTW;
					pQuad->m_vUV[ j ].y = (src.m_flWindowH * pQuad->m_vPosition[ j ].y / dst.m_flWindowH) / src.m_flRTH;
				}
				else
				{
					pQuad->m_vUV[ j ].x = pQuad->m_vPosition[ j ].x / src.m_flRTW;
					pQuad->m_vUV[ j ].y = pQuad->m_vPosition[ j ].y / src.m_flRTH;
				}
			
			}

			// Draw from src to dst

			m_pRenderContext->BindRenderTargets( *dst.m_pRTDesc );

			// Set view port to the layer RT size
			RenderViewport_t renderViewport;
			renderViewport.Init( 0, 0, dst.m_flWindowW, dst.m_flWindowH );
			m_pRenderContext->SetViewports( 1, &renderViewport );
			// shaders need the viewport set
			renderAttributes.SetFloatValue( ATTR_ViewportWidth, dst.m_flWindowW );
			renderAttributes.SetFloatValue( ATTR_ViewportHeight, dst.m_flWindowH );

			// Using clamp
			float halftexelU = ( 0.5f / src.m_flRTW );
			float halftexelV = ( 0.5f / src.m_flRTH );

			renderAttributes.SetVector2DValue( ATTR_UVClamp,
												Vector2D( src.m_flWindowW / src.m_flRTW,
															src.m_flWindowH / src.m_flRTH )
												- ( 1.0 * Vector2D( halftexelU, halftexelV ) ) );

			renderAttributes.SetIntValue( ATTR_D_TEX2DBLUR, 0 );
			renderAttributes.SetIntValue( ATTR_D_TEX2DDOWNSAMPLE, 1 ); // Simple bilinear shader w/clamp


			DrawTexturedQuadInternal( m_hMaterial, &renderAttributes, *src.m_pRenderTex, pQuad );
		}
	}
};


void CSource2Surface::ApplyGaussianBlur( CRenderAttributes &renderAttributes,  CSource2CompositionLayer *pLayer, 
										 float flBlurPasses, float flBlurStdDevHor, float flBlurStdDevVer )
{
	HRenderTexture scratchRenderTex0 = g_pSceneSystem->GetWellKnownRenderTarget( SCENE_RTGT_SCRATCH_TEXTURE_8888, SCENE_RTSIZE_FRAMEBUFFER, 0 );
	RenderTargetDesc_t scratchRTDesc0( scratchRenderTex0, RENDER_TEXTURE_HANDLE_INVALID, RENDER_SRGB );
	CTextureDesc const *scratchTexDesc0 = g_pRenderDevice->GetTextureDesc( scratchRenderTex0 );

	HRenderTexture layerRenderTex;
	RenderTargetDesc_t layerRTDesc;
	pLayer->GetRenderTargetHandleAndDesc( layerRenderTex, layerRTDesc );

	// Setup Blur_t's for the RTs. and for the layer
	BlurRT_t blurRT0( &scratchRenderTex0, &scratchRTDesc0,
						pLayer->GetWidth(), pLayer->GetHeight(),
						(float)scratchTexDesc0->m_nWidth, (float)scratchTexDesc0->m_nHeight );

	BlurRT_t blurRTLayer( &layerRenderTex, &layerRTDesc,
							pLayer->GetWidth(), pLayer->GetHeight(),
							pLayer->GetWidth() / pLayer->GetRTOriginalWidthScale(),
							pLayer->GetHeight() / pLayer->GetRTOriginalHeightScale() );

	// shaders need the viewport set
	renderAttributes.SetFloatValue( ATTR_ViewportWidth, (float)pLayer->GetWidth() );
	renderAttributes.SetFloatValue( ATTR_ViewportHeight, (float)pLayer->GetHeight() );
	
	// Enable gauusian and mat to I
	renderAttributes.SetIntValue( ATTR_D_TEX2DBLUR, 1 );
	renderAttributes.SetIntValue( ATTR_D_TEX2DDOWNSAMPLE, 0 );
	renderAttributes.SetVMatrixValue( ATTR_MatTransform, VMatrix::GetIdentityMatrix() );

	if ( !renderAttributes.GetNumSourceBlurRects() )
	{
		for ( float i = 0.0f; i < flBlurPasses; i += 1.0f )
		{
			// Draw the current layer into horizontal blur surface ( scratch well knwown target)
			DrawGaussianRTtoRT( renderAttributes, blurRT0, blurRTLayer, 'H', flBlurStdDevHor );
			DrawGaussianRTtoRT( renderAttributes, blurRTLayer, blurRT0, 'V', flBlurStdDevVer );
		}
	}
	else
	{
		HRenderTexture scratchRenderTex1 = g_pSceneSystem->GetWellKnownRenderTarget( SCENE_RTGT_SCRATCH_TEXTURE_8888, SCENE_RTSIZE_FRAMEBUFFER, 1 );
		RenderTargetDesc_t scratchRTDesc1( scratchRenderTex1, RENDER_TEXTURE_HANDLE_INVALID, RENDER_SRGB );
		CTextureDesc const *scratchTexDesc1 = g_pRenderDevice->GetTextureDesc( scratchRenderTex1 );

		BlurRT_t blurRT1( &scratchRenderTex1, &scratchRTDesc1,
						  pLayer->GetWidth(), pLayer->GetHeight(),
						  (float)scratchTexDesc1->m_nWidth, (float)scratchTexDesc1->m_nHeight );

		DrawGaussianRTtoRT( renderAttributes, blurRT0, blurRTLayer, 'H', flBlurStdDevHor );
		DrawGaussianRTtoRT( renderAttributes, blurRT1, blurRT0, 'V', flBlurStdDevVer );


		for ( float i = 1.0f; i < flBlurPasses; i += 1.0f )
		{
			// Draw the current layer into horizontal blur surface ( scratch well knwown target)
			DrawGaussianRTtoRT( renderAttributes, blurRT0, blurRT1, 'H', flBlurStdDevHor );
			DrawGaussianRTtoRT( renderAttributes, blurRT1, blurRT0, 'V', flBlurStdDevVer );
		}

		CopyBlurRectsToRT( renderAttributes, blurRTLayer, blurRT1, false );
	}

}

void CSource2Surface::DrawDownSizeRTtoRT( CRenderAttributes &renderAttributes, BlurRT_t &dst, BlurRT_t &src )
{
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	PIXEvent pix( pRenderContext, "DrawDown" );

	m_pRenderContext->BindRenderTargets( *dst.m_pRTDesc );

	// Set view port to the layer RT size
	RenderViewport_t renderViewport;
	renderViewport.Init( 0, 0, dst.m_flWindowW, dst.m_flWindowH );
	m_pRenderContext->SetViewports( 1, &renderViewport );

	ClearBlurRT( dst, dst.m_flWindowW, dst.m_flWindowH, Vector4D( 0, 0, 0, 1.0 ) );

	// shaders need the viewport set
	renderAttributes.SetFloatValue( ATTR_ViewportWidth, dst.m_flWindowW );
	renderAttributes.SetFloatValue( ATTR_ViewportHeight, dst.m_flWindowH );

	float halftexelU = ( 0.5f / src.m_flRTW );
	float halftexelV = ( 0.5f / src.m_flRTH );

	renderAttributes.SetVector2DValue( ATTR_UVClamp, Vector2D( 1.0, 1.0 ) );

#if PANDX_DRAW
	BasicQuad_t *pQuad = PanDxGetBasicQuadPtr();
#else
	BasicQuad_t pQuad[ 1 ];
#endif

	pQuad->BqInit( 0, 0, dst.m_flWindowW, dst.m_flWindowH );

	pQuad->m_vUV[ 1 ].x = ( src.m_flWindowW / src.m_flRTW );
	pQuad->m_vUV[ 2 ].x = ( src.m_flWindowW / src.m_flRTW );
	pQuad->m_vUV[ 2 ].y = ( src.m_flWindowH / src.m_flRTH );
	pQuad->m_vUV[ 3 ].y = ( src.m_flWindowH / src.m_flRTH );
	
	//pQuad->m_vUV[ 0 ] += Vector2D( halftexelU, halftexelV );
	//pQuad->m_vUV[ 1 ] += Vector2D( -1.0 * halftexelU, halftexelV );
	//pQuad->m_vUV[ 2 ] += Vector2D( -1.0 * halftexelU, -1.0 * halftexelV );
	//pQuad->m_vUV[ 3 ] += Vector2D( halftexelU, -1.0 * halftexelV );


//	for ( auto &uv : quad.m_vUV ) uv += Vector2D( halftexelU, halftexelV );

	DrawTexturedQuadInternal( m_hMaterial, &renderAttributes, *src.m_pRenderTex, pQuad );

}

void CSource2Surface::DrawUpSizeRTtoRT( CRenderAttributes &renderAttributes, BlurRT_t &dst, BlurRT_t &src )
{
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	PIXEvent pix( pRenderContext, "DrawUp" );

	m_pRenderContext->BindRenderTargets( *dst.m_pRTDesc );

	// Set view port to the layer RT size
	RenderViewport_t renderViewport;
	renderViewport.Init( 0, 0, dst.m_flWindowW, dst.m_flWindowH );
	m_pRenderContext->SetViewports( 1, &renderViewport );

	ClearBlurRT( dst, dst.m_flWindowW, dst.m_flWindowH, Vector4D( 0, 0, 0, 1.0 ) );

	// shaders need the viewport set
	renderAttributes.SetFloatValue( ATTR_ViewportWidth, dst.m_flWindowW );
	renderAttributes.SetFloatValue( ATTR_ViewportHeight, dst.m_flWindowH );

	float halftexelU = (0.5f / src.m_flRTW);
	float halftexelV = (0.5f / src.m_flRTH);

	renderAttributes.SetVector2DValue( ATTR_UVClamp,
									   Vector2D( src.m_flWindowW / src.m_flRTW,
												 src.m_flWindowH / src.m_flRTH ) 
									   - (1.0 * Vector2D( halftexelU, halftexelV ) ) );

#if PANDX_DRAW
	BasicQuad_t *pQuad = PanDxGetBasicQuadPtr();
#else
	BasicQuad_t pQuad[ 1 ];
#endif

	pQuad->BqInit( 0, 0, dst.m_flWindowW, dst.m_flWindowH );

	pQuad->m_vUV[ 1 ].x = src.m_flWindowW / src.m_flRTW;
	pQuad->m_vUV[ 2 ].x = src.m_flWindowW / src.m_flRTW;
	pQuad->m_vUV[ 2 ].y = src.m_flWindowH / src.m_flRTH;
	pQuad->m_vUV[ 3 ].y = src.m_flWindowH / src.m_flRTH;

	DrawTexturedQuadInternal( m_hMaterial, &renderAttributes, *src.m_pRenderTex, pQuad );
}

void CSource2Surface::DownSize( BlurRT_t &rt, float flFactor )
{
	rt.m_flWindowW = rt.m_flWindowW * flFactor; 
	rt.m_flWindowH = rt.m_flWindowH * flFactor;

	rt.m_flWindowW = (int)rt.m_flWindowW;
	rt.m_flWindowH = (int)rt.m_flWindowH;
}

void CSource2Surface::ApplyFastGaussianBlur( CRenderAttributes &renderAttributes, CSource2CompositionLayer *pLayer,
												   float flBlurPasses, float flBlurStdDevHor, float flBlurStdDevVer, BlurType_t blurType )
{
	if ( flBlurPasses < 1.0 ) return;

	uint64 nlayerID = pLayer->GetContextID();

	// Eco vars
	bool bEco = s_panorama_blur_ecomode.GetBool() && m_bMainMenu && (renderAttributes.GetNumSourceBlurRects() > 0) && (blurType != BT_FASTANIM);
	float flNow = m_flCurrentRenderFrameTime;
	float flEcoInterval = 1.0f / MAX( s_panorama_blur_ecomode_fps.GetFloat(), 1.0f );

	int nBlurPasses = (int)flBlurPasses;
	float flFactor;

	if ( blurType == BT_FASTANIM )
	{
		flFactor = flBlurPasses - (float)nBlurPasses;
	}
	else
	{
		flFactor = 0.5f;
	}

	if ( flFactor < 0.5f ) return;

	// Get layer and scratch RTs

	HRenderTexture scratchRenderTex0 = g_pSceneSystem->GetWellKnownRenderTarget( SCENE_RTGT_SCRATCH_TEXTURE_8888, SCENE_RTSIZE_FRAMEBUFFER, 0 );
	RenderTargetDesc_t scratchRTDesc0( scratchRenderTex0, RENDER_TEXTURE_HANDLE_INVALID, RENDER_SRGB );
	CTextureDesc const *scratchTexDesc0 = g_pRenderDevice->GetTextureDesc( scratchRenderTex0 );

	HRenderTexture scratchRenderTex1 = g_pSceneSystem->GetWellKnownRenderTarget( SCENE_RTGT_SCRATCH_TEXTURE_8888, SCENE_RTSIZE_FRAMEBUFFER, 1 );
	RenderTargetDesc_t scratchRTDesc1( scratchRenderTex1, RENDER_TEXTURE_HANDLE_INVALID, RENDER_SRGB );
	CTextureDesc const *scratchTexDesc1 = g_pRenderDevice->GetTextureDesc( scratchRenderTex1 );

	HRenderTexture layerRenderTex;
	RenderTargetDesc_t layerRTDesc;
	pLayer->GetRenderTargetHandleAndDesc( layerRenderTex, layerRTDesc );

	// Setup Blur_t's for the RTs. and for the layer
	BlurRT_t blurRT0( &scratchRenderTex0, &scratchRTDesc0,
					  pLayer->GetWidth(), pLayer->GetHeight(),
					  (float)scratchTexDesc0->m_nWidth, (float)scratchTexDesc0->m_nHeight );

	BlurRT_t blurRT1( &scratchRenderTex1, &scratchRTDesc1,
					  pLayer->GetWidth(), pLayer->GetHeight(),
					  (float)scratchTexDesc1->m_nWidth, (float)scratchTexDesc1->m_nHeight );

	const int NUMRT = 2;
	BlurRT_t	*apBlurRT[ NUMRT ] = { &blurRT0, &blurRT1 };
	unsigned int nRTIdx = 0;

#define PREV_RT() (*apBlurRT[ ( nRTIdx + NUMRT - 1 ) % NUMRT ] )
#define CURR_RT() (*apBlurRT[ nRTIdx ] )
#define NEXT_RT() ( nRTIdx = ( nRTIdx + 1 ) % NUMRT )

	BlurRT_t blurRTLayer( &layerRenderTex, &layerRTDesc,
						  pLayer->GetWidth(), pLayer->GetHeight(),
						  pLayer->GetWidth() / pLayer->GetRTOriginalWidthScale(),
						  pLayer->GetHeight() / pLayer->GetRTOriginalHeightScale() );

	//  Eco instead ?

	if ( bEco )
	{
		int cacheIdx = LookupBlurLayerCacheIdx( nlayerID, flNow - flEcoInterval, flNow );		// Will be freed if too old

		if ( cacheIdx != -1 )
		{
			CSource2CompositionLayer *pCacheLayer = m_BlurLayerCache[ cacheIdx ].m_pCachedBlurLayer;

			HRenderTexture cachelayerRenderTex;
			RenderTargetDesc_t cachelayerRTDesc;
			pCacheLayer->GetRenderTargetHandleAndDesc( cachelayerRenderTex, cachelayerRTDesc );

			BlurRT_t cacheRTLayer( &cachelayerRenderTex, &cachelayerRTDesc,
								   pCacheLayer->GetWidth(), pCacheLayer->GetHeight(),
								   pCacheLayer->GetWidth() / pCacheLayer->GetRTOriginalWidthScale(),
								   pCacheLayer->GetHeight() / pCacheLayer->GetRTOriginalHeightScale() );

			cacheRTLayer.m_flWindowW = m_BlurLayerCache[ cacheIdx ].m_flWindowW;
			cacheRTLayer.m_flWindowH = m_BlurLayerCache[ cacheIdx ].m_flWindowH;

			renderAttributes.SetIntValue( ATTR_D_TEX2DBLUR, 0 );
			renderAttributes.SetIntValue( ATTR_D_TEX2DDOWNSAMPLE, 1 ); // Up&Downsample both use a simple bilinear shader w/clamp

			CopyBlurRectsToRT( renderAttributes, blurRTLayer, cacheRTLayer, true );
			
			return;
		}
	}

	// Enable gauusian and mat to I
	renderAttributes.SetIntValue( ATTR_D_TEX2DBLUR, 1 );
	renderAttributes.SetIntValue( ATTR_D_TEX2DDOWNSAMPLE, 0 );
	renderAttributes.SetVMatrixValue( ATTR_MatTransform, VMatrix::GetIdentityMatrix() );
	
	// Draw the current layer into horizontal blur surface
	// Draw either back to the layer (1 pass ) or to RT1 (>1 pass) for vert pass
	if ( nBlurPasses < 2 )
	{
		DrawGaussianRTtoRT( renderAttributes, CURR_RT(), blurRTLayer, 'H', flBlurStdDevHor ); NEXT_RT();
		if ( !renderAttributes.GetNumSourceBlurRects() )
		{
			DrawGaussianRTtoRT( renderAttributes, blurRTLayer, PREV_RT(), 'V', flBlurStdDevVer );
		}
		else
		{
		 	DrawGaussianRTtoRT( renderAttributes, CURR_RT(), PREV_RT(), 'V', flBlurStdDevVer ); NEXT_RT();
			CopyBlurRectsToRT( renderAttributes, blurRTLayer, PREV_RT(), false );
		}

		return;
	}

	// DownSize
 	renderAttributes.SetIntValue( ATTR_D_TEX2DBLUR, 0 );
 	renderAttributes.SetIntValue( ATTR_D_TEX2DDOWNSAMPLE, 1 );
 	DownSize( CURR_RT(), flFactor );
 	DrawDownSizeRTtoRT( renderAttributes, CURR_RT(), blurRTLayer ); NEXT_RT();
 	DownSize( CURR_RT(), flFactor );
 	// Gaussian
 	renderAttributes.SetIntValue( ATTR_D_TEX2DBLUR, 1 );
 	renderAttributes.SetIntValue( ATTR_D_TEX2DDOWNSAMPLE, 0 );
 	DrawGaussianRTtoRT( renderAttributes, CURR_RT(), PREV_RT(), 'H', flBlurStdDevHor ); NEXT_RT();
 	DrawGaussianRTtoRT( renderAttributes, CURR_RT(), PREV_RT(), 'V', flBlurStdDevVer ); NEXT_RT();

	for ( int i = 2; ( i < nBlurPasses ) && ( blurRT0.m_flWindowH >= 16.0) && ( ( blurRT0.m_flWindowW >= 16.0 ) ); i++ )
	{
		// DownSize
		renderAttributes.SetIntValue( ATTR_D_TEX2DBLUR, 0 );
		renderAttributes.SetIntValue( ATTR_D_TEX2DDOWNSAMPLE, 1 );
		DownSize( CURR_RT(), flFactor );
		DrawDownSizeRTtoRT( renderAttributes, CURR_RT(), PREV_RT() ); NEXT_RT();
		DownSize( CURR_RT(), flFactor );
		// Gaussian
		renderAttributes.SetIntValue( ATTR_D_TEX2DBLUR, 1 );
		renderAttributes.SetIntValue( ATTR_D_TEX2DDOWNSAMPLE, 0 );
		DrawGaussianRTtoRT( renderAttributes, CURR_RT(), PREV_RT(), 'H', flBlurStdDevHor ); NEXT_RT();
		DrawGaussianRTtoRT( renderAttributes, CURR_RT(), PREV_RT(), 'V', flBlurStdDevVer ); NEXT_RT();
	}


	blurRTLayer.m_flWindowW = pLayer->GetWidth();
	blurRTLayer.m_flWindowH = pLayer->GetHeight();
	renderAttributes.SetIntValue( ATTR_D_TEX2DBLUR, 0 );
	renderAttributes.SetIntValue( ATTR_D_TEX2DDOWNSAMPLE, 1 ); // Up&Downsample both use a simple bilinear shader w/clamp

	if ( bEco )
	{

		int cacheIdx = AllocBlurLayerCacheIdx( nlayerID, flNow, (int)(PREV_RT().m_flWindowW + 0.5f), (int)(PREV_RT().m_flWindowH + 0.5f) );

		if ( cacheIdx != -1 )
		{

			CSource2CompositionLayer *pCacheLayer = m_BlurLayerCache[ cacheIdx ].m_pCachedBlurLayer;

			HRenderTexture cachelayerRenderTex;
			RenderTargetDesc_t cachelayerRTDesc;
			pCacheLayer->GetRenderTargetHandleAndDesc( cachelayerRenderTex, cachelayerRTDesc );

			BlurRT_t cacheRTLayer( &cachelayerRenderTex, &cachelayerRTDesc,
								   pCacheLayer->GetWidth(), pCacheLayer->GetHeight(),
								   pCacheLayer->GetWidth() / pCacheLayer->GetRTOriginalWidthScale(),
								   pCacheLayer->GetHeight() / pCacheLayer->GetRTOriginalHeightScale() );

			cacheRTLayer.m_flWindowW = PREV_RT().m_flWindowW;
			cacheRTLayer.m_flWindowH = PREV_RT().m_flWindowH;

			m_BlurLayerCache[ cacheIdx ].m_flWindowW = cacheRTLayer.m_flWindowW;
			m_BlurLayerCache[ cacheIdx ].m_flWindowH = cacheRTLayer.m_flWindowH;

			// Not actually an upsize, just a copy.
			DrawUpSizeRTtoRT( renderAttributes, cacheRTLayer, PREV_RT() );

			CopyBlurRectsToRT( renderAttributes, blurRTLayer, cacheRTLayer, true );

			return;
		}

	}

	if ( !renderAttributes.GetNumSourceBlurRects() )
	{
		DrawUpSizeRTtoRT( renderAttributes, blurRTLayer, PREV_RT() );
	}
	else
	{
		CopyBlurRectsToRT( renderAttributes, blurRTLayer, PREV_RT(), true );
	}

}

//--------------------------------------------------------------------------------------------------
// Blur Eco mode / cache
//--------------------------------------------------------------------------------------------------

void CSource2Surface::InitBlurLayerCache()
{
	if ( !m_bMainMenu ) return;

	for ( BlurLayerCache_t &blc : m_BlurLayerCache )
	{
		blc.m_flWriteTime = 0.0f;
		blc.m_flReadTime = 0.0f;
		blc.m_nLayerID = NULL;
		blc.m_pCachedBlurLayer = NULL;
	}

}

int CSource2Surface::LookupBlurLayerCacheIdx( uint64 nLayerID, float flOldestWriteTime, float flTime )
{
	if ( !m_bMainMenu ) return -1;

	for ( int i = 0; i < ARRAYSIZE( m_BlurLayerCache ); i++ )
	{
		BlurLayerCache_t &blc = m_BlurLayerCache[ i ];
		if ( blc.m_nLayerID == nLayerID )
		{
			if (blc.m_flWriteTime >= flOldestWriteTime)
			{
				blc.m_flReadTime = flTime;
				return i;
			}
			else
			{
				blc.m_nLayerID = NULL;
				if ( blc.m_pCachedBlurLayer)
				{
					delete blc.m_pCachedBlurLayer;
					blc.m_pCachedBlurLayer = NULL;
				}
				return -1;
			}
		}
		
	}

	return -1;
}


void CSource2Surface::PurgeBlurLayerCache( float flOldestWriteTime )
{
	if ( !m_bMainMenu ) return;

	for ( int i = 0; i < ARRAYSIZE( m_BlurLayerCache ); i++ )
	{
		BlurLayerCache_t &blc = m_BlurLayerCache[ i ];
		if ( blc.m_nLayerID )
		{
			if ( blc.m_flWriteTime < flOldestWriteTime )
			{
				blc.m_nLayerID = NULL;
				if ( blc.m_pCachedBlurLayer)
				{
					delete blc.m_pCachedBlurLayer;
					blc.m_pCachedBlurLayer = NULL;
				}
			}
		}
	}
}


int CSource2Surface::AllocBlurLayerCacheIdx( uint64 nLayerID, float flCurrTime, int nW, int nH )
{
	if ( !m_bMainMenu ) return -1;

	// Find an empty slot and create a Layer if required.

	for ( int i = 0; i < ARRAYSIZE( m_BlurLayerCache ); i++ )
	{
		BlurLayerCache_t &blc = m_BlurLayerCache[ i ];
		if ( blc.m_nLayerID == NULL )
		{
			if ( blc.m_pCachedBlurLayer == NULL )
			{
				blc.m_pCachedBlurLayer = GetCompositionLayer( nW, nH,
															  false, false, "textshadow_blur" );
			}

			blc.m_nLayerID = nLayerID;
			blc.m_flWriteTime = flCurrTime;

			return i;
		}

	}

	return -1;

}

void CSource2Surface::TermBlurLayerCache()
{
	if ( !m_bMainMenu ) return;

	for ( BlurLayerCache_t &blc : m_BlurLayerCache )
	{
		blc.m_flWriteTime = 0.0f;
		blc.m_flReadTime = 0.0f;
		blc.m_nLayerID = NULL;

		if ( blc.m_pCachedBlurLayer  != NULL )
		{
			delete blc.m_pCachedBlurLayer;
			blc.m_pCachedBlurLayer = NULL;
		}
	}
}

void CSource2Surface::StatsBlurLayerCache( uint64 &nCount, uint64 &nSizeInBytes )
{
	nCount = 0;
	nSizeInBytes = 0;
	if ( m_bMainMenu )
	{
		for ( int i = 0; i < ARRAYSIZE( m_BlurLayerCache ); i++ )
		{
			BlurLayerCache_t &blc = m_BlurLayerCache[i];
			if ( blc.m_nLayerID )
			{
				nCount++;
				nSizeInBytes += blc.m_pCachedBlurLayer ? blc.m_pCachedBlurLayer->RenderTargetSizeInBytes() : 0;
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called to pop a compositing layer, which leads to rendering it to the parent layer 
// or backbuffer with transforms/opacity/etc applied, and then restores drawing context to the parent
//-----------------------------------------------------------------------------
void CSource2Surface::PopCompositingLayer( const PopCompositingLayerRenderCommand_t &renderCommand )
{
	VPROF( "CSource2Surface::PopCompositingLayer" );
	//VPROF_BUDGET( "Panorama PopCompositingLayer", VPROF_BUDGETGROUP_GAME );


	if ( m_stackCompositionLayers.Count() < 2 )
	{
		AssertMsg( false, "CSource2Surface::PopCompositingLayer hit with no layers, mismatched push/pop?" );
		return;
	}

	// End draw for layer
	CSource2CompositionLayer *pLayer = m_stackCompositionLayers[m_stackCompositionLayers.Count() - 1];

#if 0
	try
	{
		CPanelPtr< CUIPanel > ptr;
		ptr.SetFromUInt64( pLayer->GetContextID() );

		Msg( " Popping composition layer for %s (%s) (%p)\n", ptr->GetID(), ptr->GetPanelType().String(), ptr->ClientPtr() );

	}
	catch( ... ) {}
#endif

	bool bLayerRedraw = pLayer->BIsDrawing();
	
	// If asked to highlight composition layers, override their border with a colored
	// border based on the depth of the composition layer stack.
	if ( s_convarPanoramaHighlightCompositionLayers.GetBool() )
	{
		pLayer->ActivateRenderTarget();

		const float k_flLayerBorderWidth = 2.0f;
		const uint32 k_unLayerBorderColors[] =
		{
			0xff00ffff, // yellow
			0xff3096ff, // orange
			0xff0000ff, // red
			0xff000080, // dark red
		};

		uint32 unBorderColor;
		if ( pLayer->BReusedFromCache() )
		{
			if ( bLayerRedraw )
			{
				unBorderColor = 0xffffff00; // purple
			}
			else
			{
				unBorderColor = 0xff00ff00; // green
			}
		}
		else if ( bLayerRedraw )
		{
			int nDepthIndex = m_stackCompositionLayers.Count() - 2;
			unBorderColor = ( nDepthIndex < ARRAYSIZE( k_unLayerBorderColors ) ) ? k_unLayerBorderColors[nDepthIndex] : k_unLayerBorderColors[ARRAYSIZE( k_unLayerBorderColors ) - 1];
		}
		else
		{
			unBorderColor = 0xffff0000; // blue
		}
		pLayer->SetBorder( k_flLayerBorderWidth, k_flLayerBorderWidth, k_flLayerBorderWidth, k_flLayerBorderWidth,
			unBorderColor, unBorderColor, unBorderColor, unBorderColor );
		pLayer->DrawBorder( this );
	}
	else if ( bLayerRedraw )
	{
		pLayer->ActivateRenderTarget();
		pLayer->DrawBorder( this );
	}

	if ( bLayerRedraw )
		pLayer->PopClipLayersAndFlush();

	Assert( pLayer->GetClipLayerCount() == 0 );

	bool bInset;
	bool bFill;
	float flHorOffset;
	float flVerOffset;
	float flBlurRadius;
	float flSpreadDistance;
	uint32 shadowColor;
	pLayer->GetBoxShadow( bInset, bFill, flHorOffset, flVerOffset, flBlurRadius, flSpreadDistance, shadowColor );
	// Msg( "layer %p : %s %s %5.2f %5.2f %5.2f %5.2f %08x\n", pLayer, bInset ? "true " : "false", bFill ? "true " : "false", flHorOffset, flVerOffset, flBlurRadius, flSpreadDistance, shadowColor );

	// If we have an outer box-shadow, need to create a layer for that first
	CSource2CompositionLayer *pShadowOutLayer = NULL; 
	OuterShadowLayerParams_t shadowOutLayerParams = {};

	CSource2CompositionLayer *pShadowInsetLayer = NULL;


	// Notice we check BFullyOccluded for inset, not outer, outer could be outside us, so we must draw it.  The bool just means our quad was fully occluded.
	bool bTransparent = ((shadowColor >> 24) & 0xff) == 0 ? true : false;
	if ( bInset == false && !bTransparent && !s_convarPanoramaDisableBoxShadow.GetBool() )
	{
		
		shadowOutLayerParams.flWidthLayer = pLayer->GetWidth();
		shadowOutLayerParams.flHeightLayer = pLayer->GetHeight();
		shadowOutLayerParams.borderRadii[0] = pLayer->AccessCornerRadii()[0];
		shadowOutLayerParams.borderRadii[1] = pLayer->AccessCornerRadii()[1];
		shadowOutLayerParams.borderRadii[2] = pLayer->AccessCornerRadii()[2];
		shadowOutLayerParams.borderRadii[3] = pLayer->AccessCornerRadii()[3];
		shadowOutLayerParams.borderRadii[4] = pLayer->AccessCornerRadii()[4];
		shadowOutLayerParams.borderRadii[5] = pLayer->AccessCornerRadii()[5];
		shadowOutLayerParams.borderRadii[6] = pLayer->AccessCornerRadii()[6];
		shadowOutLayerParams.borderRadii[7] = pLayer->AccessCornerRadii()[7];
		shadowOutLayerParams.flHorOffset = flHorOffset;
		shadowOutLayerParams.flVerOffset = flVerOffset;
		shadowOutLayerParams.flBlurRadius = flBlurRadius;
		shadowOutLayerParams.flSpreadDistance = flSpreadDistance;
		shadowOutLayerParams.shadowColor = shadowColor;
		shadowOutLayerParams.bFill = bFill;
		
		pShadowOutLayer = GetOuterShadowLayer( shadowOutLayerParams );
	}
	else if ( !pLayer->BFullyOccluded() && bInset == true && !bTransparent && !s_convarPanoramaDisableBoxShadow.GetBool() )
	{
		pShadowInsetLayer = GetInsetShadowLayer( pLayer->GetWidth(), pLayer->GetHeight(), pLayer->AccessCornerRadii(), pLayer->AccessBorderWidths(), flHorOffset, flVerOffset, flBlurRadius, flSpreadDistance, shadowColor );
	}

	BasicQuad_t *pQuad = pLayer->AccessRenderQuad();
	bool bHadOverlappingOcclusion = false;

	if ( !pLayer->BFullyOccluded() )
	{
		// If we have an occluded region then we could use the scissor to draw 4 quads around it.
		
		float flBlurPasses = 0.0f;
		float flBlurStdDevHor = 0.0f;
		float flBlurStdDevVer = 0.0f;
		BlurType_t  blurType = BT_NORMAL;
		pLayer->GetBlurValues( blurType, flBlurPasses, flBlurStdDevHor, flBlurStdDevVer );

		// If we have blur (and we redrew), then draw into another layer for blur first
		if ( bLayerRedraw && flBlurPasses > 0.0f && (flBlurStdDevHor > 0.0f || flBlurStdDevVer > 0.0f) && !s_convarPanoramaDisableBlur.GetBool() )
		{
			VPROF( "CSource2CompositionLayer::PopCompositingLayer - Draw Blur" );

			CRenderAttributes renderAttributes;

			// If we are a Blurtarget then this call will send down the rectangles for the pixel shader etc
			bool bIsBlurTarget = SetupBlurPanelAttr( renderAttributes, pLayer );

			// Apply gaussian or fast gaussian blur
			
			if ( ( bIsBlurTarget && renderAttributes.GetNumSourceBlurRects() ) || ( !bIsBlurTarget ) )
			{
				if ( blurType == BT_NORMAL )
				{
					ApplyGaussianBlur( renderAttributes, pLayer, flBlurPasses, flBlurStdDevHor, flBlurStdDevVer );
				}
				else
				{
					ApplyFastGaussianBlur( renderAttributes, pLayer, flBlurPasses, flBlurStdDevHor, flBlurStdDevVer, blurType );
				}
			}
		}
	}

	CSource2CompositionLayer *pParent = m_stackCompositionLayers[m_stackCompositionLayers.Count() - 2];

	pLayer->SetReusedFromCache( false );

	// Pop it off the stack and put it in the free list
	{
		AUTO_LOCK( m_MutexReservedLayers );
		CSource2CompositionLayer *p = m_ReservedLayers.Find( pLayer->GetContextID(), m_flCurrentRenderFrameTime );

		if ( s_convarPanoramaDisableLayerCache.GetBool() )
		{
			if ( p )
			{
				m_ReservedLayers.Remove( pLayer->GetContextID() );
			}
			m_FreeLayers.Insert( pLayer, pLayer, m_flCurrentRenderFrameTime );
			p = pLayer;
		}

		if ( p == NULL )
		{
			// Keep it in the reserved list only if it was reasonable size, otherwise allow earlier re-use since saving re-rendering it can't be very useful
			if ( pLayer->BShouldCache() && pLayer->GetWidth() * pLayer->GetHeight() > s_convarPanoramaMinCompLayerCacheCost.GetInt() )
				m_ReservedLayers.Insert( pLayer->GetContextID(), pLayer, m_flCurrentRenderFrameTime );
			else
				m_FreeLayers.Insert( pLayer, pLayer, m_flCurrentRenderFrameTime );
		}
		else
		{
			Assert( p == pLayer );
		}
	}
	m_stackCompositionLayers.Remove( m_stackCompositionLayers.Count() - 1 );

	// Restore "fractional pixel positions" to whatever it was before PushCompositingLayer
	m_eCurrentFractionalPixelPositions = pLayer->GetPriorFractionalPixelPositions();

	float flScale2DX, flScale2DY;
	pLayer->Get2DScaleFactors( flScale2DX, flScale2DY );

	// If we had a shadow layer, then it's time to draw it now - must happen even if fully occluded as this draws outside our quad, and we don't know if that was occluded
	if ( pShadowOutLayer && !pLayer->BOffscreen() )
	{
		pParent->ActivateRenderTarget();

		DrawOuterShadowLayer( pLayer->GetWidth(), pLayer->GetHeight(), pQuad->m_vPosition[0].x, pQuad->m_vPosition[0].y, flScale2DX, flScale2DY, pQuad->m_vColor.w, pLayer->AccessCornerRadii(), pLayer->AccessMatrix(), pParent->GetWidth(),
			pParent->GetHeight(), pShadowOutLayer, bFill, flHorOffset, flVerOffset, flBlurRadius, flSpreadDistance, shadowColor, pLayer->AccessRenderQuad()->m_flZ);

		if ( !s_convarPanoramaDisableOuterShadowLayerCache.GetBool() )
		{
			m_OuterShadowLayers.Insert( shadowOutLayerParams, pShadowOutLayer, m_flCurrentRenderFrameTime );
		}
		else
		{
			m_FreeLayers.Insert( pShadowOutLayer, pShadowOutLayer, m_flCurrentRenderFrameTime );
		}
	}

	pParent->ActivateRenderTarget();

	if ( !pLayer->BOffscreen() )
	{
		// forestw: use fancy quad rendering to apply outer corner clipping, inset shadow (technically a border) and desaturation while compositing
		//	const float *pflInnerRadii = pLayer->AccessCornerRadii();
		const float *pflOuterRadii = pLayer->AccessCornerRadii();

		FancyQuadParameters_t FancyParam( FancyQuadFlag_OuterCorner );
		FancyParam.m_flZ = pQuad->m_flZ;
		FancyParam.m_flVertexMin[0] = pQuad->m_vPosition[0].x;
		FancyParam.m_flVertexMin[1] = pQuad->m_vPosition[0].y;
		FancyParam.m_flVertexMax[0] = pQuad->m_vPosition[2].x;
		FancyParam.m_flVertexMax[1] = pQuad->m_vPosition[2].y;
		FancyParam.m_flTexCoordMin[0] = pQuad->m_vUV[0].x;
		FancyParam.m_flTexCoordMin[1] = pQuad->m_vUV[0].y;
		FancyParam.m_flTexCoordMax[0] = pQuad->m_vUV[2].x;
		FancyParam.m_flTexCoordMax[1] = pQuad->m_vUV[2].y;
		FancyParam.m_flOpacityTexCoordMin[0] = pQuad->m_vUV[0].x; //pQuad[ 0 ].masku1;
		FancyParam.m_flOpacityTexCoordMin[1] = pQuad->m_vUV[0].y; //pQuad[ 0 ].maskv1;
		FancyParam.m_flOpacityTexCoordMax[0] = pQuad->m_vUV[2].x; //pQuad[ 2 ].masku1;
		FancyParam.m_flOpacityTexCoordMax[1] = pQuad->m_vUV[2].y; //pQuad[2].maskv1;
		// TODO forestw: we can combine all border handling directly into this compositing step, we don't need pInsetShadowLayer
		FancyParam.m_flCornerRadii[0][0] = pflOuterRadii[0]; //outer
		FancyParam.m_flCornerRadii[0][1] = pflOuterRadii[1];
		FancyParam.m_flCornerRadii[1][0] = pflOuterRadii[2];
		FancyParam.m_flCornerRadii[1][1] = pflOuterRadii[3];
		FancyParam.m_flCornerRadii[2][0] = pflOuterRadii[4];
		FancyParam.m_flCornerRadii[2][1] = pflOuterRadii[5];
		FancyParam.m_flCornerRadii[3][0] = pflOuterRadii[6];
		FancyParam.m_flCornerRadii[3][1] = pflOuterRadii[7];
		Vector4D vec = VecColorFromABGR( shadowColor );
		FancyParam.m_flBorderColor[0] = vec.x * vec.w;
		FancyParam.m_flBorderColor[1] = vec.y * vec.w;
		FancyParam.m_flBorderColor[2] = vec.z * vec.w;
		FancyParam.m_flBorderColor[3] = vec.w;

		FancyQuadBrush_t FancyBrush( pQuad->m_vColor.x, pQuad->m_vColor.y, pQuad->m_vColor.z, pQuad->m_vColor.w ); // already premultiplied

		FancyQuadDraw_t fancyQuadDraw;
		fancyQuadDraw.m_hTexture0 = pLayer->GetTextureHandle();
		fancyQuadDraw.m_flTexture0TexCoordScale[0] = pLayer->GetRTOriginalWidthScale();
		fancyQuadDraw.m_flTexture0TexCoordScale[1] = pLayer->GetRTOriginalHeightScale();
		fancyQuadDraw.m_hTexture1 = GetOpacityMaskShaderResourceViewForTexture( pLayer->GetOpacityMaskTexture(), &fancyQuadDraw.m_flTexture1TexCoordScale[0], &fancyQuadDraw.m_flTexture1TexCoordScale[1] );
		fancyQuadDraw.m_flHueShift = pLayer->GetHueShift();
		fancyQuadDraw.m_flSaturation = pLayer->GetSaturation();
		fancyQuadDraw.m_flBrightness = pLayer->GetBrightness();
		fancyQuadDraw.m_flContrast = pLayer->GetContrast();
		fancyQuadDraw.m_flOpacityMaskOpacity = pLayer->GetOpacityMaskOpacity();
		fancyQuadDraw.m_nWide = pParent->GetWidth();
		fancyQuadDraw.m_nTall = pParent->GetHeight();
		fancyQuadDraw.m_pQuadParameters = &FancyParam;
		fancyQuadDraw.m_pQuadBrush = &FancyBrush;
		fancyQuadDraw.m_pVMatrix = pLayer->AccessMatrix();
		fancyQuadDraw.m_flScale2DX = flScale2DX;
		fancyQuadDraw.m_flScale2DY = flScale2DY;
		fancyQuadDraw.m_flTextureWidth = -1.0f;
		fancyQuadDraw.m_flTextureHeight = -1.0f;
		fancyQuadDraw.m_bClipToLayer = true;

		bool bRadialClip;
		float flUnused;
		float flRadialClipStartAngle;
		float flRadialClipSectorAngle;
		pLayer->GetRadialClip( bRadialClip, flUnused, flUnused, flRadialClipStartAngle, flRadialClipSectorAngle );
		if ( bRadialClip )
		{
			// handle negative clip size
			if ( flRadialClipSectorAngle < 0 )
			{
				flRadialClipSectorAngle = -flRadialClipSectorAngle;
				flRadialClipStartAngle -= flRadialClipSectorAngle;
			}

			// normalize start angle
			flRadialClipStartAngle = AngleNormalizePositive( flRadialClipStartAngle );

			fancyQuadDraw.m_flRadialClipStartAngle = flRadialClipStartAngle;
			fancyQuadDraw.m_flRadialClipSectorAngle = flRadialClipSectorAngle;
		}
		else
		{
			fancyQuadDraw.m_flRadialClipStartAngle = 0.0f;
			fancyQuadDraw.m_flRadialClipSectorAngle = 0.0f;
		}


		PushClipLayerRenderCommand_t cmd;
		if ( !pLayer->BFullyOccluded() )
		{
			if ( ( pLayer->GetMixBlendMode() == k_EMixBlendModeNormal )
				&& bHadOverlappingOcclusion && s_convarPanoramaExperimentalOverdrawPrevention.GetBool() )
			{
				VPROF( "PopComp- Overdraw" );

				Vector2D *vecOclludedRegion = pLayer->AccessOccludedRegion();

				CUtlVector<Vector4D> vecQuadsToDraw;
				vecQuadsToDraw.EnsureCapacity( 4 );

				// Left strip
				if ( pQuad->m_vPosition[0].x < vecOclludedRegion[0].x )
				{
					vecQuadsToDraw.AddToTail( Vector4D( pQuad->m_vPosition[0].x, pQuad->m_vPosition[0].y, vecOclludedRegion[0].x, pQuad->m_vPosition[2].y ) );
				}

				// Top strip
				if ( vecOclludedRegion[0].y > pQuad->m_vPosition[0].y )
				{
					vecQuadsToDraw.AddToTail( Vector4D( vecOclludedRegion[0].x, pQuad->m_vPosition[0].y, vecOclludedRegion[1].x, vecOclludedRegion[0].y ) );
				}

				// Right strip
				if ( pQuad->m_vPosition[2].x > vecOclludedRegion[1].x )
				{
					vecQuadsToDraw.AddToTail( Vector4D( vecOclludedRegion[1].x, pQuad->m_vPosition[0].y, pQuad->m_vPosition[2].x, pQuad->m_vPosition[2].y ) );
				}

				// Bottom strip
				if ( pQuad->m_vPosition[2].y > vecOclludedRegion[1].y )
				{
					vecQuadsToDraw.AddToTail( Vector4D( vecOclludedRegion[0].x, vecOclludedRegion[1].y, vecOclludedRegion[1].x, pQuad->m_vPosition[2].y ) );
				}

#if 0
				try
				{
					CPanelPtr< CUIPanel > ptr;
					ptr.SetFromUInt64( pLayer->GetContextID() );

					CPanelPtr< CUIPanel > ptrParent;
					ptrParent.SetFromUInt64( pParent->GetContextID() );

					Msg( " Splitting %s (%s) (%p) into strips to avoid drawing occluded region into parent (%s (%s) - %1.2fx%1.2f): %1.2f,%1.2f %1.2f,%1.2f\n", ptr->GetID(), ptr->GetPanelType().String(), ptr->ClientPtr(),
						ptrParent->GetID(), ptrParent->GetPanelType().String(), pParent->GetWidth(), pParent->GetHeight(), vecOclludedRegion[0].x, vecOclludedRegion[0].y, vecOclludedRegion[1].x, vecOclludedRegion[1].y );
					//for ( int i = 0; i < vecQuadsToDraw.Count(); ++i )
					//{
					//	Msg( "     Drawing: %1.2f,%1.2f %1.2f,%1.2f\n", vecQuadsToDraw[i].x, vecQuadsToDraw[i].y, vecQuadsToDraw[i].z, vecQuadsToDraw[i].w );
					//}
			} catch ( ... ) { }
#endif

				FOR_EACH_VEC( vecQuadsToDraw, i )
				{

					cmd.top_left.x = vecQuadsToDraw[i].x;
					cmd.top_left.y = vecQuadsToDraw[i].y;
					cmd.bottom_right.x = vecQuadsToDraw[i].z;
					cmd.bottom_right.y = vecQuadsToDraw[i].w;
					pParent->PushClipLayer( cmd );

					DrawFancyQuad( &fancyQuadDraw );

					pParent->PopClipLayer();
				}

			}
			else
			{
				VPROF( "PopComp - Draw" );

				RsBlendStateHandle_t hPriorBlendState = m_hCurrentBlendState;

				if ( pLayer->GetMixBlendMode() == k_EMixBlendModeScreen )
				{
					m_hCurrentBlendState = m_hMixScreenState;
				}
				else if ( pLayer->GetMixBlendMode() == k_EMixBlendModeMultiply )
				{
					m_hCurrentBlendState = m_hMixMultiplyState;
				}
				else if ( pLayer->GetMixBlendMode() == k_EMixBlendModeAdditive )
				{
					m_hCurrentBlendState = m_hMixAdditiveState;
				}
				else if ( pLayer->GetMixBlendMode() == k_EMixBlendModeAdditiveSRGB )
				{
					m_hCurrentBlendState = m_hMixAdditiveSRGBState;
				}
				else if ( pLayer->GetMixBlendMode() == k_EMixBlendModeOpaque )
				{
					m_hCurrentBlendState = m_hMixOpaqueState;
				}

				DrawFancyQuad( &fancyQuadDraw );

				m_hCurrentBlendState = hPriorBlendState;
			}
		}
	}


	// If we had an inset shadow, then it's time to draw it now
	if ( pShadowInsetLayer && !pLayer->BOffscreen() )
	{
		pParent->ActivateRenderTarget();
		DrawInsetShadowLayer( pLayer->GetWidth(), pLayer->GetHeight(), pQuad->m_vPosition[0].x, pQuad->m_vPosition[0].y, flScale2DX, flScale2DY, pQuad->m_vColor.w, pLayer->AccessCornerRadii(), pLayer->AccessBorderWidths(), pLayer->AccessMatrix(), pParent->GetWidth(),
			pParent->GetHeight(), pShadowInsetLayer, flHorOffset, flVerOffset, flBlurRadius, flSpreadDistance, shadowColor);

		m_FreeLayers.Insert( pShadowInsetLayer, pShadowInsetLayer, m_flCurrentRenderFrameTime );
	}

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
// Purpose: Called to push a transform matrix
//-----------------------------------------------------------------------------
void CSource2Surface::PushPanelContextInLayer( const PushPanelContextInLayerRenderCommand_t &renderCommand )
{
	if ( m_stackCompositionLayers.Count() == 0 )
	{
		Assert( m_stackCompositionLayers.Count() != 0 );
	}
	else
	{
		CSource2CompositionLayer *pLayer = m_stackCompositionLayers[ m_stackCompositionLayers.Count() - 1 ];
		pLayer->PushPanelContextInLayer( renderCommand );

		// If there is an outer box shadow, draw it now
		CSource2CompositionLayer::PanelContext_t *pPanelContext = pLayer->AccessPanelContextInLayer();
		if ( !pPanelContext )
			return;

		// Do we have an outer shadow we need to draw before the panel contents?
		if ( pPanelContext->m_bBoxShadowInset == false && (pPanelContext->m_rgbaBoxShadowColor >> 24) != 0x00 && !s_convarPanoramaDisableBoxShadow.GetBool() )
		{
			// If we have an outer box-shadow, need to create a layer for that first

			float rgRadii[8];
			V_memset( rgRadii, 0, sizeof( float ) * 8 );

			OuterShadowLayerParams_t params;
			params.flWidthLayer = pPanelContext->m_flLayerWidth;
			params.flHeightLayer = pPanelContext->m_flLayerHeight;
			params.borderRadii[0] = 0.0f;
			params.borderRadii[1] = 0.0f;
			params.borderRadii[2] = 0.0f;
			params.borderRadii[3] = 0.0f;
			params.borderRadii[4] = 0.0f;
			params.borderRadii[5] = 0.0f;
			params.borderRadii[6] = 0.0f;
			params.borderRadii[7] = 0.0f;
			params.flHorOffset = pPanelContext->m_flBoxShadowHorOffset;
			params.flVerOffset = pPanelContext->m_flBoxShadowVerOffset;
			params.flBlurRadius = pPanelContext->m_flBoxShadowBlurRadius;
			params.flSpreadDistance = pPanelContext->m_flBoxShadowSpreadDistance;
			params.shadowColor = pPanelContext->m_rgbaBoxShadowColor;
			params.bFill = pPanelContext->m_bBoxShadowFill;

			CSource2CompositionLayer *pShadowOutLayer = GetOuterShadowLayer( params );
			if ( pShadowOutLayer )
			{
				pLayer->ActivateRenderTarget();
				DrawOuterShadowLayer( pPanelContext->m_flLayerWidth, pPanelContext->m_flLayerHeight, pPanelContext->m_flPositionX, pPanelContext->m_flPositionY, 1.0f, 1.0f, 1.0f, rgRadii, pLayer->AccessPushedMatrix(), pLayer->GetWidth(), 
					pLayer->GetHeight(), pShadowOutLayer, pPanelContext->m_bBoxShadowFill, pPanelContext->m_flBoxShadowHorOffset, pPanelContext->m_flBoxShadowVerOffset, pPanelContext->m_flBoxShadowBlurRadius, pPanelContext->m_flBoxShadowSpreadDistance, pPanelContext->m_rgbaBoxShadowColor, pPanelContext->m_flPositionZ);

				if ( !s_convarPanoramaDisableOuterShadowLayerCache.GetBool() )
				{
					m_OuterShadowLayers.Insert( params, pShadowOutLayer, m_flCurrentRenderFrameTime );
				}
				else
				{
					m_FreeLayers.Insert( pShadowOutLayer, pShadowOutLayer, m_flCurrentRenderFrameTime );
				}
			}
		}

		// Save current blend state (restored in PopPanelContextInLayer) and overwrite it if necessary
		pPanelContext->m_hPriorBlendState = RENDER_BLEND_STATE_HANDLE_INVALID;
		if ( renderCommand.mix_blend_mode == k_EMixBlendModeScreen )
		{
			pPanelContext->m_hPriorBlendState = m_hCurrentBlendState;
			m_hCurrentBlendState = m_hMixScreenState;
			m_nBlendStateOverridden++;
		}
		else if ( renderCommand.mix_blend_mode == k_EMixBlendModeMultiply )
		{
			pPanelContext->m_hPriorBlendState = m_hCurrentBlendState;
			m_hCurrentBlendState = m_hMixMultiplyState;
			m_nBlendStateOverridden++;
		}
		else if ( renderCommand.mix_blend_mode == k_EMixBlendModeAdditive )
		{
			pPanelContext->m_hPriorBlendState = m_hCurrentBlendState;
			m_hCurrentBlendState = m_hMixAdditiveState;
			m_nBlendStateOverridden++;
		}
		else if ( renderCommand.mix_blend_mode == k_EMixBlendModeAdditiveSRGB )
		{
			pPanelContext->m_hPriorBlendState = m_hCurrentBlendState;
			m_hCurrentBlendState = m_hMixAdditiveSRGBState;
			m_nBlendStateOverridden++;
		}
		else if ( renderCommand.mix_blend_mode == k_EMixBlendModeOpaque )
		{
			pPanelContext->m_hPriorBlendState = m_hCurrentBlendState;
			m_hCurrentBlendState = m_hMixOpaqueState;
			m_nBlendStateOverridden++;
		}

		pPanelContext->m_ePriorFractionalPixelPositions = m_eCurrentFractionalPixelPositions;
		if ( renderCommand.fractional_pixel_positions != k_EFractionalPixelPositionsDefault )
		{
			m_eCurrentFractionalPixelPositions = renderCommand.fractional_pixel_positions;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: helper for drawing borders for panel contexts
//-----------------------------------------------------------------------------
void CSource2CompositionLayer::PanelContext_t::DrawBorderForPanelContext( CSource2Surface *pParentSurface, CSource2CompositionLayer *pParentLayer )
{
	// Is there any border at all?
	if ( m_rgBorderWidths[0] == 0.0f && m_rgBorderWidths[1] == 0.0f && m_rgBorderWidths[2] == 0.0f && m_rgBorderWidths[3] == 0.0f )
		return;

	// Early out for transparent colors
	bool bHasColor = false;
	for ( int iCur = 0; iCur < V_ARRAYSIZE( m_rgbaBorderColors ); ++iCur )
	{
		if ( ((m_rgbaBorderColors[iCur] >> 24) & 0xff) != 0 )
		{
			bHasColor = true;
			break;
		}
	}

	if ( !bHasColor )
		return;

	pParentLayer->ActivateRenderTarget();

	// forestw: draw top border with diagonal edges to meet nicely with the other borders
	CRenderAttributes renderAttributes;
	renderAttributes.SetIntValue( ATTR_D_TEXTURETYPE_NONE, 1 );
	renderAttributes.SetVMatrixValue( ATTR_MatTransform, pParentLayer->AccessPushedMatrix() ? pParentLayer->AccessPushedMatrix()->Transpose() : VMatrix::GetIdentityMatrix() );
	renderAttributes.SetFloatValue( ATTR_ViewportWidth, pParentLayer->GetWidth() );
	renderAttributes.SetFloatValue( ATTR_ViewportHeight, pParentLayer->GetHeight() );

#if PANDX_DRAW
	FancyQuad_t *q = PanDxGetFancyQuadPtr( 4 );
#else
	FancyQuad_t q[4];
#endif

	Vector2D vPosScroll = Vector2D( m_flPositionX - m_flScrollX, m_flPositionY - m_flScrollY );
		
	q[ 0 ].FqInit( vPosScroll + Vector2D( 0.0f, 0.0f ),
				 vPosScroll + Vector2D( m_rgBorderWidths[ 3 ], m_rgBorderWidths[ 0 ] ),
				 vPosScroll + Vector2D( m_flLayerWidth - m_rgBorderWidths[ 1 ], m_rgBorderWidths[ 0 ] ),
				 vPosScroll + Vector2D( m_flLayerWidth, 0.0f ),
				 VecColorFromABGRPreMul( m_rgbaBorderColors[ 0 ] ) );

	q[ 1 ].FqInit( vPosScroll + Vector2D( m_flLayerWidth - m_rgBorderWidths[ 1 ], m_rgBorderWidths[ 0 ] ),
				 vPosScroll + Vector2D( m_flLayerWidth, 0.0f ),
				 vPosScroll + Vector2D( m_flLayerWidth, m_flLayerHeight ),
				 vPosScroll + Vector2D( m_flLayerWidth - m_rgBorderWidths[ 1 ], m_flLayerHeight - m_rgBorderWidths[ 2 ] ),
				 VecColorFromABGRPreMul( m_rgbaBorderColors[ 1 ] ) );

	q[ 2 ].FqInit( vPosScroll + Vector2D( m_flLayerWidth - m_rgBorderWidths[ 1 ], m_flLayerHeight - m_rgBorderWidths[ 2 ] ),
				 vPosScroll + Vector2D( m_flLayerWidth, m_flLayerHeight ),
				 vPosScroll + Vector2D( 0.0f, m_flLayerHeight ),
				 vPosScroll + Vector2D( m_rgBorderWidths[ 3 ], m_flLayerHeight - m_rgBorderWidths[ 2 ] ),
				 VecColorFromABGRPreMul( m_rgbaBorderColors[ 2 ] ) );

	q[ 3 ].FqInit( vPosScroll + Vector2D( m_rgBorderWidths[ 3 ], m_flLayerHeight - m_rgBorderWidths[ 2 ] ),
				 vPosScroll + Vector2D( 0.0f, m_flLayerHeight ),
				 vPosScroll + Vector2D( 0.0f, 0.0f ),
				 vPosScroll + Vector2D( m_rgBorderWidths[ 3 ], m_rgBorderWidths[ 0 ] ),
				 VecColorFromABGRPreMul( m_rgbaBorderColors[ 3 ] ) );

	RenderViewport_t viewport;
	pParentSurface->m_pRenderContext->GetViewport( &viewport, 0 );
	if ( viewport.m_nHeight == 0 )
		return;

	// pre transform (since we're optimising out VS consts)
	PreXFormFancyQuadPositions( &renderAttributes, q, pParentLayer->AccessPushedMatrix(), pParentLayer->GetWidth(), pParentLayer->GetHeight(), 4 );

	#if ( PANDX_DRAW )
	if ( !g_bPanDx )
	#endif
	{
		const IMaterial2 *pMaterial = pParentSurface->m_hFancyQuadMaterial;
		IMaterialMode *pMode = pMaterial->GetMode();
		if ( !pMode )
		{
			Log_Warning( LOG_PANORAMA, "CSource2Surface::DrawFancyQuad() GetMode == NULL? Can't Render\n" );
			return;
		}

		int nVerts = 4 * 2 * 3;
		CDynamicVertexData< Source2FancyQuadVertex_t > triVB( pParentSurface->m_pRenderContext, nVerts, "PanoramaFancyQuad", "Panorama" );
		triVB.Reset();

		int vertidx[] = { 0,2,1,0,3,2 };

		for ( int i = 0; i < 4; i++ )
		{
			FancyQuad_t *pFancyQuad = q + i;

			for ( int &j : vertidx )
			{
				triVB->m_vecPosition.Init( pFancyQuad->m_Verts[ j ].m_vPosition.x, pFancyQuad->m_Verts[ j ].m_vPosition.y, 0.0f, 1.0f );
				triVB->m_vecColor0 = pFancyQuad->m_vColor;
				triVB->m_vecColor1 = pFancyQuad->m_vColorStop;
				triVB->m_vecTexCoordGradientCoord.Init( pFancyQuad->m_Verts[ j ].m_vUV.x, pFancyQuad->m_Verts[ j ].m_vUV.y,
														pFancyQuad->m_Verts[ j ].m_vUVGradient.x, pFancyQuad->m_Verts[ j ].m_vUVGradient.y );
				triVB->m_vecOpacityTexCoord.Init( pFancyQuad->m_Verts[ j ].m_vUVOpacity.x, pFancyQuad->m_Verts[ j ].m_vUVOpacity.y, 0, 0 );
				triVB->m_vecFragCoordWdHt = pFancyQuad->m_Verts[ j ].m_vFragCoordWdHt;

				triVB.AdvanceVertex();
			}
		}

		triVB.UnlockAndBind( 0, 0 );

		RectBounds_t r;
		pParentLayer->GetCurrentClipRect( r );
		Rect_t rScissor( RoundFloatToInt( r.left ), RoundFloatToInt( r.top ), RoundFloatToInt( r.right - r.left ), RoundFloatToInt( r.bottom - r.top ) );
		if ( rScissor.x < 0 )
		{
			rScissor.width += rScissor.x;
			rScissor.x = 0;
		}

		if ( rScissor.y < 0 )
		{
			rScissor.height += rScissor.y;
			rScissor.y = 0;
		}

		pParentSurface->m_pRenderContext->SetScissorRect( rScissor );

		// Draw batch
		MaterialRenderablePass_t passes[ MATERIAL_RENDERABLE_PASS_MAX ];
		int nPasses = pMode->ComputeRenderablePassesForContext( &renderAttributes, pParentSurface->m_pRenderContext, passes );
		for ( int i = 0; i < nPasses; i++ )
		{
			g_pMaterialSystem2->SetRenderStateForRenderablePass( &renderAttributes, pParentSurface->m_pRenderContext, pParentSurface->m_hSource2FancyQuadVertexLayout, passes[ i ] );

			pParentSurface->m_pRenderContext->SetCullMode( RENDER_CULLMODE_CULL_NONE );
			pParentSurface->m_pRenderContext->SetZBufferMode( RENDER_ZBUFFER_NONE );
			pParentSurface->m_pRenderContext->SetBlendState( pParentSurface->m_hCurrentBlendState );

			// PANDRAWCALL FQ - CSource2CompositionLayer.PanelContext_t.DrawBorderForPanelContext
			pParentSurface->m_pRenderContext->CtxDraw( RENDER_PRIM_TRIANGLES, 0, nVerts );
			pParentSurface->GetFrameStats().m_nDrawBorderForPanelContextCalls++;
		}

	}
	#if ( PANDX_DRAW )
	else
	{
		RectBounds_t r;
		pParentLayer->GetCurrentClipRect( r );
		Rect_t rScissor( RoundFloatToInt( r.left ), RoundFloatToInt( r.top ), RoundFloatToInt( r.right - r.left ), RoundFloatToInt( r.bottom - r.top ) );
		if ( rScissor.x < 0 )
		{
			rScissor.width += rScissor.x;
			rScissor.x = 0;
		}

		if ( rScissor.y < 0 )
		{
			rScissor.height += rScissor.y;
			rScissor.y = 0;
		}

		pParentSurface->m_pRenderContext->SetScissorRect( rScissor );

		pParentSurface->m_pRenderContext->SetCullMode( RENDER_CULLMODE_CULL_NONE );
		pParentSurface->m_pRenderContext->SetZBufferMode( RENDER_ZBUFFER_NONE );
		pParentSurface->m_pRenderContext->SetBlendState( pParentSurface->m_hCurrentBlendState );
		( (CRenderContext*)pParentSurface->m_pRenderContext )->m_pAttr = &renderAttributes;

		//PANDRAWCALL FQ - CSource2Surface.DrawFancyQuad
		PanDxDrawFancyQuads( (CRenderContext*)pParentSurface->m_pRenderContext, q, 4 );

	}
	#endif

}


//-----------------------------------------------------------------------------
// Purpose: Called to pop a transform matrix
//-----------------------------------------------------------------------------
void CSource2Surface::PopPanelContextInLayer( const PopPanelContextInLayerRenderCommand_t &renderCommand )
{
	if ( m_stackCompositionLayers.Count() == 0 )
	{
		Assert( m_stackCompositionLayers.Count() != 0 );
	}
	else
	{
		CSource2CompositionLayer *pLayer = m_stackCompositionLayers[m_stackCompositionLayers.Count() - 1];

		// If there is an inset box shadow, draw it now
		CSource2CompositionLayer::PanelContext_t *pPanelContext = pLayer->AccessPanelContextInLayer();
		if ( pPanelContext )
		{
			// Restore blend state to whatever it was before PushPanelContextInLayer
			if ( pPanelContext->m_hPriorBlendState != RENDER_BLEND_STATE_HANDLE_INVALID )
			{
				m_hCurrentBlendState = pPanelContext->m_hPriorBlendState;
				--m_nBlendStateOverridden;
			}

			// Restore "fractional pixel positions" to whatever it was before PushPanelContextInLayer
			m_eCurrentFractionalPixelPositions = pPanelContext->m_ePriorFractionalPixelPositions;
			
			if ( pPanelContext->m_bBoxShadowInset == true && (pPanelContext->m_rgbaBoxShadowColor >> 24) != 0x00 && !s_convarPanoramaDisableBoxShadow.GetBool() )
			{
				// Draw inset shadow now
				float rgRadii[8];
				V_memset( rgRadii, 0, sizeof( float ) * 8 );

				float rgBorderWidths[4];
				V_memset( rgBorderWidths, 0, sizeof( float ) * 4 );

				CSource2CompositionLayer *pShadowInsetLayer = GetInsetShadowLayer( pPanelContext->m_flLayerWidth, pPanelContext->m_flLayerHeight, rgRadii, rgBorderWidths, pPanelContext->m_flBoxShadowHorOffset, pPanelContext->m_flBoxShadowVerOffset, pPanelContext->m_flBoxShadowBlurRadius, pPanelContext->m_flBoxShadowSpreadDistance, pPanelContext->m_rgbaBoxShadowColor );
				if ( pShadowInsetLayer )
				{
					pLayer->ActivateRenderTarget();
					DrawInsetShadowLayer( pPanelContext->m_flLayerWidth, pPanelContext->m_flLayerHeight, pPanelContext->m_flPositionX, pPanelContext->m_flPositionY, 1.0f, 1.0f, 1.0f, rgRadii, rgBorderWidths, pLayer->AccessMatrix(), pLayer->GetWidth(),
						pLayer->GetHeight(), pShadowInsetLayer, pPanelContext->m_flBoxShadowHorOffset, pPanelContext->m_flBoxShadowVerOffset, pPanelContext->m_flBoxShadowBlurRadius, pPanelContext->m_flBoxShadowSpreadDistance, pPanelContext->m_rgbaBoxShadowColor);

					m_FreeLayers.Insert( pShadowInsetLayer, pShadowInsetLayer, m_flCurrentRenderFrameTime );
				}
			}

			// Draw border, this is a noop if nothing was needed
			pPanelContext->DrawBorderForPanelContext( this, pLayer );
		}

		pLayer->PopPanelContextInLayer( renderCommand );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called to push a clip layer - different than a composition layer in that
// this won't necessarily require a render target, it's just changing the clip/scissor rect
// for the current layer.
//-----------------------------------------------------------------------------
void CSource2Surface::PushClipLayer( const PushClipLayerRenderCommand_t &renderCommand )
{
	if ( m_stackCompositionLayers.Count() == 0 )
	{
		Assert( m_stackCompositionLayers.Count() != 0 );
	}
	else
	{
		CSource2CompositionLayer *pLayer = m_stackCompositionLayers[ m_stackCompositionLayers.Count() - 1 ];
		pLayer->PushClipLayer( renderCommand );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called to pop a clip layer
//-----------------------------------------------------------------------------
void CSource2Surface::PopClipLayer( const PopClipLayerRenderCommand_t &renderCommand )
{
	if ( m_stackCompositionLayers.Count() == 0 )
	{
		Assert( m_stackCompositionLayers.Count() != 0 );
	}
	else
	{
		CSource2CompositionLayer *pLayer = m_stackCompositionLayers[m_stackCompositionLayers.Count() - 1];
		pLayer->PopClipLayer();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get the current clip rect
//-----------------------------------------------------------------------------
void CSource2CompositionLayer::GetCurrentClipRect( RectBounds_t &r )
{
	if ( m_vecClipLayers.Count() > 0 )
	{
		r = m_vecClipLayers.Element( m_vecClipLayers.Count() - 1 );
	}
	else
	{
		r.left = 0;
		r.top = 0;
		r.right = m_flLayerWidth;
		r.bottom = m_flLayerHeight;
	}
}


void CSource2Surface::TextureBecameFullyResident( HRenderTexture hTex )
{
}


void CSource2Surface::TextureBecameEvicted( HRenderTexture hTex )
{
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CSource2Surface::SpewTextureInfo()
{
}


// Whenever the ConVar value changes, force a full invalidation of all windows so they redraw with the composition layer borders
void HighlightCompositionLayersChanged( IConVar *pConVar, const char *pOldValue, float flOldValue )
{
	if ( !UIEngine() )
		return;

	CUtlVector< IUIWindow * > vecWindows;
	UIEngine()->GetWindowsForDebugger( vecWindows );

	for ( IUIWindow *pWindow : vecWindows )
	{
		pWindow->ForceFullRepaint();
	}
}

// Whenever the ConVar value changes, force a clear of the text cache and a full redraw
void DebugFontSelectionChanged( IConVar *pConVar, const char *pOldValue, float flOldValue )
{
	if( !UIEngine() )
		return;

	bool bNewEnable = s_convarDebugFontSelection.GetBool();
	bool bOldEnable = flOldValue != 0.0f;

	if( bNewEnable && !bOldEnable )
	{
		CUtlVector< IUIWindow * > vecWindows;
		UIEngine()->GetWindowsForDebugger( vecWindows );

		for( IUIWindow *pWindow : vecWindows )
		{
			pWindow->ForceFullRepaint();
		}

		s_bClearTextCacheBeforeNextFrame = true;
	}

	g_IUITextServices->SetDebugFontSelection( bNewEnable );
}

//-----------------------------------------------------------------------------
// Purpose: Draw debug text into the current composition layer. Used to render panorama frame stats
//-----------------------------------------------------------------------------
void CSource2Surface::DrawDebugText( const char *pchText, float x0, float y0, float fontSize, const Color &color )
{
	RenderTextRegionCommand_t dbgTextCommand;

	dbgTextCommand.raw_text = (void*)pchText;
	dbgTextCommand.raw_text_bytes = V_strlen( pchText ) + 1;
	dbgTextCommand.text_chars = V_UnicodeLength( pchText );
	dbgTextCommand.text_encoding = k_EPanoramaTextEncodingUTF8;
	dbgTextCommand.top_left.x = x0;
	dbgTextCommand.top_left.y = y0;
	dbgTextCommand.bottom_right.x = dbgTextCommand.top_left.x + 200.0f;
	dbgTextCommand.bottom_right.y = dbgTextCommand.top_left.y + 50.0f;
	dbgTextCommand.text_align = k_ETextAlignLeft;
	dbgTextCommand.wrapping = false;
	dbgTextCommand.ellipsis = false;
	dbgTextCommand.line_height = k_flFloatNotSet;
	dbgTextCommand.text_shadow = nullptr;

	dbgTextCommand.default_format.font_name = "Arial Unicode MS";
	dbgTextCommand.default_format.font_size = fontSize;
	dbgTextCommand.default_format.letter_spacing = 1;
	dbgTextCommand.default_format.inline_object = nullptr;
	dbgTextCommand.default_format.font_weight = k_EFontWeightNormal;
	dbgTextCommand.default_format.font_style = k_EFontStyleNormal;
	dbgTextCommand.default_format.text_decoration = k_ETextDecorationNone;
	
	CRenderDataListBuilder< FillBrush_t > fillBrushCollectionBuilder( dbgTextCommand.default_format.fill_brush_collection.fill_brush, nullptr );
	CRenderDataListBuilder< FillBrush_t >::DefaultListNode_t brushListNode;
	memset( &brushListNode, 0, sizeof( brushListNode ) );
	brushListNode.entry.eFillBrushType = k_EFillBrushType_Color;
	brushListNode.entry.opacity = 1.0f;
	brushListNode.entry.color_rgba = color.GetRawColor();;
	fillBrushCollectionBuilder.AddNodeToTail( &brushListNode );

	DrawTextRegion( dbgTextCommand );
}

//-----------------------------------------------------------------------------
// Purpose: Render frame stats on the screen.
//-----------------------------------------------------------------------------

// This structure hold all stats that we want to sample for min/max/average

struct Source2SurfaceStatsTracker_t
{
	enum 
	{
		NUM_SAMPLES = 400
	};

	int m_nSampleIdx;
	Source2SurfaceStats_t m_Samples[NUM_SAMPLES];
	Source2SurfaceStats_t m_min;
	Source2SurfaceStats_t m_max;
	Source2SurfaceStats_t m_tot;
	Source2SurfaceStats_t m_avg;
	Source2SurfaceStats_t m_cur;

	void AddSample( const Source2SurfaceStats_t &sample )
	{
		m_cur = sample;
		m_Samples[m_nSampleIdx] = sample;
		m_nSampleIdx = ( m_nSampleIdx + 1 ) % NUM_SAMPLES;

		// Recompute min, max and total
		m_min = sample;
		m_max = sample;
		m_tot.InitZero();
		m_avg.InitZero();

		#define STATS_TRACKER_COMPUTE( field ) \
			m_min.field = MIN( m_min.field, current.field ); \
			m_max.field = MAX( m_max.field, current.field ); \
			m_tot.field += current.field;

		for ( int nSample = 0; nSample < NUM_SAMPLES; ++nSample )
		{
			const Source2SurfaceStats_t &current = m_Samples[nSample];

			STATS_TRACKER_COMPUTE( m_nFramesRendered );
			STATS_TRACKER_COMPUTE( m_flFrameTime );
			//STATS_TRACKER_COMPUTE( m_nCompositionLayersFreeCache );
			//STATS_TRACKER_COMPUTE( m_nCompositionLayersReservedCache );
			//STATS_TRACKER_COMPUTE( m_nCompositionLayersOuterShadowCache );
			STATS_TRACKER_COMPUTE( m_nCompositionLayersCreated );
			STATS_TRACKER_COMPUTE( m_nRTCacheHits );
			STATS_TRACKER_COMPUTE( m_nRTCacheMisses );
			STATS_TRACKER_COMPUTE( m_nDrawBorderForPanelContextCalls );
			STATS_TRACKER_COMPUTE( m_nDrawBorderCalls );
			STATS_TRACKER_COMPUTE( m_nDrawTexturedQuadInternalCalls );
			STATS_TRACKER_COMPUTE( m_nDrawfancyQuadCalls );
			STATS_TRACKER_COMPUTE( m_nPanDxDrawCalls );
			STATS_TRACKER_COMPUTE( m_nPanDxBasicDrawCalls );
			STATS_TRACKER_COMPUTE( m_nPanDxFancyDrawCalls );
		}

		#undef STATS_TRACKER_COMPUTE

		m_avg.m_nFramesRendered = m_tot.m_nFramesRendered / NUM_SAMPLES;
		m_avg.m_flFrameTime = m_tot.m_flFrameTime / NUM_SAMPLES;
		//m_avg.m_nCompositionLayersFreeCache = m_tot.m_nCompositionLayersFreeCache / NUM_SAMPLES;
		//m_avg.m_nCompositionLayersReservedCache = m_tot.m_nCompositionLayersReservedCache / NUM_SAMPLES;
		//m_avg.m_nCompositionLayersOuterShadowCache = m_tot.m_nCompositionLayersOuterShadowCache / NUM_SAMPLES;
		m_avg.m_nCompositionLayersCreated = m_tot.m_nCompositionLayersCreated / NUM_SAMPLES;
		m_avg.m_nRTCacheHits = m_tot.m_nRTCacheHits / NUM_SAMPLES;
		m_avg.m_nRTCacheMisses = m_tot.m_nRTCacheMisses / NUM_SAMPLES;
		m_avg.m_nDrawBorderForPanelContextCalls = m_tot.m_nDrawBorderForPanelContextCalls / NUM_SAMPLES;
		m_avg.m_nDrawBorderCalls = m_tot.m_nDrawBorderCalls / NUM_SAMPLES;
		m_avg.m_nDrawTexturedQuadInternalCalls = m_tot.m_nDrawTexturedQuadInternalCalls / NUM_SAMPLES;
		m_avg.m_nDrawfancyQuadCalls = m_tot.m_nDrawfancyQuadCalls / NUM_SAMPLES;
		m_avg.m_nPanDxDrawCalls = m_tot.m_nPanDxDrawCalls / NUM_SAMPLES;
		m_avg.m_nPanDxBasicDrawCalls = m_tot.m_nPanDxBasicDrawCalls / NUM_SAMPLES;
		m_avg.m_nPanDxFancyDrawCalls = m_tot.m_nPanDxFancyDrawCalls / NUM_SAMPLES;
	}
};

void CSource2Surface::LogStats( const Source2SurfaceStats_t &stats, float x0, float y0 )
{
	//
	// Spike tracker - Get the min, max and average 
	//
	static Source2SurfaceStatsTracker_t statsTracker;
	statsTracker.AddSample( stats );
	
	static CFmtStr1024 s_fmtStr;
	const float flLineOffset = 20.0f;
	const float flFontSize = 18.0f;
	const Color redColor( 255, 0, 0, 255 );
	const float flCol1 = x0 + 20.0f;
	const float flCol2 = flCol1 + 200.0f;
	
	//
	// Render stats
	//

	s_fmtStr.sprintf( "Panorama render stats: %s (%dx%d)", m_SurfaceName.Get(), m_unSurfaceWidth, m_unSurfaceHeight );
	DrawDebugText( s_fmtStr.Get(), x0, y0, flFontSize, redColor );
	y0 += flLineOffset;

	// frame time
	DrawDebugText( "Render Time", flCol1, y0, flFontSize, redColor );
	s_fmtStr.sprintf( 
		"%4.1f ms (min: %4.1f, max: %4.1f, avg: %4.1f)", 
		statsTracker.m_cur.m_flFrameTime * 1000.f, 
		statsTracker.m_min.m_flFrameTime * 1000.f,
		statsTracker.m_max.m_flFrameTime * 1000.f,
		statsTracker.m_avg.m_flFrameTime * 1000.f );
	DrawDebugText( s_fmtStr.Get(), flCol2, y0, flFontSize, redColor );
	y0 += flLineOffset;

	//
	// Layers Cache
	//

	DrawDebugText( "Number of layers in caches", x0, y0, flFontSize, redColor );
	y0 += flLineOffset;

	// Number of layers in the free composition layer cache
	DrawDebugText( "Free Cache", flCol1, y0, flFontSize, redColor );
	s_fmtStr.sprintf( "%llu (%4.1f MB)", statsTracker.m_cur.m_nCompositionLayersFreeCache, statsTracker.m_cur.m_nFreeCacheSizeInBytes / (float)( 1024 * 1024 ) );
	DrawDebugText(  s_fmtStr.Get(), flCol2, y0, flFontSize, redColor );
	y0 += flLineOffset;

	// Number of layers in the reserved cache
	DrawDebugText( "Reserved Cache", flCol1, y0, flFontSize, redColor );
	s_fmtStr.sprintf( "%llu (%4.1f MB)", statsTracker.m_cur.m_nCompositionLayersReservedCache, statsTracker.m_cur.m_nReservedCacheSizeInBytes / (float)( 1024 * 1024 ) );
	DrawDebugText( s_fmtStr.Get(), flCol2, y0, flFontSize, redColor );
	y0 += flLineOffset;

	// Number of layers in the outer shadow layer cache
	DrawDebugText( "Outer Shadow Cache", flCol1, y0, flFontSize, redColor );
	s_fmtStr.sprintf( "%llu (%4.1f MB)", statsTracker.m_cur.m_nCompositionLayersOuterShadowCache, statsTracker.m_cur.m_nOuterShadowCacheSizeInBytes / (float)( 1024 * 1024 ) );
	DrawDebugText( s_fmtStr.Get(), flCol2, y0, flFontSize, redColor );
	y0 += flLineOffset;

	// Number of layers in the image shadow layer cache
	DrawDebugText( "Image Shadow Cache", flCol1, y0, flFontSize, redColor );
	s_fmtStr.sprintf( "%llu (%4.1f MB)", statsTracker.m_cur.m_nCompositionLayersImageShadowCache, statsTracker.m_cur.m_nImageShadowCacheSizeInBytes / (float)( 1024 * 1024 ) );
	DrawDebugText( s_fmtStr.Get(), flCol2, y0, flFontSize, redColor );
	y0 += flLineOffset;

	// Number of layers in the blur layer cache
	DrawDebugText( "Blur Cache", flCol1, y0, flFontSize, redColor );
	s_fmtStr.sprintf( "%llu (%4.1f MB)", statsTracker.m_cur.m_nCompositionLayersBlurCache, statsTracker.m_cur.m_nBlurCacheSizeInBytes / (float)( 1024 * 1024 ) );
	DrawDebugText( s_fmtStr.Get(), flCol2, y0, flFontSize, redColor );
	y0 += flLineOffset;


	// Number of layers allocated this frame
	DrawDebugText( "Allocated", flCol1, y0, flFontSize, redColor );
	s_fmtStr.sprintf( 
		"%llu (min: %llu, max: %llu, avg: %llu, tot: %llu)", 
		statsTracker.m_cur.m_nCompositionLayersCreated,
		statsTracker.m_min.m_nCompositionLayersCreated,
		statsTracker.m_max.m_nCompositionLayersCreated,
		statsTracker.m_avg.m_nCompositionLayersCreated,
		statsTracker.m_tot.m_nCompositionLayersCreated );
	DrawDebugText( s_fmtStr.Get(), flCol2, y0, flFontSize, redColor );
	y0 += flLineOffset;

	// Number of layers in the text shadow cache
	DrawDebugText( "Text Shadow Cache", flCol1, y0, flFontSize, redColor );
	s_fmtStr.sprintf( "%llu (%4.1f MB)", stats.m_nTextShadowCache, stats.m_nTextShadowCacheSizeInBytes / (float)(1024 * 1024) );
	DrawDebugText( s_fmtStr.Get(), flCol2, y0, flFontSize, redColor );
	y0 += flLineOffset;

	//
	// Render target cache
	//

	DrawDebugText( "Render targets cache", x0, y0, flFontSize, redColor );
	y0 += flLineOffset;

	// Number of RTs
	DrawDebugText( "Count", flCol1, y0, flFontSize, redColor );
	s_fmtStr.sprintf( "%llu", statsTracker.m_cur.m_nRTCacheCount );
	DrawDebugText( s_fmtStr.Get(), flCol2, y0, flFontSize, redColor );
	y0 += flLineOffset;

	// Size of all RTs in bytes
	DrawDebugText( "Mem", flCol1, y0, flFontSize, redColor );
	s_fmtStr.sprintf( "%4.1f MB", statsTracker.m_cur.m_nRTCacheSizeInBytes / (float)( 1024 * 1024 ) );
	DrawDebugText( s_fmtStr.Get(), flCol2, y0, flFontSize, redColor );
	y0 += flLineOffset;

	// Cache hits
	DrawDebugText( "Cache hits", flCol1, y0, flFontSize, redColor );
	s_fmtStr.sprintf( 
		"%llu (max: %llu, tot: %llu)", 
		statsTracker.m_cur.m_nRTCacheHits,
		statsTracker.m_max.m_nRTCacheHits,
		statsTracker.m_tot.m_nRTCacheHits );
	DrawDebugText( s_fmtStr.Get(), flCol2, y0, flFontSize, redColor );
	y0 += flLineOffset;

	// Cache misses
	DrawDebugText( "Cache misses", flCol1, y0, flFontSize, redColor );
	s_fmtStr.sprintf( 
		"%llu (max: %llu, tot: %llu)", 
		statsTracker.m_cur.m_nRTCacheMisses,
		statsTracker.m_max.m_nRTCacheMisses,
		statsTracker.m_tot.m_nRTCacheMisses );
	DrawDebugText( s_fmtStr.Get(), flCol2, y0, flFontSize, redColor );
	y0 += flLineOffset;

	//
	// Draw calls
	//

#if (PANDX_DRAW)
	if ( g_bPanDx )
	{
		DrawDebugText( "Number of draw calls (PanDx mode)", x0, y0, flFontSize, redColor );
		y0 += flLineOffset;

		DrawDebugText( "Total (not batched)", flCol1, y0, flFontSize, redColor );
		s_fmtStr.sprintf(
			"%llu (min: %llu, max: %llu, avg: %llu)",
			statsTracker.m_cur.m_nPanDxDrawCalls,
			statsTracker.m_min.m_nPanDxDrawCalls,
			statsTracker.m_max.m_nPanDxDrawCalls,
			statsTracker.m_avg.m_nPanDxDrawCalls );
		DrawDebugText( s_fmtStr.Get(), flCol2, y0, flFontSize, redColor );
		y0 += flLineOffset;

		DrawDebugText( "BasicQuad", flCol1, y0, flFontSize, redColor );
		s_fmtStr.sprintf(
			"%llu (min: %llu, max: %llu, avg: %llu)",
			statsTracker.m_cur.m_nPanDxBasicDrawCalls,
			statsTracker.m_min.m_nPanDxBasicDrawCalls,
			statsTracker.m_max.m_nPanDxBasicDrawCalls,
			statsTracker.m_avg.m_nPanDxBasicDrawCalls );
		DrawDebugText( s_fmtStr.Get(), flCol2, y0, flFontSize, redColor );
		y0 += flLineOffset;

		DrawDebugText( "FancyQuad", flCol1, y0, flFontSize, redColor );
		s_fmtStr.sprintf(
			"%llu (min: %llu, max: %llu, avg: %llu)",
			statsTracker.m_cur.m_nPanDxFancyDrawCalls,
			statsTracker.m_min.m_nPanDxFancyDrawCalls,
			statsTracker.m_max.m_nPanDxFancyDrawCalls,
			statsTracker.m_avg.m_nPanDxFancyDrawCalls );
		DrawDebugText( s_fmtStr.Get(), flCol2, y0, flFontSize, redColor );
		y0 += flLineOffset;
	}
	else
#endif
	{
		DrawDebugText( "Number of draw calls", x0, y0, flFontSize, redColor );
		y0 += flLineOffset;

		DrawDebugText( "BorderPanelContext", flCol1, y0, flFontSize, redColor );
		s_fmtStr.sprintf(
			"%llu (min: %llu, max: %llu, avg: %llu)",
			statsTracker.m_cur.m_nDrawBorderForPanelContextCalls,
			statsTracker.m_min.m_nDrawBorderForPanelContextCalls,
			statsTracker.m_max.m_nDrawBorderForPanelContextCalls,
			statsTracker.m_avg.m_nDrawBorderForPanelContextCalls );
		DrawDebugText( s_fmtStr.Get(), flCol2, y0, flFontSize, redColor );
		y0 += flLineOffset;

		DrawDebugText( "Border", flCol1, y0, flFontSize, redColor );
		s_fmtStr.sprintf(
			"%llu (min: %llu, max: %llu, avg: %llu)",
			statsTracker.m_cur.m_nDrawBorderCalls,
			statsTracker.m_min.m_nDrawBorderCalls,
			statsTracker.m_max.m_nDrawBorderCalls,
			statsTracker.m_avg.m_nDrawBorderCalls );
		DrawDebugText( s_fmtStr.Get(), flCol2, y0, flFontSize, redColor );
		y0 += flLineOffset;

		DrawDebugText( "TexturedQuadInternal", flCol1, y0, flFontSize, redColor );
		s_fmtStr.sprintf(
			"%llu (min: %llu, max: %llu, avg: %llu)",
			statsTracker.m_cur.m_nDrawTexturedQuadInternalCalls,
			statsTracker.m_min.m_nDrawTexturedQuadInternalCalls,
			statsTracker.m_max.m_nDrawTexturedQuadInternalCalls,
			statsTracker.m_avg.m_nDrawTexturedQuadInternalCalls );
		DrawDebugText( s_fmtStr.Get(), flCol2, y0, flFontSize, redColor );
		y0 += flLineOffset;

		DrawDebugText( "FancyQuad", flCol1, y0, flFontSize, redColor );
		s_fmtStr.sprintf(
			"%llu (min: %llu, max: %llu, avg: %llu)",
			statsTracker.m_cur.m_nDrawfancyQuadCalls,
			statsTracker.m_min.m_nDrawfancyQuadCalls,
			statsTracker.m_max.m_nDrawfancyQuadCalls,
			statsTracker.m_avg.m_nDrawfancyQuadCalls );
		DrawDebugText( s_fmtStr.Get(), flCol2, y0, flFontSize, redColor );
		y0 += flLineOffset;
	}
}


//--------------------------------------------------------------------------------------------------
// Original version of drawtext with shadow drawn every frame
//--------------------------------------------------------------------------------------------------
void CSource2Surface::DrawTextRegionOriginal( const RenderTextRegionCommand_t &renderCommand )
{
	if ( s_convarPanoramaDisableDrawText.GetBool() )
		return;

	VPROF( "CSource2Surface::DrawTextRegion" );
	//VPROF_BUDGET( "Panorama DrawTextRegion", VPROF_BUDGETGROUP_GAME );

	if ( m_stackCompositionLayers.Count() == 0 )
	{
		Assert( m_stackCompositionLayers.Count() > 0 );
		return;
	}

	CSource2CompositionLayer *pLayer = m_stackCompositionLayers[ m_stackCompositionLayers.Count() - 1 ];
	if ( pLayer->BIsDrawing() )
	{
		pLayer->ActivateRenderTarget();

		float x0, y0, x1, y1;
		x0 = renderCommand.top_left.x;
		y0 = renderCommand.top_left.y;
		x1 = renderCommand.bottom_right.x;
		y1 = renderCommand.bottom_right.y;

		// Check rect has valid area
		if ( x1 <= x0 || y1 <= y0 )
			return;

		float flLineHeight = k_flFloatNotSet;
		if ( renderCommand.line_height > 0 )
			flLineHeight = renderCommand.line_height;

		// get textures
		UITextOpacityMaskData_t *pResult = GetCachedTextOpacityMask( renderCommand.raw_text, renderCommand.raw_text_bytes, renderCommand.text_chars, renderCommand.text_encoding,
																	 x0, y0, x1, y1, flLineHeight, renderCommand.text_align, renderCommand.wrapping, renderCommand.ellipsis, renderCommand.default_format, renderCommand.range_formats );
		if ( pResult )
		{

			// draw shadow if present first using shadow color
			if ( renderCommand.text_shadow &&
				 !s_convarPanoramaDisableDrawTextShadow.GetBool() )
			{
				float flXOffset = renderCommand.text_shadow->horizontal_offset;
				float flYOffset = renderCommand.text_shadow->vertical_offset;

				// Setup shadow fill brush
				// Setup shadow fill brush. Note that we handle the memory ourselves on the stack rather than letting it allocate normally.
				FillBrushCollection_t shadowBrushCollection;
				CRenderDataListBuilder< FillBrush_t > shadowBrushCollectionBuilder( shadowBrushCollection.fill_brush, nullptr );
				CRenderDataListBuilder< FillBrush_t >::DefaultListNode_t brushListNode;
				memset( &brushListNode, 0, sizeof( brushListNode ) );
				uint32 unShadowColor = renderCommand.text_shadow->color;

				brushListNode.entry.eFillBrushType = k_EFillBrushType_Color;
				brushListNode.entry.opacity = 1.0f;
				brushListNode.entry.color_rgba = unShadowColor;
				shadowBrushCollectionBuilder.AddNodeToTail( &brushListNode );

				// Basic fast text shadows (css property text-shadow-fast) are flagged using a value of -1.0 for blur radius and strength 
				if ( s_convarPanoramaForceFastTextShadow.GetBool() || ( renderCommand.text_shadow->blur_radius < 0.0f && renderCommand.text_shadow->strength < 0.0f ) )
				{
					if ( ( flXOffset > 0.0f ) || ( flYOffset > 0.0f ) )
					{
						for ( int iTextMaskRegion = 0; iTextMaskRegion < pResult->m_cRangeData; iTextMaskRegion++ )
						{
							if ( pResult->m_pRangeData[ iTextMaskRegion ].m_hTexture )
								DrawTextRegionRange( pLayer, x0 + flXOffset, y0 + flYOffset, x1 + flXOffset, y1 + flYOffset, pResult->m_pRangeData[ iTextMaskRegion ], shadowBrushCollection );
						}
					}
				}
				else
				{
					int xPadding = clamp( ( x1 - x0 ) * 0.1, 10, 100 );
					int yPadding = clamp( ( y1 - y0 ) * 0.1, 10, 100 );

					int nWidth = x1 - x0 + ( xPadding * 2 );
					int nHeight = y1 - y0 + ( yPadding * 2 );

					// So that we don't create a ton of tiny render targets, try to round up to a reasonable size and re-use that same size for everything
					// that can fit within it
					int nVerticalRenderTargetWidth = nWidth;
					int nVerticalRenderTargetHeight = nHeight;
					if ( nWidth <= 512 && nHeight <= 64 )
					{
						nVerticalRenderTargetWidth = 512;
						nVerticalRenderTargetHeight = 64;
					}

					// Create temporary render target to draw into and blur...
					CSource2CompositionLayer *pVerticalBlurLayer = GetCompositionLayer( nVerticalRenderTargetWidth, nVerticalRenderTargetHeight, false, false, "textshadow_blur" );
					pVerticalBlurLayer->Clear();

#if PANDX_DRAW
					BasicQuad_t *pQuad = PanDxGetBasicQuadPtr();
#else
					BasicQuad_t pQuad[ 1 ];
#endif
					pQuad->BqInit( 0, 0, nWidth, nHeight );

					m_stackCompositionLayers.AddToTail( pVerticalBlurLayer );
					pVerticalBlurLayer->ActivateRenderTargetAndClear();

					float x0Shadow = xPadding + flXOffset;
					float x1Shadow = x0Shadow + ( x1 - x0 );
					float y0Shadow = yPadding + flYOffset;
					float y1Shadow = y0Shadow + ( y1 - y0 );

					Rect_t rectScissor( 0, 0, nWidth, nHeight );
					m_pRenderContext->SetScissorRect( rectScissor );

					// draw background with our shadow fillbrush
					for ( int iTextMaskRegion = 0; iTextMaskRegion < pResult->m_cRangeData; iTextMaskRegion++ )
					{
						if ( pResult->m_pRangeData[ iTextMaskRegion ].m_hTexture )
						{
							DrawTextRegionRange( pVerticalBlurLayer, x0Shadow, y0Shadow, x1Shadow, y1Shadow, pResult->m_pRangeData[ iTextMaskRegion ], shadowBrushCollection );

							float flStrength = MIN( renderCommand.text_shadow->strength, 10.0f );
							while ( flStrength > 1.0f )
							{
								float flOffset = flStrength - 1.0f;

								DrawTextRegionRange( pVerticalBlurLayer, x0Shadow + flOffset, y0Shadow, x1Shadow + flOffset, y1Shadow, pResult->m_pRangeData[ iTextMaskRegion ], shadowBrushCollection );
								DrawTextRegionRange( pVerticalBlurLayer, x0Shadow - flOffset, y0Shadow, x1Shadow - flOffset, y1Shadow, pResult->m_pRangeData[ iTextMaskRegion ], shadowBrushCollection );
								DrawTextRegionRange( pVerticalBlurLayer, x0Shadow, y0Shadow + flOffset, x1Shadow, y1Shadow + flOffset, pResult->m_pRangeData[ iTextMaskRegion ], shadowBrushCollection );
								DrawTextRegionRange( pVerticalBlurLayer, x0Shadow, y0Shadow - flOffset, x1Shadow, y1Shadow - flOffset, pResult->m_pRangeData[ iTextMaskRegion ], shadowBrushCollection );

								flStrength -= 1.0f;
							}
						}
					}

					float flBlurRadius = renderCommand.text_shadow->blur_radius;
					float flBlurPasses = 1.0f;
					const float flMaxBlurPerPass = 12.0f;
					while ( flBlurRadius > flMaxBlurPerPass )
					{
						flBlurPasses++;
						flBlurRadius -= flMaxBlurPerPass;
					}

					int nMaxSamples = 1 + Float2Int( ceil( flMaxBlurPerPass / 1.5f ) );
					float *pSampleWeights = StackAlloc( float, nMaxSamples );
					float *pSampleOffsets = StackAlloc( float, nMaxSamples );

					CRenderAttributes renderAttributes;

					HRenderTexture hBlurLayer = g_pSceneSystem->GetWellKnownRenderTarget( SCENE_RTGT_SCRATCH_TEXTURE_8888, SCENE_RTSIZE_FRAMEBUFFER, 0 );
					CTextureDesc const *dstDesc = g_pRenderDevice->GetTextureDesc( hBlurLayer );

					for ( float flBlurPass = 0.0f; flBlurPass < flBlurPasses; flBlurPass += 1.0f )
					{
						float flBlurThisPass = flBlurRadius;
						bool bLastPass = false;
						if ( flBlurPass + 1.0f < flBlurPasses )
							flBlurThisPass = flMaxBlurPerPass;
						else
							bLastPass = true;

						int nNumSamples = g_pSceneUtils->CalculateLinearWeightsForGaussianBlur( flBlurThisPass, pSampleWeights, pSampleOffsets, nMaxSamples );

						RenderTargetDesc_t rtDesc( hBlurLayer, RENDER_TEXTURE_HANDLE_INVALID, RENDER_SRGB );
						m_pRenderContext->BindRenderTargets( rtDesc );

						RenderViewport_t renderViewport;
						renderViewport.Init( 0, 0, Min( nWidth + Max( 10, Ceil2Int( flBlurRadius ) ), (int)dstDesc->m_nWidth ), Min( nHeight + Max( 10, Ceil2Int( flBlurRadius ) ), (int)dstDesc->m_nHeight ) );
						m_pRenderContext->SetViewports( 1, &renderViewport );

						const Vector4D vecClearColors[ 1 ] = { Vector4D( 0, 0, 0, 0 ) };
						m_pRenderContext->Clear( vecClearColors, 1, RENDER_CLEAR_FLAGS_CLEAR_COLOR );

						renderViewport.Init( 0, 0, nWidth, nHeight );
						m_pRenderContext->SetViewports( 1, &renderViewport );

						renderAttributes.SetIntValue( ATTR_D_TEX2DFASTBLUR, nNumSamples - 1 );
						renderAttributes.SetVMatrixValue( ATTR_MatTransform, VMatrix::GetIdentityMatrix() );
						renderAttributes.SetFloatValue( ATTR_ViewportWidth, (float)nWidth );
						renderAttributes.SetFloatValue( ATTR_ViewportHeight, (float)nHeight );
						renderAttributes.SetFloatValue( ATTR_centerWeight, pSampleWeights[ 0 ] );

						float flInvWidth = ( 1.0f / (float)nVerticalRenderTargetWidth ) * pVerticalBlurLayer->GetRTOriginalWidthScale();
						for ( int j = 1; j < nNumSamples; ++j )
						{
							renderAttributes.SetVector4DValue( RenderAttrVector4D_t( ATTR_sample0 + j ), Vector4D( pSampleOffsets[ j ] * flInvWidth, 0.0f, pSampleWeights[ j ], 0.0f ) );
						}

						pQuad->m_vUV[ 1 ].x = ( (float)nWidth / (float)nVerticalRenderTargetWidth ) * pVerticalBlurLayer->GetRTOriginalWidthScale();
						pQuad->m_vUV[ 2 ].x = ( (float)nWidth / (float)nVerticalRenderTargetWidth ) * pVerticalBlurLayer->GetRTOriginalWidthScale();
						pQuad->m_vUV[ 2 ].y = ( (float)nHeight / (float)nVerticalRenderTargetHeight )* pVerticalBlurLayer->GetRTOriginalHeightScale();
						pQuad->m_vUV[ 3 ].y = ( (float)nHeight / (float)nVerticalRenderTargetHeight )* pVerticalBlurLayer->GetRTOriginalHeightScale();

#if PANDX_DRAW
						BasicQuad_t *pQuadLpH = PanDxGetBasicQuadPtr();
#else
						BasicQuad_t pQuadLpH[ 1 ];
#endif
						memcpy( pQuadLpH, pQuad, sizeof( BasicQuad_t ) );

						// Draw the current layer into horizontal blur surface
						DrawTexturedQuadInternal( m_hMaterial, &renderAttributes, pVerticalBlurLayer->GetTextureHandle(), pQuadLpH );

						if ( !bLastPass )
						{
							RenderTargetDesc_t rtVerDesc( pVerticalBlurLayer->GetTextureHandle(), RENDER_TEXTURE_HANDLE_INVALID, RENDER_SRGB );
							m_pRenderContext->BindRenderTargets( rtVerDesc );

							renderViewport.Init( 0, 0, Min( nWidth + Max( 10, Ceil2Int( flBlurRadius ) ), (int)nVerticalRenderTargetWidth ), Min( nHeight + Max( 10, Ceil2Int( flBlurRadius ) ), (int)nVerticalRenderTargetHeight ) );
							m_pRenderContext->SetViewports( 1, &renderViewport );

							m_pRenderContext->Clear( vecClearColors, 1, RENDER_CLEAR_FLAGS_CLEAR_COLOR );

							renderViewport.Init( 0, 0, nWidth, nHeight );
							m_pRenderContext->SetViewports( 1, &renderViewport );

							renderAttributes.SetIntValue( ATTR_D_TEX2DFASTBLUR, nNumSamples - 1 );

							renderAttributes.SetVMatrixValue( ATTR_MatTransform, VMatrix::GetIdentityMatrix() );
							renderAttributes.SetFloatValue( ATTR_ViewportWidth, (float)nWidth );
							renderAttributes.SetFloatValue( ATTR_ViewportHeight, (float)nHeight );
							renderAttributes.SetFloatValue( ATTR_centerWeight, pSampleWeights[ 0 ] );

							float flInvHeight = 1.0f / dstDesc->m_nHeight;
							for ( int j = 1; j < nNumSamples; ++j )
							{
								renderAttributes.SetVector4DValue( RenderAttrVector4D_t( ATTR_sample0 + j ), Vector4D( 0.0f, pSampleOffsets[ j ] * flInvHeight, pSampleWeights[ j ], 0.0f ) );
							}

							pQuad->m_vUV[ 1 ].x = ( (float)nWidth / (float)dstDesc->m_nWidth );
							pQuad->m_vUV[ 2 ].x = ( (float)nWidth / (float)dstDesc->m_nWidth );
							pQuad->m_vUV[ 2 ].y = ( (float)nHeight / (float)dstDesc->m_nHeight );
							pQuad->m_vUV[ 3 ].y = ( (float)nHeight / (float)dstDesc->m_nHeight );

#if PANDX_DRAW
							BasicQuad_t *pQuadLpV = PanDxGetBasicQuadPtr();
#else
							BasicQuad_t pQuadLpV[ 1 ];
#endif
							memcpy( pQuadLpV, pQuad, sizeof( BasicQuad_t ) );

							// now draw back in using vertical blur
							DrawTexturedQuadInternal( m_hMaterial, &renderAttributes, hBlurLayer, pQuadLpV );
						}
					}

					// Add blur layer back to free layers list
					m_stackCompositionLayers.Remove( m_stackCompositionLayers.Count() - 1 );

					pQuad->m_vPosition[ 0 ].x = x0 - xPadding;
					pQuad->m_vPosition[ 0 ].y = y0 - yPadding;
					pQuad->m_vPosition[ 1 ].x = x1 + xPadding;
					pQuad->m_vPosition[ 1 ].y = y0 - yPadding;
					pQuad->m_vPosition[ 2 ].x = x1 + xPadding;
					pQuad->m_vPosition[ 2 ].y = y1 + yPadding;
					pQuad->m_vPosition[ 3 ].x = x0 - xPadding;
					pQuad->m_vPosition[ 3 ].y = y1 + yPadding;

					pLayer->ActivateRenderTarget();


					RectBounds_t r;
					pLayer->GetCurrentClipRect( r );
					Rect_t rScissor( RoundFloatToInt( r.left ), RoundFloatToInt( r.top ), RoundFloatToInt( r.right - r.left ), RoundFloatToInt( r.bottom - r.top ) );
					if ( rScissor.x < 0 )
					{
						rScissor.width += rScissor.x;
						rScissor.x = 0;
					}

					if ( rScissor.y < 0 )
					{
						rScissor.height += rScissor.y;
						rScissor.y = 0;
					}

					m_pRenderContext->SetScissorRect( rScissor );

					// now draw back in using vertical blur
					if ( flBlurRadius > 0.0f )
					{
						int nNumSamples = g_pSceneUtils->CalculateLinearWeightsForGaussianBlur( flBlurRadius, pSampleWeights, pSampleOffsets, nMaxSamples );

						renderAttributes.SetIntValue( ATTR_D_TEX2DFASTBLUR, nNumSamples - 1 );
						VMatrix *pMatrix = pLayer->AccessPushedMatrix();
						renderAttributes.SetVMatrixValue( ATTR_MatTransform, pMatrix ? pMatrix->Transpose() : VMatrix::GetIdentityMatrix() );
						renderAttributes.SetFloatValue( ATTR_ViewportWidth, (float)pLayer->GetWidth() );
						renderAttributes.SetFloatValue( ATTR_ViewportHeight, (float)pLayer->GetHeight() );
						renderAttributes.SetFloatValue( ATTR_centerWeight, pSampleWeights[ 0 ] );

						float flInvHeight = 1.0f / dstDesc->m_nHeight;
						for ( int i = 1; i < nNumSamples; ++i )
						{
							renderAttributes.SetVector4DValue( RenderAttrVector4D_t( ATTR_sample0 + i ), Vector4D( 0.0f, pSampleOffsets[ i ] * flInvHeight, pSampleWeights[ i ], 0.0f ) );
						}

						pQuad->m_vUV[ 0 ].x = 0.0f;
						pQuad->m_vUV[ 0 ].y = 0.0f;
						pQuad->m_vUV[ 1 ].x = ( (float)nWidth / (float)dstDesc->m_nWidth );
						pQuad->m_vUV[ 1 ].y = 0.0f;
						pQuad->m_vUV[ 2 ].x = ( (float)nWidth / (float)dstDesc->m_nWidth );
						pQuad->m_vUV[ 2 ].y = ( (float)nHeight / (float)dstDesc->m_nHeight );
						pQuad->m_vUV[ 3 ].x = 0.0f;
						pQuad->m_vUV[ 3 ].y = ( (float)nHeight / (float)dstDesc->m_nHeight );

						DrawTexturedQuadInternal( m_hMaterial, &renderAttributes, hBlurLayer, pQuad );
					}
					else
					{
						renderAttributes.SetIntValue( ATTR_D_TEX2DBLUR, 1 );
						VMatrix *pMatrix = pLayer->AccessPushedMatrix();
						renderAttributes.SetVMatrixValue( ATTR_MatTransform, pMatrix ? pMatrix->Transpose() : VMatrix::GetIdentityMatrix() );
						renderAttributes.SetFloatValue( ATTR_BlurSigma, 0.0f );
						renderAttributes.SetFloatValue( ATTR_ViewportWidth, (float)pLayer->GetWidth() );
						renderAttributes.SetFloatValue( ATTR_ViewportHeight, (float)pLayer->GetHeight() );

						pQuad->m_vUV[ 0 ].x = 0.0f;
						pQuad->m_vUV[ 0 ].y = 0.0f;
						pQuad->m_vUV[ 1 ].x = 1.0f * pVerticalBlurLayer->GetRTOriginalWidthScale();
						pQuad->m_vUV[ 1 ].y = 0.0f;
						pQuad->m_vUV[ 2 ].x = 1.0f * pVerticalBlurLayer->GetRTOriginalWidthScale();
						pQuad->m_vUV[ 2 ].y = 1.0f * pVerticalBlurLayer->GetRTOriginalHeightScale();
						pQuad->m_vUV[ 3 ].x = 0.0f;
						pQuad->m_vUV[ 3 ].y = 1.0f * pVerticalBlurLayer->GetRTOriginalHeightScale();

						DrawTexturedQuadInternal( m_hMaterial, &renderAttributes, pVerticalBlurLayer->GetTextureHandle(), pQuad );
					}

					m_FreeLayers.Insert( pVerticalBlurLayer, pVerticalBlurLayer, m_flCurrentRenderFrameTime );

				}
			}


			// draw foreground with actual fillbrush
			for ( int iTextMaskRegion = 0; iTextMaskRegion < pResult->m_cRangeData; iTextMaskRegion++ )
			{
				const FillBrushCollection_t *pFillBrushCollection = &renderCommand.default_format.fill_brush_collection;

				int iColorRangeFormat = pResult->m_pRangeData[ iTextMaskRegion ].m_iColorIndex;
				if ( iColorRangeFormat != UITextOpacityMaskDataRange_t::k_iColorIndexUnset )
				{
					int i = 0;
					for ( const RenderTextRangeFormat_t *pRangeFormat : renderCommand.range_formats )
					{
						if ( i == iColorRangeFormat )
						{
							pFillBrushCollection = &pRangeFormat->format.fill_brush_collection;
							break;
						}
						++i;
					}
				}

				if ( pResult->m_pRangeData[ iTextMaskRegion ].m_hTexture )
					DrawTextRegionRange( pLayer, x0, y0, x1, y1, pResult->m_pRangeData[ iTextMaskRegion ], *pFillBrushCollection );
			}
		}
	}
}

#if PANDX_DRAW

//--------------------------------------------------------------------------------------------------
// PanDx -- Experiment : Draw panorama directly with DX rather than via matsys.
// Matsys etc.. is not ideal for panorama's quad at a time drawing
//--------------------------------------------------------------------------------------------------

#include "materialsystem/IShader.h"

extern CPanoramaUIEngine *g_pPanoramaUIEngineImpl;

void PanDxTermShaders();

#ifdef DX_TO_GL_ABSTRACTION
void PanDxCaptureGlState();
void PanDxRestoreGlState();
#endif

// fancy..
IDirect3DVertexDeclaration9* g_pVertexDeclFancy = NULL;
D3DVERTEXELEMENT9 g_VertexElemFancy[]
{
	{ 0, 0,      D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_POSITION,  0 },
	{ 0, 4 * 4,  D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD,  0 },
	{ 0, 8 * 4,	 D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD,  1 },
	{ 0, 12 * 4, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD,  2 },
	{ 0, 16 * 4, D3DDECLTYPE_FLOAT4,  D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD,  3 },
	{ 0, 20 * 4, D3DDECLTYPE_FLOAT4,  D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD,  4 },
	D3DDECL_END()
};
const int k_nFancyVertSize = 24 * 4;

// basic...
IDirect3DVertexDeclaration9* g_pVertexDeclBasic = NULL;
D3DVERTEXELEMENT9 g_VertexElemBasic[]
{
	{ 0, 0,      D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_POSITION,  0 },
	{ 0, 4 * 4,  D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD,  0 },
	{ 0, 8 * 4,	 D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD,  1 },
	{ 0, 12 * 4, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD,  2 },
	D3DDECL_END()
};
const uint32 k_nBasicVertSize = 16 * 4;

const uint32 k_nVertAlignment = 48 * 4;	// common alignment for basic and fancy verts

// VBs

#define VB_NUM_VERTS 4096
#define VB_NUM_BUFFERS 4	// PERF NOTE - take care when changing this!!
							// we noticed that on some HW, with 1 or 2 buffers, we should lock with the discard flag when full, whereas
							// with more than 2 buffers, it is much better to lock with discard the first time we lock in a frame
							// for reference Source1 dynamic VB's lock with the latter pattern.

// Use this convar to allow testing of above perf note, currently set to 1 to match > 2 buffers as mentioned above
// Note that on some HW we get slightly better (and less spikey) perf with this convar set to 0, but for the HW where 1 is better, perf with 0 is considerably worse, so for now we check in with it set to 1
ConVar s_convarPanoramaFlushBuffers( "@panorama_flush_buffers", "1", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );

const uint32 k_nVBNumVert = VB_NUM_VERTS;
const uint32 k_nVBSize = k_nFancyVertSize * k_nVBNumVert; // worst case fancy verts
const uint32 k_nIBSize = 2 * (k_nVBSize / k_nBasicVertSize); // worst case basic (smallest) verts
IDirect3DVertexBuffer9* g_pVB[ VB_NUM_BUFFERS ];
uint32					g_nVBBuffer = 0;	// current buffer in use
uint32					g_nVBUsed = 0;		// num bytes written to current buffer

bool g_bVBLocked = false;
bool g_bVBFlush = false;

#if defined ( DX_TO_GL_ABSTRACTION)
IDirect3DIndexBuffer9* g_pIB = nullptr;
#endif

// current ptrs into VB
unsigned char* g_pPanDxVB = nullptr;

#ifndef DX_TO_GL_ABSTRACTION
IDirect3DStateBlock9 *g_pPanDxStateBlock = 0;
#endif

void* g_PanDxBasicVsShaders[ 1 ];
void* g_PanDxBasicPsShaders[ 640 * 2 ];
void* g_PanDxFancyVsShaders[ 1 ];
void* g_PanDxFancyPsShaders[ 1280 * 2 + 1];

// Pandx own/disown/init

IDirect3DSurface9* gpBackBuffer = NULL;
IDirect3DSurface9* gpDepthStencil = NULL;

RECT g_PanSaveScissorRect;
D3DVIEWPORT9 g_PanSaveViewPort;

//
// basic shadow state
//

#define PANDX_MAX_PAN_QUADS	 4		// max panorama quads per PanDxDrawFancyQuad call

#define PANDX_MAX_SAMPLERS   4		// max 4 samplers in fancy, 1 in basic
#define PANDX_MAX_PS_CONSTS  13		// max 13 PS consts used for basic, 7 for fancy
#define PANDX_MAX_RSBATCH	 32		// max number of drawquad calls render state we batch

#define PANDX_RSBATCHTYPE_BASIC			(1<<0)
#define PANDX_RSBATCHTYPE_FANCY			(1<<1)
#define PANDX_RSBATCHTYPE_CLEAR			(1<<2)
#define PANDX_RSBATCHTYPE_VP			(1<<3)
#define PANDX_RSBATCHTYPE_SCISSOR		(1<<4)
#define PANDX_RSBATCHTYPE_SCISSORENABLE	(1<<5)
#define PANDX_RSBATCHTYPE_RT			(1<<6)

#define PANDX_RSBATCHYPE_DRAW			( PANDX_RSBATCHTYPE_BASIC | PANDX_RSBATCHTYPE_FANCY )

// since we only set three sampler states, we need to map to D3D sampler state when setting on the device
enum ePanDxSamplerStateType
{
	PANDXSAMP_ADDRESSU,			// maps to D3DSAMP_ADDRESSU	
	PANDXSAMP_ADDRESSV,			// maps to D3DSAMP_ADDRESSV
	PANDXSAMP_SRGBTEXTURE,		// maps to D3DSAMP_SRGBTEXTURE

	PANDX_MAX_SAMPLER_STATES = 3
};

D3DSAMPLERSTATETYPE g_aMapPanDxSamplerStateTypeToD3D[ PANDX_MAX_SAMPLER_STATES ] = { D3DSAMP_ADDRESSU, D3DSAMP_ADDRESSV, D3DSAMP_SRGBTEXTURE };

// Structure of all render state we set in PanDx for a draw call
ALIGN128 struct sPanDxRenderState
{
	// pixel shader consts
	// put this first to ensure cache line alignment
	float m_aPanDxPSConsts[ PANDX_MAX_PS_CONSTS * 4 ];

	uint32 m_nFlags;

	// Vertex and Pixel Shader ptrs
	void *m_pVS;
	void *m_pPS;

	// vertex declaration
	IDirect3DVertexDeclaration9* m_pVertexDecl;

	// vertex data
	IDirect3DVertexBuffer9* m_pVB;		
	int m_nVertexSize;
	int m_nVBIndex;
	int m_nNumQuads;
	void *m_pQuad;

	// samplers
	IDirect3DBaseTexture9* m_aSamplers[ PANDX_MAX_SAMPLERS ];
#ifdef PANDX_TRACK_BATCHING
	bool m_aSamplersIsRenderTarget[ PANDX_MAX_SAMPLERS ];
#endif

	// sampler states
	DWORD m_aSamplerStates[ PANDX_MAX_SAMPLERS ][ 3 ];

	// blend state
	RsBlendStateHandle_t m_blendState;

	// viewport
	D3DVIEWPORT9 m_vp;

	// scissor
	RECT m_scissor;
	bool m_bScissorEnabled;

	// clear
 	DWORD m_clearMask;
 	D3DCOLOR m_clearColor;

	// render target
	IDirect3DTexture9* m_pRT;

} ALIGN128_POST;

// shadow render state (current state set on DX device)
sPanDxRenderState g_PanDxShadowRenderState;

// render state storage for batching draw calls
sPanDxRenderState g_aPanDxBatchedRenderState[ PANDX_MAX_RSBATCH ];
uint32 g_nPanDxBatchIdx = 0;
uint32 g_nPanDxCurrentBatchType = 0;

// for tri list (drawcall) 
int g_nTriListStartIndex = 0;
int g_nTriListTriCount = 0;

// local copy used by GetViewport
D3DVIEWPORT9 g_PanDxLastViewport;

// saved sampler state and srgb render state - only used for own/disown DX
// 13 == D3DSAMP_DMAPOFFSET, which is missing from the togl headers, but is 13, d3d9 #samplerstate types is not changing.
#ifdef DX_TO_GL_ABSTRACTION
DWORD g_aPanDxSavedSamplerState[ 16 + 4 ][ 13 + 1 ]; // for save/restore on OwnDX/DisownDX, VS as well as PS samplers on GL saved (due to potential overlapping sampler usage) here, hence MAX (16 + 4) samplers
#else
DWORD g_aPanDxSavedSamplerState[ 16 ][ 13 + 1 ]; // for save/restore on OwnDX/DisownDX, only PS samplers saved on DX hence MAX (16) samplers
#endif
DWORD g_srgbRenderState;

void PanDxCommitCommonRS( IDirect3DDevice9* pDev );

void PanDxInitSamplerState( IDirect3DDevice9* pDev, int nSampler, ePanDxSamplerStateType ssType, DWORD val );
void PanDxSetSamplerState( IDirect3DDevice9* pDev, int nSampler, ePanDxSamplerStateType ssType, DWORD val );

void PanDxCommitSampler( IDirect3DDevice9* pDev, int nSampler, IDirect3DBaseTexture9* pD3DTex );

void PanDxCommitVertexDecl( IDirect3DDevice9* pDev, IDirect3DVertexDeclaration9* pVertexDecl );
void PanDxCommitStreamSource( IDirect3DDevice9* pDev, IDirect3DVertexBuffer9* pVB, int nVertexSize );

void PanDxInvalidateRT();
void PanDxInvalidateViewportAndScissor();
void PanDxFlushBasicQuads();
void PanDxFlushFancyQuads();
void PanDxFlushQuads();
void PanDxResetBatching();
void PanDxResetVBLocks();
void PanDxResetTriList();

uint32 GetPanDxAccumulatedBatchFlags();
bool IsPanDxBatchEmpty();

//--------------------------------------------------------------------------------------------------
// FancyQuad_t, BasicQuad_t scratch mem (ring buffer)
//
// Usage pattern
//	* Call GetBasic/FancyQuadPtr to grab the next piece of usable mem from the ring buffer as opposed to just allocating locally/via stack.
//  * Fill Basic/Fancy Quad struct
//  * Call DrawBasic/FancyQuad using the ptr, as opposed to a local copy.
//
// Use these GetBasic/Fancy fns to grab some memory to fill Basic/Fancy quad struct. DO NOT USE stack versions - this allows us to optimize for VB lock/fill, and reduce mem copies in the process
// This pattern breaks if you call GetBasic/FancyQuadPtr outside a loop and modify the quad contents inside differently for each iteration, calling DrawBasic/Fancy each time. 
// The assumption is that the contents remain unchanged for one batch. 
//--------------------------------------------------------------------------------------------------
panorama::FancyQuad_t g_PanDxFancyQuadScratch[ PANDX_MAX_RSBATCH * PANDX_MAX_PAN_QUADS ];	// worst case, scratch mem for one entire batch, each entry having four quads
uint32 g_nPanDxFancyScratchIndex = 0;
panorama::BasicQuad_t g_PanDxBasicQuadScratch[ PANDX_MAX_RSBATCH ];		// 
uint32 g_nPanDxBasicScratchIndex = 0;

panorama::FancyQuad_t *PanDxGetFancyQuadPtr( int nNumFancyQuads )
{
	if ( ( g_nPanDxFancyScratchIndex + nNumFancyQuads ) > ( PANDX_MAX_RSBATCH * PANDX_MAX_PAN_QUADS ) )
	{
		// start over
		g_nPanDxFancyScratchIndex = 0;
	}

	panorama::FancyQuad_t *pFQ = &g_PanDxFancyQuadScratch[ g_nPanDxFancyScratchIndex ];

	g_nPanDxFancyScratchIndex += nNumFancyQuads;

	return pFQ;
}

panorama::BasicQuad_t *PanDxGetBasicQuadPtr()
{
	if ( ( g_nPanDxBasicScratchIndex + 1 ) > ( PANDX_MAX_RSBATCH ) )
	{
		g_nPanDxBasicScratchIndex = 0;
	}

	panorama::BasicQuad_t *pBQ = &g_PanDxBasicQuadScratch[ g_nPanDxBasicScratchIndex ];

	g_nPanDxBasicScratchIndex++;

	return pBQ;
}

//--------------------------------------------------------------------------------------------------
// Init and Term
//--------------------------------------------------------------------------------------------------

void PanDxInit()
{
	if ( g_pdxInit ) return;

#if defined( ALLOW_TEXT_MODE )
	static const bool cbTextMode = CommandLine()->HasParm( "-textmode" );
#else
	static const bool cbTextMode = false;
#endif
	g_bPanDx = !cbTextMode;
	s_convarPanDx.SetValue( !cbTextMode );

	if ( !g_bPanDx )
		return;

	//PRB: Error check
	IDirect3DDevice9* pDev = g_pPanoramaUIEngineImpl->GetD3Device();
	pDev->CreateVertexDeclaration( g_VertexElemFancy, &g_pVertexDeclFancy );
	pDev->CreateVertexDeclaration( g_VertexElemBasic, &g_pVertexDeclBasic );

	for ( int i = 0; i < VB_NUM_BUFFERS; i++ )
	{
		pDev->CreateVertexBuffer( k_nVBSize, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &g_pVB[ i ], NULL );
	}

	g_bVBLocked = false;
	g_nVBBuffer = 0;
	g_nVBUsed   = 0;

	g_nPanDxFancyScratchIndex = 0;
	g_nPanDxBasicScratchIndex = 0;

	memset( g_PanDxBasicVsShaders, 0, sizeof( g_PanDxBasicVsShaders ) );
	memset( g_PanDxBasicPsShaders, 0, sizeof( g_PanDxBasicPsShaders ) );
	memset( g_PanDxFancyVsShaders, 0, sizeof( g_PanDxFancyVsShaders ) );
	memset( g_PanDxFancyPsShaders, 0, sizeof( g_PanDxFancyPsShaders ) );

	memset( g_PanDxFancyQuadScratch, 0, sizeof( g_PanDxFancyQuadScratch ) );
	memset( g_PanDxBasicQuadScratch, 0, sizeof( g_PanDxBasicQuadScratch ) );


#if defined ( DX_TO_GL_ABSTRACTION )
	// Create Index Buffer
	// Index type of SHORT is all that togl accepts, and the define is missing (it's 101 but that is not used)
	pDev->CreateIndexBuffer( k_nIBSize, D3DUSAGE_WRITEONLY, (D3DFORMAT)101 /*D3DFMT_INDEX16*/, D3DPOOL_DEFAULT, &g_pIB, NULL );
	uint16 *pIndices;
	g_pIB->Lock( 0, k_nIBSize, (void**)&pIndices, 0 );

	for ( int i = 0; i < k_nIBSize/2; i++)
	{
		pIndices[ i ] = i;
	}
	g_pIB->Unlock();
#endif

	g_pdxInit = true;
}

void PanDxTerm()
{
	if ( !g_pdxInit )
	{
		return;
	}

#if defined ( DX_TO_GL_ABSTRACTION )
	g_pIB->Release();
#endif

	g_pVertexDeclFancy->Release();
	g_pVertexDeclBasic->Release();

	for ( int i = 0; i < VB_NUM_BUFFERS; i++ )
	{
		g_pVB[ i ]->Release();
	}

#ifndef DX_TO_GL_ABSTRACTION
	if ( g_pPanDxStateBlock )
	{
		g_pPanDxStateBlock->Release();
		g_pPanDxStateBlock = NULL;
	}
#endif

	PanDxResetVBLocks();
	g_nVBBuffer = 0;
	g_nVBUsed   = 0;

	PanDxResetBatching();

	PanDxResetTriList();

	g_pdxInit = false;
}

void PanDxDeviceLost()
{
	if ( !g_pdxInit )
	{
		return;
	}

	g_pVertexDeclFancy->Release();
	g_pVertexDeclBasic->Release();

#if defined ( DX_TO_GL_ABSTRACTION )
	g_pIB->Release();
#endif

	for ( int i = 0; i < VB_NUM_BUFFERS; i++ )
	{
		g_pVB[ i ]->Release();
	}

#ifndef DX_TO_GL_ABSTRACTION
	if ( g_pPanDxStateBlock )
	{
		g_pPanDxStateBlock->Release();
		g_pPanDxStateBlock = NULL;
	}
#endif
	
	PanDxResetVBLocks();
	g_nVBBuffer = 0;
	g_nVBUsed = 0;

	PanDxResetBatching();

	PanDxResetTriList();

	g_pdxInit = false;
}

void PanDxDeviceReset()
{
	PanDxInit();
}

static bool g_bInsideOwnDx = false;

bool IsPanDxInsideOwnDx()
{
	return g_bInsideOwnDx;
}

void PanDxOwnDx()
{ 
	// Checking for devicce lost (ie CShaderDeviceDx8::IsDeactivated()  exposed via CanDownloadTextures() on the material system)
	// (Note that it is possible for the device to be marked as lost (calling CShaderDeviceDx8::MarkDeviceLost)
	// before getting the "device lost" callback (done in CShaderDeviceDx8::CheckDeviceLost) )
	if ( !g_pMaterialSystem->CanDownloadTextures() )
	{
		PanDxDeviceLost();
	}
	
	if ( !g_pdxInit ) return;

	g_bInsideOwnDx = true;

	IDirect3DDevice9* pDev = g_pPanoramaUIEngineImpl->GetD3Device();

	pDev->GetRenderTarget( 0, &gpBackBuffer );
	pDev->GetDepthStencilSurface( &gpDepthStencil );

#ifndef DX_TO_GL_ABSTRACTION
	if ( !g_pPanDxStateBlock )
	{
		pDev->CreateStateBlock( D3DSBT_ALL, &g_pPanDxStateBlock );
	}

	if ( g_pPanDxStateBlock )
	{
		g_pPanDxStateBlock->Capture();		// capture whilst we own the device
	}
	// SF does not touch SRGB settings so make sure they are consistent
	pDev->GetRenderState( D3DRS_SRGBWRITEENABLE, &g_srgbRenderState );
	pDev->SetRenderState( D3DRS_SRGBWRITEENABLE, FALSE );
#else
	PanDxCaptureGlState();
	pDev->SetRenderState( D3DRS_SRGBWRITEENABLE, FALSE );
#endif

	// save off and init samplers to well known states
#ifndef DX_TO_GL_ABSTRACTION
	for ( int i = 0; i < 16; i++ )
#else
	for ( int i = 0; i < pDev->GetTotalSamplerCount(); i++ )
#endif
	{
#ifndef DX_TO_GL_ABSTRACTION
		pDev->GetSamplerState( i, D3DSAMP_SRGBTEXTURE, &g_aPanDxSavedSamplerState[ i ][ D3DSAMP_SRGBTEXTURE ] );
		pDev->GetSamplerState( i, D3DSAMP_ADDRESSU, &g_aPanDxSavedSamplerState[ i ][ D3DSAMP_ADDRESSU ] );
		pDev->GetSamplerState( i, D3DSAMP_ADDRESSV, &g_aPanDxSavedSamplerState[ i ][ D3DSAMP_ADDRESSV ] );

		// these sampler states do not change
		pDev->GetSamplerState( i, D3DSAMP_MINFILTER, &g_aPanDxSavedSamplerState[ i ][ D3DSAMP_MINFILTER ] );
		pDev->GetSamplerState( i, D3DSAMP_MAGFILTER, &g_aPanDxSavedSamplerState[ i ][ D3DSAMP_MAGFILTER ] );
		pDev->GetSamplerState( i, D3DSAMP_MIPFILTER, &g_aPanDxSavedSamplerState[ i ][ D3DSAMP_MIPFILTER ] );
#endif
		pDev->SetSamplerState( i, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
		pDev->SetSamplerState( i, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
		pDev->SetSamplerState( i, D3DSAMP_MIPFILTER, D3DTEXF_NONE );

		PanDxCommitSampler( pDev, i, NULL );
	}

	// init PanDx sampler states
	for ( int i = 0; i < PANDX_MAX_SAMPLERS; i++ )
	{
		PanDxInitSamplerState( pDev, i, PANDXSAMP_SRGBTEXTURE, FALSE );
		PanDxInitSamplerState( pDev, i, PANDXSAMP_ADDRESSU, D3DTADDRESS_CLAMP ); 
		PanDxInitSamplerState( pDev, i, PANDXSAMP_ADDRESSV, D3DTADDRESS_CLAMP );
	}

	// Save scissorrect and viewport
	pDev->GetScissorRect( &g_PanSaveScissorRect );
	pDev->GetViewport( &g_PanSaveViewPort );

	// Now Reset renderstates

	float zero = 0.0f;
	float one = 1.0f;
	DWORD dZero = *((DWORD*)(&zero));
	DWORD dOne = *((DWORD*)(&one));

	float sixtyFour = 64.0f;

	pDev->SetRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );
	pDev->SetRenderState( D3DRS_SHADEMODE, D3DSHADE_GOURAUD );
	pDev->SetRenderState( D3DRS_LASTPIXEL, TRUE );
	pDev->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
	pDev->SetRenderState( D3DRS_DITHERENABLE, FALSE );
	pDev->SetRenderState( D3DRS_FOGENABLE, FALSE );
	pDev->SetRenderState( D3DRS_SPECULARENABLE, FALSE );
	pDev->SetRenderState( D3DRS_FOGCOLOR, 0 );
	pDev->SetRenderState( D3DRS_FOGTABLEMODE, D3DFOG_NONE );
	pDev->SetRenderState( D3DRS_FOGSTART, dZero );
	pDev->SetRenderState( D3DRS_FOGEND, dOne );
	pDev->SetRenderState( D3DRS_FOGDENSITY, dZero );
	pDev->SetRenderState( D3DRS_RANGEFOGENABLE, FALSE );
	pDev->SetRenderState( D3DRS_STENCILENABLE, FALSE );
	pDev->SetRenderState( D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP );
	pDev->SetRenderState( D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP );
	pDev->SetRenderState( D3DRS_STENCILPASS, D3DSTENCILOP_KEEP );
	pDev->SetRenderState( D3DRS_STENCILFUNC, D3DCMP_ALWAYS );
	pDev->SetRenderState( D3DRS_STENCILREF, 0 );
	pDev->SetRenderState( D3DRS_STENCILMASK, 0xFFFFFFFF );
	pDev->SetRenderState( D3DRS_STENCILWRITEMASK, 0xFFFFFFFF );
	pDev->SetRenderState( D3DRS_TEXTUREFACTOR, 0xFFFFFFFF );
	pDev->SetRenderState( D3DRS_WRAP0, 0 );
	pDev->SetRenderState( D3DRS_WRAP1, 0 );
	pDev->SetRenderState( D3DRS_WRAP2, 0 );
	pDev->SetRenderState( D3DRS_WRAP3, 0 );
	pDev->SetRenderState( D3DRS_WRAP4, 0 );
	pDev->SetRenderState( D3DRS_WRAP5, 0 );
	pDev->SetRenderState( D3DRS_WRAP6, 0 );
	pDev->SetRenderState( D3DRS_WRAP7, 0 );
	pDev->SetRenderState( D3DRS_CLIPPING, TRUE );
	pDev->SetRenderState( D3DRS_LIGHTING, TRUE );
	pDev->SetRenderState( D3DRS_AMBIENT, 0 );
	pDev->SetRenderState( D3DRS_FOGVERTEXMODE, D3DFOG_NONE );
	pDev->SetRenderState( D3DRS_COLORVERTEX, TRUE );
	pDev->SetRenderState( D3DRS_LOCALVIEWER, TRUE );
	pDev->SetRenderState( D3DRS_NORMALIZENORMALS, FALSE );
	pDev->SetRenderState( D3DRS_SPECULARMATERIALSOURCE, D3DMCS_COLOR2 );
	pDev->SetRenderState( D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_MATERIAL );
	pDev->SetRenderState( D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_MATERIAL );
	pDev->SetRenderState( D3DRS_VERTEXBLEND, D3DVBF_DISABLE );
	pDev->SetRenderState( D3DRS_CLIPPLANEENABLE, 0 );
	pDev->SetRenderState( D3DRS_POINTSIZE, dOne );
	pDev->SetRenderState( D3DRS_POINTSIZE_MIN, dOne );
	pDev->SetRenderState( D3DRS_POINTSPRITEENABLE, FALSE );
	pDev->SetRenderState( D3DRS_POINTSCALEENABLE, FALSE );
	pDev->SetRenderState( D3DRS_POINTSCALE_A, dOne );
	pDev->SetRenderState( D3DRS_POINTSCALE_B, dZero );
	pDev->SetRenderState( D3DRS_POINTSCALE_C, dZero );
	pDev->SetRenderState( D3DRS_MULTISAMPLEANTIALIAS, TRUE );
	pDev->SetRenderState( D3DRS_MULTISAMPLEMASK, 0xFFFFFFFF );
	pDev->SetRenderState( D3DRS_PATCHEDGESTYLE, D3DPATCHEDGE_DISCRETE );
	pDev->SetRenderState( D3DRS_DEBUGMONITORTOKEN, D3DDMT_ENABLE );
	pDev->SetRenderState( D3DRS_POINTSIZE_MAX, *((DWORD*)(&sixtyFour)) );
	pDev->SetRenderState( D3DRS_INDEXEDVERTEXBLENDENABLE, FALSE );
	pDev->SetRenderState( D3DRS_TWEENFACTOR, dZero );
	pDev->SetRenderState( D3DRS_POSITIONDEGREE, D3DDEGREE_CUBIC );
	pDev->SetRenderState( D3DRS_NORMALDEGREE, D3DDEGREE_LINEAR );
	pDev->SetRenderState( D3DRS_SCISSORTESTENABLE, FALSE );
	pDev->SetRenderState( D3DRS_SLOPESCALEDEPTHBIAS, dZero );
	pDev->SetRenderState( D3DRS_ANTIALIASEDLINEENABLE, FALSE );
	pDev->SetRenderState( D3DRS_MINTESSELLATIONLEVEL, dOne );
	pDev->SetRenderState( D3DRS_MAXTESSELLATIONLEVEL, dOne );
	pDev->SetRenderState( D3DRS_ADAPTIVETESS_X, dZero );
	pDev->SetRenderState( D3DRS_ADAPTIVETESS_Y, dZero );
	pDev->SetRenderState( D3DRS_ADAPTIVETESS_Z, dOne );
	pDev->SetRenderState( D3DRS_ADAPTIVETESS_W, dZero );
	pDev->SetRenderState( D3DRS_ENABLEADAPTIVETESSELLATION, FALSE );
	pDev->SetRenderState( D3DRS_TWOSIDEDSTENCILMODE, FALSE );
	pDev->SetRenderState( D3DRS_CCW_STENCILFAIL, D3DSTENCILOP_KEEP );
	pDev->SetRenderState( D3DRS_CCW_STENCILZFAIL, D3DSTENCILOP_KEEP );
	pDev->SetRenderState( D3DRS_CCW_STENCILPASS, D3DSTENCILOP_KEEP );
	pDev->SetRenderState( D3DRS_CCW_STENCILFUNC, D3DCMP_ALWAYS );
	pDev->SetRenderState( D3DRS_COLORWRITEENABLE1, 0x0000000f );
	pDev->SetRenderState( D3DRS_COLORWRITEENABLE2, 0x0000000f );
	pDev->SetRenderState( D3DRS_COLORWRITEENABLE3, 0x0000000f );
	pDev->SetRenderState( D3DRS_BLENDFACTOR, 0xffffffff );
	pDev->SetRenderState( D3DRS_DEPTHBIAS, dZero );
	pDev->SetRenderState( D3DRS_WRAP8, 0 );
	pDev->SetRenderState( D3DRS_WRAP9, 0 );
	pDev->SetRenderState( D3DRS_WRAP10, 0 );
	pDev->SetRenderState( D3DRS_WRAP11, 0 );
	pDev->SetRenderState( D3DRS_WRAP12, 0 );
	pDev->SetRenderState( D3DRS_WRAP13, 0 );
	pDev->SetRenderState( D3DRS_WRAP14, 0 );
	pDev->SetRenderState( D3DRS_WRAP15, 0 );
	pDev->SetRenderState( D3DRS_ALPHABLENDENABLE, false );
	pDev->SetRenderState( D3DRS_BLENDOP, D3DBLENDOP_ADD );
	pDev->SetRenderState( D3DRS_SEPARATEALPHABLENDENABLE, false ); 
	pDev->SetRenderState( D3DRS_BLENDOPALPHA, D3DBLENDOP_ADD );
	pDev->SetRenderState( D3DRS_ZENABLE, D3DZB_FALSE );
	pDev->SetRenderState( D3DRS_ALPHATESTENABLE, false );
	pDev->SetRenderState( D3DRS_ALPHAFUNC, D3DCMP_ALWAYS );
	pDev->SetRenderState( D3DRS_ALPHAREF, 0 );
	pDev->SetRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );
	// initial blend mode
	pDev->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
	pDev->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
	pDev->SetRenderState( D3DRS_SRCBLENDALPHA, D3DBLEND_ONE );
	pDev->SetRenderState( D3DRS_DESTBLENDALPHA, D3DBLEND_INVSRCALPHA );

	// initialize shadow state
	g_PanDxShadowRenderState.m_blendState = BLENDSTATE_ALPHA;
	g_PanDxShadowRenderState.m_pVS = NULL;
	g_PanDxShadowRenderState.m_pPS = NULL;
	g_PanDxShadowRenderState.m_pVertexDecl = NULL;
	g_PanDxShadowRenderState.m_pVB = NULL;
	g_PanDxShadowRenderState.m_nVertexSize = 0;
	V_memset( g_PanDxShadowRenderState.m_aPanDxPSConsts, 0, sizeof( g_PanDxShadowRenderState.m_aPanDxPSConsts ) );

	PanDxInvalidateRTShadowState();

	// reset batching storage
	for ( int i = 0; i < PANDX_MAX_RSBATCH; i++ )
	{
		V_memset( &g_aPanDxBatchedRenderState[ i ], 0, sizeof( g_aPanDxBatchedRenderState[ i ] ) );
		g_aPanDxBatchedRenderState[i].m_pRT = (IDirect3DTexture9 *)0xffffffff;							// set to dummy/invalid ptr, null used for backbuffer
	}

	// get inital batch states
	PanDxResetBatching();
	PanDxResetVBLocks();
 	PanDxSetRenderTarget( NULL );
 	PanDxSetScissor( 0, NULL );
 	pDev->GetViewport( &g_PanDxLastViewport );
 	PanDxSetViewPort( g_PanDxLastViewport.X, g_PanDxLastViewport.Y, g_PanDxLastViewport.Width, g_PanDxLastViewport.Height, g_PanDxLastViewport.MinZ, g_PanDxLastViewport.MaxZ );

	// init tri list (draw) 
	PanDxResetTriList();

	// these states don't seem to change between drawcalls
	PanDxCommitCommonRS( pDev );

	if ( s_convarPanoramaFlushBuffers.GetBool() )
	{
		g_bVBFlush = true;
	}

	// reset all
	V_memset( &g_PanDxBatching, 0, sizeof( g_PanDxBatching ) );
#ifdef PANDX_TRACK_BATCHING	
	g_bPanDxRenderStateChanged = true;
#endif
}

void PanDxDisownDx()
{
	if ( !g_pdxInit ) return;

	// flush any leftover batched quads
	if ( !IsPanDxBatchEmpty() )
	{
		PanDxFlushBatch( true );
	}

	g_bInsideOwnDx = false;

	IDirect3DDevice9* pDev = g_pPanoramaUIEngineImpl->GetD3Device();

#ifndef DX_TO_GL_ABSTRACTION
	if ( g_pPanDxStateBlock )
	{
		g_pPanDxStateBlock->Apply();
	}

	// Restore srgb render and sampler states
	// (Looks like state block capture/apply didn't work, should investigate at a later state)
	pDev->SetRenderState( D3DRS_SRGBWRITEENABLE, g_srgbRenderState );
	for ( int i = 0; i < 16; i++ )
	{
		pDev->SetSamplerState( i, D3DSAMP_SRGBTEXTURE, g_aPanDxSavedSamplerState[ i ][ D3DSAMP_SRGBTEXTURE ] );
	}
#else
	
	PanDxRestoreGlState();

#endif


	// Restore scissor rect and viewport

	pDev->SetScissorRect( &g_PanSaveScissorRect );
	pDev->SetViewport( &g_PanSaveViewPort );

	// Restore backbuff

	if ( gpBackBuffer )
	{
		pDev->SetRenderTarget( 0, gpBackBuffer );
		gpBackBuffer->Release();
		gpBackBuffer = 0;
	}


	if ( gpDepthStencil )
	{
		pDev->SetDepthStencilSurface( gpDepthStencil );
		gpDepthStencil->Release();
		gpDepthStencil = 0;
	}

	pDev->SetRenderState( D3DRS_SCISSORTESTENABLE, FALSE );

}


//--------------------------------------------------------------------------------------------------
// Called in pandxown...
//--------------------------------------------------------------------------------------------------

void PanDxInvalidateRTShadowState()
{
	if ( !g_pdxInit ) return;

	IDirect3DDevice9* pDev = g_pPanoramaUIEngineImpl->GetD3Device();
	pDev->SetRenderState( D3DRS_SCISSORTESTENABLE, FALSE );
	g_PanDxShadowRenderState.m_bScissorEnabled = false;

	PanDxInvalidateViewportAndScissor();

	PanDxInvalidateRT();
}


//--------------------------------------------------------------------------------------------------
// PAN DX VB's
//
// TODO? look to share one instead of basic/fancy
//--------------------------------------------------------------------------------------------------

void PanDxResetVBLocks()
{
	g_bVBLocked = false;

	// don't reset used counts
}

bool IsPanDxVBLocked()
{
	return g_bVBLocked;
}

bool PanDxVBLock( uint32 nSizeOfData )
{
	if ( !g_pdxInit ) return false;

	DWORD dwLockFlags = D3DLOCK_NOOVERWRITE;

	if ( g_bVBFlush || ( ( g_nVBUsed + nSizeOfData ) > k_nVBSize ) )
	{
//		VPROF_BUDGET( "PanDx VB DISCARD Lock", VPROF_BUDGETGROUP_GAME );
		dwLockFlags = D3DLOCK_DISCARD | D3DLOCK_NOSYSLOCK;

		// use next buffer
		g_nVBBuffer = ( g_nVBBuffer + 1 ) % VB_NUM_BUFFERS;

		// reset num verts in buffer
		g_nVBUsed = 0;

		g_bVBFlush = false;

 		if ( g_pVB[ g_nVBBuffer ]->Lock( g_nVBUsed, nSizeOfData, (void**)&g_pPanDxVB, dwLockFlags ) != D3D_OK )
 			return false;	// PRB error handle
	}
  	else
 	{
 //		VPROF_BUDGET( "PanDx VB Lock", VPROF_BUDGETGROUP_GAME );
 		if ( g_pVB[ g_nVBBuffer ]->Lock( g_nVBUsed, nSizeOfData, (void**)&g_pPanDxVB, dwLockFlags ) != D3D_OK )
 			return false;	// PRB error handle
 	}

	g_bVBLocked = true;

	return true;
}

void PanDxVBUnlock()
{
	if ( !g_bVBLocked )
		return;

	PANDX_BROKE_BATCHING( VB );

	g_pVB[ g_nVBBuffer ]->Unlock();
	g_bVBLocked = false;
}

//--------------------------------------------------------------------------------------------------
// PAN DX VB filling
//--------------------------------------------------------------------------------------------------
#define NUM_VERTS_PER_BASICQUAD 4

bool PanDxFillBasicVB()
{
	// accumulate num verts
	uint32 nNumVerts = 0;
	for ( uint32 batchIdx = 0; batchIdx < g_nPanDxBatchIdx; batchIdx++ )
	{
		nNumVerts += g_aPanDxBatchedRenderState[ batchIdx ].m_nNumQuads * NUM_VERTS_PER_BASICQUAD;
	}

	if ( nNumVerts == 0 )
		return true;

	// lock VB here with nNumVerts count, but round to fancy vert multiple to keep offset (SetStreamSource) calc simple, and all verts aligned 
	uint32 nSizeOfData = k_nBasicVertSize * nNumVerts;
	nSizeOfData = k_nVertAlignment * ( ( nSizeOfData / k_nVertAlignment ) + ( ( nSizeOfData % k_nVertAlignment ) ? 1 : 0 ) );

	if ( !PanDxVBLock( nSizeOfData ) )
		return false;

	uint32 nVBUsed = g_nVBUsed;
	Source2BasicQuadVertex_t *pVB = (Source2BasicQuadVertex_t *)( g_pPanDxVB );

// GL draws indexed strips, DX draws fans
#ifdef DX_TO_GL_ABSTRACTION
	int vertidx[] = { 0,3,1,2 };
#else
	int vertidx[] = { 0,3,2,1 };
#endif

	// fill verts from RS m_pQuad data
	for ( uint32 batchIdx = 0; batchIdx < g_nPanDxBatchIdx; batchIdx++ )
	{
		sPanDxRenderState *pBatchRS = &g_aPanDxBatchedRenderState[ batchIdx ];

		if ( ( pBatchRS->m_nNumQuads == 0 ) || ( pBatchRS->m_pQuad == NULL ) )
			continue;

		pBatchRS->m_nVBIndex = nVBUsed / k_nBasicVertSize;

		panorama::BasicQuad_t *pBQ = ( panorama::BasicQuad_t * )pBatchRS->m_pQuad;

		for ( int &i : vertidx )
		{
			pVB->m_vecPosition.Init( pBQ->m_vPosition[ i ].x, pBQ->m_vPosition[ i ].y, 0.0f, 1.0f );
			pVB->m_vecTex.Init( pBQ->m_vUV[ i ].x, pBQ->m_vUV[ i ].y, 0, 0 );
			pVB->m_vecTex1.Init( 0, 0, 0, 0 );
			pVB->m_vecColor.Init( 0, 0, 0, 0 );
			pVB++;
		}
		nVBUsed += pBatchRS->m_nNumQuads * NUM_VERTS_PER_BASICQUAD * k_nBasicVertSize;
	}

	g_nVBUsed += nSizeOfData;
	g_pPanDxVB += nSizeOfData;

	// unlock
	PanDxVBUnlock();

	return true;
}

#define NUM_VERTS_PER_FANCYQUAD 6

bool PanDxFillFancyVB()
{
	// accumulate num verts
	uint32 nNumVerts = 0;
	for ( uint32 batchIdx = 0; batchIdx < g_nPanDxBatchIdx; batchIdx++ )
	{
		nNumVerts += g_aPanDxBatchedRenderState[ batchIdx ].m_nNumQuads * NUM_VERTS_PER_FANCYQUAD;
	}

	if ( nNumVerts == 0 )
		return true;

	// lock VB here with nNumVerts count
	uint32 nSizeOfData = k_nFancyVertSize * nNumVerts;
	nSizeOfData = k_nVertAlignment * ( ( nSizeOfData / k_nVertAlignment ) + ( ( nSizeOfData % k_nVertAlignment ) ? 1 : 0 ) );

	if ( !PanDxVBLock( nSizeOfData ) )
		return false;

	uint32 nVBUsed = g_nVBUsed;
	Source2FancyQuadVertex_t *pVB = (Source2FancyQuadVertex_t *)( g_pPanDxVB );

	int vertidx[] = { 0,2,1,0,3,2 };

	// fill verts 
	for ( uint32 batchIdx = 0; batchIdx < g_nPanDxBatchIdx; batchIdx++ )
	{
		sPanDxRenderState *pBatchRS = &g_aPanDxBatchedRenderState[ batchIdx ];

		if ( ( pBatchRS->m_nNumQuads == 0 ) || ( pBatchRS->m_pQuad == NULL ) )
			continue;

		pBatchRS->m_nVBIndex = nVBUsed / k_nFancyVertSize;

		for ( int i = 0; i < pBatchRS->m_nNumQuads; i++ )
		{
			panorama::FancyQuad_t *pFQ = ( panorama::FancyQuad_t * )pBatchRS->m_pQuad + i;
			for ( int &j : vertidx )
			{
				pVB->m_vecPosition.Init( pFQ->m_Verts[ j ].m_vPosition.x, pFQ->m_Verts[ j ].m_vPosition.y, 0.0f, 1.0f );
				pVB->m_vecColor0 = pFQ->m_vColor;
				pVB->m_vecColor1 = pFQ->m_vColorStop;
				pVB->m_vecTexCoordGradientCoord.Init( pFQ->m_Verts[ j ].m_vUV.x, pFQ->m_Verts[ j ].m_vUV.y,
																  pFQ->m_Verts[ j ].m_vUVGradient.x, pFQ->m_Verts[ j ].m_vUVGradient.y );
				pVB->m_vecOpacityTexCoord.Init( pFQ->m_Verts[ j ].m_vUVOpacity.x, pFQ->m_Verts[ j ].m_vUVOpacity.y, 0, 0 );
				pVB->m_vecFragCoordWdHt = pFQ->m_Verts[ j ].m_vFragCoordWdHt;

				pVB++;
			}
		}

		nVBUsed += pBatchRS->m_nNumQuads * NUM_VERTS_PER_FANCYQUAD * k_nFancyVertSize;
	}

	g_nVBUsed += nSizeOfData;
	g_pPanDxVB += nSizeOfData;

	// unlock
	PanDxVBUnlock();

	return true;
}

//--------------------------------------------------------------------------------------------------
// PAN DX draw tri list batching
//--------------------------------------------------------------------------------------------------

void PanDxResetTriList()
{
	g_nTriListStartIndex = 0;
	g_nTriListTriCount = 0;
}

void PanDxFlushTrisList()
{
	if ( !g_pdxInit ) return;

	if ( g_nTriListTriCount == 0 ) return;

	IDirect3DDevice9* pDev = g_pPanoramaUIEngineImpl->GetD3Device();



#ifndef DX_TO_GL_ABSTRACTION
	pDev->DrawPrimitive( D3DPT_TRIANGLELIST, g_nTriListStartIndex, g_nTriListTriCount );
#else
	pDev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, g_nTriListStartIndex, g_nTriListTriCount * 3, g_nTriListStartIndex, g_nTriListTriCount);
#endif



	PanDxResetTriList();

	PANDX_STATS_BATCHING( FancyDrawCalls );
}

void PanDxAddToTriList( int nStartIndex, int nNumTris )
{
	if ( !g_pdxInit ) return;

	if ( g_nTriListTriCount == 0 )
	{
		g_nTriListStartIndex = nStartIndex;
	}

	g_nTriListTriCount += nNumTris;
}

//--------------------------------------------------------------------------------------------------
// PAN DX render state batching
//--------------------------------------------------------------------------------------------------

bool IsPanDxBatchEmpty()
{
	return ( g_nPanDxBatchIdx == 0 ) && ( g_aPanDxBatchedRenderState[0].m_nFlags == 0 );
}

uint32 GetPanDxCurrentBatchEntryFlags()
{
	if ( g_nPanDxBatchIdx >= PANDX_MAX_RSBATCH )
		return 0;

	return g_aPanDxBatchedRenderState[ g_nPanDxBatchIdx ].m_nFlags;
}

uint32 GetPanDxAccumulatedBatchFlags()
{
	return g_nPanDxCurrentBatchType;
}

void PanDxResetBatchEntry( sPanDxRenderState *pSrcRS )
{
	pSrcRS->m_pRT = (IDirect3DTexture9 *)0xffffffff;
	pSrcRS->m_nNumQuads = 0;
	pSrcRS->m_nFlags = 0;
	pSrcRS->m_clearMask = 0;
	pSrcRS->m_pQuad = NULL;
}

void PanDxEndBatchEntry()
{
	g_nPanDxBatchIdx++;

	if ( g_nPanDxBatchIdx == PANDX_MAX_RSBATCH )
	{
		PanDxFlushBatch( false );
	}
}

void PanDxResetBatching()
{
	g_nPanDxBatchIdx = 0;
	g_nPanDxCurrentBatchType = 0;
}

sPanDxRenderState *PanDxGetBatchEntry( uint32 flags )
{
	if ( !g_pdxInit ) return &g_aPanDxBatchedRenderState[0];  

	sPanDxRenderState *pBatchEntry = &g_aPanDxBatchedRenderState[ g_nPanDxBatchIdx ];

	uint32 drawType = flags & PANDX_RSBATCHYPE_DRAW;
	uint32 currentBatchDrawType = GetPanDxAccumulatedBatchFlags() & PANDX_RSBATCHYPE_DRAW;

	bool bFlushCurrentBatchEntry = ( currentBatchDrawType && drawType && ( drawType != currentBatchDrawType ) );

	if ( bFlushCurrentBatchEntry )
	{
		// the current batch queue has been batching draw calls of one type and we've hit the other
		// => flush the current batch queue before starting again

		// early flush due to to basic/fancy switch
		PanDxFlushBatch( true );

		// get fresh batch entry
		g_nPanDxBatchIdx = 0;
		pBatchEntry = &g_aPanDxBatchedRenderState[ 0 ];

		pBatchEntry->m_nFlags = flags;
		g_nPanDxCurrentBatchType = flags;

	}
	else
	{
		// end the prior batch entry if we're setting RT but have VP/Scissor settings already pending
		// alternative is to call PanDXEndBatchEntry for all SetXXXState calls, this would increase the batch count by removing some of the implicit redundant capture that
		// is happening (i.e. SetVP, SetVP, SetVP, Draw would be 4 batch entries instead of 1), so we'll keep this test here for now
		if ( ( flags == PANDX_RSBATCHTYPE_RT ) && ( GetPanDxAccumulatedBatchFlags() & ( PANDX_RSBATCHTYPE_VP | PANDX_RSBATCHTYPE_SCISSOR | PANDX_RSBATCHTYPE_SCISSORENABLE ) ) )
		{
			PanDxEndBatchEntry();
			// get a new entry
			pBatchEntry = &g_aPanDxBatchedRenderState[ g_nPanDxBatchIdx ];
		}

		pBatchEntry->m_nFlags |= flags;
		g_nPanDxCurrentBatchType |= flags;
	}

	return pBatchEntry;
}

//--------------------------------------------------------------------------------------------------
// PAN DX Set***State calls
//--------------------------------------------------------------------------------------------------

void PanDxInitSamplerState( IDirect3DDevice9* pDev, int nSampler, ePanDxSamplerStateType ssType, DWORD val )
{
	if ( !g_pdxInit ) return;

	g_PanDxShadowRenderState.m_aSamplerStates[ nSampler ][ ssType ] = val;
	pDev->SetSamplerState( nSampler, g_aMapPanDxSamplerStateTypeToD3D[ ssType ], val );
}

void PanDxCommitSamplerState( IDirect3DDevice9* pDev, int nSampler, ePanDxSamplerStateType ssType, DWORD val )
{
	if ( !g_pdxInit ) return;

	if ( val != g_PanDxShadowRenderState.m_aSamplerStates[ nSampler ][ ssType ] )
	{
		PanDxFlushTrisList();

		g_PanDxShadowRenderState.m_aSamplerStates[ nSampler ][ ssType ] = val;
		pDev->SetSamplerState( nSampler, g_aMapPanDxSamplerStateTypeToD3D[ ssType ], val );

		PANDX_BROKE_BATCHING( SamplerState );
	}
}

void PanDxCommitSampler( IDirect3DDevice9* pDev, int nSampler, IDirect3DBaseTexture9* pD3DTex )
{
	if ( !g_pdxInit ) return;

	if ( pD3DTex != g_PanDxShadowRenderState.m_aSamplers[ nSampler ] )
	{
		PanDxFlushTrisList();

		pDev->SetTexture( nSampler, pD3DTex );
		g_PanDxShadowRenderState.m_aSamplers[ nSampler ] = pD3DTex;

		PANDX_BROKE_BATCHING( Sampler );
	}
}

//--------------------------------------------------------------------------------------------------
// Textures
//--------------------------------------------------------------------------------------------------

void PanDxCommitTexture( IDirect3DDevice9 *pDev, int nSampler, sPanDxRenderState *pSrcRS )
{
	if ( !g_pdxInit ) return;

	IDirect3DBaseTexture9* pD3DTex = pSrcRS->m_aSamplers[ nSampler ];

	if ( !pD3DTex )
	{
		PanDxCommitSampler( pDev, nSampler, NULL );
	}
	else
	{

		if ( pSrcRS->m_blendState != RsBlendStateHandle_t::BLENDSTATE_MIX_ADDITIVESRGB )
		{
			PanDxCommitSamplerState( pDev, nSampler, PANDXSAMP_SRGBTEXTURE, pSrcRS->m_aSamplerStates[ nSampler ][ PANDXSAMP_SRGBTEXTURE ] );
		}
		else
		{
			PanDxCommitSamplerState( pDev, nSampler, PANDXSAMP_SRGBTEXTURE, 0 );
		}

		PanDxCommitSamplerState( pDev, nSampler, PANDXSAMP_ADDRESSU, pSrcRS->m_aSamplerStates[ nSampler ][ PANDXSAMP_ADDRESSU ] );
		PanDxCommitSamplerState( pDev, nSampler, PANDXSAMP_ADDRESSV, pSrcRS->m_aSamplerStates[ nSampler ][ PANDXSAMP_ADDRESSV ] );

		PanDxCommitSampler( pDev, nSampler, pD3DTex );
	}


#ifdef PANDX_TRACK_BATCHING
	if ( pSrcRS->m_aSamplersIsRenderTarget[ nSampler ] )
		PANDX_STATS_BATCHING( SamplerIsRenderTarget );
#endif

}

//--------------------------------------------------------------------------------------------------
// Renderstates
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Those that appear to be common for all panorama drawcalls, made once at OwnDx
//--------------------------------------------------------------------------------------------------

void PanDxCommitCommonRS( IDirect3DDevice9* pDev )
{
	if ( !g_pdxInit ) return;

	//VPROF_BUDGET( "PanDxSetCommonRS", VPROF_BUDGETGROUP_GAME );

	pDev->SetRenderState( D3DRS_ALPHAFUNC, D3DCMP_ALWAYS );
	pDev->SetRenderState( D3DRS_ALPHAREF, 0 );
	pDev->SetRenderState( D3DRS_ALPHATESTENABLE, false );
	pDev->SetRenderState( D3DRS_ZFUNC, D3DCMP_ALWAYS );
	pDev->SetRenderState( D3DRS_ZWRITEENABLE, false );
	pDev->SetRenderState( D3DRS_ZENABLE, false );
	pDev->SetRenderState( D3DRS_SRGBWRITEENABLE, true );

	// covered by which texture set != NULL ?
// 	pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );
// 	pShaderShadow->EnableTexture( SHADER_SAMPLER1, true );
// 	pShaderShadow->EnableTexture( SHADER_SAMPLER2, true );
// 	pShaderShadow->EnableTexture( SHADER_SAMPLER3, true );

	// PRB todo we need to clear this via shaderapi which will test caps etc ?
	//pShaderShadow->EnableAlphaToCoverage( false );

	pDev->SetRenderState( D3DRS_ALPHABLENDENABLE, true );
	pDev->SetRenderState( D3DRS_SEPARATEALPHABLENDENABLE, true );
	pDev->SetRenderState( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN |
						  D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA );
	pDev->SetRenderState( D3DRS_BLENDOP, D3DBLENDOP_ADD );
	pDev->SetRenderState( D3DRS_BLENDOPALPHA, D3DBLENDOP_ADD );
}

//--------------------------------------------------------------------------------------------------
// PanDxSet*** and PanDxCommit*** fns for draw calls
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Textures
//--------------------------------------------------------------------------------------------------

void PanDxSetTexture( sPanDxRenderState *pDstRS, int nSampler, ITexture* pTexture, uint32 nFlags )
{
	if ( !g_pdxInit ) return;

	if ( pTexture )
	{
		IDirect3DBaseTexture9* pD3DTex = (IDirect3DBaseTexture9 *)g_pMaterialSystem->GetPanormaTexturePtr( pTexture );

		pDstRS->m_aSamplers[ nSampler ] = pD3DTex;

		pDstRS->m_aSamplerStates[ nSampler ][ PANDXSAMP_SRGBTEXTURE ] = ( nFlags & TEXTURE_BINDFLAGS_SRGBREAD ) != 0;
		pDstRS->m_aSamplerStates[ nSampler ][ PANDXSAMP_ADDRESSU ] = ( pTexture->GetFlags() & TEXTUREFLAGS_CLAMPS ) ? D3DTADDRESS_CLAMP : D3DTADDRESS_WRAP;
		pDstRS->m_aSamplerStates[ nSampler ][ PANDXSAMP_ADDRESSV ] = ( pTexture->GetFlags() & TEXTUREFLAGS_CLAMPT ) ? D3DTADDRESS_CLAMP : D3DTADDRESS_WRAP;
	}
	else
	{
		pDstRS->m_aSamplers[ nSampler ] = nullptr;
	}

#ifdef PANDX_TRACK_BATCHING
	if ( pTexture )
	{
		pDstRS->m_aSamplersIsRenderTarget[ nSampler ] = pTexture->IsRenderTarget();
	}
	else
	{
		pDstRS->m_aSamplersIsRenderTarget[ nSampler ] = false;
	}
#endif
}

void PanDxSetTexturesBasic( sPanDxRenderState *pDstRS, CRenderAttributes* pAttr )
{
	ITexture *pTexture = nullptr;
	pAttr->GetValue( &pTexture, ATTR_Texture0 );

	PanDxSetTexture( pDstRS, 0, pTexture, (uint32)TEXTURE_BINDFLAGS_SRGBREAD );
}

void PanDxSetTexturesFancy( sPanDxRenderState *pDstRS, CRenderAttributes* pAttr )
{
	ITexture* pTexture;

	int texType = pAttr->GetValue( ATTR_D_TEXTURETYPE );

	TextureBindFlags_t flags[ 4 ] = { TEXTURE_BINDFLAGS_SRGBREAD, TEXTURE_BINDFLAGS_SRGBREAD, TEXTURE_BINDFLAGS_SRGBREAD, TEXTURE_BINDFLAGS_SRGBREAD };

	if ( texType )
	{
		if ( texType == 3 )
		{
			flags[ 0 ] = TEXTURE_BINDFLAGS_NONE;
			flags[ 1 ] = TEXTURE_BINDFLAGS_NONE;
			flags[ 2 ] = TEXTURE_BINDFLAGS_NONE;
		}

		pAttr->GetValue( &pTexture, ATTR_Texture0 );
		if ( texType == 4 )
		{
			if ( pTexture->GetFlags() & TEXTUREFLAGS_YCOCG )
			{
				// YCoCg, reset bind flag
				flags[ 0 ] = TEXTURE_BINDFLAGS_NONE;
			}
//			else
//			{
//				// texture flagged as having alpha bits, then reset type to RGBA (1)
//				texType = 1;
//			}
		}
		PanDxSetTexture( pDstRS, 0, pTexture, flags[ 0 ] );

		if ( ( pAttr->GetValue( ATTR_D_TEXTURETYPE ) == FancyQuadTextureType_YUV ) || pAttr->GetValue( ATTR_D_USEOPACITYMASK ) )
		{
			pAttr->GetValue( &pTexture, ATTR_Texture1 );
			PanDxSetTexture( pDstRS, 1, pTexture, flags[ 1 ] );
		}

		if ( pAttr->GetValue( ATTR_D_TEXTURETYPE ) == FancyQuadTextureType_YUV )
		{
			pAttr->GetValue( &pTexture, ATTR_Texture2 );
			PanDxSetTexture( pDstRS, 2, pTexture, flags[ 2 ] );
		}
	}
	else
	{
		PanDxSetTexture( pDstRS, 0, NULL, 0 );
	}

	if ( pAttr->GetValue( ATTR_D_GRADIENT_COMPLEX ) )
	{
		pAttr->GetValue( &pTexture, ATTR_Texture3 );
		PanDxSetTexture( pDstRS, 3, pTexture, flags[ 3 ] );
	}
}

void PanDxCommitTexturesBasic( IDirect3DDevice9* pDev, sPanDxRenderState *pSrcRS )
{
	if ( !g_pdxInit ) return;

	PanDxCommitTexture( pDev, 0, pSrcRS );
}

void PanDxCommitTexturesFancy( IDirect3DDevice9* pDev, sPanDxRenderState *pSrcRS )
{
	if ( !g_pdxInit ) return;

	for ( int i = 0; i < PANDX_MAX_SAMPLERS; i++ )
	{
		PanDxCommitTexture( pDev, i, pSrcRS );
	}
}

//--------------------------------------------------------------------------------------------------
// Per draw RS (blend mode)
//--------------------------------------------------------------------------------------------------

void PanDxSetPerDrawRS( sPanDxRenderState *pDstRS, RsBlendStateHandle_t blendState )
{
	pDstRS->m_blendState = blendState;
}

void PanDxCommitPerDrawRs( IDirect3DDevice9* pDev, RsBlendStateHandle_t blendState )
{
	if ( !g_pdxInit ) return;

	if ( blendState == g_PanDxShadowRenderState.m_blendState )
		return;

	PanDxFlushTrisList();
	PANDX_BROKE_BATCHING( RenderState );

	switch ( blendState )
	{

	case BLENDSTATE_ALPHA:
		pDev->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
		pDev->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );

		pDev->SetRenderState( D3DRS_SRCBLENDALPHA, D3DBLEND_ONE );
		pDev->SetRenderState( D3DRS_DESTBLENDALPHA, D3DBLEND_INVSRCALPHA );

		pDev->SetRenderState( D3DRS_SRGBWRITEENABLE, true );

		break;

		// Premultiplied Alpha Blend
	case BLENDSTATE_PREMULT_ALPHA:
		pDev->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_ONE );
		pDev->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );

		pDev->SetRenderState( D3DRS_SRCBLENDALPHA, D3DBLEND_ONE );
		pDev->SetRenderState( D3DRS_DESTBLENDALPHA, D3DBLEND_INVSRCALPHA );

		pDev->SetRenderState( D3DRS_SRGBWRITEENABLE, true );

		break;

		// Alpha Only Blend
	case BLENDSTATE_ONLY_ALPHA:
		pDev->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_ZERO );
		pDev->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );

		pDev->SetRenderState( D3DRS_SRCBLENDALPHA, D3DBLEND_ZERO );
		pDev->SetRenderState( D3DRS_DESTBLENDALPHA, D3DBLEND_INVSRCALPHA );

		pDev->SetRenderState( D3DRS_SRGBWRITEENABLE, true );

		break;

	case BLENDSTATE_MIX_MULTIPLY:
		pDev->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_DESTCOLOR );
		pDev->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ZERO );

		pDev->SetRenderState( D3DRS_SRCBLENDALPHA, D3DBLEND_ONE );
		pDev->SetRenderState( D3DRS_DESTBLENDALPHA, D3DBLEND_INVSRCALPHA );

		pDev->SetRenderState( D3DRS_SRGBWRITEENABLE, true );

		break;

	case BLENDSTATE_MIX_SCREEN:
		pDev->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_INVDESTCOLOR );
		pDev->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ONE );

		pDev->SetRenderState( D3DRS_SRCBLENDALPHA, D3DBLEND_ONE );
		pDev->SetRenderState( D3DRS_DESTBLENDALPHA, D3DBLEND_INVSRCALPHA );

		pDev->SetRenderState( D3DRS_SRGBWRITEENABLE, true );

		break;

	case BLENDSTATE_MIX_ADDITIVE:
		pDev->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_ONE );
		pDev->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ONE );

		pDev->SetRenderState( D3DRS_SRCBLENDALPHA, D3DBLEND_ONE );
		pDev->SetRenderState( D3DRS_DESTBLENDALPHA, D3DBLEND_INVSRCALPHA );

		pDev->SetRenderState( D3DRS_SRGBWRITEENABLE, true );

		break;

	case BLENDSTATE_MIX_ADDITIVESRGB:
		pDev->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_ONE );
		pDev->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ONE );

		pDev->SetRenderState( D3DRS_SRCBLENDALPHA, D3DBLEND_ONE );
		pDev->SetRenderState( D3DRS_DESTBLENDALPHA, D3DBLEND_INVSRCALPHA );

		pDev->SetRenderState( D3DRS_SRGBWRITEENABLE, false );

		break;

	case BLENDSTATE_MIX_OPAQUE:
		pDev->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_ONE );
		pDev->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ZERO );

		pDev->SetRenderState( D3DRS_SRCBLENDALPHA, D3DBLEND_ONE );
		pDev->SetRenderState( D3DRS_DESTBLENDALPHA, D3DBLEND_INVSRCALPHA );

		pDev->SetRenderState( D3DRS_SRGBWRITEENABLE, true );

		break;

	}

	g_PanDxShadowRenderState.m_blendState = blendState;
}

void PanDxCommitPerDrawRs( IDirect3DDevice9* pDev, sPanDxRenderState *pSrcRS )
{
	PanDxCommitPerDrawRs( pDev, pSrcRS->m_blendState );
}


//--------------------------------------------------------------------------------------------------
// Shaders
//--------------------------------------------------------------------------------------------------

void PanDxTermShaders()
{
}

void PanDxSetShadersBasic( void **ppVS, void **ppPS, CRenderAttributes* pAttr )
{
	if ( !g_pdxInit ) return;

	int nVSCombo = 0;

	int nPSCombo = ( pAttr->m_intAttrs[ ATTR_D_TEX2DFASTBLUR - ATTR_INT_MIN ] * 1 )
		+ ( pAttr->m_intAttrs[ ATTR_D_TEX2DBLUR - ATTR_INT_MIN ] * 9 )
		+ ( pAttr->m_intAttrs[ ATTR_D_TEX2DPARTICLE - ATTR_INT_MIN ] * 18 )
		+ ( pAttr->m_intAttrs[ ATTR_D_TEX2DDOWNSAMPLE - ATTR_INT_MIN ] * 36 );

	if ( nPSCombo >= ARRAYSIZE( g_PanDxBasicPsShaders ) )
	{
		Error( "Panorama invalid shader combo" );
		return;
	}

	void* &pMsVs = g_PanDxBasicVsShaders[ nVSCombo ];
	if ( !pMsVs )
	{
		pMsVs = g_pMaterialSystem->GetOSVertexShader( "panorama_vs30", nVSCombo );
	}

	void* &pMsPs = g_PanDxBasicPsShaders[ nPSCombo ];
	if ( !pMsPs )
	{
		pMsPs = g_pMaterialSystem->GetOSPixelShader( "panorama_ps30", nPSCombo );
	}

	*ppVS = (IDirect3DVertexShader9*)pMsVs;
	*ppPS = (IDirect3DVertexShader9*)pMsPs;
}

void PanDxSetShadersBasic( sPanDxRenderState *pDstRS, CRenderAttributes* pAttr )
{
	PanDxSetShadersBasic( &pDstRS->m_pVS, &pDstRS->m_pPS, pAttr );
}

void PanDxSetShadersFancy( sPanDxRenderState *pDstRS, CRenderAttributes* pAttr )
{
	if ( !g_pdxInit ) return;

	//VPROF_BUDGET( "PanDxSetShadersFancy", VPROF_BUDGETGROUP_GAME );

	int nVSCombo = 0;

	int texType = pAttr->GetValue( ATTR_D_TEXTURETYPE );
	if ( texType == 4 )
	{
		ITexture* pTexture;
		pAttr->GetValue( &pTexture, ATTR_Texture0 );

		if ( !( pTexture->GetFlags() & TEXTUREFLAGS_YCOCG ) )
		{
			texType = 1;
		}
	}

	int nPSCombo = ( 1 * texType )
		+ ( 5 * pAttr->m_intAttrs[ ATTR_D_PREMULTIPLY_ALPHA - ATTR_INT_MIN ] )
		+ ( 10 * pAttr->m_intAttrs[ ATTR_D_USERADIALGRADIENT - ATTR_INT_MIN ] )
		+ ( 20 * pAttr->m_intAttrs[ ATTR_D_GRADIENT_TWOSTOP - ATTR_INT_MIN ] )
		+ ( 40 * pAttr->m_intAttrs[ ATTR_D_GRADIENT_COMPLEX - ATTR_INT_MIN ] )
		+ ( 80 * pAttr->m_intAttrs[ ATTR_D_USEOUTERCORNER - ATTR_INT_MIN ] )
		+ ( 160 * pAttr->m_intAttrs[ ATTR_D_USEINNERCORNER - ATTR_INT_MIN ] )
		+ ( 320 * pAttr->m_intAttrs[ ATTR_D_COLORCORRECTION - ATTR_INT_MIN ] )
		+ ( 640 * pAttr->m_intAttrs[ ATTR_D_USEOPACITYMASK - ATTR_INT_MIN ] )
		+(  1280 * pAttr->m_intAttrs[ ATTR_D_USERADIALCLIP - ATTR_INT_MIN ]);

	if ( nPSCombo >= ARRAYSIZE( g_PanDxFancyPsShaders ) )
	{
		Error( "Panorama invalid shader combo" );
		return;
	}

	void* &pMsVs = g_PanDxFancyVsShaders[ nVSCombo ];
	if ( !pMsVs )
	{
		pMsVs = g_pMaterialSystem->GetOSVertexShader( "panoramafancy_vs30", nVSCombo );
	}

	void* &pMsPs = g_PanDxFancyPsShaders[ nPSCombo ];
	if ( !pMsPs )
	{
		pMsPs = g_pMaterialSystem->GetOSPixelShader( "panoramafancy_ps30", nPSCombo );
	}

	pDstRS->m_pVS = (IDirect3DVertexShader9*)pMsVs;
	pDstRS->m_pPS = (IDirect3DVertexShader9*)pMsPs;
}

void PanDxCommitVertexShader( IDirect3DDevice9* pDev, void* pVs )
{
	if ( !g_pdxInit ) return;

	if ( pVs != g_PanDxShadowRenderState.m_pVS )
	{
		PanDxFlushTrisList();

		g_PanDxShadowRenderState.m_pVS = pVs;
		pDev->SetVertexShader( (IDirect3DVertexShader9*)pVs );

		PANDX_BROKE_BATCHING( VS );
	}
}

void PanDxCommitPixelShader( IDirect3DDevice9* pDev, void* pPs )
{
	if ( !g_pdxInit ) return;

	if ( pPs != g_PanDxShadowRenderState.m_pPS )
	{
		PanDxFlushTrisList();

		g_PanDxShadowRenderState.m_pPS = pPs;
		pDev->SetPixelShader( (IDirect3DPixelShader9*)pPs );

		PANDX_BROKE_BATCHING( PS );
	}
}

void PanDxCommitShaders( IDirect3DDevice9* pDev, sPanDxRenderState *pSrcRS )
{
	if ( !g_pdxInit ) return;

	PanDxCommitVertexShader( pDev, pSrcRS->m_pVS );
	PanDxCommitPixelShader( pDev, pSrcRS->m_pPS );
}

//--------------------------------------------------------------------------------------------------
// Shader consts
//--------------------------------------------------------------------------------------------------

void PanDxSetPSConstsBasic( sPanDxRenderState *pDstRS, CRenderAttributes* pAttr )
{
	Vector4D *pConsts = (Vector4D *)pDstRS->m_aPanDxPSConsts;

	pAttr->GetValue( &pConsts[ 0 ], ATTR_centerWeight );
	pAttr->GetValue( &pConsts[ 1 ], ATTR_sample1 );

	pAttr->GetValue( &pConsts[ 2 ], ATTR_sample2 );
	pAttr->GetValue( &pConsts[ 3 ], ATTR_sample3 );
	pAttr->GetValue( &pConsts[ 4 ], ATTR_sample4 );
	pAttr->GetValue( &pConsts[ 5 ], ATTR_sample5 );
	pAttr->GetValue( &pConsts[ 6 ], ATTR_sample6 );
	pAttr->GetValue( &pConsts[ 7 ], ATTR_sample7 );
	pAttr->GetValue( &pConsts[ 8 ], ATTR_sample8 );

	pAttr->GetValue( &pConsts[ 9 ], ATTR_BlurMultiplyVec );
	pAttr->GetValue( &pConsts[ 10 ], ATTR_BlurSigma );
	pAttr->GetValue( &pConsts[ 11 ], ATTR_ParticleSharpness );
	pAttr->GetValue( &pConsts[ 12 ], ATTR_UVClamp );
}

void PanDxSetPSConstsFancy( sPanDxRenderState *pDstRS, CRenderAttributes* pAttr )
{
	Vector4D *pConsts = (Vector4D *)pDstRS->m_aPanDxPSConsts;
	Vector4D *pV;

	V_memset( pConsts, 0, sizeof( pDstRS->m_aPanDxPSConsts ) );

	if ( pAttr->GetValue( ATTR_D_USEOUTERCORNER ) || pAttr->GetValue( ATTR_D_USEINNERCORNER ) )
	{
		pAttr->GetValue( &pConsts[ 0 ], ATTR_TopCornerRad );
		pAttr->GetValue( &pConsts[ 1 ], ATTR_BtmCornerRad );
		pAttr->GetValue( &pConsts[ 2 ], ATTR_BorderWd );

		pV = &pConsts[ 0 ];	// ATTR_TopCornerRad
		*pV += Vector4D( 0.5, 0.5, 0.5, 0.5 );
		pV = &pConsts[ 1 ];	// ATTR_BtmCornerRad
		*pV += Vector4D( 0.5, 0.5, 0.5, 0.5 );
		pV = &pConsts[ 2 ];	// ATTR_BorderWd
		*pV += Vector4D( 0.5, 0.5, 0.5, 0.5 );
	}

	if ( pAttr->GetValue( ATTR_D_USEINNERCORNER ) )
	{
		pAttr->GetValue( &pConsts[ 3 ], ATTR_Bordercolor );
	}

	pV = &pConsts[ 4 ];

	if ( pAttr->GetValue( ATTR_D_USERADIALGRADIENT ) )
	{
		Vector4D vRadialGradientOffset;
		pAttr->GetValue( &vRadialGradientOffset, ATTR_Gradientradialoffset );
		pV->x = vRadialGradientOffset.x;
		pV->y = vRadialGradientOffset.y;
	}

	if ( pAttr->GetValue( ATTR_D_USEOPACITYMASK ) )
	{
		Vector4D vOpacityMaskOpacity;
		pAttr->GetValue( &vOpacityMaskOpacity, ATTR_OpacityMaskOpacity );
		pV->z = vOpacityMaskOpacity.x;
	}

	if ( pAttr->GetValue( ATTR_D_USERADIALCLIP ) )
	{
		pV = &pConsts[6];

		Vector4D vRadialClip;
		pAttr->GetValue( &vRadialClip, ATTR_RadialClipCenterX );
		pV->x = vRadialClip.x;
		pAttr->GetValue( &vRadialClip, ATTR_RadialClipCenterY );
		pV->y = vRadialClip.x;
		pAttr->GetValue( &vRadialClip, ATTR_RadialClipStartAngle );
		pV->z = vRadialClip.x;
		pAttr->GetValue( &vRadialClip, ATTR_RadialClipSectorAngle );
		pV->w = vRadialClip.x;
	}

	if ( pAttr->GetValue( ATTR_D_COLORCORRECTION ) )
	{
		Vector4D vH, vS, vB, vC;
		pAttr->GetValue( &vH, ATTR_HueShift );
		pAttr->GetValue( &vS, ATTR_Saturation );
		pAttr->GetValue( &vB, ATTR_Brightness );
		pAttr->GetValue( &vC, ATTR_Contrast );

		pV = &pConsts[ 5 ];
		pV->x = vH.x;
		pV->y = vS.x;
		pV->z = vB.x;
		pV->w = vC.x;
	}
}

void PanDxCommitPSConstsBasic( IDirect3DDevice9* pDev, sPanDxRenderState *pSrcRS )
{
	if ( !g_pdxInit ) return;

	Vector4D *pConsts = (Vector4D *)pSrcRS->m_aPanDxPSConsts;
	Vector4D *pShadowConsts = (Vector4D *)g_PanDxShadowRenderState.m_aPanDxPSConsts;


	bool bConstChanged = false;

#ifdef PANDX_LAZY_STATE_EVAL_CONSTS_ALL

	if ( V_memcmp( pConsts, pShadowConsts, 13 * sizeof( Vector4D ) ) )
	{
		pDev->SetPixelShaderConstantF( 0, (float *)pConsts, 13 );
		V_memcpy( pShadowConsts, pConsts, 13 * sizeof( Vector4D ) );

		bConstChanged = true;
	}

#else

	// split into most commonly changing blocks of consts

	if ( V_memcmp( &pConsts[ 0 ], &pShadowConsts[ 0 ], 2 * sizeof( Vector4D ) ) )
	{
		PanDxFlushTrisList();

		pDev->SetPixelShaderConstantF( 0, (float *)&pConsts[ 0 ], 2 );
		V_memcpy( &pShadowConsts[ 0 ], &pConsts[ 0 ], 2 * sizeof( Vector4D ) );

		bConstChanged = true;
	}

	if ( V_memcmp( &pConsts[ 2 ], &pShadowConsts[ 2 ], 7 * sizeof( Vector4D ) ) )
	{
		PanDxFlushTrisList();

		pDev->SetPixelShaderConstantF( 2, (float *)&pConsts[ 2 ], 7 );
		V_memcpy( &pShadowConsts[ 2 ], &pConsts[ 2 ], 7 * sizeof( Vector4D ) );

		bConstChanged = true;
	}

	if ( V_memcmp( &pConsts[ 9 ], &pShadowConsts[ 9 ], 4 * sizeof( Vector4D ) ) )
	{
		PanDxFlushTrisList();

		pDev->SetPixelShaderConstantF( 9, (float *)&pConsts[ 9 ], 4 );
		V_memcpy( &pShadowConsts[ 9 ], &pConsts[ 9 ], 4 * sizeof( Vector4D ) );

		bConstChanged = true;
	}

	if ( bConstChanged )
	{
		PANDX_BROKE_BATCHING( PSConst );
	}

#endif
}

void PanDxCommitPSConstsFancy( IDirect3DDevice9* pDev, sPanDxRenderState *pSrcRS )
{
	if ( !g_pdxInit ) return;

	if ( V_memcmp( pSrcRS->m_aPanDxPSConsts, g_PanDxShadowRenderState.m_aPanDxPSConsts, 7 * sizeof( Vector4D ) ) )
	{
		PanDxFlushTrisList();

		pDev->SetPixelShaderConstantF( 0, pSrcRS->m_aPanDxPSConsts, 7 );
		V_memcpy( g_PanDxShadowRenderState.m_aPanDxPSConsts, pSrcRS->m_aPanDxPSConsts, 7 * sizeof( Vector4D ) );

		PANDX_BROKE_BATCHING( PSConst );
	}
}

//--------------------------------------------------------------------------------------------------
// Vertex decl, no need to flush batched tris 
// (assumption is we never switch vertex decl since we're only batching fancy tri calls)
//--------------------------------------------------------------------------------------------------

void PanDxSetVertexDecl( sPanDxRenderState *pDstRS, IDirect3DVertexDeclaration9* pVertexDecl )
{
	pDstRS->m_pVertexDecl = pVertexDecl;
}

void PanDxCommitVertexDecl( IDirect3DDevice9* pDev, IDirect3DVertexDeclaration9* pVertexDecl )
{
	if ( !g_pdxInit ) return;

	if ( pVertexDecl != g_PanDxShadowRenderState.m_pVertexDecl )
	{
		pDev->SetVertexDeclaration( pVertexDecl );
		g_PanDxShadowRenderState.m_pVertexDecl = pVertexDecl;

		PANDX_BROKE_BATCHING( VertexDecl );
	}
}

//--------------------------------------------------------------------------------------------------
// Stream/VB. no need to flush batched tris
// (assumption is we never switch VB since we're only batching fancy tri calls)
//--------------------------------------------------------------------------------------------------

void PanDxSetStreamSource( sPanDxRenderState *pDstRS, IDirect3DVertexBuffer9* pVB, const int nVertSize, void *pQuad, const int nNumQuads )
{
	pDstRS->m_pVB			= pVB;
	pDstRS->m_nVertexSize	= nVertSize;
	pDstRS->m_nNumQuads		= nNumQuads;

	pDstRS->m_pQuad			= pQuad; 		// we'll copy vert data from this (fancy/basic quad source) into m_pVB just before we draw
}

void PanDxCommitStreamSource( IDirect3DDevice9* pDev, IDirect3DVertexBuffer9* pVB, int nVertexSize )
{
	if ( !g_pdxInit ) return;

	if ( ( pVB != g_PanDxShadowRenderState.m_pVB ) ||
		 ( nVertexSize != g_PanDxShadowRenderState.m_nVertexSize ) )
	{
		g_PanDxShadowRenderState.m_pVB = pVB;
		g_PanDxShadowRenderState.m_nVertexSize = nVertexSize;

		pDev->SetStreamSource( 0, pVB, 0, nVertexSize );

#if defined ( DX_TO_GL_ABSTRACTION )
		pDev->SetIndices( g_pIB );
#endif
	}
}

//--------------------------------------------------------------------------------------------------
// Clear
//--------------------------------------------------------------------------------------------------

static D3DCOLOR s_color;

void PanDxClearColor( uint8 r, uint8 g, uint8 b, uint8 a )
{
	s_color = D3DCOLOR_ARGB( a, r, g, b );
}

// clear should be treated like a draw call, i.e. flush any draw calls, and commit the clear immediately
void PanDxClearBuffers( bool bColor, bool bDepth, bool bStencil )
{
	if ( !g_pdxInit ) return;

	DWORD mask = 0;

	if ( bColor )
	{
		mask |= D3DCLEAR_TARGET;
	}

	if ( bDepth )
	{
		mask |= D3DCLEAR_ZBUFFER;
	}

	if ( bStencil )
	{
		mask |= D3DCLEAR_STENCIL;
	}

	if ( mask == 0 )
		return;

	// next batch entry to use these values
 	sPanDxRenderState *pBatchRS = PanDxGetBatchEntry( PANDX_RSBATCHTYPE_CLEAR );

 	pBatchRS->m_clearColor = s_color;
 	pBatchRS->m_clearMask = mask;

	//Msg( "\n%d: Set Clear %d, %d", g_nMsgCounter++, mask, s_color );

	// clear similar to a draw => end batch entry
	PanDxEndBatchEntry();
}

void PanDxCommitClearBuffers( IDirect3DDevice9* pDev, sPanDxRenderState *pSrcRS )
{
	if ( !g_pdxInit ) return;

	//Msg( "\nCommit Clear %d, %d ", pSrcRS->m_clearMask, pSrcRS->m_clearColor );

	g_PanDxShadowRenderState.m_clearColor = pSrcRS->m_clearColor;
	g_PanDxShadowRenderState.m_clearMask = pSrcRS->m_clearMask;

	PanDxFlushTrisList();

	pDev->Clear( 0, 0, pSrcRS->m_clearMask, pSrcRS->m_clearColor, 0, 0 );
	PANDX_BROKE_BATCHING( ClearRenderTarget );
}

//--------------------------------------------------------------------------------------------------
// Viewport
//--------------------------------------------------------------------------------------------------

void PanDxSetViewPort( int x, int y, int w, int h, float minZ, float maxZ )
{
	if ( !g_pdxInit ) return;

	sPanDxRenderState *pBatchRS = PanDxGetBatchEntry( PANDX_RSBATCHTYPE_VP );

	D3DVIEWPORT9 *pVP = &pBatchRS->m_vp;
	pVP->X = x;
	pVP->Y = y;
	pVP->Width = w;
	pVP->Height = h;
	pVP->MinZ = minZ;
	pVP->MaxZ = maxZ;

	V_memcpy( &g_PanDxLastViewport, &pBatchRS->m_vp, sizeof( D3DVIEWPORT9 ) );

	//Msg( "\nSet Viewport %d, %d, %d, %d", x, y, w, h );
}

void PanDxCommitViewPort( IDirect3DDevice9* pDev, sPanDxRenderState *pSrcRS )
{
	if ( !g_pdxInit ) return;

	// lazy eval
	if ( V_memcmp( &pSrcRS->m_vp, &g_PanDxShadowRenderState.m_vp, sizeof( D3DVIEWPORT9 ) ) )
	{
		PanDxFlushTrisList();

		V_memcpy( &g_PanDxShadowRenderState.m_vp, &pSrcRS->m_vp, sizeof( D3DVIEWPORT9 ) );

		pDev->SetViewport( &pSrcRS->m_vp );
		PANDX_BROKE_BATCHING( SetViewport );

		//Msg( "\nCommit Viewport %d, %d, %d, %d ", pSrcRS->m_vp.X, pSrcRS->m_vp.Y, pSrcRS->m_vp.Width, pSrcRS->m_vp.Height );
	}
}

void PanDxGetViewPort( int &x, int &y, int &w, int &h, float &minZ, float &maxZ )
{
	x = g_PanDxLastViewport.X;
	y = g_PanDxLastViewport.Y;
	w = g_PanDxLastViewport.Width;
	h = g_PanDxLastViewport.Height;
	minZ = g_PanDxLastViewport.MinZ;
	maxZ = g_PanDxLastViewport.MaxZ;
}

//--------------------------------------------------------------------------------------------------
// Scissor
//--------------------------------------------------------------------------------------------------

void PanDxSetScissor( int nCount, const Rect_t *pRects )
{
	if ( !g_pdxInit ) return;

	if ( nCount && pRects )
	{
		sPanDxRenderState *pBatchRS = PanDxGetBatchEntry( PANDX_RSBATCHTYPE_SCISSOR | PANDX_RSBATCHTYPE_SCISSORENABLE );
		RECT r;
		r.top = pRects->y;
		r.left = pRects->x;
		r.right = pRects->x + pRects->width;
		r.bottom = pRects->y + pRects->height;

		pBatchRS->m_bScissorEnabled = true;
		pBatchRS->m_scissor = r;

		//Msg("\n%d: Set Scissor, Enable true, %d, %d, %d, %d ", g_nMsgCounter++, r.top, r.left, r.right, r.bottom);
	}
	else
	{
		sPanDxRenderState *pBatchRS = PanDxGetBatchEntry( PANDX_RSBATCHTYPE_SCISSORENABLE );
		pBatchRS->m_bScissorEnabled = false;

		//Msg( "\n%d: Set ScissorEnable, false ", g_nMsgCounter++ );
	}
}

void PanDxInvalidateRT()
{
	// set to dummy/invalid ptr, null used for backbuffer
	g_PanDxShadowRenderState.m_pRT = (IDirect3DTexture9 *)0xffffffff;

	// set empty mask
	g_PanDxShadowRenderState.m_clearMask = 0;
	g_PanDxShadowRenderState.m_clearColor = 0xffffffff;
}

void PanDxInvalidateViewportAndScissor()
{
	if ( !g_pdxInit ) return;

	IDirect3DDevice9* pDev = g_pPanoramaUIEngineImpl->GetD3Device();

	pDev->GetViewport( &g_PanDxLastViewport );
	V_memcpy( &g_PanDxShadowRenderState.m_vp, &g_PanDxLastViewport, sizeof( D3DVIEWPORT9 ) );

	pDev->GetScissorRect( &g_PanDxShadowRenderState.m_scissor );
}

void PanDxCommitScissor( IDirect3DDevice9* pDev, sPanDxRenderState *pSrcRS )
{
	if ( !g_pdxInit ) return;

	if ( pSrcRS->m_nFlags & PANDX_RSBATCHTYPE_SCISSORENABLE )
	{
		// lazy eval
		if ( pSrcRS->m_bScissorEnabled != g_PanDxShadowRenderState.m_bScissorEnabled )
		{
			PanDxFlushTrisList();

			g_PanDxShadowRenderState.m_bScissorEnabled = pSrcRS->m_bScissorEnabled;
			pDev->SetRenderState( D3DRS_SCISSORTESTENABLE, pSrcRS->m_bScissorEnabled );
			PANDX_BROKE_BATCHING( SetScissorEnable );

			//Msg( "\n%d:    Commit ScissorEnable %s, ", g_nMsgCounter2, pSrcRS->m_bScissorEnabled ? "true" : "false" );
		}
	}

	if ( pSrcRS->m_nFlags & PANDX_RSBATCHTYPE_SCISSOR )
	{
		// lazy eval
 		if ( V_memcmp( &pSrcRS->m_scissor, &g_PanDxShadowRenderState.m_scissor, sizeof( RECT ) ) )
		{
			PanDxFlushTrisList();

			g_PanDxShadowRenderState.m_scissor = pSrcRS->m_scissor;
			pDev->SetScissorRect( &pSrcRS->m_scissor );
			PANDX_BROKE_BATCHING( SetScissorRect );

			//Msg( "\nCommit Scissor %d, %d, %d, %d ", pSrcRS->m_scissor.top, pSrcRS->m_scissor.left, pSrcRS->m_scissor.right, pSrcRS->m_scissor.bottom );
		}
	}
}

//--------------------------------------------------------------------------------------------------
// Render Target
//--------------------------------------------------------------------------------------------------

void PanDxSetRenderTarget( ITexture* pTexture )		// Pass NULL to set the backbuffer.
{
	if ( !g_pdxInit ) return;

	sPanDxRenderState *pBatchRS = PanDxGetBatchEntry( PANDX_RSBATCHTYPE_RT );

	if ( !pTexture )
	{
		pBatchRS->m_pRT = nullptr;
	}
	else
	{
		pBatchRS->m_pRT = (IDirect3DTexture9 *)g_pMaterialSystem->GetPanormaTexturePtr( pTexture );
	}

	//Msg("\n%d: Set RenderTarget %d", g_nMsgCounter++, (int)pBatchRS->m_pRT );
}

void PanDxCommitRenderTarget( IDirect3DDevice9* pDev, sPanDxRenderState *pSrcRS )
{
	if ( !g_pdxInit ) return;

	if ( pSrcRS->m_pRT == (IDirect3DTexture9 *)0xffffffff )
		return;

	// lazy eval
  	if ( pSrcRS->m_pRT == g_PanDxShadowRenderState.m_pRT )
 		return;

	//Msg( "\nCommit RenderTarget %d ", (int)pSrcRS->m_pRT );

	PanDxFlushTrisList();

	PANDX_BROKE_BATCHING( SetRenderTarget );

	g_PanDxShadowRenderState.m_pRT = pSrcRS->m_pRT;

#ifndef DX_TO_GL_ABSTRACTION

	if ( pSrcRS->m_pRT == nullptr )
	{
		pDev->SetRenderTarget( 0, gpBackBuffer );
	}
	else
	{
 		IDirect3DSurface9* pSurf = NULL;
 		pSrcRS->m_pRT->GetSurfaceLevel( 0, &pSurf );
		pDev->SetRenderTarget( 0, pSurf );
		pSurf->Release();
		pDev->SetDepthStencilSurface( 0 );
	}

	// committing a render target to D3D will also invalidate the viewport settings (they are set by D3D to the max extents of the render target)
	// => invalidate the shadow state for viewport and scissor

	PanDxInvalidateViewportAndScissor();

#else

	uint32 w;
	uint32 h;

	if ( pSrcRS->m_pRT == nullptr )
	{
		pDev->SetRenderTarget( 0, gpBackBuffer );
		w = gpBackBuffer->m_desc.Width;
		h = gpBackBuffer->m_desc.Height;
	}
	else
	{
 		IDirect3DSurface9* pSurf = NULL;
 		pSrcRS->m_pRT->GetSurfaceLevel( 0, &pSurf );
		pDev->SetRenderTarget( 0, pSurf );
		pSurf->Release();
		pDev->SetDepthStencilSurface( 0 );
		w = pSurf->m_desc.Width;
		h = pSurf->m_desc.Height;
	}

	// alternatively explicitly set viewport and scissor to known/sensible states for the render target

	D3DVIEWPORT9 VP;
	VP.X = 0;
	VP.Y = 0;
	VP.Width = w;
	VP.Height = h;
	VP.MinZ = 0;
	VP.MaxZ = 1;
	pDev->SetViewport( &VP );
	V_memcpy( &g_PanDxShadowRenderState.m_vp, &VP, sizeof( D3DVIEWPORT9 ) );

	RECT r;
	r.top = 0;
	r.left = 0;
	r.right = w;
	r.bottom = h;
	pDev->SetScissorRect( &r );
	g_PanDxShadowRenderState.m_scissor = r;

	// shouldn't need this
//	pDev->SetRenderState( D3DRS_SCISSORTESTENABLE, true );
//	g_PanDxShadowRenderState.m_bScissorEnabled = true;

#endif

}

//--------------------------------------------------------------------------------------------------
// Batch flush functions
//
// Flushing will 
//
// 1. Unlock the active VB
// 2. Set render target associated state if present ( Render Target, Viewport, Scissor, Clear)
// 3. Set draw call state and make drawcall if present (vertex decl, stream/VB, sampler, sampler state, shaders, consts)
//
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Flush batched quads 
//--------------------------------------------------------------------------------------------------

void PanDxFlushBasicQuads()
{
	if ( !g_pdxInit ) return;

	if ( IsPanDxBatchEmpty() )
		return;

	IDirect3DDevice9* pDev = g_pPanoramaUIEngineImpl->GetD3Device();

	// lock, fill, unlock VB
	bool bVBValid = PanDxFillBasicVB();

	//
	// Commit render states and draw
	//

	PanDxCommitVertexDecl( pDev, g_pVertexDeclBasic );	
	PanDxCommitStreamSource( pDev, g_pVB[ g_nVBBuffer ], k_nBasicVertSize );

	for ( uint32 i = 0; i < g_nPanDxBatchIdx; i++ )
	{
		sPanDxRenderState *pBatchRS = &g_aPanDxBatchedRenderState[ i ];

		//
		// Set render states
		//

		if ( pBatchRS->m_nFlags & PANDX_RSBATCHTYPE_RT )
			PanDxCommitRenderTarget( pDev, pBatchRS );

		if ( pBatchRS->m_nFlags & PANDX_RSBATCHTYPE_VP )
			PanDxCommitViewPort( pDev, pBatchRS );

		if ( pBatchRS->m_nFlags & ( PANDX_RSBATCHTYPE_SCISSOR | PANDX_RSBATCHTYPE_SCISSORENABLE ) )
			PanDxCommitScissor( pDev, pBatchRS );

		if ( pBatchRS->m_nFlags & PANDX_RSBATCHTYPE_CLEAR )
			PanDxCommitClearBuffers( pDev, pBatchRS );

		if ( pBatchRS->m_nNumQuads && bVBValid )
		{
			PanDxCommitTexturesBasic( pDev, pBatchRS );
			PanDxCommitPerDrawRs( pDev, pBatchRS );
			PanDxCommitShaders( pDev, pBatchRS );
			PanDxCommitPSConstsBasic( pDev, pBatchRS );

			PANDX_STATS_BASIC();

			//
			// draw (only quad at a time for basic)
			//
#ifndef DX_TO_GL_ABSTRACTION
			pDev->DrawPrimitive( D3DPT_TRIANGLEFAN, pBatchRS->m_nVBIndex, 2 );
#else
			pDev->DrawIndexedPrimitive( D3DPT_TRIANGLESTRIP, 0, pBatchRS->m_nVBIndex, NUM_VERTS_PER_BASICQUAD, pBatchRS->m_nVBIndex, 2 );
#endif

			PANDX_BROKE_BATCHING( PrimType );
		}

		// clear batch entry
		PanDxResetBatchEntry( pBatchRS );
	}

	//
	// reset batching
	//

	PanDxResetBatching();
}

void PanDxFlushFancyQuads()
{
	if ( !g_pdxInit ) return;

	if ( IsPanDxBatchEmpty() )
		return;

	IDirect3DDevice9* pDev = g_pPanoramaUIEngineImpl->GetD3Device();

	// lock, fill, unlock VB here
	bool bVBValid = PanDxFillFancyVB();

	PanDxCommitVertexDecl( pDev, g_pVertexDeclFancy );
	PanDxCommitStreamSource( pDev, g_pVB[ g_nVBBuffer ], k_nFancyVertSize );

	PANDX_BROKE_BATCHING( PrimType );

	for ( uint32 i = 0; i < g_nPanDxBatchIdx; i++ )
	{
		sPanDxRenderState *pBatchRS = &g_aPanDxBatchedRenderState[ i ];
		
		//
		// Set render states
		//

		if ( pBatchRS->m_nFlags & PANDX_RSBATCHTYPE_RT )
			PanDxCommitRenderTarget( pDev, pBatchRS );

		if ( pBatchRS->m_nFlags & PANDX_RSBATCHTYPE_VP )
			PanDxCommitViewPort( pDev, pBatchRS );

		if ( pBatchRS->m_nFlags & ( PANDX_RSBATCHTYPE_SCISSOR | PANDX_RSBATCHTYPE_SCISSORENABLE ) )
			PanDxCommitScissor( pDev, pBatchRS );

		if ( pBatchRS->m_nFlags & PANDX_RSBATCHTYPE_CLEAR )
			PanDxCommitClearBuffers( pDev, pBatchRS );

		if ( pBatchRS->m_nNumQuads && bVBValid )
		{
			PanDxCommitTexturesFancy( pDev, pBatchRS );
			PanDxCommitPerDrawRs( pDev, pBatchRS );
			PanDxCommitShaders( pDev, pBatchRS );
			PanDxCommitPSConstsFancy( pDev, pBatchRS );

			PANDX_STATS_FANCY( pBatchRS->m_nNumQuads );

			//
			// batch draw, accumulate num quads
			// draw call of batched prims happens when we hit a change in render state at which point we flush any batched tris
			//
			PanDxAddToTriList( pBatchRS->m_nVBIndex, 2 * pBatchRS->m_nNumQuads );
		}

		// clear batch entry
		PanDxResetBatchEntry( pBatchRS );
	}

	//
	// flush trilist
	//

	PanDxFlushTrisList();
	
	//
	// reset batching
	//

	PanDxResetBatching();
}

//--------------------------------------------------------------------------------------------------
// Early flush checks
//
// bEarlyFlush true implies we might have part filled a batch (we're flushing due to end of frame for example) so need to end the entry here.
//--------------------------------------------------------------------------------------------------
void PanDxFlushBatch( bool bEarlyFlush )
{
	if ( !g_pdxInit )
	{
		PanDxResetBatching();
		return;
	}

	if ( bEarlyFlush )
	{
		uint32 drawTypeFlags = GetPanDxCurrentBatchEntryFlags() & ( PANDX_RSBATCHTYPE_BASIC | PANDX_RSBATCHTYPE_FANCY );
		uint32 rtTypeFlags = GetPanDxCurrentBatchEntryFlags() & ( PANDX_RSBATCHTYPE_CLEAR | PANDX_RSBATCHTYPE_SCISSOR | PANDX_RSBATCHTYPE_SCISSORENABLE | PANDX_RSBATCHTYPE_RT );

		if ( rtTypeFlags && ( drawTypeFlags == 0 ) )
		{
			// We might be part way filling in current batch entry (i.e. containing VP/RT/Clear only but no draw call).
			// Increment count before flushing so that we effectively finish that batch entry.
			g_nPanDxBatchIdx++;
		}
	}

	if ( IsPanDxBatchEmpty() )
		return;

	// which draw type was being batched in the queue when we hit this flush
	if ( GetPanDxAccumulatedBatchFlags() & PANDX_RSBATCHTYPE_BASIC )
	{
		PanDxFlushBasicQuads();
	}
	else
	{
		PanDxFlushFancyQuads();
	}
}

//--------------------------------------------------------------------------------------------------
// Save all state for a draw call
//
// Places the draw call state into a queue of batch entries (g_aPanDxBatchedRenderState)
//
// Batch queue is flushed when 
// 1. There is a change from fancy<-->basic rendering - requires a VB change (though this could be changed, in practice it doesn't look like a significant win)
// 2. When a (render target surface) clear is requested
// 3. When the current batch limit is reached
//
// NOTE - batch entry may or may not contain a draw call or render state since it's possible (looking at the above list) 
// that one or the other might not occur before a flush is needed.
//--------------------------------------------------------------------------------------------------

void PanDxSetBasicQuad( CRenderContext* pCtx, panorama::BasicQuad_t *pBasicQuad )
{
	if ( !g_pdxInit ) return;

	// pQuad must comes from a PanDxGetBasicQuadPtr, so check validity here
// 	if ( ( pBasicQuad < g_PanDxBasicQuadScratch ) ||
// 		( pBasicQuad > &g_PanDxBasicQuadScratch[ PANDX_MAX_RSBATCH - 1 ] ) )
// 	{
// 		DevWarning( "PanDxSetBasicQuad - bad pBasicQuad ptr (not from PanDx scratch mem)\n" );
// 		return;
// 	}

	//Msg( "\nSetBasic " );

	// get next batch entry
	sPanDxRenderState *pBatchRS = PanDxGetBatchEntry( PANDX_RSBATCHTYPE_BASIC );

	CRenderAttributes* pAttr = pCtx->m_pAttr;

	//
	// save render state
	//

	PanDxSetTexturesBasic( pBatchRS, pAttr );
	PanDxSetPerDrawRS( pBatchRS, pCtx->m_blendState );
	PanDxSetShadersBasic( pBatchRS, pAttr );
	PanDxSetPSConstsBasic( pBatchRS, pAttr );
	PanDxSetVertexDecl( pBatchRS, g_pVertexDeclBasic );
	PanDxSetStreamSource( pBatchRS, g_pVB[ g_nVBBuffer ], k_nBasicVertSize, pBasicQuad, 1 );

	//
	// increment batch index here
	//

	PanDxEndBatchEntry();
}

void PanDxSetFancyQuad( CRenderContext* pCtx, panorama::FancyQuad_t *pFancyQuad, int nNumQuads )
{
	if ( !g_pdxInit ) return;

	if ( nNumQuads > PANDX_MAX_PAN_QUADS ) return;

	// pQuad must comes from a PanDxGetBasicQuadPtr, so check validity here
// 	if ( ( pFancyQuad < g_PanDxFancyQuadScratch ) ||
// 		( pFancyQuad > &g_PanDxFancyQuadScratch[ ( PANDX_MAX_RSBATCH + 1 ) * 4 ] ) )
// 	{
// 		DevWarning( "PanDxSetFancyQuad - bad pFancyQuad ptr (not from PanDx scratch mem)\n" );
// 		return;
// 	}

	//Msg( "\nSetFancy " );

	// get next batch entry
	sPanDxRenderState *pBatchRS = PanDxGetBatchEntry( PANDX_RSBATCHTYPE_FANCY );

	CRenderAttributes* pAttr = pCtx->m_pAttr;

	//
	// save render state
	//

	PanDxSetTexturesFancy( pBatchRS, pAttr );
	PanDxSetPerDrawRS( pBatchRS, pCtx->m_blendState );
	PanDxSetShadersFancy( pBatchRS, pAttr );
	PanDxSetPSConstsFancy( pBatchRS, pAttr );
	PanDxSetVertexDecl( pBatchRS, g_pVertexDeclFancy );
	PanDxSetStreamSource( pBatchRS, g_pVB[ g_nVBBuffer ], k_nFancyVertSize, pFancyQuad, nNumQuads );

	//
	// increment batch index here
	//

	PanDxEndBatchEntry();
}

//--------------------------------------------------------------------------------------------------
// DrawCalls
//
// Calls are placed in a queue/batch in order to optimize VB locks/unlocks without copying too much data around
// 
// 
// NOTE:
// PanDxSetXXX calls are used to ensure state is captured in the queue
// State is committed to DX via PanDxCommitXXX calls when the queue is flushed
//--------------------------------------------------------------------------------------------------
void PanDxDrawBasicQuad( CRenderContext* pCtx, panorama::BasicQuad_t *pBasicQuad )
{
	//VPROF_BUDGET( "PanDxDrawBasicQuad", VPROF_BUDGETGROUP_GAME );
	if ( !g_pdxInit ) return;

	// batch
	PanDxSetBasicQuad( pCtx, pBasicQuad );
}

void PanDxDrawFancyQuads( CRenderContext* pCtx, panorama::FancyQuad_t *pFancyQuad, int nNumQuads )
{
	//VPROF_BUDGET( "PanDxDrawFancyQuads", VPROF_BUDGETGROUP_GAME );
	if ( !g_pdxInit ) return;

	if ( pCtx->m_blendState == RsBlendStateHandle_t::BLENDSTATE_MIX_ADDITIVESRGB )
	{
		// Move all colors to SRGB space

		for( int i = 0; i < nNumQuads; i++ )
		{
			panorama::FancyQuad_t &q = pFancyQuad[ i ];

			if ( q.m_vColor.w > 0.0f )
			{
				q.m_vColor = VecColorPreDivAlpha( q.m_vColor );
				q.m_vColor = VecLinearToSrgb( q.m_vColor );
				q.m_vColor = VecColorPreMulAlpha( q.m_vColor );
			}	

			if ( q.m_vColorStop.w > 0.0f )
			{
				q.m_vColorStop = VecColorPreDivAlpha( q.m_vColorStop );
				q.m_vColorStop = VecLinearToSrgb( q.m_vColorStop );
				q.m_vColorStop = VecColorPreMulAlpha( q.m_vColorStop );
			}

		}

	}

	// batch
	PanDxSetFancyQuad( pCtx, pFancyQuad, nNumQuads );
}


void PanDxCopyBackBuffer( void* pSurf, float x0, float y0, float x1, float y1 )
{
	if ( !g_pdxInit ) return;

	PanDxFlushBatch( true );

	IDirect3DDevice9* pDev = g_pPanoramaUIEngineImpl->GetD3Device();
	RECT destRect;
	destRect.left = RoundFloatToInt( x0 );
	destRect.right = RoundFloatToInt( x1 );
	destRect.left = Max( 0, (int)destRect.left );
	destRect.right = Max( destRect.left, destRect.right );

	destRect.top = RoundFloatToInt( y0 );
	destRect.bottom = RoundFloatToInt( y1 );
	destRect.top = Max( 0, (int)destRect.top );
	destRect.bottom = Max( destRect.top, destRect.bottom );

	HRESULT hr = pDev->StretchRect(gpBackBuffer, NULL, (IDirect3DSurface9*)pSurf, &destRect, D3DTEXF_LINEAR );

	if ( hr != D3D_OK )
	{
		
		Error( "d3d stretchrect failed\n" );

	}
}

//--------------------------------------------------------------------------------------------------
// Capture and restore GL
//--------------------------------------------------------------------------------------------------
#ifdef DX_TO_GL_ABSTRACTION

DWORD g_PanDxAllRs[] =
{
	D3DRS_ZENABLE, D3DRS_FILLMODE, D3DRS_SHADEMODE, D3DRS_ZWRITEENABLE, D3DRS_ALPHATESTENABLE, D3DRS_LASTPIXEL, D3DRS_SRCBLEND, D3DRS_DESTBLEND, D3DRS_CULLMODE,
	D3DRS_ZFUNC, D3DRS_ALPHAREF, D3DRS_ALPHAFUNC, D3DRS_DITHERENABLE, D3DRS_ALPHABLENDENABLE, D3DRS_FOGENABLE, D3DRS_SPECULARENABLE, D3DRS_FOGCOLOR, D3DRS_FOGTABLEMODE,
	D3DRS_FOGSTART, D3DRS_FOGEND, D3DRS_FOGDENSITY, D3DRS_RANGEFOGENABLE, D3DRS_STENCILENABLE, D3DRS_STENCILFAIL, D3DRS_STENCILZFAIL, D3DRS_STENCILPASS, D3DRS_STENCILFUNC,
	D3DRS_STENCILREF, D3DRS_STENCILMASK, D3DRS_STENCILWRITEMASK, D3DRS_TEXTUREFACTOR, D3DRS_WRAP0, D3DRS_WRAP1, D3DRS_WRAP2, D3DRS_WRAP3, D3DRS_WRAP4, D3DRS_WRAP5,
	D3DRS_WRAP6, D3DRS_WRAP7, D3DRS_CLIPPING, D3DRS_LIGHTING, D3DRS_AMBIENT, D3DRS_FOGVERTEXMODE, D3DRS_COLORVERTEX, D3DRS_LOCALVIEWER, D3DRS_NORMALIZENORMALS,
	D3DRS_DIFFUSEMATERIALSOURCE, D3DRS_SPECULARMATERIALSOURCE, D3DRS_AMBIENTMATERIALSOURCE, D3DRS_EMISSIVEMATERIALSOURCE, D3DRS_VERTEXBLEND, D3DRS_CLIPPLANEENABLE,
	D3DRS_POINTSIZE, D3DRS_POINTSIZE_MIN, D3DRS_POINTSPRITEENABLE, D3DRS_POINTSCALEENABLE, D3DRS_POINTSCALE_A, D3DRS_POINTSCALE_B, D3DRS_POINTSCALE_C,
	D3DRS_MULTISAMPLEANTIALIAS, D3DRS_MULTISAMPLEMASK, D3DRS_PATCHEDGESTYLE, D3DRS_DEBUGMONITORTOKEN, D3DRS_POINTSIZE_MAX, D3DRS_INDEXEDVERTEXBLENDENABLE,
	D3DRS_COLORWRITEENABLE, D3DRS_TWEENFACTOR, D3DRS_BLENDOP, D3DRS_POSITIONDEGREE, D3DRS_NORMALDEGREE, D3DRS_SCISSORTESTENABLE, D3DRS_SLOPESCALEDEPTHBIAS,
	D3DRS_ANTIALIASEDLINEENABLE, D3DRS_MINTESSELLATIONLEVEL, D3DRS_MAXTESSELLATIONLEVEL, D3DRS_ADAPTIVETESS_X, D3DRS_ADAPTIVETESS_Y, D3DRS_ADAPTIVETESS_Z,
	D3DRS_ADAPTIVETESS_W, D3DRS_ENABLEADAPTIVETESSELLATION, D3DRS_TWOSIDEDSTENCILMODE, D3DRS_CCW_STENCILFAIL, D3DRS_CCW_STENCILZFAIL, D3DRS_CCW_STENCILPASS,
	D3DRS_CCW_STENCILFUNC, D3DRS_COLORWRITEENABLE1, D3DRS_COLORWRITEENABLE2, D3DRS_COLORWRITEENABLE3, D3DRS_BLENDFACTOR, D3DRS_SRGBWRITEENABLE, D3DRS_DEPTHBIAS,
	D3DRS_WRAP8, D3DRS_WRAP9, D3DRS_WRAP10, D3DRS_WRAP11, D3DRS_WRAP12, D3DRS_WRAP13, D3DRS_WRAP14, D3DRS_WRAP15, D3DRS_SEPARATEALPHABLENDENABLE, D3DRS_SRCBLENDALPHA,
	D3DRS_DESTBLENDALPHA, D3DRS_BLENDOPALPHA
};

Vector4D				g_SavePixelShaderConstantF[ 13 ];
IDirect3DIndexBuffer9*	g_SaveIndices;
IDirect3DBaseTexture9*	g_SaveTextures[ 16 + 4 ]; // max sampler count

IDirect3DVertexBuffer9* g_SaveStreamData;
UINT					g_SaveOffsetInBytes;
UINT					g_SaveStride;
IDirect3DVertexDeclaration9* g_SaveVertexDeclaration;

IDirect3DVertexShader9*	g_SaveVertexShader;
IDirect3DPixelShader9*	g_SavePixelShader;


void PanDxCaptureGlState()
{
	// We don't test for lost device/g_pdxInit here because we are called from OwndDx which has already tested it

	IDirect3DDevice9* pDev = g_pPanoramaUIEngineImpl->GetD3Device();
	
	// Scissor, viewport, rendertarget and depth stencil already handled by the DX code for the shared w/ non GL path
	
	pDev->GetPixelShaderConstantF( 0, g_SavePixelShaderConstantF[ 0 ].Base(), ARRAYSIZE( g_SavePixelShaderConstantF ) );

	pDev->GetIndices( &g_SaveIndices );

	for ( int i = 0; i < pDev->GetTotalSamplerCount(); i++ )
	{
		pDev->GetTexture( i, &g_SaveTextures[ i ] );
	}

	pDev->GetStreamSource( 0, &g_SaveStreamData, &g_SaveOffsetInBytes, &g_SaveStride );
			
	pDev->GetVertexDeclaration( &g_SaveVertexDeclaration );
	
	pDev->GetVertexShader(&g_SaveVertexShader);
	pDev->GetPixelShader( &g_SavePixelShader );

	pDev->SetCaptureMode( RS_CAPTURE_MODE_PANORAMA );
}

void PanDxRestoreGlState()
{
	IDirect3DDevice9* pDev = g_pPanoramaUIEngineImpl->GetD3Device();

	pDev->SetPixelShaderConstantF( 0, g_SavePixelShaderConstantF[ 0 ].Base(), ARRAYSIZE( g_SavePixelShaderConstantF ) );

	pDev->SetIndices( g_SaveIndices );
	if ( g_SaveIndices ) g_SaveIndices->Release();

	for ( int i = 0; i < pDev->GetTotalSamplerCount(); i++ )
	{
		pDev->SetTexture( i, g_SaveTextures[ i ] );
		if ( g_SaveTextures[ i ] ) g_SaveTextures[ i ]->Release();
	}

	pDev->SetStreamSource( 0, g_SaveStreamData, g_SaveOffsetInBytes, g_SaveStride );
	if ( g_SaveStreamData ) g_SaveStreamData->Release();

	pDev->SetVertexDeclaration( g_SaveVertexDeclaration );
	if ( g_SaveVertexDeclaration ) g_SaveVertexDeclaration->Release();

	pDev->SetVertexShader( g_SaveVertexShader );
	if ( g_SaveVertexShader ) g_SaveVertexShader->Release();
	pDev->SetPixelShader( g_SavePixelShader );
	if ( g_SavePixelShader ) g_SavePixelShader->Release();

	// Ensure shaderapi render, sampler and srgb states are on the d3ddevice

	//uint32* pRenderStates;
	//SamplerStateCopy_t* pSamplerStates;
	//bool srgbWrite;

	// Could use this call to debug state reset, or to perhaps fix loose srgb amd bug ?
	// g_pMaterialSystem->GetShaderApiDynamicState( &pRenderStates, &pSamplerStates, &srgbWrite );

	// Renderstates ( including srgb .. )
	for ( int i = 0; i < ARRAYSIZE( pDev->m_RsShadow ); i++ )
	{
		// If both game and panorama 
		if ( pDev->m_RsShadow[ i ].nCaptureMode & RS_CAPTURE_MODE_GAME )
		{
			pDev->SetRenderState( (D3DRENDERSTATETYPE)i, pDev->m_RsShadow[ i ].value );
		}
	}
	
	// Sampler States
	for ( int i = 0; i < pDev->GetTotalSamplerCount(); i++ )
 	{
		for ( int j = 0; j < ARRAYSIZE( pDev->m_SamplerStateShadow[ i ] ); j++ )
		{
			if ( pDev->m_SamplerStateShadow[i][j].nCaptureMode & RS_CAPTURE_MODE_GAME )
			{
				pDev->SetSamplerState( i, (D3DSAMPLERSTATETYPE)j, (D3DSAMPLERSTATETYPE)pDev->m_SamplerStateShadow[ i ][ j ].value );
			}
		}
 	}

	pDev->SetCaptureMode( RS_CAPTURE_MODE_GAME );

}

#endif

#endif





