//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx.h"
#include "uienginesource2.h"
#include "uitoplevelwindowsource2.h"
#include "uisoundsystemsource2.h"
#include "uirenderdevicesource2.h"
#include "interfaces/interfaces.h"
#include "filesystem.h"
#include "resourcefile/introspectedtypemanager.h"
#include "resourcesystem/stronghandle.h"
#include "resourcesystem/iresourcesystem.h"
#include "resourcesystem/resourcemanifesthelpers.h"
#include "appframework/iapplication.h"
#include "SDL.h"
#include "assetsystem/iassetsystem.h"
#include "tier1/utldict.h"
#include "iimemanager.h"
#include "renderer/source2surface.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

CSteamAPIContext steamAPIContext;

struct ImageInfo_t
{
	uint32 m_nOriginalWidth;
	uint32 m_nOriginalHeight;
};

struct DirectoryWatch_t
{
	CDirWatcher m_DirectoryWatcher;
	CUtlString	m_FullPathToRoot;
};

struct FileWatch_t
{
	FileChangeCallback_t m_FileChangeCallback;
};

class CPanoramaStyle
{
public:
	CPanoramaStyle()
	{
		m_nCRC32 = 0;
		m_pData = NULL;
		m_nDataSize = 0;
	}

	CRC32_t		m_nCRC32;
	const char *m_pData;
	unsigned int m_nDataSize;	
};

class CPanoramaLayout
{
public:
	CPanoramaLayout()
	{
		m_nCRC32 = 0;
		m_pData = NULL;
		m_nDataSize = 0;
	}

	CRC32_t		m_nCRC32;
	const char *m_pData;
	unsigned int m_nDataSize;
};

class CPanoramaDynamicImages
{
public:
	CPanoramaDynamicImages()
	{
		m_nCRC32 = 0;
		m_pData = NULL;
		m_nDataSize = 0;
	}

	CRC32_t		m_nCRC32;
	const char *m_pData;
	unsigned int m_nDataSize;
};

class CPanoramaScript
{
public:
	CPanoramaScript()
	{
		m_nCRC32 = 0;
		m_pData = NULL;
		m_nDataSize = 0;
	}

	CRC32_t		m_nCRC32;
	const char *m_pData;
	unsigned int m_nDataSize;
};

class CVectorGraphic
{
public:
	CVectorGraphic()
	{
		m_nCRC32 = 0;
		m_pData = NULL;
		m_nDataSize = 0;
	}

	CRC32_t		m_nCRC32;
	const char *m_pData;
	unsigned int m_nDataSize;
};


struct ResourceMonitor_t
{
	ResourceMonitor_t()
	{
		m_nCRC32 = 0;
		m_pTextureAsset = NULL;
	}

	~ResourceMonitor_t()
	{
		// Release our reference
		m_StrongHandleVoid.Shutdown();
	}

	CRC32_t m_nCRC32;
	CUtlString	m_ContentFilename;
	CStrongHandleVoid m_StrongHandleVoid;
	IAsset *m_pTextureAsset;
};

class CPanoramaTypeManager : public IResourceTypeManager
{
public:
	CPanoramaTypeManager( ResourceType_t resourceType ) : m_ImageDict( k_eDictCompareTypeCaseInsensitive )
	{
		m_ResourceType = resourceType;
	}

	virtual bool Init( IResourceSystemUtils *pUtils ) OVERRIDE { return true; }
	virtual void Shutdown() OVERRIDE {}

	virtual bool RequiresPostLoadFixup() OVERRIDE { return true; }

	virtual bool RequiresManifestResourceFinishedCall() const OVERRIDE { return true; }
	virtual void NotifyManifestResourceFinished( ResourceHandle_t hResource, ResourceLoadType_t nLoadType ) OVERRIDE { }
	virtual void *GetErrorResource() OVERRIDE { return NULL; }

	virtual void AllocateResource( ResourceHandle_t hResource, ResourceId_t nId, const ResourceFileHeader_t *pHeader, IRD_RegisterResourceDataUtils *pUtils ) OVERRIDE;
	virtual void DeallocateResource( void *pResourceData, IResourceDeallocatorUtils *pDeallocUtils ) OVERRIDE;

	bool GetOriginalImageDimensions( const char *pResourceName, uint32 *pOriginalWidth, uint32 *pOriginalHeight );

private:
	ResourceType_t m_ResourceType;

	CUtlDict< ImageInfo_t, int > m_ImageDict;
};

void CPanoramaTypeManager::AllocateResource( ResourceHandle_t hResource, ResourceId_t nId, const ResourceFileHeader_t *pHeader, IRD_RegisterResourceDataUtils *pUtils )
{
    const ResourceBlockEntry_t *pDataBlock = Resource_GetBlockEntry( pHeader, RESOURCE_BLOCK_ID_DATA );
    Assert( pDataBlock != NULL );

    void *pDiskData = (void*)(pDataBlock->m_pBlockData.GetPtr());
    size_t nDiskDataSize = pDataBlock->m_nBlockSize;

	// alias to the disk data for ease of parsing out components
	CUtlBuffer buffer;
	buffer.SetExternalBuffer( pDiskData, ( int )nDiskDataSize, ( int )nDiskDataSize, CUtlBuffer::READ_ONLY );
	
	CRC32_t nCRC32 = buffer.GetUnsignedInt();

	// get the image mapping table
	int nNumImages = buffer.GetUnsignedShort();
	for ( int i = 0; i < nNumImages; i++ )
	{
		char imageFilename[MAX_PATH];
		imageFilename[0] = '\0';
		buffer.GetString( imageFilename, sizeof( imageFilename ) );

		int nOriginalWidth = buffer.GetUnsignedShort();
		int nOriginalHeight = buffer.GetUnsignedShort();

		// Version 3 and above of all panorama resource files contain a crc for each image. This
		// data is not used at runtime, it is used by the resource compiler to make building vpdi
		// files faster, but we must skip it here in order to parse the file properly.

#ifndef PANORAMA_USE_S1WRAPPER
		if ( pHeader->m_nResourceVersion >= 3 )
		{
			uint32 nImageCrc = buffer.GetUnsignedInt();
			NOTE_UNUSED( nImageCrc );
		}
#endif

		// We don't use the image dimensions from vpdi files anymore, the 
		// dimensions are embedded in the textures directly. However we still
		// need to read the values above in order to properly parse the buffer.
		if ( m_ResourceType != RESOURCE_TYPE_PANORAMA_DYNAMIC_IMAGES )
		{
			// ensure these are stored as resource names, since that's how we will look these up from HRenderTexture's resource names
			V_FixSlashes( imageFilename, '/' );

			int nDictIndex = m_ImageDict.Find( imageFilename );
			if ( nDictIndex == m_ImageDict.InvalidIndex() )
			{
				nDictIndex = m_ImageDict.Insert( imageFilename );
			}
			m_ImageDict[nDictIndex].m_nOriginalWidth = nOriginalWidth;
			m_ImageDict[nDictIndex].m_nOriginalHeight = nOriginalHeight;
		}
	}

	// a parse error would be reflected as an underflow
	// usually manifests as a stale compiled version (i.e. some failure along the content builder or user sync process)
	if ( !buffer.IsValid() )
	{
		char resourceName[MAX_PATH];
		resourceName[0] = '\0';
		g_pResourceSystem->GetResourceName( hResource, resourceName, sizeof( resourceName ) );
#ifndef PANORAMA_USE_S1WRAPPER	
		AssertMsg( false, "Failed to decode compiled data properly for '%s' (Resource Version:%d, Header Version:%d)\n", resourceName, pHeader->m_nResourceVersion, pHeader->m_nHeaderVersion );
#else
		Msg( "Failed to decode compiled data properly for '%s' (Resource Version:%d, Header Version:%d)\n", resourceName, 0, 0 );
#endif 
		return;
	}

	// remaining data is the relevant runtime
	int nRuntimeDataSize = ( int )( nDiskDataSize - buffer.TellGet() );

	// strip the CRC32 and the image table data prefix from the runtime
    void *pRuntimeData = MemAlloc_Alloc( nRuntimeDataSize );
    V_memcpy( pRuntimeData, (char*)buffer.Base() + buffer.TellGet(), nRuntimeDataSize );

	if ( m_ResourceType == RESOURCE_TYPE_PANORAMA_STYLE )
	{
		CPanoramaStyle *pStyle = new CPanoramaStyle();

		pStyle->m_nCRC32 = nCRC32;
		pStyle->m_pData = (char *)pRuntimeData;
		pStyle->m_nDataSize = nRuntimeDataSize;

		pUtils->SetFinalResourceData( pStyle );
	}
	else if ( m_ResourceType == RESOURCE_TYPE_PANORAMA_LAYOUT )
	{
		CPanoramaLayout *pLayout = new CPanoramaLayout();

		pLayout->m_nCRC32 = nCRC32;
		pLayout->m_pData = (char *)pRuntimeData;
		pLayout->m_nDataSize = nRuntimeDataSize;

		pUtils->SetFinalResourceData( pLayout );
	}
	else if ( m_ResourceType == RESOURCE_TYPE_PANORAMA_DYNAMIC_IMAGES )
	{
		CPanoramaDynamicImages *pDynamicImages = new CPanoramaDynamicImages();

		pDynamicImages->m_nCRC32 = nCRC32;
		pDynamicImages->m_pData = (char *)pRuntimeData;
		pDynamicImages->m_nDataSize = nRuntimeDataSize;

		pUtils->SetFinalResourceData( pDynamicImages );
	}
	else if ( m_ResourceType == RESOURCE_TYPE_PANORAMA_SCRIPT )
	{
		CPanoramaScript *pScript = new CPanoramaScript();

		pScript->m_nCRC32 = nCRC32;
		pScript->m_pData = (char *)pRuntimeData;
		pScript->m_nDataSize = nRuntimeDataSize;

		pUtils->SetFinalResourceData( pScript );
	}
	else
	{
		Assert( false );
	}
}

