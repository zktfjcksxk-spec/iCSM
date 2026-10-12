//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx.h"
#include "panorama/data/imageloader.h"
#include "panorama/uijsregistration.h"
#include "jpegloader.h"
#include "tgaloader.h"
#include "pngloader.h"
#include "svg/svgloader.h"
#include "tier1/utlbuffer.h"

#if defined( SOURCE2_PANORAMA )
#include "engine2/igameresourceservice.h"
#include "resourcesystem/resourcemanifesthelpers.h"
#include "resourcesystem/iresourcesystem.h"
#endif

#if !defined( SOURCE2_PANORAMA ) 
#if defined( PANORAMA_PUBLIC_STEAM_SDK )
#define ClientUtils SteamUtils
#define ClientHTTP SteamHTTP
#endif
#endif


// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

namespace panorama
{
	class CWaitForMovieLoaded;
	class IUIRenderDevice;

	bool UIImageLoadParams_t::ValidateMaxSize( int nWidth, int nHeight ) const
	{
		if( !IsWithinMaxSize( nWidth, nHeight ) )
		{
#if defined( SOURCE2_PANORAMA )
			Warning( "WARNING: Image '%s' size too large (%d x %d with limit %d x %d), image load failing\n",
				m_origin.Get(), nWidth, nHeight, m_nMaxWidth, m_nMaxHeight );
			ExecuteOnce( Development_AssertMsg( false, "Image '%s' too large, %d x %d", m_origin.Get(), nWidth, nHeight ) );
#else
			Warning( "WARNING: Image size too large (%d x %d with limit %d x %d), image load failing\n",
				nWidth, nHeight, m_nMaxWidth, m_nMaxHeight );
#endif
			return false;
		}

		return true;
	}

	//
	// Data source for image data used to render an asset
	//
	class CImageData : public CRefCount
	{
	public:
		CImageData( IUIRenderDevice *pDevice, EImageFormat eDesiredOutForamt, bool bAllowAnimation );

		bool SetImageDataR8G8B8A8( const byte *pchData, int cbData, const char *pchFilePath, int nWide, int nTall, const UIImageLoadParams_t &loadParams, ESourceFormats srcFormat = k_ESourceFormatUnknown );
		bool SetImageDataB8G8R8A8( CUtlBuffer &buf, int nWide, int nTall, const UIImageLoadParams_t &loadParams, bool bPreMultiplied );

		bool SetImageDataFromResourceFile( const char *pResourceFile, const UIImageLoadParams_t &loadParams );
		bool SetImageDataFromEngineRT( const char *pchEngineRTName );

		int GetWidth();
		int GetHeight();
		bool BIsValid() { return m_nWide && m_nTall; }
		IUITexture *GetTexture();
		EImageFormat ImageFormat() { return m_eFormat; }
		bool BIsAnimating();

		void AsyncCompleteLoadOnMainThread( const byte *pubData, int cubData, CUtlDelegate< void( CImageData *, bool ) > callback );
		void OnMovieInitialized( CWaitForMovieLoaded *pLoader, CUtlDelegate< void( CImageData *, bool ) > callback, bool bSuccessfulInitialize );

#ifdef DBGFLAG_VALIDATE
		virtual void Validate( CValidator &validator, const tchar *pchName )
		{
			VALIDATE_SCOPE();
			ValidatePtrIfNeeded( m_pVideoPlayer.Get() );
		}
#endif

		bool BIsGif() { return m_eSourceFormat == k_ESourceFormatGIF; }

	private:
		virtual ~CImageData();

		ESourceFormats m_eSourceFormat;
		IUIRenderDevice *m_pDevice;
		EImageFormat m_eFormat;
		bool m_bAllowAnimation;
		int m_nWide;
		int m_nTall;
		CRefPtr< IUITexture > m_pUITexture;
		CVideoPlayerPtr m_pVideoPlayer;

		CImageData(const CImageData &src) {} // don't allow copy construction, use CImageData if you want to move around references
	};


	//
	// A proxy object between a loaded image and the consumer of images, lets us flop 
	// in a new source underneath as it loads in async but still have a stable pointer externally
	//
	class CImageProxySource : public IImageSource
	{
	public:
		CImageProxySource( CImageData *pSource, EImageFormat format, const IUIPanel *pPanel );
		virtual int GetWidth();
		virtual int GetHeight();
		virtual bool BIsValid() { return m_pImageData != NULL; }
		virtual bool BIsLoaded() { return m_bLoaded; }		// NOTE: Does not mean you successfully loaded, it means you've finished trying to load.
		virtual  bool BFailedToLoad() { return m_bFailedLoad; }
		virtual IUITexture *GetTexture() { return m_pImageData ? m_pImageData->GetTexture() : nullptr; }
		virtual EImageFormat ImageFormat() { return m_pImageData ? m_pImageData->ImageFormat() : m_eFormat; }
		virtual bool BIsAnimating() { return m_pImageData ? m_pImageData->BIsAnimating() : false; }

		void AddPanelForOnLoadEvent( const IUIPanel * panel ) { m_vecPanelsForOnLoadEvents.AddToTail( panel ); }
		void SetImageSource( CImageData *pLoader );
		CImageData *GetImageSource() { return m_pImageData; }

		virtual int GetRefCount() OVERRIDE { return m_cRef; }
		virtual int AddRef() OVERRIDE;
		virtual int Release() OVERRIDE;

		void OnImageLoaded();
		void OnFailedImageLoad(); 

		void SetupJSObjectTemplate();

#ifdef DBGFLAG_VALIDATE
		virtual void Validate( CValidator &validator, const tchar *pchName )
		{
			VALIDATE_SCOPE();
			ValidatePtrIfNeeded( m_pImageData ); // could be pointing to the default image, don't double claim that
			ValidateObj( m_vecPanelsForOnLoadEvents );
		}
#endif
	protected:
		virtual void DestroyThis() { delete this; }
		virtual ~CImageProxySource();

	private:
		volatile int32 m_cRef;
		bool m_bUnreferencedEvent;

		friend class CImageResourceManager;

		bool m_bLoaded;
		bool m_bFailedLoad;
		CImageData *m_pImageData;
		EImageFormat m_eFormat;
		CUtlVector< CPanelPtr< const IUIPanel > > m_vecPanelsForOnLoadEvents;
	};

	//
	// Helper class for Jobs we create to let us cancel them
	//
	class CImageLoaderTask 
	{
	public:
		CImageLoaderTask( CImageResourceManager *pManager ) 
		{ 
			m_bStarted = false;
			m_pManager = pManager; 
			m_pManager->m_vecLoaderTasksToStart.AddToTail( this );
			m_pManager->m_treeLoadTasks.Insert( this );
		}

		virtual ~CImageLoaderTask() 
		{ 
			m_pManager->m_treeLoadTasks.Remove( this );
		}

		// Must be implemented and do the real work in all loader tasks
		void StartLoad() 
		{
			m_bStarted = true;

			OnStartLoad();
		}

		// Must be implemented and do the real work in all loader tasks
		virtual void OnStartLoad() { }


#ifdef DBGFLAG_VALIDATE
		virtual void Validate( CValidator &validator, const char *pchName )
		{
			
		}
#endif

	private:
		bool m_bStarted;
		CImageResourceManager * m_pManager;
	};

	
}

