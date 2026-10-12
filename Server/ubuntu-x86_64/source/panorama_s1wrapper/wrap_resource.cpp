//================ Copyright (c) Valve Corporation. All Rights Reserved. ===========================
//
//
//
//==================================================================================================

#include "s1wrapper.h"
#include "tier0/icommandline.h"
#include "filesystem.h"
#include "wrap_texture.h"

#include "materialsystem/imaterialsystem.h"
#include "materialsystem/itexture.h"

#include "zip_utils.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


void ResourceData_t::OnFinalRelease() const
{
	g_pResourceSystem->DestroyExistingResource( this );
}


//--------------------------------------------------------------------------------------------------
// Helper fns
//--------------------------------------------------------------------------------------------------

// Is this resource name valid?
bool IsResourceNameValid( const char *pResourceName )
{
	if ( !pResourceName || !pResourceName[ 0 ] )
		return true;

	if ( V_IsAbsolutePath( pResourceName ) )
		return false;

	// No uppercase, no incorrect slashes
	for ( const char *c = pResourceName; *c != 0; ++c )
	{
		if ( V_isupper( *c ) || *c == '\\' )
			return false;
	}

	return true;
}



// Sets the file extension of a filename to be appropriate for a given resource type
void SetResourceFileNameExtension( const char *pFileName, ResourceType_t nType, char *pOutFileName, int nBufLen )
{
	// Create a resource name with the appropriate extension
	char pExt[ RESOURCE_EXT_MIN_BUF_SIZE ];
	ResourceTypeToExt( pExt, nType );
	V_FixupPathName( pOutFileName, nBufLen, pFileName );
	V_FixSlashes( pOutFileName, '/' );
	V_SetExtension( pOutFileName, pExt, nBufLen );
}


bool RestoreContentFileExtension(char *pOutFileName)
{
	// CSS/XML extensions are tricky because S2 wants them to be something else and panorama does not
	char *pExtension = (char *)V_GetFileExtension(pOutFileName);
	const char *pPreferredExtension = NULL;
	if (pExtension)
	{
		if (!V_stricmp(pExtension, "vcss") || !V_stricmp(pExtension, "vcss_c"))
		{
			// S2 mandates 'v' versions except ui authors don't want that, so put back to the expected content
			pPreferredExtension = "css";
		}
		else if (!V_stricmp(pExtension, "vxml") || !V_stricmp(pExtension, "vxml_c"))
		{
			// S2 mandates 'v' versions except ui authors don't want that, so put back to the expected content
			pPreferredExtension = "xml";
		}
		else if (!V_stricmp(pExtension, "vjs") || !V_stricmp(pExtension, "vjs_c"))
		{
			// S2 mandates 'v' versions except ui authors don't want that, so put back to the expected content
			pPreferredExtension = "js";
		}
		else if( !V_stricmp( pExtension, "vsvg" ) || !V_stricmp( pExtension, "vsvg_c" ) )
		{
			pPreferredExtension = "svg";
		}
		else if (!V_stricmp(pExtension, "vtex"))
		{
 			char* pSuffix = pExtension - 5;
 			if ((pSuffix > pOutFileName) && (*pSuffix == '_') &&
				 ( !V_strncmp( pSuffix, "_png.", 5 ) || !V_strncmp( pSuffix, "_vtf.", 5 ) || !V_strncmp( pSuffix, "_iic.", 5 ) 
				 || !V_strncmp( pSuffix, "_tga.", 5 ) || !V_strncmp( pSuffix, "_jpg.", 5 ) || !V_strncmp( pSuffix, "_dds.", 5 )) )
			{
 				*pSuffix = '.';
				pExtension[ -1 ] = 0;
			}

			pPreferredExtension = 0;
			return true;
		}

		if (pPreferredExtension)
		{
			V_strcpy(pExtension, pPreferredExtension);
			return true;
		}
	}

	// no change
	return false;
}

// Fixes up a resource name. Assumes the resource name is reasonably specified
// as a relative path from content/. 
bool FixupResourceName( const char *pResourceName, ResourceType_t nType, char *pOutResourceName, int nBufLen )
{
	if ( !pResourceName || !pResourceName[ 0 ] )
	{
		*pOutResourceName = 0;
		return true;
	}

	if ( V_IsAbsolutePath( pResourceName ) )
	{
		Msg( "FixupResourceName: Illegal full path passed in (\"%s\")!\n", pResourceName );
		*pOutResourceName = 0;
		return false;
	}

	char pActualResourceNameBuf[ MAX_FILEPATH ];
	char pExt[ RESOURCE_EXT_MIN_BUF_SIZE ];
	ResourceTypeToExt( pExt, nType );
	const char *pActualExt = V_GetFileExtension( pResourceName );
	if ( !pActualExt )
	{
		SetResourceFileNameExtension( pResourceName, nType, pActualResourceNameBuf, sizeof( pActualResourceNameBuf ) );
		pResourceName = pActualResourceNameBuf;
	}
	else if ( V_stricmp( pExt, pActualExt ) )
	{
		Msg( "ERROR: Resource name \"%s\" has the incorrect extension \"%s\" for the specified resource type (expected \"%s\")!\n",
				 pResourceName, V_GetFileExtension( pResourceName ), pExt );
		*pOutResourceName = 0;
		return false;
	}

	// Pretty simple: just fixup the path name and lowercase everything.
	V_FixupPathName( pOutResourceName, nBufLen, pResourceName );
	V_FixSlashes( pOutResourceName, '/' );
	return true;
}

bool FixupResourceName( const char *pResourceName, ResourceType_t nType, CUtlString *pOutName )
{
	char pFixedResourceName[ MAX_FILEPATH ];
	bool bOk = FixupResourceName( pResourceName, nType, pFixedResourceName, sizeof( pFixedResourceName ) );
	*pOutName = pFixedResourceName;
	return bOk;
}

bool FixupResourceName( const char *pResourceName, char *pOutResourceName, int nBufLen )
{
	if ( !pResourceName || !pResourceName[ 0 ] )
	{
		*pOutResourceName = 0;
		return true;
	}

	if ( V_IsAbsolutePath( pResourceName ) )
	{
		Msg( "FixupResourceName: Illegal full path passed in (\"%s\")!\n", pResourceName );
		*pOutResourceName = 0;
		return false;
	}

	const char *pActualExt = V_GetFileExtension( pResourceName );
	if ( !pActualExt )
	{
		Msg( "FixupResourceName: Illegal path, missing extension passed in (\"%s\")!\n", pResourceName );
		*pOutResourceName = 0;
		return false;
	}

	// Pretty simple: just fixup the path name, lowercase everything, and force forward slashes.
	V_FixupPathName( pOutResourceName, nBufLen, pResourceName );
	V_FixSlashes( pOutResourceName, '/' );
	return true;
}

//--------------------------------------------------------------------------------------------------
// Resource System (g_pResourceSystem) implementation
//--------------------------------------------------------------------------------------------------

