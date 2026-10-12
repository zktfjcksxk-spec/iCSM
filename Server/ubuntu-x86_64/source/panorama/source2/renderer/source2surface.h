//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef SOURCE2SURFACE_H
#define SOURCE2SURFACE_H
#pragma once

#include "../../renderer/iui3dsurface.h"
#include "iuirenderengine.h"
#include "mathlib/vmatrix.h"
#include "tier1/utlsymbol.h"
#include "tier1/utlmap.h"
#include "tier1/utllinkedlist.h"
#include "panorama/iuiengine.h"
#include "panorama/text/iuitextlayout.h"
#include "panorama/data/iimagesource.h"
#include "panorama/panoramatypes.h"
#include "../../input/mousecursor.h"
#include "resourcesystem/iresourcesystem.h"

namespace panorama
{

struct RectBounds_t
{
	float left;
	float right;
	float top;
	float bottom;
};

struct BasicQuad_t
{
	Vector4D m_vColor;
	Vector2D m_vPosition[ 4 ];
	Vector2D m_vUV[ 4 ];
	float	 m_flZ;

	inline void BqInit(Vector2D topLeft, Vector2D bottomRight)
	{
		m_flZ = 0.0f;
		m_vColor = Vector4D( 1.0, 1.0, 1.0, 1.0 );

		m_vPosition[ 0 ] = topLeft;
		m_vPosition[ 1 ] = Vector2D( bottomRight.x, topLeft.y );
		m_vPosition[ 2 ] = bottomRight;
		m_vPosition[ 3 ] = Vector2D( topLeft.x, bottomRight.y );

		m_vUV[ 0 ] = Vector2D( 0.0f, 0.0f );
		m_vUV[ 1 ] = Vector2D( 1.0f, 0.0f );
		m_vUV[ 2 ] = Vector2D( 1.0f, 1.0f );
		m_vUV[ 3 ] = Vector2D( 0.0f, 1.0f );
	}

	inline void BqInit( int x, int y, int w, int h )
	{
		BqInit( Vector2D( x, y ), Vector2D( x + w, y + h ) );
	}

	inline void BqInit( Vector4D color, Vector2D *postion, Vector2D *uv, float z )
	{
		m_vColor = color;
		memcpy( m_vPosition, postion, sizeof( m_vPosition ) );
		memcpy( m_vUV, uv, sizeof( m_vUV ) );
		m_flZ = z;
	}

};

struct ImageShadowLayerParams_t
{	
	float flImgWidth;	// Width and height given by the RenderTexturedRectRenderCommand_t
	float flImgHeight;	// top_left & bottom_right
	Vector2D m_vUV[ 2 ];
	float flBlurRadius;
	float flStrength;
	uint32 color;
	uint32 textureSampleMode;		// do not change to ETextureSampleMode (uint8) 
									// as it would add padding and memcmp might fail)
	HRenderTexture hRenderTexture;

	// Used by CCompositionLayerCache to sort layers
	bool operator <( const ImageShadowLayerParams_t &l ) const
	{
		return ( V_memcmp( this, &l, sizeof( ImageShadowLayerParams_t ) ) < 0);
	}
};

struct OuterShadowLayerParams_t
{
	float flWidthLayer;
	float flHeightLayer;
	float borderRadii[8];
	float flHorOffset;
	float flVerOffset;
	float flBlurRadius;
	float flSpreadDistance;
	uint32 shadowColor;
	bool bFill;

	// Used by CCompositionLayerCache to sort layers
	bool operator <( const OuterShadowLayerParams_t &l ) const
	{
		if ( flWidthLayer != l.flWidthLayer )
		{
			return flWidthLayer < l.flWidthLayer;
		}
		if ( flHeightLayer != l.flHeightLayer )
		{
			return flHeightLayer < l.flHeightLayer;
		}
		if ( flBlurRadius != l.flBlurRadius )
		{
			return flBlurRadius < l.flBlurRadius;
		}
		if ( flSpreadDistance != l.flSpreadDistance )
		{
			return flSpreadDistance < l.flSpreadDistance;
		}
		if ( shadowColor != l.shadowColor )
		{
			return shadowColor < l.shadowColor;
		}
		if ( flHorOffset != l.flHorOffset )
		{
			return flHorOffset < l.flHorOffset;
		}
		if ( flVerOffset != l.flVerOffset )
		{
			return flVerOffset < l.flVerOffset;
		}
		for ( int i = 0; i < 8; ++i )
		{
			if ( borderRadii[i] != l.borderRadii[i] )
			{
				return borderRadii[i] < l.borderRadii[i];
			}
		}

		return bFill < l.bFill;
	}
};

struct TextShadowKey_t
{
	bool operator <( const TextShadowKey_t &l ) const
	{
		if ( uniqueID != l.uniqueID )
		{
			return (uniqueID < l.uniqueID);
		}

		if ( !CloseEnough( data.horizontal_offset, l.data.horizontal_offset ) )
		{
			return (data.horizontal_offset < l.data.horizontal_offset);
		}

		if ( !CloseEnough( data.vertical_offset, l.data.vertical_offset ) )
		{
			return (data.vertical_offset < l.data.vertical_offset);
		}

		if ( !CloseEnough( data.blur_radius, l.data.blur_radius ) )
		{
			return (data.blur_radius < l.data.blur_radius);
		}

		if ( !CloseEnough( data.strength, l.data.strength ) )
		{
			return (data.strength < l.data.strength);
		}

		if ( !CloseEnough( data.color, l.data.color ) )
		{
			return (data.color < l.data.color);
		}

		return (data.animating < l.data.animating);
	}

	int					uniqueID;
	TextShadowData_t	data;
};


#define FANCYQUAD_MAXSTOPS 16
#define FANCYQUAD_GRADIENT_TEXTURE_SIZE 1024

enum FancyQuadTextureType_t
{
	// NOTE: Must match #defines in panorama_fancyquad.vfx shader
	FancyQuadTextureType_None = 0,
	FancyQuadTextureType_RGBA = 1,
	FancyQuadTextureType_Alpha = 2,
	FancyQuadTextureType_YUV = 3,
	FancyQuadTextureType_YCoCg = 4,
	FancyQuadTextureType_Total = 5,
};

enum FancyQuadFlag_t
{
	FancyQuadFlag_RadialGradient = ( 1 << 0 ),
	FancyQuadFlag_OuterCorner = ( 1 << 1 ),
	FancyQuadFlag_InnerCorner = ( 1 << 2 ),
	FancyQuadFlag_ColorCorrection = ( 1 << 3 ),
	FancyQuadFlag_OpacityMask = ( 1 << 4 ),
	FancyQuadFlag_GradientTwoStop = ( 1 << 5 ),
	FancyQuadFlag_GradientComplex = ( 1 << 6 ),
	FancyQuadFlag_RadialClip = ( 1 << 7 ),
	FancyQuadFlag_HasBorder = ( 1 << 8 ),
	FancyQuadFlag_Count = 9,
};

struct FancyQuad_t
{
	struct Vert_t
	{
		Vector2D m_vPosition;
		Vector2D m_vUV;
		Vector2D m_vUVGradient;
		Vector2D m_vUVOpacity;
		Vector4D m_vFragCoordWdHt;
	} m_Verts[ 4 ];

	Vector2D m_avOuterCorner[ 4 ];
	Vector2D m_avInnerCorner[ 4 ];
	Vector4D m_vColor;
	Vector4D m_vColorStop;

	float	m_flZ;
	uint32	m_nFlags; // Always have color, pos, uv, z, others are flagged from FancyQuadFlag_t

	inline void RoundPosToInt()
	{
		for ( Vert_t &vert : m_Verts )
		{
			vert.m_vPosition.x = RoundFloatToInt( vert.m_vPosition.x );
			vert.m_vPosition.y = RoundFloatToInt( vert.m_vPosition.y );
		}
	}

