//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
//	CUtlTokenizer
//
//	A class that makes it easy to write simple parsers. Takes a CUtlBuffer and 
//	provides helper functions to treat it as a stream of delimited tokens.
//
//	Supports C and C++ style comments, multiline string literals, line number tracking, and more
//
//===============================================================================
//
//	Multiline String Format:
//
//		Normally prefixed with three double-quotes and a newline, and terminated with a newline followed by three double-quotes:
//		(Ignore the indentation tabs in these examples.)
//			
//			foo = """
//			This is a multi-
//			-line string.
//			"""
//
//		No standard escape sequences are present (or allowed) in a multiline string. (eg. \n will show up as the two-character sequence '\\' '\n')
//		The only exception is that the sequence of three quotes """ is escaped with a backslash as \"""
//
//			foo = """
//			An embedded multiline string: \"""
//			Hello, world!
//			\"""
//			"""
//
//		There is a special case to allow empty strings to not have a mysterious blank line between the open and closing delimiters.
//		This is because when hand-editing files, this:
//
//			foo = """
//			"""
//
//		Is much more natural than the technically-required:
//
//			foo = """
//			
//			"""
//
//===============================================================================

#pragma once


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
#include "tier1/characterset.h"
#include "tier1/utlvector.h"
#include "tier1/utlbuffer.h"
#include "tier1/utlstring.h"
#include "tier1/utlstringtoken.h"


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
class CUtlTokenReference
{
public:
	//--------------------------------------------------------------------------------------------------
	// Constructors / initializers
	//--------------------------------------------------------------------------------------------------
	CUtlTokenReference();
	CUtlTokenReference( const CUtlTokenReference &other );
	void MakeInvalid( );
	void Init( int nLine, const char *pStart, const char *pEnd, int nTokenNumber );
	~CUtlTokenReference();


	//--------------------------------------------------------------------------------------------------
	// Accessing the token data
	//--------------------------------------------------------------------------------------------------

	// Whether or not this refers to a real token
	bool IsValid() const;

	// Get this token as a null-terminated string (pointer valid for the lifetime of this CUtlTokenReference)
	const char *AsString() const;

	CUtlStringToken MakeStringToken() const;

	// Copy this token into a CUtlSTring
	void AsString( CUtlString *pOutResult ) const;

	// Get the line number for this token
	int GetLineNumber() const;

	// Treat the token as a string literal, and unescape it
	void AsUnescapedString( CUtlString *pOutResult ) const;

	// Treat the token as a string literal, but only unqoute it (leave '\' untouched)
	void AsUnquotedString( CUtlString *pOutResult ) const;

	// Get the first character of the token
	inline char GetFirstChar() const { return *m_pStart; }

	// Get the start of the token as a pointer into the source buffer
	inline const char *GetStartPtr() const { return m_pStart; }

	// Get the last character of the token as a pointer into the source buffer
	inline const char *GetLastPtr() const { return m_pEnd - 1; }

	// Get the pointer past the end of the token
	inline const char *GetEndPtr() const { return m_pEnd; }

	inline int GetTokenSize() const { return (int)( m_pEnd - m_pStart ); }

	//--------------------------------------------------------------------------------------------------
	// Helpers to determine the token type
	//--------------------------------------------------------------------------------------------------

	// Whether or not this token is a valid identifier (letters, underscores, ':', and '.', and non-leading digits)
	// bIncludeBool: whether to include 'true' and 'false' as identifiers
	bool IsIdentifier( bool bIncludeBoolLiterals = true ) const;

	// Whether this token is a number of some sort
	bool IsNumber() const { return IsInteger() || IsFloat(); }

	// Whether this token is an integer literal
	bool IsInteger() const;

	// Whether this token is a floating point (or double) literal
	bool IsFloat() const;

	// Case insensitive 'true' or 'false'
	bool IsBool() const;

	// Whether the
	bool IsStringLiteral() const;

	// Whether the token is a multiline string literal (delimited by triple quotes such as """foo""" and supports embedded, unescaped newlines)
	bool IsMultiLineStringLiteral() const;

	// Whether the token matches the expected string
	bool IsEqual( const char *pExpectedString, bool bCaseSensitive = false ) const;


	//--------------------------------------------------------------------------------------------------
	// Standalone helpers related to parsing
	//--------------------------------------------------------------------------------------------------

	// Ensure that the specified string is a valid identifier (replaces invalid chars with '_') will return false only for NULL / empty string
	static bool MakeSafeIdentifier( CUtlString *pInOutStr );

private:
	int m_nStartingLineNumber;
	mutable char *m_pLocalStrCopy;
	const char *m_pStart;
	const char *m_pEnd; // one past the last character of the token
	int m_nTokenNumber;
};


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
struct TokenWarning_t
{
	CUtlString m_WarningText;
	int m_nLineNumber;
};

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
class CUtlTokenizer
{
public:
	//--------------------------------------------------------------------------------------------------
	// Construction and initialization
	//--------------------------------------------------------------------------------------------------
	CUtlTokenizer( );
	CUtlTokenizer( CUtlBuffer *pSrcBuffer, const char *pFilename = "(None)" );
	void Init( CUtlBuffer *pSrcBuffer, const char *pFilename = "(None)" );