class CResourceManifest;

class CResourceSystem : IResourceSystem
{
public:

	virtual void				Init();
	virtual void				Shutdown();

	// Frame update
	virtual void				Update();

	virtual void				GetActualFileName( ResourceHandle_t hResource, char *pResourceNameOut, int nBufLen );
	virtual void				GetResourceName( ResourceHandle_t hResource, char *pResourceName, int nBufLen );
	virtual ResourceType_t		GetResourceType( ResourceHandle_t hResource );


	// these two (find and create) must be called as a pair to avoid memory leak
	virtual ResourceHandle_t	FindExistingResourceByName( const char *pResourceName, ResourceType_t nResourceType );
	virtual HResourceManifest	CreateResourceManifest( int nCount, const char **ppResourceFiles, ResourceManifestLoadBehavior_t nType,
														const char *pDebugName,
														ResourceManifestLoadPriority_t nPriority = RESOURCE_MANIFEST_LOAD_PRIORITY_DEFAULT );

	virtual void				SetManifestCompletionCallback( HResourceManifest hManifest, ResourceManifestLoadCompletionCallback_t callback, void *pContext );
	virtual void				ForceSynchronizationAndBlockUntilManifestLoaded( HResourceManifest hManifest );

	virtual HResourceManifest	LoadResourceManifestFile( const char *pManifestFileName, ResourceManifestLoadBehavior_t nType,
														  const char *pDebugName,
														  ResourceManifestLoadPriority_t nPriority = RESOURCE_MANIFEST_LOAD_PRIORITY_DEFAULT );
	virtual void				DestroyResourceManifest( HResourceManifest hManifest );

	virtual ResourceHandle_t	BlockingLoadResourceByName( const char *pResourceName, ResourceType_t nResourceType, const char *pDebugName );

	virtual void				AddExistingResource( const char* pName, ResourceHandle_t );
	virtual void				DestroyExistingResource( ResourceHandle_t );

	void 						ReloadEditedResources( bool bForceReload );

	virtual char*				LoadFromPanZip( const char* fname );

	virtual uint16				GetTextureUniqueId();
	virtual void				MarkTextureUniqueIdUnused( uint16 nUniqueId );
	

	// stubbed out
	virtual void UpdateSimple() {};
	virtual void GetResourcesNamesInManifest( HResourceManifest hManifest, CUtlVector< CUtlString > &list ) const  {};
	virtual void RemoveResourceTypeManager( IResourceTypeManager *pTypeManager )  {};

	// Destroy resources marked for delete at the end of the render frame
	void DeleteRequestedResources();

	// Preload all layout / style / script files from the panorama folder
	void PreloadResources(bool bFromZip);

	// Mark all resource manifest items as loaded and removed resources from the resource system that failed to load,
	// For the texture case, Panorama can then dispatch the "ImageFailedLoad" event. S1Wrapper_Texture_t will fallback to the "error texture" 
	// if it fails to load (file does not exist or failed to convert png, ...)
	void OnManifestLoaded( CResourceManifest *pManifest );

	// data

	CUtlSymbolTable m_resourceNames;	// Holds strings & their symbol(short), Call Find() to get symbol for string, Call String() to get string for symbol

	CUtlMap< CUtlSymbol, ResourceHandle_t, int, CDefLess< CUtlSymbol > > m_mapHandlesByNameSymbol;		// Lookup res handle from Name's symbol
	CUtlMap< ResourceHandle_t, CUtlSymbol, int, CDefLess< ResourceHandle_t > > m_mapNameSymbolsByHandles;	// Lookup Name symbol from Handle

	CUtlVector< CResourceManifest * > m_activeResourceManifests;

	CTSQueue< ResourceHandle_t > m_DeletionRequests;	// Threadsafe

	CThreadFastMutex		m_mutex;

	// Texture unique identifier used to create anonymous resources (cf CRenderDevice::FindOrCreateTexture)
	// Need to recycle identifiers as the resource names are kept in a CUtlSymbolTable (m_resourceNames)
	// ( which is internally using a CUtlRBTree with a 16 bits index type)
	uint16 m_nNextTextureUniqueId;
	CUtlQueue<uint16> m_TextureUniqueIdFreeList;


	struct CfgFile_t
	{
		char* m_pName;
		char* m_pContent;
	};
	
	CUtlVector<CfgFile_t>		m_CfgFiles;
};

class CResourceManifest
{
public:

	void SetCompletionCallback( ResourceManifestLoadCompletionCallback_t callback, void *pContext );
	void InvokeCompletionCallback();

	bool IsLoaded() const;
	void MarkLoaded();

public:

	struct Item_t
	{
		char *m_pName;
		CResourceManifest *m_pLoadedInManifest;		// If not null, the item is loaded by another resource manifest
	};

	Item_t*			m_pItems;
	int				m_nCount;
	CInterlockedInt m_nPendingResourceRequests;

	ResourceManifestLoadCompletionCallback_t m_completionCallback;
	void *m_pContext;

	bool m_bLoaded;
};

class CPanoramaResource
{
public:
	CPanoramaResource()
	{
		m_nCRC32 = 0;
		m_pData = NULL;
		m_nDataSize = 0;
		m_hAsyncControl = nullptr;
	}

	~CPanoramaResource()
	{
		if ( g_pFullFileSystem && m_hAsyncControl )
		{
			g_pFullFileSystem->AsyncAbort( m_hAsyncControl );
			g_pFullFileSystem->AsyncRelease( m_hAsyncControl );
		}
	}

	// Do not change layout of m_nCRC32, m_pData & m_nDataSize
	// as it needs to match CPanoramaStyle, CPanoramaLayout, 
	// CPanoramaDynamicImages, CPanoramaScript, CVectorGraphic from uienginesource2.cpp
	CRC32_t		m_nCRC32;
	const char *m_pData;
	unsigned int m_nDataSize;

	// Async file read data
	FSAsyncControl_t m_hAsyncControl;
};

CResourceSystem g_ResourceSystem;
IResourceSystem* g_pResourceSystem = (IResourceSystem*)&g_ResourceSystem;



class CResourceSystemSingleThreaded : IResourceSystemSingleThreaded
{
public:

	CResourceSystemSingleThreaded()
	{
		m_pListener = nullptr;
	}

	virtual void RegisterToolsResourceListener( IToolsResourceListener* pListener )
	{
		m_pListener = pListener;
	}
	virtual void UnregisterToolsResourceListener( IToolsResourceListener* pListener )
	{
		m_pListener = nullptr;
	}

	void NotifyResourceStatusChange( ResourceId_t nResourceId, ResourceHandle_t hResource, ResourceType_t nResourceType, ResourceLoadType_t nLoadType )
	{
		m_pListener->NotifyResourceStatusChange( nResourceId, hResource, nResourceType, nLoadType );
	}
	
	IToolsResourceListener* m_pListener;
};

