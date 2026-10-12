#ifndef INCLUDED_IRESOURCESYSTEM_H
#define INCLUDED_IRESOURCESYSTEM_H
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

#include "wrap_other.h"

#include "resourcesystem/resourcehandle.h"
#include "resourcesystem/resourcehandletypes.h"
#include "materialsystem2/imaterialsystem2.h"
#include "scenesystem/iscenesystem.h"

#include "../rendersystem/irendercontext.h"

#define MemAlloc_Free(p) g_pMemAlloc->Free( p )


//--------------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------------

struct ResourceFileHeader_t
{

};

typedef uint32 ResourceBlockId_t;

template <typename T>
class CResourcePointer
{
	T* m_ptr;
public:
	T* GetPtr() const
	{
		return ( m_ptr );
	}
};

struct ResourceBlockEntry_t
{
	ResourceBlockId_t m_nBlockType;
	CResourcePointer<void> m_pBlockData;
	uint32 m_nBlockSize;
};



//-----------------------------------------------------------------------------
// Resource block IDs
//-----------------------------------------------------------------------------
#define RSRC_BYTE_POS( byteVal, shft )	ResourceBlockId_t( uint32(uint8(byteVal)) << uint8(shft * 8) )
#define MK_RSRC_BLOCK_ID(a, b, c, d)	ResourceBlockId_t( RSRC_BYTE_POS(a, 0) | RSRC_BYTE_POS(b, 1) | RSRC_BYTE_POS(c, 2) | RSRC_BYTE_POS(d, 3) )
#define RESOURCE_BLOCK_ID_INVALID 0xFFFFFFFF

#define RESOURCE_BLOCK_ID_DATA MK_RSRC_BLOCK_ID( 'D', 'A', 'T', 'A' )

#define RESOURCE_BLOCK_ID_BUF_SIZE 5 // 4CC + '\0'

//--------------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------------

// Believe never called in src2
inline const ResourceBlockEntry_t *Resource_GetBlockEntry( const ResourceFileHeader_t *pHeader, ResourceBlockId_t id )
{
	DebuggerBreak();
	return 0;				
}

FORCEINLINE bool ResourceIsError( ResourceHandle_t handle ) {
	return ( handle == RESOURCE_HANDLE_INVALID );
}
FORCEINLINE bool ResourceIsLoaded( ResourceHandle_t handle ) {
	return ( handle != RESOURCE_HANDLE_INVALID );
}

//--------------------------------------------------------------------------------------------------
// Interfaces
//--------------------------------------------------------------------------------------------------

class IRD_BlockingLoadResourceFileDataUtils
{
public:
	virtual void SetRequestInvalid() = 0;
	virtual void SetResourceData( void *pData ) = 0;
	//virtual void SetResourceHeader( const ResourceFileHeader_t *pHeader ) = 0;
	//virtual void SetResultRecord( ResourceNonStreamingDataRecord_t *pRecord ) = 0;
};

abstract_class IRD_RegisterResourceDataUtils
{
public:
	virtual void SetDataRegistrationFailed() = 0;
	virtual bool ShouldLoadStreamingData() = 0;
	virtual bool IsReloading() = 0;
	virtual void SetFinalResourceData( void *pData ) = 0;
	//virtual bool StreamResourceReferenceAndFixup( ResourceType_t nResourceType, ResourceHandle_t *pOutHandle, ResourceHandle_t hReferringResource, ResourceId_t nReferringResourceId, ResourceId_t nResourceId, const class CResourceRefTable *pRefTable, const char *pResourceName, char *pOutExpectedFile, int nExpectedFileBufSize, bool bDeferred ) = 0;
};

///-----------------------------------------------------------------------------
/// Utilities to deal with type construction
///-----------------------------------------------------------------------------
abstract_class IResourceAllocatorUtils
{
public:
	// Assumes control of the memory passed into it
	// NOTE: This may make a copy of the data under some circumstances.
	// The pointer to cache off is returned 
	virtual void *AssumeControl( const void *pData ) = 0;

	virtual size_t GetStreamingDataOffset( ) const = 0;
	virtual size_t GetDataSize( ) const = 0;
	virtual bool ShouldLoadStreamingData() const = 0;
	virtual ResourceHandle_t GetResourceHandle() const = 0;
};

