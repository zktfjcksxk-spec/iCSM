//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================
//
//==================================================================================================
#pragma once


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
#include "kv3lib/kv3transfer.h"
#include "resourcefile/resourcestream.h"
#include "resourcefile/resourcetype.h"
#include "resourcesystem/stronghandle.h"

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
#define KV3TRANSFERID_IKV3TransferInterface_ResourceSave MAKE_KV3TRANSFER_INTERFACE_ID( 'R', 'E', 'S', 'S' )

class IKV3TransferInterface_ResourceSave
{
public:
	static KV3Transfer_InterfaceId_t GetInterfaceId() { return KV3TRANSFERID_IKV3TransferInterface_ResourceSave; }

	virtual bool SaveExtReference( CKV3TransferSaveContext *pContext, KeyValues3 *pSaveToValue, ResourceType_t nResourceType, CResourceExtReferenceBase *pExtRefBase ) = 0;
	virtual bool SaveStrongHandle( CKV3TransferSaveContext *pContext, KeyValues3 *pSaveToValue, ResourceType_t nResourceType, CStrongHandleBase *pStrongHandleBase ) = 0;
};


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
#define KV3TRANSFERID_IKV3TransferInterface_ResourceLoad MAKE_KV3TRANSFER_INTERFACE_ID( 'R', 'E', 'S', 'L' )

class IKV3TransferInterface_ResourceLoad
{
public:
	static KV3Transfer_InterfaceId_t GetInterfaceId() { return KV3TRANSFERID_IKV3TransferInterface_ResourceLoad; }

	virtual bool LoadExtReference( CKV3TransferLoadContext *pContext, const KeyValues3 *pLoadFromValue, ResourceType_t nResourceType, CResourceExtReferenceBase *pExtRefBase ) = 0;
	virtual bool LoadStrongHandle( CKV3TransferLoadContext *pContext, const KeyValues3 *pLoadFromValue, ResourceType_t nResourceType, CStrongHandleBase *pStrongHandleBase ) = 0;
	virtual ResourceHandle_t FindExistingResource( ResourceType_t nResourceType, const char *pResourceName, bool bAddReference ) = 0;
};



//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
template< typename TResourceHandleType >
struct CKV3TransferValHelper< CResourceExtReference< TResourceHandleType > >
{
	template< typename U >
	static void SaveValue( CKV3TransferSaveContext *pContext, KeyValues3 *pSaveToValue, const CResourceExtReference< TResourceHandleType > &value )
	{
		IKV3TransferInterface_ResourceSave *pResourceSaveInterface = pContext->FindInterface< IKV3TransferInterface_ResourceSave >();
		Assert( pResourceSaveInterface != NULL );

		pResourceSaveInterface->SaveExtReference( pContext, pSaveToValue, ResourceHandleInfo_t< TResourceHandleType >::RESOURCE_TYPE, (CResourceExtReferenceBase *)&value );
	}

	template< typename U >
	static void LoadValue( CKV3TransferLoadContext *pContext, const KeyValues3 *pLoadFromValue, CResourceExtReference< TResourceHandleType > &value )
	{
		IKV3TransferInterface_ResourceLoad *pResourceLoadInterface = pContext->FindInterface< IKV3TransferInterface_ResourceLoad >();
		Assert( pResourceLoadInterface != NULL );

		pResourceLoadInterface->LoadExtReference( pContext, pLoadFromValue, ResourceHandleInfo_t< TResourceHandleType >::RESOURCE_TYPE, (CResourceExtReferenceBase *)&value );
	}

	template< typename U >
	static void LoadDefault( CKV3TransferLoadContext *pContext, const char *pDefault, CResourceExtReference< TResourceHandleType > &value )
	{
		IKV3TransferInterface_ResourceLoad *pResourceLoadInterface = pContext->FindInterface< IKV3TransferInterface_ResourceLoad >();
		Assert( pResourceLoadInterface != NULL );

		value.SetHandle( pResourceLoadInterface->FindExistingResource( ResourceHandleInfo_t< TResourceHandleType >::RESOURCE_TYPE, pDefault, true ) );
	}
};


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
template< typename T >
struct CKV3TransferValHelper< CStrongHandle< T > >
{
	template< typename U >
	static void SaveValue( CKV3TransferSaveContext *pContext, KeyValues3 *pSaveToValue, const CStrongHandle< T > &value )
	{
		IKV3TransferInterface_ResourceSave *pResourceSaveInterface = pContext->FindInterface< IKV3TransferInterface_ResourceSave >();
		Assert( pResourceSaveInterface != NULL );

		pResourceSaveInterface->SaveStrongHandle( pContext, pSaveToValue, T::RESOURCE_TYPE, (CStrongHandleBase *)&value );
	}

	template< typename U >
	static void LoadValue( CKV3TransferLoadContext *pContext, const KeyValues3 *pLoadFromValue, CStrongHandle< T > &value )
	{
		IKV3TransferInterface_ResourceLoad *pResourceLoadInterface = pContext->FindInterface< IKV3TransferInterface_ResourceLoad >();
		Assert( pResourceLoadInterface != NULL );

		pResourceLoadInterface->LoadStrongHandle( pContext, pLoadFromValue, T::RESOURCE_TYPE, (CStrongHandleBase *)&value );
	}