CResourceSystemSingleThreaded g_ResourceSystemSingleThreaded;
IResourceSystemSingleThreaded* g_pResourceSystemSingleThreaded = (IResourceSystemSingleThreaded*)&g_ResourceSystemSingleThreaded;

static bool EndFrameCleanup()
{
	g_ResourceSystem.DeleteRequestedResources();
	return false;
}


//--------------------------------------------------------------------------------------------------
//
//	CResourceSystem Methods
//
//--------------------------------------------------------------------------------------------------

void CResourceSystem::Init()
{
	g_pMaterialSystem->AddEndFramePriorToNextContextFunc( &EndFrameCleanup );

	S1Wrapper_Texture_t::StaticInit();

#if defined( OSX ) && defined( __aarch64__ ) && defined( NO_STEAM )
	const char *overlay = CommandLine()->ParmValue( "-panorama_native_overlay", "" );
	if ( overlay[0] )
		g_pFullFileSystem->AddSearchPath( overlay, "PANORAMA_NATIVE_OVERLAY" );
#endif

	// Preload all layout / style / script files from the panorama folder

//#if DEVELOPMENT_ONLY
	// In development check for packed and signed panorama zip file,
	// if that file doesn't exist, then load from scattered files on local filesystem
	if ( !g_pFullFileSystem->FileExists( PANORAMA_ZIPFILE_NAME, NULL ) )
	{
		PreloadResources( false );
	}
	else
//#endif
	{
		PreloadResources( true );
	}
}

void CResourceSystem::Shutdown()
{
	S1Wrapper_Texture_t::StaticShutdown();
	
	g_pMaterialSystem->RemoveEndFramePriorToNextContextFunc( &EndFrameCleanup );

	DeleteRequestedResources();

	for ( int i = 0; i < m_CfgFiles.Count(); i++)
	{
		free( m_CfgFiles[ i ].m_pName );
		free( m_CfgFiles[ i ].m_pContent );
	}

}

// MUST be called on the main thread (following source 2 implementation)
void CResourceSystem::Update()
{
	VPROF_BUDGET( "CResourceSystem::Update", VPROF_BUDGETGROUP_TENFOOT );

	AUTO_LOCK( m_mutex );

	Assert( ThreadInMainThread() );

	CUtlVector< CResourceManifest* > loadedManifest;
	FOR_EACH_VEC( m_activeResourceManifests, nResourceManifest )
	{
		CResourceManifest *pResourceManifest = m_activeResourceManifests[nResourceManifest];

		if ( pResourceManifest->m_nPendingResourceRequests.AssignIf( 0, -1 ) )
		{
			// Finished loading all resources

			// Check manifest dependencies
			bool bManifestLoaded = true;
			for ( int nItem = 0; nItem < pResourceManifest->m_nCount; ++nItem )
			{
				CResourceManifest::Item_t *pItem = &pResourceManifest->m_pItems[nItem];
				if ( pItem->m_pLoadedInManifest )
				{
					Assert( m_activeResourceManifests.Find( pItem->m_pLoadedInManifest ) != -1 );
					if ( pItem->m_pLoadedInManifest->IsLoaded() )
					{
						pItem->m_pLoadedInManifest = nullptr;
					}
					else
					{
						bManifestLoaded = false;
					}
				}
			}
			
			if ( bManifestLoaded )
			{
				// Mark all manifest items as load and remove resources from the resource system that failed to load.
				// (S1Wrapper_Texture_t will fallback to the "error texture" if it fails to load (file does not exist or failed to convert png, ...))
				OnManifestLoaded( pResourceManifest );

				pResourceManifest->MarkLoaded();

				// Can't invoke the manifest completion callback at this point
				// as the completion callback could destroy the manifest and 
				// remove it from m_activeResourceManifests.
				loadedManifest.AddToTail( pResourceManifest );
			}
			else
			{
				// Force another check next frame
				pResourceManifest->m_nPendingResourceRequests = 0;
			}
		}
	}

	FOR_EACH_VEC( loadedManifest, nResourceManifest )
	{
		// Call completion callback if necessary
		loadedManifest[nResourceManifest]->InvokeCompletionCallback();
	}
}

void  CResourceSystem::OnManifestLoaded( CResourceManifest *pManifest )
{
	VPROF_BUDGET( "CResourceSystem::OnManifestLoaded", VPROF_BUDGETGROUP_TENFOOT );

	for ( int nItem = 0; nItem < pManifest->m_nCount; ++nItem )
	{
		char *pResourceName = pManifest->m_pItems[ nItem ].m_pName;
		CUtlSymbol nameSymbol = m_resourceNames.Find( pResourceName );
		Assert( nameSymbol.IsValid() ); // Called after a manifest has been loaded 
										// therefore all names should be in the symbol table

		int nIndex = m_mapHandlesByNameSymbol.Find( nameSymbol );

		if ( nIndex != m_mapHandlesByNameSymbol.InvalidIndex() )
		{
			ResourceHandle_t hResource = m_mapHandlesByNameSymbol[nIndex];

			if ( hResource != RESOURCE_HANDLE_INVALID )
			{
				// Mark resource as loaded
				hResource->m_pLoadedInManifest = nullptr;

				// Remove resources that failed to load
				bool bRemoveResource = false;
				switch ( hResource->m_nType )
				{
				case RESOURCE_TYPE_PANORAMA_LAYOUT:
				case RESOURCE_TYPE_PANORAMA_SCRIPT:
				case RESOURCE_TYPE_PANORAMA_STYLE:
				case RESOURCE_TYPE_PANORAMA_DYNAMIC_IMAGES:
				case RESOURCE_TYPE_VECTOR_GRAPHIC:
					{
						CPanoramaResource *pResource = (CPanoramaResource*)( hResource->m_handle );
						if ( pResource->m_nDataSize == 0 && !pResource->m_pData )
						{
							bRemoveResource = true;
						}
					}
					break;
				case RESOURCE_TYPE_TEXTURE:
					{
						S1Wrapper_Texture_t *pTexture = (S1Wrapper_Texture_t *)( hResource->m_handle );
						if ( pTexture->IsErrorTexture() )
						{
							bRemoveResource = true;
						}
					}
					break;
				default:
					break;
				}
				
				if ( bRemoveResource )
				{
					DestroyExistingResource( hResource );

					// Resource not actually deleted (and removed from the resource system)
					// in DestroyExistingResource - only marked for delete
					// (Resource will actually be deleted in Endframe)
					// Ensure that FindExistingResourceByName will return RESOURCE_HANDLE_INVALID
					// if we are looking for the resource by name
					m_mapHandlesByNameSymbol.InsertOrReplace( nameSymbol, RESOURCE_HANDLE_INVALID );
				}
			}
		}
	}
}

void CResourceSystem::GetActualFileName( ResourceHandle_t hResource, char *pResourceNameOut, int nBufLen )
{
	GetResourceName( hResource, pResourceNameOut, nBufLen );
}

