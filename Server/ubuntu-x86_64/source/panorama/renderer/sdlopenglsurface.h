//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef SDLOPENGLSURFACE_H
#define SDLOPENGLSURFACE_H

#define GL_GLEXT_PROTOTYPES

#include "iui3dsurface.h"
#include "mathlib/vmatrix.h"
#include "tier1/utlsymbol.h"
#include "tier1/utlmap.h"
#include "tier1/utllinkedlist.h"
#include "text/uitextlayoutposix.h"
#include "panorama/iuiengine.h"
#include "panorama/text/iuitextlayout.h"
#include "panorama/data/iimagesource.h"
#include "panorama/panoramatypes.h"
#include "input/mousecursor.h"
#include "uitoplevelwindowoverlay.h"
#include "opengllazyshaders.h"


//#include <tier0/memdbgoff.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#ifdef LINUX
//
// DBUS support
#include "glib-object.h"
#include "gio/gio.h"
#endif
typedef SDL_GLContext GLContext;
#include <tier0/memdbgon.h>

#define FANCYQUAD_MAXSTOPS 16
#define FANCYQUAD_GRADIENT_TEXTURE_SIZE 1024

extern CCommandLineParam g_DumpUsedShaders;

namespace panorama
{

#ifdef LINUX
//
// Convenient wrapper class for handling dbus signals
// 

class CDBusCallback {
public:
	CDBusCallback(const char *interface_name, const char *signal_name, const char *object_path, GDBusSignalCallback pCallback, void *pData)
	{
		//
		// Unfortunately, this file is being built with glib headers newer than the
		// glib we are actually running with in the steam runtime because pango pulls
		// in the headers for its own modified version of glib 
		//  
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
		g_type_init( );		// necessary for dbus callbacks when running panorama outside the Steam process
#pragma GCC diagnostic pop

		m_connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM,NULL,NULL);
		if ( m_connection )
		{
			m_subscription = g_dbus_connection_signal_subscribe(m_connection,
																NULL,
																interface_name,
																signal_name,
																object_path,
																NULL,
																G_DBUS_SIGNAL_FLAGS_NONE,
																pCallback,
																pData,
																NULL);
				
		}
	}
	~CDBusCallback()
	{
		if ( m_connection )
		{
			g_dbus_connection_signal_unsubscribe(m_connection, m_subscription);
			g_object_unref(m_connection);
		}
	}
private:
	GDBusConnection *m_connection;
	guint m_subscription;
};
#endif

enum FancyQuadTextureType_t
{
	FancyQuadTextureType_None,
	FancyQuadTextureType_RGBA,
	FancyQuadTextureType_Premul,
	FancyQuadTextureType_Alpha,
	FancyQuadTextureType_YUV,
	FancyQuadTextureType_Total,
};
enum FancyQuadFlag_t
{
	FancyQuadFlag_RadialGradient = (1<<0),
	FancyQuadFlag_OuterCorner = (1<<1),
	FancyQuadFlag_InnerCorner = (1<<2),
	FancyQuadFlag_Saturation = (1<<3),
	FancyQuadFlag_OpacityMask = (1<<4),
	FancyQuadFlag_GradientTwoStop = (1<<5),
	FancyQuadFlag_GradientComplex = (1<<6),
	FancyQuadFlag_Total = (1<<7),
	FancyQuadFlag_Count = 7,
};

// Vertex struct format
struct VertexTextured_t
{
	float x, y, z, rhw;
	float r, g, b, a;
	float u, v; // texture coordinates
	float masku1, maskv1;
	float masku2, maskv2;
};

struct RectBounds_t
{
	float left;
	float right;
	float top;
	float bottom;
};
	
// Forward declaration
class COpenGLSurface;

// forestw: forward declarations for the FancyQuad code
struct FancyQuadVertex_t;
struct FancyQuadBrush_t;
struct FancyQuadParameters_t;
	
	
//-----------------------------------------------------------------------------
// Purpose: GetProcAddress for GL apis
//-----------------------------------------------------------------------------
template <typename FUNCPTR_TYPE>
class CDynamicFunctionOpenGL
{
public:
	CDynamicFunctionOpenGL( const char *pszName )
	{
		m_pfn = ( FUNCPTR_TYPE )SDL_GL_GetProcAddress( pszName );
	}
	
	operator bool() { return m_pfn != NULL; }
	bool operator !() { return !m_pfn; }
	operator FUNCPTR_TYPE() { return m_pfn; }
	
private:
	FUNCPTR_TYPE m_pfn;
};

namespace SDLOGLSurfaceNameSpace
{
class CCompositionLayer
{
public:

	// Constructor 
	CCompositionLayer( COpenGLSurface *pParentSurface, GLContext hGLContext, float width, float height );
	~CCompositionLayer();

	// Constructor for dummy composition layers used for searching
	CCompositionLayer(float width, float height) : m_flLayerWidth(width), m_flLayerHeight(height), m_hFBO(0), m_hGLTexture(0), m_pVecClipLayers(NULL) {};

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
	void PushCliplayersAndBeginDraw( float flScaleX, float flScaleY, float flTranslateX, float flTranslateY  );

	// Pop clip layers and end drawing for layer
	void PopClipLayersAndFlush();

	// Draw the border for the layer
	void DrawBorder( COpenGLSurface *pBaseSurface );
	
	// clear the contents of the render buffer
	void Clear();

	// Access render quad info
	VertexTextured_t *AccessRenderQuad() { return m_RenderQuad; }

	// Access matrix data
	float *AccessMatrix() { return m_flMatrix; }

