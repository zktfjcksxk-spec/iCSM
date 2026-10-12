//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================
//
//==================================================================================================

#include "kv3lib/keyvalues3.h"

#include "tier1/utlstring.h"
#include "tier1/utlbuffer.h"
#include "tier1/utlvector.h"
#include "kv3lib/utltokenizer.h"
#include "bitvec.h"
#include "filesystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
const double KEYVALUES3_IDENTICAL_EPSILON = 0.0000001f;


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
enum KeyValues3InternalType_t: uint8
{
	// The following must match the public type enum
	KEYVALUES3_INTERNAL_TYPE_INVALID,
	KEYVALUES3_INTERNAL_TYPE_NULL,
	KEYVALUES3_INTERNAL_TYPE_BOOL,
	KEYVALUES3_INTERNAL_TYPE_INT64,
	KEYVALUES3_INTERNAL_TYPE_UINT64,
	KEYVALUES3_INTERNAL_TYPE_DOUBLE,
	KEYVALUES3_INTERNAL_TYPE_STRING,
	KEYVALUES3_INTERNAL_TYPE_BINARY_BLOB,
	KEYVALUES3_INTERNAL_TYPE_ARRAY,
	KEYVALUES3_INTERNAL_TYPE_TABLE,

	KEYVALUES3_INTERNAL_TYPE_SHORT_STRING,		// string is short enough to be stored in the value (m_AsShortString)
	KEYVALUES3_INTERNAL_TYPE_EXTERNAL_STRING,	// char* to externally owned memory
	KEYVALUES3_INTERNAL_TYPE_EXTERNAL_BLOB,		// m_pBinaryBlobExternal
	KEYVALUES3_INTERNAL_TYPE_MAX,
};

COMPILE_TIME_ASSERT( (uint8)KEYVALUES3_TYPE_INVALID			== (uint8)KEYVALUES3_INTERNAL_TYPE_INVALID		);
COMPILE_TIME_ASSERT( (uint8)KEYVALUES3_TYPE_NULL			== (uint8)KEYVALUES3_INTERNAL_TYPE_NULL			);
COMPILE_TIME_ASSERT( (uint8)KEYVALUES3_TYPE_BOOL			== (uint8)KEYVALUES3_INTERNAL_TYPE_BOOL			);
COMPILE_TIME_ASSERT( (uint8)KEYVALUES3_TYPE_INT64			== (uint8)KEYVALUES3_INTERNAL_TYPE_INT64		);
COMPILE_TIME_ASSERT( (uint8)KEYVALUES3_TYPE_UINT64			== (uint8)KEYVALUES3_INTERNAL_TYPE_UINT64		);
COMPILE_TIME_ASSERT( (uint8)KEYVALUES3_TYPE_DOUBLE			== (uint8)KEYVALUES3_INTERNAL_TYPE_DOUBLE		);
COMPILE_TIME_ASSERT( (uint8)KEYVALUES3_TYPE_STRING			== (uint8)KEYVALUES3_INTERNAL_TYPE_STRING		);
COMPILE_TIME_ASSERT( (uint8)KEYVALUES3_TYPE_BINARY_BLOB		== (uint8)KEYVALUES3_INTERNAL_TYPE_BINARY_BLOB	);
COMPILE_TIME_ASSERT( (uint8)KEYVALUES3_TYPE_ARRAY			== (uint8)KEYVALUES3_INTERNAL_TYPE_ARRAY		);
COMPILE_TIME_ASSERT( (uint8)KEYVALUES3_TYPE_TABLE			== (uint8)KEYVALUES3_INTERNAL_TYPE_TABLE		);

COMPILE_TIME_ASSERT( ( 1<<KEYVALUES3_BITS_FOR_TYPE ) >= KEYVALUES3_INTERNAL_TYPE_MAX );


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
class CKeyValues3Metadata
{
public:
	CKeyValues3Metadata()
		: m_nLineNumber( -1 )
		, m_File( UTL_INVAL_SYMBOL_LARGE ) {}

	int m_nLineNumber;
	CUtlSymbolLarge m_File;
};


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
class CKeyValues3MetadataCluster
{
public:
	CKeyValues3Metadata m_Metadata[ KV3_CLUSTER_VALUE_COUNT ];
};


//--------------------------------------------------------------------------------------------------
// CKeyValues3Cluster - implementation detail for pooled KV3s (use CKeyValues3Context )
//--------------------------------------------------------------------------------------------------
class ALIGN_N( KV3_CLUSTER_ALIGNOF ) CKeyValues3Cluster: public CAlignedNewDeleteNoBase< KV3_CLUSTER_SIZEOF >
{
public:
	CKeyValues3Cluster();
	~CKeyValues3Cluster();

	static CKeyValues3Cluster *ClusterFromPointer( const KeyValues3 *pKV3 );

	void Init( CKeyValues3Context *pContext );
	int AllocationCount();
	inline bool IsFull() const { return m_nAllocatedFlags == KV3_CLUSTER_FULL; }
	inline bool IsEmpty() const { return m_nAllocatedFlags == 0; }
	KeyValues3 *Allocate();
	void Free( KeyValues3 *pKV3 );

	KeyValues3 *Get( int nIndex );

	CKeyValues3Context *GetParentContext() { Assert(m_pContext); return m_pContext; }

	int GetIndexForKV3( const KeyValues3 *pKV3 );
	CKeyValues3Metadata *GetMetadataForKV3( const KeyValues3 *pKV3 );

	void Dump();

	CKeyValues3Cluster *GetNextFreeCluster() { return m_pNextFreeCluster; }
	void SetNextFreeCluster( CKeyValues3Cluster *pCluster ) { m_pNextFreeCluster = pCluster; }

	void SetMetadataEnabled( bool bMetadata );

private:
	// cluster metadata
	CKeyValues3Context *m_pContext;
	CKeyValues3Cluster *m_pNextFreeCluster;
	CKeyValues3MetadataCluster *m_pMetadata;
	uint64 m_nAllocatedFlags; // some unused high bits, always zero

	// Values
	KeyValues3 m_Values[ KV3_CLUSTER_VALUE_COUNT ];
}
ALIGN_N_POST( KV3_CLUSTER_ALIGNOF );


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
struct KeyValues3ContextImpl_t
{
	KeyValues3ContextImpl_t()
		: m_EmbeddedCluster()
		, m_AdditionalClusters()
		, m_pFreeClusterList( nullptr )
		, m_Symbols()
		, m_nOutstandingKV3Allocations( 0 )
	{
	}

	CKeyValues3Cluster m_EmbeddedCluster;
	CUtlVectorFixedGrowable< CKeyValues3Cluster*, 8 > m_AdditionalClusters;
	CKeyValues3Cluster *m_pFreeClusterList; // linked list of clusters that have free space
	CUtlSymbolTableLarge m_Symbols;
	int m_nOutstandingKV3Allocations;
	bool m_bMetadataEnabled : 1;
};
COMPILE_TIME_ASSERT( sizeof( KeyValues3ContextImpl_t ) == sizeof( CKeyValues3Context ) );


//--------------------------------------------------------------------------------------------------
// (Implementation Detail)
//--------------------------------------------------------------------------------------------------
class CKeyValues3Table
{
public:
	CKeyValues3Table();
	~CKeyValues3Table();

	void Init( KeyValues3 *pParent );
	void Free( KeyValues3 *pParent );

	int FindMember_Internal( CKV3MemberName memberName ) const;

	int FindOrCreateMember_Internal( KeyValues3 *pParent, CKV3MemberName memberName, bool bUseExternalStorageForName, bool *pOutCreated );

	KeyValues3 *MemberData( int nIndex );
	const KeyValues3 *MemberData( int nIndex ) const;
	const char *MemberName( int nIndex ) const;
	KeyValues3LowercaseHash_t MemberNameHash( int nIndex ) const;
	CKV3MemberName KV3MemberName( int nIndex ) const;

	void Clear( KeyValues3 *pParent ); // removes all Table

	void CopyFrom( KeyValues3 *pParent, const CKeyValues3Table *pOther );
	void CopyMatchingKeysFrom( const CKeyValues3Table *pOther );

	bool IsIdenticalTo( const CKeyValues3Table *pOther, bool bAssertOnFailure ) const;

	int Count() const { return m_MemberNameLowerHash.Count(); }
	void Remove( KeyValues3 *pParent, int nIndex );
	bool Remove( KeyValues3 *pParent, KeyValues3 *pMemberData );

private:
	enum MemberFlags_t
	{
		MEMBER_FLAG_EXTERNAL_NAME = 1<<0,
	};

	enum { KV3_TABLE_EMBEDDED_MEMBER_STORAGE_COUNT = 8 };
	CUtlVectorFixedGrowable< KeyValues3LowercaseHash_t, KV3_TABLE_EMBEDDED_MEMBER_STORAGE_COUNT > m_MemberNameLowerHash;
	CUtlVectorFixedGrowable< const char *, KV3_TABLE_EMBEDDED_MEMBER_STORAGE_COUNT > m_MemberName;
	CUtlVectorFixedGrowable< KeyValues3*, KV3_TABLE_EMBEDDED_MEMBER_STORAGE_COUNT > m_MemberData;
	CUtlVectorFixedGrowable< uint8, KV3_TABLE_EMBEDDED_MEMBER_STORAGE_COUNT > m_MemberFlags;
};


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
#define EMBEDDED_CLUSTER_INDEX_OF_ROOT 0

COMPILE_TIME_ASSERT( sizeof( CKeyValues3Cluster ) == KV3_CLUSTER_SIZEOF );
COMPILE_TIME_ASSERT( VALIGNOF_PORTABLE( CKeyValues3Cluster ) == KV3_CLUSTER_ALIGNOF );
COMPILE_TIME_ASSERT( KV3_CLUSTER_SIZEOF == KV3_CLUSTER_ALIGNOF );

COMPILE_TIME_ASSERT( VALIGNOF_PORTABLE( CKeyValues3Context ) >= KV3_CLUSTER_ALIGNOF );