void CPanoramaTypeManager::DeallocateResource( void *pResourceData, IResourceDeallocatorUtils *pDeallocUtils )
{
	if ( pResourceData == NULL )
		return;

	if ( m_ResourceType == RESOURCE_TYPE_PANORAMA_STYLE )
	{
		CPanoramaStyle *pStyle = (CPanoramaStyle*)pResourceData;
		MemAlloc_Free( (void *)pStyle->m_pData );
		delete pStyle;
	}
	else if ( m_ResourceType == RESOURCE_TYPE_PANORAMA_LAYOUT )
	{
		CPanoramaLayout *pLayout = (CPanoramaLayout*)pResourceData;
		MemAlloc_Free( (void *)pLayout->m_pData );
		delete pLayout;
	}
	else if ( m_ResourceType == RESOURCE_TYPE_PANORAMA_DYNAMIC_IMAGES )
	{
		CPanoramaDynamicImages *pDynamicImages = (CPanoramaDynamicImages*)pResourceData;
		MemAlloc_Free( (void *)pDynamicImages->m_pData );
		delete pDynamicImages;
	}
	else if ( m_ResourceType == RESOURCE_TYPE_PANORAMA_SCRIPT )
	{
		CPanoramaScript *pDynamicImages = (CPanoramaScript*)pResourceData;
		MemAlloc_Free( (void *)pDynamicImages->m_pData );
		delete pDynamicImages;
	}
	else
	{
		Assert( false );
	}
}

bool CPanoramaTypeManager::GetOriginalImageDimensions( const char *pResourceName, uint32 *pOriginalWidth, uint32 *pOriginalHeight )
{
	// ensure a resource name
	CUtlString imageFilename = pResourceName;
	imageFilename.FixSlashes( '/' );

	int nDictIndex = m_ImageDict.Find( imageFilename );
	if ( nDictIndex == m_ImageDict.InvalidIndex() )
	{
		// not found
		return false;
	}

	if ( pOriginalWidth )
	{
		*pOriginalWidth = m_ImageDict[nDictIndex].m_nOriginalWidth;
	}

	if ( pOriginalHeight )
	{
		*pOriginalHeight = m_ImageDict[nDictIndex].m_nOriginalHeight;
	}

	return true;
}

namespace panorama
{

class CSource2UIFileSystem : public IUIFileSystem
{
public:
	// fully load the file into the buffer object
	virtual bool LoadFileIntoBuffer( const char *pchFile, CUtlBuffer &buf, bool bText, FileChangeCallback_t fileChangeCallback, uint nPadding = 0 );

	// fully load the 
	virtual HLOADINTOBUFFER LoadFileIntoBufferAsync( const char *pchFile, CUtlBuffer &buf, bool bText, CUtlDelegate< LoadFileIntoBufferCallback_t > del );

	// Cancel async load
	virtual bool CancelLoadFileIntoBufferAsync( HLOADINTOBUFFER hLoad ) OVERRIDE;

	// replace this file with the contents of the buffer object
	virtual bool SaveBufferToFile( CUtlBuffer &buf, const char *pchFile );

	// return true if this file is on disk (or in a resource file)
	virtual bool FileExists( const char *pchFile );

	// Returns true if input name restored, false if no change required
	virtual bool RestoreContentFilename( const char *pchFile, CUtlString &fixedFilename );
	virtual bool RestoreResourceFilename( const char *pchFile, CUtlString &fixedFilename );

	// Run frame on main thread
	virtual void RunFrame();

	void OnAsyncLoadCompletion( IAsyncFileRequest *pRequest, const char *pchFile, CUtlBuffer *pBuf, CUtlDelegate< LoadFileIntoBufferCallback_t > del );

	virtual char* LoadFromPanZip( const char* fname );

private:

