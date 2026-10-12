//========= Copyright Valve Corporation, All rights reserved. =================//
//
// Purpose: A radix trie, or Patricia trie, is a trie where nodes with only one
// child are merged with the child.
//
// This file implements a radix trie over C strings. It is encoding agnostic,
// with the exception of a single NUL terminator, so it would work for UTF-8
// but not for UTF-16. Note that internal nodes in the tree could be in the
// middle of a UTF-8 byte sequence; but no such nodes will be marked as
// "complete", and they will be traversed during examination of a single
// code point. UTF-8 correctness is asserted when strings come in at Add time,
// and prefixes are also asserted at suggestion time.

// There is a naive implementation, intended to be used to "pickle" into a more
// efficient format used by the faster, read-only, implementation.
//
//=============================================================================//

#ifndef UTLRADIXTRIE_H
#define UTLRADIXTRIE_H

#ifdef _WIN32
#pragma once
#endif

#include "tier0/dbg.h"
#include "tier1/utlstring.h"
#include "tier1/utlvector.h"
#include "tier1/utldict.h"
#include "tier1/strtools.h"

// Seriously?
#include "steamclientpublic.h"
#if defined( SOURCE2_PANORAMA )
#include "zip_utils.h"
#define k_cSmallBuff 255
#else
#include "steamcommon.h"
#endif

struct UtlRadixTrieCandidate_t
{
	char rgch[ k_cSmallBuff ];
	float probability;
};


//
// lessfunc to sort candidates by probability
//
bool UtlRadixTrieCandidateLessFunc( const UtlRadixTrieCandidate_t &c1, const UtlRadixTrieCandidate_t &c2, void *context );

#ifdef SOURCE2_PANORAMA
class CUtlSortVectorUtlRadixTrieCandidateLess
{
public:
	bool Less( const UtlRadixTrieCandidate_t& lhs, const UtlRadixTrieCandidate_t& rhs, void *context )
	{
		return UtlRadixTrieCandidateLessFunc( lhs, rhs, context );
	}
};

typedef CUtlSortVector< UtlRadixTrieCandidate_t, CUtlSortVectorUtlRadixTrieCandidateLess > CandidateList_t;
#else
typedef CUtlSortVector< UtlRadixTrieCandidate_t > CandidateList_t;
#endif

// 
// used at add and search time - computes length of common prefix of two strings.
//
static unsigned int CbPrefixInCommonCaseInsensitive( const char *s1, const char *s2 )
{
	unsigned int cch = 0;

	while ( *s1 && *s2 )
	{
		const char ch1 = *s1, ch2 = *s2;
		if ( tolower( ch1 ) != tolower( ch2 ) )
		{
			break;
		}

		cch++;
		s1++;
		s2++;
	}

	return cch;
}


//
// abstract base class, also templatized over the node type, for shared impl of the tree lookup.
//
// Not for consumption, just needs to be in the header for the template specializations below.
//
template< class N, class NR >
class CRadixTrieBase
{
public:
	virtual ~CRadixTrieBase()
	{
		// empty
	}


	//
	// similar check, but a loose search using word frequency data
	//
	bool BFindPrefixLoose( const char *szPrefix, CUtlString &sRest ) const
	{
#ifdef SOURCE2_PANORAMA
		CandidateList_t vecCandidates;
#else
		CandidateList_t vecCandidates( UtlRadixTrieCandidateLessFunc );
#endif
		FindCandidates( szPrefix, vecCandidates, 1 );

		if ( vecCandidates.Count() )
		{
			sRest = vecCandidates[ 0 ].rgch;
			return true;
		}
		sRest.Clear();
		return false;
	}

	struct SearchResult_t
	{
		NR m_Node;
		unsigned int m_unConsumed;
		bool m_bPartialMatch;
	};

	SearchResult_t FindBestMatchingParentNode( const char *szFind ) const
	{
		return const_cast<CRadixTrieBase *>( this )->FindBestMatchingParentNode( szFind );
	}

	SearchResult_t FindBestMatchingParentNode( const char *szFind )
	{
		VPROF_BUDGET( "CRadixTrieBase::FindBestMatchingParentNode", VPROF_BUDGETGROUP_TENFOOT );

		const char *sz = szFind;
		
		N *pNode = &const_cast<N &>( GetRoot() );

label_IterateChildren:
		if ( *sz )
		{
			const auto iChildren = CChildren( *pNode );
			for ( int i = 0; i < iChildren; i++ )
			{
				auto &child = const_cast<N &>( GetChild( *pNode, i ) );
				const char *pchLabel = GetLabel( child );
				unsigned int cchLabel = V_strlen( pchLabel );
				
				// Skip terminator nodes.
				if ( cchLabel == 0 )
					continue;

				const auto unPrefixLength = CbPrefixInCommonCaseInsensitive( sz, pchLabel );
				if ( unPrefixLength == 0 )
					continue;

				// If we partially match this prefix, we'll want to split this node. We're done at this
				// node and can't look further.
				if ( unPrefixLength < cchLabel )
				{
					SearchResult_t result = { NodeRef( child ), ( unsigned int )( sz - szFind ), true };
					return result;
				}

				// If we fully match this prefix, recurse further into the children for this node
				// looking for a further child node.
				if ( unPrefixLength == cchLabel )
				{
					sz += unPrefixLength;
					pNode = &child;

					goto label_IterateChildren;
				}

				// If we fall off the bottom, we didn't have any luck with this child node.
			}
		}

		// We didn't find any matching children at all. Return as far as we got.
		SearchResult_t result = { NodeRef( *pNode ), ( unsigned int )( sz - szFind ), false };
		return result;
	}

