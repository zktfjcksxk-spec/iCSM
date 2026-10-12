//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef UIFONTFILELOADERWIN32_H
#define UIFONTFILELOADERWIN32_H

#ifdef _WIN32
#pragma once
#endif

#include <DWrite.h>
#include "panorama/text/iuitextlayout.h"
#include "uifontfile.h"
#include "uitextlayoutwin32.h"
#include "tier0/threadtools.h"

namespace panorama
{

class CFontFileValidatable;

#ifdef DBGFLAG_VALIDATE
extern CUtlRBTree< CFontFileValidatable *, int, CDefLess< CFontFileValidatable * > > g_treeValidate;
#endif

class CFontFileValidatable
{
public:
	CFontFileValidatable()
	{
#ifdef DBGFLAG_VALIDATE
		g_treeValidate.Insert( this );
#endif
	}

	virtual ~CFontFileValidatable()
	{
#ifdef DBGFLAG_VALIDATE
		g_treeValidate.Remove( this );
#endif
	}

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const char *pchName ) = 0;	
#endif
};


struct FontFileIdentifier_t
{
	CPanoramaSymbol symFontFileOrPackage;
	int iFontPackageIndex;
};

//-----------------------------------------------------------------------------
// Purpose: Implements the IDWriteFontFileStream interface in terms of a font
// shipped in an encrypted UIFont container in the application
//-----------------------------------------------------------------------------
class UIFontFileStream : public IDWriteFontFileStream, public CFontFileValidatable
{
public:
	explicit UIFontFileStream( int iIdentifier );
	virtual ~UIFontFileStream();

	// IUnknown methods
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** ppvObject);
	virtual ULONG STDMETHODCALLTYPE AddRef();
	virtual ULONG STDMETHODCALLTYPE Release();

	// IDWriteFontFileStream methods
	virtual HRESULT STDMETHODCALLTYPE ReadFileFragment(
		void const** fragmentStart, // [fragmentSize] in bytes
		UINT64 fileOffset,
		UINT64 fragmentSize,
		OUT void** fragmentContext
		);

	virtual void STDMETHODCALLTYPE ReleaseFileFragment(
		void* fragmentContext
		);

	virtual HRESULT STDMETHODCALLTYPE GetFileSize(
		OUT UINT64* fileSize
		);

	virtual HRESULT STDMETHODCALLTYPE GetLastWriteTime(
		OUT UINT64* lastWriteTime
		);

	bool IsInitialized()
	{
		return m_bFileLoaded;
	}

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const char *pchName )
	{
		validator.ClaimMemory( this );
		ValidateObj( m_bufFileData );
	}
#endif

private:
	CUtlBuffer m_bufFileData;
	bool m_bFileLoaded;
	ULONG refCount_;
};



//-----------------------------------------------------------------------------
// Purpose: Class to load custom font collection
//-----------------------------------------------------------------------------
class UIFontCollectionLoader : public IDWriteFontCollectionLoader
{
public:
	UIFontCollectionLoader() : refCount_(0)
	{
	}

	// IUnknown methods
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** ppvObject);
	virtual ULONG STDMETHODCALLTYPE AddRef();
	virtual ULONG STDMETHODCALLTYPE Release();

	// IDWriteFontCollectionLoader methods
	virtual HRESULT STDMETHODCALLTYPE CreateEnumeratorFromKey(
		IDWriteFactory* factory,
		void const* collectionKey,                      // [collectionKeySize] in bytes
		UINT32 collectionKeySize,
		OUT IDWriteFontFileEnumerator** fontFileEnumerator
		);

	// Gets the singleton loader instance.
	static IDWriteFontCollectionLoader* GetLoader()
	{
		return instance_;
	}

	static void SetInstance( IDWriteFontCollectionLoader *pInstance )
	{
		instance_ = pInstance;
		instance_->AddRef();
	}

	static void ReleaseInstance()
	{
		SAFE_RELEASE( instance_ );
	}

	static bool IsLoaderInitialized()
	{
		return instance_ != NULL;
	}

	static FontFileIdentifier_t *GetFontIdentifier( int iIndex ) { return &(m_listFontIdentifiers[iIndex]); }
	static int AllocateFontIdentifier() { return m_listFontIdentifiers.AddToTail(); }

