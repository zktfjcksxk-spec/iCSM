//============ Copyright (c) Valve Corporation, All rights reserved. ============

#include "kv3lib/keyvalues3.h"
#include "kv3lib/kv3transfer.h"

#include "tier1/utlbuffer.h"
#include "kv3lib/utltokenizer.h"
#include "tier1/fmtstr.h"
#include "tier1/utlvector.h"

#include "keyvalues3_text.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
const CKV3MemberName KV3_MEMBER_NAME_WILDCARD( "*" );


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
class CLoadKV3FromKV1Text
{
public:
	CLoadKV3FromKV1Text( KeyValues3 *pRoot, CUtlString *pOutErrorMessage, CUtlBuffer *pSrcBuffer, const char *pFilename, KV1TextEscapeBehavior_t nEscapeBehavior, const KV1ToKV3Translation_t *pTranslations, int nTranslations );
	~CLoadKV3FromKV1Text();
	bool Parse();

private:
	bool ReadValue( KeyValues3 *pTarget, const KV1ToKV3Translation_t *pParentTranslation );
	bool ReadKey( CUtlString *pOutKeyName );
	bool ReadSubkeys( KeyValues3 *pTarget, const KV1ToKV3Translation_t *pParentTranslation );
	void ReportError( const char *pError );

	bool m_bLoadedOk;
	CUtlTokenizer m_Tokenizer;
	KeyValues3 *m_pRoot;
	CUtlString *m_pOutErrorMessage;
	CUtlBuffer *m_pSrcBuffer;
	KV1TextEscapeBehavior_t m_nEscapeBehavior;

	CUtlVectorFixedGrowable< CKV3MemberName, 8 > m_PathStack;

	class CTranslationPath
	{
	public:
		CTranslationPath( const char *pPath );
		bool Matches( const CKV3MemberName *pPath, int nPath ) const;
	private:
		CUtlVectorFixedGrowable< CKV3MemberName, 8 > m_Path;
		CSplitString m_SplitPath;
	};

	const KV1ToKV3Translation_t *m_pTranslations;
	int m_nTranslations;
	CUtlVectorFixedGrowable< CTranslationPath*, 8 > m_TranslationPaths;

