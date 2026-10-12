//====== Copyright © 2017, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
// Smart pointers for different memory ownership patterns
//
// Example Usage:
//
//  UtlOwnedPtr -- one single strong reference to data
//  Main use cases:
//  - Tree-like data structures.  Use UtlOwnedPtr to hold subtrees.
//  - Containers holding object pointers (no accidental forgetting to PurgeAndDeleteElements)
//  - Temporary heap-allocated buffers (no accidental forgetting to free / boilerplate free on each return)
//  - Data you want to return as a pointer but avoid returning in early-out cases (reduce boilerplate frees)
//  - Quick one-off RAII usage
//
//  Examples:
//	{
//		UtlOwnedPtr<int> pOwned = AllocOwned<int>(3);	// allocate new owned data and get a pointer
//		*pOwned = 4;									// access / modify pointed-to data
//		Assert( pOwned );								// not null
//		UtlOwnedPtr<int> pOwned2 = Move(pOwned);		// give data to a new owner
//		Assert( pOwned2 );								// not null
//		Assert( !pOwned );								// null -- no longer has ownership
//	}													// heap data is automatically freed when pOwned2 is destructed
//
//  {
//      UtlOwnedPtr<FILE, UtlGenericDeleter<FILE>> pFile;
//      pFile.GetDeleter() = &fclose;
//      pFile.Attach( fopen( "example.txt", "r" ) );
//
//      ...
//      if( fwrite( pFile.Get(), ... ) == 0) return;	// don't need to close file in error return, automatically done
//      ...
//  }													// file is automatically closed at end of scope
//
//  UtlRefCountedPtr -- single-threaded shared pointers to some data
//	{
//		UtlRefCountedPtr<int> pShared = AllocRefCountedPtr<int>(3);
//														// allocate new shared data and get a pointer
//		*pShared = 4;									// access / modify pointed-to data
//		Assert( pShared );								// not null
//		UtlRefCountedPtr<int> pShared2 = pShared1;		// no move required!
//		Assert( pShared2 );								// new pointer is valid
//		Assert( pShared );								// old pointer is still valid
//	}													// heap data is automatically freed when both references are destructed
//
// NOTES:
// * You can't upcast an owned pointer -- that is, you can't move from a
//   UtlOwnedPtr<Derived> into a UtlOwnedPtr<Base>.
//
//   There may be cases in which doing so is safe (e.g. if Base has a virtual
//   destructor), but we don't currently have a way to enforce any those conditions,
//   so we disallow it.
//
//   However, we *could* safely allow upcasting at construction time (via something
//   like AllocOwnedUpcast<Base, Derived>(args)), but, to be fully safe, this would
//   require a non-zero-size Deleter.
//
//   You can unsafely chat this by using UtlOwnedPtr<Base>::Attach() if you know
//   that Base has a virtual destructor.
//=============================================================================//

// UtlRefCountedPtrMT lock-free design:
// * IMPORTANT!!
//   UtlRefCountedPtrMT themselves are *not* accessible from multiple threads
//   by default, and need some access control mechanism if their data is shared.
//   (e.g. mutex).  Instead, they imply that the embedded object might be visible
//   to multiple threads.
//
// * Each weak/strong pointer points to a "cell" containing a weak and strong
//   reference count (along with a pointer to the contained data).  This cell
//   is where all the thread-safety logic is contained, since many threads
//   may have pointers that point at it.
//
// * The cell's contained data pointer is kept alive as long as mStrongRefCount > 0
// * The cell itself is kept alive as long as either mStrongRefCount > 0 or
//   mWeakRefCount > 0
//
// * Each UtlRefCountedPtrMT pointing at a cell is paired with 1 increment to
//   mStrongRefCount.
// * Therefore, if mStrongRefCount is 1, at most 1 thread has a UtlRefCountedPtrMT
//   pointing at this cell.  But it's possible that another thread with a weak
//   reference strengthens its pointer and increments mStrongRefCount.
// * So instead we maintain the invariant that once mStrongRefCount becomes 0,
//   it cannot be increased again (it's impossible to strengthen a weak pointer
//   if no known strong pointers are live).
// * Each UtlWeakPtrMT pointing at this cell is paired with 1 increment to
//   mWeakRefCount.
// * In addition, a UtlRefCountedPtrMT in the process of releasing sometimes also
//   increments mWeakRefCount temporarily.  In particular, any thread that might
//   decrement mStrongRefCount to 0 must first increment mWeakRefCount, effectively
//   creating an additional temporary weak pointer while the UtlRefCountedPtrMT is
//   in the process of releasing its reference.
// * This means that when mStrongRefCount initially becomes 0, mWeakRefCount will always
//   be at least 1, keeping the cell alive.
// * When mStrongRefCount is 0 and mWeakRefCount is 1, there is at most 1 thread with
//   a reference to the cell.  At this point it's safe for that thread to delete
//   the cell since no other threads can possibly access the ref-counts or embedded
//   pointer.
//
// TODO: There is still a tiny data race in WeakRelease() where the UtlRefCell might
//       leak.  This is a pretty small object and the race is unlikely, but it would
//       be good to come up with a design that avoids this leak.
//

#ifndef TIER1_UTLPOINTERS_H
#define TIER1_UTLPOINTERS_H

#ifdef _WIN32
#pragma once
#endif

#include <tier0/platform.h> // VALVE_CPP11
#include <tier0/threadtools.h>

#if !VALVE_CPP11
#error "<tier1/utlpointers.h> requires a C++11 compiler"
#endif

#include <tier1/template_utils.h>

// Tag types, used in various constructors
enum GiveOwnership_tag { PTR_GIVE_OWNERSHIP };
enum Construct_tag { PTR_CONSTRUCT };

// Standard deleters
template <typename T>
struct UtlDeleter
{
	static void Delete( T* pT ) { delete pT; } // NB: delete nullptr is defined by the standard to be a no-op
};

template <typename T>
struct UtlArrayDeleter
{
	static void Delete( T* pT ) { delete[] pT; } // NB: delete[] nullptr is defined by the standard to be a no-op
};

template <typename T>
struct UtlFreeDeleter
{
	static void Delete( T* pT ) { free( pT ); } // NB: free(NULL) is defined by the standard to be a no-op
};

