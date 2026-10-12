#ifndef INCLUDED_IRENDERDEVICE_H
#define INCLUDED_IRENDERDEVICE_H
//================ Copyright (c) Valve Corporation. All Rights Reserved. ===========================
//
//
//
//==================================================================================================

#include "bitmap/imageformat.h"
#include "vertexcolor.h"
#include "resourcesystem/iresourcesystem.h"

//-----------------------------------------------------------------------------
// Window creation
//-----------------------------------------------------------------------------

#define	WINDOW_CREATE_SHOW_FRAME  0x0001
#define	WINDOW_CREATE_RESIZING  0x0002
#define	WINDOW_CREATE_SHOW_TASKBAR_ICON  0x0004
#define	WINDOW_CREATE_DISABLED  0x0008
#define	WINDOW_CREATE_VISIBLE  0x0010
#define	WINDOW_CREATE_BRING_TO_FOREGROUND  0x0020
#define	WINDOW_CREATE_FULLSCREEN  0x0040
#define	WINDOW_CREATE_COOP_FULLSCREEN  0x0080
	// There are times where a game may not be polling window
	// events in a timely fashion and the OS may think the
	// app is hung.  This flag when supported by the OS
	// allows those checks to be disabled so that the user
	// doesn't get hung-app notifications when the app expects
	// to have times when it appears hung.
#define	WINDOW_CREATE_NO_HANG_CHECKS  0x0100

//DECLARE_POINTER_HANDLE( HResourceManifest );
#define RESOURCE_MANIFEST_HANDLE_INVALID ( (HResourceManifest)0 )
#define RESOURCE_TYPE_RESOURCE_MANIFEST RESOURCE_TYPE_8CC( 'v', 'r', 'm', 'a', 'n', 0, 0, 0 )


//-----------------------------------------------------------------------------
// Render Device API
//-----------------------------------------------------------------------------
enum RenderDeviceAPI_t
{
	// PC
	RENDER_DEVICE_API_DX9 = 0,
	RENDER_DEVICE_API_DX11,
	RENDER_DEVICE_API_GL,
	RENDER_DEVICE_API_MANTLE,
	RENDER_DEVICE_API_VULKAN,

	// Empty
	RENDER_DEVICE_API_EMPTY,

	RENDER_DEVICE_API_MAX
};

//-----------------------------------------------------------------------------
// Adapter info
//-----------------------------------------------------------------------------
const int RENDER_ADAPTER_NAME_LENGTH = 512;

// Vendor IDs sometimes needed for vendor-specific code
#define VENDORID_NVIDIA	0x10DE
#define VENDORID_ATI	0x1002
#define VENDORID_INTEL  0x8086


struct RenderAdapterInfo_t
{
	char m_pDriverName[ RENDER_ADAPTER_NAME_LENGTH ];
	unsigned int m_VendorID;
	unsigned int m_DeviceID;
	unsigned int m_SubSysID;
	unsigned int m_Revision;
	int m_nDXSupportLevel;			// This is the *preferred* dx support level
	int m_nMinDXSupportLevel;
	int m_nMaxDXSupportLevel;
	unsigned int m_nDriverVersionHigh;
	unsigned int m_nDriverVersionLow;
};


//-----------------------------------------------------------------------------
// Flags to be used with the CreateDevice call
//-----------------------------------------------------------------------------
enum RenderCreateDeviceFlags_t
{
	RENDER_CREATE_DEVICE_RESIZE_WINDOWS = 0x1,
	RENDER_CREATE_SOFTWARE_RASTERIZER = 0x2,
	RENDER_CREATE_DEVICE_WITHOUT_FRAME_LATENCY_LIMIT = 0x4,

};

//-----------------------------------------------------------------------------
// for vertex/index buffers. What type is it?
//-----------------------------------------------------------------------------
enum RenderBufferType_t
{
	RENDER_BUFFER_TYPE_STATIC = 0,			// GPU can read from it only, CPU can only write once
	RENDER_BUFFER_TYPE_SEMISTATIC,			// GPU can read, writes are infrequent from CPU
	RENDER_BUFFER_TYPE_STAGING,				// GPU can write, CPU can read
	RENDER_BUFFER_TYPE_GPU_ONLY,			// GPU can read/write, CPU cannot read/write (used for GPU-generated data)

	RENDER_BUFFER_TYPE_COUNT,
};

