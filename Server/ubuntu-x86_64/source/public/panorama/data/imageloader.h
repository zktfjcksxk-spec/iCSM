//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef IMAGESOURCE_H
#define IMAGESOURCE_H
#pragma once

#include "panorama/data/iimagesource.h"
#include "panorama/controls/panelptr.h"
#include "tier1/utlbuffer.h"
#include "tier1/fileio.h"
#include "tier1/utlmap.h"
#include "refcount.h"
#include "../../panorama/uifileresource.h"
#include "tier1/utldelegate.h"

namespace panorama
{

// Helper to determine byte size of a pixel in a given format
int GetFormatPixelBytes( EImageFormat format );

class CMovie;
class IUIRenderDevice;
class IUITexture;
class CImageData;


// Helper for converting rgba8 to a8 throwing out rgb channels
void ConvertRGBA8ToA8( CUtlBuffer &bufIn, CUtlBuffer &bufOut, uint32 unWide, uint32 unTall );

class CImageProxySource;
class CImageLoaderTask;

#if defined( SOURCE2_PANORAMA )
class CLoadFromVTexTask;
class CLoadFromSvgTask;
#endif

enum ESourceFormats
{
	k_ESourceFormatUnknown,
	k_ESourceFormatTGA,
	k_ESourceFormatPNG,
	k_ESourceFormatJPG,
	k_ESourceFormatRawRGBA,
	k_ESourceFormatGIF,
	k_ESourceFormatSVG,
	k_ESourceFormatEngineRT,
	k_ESourceFormatVTEX
};

class CImageData;
typedef void (ImageDecodeCallback_t)( bool bSuccess, CImageData *pNewImage, CUtlBuffer *pBufDecoded );

class IImageDecodeWorkItem
{
public:

	virtual ~IImageDecodeWorkItem() {}

	virtual void RunWorkItem() = 0;
	virtual void DispatchResult() = 0;
};

class CImageDecodeWorkItem : public IImageDecodeWorkItem
{
public:
	CImageDecodeWorkItem( IUIRenderDevice *pRenderDevice, CUtlBuffer &bufDataInMayModify, const char *pchFilePath, int nWide, int nTall,
		EImageFormat formatOut, const UIImageLoadParams_t &loadParams, CUtlDelegate< ImageDecodeCallback_t > del );
	~CImageDecodeWorkItem();

	void RunWorkItem();
	void DispatchResult();

#if defined( SOURCE2_PANORAMA ) 
	void UseAsyncFilesystemDeallocator() { m_bUseAsyncFilesystemDeallocator = true; }

	bool m_bUseAsyncFilesystemDeallocator;
#endif
	ESourceFormats m_srcFormat;

private:

	bool m_bSuccess;
	IUIRenderDevice *m_pDevice;
	CUtlBuffer *m_pBuffer;
	CUtlString m_strFilePath;
	int m_nWide;
	int m_nTall;
	UIImageLoadParams_t m_loadParams;
	EImageFormat m_eFormat;
	CImageData *m_pNewImage;

	CUtlDelegate< ImageDecodeCallback_t > m_Del;
};


#ifdef PANORAMA_USE_S1WRAPPER

//-----------------------------------------------------------------------------
//
// Value that is guaranteed to be atomic and load/load or store/store
// consistent when read and written by multiple threads.  Prevents
// compiler reordering of loads across loads and stores across stores
// to avoid the compiler moving code around before or after load/store
// usage.
//
// Use this instead of declaring variables volatile if your only
// intent with volatile is to get the non-portable Visual Studio automatic
// barrier behavior.
//
// Only usable with primitive types.
//
//-----------------------------------------------------------------------------

template< typename T >
class CThreadSyncValue
{
public:
	// Allow arbitrary construction of T.
	template <typename... ConstructArgs>
	CThreadSyncValue( ConstructArgs... constructArgs )
		: m_value( constructArgs... ) {}

	T Load() const
	{
		// For this CPU we automatically get load-acquire semantics so
		// we only need to prevent compiler reordering.
		ThreadMemoryBarrier();
		return m_value;
	}

	void Store( T value )
	{
		// For this CPU we automatically get store-release semantics so
		// we only need to prevent compiler reordering.
		ThreadMemoryBarrier();
		m_value = value;
	}

protected:
	// No need for volatile or other special qualifiers here as
	// we use explicit barriers in Load/Store.
	T m_value;
};

#endif

class CImageDecodeWorkThreadPool;
class CImageDecodeThread : public CThread
{
public:
	CImageDecodeThread( CImageDecodeWorkThreadPool *pParent )
	{
		m_bExit = false;
		m_pParent = pParent;
	}

