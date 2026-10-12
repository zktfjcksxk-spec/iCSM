#ifndef INCLUDED_IRENDERCONTEXTBLAH_H
#define INCLUDED_IRENDERCONTEXTBLAH_H
//================ Copyright (c) Valve Corporation. All Rights Reserved. ===========================
//
//
//
//==================================================================================================

#include "tier0/platform.h"
#include "tier0/dbg.h"
#include "filesystem/iasyncfilesystem.h"
#include "tier1/utlstringtoken.h"
#include "UtlStringMap.h"
#include "tier1/strtools.h"

#include "wrap_other.h"

#include "resourcesystem/resourcehandle.h"
#include "resourcesystem/resourcehandletypes.h"

#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imesh.h"

#include "panorama/s1wrapperRenderAttributes.h"



//-----------------------------------------------------------------------------
// Viewport structure
//-----------------------------------------------------------------------------
#define RENDER_VIEWPORT_VERSION 1
struct RenderViewport_t
{
	int m_nVersion;
	int m_nTopLeftX;
	int m_nTopLeftY;
	int m_nWidth;
	int m_nHeight;
	float m_flMinZ;
	float m_flMaxZ;

	RenderViewport_t() : m_nVersion( RENDER_VIEWPORT_VERSION ) {}
	RenderViewport_t( int x, int y, int nWidth, int nHeight, float flMinZ = 0.0f, float flMaxZ = 1.0f ) : m_nVersion( RENDER_VIEWPORT_VERSION ) { Init( x, y, nWidth, nHeight, flMinZ, flMaxZ ); }

	void Init()
	{
		memset( this, 0, sizeof( RenderViewport_t ) );
		m_nVersion = RENDER_VIEWPORT_VERSION;
	}

	void Init( int x, int y, int nWidth, int nHeight, float flMinZ = 0.0f, float flMaxZ = 1.0f )
	{
		m_nVersion = RENDER_VIEWPORT_VERSION;
		m_nTopLeftX = x; m_nTopLeftY = y; m_nWidth = nWidth; m_nHeight = nHeight;
		m_flMinZ = flMinZ;
		m_flMaxZ = flMaxZ;
	}

	void Init( Rect_t const &rect, float flMinZ = 0.0f, float flMaxZ = 1.0f )
	{
		Init( rect.x, rect.y, rect.width, rect.height, flMinZ, flMaxZ );
	}

	bool IsValid() const
	{
		return ( m_nWidth != 0 && m_nHeight != 0 );
	}

	FORCEINLINE float NormalizedXCoordRelative( int nScrX ) const
	{
		return RemapValClamped( nScrX, 0, m_nWidth - 1, -1, 1 );
	}

	FORCEINLINE float NormalizedYCoordRelative( int nScrY ) const
	{
		return RemapValClamped( nScrY, 0, m_nHeight - 1, -1, 1 );
	}

	/// convert from pixel coordinates to viewport coordinates. Note that this is a direct mapping.. users need to understand that pixels actually lie at an offset of .5,.5. This means for instance that if you want to draw a single pixel at location
	/// 5,7, you would want to draw a rectangle spanning from (5,7) to (6,8). This will cover the single pixel sampling location that is at 5.5, 7.5. Z is not remapped to viewport z range but is used as-is
	FORCEINLINE Vector ConvertPixelCoordinateToNDCCoordinates( float flXCoord, float flYCoord, float flZCoord ) const
	{
		Vector vRet;
		vRet.x = RemapVal( flXCoord, m_nTopLeftX, m_nTopLeftX + m_nWidth, -1, 1 );
		vRet.y = RemapVal( flYCoord, m_nTopLeftY, m_nTopLeftY + m_nHeight, 1, -1 );
		vRet.z = flZCoord;
		return vRet;
	}
};

enum RenderColorSpace_t : uint8
{
	RENDER_LINEAR = 0,
	RENDER_SRGB = 1
};

//-----------------------------------------------------------------------------
// Packs a float4 color to ARGB 8-bit int color compatible with D3D9
//-----------------------------------------------------------------------------
FORCEINLINE uint32 PackFloat4ColorToUInt32( float flR, float flG, float flB, float flA )
{
	uint32 nR, nG, nB, nA;
	nR = uint32( flR * 255.0f );
	nG = uint32( flG * 255.0f );
	nB = uint32( flB * 255.0f );
	nA = uint32( flA * 255.0f );
	return ( ( nA & 0xFF ) << 24 ) | ( ( nR & 0xFF ) << 16 ) | ( ( nG & 0xFF ) << 8 ) | ( ( nB & 0xFF ) );
}

//-----------------------------------------------------------------------------
// Packs a float4 color to ARGB 8-bit int color compatible with D3D9, clamping the floats to [0, 1]
//-----------------------------------------------------------------------------
FORCEINLINE uint32 ClampAndPackFloat4ColorToUInt32( float flR, float flG, float flB, float flA )
{
	uint32 nR, nG, nB, nA;
	nR = uint32( clamp( flR, 0.0f, 1.0f ) * 255.0f );
	nG = uint32( clamp( flG, 0.0f, 1.0f ) * 255.0f );
	nB = uint32( clamp( flB, 0.0f, 1.0f ) * 255.0f );
	nA = uint32( clamp( flA, 0.0f, 1.0f ) * 255.0f );
	return ( ( nA & 0xFF ) << 24 ) | ( ( nR & 0xFF ) << 16 ) | ( ( nG & 0xFF ) << 8 ) | ( ( nB & 0xFF ) );
}

//-----------------------------------------------------------------------------
// Unpacks an ARGB 8-bit int color to 4 floats
//-----------------------------------------------------------------------------
FORCEINLINE void UnpackUint32ColorToFloat4( uint32 nPackedColor, float *pflR, float *pflG, float *pflB, float *pflA )
{
	uint32 nR, nG, nB, nA;

	nR = ( nPackedColor >> 16 ) & 0xFF;
	nG = ( nPackedColor >> 8 ) & 0xFF;
	nB = (nPackedColor)& 0xFF;
	nA = ( nPackedColor >> 24 ) & 0xFF;

	*pflR = nR * 255.0f;
	*pflG = nG * 255.0f;
	*pflB = nB * 255.0f;
	*pflA = nA * 255.0f;
}




//-----------------------------------------------------------------------------
// Describes a depth or color back buffer
//-----------------------------------------------------------------------------
enum SwapChainBuffer_t
{
	SWAP_CHAIN_BUFFER_INVALID = -1,
	SWAP_CHAIN_BUFFER_COLOR = 0,
	SWAP_CHAIN_BUFFER_DEPTH
};

enum RsCullMode_t
{
	RS_CULL_NONE = 0,
	RS_CULL_BACK = 1,
	RS_CULL_FRONT = 2
};

enum RsFillMode_t
{
	RS_FILL_SOLID = 0,
	RS_FILL_WIREFRAME = 1
};

