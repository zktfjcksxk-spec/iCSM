//============ Copyright (c) Valve Corporation, All rights reserved. ============

#include "kv3lib/keyvalues3.h"
#include "kv3lib/kv3transfer.h"

#include "keyvalues3_text.h"

#include "tier1/utlbuffer.h"
#include "kv3lib/utltokenizer.h"
#include "tier1/fmtstr.h"
#include "kv3lib/blockcompress.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


//--------------------------------------------------------------------------------------------------
// Flag values are serialized as part of the binary format, so we make sure they're unchanged.
// (This could later be changed with a different binary encoding if we really need to.)
//--------------------------------------------------------------------------------------------------
COMPILE_TIME_ASSERT( KEYVALUES3_FLAG_RESOURCE_REFERENCE				== 1<<0 );


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
struct KV3BinaryLoadContext_t
{
	KV3BinaryLoadContext_t()
	{
		m_pOutErrorMessage = NULL;
		m_pSrcBuffer = NULL;
		m_ppStrings = NULL;
		m_pStringHashes = NULL;
		m_nNumStrings = 0;
		m_bRelyOnBufferForStorage = false;
	}

	CUtlString *m_pOutErrorMessage;
	CUtlBuffer *m_pSrcBuffer;
	const char **m_ppStrings;
	KeyValues3LowercaseHash_t *m_pStringHashes;
	int m_nNumStrings;
	bool m_bRelyOnBufferForStorage;

	const char *String( int nIndex )
	{
		if ( nIndex < 0 )
		{
			return "";
		}
		else if ( nIndex < m_nNumStrings )
		{
			return m_ppStrings[nIndex];
		}
		else
		{
			return "";
		}
	}

