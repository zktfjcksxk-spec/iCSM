//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx.h"
#include "uifontfileloaderwin32.h"
#include "uienginewin32.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

// static 
CUtlLinkedList< FontFileIdentifier_t, int > UIFontCollectionLoader::m_listFontIdentifiers;

// Singleton instance
IDWriteFontCollectionLoader* UIFontCollectionLoader::instance_ = NULL;

// Singleton instance
IDWriteFontFileLoader* UIFontFileLoader::instance_ = NULL;

#ifdef DBGFLAG_VALIDATE
CUtlRBTree< CFontFileValidatable *, int, CDefLess< CFontFileValidatable * > > panorama::g_treeValidate;
#endif


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
UIFontFileStream::UIFontFileStream( int iIdentifier ) :
	refCount_(0),
	m_bFileLoaded( false )
{
	FontFileIdentifier_t *pIdentifier = UIFontCollectionLoader::GetFontIdentifier( iIdentifier );

	if ( pIdentifier->iFontPackageIndex != -1 )
	{
		CUIFontPackage package( pIdentifier->symFontFileOrPackage.String() );
		CUtlString symFontName;
		m_bFileLoaded = package.BGetFontNameAndData( pIdentifier->iFontPackageIndex, symFontName, &m_bufFileData );
	}
	else
	{
		m_bFileLoaded = LoadFileIntoBuffer( pIdentifier->symFontFileOrPackage.String(), m_bufFileData, false );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
UIFontFileStream::~UIFontFileStream()
{
	m_bufFileData.Clear();
}


//-----------------------------------------------------------------------------
// Purpose: QueryInterface
//-----------------------------------------------------------------------------
HRESULT STDMETHODCALLTYPE UIFontFileStream::QueryInterface(REFIID iid, void** ppvObject)
{
	if (iid == IID_IUnknown || iid == __uuidof(IDWriteFontFileStream))
	{
		*ppvObject = this;
		AddRef();
		return S_OK;
	}
	else
	{
		*ppvObject = NULL;
		return E_NOINTERFACE;
	}
}


//-----------------------------------------------------------------------------
// Purpose: AddRef
//-----------------------------------------------------------------------------
ULONG STDMETHODCALLTYPE UIFontFileStream::AddRef()
{
	return InterlockedIncrement( &refCount_ );
}


//-----------------------------------------------------------------------------
// Purpose: Release
//-----------------------------------------------------------------------------
ULONG STDMETHODCALLTYPE UIFontFileStream::Release()
{
	ULONG newCount = InterlockedDecrement( &refCount_ );
	if ( newCount == 0 )
		delete this;

	return newCount;
}


//-----------------------------------------------------------------------------
// Purpose: Read a portion of the file
//-----------------------------------------------------------------------------
HRESULT STDMETHODCALLTYPE UIFontFileStream::ReadFileFragment(
	void const** fragmentStart, // [fragmentSize] in bytes
	UINT64 fileOffset,
	UINT64 fragmentSize,
	OUT void** fragmentContext
	)
{
	VPROF_BUDGET( "UIFontFileStream::ReadFileFragment", VPROF_BUDGETGROUP_STEAMUI );

	// The loader is responsible for doing a bounds check.
	if (fileOffset <= m_bufFileData.TellPut() && 
		fragmentSize <= m_bufFileData.TellPut() - fileOffset)
	{
		*fragmentStart = static_cast<BYTE const*>(m_bufFileData.Base()) + static_cast<size_t>(fileOffset);
		*fragmentContext = NULL;
		return S_OK;
	}
	else
	{
		*fragmentStart = NULL;
		*fragmentContext = NULL;
		return E_FAIL;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Release a portion of the file
//-----------------------------------------------------------------------------
void STDMETHODCALLTYPE UIFontFileStream::ReleaseFileFragment( void* fragmentContext )
{
}


//-----------------------------------------------------------------------------
// Purpose: Get the total file size
//-----------------------------------------------------------------------------
HRESULT STDMETHODCALLTYPE UIFontFileStream::GetFileSize(	OUT UINT64* fileSize )
{
	*fileSize = m_bufFileData.TellPut();
	return S_OK;
}


//-----------------------------------------------------------------------------
// Purpose: Get the last write time
//-----------------------------------------------------------------------------
HRESULT STDMETHODCALLTYPE UIFontFileStream::GetLastWriteTime( OUT UINT64* lastWriteTime )
{
	// The concept of last write time does not apply to this loader.
	*lastWriteTime = 0;
	return E_NOTIMPL;
}


//-----------------------------------------------------------------------------
// Purpose: QueryInterface
//-----------------------------------------------------------------------------
HRESULT STDMETHODCALLTYPE UIFontFileLoader::QueryInterface( REFIID iid, void** ppvObject )
{
	if( iid == IID_IUnknown || iid == __uuidof( IDWriteFontFileLoader ) )
	{
		*ppvObject = this;
		AddRef();
		return S_OK;
	}
	else
	{
		*ppvObject = NULL;
		return E_NOINTERFACE;
	}
}


//-----------------------------------------------------------------------------
// Purpose: AddRef
//-----------------------------------------------------------------------------
ULONG STDMETHODCALLTYPE UIFontFileLoader::AddRef()
{
	return InterlockedIncrement (&refCount_ );
}


//-----------------------------------------------------------------------------
// Purpose: Release
//-----------------------------------------------------------------------------
ULONG STDMETHODCALLTYPE UIFontFileLoader::Release()
{
	ULONG newCount = InterlockedDecrement( &refCount_ );
	if (newCount == 0)
		delete this;

	return newCount;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
UIFontFileLoader::~UIFontFileLoader()
{
	FOR_EACH_MAP_FAST( m_mapFontFileStreams, i )
	{
		m_mapFontFileStreams[i]->Release();
	}
	m_mapFontFileStreams.Purge();
}


//-----------------------------------------------------------------------------
// Purpose: CreateStreamFromKey
//-----------------------------------------------------------------------------
HRESULT STDMETHODCALLTYPE UIFontFileLoader::CreateStreamFromKey(
	void const* fontFileReferenceKey,       // [fontFileReferenceKeySize] in bytes
	UINT32 fontFileReferenceKeySize,
	OUT IDWriteFontFileStream** fontFileStream
	)
{
	VPROF_BUDGET( "UIFontFileLoader::CreateStreamFromKey", VPROF_BUDGETGROUP_STEAMUI );
	*fontFileStream = NULL;

	int key = *((int*)fontFileReferenceKey );

	int iMap = m_mapFontFileStreams.Find( key );
	if ( iMap != m_mapFontFileStreams.InvalidIndex() )
	{
		*fontFileStream = m_mapFontFileStreams[iMap];
		(*fontFileStream)->AddRef();
		return S_OK;
	}

	UIFontFileStream* stream = new UIFontFileStream( key );
	if (stream == NULL)
		return E_OUTOFMEMORY;

	if ( !stream->IsInitialized() )
	{
		delete stream;
		return E_FAIL;
	}

	if ( stream )
		stream->AddRef();
	*fontFileStream = stream;

	m_mapFontFileStreams.Insert( key, stream );
	stream->AddRef();

	return S_OK;
}





//-----------------------------------------------------------------------------
// Purpose: QueryInterface
//-----------------------------------------------------------------------------
HRESULT STDMETHODCALLTYPE UIFontCollectionLoader::QueryInterface( REFIID iid, void** ppvObject )
{
	if( iid == IID_IUnknown || iid == __uuidof(IDWriteFontCollectionLoader) )
	{
		*ppvObject = this;
		AddRef();
		return S_OK;
	}
	else
	{
		*ppvObject = NULL;
		return E_NOINTERFACE;
	}
}


//-----------------------------------------------------------------------------
// Purpose: AddRef
//-----------------------------------------------------------------------------
ULONG STDMETHODCALLTYPE UIFontCollectionLoader::AddRef()
{
	return InterlockedIncrement( &refCount_ );
}


//-----------------------------------------------------------------------------
// Purpose: Release
//-----------------------------------------------------------------------------
ULONG STDMETHODCALLTYPE UIFontCollectionLoader::Release()
{
	ULONG newCount = InterlockedDecrement( &refCount_ );
	if ( newCount == 0 )
		delete this;

	return newCount;
}


//-----------------------------------------------------------------------------
// Purpose: CreateEnumeratorFromKey
//-----------------------------------------------------------------------------
HRESULT STDMETHODCALLTYPE UIFontCollectionLoader::CreateEnumeratorFromKey(
	IDWriteFactory* factory,
	void const* collectionKey,                      // [collectionKeySize] in bytes
	UINT32 collectionKeySize,
	OUT IDWriteFontFileEnumerator** fontFileEnumerator
	)
{
	*fontFileEnumerator = NULL;

	HRESULT hr = S_OK;

	UIFontFileEnumerator* enumerator = new UIFontFileEnumerator( factory );
	if ( enumerator == NULL )
		return E_OUTOFMEMORY;

	const char *pchPath = static_cast<const char *>(collectionKey);
	hr = enumerator->Initialize( pchPath );

	if ( FAILED(hr) )
	{
		delete enumerator;
		return hr;
	}

	*fontFileEnumerator = enumerator;
	return hr;
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
UIFontFileEnumerator::UIFontFileEnumerator( IDWriteFactory* factory ) : 
refCount_(0), 
	factory_(factory),
	currentFile_(),
	m_pDirIterator( NULL ),
	m_pCurrentUIFontPackage( NULL )
{
	factory_->AddRef();
}


//-----------------------------------------------------------------------------
// Purpose: Initialize
//-----------------------------------------------------------------------------
HRESULT UIFontFileEnumerator::Initialize( const char *pchFontPath )
{
	Msg( "Initializing font file enumerator: %s\n", pchFontPath );
	m_strFontPath = pchFontPath;

	CUtlString strSearchPath = pchFontPath;
	strSearchPath += "*";

	m_pDirIterator = new CDirIterator( strSearchPath.String() );
	return S_OK;
}


//-----------------------------------------------------------------------------
// Purpose: QueryInterface
//-----------------------------------------------------------------------------
HRESULT STDMETHODCALLTYPE UIFontFileEnumerator::QueryInterface( REFIID iid, OUT void** ppvObject )
{
	if ( iid == IID_IUnknown || iid == __uuidof(IDWriteFontFileEnumerator) )
	{
		*ppvObject = this;
		AddRef();
		return S_OK;
	}
	else
	{
		*ppvObject = NULL;
		return E_NOINTERFACE;
	}
}


//-----------------------------------------------------------------------------
// Purpose: AddRef
//-----------------------------------------------------------------------------
ULONG STDMETHODCALLTYPE UIFontFileEnumerator::AddRef()
{
	return InterlockedIncrement( &refCount_ );
}


//-----------------------------------------------------------------------------
// Purpose: Release
//-----------------------------------------------------------------------------
ULONG STDMETHODCALLTYPE UIFontFileEnumerator::Release()
{
	ULONG newCount = InterlockedDecrement( &refCount_ );
	if ( newCount == 0 )
		delete this;

	return newCount;
}


//-----------------------------------------------------------------------------
// Purpose: MoveNext
//-----------------------------------------------------------------------------
HRESULT STDMETHODCALLTYPE UIFontFileEnumerator::MoveNext( OUT BOOL* hasCurrentFile )
{
	HRESULT hr = S_OK;

	int iIdentifier = UIFontCollectionLoader::AllocateFontIdentifier();
	FontFileIdentifier_t *pFontIdentifier = UIFontCollectionLoader::GetFontIdentifier( iIdentifier );
	
	pFontIdentifier->symFontFileOrPackage = "";
	pFontIdentifier->iFontPackageIndex = -1;

	*hasCurrentFile = FALSE;
	SAFE_RELEASE( currentFile_ );
	
	while ( 1 )
	{
		int iNextPackageFileIndex = m_pCurrentUIFontPackage ? m_pCurrentUIFontPackage->GetNextFileIndex() : -1;
		if ( iNextPackageFileIndex == -1 )
		{
			SAFE_DELETE( m_pCurrentUIFontPackage );

			while( m_pDirIterator->BNextFile() ) 
			{
				if ( !m_pDirIterator->BCurrentIsDir() )
				{
					if( V_stristr( m_pDirIterator->CurrentFileName(), ".ttf" ) != NULL || V_stristr( m_pDirIterator->CurrentFileName(), ".otf" ) != NULL )
					{
						CUtlString strFullPath = m_strFontPath;
						strFullPath += m_pDirIterator->CurrentFileName();
						pFontIdentifier->symFontFileOrPackage = strFullPath.String();

						hr = factory_->CreateCustomFontFileReference(
							&iIdentifier,
							sizeof( iIdentifier ),
							UIFontFileLoader::GetLoader(),
							&currentFile_
						);

						if ( SUCCEEDED( hr ) )
						{
							Msg( "CreateCustomFontfileReference succeeded\n" );
							*hasCurrentFile = TRUE;
							return S_OK;
						}
						else
						{
							Msg( "CreateCustomFontFileReference failed: 0x%X\n", hr );
						}
					}
					else if ( V_stristr( m_pDirIterator->CurrentFileName(), ".uifont" ) != NULL )
					{
						CUtlString strFullPath = m_strFontPath;
						strFullPath += m_pDirIterator->CurrentFileName();
						m_pCurrentUIFontPackage = new CUIFontPackage( strFullPath );
						iNextPackageFileIndex = m_pCurrentUIFontPackage->GetNextFileIndex();
						break;
					}
				}
			}
		}

		if ( m_pCurrentUIFontPackage )
		{
			if ( iNextPackageFileIndex != m_pCurrentUIFontPackage->InvalidFileIndex() )
			{
				pFontIdentifier->iFontPackageIndex = iNextPackageFileIndex;
				pFontIdentifier->symFontFileOrPackage = m_pCurrentUIFontPackage->GetPackageFileFullPath();
				
				hr = factory_->CreateCustomFontFileReference(
						&iIdentifier,
						sizeof( iIdentifier ),
						UIFontFileLoader::GetLoader(),
						&currentFile_
						);

				if ( SUCCEEDED( hr ) )
				{
					*hasCurrentFile = TRUE;
					return S_OK;
				}
				else
				{
					Msg( "CreateCustomFontFileReference failed: 0x%X\n", hr );
				}
			}
			else
			{
				SAFE_DELETE( m_pCurrentUIFontPackage );
			}
		}
		else
		{
			break;
		}
	}

	return hr;
}


//-----------------------------------------------------------------------------
// Purpose: GetCurrentFontFile
//-----------------------------------------------------------------------------
HRESULT STDMETHODCALLTYPE UIFontFileEnumerator::GetCurrentFontFile( OUT IDWriteFontFile** fontFile )
{
	*fontFile = currentFile_;
	if ( currentFile_ )
	{
		currentFile_->AddRef();
		return S_OK;
	}

	return E_FAIL;
}