void CResourceSystem::GetResourceName( ResourceHandle_t hResource, char *pResourceName, int nBufLen )
{
	AUTO_LOCK( m_mutex );

	int nIndex = m_mapNameSymbolsByHandles.Find( hResource );
	if (nIndex != m_mapNameSymbolsByHandles.InvalidIndex())
	{
		V_strncpy(pResourceName, m_resourceNames.String(m_mapNameSymbolsByHandles[nIndex]), nBufLen);
		return;
	}

	pResourceName[0] = 0;
};

ResourceType_t CResourceSystem::GetResourceType( ResourceHandle_t hResource )  
{ 
	if ( hResource == RESOURCE_HANDLE_INVALID )
	{
		return RESOURCE_TYPE_NONE;
	}
	
	return hResource->m_nType; 
}

inline ResourceHandle_t CResourceSystem::FindExistingResourceByName(const char *pResourceName, ResourceType_t nResourceType)
{
	AUTO_LOCK( m_mutex );

	if (!pResourceName || !pResourceName[0])
	{
		return RESOURCE_HANDLE_INVALID;
	}

	ResourceType_t nDeducedType = DeduceResourceTypeFromResourceName(pResourceName);
	if (nResourceType != RESOURCE_TYPE_NONE)
	{
		if (nDeducedType != nResourceType)
		{
			// mismatch between the resource name and resource type (eg. you asked for a RESOURCE_TYPE_MODEL with name "foo.vpcf")
			Msg("Msg: %s resource is the wrong resource type!\n", pResourceName);
			return RESOURCE_HANDLE_INVALID; // don't create an entry in the id table, because what kind of resource should it be!?
		}
	}
	else
	{
		if (nDeducedType == RESOURCE_TYPE_NONE)
		{
			Msg("Msg: resource '%s' is an unknown resource type!\n", pResourceName);
			return RESOURCE_HANDLE_INVALID; // don't create an entry in the id table, because what kind of resource should it be!?
		}

		nResourceType = nDeducedType;
	}

	char pFixedUpResourceName[MAX_PATH];
	FixupResourceName(pResourceName, nResourceType, pFixedUpResourceName, sizeof(pFixedUpResourceName));

	if (!IsResourceNameValid(pFixedUpResourceName))
	{
		Msg("Msg: FindResourceByName called with invalid resource name \"%s\"\n", pFixedUpResourceName);
		// 		 return GetErrorResource( nResourceType );
		return RESOURCE_HANDLE_INVALID;
	}

	CUtlSymbol nameSymbol = m_resourceNames.Find(pFixedUpResourceName);
	if (!nameSymbol.IsValid())
	{
		return RESOURCE_HANDLE_INVALID;
	}

	int nIndex = m_mapHandlesByNameSymbol.Find(nameSymbol);

	if (nIndex == m_mapHandlesByNameSymbol.InvalidIndex())
	{
		return RESOURCE_HANDLE_INVALID;
	}

	ResourceHandle_t handle = m_mapHandlesByNameSymbol[nIndex];
	// Make sure to return RESOURCE_HANDLE_INVALID if the resource system is currently loading 
	// the resource asynchronously. It is valid to create multiple resource manifest responsible
	// for loading the same resource.
	if ( handle != RESOURCE_HANDLE_INVALID && !handle->m_pLoadedInManifest )
	{
		return handle;
	}
	else
	{
		return RESOURCE_HANDLE_INVALID;
	}
}

static void OnS1WrapperTextureCreated( void *pContext )
{
	CResourceManifest* pManifest = (CResourceManifest*)pContext;
	pManifest->m_nPendingResourceRequests--;
}

static void AsyncFileReadCompletionCallback( const FileAsyncRequest_t &request, int numReadBytes, FSAsyncStatus_t asyncStatus )
{
	ResourceData_t *pResourceData = ( ResourceData_t * )request.pContext;
	CPanoramaResource *pResource = (CPanoramaResource*)pResourceData->m_handle;

	if ( asyncStatus == FSASYNC_OK )
	{
		void* pData = malloc( numReadBytes );
		V_memcpy( pData, request.pData, numReadBytes );
		pResource->m_nDataSize = numReadBytes;
		pResource->m_pData = (char*)pData;
		pResource->m_nCRC32 = CRC32_ProcessSingleBuffer( pData, numReadBytes );
	}
	else
	{
		Warning( "Resource %s failed to load.\n", request.pszFilename );
	}

	g_pFullFileSystem->FreeOptimalReadBuffer( request.pData );
	Assert( pResourceData->m_pLoadedInManifest );
	pResourceData->m_pLoadedInManifest->m_nPendingResourceRequests--;
}