//-----------------------------------------------------------------------------
// Constant buffer types:
//
// Static:
// Implied scope: Global
// The initial state of static buffers must be provided at creation time, and
// cannot be updated.
//
// Semistatic:
// Implied scope: Global
// The most flexible type. Can be updated and binded to different render contexts (i.e.
// you can update a semistatic buffer on one context, then only bind it to many others, 
// even on different frames).
// Can be updated/bound by multiple contexts on multiple threads in parallel.
// State remains valid across frames.
//
// On 360, the GPU must be used to update the global state of semistatic buffers,
// so only use these when necessary. semistatic buffers have a global 
// buffer which is updated by the GPU, and a context local buffer which lives on 
// the GPU framestack. Multiple updates to a semi-static buffer within a render context
// are as efficient as updating dynamic constant buffers, but right before the 
// a render context's command stream is detached the final GPU framestack buffer 
// will be GPU copied to the global buffer.
//
// Dynamic:
// Implied scope: Per-frame or per-context
// - Those created at the device level (by IRenderDevice::CreateConstantBuffer()) must 
// always be updated before binding them *within the same render context*. Can be 
// updated+bound by multiple render contexts on multiple threads in parallel. 
// Use this type if you are *always* going to update a buffer, then immediately bind it
// within the same render context.
//
// - Those created at the context level (by IRenderContext::GetDynamicConstantBuffer())
// are initialized at creation time, cannot be updated, but can be binded to multiple 
// render contexts within a single frame.
//
// On 360, both types of dynamic buffers live on the GPU framestack.
//
//-----------------------------------------------------------------------------
enum ConstantBufferType_t
{
	CONSTANT_BUFFER_TYPE_SEMISTATIC,		// GPU can read, writes are infrequent from CPU
	CONSTANT_BUFFER_TYPE_DYNAMIC,			// GPU can read, writes are frequent from CPU

	CONSTANT_BUFFER_TYPE_COUNT
};

enum
{
	RENDER_SYSTEM_MAX_INPUT_LAYOUT_ELEMENTS = 16
};


//-----------------------------------------------------------------------------
// Display mode flags
//-----------------------------------------------------------------------------
enum RenderDisplayModeFlags_t
{
	// Display mode usage flags.  Borderless window
	// is distinguished from window because a window frame
	// takes more space so it's allowed to have a borderless
	// window that exactly matches an adapter's monitor resolution
	// but a window with a frame would not fit.
	RENDER_DISPLAY_MODE_EXCLUSIVE_FULLSCREEN = 0x00000001, // True fullscreen
	RENDER_DISPLAY_MODE_COOPERATIVE_FULLSCREEN = 0x00000002, // Desktop-cooperative fullscreen
	RENDER_DISPLAY_MODE_BORDERLESS_WINDOW = 0x00000004,
	RENDER_DISPLAY_MODE_BORDERED_WINDOW = 0x00000008,
	RENDER_DISPLAY_MODE_ALL_USAGE = 0x0000000f,
};

//-----------------------------------------------------------------------------
// Describes how to set the mode
//-----------------------------------------------------------------------------
#define RENDER_DISPLAY_MODE_VERSION 1

struct RenderDisplayMode_t
{
	RenderDisplayMode_t() { memset( this, 0, sizeof( RenderDisplayMode_t ) ); m_nVersion = RENDER_DISPLAY_MODE_VERSION; }

	int m_nVersion;
	int m_nWidth;					// 0 when running windowed means use desktop resolution
	int m_nHeight;
	ImageFormat m_Format;			// use ImageFormats (ignored for windowed mode)
	int m_nRefreshRateNumerator;	// Refresh rate. Use 0 in numerator + denominator for a default setting.
	int m_nRefreshRateDenominator;	// Refresh rate = numerator / denominator.
	uint32 m_nFlags;
};

enum RenderMultisampleType_t
{
	RENDER_MULTISAMPLE_INVALID = -1,
	RENDER_MULTISAMPLE_NONE = 0,
	RENDER_MULTISAMPLE_2X = 1,
	RENDER_MULTISAMPLE_4X = 2,
	RENDER_MULTISAMPLE_6X = 3,
	RENDER_MULTISAMPLE_8X = 4,
	RENDER_MULTISAMPLE_16X = 5,

	RENDER_MULTISAMPLE_TYPE_COUNT
};

//-----------------------------------------------------------------------------
// Specifies the intended texture usage. 
// Render targets must be GPU_ONLY.
//-----------------------------------------------------------------------------
enum TextureUsage_t
{
	TEXTURE_USAGE_INVALID = -1,

	TEXTURE_USAGE_DEFAULT = 0,

	// GPU can read from it only, CPU can only write once
	// Scope must be GLOBAL.
	TEXTURE_USAGE_STATIC,

	// GPU can read, writes are infrequent from CPU
	// Scope must be GLOBAL.
	TEXTURE_USAGE_SEMISTATIC,

	// GPU can read, writes are frequent from CPU
	// Scope can be global, per-frame, or per-context.
	TEXTURE_USAGE_DYNAMIC,

	// GPU can read/write, CPU will never read
	// Currently, scope must be global.
	TEXTURE_USAGE_GPU_ONLY,

	TEXTURE_USAGE_COUNT
};


//-----------------------------------------------------------------------------
// Specifies the intended texture scope. 
//-----------------------------------------------------------------------------
enum TextureScope_t
{
	TEXTURE_SCOPE_INVALID = -1,

	TEXTURE_SCOPE_GLOBAL = 0,
	TEXTURE_SCOPE_DEFAULT = TEXTURE_SCOPE_GLOBAL,