enum RsComparison_t
{
	RS_CMP_NEVER = 0,
	RS_CMP_LESS = 1,
	RS_CMP_EQUAL = 2,
	RS_CMP_LESS_EQUAL = 3,
	RS_CMP_GREATER = 4,
	RS_CMP_NOT_EQUAL = 5,
	RS_CMP_GREATER_EQUAL = 6,
	RS_CMP_ALWAYS = 7
};

enum RsStencilOp_t
{
	RS_STENCIL_OP_KEEP = 0,
	RS_STENCIL_OP_ZERO = 1,
	RS_STENCIL_OP_REPLACE = 2,
	RS_STENCIL_OP_INCR_SAT = 3,
	RS_STENCIL_OP_DECR_SAT = 4,
	RS_STENCIL_OP_INVERT = 5,
	RS_STENCIL_OP_INCR = 6,
	RS_STENCIL_OP_DECR = 7
};

enum RsHiStencilComparison360_t
{
	RS_HI_STENCIL_CMP_EQUAL = 0,
	RS_HI_STENCIL_CMP_NOT_EQUAL = 1
};

enum RsHiZMode360_t
{
	RS_HI_Z_AUTOMATIC = 0,
	RS_HI_Z_DISABLE = 1,
	RS_HI_Z_ENABLE = 2
};

enum RsBlendOp_t
{
	RS_BLEND_OP_ADD = 0,
	RS_BLEND_OP_SUBTRACT = 1,
	RS_BLEND_OP_REV_SUBTRACT = 2,
	RS_BLEND_OP_MIN = 3,
	RS_BLEND_OP_MAX = 4
};

enum RsBlendMode_t
{
	RS_BLEND_MODE_ZERO = 0,
	RS_BLEND_MODE_ONE = 1,
	RS_BLEND_MODE_SRC_COLOR = 2,
	RS_BLEND_MODE_INV_SRC_COLOR = 3,
	RS_BLEND_MODE_SRC_ALPHA = 4,
	RS_BLEND_MODE_INV_SRC_ALPHA = 5,
	RS_BLEND_MODE_DEST_ALPHA = 6,
	RS_BLEND_MODE_INV_DEST_ALPHA = 7,
	RS_BLEND_MODE_DEST_COLOR = 8,
	RS_BLEND_MODE_INV_DEST_COLOR = 9,
	RS_BLEND_MODE_SRC_ALPHA_SAT = 10,
	RS_BLEND_MODE_BLEND_FACTOR = 11,	// Removed INV_BLEND_FACTOR in favor of _SRC1 dual-source color blending because the inv blend factor blend seems of limited usefulness
	RS_BLEND_MODE_SRC1_COLOR = 12,		// and I want to keep the enum fitting into 4 bits for packing reasons
	RS_BLEND_MODE_INV_SRC1_COLOR = 13,
	RS_BLEND_MODE_SRC1_ALPHA = 14,
	RS_BLEND_MODE_INV_SRC1_ALPHA = 15
};

enum RsColorWriteEnableBits_t
{
	RS_COLOR_WRITE_ENABLE_R = 0x1,
	RS_COLOR_WRITE_ENABLE_G = 0x2,
	RS_COLOR_WRITE_ENABLE_B = 0x4,
	RS_COLOR_WRITE_ENABLE_A = 0x8,
	RS_COLOR_WRITE_ENABLE_ALL = RS_COLOR_WRITE_ENABLE_R | RS_COLOR_WRITE_ENABLE_G | RS_COLOR_WRITE_ENABLE_B | RS_COLOR_WRITE_ENABLE_A
};

enum
{
	RS_MAX_RENDER_TARGETS = 8
};


// description of stencil mode. There's one of these in the RsDepthStencilStateHandle_t plus it's used for overrides
struct RsStencilStateDesc_t
{
	bool m_bStencilEnable;
	uint8 m_nStencilReadMask;
	uint8 m_nStencilWriteMask;

	RsStencilOp_t m_frontStencilFailOp;
	RsStencilOp_t m_frontStencilDepthFailOp;
	RsStencilOp_t m_frontStencilPassOp;
	RsComparison_t m_frontStencilFunc;

	RsStencilOp_t m_backStencilFailOp;
	RsStencilOp_t m_backStencilDepthFailOp;
	RsStencilOp_t m_backStencilPassOp;
	RsComparison_t m_backStencilFunc;
};

// Parameters to override the stencil state and ref value set by the material.
struct RsStencilStateOverride_t
{
	int32 m_nStencilRefValue;								// -1 = don't override
	RsStencilStateDesc_t m_stencilState;					// replaces that used by the material.
};



struct RenderTargetDesc_t
{
	RenderTargetDesc_t()
	{
		Clear();
	}

	void Clear()
	{
		for ( int i = 0; i < RS_MAX_RENDER_TARGETS; i++ )
		{
			m_pColorTargets[ i ] = RENDER_TEXTURE_HANDLE_INVALID;
			m_nColorTargetIndices[ i ] = 0;
		}
		m_hDepthTarget = RENDER_TEXTURE_HANDLE_INVALID;
		m_nDepthTargetIndex = 0;

		m_nColorTargetCount = 0;
		m_allowSrgbWrite = RENDER_LINEAR;
		m_bReadOnlyDepthStencil = false;
	}

	//RenderTargetDesc_t( SwapChainHandle_t hSwapChain, bool bUseColorBuffer = true, bool bUseDepthBuffer = true, RenderColorSpace_t allowSrgbWrites = RENDER_SRGB )
	//{
	//	Clear();
	//	Init( hSwapChain, bUseColorBuffer, bUseDepthBuffer, allowSrgbWrites );
	//}

	RenderTargetDesc_t( HRenderTexture hColor, HRenderTexture hDepth, RenderColorSpace_t allowSrgbWrites = RENDER_SRGB )
	{
		Clear();
		Init( hColor, hDepth, allowSrgbWrites );
	}

	RenderTargetDesc_t( HRenderTexture hColor0, HRenderTexture hColor1, HRenderTexture hDepth, RenderColorSpace_t allowSrgbWrites = RENDER_SRGB )
	{
		Clear();
		Init( hColor0, hColor1, hDepth, allowSrgbWrites );
	}

	RenderTargetDesc_t( HRenderTexture hColor0, HRenderTexture hColor1, HRenderTexture hColor2, HRenderTexture hDepth, RenderColorSpace_t allowSrgbWrites = RENDER_SRGB )
	{
		Clear();
		Init( hColor0, hColor1, hColor2, hDepth, allowSrgbWrites );
	}

	RenderTargetDesc_t( HRenderTexture hColor0, HRenderTexture hColor1, HRenderTexture hColor2, HRenderTexture hColor3, HRenderTexture hDepth, RenderColorSpace_t allowSrgbWrites = RENDER_SRGB )
	{
		Clear();
		Init( hColor0, hColor1, hColor2, hColor3, hDepth, allowSrgbWrites );
	}