HResourceManifest CResourceSystem::CreateResourceManifest(	int nCount, const char **ppResourceFiles, ResourceManifestLoadBehavior_t nType,
															const char *pDebugName, ResourceManifestLoadPriority_t nPriority)
{
	VPROF_BUDGET( "CResourceSystem::CreateResourceManifest", VPROF_BUDGETGROUP_TENFOOT );

	AUTO_LOCK( m_mutex );

	// Extract duplicate resources. This is ok that it's slow because it
	// is only going to be called from within tools
	CUtlSymbolTable foundResources(0, nCount, true);
	CResourceManifest* pManifest = (CResourceManifest*)calloc(sizeof(CResourceManifest) + sizeof(CResourceManifest::Item_t)*nCount, 1);
	pManifest->m_pItems = (CResourceManifest::Item_t*)(pManifest + 1);
	pManifest->m_nPendingResourceRequests = 0;
	pManifest->m_completionCallback = NULL;
	pManifest->m_pContext = NULL;
	pManifest->m_bLoaded = false;

	m_activeResourceManifests.AddToTail( pManifest );
	
	int nActualCount = 0;
	for (int i = 0; i < nCount; ++i)
	{
		char pFixedResourceName[MAX_PATH];
		if (!FixupResourceName(ppResourceFiles[i], pFixedResourceName, sizeof(pFixedResourceName)))
		{
			Msg("\t\t[ FAIL ] - Manifest contains an illegal resource name \"%s\"!\n", ppResourceFiles[i]);
			continue;
		}
		Assert(*pFixedResourceName != '.');
		if (UTL_INVAL_SYMBOL == foundResources.Find(pFixedResourceName))
		{
			foundResources.AddString(pFixedResourceName);
			pManifest->m_pItems[nActualCount].m_pName = strdup(pFixedResourceName);
			pManifest->m_pItems[nActualCount].m_pLoadedInManifest = nullptr;	// For now, assuming the item is loaded by the current manifest

			ResourceType_t nDeducedType = DeduceResourceTypeFromResourceName(pFixedResourceName);
			CUtlSymbol nameSymbol = m_resourceNames.AddString(pFixedResourceName); // Add is a find or add
			ResourceHandle_t handleResource = RESOURCE_HANDLE_INVALID;
			CUtlBuffer buf;
			CPanoramaResource* pResource = NULL;

			// Check if the item is being loaded by another resource manifest
			// (ie item already registered with the resource system and the corresponding
			// ResourceData_t is marked as "loading")
			int nIndex = m_mapHandlesByNameSymbol.Find( nameSymbol );
			if ( nIndex != m_mapHandlesByNameSymbol.InvalidIndex() )
			{
				ResourceHandle_t hResource = m_mapHandlesByNameSymbol[nIndex];

				if ( ( hResource != RESOURCE_HANDLE_INVALID ) && hResource->m_pLoadedInManifest )
				{
					pManifest->m_pItems[nActualCount].m_pLoadedInManifest = hResource->m_pLoadedInManifest;
				}
			}

			if ( !pManifest->m_pItems[nActualCount].m_pLoadedInManifest )
			{
				// For Source1 CSGO use the source2 style name in the mapping table but restore the
				// content file extension for actually reading resource from disk (i.e. xml/css/js)
				RestoreContentFileExtension( pFixedResourceName );

				switch ( nDeducedType )
				{
				case RESOURCE_TYPE_PANORAMA_LAYOUT:
				case RESOURCE_TYPE_PANORAMA_SCRIPT:
				case RESOURCE_TYPE_PANORAMA_STYLE:
				case RESOURCE_TYPE_PANORAMA_DYNAMIC_IMAGES:
				case RESOURCE_TYPE_VECTOR_GRAPHIC:
					{
						// File read asynchronously.
						// Set up CPanoramaResource and ResourceData_t data structure. The file read
						// completion callback is responsible for filling CPanoramaResource with 
						// the actual data
						pResource = new CPanoramaResource();
						ResourceData_t *pResourceData = new ResourceData_t( (intptr_t)pResource, nDeducedType );
						handleResource = pResourceData;
						if ( handleResource != RESOURCE_HANDLE_INVALID )
						{
							pManifest->m_nPendingResourceRequests++;
							handleResource->m_pLoadedInManifest = pManifest;

							// Read file asynchronously
							FileAsyncRequest_t asyncFileRequest;
							asyncFileRequest.pszFilename = pFixedResourceName;
							asyncFileRequest.pContext = pResourceData;
							asyncFileRequest.pfnCallback = &AsyncFileReadCompletionCallback;
							asyncFileRequest.priority = 0;
							asyncFileRequest.flags = FSASYNC_FLAGS_ALLOCNOFREE;

							g_pFullFileSystem->AsyncRead( asyncFileRequest, &pResource->m_hAsyncControl );
						}
					}
					break;
				case RESOURCE_TYPE_TEXTURE:
					pManifest->m_nPendingResourceRequests++;	// loaded asynchronously
					handleResource = S1Wrapper_CreateTextureAsync( pFixedResourceName, UINT16_MAX, &OnS1WrapperTextureCreated, pManifest ).GetResourceHandle();
					if ( handleResource != RESOURCE_HANDLE_INVALID )
					{
						handleResource->m_pLoadedInManifest = pManifest;
					}
					break;
				case RESOURCE_TYPE_MATERIAL:
					handleResource = g_pMaterialSystem2->FindOrCreateMaterialFromResource( pFixedResourceName ).GetResourceHandle();
					break;
				default:
					Msg( "Error: Invalid resource type requested %s\n", pFixedResourceName );
				}

				// 7ls TODO 
				// Should probably change the way we add/remove resource handle to the resource system
				// Only insert new resource handle if it doesn't exist. Consider this case
				//		Thread A			Thread B
				//		Find("blah")		
				//							Find("blah")
				//		Create("blah")
				//							Create("blah")	going to create a new resource handle for "blah"
				//											ideally we should not insert it and subsequent Find
				//											will return the resource handle created on Thread A
				//
				//		Destroy("blah")		throw an error as it will be unable to find the resource handle created
				//							on Thread A

				if ( handleResource != RESOURCE_HANDLE_INVALID )
				{
					m_mapHandlesByNameSymbol.InsertOrReplace( nameSymbol, handleResource );
					m_mapNameSymbolsByHandles.InsertOrReplace( handleResource, nameSymbol );
				}
			}

			++nActualCount;
		}
	}
	pManifest->m_nCount = nActualCount;

	return (HResourceManifest)pManifest;
};

void CResourceSystem::ReloadEditedResources( bool bForceReload )
{
	AUTO_LOCK( m_mutex );

	static int sCRCInc = 1;

	FOR_EACH_MAP_FAST( m_mapNameSymbolsByHandles, idx )
	{
		ResourceHandle_t handle = m_mapNameSymbolsByHandles.Key( idx );

		int nIndex = m_mapNameSymbolsByHandles.Find( handle );
		if ( nIndex == m_mapNameSymbolsByHandles.InvalidIndex() ) Error( "Can't find res for handle\n" );
		CUtlSymbol nameSymbol = m_mapNameSymbolsByHandles[ nIndex ];

		ResourceType_t nDeducedType = DeduceResourceTypeFromResourceName( m_resourceNames.String( nameSymbol ) );

		switch ( nDeducedType )
		{
		case RESOURCE_TYPE_PANORAMA_LAYOUT:
		case RESOURCE_TYPE_PANORAMA_SCRIPT:
		case RESOURCE_TYPE_PANORAMA_STYLE:
		case RESOURCE_TYPE_VECTOR_GRAPHIC:
			char pFixedResourceName[ MAX_PATH ];
			strcpy( pFixedResourceName, m_resourceNames.String( nameSymbol ) );
			RestoreContentFileExtension( pFixedResourceName );

			Msg( "Checking resource %s\n", pFixedResourceName );

			CUtlBuffer buf;
			if ( !g_pFullFileSystem->ReadFile( pFixedResourceName, NULL, buf ) )
			{
				Msg( "Resource %s failed to load.\n", pFixedResourceName );
				continue;
			}
						
			CPanoramaResource resource, *pResource;

			void* pData = malloc( buf.TellPut() );
			V_memcpy( pData, buf.Base(), buf.TellPut() );
			resource.m_nDataSize = buf.TellPut();
			resource.m_pData = (char*)pData;
			resource.m_nCRC32 = CRC32_ProcessSingleBuffer( pData, buf.TellPut() );

			pResource = (CPanoramaResource*)handle->m_handle;

			if ( ( pResource->m_nCRC32 != resource.m_nCRC32 ) || bForceReload )
			{
				Msg( "*** Reload : %s\n", pFixedResourceName );

				free( (void*)pResource->m_pData );
				pResource->m_nDataSize = resource.m_nDataSize;
				pResource->m_nCRC32 = resource.m_nCRC32;

				int nValidCRC32 = pResource->m_nCRC32;

				if ( bForceReload )
				{
					pResource->m_nCRC32 += sCRCInc++;
				}

				pResource->m_pData = resource.m_pData;

				g_ResourceSystemSingleThreaded.NotifyResourceStatusChange( 0, handle, nDeducedType, RESOURCE_LOAD_RELOAD );

				if (bForceReload)
				{
					pResource->m_nCRC32 = nValidCRC32;
				}
			}
			else
			{
				free( pData );
			}
		}
	}
}

