//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx_client.h"
#include "textinput_suggest.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

// USE_PICKLING means to use the optimized/compiled radix trie.
// turning off this define only works when running out of perforce.
// (the required non-pickled files are not in the client package.)
#define USE_PICKLING

//-----------------------------------------------------------------------------
// Purpose: Lessfunc to sort candidates by probability
//-----------------------------------------------------------------------------
bool UtlRadixTrieCandidateLessFunc( const UtlRadixTrieCandidate_t &c1, const UtlRadixTrieCandidate_t &c2, void *context )
{
	return c1.probability > c2.probability;
}


namespace panorama
{
	
//-----------------------------------------------------------------------------
// Purpose: Loads a non-optimized radix trie for the specified language
//-----------------------------------------------------------------------------
bool CTextInputSuggestDumbRadixTrie::BInitialize( ELanguage language )
{
#if !defined( SOURCE2_PANORAMA_FIXME )
	CPathString sWordList(
		CFmtStr1024( "../external/dictionaries/tenfoot/%s_words.txt",
		GetLanguageShortName( language ) ) );

	if ( !m_trie.BLoad( sWordList.GetUTF8Path() ) )
	{
		return false;
	}

	return true;
#else
	return false;
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Loads a pickled radix trie for the specified language
//-----------------------------------------------------------------------------	
bool CTextInputSuggestRadixTrie::BInitialize( ELanguage language )
{
#if !defined( SOURCE2_PANORAMA_FIXME )
	CPathString sPickledWordList( CFmtStr1024( "%s/%s_compiled_words.dic",
		UIEngine()->GetLocalPathForNamedPath( "{wordlists}" ),
		GetLanguageShortName( language ) ) );

	// attempt to load pickled dictionary
	CUtlBuffer buf;
	if ( !UIEngine()->UIFileSystem()->LoadFileIntoBuffer( sPickledWordList.GetUTF8Path(), buf, false ) )
	{
		return false;
	}

	if ( !m_trie.BLoad( buf ) )
	{
		return false;
	}

	return true;
#else
	return false;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Factory function
//-----------------------------------------------------------------------------
panorama::ITextInputSuggest *CreateInputSuggest( ELanguage language )
{
	panorama::ITextInputSuggest *psuggest = NULL;
#ifdef USE_PICKLING
	psuggest = new CTextInputSuggestRadixTrie();
#else
	psuggest = new CTextInputSuggestDumbRadixTrie();
#endif

	if ( !psuggest->BInitialize( language ) )
	{
		delete psuggest;
		return NULL;
	}

	return psuggest;
}

} // namespace panorama