	TEXTURE_SCOPE_PER_FRAME,

	TEXTURE_SCOPE_PER_CONTEXT,

	TEXTURE_SCOPE_COUNT
};


//-----------------------------------------------------------------------------
// Describes how to set the device
//-----------------------------------------------------------------------------
#define RENDER_DEVICE_INFO_VERSION 1

struct RenderDeviceInfo_t
{
	RenderDeviceInfo_t() { memset( this, 0, sizeof( RenderDeviceInfo_t ) ); m_nVersion = RENDER_DEVICE_INFO_VERSION; m_DisplayMode.m_nVersion = RENDER_DISPLAY_MODE_VERSION; }

	int m_nVersion;
	RenderDisplayMode_t m_DisplayMode;
	int m_nBackBufferCount;				// valid values are 1 or 2 [2 results in triple buffering]
	RenderMultisampleType_t m_nMultisampleType;

	uint8 m_nModeUsage;                 // RENDER_DISPLAY_MODE usage flags for fullscreen/windowed.

	bool m_bUseStencil : 1;
	bool m_bWaitForVSync : 1;			// Would we not present until vsync?
	bool m_bScaleToOutputResolution : 1;// 360 ONLY: sets up hardware scaling
	bool m_bProgressive : 1;			// 360 ONLY: interlaced or progressive
	bool m_bUsingMultipleWindows : 1; 	// Forces D3DPresent to use _COPY instead
	bool m_bIsMainWindow : 1;

	//
	// Mode usage helpers.
	//

	bool HasUsageType() const
	{
		return ( m_nModeUsage & RENDER_DISPLAY_MODE_ALL_USAGE ) != 0;
	}

	bool IsExclusiveFullscreen() const
	{
		return ( m_nModeUsage & RENDER_DISPLAY_MODE_EXCLUSIVE_FULLSCREEN ) != 0;
	}
	bool IsCooperativeFullscreen() const
	{
		return ( m_nModeUsage & RENDER_DISPLAY_MODE_COOPERATIVE_FULLSCREEN ) != 0;
	}
	bool IsBorderlessWindow() const
	{
		return ( m_nModeUsage & RENDER_DISPLAY_MODE_BORDERLESS_WINDOW ) != 0;
	}
	bool IsBorderedWindow() const
	{
		return ( m_nModeUsage & RENDER_DISPLAY_MODE_BORDERED_WINDOW ) != 0;
	}

	bool HasWindowBorder() const
	{
		return ( m_nModeUsage & RENDER_DISPLAY_MODE_BORDERED_WINDOW ) != 0;
	}
	// All modes other than true fullscreen use a regular window underneath.
	bool HasWindowedState() const
	{
		return ( m_nModeUsage & RENDER_DISPLAY_MODE_EXCLUSIVE_FULLSCREEN ) == 0;
	}
	bool DoesCoverMonitor() const
	{
		return ( m_nModeUsage & ( RENDER_DISPLAY_MODE_EXCLUSIVE_FULLSCREEN |
			RENDER_DISPLAY_MODE_COOPERATIVE_FULLSCREEN ) ) != 0;
	}

	void SetExclusiveFullscreen()
	{
		m_nModeUsage = ( m_nModeUsage & ~RENDER_DISPLAY_MODE_ALL_USAGE ) |
			RENDER_DISPLAY_MODE_EXCLUSIVE_FULLSCREEN;
	}
	void SetCooperativeFullscreen()
	{
		m_nModeUsage = ( m_nModeUsage & ~RENDER_DISPLAY_MODE_ALL_USAGE ) |
			RENDER_DISPLAY_MODE_COOPERATIVE_FULLSCREEN;
	}
	void SetBorderlessWindow()
	{
		m_nModeUsage = ( m_nModeUsage & ~RENDER_DISPLAY_MODE_ALL_USAGE ) |
			RENDER_DISPLAY_MODE_BORDERLESS_WINDOW;
	}
	void SetBorderedWindow()
	{
		m_nModeUsage = ( m_nModeUsage & ~RENDER_DISPLAY_MODE_ALL_USAGE ) |
			RENDER_DISPLAY_MODE_BORDERED_WINDOW;
	}

	// Video settings are stored as separate bools so simplify
	// splitting and combining those.
	void GetSettingsFlags( bool *pFullscreen,
						   bool *pCoopFullscreen,
						   bool *pNoBorder ) const
	{
		if ( IsExclusiveFullscreen() )
		{
			*pFullscreen = true;
			*pCoopFullscreen = false;
			*pNoBorder = true;
		}
		else if ( IsCooperativeFullscreen() )
		{
			*pFullscreen = false;
			*pCoopFullscreen = true;
			*pNoBorder = true;
		}
		else if ( IsBorderlessWindow() )
		{
			*pFullscreen = false;
			*pCoopFullscreen = false;
			*pNoBorder = true;
		}
		else
		{
			Assert( IsBorderedWindow() );
			*pFullscreen = false;
			*pCoopFullscreen = false;
			*pNoBorder = false;
		}
	}
	void SetSettingsFlags( bool bFullscreen,
						   bool bCoopFullscreen,
						   bool bNoBorder )
	{
		if ( bFullscreen )
		{
			SetExclusiveFullscreen();
		}
		else if ( bCoopFullscreen )
		{
			SetCooperativeFullscreen();
		}
		else if ( bNoBorder )
		{
			SetBorderlessWindow();
		}
		else
		{
			SetBorderedWindow();
		}
	}

