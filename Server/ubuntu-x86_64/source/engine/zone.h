//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#ifndef ZONE_H
#define ZONE_H
#pragma once

#include "tier0/dbg.h"



// [iestyn:2018.11.22]
//
//    The Hunk allocator predates the highly optimized Small Block Heap. At this point,
//    its value is greatly diminished and it is putting pressure on address space for 32-bit
//    game clients. Define HUNK_ALLOC_DEPRECATED to route all hunk allocs to the tier0 heap.
//
// $TODO: remove the hunk implementation altogether once this change is fully validated
//
#define HUNK_ALLOC_DEPRECATED

#if defined( HUNK_ALLOC_DEPRECATED )


#include "tier1/utlvector.h"

struct CHunkDeprecated
{
	CHunkDeprecated()
	{
		m_nTotalAllocated = 0;			// Total number of bytes allocated
		m_nLowMark = -1;				// Records the number of allocations performed during engine init (so we can quickly free anything allocated after this point)
		m_nLowMarkTotalAllocated = -1;	// Records the number of bytes allocated during engine init
	}

	void *Alloc( int size, const char *name, bool bClear )
	{
		// Hunk allocs must be 16-byte aligned.
		// On Windows, rounding up to a multiple of 16 would achieve this but that doesn't work for 32-bit Linux
		// so we use MemAlloc_AllocAligned (this wastes ~20 bytes per alloc but there are only ~20k hunk allocs so that's less than 0.5MB).
		MEM_ALLOC_CREDIT_( name );
		void* pResult = MemAlloc_AllocAligned( size, 16 );
		if ( bClear )
		{
			memset( pResult, 0, size );
		}
		m_Allocations.AddToTail( pResult );
		m_nTotalAllocated += size;
		return pResult;
	}

	int SetLowMark( void )
	{
		// We expect this to be called exactly once, on engine init
		CrashAssert( m_nLowMark == -1 && m_nLowMarkTotalAllocated == -1 );
		m_nLowMark = m_Allocations.Count();
		m_nLowMarkTotalAllocated = m_nTotalAllocated;
		return m_nLowMark;
	}

	void FreeToLowMark( int mark )
	{
		// Restore back to the state when LowMark was called
		CrashAssert( mark == m_nLowMark );
		while( m_Allocations.Count() > mark )
		{
			// MemAlloc_AllocAligned must be paired with MemAlloc_FreeAligned to ensure that correct pointer to the low-level block memory is released
			MemAlloc_FreeAligned( m_Allocations.Tail() );
			m_Allocations.RemoveMultipleFromTail( 1 );
		}
		m_nTotalAllocated = m_nLowMarkTotalAllocated;
	}

	void Purge()
	{
		m_nLowMark = 0;
		m_nLowMarkTotalAllocated = 0;
		FreeToLowMark( 0 );
		m_Allocations.Purge();
	}

	int Overhead()
	{
		return m_Allocations.NumAllocated() * sizeof( m_Allocations[0] );
	}

	void Print()
	{
		Msg( "Total used memory (on heap):  %d  (%d overhead)\n", (m_nTotalAllocated + Overhead()), Overhead() );
	}

	void CrashAssert( bool condition )
	{
		// If any of our asserts fail we will get nasty memory access bugs later on. It is far better to crash here immediately with an unambiguous callstack.
		if ( !condition )
		{
			static uint8* crashme = nullptr;
			crashme[0] = 0;
		}
	}

	int					m_nTotalAllocated;			// Total number of bytes allocated
	CUtlVector< void* >	m_Allocations;				// Individual allocations
	int					m_nLowMark;					// Records the number of allocations performed during engine init (so we can quickly free anything allocated after this point)
	int					m_nLowMarkTotalAllocated;	// Records the number of bytes allocated during engine init

	static CHunkDeprecated s_Hunk; // Singleton allocator
};

void Memory_Init( void );
void Memory_Shutdown( void );
inline void Hunk_OnMapStart( int nEstimatedBytes ) {}
inline void *Hunk_AllocName( int size, const char *name, bool bClear = true ) { return CHunkDeprecated::s_Hunk.Alloc( size, name, bClear ); }
inline int	Hunk_LowMark( void ) { return CHunkDeprecated::s_Hunk.SetLowMark(); }
inline void Hunk_FreeToLowMark( int mark ) { CHunkDeprecated::s_Hunk.FreeToLowMark( mark ); }
inline int Hunk_MallocSize() { return CHunkDeprecated::s_Hunk.m_nTotalAllocated; }
inline int Hunk_Size() { return CHunkDeprecated::s_Hunk.m_nTotalAllocated + CHunkDeprecated::s_Hunk.Overhead(); }
inline void Hunk_Print() { CHunkDeprecated::s_Hunk.Print(); }