	//
	// FindCandidates - public interface
	//
	void FindCandidates( const char *szPrefix, CandidateList_t &vecCandidates, int cMaxCandidates ) const
	{
		VPROF_BUDGET( "CRadixTrieBase::FindCandidates (wrapper)", VPROF_BUDGETGROUP_TENFOOT );

		vecCandidates.RemoveAll();

		// If this goes off, we are passing bad text into the correction engine
		Assert( Q_UnicodeValidate( szPrefix ) );

		SearchResult_t result = FindBestMatchingParentNode( szPrefix );

		// If we failed to consume our whole prefix string while walking the tree, this could either mean "we
		// have no potential matches" or "we got to the end of our string but couldn't find a full match in the
		// tree" (ie., we're searching for "eel" and there's an "eels" node).
		//
		// We distinguish between these by seeing, if we have characters left, 
		if ( result.m_unConsumed < ( unsigned int )V_strlen( szPrefix ) && !result.m_bPartialMatch )
			return;

		// Call into recursive search worker.
		char rgch[ k_cSmallBuff ];
		V_strcpy_safe( rgch, szPrefix );
		rgch[ result.m_unConsumed ] = '\0';

		FindCandidates_R( result.m_Node, result.m_bPartialMatch, rgch, V_ARRAYSIZE( rgch ), vecCandidates, cMaxCandidates );
	}
	
protected:
	// required impl, not part of the public interface. these are on the tree rather than the
	// node because they refer back to the tree
	virtual const N &GetRoot() const = 0;
	virtual const N &GetChild( const N &node, int iChild ) const = 0;
	virtual int CChildren( const N &node ) const = 0;
	virtual const char *GetLabel( const N &node ) const = 0;
	virtual NR NodeRef( N& node ) = 0;

	//
	// FindCandidates_R - impl
	//
	void FindCandidates_R( const N &node, bool bAppendCurrentNodeContents, char *szCurrenString, int cchPrefixAlloc, CandidateList_t &vecCandidates, int cMaxCandidates ) const
	{
		VPROF_BUDGET( "CRadixTrieBase::FindCandidates", VPROF_BUDGETGROUP_TENFOOT );

		// We expect terminator nodes to have no children. They may have string content if we need to store
		// the full string for some reason (ie., contains in-line caps and can't be reconstructed from walking
		// the tree) or may be empty.
		Assert( (node.GetFrequency() > 0.0f) == (CChildren( node ) == 0) );

		// if this node is complete, consider it as a candidate
		if ( node.GetFrequency() > 0.0f )
		{
			// see if the probability is high enough - if not, don't add it, as it's
			// expensive to do
			if ( vecCandidates.Count() < cMaxCandidates ||
				node.GetFrequency() > vecCandidates[ vecCandidates.Count() - 1 ].probability )
			{
				VPROF_BUDGET( "CRadixTrieBase::FindCandidates(leaf)", VPROF_BUDGETGROUP_TENFOOT );

				UtlRadixTrieCandidate_t c;
				
				// Is this a terminator node with no internal content (meaning "use string reconstructed from
				// walking the tree to get here") or with internal content (meaning "just use whatever I say")?
				const char *pszLabel = GetLabel( node );
				V_strcpy_safe( c.rgch, pszLabel[0] == '\0' ? szCurrenString : pszLabel );
				c.probability = node.GetFrequency();
				vecCandidates.Insert( c );
				if ( vecCandidates.Count() > cMaxCandidates )
				{
					vecCandidates.Remove( vecCandidates[ cMaxCandidates ] );
				}
			}
		}
		else
		// recurse on any children
		{
			VPROF_BUDGET( "CRadixTrieBase::FindCandidates(recursion)", VPROF_BUDGETGROUP_TENFOOT );

			char *pchEndOfPrefix = szCurrenString;

			if ( bAppendCurrentNodeContents )
			{
				pchEndOfPrefix += V_strlen( szCurrenString );

				// append node's prefix to szPrefixSoFar
				V_strncat( szCurrenString, GetLabel( node ), cchPrefixAlloc );
			}

			int cchildren = CChildren( node );
			for ( int i = 0; i < cchildren; i++ )
			{
				const N &nodeT = GetChild( node, i );
				FindCandidates_R( nodeT, true, szCurrenString, cchPrefixAlloc, vecCandidates, cMaxCandidates );
			}

			// we are done with our node
			*pchEndOfPrefix = 0;
		}
	}
};


class CUtlRadixTrie;
class CPickledRadixTrie;

//
// totally naive impl - separate allocations for string and for children within each node.
// very heap spammy, non performant, just used to pickle tries.
//
class CUtlRadixTrieNode
{
public:
	CUtlRadixTrieNode() : m_flFrequency( 0.0f )
	{
		// empty label and no children
	}