ResourceHandle_t CResourceSystem::BlockingLoadResourceByName( const char *pResourceName, ResourceType_t nResourceType, const char *pDebugName )
{
	ResourceHandle_t hResource = FindExistingResourceByName( pResourceName, nResourceType );

	if ( hResource != RESOURCE_HANDLE_INVALID )
		return hResource;

	HResourceManifest hManifest = CreateResourceManifest( 1, &pResourceName, RESOURCE_MANIFEST_LOAD_STREAMING_DATA, "Panorama", RESOURCE_MANIFEST_LOAD_PRIORITY_HIGH );
	if ( hManifest != RESOURCE_MANIFEST_HANDLE_INVALID )
	{
		g_pResourceSystem->ForceSynchronizationAndBlockUntilManifestLoaded( hManifest );
	}

	hResource = FindExistingResourceByName( pResourceName, nResourceType );

	if ( hManifest != RESOURCE_MANIFEST_HANDLE_INVALID )
	{
		DestroyResourceManifest( hManifest );
	}
	return hResource;
}

void CResourceSystem::AddExistingResource( const char* pName, ResourceHandle_t handle )
{
	AUTO_LOCK( m_mutex );

	ResourceHandle_t found = FindExistingResourceByName( pName, RESOURCE_TYPE_NONE );

	if ( found != RESOURCE_HANDLE_INVALID )
	{
		if ( handle != found ) Error( "Attempt to add a new handle for an existing resource");
	}
	else
	{
		char pFixedResourceName[ MAX_PATH ];
		if ( !FixupResourceName( pName, pFixedResourceName, sizeof( pFixedResourceName ) ) )
		{
			Msg( "\t\t[ FAIL ] - Manifest contains an illegal resource name \"%s\"!\n", pName );
		}

		CUtlSymbol nameSymbol = m_resourceNames.AddString( pFixedResourceName );
		m_mapHandlesByNameSymbol.InsertOrReplace(nameSymbol, handle);
		m_mapNameSymbolsByHandles.InsertOrReplace(handle, nameSymbol);
	}
}

void CResourceSystem::DestroyExistingResource( ResourceHandle_t handle )
{
	if ( handle == RESOURCE_HANDLE_INVALID )
		return;

	m_DeletionRequests.PushItem( handle );
}

void CResourceSystem::DeleteRequestedResources()
{
	AUTO_LOCK( m_mutex );

	// Build a list of resources to delete, remove duplicates
	ResourceHandle_t hResource;
	CUtlVectorFixedGrowable< ResourceHandle_t, 32 > list;
	while ( m_DeletionRequests.PopItem( &hResource ) )
	{
		// Could happen - someone decrements a refcount, which queues up a deletion,
		// then someone else adds a reference later
		if ( hResource->m_nRefCount > 0 )
		{
			continue;
		}
		
		// Remove handles from resource system tables
		// If handle is not in m_mapNameSymbolsByHandles, it is a duplicate
		int nIndex = m_mapNameSymbolsByHandles.Find( hResource );
		if ( nIndex != m_mapNameSymbolsByHandles.InvalidIndex() )
		{
			CUtlSymbol nameSymbol = m_mapNameSymbolsByHandles[nIndex];

			m_mapHandlesByNameSymbol.InsertOrReplace( nameSymbol, RESOURCE_HANDLE_INVALID );
			m_mapNameSymbolsByHandles.RemoveAt( nIndex );

			list.AddToTail( hResource );
		}
		else
		{
			// PARANOID MODE - Make sure it is a duplicate
			DevAssert( list.Find( hResource ) != -1 );
		}
	}


	int nCount = list.Count();
	if ( nCount )
	{
#ifdef DX_TO_GL_ABSTRACTION
		// DeleteRequestedResources called on the "Main" thread
		// On Linux/Mac, the "Render" thread owns the GL context.
		// Make sure to lock/unlock materialsystem in order to own
		// the GL context before deleting textures / render targets
		// (if we do not own the GL context, GL will leak VRAM !)
		MaterialLock_t hMaterialLock = g_pMaterialSystem->Lock();
#endif
		for ( int i = 0; i < nCount; i++ )
		{
			ResourceHandle_t hResourceToDelete = list[i];

			switch ( GetResourceType( hResourceToDelete ) )
			{
			case RESOURCE_TYPE_TEXTURE:
				S1Wrapper_DestroyTexture( hResourceToDelete );
				break;

			case RESOURCE_TYPE_PANORAMA_LAYOUT:
			case RESOURCE_TYPE_PANORAMA_SCRIPT:
			case RESOURCE_TYPE_PANORAMA_STYLE:
			case RESOURCE_TYPE_VECTOR_GRAPHIC:
			{
				CPanoramaResource *pResource = (CPanoramaResource*)hResourceToDelete->m_handle;
				if ( pResource->m_pData )
				{
					free( (void*)pResource->m_pData );
				}
				delete pResource;
				delete hResourceToDelete;
				break;
			}

			case RESOURCE_TYPE_MATERIAL:
				break;

			default:
				Warning( "Trying to destroy panorama resource of type %s\n", (char*)GetResourceType( hResourceToDelete ) );
				break;
			}
		}
#ifdef DX_TO_GL_ABSTRACTION
		g_pMaterialSystem->Unlock( hMaterialLock );
#endif
	}
}

void CResourceSystem::DestroyResourceManifest(HResourceManifest hManifest)
{
	AUTO_LOCK( m_mutex );

	if (hManifest)
	{
		CResourceManifest* pManifest = (CResourceManifest*)hManifest;

		// Calling FindAndRemove as we need to preserve order (resource manifest can 
		// be dependant on another resource manifest)
		bool bDeleted = m_activeResourceManifests.FindAndRemove( pManifest );
		Verify( bDeleted == true );

		for (int i = 0; i < pManifest->m_nCount; ++i)
		{
			if (pManifest->m_pItems[i].m_pName)
			{
				free(pManifest->m_pItems[i].m_pName);
			}
		}
		free(pManifest);
	}
};


HResourceManifest CResourceSystem::LoadResourceManifestFile(const char *pManifestFileName, ResourceManifestLoadBehavior_t nType,
	const char *pDebugName,
	ResourceManifestLoadPriority_t nPriority)
{
	if (V_strstr(pManifestFileName, "panorama.vrman"))
	{
		const char *resources[] =
		{
			"panorama/dynamic_images.vpdi"
		};

		return CreateResourceManifest(ARRAYSIZE(resources), resources, nType, pDebugName, nPriority);
	}
	else
	{
		Msg( "Manifest file %s not read by dummy implementation of LoadResourceManifestFile.\n", pManifestFileName );
	}
	return 0;
}

