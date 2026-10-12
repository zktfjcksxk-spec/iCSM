//============ Copyright (c) Valve Corporation, All rights reserved. ============

#include "kv3lib/utltokenizer.h"

#include "tier0/vprof.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
// For profiling CUtlTokenizer
#if 1
#define UTL_TOKENIZER_BUDGET( X, Y )
#else
#define UTL_TOKENIZER_BUDGET( X, Y ) VPROF_BUDGET( X, Y )
#endif


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
inline bool IsDelimiter( const characterset_t &charSet, char nChar, char nPrevChar, bool bWhiteSpaceIsDelimiter )
{
	bool bUnescapedQuote = ( nPrevChar != '\\' ) && ( ( nChar == '\"' ) || ( nChar == '\'' ) );
	bool bWhitespaceOrInvalidChar = ( nChar <= ' ' );
	return ( IN_CHARACTERSET( charSet, nChar ) || bUnescapedQuote || ( bWhitespaceOrInvalidChar && bWhiteSpaceIsDelimiter ) );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
inline bool AtEndOfBuffer( CUtlBuffer *pBuffer )
{
	return ( pBuffer->GetBytesRemaining() == 0 ) || ( *(char*)pBuffer->PeekGet() == '\0' );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
CUtlTokenReference::CUtlTokenReference()
{
	m_pLocalStrCopy = NULL;
	m_pStart = NULL;
	m_pEnd = NULL;
	m_nTokenNumber = -1;
	m_nStartingLineNumber = 0;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
CUtlTokenReference::CUtlTokenReference( const CUtlTokenReference &other )
{
	m_pLocalStrCopy = NULL;
	m_pStart = other.m_pStart;
	m_pEnd = other.m_pEnd;
	m_nTokenNumber = other.m_nTokenNumber;
	m_nStartingLineNumber = other.m_nStartingLineNumber;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CUtlTokenReference::MakeInvalid()
{
	if ( m_pLocalStrCopy )
	{
		free( m_pLocalStrCopy );
	}	

	m_pLocalStrCopy = NULL;
	m_pStart = NULL;
	m_pEnd = NULL;
	m_nTokenNumber = -1;
	m_nStartingLineNumber = 0;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CUtlTokenReference::Init( int nLine, const char *pStart, const char *pEnd, int nTokenNumber )
{
	if ( m_pLocalStrCopy )
	{
		free( m_pLocalStrCopy );
	}

	m_nStartingLineNumber = nLine;
	m_pLocalStrCopy = NULL;
	m_pStart = pStart;
	m_pEnd = pEnd;
	m_nTokenNumber = nTokenNumber;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
CUtlTokenReference::~CUtlTokenReference()
{
	if ( m_pLocalStrCopy )
	{
		free( m_pLocalStrCopy );
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
int CUtlTokenReference::GetLineNumber() const
{
	return m_nStartingLineNumber;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
const char *CUtlTokenReference::AsString( ) const
{
	UTL_TOKENIZER_BUDGET( "CUtlTokenReference.AsString", "SchemaUnserialize" );
	if ( !m_pLocalStrCopy && IsValid() )
	{
		int nLen = (int)( m_pEnd - m_pStart );
		m_pLocalStrCopy = (char*)malloc( nLen + 1 );
		V_memcpy( m_pLocalStrCopy, m_pStart, nLen );
		m_pLocalStrCopy[nLen] = '\0';
	}

	return m_pLocalStrCopy;
}


CUtlStringToken CUtlTokenReference::MakeStringToken() const
{
	return ::MakeStringToken( m_pStart, m_pEnd );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CUtlTokenReference::AsString( CUtlString *pOutResult ) const
{
	int nLen = (int)( m_pEnd - m_pStart );
	pOutResult->SetDirect( m_pStart, nLen );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool UtlTokenizer_QuoteAndEscapeString( const char *pStart, CUtlString *pOutResult )
{
	*pOutResult = "\"";

	bool bNeededEscaping = false;

	for ( const char *pStr = pStart; *pStr != '\0'; pStr++ )
	{
		char c = *pStr;

		switch ( c )
		{
		case '\n':
			(*pOutResult) += "\\n";
			bNeededEscaping = true;
			break;
		case '\t':
			(*pOutResult) += "\\t";
			bNeededEscaping = true;
			break;
		case '\'':
			(*pOutResult) += "\\\'";
			bNeededEscaping = true;
			break;
		case '\"':
			(*pOutResult) += "\\\"";
			bNeededEscaping = true;
			break;
		case ' ':
			bNeededEscaping = true;
		default:
			(*pOutResult) += c;
			break;
		}
	}

	(*pOutResult) += "\"";
	return bNeededEscaping;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
static void UtlTokenizer_UnqescapeOrUnquoteString( const char *pStart, const char *pEnd, bool bPerformUnescape, CUtlString *pOutResult )
{
	*pOutResult = "";

	if ( UtlTokenizer_IsMultilineString( pStart, pEnd ) )
	{
		// Skip """ chars in the beginning and end.
		CUtlString strCopy;
		strCopy.SetDirect( pStart + 3, (int)( pEnd - pStart ) - 6 );

		// We can't be sure that \r's haven't sneaked through - we need to handle it here to make
		// the in-memory representation \r-free. (\r's will be reintroduced by the utlbuffer on
		// serialization as appropriate.)
		strCopy = strCopy.Replace( "\r\n", "\n" );

		// Strip expected leading and trailing \n's
		if ( strCopy.Length() >= 1 )
		{
			Assert( strCopy[0] == '\n' );
			strCopy = strCopy.Slice( 1 );
		}
		if ( strCopy.Length() >= 1 )
		{
			Assert( strCopy[ strCopy.Length() - 1 ] == '\n' );
			strCopy = strCopy.Slice( 0, strCopy.Length() - 1 );
		}

		// Unescape the one magic sequence
		*pOutResult = strCopy.Replace( "\\\"\"\"", "\"\"\"" );
		
		return;
	}

	bool bEscaped = false;
	for ( const char *pStr = pStart; pStr != pEnd; pStr++ )
	{
		char c = *pStr;

		// ignore leading & trailing quotes
		if ( ( pStr == pStart || pStr == (pEnd-1) ) && c == '\"' )
			continue;

		if ( bPerformUnescape && !bEscaped && c == '\\' )
		{
			bEscaped = true;
			continue;
		}
		
		if( bEscaped )
		{
			switch( c )
			{
			case 'n':
				c = '\n';
				break;
			case 't':
				c = '\t';
				break;
			}
			bEscaped = false;
		}

		(*pOutResult) += c;
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void UtlTokenizer_UnescapeString( const char *pStart, const char *pEnd, CUtlString *pOutResult )
{
	UtlTokenizer_UnqescapeOrUnquoteString( pStart, pEnd, true, pOutResult );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void UtlTokenizer_UnquoteString( const char *pStart, const char *pEnd, CUtlString *pOutResult )
{
	UtlTokenizer_UnqescapeOrUnquoteString( pStart, pEnd, false, pOutResult );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CUtlTokenReference::AsUnescapedString( CUtlString *pOutResult ) const
{
	UtlTokenizer_UnescapeString( m_pStart, m_pEnd, pOutResult );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CUtlTokenReference::AsUnquotedString( CUtlString *pOutResult ) const
{
	UtlTokenizer_UnquoteString( m_pStart, m_pEnd, pOutResult );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CUtlTokenReference::IsValid() const
{
	return ( ( m_pStart != 0 ) && ( m_pEnd != 0 ) && ( m_pStart != m_pEnd ) );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
#define TO_LOWER_CHAR( x )  (( ( x >= 'A' ) && ( x <= 'Z' ) )?( x + 32 ) : x )
#define IS_ALPHA_CHAR( x ) ( ( ( x >= 'A' ) && ( x <= 'Z' ) ) || ( ( x >= 'a' ) && ( x <= 'z' ) ) || ( x == '_' ) )
#define IS_DIGIT_CHAR( x ) ( ( ( x >= '0' ) && ( x <= '9' ) ) )
#define IS_INTEGER_CHAR( x ) ( IS_DIGIT_CHAR(x) || ( x == '+' ) || ( x == '-' ) )
#define IS_FLOAT_CHAR( x ) ( IS_INTEGER_CHAR(x) || ( x == '.' ) || ( x == 'e' ) || ( x == 'E' ) )
#define IS_IDENTIFIER_CHAR( x ) ( IS_ALPHA_CHAR(x) || ( x == ':' ) || ( x == '.' ) )


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CUtlTokenReference::MakeSafeIdentifier( CUtlString *pInOutStr )
{
	if ( pInOutStr == NULL )
		return false;

	int nLen = pInOutStr->Length();
	if ( nLen == 0 )
		return false;

	for ( int i = 0; i < nLen; ++i )
	{
		char *pChar = ( pInOutStr->GetForModify() ) + i;
		if ( IS_IDENTIFIER_CHAR(*pChar) )
			continue; // chars valid

		if ( i != 0 && IS_DIGIT_CHAR(*pChar) )
			continue; // digits allowed after first char

		// replace invalid chars
		(*pChar) = '_';
	}

	return true;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CUtlTokenReference::IsIdentifier( bool bIncludeBoolLiterals ) const
{
	if ( !IsValid() )
		return false;

	for ( const char *pChar = m_pStart; pChar != m_pEnd; ++pChar )
	{
		if ( IS_IDENTIFIER_CHAR(*pChar) )
			continue;

		if ( pChar == m_pStart || !IS_DIGIT_CHAR(*pChar) )
			return false;
	}

	if ( !bIncludeBoolLiterals && IsBool() )
	{
		// special case - 'true' and 'false' are otherwise-valid identifiers
		return false;
	}

	return true;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CUtlTokenReference::IsInteger( ) const
{
	if ( !IsValid() )
		return false;

	for ( const char *pChar = m_pStart; pChar != m_pEnd; ++pChar )
	{
		if ( !IS_INTEGER_CHAR(*pChar) )
			return false;
	}

	return true;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CUtlTokenReference::IsFloat( ) const
{
	// FIXME - Add support for NAN syntax
	// FIXME - Overly permissive (will return true for eg. "1e2e1e5e4")
	if ( !IsValid() )
		return false;

	for ( const char *pChar = m_pStart; pChar != m_pEnd; ++pChar )
	{
		if ( !IS_FLOAT_CHAR(*pChar) )
			return false;
	}

	return true;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CUtlTokenReference::IsBool() const
{
	return IsEqual( "true" ) || IsEqual( "false" );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CUtlTokenReference::IsStringLiteral() const
{
	if ( !IsValid() || (m_pEnd - m_pStart) < 2 )
		return false;

	return ( ( *m_pStart == '\"' ) && ( (*(m_pEnd-1)) == '\"' ) );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool UtlTokenizer_IsMultilineString( const char *pStart, const char *pEnd )
{
	if ( !pStart || !pEnd || (pEnd - pStart) < 6 )
		return false;

	return ( ( V_strncmp( pStart, "\"\"\"", 3 ) == 0 ) && 
			 ( V_strncmp( pEnd-3, "\"\"\"", 3 ) == 0 ) );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CUtlTokenReference::IsMultiLineStringLiteral() const
{
	if ( !IsValid() )
		return false;

	return UtlTokenizer_IsMultilineString( m_pStart, m_pEnd );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CUtlTokenReference::IsEqual( const char *pExpectedString, bool bCaseSensitive ) const
{
	if ( !IsValid() || !pExpectedString )
		return false;

	for ( const char *pChar = m_pStart; pChar != m_pEnd; )
	{
		if ( *pExpectedString == '\0' )
			return false;

		if ( bCaseSensitive )
		{
			if ( *pChar != *pExpectedString )
				return false;
		}
		else
		{
			if ( TO_LOWER_CHAR(*pChar) != TO_LOWER_CHAR(*pExpectedString) )
				return false;
		}

		pExpectedString++;
		pChar++;
	}

	return ( *pExpectedString == '\0' ); // only equal if we consumed exactly
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
CUtlTokenizer::CUtlTokenizer( )
{
	m_bParseStringsAsAtomicTokens = true;
	m_bSkipComments = true;
	m_bSkipWhitespace = true;
	m_bWhiteSpaceIsDelimiter = true;

	CharacterSetBuild( &m_TokenDelimiters, " \t\n\r" );

	m_pSrcBuffer = NULL;
	m_nLineNumberAfterLookaheadTokens = 0;
}


void CUtlTokenizer::Init( CUtlBuffer *pSrcBuffer, const char *pFilename )
{
	m_pSrcBuffer = pSrcBuffer;
	m_pSrcBuffer->SeekGet( CUtlBuffer::SEEK_HEAD, 0 );
	m_nLineNumberAfterLookaheadTokens = 1;
	m_Filename = pFilename;
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
CUtlTokenizer::CUtlTokenizer( CUtlBuffer *pSrcBuffer, const char *pFilename )
{
	m_bParseStringsAsAtomicTokens = true;
	m_bSkipComments = true;
	m_bSkipWhitespace = true;
	m_bWhiteSpaceIsDelimiter = true;

	CharacterSetBuild( &m_TokenDelimiters, " \t\n\r" );

	m_pSrcBuffer = pSrcBuffer;
	m_pSrcBuffer->SeekGet( CUtlBuffer::SEEK_HEAD, 0 );
	m_nLineNumberAfterLookaheadTokens = 1;
	m_Filename = pFilename;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CUtlTokenizer::Rewind()
{
	m_pSrcBuffer->SeekGet( CUtlBuffer::SEEK_HEAD, 0 );
	m_LookaheadTokens.RemoveAll();
	m_LastConsumedToken.MakeInvalid();
	m_nLineNumberAfterLookaheadTokens = 1;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CUtlTokenizer::SetTokenDelimiters( const char *pDelimiterChars )
{
	CharacterSetBuild( &m_TokenDelimiters, pDelimiterChars );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CUtlTokenizer::SetParseStringsAsAtomicTokens( bool bParseStringsAsAtomicTokens )
{
	m_bParseStringsAsAtomicTokens = bParseStringsAsAtomicTokens;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CUtlTokenizer::SetSkipComments( bool bSkipComments )
{
	m_bSkipComments = bSkipComments;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CUtlTokenizer::SetSkipWhitespace( bool bSkipWhitespace )
{
	m_bSkipWhitespace = bSkipWhitespace;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CUtlTokenizer::SetWhitespaceIsDelimiter( bool bWhiteSpaceIsDelimiter )
{
	m_bWhiteSpaceIsDelimiter = bWhiteSpaceIsDelimiter;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CUtlTokenizer::PeekIdentifier( int nTokenIndex )
{
	if ( !ReadUpThroughToken( nTokenIndex ) )
		return false;

	return m_LookaheadTokens[nTokenIndex].IsIdentifier();
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CUtlTokenizer::PeekAtomicToken( int nTokenIndex, const char *pExpectedString, bool bCaseSensitive )
{
	if ( !ReadUpThroughToken( nTokenIndex ) )
		return false;

	return m_LookaheadTokens[nTokenIndex].IsEqual( pExpectedString, bCaseSensitive );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CUtlTokenizer::PeekToken( int nTokenIndex, CUtlTokenReference *pOutToken )
{
	if ( !ReadUpThroughToken( nTokenIndex ) )
		return false;

	if ( pOutToken )
	{
		*pOutToken = m_LookaheadTokens[nTokenIndex];
	}

	return true;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CUtlTokenizer::ConsumeIdentifier( CUtlTokenReference *pOutToken )
{
	if ( !PeekIdentifier( 0 ) )
		return false;

	if ( pOutToken )
	{
		*pOutToken = m_LookaheadTokens[0];
	}

	m_LastConsumedToken = m_LookaheadTokens.Head();
	m_LookaheadTokens.RemoveMultipleFromHead( 1 );
	return true;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CUtlTokenizer::ConsumeArbitraryToken( CUtlTokenReference *pOutToken )
{
	if ( !ReadUpThroughToken( 0 ) )
		return false;

	if ( pOutToken )
	{
		*pOutToken = m_LookaheadTokens[0];
	}

	m_LastConsumedToken = m_LookaheadTokens.Head();
	m_LookaheadTokens.RemoveMultipleFromHead( 1 );
	return true;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CUtlTokenizer::ConsumeAtomicToken( const char *pExpectedString, bool bCaseSensitive )
{
	if ( !PeekAtomicToken( 0, pExpectedString, bCaseSensitive ) )
		return false;

	m_LastConsumedToken = m_LookaheadTokens.Head();
	m_LookaheadTokens.RemoveMultipleFromHead( 1 );
	return true;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CUtlTokenizer::ConsumeUntilAtomic( const char *pExpectedString, bool bCaseSensitive )
{
	while ( !PeekAtomicToken( 0, pExpectedString, bCaseSensitive ) )
	{
		if ( !ConsumeArbitraryToken( NULL ) )
			return false;
	}

	return ConsumeAtomicToken( pExpectedString, bCaseSensitive );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CUtlTokenizer::ReadUpThroughToken( int nTokenIndex )
{
	m_LookaheadTokens.EnsureCapacity( nTokenIndex + 1 );
	int nTokensNeeded = ( nTokenIndex + 1 ) - m_LookaheadTokens.Count();
	for ( int i = 0; i < nTokensNeeded; ++i )
	{
		if ( !ReadToken() )
		{
			return false;
		}
	}

	return true;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CUtlTokenizer::ReadToken( )
{
	UTL_TOKENIZER_BUDGET( "CUtlTokenizer.ReadToken", "SchemaUnserialize" );
	
	const char *pStart = NULL;
	const char *pEnd = NULL;
	int nStartingLine;

	static int s_nTokens = 0;
	static int s_nDebugBreakForToken = -1;
	if ( s_nTokens == s_nDebugBreakForToken )
	{
		DebuggerBreakIfDebugging();
	}

	bool bStringTokenContainsLineContinuations = false;
	if ( ReadTokenChars( &pStart, &pEnd, &bStringTokenContainsLineContinuations, &nStartingLine ) )
	{
		CUtlTokenReference &t = m_LookaheadTokens[m_LookaheadTokens.AddToTail()];
		t.Init( nStartingLine, pStart, pEnd, s_nTokens );
		s_nTokens++;

		return true;
	}
	else
	{
		return false;
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
int EatWhiteSpaceCountingLines( CUtlBuffer *pBuffer )
{
	int nLineCount = 0;
	while ( !AtEndOfBuffer( pBuffer ) )
	{
		const char *pPeekPos = (const char*)pBuffer->PeekGet( sizeof( char ), 0 );

		if ( !pPeekPos || !V_isspace( *pPeekPos ) )
			break;

		char c = pBuffer->GetChar();
		if ( c == '\n' ) // intentionally don't count \r here (just assuming we see \r with \n)
		{
			nLineCount++;
		}
	}

	return nLineCount;
}

//--------------------------------------------------------------------------------------------------
// If present, will consume either pExpected or pExpectedCR (version that contains \r character) and increment pInOutLineNumber
// Otherwise returns false
//--------------------------------------------------------------------------------------------------
bool ConsumeMultilineStringDelimiter( CUtlBuffer *pBuffer, const char *pExpected, const char *pExpectedCR, int *pInOutLineNumber )
{
	int nLen = V_strlen( pExpected );
	int nLenCR = V_strlen( pExpectedCR );

	const char *pStr = (const char*)pBuffer->PeekGet( nLen, 0 );
	const char *pStrCR = (const char*)pBuffer->PeekGet( nLenCR, 0 );

	bool bDelimiter = pStr && ( V_strncmp( pStr, pExpected, nLen ) == 0 );
	bool bDelimiterCR = pStrCR && ( V_strncmp( pStrCR, pExpectedCR, nLenCR ) == 0 );

	if ( !bDelimiter && !bDelimiterCR )
		return false;

	// Consume the delimiter
	int nConsume = bDelimiter ? nLen : nLenCR;
	pBuffer->SeekGet( CUtlBuffer::SEEK_CURRENT, nConsume );
	(*pInOutLineNumber)++; // we assume that this delimiter only corresponds to a single line
	return true;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CUtlTokenizer::ReadTokenChars( const char **pOutStart, const char **pOutEnd, bool *pOutContainsLineContinuation, int *pOutStartingLine )
{
	// skip whitespace + comments
	while ( true )
	{
		if ( AtEndOfBuffer( m_pSrcBuffer ) )
		{
			return false;
		}

		if ( m_bSkipWhitespace )
		{
			m_nLineNumberAfterLookaheadTokens += EatWhiteSpaceCountingLines( m_pSrcBuffer );
		}

		int nSkippedCLines = 0;
		if ( m_bSkipComments && m_pSrcBuffer->EatCComment( &nSkippedCLines ) )
		{
			m_nLineNumberAfterLookaheadTokens += nSkippedCLines;
		}
		else if ( m_bSkipComments && m_pSrcBuffer->EatCPPComment() )
		{
			// ate a comment
			m_nLineNumberAfterLookaheadTokens++;
		}
		else 
		{
			// not a comment - break out
			break;
		}
	}

	*pOutStartingLine = m_nLineNumberAfterLookaheadTokens;

	if ( AtEndOfBuffer( m_pSrcBuffer ) )
	{
		return false;
	}

	*pOutStart = (const char*)m_pSrcBuffer->PeekGet( sizeof( char ), 0 );
	if ( !*pOutStart )
	{
		return false;
	}
	char nFirstChar = m_pSrcBuffer->GetChar();

	// end of buffer
	if ( nFirstChar == 0 )
	{
		return false;
	}

	// handle quoted strings
	if ( m_bParseStringsAsAtomicTokens && ( nFirstChar == '\"' || nFirstChar == '\'' ) )
	{
		char nQuoteType = nFirstChar;

		//--------------------------------------------------------------------------------------------------
		// Multiline strings
		// (All of the code here is extra-sensitive to \r characters because newlines persist into the final unescaped string value.)
		if ( nQuoteType == '\"' && ConsumeMultilineStringDelimiter( m_pSrcBuffer, "\"\"\n", "\"\"\r\n", &m_nLineNumberAfterLookaheadTokens ) )
		{
			// check for special extra-short empty string
			if ( ConsumeMultilineStringDelimiter ( m_pSrcBuffer, "\"\"\"\n", "\"\"\"\r\n", &m_nLineNumberAfterLookaheadTokens ) )
			{
				*pOutEnd = (const char*)m_pSrcBuffer->PeekGet() - 1;
				return true;
			}

			bool bEscaped = false;
			// read the multiline string until we see and ending
			while( !AtEndOfBuffer( m_pSrcBuffer ) )
			{
				if ( !bEscaped && ConsumeMultilineStringDelimiter( m_pSrcBuffer, "\n\"\"\"", "\r\n\"\"\"", &m_nLineNumberAfterLookaheadTokens ) )
				{
					// End of multiline string
					*pOutEnd = (const char*)m_pSrcBuffer->PeekGet();
					return true;
				}

				char c = m_pSrcBuffer->GetChar();
				if ( c == '\n' )
				{
					m_nLineNumberAfterLookaheadTokens++;
				}
				else if ( !c )
				{
					// EOF
					return false;
				}

				bEscaped = ( c == '\\' );
			}
		}

		//--------------------------------------------------------------------------------------------------
		// Standard string / char
		bool bWarnedForNewlineInStringLiteral = false;
		bool bPrevCharWasEscaped = false;
		char prevChar = '\0';
		char prevPrevChar = '\0';
		while( !AtEndOfBuffer( m_pSrcBuffer ) )
		{
			char nCurChar = m_pSrcBuffer->GetChar();
			bool bCurrentCharIsEscaped = ( prevChar == '\\' && !bPrevCharWasEscaped );
			bool bUnescapedEndQuote = !bCurrentCharIsEscaped && ( nCurChar == nQuoteType );
			if ( !nCurChar || bUnescapedEndQuote )
			{
				*pOutEnd = (const char*)m_pSrcBuffer->PeekGet();
				return true;
			}

			bool bSlashR = ( prevChar == '\\' && nCurChar == '\r' );
			bool bSlashN = ( prevChar == '\\' && nCurChar == '\n' );
			bool bSlashRN = ( prevPrevChar == '\\' && prevChar == '\r' && nCurChar == '\n' );
			bool bRN = ( prevChar == '\r' && nCurChar == '\n' );
			if ( nCurChar == '\r' || nCurChar == '\n' )
			{
				if ( bSlashR || bSlashN || bSlashRN )
				{
					*pOutContainsLineContinuation = true;
				}
				else if ( !bWarnedForNewlineInStringLiteral )
				{
					// non-continuation newline in string literal
					TokenWarning_t &warn = m_Warnings[m_Warnings.AddToTail()];
					warn.m_nLineNumber = m_nLineNumberAfterLookaheadTokens;
					warn.m_WarningText = "Newline in string literal";
					bWarnedForNewlineInStringLiteral = true;
				}

				if ( nCurChar != '\r' || bRN ) // only treat \r\n as a single newline
				{
					m_nLineNumberAfterLookaheadTokens++;
				}
			}

			prevPrevChar = prevChar;
			prevChar = nCurChar;
			bPrevCharWasEscaped = bCurrentCharIsEscaped;
		}

		// In this case, we hit the end of the buffer before hitting the end quote
		*pOutEnd = (const char*)m_pSrcBuffer->PeekGet();
		return true;
	}

	// parse single characters
	if ( IsDelimiter( m_TokenDelimiters, nFirstChar, '\0', m_bWhiteSpaceIsDelimiter ) )
	{
		*pOutEnd = (const char*)m_pSrcBuffer->PeekGet();
		return true;
	}

	// parse a regular word
	char nPrevChar = '\0';
	while ( true )
	{
		char nCurChar = m_pSrcBuffer->GetChar();
		if ( IsDelimiter( m_TokenDelimiters, nCurChar, nPrevChar, m_bWhiteSpaceIsDelimiter ) )
		{
			m_pSrcBuffer->SeekGet( CUtlBuffer::SEEK_CURRENT, -1 );
			*pOutEnd = (const char*)m_pSrcBuffer->PeekGet();
			return true;
		}

		if ( AtEndOfBuffer( m_pSrcBuffer ) )
		{
			*pOutEnd = (const char*)m_pSrcBuffer->Base() + m_pSrcBuffer->TellGet();
			return true;
		}

		nPrevChar = nCurChar;
	}

	return false;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CUtlTokenizer::AnyTokensRemaining()
{
	return ReadUpThroughToken( 0 );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
int CUtlTokenizer::GetCurrentLineNumber()
{
	if ( m_LookaheadTokens.Count() )
	{
		return m_LookaheadTokens.Head().GetLineNumber();
	}
	else
	{
		return m_nLineNumberAfterLookaheadTokens;
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CUtlTokenizer::HasWarnings() const
{
	return ( m_Warnings.Count() > 0 );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
int CUtlTokenizer::GetWarningCount() const
{
	return m_Warnings.Count() > 0;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
const TokenWarning_t &CUtlTokenizer::GetWarning( int nIndex ) const
{
	return m_Warnings[nIndex];
}


bool CUtlTokenizer::IsDelimiterOrWhitespace( const CUtlTokenReference &token ) const
{
	for ( const char *p = token.GetStartPtr(); p < token.GetEndPtr(); ++p )
	{
		if ( *p <= ' ' || IN_CHARACTERSET( m_TokenDelimiters, *p ) )
		{
			// this is whitespace, invalid character or delimiter
		}
		else
		{
			// this isn't delimiter or whitespace
			return false;
		}
	}
	return true; // everything is whitespace or delimiter
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CUtlTokenizer::GetLastConsumedToken( CUtlTokenReference *pOutToken )
{
	if ( m_LastConsumedToken.IsValid() )
	{
		if ( pOutToken )
		{
			*pOutToken = m_LastConsumedToken;
		}
		return true;
	}
	else 
	{
		if ( pOutToken )
		{
			pOutToken->MakeInvalid();
		}
		return false;
	}
}