	inline void FqInit( Vector2D topLeft, Vector2D bottomRight, Vector2D topLeftUV, Vector2D bottomRightUV )
	{
		memset( this, 0, sizeof( *this ) );


		m_nFlags = 0;
		m_flZ = 0.0f;
		m_vColor = Vector4D( 1.0, 1.0, 1.0, 1.0 );

		m_Verts[ 0 ].m_vPosition = topLeft;
		m_Verts[ 1 ].m_vPosition = Vector2D( bottomRight.x, topLeft.y );
		m_Verts[ 2 ].m_vPosition = bottomRight;
		m_Verts[ 3 ].m_vPosition = Vector2D( topLeft.x, bottomRight.y );

		m_Verts[ 0 ].m_vUV = topLeftUV;
		m_Verts[ 2 ].m_vUV = bottomRightUV;
		

		Vector2D vExtentUV = bottomRightUV - topLeftUV;
		// Detect which texture coordinate axis is along the width of the quad 
		if ( vExtentUV.x * vExtentUV.y > 0.0f )
		{
			// U along width of quad
			m_Verts[ 1 ].m_vUV = Vector2D( bottomRightUV.x, topLeftUV.y );
			m_Verts[ 3 ].m_vUV = Vector2D( topLeftUV.x, bottomRightUV.y );
		}
		else
		{
			// V along width of quad
			// uvs rotated - v changing along the width of the panel
			m_Verts[1].m_vUV = Vector2D( topLeftUV.x, bottomRightUV.y );
			m_Verts[3].m_vUV = Vector2D( bottomRightUV.x, topLeftUV.y );
		}
	}

	inline void FqInit( Vector2D topLeft, Vector2D bottomRight, Vector2D topLeftUV, Vector2D bottomRightUV, Vector2D topLeftOpUV, Vector2D bottomRightOpUV )
	{
		FqInit( topLeft, bottomRight, topLeftUV, bottomRightUV );

		m_Verts[ 0 ].m_vUVOpacity = topLeftOpUV;
		m_Verts[ 1 ].m_vUVOpacity = Vector2D( bottomRightOpUV.x, topLeftOpUV.y );;
		m_Verts[ 2 ].m_vUVOpacity = bottomRightOpUV;
		m_Verts[ 3 ].m_vUVOpacity = Vector2D( topLeftOpUV.x, bottomRightOpUV.y );
	}


	inline void FqInit( Vector2D topLeft, Vector2D bottomRight )
	{
		memset( this, 0, sizeof( *this ) );

		m_nFlags = 0;
		m_flZ = 0.0f;
		m_vColor = Vector4D( 1.0, 1.0, 1.0, 1.0 );

		m_Verts[ 0 ].m_vPosition = topLeft;
		m_Verts[ 1 ].m_vPosition = Vector2D( bottomRight.x, topLeft.y );
		m_Verts[ 2 ].m_vPosition = bottomRight;
		m_Verts[ 3 ].m_vPosition = Vector2D( topLeft.x, bottomRight.y );
	
		m_Verts[ 0 ].m_vUV = Vector2D( 0.0f, 0.0f );
		m_Verts[ 1 ].m_vUV = Vector2D( 1.0f, 0.0f );
		m_Verts[ 2 ].m_vUV = Vector2D( 1.0f, 1.0f );
		m_Verts[ 3 ].m_vUV = Vector2D( 0.0f, 1.0f );
	}

	inline void FqInit( float x, float y, float w, float h )
	{
		FqInit( Vector2D( x, y ), Vector2D( x + w, y + h ) );
	}

	inline void FqInit( Vector4D color, Vector2D *position, Vector2D *uv, float z )
	{
		memset( this, 0, sizeof( *this ) );

		m_nFlags = 0;
		m_vColor = color;
		m_flZ = z;

		int idx = 0;
		for ( Vert_t &vert : m_Verts )
		{
			vert.m_vPosition = position[ idx ];
			vert.m_vUV = uv[ idx ];
			idx++;
		}
	}

	inline void FqInit( Vector2D p0, Vector2D p1, Vector2D p2, Vector2D p3, Vector4D color )
	{
		memset( this, 0, sizeof( *this ) );

		m_nFlags = 0;
		m_flZ = 0.0f;
		m_vColor = color;

		m_Verts[ 0 ].m_vPosition = p0;
		m_Verts[ 1 ].m_vPosition = p1;
		m_Verts[ 2 ].m_vPosition = p2;
		m_Verts[ 3 ].m_vPosition = p3;
	}
};

struct FancyQuadBrush_t
{
	FancyQuadBrush_t()
	{
		memset( this, 0, sizeof( *this ) );
		m_nGradientStops = 0;
		m_bIsLinearGradient = m_bIsRadialGradient = m_bAlphaOnlyTexture = false;
	}

	FancyQuadBrush_t( float r, float g, float b, float a )
	{
		memset( this, 0, sizeof( *this ) );
		m_nGradientStops = 0;
		m_bIsLinearGradient = m_bIsRadialGradient = m_bAlphaOnlyTexture = false;
		m_flColor[ 0 ][ 0 ] = r;
		m_flColor[ 0 ][ 1 ] = g;
		m_flColor[ 0 ][ 2 ] = b;
		m_flColor[ 0 ][ 3 ] = a;
	}

	int  m_nGradientStops; // how many stops are used by this gradient (usually 2 if gradient is in use)
	bool m_bIsLinearGradient;
	bool m_bIsRadialGradient;
	bool m_bAlphaOnlyTexture;
	float m_flColor[ FANCYQUAD_MAXSTOPS ][ 4 ]; // gradient colors (start, end)
	float m_flGradientStops[ FANCYQUAD_MAXSTOPS ]; // gradient colors (start, end)
	float m_flGradientStartPoint[ 2 ]; // gradient start point (linear, radial)
	float m_flGradientEndPoint[ 2 ]; // gradient end point (linear only)
	float m_flGradientRadii[ 2 ]; // gradient radius (radial only)
};

struct FancyQuadParameters_t
{
	FancyQuadParameters_t( uint32 nFLags)
	{
		memset( this, 0, sizeof( *this ) );
		m_nFlags = nFLags;
	}

	uint32 m_nFlags;

	float m_flVertexMin[ 2 ], m_flVertexMax[ 2 ]; // position of top left and bottom right corner
	float m_flTexCoordMin[ 2 ], m_flTexCoordMax[ 2 ]; // texcoord of top left and bottom right corner
	float m_flOpacityTexCoordMin[ 2 ], m_flOpacityTexCoordMax[ 2 ]; // OpacityMask texcoord of top left and bottom right corner
	float m_flCornerRadii[ 4 ][ 2 ]; // each corner has its own rounding in both directions (topleft topright bottomright bottomleft)
	float m_flBorderWidth[ 4 ]; // each edge has a configurable thickness (top right bottom left)
	float m_flBorderColor[ 4 ]; // WIth rounded corners, all borders use the same color from [0]
	float m_flZ; // some quads are at different depth

private:
	FancyQuadParameters_t()
	{
	}

};

struct FancyQuadDraw_t
{
	FancyQuadDraw_t()
	{
		Init();
	}

	void Init()
	{
		m_hTexture0 = RENDER_TEXTURE_HANDLE_INVALID;
		m_hTexture1 = RENDER_TEXTURE_HANDLE_INVALID;
		m_hTexture2 = RENDER_TEXTURE_HANDLE_INVALID;

		m_flTexture0TexCoordScale[ 0 ] = 1.0f;
		m_flTexture0TexCoordScale[ 1 ] = 1.0f;

		m_flTexture1TexCoordScale[ 0 ] = 1.0f;
		m_flTexture1TexCoordScale[ 1 ] = 1.0f;

		m_flHueShift = 0.0f;
		m_flSaturation = 1.0f;
		m_flBrightness = 1.0f;
		m_flContrast = 1.0f;

		m_flOpacityMaskOpacity = 1.0f;

		m_nWide = 0;
		m_nTall = 0;

		m_pQuadParameters = NULL;
		m_pQuadBrush = NULL;
		m_pVMatrix = NULL;

		m_flScale2DX = 1.0f;
		m_flScale2DY = 1.0f;

		m_flTextureWidth = 1.0f;
		m_flTextureHeight = 1.0f;

		m_flRadialClipStartAngle = 0.0f;
		m_flRadialClipSectorAngle = 0.0f;

		m_bTexIsNotPremul = false;
		m_bIsAlphaTexture = false;
		m_bIsYUVTexture = false;
		m_bIsYCoCgTexture = false;
		m_bRawCoords = false;
		m_bClipToLayer = false;
	}

	HRenderTexture m_hTexture0;
	HRenderTexture m_hTexture1;
	HRenderTexture m_hTexture2;

	float m_flHueShift;
	float m_flSaturation;
	float m_flBrightness;
	float m_flContrast;

	float m_flOpacityMaskOpacity;

	float m_flTexture0TexCoordScale[ 2 ];
	float m_flTexture1TexCoordScale[ 2 ];

	int32 m_nWide;
	int32 m_nTall;

	const FancyQuadParameters_t	*m_pQuadParameters;
	const FancyQuadBrush_t *m_pQuadBrush;
	const VMatrix *m_pVMatrix;

	float m_flScale2DX;
	float m_flScale2DY;

	float m_flTextureWidth;
	float m_flTextureHeight;

	float m_flRadialClipStartAngle;
	float m_flRadialClipSectorAngle;