	// A number of places need to temporarily suppress exclusive fullscreen
	// so make that simple.
	uint32 DemoteExclusiveFullscreen()
	{
		uint32 nOldUsage = m_nModeUsage & RENDER_DISPLAY_MODE_ALL_USAGE;
		if ( IsExclusiveFullscreen() )
		{
			SetCooperativeFullscreen();
			return nOldUsage;
		}
		// Indicate that we didn't do anything.
		return 0;
	}
	void RestoreExclusiveFullscreen( uint32 nOldUsage )
	{
		if ( nOldUsage )
		{
			Assert( ( nOldUsage & ~RENDER_DISPLAY_MODE_ALL_USAGE ) == 0 );
			m_nModeUsage = (uint8)( ( m_nModeUsage & ~RENDER_DISPLAY_MODE_ALL_USAGE ) | nOldUsage );
		}
	}

	// Translate mode flags to platwindow flags.
	int GetWindowFlags() const
	{
		int nFlags = WINDOW_CREATE_SHOW_TASKBAR_ICON;

		if ( IsExclusiveFullscreen() )
		{
			nFlags |= WINDOW_CREATE_FULLSCREEN;
		}
		else if ( IsCooperativeFullscreen() )
		{
			nFlags |= WINDOW_CREATE_COOP_FULLSCREEN;
		}
		else if ( HasWindowBorder() )
		{
			nFlags |= WINDOW_CREATE_SHOW_FRAME;
		}

		return nFlags;
	}
};

typedef void( *RenderModeChangeCallbackFunc_t )( void );

class IRenderDeviceEventListener
{
public:
	virtual void OnDeviceLost() = 0;
	virtual void OnDeviceCreated() = 0;
	virtual void OnDeviceRestored() = 0;
	virtual void OnModeChanged( const RenderDeviceInfo_t &mode ) = 0;
};

enum RenderSlotType_t
{
	RENDER_SLOT_INVALID = -1,
	RENDER_SLOT_PER_VERTEX = 0,
	RENDER_SLOT_PER_INSTANCE = 1,
};
	
#define RENDER_INPUT_LAYOUT_FIELD_SEMANTIC_NAME_SIZE 32

struct RenderInputLayoutField_t
{
// 	TYPEMETA( MNoScatter );
// 	DECLARE_SCHEMA_DATA_CLASS( RenderInputLayoutField_t )

	uint8 m_pSemanticName[ RENDER_INPUT_LAYOUT_FIELD_SEMANTIC_NAME_SIZE ];
	int32 m_nSemanticIndex;

	// COLOR_FORMAT_*** value up to 88, see enum ColorFormat_t in source2\src\public\bitmap\colorformat.h.
	// See ColorFormatToDeclType() in inputlayoutdx9.cpp for how this gets converted into DECLTYPE
	// Not all COLOR_FORMAT_'s are supported
	uint32 m_Format;

	// offset of the field in bytes, computed using g_BytesForColorFormat[m_Format]
	int32 m_nOffset;
	int32 m_nSlot;
	RenderSlotType_t m_nSlotType;
	int32 m_nInstanceStepRate;
};

//-----------------------------------------------------------------------------
// Methods related to discovering and selecting devices
//-----------------------------------------------------------------------------
abstract_class IRenderDeviceMgr
{
public:
	virtual void AddDeviceEventListener( IRenderDeviceEventListener *pObject ) = 0;
	virtual void RemoveDeviceEventListener( IRenderDeviceEventListener *pObject ) = 0;
};

#define g_pRenderDeviceMgr g_pRenderDeviceMgr2
extern IRenderDeviceMgr* g_pRenderDeviceMgr;

//-----------------------------------------------------------------------------
// Vertex field description
//-----------------------------------------------------------------------------