	template< typename U >
	static void LoadDefault( CKV3TransferLoadContext *pContext, const char *pDefault, CStrongHandle< T > &value )
	{
		IKV3TransferInterface_ResourceLoad *pResourceLoadInterface = pContext->FindInterface< IKV3TransferInterface_ResourceLoad >();
		Assert( pResourceLoadInterface != NULL );

		value = CWeakHandle< T >::FromUntypedHandleUnchecked( pResourceLoadInterface->FindExistingResource( T::RESOURCE_TYPE, pDefault, false ) );
	}
};


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
inline void TransferValueIntoResourceString( CKV3TransferBlockAllocator *pBlockAllocator, CResourceString &value, const char *pString )
{
	Assert( pBlockAllocator != NULL );
	int nLength = pString ? V_strlen( pString ) : 0;
	if ( nLength == 0 )
	{
		value.SetNull();
	}
	else
	{
		char *pMem = pBlockAllocator->Alloc<char>( nLength + 1 );
		memcpy( pMem, pString, nLength + 1 );
		((char*)pMem)[nLength] = '\0'; // ensure null terminated
		value.SetRawPtr( pMem );
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
template<>
struct CKV3TransferValHelper< CResourceString >
{
	template< typename U >
	static void SaveValue( CKV3TransferSaveContext *pContext, KeyValues3 *pSaveToValue, const CResourceString &value )
	{
		pSaveToValue->SetValueString( value.GetPtr() );
	}

	template< typename U >
	static void LoadValue( CKV3TransferLoadContext *pContext, const KeyValues3 *pLoadFromValue, CResourceString &value )
	{
		const char *pString = pLoadFromValue->GetValueString();
		TransferValueIntoResourceString( pContext->GetBlockAllocator(), value, pString );
	}

	template< typename U >
	static void LoadDefault( CKV3TransferLoadContext *pContext, const char *pDefault, CResourceString &value )
	{
		TransferValueIntoResourceString( pContext->GetBlockAllocator(), value, pDefault );
	}
};


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
template< typename T >
struct CKV3TransferValHelper< CResourceArray< T > >
{
	template< typename U >
	static void SaveValue( CKV3TransferSaveContext *pContext, KeyValues3 *pSaveToValue, const CResourceArray< T > &value )
	{
		int nCount = value.Count();

		pSaveToValue->SetArrayElementCount( nCount );

		for ( int i = 0; i < nCount; ++i )
		{
			pContext->SaveValueDirect( value[ i ], pSaveToValue->GetArrayElement( i ) );
		}
	}

	template< typename U >
	static void LoadValue( CKV3TransferLoadContext *pContext, const KeyValues3 *pLoadFromValue, CResourceArray< T > &value )
	{
		Assert( pContext->GetBlockAllocator() != NULL );

		int nCount = pLoadFromValue->GetArrayElementCount();

		T *pElements = (T*)pContext->GetBlockAllocator()->Alloc<T>( nCount );
		value.WriteDirect( nCount, pElements );

		for ( int i = 0; i < nCount; ++i )
		{
			pContext->LoadValueDirect( value[ i ], pLoadFromValue->GetArrayElement( i ) );
		}
	}
};


//--------------------------------------------------------------------------------------------------
// Resource array of byte is treated specially as a binary blob, similar to byte[]
//--------------------------------------------------------------------------------------------------
template<>
struct CKV3TransferValHelper< CResourceArray< byte > >
{
	template< typename U >
	static void SaveValue( CKV3TransferSaveContext *pContext, KeyValues3 *pSaveToValue, const CResourceArray< byte > &value )
	{
		pSaveToValue->SetToBinaryBlob( value.Base(), value.Count() );
	}

	template< typename U >
	static void LoadValue( CKV3TransferLoadContext *pContext, const KeyValues3 *pLoadFromValue, CResourceArray< byte > &value )
	{
		Assert( pContext->GetBlockAllocator() != NULL );

		if ( pLoadFromValue->GetType() == KEYVALUES3_TYPE_BINARY_BLOB )
		{
			int nCount = pLoadFromValue->GetBinaryBlobSize();
			byte *pElements = pContext->GetBlockAllocator()->AllocBlockBytes( nCount, 1 );
			value.WriteDirect( nCount, pElements );

			V_memcpy( pElements, pLoadFromValue->GetBinaryBlobBase(), nCount );
		}
		else
		{
			int nCount = pLoadFromValue->GetArrayElementCount();
			byte *pElements = pContext->GetBlockAllocator()->AllocBlockBytes( nCount, 1 );
			value.WriteDirect( nCount, pElements );

			for ( int i = 0; i < nCount; ++i )
			{
				pContext->LoadValueDirect( value[ i ], pLoadFromValue->GetArrayElement( i ) );
			}
		}
	}
};


//--------------------------------------------------------------------------------------------------
// Resource Pointer (owning pointer; cycles and diamonds aren't supported)
//--------------------------------------------------------------------------------------------------
template< typename T >
struct CKV3TransferValHelper< CResourcePointer< T > >
{
	template< typename U >
	static void SaveValue( CKV3TransferSaveContext *pContext, KeyValues3 *pSaveToValue, const CResourcePointer< T > &value )
	{
		const T *pObj = value.GetPtr();
		pContext->SaveClassPointer( pObj, pSaveToValue );
	}

	template< typename U >
	static void LoadValue( CKV3TransferLoadContext *pContext, const KeyValues3 *pLoadFromValue, CResourcePointer< T > &value )
	{
		T *pObj = NULL;
		pContext->LoadOwningPointer( pObj, pLoadFromValue );
		value.SetRawPtr( pObj );
	}
};