template <typename T>
struct UtlNullDeleter
{
	static void Delete( T* pT ) {}
};

// wraps a function pointer for deletion
template <typename T>
struct UtlGenericDeleter
{
	UtlGenericDeleter() : mDeleter( nullptr ) {}
	UtlGenericDeleter( void( *pDeleter )( T* ) ) : mDeleter( pDeleter ) {}
	void Delete( T* pT ) { 
		Assert( mDeleter != nullptr ); // You forgot to set a deleter.  Set to UtlNullDeleter<T> if you really want to do nothing
		if ( mDeleter ) mDeleter( pT );
	}

	// implicit conversion from "static" deleters
	UtlGenericDeleter( const UtlDeleter<T>& ) : mDeleter( &UtlDeleter<T>::Delete ) {}
	UtlGenericDeleter( const UtlArrayDeleter<T>& ) : mDeleter( &UtlArrayDeleter<T>::Delete ) {}
	UtlGenericDeleter( const UtlFreeDeleter<T>& ) : mDeleter( &UtlFreeDeleter<T>::Delete ) {}
	UtlGenericDeleter( const UtlNullDeleter<T>& ) : mDeleter( &UtlNullDeleter<T>::Delete ) {}

	void( *mDeleter )( T* );
};

// A pointer that deletes its contents when it is destructed -- similar to std::unique_ptr
// We derive from D to take advantage of the empty-base-class optimization for most deleters
//
// Note that it should be safe to store a UtlOwnedPtr in Utl structures as it is trivially relocatable
// (i.e. can be moved with memcpy as long as you don't destroy the old version))
template < typename T, typename D = UtlDeleter<T> >
class UtlOwnedPtr : private D
{
public:
	typedef T Element;
	typedef T* Pointer;
	typedef D Deleter;

	// Utility access to deleter as base class
	const FORCEINLINE Deleter& GetDeleter() const { return *this; }
	FORCEINLINE Deleter& GetDeleter() { return *this; }

	~UtlOwnedPtr();

	// allow implicit construction from nullptr
	UtlOwnedPtr();
	UtlOwnedPtr( nullptr_t );
	UtlOwnedPtr( nullptr_t, Deleter&& d );
	UtlOwnedPtr( nullptr_t, const Deleter& d );

	// Attach an existing object to a new pointer
	UtlOwnedPtr( GiveOwnership_tag, Element* p );
	UtlOwnedPtr( GiveOwnership_tag, Element* p, Deleter&& d );
	UtlOwnedPtr( GiveOwnership_tag, Element* p, const Deleter& d );

	// Forwarding constructor to the target's constructor, using new to construct the object
	// Only makes sense using a standard "delete p" deleter, which we enforce by making sure
	// that our deleter is assignable from UtlDeleter<T>.
	template <typename... Args>
	explicit UtlOwnedPtr( Construct_tag, Args&&... args );

	// No copy constructor/assignment
	UtlOwnedPtr( const UtlOwnedPtr& ) = delete;
	UtlOwnedPtr& operator=( const UtlOwnedPtr& ) = delete;

	// Move-able
	UtlOwnedPtr( UtlOwnedPtr&& moveFrom );
	UtlOwnedPtr& operator=( UtlOwnedPtr&& moveFrom );

	// Move with deleter conversion
	template <typename D2>
	UtlOwnedPtr( UtlOwnedPtr<T, D2>&& moveFrom );
	template <typename D2>
	UtlOwnedPtr& operator=( UtlOwnedPtr<T, D2>&& moveFrom );

	// special case: can assign null even without move
	UtlOwnedPtr& operator=( nullptr_t );

	// Clear the pointer
	void Reset();

	// Test for non-null
	explicit operator bool() const;

	// Attach a pointer, giving its ownership to the UtlOwnedPtr
	//
	// NB: Be careful attaching to base classes of derived pointers,
	//     that the deleter is compatible.
	//
	// In particular, if Base doesn't have a virtual destructor, then
	// attaching a derived class to a UtlOwnedPtr<Base> will lead to
	// undefined behavior when the object is deleted via the non-virtual
	// Base pointer.
	void Attach( Element* pT );

	// Detach a pointer from the UtlOwnedPtr, claiming ownership
	Pointer Detach();

	void Swap( UtlOwnedPtr& other );

	// Access to pointer
	Pointer Get() const;
	Pointer operator->() const;
	Element& operator*() const;

private:
	T* mPtr;
};

// copy/move construct with type inference on argument.
//
// Example usage:
//      CFoo someFoo = ...;
//
//      // copy someFoo onto the heap
//		UtlOwnedPtr<CFoo> pFooCopy = MakeOwned( someFoo );
//
//      // move someFoo onto the heap
//      UtlOwnedPtr<CFoo> pFoo = MakeOwned( Move(someFoo) );
template <typename T>
UtlOwnedPtr< FORWARD_CONSTRUCT_TYPE( T ) > MakeOwned( T&& copyOrMove )
{
	return UtlOwnedPtr< FORWARD_CONSTRUCT_TYPE( T ) >( PTR_CONSTRUCT, FORWARD_CONSTRUCT_ARG( T, copyOrMove ) );
}

template <typename T, typename... Args>
UtlOwnedPtr<T> AllocOwned( Args&&... args )
{
	return UtlOwnedPtr<T>( PTR_CONSTRUCT, Forward<Args>( args )... );
}

template <typename T>
UtlOwnedPtr<T, UtlArrayDeleter<T>> AllocOwnedArray( int size )
{
	return UtlOwnedPtr<T, UtlArrayDeleter<T>>( PTR_GIVE_OWNERSHIP, new T[size] );
}

//////////////////////////////////////////////////////////////////////////
// UtlRefCountedPtr and useful side data


// forward declare
template <typename R>
class UtlRefCountedPtrCell;

template <typename T, typename R>
class UtlWeakPtr;

template <typename T, typename R = int>
class UtlRefCountedPtr
{
public:
	typedef T Element;
	typedef T* Pointer;
	typedef R RefCount;
	typedef UtlRefCountedPtrCell<R> Cell;
	typedef UtlWeakPtr<T, R> WeakPtr;

	~UtlRefCountedPtr();
	UtlRefCountedPtr();