	bool m_bTexIsNotPremul;
	bool m_bIsAlphaTexture;
	bool m_bIsYUVTexture;
	bool m_bIsYCoCgTexture;
	bool m_bRawCoords;
	bool m_bClipToLayer;
};

class CSource2Surface;
class CSource2CompositionLayer;

struct Source2SurfaceStats_t
{
	uint64 m_nFramesRendered;
	float64 m_flFrameTime;

	// Number of composition layers in caches
	uint64 m_nCompositionLayersFreeCache;
	uint64 m_nCompositionLayersReservedCache;
	uint64 m_nCompositionLayersOuterShadowCache;
	uint64 m_nCompositionLayersImageShadowCache;
	uint64 m_nCompositionLayersBlurCache;
	// Layers allocated
	uint64 m_nCompositionLayersCreated;

	// Size in bytes of each composition layer cache
	uint64 m_nFreeCacheSizeInBytes;
	uint64 m_nReservedCacheSizeInBytes;
	uint64 m_nOuterShadowCacheSizeInBytes;
	uint64 m_nImageShadowCacheSizeInBytes;
	uint64 m_nBlurCacheSizeInBytes;

	// Render targets cache
	uint64 m_nRTCacheCount;
	uint64 m_nRTCacheSizeInBytes;
	uint64 m_nRTCacheHits;
	uint64 m_nRTCacheMisses;

	// Number of calls to m_pRenderContext->Draw (not PanDx mode)
	uint64 m_nDrawBorderForPanelContextCalls;
	uint64 m_nDrawBorderCalls;
	uint64 m_nDrawTexturedQuadInternalCalls;
	uint64 m_nDrawfancyQuadCalls;

	// Number of draw calls (PanDx mode)
	uint64 m_nPanDxDrawCalls;					// total draw calls between own/disown dx
	uint64 m_nPanDxBasicDrawCalls;				// totall basicquad calls
	uint64 m_nPanDxFancyDrawCalls;				// total fancyquad draw calls

	// Text Shadow Cache
	uint64 m_nTextShadowCache;
	uint64 m_nTextShadowCacheSizeInBytes;

	void InitZero()
	{
		V_memset( this, 0, sizeof(*this) );
	}
};


//
// A very simple cache for render targets
//
class CSource2RenderTargetCache
{
public:

	CSource2RenderTargetCache() : m_nSizeRTsInBytes( 0 ) {}
	~CSource2RenderTargetCache() { PurgeOverSizeLimit( 0 ); }

	void Add( HRenderTexture hRenderTarget );

	// Try to find a compatible-size render target corresponding to the given 
	// width and height:
	//	* try to find a exact match first
	//	* otherwise try to return the smallest render target with a 
	//	  bigger or equal size (closest match)
	// Returns RESOURCE_HANDLE_INVALID if no match found
	HRenderTexture GetMatching( int16 nWidth, int16 nHeight );

	// Removes render targets until cache size is under the nMaxCacheSizeInBytes limit.
	void PurgeOverSizeLimit( uint64 nMaxCacheSizeInBytes );

	uint64 Count() const
	{
		return m_cachedRTs.Count();
	}

	uint64 SizeInBytes() const
	{
		return m_nSizeRTsInBytes;
	}

private:

	struct CacheEntry_t
	{
		int16			m_nWidth;
		int16			m_nHeight;
		HRenderTexture	m_hRenderTarget;
	};

	// TODO Perf - Currently using a linked list. Check perf impact in GetMatching()
	CUtlLinkedList< CacheEntry_t, int > m_cachedRTs;
	uint64 m_nSizeRTsInBytes;
};


//
// A simple LRU cache for composition layers
// 
template <typename K, typename L = CDefLess<K>, bool ALLOW_DUPES = false >
class CCompositionLayerCache
{
public:
	CCompositionLayerCache() {}
	~CCompositionLayerCache()
	{
		DeleteAll();
	}

	//
	// Removes and deletes all layers older than the supplied
	// flTimestamp
	// 
	void Purge( double flTimestamp )
	{
		while ( m_LRU.Count() )
		{
			int iList = m_LRU.Head();
			LRU_t& lru = m_LRU[iList];
			if ( lru.m_flLastUseTime <= flTimestamp )
			{
				Layer_t& data = m_Layers[lru.m_iMap];
				delete data.m_pLayer;
				m_Layers.RemoveAt( lru.m_iMap );
				m_LRU.Remove( iList );
			}
			else
			{
				break;
			}
		}
	}

	//
	// If the specified layer exists, sets its timestamp and
	// moves it to the end of the LRU list
	// 
	bool Touch( K ulLayerID, float flWidth, float flHeight, double flTimestamp )
	{
		COMPILE_TIME_ASSERT( !ALLOW_DUPES );
		int iMap = m_Layers.Find( ulLayerID );
		if ( iMap != m_Layers.InvalidIndex() )
		{
			Layer_t& layer = m_Layers[iMap];
			if ( layer.m_pLayer->GetCreationWidth() == flWidth && layer.m_pLayer->GetCreationHeight() == flHeight )
			{
				m_LRU.LinkToTail( layer.m_iLRUIndex );
				m_LRU[m_LRU.Tail()].m_flLastUseTime = flTimestamp;
				layer.m_iLRUIndex = m_LRU.Tail();

				return true;
			}
		}
		return false;
	}

	// Version that does require/check w/h
	bool Touch( K ulLayerID, double flTimestamp )
	{
		COMPILE_TIME_ASSERT( !ALLOW_DUPES );
		int iMap = m_Layers.Find( ulLayerID );
		if ( iMap != m_Layers.InvalidIndex() )
		{
			Layer_t& layer = m_Layers[ iMap ];
			m_LRU.LinkToTail( layer.m_iLRUIndex );
			m_LRU[ m_LRU.Tail() ].m_flLastUseTime = flTimestamp;
			layer.m_iLRUIndex = m_LRU.Tail();

			return true;
		}
		return false;
	}


	void Insert( K ulLayerID, CSource2CompositionLayer *pLayer, double flTimestamp )
	{
		int iMap = m_Layers.Find( ulLayerID );
		Assert( ALLOW_DUPES || iMap == m_Layers.InvalidIndex() );

		if ( iMap == m_Layers.InvalidIndex() || ALLOW_DUPES )
		{
			Layer_t layer;
			layer.m_pLayer = pLayer;
			layer.m_iLRUIndex = -1;
			if ( ALLOW_DUPES )
				iMap = m_Layers.InsertWithDupes( ulLayerID, layer );
			else
				iMap = m_Layers.Insert( ulLayerID, layer );
			LRU_t lru;
			lru.m_iMap = iMap;
			lru.m_flLastUseTime = flTimestamp;
			m_Layers[iMap].m_iLRUIndex = m_LRU.AddToTail( lru );
		}
	}

	void Remove( K ulLayerID ) // use FindAndRemove() for the dupes case
	{
		COMPILE_TIME_ASSERT( !ALLOW_DUPES );
		int iMap = m_Layers.Find( ulLayerID );
		if ( iMap != m_Layers.InvalidIndex() )
		{
			Layer_t& layer = m_Layers[iMap];
			m_LRU.Remove( layer.m_iLRUIndex );
			m_Layers.RemoveAt( iMap );
		}
	}

	CSource2CompositionLayer *Find( K ulLayerID, double flTimestamp )
	{
		COMPILE_TIME_ASSERT( !ALLOW_DUPES );
		int iMap = m_Layers.Find( ulLayerID );
		if ( iMap != m_Layers.InvalidIndex() )
		{
			Layer_t& layer = m_Layers[iMap];
			m_LRU.LinkToTail( layer.m_iLRUIndex );
			m_LRU[m_LRU.Tail()].m_flLastUseTime = flTimestamp;
			layer.m_iLRUIndex = m_LRU.Tail();

			return layer.m_pLayer;
		}
		return NULL;
	}

	CSource2CompositionLayer *FindAndRemove( K ulLayerID, float flTimestamp )
	{
		int iMap = m_Layers.FindFirst( ulLayerID );
		if ( iMap != m_Layers.InvalidIndex() )
		{
			Layer_t layer = m_Layers[iMap];
			m_LRU.Remove( layer.m_iLRUIndex );
			m_Layers.RemoveAt( iMap );

			return layer.m_pLayer;
		}
		return NULL;
	}



