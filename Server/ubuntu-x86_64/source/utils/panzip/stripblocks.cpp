#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utlvector.h>
#include <utlstack.h>
#include "tier0\commonmacros.h"
#include "tier1\keyvalues.h"
#include "filesystem.h"
#include "stripblocks.h"

struct EscapeSequence_t
{
	char nSeq;
	char nVal;
};

EscapeSequence_t g_escapeSequences[] = 
{
	{ 'a',	0x07 },
	{ 'b',	0x08 },
	{ 'f',	0x0C },
	{ 'n',	0x0A },
	{ 'r',	0x0D },
	{ 't',	0x09 },
	{ 'v',	0x0B },
	{ '\\',	0x5C },
	{ '\'',	0x27 },
	{ '\"',	0x22 },
	{ '|',  '|'}	// Pipe is token separator in cfg file, but can be escaped
};

// Tokens that can be specified as end tokens for a block, but also crop up often in regular
// text. If encountered when the block is active, end the block, otherwise passed through to output
char g_SpecialChars[] = 
{
	'\n',
	'\r',
	'\t'
};

//-----------------------------------------------------------------------------
// CStringsArray
//-----------------------------------------------------------------------------


void CStringsArray::AddString(const char *pStr )
{
	StringAndLen_t strAndLen = { _strdup( pStr ), (unsigned int)( strlen( pStr ) ) };
	m_vec.AddToTail( strAndLen );
}

void CStringsArray::Free()
{
	int nNumStr = NumStrings();

	for( int i = 0; i < nNumStr; i++ )
	{
		free( (void*)m_vec[i].pStr );
	}

	m_vec.SetCount( 0 );
}

void CStringsArray::Log()
{
	int nNumStrings = NumStrings();

	Msg( "CStringsArray: %d strings\n", nNumStrings );
	
	for ( int i = 0; i < NumStrings(); i++ )
	{
		StringAndLen_t const *pStrLen = Get( i );
		Msg( "%d: %s (%d)\n", i, pStrLen->pStr, pStrLen->nStrLen );
	}
}

//-----------------------------------------------------------------------------
// CStringsArray
//-----------------------------------------------------------------------------

void CBlockDef::Log()
{
	Msg( "CBlockDef: %s\n", m_pName );
	m_tokens[0].Log();
	m_tokens[1].Log();
}

void CBlockDef::Free()
{
	if ( m_pName )
	{
		free( (void*)m_pName );
		m_pName = nullptr;
	}

	m_tokens[0].Free();
	m_tokens[1].Free();
}

//-----------------------------------------------------------------------------
// Tokens matching
//-----------------------------------------------------------------------------

bool IsSpecialChar( char t )
{
	for ( int i = 0; i < ARRAYSIZE( g_SpecialChars ); i++ )
	{
		if ( g_SpecialChars[i] == t )
		{
			return true;
		}
	}

	return false;
}

bool IsSpecialCharsOnlyString( const char *pStr, int nLen )
{
	for ( int i = 0; i < nLen; i++ )
	{
		if( !IsSpecialChar( pStr[i] ) )
		{
			return false;
		}
	}

	return true;
}

struct BlockMatch_t
{
	int nBlock;
	CBlockDef::TokensType_t tokenType;
	int nTokenLen;
	const char *pTokenStr;
};

