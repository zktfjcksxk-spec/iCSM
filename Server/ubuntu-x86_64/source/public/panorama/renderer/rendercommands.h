//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef RENDERCOMMANDS_H
#define RENDERCOMMANDS_H
#pragma once

#include "tier0/basetypes.h"
#include "tier1/utlbuffer.h"
#include "tier1/refcount.h"
#include "mathlib/vmatrix.h"
#include "panorama/text/uitexttypes.h"
#include "memstack.h"


namespace panorama
{

const size_t k_unRenderCommandStackSize = 4096;


//
// These are all the possible actual rendering commands our surface can output. They correspond
// to the structs below.
//
enum ERenderCommand : uint8
{
	k_EBeginFrame					= 0,
	k_EEndFrame						= 1,
	k_EClearBackBuffer				= 2,
	//k_ESetRenderEffect				= 3,
	k_ECmdDrawTexturedRect				= 4,
	k_EPushAnimationAndTransformContext = 5,
	k_EPopAnimationAndTransformContext = 6,
	k_EPushCompositingLayer			= 7,
	k_EPopCompositingLayer			= 8,
	k_EDrawFilledRect				= 9,
	k_EDrawTextRegion				= 10,
	//k_ELoadOpacityMaskA8			= 11,
	k_EBeginPaintBackground			= 12,
	k_EEndPaintBackground			= 13,
	k_EPushClipLayer				= 14,
	k_EPopClipLayer					= 15,
	k_EBeginPaintLast				= 16,
	k_EEndPaintLast					= 17,
	//k_EDeleteTexture				= 18,
	k_EDeleteParticleSystem			= 19,
	k_ECmdFreeCompositingLayer		= 20,
	k_ECmdLockTexture				= 21,
	k_EDeletePanel					= 22,
	k_ERequestRenderCallback		= 23, // source 2 only, but reserve the value here always
	k_EPushPanelContextInLayer		= 24,
	k_EPopPanelContextInLayer		= 25,
	k_ENestedCommand				= 26,
	k_ENestedCommandList			= 27,
	k_EPushBlurPanels				= 28,

	k_ERenderCommand_Count			= 29
};
const char *GetRenderCommandTypeName( ERenderCommand eCommandType );

class IUITexture;
class CRenderCommandList;


//
// A RenderCommand_t is a rendering operation that we pass between panorama threads. They must
// always be allocated as part of a CRenderCommandList.
// 
// !!!!!!WARNING!!!!!!
// 
// RenderCommand_t objects DO NOT HAVE THEIR DESTRUCTORS CALLED!!!!!
//
// !!!!!!WARNING!!!!!!
//
struct RenderCommand_t
{
	RenderCommand_t( ERenderCommand eType )
		: eCommandType( eType )
		, pNextRenderCommand( nullptr )
	{
	}

	void CopyBaseCommandFrom( const RenderCommand_t &other, CRenderCommandList &commandList )
	{
		// Don't actually need to copy anything, but make sure things are setup correctly
		Assert( eCommandType == other.eCommandType );
		Assert( pNextRenderCommand == nullptr );
	}

	RenderCommand_t *pNextRenderCommand;
	ERenderCommand eCommandType;
};

//
// Simple class to collect stats about how a render command list is being used
//
struct RenderCommandListStats_t
{
	RenderCommandListStats_t() : unTotalCommandCount( 0 ), unTotalBytesAllocated( 0 )
	{
		memset( &aCommandCounts, 0, sizeof( aCommandCounts ) );
	}

	uint32 aCommandCounts[ k_ERenderCommand_Count ];
	uint32 unTotalCommandCount;
	uint32 unTotalBytesAllocated;
};

//
// A CRenderCommandList acts as a linked list of render commands allocated in a memory stack.
// Because the allocations are sequential, iterating through a CRenderCommandList has memory
// caching characteristics more similar to a vector than a linked list, which makes it optimal
// for the render command buffers panorama uses to transfer data between threads.
// 
// !!!!!!WARNING!!!!!!
//
// Objects allocated via a CRenderCommandList WILL NOT HAVE THEIR DESTRUCTOR CALLED. Instead,
// the internal memory stacks are freed en-masse.
//
// !!!!!!WARNING!!!!!!
// 
class CRenderCommandList : public CRefCount
{
public:
	CRenderCommandList();
	~CRenderCommandList();

	// Allocate a new render command and add it to the list
	template < typename T > T *PushCommand()
	{
		T* pCommand = AllocType< T >();
		if ( m_pFirstCommand == nullptr )
		{
			Assert( m_pLastCommand == nullptr );
			m_pFirstCommand = pCommand;
		}
		else
		{
			Assert( m_pLastCommand->pNextRenderCommand == nullptr );
			m_pLastCommand->pNextRenderCommand = pCommand;
		}

		m_pLastCommand = pCommand;
		return pCommand;
	}

	RenderCommand_t *GetFirstCommand() const { return m_pFirstCommand; }

	// Allocates a chunk of memory of the given size.
	inline void *Alloc( uint32 unSize )
	{
		m_unTotalBytesAllocated += unSize;

		// If the allocation would take more than half of the space of a memory stack,
		// just allocate it on its own instead.
		if ( unSize >= k_unRenderCommandStackSize / 2 )
		{
			byte *pOversizedAlloc = new byte[ unSize ];
			m_vecOversizedAllocs.AddToTail( pOversizedAlloc );
			return pOversizedAlloc;
		}

		CMemoryStack *pCurrentMemoryStack = m_vecMemoryStacks.IsEmpty() ? nullptr : m_vecMemoryStacks.Tail();
		if ( !pCurrentMemoryStack || !pCurrentMemoryStack->WillAllocSucceed( ( unsigned int )unSize ) )
		{
			pCurrentMemoryStack = AcquireMemoryStack();
			m_vecMemoryStacks.AddToTail( pCurrentMemoryStack );
		}

		return pCurrentMemoryStack->Alloc( ( unsigned int )unSize, true );
	}

	// Create an object of the given type. Does call the object's constructor.
	template < typename T > T *AllocType()
	{
		void *pMemory = Alloc( sizeof( T ) );
		return new ( pMemory )T();
	}

	// Copy the given string into memory allocated as part of this list
	inline const char *CopyString( const char *pszString )
	{
		if ( !pszString )
			return nullptr;

		int nLength = V_strlen( pszString );

		char *pszCopy = ( char * )Alloc( nLength + 1 );
		V_strcpy( pszCopy, pszString );
		return pszCopy;
	}

	uint32 GetTotalBytesAllocated() const { return m_unTotalBytesAllocated; }

	void AddTextureReference( IUITexture *pTexture );
	void AddObjectReference( CRefCount *pObject );
	void AddObjectReferenceDelayedRelease( CRefCount *pObject );

	void CopyObjectReferences( CRenderCommandList &other );

private:
	virtual CMemoryStack *AcquireMemoryStack();

	RenderCommand_t *m_pFirstCommand;
	RenderCommand_t *m_pLastCommand;
	CUtlVectorFixedGrowable< CMemoryStack *, 4 > m_vecMemoryStacks; // todo(ericl): switch to using CUtlScratchMemoryPool
	CUtlVector< byte * > m_vecOversizedAllocs;
	CUtlVectorFixedGrowable< IUITexture *, 8 > m_vecTextures;
	CUtlVector< CRefCount * > m_vecRefCountObjs;
	CUtlVector< CRefCount * > m_vecRefCountObjsDelayedRelease;
	uint32 m_unTotalBytesAllocated;
};

// Helper method to copy data from one spot to another using the given CRenderCommandList.
template < typename T > void CopyRenderData( T &to, const T &from, CRenderCommandList &commandList )
{
	// By default, use the CopyFrom method that each type should have
	to.CopyFrom( from, commandList );
}

// Specializations for primitive types
template <> inline void CopyRenderData< double >( double &to, const double &from, CRenderCommandList &commandList ) { to = from; }
template <> inline void CopyRenderData< float >( float &to, const float &from, CRenderCommandList &commandList ) { to = from; }
template <> inline void CopyRenderData< uint32 >( uint32 &to, const uint32 &from, CRenderCommandList &commandList ) { to = from; }
template <> inline void CopyRenderData< uint64 >( uint64 &to, const uint64 &from, CRenderCommandList &commandList ) { to = from; }

// Copy a pointer, allocating as necessary
template < typename T > void CopyRenderDataPointer( T *&to, T * const &from, CRenderCommandList &commandList )
{
	Assert( to == nullptr );
	if ( from )
	{
		to = commandList.AllocType< T >();
		CopyRenderData( *to, *from, commandList );
	}
}

// Helper method to swap the internals of some render data
template < typename T > void SwapRenderData( T &a, T &b )
{
	// By default, assume there's a swap method on the data type
	a.Swap( b );
}

// Specializations for primitive types
template <> inline void SwapRenderData< double >( double &a, double &b )	{ std::swap( a, b ); }
template <> inline void SwapRenderData< float >( float &a, float &b )		{ std::swap( a, b ); }
template <> inline void SwapRenderData< uint32 >( uint32 &a, uint32 &b )	{ std::swap( a, b ); }
template <> inline void SwapRenderData< uint64 >( uint64 &a, uint64 &b )	{ std::swap( a, b ); }


//
// A linked list that works with an CRenderCommandList as its allocator. Note that you can't remove items from the list -
// you can only add to the end or iterate through it. Because the CRenderCommandList allocator allocates sequentially, walking these
// lists should have the performance of a vector rather than a traditional linked list. To add items to this list, use a
// CRenderDataListBaseBuilder
//
class CRenderDataListBase
{
public:
	CRenderDataListBase()
		: m_pStartNode( nullptr )
	{
	}

	const void* GetFirst() const
	{
		return m_pStartNode ? &m_pStartNode->data : nullptr;
	}

