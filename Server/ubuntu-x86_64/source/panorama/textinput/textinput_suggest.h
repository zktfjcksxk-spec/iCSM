//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef PANORAMA_TEXTINPUT_SUGGEST_H
#define PANORAMA_TEXTINPUT_SUGGEST_H

#include "utlradixtrie.h"

namespace panorama
{

//-----------------------------------------------------------------------------
// Purpose: Auto-suggestion interface
//-----------------------------------------------------------------------------
class ITextInputSuggest
{
public:
	virtual bool BInitialize( ELanguage language ) = 0;
	virtual ~ITextInputSuggest() {}
	virtual bool SuggestWord( const char *szPrefix, CUtlString &sSuggestion ) const = 0;
	virtual void SuggestWords( const char *szPrefix, CandidateList_t &vecCandidates, int cMaxCandidates ) const = 0;

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName ) = 0;
#endif
};


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
template < class T >
class CTextInputSuggestRadixTree : public ITextInputSuggest
{
public:
	// Perform a suggestion. The suggestion engine will treat a word as	a prefix of itself, so we need to make
	// sure that the "rest of the text"	(put into sSuggestion) isn't empty. 
	virtual bool SuggestWord( const char *szPrefix, CUtlString &sSuffix ) const OVERRIDE
	{
		CUtlString sSuggestion;
		bool bSuggested = m_trie.BFindPrefixLoose( szPrefix, sSuggestion );

		if ( bSuggested && !sSuggestion.IsEmpty() )
		{
			if ( sSuggestion.Length() >= V_strlen( szPrefix ) )
			{
				sSuffix = sSuggestion.String() + V_strlen( szPrefix );
			}
			else
			{
				sSuffix = "";
			}
		}
		else
		{
			sSuffix.Clear();
		}
		return !sSuffix.IsEmpty();
	}

	// Perform a loose suggestion. Returns a list of candidates sorted by decreasing order of word frequency.
	virtual void SuggestWords( const char *szPrefix, CandidateList_t &vecCandidates, int cMaxCandidates ) const OVERRIDE
	{
		return m_trie.FindCandidates( szPrefix, vecCandidates, cMaxCandidates );
	}

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName ) OVERRIDE
	{
		VALIDATE_SCOPE();
		ValidateObj( m_trie );
	}
#endif

protected:
	T m_trie;
};

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class CTextInputSuggestDumbRadixTrie : public CTextInputSuggestRadixTree<CUtlRadixTrie>
{
public:
	virtual bool BInitialize( ELanguage language ) OVERRIDE;
};


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CTextInputSuggestRadixTrie : public CTextInputSuggestRadixTree<CPickledRadixTrie>
{
public:
	virtual bool BInitialize( ELanguage language ) OVERRIDE;
};

ITextInputSuggest *CreateInputSuggest( ELanguage language );

} // namespace panorama

#endif // PANORAMA_TEXTINPUT_SUGGEST_H