	float GetFrequency() const { return m_flFrequency; }

#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, const tchar *pchName )
	{
		VALIDATE_SCOPE();
		ValidateObj( m_label );
		ValidateObj( m_rawLabel );
		ValidateObj( m_children );
	}
#endif

	float m_flFrequency;					// word frequency, per million
	CUtlStringBuilder m_label;				// label on this node (text to get through)
	CUtlString m_rawLabel;					// if we have a label that couldn't be reconstructed by walking the tree (ie., contains capital letters), we store the raw text here so we can recover it; this will get moved into the label post-all-splits when we pickle
	CCopyableUtlVector< int > m_children;	// indices into the tree of the children of this node
};

	
class CUtlRadixTrieNodeRef
{
public:
	// Doesn't really make sense to ever use this except for "we're going to initialize this later and know for sure
	// we won't use it until then, but can't declare the variable when it's used because of whatever C++ silliness".
	CUtlRadixTrieNodeRef() { }

	CUtlRadixTrieNodeRef( CUtlRadixTrie& tree, CUtlRadixTrieNode& node );

	operator CUtlRadixTrieNode& ()
	{
		return Get();
	}

	CUtlRadixTrieNode& Get();

private:
	CUtlRadixTrie *m_pTree;
	unsigned int m_unNodeIndex;
};


//
// And here is the naive radix tree impl, with the node type above.
//
// This trie should be used only to load and pickle a word list. It is not performant,
// but it supports changes to the tree (which the pickled class does not)
//
class CUtlRadixTrie : public CRadixTrieBase< CUtlRadixTrieNode, CUtlRadixTrieNodeRef >
{
	friend class CUtlRadixTrieNodeRef;
	friend class CPickledRadixTrie;

public:
	CUtlRadixTrie()
	{
		m_flFrequencyMax = 0.0f;
		m_bFinalized = false;

		// add the root element
		CUtlRadixTrieNode root;

		int iRoot = m_tree.AddToTail( root );
		Assert( iRoot == 0 );
	}


	//
	// Loads a trie from a flat word list.
	// word list trims whitespace and has a comment syntax (# at beginning of line)
	//
	bool BLoad( const char *szFilename, int cMaxWords = -1 )
	{
#ifdef _WIN32
		Assert( !m_bFinalized );

		// Windows impl follows
		FILE *f = fopen( szFilename, "rt" );
		if (!f)
			return false;

		int iLine = 0;
		int cAdded = 0;

		// read header line
		char lineHeader[ k_cSmallBuff ];
		if ( !fgets( lineHeader, sizeof( lineHeader ), f ) )
		{
			Msg( "Can't read header line - empty file?\n" );
			return false;
		}

		// validate header line
		if ( !( V_strstr( lineHeader, "perMillion" ) &&
				V_strstr( lineHeader, "Count" ) ) )
		{
			Msg( "Invalid header line: %s\n", lineHeader );
			return false;
		}

		while (++iLine, !feof(f))
		{
			// expected format is, whitespace delimited:
			//		word
			//		count in corpus (ignored)
			//		count per million
			//		length (ignored)
			char line[ k_cSmallBuff ];

			if ( fgets(line, sizeof(line), f) )
			{
				V_StrTrim( line );

				if ( V_strlen( line ) == 0 )
				{
					// skip empty lines
					continue;
				}

				if ( line[ 0 ] == '#' )
				{
					// sh-like comment syntax
					continue;
				}

				char rgchWord[ k_cSmallBuff ];
				float frequency = 0.0f;
				REFERENCE( frequency );

				int cField = sscanf( line, "%s %*d %f %*d", rgchWord, &frequency );
				if ( cField != 2 )
				{
					// error on this line - occurs due to phrases with embedded spaces.
					// TODO need to scanf for tab delimiter in order to catch these phrases
					
					Msg( "Error on line: %s\n", line );
					continue;
				}

				Add( rgchWord, frequency );
				m_flFrequencyMax = MAX( m_flFrequencyMax, frequency );
				cAdded++;

				if ( cMaxWords > 0 && cAdded >= cMaxWords )
				{
					Msg( "Limit of %d words reached\n", cMaxWords );
					break;
				}
			}
		}

		fclose(f);

		for ( CUtlRadixTrieNode& Node : m_tree )
		{
			if ( Node.m_rawLabel.Length() > 0 )
			{
				Node.m_label = Node.m_rawLabel;
			}
		}

		m_bFinalized = true;

		return true;
#else
		// we call fgets in this method. If the text file has \r\n on it, fgets on posix will tear off the \n but not the \r,
		// so every word in the wordlist will have a newline on it.
		//
		// if you need this code to work on osx or linux, it's probably time to write Q_fgets that handles all that.
		AssertMsg( false, "fgets newline discipline will leave carriage returns in - implement wrapper if you need this" );
		return false;
#endif
	}

	CUtlRadixTrieNodeRef AddNewNode( CUtlRadixTrieNodeRef ParentNode, const char *pszRemainder, const char *pszRawLabel, float freqPerMillion )
	{
		// All terminator nodes require a frequency greater than zero. They may or may not have string content.
		Assert( (V_strlen( pszRemainder ) > 0) || (freqPerMillion > 0.0f) );

		CUtlRadixTrieNode nodeNew;
		nodeNew.m_flFrequency = freqPerMillion;
		nodeNew.m_label = pszRemainder;
		nodeNew.m_rawLabel = pszRawLabel;

		int iNew = m_tree.AddToTail( nodeNew );
		ParentNode.Get().m_children.AddToTail( iNew );

		return NodeRef( m_tree[ iNew ] );
	}