	// Set desaturation
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
	void SetOpacityMaskTextureID( uint32 unTextureID, float flOpacityMaskOpacity ) { m_unOpacityMaskTextureID = unTextureID; m_flOpacityMaskOpacity = flOpacityMaskOpacity; }

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

	void SetBoxShadow( bool bInset, bool bFill, float flHorOffset, float flVerOffset, float flBlurRadius, float flSpreadDistance, uint32 rgbaShadowColor, bool bAnimating )
	{
		m_bBoxShadowInset = bInset;
		m_bBoxShadowFill = bFill;
		m_flBoxShadowHorOffset = flHorOffset;
		m_flBoxShadowVerOffset = flVerOffset;
		m_flBoxShadowBlurRadius = flBlurRadius;
		m_flBoxShadowSpreadDistance = flSpreadDistance;
		m_rgbaBoxShadowColor = rgbaShadowColor;
		m_bAnimatingBoxShadow = bAnimating;
	}

	void GetBoxShadow( bool &bInset, bool &bFill, float &flHorOffset, float &flVerOffset, float &flBlurRadius, float &flSpreadDistance, uint32 &rgbaShadowColor, bool &bAnimating )
	{
		bInset = m_bBoxShadowInset;
		bFill = m_bBoxShadowFill;
		flHorOffset = m_flBoxShadowHorOffset;
		flVerOffset = m_flBoxShadowVerOffset;
		flBlurRadius = m_flBoxShadowBlurRadius;
		flSpreadDistance = m_flBoxShadowSpreadDistance;
		rgbaShadowColor = m_rgbaBoxShadowColor;
		bAnimating = m_bAnimatingBoxShadow;
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
	
	void GetCurrentClipRect( RectBounds_t &r );

	// Get current clip layer count for composition layer
	uint32 GetClipLayerCount();

	// Draws an inset shadow (unblurred) into the layer
	void DrawInsetShadowIntoLayer( COpenGLSurface *pBaseSurface, float flPadding, float flWidth, float flHeight, float flHorOffset, float flVerOffset, float flSpreadDistance, uint32 shadowColor, float *pflInnerRadii );

	// Sort on size only
	bool operator <( const CCompositionLayer &l ) const
	{
		if ( m_flLayerWidth < l.m_flLayerWidth )
			return true;
		else if ( m_flLayerWidth > l.m_flLayerWidth )
			return false;

		return m_flLayerHeight < l.m_flLayerHeight;
	}
	
	int GetOGLTextureID() { return m_hGLTexture; }
	int GetOGLFBOHandle() { return m_hFBO; }
	void ActivateRenderTarget(); // use this layer as rendertarget

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName );
#endif

private:

	bool BHasNoRounding( const CRadiusData &msg );

	// Ptr to parent surface
	COpenGLSurface *m_pParentSurface;

	// Have we called BeginDraw() already?
	bool m_bIsDrawing;

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

	// Clip layers for the layer
	CUtlVector<CMsgPushClipLayer> *m_pVecClipLayers;

	// Quad to use when rendering to parent layer
	VertexTextured_t m_RenderQuad[4];
	
	// 2D scaling factors to apply centered around untransformed object
	float m_flScale2D[2];

	// 2d rotation to apply cenetered around untransformed object
	float m_flRotate2D;
	
	uint32 m_unOpacityMaskTextureID;
	float m_flOpacityMaskOpacity;

	// the OGL handle to the FBO used for this layer
	uint32 m_hFBO;
	uint32 m_hGLTexture;
};

} // namespace

class IOGLUITexture
{
public:
	virtual ~IOGLUITexture() {}
	
	// These may or may not do locking, depending on the implementation (they only do for double buffered 
	// textures currently), but they must be implemented to provide safety such that while locked the 
	// caller on the render thread can call the various access functions and not worry about data changing 
	// underneath them due to actions of the owning thread.
	virtual int LockAndGetCurrentTexture( GLint &glTextNum, int nSerial, COpenGLSurface *pSurface ) = 0;
	virtual void Unlock( int iLockHandle ) = 0;
	
#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName ) = 0;
#endif
	
};

	
class COpenGLTexture : public IUITexture, public IOGLUITexture
{
public:
	
	// Constructor
	COpenGLTexture( uint32 unTextureID, void *pubTextureData, uint32 unWidth, uint32 unHeight, uint32 unStride, E2DTextureFormat eFormat, EAlphaChannelType eAlphaChannelType );
	
	// Destructor
	virtual ~COpenGLTexture();
	
	// IUITexture interface
	virtual uint32 GetTextureID() { return m_unTextureID; }
	virtual uint32 GetTextureWidth() { return m_unWidth; }
	virtual uint32 GetTextureHeight() { return m_unHeight; }
	virtual uint32 GetStride() { return m_unStride; }
	virtual E2DTextureFormat GetFormat() { return m_eFormat; }
	virtual EAlphaChannelType GetAlphaChannelType() { return m_eAlphaChannelType; }
	virtual bool BIsReady() { return m_bTextureUploaded; }
	
	virtual uint32 GetOGLTextureID() { return m_nOGLTextureID; }
	
	virtual GLuint GetOGLTextureFormat() { return m_oglTextureFormat; }
	virtual GLuint GetOGLInternalTextureFormat() { return m_oglInternalTextureFormat; }
	void SetTextureUploaded() { m_bTextureUploaded = true; }
	void FreeTextureData();
	
	void CreateOGLTextureIDIfNeeded( COpenGLSurface *pSurface );
	