#define KV3_CLUSTER_VALUE_TO_CLUSTER_POINTER_MASK 0x3FF
COMPILE_TIME_ASSERT( KV3_CLUSTER_VALUE_TO_CLUSTER_POINTER_MASK == (KV3_CLUSTER_ALIGNOF-1) );


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
CKeyValues3Cluster *CKeyValues3Cluster::ClusterFromPointer( const KeyValues3 *pKV3 )
{
	if ( pKV3->m_bContextIndependent )
	{
		return NULL;
	}
	else
	{
		// assumes pKV3 points somewhere into CKeyValues3Cluster::m_Values
		return reinterpret_cast< CKeyValues3Cluster* >( ( (intp)pKV3 ) & (~KV3_CLUSTER_VALUE_TO_CLUSTER_POINTER_MASK) );
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
CKeyValues3Cluster::CKeyValues3Cluster()
{
	if ( ( (intp)this & KV3_CLUSTER_VALUE_TO_CLUSTER_POINTER_MASK ) != 0 )
	{
		Plat_FatalError( "CKeyValues3Cluster allocated with bad alignment! (%p) (If this is inside of a heap allocation, you may need to inherit from CAlignedNewDelete< KV3_CLUSTER_SIZEOF >.)\n", this );
	}
	V_memset( this, 0, sizeof(*this) );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
CKeyValues3Cluster::~CKeyValues3Cluster()
{
	delete m_pMetadata;
	m_pMetadata = nullptr;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CKeyValues3Cluster::Init( CKeyValues3Context *pContext )
{
	m_pContext = pContext;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
KeyValues3 *CKeyValues3Cluster::Get( int nIndex )
{
	AssertDbg( nIndex >= 0 && nIndex < KV3_CLUSTER_VALUE_COUNT );
	AssertDbg( ( m_nAllocatedFlags & ( uint64(1) << nIndex ) ) != 0 );
	AssertDbg( (m_nAllocatedFlags & 0x8000000000000000) == 0 );
	return &m_Values[ nIndex ];
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
int CKeyValues3Cluster::GetIndexForKV3( const KeyValues3 *pKV3 )
{
	if ( !Verify( pKV3 >= m_Values ) )
		return -1;

	size_t nIndex = ( pKV3 - m_Values );
	if ( !Verify( nIndex < KV3_CLUSTER_VALUE_COUNT ) )
		return -1;

	return (int)nIndex;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
CKeyValues3Metadata *CKeyValues3Cluster::GetMetadataForKV3( const KeyValues3 *pKV3 )
{
	int nIndex = GetIndexForKV3( pKV3 );
	if ( !Verify( nIndex != -1 ) )
		return nullptr;

	if ( !m_pMetadata )
		return nullptr;

	return &( m_pMetadata->m_Metadata[ nIndex ] );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CKeyValues3Cluster::Dump()
{
	Msg( " - %d / %d allocated\n", AllocationCount(), (int)KV3_CLUSTER_VALUE_COUNT );
	for ( int i = 0; i < KV3_CLUSTER_VALUE_COUNT; ++i )
	{
		uint64 nMask = uint64(1) << i;
		if ( ( m_nAllocatedFlags & nMask ) != 0 )
		{
			CUtlString s;
			m_Values[ i ].GetValueAsString( &s );
			Msg( " - Cluster[%d]@%8p = '%s' (type %d)\n", i, &m_Values[i], s.Get(), (int)m_Values[i].GetType() );
		}
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CKeyValues3Cluster::SetMetadataEnabled( bool bMetadata )
{
	if ( bMetadata )
	{
		if ( m_pMetadata )
			return;

		m_pMetadata = new CKeyValues3MetadataCluster;
	}
	else
	{
		delete m_pMetadata;
		m_pMetadata = nullptr;
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
int CKeyValues3Cluster::AllocationCount()
{
	return PopulationCount( m_nAllocatedFlags );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
inline static int IndexOfLeastSignificantZeroBit( uint64 nValue )
{
	uint64 nInverted = ~nValue;

	uint32 nInvertedLo = uint32( nInverted );
	int nFirstBit = FirstBitInWord( nInvertedLo, 0 );
	if ( nFirstBit >= 0 )
		return nFirstBit;

	uint32 nInvertedHi = uint32( nInverted >> 32 );
	return FirstBitInWord( nInvertedHi, 32 );
}


//--------------------------------------------------------------------------------------------------
// New is #defined for debugging.  However, placement new needs to call
// new directly, so we avoid that #define here by including memdbgoff.h
// temporarily.
//--------------------------------------------------------------------------------------------------
#include <tier0/memdbgoff.h>
KeyValues3 *CKeyValues3Cluster::Allocate()
{
	int nIndexOfZeroBit = IndexOfLeastSignificantZeroBit( m_nAllocatedFlags );
	if ( nIndexOfZeroBit == -1 )
	{
		AssertMsg( false, "Called Allocate() on a full CKeyValues3Cluster" );
		return NULL;
	}

	uint64 nMask = uint64( 1 ) << nIndexOfZeroBit;
	AssertDbg( ( m_nAllocatedFlags & nMask ) == 0 );
	AssertDbg( ( m_nAllocatedFlags & 0x8000000000000000 ) == 0 );
	m_nAllocatedFlags |= nMask;
	KeyValues3 *pKV = &m_Values[nIndexOfZeroBit];
	::new( pKV ) KeyValues3( m_pContext ); // not using Construct() because this is a private constructor
	return pKV;
}
#include <tier0/memdbgon.h>	// re-enable new/delete tracking

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CKeyValues3Cluster::Free( KeyValues3 *pKV3 )
{
	bool bInCluster = ( &m_Values[0] <= pKV3 && pKV3 <= &m_Values[ KV3_CLUSTER_VALUE_COUNT - 1 ] );
	AssertDbg( bInCluster );
	if ( !bInCluster )
		return;

	intp nIndex = ( intp(pKV3) - intp(m_Values) ) / sizeof(KeyValues3);
	AssertDbg( &m_Values[nIndex] == pKV3 );
	uint64 nMask = uint64(1) << nIndex;
	AssertDbg( ( m_nAllocatedFlags & nMask ) != 0 );

	Destruct( pKV3 );
	V_memset( pKV3, 0, sizeof(KeyValues3) );

	m_nAllocatedFlags &= ~nMask; // clear allocation
	Assert( (m_nAllocatedFlags & 0x8000000000000000) == 0 );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
struct KeyValues3Array_t
{
	KeyValues3 **AsKVArray() { return (KeyValues3 **)( (byte*)this + sizeof(m_nCount) ); }
	intp m_nCount;
	// KeyValues3*[m_nCount]
};


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
struct KeyValues3BinaryBlob_t
{
	const byte *AsByteArray() const { return (byte*)( (byte*)this + sizeof(m_nCount) ); }
	byte *AsByteArray() { return (byte*)( (byte*)this + sizeof(m_nCount) ); }
	intp m_nCount;
	// byte[m_nCount]
};


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
struct KeyValues3BinaryBlobExternal_t
{
	const byte *AsByteArray() const { return m_pData; }
	intp m_nCount;
	const byte *m_pData;
};


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
CKeyValues3Iterator::CKeyValues3Iterator()
{
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
CKeyValues3Iterator::CKeyValues3Iterator( KeyValues3 *pKV3 )
{
	Init( pKV3 );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CKeyValues3Iterator::Init( KeyValues3 *pKV3 )
{
	m_Stack.RemoveAll();
	if ( pKV3 != NULL )
	{
		StackEntry_t &e = m_Stack[ m_Stack.AddToTail() ];
		e.m_nIndex = -1;
		e.m_pKV = pKV3;
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CKeyValues3Iterator::IsValid() const
{
	return ( m_Stack.Count() != 0 );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CKeyValues3Iterator::Advance()
{
	Assert( IsValid() );

	while ( true )
	{
		if ( m_Stack.Count() == 0 )
		{
			// all done!
			return;
		}

		StackEntry_t &e = m_Stack.Tail();
		KeyValues3 *pCurrent = e.m_pKV;
		if ( pCurrent->GetType() == KEYVALUES3_TYPE_ARRAY )
		{
			// iterating an array
			e.m_nIndex++;

			if ( e.m_nIndex < pCurrent->GetArrayElementCount() )
			{
				// push the array element
				StackEntry_t &e2 = m_Stack[ m_Stack.AddToTail() ];
				e2.m_nIndex = -1;
				e2.m_pKV = pCurrent->GetArrayElement( e.m_nIndex );
				return;
			}
			else
			{
				// done with this array
				m_Stack.RemoveMultipleFromTail( 1 );
			}
		}
		else if ( pCurrent->GetType() == KEYVALUES3_TYPE_TABLE )
		{
			// iterating a table
			e.m_nIndex++;

			if ( e.m_nIndex < pCurrent->GetMemberCount() )
			{
				// push the table member
				StackEntry_t &e2 = m_Stack[ m_Stack.AddToTail() ];
				e2.m_nIndex = -1;
				e2.m_pKV = pCurrent->GetMember( e.m_nIndex );
				return;
			}
			else
			{
				// done with this array
				m_Stack.RemoveMultipleFromTail( 1 );
			}
		}
		else
		{
			// done with this guy
			m_Stack.RemoveMultipleFromTail( 1 );
		}
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
KeyValues3 *CKeyValues3Iterator::Get() const
{
	if ( m_Stack.Count() == 0 )
		return NULL;

	return m_Stack.Tail().m_pKV;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
CKeyValues3Table *KeyValues3::GetTable()
{
	if ( GetType() == KEYVALUES3_TYPE_TABLE )
	{
		return m_pTable;
	}
	else
	{
		return NULL;
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
const CKeyValues3Table *KeyValues3::GetTable() const
{
	if ( GetType() == KEYVALUES3_TYPE_TABLE )
	{
		return m_pTable;
	}
	else
	{
		return NULL;
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void KeyValues3::SetToEmptyTable()
{
	Internal_PrepareForType( KEYVALUES3_TYPE_TABLE );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
int KeyValues3::GetMemberInt( CKV3MemberName memberName, int defaultValue ) const
{
	const KeyValues3 *pMember = Internal_FindMember( memberName );
	if ( !pMember )
		return defaultValue;

	return pMember->GetValueInt();
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool KeyValues3::GetMemberBool( CKV3MemberName memberName, bool defaultValue ) const
{
	const KeyValues3 *pMember = Internal_FindMember( memberName );
	if ( !pMember )
		return defaultValue;

	return pMember->GetValueBool();
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
uint64 KeyValues3::GetMemberUint64( CKV3MemberName memberName, uint64 defaultValue ) const
{
	const KeyValues3 *pMember = Internal_FindMember( memberName );
	if ( !pMember )
		return defaultValue;

	return pMember->GetValueUint64();
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
int64 KeyValues3::GetMemberInt64( CKV3MemberName memberName, int64 defaultValue ) const
{
	const KeyValues3 *pMember = Internal_FindMember( memberName );
	if ( !pMember )
		return defaultValue;

	return pMember->GetValueInt64();
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
float KeyValues3::GetMemberFloat( CKV3MemberName memberName, float defaultValue ) const
{
	const KeyValues3 *pMember = Internal_FindMember( memberName );
	if ( !pMember )
		return defaultValue;

	return pMember->GetValueFloat();
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
double KeyValues3::GetMemberDouble( CKV3MemberName memberName, double defaultValue ) const
{
	const KeyValues3 *pMember = Internal_FindMember( memberName );
	if ( !pMember )
		return defaultValue;

	return pMember->GetValueDouble();
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void KeyValues3::GetMemberAsString( CKV3MemberName memberName, char *pOutData, int nBufSize, const char *defaultValue ) const
{
	const KeyValues3 *pMember = Internal_FindMember( memberName );
	if ( !pMember )
	{
		V_strncpy( pOutData, defaultValue, nBufSize );
		return;
	}

	pMember->GetValueAsString( pOutData, nBufSize );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void KeyValues3::GetMemberAsString( CKV3MemberName memberName, CUtlString *pOutData, const char *defaultValue ) const
{
	const KeyValues3 *pMember = Internal_FindMember( memberName );
	if ( !pMember )
	{
		*pOutData = defaultValue;
		return;
	}

	pMember->GetValueAsString( pOutData );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
const char *KeyValues3::GetMemberString( CKV3MemberName memberName, const char *defaultValue ) const
{
	const KeyValues3 *pMember = Internal_FindMember( memberName );
	if ( !pMember || pMember->GetType() != KEYVALUES3_TYPE_STRING )
	{
		return defaultValue;
	}

	return pMember->GetValueString();
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
const byte *KeyValues3::GetBinaryBlobBase() const
{
	if ( GetType() != KEYVALUES3_TYPE_BINARY_BLOB )
	{
		AssertMsg( false, "Can't call GetBinaryBlobBase() on a non-blob." );
		return NULL;
	}

	return Internal_BinaryBlobBase();
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
const byte *KeyValues3::Internal_BinaryBlobBase() const
{
	if ( m_InternalType == KEYVALUES3_INTERNAL_TYPE_EXTERNAL_BLOB )
	{
		return m_pBinaryBlobExternal ? m_pBinaryBlobExternal->AsByteArray() : NULL;
	}
	else
	{
		return m_pBinaryBlob ? m_pBinaryBlob->AsByteArray() : NULL;
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
int KeyValues3::Internal_BinaryBlobSize() const
{
	if ( m_InternalType == KEYVALUES3_INTERNAL_TYPE_EXTERNAL_BLOB )
	{
		return m_pBinaryBlobExternal ? (int)m_pBinaryBlobExternal->m_nCount : 0;
	}
	else
	{
		return m_pBinaryBlob ? (int)m_pBinaryBlob->m_nCount : 0;
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
int KeyValues3::GetBinaryBlobSize() const
{
	if ( GetType() != KEYVALUES3_TYPE_BINARY_BLOB )
	{
		AssertMsg( false, "Can't call GetBinaryBlobLength() on a non-blob." );
		return 0;
	}

	return Internal_BinaryBlobSize();
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
byte KeyValues3::GetBinaryBlobByte( int nIndex ) const
{
	if ( GetType() != KEYVALUES3_TYPE_BINARY_BLOB )
	{
		AssertMsg( false, "Can't call GetBinaryBlobByte() on a non-blob." );
		return 0;
	}

	int nCount = Internal_BinaryBlobSize();
	if ( nIndex >= nCount )
	{
		AssertMsg2( false, "Bad index for GetBinaryBlobByte() (%d >= %d)", nIndex, nCount );
		return 0;
	}

	return Internal_BinaryBlobBase()[ nIndex ];
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void KeyValues3::Internal_AimAtExternalBinaryBlob( const byte *pData, int nSize )
{
	Internal_PrepareForInternalType( KEYVALUES3_INTERNAL_TYPE_EXTERNAL_BLOB );
	m_pBinaryBlobExternal = CKeyValues3Context::AllocBlobExternal( this, pData, nSize );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void KeyValues3::SetToBinaryBlob( const byte *pData, int nSize )
{
	Internal_PrepareForType( KEYVALUES3_TYPE_BINARY_BLOB );
	m_pBinaryBlob = CKeyValues3Context::AllocBlob( this, pData, nSize );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void KeyValues3::SetToZeroedBinaryBlob( int nSize )
{
	Internal_PrepareForType( KEYVALUES3_TYPE_BINARY_BLOB );
	m_pBinaryBlob = CKeyValues3Context::AllocZeroedBlob( this, nSize );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void KeyValues3::SetToEmptyArray()
{
	Internal_PrepareForType( KEYVALUES3_TYPE_ARRAY );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
int KeyValues3::GetArrayElementCount() const
{
	if ( GetType() == KEYVALUES3_TYPE_ARRAY )
	{
		if ( m_pArray )
		{
			return (int)m_pArray->m_nCount;
		}
		else
		{
			return 0;
		}
	}
	else
	{
		return 0;
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void KeyValues3::SetArrayElementCount( int nCount )
{
	if ( GetType() != KEYVALUES3_TYPE_ARRAY )
	{
		Internal_PrepareForType( KEYVALUES3_TYPE_ARRAY );
	}
	Internal_SetArrayCount( nCount );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void KeyValues3::Internal_SetArrayCount( int nCount )
{
	AssertDbg( GetType() == KEYVALUES3_TYPE_ARRAY );

	if ( m_pArray && m_pArray->m_nCount == nCount )
		return;

	m_pArray = CKeyValues3Context::ReallocArray( this, m_pArray, nCount );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
KeyValues3 *KeyValues3::GetArrayElement( int nIndex )
{
	if ( GetType() == KEYVALUES3_TYPE_ARRAY )
	{
		if ( m_pArray && ( nIndex >= 0 && nIndex < m_pArray->m_nCount ) )
		{
			return m_pArray->AsKVArray()[nIndex];
		}
		else
		{
			AssertMsg2( false, "KeyValues3::GetElement - Bad index %d (count = %d)", nIndex, int( m_pArray ? m_pArray->m_nCount : 0 ) );
			return NULL;
		}
	}
	else
	{
		return NULL;
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
const KeyValues3 *KeyValues3::GetArrayElement( int nIndex ) const
{
	return const_cast<KeyValues3*>(this)->GetArrayElement( nIndex );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool KeyValues3::GetValueBool() const
{
	if ( GetType() != KEYVALUES3_TYPE_BOOL )
	{
		return ( GetValueAsNumeric<int>() != 0 );
	}
	return m_bAsBool;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
int KeyValues3::GetValueInt() const
{
	return GetValueAsNumeric<int>();
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
int64 KeyValues3::GetValueInt64() const
{
	return GetValueAsNumeric<int64>();
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
uint64 KeyValues3::GetValueUint64() const
{
	return GetValueAsNumeric<uint64>();
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
float KeyValues3::GetValueFloat() const
{
	return GetValueAsNumeric<float>();
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
double KeyValues3::GetValueDouble() const
{
	return GetValueAsNumeric<double>();
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool KeyValues3::IsNull() const
{
	return ( GetType() == KEYVALUES3_TYPE_NULL );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void KeyValues3::GetValueAsString( char *pOutData, int nBufSize ) const
{
	if ( m_InternalType == KEYVALUES3_INTERNAL_TYPE_BOOL )
	{
		V_snprintf( pOutData, nBufSize, "%s", m_bAsBool ? "true" : "false" );
	}
	else if ( m_InternalType == KEYVALUES3_INTERNAL_TYPE_INT64 )
	{
		V_snprintf( pOutData, nBufSize, "%lld", m_nAsInt64 );
	}
	else if ( m_InternalType == KEYVALUES3_INTERNAL_TYPE_UINT64 )
	{
		V_snprintf( pOutData, nBufSize, "%llu", m_nAsUint64 );
	}
	else if ( m_InternalType == KEYVALUES3_INTERNAL_TYPE_DOUBLE )
	{
		V_snprintf( pOutData, nBufSize, "%f", m_flAsDouble );
	}
	else if ( ( m_InternalType == KEYVALUES3_INTERNAL_TYPE_STRING ) || ( m_InternalType == KEYVALUES3_INTERNAL_TYPE_SHORT_STRING ) || ( m_InternalType == KEYVALUES3_INTERNAL_TYPE_EXTERNAL_STRING ) )
	{
		V_strncpy( pOutData, GetValueString(), nBufSize );
	}
	else if ( m_InternalType == KEYVALUES3_INTERNAL_TYPE_BINARY_BLOB )
	{
		V_strncpy( pOutData, "", nBufSize );
	}
	else if ( m_InternalType == KEYVALUES3_INTERNAL_TYPE_EXTERNAL_BLOB )
	{
		V_strncpy( pOutData, "", nBufSize );
	}
	else if ( m_InternalType == KEYVALUES3_INTERNAL_TYPE_NULL )
	{
		V_strncpy( pOutData, "", nBufSize );
	}
	else
	{
		AssertMsg1( false, "Unknown KV3 type '%d'.", (int)m_InternalType );
		V_strncpy( pOutData, "", nBufSize );
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void KeyValues3::GetValueAsString( CUtlString *pOutString ) const
{
	if ( m_InternalType == KEYVALUES3_INTERNAL_TYPE_BOOL )
	{
		pOutString->Format( "%s", m_bAsBool ? "true" : "false" );
	}
	else if ( m_InternalType == KEYVALUES3_INTERNAL_TYPE_INT64 )
	{
		pOutString->Format( "%lld", m_nAsInt64 );
	}
	else if ( m_InternalType == KEYVALUES3_INTERNAL_TYPE_UINT64 )
	{
		pOutString->Format( "%llu", m_nAsUint64 );
	}
	else if ( m_InternalType == KEYVALUES3_INTERNAL_TYPE_DOUBLE )
	{
		pOutString->Format( "%g", m_flAsDouble );
	}
	else if ( ( m_InternalType == KEYVALUES3_INTERNAL_TYPE_STRING ) || ( m_InternalType == KEYVALUES3_INTERNAL_TYPE_SHORT_STRING ) || ( m_InternalType == KEYVALUES3_INTERNAL_TYPE_EXTERNAL_STRING ) )
	{
		pOutString->Set( GetValueString() );
	}
	else if ( m_InternalType == KEYVALUES3_INTERNAL_TYPE_BINARY_BLOB )
	{
		pOutString->Set( "" );
	}
	else if ( m_InternalType == KEYVALUES3_INTERNAL_TYPE_EXTERNAL_BLOB )
	{
		pOutString->Set( "" );
	}
	else if ( m_InternalType == KEYVALUES3_INTERNAL_TYPE_NULL )
	{
		pOutString->Set( "" );
	}
	else if ( m_InternalType == KEYVALUES3_INTERNAL_TYPE_ARRAY )
	{
		pOutString->Set( "" );
	}
	else if ( m_InternalType == KEYVALUES3_INTERNAL_TYPE_TABLE )
	{
		pOutString->Set( "" );
	}
	else
	{
		AssertMsg1( false, "Unknown KV3 type '%d'.", (int)m_InternalType );
		pOutString->Set( "" );
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
const char *KeyValues3::GetValueString( const char *pDefaultValue /* = "" */ ) const
{
	if ( ( m_InternalType == KEYVALUES3_INTERNAL_TYPE_STRING ) || ( m_InternalType == KEYVALUES3_INTERNAL_TYPE_EXTERNAL_STRING ) )
	{
		return m_pAsCharPtr;
	}
	else if ( m_InternalType == KEYVALUES3_INTERNAL_TYPE_SHORT_STRING )
	{
		return m_AsShortString;
	}
	else
	{
		return pDefaultValue;
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void KeyValues3::SetValueString( const char *value )
{
	if ( value == nullptr )
	{
		value = "";
	}

	if ( V_strlen( value ) <= KEYVALUES3_SHORT_STRING_LENGTH )
	{
		Internal_SetToShortString( value );
		return;
	}

	Internal_PrepareForType( KEYVALUES3_TYPE_STRING );
	m_pAsCharPtr = CKeyValues3Context::AllocString( this, value );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void KeyValues3::SetValueResourceString( const char *value )
{
	SetValueString( value );
	SetFlag( KEYVALUES3_FLAG_RESOURCE_REFERENCE );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void KeyValues3::Internal_AimAtExternalString( const char *value )
{
	if ( V_strlen( value ) <= KEYVALUES3_SHORT_STRING_LENGTH )
	{
		Internal_SetToShortString( value );
		return;
	}

	Internal_PrepareForInternalType( KEYVALUES3_INTERNAL_TYPE_EXTERNAL_STRING );
	m_pAsCharPtr = value;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void KeyValues3::Internal_SetToShortString( const char *value )
{
	Internal_PrepareForInternalType( KEYVALUES3_INTERNAL_TYPE_SHORT_STRING );

	V_strncpy( m_AsShortString, value, sizeof(m_AsShortString) );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void KeyValues3::SetValueInt( int value )
{
	Internal_PrepareForType( KEYVALUES3_TYPE_INT64 );
	m_nAsInt64 = value;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void KeyValues3::SetValueBool( bool value )
{
	Internal_PrepareForType( KEYVALUES3_TYPE_BOOL );
	m_bAsBool = value;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void KeyValues3::SetValueFloat( double value )
{
	Internal_PrepareForType( KEYVALUES3_TYPE_DOUBLE );
	m_flAsDouble = value;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void KeyValues3::SetValueUint64( uint64 value )
{
	Internal_PrepareForType( KEYVALUES3_TYPE_UINT64 );
	m_nAsUint64 = value;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void KeyValues3::SetValueInt64( int64 value )
{
	Internal_PrepareForType( KEYVALUES3_TYPE_INT64 );
	m_nAsInt64 = value;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void KeyValues3::SetValueDouble( double value )
{
	Internal_PrepareForType( KEYVALUES3_TYPE_DOUBLE );
	m_flAsDouble = value;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void KeyValues3::SetToNull()
{
	Internal_PrepareForType( KEYVALUES3_TYPE_NULL );
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void KeyValues3::SetIntFromString( const char *pString )
{
	if ( pString == NULL || pString[0] == '\0' )
	{
		SetValueInt( 0 );
		return;
	}

	// since we store our ints in 64 bit, only question is whether it's uint or not
	if ( pString[0] == '-' )
	{
		SetValueInt64( V_atoi64( pString ) );
	}
	else
	{
		uint64 nValue = V_atoui64( pString );
		if ( nValue > INT64_MAX )
		{
			SetValueUint64( nValue );
		}
		else
		{
			SetValueInt64( nValue );
		}
	}
}



//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void KeyValues3::ParseValueFromString( const char *pString )
{
	if ( pString == NULL )
	{
		pString = "";
	}

	if ( GetType() == KEYVALUES3_TYPE_NULL )
	{
		// no work!
	}
	else if ( GetType() == KEYVALUES3_TYPE_ARRAY )
	{
		// TODO: support commas?
		CSplitString split( pString, " " ); // this is fairly slow
		int nSplitLen = split.Count();
		SetArrayElementCount( nSplitLen );
		for ( int i = 0; i < nSplitLen; ++i )
		{
			GetArrayElement( i )->ParseValueFromString( split[i] );
		}
	}
	else if ( GetType() == KEYVALUES3_TYPE_INT64 )
	{
		SetValueInt64( V_atoi64( pString ) );
	}
	else if ( GetType() == KEYVALUES3_TYPE_UINT64 )
	{
		SetValueInt64( V_atoui64( pString ) );
	}
	else if ( GetType() == KEYVALUES3_TYPE_DOUBLE )
	{
		SetValueDouble( V_atod( pString ) );
	}
	else if ( GetType() == KEYVALUES3_TYPE_STRING )
	{
		SetValueString( pString );
	}
	else if ( GetType() == KEYVALUES3_TYPE_BINARY_BLOB )
	{
		SetToBinaryBlob( (byte*)pString, V_strlen(pString)+1 );
	}
	else if ( GetType() == KEYVALUES3_TYPE_BOOL )
	{
		if ( !V_stricmp( pString, "true" ) )
		{
			SetValueBool( true );
		}
		else if ( !V_stricmp( pString, "false" ) )
		{
			SetValueBool( false );
		}
		else
		{
			SetValueBool( V_atoi( pString ) != 0 );
		}
	}
	else
	{
		AssertMsg1( false, "Unsupported key type for KeyValues3::ParseValueFromString - %d", GetType() );
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool KeyValues3::EqualsValueFromString( const char *pString )
{
	KeyValues3 temp;
	temp.CopyFrom( this );
	temp.ParseValueFromString( pString );

	return IsIdenticalTo( &temp, false );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void KeyValues3::EnsureIsArray( int nCount, KeyValues3Type_t nType /* = KEYVALUES3_TYPE_NULL */ )
{
	if ( GetType() != KEYVALUES3_TYPE_ARRAY )
	{
		SetToEmptyArray();
	}

	if ( GetArrayElementCount() != nCount )
	{
		SetArrayElementCount( nCount );
	}

	for ( int i = 0; i < nCount; ++i )
	{
		GetArrayElement( i )->EnsureTypeIs( nType );
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void KeyValues3::EnsureIsAnyArray( int nCount )
{
	if ( GetType() != KEYVALUES3_TYPE_ARRAY )
	{
		SetToEmptyArray();
	}

	if ( GetArrayElementCount() != nCount )
	{
		SetArrayElementCount( nCount );
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void KeyValues3::EnsureTypeIs( KeyValues3Type_t nType )
{
	if ( GetType() == nType )
		return;

	switch ( nType )
	{
		case KEYVALUES3_TYPE_NULL:
		{
			SetToNull();
			break;
		}
		case KEYVALUES3_TYPE_BOOL:
		{
			SetValueBool( GetValueInt() != 0 );
			break;
		}
		case KEYVALUES3_TYPE_INT64:
		{
			SetValueInt64( GetValueInt64() );
			break;
		}
		case KEYVALUES3_TYPE_UINT64:
		{
			SetValueUint64( GetValueUint64() );
			break;
		}
		case KEYVALUES3_TYPE_DOUBLE:
		{
			SetValueDouble( GetValueDouble() );
			break;
		}
		case KEYVALUES3_TYPE_STRING:
		{
			CUtlString temp;
			GetValueAsString( &temp );
			SetValueString( temp.Get() );
			break;
		}
		case KEYVALUES3_TYPE_BINARY_BLOB:
		{
			AssertMsg( false, "EnsureTypeIs(KEYVALUES3_TYPE_BINARY_BLOB) not supported!" );
			SetToZeroedBinaryBlob( 0 );
			break;
		}
		case KEYVALUES3_TYPE_ARRAY:
		{
			SetToEmptyArray();
			return;
		}
		case KEYVALUES3_TYPE_TABLE:
		{
			SetToEmptyTable();
			return;
		}
		default:
		{
			AssertMsg1( false, "EnsureTypeIs - unknown type '%d'", nType );
		}
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
KeyValues3 *KeyValues3::SetMemberToNull( CKV3MemberName memberName )
{
	KeyValues3 *pMember = FindOrCreateMember( memberName );
	pMember->SetToNull();
	return pMember;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
KeyValues3 *KeyValues3::SetMemberString( CKV3MemberName memberName, const char *value )
{
	KeyValues3 *pMember = FindOrCreateMember( memberName );
	pMember->SetValueString( value );
	return pMember;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
KeyValues3 *KeyValues3::SetMemberResourceString( CKV3MemberName memberName, const char *value )
{
	KeyValues3 *pMember = FindOrCreateMember( memberName );
	pMember->SetValueResourceString( value );
	return pMember;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
KeyValues3 *KeyValues3::SetMemberInt( CKV3MemberName memberName, int value )
{
	KeyValues3 *pMember = FindOrCreateMember( memberName );
	pMember->SetValueInt( value );
	return pMember;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
KeyValues3 *KeyValues3::SetMemberBool( CKV3MemberName memberName, bool value )
{
	KeyValues3 *pMember = FindOrCreateMember( memberName );
	pMember->SetValueBool( value );
	return pMember;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
KeyValues3 *KeyValues3::SetMemberFloat( CKV3MemberName memberName, float value )
{
	KeyValues3 *pMember = FindOrCreateMember( memberName );
	pMember->SetValueFloat( value );
	return pMember;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
KeyValues3 *KeyValues3::SetMemberInt64( CKV3MemberName memberName, int64 value )
{
	KeyValues3 *pMember = FindOrCreateMember( memberName );
	pMember->SetValueInt64( value );
	return pMember;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
KeyValues3 *KeyValues3::SetMemberUint64( CKV3MemberName memberName, uint64 value )
{
	KeyValues3 *pMember = FindOrCreateMember( memberName );
	pMember->SetValueUint64( value );
	return pMember;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
KeyValues3 *KeyValues3::SetMemberDouble( CKV3MemberName memberName, double value )
{
	KeyValues3 *pMember = FindOrCreateMember( memberName );
	pMember->SetValueDouble( value );
	return pMember;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
KeyValues3 *KeyValues3::SetMemberToEmptyTable( CKV3MemberName memberName )
{
	KeyValues3 *pMember = FindOrCreateMember( memberName );
	pMember->SetToEmptyTable();
	return pMember;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void KeyValues3::SetMemberToCopyOfValue( CKV3MemberName memberName, KeyValues3 *pValue )
{
	KeyValues3 *pMember = FindOrCreateMember( memberName );
	pMember->CopyFrom( pValue );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
KeyValues3 *KeyValues3::FindOrCreateMember( CKV3MemberName memberName, bool *pOutCreated )
{
	Assert( memberName.m_pString && memberName.m_pString[0] != '\0' );

	if ( GetType() != KEYVALUES3_TYPE_TABLE )
	{
		Internal_PrepareForType( KEYVALUES3_TYPE_TABLE );
	}

	Assert( GetType() == KEYVALUES3_TYPE_TABLE ); // temp defensive assert
	Assert( GetTable() != nullptr ); // temp defensive assert

	int nMemberIndex = GetTable()->FindOrCreateMember_Internal( this, memberName, false, pOutCreated );

	Assert( GetType() == KEYVALUES3_TYPE_TABLE ); // temp defensive assert
	Assert( GetTable() != nullptr ); // temp defensive assert

	return GetTable()->MemberData( nMemberIndex );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void KeyValues3::SetValueFloatArray( int nFloats, const float *pValues )
{
	SetArrayElementCount( nFloats );
	for ( int iFloat = 0; iFloat < nFloats; ++iFloat )
	{
		GetArrayElement( iFloat )->SetValueFloat( pValues[ iFloat ] );
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool KeyValues3::GetValueFloatArray( int nFloats, float *pOutValues ) const
{
	int nAvailable = Min( nFloats, GetArrayElementCount() );
	int iFloat = 0;
	for ( ; iFloat < nAvailable; ++iFloat )
	{
		pOutValues[ iFloat ] = GetArrayElement( iFloat )->GetValueFloat();
	}

	if ( iFloat == nAvailable )
		return nFloats == GetArrayElementCount();
	
	for ( ; iFloat < nFloats; ++iFloat )
	{
		// fill any remaining values with 0
		pOutValues[ iFloat ] = 0;
	}

	return false;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void KeyValues3::SetMemberFloatArray( CKV3MemberName memberName, int nFloats, const float *pValues /*= KeyValues3LowercaseHash_t() */ )
{
	KeyValues3 *pMember = FindOrCreateMember( memberName );
	Assert( pMember );
	pMember->SetValueFloatArray( nFloats, pValues );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool KeyValues3::GetMemberFloatArray( CKV3MemberName memberName, int nFloats, float *pOutValues /*= KeyValues3LowercaseHash_t() */ ) const
{
	if ( const KeyValues3 *pMember = FindMember( memberName ) )
	{
		return pMember->GetValueFloatArray( nFloats, pOutValues );
	}
	else
	{
		V_memset( pOutValues, 0, sizeof( float )*nFloats );
		return false;
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
KeyValues3 *KeyValues3::Internal_FindOrCreateMemberAimedAtExternalName( CKV3MemberName memberName )
{
	if ( GetType() != KEYVALUES3_TYPE_TABLE )
	{
		Internal_PrepareForType( KEYVALUES3_TYPE_TABLE );
	}

	int nMemberIndex = GetTable()->FindOrCreateMember_Internal( this, memberName, true, nullptr );
	return GetTable()->MemberData( nMemberIndex );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
KeyValues3 *KeyValues3::FindMember( CKV3MemberName memberName )
{
	return Internal_FindMember( memberName );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
KeyValues3 *KeyValues3::Internal_FindMember( CKV3MemberName memberName )
{
	if ( GetType() != KEYVALUES3_TYPE_TABLE )
		return NULL;

	int nMemberIndex = GetTable()->FindMember_Internal( memberName );
	if ( nMemberIndex == -1 )
		return NULL;

	return GetTable()->MemberData( nMemberIndex );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
int KeyValues3::GetMemberCount() const
{
	if ( GetType() != KEYVALUES3_TYPE_TABLE )
		return 0;

	return GetTable()->Count();
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
KeyValues3 *KeyValues3::GetMember( int nIndex )
{
	if ( GetType() != KEYVALUES3_TYPE_TABLE )
		return NULL;

	CKeyValues3Table *pTable = GetTable();
	if ( nIndex < 0 || nIndex >= pTable->Count() )
		return NULL;

	return pTable->MemberData( nIndex );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
const KeyValues3 *KeyValues3::GetMember( int nIndex ) const
{
	if ( GetType() != KEYVALUES3_TYPE_TABLE )
		return NULL;

	const CKeyValues3Table *pTable = GetTable();
	if ( nIndex < 0 || nIndex >= pTable->Count() )
		return NULL;

	return pTable->MemberData( nIndex );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
const char *KeyValues3::GetMemberName( int nIndex ) const
{
	if ( GetType() != KEYVALUES3_TYPE_TABLE )
		return NULL;

	const CKeyValues3Table *pTable = GetTable();
	if ( nIndex < 0 || nIndex >= pTable->Count() )
		return NULL;

	return pTable->MemberName( nIndex );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
CKV3MemberName KeyValues3::GetKV3MemberName( int nIndex ) const
{
	if ( GetType() != KEYVALUES3_TYPE_TABLE )
		return CKV3MemberName();

	const CKeyValues3Table *pTable = GetTable();
	if ( nIndex < 0 || nIndex >= pTable->Count() )
		return CKV3MemberName();

	return pTable->KV3MemberName( nIndex );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void KeyValues3::GetMemberNameEscaped( int nIndex, CUtlString *pOutName ) const
{
	const char *pName = GetMemberName( nIndex );
	if ( !UtlTokenizer_QuoteAndEscapeString( pName, pOutName ) )
	{
		// prefer the unescaped version if it wasn't necessary
		*pOutName = pName;
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool KeyValues3::RemoveMember( int nIndex )
{
	if ( GetType() != KEYVALUES3_TYPE_TABLE )
		return false;

	CKeyValues3Table *pTable = GetTable();
	if ( nIndex < 0 || nIndex >= pTable->Count() )
		return false;

	pTable->Remove( this, nIndex );
	return true;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool KeyValues3::RemoveMember( CKV3MemberName memberName )
{
	if ( GetType() != KEYVALUES3_TYPE_TABLE )
		return false;

	CKeyValues3Table *pTable = GetTable();
	int nMemberIndex = pTable->FindMember_Internal( memberName );
	if ( nMemberIndex == -1 )
		return false;

	pTable->Remove( this, nMemberIndex );
	return true;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool KeyValues3::RemoveMember( KeyValues3 *pMember )
{
	if ( m_InternalType != KEYVALUES3_TYPE_TABLE )
		return false;

	CKeyValues3Table *pTable = GetTable();
	return pTable->Remove( this, pMember );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool KeyValues3::HasFlag( KeyValues3Flag_t nFlag ) const
{
	return ( int(m_Flags) & int(nFlag) ) != 0;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool KeyValues3::HasAnyFlags() const
{
	return ( int(m_Flags) != 0 );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
KeyValues3Flag_t KeyValues3::GetAllFlags() const
{
	return KeyValues3Flag_t(m_Flags);
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool KeyValues3::HasMetadata() const
{
	CKeyValues3Metadata *pMeta = Internal_GetMetadata( nullptr );
	return ( pMeta != nullptr );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
int KeyValues3::Metadata_GetLineNumber() const
{
	CKeyValues3Metadata *pMeta = Internal_GetMetadata( nullptr );
	if ( !Verify( pMeta ) )
		return -1;

	return pMeta->m_nLineNumber;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
const char *KeyValues3::Metadata_GetFilename() const
{
	CKeyValues3Metadata *pMeta = Internal_GetMetadata( nullptr );
	if ( !Verify( pMeta ) )
		return "";

	return pMeta->m_File.String();
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void KeyValues3::Metadata_SetFileLineNumber( const char *pFilename, int nLine ) const
{
	CKeyValues3Context *pContext = nullptr;
	CKeyValues3Metadata *pMeta = Internal_GetMetadata( &pContext );
	if ( !Verify( pMeta ) )
		return;

	pMeta->m_File = pContext->Impl().m_Symbols.AddString( pFilename );
	pMeta->m_nLineNumber = nLine;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void KeyValues3::SetFlag( KeyValues3Flag_t nFlag, bool bSet )
{
	if ( bSet )
	{
		m_Flags = KeyValues3Flag_t( int(m_Flags) | int(nFlag) );
	}
	else
	{
		m_Flags = KeyValues3Flag_t( int(m_Flags) & (~int(nFlag)) );
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void KeyValues3::SetAllFlags( KeyValues3Flag_t nFlags )
{
	m_Flags = nFlags;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool KeyValues3::IsIdenticalTo( const KeyValues3 *pOther, bool bAssertOnFailure ) const
{
	if ( GetType() != pOther->GetType() )
	{
		if ( bAssertOnFailure )
		{
			AssertMsg( false, "KeyValues3::IsIdenticalTo failure!" );
		}
		return false;
	}

	if ( m_Flags != pOther->m_Flags )
	{
		if ( bAssertOnFailure )
		{
			AssertMsg( false, "KeyValues3::IsIdenticalTo failure!" );
		}
		return false;
	}

	bool bIdentical = false;

	switch ( pOther->GetType() )
	{
		case KEYVALUES3_TYPE_NULL:		bIdentical = true; break;
		case KEYVALUES3_TYPE_BOOL:		bIdentical = ( m_bAsBool == pOther->m_bAsBool ); break;
		case KEYVALUES3_TYPE_INT64:		bIdentical = ( m_nAsInt64 == pOther->m_nAsInt64 ); break;
		case KEYVALUES3_TYPE_UINT64:	bIdentical = ( m_nAsUint64 == pOther->m_nAsUint64 ); break;
		case KEYVALUES3_TYPE_DOUBLE:
		{
			bIdentical = ( fabs( m_flAsDouble - pOther->m_flAsDouble ) <= KEYVALUES3_IDENTICAL_EPSILON );
			break;
		}

		case KEYVALUES3_TYPE_STRING:
		{
			bIdentical = !V_strcmp( GetValueString(), pOther->GetValueString() );
			break;
		}

		case KEYVALUES3_TYPE_BINARY_BLOB:
		{
			int nLength = GetBinaryBlobSize();
			if ( nLength != pOther->GetBinaryBlobSize() )
			{
				bIdentical = false;
				break;
			}

			bIdentical = !V_memcmp( GetBinaryBlobBase(), pOther->GetBinaryBlobBase(), nLength );
			break;
		}

		case KEYVALUES3_TYPE_ARRAY:
		{
			int nCount = GetArrayElementCount();
			if ( nCount != pOther->GetArrayElementCount() )
			{
				bIdentical = false;
				break;
			}

			for ( int i = 0; i < nCount; ++i )
			{
				const KeyValues3 *pElement = GetArrayElement( i );
				const KeyValues3 *pOtherElement = pOther->GetArrayElement( i );
				if ( !pElement->IsIdenticalTo( pOtherElement, bAssertOnFailure ) )
				{
					return false;
				}
			}
			return true;
		}
		case KEYVALUES3_TYPE_TABLE:
		{
			return GetTable()->IsIdenticalTo( pOther->GetTable(), bAssertOnFailure );
		}

		default:
		{
			AssertMsg1( false, "KeyValues3::IsIdenticalTo - unknown type '%d'", (int)pOther->m_InternalType );
			return false;
		}
	}

	if ( !bIdentical && bAssertOnFailure )
	{
		AssertMsg( false, "KeyValues3::IsIdenticalTo failure!" );
	}

	return bIdentical;
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void KeyValues3::CopyFrom( const KeyValues3 *pOther )
{
	KeyValues3Type_t nNewType = pOther->GetType();
	Internal_PrepareForType( nNewType );

	m_Flags = pOther->m_Flags;

	switch ( nNewType )
	{
		case KEYVALUES3_TYPE_NULL:
			break;
		case KEYVALUES3_TYPE_BOOL:
			m_bAsBool = pOther->m_bAsBool;
			break;
		case KEYVALUES3_TYPE_INT64:
			m_nAsInt64 = pOther->m_nAsInt64;
			break;
		case KEYVALUES3_TYPE_UINT64:
			m_nAsUint64 = pOther->m_nAsUint64;
			break;
		case KEYVALUES3_TYPE_DOUBLE:
			m_flAsDouble = pOther->m_flAsDouble;
			break;
		case KEYVALUES3_TYPE_STRING:
		{
			SetValueString( pOther->GetValueString() );
			break;
		}
		case KEYVALUES3_TYPE_BINARY_BLOB:
		{
			const byte *pBlob = pOther->Internal_BinaryBlobBase();
			int nBlobSize = pOther->Internal_BinaryBlobSize();
			if ( nBlobSize )
			{
				SetToBinaryBlob( pBlob, nBlobSize );
			}
			else
			{
				// zero length
				SetToZeroedBinaryBlob( 0 );
			}
			break;
		}
		case KEYVALUES3_TYPE_ARRAY:
		{
			int nCount = pOther->m_pArray ? (int)pOther->m_pArray->m_nCount : 0;
			Internal_SetArrayCount( nCount );
			for ( int i = 0; i < nCount; ++i )
			{
				KeyValues3 *pOtherElement = pOther->m_pArray->AsKVArray()[ i ];
				KeyValues3 *pElement = m_pArray->AsKVArray()[ i ];
				pElement->CopyFrom( pOtherElement );
			}
			break;
		}
		case KEYVALUES3_TYPE_TABLE:
		{
			SetToEmptyTable();
			GetTable()->CopyFrom( this, pOther->GetTable() );
			break;
		}

		default:
		{
			Assert(0);
		}
	}
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void KeyValues3::CopyMatchingKeysFrom( const KeyValues3 *pOther )
{
	if ( GetType() != KEYVALUES3_TYPE_TABLE || pOther->GetType() != KEYVALUES3_TYPE_TABLE )
		return;

	CKeyValues3Table *pTable = GetTable();

	for ( int iMember = 0; iMember < pTable->Count(); ++iMember )
	{
		KeyValues3LowercaseHash_t nMemberNameHash = pTable->MemberNameHash( iMember );
		const char *pMemberName = pTable->MemberName( iMember );
		const KeyValues3 *pOtherMember = pOther->FindMember( CKV3MemberName( pMemberName, nMemberNameHash ) );
		if ( !pOtherMember )
			continue;

		pTable->MemberData( iMember )->CopyFrom( pOtherMember );
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void KeyValues3::MoveFrom( KeyValues3 *pOther )
{
	if ( pOther->GetParentContext() != GetParentContext() )
	{
		// different context - can't take control of memory
		CopyFrom( pOther );
		pOther->SetToNull();
		return;
	}

	if ( pOther->m_InternalType == KEYVALUES3_INTERNAL_TYPE_TABLE )
	{
		SetToNull();
		m_InternalType = KEYVALUES3_INTERNAL_TYPE_TABLE;
		m_pTable = pOther->m_pTable;

		// Manually set here because m_pTable being NULL isn't normally allowed
		pOther->m_InternalType = KEYVALUES3_INTERNAL_TYPE_NULL;
		pOther->m_pTable = NULL;
	}
	else if ( pOther->m_InternalType == KEYVALUES3_INTERNAL_TYPE_ARRAY )
	{
		SetToNull();
		m_InternalType = KEYVALUES3_INTERNAL_TYPE_ARRAY;
		m_pArray = pOther->m_pArray;
		pOther->m_pArray = NULL;
		pOther->SetToNull();
	}
	else if ( pOther->m_InternalType == KEYVALUES3_INTERNAL_TYPE_BINARY_BLOB )
	{
		SetToNull();
		m_InternalType = KEYVALUES3_INTERNAL_TYPE_BINARY_BLOB;
		m_pBinaryBlob = pOther->m_pBinaryBlob;
		pOther->m_pBinaryBlob = NULL;
		pOther->SetToNull();
	}
	else if ( pOther->m_InternalType == KEYVALUES3_INTERNAL_TYPE_EXTERNAL_BLOB )
	{
		SetToNull();
		m_InternalType = KEYVALUES3_INTERNAL_TYPE_EXTERNAL_BLOB;
		m_pBinaryBlob = pOther->m_pBinaryBlob;
		pOther->m_pBinaryBlob = NULL;
		pOther->SetToNull();
	}
	else
	{
		// simple copy
		CopyFrom( pOther );
		pOther->SetToNull();
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
CKeyValues3Table::CKeyValues3Table()
{
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
CKeyValues3Table::~CKeyValues3Table()
{
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CKeyValues3Table::Free( KeyValues3 *pParent )
{
	int nMemberCount = m_MemberData.Count();
	AssertDbg( nMemberCount == m_MemberName.Count() );
	AssertDbg( nMemberCount == m_MemberNameLowerHash.Count() );
	AssertDbg( nMemberCount == m_MemberFlags.Count() );

	for ( int i = 0; i < nMemberCount; ++i )
	{
		CKeyValues3Context::FreeKV( pParent, m_MemberData[ i ] );
	}
	for ( int i = 0; i < nMemberCount; ++i )
	{
		CKeyValues3Context::FreeSharedString( pParent, m_MemberName[ i ] );
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CKeyValues3Table::Init( KeyValues3 *pParent )
{
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
int CKeyValues3Table::FindOrCreateMember_Internal( KeyValues3 *pParent, CKV3MemberName memberName, bool bUseExternalStorageForName, bool *pOutCreated )
{
	int nMemberCount = m_MemberNameLowerHash.Count();
	AssertDbg( nMemberCount == m_MemberData.Count() );
	AssertDbg( nMemberCount == m_MemberName.Count() );
	AssertDbg( nMemberCount == m_MemberFlags.Count() );

	for ( int i = 0; i < nMemberCount; ++i )
	{
		if ( m_MemberNameLowerHash[ i ] == memberName.m_Hash )
		{
			if ( KV3_DEBUG_CHECK_FOR_HASH_COLLISIONS )
			{
				const char *pActualMemberName = m_MemberName[ i ];
				NOTE_UNUSED( pActualMemberName );
				AssertMsg3( !V_stricmp( pActualMemberName, memberName.m_pString ), "KV3 Member name hash collision! '%s' and '%s' hash to the same value (%d)!", pActualMemberName, memberName.m_pString, (int)KeyValues3LowercaseHashToInt(memberName.m_Hash) );
			}
			if ( pOutCreated )
			{
				*pOutCreated = false;
			}
			return i;
		}
	}

	// add new member
	uint8 nFlags = 0;
	int nNewIndex = m_MemberNameLowerHash.AddToTail( memberName.m_Hash );
	if ( bUseExternalStorageForName )
	{
		m_MemberName.AddToTail( memberName.m_pString );
		nFlags |= MEMBER_FLAG_EXTERNAL_NAME;
	}
	else
	{
		m_MemberName.AddToTail( CKeyValues3Context::AllocSharedString( pParent, memberName.m_pString ) );
	}

	m_MemberData.AddToTail( CKeyValues3Context::AllocKV( pParent ) );
	m_MemberFlags.AddToTail( nFlags );
	if ( pOutCreated )
	{
		*pOutCreated = true;
	}
	return nNewIndex;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
int CKeyValues3Table::FindMember_Internal( CKV3MemberName memberName ) const
{
	int nMemberCount = m_MemberNameLowerHash.Count();
	AssertDbg( nMemberCount == m_MemberData.Count() );
	AssertDbg( nMemberCount == m_MemberName.Count() );
	AssertDbg( nMemberCount == m_MemberFlags.Count() );

	if ( KV3_DEBUG_CHECK_FOR_HASH_COLLISIONS )
	{
		AssertDbg( KV3MakeLowerHash( memberName.m_pString ) == memberName.m_Hash );
	}

	for ( int i = 0; i < nMemberCount; ++i )
	{
		if ( m_MemberNameLowerHash[ i ] == memberName.m_Hash )
		{
			if ( KV3_DEBUG_CHECK_FOR_HASH_COLLISIONS )
			{
				const char *pActualMemberName = m_MemberName[ i ];
				NOTE_UNUSED( pActualMemberName );
				AssertMsg3( !V_stricmp( pActualMemberName, memberName.m_pString ), "KV3 Member name hash collision! '%s' and '%s' hash to the same value (%d)!", pActualMemberName, memberName.m_pString, (int)KeyValues3LowercaseHashToInt(memberName.m_Hash) );
			}
			return i;
		}
	}

	return -1;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
KeyValues3 *CKeyValues3Table::MemberData( int nIndex )
{
	int nMemberCount = m_MemberNameLowerHash.Count();
	AssertDbg( nMemberCount == m_MemberData.Count() );
	AssertDbg( nMemberCount == m_MemberName.Count() );
	AssertDbg( nMemberCount == m_MemberFlags.Count() );

	Assert( nIndex >= 0 && nIndex < nMemberCount );

	return m_MemberData[ nIndex ];
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
const KeyValues3 *CKeyValues3Table::MemberData( int nIndex ) const
{
	int nMemberCount = m_MemberNameLowerHash.Count();
	AssertDbg( nMemberCount == m_MemberData.Count() );
	AssertDbg( nMemberCount == m_MemberName.Count() );
	AssertDbg( nMemberCount == m_MemberFlags.Count() );

	Assert( nIndex >= 0 && nIndex < nMemberCount );

	return m_MemberData[ nIndex ];
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
const char *CKeyValues3Table::MemberName( int nIndex ) const
{
	int nMemberCount = m_MemberNameLowerHash.Count();
	AssertDbg( nMemberCount == m_MemberData.Count() );
	AssertDbg( nMemberCount == m_MemberName.Count() );
	AssertDbg( nMemberCount == m_MemberFlags.Count() );

	Assert( nIndex >= 0 && nIndex < nMemberCount );

	return m_MemberName[ nIndex ];
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
KeyValues3LowercaseHash_t CKeyValues3Table::MemberNameHash( int nIndex ) const
{
	int nMemberCount = m_MemberNameLowerHash.Count();
	AssertDbg( nMemberCount == m_MemberData.Count() );
	AssertDbg( nMemberCount == m_MemberName.Count() );
	AssertDbg( nMemberCount == m_MemberFlags.Count() );

	Assert( nIndex >= 0 && nIndex < nMemberCount );

	return m_MemberNameLowerHash[ nIndex ];
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
CKV3MemberName CKeyValues3Table::KV3MemberName( int nIndex ) const
{
	int nMemberCount = m_MemberNameLowerHash.Count();
	AssertDbg( nMemberCount == m_MemberData.Count() );
	AssertDbg( nMemberCount == m_MemberName.Count() );
	AssertDbg( nMemberCount == m_MemberFlags.Count() );

	Assert( nIndex >= 0 && nIndex < nMemberCount );

	return CKV3MemberName( m_MemberName[ nIndex ], m_MemberNameLowerHash[ nIndex ] );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CKeyValues3Table::CopyFrom( KeyValues3 *pParent, const CKeyValues3Table *pOther )
{
	Clear( pParent );

	int nOtherCount = pOther->m_MemberNameLowerHash.Count();
	AssertDbg( nOtherCount == pOther->m_MemberData.Count() );
	AssertDbg( nOtherCount == pOther->m_MemberName.Count() );
	AssertDbg( nOtherCount == pOther->m_MemberFlags.Count() );

	m_MemberNameLowerHash.SetCount( nOtherCount );
	m_MemberData.SetCount( nOtherCount );
	m_MemberName.SetCount( nOtherCount );
	m_MemberFlags.SetCount( nOtherCount );

	for ( int i = 0; i < nOtherCount; ++i )
	{
		const char *pName = pOther->m_MemberName[ i ];
		KeyValues3LowercaseHash_t nLowerHash = pOther->m_MemberNameLowerHash[ i ];
		AssertDbg( nLowerHash == KV3MakeLowerHash( pName ) );

		m_MemberNameLowerHash[ i ] = nLowerHash;
		m_MemberFlags[ i ] = 0; // can't preserve external pointers across this boundary
		m_MemberName[ i ] = CKeyValues3Context::AllocSharedString( pParent, pName );
		m_MemberData[ i ] = CKeyValues3Context::AllocKV( pParent );
		m_MemberData[ i ]->CopyFrom( pOther->m_MemberData[ i ] );
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CKeyValues3Table::Clear( KeyValues3 *pParent )
{
	int nMemberCount = m_MemberNameLowerHash.Count();
	AssertDbg( nMemberCount == m_MemberData.Count() );
	AssertDbg( nMemberCount == m_MemberName.Count() );
	AssertDbg( nMemberCount == m_MemberFlags.Count() );

	for ( int i = 0; i < nMemberCount; ++i )
	{
		CKeyValues3Context::FreeKV( pParent, m_MemberData[ i ] );
	}

	m_MemberNameLowerHash.RemoveAll();
	m_MemberData.RemoveAll();
	m_MemberName.RemoveAll();
	m_MemberFlags.RemoveAll();
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CKeyValues3Table::Remove( KeyValues3 *pParent, KeyValues3 *pMemberData )
{
	int nMemberCount = m_MemberNameLowerHash.Count();
	AssertDbg( nMemberCount == m_MemberData.Count() );
	AssertDbg( nMemberCount == m_MemberName.Count() );
	AssertDbg( nMemberCount == m_MemberFlags.Count() );

	for ( int i = 0; i < nMemberCount; ++i )
	{
		if ( m_MemberData[ i ] == pMemberData )
		{
			Remove( pParent, i );
			return true;
		}
	}

	return false;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CKeyValues3Table::Remove( KeyValues3 *pParent, int nIndex )
{
	int nMemberCount = m_MemberNameLowerHash.Count();
	AssertDbg( nMemberCount == m_MemberData.Count() );
	AssertDbg( nMemberCount == m_MemberName.Count() );
	AssertDbg( nMemberCount == m_MemberFlags.Count() );

	Assert( nIndex >= 0 && nIndex < nMemberCount );

	CKeyValues3Context::FreeKV( pParent, m_MemberData[ nIndex ] );

	m_MemberData.Remove( nIndex );
	m_MemberName.Remove( nIndex );
	m_MemberNameLowerHash.Remove( nIndex );
	m_MemberFlags.Remove( nIndex );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CKeyValues3Table::IsIdenticalTo( const CKeyValues3Table *pOther, bool bAssertOnFailure ) const
{
	int nCount = Count();
	if ( !pOther || ( nCount != pOther->Count() ) )
	{
		if ( bAssertOnFailure )
		{
			AssertMsg( false, "CKeyValues3Table::IsIdenticalTo failure!" );
		}
		return false;
	}

	for ( int i = 0; i < nCount; ++i )
	{
		const char *pMemberName = m_MemberName[ i ];
		KeyValues3LowercaseHash_t nNameHash = m_MemberNameLowerHash[ i ];

		int nOtherIndex = pOther->FindMember_Internal( CKV3MemberName( pMemberName, nNameHash ) );
		if ( nOtherIndex == -1 )
		{
			if ( bAssertOnFailure )
			{
				AssertMsg( false, "CKeyValues3Table::IsIdenticalTo failure!" );
			}
			return false;
		}

		if ( !m_MemberData[ i ]->IsIdenticalTo( pOther->m_MemberData[ nOtherIndex ], bAssertOnFailure ) )
		{
			return false;
		}
	}

	return true;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void KeyValues3::Internal_ClearDataWithoutFreeing()
{
	m_nAsUint64 = 0;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void KeyValues3::Internal_PrepareForInternalType( KeyValues3InternalType_t newType )
{
	if ( newType == KeyValues3InternalType_t(m_InternalType) )
	{
		// some types need to be empty even if we're already the right type
		if ( m_InternalType == KEYVALUES3_INTERNAL_TYPE_TABLE )
		{
			// empty the table but don't need to delete the control object
			m_pTable->Clear( this );
		}
		else
		{
			Internal_FreeAllocation();
		}
		return;
	}

	Internal_FreeAllocation();
	Internal_ClearDataWithoutFreeing();
	m_InternalType = newType;
	Internal_NewAllocation();
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void KeyValues3::Internal_FreeAllocation()
{
	switch ( m_InternalType )
	{
	case KEYVALUES3_TYPE_STRING:
	{
		CKeyValues3Context::FreeString( this, m_pAsCharPtr );
		m_pAsCharPtr = NULL;
		break;
	}
	case KEYVALUES3_TYPE_ARRAY:
	{
		CKeyValues3Context::FreeArray( this, m_pArray );
		m_pArray = NULL;
		break;
	}
	case KEYVALUES3_TYPE_BINARY_BLOB:
	{
		CKeyValues3Context::FreeBlob( this, m_pBinaryBlob );
		m_pBinaryBlob = NULL;
		break;
	}
	case KEYVALUES3_INTERNAL_TYPE_EXTERNAL_BLOB:
	{
		CKeyValues3Context::FreeBlobExternal( this, m_pBinaryBlobExternal );
		m_pBinaryBlobExternal = NULL;
		break;
	}
	case KEYVALUES3_TYPE_TABLE:
		CKeyValues3Context::FreeTable( this, m_pTable );
		m_pTable = NULL;
		break;
	default:
		// other types don't have an allocation
		break;
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void KeyValues3::Internal_NewAllocation()
{
	switch ( m_InternalType )
	{
	case KEYVALUES3_TYPE_TABLE:
		m_pTable = CKeyValues3Context::AllocTable( this );
		break;
	default:
		// other types don't have an inherent allocation
		break;
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
KeyValues3::KeyValues3()
{
	m_bContextIndependent = true;
	m_InternalType = KEYVALUES3_INTERNAL_TYPE_NULL;
	m_Flags = KEYVALUES3_FLAG_NONE;
	Internal_ClearDataWithoutFreeing();
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
KeyValues3::KeyValues3( const KeyValues3 &rhs )
{
	m_bContextIndependent = true;
	m_InternalType = KEYVALUES3_INTERNAL_TYPE_NULL;
	m_Flags = KEYVALUES3_FLAG_NONE;
	Internal_ClearDataWithoutFreeing();
	CopyFrom( &rhs );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
KeyValues3::KeyValues3( CKeyValues3Context *pContext )
{
	AssertDbg( pContext != NULL );
	AssertDbg( GetParentContext() == pContext );

	m_bContextIndependent = false;
	m_InternalType = KEYVALUES3_INTERNAL_TYPE_NULL;
	m_Flags = KEYVALUES3_FLAG_NONE;
	Internal_ClearDataWithoutFreeing();
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
KeyValues3::~KeyValues3()
{
	Internal_FreeAllocation();

#ifdef _DEBUG
	V_memset( this, 0xEE, sizeof(this) );
#endif
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
KeyValues3 &KeyValues3::operator=( const KeyValues3 &rhs )
{
	CopyFrom( &rhs );
	return *this;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
CKeyValues3Context *KeyValues3::GetParentContext() const
{
	CKeyValues3Cluster *pCluster = CKeyValues3Cluster::ClusterFromPointer( this );
	return pCluster ? pCluster->GetParentContext() : NULL;
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
CKeyValues3Metadata *KeyValues3::Internal_GetMetadata( CKeyValues3Context **pOutContext ) const
{
	CKeyValues3Cluster *pCluster = CKeyValues3Cluster::ClusterFromPointer( this );
	if ( !pCluster )
	{
		if ( pOutContext )
		{
			*pOutContext = nullptr;
		}
		return nullptr;
	}

	if ( pOutContext )
	{
		*pOutContext = pCluster->GetParentContext();
	}
	return pCluster->GetMetadataForKV3( this );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
KeyValues3Type_t KeyValues3::GetType() const
{
	if ( m_InternalType == KEYVALUES3_INTERNAL_TYPE_SHORT_STRING )
	{
		return KEYVALUES3_TYPE_STRING;
	}

	if ( m_InternalType == KEYVALUES3_INTERNAL_TYPE_EXTERNAL_STRING )
	{
		return KEYVALUES3_TYPE_STRING;
	}

	if ( m_InternalType == KEYVALUES3_INTERNAL_TYPE_EXTERNAL_BLOB )
	{
		return KEYVALUES3_TYPE_BINARY_BLOB;
	}

	return (KeyValues3Type_t)m_InternalType;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
CKeyValues3Context::CKeyValues3Context()
{
	// clang on OSX was putting in an entire extra block of
	// padding for no obvious reason.  Make sure that
	// things are the size we expect them to be.
	COMPILE_TIME_ASSERT( sizeof( *this ) == 2 * KV3_CLUSTER_SIZEOF );

	Construct< KeyValues3ContextImpl_t >( &Impl() );
	Impl().m_EmbeddedCluster.Init( this );
	Impl().m_pFreeClusterList = &Impl().m_EmbeddedCluster;

	Impl().m_EmbeddedCluster.Allocate()->SetToNull(); // Root
	AssertDbg( Impl().m_EmbeddedCluster.Get( EMBEDDED_CLUSTER_INDEX_OF_ROOT ) == Root() );

	Impl().m_nOutstandingKV3Allocations = 0;
	Impl().m_bMetadataEnabled = false;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
CKeyValues3Context::~CKeyValues3Context()
{
	Root()->SetToNull();
	Assert( Impl().m_EmbeddedCluster.AllocationCount() == 1 ); // root
	Assert( Impl().m_AdditionalClusters.Count() == 0 );
	Assert( Impl().m_nOutstandingKV3Allocations == 0 );

#ifdef DBGFLAG_ASSERT
	if ( Impl().m_nOutstandingKV3Allocations != 0 || Impl().m_AdditionalClusters.Count() > 0 )
	{
		Msg( "KV3 leak detected - dumping clusters:\n" );
		Msg( "- Embedded Cluster:\n" );
		Impl().m_EmbeddedCluster.Dump();
		for ( int i = 0; i < Impl().m_AdditionalClusters.Count(); ++i )
		{
			Msg( "- Additional Cluster %d:\n", i );
			Impl().m_AdditionalClusters[ i ]->Dump();
		}
	}
#endif

	Destruct( &Impl() );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
KeyValues3 *CKeyValues3Context::AllocKV( KeyValues3 *pParent )
{
	CKeyValues3Context *pContext = pParent->GetParentContext();
	if ( pContext )
	{
		return pContext->AllocKV_Internal();
	}
	else
	{
		return new KeyValues3();
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
KeyValues3 *CKeyValues3Context::AllocKV_Internal()
{
	Impl().m_nOutstandingKV3Allocations++;

	if ( Impl().m_pFreeClusterList != NULL )
	{
		AssertDbg( !Impl().m_pFreeClusterList->IsFull() );

		KeyValues3 *pResult = Impl().m_pFreeClusterList->Allocate();

		if ( Impl().m_pFreeClusterList->IsFull() )
		{
			// remove from free list
			CKeyValues3Cluster *pNextFreeCluster = Impl().m_pFreeClusterList->GetNextFreeCluster();
			Impl().m_pFreeClusterList->SetNextFreeCluster( NULL );
			Impl().m_pFreeClusterList = pNextFreeCluster;
		}

		return pResult;
	}

	CKeyValues3Cluster *pNewCluster = new CKeyValues3Cluster();
	pNewCluster->Init( this );

	Impl().m_AdditionalClusters.AddToTail( pNewCluster );

	// free list was empty, but now nonempty
	Impl().m_pFreeClusterList = pNewCluster;

	KeyValues3 *pResult = pNewCluster->Allocate();
	return pResult;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CKeyValues3Context::FreeKV( KeyValues3 *pParent, KeyValues3 *pKV )
{
	AssertDbg( pKV->GetParentContext() == pParent->GetParentContext() );
	CKeyValues3Context *pContext = pParent->GetParentContext();
	if ( pContext )
	{
		return pContext->FreeKV_Internal( pKV );
	}
	else
	{
		delete pKV;
	}
}


//--------------------------------------------------------------------------------------------------
// WARNING: This function is reentrant! Freeing a table or array may recursively free other things
//--------------------------------------------------------------------------------------------------
void CKeyValues3Context::FreeKV_Internal( KeyValues3 *pKV )
{
	Impl().m_nOutstandingKV3Allocations--;
	AssertDbg( pKV != Root() );

	CKeyValues3Cluster *pCluster = CKeyValues3Cluster::ClusterFromPointer( pKV );
	pCluster->Free( pKV );

	int nNewAllocCount = pCluster->AllocationCount();

	if ( nNewAllocCount == 0 ) // was the last one
	{
		if ( pCluster != &( Impl().m_EmbeddedCluster ) )
		{
			AssertDbg( pCluster->GetParentContext() == this );
			Verify( Impl().m_AdditionalClusters.FindAndFastRemove( pCluster ) );

			// remove from the free list
			if ( Impl().m_pFreeClusterList == pCluster )
			{
				Impl().m_pFreeClusterList = pCluster->GetNextFreeCluster();
			}
			else
			{
				CKeyValues3Cluster *pPrevClusterInFreeList = Impl().m_pFreeClusterList;
				while ( pPrevClusterInFreeList && pPrevClusterInFreeList->GetNextFreeCluster() != pCluster )
				{
					pPrevClusterInFreeList = pPrevClusterInFreeList->GetNextFreeCluster();
				}
				Assert( pPrevClusterInFreeList != NULL ); // better be in there
				if ( pPrevClusterInFreeList )
				{
					pPrevClusterInFreeList->SetNextFreeCluster( pCluster->GetNextFreeCluster() );
				}
			}

			delete pCluster;
		}
	}
	else if ( nNewAllocCount == ( KV3_CLUSTER_VALUE_COUNT - 1 ) ) // was full but now have space - NOTE: this is a little subtle because of reentrancy, we don't want to double-add a free cluster to the list
	{
		// add to the free list
		pCluster->SetNextFreeCluster( Impl().m_pFreeClusterList );
		Impl().m_pFreeClusterList = pCluster;
	}
	else
	{
		AssertDbg( !pCluster->IsEmpty() );
		AssertDbg( !pCluster->IsFull() );
	}
}


//--------------------------------------------------------------------------------------------------
// TODO: Allow for array capacity >= current count to reduce reallocations
//--------------------------------------------------------------------------------------------------
KeyValues3Array_t *CKeyValues3Context::ReallocArray( KeyValues3 *pParent, KeyValues3Array_t *pOldArray, int nNewCount )
{
	int nOldCount = pOldArray ? (int)pOldArray->m_nCount : 0;
	if ( nNewCount == nOldCount )
	{
		return pOldArray;
	}

	if ( nNewCount == 0 )
	{
		if ( pOldArray )
		{
			FreeArray( pParent, pOldArray );
			return NULL;
		}
	}

	KeyValues3Array_t *pNewArray = (KeyValues3Array_t *)malloc( sizeof( KeyValues3Array_t ) + nNewCount * sizeof(KeyValues*) );
	pNewArray->m_nCount = nNewCount;

	KeyValues3 **pNewAr = pNewArray->AsKVArray();
	KeyValues3 **pOldAr = pOldArray->AsKVArray();

	int i = 0;
	int nCopy = MIN( nNewCount, nOldCount );

	for ( ; i < nCopy; ++i )
	{
		// transfer the pointers that we can directly
		pNewAr[i] = pOldAr[i];
		pOldAr[i] = NULL;
	}

	if ( nNewCount > nOldCount )
	{
		for ( ; i < nNewCount; ++i )
		{
			// more new - fill in the new slots
			pNewAr[i] = AllocKV( pParent );
		}
	}
	else
	{
		for ( ; i < nOldCount; ++i )
		{
			// more old - free any stragglers
			FreeKV( pParent, pOldAr[ i ] );
		}
	}

	free( pOldArray );
	return pNewArray;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
KeyValues3Array_t *CKeyValues3Context::ArrayInsertMultipleBefore( KeyValues3 *pParent, KeyValues3Array_t *pOldArray, int nIndexToInsertBefore, int nInsertCount )
{
	AssertDbg( nInsertCount != 0 );
	if ( nInsertCount == 0 )
		return pOldArray;

	int nOldCount = pOldArray ? (int)pOldArray->m_nCount : 0;
	int nNewCount = nOldCount + nInsertCount;

	KeyValues3Array_t *pNewArray = (KeyValues3Array_t *)malloc( sizeof( KeyValues3Array_t ) + nNewCount * sizeof(KeyValues*) );
	pNewArray->m_nCount = nNewCount;

	KeyValues3 **pNewAr = pNewArray->AsKVArray();
	KeyValues3 **pOldAr = pOldArray->AsKVArray();

	for ( int i = 0; i < nIndexToInsertBefore; ++i )
	{
		// transfer the leading guys
		pNewAr[ i ] = pOldAr[ i ];
		pOldAr[ i ] = NULL;
	}

	for ( int i = 0; i < nInsertCount; ++i )
	{
		// fill in the new slots
		int nNewIndex = nIndexToInsertBefore + i;
		pNewAr[ nNewIndex ] = AllocKV( pParent );
	}

	int nNumAfterInsert = MAX( 0, nOldCount - nIndexToInsertBefore );
	for ( int i = 0; i < nNumAfterInsert; ++i )
	{
		// transfer the trailing guys
		int nOldIndex = nIndexToInsertBefore + i;
		int nNewIndex = nIndexToInsertBefore + i + nInsertCount;
		pNewAr[ nNewIndex ] = pOldAr[ nOldIndex ];
		pOldAr[ nOldIndex ] = NULL;
	}

	free( pOldArray );
	return pNewArray;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
KeyValues3Array_t *CKeyValues3Context::ArrayRemoveMultiple( KeyValues3 *pParent, KeyValues3Array_t *pOldArray, int nFirstIndexToRemove, int nRemoveCount )
{
	AssertDbg( nRemoveCount != 0 );
	if ( nRemoveCount == 0 )
		return pOldArray;

	int nOldCount = pOldArray ? (int)pOldArray->m_nCount : 0;
	int nOnePastLastRemoved = nFirstIndexToRemove + nRemoveCount;
	AssertDbg( nOnePastLastRemoved <= nOldCount );
	if ( nOnePastLastRemoved > nOldCount )
		return pOldArray;
	
	int nNewCount = nOldCount - nRemoveCount;
	AssertDbg( nNewCount >= 0 );

	if ( nNewCount == 0 )
	{
		if ( pOldArray )
		{
			FreeArray( pParent, pOldArray );
			return NULL;
		}
	}

	KeyValues3Array_t *pNewArray = (KeyValues3Array_t *)malloc( sizeof( KeyValues3Array_t ) + nNewCount * sizeof(KeyValues*) );
	pNewArray->m_nCount = nNewCount;

	KeyValues3 **pNewAr = pNewArray->AsKVArray();
	KeyValues3 **pOldAr = pOldArray->AsKVArray();

	for ( int i = 0; i < nFirstIndexToRemove; ++i )
	{
		// transfer the leading guys
		pNewAr[ i ] = pOldAr[ i ];
		pOldAr[ i ] = NULL;
	}

	for ( int i = 0; i < nRemoveCount; ++i )
	{
		// free the middle guys
		int nOldIndex = nFirstIndexToRemove + i;
		FreeKV( pParent, pOldAr[ nOldIndex ] );
	}

	int nNumAfterRemoved = MAX( 0, nOldCount - nOnePastLastRemoved );
	for ( int i = 0; i < nNumAfterRemoved; ++i )
	{
		// transfer the trailing guys
		int nOldIndex = nOnePastLastRemoved + i;
		int nNewIndex = nFirstIndexToRemove + i;
		pNewAr[ nNewIndex ] = pOldAr[ nOldIndex ];
		pOldAr[ nOldIndex ] = NULL;
	}

	free( pOldArray );
	return pNewArray;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CKeyValues3Context::FreeArray( KeyValues3 *pParent, KeyValues3Array_t *pArray )
{
	if ( pArray )
	{
		int nCount = (int)pArray->m_nCount;
		for ( int i = 0; i < nCount; ++i )
		{
			FreeKV( pParent, pArray->AsKVArray()[i] );
		}
		free( pArray );
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
KeyValues3BinaryBlob_t *CKeyValues3Context::AllocZeroedBlob( KeyValues3 *pParent, int nSize )
{
	if ( nSize == 0 )
	{
		return NULL;
	}

	KeyValues3BinaryBlob_t *pNewBlob = (KeyValues3BinaryBlob_t *)malloc( sizeof( KeyValues3BinaryBlob_t ) + nSize * sizeof(byte) );
	pNewBlob->m_nCount = nSize;
	V_memset( pNewBlob->AsByteArray(), 0, nSize );
	return pNewBlob;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
KeyValues3BinaryBlob_t *CKeyValues3Context::AllocBlob( KeyValues3 *pParent, const byte *pData, int nSize )
{
	if ( nSize == 0 )
	{
		return NULL;
	}

	KeyValues3BinaryBlob_t *pNewBlob = (KeyValues3BinaryBlob_t *)malloc( sizeof( KeyValues3BinaryBlob_t ) + nSize * sizeof(byte) );
	pNewBlob->m_nCount = nSize;
	V_memcpy( pNewBlob->AsByteArray(), pData, nSize );
	return pNewBlob;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
KeyValues3BinaryBlobExternal_t *CKeyValues3Context::AllocBlobExternal( KeyValues3 *pParent, const byte *pData, int nSize )
{
	if ( nSize == 0 )
	{
		return NULL;
	}

	KeyValues3BinaryBlobExternal_t *pNewBlob = (KeyValues3BinaryBlobExternal_t *)malloc( sizeof( KeyValues3BinaryBlobExternal_t ) );
	pNewBlob->m_nCount = nSize;
	pNewBlob->m_pData = pData;
	return pNewBlob;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CKeyValues3Context::FreeBlob( KeyValues3 *pParent, KeyValues3BinaryBlob_t *pBlob )
{
	if ( pBlob )
	{
		free( pBlob );
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CKeyValues3Context::FreeBlobExternal( KeyValues3 *pParent, KeyValues3BinaryBlobExternal_t *pBlob )
{
	if ( pBlob )
	{
		free( pBlob );
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
CKeyValues3Table *CKeyValues3Context::AllocTable( KeyValues3 *pParent )
{
	CKeyValues3Table *pTable = new CKeyValues3Table();
	pTable->Init( pParent );
	return pTable;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CKeyValues3Context::FreeTable( KeyValues3 *pParent, CKeyValues3Table *pTable )
{
	if ( !pTable )
		return;

	pTable->Free( pParent );
	delete pTable;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
KeyValues3ContextImpl_t &CKeyValues3Context::Impl()
{
	return *( reinterpret_cast< KeyValues3ContextImpl_t* >( &m_Impl ) );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
const KeyValues3ContextImpl_t & CKeyValues3Context::Impl() const
{
	return *( reinterpret_cast< const KeyValues3ContextImpl_t* >( &m_Impl ) );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
const char *CKeyValues3Context::AllocSharedString( KeyValues3 *pParent, const char *pStr )
{
	CKeyValues3Context *pContext = pParent->GetParentContext();
	if ( pContext )
	{
		return pContext->Impl().m_Symbols.AddString( pStr ).String();
	}
	else
	{
		return strdup( pStr );
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CKeyValues3Context::FreeSharedString( KeyValues3 *pParent, const char *pStr )
{
	CKeyValues3Context *pContext = pParent->GetParentContext();
	if ( pContext )
	{
		// no-op
	}
	else
	{
		if ( pStr )
		{
			free( (char*)pStr );
		}
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
const char *CKeyValues3Context::AllocString( KeyValues3 *pParent, const char *pStr )
{
	return strdup( pStr );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CKeyValues3Context::FreeString( KeyValues3 *pParent, const char *pStr )
{
	free( (char*)pStr );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
KeyValues3 *CKeyValues3Context::Root()
{
	return Impl().m_EmbeddedCluster.Get( EMBEDDED_CLUSTER_INDEX_OF_ROOT );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CKeyValues3Context::IsIdenticalTo( const CKeyValues3Context *pOther, bool bAssertOnFailure ) const
{
	return Root()->IsIdenticalTo( pOther->Root(), bAssertOnFailure );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CKeyValues3Context::CopyFrom( const CKeyValues3Context *pOther )
{
	Root()->CopyFrom( pOther->Root() );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CKeyValues3Context::SetMetadataEnabled( bool bMetadata )
{
	if ( Impl().m_bMetadataEnabled == bMetadata )
		return;

	Impl().m_EmbeddedCluster.SetMetadataEnabled( bMetadata );
	for ( int i = 0; i < Impl().m_AdditionalClusters.Count(); ++i )
	{
		Impl().m_AdditionalClusters[ i ]->SetMetadataEnabled( bMetadata );
	}

	Impl().m_bMetadataEnabled = bMetadata;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CKeyValues3Context::IsMetadataEnabled() const
{
	return Impl().m_bMetadataEnabled;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool IsKV3Data( void *pBuffer, int nBufLength )
{
	if ( LooksLikeKV3TextData( pBuffer, nBufLength ) )
	{
		return true;
	}
	else if ( LooksLikeKV3BinaryData( pBuffer, nBufLength ) )
	{
		return true;
	}

	return false;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool IsKV3Data( CUtlBuffer *pSrcBuffer )
{
	int nGet = pSrcBuffer->TellGet();
	int nMaxPut = pSrcBuffer->TellMaxPut();
	int nBytesLeft = nMaxPut - nGet;
	void *pBuffer = ( (byte*)pSrcBuffer->Base() + nGet );
	return IsKV3Data( pBuffer, nBytesLeft );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool LoadKV3FromFile( CKeyValues3Context *pContext, CUtlString *pOutErrorMessage, const char *pFilename, const char *pPath, const KV3ID_t &expectedFormat )
{
	if ( pOutErrorMessage )
	{
		*pOutErrorMessage = "";
	}

	CUtlBuffer buf( 0, 0, 0 );
	if ( !g_pFullFileSystem->ReadFile( pFilename, pPath, buf ) )
	{
		if ( pOutErrorMessage )
		{
			*pOutErrorMessage = "Failed to read file.";
		}
		return false;
	}

	return LoadKV3( pContext, pOutErrorMessage, &buf, expectedFormat, pFilename );
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool LoadKV3FromFile( KeyValues3 *pTargetKV3, CUtlString *pOutErrorMessage, const char *pFilename, const char *pPath, const KV3ID_t &expectedFormat )
{
	if ( pOutErrorMessage )
	{
		*pOutErrorMessage = "";
	}

	CUtlBuffer buf( 0, 0, 0 );
	if ( !g_pFullFileSystem->ReadFile( pFilename, pPath, buf ) )
	{
		if ( pOutErrorMessage )
		{
			*pOutErrorMessage = "Failed to read file.";
		}
		return false;
	}

	return LoadKV3( pTargetKV3, pOutErrorMessage, &buf, expectedFormat, pFilename );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool LooksLikeOldSchemaText( void *pData, int nBufLen )
{
	const char SCHEMA_TEXT_PREFIX[] = "<!-- schema ";
	const char *pPrefix = (const char*)( pData );
	return !V_strncmp( pPrefix, SCHEMA_TEXT_PREFIX, MIN( nBufLen, ARRAYSIZE(SCHEMA_TEXT_PREFIX)-1 ) );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool LoadKV3( CKeyValues3Context *pContext, CUtlString *pOutErrorMessage, CUtlBuffer *pSrcBuffer, const KV3ID_t &expectedFormat, const char *pReferenceFilename )
{
	return LoadKV3( pContext->Root(), pOutErrorMessage, pSrcBuffer, expectedFormat, pReferenceFilename );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool LoadKV3( KeyValues3 *pTargetKV3, CUtlString *pOutErrorMessage, CUtlBuffer *pSrcBuffer, const KV3ID_t &expectedFormat, const char *pReferenceFilename )
{
	if ( pOutErrorMessage )
	{	
		*pOutErrorMessage = "";
	}

	int nGet = pSrcBuffer->TellGet();
	int nMaxPut = pSrcBuffer->TellMaxPut();
	int nBytesLeft = nMaxPut - nGet;
	void *pBuffer = ( (byte*)pSrcBuffer->Base() + nGet );

	if ( LooksLikeOldSchemaText( pBuffer, nBytesLeft ) )
	{
		return LoadKV3FromOldSchemaText( pTargetKV3, pOutErrorMessage, pSrcBuffer, expectedFormat, pReferenceFilename );
	}
	else if ( LooksLikeKV3TextData( pBuffer, nBytesLeft ) )
	{
		return LoadKV3Text( pTargetKV3, pOutErrorMessage, (const char*)pSrcBuffer->Base(), expectedFormat, pReferenceFilename );
	}
	else if ( LooksLikeKV3BinaryData( pBuffer, nBytesLeft ) )
	{
		return LoadKV3Binary( pTargetKV3, pOutErrorMessage, pSrcBuffer, nullptr, expectedFormat, pReferenceFilename );
	}
	else
	{
		if ( pOutErrorMessage )
		{	
			*pOutErrorMessage = "Unable to determine buffer encoding.";
		}
		return false;
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool SaveKV3( const KV3ID_t &encodingId, const KV3ID_t &formatId, const KeyValues3 *pRoot, CUtlString *pOutErrorMessage, CUtlBuffer *pDestBuffer )
{
	if ( encodingId == KV3_ENCODING_TEXT )
	{
		return SaveKV3Text( encodingId, formatId, pRoot, pOutErrorMessage, pDestBuffer );
	}
	else
	{
		return SaveKV3Binary( encodingId, formatId, pRoot, pOutErrorMessage, pDestBuffer );
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool SaveKV3ToFile( const KV3ID_t &encodingId, const KV3ID_t &formatId, const KeyValues3 *pRoot, CUtlString *pOutErrorMessage, const char *pFilename, const char *pPath )
{
	CUtlBuffer buf( 0, 0, encodingId == KV3_ENCODING_TEXT ? CUtlBuffer::TEXT_BUFFER : 0 );
	if ( !SaveKV3( encodingId, formatId, pRoot, pOutErrorMessage, &buf ) )
		return false;

	if ( !g_pFullFileSystem->WriteFile( pFilename, pPath, buf ) )
	{
		if ( pOutErrorMessage )
		{
			pOutErrorMessage->Format( "Unable to write file: %s\n", pFilename );
		}
		return false;
	}

	return true;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void KeyValues3::ArraySwapItems( int nIndex1, int nIndex2 )
{
	if ( GetType() != KEYVALUES3_TYPE_ARRAY )
	{
		AssertMsg( false, "KeyValues3::ArraySwapItems - Not an array!" );
		return;
	}

	int nArrayCount = m_pArray ? (int)m_pArray->m_nCount : 0;
	if ( ( nIndex1 < 0 || nIndex1 >= nArrayCount ) || ( nIndex2 < 0 || nIndex2 >= nArrayCount ) )
	{
		AssertMsg3( false, "KeyValues3::ArraySwapItems - Bad index %d or %d (count = %d)", nIndex1, nIndex2, nArrayCount );
		return;
	}

	KeyValues3 **pArray = m_pArray->AsKVArray();
	Swap( pArray[ nIndex1 ], pArray[ nIndex2 ] );
}


//--------------------------------------------------------------------------------------------------
// [ 0, 1, 2, 3 ] ==> ArrayInsertMultipleBefore( 1, 2 ) ==> [ 0, null, null, 1, 2, 3 ]
//--------------------------------------------------------------------------------------------------
void KeyValues3::ArrayInsertMultipleBefore( int nIndexToInsertBefore, int nInsertCount )
{
	m_pArray = CKeyValues3Context::ArrayInsertMultipleBefore( this, m_pArray, nIndexToInsertBefore, nInsertCount);
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void KeyValues3::ArrayRemoveMultiple( int nFirstIndexToRemove, int nRemoveCount )
{
	m_pArray = CKeyValues3Context::ArrayRemoveMultiple( this, m_pArray, nFirstIndexToRemove, nRemoveCount);
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
KeyValues3 *KeyValues3::ArrayAddToTail()
{
	if ( GetType() != KEYVALUES3_TYPE_ARRAY )
	{
		SetToEmptyArray();
	}

	int nIdx = GetArrayElementCount();
	ArrayInsertMultipleBefore( nIdx, 1 );
	return GetArrayElement( nIdx );
}