enum ColorFormat_t
{
	COLOR_FORMAT_UNKNOWN = 0,
	COLOR_FORMAT_R32G32B32A32_TYPELESS = 1,
	COLOR_FORMAT_R32G32B32A32_FLOAT = 2,
	COLOR_FORMAT_R32G32B32A32_UINT = 3,
	COLOR_FORMAT_R32G32B32A32_SINT = 4,
	COLOR_FORMAT_R32G32B32_TYPELESS = 5,
	COLOR_FORMAT_R32G32B32_FLOAT = 6,
	COLOR_FORMAT_R32G32B32_UINT = 7,
	COLOR_FORMAT_R32G32B32_SINT = 8,
	COLOR_FORMAT_R16G16B16A16_TYPELESS = 9,
	COLOR_FORMAT_R16G16B16A16_FLOAT = 10,
	COLOR_FORMAT_R16G16B16A16_UNORM = 11,
	COLOR_FORMAT_R16G16B16A16_UINT = 12,
	COLOR_FORMAT_R16G16B16A16_SNORM = 13,
	COLOR_FORMAT_R16G16B16A16_SINT = 14,
	COLOR_FORMAT_R32G32_TYPELESS = 15,
	COLOR_FORMAT_R32G32_FLOAT = 16,
	COLOR_FORMAT_R32G32_UINT = 17,
	COLOR_FORMAT_R32G32_SINT = 18,
	COLOR_FORMAT_R32G8X24_TYPELESS = 19,
	COLOR_FORMAT_D32_FLOAT_S8X24_UINT = 20,
	COLOR_FORMAT_R32_FLOAT_X8X24_TYPELESS = 21,
	COLOR_FORMAT_X32_TYPELESS_G8X24_UINT = 22,
	COLOR_FORMAT_R10G10B10A2_TYPELESS = 23,
	COLOR_FORMAT_R10G10B10A2_UNORM = 24,
	COLOR_FORMAT_R10G10B10A2_UINT = 25,
	COLOR_FORMAT_R11G11B10_FLOAT = 26,
	COLOR_FORMAT_R8G8B8A8_TYPELESS = 27,
	COLOR_FORMAT_R8G8B8A8_UNORM = 28,
	COLOR_FORMAT_R8G8B8A8_UNORM_SRGB = 29,
	COLOR_FORMAT_R8G8B8A8_UINT = 30,
	COLOR_FORMAT_R8G8B8A8_SNORM = 31,
	COLOR_FORMAT_R8G8B8A8_SINT = 32,
	COLOR_FORMAT_R16G16_TYPELESS = 33,
	COLOR_FORMAT_R16G16_FLOAT = 34,
	COLOR_FORMAT_R16G16_UNORM = 35,
	COLOR_FORMAT_R16G16_UINT = 36,
	COLOR_FORMAT_R16G16_SNORM = 37,
	COLOR_FORMAT_R16G16_SINT = 38,
	COLOR_FORMAT_R32_TYPELESS = 39,
	COLOR_FORMAT_D32_FLOAT = 40,
	COLOR_FORMAT_R32_FLOAT = 41,
	COLOR_FORMAT_R32_UINT = 42,
	COLOR_FORMAT_R32_SINT = 43,
	COLOR_FORMAT_R24G8_TYPELESS = 44,
	COLOR_FORMAT_D24_UNORM_S8_UINT = 45,
	COLOR_FORMAT_R24_UNORM_X8_TYPELESS = 46,
	COLOR_FORMAT_X24_TYPELESS_G8_UINT = 47,
	COLOR_FORMAT_R8G8_TYPELESS = 48,
	COLOR_FORMAT_R8G8_UNORM = 49,
	COLOR_FORMAT_R8G8_UINT = 50,
	COLOR_FORMAT_R8G8_SNORM = 51,
	COLOR_FORMAT_R8G8_SINT = 52,
	COLOR_FORMAT_R16_TYPELESS = 53,
	COLOR_FORMAT_R16_FLOAT = 54,
	COLOR_FORMAT_D16_UNORM = 55,
	COLOR_FORMAT_R16_UNORM = 56,
	COLOR_FORMAT_R16_UINT = 57,
	COLOR_FORMAT_R16_SNORM = 58,
	COLOR_FORMAT_R16_SINT = 59,
	COLOR_FORMAT_R8_TYPELESS = 60,
	COLOR_FORMAT_R8_UNORM = 61,
	COLOR_FORMAT_R8_UINT = 62,
	COLOR_FORMAT_R8_SNORM = 63,
	COLOR_FORMAT_R8_SINT = 64,
	COLOR_FORMAT_A8_UNORM = 65,
	COLOR_FORMAT_R1_UNORM = 66,
	COLOR_FORMAT_R9G9B9E5_SHAREDEXP = 67,
	COLOR_FORMAT_R8G8_B8G8_UNORM = 68,
	COLOR_FORMAT_G8R8_G8B8_UNORM = 69,
	COLOR_FORMAT_BC1_TYPELESS = 70,
	COLOR_FORMAT_BC1_UNORM = 71,
	COLOR_FORMAT_BC1_UNORM_SRGB = 72,
	COLOR_FORMAT_BC2_TYPELESS = 73,
	COLOR_FORMAT_BC2_UNORM = 74,
	COLOR_FORMAT_BC2_UNORM_SRGB = 75,
	COLOR_FORMAT_BC3_TYPELESS = 76,
	COLOR_FORMAT_BC3_UNORM = 77,
	COLOR_FORMAT_BC3_UNORM_SRGB = 78,
	COLOR_FORMAT_BC4_TYPELESS = 79,
	COLOR_FORMAT_BC4_UNORM = 80,
	COLOR_FORMAT_BC4_SNORM = 81,
	COLOR_FORMAT_BC5_TYPELESS = 82,
	COLOR_FORMAT_BC5_UNORM = 83,
	COLOR_FORMAT_BC5_SNORM = 84,
	COLOR_FORMAT_B5G6R5_UNORM = 85,
	COLOR_FORMAT_B5G5R5A1_UNORM = 86,
	COLOR_FORMAT_B8G8R8A8_UNORM = 87,
	COLOR_FORMAT_B8G8R8X8_UNORM = 88,
	COLOR_FORMAT_FORCE_UINT = 0xffffffffUL,
};

