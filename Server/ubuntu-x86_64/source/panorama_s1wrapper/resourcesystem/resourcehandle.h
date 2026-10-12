#ifndef INCLUDED_RESOURCEHANDLE_H
#define INCLUDED_RESOURCEHANDLE_H
//================ Copyright (c) Valve Corporation. All Rights Reserved. ===========================
//
//
//
//==================================================================================================

#include "resourcefile/resourcetype.h"

class IMaterial2;

enum ResourceDeallocationType_t
{
	RESOURCE_DATA_IS_ERROR = 0,
	RESOURCE_DATA_IS_FALLBACK,
	RESOURCE_DATA_IS_ACTUAL_DATA,
};

enum ResourceLoadType_t
{
	RESOURCE_LOAD_IGNORED, // This case captures when a manifest references an already-loaded resource
	RESOURCE_LOAD_SUCCESS,
	RESOURCE_LOAD_FAIL,
	RESOURCE_LOAD_RELOAD,
};

typedef uint64 ResourceId_t;
#define RESOURCE_ID_INVALID ResourceId_t(0)
typedef uint64 ResourceType_t;

class CResourceManifest;

struct ResourceData_t
{
	ResourceData_t() : m_handle( 0 ), m_nType( RESOURCE_TYPE_NONE ), m_pLoadedInManifest( nullptr ) {}
	ResourceData_t( intptr_t handle, ResourceType_t nType ) : m_handle( handle ), m_nType( nType ), m_pLoadedInManifest( nullptr ) {}

	void AddRef() const
	{
		++m_nRefCount;
	}

	void Release() const
	{
		int nRefCount = --m_nRefCount;
		if ( nRefCount == 0 )
		{
			OnFinalRelease();
		}
	}

	void OnFinalRelease() const;
	
	intptr_t m_handle;
	ResourceType_t m_nType;
	mutable CResourceManifest *m_pLoadedInManifest;		// Only used by the resource system. If not null, resource loaded asynchronously 
														// by the resource system

	mutable CInterlockedInt m_nRefCount;
};


typedef const ResourceData_t *ResourceHandle_t;
#define RESOURCE_HANDLE_INVALID ( (ResourceHandle_t)0 )


class CResourceWeakHandle
{
public:

	// Constructors
	CResourceWeakHandle();
	CResourceWeakHandle( const CResourceWeakHandle &hResource );
	CResourceWeakHandle( ResourceHandle_t hResource );
	~CResourceWeakHandle();

	// Assignment
	CResourceWeakHandle& operator=( const CResourceWeakHandle &src );

	// Comparison operators
	bool operator==( const CResourceWeakHandle &hResource ) const;
	bool operator!=( const CResourceWeakHandle &hResource ) const;

	// Cast operators
	operator ResourceHandle_t() const;

	ResourceHandle_t GetResourceHandle() const;
	void *GetData();

	bool IsValid() const;
	bool IsLoaded() const;
	bool IsError() const;

	// Static methods
	static CResourceWeakHandle StaticInvalidHandle() { return RESOURCE_HANDLE_INVALID; }
	static CResourceWeakHandle FromUntypedHandle( ResourceHandle_t hResource );

private:

	ResourceHandle_t m_hResource;
};


class CResourceStrongHandle
{
public:

	// Constructors
	CResourceStrongHandle();
	CResourceStrongHandle( ResourceHandle_t hResource );
	CResourceStrongHandle( const CResourceWeakHandle &hResource );
	~CResourceStrongHandle();

	// Init / Shutdown
	void Shutdown();

	// Assignment
	CResourceStrongHandle& operator=( const CResourceStrongHandle &src );

	// Comparison operators
	bool operator==( const CResourceStrongHandle &hResource ) const;
	bool operator!=( const CResourceStrongHandle &hResource ) const;
	bool operator!=( const CResourceWeakHandle &hResource ) const;

	// Cast operators
	operator CResourceWeakHandle() const;
	operator ResourceHandle_t() const;
	operator const IMaterial2*( );

	ResourceHandle_t GetResourceHandle() const;

	bool IsValid() const;
	bool IsLoaded() const;
	bool IsError() const;

	// Static methods
	//static ResourceHandle_t StaticInvalidHandle() { return RESOURCE_HANDLE_INVALID; }

private:

	/// We do not want people passing these around as arguments to functions
	/// Doing so would cause huge spammage of the interlocked int refcount
	/// which would cause perf problems. 
	CResourceStrongHandle( const CResourceStrongHandle &src );

	ResourceHandle_t m_hResource;
};


typedef CResourceWeakHandle HMaterial;
typedef CResourceStrongHandle HMaterialStrong;
typedef CResourceWeakHandle HPanoramaLayout;
typedef CResourceWeakHandle HPanoramaScript;
typedef CResourceWeakHandle HPanoramaStyle;
typedef CResourceWeakHandle HVectorGraphic;
typedef CResourceWeakHandle HRenderTexture;
typedef CResourceStrongHandle HRenderTextureStrong;
typedef CResourceWeakHandle SwapChainHandle_t;
typedef CResourceStrongHandle CStrongHandleVoid;

//-----------------------------------------------------------------------------
// CResourceWeakHandle Methods
//-----------------------------------------------------------------------------

FORCEINLINE CResourceWeakHandle::CResourceWeakHandle()
{
	m_hResource = RESOURCE_HANDLE_INVALID;
}