// CHunkMemory must be 16-byte aligned, so use CUtlMemoryAligned<T,16>
template< typename T >
class CHunkMemory : public CUtlMemoryAligned< T, 16 >
{
public:
	typedef CUtlMemoryAligned< T, 16 > BaseClassUtlMemoryAligned_t;
public:
	CHunkMemory( int nGrowSize = 0, int nInitSize = 0 ) : BaseClassUtlMemoryAligned_t( nGrowSize, nInitSize ) {};
	CHunkMemory( T* pMemory, int numElements ) : BaseClassUtlMemoryAligned_t( pMemory, numElements ) {};
	CHunkMemory( const T* pMemory, int numElements ) : BaseClassUtlMemoryAligned_t( pMemory, numElements ) {};
};

#define HUNK_ALLOC_CREDIT_( _name_ )	MEM_ALLOC_CREDIT_( _name_ );


#else // HUNK_ALLOC_DEPRECATED

void Memory_Init (void);
void Memory_Shutdown( void );

void Hunk_OnMapStart( int nEstimatedBytes );

void *Hunk_AllocName (int size, const char *name, bool bClear = true );

int	Hunk_LowMark (void);
void Hunk_FreeToLowMark (int mark);

void Hunk_Check (void);

int Hunk_MallocSize();
int Hunk_Size();

void Hunk_Print();

// Deal with memory attribution for CHunkMemory
#define HUNK_ALLOC_CREDIT_( _name_ )	MEM_ALLOC_CREDIT_( _name_ ); CHunkAllocCredit hunkAllocAttributeAlloction( _name_ );
class CHunkAllocCredit
{
public:
	 CHunkAllocCredit( const char *name )	{ PushAllocDbgInfo( name ); };
	 ~CHunkAllocCredit( void )				{ PopAllocDbgInfo(); };

	static void PushAllocDbgInfo( const char *name )
	{
		Assert( name && name[0] );
		++s_DbgInfoStackDepth;
		Assert( s_DbgInfoStackDepth < DBG_INFO_STACK_DEPTH );
		if ( s_DbgInfoStackDepth < DBG_INFO_STACK_DEPTH )
			s_DbgInfoStack[s_DbgInfoStackDepth] = ( name && name[0] ) ? name : "CHunkMemory";
	}
	static void PopAllocDbgInfo( void )
	{
		Assert( s_DbgInfoStackDepth >= 0 );
		if ( s_DbgInfoStackDepth >= 0 )
			s_DbgInfoStack[ s_DbgInfoStackDepth-- ] = NULL;
	}
	static const char *GetAllocDbgInfo( void )
	{
		int index = MIN( s_DbgInfoStackDepth, (DBG_INFO_STACK_DEPTH-1) );
		return ( index >= 0 ) ? s_DbgInfoStack[index] : "CHunkMemory";
	}

	static const int DBG_INFO_STACK_DEPTH = 8;
	static const char *s_DbgInfoStack[ DBG_INFO_STACK_DEPTH ];
	static int s_DbgInfoStackDepth;
};

template< typename T >
class CHunkMemory
{
public:
	// constructor, destructor
	CHunkMemory( int nGrowSize = 0, int nInitSize = 0 )		{ m_pMemory = NULL; m_nAllocated = 0; if ( nInitSize ) Grow( nInitSize ); }
	CHunkMemory( T* pMemory, int numElements )				{ Assert( 0 ); }

	// Can we use this index?
	bool IsIdxValid( int i ) const							{ return (i >= 0) && (i < m_nAllocated); }

	// Gets the base address
	T* Base()												{ return (T*)m_pMemory; }
	const T* Base() const									{ return (T*)m_pMemory; }

	// element access
	T& operator[]( int i )									{ Assert( IsIdxValid(i) ); return Base()[i];	}
	const T& operator[]( int i ) const						{ Assert( IsIdxValid(i) ); return Base()[i];	}
	T& Element( int i )										{ Assert( IsIdxValid(i) ); return Base()[i];	}
	const T& Element( int i ) const							{ Assert( IsIdxValid(i) ); return Base()[i];	}

	// Attaches the buffer to external memory....
	void SetExternalBuffer( T* pMemory, int numElements )	{ Assert( 0 ); }

	// Size
	int NumAllocated() const								{ return m_nAllocated; }
	int Count() const										{ return m_nAllocated; }

	// Grows the memory, so that at least allocated + num elements are allocated
	void Grow( int num = 1 )								{ Assert( !m_nAllocated ); m_pMemory = (T *)Hunk_AllocName( num * sizeof(T), CHunkAllocCredit::GetAllocDbgInfo(), false ); m_nAllocated = num; }

	// Makes sure we've got at least this much memory
	void EnsureCapacity( int num )							{ Assert( num <= m_nAllocated ); }

	// Memory deallocation
	void Purge()											{ m_nAllocated = 0; }

	// Purge all but the given number of elements (NOT IMPLEMENTED IN )
	void Purge( int numElements )							{ Assert( 0 ); }

	// is the memory externally allocated?
	bool IsExternallyAllocated() const						{ return false; }

	// Set the size by which the memory grows
	void SetGrowSize( int size )							{}

private:
	T *m_pMemory;
	int m_nAllocated;
};

#endif // HUNK_ALLOC_DEPRECATED

#endif // ZONE_H