void CResourceSystem::SetManifestCompletionCallback(HResourceManifest hManifest, ResourceManifestLoadCompletionCallback_t callback, void *pContext)
{
	if ( hManifest )
	{
		CResourceManifest* pManifest = (CResourceManifest*)hManifest;

		pManifest->SetCompletionCallback( callback, pContext );
	}
}

// Blocks until the given manifest is loaded - may cause other manifest to load as well
// Must be called on the main thread (following source 2 implementation)
void CResourceSystem::ForceSynchronizationAndBlockUntilManifestLoaded( HResourceManifest hManifest )
{
	Assert( ThreadInMainThread() );

	CResourceManifest *pManifest = (CResourceManifest*)hManifest;
	if ( !pManifest || pManifest->IsLoaded() )
		return;

	while ( true )
	{
		Update();
		if ( pManifest->IsLoaded() )
			break;
	}
}

// Helper function to build a list of  all layout / style / script file from a folder and its subfolders
// It is the caller's responsibility to deallocate strings from the list.
static void PreloadResources_Helper( CUtlVector< const char* > &list, const char *pDirectory )
{
	char pSearchString[MAX_PATH];
	V_snprintf( pSearchString, MAX_PATH, "%s\\*", pDirectory );
	
	// get the list of files
	FileFindHandle_t hFind;
	const char *pFoundFile = g_pFullFileSystem->FindFirstEx( pSearchString, "MOD", &hFind );

	// add all the items
	CUtlVector< CUtlString > subDirs;
	for ( ; pFoundFile; pFoundFile = g_pFullFileSystem->FindNext( hFind ) )
	{
		if ( g_pFullFileSystem->FindIsDirectory( hFind ) )
		{
			if ( V_strnicmp( pFoundFile, ".", 2 ) && V_strnicmp( pFoundFile, "..", 3 ) )
			{
				char pChildDir[MAX_PATH];
				V_snprintf( pChildDir, MAX_PATH, "%s\\%s", pDirectory, pFoundFile );

				subDirs.AddToTail( pChildDir );
			}
			continue;
		}

		// Check extension 
		const char* pExtension = V_GetFileExtensionSafe( pFoundFile );
		if ( V_stricmp( pExtension, "xml" ) && V_stricmp( pExtension, "js" ) && V_stricmp( pExtension, "css" ) )
			continue;

		// Allocate space for the string that we are about to add to the list. Concatenation of pDirectory and pFoundFile
		// Make sure to use the S2 extension format ie vxml / vcss / vjs

		char pFileNameNoExt[MAX_PATH];
		V_StripExtension( pFoundFile, pFileNameNoExt, MAX_PATH );

		int nLen = V_strlen( pDirectory ) + V_strlen( pFoundFile ) + 3;	// Adding 3 to account for '\\', 'v' (before the extension) and the null terminator
		char *pChildPath = (char*)malloc( nLen * sizeof( char ) );
		V_snprintf( pChildPath, nLen, "%s\\%s.v%s", pDirectory, pFileNameNoExt, pExtension );

		list.AddToTail( pChildPath );
	}

	g_pFullFileSystem->FindClose( hFind );

	int nCount = subDirs.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		PreloadResources_Helper( list, subDirs[i] );
	}
}

// CS:GO crypto signature digest validation hacky callback into higher level panorama.dll
bool ( *g_pfnPanoramaResourceFileIntegrityCheck )( CUtlBuffer &bufFileData, void *&pvResourceData, int &numResourceBytes ) = NULL;

// Preload all layout / style / script files from the panorama folder
void CResourceSystem::PreloadResources(bool bFromZip)
{
#if DEVELOPMENT_ONLY
	if ( !bFromZip )
	{
		// Collect list of files to load

		CUtlVector< const char* > resourceList;
		PreloadResources_Helper( resourceList, "panorama" );

		// Load files

		HResourceManifest hManifest = CreateResourceManifest( resourceList.Count(), resourceList.Base(), RESOURCE_MANIFEST_LOAD_STREAMING_DATA, "Panorama", RESOURCE_MANIFEST_LOAD_PRIORITY_HIGH );
		if ( hManifest != RESOURCE_MANIFEST_HANDLE_INVALID )
		{
			g_pResourceSystem->ForceSynchronizationAndBlockUntilManifestLoaded( hManifest );
		}

		if ( hManifest != RESOURCE_MANIFEST_HANDLE_INVALID )
		{
			DestroyResourceManifest( hManifest );
		}

		// Free all strings allocated by PreloadResources_Helper

		for ( int nResource = 0; nResource < resourceList.Count(); ++nResource )
		{
			free( (void*)resourceList.Element( nResource ) );
		}

	}
	else
#endif
	{
		CUtlBuffer buf;
		char fname[ MAX_PATH ];
		char ext[ MAX_PATH ];
		int nFileSize;

		const char* pZipName = PANORAMA_ZIPFILE_NAME;

		if ( !g_pFullFileSystem->ReadFile( pZipName, NULL, buf ) )
		{
			Error( "Resource %s failed to load.\n", pZipName );
		}

		if ( !g_pfnPanoramaResourceFileIntegrityCheck )
		{
			Error( "Resource %s parser is not configured.\n", pZipName );
		}

		void *pvResourceFileData = NULL;
		int numResourceFileBytes = 0;
		if ( !(*g_pfnPanoramaResourceFileIntegrityCheck)( buf, pvResourceFileData, numResourceFileBytes ) )
		{
			Error( "Resource %s has invalid data.\n", pZipName );
		}

		IZip* pZip = IZip::CreateZip();
		pZip->ParseFromBuffer( pvResourceFileData, numResourceFileBytes );

		int numFilesLoadedFromZip = 0;
		int id = -1;

		while ( (id = pZip->GetNextFilename(id, fname, sizeof(fname), nFileSize ))  != -1)
		{
			const char* pExtension = V_GetFileExtensionSafe( fname );

			if (!strcmp(pExtension, "cfg"))
			{
				// cfg files are not loaded via the resource system

				int i = m_CfgFiles.AddToTail();
				m_CfgFiles[ i ].m_pName = strdup( fname );
				V_FixSlashes( m_CfgFiles[ i ].m_pName, '/' );

				// Read file
				buf.Clear();
				if ( !pZip->ReadFileFromZip( fname, false, buf ) )
				{
					Error( "Resource %s is corrupt around %s\n", pZipName, fname );
				}
				++ numFilesLoadedFromZip;

				char* pContent = (char*)malloc( buf.TellPut() + 1 );
				memcpy( pContent, buf.Base(), buf.TellPut() );
				pContent[ buf.TellPut() ] = 0;

				m_CfgFiles[ i ].m_pContent = pContent;
			}
			else
			{
				char pFixedResourceName[ MAX_PATH ];
				
				if ( !FixupResourceName( fname, pFixedResourceName, sizeof( pFixedResourceName ) ) )
				{
					Msg( "\t\t[ FAIL ] - Manifest contains an illegal resource name \"%s\"!\n", fname );
					continue;
				}

				// Read file
				buf.Clear();
				if ( !pZip->ReadFileFromZip( fname, false, buf ) )
				{
					Error( "Resource %s is corrupt around %s\n", pZipName, fname );
				}
				++ numFilesLoadedFromZip;

#if defined( OSX ) && defined( __aarch64__ ) && defined( NO_STEAM )
				if ( CommandLine()->ParmValue( "-panorama_native_overlay", "" )[0] )
				{
					CUtlBuffer overlayBuffer;
					if ( g_pFullFileSystem->ReadFile( pFixedResourceName, "PANORAMA_NATIVE_OVERLAY", overlayBuffer ) )
					{
						buf.Clear();
						buf.Put( overlayBuffer.Base(), overlayBuffer.TellPut() );
					}
				}
#endif

				// Replaces extensions css, js, xml with vcss etc..
				V_strncpy( ext, pExtension, MAX_PATH );
				if ( !(V_stricmp( ext, "xml" ) && V_stricmp( ext, "js" ) && V_stricmp( ext, "css" )) )
				{
					V_StripExtension( pFixedResourceName, fname, MAX_PATH );
					V_snprintf( pFixedResourceName, MAX_PATH, "%s.v%s", fname, ext );
				}

				// Add file to resource sys
				int nLen = buf.TellPut();
				void* pData = malloc( buf.TellPut() );
				memcpy( pData, buf.Base(), nLen );

				ResourceType_t nDeducedType = DeduceResourceTypeFromResourceName( pFixedResourceName );
				CUtlSymbol nameSymbol = m_resourceNames.AddString( pFixedResourceName ); // Add is a find or add
				ResourceHandle_t handleResource = RESOURCE_HANDLE_INVALID;

				CPanoramaResource* pResource = new CPanoramaResource();
				ResourceData_t *pResourceData = new ResourceData_t( (intptr_t)pResource, nDeducedType );
				handleResource = pResourceData;
				if ( handleResource != RESOURCE_HANDLE_INVALID )
				{
					pResource->m_nDataSize = nLen;
					pResource->m_pData = (char*)pData;
					pResource->m_nCRC32 = CRC32_ProcessSingleBuffer( pData, nLen );

					// Insert into res sys
					m_mapHandlesByNameSymbol.InsertOrReplace( nameSymbol, handleResource );
					m_mapNameSymbolsByHandles.InsertOrReplace( handleResource, nameSymbol );

					//Msg( "Loaded %s\n", pFixedResourceName );
				}
			}
		}

		if ( !numFilesLoadedFromZip )
		{
			Error( "Resource %s contains no packed data.\n", pZipName );
		}
	}
}