	RenderTargetDesc_t( int nCount, HRenderTexture *pColorTargets, HRenderTexture hDepth, RenderColorSpace_t allowSrgbWrites = RENDER_SRGB )
	{
		Clear();
		Init( nCount, pColorTargets, hDepth, allowSrgbWrites );
	}

	// See below for implementation
	void Init( SwapChainHandle_t hSwapChain, bool bUseColorBuffer = true, bool bUseDepthBuffer = true, RenderColorSpace_t allowSrgbWrites = RENDER_SRGB );

	void Init( HRenderTexture hColor, HRenderTexture hDepth, RenderColorSpace_t allowSrgbWrites = RENDER_SRGB )
	{
		m_hDepthTarget = hDepth;
		m_nColorTargetCount = 1;
		m_pColorTargets[ 0 ] = hColor;
		m_allowSrgbWrite = allowSrgbWrites;
	}
	void Init( HRenderTexture hColor0, HRenderTexture hColor1, HRenderTexture hDepth, RenderColorSpace_t allowSrgbWrites = RENDER_SRGB )
	{
		m_hDepthTarget = hDepth;
		m_nColorTargetCount = 2;
		m_pColorTargets[ 0 ] = hColor0;
		m_pColorTargets[ 1 ] = hColor1;
		m_allowSrgbWrite = allowSrgbWrites;
	}
	void Init( HRenderTexture hColor0, HRenderTexture hColor1, HRenderTexture hColor2, HRenderTexture hDepth, RenderColorSpace_t allowSrgbWrites = RENDER_SRGB )
	{
		m_hDepthTarget = hDepth;
		m_nColorTargetCount = 3;
		m_pColorTargets[ 0 ] = hColor0;
		m_pColorTargets[ 1 ] = hColor1;
		m_pColorTargets[ 2 ] = hColor2;
		m_allowSrgbWrite = allowSrgbWrites;
	}
	void Init( HRenderTexture hColor0, HRenderTexture hColor1, HRenderTexture hColor2, HRenderTexture hColor3, HRenderTexture hDepth, RenderColorSpace_t allowSrgbWrites = RENDER_SRGB )
	{
		m_hDepthTarget = hDepth;
		m_nColorTargetCount = 4;
		m_pColorTargets[ 0 ] = hColor0;
		m_pColorTargets[ 1 ] = hColor1;
		m_pColorTargets[ 2 ] = hColor2;
		m_pColorTargets[ 3 ] = hColor3;
		m_allowSrgbWrite = allowSrgbWrites;
	}
	void Init( int nCount, HRenderTexture *pColorTargets, HRenderTexture hDepth, RenderColorSpace_t allowSrgbWrites = RENDER_SRGB )
	{
		m_hDepthTarget = hDepth;
		m_nColorTargetCount = (short)nCount;
		V_memcpy( m_pColorTargets, pColorTargets, nCount * sizeof( HRenderTexture ) );
		m_allowSrgbWrite = allowSrgbWrites;
	}

	void Init( const RenderTargetDesc_t &desc )
	{
		V_memcpy( this, &desc, sizeof( RenderTargetDesc_t ) );
	}

	inline bool IsValid() const
	{
		return ( m_nColorTargetCount && ( m_pColorTargets[ 0 ] != RENDER_TEXTURE_HANDLE_INVALID ) ) ||
			( m_hDepthTarget != RENDER_TEXTURE_HANDLE_INVALID );
	}

	FORCEINLINE void SetReadOnlyDepthStencil( bool bIsReadOnly )
	{
		m_bReadOnlyDepthStencil = bIsReadOnly;
	}

	HRenderTexture m_pColorTargets[ RS_MAX_RENDER_TARGETS ];
	HRenderTexture m_hDepthTarget;
	int m_nColorTargetIndices[ RS_MAX_RENDER_TARGETS ];
	int m_nDepthTargetIndex;
	short m_nColorTargetCount;
	RenderColorSpace_t m_allowSrgbWrite;
	bool m_bReadOnlyDepthStencil : 1;
};

enum RenderShaderType_t
{
	RENDER_SHADER_TYPE_INVALID = -1,
	RENDER_PIXEL_SHADER = 0,
	RENDER_VERTEX_SHADER,
	RENDER_GEOMETRY_SHADER,
	RENDER_HULL_SHADER,
	RENDER_DOMAIN_SHADER,
	RENDER_COMPUTE_SHADER,

	RENDER_SHADER_TYPE_COUNT,
};


// clear flags
enum RenderClearFlags_t
{
	RENDER_CLEAR_FLAGS_CLEAR_COLOR0 = 0x1,
	RENDER_CLEAR_FLAGS_CLEAR_COLOR1 = 0x2,
	RENDER_CLEAR_FLAGS_CLEAR_COLOR2 = 0x4,
	RENDER_CLEAR_FLAGS_CLEAR_COLOR3 = 0x8,
	RENDER_CLEAR_FLAGS_CLEAR_COLOR4 = 0x10,
	RENDER_CLEAR_FLAGS_CLEAR_COLOR5 = 0x20,
	RENDER_CLEAR_FLAGS_CLEAR_COLOR6 = 0x40,
	RENDER_CLEAR_FLAGS_CLEAR_COLOR7 = 0x80,
	RENDER_CLEAR_FLAGS_CLEAR_COLOR = ( RENDER_CLEAR_FLAGS_CLEAR_COLOR0 | RENDER_CLEAR_FLAGS_CLEAR_COLOR1 | RENDER_CLEAR_FLAGS_CLEAR_COLOR2 | RENDER_CLEAR_FLAGS_CLEAR_COLOR3 | RENDER_CLEAR_FLAGS_CLEAR_COLOR4 | RENDER_CLEAR_FLAGS_CLEAR_COLOR5 | RENDER_CLEAR_FLAGS_CLEAR_COLOR6 | RENDER_CLEAR_FLAGS_CLEAR_COLOR7 ),

	RENDER_CLEAR_FLAGS_CLEAR_DEPTH = 0x100,
	RENDER_CLEAR_FLAGS_CLEAR_STENCIL = 0x200,
	RENDER_CLEAR_FLAGS_PRESERVE_HIGH_Z = 0x400,	// Only for 360
};


//-----------------------------------------------------------------------------
// Enums for builtin state objects
//-----------------------------------------------------------------------------
enum RenderCullMode_t
{
	RENDER_CULLMODE_CULL_BACKFACING = 0,		// this culls polygons with clockwise winding
	RENDER_CULLMODE_CULL_FRONTFACING = 1,		// this culls polygons with counterclockwise winding
	RENDER_CULLMODE_CULL_NONE = 2,				// no culling
};

enum RenderZBufferMode_t
{
	RENDER_ZBUFFER_NONE = 0,
	RENDER_ZBUFFER_ZTEST_AND_WRITE,
	RENDER_ZBUFFER_ZTEST_NO_WRITE,
	RENDER_ZBUFFER_ZTEST_GREATER_NO_WRITE,
	RENDER_ZBUFFER_ZTEST_EQUAL_NO_WRITE,