	const void* GetNext( const void *pCurrent ) const
	{
		const ListNode_t *pCurrentNode = reinterpret_cast< const ListNode_t * >( reinterpret_cast< const byte * >( pCurrent ) - offsetof( ListNode_t, data ) );
		const ListNode_t *pNextNode = pCurrentNode->pNext;
		return pNextNode ? &pNextNode->data : nullptr;
	}

	void *GetFirstMutable()
	{
		return const_cast< void * >( GetFirst() );
	}

	void *GetNextMutable( void *pCurrent )
	{
		return const_cast< void * >( GetNext( pCurrent ) );
	}

	bool IsEmpty() const
	{
		return m_pStartNode == nullptr;
	}

	struct ListNode_t
	{
		ListNode_t *pNext;
		byte data[ 1 ];
	};

	void Swap( CRenderDataListBase &other )
	{
		std::swap( m_pStartNode, other.m_pStartNode );
	}

	// Range-based for loop support
	class CIterator
	{
	public:
		CIterator( const CRenderDataListBase *pList, const void *pCurrent ) : m_pList( pList ), m_pCurrent( pCurrent )
		{
			Assert( m_pList );
		}

		const void *operator*() const { return m_pCurrent; }
		void operator++() { m_pCurrent = m_pList->GetNext( m_pCurrent ); }
		bool operator!=( const CIterator& other ) const { return m_pList != other.m_pList || m_pCurrent != other.m_pCurrent; }

	private:
		const CRenderDataListBase *m_pList;
		const void *m_pCurrent;
	};
	CIterator begin() const { return CIterator( this, GetFirst() ); }
	CIterator end() const { return CIterator( this, nullptr ); }

	class CMutableIterator
	{
	public:
		CMutableIterator( CRenderDataListBase *pList, void *pCurrent ) : m_pList( pList ), m_pCurrent( pCurrent )
		{
			Assert( m_pList );
		}

		void *operator*() const { return m_pCurrent; }
		void operator++() { m_pCurrent = m_pList->GetNextMutable( m_pCurrent ); }
		bool operator!=( const CMutableIterator& other ) const { return m_pList != other.m_pList || m_pCurrent != other.m_pCurrent; }

	private:
		CRenderDataListBase *m_pList;
		void *m_pCurrent;
	};
	CMutableIterator begin() { return CMutableIterator( this, GetFirstMutable() ); }
	CMutableIterator end() { return CMutableIterator( this, nullptr ); }


private:
	// No copy constructor.
	CRenderDataListBase( const CRenderDataListBase &other ) = delete;

	ListNode_t *m_pStartNode;

	friend class CRenderDataListBaseBuilder;
};


//
// A utility class to add items to a CRenderDataListBase. Internally caches a pointer to the final element of the
// list, so AddToTail will be O(1).
//
class CRenderDataListBaseBuilder
{
public:
	CRenderDataListBaseBuilder( CRenderDataListBase &list, CRenderCommandList *pCommandList )
		: m_list( list )
		, m_pCommandList( pCommandList )
	{
		AssertMsg( m_list.IsEmpty(), "It's unexpected and inefficient to add to a render data list that has already had items added to it." );

		m_pLastNode = m_list.m_pStartNode;
		while ( m_pLastNode && m_pLastNode->pNext != nullptr )
		{
			m_pLastNode = m_pLastNode->pNext;
		}
	}

	void *AddToTail( uint32 unBytesSize )
	{
		uint32 unTotalBytesSize = sizeof( CRenderDataListBase::ListNode_t* ) + unBytesSize;
		CRenderDataListBase::ListNode_t *pNewNode = reinterpret_cast< CRenderDataListBase::ListNode_t * >( m_pCommandList->Alloc( unTotalBytesSize ) );
		AddNodeToTail( pNewNode );
		return &m_pLastNode->data;
	}

	template < typename T >
	T *AddToTailTyped()
	{
		void *pObjectMemory = AddToTail( sizeof( T ) );
		T *pObject = new ( pObjectMemory ) T();
		return pObject;
	}

	// If you call this directly, you are responsible for keeping the memory of the node alive
	// while the list exists. Almost always you should just call AddToTail instead.
	void AddNodeToTail( CRenderDataListBase::ListNode_t *pNewNode )
	{
		if ( m_list.m_pStartNode == nullptr )
		{
			Assert( m_pLastNode == nullptr );
			m_list.m_pStartNode = pNewNode;
		}
		else
		{
			Assert( m_pLastNode->pNext == nullptr );
			m_pLastNode->pNext = pNewNode;
		}

		m_pLastNode = pNewNode;
	}

private:
	CRenderDataListBase &m_list;
	CRenderCommandList *m_pCommandList;
	CRenderDataListBase::ListNode_t *m_pLastNode;
};

// Forward declare
template < typename T > class CRenderDataListBuilder;

//
// A typed linked list that works with an CRenderCommandList as its allocator. Note that you can't remove items from the list -
// you can only add to the end or iterate through it. Because the CRenderCommandList allocator allocates sequentially, walking these
// lists should have the performance of a vector rather than a traditional linked list. Use CRenderDataListBuilder< T > to add
// elements to the list.
//
template < typename T >
class CRenderDataList
{
public:
	CRenderDataList()
	{
	}

	void CopyFrom( const CRenderDataList< T > &from, CRenderCommandList &commandList )
	{
		CRenderDataListBuilder< T > listBuilder( *this, &commandList );
		for ( const T* pFromItem = from.GetFirst(); pFromItem != nullptr; pFromItem = from.GetNext( pFromItem ) )
		{
			T* pToItem = listBuilder.AddToTail();
			CopyRenderData( *pToItem, *pFromItem, commandList );
		}
	}

	const T *GetFirst() const { return reinterpret_cast< const T * >( m_listBase.GetFirst() ); }
	const T *GetNext( const T *pCurrent ) const { return reinterpret_cast< const T * >( m_listBase.GetNext( pCurrent ) ); }

	T *GetFirstMutable() { return reinterpret_cast< T * >( m_listBase.GetFirstMutable() ); }
	T *GetNextMutable( T *pCurrent ) { return reinterpret_cast< T * >( m_listBase.GetNextMutable( pCurrent ) ); }

	bool IsEmpty() const { return m_listBase.IsEmpty(); }


	void Swap( CRenderDataList< T > &other )
	{
		m_listBase.Swap( other.m_listBase );
	}

	// Range-based for loop support
	class CIterator
	{
	public:
		CIterator( const CRenderDataList< T > *pList, const T *pCurrent ) : m_pList( pList ), m_pCurrent( pCurrent )
		{
			Assert( m_pList );
		}

		const T *operator*( ) const { return m_pCurrent; }
		void operator++( ) { m_pCurrent = m_pList->GetNext( m_pCurrent ); }
		bool operator!=( const CIterator& other ) const { return m_pList != other.m_pList || m_pCurrent != other.m_pCurrent; }

	private:
		const CRenderDataList< T > *m_pList;
		const T *m_pCurrent;
	};
	CIterator begin() const { return CIterator( this, GetFirst() ); }
	CIterator end() const { return CIterator( this, nullptr ); }

	class CMutableIterator
	{
	public:
		CMutableIterator( CRenderDataList< T > *pList, T *pCurrent ) : m_pList( pList ), m_pCurrent( pCurrent )
		{
			Assert( m_pList );
		}

		T *operator*( ) const { return m_pCurrent; }
		void operator++( ) { m_pCurrent = m_pList->GetNextMutable( m_pCurrent ); }
		bool operator!=( const CMutableIterator& other ) const { return m_pList != other.m_pList || m_pCurrent != other.m_pCurrent; }

	private:
		CRenderDataList< T > *m_pList;
		T *m_pCurrent;
	};
	CMutableIterator begin() { return CMutableIterator( this, GetFirstMutable() ); }
	CMutableIterator end() { return CMutableIterator( this, nullptr ); }

private:
	// No copy constructor. Must use CopyFrom
	CRenderDataList( const CRenderDataList &other ) = delete;

	CRenderDataListBase m_listBase;

	template < typename U > friend class CRenderDataListBuilder;
};


//
// A utility class to add items to a CRenderDataList< T >. Internally caches a pointer to the final element of the
// list, so AddToTail will be O(1).
//
template < typename T >
class CRenderDataListBuilder
{
public:
	CRenderDataListBuilder( CRenderDataList< T > &list, CRenderCommandList *pCommandList )
		: m_baseBuilder( list.m_listBase, pCommandList )
	{
	}

	T* AddToTail()
	{
		return m_baseBuilder.AddToTailTyped< T >();
	}

	struct DefaultListNode_t
	{
		CRenderDataListBase::ListNode_t *pNext;
		T entry;
	};

	// If you call this directly, you are responsible for keeping the memory of the node alive
	// while the list exists. Almost always you should just call AddToTail instead.
	void AddNodeToTail( DefaultListNode_t *pNewNode )
	{
#ifndef PLATFORM_OSX
		static_assert( offsetof( DefaultListNode_t, entry ) == offsetof( CRenderDataListBase::ListNode_t, data ), "Default list node for CRenderDataList doesn't line up with the list node in the base class" );
#else
		static_assert( __builtin_offsetof( DefaultListNode_t, entry ) == __builtin_offsetof( CRenderDataListBase::ListNode_t, data ), "Default list node for CRenderDataList doesn't line up with the list node in the base class" );
#endif
		m_baseBuilder.AddNodeToTail( reinterpret_cast< CRenderDataListBase::ListNode_t * >( pNewNode ) );
	}

private:
	CRenderDataListBaseBuilder m_baseBuilder;
};


//
// A small wrapper class around an IUITexture that enforces correct texture ref counting
//
class CRenderCommandTexture
{
public:
	CRenderCommandTexture() : m_pTexture( nullptr ) {}