	// Set the delimiters that are considered to separate tokens
	// ( Non-whitespace delimiters will still appear in the token stream. Whitespace will not unless you call SetSkipWhitespace(false) )
	// The default is just whitespace
	// A good example string would be: " \t\n\r@()[]{}=&,:*|;'!"
	void SetTokenDelimiters( const char *pDelimiterChars );

	// Set whether or not to treat ' and " delimited strings as single tokens (true by default)
	void SetParseStringsAsAtomicTokens( bool bParseStringsAsAtomicTokens );

	// Set whether or not to treat // and /* */ as comments, and hide the commented tokens (true by default)
	void SetSkipComments( bool bSkipComments );

	// Set whether or not to skip whitespace - if false, then whitespace will appear in the token stream (true by default)
	void SetSkipWhitespace( bool bSkipWhitespace );

	// Set whether or not whitespace is an implicit delimiter regardless of the delimiter character set (true by default)
	void SetWhitespaceIsDelimiter( bool bWhiteSpaceIsDelimiter );

	//--------------------------------------------------------------------------------------------------
	// Functions related to consuming tokens
	//--------------------------------------------------------------------------------------------------

	// Consume the current token if it is a valid C identifier (see CUtlTokenReference::IsIdentifier) (returns true on success)
	bool ConsumeIdentifier( CUtlTokenReference *pOutToken );

	// Consume the current token if it matches the expected token (returns true on success)
	bool ConsumeAtomicToken( const char *pExpectedString, bool bCaseSensitive = false );

	// Consume the current token (returns true unless there were no tokens available)
	// pOutToken can be NULL to blindly eat a token
	bool ConsumeArbitraryToken( CUtlTokenReference *pOutToken );
	bool ConsumeNextToken() { return ConsumeArbitraryToken( NULL ); }

	// Consume tokens until the specified token
	bool ConsumeUntilAtomic( const char *pExpectedString, bool bCaseSensitive = false );


	//--------------------------------------------------------------------------------------------------
	// Functions related to peeking ahead in the token stream
	//--------------------------------------------------------------------------------------------------

	// Returns true if the specified lookahead token (0 = current) is a valid C identifier
	bool PeekIdentifier( int nTokenIndex );

	// Returns true if the specified lookahead token (0 = current) matches the expected string
	bool PeekAtomicToken( int nTokenIndex, const char *pExpectedString, bool bCaseSensitive = false );

	// Peek at a lookahead token (0 = current) or returns false if there aren't that many tokens left
	bool PeekToken( int nTokenIndex, CUtlTokenReference *pOutToken );

	inline const char *GetCurrentReadPointer( )
	{
		if ( ReadUpThroughToken( 0 ) )
		{
			return m_LookaheadTokens[0].GetStartPtr();
		}
		else
		{
			// no more data
			return NULL;
		}
	}

	// Get the most recently consumed token 
	bool GetLastConsumedToken( CUtlTokenReference *pOutToken );


	//--------------------------------------------------------------------------------------------------
	// Misc helper functions
	//--------------------------------------------------------------------------------------------------

	// Returns whether or not there are any more tokens in the buffer
	bool AnyTokensRemaining();

	// Rewind to the beginning of the buffer
	void Rewind();

	// Returns the line number of the current token
	int GetCurrentLineNumber();
	
	// Returns the filename associated with this tokenizer
	inline const char *GetFilename() { return m_Filename.Get(); }

	//--------------------------------------------------------------------------------------------------
	// Tokenizer warnings
	//--------------------------------------------------------------------------------------------------
	bool HasWarnings() const;
	int GetWarningCount() const;
	const TokenWarning_t &GetWarning( int nIndex ) const;

	bool IsDelimiterOrWhitespace( const CUtlTokenReference &token )const;
private:
	bool ReadUpThroughToken( int nTokens );
	bool ReadToken( );
	bool ReadTokenChars( const char **pOutStart, const char **pOutEnd, bool *pOutContainsLineContinuation, int *pOutStartingLine );

	characterset_t	m_TokenDelimiters;
	CUtlBuffer *m_pSrcBuffer;
	CUtlVectorFixedGrowable<CUtlTokenReference,8> m_LookaheadTokens;
	CUtlTokenReference m_LastConsumedToken;
	int m_nLineNumberAfterLookaheadTokens;
	CUtlString m_Filename;
	CUtlVector<TokenWarning_t> m_Warnings;
	bool m_bParseStringsAsAtomicTokens;
	bool m_bSkipComments;
	bool m_bSkipWhitespace;
	bool m_bWhiteSpaceIsDelimiter;
};

bool UtlTokenizer_QuoteAndEscapeString( const char *pStart, CUtlString *pOutResult ); // returns whether or not the string needed to be escaped
void UtlTokenizer_UnescapeString( const char *pStart, const char *pEnd, CUtlString *pOutResult );
void UtlTokenizer_UnquoteString( const char *pStart, const char *pEnd, CUtlString *pOutResult ); // strip quotes but leave '\' untouched
bool UtlTokenizer_IsMultilineString( const char *pStart, const char *pEnd );