	// Attach to existing object
	UtlRefCountedPtr( GiveOwnership_tag, T* pData );
	template <typename D>
	UtlRefCountedPtr( GiveOwnership_tag, T* pData, D&& deleter );

	// Allocate and construct new object
	template <typename... Args>
	explicit UtlRefCountedPtr( Construct_tag, Args&&... args );

	// Nullify
	UtlRefCountedPtr( nullptr_t );
	UtlRefCountedPtr& operator=( nullptr_t );

	// Copy (adds a reference)
	UtlRefCountedPtr( const UtlRefCountedPtr& other );
	UtlRefCountedPtr& operator=( const UtlRefCountedPtr& other );

	// Upcast copy -- allows treating ptr<Derived> as if it was ptr<Base>
	template <typename S, TEMPLATE_REQUIRE_CONVERTIBLE( T*, S* )>
	UtlRefCountedPtr( const UtlRefCountedPtr<S, R>& other );
	template <typename S, TEMPLATE_REQUIRE_CONVERTIBLE( T*, S* )>
	UtlRefCountedPtr& operator=( const UtlRefCountedPtr<S, R>& other );

	// Move from another pointer (faster, no AddRef/Release)
	UtlRefCountedPtr( UtlRefCountedPtr&& other );
	UtlRefCountedPtr& operator= ( UtlRefCountedPtr&& other );

	// Upcast move -- allows treating ptr<Derived> as if it was ptr<Base>
	template <typename S, TEMPLATE_REQUIRE_CONVERTIBLE( T*, S* )>
	UtlRefCountedPtr( UtlRefCountedPtr<S, R>&& other );
	template <typename S, TEMPLATE_REQUIRE_CONVERTIBLE(T*, S*)>
	UtlRefCountedPtr& operator=( UtlRefCountedPtr<S, R>&& other );

	// construct from weak pointer
	template <typename S, TEMPLATE_REQUIRE_CONVERTIBLE( T*, S* )>
	UtlRefCountedPtr( const UtlWeakPtr<S, R>& other );
	template <typename S, TEMPLATE_REQUIRE_CONVERTIBLE( T*, S* )>
	UtlRefCountedPtr& operator=( const UtlWeakPtr<S, R>& other );

	// move from owned pointer, including custom deleter if any
	template <typename S, typename D, TEMPLATE_REQUIRE_CONVERTIBLE( T*, S* )>
	UtlRefCountedPtr( UtlOwnedPtr<S, D>&& other );
	template <typename S, typename D, TEMPLATE_REQUIRE_CONVERTIBLE( T*, S* )>
	UtlRefCountedPtr& operator=( UtlOwnedPtr<S, D>&& other );

	explicit operator bool() const;

	Pointer operator->() const;
	Element& operator*() const;

	Pointer Get() const;
	WeakPtr GetWeak() const;

	void Reset();
	template <typename S, TEMPLATE_REQUIRE_CONVERTIBLE( T*, S* )>
	void Attach( S* pData );
	template <typename S, typename D, TEMPLATE_REQUIRE_CONVERTIBLE( T*, S* )>
	void Attach( S* pData, D&& deleter );

	// Detaches only if this is the last remaining reference to this data.
	// Returns null if the pointer was already null or if the detach fails
	// due to other references existing.
	T* TryDetach();

	int GetRefCount() const;
	int GetWeakRefCount() const;

private:
	// Invariants:
	// either mpCell == nullptr && mpData == nullptr,
	//     or mpData == mpCell->mpData
	//
	// either mpCell == nullptr
	//     or mpCell->GetRefCount() > 0 (we hold a reference)
	T* mpData;
	Cell *mpCell;

	// Other pointers are allowed to access mpCell internally
	template <typename TFriend, typename RFriend>
	friend class UtlRefCountedPtr;
	template <typename TFriend, typename RFriend>
	friend class UtlWeakPtr;
};

template <typename T, typename R = int>
class UtlWeakPtr
{
public:
	typedef T Element;
	typedef R RefCount;
	typedef T* Pointer;
	typedef UtlRefCountedPtrCell<R> Cell;
	typedef UtlRefCountedPtr<T, R> StrongPtr;

	~UtlWeakPtr();
	UtlWeakPtr();
	UtlWeakPtr( nullptr_t );
	UtlWeakPtr& operator=( nullptr_t );

	// copy
	UtlWeakPtr( const UtlWeakPtr& other );
	UtlWeakPtr& operator= ( const UtlWeakPtr& other );
	template <typename S, TEMPLATE_REQUIRE_CONVERTIBLE( T*, S* )>
	UtlWeakPtr( const UtlWeakPtr<S, R>& other );
	template <typename S, TEMPLATE_REQUIRE_CONVERTIBLE( T*, S* )>
	UtlWeakPtr& operator= ( const UtlWeakPtr<S, R>& other );

	// move
	UtlWeakPtr( UtlWeakPtr&& other );
	UtlWeakPtr& operator=( UtlWeakPtr&& other );
	template <typename S, TEMPLATE_REQUIRE_CONVERTIBLE( T*, S* )>
	UtlWeakPtr( UtlWeakPtr<S, R>&& other );
	template <typename S, TEMPLATE_REQUIRE_CONVERTIBLE( T*, S* )>
	UtlWeakPtr& operator=( UtlWeakPtr<S, R>&& other );

	// construct from strong pointer
	template <typename S, TEMPLATE_REQUIRE_CONVERTIBLE( T*, S* )>
	UtlWeakPtr( const UtlRefCountedPtr<S, R>& other );
	template <typename S, TEMPLATE_REQUIRE_CONVERTIBLE( T*, S* )>
	UtlWeakPtr& operator= ( const UtlRefCountedPtr<S, R>& other );

	StrongPtr Strengthen() const;

	void Reset();

	// Note that for UtlWeakPtrMT, this isn't actually a guarantee that Strengthen() will return
	// non-null; another thread could hold the last strong reference and release it before you
	// call Strengthen().  But it's a fast check as to whether you will likely be able to access
	// the contents of the pointer.
	explicit operator bool() const;