	void CopyFrom( const CRenderCommandTexture &from, CRenderCommandList &commandList )
	{
		m_pTexture = from.m_pTexture;
		commandList.AddTextureReference( m_pTexture );
	}

	void SetTexture( IUITexture *pTexture, CRenderCommandList &commandList );
	IUITexture *GetTexture() const { return m_pTexture; }

private:
	IUITexture *m_pTexture;
};


//
// A 3d point
//
struct RenderPoint_t
{
	void CopyFrom( const RenderPoint_t &from, CRenderCommandList &commandList )
	{
		*this = from;
	}

	float x;
	float y;
	float z;
};


//
// A 2d point
//
struct RenderPoint2D_t
{
	void CopyFrom( const RenderPoint2D_t &from, CRenderCommandList &commandList )
	{
		*this = from;
	}

	float x;
	float y;
};


// 
// A 4x4 matrix
//
struct RenderMatrix4x4_t
{
	void CopyFrom( const RenderMatrix4x4_t &from, CRenderCommandList &commandList )
	{
		*this = from;
	}

	// row 0
	float m00;
	float m01;
	float m02;
	float m03;

	// row 1
	float m10;
	float m11;
	float m12;
	float m13;

	// row 2
	float m20;
	float m21;
	float m22;
	float m23;

	// row 3
	float m30;
	float m31;
	float m32;
	float m33;
};


//
// Signals start of a frame
//
struct BeginFrameRenderCommand_t : public RenderCommand_t
{
	BeginFrameRenderCommand_t() : RenderCommand_t( k_EBeginFrame ) {}

	void PushCommandCopy( CRenderCommandList &commandList ) const
	{
		BeginFrameRenderCommand_t *pOther = commandList.PushCommand< BeginFrameRenderCommand_t >();
		
		pOther->CopyBaseCommandFrom( *this, commandList );

		pOther->frame_paint_time = frame_paint_time;
		pOther->surface_width = surface_width;
		pOther->surface_height = surface_height;
		pOther->render_target = render_target;
		pOther->ui_scale_factor = ui_scale_factor;
		pOther->empty_frame = empty_frame;
		pOther->clear_gpu_resources_before_frame = clear_gpu_resources_before_frame;
	}

	double frame_paint_time;
	uint32 surface_width;
	uint32 surface_height;
	uint32 render_target;
	double ui_scale_factor;
	bool empty_frame : 1;
	bool clear_gpu_resources_before_frame : 1;
};


//
// Signals end of a frame
//
struct EndFrameRenderCommand_t : public RenderCommand_t
{
	EndFrameRenderCommand_t() : RenderCommand_t( k_EEndFrame ) {}

	void PushCommandCopy( CRenderCommandList &commandList ) const
	{
		EndFrameRenderCommand_t *pOther = commandList.PushCommand< EndFrameRenderCommand_t >();

		pOther->CopyBaseCommandFrom( *this, commandList );
		
		pOther->mouse_cursor_texture.CopyFrom( mouse_cursor_texture, commandList );
		pOther->mouse_cursor_hotspot.CopyFrom( mouse_cursor_hotspot, commandList );
	}

	CRenderCommandTexture mouse_cursor_texture;
	RenderPoint2D_t mouse_cursor_hotspot;
};


//
// Clears back buffer 
//
struct ClearBackbufferRenderCommand_t : public RenderCommand_t
{
	ClearBackbufferRenderCommand_t() : RenderCommand_t( k_EClearBackBuffer ) {}

	void PushCommandCopy( CRenderCommandList &commandList ) const
	{
		ClearBackbufferRenderCommand_t *pOther = commandList.PushCommand< ClearBackbufferRenderCommand_t >();

		pOther->CopyBaseCommandFrom( *this, commandList );

		pOther->clear_color_rgba = clear_color_rgba;
	}

	uint32 clear_color_rgba;
};


//
// Deletes a panel (notifies one is deleted)
//
struct DeletePanelRenderCommand_t : public RenderCommand_t
{
	DeletePanelRenderCommand_t() : RenderCommand_t( k_EDeletePanel ) {}

	void PushCommandCopy( CRenderCommandList &commandList ) const
	{
		DeletePanelRenderCommand_t *pOther = commandList.PushCommand< DeletePanelRenderCommand_t >();

		pOther->CopyBaseCommandFrom( *this, commandList );

		pOther->context_id = context_id;
	}

	uint64 context_id;
};

//
// Deletes a particle system
//
struct DeleteParticleSystemRenderCommand_t : public RenderCommand_t
{
	DeleteParticleSystemRenderCommand_t() : RenderCommand_t( k_EDeleteParticleSystem ) {}

	void PushCommandCopy( CRenderCommandList &commandList ) const
	{
		DeleteParticleSystemRenderCommand_t *pOther = commandList.PushCommand< DeleteParticleSystemRenderCommand_t >();

		pOther->CopyBaseCommandFrom( *this, commandList );

		pOther->panel_handle = panel_handle;
		pOther->brush_index = brush_index;
	}

	uint64 panel_handle;
	uint32 brush_index;
};


//
// Radius data
//
struct RadiusData_t
{
	void CopyFrom( const RadiusData_t &from, CRenderCommandList &commandList )
	{
		*this = from;
	}

	struct CornerRadius_t
	{
		float horizontal;
		float vertical;
	};
	CornerRadius_t top_left;
	CornerRadius_t top_right;
	CornerRadius_t bottom_right;
	CornerRadius_t bottom_left;
};


//
// Border Data
//
struct BorderData_t
{
	void CopyFrom( const BorderData_t &from, CRenderCommandList &commandList )
	{
		*this = from;
	}

	struct BorderSide_t
	{
		uint32 style;
		float width;
		uint32 color;
	};
	BorderSide_t top;
	BorderSide_t right;
	BorderSide_t bottom;
	BorderSide_t left;
};

//
// Box Shadow Data
//
struct BoxShadowData_t
{
	void CopyFrom( const BoxShadowData_t &from, CRenderCommandList &commandList )
	{
		*this = from;
	}

	float horizontal_offset;
	float vertical_offset;
	float blur_radius;
	float spread_distance;
	uint32 color;
	bool fill : 1;
	bool inset : 1;
	bool animating : 1;
};

//
// Text Shadow Data
//
struct TextShadowData_t
{
	void CopyFrom( const TextShadowData_t &from, CRenderCommandList &commandList )
	{
		*this = from;
	}

	float horizontal_offset;
	float vertical_offset;
	float blur_radius;
	float strength;
	uint32 color;
	bool animating : 1;
};

//
// Image Shadow Data
//
struct ImageShadowData_t
{
	void CopyFrom( const ImageShadowData_t &from, CRenderCommandList &commandList )
	{
		*this = from;
	}

	float horizontal_offset;
	float vertical_offset;
	float blur_radius;
	float strength;
	uint32 color;
	bool animating : 1;
};

//
// CSS rect clip data
//
struct RectClipData_t
{
	void CopyFrom( const RectClipData_t &from, CRenderCommandList &commandList )
	{
		*this = from;
	}

	float left;
	float top;
	float right;
	float bottom;
};

//
// CSS radial clip data
//
struct RadialClipData_t
{
	void CopyFrom( const RadialClipData_t &from, CRenderCommandList &commandList )
	{
		*this = from;
	}

	float center_x;
	float center_y;
	float start_angle;
	float sector_angle;
};


//
// CSS clip data
//
struct ClipData_t
{
	void CopyFrom( const ClipData_t &from, CRenderCommandList &commandList )
	{
		CopyRenderDataPointer( rect_clip, from.rect_clip, commandList );
		CopyRenderDataPointer( radial_clip, from.radial_clip, commandList );
	}

	RectClipData_t *rect_clip;
	RadialClipData_t *radial_clip;
};


//
// Gaussian values
//
struct GaussianValues_t
{
	void CopyFrom( const GaussianValues_t &from, CRenderCommandList &commandList )
	{
		*this = from;
	}

	float passes;
	float stddev_hor;
	float stddev_ver;
	BlurType_t blurType;
};


//
// Push a compositing layer
//
struct PushCompositingLayerRenderCommand_t : public RenderCommand_t
{
	PushCompositingLayerRenderCommand_t() : RenderCommand_t( k_EPushCompositingLayer ) {}

	void PushCommandCopy( CRenderCommandList &commandList ) const
	{
		PushCompositingLayerRenderCommand_t *pOther = commandList.PushCommand< PushCompositingLayerRenderCommand_t >();

		pOther->CopyBaseCommandFrom( *this, commandList );

		pOther->layer_id = layer_id;
		pOther->width = width;
		pOther->height = height;

		pOther->layer_quad_top_left.CopyFrom( layer_quad_top_left, commandList );
		pOther->layer_quad_top_right.CopyFrom( layer_quad_top_right, commandList );
		pOther->layer_quad_bottom_left.CopyFrom( layer_quad_bottom_left, commandList );
		pOther->layer_quad_bottom_right.CopyFrom( layer_quad_bottom_right, commandList );

		pOther->transform.CopyFrom( transform, commandList );

		pOther->perspective_depth = perspective_depth;

		pOther->opacity = opacity;
		pOther->composition_color = composition_color;

		pOther->saturation = saturation;
		pOther->hue_shift = hue_shift;
		pOther->brightness = brightness;
		pOther->contrast = contrast;

		pOther->opacity_mask_texture.CopyFrom( opacity_mask_texture, commandList );
		pOther->opacity_mask_opacity = opacity_mask_opacity;

		CopyRenderDataPointer( pOther->border, border, commandList );
		CopyRenderDataPointer( pOther->border_radius, border_radius, commandList );
		CopyRenderDataPointer( pOther->box_shadow, box_shadow, commandList );
		CopyRenderDataPointer( pOther->radial_clip, radial_clip, commandList );
		CopyRenderDataPointer( pOther->text_shadow, text_shadow, commandList );

		pOther->gaussian_blur.CopyFrom( gaussian_blur, commandList );

		pOther->scale2d_factors.CopyFrom( scale2d_factors, commandList );

		pOther->rotate_2d = rotate_2d;

		pOther->occluded_left_edge = occluded_left_edge;
		pOther->occluded_top_edge = occluded_top_edge;
		pOther->occluded_right_edge = occluded_right_edge;
		pOther->occluded_bottom_edge = occluded_bottom_edge;

		pOther->composition_layer_texture_name = commandList.CopyString( composition_layer_texture_name );

		pOther->mix_blend_mode = mix_blend_mode;

		pOther->needs_clear = needs_clear;
		pOther->needs_depth = needs_depth;
		pOther->needs_intermediate_texture = needs_intermediate_texture;
		pOther->needs_redraw_every_frame = needs_redraw_every_frame;
		pOther->always_cache_composition_layer = always_cache_composition_layer;
		pOther->offscreen_composition_layer = offscreen_composition_layer;
		pOther->fractional_pixel_positions = fractional_pixel_positions;
	}

