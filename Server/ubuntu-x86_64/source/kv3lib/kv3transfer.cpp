//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================
//
//==================================================================================================

#include "kv3lib/kv3transfer.h"


// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
CKV3TransferBlockAllocator::CKV3TransferBlockAllocator()
{
	m_pBufferBase = NULL;
	m_pNextAlloc = NULL;
	m_nBufferSize = 0;
	m_bExternalAllocation = false;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
CKV3TransferBlockAllocator::~CKV3TransferBlockAllocator()
{
	Free();
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CKV3TransferBlockAllocator::Init( uint nReserveSize )
{
	Free();
	m_pBufferBase = ( byte* )new byte[nReserveSize];
	m_pNextAlloc = m_pBufferBase;
	m_nBufferSize = nReserveSize;
	m_bExternalAllocation = false;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CKV3TransferBlockAllocator::Init( void *pExternalAllocation, uint nSize )
{
	Free();
	m_pBufferBase = ( byte* )pExternalAllocation;
	m_pNextAlloc = m_pBufferBase;
	m_nBufferSize = nSize;
	m_bExternalAllocation = true;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CKV3TransferBlockAllocator::Free()
{
	if ( !m_bExternalAllocation )
	{
		FreeAllocation( m_pBufferBase );
	}
	m_pBufferBase = NULL;
	m_pNextAlloc = NULL;
	m_nBufferSize = 0;
	m_bExternalAllocation = false;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
byte *CKV3TransferBlockAllocator::TakeControlOfAllocaction() // call FreeAllocation when done
{
	byte *pResult = m_pBufferBase;
	m_pBufferBase = NULL;
	m_pNextAlloc = NULL;
	m_nBufferSize = 0;

	return pResult;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CKV3TransferBlockAllocator::FreeAllocation( byte *pAlloc )
{
	delete [] pAlloc;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
byte *CKV3TransferBlockAllocator::AllocBlockBytes( int nSize, uintp nAlign )
{
	if ( nSize == 0 )
		return NULL;

	byte* pResult = (byte*)AlignValue( m_pNextAlloc, nAlign );
	if ( IsDebug() )
	{
		V_memset( pResult, 0xFF, nSize );
	}
	m_pNextAlloc = pResult + nSize;
	Assert( (uintp)m_pNextAlloc <= (uintp)m_pBufferBase + m_nBufferSize );
	return pResult;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
CKV3TransferContextBase::CKV3TransferContextBase()
{
	m_Result = KV3TRANSFER_SUCCESS;

	for ( int i = 0; i < ARRAYSIZE(m_Interfaces); ++i )
	{
		m_Interfaces[i].m_nId = KV3TRANSFER_INTERFACE_ID_INVALID;
		m_Interfaces[i].m_pInterface = NULL;
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void *CKV3TransferContextBase::FindInterfaceVoid( KV3Transfer_InterfaceId_t nId )
{
	Assert( nId != KV3TRANSFER_INTERFACE_ID_INVALID );

	for ( int i = 0; i < ARRAYSIZE( m_Interfaces ); ++i )
	{
		if ( m_Interfaces[i].m_nId == nId )
		{
			return m_Interfaces[i].m_pInterface;
		}
	}

	char interfaceName[5];
	*(KV3Transfer_InterfaceId_t*)&interfaceName = nId;
	interfaceName[4] = '\0';
	AssertMsg2( false, "Failed to find transfer interface %d (%s)", nId, interfaceName );
	return NULL;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CKV3TransferContextBase::AddInterfaceVoid( KV3Transfer_InterfaceId_t nId, void *pInterface )
{
	Assert( nId != KV3TRANSFER_INTERFACE_ID_INVALID );

	for ( int i = 0; i < ARRAYSIZE( m_Interfaces ); ++i )
	{
		if ( m_Interfaces[i].m_nId == KV3TRANSFER_INTERFACE_ID_INVALID )
		{
			// found an empty slot
			m_Interfaces[i].m_nId = nId;
			m_Interfaces[i].m_pInterface = pInterface;
			return;
		}
	}

	char interfaceName[5];
	*(KV3Transfer_InterfaceId_t*)&interfaceName = nId;
	interfaceName[4] = '\0';
	AssertMsg2( false, "Failed to add transfer interface %d (%s) - too many registered interfaces!", nId, interfaceName );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CKV3TransferContextBase::NoteFailure( PRINTF_FORMAT_STRING const char *pFormat, ... )
{
	m_Result = KV3TRANSFER_FAIL;
	va_list marker;

	va_start( marker, pFormat );
	m_ErrorMessage.FormatV( pFormat, marker );
	va_end( marker );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
KV3TransferResult_t CKV3TransferContextBase::GetResult()
{
	return m_Result;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
CKV3TransferLoadContext::CKV3TransferLoadContext()
	: m_pBlockAllocator( NULL )
{
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
CKV3TransferLoadContext::~CKV3TransferLoadContext()
{
	if ( m_pBlockAllocator )
	{
		delete m_pBlockAllocator;
		m_pBlockAllocator = nullptr;
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CKV3TransferLoadContext::SetBlockAllocator( uint nReserveSize )
{
	Assert( m_pBlockAllocator == NULL );
	m_pBlockAllocator = new CKV3TransferBlockAllocator();
	m_pBlockAllocator->Init( nReserveSize );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CKV3TransferLoadContext::SetExternalBlockAllocation( void *pAlloc, uint nReserveSize )
{
	Assert( m_pBlockAllocator == NULL );
	m_pBlockAllocator = new CKV3TransferBlockAllocator();
	m_pBlockAllocator->Init( pAlloc, nReserveSize );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
uint64 CKV3TransferLoadContext::GetExternalBlockAllocationUsage()
{
	if ( !m_pBlockAllocator )
		return 0;

	return m_pBlockAllocator->GetBlockSize();
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CKV3TransferLoadContext::PushSource( const KeyValues3 *pSource )
{
	m_pSourceObject = pSource;
	m_SourceStack.Push( pSource );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CKV3TransferLoadContext::PopSource()
{
	m_SourceStack.Pop();
	if ( m_SourceStack.Count() > 0 )
	{
		m_pSourceObject = m_SourceStack.Top();
	}
	else
	{
		m_pSourceObject = NULL;
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
CKV3TransferSaveContext::CKV3TransferSaveContext()
{
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CKV3TransferSaveContext::PrepareTargetForClass( KeyValues3 *pObjectValue, KV3TransferClassBehavior_t nClassBehavior, const char *pPolymorphicClassName )
{
	if ( nClassBehavior == KV3TRANSFER_CLASS_AS_POLYMORPHIC_TABLE )
	{
		Assert( nClassBehavior == KV3TRANSFER_CLASS_AS_POLYMORPHIC_TABLE );
		Assert( pPolymorphicClassName && pPolymorphicClassName[0] != '\0' );
		pObjectValue->SetToEmptyTable();
		pObjectValue->SetMemberString( KV3TRANSFER_CLASSNAME_MEMBER, pPolymorphicClassName );
		return true;
	}
	if ( nClassBehavior == KV3TRANSFER_CLASS_AS_DATA )
	{
		pObjectValue->SetToNull();
		return true;
	}
	else if ( nClassBehavior == KV3TRANSFER_CLASS_AS_SIMPLE_TABLE )
	{
		pObjectValue->SetToEmptyTable();
		return true;
	}
	else
	{
		Assert( nClassBehavior == KV3TRANSFER_CLASS_UNIMPLEMENTED );
		NoteFailure( "Failed to save unsupported class" );
		pObjectValue->SetToNull();
		return false;
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CKV3TransferSaveContext::PushTarget( KeyValues3 *pTarget )
{
	m_pTargetObject = pTarget;
	m_TargetStack.Push( pTarget );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CKV3TransferSaveContext::PopTarget()
{
	m_TargetStack.Pop();
	if ( m_TargetStack.Count() > 0 )
	{
		m_pTargetObject = m_TargetStack.Top();
	}
	else
	{
		m_pTargetObject = NULL;
	}
}