// GetBlockMatchingStreamPos
//	Searches all tokens, start and end, and returns largest match L
//	If L exists as both a start token, and as multiple end tokens, the priority is to end the current block, 
//	followed by starting a new block.
bool GetBlockMatchingStreamPos( BlockMatch_t *pResult, int nCurBlock, const char *pStream, unsigned int nStreamLen, CBlockDef *pBlockDefs, int nNumBlockDefs )
{
	unsigned int nLargestMatchLength = 0;
	int nMatchedBlockIdx = -1;
	CBlockDef::TokensType_t nMatchedTokenType = CBlockDef::NUM_TOKEN_TYPES;
	const char *pMatchedToken = nullptr;

	for ( int i = 0; i < nNumBlockDefs; i++ )
	{
		CBlockDef *pBlock = &pBlockDefs[i];

		for ( int j = CBlockDef::START_TOKENS; j < CBlockDef::NUM_TOKEN_TYPES; j++ )
		{
			CStringsArray *pTokens = &pBlock->m_tokens[ j ];
			int nNumTokens = pTokens->NumStrings();

			for ( int k = 0; k < nNumTokens; k++ )
			{
				const StringAndLen_t *pToken = pTokens->Get( k );

				// If we already have a longer match, then ignore this token
				if ( nLargestMatchLength > pToken->nStrLen )
				{
					continue;
				}

				// See if we have enough data left in the stream to match this token				
				if ( nStreamLen <= pToken->nStrLen )
				{
					continue;
				}

				// Try match
				if ( !strncmp( pToken->pStr, pStream, pToken->nStrLen ) )
				{
					bool bSaveThisToken = false;

					// If this is the longest match so far, save it
					if ( nLargestMatchLength < pToken->nStrLen )
					{
						bSaveThisToken = true;
					}
					else
					{
						CBlockDef::TokensType_t tokenType = (CBlockDef::TokensType_t)j;

						// Token is same length as the one currently saved. The one currently saved is either
						// the only start token that matches this string, or the end token of some block.
						if ( ( i == nCurBlock ) && ( tokenType == CBlockDef::END_TOKENS ) )
						{
							// This token would end current block, trumps all else
							bSaveThisToken = true;
						}
						else if ( tokenType == CBlockDef::START_TOKENS )
						{
							// Error if we already saved this as a start token
							if ( nMatchedTokenType == CBlockDef::START_TOKENS )
							{
								Msg( "Error: Start tokens must be unique. Multiple %s found.\n", pToken->pStr );
								exit(-1);	// returning false won't do
							}

							bSaveThisToken = true;
						}
					}

					nLargestMatchLength = pToken->nStrLen;
					nMatchedBlockIdx = i;
					nMatchedTokenType = (CBlockDef::TokensType_t)j;
					pMatchedToken = pToken->pStr;
				}
			}
		}
	}

	if ( nMatchedBlockIdx != -1 )
	{
		pResult->nBlock = nMatchedBlockIdx;
		pResult->nTokenLen = nLargestMatchLength;
		pResult->tokenType = nMatchedTokenType;
		pResult->pTokenStr = pMatchedToken;
		return true;
	}
	else
	{
		return false;
	}
}

//-----------------------------------------------------------------------------
// StripBlocks
//-----------------------------------------------------------------------------