	int GetRefCount() const;
	int GetWeakRefCount() const;

private:
	// Invariants:
	// either mpCell == nullptr && mpUnsafeData == nullptr,
	//     or mpUnsafeData == mpCell->mpData,
	//     or mpCell->GetRefCount() == 0 (in this case, mpUnsafeData will be dangling and cannot be dereferenced)
	//
	// either mpCell = nullptr,
	//     or mpCell->GetWeakRefCount() > 0
	T* mpUnsafeData;	// Need to use Strengthen() before accessing this
	Cell* mpCell;

	// Other pointers are allowed to access mpCell internally
	template <typename TFriend, typename RFriend>
	friend class UtlRefCountedPtr;
	template <typename TFriend, typename RFriend>
	friend class UtlWeakPtr;
};

#if 0
// TODO: Our linux GCC doesn't support template using declarations.
// Could hack with an empty subclass that uses forwarding constructors?
// We don't currently use these declarations, so commenting out for now.
template <typename T>
using UtlRefCountedPtrMT = UtlRefCountedPtr<T, CInterlockedIntT<int>>;
template <typename T>
using UtlWeakPtrMT = UtlWeakPtr<T, CInterlockedIntT<int>>;
#endif

//////////////////////////////////////////////////////////////////////////
// Implementation details

template <typename T, typename D>
FORCEINLINE void UtlOwnedPtr<T, D>::Reset()
{
	if ( mPtr )
		GetDeleter().Delete( mPtr );

	mPtr = nullptr;
}

template <typename T, typename D>
FORCEINLINE UtlOwnedPtr<T,D>::~UtlOwnedPtr()
{
	Reset();
}

template <typename T, typename D>
FORCEINLINE UtlOwnedPtr<T,D>::UtlOwnedPtr()
	: Deleter()
	, mPtr( nullptr )
{
}

template <typename T, typename D>
FORCEINLINE UtlOwnedPtr<T,D>::UtlOwnedPtr( nullptr_t )
	: Deleter()
	, mPtr( nullptr )
{
}

template <typename T, typename D>
FORCEINLINE UtlOwnedPtr<T,D>::UtlOwnedPtr( nullptr_t, Deleter&& d )
	: Deleter( Move( d ) )
	, mPtr( nullptr )
{
}

template <typename T, typename D>
FORCEINLINE UtlOwnedPtr<T,D>::UtlOwnedPtr( nullptr_t, const Deleter& d )
	: Deleter( d )
	, mPtr( nullptr )
{
}

template <typename T, typename D>
FORCEINLINE UtlOwnedPtr<T,D>::UtlOwnedPtr( GiveOwnership_tag, Element* p )
	: Deleter()
	, mPtr( p )
{
}

template <typename T, typename D>
FORCEINLINE UtlOwnedPtr<T,D>::UtlOwnedPtr( GiveOwnership_tag, Element* p, Deleter&& d )
	: Deleter( Move( d ) )
	, mPtr( p )
{
}

template <typename T, typename D>
FORCEINLINE UtlOwnedPtr<T,D>::UtlOwnedPtr( GiveOwnership_tag, Element* p, const Deleter& d )
	: Deleter( d )
	, mPtr( p )
{
}

template <typename T, typename D>
template <typename... Args>
FORCEINLINE UtlOwnedPtr<T,D>::UtlOwnedPtr( Construct_tag, Args&&... args )
	: Deleter( UtlDeleter<T>() ) // requires copy-constructible from UtlDeleter<T> since we are using new to create the object
	, mPtr( new T( Forward<Args>( args )... ) )
{
}

template <typename T, typename D>
FORCEINLINE UtlOwnedPtr<T,D>::UtlOwnedPtr( UtlOwnedPtr&& moveFrom )
	: Deleter( Move( moveFrom.GetDeleter() ) )
	, mPtr( moveFrom.Detach() )
{
}

template <typename T, typename D>
FORCEINLINE UtlOwnedPtr<T,D>& UtlOwnedPtr<T,D>::operator=( UtlOwnedPtr&& moveFrom )
{
	// This ordering is careful to gracefully handle self move-assignment
	//
	// NB: we don't handle the deleter move throwing an exception.
	T* tmp = moveFrom.Detach();
	Reset();
	GetDeleter() = Move( moveFrom.GetDeleter() );
	mPtr = tmp;

	return *this;
};

template <typename T, typename D>
template <typename D2>
FORCEINLINE UtlOwnedPtr<T,D>::UtlOwnedPtr( UtlOwnedPtr<T,D2>&& moveFrom )
	: Deleter( moveFrom.GetDeleter() )
	, mPtr( moveFrom.Detach() )
{
}

template <typename T, typename D>
template <typename D2>
FORCEINLINE UtlOwnedPtr<T,D>& UtlOwnedPtr<T,D>::operator=( UtlOwnedPtr<T,D2>&& moveFrom )
{
	// This ordering is careful to gracefully handle self move-assignment
	//
	// NB: we don't handle the deleter move throwing an exception.
	T* tmp = moveFrom.Detach();
	Reset();
	GetDeleter() = Move( moveFrom.GetDeleter() );
	mPtr = tmp;

	return *this;
}

template <typename T, typename D>
FORCEINLINE UtlOwnedPtr<T,D>& UtlOwnedPtr<T,D>::operator=( nullptr_t )
{
	Reset();
	return *this;
}

template <typename T, typename D>
FORCEINLINE UtlOwnedPtr<T,D>::operator bool() const
{
	return mPtr != nullptr;
}

template <typename T, typename D>
void UtlOwnedPtr<T,D>::Attach( Element* pT )
{
	Assert( pT != mPtr || pT == nullptr ); // assert on Attach( Get() ) which is almost certainly a bug.
	Reset();
	mPtr = pT;
}

template <typename T, typename D>
T* UtlOwnedPtr<T,D>::Detach()
{
	Pointer result = mPtr;
	mPtr = nullptr;
	return result;
}

template <typename T, typename D>
void UtlOwnedPtr<T,D>::Swap( UtlOwnedPtr& other )
{
	UtlOwnedPtr tmp = Move( other );
	other = Move( *this );
	*this = Move( tmp );
}

template <typename T, typename D>
FORCEINLINE T* UtlOwnedPtr<T,D>::Get() const
{
	return mPtr;
}

template <typename T, typename D>
FORCEINLINE T* UtlOwnedPtr<T,D>::operator->() const
{
	return mPtr;
}

