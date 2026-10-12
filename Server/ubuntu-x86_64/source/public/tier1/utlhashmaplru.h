//========= Copyright Valve Corporation, All rights reserved. =================//
//
// Purpose: index-based hash map container
//
//=============================================================================//

#ifndef UTLHASHMAPLRU_H
#define UTLHASHMAPLRU_H

#ifdef _WIN32
#pragma once
#endif

#include "utlhashmap.h"
#include "utllinkedlist.h"

// This is a useful macro to iterate from start to end in order in a map
#define FOR_EACH_HASHMAP_LRU( mapName, iteratorName ) \
for ( int iteratorName = (mapName).Oldest(); iteratorName != (mapName).InvalidIndex(); iteratorName = (mapName).NextNewer( iteratorName ) )

//-----------------------------------------------------------------------------
//
// Purpose:	An associative container. Pretty much identical to CUtlHashMapLRU
//			except with age info also tracked and the ability to ask for the
//			oldest item.
//
//-----------------------------------------------------------------------------
template <typename K, typename T, typename L = CDefEquals<K>, typename H = HashMapFunctor_t<K> > 
class CUtlHashMapLRU
{
public:
	typedef K KeyType_t;
	typedef T ElemType_t;
	typedef int IndexType_t;
	typedef L EqualityFunc_t;
	typedef H HashFunc_t;

	CUtlHashMapLRU()
		: m_bRetainInsertOrderOnly( false )
	{
	}

	CUtlHashMapLRU( int cElementsExpected )
	: m_hashMap( cElementsExpected ), m_listLRU( cElementsExpected ), m_bRetainInsertOrderOnly( false )
	{
	}

	~CUtlHashMapLRU()
	{
		RemoveAll();
	}

	void SetRetainInsertOrderOnly( bool bRetain = true )		{ m_bRetainInsertOrderOnly = bRetain; }
	bool GetRetainInsertOrderOnly() const						{ return m_bRetainInsertOrderOnly; }

	// gets particular elements and automatically moves them to the head of the LRU
	ElemType_t &		Element( IndexType_t i )			{ PromoteOnAccess( i ); return m_hashMap.Element( i ).e; }
	ElemType_t &		operator[]( IndexType_t i )			{ PromoteOnAccess( i ); return m_hashMap.Element( i ).e; }
	KeyType_t &			Key( IndexType_t i )				{ return m_hashMap.Key( i ); }
	const KeyType_t &	Key( IndexType_t i ) const			{ return m_hashMap.Key( i ); }

	// Gets at elements without updating their place in the LRU
	ElemType_t &		Peek( IndexType_t i )				{ return m_hashMap.Element( i ).e; }
	const ElemType_t &	Peek( IndexType_t i ) const			{ return m_hashMap.Element( i ).e; }
	ElemType_t &		PeekNewest()						{ return m_hashMap.Element( MostRecentUsed() ).e; }
	const ElemType_t &	PeekNewest() const					{ return m_hashMap.Element( MostRecentUsed() ).e; }
	ElemType_t &		PeekOldest()						{ return m_hashMap.Element( LeastRecentUsed() ).e; }
	const ElemType_t &	PeekOldest() const					{ return m_hashMap.Element( LeastRecentUsed() ).e; }

	// Mark an element as accessed without actually doing a get
	void				Touch( IndexType_t i )				{ PromoteOnAccess( i ); }

	// Num elements
	IndexType_t Count() const								{ return m_hashMap.Count(); }

	// Max "size" of the vector
	IndexType_t  MaxElement() const							{ return m_hashMap.MaxElement(); }

	// Checks if a node is valid and in the map
	bool  IsValidIndex( IndexType_t i ) const				{ return m_hashMap.IsValidIndex( i ); }

	// Invalid index
	static IndexType_t InvalidIndex()						{ return -1; }

