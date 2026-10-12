//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================
//
//==================================================================================================
#pragma once


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
#include "kv3transfer_resource.h"


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
class CKV3Transfer_ResourceLoadInterface: public IKV3TransferInterface_ResourceLoad
{
public:
	CKV3Transfer_ResourceLoadInterface( IRD_RegisterResourceDataUtils *pDataUtils, ResourceHandle_t hOriginatingResource, ResourceId_t nOriginatingResourceId )
	{
		m_pDataUtils = pDataUtils;
		m_hOriginatingResource = hOriginatingResource;
		m_nOriginatingResourceId = nOriginatingResourceId;
	}

	virtual bool LoadExtReference( CKV3TransferLoadContext *pContext, const KeyValues3 *pLoadFromValue, ResourceType_t nResourceType, CResourceExtReferenceBase *pExtRefBase ) OVERRIDE
	{
		CStrongHandleVoid hResource;
		bool bSuccess = HandleResourceReference( pContext, nResourceType, pLoadFromValue, &hResource );

		if ( hResource != RESOURCE_HANDLE_INVALID )
		{
			ResourceAddRef( hResource, (uintp)(m_nOriginatingResourceId.GetRaw()), RLTG_SCHEMA_REFERENCE );
		}

		pExtRefBase->SetHandle( hResource );

		return bSuccess;
	}

	virtual bool LoadStrongHandle( CKV3TransferLoadContext *pContext, const KeyValues3 *pLoadFromValue, ResourceType_t nResourceType, CStrongHandleBase *pStrongHandleBase ) OVERRIDE
	{
		bool bSuccess = HandleResourceReference( pContext, nResourceType, pLoadFromValue, ( (CStrongHandleVoid*)pStrongHandleBase ) );
		return bSuccess;
	}

	virtual ResourceHandle_t FindExistingResource( ResourceType_t nResourceType, const char *pResourceName, bool bAddReference ) OVERRIDE
	{
		if ( pResourceName == NULL || pResourceName[0] == '\0' )
		{
			return RESOURCE_HANDLE_INVALID;
		}

		ResourceHandle_t hResource = g_pResourceSystem->FindExistingResourceByName( pResourceName, nResourceType );
		AssertMsg( hResource != RESOURCE_HANDLE_INVALID, "Failed to find default-value resource '%s' - must already be resident!", pResourceName );

		if ( bAddReference && hResource != RESOURCE_HANDLE_INVALID )
		{
			ResourceAddRef( hResource, (uintp)(m_nOriginatingResourceId.GetRaw()), RLTG_SCHEMA_REFERENCE );
		}

		return hResource;
	}

	bool HandleResourceReference( CKV3TransferLoadContext *pContext, ResourceType_t nResourceType, const KeyValues3 *pLoadFromValue, CStrongHandleVoid *pOutHandle )
	{
		Assert( g_pResourceSystem );
		const char *pResourceName = pLoadFromValue->GetValueString();
		if ( pResourceName[0] == '\0' )
		{
			*pOutHandle = RESOURCE_HANDLE_INVALID;
			return true;
		}

		if ( !pLoadFromValue->HasFlag( KEYVALUES3_FLAG_RESOURCE_REFERENCE ) )
		{
			pContext->NoteFailure( "Tried to load resource reference '%s' from a value without a resource reference flag.", pResourceName );
			return false;
		}

		char pFixedResourceName[MAX_FILEPATH];
		FixupResourceName( pResourceName, nResourceType, pFixedResourceName, sizeof(pFixedResourceName) );
		if ( pFixedResourceName[0] == '\0' )
		{
			*pOutHandle = RESOURCE_HANDLE_INVALID;
			return true;
		}

		return m_pDataUtils->RegisterResourceExternalReference( m_hOriginatingResource, pFixedResourceName, pOutHandle );
	}

	IRD_RegisterResourceDataUtils *m_pDataUtils;
	ResourceHandle_t m_hOriginatingResource;
	ResourceId_t m_nOriginatingResourceId;
};


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
class CKV3Transfer_EmptyResourceLoadInterface: public IKV3TransferInterface_ResourceLoad
{
public:
	virtual bool LoadExtReference( CKV3TransferLoadContext *pContext, const KeyValues3 *pLoadFromValue, ResourceType_t nResourceType, CResourceExtReferenceBase *pExtRefBase ) OVERRIDE
	{
		ResourceHandle_t hResource = GetHandleForResource( pLoadFromValue, nResourceType );
		pExtRefBase->SetHandle( hResource );

		return true;
	}

	virtual bool LoadStrongHandle( CKV3TransferLoadContext *pContext, const KeyValues3 *pLoadFromValue, ResourceType_t nResourceType, CStrongHandleBase *pStrongHandleBase ) OVERRIDE
	{
		ResourceHandle_t hResource = GetHandleForResource( pLoadFromValue, nResourceType );
		*((CStrongHandleVoid*)pStrongHandleBase) = hResource;

		return true;
	}

	virtual ResourceHandle_t FindExistingResource( ResourceType_t nResourceType, const char *pResourceName, bool bAddReference ) OVERRIDE
	{
		return RESOURCE_HANDLE_INVALID;
	}

	ResourceHandle_t GetHandleForResource( const KeyValues3 *pLoadFromValue, ResourceType_t nResourceType )
	{
		Assert( g_pResourceSystem );

		const char *pResourceName = pLoadFromValue->GetValueString();
		if ( pResourceName[0] == '\0' )
		{
			return RESOURCE_HANDLE_INVALID;
		}

		char pFixedResourceName[MAX_FILEPATH];
		FixupResourceName( pResourceName, nResourceType, pFixedResourceName, sizeof(pFixedResourceName) );

		if ( pFixedResourceName[0] == '\0' )
		{
			return RESOURCE_HANDLE_INVALID;
		}

		// This is creating a placeholder reference so we can get at the name and resource id, but NOT the data itself.
		return g_pResourceSystem->FindOrRegisterResourceByName( pFixedResourceName, nResourceType, false );
	}
};


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
class CKV3TransferInterface_ResourceSave : public IKV3TransferInterface_ResourceSave
{
public:
	virtual bool SaveExtReference( CKV3TransferSaveContext *pContext, KeyValues3 *pSaveToValue, ResourceType_t nResourceType, CResourceExtReferenceBase *pExtRefBase ) OVERRIDE
	{
		AssertMsg( false, "You shouldn't get here" );
		return false;
	}

	virtual bool SaveStrongHandle( CKV3TransferSaveContext *pContext, KeyValues3 *pSaveToValue, ResourceType_t nResourceType, CStrongHandleBase *pStrongHandleBase ) OVERRIDE
	{
		if ( !pStrongHandleBase )
			return false;

		char pResourceName[ MAX_PATH ];
		const ResourceBindingBase_t *pBinding = pStrongHandleBase->GetResourceBindingBase();
		if ( !pBinding )
			return false;

		ResourceGetName( pBinding, pResourceName, sizeof( pResourceName ) );
		pSaveToValue->SetValueString( pResourceName );
		pSaveToValue->SetFlag( KEYVALUES3_FLAG_RESOURCE_REFERENCE );
		return true;
	}
};