abstract_class IResourceDeallocatorUtils
{
public:
	// Releases control of the memory passed into it
	virtual void ReleaseControl( const void *pData ) = 0; 
	virtual ResourceDeallocationType_t GetDeallocationType( ) const = 0;
	virtual ResourceHandle_t GetResourceHandle() const = 0;
	virtual bool IsQueuedDeallocate() const = 0;
	virtual void *GetReleasedData() const = 0;
};

///-----------------------------------------------------------------------------
///-----------------------------------------------------------------------------
class IAsyncResourceDataRequest
{
public:
	virtual AsyncRequestStatus_t GetRequestStatus() = 0;
	virtual const char *GetFileName() = 0;
	virtual void *GetResultBuffer() = 0;
	virtual size_t GetResultBufferSize() = 0;
	virtual void KeepResultBuffer() = 0;								// User wants to keeps buffer allocated by the file system
	virtual void ReleaseResultBuffer() = 0;								// User decides they want the request to take care of releasing the buffer
	virtual void AssignCallback( CFunctor* pCallback ) = 0;						// Add a completion callback to this request
};

///-----------------------------------------------------------------------------
/// Helpers only available to resource type managers
///-----------------------------------------------------------------------------
abstract_class IResourceSystemUtils
{
public:
	/// Indicates a loading manifest needs to wait for a streaming request
	virtual void WaitForStreamingData( ResourceHandle_t hResource ) = 0;

	/// Indicates a streaming request has finished
	virtual void MarkStreamingDataLoaded( ResourceHandle_t hResource ) = 0;

	/// nLoadSize == 0 means load all available data till the end of the file
	virtual IAsyncResourceDataRequest *CreateAsyncResourceDataRequest( ResourceHandle_t hResource, int64 nFileOffset, size_t nLoadSizeBytes ) = 0;

	virtual void SubmitAsyncResourceDataRequest( IAsyncResourceDataRequest *pRequest ) = 0;
	virtual bool IsBlockingOnManifestLoad() = 0;
};

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
abstract_class IResourceTypeManager
{
public:
	//--------------------------------------------------------------------------------------------------
	//--------------------------------------------------------------------------------------------------
	virtual bool Init( IResourceSystemUtils *pUtils ) { return true; }
	virtual void Shutdown() { }

	// Returns an error resource, will be called after Init(). This will be the data pointer for the error resource,
	// and any resource of this type that fails to load. (Can be NULL but be aware that people may try to dereference it!)
	virtual void *GetErrorResource() = 0;

	//--------------------------------------------------------------------------------------------------
	// Frame updates
	//--------------------------------------------------------------------------------------------------
	virtual bool NeedsFrameUpdate() const { return false; }
	virtual void FrameUpdate( int nFinishedFrameCount ) {}

	//--------------------------------------------------------------------------------------------------
	// Manifest completion
	//--------------------------------------------------------------------------------------------------
	// Does the resource type manager do non-zero work in NotifyManifestResourceFinished?
	// If this function returns false, then NotifyManifestResourceFinished will *not* be called.
	virtual bool RequiresManifestResourceFinishedCall() const { return false; }

	// Called after a manifest finished loading, containing a resource defined by this type manager
	// At this point, all extrefs are loaded.
	// NOTE: You must return 'true' from RequiresManifestResourceFinishedCall() for this to get called!
	virtual void NotifyManifestResourceFinished( ResourceHandle_t hResource, ResourceLoadType_t nLoadType ) { }

	// This will cause resources to be partially resident until the manifest is complete (RESOURCE_STATUS_PARTIALLY_RESIDENT)
	// This is generally necessary if your resource contains any extrefs,
	// since it shouldn't be visible to the outside world until they have been loaded also.
	virtual bool RequiresPostLoadFixup() = 0;

	//--------------------------------------------------------------------------------------------------
	// Interface for resources with streaming data
	//--------------------------------------------------------------------------------------------------
	// Does this resource type contain streaming data? (If not, the system will just load the entire file in one gulp)
	virtual bool DoesResourceTypeContainStreamingData() const { return false; }

	// Only used when the resoruce type contains streaming data; indicates how much data to load 
	// when loading the initial resource header / non-streaming data. Return -1
	// to allow the resource system to choose a reasonable default size
	virtual int GetNonStreamingDataLoadSize() const { Assert(false); return -1; }

	// Called to ensure the streaming data for a resource is resident. Should be a no-op for a resource with its streaming data already resident.
	// Should call IResourceSystemUtils::WaitForStreamingData() and issue a load request if streaming data is not resident.
	// Called only for type managers that return true in DoesResourceTypeContainStreamingData().
	virtual void LoadStreamingData( ResourceHandle_t hResource, IResourceSystemUtils *pUtils ) { Assert(false); }

	//--------------------------------------------------------------------------------------------------
	// Data Handling API
	//--------------------------------------------------------------------------------------------------
	virtual void AllocateResource( ResourceHandle_t hResource, ResourceId_t nId, const ResourceFileHeader_t *pHeader, IRD_RegisterResourceDataUtils *pUtils ) = 0;
	virtual void DeallocateResource( void *pResourceData, IResourceDeallocatorUtils *pDealloc ) = 0;
};