#ifdef DBGFLAG_VALIDATE
	static void ValidateStatics( CValidator &validator, const char *pchName )
	{
		validator.ClaimMemory( instance_ );
		ValidateObj( m_listFontIdentifiers );
		ValidateObj( g_treeValidate );
		FOR_EACH_RBTREE_FAST( g_treeValidate, i )
		{
			g_treeValidate[i]->Validate( validator, "UIFontFileLoaderWin32.h" );
		}
	}
#endif

private:
	ULONG refCount_;

	// We use linked list indexes as the persistently valid ptr values for font keys, never
	// remove from this list until the object destructs, we'll leak some data, but it's the only way to
	// handle this safely.
	static CUtlLinkedList< FontFileIdentifier_t, int > m_listFontIdentifiers;

	static IDWriteFontCollectionLoader* instance_;
};


//-----------------------------------------------------------------------------
// Purpose: UIFontFileLoader
//-----------------------------------------------------------------------------
class UIFontFileLoader : public IDWriteFontFileLoader, public CFontFileValidatable
{
public:
	UIFontFileLoader() : refCount_(0)
	{
	}

	virtual ~UIFontFileLoader();

	// IUnknown methods
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** ppvObject);
	virtual ULONG STDMETHODCALLTYPE AddRef();
	virtual ULONG STDMETHODCALLTYPE Release();

	// IDWriteFontFileLoader methods
	virtual HRESULT STDMETHODCALLTYPE CreateStreamFromKey(
		void const* fontFileReferenceKey,       // [fontFileReferenceKeySize] in bytes
		UINT32 fontFileReferenceKeySize,
		OUT IDWriteFontFileStream** fontFileStream
		);

	// Gets the singleton loader instance.
	static IDWriteFontFileLoader* GetLoader()
	{
		return instance_;
	}

	static void SetInstance( IDWriteFontFileLoader *pInstance )
	{
		instance_ = pInstance;
		instance_->AddRef();
	}

	static void ReleaseInstance()
	{
		SAFE_RELEASE( instance_ );
	}

	static bool IsLoaderInitialized()
	{
		return instance_ != NULL;
	}

#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, const char *pchName )
	{
		if ( !validator.IsClaimed( instance_ ) )
			validator.ClaimMemory( instance_ );
		ValidateObj( m_mapFontFileStreams );
	}
#endif

private:
	ULONG refCount_;

	CUtlMap< int, UIFontFileStream *, int, CDefLess< int > > m_mapFontFileStreams;

	static IDWriteFontFileLoader* instance_;
};


//-----------------------------------------------------------------------------
// Purpose: UIFontFileEnumerator
//-----------------------------------------------------------------------------
class UIFontFileEnumerator : public IDWriteFontFileEnumerator, public CFontFileValidatable
{
public:
	UIFontFileEnumerator( IDWriteFactory* factory );

	HRESULT Initialize(	const char *pchFontPath	);

	virtual ~UIFontFileEnumerator()
	{
		SAFE_DELETE( m_pCurrentUIFontPackage );
		SAFE_RELEASE( currentFile_ );
		SAFE_RELEASE( factory_ );
		SAFE_DELETE( m_pDirIterator );
	}

	// IUnknown methods
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** ppvObject);
	virtual ULONG STDMETHODCALLTYPE AddRef();
	virtual ULONG STDMETHODCALLTYPE Release();

	// IDWriteFontFileEnumerator methods
	virtual HRESULT STDMETHODCALLTYPE MoveNext(OUT BOOL* hasCurrentFile);
	virtual HRESULT STDMETHODCALLTYPE GetCurrentFontFile(OUT IDWriteFontFile** fontFile);

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const char *pchName )
	{
		// Claim ourselves as well, because of how our registration stuff works and how com calls create us
		validator.ClaimMemory( this );
		ValidatePtr( m_pDirIterator );
		ValidatePtr( m_pCurrentUIFontPackage );
		ValidateObj( m_strFontPath );
	}
#endif

private:
	ULONG refCount_;

	IDWriteFactory* factory_;
	IDWriteFontFile* currentFile_;

	CDirIterator *m_pDirIterator;
	CUIFontPackage *m_pCurrentUIFontPackage;
	CUtlString m_strFontPath;
};

}

#endif // UIFONTFILELOADERWIN32_H