	void DeleteAll()
	{
		FOR_EACH_MAP_FAST( m_Layers, i )
		{
			UIEngine()->MarkLayerToRepaintThreadSafe( m_Layers[i].m_pLayer->GetContextID() );
			delete m_Layers[i].m_pLayer;
		}
		m_Layers.RemoveAll();
		m_LRU.RemoveAll();
	}


#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName )
	{
		VALIDATE_SCOPE();
		ValidateObj( m_Layers );
		ValidateObj( m_LRU );
		FOR_EACH_MAP_FAST( m_Layers, i )
		{
			ValidatePtr( m_Layers[i].m_pLayer );
		}

	}
#endif

	int Count() const
	{
		return m_LRU.Count();
	}

	uint64 RenderTargetSizeInBytes() const
	{
		int nSizeInBytes = 0;
		FOR_EACH_MAP_FAST( m_Layers, i )
		{
			nSizeInBytes += m_Layers[i].m_pLayer->RenderTargetSizeInBytes();
		}
		return nSizeInBytes;
	}


private:
	struct Layer_t
	{
		CSource2CompositionLayer * m_pLayer;
		int m_iLRUIndex;	// points into LRU list
	};
	struct LRU_t
	{
		double m_flLastUseTime;
		int m_iMap;			// points into m_Layers
	};
	CUtlMap<K, Layer_t, int, L> m_Layers;
	CUtlLinkedList<LRU_t, int> m_LRU;
};


class CSource2CompositionLayer
{
public:

	struct PanelContext_t
	{
		// Transform matrix for the panel within the layer
		VMatrix m_VMatrix;

		// box shadow data
		bool m_bBoxShadowInset;
		bool m_bBoxShadowFill;
		float m_flBoxShadowHorOffset;
		float m_flBoxShadowVerOffset;
		float m_flBoxShadowBlurRadius;
		float m_flBoxShadowSpreadDistance;
		uint32 m_rgbaBoxShadowColor;

		// Size of the panel
		float m_flLayerWidth;
		float m_flLayerHeight;

		// Scroll offsets of panel
		float m_flScrollX;
		float m_flScrollY;

		// Position of the panel within containing layer
		float m_flPositionX;
		float m_flPositionY;
		float m_flPositionZ;

		// border data
		float m_rgBorderWidths[4];
		uint32 m_rgbaBorderColors[4];

		//
		Vector4D m_multColor;

		// Saving / Restoring blend state in PushPanelContextInLayer / PopPanelContextInLayer
		RsBlendStateHandle_t m_hPriorBlendState;

		// Saving / Restoring "fractional pixel positions" in PushPanelContextInLayer / PopPanelContextInLayer
		EFractionalPixelPositions m_ePriorFractionalPixelPositions;

		void DrawBorderForPanelContext( CSource2Surface *pParentSurface, CSource2CompositionLayer *pParentLayer );
	};

	// Constructor 
	CSource2CompositionLayer( CSource2Surface *pParentSurface, float flWidth, float flHeight, bool bNeedsDepth, bool bNeedsIntermediate, bool bIsBackBuffer, bool bOffscreen, const char *pchType );
	~CSource2CompositionLayer();

	// Constructor for dummy composition layers used for searching
	CSource2CompositionLayer( float flWidth, float flHeight, bool bNeedsDepth, bool bNeedsIntermediate );

	// Restore the composition layer to neutral settings, comparable
	// to a freshly-constructed layer.  Used to reset layers pulled
	// from the reuse cache.
	void ResetToDefault();

	// Is the layer in drawing mode?
	bool BIsDrawing() { return m_bIsDrawing; }

	// Set a context id for the layer
	void SetContextID( uint64 ulContextID ) { m_ulContextID = ulContextID; }

	// Get the context id for the layer
	uint64 GetContextID() { return m_ulContextID; }

	bool BIsBackbuffer() { return m_bIsBackBuffer;  }

	void SetFullyOccluded( bool bFullyOccluded ) { m_bFullyOccluded = bFullyOccluded; }
	void ClearOccludedRegion() { m_bOccludedRegionValid = false;  }
	void SetOccludedRegion( float flLeft, float flTop, float flRight, float flBottom ) 
	{ 
		m_bOccludedRegionValid = true;
		m_vecOccludedRegion[0].x = flLeft; 
		m_vecOccludedRegion[0].y = flTop; 
		m_vecOccludedRegion[1].x = flRight; 
		m_vecOccludedRegion[1].y = flBottom; 
	}


	bool BFullyOccluded() { return m_bFullyOccluded; }

	bool BHasOclludedRegion() { return m_bOccludedRegionValid; }
	Vector2D *AccessOccludedRegion() { return m_vecOccludedRegion; }

	// Get width (integral)
	float GetWidth() { return m_flLayerWidth; }

	// Get height (integral)
	float GetHeight() { return m_flLayerHeight; }

	// Get non-ceil width (possibly non-integral)
	float GetCreationWidth() { return m_flLayerCreationWidth; }

	// Get non-ceil height (possibly non-integral)
	float GetCreationHeight() { return m_flLayerCreationHeight; }

	// Get gaussian blur std deviation
	void GetBlurValues( BlurType_t &blurType, float &flPasses, float &flStdDevHor, float &flStdDevVer )
	{
		flPasses = m_flBlurPasses;
		flStdDevHor = m_flBlurStdDevHor;
		flStdDevVer = m_flBlurStdDevVer;
		blurType = m_blurType;
	}