	// Insert method
	IndexType_t  Insert( const KeyType_t &key, const ElemType_t &insert );
	IndexType_t  Insert( const KeyType_t &key )				{ return Insert( key, ElemType_t() ); }
	IndexType_t  FindOrInsert( const KeyType_t &key, const ElemType_t &insert );
	IndexType_t  FindOrInsert( const KeyType_t &key )		{ return FindOrInsert( key, ElemType_t() ); }
	IndexType_t  InsertWithDupes( const KeyType_t &key, const ElemType_t &insert );
	IndexType_t  InsertOrReplace( const KeyType_t &key, const ElemType_t &insert );

	// Finds an element
	IndexType_t  Find( const KeyType_t &key ) const			{ return m_hashMap.Find( key ); }
	IndexType_t	 Oldest() const								{ return LeastRecentUsed(); }
	IndexType_t  Newest() const								{ return MostRecentUsed(); }
	IndexType_t  LeastRecentUsed() const;
	IndexType_t	 MostRecentUsed() const;
	IndexType_t  NextOlder( IndexType_t nIdx );
	IndexType_t  NextNewer( IndexType_t nIdx );
	
	// has an element.
	bool HasElement( const KeyType_t &key ) const
	{
		return Find( key ) != InvalidIndex();
	}

	void EnsureCapacity( int num )							{ m_hashMap.EnsureCapacity( num ); m_listLRU.EnsureCapacity( num ); }

	void RemoveAt( IndexType_t i );
	bool Remove( const KeyType_t &key );
	void RemoveAll()										{ m_hashMap.RemoveAll(); m_listLRU.RemoveAll(); }
	void Purge()											{ m_hashMap.Purge(); m_listLRU.Purge(); }

#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, const char *pchName )
	{
		ValidateObj( m_hashMap );
		ValidateObj( m_listLRU );
	}
#endif // DBGFLAG_VALIDATE

private:
	struct ElementBucket_t
	{
		ElementBucket_t() : idxLRU(-1) { }
		ElementBucket_t( const ElemType_t &_e, const IndexType_t &_idxLRU )
		: e(_e), idxLRU(_idxLRU) { }
		
		ElementBucket_t( const IndexType_t &_idxLRU )
		: idxLRU(_idxLRU) { }
		
		ElemType_t  e;
		IndexType_t idxLRU;
	};
	
	void PromoteOnAccess( IndexType_t nHashIdx ) { if ( !m_bRetainInsertOrderOnly ) InternalPromote( nHashIdx ); }
	void InternalPromote( IndexType_t nHashIdx );
	
	CUtlHashMap<KeyType_t, ElementBucket_t, L, H>		m_hashMap;
	CUtlLinkedList< IndexType_t >	m_listLRU;
	bool							m_bRetainInsertOrderOnly;
};

template <typename K, typename T, typename L, typename H> 
inline typename CUtlHashMapLRU<K,T,L,H>::IndexType_t
CUtlHashMapLRU<K,T,L,H>::Insert(const KeyType_t &key, const ElemType_t &insert)
{
	// hashmap's insert is the same as InsertOrReplace
	return InsertOrReplace( key, insert );
}

template <typename K, typename T, typename L, typename H> 
inline typename CUtlHashMapLRU<K,T,L,H>::IndexType_t
CUtlHashMapLRU<K,T,L,H>::FindOrInsert(const KeyType_t &key, const ElemType_t &insert)
{
	IndexType_t nHashIdx = m_hashMap.FindOrInsert( key, ElementBucket_t( insert, -1 ) );
	if ( m_hashMap[ nHashIdx ].idxLRU == -1 )
	{
		m_hashMap[ nHashIdx ].idxLRU = m_listLRU.AddToHead( nHashIdx );
	}
	return nHashIdx;
}

template <typename K, typename T, typename L, typename H> 
inline typename CUtlHashMapLRU<K,T,L,H>::IndexType_t
CUtlHashMapLRU<K,T,L,H>::InsertWithDupes(const KeyType_t &key, const ElemType_t &insert)
{
	IndexType_t nHashIdx = m_hashMap.InsertWithDupes( key, ElementBucket_t( insert, 0 ) );
	IndexType_t nListIdx = m_listLRU.AddToHead( nHashIdx );
	m_hashMap.Element(nHashIdx).idxLRU = nListIdx;
	
	return nHashIdx;
}