	const KV1ToKV3Translation_t *TranslationForPath();
};


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
CLoadKV3FromKV1Text::CTranslationPath::CTranslationPath( const char *pPath )
	: m_SplitPath( pPath, "/" )
{
	m_Path.SetCount( m_SplitPath.Count() );
	for ( int iPath = 0; iPath < m_SplitPath.Count(); ++iPath )
	{
		m_Path[ iPath ] = CKV3MemberName( m_SplitPath[ iPath ] );
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CLoadKV3FromKV1Text::CTranslationPath::Matches( const CKV3MemberName *pPath, int nPath ) const
{
	if ( nPath != m_Path.Count() )
		return false;

	for ( int iPath = 0; iPath < nPath; ++iPath )
	{
		const CKV3MemberName &pathPart = m_Path[ iPath ];
		if ( pathPart != pPath[ iPath ] && pathPart != KV3_MEMBER_NAME_WILDCARD )
		{
			return false;
		}
	}

	return true;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool LoadKV3FromKV1Text_Translated( KeyValues3 *pRoot, CUtlString *pOutErrorMessage, const char *pBuffer, KV1TextEscapeBehavior_t nEscapeBehavior, const KV1ToKV3Translation_t *pTranslations, int nTranslations, const char *pReferenceFilename )
{
	int nLen = V_strlen( pBuffer );
	CUtlBuffer buf( pBuffer, nLen+1, CUtlBuffer::READ_ONLY | CUtlBuffer::TEXT_BUFFER );

	// Translate UTF-16 to UTF-8 before proceeding
	if ( KV3Helper_IsUTF16Buffer( pBuffer, nLen ) )
	{
		int nUTF8Len = V_UTF16ToUTF8( (uchar16*)(pBuffer+2), NULL, 0 );
		char *pUTF8Buf = new char[ nUTF8Len ];
		V_UTF16ToUTF8( (uchar16*)(pBuffer+2), pUTF8Buf, nUTF8Len );
		buf.AssumeMemory( pUTF8Buf, nUTF8Len, nUTF8Len, CUtlBuffer::READ_ONLY | CUtlBuffer::TEXT_BUFFER );
	}

	CLoadKV3FromKV1Text loader( pRoot, pOutErrorMessage, &buf, pReferenceFilename, nEscapeBehavior, pTranslations, nTranslations );
	return loader.Parse();
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool LoadKV3FromKV1Text( KeyValues3 *pRoot, CUtlString *pOutErrorMessage, const char *pBuffer, KV1TextEscapeBehavior_t nEscapeBehavior, const char *pReferenceFilename )
{
	return LoadKV3FromKV1Text_Translated( pRoot, pOutErrorMessage, pBuffer, nEscapeBehavior, nullptr, 0, pReferenceFilename );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool LoadKV3FromKV1File( KeyValues3 *pRoot, CUtlString *pOutErrorMessage, const char *pPath, const char *pFilename, KV1TextEscapeBehavior_t nEscapeBehavior )
{
	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );
	if ( !g_pFullFileSystem->ReadFile( pFilename, pPath, buf ) )
	{
		if ( pOutErrorMessage )
		{
			*pOutErrorMessage = "Failed to read file.";
		}
		return false;
	}
	return LoadKV3FromKV1Text( pRoot, pOutErrorMessage, (const char*)buf.Base(), nEscapeBehavior, pFilename );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
#define KV1_TOKEN_OPEN_CURLY "{"
#define KV1_TOKEN_CLOSE_CURLY "}"
#define KV1_TOKEN_WHITESPACE " \t\n"
#define KV1_TOKEN_QUOTECHARS "\'\""
#define KV1_TOKEN_ASSIGN_CHAR "="

#define ALL_KV1_TOKEN_DELIMITERS \
	KV1_TOKEN_OPEN_CURLY \
	KV1_TOKEN_CLOSE_CURLY \
	KV1_TOKEN_WHITESPACE \
	KV1_TOKEN_QUOTECHARS \
	KV1_TOKEN_ASSIGN_CHAR


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
CLoadKV3FromKV1Text::CLoadKV3FromKV1Text( KeyValues3 *pRoot, CUtlString *pOutErrorMessage, CUtlBuffer *pSrcBuffer, const char *pFilename, KV1TextEscapeBehavior_t nEscapeBehavior, const KV1ToKV3Translation_t *pTranslations, int nTranslations )
	: m_bLoadedOk( true )
	, m_Tokenizer( pSrcBuffer, pFilename )
	, m_pRoot( pRoot )
	, m_pOutErrorMessage( pOutErrorMessage )
	, m_pSrcBuffer( pSrcBuffer )
	, m_nEscapeBehavior( nEscapeBehavior )
	, m_PathStack()
	, m_pTranslations( pTranslations )
	, m_nTranslations( nTranslations )
	, m_TranslationPaths()
{
	m_Tokenizer.SetTokenDelimiters( ALL_KV1_TOKEN_DELIMITERS );

	for ( int iTranslation = 0; iTranslation < nTranslations; ++iTranslation )
	{
		m_TranslationPaths.AddToTail( new CTranslationPath( pTranslations[ iTranslation ].m_pPath ) );
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
CLoadKV3FromKV1Text::~CLoadKV3FromKV1Text()
{
	m_TranslationPaths.PurgeAndDeleteElements();
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CLoadKV3FromKV1Text::ReportError( const char *pError )
{
	m_bLoadedOk = false;

	CUtlTokenReference nextToken;

	if ( m_pOutErrorMessage )
	{
		if ( m_Tokenizer.PeekToken( 0, &nextToken ) )
		{
			*m_pOutErrorMessage += CFmtStr( "Line %d at \"%s\": ", m_Tokenizer.GetCurrentLineNumber(), nextToken.AsString() ).Access();
		}
		else
		{
			*m_pOutErrorMessage += CFmtStr( "Line %d: ", m_Tokenizer.GetCurrentLineNumber() ).Access();
		}

		*m_pOutErrorMessage += pError;
		*m_pOutErrorMessage += "\n";
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
const KV1ToKV3Translation_t *CLoadKV3FromKV1Text::TranslationForPath()
{
	for ( int iTranslation = 0; iTranslation < m_nTranslations; ++iTranslation )
	{
		const KV1ToKV3Translation_t *pTranslation = &m_pTranslations[ iTranslation ];
		const CTranslationPath *pPath = m_TranslationPaths[ iTranslation ];

		if ( pPath->Matches( m_PathStack.Base(), m_PathStack.Count() ) )
		{
			return pTranslation;
		}
	}
	/*
		const char *pCheck = pTranslation->m_pPath;
		int nIndex = 0;
		while ( *pCheck && nIndex < m_PathStack.Count() )
		{
			CKV3MemberName memberName = m_PathStack[ nIndex ];
			const char *pPathPart = memberName.m_pString;
			while ( *pCheck && *pPathPart )
			{
				if ( V_tolower_fast( *pCheck ) != V_tolower_fast( *pPathPart ) )
				{
					nIndex = -1;
					break;
				}
				++pCheck;
				++pPathPart;
			}

			if ( *pCheck && *pCheck != '.' )
			{
				nIndex = -1;
				break;
			}
			++pCheck;

			if ( nIndex == -1 )
				break;

			nIndex++;
		}

		if ( *pCheck == 0 && nIndex == m_PathStack.Count() )
		{
			return pTranslation;
		}
	}
	*/

	return nullptr;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CLoadKV3FromKV1Text::Parse()
{
	m_Tokenizer.Rewind();

	CUtlString rootKeyName;
	if ( !ReadKey( &rootKeyName ) )
	{
		return false;
	}

	const KV1ToKV3Translation_t *pRootTranslation = TranslationForPath();
	if ( !ReadValue( m_pRoot, pRootTranslation ) )
	{
		return false;
	}

	// KV3 doesn't have a place for the root key - if it's nonempty then set it as a special member on the root
	// (but not if we turned the table into something else like an array)
	if ( rootKeyName.Length() && m_pRoot->GetType() == KEYVALUES3_TYPE_TABLE )
	{
		m_pRoot->SetMemberString( ROOT_KEYNAME_MEMBER_NAME, rootKeyName.Get() );
	}

	return m_bLoadedOk;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CLoadKV3FromKV1Text::ReadKey( CUtlString *pOutKeyName )
{
	//--------------------------------------------------------------------------------------------------
	CUtlTokenReference keyName;
	m_Tokenizer.ConsumeArbitraryToken( &keyName );

	if ( keyName.IsStringLiteral() )
	{
		if ( m_nEscapeBehavior == KV1_HAS_ESCAPE_SEQUENCES )
		{
			keyName.AsUnescapedString( pOutKeyName );
		}
		else
		{
			keyName.AsUnquotedString( pOutKeyName );
		}
	}
	else if ( keyName.IsEqual( KV1_TOKEN_OPEN_CURLY ) || 
		keyName.IsEqual( KV1_TOKEN_CLOSE_CURLY ) || 
		keyName.IsEqual( KV1_TOKEN_ASSIGN_CHAR ) )
	{
		ReportError( "Bad keyname" );
		return false;
	}
	else
	{
		// this is incredibly permissive
		keyName.AsString( pOutKeyName );
	}

	return true;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CLoadKV3FromKV1Text::ReadSubkeys( KeyValues3 *pTarget, const KV1ToKV3Translation_t *pParentTranslation )
{
	//--------------------------------------------------------------------------------------------------
	// {
	//--------------------------------------------------------------------------------------------------
	if ( !m_Tokenizer.ConsumeAtomicToken( KV1_TOKEN_OPEN_CURLY ) )
	{
		ReportError( "Expected " KV1_TOKEN_OPEN_CURLY );
		return false;
	}

	//--------------------------------------------------------------------------------------------------
	// "KEY" "VALUE"
	//--------------------------------------------------------------------------------------------------
	CUtlString subkeyName;
	while ( !m_Tokenizer.PeekAtomicToken( 0, KV1_TOKEN_CLOSE_CURLY ) )
	{
		if ( !ReadKey( &subkeyName ) )
		{
			return false;
		}

		// member value

		// NOTE: we don't change PathStack after this even if we pick a different member name below,
		// because translations are authored in terms of the orignal KV names, not our translated versions
		CKV3MemberName memberName( subkeyName.Get() );
		m_PathStack.AddToTail( memberName );

		const KV1ToKV3Translation_t *pMemberTranslation = TranslationForPath();
		KeyValues3 *pMember = nullptr;

		if ( !pTarget )
		{
			pMember = nullptr;
			pMemberTranslation = nullptr;
		}
		else if ( pParentTranslation && pParentTranslation->m_nTranslationType == KV1TOKV3_TRANSLATE_SUBKEYS_INTO_ARRAY )
		{
			pMember = pTarget->ArrayAddToTail();
		}
		else if ( pParentTranslation && pParentTranslation->m_nTranslationType == KV1TOKV3_TRANSLATE_SUBKEY_NAMES_INTO_ARRAY )
		{
			pMember = pTarget->ArrayAddToTail();
			pMember->SetValueString( subkeyName.Get() );
			pMember = nullptr; // don't actually load the value
		}
		else if ( pMemberTranslation )
		{
			switch ( pMemberTranslation->m_nTranslationType )
			{
				// the value of this key is a table that we will turn into an array (and we might want to rename this key)
				case KV1TOKV3_TRANSLATE_SUBKEYS_INTO_ARRAY:
				case KV1TOKV3_TRANSLATE_SUBKEY_NAMES_INTO_ARRAY:
				{
					if ( pMemberTranslation->m_pNewKeyName )
					{
						memberName = CKV3MemberName( pMemberTranslation->m_pNewKeyName );
					}
					pMember = pTarget->FindOrCreateMember( memberName );
					// leave it untouched? (this is a little subtle either way)
					break;
				}
				
				// all instances of this key should be coalesced into an array
				case KV1TOKV3_TRANSLATE_DUPLICATE_KEY_INTO_ARRAY:
				{
					if ( pMemberTranslation->m_pNewKeyName )
					{
						memberName = CKV3MemberName( pMemberTranslation->m_pNewKeyName );
					}
					KeyValues3 *pArray = pTarget->FindOrCreateMember( memberName );
					pMember = pArray->ArrayAddToTail();
					break;
				}

				case KV1TOKV3_TRANSLATE_UNIQIFY_KEYS:
				{
					bool bCreated = false;
					pMember = pTarget->FindOrCreateMember( memberName, &bCreated );

					if ( !bCreated )
					{
						int nIndex = 1;
						CFmtStr uniqueNameStr;
						CKV3MemberName uniqueName( "" );
						do 
						{
							// iterate looking for the first "membername#123" we can find
							uniqueNameStr.Format( "%s#%d", memberName.m_pString, nIndex );
							uniqueName = CKV3MemberName( uniqueNameStr.Get() );
							pMember = pTarget->FindOrCreateMember( uniqueName, &bCreated );
						} while ( !bCreated );
					}
					break;
				}

				default:
				{
					AssertMsg1( false, "Bad translation type: %d", (int)pMemberTranslation->m_nTranslationType );
				}
			}
		}
		else
		{
			pMember = pTarget->FindOrCreateMember( memberName );
			pMember->SetToNull(); // default behavior is to stomp any existing value
		}

		if ( !ReadValue( pMember, pMemberTranslation ) )
		{
			return false;
		}

		m_PathStack.RemoveMultipleFromTail( 1 );
	}

	//--------------------------------------------------------------------------------------------------
	// }
	//--------------------------------------------------------------------------------------------------
	if ( !m_Tokenizer.ConsumeAtomicToken( KV1_TOKEN_CLOSE_CURLY ) )
	{
		ReportError( "Expected " KV1_TOKEN_CLOSE_CURLY );
		return false;
	}

	return true;
}


//--------------------------------------------------------------------------------------------------
// Copied from KeyValues::RecursiveLoadFromBuffer()
//--------------------------------------------------------------------------------------------------
static void SetKV3ValueUsingKV1Logic( KeyValues3 *pTarget, const char *value )
{
	int len = V_strlen( value );

	// Here, let's determine if we got a float or an int....
	char* pIEnd;	// pos where int scan ended
	char* pFEnd;	// pos where float scan ended
	const char* pSEnd = value + len ; // pos where token ends

	int64 ival = V_strtoi64( value, &pIEnd, 10 );
    // We parse an int64 but we check against the 32-bit limits since
    // that is what we're looking for.  The int64 parsing also
	// makes it easy to detect overflow values.
	bool bIntOverflow = ( ival > INT32_MAX || ival < INT32_MIN );

#ifdef CSTRIKE15 // $$$REI
	errno = 0;
	double doubleVal = strtod( value, &pFEnd );
	float fval = (float)doubleVal;
	bool bFloatOverOrUnderflow = ( errno == ERANGE );
#else
	V_SetErrno( 0 );
	double doubleVal = V_strtod( value, &pFEnd );
	float fval = ( float )doubleVal;
	bool bFloatOverOrUnderflow = ( V_GetErrno() == ERANGE );
#endif

#ifdef POSIX
	// strtod supports hex representation in strings under posix but we DON'T
	// want that support in keyvalues, so undo it here if needed
	if ( len > 1 &&  tolower(value[1]) == 'x' )
	{
		fval = 0.0f;
		pFEnd = (char *)value;
	}
#endif
				
	if ( *value == 0 )
	{
		pTarget->SetValueString( value );
	}
	else if ( ( 18 == len ) && ( value[0] == '0' ) && ( value[1] == 'x' ) )
	{
		// an 18-byte value prefixed with "0x" (followed by 16 hex digits) is an int64 value
		int64 retVal = 0;
		for( int i=2; i < 2 + 16; i++ )
		{
			char digit = value[i];
			if ( digit >= 'a' )
			{
				digit -= 'a' - ( '9' + 1 );
			}
			else
			{
				if ( digit >= 'A' )
				{
					digit -= 'A' - ( '9' + 1 );
				}
			}
			retVal = ( retVal * 16 ) + ( digit - '0' );
		}
		pTarget->SetValueUint64( retVal );
	}
	else if ( (pFEnd > pIEnd) && (pFEnd == pSEnd) && !bFloatOverOrUnderflow )
	{
		pTarget->SetValueFloat( fval );
	}
	else if (pIEnd == pSEnd && !bIntOverflow)
	{
		pTarget->SetValueInt64( ival );
	}
	else
	{
		pTarget->SetValueString( value );
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CLoadKV3FromKV1Text::ReadValue( KeyValues3 *pTarget, const KV1ToKV3Translation_t *pParentTranslation )
{
	if ( pTarget && pTarget->HasMetadata() )
	{
		pTarget->Metadata_SetFileLineNumber( m_Tokenizer.GetFilename(), m_Tokenizer.GetCurrentLineNumber() );
	}

	if ( m_Tokenizer.PeekAtomicToken( 0, KV1_TOKEN_OPEN_CURLY ) )
	{
		return ReadSubkeys( pTarget, pParentTranslation );
	}

	CUtlTokenReference valueToken;
	if ( !m_Tokenizer.ConsumeArbitraryToken( &valueToken ) )
	{
		ReportError( "Expected value." );
		return false;
	}

	if ( valueToken.IsEqual( KV1_TOKEN_ASSIGN_CHAR ) )
	{
		if ( !m_Tokenizer.ConsumeArbitraryToken( &valueToken ) )
		{
			ReportError( "Expected value after " KV1_TOKEN_ASSIGN_CHAR );
			return false;
		}
	}

	if ( valueToken.IsStringLiteral() )
	{
		CUtlString tokenValue;
		if ( m_nEscapeBehavior == KV1_HAS_ESCAPE_SEQUENCES )
		{
			valueToken.AsUnescapedString( &tokenValue );
		}
		else
		{
			valueToken.AsUnquotedString( &tokenValue );
		}

		if ( pTarget )
		{
			SetKV3ValueUsingKV1Logic( pTarget, tokenValue );
		}
	}
	else if ( valueToken.IsEqual( KV1_TOKEN_OPEN_CURLY ) || 
		valueToken.IsEqual( KV1_TOKEN_CLOSE_CURLY ) || 
		valueToken.IsEqual( KV1_TOKEN_ASSIGN_CHAR ) )
	{
		ReportError( "Bad value" );
		return false;
	}
	else
	{
		// be as permissive as kv1
		if ( pTarget )
		{
			pTarget->SetValueString( valueToken.AsString() );
		}
	}

	return true;
}
