//============ Copyright (c) Valve Corporation, All rights reserved. ============

#include "kv3lib/keyvalues3.h"
#include "kv3lib/kv3transfer.h"

#include "keyvalues3_text.h"

#include "tier1/utlbuffer.h"
#include "kv3lib/utltokenizer.h"
#include "tier1/fmtstr.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
const char *KV3FlagToText( KeyValues3Flag_t nFlag );
KeyValues3Flag_t KV3FlagFromText( const char *pStr );


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
class CLoadKV3FromText
{
public:
	CLoadKV3FromText( KeyValues3 *pRootTarget, CUtlString *pOutErrorMessage, CUtlBuffer *pSrcBuffer, const char *pFilename );
	~CLoadKV3FromText();
	bool Parse( bool bSkipHeader );
	const V_uuid_t &LoadedFormatUUID();
	const char *LoadedFormatName();

private:
	bool ReadHeader();
	bool ReadTable( KeyValues3 *pTarget );
	bool ReadValue( KeyValues3 *pTarget );

	bool ReadArrayValue( KeyValues3 *pTarget );
	bool ReadBinaryBlobValue( KeyValues3 *pTarget );
	bool ReadLiteralValue( KeyValues3 *pTarget );

	void ReportError( const char *pError );
	void ReportErrorNoLine( const char *pError );