	GLenum GetOGLTextureType();
	
	virtual int LockAndGetCurrentTexture( GLint &glTextNum, int nSerial, COpenGLSurface *pSurface )
	{
		REFERENCE( nSerial );
		glTextNum = m_nOGLTextureID;
		return -1;
	}
	
	virtual void Unlock( int iLockHandle ) { }

	virtual void *GetTextureData() {return m_pubTextureData;}
	
#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName )
	{
		VALIDATE_SCOPE();	
	}
#endif
	
private:
	bool m_bTextureUploaded;
	uint32 m_unStride;
	uint32 m_unTextureID;
	uint32 m_unWidth;
	uint32 m_unHeight;
	E2DTextureFormat m_eFormat;
	EAlphaChannelType m_eAlphaChannelType;
	
	GLuint m_oglTextureFormat;
	GLuint m_oglInternalTextureFormat;

	uint32 m_nOGLTextureID;
	void *m_pubTextureData;
};


//
// Implementation of a double buffered texture for efficient hardware 
// accelerated handling of frequently updated textures via double buffering.
//
class COpenGLDoubleBufferedTexture : public IUIDoubleBufferedTexture, public IOGLUITexture
{
public:
	// Constructor
	COpenGLDoubleBufferedTexture( uint32 unTextureID, uint32 unTextureWidth, uint32 unTextureHeight, uint32 unStride, E2DTextureFormat eFormat, EAlphaChannelType eAlphaChannelType, bool bSerializedUploads );
	
	// Destructor
	virtual ~COpenGLDoubleBufferedTexture();
	
	// IUITexture interface
	virtual uint32 GetTextureID() { return m_unTextureID; }
	virtual uint32 GetTextureWidth() { return m_unWidth; }
	virtual uint32 GetTextureHeight() { return m_unHeight; }
	virtual uint32 GetStride() { return m_unStride; }
	virtual E2DTextureFormat GetFormat() { return m_eFormat; }
	virtual EAlphaChannelType GetAlphaChannelType() { return m_eAlphaChannelType; }
	virtual bool BIsReady() { return m_bTextureUploaded; }
		
	// IUIDoubleBufferedTexture interface
	virtual int32 UpdateTextureData( void *pBuffer );
	
	virtual void Unlock( int iLockHandle );
	virtual int LockAndGetCurrentTexture( GLint &glTextNum, int nSerial, COpenGLSurface *pSurface );

	void CreateOGLTextureIDIfNeeded( COpenGLSurface *pSurface );
	void UploadOGLTextureIfNeeded( COpenGLSurface *pSurface );

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName )
	{
		VALIDATE_SCOPE();	
	}
#endif

private:
	int32 m_nSerial; // when not using immediate mode the unique value of this uploaded texture, ties back to drawrect calls with a serial
	bool m_bTextureUploaded;
	uint32 m_unStride;
	uint32 m_unTextureID;
	uint32 m_unWidth;
	uint32 m_unHeight;
	E2DTextureFormat m_eFormat;
	EAlphaChannelType m_eAlphaChannelType;
	
	int32 m_nDrawSerial;

	int32 m_iPendingUpload; // the texture index of the next slot to upload into
	CThreadEvent m_DrawEvent; // used to wake the main thread if its blocking for an upload
	bool m_bSerializedUploads;
	int m_iCurRenderTexture;
	
	GLuint m_oglTextureFormat;
	GLuint m_oglInternalTextureFormat;
	
	bool m_bCreatedOGLTextures;
	GLuint m_nOGLTextureID;

	struct Texture_t
	{
		// Not a spin lock, because if the provider thread (movies) renders
		// faster than we render in the render thread it will block on this,
		// which is fine and essentially sleep time, but we shouldn't spin.
		CThreadMutex m_Lock;
		int32 m_nSerial;
		GLuint m_nOGLPBOID;	
		void *m_pTextureData;
		bool m_bTextureUploadPending;

	};
	
	Texture_t m_rgTextures[2];
	
	// Tracking and locking for which texture should currently be used for rendering
	CThreadSpinLock m_IndexLock;
};

	
//
// Implementation of a double buffered texture for efficient hardware 
// accelerated handling of frequently updated textures via double buffering.
//
class COpenGLDoubleBufferedYUV420Texture : public IUIDoubleBufferedYUV420Texture
{
public:
	// Constructor
	COpenGLDoubleBufferedYUV420Texture( COpenGLSurface *pSurface, uint32 unTextureID, uint32 unTextureWidth, uint32 unTextureHeight );
	
	// Destructor
	virtual ~COpenGLDoubleBufferedYUV420Texture();
	
	// IUITexture interface
	virtual uint32 GetTextureID() { return m_unTextureID; }
	virtual uint32 GetTextureWidth() { return m_unWidth; }
	virtual uint32 GetTextureHeight() { return m_unHeight; }
	virtual uint32 GetStride() { return m_unTextureStride; }
	virtual E2DTextureFormat GetFormat() { return k_EFormatYUV420; }
	virtual EAlphaChannelType GetAlphaChannelType() { return k_EAlphaChannelType_None; }
	virtual bool BIsReady() { return m_bTextureUploaded; }
	
	// IUIDoubleBufferedYUV420Texture interface
	virtual bool BUpdateTextureData( void *pYBuffer, void *pUBuffer, void *pVBuffer, uint unStrideY, uint unStrideU, uint unStrideV );

	// IOGLTexture interface
	virtual uint32 GetOGLTextureID() { return m_Textures[0].m_nOGLTextureID; }