template< int nIndexCount >
struct VertexBlendIndices_t
{
	uint8 m_pBlendIndices[ nIndexCount ];
};

template < class T > inline ColorFormat_t ComputeColorFormat( T* )
{
	return COLOR_FORMAT_UNKNOWN;
}

template <> inline ColorFormat_t ComputeColorFormat( float32* )
{
	return COLOR_FORMAT_R32_FLOAT;
}

template <> inline ColorFormat_t ComputeColorFormat( Vector2D* )
{
	return COLOR_FORMAT_R32G32_FLOAT;
}

template <> inline ColorFormat_t ComputeColorFormat( Vector* )
{
	return COLOR_FORMAT_R32G32B32_FLOAT;
}

template <> inline ColorFormat_t ComputeColorFormat( Vector4D* )
{
	return COLOR_FORMAT_R32G32B32A32_FLOAT;
}

template <> inline ColorFormat_t ComputeColorFormat( VertexColor_t* )
{
	// FIXME: How to get at SRGB formats?
	return COLOR_FORMAT_R8G8B8A8_UNORM;
}

template <> inline ColorFormat_t ComputeColorFormat( uint16* )
{
	// FIXME: How to get at SRGB formats?
	return COLOR_FORMAT_R16_UINT;
}

template <> inline ColorFormat_t ComputeColorFormat( VertexBlendIndices_t< 1 >* )
{
	return COLOR_FORMAT_R8_UINT;
}

template <> inline ColorFormat_t ComputeColorFormat( VertexBlendIndices_t< 2 >* )
{
	return COLOR_FORMAT_R8G8_UINT;
}

template <> inline ColorFormat_t ComputeColorFormat( VertexBlendIndices_t< 3 >* )
{
	return COLOR_FORMAT_UNKNOWN;
}

template <> inline ColorFormat_t ComputeColorFormat( VertexBlendIndices_t< 4 >* )
{
	return COLOR_FORMAT_R8G8B8A8_UINT;
}

#define DEFINE_PER_VERTEX_FIELD( _slot, _name, _index, _vertexformat, _field )	\
	{ _name, _index, ComputeColorFormat( &(((_vertexformat*)0)->_field) ), (int)offsetof( _vertexformat, _field ), _slot, RENDER_SLOT_PER_VERTEX, 0 },

#define DEFINE_PER_VERTEX_FIELD_SHADER_FRIENDLY( _slot, _name, _vertexformat, _field )	\
{ _name, SHADER_FRIENDLY_SEMANTIC_INDEX, ComputeColorFormat( &(((_vertexformat*)0)->_field) ), (int)offsetof( _vertexformat, _field ), _slot, RENDER_SLOT_PER_VERTEX, 0 },

#define DEFINE_PER_INSTANCE_FIELD( _slot, _stepRate, _name, _index, _vertexformat, _field )	\
	{ _name, _index, ComputeColorFormat( &(((_vertexformat*)0)->_field) ), (int)offsetof( _vertexformat, _field ), _slot, RENDER_SLOT_PER_INSTANCE, _stepRate }, 


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------

enum TextureCreationExtraDataType_t
{
	TEXTURE_CREATION_DATA_INVALID,

	TEXTURE_CREATION_DATA_VTEX_FALLBACK_BITS,
	TEXTURE_CREATION_DATA_VTEX_SHEET,
	TEXTURE_CREATION_DATA_VTEX_METADATA,
};

DEFINE_ENUM_BITWISE_OPERATORS( TextureCreationExtraDataType_t );

enum TextureOnDiskCompressionType_t
{
	TEXTURE_COMPRESSION_NONE = 0,
	TEXTURE_COMPRESSION_JPEG = 1,
	TEXTURE_COMPRESSION_PNG = 2
};

enum TextureDecodingFlags_t
{
	TEXTURE_DECODE_FLAGS					= 0x0000,
	TEXTURE_DECODE_COLOR_DILATION			= 0x0001,
	TEXTURE_DECODE_CONVERT_TO_YCOCG_DXT5	= 0x0002,
};
DEFINE_ENUM_BITWISE_OPERATORS( TextureDecodingFlags_t );