	// Stencil modes
	RENDER_ZBUFFER_NONE_STENCIL_TEST_NOTEQUAL,
	RENDER_ZBUFFER_ZTEST_AND_WRITE_STENCIL_SET1,
	RENDER_ZBUFFER_ZTEST_NO_WRITE_STENCIL_TEST_NOTEQUAL_SET0,
	RENDER_ZBUFFER_ZTEST_GREATER_NO_WRITE_STENCIL_TEST_NOTEQUAL_SET0,

	RENDER_ZBUFFER_ZTEST_NO_WRITE_STENCIL_TEST_EQUAL_1_NOSET,
	RENDER_ZBUFFER_ZTEST_GREATER_NO_WRITE_STENCIL_TEST_EQUAL_1_NOSET,

	RENDER_ZBUFFER_NUM_BUILTIN_MODES
};

enum RenderBlendMode_t
{
	RENDER_BLEND_NONE = 0,									// dest.rgb = src.rgb
	RENDER_BLEND_NOPIXELWRITE = 1,							// no write to dest
	RENDER_BLEND_RGBAPIXELWRITE,							// dest.rgba = src.rgba
	RENDER_BLEND_ALPHABLENDING,								// dest.rgb = lerp( dest.rgb, src.srb, src.a )
	RENDER_BLEND_ADDITIVE_ON_ALPHA,							// dest.rgb = src.rgb * src.a + dest.rgb
	RENDER_BLEND_ADDITIVE,									// dest.rgb += src.rgb
	RENDER_BLEND_MULTIPLY,									// dest.rgba *= src.rgba
	RENDER_NUM_BUILTIN_BLENDSTATES
};


// There are a lot more Query Types than this, but this is the intersection between DX9 and DX11
// And currently, we only really use EVENT and OCCLUSION
enum RenderQueryType_t
{
	RENDER_QUERY_EVENT = 0,             // D3DISSUE_END
	RENDER_QUERY_OCCLUSION          	// D3DISSUE_BEGIN, D3DISSUE_END
	//	RENDER_QUERY_TIMESTAMP,          	// D3DISSUE_END
	//	RENDER_QUERY_TIMESTAMPDISJOINT,  	// D3DISSUE_BEGIN, D3DISSUE_END
	//	RENDER_QUERY_PIPELINETIMINGS    	// D3DISSUE_BEGIN, D3DISSUE_END
};


// Query states
enum RenderQueryState_t
{
	RENDER_QUERY_STATE_INVALID = -1,	// Invalid state
	RENDER_QUERY_STATE_SIGNALED = 0,	// Has app-readable state, can use GetData()
	RENDER_QUERY_STATE_BUILDING,		// Between Begin/End, for example (on the context thread)
	RENDER_QUERY_STATE_QUEUED,			// In our D3D9 queue, but not submitted to D3D (will not occur on DX11)
	RENDER_QUERY_STATE_RT_BUILDING,		// Between Begin/End, for example (on the render submission thread)
	RENDER_QUERY_STATE_ISSUED			// In flight and out of our control.  Can poll to see when signaled
};


enum RenderPrimitiveType_t
{
	RENDER_PRIM_POINTS = 0,
	RENDER_PRIM_LINES,
	RENDER_PRIM_LINES_WITH_ADJACENCY,
	RENDER_PRIM_LINE_STRIP,
	RENDER_PRIM_LINE_STRIP_WITH_ADJACENCY,
	RENDER_PRIM_TRIANGLES,
	RENDER_PRIM_TRIANGLES_WITH_ADJACENCY,
	RENDER_PRIM_TRIANGLE_STRIP,
	RENDER_PRIM_TRIANGLE_STRIP_WITH_ADJACENCY,
	RENDER_PRIM_INSTANCED_QUADS,
	RENDER_PRIM_HETEROGENOUS,
	RENDER_PRIM_1_CONTROL_POINT_PATCHLIST,
	RENDER_PRIM_2_CONTROL_POINT_PATCHLIST,
	RENDER_PRIM_3_CONTROL_POINT_PATCHLIST,
	RENDER_PRIM_4_CONTROL_POINT_PATCHLIST,
	RENDER_PRIM_5_CONTROL_POINT_PATCHLIST,
	RENDER_PRIM_6_CONTROL_POINT_PATCHLIST,
	RENDER_PRIM_7_CONTROL_POINT_PATCHLIST,
	RENDER_PRIM_8_CONTROL_POINT_PATCHLIST,
	RENDER_PRIM_9_CONTROL_POINT_PATCHLIST,
	RENDER_PRIM_10_CONTROL_POINT_PATCHLIST,
	RENDER_PRIM_11_CONTROL_POINT_PATCHLIST,
	RENDER_PRIM_12_CONTROL_POINT_PATCHLIST,
	RENDER_PRIM_13_CONTROL_POINT_PATCHLIST,
	RENDER_PRIM_14_CONTROL_POINT_PATCHLIST,
	RENDER_PRIM_15_CONTROL_POINT_PATCHLIST,
	RENDER_PRIM_16_CONTROL_POINT_PATCHLIST,
	RENDER_PRIM_17_CONTROL_POINT_PATCHLIST,
	RENDER_PRIM_18_CONTROL_POINT_PATCHLIST,
	RENDER_PRIM_19_CONTROL_POINT_PATCHLIST,
	RENDER_PRIM_20_CONTROL_POINT_PATCHLIST,
	RENDER_PRIM_21_CONTROL_POINT_PATCHLIST,
	RENDER_PRIM_22_CONTROL_POINT_PATCHLIST,
	RENDER_PRIM_23_CONTROL_POINT_PATCHLIST,
	RENDER_PRIM_24_CONTROL_POINT_PATCHLIST,
	RENDER_PRIM_25_CONTROL_POINT_PATCHLIST,
	RENDER_PRIM_26_CONTROL_POINT_PATCHLIST,
	RENDER_PRIM_27_CONTROL_POINT_PATCHLIST,
	RENDER_PRIM_28_CONTROL_POINT_PATCHLIST,
	RENDER_PRIM_29_CONTROL_POINT_PATCHLIST,
	RENDER_PRIM_30_CONTROL_POINT_PATCHLIST,
	RENDER_PRIM_31_CONTROL_POINT_PATCHLIST,
	RENDER_PRIM_32_CONTROL_POINT_PATCHLIST,
	RENDER_PRIM_TYPE_COUNT,
};
//DECLARE_SCHEMA_ENUM( RenderPrimitiveType_t );

enum InputLayoutVariation_t
{
	INPUT_LAYOUT_VARIATION_DEFAULT = 0x00,
	INPUT_LAYOUT_VARIATION_STREAM1_MAT3X4,
	INPUT_LAYOUT_VARIATION_STREAM1_INSTANCEID,
	INPUT_LAYOUT_VARIATION_STREAM1_INSTANCEID_IRRADIANCEINFO,
	INPUT_LAYOUT_VARIATION_STREAM1_INSTANCEID_MORPH_VERT_ID,