	KeyValues3LowercaseHash_t StringHash( int nIndex )
	{
		if ( nIndex < 0 )
		{
			return KEYVALUES3_LOWERCASE_HASH_INVALID;
		}
		else if ( nIndex < m_nNumStrings )
		{
			KeyValues3LowercaseHash_t nHash = m_pStringHashes[nIndex];
			if ( !nHash.IsValid() )
			{
				nHash = KV3MakeLowerHash( String( nIndex ) );
				m_pStringHashes[nIndex] = nHash;
			}

			return nHash;
		}
		else
		{
			return KEYVALUES3_LOWERCASE_HASH_INVALID;
		}
	}
};


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
#define CHECK_READ( num_bytes, failure_return ) \
	if ( loadContext.m_pSrcBuffer->GetBytesRemaining() < num_bytes ) \
	{ AssertDbg(false); if ( loadContext.m_pOutErrorMessage ) { *loadContext.m_pOutErrorMessage = "Unexpected end of file";  } return failure_return; }


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
KeyValues3BinaryType_t ReadTypeAndFlags( KV3BinaryLoadContext_t &loadContext, KeyValues3 *pTarget )
{
	CHECK_READ( 1, KEYVALUES3_BINARY_TYPE_INVALID );

	byte nBinaryType = loadContext.m_pSrcBuffer->GetUnsignedChar();
	if ( nBinaryType & KEYVALUES3_BINARY_TYPE_FLAG_HAS_VALUE_FLAGS )
	{
		CHECK_READ( 1, KEYVALUES3_BINARY_TYPE_INVALID );
		byte nFlags = loadContext.m_pSrcBuffer->GetUnsignedChar();
		pTarget->SetAllFlags( (KeyValues3Flag_t)nFlags );
	}

	return KeyValues3BinaryType_t( nBinaryType & ~KEYVALUES3_BINARY_TYPE_FLAG_HAS_VALUE_FLAGS );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
class CKV3ExternalDataWriter
{
public:
	static void AimAtExternalString( KeyValues3 *pTarget, const char *pString )
	{
		pTarget->Internal_AimAtExternalString( pString );
	}
	static void AimAtExternalBinaryBlob( KeyValues3 *pTarget, const byte *pData, int nSize )
	{
		pTarget->Internal_AimAtExternalBinaryBlob( pData, nSize );
	}
	static KeyValues3 *FindOrCreateMemberAimedAtExternalName( KeyValues3 *pTarget, CKV3MemberName memberNameAndHash )
	{
		return pTarget->Internal_FindOrCreateMemberAimedAtExternalName( memberNameAndHash );
	}
};


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool LoadKV3Binary_R( KV3BinaryLoadContext_t &loadContext, KeyValues3 *pTarget )
{
	KeyValues3BinaryType_t nBinaryType = ReadTypeAndFlags( loadContext, pTarget );
	if ( nBinaryType == KEYVALUES3_BINARY_TYPE_INVALID )
	{
		return false;
	}

	switch ( nBinaryType )
	{
		case KEYVALUES3_BINARY_TYPE_NULL:
		{
			pTarget->SetToNull();
			return true;
		}
		case KEYVALUES3_BINARY_TYPE_BOOL:
		{
			CHECK_READ( sizeof(unsigned char), false );
			bool bValue = loadContext.m_pSrcBuffer->GetUnsignedChar() != 0;

			pTarget->SetValueBool( bValue );
			return true;
		}
		case KEYVALUES3_BINARY_TYPE_INT64:
		{
			CHECK_READ( sizeof(int64), false );
			int64 nValue = loadContext.m_pSrcBuffer->GetInt64();

			pTarget->SetValueInt64( nValue );
			return true;
		}
		case KEYVALUES3_BINARY_TYPE_UINT64:
		{
			CHECK_READ( sizeof(uint64), false );
			uint64 nValue = loadContext.m_pSrcBuffer->GetUnsignedInt64();

			pTarget->SetValueUint64( nValue );
			return true;
		}
		case KEYVALUES3_BINARY_TYPE_DOUBLE:
		{
			CHECK_READ( sizeof(double), false );
			double doubleValue = loadContext.m_pSrcBuffer->GetDouble();

			pTarget->SetValueDouble( doubleValue );
			return true;
		}
		case KEYVALUES3_BINARY_TYPE_STRING:
		{
			CHECK_READ( sizeof(int), false );
			int nString = loadContext.m_pSrcBuffer->GetInt();

			if ( loadContext.m_bRelyOnBufferForStorage )
			{
				CKV3ExternalDataWriter::AimAtExternalString( pTarget, loadContext.String( nString ) );
			}
			else
			{
				pTarget->SetValueString( loadContext.String( nString ) );
			}
			return true;
		}
		case KEYVALUES3_BINARY_TYPE_BINARY_BLOB:
		{
			CHECK_READ( sizeof(int), false );
			int nLength = loadContext.m_pSrcBuffer->GetInt();

			CHECK_READ( nLength, false );
			byte *pBlob = (byte*)loadContext.m_pSrcBuffer->PeekGet();
			loadContext.m_pSrcBuffer->SeekGet( CUtlBuffer::SEEK_CURRENT, nLength );

			if ( loadContext.m_bRelyOnBufferForStorage )
			{
				CKV3ExternalDataWriter::AimAtExternalBinaryBlob( pTarget, pBlob, nLength );
			}
			else
			{
				pTarget->SetToBinaryBlob( pBlob, nLength );
			}
			return true;
		}
		case KEYVALUES3_BINARY_TYPE_TABLE:
		{
			CHECK_READ( sizeof(int), false );
			int nMemberCount = loadContext.m_pSrcBuffer->GetInt();
			pTarget->SetToEmptyTable();

			for ( int i = 0; i < nMemberCount; ++i )
			{
				CHECK_READ( sizeof(int), false );
				int nMemberName = loadContext.m_pSrcBuffer->GetInt();

				KeyValues3 *pMember;

				CKV3MemberName memberName( loadContext.String( nMemberName ), loadContext.StringHash( nMemberName ) );
				
				if ( loadContext.m_bRelyOnBufferForStorage )
				{
					pMember = CKV3ExternalDataWriter::FindOrCreateMemberAimedAtExternalName( pTarget, memberName );
				}
				else
				{
					pMember = pTarget->FindOrCreateMember( memberName );
				}

				if ( !LoadKV3Binary_R( loadContext, pMember ) )
				{
					return false;
				}
			}
			return true;
		}
		case KEYVALUES3_BINARY_TYPE_ARRAY:
		{
			CHECK_READ( sizeof(int), false );
			int nCount = loadContext.m_pSrcBuffer->GetInt();
			pTarget->SetArrayElementCount( nCount );

			for ( int i = 0; i < nCount; ++i )
			{
				KeyValues3 *pElement = pTarget->GetArrayElement( i );
				if ( !LoadKV3Binary_R( loadContext, pElement ) )
				{
					return false;
				}
			}
			return true;
		}

		default:
		{
			if ( loadContext.m_pOutErrorMessage )
			{
				loadContext.m_pOutErrorMessage->Format( "Unrecognized type '%d'", nBinaryType );
			}
			return false;
		}
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool LoadKV3BinaryUncompressed( KV3BinaryLoadContext_t &loadContext, KeyValues3 *pTarget, const V_uuid_t &loadedFormat, const KV3ID_t &expectedFormat )
{
	int nNumStrings = loadContext.m_pSrcBuffer->GetInt();

	// strings
	CUtlVectorFixedGrowable<KeyValues3LowercaseHash_t,256> stringHashes;
	CUtlVectorFixedGrowable<const char*,256> stringPtrs;
	stringPtrs.EnsureCapacity( nNumStrings );
	stringHashes.EnsureCapacity( nNumStrings );
	for ( int i = 0; i < nNumStrings; ++i )
	{
		const char *pStr = (const char*)( (byte*)loadContext.m_pSrcBuffer->PeekGet() );
		stringPtrs.AddToTail( pStr );
		stringHashes.AddToTail( KeyValues3LowercaseHash_t() );
		int nLen = loadContext.m_pSrcBuffer->PeekStringLength();
		loadContext.m_pSrcBuffer->SeekGet( CUtlBuffer::SEEK_CURRENT, nLen );
	}

	loadContext.m_pStringHashes = stringHashes.Base();
	loadContext.m_ppStrings = stringPtrs.Base();
	loadContext.m_nNumStrings = stringPtrs.Count();
	if ( !LoadKV3Binary_R( loadContext, pTarget ) )
	{
		return false;
	}

	// end marker (significantly increases the odds we catch corrupt/bad data)
	CHECK_READ( sizeof(uint32), false );
	uint32 nKV3BinaryIdentifier = loadContext.m_pSrcBuffer->GetUnsignedInt();
	if ( nKV3BinaryIdentifier != KV3_BINARY_ENCODING_EOF_MARKER )
	{
		if ( loadContext.m_pOutErrorMessage )
		{
			*loadContext.m_pOutErrorMessage = "Invalid data.";
		}
		return false;
	}

	KV3ID_t loadedFormatID( nullptr, loadedFormat );
	return ConvertKV3Format( pTarget, loadedFormatID, expectedFormat, loadContext.m_pOutErrorMessage );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool LooksLikeKV3BinaryData( void *pData, int nBufLen )
{
	if ( nBufLen < sizeof(uint32) )
	{
		return false;
	}

	return ( *(uint32*)pData == KV3_BINARY_ENCODING_BEGIN_MARKER );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool LoadKV3Binary( KeyValues3 *pRootTarget, CUtlString *pOutErrorMessage, CUtlBuffer *pSrcBuffer, CUtlBuffer *pLongTermScratchBuffer, const KV3ID_t &expectedFormat, const char *pReferenceFilename )
{
	KV3BinaryLoadContext_t loadContext;
	loadContext.m_pOutErrorMessage = pOutErrorMessage;
	loadContext.m_pSrcBuffer = pSrcBuffer;

	//--------------------------------------------------------------------------------------------------
	// 
	CHECK_READ( sizeof(uint32), false );
	uint32 nKV3BinaryIdentifier = pSrcBuffer->GetUnsignedInt();
	if ( nKV3BinaryIdentifier != KV3_BINARY_ENCODING_BEGIN_MARKER )
	{
		*pOutErrorMessage = "Bad header: invalid binary marker";
		return false;
	}

	//--------------------------------------------------------------------------------------------------
	// Encoding and format
	V_uuid_t loadedEncodingUUID = NULL_UUID;
	V_uuid_t loadedFormatUUID = NULL_UUID;

	CHECK_READ( sizeof(loadedEncodingUUID), false );
	pSrcBuffer->Get( &loadedEncodingUUID, sizeof(loadedEncodingUUID) );

	CHECK_READ( sizeof(loadedFormatUUID), false );
	pSrcBuffer->Get( &loadedFormatUUID, sizeof(loadedFormatUUID) );

	if ( loadedEncodingUUID != KV3_ENCODING_BINARY_UNCOMPRESSED.m_UUID && loadedEncodingUUID != KV3_ENCODING_BINARY_BLOCK_COMPRESSED.m_UUID )
	{
		*pOutErrorMessage = "Bad header: unrecognized encoding id";
		return false;
	}

	//--------------------------------------------------------------------------------------------------
	// Optional compression
	if ( loadedEncodingUUID == KV3_ENCODING_BINARY_BLOCK_COMPRESSED.m_UUID )
	{
		int nCompressedSize = pSrcBuffer->TellMaxPut() - pSrcBuffer->TellGet();
		uint32 nDecompressedSize = CBlockCompress::GetDecompressedSize( (byte*)pSrcBuffer->PeekGet(), nCompressedSize );
		if ( nDecompressedSize == 0 )
		{
			*pOutErrorMessage = "Bad header: invalid compression header";
			return false;
		}

		if ( pLongTermScratchBuffer )
		{
			pLongTermScratchBuffer->EnsureCapacity( pLongTermScratchBuffer->TellPut() + nDecompressedSize );
			byte *pTarget = (byte*)pLongTermScratchBuffer->PeekPut();

			uint32 nActualDecompressedSize = CBlockCompress::FastDecompress( (byte*)pSrcBuffer->PeekGet(), nCompressedSize, pTarget, nDecompressedSize );
			if ( nActualDecompressedSize != nDecompressedSize )
			{
				*pOutErrorMessage = "Decompression failure";
				return false;
			}

			pLongTermScratchBuffer->SeekPut( CUtlBuffer::SEEK_CURRENT, nDecompressedSize );

			loadContext.m_pSrcBuffer = pLongTermScratchBuffer; // rest of the load happens from the uncompressed buffer
			loadContext.m_bRelyOnBufferForStorage = true; // given a scratch buffer, we can point into it
			return LoadKV3BinaryUncompressed( loadContext, pRootTarget, loadedFormatUUID, expectedFormat );
		}
		else
		{
			// no scratch buffer - use temp storage
			CUtlVectorFixedGrowable<byte,1024> decompressedMemory;
			decompressedMemory.EnsureCount( nDecompressedSize );
			uint32 nActualDecompressedSize = CBlockCompress::FastDecompress( (byte*)pSrcBuffer->PeekGet(), nCompressedSize, decompressedMemory.Base(), nDecompressedSize );
			if ( nActualDecompressedSize != nDecompressedSize )
			{
				*pOutErrorMessage = "Decompression failure";
				return false;
			}

			CUtlBuffer decompressedBuffer( decompressedMemory.Base(), nActualDecompressedSize, CUtlBuffer::READ_ONLY );
			loadContext.m_pSrcBuffer = &decompressedBuffer; // rest of the load happens from the uncompressed buffer
			return LoadKV3BinaryUncompressed( loadContext, pRootTarget, loadedFormatUUID, expectedFormat );
		}
	}
	else
	{
		if ( pLongTermScratchBuffer )
		{
			if ( pLongTermScratchBuffer != pSrcBuffer )
			{
				// kinda sucks
				pLongTermScratchBuffer->CopyBuffer( *pSrcBuffer );
				loadContext.m_pSrcBuffer = pLongTermScratchBuffer;
			}
			loadContext.m_bRelyOnBufferForStorage = true;
		}

		return LoadKV3BinaryUncompressed( loadContext, pRootTarget, loadedFormatUUID, expectedFormat );
	}
}