schema enum TextureSpecificationFlags_t
{
	TSPEC_FLAGS = 0x0000,
	TSPEC_RENDER_TARGET = 0x0001,
	TSPEC_VERTEX_TEXTURE = 0x0002,
	TSPEC_UNFILTERABLE_OK = 0x0004,
	TSPEC_RENDER_TARGET_SAMPLEABLE = 0x0008,
	TSPEC_SUGGEST_CLAMPS = 0x0010,
	TSPEC_SUGGEST_CLAMPT = 0x0020,
	TSPEC_SUGGEST_CLAMPU = 0x0040,
	TSPEC_NO_LOD = 0x0080,	// Don't downsample on lower-level cards
	TSPEC_CUBE_TEXTURE = 0x0100,
	TSPEC_VOLUME_TEXTURE = 0x0200,
	TSPEC_TEXTURE_ARRAY = 0x0400,
	TSPEC_TEXTURE_GEN_MIP_MAPS = 0x0800,	// Must be used with TSPEC_RENDER_TARGET for it to have any effect on D3D11
	TSPEC_LINE_TEXTURE_360 = 0x1000,	// 360 specific: height must be 1, nummips must be 1, and linear addressing must be enabled. This creates a line texture resource, tfetch1D must be used to fetch in shader.
	TSPEC_LINEAR_ADDRESSING_360 = 0x2000,	// 360 specific: Create a linear texture
	TSPEC_USE_TYPED_IMAGEFORMAT = 0x4000,	// Create using a typed image format. This is for scaleform.
	TSPEC_SHARED_RESOURCE = 0x8000,	// For sharing a texture handle between different d3d devices (only works for 2D non-mipped textures)
	TSPEC_RENDER_TARGET_WITHDS = 0x10000,	// S1 specific, depth-stencil to be created with color target
};
//DECLARE_SCHEMA_ENUM( TextureSpecificationFlags_t );
DEFINE_ENUM_BITWISE_OPERATORS( TextureSpecificationFlags_t );

class CTextureDesc
{
public:
	CTextureDesc()
	{
		memset( this, 0, sizeof( *this ) );
	}

	~CTextureDesc()
	{
	}

	CTextureDesc( const CTextureDesc &rhs )
	{
		*this = rhs;
	}

	CTextureDesc &operator=( const CTextureDesc &rhs )
	{
		m_nWidth = rhs.m_nWidth;
		m_nHeight = rhs.m_nHeight;
		m_nDepth = rhs.m_nDepth;
		m_nImageFormat = rhs.m_nImageFormat;
		m_nNumMipLevels = rhs.m_nNumMipLevels;
		m_nFlags = rhs.m_nFlags;
		m_nPicmip0Res = rhs.m_nPicmip0Res;
		m_nDecodeFlags = rhs.m_nDecodeFlags;
		m_nDisplayRectWidth = rhs.m_nDisplayRectWidth;
		m_nDisplayRectHeight = rhs.m_nDisplayRectHeight;

		return *this;
	}

	FORCEINLINE int GetArrayCount() const
	{
		// 'Array count' is really the number of 2D 'slices' - it's m_nDepth for 2D arrays and 6*m_nDepth for cubemap arrays.
		
		// We don't support volume texture arrays.
		Assert( ( ( m_nFlags & TSPEC_VOLUME_TEXTURE ) == 0 ) || ( ( m_nFlags & TSPEC_TEXTURE_ARRAY ) == 0 ) );
		
		// If you're not a volume texture or array, you should have depth of 1
		Assert( ( ( m_nFlags & (TSPEC_VOLUME_TEXTURE|TSPEC_TEXTURE_ARRAY) ) != 0 ) || ( m_nDepth == 1 ) );
		
		int nCount = ( m_nFlags & TSPEC_TEXTURE_ARRAY ) ? m_nDepth : 1;
		return ( m_nFlags & TSPEC_CUBE_TEXTURE ) ? nCount * 6 : nCount;
	}

	FORCEINLINE int GetDepth() const
	{
		return ( m_nFlags & TSPEC_VOLUME_TEXTURE ) ? m_nDepth : 1;
	}

	int16				m_nWidth;
	int16				m_nHeight;
	int16				m_nDepth;		// Doubles as slice count if TSPEC_TEXTURE_ARRAY is specified. Cannot do arrays of volume textures
	int16				m_nNumMipLevels;
	int16				m_nPicmip0Res;
	TextureDecodingFlags_t m_nDecodeFlags;
	ImageFormat			m_nImageFormat;
	TextureSpecificationFlags_t m_nFlags;

	int16				m_nDisplayRectWidth;	// Width of the sub-rect of the texture that should actually be displayed
	int16				m_nDisplayRectHeight;	// Height of the sub-rect of the texture that should actually be displayed
};


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
struct TextureCreationExtraData_t
{
	int m_nDataSize;
	const void *m_pData; // non-owning pointer to data, copy if you need it
	TextureCreationExtraDataType_t m_Type;
};


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
class CTextureCreationDesc: public CTextureDesc
{
	typedef CTextureDesc BaseClass;
public:
	CTextureCreationDesc()
	{
		memset( this, 0, sizeof( *this ) );
		AssertDbg( m_nUsage == TEXTURE_USAGE_DEFAULT );
		AssertDbg( m_nScope == TEXTURE_SCOPE_DEFAULT );
	}