	CUtlRadixTrieNodeRef AddNewPrefixNode( CUtlRadixTrieNodeRef ParentNode, const char *pszRemainder )
	{
		return AddNewNode( ParentNode, pszRemainder, "", 0.0f );
	}

	CUtlRadixTrieNodeRef AddNewTerminatorNode( CUtlRadixTrieNodeRef ParentNode, const char *pszFullString, float freqPerMillion )
	{
		// If we do something like add "he" and then "heel", we need to track that "he" is a complete word,
		// even though it has children (in this case "el"). Originally this was stored with a flag per node,
		// but this complicated the splitting logic and made the storage for frequency inconsistent.
		static const char *s_pszTerminatorNode = "";

		// If we can't reconstruct our string by walking the tree, we store the full string in the terminator
		// node and just use that at query time. This is a ~15-20% space cost but it means that we can easily
		// do case-insensitive queries against a case-preserving dataset without affecting the internal tree
		// structure.
		bool bRequiresFullTerminatorString = false;
		for ( const char *pc = pszFullString; *pc; pc++ )
		{
			// FIXME: Right now this is broken for both non-ASCII characters as well as basically all of UTF8
			//		  that takes more than one byte to store. It would be better to replace this with Unicode
			//		  iteration of codepoints and then look those up to determine case but we don't have that
			//		  data in the client right now.
			if ( V_isupper( *pc ) )
			{
				bRequiresFullTerminatorString = true;
				break;
			}
		}

		return AddNewNode( ParentNode, s_pszTerminatorNode, bRequiresFullTerminatorString ? pszFullString : s_pszTerminatorNode, freqPerMillion );
	}

	virtual CUtlRadixTrieNodeRef NodeRef( CUtlRadixTrieNode& node ) OVERRIDE
	{
		return CUtlRadixTrieNodeRef( *this, node );
	}

	//
	// Adds a string to the trie. This is a radix trie, not a regular trie, so labels on nodes can be
	// more than one character long. Adding a node means we need to iterate through a label and find the
	// proper spot to split it.
	//
	void Add( const char *sz, float freqPerMillion )
	{
		Assert( sz );
		Assert( *sz );

		// If this goes off, we are passing bad text into the correction engine
		Assert( Q_UnicodeValidate( sz ) );

		// Look through all the children of our root node for a potential match.
		SearchResult_t result = FindBestMatchingParentNode( sz );

		// Do we already have this entire string in our tree? Prevent multiple adds.
		if ( result.m_unConsumed == ( unsigned int )V_strlen( sz ) )
			return;

		// If we find no potential match, that's the same case as "we have an empty tree" as far as we're concerned
		// and we allocate a completely fresh node.
		CUtlRadixTrieNodeRef ParentNode = result.m_Node;

		if ( &ParentNode.Get() == &GetRoot() )
		{
			Assert( result.m_unConsumed == 0 );

			AddNewTerminatorNode( AddNewPrefixNode( ParentNode, sz ), sz, freqPerMillion );
			return;
		}

		// If we make it here we either have a node with a string that's too long ("quick" -> "quickly") and we want to
		// add the remaining characters to the end of our existing nodes, or we have have a node with a string that's too
		// short ("quickly" -> "quick") and we want to split the existing node and then add two children (one with the new
		// end bits and one with the previous ending characters (possibly an empty terminator)).
		//
		// bPartialMatchForSplit here means "did we return a node that we have a partial match in?". If we did, we may need
		// to do a split; if not, we know we can get away with just doing a simple add.
		const auto unPrefixLength = result.m_bPartialMatch
								  ? CbPrefixInCommonCaseInsensitive( sz + result.m_unConsumed, GetLabel( ParentNode ) )
								  : 0;

		Assert( unPrefixLength <= ( unsigned int )V_strlen( sz + result.m_unConsumed ) );
		Assert( unPrefixLength <= ( unsigned int )V_strlen( GetLabel( ParentNode ) ) );

		// If we have no common prefix with the node we're looking at, it means that we need to add a fresh new child.
		if ( unPrefixLength == 0 )
		{
			AddNewTerminatorNode( AddNewPrefixNode( ParentNode, sz + result.m_unConsumed ), sz, freqPerMillion );
		}
		// We must have a common prefix with this node. We'll need to split it at the end of our prefix and add two new
		// children nodes.
		else
		{
			// We're going to be splitting our existing parent node. We'd like to add a new interstitial parent and then
			// point our found node to that, but the internals of this data structure make that problematic. Instead, we
			// split the existing parent and then move its current children to the new child we make. In order to do this
			// we store off the children beforehand and then append them later to the new node.
			CUtlVector<int> vecOldParentChildren;
			vecOldParentChildren.Swap( ParentNode.Get().m_children );

			// If we're splitting our parent node and don't have any contents of our own, we don't need to make a new prefix
			// node, just add a new terminator. (ie., we have the node "question" and we add "quest".)
			CUtlRadixTrieNodeRef InterstitialNode  = sz[ result.m_unConsumed + unPrefixLength ]
												   ? AddNewPrefixNode( ParentNode, sz + result.m_unConsumed + unPrefixLength )
												   : ParentNode;
			AddNewTerminatorNode( InterstitialNode, sz, freqPerMillion );

			// We don't know whether this is a prefix or a terminator node, but we don't really care -- we'll just copy
			// whatever we have.
			CUtlRadixTrieNodeRef OldParentNode = AddNewNode( ParentNode, GetLabel( ParentNode ) + unPrefixLength, "", ParentNode.Get().GetFrequency() );
			OldParentNode.Get().m_children.Swap( vecOldParentChildren );
			OldParentNode.Get().m_rawLabel.Swap( ParentNode.Get().m_rawLabel );

			// Truncate the token in our new parent node to reflect the fact that part of what we used to contain is
			// now in our new child.
			ParentNode.Get().m_label.Truncate( unPrefixLength );
			Assert( V_strlen( GetLabel( ParentNode ) ) > 0 );

			// Our new parent node is now a center node in tree and so will have these two new children but no state of its own.
			ParentNode.Get().m_flFrequency = 0.0f;
		}
	}