FORCEINLINE CResourceWeakHandle::CResourceWeakHandle( const CResourceWeakHandle &hResource )
{
	m_hResource = hResource.m_hResource;
}

FORCEINLINE CResourceWeakHandle::CResourceWeakHandle( ResourceHandle_t hResource )
{
	m_hResource = hResource;
}

FORCEINLINE CResourceWeakHandle::~CResourceWeakHandle()
{
	m_hResource = RESOURCE_HANDLE_INVALID;
}

FORCEINLINE CResourceWeakHandle& CResourceWeakHandle::operator=( const CResourceWeakHandle &src )
{
	m_hResource = src.m_hResource;
	return *this;
}

FORCEINLINE bool CResourceWeakHandle::operator==( const CResourceWeakHandle &hResource ) const
{
	return ( m_hResource == hResource.m_hResource );
}

FORCEINLINE bool CResourceWeakHandle::operator!=( const CResourceWeakHandle &hResource ) const
{
	return ( m_hResource != hResource.m_hResource );
}

FORCEINLINE CResourceWeakHandle::operator ResourceHandle_t() const
{
	return m_hResource;
}

FORCEINLINE ResourceHandle_t CResourceWeakHandle::GetResourceHandle() const
{
	return m_hResource;
}

FORCEINLINE void *CResourceWeakHandle::GetData()
{
	if ( !IsValid() )
		return nullptr;

	return (void*)m_hResource->m_handle;
}

FORCEINLINE bool CResourceWeakHandle::IsValid() const
{
	return ( ( m_hResource != RESOURCE_HANDLE_INVALID ) && ( m_hResource->m_nType != RESOURCE_TYPE_NONE ) );
}

FORCEINLINE bool CResourceWeakHandle::IsLoaded() const
{
	return ( IsValid() && m_hResource->m_handle );
}

FORCEINLINE bool CResourceWeakHandle::IsError() const
{
	return ( IsValid() && !m_hResource->m_handle );
}

FORCEINLINE CResourceWeakHandle CResourceWeakHandle::FromUntypedHandle( ResourceHandle_t hResource )
{
	CResourceWeakHandle result( hResource );
	return result;
}


//-----------------------------------------------------------------------------
// CResourceStrongHandle Methods
//-----------------------------------------------------------------------------

FORCEINLINE CResourceStrongHandle::CResourceStrongHandle()
{
	m_hResource = RESOURCE_HANDLE_INVALID;
}

FORCEINLINE CResourceStrongHandle::CResourceStrongHandle( ResourceHandle_t hResource )
{
	m_hResource = hResource;
	if ( m_hResource != RESOURCE_HANDLE_INVALID )
	{
		( (ResourceData_t*)m_hResource )->AddRef();
	}
}

FORCEINLINE CResourceStrongHandle::CResourceStrongHandle( const CResourceWeakHandle &hResource )
{
	m_hResource = hResource.GetResourceHandle();
	if ( m_hResource != RESOURCE_HANDLE_INVALID )
	{
		( (ResourceData_t*)m_hResource )->AddRef();
	}
}

FORCEINLINE CResourceStrongHandle::~CResourceStrongHandle()
{
	Shutdown();
}

FORCEINLINE void CResourceStrongHandle::Shutdown()
{
	if ( m_hResource != RESOURCE_HANDLE_INVALID )
	{
		((ResourceData_t*)m_hResource)->Release();
		m_hResource = RESOURCE_HANDLE_INVALID;
	}
}

FORCEINLINE CResourceStrongHandle& CResourceStrongHandle::operator=( const CResourceStrongHandle &src )
{
	Shutdown();

	m_hResource = src.m_hResource;
	if ( m_hResource != RESOURCE_HANDLE_INVALID )
	{
		( (ResourceData_t*)m_hResource )->AddRef();
	}

	return *this;
}

FORCEINLINE bool CResourceStrongHandle::operator==( const CResourceStrongHandle &hResource ) const
{
	return ( m_hResource == hResource.m_hResource );
}

FORCEINLINE bool CResourceStrongHandle::operator!=( const CResourceStrongHandle &hResource ) const
{
	return ( m_hResource != hResource.m_hResource );
}

FORCEINLINE bool CResourceStrongHandle::operator!=( const CResourceWeakHandle &hResource ) const
{
	return ( m_hResource != hResource.GetResourceHandle() );
}

FORCEINLINE CResourceStrongHandle::operator CResourceWeakHandle() const
{
	return CResourceWeakHandle( m_hResource );
}

FORCEINLINE CResourceStrongHandle::operator ResourceHandle_t() const
{
	return m_hResource;
}

FORCEINLINE bool CResourceStrongHandle::IsValid() const
{
	return ( ( m_hResource != RESOURCE_HANDLE_INVALID ) && ( m_hResource->m_nType != RESOURCE_TYPE_NONE ) );
}

FORCEINLINE bool CResourceStrongHandle::IsLoaded() const
{
	return ( IsValid() && m_hResource->m_handle );
}

FORCEINLINE bool CResourceStrongHandle::IsError() const
{
	return ( IsValid() && !m_hResource->m_handle );
}

FORCEINLINE ResourceHandle_t CResourceStrongHandle::GetResourceHandle() const
{
	return m_hResource;
}

#endif // INCLUDED_RESOURCEHANDLE_H