	uint64 layer_id;
	float width;
	float height;

	RenderPoint_t layer_quad_top_left;
	RenderPoint_t layer_quad_top_right;
	RenderPoint_t layer_quad_bottom_left;
	RenderPoint_t layer_quad_bottom_right;

	RenderMatrix4x4_t transform;

	float perspective_depth;

	float opacity;
	uint32 composition_color;

	float saturation;
	float hue_shift;
	float brightness;
	float contrast;

	CRenderCommandTexture opacity_mask_texture;
	float opacity_mask_opacity;

	BorderData_t *border;
	RadiusData_t *border_radius;
	BoxShadowData_t *box_shadow;
	RadialClipData_t *radial_clip;
	TextShadowData_t *text_shadow;

	GaussianValues_t gaussian_blur; // todo(ericl): make pointer?

	RenderPoint2D_t scale2d_factors;

	float rotate_2d;

	float occluded_left_edge;
	float occluded_top_edge;
	float occluded_right_edge;
	float occluded_bottom_edge;

	const char *composition_layer_texture_name;

	EMixBlendMode mix_blend_mode;
	EFractionalPixelPositions fractional_pixel_positions;

	bool needs_clear : 1;
	bool needs_depth : 1;
	bool needs_intermediate_texture : 1;
	bool needs_redraw_every_frame : 1;
	bool always_cache_composition_layer : 1;
	bool offscreen_composition_layer : 1;
};


//
// Pop a compositing layer
//
struct PopCompositingLayerRenderCommand_t : public RenderCommand_t
{
	PopCompositingLayerRenderCommand_t() : RenderCommand_t( k_EPopCompositingLayer ) {}

	void PushCommandCopy( CRenderCommandList &commandList ) const
	{
		PopCompositingLayerRenderCommand_t *pOther = commandList.PushCommand< PopCompositingLayerRenderCommand_t >();

		pOther->CopyBaseCommandFrom( *this, commandList );
	}
};


//
// Free a compositing layer
//
struct FreeCompositingLayerRenderCommand_t : public RenderCommand_t
{
	FreeCompositingLayerRenderCommand_t() : RenderCommand_t( k_ECmdFreeCompositingLayer ) {}

	void PushCommandCopy( CRenderCommandList &commandList ) const
	{
		FreeCompositingLayerRenderCommand_t *pOther = commandList.PushCommand< FreeCompositingLayerRenderCommand_t >();

		pOther->CopyBaseCommandFrom( *this, commandList );

		pOther->layer_id = layer_id;
	}

	uint64 layer_id;
};


//
// Generic transition data
//
struct TransitionData_t
{
	TransitionData_t()
		: timing_func( k_EAnimationUnset )
	{}

	void CopyFrom( const TransitionData_t &from, CRenderCommandList &commandList )
	{
		*this = from;
	}

	float start_time;							// when the transition started
	float delay_seconds;						// delay after start time before starting transition
	float duration_seconds;					// duration of transition after delay
	float cubic_bezier_0;
	float cubic_bezier_1;
	float cubic_bezier_2;
	float cubic_bezier_3;
	EAnimationTimingFunction timing_func;
};


//
// Generic animation frame data
// 
struct BaseAnimationFrameData_t
{
	BaseAnimationFrameData_t()
		: timing_func( k_EAnimationUnset )
	{}

	void CopyFrom( const BaseAnimationFrameData_t &from, CRenderCommandList &commandList )
	{
		*this = from;
	}

	float percent;							// percent into animation when this frame should start
	float cubic_bezier_0;
	float cubic_bezier_1;
	float cubic_bezier_2;
	float cubic_bezier_3;
	EAnimationTimingFunction timing_func;
};


//
// Generic animation data
//
struct BaseAnimationData_t
{
	BaseAnimationData_t()
		: timing_func( k_EAnimationUnset )
	{}

	void CopyFrom( const BaseAnimationData_t &from, CRenderCommandList &commandList )
	{
		*this = from;
	}

	float start_time;							// when the animation started
	float delay_seconds;						// delay after start time before starting animation
	float duration_seconds;					// duration of animation after delay
	float cubic_bezier_0;
	float cubic_bezier_1;
	float cubic_bezier_2;
	float cubic_bezier_3;
	uint32 direction;							// EAnimationDirection
	uint32 fillMode;							// EAnimationFillMode
	float iteration;							// could be k_flFloatInfiniteIteration
	EAnimationTimingFunction timing_func;
};


//
// Base class for a property that could potentially animate or transition.
//
template < typename T >
struct PropertyWithTransition_t
{
	void CopyFrom( const PropertyWithTransition_t< T > &from, CRenderCommandList &commandList )
	{
		CopyRenderData( base, from.base, commandList );
		CopyRenderData( transition, from.transition, commandList );

		CopyRenderDataPointer( transition_data, from.transition_data, commandList );

		style_symbol = from.style_symbol;

		animations.CopyFrom( from.animations, commandList );
	}

	void Swap( PropertyWithTransition_t< T > &other )
	{
		SwapRenderData( base, other.base );
		SwapRenderData( transition, other.transition );
		std::swap( transition_data, other.transition_data );
		std::swap( style_symbol, other.style_symbol );
		animations.Swap( other.animations );
	}

	T base;
	T transition;
	TransitionData_t *transition_data;
	uint32 style_symbol;

	// A property can have multiple animations, each of which can have multiple frames.
	// The data in those frames is dependent on the property type, which is why these
	// internal structs exist. For example, you'd refer to the position property's animations as
	// PositionWithTransition_t::AnimationFrameData_t.
	struct AnimationFrameData_t : public BaseAnimationFrameData_t
	{
		T data;
	};
	struct AnimationData_t : public BaseAnimationData_t
	{
		CRenderDataList< AnimationFrameData_t > frames;
	};
	CRenderDataList< AnimationData_t > animations;
};


//
// Point with a transition
//
struct PointWithTransition_t : public PropertyWithTransition_t < RenderPoint_t > {};


//
// Color data with transition
//
struct ColorWithTransition_t : public PropertyWithTransition_t < uint32 > {};


//
// Gradient color stop
//
struct ColorStop_t
{
	void CopyFrom( const ColorStop_t &from, CRenderCommandList &commandList )
	{
		*this = from;
	}

	float position;
	uint32 color_rgba;
};


//
// Linear gradient data
//
struct LinearGradient_t
{
	void CopyFrom( const LinearGradient_t &from, CRenderCommandList &commandList )
	{
		start_position.CopyFrom( from.start_position, commandList );
		end_position.CopyFrom( from.end_position, commandList );

		color_stop.CopyFrom( from.color_stop, commandList );
	}

	// NOTE: Update CLinearGradientLessThan in d3d10d2dsurface.cpp if you change/add to this in anyway!

	RenderPoint2D_t start_position;
	RenderPoint2D_t end_position;

	CRenderDataList< ColorStop_t > color_stop;
};


//
// Radial gradient data
//
struct RadialGradient_t
{
	void CopyFrom( const RadialGradient_t &from, CRenderCommandList &commandList )
	{
		center_position.CopyFrom( from.center_position, commandList );
		offset_distance.CopyFrom( from.offset_distance, commandList );
		radii.CopyFrom( from.radii, commandList );

		color_stop.CopyFrom( from.color_stop, commandList );
	}

	RenderPoint2D_t center_position;
	RenderPoint2D_t offset_distance;
	RenderPoint2D_t radii;

	CRenderDataList< ColorStop_t > color_stop;
};

// 
// Particle inside a particle system
//
struct Particle_t
{
	void CopyFrom( const Particle_t &from, CRenderCommandList &commandList )
	{
		particle_position.CopyFrom( from.particle_position, commandList );
		particle_size = from.particle_size;
		particle_sharpness = from.particle_sharpness;
		color_rgba = from.color_rgba;
	}