	INPUT_LAYOUT_VARIATION_MAX
};
//DECLARE_SCHEMA_ENUM( InputLayoutVariation_t );

#define RENDER_INPUT_LAYOUT_INVALID RenderInputLayout_t(0)
#define RENDER_BLEND_STATE_HANDLE_INVALID RsBlendStateHandle_t(0)




struct RsBlendStateDesc_t
{
public:
	RsBlendStateDesc_t();
	explicit RsBlendStateDesc_t( bool bBlendEnable, bool bAlphaToCoverageEnable, bool bIndependendBlendEnable,
								 RsBlendMode_t srcBlend, RsBlendMode_t destBlend, RsBlendOp_t blendOp,
								 RsBlendMode_t srcBlendAlpha, RsBlendMode_t destBlendAlpha, RsBlendOp_t blendOpAlpha,
								 uint8 nRenderTargetWriteMask, bool bSrgbWriteEnable )
	{
		m_mixBlendMode = 0;
		m_blendOpBits = 0;
		m_blendOpAlphaBits = 0;

		SetAlphaToCoverageEnabled( bAlphaToCoverageEnable );
		SetIndependentBlendEnabled( bIndependendBlendEnable );
		for ( int i = 0; i < RS_MAX_RENDER_TARGETS; i++ )
		{
			SetAlphaBlendEnabled( bBlendEnable, i );
			SetSrcBlend( srcBlend, i );
			SetDestBlend( destBlend, i );
			SetBlendOp( blendOp, i );
			SetSrcBlendAlpha( srcBlendAlpha, i );
			SetDestBlendAlpha( destBlendAlpha, i );
			SetBlendOpAlpha( blendOpAlpha, i );
			SetRenderTargetWriteMask( nRenderTargetWriteMask, i );
			SetSrgbWriteEnabled( bSrgbWriteEnable, i );

		}
	}

	FORCEINLINE bool IsAlphaToCoverageEnabled() const {
		return m_bAlphaToCoverageEnable;
	}
	FORCEINLINE void SetAlphaToCoverageEnabled( bool bEnable ) {
		m_bAlphaToCoverageEnable = bEnable;
	}
	FORCEINLINE bool IsIndependentBlendEnabled() const {
		return m_bIndependentBlendEnable;
	}
	FORCEINLINE void SetIndependentBlendEnabled( bool bEnable ) {
		m_bIndependentBlendEnable = bEnable;
	}
	FORCEINLINE bool IsAlphaBlendEnabled( int nRenderTargetIdx = 0 ) const {
		return ( m_blendEnableBits & ( 1 << nRenderTargetIdx ) ) != 0;
	}
	FORCEINLINE void SetAlphaBlendEnabled( bool bEnable, int nRenderTargetIdx = 0 ) {
		m_blendEnableBits |= ( bEnable << nRenderTargetIdx );
	}
	FORCEINLINE bool IsSrgbWriteEnabled( int nRenderTargetIdx = 0 ) const {
		return ( m_srgbWriteEnableBits & ( 1 << nRenderTargetIdx ) ) != 0;
	}
	FORCEINLINE void SetSrgbWriteEnabled( bool bEnable, int nRenderTargetIdx = 0 ) {
		m_srgbWriteEnableBits |= ( bEnable << nRenderTargetIdx );
	}
	FORCEINLINE RsBlendMode_t GetSrcBlend( int nRenderTargetIdx = 0 ) const {
		return static_cast<RsBlendMode_t>( ( m_srcBlendBits >> ( nRenderTargetIdx * 4 ) ) & 0xF );
	}
	FORCEINLINE void SetSrcBlend( RsBlendMode_t blendMode, int nRenderTargetIdx = 0 ) {
		m_srcBlendBits &= ~( 0xF << ( nRenderTargetIdx * 4 ) );
		m_srcBlendBits |= blendMode << ( nRenderTargetIdx * 4 );
	}
	FORCEINLINE RsBlendMode_t GetDestBlend( int nRenderTargetIdx = 0 ) const {
		return static_cast<RsBlendMode_t>( ( m_destBlendBits >> ( nRenderTargetIdx * 4 ) ) & 0xF );
	}
	FORCEINLINE void SetDestBlend( RsBlendMode_t blendMode, int nRenderTargetIdx = 0 ) {
		m_destBlendBits &= ~( 0xF << ( nRenderTargetIdx * 4 ) );
		m_destBlendBits |= blendMode << ( nRenderTargetIdx * 4 );
	}
	FORCEINLINE RsBlendMode_t GetSrcBlendAlpha( int nRenderTargetIdx = 0 ) const {
		return static_cast<RsBlendMode_t>( ( m_srcBlendAlphaBits >> ( nRenderTargetIdx * 4 ) ) & 0xF );
	}
	FORCEINLINE void SetSrcBlendAlpha( RsBlendMode_t blendMode, int nRenderTargetIdx = 0 ) {
		m_srcBlendAlphaBits &= ~( 0xF << ( nRenderTargetIdx * 4 ) );
		m_srcBlendAlphaBits |= blendMode << ( nRenderTargetIdx * 4 );
	}
	FORCEINLINE RsBlendMode_t GetDestBlendAlpha( int nRenderTargetIdx = 0 ) const {
		return static_cast<RsBlendMode_t>( ( m_destBlendAlphaBits >> ( nRenderTargetIdx * 4 ) ) & 0xF );
	}
	FORCEINLINE void SetDestBlendAlpha( RsBlendMode_t blendMode, int nRenderTargetIdx = 0 ) {
		m_destBlendAlphaBits &= ~( 0xF << ( nRenderTargetIdx * 4 ) );
		m_destBlendAlphaBits |= blendMode << ( nRenderTargetIdx * 4 );
	}
	FORCEINLINE uint8 GetRenderTargetWriteMask( int nRenderTargetIdx = 0 ) const {
		return ( m_renderTargetWriteMaskBits >> ( nRenderTargetIdx * 4 ) ) & 0xF;
	}
	FORCEINLINE void SetRenderTargetWriteMask( uint8 nWriteMask, int nRenderTargetIdx = 0 ) {
		m_renderTargetWriteMaskBits &= ~( 0xF << ( nRenderTargetIdx * 4 ) );
		m_renderTargetWriteMaskBits |= ( nWriteMask & 0xF ) << ( nRenderTargetIdx * 4 );
	}
	FORCEINLINE RsBlendOp_t GetBlendOp( int nRenderTargetIdx = 0 ) const {
		return static_cast<RsBlendOp_t>( ( m_blendOpBits >> ( nRenderTargetIdx * 3 ) ) & 0x7 );
	}
	FORCEINLINE void SetBlendOp( RsBlendOp_t blendOp, int nRenderTargetIdx = 0 ) {
		m_blendOpBits &= ~( 0x7 << ( nRenderTargetIdx * 3 ) );
		m_blendOpBits |= blendOp << ( nRenderTargetIdx * 3 );
	}
	FORCEINLINE RsBlendOp_t GetBlendOpAlpha( int nRenderTargetIdx = 0 ) const {
		return static_cast<RsBlendOp_t>( ( m_blendOpAlphaBits >> ( nRenderTargetIdx * 3 ) ) & 0x7 );
	}
	FORCEINLINE void SetBlendOpAlpha( RsBlendOp_t blendOp, int nRenderTargetIdx = 0 ) {
		m_blendOpAlphaBits &= ~( 0x7 << ( nRenderTargetIdx * 3 ) );
		m_blendOpAlphaBits |= blendOp << ( nRenderTargetIdx * 3 );
	}