	~CImageDecodeThread()
	{

	}

	void Stop() { m_bExit = true; }

	virtual int Run() OVERRIDE;

	bool HasFinishedRunning() { return m_bhasFinishedRunning.Load(); }

private:
	CThreadSyncValue<bool> m_bExit;
	CImageDecodeWorkThreadPool *m_pParent;
	CThreadSyncValue<bool> m_bhasFinishedRunning;
};

class CImageDecodeWorkThreadPool
{
public:
	CImageDecodeWorkThreadPool();
	~CImageDecodeWorkThreadPool();

	// Run frame on main thread
	void RunFrame();

#if defined( SOURCE2_PANORAMA ) 
	void WaitForAllJobs();
#endif


	void AddWorkItem( IImageDecodeWorkItem  *pWorkItem );
	
#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName )
	{
		VALIDATE_SCOPE();
		AUTO_LOCK( m_AsyncIoLock );
		ValidateObj( m_llAsyncIORequests );
		ValidateObj( m_llAsyncIOResults );
		for ( int i = 0; i < V_ARRAYSIZE( m_pWorkThreads ); ++i )
		{
			validator.ClaimMemory( m_pWorkThreads[i] );
		}
	}
#endif

private:

	friend class CImageDecodeThread;
#if defined( SOURCE2_PANORAMA ) 
	CImageDecodeThread * m_pWorkThreads[1];
#else
	CImageDecodeThread * m_pWorkThreads[4];
#endif

	CThreadMutex m_AsyncIoLock;
	CThreadEvent m_ThreadEvent;
	CUtlLinkedList< IImageDecodeWorkItem *, int > m_llAsyncIORequests;
	CUtlLinkedList< IImageDecodeWorkItem *, int > m_llAsyncIOResults;
};

//
// A container of images we have loaded
//
class CImageResourceManager : public IUIImageManager
{
public:
	CImageResourceManager( IUIRenderDevice *pDevice );
	~CImageResourceManager();
	virtual void Shutdown();
	void ShutdownForEngine();

	virtual IImageSource *LoadImageFromURL( const IUIPanel *pPanel, const char *pchResourceURLDefault, const char *pchResourceURL, bool bPrioritizeLoad, EImageFormat imgFormatOut, const UIImageLoadParams_t &loadParams ) OVERRIDE;
	virtual IImageSource *LoadImageFileFromMemory( const IUIPanel *pPanel, const char *pchResourceURLDefault, const CUtlBuffer &bufFile, const UIImageLoadParams_t &loadParams ) OVERRIDE;
	virtual IImageSource *LoadImageFromMemory( const IUIPanel *pPanel, const char *pchResourceURLDefault, const CUtlBuffer &bufRGBA, int nWide, int nTall, EImageFormat imgFormatIn, const UIImageLoadParams_t &loadParams ) OVERRIDE;
	virtual IImageSource *LoadImageFromEngineRT( const IUIPanel *pPanel, const char *pchEngineRTName, const UIImageLoadParams_t &loadParams ) OVERRIDE;
	virtual CUtlString GetPchImageSourcePath( IImageSource *pImageSource ) OVERRIDE;

	virtual void ReloadChangedFile( const char *pchFile ) OVERRIDE;
	virtual void ReloadChangedImage( IImageSource *pImageToReload ) OVERRIDE;

	bool OnImageUnreferenced( CImageProxySource *pImage );
	
	void OnResolutionChange( float fRelativeScalefactor );

	void RunFrame();

	void QueueImageDecodeWorkItem( CImageDecodeWorkItem *pWorkItem ); 

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName );
#endif

private:
	IImageSource *LoadImageInternal( const IUIPanel *pPanel, CFileResource &fileResourceDefault, CFileResource &fileResource, bool bPrioritizeLoad, EImageFormat imgFormatOut, const UIImageLoadParams_t &loadParams );
	CImageProxySource *GetDefaultImage( CFileResource &fileDefault, EImageFormat imgFormatOut, bool bAllowAnimation );

	friend class CLoadFileURLTask;
	friend class CLoadFileLocalTask;
	friend class CImageLoaderTask;
#if defined( SOURCE2_PANORAMA )
	friend class CLoadFromVTexTask;
	friend class CLoadFromSvgTask;
#endif

	// Internally adds image to our tracking maps
	void AddImageToManager( CFileResource &resource, CImageProxySource *pImageProxy, const UIImageLoadParams_t &loadParams );
	bool RemoveImageFromManager( IImageSource *pImage );