	virtual uint32 GetOGLTextureIDU() { return m_Textures[1].m_nOGLTextureID; }
	virtual uint32 GetOGLTextureIDV() { return m_Textures[2].m_nOGLTextureID; }

	void CreateOGLTextureIDIfNeeded( COpenGLSurface *pSurface );
	void UploadOGLTextureIfNeeded( COpenGLSurface *pSurface );
	uint32 GetStrideUV() { return (m_unTextureStride + 1) / 2; }

private:
	int32 m_nSerial; // when not using immediate mode the unique value of this uploaded texture, ties back to drawrect calls with a serial
	bool m_bTextureUploaded;
	uint32 m_unTextureStride;
	uint32 m_unTextureID;
	uint32 m_unWidth;
	uint32 m_unHeight;
	GLuint m_oglTextureFormat;
	GLuint m_oglInternalTextureFormat;

	CThreadMutex m_BufferLock;
	bool m_bTextureUploadPending;
	bool m_bOGLInitialized;
	double m_flLastRenderThreadFrameTimeOnUpdate;
	COpenGLSurface *m_pSurface;
	
	struct TextureData_t
	{
		GLuint m_nOGLTextureID;	
		GLuint m_nOGLPBOID;	
		void *m_pTextureData;
	};
	
	TextureData_t m_Textures[3]; // one each for YUV
};

