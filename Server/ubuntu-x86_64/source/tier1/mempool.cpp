//===== Copyright 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "tier1/mempool.h"
#include <stdio.h>
#include <memory.h>
#include "tier0/dbg.h"
#include <ctype.h>
#include "tier1/strtools.h"

#ifndef _PS3
#if defined( OSX )
#include <malloc/malloc.h>
#else
#include <malloc.h>
#endif
#endif

// Should be last include
#include "tier0/memdbgon.h"

MemoryPoolReportFunc_t CUtlMemoryPool::g_ReportFunc = 0;

//-----------------------------------------------------------------------------
// Error reporting...  (debug only)
//-----------------------------------------------------------------------------

void CUtlMemoryPool::SetErrorReportFunc( MemoryPoolReportFunc_t func )
{
	g_ReportFunc = func;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CUtlMemoryPool::CUtlMemoryPool( int blockSize, int numElements, int growMode, const char *pszAllocOwner, int nAlignment )
{
#ifdef _X360
	if( numElements > 0 && growMode != GROW_NONE )
	{
		numElements = 1;
	}
#endif

	m_nAlignment = ( nAlignment != 0 ) ? nAlignment : 1;
	Assert( IsPowerOfTwo( m_nAlignment ) );
	m_BlockSize = blockSize < sizeof(void*) ? sizeof(void*) : blockSize;
	m_BlockSize = AlignValue( m_BlockSize, m_nAlignment );
	m_BlocksPerBlob = numElements;
	m_PeakAlloc = 0;
	m_GrowMode = growMode;
	if ( !pszAllocOwner )
	{
		pszAllocOwner = __FILE__;
	}
	m_pszAllocOwner = pszAllocOwner;
	Init();
	AddNewBlob();
}

//-----------------------------------------------------------------------------
// Purpose: Frees the memory contained in the mempool, and invalidates it for
//			any further use.
// Input  : *memPool - the mempool to shutdown
//-----------------------------------------------------------------------------
CUtlMemoryPool::~CUtlMemoryPool()
{
	if (m_BlocksAllocated > 0)
	{
		ReportLeaks();
	}
	Clear();
}


//-----------------------------------------------------------------------------
// Resets the pool
//-----------------------------------------------------------------------------
void CUtlMemoryPool::Init()
{
	m_NumBlobs = 0;
	m_BlocksAllocated = 0;
	m_pHeadOfFreeList = 0;
	m_BlobHead.m_pNext = m_BlobHead.m_pPrev = &m_BlobHead;
}


//-----------------------------------------------------------------------------
// Frees everything
//-----------------------------------------------------------------------------
void CUtlMemoryPool::Clear()
{
	// Free everything..
	CBlob *pNext;
	for( CBlob *pCur = m_BlobHead.m_pNext; pCur != &m_BlobHead; pCur = pNext )
	{
		pNext = pCur->m_pNext;
		free( pCur );
	}
	Init();
}


//-----------------------------------------------------------------------------
// Is an allocation within the pool? 
//-----------------------------------------------------------------------------
bool CUtlMemoryPool::IsAllocationWithinPool( void *pMem ) const
{
	for( CBlob *pCur = m_BlobHead.m_pNext; pCur != &m_BlobHead; pCur = pCur->m_pNext )
	{
		// Is the allocation within the blob?
		if ( ( pMem < pCur->m_Data ) || ( pMem >= pCur->m_Data + pCur->m_NumBytes ) )
			continue;

		// Make sure the allocation is on a block boundary
		intp pFirstAllocation = AlignValue( ( intp ) pCur->m_Data, m_nAlignment );

		intp nOffset = (intp)pMem - pFirstAllocation;
		return ( nOffset % m_BlockSize ) == 0;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Reports memory leaks 
//-----------------------------------------------------------------------------
void CUtlMemoryPool::ReportLeaks()
{
#ifdef _DEBUG
	if (!g_ReportFunc)
		return;

	g_ReportFunc("Memory leak: mempool blocks left in memory: %d\n", m_BlocksAllocated);

	// walk and destroy the free list so it doesn't intefere in the scan
	while (m_pHeadOfFreeList != NULL)
	{
		void *next = *((void**)m_pHeadOfFreeList);
		memset(m_pHeadOfFreeList, 0, m_BlockSize);
		m_pHeadOfFreeList = next;
	}

	g_ReportFunc("Dumping memory: \'");

	for( CBlob *pCur=m_BlobHead.m_pNext; pCur != &m_BlobHead; pCur=pCur->m_pNext )
	{
		// scan the memory block and dump the leaks
		char *scanPoint = (char *)pCur->m_Data;
		char *scanEnd = pCur->m_Data + pCur->m_NumBytes;
		bool needSpace = false;

		while (scanPoint < scanEnd)
		{
			// search for and dump any strings
			if ((unsigned)(*scanPoint + 1) <= 256 && V_isprint(*scanPoint))
			{
				g_ReportFunc("%c", *scanPoint);
				needSpace = true;
			}
			else if (needSpace)
			{
				needSpace = false;
				g_ReportFunc(" ");
			}

			scanPoint++;
		}
	}

	g_ReportFunc("\'\n");
#endif // _DEBUG
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CUtlMemoryPool::AddNewBlob()
{
	MEM_ALLOC_CREDIT_(m_pszAllocOwner);

	int sizeMultiplier;

	if( m_GrowMode == GROW_SLOW )
	{
		sizeMultiplier = 1;
	}
	else
	{
		if ( m_GrowMode == GROW_NONE )
		{
			// Can only have one allocation when we're in this mode
			if( m_NumBlobs != 0 )
			{
				Assert( !"CUtlMemoryPool::AddNewBlob: mode == GROW_NONE" );
				return;
			}
		}
		
		// GROW_FAST and GROW_NONE use this.
		sizeMultiplier = m_NumBlobs + 1;
	}

	// maybe use something other than malloc?
	int nElements = m_BlocksPerBlob * sizeMultiplier;
	int blobSize = m_BlockSize * nElements;
	CBlob *pBlob = (CBlob*)malloc( sizeof(CBlob) - 1 + blobSize + ( m_nAlignment - 1 ) );
	Assert( pBlob );
	
	// Link it in at the end of the blob list.
	pBlob->m_NumBytes = blobSize;
	pBlob->m_pNext = &m_BlobHead;
	pBlob->m_pPrev = pBlob->m_pNext->m_pPrev;
	pBlob->m_pNext->m_pPrev = pBlob->m_pPrev->m_pNext = pBlob;

	// setup the free list
	m_pHeadOfFreeList = AlignValue( pBlob->m_Data, m_nAlignment );
	Assert (m_pHeadOfFreeList);

	void **newBlob = (void**)m_pHeadOfFreeList;
	for (int j = 0; j < nElements-1; j++)
	{
		newBlob[0] = (char*)newBlob + m_BlockSize;
		newBlob = (void**)newBlob[0];
	}

	// null terminate list
	newBlob[0] = NULL;
	m_NumBlobs++;
}


void* CUtlMemoryPool::Alloc()
{
	return Alloc( m_BlockSize );
}


void* CUtlMemoryPool::AllocZero()
{
	return AllocZero( m_BlockSize );
}


//-----------------------------------------------------------------------------
// Purpose: Allocs a single block of memory from the pool.  
// Input  : amount - 
//-----------------------------------------------------------------------------
void *CUtlMemoryPool::Alloc( size_t amount )
{
	void *returnBlock;

	if ( amount > (size_t)m_BlockSize )
		return NULL;

	if ( !m_pHeadOfFreeList )
	{
		// returning NULL is fine in GROW_NONE
		if ( m_GrowMode == GROW_NONE && m_NumBlobs > 0 )
		{
			//Assert( !"CUtlMemoryPool::Alloc: tried to make new blob with GROW_NONE" );
			return NULL;
		}

		// overflow
		AddNewBlob();

		// still failure, error out
		if ( !m_pHeadOfFreeList )
		{
			Assert( !"CUtlMemoryPool::Alloc: ran out of memory" );
			return NULL;
		}
	}
	m_BlocksAllocated++;
	m_PeakAlloc = MAX(m_PeakAlloc, m_BlocksAllocated);

	returnBlock = m_pHeadOfFreeList;

	// move the pointer the next block
	m_pHeadOfFreeList = *((void**)m_pHeadOfFreeList);

	return returnBlock;
}

//-----------------------------------------------------------------------------
// Purpose: Allocs a single block of memory from the pool, zeroes the memory before returning
// Input  : amount - 
//-----------------------------------------------------------------------------
void *CUtlMemoryPool::AllocZero( size_t amount )
{
	void *mem = Alloc( amount );
	if ( mem )
	{
		V_memset( mem, 0x00, ( int )amount );
	}
	return mem;
}

//-----------------------------------------------------------------------------
// Purpose: Frees a block of memory
// Input  : *memBlock - the memory to free
//-----------------------------------------------------------------------------
void CUtlMemoryPool::Free( void *memBlock )
{
	if ( !memBlock )
		return;  // trying to delete NULL pointer, ignore

#ifdef _DEBUG
	// check to see if the memory is from the allocated range
	bool bOK = false;
	for( CBlob *pCur=m_BlobHead.m_pNext; pCur != &m_BlobHead; pCur=pCur->m_pNext )
	{
		if (memBlock >= pCur->m_Data && (char*)memBlock < (pCur->m_Data + pCur->m_NumBytes))
		{
			bOK = true;
		}
	}
	Assert (bOK);
#endif // _DEBUG

#ifdef _DEBUG	
	// invalidate the memory
	memset( memBlock, 0xDD, m_BlockSize );
#endif

	m_BlocksAllocated--;

	// make the block point to the first item in the list
	*((void**)memBlock) = m_pHeadOfFreeList;

	// the list head is now the new block
	m_pHeadOfFreeList = memBlock;
}

int CUtlMemoryPool::Size() const		
{ 
	uint32 size = 0;

	for( CBlob *pCur=m_BlobHead.m_pNext; pCur != &m_BlobHead; pCur=pCur->m_pNext )
	{
		size += pCur->m_NumBytes;
	}
	return size;
}


//-----------------------------------------------------------------------------
//
// CScratchMemoryPool
//
//-----------------------------------------------------------------------------
CUtlScratchMemoryPool::CUtlScratchMemoryPool()
{
	m_pFirstBlock = NULL;
	m_nBlockSize = 0;
	m_bSearchAllBlocks = false;
#if UTL_SCRATCH_MEMORY_POOL_STATS
	m_nNumAllocs = 0;
	m_nBytesAllocated = 0;
	m_nBytesWasted = 0;
#endif
}

CUtlScratchMemoryPool::CUtlScratchMemoryPool( int nBlockSize, void *pExternalMem, bool bSearchAllBlocks )
{
	m_pFirstBlock = NULL;
	m_nBlockSize = 0;
	m_bSearchAllBlocks = false;

	Init( nBlockSize, pExternalMem, bSearchAllBlocks );
}

CUtlScratchMemoryPool::~CUtlScratchMemoryPool()
{
	FreeAll();
}

void CUtlScratchMemoryPool::Init( int nBlockSize, void *pExternalMem, bool bSearchAllBlocks )
{
	Assert( !IsInitialized() );

	// The stack memory must be 16-byte aligned, as must be the block size
	Assert( AlignValue( (intp)pExternalMem, 16 ) == (intp)pExternalMem );
	Assert( AlignValue( nBlockSize, 16 ) == nBlockSize );

	// It's nonsensical if this is not true
	Assert( nBlockSize >= 16 * sizeof( MemoryBlock_t ) );

	m_pFirstBlock = NULL;
	m_nBlockSize = nBlockSize;
	m_bSearchAllBlocks = bSearchAllBlocks;
#if UTL_SCRATCH_MEMORY_POOL_STATS
	m_nNumAllocs = 0;
	m_nBytesAllocated = 0;
	m_nBytesWasted = 0;
#endif

	if ( pExternalMem )
	{
		Assert( nBlockSize <= 128 * 1024 );	// Using more than 128k of stack memory is probably not a good idea
		m_pFirstBlock = (MemoryBlock_t*)pExternalMem;
		m_pFirstBlock->m_pNext = NULL;
		m_pFirstBlock->m_bSkipDeallocation = true;
		m_pFirstBlock->m_nBytesFree = nBlockSize - sizeof( MemoryBlock_t );
		Assert( AlignValue( m_pFirstBlock->m_nBytesFree, 16 ) == m_pFirstBlock->m_nBytesFree );

		//MemAlloc_InitFillAlloc( m_pFirstBlock->GetAllocPtr( 0 ), m_pFirstBlock->m_nBytesFree, m_pFirstBlock );
	}
}

void* CUtlScratchMemoryPool::AllocAligned( int nSizeInBytes, int nAlignment )
{
	AssertDbg( IsInitialized() );
	AssertDbg( nAlignment >= 1 && nAlignment <= 16 && IsPowerOfTwo( nAlignment ) );

	if ( nSizeInBytes == 0 )
		return NULL;

#if UTL_SCRATCH_MEMORY_POOL_STATS
	m_nNumAllocs++;
	m_nBytesAllocated += nSizeInBytes;
#endif

	MemoryBlock_t *pBlock = m_pFirstBlock;
	while ( pBlock )
	{
		// Determine our free space adjusted by the requested alignment.
		// This rounds down as we're adjusting for how much free space we
		// will lose due to alignment.
		int nBytesFreeAligned = pBlock->m_nBytesFree & ~( nAlignment - 1 );

		// Do we have room?
		if ( nBytesFreeAligned >= nSizeInBytes )
		{
			// Yes! Use it
#if UTL_SCRATCH_MEMORY_POOL_STATS
			// Track space wasted due to alignment.
			m_nBytesWasted += pBlock->m_nBytesFree - nBytesFreeAligned;
#endif
			int nOffset = GetBlockUsableBytes() - nBytesFreeAligned;
			pBlock->m_nBytesFree = nBytesFreeAligned - nSizeInBytes;
			uint8* pMemory = pBlock->GetAllocPtr( nOffset );
			AssertDbg( AlignValue( (intp)pMemory, nAlignment ) == (intp)pMemory );
			return pMemory;
		}

		if ( !m_bSearchAllBlocks )
		{
			break;
		}

		pBlock = pBlock->m_pNext;
	}

	// No room? Allocate a new block

#if UTL_SCRATCH_MEMORY_POOL_STATS
	if ( !m_bSearchAllBlocks )
	{
		// Anything free in the current block will now be unusable.
		m_nBytesWasted += m_pFirstBlock->m_nBytesFree;
	}
#endif

	void *pMemory = AddNewBlock( nSizeInBytes );
	AssertDbg( AlignValue( (intp)pMemory, nAlignment ) == (intp)pMemory );
	return pMemory;
}

void *CUtlScratchMemoryPool::AddNewBlock( int nSizeInBytes )
{
	// Should we just allocate a standalone block?
	// Let's do it if the request is large enough (and, of course, we didn't fit in the alloc call before).
	bool bUseStandaloneBlock = ( nSizeInBytes > ( m_nBlockSize >> 1 ) );
	if ( !bUseStandaloneBlock )
	{
		// The block size must be a multiple of 16 bytes so by the IMemAlloc rules
		// it will always be 16-byte aligned.
		MemoryBlock_t *pNewBlock = (MemoryBlock_t*)MemAlloc_Alloc( m_nBlockSize );
		pNewBlock->m_bSkipDeallocation = false;
		pNewBlock->m_pNext = m_pFirstBlock;
		m_pFirstBlock = pNewBlock;
		pNewBlock->m_nBytesFree = GetBlockUsableBytes() - nSizeInBytes;

		// Return the memory after the block header
		return pNewBlock->GetAllocPtr( 0 );
	}
	else
	{
		// Make sure that the alloc size is a multiple of 16 bytes so by the IMemAlloc rules
		// it will always be 16-byte aligned.
		MemoryBlock_t *pNewBlock = (MemoryBlock_t*)MemAlloc_Alloc( AlignValue( nSizeInBytes + sizeof( MemoryBlock_t ), 16 ) );
		pNewBlock->m_bSkipDeallocation = false;
		pNewBlock->m_nBytesFree = 0;
		if ( m_pFirstBlock )
		{
			// Don't disturb the current block we're adding to
			pNewBlock->m_pNext = m_pFirstBlock->m_pNext;
			m_pFirstBlock->m_pNext = pNewBlock;
		}
		else
		{
			pNewBlock->m_pNext = NULL;
			m_pFirstBlock = pNewBlock;
		}

		// Return the memory after the block header
		return pNewBlock->GetAllocPtr( 0 );
	}
}

// It is sometimes possible to free the last allocation(s) you made
UtlScratchMemoryPoolMark_t CUtlScratchMemoryPool::GetCurrentAllocPoint() const
{
	Assert( !m_bSearchAllBlocks );

	UtlScratchMemoryPoolMark_t mark;
	mark.m_pBlock = m_pFirstBlock;
	mark.m_nBytesFree = GetFirstBytesFree();
#if UTL_SCRATCH_MEMORY_POOL_STATS
	mark.m_nNumAllocs = m_nNumAllocs;
	mark.m_nBytesAllocated = m_nBytesAllocated;
	mark.m_nBytesWasted = m_nBytesWasted;
#endif
	return mark;
}

void CUtlScratchMemoryPool::FreeToAllocPoint( UtlScratchMemoryPoolMark_t mark )
{
	Assert( !m_bSearchAllBlocks );

	// NOTE: This works even in light of the fact that oversized blocks are not
	// allocated as part of the current block, since those oversized blocks are always
	// inserted *prior* to the current block. It does mean that oversized blocks
	// will not get deallocated in the course of this function, though.
	while ( m_pFirstBlock != (MemoryBlock_t*)mark.m_pBlock )
	{
		Assert( m_pFirstBlock );
		MemoryBlock_t *pNext = m_pFirstBlock->m_pNext;
		Assert( !m_pFirstBlock->m_bSkipDeallocation );	// Illegal to free the external allocation
		MemAlloc_Free( m_pFirstBlock );
		m_pFirstBlock = pNext;
	}

	if ( m_pFirstBlock )
	{
		m_pFirstBlock->m_nBytesFree = mark.m_nBytesFree;
	}
#if UTL_SCRATCH_MEMORY_POOL_STATS
	m_nNumAllocs = mark.m_nNumAllocs;
	m_nBytesAllocated = mark.m_nBytesAllocated;
	m_nBytesWasted = mark.m_nBytesWasted;
#endif
	if ( m_pFirstBlock )
	{
		//MemAlloc_InitFillAlloc( m_pFirstBlock->GetAllocPtr( m_pFirstBlock->m_nBytesFree ), GetBlockUsableBytes() - m_pFirstBlock->m_nBytesFree, m_pFirstBlock );
	}
}

// Frees everything	(except the external allocation)
void CUtlScratchMemoryPool::FreeAll()
{
	MemoryBlock_t *pExternalAllocation = NULL;
	MemoryBlock_t *pNext;
	for ( MemoryBlock_t *pBlock = m_pFirstBlock; pBlock != NULL; pBlock = pNext )
	{
		pNext = pBlock->m_pNext;
		if ( !pBlock->m_bSkipDeallocation )
		{
			MemAlloc_Free( pBlock );
		}
		else
		{
			Assert( !pExternalAllocation );
			pExternalAllocation = pBlock;
			pExternalAllocation->m_pNext = NULL;	// Blat out the next ptr for next time
		}
	}
	m_pFirstBlock = pExternalAllocation;
	if ( m_pFirstBlock )
	{
		m_pFirstBlock->m_nBytesFree = GetBlockUsableBytes();
		//MemAlloc_InitFillAlloc( m_pFirstBlock->GetAllocPtr( 0 ), m_pFirstBlock->m_nBytesFree, m_pFirstBlock );
	}
#if UTL_SCRATCH_MEMORY_POOL_STATS
	m_nNumAllocs = 0;
	m_nBytesAllocated = 0;
	m_nBytesWasted = 0;
#endif
}

#if UTL_SCRATCH_MEMORY_POOL_STATS
void CUtlScratchMemoryPool::LogStats( LoggingChannelID_t nChannel, const char *pName ) const
{
	Log_Msg( nChannel, "Scratch memory pool stats for %s:\n", pName );
	Log_Msg( nChannel, "%u allocs with %u bytes allocated across %u %u-byte blocks%s\n",
		m_nNumAllocs, m_nBytesAllocated, GetNumBlocks(), m_nBlockSize,
		HasExternalBlock() ? " (one external)" : "" );
	Log_Msg( nChannel, "%u bytes free in first block, %u bytes wasted (unused total %u)\n",
		GetFirstBytesFree(), m_nBytesWasted, GetBytesUnused() );
}
#endif
