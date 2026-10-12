//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx.h"
#include "sdlopenglsurface.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include "mathlib/vmatrix.h"
#include "tier1/fileio.h"
#include "../public/misc.h"
#include "tier0/vprof.h"
#include "fmtstr.h"
#include "panorama/panoramatypes.h"
#include "panorama/layout/csshelpers.h"
#include "color.h"
#include "tier1/checksum_crc.h"
#include "renderer/uirenderengine.h"
#include "../../overlay/common/rendermessages.h"
#include "openvr.h"
#include <vrapi.h>
#include "jpegloader.h"
#include "text/uitexttypes.h"
#define glGetCurrentContext SDL_GL_GetCurrentContext

#ifdef LINUX
#include <GL/glx.h> // for glXCreateContext,glXDestroyContext,glXMakeCurrent,glXGetCurrentContext
#elif defined(OSX)

typedef void (*PFNGLBLENDEQUATIONSEPARATEPROC) (GLenum modeRGB, GLenum modeAlpha);
#elif defined(WIN32)
#define __func__ __FUNCTION__
#include "win32openglfuncs.h"

#else
#error "GL defines please"
#endif

using namespace panorama::SDLOGLSurfaceNameSpace;

#ifdef _DEBUG
#define GL_CHECK_CURRENT_CONTEXT if ( glGetCurrentContext() == NULL ) Msg( "THREAD %u %s:%i: context bad %p\n", (uint32)ThreadGetCurrentId(), __FILE__, __LINE__, glGetCurrentContext() );

#define CHECK_GL_ERRORS() _CHECK_GL_ERRORS( __FILE__, __LINE__ )
// uncomment this if you really want full GL checking, in debug builds this can kill perf
//#define ACTUALLY_CHECK_GL_ERRORS
static void _CHECK_GL_ERRORS( char *file, int line )
{ 
#ifdef ACTUALLY_CHECK_GL_ERRORS
	int e = glGetError();
	if ( e ) 
	{	
		Msg( "%s:%i: glGetError() = %i\n", file, line, e );
	} 
#endif
}

#else
#define GL_CHECK_CURRENT_CONTEXT
static void CHECK_GL_ERRORS() {}
#endif

#ifdef WIN32
extern ConVar s_convarPanoramaVsync;
#else
ConVar s_convarPanoramaVsync( "@panorama_vsync", "1" );
#endif
ConVar s_convarPanoramaMaxFreeFBO( "@panorama_max_free_fbo", "200");
ConVar s_convarPanoramaMinFreeFBO( "@panorama_mix_free_fbo", "8");
ConVar s_convarPanoramaFBOAllocBatch( "@panorama_fbo_alloc_batch", "8");

CCommandLineParam g_DumpUsedShaders( "-dumpusedshaders", "Compile and link all shaders lazily, and print used ones on exit (in format for glfancyquadshaders.cfg)" );

const uint64 k_cbPBOTextureBuffer = 4*k_nMillion; // size for our PBO upload buffers for textures


// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

// forestw: the master enable for sRGB color correct rendering
static const bool s_bSRGB = false;

//#define GLCLEARTEST
//#define DEBUGDUMPTEXTURES
//#define DEBUGDUMPFRAMEBUFFERS
//#define DEBUGDUMPSTATE
//#define DEBUGNODRAW

using namespace panorama;

// forestw: variables for dumping textures, framebuffers and state
int s_nDebugDumpTexture_FrameNumber = 0;
int s_nDebugDumpTexture_Number = 0;

#define GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB 0x8242

// 2 because 0 is treated as invalid and 1 is reserved for the magic white texture
CInterlockedInt COpenGLSurface::s_unNextOverlayTextureID = 2;

const char *FragSource_FancyQuadTextureVersion = "#version 120\n";

const char *FragSource_FancyQuadTextureType[FancyQuadTextureType_Total] =
{
	"#define TEXTURETYPE_NONE\n",
	"#define TEXTURETYPE_RGBA\n",
	"#define TEXTURETYPE_PREMUL\n",
	"#define TEXTURETYPE_ALPHA\n",
	"#define TEXTURETYPE_YUV\n",
};
const char *FragSource_FancyQuadFlag[FancyQuadFlag_Count] =
{
	"#define USERADIALGRADIENT\n",
	"#define USEOUTERCORNER\n",
	"#define USEINNERCORNER\n",
	"#define USESATURATION\n",
	"#define USEOPACITYMASK\n",
	"#define GRADIENT_TWOSTOP\n",
	"#define GRADIENT_COMPLEX\n",
};

struct panorama::FancyQuadVertex_t
{
	float m_flPosition[4]; // XYZW
	float m_flTexCoordGradientCoord[4]; // Texture0 coords + gradient coords
	float m_flColor[2][4]; // start and end gradient colors
	float m_flOuterCornerCoord[4][2]; // coords for rounded corners
	float m_flInnerCornerCoord[4][2]; // coords for rounded corners
	float m_flOpacityTexCoord[4]; // OpacityMask coords
};

struct panorama::FancyQuadBrush_t
{
	float m_flColor[FANCYQUAD_MAXSTOPS][4]; // gradient colors (start, end)
	float m_flGradientStops[FANCYQUAD_MAXSTOPS]; // gradient colors (start, end)
	float m_flGradientStartPoint[2]; // gradient start point (linear, radial)
	float m_flGradientEndPoint[2]; // gradient end point (linear only)
	float m_flGradientRadii[2]; // gradient radius (radial only)
	int m_nGradientStops; // how many stops are used by this gradient (usually 2 if gradient is in use)
	bool m_bIsLinearGradient;
	bool m_bIsRadialGradient;
	bool m_bAlphaOnlyTexture;
};

struct panorama::FancyQuadParameters_t
{
	float m_flVertexMin[2], m_flVertexMax[2]; // position of top left and bottom right corner
	float m_flTexCoordMin[2], m_flTexCoordMax[2]; // texcoord of top left and bottom right corner
	float m_flOpacityTexCoordMin[2], m_flOpacityTexCoordMax[2]; // OpacityMask texcoord of top left and bottom right corner
	float m_flInnerCornerRadii[4][2]; // each corner has its own rounding in both directions (topleft topright bottomright bottomleft)
	float m_flOuterCornerRadii[4][2]; // each corner has its own rounding in both directions (topleft topright bottomright bottomleft)
	float m_flBorderWidth[4]; // each edge has a configurable thickness (top right bottom left)
	float m_flBorderColor[4]; // the quad may have a border color if it uses m_flInnerCornerRadii
	float m_flZ; // some quads are at different depth
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
static float s_flMatrixIdentity[16] = { 1.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 1.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 1.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 1.0f };


CDynamicFunctionOpenGL< PFNGLBLENDEQUATIONSEPARATEPROC > g_glBlendEquationSeparate( "glBlendEquationSeparate" );

CInterlockedInt COpenGLSurface::s_unNextTextureID = 1;

//-----------------------------------------------------------------------------
// Purpose: Dump OpenGL texture to disk for debugging purposes
//-----------------------------------------------------------------------------
static void DebugDumpTexture( int texnum )
{
#ifdef DEBUGDUMPTEXTURES
	GLint oldtexnum = 0;
	GLint width = 0, height = 0;
	glFlush(); // just for good measure
	glGetIntegerv( GL_TEXTURE_BINDING_2D, &oldtexnum );
	SetTexture( GL_TEXTURE0_ARB, texnum );
	glGetTexLevelParameteriv( GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width );
	glGetTexLevelParameteriv( GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height );
	if ( width * height > 0 )
	{
		GLubyte *buffer = ( GLubyte * ) malloc( width * height * 4 + 18 );
		memset( buffer, 0, 18 );
		buffer[2] = 2;          // uncompressed type
		buffer[12] = (width >> 0) & 0xFF;
		buffer[13] = (width >> 8) & 0xFF;
		buffer[14] = (height >> 0) & 0xFF;
		buffer[15] = (height >> 8) & 0xFF;
		buffer[16] = 32;        // pixel size
		buffer[17] = 8; // 8 bits of alpha
		glGetTexImage( GL_TEXTURE_2D, 0, GL_BGRA, GL_UNSIGNED_BYTE, buffer + 18 );
		char name[1024];
		name[1023] = 0;
		snprintf( name, sizeof( name ) - 1, "/tmp/panorama_%03i_%03it.tga", s_nDebugDumpTexture_FrameNumber, s_nDebugDumpTexture_Number );
		FILE *file = fopen( name, "wb" );
		fwrite( buffer, 1, width * height * 4 + 18, file );
		fclose( file );
		free( buffer );
		Msg( "WROTE %s\n", name );
		s_nDebugDumpTexture_Number++;
	}
	// restore texture binding
	SetTexture( GL_TEXTURE0_ARB, oldtexnum );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Dump OpenGL framebuffer to disk for debugging purposes
//-----------------------------------------------------------------------------
static void DebugDumpFramebuffer()
{
#ifdef DEBUGDUMPFRAMEBUFFERS
	GLint width = 0, height = 0;
	GLint viewport[4];
	glGetIntegerv( GL_VIEWPORT, viewport );
	width = viewport[2];
	height = viewport[3];
	if ( width * height > 0 )
	{
		GLubyte *buffer = ( GLubyte * ) malloc( width * height * 4 + 18 );
		memset( buffer, 0, 18 );
		buffer[2] = 2;          // uncompressed type
		buffer[12] = (width >> 0) & 0xFF;
		buffer[13] = (width >> 8) & 0xFF;
		buffer[14] = (height >> 0) & 0xFF;
		buffer[15] = (height >> 8) & 0xFF;
		buffer[16] = 32;        // pixel size
		buffer[17] = 8; // 8 bits of alpha
		glReadPixels( viewport[0], viewport[1], viewport[2], viewport[3], GL_BGRA, GL_UNSIGNED_BYTE, buffer + 18 );
		char name[1024];
		name[1023] = 0;
		snprintf( name, sizeof( name ) - 1, "/tmp/panorama_%03i_%03if.tga", s_nDebugDumpTexture_FrameNumber, s_nDebugDumpTexture_Number );
		FILE *file = fopen( name, "wb" );
		fwrite( buffer, 1, width * height * 4 + 18, file );
		fclose( file );
		free( buffer );
		Msg( "WROTE %s\n", name );
		s_nDebugDumpTexture_Number++;
	}
#endif
}


#ifdef DEBUGDUMPSTATE
struct GLStateDump_t
{
	double dDepthClearValue;
	double dDepthRange[2];
	int nActiveTexture;
	int nAlphaTest;
	int nBlend;
	int nBlendEquationAlpha;
	int nBlendEquationRGB;
	int nBlendSrc;
	int nBlendDst;
	int nBlendSrcAlpha;
	int nBlendSrcRGB;
	int nBlendDstAlpha;
	int nBlendDstRGB;
	int nColorMask;
	int nCullFace;
	int nCullFaceMode;
	int nDepthBits;
	int nDepthFunc;
	int nDepthMask;
	int nDepthTest;
	int nDither;
	int nFBO;
	int nFrontFace;
	int nProgram;
	int nScissor[4];
	int nScissorTest;
	int nStencilTest;
	int nTexture0ID;
	int nTexture0Width;
	int nTexture0Height;
	int nViewport[4];

	void Get()
	{
		// note: we could use glIsEnabled or glGetBooleanv for booleans but for consistency this just uses glGetIntegerv
		memset(this, 0, sizeof(*this));
		glGetDoublev( GL_DEPTH_CLEAR_VALUE, &dDepthClearValue );
		glGetDoublev( GL_DEPTH_RANGE, dDepthRange );
		glGetIntegerv( GL_ACTIVE_TEXTURE, &nActiveTexture );
		glGetIntegerv( GL_ALPHA_TEST, &nAlphaTest );
		glGetIntegerv( GL_BLEND, &nBlend );
		glGetIntegerv( GL_BLEND_SRC, &nBlendSrc );
		glGetIntegerv( GL_BLEND_DST, &nBlendDst );
		glGetIntegerv( GL_BLEND_EQUATION_ALPHA, &nBlendEquationAlpha );
		glGetIntegerv( GL_BLEND_EQUATION_RGB, &nBlendEquationRGB );
		glGetIntegerv( GL_BLEND_SRC_ALPHA, &nBlendSrcAlpha );
		glGetIntegerv( GL_BLEND_SRC_RGB, &nBlendSrcRGB );
		glGetIntegerv( GL_BLEND_DST_ALPHA, &nBlendDstAlpha );
		glGetIntegerv( GL_BLEND_DST_RGB, &nBlendDstRGB );
		glGetIntegerv( GL_COLOR_WRITEMASK, &nColorMask );
		glGetIntegerv( GL_CULL_FACE, &nCullFace );
		glGetIntegerv( GL_CULL_FACE_MODE, &nCullFaceMode );
		glGetIntegerv( GL_CURRENT_PROGRAM, &nProgram );
		glGetIntegerv( GL_DEPTH_BITS, &nDepthBits );
		glGetIntegerv( GL_DEPTH_FUNC, &nDepthFunc );
		glGetIntegerv( GL_DEPTH_TEST, &nDepthTest );
		glGetIntegerv( GL_DEPTH_WRITEMASK, &nDepthMask );
		glGetIntegerv( GL_DITHER, &nDither );
		glGetIntegerv( GL_FRAMEBUFFER_BINDING_EXT, &nFBO );
		glGetIntegerv( GL_FRONT_FACE, &nFrontFace );
		glGetIntegerv( GL_SCISSOR_BOX, nScissor );
		glGetIntegerv( GL_SCISSOR_TEST, &nScissorTest );
		glGetIntegerv( GL_STENCIL_TEST, &nStencilTest );
		glGetIntegerv( GL_TEXTURE_BINDING_2D, &nTexture0ID );
		glGetIntegerv( GL_VIEWPORT, nViewport );
		glGetTexLevelParameteriv( GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &nTexture0Height );
		glGetTexLevelParameteriv( GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &nTexture0Width );
	}

	void PrintChanges( GLStateDump_t &old )
	{
		Msg( "%c ActiveTexture      = %i\n"         , old.nActiveTexture      != nActiveTexture                      ? '*' : ' ', nActiveTexture                                         );
		Msg( "%c AlphaTest          = %i\n"         , old.nAlphaTest          != nAlphaTest                          ? '*' : ' ', nAlphaTest                                             );
		Msg( "%c Blend              = %i\n"         , old.nBlend              != nBlend                              ? '*' : ' ', nBlend                                                 );
		Msg( "%c BlendSrc           = %i\n"         , old.nBlendSrc           != nBlendSrc                           ? '*' : ' ', nBlendSrc                                              );
		Msg( "%c BlendDst           = %i\n"         , old.nBlendDst           != nBlendDst                           ? '*' : ' ', nBlendDst                                              );
		Msg( "%c BlendEquationAlpha = %i\n"         , old.nBlendEquationAlpha != nBlendEquationAlpha                 ? '*' : ' ', nBlendEquationAlpha                                    );
		Msg( "%c BlendEquationRGB   = %i\n"         , old.nBlendEquationRGB   != nBlendEquationRGB                   ? '*' : ' ', nBlendEquationRGB                                      );
		Msg( "%c BlendSrcAlpha      = %i\n"         , old.nBlendSrcAlpha      != nBlendSrcAlpha                      ? '*' : ' ', nBlendSrcAlpha                                         );
		Msg( "%c BlendSrcRGB        = %i\n"         , old.nBlendSrcRGB        != nBlendSrcRGB                        ? '*' : ' ', nBlendSrcRGB                                           );
		Msg( "%c BlendDstAlpha      = %i\n"         , old.nBlendDstAlpha      != nBlendDstAlpha                      ? '*' : ' ', nBlendDstAlpha                                         );
		Msg( "%c BlendDstRGB        = %i\n"         , old.nBlendDstRGB        != nBlendDstRGB                        ? '*' : ' ', nBlendDstRGB                                           );
		Msg( "%c ColorMask          = %i\n"         , old.nColorMask          != nColorMask                          ? '*' : ' ', nColorMask                                             );
		Msg( "%c CullFace           = %i\n"         , old.nCullFace           != nCullFace                           ? '*' : ' ', nCullFace                                              );
		Msg( "%c CullFaceMode       = %i\n"         , old.nCullFaceMode       != nCullFaceMode                       ? '*' : ' ', nCullFaceMode                                          );
		Msg( "%c DepthBits          = %i\n"         , old.nDepthBits          != nDepthBits                          ? '*' : ' ', nDepthBits                                             );
		Msg( "%c DepthClearValue    = %f\n"         , old.dDepthClearValue    != dDepthClearValue                    ? '*' : ' ', dDepthClearValue                                       );
		Msg( "%c DepthFunc          = %i\n"         , old.nDepthFunc          != nDepthFunc                          ? '*' : ' ', nDepthFunc                                             );
		Msg( "%c DepthMask          = %i\n"         , old.nDepthMask          != nDepthMask                          ? '*' : ' ', nDepthMask                                             );
		Msg( "%c DepthRange         = %f %f\n"      , memcmp( old.dDepthRange, dDepthRange, sizeof( dDepthRange ) ) ? '*' : ' ', dDepthRange[0], dDepthRange[1]                         );
		Msg( "%c DepthTest          = %i\n"         , old.nDepthTest          != nDepthTest                          ? '*' : ' ', nDepthTest                                             );
		Msg( "%c Dither             = %i\n"         , old.nDither             != nDither                             ? '*' : ' ', nDither                                                );
		Msg( "%c FBO                = %i\n"         , old.nFBO                != nFBO                                ? '*' : ' ', nFBO                                                   );
		Msg( "%c FrontFace          = %i\n"         , old.nFrontFace          != nFrontFace                          ? '*' : ' ', nFrontFace                                             );
		Msg( "%c Program            = %i\n"         , old.nProgram            != nProgram                            ? '*' : ' ', nProgram                                               );
		Msg( "%c Scissor            = %i %i %i %i\n", memcmp( old.nScissor   , nScissor   , sizeof( nScissor    ) ) ? '*' : ' ', nScissor[0], nScissor[1], nScissor[2], nScissor[3]     );
		Msg( "%c ScissorTest        = %i\n"         , old.nScissorTest        != nScissorTest                        ? '*' : ' ', nScissorTest                                           );
		Msg( "%c StencilTest        = %i\n"         , old.nStencilTest        != nStencilTest                        ? '*' : ' ', nStencilTest                                           );
		Msg( "%c Texture0ID         = %i\n"         , old.nTexture0ID         != nTexture0ID                         ? '*' : ' ', nTexture0ID                                            );
		Msg( "%c Texture0Width      = %i\n"         , old.nTexture0Width      != nTexture0Width                      ? '*' : ' ', nTexture0Width                                         );
		Msg( "%c Texture0Height     = %i\n"         , old.nTexture0Height     != nTexture0Height                     ? '*' : ' ', nTexture0Height                                        );
		Msg( "%c Viewport           = %i %i %i %i\n", memcmp( old.nViewport  , nViewport  , sizeof( nViewport   ) ) ? '*' : ' ', nViewport[0], nViewport[1], nViewport[2], nViewport[3] );
	}
};
#endif


//-----------------------------------------------------------------------------
// Purpose: Dump OpenGL state information for debugging purposes
//-----------------------------------------------------------------------------
// forestw: function to dump GL state every draw or clear for debugging panorama
static void DebugDumpState( const char *funcname )
{
#ifdef DEBUGDUMPSTATE
	static GLStateDump_t old;
	GLStateDump_t current;
	current.Get();
	if ( !old.nActiveTexture )
		old = current; // first time
	Msg(" %s GL state dump:\n", funcname );
	current.PrintChanges(old);
	old = current;
#endif
}


/// Make the specified window and context current
bool COpenGLSurface::MakeCurrent( SDL_Window *window, GLContext hGLContext )
{
	return SDL_GL_MakeCurrent( window, hGLContext ) == 0;
}


//-----------------------------------------------------------------------------
// Purpose: Decode our 0xAABBGGRR color constants to linear color (from sRGB or linear RGB)
//-----------------------------------------------------------------------------
void LinearColorFromABGR( float &r, float &g, float &b, float &a, unsigned int c, bool bSRGB )
{
	// forestw: this is to exact sRGB spec, not a mere pow 2.2
	static float fLinear[256];
	if ( bSRGB && s_bSRGB )
	{
		if ( !fLinear[255] )
		{
			for ( int i = 0; i < 256; i++ )
			{
				float f = i * (1.0f / 255.0f);
				fLinear[i] = ( ( ( f ) <= 0.04045f) ? ( f ) * (1.0f / 12.92f) : ( float )pow( ( ( f ) + 0.055f ) * ( 1.0f / 1.055f ), 2.4f ) );
			}
		}
		r = fLinear[( c       ) & 0xff];
		g = fLinear[( c >>  8 ) & 0xff];
		b = fLinear[( c >> 16 ) & 0xff];
	}
	else
	{
		r = ( ( c       ) & 0xff ) * (1.0f / 255.0f);
		g = ( ( c >>  8 ) & 0xff ) * (1.0f / 255.0f);
		b = ( ( c >> 16 ) & 0xff ) * (1.0f / 255.0f);
	}
	a = ( ( c >> 24 ) & 0xff ) * (1.0f / 255.0f);
}

//-----------------------------------------------------------------------------
// Purpose: Constructor 
//-----------------------------------------------------------------------------
CCompositionLayer::CCompositionLayer( COpenGLSurface *pParentSurface, GLContext hGLContext, float width, float height )
{
	m_ulContextID = 0;
	m_bIsDrawing = false;

	m_pParentSurface = pParentSurface;

	m_hFBO = m_pParentSurface->CreateFBO(width, height, &m_hGLTexture);

	m_pVecClipLayers = NULL; 
	m_flLayerWidth = ceil( width );
	m_flLayerHeight = ceil( height );

	m_flSaturation = 1.0f;
	m_flHueShift = 0.0f;
	m_flBrightness = 1.0f;
	m_flContrast = 1.0f;
	m_unOpacityMaskTextureID = 0;
	m_flBlurPasses = 1.0f;
	m_flBlurStdDevHor = 0.0f;
	m_flBlurStdDevVer = 0.0f;
	m_flOpacityMaskOpacity = 1.0f;
	V_memset( m_RenderQuad, 0, sizeof( m_RenderQuad ) );

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
	VPROF_BUDGET( "CCompositionLayer::~CCompositionLayer()", VPROF_BUDGETGROUP_TENFOOT );

	SAFE_DELETE( m_pVecClipLayers );
	if ( m_hFBO )
	{
		VPROF_BUDGET( "CCompositionLayer::~CCompositionLayer() FBO", VPROF_BUDGETGROUP_TENFOOT );



		m_pParentSurface->FreeFBO( m_hFBO );
		m_hFBO = 0;
	}
	if ( m_hGLTexture )
	{
		VPROF_BUDGET( "CCompositionLayer::~CCompositionLayer() Texture", VPROF_BUDGETGROUP_TENFOOT );

//		Msg( "glDeleteTextures fbo%i\n", m_hGLTexture );
		glDeleteTextures( 1, &m_hGLTexture );
		m_hGLTexture = 0;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Helper for pushing clip layers and beginning draw on d2d target
//-----------------------------------------------------------------------------
void CCompositionLayer::ActivateRenderTarget()
{
//	Msg( "Layer %p -> ActivateRenderTarget() : fbo %i -> fbo %i \n", this, hCurrentFBO, m_hFBO );

	if ( !m_hFBO )
		return;

	if ( m_pParentSurface->GetLastActiveFBO() == (GLint)m_hFBO )
		return;

	VPROF_BUDGET( "CCompositionLayer::ActivateRenderTarget ", VPROF_BUDGETGROUP_TENFOOT );
	m_pParentSurface->SetLastActiveFBO( (GLint)m_hFBO );

	// use this layer's FBO
	glBindFramebufferEXT( GL_FRAMEBUFFER_EXT, m_hFBO );
	// set the viewport to the whole framebuffer
	glViewport( 0, 0, m_flLayerWidth, m_flLayerHeight );
}


//-----------------------------------------------------------------------------
// Purpose: Helper for pushing clip layers and beginning draw on d2d target
//-----------------------------------------------------------------------------
void CCompositionLayer::PushCliplayersAndBeginDraw( float flScaleX, float flScaleY, float flTranslateX, float flTranslateY  )
{
	VPROF_BUDGET( "CCompositionLayer::PushCliplayersAndBeginDraw ", VPROF_BUDGETGROUP_TENFOOT );
	if ( !m_hFBO )
		return;

	Assert( !m_bIsDrawing );
	if ( !m_bIsDrawing )
	{
		GL_CHECK_CURRENT_CONTEXT;

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
void CCompositionLayer::PopClipLayersAndFlush()
{
	if( m_bIsDrawing )
	{
		m_bIsDrawing = false;
		GL_CHECK_CURRENT_CONTEXT;
	}
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
// Purpose: clear the texture for this layer
//-----------------------------------------------------------------------------
void CCompositionLayer::Clear()
{
	GL_CHECK_CURRENT_CONTEXT;
	CHECK_GL_ERRORS();
	ActivateRenderTarget();

#ifdef GLCLEARTEST
	glClearColor( 0.0f, 1.0f, 0.0f, 0.25f );
#else
	glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
#endif

	glClear( GL_COLOR_BUFFER_BIT );

	DebugDumpFramebuffer();
	DebugDumpState( __func__ );

	CHECK_GL_ERRORS();
}

//-----------------------------------------------------------------------------
// Purpose: Draws a layer inset shadow image (unblurred) into this layer
//-----------------------------------------------------------------------------
void CCompositionLayer::DrawInsetShadowIntoLayer( COpenGLSurface *pBaseSurface, float flPadding, float flWidth, float flHeight, float flHorOffset, float flVerOffset, float flSpreadDistance, uint32 shadowColor, float *pflInnerRadii )
{
	float viewWidth = flWidth, viewHeight = flHeight;
	VPROF_BUDGET( "CCompositionLayer::DrawInsetShadowIntoLayer", VPROF_BUDGETGROUP_TENFOOT );

	flWidth -= flPadding * 2;
	flHeight -= flPadding * 2;
	flHorOffset += flPadding;
	flVerOffset += flPadding;

	// forestw: this function renders a rounded corner alpha blended rectangle into the layer
	FancyQuadParameters_t FancyQuad;
	memset( &FancyQuad, 0, sizeof( FancyQuad ) );
	FancyQuad.m_flZ = 0.0f;
	FancyQuad.m_flVertexMin[0] = 0;
	FancyQuad.m_flVertexMin[1] = 0;
	FancyQuad.m_flVertexMax[0] = m_flLayerWidth;
	FancyQuad.m_flVertexMax[1] = m_flLayerHeight;
	FancyQuad.m_flTexCoordMin[0] = 0.0f;
	FancyQuad.m_flTexCoordMin[1] = 0.0f;
	FancyQuad.m_flTexCoordMax[0] = 1.0f;
	FancyQuad.m_flTexCoordMax[1] = 1.0f;
	FancyQuad.m_flOpacityTexCoordMin[0] = 0.0f;
	FancyQuad.m_flOpacityTexCoordMin[1] = 0.0f;
	FancyQuad.m_flOpacityTexCoordMax[0] = 0.0f;
	FancyQuad.m_flOpacityTexCoordMax[1] = 0.0f;
	// A radius of 1.0 is also sharp to FancyQuad, but is the minimum required
	// to properly initialize all the inner radii data structures so that
	// BorderWidth has any effect, since it uses the same codepath in the shader
	FancyQuad.m_flInnerCornerRadii[0][0] = MAX( pflInnerRadii[0], 1.0f);
	FancyQuad.m_flInnerCornerRadii[0][1] = MAX( pflInnerRadii[1], 1.0f);
	FancyQuad.m_flInnerCornerRadii[1][0] = MAX( pflInnerRadii[2], 1.0f);
	FancyQuad.m_flInnerCornerRadii[1][1] = MAX( pflInnerRadii[3], 1.0f);
	FancyQuad.m_flInnerCornerRadii[2][0] = MAX( pflInnerRadii[4], 1.0f);
	FancyQuad.m_flInnerCornerRadii[2][1] = MAX( pflInnerRadii[5], 1.0f);
	FancyQuad.m_flInnerCornerRadii[3][0] = MAX( pflInnerRadii[6], 1.0f);
	FancyQuad.m_flInnerCornerRadii[3][1] = MAX( pflInnerRadii[7], 1.0f);
	FancyQuad.m_flOuterCornerRadii[0][0] = 0.0f;
	FancyQuad.m_flOuterCornerRadii[0][1] = 0.0f;
	FancyQuad.m_flOuterCornerRadii[1][0] = 0.0f;
	FancyQuad.m_flOuterCornerRadii[1][1] = 0.0f;
	FancyQuad.m_flOuterCornerRadii[2][0] = 0.0f;
	FancyQuad.m_flOuterCornerRadii[2][1] = 0.0f;
	FancyQuad.m_flOuterCornerRadii[3][0] = 0.0f;
	FancyQuad.m_flOuterCornerRadii[3][1] = 0.0f;
	FancyQuad.m_flBorderWidth[0] = flVerOffset;
	FancyQuad.m_flBorderWidth[1] = flHorOffset;
	FancyQuad.m_flBorderWidth[2] = m_flLayerHeight - ( flVerOffset + flHeight - flSpreadDistance );
	FancyQuad.m_flBorderWidth[3] = m_flLayerWidth - ( flHorOffset + flWidth - flSpreadDistance );
	float r, g, b, a;
	LinearColorFromABGR( r, g, b, a, shadowColor, true );
	FancyQuad.m_flBorderColor[0] = r * a;
	FancyQuad.m_flBorderColor[1] = g * a;
	FancyQuad.m_flBorderColor[2] = b * a;
	FancyQuad.m_flBorderColor[3] = a;
	FancyQuadBrush_t FancyBrush;
	memset( &FancyBrush, 0, sizeof( FancyBrush ) );
	FancyBrush.m_flColor[0][0] = 0.0f;
	FancyBrush.m_flColor[0][1] = 0.0f;
	FancyBrush.m_flColor[0][2] = 0.0f;
	FancyBrush.m_flColor[0][3] = 0.0f;
	FancyBrush.m_flGradientStartPoint[0] = 0.0f;
	FancyBrush.m_flGradientStartPoint[1] = 0.0f;
	FancyBrush.m_flGradientEndPoint[0] = 0.0f;
	FancyBrush.m_flGradientEndPoint[1] = 0.0f;
	FancyBrush.m_flGradientRadii[0] = 0.0f;
	FancyBrush.m_flGradientRadii[1] = 0.0f;
	FancyBrush.m_bIsLinearGradient = false;
	FancyBrush.m_bIsRadialGradient = false;
	pBaseSurface->DrawFancyQuad( 0, 0, 0, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, viewWidth, viewHeight, FancyQuad, FancyBrush, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f, false, false, false, false, s_flMatrixIdentity );
}


//-----------------------------------------------------------------------------
// Purpose: Draw the border for the layer
//-----------------------------------------------------------------------------
void CCompositionLayer::DrawBorder( COpenGLSurface *pBaseSurface )
{
	int i, j;
	float r, g, b, a;
	FancyQuadVertex_t v[16];
	VPROF_BUDGET( "CCompositionLayer::DrawBorder", VPROF_BUDGETGROUP_TENFOOT );
	// Is there any border at all?
	if ( m_rgBorderWidths[0] == 0.0f && m_rgBorderWidths[1] == 0.0f && m_rgBorderWidths[2] == 0.0f && m_rgBorderWidths[3] == 0.0f )
		return;

	// Early out for transparent colors
	bool bHasColor = false;
	for ( int iCur = 0; iCur < V_ARRAYSIZE( m_rgbaBorderColors ); ++iCur )
	{
		if ( ( ( m_rgbaBorderColors[iCur] >> 24 ) & 0xff ) != 0 )
		{
			bHasColor = true;
			break;
		}
	}

	if ( !bHasColor )
		return;

	bool bHasRounding = false;
	for( i = 0; i < V_ARRAYSIZE( m_rgCornerRadii ); ++i )
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
		CHECK_GL_ERRORS();
		CLazyShaderProgram &program = pBaseSurface->m_shaderprogramFancyQuadUber[FancyQuadTextureType_None][0];
		if ( pBaseSurface->m_LastProgram != program.GetProgram() )
		{
			glUseProgram( program.GetProgram() );
			pBaseSurface->m_LastProgram = program.GetProgram();
		}
		glUniformMatrix4fv( program.m_matTransformLoc, 1, GL_FALSE, ( GLfloat * ) s_flMatrixIdentity );
		glUniform1f( program.m_viewportWidthLoc, m_flLayerWidth );
		glUniform1f( program.m_viewportHeightLoc, m_flLayerHeight );
		CHECK_GL_ERRORS();
	
		// forestw: here we have a challenge:
		// each border line has configurable thickness
		// each border line has configurable color
		// each border line has mitered ends (first and last edges are diagonal in the corners), which can cause artifacts with antialiasing if drawn separately
		// the inner content area may have corner rounding (revealing border color in the removed portion)
		// D3D10D2D code and OSX code differ in how they handle different border colors with inner radius, both look broken/untested
		// FIXME forestw: it is my opinion that the highest quality rendering of all this functionality would be to use shader logic for the mitered corner color handling and inner radius handling, this entire primitive can become a quad or collection of smaller quads (to save fillrate on the middle)
		memset( v, 0, sizeof( v ) );
		v[ 0].m_flPosition[0] = 0.0f;
		v[ 0].m_flPosition[1] = 0.0f;
		v[ 1].m_flPosition[0] = m_rgBorderWidths[3];
		v[ 1].m_flPosition[1] = m_rgBorderWidths[0];
		v[ 2].m_flPosition[0] = m_flLayerWidth - m_rgBorderWidths[1];
		v[ 2].m_flPosition[1] = m_rgBorderWidths[0];
		v[ 3].m_flPosition[0] = m_flLayerWidth;
		v[ 3].m_flPosition[1] = 0.0f;

		v[ 4].m_flPosition[0] = m_flLayerWidth - m_rgBorderWidths[1];
		v[ 4].m_flPosition[1] = m_rgBorderWidths[0];
		v[ 5].m_flPosition[0] = m_flLayerWidth;
		v[ 5].m_flPosition[1] = 0.0f;
		v[ 6].m_flPosition[0] = m_flLayerWidth;
		v[ 6].m_flPosition[1] = m_flLayerHeight;
		v[ 7].m_flPosition[0] = m_flLayerWidth - m_rgBorderWidths[1];
		v[ 7].m_flPosition[1] = m_flLayerHeight - m_rgBorderWidths[2];

		v[ 8].m_flPosition[0] = m_flLayerWidth - m_rgBorderWidths[1];
		v[ 8].m_flPosition[1] = m_flLayerHeight - m_rgBorderWidths[2];
		v[ 9].m_flPosition[0] = m_flLayerWidth;
		v[ 9].m_flPosition[1] = m_flLayerHeight;
		v[10].m_flPosition[0] = 0.0f;
		v[10].m_flPosition[1] = m_flLayerHeight;
		v[11].m_flPosition[0] = m_rgBorderWidths[3];
		v[11].m_flPosition[1] = m_flLayerHeight - m_rgBorderWidths[2];

		v[12].m_flPosition[0] = m_rgBorderWidths[3];
		v[12].m_flPosition[1] = m_flLayerHeight - m_rgBorderWidths[2];
		v[13].m_flPosition[0] = 0.0f;
		v[13].m_flPosition[1] = m_flLayerHeight;
		v[14].m_flPosition[0] = 0.0f;
		v[14].m_flPosition[1] = 0.0f;
		v[15].m_flPosition[0] = m_rgBorderWidths[3];
		v[15].m_flPosition[1] = m_rgBorderWidths[0];
	
		for ( i = 0; i < 16; i++ )
		{
			j = i >> 2;
			LinearColorFromABGR( r, g, b, a, m_rgbaBorderColors[j], true );
			v[i].m_flPosition[2] = 0.0f;
			v[i].m_flPosition[3] = 1.0f;
			v[i].m_flColor[0][0] = v[i].m_flColor[1][0] = r * a;
			v[i].m_flColor[0][1] = v[i].m_flColor[1][1] = g * a;
			v[i].m_flColor[0][2] = v[i].m_flColor[1][2] = b * a;
			v[i].m_flColor[0][3] = v[i].m_flColor[1][3] = a;
		}
	
#ifndef DEBUGNODRAW
		// TODO forestw: shovel this into a mesh queue of some kind
		CHECK_GL_ERRORS();
		glBegin( GL_QUADS );
		for ( i = 0; i < 16; i++ )
		{
			glMultiTexCoord4fv( GL_TEXTURE0_ARB, v[i].m_flTexCoordGradientCoord );
			glMultiTexCoord4fv( GL_TEXTURE1_ARB, v[i].m_flOuterCornerCoord[0] );
			glMultiTexCoord4fv( GL_TEXTURE2_ARB, v[i].m_flOuterCornerCoord[2] );
			glMultiTexCoord4fv( GL_TEXTURE3_ARB, v[i].m_flColor[0] );
			glMultiTexCoord4fv( GL_TEXTURE4_ARB, v[i].m_flColor[1] );
			glMultiTexCoord4fv( GL_TEXTURE5_ARB, v[i].m_flInnerCornerCoord[0] );
			glMultiTexCoord4fv( GL_TEXTURE6_ARB, v[i].m_flInnerCornerCoord[2] );
			glMultiTexCoord4fv( GL_TEXTURE7_ARB, v[i].m_flOpacityTexCoord );
			glVertex4fv( v[i].m_flPosition );
		}
		glEnd();
		CHECK_GL_ERRORS();
#endif
	}
	else
	{
		// forestw: surrounded by a border color, with rounded inner corners
		AssertMsg( m_rgbaBorderColors[0] == m_rgbaBorderColors[1] && m_rgbaBorderColors[0] == m_rgbaBorderColors[2] && m_rgbaBorderColors[0] == m_rgbaBorderColors[3], "DrawBorder with rounded corners must use same color on all sides" );
		FancyQuadParameters_t FancyQuad;
		memset( &FancyQuad, 0, sizeof( FancyQuad ) );
		FancyQuad.m_flZ = 0.0f;
		FancyQuad.m_flVertexMin[0] = 0;
		FancyQuad.m_flVertexMin[1] = 0;
		FancyQuad.m_flVertexMax[0] = m_flLayerWidth;
		FancyQuad.m_flVertexMax[1] = m_flLayerHeight;
		FancyQuad.m_flTexCoordMin[0] = 0.0f;
		FancyQuad.m_flTexCoordMin[1] = 0.0f;
		FancyQuad.m_flTexCoordMax[0] = 1.0f;
		FancyQuad.m_flTexCoordMax[1] = 1.0f;
		FancyQuad.m_flOpacityTexCoordMin[0] = 0.0f;
		FancyQuad.m_flOpacityTexCoordMin[1] = 0.0f;
		FancyQuad.m_flOpacityTexCoordMax[0] = 0.0f;
		FancyQuad.m_flOpacityTexCoordMax[1] = 0.0f;
		FancyQuad.m_flInnerCornerRadii[0][0] = MAX( 1.0f, m_rgCornerRadii[0] - m_rgBorderWidths[3] );
		FancyQuad.m_flInnerCornerRadii[0][1] = MAX( 1.0f, m_rgCornerRadii[1] - m_rgBorderWidths[0] );
		FancyQuad.m_flInnerCornerRadii[1][0] = MAX( 1.0f, m_rgCornerRadii[2] - m_rgBorderWidths[1] );
		FancyQuad.m_flInnerCornerRadii[1][1] = MAX( 1.0f, m_rgCornerRadii[3] - m_rgBorderWidths[0] );
		FancyQuad.m_flInnerCornerRadii[2][0] = MAX( 1.0f, m_rgCornerRadii[4] - m_rgBorderWidths[1] );
		FancyQuad.m_flInnerCornerRadii[2][1] = MAX( 1.0f, m_rgCornerRadii[5] - m_rgBorderWidths[2] );
		FancyQuad.m_flInnerCornerRadii[3][0] = MAX( 1.0f, m_rgCornerRadii[6] - m_rgBorderWidths[3] );
		FancyQuad.m_flInnerCornerRadii[3][1] = MAX( 1.0f, m_rgCornerRadii[7] - m_rgBorderWidths[2] );
		FancyQuad.m_flOuterCornerRadii[0][0] = 0.0f;
		FancyQuad.m_flOuterCornerRadii[0][1] = 0.0f;
		FancyQuad.m_flOuterCornerRadii[1][0] = 0.0f;
		FancyQuad.m_flOuterCornerRadii[1][1] = 0.0f;
		FancyQuad.m_flOuterCornerRadii[2][0] = 0.0f;
		FancyQuad.m_flOuterCornerRadii[2][1] = 0.0f;
		FancyQuad.m_flOuterCornerRadii[3][0] = 0.0f;
		FancyQuad.m_flOuterCornerRadii[3][1] = 0.0f;
		FancyQuad.m_flBorderWidth[0] = m_rgBorderWidths[0];
		FancyQuad.m_flBorderWidth[1] = m_rgBorderWidths[1];
		FancyQuad.m_flBorderWidth[2] = m_rgBorderWidths[2];
		FancyQuad.m_flBorderWidth[3] = m_rgBorderWidths[3];
		LinearColorFromABGR( r, g, b, a, m_rgbaBorderColors[0], true );
		FancyQuad.m_flBorderColor[0] = r * a;
		FancyQuad.m_flBorderColor[1] = g * a;
		FancyQuad.m_flBorderColor[2] = b * a;
		FancyQuad.m_flBorderColor[3] = a;
		// color is transparent black but the FancyQuad.m_flBorderColor will override in the right areas
		FancyQuadBrush_t FancyBrush;
		memset( &FancyBrush, 0, sizeof( FancyBrush ) );
		FancyBrush.m_flColor[0][0] = 0.0f;
		FancyBrush.m_flColor[0][1] = 0.0f;
		FancyBrush.m_flColor[0][2] = 0.0f;
		FancyBrush.m_flColor[0][3] = 0.0f;
		FancyBrush.m_flGradientStartPoint[0] = 0.0f;
		FancyBrush.m_flGradientStartPoint[1] = 0.0f;
		FancyBrush.m_flGradientEndPoint[0] = 0.0f;
		FancyBrush.m_flGradientEndPoint[1] = 0.0f;
		FancyBrush.m_flGradientRadii[0] = 0.0f;
		FancyBrush.m_flGradientRadii[1] = 0.0f;
		FancyBrush.m_bIsLinearGradient = false;
		FancyBrush.m_bIsRadialGradient = false;
		pBaseSurface->DrawFancyQuad( 0, 0, 0, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, m_flLayerWidth, m_flLayerHeight, FancyQuad, FancyBrush, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f, false, false, false, false, s_flMatrixIdentity );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Helper to determine if a clip layer is an axis aligned rect
//-----------------------------------------------------------------------------
bool CCompositionLayer::BHasNoRounding( const CRadiusData &msg )
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
// Purpose: Push a new clip layer into the composition layer
//-----------------------------------------------------------------------------
void CCompositionLayer::PushClipLayer( const CMsgPushClipLayer &msg )
{
	if ( !m_pVecClipLayers )
	{
		m_pVecClipLayers = new CUtlVector < CMsgPushClipLayer >;
	}
	m_pVecClipLayers->AddToTail( msg );
}


//-----------------------------------------------------------------------------
// Purpose:Pop a clip layer out of the composition layer
//-----------------------------------------------------------------------------
void CCompositionLayer::PopClipLayer()
{
	if ( m_pVecClipLayers && m_pVecClipLayers->Count() > 0 )
	{
		m_pVecClipLayers->Remove( m_pVecClipLayers->Count() - 1 );
	}
	else
	{
		AssertMsg( false, "Called CCompositionLayer::PopClipLayer with no clip layers pushed" );
	}
}


typedef const CMsgLinearGradient * CLinearGradientMapKey;
static bool CLinearGradientLessThan( const CLinearGradientMapKey &lhs, const CLinearGradientMapKey &rhs )
{
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
	for( int i = 0; i < lhs->color_stop_size(); ++i )
	{
		if ( lhs->color_stop( i ).color_rgba() < rhs->color_stop( i ).color_rgba() )
			return true;
		else if( lhs->color_stop( i ).color_rgba() > rhs->color_stop( i ).color_rgba() )
			return false;

		if ( lhs->color_stop( i ).color_rgba() < rhs->color_stop( i ).color_rgba() )
			return true;
		else if( lhs->color_stop( i ).color_rgba() > rhs->color_stop( i ).color_rgba() )
			return false;
	}

	return false;
}


typedef const CMsgRadialGradient * CRadialGradientMapKey;
static bool CRadialGradientLessThan( const CRadialGradientMapKey &lhs, const CRadialGradientMapKey &rhs )
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
	for( int i = 0; i < lhs->color_stop_size(); ++i )
	{
		if ( lhs->color_stop( i ).color_rgba() < rhs->color_stop( i ).color_rgba() )
			return true;
		else if( lhs->color_stop( i ).color_rgba() > rhs->color_stop( i ).color_rgba() )
			return false;

		if ( lhs->color_stop( i ).color_rgba() < rhs->color_stop( i ).color_rgba() )
			return true;
		else if( lhs->color_stop( i ).color_rgba() > rhs->color_stop( i ).color_rgba() )
			return false;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
COpenGLSurface::COpenGLSurface() : m_bResumed(false), m_bVTSwitched(false)
#ifdef LINUX
								  ,ResumeNotify("org.freedesktop.UPower", "Resuming", NULL, ResumingCallback, (void *)this)
								  ,VTNotify("org.freedesktop.ConsoleKit.Seat", "ActiveSessionChanged", NULL, VTNotifyCallback, (void *)this)
								  ,LogindVTNotify("org.freedesktop.DBus.Properties", "PropertiesChanged", "/org/freedesktop/login1/seat/seat0", LogindVTNotifyCallback, (void *)this)
#endif
{
	V_memset( m_rpDistortionMap, 0x0, sizeof(m_rpDistortionMap) );
	V_memset( m_LastTextureID, 0x0, sizeof( m_LastTextureID ) );

	m_flCurrentRenderFrameTime = 0.0f;
	m_LastTextTextureDumpTime = 0.0f;
	m_LastFBOActive = 0.0;
	m_LastProgram = 0;
	
	m_nFreeGLTextureIndex = -1;
	m_hGLContext = 0;
	m_hSDLWindow = 0;
	m_iGradientTextureName = 0;
	
	m_bVsyncEnabled = s_convarPanoramaVsync.GetBool();
	m_unSurfaceWidth = 0;
	m_unSurfaceHeight = 0;
	m_pUITextureOpaqueMask = NULL;
	m_ERenderState = k_ERenderStateUnset;
	m_eRenderTarget = IUIEngine::k_ERenderTargetUnset;
	m_flScaleBackbufferX = 1.0f;
	m_flTranslateBackbufferX = 0.0f;
	m_flScaleBackbufferY = 1.0f;
	m_flTranslateBackbufferY = 0.0f;
	m_nOverlayTextureID = 0;
	m_dwTargetOverlayPID = 0;
	m_pBackBufferSharedMemStream = NULL;
	m_pBackBufferSharedMemEvent = NULL;
	m_pBackBufferSharedMemWriteEvent = NULL;
	m_unBackBufferSharedMenEventFails = 0;
	m_unTotalFBOs = 0;
#if defined(LINUX)
	m_iCurrentWMOpactity = 0xFFFFFFFF; // set it to an invalid value
#endif

	m_nLastFrameMillisecondsIndex = -1;
	for( int i = 0; i < V_ARRAYSIZE( m_rgflMillisecondsFrame ); ++i )
	{
		m_rgflMillisecondsFrame[i] = FLT_MAX;
	}

	m_pTextTextureCache = NULL;
    m_pTextLayoutDrawCache = NULL;

	//m_mapLinearGradientBrushes.SetLessFunc( CLinearGradientLessThan );
	//m_mapRadialGradientBrushes.SetLessFunc( CRadialGradientLessThan );

	// forestw: clear the shader program info
	DestroyDeviceResources( false );
	
	V_memset( &m_leftSteamPadPointer, 0, sizeof( m_leftSteamPadPointer ) );
	V_memset( &m_rightSteamPadPointer, 0, sizeof( m_rightSteamPadPointer ) );
	
#if defined(LINUX)
	m_xDpy = None;
	m_xWindow = None;
#endif

#ifdef PANORAMA_PUBLIC_STEAM_SDK
	// Need to make sure VR APIs available for trackpad controllers
	vrapi::EnsureOpenVRAPILoaded();
#endif // PANORAMA_PUBLIC_STEAM_SDK

}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
COpenGLSurface::~COpenGLSurface()
{
	if ( m_hSDLWindow )
	{
		MakeCurrent( m_hSDLWindow, 0 );
		if ( m_hGLContext )
		{
			bool bSuccess = MakeCurrent( m_hSDLWindow, m_hGLContext );
			if ( !bSuccess && IsWindows() )
			{
				ThreadSleep( 10 );
				bSuccess = MakeCurrent( m_hSDLWindow, m_hGLContext );

				int nTryCount = 0;
				while ( !bSuccess && nTryCount  < 1000 )
				{
					ThreadSleep( 10 );
					bSuccess = MakeCurrent( m_hSDLWindow, m_hGLContext );
					nTryCount++;
				}
				Assert( nTryCount <= 2 );
			}
			Assert( bSuccess ); 

			GL_CHECK_CURRENT_CONTEXT;
		}
	}

	DestroyDeviceResources( true );
	SAFE_DELETE( m_pUITextureOpaqueMask );

	glBindFramebufferEXT( GL_FRAMEBUFFER_EXT, 0 );
	CHECK_GL_ERRORS();

	FOR_EACH_MAP_FAST( m_mapTextures, i )
	{
		delete m_mapTextures[i];
	}
	m_mapTextures.RemoveAll();
	m_vecPBOTextures.RemoveAll();

	g_IUITextServices->FreeTextTextureCache( m_pTextTextureCache );
	g_IUITextServices->FreeTextLayoutDrawCache( m_pTextLayoutDrawCache );

	FOR_EACH_VEC( m_vecAllTextBitmaps, i )
	{
		GLuint unTextureId = m_vecAllTextBitmaps[i];
		Assert( glIsTexture( unTextureId ) );
		glDeleteTextures( 1, &unTextureId );
	}
	m_vecAllTextBitmaps.RemoveAll();

   
	// Should only have the backbuffer layer in stack, or zero
	Assert( m_stackCompositionLayers.Count() <= 1 );
	FOR_EACH_VEC( m_stackCompositionLayers, i )
	{
		SAFE_DELETE( m_stackCompositionLayers[i] );
	}
	m_stackCompositionLayers.RemoveAll();

	m_FreeLayers.DeleteAll();
	m_ReservedLayers.DeleteAll();
	m_ShadowLayers.DeleteAll();

	DeleteFBOFreeList();
	AssertMsg1(m_unTotalFBOs == 0, "Leaking %d FBOs\n", m_unTotalFBOs);		// ensure we are not leaking drawables

	if ( IUIEngine::BIsOverlayTarget( m_eRenderTarget ) && m_hSDLWindow )
	{
		SDL_DestroyWindow( m_hSDLWindow );
		m_hSDLWindow = NULL;
	}
	m_hGLContext = NULL;

#if defined(LINUX)
	m_xDpy = None;
	m_xWindow = None;
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Initialization
//-----------------------------------------------------------------------------
bool COpenGLSurface::BInitialize( SDL_Window *pSDLWindow, void *hGLContext,
		int nSurfaceWidth, int nSurfaceHeight, int nWindowWidth, int nWindowHeight, IUIEngine::ERenderTarget eRenderType, bool bEnforceAspectRatio, bool bFixedSurfaceSize, CMouseCursorRender *pCursorRender )
{
	AUTO_LOCK( m_mutexTexturePBO ); // lock the PBO texture buffer until we finish initializing

	const char szSteamProperty[] = "STEAM_BIGPICTURE";
	const char szSteamOverlayProperty[] = "STEAM_OVERLAY";
	const char *szWindowProperty = szSteamProperty;

	m_unSurfaceWidth = nSurfaceWidth;
	m_unSurfaceHeight = nSurfaceHeight;
	m_ERenderState = k_ERenderStateUnset;
	m_eRenderTarget = eRenderType;
	m_pCursorRender = pCursorRender;
	m_hSDLWindow = pSDLWindow;
	m_unWindowWidth = nWindowWidth;
	m_unWindowHeight = nWindowHeight;
	m_bFixedSurfaceSize = bFixedSurfaceSize;

	m_hGLContext = hGLContext;

	m_bEnforceAspectRatio = bEnforceAspectRatio;
	ComputeBackbufferScaling();
	
	m_pTextTextureCache = g_IUITextServices->CreateTextTextureCache( this );
	m_pTextLayoutDrawCache = g_IUITextServices->CreateTextLayoutDrawCache( this );

	if ( IUIEngine::BIsOverlayTarget( m_eRenderTarget ) || IUIEngine::BIsRenderingToOpenVROverlay( m_eRenderTarget ) )
	{
		unsigned int iOverlayWindowFlags = SDL_WINDOW_OPENGL;
		
		szWindowProperty = szSteamOverlayProperty;
		
		if ( IUIEngine::BIsRenderingToTexture( m_eRenderTarget ) )
			iOverlayWindowFlags |= SDL_WINDOW_HIDDEN;
		else
		{
			Assert ( m_eRenderTarget == IUIEngine::k_ERenderToOverlaySteamWM );
			
			// Code to pick ARGB visual goes here.
		}

		// when using the overlay make our own internal SDL window to use
		Assert( m_hSDLWindow == NULL );
		m_hSDLWindow = SDL_CreateWindow( "SteamOverlay", 0, 0, m_unWindowWidth, m_unWindowHeight, iOverlayWindowFlags );
		if ( !m_hSDLWindow )
			Error( "SDL_CreateWindow failed" );

		Assert( hGLContext == NULL );
		
#if GL_DEBUG
		SDL_GL_SetAttribute( SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG );
#endif

		// SDL's GL context type is just a regular GLXContext
		m_hGLContext = SDL_GL_CreateContext( m_hSDLWindow );
		if ( !m_hGLContext )
			Error( "SDL_GL_CreateContext failed" );
	}

	// get the X11 variables out of SDL because we use them ourselves...
	SDL_SysWMinfo SDLWMinfo;
	memset( &SDLWMinfo, 0, sizeof( SDLWMinfo ) );
	SDL_VERSION( &SDLWMinfo.version );
	if ( !SDL_GetWindowWMInfo( m_hSDLWindow, &SDLWMinfo ) )
		Error(" SDL_GetWindowWMInfo failed" );

#ifdef LINUX
	if ( SDLWMinfo.subsystem != SDL_SYSWM_X11 )
		Error(" SDL_GetWindowWMInfo returned an unsupported subsystem (we require X11)" );
#elif defined(OSX)
	if ( SDLWMinfo.subsystem != SDL_SYSWM_COCOA )
		Error(" SDL_GetWindowWMInfo returned an unsupported subsystem (we require Cocoa)" );
#elif defined(WIN32)
	if ( SDLWMinfo.subsystem != SDL_SYSWM_WINDOWS )
		Error( " SDL_GetWindowWMInfo returned an unsupported subsystem (we require Windows)" );
#else 
#error
#endif
	
#if defined(LINUX)
	// Tag the window to give context to the Steambox compositor
	unsigned int iValue = 1;
	
	m_xDpy = SDLWMinfo.info.x11.display;
	m_xWindow = SDLWMinfo.info.x11.window;
	
	XChangeProperty(m_xDpy, m_xWindow, XInternAtom(m_xDpy, szWindowProperty, False),
					XA_CARDINAL, 32, PropModeReplace, (unsigned char *) &iValue, 1L);

	GDBusConnection *connection = g_bus_get_sync( G_BUS_TYPE_SYSTEM, NULL, NULL );

	if ( !GetLogindSession( connection, sCurrentSession ) )
	{
		// Fall back to ConsoleKit if LoginD isn't available
		GVariant *reply = g_dbus_connection_call_sync ( connection,
														"org.freedesktop.ConsoleKit",
														"/org/freedesktop/ConsoleKit/Manager",
														"org.freedesktop.ConsoleKit.Manager",
														"GetCurrentSession",
														NULL,
														NULL,
														G_DBUS_CALL_FLAGS_NONE,
														2000,
														NULL,
														NULL );
		
		if (reply)
		{
			char *szReply = NULL;
			g_variant_get (reply, "(o)", &szReply);
			sCurrentSession = szReply;
			g_variant_unref ( reply );
			reply = NULL;
		}
	}
	
	Msg( "COpenGLSurface::BInitialize: Current session is %s\n", sCurrentSession.String() );
#endif

	bool bSuccess = MakeCurrent( m_hSDLWindow, m_hGLContext );
	Assert( bSuccess );

#if GL_DEBUG
	//
	// Enable debug callback
	//
	// EnableGLDebug();
#endif

	GL_CHECK_CURRENT_CONTEXT;

	SDL_GL_SetSwapInterval( m_bVsyncEnabled ? 1 : 0 );	
	
	if ( !BCreateOpenGLDeviceResources() )
		return false;

	if ( !BRecreateBaseCompositionLayer() )
		return false;
	
	GL_CHECK_CURRENT_CONTEXT;
	
	MakeCurrent( m_hSDLWindow, 0 );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Compute back buffer scaling/translation factors
//-----------------------------------------------------------------------------
void COpenGLSurface::ComputeBackbufferScaling()
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


#if defined(LINUX) && defined(_DEBUG)
//
//  Defines needed to setup GLX debugging
//
//  Most of these will be defined when we go to a later version of gl (we're currently at GL_GLEXT_VERSION 40)
//

typedef GLXContext (*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig, GLXContext, Bool, const int*);

typedef void (APIENTRY *GLDEBUGPROCARB)(GLenum source,
                      GLenum type,
                      GLuint id,
                      GLenum severity,
                      GLsizei length,
                      const GLchar* message,
                      const GLvoid* userParam);

typedef void (APIENTRY * GLDEBUGMESSAGECONTROLARBPROC)(GLenum source,
                GLenum type,
                GLenum severity,
                GLsizei count,
                const GLuint* ids,
                GLboolean enabled);

//  Get's called whenever we send a bad message to GL.
typedef void (APIENTRY *GLDEBUGMESSAGECALLBACKARBPROC)(GLDEBUGPROCARB callback,
                 GLvoid* userParam);
#define GLX_CONTEXT_FLAGS_ARB                   0x2094
#define GLX_CONTEXT_DEBUG_BIT_ARB               0x0001

#define GL_DEBUG_SEVERITY_LOW_ARB         0x9148
#define GL_DEBUG_SEVERITY_MEDIUM_ARB      0x9147
#define GL_DEBUG_SEVERITY_HIGH_ARB        0x9146

#define GL_DEBUG_SOURCE_API_ARB           0x8246
#define GL_DEBUG_SOURCE_WINDOW_SYSTEM_ARB 0x8247
#define GL_DEBUG_SOURCE_SHADER_COMPILER_ARB 0x8248
#define GL_DEBUG_SOURCE_THIRD_PARTY_ARB   0x8249
#define GL_DEBUG_SOURCE_APPLICATION_ARB   0x824A
#define GL_DEBUG_SOURCE_OTHER_ARB         0x824B

#define GL_DEBUG_TYPE_ERROR_ARB           0x824C
#define GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_ARB 0x824D
#define GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_ARB 0x824E
#define GL_DEBUG_TYPE_PORTABILITY_ARB     0x824F
#define GL_DEBUG_TYPE_PERFORMANCE_ARB     0x8250
#define GL_DEBUG_TYPE_OTHER_ARB           0x8251
//
//  Pretty print routines for our callback function.
//
const char* glSourceToString(GLenum source)
{
  switch (source)
    {
    case GL_DEBUG_SOURCE_API_ARB:                           return "API";
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM_ARB:         return "WINDOW_SYSTEM";
    case GL_DEBUG_SOURCE_SHADER_COMPILER_ARB:       return "SHADER_COMPILER";
    case GL_DEBUG_SOURCE_THIRD_PARTY_ARB:           return "THIRD_PARTY";
    case GL_DEBUG_SOURCE_APPLICATION_ARB:           return "APPLICATION";
    case GL_DEBUG_SOURCE_OTHER_ARB:                         return "OTHER";
    default:                                                                        break;
    }
  return "UNKNOWN";
}

const char* glTypeToString(GLenum type)
{
  switch (type)
    {
    case GL_DEBUG_TYPE_ERROR_ARB:                           return "ERROR";
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_ARB:     return "DEPRECATION";
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_ARB:      return "UNDEFINED_BEHAVIOR";
    case GL_DEBUG_TYPE_PORTABILITY_ARB:                     return "PORTABILITY";
    case GL_DEBUG_TYPE_PERFORMANCE_ARB:                     return "PERFORMANCE";
    case GL_DEBUG_TYPE_OTHER_ARB:                           return "OTHER";
    default:                                                                        break;
    }
  return "UNKNOWN";
}

const char* glSeverityToString(GLenum severity)
{
  switch (severity)
    {
    case GL_DEBUG_SEVERITY_HIGH_ARB:                        return "HIGH";
    case GL_DEBUG_SEVERITY_MEDIUM_ARB:                      return "MEDIUM";
    case GL_DEBUG_SEVERITY_LOW_ARB:                         return "LOW";
    default:                                                                        break;
    }
  return "UNKNOWN";
}


//-----------------------------------------------------------------------------
// Purpose: Enable the GL debugging callback
//-----------------------------------------------------------------------------
void COpenGLSurface::EnableGLDebug() 
{ 
	static bool runOnce = true;
	if ( runOnce )
	{
		//
		//  Binding the GL debugging functions here...We only need to do this once.
		//

		GLDEBUGMESSAGECALLBACKARBPROC glDebugMessageCallbackARB = 0;
		glDebugMessageCallbackARB = (GLDEBUGMESSAGECALLBACKARBPROC) glXGetProcAddress( (const GLubyte *) "glDebugMessageCallbackARB" );

		GLDEBUGMESSAGECONTROLARBPROC glDebugMessageControlARB = 0;
		glDebugMessageControlARB = (GLDEBUGMESSAGECONTROLARBPROC) glXGetProcAddress( (const GLubyte *) "glDebugMessageControlARB" );

		if ( glDebugMessageControlARB != NULL )
		{
			glDebugMessageControlARB(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, (const GLuint *)NULL, GL_TRUE);
			CHECK_GL_ERRORS();

			// Gonna filter these out, they're "chatty".                                                                                                                                       
			glDebugMessageControlARB(GL_DEBUG_SOURCE_API_ARB, GL_DEBUG_TYPE_PERFORMANCE_ARB, GL_DONT_CARE, 0, NULL, GL_FALSE);
			GLuint filter[] = {131204, 131185, 131188};
			glDebugMessageControlARB(GL_DEBUG_SOURCE_API_ARB, GL_DEBUG_TYPE_OTHER_ARB, GL_DONT_CARE, 3, filter, GL_FALSE);
			CHECK_GL_ERRORS();
		}


		// Register the actual callback
		if ( glDebugMessageCallbackARB != NULL )
		{
			glDebugMessageCallbackARB(GL_Debug_Output_Callback, (void*)NULL);
			CHECK_GL_ERRORS();
		}


		runOnce = false;	// don't run again
	}
}

//
//  Actual callback function called by GL whenever we make a mistake with the GL driver.
//
void APIENTRY COpenGLSurface::GL_Debug_Output_Callback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const GLvoid* userParam)
{
    const char *sSource = glSourceToString(source);
    const char *sType = glTypeToString(type);
    const char  *sSeverity = glSeverityToString(severity);

    //  At this point we only care about real errors, and high severities.
    //  We could also choose any of GL_DEBUG_TYPEs output.  Change the type comparison below to get them.
    if ( GL_DEBUG_SEVERITY_HIGH_ARB == severity ||  GL_DEBUG_TYPE_ERROR_ARB == type )
    {
        Msg("GL: [%s][%s][%s][%d]: %s\n", sSource, sType, sSeverity, id, message);
		DebuggerBreak();
    }
	else
	{
        Msg("GL: [%s][%s][%s][%d]: %s\n", sSource, sType, sSeverity, id, message);
	}

}
#endif


//-----------------------------------------------------------------------------
// Purpose: see if we should reload our shaders
//-----------------------------------------------------------------------------
void COpenGLSurface::ReloadChangedFile( const char *pchFile )
{
	if  ( strstr( pchFile, "orthographic2d.vert" ) != NULL 
		 || strstr( pchFile, "tex2dblur.frag" ) != NULL 
		 || strstr( pchFile, "tex2dparticle.frag" ) != NULL 
		 || strstr( pchFile, "fancyquaduber.vert" ) != NULL 
		 || strstr( pchFile, "fancyquaduber.frag" ) != NULL 
		 )
	{
		// XXX: This seems broken - it's trying to access the same context on
		// the main thread that we use to render on our rendering thread. In
		// addition, it's tearing down and reinitializing the resources while
		// the render therad is running.
		DestroyDeviceResources( true );
		BCreateOpenGLDeviceResources();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Destroy opengl device resources
//-----------------------------------------------------------------------------
void COpenGLSurface::DestroyDeviceResources( bool bShuttingDown )
{
	m_shaderprogramBlur.Clear();
	m_shaderprogramParticle.Clear();

	bool bDumpShaders = bShuttingDown && CommandLine()->FindParm( g_DumpUsedShaders.GetHParam() );
	if ( bDumpShaders )
	{
		Msg( "\n" );
		Msg( "==========================================\n" );
		Msg( "Dumping list of loaded fancy quad shaders:\n" );
	}
	
	for ( int nTextureType = 0; nTextureType < FancyQuadTextureType_Total; nTextureType++ )
	{
		for ( int nShaderFlags = 0; nShaderFlags < FancyQuadFlag_Total; nShaderFlags++ )
		{
			CLazyShaderProgram &shader = m_shaderprogramFancyQuadUber[nTextureType][nShaderFlags];
			if ( bDumpShaders && shader.BLinked() )
				Msg( "%i,%i\n", nTextureType, nShaderFlags );

			shader.Clear();
		}
	}

	if ( bDumpShaders )
		Msg( "==========================================\n\n" );
	
    if ( m_iGradientTextureName )
    {
        glDeleteTextures(1, &m_iGradientTextureName);
		CHECK_GL_ERRORS();
        m_iGradientTextureName = 0;
    }
}


//-----------------------------------------------------------------------------
// Purpose: Should only be called on overlay render-to-texture instances, tells 
// us to push the overlay render cmd stream necessary to draw our content.  This is 
// called from the main thread outside the render thread.
//-----------------------------------------------------------------------------
void COpenGLSurface::PushOverlayRenderCmdStream( CSharedMemStream *pRenderStream, DWORD dwPID, float flOpacity, uint32 unGameWidth, uint32 unGameHeight, EOverlayWindowAlignment alignment )
{
#if defined(LINUX)
	// On Steambox, just send the overlay opacity to the compositor and exit.
	if ( m_eRenderTarget == IUIEngine::k_ERenderToOverlaySteamWM )
	{
		double dOpacity = flOpacity;
		unsigned int iOpacity = (unsigned int)(dOpacity * 0xFFFFFFFF);
		
		if ( m_xDpy == None || m_xWindow == None )
			return;

		if ( iOpacity == m_iCurrentWMOpactity )
			return;

		m_iCurrentWMOpactity = iOpacity;
		XChangeProperty( m_xDpy, m_xWindow, XInternAtom(m_xDpy, "_NET_WM_WINDOW_OPACITY", False),
						 XA_CARDINAL, 32, PropModeReplace, (unsigned char *) &iOpacity, 1L );
		return;
	}
#endif
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
#define DWORD_ARGB(a,r,g,b) \
((DWORD)((((a)&0xff)<<24)|(((r)&0xff)<<16)|(((g)&0xff)<<8)|((b)&0xff)))

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
// Purpose: Helper to check if the given flags are valid in combination.
//-----------------------------------------------------------------------------
static bool BAreValidShaderFlags( const int nShaderFlags )
{
	// These are mutually exclusive, so we don't generate shaders with both of them set.
	const int k_nAllGradientFlags = FancyQuadFlag_GradientComplex | FancyQuadFlag_GradientTwoStop;
	if ((nShaderFlags & k_nAllGradientFlags) == k_nAllGradientFlags)
		return false;
	
	// Using radial gradients only makes sense if we have one of the gradient flags enabled.
	if ((nShaderFlags & k_nAllGradientFlags) == 0 && (nShaderFlags & FancyQuadFlag_RadialGradient) != 0)
		return false;
	
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Create opengl device resources
//-----------------------------------------------------------------------------
bool COpenGLSurface::BCreateOpenGLDeviceResources()
{
	bool success = true; // forestw: we hope

	GL_CHECK_CURRENT_CONTEXT;
	CHECK_GL_ERRORS();

	if ( CLazyShaderProgram::BTryingBinaryShaders() )
	{
		GLint formats = 0;
		glGetIntegerv( GL_NUM_PROGRAM_BINARY_FORMATS, &formats );
		if ( formats < 1 )
			Msg( "COpenGLSurface::BCreateOpenGLDeviceResources: Driver does not support any formats for binary shaders.\n" );
		else
			Msg( "COpenGLSurface::BCreateOpenGLDeviceResources: Driver supports %i binary shader formats, attempting to use them.\n", formats );
	}

	//
	// 1. Load the shaders, both vertex and frag
	// 
	
	// this vertex shader is used with several following fragment shaders
	CVertexShader *ortographic2DVert = new CVertexShader();
	if ( !ortographic2DVert->AddSourceFromFile( "opengl/orthographic2d.vert" ) )
		success = false;
	
	CFragmentShader *fragShader = new CFragmentShader();
	if ( !fragShader->AddSourceFromFile( "opengl/tex2dblur.frag" ) )
		success = false;

	if ( !m_shaderprogramBlur.Attach( ortographic2DVert, fragShader ) )
		success = false;
	fragShader->Release();

	fragShader = new CFragmentShader();
	if ( !fragShader->AddSourceFromFile( "opengl/tex2dparticle.frag" ) )
		success = false;

	if ( !m_shaderprogramParticle.Attach( ortographic2DVert, fragShader ) )
		success = false;
	fragShader->Release();
	fragShader = NULL;

	ortographic2DVert->Release();
	ortographic2DVert = NULL;

	CVertexShader *fancyQuadUberVert = new CVertexShader();
	if ( !fancyQuadUberVert->AddSourceFromFile( "opengl/fancyquaduber.vert" ) )
		success = false;

	// Compile permutations of the fancyquaduber shader
	CUtlString strUTF8 = UIEngine()->GetLocalPathForNamedPath( "{shaders}" );
	strUTF8 += "opengl/fancyquaduber.frag";
	CUtlBuffer bufFile;
	LoadFileIntoBuffer( strUTF8.String(), bufFile, false );
	if ( bufFile.TellPut() == 0 )
	{
		Msg( "Failed to read fragment file %s", strUTF8.String() );
		return false;
	}

	// Load sources for all the permutations of the FancyQuadUber shader, with 4 different texture types and a variety of flags set.
	for ( int nTextureType = 0; nTextureType < FancyQuadTextureType_Total; nTextureType++ )
	{
		for ( int nShaderFlags = 0; nShaderFlags < FancyQuadFlag_Total; nShaderFlags++ )
		{
			if ( !BAreValidShaderFlags( nShaderFlags ) )
				continue;

			CFragmentShader *fancyQuadUberFrag = new CFragmentShader();
			fancyQuadUberFrag->SetName( "opengl/fancyquaduber.frag" );
			fancyQuadUberFrag->AddSource( FragSource_FancyQuadTextureVersion );
			fancyQuadUberFrag->AddSource( FragSource_FancyQuadTextureType[nTextureType] );
			for ( int nFlagNum = 0; ( 1 << nFlagNum ) <= nShaderFlags; nFlagNum++ )
			{
				if ( ( nShaderFlags & ( 1 << nFlagNum ) ) != 0 )
					fancyQuadUberFrag->AddSource( FragSource_FancyQuadFlag[nFlagNum] );
			}
			fancyQuadUberFrag->AddSource( ( const char * )bufFile.Base() );

			CLazyShaderProgram &fancyProgram = m_shaderprogramFancyQuadUber[nTextureType][nShaderFlags];
			if ( !fancyProgram.Attach( fancyQuadUberVert, fancyQuadUberFrag ) )
				success = false;

			fancyProgram.SetName( CFmtStr( "fancyquaduber[%i][%i]", nTextureType, nShaderFlags ).String() );

			fancyQuadUberFrag->Release();
		}
	}
	
	fancyQuadUberVert->Release();
	fancyQuadUberVert = NULL;

	CHECK_GL_ERRORS();

	if ( CLazyShaderProgram::BShouldPreloadShaders() )
	{
		if ( !BPreloadShaders() )
			success = false;
	}
	
	//
	// 2. Now make the PBO's for texture uploads
	//
	
	m_iCurrentWriteTexturePBO = 0;
	m_cbTextureWriteOffset = 0;
	for ( int i = 0; i < V_ARRAYSIZE(m_hTexturePBO); i++ )
	{
		glGenBuffers( 1, &m_hTexturePBO[i] );
	
		glBindBuffer( GL_PIXEL_UNPACK_BUFFER, m_hTexturePBO[i] );
		glBufferData( GL_PIXEL_UNPACK_BUFFER, k_cbPBOTextureBuffer, NULL, GL_DYNAMIC_DRAW );
		CHECK_GL_ERRORS();
		glBindBuffer( GL_PIXEL_UNPACK_BUFFER, 0 );
	}
	
	glBindBuffer( GL_PIXEL_UNPACK_BUFFER, m_hTexturePBO[0] );
	m_pbTextureUploads = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
	glBindBuffer( GL_PIXEL_UNPACK_BUFFER, 0 );
	
	
	if ( IUIEngine::BIsRenderingToTexture( m_eRenderTarget ) )
	{
		m_iCurrentOverlayPBO = 0;
		for ( int i = 0; i < V_ARRAYSIZE(m_hOverlayPBO); i++ )
		{
			glGenBuffers( 1, &m_hOverlayPBO[i] );
			
			glBindBuffer( GL_ARRAY_BUFFER, m_hOverlayPBO[i] );
			glBufferData( GL_ARRAY_BUFFER, m_unWindowWidth*m_unWindowHeight*4, NULL, GL_STREAM_READ );
			CHECK_GL_ERRORS();
			glBindBuffer( GL_ARRAY_BUFFER, 0 );
		}
	}
	
	// Initialize our scratch gradient buffer.
	glGenTextures(1, &m_iGradientTextureName);
	glBindTexture( GL_TEXTURE_1D, m_iGradientTextureName );
	glTexImage1D(	GL_TEXTURE_1D, 0, GL_RGBA, FANCYQUAD_GRADIENT_TEXTURE_SIZE,
					0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri( GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glBindTexture( GL_TEXTURE_1D, 0 );
	
	return success;
}

//-----------------------------------------------------------------------------------------
// Purpose: Parse a line from glfancyquadshaders.cfg, which has two comma-separated values
//-----------------------------------------------------------------------------------------
static bool ParseShaderDesc( const char *pchShaderDesc, int &nTextureType, int &nShaderFlags )
{
	char *pchEndStr = NULL;
	nTextureType = V_atoi( pchShaderDesc, &pchEndStr );
	if ( !pchEndStr )
		return false;

	const char *pchShaderFlags = V_strchr( pchShaderDesc, ',' );
	if ( !pchShaderFlags || !*pchShaderFlags )
		return false;
	
	nShaderFlags = V_atoi( pchShaderFlags + 1, &pchEndStr );
	if ( !pchEndStr )
		return false;

	if ( nTextureType < 0 || nTextureType >= FancyQuadTextureType_Total )
		return false;
	
	if ( nShaderFlags < 0 || nShaderFlags >= FancyQuadFlag_Total )
		return false;
	
	return true;
}

//--------------------------------------------------------------------------------------
// Purpose: Compile & link the shaders we'll be using, based on glfancyquadshaders.cfg.
//--------------------------------------------------------------------------------------
bool COpenGLSurface::BPreloadShaders()
{
	bool bSuccess = true;

	if ( !m_shaderprogramBlur.Preload() )
		bSuccess = false;

	if ( !m_shaderprogramParticle.Preload() )
		bSuccess = false;

	CUtlString strUTF8 = UIEngine()->GetLocalPathForNamedPath( "{shaders}" );
	strUTF8 += "glfancyquadshaders.cfg";

	CUtlBuffer bufFile;
	if ( !LoadFileIntoBuffer( strUTF8.String(), bufFile, true ) )
	{
		Warning( "Failed to read shader cfg file (%s) - will preload every shader.\n", strUTF8.String() );
		for ( int nTextureType = 0; nTextureType < FancyQuadTextureType_Total; nTextureType++ )
		{
			for ( int nShaderFlags = 0; nShaderFlags < FancyQuadFlag_Total; nShaderFlags++ )
			{
				if ( !BAreValidShaderFlags( nShaderFlags ) )
					continue;

				if ( !m_shaderprogramFancyQuadUber[nTextureType][nShaderFlags].Preload() )
					bSuccess = false;
			}
		}

		return bSuccess;
	}

	char pchShaderDesc[32];
	while ( bufFile.GetLine( pchShaderDesc, V_ARRAYSIZE( pchShaderDesc ) ) )
	{
		int nTextureType = 0;
		int nShaderFlags = 0;
		if ( !ParseShaderDesc( pchShaderDesc, nTextureType, nShaderFlags ) )
		{
			AssertMsg1( true, "glfancyquadshaders.cfg contained invalid description: '%s'", pchShaderDesc );
			continue;
		}

		if ( !m_shaderprogramFancyQuadUber[nTextureType][nShaderFlags].Preload() )
			bSuccess = false;
	}
	
	CHECK_GL_ERRORS();

	return bSuccess;
}


//-----------------------------------------------------------------------------
// Purpose: Create resources that are resizable
//-----------------------------------------------------------------------------
bool COpenGLSurface::BRecreateBaseCompositionLayer()
{
	VPROF_BUDGET( "COpenGLSurface::BRecreateBaseCompositionLayer", VPROF_BUDGETGROUP_TENFOOT );

	//
	// Flush all our FBOs and cached composition layers
	//
	DropLayerCaches( false );
	DeleteFBOFreeList();
	
	// BUGBUG We shouldn't have to do this; the gradient texture is always reuploaded.
	// It looks like there's a driver bug where the texture sometimes gets irremediably
	// destroyed on suspend resume and this works around it.
	if ( m_iGradientTextureName )
	{
		glDeleteTextures(1, &m_iGradientTextureName);
		glGenTextures(1, &m_iGradientTextureName);
		glBindTexture( GL_TEXTURE_1D, m_iGradientTextureName );
		glTexImage1D(	GL_TEXTURE_1D, 0, GL_RGBA, FANCYQUAD_GRADIENT_TEXTURE_SIZE,
						0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		glTexParameteri( GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		glBindTexture( GL_TEXTURE_1D, 0 );
	}

	// Should only have the backbuffer layer in stack, or zero
	Assert( m_stackCompositionLayers.Count() <= 1 );
	FOR_EACH_VEC( m_stackCompositionLayers, i )
	{
		SAFE_DELETE( m_stackCompositionLayers[i] );
	}
	m_stackCompositionLayers.RemoveAll();

	// Setup top level of composition layers
	Assert( m_stackCompositionLayers.Count() == 0 );
	
	GL_CHECK_CURRENT_CONTEXT;
	
	glBindFramebufferEXT( GL_FRAMEBUFFER_EXT, 0 );
	SetLastActiveFBO( 0 );

	CHECK_GL_ERRORS();
	// forestw: retrieve the system fbo provided to us so we can return to it as needed (if we ever do an OpenGL ES port we will need to honor the system fbo as it is not object 0).
	glGetIntegerv( GL_FRAMEBUFFER_BINDING_EXT, &m_hSystemFBO );
	CHECK_GL_ERRORS();

	CCompositionLayer *pLayer = new CCompositionLayer( this, m_hGLContext, m_unSurfaceWidth, m_unSurfaceHeight );
	m_stackCompositionLayers.AddToTail( pLayer );
	
	float r = 1.0f;
	float g = 1.0f;
	float b = 1.0f;
	float flOpacity = 1.0f;
	
	VertexTextured_t *pQuad = pLayer->AccessRenderQuad();
	pQuad[0].r = r;
	pQuad[0].g = g;
	pQuad[0].b = b;
	pQuad[0].a = flOpacity;
	pQuad[0].rhw = 1.0f;
	pQuad[0].masku1 = pQuad[0].masku2 = pQuad[0].u = 0.0f;
	pQuad[0].maskv1 = pQuad[0].maskv2 = pQuad[0].v = 1.0f;
	pQuad[0].x = 0;
	pQuad[0].y = 0;
	pQuad[0].z = 0;
	
	pQuad[1].r = r;
	pQuad[1].g = g;
	pQuad[1].b = b;
	pQuad[1].a = flOpacity;
	pQuad[1].rhw = 1.0f;
	pQuad[1].masku1 = pQuad[1].masku2 = pQuad[1].u = 1.0f;
	pQuad[1].maskv1 = pQuad[1].maskv2 = pQuad[1].v = 1.0f;
	pQuad[1].x = m_unSurfaceWidth;
	pQuad[1].y = 0;
	pQuad[1].z = 0;
	
	pQuad[2].r = r;
	pQuad[2].g = g;
	pQuad[2].b = b;
	pQuad[2].a = flOpacity;
	pQuad[2].rhw = 1.0f;
	pQuad[2].masku1 = pQuad[2].masku2 = pQuad[2].u = 1.0f;
	pQuad[2].maskv1 = pQuad[2].maskv2 = pQuad[2].v = 0.0f;
	pQuad[2].x = m_unSurfaceWidth;
	pQuad[2].y = m_unSurfaceHeight;
	pQuad[2].z = 0;
	
	pQuad[3].r = r;
	pQuad[3].g = g;
	pQuad[3].b = b;
	pQuad[3].a = flOpacity;
	pQuad[3].rhw = 1.0f;
	pQuad[3].masku1 = pQuad[3].masku2 = pQuad[3].u = 0.0f;
	pQuad[3].maskv1 = pQuad[3].maskv2 = pQuad[3].v = 0.0f;
	pQuad[3].x = 0;
	pQuad[3].y = m_unSurfaceHeight;
	pQuad[3].z = 0;

	CHECK_GL_ERRORS();

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Determine if we need a mode switch or state update of some kind and do it
//-----------------------------------------------------------------------------
bool COpenGLSurface::BUpdateRenderStateIfNeeded( IUIEngine::ERenderTarget eMsgRenderTarget )
{
	if ( IUIEngine::BValidRenderStateChange( GetRenderTarget(), eMsgRenderTarget ) )
	{
		m_eRenderTarget = eMsgRenderTarget;
		if ( m_eRenderTarget == IUIEngine::k_ERenderFullScreen || m_eRenderTarget == IUIEngine::k_ERenderBorderlessFullScreenWindow )
		{
			SDL_SetWindowFullscreen( m_hSDLWindow, SDL_WINDOW_FULLSCREEN_DESKTOP );
		}
		else
		{
			SDL_SetWindowFullscreen( m_hSDLWindow, SDL_FALSE );
		}
		return true;
	}
	
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Determine if we need to resize the backbuffer and do it
//-----------------------------------------------------------------------------
bool COpenGLSurface::BUpdateWindowSizeIfNeeded( uint32 nWidth, uint32 nHeight )
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

		//
		// Resize the overlay PBO
		//
		if ( IUIEngine::BIsRenderingToTexture( m_eRenderTarget ) )
		{
			for (int i = 0; i < V_ARRAYSIZE(m_hOverlayPBO); i++ )
			{
				glBindBuffer( GL_ARRAY_BUFFER, m_hOverlayPBO[i]);
				glBufferData( GL_ARRAY_BUFFER, m_unWindowWidth*m_unWindowHeight*4, NULL, GL_STREAM_READ );
				CHECK_GL_ERRORS();
				glBindBuffer( GL_ARRAY_BUFFER, 0 );
			}
		}
		return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Swap the pbo for pending textures and upload as needed
//-----------------------------------------------------------------------------
void COpenGLSurface::PerformPendingTextureUploads()
{
	VPROF_BUDGET( "COpenGLSurface::PerformPendingTextureUploads()", VPROF_BUDGETGROUP_TENFOOT );

	CUtlVector<PendingTextureUpload_t> vecPendingUploads;
	int hCurrentPBO = -1;
	
	{
		// lock and swap PBO's if uploads are pending
		AUTO_LOCK( m_mutexTexturePBO );
		vecPendingUploads.Swap( m_vecPendingTextureUploads );
		
		if ( vecPendingUploads.Count() > 0 )
		{
			hCurrentPBO = m_iCurrentWriteTexturePBO;
			glBindBuffer( GL_PIXEL_UNPACK_BUFFER, m_hTexturePBO[hCurrentPBO] );
			glUnmapBuffer( GL_PIXEL_UNPACK_BUFFER );
			glBindBuffer( GL_PIXEL_UNPACK_BUFFER, 0 );
			
			m_iCurrentWriteTexturePBO = (m_iCurrentWriteTexturePBO+1)%V_ARRAYSIZE(m_hTexturePBO);

			glBindBuffer( GL_PIXEL_UNPACK_BUFFER, m_hTexturePBO[m_iCurrentWriteTexturePBO] );
			glBufferData( GL_PIXEL_UNPACK_BUFFER, k_cbPBOTextureBuffer, NULL, GL_DYNAMIC_DRAW );
			m_pbTextureUploads = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
			glBindBuffer( GL_PIXEL_UNPACK_BUFFER, 0 );
		}
	}
	
	// Lock is released now, lets commit the uploads
	if ( vecPendingUploads.Count() > 0 )
	{
		glBindBuffer( GL_PIXEL_UNPACK_BUFFER, m_hTexturePBO[hCurrentPBO] );
		
		// first commit all the PBO uploads
		FOR_EACH_VEC( vecPendingUploads, i )
		{
			if ( !vecPendingUploads[i].bPBOUpload )
				continue;
			
			COpenGLTexture *pTexture = vecPendingUploads[i].pTexture;
			pTexture->CreateOGLTextureIDIfNeeded( this );

			Assert( pTexture->GetOGLTextureID() );
			Assert( pTexture->GetTextureWidth() > 0 );
			Assert( pTexture->GetTextureHeight() > 0 );

			SetTexture( GL_TEXTURE0_ARB, pTexture->GetOGLTextureID() );

			CHECK_GL_ERRORS();
			glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
			glPixelStorei( GL_UNPACK_ROW_LENGTH, pTexture->GetStride() );
			
			CHECK_GL_ERRORS();
			glTexImage2D( GL_TEXTURE_2D, 0, pTexture->GetOGLInternalTextureFormat(), pTexture->GetTextureWidth(), pTexture->GetTextureHeight(), 0, pTexture->GetOGLTextureFormat(),  pTexture->GetOGLTextureType(), (void *)vecPendingUploads[i].iPBOOffset );
			CHECK_GL_ERRORS();
			
			// restore unpack row length because it can break other glTexImage2D code using this context
			glPixelStorei( GL_UNPACK_ROW_LENGTH, 0 );
			
			CHECK_GL_ERRORS();
			
			pTexture->SetTextureUploaded();
			m_vecPBOTextures.AddToTail(pTexture);
		}
		
		glBindBuffer( GL_PIXEL_UNPACK_BUFFER, 0 );

		// now do the direct uploads
		FOR_EACH_VEC( vecPendingUploads, i )
		{
			if ( vecPendingUploads[i].bPBOUpload )
				continue;

			COpenGLTexture *pTexture = vecPendingUploads[i].pTexture;
			pTexture->CreateOGLTextureIDIfNeeded( this );

			Assert( pTexture->GetOGLTextureID() );
			Assert( pTexture->GetTextureWidth() > 0 );
			Assert( pTexture->GetTextureHeight() > 0 );

			SetTexture( GL_TEXTURE0_ARB, pTexture->GetOGLTextureID() );
			
			CHECK_GL_ERRORS();
			glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
			glPixelStorei( GL_UNPACK_ROW_LENGTH, pTexture->GetStride() );
			
			CHECK_GL_ERRORS();
			glTexImage2D( GL_TEXTURE_2D, 0, pTexture->GetOGLInternalTextureFormat(), pTexture->GetTextureWidth(), pTexture->GetTextureHeight(), 0, pTexture->GetOGLTextureFormat(),  pTexture->GetOGLTextureType(), pTexture->GetTextureData() );
			CHECK_GL_ERRORS();
			
			// restore unpack row length because it can break other glTexImage2D code using this context
			glPixelStorei( GL_UNPACK_ROW_LENGTH, 0 );
			
			CHECK_GL_ERRORS();
			
			pTexture->SetTextureUploaded();

			// Since this upload didn't go through the PBO, we don't need to keep the data around.
			pTexture->FreeTextureData();

		}		
	}
	
	// create any textures that need it
	{
		AUTO_LOCK( m_lockTextureMap );
		FOR_EACH_VEC( m_vecPendingYUVCreates, i )
		{
			m_vecPendingYUVCreates[i]->CreateOGLTextureIDIfNeeded( this );
			Assert( m_vecPendingYUVCreates[i]->GetOGLTextureID() );
		}
		m_vecPendingYUVCreates.RemoveAll();

		FOR_EACH_VEC( m_vecPendingDoubleBufferCreates, i )
		{
			m_vecPendingDoubleBufferCreates[i]->CreateOGLTextureIDIfNeeded( this );
		}
		m_vecPendingDoubleBufferCreates.RemoveAll();

	}
}

//-----------------------------------------------------------------------------
// Purpose: After resume from suspend, all texture data stored
// in the graphics memory is lost. This method re-uploads our
// textures to the graphics card.
//-----------------------------------------------------------------------------
void COpenGLSurface::ReloadTextures() 
{ 
	COpenGLTexture *pTexture;
	FOR_EACH_VEC(m_vecPBOTextures, i)
	{
		pTexture = m_vecPBOTextures[i];
		 // re-upload this texture from memory
		Assert( pTexture->GetOGLTextureID() );
		Assert( pTexture->GetTextureWidth() > 0 );
		Assert( pTexture->GetTextureHeight() > 0 );
		Assert( pTexture->GetTextureData() != NULL);

		SetTexture( GL_TEXTURE0_ARB, pTexture->GetOGLTextureID() );

		CHECK_GL_ERRORS();
		glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
		glPixelStorei( GL_UNPACK_ROW_LENGTH, pTexture->GetStride() );

		CHECK_GL_ERRORS();
		glTexImage2D( GL_TEXTURE_2D, 0, pTexture->GetOGLInternalTextureFormat(), pTexture->GetTextureWidth(), pTexture->GetTextureHeight(), 0, pTexture->GetOGLTextureFormat(),  GL_UNSIGNED_BYTE, pTexture->GetTextureData() );
		CHECK_GL_ERRORS();

		// BUGBUG This should be OK but we still get intermittent corruption with it
#if 0
		// texture data is not needed since the re-upload didn't go through the PBO
		pTexture->FreeTextureData();
#endif
	}

	// BUGBUG This should be OK but we still get intermittent corruption with it
	#if 0
	//
	// We won't need to reupload any of these textures since they
	// have been re-uploaded from main memory. So remove everything
	// from the list.
	// 
	m_vecPBOTextures.RemoveAll();
#endif

	// restore unpack row length because it can break other glTexImage2D code using this context
	glPixelStorei( GL_UNPACK_ROW_LENGTH, 0 );
	CHECK_GL_ERRORS();
}


//-----------------------------------------------------------------------------
// Purpose: BeginFrame
//-----------------------------------------------------------------------------
void COpenGLSurface::BeginFrame( const CRenderMsg< CMsgBeginFrame > &renderMsg )
{
	VPROF_BUDGET( "COpenGLSurface::BeginFrame", VPROF_BUDGETGROUP_TENFOOT );

	s_nDebugDumpTexture_FrameNumber++;
	s_nDebugDumpTexture_Number = 1;
	
	m_flCurrentRenderFrameTime = Plat_FloatTime();
	
	if( renderMsg.BodyConst().clear_gpu_resources_before_frame() )
	{
		// Not actually implemented
		//ClearGPUResources();
	}

	bool bVsyncPrevious = m_bVsyncEnabled;
	m_bVsyncEnabled = s_convarPanoramaVsync.GetBool();
	if ( bVsyncPrevious != m_bVsyncEnabled )
	{
		SDL_GL_SetSwapInterval( m_bVsyncEnabled ? 1 : 0 );	
	}	

	bool bSuccess = MakeCurrent( m_hSDLWindow, m_hGLContext );
	Assert( bSuccess );
	IUIEngine::ERenderTarget eMsgRenderTarget = (IUIEngine::ERenderTarget)renderMsg.BodyConst().render_target();
	uint32 nWidth = renderMsg.BodyConst().surface_width();
	uint32 nHeight = renderMsg.BodyConst().surface_height();
	if ( BUpdateRenderStateIfNeeded( eMsgRenderTarget ) || BUpdateWindowSizeIfNeeded( nWidth, nHeight ) )
	{
		ComputeBackbufferScaling();
		DbgVerify( BRecreateBaseCompositionLayer() );
	}
	
	GL_CHECK_CURRENT_CONTEXT;
	
	if ( m_bResumed )
	{
		m_bResumed = false;
		DbgVerify( BRecreateBaseCompositionLayer() );
		ReloadTextures();
	}
	PerformPendingTextureUploads();
	
	// This is a trick to force drivers like nVidia to not try to do "threaded optimization"
	// which means GL stuff is all defered to a thread, but whenever we do a synchronous call,
	// like glGetError, glCheckFramebufferStatusEXT, or even glGenTextures we block horribly long,
	// not a good plan for us.
	glEnable( GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB );

	// we always use premultiplied-alpha blend - well almost always, but we'll set here and then only change as needed inside a frame
	glEnable( GL_BLEND );
	if ( g_glBlendEquationSeparate )
		g_glBlendEquationSeparate( GL_FUNC_ADD, GL_FUNC_ADD );
	glBlendFuncSeparate( GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA );

	ActivateRenderTarget();

#ifdef GLCLEARTEST
	glClearColor( 1.0f, 0.0f, 0.0f, 0.2f );
#else
	glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
#endif
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	CHECK_GL_ERRORS();

	DebugDumpFramebuffer();
	DebugDumpState( __func__ );

	if ( m_stackCompositionLayers.Count() == 0 )
	{
		Assert( m_stackCompositionLayers.Count() != 0 );
	}
	else
	{
		CCompositionLayer *pLayer = m_stackCompositionLayers[ m_stackCompositionLayers.Count()-1 ];
		pLayer->Clear();
		if ( m_stackCompositionLayers.Count() == 1 )
			pLayer->PushCliplayersAndBeginDraw( m_flScaleBackbufferX, m_flScaleBackbufferY, m_flTranslateBackbufferX, m_flTranslateBackbufferY);
		else
			pLayer->PushCliplayersAndBeginDraw( 1.0f, 1.0f, 0.0f, 0.0f );

	}

	m_flLastPaintFrameTime = renderMsg.BodyConst().frame_paint_time();
}


//-----------------------------------------------------------------------------
// Purpose: Clear back buffer
//-----------------------------------------------------------------------------
void COpenGLSurface::ClearBackbuffer( const CRenderMsg< CMsgClearBackbuffer > &renderCommand )
{
	CHECK_GL_ERRORS();
	ActivateRenderTarget();
	CHECK_GL_ERRORS();
	
	const CMsgClearBackbuffer &msgBody = renderCommand.BodyConst();
	float r, g, b, a, color[4];
	LinearColorFromABGR( r, g, b, a, msgBody.clear_color_rgba(), true );
	color[0] = r * a;
	color[1] = g * a;
	color[2] = b * a;
	color[3] = a;

#ifdef GLCLEARTEST
	color[0] = 1.0f;
	color[1] = 0;
	color[2] = 0;
	color[3] = 0.2f;
#endif

	glClearColor( color[0], color[1], color[2], color[3] );
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	CHECK_GL_ERRORS();

	DebugDumpFramebuffer();
	DebugDumpState( __func__ );
}


//-----------------------------------------------------------------------------
// Purpose: Create a texture id
//-----------------------------------------------------------------------------
GLuint COpenGLSurface::CreateOpenGLTextureId()
{
	// forestw: this function is only called with a context already active

	GL_CHECK_CURRENT_CONTEXT;
	CHECK_GL_ERRORS();

	GLuint textNum;

	{
		AUTO_LOCK( m_glTextureMutex );
		if ( m_nFreeGLTextureIndex == -1 || m_nFreeGLTextureIndex >= V_ARRAYSIZE( m_rgFreeGLTextureIDs ) )
		{
			m_nFreeGLTextureIndex = 0;
			glGenTextures( V_ARRAYSIZE( m_rgFreeGLTextureIDs ), m_rgFreeGLTextureIDs );
		}
		textNum = m_rgFreeGLTextureIDs[m_nFreeGLTextureIndex++];
	}

	CHECK_GL_ERRORS();

	SetTexture( GL_TEXTURE0_ARB, textNum );
	CHECK_GL_ERRORS();

	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	CHECK_GL_ERRORS();

// forestw: we can't actually restore to the old context in this case, this code is also called from CUITextLayout::DrawRun and that function may rely on being left with a valid context
	return textNum;
}

//-----------------------------------------------------------------------------
// Purpose: Create a texture
//-----------------------------------------------------------------------------
void GetInternalOGLTextureFormat( E2DTextureFormat eFormat, EAlphaChannelType eAlphaChannelType, GLuint *pTextureFormat, GLuint *pInternalTextureFormat )
{	
	if ( pTextureFormat || pInternalTextureFormat )
	{
		GLuint format = GL_RGBA;
		GLuint internalFormat = s_bSRGB ? GL_SRGB8_ALPHA8_EXT : GL_RGBA;
		switch ( eFormat )
		{
			case k_EFormatRGBA8:
				if ( eAlphaChannelType == k_EAlphaChannelType_None )
					format = GL_RGB;
				else if ( eAlphaChannelType == k_EAlphaChannelType_Normal )
					format = GL_RGBA;
				else
				{
					Assert( eAlphaChannelType == k_EAlphaChannelType_PreMultiplied );
					format = GL_RGBA;
				}
				
				break;
			case k_EFormatBGRA8:
				format = GL_BGRA_EXT;
				break;
			case k_EFormatBGR8:
				format = GL_BGRA_EXT; // BGR8 data is actually 4 byte, but ignore the alpha channel
				break;
				
			case k_EFormatA8:
				format = GL_ALPHA;
				internalFormat = GL_ALPHA8;
				break;
				
			case k_EFormatYUV420:
#if defined(OSX) // GL_R8 not defined (part of GL_ARB_texture_rg)
				format = GL_RED;
				internalFormat = GL_R8;
#else
				format = GL_RED;
				internalFormat = GL_RGBA;
#endif
				break;

			case k_EFormatR16G16B16A16:
				format = GL_RGBA;
				internalFormat = GL_RGBA16;
				break;
				
			default:
				AssertMsg( false, "Unsupported format" );
				break;
		}

		if ( pTextureFormat )
			*pTextureFormat = format;
		if ( pInternalTextureFormat ) 
			*pInternalTextureFormat = internalFormat;
	}
}


int GetNumBytesForFormat( GLuint format )
{
	switch( format )
	{
		case GL_SRGB8_ALPHA8_EXT:
		case GL_RGBA:
			return 4;
		case GL_ALPHA8:
#if defined(OSX) // GL_R8 not defined (part of GL_ARB_texture_rg)
		case GL_R8:
#endif
			return 1;
		case GL_RGBA16:
			return 8;
		default:
			AssertMsg1( false, "need size for this OGL format: %d\n", format );
			return 4;
	}
}


//-----------------------------------------------------------------------------
// Purpose: constructor, owns an ogl texture
//-----------------------------------------------------------------------------
COpenGLTexture::COpenGLTexture( uint32 unTextureID, void *pubTextureData, uint32 unWidth, uint32 unHeight, uint32 unStride, E2DTextureFormat eFormat, EAlphaChannelType eAlphaChannelType )
{
	Assert(pubTextureData != NULL);
    m_unStride = unStride;
	m_unTextureID = unTextureID;
	m_unWidth = unWidth;
	m_unHeight = unHeight;
	m_eFormat = eFormat;
	m_eAlphaChannelType = eAlphaChannelType;
	m_bTextureUploaded = false;
    
	GLuint format = GL_RGBA;
	GLuint internalFormat = s_bSRGB ? GL_SRGB8_ALPHA8_EXT : GL_RGBA;
	GetInternalOGLTextureFormat( m_eFormat, m_eAlphaChannelType, &format, &internalFormat );
	
	m_oglTextureFormat = format;
	m_oglInternalTextureFormat = internalFormat;

	m_nOGLTextureID = 0xffffffff;

	//
	// Copy the data to our internal buffer so it can be restored
	// after a suspend/resume
	// 
	uint cbSize = unWidth * unHeight * GetNumBytesForFormat(m_oglInternalTextureFormat);
	m_pubTextureData = malloc( cbSize );
	if ( m_pubTextureData != NULL )
	{
		V_memcpy(m_pubTextureData, pubTextureData, cbSize);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Returns the type to pass to glTexImage2D for this texture format
//-----------------------------------------------------------------------------
GLenum COpenGLTexture::GetOGLTextureType()
{
	if( m_eFormat == k_EFormatR16G16B16A16 )
		return GL_UNSIGNED_SHORT;
	else
		return GL_UNSIGNED_BYTE;
}


//-----------------------------------------------------------------------------
// Purpose: Called to indicate that the cached texture data is
// no longer required (has been uploaded to the GPU without
// using the PBO buffer) and can be freed.
//-----------------------------------------------------------------------------
void COpenGLTexture::FreeTextureData() 
{ 
	Assert(m_pubTextureData != NULL);
	free(m_pubTextureData);
	m_pubTextureData = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: make the GL texture ID if we haven't already
//-----------------------------------------------------------------------------
void COpenGLTexture::CreateOGLTextureIDIfNeeded( COpenGLSurface *pSurface )
{
	if ( m_nOGLTextureID == 0xffffffff )
	{		
		GL_CHECK_CURRENT_CONTEXT;
		
		GLuint textNum = pSurface->CreateOpenGLTextureId();
		
		Assert( glIsTexture( textNum ) == GL_TRUE );
		
		m_nOGLTextureID = textNum;
	}
}


//-----------------------------------------------------------------------------
// Purpose: destructor
//-----------------------------------------------------------------------------
COpenGLTexture::~COpenGLTexture()
{
	if ( glIsTexture( m_nOGLTextureID ) )
	{
		glDeleteTextures( 1, ( GLuint * )&m_nOGLTextureID );
	}
	if ( m_pubTextureData != NULL )
	{
		free( m_pubTextureData );
	}
}


//-----------------------------------------------------------------------------
// Purpose: a double buffered texture
//-----------------------------------------------------------------------------
COpenGLDoubleBufferedTexture::COpenGLDoubleBufferedTexture( uint32 unTextureID, uint32 unTextureWidth, uint32 unTextureHeight, uint32 unStride, E2DTextureFormat eFormat, EAlphaChannelType eAlphaChannelType, bool bSerializedUploads )
{
	VPROF_BUDGET( "COpenGLDoubleBufferedTexture - constructor", VPROF_BUDGETGROUP_TENFOOT );
	m_unStride = unStride;
	m_unTextureID = unTextureID;
	m_unWidth = unTextureWidth;
	m_unHeight = unTextureHeight;
	m_eFormat = eFormat;
	m_eAlphaChannelType = eAlphaChannelType;
	m_bTextureUploaded = false;
	m_bSerializedUploads = bSerializedUploads;

	m_iCurRenderTexture = -1;
	m_iPendingUpload = -1;
	m_nSerial = 1; // 0 means ignore serial numbers so seed from 1
	m_nDrawSerial = 0;
	m_bCreatedOGLTextures = false;

	GLuint format = GL_RGBA;
	GLuint internalFormat = s_bSRGB ? GL_SRGB8_ALPHA8_EXT : GL_RGBA;
	GetInternalOGLTextureFormat( m_eFormat, m_eAlphaChannelType, &format, &internalFormat );
	
	m_oglTextureFormat = format;
	m_oglInternalTextureFormat = internalFormat;
}


//-----------------------------------------------------------------------------
// Purpose: make the OGL textures if pending
//-----------------------------------------------------------------------------
void COpenGLDoubleBufferedTexture::CreateOGLTextureIDIfNeeded( COpenGLSurface *pSurface )
{
	if ( !m_bCreatedOGLTextures )
	{
		GLuint textNum = pSurface->CreateOpenGLTextureId();
		Assert( glIsTexture( textNum ) == GL_TRUE );
		
		m_nOGLTextureID = textNum;

		for( int i=0; i < V_ARRAYSIZE( m_rgTextures ); ++i )
		{
			m_rgTextures[i].m_nSerial = -1;
			m_rgTextures[i].m_bTextureUploadPending = false;
			
			// make the PBO's for the data too	
			glGenBuffers( 1, &m_rgTextures[i].m_nOGLPBOID );
			
			glBindBuffer( GL_PIXEL_UNPACK_BUFFER, m_rgTextures[i].m_nOGLPBOID );
			int iBufferSize = m_unHeight * m_unStride * GetNumBytesForFormat( m_oglInternalTextureFormat );
			glBufferData( GL_PIXEL_UNPACK_BUFFER, iBufferSize, NULL, GL_DYNAMIC_DRAW );
			m_rgTextures[i].m_pTextureData = glMapBuffer( GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY );
			CHECK_GL_ERRORS();
			glBindBuffer( GL_PIXEL_UNPACK_BUFFER, 0 );
		}
		
		m_bCreatedOGLTextures = true;
		m_bTextureUploaded = true;
	}
}


//-----------------------------------------------------------------------------
// Purpose: destructor
//-----------------------------------------------------------------------------
COpenGLDoubleBufferedTexture::~COpenGLDoubleBufferedTexture()
{
	{
		AUTO_LOCK( m_IndexLock );
		m_iCurRenderTexture = -1;
	}
	
	glDeleteTextures( 1, &m_nOGLTextureID );

	for( int i=0; i < V_ARRAYSIZE( m_rgTextures ); ++i )
	{
		AUTO_LOCK( m_rgTextures[i].m_Lock );
		glDeleteBuffers( 1, &m_rgTextures[i].m_nOGLPBOID );
	}
}


//-----------------------------------------------------------------------------
// Purpose: upload any pending PBO on the render thread
//-----------------------------------------------------------------------------
void COpenGLDoubleBufferedTexture::UploadOGLTextureIfNeeded( COpenGLSurface *pSurface )
{
	if ( m_rgTextures[m_iCurRenderTexture].m_bTextureUploadPending )
	{
		glBindBuffer( GL_PIXEL_UNPACK_BUFFER, m_rgTextures[m_iCurRenderTexture].m_nOGLPBOID );
		glUnmapBuffer( GL_PIXEL_UNPACK_BUFFER );

		Assert( m_nOGLTextureID );
		Assert( GetTextureWidth() > 0 );
		Assert( GetTextureHeight() > 0 );

		GL_CHECK_CURRENT_CONTEXT;
		CHECK_GL_ERRORS();
		pSurface->SetTexture( GL_TEXTURE0_ARB, m_nOGLTextureID );
		CHECK_GL_ERRORS();
		
		glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
		glPixelStorei( GL_UNPACK_ROW_LENGTH, m_unStride );
		
		glTexImage2D( GL_TEXTURE_2D, 0, m_oglInternalTextureFormat, GetTextureWidth(), GetTextureHeight(), 0, m_oglTextureFormat , GL_UNSIGNED_BYTE, 0 );
		CHECK_GL_ERRORS();
		
		glBufferData( GL_PIXEL_UNPACK_BUFFER, m_unHeight * m_unStride * GetNumBytesForFormat( m_oglInternalTextureFormat ), NULL, GL_DYNAMIC_DRAW );
		m_rgTextures[m_iCurRenderTexture].m_pTextureData = glMapBuffer( GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY );

		// forestw: restore unpack row length because it can break other glTexImage2D code using this context
		glPixelStorei( GL_UNPACK_ROW_LENGTH, 0 );
		
		CHECK_GL_ERRORS();
		glBindBuffer( GL_PIXEL_UNPACK_BUFFER, 0 );
	
		m_rgTextures[m_iCurRenderTexture].m_bTextureUploadPending = false;
	}
}


//-----------------------------------------------------------------------------
// Purpose: upload this texture data using the context for this thread, OGL handles not swapping in the new texture data till its ready
//-----------------------------------------------------------------------------
int32 COpenGLDoubleBufferedTexture::UpdateTextureData( void *pBuffer )
{
	VPROF_BUDGET( "COpenGLDoubleBufferedTexture::UpdateTextureData", VPROF_BUDGETGROUP_TENFOOT );
	if ( !m_bCreatedOGLTextures )
		return -1;
	
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
			VPROF_BUDGET( "COpenGLDoubleBufferedTexture::UpdateTextureData - wait for free upload slot", VPROF_BUDGETGROUP_TENFOOT );
			m_IndexLock.Unlock();
			m_DrawEvent.Wait();
			m_IndexLock.Lock();
		}
		
		iUpdateIndex = ( m_iPendingUpload + 1 ) % V_ARRAYSIZE(m_rgTextures);
	}
	
	m_iPendingUpload++;
	
	{
		VPROF_BUDGET( "COpenGLDoubleBufferedTexture::UpdateTextureData - lock/sleep for double buffering", VPROF_BUDGETGROUP_TENFOOT );
		m_rgTextures[iUpdateIndex].m_Lock.Lock();
	}
	
	m_IndexLock.Unlock();
	
	V_memcpy( m_rgTextures[iUpdateIndex].m_pTextureData, pBuffer, m_unHeight * m_unStride * GetNumBytesForFormat( m_oglInternalTextureFormat ) );
	m_rgTextures[iUpdateIndex].m_bTextureUploadPending = true;
	
	m_rgTextures[iUpdateIndex].m_Lock.Unlock();
	
	VPROF_BUDGET( "COpenGLDoubleBufferedTexture -- indexlock", VPROF_BUDGETGROUP_TENFOOT );
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
int COpenGLDoubleBufferedTexture::LockAndGetCurrentTexture( GLint &glTextNum, int nSerial, COpenGLSurface *pSurface )
{
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
	
	COpenGLDoubleBufferedTexture::Texture_t &textureData = m_rgTextures[iIndex];
	
	m_nDrawSerial = textureData.m_nSerial;
	m_DrawEvent.Set(); // pulse the update thread as we have moved on
	
	// We return still holding this lock! Unlock call will unlock it.
	textureData.m_Lock.Lock();

	UploadOGLTextureIfNeeded( pSurface );

	m_IndexLock.Unlock();
	
	Assert( !m_bSerializedUploads || nSerial == textureData.m_nSerial );
	//glTextNum = textureData.m_nOGLTextureID;
	
	glTextNum = m_nOGLTextureID;
	return iIndex;
}


//-----------------------------------------------------------------------------
// Purpose: Return texture data, locking it such that it won't be modified underneath caller (which must be render thread)
//-----------------------------------------------------------------------------
void COpenGLDoubleBufferedTexture::Unlock( int iLockHandle )
{
	if ( iLockHandle != -1 )
	{
		m_rgTextures[iLockHandle].m_Lock.Unlock();
	}
}


//-----------------------------------------------------------------------------
// Purpose: a YUV double buffered texture
//-----------------------------------------------------------------------------
COpenGLDoubleBufferedYUV420Texture::COpenGLDoubleBufferedYUV420Texture( COpenGLSurface *pSurface, uint32 unTextureID, uint32 unTextureWidth, uint32 unTextureHeight )
{
	m_nSerial = 0;
	m_unTextureID = unTextureID;
	m_unWidth = unTextureWidth;
	m_unHeight = unTextureHeight;
	m_unTextureStride = m_unWidth;
	m_bTextureUploaded = false;
	m_flLastRenderThreadFrameTimeOnUpdate = -1.0f;
	m_pSurface = pSurface;
	
	GLuint format = GL_RGBA;
	GLuint internalFormat = s_bSRGB ? GL_SRGB8_ALPHA8_EXT : GL_RGBA;
	GetInternalOGLTextureFormat( k_EFormatYUV420, k_EAlphaChannelType_None, &format, &internalFormat );
	
	m_oglTextureFormat = format;
	m_oglInternalTextureFormat = internalFormat;
	
	m_bTextureUploadPending = false;
	m_bOGLInitialized = false;

	V_memset( m_Textures, 0x0, sizeof(m_Textures) );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
COpenGLDoubleBufferedYUV420Texture::~COpenGLDoubleBufferedYUV420Texture()
{
	AUTO_LOCK( m_BufferLock );
	
	if ( m_bOGLInitialized )
	{
		glBindBuffer( GL_PIXEL_UNPACK_BUFFER, 0 );

		for ( int i = 0; i < V_ARRAYSIZE(m_Textures); i++ )
		{
			glDeleteBuffers( 1, &m_Textures[i].m_nOGLPBOID );
			glDeleteTextures( 1, &m_Textures[i].m_nOGLTextureID );
		}

		m_bOGLInitialized = false;
	}
	else
	{
		for( int i = 0; i < V_ARRAYSIZE( m_Textures ); i++ )
		{
			if ( !m_Textures[i].m_pTextureData )
				continue;

			free( m_Textures[i].m_pTextureData );
			m_Textures[i].m_pTextureData = NULL;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: create pending texture id's and PBO's
//-----------------------------------------------------------------------------
void COpenGLDoubleBufferedYUV420Texture::CreateOGLTextureIDIfNeeded( COpenGLSurface *pSurface )
{
	AUTO_LOCK( m_BufferLock );
	VPROF_BUDGET( "COpenGLDoubleBufferedYUV420Texture::CreateOGLTextureIDIfNeeded", VPROF_BUDGETGROUP_TENFOOT );
	if ( !m_bOGLInitialized )
	{
		for ( int i = 0; i < V_ARRAYSIZE(m_Textures); i++ )
		{
			// make the texture id
			m_Textures[i].m_nOGLTextureID = pSurface->CreateOpenGLTextureId();
			Assert( glIsTexture( m_Textures[i].m_nOGLTextureID ) == GL_TRUE );
		
			// make the PBO's for the data too	
			glGenBuffers( 1, &m_Textures[i].m_nOGLPBOID );
			
			glBindBuffer( GL_PIXEL_UNPACK_BUFFER, m_Textures[i].m_nOGLPBOID );

			uint unStride = (i == 0) ? m_unTextureStride : GetStrideUV();
			uint unHeight = (i == 0) ? m_unHeight : (m_unHeight / 2);
			int iBufferSize = m_unHeight * unStride * GetNumBytesForFormat( m_oglInternalTextureFormat );
			void *pInitialBuffer = m_Textures[i].m_pTextureData;

			glBufferData( GL_PIXEL_UNPACK_BUFFER, iBufferSize, NULL, GL_DYNAMIC_DRAW );
			m_Textures[i].m_pTextureData = glMapBuffer( GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY );
			CHECK_GL_ERRORS();
			glBindBuffer( GL_PIXEL_UNPACK_BUFFER, 0 );

			// if update texture has already been called, copy off data from temporary buffer
			if ( pInitialBuffer )
			{
				V_memcpy( (byte*)m_Textures[i].m_pTextureData, pInitialBuffer, unStride * unHeight );
				free( pInitialBuffer );
				pInitialBuffer = NULL;
				m_bTextureUploadPending = true;
			}
		}
		
		m_bOGLInitialized = true;
	}
}


//-----------------------------------------------------------------------------
// Purpose: a YUV double buffered texture
//-----------------------------------------------------------------------------
bool COpenGLDoubleBufferedYUV420Texture::BUpdateTextureData( void *pYBuffer, void *pUBuffer, void *pVBuffer, uint unStrideY, uint unStrideU, uint unStrideV )
{
	{
		VPROF_BUDGET( "COpenGLDoubleBufferedYUV420Texture::UpdateTextureData - waiting on rendering", VPROF_BUDGETGROUP_TENFOOT );
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
	AUTO_LOCK( m_BufferLock );
	VPROF_BUDGET( "COpenGLDoubleBufferedYUV420Texture::UpdateTextureData", VPROF_BUDGETGROUP_TENFOOT );

	m_flLastRenderThreadFrameTimeOnUpdate = m_pSurface->GetCurrentRenderThreadFrameTime();

	uint32 unHalfHeight = m_unHeight/2;
	uint32 unHalfWidth = m_unWidth/2;
	uint32 unTextStrideUV = GetStrideUV();
	if ( !m_bOGLInitialized )
	{
		// opengl texture hasn't been created yet so allocate temporary textures for data
		if ( !m_Textures[0].m_pTextureData )
			m_Textures[0].m_pTextureData = malloc( m_unTextureStride * m_unHeight );
			
		if ( !m_Textures[1].m_pTextureData )
			m_Textures[1].m_pTextureData = malloc( unTextStrideUV * unHalfHeight );
			
		if ( !m_Textures[2].m_pTextureData )
			m_Textures[2].m_pTextureData = malloc( unTextStrideUV * unHalfHeight );
	}

	for( uint32 i=0; i < m_unHeight; ++i )
	{
		V_memcpy( (byte*)m_Textures[0].m_pTextureData + ( i * m_unTextureStride ), (byte*)pYBuffer + ( i * unStrideY ), m_unWidth );
	}

	for( uint32 i=0; i < unHalfHeight; ++i )
	{
		V_memcpy( (byte*)m_Textures[1].m_pTextureData + ( i * unTextStrideUV ), (byte*)pUBuffer + ( i * unStrideU ), unHalfWidth );
		V_memcpy( (byte*)m_Textures[2].m_pTextureData + ( i * unTextStrideUV ), (byte*)pVBuffer + ( i * unStrideV ), unHalfWidth );
	}
	
	m_bTextureUploadPending = true;
	}
	
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: a YUV double buffered texture
//-----------------------------------------------------------------------------
void COpenGLDoubleBufferedYUV420Texture::UploadOGLTextureIfNeeded( COpenGLSurface *pSurface )
{
	AUTO_LOCK( m_BufferLock );
	VPROF_BUDGET( "COpenGLDoubleBufferedYUV420Texture::UploadOGLTextureIfNeeded", VPROF_BUDGETGROUP_TENFOOT );

	if ( m_bTextureUploadPending )
	{
		glBindBuffer( GL_PIXEL_UNPACK_BUFFER, m_Textures[0].m_nOGLPBOID );

		glUnmapBuffer( GL_PIXEL_UNPACK_BUFFER );

		Assert( m_Textures[0].m_nOGLTextureID );
		Assert( m_Textures[1].m_nOGLTextureID );
		Assert( m_Textures[2].m_nOGLTextureID );
		Assert( GetTextureWidth() > 0 );
		Assert( GetTextureHeight() > 0 );

		GL_CHECK_CURRENT_CONTEXT;
		CHECK_GL_ERRORS();
		glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
		glPixelStorei( GL_UNPACK_ROW_LENGTH, m_unTextureStride );
		
		pSurface->SetTexture( GL_TEXTURE0_ARB, m_Textures[0].m_nOGLTextureID );
		glTexImage2D( GL_TEXTURE_2D, 0, m_oglInternalTextureFormat, GetTextureWidth(), GetTextureHeight(), 0, m_oglTextureFormat , GL_UNSIGNED_BYTE, 0 );
		CHECK_GL_ERRORS();

		int iBufferSize = m_unHeight * m_unTextureStride * GetNumBytesForFormat( m_oglInternalTextureFormat );
		glBufferData( GL_PIXEL_UNPACK_BUFFER, iBufferSize, NULL, GL_DYNAMIC_DRAW );

		m_Textures[0].m_pTextureData = glMapBuffer( GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY );

		// U and V are half sized to the Y buffer
		
		glPixelStorei( GL_UNPACK_ROW_LENGTH, GetStrideUV() );

		glBindBuffer( GL_PIXEL_UNPACK_BUFFER, m_Textures[1].m_nOGLPBOID );
		glUnmapBuffer( GL_PIXEL_UNPACK_BUFFER );

		pSurface->SetTexture( GL_TEXTURE0_ARB, m_Textures[1].m_nOGLTextureID );
		glTexImage2D( GL_TEXTURE_2D, 0, m_oglInternalTextureFormat, GetTextureWidth() / 2, GetTextureHeight() / 2, 0, m_oglTextureFormat , GL_UNSIGNED_BYTE, 0 );
		CHECK_GL_ERRORS();

		iBufferSize = m_unHeight * GetStrideUV() * GetNumBytesForFormat( m_oglInternalTextureFormat );
		glBufferData( GL_PIXEL_UNPACK_BUFFER, iBufferSize, NULL, GL_DYNAMIC_DRAW );
		m_Textures[1].m_pTextureData = glMapBuffer( GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY );

		glBindBuffer( GL_PIXEL_UNPACK_BUFFER, m_Textures[2].m_nOGLPBOID );
		glUnmapBuffer( GL_PIXEL_UNPACK_BUFFER );

		pSurface->SetTexture( GL_TEXTURE0_ARB, m_Textures[2].m_nOGLTextureID );
		glTexImage2D( GL_TEXTURE_2D, 0, m_oglInternalTextureFormat, GetTextureWidth() / 2, GetTextureHeight() / 2, 0, m_oglTextureFormat , GL_UNSIGNED_BYTE, 0 );

		glBufferData( GL_PIXEL_UNPACK_BUFFER, iBufferSize, NULL, GL_DYNAMIC_DRAW );
		m_Textures[2].m_pTextureData = glMapBuffer( GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY );

		// forestw: restore unpack row length because it can break other glTexImage2D code using this context
		glPixelStorei( GL_UNPACK_ROW_LENGTH, 0 );

		CHECK_GL_ERRORS();
		
		glBindBuffer( GL_PIXEL_UNPACK_BUFFER, 0 );

		m_bTextureUploaded = true;
		m_bTextureUploadPending = false;
		
		m_nSerial++;
	}
}


//-----------------------------------------------------------------------------
// Purpose: delete an allocated texture, must be thread safe
//-----------------------------------------------------------------------------
bool COpenGLSurface::BDeleteTexture( IUITexture *pTexture )
{
	//Msg( "BDeleteTexture %i\n", pTexture->GetTextureID() );
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
// Purpose: helper to load rgba data into a ogl texture
//-----------------------------------------------------------------------------
bool COpenGLSurface::BCreateTexture( IUITexture **pTextureOutput, void *pubTextureData, uint32 unWidth, uint32 unHeight, uint32 unStride, E2DTextureFormat eFormat, EAlphaChannelType eAlphaChannelType )
{
	COpenGLTexture *pTexture = new COpenGLTexture( ++s_unNextTextureID, pubTextureData, unWidth, unHeight, unStride, eFormat, eAlphaChannelType );
	//Msg( "BCreateTexture = %i\n", pTexture->GetTextureID() );
	
	if ( pubTextureData )
	{
		uint64 cbPBOOffset = 0;
		uint cbWrite = unWidth*unHeight*GetNumBytesForFormat(pTexture->GetOGLInternalTextureFormat());
		// lock and swap PBO's if uploads are pending
		m_mutexTexturePBO.Lock();
		cbPBOOffset = m_cbTextureWriteOffset;
		
		if ( m_pbTextureUploads && cbPBOOffset+cbWrite < k_cbPBOTextureBuffer )
		{
			m_cbTextureWriteOffset += cbWrite;
			m_vecPendingTextureUploads.AddToTail( PendingTextureUpload_t( cbPBOOffset, pTexture ) );
		
			V_memcpy( (void *)((const char *)m_pbTextureUploads + cbPBOOffset), pubTextureData, cbWrite );
		
			m_mutexTexturePBO.Unlock();
		}
		else
		{
			m_vecPendingTextureUploads.AddToTail( PendingTextureUpload_t( pTexture ) );
			m_mutexTexturePBO.Unlock();
		}
	}
												 
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
// Purpose: return a double buffered texture
//-----------------------------------------------------------------------------
bool COpenGLSurface::BCreateDoubleBufferedTexture( IUIDoubleBufferedTexture **pDoubleBufferedOutput, uint32 unWidth, uint32 unHeight, uint32 unStride, E2DTextureFormat eFormat, EAlphaChannelType eAlphaChannelType, bool bSerialized ) 
{
	COpenGLDoubleBufferedTexture *pTexture = new COpenGLDoubleBufferedTexture( ++s_unNextTextureID, unWidth, unHeight, unStride, eFormat, eAlphaChannelType, bSerialized );
	if ( pTexture )
	{
		{
			AUTO_LOCK( m_lockTextureMap );
			m_mapTextures.Insert( pTexture->GetTextureID(), pTexture );
			m_vecPendingDoubleBufferCreates.AddToTail( pTexture );
		}
		*pDoubleBufferedOutput = pTexture;
		return true;
	}
    return false;
}


//-----------------------------------------------------------------------------
// Purpose: a texture with yuv support
//-----------------------------------------------------------------------------
bool COpenGLSurface::BCreateDoubleBufferedYUV420Texture( IUIDoubleBufferedYUV420Texture **pDoubleBufferedYUV420Output, uint32 unWidth, uint32 unHeight )
{
	COpenGLDoubleBufferedYUV420Texture *pTexture = new COpenGLDoubleBufferedYUV420Texture( this, ++s_unNextTextureID, unWidth, unHeight );
	if ( pTexture )
	{
		{
			AUTO_LOCK( m_lockTextureMap );
			m_mapTextures.Insert( pTexture->GetTextureID(), pTexture );
			m_vecPendingYUVCreates.AddToTail( pTexture );
		}
		*pDoubleBufferedYUV420Output = pTexture;
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Draw a textured quad
//-----------------------------------------------------------------------------
void COpenGLSurface::DrawTexturedQuadInternal( const CLazyShaderProgram &program, uint32 nTextureID, VertexTextured_t *points, float flScale2DX, float flScale2DY, float flRotate2D, float flTextureWidth, float flTextureHeight )
{
	VPROF_BUDGET( "COpenGLSurface::DrawTexturedQuadInternal", VPROF_BUDGETGROUP_TENFOOT );
//	Msg( "DrawTexturedQuadInternal %f,%f to %f,%f\n", points[0].x, points[0].y, points[2].x, points[2].y );

	if ( nTextureID )
		DebugDumpTexture( nTextureID );
		GL_CHECK_CURRENT_CONTEXT;
	CHECK_GL_ERRORS();
	if (program.m_textureLoc >= 0)
		glUniform1i( program.m_textureLoc, 0 );
	if (program.m_texture1Loc >= 0)
		glUniform1i( program.m_texture1Loc, 1 );
	if (program.m_texture2Loc >= 0)
		glUniform1i( program.m_texture2Loc, 2 );

	SetTexture( GL_TEXTURE0_ARB, nTextureID );

#ifndef DEBUGNODRAW
	glBegin( GL_QUADS );
	
	float flWidth = ( ( points[1].x - points[0].x ) + ( points[2].x - points[3].x ) ) / 2.0f;
	float flHeight = ( ( points[3].y - points[0].y ) + ( points[2].y - points[1].y ) ) / 2.0f;
	if ( flTextureWidth > 0.0f )
		flWidth = flTextureWidth;
	if ( flTextureHeight > 0.0f )
		flHeight = flTextureHeight;
	float flXOffset = ( flWidth - ( flScale2DX * flWidth ) ) / 2.0f;
	float flYOffset = ( flHeight - ( flScale2DY * flHeight ) ) / 2.0f;

	points[0].x += flXOffset;
	points[1].x -= flXOffset;
	points[2].x -= flXOffset;
	points[3].x += flXOffset;

	points[0].y += flYOffset;
	points[1].y += flYOffset;
	points[2].y -= flYOffset;
	points[3].y -= flYOffset;

	if ( flRotate2D > 0.00001f || flRotate2D < -0.00001f )
	{
		float flSine;
		float flCosine;
		float flRadians = DEG2RAD( flRotate2D );
		SinCos( flRadians, &flSine, &flCosine );

		float flXTranslate = points[0].x + ((points[1].x - points[0].x )/2.0f);
		float flYTranslate = points[0].y + ((points[3].y - points[0].y )/2.0f);

		for( int iCorner=0; iCorner < 4; ++iCorner )
		{
			float x = ( points[iCorner].x - flXTranslate );
			float y = ( points[iCorner].y - flYTranslate );

			points[iCorner].x = x * flCosine - y * flSine;
			points[iCorner].y = y * flCosine + x * flSine;

			points[iCorner].x += flXTranslate;
			points[iCorner].y += flYTranslate;
		}
	}

	int i = 0;
	glColor4f( points[i].r, points[i].g, points[i].b, points[i].a );
	glMultiTexCoord2f( GL_TEXTURE0_ARB, points[i].u, points[i].v );
	glMultiTexCoord2f( GL_TEXTURE1_ARB, points[i].masku1, points[i].maskv1 );
	glMultiTexCoord2f( GL_TEXTURE2_ARB, points[i].masku2, points[i].maskv2 );
	glVertex3f( points[i].x, points[i].y, points[i].z );

	i++;
	glColor4f( points[i].r, points[i].g, points[i].b, points[i].a );
	glMultiTexCoord2f( GL_TEXTURE0_ARB, points[i].u, points[i].v );
	glMultiTexCoord2f( GL_TEXTURE1_ARB, points[i].masku1, points[i].maskv1 );
	glMultiTexCoord2f( GL_TEXTURE2_ARB, points[i].masku2, points[i].maskv2 );
	glVertex3f( points[i].x, points[i].y, points[i].z );

	i++;
	glColor4f( points[i].r, points[i].g, points[i].b, points[i].a );
	glMultiTexCoord2f( GL_TEXTURE0_ARB, points[i].u, points[i].v );
	glMultiTexCoord2f( GL_TEXTURE1_ARB, points[i].masku1, points[i].maskv1 );
	glMultiTexCoord2f( GL_TEXTURE2_ARB, points[i].masku2, points[i].maskv2 );
	glVertex3f( points[i].x, points[i].y, points[i].z );

	i++;
	glColor4f( points[i].r, points[i].g, points[i].b, points[i].a );
	glMultiTexCoord2f( GL_TEXTURE0_ARB, points[i].u, points[i].v );
	glMultiTexCoord2f( GL_TEXTURE1_ARB, points[i].masku1, points[i].maskv1 );
	glMultiTexCoord2f( GL_TEXTURE2_ARB, points[i].masku2, points[i].maskv2 );
	glVertex3f( points[i].x, points[i].y, points[i].z );

	glEnd();
#endif

	DebugDumpFramebuffer();
	DebugDumpState( __func__ );

	CHECK_GL_ERRORS();
}

void COpenGLSurface::UpdateFancyQuadGradientTexture( const FancyQuadBrush_t &brush )
{
	int iCurrentTexel;
	int iCurrentStop = 0;
	int iLastStopUpdated = -1;
	unsigned char pCurrentColor[4] = { 0, 0, 0, 0 };
	unsigned char pNextColor[4] = { 0, 0, 0, 0 };
	float flCurrentStopSpacing = 0.0f;
	float flCurrentStopProgression;
	// Our gradient space goes from 0.0 to 1.0, this is the position difference
	// between each texel in our gradient texture in gradient space.
	const float flGradientTexelScale = 1.0f / (FANCYQUAD_GRADIENT_TEXTURE_SIZE - 1);
	
	float flCurrentGradientPosition;
	
	Assert( brush.m_nGradientStops <= FANCYQUAD_MAXSTOPS );
	
	iCurrentTexel = 0;
	
	// Iterate over each texel in our gradient texture and compute its color in
	// gradient space.
	while ( iCurrentTexel < FANCYQUAD_GRADIENT_TEXTURE_SIZE )
	{
		// How far this texel is into gradient space.
		flCurrentGradientPosition = iCurrentTexel * flGradientTexelScale;
		
		// Skip ahead if we passed stop(s).
		while ( brush.m_flGradientStops[iCurrentStop + 1] < flCurrentGradientPosition )
		{
			iCurrentStop++;
		}
		
		// Last stop ought to be 1.0, so we can't have passed it.
		Assert( iCurrentStop < brush.m_nGradientStops );
		
		// If we're between two stops we've never seen, stash their colors for
		// easy interpolation later.
		if ( iLastStopUpdated != iCurrentStop )
		{
			for ( int c = 0; c < 4; c++ )
			{
				pCurrentColor[c] = brush.m_flColor[iCurrentStop][c] * 255;
				pNextColor[c] = brush.m_flColor[iCurrentStop + 1][c] * 255;
			}

			flCurrentStopSpacing = 	brush.m_flGradientStops[iCurrentStop + 1] -
									brush.m_flGradientStops[iCurrentStop];

			iLastStopUpdated = iCurrentStop;
		}

		Assert( flCurrentGradientPosition >= brush.m_flGradientStops[iCurrentStop] );
		Assert( flCurrentGradientPosition <= brush.m_flGradientStops[iCurrentStop + 1] );

		// Now, figure out where we stand between our two bounding stops.
		flCurrentStopProgression = 	flCurrentGradientPosition -
									brush.m_flGradientStops[iCurrentStop];
									
		// Normalize into current stop scale.
		flCurrentStopProgression /= flCurrentStopSpacing;
		
		// Interpolate each component between the two colors according to our
		// local progress in the current stop.
		for ( int c = 0; c < 4; c++ )
		{
			m_GradientTextureBuffer[iCurrentTexel][c] =
				pCurrentColor[c] * (1.0f - flCurrentStopProgression) +
				pNextColor[c] * flCurrentStopProgression;
		}
		
		iCurrentTexel++;
	}
}


void COpenGLSurface::SetTexture( GLint unit, GLint textureid )
{
	if ( textureid == 0 )
		return;

	if ( m_LastTextureID[unit-GL_TEXTURE0_ARB] != textureid )
	{
		VPROF_BUDGET( "COpenGLSurface::SetTexture - actual change", VPROF_BUDGETGROUP_TENFOOT );
		if ( unit != GL_TEXTURE0_ARB )
			glActiveTexture( unit );

		glBindTexture( GL_TEXTURE_2D, textureid );
		m_LastTextureID[unit-GL_TEXTURE0_ARB] = textureid;

		if ( unit != GL_TEXTURE0_ARB )
			glActiveTexture( GL_TEXTURE0_ARB );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Draw a fancy quad (gradients, corners), optionally with texture
//-----------------------------------------------------------------------------
void COpenGLSurface::DrawFancyQuad( uint32 nTexture0ID, uint32 nTexture1OpacityMask, uint32 nTexture2ID, float flSaturation, float flHueShift, float flBrightness, float flContrast, float flOpacityMaskOpacity, int nWide, int nTall, const FancyQuadParameters_t &p, const FancyQuadBrush_t &brush, float flScale2DX, float flScale2DY, float flRotate2D, float flTextureWidth, float flTextureHeight, bool texisnotpremul, bool isalphatexture, bool isyuvtexture, bool rawcoords, const float *flMatrix, bool bClipToLayer )
{
	int i, j;
	float flWidth = 1.0f, flHeight = 1.0f, flXOffset = 0.0f, flYOffset = 0.0f;
	FancyQuadVertex_t v[4];
	float flAdjustedVertexMin[2];
	float flAdjustedVertexMax[2];
	float flAdjustedTexCoordMin[2];
	float flAdjustedTexCoordMax[2];
	float flGradientMatrix[3][2];
	float flScaledInnerCornerRadii[4][2];
	float flScaledOuterCornerRadii[4][2];
	float flOuterCornerMatrix[4][3][2];
	float flInnerCornerMatrix[4][3][2];
	VPROF_BUDGET( "COpenGLSurface::DrawFancyQuad", VPROF_BUDGETGROUP_TENFOOT );

	Assert( m_stackCompositionLayers.Count() );
	if ( !m_stackCompositionLayers.Count() )
		return;
	CCompositionLayer *pLayer = m_stackCompositionLayers[ m_stackCompositionLayers.Count()-1 ];

	if ( nWide == -1 )
		nWide = pLayer->GetWidth();
	if ( nTall == -1 )
		nTall = pLayer->GetHeight();

	if ( !rawcoords )
	{
		flWidth = ( p.m_flVertexMax[0] - p.m_flVertexMin[0] );
		flHeight = ( p.m_flVertexMax[1] - p.m_flVertexMin[1] );
		if ( flTextureWidth > 0.0f )
			flWidth = flTextureWidth;
		if ( flTextureHeight > 0.0f )
			flHeight = flTextureHeight;
		flXOffset = ( flWidth - ( flScale2DX * flWidth ) ) / 2.0f;
		flYOffset = ( flHeight - ( flScale2DY * flHeight ) ) / 2.0f;
		
		i = 0;
		while (i < 4)
		{
			flScaledInnerCornerRadii[i][0] = p.m_flInnerCornerRadii[i][0] * flScale2DX;
			flScaledInnerCornerRadii[i][1] = p.m_flInnerCornerRadii[i][1] * flScale2DY;

			flScaledOuterCornerRadii[i][0] = p.m_flOuterCornerRadii[i][0] * flScale2DX;
			flScaledOuterCornerRadii[i][1] = p.m_flOuterCornerRadii[i][1] * flScale2DY;
			i++;
		}
	}
	else
	{
		i = 0;
		while (i < 4)
		{
			flScaledInnerCornerRadii[i][0] = p.m_flInnerCornerRadii[i][0];
			flScaledInnerCornerRadii[i][1] = p.m_flInnerCornerRadii[i][1];
			
			flScaledOuterCornerRadii[i][0] = p.m_flOuterCornerRadii[i][0];
			flScaledOuterCornerRadii[i][1] = p.m_flOuterCornerRadii[i][1];
			i++;
		}
	}
	
	flAdjustedVertexMin[0] = p.m_flVertexMin[0] + flXOffset;
	flAdjustedVertexMin[1] = p.m_flVertexMin[1] + flYOffset;
	flAdjustedVertexMax[0] = p.m_flVertexMax[0] - flXOffset;
	flAdjustedVertexMax[1] = p.m_flVertexMax[1] - flYOffset;
	flAdjustedTexCoordMin[0] = p.m_flTexCoordMin[0];
	flAdjustedTexCoordMin[1] = p.m_flTexCoordMin[1];
	flAdjustedTexCoordMax[0] = p.m_flTexCoordMax[0];
	flAdjustedTexCoordMax[1] = p.m_flTexCoordMax[1];
	
	float flOriginalWidth = fabs(flAdjustedVertexMax[0] - flAdjustedVertexMin[0]);
	float flOriginalHeight = fabs(flAdjustedVertexMax[1] - flAdjustedVertexMin[1]);
	float flUWidth = fabs(flAdjustedTexCoordMax[0] - flAdjustedTexCoordMin[0]);
	float flVWidth = fabs(flAdjustedTexCoordMax[1] - flAdjustedTexCoordMin[1]);
	
	
	if ( bClipToLayer )
	{
		RectBounds_t r;
		pLayer->GetCurrentClipRect( r );
		if ( r.left > flAdjustedVertexMin[0] )
		{
			flAdjustedTexCoordMin[0] = flAdjustedTexCoordMin[0] + ( ( r.left - flAdjustedVertexMin[0] ) / flOriginalWidth )*flUWidth;
			flAdjustedVertexMin[0] = r.left;
		}
		
		if ( flAdjustedVertexMax[0] > r.right )
		{
			flAdjustedTexCoordMax[0] = flAdjustedTexCoordMax[0] - ( ( flAdjustedVertexMax[0] - r.right ) / flOriginalWidth )*flUWidth;
			flAdjustedVertexMax[0] = r.right;
		}
		
		if ( r.top > flAdjustedVertexMin[1] )
		{
			flAdjustedTexCoordMin[1] = flAdjustedTexCoordMin[1] + ( ( r.top - flAdjustedVertexMin[1] ) / flOriginalHeight )*flVWidth;
			flAdjustedVertexMin[1] = r.top;
		}
		
		if ( flAdjustedVertexMax[1] > r.bottom )
		{
			flAdjustedTexCoordMax[1] = flAdjustedTexCoordMax[1] - ( ( flAdjustedVertexMax[1] - r.bottom ) / flOriginalHeight )*flVWidth;
			flAdjustedVertexMax[1] = r.bottom;
		}
	}
	
	if ( brush.m_bIsRadialGradient )
	{
		// radial gradient - we just need the inverse radius as a scaling factor around the start point
		flGradientMatrix[0][0] = 1.0f / brush.m_flGradientRadii[0];
		flGradientMatrix[0][1] = 0.0f;
		flGradientMatrix[1][0] = 0.0f;
		flGradientMatrix[1][1] = 1.0f / brush.m_flGradientRadii[1];
	}
	else if ( brush.m_bIsLinearGradient )
	{
		// linear gradient - we need a single vector along the desired direction, its length must be the inverse length, which is easily achieved with a divide by squared length ( 1 / sqrt without the sqrt )
		float flDelta[2], flInvDelta[2], flInvSqrLength;
		flDelta[0] = brush.m_flGradientEndPoint[0] - brush.m_flGradientStartPoint[0];
		flDelta[1] = brush.m_flGradientEndPoint[1] - brush.m_flGradientStartPoint[1];
		flInvSqrLength = 1.0f / ( flDelta[0] * flDelta[0] + flDelta[1] * flDelta[1] );
		flInvDelta[0] = flDelta[0] * flInvSqrLength;
		flInvDelta[1] = flDelta[1] * flInvSqrLength;
		flGradientMatrix[0][0] = flInvDelta[0];
		flGradientMatrix[0][1] = 0.0f;
		flGradientMatrix[1][0] = flInvDelta[1];
		flGradientMatrix[1][1] = 0.0f;
	}
	else
	{
		flGradientMatrix[0][0] = 0.0f;
		flGradientMatrix[0][1] = 0.0f;
		flGradientMatrix[1][0] = 0.0f;
		flGradientMatrix[1][1] = 0.0f;
	}
	flGradientMatrix[2][0] = -( brush.m_flGradientStartPoint[0] * flGradientMatrix[0][0] + brush.m_flGradientStartPoint[1] * flGradientMatrix[1][0] );
	flGradientMatrix[2][1] = -( brush.m_flGradientStartPoint[0] * flGradientMatrix[0][1] + brush.m_flGradientStartPoint[1] * flGradientMatrix[1][1] );

	// clear corner matrices, then populate them as needed
	// all corner matrices transform a screen coordinate into a 0-1 arc space representing a bottom-right corner of radius 1
	flOuterCornerMatrix[0][0][0] = 0.0f;flOuterCornerMatrix[0][0][1] = 0.0f;
	flOuterCornerMatrix[0][1][0] = 0.0f;flOuterCornerMatrix[0][1][1] = 0.0f;
	flOuterCornerMatrix[0][2][0] = 0.0f;flOuterCornerMatrix[0][2][1] = 0.0f;
	flOuterCornerMatrix[1][0][0] = 0.0f;flOuterCornerMatrix[1][0][1] = 0.0f;
	flOuterCornerMatrix[1][1][0] = 0.0f;flOuterCornerMatrix[1][1][1] = 0.0f;
	flOuterCornerMatrix[1][2][0] = 0.0f;flOuterCornerMatrix[1][2][1] = 0.0f;
	flOuterCornerMatrix[2][0][0] = 0.0f;flOuterCornerMatrix[2][0][1] = 0.0f;
	flOuterCornerMatrix[2][1][0] = 0.0f;flOuterCornerMatrix[2][1][1] = 0.0f;
	flOuterCornerMatrix[2][2][0] = 0.0f;flOuterCornerMatrix[2][2][1] = 0.0f;
	flOuterCornerMatrix[3][0][0] = 0.0f;flOuterCornerMatrix[3][0][1] = 0.0f;
	flOuterCornerMatrix[3][1][0] = 0.0f;flOuterCornerMatrix[3][1][1] = 0.0f;
	flOuterCornerMatrix[3][2][0] = 0.0f;flOuterCornerMatrix[3][2][1] = 0.0f;
	flInnerCornerMatrix[0][0][0] = 0.0f;flInnerCornerMatrix[0][0][1] = 0.0f;
	flInnerCornerMatrix[0][1][0] = 0.0f;flInnerCornerMatrix[0][1][1] = 0.0f;
	flInnerCornerMatrix[0][2][0] = 0.0f;flInnerCornerMatrix[0][2][1] = 0.0f;
	flInnerCornerMatrix[1][0][0] = 0.0f;flInnerCornerMatrix[1][0][1] = 0.0f;
	flInnerCornerMatrix[1][1][0] = 0.0f;flInnerCornerMatrix[1][1][1] = 0.0f;
	flInnerCornerMatrix[1][2][0] = 0.0f;flInnerCornerMatrix[1][2][1] = 0.0f;
	flInnerCornerMatrix[2][0][0] = 0.0f;flInnerCornerMatrix[2][0][1] = 0.0f;
	flInnerCornerMatrix[2][1][0] = 0.0f;flInnerCornerMatrix[2][1][1] = 0.0f;
	flInnerCornerMatrix[2][2][0] = 0.0f;flInnerCornerMatrix[2][2][1] = 0.0f;
	flInnerCornerMatrix[3][0][0] = 0.0f;flInnerCornerMatrix[3][0][1] = 0.0f;
	flInnerCornerMatrix[3][1][0] = 0.0f;flInnerCornerMatrix[3][1][1] = 0.0f;
	flInnerCornerMatrix[3][2][0] = 0.0f;flInnerCornerMatrix[3][2][1] = 0.0f;

	// outer corner rounding
	if ( flScaledOuterCornerRadii[0][0] * flScaledOuterCornerRadii[0][1] )
	{
		// top-left
		flOuterCornerMatrix[0][0][0] = -1.0f / flScaledOuterCornerRadii[0][0];
		flOuterCornerMatrix[0][0][1] = 0.0f;
		flOuterCornerMatrix[0][1][0] = 0.0f;
		flOuterCornerMatrix[0][1][1] = -1.0f / flScaledOuterCornerRadii[0][1];
		flOuterCornerMatrix[0][2][0] = -(flAdjustedVertexMin[0] + 1.0f) * flOuterCornerMatrix[0][0][0] + 1.0f;
		flOuterCornerMatrix[0][2][1] = -(flAdjustedVertexMin[1] + 1.0f) * flOuterCornerMatrix[0][1][1] + 1.0f;
	}
	if ( flScaledOuterCornerRadii[1][0] * flScaledOuterCornerRadii[1][1] )
	{
		// top-right
		flOuterCornerMatrix[1][0][0] = 1.0f / flScaledOuterCornerRadii[1][0];
		flOuterCornerMatrix[1][0][1] = 0.0f;
		flOuterCornerMatrix[1][1][0] = 0.0f;
		flOuterCornerMatrix[1][1][1] = -1.0f / flScaledOuterCornerRadii[1][1];
		flOuterCornerMatrix[1][2][0] = -(flAdjustedVertexMax[0] - 1.0f) * flOuterCornerMatrix[1][0][0] + 1.0f;
		flOuterCornerMatrix[1][2][1] = -(flAdjustedVertexMin[1] + 1.0f) * flOuterCornerMatrix[1][1][1] + 1.0f;
	}
	if ( flScaledOuterCornerRadii[2][0] * flScaledOuterCornerRadii[2][1] )
	{
		// bottom-right
		flOuterCornerMatrix[2][0][0] = 1.0f / flScaledOuterCornerRadii[2][0];
		flOuterCornerMatrix[2][0][1] = 0.0f;
		flOuterCornerMatrix[2][1][0] = 0.0f;
		flOuterCornerMatrix[2][1][1] = 1.0f / flScaledOuterCornerRadii[2][1];
		flOuterCornerMatrix[2][2][0] = -(flAdjustedVertexMax[0] - 1.0f) * flOuterCornerMatrix[2][0][0] + 1.0f;
		flOuterCornerMatrix[2][2][1] = -(flAdjustedVertexMax[1] - 1.0f) * flOuterCornerMatrix[2][1][1] + 1.0f;
	}
	if ( flScaledOuterCornerRadii[3][0] * flScaledOuterCornerRadii[3][1] )
	{
		// bottom-left
		flOuterCornerMatrix[3][0][0] = -1.0f / flScaledOuterCornerRadii[3][0];
		flOuterCornerMatrix[3][0][1] = 0.0f;
		flOuterCornerMatrix[3][1][0] = 0.0f;
		flOuterCornerMatrix[3][1][1] = 1.0f / flScaledOuterCornerRadii[3][1];
		flOuterCornerMatrix[3][2][0] = -(flAdjustedVertexMin[0] + 1.0f) * flOuterCornerMatrix[3][0][0] + 1.0f;
		flOuterCornerMatrix[3][2][1] = -(flAdjustedVertexMax[1] - 1.0f) * flOuterCornerMatrix[3][1][1] + 1.0f;
	}

	// inner corners are brought inward by the border width (earlier we enlarged the entire quad to include the border)
	if ( flScaledInnerCornerRadii[0][0] * flScaledInnerCornerRadii[0][1] )
	{
		// top-left
		flInnerCornerMatrix[0][0][0] = -1.0f / flScaledInnerCornerRadii[0][0];
		flInnerCornerMatrix[0][0][1] = 0.0f;
		flInnerCornerMatrix[0][1][0] = 0.0f;
		flInnerCornerMatrix[0][1][1] = -1.0f / flScaledInnerCornerRadii[0][1];
		flInnerCornerMatrix[0][2][0] = -(flAdjustedVertexMin[0] + p.m_flBorderWidth[3] + 1.0f) * flInnerCornerMatrix[0][0][0] + 1.0f;
		flInnerCornerMatrix[0][2][1] = -(flAdjustedVertexMin[1] + p.m_flBorderWidth[0] + 1.0f) * flInnerCornerMatrix[0][1][1] + 1.0f;
	}
	if ( flScaledInnerCornerRadii[1][0] * flScaledInnerCornerRadii[1][1] )
	{
		// top-right
		flInnerCornerMatrix[1][0][0] = 1.0f / flScaledInnerCornerRadii[1][0];
		flInnerCornerMatrix[1][0][1] = 0.0f;
		flInnerCornerMatrix[1][1][0] = 0.0f;
		flInnerCornerMatrix[1][1][1] = -1.0f / flScaledInnerCornerRadii[1][1];
		flInnerCornerMatrix[1][2][0] = -(flAdjustedVertexMax[0] - p.m_flBorderWidth[1] - 1.0f) * flInnerCornerMatrix[1][0][0] + 1.0f;
		flInnerCornerMatrix[1][2][1] = -(flAdjustedVertexMin[1] + p.m_flBorderWidth[0] + 1.0f) * flInnerCornerMatrix[1][1][1] + 1.0f;
	}
	if ( flScaledInnerCornerRadii[2][0] * flScaledInnerCornerRadii[2][1] )
	{
		// bottom-right
		flInnerCornerMatrix[2][0][0] = 1.0f / flScaledInnerCornerRadii[2][0];
		flInnerCornerMatrix[2][0][1] = 0.0f;
		flInnerCornerMatrix[2][1][0] = 0.0f;
		flInnerCornerMatrix[2][1][1] = 1.0f / flScaledInnerCornerRadii[2][1];
		flInnerCornerMatrix[2][2][0] = -(flAdjustedVertexMax[0] - p.m_flBorderWidth[1] - 1.0f) * flInnerCornerMatrix[2][0][0] + 1.0f;
		flInnerCornerMatrix[2][2][1] = -(flAdjustedVertexMax[1] - p.m_flBorderWidth[2] - 1.0f) * flInnerCornerMatrix[2][1][1] + 1.0f;
	}
	if ( flScaledInnerCornerRadii[3][0] * flScaledInnerCornerRadii[3][1] )
	{
		// bottom-left
		flInnerCornerMatrix[3][0][0] = -1.0f / flScaledInnerCornerRadii[3][0];
		flInnerCornerMatrix[3][0][1] = 0.0f;
		flInnerCornerMatrix[3][1][0] = 0.0f;
		flInnerCornerMatrix[3][1][1] = 1.0f / flScaledInnerCornerRadii[3][1];
		flInnerCornerMatrix[3][2][0] = -(flAdjustedVertexMin[0] + p.m_flBorderWidth[3] + 1.0f) * flInnerCornerMatrix[3][0][0] + 1.0f;
		flInnerCornerMatrix[3][2][1] = -(flAdjustedVertexMax[1] - p.m_flBorderWidth[2] - 1.0f) * flInnerCornerMatrix[3][1][1] + 1.0f;
	}

	// the gradient math needs the original positions
	float flOriginalPosition[4][2];
	flOriginalPosition[0][0] = p.m_flVertexMin[0];
	flOriginalPosition[0][1] = p.m_flVertexMin[1];
	flOriginalPosition[1][0] = p.m_flVertexMax[0];
	flOriginalPosition[1][1] = p.m_flVertexMin[1];
	flOriginalPosition[2][0] = p.m_flVertexMax[0];
	flOriginalPosition[2][1] = p.m_flVertexMax[1];
	flOriginalPosition[3][0] = p.m_flVertexMin[0];
	flOriginalPosition[3][1] = p.m_flVertexMax[1];

#if 0
	float flOriginalTexCoord[4][2];
	flOriginalTexCoord[0][0] = p.m_flTexCoordMin[0];
	flOriginalTexCoord[0][1] = p.m_flTexCoordMin[1];
	flOriginalTexCoord[1][0] = p.m_flTexCoordMax[0];
	flOriginalTexCoord[1][1] = p.m_flTexCoordMin[1];
	flOriginalTexCoord[2][0] = p.m_flTexCoordMax[0];
	flOriginalTexCoord[2][1] = p.m_flTexCoordMax[1];
	flOriginalTexCoord[3][0] = p.m_flTexCoordMin[0];
	flOriginalTexCoord[3][1] = p.m_flTexCoordMax[1];
#endif

	v[0].m_flPosition[0] = flAdjustedVertexMin[0];
	v[0].m_flPosition[1] = flAdjustedVertexMin[1];
	v[1].m_flPosition[0] = flAdjustedVertexMax[0];
	v[1].m_flPosition[1] = flAdjustedVertexMin[1];
	v[2].m_flPosition[0] = flAdjustedVertexMax[0];
	v[2].m_flPosition[1] = flAdjustedVertexMax[1];
	v[3].m_flPosition[0] = flAdjustedVertexMin[0];
	v[3].m_flPosition[1] = flAdjustedVertexMax[1];
	
	v[0].m_flTexCoordGradientCoord[0] = flAdjustedTexCoordMin[0];
	v[0].m_flTexCoordGradientCoord[1] = flAdjustedTexCoordMin[1];
	v[1].m_flTexCoordGradientCoord[0] = flAdjustedTexCoordMax[0];
	v[1].m_flTexCoordGradientCoord[1] = flAdjustedTexCoordMin[1];
	v[2].m_flTexCoordGradientCoord[0] = flAdjustedTexCoordMax[0];
	v[2].m_flTexCoordGradientCoord[1] = flAdjustedTexCoordMax[1];
	v[3].m_flTexCoordGradientCoord[0] = flAdjustedTexCoordMin[0];
	v[3].m_flTexCoordGradientCoord[1] = flAdjustedTexCoordMax[1];

	v[0].m_flOpacityTexCoord[0] = p.m_flOpacityTexCoordMin[0];
	v[0].m_flOpacityTexCoord[1] = p.m_flOpacityTexCoordMin[1];
	v[1].m_flOpacityTexCoord[0] = p.m_flOpacityTexCoordMax[0];
	v[1].m_flOpacityTexCoord[1] = p.m_flOpacityTexCoordMin[1];
	v[2].m_flOpacityTexCoord[0] = p.m_flOpacityTexCoordMax[0];
	v[2].m_flOpacityTexCoord[1] = p.m_flOpacityTexCoordMax[1];
	v[3].m_flOpacityTexCoord[0] = p.m_flOpacityTexCoordMin[0];
	v[3].m_flOpacityTexCoord[1] = p.m_flOpacityTexCoordMax[1];

	// forestw: generate gradient coords and corner coords, and copy in the gradient colors
	// doing all this vertex matrix transform work on CPU because it takes less CPU time than setting uniforms
	for ( i = 0; i < 4; i++ )
	{
		v[i].m_flPosition[2] = p.m_flZ;
		v[i].m_flPosition[3] = 1.0f;
		v[i].m_flTexCoordGradientCoord[2] = flOriginalPosition[i][0] * flGradientMatrix[0][0] + flOriginalPosition[i][1] * flGradientMatrix[1][0] + flGradientMatrix[2][0];
		v[i].m_flTexCoordGradientCoord[3] = flOriginalPosition[i][0] * flGradientMatrix[0][1] + flOriginalPosition[i][1] * flGradientMatrix[1][1] + flGradientMatrix[2][1];
		v[i].m_flOpacityTexCoord[2] = 0.0f;
		v[i].m_flOpacityTexCoord[3] = 0.0f;
		v[i].m_flColor[0][0] = brush.m_flColor[0][0];
		v[i].m_flColor[0][1] = brush.m_flColor[0][1];
		v[i].m_flColor[0][2] = brush.m_flColor[0][2];
		v[i].m_flColor[0][3] = brush.m_flColor[0][3];
		v[i].m_flColor[1][0] = brush.m_flColor[1][0];
		v[i].m_flColor[1][1] = brush.m_flColor[1][1];
		v[i].m_flColor[1][2] = brush.m_flColor[1][2];
		v[i].m_flColor[1][3] = brush.m_flColor[1][3];
		// rounded corners are done with a coordinate matrix to transform positions into a single arc space to keep the per-pixel cost low
		for ( j = 0; j < 4; j++ )
		{
			v[i].m_flOuterCornerCoord[j][0] = v[i].m_flPosition[0] * flOuterCornerMatrix[j][0][0] + v[i].m_flPosition[1] * flOuterCornerMatrix[j][1][0] + flOuterCornerMatrix[j][2][0];
			v[i].m_flOuterCornerCoord[j][1] = v[i].m_flPosition[0] * flOuterCornerMatrix[j][0][1] + v[i].m_flPosition[1] * flOuterCornerMatrix[j][1][1] + flOuterCornerMatrix[j][2][1];
			v[i].m_flInnerCornerCoord[j][0] = v[i].m_flPosition[0] * flInnerCornerMatrix[j][0][0] + v[i].m_flPosition[1] * flInnerCornerMatrix[j][1][0] + flInnerCornerMatrix[j][2][0];
			v[i].m_flInnerCornerCoord[j][1] = v[i].m_flPosition[0] * flInnerCornerMatrix[j][0][1] + v[i].m_flPosition[1] * flInnerCornerMatrix[j][1][1] + flInnerCornerMatrix[j][2][1];
		}
	}

	{
		VPROF_BUDGET( "COpenGLSurface::DrawFancyQuad - GL render", VPROF_BUDGETGROUP_TENFOOT );

		GL_CHECK_CURRENT_CONTEXT;
		CHECK_GL_ERRORS();

		int nType = 0;
		int nFlags = 0;
		for ( i = 0; i < 4; i++ )
		{
			if ( flScaledOuterCornerRadii[i][0] * flScaledOuterCornerRadii[i][1] > 0.0f )
				nFlags |= FancyQuadFlag_OuterCorner;
			if ( flScaledInnerCornerRadii[i][0] * flScaledInnerCornerRadii[i][1] > 0.0f )
				nFlags |= FancyQuadFlag_InnerCorner;
			if ( p.m_flBorderWidth[i] > 0.0f )
				nFlags |= FancyQuadFlag_InnerCorner;
		}
		if ( brush.m_nGradientStops > 2 )
			nFlags |= FancyQuadFlag_GradientComplex;
		else if ( brush.m_nGradientStops > 1 )
			nFlags |= FancyQuadFlag_GradientTwoStop;
		if ( brush.m_bIsRadialGradient )
			nFlags |= FancyQuadFlag_RadialGradient;
		if ( flSaturation != 1.0f )
			nFlags |= FancyQuadFlag_Saturation;
		if ( nTexture1OpacityMask )
			nFlags |= FancyQuadFlag_OpacityMask;
		// the premul shader will multiply the texture's rgb by the texture's alpha, we don't need this if using an alpha texture or no texture, and if the texture is already premul then we don't need it there either...
		if ( !nTexture0ID )
			nType = FancyQuadTextureType_None;
		else if ( isyuvtexture )
			nType = FancyQuadTextureType_YUV;
		else if ( isalphatexture )
			nType = FancyQuadTextureType_Alpha;
		else if ( texisnotpremul )
			nType = FancyQuadTextureType_Premul;
		else
			nType = FancyQuadTextureType_RGBA;

		if ( nTexture0ID )
			DebugDumpTexture( nTexture0ID );
		if ( nTexture1OpacityMask )
			DebugDumpTexture( nTexture1OpacityMask );
		if ( nTexture2ID )
			DebugDumpTexture( nTexture2ID );

		CLazyShaderProgram &program = m_shaderprogramFancyQuadUber[nType][nFlags];

		if ( m_LastProgram != program.GetProgram() )
		{
			VPROF_BUDGET( "COpenGLSurface::DrawFancyQuad - glUseProgram", VPROF_BUDGETGROUP_TENFOOT );
			glUseProgram( program.GetProgram() );
			m_LastProgram = program.GetProgram();
		}

		if ( flMatrix == s_flMatrixIdentity )
		{
			if ( !program.m_bLastMatrixWasIdentity )
			{
				glUniformMatrix4fv( program.m_matTransformLoc, 1, GL_FALSE, ( GLfloat * ) flMatrix );
				program.m_bLastMatrixWasIdentity = true;
			}
		}
		else
		{
			VPROF_BUDGET( "COpenGLSurface::DrawFancyQuad - glUniformMatrix4fv", VPROF_BUDGETGROUP_TENFOOT );
			glUniformMatrix4fv( program.m_matTransformLoc, 1, GL_FALSE, ( GLfloat * ) flMatrix );
			program.m_bLastMatrixWasIdentity = false;
		}


		{
			VPROF_BUDGET( "COpenGLSurface::DrawFancyQuad - glUniform", VPROF_BUDGETGROUP_TENFOOT );

			glUniform1f( program.m_viewportWidthLoc, ( float )nWide );
			glUniform1f( program.m_viewportHeightLoc, ( float )nTall );
			if ( program.m_textureLoc >= 0 )
				glUniform1i( program.m_textureLoc, 0 );
			if ( program.m_texture1Loc >= 0 )
				glUniform1i( program.m_texture1Loc, 1 );
			if ( program.m_texture2Loc >= 0 )
				glUniform1i( program.m_texture2Loc, 2 );
			if ( program.m_outercornerradii0 >= 0 )
			{
				glUniform4fv( program.m_outercornerradii0, 1, flScaledOuterCornerRadii[0]);
				glUniform4fv( program.m_outercornerradii1, 1, flScaledOuterCornerRadii[2]);
			}
			if ( program.m_innercornerradii0 >= 0 )
			{
				glUniform4fv( program.m_innercornerradii0, 1, flScaledInnerCornerRadii[0]);
				glUniform4fv( program.m_innercornerradii1, 1, flScaledInnerCornerRadii[2]);
			}
			if ( program.m_bordercolor >= 0 )
			{
				glUniform4fv( program.m_bordercolor, 1, p.m_flBorderColor );
			}
			if ( program.m_SaturationLoc >= 0 )
			{
				glUniform1f( program.m_SaturationLoc, flSaturation );
			}
			if ( program.m_BrightnessLoc >= 0 )
			{
				glUniform1f( program.m_BrightnessLoc, flBrightness );
			}
			if ( program.m_ContrastLoc >= 0 )
			{
				glUniform1f( program.m_ContrastLoc, flContrast );
			}
			if ( program.m_HueShiftLoc >= 0 )
			{
				glUniform1f( program.m_HueShiftLoc, flHueShift );
			}
			if ( program.m_OpacityMaskOpacityLoc >= 0 )
			{
				glUniform1f( program.m_OpacityMaskOpacityLoc, flOpacityMaskOpacity );
			}
			if ( program.m_gradientradialoffset >= 0)
			{
				// forestw: using *2 on the gradientradialoffset makes the main menu background match the D2D version, I don't know if this is correct though
				glUniform4f( program.m_gradientradialoffset, (brush.m_flGradientEndPoint[0] - brush.m_flGradientStartPoint[0]) / brush.m_flGradientRadii[0] * 2.0f, (brush.m_flGradientEndPoint[1] - brush.m_flGradientStartPoint[1]) / brush.m_flGradientRadii[1] * 2.0f, 0.0f, 0.0f );
			}
		}
		
		if ( nTexture2ID )
		{
			VPROF_BUDGET( "COpenGLSurface::DrawFancyQuad - nTexture2ID", VPROF_BUDGETGROUP_TENFOOT );
			SetTexture( GL_TEXTURE2_ARB, nTexture2ID );
		}
		if ( nTexture1OpacityMask )
		{
			VPROF_BUDGET( "COpenGLSurface::DrawFancyQuad - nTexture1OpacityMask", VPROF_BUDGETGROUP_TENFOOT );
			SetTexture( GL_TEXTURE1_ARB, nTexture1OpacityMask );
		}
		if ( nFlags & FancyQuadFlag_GradientComplex )
		{
			VPROF_BUDGET( "COpenGLSurface::DrawFancyQuad - GradientComplex", VPROF_BUDGETGROUP_TENFOOT );
			Assert(m_iGradientTextureName);

			UpdateFancyQuadGradientTexture ( brush );
		
			glActiveTexture( GL_TEXTURE3_ARB );
			glBindTexture( GL_TEXTURE_1D, m_iGradientTextureName );
			glTexImage1D(	GL_TEXTURE_1D, 0, GL_RGBA, FANCYQUAD_GRADIENT_TEXTURE_SIZE,
							0, GL_RGBA, GL_UNSIGNED_BYTE, &m_GradientTextureBuffer[0][0]);
			glActiveTexture( GL_TEXTURE0_ARB );
		
			if ( program.m_texture3Loc >= 0 )
			{
				glUniform1i( program.m_texture3Loc, 3 );
			}
		}

		{
			SetTexture( GL_TEXTURE0_ARB, nTexture0ID );
			CHECK_GL_ERRORS();
		}

		if ( brush.m_bAlphaOnlyTexture )
		{
#define GL_TEXTURE_SWIZZLE_RGBA           0x8E46
			SetTexture( GL_TEXTURE0_ARB, nTexture0ID );
			GLint swizzleMask[] = { GL_ALPHA, GL_ALPHA, GL_ALPHA, GL_ALPHA };
			glTexParameteriv( GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask );

			CHECK_GL_ERRORS();
		}

		// Apply 2D rotate to position values
		if ( flRotate2D > 0.00001f || flRotate2D < -0.00001f )
		{
			float flSine;
			float flCosine;
			float flRadians = DEG2RAD( flRotate2D );
			SinCos( flRadians, &flSine, &flCosine );

			float flXTranslate = v[0].m_flPosition[0] + ((v[1].m_flPosition[0] - v[0].m_flPosition[0] )/2.0f);
			float flYTranslate = v[0].m_flPosition[1] + ((v[3].m_flPosition[1] - v[0].m_flPosition[1] )/2.0f);

			for( int iCorner=0; iCorner < 4; ++iCorner )
			{
				float x = ( v[iCorner].m_flPosition[0] - flXTranslate );
				float y = ( v[iCorner].m_flPosition[1] - flYTranslate );

				v[iCorner].m_flPosition[0] = x * flCosine - y * flSine;
				v[iCorner].m_flPosition[1] = y * flCosine + x * flSine;

				v[iCorner].m_flPosition[0] += flXTranslate;
				v[iCorner].m_flPosition[1] += flYTranslate;
			}
		}

	#ifndef DEBUGNODRAW
		// TODO forestw: shovel this into a mesh queue of some kind
		{
			VPROF_BUDGET( "COpenGLSurface::DrawFancyQuad - Mesh", VPROF_BUDGETGROUP_TENFOOT );
			CHECK_GL_ERRORS();
			glBegin( GL_QUADS );
			for ( i = 0; i < 4; i++ )
			{
				glMultiTexCoord4fv( GL_TEXTURE0_ARB, v[i].m_flTexCoordGradientCoord );
				glMultiTexCoord4fv( GL_TEXTURE1_ARB, v[i].m_flOuterCornerCoord[0] );
				glMultiTexCoord4fv( GL_TEXTURE2_ARB, v[i].m_flOuterCornerCoord[2] );
				glMultiTexCoord4fv( GL_TEXTURE3_ARB, v[i].m_flColor[0] );
				glMultiTexCoord4fv( GL_TEXTURE4_ARB, v[i].m_flColor[1] );
				glMultiTexCoord4fv( GL_TEXTURE5_ARB, v[i].m_flInnerCornerCoord[0] );
				glMultiTexCoord4fv( GL_TEXTURE6_ARB, v[i].m_flInnerCornerCoord[2] );
				glMultiTexCoord4fv( GL_TEXTURE7_ARB, v[i].m_flOpacityTexCoord );
				glVertex4fv( v[i].m_flPosition );
			}
			glEnd();
		}
	#endif

		if ( brush.m_bAlphaOnlyTexture )
		{
			SetTexture( GL_TEXTURE0_ARB, nTexture0ID );
			GLint swizzleMask[] = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA };
			glTexParameteriv( GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask );

			CHECK_GL_ERRORS();
		}

		DebugDumpFramebuffer();
		DebugDumpState( __func__ );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get the D2D bitmap object for a given textureid
//-----------------------------------------------------------------------------
IUITexture *COpenGLSurface::GetOGLTextureForTextureID( uint32 unTextureID )
{
	// Lookup the texture
	{
        AUTO_LOCK( m_lockTextureMap );
		int iMap = m_mapTextures.Find( unTextureID );		
		if ( iMap != m_mapTextures.InvalidIndex() )
            return m_mapTextures[iMap];
    }
    
	return 0;
}


//-----------------------------------------------------------------------------
// Purpose: Lock texture, to increment it's draw serial probably even though it was culled
//-----------------------------------------------------------------------------
void COpenGLSurface::LockTexture( const CRenderMsg<CMsgLockTexture> &renderCommand )
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
		COpenGLDoubleBufferedYUV420Texture *pYUVTexture = ( COpenGLDoubleBufferedYUV420Texture * )pTexture;
		pYUVTexture->UploadOGLTextureIfNeeded( this );
	}
	else
	{
		IOGLUITexture *pOGLUITexture = dynamic_cast<IOGLUITexture *>(pTexture);

		GLint glTextureID;
		int iLock = pOGLUITexture->LockAndGetCurrentTexture( glTextureID, body.texture_serial(), this );
		pOGLUITexture->Unlock( iLock );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Draw a textured quad
//-----------------------------------------------------------------------------
void COpenGLSurface::DrawTexturedRect( const CRenderMsg< CMsgRenderTexturedRect > &renderCommand )
{
	VPROF_BUDGET( "COpenGLSurface::DrawTexturedRect", VPROF_BUDGETGROUP_TENFOOT );
		GL_CHECK_CURRENT_CONTEXT;

	const CMsgRenderTexturedRect &msgBody = renderCommand.BodyConst();
//	Msg( "DrawTexturedRect tex%i %f,%f to %f,%f\n", msgBody.texture_id(), msgBody.top_left().x(), msgBody.top_left().y(), msgBody.bottom_right().x(), msgBody.bottom_right().y() );

	IUITexture *pTexture = NULL;
	{
		AUTO_LOCK( m_lockTextureMap );
		short iMap = m_mapTextures.Find( msgBody.texture_id() );
		if ( iMap == m_mapTextures.InvalidIndex() )
			return;

		pTexture = m_mapTextures[iMap];
	}

	if ( m_stackCompositionLayers.Count() == 0 )
	{
		Assert( m_stackCompositionLayers.Count() > 0 );
		return;
	}

	CCompositionLayer *pLayer = m_stackCompositionLayers[ m_stackCompositionLayers.Count()-1 ];
	if ( pLayer->BIsDrawing() )
	{
		// Ok, we have the locked texture data, setup shader resource view variables, and draw into current composition layer.
		pLayer->PopClipLayersAndFlush();
		pLayer->ActivateRenderTarget();

		float x0 = msgBody.top_left().x();
		float x1 = msgBody.bottom_right().x();

		float y0 = msgBody.top_left().y();
		float y1 = msgBody.bottom_right().y();

		float u0 = msgBody.texture_top_left().x();
		float u1 = msgBody.texture_bottom_right().x();

		float v0 = msgBody.texture_top_left().y();
		float v1 = msgBody.texture_bottom_right().y();

		E2DTextureFormat eFormat = pTexture->GetFormat();

		// forestw: prepare the fancy quad parameters and brush according to the message
		FancyQuadParameters_t FancyQuad;
		memset( &FancyQuad, 0, sizeof( FancyQuad ) );
		FancyQuad.m_flZ = 0.0f;
		FancyQuad.m_flVertexMin[0] = x0;
		FancyQuad.m_flVertexMin[1] = y0;
		FancyQuad.m_flVertexMax[0] = x1;
		FancyQuad.m_flVertexMax[1] = y1;
		FancyQuad.m_flTexCoordMin[0] = u0;
		FancyQuad.m_flTexCoordMin[1] = v0;
		FancyQuad.m_flTexCoordMax[0] = u1;
		FancyQuad.m_flTexCoordMax[1] = v1;
		FancyQuad.m_flOpacityTexCoordMin[0] = u0;
		FancyQuad.m_flOpacityTexCoordMin[1] = v0;
		FancyQuad.m_flOpacityTexCoordMax[0] = u1;
		FancyQuad.m_flOpacityTexCoordMax[1] = v1;
		FancyQuad.m_flInnerCornerRadii[0][0] = 0.0f;
		FancyQuad.m_flInnerCornerRadii[0][1] = 0.0f;
		FancyQuad.m_flInnerCornerRadii[1][0] = 0.0f;
		FancyQuad.m_flInnerCornerRadii[1][1] = 0.0f;
		FancyQuad.m_flInnerCornerRadii[2][0] = 0.0f;
		FancyQuad.m_flInnerCornerRadii[2][1] = 0.0f;
		FancyQuad.m_flInnerCornerRadii[3][0] = 0.0f;
		FancyQuad.m_flInnerCornerRadii[3][1] = 0.0f;
		FancyQuad.m_flOuterCornerRadii[0][0] = 0.0f;
		FancyQuad.m_flOuterCornerRadii[0][1] = 0.0f;
		FancyQuad.m_flOuterCornerRadii[1][0] = 0.0f;
		FancyQuad.m_flOuterCornerRadii[1][1] = 0.0f;
		FancyQuad.m_flOuterCornerRadii[2][0] = 0.0f;
		FancyQuad.m_flOuterCornerRadii[2][1] = 0.0f;
		FancyQuad.m_flOuterCornerRadii[3][0] = 0.0f;
		FancyQuad.m_flOuterCornerRadii[3][1] = 0.0f;
		FancyQuad.m_flBorderWidth[0] = 0.0f;
		FancyQuad.m_flBorderWidth[1] = 0.0f;
		FancyQuad.m_flBorderWidth[2] = 0.0f;
		FancyQuad.m_flBorderWidth[3] = 0.0f;
		FancyQuad.m_flBorderColor[0] = 0.0f;
		FancyQuad.m_flBorderColor[1] = 0.0f;
		FancyQuad.m_flBorderColor[2] = 0.0f;
		FancyQuad.m_flBorderColor[3] = 0.0f;
		FancyQuadBrush_t FancyBrush;
		memset( &FancyBrush, 0, sizeof( FancyBrush ) );
		FancyBrush.m_flColor[0][0] = 1.0f;
		FancyBrush.m_flColor[0][1] = 1.0f;
		FancyBrush.m_flColor[0][2] = 1.0f;
		FancyBrush.m_flColor[0][3] = 1.0f;
		FancyBrush.m_flGradientStartPoint[0] = 0.0f;
		FancyBrush.m_flGradientStartPoint[1] = 0.0f;
		FancyBrush.m_flGradientEndPoint[0] = 0.0f;
		FancyBrush.m_flGradientEndPoint[1] = 0.0f;
		FancyBrush.m_flGradientRadii[0] = 0.0f;
		FancyBrush.m_flGradientRadii[1] = 0.0f;
		FancyBrush.m_bIsLinearGradient = false;
		FancyBrush.m_bIsRadialGradient = false;
		if ( msgBody.has_texture_sample_mode() )
		{
			switch ( (ETextureSampleMode)msgBody.texture_sample_mode() )
			{
			case k_ETextureSampleModeAlphaOnly:
				FancyBrush.m_bAlphaOnlyTexture = true;
				break;
			case k_ETextureSampleModeNormal:
				break;
			default:
				AssertMsg( false, "Unknown texture sampling type" );
				break;
			}
		}
		// Special handling for YUV420
		if ( eFormat == k_EFormatYUV420 )
		{
			COpenGLDoubleBufferedYUV420Texture *pYUVTexture = ( COpenGLDoubleBufferedYUV420Texture * )pTexture;
			pYUVTexture->UploadOGLTextureIfNeeded( this );

			if ( !pTexture->BIsReady() )
				return;

			DrawFancyQuad( pYUVTexture->GetOGLTextureID(), pYUVTexture->GetOGLTextureIDU(), pYUVTexture->GetOGLTextureIDV(), 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, pLayer->GetWidth(), pLayer->GetHeight(), FancyQuad, FancyBrush, 1.0f, 1.0f, 0.0f, pTexture->GetTextureWidth(), pTexture->GetTextureHeight(), false, false, true, false, s_flMatrixIdentity, true );
		}
		else
		{
		
			IOGLUITexture *pOGLUITexture = dynamic_cast<IOGLUITexture *>(pTexture);

			GLint glTextureID;
			int iLock = pOGLUITexture->LockAndGetCurrentTexture( glTextureID, renderCommand.BodyConst().texture_serial(), this );

			if ( pTexture->BIsReady() )
			{
				DrawFancyQuad( glTextureID, 0, 0, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, pLayer->GetWidth(), pLayer->GetHeight(), FancyQuad, FancyBrush, 1.0f, 1.0f, 0.0f, pTexture->GetTextureWidth(), pTexture->GetTextureHeight(), pTexture->GetAlphaChannelType() != k_EAlphaChannelType_PreMultiplied, false, false, false, s_flMatrixIdentity, true );
			}
			
			pOGLUITexture->Unlock( iLock );

		}
	}

	if ( m_stackCompositionLayers.Count() == 1 )
		pLayer->PushCliplayersAndBeginDraw( m_flScaleBackbufferX, m_flScaleBackbufferY, m_flTranslateBackbufferX, m_flTranslateBackbufferY );
	else
		pLayer->PushCliplayersAndBeginDraw( 1.0f, 1.0f, 0.0f, 0.0f );

}


//-----------------------------------------------------------------------------
// Purpose: Set up a FancyQuadBrush parameter structure from a CMsgFillBrush
//-----------------------------------------------------------------------------
void COpenGLSurface::SetFancyQuadFillBrush( FancyQuadBrush_t &FancyBrush, const CMsgFillBrush &brushMsg, float offsetx, float offsety )
{
	uint32 rgba;
	float r,g,b,a,p;
	int iColorStop;
	float maxposition = 1.0f;
	bool bPrintGradient = false;
	memset(&FancyBrush, 0, sizeof(FancyBrush));
	if ( brushMsg.has_linear_gradient()  )
	{
		const CMsgLinearGradient &msg = brushMsg.linear_gradient();

		if (bPrintGradient)
			Msg( "SetFancyQuadFillBrush: linear gradient:" );
		for( iColorStop = 0; iColorStop < msg.color_stop_size(); ++iColorStop )
		{
			rgba = msg.color_stop( iColorStop ).color_rgba();
			p = msg.color_stop( iColorStop ).position();
			maxposition = p;
			LinearColorFromABGR( r, g, b, a, rgba, true );
			if (bPrintGradient)
				Msg( " %08x@%.3f", rgba, p );

			a *= brushMsg.opacity();
			FancyBrush.m_flColor[iColorStop][0] = r * a;
			FancyBrush.m_flColor[iColorStop][1] = g * a;
			FancyBrush.m_flColor[iColorStop][2] = b * a;
			FancyBrush.m_flColor[iColorStop][3] = a;
			FancyBrush.m_flGradientStops[iColorStop] = p;
			FancyBrush.m_nGradientStops = iColorStop + 1;
			
			Assert( iColorStop < FANCYQUAD_MAXSTOPS );
		}
		if (bPrintGradient)
			Msg( "\n" );

		// forestw: we have two gradient methods in the shader, one is fast (2 stops, normalized to position 1.0), the other is slow (up to 4 stops at configurable positions)
		// if we're using 2 stops we need to normalize the second position
		if (FancyBrush.m_nGradientStops > 2)
		{
			// if we're using the complex gradient method, skip the gradient collapse
			maxposition = 1.0f;
		}
		FancyBrush.m_flGradientStartPoint[0] = msg.start_position().x() + offsetx;
		FancyBrush.m_flGradientStartPoint[1] = msg.start_position().y() + offsety;
		FancyBrush.m_flGradientEndPoint[0] = msg.start_position().x() + ( msg.end_position().x() - msg.start_position().x() ) * maxposition + offsetx;
		FancyBrush.m_flGradientEndPoint[1] = msg.start_position().y() + ( msg.end_position().y() - msg.start_position().y() ) * maxposition + offsety;
		FancyBrush.m_flGradientRadii[0] = 0.0f;
		FancyBrush.m_flGradientRadii[1] = 0.0f;
		FancyBrush.m_bIsLinearGradient = true;
		FancyBrush.m_bIsRadialGradient = false;
	}
	else if ( brushMsg.has_radial_gradient()  )
	{
		const CMsgRadialGradient &msg = brushMsg.radial_gradient();

		if (bPrintGradient)
			Msg( "SetFancyQuadFillBrush: radial gradient:" );
		for( iColorStop = 0; iColorStop < msg.color_stop_size(); ++iColorStop )
		{
			rgba = msg.color_stop( iColorStop ).color_rgba();
			p = msg.color_stop( iColorStop ).position();
			maxposition = p;
			LinearColorFromABGR( r, g, b, a, rgba, true );
			if (bPrintGradient)
				Msg( " %08x@%.3f", rgba, p );

			a *= brushMsg.opacity();
			FancyBrush.m_flColor[iColorStop][0] = r * a;
			FancyBrush.m_flColor[iColorStop][1] = g * a;
			FancyBrush.m_flColor[iColorStop][2] = b * a;
			FancyBrush.m_flColor[iColorStop][3] = a;
			FancyBrush.m_flGradientStops[iColorStop] = p;
			FancyBrush.m_nGradientStops = iColorStop + 1;

			Assert( iColorStop < FANCYQUAD_MAXSTOPS );
		}
		if (bPrintGradient)
			Msg( "\n" );
		// pad the gradient to fill remaining stops
		while( iColorStop < FANCYQUAD_MAXSTOPS )
		{
			FancyBrush.m_flColor[iColorStop][0] = FancyBrush.m_flColor[iColorStop-1][0];
			FancyBrush.m_flColor[iColorStop][1] = FancyBrush.m_flColor[iColorStop-1][1];
			FancyBrush.m_flColor[iColorStop][2] = FancyBrush.m_flColor[iColorStop-1][2];
			FancyBrush.m_flColor[iColorStop][3] = FancyBrush.m_flColor[iColorStop-1][3];
			FancyBrush.m_flGradientStops[iColorStop] = FancyBrush.m_flGradientStops[iColorStop-1];
			iColorStop++;
		}

		// forestw: we have two gradient methods in the shader, one is fast (2 stops, normalized to position 1.0), the other is slow (up to 4 stops at configurable positions)
		// if we're using 2 stops we need to normalize the second position
		if (FancyBrush.m_nGradientStops > 2)
		{
			// if we're using the complex gradient method, skip the gradient collapse
			maxposition = 1.0f;
		}
		FancyBrush.m_flGradientStartPoint[0] = msg.center_position().x() + offsetx;
		FancyBrush.m_flGradientStartPoint[1] = msg.center_position().y() + offsety;
		FancyBrush.m_flGradientEndPoint[0] = msg.center_position().x() + msg.offset_distance().x() + offsetx;
		FancyBrush.m_flGradientEndPoint[1] = msg.center_position().y() + msg.offset_distance().y() + offsety;
		FancyBrush.m_flGradientRadii[0] = msg.radii().x() * maxposition;
		FancyBrush.m_flGradientRadii[1] = msg.radii().y() * maxposition;
		FancyBrush.m_bIsLinearGradient = false;
		FancyBrush.m_bIsRadialGradient = true;
	}
	else if ( brushMsg.has_color_rgba()  )
	{
		LinearColorFromABGR( r, g, b, a, brushMsg.color_rgba(), true );
		a *= brushMsg.opacity();
		FancyBrush.m_flColor[0][0] = r * a;
		FancyBrush.m_flColor[0][1] = g * a;
		FancyBrush.m_flColor[0][2] = b * a;
		FancyBrush.m_flColor[0][3] = a;
		FancyBrush.m_flGradientStartPoint[0] = 0.0f;
		FancyBrush.m_flGradientStartPoint[1] = 0.0f;
		FancyBrush.m_flGradientEndPoint[0] = 0.0f;
		FancyBrush.m_flGradientEndPoint[1] = 0.0f;
		FancyBrush.m_flGradientRadii[0] = 0.0f;
		FancyBrush.m_flGradientRadii[1] = 0.0f;
		FancyBrush.m_bIsLinearGradient = false;
		FancyBrush.m_bIsRadialGradient = false;
	}
	else
	{
		FancyBrush.m_flColor[0][0] = 1.0f;
		FancyBrush.m_flColor[0][1] = 1.0f;
		FancyBrush.m_flColor[0][2] = 1.0f;
		FancyBrush.m_flColor[0][3] = 1.0f;
		FancyBrush.m_flGradientStartPoint[0] = 0.0f;
		FancyBrush.m_flGradientStartPoint[1] = 0.0f;
		FancyBrush.m_flGradientEndPoint[0] = 0.0f;
		FancyBrush.m_flGradientEndPoint[1] = 0.0f;
		FancyBrush.m_flGradientRadii[0] = 0.0f;
		FancyBrush.m_flGradientRadii[1] = 0.0f;
		FancyBrush.m_bIsLinearGradient = false;
		FancyBrush.m_bIsRadialGradient = false;
	}
	
	if ( FancyBrush.m_bIsLinearGradient || FancyBrush.m_bIsRadialGradient )
	{
		iColorStop = FancyBrush.m_nGradientStops;

		// pad the gradient to fill remaining stops
		while( iColorStop < FANCYQUAD_MAXSTOPS )
		{
			FancyBrush.m_flColor[iColorStop][0] = FancyBrush.m_flColor[iColorStop-1][0];
			FancyBrush.m_flColor[iColorStop][1] = FancyBrush.m_flColor[iColorStop-1][1];
			FancyBrush.m_flColor[iColorStop][2] = FancyBrush.m_flColor[iColorStop-1][2];
			FancyBrush.m_flColor[iColorStop][3] = FancyBrush.m_flColor[iColorStop-1][3];
			FancyBrush.m_flGradientStops[iColorStop] = 1.0f;
			iColorStop++;
		}
		
		// If the inbound gradient doesn't end at 1.0, promote one of our added padding
		// stops to the actual last stop of the gradient.
		if ( FancyBrush.m_flGradientStops[FancyBrush.m_nGradientStops - 1] < 1.0f )
		{
			FancyBrush.m_nGradientStops++;
			Assert ( FancyBrush.m_nGradientStops < FANCYQUAD_MAXSTOPS );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Draw a filled quad
//-----------------------------------------------------------------------------
void COpenGLSurface::DrawFilledRect( const CRenderMsg< CMsgRenderFilledRect > &renderCommand )
{
	VPROF_BUDGET( "COpenGLSurface::DrawFilledRect", VPROF_BUDGETGROUP_TENFOOT );
	if ( m_stackCompositionLayers.Count() == 0 )
	{
		Assert( m_stackCompositionLayers.Count() > 0 );
		return;
	}

	const CMsgRenderFilledRect &msgBody = renderCommand.BodyConst();
//	Msg( "DrawFilledRect %f,%f to %f,%f\n", msgBody.top_left().x(), msgBody.top_left().y(), msgBody.bottom_right().x(), msgBody.bottom_right().y() );

	CCompositionLayer *pLayer = m_stackCompositionLayers[ m_stackCompositionLayers.Count()-1 ];
	if ( pLayer->BIsDrawing() )
	{
		const CMsgRenderFillBrushCollection &fill_brush_collection = msgBody.fill_brush_collection();
		int cBrushes = fill_brush_collection.fill_brush_size();
		GL_CHECK_CURRENT_CONTEXT;
		float x0 = msgBody.top_left().x();
		float y0 = msgBody.top_left().y();
		float x1 = msgBody.bottom_right().x();
		float y1 = msgBody.bottom_right().y();
		pLayer->ActivateRenderTarget();
		for ( int i = 0; i < cBrushes; ++i )
		{
			const CMsgFillBrush &brushMsg = fill_brush_collection.fill_brush( i );
			if ( brushMsg.has_particle_system() )
			{
				VertexTextured_t quad[4];
				
				const CMsgParticleSystem &system = brushMsg.particle_system();
				int nParticles = system.particles_size();
				{
					VPROF_BUDGET( "DrawFilledRect - ParticleSystem Loop", VPROF_BUDGETGROUP_TENFOOT );

					if ( m_LastProgram != m_shaderprogramParticle.GetProgram() )
					{
						glUseProgram( m_shaderprogramParticle.GetProgram() );
						m_LastProgram =  m_shaderprogramParticle.GetProgram();
					}
					glUniformMatrix4fv( m_shaderprogramParticle.m_matTransformLoc, 1, GL_FALSE, s_flMatrixIdentity );
					glUniform1f( m_shaderprogramParticle.m_viewportWidthLoc, ( float )pLayer->GetWidth() );
					glUniform1f( m_shaderprogramParticle.m_viewportHeightLoc, ( float )pLayer->GetHeight() );

					for ( int iParticle = 0; iParticle < nParticles; ++iParticle )
					{
						const CMsgParticle &particle = system.particles( iParticle );
																		
						float flSharpness = particle.particle_sharpness();
						float flHalfSize = particle.particle_size() / 2.0f;					
												
						uint32 rgba = particle.color_rgba();
						float r,g,b,a;
						
						r = (rgba&0xff) / 255.0f;
						g = ((rgba>>8)&0xff) / 255.0f;
						b = ((rgba>>16)&0xff) / 255.0f;
						a = ((rgba>>24)&0xff) / 255.0f;

						quad[0].r = r;
						quad[0].g = g;
						quad[0].b = b;
						quad[0].a = a * brushMsg.opacity();
						quad[0].rhw = 1.0f;
						quad[0].u = 0.0f;
						quad[0].v = 1.0f;
						quad[0].masku1 = quad[0].masku2 = flSharpness;
						quad[0].maskv1 = quad[0].maskv2 = flSharpness;
						quad[0].x = particle.particle_position().x() - flHalfSize;
						quad[0].y = particle.particle_position().y() - flHalfSize;
						quad[0].z = particle.particle_position().z();
						
						quad[1].r = r;
						quad[1].g = g;
						quad[1].b = b;
						quad[1].a = a * brushMsg.opacity();;
						quad[1].rhw = 1.0f;
						quad[1].u = 1.0f;
						quad[1].v = 1.0f;
						quad[1].masku1 = quad[1].masku2 = flSharpness;
						quad[1].maskv1 = quad[1].maskv2 = flSharpness;
						quad[1].x = particle.particle_position().x() + flHalfSize;
						quad[1].y = particle.particle_position().y() - flHalfSize;
						quad[1].z = particle.particle_position().z();
						
						quad[2].r = r;
						quad[2].g = g;
						quad[2].b = b;
						quad[2].a = a * brushMsg.opacity();;
						quad[2].rhw = 1.0f;
						quad[2].u = 1.0f;
						quad[2].v = 0.0f;
						quad[2].masku1 = quad[2].masku2 = flSharpness;
						quad[2].maskv1 = quad[2].maskv2 = flSharpness;
						quad[2].x = particle.particle_position().x() + flHalfSize;
						quad[2].y = particle.particle_position().y() + flHalfSize;
						quad[2].z = particle.particle_position().z();
						
						quad[3].r = r;
						quad[3].g = g;
						quad[3].b = b;
						quad[3].a = a * brushMsg.opacity();
						quad[3].rhw = 1.0f;
						quad[3].u = 0.0f;
						quad[3].v = 0.0f;
						quad[3].masku1 = quad[3].masku2 = flSharpness;
						quad[3].maskv1 = quad[3].maskv2 = flSharpness;
						quad[3].x = particle.particle_position().x() - flHalfSize;
						quad[3].y = particle.particle_position().y() + flHalfSize;
						quad[3].z = particle.particle_position().z();
						
						glUniform1f( m_shaderprogramParticle.m_particleSharpness, flSharpness );

						DrawTexturedQuadInternal( m_shaderprogramParticle, pLayer->GetOGLTextureID(), quad, 1.0, 1.0, 0.0f );
					}
				}
				
			}
			else
			{
				// forestw: prepare the fancy quad parameters and brush according to the message
				FancyQuadParameters_t FancyQuad;
				memset( &FancyQuad, 0, sizeof( FancyQuad ) );
				FancyQuad.m_flZ = msgBody.top_left().z();
				FancyQuad.m_flVertexMin[0] = x0;
				FancyQuad.m_flVertexMin[1] = y0;
				FancyQuad.m_flVertexMax[0] = x1;
				FancyQuad.m_flVertexMax[1] = y1;
				FancyQuad.m_flTexCoordMin[0] = 0.0f;
				FancyQuad.m_flTexCoordMin[1] = 0.0f;
				FancyQuad.m_flTexCoordMax[0] = 1.0f;
				FancyQuad.m_flTexCoordMax[1] = 1.0f;
				FancyQuad.m_flOpacityTexCoordMin[0] = 0.0f;
				FancyQuad.m_flOpacityTexCoordMin[1] = 0.0f;
				FancyQuad.m_flOpacityTexCoordMax[0] = 0.0f;
				FancyQuad.m_flOpacityTexCoordMax[1] = 0.0f;
				FancyQuad.m_flInnerCornerRadii[0][0] = 0.0f;
				FancyQuad.m_flInnerCornerRadii[0][1] = 0.0f;
				FancyQuad.m_flInnerCornerRadii[1][0] = 0.0f;
				FancyQuad.m_flInnerCornerRadii[1][1] = 0.0f;
				FancyQuad.m_flInnerCornerRadii[2][0] = 0.0f;
				FancyQuad.m_flInnerCornerRadii[2][1] = 0.0f;
				FancyQuad.m_flInnerCornerRadii[3][0] = 0.0f;
				FancyQuad.m_flInnerCornerRadii[3][1] = 0.0f;
				FancyQuad.m_flOuterCornerRadii[0][0] = 0.0f;
				FancyQuad.m_flOuterCornerRadii[0][1] = 0.0f;
				FancyQuad.m_flOuterCornerRadii[1][0] = 0.0f;
				FancyQuad.m_flOuterCornerRadii[1][1] = 0.0f;
				FancyQuad.m_flOuterCornerRadii[2][0] = 0.0f;
				FancyQuad.m_flOuterCornerRadii[2][1] = 0.0f;
				FancyQuad.m_flOuterCornerRadii[3][0] = 0.0f;
				FancyQuad.m_flOuterCornerRadii[3][1] = 0.0f;
				FancyQuad.m_flBorderWidth[0] = 0.0f;
				FancyQuad.m_flBorderWidth[1] = 0.0f;
				FancyQuad.m_flBorderWidth[2] = 0.0f;
				FancyQuad.m_flBorderWidth[3] = 0.0f;
				FancyQuad.m_flBorderColor[0] = 0.0f;
				FancyQuad.m_flBorderColor[1] = 0.0f;
				FancyQuad.m_flBorderColor[2] = 0.0f;
				FancyQuad.m_flBorderColor[3] = 0.0f;
				FancyQuadBrush_t FancyBrush;
				SetFancyQuadFillBrush( FancyBrush, brushMsg, x0, y0 );
				// forestw: fancy quad rendering
				DrawFancyQuad( 0, 0, 0, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, -1, -1, FancyQuad, FancyBrush, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f, false, false, false, false, s_flMatrixIdentity, true );
			}
		}
	}

}


//-----------------------------------------------------------------------------
// Purpose: Generate or return cached text opacity mask texture
//-----------------------------------------------------------------------------
UITextOpacityMaskData_t *COpenGLSurface::GetCachedTextOpacityMask( const void *pRawText, int cbRawText, int cTextChars, EPanoramaTextEncoding eTextEncoding, float x0, float y0, float x1, float y1, float flLineHeight, ETextAlign align, bool bWrap, bool bEllipsis, const CMsgRenderTextFormat &defaultFormat, const google::protobuf::RepeatedPtrField< CMsgRenderTextRangeFormat > &rangeFormats )
{
	VPROF_BUDGET( "COpenGLSurface::GetCachedTextOpacityMask", VPROF_BUDGETGROUP_TENFOOT );


	// Need text to do any work
	if ( !pRawText || cTextChars <= 0 )
		return NULL;

	UITextLayoutProperties_t *pKey = m_pTextLayoutDrawCache->AllocTextLayoutProperties( pRawText, cbRawText, cTextChars, eTextEncoding, x0, y0, x1, y1, flLineHeight, align, bWrap, bEllipsis, UIEngine()->GetDisplayLanguage() );

	RenderMsgToTextLayoutKey( defaultFormat, rangeFormats, pKey );

	CUtlVectorFixedGrowable<const char *, 4> vecRangeFontNames;
	vecRangeFontNames.SetCount( rangeFormats.size() );

	for ( int i = 0; i < rangeFormats.size(); i++ )
	{
		if ( rangeFormats.Get(i).format().has_font_name() )
		{
			vecRangeFontNames[i] = rangeFormats.Get(i).format().font_name().c_str();
		}
		else
		{
			vecRangeFontNames[i] = NULL;
		}
	}
    
	UITextOpacityMaskData_t *pData = m_pTextLayoutDrawCache->GetTextOpacityMask( pRawText, cbRawText, cTextChars, eTextEncoding, x0, y0, defaultFormat.font_name().c_str(), pKey, vecRangeFontNames.Base(), m_flCurrentRenderFrameTime, NULL );

	m_pTextLayoutDrawCache->FreeTextLayoutProperties( pKey );

	return pData;
}


//-----------------------------------------------------------------------------
// Purpose: Creates or finds a cached text alpha texture
//-----------------------------------------------------------------------------
UITextTextureRegion_t COpenGLSurface::GetTextureRegion( int32 iWidth, int32 iHeight )
{
    return m_pTextTextureCache->GetTextureRegion( iWidth, iHeight );
}


//-----------------------------------------------------------------------------
// Purpose: Called at the start of updating a font texture
//-----------------------------------------------------------------------------
void COpenGLSurface::StartUpdateFontGlyphTexture( UITextTextureHandle_t hTexture )
{
	SetTexture( GL_TEXTURE0_ARB, (GLuint)((intptr_t)hTexture & (intptr_t)0xFFFFFFFF) );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );

	// Force the next update to set the OpenGL row width.
	m_iLastFontGlyphTextureWidth = -1;
}


//-----------------------------------------------------------------------------
// Purpose: Called to update font texture
//-----------------------------------------------------------------------------
void COpenGLSurface::UpdateFontGlyphTexture( UITextTextureHandle_t hTexture, int xOffset, int yOffset, int width, int height, void *pSourceData )
{
	if ( width != m_iLastFontGlyphTextureWidth )
	{
		glPixelStorei( GL_UNPACK_ROW_LENGTH, width );
		m_iLastFontGlyphTextureWidth = width;
	}

	
	glTexSubImage2D( GL_TEXTURE_2D, 0, xOffset, yOffset, width, height, GL_ALPHA, GL_UNSIGNED_BYTE, pSourceData );
}

	const uint32 unTargetTexWidth = 1024;

//-----------------------------------------------------------------------------
// Purpose: Called at the end of updating a font texture
//-----------------------------------------------------------------------------
void COpenGLSurface::EndUpdateFontGlyphTexture( UITextTextureHandle_t hTexture )
{
	glPixelStorei( GL_UNPACK_ROW_LENGTH, 0 );
}


//-----------------------------------------------------------------------------
// Purpose: Allocate a new alpha-only texture for text rendering
//-----------------------------------------------------------------------------
UITextTextureHandle_t COpenGLSurface::AllocAlphaTexture( int32 iWidth, int32 iHeight )
{
    GLuint unRenderTarget = CreateOpenGLTextureId();
    Assert( unRenderTarget );

    glTexImage2D( GL_TEXTURE_2D, 0, GL_ALPHA8, iWidth, iHeight, 0, GL_ALPHA, GL_UNSIGNED_BYTE, NULL );

    m_vecAllTextBitmaps.AddToTail( unRenderTarget );
    return (UITextTextureHandle_t)(intptr_t)unRenderTarget;
}


//-----------------------------------------------------------------------------
// Purpose: Free a texture
//-----------------------------------------------------------------------------
void COpenGLSurface::FreeTexture( UITextTextureHandle_t hTexture )
{
    GLuint unTextureId = (GLuint)((intptr_t)hTexture & (intptr_t)0xFFFFFFFF);
    Assert( glIsTexture( unTextureId ) );
    m_vecAllTextBitmaps.FindAndRemove( unTextureId );
    glDeleteTextures( 1, &unTextureId );
}


//-----------------------------------------------------------------------------
// Purpose: Internal helper to draw a range of text with a specified brush
//-----------------------------------------------------------------------------
void COpenGLSurface::DrawTextRegionRange( CCompositionLayer *pLayer, float x0, float y0, float x1, float y1, UITextOpacityMaskDataRange_t &maskRange, const CMsgRenderFillBrushCollection &fill_brush_collection )
{	
	// x0,y0,x1,y1 is the rect encompassing the entire text layout (all lines)
	// maskRange.m_x0,x1,y0,y1 defines the rect of the run
	x0 += maskRange.m_flStringOffsetX;
	x1 = x0 + maskRange.m_x1 - maskRange.m_x0;
	y0 += maskRange.m_flStringOffsetY;
	y1 = y0 + maskRange.m_y1 - maskRange.m_y0;
	
	float u0 = maskRange.m_x0 / maskRange.m_flTextureWidth;
	float u1 = maskRange.m_x1 / maskRange.m_flTextureWidth;

	float v0 = maskRange.m_y0 / maskRange.m_flTextureHeight;
	float v1 = maskRange.m_y1 / maskRange.m_flTextureHeight;

	//Msg( "Drawing Text: %1.2f,%1.2f - %1.2f,%1.2f - %1.2f,%1.2f - %1.2f,%1.2f\n", x0,y0, x1,y1, u0,v0, u1,v1 );

//	glClearColor( 0.0f, 0.5f, 0.0f, 1.0f );
//	glClear( GL_COLOR_BUFFER_BIT );
	// FIXME forestw: show at least something for text regions
	FancyQuadParameters_t FancyQuad;
	memset( &FancyQuad, 0, sizeof( FancyQuad ) );
	FancyQuad.m_flZ = 0.0f;
	FancyQuad.m_flVertexMin[0] = x0;
	FancyQuad.m_flVertexMin[1] = y0;
	FancyQuad.m_flVertexMax[0] = x1;
	FancyQuad.m_flVertexMax[1] = y1;
	FancyQuad.m_flTexCoordMin[0] = u0;
	FancyQuad.m_flTexCoordMin[1] = v0;
	FancyQuad.m_flTexCoordMax[0] = u1;
	FancyQuad.m_flTexCoordMax[1] = v1;
	FancyQuad.m_flOpacityTexCoordMin[0] = 0.0f;
	FancyQuad.m_flOpacityTexCoordMin[1] = 1.0f;
	FancyQuad.m_flOpacityTexCoordMax[0] = 0.0f;
	FancyQuad.m_flOpacityTexCoordMax[1] = 1.0f;
	FancyQuad.m_flInnerCornerRadii[0][0] = 0.0f;
	FancyQuad.m_flInnerCornerRadii[0][1] = 0.0f;
	FancyQuad.m_flInnerCornerRadii[1][0] = 0.0f;
	FancyQuad.m_flInnerCornerRadii[1][1] = 0.0f;
	FancyQuad.m_flInnerCornerRadii[2][0] = 0.0f;
	FancyQuad.m_flInnerCornerRadii[2][1] = 0.0f;
	FancyQuad.m_flInnerCornerRadii[3][0] = 0.0f;
	FancyQuad.m_flInnerCornerRadii[3][1] = 0.0f;
	FancyQuad.m_flOuterCornerRadii[0][0] = 0.0f;
	FancyQuad.m_flOuterCornerRadii[0][1] = 0.0f;
	FancyQuad.m_flOuterCornerRadii[1][0] = 0.0f;
	FancyQuad.m_flOuterCornerRadii[1][1] = 0.0f;
	FancyQuad.m_flOuterCornerRadii[2][0] = 0.0f;
	FancyQuad.m_flOuterCornerRadii[2][1] = 0.0f;
	FancyQuad.m_flOuterCornerRadii[3][0] = 0.0f;
	FancyQuad.m_flOuterCornerRadii[3][1] = 0.0f;
	FancyQuad.m_flBorderWidth[0] = 0.0f;
	FancyQuad.m_flBorderWidth[1] = 0.0f;
	FancyQuad.m_flBorderWidth[2] = 0.0f;
	FancyQuad.m_flBorderWidth[3] = 0.0f;
	FancyQuad.m_flBorderColor[0] = 0.0f;
	FancyQuad.m_flBorderColor[1] = 0.0f;
	FancyQuad.m_flBorderColor[2] = 0.0f;
	FancyQuad.m_flBorderColor[3] = 0.0f;
	FancyQuadBrush_t FancyBrush;
	memset( &FancyBrush, 0, sizeof( FancyBrush ) );
	int cBrushes = fill_brush_collection.fill_brush_size();
	for ( int i = 0; i < cBrushes; ++i )
	{
		SetFancyQuadFillBrush( FancyBrush, fill_brush_collection.fill_brush( i ), x0, y0 );
		DrawFancyQuad( (uint32)((intptr_t)maskRange.m_hTexture & (intptr_t)0xFFFFFFFF), 0, 0, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, -1, -1, FancyQuad, FancyBrush, 1.0f, 1.0f, 0.0f, maskRange.m_flTextureWidth, maskRange.m_flTextureHeight, false, true, false, true, s_flMatrixIdentity, true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handle drawing text region
//-----------------------------------------------------------------------------
void COpenGLSurface::DrawTextRegion( const CRenderMsg< CMsgRenderTextRegion > &renderCommand ) 
{
	VPROF_BUDGET( "COpenGLSurface::DrawTextRegion", VPROF_BUDGETGROUP_TENFOOT );
	if ( m_stackCompositionLayers.Count() == 0 )
	{
		Assert( m_stackCompositionLayers.Count() > 0 );
		return;
	}
	
	CCompositionLayer *pLayer = m_stackCompositionLayers[ m_stackCompositionLayers.Count()-1 ];
	if ( pLayer->BIsDrawing() )
	{
		const CMsgRenderTextRegion &msgBody = renderCommand.BodyConst();
		pLayer->ActivateRenderTarget();
		
		float x0, y0, x1, y1;
		x0 = msgBody.top_left().x();
		y0 = msgBody.top_left().y();
		x1 = msgBody.bottom_right().x();
		y1 = msgBody.bottom_right().y();
		//Msg( "DrawTextRegion %f,%f to %f,%f\n", msgBody.top_left().x(), msgBody.top_left().y(), msgBody.bottom_right().x(), msgBody.bottom_right().y() );
		
		// Check rect has valid area
		if ( x1 <= x0 || y1 <= y0 )
			return;
		
		float flLineHeight = k_flFloatNotSet;
		if ( msgBody.has_line_height() )
			flLineHeight = msgBody.line_height();
		
		// get textures
		UITextOpacityMaskData_t *pResult = GetCachedTextOpacityMask( msgBody.raw_text().data(), msgBody.raw_text().size(), msgBody.text_chars(), (EPanoramaTextEncoding)msgBody.text_encoding(), x0, y0, x1, y1, flLineHeight, ( ETextAlign )msgBody.text_align(), msgBody.wrapping(), msgBody.ellipsis(), 
																  msgBody.default_format(), msgBody.range_formats() );
		if ( pResult )
		{
			// draw shadow if present first using shadow color
			if ( msgBody.has_text_shadow() )
			{
				// Create temporary render target to draw into and blur...
				CCompositionLayer *pHorizontalBlurLayer = GetCompositionLayer( pLayer->GetWidth(), pLayer->GetHeight() );
				CCompositionLayer *pVerticalBlurLayer = GetCompositionLayer( pLayer->GetWidth(), pLayer->GetHeight() );
				pVerticalBlurLayer->Clear();
				pHorizontalBlurLayer->Clear();
				m_stackCompositionLayers.AddToTail( pHorizontalBlurLayer );
				m_stackCompositionLayers.AddToTail( pVerticalBlurLayer );

				VertexTextured_t quad[4];
				quad[0].r = quad[0].g = quad[0].b = quad[0].a = 1.0f;
				quad[0].rhw = 1.0f;
				quad[0].u = quad[0].masku1 = quad[0].masku2 = 0.0f;
				quad[0].v = quad[0].maskv1 = quad[0].maskv2 = 0.0f;
				quad[0].x = 0.0f;
				quad[0].y = 0.0f;
				quad[0].z = 0.0f;

				quad[1].r = quad[1].g = quad[1].b = quad[1].a = 1.0f;
				quad[1].rhw = 1.0f;
				quad[1].u = quad[1].masku1 = quad[1].masku2 = 1.0f;
				quad[1].v = quad[1].maskv1 = quad[1].maskv2 = 0.0f;
				quad[1].x = pLayer->GetWidth();
				quad[1].y = 0.0f;
				quad[1].z = 0.0f;

				quad[2].r = quad[2].g = quad[2].b = quad[2].a = 1.0f;
				quad[2].rhw = 1.0f;
				quad[2].u = quad[2].masku1 = quad[2].masku2 = 1.0f;
				quad[2].v = quad[2].maskv1 = quad[2].maskv2 = 1.0f;
				quad[2].x = pLayer->GetWidth();
				quad[2].y = pLayer->GetHeight();
				quad[2].z = 0.0f;

				quad[3].r = quad[3].g = quad[3].b = quad[3].a = 1.0f;
				quad[3].rhw = 1.0f;
				quad[3].u = quad[3].masku1 = quad[3].masku2 = 0.0f;
				quad[3].v = quad[3].maskv1 = quad[3].maskv2 = 1.0f;
				quad[3].x = 0.0f;
				quad[3].y = pLayer->GetHeight();
				quad[3].z = 0.0f;

				float flXOffset = msgBody.text_shadow().horizontal_offset();
				float flYOffset = msgBody.text_shadow().vertical_offset();

				// Setup shadow fill brush
				CMsgRenderFillBrushCollection shadowBrushCollection;
				CMsgFillBrush *pShadowBrush = shadowBrushCollection.add_fill_brush();
				pShadowBrush->set_opacity( 1.0f );
				pShadowBrush->set_color_rgba( msgBody.text_shadow().color() );

				pVerticalBlurLayer->ActivateRenderTarget();
				pVerticalBlurLayer->Clear();

				// draw background with our shadow fillbrush
				for ( int iTextMaskRegion = 0; iTextMaskRegion < pResult->m_cRangeData; iTextMaskRegion++ )
				{
					if ( pResult->m_pRangeData[iTextMaskRegion].m_hTexture )
					{
						DrawTextRegionRange( pVerticalBlurLayer, x0 + flXOffset, y0 + flYOffset, x1 + flXOffset, y1 + flYOffset, pResult->m_pRangeData[iTextMaskRegion], shadowBrushCollection );

						float flStrength = msgBody.text_shadow().strength();
						while ( flStrength > 1.0f )
						{
							float flOffset = flStrength - 1.0f;

							DrawTextRegionRange( pVerticalBlurLayer, x0 + flXOffset + flOffset, y0 + flYOffset, x1 + flXOffset + flOffset, y1 + flYOffset, pResult->m_pRangeData[iTextMaskRegion], shadowBrushCollection );
							DrawTextRegionRange( pVerticalBlurLayer, x0 + flXOffset - flOffset, y0 + flYOffset, x1 + flXOffset - flOffset, y1 + flYOffset, pResult->m_pRangeData[iTextMaskRegion], shadowBrushCollection );
							DrawTextRegionRange( pVerticalBlurLayer, x0 + flXOffset, y0 + flYOffset + flOffset, x1 + flXOffset, y1 + flYOffset + flOffset, pResult->m_pRangeData[iTextMaskRegion], shadowBrushCollection );
							DrawTextRegionRange( pVerticalBlurLayer, x0 + flXOffset, y0 + flYOffset - flOffset, x1 + flXOffset, y1 + flYOffset - flOffset, pResult->m_pRangeData[iTextMaskRegion], shadowBrushCollection );

							flStrength -= 1.0f;
						}
					}
				}

				float flBlurStdDev = msgBody.text_shadow().blur_radius() / 2;
				float flBlurPasses = 1;
				const float flMaxStdDevPerPass = 6;
				while ( flBlurStdDev > flMaxStdDevPerPass )
				{
					flBlurPasses++;
					flBlurStdDev = flBlurStdDev - flMaxStdDevPerPass;
				}

				if ( m_LastProgram != m_shaderprogramBlur.GetProgram() )
				{
					glUseProgram( m_shaderprogramBlur.GetProgram() );
					m_LastProgram = m_shaderprogramBlur.GetProgram();
				}
				glUniformMatrix4fv( m_shaderprogramBlur.m_matTransformLoc, 1, GL_FALSE, s_flMatrixIdentity );
				// Draw the current layer into horizontal blur surface

				for ( float i = 0.0f; i < flBlurPasses; i += 1.0f )
				{
					float flStdDevThisPass = flBlurStdDev;
					if ( i + 1.0f < flBlurPasses )
						flStdDevThisPass = flMaxStdDevPerPass;

					pHorizontalBlurLayer->ActivateRenderTarget();
					pHorizontalBlurLayer->Clear();

					glUniform1f( m_shaderprogramBlur.m_viewportWidthLoc, (float)pHorizontalBlurLayer->GetWidth() );
					glUniform1f( m_shaderprogramBlur.m_viewportHeightLoc, (float)pHorizontalBlurLayer->GetHeight() );
					glUniform1f( m_shaderprogramBlur.m_blurStdDevLoc, flStdDevThisPass );
					glUniform2f( m_shaderprogramBlur.m_blurDirection, 1.0f / pHorizontalBlurLayer->GetWidth(), 0.0f );

					// Draw the current layer into horizontal blur surface
					DrawTexturedQuadInternal( m_shaderprogramBlur, pVerticalBlurLayer->GetOGLTextureID(), quad, 1.0, 1.0, 0.0f );

					// now render the blurred layer back onto the base surface
					pVerticalBlurLayer->ActivateRenderTarget();
					pVerticalBlurLayer->Clear();

					glUniform1f( m_shaderprogramBlur.m_viewportWidthLoc, (float)pVerticalBlurLayer->GetWidth() );
					glUniform1f( m_shaderprogramBlur.m_viewportHeightLoc, (float)pVerticalBlurLayer->GetHeight() );
					glUniform1f( m_shaderprogramBlur.m_blurStdDevLoc, flStdDevThisPass );
					glUniform2f( m_shaderprogramBlur.m_blurDirection, 1.0f / pVerticalBlurLayer->GetWidth(), 0.0f );

					// now draw back in using vertical blur
					DrawTexturedQuadInternal( m_shaderprogramBlur, pHorizontalBlurLayer->GetOGLTextureID(), quad, 1.0, 1.0, 0.0f );
				}

				// Add blur layer back to free layers list
				m_stackCompositionLayers.Remove( m_stackCompositionLayers.Count() - 1 );
				m_stackCompositionLayers.Remove( m_stackCompositionLayers.Count() - 1 );

				pLayer->ActivateRenderTarget();

				// now draw back in using vertical blur
				DrawTexturedQuadInternal( m_shaderprogramBlur, pVerticalBlurLayer->GetOGLTextureID(), quad, 1.0, 1.0, 0.0f );

				m_FreeLayers.Insert( pHorizontalBlurLayer, pHorizontalBlurLayer, m_flCurrentRenderFrameTime );
				m_FreeLayers.Insert( pVerticalBlurLayer, pVerticalBlurLayer, m_flCurrentRenderFrameTime );
			}

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
				if ( pResult->m_pRangeData[iTextMaskRegion].m_hTexture )
					DrawTextRegionRange( pLayer, x0, y0, x1, y1, pResult->m_pRangeData[iTextMaskRegion], *pFillBrush );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Switch to the window framebuffer and reset related state
//-----------------------------------------------------------------------------
void COpenGLSurface::ActivateRenderTarget()
{
//	Msg( "Surface %p -> ActivateRenderTarget() : fbo %i -> fbo %i \n", this, hCurrentFBO, m_hSystemFBO );

	if ( m_LastFBOActive == m_hSystemFBO )
		return;

	VPROF_BUDGET( "COpenGLSurface::ActivateRenderTarget ", VPROF_BUDGETGROUP_TENFOOT );


	m_LastFBOActive = m_hSystemFBO;

	// use this layer's FBO
	glBindFramebufferEXT( GL_FRAMEBUFFER_EXT, m_hSystemFBO );

	// set the viewport to the whole framebuffer
	glViewport( 0, 0, m_unWindowWidth, m_unWindowHeight );

	//
	// None of the below ever changes/is actually needed, so this just describes our expected state,
	// don't actually call any of it.
	//

	// use sRGB if it's enabled
	//if ( s_bSRGB )
	//	glEnable( GL_FRAMEBUFFER_SRGB_EXT );
	//else
	//	glDisable( GL_FRAMEBUFFER_SRGB_EXT );

	// we do not use depth
	//glDepthRange( 0, 1 );
	//glDisable( GL_DEPTH_TEST );
	// we do not use stencil
	//glDisable( GL_STENCIL_TEST );

	// we do not use scissor, DrawFancyQuad and any other draw calls should scissor quads before drawing
	//glDisable( GL_SCISSOR_TEST );
	
	// we do not use alpha test (deprecated with shaders)
	//glDisable( GL_ALPHA_TEST );
}


//-----------------------------------------------------------------------------
// Purpose: draw the mouse cursor on the screen
//-----------------------------------------------------------------------------
void COpenGLSurface::DrawMouseCursor( uint32 nMouseTextureID, const Vector2D &ptHotspot )
{
	// draw the mouse cursor on the screen as our last act before present
	VPROF_BUDGET( "COpenGLSurface::DrawMouseCursor - mouse cursor", VPROF_BUDGETGROUP_TENFOOT );

	// pump the frame loop for the cursor
#ifndef WIN32
	m_pCursorRender->RunRenderFrame( m_hSDLWindow, m_flCurrentRenderFrameTime, m_unSurfaceWidth, m_unSurfaceHeight, m_bEnforceAspectRatio );
#else
	SDL_SysWMinfo SDLWMinfo;
	memset( &SDLWMinfo, 0, sizeof( SDLWMinfo ) );
	SDL_VERSION( &SDLWMinfo.version );
	if ( !SDL_GetWindowWMInfo( m_hSDLWindow, &SDLWMinfo ) )
		Error( " SDL_GetWindowWMInfo failed" );

	m_pCursorRender->RunRenderFrame( SDLWMinfo.info.win.window, m_flCurrentRenderFrameTime, m_unSurfaceWidth, m_unSurfaceHeight, m_bEnforceAspectRatio );
#endif
	if ( m_pCursorRender->BCursorVisible() )
	{
		Vector2D pt = m_pCursorRender->GetRenderCursorPosition();
		float flOpacity = m_pCursorRender->GetCursorOpacity();

		// check the cursor is inside our screen
		IUITexture *pTexture  = GetOGLTextureForTextureID( nMouseTextureID );	
		if ( !pTexture )
		{
			AssertMsgOnce( false, "Invalid textureid to COpenGLSurface::DrawMouseCursor" );
			return;
		}

		float u0 = 0.0f;
		float u1 = 1.0f;
		
		float v0 = 0.0f;
		float v1 = 1.0f;
	   
		float flScaledCursorWidth = pTexture->GetTextureWidth() * GetWindowScaleFactor();
		float flScaledCursorHeight = pTexture->GetTextureHeight() * GetWindowScaleFactor();

		int x0 = pt.x - ptHotspot.x * flScaledCursorWidth;
		int x1 = x0 + flScaledCursorWidth;
		
		int y0 = pt.y - ptHotspot.y * flScaledCursorHeight;
		int y1 = y0 + flScaledCursorHeight;
		
		FancyQuadParameters_t FancyQuad;
		memset( &FancyQuad, 0, sizeof( FancyQuad ) );
		FancyQuad.m_flZ = 0.0f;
		FancyQuad.m_flVertexMin[0] = x0;
		FancyQuad.m_flVertexMin[1] = y0;
		FancyQuad.m_flVertexMax[0] = x1;
		FancyQuad.m_flVertexMax[1] = y1;
		FancyQuad.m_flTexCoordMin[0] = u0;
		FancyQuad.m_flTexCoordMin[1] = v0;
		FancyQuad.m_flTexCoordMax[0] = u1;
		FancyQuad.m_flTexCoordMax[1] = v1;
		FancyQuad.m_flOpacityTexCoordMin[0] = u0;
		FancyQuad.m_flOpacityTexCoordMin[1] = v0;
		FancyQuad.m_flOpacityTexCoordMax[0] = u1;
		FancyQuad.m_flOpacityTexCoordMax[1] = v1;
		FancyQuad.m_flInnerCornerRadii[0][0] = 0.0f;
		FancyQuad.m_flInnerCornerRadii[0][1] = 0.0f;
		FancyQuad.m_flInnerCornerRadii[1][0] = 0.0f;
		FancyQuad.m_flInnerCornerRadii[1][1] = 0.0f;
		FancyQuad.m_flInnerCornerRadii[2][0] = 0.0f;
		FancyQuad.m_flInnerCornerRadii[2][1] = 0.0f;
		FancyQuad.m_flInnerCornerRadii[3][0] = 0.0f;
		FancyQuad.m_flInnerCornerRadii[3][1] = 0.0f;
		FancyQuad.m_flOuterCornerRadii[0][0] = 0.0f;
		FancyQuad.m_flOuterCornerRadii[0][1] = 0.0f;
		FancyQuad.m_flOuterCornerRadii[1][0] = 0.0f;
		FancyQuad.m_flOuterCornerRadii[1][1] = 0.0f;
		FancyQuad.m_flOuterCornerRadii[2][0] = 0.0f;
		FancyQuad.m_flOuterCornerRadii[2][1] = 0.0f;
		FancyQuad.m_flOuterCornerRadii[3][0] = 0.0f;
		FancyQuad.m_flOuterCornerRadii[3][1] = 0.0f;
		FancyQuad.m_flBorderWidth[0] = 0.0f;
		FancyQuad.m_flBorderWidth[1] = 0.0f;
		FancyQuad.m_flBorderWidth[2] = 0.0f;
		FancyQuad.m_flBorderWidth[3] = 0.0f;
		FancyQuad.m_flBorderColor[0] = 0.0f;
		FancyQuad.m_flBorderColor[1] = 0.0f;
		FancyQuad.m_flBorderColor[2] = 0.0f;
		FancyQuad.m_flBorderColor[3] = 0.0f;
		FancyQuadBrush_t FancyBrush;
		memset( &FancyBrush, 0, sizeof( FancyBrush ) );
		FancyBrush.m_flColor[0][0] = flOpacity;
		FancyBrush.m_flColor[0][1] = flOpacity;
		FancyBrush.m_flColor[0][2] = flOpacity;
		FancyBrush.m_flColor[0][3] = flOpacity;
		FancyBrush.m_flGradientStartPoint[0] = 0.0f;
		FancyBrush.m_flGradientStartPoint[1] = 0.0f;
		FancyBrush.m_flGradientEndPoint[0] = 0.0f;
		FancyBrush.m_flGradientEndPoint[1] = 0.0f;
		FancyBrush.m_flGradientRadii[0] = 0.0f;
		FancyBrush.m_flGradientRadii[1] = 0.0f;
		FancyBrush.m_bIsLinearGradient = false;
		FancyBrush.m_bIsRadialGradient = false;
		
		COpenGLTexture *pOGLTexture = dynamic_cast< COpenGLTexture * >( pTexture );
		Assert( pOGLTexture );
		DrawFancyQuad( pOGLTexture->GetOGLTextureID(), 0, 0, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, m_unSurfaceWidth, m_unSurfaceHeight, FancyQuad, FancyBrush, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f, pTexture->GetAlphaChannelType() != k_EAlphaChannelType_PreMultiplied, false, false, false, s_flMatrixIdentity );
	}
}

void COpenGLSurface::DrawSteamPadPointer( SteamPadPointer_t *pPointer, int padX, int padY )
{
	VPROF_BUDGET( "COpenGLSurface::DrawSteamPadPointer", VPROF_BUDGETGROUP_TENFOOT );

	if ( pPointer->bVisible == false )
		return;
	
	FancyQuadParameters_t FancyQuad;
	memset( &FancyQuad, 0, sizeof( FancyQuad ) );
	
	FancyQuadBrush_t FancyBrush;
	memset( &FancyBrush, 0, sizeof( FancyBrush ) );
	
	const float flOpacity = pPointer->flOpacity;
	
	const float u0 = 0.0f;
	const float u1 = 1.0f;
	
	const float v0 = 0.0f;
	const float v1 = 1.0f;
	
	IUITexture *pTexture = GetOGLTextureForTextureID( pPointer->nTextureID );
	
	if ( !pTexture )
	{
		AssertMsgOnce( false, "Invalid textureid to COpenGLSurface::DrawSteamPadPointer" );
		return;
	}
	
	float flScaledCursorWidth = pTexture->GetTextureWidth() * GetWindowScaleFactor();
	float flScaledCursorHeight = pTexture->GetTextureHeight() * GetWindowScaleFactor();
	
	struct Identity { static float I( float f, bool ) { return f; } };
	auto funcPreRenderCalculatePaddOffset = pPointer->funcPreRenderCalculatePadOffset ? pPointer->funcPreRenderCalculatePadOffset : &Identity::I;

	float controllerX = (*funcPreRenderCalculatePaddOffset)( padX / 32768.0, false ) * pPointer->flRadius + pPointer->vecCenter.x;
	float controllerY = (*funcPreRenderCalculatePaddOffset)( -padY / 32768.0, true ) * pPointer->flRadius + pPointer->vecCenter.y;
	
	float x0 = controllerX - flScaledCursorWidth / 2.0;
	float x1 = x0 + flScaledCursorWidth;
	
	float y0 = controllerY - flScaledCursorHeight / 2.0;
	float y1 = y0 + flScaledCursorHeight;
	
	FancyQuad.m_flZ = 0.0f;
	FancyQuad.m_flTexCoordMin[0] = u0;
	FancyQuad.m_flTexCoordMin[1] = v0;
	FancyQuad.m_flTexCoordMax[0] = u1;
	FancyQuad.m_flTexCoordMax[1] = v1;
	FancyQuad.m_flOpacityTexCoordMin[0] = u0;
	FancyQuad.m_flOpacityTexCoordMin[1] = v0;
	FancyQuad.m_flOpacityTexCoordMax[0] = u1;
	FancyQuad.m_flOpacityTexCoordMax[1] = v1;
	FancyQuad.m_flInnerCornerRadii[0][0] = 0.0f;
	FancyQuad.m_flInnerCornerRadii[0][1] = 0.0f;
	FancyQuad.m_flInnerCornerRadii[1][0] = 0.0f;
	FancyQuad.m_flInnerCornerRadii[1][1] = 0.0f;
	FancyQuad.m_flInnerCornerRadii[2][0] = 0.0f;
	FancyQuad.m_flInnerCornerRadii[2][1] = 0.0f;
	FancyQuad.m_flInnerCornerRadii[3][0] = 0.0f;
	FancyQuad.m_flInnerCornerRadii[3][1] = 0.0f;
	FancyQuad.m_flOuterCornerRadii[0][0] = 0.0f;
	FancyQuad.m_flOuterCornerRadii[0][1] = 0.0f;
	FancyQuad.m_flOuterCornerRadii[1][0] = 0.0f;
	FancyQuad.m_flOuterCornerRadii[1][1] = 0.0f;
	FancyQuad.m_flOuterCornerRadii[2][0] = 0.0f;
	FancyQuad.m_flOuterCornerRadii[2][1] = 0.0f;
	FancyQuad.m_flOuterCornerRadii[3][0] = 0.0f;
	FancyQuad.m_flOuterCornerRadii[3][1] = 0.0f;
	FancyQuad.m_flBorderWidth[0] = 0.0f;
	FancyQuad.m_flBorderWidth[1] = 0.0f;
	FancyQuad.m_flBorderWidth[2] = 0.0f;
	FancyQuad.m_flBorderWidth[3] = 0.0f;
	FancyQuad.m_flBorderColor[0] = 0.0f;
	FancyQuad.m_flBorderColor[1] = 0.0f;
	FancyQuad.m_flBorderColor[2] = 0.0f;
	FancyQuad.m_flBorderColor[3] = 0.0f;

	FancyBrush.m_flColor[0][0] = flOpacity;
	FancyBrush.m_flColor[0][1] = flOpacity;
	FancyBrush.m_flColor[0][2] = flOpacity;
	FancyBrush.m_flColor[0][3] = flOpacity;
	FancyBrush.m_flGradientStartPoint[0] = 0.0f;
	FancyBrush.m_flGradientStartPoint[1] = 0.0f;
	FancyBrush.m_flGradientEndPoint[0] = 0.0f;
	FancyBrush.m_flGradientEndPoint[1] = 0.0f;
	FancyBrush.m_flGradientRadii[0] = 0.0f;
	FancyBrush.m_flGradientRadii[1] = 0.0f;
	FancyBrush.m_bIsLinearGradient = false;
	FancyBrush.m_bIsRadialGradient = false;
	
	FancyQuad.m_flVertexMin[0] = x0;
	FancyQuad.m_flVertexMin[1] = y0;
	FancyQuad.m_flVertexMax[0] = x1;
	FancyQuad.m_flVertexMax[1] = y1;
		
	COpenGLTexture *pOGLTexture = dynamic_cast< COpenGLTexture * >( pTexture );
	Assert( pOGLTexture );
	DrawFancyQuad( pOGLTexture->GetOGLTextureID(), 0, 0, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, m_unSurfaceWidth, m_unSurfaceHeight, FancyQuad, FancyBrush, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f, pTexture->GetAlphaChannelType() != k_EAlphaChannelType_PreMultiplied, false, false, false, s_flMatrixIdentity );
}


//-----------------------------------------------------------------------------
// Purpose: EndFrame
//-----------------------------------------------------------------------------
void COpenGLSurface::PresentBackBuffer()
{
	if ( !IUIEngine::BIsRenderingToTexture( m_eRenderTarget ) )
	{
		SDL_GL_SwapWindow( m_hSDLWindow );
	}
	else
	{
		if ( m_dwTargetOverlayPID == 0 )
			return;
		
		if ( m_nOverlayTextureID == 0 )
			m_nOverlayTextureID = ++s_unNextOverlayTextureID;
		
		if ( !m_pBackBufferSharedMemStream )
		{
			m_pBackBufferSharedMemStream = new CSharedMemStream( CFmtStr1024( "GameOverlayRender_SharedTex_%d_%d", m_nOverlayTextureID, m_dwTargetOverlayPID ).Access(), SHMEMSTREAM_SIZE_ONE_MBYTE * 16, 100 );
		}
		
		if ( !m_pBackBufferSharedMemEvent )
		{
			m_pBackBufferSharedMemEvent = IPC::CreateEvent( CFmtStr1024( "GameOverlayRender_SharedTexRead_%d_%d", m_nOverlayTextureID, m_dwTargetOverlayPID ).Access(), false, false, NULL );
		}

		if( !m_pBackBufferSharedMemWriteEvent )
		{
			m_pBackBufferSharedMemWriteEvent = IPC::CreateEvent( CFmtStr1024( "GameOverlayRender_SharedTexWrite_%d_%d", m_nOverlayTextureID, m_dwTargetOverlayPID ).Access(), false, false, NULL );
		}

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
		
		CCompositionLayer *pLayer = m_stackCompositionLayers[ m_stackCompositionLayers.Count() - 1];

		glBindFramebufferEXT( GL_FRAMEBUFFER_EXT, pLayer->GetOGLFBOHandle() );
		SetLastActiveFBO( pLayer->GetOGLFBOHandle() );
		
		// read pixels from framebuffer to PBO
		// glReadPixels() should return immediately.
		glBindBuffer( GL_PIXEL_PACK_BUFFER, m_hOverlayPBO[m_iCurrentOverlayPBO] );
		CHECK_GL_ERRORS();
		glReadPixels( 0, 0, m_unWindowWidth, m_unWindowHeight, GL_BGRA, GL_UNSIGNED_BYTE, 0 );
		CHECK_GL_ERRORS();
		
		m_iCurrentOverlayPBO = (m_iCurrentOverlayPBO+1)%V_ARRAYSIZE(m_hOverlayPBO);
		
		// map the PBO to process its data by CPU
		glBindBuffer( GL_PIXEL_PACK_BUFFER, m_hOverlayPBO[m_iCurrentOverlayPBO] );
		CHECK_GL_ERRORS();
		GLubyte* ptr = (GLubyte*)glMapBuffer( GL_PIXEL_PACK_BUFFER, GL_READ_ONLY );
		CHECK_GL_ERRORS();
		if( ptr )
		{
			RenderUpdateSharedTextureHeader_t header;
			
			header.m_unMagic = k_unUpdateSharedTextureMagicNumber;
			header.m_unHeight = m_unWindowHeight;
			header.m_unWidth = m_unWindowWidth;
			header.m_unRowPitch = m_unWindowWidth*4;
			header.m_format = (ETextureFormat)m_eOverlayTextureFormat;
			
			/*
			bool bRun = false;
			if ( bRun )
			{
				GLubyte *buffer = ( GLubyte * ) malloc( m_unWindowWidth * m_unWindowHeight * 4 + 18 );
				memset( buffer, 0, 18 );
				buffer[2] = 2;          // uncompressed type
				buffer[12] = (m_unWindowWidth >> 0) & 0xFF;
				buffer[13] = (m_unWindowWidth >> 8) & 0xFF;
				buffer[14] = (m_unWindowHeight >> 0) & 0xFF;
				buffer[15] = (m_unWindowHeight >> 8) & 0xFF;
				buffer[16] = 32;        // pixel size
				buffer[17] = 8; // 8 bits of alpha
				V_memcpy( &buffer[18], ptr, m_unWindowWidth * m_unWindowHeight * 4  );
				char name[1024];
				name[1023] = 0;
				snprintf( name, sizeof( name ) - 1, "/tmp/panorama_%03i_%03if.tga", s_nDebugDumpTexture_FrameNumber, s_nDebugDumpTexture_Number );
				FILE *file = fopen( name, "wb" );
				fwrite( buffer, 1, m_unWindowWidth * m_unWindowHeight * 4 + 18, file );
				fclose( file );
				free( buffer );
				Msg( "WROTE %s\n", name );
				s_nDebugDumpTexture_Number++;
			}*/
			
			if ( (ETextureFormat)m_eOverlayTextureFormat != (int32) k_ETextureBGRA8 )
			{
				header.m_unRowPitch = m_unWindowWidth*4;
				m_pBackBufferSharedMemStream->Put( &header, sizeof( RenderUpdateSharedTextureHeader_t ) );
				
				m_bufPBO.EnsureCapacity( m_unWindowWidth * 4 );
				
				byte * pData = (byte*)ptr;
				for( int32 y=m_unWindowHeight-1; y >=0; --y )
				{
					pData = (byte*)ptr + (y*header.m_unRowPitch);
					byte *pOut = (byte*)m_bufPBO.Base();
					for ( uint32 x=0; x < m_unWindowWidth; ++ x )
					{
						byte *pPixel = pData + x*4;
						if ( (ETextureFormat)m_eOverlayTextureFormat == k_ETextureRGBA8 )
						{
							*(pOut) = *(pPixel+2);
							*(pOut+1) = *(pPixel+1);
							*(pOut+2) = *(pPixel);
							*(pOut+3) = *(pPixel+3);
						}
						else
						{
							AssertMsg( false, "Uknown texture format desired by overlay" );
						}
						pOut += 4;
					}
					if ( m_pBackBufferSharedMemStream->Put( m_bufPBO.Base(), m_unWindowWidth * 4 ) < m_unWindowWidth * 4 )
					{
						// Other side is probably dead/hung
						break;
					}
					m_bufPBO.Clear();
				}
			}
			else
			{
				header.m_unRowPitch = m_unWindowWidth*4;
				m_pBackBufferSharedMemStream->Put( &header, sizeof( RenderUpdateSharedTextureHeader_t ) );
				
				byte * pData = (byte*)ptr;
				for( int32 y=m_unWindowHeight-1; y >=0; --y )
				{
					pData = (byte*)ptr + (y*header.m_unRowPitch);
					
					if ( m_pBackBufferSharedMemStream->Put( pData, m_unWindowWidth * 4 ) < m_unWindowWidth * 4 )
					{
						// Other side is probably dead/hung
						break;
					}
				}
			}
			
			glUnmapBuffer( GL_PIXEL_PACK_BUFFER );

			m_pBackBufferSharedMemWriteEvent->SetEvent();
		}
		
		// back to conventional pixel operation
		glBindBuffer( GL_PIXEL_PACK_BUFFER, 0 );
		
		glBindFramebufferEXT( GL_FRAMEBUFFER_EXT, 0 );
		SetLastActiveFBO( 0 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Dump OpenGL texture to disk for debugging purposes
//-----------------------------------------------------------------------------
#if 0
static void DebugDumpTextureWin32( COpenGLSurface *pSurface, int texnum, const char *suffix )
{
	GLint oldtexnum = 0;
	GLint width = 0, height = 0;
	glFlush(); // just for good measure
	glGetIntegerv( GL_TEXTURE_BINDING_2D, &oldtexnum );
	pSurface->SetTexture( GL_TEXTURE0_ARB, texnum );
	glGetTexLevelParameteriv( GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width );
	glGetTexLevelParameteriv( GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height );
	if ( width * height > 0 )
	{
		GLubyte *buffer = (GLubyte *)malloc( width * height * 4 + 18 );
		memset( buffer, 0, 18 );
		buffer[2] = 2;          // uncompressed type
		buffer[12] = (width >> 0) & 0xFF;
		buffer[13] = (width >> 8) & 0xFF;
		buffer[14] = (height >> 0) & 0xFF;
		buffer[15] = (height >> 8) & 0xFF;
		buffer[16] = 32;        // pixel size
		buffer[17] = 0x28; // 8 bits of alpha, flip vertical
		glGetTexImage( GL_TEXTURE_2D, 0, GL_BGRA, GL_UNSIGNED_BYTE, buffer + 18 );
		char name[1024];
		name[1023] = 0;
		V_snprintf( name, sizeof( name ) - 1, "u:\\panorama_%03i_%03it_%d_%s.tga", s_nDebugDumpTexture_FrameNumber, s_nDebugDumpTexture_Number, texnum, suffix );
		FILE *file = fopen( name, "wb" );
		fwrite( buffer, 1, width * height * 4 + 18, file );
		fclose( file );
		free( buffer );
		Msg( "WROTE %s\n", name );
		s_nDebugDumpTexture_Number++;
	}
	// restore texture binding
	pSurface->SetTexture( GL_TEXTURE0_ARB, oldtexnum );
}
#endif


//-----------------------------------------------------------------------------
// Purpose: EndFrame
//-----------------------------------------------------------------------------
void COpenGLSurface::EndFrame( const CRenderMsg< CMsgEndFrame > &renderCommand )
{
	VPROF_BUDGET( "COpenGLSurface::EndFrame", VPROF_BUDGETGROUP_TENFOOT );
	
	GL_CHECK_CURRENT_CONTEXT;

	Assert( m_stackCompositionLayers.Count() == 1 );
	CCompositionLayer *pLayer = m_stackCompositionLayers[ m_stackCompositionLayers.Count() - 1];

	AssertMsgOnce( pLayer->BIsDrawing(), "Need to still be drawing for the cursor" );

	pLayer->ActivateRenderTarget();

	// render the mouse cursor into the main FBO
	int32 nMouseTextureID = 0;
	if ( renderCommand.BodyConst().has_mouse_cursor_texture_id() )
		nMouseTextureID = renderCommand.BodyConst().mouse_cursor_texture_id();
	if ( nMouseTextureID != 0 )
	{
		Vector2D ptHotspot;
		ptHotspot.x = renderCommand.BodyConst().mouse_cursor_hotspot_x();
		ptHotspot.y = renderCommand.BodyConst().mouse_cursor_hotspot_y();
		DrawMouseCursor( nMouseTextureID, ptHotspot );
	}
	
	// In the same fashion, draw any steam controller cursors
#if !defined( PANORAMA_PUBLIC_STEAM_SDK )
	{
		SteamControllerStateInternal_t controllerState;
		
		ClientControllerLocal()->GetControllerState( m_leftSteamPadPointer.iControllerID, &controllerState );
		
		if ( controllerState.ulButtons & STEAM_LEFTPAD_FINGERDOWN_MASK )
			DrawSteamPadPointer( &m_leftSteamPadPointer, controllerState.sLeftPadX, controllerState.sLeftPadY );
		
		ClientControllerLocal()->GetControllerState( m_rightSteamPadPointer.iControllerID, &controllerState );
		
		if ( controllerState.ulButtons & STEAM_RIGHTPAD_FINGERDOWN_MASK )
			DrawSteamPadPointer( &m_rightSteamPadPointer, controllerState.sRightPadX, controllerState.sRightPadY );
	}
#endif

	// pop MUST be after the cursor drawing above so we are still in a draw call
	pLayer->PopClipLayersAndFlush();	
	
	// now draw all our accrued fbo image to the back buffer
	ActivateRenderTarget();

	CHECK_GL_ERRORS();
	FancyQuadParameters_t FancyQuad;
	memset( &FancyQuad, 0, sizeof( FancyQuad ) );
	FancyQuad.m_flZ = 0.0f;
	if ( BBackBufferScalingNeeded() )
	{
		FancyQuad.m_flVertexMin[0] = m_flTranslateBackbufferX;
		FancyQuad.m_flVertexMin[1] = m_flTranslateBackbufferY;
		FancyQuad.m_flVertexMax[0] = RoundFloatToInt(m_unSurfaceWidth*m_flScaleBackbufferX + m_flTranslateBackbufferX);
		FancyQuad.m_flVertexMax[1] = RoundFloatToInt(m_unSurfaceHeight*m_flScaleBackbufferY + m_flTranslateBackbufferY);
	}
	else
	{
		FancyQuad.m_flVertexMin[0] = 0.0f;
		FancyQuad.m_flVertexMin[1] = 0.0f;
		FancyQuad.m_flVertexMax[0] = m_unSurfaceWidth;
		FancyQuad.m_flVertexMax[1] = m_unSurfaceHeight;
	}
	
	FancyQuad.m_flTexCoordMin[0] = 0.0f;
	FancyQuad.m_flTexCoordMin[1] = 1.0f; // flip image
	FancyQuad.m_flTexCoordMax[0] = 1.0f;
	FancyQuad.m_flTexCoordMax[1] = 0.0f;
	FancyQuad.m_flOpacityTexCoordMin[0] = 0.0f;
	FancyQuad.m_flOpacityTexCoordMin[1] = 0.0f;
	FancyQuad.m_flOpacityTexCoordMax[0] = 0.0f;
	FancyQuad.m_flOpacityTexCoordMax[1] = 0.0f;
	FancyQuad.m_flInnerCornerRadii[0][0] = 0.0f;
	FancyQuad.m_flInnerCornerRadii[0][1] = 0.0f;
	FancyQuad.m_flInnerCornerRadii[1][0] = 0.0f;
	FancyQuad.m_flInnerCornerRadii[1][1] = 0.0f;
	FancyQuad.m_flInnerCornerRadii[2][0] = 0.0f;
	FancyQuad.m_flInnerCornerRadii[2][1] = 0.0f;
	FancyQuad.m_flInnerCornerRadii[3][0] = 0.0f;
	FancyQuad.m_flInnerCornerRadii[3][1] = 0.0f;
	FancyQuad.m_flOuterCornerRadii[0][0] = 0.0f;
	FancyQuad.m_flOuterCornerRadii[0][1] = 0.0f;
	FancyQuad.m_flOuterCornerRadii[1][0] = 0.0f;
	FancyQuad.m_flOuterCornerRadii[1][1] = 0.0f;
	FancyQuad.m_flOuterCornerRadii[2][0] = 0.0f;
	FancyQuad.m_flOuterCornerRadii[2][1] = 0.0f;
	FancyQuad.m_flOuterCornerRadii[3][0] = 0.0f;
	FancyQuad.m_flOuterCornerRadii[3][1] = 0.0f;
	FancyQuadBrush_t FancyBrush;
	memset( &FancyBrush, 0, sizeof( FancyBrush ) );
	FancyBrush.m_flColor[0][0] = 1.0f;
	FancyBrush.m_flColor[0][1] = 1.0f;
	FancyBrush.m_flColor[0][2] = 1.0f;
	FancyBrush.m_flColor[0][3] = 1.0f;
	FancyBrush.m_flGradientStartPoint[0] = 0.0f;
	FancyBrush.m_flGradientStartPoint[1] = 0.0f;
	FancyBrush.m_flGradientEndPoint[0] = 0.0f;
	FancyBrush.m_flGradientEndPoint[1] = 0.0f;
	FancyBrush.m_flGradientRadii[0] = 0.0f;
	FancyBrush.m_flGradientRadii[1] = 0.0f;
	FancyBrush.m_bIsLinearGradient = false;
	FancyBrush.m_bIsRadialGradient = false;
	DrawFancyQuad( pLayer->GetOGLTextureID(), 0, 0, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, m_unWindowWidth, m_unWindowHeight, FancyQuad, FancyBrush, 1.0f, 1.0f, 0.0f, -1.0f, -1.0f, false, false, false, true, s_flMatrixIdentity );
	CHECK_GL_ERRORS();

	DebugDumpFramebuffer();
	DebugDumpState( __func__ );
	
	{
		VPROF_BUDGET( "GL::Present - vsync sleep", VPROF_BUDGETGROUP_TENFOOT );
		PresentBackBuffer();
		CHECK_GL_ERRORS();
	}

	CHECK_GL_ERRORS();
	glUseProgram( 0 );
	m_LastProgram = 0;
	CHECK_GL_ERRORS();

	VPROF_BUDGET( "EndFrame - post present", VPROF_BUDGETGROUP_TENFOOT );

	// Delete any YUV420 textures we've been told to free
	while( m_tsQueueTextureDeletes.Count() )
	{
		VPROF_BUDGET( "EndFrame - tex delete", VPROF_BUDGETGROUP_TENFOOT );

		uint32 unYUV420ID = 0;
		if( m_tsQueueTextureDeletes.PopItem( &unYUV420ID ) )
		{
			AUTO_LOCK( m_lockTextureMap );

			short iMap = m_mapTextures.Find( unYUV420ID );
			if ( iMap != m_mapTextures.InvalidIndex() )
			{
				IUITexture *pTexture = m_mapTextures[iMap];
				delete pTexture;
				m_mapTextures.RemoveAt( iMap );
				m_vecPendingYUVCreates.FindAndRemove( static_cast<COpenGLDoubleBufferedYUV420Texture *>(pTexture) );
				m_vecPendingDoubleBufferCreates.FindAndRemove( static_cast<COpenGLDoubleBufferedTexture *>(pTexture) ); 
				m_vecPBOTextures.FindAndRemove( static_cast<COpenGLTexture *>(pTexture));
			}
			else
			{
				AssertMsg( false, "YUV420 texture in delete list doesn't exist.  Bad delete call previously?" );
			}
		}
	}

	double flNow = m_flCurrentRenderFrameTime;
	{
		VPROF_BUDGET( "EndFrame - composition lru", VPROF_BUDGETGROUP_TENFOOT );
		m_FreeLayers.Purge( flNow - 2.0f );
	}

	{
		VPROF_BUDGET( "EndFrame - reserved lru", VPROF_BUDGETGROUP_TENFOOT );

		AUTO_LOCK( m_MutexReservedLayers );
		m_ReservedLayers.Purge(flNow - 3.0f);
	}

	{
		VPROF_BUDGET( "EndFrame - shadow lru", VPROF_BUDGETGROUP_TENFOOT );

		m_ShadowLayers.Purge( flNow - 1.0f );
	}
	
#if 0
	if ( GetAsyncKeyState( VK_F4 ) && m_flCurrentRenderFrameTime - m_LastTextTextureDumpTime > 4.0f )
	{
		m_LastTextTextureDumpTime = m_flCurrentRenderFrameTime;
		FOR_EACH_VEC( m_vecAllTextBitmaps, i )
		{
			DebugDumpTextureWin32( this, m_vecAllTextBitmaps[i], "textpage" );
		}
	}
#endif


	{
		VPROF_BUDGET( "EndFrame - opacity lru", VPROF_BUDGETGROUP_TENFOOT );
		m_pTextLayoutDrawCache->DeleteOlderEntriesToTextureCache( flNow - 1.0f, m_pTextTextureCache );
	}

	if ( m_vecFreeFBOs.Count() > 0 && m_vecFreeFBOs.Count() > s_convarPanoramaMinFreeFBO.GetInt() )
	{
		glDeleteFramebuffersEXT( 1, &(m_vecFreeFBOs[m_vecFreeFBOs.Count()-1]) );
		m_vecFreeFBOs.Remove( m_vecFreeFBOs.Count()-1 );
		--m_unTotalFBOs;
	}

	m_FrameTimer.End();
	m_nLastFrameMillisecondsIndex++;
	if ( m_nLastFrameMillisecondsIndex >= V_ARRAYSIZE( m_rgflMillisecondsFrame ) )
		m_nLastFrameMillisecondsIndex = 0;

	m_rgflMillisecondsFrame[ m_nLastFrameMillisecondsIndex ] = m_FrameTimer.GetDuration().GetMillisecondsF();
	m_FrameTimer.Start();
}


//-----------------------------------------------------------------------------
// Purpose: Gets framerate average
//-----------------------------------------------------------------------------
float COpenGLSurface::GetFPSAverage()
{
	double flSum = 0.0f;
	int nDivisor = 0;
	for ( int i = 0; i < V_ARRAYSIZE( m_rgflMillisecondsFrame ); ++i )
	{
		if ( m_rgflMillisecondsFrame[i] != FLT_MAX )
		{
			++nDivisor;
			flSum += m_rgflMillisecondsFrame[i];
		}
	}

	return 1000.0f / ( ( float ) ( ( double )flSum / ( double )nDivisor ) );
}


//-----------------------------------------------------------------------------
// Purpose: Get the FPS average since creation
//-----------------------------------------------------------------------------
float COpenGLSurface::GetSessionFPSAverages()
{
	// BUGBUG - alfred, need real impl here
	return GetFPSAverage();
}


//-----------------------------------------------------------------------------
// Purpose: Free FBO object for reuse
//-----------------------------------------------------------------------------
void COpenGLSurface::FreeFBO( uint32 hFBO )
{
	GL_CHECK_CURRENT_CONTEXT;
	if ( m_vecFreeFBOs.Count() < s_convarPanoramaMaxFreeFBO.GetInt() )
	{
		m_vecFreeFBOs.AddToTail(hFBO); 
	} 
	else 
	{
		glDeleteFramebuffersEXT(1, &hFBO);
		--m_unTotalFBOs;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get a free FBO to re-use, or allocate more if needed
//-----------------------------------------------------------------------------
uint32 COpenGLSurface::GetFreeFBO()
{
	VPROF_BUDGET( "COpenGLSurface::GetFreeFBO", VPROF_BUDGETGROUP_TENFOOT );
	if ( m_vecFreeFBOs.Count() == 0 )
	{
		VPROF_BUDGET( "COpenGLSurface::GetFreeFBO - alloc", VPROF_BUDGETGROUP_TENFOOT );
		uint32 rgFBOs[10];
		int numFBOs = s_convarPanoramaFBOAllocBatch.GetInt();
		Assert(V_ARRAYSIZE(rgFBOs) >= numFBOs);
		Assert(numFBOs > 0);
		glGenFramebuffersEXT( numFBOs, &rgFBOs[0] );
		m_vecFreeFBOs.AddMultipleToTail(numFBOs, &rgFBOs[0]);
		m_unTotalFBOs+=numFBOs;
	}

	if ( m_vecFreeFBOs.Count() ) 
	{
		uint32 hFBO = m_vecFreeFBOs[ m_vecFreeFBOs.Count()-1 ];
		m_vecFreeFBOs.Remove( m_vecFreeFBOs.Count()-1 );
		return hFBO;
	}
	else
	{
		AssertMsg( false, "No FBO free, couldn't create?" );
		return 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Allocates and initializes a new FBO 
//-----------------------------------------------------------------------------
uint32 COpenGLSurface::CreateFBO(float width, float height, GLuint *pTextureId, bool bRetry) 
{ 
	VPROF_BUDGET( "COpenGLSurface::CreateFBO", VPROF_BUDGETGROUP_TENFOOT );
	GLuint hFBO = GetFreeFBO();
	CHECK_GL_ERRORS();

	SetLastActiveFBO( 0 );
	glBindFramebufferEXT( GL_FRAMEBUFFER_EXT, hFBO );
	
	// If we hit the FBO limit on NVIDIA+Linux, it'll set an error here instead
	// of failing FBO completeness below.
	{
		VPROF_BUDGET( "COpenGLSurface::CreateFBO - check oom", VPROF_BUDGETGROUP_TENFOOT );
		if ( glGetError() == GL_OUT_OF_MEMORY )
		{
			FreeFBO(hFBO);

			if ( bRetry == false )
			{
				Msg("CreateFBO: retry FBO allocation failed, giving up\n");
				return 0; 
			}
			Msg("CreateFBO: FBO allocation failed, purging cache\n");
			//
			// free the FBO caches and try one more time.
			//
			DropLayerCaches( true );
			DeleteFBOFreeList();

			hFBO = CreateFBO(width, height, pTextureId, false);
			if ( hFBO )
			{
				Msg("CreateFBO: retry FBO allocation succeeded!\n");
			}
			return hFBO;
		}
	}
	CHECK_GL_ERRORS();

	// create a texture object
	GLuint textureId = CreateOpenGLTextureId();

#ifdef OSX
	glTexParameteri( GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_FALSE ); // automatic mipmap
#endif
	CHECK_GL_ERRORS();

	//Assert( search.GetWidth() < 1500 && search.GetHeight() < 1500 );

	{
		VPROF_BUDGET( "COpenGLSurface::CreateFBO - glTexImage2D", VPROF_BUDGETGROUP_TENFOOT );
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0 );
	}
	CHECK_GL_ERRORS();

	// attach the texture to FBO color attachment point, have one FBO per popup for now
	{
		VPROF_BUDGET( "COpenGLSurface::CreateFBO - glFramebufferTexture2DEXT", VPROF_BUDGETGROUP_TENFOOT );
		glFramebufferTexture2DEXT( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D,  textureId, 0 );
	}

	CHECK_GL_ERRORS();

	{
		VPROF_BUDGET( "COpenGLSurface::CreateFBO - status check", VPROF_BUDGETGROUP_TENFOOT );
		GLenum status = glCheckFramebufferStatusEXT( (GLenum)GL_FRAMEBUFFER_EXT );
		if ( status != GL_FRAMEBUFFER_COMPLETE_EXT )
		{
			glDeleteTextures(1, &textureId);
			FreeFBO(hFBO);

			if ( bRetry )
			{
				Msg("CreateFBO: FBO allocation failed, purging cache\n");
				//
				// free the FBO caches and try one more time.
				//
				DropLayerCaches( true );
				DeleteFBOFreeList();

				if (bRetry) 
				{
					hFBO = CreateFBO(width, height, pTextureId, false);
					if ( hFBO )
					{
						Msg("CreateFBO: retry FBO allocation succeeded!\n");
					}
					return hFBO;
				}
			}
			else
			{
				Msg("CreateFBO: retry FBO allocation failed, giving up\n");
			}
			return 0; 
		}
	}

	*pTextureId = textureId;
	return hFBO;
}


//-----------------------------------------------------------------------------
// Purpose: Delete any free FBO handles in the free list
//-----------------------------------------------------------------------------
void COpenGLSurface::DeleteFBOFreeList()
{
	GL_CHECK_CURRENT_CONTEXT;

	FOR_EACH_VEC(m_vecFreeFBOs, i)
	{
		uint32 hFBO = m_vecFreeFBOs[i];
		glDeleteFramebuffersEXT( 1, &hFBO );
		CHECK_GL_ERRORS();
	}
	m_unTotalFBOs -= m_vecFreeFBOs.Count();
	if ( m_vecFreeFBOs.Count() > 0 )
		Msg( "DeleteFBOFreeList: Removing %u FBO objects, total %u\n", m_vecFreeFBOs.Count(), m_unTotalFBOs );
	m_vecFreeFBOs.RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose: Delete any cached composition layer lists that could be taking up FBOs
//-----------------------------------------------------------------------------
void COpenGLSurface::DropLayerCaches( bool bInsideFrameRendering )
{
	{
		AUTO_LOCK( m_MutexReservedLayers );
		// you can't remove all the layers here, the animation thread does an async call to ask about layers and
		// we may have said they are cached, so instead just purge any that weren't pinged by the last frame
		if ( bInsideFrameRendering )
		{
			m_ReservedLayers.Purge( m_flCurrentRenderFrameTime - 0.1f );
		}
		else
		{
			// If we know we're outside a frame, we can remove everything
			m_ReservedLayers.DeleteAll();
		}
	}
	m_ShadowLayers.DeleteAll();
	m_FreeLayers.DeleteAll();
}

//-----------------------------------------------------------------------------
// Purpose: Find a matching free composition layer, or create a new one
//-----------------------------------------------------------------------------
CCompositionLayer * COpenGLSurface::GetCompositionLayer( float width, float height )
{
	CCompositionLayer search(width,height);
	CCompositionLayer *pLayer = NULL;
	pLayer = m_FreeLayers.FindAndRemove( &search, m_flCurrentRenderFrameTime );
	if ( pLayer == NULL )
	{
		VPROF_BUDGET( "COpenGLSurface::GetCompositionLayer - create new", VPROF_BUDGETGROUP_TENFOOT );	

		GL_CHECK_CURRENT_CONTEXT;
		pLayer = new CCompositionLayer( this, m_hGLContext, search.GetWidth(), search.GetHeight() );
		pLayer->Clear(); // make sure we initialize the layer to empty, on OSX this isn't automatic
	}
	
	if ( pLayer )
	{
		pLayer->CheckAndClearClipLayers();
	}
	return pLayer;
}

//-----------------------------------------------------------------------------
// Purpose: Called to push a new compositing layer
//-----------------------------------------------------------------------------
void COpenGLSurface::ClearShaderResourceVariables()
{
}


//-----------------------------------------------------------------------------
// Purpose: Explict free of a composition layer
//-----------------------------------------------------------------------------
void COpenGLSurface::FreeCompositingLayer( const CRenderMsg<CMsgFreeCompositingLayer> &renderCommand )
{
	AUTO_LOCK( m_MutexReservedLayers );
	CCompositionLayer * pLayer = m_ReservedLayers.Find( renderCommand.BodyConst().layer_id(), m_flCurrentRenderFrameTime );
	if ( pLayer )
	{
		m_ReservedLayers.Remove( renderCommand.BodyConst().layer_id() );
		m_FreeLayers.Insert( pLayer, pLayer, m_flCurrentRenderFrameTime );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called to ping a composition layer, verifying it's reserved for re-use, and 
// increasing it's expiration so it doesn't LRU for a bit.
//-----------------------------------------------------------------------------
bool COpenGLSurface::PingCompositingLayer( uint64 ulLayerID, float flWidth, float flHeight )
{
	AUTO_LOCK( m_MutexReservedLayers );
	return m_ReservedLayers.Touch(ulLayerID,flWidth,flHeight,m_flCurrentRenderFrameTime);
}


//-----------------------------------------------------------------------------
// Purpose: Called to push a new compositing layer
//-----------------------------------------------------------------------------
void COpenGLSurface::PushCompositingLayer( const CRenderMsg< CMsgPushCompositingLayer > &renderCommand )
{
	VPROF_BUDGET( "COpenGLSurface::PushCompositingLayer", VPROF_BUDGETGROUP_TENFOOT );
	GL_CHECK_CURRENT_CONTEXT;

	if ( m_stackCompositionLayers.Count() )
	{
		CCompositionLayer *pLayer = m_stackCompositionLayers[m_stackCompositionLayers.Count()-1];
		pLayer->PopClipLayersAndFlush();
	}

	const CMsgPushCompositingLayer &msgBody = renderCommand.BodyConst();

	CCompositionLayer *pLayer = NULL;
	{
		VPROF_BUDGET( "COpenGLSurface::PushCompositingLayer - find", VPROF_BUDGETGROUP_TENFOOT );
		AUTO_LOCK( m_MutexReservedLayers );
		pLayer = m_ReservedLayers.Find( msgBody.layer_id(), m_flCurrentRenderFrameTime );
		if ( pLayer )
		{
			//
			// Check to make sure it matches our size
			// 

			if ( (pLayer->GetWidth() != ceil( msgBody.width() )) || (pLayer->GetHeight() != ceil( msgBody.height() )) )
			{
				m_ReservedLayers.Remove( msgBody.layer_id() );
				// Free up the layer, since it no longer matches our size, should only happen if also actually redrawing
				Assert( msgBody.needs_clear() );
				m_FreeLayers.Insert( pLayer, pLayer, m_flCurrentRenderFrameTime );
				pLayer = NULL;
			}
		}
	}

	if ( !pLayer )
	{
		pLayer = GetCompositionLayer( msgBody.width(), msgBody.height() );
	}
	
	float flWidthDiscard = ( ceil( msgBody.width() ) - msgBody.width() ) / ceil( msgBody.width() );
	float flHeightDiscard = ( ceil( msgBody.height() ) - msgBody.height() ) / ceil( msgBody.height() );

	// Should now be valid, unless D3D actually failed...
	if ( pLayer )
	{
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
		pQuad[0].masku1 = pQuad[0].masku2 = pQuad[0].u = 0.0f;
		pQuad[0].maskv1 = pQuad[0].maskv2 = pQuad[0].v = ( 1.0f - flHeightDiscard );
		pQuad[0].x = msgBody.layer_quad_top_left_x();
		pQuad[0].y = msgBody.layer_quad_top_left_y();
		pQuad[0].z = msgBody.layer_quad_top_left_z();

		pQuad[1].r = r;
		pQuad[1].g = g;
		pQuad[1].b = b;
		pQuad[1].a = flOpacity;
		pQuad[1].rhw = 1.0f;
		pQuad[1].masku1 = pQuad[1].masku2 = pQuad[1].u = ( 1.0f - flWidthDiscard );
		pQuad[1].maskv1 = pQuad[1].maskv2 = pQuad[1].v = ( 1.0f - flHeightDiscard );
		pQuad[1].x = msgBody.layer_quad_top_right_x();
		pQuad[1].y = msgBody.layer_quad_top_right_y();
		pQuad[1].z = msgBody.layer_quad_top_right_z();

		pQuad[2].r = r;
		pQuad[2].g = g;
		pQuad[2].b = b;
		pQuad[2].a = flOpacity;
		pQuad[2].rhw = 1.0f;
		pQuad[2].masku1 = pQuad[2].masku2 = pQuad[2].u = ( 1.0f - flWidthDiscard );
		pQuad[2].maskv1 = pQuad[2].maskv2 = pQuad[2].v = 0.0f;
		pQuad[2].x = msgBody.layer_quad_bottom_right_x();
		pQuad[2].y = msgBody.layer_quad_bottom_right_y();
		pQuad[2].z = msgBody.layer_quad_bottom_right_z();

		pQuad[3].r = r;
		pQuad[3].g = g;
		pQuad[3].b = b;
		pQuad[3].a = flOpacity;
		pQuad[3].rhw = 1.0f;
		pQuad[3].masku1 = pQuad[3].masku2 = pQuad[3].u = 0.0f;
		pQuad[3].maskv1 = pQuad[3].maskv2 = pQuad[3].v = 0.0f;
		pQuad[3].x = msgBody.layer_quad_bottom_left_x();
		pQuad[3].y = msgBody.layer_quad_bottom_left_y();
		pQuad[3].z = msgBody.layer_quad_bottom_left_z();

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
			//Msg( "msg layer %p : %s %s %5.2f %5.2f %5.2f %5.2f %08x\n", pLayer, shadow.inset() ? "true " : "false", shadow.fill() ? "true " : "false", shadow.horizontal_offset(), shadow.vertical_offset(), shadow.blur_radius(), shadow.spread_distance(), shadow.color() );
			pLayer->SetBoxShadow( shadow.inset(), shadow.fill(), shadow.horizontal_offset(), shadow.vertical_offset(), 
				shadow.blur_radius(), shadow.spread_distance(), shadow.color(), shadow.animating() );
		}
		else
		{
			pLayer->SetBoxShadow( false, false, 0.0f, 0.0f, 0.0f, 0.0f, 0, false );
		}

		pLayer->SetBlurValues( msgBody.gaussianblur_passes(), msgBody.gaussianblur_stddevhor(), msgBody.gaussianblur_stddevver() );

		pLayer->Set2DScaleFactors( msgBody.scale_2d_factors_x(), msgBody.scale_2d_factors_y() );

		ClearShaderResourceVariables();

		// Add to stack to keep track of this layer
		m_stackCompositionLayers.AddToTail( pLayer );

#if defined( ACTUALLY_CHECK_GL_ERRORS )
		{
			VPROF_BUDGET( "COpenGLSurface::PushCompositingLayer - debug checks", VPROF_BUDGETGROUP_TENFOOT );
			CHECK_GL_ERRORS();
			SetLastActiveFBO( 0 );
			glBindFramebufferEXT( GL_FRAMEBUFFER_EXT, pLayer->GetOGLFBOHandle() );
			CHECK_GL_ERRORS();

			// check FBO status
			GLenum status = glCheckFramebufferStatusEXT( GL_FRAMEBUFFER_EXT );
			CHECK_GL_ERRORS();
			if( status != GL_FRAMEBUFFER_COMPLETE_EXT )
			{
				AssertMsgOnce( false, "Failed to attach FBO\n" );
				Warning( "Failed to attach FBO\n" );
				return;
			}
		}
#endif

		// We don't actually clear and begin draw until we receive the clear message for the layer
		if ( msgBody.needs_clear() )
			ClearCompositingLayer( pLayer );

		CHECK_GL_ERRORS();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get shader resource view for an opacity mask
//-----------------------------------------------------------------------------
GLuint COpenGLSurface::GetOpacityMaskShaderResourceViewForTexture( uint32 unTextureID )
{
	VPROF_BUDGET( "COpenGLSurface::GetOpacityMaskShaderResourceViewForTexture", VPROF_BUDGETGROUP_TENFOOT );

	IUITexture *pTexture = NULL;
	{
		AUTO_LOCK( m_lockTextureMap );
		short iMap = m_mapTextures.Find( unTextureID );
		if ( iMap != m_mapTextures.InvalidIndex() )
			pTexture = m_mapTextures[iMap];
	}
	if ( pTexture )
	{
		COpenGLTexture *pOGLTexture = dynamic_cast< COpenGLTexture * >( pTexture );
		Assert( pOGLTexture );
		return pOGLTexture->GetOGLTextureID();
	}
	
	return 0;
}


//-----------------------------------------------------------------------------
// Purpose: Called to create outer shadow layer for a given layer
//-----------------------------------------------------------------------------
CCompositionLayer *COpenGLSurface::GetOuterShadowLayer( CCompositionLayer *pSourceLayer, bool bFill, float flHorOffset, float flVerOffset, float flBlurRadius, float flSpreadDistance, uint32 shadowColor, bool bAnimating )
{
	VPROF_BUDGET( "COpenGLSurface::GetOuterShadowLayer", VPROF_BUDGETGROUP_TENFOOT );	
	GL_CHECK_CURRENT_CONTEXT;

	CCompositionLayer *pShadowOutLayer = NULL;

	ShadowLayerKey_t key;
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

	if ( !bAnimating )
	{
		pShadowOutLayer = m_ShadowLayers.Find( key, m_flCurrentRenderFrameTime );;
		if ( pShadowOutLayer != NULL )
		{
			return pShadowOutLayer;
		}
	}


//	if ( shadowColor == 0xffaf9654 )
//		Msg( "new   %.0fx%.0f %s %s %5.2f %5.2f %5.2f %5.2f %08x\n", key.m_flBaseWidth, key.m_flBaseHeight, key.m_bInset ? "true" : "false", key.m_bFill ? "true" : "false",  key.m_flHorOffset, key.m_flVerOffset, key.m_flBlurRadius, key.m_flSpreadDistance, key.m_shadowColor );

	if ( shadowColor != 0x00000000 )
	{

		float flPadding = ceil( flBlurRadius );
		// Make sure to bloat the original layer size by even dimensions for the
		// glow/shadow layer, or we won't be able to render in the exact center
		// and will end up with a shadow that jitters around when animated.
		int iSpreadDistance = int( flSpreadDistance + 2 ) & ~1;
		float flWidth = pSourceLayer->GetWidth() + iSpreadDistance + (flPadding * 2.0f);
		float flHeight = pSourceLayer->GetHeight() + iSpreadDistance + (flPadding * 2.0f);

		float flNewWidth = (uint32)flWidth + (32 - (uint32)flWidth % 32 );
		float flNewHeight = (uint32)flHeight + ( 32 - (uint32)flHeight % 32 );

		float flPaddingHor = flPadding + (flNewWidth - flWidth)/2;
		float flPaddingVer = flPadding + (flNewHeight - flHeight)/2;

		flWidth = flNewWidth;
		flHeight = flNewHeight;

		GL_CHECK_CURRENT_CONTEXT;
		// Note: Composition layer will ceil() width/height, that's ok, we'll just draw slightly stretched, but when we draw into
		// the parent context we'll squish back down appropriately, which will result in pretty good linearly interpolated results for
		// sub pixel shadow boundaries.
		//CCompositionLayer *pColorLayer = NULL;
		CCompositionLayer *pHorizontalBlurLayer = NULL;
		{
			pShadowOutLayer = GetCompositionLayer( flWidth, flHeight  );
			pHorizontalBlurLayer = GetCompositionLayer( flWidth, flHeight  );
			pShadowOutLayer->Clear();
			pHorizontalBlurLayer->Clear();
		}

		//{
		//	CCompositionLayer search( this, m_hGLContext, 0, 0, pSourceLayer->GetWidth(), pSourceLayer->GetHeight() );
		//	pColorLayer = GetCompositionLayer( search );
		//}

		m_stackCompositionLayers.AddToTail( pShadowOutLayer );

		float flshadowcolor[4];
		LinearColorFromABGR( flshadowcolor[0], flshadowcolor[1], flshadowcolor[2], flshadowcolor[3], shadowColor, true );
		flshadowcolor[0] *= flshadowcolor[3];
		flshadowcolor[1] *= flshadowcolor[3];
		flshadowcolor[2] *= flshadowcolor[3];

		GL_CHECK_CURRENT_CONTEXT;
		CHECK_GL_ERRORS();

//		glDisable( GL_BLEND ); // forestw: we don't need blending for these stages
		// clear the color layer to the color we want
//		pColorLayer->ActivateRenderTarget();
//		glClearColor( flshadowcolor[0], flshadowcolor[1], flshadowcolor[2], flshadowcolor[3] );
//		glClear( GL_COLOR_BUFFER_BIT );
		// and clear the outer shadow layer to blank
		pShadowOutLayer->ActivateRenderTarget();
		glClearColor( 0.0, 0.0, 0.0, 0.0 );
		glClear( GL_COLOR_BUFFER_BIT );

		DebugDumpFramebuffer();
		DebugDumpState( __func__ );
		
		// We draw a bloated primitive around the original border; we want to
		// offset the corner radii by the size difference so that they still
		// align vertically and horizontally
		float flCornerOffsetHor = ((pShadowOutLayer->GetWidth() - pSourceLayer->GetWidth()) / 2) - flPaddingHor;
		float flCornerOffsetVer = ((pShadowOutLayer->GetHeight() - pSourceLayer->GetHeight()) / 2) - flPaddingVer;

		// set up fancyquad to draw with corner rounding
		FancyQuadParameters_t FancyQuad;
		memset( &FancyQuad, 0, sizeof( FancyQuad ) );
		FancyQuad.m_flZ = 0.0f;
		FancyQuad.m_flVertexMin[0] = flPaddingHor;
		FancyQuad.m_flVertexMin[1] = flPaddingVer;
		FancyQuad.m_flVertexMax[0] = pShadowOutLayer->GetWidth() - flPaddingHor;
		FancyQuad.m_flVertexMax[1] = pShadowOutLayer->GetHeight() - flPaddingVer;
		FancyQuad.m_flTexCoordMin[0] = 0.0f;
		FancyQuad.m_flTexCoordMin[1] = 1.0f; // flip image
		FancyQuad.m_flTexCoordMax[0] = 1.0f;
		FancyQuad.m_flTexCoordMax[1] = 0.0f;
		FancyQuad.m_flOpacityTexCoordMin[0] = 0.0f;
		FancyQuad.m_flOpacityTexCoordMin[1] = 0.0f;
		FancyQuad.m_flOpacityTexCoordMax[0] = 0.0f;
		FancyQuad.m_flOpacityTexCoordMax[1] = 0.0f;
		FancyQuad.m_flInnerCornerRadii[0][0] = 0.0f;
		FancyQuad.m_flInnerCornerRadii[0][1] = 0.0f;
		FancyQuad.m_flInnerCornerRadii[1][0] = 0.0f;
		FancyQuad.m_flInnerCornerRadii[1][1] = 0.0f;
		FancyQuad.m_flInnerCornerRadii[2][0] = 0.0f;
		FancyQuad.m_flInnerCornerRadii[2][1] = 0.0f;
		FancyQuad.m_flInnerCornerRadii[3][0] = 0.0f;
		FancyQuad.m_flInnerCornerRadii[3][1] = 0.0f;
		FancyQuad.m_flOuterCornerRadii[0][0] = key.m_rgBorderRadii[0] + flCornerOffsetHor;
		FancyQuad.m_flOuterCornerRadii[0][1] = key.m_rgBorderRadii[1] + flCornerOffsetVer;
		FancyQuad.m_flOuterCornerRadii[1][0] = key.m_rgBorderRadii[2] + flCornerOffsetHor;
		FancyQuad.m_flOuterCornerRadii[1][1] = key.m_rgBorderRadii[3] + flCornerOffsetVer;
		FancyQuad.m_flOuterCornerRadii[2][0] = key.m_rgBorderRadii[4] + flCornerOffsetHor;
		FancyQuad.m_flOuterCornerRadii[2][1] = key.m_rgBorderRadii[5] + flCornerOffsetVer;
		FancyQuad.m_flOuterCornerRadii[3][0] = key.m_rgBorderRadii[6] + flCornerOffsetHor;
		FancyQuad.m_flOuterCornerRadii[3][1] = key.m_rgBorderRadii[7] + flCornerOffsetVer;
		FancyQuad.m_flBorderWidth[0] = 0.0f;
		FancyQuad.m_flBorderWidth[1] = 0.0f;
		FancyQuad.m_flBorderWidth[2] = 0.0f;
		FancyQuad.m_flBorderWidth[3] = 0.0f;
		FancyQuadBrush_t FancyBrush;
		memset( &FancyBrush, 0, sizeof( FancyBrush ) );
		FancyBrush.m_flColor[0][0] = flshadowcolor[0];
		FancyBrush.m_flColor[0][1] = flshadowcolor[1];
		FancyBrush.m_flColor[0][2] = flshadowcolor[2];
		FancyBrush.m_flColor[0][3] = flshadowcolor[3];
		FancyBrush.m_flGradientStartPoint[0] = 0.0f;
		FancyBrush.m_flGradientStartPoint[1] = 0.0f;
		FancyBrush.m_flGradientEndPoint[0] = 0.0f;
		FancyBrush.m_flGradientEndPoint[1] = 0.0f;
		FancyBrush.m_flGradientRadii[0] = 0.0f;
		FancyBrush.m_flGradientRadii[1] = 0.0f;
		FancyBrush.m_bIsLinearGradient = false;
		FancyBrush.m_bIsRadialGradient = false;
		// Draw color layer into shadow output layer, with opacity mask from original layer to get correct border box.
		DrawFancyQuad( 0, 0, 0, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, pShadowOutLayer->GetWidth(), pShadowOutLayer->GetHeight(), FancyQuad, FancyBrush, 1.0f, 1.0f, 0.0f, -1.0f, -1.0f, false, false, false, false, s_flMatrixIdentity );

		if( flBlurRadius > 0.0f )
		{
			// The shadow layer now contains the correctly sized and shaped shadow, blur needs to be applied before it is drawn into parent layer.
			m_stackCompositionLayers.Remove( m_stackCompositionLayers.Count()-1 );
			m_stackCompositionLayers.AddToTail( pHorizontalBlurLayer );

			VertexTextured_t blurquad[4];
			blurquad[0].r = blurquad[0].g = blurquad[0].b = blurquad[0].a = 1.0f;
			blurquad[0].rhw = 1.0f;
			blurquad[0].masku1 = blurquad[0].masku2 = blurquad[0].u = 0.0f;
			blurquad[0].maskv1 = blurquad[0].maskv2 = blurquad[0].v = 1.0f;
			blurquad[0].x = 0.0f;
			blurquad[0].y = 0.0f;
			blurquad[0].z = 0.0f;

			blurquad[1].r = blurquad[1].g = blurquad[1].b = blurquad[1].a = 1.0f;
			blurquad[1].rhw = 1.0f;
			blurquad[1].masku1 = blurquad[1].masku2 = blurquad[1].u = 1.0f;
			blurquad[1].maskv1 = blurquad[1].maskv2 = blurquad[1].v = 1.0f;
			blurquad[1].x = pHorizontalBlurLayer->GetWidth();
			blurquad[1].y = 0.0f;
			blurquad[1].z = 0.0f;

			blurquad[2].r = blurquad[2].g = blurquad[2].b = blurquad[2].a = 1.0f;
			blurquad[2].rhw = 1.0f;
			blurquad[2].masku1 = blurquad[2].masku2 = blurquad[2].u = 1.0f;
			blurquad[2].maskv1 = blurquad[2].maskv2 = blurquad[2].v = 0.0f;
			blurquad[2].x = pHorizontalBlurLayer->GetWidth();
			blurquad[2].y = pHorizontalBlurLayer->GetHeight();
			blurquad[2].z = 0.0f;

			blurquad[3].r = blurquad[3].g = blurquad[3].b = blurquad[3].a = 1.0f;
			blurquad[3].rhw = 1.0f;
			blurquad[3].masku1 = blurquad[3].masku2 = blurquad[3].u = 0.0f;
			blurquad[3].maskv1 = blurquad[3].maskv2 = blurquad[3].v = 0.0f;
			blurquad[3].x = 0.0f;
			blurquad[3].y = pHorizontalBlurLayer->GetHeight();
			blurquad[3].z = 0.0f;

			CHECK_GL_ERRORS();


			// draw a blurred version of the current layer into the horizontalblur surface
			pHorizontalBlurLayer->ActivateRenderTarget();
			glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
			glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

			DebugDumpFramebuffer();
			DebugDumpState( __func__ );

			if ( m_LastProgram != m_shaderprogramBlur.GetProgram() )
			{
				glUseProgram( m_shaderprogramBlur.GetProgram() );
				m_LastProgram = m_shaderprogramBlur.GetProgram();
			}
			glUniformMatrix4fv( m_shaderprogramBlur.m_matTransformLoc, 1, GL_FALSE, s_flMatrixIdentity );
			glUniform1f( m_shaderprogramBlur.m_viewportWidthLoc, ( float )pHorizontalBlurLayer->GetWidth() );
			glUniform1f( m_shaderprogramBlur.m_viewportHeightLoc, ( float )pHorizontalBlurLayer->GetHeight() );
			glUniform1f( m_shaderprogramBlur.m_blurStdDevLoc, flBlurRadius / 2.0f );
			glUniform2f( m_shaderprogramBlur.m_blurDirection, 1.0f / pHorizontalBlurLayer->GetWidth(), 0.0f );
			// Draw the current layer into horizontal blur surface
			DrawTexturedQuadInternal( m_shaderprogramBlur, pShadowOutLayer->GetOGLTextureID(), blurquad, 1.0, 1.0, 0.0f );

			// now render the blurred layer back onto the base surface
			pShadowOutLayer->ActivateRenderTarget();
			glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
			glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

			DebugDumpFramebuffer();
			DebugDumpState( __func__ );

			if ( m_LastProgram != m_shaderprogramBlur.GetProgram() )
			{
				glUseProgram( m_shaderprogramBlur.GetProgram() );
				m_LastProgram = m_shaderprogramBlur.GetProgram();
			}
			glUniformMatrix4fv( m_shaderprogramBlur.m_matTransformLoc, 1, GL_FALSE, s_flMatrixIdentity );
			glUniform1f( m_shaderprogramBlur.m_viewportWidthLoc, ( float )pShadowOutLayer->GetWidth() );
			glUniform1f( m_shaderprogramBlur.m_viewportHeightLoc, ( float )pShadowOutLayer->GetHeight() );
			glUniform1f( m_shaderprogramBlur.m_blurStdDevLoc, flBlurRadius / 2.0f );
			glUniform2f( m_shaderprogramBlur.m_blurDirection, 0.0f, 1.0f / pHorizontalBlurLayer->GetHeight() );
			// now draw the horizontally blurred surface into the original surface using vertical blur
			DrawTexturedQuadInternal( m_shaderprogramBlur, pHorizontalBlurLayer->GetOGLTextureID(), blurquad, 1.0, 1.0, 0.0f );

			CHECK_GL_ERRORS();
		}

		// If we aren't filling the inside of the box with the shadow as well, then clip it
		if ( !bFill )
		{
			float *pflCornerRadii = pSourceLayer->AccessCornerRadii();
			int corners[2][2];
			// forestw: round toward inner rectangle, otherwise we get pixel gaps on scaling glowing main menu buttons
			corners[0][0] = ceil( ( flWidth - pSourceLayer->GetWidth() ) * 0.5f );
			corners[0][1] = ceil( ( flHeight - pSourceLayer->GetHeight() ) * 0.5f );
			corners[1][0] = floor( ( flWidth - pSourceLayer->GetWidth() ) * 0.5f + pSourceLayer->GetWidth() );
			corners[1][1] = floor( ( flHeight - pSourceLayer->GetHeight() ) * 0.5f + pSourceLayer->GetHeight() );

			// forestw: the D3D code uses an opacity mask to cut out the middle when rounding is used, here we just draw another fancyquad to do it
			// set up fancyquad to draw with corner rounding
			//FancyQuadParameters_t FancyQuad;
			memset( &FancyQuad, 0, sizeof( FancyQuad ) );
			FancyQuad.m_flZ = 0.0f;
			FancyQuad.m_flVertexMin[0] = corners[0][0];
			FancyQuad.m_flVertexMin[1] = corners[0][1];
			FancyQuad.m_flVertexMax[0] = corners[1][0];
			FancyQuad.m_flVertexMax[1] = corners[1][1];
			FancyQuad.m_flTexCoordMin[0] = 0.0f;
			FancyQuad.m_flTexCoordMin[1] = 1.0f; // flip image
			FancyQuad.m_flTexCoordMax[0] = 1.0f;
			FancyQuad.m_flTexCoordMax[1] = 0.0f;
			FancyQuad.m_flOpacityTexCoordMin[0] = 0.0f;
			FancyQuad.m_flOpacityTexCoordMin[1] = 0.0f;
			FancyQuad.m_flOpacityTexCoordMax[0] = 0.0f;
			FancyQuad.m_flOpacityTexCoordMax[1] = 0.0f;
			FancyQuad.m_flInnerCornerRadii[0][0] = 0.0f;
			FancyQuad.m_flInnerCornerRadii[0][1] = 0.0f;
			FancyQuad.m_flInnerCornerRadii[1][0] = 0.0f;
			FancyQuad.m_flInnerCornerRadii[1][1] = 0.0f;
			FancyQuad.m_flInnerCornerRadii[2][0] = 0.0f;
			FancyQuad.m_flInnerCornerRadii[2][1] = 0.0f;
			FancyQuad.m_flInnerCornerRadii[3][0] = 0.0f;
			FancyQuad.m_flInnerCornerRadii[3][1] = 0.0f;
			FancyQuad.m_flOuterCornerRadii[0][0] = pflCornerRadii[0];
			FancyQuad.m_flOuterCornerRadii[0][1] = pflCornerRadii[1];
			FancyQuad.m_flOuterCornerRadii[1][0] = pflCornerRadii[2];
			FancyQuad.m_flOuterCornerRadii[1][1] = pflCornerRadii[3];
			FancyQuad.m_flOuterCornerRadii[2][0] = pflCornerRadii[4];
			FancyQuad.m_flOuterCornerRadii[2][1] = pflCornerRadii[5];
			FancyQuad.m_flOuterCornerRadii[3][0] = pflCornerRadii[6];
			FancyQuad.m_flOuterCornerRadii[3][1] = pflCornerRadii[7];
			FancyQuad.m_flBorderWidth[0] = 0.0f;
			FancyQuad.m_flBorderWidth[1] = 0.0f;
			FancyQuad.m_flBorderWidth[2] = 0.0f;
			FancyQuad.m_flBorderWidth[3] = 0.0f;
			//FancyQuadBrush_t FancyBrush;
			memset( &FancyBrush, 0, sizeof( FancyBrush ) );
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
			glBlendFuncSeparate( GL_ZERO, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE_MINUS_SRC_ALPHA );
			DrawFancyQuad( 0, 0, 0, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, pShadowOutLayer->GetWidth(), pShadowOutLayer->GetHeight(), FancyQuad, FancyBrush, 1.0f, 1.0f, 0.0f, -1.0f, -1.0f, false, false, false, false, s_flMatrixIdentity );
			glBlendFuncSeparate( GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA );

			DebugDumpFramebuffer();
			DebugDumpState( __func__ );
		}
		
		m_stackCompositionLayers.Remove( m_stackCompositionLayers.Count()-1 );

		// finally, horizontal blur layer as well
		m_FreeLayers.Insert( pHorizontalBlurLayer, pHorizontalBlurLayer, m_flCurrentRenderFrameTime );
	}

	// Add to list of cached shadow layers
	if ( !bAnimating )
		m_ShadowLayers.Insert( key, pShadowOutLayer, m_flCurrentRenderFrameTime );
	else
		m_FreeLayers.Insert( pShadowOutLayer, pShadowOutLayer, m_flCurrentRenderFrameTime );

	// Shadow layer is returned, and must be put back in free stack by caller
	return pShadowOutLayer;
}


//-----------------------------------------------------------------------------
// Purpose: Called to draw and free a composition layer created with CreateOuterShadowLayer
//-----------------------------------------------------------------------------
void COpenGLSurface::DrawOuterShadowLayer( CCompositionLayer *pParentLayer, CCompositionLayer *pShadowLayer, CCompositionLayer *pSourceLayer, bool bFill, float flHorOffset, float flVerOffset, float flBlurRadius, float flSpreadDistance, uint32 shadowColor )
{
	VPROF_BUDGET( "COpenGLSurface::DrawOuterShadowLayer", VPROF_BUDGETGROUP_TENFOOT );
	GL_CHECK_CURRENT_CONTEXT;
	
	VertexTextured_t *pBoxQuad = pSourceLayer->AccessRenderQuad();
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


	float *pflCornerRadii = pSourceLayer->AccessCornerRadii();
	//float flXInset = ( pShadowLayer->GetWidth() - pSourceLayer->GetWidth() ) / 2.0f;
	//float flYInset = ( pShadowLayer->GetHeight() - pSourceLayer->GetHeight() ) / 2.0f;

	// set up fancyquad to draw with corner rounding
	FancyQuadParameters_t FancyQuad;
	memset( &FancyQuad, 0, sizeof( FancyQuad ) );
	FancyQuad.m_flZ = pBoxQuad[0].z;
	FancyQuad.m_flVertexMin[0] = pBoxQuad[0].x - flPaddingHor + flHorOffset;
	FancyQuad.m_flVertexMin[1] = pBoxQuad[0].y - flPaddingVer + flVerOffset;
	FancyQuad.m_flVertexMax[0] = pBoxQuad[2].x + flPaddingHor + flHorOffset + flSpreadDistance;
	FancyQuad.m_flVertexMax[1] = pBoxQuad[2].y + flPaddingVer + flVerOffset + flSpreadDistance;
	FancyQuad.m_flTexCoordMin[0] = 0.0f;
	FancyQuad.m_flTexCoordMin[1] = 1.0f; // flip image
	FancyQuad.m_flTexCoordMax[0] = 1.0f;
	FancyQuad.m_flTexCoordMax[1] = 0.0f;
	FancyQuad.m_flOpacityTexCoordMin[0] = 0.0f;
	FancyQuad.m_flOpacityTexCoordMin[1] = 0.0f;
	FancyQuad.m_flOpacityTexCoordMax[0] = 0.0f;
	FancyQuad.m_flOpacityTexCoordMax[1] = 0.0f;
	FancyQuad.m_flInnerCornerRadii[0][0] = 0.0f;
	FancyQuad.m_flInnerCornerRadii[0][1] = 0.0f;
	FancyQuad.m_flInnerCornerRadii[1][0] = 0.0f;
	FancyQuad.m_flInnerCornerRadii[1][1] = 0.0f;
	FancyQuad.m_flInnerCornerRadii[2][0] = 0.0f;
	FancyQuad.m_flInnerCornerRadii[2][1] = 0.0f;
	FancyQuad.m_flInnerCornerRadii[3][0] = 0.0f;
	FancyQuad.m_flInnerCornerRadii[3][1] = 0.0f;
	FancyQuad.m_flOuterCornerRadii[0][0] = pflCornerRadii[0];
	FancyQuad.m_flOuterCornerRadii[0][1] = pflCornerRadii[1];
	FancyQuad.m_flOuterCornerRadii[1][0] = pflCornerRadii[2];
	FancyQuad.m_flOuterCornerRadii[1][1] = pflCornerRadii[3];
	FancyQuad.m_flOuterCornerRadii[2][0] = pflCornerRadii[4];
	FancyQuad.m_flOuterCornerRadii[2][1] = pflCornerRadii[5];
	FancyQuad.m_flOuterCornerRadii[3][0] = pflCornerRadii[6];
	FancyQuad.m_flOuterCornerRadii[3][1] = pflCornerRadii[7];
	FancyQuad.m_flBorderWidth[0] = 0.0f;
	FancyQuad.m_flBorderWidth[1] = 0.0f;
	FancyQuad.m_flBorderWidth[2] = 0.0f;
	FancyQuad.m_flBorderWidth[3] = 0.0f;
	FancyQuadBrush_t FancyBrush;
	memset( &FancyBrush, 0, sizeof( FancyBrush ) );
	FancyBrush.m_flColor[0][0] = flOpacity;
	FancyBrush.m_flColor[0][1] = flOpacity;
	FancyBrush.m_flColor[0][2] = flOpacity;
	FancyBrush.m_flColor[0][3] = flOpacity;
	FancyBrush.m_flGradientStartPoint[0] = 0.0f;
	FancyBrush.m_flGradientStartPoint[1] = 0.0f;
	FancyBrush.m_flGradientEndPoint[0] = 0.0f;
	FancyBrush.m_flGradientEndPoint[1] = 0.0f;
	FancyBrush.m_flGradientRadii[0] = 0.0f;
	FancyBrush.m_flGradientRadii[1] = 0.0f;
	FancyBrush.m_bIsLinearGradient = false;
	FancyBrush.m_bIsRadialGradient = false;
	// Draw the current layer into its parent with corner rounding if needed
	DrawFancyQuad( pShadowLayer->GetOGLTextureID(), 0, 0, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, pParentLayer->GetWidth(), pParentLayer->GetHeight(), FancyQuad, FancyBrush, flScale2DX, flScale2DY, flRotate2D, -1.0f, -1.0f, false, false, false, false, pSourceLayer->AccessMatrix() );
}


//-----------------------------------------------------------------------------
// Purpose: Called to create inset shadow layer for a given layer
//-----------------------------------------------------------------------------
CCompositionLayer *COpenGLSurface::GetInsetShadowLayer( CCompositionLayer *pSourceLayer, float flHorOffset, float flVerOffset, float flBlurRadius, float flSpreadDistance, uint32 shadowColor, bool bAnimating )
{
	VPROF_BUDGET( "COpenGLSurface::GetInsetShadowLayer", VPROF_BUDGETGROUP_TENFOOT );
	GL_CHECK_CURRENT_CONTEXT;
	
	ShadowLayerKey_t key;
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

	CCompositionLayer *pShadowOutLayer = m_ShadowLayers.Find( key, m_flCurrentRenderFrameTime );

	if ( pShadowOutLayer != NULL )
	{
		return pShadowOutLayer;
	}

	if ( shadowColor != 0x00000000 )
	{
		float *pflOuterRaddi = pSourceLayer->AccessCornerRadii();
		float *pflBorderWidths = pSourceLayer->AccessBorderWidths();

		float flPadding = ceil( flBlurRadius );
		float flWidth = ceil( flPadding * 2.0f + pSourceLayer->GetWidth() - pflBorderWidths[1] - pflBorderWidths[3] );
		float flHeight = ceil( flPadding * 2.0f + pSourceLayer->GetHeight() - pflBorderWidths[0] - pflBorderWidths[2] );

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
			pShadowOutLayer = GetCompositionLayer( flWidth, flHeight );
			pHorizontalBlurLayer = GetCompositionLayer( flWidth, flHeight );
			pShadowOutLayer->Clear();
			pHorizontalBlurLayer->Clear();
		}

		// Draw the unblurred shadow
		pShadowOutLayer->ActivateRenderTarget();
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
			blurquad[0].maskv1 = blurquad[0].maskv2 = blurquad[0].v = 1.0f;
			blurquad[0].x = 0.0f;
			blurquad[0].y = 0.0f;
			blurquad[0].z = 0.0f;

			blurquad[1].r = blurquad[1].g = blurquad[1].b = blurquad[1].a = 1.0f;
			blurquad[1].rhw = 1.0f;
			blurquad[1].masku1 = blurquad[1].masku2 = blurquad[1].u = 1.0f;
			blurquad[1].maskv1 = blurquad[1].maskv2 = blurquad[1].v = 1.0f;
			blurquad[1].x = pHorizontalBlurLayer->GetWidth();
			blurquad[1].y = 0.0f;
			blurquad[1].z = 0.0f;

			blurquad[2].r = blurquad[2].g = blurquad[2].b = blurquad[2].a = 1.0f;
			blurquad[2].rhw = 1.0f;
			blurquad[2].masku1 = blurquad[2].masku2 = blurquad[2].u = 1.0f;
			blurquad[2].maskv1 = blurquad[2].maskv2 = blurquad[2].v = 0.0f;
			blurquad[2].x = pHorizontalBlurLayer->GetWidth();
			blurquad[2].y = pHorizontalBlurLayer->GetHeight();
			blurquad[2].z = 0.0f;

			blurquad[3].r = blurquad[3].g = blurquad[3].b = blurquad[3].a = 1.0f;
			blurquad[3].rhw = 1.0f;
			blurquad[3].masku1 = blurquad[3].masku2 = blurquad[3].u = 0.0f;
			blurquad[3].maskv1 = blurquad[3].maskv2 = blurquad[3].v = 0.0f;
			blurquad[3].x = 0.0f;
			blurquad[3].y = pHorizontalBlurLayer->GetHeight();
			blurquad[3].z = 0.0f;

			pHorizontalBlurLayer->ActivateRenderTarget();
			glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
			glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

			DebugDumpFramebuffer();
			DebugDumpState( __func__ );

			// setup the blur parameters in the frag shader
			if ( m_LastProgram != m_shaderprogramBlur.GetProgram() )
			{
				glUseProgram( m_shaderprogramBlur.GetProgram() );
				m_LastProgram = m_shaderprogramBlur.GetProgram();
			}
			glUniformMatrix4fv( m_shaderprogramBlur.m_matTransformLoc, 1, GL_FALSE, s_flMatrixIdentity );
			glUniform1f( m_shaderprogramBlur.m_viewportWidthLoc, ( float )pHorizontalBlurLayer->GetWidth() );
			glUniform1f( m_shaderprogramBlur.m_viewportHeightLoc, ( float )pHorizontalBlurLayer->GetHeight() );
			glUniform1f( m_shaderprogramBlur.m_blurStdDevLoc, flBlurRadius/2.0 );
			glUniform2f( m_shaderprogramBlur.m_blurDirection, 1.0f / pShadowOutLayer->GetWidth(), 0.0f );
			// draw the current layer into horizontal blur surface
			DrawTexturedQuadInternal( m_shaderprogramBlur, pShadowOutLayer->GetOGLTextureID(), blurquad, 1.0, 1.0, 0.0f );

			pShadowOutLayer->ActivateRenderTarget();
			glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
			glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

			DebugDumpFramebuffer();
			DebugDumpState( __func__ );

			if ( m_LastProgram != m_shaderprogramBlur.GetProgram() )
			{
				glUseProgram( m_shaderprogramBlur.GetProgram() );
				m_LastProgram = m_shaderprogramBlur.GetProgram();
			}
			glUniformMatrix4fv( m_shaderprogramBlur.m_matTransformLoc, 1, GL_FALSE, s_flMatrixIdentity );
			glUniform1f( m_shaderprogramBlur.m_viewportWidthLoc, ( float )pShadowOutLayer->GetWidth() );
			glUniform1f( m_shaderprogramBlur.m_viewportHeightLoc, ( float )pShadowOutLayer->GetHeight() );
			glUniform1f( m_shaderprogramBlur.m_blurStdDevLoc, flBlurRadius/2.0 );
			glUniform2f( m_shaderprogramBlur.m_blurDirection, 0.0f, 1.0f / pHorizontalBlurLayer->GetHeight() );
			// now draw back to the orignal surface using vertical blur
			DrawTexturedQuadInternal( m_shaderprogramBlur, pHorizontalBlurLayer->GetOGLTextureID(), blurquad, 1.0, 1.0, 0.0f );
		}

		m_stackCompositionLayers.Remove( m_stackCompositionLayers.Count()-1 );

		// finally, horizontal blur layer as well
		m_FreeLayers.Insert( pHorizontalBlurLayer, pHorizontalBlurLayer, m_flCurrentRenderFrameTime );
	}

	// Add to list of cached shadow layers
	m_ShadowLayers.Insert( key, pShadowOutLayer, m_flCurrentRenderFrameTime );

	// Shadow layer is returned, and must be put back in free stack by caller
	return pShadowOutLayer;
}


//-----------------------------------------------------------------------------
// Purpose: Called to draw and free a composition layer created with CreateInsetShadowLayer
//-----------------------------------------------------------------------------
void COpenGLSurface::DrawInsetShadowLayer( CCompositionLayer *pParentLayer, CCompositionLayer *pShadowLayer, CCompositionLayer *pSourceLayer, float flHorOffset, float flVerOffset, float flBlurRadius, float flSpreadDistance, uint32 shadowColor )
{
	VPROF_BUDGET( "COpenGLSurface::DrawInsetShadowLayer", VPROF_BUDGETGROUP_TENFOOT );
	GL_CHECK_CURRENT_CONTEXT;
	
	float *pflOuterRaddi = pSourceLayer->AccessCornerRadii();
	float *pflBorderWidths = pSourceLayer->AccessBorderWidths();
	VertexTextured_t *pBoxQuad = pSourceLayer->AccessRenderQuad();
	float flPadding = ceil( flBlurRadius );
	float flUAdjustment = flPadding / pShadowLayer->GetWidth();
	float flVAdjustment = flPadding / pShadowLayer->GetHeight();
	float flScale2DX, flScale2DY;
	pSourceLayer->Get2DScaleFactors( flScale2DX, flScale2DY );
	float flRotate2D;
	pSourceLayer->Get2DRotate( flRotate2D );
	
	// set up fancyquad to draw with corner rounding
	FancyQuadParameters_t FancyQuad;
	memset( &FancyQuad, 0, sizeof( FancyQuad ) );
	FancyQuad.m_flZ = pBoxQuad[0].z;
	FancyQuad.m_flVertexMin[0] = ( pBoxQuad[0].x + pflBorderWidths[3] );
	FancyQuad.m_flVertexMin[1] = ( pBoxQuad[0].y + pflBorderWidths[0] );
	FancyQuad.m_flVertexMax[0] = ( pBoxQuad[2].x - pflBorderWidths[1] );
	FancyQuad.m_flVertexMax[1] = ( pBoxQuad[2].y - pflBorderWidths[2] );
	FancyQuad.m_flTexCoordMin[0] = flUAdjustment;
	FancyQuad.m_flTexCoordMin[1] = flVAdjustment;
	FancyQuad.m_flTexCoordMax[0] = 1.0f - flUAdjustment;
	FancyQuad.m_flTexCoordMax[1] = 1.0f - flVAdjustment;
	FancyQuad.m_flOpacityTexCoordMin[0] = 0.0f;
	FancyQuad.m_flOpacityTexCoordMin[1] = 0.0f;
	FancyQuad.m_flOpacityTexCoordMax[0] = 0.0f;
	FancyQuad.m_flOpacityTexCoordMax[1] = 0.0f;
	FancyQuad.m_flInnerCornerRadii[0][0] = 0.0f;
	FancyQuad.m_flInnerCornerRadii[0][1] = 0.0f;
	FancyQuad.m_flInnerCornerRadii[1][0] = 0.0f;
	FancyQuad.m_flInnerCornerRadii[1][1] = 0.0f;
	FancyQuad.m_flInnerCornerRadii[2][0] = 0.0f;
	FancyQuad.m_flInnerCornerRadii[2][1] = 0.0f;
	FancyQuad.m_flInnerCornerRadii[3][0] = 0.0f;
	FancyQuad.m_flInnerCornerRadii[3][1] = 0.0f;
	FancyQuad.m_flOuterCornerRadii[0][0] = pflOuterRaddi[0] - pflBorderWidths[3];
	FancyQuad.m_flOuterCornerRadii[0][1] = pflOuterRaddi[1] - pflBorderWidths[0];
	FancyQuad.m_flOuterCornerRadii[1][0] = pflOuterRaddi[2] - pflBorderWidths[1];
	FancyQuad.m_flOuterCornerRadii[1][1] = pflOuterRaddi[3] - pflBorderWidths[0];
	FancyQuad.m_flOuterCornerRadii[2][0] = pflOuterRaddi[4] - pflBorderWidths[1];
	FancyQuad.m_flOuterCornerRadii[2][1] = pflOuterRaddi[5] - pflBorderWidths[2];
	FancyQuad.m_flOuterCornerRadii[3][0] = pflOuterRaddi[6] - pflBorderWidths[3];
	FancyQuad.m_flOuterCornerRadii[3][1] = pflOuterRaddi[7] - pflBorderWidths[2];
	FancyQuad.m_flBorderWidth[0] = 0.0f;
	FancyQuad.m_flBorderWidth[1] = 0.0f;
	FancyQuad.m_flBorderWidth[2] = 0.0f;
	FancyQuad.m_flBorderWidth[3] = 0.0f;
	FancyQuadBrush_t FancyBrush;
	memset( &FancyBrush, 0, sizeof( FancyBrush ) );
	FancyBrush.m_flColor[0][0] = pBoxQuad[3].a; // white premultiplied
	FancyBrush.m_flColor[0][1] = pBoxQuad[3].a;
	FancyBrush.m_flColor[0][2] = pBoxQuad[3].a;
	FancyBrush.m_flColor[0][3] = pBoxQuad[3].a;
	FancyBrush.m_flGradientStartPoint[0] = 0.0f;
	FancyBrush.m_flGradientStartPoint[1] = 0.0f;
	FancyBrush.m_flGradientEndPoint[0] = 0.0f;
	FancyBrush.m_flGradientEndPoint[1] = 0.0f;
	FancyBrush.m_flGradientRadii[0] = 0.0f;
	FancyBrush.m_flGradientRadii[1] = 0.0f;
	FancyBrush.m_bIsLinearGradient = false;
	FancyBrush.m_bIsRadialGradient = false;
	// Draw the current layer into its parent with corner rounding if needed
	DrawFancyQuad( pShadowLayer->GetOGLTextureID(), 0, 0, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, pParentLayer->GetWidth(), pParentLayer->GetHeight(), FancyQuad, FancyBrush, flScale2DX, flScale2DY, flRotate2D, -1.0f, -1.0f, false, false, false, false, pSourceLayer->AccessMatrix() );
}


//-----------------------------------------------------------------------------
// Purpose: Called to clear contents of current compositing layer
//-----------------------------------------------------------------------------
void COpenGLSurface::ClearCompositingLayer( CCompositionLayer *pLayer )
{
    pLayer->Clear();

	if ( m_stackCompositionLayers.Count() == 1 )
		pLayer->PushCliplayersAndBeginDraw( m_flScaleBackbufferX, m_flScaleBackbufferY, m_flTranslateBackbufferX, m_flTranslateBackbufferY );
	else
		pLayer->PushCliplayersAndBeginDraw( 1.0f, 1.0f, 0.0f, 0.0f );

	Assert( pLayer->GetClipLayerCount() == 0 );
}


//-----------------------------------------------------------------------------
// Purpose: Called to pop a compositing layer
//-----------------------------------------------------------------------------
void COpenGLSurface::PopCompositingLayer( const CRenderMsg< CMsgPopCompositingLayer > &renderCommand )
{
	VPROF_BUDGET( "COpenGLSurface::PopCompositingLayer", VPROF_BUDGETGROUP_TENFOOT );
	if ( m_stackCompositionLayers.Count() < 2 )
	{
		AssertMsg( false, "COpenGLSurface::PopCompositingLayer hit with no layers, mismatched push/pop?" );
		return;
	}

	// End draw for layer
	CCompositionLayer *pLayer = m_stackCompositionLayers[m_stackCompositionLayers.Count()-1];

	bool bLayerRedraw = pLayer->BIsDrawing();
	if ( bLayerRedraw )
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
	bool bAnimating;
	uint32 shadowColor;
	pLayer->GetBoxShadow( bInset, bFill, flHorOffset, flVerOffset, flBlurRadius, flSpreadDistance, shadowColor, bAnimating );
	// Msg( "layer %p : %s %s %5.2f %5.2f %5.2f %5.2f %08x\n", pLayer, bInset ? "true " : "false", bFill ? "true " : "false", flHorOffset, flVerOffset, flBlurRadius, flSpreadDistance, shadowColor );

	// If we have an outer box-shadow, need to create a layer for that first
	CCompositionLayer *pShadowOutLayer = NULL;
	CCompositionLayer *pShadowInsetLayer = NULL;

	bool bTransparent = ( ( shadowColor >> 24 ) & 0xff ) == 0 ? true : false;
	if ( bInset == false && !bTransparent )
		pShadowOutLayer = GetOuterShadowLayer( pLayer, bFill, flHorOffset, flVerOffset, flBlurRadius, flSpreadDistance, shadowColor, bAnimating );
	else if ( bInset == true && !bTransparent )
		pShadowInsetLayer = GetInsetShadowLayer( pLayer, flHorOffset, flVerOffset, flBlurRadius, flSpreadDistance, shadowColor, bAnimating );

	float flBlurPasses = 0.0f;
	float flBlurStdDevHor = 0.0f;
	float flBlurStdDevVer = 0.0f;
	pLayer->GetBlurValues( flBlurPasses, flBlurStdDevHor , flBlurStdDevVer );

	// If we have blur (and we redrew), then draw into another layer for blur first
	if ( bLayerRedraw && flBlurPasses > 0.0f && ( flBlurStdDevHor > 0.0f || flBlurStdDevVer > 0.0f ) )
	{
		VPROF_BUDGET( "COpenGLSurface::PopCompositingLayer - Draw Blur", VPROF_BUDGETGROUP_TENFOOT );
		GL_CHECK_CURRENT_CONTEXT;
		CCompositionLayer *pHorizontalBlurLayer = GetCompositionLayer( pLayer->GetWidth(), pLayer->GetHeight() );
		m_stackCompositionLayers.AddToTail( pHorizontalBlurLayer );

		VertexTextured_t quad[4];
		quad[0].r = quad[0].g = quad[0].b = quad[0].a = 1.0f;
		quad[0].rhw = 1.0f;
		quad[0].u = quad[0].masku1 = quad[0].masku2 = 0.0f;
		quad[0].v = quad[0].maskv1 = quad[0].maskv2 = 1.0f;
		quad[0].x = 0.0f;
		quad[0].y = 0.0f;
		quad[0].z = 0.0f;

		quad[1].r = quad[1].g = quad[1].b = quad[1].a = 1.0f;
		quad[1].rhw = 1.0f;
		quad[1].u = quad[1].masku1 = quad[1].masku2 = 1.0f;
		quad[1].v = quad[1].maskv1 = quad[1].maskv2 = 1.0f;
		quad[1].x = pLayer->GetWidth();
		quad[1].y = 0.0f;
		quad[1].z = 0.0f;

		quad[2].r = quad[2].g = quad[2].b = quad[2].a = 1.0f;
		quad[2].rhw = 1.0f;
		quad[2].u = quad[2].masku1 = quad[2].masku2 = 1.0f;
		quad[2].v = quad[2].maskv1 = quad[2].maskv2 = 0.0f;
		quad[2].x = pLayer->GetWidth();
		quad[2].y = pLayer->GetHeight();
		quad[2].z = 0.0f;

		quad[3].r = quad[3].g = quad[3].b = quad[3].a = 1.0f;
		quad[3].rhw = 1.0f;
		quad[3].u = quad[3].masku1 = quad[3].masku2 = 0.0f;
		quad[3].v = quad[3].maskv1 = quad[3].maskv2 = 0.0f;
		quad[3].x = 0.0f;
		quad[3].y = pLayer->GetHeight();
		quad[3].z = 0.0f;


		for( float i = 0.0f; i < flBlurPasses; i += 1.0f )
		{
			GL_CHECK_CURRENT_CONTEXT;
			pHorizontalBlurLayer->ActivateRenderTarget();
			glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
			glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

			DebugDumpFramebuffer();
			DebugDumpState( __func__ );

			// setup the blur parameters in the frag shader
			if ( m_LastProgram != m_shaderprogramBlur.GetProgram() )
			{
				glUseProgram( m_shaderprogramBlur.GetProgram() );
				m_LastProgram = m_shaderprogramBlur.GetProgram();
			}
			glUniformMatrix4fv( m_shaderprogramBlur.m_matTransformLoc, 1, GL_FALSE, s_flMatrixIdentity );
			glUniform1f( m_shaderprogramBlur.m_viewportWidthLoc, ( float )pHorizontalBlurLayer->GetWidth() );
			glUniform1f( m_shaderprogramBlur.m_viewportHeightLoc, ( float )pHorizontalBlurLayer->GetHeight() );
			glUniform1f( m_shaderprogramBlur.m_blurStdDevLoc, flBlurStdDevHor );
			glUniform2f( m_shaderprogramBlur.m_blurDirection, 1.0f / pLayer->GetWidth(), 0.0f );
			// Draw the current layer into horizontal blur surface
			DrawTexturedQuadInternal( m_shaderprogramBlur, pLayer->GetOGLTextureID(), quad, 1.0, 1.0, 0.0f );
			// now render the blurred layer back onto the base surface
			pLayer->ActivateRenderTarget();
			glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
			glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

			DebugDumpFramebuffer();
			DebugDumpState( __func__ );

			if ( m_LastProgram != m_shaderprogramBlur.GetProgram() )
			{
				glUseProgram( m_shaderprogramBlur.GetProgram() );
				m_LastProgram = m_shaderprogramBlur.GetProgram();
			}
			glUniformMatrix4fv( m_shaderprogramBlur.m_matTransformLoc, 1, GL_FALSE, s_flMatrixIdentity );
			glUniform1f( m_shaderprogramBlur.m_viewportWidthLoc, ( float )pLayer->GetWidth() );
			glUniform1f( m_shaderprogramBlur.m_viewportHeightLoc, ( float )pLayer->GetHeight() );
			glUniform1f( m_shaderprogramBlur.m_blurStdDevLoc, flBlurStdDevVer );
			glUniform2f( m_shaderprogramBlur.m_blurDirection, 0.0f, 1.0f / pHorizontalBlurLayer->GetHeight() );
			// now draw back in using vectical blur
			DrawTexturedQuadInternal( m_shaderprogramBlur, pHorizontalBlurLayer->GetOGLTextureID(), quad, 1.0, 1.0, 0.0f );
		}

		// Add blur layer back to free layers list
		m_stackCompositionLayers.Remove( m_stackCompositionLayers.Count()-1 );

		m_FreeLayers.Insert( pHorizontalBlurLayer, pHorizontalBlurLayer, m_flCurrentRenderFrameTime );
	}
	
	CCompositionLayer *pParent = m_stackCompositionLayers[m_stackCompositionLayers.Count()-2];

	// Pop it off the stack and put it in the free list
	{
		AUTO_LOCK( m_MutexReservedLayers );
		CCompositionLayer *p = m_ReservedLayers.Find(pLayer->GetContextID(), m_flCurrentRenderFrameTime);
		if ( p == NULL )
		{
			m_ReservedLayers.Insert( pLayer->GetContextID(), pLayer, m_flCurrentRenderFrameTime );
		}
		else
		{
			Assert( p == pLayer );
		}
	}
	m_stackCompositionLayers.Remove( m_stackCompositionLayers.Count()-1 );
	
	GL_CHECK_CURRENT_CONTEXT;
	
	CHECK_GL_ERRORS();

	// If we had a shadow layer, then it's time to draw it now
	if ( pShadowOutLayer )
	{
		pParent->ActivateRenderTarget();
		DrawOuterShadowLayer( pParent, pShadowOutLayer, pLayer, bFill, flHorOffset, flVerOffset, flBlurRadius, flSpreadDistance, shadowColor  );
	}


	pParent->ActivateRenderTarget();

	float flScale2DX, flScale2DY;
	pLayer->Get2DScaleFactors( flScale2DX, flScale2DY );
	float flRotate2D;
	pLayer->Get2DRotate( flRotate2D );

	// forestw: use fancy quad rendering to apply outer corner clipping, inset shadow (technically a border) and desaturation while compositing
//	const float *pflInnerRadii = pLayer->AccessCornerRadii();
	const float *pflOuterRadii = pLayer->AccessCornerRadii();
	VertexTextured_t *pQuad = pLayer->AccessRenderQuad();
	FancyQuadParameters_t FancyQuad;
	memset( &FancyQuad, 0, sizeof( FancyQuad ) );
	FancyQuad.m_flZ = pQuad[0].z;
	FancyQuad.m_flVertexMin[0] = pQuad[0].x;
	FancyQuad.m_flVertexMin[1] = pQuad[0].y;
	FancyQuad.m_flVertexMax[0] = pQuad[2].x;
	FancyQuad.m_flVertexMax[1] = pQuad[2].y;
	FancyQuad.m_flTexCoordMin[0] = pQuad[0].u;
	FancyQuad.m_flTexCoordMin[1] = pQuad[0].v;
	FancyQuad.m_flTexCoordMax[0] = pQuad[2].u;
	FancyQuad.m_flTexCoordMax[1] = pQuad[2].v;
	FancyQuad.m_flOpacityTexCoordMin[0] = pQuad[0].masku1;
	FancyQuad.m_flOpacityTexCoordMin[1] = pQuad[2].maskv1;
	FancyQuad.m_flOpacityTexCoordMax[0] = pQuad[2].masku1;
	FancyQuad.m_flOpacityTexCoordMax[1] = pQuad[0].maskv1;
	// TODO forestw: we can combine all border handling directly into this compositing step, we don't need pInsetShadowLayer or pBorderImageLayer
	FancyQuad.m_flInnerCornerRadii[0][0] = 0.0f;
	FancyQuad.m_flInnerCornerRadii[0][1] = 0.0f;
	FancyQuad.m_flInnerCornerRadii[1][0] = 0.0f;
	FancyQuad.m_flInnerCornerRadii[1][1] = 0.0f;
	FancyQuad.m_flInnerCornerRadii[2][0] = 0.0f;
	FancyQuad.m_flInnerCornerRadii[2][1] = 0.0f;
	FancyQuad.m_flInnerCornerRadii[3][0] = 0.0f;
	FancyQuad.m_flInnerCornerRadii[3][1] = 0.0f;
	FancyQuad.m_flOuterCornerRadii[0][0] = pflOuterRadii[0];
	FancyQuad.m_flOuterCornerRadii[0][1] = pflOuterRadii[1];
	FancyQuad.m_flOuterCornerRadii[1][0] = pflOuterRadii[2];
	FancyQuad.m_flOuterCornerRadii[1][1] = pflOuterRadii[3];
	FancyQuad.m_flOuterCornerRadii[2][0] = pflOuterRadii[4];
	FancyQuad.m_flOuterCornerRadii[2][1] = pflOuterRadii[5];
	FancyQuad.m_flOuterCornerRadii[3][0] = pflOuterRadii[6];
	FancyQuad.m_flOuterCornerRadii[3][1] = pflOuterRadii[7];
	FancyQuad.m_flBorderWidth[0] = 0.0f;
	FancyQuad.m_flBorderWidth[1] = 0.0f;
	FancyQuad.m_flBorderWidth[2] = 0.0f;
	FancyQuad.m_flBorderWidth[3] = 0.0f;
	float r, g, b, a;
	LinearColorFromABGR( r, g, b, a, shadowColor, true );
	FancyQuad.m_flBorderColor[0] = r * a;
	FancyQuad.m_flBorderColor[1] = g * a;
	FancyQuad.m_flBorderColor[2] = b * a;
	FancyQuad.m_flBorderColor[3] = a;
	FancyQuadBrush_t FancyBrush;
	memset( &FancyBrush, 0, sizeof( FancyBrush ) );
	FancyBrush.m_flColor[0][0] = pQuad[0].r; // already premultiplied
	FancyBrush.m_flColor[0][1] = pQuad[0].g;
	FancyBrush.m_flColor[0][2] = pQuad[0].b;
	FancyBrush.m_flColor[0][3] = pQuad[0].a;
	FancyBrush.m_flGradientStartPoint[0] = 0.0f;
	FancyBrush.m_flGradientStartPoint[1] = 0.0f;
	FancyBrush.m_flGradientEndPoint[0] = 0.0f;
	FancyBrush.m_flGradientEndPoint[1] = 0.0f;
	FancyBrush.m_flGradientRadii[0] = 0.0f;
	FancyBrush.m_flGradientRadii[1] = 0.0f;
	FancyBrush.m_bIsLinearGradient = false;
	FancyBrush.m_bIsRadialGradient = false;
	DrawFancyQuad( pLayer->GetOGLTextureID(), GetOpacityMaskShaderResourceViewForTexture( pLayer->GetOpacityMaskTextureID() ), 0, pLayer->GetSaturation(), pLayer->GetHueShift(), pLayer->GetBrightness(), pLayer->GetContrast(), pLayer->GetOpacityMaskOpacity(), pParent->GetWidth(), pParent->GetHeight(), FancyQuad, FancyBrush, flScale2DX, flScale2DY, flRotate2D, -1.0f, -1.0f, false, false, false, false, pLayer->AccessMatrix() );

	// If we had an inset shadow, then it's time to draw it now
	if ( pShadowInsetLayer )
	{
		pParent->ActivateRenderTarget();
		DrawInsetShadowLayer( pParent, pShadowInsetLayer, pLayer, flHorOffset, flVerOffset, flBlurRadius, flSpreadDistance, shadowColor  );
		CHECK_GL_ERRORS();
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
	CHECK_GL_ERRORS();
}


//-----------------------------------------------------------------------------
// Purpose: Called to push a clipping layer
//-----------------------------------------------------------------------------
void COpenGLSurface::PushClipLayer( const CRenderMsg< CMsgPushClipLayer > &renderCommand )
{
	VPROF_BUDGET( "COpenGLSurface::PushClipLayer", VPROF_BUDGETGROUP_TENFOOT );
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
void COpenGLSurface::PopClipLayer( const CRenderMsg< CMsgPopClipLayer > &renderCommand )
{
	VPROF_BUDGET( "COpenGLSurface::PopClipLayer", VPROF_BUDGETGROUP_TENFOOT );
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


//-----------------------------------------------------------------------------
// Purpose: Get the current clip rect
//-----------------------------------------------------------------------------
void CCompositionLayer::GetCurrentClipRect( RectBounds_t &r )
{
	if ( m_pVecClipLayers && m_pVecClipLayers->Count() > 0 )
	{
		CMsgPushClipLayer &msg = m_pVecClipLayers->Element( m_pVecClipLayers->Count() -1 );
		r.left = msg.top_left().x();
		r.top = msg.top_left().y();
		r.right = msg.bottom_right().x();
		r.bottom = msg.bottom_right().y();
	}
	else
	{
		r.left = 0;
		r.top = 0;
		r.right = m_flLayerWidth;
		r.bottom = m_flLayerHeight;
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
void COpenGLSurface::Validate( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE();

	ValidateObj( m_vecClipLayers );
	ValidateObj( m_FreeLayers );

	ValidateObj( m_stackCompositionLayers ); 	
	FOR_EACH_VEC( m_stackCompositionLayers, i )
	{
		ValidatePtr( m_stackCompositionLayers[i] );
	}

	{
		AUTO_LOCK( m_MutexReservedLayers );
		ValidateObj( m_ReservedLayers );
	}

	ValidateObj( m_ShadowLayers );
    
    m_tsQueueTextureDeletes.ValidateDataStructureOnly( validator, "m_tsQueueTextureDeletes" );
}

#endif