	// Set gaussian blur std deviation
	void SetBlurValues( BlurType_t blurType, float flPasses, float flStdDevHor, float flStdDevVer )
	{
		m_flBlurPasses = flPasses;
		m_flBlurStdDevHor = flStdDevHor;
		m_flBlurStdDevVer = flStdDevVer;
		m_blurType = blurType;
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
	void DrawBorder( CSource2Surface *pBaseSurface );

	// clear the contents of the render buffer
	void Clear();

	// Access render quad info
	BasicQuad_t *AccessRenderQuad() { return &m_RenderQuad; }

	// Access matrix data
	VMatrix *AccessMatrix() 
	{ 
		return &m_VMatrix; 
	}

	VMatrix *AccessPushedMatrix()
	{
		if ( m_vecPanelContext.Count() )
			return &( m_vecPanelContext[m_vecPanelContext.Count() - 1].m_VMatrix );

		return NULL;
	}

	// Get/set hue shift
	void SetHueShift( float flHueShift ) { m_flHueShift = flHueShift; }
	float GetHueShift() { return m_flHueShift; }

	// Get/set saturation
	void SetSaturation( float flSaturation ) { m_flSaturation = flSaturation; }
	float GetSaturation() { return m_flSaturation; }

	// Get/set brightness
	void SetBrightness( float flBrightness ) { m_flBrightness = flBrightness; }
	float GetBrightness() { return m_flBrightness; }

	// Get/set contrast
	void SetContrast( float flContrast ) { m_flContrast = flContrast; }
	float GetContrast() { return m_flContrast; }

	// Set opacity mask texture
	void SetOpacityMaskTexture( panorama::IUITexture *pTexture, float flOpacityMaskOpacity )
	{
		m_pOpacityMaskTexture.SafeRelease();

		m_pOpacityMaskTexture = pTexture;
		m_flOpacityMaskOpacity = flOpacityMaskOpacity;

		if ( m_pOpacityMaskTexture )
			m_pOpacityMaskTexture->AddRef();
	}

	// Access opacity mask texture
	bool HasOpacityMaskTexture() const { return m_pOpacityMaskTexture; }
	panorama::IUITexture *GetOpacityMaskTexture() { return m_pOpacityMaskTexture; }
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

	void SetBoxShadow( bool bInset, bool bFill, float flHorOffset, float flVerOffset, float flBlurRadius, float flSpreadDistance, uint32 rgbaShadowColor )
	{
		m_bBoxShadowInset = bInset;
		m_bBoxShadowFill = bFill;
		m_flBoxShadowHorOffset = flHorOffset;
		m_flBoxShadowVerOffset = flVerOffset;
		m_flBoxShadowBlurRadius = flBlurRadius;
		m_flBoxShadowSpreadDistance = flSpreadDistance;
		m_rgbaBoxShadowColor = rgbaShadowColor;
	}

	void GetBoxShadow( bool &bInset, bool &bFill, float &flHorOffset, float &flVerOffset, float &flBlurRadius, float &flSpreadDistance, uint32 &rgbaShadowColor )
	{
		bInset = m_bBoxShadowInset;
		bFill = m_bBoxShadowFill;
		flHorOffset = m_flBoxShadowHorOffset;
		flVerOffset = m_flBoxShadowVerOffset;
		flBlurRadius = m_flBoxShadowBlurRadius;
		flSpreadDistance = m_flBoxShadowSpreadDistance;
		rgbaShadowColor = m_rgbaBoxShadowColor;
	}

	void SetRadialClip( bool bRadialClip, float flCenterX, float flCenterY, float flStartAngle, float flSectorAngle )
	{
		m_bRadialClip = bRadialClip;
		m_flRadialClipCenterX = flCenterX;
		m_flRadialClipCenterY = flCenterY;
		m_flRadialClipStartAngle = flStartAngle;
		m_flRadialClipSectorAngle = flSectorAngle;
	}

	void GetRadialClip( bool &bRadialClip, float &flCenterX, float &flCenterY, float &flStartAngle, float &flSectorAngle )
	{
		bRadialClip = m_bRadialClip;
		flCenterX = m_flRadialClipCenterX;
		flCenterY = m_flRadialClipCenterY;
		flStartAngle = m_flRadialClipStartAngle;
		flSectorAngle = m_flRadialClipSectorAngle;
	}

	void Set2DScaleFactors( float x, float y )
	{
		m_flScale2D[0] = x;
		m_flScale2D[1] = y;
	}

	void SetMixBlendMode( EMixBlendMode mode )
	{
		m_eBlendMode = mode;
	}

	EMixBlendMode GetMixBlendMode( )
	{		
		return m_eBlendMode;
	}

	void SetPriorClampFractionalPixelPositions( EFractionalPixelPositions ePriorFractionalPixelPositions )
	{
		m_ePriorFractionalPixelPositions = ePriorFractionalPixelPositions;
	}

	EFractionalPixelPositions GetPriorFractionalPixelPositions() const 
	{
		return m_ePriorFractionalPixelPositions;
	}

	void Get2DScaleFactors( float &x, float &y )
	{
		x = m_flScale2D[0];
		y = m_flScale2D[1];
	}

	// Access corner rounding data for the layer
	float *AccessCornerRadii() { return m_rgCornerRadii; }

	// Access border width data for the layer
	float *AccessBorderWidths() { return m_rgBorderWidths; }

	// Push a new clip layer into this composition layer
	void PushClipLayer( const PushClipLayerRenderCommand_t &renderCommand );

	// Pop a clip layer out of the composition layer
	void PopClipLayer();

	void PushPanelContextInLayer( const PushPanelContextInLayerRenderCommand_t &renderCommand );

	PanelContext_t * AccessPanelContextInLayer() 
	{ 
		if ( m_vecPanelContext.Count() )
			return &(m_vecPanelContext[m_vecPanelContext.Count() - 1]);

		return NULL;
	}

	void PopPanelContextInLayer( const PopPanelContextInLayerRenderCommand_t &renderCommand );

	void GetCurrentClipRect( RectBounds_t &r );

	// Get current clip layer count for composition layer
	uint32 GetClipLayerCount();

	// Draws an inset shadow (unblurred) into the layer
	void DrawInsetShadowIntoLayer( CSource2Surface *pBaseSurface, float flPadding, float flWidth, float flHeight, float flHorOffset, float flVerOffset, float flSpreadDistance, uint32 shadowColor, float *pflInnerRadii );

	// Sort on size, then depth only
	bool operator <(const CSource2CompositionLayer &l) const
	{
		if ( m_flLayerWidth != l.m_flLayerWidth )
		{
			return ( m_flLayerWidth < l.m_flLayerWidth );
		}

		if ( m_flLayerHeight != l.m_flLayerHeight )
		{
			return ( m_flLayerHeight < l.m_flLayerHeight );
		}

		if ( m_bNeedsDepth != l.m_bNeedsDepth )
		{
			return ( m_bNeedsDepth < l.m_bNeedsDepth );
		}

		return m_bNeedsIntermediate < l.m_bNeedsIntermediate;
	}


	HRenderTexture GetTextureHandle() 
	{ 
		return m_hRenderTarget;
	}
	
	HRenderTexture GetIntermediateTextureHandle() 
	{ 
		return m_hIntermediateRenderTarget; 
	}

	HRenderTexture GetDepthStencilTextureHandle() 
	{ 
		return m_hDepthStencilRenderTarget; 
	}

	float GetRTOriginalWidthScale() const { return m_flRTOriginalWidthScale; }
	float GetRTOriginalHeightScale() const { return m_flRTOriginalHeightScale; }

	void SetMotionBlurValues( float fVelocity, float fDirX, float fDirY )
	{
		m_flMotionBlurVelocity = fVelocity;
		m_flMotionBlurDirection[0] = fDirX;
		m_flMotionBlurDirection[1] = fDirY;
	}

	void GetMotionBlurValues( float *out_pfVelocity, float *out_pfDirX, float *out_pfDirY ) const
	{
		*out_pfVelocity = m_flMotionBlurVelocity;
		*out_pfDirX = m_flMotionBlurDirection[0];
		*out_pfDirY = m_flMotionBlurDirection[1];
	}

	void ActivateRenderTarget();
	void ActivateRenderTargetAndClear();
	bool GetRenderTargetHandleAndDesc( HRenderTexture &hRT, RenderTargetDesc_t &rtDesc);

	void SetShouldCache( bool bShouldCache ) { m_bShouldCache = bShouldCache; }
	bool BShouldCache() const { return m_bShouldCache; }

	void SetReusedFromCache( bool bReused ) { m_bReusedFromCache = bReused; }
	bool BReusedFromCache() const { return m_bReusedFromCache; }

	void SetOffscreen( bool bOffscreen ) { m_bOffscreen = bOffscreen; }
	bool BOffscreen() const { return m_bOffscreen; }

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName );
#endif

	int RenderTargetSizeInBytes() const { return m_nRTSizeInBytes; }

private:

	void InternalActivateRenderTarget( bool bClearRenderTarget );

	// Ptr to parent surface
	CSource2Surface *m_pParentSurface;

	// Have we called BeginDraw() already?
	bool m_bIsDrawing;

	// context id for the layer
	uint64 m_ulContextID;

	bool m_bIsBackBuffer;

	// Size of the layer after ceil is used on the creation values.
	float m_flLayerWidth;
	float m_flLayerHeight;
	// Original creation values.
	float m_flLayerCreationWidth;
	float m_flLayerCreationHeight;

	// Transform matrix for the layer
	VMatrix m_VMatrix;

	// Vector of pushed panel contexts within the layer
	CUtlVector< PanelContext_t > m_vecPanelContext;

	// Corner rounding data for the layer
	float m_rgCornerRadii[8];

	// Border data
	float m_rgBorderWidths[4];
	uint32 m_rgbaBorderColors[4];

	// Color correction aspects for the layer
	float m_flHueShift;
	float m_flSaturation;
	float m_flBrightness;
	float m_flContrast;

	// Gaussian blur std deviation
	float m_flBlurPasses;
	float m_flBlurStdDevHor;
	float m_flBlurStdDevVer;
	BlurType_t  m_blurType;

	// Occlusion data
	bool m_bFullyOccluded;
	bool m_bOccludedRegionValid;
	Vector2D m_vecOccludedRegion[2];

	// Blend mode
	EMixBlendMode m_eBlendMode;

	// box shadow data
	bool m_bBoxShadowInset;
	bool m_bBoxShadowFill;
	float m_flBoxShadowHorOffset;
	float m_flBoxShadowVerOffset;
	float m_flBoxShadowBlurRadius;
	float m_flBoxShadowSpreadDistance;
	uint32 m_rgbaBoxShadowColor;

	// radial clip
	bool m_bRadialClip;
	float m_flRadialClipCenterX;
	float m_flRadialClipCenterY;
	float m_flRadialClipStartAngle;
	float m_flRadialClipSectorAngle;

	// Scale/translate for the enitre layer
	float m_flScaleLayerX;
	float m_flScaleLayerY;
	float m_flTranslateLayerX;
	float m_flTranslateLayerY;

	// Clip layers for the layer
	CUtlVector<RectBounds_t> m_vecClipLayers;

	// Quad to use when rendering to parent layer
	BasicQuad_t m_RenderQuad;

	// 2D scaling factors to apply centered around untransformed object
	float m_flScale2D[2];

	CRefPtr< panorama::IUITexture > m_pOpacityMaskTexture;
	float m_flOpacityMaskOpacity;

	bool m_bUseBackBuffer;
	HRenderTextureStrong m_hRenderTarget;
	HRenderTextureStrong m_hIntermediateRenderTarget;
	HRenderTextureStrong m_hDepthStencilRenderTarget;
	// scale factor to apply when drawing using the render target in case 
	// the render target is bigger than the layer
	float m_flRTOriginalWidthScale;
	float m_flRTOriginalHeightScale;

	bool m_bNeedsDepth;
	bool m_bNeedsIntermediate;
	bool m_bShouldCache;
	bool m_bReusedFromCache;
	bool m_bOffscreen;