	RenderPoint_t particle_position;
	float particle_size;
	float particle_sharpness;
	uint32 color_rgba;
};


//
// Particle system data
//
struct ParticleSystem_t
{
	void CopyFrom( const ParticleSystem_t &from, CRenderCommandList &commandList )
	{
		base_position.CopyFrom( from.base_position, commandList );
		base_position_variance.CopyFrom( from.base_position_variance, commandList );

		particle_size = from.particle_size;
		particle_size_variance = from.particle_size_variance;

		particles_per_second = from.particles_per_second;
		particles_per_second_variance = from.particles_per_second_variance;

		particle_lifespan_seconds = from.particle_lifespan_seconds;
		particle_lifespan_seconds_variance = from.particle_lifespan_seconds_variance;

		particle_initial_velocity.CopyFrom( from.particle_initial_velocity, commandList );
		particle_initial_velocity_variance.CopyFrom( from.particle_initial_velocity_variance, commandList );

		gravity_acceleration.CopyFrom( from.gravity_acceleration, commandList );
		gravity_acceleration_particle_variance.CopyFrom( from.gravity_acceleration_particle_variance, commandList );

		color_start_rgba = from.color_start_rgba;
		color_start_rgba_variance = from.color_start_rgba_variance;

		color_end_rgba = from.color_end_rgba;
		color_end_rgba_variance = from.color_end_rgba_variance;

		parent_panel_handle = from.parent_panel_handle;
		parent_brush_index = from.parent_brush_index;

		particle_sharpness = from.particle_sharpness;
		particle_sharpness_variance = from.particle_sharpness_variance;

		particle_flicker = from.particle_flicker;
		particle_flicker_variance = from.particle_flicker_variance;

		particle_velocity_min.CopyFrom( from.particle_velocity_min, commandList );
		particle_velocity_max.CopyFrom( from.particle_velocity_max, commandList );

		particles.CopyFrom( from.particles, commandList );
	}

	RenderPoint_t base_position;
	RenderPoint_t base_position_variance;

	float particle_size;
	float particle_size_variance;

	float particles_per_second;
	float particles_per_second_variance;

	float particle_lifespan_seconds;
	float particle_lifespan_seconds_variance;

	RenderPoint_t particle_initial_velocity;
	RenderPoint_t particle_initial_velocity_variance;

	RenderPoint_t gravity_acceleration;
	RenderPoint_t gravity_acceleration_particle_variance;

	uint32 color_start_rgba;
	uint32 color_start_rgba_variance;

	uint32 color_end_rgba;
	uint32 color_end_rgba_variance;

	uint64 parent_panel_handle;
	uint32 parent_brush_index;

	float particle_sharpness;
	float particle_sharpness_variance;

	float particle_flicker;
	float particle_flicker_variance;

	RenderPoint_t particle_velocity_min;
	RenderPoint_t particle_velocity_max;

	// Only filled in after the animation thread and used only by rendering
	CRenderDataList< Particle_t > particles;
};


enum EFillBrushType : uint8
{
	k_EFillBrushType_None,
	k_EFillBrushType_Color,
	k_EFillBrushType_LinearGradient,
	k_EFillBrushType_RadialGradient,
	k_EFillBrushType_ParticleSystem,
};

//
// Fill brush data
//
struct FillBrush_t
{
	void CopyFrom( const FillBrush_t &from, CRenderCommandList &commandList )
	{
		opacity = from.opacity;

		eFillBrushType = from.eFillBrushType;

		switch ( from.eFillBrushType )
		{
			case k_EFillBrushType_Color:
				color_rgba = from.color_rgba;
				break;

			case k_EFillBrushType_LinearGradient:
				CopyRenderDataPointer( linear_gradient, from.linear_gradient, commandList );
				break;

			case k_EFillBrushType_RadialGradient:
				CopyRenderDataPointer( radial_gradient, from.radial_gradient, commandList );
				break;

			case k_EFillBrushType_ParticleSystem:
				CopyRenderDataPointer( particle_system, from.particle_system, commandList );
				break;
		}
	}

	float opacity;
	union
	{
		uint32 color_rgba;
		LinearGradient_t *linear_gradient;
		RadialGradient_t *radial_gradient;
		ParticleSystem_t *particle_system;
	};
	EFillBrushType eFillBrushType;
};


//
// Fill brush data
//
struct FillBrushCollection_t
{
	void CopyFrom( const FillBrushCollection_t &from, CRenderCommandList &commandList )
	{
		fill_brush.CopyFrom( from.fill_brush, commandList );
	}

	void Swap( FillBrushCollection_t &other )
	{
		fill_brush.Swap( other.fill_brush );
	}

	CRenderDataList< FillBrush_t > fill_brush;
};


//
// Fill brush data with transition
//
struct FillBrushCollectionWithTransition_t : public PropertyWithTransition_t < FillBrushCollection_t >
{
	void AddSolidColorFillBrushNoTransition( uint32 unColor, CRenderCommandList &commandList )
	{
		CRenderDataListBuilder< FillBrush_t > listBuilder( base.fill_brush, &commandList );
		FillBrush_t *pFillBrush = listBuilder.AddToTail();
		pFillBrush->eFillBrushType = k_EFillBrushType_Color;
		pFillBrush->color_rgba = unColor;
		pFillBrush->opacity = 1.0f;
	}

};


// 
// Position data
//
struct PanelPositionWithTransition_t : public PropertyWithTransition_t < RenderPoint_t >
{
	void CopyFrom( const PanelPositionWithTransition_t &from, CRenderCommandList &commandList )
	{
		PropertyWithTransition_t < RenderPoint_t >::CopyFrom( from, commandList );

		CopyRenderDataPointer( scroll_offset, from.scroll_offset, commandList );
		CopyRenderDataPointer( scroll_offset_target, from.scroll_offset_target, commandList );
		CopyRenderDataPointer( scroll_transition_x, from.scroll_transition_x, commandList );
		CopyRenderDataPointer( scroll_transition_y, from.scroll_transition_y, commandList );
	}

	// scroll data, does animate
	RenderPoint2D_t *scroll_offset;
	RenderPoint2D_t *scroll_offset_target;
	TransitionData_t *scroll_transition_x;
	TransitionData_t *scroll_transition_y;
};


//
// Opacity data
//
struct OpacityWithTransition_t : public PropertyWithTransition_t < float > {};


//
// BkImgOpacity
// 
struct BackgroundImgOpacityWithTransition_t : public PropertyWithTransition_t < float > {};


//
// Scale2D & Rotate2D data
//
struct Scale2DWithTransition_t : public PropertyWithTransition_t< RenderPoint2D_t > {};
struct Rotate2DWithTransition_t : public PropertyWithTransition_t < float > {};


//
// WashColor data
//
struct WashColor_t
{
	uint32 m_color;
	bool m_bFast;
};
struct WashColorWithTransition_t : public PropertyWithTransition_t < WashColor_t > {};

//
// Opacity mask
//
struct OpacityMask_t
{
	void CopyFrom( const OpacityMask_t &from, CRenderCommandList &commandList )
	{
		opacity_mask_texture.CopyFrom( from.opacity_mask_texture, commandList );
		opacity_mask_opacity = from.opacity_mask_opacity;
	}

	CRenderCommandTexture opacity_mask_texture;
	float opacity_mask_opacity;
};
struct OpacityMaskWithTransition_t : public PropertyWithTransition_t < OpacityMask_t > {};


//
// Hue, Saturation, Brightness, and Contrast data
//
struct HueShiftWithTransition_t : public PropertyWithTransition_t < float > {};
struct SaturationWithTransition_t : public PropertyWithTransition_t < float > {};
struct BrightnessWithTransition_t : public PropertyWithTransition_t < float > {};
struct ContrastWithTransition_t : public PropertyWithTransition_t < float > {};


//
// Gaussian blur
//
struct GaussianBlurWithTransition_t : public PropertyWithTransition_t < GaussianValues_t > {};


//
// Perspective data
//
struct TransformPerspectiveWithTransition_t : public PropertyWithTransition_t < float > {};

//
// Perspective origin data
//
struct TransformPerspectiveOriginWithTransition_t : public PropertyWithTransition_t < RenderPoint_t > {};


//
// Transform origin data
//
struct TransformOriginData_t
{
	void CopyFrom( const TransformOriginData_t &from, CRenderCommandList &commandList )
	{
		*this = from;
	}

	float x;
	float y;
	bool x_is_percent : 1;
	bool y_is_percent : 1;
	bool is_parent_relative : 1;
};
struct TransformOriginWithTransition_t : public PropertyWithTransition_t < TransformOriginData_t > {};

//
// Transform Matrix
//
struct TransformMatrixWithTransition_t : public PropertyWithTransition_t < RenderMatrix4x4_t > {};

//
// Borders
//
struct BorderRadiusWithTransition_t : public PropertyWithTransition_t < RadiusData_t > {};
struct BorderWithTransition_t : public PropertyWithTransition_t < BorderData_t > {};

//
// Box shadow message
//
struct BoxShadowWithTransition_t : public PropertyWithTransition_t < BoxShadowData_t > {};

//
// Text shadow message
//
struct TextShadowWithTransition_t : public PropertyWithTransition_t < TextShadowData_t > {};

//
// Image shadow message
//
struct ImageShadowWithTransition_t : public PropertyWithTransition_t < ImageShadowData_t > {};

//
// Clip message
// 
struct ClipWithTransition_t : public PropertyWithTransition_t < ClipData_t > {};

//
// Push clip layer
//
struct PushClipLayerRenderCommand_t : public RenderCommand_t
{
	PushClipLayerRenderCommand_t() : RenderCommand_t( k_EPushClipLayer ) {}

	void PushCommandCopy( CRenderCommandList &commandList ) const
	{
		PushClipLayerRenderCommand_t *pOther = commandList.PushCommand< PushClipLayerRenderCommand_t >();
		pOther->CopyBaseCommandFrom( *this, commandList );

		pOther->top_left.CopyFrom( top_left, commandList );
		pOther->bottom_right.CopyFrom( bottom_right, commandList );
		pOther->border_radius.CopyFrom( border_radius, commandList );

		pOther->context_id = context_id;
	}

	uint64			context_id;

	RenderPoint2D_t top_left;
	RenderPoint2D_t bottom_right;

	RadiusData_t border_radius;
};

//
// Pop clip layer
//
struct PopClipLayerRenderCommand_t : public RenderCommand_t
{
	PopClipLayerRenderCommand_t() : RenderCommand_t( k_EPopClipLayer ) {}