	FORCEINLINE uint32 HashValue() const
	{
		// TODO: Optimize this
		return HashItem( *this );
	}

	FORCEINLINE bool operator==( RsBlendStateDesc_t const &state ) const
	{
		return memcmp( this, &state, sizeof( RsBlendStateDesc_t ) ) == 0;
	}

	void Serialize( CUtlBuffer &buf ) const
	{
		buf.PutChar( m_bAlphaToCoverageEnable );
		buf.PutChar( m_bIndependentBlendEnable );
		buf.PutChar( 0 /*m_bHighPrecisionBlendEnable360*/ );

		for ( uint i = 0; i < RS_MAX_RENDER_TARGETS; i++ )
			buf.PutChar( IsAlphaBlendEnabled( i ) );

		for ( uint i = 0; i < RS_MAX_RENDER_TARGETS; i++ )
			buf.PutChar( (char)GetSrcBlend( i ) );

		for ( uint i = 0; i < RS_MAX_RENDER_TARGETS; i++ )
			buf.PutChar( (char)GetDestBlend( i ) );

		for ( uint i = 0; i < RS_MAX_RENDER_TARGETS; i++ )
			buf.PutChar( (char)GetBlendOp( i ) );

		for ( uint i = 0; i < RS_MAX_RENDER_TARGETS; i++ )
			buf.PutChar( (char)GetSrcBlendAlpha( i ) );

		for ( uint i = 0; i < RS_MAX_RENDER_TARGETS; i++ )
			buf.PutChar( (char)GetDestBlendAlpha( i ) );

		for ( uint i = 0; i < RS_MAX_RENDER_TARGETS; i++ )
			buf.PutChar( (char)GetBlendOpAlpha( i ) );

		for ( uint i = 0; i < RS_MAX_RENDER_TARGETS; i++ )
			buf.PutChar( GetRenderTargetWriteMask( i ) );

		for ( uint i = 0; i < RS_MAX_RENDER_TARGETS; i++ )
			buf.PutChar( IsSrgbWriteEnabled( i ) );
	}

	void Unserialize( CUtlBuffer &buf )
	{
		m_bAlphaToCoverageEnable = ( buf.GetChar() != 0 );
		m_bIndependentBlendEnable = ( buf.GetChar() != 0 );
		//m_bHighPrecisionBlendEnable360 = ( buf.GetChar() != 0 );
		buf.GetChar();

		for ( uint i = 0; i < RS_MAX_RENDER_TARGETS; i++ )
			SetAlphaBlendEnabled( buf.GetChar() != 0, i );

		for ( uint i = 0; i < RS_MAX_RENDER_TARGETS; i++ )
			SetSrcBlend( static_cast<RsBlendMode_t>( buf.GetChar() ), i );

		for ( uint i = 0; i < RS_MAX_RENDER_TARGETS; i++ )
			SetDestBlend( static_cast<RsBlendMode_t>( buf.GetChar() ), i );

		for ( uint i = 0; i < RS_MAX_RENDER_TARGETS; i++ )
			SetBlendOp( static_cast<RsBlendOp_t>( buf.GetChar() ), i );

		for ( uint i = 0; i < RS_MAX_RENDER_TARGETS; i++ )
			SetSrcBlendAlpha( static_cast<RsBlendMode_t>( buf.GetChar() ), i );

		for ( uint i = 0; i < RS_MAX_RENDER_TARGETS; i++ )
			SetDestBlendAlpha( static_cast<RsBlendMode_t>( buf.GetChar() ), i );

		for ( uint i = 0; i < RS_MAX_RENDER_TARGETS; i++ )
			SetBlendOpAlpha( static_cast<RsBlendOp_t>( buf.GetChar() ), i );

		for ( uint i = 0; i < RS_MAX_RENDER_TARGETS; i++ )
			SetRenderTargetWriteMask( buf.GetChar(), i );

		for ( uint i = 0; i < RS_MAX_RENDER_TARGETS; i++ )
			SetSrgbWriteEnabled( buf.GetChar() != 0, i );
	}


	uint32 m_mixBlendMode;

private:
	uint32 m_srcBlendBits;
	uint32 m_destBlendBits;
	uint32 m_srcBlendAlphaBits;
	uint32 m_destBlendAlphaBits;
	uint32 m_renderTargetWriteMaskBits;
	uint32 m_blendOpBits : 30;
	uint32 m_bAlphaToCoverageEnable : 1;
	uint32 m_bIndependentBlendEnable : 1;
	uint32 m_blendOpAlphaBits;
	uint8 m_blendEnableBits;
	uint8 m_srgbWriteEnableBits;
};

FORCEINLINE const RsBlendStateDesc_t &DefaultBlendStateDesc()
{
	static const RsBlendStateDesc_t defaultBlendState( false, false, false,
													   RS_BLEND_MODE_SRC_ALPHA, RS_BLEND_MODE_INV_SRC_ALPHA, RS_BLEND_OP_ADD,
													   RS_BLEND_MODE_SRC_ALPHA, RS_BLEND_MODE_INV_SRC_ALPHA, RS_BLEND_OP_ADD,
													   0xF,
													   false );
	return defaultBlendState;
}


FORCEINLINE RsBlendStateDesc_t::RsBlendStateDesc_t()
{
	COMPILE_TIME_ASSERT( RS_MAX_RENDER_TARGETS <= 8 );
	COMPILE_TIME_ASSERT( RS_BLEND_MODE_INV_SRC1_ALPHA < 16 );
	COMPILE_TIME_ASSERT( RS_BLEND_OP_MAX < 8 );
	V_memcpy( this, &DefaultBlendStateDesc(), sizeof( RsBlendStateDesc_t ) );
}

class CTextureDesc;

struct LockDesc_t
{
	void *m_pMemory;
};

typedef int VertexBufferHandle_t;

enum RenderInputLayout_t
{
	RIL_STDVERT = 1,			// POS and 3 tex coord
	RIL_FANCY = 2				// POS and 8 tex coords
};

enum RsBlendStateHandle_t
{
	BLENDSTATE_ALPHA = 1,
	BLENDSTATE_PREMULT_ALPHA,
	BLENDSTATE_ONLY_ALPHA,
	BLENDSTATE_MIX_SCREEN,
	BLENDSTATE_MIX_MULTIPLY,
	BLENDSTATE_MIX_ADDITIVE,
	BLENDSTATE_MIX_ADDITIVESRGB,
	BLENDSTATE_MIX_OPAQUE,
	BLENDSTATE_MAX
};