char* CResourceSystem::LoadFromPanZip( const char* fname )
{
	for ( int i = 0; i < m_CfgFiles.Count(); i++ )
	{
		if ( !strcmp(fname, m_CfgFiles[i].m_pName))
		{
			return m_CfgFiles[ i ].m_pContent;
		}
	}

	return NULL;
}

uint16 CResourceSystem::GetTextureUniqueId()
{
	AUTO_LOCK( m_mutex );
	
	uint16 id = m_TextureUniqueIdFreeList.Count() ? m_TextureUniqueIdFreeList.RemoveAtTail() : ( ++m_nNextTextureUniqueId );
	AssertMsg( id != UINT16_MAX, "CResourceSystem - Texture unique identifier overflow" );
	return id;
}

void CResourceSystem::MarkTextureUniqueIdUnused( uint16 nUniqueId )
{
	AUTO_LOCK( m_mutex );

	if ( nUniqueId != UINT16_MAX )
	{
		// Recycle texture unique identifier
		m_TextureUniqueIdFreeList.Insert( nUniqueId );
	}
}

//--------------------------------------------------------------------------------------------------
//
//	CResourceManifest Methods
//
//--------------------------------------------------------------------------------------------------

void CResourceManifest::SetCompletionCallback( ResourceManifestLoadCompletionCallback_t callback, void *pContext )
{
	m_completionCallback = callback;
	m_pContext = pContext;
}

void CResourceManifest::InvokeCompletionCallback()
{
	VPROF_BUDGET( "CResourceManifest::InvokeCompletionCallback()", VPROF_BUDGETGROUP_TENFOOT );
	if ( m_completionCallback )
	{
		m_completionCallback( ( HResourceManifest )this, m_pContext );
	}
}

bool CResourceManifest::IsLoaded() const
{
	return m_bLoaded;
}

void CResourceManifest::MarkLoaded()
{
	m_bLoaded = true;
}


//--------------------------------------------------------------------------------------------------
// Assets
//--------------------------------------------------------------------------------------------------


class CAsset : public IAsset
{
public:
	virtual const char *GetAbsolutePath_Transient( AssetLocation_t nLocation, ModSearchIndex_t nModFilter = MOD_SEARCH_INDEX_LEAFIEST_FOR_LOCATION ) const;

	CUtlString m_filename;
};

const char *CAsset::GetAbsolutePath_Transient( AssetLocation_t nLocation, ModSearchIndex_t nModFilter) const
{
	return m_filename.Get();
}


class CAssetSystem : IAssetSystem
{
public:

	CAssetSystem() { m_crc = 0; }

	virtual IAsset*	FindAssetByFilename( const char *pFilename );
	virtual void	OpenForEdit( const CUtlVector<IAsset*> &assets, AssetLocation_t nFileLocation = ASSET_LOCATION_CONTENT );
	virtual bool	GetAbsoluteFileCRC( const char *pAbsolutePath, CRC32_t *pOutCRC );

	CRC32_t m_crc;
	CUtlStringMap<IAsset*> m_assets;

};

IAsset*	CAssetSystem::FindAssetByFilename( const char *pFilename ) 
{
	if ( m_assets.Find( pFilename ) )
	{
		return m_assets[ pFilename ];
	}

	CAsset* pAsset = new CAsset;
	pAsset->m_filename = pFilename;

	m_assets[ pFilename ] = pAsset;

	return (IAsset*)pAsset;
}

void	CAssetSystem::OpenForEdit( const CUtlVector<IAsset*> &assets, AssetLocation_t nFileLocation) 
{
}

bool	CAssetSystem::GetAbsoluteFileCRC( const char *pAbsolutePath, CRC32_t *pOutCRC ) 
{ 
	m_crc++;
	*pOutCRC = m_crc;
	return true;
}

CAssetSystem g_AssetSystem;
IAssetSystem* g_pAssetSystem = (IAssetSystem*)&g_AssetSystem;



//--------------------------------------------------------------------------------------------------
// ConCommand to reload resources
//--------------------------------------------------------------------------------------------------

void wrapper_panorama_reload( bool bForceReload )
{
	g_ResourceSystem.ReloadEditedResources( bForceReload );
}