using namespace panorama;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CImageDecodeWorkItem::CImageDecodeWorkItem( IUIRenderDevice *pRenderDevice, CUtlBuffer &bufDataInMayModify, const char *pchFilePath, int nWide, int nTall,
	EImageFormat formatOut, const UIImageLoadParams_t &loadParams, CUtlDelegate< ImageDecodeCallback_t > del )
{
	m_bSuccess = false;

	m_pNewImage = NULL;
	m_pBuffer = new CUtlBuffer();
	m_pBuffer->Swap( bufDataInMayModify );
	m_strFilePath = pchFilePath;
	m_nWide = nWide;
	m_nTall = nTall;
	m_loadParams = loadParams;
	m_eFormat = formatOut;
	m_pDevice = pRenderDevice;
	m_srcFormat = k_ESourceFormatUnknown;

	m_Del = del;

#if defined( SOURCE2_PANORAMA ) 
	m_bUseAsyncFilesystemDeallocator = false;
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CImageDecodeWorkItem::~CImageDecodeWorkItem()
{
#if defined( SOURCE2_PANORAMA ) 
	if ( m_bUseAsyncFilesystemDeallocator && m_pBuffer )
	{
		g_pAsyncFileSystem->ReleaseBuffer( m_pBuffer->DetachMemory() );
	}
#endif
	SAFE_DELETE( m_pBuffer );
	SAFE_RELEASE( m_pNewImage );
}


//-----------------------------------------------------------------------------
// Purpose: Process work item (threaded work)
//-----------------------------------------------------------------------------
void CImageDecodeWorkItem::RunWorkItem()
{
	VPROF_BUDGET( "CImageDecodeWorkItem::RunWorkItem", VPROF_BUDGETGROUP_TENFOOT );

	CImageData *pNewImage = new CImageData( m_pDevice, m_eFormat, m_loadParams.m_bAllowAnimation );
	m_bSuccess = pNewImage->SetImageDataR8G8B8A8( (byte*)m_pBuffer->Base(), m_pBuffer->TellPut(), m_strFilePath.String(), m_nWide, m_nTall, m_loadParams, m_srcFormat );
	if ( !m_bSuccess )
	{
		SAFE_RELEASE( pNewImage );
	}
	else
	{
		m_pNewImage = pNewImage;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Dispatch result for work item
//-----------------------------------------------------------------------------
void CImageDecodeWorkItem::DispatchResult()
{
	VPROF_BUDGET( "CImageDecodeWorkItem::DispatchResult", VPROF_BUDGETGROUP_TENFOOT );

	m_Del( m_bSuccess, m_pNewImage, m_pBuffer );

	// By dispatching we just gave away our pointer, the handler will free if needed
	m_pNewImage = NULL;
}

class CWaitForAllJobsWorkItem : public IImageDecodeWorkItem
{
public:

	CWaitForAllJobsWorkItem() {}

	void RunWorkItem()
	{
		m_WaitEvent.Set();
	}

	void DispatchResult() {}

	CThreadEvent m_WaitEvent;
};


//-----------------------------------------------------------------------------
// Purpose: constructor
//-----------------------------------------------------------------------------
CImageDecodeWorkThreadPool::CImageDecodeWorkThreadPool()
{
	int nWorkThreads = V_ARRAYSIZE( m_pWorkThreads );
#if defined( SOURCE2_PANORAMA )
	DevAssertMsg( nWorkThreads == 1, "Expeciting one worker, relied upon for svg reload or resolution changes." );
#endif
	
	for ( int i = 0; i < nWorkThreads; ++i )
	{
		m_pWorkThreads[i] = new CImageDecodeThread( this );
		m_pWorkThreads[i]->SetName( "Panorama Image Decode" );
		m_pWorkThreads[i]->Start( 64 * 1024 );
#ifdef PANORAMA_USE_S1WRAPPER
		ThreadSetDebugName( m_pWorkThreads[i]->GetThreadHandle(), "Panorama Image Decode" );
#endif
	}
}


//-----------------------------------------------------------------------------
// Purpose: destructor
//-----------------------------------------------------------------------------
CImageDecodeWorkThreadPool::~CImageDecodeWorkThreadPool()
{
	// Stop all the threads
	for ( int i = 0; i < V_ARRAYSIZE( m_pWorkThreads ); ++i )
	{
		m_pWorkThreads[ i ]->Stop();
	}

	// Wake all the threads up so they exit

	for (;;)
	{	
		bool bAllThreadsDone = true;

		for ( int i = 0; i < V_ARRAYSIZE( m_pWorkThreads ); ++i )
		{
			if ( !m_pWorkThreads[i]->HasFinishedRunning() )
			{
				m_ThreadEvent.Set();
				bAllThreadsDone = false;
				break;
			}
		}

		if ( bAllThreadsDone )
		{
			break;
		}
	}
	
	for ( int i = 0; i < V_ARRAYSIZE( m_pWorkThreads ); ++i )
	{
		m_pWorkThreads[ i ]->Join();
		SAFE_DELETE( m_pWorkThreads[ i ] );
	}
}


//-----------------------------------------------------------------------------
// Purpose: dispatch completed work items on main thread
//-----------------------------------------------------------------------------
void CImageDecodeWorkThreadPool::RunFrame()
{
	VPROF_BUDGET( "CImageDecodeWorkThreadPool::RunFrame", VPROF_BUDGETGROUP_TENFOOT );
	m_AsyncIoLock.Lock();
	while ( m_llAsyncIOResults.Count() )
	{
		int iHead = m_llAsyncIOResults.Head();
		IImageDecodeWorkItem *pResult = m_llAsyncIOResults.Element( iHead );
		m_llAsyncIOResults.Remove( iHead );

		m_AsyncIoLock.Unlock();

		pResult->DispatchResult();
		delete pResult;

		m_AsyncIoLock.Lock();
	}
	m_AsyncIoLock.Unlock();
}


#if defined( SOURCE2_PANORAMA ) 
//-----------------------------------------------------------------------------
// Purpose: dispatch completed work items on main thread
//-----------------------------------------------------------------------------
void CImageDecodeWorkThreadPool::WaitForAllJobs()
{
	Assert( ThreadInMainThread() );

	CWaitForAllJobsWorkItem *pWaitJob = new CWaitForAllJobsWorkItem();
	AddWorkItem( pWaitJob );
	pWaitJob->m_WaitEvent.Wait();
	RunFrame();		// responsible for freeing pWaitJob
}
#endif


//-----------------------------------------------------------------------------
// Purpose: queue work item
//-----------------------------------------------------------------------------
void CImageDecodeWorkThreadPool::AddWorkItem( IImageDecodeWorkItem  *pWorkItem )
{
	{
		AUTO_LOCK( m_AsyncIoLock );
		m_llAsyncIORequests.AddToTail( pWorkItem );
	}
	m_ThreadEvent.Set();
}


//-----------------------------------------------------------------------------
// Purpose: Run loop for image decode threads
//-----------------------------------------------------------------------------
int CImageDecodeThread::Run()
{
#if defined( VPROF_ENABLED ) && !defined( SOURCE2_PANORAMA )
	CVProfile *pProfile = GetVProfProfileForCurrentThread();
#endif

	m_bhasFinishedRunning = false;

	bool bIsTextMode = CommandLine()->FindParm( "-textmode" );

	while ( !m_bExit.Load() )
	{
#if defined( VPROF_ENABLED ) && !defined( SOURCE2_PANORAMA )
		if ( pProfile )
			pProfile->MarkFrame( GetName() );
#endif
		VPROF_BUDGET( "CImageDecodeThread::Run", VPROF_BUDGETGROUP_TENFOOT );

		IImageDecodeWorkItem *pWorkItem = NULL;
		m_pParent->m_AsyncIoLock.Lock();
		if ( m_pParent->m_llAsyncIORequests.Count() )
		{
			VPROF_BUDGET( "CImageDecodeThread::Run - run item", VPROF_BUDGETGROUP_TENFOOT );
			int iHead = m_pParent->m_llAsyncIORequests.Head();
			pWorkItem = m_pParent->m_llAsyncIORequests[iHead];
			m_pParent->m_llAsyncIORequests.Remove( iHead );
			m_pParent->m_AsyncIoLock.Unlock();

			pWorkItem->RunWorkItem();

			AUTO_LOCK( m_pParent->m_AsyncIoLock );
			m_pParent->m_llAsyncIOResults.AddToTail( pWorkItem );
		}
		else
		{
			VPROF_BUDGET( "CImageDecodeThread::Run - sleep", VPROF_BUDGETGROUP_TENFOOT );
			m_pParent->m_AsyncIoLock.Unlock();
			m_pParent->m_ThreadEvent.Wait();
		}
	}

	m_bhasFinishedRunning = true;

	return 0;
}


DECLARE_PANORAMA_EVENT1( ImageUnreferenced, CImageProxySource * )
DEFINE_PANORAMA_EVENT( ImageUnreferenced )

//-----------------------------------------------------------------------------
// Purpose: Ref count add for IImageSource
//-----------------------------------------------------------------------------
int CImageProxySource::AddRef()
{ 
	return ThreadInterlockedIncrement( &m_cRef ); 
}


//-----------------------------------------------------------------------------
// Purpose: Ref count release for IImageSource
//-----------------------------------------------------------------------------
int CImageProxySource::Release()
{ 
	Assert( m_cRef > 0 );
	int cRef = ThreadInterlockedDecrement( &m_cRef );

	if ( 0 == cRef )
		DestroyThis();
	else if ( 1 == cRef && !m_bUnreferencedEvent )
	{
		m_bUnreferencedEvent = true;
		UIEngine()->DispatchEventAsync( 1.0, ImageUnreferenced::MakeEvent( NULL, this ) );
	}

	return cRef;
}


//-----------------------------------------------------------------------------
// Purpose: Helper to determine byte size of a pixel in a given format
//-----------------------------------------------------------------------------
int panorama::GetFormatPixelBytes( EImageFormat format )
{
	switch( format )
	{
	case k_EImageFormatR8G8B8A8:
	case k_EImageFormatB8G8R8A8_PreMultiplied:
	case k_EImageFormatB8G8R8A8:
		return 4;
	case k_EImageFormatA8:
		return 1;
	default:
		AssertMsg( false, "Invalid format to GetFormatPixelBytes" );
		return 0;
	}
}



//-----------------------------------------------------------------------------
// Purpose: constructor
//-----------------------------------------------------------------------------
CImageData::CImageData( IUIRenderDevice *pDevice, EImageFormat eDesiredOutForamt, bool bAllowAnimation )
{
	m_nTall = m_nWide = 0;
	m_eFormat = eDesiredOutForamt;
	m_pDevice = pDevice;
	m_pUITexture = nullptr;
	m_eSourceFormat = k_ESourceFormatUnknown;
	m_bAllowAnimation = bAllowAnimation;
}


//-----------------------------------------------------------------------------
// Purpose: destructor
//-----------------------------------------------------------------------------
CImageData::~CImageData()
{
}


//-----------------------------------------------------------------------------
// Purpose: Get texture
//-----------------------------------------------------------------------------
IUITexture *CImageData::GetTexture() 
{ 
	if ( m_pUITexture )
		return m_pUITexture;

	if ( m_pVideoPlayer.IsValid() )
		return m_pVideoPlayer->GetTexture();

	return 0;
}


//-----------------------------------------------------------------------------
// Purpose: convert rgba8 data to a8 data throwing out rgb channels
//-----------------------------------------------------------------------------
void panorama::ConvertRGBA8ToA8( CUtlBuffer &bufIn, CUtlBuffer &bufOut, uint32 unWide, uint32 unTall )
{
	bufOut.EnsureCapacity( unTall * unWide );
	bufOut.SeekPut( CUtlBuffer::SEEK_HEAD, unTall * unWide );

	byte *pNew = (byte*)bufOut.Base();
	byte *pOld = (byte*)bufIn.Base();
	for( uint32 i=0; i<unTall*unWide; ++i )
	{
		pOld +=3;
		*pNew++ = *pOld++;
	}
}


//-----------------------------------------------------------------------------
// Purpose: load memory based BGRA data
//-----------------------------------------------------------------------------
bool CImageData::SetImageDataB8G8R8A8( CUtlBuffer &buf, int nWide, int nTall, const UIImageLoadParams_t &loadParams, bool bPreMultiplied )
{
	Assert( nWide && nTall );

	if ( !loadParams.ValidateMaxSize( nWide, nTall ) )
	{
		return false;
	}
	
	m_nWide = nWide; 
	m_nTall = nTall;

	if ( loadParams.HasResize() )
	{
		int nResizeWidth = loadParams.m_nResizeWidth;
		int nResizeHeight = loadParams.m_nResizeHeight;
		BResizeImageRGBA( buf, m_nWide, m_nTall, nResizeWidth, nResizeHeight, bPreMultiplied );
		m_nWide = nResizeWidth;
		m_nTall = nResizeHeight;
	}

	return m_pDevice->BCreateTexture( &m_pUITexture, (void *)buf.Base(), m_nWide, m_nTall, m_nWide, k_EFormatBGRA8, bPreMultiplied ? k_EAlphaChannelType_PreMultiplied : k_EAlphaChannelType_Normal );
}	


//-----------------------------------------------------------------------------
// Purpose: trigger loading this resource
//-----------------------------------------------------------------------------
bool CImageData::SetImageDataR8G8B8A8( const byte *pchData, int cbData, const char *pchFilePath, int nWide, int nTall, const UIImageLoadParams_t &loadParams, ESourceFormats srcFormat /*= k_ESourceFormatUnknown*/ )
{
	VPROF_BUDGET( "CImageData::SetImageDataR8G8B8A8", VPROF_BUDGETGROUP_TENFOOT );
	CFastTimer timer;
	timer.Start();

	if ( !loadParams.ValidateMaxSize( nWide, nTall ) )
		return false;
	
	if ( loadParams.m_nResizeWidth == 0 || loadParams.m_nResizeHeight == 0 )
		return false;

	CUtlBuffer bufTemp;
	bufTemp.EnsureCapacity( nTall*nWide*4 );

	CUtlVector<ESourceFormats> vecFormatsToTry;

	if( srcFormat != k_ESourceFormatUnknown )
	{
		vecFormatsToTry.AddToTail( srcFormat );
	}
	else if ( pchFilePath == NULL || pchFilePath[0] == 0 )
	{
		if( nWide && nTall )
		{
			vecFormatsToTry.AddToTail( k_ESourceFormatRawRGBA );
		}
		else
		{
			if ( cbData > 4 && pchData[0] == 0x89 && pchData[1] == 0x50 && pchData[2] == 0x4E && pchData[3] == 0x47 )
				vecFormatsToTry.AddToTail( k_ESourceFormatPNG );
			else if ( cbData > 2 && pchData[0] == 0xFF && pchData[1] == 0xD8 )
				vecFormatsToTry.AddToTail( k_ESourceFormatJPG );
			else if ( cbData > 5 && pchData[0] == 0x47 && pchData[1] == 0x49 && pchData[2] == 0x46 && pchData[3] == 0x38 && pchData[4] == 0x39 )
				vecFormatsToTry.AddToTail( k_ESourceFormatGIF );
			else
			{
				vecFormatsToTry.AddToTail( k_ESourceFormatPNG );
				vecFormatsToTry.AddToTail( k_ESourceFormatJPG );
				vecFormatsToTry.AddToTail( k_ESourceFormatTGA );
			}
		}
	}
	else if ( V_stristr( pchFilePath, ".tga" ) )
	{
		vecFormatsToTry.AddToTail( k_ESourceFormatTGA );
		vecFormatsToTry.AddToTail( k_ESourceFormatPNG );
		vecFormatsToTry.AddToTail( k_ESourceFormatJPG );
	}
	else if ( V_stristr( pchFilePath, ".jpg" ) || V_stristr( pchFilePath, ".jpeg" ) )
	{
		vecFormatsToTry.AddToTail( k_ESourceFormatJPG );
		vecFormatsToTry.AddToTail( k_ESourceFormatPNG );
		vecFormatsToTry.AddToTail( k_ESourceFormatTGA );
	}
	else if ( V_stristr( pchFilePath, ".png" ) )
	{
		vecFormatsToTry.AddToTail( k_ESourceFormatPNG );
		vecFormatsToTry.AddToTail( k_ESourceFormatJPG );
		vecFormatsToTry.AddToTail( k_ESourceFormatTGA );
	}
	else if ( V_stristr( pchFilePath, ".gif" ) )
	{
		vecFormatsToTry.AddToTail( k_ESourceFormatGIF );		
	}
	else if( V_stristr( pchFilePath, ".svg" ) )
	{
		vecFormatsToTry.AddToTail( k_ESourceFormatSVG );
	}
	else
	{
		//AssertMsg1( false,  "Image extension for %s not explicitly known, may fail to load", pchFilePath );
		if ( cbData > 4 && pchData[0] == 0x89 && pchData[1] == 0x50 && pchData[2] == 0x4E && pchData[3] == 0x47 )
			vecFormatsToTry.AddToTail( k_ESourceFormatPNG );
		else if ( cbData > 2 && pchData[0] == 0xFF && pchData[1] == 0xD8)
			vecFormatsToTry.AddToTail( k_ESourceFormatJPG );
		else if ( cbData > 5 && pchData[0] == 0x47 && pchData[1] == 0x49 && pchData[2] == 0x46 && pchData[3] == 0x38 && pchData[4] == 0x39 )
			vecFormatsToTry.AddToTail( k_ESourceFormatGIF );
		else 
		{
			vecFormatsToTry.AddToTail( k_ESourceFormatPNG );
			vecFormatsToTry.AddToTail( k_ESourceFormatJPG );
			vecFormatsToTry.AddToTail( k_ESourceFormatTGA );
		}
	}

	FOR_EACH_VEC( vecFormatsToTry, i )
	{
		if ( vecFormatsToTry[i] == k_ESourceFormatRawRGBA )
		{
			m_eSourceFormat = k_ESourceFormatRawRGBA;
			bufTemp.Put( pchData, cbData );
			m_nWide = nWide; 
			m_nTall = nTall;
			break;
		}
		else if ( vecFormatsToTry[i] == k_ESourceFormatTGA )
		{
			VPROF_BUDGET( "CImageData::SetImageDataR8G8B8A8 - LoadTGA", VPROF_BUDGETGROUP_TENFOOT );
			char *pchImageBytes = NULL;
			int nBytes;
			if ( LoadTGA(cbData, (char *)pchData, (byte **)&pchImageBytes, &nBytes, &m_nWide, &m_nTall ) )
			{
				m_eSourceFormat = k_ESourceFormatTGA;
				bufTemp.Put( pchImageBytes, nBytes );
				delete [] pchImageBytes;
				break;
			}
		}
		else if ( vecFormatsToTry[i] == k_ESourceFormatJPG )
		{
			VPROF_BUDGET( "CImageData::SetImageDataR8G8B8A8 - Jpeg", VPROF_BUDGETGROUP_TENFOOT );
			if ( ConvertJpegToRGBA( (const byte *)pchData, cbData, bufTemp, m_nWide, m_nTall ) )
			{
				m_eSourceFormat = k_ESourceFormatJPG;
				break;
			}
		}
		else if ( vecFormatsToTry[i] == k_ESourceFormatPNG )
		{
			VPROF_BUDGET( "CImageData::SetImageDataR8G8B8A8 - Png", VPROF_BUDGETGROUP_TENFOOT );

			if ( ConvertPNGToRGBA( (const byte *)pchData, cbData, bufTemp, m_nWide, m_nTall ) )
			{
				m_eSourceFormat = k_ESourceFormatPNG;
				break;
			}
		}
		else if( vecFormatsToTry[i] == k_ESourceFormatSVG )
		{
			VPROF_BUDGET( "CImageData::SetImageDataR8G8B8A8 - SVG", VPROF_BUDGETGROUP_TENFOOT );

			if( loadParams.m_nResizeWidth != k_ResizeNone )
			{
				m_nWide = loadParams.m_nResizeWidth;
			}

			if( loadParams.m_nResizeHeight != k_ResizeNone )
			{
				m_nTall = loadParams.m_nResizeHeight;
			}

			if( ConvertSVGToRGBA( (const byte *)pchData, cbData, bufTemp, m_nWide, m_nTall, loadParams.m_fScaleFactor, &loadParams.m_svgAttributeOverrides ) )
			{
				m_eSourceFormat = k_ESourceFormatSVG;
				break;
			}
		}
	}

	bool bLoadedImage = ( m_nWide != 0 && m_nTall != 0 );

	// PANORAMA_USE_S1WRAPPER - Dropping gif support (currently unused, except from an incorrect rss image)
	// If we need to re-add gif support, make sure to revisit ImageLoader / UIEngine shutdown process
	// (as the CImageData class will create an instance of the video player, and destroying the video
	// player will try to unregister panorama events after shutting down UIEngine in this case, cf JITA CSGO-1639)
#ifndef PANORAMA_USE_S1WRAPPER
	// gifs are special, need to return true and let caller handle on main thread
	if ( !bLoadedImage && vecFormatsToTry.HasElement( k_ESourceFormatGIF ) )
	{
		m_eSourceFormat = k_ESourceFormatGIF;
		return true;
	}
#endif

	if ( !bLoadedImage )
	{
		Msg( "Failed to load image data from %s\n", pchFilePath );
		return false;
	}

	if ( !loadParams.ValidateMaxSize( m_nWide, m_nTall ) )
	{
		return false;
	}
	
	if ( loadParams.HasResize() && (m_eSourceFormat != k_ESourceFormatSVG) )
	{
		VPROF_BUDGET( "CImageData::SetImageDataR8G8B8A8 - resize", VPROF_BUDGETGROUP_TENFOOT );

		int nResizeWidth = loadParams.m_nResizeWidth;
		int nResizeHeight = loadParams.m_nResizeHeight;
		BResizeImageRGBA( bufTemp, m_nWide, m_nTall, nResizeWidth, nResizeHeight );
		m_nWide = nResizeWidth;
		m_nTall = nResizeHeight;
	}

	if ( m_eFormat == k_EImageFormatA8 )
	{
		VPROF_BUDGET( "CImageData::SetImageDataR8G8B8A8 - A8 conversion", VPROF_BUDGETGROUP_TENFOOT );

		CUtlBuffer bufOut;
		ConvertRGBA8ToA8( bufTemp, bufOut, m_nWide, m_nTall );
		bufTemp.Swap( bufOut );
	}

	if ( m_nWide == 0 || m_nTall == 0 )
	{
		Msg( "Failed to load image data from %s\n", pchFilePath );
		return false;
	}

	E2DTextureFormat surfaceFormat = k_EFormatRGBA8;
	if ( m_eFormat == k_EImageFormatA8 )
		surfaceFormat = k_EFormatA8;

	{
		VPROF_BUDGET( "CImageData::SetImageDataR8G8B8A8 - texture creation", VPROF_BUDGETGROUP_TENFOOT );
		m_pDevice->BCreateTexture( &m_pUITexture, bufTemp.Base(), m_nWide, m_nTall, m_nWide, surfaceFormat, k_EAlphaChannelType_Normal );
	}

	timer.End();


#ifndef PANORAMA_USE_S1WRAPPER

#if defined( SOURCE2_PANORAMA )
	int nLoadTime = (int)timer.GetDuration().GetMilliseconds64();
#else
	int nLoadTime = timer.GetDuration().GetMilliseconds();
#endif

#ifdef DEBUG
	if ( nLoadTime > 50 )
#else
	if ( nLoadTime > 20 )
#endif
	{
		Msg( "Slow image load - %s (dimensions %dx%d, took %d msec)\n", pchFilePath, m_nWide, m_nTall, nLoadTime );
	}

#endif

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CImageData::SetImageDataFromResourceFile( const char *pResourceFile, const UIImageLoadParams_t &loadParams )
{
#if defined( SOURCE2_PANORAMA )
	if ( !m_pDevice->BCreateTexture( &m_pUITexture, pResourceFile ) )
		return false;

	if ( !loadParams.ValidateMaxSize( m_pUITexture->GetOriginalWidth(), m_pUITexture->GetOriginalHeight() ) )
	{
		return false;
	}
	
	m_nWide = m_pUITexture->GetOriginalWidth();
	m_nTall = m_pUITexture->GetOriginalHeight();
	m_eSourceFormat = k_ESourceFormatVTEX;

	return true;
#else
	AssertMsg( false, "SetImageFromResourceFile should only be called in source2" );
	return false;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CImageData::SetImageDataFromEngineRT( const char *pchEngineRTName )
{
#if defined( SOURCE2_PANORAMA )
	if ( !m_pDevice->BCreateTextureEngineRTRef( &m_pUITexture, pchEngineRTName ) )
		return false;

	m_nWide = m_pUITexture->GetOriginalWidth();
	m_nTall = m_pUITexture->GetOriginalHeight();
	m_eSourceFormat = k_ESourceFormatEngineRT;

	return true;
#else
	AssertMsg( false, "SetImageFromResourceFile should only be called in source2" );
	return false;
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Used by BYieldingCOmpleteLoadOnMainThread to listen & wait for movie playback state changes
//-----------------------------------------------------------------------------
namespace panorama 
{
	class CWaitForMovieLoaded
	{
	public:
		CWaitForMovieLoaded( CImageData *pImageData, CUtlDelegate< void( CImageData *, bool ) > callback )
		{
			m_pImageData = pImageData;
			m_callback = callback;
			m_bSuccess = false;
		}

		void OnVideoPlayerEvent( EVideoPlayerEvent eEvent )
		{
			if ( eEvent == k_EVideoPlayerEventInit || eEvent == k_EVideoPlayerEventEnd )
			{
				m_bSuccess = (eEvent == k_EVideoPlayerEventInit);

				m_pImageData->OnMovieInitialized( this, m_callback, m_bSuccess );
			}
		}

		bool BSuccess() { return m_bSuccess; }

	private:
		CImageData *m_pImageData;
		CUtlDelegate< void( CImageData *, bool ) > m_callback;
		bool m_bSuccess;
	};
}


//-----------------------------------------------------------------------------
// Purpose: Finishes loading an image on the main thread
//-----------------------------------------------------------------------------
void CImageData::AsyncCompleteLoadOnMainThread( const byte *pubData, int cubData, CUtlDelegate< void( CImageData *, bool ) > callback )
{
	// only care about gifs which need to be movies...
	if ( m_eSourceFormat != k_ESourceFormatGIF )
	{
		callback( this, true );
		return;
	}

	Assert( m_pDevice );
#if !defined( SOURCE2_PANORAMA )
	m_pVideoPlayer.SetNoRef( new CPanoramaVideoPlayer( m_pDevice ) );
#else
	CPanoramaVideoPlayer *pPlayer = new CPanoramaVideoPlayer( m_pDevice );
	m_pVideoPlayer = pPlayer;
	pPlayer->Release();
#endif

	// install a listener for state changes
	CWaitForMovieLoaded *pWaitForMovie = new CWaitForMovieLoaded( this, callback );
	m_pVideoPlayer->RegisterEventCallback( UtlMakeDelegate( pWaitForMovie, &CWaitForMovieLoaded::OnVideoPlayerEvent ) );

	bool bSuccess = m_pVideoPlayer->BLoad( pubData, cubData );
	if ( bSuccess )
	{
		if ( !m_bAllowAnimation )
		{
			//
			// BUGBUG: This should be cleaner.
			// Rather than trying to pause the player after a single frame, set
			// the playback speed to something absurdly slow.  The player will
			// play the first frame and then never get around to playing the
			// second.
			//
			m_pVideoPlayer->SetPlaybackSpeed( 0.00000001f );
		}
		m_pVideoPlayer->SetRepeat( true );
		m_pVideoPlayer->Play();
	}
	else
	{
		m_pVideoPlayer->UnregisterEventCallback( UtlMakeDelegate( pWaitForMovie, &CWaitForMovieLoaded::OnVideoPlayerEvent ) );
		SAFE_DELETE( pWaitForMovie );

		callback( this, bSuccess );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Movie called us back saying it completed initialization (or failed somehow)
//-----------------------------------------------------------------------------
void CImageData::OnMovieInitialized( CWaitForMovieLoaded *pWaitForMovie, CUtlDelegate< void( CImageData *, bool ) > callback, bool bSuccessfulInitialize )
{
	if ( bSuccessfulInitialize )
	{
		uint32 unTextureWidth = 0;
		uint32 unTextureHeight = 0;
		m_pVideoPlayer->GetTextureSize( unTextureWidth, unTextureHeight );
		m_nWide = unTextureWidth;
		m_nTall = unTextureHeight;
	}

	m_pVideoPlayer->UnregisterEventCallback( UtlMakeDelegate( pWaitForMovie, &CWaitForMovieLoaded::OnVideoPlayerEvent ) );
	SAFE_DELETE( pWaitForMovie );

	callback( this, bSuccessfulInitialize );
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if movie that is playing
//-----------------------------------------------------------------------------
bool CImageData::BIsAnimating()
{
	return ( m_pVideoPlayer.IsValid() && m_pVideoPlayer->GetPlaybackState() != k_EVideoPlayerPlaybackStateStop );
}


//-----------------------------------------------------------------------------
// Purpose: how many pixels wide
//-----------------------------------------------------------------------------
int CImageData::GetWidth()
{
	if ( m_pVideoPlayer.IsValid() )
	{
		uint32 unTextureWidth = 0;
		uint32 unTextureHeight = 0;
		m_pVideoPlayer->GetTextureSize( unTextureWidth, unTextureHeight );
		m_nWide = unTextureWidth;
		m_nTall = unTextureHeight;
	}

	return m_nWide;
}


//-----------------------------------------------------------------------------
// Purpose: how many pixels high
//-----------------------------------------------------------------------------
int CImageData::GetHeight()
{
	if ( m_pVideoPlayer.IsValid() )
	{
		uint32 unTextureWidth = 0;
		uint32 unTextureHeight = 0;
		m_pVideoPlayer->GetTextureSize( unTextureWidth, unTextureHeight );
		m_nWide = unTextureWidth;
		m_nTall = unTextureHeight;
	}

	return m_nTall;
}


//-----------------------------------------------------------------------------
// Purpose: constructor
//-----------------------------------------------------------------------------
CImageResourceManager::CImageResourceManager( IUIRenderDevice *pDevice )
{
	m_bInited = false;
	m_pDevice = pDevice;
	m_bInited = true;
	m_bEventsRegistered = true;

	m_pImageDecodePool = new CImageDecodeWorkThreadPool();

	RegisterForUnhandledEvent( ImageUnreferenced(), this, &CImageResourceManager::OnImageUnreferenced );
}


//-----------------------------------------------------------------------------
// Purpose: destructor
//-----------------------------------------------------------------------------
CImageResourceManager::~CImageResourceManager()
{
	SAFE_DELETE( m_pImageDecodePool );
	Assert( !m_bInited );
}


//-----------------------------------------------------------------------------
// Purpose: Run frame
//-----------------------------------------------------------------------------
void CImageResourceManager::RunFrame()
{
	m_pImageDecodePool->RunFrame();

	FOR_EACH_VEC( m_vecLoaderTasksToStart, i )
	{
		m_vecLoaderTasksToStart[i]->StartLoad();
	}
	m_vecLoaderTasksToStart.RemoveAll();
}


//-----------------------------------------------------------------------------
// Purpose: Add an image to our tracking maps
//-----------------------------------------------------------------------------
void CImageResourceManager::AddImageToManager( CFileResource &resource, CImageProxySource *pImageData, const UIImageLoadParams_t &loadParams )
{
	UrlImageKey_t key;
	key.loadParams = loadParams;

	if ( resource.BIsValid() )
	{
		key.fileResource = resource;
		m_mapImagesByURL.Insert( key, pImageData );
		m_mapAllImages.Insert( (IImageSource*)pImageData, key );
	}
	else
	{
		// Just adding to the all images map then
		m_mapAllImages.Insert( (IImageSource*)pImageData, key );
	}


	// We'll just count one ref for both maps, since we always update them together
	pImageData->AddRef();
}


//-----------------------------------------------------------------------------
// Purpose: Remove an image from our tracking maps
//-----------------------------------------------------------------------------
bool CImageResourceManager::RemoveImageFromManager( IImageSource *pImage )
{
	if ( !pImage )
		return false;

	int iMap = m_mapAllImages.Find( pImage );
	if ( iMap == m_mapAllImages.InvalidIndex() )
		return false;

	UrlImageKey_t &key = m_mapAllImages[iMap];
	if ( key.fileResource.BIsValid() )
	{
		int iResourceMap = m_mapImagesByURL.Find( key );
		if ( iResourceMap != m_mapImagesByURL.InvalidIndex() )
		{
			m_mapImagesByURL.RemoveAt( iResourceMap );
		}
		else
		{
			AssertMsg(false, "Should have image in resource map if resource URL is valid!" );
		}
	}

	m_mapAllImages.RemoveAt( iMap );
	pImage->Release();

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: destructor
//-----------------------------------------------------------------------------
void CImageResourceManager::Shutdown()
{
	if ( m_bInited )
	{
		ShutdownForEngine();
		m_bInited = false;

		// This is pretty awful, but if we have a pending async file request whose completion is queued, then
		// we need to make sure that's processed before we delete the image loader task out from under it.
		// Otherwise we crash trying to reference the task's memory buffer, which is now bogus.
		UIEngine()->UIFileSystem()->RunFrame();

		SAFE_DELETE( m_pImageDecodePool );

		FOR_EACH_VEC( m_vecLoaderTasksToStart, i )
		{
			delete m_vecLoaderTasksToStart[i];
		}
		m_vecLoaderTasksToStart.RemoveAll();

		FOR_EACH_RBTREE_FAST( m_treeLoadTasks, i )
		{
			delete m_treeLoadTasks.Element( i );
		}
		m_treeLoadTasks.RemoveAll();

		FOR_EACH_MAP_FAST( m_mapAllImages, i )
		{
			m_mapAllImages.Key( i )->Release();
		}

		m_mapAllImages.RemoveAll();
		m_mapImagesByURL.RemoveAll();	

		m_pDevice = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: destructor
//-----------------------------------------------------------------------------
void CImageResourceManager::ShutdownForEngine()
{
	if ( m_bEventsRegistered )
	{
		UnregisterForUnhandledEvent( ImageUnreferenced(), this, &CImageResourceManager::OnImageUnreferenced );
		m_bEventsRegistered = false;
	}
}

#if defined( SOURCE2_PANORAMA )
//-----------------------------------------------------------------------------
// S2 UI Dev's don't want to do the image name compilation dance (or wonder why they have to when SF did not impose that penalty).
// The rule became very symmetric, the compilation step is hidden, they refer in all places (xml/css/code) using the content path.
//-----------------------------------------------------------------------------
bool CImageResourceManager::FixupFileResourceToCompiledImage( CFileResource &fileResource )
{
	if ( !fileResource.BIsLocalPath() )
	{
		// not a local path, cannot be a compiled image. no change
		return false;
	}

	if ( fileResource.BIsRawFilePath() )
	{
		// this path doesn't want name mangling
		return false;
	}
	
	// All local path images are expected to be compiled. The compiler has been hooked into various paths where images can be specified.
	const char *pExtension = V_GetFileExtensionSafe( fileResource.GetReferencePath().Get() );
#ifndef PANORAMA_USE_S1WRAPPER
	if ( !V_stricmp( pExtension, "png" ) || !V_stricmp( pExtension, "tga" ) || !V_stricmp( pExtension, "jpg" ) || !V_stricmp( pExtension, "psd" ) )
#else
	if ( !V_stricmp( pExtension, "png" ) || 
		 !V_stricmp( pExtension, "tga" ) || 
		 !V_stricmp( pExtension, "jpg" ) || 
		 !V_stricmp( pExtension, "psd" ) || 
		 !V_stricmp( pExtension, "iic" ) || 
		 !V_stricmp( pExtension, "vtf" ) ||
		 !V_stricmp( pExtension, "dds" ) )
#endif
	{
		// Change over from known images (that should be compiled) into the compiled variant.
		CUtlString compiledImageString = fileResource.GetReferencePath().StripExtension();
		compiledImageString.Append( CFmtStr( "_%s.vtex", pExtension ).Get() );

		fileResource.GetReferencePathForModify() = compiledImageString.Get();
		
		// changed
		return true;
	}

	if( !V_stricmp( pExtension, "svg" ) )
	{
		CUtlString compiledImageString = fileResource.GetReferencePath().StripExtension();
		compiledImageString.Append( ".vsvg" );
		fileResource.GetReferencePathForModify() = compiledImageString.Get();
		return true;
	}

	// no change
	return false;
}
#endif

//-----------------------------------------------------------------------------
// Purpose: load an image
//-----------------------------------------------------------------------------
IImageSource *CImageResourceManager::LoadImageFromURL( const IUIPanel *pPanel, const char *pchResourceURLDefault, const char *pchResourceURL, bool bPrioritizeLoad, EImageFormat imgFormatOut, const UIImageLoadParams_t &loadParams )
{
	CFileResource fileDefault( pchResourceURLDefault );	
	CFileResource fileResource( pchResourceURL );

	// nothing to load if both paths are invalid
	if ( !fileResource.BIsValid() && !fileDefault.BIsValid() )
		return NULL;

	// if the target is invalid, use default
	if ( !fileResource.BIsValid() )
		fileResource = fileDefault;

#if defined( SOURCE2_PANORAMA )
	FixupFileResourceToCompiledImage( fileDefault );
	FixupFileResourceToCompiledImage( fileResource );
#endif

	Assert( ThreadInMainThread() );

	EResourceImageType eType = DetermineResourceType( fileResource );
	
	// If type is unknown, then let's just give it a shot below... will usually figure it out based on contents.  
	// This happens for urls that just don't include an extension.
	if ( eType != k_EResourceImageTypeImage && eType != k_EResourceImageTypeUnknown )
	{
		Msg( "Invalid image URL (unknown type) to LoadImageFromURL: %s\n", pchResourceURL );
		return NULL;
	}

	return LoadImageInternal( pPanel, fileDefault, fileResource, bPrioritizeLoad, imgFormatOut, loadParams );
}


//-----------------------------------------------------------------------------
// Purpose: Find default image and load if needed
//-----------------------------------------------------------------------------
CImageProxySource *CImageResourceManager::GetDefaultImage( CFileResource &fileDefault, EImageFormat imgFormatOut, bool bAllowAnimation )
{
	CImageProxySource *pDefaultImage = NULL;
	if ( fileDefault.BIsValid()  )
	{
		if ( !fileDefault.BIsLocalPath() )
		{
			// Spew about this, should be a bug
			AssertMsg1( false, "Using a non-local image default %s, don't do that!", fileDefault.GetReferencePath().String() );
		}
		else if ( fileDefault.BIsValid() )
		{
			// Make sure that the default image is loaded, synchronously for now (could just move to front of work thread pool queue?)
			UrlImageKey_t key;
			key.fileResource = fileDefault;
			key.loadParams.m_bAllowAnimation = bAllowAnimation;

			int iDefault = m_mapImagesByURL.Find( key );
			if ( iDefault == m_mapImagesByURL.InvalidIndex() )
			{
				pDefaultImage = new CImageProxySource( NULL, imgFormatOut, NULL );
				AddImageToManager( fileDefault, pDefaultImage, key.loadParams );
				LoadLocalFileSynchronous( fileDefault, imgFormatOut, key.loadParams );
			}
			else
			{
				pDefaultImage = m_mapImagesByURL[iDefault];
				pDefaultImage->AddRef();
			}
		}
	}

	return pDefaultImage;
}


//-----------------------------------------------------------------------------
// Purpose: find an image and return it's source string
//-----------------------------------------------------------------------------
CUtlString CImageResourceManager::GetPchImageSourcePath( IImageSource *pImageSource )
{
	if ( !pImageSource )
		return "none";

	int iMap = m_mapAllImages.Find( pImageSource );
	if ( iMap != m_mapAllImages.InvalidIndex() )
	{
		UrlImageKey_t &key = m_mapAllImages[iMap];
		return key.fileResource.BIsValid() ? key.fileResource.GetReferencePath() : "(memory)";
	}

	return "none";
}


//-----------------------------------------------------------------------------
// Purpose: load an image
//-----------------------------------------------------------------------------
IImageSource *CImageResourceManager::LoadImageInternal( const IUIPanel *pPanel, CFileResource &fileDefault, CFileResource &fileResource, bool bPrioritizeLoad, EImageFormat imgFormatOut, const UIImageLoadParams_t &loadParams )
{
	VPROF_BUDGET( "CImageResourceManager::LoadImageInternal", VPROF_BUDGETGROUP_TENFOOT );
	// Default doesn't have to be valid, could be empty, but if it is valid, then it must
	// be a local file!
	CImageProxySource *pDefaultImage = GetDefaultImage( fileDefault, imgFormatOut, loadParams.m_bAllowAnimation );

	UrlImageKey_t key;
	key.fileResource = fileResource;
	key.loadParams = loadParams;

	int iIndex = m_mapImagesByURL.Find( key );
	if ( iIndex == m_mapImagesByURL.InvalidIndex() )
	{
		// Use default, but not if it's an A8 image or we were told not to
		CImageProxySource *pImage = new CImageProxySource( pDefaultImage ? pDefaultImage->GetImageSource() : NULL, imgFormatOut, pPanel );

		AddImageToManager( fileResource, pImage, loadParams );
		AddLoad( fileResource, imgFormatOut, bPrioritizeLoad, loadParams );

		SAFE_RELEASE( pDefaultImage );

		return pImage;
	}
	else
	{
		// panel data is already loaded, or in the process of loading
		CImageProxySource *pData = m_mapImagesByURL[iIndex];
		if ( pData->BIsLoaded() )
		{
			// Try it again, since something is trying to load again separately and maybe the HTTP failure was transient
			if ( fileResource.BIsHTTPURL() && pData->BFailedToLoad() )
			{
				pData->AddPanelForOnLoadEvent( pPanel );
				AddLoad( fileResource, imgFormatOut, bPrioritizeLoad, loadParams );
			}
			else if( pData->BFailedToLoad() )
				DispatchEventAsync( ImageFailedLoad(), pPanel, pData );

			// needs to be async because the caller will most likely take this pointer, store it as a member, then compare it in the event
			DispatchEventAsync( ImageLoaded(), pPanel, pData );
		}
		else
		{
			pData->AddPanelForOnLoadEvent( pPanel );
		}

		SAFE_RELEASE( pDefaultImage );

		pData->AddRef();
		return pData;
	}

	// unreachable
}


//-----------------------------------------------------------------------------
// Purpose: we have data UpdateImageResourcefor an image, happens on the main thread
//-----------------------------------------------------------------------------
bool CImageResourceManager::OnImageLoaded( CFileResource & resource, CImageData *pImage, const UIImageLoadParams_t &loadParams )
{
	UrlImageKey_t key;
	key.fileResource = resource;
	key.loadParams = loadParams;

	int iIndex = m_mapImagesByURL.Find( key );
	if ( iIndex == m_mapImagesByURL.InvalidIndex() )
	{
		// It's ok if this gets hit, it probably means some panel requested an image, then was destroyed
		// and the refcount went to 0 on that image before our async http/file load actually completed.  We
		// can just return and the data will get cleaned up since we don't take a ref.
		return false;
	}

	Assert( m_mapImagesByURL[iIndex] != NULL );
	m_mapImagesByURL[iIndex]->SetImageSource( pImage );

	m_mapImagesByURL[iIndex]->OnImageLoaded();
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: an image failed to load
//-----------------------------------------------------------------------------
bool CImageResourceManager::OnFailedImageLoad( CFileResource & resource, const UIImageLoadParams_t &loadParams )
{
	UrlImageKey_t key;
	key.fileResource = resource;
	key.loadParams = loadParams;

	int iIndex = m_mapImagesByURL.Find( key );
	if ( iIndex == m_mapImagesByURL.InvalidIndex() )
	{
		// It's ok if this gets hit, it probably means some panel requested an image, then was destroyed
		// and the refcount went to 0 on that image before our async http/file load actually completed.  We
		// can just return and the data will get cleaned up since we don't take a ref.
		return false;
	}

	Assert( m_mapImagesByURL[iIndex] != NULL );
	m_mapImagesByURL[iIndex]->OnFailedImageLoad();

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: We get this call asynchronously after an images ref count goes to 1,
// that normally means we have the only remaining reference.  Our job then is to
// check if the image is still unreferenced and free/remove it if so.
//-----------------------------------------------------------------------------
bool CImageResourceManager::OnImageUnreferenced( CImageProxySource *pImage )
{
	// make sure we own this image. If not, it could be invalid (owning manager shutdown while async message was queued) so don't touch the pointer
	int iMap = m_mapAllImages.Find( pImage );
	if ( iMap == m_mapAllImages.InvalidIndex() )
		return false;

	pImage->m_bUnreferencedEvent = false;

	// Has the ref count increased?  We are done if so.
	if ( pImage->GetRefCount() > 1 )
		return true;

	return RemoveImageFromManager( pImage );
}


void CImageResourceManager::OnResolutionChange( float fRelativeScalefactor )
{
	FOR_EACH_MAP_FAST( m_mapImagesByURL, i )
	{
		float fScaleFactor = m_mapImagesByURL.Key( i ).loadParams.m_fScaleFactor;
		if( fScaleFactor > 0 )
		{
			float newScaleFactor = fScaleFactor*fRelativeScalefactor;
			// Update scalefactor in both image maps
			m_mapImagesByURL.Key( i ).loadParams.m_fScaleFactor = newScaleFactor;
			IImageSource* pImageSource = m_mapImagesByURL.Element( i );
			int nIndex = m_mapAllImages.Find( pImageSource );
			if( nIndex != m_mapAllImages.InvalidIndex() )
			{
				m_mapAllImages.Element( nIndex ).loadParams.m_fScaleFactor = newScaleFactor;
			}
			ReloadChangedImage( pImageSource );
		}
	}
#if defined( SOURCE2_PANORAMA ) 
	RunFrame();
	m_pImageDecodePool->WaitForAllJobs();
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Helper job to load a file from a URL
//-----------------------------------------------------------------------------
namespace panorama
{
class CLoadFileURLTask : public CImageLoaderTask
{
public:
	CLoadFileURLTask( CFileResource &resource, CImageResourceManager *pLoader, IUIRenderDevice *pDevice, EImageFormat eFormat, bool bPrioritizeLoad, const UIImageLoadParams_t &loadParams ) 
		: CImageLoaderTask( pLoader ), m_HTTPRequestCompleted( this, &CLoadFileURLTask::OnHTTPRequestCompleted )
	{
		m_pResourceLoader = pLoader;
		m_FileResource = resource;
		m_eFormat = eFormat;
		m_pDevice = pDevice;
		m_bPrioritizeLoad = bPrioritizeLoad;
		m_loadParams = loadParams;
		m_hRequest = INVALID_HTTPREQUEST_HANDLE;
		m_hCookieContainer = INVALID_HTTPCOOKIE_HANDLE;
	}

	~CLoadFileURLTask()
	{
		if ( m_hRequest != INVALID_HTTPREQUEST_HANDLE )
		{
			ClientHTTP()->ReleaseHTTPRequest( m_hRequest );
			m_hRequest = INVALID_HTTPREQUEST_HANDLE;
		}

		if ( m_hCookieContainer != INVALID_HTTPCOOKIE_HANDLE )
		{
			ClientHTTP()->ReleaseCookieContainer( m_hCookieContainer );
			m_hCookieContainer = INVALID_HTTPCOOKIE_HANDLE;
		}
	}

	virtual void OnStartLoad() OVERRIDE
	{
		if ( !ClientHTTP() )
		{
#if !defined( PANORAMA_PUBLIC_STEAM_SDK )
			AssertMsg( false, "ClientHTTP() is NULL when using client interfaces." );
#endif
			// If ClientHTTP is NULL that means we're running with public ISteam interfaces and
			// Steam itself wasn't available. Just fail all requests
			HTTPRequestCompleted_t callback;
			callback.m_bRequestSuccessful = false;
			callback.m_hRequest = INVALID_HTTPREQUEST_HANDLE;
			callback.m_eStatusCode = k_EHTTPStatusCode501NotImplemented;
			OnHTTPRequestCompleted( &callback, false );
			return;
		}

		m_hRequest = ClientHTTP()->CreateHTTPRequest( k_EHTTPMethodGET, m_FileResource.GetReferencePath() );

		const CUtlVector<CUtlString> &vecCookies = m_FileResource.GetCookieHeadersForHTTPURL();
		if ( vecCookies.Count() )
		{
			m_hCookieContainer = ClientHTTP()->CreateCookieContainer( false );

			char rgchDomain[1024];
			V_ExtractDomainFromURL( m_FileResource.GetReferencePath(), rgchDomain, V_ARRAYSIZE( rgchDomain ) );

			FOR_EACH_VEC( vecCookies, iCookie )
			{
				ClientHTTP()->SetCookie( m_hCookieContainer, rgchDomain, "/", vecCookies[iCookie].String() );
			}

			ClientHTTP()->SetHTTPRequestCookieContainer( m_hRequest, m_hCookieContainer );
		}

		if ( m_bPrioritizeLoad )
		{
			// bugbug jmccaskey - should we try to respect this?  Steam doesn't let us prioritize right now... 
			// it would have to try to do so only for our own requests?  This isn't really used anyway?
			//UIEngine()->HTTPClient()->PrioritizeRequest( m_pHTTPRequest );
		}

		SteamAPICall_t hSteamAPICall;
		if ( ClientHTTP()->SendHTTPRequest( m_hRequest, &hSteamAPICall ) )
		{
			// Add call handle to get callback
			m_HTTPRequestCompleted.AddCall( hSteamAPICall );
		}
		else
		{
			// Not really ready, but this handles failure too
			HTTPRequestCompleted_t callback;
			callback.m_bRequestSuccessful = false;
			callback.m_hRequest = m_hRequest;
			callback.m_eStatusCode = k_EHTTPStatusCode500InternalServerError;
			OnHTTPRequestCompleted( &callback, false );
		}
				
	}

	STEAM_CALLRESULT( CLoadFileURLTask, HTTPRequestCompleted, HTTPRequestCompleted_t )
	{
		if ( pParam && pParam->m_bRequestSuccessful && pParam->m_eStatusCode == k_EHTTPStatusCode200OK )
		{
			uint32 unBodySize = 0;
			ClientHTTP()->GetHTTPResponseBodySize( m_hRequest, &unBodySize );
			CUtlBuffer bufBody;
			bufBody.EnsureCapacity( unBodySize );

			if ( ClientHTTP()->GetHTTPResponseBodyData( m_hRequest, (uint8*)bufBody.Base(), unBodySize ) && unBodySize >= 4 )
			{
				bufBody.SeekPut( CUtlBuffer::SEEK_HEAD, unBodySize );
				CImageDecodeWorkItem *pWorkItem = new CImageDecodeWorkItem( m_pDevice, bufBody, m_FileResource.GetReferencePath(), 0, 0, m_eFormat, m_loadParams, UtlMakeDelegate( this, &CLoadFileURLTask::OnImageDecodeCompletion ) );
				m_pResourceLoader->QueueImageDecodeWorkItem( pWorkItem );
			}
			else
			{
				OnImageDecodeCompletion( false, NULL, NULL );
			}
		}
		else
		{
			OnImageDecodeCompletion( false, NULL, NULL );
		}

	}

	void OnImageDecodeCompletion( bool bSuccess, CImageData *pImage, CUtlBuffer *pBufDecoded )
	{
		// decode the actual data
		bool bSuccessResult = false;
		if ( bSuccess )
		{
			pImage->AsyncCompleteLoadOnMainThread( (const byte*)pBufDecoded->Base(), pBufDecoded->TellPut(), UtlMakeDelegate( this, &CLoadFileURLTask::OnImageDataLoadCompletion ) );
			bSuccessResult = true;
		}
		
		if ( !bSuccessResult )
		{
			OnImageDataLoadCompletion( pImage, false );
		}
	}

	void OnImageDataLoadCompletion( CImageData *pImage, bool bSuccess )
	{
		if ( bSuccess )
		{
			m_pResourceLoader->OnImageLoaded( m_FileResource, pImage, m_loadParams );
		}
		else
		{
			m_pResourceLoader->OnFailedImageLoad( m_FileResource, m_loadParams );
		}

		SAFE_RELEASE( pImage );
		delete this;
	}


#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const char *pchName )
	{
		CImageLoaderTask::Validate( validator, pchName );
		VALIDATE_SCOPE();
		ValidateObj( m_FileResource );
		ValidateObj( m_HTTPRequestCompleted );
	}
#endif

private:
	HTTPRequestHandle m_hRequest;
	HTTPCookieContainerHandle m_hCookieContainer;

	IUIRenderDevice *m_pDevice;
	CImageResourceManager *m_pResourceLoader;
	CFileResource m_FileResource;
	EImageFormat m_eFormat;
	UIImageLoadParams_t m_loadParams;
	bool m_bPrioritizeLoad;
};

#if defined( SOURCE2_PANORAMA )

void OnImageManifestLoaded( HResourceManifest hManifest, void *pContext );

class CLoadFromVTexTask : public CImageLoaderTask
{

public:
	CLoadFromVTexTask( CFileResource &resource, CImageResourceManager *pLoader, IUIRenderDevice *pDevice, EImageFormat eFormat, const UIImageLoadParams_t &loadParams ) : CImageLoaderTask( pLoader )
	{
		m_pResourceLoader = pLoader;
		m_FileResource = resource;
		m_eFormat = eFormat;
		m_pDevice = pDevice;
		m_loadParams = loadParams;
		m_hManifest = RESOURCE_MANIFEST_HANDLE_INVALID;
	}

	virtual ~CLoadFromVTexTask()
	{
		if ( m_hManifest != RESOURCE_MANIFEST_HANDLE_INVALID )
		{
			g_pResourceSystem->SetManifestCompletionCallback( m_hManifest, NULL, 0 );
			g_pResourceSystem->DestroyResourceManifest( m_hManifest );
		}
	}

	void SetImageData()
	{
		// Make sure we have a valid resource
		ResourceHandle_t hResource = g_pResourceSystem->FindExistingResourceByName( m_FileResource.GetReferencePath(), RESOURCE_TYPE_TEXTURE );
		if ( ResourceIsError( hResource ) )
		{
			m_pResourceLoader->OnFailedImageLoad( m_FileResource, m_loadParams );
		}
		else
		{
			// route as a resource
			CImageData *pImage = new CImageData( m_pDevice, m_eFormat, m_loadParams.m_bAllowAnimation );
			if ( !pImage->SetImageDataFromResourceFile( m_FileResource.GetReferencePath(), m_loadParams ) )
			{
				m_pResourceLoader->OnFailedImageLoad( m_FileResource, m_loadParams );
			}
			else
			{
				m_pResourceLoader->OnImageLoaded( m_FileResource, pImage, m_loadParams );
			}
			SAFE_RELEASE( pImage );
		}
	}

	virtual void OnStartLoad() OVERRIDE
	{
		ResourceHandle_t hResource = g_pResourceSystem->FindExistingResourceByName( m_FileResource.GetReferencePath(), RESOURCE_TYPE_TEXTURE );
		if ( hResource == RESOURCE_HANDLE_INVALID || !ResourceIsLoaded( hResource ) )
		{
			const char *resources[1];
			resources[0] = m_FileResource.GetReferencePath();

			m_hManifest = g_pResourceSystem->CreateResourceManifest( 1, resources, RESOURCE_MANIFEST_LOAD_STREAMING_DATA, "Panorama", RESOURCE_MANIFEST_LOAD_PRIORITY_HIGH );
			if ( m_hManifest == RESOURCE_MANIFEST_HANDLE_INVALID )
			{
				m_pResourceLoader->OnFailedImageLoad( m_FileResource, m_loadParams );
				delete this;
				return;
			}

			g_pResourceSystem->SetManifestCompletionCallback( m_hManifest, &OnImageManifestLoaded, this );
		}
		else
		{
			SetImageData();
			delete this;
			return;
		}
	}

	void OnCurrentManifestLoaded( HResourceManifest hManifest )
	{
		Assert( hManifest == m_hManifest );
		SetImageData();
		delete this;
	}
	

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const char *pchName )
	{
		ValidateObj( m_bufData );
		ValidateObj( m_FileResource );
	}
#endif

private:
	IUIRenderDevice *m_pDevice;
	CFileResource m_FileResource;
	EImageFormat m_eFormat;
	CImageResourceManager *m_pResourceLoader;
	CUtlBuffer m_bufData;
	UIImageLoadParams_t m_loadParams;
	HResourceManifest m_hManifest;
};

void OnImageManifestLoaded( HResourceManifest hManifest, void *pContext )
{
	((CLoadFromVTexTask*)pContext)->OnCurrentManifestLoaded( hManifest );
}

#endif 


//-----------------------------------------------------------------------------
// Purpose: Helper job to load a file from disk
//-----------------------------------------------------------------------------
class CLoadFileLocalTask: public CImageLoaderTask
{
public:
	CLoadFileLocalTask( CFileResource &resource, CImageResourceManager *pLoader, IUIRenderDevice *pDevice, EImageFormat eFormat, const UIImageLoadParams_t &loadParams ) : CImageLoaderTask( pLoader )
	{
		m_pResourceLoader = pLoader;
		m_FileResource = resource;
		m_eFormat = eFormat;
		m_pDevice = pDevice;
		m_loadParams = loadParams;
		m_hAsyncFileLoad = NULL;
	}

	virtual ~CLoadFileLocalTask()
	{
		if ( m_hAsyncFileLoad )
			UIEngine()->UIFileSystem()->CancelLoadFileIntoBufferAsync( m_hAsyncFileLoad );
	}

	virtual void OnStartLoad() OVERRIDE
	{
		Assert( m_FileResource.BIsLocalPath() );
#ifndef PANORAMA_USE_S1WRAPPER 
		const char *pchFile = m_FileResource.GetReferencePath();
#else
		m_s1Filepath.Format( "materials/%s", m_FileResource.GetReferencePath().Get() );
		const char *pchFile = m_s1Filepath.String();
#endif

		m_hAsyncFileLoad = UIEngine()->UIFileSystem()->LoadFileIntoBufferAsync( pchFile, m_bufData, false, UtlMakeDelegate( this, &CLoadFileLocalTask::OnFileLoadCompletion ) );
	}

	void OnFileLoadCompletion( const char *pchFile, CUtlBuffer &bufData, bool bFileLoadSuccess )
	{
		// Should get called back with our buffer, otherwise, WTF
		Assert( &bufData == &m_bufData );
		m_hAsyncFileLoad = NULL;

		if ( bFileLoadSuccess && bufData.TellPut() >= 4 )
		{
			CImageDecodeWorkItem *pWorkItem = new CImageDecodeWorkItem( m_pDevice, bufData, m_FileResource.GetReferencePath(), 0, 0,
				m_eFormat, m_loadParams, UtlMakeDelegate( this, &CLoadFileLocalTask::OnImageDecodeCompletion ) );
#if defined( SOURCE2_PANORAMA ) 
			pWorkItem->UseAsyncFilesystemDeallocator();
#endif
			m_pResourceLoader->QueueImageDecodeWorkItem( pWorkItem );
		}
		else
		{
#if defined( SOURCE2_PANORAMA )
			// If we got some data we need to be careful to release it properly
			// back to the filesystem instead of just letting m_bufData free it
			// as this is an aligned allocation and not a regular allocation.
			if ( bFileLoadSuccess )
			{
				g_pAsyncFileSystem->ReleaseBuffer( m_bufData.DetachMemory() );
			}
#endif
			
			OnImageDecodeCompletion( false, NULL, NULL );
		}
	}

	void OnImageDecodeCompletion( bool bSuccess, CImageData *pImage, CUtlBuffer *pBufDecoded )
	{
		if ( bSuccess )
			pImage->AsyncCompleteLoadOnMainThread( (const byte*)pBufDecoded->Base(), pBufDecoded->TellPut(), UtlMakeDelegate( this, &CLoadFileLocalTask::OnImageDataLoadCompletion ) );
		else
			OnImageDataLoadCompletion( pImage, bSuccess );
	}

	void OnImageDataLoadCompletion( CImageData *pImage, bool bLoadSucccess )
	{
		if ( bLoadSucccess )
		{
			m_pResourceLoader->OnImageLoaded( m_FileResource, pImage, m_loadParams );
		}
		else
		{
			m_pResourceLoader->OnFailedImageLoad( m_FileResource, m_loadParams );
		}
		SAFE_RELEASE( pImage );
		delete this;
	}

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const char *pchName )
	{
		ValidateObj( m_bufData );
		ValidateObj( m_FileResource );
	}
#endif

protected:
	IUIRenderDevice *m_pDevice;
	CFileResource m_FileResource;
	EImageFormat m_eFormat;
	CImageResourceManager *m_pResourceLoader;
	CUtlBuffer m_bufData;
	UIImageLoadParams_t m_loadParams;
	HLOADINTOBUFFER m_hAsyncFileLoad;
#ifdef PANORAMA_USE_S1WRAPPER
	CUtlString m_s1Filepath;
#endif
};


void OnSvgManifestLoaded( HResourceManifest hManifest, void *pContext );

class CLoadFromSvgTask : public CImageLoaderTask
{
public:
	CLoadFromSvgTask( CFileResource &resource, CImageResourceManager *pLoader, IUIRenderDevice *pDevice, EImageFormat eFormat, const UIImageLoadParams_t &loadParams ) 
		: CImageLoaderTask( pLoader )
	{
		m_pResourceLoader = pLoader;
		m_FileResource = resource;
		m_eFormat = eFormat;
		m_pDevice = pDevice;
		m_loadParams = loadParams;
		m_hManifest = RESOURCE_MANIFEST_HANDLE_INVALID;
	}

	virtual ~CLoadFromSvgTask()
	{
		if ( m_hManifest != RESOURCE_MANIFEST_HANDLE_INVALID )
		{
			g_pResourceSystem->SetManifestCompletionCallback( m_hManifest, NULL, 0 );
			g_pResourceSystem->DestroyResourceManifest( m_hManifest );
		}
	}

	virtual void OnStartLoad() OVERRIDE
	{
		ResourceHandle_t hResource = g_pResourceSystem->FindExistingResourceByName( m_FileResource.GetReferencePath(), RESOURCE_TYPE_VECTOR_GRAPHIC );
		if ( hResource == RESOURCE_HANDLE_INVALID || !ResourceIsLoaded( hResource ) )
		{
			const char *resources[1];
			resources[0] = m_FileResource.GetReferencePath();

			m_hManifest = g_pResourceSystem->CreateResourceManifest( 1, resources, RESOURCE_MANIFEST_LOAD_STREAMING_DATA, "Panorama", RESOURCE_MANIFEST_LOAD_PRIORITY_HIGH );
			if ( m_hManifest == RESOURCE_MANIFEST_HANDLE_INVALID )
			{
				OnSvgDecodeCompletion( false, NULL, NULL );
				return;
			}

			g_pResourceSystem->SetManifestCompletionCallback( m_hManifest, &OnSvgManifestLoaded, this );
		}
		else
		{
			StartSvgDecode();
		}
	}

	void OnCurrentManifestLoaded( HResourceManifest hManifest )
	{
		Assert( hManifest == m_hManifest );
		StartSvgDecode();
	}

	void StartSvgDecode()
	{
		// Make sure we have a valid resource
		ResourceHandle_t hResource = g_pResourceSystem->FindExistingResourceByName( m_FileResource.GetReferencePath(), RESOURCE_TYPE_VECTOR_GRAPHIC );
		if ( ResourceIsError( hResource ) )
		{
			OnSvgDecodeCompletion( false, NULL, NULL );
		}
		else
		{
			// Note that LoadFileIntoBuffer should not read the file from disk at this point as it is 
			// already registered with the resource system
			if ( UIEngine()->UIFileSystem()->LoadFileIntoBuffer( m_FileResource.GetReferencePath(), m_bufData, true ) )
			{
				CImageDecodeWorkItem *pWorkItem = new CImageDecodeWorkItem( m_pDevice, m_bufData, m_FileResource.GetReferencePath(), 0, 0,
					m_eFormat, m_loadParams, UtlMakeDelegate( this, &CLoadFromSvgTask::OnSvgDecodeCompletion ) );
				pWorkItem->m_srcFormat = k_ESourceFormatSVG;
				m_pResourceLoader->QueueImageDecodeWorkItem( pWorkItem );
			}
			else
			{
				// Should not happen as the resource should be in the resource system
				Assert( 0 );
				OnSvgDecodeCompletion( false, NULL, NULL );
			}

		}
	}

	void OnSvgDecodeCompletion( bool bSuccess, CImageData *pImage, CUtlBuffer *pBufDecoded )
	{
		if ( bSuccess )
			pImage->AsyncCompleteLoadOnMainThread( (const byte*)pBufDecoded->Base(), pBufDecoded->TellPut(), UtlMakeDelegate( this, &CLoadFromSvgTask::OnSvgDataLoadCompletion ) );
		else
			OnSvgDataLoadCompletion( pImage, bSuccess );
	}

	void OnSvgDataLoadCompletion( CImageData *pImage, bool bLoadSucccess )
	{
		if ( bLoadSucccess )
		{
			m_pResourceLoader->OnImageLoaded( m_FileResource, pImage, m_loadParams );
		}
		else
		{
			m_pResourceLoader->OnFailedImageLoad( m_FileResource, m_loadParams );
		}
		SAFE_RELEASE( pImage );
		delete this;
	}

private:

	IUIRenderDevice *m_pDevice;
	CFileResource m_FileResource;
	EImageFormat m_eFormat;
	CImageResourceManager *m_pResourceLoader;
	UIImageLoadParams_t m_loadParams;
	CUtlBuffer m_bufData;
	HResourceManifest m_hManifest;
};

void OnSvgManifestLoaded( HResourceManifest hManifest, void *pContext )
{
	( (CLoadFromSvgTask*)pContext )->OnCurrentManifestLoaded( hManifest );
}


//-----------------------------------------------------------------------------
// Purpose: Job to load a texture from in-memory data
//-----------------------------------------------------------------------------
class CLoadInMemoryImageTask : public CImageLoaderTask
{
public:
	CLoadInMemoryImageTask( CImageProxySource *pImageSourceTarget, const CUtlBuffer &bufRGBA, CImageResourceManager *pLoader, IUIRenderDevice *pDevice, EImageFormat eFormat, uint32 nWide, uint32 nTall, const UIImageLoadParams_t &loadParams ) : CImageLoaderTask( pLoader )
	{
		m_pResourceLoader = pLoader;
		m_pImageSourceTarget = pImageSourceTarget;
		m_pImageSourceTarget->AddRef();
		m_bufRGBA.Put( bufRGBA.Base(), bufRGBA.TellPut() );
		m_eFormat = eFormat;
		m_pDevice = pDevice;
		m_nWide = nWide;
		m_nTall = nTall;
		m_loadParams = loadParams;
	}

	virtual ~CLoadInMemoryImageTask()
	{
		m_pImageSourceTarget->Release();
	}

	virtual void OnStartLoad() OVERRIDE
	{
		bool bSuccess = false;
		
		if ( m_bufRGBA.TellPut() >= 4 )
		{
			if ( ( m_eFormat == k_EImageFormatB8G8R8A8_PreMultiplied ) || ( m_eFormat == k_EImageFormatB8G8R8A8 ) )
			{
				CImageData *pNewImage = new CImageData( m_pDevice, k_EImageFormatR8G8B8A8, m_loadParams.m_bAllowAnimation );
				bSuccess = pNewImage->SetImageDataB8G8R8A8( m_bufRGBA, m_nWide, m_nTall, m_loadParams, ( ( m_eFormat == k_EImageFormatB8G8R8A8_PreMultiplied ) ? true : false ) );
				OnImageDataLoadCompletion( pNewImage, bSuccess );
				return;
			}
			else
			{
				Assert( m_eFormat == k_EImageFormatR8G8B8A8 );
				
				// Raw image, just swap in right away, don't need a decode work item, this will be fast
				if ( m_nWide && m_nTall && !m_loadParams.HasResize() )
				{
					CImageData *pNewImage = new CImageData( m_pDevice, k_EImageFormatR8G8B8A8, m_loadParams.m_bAllowAnimation );
					bSuccess = pNewImage->SetImageDataR8G8B8A8( (const byte*)m_bufRGBA.Base(), m_bufRGBA.TellPut(),  "", m_nWide, m_nTall, m_loadParams );
					OnImageDataLoadCompletion( pNewImage, bSuccess );
				}
				else
				{
					// We don't have width/height so we'll have to try decoding as png/jpg/tga/etc... do that as work item since it's expensive
					CImageDecodeWorkItem *pWorkItem = new CImageDecodeWorkItem( m_pDevice, m_bufRGBA, "", m_nWide, m_nTall,
						m_eFormat, m_loadParams, UtlMakeDelegate( this, &CLoadInMemoryImageTask::OnImageDecodeCompletion ) );

					m_pResourceLoader->QueueImageDecodeWorkItem( pWorkItem );
				}

				bSuccess = true;
			}
		}

		if ( !bSuccess )
			OnImageDecodeCompletion( false, NULL, NULL );
	}

	void OnImageDecodeCompletion( bool bSuccess, CImageData *pNewImage, CUtlBuffer *pBufDecoded )
	{
		if ( bSuccess )
			pNewImage->AsyncCompleteLoadOnMainThread( (const byte*)pBufDecoded->Base(), pBufDecoded->TellPut(), UtlMakeDelegate( this, &CLoadInMemoryImageTask::OnImageDataLoadCompletion ) );
		else	
			OnImageDataLoadCompletion( pNewImage, bSuccess );
	}

	void OnImageDataLoadCompletion( CImageData *pNewImage, bool bSuccess )
	{
		if ( bSuccess )
		{
			m_pImageSourceTarget->SetImageSource( pNewImage );
			m_pImageSourceTarget->OnImageLoaded();
		}
		else
		{
			m_pImageSourceTarget->OnFailedImageLoad();
		}

		SAFE_RELEASE( pNewImage );
		delete this;
	}

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const char *pchName )
	{
		ValidateObj( m_bufRGBA );
		ValidatePtrIfNeeded( m_pImageSourceTarget );
	}
#endif

private:
	CImageProxySource *m_pImageSourceTarget;
	IUIRenderDevice *m_pDevice;
	uint32 m_nWide, m_nTall;
	CUtlBuffer m_bufRGBA;
	EImageFormat m_eFormat;
	CImageResourceManager *m_pResourceLoader;
	UIImageLoadParams_t m_loadParams;
};
} // namespace panorama



//-----------------------------------------------------------------------------
// Purpose: enqueue a resource to load
//-----------------------------------------------------------------------------
void CImageResourceManager::AddLoad( CFileResource &resource, EImageFormat eFormat, bool bPrioritizeLoad, const UIImageLoadParams_t &loadParams )
{
	if ( resource.BIsLocalPath() )
	{
#if defined( SOURCE2_PANORAMA )
		const char *pchFile = resource.GetReferencePath();

		ResourceType_t resourceType = DeduceResourceTypeFromResourceName( pchFile );
		if ( resourceType == RESOURCE_TYPE_TEXTURE )
		{
			new CLoadFromVTexTask( resource, this, m_pDevice, eFormat, loadParams );
		}
		else if( resourceType == RESOURCE_TYPE_VECTOR_GRAPHIC )
		{
			new CLoadFromSvgTask( resource, this, m_pDevice, eFormat, loadParams );
		}
		else
		{
			new CLoadFileLocalTask( resource, this, m_pDevice, eFormat, loadParams );
		}
#else
		new CLoadFileLocalTask( resource, this, m_pDevice, eFormat, loadParams );
#endif
	}
	else if ( resource.BIsHTTPURL() )
	{
		new CLoadFileURLTask( resource, this, m_pDevice, eFormat, bPrioritizeLoad, loadParams );
	}
	else
	{
		Assert( !"Unsupported file resource type" );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Queue image reload
//-----------------------------------------------------------------------------
void CImageResourceManager::QueueImageDecodeWorkItem( CImageDecodeWorkItem *pWorkItem )
{ 
	m_pImageDecodePool->AddWorkItem( pWorkItem );

	// Also dispatch any other finished items right away so we don't get behind
	m_pImageDecodePool->RunFrame();
}


//-----------------------------------------------------------------------------
// Purpose: we have a file on disk, lets load it
//-----------------------------------------------------------------------------

#if defined( SOURCE2_PANORAMA )

bool CImageResourceManager::LoadLocalFileSynchronous( CFileResource & resource, EImageFormat eFormat, const UIImageLoadParams_t &loadParams )
{
	VPROF_BUDGET( "CImageResourceManager::LoadLocalFileSynchronous", VPROF_BUDGETGROUP_TENFOOT );

	// If we don't have the root resource, block and load it so it's available for SetImageDataFromResourceFile
	ResourceHandle_t hResource = g_pResourceSystem->FindExistingResourceByName( resource.GetReferencePath(), RESOURCE_TYPE_TEXTURE );
	if ( hResource == RESOURCE_HANDLE_INVALID )
	{
		g_pResourceSystem->BlockingLoadResourceByName( resource.GetReferencePath(), RESOURCE_TYPE_TEXTURE, "CImageResourceManager::LoadLocalFileSynchronous()" );
	}

	CImageData *pNewImage = new CImageData( m_pDevice, eFormat, loadParams.m_bAllowAnimation );
	if ( !pNewImage->SetImageDataFromResourceFile( resource.GetReferencePath(), loadParams ) )
	{
		SAFE_RELEASE( pNewImage );
		return OnFailedImageLoad( resource, loadParams );
	}

	bool bResult = OnImageLoaded( resource, pNewImage, loadParams );
	SAFE_RELEASE( pNewImage );

	return bResult;
}

#else

bool CImageResourceManager::LoadLocalFileSynchronous( CFileResource & resource, EImageFormat eFormat, bool bAllowAnimation )
{
	VPROF_BUDGET( "CImageResourceManager::LoadLocalFileSynchronous", VPROF_BUDGETGROUP_TENFOOT );
	const char *pchFile = NULL;
	pchFile = resource.GetReferencePath();
	CUtlBuffer fileData;
	if ( UIEngine()->UIFileSystem()->LoadFileIntoBuffer( pchFile, fileData, false ) )
	{
		// decode the actual data
		CImageData *pNewImage = new CImageData( m_pDevice, eFormat, bAllowAnimation );
		const char *pchPath = NULL;
		pchPath = resource.GetReferencePath();
		bool bSuccess = pNewImage->SetImageDataR8G8B8A8( (const byte *)fileData.Base(), fileData.TellPut(), pchPath );
		if ( !bSuccess )
		{
			SAFE_RELEASE( pNewImage );
			return OnFailedImageLoad( resource, -1, -1, bAllowAnimation );
		}

		// Loading gifs doesn't work yet, needs blocking version fo below commented out call
		Assert( !pNewImage->BIsGif() );
		if ( pNewImage->BIsGif() )
			bSuccess = false;
			
		//bSuccess = pNewImage->BlockingCompleteLoadOnMainThread( (const byte *)fileData.Base(), fileData.TellPut() );
		if ( !bSuccess )
		{
			SAFE_RELEASE( pNewImage );
			return OnFailedImageLoad( resource, -1, -1, bAllowAnimation );
		}
		
		bool bResult = OnImageLoaded( resource, pNewImage, -1, -1, bAllowAnimation );
		SAFE_RELEASE( pNewImage );

		return bResult;
	}
	else
	{
		return OnFailedImageLoad( resource, -1, -1, bAllowAnimation );
	}
}

#endif


//-----------------------------------------------------------------------------
// Purpose: checks for changed images, and reloads any that changed
//-----------------------------------------------------------------------------
void CImageResourceManager::ReloadChangedFile( const char *pchFile )
{
#ifndef PANORAMA_USE_S1WRAPPER
	CUtlString strResource = pchFile;
	if ( !StringAfterPrefix( strResource.Get(), "s2r://" ) )
	{
		strResource = "file://";
		strResource.Append(pchFile);
	}

	CFileResource updatedFile( strResource );
	FOR_EACH_MAP_FAST( m_mapImagesByURL, i )
	{
		if ( m_mapImagesByURL.Key(i).fileResource == updatedFile )
		{
			AddLoad( updatedFile, m_mapImagesByURL[i]->ImageFormat(), false, m_mapImagesByURL.Key(i).loadParams );
		}
	}
#else
	FOR_EACH_MAP_FAST( m_mapImagesByURL, i )
	{
		if ( m_mapImagesByURL.Key(i).fileResource.GetReferencePath() == pchFile )
		{
			AddLoad( m_mapImagesByURL.Key( i ).fileResource, m_mapImagesByURL[i]->ImageFormat(), false, m_mapImagesByURL.Key(i).loadParams );
		}
	}
#endif
}


//-----------------------------------------------------------------------------
// Purpose: reload from disk this already loaded image
//-----------------------------------------------------------------------------
void CImageResourceManager::ReloadChangedImage( IImageSource *pImageToReload )
{
	int iAllImage = m_mapAllImages.Find( pImageToReload );
	if ( iAllImage != m_mapAllImages.InvalidIndex() )
	{
		if ( m_mapAllImages.Key(iAllImage) == pImageToReload )
		{
			AddLoad( m_mapAllImages[ iAllImage ].fileResource,  m_mapAllImages.Key(iAllImage)->ImageFormat(), false, m_mapAllImages[ iAllImage ].loadParams );
		}
	}
}


IImageSource *CImageResourceManager::LoadImageFileFromMemory( const IUIPanel *pPanel, const char *pchResourceURLDefault, const CUtlBuffer &bufFile, const UIImageLoadParams_t &loadParams )
{
	CUtlBuffer bufRGBA;
	int nWide = 0;
	int nTall = 0;

	if ( bufFile.TellPut() > 4 && ( (const byte *)bufFile.Base() )[0] == 0x89 && ( (const byte *)bufFile.Base() )[1] == 0x50 && ( (const byte *)bufFile.Base() )[2] == 0x4E && ( (const byte *)bufFile.Base() )[3] == 0x47 )
	{
		// It's a PNG
		if ( !ConvertPNGToRGBA( (const byte *)bufFile.Base(), bufFile.TellPut(), bufRGBA, nWide, nTall ) )
			return NULL;
	}
	else if ( bufFile.TellPut() > 2 && ( (const byte *)bufFile.Base() )[0] == 0xFF && ( (const byte *)bufFile.Base() )[1] == 0xD8 )
	{
		// It's a JPG
		if ( !ConvertJpegToRGBA( (const byte *)bufFile.Base(), bufFile.TellPut(), bufRGBA, nWide, nTall ) )
			return NULL;
	}
	else if( bufFile.TellPut() > 2 && ((const byte *)bufFile.Base())[0] == '<' && ((const byte *)bufFile.Base())[1] == '?' )
	{
		// It's an SVG
		if( !ConvertSVGToRGBA( (const byte *)bufFile.Base(), bufFile.TellPut(), bufRGBA, nWide, nTall ) )
			return NULL;
	}
	else if ( bufFile.TellPut() > 5 && ( (const byte *)bufFile.Base() )[0] == 0x47 && ( (const byte *)bufFile.Base() )[1] == 0x49 && ( (const byte *)bufFile.Base() )[2] == 0x46 && ( (const byte *)bufFile.Base() )[3] == 0x38 && ( (const byte *)bufFile.Base() )[4] == 0x39 )
	{
		// It's a TGA
		char *pchImageBytes = NULL;
		int nBytes;
		if ( LoadTGA( bufFile.TellPut(), (char *)bufFile.Base(), (byte **)&pchImageBytes, &nBytes, &nWide, &nTall ) )
		{
			bufRGBA.Put( pchImageBytes, nBytes );
			delete[] pchImageBytes;
		}
	}
	else
	{
		// No idea what this is
		return NULL;
	}

	if ( !loadParams.ValidateMaxSize( nWide, nTall ) )
	{
		return NULL;
	}
	
	return LoadImageFromMemory( pPanel, pchResourceURLDefault, bufRGBA, nWide, nTall, k_EImageFormatR8G8B8A8, loadParams );
}


//-----------------------------------------------------------------------------
// Purpose: load an image from already loaded rgba image data
//-----------------------------------------------------------------------------
IImageSource *CImageResourceManager::LoadImageFromMemory( const IUIPanel *pPanel, const char *pchResourceURLDefault, const CUtlBuffer &bufRGBA, int nWide, int nTall, EImageFormat imgFormatIn, const UIImageLoadParams_t &loadParams )
{
	VPROF_BUDGET( "CImageResourceManager::LoadImageFromMemory", VPROF_BUDGETGROUP_TENFOOT );

	if ( !bufRGBA.TellPut() )
		return NULL;

	// Not really using this right now since in-memory images are loaded synchronously, should use it if
	// we make the load async, which we ought to eventually.
	CFileResource fileDefault( pchResourceURLDefault );
	CImageProxySource *pDefaultImage = GetDefaultImage( fileDefault, k_EImageFormatR8G8B8A8, loadParams.m_bAllowAnimation );
	CImageProxySource *pImageData = new CImageProxySource( pDefaultImage ? pDefaultImage->GetImageSource() : NULL, k_EImageFormatR8G8B8A8, pPanel );
	SAFE_RELEASE( pDefaultImage );

	CFileResource resource;
	AddImageToManager( resource, pImageData, loadParams );

	new CLoadInMemoryImageTask( pImageData, bufRGBA, this, m_pDevice, imgFormatIn, nWide, nTall, loadParams );

	return pImageData;
}


//-----------------------------------------------------------------------------
// Purpose: load an image from an engine texture name 
//-----------------------------------------------------------------------------
IImageSource *CImageResourceManager::LoadImageFromEngineRT( const IUIPanel *pPanel, const char *pchEngineRTName, const UIImageLoadParams_t &loadParams )
{
	VPROF_BUDGET( "CImageResourceManager::LoadImageFromEngineRT", VPROF_BUDGETGROUP_TENFOOT );

	if ( !pchEngineRTName || pchEngineRTName[0] == '\0' )
	{
		return nullptr;
	}

	CImageProxySource *pImageData = new CImageProxySource( nullptr, k_EImageFormatR8G8B8A8, pPanel );

	CFileResource resource;
	AddImageToManager( resource, pImageData, loadParams );

	CImageData *pNewImage = new CImageData( m_pDevice, k_EImageFormatR8G8B8A8, loadParams.m_bAllowAnimation );
	if ( pNewImage->SetImageDataFromEngineRT( pchEngineRTName ) )
	{
		pImageData->SetImageSource( pNewImage );
		pImageData->OnImageLoaded();
	}
	else
	{
		pImageData->OnFailedImageLoad();
	}
	SAFE_RELEASE( pNewImage );

	return pImageData;
}



//-----------------------------------------------------------------------------
// Purpose: wrapper around underlying images we load
//-----------------------------------------------------------------------------
CImageProxySource::CImageProxySource( CImageData *pSource, EImageFormat format, const IUIPanel *pPanel )
{
	m_cRef = 1; 
	m_bUnreferencedEvent = false;
	m_pImageData = pSource;
	if ( m_pImageData )
		m_pImageData->AddRef();

	m_eFormat = format;
	if ( pPanel )
		m_vecPanelsForOnLoadEvents.AddToTail( pPanel );

	m_bLoaded = false;
	m_bFailedLoad = false;

	// Only register the type once
	if ( !UIEngine()->IsObjectTypeExposedToJavaScript( GetJSTypeName() ) )
	{
		CUtlAbstractDelegate del = UtlMakeDelegate( this, &CImageProxySource::SetupJSObjectTemplate ).GetAbstractDelegate();
		UIEngine()->ExposeObjectTypeToJavaScript( GetJSTypeName(), del );
	}

}


//-----------------------------------------------------------------------------
// Purpose: destructor
//-----------------------------------------------------------------------------
CImageProxySource::~CImageProxySource()
{
	Assert( 0 == m_cRef );
	SAFE_RELEASE( m_pImageData );
}


//-----------------------------------------------------------------------------
// Purpose: Set up Javascript accessors and methods
//-----------------------------------------------------------------------------
void CImageProxySource::SetupJSObjectTemplate()
{
	RegisterJSMethod( "GetWidth", PANORAMA_DELEGATE( &CImageProxySource::GetWidth ) );
	RegisterJSMethod( "GetHeight", PANORAMA_DELEGATE( &CImageProxySource::GetHeight ) );
	RegisterJSMethod( "ImageFormat", PANORAMA_DELEGATE( &CImageProxySource::ImageFormat ) );
	RegisterJSMethod( "BIsAnimating", PANORAMA_DELEGATE( &CImageProxySource::BIsAnimating ) );
}


//-----------------------------------------------------------------------------
// Purpose: how wide is the underlying image
//-----------------------------------------------------------------------------
int CImageProxySource::GetWidth()
{
	return m_pImageData ? m_pImageData->GetWidth() : 0;
}


//-----------------------------------------------------------------------------
// Purpose: how high is the underlying image
//-----------------------------------------------------------------------------
int CImageProxySource::GetHeight()
{
	return m_pImageData ? m_pImageData->GetHeight() : 0;
}


//-----------------------------------------------------------------------------
// Purpose: switch the existing image for this new one, manipulating ref count as appropriate
//-----------------------------------------------------------------------------
void CImageProxySource::SetImageSource( CImageData *pLoader )
{
	SAFE_RELEASE( m_pImageData );

	m_pImageData = pLoader;
	if ( m_pImageData )
		m_pImageData->AddRef();
}


//-----------------------------------------------------------------------------
// Purpose: dispatch on load events to everyone waiting on them
//-----------------------------------------------------------------------------
void CImageProxySource::OnImageLoaded()
{
	m_bLoaded = true;

	if ( !UIEngine() )
		return;

	FOR_EACH_VEC( m_vecPanelsForOnLoadEvents, i )
	{
		const IUIPanel *pPanel = m_vecPanelsForOnLoadEvents[i].Get();
		if ( pPanel )
		{
			if ( CUIPanel::BInLayoutLoad() )
				DispatchEventAsync( ImageLoaded(), pPanel, this );
			else
				DispatchEvent( ImageLoaded(), pPanel, this );
		}
	}
	m_vecPanelsForOnLoadEvents.Purge();
}


//-----------------------------------------------------------------------------
// Purpose: dispatch on failed to load events to everyone waiting on them
//-----------------------------------------------------------------------------
void CImageProxySource::OnFailedImageLoad()
{
	m_bLoaded = true;
	m_bFailedLoad = true;

	if ( !UIEngine() )
		return;

	FOR_EACH_VEC( m_vecPanelsForOnLoadEvents, i )
	{
		const IUIPanel *pPanel = m_vecPanelsForOnLoadEvents[i].Get();
		if ( pPanel )
		{
			if ( CUIPanel::BInLayoutLoad() )
				DispatchEventAsync( ImageFailedLoad(), pPanel, this );
			else
				DispatchEvent( ImageFailedLoad(), pPanel, this );
		}
	}
	m_vecPanelsForOnLoadEvents.Purge();
}


#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: validate mem
//-----------------------------------------------------------------------------
void CImageResourceManager::Validate( CValidator &validator, const tchar *pchName )
{
	VALIDATE_SCOPE();
	ValidateObj( m_mapAllImages );
	FOR_EACH_MAP_FAST( m_mapAllImages, i )
	{
		if ( !validator.IsClaimed( m_mapAllImages.Key(i) ))
			ValidatePtr( m_mapAllImages.Key(i) );

		ValidateObj( m_mapAllImages[i].fileResource );
	}

	ValidateObj( m_mapImagesByURL );
	FOR_EACH_MAP_FAST( m_mapImagesByURL, i )
	{
		ValidateObj( m_mapImagesByURL.Key(i).fileResource );
		if ( !validator.IsClaimed( m_mapImagesByURL[i] ))
			ValidatePtr( m_mapImagesByURL[i] );
	}

	ValidateObj( m_vecLoaderTasksToStart );
	FOR_EACH_VEC( m_vecLoaderTasksToStart, i )
	{
		ValidatePtr( m_vecLoaderTasksToStart[i] );
	}

	ValidateObj( m_treeLoadTasks );
	FOR_EACH_RBTREE_FAST( m_treeLoadTasks, i )
	{
		ValidatePtr( m_treeLoadTasks.Element( i ) );
	}

	ValidatePtr( m_pImageDecodePool );
}
#endif