	// Loads a resource, returns vector index
	bool OnImageLoaded( CFileResource & resource, CImageData *pImage, const UIImageLoadParams_t &loadParams );
	bool OnFailedImageLoad( CFileResource & resource, const UIImageLoadParams_t &loadParams );
	void AddLoad( CFileResource &resource, EImageFormat eFormat, bool bPrioritizeLoad, const UIImageLoadParams_t &loadParams );

	// Synchronous load only used for initial global default image
	bool LoadLocalFileSynchronous( CFileResource &resource, EImageFormat eFormat, const UIImageLoadParams_t &loadParams );

#if defined( SOURCE2_PANORAMA )
	bool FixupFileResourceToCompiledImage( CFileResource &fileResource );
#endif

	struct UrlImageKey_t
	{
		CFileResource fileResource;
		UIImageLoadParams_t loadParams;

		bool operator <( const UrlImageKey_t &l ) const
		{
			if ( loadParams.m_nResizeWidth < l.loadParams.m_nResizeWidth )
				return true;
			else if ( loadParams.m_nResizeWidth > l.loadParams.m_nResizeWidth )
				return false;

			if ( loadParams.m_nResizeHeight < l.loadParams.m_nResizeHeight )
				return true;
			else if ( loadParams.m_nResizeHeight > l.loadParams.m_nResizeHeight )
				return false;

			if ( loadParams.m_nMaxWidth < l.loadParams.m_nMaxWidth )
				return true;
			else if ( loadParams.m_nMaxWidth > l.loadParams.m_nMaxWidth )
				return false;

			if ( loadParams.m_nMaxHeight < l.loadParams.m_nMaxHeight )
				return true;
			else if ( loadParams.m_nMaxHeight > l.loadParams.m_nMaxHeight )
				return false;

			if( (l.loadParams.m_fScaleFactor - loadParams.m_fScaleFactor) > 0.01f )
				return true;
			else if( (loadParams.m_fScaleFactor - l.loadParams.m_fScaleFactor) > 0.01f )
				return false;

			if ( loadParams.m_bAllowAnimation && !l.loadParams.m_bAllowAnimation )
				return true;
			else if ( !loadParams.m_bAllowAnimation && l.loadParams.m_bAllowAnimation )
				return false;

			if( loadParams.m_svgAttributeOverrides.m_nFlags < l.loadParams.m_svgAttributeOverrides.m_nFlags )
				return true;
			if( loadParams.m_svgAttributeOverrides.m_nFlags > l.loadParams.m_svgAttributeOverrides.m_nFlags )
				return false;

			if( loadParams.m_svgAttributeOverrides.m_nFlags )
			{
				for( int i = 0; i < k_ESvgAttributeMax; ++i )
				{
					if( loadParams.m_svgAttributeOverrides.m_nFlags & (1 << i) )
					{
						// Opacity and length values are compared as floats 
						if( (i == k_ESvgAttributeFill_opacity) || (i == k_ESvgAttributeStroke_opacity) || (i == k_ESvgAttributeStroke_width) ||
							(i == k_ESvgAttributeOpacity) )
						{
							float fVal = *(float*)&loadParams.m_svgAttributeOverrides.m_overrides[i];
							float fLVal = *(float*)&l.loadParams.m_svgAttributeOverrides.m_overrides[i];
							if( (fLVal - fVal) > 0.001f )
								return true;
							if( (fVal - fLVal) > 0.001f )
								return false;
						}
						else
						{
							int nVal = *(int*)&loadParams.m_svgAttributeOverrides.m_overrides[i];
							int nLVal = *(int*)&l.loadParams.m_svgAttributeOverrides.m_overrides[i];
							if( nVal < nLVal )
								return true;
							if( nVal > nLVal )
								return false;
						}
					}
				}
			}

			return fileResource < l.fileResource;
		}
	};
	CUtlMap< UrlImageKey_t, CImageProxySource *, int, CDefLess< UrlImageKey_t > > m_mapImagesByURL;
	CUtlMap< IImageSource *, UrlImageKey_t, int, CDefLess< IImageSource *> > m_mapAllImages;

	CUtlVector< CImageLoaderTask * > m_vecLoaderTasksToStart;
	CUtlRBTree< CImageLoaderTask *, int, CDefLess< CImageLoaderTask * > > m_treeLoadTasks;

	IUIRenderDevice *m_pDevice;

	CImageDecodeWorkThreadPool *m_pImageDecodePool;

	bool m_bInited;
	bool m_bEventsRegistered;
};

} // namespace panorama

#endif // IMAGESOURCE_H