template <typename K, typename T, typename L, typename H> 
inline typename CUtlHashMapLRU<K,T,L,H>::IndexType_t
CUtlHashMapLRU<K,T,L,H>::InsertOrReplace(const KeyType_t &key, const ElemType_t &insert)
{
	IndexType_t nHashIdx = m_hashMap.Find( key );
	if ( nHashIdx != m_hashMap.InvalidIndex() )
	{
		// replace and promote
		m_hashMap.Element( nHashIdx ).e = insert;
		InternalPromote( nHashIdx );
		return nHashIdx;
	}
	
	// No dups, so safe to do this:
	return InsertWithDupes( key, insert );
}

template <typename K, typename T, typename L, typename H> 
inline void CUtlHashMapLRU<K,T,L,H>::RemoveAt(IndexType_t i)
{
	m_listLRU.Remove( m_hashMap.Element( i ).idxLRU );
	m_hashMap.RemoveAt( i );
}

template <typename K, typename T, typename L, typename H> 
inline bool CUtlHashMapLRU<K,T,L,H>::Remove(const KeyType_t &key)
{
	IndexType_t nHashIdx = m_hashMap.Find( key );
	if ( nHashIdx != m_hashMap.InvalidIndex() )
	{
		RemoveAt( nHashIdx );
		return true;
	}
	return false;
}

template <typename K, typename T, typename L, typename H> 
inline typename CUtlHashMapLRU<K,T,L,H>::IndexType_t
CUtlHashMapLRU<K,T,L,H>::NextOlder( IndexType_t nHashIdx )
{
	IndexType_t nListIdx = m_listLRU.Next( m_hashMap.Element( nHashIdx ).idxLRU );
	if ( nListIdx != m_listLRU.InvalidIndex() )
		return m_listLRU.Element( nListIdx );
	return InvalidIndex();
}

template <typename K, typename T, typename L, typename H> 
inline typename CUtlHashMapLRU<K,T,L,H>::IndexType_t
CUtlHashMapLRU<K,T,L,H>::NextNewer( IndexType_t nHashIdx )
{
	IndexType_t nListIdx = m_listLRU.Previous( m_hashMap.Element( nHashIdx ).idxLRU );
	if ( nListIdx != m_listLRU.InvalidIndex() )
		return m_listLRU.Element( nListIdx );
	return InvalidIndex();
}

template <typename K, typename T, typename L, typename H> 
inline void CUtlHashMapLRU<K,T,L,H>::InternalPromote(IndexType_t nHashIdx)
{
	const ElementBucket_t &bucket = m_hashMap.Element( nHashIdx );
	if ( m_listLRU.Head() != bucket.idxLRU )
		m_listLRU.LinkToHead( bucket.idxLRU );
}

template <typename K, typename T, typename L, typename H> 
inline typename CUtlHashMapLRU<K,T,L,H>::IndexType_t
CUtlHashMapLRU<K,T,L,H>::LeastRecentUsed() const
{
	IndexType_t nListIdx = m_listLRU.Tail();
	if ( nListIdx != m_listLRU.InvalidIndex() )
		return m_listLRU.Element( nListIdx ); 
	return InvalidIndex();
}

template <typename K, typename T, typename L, typename H> 
inline typename CUtlHashMapLRU<K,T,L,H>::IndexType_t
CUtlHashMapLRU<K,T,L,H>::MostRecentUsed() const
{
	IndexType_t nListIdx = m_listLRU.Head();
	if ( nListIdx != m_listLRU.InvalidIndex() )
		return m_listLRU.Element( nListIdx ); 
	return InvalidIndex();
}


#endif // UTLHASHMAPLRU_H