	void PushCommandCopy( CRenderCommandList &commandList ) const
	{
		PopClipLayerRenderCommand_t *pOther = commandList.PushCommand< PopClipLayerRenderCommand_t >();
		pOther->CopyBaseCommandFrom( *this, commandList );
	}

};


//
// Push individual panel context inside composition layer
//
struct PushPanelContextInLayerRenderCommand_t : public RenderCommand_t
{
	PushPanelContextInLayerRenderCommand_t() : RenderCommand_t( k_EPushPanelContextInLayer ) {}

	void PushCommandCopy( CRenderCommandList &commandList ) const
	{
		PushPanelContextInLayerRenderCommand_t *pOther = commandList.PushCommand< PushPanelContextInLayerRenderCommand_t >();
		pOther->CopyBaseCommandFrom( *this, commandList );

		CopyRenderDataPointer( pOther->transform, transform, commandList );

		CopyRenderDataPointer( pOther->box_shadow, box_shadow, commandList );

		pOther->width = width;
		pOther->height = height;

		pOther->context_id = context_id;

		pOther->position.CopyFrom( position, commandList );

		CopyRenderDataPointer( pOther->border, border, commandList );

		pOther->scroll_offset.CopyFrom( scroll_offset, commandList );

		pOther->composition_color = composition_color;

		pOther->mix_blend_mode = mix_blend_mode;

		pOther->fractional_pixel_positions = fractional_pixel_positions;
	}

	// Transform matrix 
	RenderMatrix4x4_t *transform;

	// Box shadow
	BoxShadowData_t *box_shadow;

	// id of panel
	uint64 context_id;

	// Size of panel
	float width;
	float height;

	// Position of panel within layer
	RenderPoint_t position;

	// border data
	BorderData_t *border;

	// Scroll offsets of panel
	RenderPoint2D_t scroll_offset;

	// Wash color - fast version which doesn't require a composition layer
	uint32 composition_color;

	EMixBlendMode mix_blend_mode;

	EFractionalPixelPositions fractional_pixel_positions;
};


//
// Pop individual panel context inside composition layer
//
struct PopPanelContextInLayerRenderCommand_t : public RenderCommand_t
{
	PopPanelContextInLayerRenderCommand_t() : RenderCommand_t( k_EPopPanelContextInLayer ) {}

	void PushCommandCopy( CRenderCommandList &commandList ) const
	{
		PopPanelContextInLayerRenderCommand_t *pOther = commandList.PushCommand< PopPanelContextInLayerRenderCommand_t >();
		pOther->CopyBaseCommandFrom( *this, commandList );
	}
};

enum EStyleOptionalProperty : uint8
{
	k_EStyleOptionalProperty_TransformMatrix,
	k_EStyleOptionalProperty_TransformOrigin,
	k_EStyleOptionalProperty_TransformPerspective,
	k_EStyleOptionalProperty_TransformPerspectiveOrigin,
	k_EStyleOptionalProperty_Opacity,
	k_EStyleOptionalProperty_WashColor,
	k_EStyleOptionalProperty_HueShift,
	k_EStyleOptionalProperty_Saturation,
	k_EStyleOptionalProperty_Brightness,
	k_EStyleOptionalProperty_Contrast,
	k_EStyleOptionalProperty_OpacityMask,
	k_EStyleOptionalProperty_BorderRadius,
	k_EStyleOptionalProperty_GaussianBlur,
	k_EStyleOptionalProperty_Border,
	k_EStyleOptionalProperty_BoxShadow,
	k_EStyleOptionalProperty_Scale2D,
	k_EStyleOptionalProperty_Rotate2D,
	k_EStyleOptionalProperty_TextShadow,
	k_EStyleOptionalProperty_ImageShadow,
	k_EStyleOptionalProperty_Clip,
};

template < typename T >
struct OptionalProperty_t
{
	EStyleOptionalProperty property_type;
	T property_data;
};

//
// Push animation and transform context for a panel
//
struct PushAAndTContextRenderCommand_t : public RenderCommand_t
{
	PushAAndTContextRenderCommand_t() : RenderCommand_t( k_EPushAnimationAndTransformContext ) {}

	uint64 context_id;
	float width;
	float height;
	float zindex;

	PanelPositionWithTransition_t panel_position;

	CRenderDataListBase optional_properties;

	const char *composition_layer_texture_name;

	// Indicates whether the layer should re-use an existing composition layer
	// and just composite or whether it intends to re-draw all of it's children 
	// completely.
	EPanelRepaint needs_full_repaint;

	EMixBlendMode mix_blend_mode;
	// Giving content creator control whether to clamp or not to pixel boundaries
	EFractionalPixelPositions fractional_pixel_positions;

	bool has_children : 1;
	bool children_have_3dtransforms : 1;

	// Flag to supress pushing clipping for this context if there is no explicit clip
	bool suppress_clip_to_bounds : 1;

	// Indicates that the rendering pipeline used for this layer requires an intermediate
	// texture be created on the surface
	bool needs_intermediate_texture : 1;

	// Indicates whether we should do clipping after applying the transform rather than before.
	bool clip_after_transform : 1;

	// Does this panel want to get hit test results, or should mouse clicks go through it.   
	// Children are still hit tested, thus we'll really test all panels, just won't return it as
	// the current hover panel ever.
	bool wants_hit_test : 1;

	// Should this panel recurse into children for hit testing?
	bool wants_hit_test_children : 1;

	bool opaque_background : 1;

	// Do we need to output the screenspace quad (non axis-aligned) for this panel from animation thread?
	bool wants_screenspace_quad_output : 1;

	// Content creator wants this panel to always use a composition layer
	bool require_composition_layer : 1;

	// Content creator wants this panel to never use a composition layer
	bool force_no_composition_layer : 1;

	// Content hint that this panel's composition layer is important to cache
	bool always_cache_composition_layer : 1;

	// Content hint that this panel's composition layer will be rendered to an offscreen render target
	bool offscreen_composition_layer : 1;
};

//
// Pop animation and transform context for a panel
//
struct PopAAndTContextRenderCommand_t : public RenderCommand_t
{
	PopAAndTContextRenderCommand_t() : RenderCommand_t( k_EPopAnimationAndTransformContext ) {}

	void PushCommandCopy( CRenderCommandList &commandList ) const
	{
		PopAAndTContextRenderCommand_t *pOther = commandList.PushCommand< PopAAndTContextRenderCommand_t >();
		pOther->CopyBaseCommandFrom( *this, commandList );

		pOther->context_id = context_id;
	}

	uint64 context_id;
};


//
// Indicate start of painting background for panel, will render/sort draw ops in parents context
//
struct BeginPaintBackgroundRenderCommand_t : public RenderCommand_t
{
	BeginPaintBackgroundRenderCommand_t() : RenderCommand_t( k_EBeginPaintBackground ) {}

	void PushCommandCopy( CRenderCommandList &commandList ) const
	{
		BeginPaintBackgroundRenderCommand_t *pOther = commandList.PushCommand< BeginPaintBackgroundRenderCommand_t >();
		pOther->CopyBaseCommandFrom( *this, commandList );
	}
};


//
// Indicate end of painting background for panel, will return to rendering/sorting draw ops in own context
//
struct EndPaintBackgroundRenderCommand_t : public RenderCommand_t
{
	EndPaintBackgroundRenderCommand_t() : RenderCommand_t( k_EEndPaintBackground ) {}

	void PushCommandCopy( CRenderCommandList &commandList ) const
	{
		EndPaintBackgroundRenderCommand_t *pOther = commandList.PushCommand< EndPaintBackgroundRenderCommand_t >();
		pOther->CopyBaseCommandFrom( *this, commandList );
	}
};


//
// Indicate start of painting contents for current context which should be drawn last after normal children
//
struct BeginPaintLastRenderCommand_t : public RenderCommand_t
{
	BeginPaintLastRenderCommand_t() : RenderCommand_t( k_EBeginPaintLast ) {}

	void PushCommandCopy( CRenderCommandList &commandList ) const
	{
		BeginPaintLastRenderCommand_t *pOther = commandList.PushCommand< BeginPaintLastRenderCommand_t >();
		pOther->CopyBaseCommandFrom( *this, commandList );
	}
};


//
// Indicate end of  painting contents for current context which should be drawn last after normal children
//
struct EndPaintLastRenderCommand_t : public RenderCommand_t
{
	EndPaintLastRenderCommand_t() : RenderCommand_t( k_EEndPaintLast ) {}

	void PushCommandCopy( CRenderCommandList &commandList ) const
	{
		EndPaintLastRenderCommand_t *pOther = commandList.PushCommand< EndPaintLastRenderCommand_t >();
		pOther->CopyBaseCommandFrom( *this, commandList );
	}
};


//
// Draw a filled rect, paint command, into animation
//
struct DrawFilledRectRenderCommand_t : public RenderCommand_t
{
	DrawFilledRectRenderCommand_t() : RenderCommand_t( k_EDrawFilledRect ) {}

	void PushCommandCopy( CRenderCommandList &commandList ) const
	{
		DrawFilledRectRenderCommand_t *pOther = commandList.PushCommand< DrawFilledRectRenderCommand_t >();
		pOther->CopyBaseCommandFrom( *this, commandList );

		pOther->top_left.CopyFrom( top_left, commandList );
		pOther->bottom_right.CopyFrom( bottom_right, commandList );
		pOther->fill_brush_collection.CopyFrom( fill_brush_collection, commandList );
		pOther->antialiasing = antialiasing;
		pOther->context_id = context_id;
	}