	bool m_bLoadedOk;
	CUtlTokenizer m_Tokenizer;
	KeyValues3 *m_pRootTarget;
	CUtlString *m_pOutErrorMessage;
	CUtlBuffer *m_pSrcBuffer;
	V_uuid_t m_LoadedFormatUUID;
	CUtlString m_LoadedFormatName;
};


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool KV3Helper_HasUTF8BOM( const char *pBuffer, int nLen )
{
	return ( nLen > 3 && (uint8)pBuffer[0] == 0xEF && (uint8)pBuffer[1] == 0xBB && (uint8)pBuffer[2] == 0xBF );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool KV3Helper_IsUTF16Buffer( const char *pBuffer, int nLen )
{
	return ( nLen > 2 && (uint8)pBuffer[0] == 0xFF && (uint8)pBuffer[1] == 0xFE );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool LooksLikeKV3TextData( void *pData, int nBufLen )
{
	const char *pChData = (const char *)pData;

	// If it's UTF8 or UTF16, just skip over the BOM (in both pointers)
	if ( KV3Helper_HasUTF8BOM( pChData, nBufLen ) )
	{
		pData = (void *)( pChData += 3 );
		nBufLen -= 3;
	}

	if ( KV3Helper_IsUTF16Buffer( pChData, nBufLen ) )
	{
		pData = (void *)( pChData += 2 );
		nBufLen -= 2;
	}

	const char KV3_TEXT_PREFIX[] = "<!-- kv3 ";
	if ( nBufLen < ARRAYSIZE(KV3_TEXT_PREFIX)-1 )
		return false;

	return ( !V_strncmp( pChData, KV3_TEXT_PREFIX, ARRAYSIZE(KV3_TEXT_PREFIX)-1 ) );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool LoadKV3Text_Internal( KeyValues3 *pRootTarget, CUtlString *pOutErrorMessage, const char *pBuffer, const char *pReferenceFilename, const KV3ID_t &expectedFormat, const KV3ID_t *pSkipHeaderAndAssumeFormat )
{
	int nLen = V_strlen( pBuffer );

	// Skip over the UTF-8 BOM if present
	if ( KV3Helper_HasUTF8BOM( pBuffer, nLen ) )
	{
		pBuffer += 3;
		nLen -= 3;
	}

	CUtlBuffer buf( pBuffer, nLen+1, CUtlBuffer::READ_ONLY | CUtlBuffer::TEXT_BUFFER );

	// Translate UTF-16 to UTF-8 before proceeding
	if ( KV3Helper_IsUTF16Buffer( pBuffer, nLen ) )
	{
		int nUTF8Len = V_UTF16ToUTF8( (uchar16*)(pBuffer+2), NULL, 0 );
		char *pUTF8Buf = new char[ nUTF8Len ];
		V_UTF16ToUTF8( (uchar16*)(pBuffer+2), pUTF8Buf, nUTF8Len );
		buf.AssumeMemory( pUTF8Buf, nUTF8Len, nUTF8Len, CUtlBuffer::READ_ONLY | CUtlBuffer::TEXT_BUFFER );
	}

	bool bSkipHeader = ( pSkipHeaderAndAssumeFormat != nullptr );

	CLoadKV3FromText loader( pRootTarget, pOutErrorMessage, &buf, pReferenceFilename );
	if ( !loader.Parse( bSkipHeader ) )
	{
		return false;
	}

	KV3ID_t loadedFormatId( loader.LoadedFormatName(), loader.LoadedFormatUUID() );
	const KV3ID_t *pDataFormat = pSkipHeaderAndAssumeFormat;
	if ( !pDataFormat )
	{
		pDataFormat = &loadedFormatId;
	}

	return ConvertKV3Format( pRootTarget, *pDataFormat, expectedFormat, pOutErrorMessage );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool LoadKV3Text( KeyValues3 *pRootTarget, CUtlString *pOutErrorMessage, const char *pBuffer, const KV3ID_t &expectedFormat, const char *pReferenceFilename )
{
	return LoadKV3Text_Internal( pRootTarget, pOutErrorMessage, pBuffer, pReferenceFilename, expectedFormat, nullptr );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool LoadKV3Text_NoHeader( KeyValues3 *pRootTarget, CUtlString *pOutErrorMessage, const char *pBuffer, const KV3ID_t &expectedFormat, const char *pReferenceFilename )
{
	return LoadKV3Text_Internal( pRootTarget, pOutErrorMessage, pBuffer, pReferenceFilename, expectedFormat, &KV3_FORMAT_GENERIC );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool LoadKV3Text( KeyValues3 *pRootTarget, CUtlString *pOutErrorMessage, CUtlBuffer *pBuffer, const KV3ID_t &expectedFormat, const char *pReferenceFilename )
{
	if ( !pBuffer->IsText() )
	{
		if ( pOutErrorMessage )
		{
			*pOutErrorMessage = "Expected CUtlBuffer::TEXT_BUFFER for LoadKV3Text";
		}
		return false;
	}

	return LoadKV3Text( pRootTarget, pOutErrorMessage, (const char*)pBuffer->Base(), expectedFormat, pReferenceFilename );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
#define ALL_KV3_TOKEN_DELIMITERS \
	KV3_TOKEN_OPEN_CURLY \
	KV3_TOKEN_CLOSE_CURLY \
	KV3_TOKEN_OPEN_SQUARE \
	KV3_TOKEN_CLOSE_SQUARE \
	KV3_TOKEN_EQUAL \
	KV3_TOKEN_ARRAY_SEPARATOR \
	KV3_TOKEN_WHITESPACE \
	KV3_TOKEN_QUOTECHARS \
	KV3_TOKEN_COLON \
	KV3_TOKEN_VALUE_TAG_COMBINER \
	KV3_TOKEN_SEMI


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
CLoadKV3FromText::CLoadKV3FromText( KeyValues3 *pRootTarget, CUtlString *pOutErrorMessage, CUtlBuffer *pSrcBuffer, const char *pFilename )
	: m_bLoadedOk( true )
	, m_Tokenizer( pSrcBuffer, pFilename )
	, m_pRootTarget( pRootTarget )
	, m_pOutErrorMessage( pOutErrorMessage )
	, m_pSrcBuffer( pSrcBuffer )
	, m_LoadedFormatUUID( NULL_UUID )
	, m_LoadedFormatName()
{
	m_Tokenizer.SetTokenDelimiters( ALL_KV3_TOKEN_DELIMITERS );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
CLoadKV3FromText::~CLoadKV3FromText()
{
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CLoadKV3FromText::ReportError( const char *pError )
{
	CUtlTokenReference nextToken;
	if ( m_Tokenizer.PeekToken( 0, &nextToken ) )
	{
		ReportErrorNoLine( CFmtStr( "Line %d at \"%s\": %s", m_Tokenizer.GetCurrentLineNumber(), nextToken.AsString(), pError ).Access() );
	}
	else
	{
		ReportErrorNoLine( CFmtStr( "Line %d: %s", m_Tokenizer.GetCurrentLineNumber(), pError ).Access() );
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CLoadKV3FromText::ReportErrorNoLine( const char *pError )
{
	m_bLoadedOk = false;

	if ( m_pOutErrorMessage )
	{
		*m_pOutErrorMessage += pError;
		*m_pOutErrorMessage += "\n";
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CLoadKV3FromText::Parse( bool bSkipHeader )
{
	m_Tokenizer.Rewind();
	
	if ( !bSkipHeader && !ReadHeader() )
	{
		ReportError( "Invalid header" );
		return false;
	}

	if ( !ReadValue( m_pRootTarget ) )
	{
		return false;
	}

	return m_bLoadedOk;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
const V_uuid_t &CLoadKV3FromText::LoadedFormatUUID()
{
	return m_LoadedFormatUUID;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
const char *CLoadKV3FromText::LoadedFormatName()
{
	return m_LoadedFormatName;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CLoadKV3FromText::ReadHeader( )
{
	// <!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->

	CUtlTokenReference encodingName;
	CUtlTokenReference encodingId;
	CUtlTokenReference formatName;
	CUtlTokenReference formatId;

#define EXPECT_HEADER_TOKEN( TOKEN ) if ( !m_Tokenizer.ConsumeAtomicToken( TOKEN ) ) { ReportError( "Bad header format (expected '" TOKEN "'" ); return false; }

	EXPECT_HEADER_TOKEN( KV3_TOKEN_BEGIN_HEADER );
	EXPECT_HEADER_TOKEN( KV3_TOKEN_HEADER_ID );

	EXPECT_HEADER_TOKEN( KV3_TOKEN_HEADER_ENCODING );
	EXPECT_HEADER_TOKEN( KV3_TOKEN_COLON );

	if ( !m_Tokenizer.ConsumeIdentifier( &encodingName ))
	{
		ReportError( "Bad header format (expected encoding name)" );
		return false;
	}

	EXPECT_HEADER_TOKEN( KV3_TOKEN_COLON );
	EXPECT_HEADER_TOKEN( KV3_TOKEN_HEADER_VERSION );

	if ( !m_Tokenizer.ConsumeAtomicToken( KV3_TOKEN_OPEN_CURLY ) ||
		 !m_Tokenizer.ConsumeArbitraryToken( &encodingId ) ||
		 !m_Tokenizer.ConsumeAtomicToken( KV3_TOKEN_CLOSE_CURLY )
		)
	{
		ReportError( "Bad header format (expected encoding uuid)" );
	}

	EXPECT_HEADER_TOKEN( KV3_TOKEN_HEADER_FORMAT );
	EXPECT_HEADER_TOKEN( KV3_TOKEN_COLON );

	if ( !m_Tokenizer.ConsumeIdentifier( &formatName ))
	{
		ReportError( "Bad header format (expected format name)" );
		return false;
	}

	EXPECT_HEADER_TOKEN( KV3_TOKEN_COLON );
	EXPECT_HEADER_TOKEN( KV3_TOKEN_HEADER_VERSION );

	if ( !m_Tokenizer.ConsumeAtomicToken( KV3_TOKEN_OPEN_CURLY ) ||
		 !m_Tokenizer.ConsumeArbitraryToken( &formatId ) ||
		 !m_Tokenizer.ConsumeAtomicToken( KV3_TOKEN_CLOSE_CURLY )
		)
	{
		ReportError( "Bad header format (expected format uuid)" );
	}

	if ( !m_Tokenizer.ConsumeAtomicToken( KV3_TOKEN_END_HEADER ) )
	{
		ReportError( "Bad header format" );
		return false;
	}

#undef  EXPECT_HEADER_TOKEN

	//--------------------------------------------------------------------------------------------------
	// validate the data

	V_uuid_t encodingUUID;
	if ( !Plat_UUIDFromString( &encodingUUID, encodingId.AsString() ) ||
		 !Plat_UUIDFromString( &m_LoadedFormatUUID, formatId.AsString() ) )
	{
		ReportError( "Bad header format (malformed UUID)" );
		return false;
	}

	m_LoadedFormatName = formatName.AsString();

	if ( V_stricmp( encodingName.AsString(), KV3_ENCODING_TEXT.m_pName  ) || encodingUUID != KV3_ENCODING_TEXT.m_UUID )
	{
		ReportError( "Bad header (unrecognized encoding specifier)" );
		return false;
	}

	return true;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CLoadKV3FromText::ReadTable( KeyValues3 *pTarget )
{
	// GRAMMAR: '{'
	if ( !m_Tokenizer.ConsumeAtomicToken( KV3_TOKEN_OPEN_CURLY ) )
	{
		ReportError( "Expected '" KV3_TOKEN_OPEN_CURLY "'" );
		return false;
	}

	//--------------------------------------------------------------------------------------------------
	// begin
	KeyValues3 *pObject = pTarget;
	Assert( pObject != NULL );
	pObject->SetToEmptyTable();

	//--------------------------------------------------------------------------------------------------
	// members
	for ( ;; )
	{
		if ( m_Tokenizer.PeekAtomicToken( 0, KV3_TOKEN_CLOSE_CURLY ) )
		{
			// GRAMMAR: '}'
			m_Tokenizer.ConsumeAtomicToken( KV3_TOKEN_CLOSE_CURLY );
			break;
		}
		else
		{
			// GRAMMAR: member_name '=' member_value
			CUtlTokenReference memberName;
			if ( !m_Tokenizer.ConsumeArbitraryToken( &memberName ) )
			{
				ReportError( CFmtStr("Expected member name or '}'").Access() );
				return false;
			}

			if ( !memberName.IsIdentifier() && !memberName.IsStringLiteral() )
			{
				ReportError( CFmtStr("Invalid member name '%s'", memberName.AsString() ).Access() );
				return false;
			}

			if ( !m_Tokenizer.ConsumeAtomicToken( KV3_TOKEN_EQUAL ) )
			{
				ReportError( CFmtStr("Expected '=' after member name '%s'", memberName.AsString()).Access() );
				return false;
			}

			// member value
			CUtlString unescapedName;
			memberName.AsUnescapedString( &unescapedName );
			KeyValues3 *pMember = pObject->SetMemberToNull( CKV3MemberName( unescapedName.Get() ) );
			if ( !ReadValue( pMember ) )
			{
				ReportError( CFmtStr("Invalid data for member '%s'", memberName.AsString()).Access() );
				return false;
			}
		}
	}

	//--------------------------------------------------------------------------------------------------
	// end
	return true;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CLoadKV3FromText::ReadValue( KeyValues3 *pTarget )
{
	// Member Value:
	// GRAMMAR: ( tag_name ('+' tag_name)* ':' )? array_value | table | member_literal

	if ( pTarget->HasMetadata() )
	{
		pTarget->Metadata_SetFileLineNumber( m_Tokenizer.GetFilename(), m_Tokenizer.GetCurrentLineNumber() );
	}

	while ( m_Tokenizer.PeekIdentifier( 0 ) &&
		    ( m_Tokenizer.PeekAtomicToken( 1, KV3_TOKEN_VALUE_TAG_COMBINER ) || m_Tokenizer.PeekAtomicToken( 1, KV3_TOKEN_COLON ) ) )
	{
		CUtlTokenReference flag_name;
		m_Tokenizer.ConsumeIdentifier( &flag_name );
		m_Tokenizer.ConsumeNextToken(); // '+' or ':'

		KeyValues3Flag_t nFlag = KV3FlagFromText( flag_name.AsString() );
		if ( nFlag == KEYVALUES3_FLAG_NONE )
		{
			ReportError( CFmtStr( "Unrecognized flag name '%s'", flag_name.AsString() ) );
			ReportError( "Expected '" KV3_TOKEN_OPEN_SQUARE "'" );
			return false;
		}

		pTarget->SetFlag( nFlag );
	}

	if ( m_Tokenizer.PeekAtomicToken( 0, KV3_TOKEN_OPEN_SQUARE ) )
	{
		// array
		return ReadArrayValue( pTarget );
	}
	else if ( m_Tokenizer.PeekAtomicToken( 0, KV3_TOKEN_BEGIN_BINARY_BLOB ) && m_Tokenizer.PeekAtomicToken( 1, KV3_TOKEN_OPEN_SQUARE ) )
	{
		// binary blob
		return ReadBinaryBlobValue( pTarget );
	}
	else if ( m_Tokenizer.PeekAtomicToken( 0, KV3_TOKEN_OPEN_CURLY ) )
	{
		// table
		return ReadTable( pTarget );
	}
	else
	{
		// literal value
		return ReadLiteralValue( pTarget );
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CLoadKV3FromText::ReadArrayValue( KeyValues3 *arrayTarget )
{
	//--------------------------------------------------------------------------------------------------
	// figure out what our end token is
	if ( !m_Tokenizer.ConsumeAtomicToken( KV3_TOKEN_OPEN_SQUARE ) )
	{
		ReportError( "Expected '" KV3_TOKEN_OPEN_SQUARE "'" );
		return false;
	}

	//--------------------------------------------------------------------------------------------------
	// loop over the array items
	arrayTarget->SetArrayElementCount( 0 );

	int nArrayIndex = 0;
	for ( ;; )
	{
		if ( m_Tokenizer.ConsumeAtomicToken( KV3_TOKEN_CLOSE_SQUARE ) )
		{
			// finished!
			break;
		}
		else
		{
			arrayTarget->SetArrayElementCount( nArrayIndex + 1 );
			KeyValues3 *pElement = arrayTarget->GetArrayElement( nArrayIndex );

			// handle the value
			if( !ReadValue( pElement ) )
			{
				ReportError( "Expected value or '" KV3_TOKEN_CLOSE_SQUARE "'" );
				return false;
			}

			// separator or end
			if ( !m_Tokenizer.ConsumeAtomicToken( KV3_TOKEN_ARRAY_SEPARATOR ) &&
				 !m_Tokenizer.PeekAtomicToken( 0, KV3_TOKEN_CLOSE_SQUARE ) )
			{
				ReportError( CFmtStr( "Expected '" KV3_TOKEN_ARRAY_SEPARATOR "' or '%s'", KV3_TOKEN_CLOSE_SQUARE ) );
				return false;
			}

			nArrayIndex++;
		}
	}

	//--------------------------------------------------------------------------------------------------
	// all done
	return true;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
static bool ByteFromToken( const CUtlTokenReference &token, byte *pOutValue )
{
	*pOutValue = 0;

	if ( token.GetTokenSize() != 2 )
	{
		return false;
	}

	char c;
	c = token.GetStartPtr()[0];

	if ( c >= '0' && c <= '9' )
	{
		*pOutValue += c - '0';
	}
	else if (c >= 'a' && c <= 'f')
	{
		*pOutValue += c - 'a' + 10;
	}
	else if (c >= 'A' && c <= 'F')
	{
		*pOutValue += c - 'A' + 10;
	}
	else
	{
		return false;
	}

	*pOutValue <<= 4;
	c = token.GetStartPtr()[1];

	if ( c >= '0' && c <= '9' )
	{
		*pOutValue += c - '0';
	}
	else if (c >= 'a' && c <= 'f')
	{
		*pOutValue += c - 'a' + 10;
	}
	else if (c >= 'A' && c <= 'F')
	{
		*pOutValue += c - 'A' + 10;
	}
	else
	{
		return false;
	}

	return true;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CLoadKV3FromText::ReadBinaryBlobValue( KeyValues3 *arrayTarget )
{
	//--------------------------------------------------------------------------------------------------
	// figure out what our end token is
	if ( !m_Tokenizer.ConsumeAtomicToken( KV3_TOKEN_BEGIN_BINARY_BLOB ) )
	{
		ReportError( "Expected '" KV3_TOKEN_BEGIN_BINARY_BLOB "'" );
		return false;
	}

	if ( !m_Tokenizer.ConsumeAtomicToken( KV3_TOKEN_OPEN_SQUARE ) )
	{
		ReportError( "Expected '" KV3_TOKEN_OPEN_SQUARE "'" );
		return false;
	}

	//--------------------------------------------------------------------------------------------------
	// loop over the array items
	
	CUtlVectorFixedGrowable< byte, 1024 > byteArr;

	for ( ;; )
	{
		if ( m_Tokenizer.ConsumeAtomicToken( KV3_TOKEN_CLOSE_SQUARE ) )
		{
			// finished!
			break;
		}
		else
		{
			CUtlTokenReference memberValue;
			if ( !m_Tokenizer.ConsumeArbitraryToken( &memberValue ) )
			{
				ReportError( "Expected token" );
				return false;
			}

			byte byteVal = 0;
			if ( !ByteFromToken( memberValue, &byteVal ) )
			{
				ReportError( "Expected hex byte (eg. 00-FF)" );
				return false;
			}

			byteArr.AddToTail( byteVal );
		}
	}

	arrayTarget->SetToBinaryBlob( byteArr.Base(), byteArr.Count() );

	//--------------------------------------------------------------------------------------------------
	// all done
	return true;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CLoadKV3FromText::ReadLiteralValue( KeyValues3 *pTarget )
{
	CUtlTokenReference memberValue;
	if ( !m_Tokenizer.ConsumeArbitraryToken( &memberValue ) )
	{
		ReportError( "Expected token" );
		return false;
	}

	if ( pTarget == NULL ) // skipping
	{
		return true;
	}

	//--------------------------------------------------------------------------------------------------

	if ( memberValue.IsBool() )
	{
		if ( memberValue.IsEqual( "true" ) )
		{
			pTarget->SetValueBool( true );
		}
		else if ( memberValue.IsEqual( "false" ) )
		{
			pTarget->SetValueBool( false );
		}
		else
		{
			ReportError( "Failed to assign bool literal value" );
		}
	}
	else if ( memberValue.IsInteger() ) 
	{
		pTarget->SetIntFromString( memberValue.AsString() );
	}
	else if ( memberValue.IsFloat() )
	{
		double doubleValue = V_atod( memberValue.AsString() );
		pTarget->SetValueDouble( doubleValue );
	}
	else if ( memberValue.IsStringLiteral() )
	{
		CUtlString strUnescaped;
		memberValue.AsUnescapedString( &strUnescaped );
		pTarget->SetValueString( strUnescaped.Get() );

		if ( memberValue.IsMultiLineStringLiteral() )
		{
			// Remember this for later
			pTarget->SetFlag( KEYVALUES3_FLAG_MULTILINE_STRING );
		}
	}
	else if ( memberValue.IsEqual( "null" ) )
	{
		pTarget->SetToNull();
	}
	else
	{
		ReportErrorNoLine( CFmtStr( "Line %d: Invalid value \"%s\"", memberValue.GetLineNumber(), memberValue.AsString() ).Access() );
		return false;
	}

	return true;
}