template <typename T, typename D>
FORCEINLINE T& UtlOwnedPtr<T,D>::operator*() const
{
	return *mPtr;
}

//////////////////////////////////////////////////////////////////////////
// Implement CompareAndSwap for single-threaded (non-synchronized) and
// multi-threaded (synchronized) integers

struct UtlRefCount_AtomicOperations
{
	// Single threaded case
	static FORCEINLINE bool CompareAndSwap( int& obj, int oldValue, int newValue )
	{
		Assert( obj == oldValue );
		obj = newValue;
		return true;
	}

	// Multi-threaded
	static FORCEINLINE bool CompareAndSwap( CInterlockedIntT<int>& obj, int oldValue, int newValue )
	{
		return obj.AssignIf( oldValue, newValue );
	}
};

template <typename R>
class UtlRefCountedPtrCell
{
public:
	typedef R RefCount;

	virtual ~UtlRefCountedPtrCell() {}; // TODO: Replace with = default (doesn't work on our linux gcc dues to nothrow(true) specifier)
	UtlRefCountedPtrCell() : mStrongRefCount( 1 ), mWeakRefCount( 0 ) {}

	// No copy/move
	UtlRefCountedPtrCell( const UtlRefCountedPtrCell& ) = delete;
	UtlRefCountedPtrCell& operator=( const UtlRefCountedPtrCell& ) = delete;

	// delete embedded pointer
	virtual void DeleteElement() = 0;

	// detach embedded pointer without deleting it
	virtual void DetachElement() = 0;

	// This AddRef() should only be called by objects that already hold a strong reference;
	// e.g. we should never AddRef() from 0.  Weak pointers should obtain a reference through
	// Strengthen(), which goes through a more complicated flow to make sure that the object
	// isn't in the process of being destroyed on another thread.
	FORCEINLINE void AddRef()
	{
		Assert( mStrongRefCount > 0 );
		++mStrongRefCount;
	}

	// Similarily, this Release() should only be called by objects hold a strong reference and
	// are in the process of releasing it.
	void Release()
	{
		Assert( mStrongRefCount > 0 );

		// See design notes at top of file for an explanation of this design.
		for ( ;;)
		{
			int refValue = mStrongRefCount;

			// If we are releasing the last strong reference, temporarily grab a weak reference,
			// so that we can coordinate with any existing weak references on deleting this object
			// when the last weak reference is removed.
			//
			// Note that it's possible that a weak reference in another thread was Strengthened()
			// during this period, in which case CompareAndSwap() will fail.  In this case we
			// release our temporary weak reference and try again.
			if ( refValue == 1 )
			{
				++mWeakRefCount;

				// Check if this is really the last release
				if ( !UtlRefCount_AtomicOperations::CompareAndSwap( mStrongRefCount, 1, 0 ) )
				{
					--mWeakRefCount;
					continue; // try again
				}

				// This was the last release.  Delete the underlying object.
				DeleteElement();

				// Release our temporarily weak reference.  We know that mStrongRefCount == 0
				// so immediately delete the object if this was the last weak reference.
				if ( --mWeakRefCount == 0 )
					delete this;

				return;
			}

			// fast path, not decrementing from 1 to zero.  just do the decrement
			if ( !UtlRefCount_AtomicOperations::CompareAndSwap( mStrongRefCount, refValue, refValue - 1 ) )
				continue;

			return;
		}
	}

	// Like "Release()", but only executes if this is the last strong reference *and* doesn't free the target.
	// Returns whether the detach was successful
	bool TryDetach()
	{
		Assert( mStrongRefCount > 0 );

		for( ;; )
		{
			int refValue = mStrongRefCount;
			if ( refValue != 1 )
				return false;

			// As in the "Release()" protocol, we need to add a weakref here to keep the cell alive
			// until we are done with it.
			++mWeakRefCount;

			if ( !UtlRefCount_AtomicOperations::CompareAndSwap( mStrongRefCount, 1, 0 ) )
			{
				// A weak pointer was probably strengthened before we could decrement the refcount to 0.
				// Try again (will probably immediately return false)
				--mWeakRefCount;
				continue;
			}

			DetachElement();

			// At this point we know mStrongRefCount == 0, so if we are the last weak reference, delete the cell too.
			if ( --mWeakRefCount == 0 )
				delete this;

			return true;
		}
	}

	// Increments mStrongRefCount iff it is nonzero, returns whether it was nonzero
	bool Strengthen()
	{
		Assert( mWeakRefCount > 0 );

		for ( ;; )
		{
			int refValue = mStrongRefCount;

			// If we have no strong references, then the underlying object is no longer valid.
			// So we can't strengthen a reference.
			if ( refValue <= 0 )
				return false;

			// Increment refcount (unless someone has changed it behind our back, in which case try again)
			if ( UtlRefCount_AtomicOperations::CompareAndSwap( mStrongRefCount, refValue, refValue + 1 ) )
				return true;
		}
	}

	FORCEINLINE void WeakAddRef()
	{
		++mWeakRefCount;
	}

	FORCEINLINE void WeakRelease()
	{
		if ( mStrongRefCount == 0 )
		{
			// We might be the last reference.
			if ( --mWeakRefCount == 0 )
				delete this;

			return;
		}

		// TODO: Data Race here!
		// 
		// If another thread wakes up and decrements
		// mStrongRefCount to 0, before we decrement our refcount?
		//
		// In that case we won't notice it, and the cell itself will leak.
		// 
		// This is not too scary, since the cell is pretty small, and the
		// race is very unlikely.  There's no risk of memory unsafety, at least,
		// but it's still annoying.
		//
		// Maybe we should keep both refcounts in a single int or int64
		// and modify them atomically?
		--mWeakRefCount;
	}

	FORCEINLINE int GetRefCount() const { return mStrongRefCount; }
	FORCEINLINE int GetWeakRefCount() const { return mWeakRefCount; }

private:
	RefCount mStrongRefCount;
	RefCount mWeakRefCount;
};

// Usually never have a pointer to this, just the 
template <typename T, typename R, typename D = UtlDeleter<T> >
class UtlRefCountedPtrCellImpl : public UtlRefCountedPtrCell<R>
{
public:
	template <typename... Args>
	explicit UtlRefCountedPtrCellImpl( T* pData, Args&&... args )
		: UtlRefCountedPtrCell<R>()
		, mDeleter( Forward<Args>(args)... )
		, mpData( pData )
	{
	}