	//
	// Stats info
	//
	void DumpStatistics() const
	{
		// compute stats on average character count per node, branching factor, etc.

		size_t cch = 0;
		int cnode = m_tree.Count();
		int ckids = 0;
		int cinteriornodes = 0;

		FOR_EACH_VEC( m_tree, i )
		{
			const CUtlRadixTrieNode &node = m_tree[ i ];

			cch += node.m_label.Length();
			if ( node.m_children.Count() )
			{
				cinteriornodes++;
				ckids += node.m_children.Count();
			}
		}

		Msg( "trie stats:\n" );
		Msg( "    Nodes: %d\n", cnode );
		Msg( "    Average label length %.2f\n", (float) cch / (float) cnode );
		Msg( "    Average branching factor %.2f\n", (float) ckids / (float) cinteriornodes );
	}


	//
	// Utility method for pickling
	//
	float GetMaxFrequency() const
	{
		return m_flFrequencyMax;
	}


	//
	// Utility method to be a well-behaved trie - returns our root
	//
	virtual const CUtlRadixTrieNode &GetRoot() const OVERRIDE
	{
		return m_tree[ 0 ];
	}


	//
	// Utility method to be a well-behaved trie - returns specified child of specified node
	//
	virtual const CUtlRadixTrieNode &GetChild( const CUtlRadixTrieNode &node, int iChild ) const OVERRIDE
	{
		return m_tree[ node.m_children[ iChild ] ];
	}


	//
	// Utility method to be a well-behaved trie - returns label on a given node
	//
	virtual const char *GetLabel( const CUtlRadixTrieNode &node ) const OVERRIDE
	{
		return node.m_label.String();
	}


	//
	// Utility method to be a well-behaved trie - returns number of children of specified node
	//
	virtual int CChildren( const CUtlRadixTrieNode &node ) const OVERRIDE
	{
		return node.m_children.Count();
	}