bool StripBlocksInternal( char *pStreamOut, const char *pStream, unsigned int nStreamLen, CBlockDef *pBlockDefs, int nNumBlockDefs )
{
	CUtlStack<int> blockStack;
	blockStack.Push( -1 );
	unsigned int i, stringStartIdx;

	for ( i = 0; i < nStreamLen; )
	{
		BlockMatch_t blockMatch;
		int nNumBlanks = 0;

		// Special processing for any strings found outside blocks
		if ( (blockStack.Top() == -1) && ( (pStream[i] == '\"') || (pStream[i] == '\'') ) )
		{
			char startChar = pStream[i];
			stringStartIdx = i;

			for (;;)
			{
				pStreamOut[i] = pStream[i];
				i++;

				// Check if we're at the end of the string
				if( ( pStream[i] == startChar ) && ( pStream [i-1] != '\\' ) )
				{
					pStreamOut[i] = pStream[i];
					i++;

					break;
				}

				if ( i == nStreamLen )
				{
					Msg( "Reached end of stream without matching quote. If not bad formatting, could be because comments stripping is off.\n" );
					return false;
				}
			}
		}
		
		// Check if matching a token
		int nCurBlock = blockStack.Top();
		if ( GetBlockMatchingStreamPos( &blockMatch, nCurBlock, &pStream[i], nStreamLen - i, pBlockDefs, nNumBlockDefs ) )
		{
			nNumBlanks = blockMatch.nTokenLen;
			
			bool bInNoFurtherNestingBlock = ( nCurBlock != -1 && pBlockDefs[ nCurBlock ].m_bNoFurtherNesting );

			if ( blockMatch.tokenType == CBlockDef::END_TOKENS )
			{
				if ( blockMatch.nBlock == nCurBlock )
				{
					// Found end of current block
					blockStack.Pop();
				}
				else if ( blockMatch.nTokenLen == 1 )
				{
					if ( nCurBlock == -1 )
					{ 
						// Allow single char end tokens outside blocks to pass through to output
						nNumBlanks = 0;
					}
					else
					{
						// Just treat this as not being an end token
					}
				}
				else if( !bInNoFurtherNestingBlock )
				{
					// Error - found ending token for a block that is not the current block
					Msg( "Error: Found token %s that is not the ending token of current block\n", blockMatch.pTokenStr );
					return false;
				}
			}
			else if ( !bInNoFurtherNestingBlock )
			{
				// Start new block 
				blockStack.Push( blockMatch.nBlock );
			}
		}
		else if ( blockStack.Top() != -1 )
		{
			// Still in the cur block, keep going
			nNumBlanks = 1;
		}

		if ( nNumBlanks )
		{
			// Encountered a start/end token, or still in a block
			for(int j = 0; j < nNumBlanks; j++)
			{
				if( IsSpecialChar( pStream[i] ) )
				{
					pStreamOut[i] = pStream[i];
				}
				else
				{
					pStreamOut[i] = ' ';
				}

				i++;
			}
		}
		else
		{
			// Regular text
			pStreamOut[i] = pStream[i];
			i++;
		}
	}

	if ( i != nStreamLen )
	{
		Msg( "Error: Output contains less characters than input; parse error!\n" );
		return false;
	}
	else
	{
		return true;
	}
}

//-----------------------------------------------------------------------------
// BlockDefs from kv
//-----------------------------------------------------------------------------

bool StringsArrayFromBlockTokens(CStringsArray *pStrings, const char *pBlockTokens)
{
	// Alloc output string
	char *pStrOut = (char*)malloc( strlen( pBlockTokens ) + 1 );
	int nOutIdx = 0;

	// Extract tokens separated by '|'

	int nInputLen = strlen( pBlockTokens );
	for ( int i = 0; i < nInputLen; )
	{
		// Check escape sequences
		if ( pBlockTokens[i] == '\\' )
		{
			bool bFoundEscSeq = false;

			if( i < (nInputLen - 1) )
			{
				for ( int lp = 0; lp < ARRAYSIZE (g_escapeSequences); lp++ )
				{
					if ( pBlockTokens[ i+1 ] == g_escapeSequences[lp].nSeq )
					{
						// Found escape sequence
						pStrOut[ nOutIdx++ ] = g_escapeSequences[lp].nVal;
						bFoundEscSeq = true;
						i += 2;
						break;
					}
				}
			}

			if ( !bFoundEscSeq )
			{
				Msg( "Bad escape sequence in block token %s\n", pBlockTokens );
				return false;
			}
		}
		else
		{
			// Output char or separator
			if ( pBlockTokens[i] == '|' )
			{
				// New token
				pStrOut[ nOutIdx ] = 0;
				pStrings->AddString( pStrOut );
				nOutIdx = 0;
			}
			else
			{
				// Output char
				pStrOut[ nOutIdx++ ] = pBlockTokens[i];
			}

			i++;
		}
	}

	// Add remaining bit of input
	if ( nOutIdx > 0 )
	{
		pStrOut[ nOutIdx ] = 0;
		pStrings->AddString( pStrOut );
	}
	
	free( pStrOut );
	return true;
}