	~UtlRefCountedPtrCellImpl()
	{
		Assert( mpData == nullptr ); // should have had DeleteElement() called already
	}

	virtual void DeleteElement() OVERRIDE { mDeleter.Delete( mpData ); mpData = nullptr; }
	virtual void DetachElement() OVERRIDE { mpData = nullptr; }

	D mDeleter;
	T* mpData; // never changes after construction except to be nulled when the last strong reference is removed
};

//////////////////////////////////////////////////////////////////////////

template <typename T, typename R>
FORCEINLINE UtlRefCountedPtr<T,R>::~UtlRefCountedPtr()
{
	if ( mpCell )
		mpCell->Release();
	
	// don't really need to do these since the storage is about to go away
	// but may as well.
	mpData = nullptr;
	mpCell = nullptr;
}

template <typename T, typename R>
FORCEINLINE void UtlRefCountedPtr<T, R>::Reset()
{
	// Decrement refcount and free data
	if ( mpCell )
		mpCell->Release();

	mpData = nullptr;
	mpCell = nullptr;
}

template <typename T, typename R>
FORCEINLINE UtlRefCountedPtr<T, R>::UtlRefCountedPtr()
	: mpData( nullptr )
	, mpCell( nullptr )
{
}

template <typename T, typename R>
FORCEINLINE UtlRefCountedPtr<T, R>::UtlRefCountedPtr( GiveOwnership_tag, T* pData )
	: mpData( pData )
	, mpCell( new UtlRefCountedPtrCellImpl<T, R>( pData ) )
{
	// note: new cell starts with refcount 1, no need to addref here
}


template <typename T, typename R>
template <typename D>
FORCEINLINE UtlRefCountedPtr<T, R>::UtlRefCountedPtr( GiveOwnership_tag, T* pData, D&& deleter )
	: mpData( pData )
	, mpCell( new UtlRefCountedPtrCellImpl<T, R, FORWARD_CONSTRUCT_TYPE( D )>
				( pData, FORWARD_CONSTRUCT_ARG( D, deleter ) ) )
{
	// note: new cell starts with refcount 1, no need to addref here
}

template <typename T, typename R>
template <typename... Args>
FORCEINLINE UtlRefCountedPtr<T, R>::UtlRefCountedPtr( Construct_tag, Args&&... args )
	: mpData( new T( Forward<Args>( args )... ) )
	, mpCell( new UtlRefCountedPtrCellImpl<T, R>( mpData ) )
{
}

template <typename T, typename R>
FORCEINLINE UtlRefCountedPtr<T, R>::UtlRefCountedPtr( nullptr_t )
	: mpData( nullptr )
	, mpCell( nullptr )
{
}

template <typename T, typename R>
FORCEINLINE UtlRefCountedPtr<T, R>& UtlRefCountedPtr<T, R>::operator=( nullptr_t )
{
	if ( mpCell )
		mpCell->Release();
	mpData = nullptr;
	mpCell = nullptr;
	return *this;
}

template <typename T, typename R>
FORCEINLINE UtlRefCountedPtr<T, R>::UtlRefCountedPtr( const UtlRefCountedPtr& other )
	: mpData( other.mpData )
	, mpCell( other.mpCell )
{
	if ( mpCell )
		mpCell->AddRef();
}

template <typename T, typename R>
FORCEINLINE UtlRefCountedPtr<T, R>& UtlRefCountedPtr<T, R>::operator=( const UtlRefCountedPtr& other )
{
	// ordering is important here to correctly handle self-assignment
	if ( other.mpCell )
		other.mpCell->AddRef();
	if ( mpCell )
		mpCell->Release();
	mpData = other.mpData;
	mpCell = other.mpCell;
	return *this;
}

template <typename T, typename R>
template <typename S, TEMPLATE_REQUIRE_CONVERTIBLE_REDECL( T*, S* )>
FORCEINLINE UtlRefCountedPtr<T, R>::UtlRefCountedPtr( const UtlRefCountedPtr<S, R>& other )
	: mpData( other.mpData )
	, mpCell( other.mpCell )
{
	if ( mpCell )
		mpCell->AddRef();
}

template <typename T, typename R>
template <typename S, TEMPLATE_REQUIRE_CONVERTIBLE_REDECL( T*, S* )>
FORCEINLINE UtlRefCountedPtr<T, R>& UtlRefCountedPtr<T, R>::operator=( const UtlRefCountedPtr<S, R>& other )
{
	// ordering is important here to correctly handle self-assignment
	if ( other.mpCell )
		other.mpCell->AddRef();
	if ( mpCell )
		mpCell->Release();

	mpData = other.mpData;
	mpCell = other.mpCell;
	return *this;
}

template <typename T, typename R>
FORCEINLINE UtlRefCountedPtr<T, R>::UtlRefCountedPtr( UtlRefCountedPtr&& other )
	: mpData( other.mpData )
	, mpCell( other.mpCell )
{
	other.mpData = nullptr;
	other.mpCell = nullptr;
}

template <typename T, typename R>
FORCEINLINE UtlRefCountedPtr<T, R>& UtlRefCountedPtr<T, R>::operator= ( UtlRefCountedPtr&& other )
{
	// ordering is important here to correctly handle self-move-assignment
	T* pData = other.mpData;
	Cell* pCell = other.mpCell;

	other.mpData = nullptr;
	other.mpCell = nullptr;

	if ( mpCell )
		mpCell->Release();

	mpData = pData;
	mpCell = pCell;
	return *this;
}

template <typename T, typename R>
template <typename S, TEMPLATE_REQUIRE_CONVERTIBLE_REDECL( T*, S* )>
FORCEINLINE UtlRefCountedPtr<T, R>::UtlRefCountedPtr( UtlRefCountedPtr<S, R>&& other )
	: mpData( other.mpData )
	, mpCell( other.mpCell )
{
	other.mpData = nullptr;
	other.mpCell = nullptr;
}