//
// A simple LRU cache for composition layers
// 
template <typename K, typename L=CDefLess<K>, bool ALLOW_DUPES = false >
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
	void Purge(double flTimestamp)
	{
		while( m_LRU.Count() )
		{
			int iList = m_LRU.Head();
			LRU_t& lru = m_LRU[iList];
			if ( lru.m_flLastUseTime <= flTimestamp )
			{
				Layer_t& data = m_Layers[lru.m_iMap];
				delete data.m_pLayer;
				m_Layers.RemoveAt(lru.m_iMap);
				m_LRU.Remove(iList);
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
	bool Touch(K ulLayerID, float flWidth, float flHeight, double flTimestamp)
	{
		COMPILE_TIME_ASSERT( !ALLOW_DUPES );
		int iMap = m_Layers.Find(ulLayerID);
		if ( iMap != m_Layers.InvalidIndex() )
		{
			Layer_t& layer = m_Layers[iMap];
			if ( layer.m_pLayer->GetWidth() == flWidth && layer.m_pLayer->GetHeight() == flHeight )
			{
				m_LRU.LinkToTail(layer.m_iLRUIndex);
				m_LRU[m_LRU.Tail()].m_flLastUseTime = flTimestamp;
				layer.m_iLRUIndex = m_LRU.Tail();
				return true;
			}
		}
		return false;
	}
	
	void Insert(K ulLayerID, SDLOGLSurfaceNameSpace::CCompositionLayer *pLayer, double flTimestamp )
	{
		int iMap = m_Layers.Find(ulLayerID);
		Assert( ALLOW_DUPES || iMap == m_Layers.InvalidIndex() );

		if ( iMap == m_Layers.InvalidIndex() || ALLOW_DUPES )
		{
			Layer_t layer;
			layer.m_pLayer = pLayer;
			layer.m_iLRUIndex = -1;
			if ( ALLOW_DUPES )
				iMap = m_Layers.InsertWithDupes(ulLayerID, layer);
			else
				iMap = m_Layers.Insert(ulLayerID, layer);
			LRU_t lru;
			lru.m_iMap = iMap;
			lru.m_flLastUseTime = flTimestamp;
			m_Layers[iMap].m_iLRUIndex = m_LRU.AddToTail( lru );
		}
	}

	void Remove(K ulLayerID) // use FindAndRemove() for the dupes case
	{
		COMPILE_TIME_ASSERT( !ALLOW_DUPES );
		int iMap = m_Layers.Find(ulLayerID);
		if ( iMap != m_Layers.InvalidIndex() )
		{
			Layer_t& layer = m_Layers[iMap];
			m_LRU.Remove( layer.m_iLRUIndex );
			m_Layers.RemoveAt( iMap );
		}
	}

	SDLOGLSurfaceNameSpace::CCompositionLayer *Find(K ulLayerID, double flTimestamp)
	{
		COMPILE_TIME_ASSERT( !ALLOW_DUPES );
		int iMap = m_Layers.Find(ulLayerID);
		if ( iMap != m_Layers.InvalidIndex() )
		{
			Layer_t& layer = m_Layers[iMap];
			m_LRU.LinkToTail(layer.m_iLRUIndex);
			m_LRU[m_LRU.Tail()].m_flLastUseTime = flTimestamp;
			layer.m_iLRUIndex = m_LRU.Tail();

			return layer.m_pLayer;
		}
		return NULL;
	}

	SDLOGLSurfaceNameSpace::CCompositionLayer *FindAndRemove(K ulLayerID, float flTimestamp)
	{
		int iMap = m_Layers.FindFirst(ulLayerID);
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


private:
	
	struct Layer_t
	{
		SDLOGLSurfaceNameSpace::CCompositionLayer * m_pLayer;
		int m_iLRUIndex;	// points into LRU list
	};
	struct LRU_t
	{
		double m_flLastUseTime;
		int m_iMap;			// points into m_Layers
	};
	CUtlMap<K,Layer_t,int,L> m_Layers;
	CUtlLinkedList<LRU_t, int> m_LRU;
};

//
// Implementation of IUI3DSurface in OpenGL. 
//
class COpenGLSurface : public IUI3DSurface, public IUITextTextureStorage, public IUITextTextureProvider
{
public:

	COpenGLSurface();
	~COpenGLSurface();

	bool BInitialize( SDL_Window *pSDLWindow, void *hGLContext, int nSurfaceWidth, int nSurfaceHeight, int nWindowWidth, int nWindowHeight, IUIEngine::ERenderTarget eRenderType, bool bEnforceAspectRatio, bool bFixedSurfaceSize, CMouseCursorRender *pCursorRender );

	// IUI3DSurface implementation
	virtual double GetLastFramePaintTime() { return m_flLastPaintFrameTime; }
	virtual void ReloadChangedFile( const char *pchFile );

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
	virtual bool BCreateDoubleBufferedTexture( IUIDoubleBufferedTexture **pDoubleBufferedOutput, uint32 unWidth, uint32 unHeight, uint32 unStride, E2DTextureFormat eFormat, EAlphaChannelType eAlphaChannelType, bool bSerialized );
	virtual bool BCreateDoubleBufferedYUV420Texture( IUIDoubleBufferedYUV420Texture **pDoubleBufferedYUV420Output, uint32 unWidth, uint32 unHeight );
	virtual bool BDeleteTexture( IUITexture *pTexture );
	virtual void DrawTexturedRect( const CRenderMsg<CMsgRenderTexturedRect> &renderCommand );
	virtual void LockTexture( const CRenderMsg<CMsgLockTexture> &renderCommand );
	virtual void FreeCompositingLayer( const CRenderMsg<CMsgFreeCompositingLayer> &renderCommand );

	virtual bool PingCompositingLayer( uint64 ulLayerID, float flWidth, float flHeight );
	// Get the render thread frame time
	volatile double GetCurrentRenderThreadFrameTime() { return m_flCurrentRenderFrameTime; }

// Handle render callback request (basically calls back raw panel code to draw direct to surface, source 2 only noop elsewhere)
	virtual void RequestRenderCallback( const CRenderMsg<CMsgRequestRenderCallback> &renderCommand ) { }
	

	virtual float GetFPSAverage();

	// Get the FPS average since creation
	virtual float GetSessionFPSAverages();
	virtual volatile int GetNumPeriodsBelowMinFPS() { return 0; }
	
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
		if ( !m_pCursorRender )
			return true;
		return m_pCursorRender->BCursorVisible();
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
	virtual bool BSurfaceOccluded()
	{
		return !BHasVTFocus();
	}

	// Get the scaling factor that is being applied to all UI by the framework
	virtual float GetWindowScaleFactor() { return m_flScaleFactor; }

	// Set the scaling factor that is being applied to all UI by the framework
	virtual void SetWindowScaleFactor( float flScaleFactor ) { m_flScaleFactor = flScaleFactor; }
	
	virtual void PushOverlayRenderCmdStream( CSharedMemStream *pRenderStream, DWORD dwPID, float flOpacity, uint32 unGameWidth, uint32 unGameHeight, EOverlayWindowAlignment alignment );
	// Set desired output texture format for overlay surfaces
	virtual void SetOverlayTextureFormat( int32 eTextureFormat ) { m_eOverlayTextureFormat = eTextureFormat; }

	virtual void SetOverlayLetterboxColor( Color c ) { m_cLetterBoxColor = c; }

	bool BHasVTFocus(){ return !m_bVTSwitched; }

	GLuint CreateOpenGLTextureId();
	void SetTexture( GLint unit, GLint textureid );

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName );
#endif

	// IUITextTextureStorage
	virtual uint32 GetMaximumTextureWidth() OVERRIDE { return 1920; }
	virtual uint32 GetMaximumTextureHeight() OVERRIDE { return 1920; }
	virtual UITextTextureRegion_t GetTextureRegion( int32 iWidth, int32 iHeight ) OVERRIDE;
	virtual void StartUpdateFontGlyphTexture( UITextTextureHandle_t hTexture ) OVERRIDE;
	virtual void UpdateFontGlyphTexture( UITextTextureHandle_t hTexture, int xOffset, int yOffset, int width, int height, void *pSourceData ) OVERRIDE;
	virtual void EndUpdateFontGlyphTexture( UITextTextureHandle_t hTexture ) OVERRIDE;

	// IUITextTextureProvider.
	virtual UITextTextureHandle_t AllocAlphaTexture( int32 iWidth, int32 iHeight ) OVERRIDE;
	virtual void FreeTexture( UITextTextureHandle_t hTexture ) OVERRIDE;

	uint32 GetSurfaceWidth() const { return m_unSurfaceWidth; }

private:

	friend class SDLOGLSurfaceNameSpace::CCompositionLayer;

#if defined(LINUX) && defined(_DEBUG) 
	// register OpenGL debug callback
	static void EnableGLDebug();
	static void APIENTRY GL_Debug_Output_Callback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const GLvoid* userParam);
#endif

	// Free a FBO for re-use
	void FreeFBO( uint32 hFBO );
	
	// Get (allocating if needed) a free FBO object
	uint32 CreateFBO(float width, float height, GLuint *pTextureId, bool bRetry = true);
	uint32 GetFreeFBO();
	void DeleteFBOFreeList();
	void DropLayerCaches( bool bOutsideFrameRendering );

	uint32 GetSurfaceHeight() const { return m_unSurfaceHeight; }
	uint32 GetWindowWidth() const { return m_unWindowWidth; }
	uint32 GetWindowHeight() const { return m_unWindowHeight; }
	IUIEngine::ERenderTarget GetRenderTarget() const { return m_eRenderTarget; }
	bool BBackBufferScalingNeeded() const { return m_unWindowHeight != m_unSurfaceHeight || m_unWindowWidth != m_unSurfaceWidth;}
	void PresentBackBuffer();

	void DestroyDeviceResources( bool bShuttingDown );
	bool BCreateOpenGLDeviceResources();
	bool BPreloadShaders();
	bool BRecreateBaseCompositionLayer();
	uint32 GetOGLTextureIDForTexture( IUITexture *pTexture );
	IUITexture *GetOGLTextureForTextureID( uint32 unTextureID );
	bool BUpdateWindowSizeIfNeeded( uint32 nWidth, uint32 nHeight );
	bool BUpdateRenderStateIfNeeded( IUIEngine::ERenderTarget eMsgRenderTarget );

	bool MakeCurrent( SDL_Window *window, GLContext hGLContext );
	
	// handles to our shaders
	CLazyShaderProgram m_shaderprogramBlur;
	CLazyShaderProgram m_shaderprogramParticle;
	// forestw: this shader has permutations
	CLazyShaderProgram m_shaderprogramFancyQuadUber[FancyQuadTextureType_Total][FancyQuadFlag_Total];
	
	CLazyShaderProgram m_shaderprogramVRHUDDistort;
	IUITexture *m_rpDistortionMap[2];
	
	GLint m_hSystemFBO; // forestw: the system may have provided a non-zero fbo handle to us, so keep track of it
	GLint GetSystemFBO() {return m_hSystemFBO;};

	GLint GetLastActiveFBO() { return m_LastFBOActive; }
	void SetLastActiveFBO( GLint fbo ) { m_LastFBOActive = fbo; }
	GLint m_LastFBOActive;

	SDLOGLSurfaceNameSpace::CCompositionLayer *GetCompositionLayer( float width, float height );
	
	void ClearShaderResourceVariables();
	void ClearCompositingLayer( SDLOGLSurfaceNameSpace::CCompositionLayer *pLayer );

	void SetFancyQuadFillBrush( FancyQuadBrush_t &fancybrush, const CMsgFillBrush &brushMsg, float offsetx, float offsety );

	GLuint GetOpacityMaskShaderResourceViewForTexture( uint32 unTextureID );
	
	SDLOGLSurfaceNameSpace::CCompositionLayer *GetOuterShadowLayer( SDLOGLSurfaceNameSpace::CCompositionLayer *pSourceLayer, bool bFill, float flHorOffset, float flVerOffset, float flBlurRadius, float flSpreadDistance, uint32 shadowColor, bool bAnimating );
	void DrawOuterShadowLayer( SDLOGLSurfaceNameSpace::CCompositionLayer *pParentLayer, SDLOGLSurfaceNameSpace::CCompositionLayer *pShadowLayer, SDLOGLSurfaceNameSpace::CCompositionLayer *pSourceLayer, bool bFill, float flHorOffset, float flVerOffset, float flBlurRadius, float flSpreadDistance, uint32 shadowColor );
	SDLOGLSurfaceNameSpace::CCompositionLayer *GetInsetShadowLayer( SDLOGLSurfaceNameSpace::CCompositionLayer *pSourceLayer, float flHorOffset, float flVerOffset, float flBlurRadius, float flSpreadDistance, uint32 shadowColor, bool bAnimating );
	void DrawInsetShadowLayer( SDLOGLSurfaceNameSpace::CCompositionLayer *pParentLayer, SDLOGLSurfaceNameSpace::CCompositionLayer *pShadowLayer, SDLOGLSurfaceNameSpace::CCompositionLayer *pSourceLayer, float flHorOffset, float flVerOffset, float flBlurRadius, float flSpreadDistance, uint32 shadowColor );
	void DrawMouseCursor( uint32 nMouseTextureID, const Vector2D &ptHotspot );
	void DrawSteamPadPointer( SteamPadPointer_t *pPointer, int padX, int padY );

	UITextOpacityMaskData_t *GetCachedTextOpacityMask( const void *pRawText, int cbRawText, int cTextChars, EPanoramaTextEncoding eTextEncoding, float x0, float y0, float x1, float y1, float flLineHeight, ETextAlign align, bool bWrap, bool bEllipsis, const CMsgRenderTextFormat &defaultFormat, const google::protobuf::RepeatedPtrField<CMsgRenderTextRangeFormat > &rangeFormats );
	void DrawTextRegionRange( SDLOGLSurfaceNameSpace::CCompositionLayer *pLayer, float x0, float y0, float x1, float y1, UITextOpacityMaskDataRange_t &maskRange, const CMsgRenderFillBrushCollection &fill_brush_collection );
	void DrawTexturedQuadInternal( const CLazyShaderProgram &program, uint32 nTextureID, VertexTextured_t *points, float flScale2DX, float flScale2DY, float flRotate2D, float flTextureWidth = -1.0, float flTextureHeight = -1.0f );
	void DrawFancyQuad( uint32 nTexture0ID, uint32 nTexture1ID, uint32 nTexture2ID, float flDesaturation, float flHueShift, float flBrightness, float flContrast, float flOpacityMaskOpacity, int nWide, int nTall, const FancyQuadParameters_t &p, const FancyQuadBrush_t &brush, float flScale2DX, float flScale2DY, float flRotate2D, float flTextureWidth, float flTextureHeight, bool texisnotpremul, bool isalphatexture, bool isyuvtexture, bool rawcoords, const float *matrix, bool bClipToLayer = false );

	void ActivateRenderTarget();
	void ComputeBackbufferScaling();
	void PerformPendingTextureUploads();
	
	void UpdateFancyQuadGradientTexture( const FancyQuadBrush_t &brush );
	void ReloadTextures();
	
	bool m_bVsyncEnabled;

	double m_flLastPaintFrameTime;

	IUIEngine::ERenderTarget m_eRenderTarget;


	CUtlVector<CMsgPushClipLayer> m_vecClipLayers;

	enum ERenderState_t
	{
		k_ERenderStateUnset = 0,
		k_ERenderStateDrawTexturedQuad = 1,
	};

	ERenderState_t m_ERenderState;
	
	CUtlVector< GLuint > m_vecAllTextBitmaps;
	double m_LastTextTextureDumpTime;

	IUITextTextureCache *m_pTextTextureCache;
    IUITextLayoutDrawCache *m_pTextLayoutDrawCache;


	// List of free composition layers for re-use
	CCompositionLayerCache<SDLOGLSurfaceNameSpace::CCompositionLayer *,CDefLessPtr<SDLOGLSurfaceNameSpace::CCompositionLayer>, true > m_FreeLayers;

	CCompositionLayerCache<uint64> m_ReservedLayers;
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

	CCompositionLayerCache< ShadowLayerKey_t > m_ShadowLayers;

	// Stack of composition layers currently pushed
	CUtlVector< SDLOGLSurfaceNameSpace::CCompositionLayer * > m_stackCompositionLayers;

	// Vector of free FBO objects for use
	CUtlVector< uint32 > m_vecFreeFBOs;
	uint32 m_unTotalFBOs;

	uint32 m_unSurfaceWidth;
	uint32 m_unSurfaceHeight;
	float m_flScaleFactor;
	
	// Opacity mask texture id for the layer
	COpenGLTexture *m_pUITextureOpaqueMask;

	// FPS average data
	CFastTimer m_FrameTimer;
	int m_nLastFrameMillisecondsIndex;
	float m_rgflMillisecondsFrame[PANORAMA_FRAMES_FOR_FPS_AVERAGES];

	// Current render frame time
	double m_flCurrentRenderFrameTime;

	SDL_Window *m_hSDLWindow;
	GLContext m_hGLContext;
	
	bool m_bEnforceAspectRatio;
	bool m_bFixedSurfaceSize;
	float m_flScaleBackbufferX;
	float m_flTranslateBackbufferX;
	float m_flScaleBackbufferY;
	float m_flTranslateBackbufferY;
	uint32 m_unWindowWidth;
	uint32 m_unWindowHeight;

	// owner of details about the cursor state
	CMouseCursorRender *m_pCursorRender;

	static CInterlockedInt s_unNextTextureID;
	CThreadSpinLock m_lockTextureMap;
	CUtlMap< uint32, IUITexture *, short, CDefLess< uint32 > > m_mapTextures;
	CTSQueue< uint32 > m_tsQueueTextureDeletes;
	CUtlVector<COpenGLDoubleBufferedYUV420Texture *> m_vecPendingYUVCreates;
	CUtlVector<COpenGLDoubleBufferedTexture *> m_vecPendingDoubleBufferCreates;
	
	GLuint m_hTexturePBO[2];
	CThreadMutex m_mutexTexturePBO;
	
	int m_iCurrentWriteTexturePBO;
	uint64 m_cbTextureWriteOffset;
	void *m_pbTextureUploads;
	
	struct PendingTextureUpload_t
	{
		PendingTextureUpload_t( uint64 iOffset, COpenGLTexture *pTex )
		{
			bPBOUpload = true;
			iPBOOffset = iOffset;
			pTexture = pTex;
		}
		
		PendingTextureUpload_t( COpenGLTexture *pTex )
		{
			bPBOUpload = false;
			iPBOOffset = (uint64)-1;
			pTexture = pTex;
		}

		bool bPBOUpload;
		uint64 iPBOOffset;
		COpenGLTexture *pTexture;
	};
	
	CUtlVector<PendingTextureUpload_t> m_vecPendingTextureUploads;

	int32 m_eOverlayTextureFormat;
	CSharedMemStream *m_pBackBufferSharedMemStream;
	IPC::IEvent *m_pBackBufferSharedMemEvent;
	IPC::IEvent *m_pBackBufferSharedMemWriteEvent;
	uint32 m_unBackBufferSharedMenEventFails;
	
	DWORD m_dwTargetOverlayPID;
	bool m_bRenderSharedSurface;
	float m_flTimeLastSharedTexUpdate;

	static CInterlockedInt s_unNextOverlayTextureID;
	int m_nOverlayTextureID;
	Color m_cLetterBoxColor;
	GLuint m_hOverlayPBO[2];
	int m_iCurrentOverlayPBO;
	CUtlBuffer m_bufPBO;
	
	GLuint m_iGradientTextureName;
	unsigned char m_GradientTextureBuffer[FANCYQUAD_GRADIENT_TEXTURE_SIZE][4];

	bool m_bResumed;
	bool m_bVTSwitched;
	CUtlVector<COpenGLTexture *> m_vecPBOTextures;	// stores the list of textures which need to be re-uploaded after suspend/resume

	int m_nFreeGLTextureIndex;
	GLuint m_rgFreeGLTextureIDs[256];
	CThreadMutex m_glTextureMutex;

	GLuint m_LastProgram;
	GLint m_LastTextureID[16];

	SteamPadPointer_t m_leftSteamPadPointer;
	SteamPadPointer_t m_rightSteamPadPointer;

	// Used for tracking when we need to set a new OpenGL row width
	// during font glyph texture update.
	int m_iLastFontGlyphTextureWidth;
    
#ifdef LINUX

	//
	// Support for getting system resume notification so we can
	// re-upload our texture data
	// 

	CDBusCallback ResumeNotify;
	CDBusCallback VTNotify;
	CDBusCallback LogindVTNotify;
	static void ResumingCallback(GDBusConnection *connection,
                      const gchar *sender_name,
                      const gchar *object_path,
                      const gchar *interface_name,
                      const gchar *signal_name,
                      GVariant *parameters,
                      gpointer user_data)
	{
		Msg("COpenGLSurface::ResumingCallback detected system resume\n");
		COpenGLSurface *surface = (COpenGLSurface *)user_data;
		surface->m_bResumed = true;
	}

	static void VTNotifyCallback(GDBusConnection *connection,
								 const gchar *sender_name,
								const gchar *object_path,
								const gchar *interface_name,
								const gchar *signal_name,
								GVariant *parameters,
								gpointer user_data)
	{
		COpenGLSurface *surface = (COpenGLSurface *)user_data;

		char *szActiveSessionName = NULL;

		if ( !parameters )
			return;
		
		g_variant_get( parameters, "(s)", &szActiveSessionName );
		
		if (!szActiveSessionName)
			return;
		
		Msg( "COpenGLSurface::VTNotifyCallback: Active session is now %s\n", szActiveSessionName );
		
		if ( surface->sCurrentSession == szActiveSessionName )
		{
			Msg("COpenGLSurface::VTNotifyCallback: Switched back to Steam\n");
			surface->m_bResumed = true;
			if ( surface->m_bVTSwitched )
				UIEngine()->DispatchEventAsync( 0.0f, AsyncPanoramaSurfaceReturned::MakeEvent( NULL ) );
			surface->m_bVTSwitched = false;
		}
		else
		{
			Msg("COpenGLSurface::VTNotifyCallback: Switched away from Steam\n");
			surface->m_bVTSwitched = true;
			UIEngine()->DispatchEventAsync( 0.0f, AsyncPanoramaSurfaceLost::MakeEvent( NULL ) );
		}


	}

	static void LogindVTNotifyCallback(GDBusConnection *connection,
									   const gchar *sender_name,
									   const gchar *object_path,
									   const gchar *interface_name,
									   const gchar *signal_name,
									   GVariant *parameters,
									   gpointer user_data)
	{
		COpenGLSurface *surface = (COpenGLSurface *)user_data;

		CUtlString strActiveSession;
		bool bNewSession = false;

		if ( !parameters )
			return;

		//
		// Check if it's been invalidated, if so go re-read it
		// 
		GVariantIter *iter;
		g_variant_get_child( parameters, 2, "as", &iter );
		gchar *str;
		while ( g_variant_iter_loop( iter, "s", &str ) )
		{
			if ( strcmp( str, "ActiveSession" ) == 0 )
			{
				GetLogindSession( connection, strActiveSession );
				bNewSession = true;
			}
		}
		g_variant_iter_free( iter );

		if ( !bNewSession )
		{
			//
			// Check if a new value has been assigned
			// 
			GVariant *asv;
			asv = g_variant_get_child_value( parameters, 1 );
			if ( asv )
			{
				gchar *s, *path;
				if ( g_variant_lookup( asv, "ActiveSession", "(so)", &s, &path ) )
				{
					strActiveSession = s;
					bNewSession = true;
				}
				g_variant_unref( asv );
			}

		}

		if ( bNewSession )
		{
			Msg( "COpenGLSurface::LogindVTNotifyCallback: Active session is now %s\n", strActiveSession.String( ) );

			if ( surface->sCurrentSession == strActiveSession )
			{
				Msg("COpenGLSurface::LogindVTNotifyCallback: Switched back to Steam\n");
				surface->m_bResumed = true;
				if ( surface->m_bVTSwitched )
					UIEngine()->DispatchEventAsync( 0.0f, AsyncPanoramaSurfaceReturned::MakeEvent( NULL ) );
				surface->m_bVTSwitched = false;
			}
			else
			{
				Msg("COpenGLSurface::LogindVTNotifyCallback: Switched away from Steam\n");
				surface->m_bVTSwitched = true;
				UIEngine()->DispatchEventAsync( 0.0f, AsyncPanoramaSurfaceLost::MakeEvent( NULL ) );
			}
		}
	}

	static bool GetLogindSession( GDBusConnection *connection, CUtlString& strSession )
	{
		GVariant *reply = g_dbus_connection_call_sync ( connection,
														"org.freedesktop.login1",
														"/org/freedesktop/login1/seat/seat0",
														"org.freedesktop.DBus.Properties",
														"Get",
														g_variant_new( "(ss)", "org.freedesktop.login1.Seat", "ActiveSession" ),
														NULL,
														G_DBUS_CALL_FLAGS_NONE,
														2000,
														NULL,
														NULL );
		if ( reply )
		{
			GVariant *value;
			const char *str = NULL;

			g_variant_get_child( reply, 0, "v", &value );
			g_variant_get_child( value, 0, "s", &str );
			if ( str )
			{
				strSession = str;
			}
			g_variant_unref( value );
			g_variant_unref( reply );
			if ( str && strcmp(str, "") )
				return true;
		}
		return false;
	}

	
	CUtlString sCurrentSession;
	
	Display *m_xDpy;
	Window m_xWindow;
	unsigned int m_iCurrentWMOpactity;
#endif
};

} // namespace panorama

#endif // SDLOPENGLSURFACE_H