	uint64 context_id;
	RenderPoint_t top_left;
	RenderPoint_t bottom_right;
	FillBrushCollectionWithTransition_t fill_brush_collection;
	EAntialiasing antialiasing;
};


//
// Draw a filled rect, render command, comes out of animation
//
struct RenderFilledRectRenderCommand_t : public RenderCommand_t
{
	RenderFilledRectRenderCommand_t() : RenderCommand_t( k_EDrawFilledRect ) {}

	void PushCommandCopy( CRenderCommandList &commandList ) const
	{
		RenderFilledRectRenderCommand_t *pOther = commandList.PushCommand< RenderFilledRectRenderCommand_t >();
		pOther->CopyBaseCommandFrom( *this, commandList );

		pOther->top_left.CopyFrom( top_left, commandList );
		pOther->bottom_right.CopyFrom( bottom_right, commandList );
		pOther->fill_brush_collection.CopyFrom( fill_brush_collection, commandList );
		pOther->antialiasing = antialiasing;
		pOther->context_id = context_id;

	}

	uint64 context_id;
	RenderPoint_t top_left;
	RenderPoint_t bottom_right;
	FillBrushCollection_t fill_brush_collection;
	EAntialiasing antialiasing;
};


//
// Draw a textured rect, from layout to animation thread
//
struct DrawTexturedRectRenderCommand_t : public RenderCommand_t
{
	DrawTexturedRectRenderCommand_t() : RenderCommand_t( k_ECmdDrawTexturedRect ) {}

	void PushCommandCopy( CRenderCommandList &commandList ) const
	{
		DrawTexturedRectRenderCommand_t *pOther = commandList.PushCommand< DrawTexturedRectRenderCommand_t >();
		pOther->CopyBaseCommandFrom( *this, commandList );

		pOther->top_left.CopyFrom( top_left, commandList );
		pOther->bottom_right.CopyFrom( bottom_right, commandList );
		
		pOther->texture.CopyFrom( texture, commandList );
		pOther->texture_top_left.CopyFrom( texture_top_left, commandList );
		pOther->texture_bottom_right.CopyFrom( texture_bottom_right, commandList );

		pOther->texture_serial = texture_serial;
		pOther->texture_sample_mode = texture_sample_mode;

		CopyRenderDataPointer( pOther->texture_opacity, texture_opacity, commandList );
	}

	RenderPoint2D_t top_left;
	RenderPoint2D_t bottom_right;

	CRenderCommandTexture texture;
	RenderPoint2D_t texture_top_left;
	RenderPoint2D_t texture_bottom_right;

	int32 texture_serial;
	ETextureSampleMode texture_sample_mode;

	OpacityWithTransition_t *texture_opacity;
};


// Currently commented out because they're not being used
////
//// Draw a textured rect, from animation thread to rendering, post interpolation
////
//struct DrawDoubleBufferedRectRenderCommand_t : public RenderCommand_t
//{
//	DrawDoubleBufferedRectRenderCommand_t() : RenderCommand_t( k_ECmdDrawTexturedRect ) {}
//
//	RenderPoint2D_t top_left;
//	RenderPoint2D_t bottom_right;
//
//	CRenderCommandTexture texture;
//	RenderPoint2D_t texture_top_left;
//	RenderPoint2D_t texture_bottom_right;
//};
//
//
////
//// Draw a textured rect, from animation thread to rendering, post interpolation
////
//struct DrawYUV420DoubleBufferedRectRenderCommand_t : public RenderCommand_t
//{
//	DrawYUV420DoubleBufferedRectRenderCommand_t() : RenderCommand_t( k_ECmdDrawTexturedRect ) {}
//
//	RenderPoint2D_t top_left;
//	RenderPoint2D_t bottom_right;
//
//	CRenderCommandTexture texture;
//	RenderPoint2D_t texture_top_left;
//	RenderPoint2D_t texture_bottom_right;
//};


//
// Draw a textured rect, from animation thread to rendering, post interpolation
//
struct RenderTexturedRectRenderCommand_t : public RenderCommand_t
{
	RenderTexturedRectRenderCommand_t() 
		: RenderCommand_t( k_ECmdDrawTexturedRect )
		, texture_opacity( 1.0 )
	{}

	void PushCommandCopy( CRenderCommandList &commandList ) const
	{
		RenderTexturedRectRenderCommand_t *pOther = commandList.PushCommand< RenderTexturedRectRenderCommand_t >();
		pOther->CopyBaseCommandFrom( *this, commandList );

		pOther->top_left.CopyFrom( top_left, commandList );
		pOther->bottom_right.CopyFrom( bottom_right, commandList );

		pOther->texture.CopyFrom( texture, commandList );
		pOther->texture_top_left.CopyFrom( texture_top_left, commandList );
		pOther->texture_bottom_right.CopyFrom( texture_bottom_right, commandList );

		pOther->texture_serial = texture_serial;
		pOther->texture_sample_mode = texture_sample_mode;
		pOther->texture_opacity = texture_opacity;
		
		CopyRenderDataPointer( pOther->img_shadow, img_shadow, commandList );
	}

	RenderPoint2D_t top_left;
	RenderPoint2D_t bottom_right;

	CRenderCommandTexture texture;
	RenderPoint2D_t texture_top_left;
	RenderPoint2D_t texture_bottom_right;

	int32 texture_serial;
	ETextureSampleMode texture_sample_mode;
	float texture_opacity;
	
	ImageShadowData_t *img_shadow;

};


//
// Just lock a texture, incrementing it's draw serial
//
struct LockTextureRenderCommand_t : public RenderCommand_t
{
	LockTextureRenderCommand_t() : RenderCommand_t( k_ECmdLockTexture ) {}

	void PushCommandCopy( CRenderCommandList &commandList ) const
	{
		RenderTexturedRectRenderCommand_t *pOther = commandList.PushCommand< RenderTexturedRectRenderCommand_t >();
		pOther->CopyBaseCommandFrom( *this, commandList );

		pOther->texture.CopyFrom( texture, commandList );
		pOther->texture_serial = texture_serial;
	}

	CRenderCommandTexture texture;
	int32 texture_serial;
};


//
// Parameters describing an object placed inline within text
// 
struct TextInlineObject_t
{
	void CopyFrom( const TextInlineObject_t &from, CRenderCommandList &commandList )
	{
		*this = from;
	}

	float width;
	float height;
};


//
// Represents text format data that can be applied to any section of text
//
struct TextFormat_t
{
	TextFormat_t()
		: font_weight( k_EFontWeightUnset )
		, font_style( k_EFontStyleUnset )
		, text_decoration( k_ETextDecorationUnset )
	{}

	void CopyFrom( const TextFormat_t &from, CRenderCommandList &commandList )
	{
		font_name = commandList.CopyString( from.font_name );
		font_size = from.font_size;
		fill_brush_collection.CopyFrom( from.fill_brush_collection, commandList );
		letter_spacing = from.letter_spacing;
		CopyRenderDataPointer( inline_object, from.inline_object, commandList );
		font_weight = from.font_weight;
		font_style = from.font_style;
		text_decoration = from.text_decoration;
	}

	const char *font_name;
	float font_size;
	FillBrushCollectionWithTransition_t fill_brush_collection;
	int32 letter_spacing;
	TextInlineObject_t *inline_object;
	EFontWeight font_weight;
	EFontStyle font_style;
	ETextDecoration text_decoration;
};


//
// Specifies a custom text format for a region of text
//
struct TextRangeFormatData_t
{
	void CopyFrom( const TextRangeFormatData_t &from, CRenderCommandList &commandList )
	{
		start_index = from.start_index;
		end_index = from.end_index;
		format.CopyFrom( from.format, commandList );
	}

	uint32 start_index;
	uint32 end_index;
	TextFormat_t format;
};


//
// Represents text format data that can be applied to any section of text
//
struct RenderTextFormat_t
{
	RenderTextFormat_t()
		: font_weight( k_EFontWeightUnset )
		, font_style( k_EFontStyleUnset )
		, text_decoration( k_ETextDecorationUnset )
	{}

	void CopyFrom( const RenderTextFormat_t &from, CRenderCommandList &commandList )
	{
		font_name = commandList.CopyString( from.font_name );
		font_size = from.font_size;
		fill_brush_collection.CopyFrom( from.fill_brush_collection, commandList );
		letter_spacing = from.letter_spacing;
		CopyRenderDataPointer( inline_object, from.inline_object, commandList );
		font_weight = from.font_weight;
		font_style = from.font_style;
		text_decoration = from.text_decoration;
	}

	const char *font_name;
	float font_size;
	FillBrushCollection_t fill_brush_collection;
	int32 letter_spacing;
	TextInlineObject_t *inline_object;
	EFontWeight font_weight;
	EFontStyle font_style;
	ETextDecoration text_decoration;
};


//
// Specifies a custom text format for a region of text
//
struct RenderTextRangeFormat_t
{
	void CopyFrom( const RenderTextRangeFormat_t &from, CRenderCommandList &commandList )
	{
		start_index = from.start_index;
		end_index = from.end_index;
		format.CopyFrom( from.format, commandList );
	}

	uint32 start_index;
	uint32 end_index;
	RenderTextFormat_t format;
};


//
// Draw text into a rect region, from layout to animation thread
//
struct DrawTextRegionRenderCommand_t : public RenderCommand_t
{
	DrawTextRegionRenderCommand_t() : RenderCommand_t( k_EDrawTextRegion ) {}