template <typename T, typename R>
template <typename S, TEMPLATE_REQUIRE_CONVERTIBLE_REDECL( T*, S* )>
FORCEINLINE UtlRefCountedPtr<T, R>& UtlRefCountedPtr<T, R>::operator=( UtlRefCountedPtr<S, R>&& other )
{
	// ordering is important here to correctly handle self-move-assignment
	T* pData = other.mpData;
	Cell* pCell = other.mpCell;

	other.mpData = nullptr;
	other.mpCell = nullptr;

	if ( mpCell )
		mpCell->Release();

	mpData = pData;
	mpCell = pCell;
	return *this;
}

template <typename T, typename R>
template <typename S, TEMPLATE_REQUIRE_CONVERTIBLE_REDECL( T*, S* )>
FORCEINLINE UtlRefCountedPtr<T, R>::UtlRefCountedPtr( const UtlWeakPtr<S, R>& other )
{
	if ( other.mpCell->Strengthen() )
	{
		mpCell = other.mpCell;
		mpData = other.mpUnsafeData; // safe because we just called Strengthen()
	}
	else
	{
		mpData = nullptr;
		mpCell = nullptr;
	}
}

template <typename T, typename R>
template <typename S, TEMPLATE_REQUIRE_CONVERTIBLE_REDECL( T*, S* )>
FORCEINLINE UtlRefCountedPtr<T, R>& UtlRefCountedPtr<T, R>::operator=( const UtlWeakPtr<S, R>& other )
{
	// We are a bit careful here, to handle the case of assigning the last remaining strong pointer
	// to an object with a weak pointer to the same object.  In this case we make sure the
	// pointer stays valid.
	if ( other.mpCell->Strengthen() )
	{
		if ( mpCell )
			mpCell->Release();

		// Strengthen() adds a strong reference to other.mpCell,
		// so no need to AddRef() here.
		mpCell = other.mpCell;
		mpData = other.mpUnsafeData; // safe because we just called Strengthen()
	}
	else
	{
		Reset();
	}
	return *this;
}

template <typename T, typename R>
template <typename S, typename D, TEMPLATE_REQUIRE_CONVERTIBLE_REDECL( T*, S* )>
FORCEINLINE UtlRefCountedPtr<T, R>::UtlRefCountedPtr( UtlOwnedPtr<S, D>&& other )
{
	// When constructing a RefCounted pointer from an owned pointer, we use the
	// incoming types S/D to generate the CellImpl, so that the deleter can be
	// called properly even if we have up-casted our pointer.
	S* pData = other.Detach();
	mpData = pData; // might be an upcast here
	mpCell = new UtlRefCountedPtrCellImpl<S, R, D>( pData, Move( other.GetDeleter() ) );
}

template <typename T, typename R>
template <typename S, typename D, TEMPLATE_REQUIRE_CONVERTIBLE_REDECL( T*, S* )>
FORCEINLINE UtlRefCountedPtr<T, R>& UtlRefCountedPtr<T, R>::operator=( UtlOwnedPtr<S, D>&& other )
{
	// When constructing a RefCounted pointer from an owned pointer, we use the
	// incoming types S/D to generate the CellImpl, so that the deleter can be
	// called properly even if we have up-casted our pointer.
	S* pData = other.Detach();
	mpData = pData; // might be an upcast here
	mpCell = new UtlRefCountedPtrCellImpl<S, R, D>( pData, Move( other.GetDeleter() ) );
}

template <typename T, typename R>
FORCEINLINE UtlRefCountedPtr<T, R>::operator bool() const
{
	// No need to check mpCell, we have a strong reference
	return mpData != nullptr;
}

template <typename T, typename R>
FORCEINLINE T* UtlRefCountedPtr<T, R>::operator->() const
{
	return mpData;
}

template <typename T, typename R>
FORCEINLINE T& UtlRefCountedPtr<T, R>::operator*() const
{
	return *mpData;
}

template <typename T, typename R>
FORCEINLINE T* UtlRefCountedPtr<T, R>::Get() const
{
	return mpData;
}

template <typename T, typename R>
FORCEINLINE UtlWeakPtr<T,R> UtlRefCountedPtr<T, R>::GetWeak() const
{
	return WeakPtr( *this );
}

template <typename T, typename R>
template <typename S, TEMPLATE_REQUIRE_CONVERTIBLE_REDECL( T*, S* )>
FORCEINLINE void UtlRefCountedPtr<T, R>::Attach( S* pData )
{
	Reset();
	mpData = pData;
	mpCell = new UtlRefCountedPtrCellImpl<S, R>( pData );
	// new cell starts with refcount 1, no need to addref here
}

template <typename T, typename R>
template <typename S, typename D, TEMPLATE_REQUIRE_CONVERTIBLE_REDECL( T*, S* )>
FORCEINLINE void UtlRefCountedPtr<T, R>::Attach( S* pData, D&& deleter )
{
	Reset();
	mpData = pData;
	mpCell = new UtlRefCountedPtrCellImpl<S, R, FORWARD_CONSTRUCT_TYPE( D )>( pData, FORWARD_CONSTRUCT_ARG( D, deleter ) );
	// new cell starts with refcount 1, no need to addref here
}

template <typename T, typename R>
FORCEINLINE T* UtlRefCountedPtr<T, R>::TryDetach()
{
	if ( !mpCell )
		return nullptr;

	if ( !mpCell->TryDetach() )
		return nullptr;

	T* result = mpData;
	mpData = nullptr;
	mpCell = nullptr;

	return result;
}

template <typename T, typename R>
FORCEINLINE int UtlRefCountedPtr<T, R>::GetRefCount() const
{
	if ( mpCell )
		return mpCell->GetRefCount();

	return -1;
}

template <typename T, typename R>
FORCEINLINE int UtlRefCountedPtr<T, R>::GetWeakRefCount() const
{
	if ( mpCell )
		return mpCell->GetWeakRefCount();

	return -1;
}

//////////////////////////////////////////////////////////////////////////

template <typename T, typename R>
FORCEINLINE void UtlWeakPtr<T, R>::Reset()
{
	if ( mpCell )
		mpCell->WeakRelease();
	mpCell = nullptr;
}

template <typename T, typename R>
FORCEINLINE UtlWeakPtr<T, R>::~UtlWeakPtr()
{
	if ( mpCell )
		mpCell->WeakRelease();
	mpUnsafeData = nullptr;
	mpCell = nullptr;
}

