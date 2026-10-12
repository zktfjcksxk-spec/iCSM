//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================
//
//==================================================================================================

#include "keyvalues3_text.h"

#include "kv3lib/keyvalues3.h"
#include "tier1/utlbuffer.h"
#include "tier1/utlstring.h"
#include "tier1/floatprint.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
const int MULTIPLE_ARRAY_ENTRIES_PER_ROW = 4;
const int MULTIPLE_BLOB_ENTRIES_PER_ROW = 32;

constexpr FPPrettyPrintOptions kKV3PrettyPrintOptions = kFPPrettyPrintOptionsDefault.WithAlwaysIncludeDecimalPoint( true );

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
const char *KV3FlagToText( KeyValues3Flag_t nFlag )
{
	switch ( nFlag )
	{
	case KEYVALUES3_FLAG_RESOURCE_REFERENCE:
		return "resource";
	default:
		return NULL;
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
KeyValues3Flag_t KV3FlagFromText( const char *pStr )
{
	if ( !V_stricmp( pStr, "resource" ) )
	{
		return KEYVALUES3_FLAG_RESOURCE_REFERENCE;
	}
	else
	{
		return KEYVALUES3_FLAG_NONE;
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
static bool IsTerseType( const KeyValues3 *pData )
{
	switch ( pData->GetType() )
	{
		case KEYVALUES3_TYPE_NULL:
		case KEYVALUES3_TYPE_BOOL:
		case KEYVALUES3_TYPE_INT64:
		case KEYVALUES3_TYPE_UINT64:
		case KEYVALUES3_TYPE_DOUBLE:
			return true;
		default:
			return false;
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
static bool IsArrayOfTerseType( const KeyValues3 *pData )
{
	for ( int i = 0; i < pData->GetArrayElementCount(); ++i )
	{
		if ( !IsTerseType( pData->GetArrayElement( i ) ) )
		{
			return false;
		}
	}

	return true;
}


//--------------------------------------------------------------------------------------------------
// x
//--------------------------------------------------------------------------------------------------
static void StripTrailingZeros( char *pStr )
{
	char *p = pStr;
	char *pDot = nullptr;

	while ( *p != '\0' )
	{
		if ( *p == '.' )
		{
			pDot = p;
		}
		p++;
	}

	if ( !pDot )
		return;

	p--;
	char *pCharAfterDot = pDot + 1;
	while ( p > pCharAfterDot && *p == '0' )
	{
		*p = '\0';
		p--;
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool SaveKV3Text_R( const KeyValues3 *pData, CUtlString *pOutErrorMessage, CUtlBuffer *pDestBuffer, bool bAfterEquals )
{
	//--------------------------------------------------------------------------------------------------
	// Flags
	KeyValues3Flag_t FLAGS_OTHER_THAN_MULTILINE_STRING = KeyValues3Flag_t( ~KEYVALUES3_FLAG_MULTILINE_STRING );

	// Multiline strings are handled in the literal text format
	if ( 0 != ( pData->GetAllFlags() & FLAGS_OTHER_THAN_MULTILINE_STRING ) )
	{
		bool bFirstFlag = true;
		for ( int iFlag = 1; iFlag <= KEYVALUES3_FLAG_LAST_VALUE; iFlag <<= 1 )
		{
			if ( pData->HasFlag( (KeyValues3Flag_t)iFlag ) )
			{
				if ( !bFirstFlag )
				{
					pDestBuffer->Printf( KV3_TOKEN_VALUE_TAG_COMBINER );
				}

				const char *pFlagName = KV3FlagToText( (KeyValues3Flag_t)iFlag );
				pDestBuffer->Printf( "%s", pFlagName );

				bFirstFlag = false;
			}
		}

		pDestBuffer->Printf( KV3_TOKEN_COLON );
	}

	//--------------------------------------------------------------------------------------------------
	// Value
	switch ( pData->GetType() )
	{
		case KEYVALUES3_TYPE_NULL:
		{
			pDestBuffer->Printf( "null" );
			break;
		}
		case KEYVALUES3_TYPE_BOOL:
		{
			pDestBuffer->Printf( "%s", pData->GetValueBool() ? "true" : "false" );
			break;
		}
		case KEYVALUES3_TYPE_INT64:
		{
			pDestBuffer->Printf( "%lld", pData->GetValueInt64() );
			break;
		}
		case KEYVALUES3_TYPE_UINT64:
		{
			pDestBuffer->Printf( "%llu", pData->GetValueUint64() );
			break;
		}
		case KEYVALUES3_TYPE_DOUBLE:
		{
			char pStr[kKV3PrettyPrintOptions.RequiredBufSizeDouble()];
			V_PrintDouble( pData->GetValueDouble(), pStr, sizeof( pStr ), kKV3PrettyPrintOptions );

			pDestBuffer->PutString( pStr );
			break;
		}
		case KEYVALUES3_TYPE_STRING:
		{
			if ( pData->HasFlag( KEYVALUES3_FLAG_MULTILINE_STRING ) )
			{
				// multiline strings aren't tabbed in, and begin with """"
				pDestBuffer->EnableTabs( false );
				pDestBuffer->Printf( "\"\"\"\n" );

				// write out the lightly-escaped string
				CUtlString multilineString = pData->GetValueString();
				multilineString = multilineString.Replace( "\"\"\"", "\\\"\"\"" ); // replace """ with \"""
				pDestBuffer->PutString( multilineString.Get() );

				// end with """ on its own line and return to normal tab rules
				pDestBuffer->Printf( "\n\"\"\"" );
				pDestBuffer->EnableTabs( true );
			}
			else
			{
				pDestBuffer->PutDelimitedString( GetCStringCharConversion(), pData->GetValueString() );
			}
			break;
		}
		case KEYVALUES3_TYPE_BINARY_BLOB:
		{
			int nCount = pData->GetBinaryBlobSize();
			bool bShort = ( nCount <= MULTIPLE_BLOB_ENTRIES_PER_ROW );

			if ( bAfterEquals && !bShort )
			{
				pDestBuffer->Printf( "\n#[" );
			}
			else
			{
				pDestBuffer->Printf( "#[ " );
			}

			if ( !bShort )
			{
				pDestBuffer->PutString( "\n" );
				pDestBuffer->PushTab();
			}

			for ( int i = 0; i < nCount; ++i )
			{
				if ( i > 0 )
				{
					if ( i % MULTIPLE_BLOB_ENTRIES_PER_ROW == 0 )
					{
						pDestBuffer->Printf( "\n" );
					}
					else
					{
						pDestBuffer->Printf( " " );
					}
				}

				byte b = pData->GetBinaryBlobByte( i );
				pDestBuffer->Printf( "%02X", (int)b );
			}

			if ( !bShort )
			{
				pDestBuffer->Printf( "\n" );
				pDestBuffer->PopTab();
			}
			else
			{
				pDestBuffer->Printf( " " );
			}

			pDestBuffer->PutString( "]" );
			break;
		}

		case KEYVALUES3_TYPE_ARRAY:
		{
			int nCount = pData->GetArrayElementCount();
			bool bMultiplePerRow = IsArrayOfTerseType( pData );
			bool bSingleRow = ( nCount <= MULTIPLE_ARRAY_ENTRIES_PER_ROW ) && bMultiplePerRow;

			if ( bAfterEquals && !bSingleRow )
			{
				pDestBuffer->Printf( "\n[" );
			}
			else
			{
				pDestBuffer->Printf( "[ " );
			}

			if ( !bSingleRow )
			{
				pDestBuffer->PutString( "\n" );
				pDestBuffer->PushTab();
			}

			for ( int i = 0; i < nCount; ++i )
			{
				if ( i > 0 )
				{
					if ( !bMultiplePerRow || ( i % MULTIPLE_ARRAY_ENTRIES_PER_ROW == 0 ) )
					{
						pDestBuffer->Printf( "\n" );
					}
					else
					{
						pDestBuffer->Printf( " " );
					}
				}

				if ( !SaveKV3Text_R( pData->GetArrayElement( i ), pOutErrorMessage, pDestBuffer, false ) )
				{
					return false;
				}

				if ( !( bSingleRow && i == nCount - 1 ) ) // skip the trailing comma for short arrays
				{
					pDestBuffer->Printf( "," );
				}
			}

			if ( !bSingleRow )
			{
				pDestBuffer->Printf( "\n" );
				pDestBuffer->PopTab();
			}
			else
			{
				pDestBuffer->PutString( " " );
			}

			pDestBuffer->PutString( "]" );
			break;
		}
		case KEYVALUES3_TYPE_TABLE:
		{
			if ( bAfterEquals )
			{
				pDestBuffer->PutString( "\n" );
			}
			pDestBuffer->Printf( "{\n" );
			pDestBuffer->PushTab();
			CUtlString memberName;
			int nCount = pData->GetMemberCount();
			for ( int i = 0; i < nCount; ++i )
			{
				pData->GetMemberNameEscaped( i, &memberName );
				pDestBuffer->PutString( memberName );
				pDestBuffer->PutString( " = " );
				if ( !SaveKV3Text_R( pData->GetMember( i ), pOutErrorMessage, pDestBuffer, true ) )
				{
					return false;
				}

				pDestBuffer->PutString( "\n" );
			}
			pDestBuffer->PopTab();
			pDestBuffer->PutString( "}" );
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
bool SaveKV3Text( const KV3ID_t &encodingId, const KV3ID_t &formatId, const KeyValues3 *pRoot, CUtlString *pOutErrorMessage, CUtlBuffer *pDestBuffer )
{
	if ( !pDestBuffer->IsText() )
	{
		if ( pOutErrorMessage )
		{
			*pOutErrorMessage = "Cannot save KV3 text to a non-text buffer!";
		}
		return false;
	}

	if ( encodingId != KV3_ENCODING_TEXT )
	{
		if ( pOutErrorMessage )
		{
			*pOutErrorMessage = "Unsupported text encoding id.";
		}
		return false;
	}

	CUUIDString encodingStr( encodingId.m_UUID );
	CUUIDString formatStr( formatId.m_UUID );

	const char *pEncodingName = encodingId.m_pName;
	const char *pFormatName = formatId.m_pName;

	// eg. <!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
	pDestBuffer->Printf( "<!-- kv3 encoding:%s:version{%s} format:%s:version{%s} -->\n", pEncodingName, encodingStr.Get(), pFormatName, formatStr.Get() );

	return SaveKV3Text_R( pRoot, pOutErrorMessage, pDestBuffer, false );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool SaveKV3Text_ToString( const KV3ID_t &encodingId, const KV3ID_t &formatId, const KeyValues3 *pRoot, CUtlString *pOutErrorMessage, CUtlString *pDestString )
{
	CUtlBuffer textBuffer( 0, 0, CUtlBuffer::TEXT_BUFFER );
	if ( !SaveKV3Text( encodingId, formatId, pRoot, pOutErrorMessage, &textBuffer ) )
	{
		return false;
	}

	pDestString->Set( (const char*)textBuffer.Base() );
	return true;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool SaveKV3Text_NoHeader( const KeyValues3 *pRoot, CUtlString *pOutErrorMessage, CUtlBuffer *pDestBuffer )
{
	if ( !pDestBuffer->IsText() )
	{
		if ( pOutErrorMessage )
		{
			*pOutErrorMessage = "Cannot save KV3 text to a non-text buffer!";
		}
		return false;
	}

	return SaveKV3Text_R( pRoot, pOutErrorMessage, pDestBuffer, false );
}

