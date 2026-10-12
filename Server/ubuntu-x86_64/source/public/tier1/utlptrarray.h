//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#ifndef UTLPTRARRAY_H
#define UTLPTRARRAY_H

#ifdef _WIN32
#pragma once
#endif


#define FOR_EACH_PTR_ARRAY( arrayName, iteratorName ) \
	for ( uint iteratorName = 0; iteratorName < (arrayName).Count(); iteratorName++ )

#define FOR_EACH_PTR_ARRAY_BACK( arrayName, iteratorName ) \
	for ( int iteratorName = (arrayName).Count()-1; iteratorName >= 0; iteratorName-- )

//-----------------------------------------------------------------------------
// The CUtlPtrArray class:
// Wraps an array pointer and count. Intended to be as light weight as possible
//-----------------------------------------------------------------------------
#include "tier0/memdbgon.h"
template <class T>
class CUtlPtrArray
{
public:
	CUtlPtrArray()
	{
		m_pMemory = NULL;
		m_cElements = 0;
	}

	CUtlPtrArray( const T *pMemoryToCopy, int cElements )
	{ 
		m_pMemory = NULL;
		m_cElements = 0;
		Copy( pMemoryToCopy, cElements );
	}
	
	~CUtlPtrArray()
	{
		Purge();
	}

	void Allocate( uint cElements )
	{
		Purge();
		m_cElements = cElements;
		m_pMemory = (T*)malloc( cElements * sizeof( T ) );

		// Invoke default constructors
		for ( uint i = 0; i < cElements; i++ )
			Construct( &m_pMemory[ i ] );
	}

	void Copy( const T *pMemory, uint cElements )
	{
		Allocate( cElements );
		for( uint i = 0; i < cElements; i++ )
			m_pMemory[i] = pMemory[i];
	}

	void Purge()
	{
		if ( m_pMemory )
		{
			for ( uint i = 0; i < m_cElements; i++ )
				Destruct( &m_pMemory[ i ] );

			free( m_pMemory );
			m_pMemory = NULL;
		}

		m_cElements = 0;
	}

	uint Count() const { return m_cElements; }

	T& operator[]( uint i )
	{
		return Element( i );
	}

	const T& operator[]( uint i ) const
	{
		return Element( i );
	}

	T& Element( uint i )
	{
		Assert( i < m_cElements );
		return m_pMemory[i];
	}

	const T& Element( uint i ) const
	{
		Assert( i < m_cElements );
		return m_pMemory[i];
	}

	T* Base()
	{
		return m_pMemory;
	}

	const T* Base() const
	{
		return m_pMemory;
	}

	// takes ownership of a buffer. WILL FREE ON DESTRUCT!
	void TakeOwnership( T *pMemory, int cElements )
	{
		Purge();
		m_pMemory = pMemory;
		m_cElements = cElements;		
	}

	// drops ownership owned buffer. Caller should free memory
	void DetatchAndClear()
	{
		m_pMemory = NULL;
		m_cElements = 0;
	}

	void Swap( CUtlPtrArray< T > &rhs )
	{
		T *pOther = rhs.m_pMemory;
		uint cOther = rhs.m_cElements;

		rhs.m_pMemory = m_pMemory;
		rhs.m_cElements = m_cElements;
		m_pMemory = pOther;
		m_cElements = cOther;
	}


#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, const char *pchName )
	{
		validator.Push( "CUtlPtrArray", this, pchName );

		if ( NULL != m_pMemory )
			validator.ClaimMemory( m_pMemory );

		validator.Pop();
	}
#endif // DBGFLAG_VALIDATE

private:
	CUtlPtrArray( const CUtlPtrArray &rhs );
	CUtlPtrArray& operator=(const CUtlPtrArray &rhs) const;

	T *m_pMemory;
	uint m_cElements;
};
#include "tier0/memdbgoff.h"

#endif // UTLPTRARRAY_H