abstract_class IToolsResourceListener
{
public:
	virtual void NotifyResourceStatusChange( ResourceId_t nResourceId, ResourceHandle_t hResource, ResourceType_t nResourceType, ResourceLoadType_t nLoadType ) = 0;
};

abstract_class IResourceSystem
{
public:

	virtual void				Init() = 0;
	virtual void				Shutdown() = 0;

	virtual void				Update() = 0;
	virtual void				UpdateSimple() = 0;
	virtual void				GetActualFileName( ResourceHandle_t hResource, char *pResourceNameOut, int nBufLen ) = 0;
	virtual void				GetResourceName( ResourceHandle_t hResource, char *pResourceName, int nBufLen ) = 0;
	virtual ResourceType_t		GetResourceType( ResourceHandle_t hResource ) = 0;
	// these two (find and create) must be called as a pair to avoid memory leak
	virtual ResourceHandle_t	FindExistingResourceByName( const char *pResourceName, ResourceType_t nResourceType ) = 0;
	virtual HResourceManifest	CreateResourceManifest( int nCount, const char **ppResourceFiles, ResourceManifestLoadBehavior_t nType, const char *pDebugName, ResourceManifestLoadPriority_t nPriority = RESOURCE_MANIFEST_LOAD_PRIORITY_DEFAULT ) = 0;
	virtual void				ForceSynchronizationAndBlockUntilManifestLoaded( HResourceManifest hManifest ) = 0;
	virtual void				SetManifestCompletionCallback( HResourceManifest hManifest, ResourceManifestLoadCompletionCallback_t callback, void *pContext ) = 0;
	virtual void				DestroyResourceManifest( HResourceManifest hManifest ) = 0;
	virtual HResourceManifest	LoadResourceManifestFile( const char *pManifestFileName, ResourceManifestLoadBehavior_t nType, const char *pDebugName, ResourceManifestLoadPriority_t nPriority = RESOURCE_MANIFEST_LOAD_PRIORITY_DEFAULT ) = 0;
	virtual void				GetResourcesNamesInManifest( HResourceManifest hManifest, CUtlVector< CUtlString > &list ) const = 0;
	virtual void				RemoveResourceTypeManager( IResourceTypeManager *pTypeManager ) = 0;

	virtual ResourceHandle_t	BlockingLoadResourceByName( const char *pResourceName, ResourceType_t nResourceType, const char *pDebugName ) = 0;

	virtual void				AddExistingResource( const char* pName, ResourceHandle_t ) = 0;
	virtual void				DestroyExistingResource( ResourceHandle_t ) = 0;

	virtual char*				LoadFromPanZip( const char* fname ) = 0;

	virtual uint16				GetTextureUniqueId() = 0;
	virtual void				MarkTextureUniqueIdUnused( uint16 nUniqueId ) = 0;


	template <ResourceType_t nResourceType> inline ResourceHandle_t	FindExistingResourceByName( const char *pResourceName )
	{
		return FindExistingResourceByName( pResourceName, nResourceType );
	}
	template <ResourceType_t nResourceType> void InstallTypeManager( IResourceTypeManager *pTypeManager ) {}

};

#define g_pResourceSystem g_pResourceSystem2
extern IResourceSystem* g_pResourceSystem;