template <typename T, typename R>
FORCEINLINE UtlWeakPtr<T, R>::UtlWeakPtr()
	: mpUnsafeData( nullptr )
	, mpCell( nullptr )
{
}

template <typename T, typename R>
FORCEINLINE UtlWeakPtr<T, R>::UtlWeakPtr( nullptr_t )
	: mpUnsafeData( nullptr )
	, mpCell( nullptr )
{
}

template <typename T, typename R>
FORCEINLINE UtlWeakPtr<T, R>& UtlWeakPtr<T, R>::operator=( nullptr_t )
{
	if ( mpCell )
		mpCell->WeakRelease();
	mpUnsafeData = nullptr;
	mpCell = nullptr;
	return *this;
}

template <typename T, typename R>
FORCEINLINE UtlWeakPtr<T, R>::UtlWeakPtr( const UtlWeakPtr& other )
	: mpUnsafeData( other.mpUnsafeData )
	, mpCell( other.mpCell )
{
	if ( mpCell )
		mpCell->WeakAddRef();
}

template <typename T, typename R>
FORCEINLINE UtlWeakPtr<T, R>& UtlWeakPtr<T, R>::operator= ( const UtlWeakPtr& other )
{
	// ordering important to handle self-assignment
	if ( other.mpCell )
		other.mpCell->WeakAddRef();
	if ( mpCell )
		mpCell->WeakRelease();
	mpUnsafeData = other.mpUnsafeData;
	mpCell = other.mpCell;
	return *this;
}

template <typename T, typename R>
template <typename S, TEMPLATE_REQUIRE_CONVERTIBLE_REDECL( T*, S* )>
FORCEINLINE UtlWeakPtr<T, R>::UtlWeakPtr( const UtlWeakPtr<S, R>& other )
	: mpUnsafeData( other.mpUnsafeData )
	, mpCell( other.mpCell )
{
	if ( mpCell )
		mpCell->WeakAddRef();
}

template <typename T, typename R>
template <typename S, TEMPLATE_REQUIRE_CONVERTIBLE_REDECL( T*, S* )>
FORCEINLINE UtlWeakPtr<T, R>& UtlWeakPtr<T, R>::operator= ( const UtlWeakPtr<S, R>& other )
{
	if ( other.mpCell )
		other.mpCell->WeakAddRef();
	if ( mpCell )
		mpCell->WeakRelease();
	mpUnsafeData = other.mpUnsafeData;
	mpCell = other.mpCell;
	return *this;
}

template <typename T, typename R>
FORCEINLINE UtlWeakPtr<T, R>::UtlWeakPtr( UtlWeakPtr&& other )
	: mpUnsafeData( other.mpUnsafeData )
	, mpCell( other.mpCell )
{
	other.mpUnsafeData = nullptr;
	other.mpCell = nullptr;
}

template <typename T, typename R>
FORCEINLINE UtlWeakPtr<T, R>& UtlWeakPtr<T, R>::operator=( UtlWeakPtr&& other )
{
	// ordering is important here to correctly handle self-move-assignment
	T* pUnsafeData = other.mpUnsafeData;
	Cell* pCell = other.mpCell;

	other.mpUnsafeData = nullptr;
	other.mpCell = nullptr;

	Reset();

	mpUnsafeData = pUnsafeData;
	mpCell = pCell;
	return *this;
}

template <typename T, typename R>
template <typename S, TEMPLATE_REQUIRE_CONVERTIBLE_REDECL( T*, S* )>
FORCEINLINE UtlWeakPtr<T, R>::UtlWeakPtr( UtlWeakPtr<S, R>&& other )
	: mpUnsafeData( other.mpUnsafeData )
	, mpCell( other.mpCell )
{
	other.mpUnsafeData = nullptr;
	other.mpCell = nullptr;
}

template <typename T, typename R>
template <typename S, TEMPLATE_REQUIRE_CONVERTIBLE_REDECL( T*, S* )>
FORCEINLINE UtlWeakPtr<T, R>& UtlWeakPtr<T, R>::operator=( UtlWeakPtr<S, R>&& other )
{
	// ordering is important here to correctly handle self-move-assignment
	T* pUnsafeData = other.mpUnsafeData;
	Cell* pCell = other.mpCell;

	other.mpUnsafeData = nullptr;
	other.mpCell = nullptr;

	Reset();

	mpUnsafeData = pUnsafeData;
	mpCell = pCell;
	return *this;
}

template <typename T, typename R>
template <typename S, TEMPLATE_REQUIRE_CONVERTIBLE_REDECL( T*, S* )>
FORCEINLINE UtlWeakPtr<T, R>::UtlWeakPtr( const UtlRefCountedPtr<S, R>& other )
	: mpUnsafeData( other.mpData )
	, mpCell( other.mpCell )
{
	if ( mpCell )
		mpCell->WeakAddRef();
}

template <typename T, typename R>
template <typename S, TEMPLATE_REQUIRE_CONVERTIBLE_REDECL( T*, S* )>
FORCEINLINE UtlWeakPtr<T, R>& UtlWeakPtr<T, R>::operator= ( const UtlRefCountedPtr<S, R>& other )
{
	// Unlike assigning a strong pointer from a weak pointer, there's no situation in which
	// we care about the weak pointer staying valid even if they point to the same object.
	// So this code path can be very straightforward.
	Reset();
	mpUnsafeData = other.mpData;
	mpCell = other.mpCell;
	if ( mpCell )
		mpCell->WeakAddRef();
}

template <typename T, typename R>
FORCEINLINE UtlRefCountedPtr<T,R> UtlWeakPtr<T, R>::Strengthen() const
{
	return StrongPtr( *this );
}

template <typename T, typename R>
FORCEINLINE UtlWeakPtr<T, R>::operator bool() const
{
	return mpCell != nullptr && mpCell->GetRefCount() > 0;
}

template <typename T, typename R>
FORCEINLINE int UtlWeakPtr<T, R>::GetRefCount() const
{
	if ( mpCell )
		return mpCell->GetRefCount();

	return -1;
}

template <typename T, typename R>
FORCEINLINE int UtlWeakPtr<T, R>::GetWeakRefCount() const
{
	if ( mpCell )
		return mpCell->GetWeakRefCount();

	return -1;
}


#endif // UTLPOINTERS_H