enum RenderShaderHandle_t
{
	RENDER_SHADER_HANDLE_INVALID = 0,
	SHADER_PANORAMA = 1,
	SHADER_PANORAMA_FANCY,
	SHADER_BASIC_QUAD
};

typedef uint64 ConstantBufferHandle_t;

#define CONSTANT_BUFFER_HANDLE_INVALID (0xffffffffffffffff)

class CVsInputSignatureVector
{
public:

	uint64 m_nHandle;
};

struct RenderClearInfo_t
{
	const Vector4D *m_pClearColorArray;
	int m_nNumColors;
	int m_nFlags;
	int m_nStencilBitToCheck;
	int m_nStencilComparisonValue;

	// NOTE: This does not copy the color so the info can't live longer
	// than the color value passed in.
	RenderClearInfo_t( const Vector4D &vecRGBAColor, int nFlags = RENDER_CLEAR_FLAGS_CLEAR_COLOR )
	{
		m_pClearColorArray = &vecRGBAColor;
		m_nNumColors = 1;
		m_nFlags = nFlags;
		m_nStencilBitToCheck = -1;
		m_nStencilComparisonValue = 0;
	}
	RenderClearInfo_t( const Vector4D *pClearColorArray, int nNumColors, int nFlags = RENDER_CLEAR_FLAGS_CLEAR_COLOR, int nStencilBitToCheck = -1, int nStencilComparisonValue = 0 )
	{
		m_pClearColorArray = pClearColorArray;
		m_nNumColors = nNumColors;
		m_nFlags = nFlags;
		m_nStencilBitToCheck = nStencilBitToCheck;
		m_nStencilComparisonValue = nStencilComparisonValue;
	}
};




//typedef uint32 RenderInputLayout_t;
//typedef uint32 RsBlendStateHandle_t;
typedef uint32 HRenderBuffer;

typedef ResourceHandle_t HRenderableStrong;

typedef CUtlDelegate< void( const void * )> DataRecycleDelegate_t;

//-----------------------------------------------------------------------------
// Render context interface
//-----------------------------------------------------------------------------

class IMaterialMode;

enum RenderTextureDetail_t
{
	RENDER_TEXTURE_DETAIL_DEFAULT = 0,
	RENDER_TEXTURE_DETAIL_LOW = -1,
	RENDER_TEXTURE_DETAIL_MEDIUM = -2,
	RENDER_TEXTURE_DETAIL_HIGH = -3,
	RENDER_TEXTURE_DETAIL_INVALID = -4,
};

//-----------------------------------------------------------------------------
// Render Texture Dimension
//-----------------------------------------------------------------------------
enum RenderTextureDimension_t : int8
{
	RENDER_TEXTURE_DIMENSION_INVALID = -1,
	RENDER_TEXTURE_DIMENSION_1D = 0,
	RENDER_TEXTURE_DIMENSION_2D,
	RENDER_TEXTURE_DIMENSION_2D_ARRAY,
	RENDER_TEXTURE_DIMENSION_3D,
	RENDER_TEXTURE_DIMENSION_CUBE,
	RENDER_TEXTURE_DIMENSION_CUBE_ARRAY,

	RENDER_TEXTURE_DIMENSION_COUNT,
};


abstract_class IRenderContext
{
	// rendering functions. in flux
public:

	IRenderContext() 
	{
	
	}

	enum ClearSubrectResult_t
	{
		// Basic operations.
		k_EClearedError,	// Error during the clear.
		k_EClearedNone,		// Nothing needed to be cleared (such as no render target bound).
		k_EClearedSubrect,	// Cleared just the subrect.
		k_EClearedTarget,	// Cleared the full target.

							// Bits added to the above to indicate what pieces of render state, if any,
							// were changed during the clear.
							k_EClearedChangedRenderState = 0x80000000, // Arbitrary render state changed, state is unknown.
							k_EClearedSetTargetViewport = 0x40000000, // Viewport set to full target.
							k_EClearedSetSubrectViewport = 0x20000000, // Viewport set to subrect.
	};

	/// Called when the context is handed to a thread.
	virtual void AttachToCurrentThread() = 0;

	//	/// Clear the rendertarget. You can specify a different vec4d color for each element of it. If
	//	/// you supply fewer elements than the number of elements bound in the render target, the last
	//	/// supplied color will be used for those targets.
	//	/// Note that Clear will obey the viewport. On some hardware, Clear will suffer
	//	/// performance penalties if the viewport doesn't match the full render target dimensions.
	//	/// Use IRenderHardwareConfig::IsFastSubrectClearSupported to detect if this is the case.
	//	// nFlags is an OR of RENDER_CLEAR_FLAGS_CLEAR_xxx flags
	virtual void Clear( const Vector4D *pClearColorArray, int nNumColors, int nFlags = RENDER_CLEAR_FLAGS_CLEAR_COLOR, int nStencilBitToCheck = -1, int nStencilComparisonValue = 0 ) = 0;
	//
	//	/// shortcut backwards-compatible function for clearing all render target elements to the same color
	FORCEINLINE void Clear( const Vector4D &vecRGBAColor, int nFlags = RENDER_CLEAR_FLAGS_CLEAR_COLOR )
	{
		Clear( &vecRGBAColor, 1, nFlags );
	}

	virtual void SetViewports( int nCount, const RenderViewport_t* pViewports ) = 0;
	virtual void GetViewport( RenderViewport_t *pViewport, int nViewport ) = 0;
	//
	//	// Restriction: This can only be called as the first call after a context is created, or the
	//	// first call after a Submit.
	virtual bool BindRenderTargets( const RenderTargetDesc_t &renderTargetDesc ) = 0;
//	virtual void UnlockVertexBuffer( VertexBufferHandle_t hVertexBuffer, int nWrittenSizeInBytes, LockDesc_t *pDesc ) = 0;
	virtual void SetTextureData( HRenderTexture hTexture, const CTextureDesc *pDataDesc, const void *pData,
								 int nDataSize, bool bIsPreTiled = false, int nSpecificMipLevelToSet = -1, 
								 Rect3D_t const *pSubRectToUpdate = nullptr, uint32 nTextureUpdateFlags = 0, 
								 const DataRecycleDelegate_t *pDataRecycleDelegate = nullptr ) = 0;
	virtual void CtxDraw( RenderPrimitiveType_t type, int nFirstVertex, int nVertexCount ) = 0;
	virtual void SetCullMode( RenderCullMode_t eCullMode ) = 0;
	//virtual void SetBlendMode( RenderBlendMode_t eBlendMode, float const *pBlendFactor = nullptr ) = 0;
	virtual void SetZBufferMode( RenderZBufferMode_t eZBufferMode ) = 0;
	virtual void SetBlendState( RsBlendStateHandle_t blendState, float const *pBlendFactor = nullptr, uint32 nSampleMask = 0xFFFFFFFF ) = 0;
	virtual void SetScissorRects( int nCount, const Rect_t *pRects ) = 0;
	FORCEINLINE void SetScissorRect( Rect_t const &rect )
	{
		SetScissorRects( 1, &rect );
	}

	virtual ClearSubrectResult_t ClearSubrect( const RenderClearInfo_t &clearInfo, int nX, int nY, int nWidth, int nHeight, bool bForceSubrect = false ) = 0;

	// New functions from the latest integration



	virtual void BindTexture( int nTextureIndex, HRenderTexture hTexture, RenderTextureDimension_t nBindType, RenderShaderType_t nTargetPipelineStage = RENDER_PIXEL_SHADER,
							  RenderColorSpace_t nColorSpace = RENDER_LINEAR, int nRequiredMipSize = RENDER_TEXTURE_DETAIL_DEFAULT ) {};

	void ResetMaterialStateHint() {}

	inline const IMaterialMode *GetMaterialStateHintMode() const
	{
		return 0;
	
	}
	FORCEINLINE HRenderTexture GetMaterialStateHintTexture() const
	{
		return 0;
	
	}
	FORCEINLINE uint64 GetMaterialStateHintKey() const
	{
		return 0;
	
	}


	inline void SetMaterialStateHint( const IMaterialMode *pMode, HRenderTexture hTexture, uint64 nKey ) {}



protected:

};