	//-----------------------------------------------------------------------------
	// Purpose: Validation
	//-----------------------------------------------------------------------------
#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName )
	{
		VALIDATE_SCOPE();

		ValidateObj( m_tree );
		FOR_EACH_VEC( m_tree, i )
		{
			ValidateObj( m_tree );
		}
	}
#endif

private:
	CUtlVector< CUtlRadixTrieNode > m_tree;		// this is the tree
	float m_flFrequencyMax;
	bool m_bFinalized;
};


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
inline CUtlRadixTrieNodeRef::CUtlRadixTrieNodeRef( CUtlRadixTrie& tree, CUtlRadixTrieNode& node )
	: m_pTree( &tree )
	, m_unNodeIndex( &node - &tree.GetRoot() )
{
	//
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
inline CUtlRadixTrieNode& CUtlRadixTrieNodeRef::Get()
{
	return m_pTree->m_tree[ m_unNodeIndex ];
}


//
// a more optimized approach, looks something like this:
//	[4 bytes] magic number
//		[4 bytes] number of labels
//		[4 bytes] number of total children (includes nul terminators so it's not just nodes - 1)
//		[4 bytes] number of nodes
//	[4 bytes] magic number
//		a\0b\0c\0...zzz\0 - bunch of concatenated C strings
//	[4 bytes] magic number
//		child child child nil child child child nil nil etc. - list of children for each node, each 4 bytes
//	[4 bytes] magic number
//		[node][node]...[node] - read into utlvector
//	[4 bytes] magic number
//
// each node has
// -- complete flag
// -- label index for its label
// -- index into vector holding children topology
//
// when we read a particular label in, we set up a pointer for it in the associated array
// then rgsz[ node.symindex ] gives you the label for a given node.
// we could make that array a temporary array, and swizzle the indices in the tree
// into pointers in memory... but that makes the serialized data structure 32- vs 64-bit sensitive.
//
// the children array is just a flat array of indices.
//
// with this approach, you have to know the whole symbol table ahead of time, or be willing
// to reallocate it.
//
// sadly building an optimized symbol table while building the tree is hard, because we will create
// a prefix then split it up, leaving orphaned symbols. So they'd have to be refcounted.
//
// TODO maybe store the children without terminators - the count of children of each node can be stored
// as a 16 bit value in an array over the nodes (or even in each node). We'll waste two bytes
// per node that has no children, but save four bytes per node that does have children. Need to do
// a little arithmetic to see which way wins.

//
// Node type for the pickled tree.
//
// two bytes of label - this is an index into the global label table
// one bit of "am i a complete node"
// 31 bits of first child - this is an index onto the global children table for the first child of this node.
//
// total size is six bytes.
//
#pragma pack( push, 1 )
struct CPickledRadixTrieNode
{
	static const uint32 CHILD_NIL = 0x7fffff;
	uint16 iLabel;				// index into label table - max 64K labels

	uint32 frequency : 9;		// word frequency, quantized: 0 is invalid (means not a complete word), 1 is infrequent...511 is very frequent
	uint32 iChildFirst : 23;	// index of first child in children table, or CHILD_NIL if no children

	float GetFrequency() const { return (float)frequency; }

	static const uint32 s_frequencyMax = 511;

};
#pragma pack( pop )

class CPickledRadixTrieNodeRef
{
public:
	// Doesn't really make sense to ever use this except for "we're going to initialize this later and know for sure
	// we won't use it until then, but can't declare the variable when it's used because of whatever C++ silliness".
	CPickledRadixTrieNodeRef() { }

	CPickledRadixTrieNodeRef( CPickledRadixTrie& tree, CPickledRadixTrieNode& node );

	operator CPickledRadixTrieNode& ()
	{
		return Get();
	}

	CPickledRadixTrieNode& Get();

private:
	CPickledRadixTrie *m_pTree;
	unsigned int m_unNodeIndex;
};


//
// And here is the pickled trie class, the node type of which is just above
//
class CPickledRadixTrie : public CRadixTrieBase< CPickledRadixTrieNode, CPickledRadixTrieNodeRef >
{
	friend class CPickledRadixTrieNodeRef;

	static const uint32 m_unMagic = 0xC0DEDD1C;

public:
	CPickledRadixTrie() :
		m_cLabels( 0 ),
		m_cubLabels( 0 ),
		m_cChildren( 0 ),
		m_cNodes( 0 ),
		m_rgszLabels( NULL ),
		m_szTable( NULL )
	{
		COMPILE_TIME_ASSERT( sizeof( CPickledRadixTrieNode ) == 6 );
	}


	~CPickledRadixTrie()
	{
		SAFE_DELETE( m_rgszLabels );
		SAFE_DELETE( m_szTable );
	}


	//
	// Loads us from a pickled/zipped file.
	//
	// leaves us in an inconsistent state if we return false.
	// only safe thing to do then is delete us.
	//
	bool BLoad( CUtlBuffer &bufZipped )
	{
		CUtlBuffer buf;			// pickled / uncompressed temporary in memory

#if defined( SOURCE2_PANORAMA )
		IZip *pZip = IZip::CreateZip( NULL, false );

		pZip->ParseFromBuffer( bufZipped.Base(), bufZipped.TellPut() );
		if ( !pZip->ReadFileFromZip( "data", false, buf ) )
			return false;

		IZip::ReleaseZip( pZip );
#else
		if ( k_EResultOK != GUnzipToBuffer( bufZipped.Base(), bufZipped.TellPut(),
			buf, 20 * k_nMillion ) )
		{
			return false;
		}
#endif
		// read magic
		if ( buf.GetUnsignedInt() != m_unMagic )
		{
			return false;
		}

		m_cLabels = buf.GetUnsignedInt();
		m_cubLabels = buf.GetUnsignedInt();
		m_cChildren = buf.GetUnsignedInt();
		m_cNodes = buf.GetUnsignedInt();

		// read interstitial magic
		if ( buf.GetUnsignedInt() != m_unMagic )
		{
			return false;
		}

		// TODO more sanity checks
		// note we can share labels, so labels > nodes is not a valid assumption

		// allocate and read structures

		// dictionary
		char *pb = new char[ m_cubLabels ];
		if ( !buf.Get( pb, m_cubLabels ) )
			return false;

		m_szTable = pb;

		// build dictionary lookup table
		const char **rgsz = new const char*[ m_cLabels ];
		char *sz = pb;
		for ( uint32 isz = 0;
			isz < m_cLabels;
			isz++ )
		{
			rgsz[ isz ] = sz;
			sz += V_strlen( sz ) + 1;
		}
		m_rgszLabels = rgsz;

		// read interstitial magic
		if ( buf.GetUnsignedInt() != m_unMagic )
			return false;

		// children
		m_vecChildren.SetCount( m_cChildren );
		if ( !buf.Get( m_vecChildren.Base(), sizeof( uint32 ) * m_cChildren ) )
			return false;
		// read interstitial magic
		if ( buf.GetUnsignedInt() != m_unMagic )
			return false;

		// tree
		m_vecNodes.SetCount( m_cNodes );
		if ( !buf.Get( m_vecNodes.Base(), sizeof( CPickledRadixTrieNode ) * m_cNodes ) )
			return false;

		// read final magic
		if ( buf.GetUnsignedInt() != m_unMagic )
			return false;

		return true;

	}


	//
	// Quantizes a floating-point frequency value (we are expecting
	// words per million, but anything should work as long as it has
	// a range of 0 to some fairly large number).
	//
	// We quantize to [ 1, 511 ], using 0 to signify a non-termination
	// node.
	// 
	// Everything is normalized so the most frequent word will have
	// a frequency of 511, and everything else will vary based on that.
	//
	static uint32 QuantizeFrequency( float frequency, float frequencyMax )
	{
		if ( frequency <= 0.0f )
			return 0;

		// convert frequency per million into a number more guaranteed to be greater than one, so as to keep
		// the logs all positive
		//
		// (could use the absolute word frequency from the corpus instead)
		frequency *= k_nMillion;
		frequencyMax *= k_nMillion;
		frequency = MAX( frequency, 1.0f );

		// we want the logarithm of the frequencyMax value to yield 511, so figure out the base that makes that happen
		// FUTURE we could really only do this once.
		float newbase = exp( log( frequencyMax ) / (float)CPickledRadixTrieNode::s_frequencyMax );

		// now compute the base of our frequency in that base
		float logarithm = log( frequency ) / log( newbase );
		logarithm = MAX( logarithm, 1.0f );

		unsigned int iFrequency = floor( logarithm );
		return clamp( iFrequency, uint32(1), CPickledRadixTrieNode::s_frequencyMax );
	}


	//
	// Pickles an existing radix trie into a more heap friendly format.
	//
	// Format is described above. The output is gzipped.
	//
	// Basic plan is to iterate through the unique labels and write them down,
	// then write down the child arrays, then write down the actual tree.
	//
	static bool Pickle( const CUtlRadixTrie &src, CUtlBuffer &bufZipped, bool bCaseSensitive, float frequencyMax )
	{
		CUtlBuffer buf;	// temporary uncompressed but pickled buffer in memory

		// nodes in the tree
		int cNodes = src.m_tree.Count();
		int cLeaves = 0;

		// total number of children - used to figure out size of children table
		int cTotalChildren = 0;
		FOR_EACH_VEC( src.m_tree, i )
		{
			const CUtlRadixTrieNode &node = src.m_tree[ i ];
			uint32 cKids = node.m_children.Count();
			cTotalChildren += cKids;
			if ( cKids )
				cTotalChildren += 1;		// every child list is terminated if it is serialized at all
		}

		CUtlVector< uint32 > vecChildren( 0, cTotalChildren );
		vecChildren.SetCount( cTotalChildren );
		int iChildPut = 0;

		// private symbol table so we know exactly what labels we have
		CUtlDict< const char * > dictLabels( bCaseSensitive ? k_eDictCompareTypeCaseSensitive : k_eDictCompareTypeCaseInsensitive,
			0, cNodes );

		CUtlVector< CPickledRadixTrieNode > treePickled( 0, cNodes );
		treePickled.SetCount( cNodes );

		uint32 cubDict = 0;

		FOR_EACH_VEC( src.m_tree, i )
		{
			const CUtlRadixTrieNode &node = src.m_tree[ i ];

			// write the node's label into the label table if it's not there already
			int idxString = dictLabels.Find( node.m_label );
			if ( idxString == dictLabels.InvalidIndex() )
			{
				idxString = dictLabels.Insert( node.m_label );
				cubDict += V_strlen( node.m_label ) + 1;
			}

			// we can write the pickled node's children and write the node itself
			CPickledRadixTrieNode &nodePickled = treePickled[ i ];
			nodePickled.frequency = QuantizeFrequency( node.GetFrequency(), frequencyMax );
			nodePickled.iLabel = idxString;
			if ( node.m_children.Count() != 0 )
			{
				nodePickled.iChildFirst = iChildPut;

				// write the node's children into the children table
				FOR_EACH_VEC( node.m_children, iChild )
				{
					vecChildren[ iChildPut++ ] = node.m_children[ iChild ];
				}
				// and terminator
				vecChildren[ iChildPut++ ] = CPickledRadixTrieNode::CHILD_NIL;
			}
			else
			{
				nodePickled.iChildFirst = CPickledRadixTrieNode::CHILD_NIL;
				cLeaves++;
			}
		}

		// consistency checks

		// verify every node's ichildfirst and ilabel is in range
		FOR_EACH_VEC( treePickled, i )
		{
			CPickledRadixTrieNode &node = treePickled[ i ];

			Assert( node.iLabel < dictLabels.Count() );
			Assert( node.iChildFirst == CPickledRadixTrieNode::CHILD_NIL || node.iChildFirst < (uint32)vecChildren.Count() );
		}

		// verify every item in child arrays is in range
		FOR_EACH_VEC( vecChildren, i )
		{
			uint32 iChild = vecChildren[ i ];
			Assert( iChild == CPickledRadixTrieNode::CHILD_NIL || iChild < (uint32)treePickled.Count() );
		}

		// start serializing

		Msg( "dictionary: %d bytes\n", cubDict );
		Msg( "children: %d bytes\n", (int)( vecChildren.Count() * sizeof( uint32 ) ) );
		Msg( "nodes (%d interior, %d leaves): %d bytes\n",
			cNodes - cLeaves, cLeaves,
			(int)( treePickled.Count() * sizeof( CPickledRadixTrieNode ) ) );

		// magic
		buf.PutUnsignedInt( m_unMagic );

		// # labels
		buf.PutUnsignedInt( dictLabels.Count() );

		// # dict CB
		buf.PutUnsignedInt( cubDict );

		// # children in toto
		buf.PutUnsignedInt( vecChildren.Count() );

		// # nodes
		buf.PutUnsignedInt( treePickled.Count() );

		// interstitial magic
		buf.PutUnsignedInt( m_unMagic );

		// dictionary - do not serialize the offsets table, that can be in memory only
		uint32 cub = 0;
		FOR_EACH_DICT_FAST( dictLabels, i )
		{
			const char *szLabel = dictLabels.GetElementName( i );
			uint32 cubThisLabel = V_strlen( szLabel ) + 1;
			buf.Put( szLabel, cubThisLabel );
			cub += cubThisLabel;
		}
		Assert( cub == cubDict );

		// interstitial magic
		buf.PutUnsignedInt( m_unMagic );

		// children
		buf.Put( vecChildren.Base(), vecChildren.Count() * sizeof( uint32 ) );

		// interstitial magic
		buf.PutUnsignedInt( m_unMagic );

		// the tree
		buf.Put( treePickled.Base(), treePickled.Count() * sizeof( CPickledRadixTrieNode ) );

		// terminal magic
		buf.PutUnsignedInt( m_unMagic );

		if ( !buf.IsValid() )
		{
			return false;
		}

		Msg( "Total size %d bytes\n", buf.TellPut() );

#if defined( SOURCE2_PANORAMA )
		IZip *pZip = IZip::CreateZip( NULL, false );

		pZip->AddBufferToZip( "data", buf.Base(), buf.TellPut(), false );
		pZip->SaveToBuffer( bufZipped );

		IZip::ReleaseZip( pZip );
#else
		// zip it as hard as possible, since this process runs offline
		if ( k_EResultOK != GZipToBuffer( buf.Base(), buf.TellPut(), bufZipped, 9 ) )
		{
			return false;
		}
#endif
		Msg( "Compressed size %d bytes (%.1f%% of original)\n", bufZipped.TellPut(),
			(float)bufZipped.TellPut() * 100 / buf.TellPut() );

		return true;
	}


	//
	// Utility method to be a well-behaved trie - returns our root
	//
	virtual const CPickledRadixTrieNode &GetRoot() const OVERRIDE
	{
		return m_vecNodes[ 0 ];
	}


	//
	// Utility method to be a well-behaved trie - returns # of children for a node
	//
	virtual int CChildren( const CPickledRadixTrieNode &node ) const OVERRIDE
	{
		VPROF_BUDGET( "CPickledRadixTrie::CChildren", VPROF_BUDGETGROUP_TENFOOT );

		if ( node.iChildFirst == CPickledRadixTrieNode::CHILD_NIL )
			return 0;

		uint32 iChild = node.iChildFirst;
		while ( m_vecChildren[ iChild ] != CPickledRadixTrieNode::CHILD_NIL )
		{
			iChild++;
		}

		return iChild - node.iChildFirst;
	}

	//
	// Utility method to be a well-behaved trie - fetches a child of a node
	//
	virtual const CPickledRadixTrieNode &GetChild( const CPickledRadixTrieNode &node, int iChild ) const OVERRIDE
	{
		int iChildTarget = node.iChildFirst + iChild; // index into global child array
		Assert( iChild < CChildren( node ) );

		int iNodeChild = m_vecChildren[ iChildTarget ];
		return m_vecNodes[ iNodeChild ];
	}

	//
	// Utility method to be a well-behaved trie - fetches label of a node
	//
	virtual const char *GetLabel( const CPickledRadixTrieNode &node ) const OVERRIDE
	{
		return m_rgszLabels[ node.iLabel ];
	}

	//-----------------------------------------------------------------------------
	// Purpose: Validation
	//-----------------------------------------------------------------------------
	virtual CPickledRadixTrieNodeRef NodeRef( CPickledRadixTrieNode& node ) OVERRIDE
	{
		return CPickledRadixTrieNodeRef ( *this, node );
	}


	//-----------------------------------------------------------------------------
	// Purpose: Validation
	//-----------------------------------------------------------------------------
#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName )
	{
		VALIDATE_SCOPE();
		ValidateObj( m_vecChildren );
		ValidateObj( m_vecNodes );

		validator.ClaimArrayMemory( m_rgszLabels );
		validator.ClaimArrayMemory( (void*)m_szTable );
	}
#endif

private:
	// string table and labels table
	const char **m_rgszLabels;
	const char *m_szTable;

	CUtlVector< int > m_vecChildren;
	CUtlVector< CPickledRadixTrieNode > m_vecNodes;

	uint32 m_cLabels;
	uint32 m_cubLabels;
	uint32 m_cChildren;
	uint32 m_cNodes;
};


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
inline CPickledRadixTrieNodeRef::CPickledRadixTrieNodeRef( CPickledRadixTrie& tree, CPickledRadixTrieNode& node )
	: m_pTree( &tree )
	, m_unNodeIndex( &node - &tree.GetRoot() )
{
	//
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
inline CPickledRadixTrieNode& CPickledRadixTrieNodeRef::Get()
{
	return m_pTree->m_vecNodes[ m_unNodeIndex ];
}

#endif // UTLRADIXTRIE_H