	CTextureCreationDesc( const CTextureCreationDesc &rhs )
	{
		*this = rhs;
	}

	~CTextureCreationDesc()
	{
	}

	CTextureCreationDesc &operator=( const CTextureCreationDesc &rhs )
	{
		BaseClass::operator=( rhs );
		m_Reflectivity = rhs.m_Reflectivity;
		m_nMultisampleType = rhs.m_nMultisampleType;
		m_nUsage = rhs.m_nUsage;
		m_nScope = rhs.m_nScope;
		m_compressionType = rhs.m_compressionType;
		m_ExtraData.CopyArray( rhs.m_ExtraData.Base(), rhs.m_ExtraData.Count() );
		return *this;
	}

	const TextureCreationExtraData_t *FindExtraData( TextureCreationExtraDataType_t nType ) const
	{
		for ( int i = 0; i < m_ExtraData.Count(); ++i )
		{
			if ( m_ExtraData[ i ].m_Type == nType )
			{
				return &( m_ExtraData[i] );
			}
		}

		return NULL;
	}

	Vector4D				m_Reflectivity;
	RenderMultisampleType_t	m_nMultisampleType;
	TextureUsage_t			m_nUsage;
	TextureScope_t			m_nScope;
	TextureOnDiskCompressionType_t m_compressionType;

	CUtlVector< TextureCreationExtraData_t > m_ExtraData;

};


enum RenderSystemAssetFileLoadMode_t // controls behavior when rendersystem assets are created from files
{
	LOADMODE_IMMEDIATE,										// asset is created and loaded from disk immediately
	LOADMODE_ASYNCHRONOUS,									// asset will start loading asynchronously
	LOADMODE_STREAMED,										// asset will be asynchronously loaded when referenced.
};

abstract_class IRenderDevice
{
public:

 	virtual const CTextureDesc *GetTextureDesc( HRenderTexture hTexture ) const = 0; // runtime representation of the current streamed data
 	virtual HRenderTexture FindOrCreateFileTexture( const char *pFileName, RenderSystemAssetFileLoadMode_t nLoadMode = LOADMODE_ASYNCHRONOUS ) = 0;
 	virtual HRenderTexture FindOrCreateTexture( const char *pResourceName, bool bIsAnonymous, const CTextureCreationDesc *pDescriptor, const CTextureDesc *pDataDesc = NULL, const void *pData = NULL, int nDataSize = 0, bool bIsPreTiled = false ) = 0;
 	virtual void AsyncSetTextureData( HRenderTexture hTexture, const CTextureDesc *pDataDesc, const void *pData, int nDataSize, bool bIsPreTiled = false, int nSpecificMipLevelToSet = -1, Rect3D_t const *pSubRectToUpdate = nullptr, uint32 nTextureUpdateFlags = 0, const DataRecycleDelegate_t *pDataRecycleDelegate = nullptr ) = 0;
 	virtual const CTextureDesc *GetOnDiskTextureDesc( HRenderTexture hTexture ) const = 0; // runtime representation of the on-disk data
 	virtual PlatWindow_t GetSwapChainPlatWindow( SwapChainHandle_t hSwapChain ) const = 0;

	virtual void DestroyConstantBuffer( ConstantBufferHandle_t hConstantBuffer ) {};

	virtual HRenderTexture CreateAlphaTexture( int32 iWidth, int32 iHeight ) = 0;
	virtual void DestroyAlphaTexture( HRenderTexture hTexture ) = 0;
	virtual void UpdateAlphaTexture( HRenderTexture hTexture, int32 xOffset, int32 yOffset, int32 iWidth, int32 iHeight, void *pImageData ) = 0;

	virtual HRenderTexture CreateTextureFromEngineRT( const char *pchEngineRTName ) = 0;
	virtual void DestroyTextureFromEngineRT( HRenderTexture hTexture ) = 0;

	virtual void PushPanelRT( HRenderTexture hRT ) = 0;
	virtual void PopPanelRT() = 0;

	virtual void ExcludeTextureFromForceIntoHardware( HRenderTexture hTexture, bool bExclude ) = 0;
};

#define g_pRenderDevice g_pRenderDevice2			// Don't collide with S1 iface
extern IRenderDevice* g_pRenderDevice;

class ISceneView
{
public:

		virtual SwapChainHandle_t GetSwapChain() const = 0;
};


FORCEINLINE bool Plat_IsWindowFocused( PlatWindow_t  hWindow ) 
{
#ifdef PLATFORM_POSIX
	return true;
#else
	return ((HWND)hWindow == ::GetFocus());
#endif
}
#ifdef PLATFORM_WINDOWS
FORCEINLINE HWND Plat_WindowToOsSpecificHandle( PlatWindow_t hWindow )
{
	return (HWND)hWindow;
}
#endif



#endif // INCLUDED_IRENDERDEVICE_H