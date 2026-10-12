//========= Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: An auto-pointer class designed to make using complex data as 
//	template parameters in tier1 containers easier.
//
//==========================================================================//

#ifndef UTLPTR_H
#define UTLPTR_H

#include "utlrbtree.h"

//-----------------------------------------------------------------------------
// Purpose: An auto-pointer class designed to make using complex data as 
//	template parameters in tier1 containers easier.
//-----------------------------------------------------------------------------
template <typename T>
class CUtlPtr
{
public:
	// Constructor/Destructor
	CUtlPtr();
	CUtlPtr( T *pObj );
	~CUtlPtr();

	// Copying
	CUtlPtr( const CUtlPtr &that );
	CUtlPtr &operator=( const CUtlPtr &that );

	// Accessing the object
	const T* operator->() const;
	T* operator->();
	const T& operator *() const;
	T& operator *();

	// Getting/Setting the tracked object
	T* GetPtr();
	const T* GetPtr() const;
	void SetPtr( T* pT );

#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, const char *pchName );
#endif

private:
	// Struct that tracks how many CUtlPtr's point to the same object
	// All CUtlPtr's for a given object will have a shared pointer to
	// one of these
	struct SharedRefCount_t
	{
		T *m_pT;			// The object being tracked
		int m_nRefCount;	// The count of pointers tracking the object
	};

	// Internal state management
	void Init( T* pT );
	void Release();

	// Don't auto-convert this to a pointer. Use the explicit GetPtr/SetPtr instead
	operator T*();

	SharedRefCount_t *m_pRef;
};


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
template <typename T>
inline CUtlPtr<T>::CUtlPtr( ) : m_pRef( NULL )
{
	Init( NULL );
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
template <typename T>
inline CUtlPtr<T>::CUtlPtr( T *pObj ) : m_pRef( NULL )
{
	Init( pObj );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
template <typename T>
inline CUtlPtr<T>::~CUtlPtr()
{
	Release();
}


//-----------------------------------------------------------------------------
// Purpose: Copy constructor
//-----------------------------------------------------------------------------
template <typename T>
inline CUtlPtr<T>::CUtlPtr( const CUtlPtr<T> &that )
: m_pRef( NULL )
{
	*this = that;
}


//-----------------------------------------------------------------------------
// Purpose: Copy assignment
//-----------------------------------------------------------------------------
template <typename T>
inline CUtlPtr<T> &CUtlPtr<T>::operator=( const CUtlPtr<T> &that )
{
	Release();
	
	if ( that.m_pRef )
	{
		m_pRef = that.m_pRef;
		m_pRef->m_nRefCount++;
	}

	return *this;
}


//-----------------------------------------------------------------------------
// Purpose: Accessor
// Returns: A reference to the tracked object
//-----------------------------------------------------------------------------
template <typename T>
inline T* CUtlPtr<T>::operator->()
{
	return m_pRef->m_pT;
}

template <typename T>
inline const T* CUtlPtr<T>::operator->() const
{
	return m_pRef->m_pT;
}


//-----------------------------------------------------------------------------
// Purpose: Accessor
// Returns: A reference to the tracked object
//-----------------------------------------------------------------------------
template <typename T>
inline T& CUtlPtr<T>::operator *()
{
	return *( m_pRef->m_pT );
}

template <typename T>
inline const T& CUtlPtr<T>::operator *() const
{
	return *( m_pRef->m_pT );
}


//-----------------------------------------------------------------------------
// Purpose: Accessor
// Returns: A pointer to the memory being tracked
//-----------------------------------------------------------------------------
template <typename T>
inline T* CUtlPtr<T>::GetPtr()
{
	if ( m_pRef )
		return m_pRef->m_pT;

	return NULL;
}

template <typename T>
inline const T* CUtlPtr<T>::GetPtr() const
{
	if ( m_pRef )
		return m_pRef->m_pT;

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Explicit settor. Changes the object being tracked to the new object
//	passed in.
// Input:	pT - Pointer to the new object to be tracked.
//-----------------------------------------------------------------------------
template <typename T>
inline void CUtlPtr<T>::SetPtr( T* pT )
{
	Release();
	Init( pT );
}


//-----------------------------------------------------------------------------
// Purpose: Initializes the internal tracking of an object
// Input:	pT - Pointer to the new object to be tracked.
//-----------------------------------------------------------------------------
template <typename T>
inline void CUtlPtr<T>::Init( T* pT )
{
	if ( pT )
	{
		m_pRef = new SharedRefCount_t;
		m_pRef->m_pT = pT;
		m_pRef->m_nRefCount = 1;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Safely stops tracking the tracked object.
//-----------------------------------------------------------------------------
template <typename T>
inline void CUtlPtr<T>::Release()
{
	if ( !m_pRef )
		return;

	m_pRef->m_nRefCount--;
	if ( 0 == m_pRef->m_nRefCount )
	{
		delete m_pRef->m_pT;
		delete m_pRef;
	}

	m_pRef = NULL;
}


//-----------------------------------------------------------------------------
// Data and memory validation
//-----------------------------------------------------------------------------
#ifdef DBGFLAG_VALIDATE
template <typename T> 
void CUtlPtr<T>::Validate( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE();
	validator.ClaimMemory( m_pRef );
}
#endif // DBGFLAG_VALIDATE


//-----------------------------------------------------------------------------
// Subclass of CUtlPtr that defines operator< so that it can be used as a key
// in a CUtl container
//-----------------------------------------------------------------------------
template <typename T>
class CUtlKeyPtr : public CUtlPtr<T>
{
public:
	CUtlKeyPtr() : CUtlPtr<T>() { }
	CUtlKeyPtr( T *pObj ) : CUtlPtr<T>( pObj ) { }
	CUtlKeyPtr( const CUtlKeyPtr &that ) : CUtlPtr<T>( that ) { }

	bool operator<( const CUtlKeyPtr &that ) const;
};

template <typename T>
inline bool CUtlKeyPtr<T>::operator<( const CUtlKeyPtr<T> &that ) const
{
	return *( this->GetPtr() ) < *( that.GetPtr() );
}




#endif // UTLPTR_H