abstract_class IResourceSystemSingleThreaded
{
public:
	virtual void RegisterToolsResourceListener( IToolsResourceListener* pListener ) = 0;
	virtual void UnregisterToolsResourceListener( IToolsResourceListener* pListener ) = 0;
	virtual void NotifyResourceStatusChange( ResourceId_t nResourceId, ResourceHandle_t hResource, ResourceType_t nResourceType, ResourceLoadType_t nLoadType ) = 0;

	// Given a list of filenames, create in the CUtlBuffer a .vrman file
	//virtual void CreateManifestFileFromStringList( const char *pDstFilename, const CUtlSymbolTable &resourceNames, CUtlBuffer &buf ) = 0;

	// Does a blocking load of a resource file, returning the non-streaming data
	// You must call back to free this pointer returned by this function
	// ppResourceData is a pointer to the actual resource data
	// ppOutHeader (optional) will point to the resource header
	// NOTE: If you're using this from inside a resource compile, you should probably use CSisterResourceLoader instead (which ensures the resource is up-to-date)
// 	virtual ResourceNonStreamingDataHandle_t BlockingLoadResourceFileNonStreamingPortion( const char *pResourceName, void **ppResourceData, const ResourceFileHeader_t **ppOutHeader ) = 0;
// 	virtual void FreeResourceFileNonStreamingPortion( ResourceNonStreamingDataHandle_t hData ) = 0;
};


extern IResourceSystemSingleThreaded* g_pResourceSystemSingleThreaded;


inline void ResourceTypeToExt( char *pOutChars, size_t nBufferSize, ResourceType_t nType )
{
	Assert( nBufferSize >= RESOURCE_EXT_MIN_BUF_SIZE );
	V_memset( pOutChars, 0, nBufferSize );
	( *(uint64*)( pOutChars ) ) = LittleQWord( nType );
}

template <size_t maxLenInChars> inline void ResourceTypeToExt( char( &pDest )[ maxLenInChars ], ResourceType_t nType )
{
	COMPILE_TIME_ASSERT( maxLenInChars >= RESOURCE_EXT_MIN_BUF_SIZE );
	ResourceTypeToExt( pDest, maxLenInChars, nType );
}

inline void CharsToResourceType(ResourceType_t *pOutType, const char *pChars)
{
	*pOutType = 0;
	int nShift = 0;
	while (*pChars)
	{
		*pOutType |= (uint64(*pChars) << nShift);
		nShift += 8;
		pChars++;
	}
}

FORCEINLINE ResourceType_t CharsToResourceType( const char *pChars )
{
	ResourceType_t ret;
	CharsToResourceType( &ret, pChars );
	return ret;
}



inline ResourceType_t DeduceResourceTypeFromResourceName( const char *pResourceName )
{
	if (!pResourceName)
		return RESOURCE_TYPE_NONE;

	// TODO - I bet this can be better; needs to deal with ugly cases though
	const char *pExt = V_GetFileExtension(pResourceName);

	if (!pExt)
		return RESOURCE_TYPE_NONE;

	// clear out any '_' and trailing characters
	char pExtWithNoTrailing[MAX_FILEPATH];
	V_memset(pExtWithNoTrailing, 0, sizeof(pExtWithNoTrailing));
	pExtWithNoTrailing[MAX_FILEPATH - 1] = 0;
	int nExtLen = V_strlen(pExt);
	V_memcpy(pExtWithNoTrailing, pExt, MIN(nExtLen + 1, MAX_FILEPATH - 1));
	V_strlower(pExtWithNoTrailing);
	for (int i = 0; i < nExtLen; ++i)
	{
		if (pExtWithNoTrailing[i] == '_')
		{
			for (int j = i; j < nExtLen; ++j)
			{
				pExtWithNoTrailing[j] = 0;
			}
			break;
		}
	}

	// go
	ResourceType_t nResult = 0;
	CharsToResourceType(&nResult, pExtWithNoTrailing);
	return nResult;
}


extern IResourceSystemSingleThreaded* g_pResourceSystemSingleThreaded;



// Callback interface to get information about textures being streamed in and being evicted
class ITextureResidencyListener
{
public:
	virtual void TextureBecameFullyResident( HRenderTexture hTex ) = 0;
	virtual void TextureBecameEvicted( HRenderTexture hTex ) = 0;
};



inline void Recycle_Delete( const void *pData )
{
	delete[]( uint8* )pData;
}

inline void Recycle_NoAction( const void *pData ) //for static memory
{
}

#endif // INCLUDED_IRESOURCESYSTEM_H