//-----------------------------------------------------------------------------
// Implement Render context interface
//-----------------------------------------------------------------------------

#define RESOURCE_TYPE_BACKBUFFER RESOURCE_TYPE_8CC( 'b', 'a', 'c', 'k', 'b', 'u', 'f', 'f' )

#include "materialsystem2/imaterialsystem2.h"



class CRenderContext : public IRenderContext
{
	// rendering functions. in flux
public:
	/// Called when the context is handed to a thread.
	virtual void AttachToCurrentThread();

	//	/// Clear the rendertarget. You can specify a different vec4d color for each element of it. If
	//	/// you supply fewer elements than the number of elements bound in the render target, the last
	//	/// supplied color will be used for those targets.
	//	/// Note that Clear will obey the viewport. On some hardware, Clear will suffer
	//	/// performance penalties if the viewport doesn't match the full render target dimensions.
	//	/// Use IRenderHardwareConfig::IsFastSubrectClearSupported to detect if this is the case.
	//	// nFlags is an OR of RENDER_CLEAR_FLAGS_CLEAR_xxx flags
	virtual void Clear( const Vector4D *pClearColorArray, int nNumColors, int nFlags = RENDER_CLEAR_FLAGS_CLEAR_COLOR, int nStencilBitToCheck = -1, int nStencilComparisonValue = 0 );
	//

	virtual void SetViewports( int nCount, const RenderViewport_t* pViewports );
	virtual void GetViewport( RenderViewport_t *pViewport, int nViewport );
	//
	//	// Restriction: This can only be called as the first call after a context is created, or the
	//	// first call after a Submit.
	virtual bool BindRenderTargets( const RenderTargetDesc_t &renderTargetDesc );

	// Render target access for debugger RT
	virtual void PushDebuggerRenderTarget( const RenderTargetDesc_t &renderTargetDesc );
	virtual void PopDebuggerRenderTarget();
	
	virtual void SetTextureData( HRenderTexture hTexture, const CTextureDesc *pDataDesc, const void *pData,
								 int nDataSize, bool bIsPreTiled = false, int nSpecificMipLevelToSet = -1,
								 Rect3D_t const *pSubRectToUpdate = nullptr, uint32 nTextureUpdateFlags = 0,
								 const DataRecycleDelegate_t *pDataRecycleDelegate = nullptr );
	virtual void CtxDraw( RenderPrimitiveType_t type, int nFirstVertex, int nVertexCount );
	virtual void SetCullMode( RenderCullMode_t eCullMode );
	//virtual void SetBlendMode( RenderBlendMode_t eBlendMode, float const *pBlendFactor = nullptr );
	virtual void SetZBufferMode( RenderZBufferMode_t eZBufferMode );
	virtual void SetBlendState( RsBlendStateHandle_t blendState, float const *pBlendFactor = nullptr, uint32 nSampleMask = 0xFFFFFFFF );
	//virtual CRenderAttributes &GetAttributesForModify();
	virtual void SetScissorRects( int nCount, const Rect_t *pRects );

	virtual ClearSubrectResult_t ClearSubrect( const RenderClearInfo_t &clearInfo, int nX, int nY, int nWidth, int nHeight, bool bForceSubrect = false )
	{
		DevAssertMsg( 0, "Implement me (if necessary) !!!" );
		return k_EClearedError;
	}

	// Local code and data

	void UpdateMesh(IMesh* pMesh);
	void UpdateMaterial();

	CRenderAttributes* m_pAttr;
	CMatRenderContextPtr m_pMatRenderContext;

	CRenderContext( IMaterialSystem * pMatSys )
	: 
		m_pMatRenderContext( pMatSys )
	{
// 		m_hCurrentRT = HRenderTexture( 0, RESOURCE_TYPE_NONE );
// 		m_nScissorRects = 0;

		if ( !m_apPanMaterial[0] )
		{
			for ( int i = 0; i < BLENDSTATE_MAX; i++ )
			{
				char mtlName[ MAX_PATH ];
				V_sprintf_safe( mtlName, "panorama%d", i );
				KeyValues *pVMTKeyValues = new KeyValues( "panorama" );
				pVMTKeyValues->SetInt( "$blendstate", i );
				pVMTKeyValues->SetInt( "$renderattr", 0 );
#ifdef PLATFORM_64BITS
				pVMTKeyValues->SetInt( "$renderattr_high", 0 );
#endif
				m_apPanMaterial[i] = g_pMaterialSystem->CreateMaterial( mtlName, pVMTKeyValues );

				V_sprintf_safe( mtlName, "panoramafancy%d", i );
				KeyValues *pVMTKeyValuesFancy = new KeyValues( "panoramafancy" );
				pVMTKeyValuesFancy->SetInt( "$blendstate", i );
				pVMTKeyValuesFancy->SetInt( "$renderattr", 0 );
#ifdef PLATFORM_64BITS
				pVMTKeyValuesFancy->SetInt( "$renderattr_high", 0 );
#endif
				m_apFancyMaterial[ i ] = g_pMaterialSystem->CreateMaterial( mtlName, pVMTKeyValuesFancy );
			}
		}
	}

	RsBlendStateHandle_t m_blendState;

	int				m_nPanMaterial;
	IMaterial*		m_pMaterial;

	static IMaterial* m_apPanMaterial[ BLENDSTATE_MAX ];				// one per render state
	static IMaterial* m_apFancyMaterial[ BLENDSTATE_MAX ];

	int m_nVertCount;
	void* m_pBaseVB;

	static int				m_nScissorRects;
	static ResourceData_t	m_backBufferResourceData;
	static HRenderTexture	m_hCurrentRT;
};


#endif // INCLUDED_IRENDERCONTEXT_H