	EFractionalPixelPositions m_ePriorFractionalPixelPositions;

	float m_flMotionBlurVelocity;
	float m_flMotionBlurDirection[2];
	int m_iMotionBlurSampleCount;

	// Sizes of all render target allocated - used for stats
	int m_nRTSizeInBytes;
};

class CSource2Surface : public ITextureResidencyListener, public IUI3DSurface, public IUITextTextureStorage, public IUITextTextureProvider
{
public:
	CSource2Surface( const char *pName );
	virtual ~CSource2Surface();

	bool BInitialize( int nSurfaceWidth, int nSurfaceHeight, int nWindowWidth, int nWindowHeight, bool bEnforceAspectRatio, bool bFixedSurfaceSize, CMouseCursorRender *pCursorRender );

	// Called to access the paint time of the last rendered frame (ie, time it was first generated in paint thread)
	virtual double GetLastFramePaintTime() { return 0.0; }

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

	virtual void SetRenderContext( ISceneView *pView, IRenderContext *pRenderContext, ISceneLayer *pLayer ) OVERRIDE
	{
		m_pSceneView = pView;
		m_pRenderContext = pRenderContext;
		m_pSceneLayer = pLayer;
	}

	// main engine noticed that a file on disk changed, see if we want to reload it... implement for shader reload?
	virtual void ReloadChangedFile( const char * ) { }

	// Called at the start of every frame. Should setup general state for the frame, call beginscene(), etc.
	virtual void BeginFrame( const BeginFrameRenderCommand_t &renderCommand ) OVERRIDE;

	// Called at the end of every frame.  Should do any per-frame cleanup, call endscene(), etc.
	virtual void EndFrame( const EndFrameRenderCommand_t &renderCommand ) OVERRIDE;

	// Called to tell us to free all cached GPU resources that we can
	void ClearGPUResources();

	// Called to clear the backbuffer to a solid color.
	virtual void ClearBackbuffer( const ClearBackbufferRenderCommand_t &renderCommand ) OVERRIDE;

	// Called to draw a filled quad
	virtual void DrawFilledRect( const RenderFilledRectRenderCommand_t &renderCommand ) OVERRIDE;

	// Handle drawing text region
	virtual void DrawTextRegion( const RenderTextRegionCommand_t &renderCommand ) OVERRIDE;
	virtual void DrawTextRegionOriginal( const RenderTextRegionCommand_t &renderCommand ) OVERRIDE;

	// Called to draw a textured quad with a texture object
	virtual void DrawTexturedRect( const RenderTexturedRectRenderCommand_t &renderCommand ) OVERRIDE;

	// Handle render callback request (basically calls back raw panel code to draw direct to surface, source 2 only noop elsewhere)
	virtual void RequestRenderCallback( const RequestRenderCallbackCommand_t &renderCommand ) OVERRIDE;

	// Called to push a new compositing layer
	virtual void PushCompositingLayer( const PushCompositingLayerRenderCommand_t &renderCommand ) OVERRIDE;

	// Called to pop a compositing layer, which leads to rendering it to the parent layer or backbuffer with transforms/opacity/etc applied
	virtual void PopCompositingLayer( const PopCompositingLayerRenderCommand_t &renderCommand ) OVERRIDE;

	// Called to push a clip layer
	virtual void PushClipLayer( const PushClipLayerRenderCommand_t &renderCommand ) OVERRIDE;

	// Called to pop a clip layer
	virtual void PopClipLayer( const PopClipLayerRenderCommand_t &renderCommand ) OVERRIDE;

	// Called to push a transform matrix
	virtual void PushPanelContextInLayer( const PushPanelContextInLayerRenderCommand_t &renderCommand ) OVERRIDE;

	// Called to pop a transform matrix
	virtual void PopPanelContextInLayer( const PopPanelContextInLayerRenderCommand_t &renderCommand ) OVERRIDE;

	// Just lock a texture, used to increment it's draw serial when it's actually being culled
	virtual void LockTexture( const LockTextureRenderCommand_t &renderCommand ) OVERRIDE {};

	// Thread safe function which lets another thread verify we have a cached representation of a given layer
	// so it can be re-rendered without a full redraw, and also makes sure we don't LRU it right away moving it
	// to the end of our list for throw out.
	virtual bool PingCompositingLayer( uint64 ulLayerID, float flWidth, float flHeight );

	// Thread safe function which lets another thread get the render target name of a given layer
	// Returns nullptr if not found
	virtual const char *GetCompositionLayerRenderTargetName( uint64 ulLayerID ) OVERRIDE;

	// Force throwing out of a compositing layer, called from animation thread.  This makes sure we stop re-using a layer
	// which the animation thread knows it's culling render operations to and thus knows is now dirty and not re-usuable
	virtual void FreeCompositingLayer( const FreeCompositingLayerRenderCommand_t &renderCommand ) OVERRIDE;

	// Get the FPS average from the render thread from the last few frames
	virtual float GetFPSAverage() { return 0.0; }

	// Get the FPS average since creation
	virtual float GetSessionFPSAverages() { return 0.0; }

	// Ask if VSync is enabled
	virtual bool BVsyncEnabled() { return false; }

	// Unused in source2 panorama, used in Steam versions to control render thread framerate, but we'll render at games 
	// framerate and will not control sleeping in panorama inside Source2, so this value could be anything.
	virtual uint64 GetMinFrameTimeInMicroseconds()
	{
		// 120fps 
		return 8333;
	}

	// If returns true, there's nothing to draw, there's no visibility to the frame
	// right now.
	virtual bool BSurfaceOccluded() { return false; }

	// Get the scaling factor that is being applied to all UI by the framework
	virtual float GetWindowScaleFactor() { return m_flScaleFactor; }

	// Set the scaling factor that is being applied to all UI by the framework
	virtual void SetWindowScaleFactor( float flScaleFactor ) { m_flScaleFactor = flScaleFactor; }

	//
	// Overlay rendering only, noop functions in source2
	//
	virtual void PushOverlayRenderCmdStream( CSharedMemStream *pRenderStream, DWORD dwPID, float flOpacity, uint32 unGameWidth, uint32 unGameHeight, EOverlayWindowAlignment alignment ) { }
	virtual void SetOverlayTextureFormat( int32 eTextureFormat ) { }
	virtual void SetOverlayLetterboxColor( Color c ) { }

	// return the number of periods we saw below the min fps since panorama creation
	virtual volatile int GetNumPeriodsBelowMinFPS() { return 0; }

	// Helper to tell surface directly where steam pad pointer is... thread safe.. noop in source 2
	virtual void UpdateSteamPadPointers( SteamPadPointer_t* leftPointer, SteamPadPointer_t* rightPointer ) OVERRIDE { }

	// If the specified text region exists in the "text layout draw cache", sets its timestamp and
	// moves it to the end of the LRU list
	virtual void TouchTextRegionInCache( const RenderTextRegionCommand_t &renderCommand ) OVERRIDE;

	// If the specified text region doesn't exist in the "text layout draw cache", kick off
	// an async job to populate it
	virtual void AsyncAddTextRegionToCache( const DrawTextRegionRenderCommand_t &renderCommand ) OVERRIDE;

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

	// ITextureResidencyListener
	virtual void TextureBecameFullyResident( HRenderTexture hTex );
	virtual void TextureBecameEvicted( HRenderTexture hTex );

	uint32 GetSurfaceWidth() const { return m_unSurfaceWidth; }
	uint32 GetSurfaceHeight() const { return m_unSurfaceHeight; }

	void OnDeviceRestored();

	void SpewTextureInfo();

private:

	friend class CSource2CompositionLayer;

	// Helper util structure to allow us to make the downsample and gaussian passes generic
	struct BlurRT_t
	{
		HRenderTexture* m_pRenderTex;			// using RT as tex
		RenderTargetDesc_t* m_pRTDesc;		// seetting/binding RT
		float m_flWindowW;
		float m_flWindowH;					// width and height that we are using in this RT
		float m_flRTW;						// RT sizes
		float m_flRTH;


		BlurRT_t( HRenderTexture* renderTex, RenderTargetDesc_t* rtDesc,
			float flWindowW, float flWindowH, float flrtW, float flrtH )
			: m_pRenderTex( renderTex ), m_pRTDesc( rtDesc )
		{
			m_flWindowW = flWindowW;
			m_flWindowH = flWindowH;
			m_flRTW = flrtW;
			m_flRTH = flrtH;
		}

	};

