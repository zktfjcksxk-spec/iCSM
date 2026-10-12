//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================
//
//==================================================================================================

#include "tier0/platform.h"
#include "kv3lib/blockcompress.h"
#include "tier1/utlsymbollarge.h"

#include "keyvalues3_text.h"

#include "kv3lib/keyvalues3.h"

#include "tier1/utlbuffer.h"
#include "tier1/utlstring.h"


// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
struct KV3BinarySaveContext_t
{
	CUtlString *m_pOutErrorMessage;
	CUtlBuffer *m_pDestBuffer;
	CUtlSymbolTableLarge m_Symbols; // case sensitive
};


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool GatherSymbols_R( KV3BinarySaveContext_t &saveContext, const KeyValues3 *pData )
{
	switch ( pData->GetType() )
	{
		case KEYVALUES3_TYPE_STRING:
		{
			saveContext.m_Symbols.AddString( pData->GetValueString() );
			return true;
		}

		case KEYVALUES3_TYPE_ARRAY:
		{
			int nCount = pData->GetArrayElementCount();
			for ( int i = 0; i < nCount; ++i )
			{
				if ( !GatherSymbols_R( saveContext, pData->GetArrayElement( i ) ) )
				{
					return false;
				}
			}
			return true;
		}

		case KEYVALUES3_TYPE_TABLE:
		{
			int nCount = pData->GetMemberCount();
			for ( int i = 0; i < nCount; ++i )
			{
				saveContext.m_Symbols.AddString( pData->GetMemberName( i ) );

				const KeyValues3 *pMember = pData->GetMember( i );
				if ( !GatherSymbols_R( saveContext, pMember ) )
				{
					return false;
				}
			}
			return true;
		}

		default:
		{
			// other types dont have symbols
			return true;
		}
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void PutTypeAndFlags( KV3BinarySaveContext_t &saveContext, const KeyValues3 *pData, KeyValues3BinaryType_t nType )
{
	if ( pData->GetAllFlags() != KEYVALUES3_FLAG_NONE )
	{
		uint8 nFlagsByte = (uint8)pData->GetAllFlags();
		saveContext.m_pDestBuffer->PutUnsignedChar( (uint8)( nType | KEYVALUES3_BINARY_TYPE_FLAG_HAS_VALUE_FLAGS ) );
		saveContext.m_pDestBuffer->PutUnsignedChar( nFlagsByte );
	}
	else
	{
		saveContext.m_pDestBuffer->PutUnsignedChar( (uint8)nType );
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool SaveKV3Binary_R( KV3BinarySaveContext_t &saveContext, const KeyValues3 *pData )
{
	CUtlBuffer *pDestBuffer = saveContext.m_pDestBuffer;

	switch ( pData->GetType() )
	{
		case KEYVALUES3_TYPE_NULL:
		{
			PutTypeAndFlags( saveContext, pData, KEYVALUES3_BINARY_TYPE_NULL );
			break;
		}
		case KEYVALUES3_TYPE_BOOL:
		{
			PutTypeAndFlags( saveContext, pData, KEYVALUES3_BINARY_TYPE_BOOL );

			pDestBuffer->PutUnsignedChar( pData->GetValueBool() ? 1 : 0 );
			break;
		}
		case KEYVALUES3_TYPE_INT64:
		{
			PutTypeAndFlags( saveContext, pData, KEYVALUES3_BINARY_TYPE_INT64 );

			pDestBuffer->PutInt64( pData->GetValueInt64() );
			break;
		}
		case KEYVALUES3_TYPE_UINT64:
		{
			PutTypeAndFlags( saveContext, pData, KEYVALUES3_BINARY_TYPE_UINT64 );

			pDestBuffer->PutUnsignedInt64( pData->GetValueUint64() );
			break;
		}
		case KEYVALUES3_TYPE_DOUBLE:
		{
			PutTypeAndFlags( saveContext, pData, KEYVALUES3_BINARY_TYPE_DOUBLE );

			pDestBuffer->PutDouble( pData->GetValueDouble() );
			break;
		}
		case KEYVALUES3_TYPE_STRING:
		{
			PutTypeAndFlags( saveContext, pData, KEYVALUES3_BINARY_TYPE_STRING );

			int nSymbol = saveContext.m_Symbols.FindElementIndex( pData->GetValueString() );
			pDestBuffer->PutInt( nSymbol );
			break;
		}
		case KEYVALUES3_TYPE_BINARY_BLOB:
		{
			PutTypeAndFlags( saveContext, pData, KEYVALUES3_BINARY_TYPE_BINARY_BLOB );

			int nCount = pData->GetBinaryBlobSize();
			pDestBuffer->PutInt( nCount );
			pDestBuffer->Put( pData->GetBinaryBlobBase(), nCount );
			break;
		}
		case KEYVALUES3_TYPE_ARRAY:
		{
			PutTypeAndFlags( saveContext, pData, KEYVALUES3_BINARY_TYPE_ARRAY );

			int nCount = pData->GetArrayElementCount();
			pDestBuffer->PutInt( nCount );
			for ( int i = 0; i < nCount; ++i )
			{
				SaveKV3Binary_R( saveContext, pData->GetArrayElement( i ) );
			}
			break;
		}
		case KEYVALUES3_TYPE_TABLE:
		{
			PutTypeAndFlags( saveContext, pData, KEYVALUES3_BINARY_TYPE_TABLE );

			int nCount = pData->GetMemberCount();
			pDestBuffer->PutInt( nCount );
			for ( int i = 0; i < nCount; ++i )
			{
				const char *pName = pData->GetMemberName( i );
				int nNameSymbol = saveContext.m_Symbols.FindElementIndex( pName );
				pDestBuffer->PutInt( nNameSymbol );
				SaveKV3Binary_R( saveContext, pData->GetMember( i ) );
			}
			break;
		}
		default:
		{
			Assert( 0 );
			return false;
		}
	}

	return true;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool SaveKV3BinaryUncompressed( KV3BinarySaveContext_t &saveContext, const KeyValues3 *pRoot )
{
	int nNumStrings = saveContext.m_Symbols.GetNumStrings();
	saveContext.m_pDestBuffer->PutInt( nNumStrings );

	for ( int iString = 0; iString < nNumStrings; ++iString )
	{
		const char *pString = saveContext.m_Symbols.GetElementString( iString );
		saveContext.m_pDestBuffer->PutString( pString );
	}

	if ( !SaveKV3Binary_R( saveContext, pRoot ) )
	{
		return false;
	}

	saveContext.m_pDestBuffer->PutUnsignedInt( KV3_BINARY_ENCODING_EOF_MARKER );
	return true;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool SaveKV3Binary( const KV3ID_t &encodingId, const KV3ID_t &formatId, const KeyValues3 *pRoot, CUtlString *pOutErrorMessage, CUtlBuffer *pDestBuffer )
{
	if ( pDestBuffer->IsText() )
	{
		*pOutErrorMessage = "Can't write binary data to a text buffer.";
		return false;
	}

	if ( encodingId != KV3_ENCODING_BINARY_BLOCK_COMPRESSED && encodingId != KV3_ENCODING_BINARY_UNCOMPRESSED )
	{
		*pOutErrorMessage = "Unsupported binary encoding id.";
		return false;
	}
	
	//--------------------------------------------------------------------------------------------------
	// always have uncompressed marker, encoding + format
	pDestBuffer->PutUnsignedInt( KV3_BINARY_ENCODING_BEGIN_MARKER );

	V_uuid_t encodingUUID = encodingId.m_UUID;
	V_uuid_t formatUUID = formatId.m_UUID;
	pDestBuffer->Put( &encodingUUID, sizeof(encodingUUID) );
	pDestBuffer->Put( &formatUUID, sizeof(formatUUID) );

	KV3BinarySaveContext_t saveContext;
	saveContext.m_pOutErrorMessage = pOutErrorMessage;
	saveContext.m_pDestBuffer = NULL;

	if ( !GatherSymbols_R( saveContext, pRoot ) )
	{
		return false;
	}

	bool bCompress = ( encodingId == KV3_ENCODING_BINARY_BLOCK_COMPRESSED );
	if ( bCompress )
	{
		CUtlBuffer tempBuffer;
		saveContext.m_pDestBuffer = &tempBuffer;

		if ( !SaveKV3BinaryUncompressed( saveContext, pRoot ) )
		{
			return false;
		}
		
		// compress
		int nSrcBufSize = tempBuffer.TellMaxPut();
		pDestBuffer->EnsureCapacity( pDestBuffer->TellPut() + nSrcBufSize + BLOCK_COMPRESS_MAX_EXPANSION_SIZE );
		uint32 nWrittenSize = CBlockCompress::FastCompress( (byte*)tempBuffer.Base(), nSrcBufSize, (byte*)pDestBuffer->Base() + pDestBuffer->TellPut(), nSrcBufSize + BLOCK_COMPRESS_MAX_EXPANSION_SIZE );
		pDestBuffer->SeekPut( CUtlBuffer::SEEK_CURRENT, nWrittenSize );

		return true;
	}
	else
	{
		// save direct
		saveContext.m_pDestBuffer = pDestBuffer;
		return SaveKV3BinaryUncompressed( saveContext, pRoot );
	}
}