	CIOCompletionQueue m_IOCompletionQueue;
};
CSource2UIFileSystem s_Source2Filesystem;


bool CSource2UIFileSystem::RestoreContentFilename( const char *pchFile, CUtlString &fixedFilename )
{
	fixedFilename = pchFile;

	// CSS/XML extensions are tricky because S2 wants them to be something else and panorama does not
	const char *pExtension = V_GetFileExtensionSafe( pchFile );
	const char *pPreferredExtension = NULL;
	if ( !V_stricmp( pExtension, "vcss" ) || !V_stricmp( pExtension, "vcss_c" ) )
	{
		// S2 mandates 'v' versions except ui authors don't want that, so put back to the expected content
		pPreferredExtension = "css";
	}
	else if ( !V_stricmp( pExtension, "vxml" ) || !V_stricmp( pExtension, "vxml_c" ) )
	{
		// S2 mandates 'v' versions except ui authors don't want that, so put back to the expected content
		pPreferredExtension = "xml";
	}
	else if ( !V_stricmp( pExtension, "vjs" ) || !V_stricmp( pExtension, "vjs_c" ) )
	{
		// S2 mandates 'v' versions except ui authors don't want that, so put back to the expected content
		pPreferredExtension = "js";
	}

	if ( pPreferredExtension )
	{
		fixedFilename = fixedFilename.StripExtension();
		fixedFilename += CFmtStr( ".%s", pPreferredExtension );
		
		return true;
	}

	// no change
	return false;
}

bool CSource2UIFileSystem::RestoreResourceFilename( const char *pchFile, CUtlString &fixedFilename )
{
	fixedFilename = pchFile;

	// CSS/XML extensions are tricky because S2 wants them to be something else and panorama does not
	const char *pExtension = V_GetFileExtensionSafe( pchFile );
	const char *pPreferredExtension = NULL;
	if ( !V_stricmp( pExtension, "css" ) || !V_stricmp( pExtension, "vcss_c" ) )
	{
		pPreferredExtension = "vcss";
	}
	else if ( !V_stricmp( pExtension, "xml" ) || !V_stricmp( pExtension, "vxml_c" ) )
	{
		pPreferredExtension = "vxml";
	}
	else if ( !V_stricmp( pExtension, "js" ) || !V_stricmp( pExtension, "vjs_c" ) )
	{
		pPreferredExtension = "vjs";
	}
		
	if ( pPreferredExtension )
	{
		fixedFilename = fixedFilename.StripExtension();
		fixedFilename += CFmtStr( ".%s", pPreferredExtension );
		
		return true;
	}

	// no change
	return false;
}


void CSource2UIFileSystem::RunFrame()
{
	m_IOCompletionQueue.ProcessAllResultCallbacks();
}

void CSource2UIFileSystem::OnAsyncLoadCompletion( IAsyncFileRequest *pRequest, const char *pchFile, CUtlBuffer *pBuf, CUtlDelegate< LoadFileIntoBufferCallback_t > del )
{
	if ( pRequest && pRequest->GetRequestStatus() == ASYNC_REQUEST_OK )
	{
		pRequest->KeepResultBuffer();
		pBuf->AssumeMemory( pRequest->GetResultBuffer(), ( int )pRequest->GetResultBufferSize(), ( int )pRequest->GetResultBufferSize(), CUtlBuffer::READ_ONLY );
		del( pchFile, *pBuf, true );
	}
	else
	{
#ifdef PANORAMA_USE_S1WRAPPER
		Warning( "Failed to asynchronously load %s\n", pchFile );
#endif
		del( pchFile, *pBuf, false );
	}
}


char* CSource2UIFileSystem::LoadFromPanZip( const char* fname )
{
	return g_pResourceSystem->LoadFromPanZip( fname );
}

HLOADINTOBUFFER CSource2UIFileSystem::LoadFileIntoBufferAsync( const char *pchFile, CUtlBuffer &buf, bool bText, CUtlDelegate< LoadFileIntoBufferCallback_t > del )
{
#ifndef PANORAMA_USE_S1WRAPPER
	IAsyncFileRequest *pRequest = AsyncReadFile( pchFile, NULL );
#else
	IAsyncFileRequest *pRequest = AsyncReadFile( pchFile );
#endif
	pRequest->AssignCallbackAndQueue( &m_IOCompletionQueue, CreateFunctor( this, &CSource2UIFileSystem::OnAsyncLoadCompletion, pRequest, pchFile, &buf, del ) );
	g_pAsyncFileSystem->SubmitAsyncFileRequest( pRequest );


	return pRequest;
}


bool CSource2UIFileSystem::CancelLoadFileIntoBufferAsync( HLOADINTOBUFFER hLoad )
{
#ifndef PANORAMA_USE_S1WRAPPER
	g_pAsyncFileSystem->ReleaseAsyncRequest( (IAsyncFileRequest*)hLoad, ASYNC_RELEASE_BEHAVIOR_CANCEL_CALLBACKS );
#else
	g_pAsyncFileSystem->ReleaseAsyncRequest( (IAsyncFileRequest*)hLoad );
#endif
	return true;
}


bool CSource2UIFileSystem::LoadFileIntoBuffer( const char *pchFile, CUtlBuffer &buf, bool bText, FileChangeCallback_t fileChangeCallback, uint nPadding ) 
{
	buf.SetBufferType( bText, false );

	// filename and extensions can be provided from various sources (author or compiler), so do the right expected thing here
	// regardless of incoming extension (we play games with this in order to allow content to have non 'v' prefixed versions), fixup to EXPECTED extension
	CUtlString filenameString;
	RestoreResourceFilename( pchFile, filenameString );

	// identify expected compiled resources	
	ResourceType_t nResourceType = DeduceResourceTypeFromResourceName( filenameString.Get() );
	if ( nResourceType == RESOURCE_TYPE_PANORAMA_STYLE || nResourceType == RESOURCE_TYPE_PANORAMA_LAYOUT || nResourceType == RESOURCE_TYPE_PANORAMA_SCRIPT 
		|| nResourceType == RESOURCE_TYPE_VECTOR_GRAPHIC )
	{
		// expected compiled objects get passed onto the resource system		
		bool bSuccess = false;

		HResourceManifest hResourceManifest = RESOURCE_MANIFEST_HANDLE_INVALID;
		ResourceHandle_t hResource = g_pResourceSystem->FindExistingResourceByName( filenameString.Get(), nResourceType );
		if ( hResource == RESOURCE_HANDLE_INVALID )
		{
			const char *resources[1];
			resources[0] = filenameString.Get();

			hResourceManifest = g_pResourceSystem->CreateResourceManifest( 1, resources, RESOURCE_MANIFEST_LOAD_STREAMING_DATA, "Panorama", RESOURCE_MANIFEST_LOAD_PRIORITY_HIGH );
			if ( hResourceManifest != RESOURCE_MANIFEST_HANDLE_INVALID )
			{
				g_pResourceSystem->ForceSynchronizationAndBlockUntilManifestLoaded( hResourceManifest ); 
			}


			// 7ls :: Revisit this logic
			hResource = g_pResourceSystem->FindExistingResourceByName( filenameString.Get(), nResourceType );
			if ( hResource != RESOURCE_HANDLE_INVALID )
			{
				( (CUIEngineSource2*)UIEngine() )->MonitorResourceForChanges( hResource );
			}


		}

		if ( nResourceType == RESOURCE_TYPE_PANORAMA_STYLE )
		{
			HPanoramaStyle hPanoramaStyle = g_pResourceSystem->FindExistingResourceByName< RESOURCE_TYPE_PANORAMA_STYLE >( filenameString.Get() );
			if ( hPanoramaStyle.IsValid() && !hPanoramaStyle.IsError() )
			{
				CPanoramaStyle *pPanoramaStyle = (CPanoramaStyle*)hPanoramaStyle.GetData();
				buf.Put( pPanoramaStyle->m_pData, pPanoramaStyle->m_nDataSize ); 
				bSuccess = true;
			}

			((CUIEngineSource2*)UIEngine())->MonitorResourceForChanges( hPanoramaStyle );
		}
		else if ( nResourceType == RESOURCE_TYPE_PANORAMA_LAYOUT )
		{
			HPanoramaLayout hPanoramaLayout = g_pResourceSystem->FindExistingResourceByName< RESOURCE_TYPE_PANORAMA_LAYOUT >( filenameString.Get() );
			if ( hPanoramaLayout.IsValid() && !hPanoramaLayout.IsError() )
			{
				CPanoramaLayout *pPanoramaLayout = (CPanoramaLayout*)hPanoramaLayout.GetData();
				buf.Put( pPanoramaLayout->m_pData, pPanoramaLayout->m_nDataSize ); 
				bSuccess = true;
			}

			char szLayoutFilename[ MAX_PATH ];
			g_pResourceSystem->GetActualFileName( hPanoramaLayout, szLayoutFilename, sizeof( szLayoutFilename ) );
			#define CUSTOM_GAME_LAYOUT_DIRECTORY ( "panorama/layout/custom_game/" )
			if ( V_strstr( szLayoutFilename, CUSTOM_GAME_LAYOUT_DIRECTORY ) == nullptr && g_pApplication->IsFileInAddon( szLayoutFilename ) )
			{
				Plat_FatalError( "Error loading %s: Addons can only add layouts in the %s subdirectory.", szLayoutFilename, CUSTOM_GAME_LAYOUT_DIRECTORY );
				return false;
			}

			((CUIEngineSource2*)UIEngine())->MonitorResourceForChanges( hPanoramaLayout );
		}
		else if ( nResourceType == RESOURCE_TYPE_PANORAMA_SCRIPT )
		{
			HPanoramaScript hPanoramaScript = g_pResourceSystem->FindExistingResourceByName< RESOURCE_TYPE_PANORAMA_SCRIPT >( filenameString.Get() );
			if ( hPanoramaScript.IsValid() && !hPanoramaScript.IsError() )
			{
				CPanoramaScript *pPanoramaScript = (CPanoramaScript*)hPanoramaScript.GetData();
				buf.Put( pPanoramaScript->m_pData, pPanoramaScript->m_nDataSize ); 
				bSuccess = true;
			}

			((CUIEngineSource2*)UIEngine())->MonitorResourceForChanges( hPanoramaScript );
		}
		else if( nResourceType == RESOURCE_TYPE_VECTOR_GRAPHIC )
		{
			HVectorGraphic hSvg = g_pResourceSystem->FindExistingResourceByName< RESOURCE_TYPE_VECTOR_GRAPHIC >( filenameString.Get() );
			if( hSvg.IsValid() && !hSvg.IsError() )
			{
				CVectorGraphic *pSvg = (CVectorGraphic*)hSvg.GetData();
				buf.Put( pSvg->m_pData, pSvg->m_nDataSize );
				bSuccess = true;
			}

			((CUIEngineSource2*)UIEngine())->MonitorResourceForChanges( hSvg );
		}

		if ( hResourceManifest != RESOURCE_MANIFEST_HANDLE_INVALID )
		{
			g_pResourceSystem->DestroyResourceManifest( hResourceManifest );
		}

		return bSuccess;
	}
	else
	{
		((CUIEngineSource2*)UIEngine())->MonitorFileForChanges( pchFile, fileChangeCallback );
	}


	// Code to pad the buffer in the case where we want to append some data to it but not have to reallocate/grow the buffer
	if ( nPadding > 0 )
	{
		Assert( !bText ); // we are going to read the file in binary here; we don't support padding text IO
		FileHandle_t hFile = g_pFullFileSystem->Open( pchFile, "rb", nullptr );
		if ( hFile != FILESYSTEM_INVALID_HANDLE )
		{
			uint nSize = g_pFullFileSystem->Size( hFile );
			buf.EnsureCapacity( buf.TellPut() + nSize + nPadding );
			int nBytesRead = g_pFullFileSystem->Read( buf.PeekPut(), nSize, hFile );
			buf.SeekPut( CUtlBuffer::SEEK_CURRENT, nBytesRead );
			g_pFullFileSystem->Close( hFile );
			return nBytesRead > 0;
		}
	}
	return g_pFullFileSystem->ReadFile( pchFile, NULL, buf );
}

bool CSource2UIFileSystem::SaveBufferToFile( CUtlBuffer &buf, const char *pchFile ) 
{
	return g_pFullFileSystem->WriteFile( pchFile, NULL, buf );
}

bool CSource2UIFileSystem::FileExists( const char *pchFile ) 
{
	return g_pFullFileSystem->FileExists( pchFile, NULL );
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CUIEngineSource2::CUIEngineSource2() : 
	m_bInShutdown( false ),
	m_MonitoredFiles( k_eDictCompareTypeCaseInsensitive )
{
	m_pFileSystem = &s_Source2Filesystem;

	m_pPanoramaStyleTypeManager = NULL;
	m_pPanoramaLayoutTypeManager = NULL;
	m_pPanoramaDynamicImagesTypeManager = NULL;
	m_pPanoramaScriptTypeManager = NULL;

	m_pRenderDevice = new CUIRenderDeviceSource2();
	m_pImageResourceManager = new CImageResourceManager( m_pRenderDevice );

	m_hRequiredManifest = RESOURCE_MANIFEST_HANDLE_INVALID;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CUIEngineSource2::~CUIEngineSource2()
{
	// May or may not have already shutdown
	Shutdown();
}


//-----------------------------------------------------------------------------
// Purpose: Do one time initialization of the engine
//-----------------------------------------------------------------------------
bool CUIEngineSource2::BInitialize()
{
	// Anything needed here?  Not for now?
	bool bRet = steamAPIContext.Init();
	if ( !Plat_IsInTestMode() && ( bRet == false ) )
	{
		Log_Msg( LOG_PANORAMA, "Steam API unavailable\n" );
	}

	// Our app system dependencies should have gotten
	// some text services for us.
	if ( !g_IUITextServices )
	{
		return false;
	}

#if PANDX_DRAW
	PanDxInit();
#endif

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Init the subsystems we use
//-----------------------------------------------------------------------------
bool CUIEngineSource2::StartupSubsystems( IUISettings *pSettings, PlatWindow_t hWindow )
{

	g_pRenderDeviceMgr->AddDeviceEventListener( this );

	if ( !CUIEngine::StartupSubsystems( pSettings, hWindow ) )
		return false;

    if ( !steamAPIContext.SteamHTMLSurface() )
        return false;
    
    steamAPIContext.SteamHTMLSurface()->Init();

// 7ls	
//	if ( g_pApplication->IsInToolsMode() )
//	{
		g_pResourceSystemSingleThreaded->RegisterToolsResourceListener( this );
//	}

	m_pPanoramaStyleTypeManager = new CPanoramaTypeManager( RESOURCE_TYPE_PANORAMA_STYLE );
	m_pPanoramaLayoutTypeManager = new CPanoramaTypeManager( RESOURCE_TYPE_PANORAMA_LAYOUT );
	m_pPanoramaDynamicImagesTypeManager = new CPanoramaTypeManager( RESOURCE_TYPE_PANORAMA_DYNAMIC_IMAGES );
	m_pPanoramaScriptTypeManager = new CPanoramaTypeManager( RESOURCE_TYPE_PANORAMA_SCRIPT );

	g_pResourceSystem->InstallTypeManager< RESOURCE_TYPE_PANORAMA_STYLE >( m_pPanoramaStyleTypeManager );
	g_pResourceSystem->InstallTypeManager< RESOURCE_TYPE_PANORAMA_LAYOUT >( m_pPanoramaLayoutTypeManager );
	g_pResourceSystem->InstallTypeManager< RESOURCE_TYPE_PANORAMA_DYNAMIC_IMAGES >( m_pPanoramaDynamicImagesTypeManager );
	g_pResourceSystem->InstallTypeManager< RESOURCE_TYPE_PANORAMA_SCRIPT >( m_pPanoramaScriptTypeManager );

	
	// Load the panorama resource manifest when in tools mode, this is needed so that
	// there is actually a reference to the vpdi files to get them to auto reload.
	if ( g_pApplication->IsInToolsMode() )
	{
		m_hRequiredManifest = g_pResourceSystem->LoadResourceManifestFile( "panorama/panorama.vrman", RESOURCE_MANIFEST_LOAD_STREAMING_DATA, "Panorama Required Manifest" );
		if ( m_hRequiredManifest == RESOURCE_MANIFEST_HANDLE_INVALID )
			return false;
	
		g_pResourceSystem->ForceSynchronizationAndBlockUntilManifestLoaded( m_hRequiredManifest );
	
		// check all the required resources loaded
		CUtlVector< CUtlString > requiredList;
		g_pResourceSystem->GetResourcesNamesInManifest( m_hRequiredManifest, requiredList );
		for ( int i = 0; i < requiredList.Count(); i++ )
		{
			ResourceHandle_t hResourceHandle = g_pResourceSystem->FindExistingResourceByName( requiredList[i].Get(), DeduceResourceTypeFromResourceName( requiredList[i].Get() ) );
			if ( hResourceHandle == RESOURCE_HANDLE_INVALID || ResourceIsError( hResourceHandle ) || !ResourceIsLoaded( hResourceHandle ) )
			{
				// required resources failed to load
				return false;
			}
		}
	}

	if ( g_pIMEManager && !CommandLine()->HasParm( "-noime" ) )
	{
		if ( !g_pIMEManager->Setup( hWindow ) )
		{
			// expected IME setup failed
			return false;
		}
	}

	// success
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Create a new top level window
//-----------------------------------------------------------------------------
IUIWindow *CUIEngineSource2::CreateNewUILayerWindow( uint32 xPos, uint32 yPos, uint32 width, uint32 height, bool bFixedSurfaceSize, bool bEnforceWindowAspectRatio, bool bUseCustomMouseCursor, bool bAcceptKBandMouse, const char *pName, InputContextHandle_t hInputContext )
{
	CTopLevelWindowSource2 * pWindowImpl = new CTopLevelWindowSource2( this, pName, hInputContext );
	if ( !pWindowImpl->BInitializeSurface(xPos, yPos, width, height, bFixedSurfaceSize, bEnforceWindowAspectRatio, bUseCustomMouseCursor, bAcceptKBandMouse, true ) )
	{
		delete pWindowImpl;
		return NULL;
	}

	if( !pWindowImpl )
		return NULL;

	if ( !pWindowImpl->FinishInitialization() )
	{
		delete pWindowImpl;
		return NULL;
	}

	m_vecWindows.AddToTail( pWindowImpl );
	return pWindowImpl;
}

//-----------------------------------------------------------------------------
// Purpose: Create a new offscreen window, used for rendering individual panels to later be used as textures on models
//-----------------------------------------------------------------------------
IUIWindow *CUIEngineSource2::CreateNewOffscreenUIWindow( uint32 width, uint32 height, const char *pName, InputContextHandle_t hInputContext, bool bDrawToBackBuffer )
{
	// world UI settings
	bool bFixedSurfaceSize = false;
	bool bEnforceWindowAspectRatio = false;
	bool bUseCustomMouseCursor = false;
	bool bAcceptKBandMouse = false;
	uint32 xPos = 0;
	uint32 yPos = 0;

	CTopLevelWindowSource2 * pWindowImpl = new CTopLevelWindowSource2( this, pName, hInputContext );
	if ( !pWindowImpl->BInitializeSurface( xPos, yPos, width, height, bFixedSurfaceSize, bEnforceWindowAspectRatio, bUseCustomMouseCursor, bAcceptKBandMouse, bDrawToBackBuffer ) )
	{
		delete pWindowImpl;
		return NULL;
	}

	if ( !pWindowImpl )
		return NULL;

	// Offscreen UI doesn't want auto-mouse-up
	pWindowImpl->SetUseAutoMouseUpBehavior( false );

	if ( !pWindowImpl->FinishInitialization() )
	{
		delete pWindowImpl;
		return NULL;
	}

	m_vecWindows.AddToTail( pWindowImpl );
	return pWindowImpl;
}


//-----------------------------------------------------------------------------
// Purpose: Destroy a window (UI Layer or Offscreen)
//-----------------------------------------------------------------------------

bool CUIEngineSource2::DestroyWindow( IUIWindow *pWindow )
{
	int iFoundWindow = m_vecWindows.InvalidIndex();
	for ( int iWindow = 0; iWindow < m_vecWindows.Count(); iWindow++ )
	{
		if ( m_vecWindows[iWindow] == pWindow )
		{
			iFoundWindow = iWindow;
			break;
		}
	}

	if ( iFoundWindow == m_vecWindows.InvalidIndex() )
		return false;

	delete m_vecWindows[iFoundWindow];  // this automatically removes it from m_vecWindows
	return true;
}

void CUIEngineSource2::OnResolutionChange( float fRelativeScalefactor )
{
	m_pImageResourceManager->OnResolutionChange( fRelativeScalefactor );
}

void CUIEngineSource2::OnGPUMemLevelChanged()
{
	OnDeviceLost();
	OnDeviceRestored();
}

//-----------------------------------------------------------------------------
// Purpose: Run a frame, pumps input, paints, whatever.
//-----------------------------------------------------------------------------
void CUIEngineSource2::RunPlatformFrame()
{
	// Run Win32 input/message loop
	bool bAnyCursorVisible = false;
	FOR_EACH_VEC( m_vecWindows, i )
	{
		if ( m_vecWindows[i]->BHasVisibleHoverCursor() )
		{
			bAnyCursorVisible = true;
		}
			
		m_vecWindows[i]->RunPlatformFrame();
	}

#ifndef PANORAMA_USE_S1WRAPPER 
	if ( !bAnyCursorVisible )
	{
		FOR_EACH_VEC( m_vecWindows, i )
		{
			m_vecWindows[i]->SetMouseCursor( eMouseCursor_PassThrough );
		}
	}
#endif

	m_pImageResourceManager->RunFrame();

	CUIEngine::RunPlatformFrame();

	UpdateFileMonitoring();
	ServiceQueuedReloads();
}

//-----------------------------------------------------------------------------
// Purpose: Shutdown the UI engine including the surface and window
//-----------------------------------------------------------------------------
void CUIEngineSource2::Shutdown()
{
	if ( m_bInShutdown )
		return;
	m_bInShutdown = true;

    if ( steamAPIContext.SteamHTMLSurface() )
    {
        steamAPIContext.SteamHTMLSurface()->Shutdown();
    }

	if ( m_pImageResourceManager )
	{
		// Do shutdown tasks that need the engine.
		m_pImageResourceManager->ShutdownForEngine();
	}

	CUIEngine::Shutdown();

	if ( m_pImageResourceManager )
	{
		// Do shutdown tasks that have to happen after the engine shutdown.
		// The engine shutdown will clean up windows and that will do panel
		// work and those panels may reference the image resource manager
		// so we don't want to null the pointer until after engine shutdown.
		m_pImageResourceManager->Shutdown();
		SAFE_DELETE( m_pImageResourceManager );
	}

	m_MonitoredResources.PurgeAndDeleteElements();

	g_pRenderDeviceMgr->RemoveDeviceEventListener( this );

	if ( m_hRequiredManifest != RESOURCE_MANIFEST_HANDLE_INVALID )
	{
		g_pResourceSystem->DestroyResourceManifest( m_hRequiredManifest );
		m_hRequiredManifest = RESOURCE_MANIFEST_HANDLE_INVALID;
	}

	if ( m_pPanoramaScriptTypeManager )
	{
		g_pResourceSystem->RemoveResourceTypeManager( m_pPanoramaScriptTypeManager );
		delete m_pPanoramaScriptTypeManager;
		m_pPanoramaScriptTypeManager = NULL;
	}

	if ( m_pPanoramaDynamicImagesTypeManager )
	{
		g_pResourceSystem->RemoveResourceTypeManager( m_pPanoramaDynamicImagesTypeManager );
		delete m_pPanoramaDynamicImagesTypeManager;
		m_pPanoramaDynamicImagesTypeManager = NULL;
	}

	if ( m_pPanoramaStyleTypeManager )
	{
		g_pResourceSystem->RemoveResourceTypeManager( m_pPanoramaStyleTypeManager );
		delete m_pPanoramaStyleTypeManager;
		m_pPanoramaStyleTypeManager = NULL;
	}

	if ( m_pPanoramaLayoutTypeManager )
	{
		g_pResourceSystem->RemoveResourceTypeManager( m_pPanoramaLayoutTypeManager );
		delete m_pPanoramaLayoutTypeManager;
		m_pPanoramaLayoutTypeManager = NULL;
	}

// 7ls
//	if ( g_pApplication->IsInToolsMode() )
//	{
		g_pResourceSystemSingleThreaded->UnregisterToolsResourceListener( this );
//	}

	m_MonitoredFiles.PurgeAndDeleteElements();
	m_MonitoredDirectories.PurgeAndDeleteElements();

#if PANDX_DRAW 
	PanDxTerm();
#endif

	// Controlled by engine
	//SteamAPI_Shutdown();
	m_bInShutdown = false;
}


//-----------------------------------------------------------------------------
// Purpose: Shows a native message box.. usually for development
//-----------------------------------------------------------------------------
bool CUIEngineSource2::ShowNativeTopMostMessageBox( const char *pchMsg, const char *pchTitle, ENativeMessageBoxType_t eType )
{
	switch ( eType )
	{
	case panorama::IUIEngine::k_ENativeMessageOk:
		{
			if ( CommandLine()->HasParm( "-nojserrors" ) )	// This function is predominantly used to display errors when
				return true;								// parsing XML/CSS files, allow this flag to override (mapbuilders, etc.)

			return SDL_ShowSimpleMessageBox( SDL_MESSAGEBOX_INFORMATION, pchTitle, pchMsg, NULL ) == 0;
		}
		break;
	case panorama::IUIEngine::k_ENativeMessageYesNo:
		{
			const SDL_MessageBoxButtonData buttons[] = {
					/* .flags, .buttonid, .text */ 
					{ SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "No" },
					{ SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Yes" },
			};
			const SDL_MessageBoxData messageboxdata = {
				SDL_MESSAGEBOX_INFORMATION, /* .flags */
				NULL, /* .window */
				pchTitle, /* .title */
				pchMsg, /* .message */
				SDL_arraysize( buttons ), /* .numbuttons */
				buttons, /* .buttons */
				NULL /* .colorScheme */
			};
			
			int buttonid;
			if ( SDL_ShowMessageBox( &messageboxdata, &buttonid ) < 0 ) 
			{
				AssertMsg( false, "error displaying message box" );
				return false;
			}

			if ( buttonid != 1 )
				return false;
			else
				return true;
		}
		break;
	default:
		break;
	}
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Copies text to the system clipboard
//-----------------------------------------------------------------------------
void CUIEngineSource2::CopyToClipboardImpl( const char *pchTextUTF8 )
{
	SDL_SetClipboardText( pchTextUTF8 );
}


//-----------------------------------------------------------------------------
// Purpose: Gets clipboard text as UTF8
//-----------------------------------------------------------------------------
void CUIEngineSource2::GetClipboardTextImpl( CUtlString &strUTF8 ) const
{
	if ( SDL_HasClipboardText() )
	{
		char *clip = SDL_GetClipboardText();
		if (clip)
		{
			strUTF8 = clip;
			SDL_free(clip);
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get information about the users GPU
//-----------------------------------------------------------------------------
bool CUIEngineSource2::BGetGPUInformation( char *rgchGPUDesc, uint32 unGPUDescBytes, uint64 *pulDedicatedGPUMem, uint64 *pulDedicatedSystemMem, uint64 *pulSharedMem )
{
	if ( rgchGPUDesc && unGPUDescBytes > 0 )
		rgchGPUDesc[0] = 0;

	if ( pulDedicatedGPUMem )
		*pulDedicatedGPUMem = 0;

	if ( pulDedicatedSystemMem )
		*pulDedicatedSystemMem = 0;

	if ( pulSharedMem )
		*pulSharedMem = 0;

	// bugbug jmc - implement this for source 2... do we even need it though?
	return false;
}
	

//-----------------------------------------------------------------------------
// Purpose: return the locale we are currently on
//-----------------------------------------------------------------------------
ELanguage CUIEngineSource2::GetCurrentInputLocale() 
{ 
	return k_Lang_English;
}
	

//-----------------------------------------------------------------------------
// Purpose: Return true if input in the given language is supported
//-----------------------------------------------------------------------------	
bool CUIEngineSource2::BHaveInputLocale( ELanguage language ) 
{ 
	// Restrict input locales to those matching keyboard layouts?  Win32 impl in steam does this...

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: change to this locale
//-----------------------------------------------------------------------------
void CUIEngineSource2::SetInputLocale( ELanguage language )
{
	// Steam win32 impl sets input locale to match keyboard layout... as well... do that?

}


#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: validate mem
//-----------------------------------------------------------------------------
void CUIEngineSource2::Validate( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE();

	CUIEngine::Validate( validator, pchName );
	CTopLevelWindowSDL::ValidateStatics( validator, "CTopLevelWindowSDL::ValidateStatics" );
}
#endif


void CUIEngineSource2::NotifyResourceStatusChange( ResourceId_t nResourceId, ResourceHandle_t hResource, ResourceType_t nResourceType, ResourceLoadType_t nLoadType )
{
	if ( nLoadType != RESOURCE_LOAD_RELOAD )
		return;

	int nMapIndex = m_MonitoredResources.Find( hResource );
	if ( nMapIndex == m_MonitoredResources.InvalidIndex() )
	{
		// not monitored, ignore
		return;
	}

	ResourceMonitor_t *pResourceMonitor = m_MonitoredResources[nMapIndex];

	CRC32_t nCRC32 = 0;
	if ( nResourceType == RESOURCE_TYPE_PANORAMA_STYLE )
	{
		HPanoramaStyle hPanoramaStyle = HPanoramaStyle::FromUntypedHandle( hResource );
		if ( hPanoramaStyle.IsValid() && !hPanoramaStyle.IsError() )
		{
			CPanoramaStyle *pPanoramaStyle = (CPanoramaStyle*)hPanoramaStyle.GetData();
			nCRC32 = pPanoramaStyle->m_nCRC32;
		}
	}
	else if ( nResourceType == RESOURCE_TYPE_PANORAMA_LAYOUT )
	{
		HPanoramaLayout hPanoramaLayout = HPanoramaLayout::FromUntypedHandle( hResource );
		if ( hPanoramaLayout.IsValid() && !hPanoramaLayout.IsError() )
		{
			CPanoramaLayout *pPanoramaLayout = (CPanoramaLayout*)hPanoramaLayout.GetData();
			nCRC32 = pPanoramaLayout->m_nCRC32;
		}
	}
	else if ( nResourceType == RESOURCE_TYPE_PANORAMA_SCRIPT )
	{
		HPanoramaScript hPanoramaScript = HPanoramaScript::FromUntypedHandle( hResource );
		if ( hPanoramaScript.IsValid() && !hPanoramaScript.IsError() )
		{
			CPanoramaScript *pPanoramaScript = (CPanoramaScript*)hPanoramaScript.GetData();
			nCRC32 = pPanoramaScript->m_nCRC32;
		}
	}
	else if( nResourceType == RESOURCE_TYPE_VECTOR_GRAPHIC )
	{
		HVectorGraphic hSvg = HVectorGraphic::FromUntypedHandle( hResource );
		if( hSvg.IsValid() && !hSvg.IsError() )
		{
			CVectorGraphic *pSvg = (CVectorGraphic*)hSvg.GetData();
			nCRC32 = pSvg->m_nCRC32;
		}
	}
	else if ( nResourceType == RESOURCE_TYPE_TEXTURE )
	{
		if ( pResourceMonitor->m_pTextureAsset )
		{
			// use the asset system to get a CRC
			CUtlString absoluteFilename = pResourceMonitor->m_pTextureAsset->GetAbsolutePath_Transient( ASSET_LOCATION_GAME );
			g_pAssetSystem->GetAbsoluteFileCRC( absoluteFilename.Get(), &nCRC32 );
		}
	}

	Log_Detailed( LOG_PANORAMA, "ResourceStatusChange: (CRC: Expected: 0x%8.8x, Got: 0x%8.8x), '%s'\n", pResourceMonitor->m_nCRC32, nCRC32, pResourceMonitor->m_ContentFilename.Get() );

	if ( nCRC32 && pResourceMonitor->m_nCRC32 == nCRC32 )
	{
		// valid CRC's match
		// no change, an unlrelated dependent compile, ignore
		Log_Detailed( LOG_PANORAMA, "ResourceStatusChange:\tCRC matches, Ignoring '%s'\n", pResourceMonitor->m_ContentFilename.Get() );
		return;
	}

	Log_Detailed( LOG_PANORAMA, LOG_COLOR_YELLOW, "ResourceStatusChange:\tReloadChangedFile '%s'\n", pResourceMonitor->m_ContentFilename.Get() );

	if ( -1 == m_QueuedResourceReloads.Find( pResourceMonitor ) )
	{
		m_QueuedResourceReloads.AddToTail( pResourceMonitor );
	}
}

void CUIEngineSource2::ServiceQueuedReloads()
{
	// service queued reloads at a known time so we don't trigger reentrancy in the resourcesystem
	for ( int iReload = 0; iReload < m_QueuedResourceReloads.Count(); ++iReload )
	{
		ResourceMonitor_t *pResourceMonitor = m_QueuedResourceReloads[ iReload ];
		ResourceType_t nResourceType = g_pResourceSystem->GetResourceType( pResourceMonitor->m_StrongHandleVoid );

		if ( nResourceType == RESOURCE_TYPE_PANORAMA_STYLE || nResourceType == RESOURCE_TYPE_PANORAMA_LAYOUT || nResourceType == RESOURCE_TYPE_PANORAMA_SCRIPT )
		{
			UILayoutManagerInternal()->ReloadChangedFile( pResourceMonitor->m_ContentFilename.Get() );
		}
		else if( nResourceType == RESOURCE_TYPE_VECTOR_GRAPHIC )
		{
			m_pImageResourceManager->ReloadChangedFile( pResourceMonitor->m_ContentFilename.Get() );
		}
//7ls
// 		else if ( nResourceType == RESOURCE_TYPE_TEXTURE )
// 		{
// 			ReloadImageResourceFile( pResourceMonitor->m_ContentFilename.Get() );
// 		}
	}

	m_QueuedResourceReloads.RemoveAll();
}


void CUIEngineSource2::OnFileCacheRemoved( CPanoramaSymbol fileSymbol )
{
	// this is kinda slow but only happens in tools mode, when addon search paths are changed
	FOR_EACH_MAP_FAST( m_MonitoredResources, i )
	{
		ResourceMonitor_t *pMonitor = m_MonitoredResources[ i ];
		if ( pMonitor->m_ContentFilename.IsEqual_CaseSensitive( fileSymbol.String() ) )
		{
			pMonitor->m_StrongHandleVoid.Shutdown();
			pMonitor->m_ContentFilename = "";
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Stop monitoring a resource for changes
//-----------------------------------------------------------------------------
void CUIEngineSource2::StopMonitoringResourceForChanges( ResourceHandle_t hResource )
{
	int nMapIndex = m_MonitoredResources.Find( hResource );
	if ( nMapIndex == m_MonitoredResources.InvalidIndex() )
		return;

	ResourceMonitor_t *pMonitor = m_MonitoredResources[nMapIndex];
	
	pMonitor->m_StrongHandleVoid.Shutdown();

	m_MonitoredResources.RemoveAt( nMapIndex );
}


//-----------------------------------------------------------------------------
// Purpose: Start monitoring a resource for changes
//-----------------------------------------------------------------------------
bool CUIEngineSource2::MonitorResourceForChanges( ResourceHandle_t hResource )
{
#ifndef PANORAMA_USE_S1WRAPPER
	if ( !g_pApplication->IsInToolsMode() )
		return false;
#endif
	
	CRC32_t nCRC32 = 0;
	ResourceType_t nResourceType = g_pResourceSystem->GetResourceType( hResource );
	if ( nResourceType == RESOURCE_TYPE_PANORAMA_STYLE )
	{
		HPanoramaStyle hPanoramaStyle = HPanoramaStyle::FromUntypedHandle( hResource );
		if ( hPanoramaStyle.IsValid() && !hPanoramaStyle.IsError() )
		{
			CPanoramaStyle *pPanoramaStyle = (CPanoramaStyle*)hPanoramaStyle.GetData();
			nCRC32 = pPanoramaStyle->m_nCRC32;
		}
	}
	else if ( nResourceType == RESOURCE_TYPE_PANORAMA_LAYOUT )
	{
		HPanoramaLayout hPanoramaLayout = HPanoramaLayout::FromUntypedHandle( hResource );
		if ( hPanoramaLayout.IsValid() && !hPanoramaLayout.IsError() )
		{
			CPanoramaLayout *pPanoramaLayout = (CPanoramaLayout*)hPanoramaLayout.GetData();
			nCRC32 = pPanoramaLayout->m_nCRC32;
		}
	}
	else if ( nResourceType == RESOURCE_TYPE_PANORAMA_SCRIPT )
	{
		HPanoramaScript hPanoramaScript = HPanoramaScript::FromUntypedHandle( hResource );
		if ( hPanoramaScript.IsValid() && !hPanoramaScript.IsError() )
		{
			CPanoramaScript *pPanoramaScript = (CPanoramaScript*)hPanoramaScript.GetData();
			nCRC32 = pPanoramaScript->m_nCRC32;
		}
	}
	else if( nResourceType == RESOURCE_TYPE_VECTOR_GRAPHIC )
	{
		HVectorGraphic hSvg = HVectorGraphic::FromUntypedHandle( hResource );
		if( hSvg.IsValid() && !hSvg.IsError() )
		{
			CVectorGraphic *pSvg = (CVectorGraphic*)hSvg.GetData();
			nCRC32 = pSvg->m_nCRC32;
		}
	}
	else if ( nResourceType == RESOURCE_TYPE_TEXTURE )
	{
		// 7ls
		return false;
	}
	else
	{
		// unknown, ignore
		return false;
	}

	ResourceMonitor_t *pResourceMonitor = NULL;

	int nMapIndex = m_MonitoredResources.Find( hResource );
	if ( nMapIndex == m_MonitoredResources.InvalidIndex() )
	{
		// not found, create
		pResourceMonitor = new ResourceMonitor_t();
		nMapIndex = m_MonitoredResources.Insert( hResource, pResourceMonitor );
	}

	pResourceMonitor = m_MonitoredResources[nMapIndex];

	if ( pResourceMonitor->m_ContentFilename.IsEmpty() )
	{
		char resourceName[MAX_PATH];
		resourceName[0] = '\0';
		g_pResourceSystem->GetResourceName( hResource, resourceName, sizeof( resourceName ) );
	
		// track the expected content filename (i.e. what panorama wants it to be)
		CUtlString contentFilename;
		m_pFileSystem->RestoreContentFilename( resourceName, contentFilename );

		// this is a convoluted fixup, but it obeys the fixup contracts expected elsewhere
		CFileResource fileResource( CFmtStr( "s2r://%s", contentFilename.Get() ).Get() );
		pResourceMonitor->m_ContentFilename = fileResource.GetReferencePath();

		// track the resource to ensure its lifetime for monitoring
		pResourceMonitor->m_StrongHandleVoid = hResource;
	}

	if ( nResourceType == RESOURCE_TYPE_TEXTURE )
	{
		if ( !pResourceMonitor->m_pTextureAsset )
		{
			// find our asset
			char resourceName[MAX_PATH];
			resourceName[0] = '\0';
			g_pResourceSystem->GetResourceName( hResource, resourceName, sizeof( resourceName ) );

			if ( !V_striEndsWith( resourceName, "_c" ) )
			{
				V_strncat( resourceName, "_c", sizeof ( resourceName ) );
			}
			pResourceMonitor->m_pTextureAsset = g_pAssetSystem->FindAssetByFilename( resourceName );
		}
		
		if ( pResourceMonitor->m_pTextureAsset )
		{
			// use the asset system to get a CRC
			CUtlString absoluteFilename = pResourceMonitor->m_pTextureAsset->GetAbsolutePath_Transient( ASSET_LOCATION_GAME );
			g_pAssetSystem->GetAbsoluteFileCRC( absoluteFilename.Get(), &nCRC32 );
		}	
	}

	// track the expected/changing crc for parity rejection
	pResourceMonitor->m_nCRC32 = nCRC32;

	return true;
}

void CUIEngineSource2::ReloadImageResourceFile( const char *pResourceFile )
{
	FOR_EACH_VEC( m_vecWindows, iWindow )
	{
		m_vecWindows[iWindow]->ReloadChangedFile( CFmtStr( "s2r://%s", pResourceFile ).Get() );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CUIEngineSource2::MonitorFileForChanges( const char *pFilename, FileChangeCallback_t fileChangeCallback )
{
	// S1 not yet allowing live update of misc files

#ifdef PANORAMA_USE_S1WRAPPER
	return false;
#endif

	if ( !g_pApplication->IsInToolsMode() || !fileChangeCallback )
		return false;

	CUtlString fullPathString;
	if ( V_IsAbsolutePath( pFilename ) )
	{
		fullPathString = pFilename;
	}
	else
	{
		char fullPath[MAX_PATH];
		if ( !g_pFullFileSystem->RelativePathToFullPath( pFilename, "GAME", fullPath, sizeof( fullPath ) ) )
		{
			// can't resolve
			return false;
		}
		fullPathString = fullPath;
	}
	fullPathString.FixSlashes( CORRECT_PATH_SEPARATOR );

	int iDictIndex = m_MonitoredFiles.Find( fullPathString.Get() );
	if ( iDictIndex != m_MonitoredFiles.InvalidIndex() )
	{
		// already monitored
		return true;
	}

	FileWatch_t *pFileWatch = new FileWatch_t();
	pFileWatch->m_FileChangeCallback = fileChangeCallback;
	m_MonitoredFiles.Insert( fullPathString.Get(), pFileWatch );

	CUtlString rootPathString = fullPathString.StripFilename();
	for ( int i = 0; i < m_MonitoredDirectories.Count(); i++ )
	{
		if ( !V_stricmp( m_MonitoredDirectories[i]->m_FullPathToRoot.Get(), rootPathString.Get() ) )
		{
			// already monitored
			return true;
		}
	}

	DirectoryWatch_t *pDirectoryWatch = new	DirectoryWatch_t();
#ifndef PANORAMA_USE_S1WRAPPER
	pDirectoryWatch->m_DirectoryWatcher.SetDirToWatch( rootPathString.Get(), k_EFioStandardMetadata, false );
#else
	pDirectoryWatch->m_DirectoryWatcher.SetDirToWatch( rootPathString.Get() );
#endif
	pDirectoryWatch->m_FullPathToRoot = rootPathString;
	m_MonitoredDirectories.AddToTail( pDirectoryWatch );

	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CUIEngineSource2::UpdateFileMonitoring()
{
#ifdef PANORAMA_USE_S1WRAPPER
	return;
#endif

	if ( !g_pApplication->IsInToolsMode() )
		return;

	struct ChangedFile_t
	{
		CUtlString m_FullPathString;
		FileChangeCallback_t m_FileChangeCallback;
	};

	CUtlVector< ChangedFile_t > changedFiles;
	CUtlString filenameString;
	for ( int i = 0; i < m_MonitoredDirectories.Count(); i++ )
	{
		while ( m_MonitoredDirectories[i]->m_DirectoryWatcher.GetChangedFile( &filenameString ) )
		{
			CUtlString fullPathString = CUtlString::PathJoin( m_MonitoredDirectories[i]->m_FullPathToRoot.Get(), filenameString );
			fullPathString.FixSlashes( CORRECT_PATH_SEPARATOR );

			// consider only monitored files within monitored directory
			int nDictIndex = m_MonitoredFiles.Find( fullPathString.Get() );
			if ( nDictIndex != m_MonitoredFiles.InvalidIndex() )
			{
				int nChangedIndex = changedFiles.AddToTail();
				changedFiles[nChangedIndex].m_FullPathString = fullPathString;
				changedFiles[nChangedIndex].m_FileChangeCallback = m_MonitoredFiles[nDictIndex]->m_FileChangeCallback;
			}
		}
	}

	// invoke callbacks
	for ( int i = 0; i < changedFiles.Count(); i++ )
	{
		changedFiles[i].m_FileChangeCallback( changedFiles[i].m_FullPathString.Get() );
	}
}

bool CUIEngineSource2::GetOriginalImageDimensions( const char *pResourceFile, uint32 *pOriginalWidth, uint32 *pOriginalHeight )
{
	// the image compile info will be the same, regardless of who compiled it
	if ( m_pPanoramaStyleTypeManager && m_pPanoramaStyleTypeManager->GetOriginalImageDimensions( pResourceFile, pOriginalWidth, pOriginalHeight ) )
		return true;

	if ( m_pPanoramaLayoutTypeManager && m_pPanoramaLayoutTypeManager->GetOriginalImageDimensions( pResourceFile, pOriginalWidth, pOriginalHeight ) )
		return true;

	// not found
	return false;
}

IUISoundSystem *CUIEngineSource2::CreateSoundSystem()
{
	return new CUISoundSystemSource2();
}

void CUIEngineSource2::OnDeviceLost()
{
	for( int i = 0; i < m_vecWindows.Count(); i++ )
	{
		m_vecWindows[i]->OnDeviceLost();
	}
}

void CUIEngineSource2::OnDeviceRestored()
{
	for( int i = 0; i < m_vecWindows.Count(); i++ )
	{
		m_vecWindows[i]->OnDeviceRestored();
	}
}

void CUIEngineSource2::OnDeviceCreated()
{
	// Should never happen.
	// Assert( 0 );
}

void CUIEngineSource2::OnModeChanged( const RenderDeviceInfo_t &mode )
{
	// SetScreenSize( mode.m_DisplayMode.m_nWidth, mode.m_DisplayMode.m_nHeight );
}


//-----------------------------------------------------------------------------
// return all of our windows so the debugger can view them
//-----------------------------------------------------------------------------
void CUIEngineSource2::GetWindowsForDebugger( CUtlVector<IUIWindow *> &vecWindows )
{
	FOR_EACH_VEC( m_vecWindows, i )
	{
		vecWindows.AddToTail( m_vecWindows[i] );
	}
}

void CUIEngineSource2::ReloadChangedFile( const char *pchFile )
{
	m_pImageResourceManager->ReloadChangedFile( pchFile );
}

} // namespace panorama