bool BlockDefsFromKeyValues( CBlockDef **ppBlockDefs, int *pNumBlockDefs, KeyValues *pKV )
{
	*ppBlockDefs = nullptr;
	*pNumBlockDefs = 0;

	KeyValues *pKeyBlockDefs = pKV->FindKey( "BlockDefs" );
	if ( !pKeyBlockDefs )
	{
		Msg( "Error: KeyValues does not contain a BlockDefs section. Are you running from the GAME folder?\n" );
		KeyValuesDumpAsDevMsg( pKV, 2, 0 );
		return false;
	}

	// Count and allocate block defs

	for ( KeyValues  *pKey = pKeyBlockDefs->GetFirstSubKey(); pKey; pKey = pKey->GetNextKey() )
    {
		(*pNumBlockDefs)++;
	}

	*ppBlockDefs = new CBlockDef[ *pNumBlockDefs ];

	// Fill in block defs
	CBlockDef *pBlockDef = (*ppBlockDefs);
	for( KeyValues *pKey = pKeyBlockDefs->GetFirstSubKey(); pKey; pKey = pKey->GetNextKey() )
	{
		pBlockDef->m_pName = _strdup( pKey->GetName() );
		
		KeyValues *pNoFurtherNesting = pKey->FindKey( "no_further_nesting" );
		pBlockDef->m_bNoFurtherNesting = ( pNoFurtherNesting && ( pNoFurtherNesting->GetInt() != 0 ) );

		KeyValues *pStart = pKey->FindKey( "start" );
		if ( !pStart )
		{
			Msg( "BlockDef %s does not contain a start key\n", pBlockDef->m_pName );
			return false;
		}

		KeyValues *pEnd = pKey->FindKey("end");
		if( !pEnd )
		{
			Msg( "BlockDef %s does not contain an end key\n", pBlockDef->m_pName );
			return false;
		}

		if ( !StringsArrayFromBlockTokens( &pBlockDef->m_tokens[0], pStart->GetString() ) ||
			 !StringsArrayFromBlockTokens( &pBlockDef->m_tokens[1], pEnd->GetString() ) )
		{
			return false;
		}

		pBlockDef++;
	}

	 return true;
}

//-----------------------------------------------------------------------------
// External interface
//-----------------------------------------------------------------------------

bool StripBlocks( const char *pSrcFilePath, const char *pDestFilePath, CBlockDef *pBlockDefs, int nNumBlockDefs )
{	
	FILE *fp = fopen( pSrcFilePath, "rb" );
	if ( !fp )
	{
		Msg( "Unable to open %s for reading: %s\n", pSrcFilePath );
		return false;
	}

	// Use fseek/ftell to get file size, even though there are caveats around this. 
	// https://wiki.sei.cmu.edu/confluence/display/c/FIO19-C.+Do+not+use+fseek%28%29+and+ftell%28%29+to+compute+the+size+of+a+regular+file
	fseek( fp, 0, SEEK_END );
	long size = ftell( fp );
	fseek( fp, 0, SEEK_SET );

	char *pBuff = (char*)malloc( size );
	fread( pBuff, 1, size, fp );
	fclose (fp);

	char *pBuffOut = (char*)malloc(size);
	memset( pBuffOut, 0, size );

	if ( nNumBlockDefs == 0 )
	{
		memcpy( pBuffOut, pBuff, size );
	}
	else if (!StripBlocksInternal( pBuffOut, pBuff, (unsigned int)size, pBlockDefs, nNumBlockDefs ) )
	{
		Msg( "Failed to strip file: %s\n", pSrcFilePath );
		return false;
	}
	
	free( pBuff );

	fp = fopen( pDestFilePath, "wb" );
	if ( !fp )
	{
		Msg( "Unable to open %s for writing: %s\n", pDestFilePath );
		return false;
	}

	fwrite( pBuffOut, 1, size, fp );
	fclose( fp );

	return true;
}