	void DownSize( BlurRT_t &rt, float flFactor );
	bool SetupBlurPanelAttr( CRenderAttributes &renderAttributes, CSource2CompositionLayer *pLayer );
	void ClearBlurRT( BlurRT_t &blurRT, float flW, float flH, Vector4D color );
	void DrawGaussianRTtoRT( CRenderAttributes &renderAttributes, BlurRT_t &dst, BlurRT_t &src, char cHorV, float flStdDev );
	void CopyBlurRectsToRT( CRenderAttributes &renderAttributes, BlurRT_t &dst, BlurRT_t &src, bool bUpSizeInCopy );
	void DrawDownSizeRTtoRT( CRenderAttributes &renderAttributes, BlurRT_t &dst, BlurRT_t &src );
	void DrawUpSizeRTtoRT( CRenderAttributes &renderAttributes, BlurRT_t &dst, BlurRT_t &src );
	void ApplyGaussianBlur( CRenderAttributes &renderAttributes, CSource2CompositionLayer *pLayer, float flBlurPasses, float flBlurStdDevHor, float flBlurStdDevVer );
	void ApplyFastGaussianBlur( CRenderAttributes &renderAttributes, CSource2CompositionLayer *pLayer, float flBlurPasses, float flBlurStdDevHor, float flBlurStdDevVer, BlurType_t blurType = BT_FAST );

	void DrawFancyQuad( const FancyQuadDraw_t *pFancyQuadDraw );
	void DrawTexturedQuadInternal( HMaterial hMaterial, CRenderAttributes *pRenderAttributes, HRenderTexture hTextureID, BasicQuad_t *pBasicQuad );
	HRenderTexture UpdateFancyQuadGradientTexture( const FancyQuadBrush_t *pFancyQuadBrush );

	CSource2CompositionLayer *GetCompositionLayer( float width, float height, bool bNeedsDepth, bool bNeedsIntermediate, const char *pchType );

	void ClearCompositingLayer( CSource2CompositionLayer *pLayer );

	uint32 GetWindowWidth() const { return m_unWindowWidth; }
	uint32 GetWindowHeight() const { return m_unWindowHeight; }
	bool BUpdateWindowSizeIfNeeded( uint32 nWidth, uint32 nHeight );

	// Activate current composition layer as render target
	void GetBackBufferRenderTarget( HRenderTexture &hRT, RenderTargetDesc_t &rtDesc );
	void ActivateBackBufferRenderTarget();

	void ComputeBackbufferScaling();

	CSource2CompositionLayer *GetImageShadowLayer( const ImageShadowLayerParams_t &params, IUITexture *pTexture, float flOriginalWidthScale, float flOriginalHeightScale );

	CSource2CompositionLayer *GetOuterShadowLayer( const OuterShadowLayerParams_t &params );
	void DrawOuterShadowLayer( float flWidthSourceLayer, float flHeightSourceLayer, float flXPosInParent, float flYPosInParent, float flScale2DX, float flScale2DY, float flOpacity, float *pflCornerRadii, VMatrix *pMatTransform,
		float flWidthParentLayer, float flHeightParentLayer, CSource2CompositionLayer *pShadowLayer, bool bFill, float flHorOffset, float flVerOffset, float flBlurRadius, float flSpreadDistance, uint32 shadowColor, float flZ );

	CSource2CompositionLayer *GetInsetShadowLayer( float flWidthLayer, float flHeightLayer, float *pBorderRadii, float *pBorderWidths, float flHorOffset, float flVerOffset, float flBlurRadius, float flSpreadDistance, uint32 shadowColor );
	void DrawInsetShadowLayer( float flWidthSourceLayer, float flHeightSourceLayer, float flXPosInParent, float flYPosInParent, float flScale2DX, float flScale2DY, float flOpacity, float *pflOuterRadii, float *pflBorderWidths, VMatrix *pMatTransform,
		float flWidthParentLayer, float flHeightParentLayer, CSource2CompositionLayer *pShadowLayer, float flHorOffset, float flVerOffset, float flBlurRadius, float flSpreadDistance, uint32 shadowColor );

	HRenderTexture GetOpacityMaskShaderResourceViewForTexture( IUITexture *pTexture, float *pOriginalWidthScale, float *pOriginalHeightScale );

	void SetFancyQuadFillBrush( FancyQuadBrush_t &fancybrush, const FillBrush_t &brush, float offsetx, float offsety );

	void DestroyDeviceResources( bool bShuttingDown );
	bool BCreateDeviceResources();

	bool BRecreateBaseCompositionLayer();

	bool BBackBufferScalingNeeded() const { return m_unWindowHeight != m_unSurfaceHeight || m_unWindowWidth != m_unSurfaceWidth; }

	UITextOpacityMaskData_t *GetCachedTextOpacityMask( const void *pRawText, int cbRawText, int cTextChars, EPanoramaTextEncoding eTextEncoding, float x0, float y0, float x1, float y1, float flLineHeight, ETextAlign align, bool bWrap, bool bEllipsis, const RenderTextFormat_t &defaultFormat, const CRenderDataList< RenderTextRangeFormat_t > &rangeFormats );

	void DrawTextRegionRange( CSource2CompositionLayer *pLayer, float x0, float y0, float x1, float y1, UITextOpacityMaskDataRange_t &maskRange, const FillBrushCollection_t &fill_brush_collection );

	// Draw debug text into the current composition layer. Used to render panorama frame stats
	void DrawDebugText( const char *pchText, float x0, float y0, float fontSize, const Color &color );

	Source2SurfaceStats_t &GetFrameStats()
	{
		return m_frameStats;
	}

	void LogStats( const Source2SurfaceStats_t &stats, float x0, float y0 );

	CUtlString m_SurfaceName;

	ISceneView *m_pSceneView;
	IRenderContext *m_pRenderContext;
	ISceneLayer *m_pSceneLayer;

	// owner of details about the cursor state
	CMouseCursorRender *m_pCursorRender;

	uint32 m_unSurfaceWidth;
	uint32 m_unSurfaceHeight;
	uint32 m_unWindowWidth;
	uint32 m_unWindowHeight;
	bool m_bFixedSurfaceSize;
	bool m_bEnforceAspectRatio;
	bool m_bUseBackBufferDirectly;
	float m_flScaleFactor;
	bool m_bDeviceRestore;
	bool m_bMainMenu;

	float m_flScaleBackbufferX;
	float m_flTranslateBackbufferX;
	float m_flScaleBackbufferY;
	float m_flTranslateBackbufferY;

	// Current render frame time
	double m_flCurrentRenderFrameTime;

	// Last paint frame time
	double m_flLastPaintFrameTime;

	// Stats
	Source2SurfaceStats_t m_frameStats;

	CThreadMutex m_lockTextureMap;
	CUtlMap< uint32, IUITexture *, int, CDefLess< uint32 > > m_mapTextures;

	CTSQueue< uint32 > m_tsQueueTextureDeletes;

	// Stack of composition layers currently pushed
	CUtlVector< CSource2CompositionLayer * > m_stackCompositionLayers;

	// Render target cache (used by composition layer)
	CSource2RenderTargetCache m_RenderTargetCache;

	// List of free composition layers for re-use
	CCompositionLayerCache<CSource2CompositionLayer *, CDefLessPtr<CSource2CompositionLayer>, true > m_FreeLayers;

	CCompositionLayerCache<uint64> m_ReservedLayers;
	CThreadMutex m_MutexReservedLayers;

	CCompositionLayerCache< OuterShadowLayerParams_t, CDefLess< OuterShadowLayerParams_t >, true > m_OuterShadowLayers;
	CCompositionLayerCache< ImageShadowLayerParams_t, CDefLess< ImageShadowLayerParams_t >, true > m_ImageShadowLayers;

	// text shadow cache
	//TextShadowKey_t

	CCompositionLayerCache< TextShadowKey_t, CDefLess<TextShadowKey_t>, false> m_TextShadowCache;


	IUITextTextureCache *m_pTextTextureCache;
	IUITextLayoutDrawCache *m_pTextLayoutDrawCache;

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
		float m_rgBorderRadii[ 8 ];

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

			for ( int i = 0; i < V_ARRAYSIZE( m_rgBorderRadii ); ++i )
			{
				if ( m_rgBorderRadii[ i ] < r.m_rgBorderRadii[ i ] )
					return true;
				else if ( m_rgBorderRadii[ i ] > r.m_rgBorderRadii[ i ] )
					return false;
			}