	void PushCommandCopy( CRenderCommandList &commandList ) const
	{
		DrawTextRegionRenderCommand_t *pOther = commandList.PushCommand< DrawTextRegionRenderCommand_t >();
		pOther->CopyBaseCommandFrom( *this, commandList );

		if ( raw_text )
		{
			pOther->raw_text = commandList.Alloc( raw_text_bytes );
			V_memcpy( pOther->raw_text, raw_text, raw_text_bytes );
			pOther->raw_text_bytes = raw_text_bytes;
		}

		pOther->text_chars = text_chars;
		pOther->default_format.CopyFrom( default_format, commandList );
		pOther->line_height = line_height;
		pOther->top_left.CopyFrom( top_left, commandList );
		pOther->bottom_right.CopyFrom( bottom_right, commandList );
		pOther->range_formats.CopyFrom( range_formats, commandList );
		pOther->text_encoding = text_encoding;
		pOther->text_align = text_align;
		pOther->wrapping = wrapping;
		pOther->ellipsis = ellipsis;
	}

	// Includes zero termination.
	void *raw_text;
	uint32 raw_text_bytes;
	// Character count for raw_text, does not include the terminator.
	int32 text_chars;
	TextFormat_t default_format;
	uint32 line_height;
	RenderPoint2D_t top_left;
	RenderPoint2D_t bottom_right;
	CRenderDataList< TextRangeFormatData_t > range_formats;
	EPanoramaTextEncoding text_encoding;
	ETextAlign text_align;
	bool wrapping : 1;
	bool ellipsis : 1;
};


//
// Draw text into a rect region, from animation, to rendering, post interpolation of fills
//
struct RenderTextRegionCommand_t : public RenderCommand_t
{
	RenderTextRegionCommand_t() : RenderCommand_t( k_EDrawTextRegion ) {}

	void PushCommandCopy( CRenderCommandList &commandList ) const
	{
		RenderTextRegionCommand_t *pOther = commandList.PushCommand< RenderTextRegionCommand_t >();
		pOther->CopyBaseCommandFrom( *this, commandList );

		if ( raw_text )
		{
			pOther->raw_text = commandList.Alloc( raw_text_bytes );
			V_memcpy( pOther->raw_text, raw_text, raw_text_bytes );
			pOther->raw_text_bytes = raw_text_bytes;
		}

		pOther->text_chars = text_chars;
		pOther->default_format.CopyFrom( default_format, commandList );
		pOther->line_height = line_height;
		pOther->top_left.CopyFrom( top_left, commandList );
		pOther->bottom_right.CopyFrom( bottom_right, commandList );
		pOther->range_formats.CopyFrom( range_formats, commandList );
		pOther->text_encoding = text_encoding;
		pOther->text_align = text_align;
		pOther->wrapping = wrapping;
		pOther->ellipsis = ellipsis;

		CopyRenderDataPointer( pOther->text_shadow, text_shadow, commandList );
	}

	// Includes zero termination.
	void *raw_text;
	uint32 raw_text_bytes;
	// Character count for raw_text, does not include the terminator.
	int32 text_chars;
	RenderTextFormat_t default_format;
	uint32 line_height;
	RenderPoint2D_t top_left;
	RenderPoint2D_t bottom_right;
	CRenderDataList< RenderTextRangeFormat_t > range_formats;
	EPanoramaTextEncoding text_encoding;
	ETextAlign text_align;
	bool wrapping : 1;
	bool ellipsis : 1;

	TextShadowData_t *text_shadow;
};

//
// Tell the render thread to call the panel back on the specified method, which will then be able to do direct render system calls on the render thread
// 
struct RequestRenderCallbackCommand_t : public RenderCommand_t
{
	RequestRenderCallbackCommand_t() : RenderCommand_t( k_ERequestRenderCallback ) {}

	void PushCommandCopy( CRenderCommandList &commandList ) const
	{
		RequestRenderCallbackCommand_t *pOther = commandList.PushCommand< RequestRenderCallbackCommand_t >();
		pOther->CopyBaseCommandFrom( *this, commandList );

		// Note that the ref count is handled elsewhere
		pOther->pCallbackObj = pCallbackObj;

		pOther->top_left.CopyFrom( top_left, commandList );
		pOther->bottom_right.CopyFrom( bottom_right, commandList );

		pOther->top_left_padding.CopyFrom( top_left_padding, commandList );
		pOther->bottom_right_padding.CopyFrom( bottom_right_padding, commandList );

		pOther->flags = flags;
		pOther->panelRT.CopyFrom( panelRT, commandList );
	}

	class CRenderThreadCallback *pCallbackObj;

	// Quad comes through so in the callback we can be told where to render into active composition layer, callback should clip correctly.
	RenderPoint2D_t top_left;
	RenderPoint2D_t bottom_right;

	RenderPoint2D_t top_left_padding;
	RenderPoint2D_t bottom_right_padding;

	ERenderCallbackFlags flags;
	CRenderCommandTexture panelRT;
};


//
// A render command that is just a reference to another command
// 
struct NestedRenderCommand_t : public RenderCommand_t
{
	NestedRenderCommand_t() : RenderCommand_t( k_ENestedCommand ) {}

	void PushCommandCopy( CRenderCommandList &commandList ) const
	{
		NestedRenderCommand_t *pOther = commandList.PushCommand< NestedRenderCommand_t >();
		pOther->CopyBaseCommandFrom( *this, commandList );
	}

	RenderCommand_t *command;
};


//
// A render command that is just a reference to another command
// 
struct NestedCommandListCommand_t : public RenderCommand_t
{
	NestedCommandListCommand_t() : RenderCommand_t( k_ENestedCommandList ) {}

	CRenderCommandList *command_list;
};


//
// Render command for blur rectangles
//

struct BlurPanelsCommand_t : public RenderCommand_t
{
	BlurPanelsCommand_t( ) : RenderCommand_t( k_EPushBlurPanels ) {}

	void PushCommandCopy( CRenderCommandList &commandList ) const
	{
		BlurPanelsCommand_t *pOther = commandList.PushCommand< BlurPanelsCommand_t >( );
		pOther->CopyBaseCommandFrom( *this, commandList );

		pOther->blurPanel = blurPanel;
		pOther->srcPanels.CopyFrom( srcPanels, commandList );
	}

	uint64	blurPanel;
	CRenderDataList< uint64 > srcPanels;
};




// 
// Convert a VMatrix into a RenderMatrix4x4_t
//
inline void VMatrixToRenderMatrix( RenderMatrix4x4_t &renderMatrix, const VMatrix &vmatrix )
{
	renderMatrix.m00 = vmatrix[ 0 ][ 0 ];
	renderMatrix.m01 = vmatrix[ 0 ][ 1 ];
	renderMatrix.m02 = vmatrix[ 0 ][ 2 ];
	renderMatrix.m03 = vmatrix[ 0 ][ 3 ];

	renderMatrix.m10 = vmatrix[ 1 ][ 0 ];
	renderMatrix.m11 = vmatrix[ 1 ][ 1 ];
	renderMatrix.m12 = vmatrix[ 1 ][ 2 ];
	renderMatrix.m13 = vmatrix[ 1 ][ 3 ];

	renderMatrix.m20 = vmatrix[ 2 ][ 0 ];
	renderMatrix.m21 = vmatrix[ 2 ][ 1 ];
	renderMatrix.m22 = vmatrix[ 2 ][ 2 ];
	renderMatrix.m23 = vmatrix[ 2 ][ 3 ];

	renderMatrix.m30 = vmatrix[ 3 ][ 0 ];
	renderMatrix.m31 = vmatrix[ 3 ][ 1 ];
	renderMatrix.m32 = vmatrix[ 3 ][ 2 ];
	renderMatrix.m33 = vmatrix[ 3 ][ 3 ];
}


// 
// Convert a RenderMatrix4x4_t into a VMatrix
//
inline void RenderMatrixToVMatrix( VMatrix &matrix, const RenderMatrix4x4_t &renderMatrix, bool *pIsIdentity = NULL )
{
	matrix[ 0 ][ 0 ] = renderMatrix.m00;
	matrix[ 0 ][ 1 ] = renderMatrix.m01;
	matrix[ 0 ][ 2 ] = renderMatrix.m02;
	matrix[ 0 ][ 3 ] = renderMatrix.m03;
	
	matrix[ 1 ][ 0 ] = renderMatrix.m10;
	matrix[ 1 ][ 1 ] = renderMatrix.m11;
	matrix[ 1 ][ 2 ] = renderMatrix.m12;
	matrix[ 1 ][ 3 ] = renderMatrix.m13;

	matrix[ 2 ][ 0 ] = renderMatrix.m20;
	matrix[ 2 ][ 1 ] = renderMatrix.m21;
	matrix[ 2 ][ 2 ] = renderMatrix.m22;
	matrix[ 2 ][ 3 ] = renderMatrix.m23;

	matrix[ 3 ][ 0 ] = renderMatrix.m30;
	matrix[ 3 ][ 1 ] = renderMatrix.m31;
	matrix[ 3 ][ 2 ] = renderMatrix.m32;
	matrix[ 3 ][ 3 ] = renderMatrix.m33;

	if ( pIsIdentity )
	{
		*pIsIdentity = matrix.IsIdentity();
	}
}


// 
// Convert a Vector to/from a RenderPoint_t
//
inline void VectorToRenderPoint( RenderPoint_t &point, const Vector &v )
{
	point.x = v.x;
	point.y = v.y;
	point.z = v.z;
}
inline void RenderPointToVector( Vector &v, const RenderPoint_t &point )
{
	v.x = point.x;
	v.y = point.y;
	v.z = point.z;
}


// 
// Convert a text format and text ranges into a UITextLayoutProperties_t
//
void RenderCommandToTextLayoutKey( const RenderTextFormat_t &defaultFormat, const CRenderDataList< RenderTextRangeFormat_t > &rangeFormats, UITextLayoutProperties_t *pKey );
void RenderCommandToTextLayoutKey( const TextFormat_t &defaultFormat, const CRenderDataList< TextRangeFormatData_t > &rangeFormats, UITextLayoutProperties_t *pKey );


} // namespace panorama

#endif // RENDERCOMMANDS_H