			return m_shadowColor < r.m_shadowColor;
		}
	};

	static CInterlockedInt s_nCompositionRenderTargets;
	static CInterlockedInt s_nTextureIDs;

	HResourceManifest m_hResourceManifest;

	HMaterialStrong m_hMaterial;
	HMaterialStrong m_hFancyQuadMaterial;

	RenderInputLayout_t m_hSource2FancyQuadVertexLayout;
	RenderInputLayout_t m_hSource2VertexTexturedLayout;

	RsBlendStateHandle_t m_hAlphaBlendState;
	RsBlendStateHandle_t m_hPremultipliedAlphaBlendState;
	RsBlendStateHandle_t m_hAlphaOnlyBlendState;
	RsBlendStateHandle_t m_hMixScreenState;
	RsBlendStateHandle_t m_hMixMultiplyState;
	RsBlendStateHandle_t m_hMixAdditiveState;
	RsBlendStateHandle_t m_hMixAdditiveSRGBState;
	RsBlendStateHandle_t m_hMixOpaqueState;

	RsBlendStateHandle_t m_hCurrentBlendState;
	uint32				 m_nBlendStateOverridden;

	EFractionalPixelPositions m_eCurrentFractionalPixelPositions;

	struct FancyQuadGradientTexture_t
	{
		HRenderTextureStrong m_hTexture;
		int m_nStops;
		int m_nAge;
		float m_flStops[ FANCYQUAD_MAXSTOPS ];
		float m_flColor[ FANCYQUAD_MAXSTOPS ][ 4 ];

		bool MatchesBrush( const FancyQuadBrush_t *pBrush ) const;
		void SetFromBrush( const FancyQuadBrush_t *pBrush, HRenderTexture hTexture );
	};

	int m_nGradientTextures;
	FancyQuadGradientTexture_t m_GradientTextures[ 10 ];
	unsigned char m_GradientTextureBuffer[ FANCYQUAD_GRADIENT_TEXTURE_SIZE ][ 4 ];

	// Tracked panels (for blur rectangles etc..)

	struct CBlurPanels
	{
		uint64				nTarget_id;
		Vector2D			m_vTopLeft;
		Vector2D			m_vBottomRight;
		VMatrix				m_Matrix;
		CUtlVector<uint64>	source_ids;
	};

	struct CTrackedPanelData
	{
		uint64	m_nFrameidx;
		Vector2D m_vTopLeft;
		Vector2D m_vBottomRight;
		VMatrix	 m_Matrix;

		CTrackedPanelData()
		{
			m_nFrameidx = (uint64)~0;
			m_vTopLeft = Vector2D( 0.0, 0.0 );
			m_vBottomRight = Vector2D( 0.0, 0.0 );
			m_Matrix.Identity();
		}

	};

	CUtlVector<CBlurPanels> m_BlurPanels;		// One for each CSGOBlurPanels. Had Panel ID, rectangle&matrix, ultvec of sourceids
	CUtlVector<uint64> m_TrackedSourcePanelIDs; // This vector is used to find a source panelID and then it's idx is used for m_TrackedSourcePanelData
	CUtlVector<CTrackedPanelData> m_TrackedSourcePanelData; // rectangle and matrix for the sourcepanel

	// Cache some blur target results for panorama_eco mode ( main menu only )

	static const int MAX_BT_CACHE = 32;

	struct BlurLayerCache_t
	{
		float m_flWindowW;
		float m_flWindowH;
		float m_flWriteTime;		// Access times
		float m_flReadTime;
		//CSource2CompositionLayer *m_pSrcLayer;
		uint64 m_nLayerID;
		CSource2CompositionLayer *m_pCachedBlurLayer;

	} m_BlurLayerCache[ MAX_BT_CACHE ];

	void InitBlurLayerCache();
	int LookupBlurLayerCacheIdx( uint64 nLayerID, float flOldestWriteTime, float flTime ); // Returns -1 if there is no valid cache
	int AllocBlurLayerCacheIdx( uint64 nLayerID, float flCurrTime, int nW, int nH );
	void TermBlurLayerCache();
	void PurgeBlurLayerCache( float flOldestWriteTime );
	void StatsBlurLayerCache( uint64 &nCount, uint64 &nSizeInBytes );

public:

	int FindBlurPanel( uint64 panelID )
	{
		for ( int i = 0; i < m_BlurPanels.Count(); i++ )
		{
			if ( m_BlurPanels[ i ].nTarget_id == panelID )
			{
				return i;
			}
		}
		return -1;
	}


	void ResetBlurPanels()
	{
		m_BlurPanels.RemoveAll();
		m_TrackedSourcePanelIDs.RemoveAll();
		m_TrackedSourcePanelData.RemoveAll();
	}


	void AddBlurPanel( uint64 panelID, const CRenderDataList< uint64 > &srcPanels )
	{
		int idx = FindBlurPanel( panelID );
		if ( idx == -1 )
		{
			idx = m_BlurPanels.AddToTail();
		}

		if ( idx != -1 )
		{
			m_BlurPanels[ idx ].nTarget_id = panelID;

			for ( const uint64 *pID : srcPanels )
			{
				m_BlurPanels[ idx ].source_ids.AddToTail( *pID );
				AddSourcePanel( *pID );
			}
		}
	}

	void AddBlurPanelData( uint64 panelID, const Vector2D &topLeft, const Vector2D &bottomRight, const RenderMatrix4x4_t &rmatrix )
	{
		int idx = FindBlurPanel( panelID );

		if ( idx != -1 )
		{
			VMatrix matrix;
			RenderMatrixToVMatrix( matrix, rmatrix );

			m_BlurPanels[ idx ].nTarget_id = panelID;
			m_BlurPanels[ idx ].m_vTopLeft = topLeft;
			m_BlurPanels[ idx ].m_vBottomRight = bottomRight;
			m_BlurPanels[ idx ].m_Matrix = matrix;
		}

	}



	void AddSourcePanel( uint64 panelID )
	{
		if ( m_TrackedSourcePanelIDs.Find( panelID ) == -1 )
		{
			int idx = m_TrackedSourcePanelIDs.AddToTail();
			m_TrackedSourcePanelIDs[ idx ] = panelID;
			m_TrackedSourcePanelData.AddToTail();
		}

	}

	void AddSourcePanelData( uint64 panelID, const Vector2D &topLeft, const Vector2D &bottomRight, const RenderMatrix4x4_t &rmatrix )
	{
		int idx = m_TrackedSourcePanelIDs.Find( panelID );

		if ( idx != -1 )
		{
			// Only update with the first data available so that, for example, a fillrect does not override the containing compostion layer coords

			if ( m_TrackedSourcePanelData[ idx ].m_nFrameidx != m_frameStats.m_nFramesRendered )
			{
				m_TrackedSourcePanelData[ idx ].m_nFrameidx = m_frameStats.m_nFramesRendered;

				VMatrix matrix;
				RenderMatrixToVMatrix( matrix, rmatrix );

				m_TrackedSourcePanelData[ idx ].m_Matrix = matrix;
				m_TrackedSourcePanelData[ idx ].m_vTopLeft = topLeft;
				m_TrackedSourcePanelData[ idx ].m_vBottomRight = bottomRight;
			}
		}

	}

};

} // namespace panorama

#if defined ( WIN32 ) || defined ( DX_TO_GL_ABSTRACTION )
#define PANDX_DRAW 1
#else 
#define PANDX_DRAW 0
#endif


#if PANDX_DRAW

//--------------------------------------------------------------------------------------------------
// PanDx -- Draw panorama directly with DX rather than via matsys.
// Matsys etc.. is not ideal for panorama's quad at a time drawing
//--------------------------------------------------------------------------------------------------

extern bool g_bPanDx;

bool IsPanDxInsideOwnDx();

void PanDxInit();
void PanDxTerm();

void PanDxOwnDx();			// Start rendering directly with DX
void PanDxDisownDx();		// Go back to matsys
bool IsPanDxInsideOwnDx();	// Is between Own/Disown

void PanDxDeviceReset();

void PanDxDrawFancyQuads( CRenderContext* pCtx, panorama::FancyQuad_t *pFancyQuad, int nNumQuads = 1 );
void PanDxDrawBasicQuad( CRenderContext* pCtx, panorama::BasicQuad_t *pBasicQuad );

void PanDxClearColor( uint8 r, uint8 g, uint8 b, uint8 a );
void PanDxClearBuffers( bool bColor, bool bDepth, bool bStencil );

void PanDxSetViewPort( int x, int y, int w, int h, float minZ, float maxZ );
void PanDxGetViewPort( int &x, int &y, int &w, int &h, float &minZ, float &maxZ );
void PanDxSetScissor( int nCount, const Rect_t *pRects );

void PanDxSetRenderTarget( ITexture* pTexture );		// Pass NULL to set the backbuffer.
void PanDxCopyBackBuffer( void* pSurf, float x0, float y0, float x1, float y1 );

void PanDxFlushBatch( bool bEarlyFlush );
void PanDxInvalidateRTShadowState();


#endif

#endif // 
