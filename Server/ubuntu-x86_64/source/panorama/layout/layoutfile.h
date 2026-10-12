//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef LAYOUTPARSER_H
#define LAYOUTPARSER_H

#ifdef _WIN32
#pragma once
#endif

#include "utlsymbol.h"
#include "stylefile.h"
#include "tier1/utlptr.h"
#include "tier1/checksum_sha1.h"
#if !defined( SOURCE2_PANORAMA )
#include "steamcommon.h"
#endif
#include "panorama/uievent.h"
#include "tier1/smartptr.h"

namespace panorama
{

class CLayoutFileXMLParser;
class CLoadLayoutFileAsyncJob;
class CLayoutFile;
class CJSFile;

typedef CSmartPtr< CLayoutFile > LayoutFilePtr_t;
typedef CSmartPtr< CJSFile > JSFilePtr_t;

//-----------------------------------------------------------------------------
// Purpose: Types of files in include blocks in layout files
//-----------------------------------------------------------------------------
enum ELayoutIncludeFileType
{
	k_ELayoutIncludeFileTypeInvalid = 0,
	k_ELayoutIncludeFileTypeCSS = 1,
	k_ELayoutIncludeFileTypeJS = 2
};


//-----------------------------------------------------------------------------
// Purpose: Parsed description of a panel from a layout file
//-----------------------------------------------------------------------------
struct PanelDescription_t
{
	PanelDescription_t() {}
	~PanelDescription_t()
	{
		FOR_EACH_VEC( m_vecChildren, i )
		{
			delete m_vecChildren[i];
		}
	}

#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, const tchar *pchName )
	{
		VALIDATE_SCOPE();
		ValidateObj( m_strID );
		ValidateObj( m_vecChildren );
		FOR_EACH_VEC( m_vecChildren, i )
		{
			ValidatePtr( m_vecChildren[i] );
		}
		ValidateObj( m_mapProperties );
		FOR_EACH_MAP_FAST( m_mapProperties, i )
		{
			ValidateObj( m_mapProperties[i] );
		}
	}
#endif

	CPanoramaSymbol m_symType;
	CUtlString m_strID;
	CUtlMap< CPanoramaSymbol, CUtlString, short, CDefLess< CPanoramaSymbol > > m_mapProperties;
	CUtlVector< PanelDescription_t* > m_vecChildren;
};


//-----------------------------------------------------------------------------
// Purpose: Externally loaded JS File
//-----------------------------------------------------------------------------
class CJSFile: public CRefCount
{
public:
	CJSFile() { m_Buffer.SetBufferType( true, true ); }
	~CJSFile() {}

	void LoadFromBuffer( CPanoramaSymbol symPath, const CUtlBuffer &buffer );
	bool LoadFromFile( CPanoramaSymbol symPath );
	const char *GetBuffer() const;
	CPanoramaSymbol GetPathSymbol() const { return m_symPath; }

	void SetOriginalPath( const char *pchPath ) { m_strOriginalPath = pchPath; }
	const char *GetOriginalPath() { return m_strOriginalPath; }

	bool BMultipleReferences() const { return (GetRefCount() > 1); }
	int GetRefCount() const { return CRefCount::GetRefCount(); }
	
#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, const tchar *pchName );
#endif

private:
	CUtlBuffer m_Buffer;
	CPanoramaSymbol m_symPath;
	CUtlString m_strOriginalPath;				// could have had version param stripped; this needs to be re-evaluated with when we reload JS in dev
};


//-----------------------------------------------------------------------------
// Purpose: Script in the context of a layout (either inline or external)
//-----------------------------------------------------------------------------
struct JSInclude_t
{
	CUtlString strFilename;
	CUtlString strFilenameResolved;

	CUtlString m_strInlineContents; // valid for inline javascript
	JSFilePtr_t m_pJSFile; // valid for external JS files

	int nLine;
	int nCol;
	
	const char *GetScriptContents() const
	{
		if ( m_pJSFile.Get() )
		{
			return m_pJSFile->GetBuffer();
		}
		else
		{
			return m_strInlineContents.Get();
		}
	}
};


//-----------------------------------------------------------------------------
// Purpose: Layout file contents
//-----------------------------------------------------------------------------
class CLayoutFile : public CRefCount, public IUILayoutFile
{
public:
	CLayoutFile();
	virtual ~CLayoutFile();

	virtual bool BReplaceDefines( char *rgchBuffer, uint cubBuff, uint unFileOrder ) OVERRIDE;

	// can be used instead of ApplyPanelStyles when you need more control over applying styles (for example, label uses for 'fake' panels)
	virtual void BuildMatchingStyleList( CUtlVector< CascadeStyleFileInfo_t > &vecStyles, const CPanelIdentifiers &panelID, IUILayoutFile *pPreviousLayoutFile ) OVERRIDE { m_styleFileSet.BuildMatchingStyleList( this, vecStyles, panelID, (CLayoutFile*)pPreviousLayoutFile ); }

	virtual void GetStyleFileSymbols( StyleFileIndex_t iFileIndex, CUtlVector< CPanoramaSymbol > &vecStyleFileSymbols ) const OVERRIDE;
	virtual CPanoramaSymbol GetLayoutFileSymbol() const OVERRIDE{ return m_symLayoutFile; }

	virtual const CStyleAnimation *GetAnimation( CPanoramaSymbol symName ) OVERRIDE { return m_styleFileSet.GetAnimation( symName ); }

	virtual const char *GetDefine( const char *pchName ) OVERRIDE { return m_styleFileSet.GetDefine( pchName ); }

	ELoadLayoutFileResult LoadFromFile( const char *pchFile, bool bIsPartial );
	bool BLoadFromBuffer( CPanoramaSymbol symPath, CUtlBuffer &buffer, bool bIsPartial );
	ELoadLayoutFileResult Reload();
	bool BReloadFromBuffer( CUtlBuffer &buffer );

	bool BIsPartialLayout() { return m_bIsPartial; }
	
	bool BContainsStyleFile( CPanoramaSymbol symPath ) const { return m_styleFileSet.BContainsStyleFile( symPath ); }

	PanelDescription_t *GetPanelDescription() { return m_pPanelDescription; }
	PanelDescription_t *GetSnippet( const char *pchSnippetName ) { return m_mapSnippets.FindElement( pchSnippetName, nullptr ); }

	const CUtlVector< JSInclude_t > &GetLayoutFileScriptIncludes() { return m_vecJavscriptIncludes; }

	// can be used instead of ApplyPanelStyles when you need more control over applying styles (for example, label uses for 'fake' panels)
	virtual bool ApplyMatchedStylesToPanelStyle( IUIPanelStyle *pPanelStyle, const CUtlVector< CascadeStyleFileInfo_t > &vecStyles, EStyleRepaint &eRepaint, bool &bInheritablePropertiesChanged ) OVERRIDE { return CStyleFileSet::ApplyMatchedStylesToPanelStyle( (CPanelStyle*)pPanelStyle, vecStyles, eRepaint, bInheritablePropertiesChanged ); }
	bool BParseStyleTag( CUtlBuffer &buf, StylePropertyHash_t *pstyleProperties );
	
	int GetReloadCount() const { return m_nReloadCount; }
	bool BMultipleReferences() const { return (GetRefCount() > 1); }
	int GetRefCount() const { return CRefCount::GetRefCount(); }
	
#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, const tchar *pchName );
#endif

	// Checks if any of the style files loaded have a "descendant selector" matching the panel for the given style flag
	// Used to filter which panel subtree to invalidate when adding / removing style flags
	bool BHasAnyDescendantSelectorMatchingStyleFlag( EStyleFlags eStyleFlag, int nOldPanelFlags, int nNewPanelFlags, const IUIPanel &panel, CUtlVector< CPanoramaSymbol > &vecVisitedStyleFiles ) const { return m_styleFileSet.BHasAnyDescendantSelectorMatchingStyleFlag( eStyleFlag, nOldPanelFlags, nNewPanelFlags, panel, vecVisitedStyleFiles ); }
	// Checks if any of the style files loaded have a "descendant selector" matching the panel for the given classes
	// Used to filter which panel subtree to invalidate when adding / removing classes
	bool BHasAnyDescendantSelectorMatchingClasses( const CUtlPtrArray< CPanoramaSymbol > &arrChangedClasses, const CUtlPtrArray< CPanoramaSymbol > &arrOldClasses, const CUtlPtrArray< CPanoramaSymbol > &arrNewClasses, const IUIPanel &panel, CUtlVector< CPanoramaSymbol > &vecVisitedStyleFiles ) const { return m_styleFileSet.BHasAnyDescendantSelectorMatchingClasses( arrChangedClasses, arrOldClasses, arrNewClasses, panel, vecVisitedStyleFiles ); }

protected:
	friend class CLayoutFileXMLParser;
	
	bool BAddStyle( const char *pchPath );
	bool BAddJavaScript( const char *pchPath );
	void AddInlineJavascript( const char *pchScript, int nLine, int nCol );
	void SetPanelDescription( PanelDescription_t *pPanelDescription );
	void ClearPanelDescription();
	void AddSnippet( const char *pszSnippetName, PanelDescription_t *pPanelDescription );
	void ClearSnippets();

private:
	bool m_bIsPartial;
	CPanoramaSymbol m_symLayoutFile;
	CStyleFileSet m_styleFileSet;
	PanelDescription_t *m_pPanelDescription;
	int m_nReloadCount;

	CUtlVector<JSInclude_t> m_vecJavscriptIncludes;
	CUtlMap< CUtlString, PanelDescription_t * > m_mapSnippets;
};

class CLoadLayoutFileAsync;
class CReloadStyleFile;

//-----------------------------------------------------------------------------
// Purpose: Manages layout files, styles, etc.
//-----------------------------------------------------------------------------
class CLayoutManager : public IUILayoutManager
{
public:
	CLayoutManager();
	~CLayoutManager();

	// raw access. Need to be careful with pointer scope
	virtual IUILayoutFile *GetLayoutFile( const char *pchFile, bool bPartialLayout ) OVERRIDE;
	virtual IUILayoutFile *GetLayoutFile( CPanoramaSymbol symPath ) OVERRIDE;

	virtual void SaveInMemoryFiles() OVERRIDE;
	virtual void RevertInMemoryFiles() OVERRIDE;
	virtual bool BHasFilesInMemory() const OVERRIDE { return (m_mapInMemoryFiles.Count() > 0); }
	virtual bool LoadStyleIntoBuffer( CPanoramaSymbol symFile, CUtlBuffer &buffer ) OVERRIDE;
	virtual bool UpdateStyleInMemory( EUpdateStyleType eUpdateType, CPanoramaSymbol symStyleFile, uint unLocation, const char *pchUpdatedStyle ) OVERRIDE;
	virtual bool BConvertHTTPPathToLocalP4Path( const char *pchFile, CUtlString &strOut ) OVERRIDE;
	bool BParsePartialLayout( PanelDescription_t **ppPanelDescription, const char *pchXML );


	LayoutFilePtr_t GetLayoutFileFromString( const char *pchXMLString, bool bPartialLayout );
	void GetLayoutFileAsync( const char *pchFile, IUIPanel *pNotifyPanel, bool bPartialLayout );
	void GetLayoutFileFromStringAsync( const char *pchXMLString, IUIPanel *pNotifyPanel, bool bPartialLayout );
	LayoutFilePtr_t GetCLayoutFile( const char *pchFile, bool bPartialLayout );
	LayoutFilePtr_t GetCLayoutFile( CPanoramaSymbol symPath );
	StyleFilePtr_t GetStyleFile( const char *pchFile, const CStyleFileSet &existingFileSet, uint &unFileOrder, bool bUseCache );
	JSFilePtr_t GetJavaScriptFile( const char *pchFile, CUtlString *pOutResolvedPath );

	bool BNormalizeStyleFilePath( const char *pchFile, CPanoramaSymbol &symPath, CFileResource *pFileResource = nullptr, CUtlString *pstrPath = nullptr ) const;

	void ClearFileCache();
	void PrintCacheStatus();

	CUtlString StripVersionGetParamFromURL( const char *pchURL ) const;

	void ReloadChangedFile( const char *pchFile );

#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, const tchar *pchName );
#endif

private:
	friend class CLoadLayoutFileAsync;
	friend class CReloadStyleFile;

	void TrackAsyncLoad( CLoadLayoutFileAsync *pLoader );
	void ClearAsyncLoad( CLoadLayoutFileAsync *pLoader );

	void TrackAsyncReload( CReloadStyleFile *pLoader );
	void ClearAsyncReload( CReloadStyleFile *pLoader );

	bool AutoReloadFailedFileLoads();
	void ReloadLayoutFile( CPanoramaSymbol symPath );
	void ReloadStyleFile( CPanoramaSymbol symPath );
	void ReloadScriptFile( CPanoramaSymbol symPath );

	// callback used internally by loader job for async loads
	void OnLayoutFileBufferLoaded( CUtlBuffer &bufFile, CPanoramaSymbol symName, const char *pchLocalP4Path, IUIPanel *pNotifyPanel, bool bRemoteCSSAllLoaded, bool bWasFileReload, ELoadLayoutAsyncDetails eDetails, bool bIsPartialLayout );
	
	// callback used internally by loader job as it pre-caches styles files
	void OnLoadRemoteCSSFileFromBuffer( CUtlBuffer &bufFile, const CUtlString &filePath, bool bReloadStylesForFile );
	void OnLoadRemoteJSFileFromBuffer( CUtlBuffer &bufFile, const CUtlString &filePath, bool bReloadStylesForFile );

	// check if CSS for file/http path is already cached
	bool BIsIncludeBufferCached( ELayoutIncludeFileType eType, const char *pchPath );
	void AddToLocalP4Paths( const CUtlString &filePath, CPanoramaSymbol symPath );
	void InsertLocalToHTTP( const CUtlString &filePath, CPanoramaSymbol symPath );

	CUtlMap< CPanoramaSymbol, CUtlBuffer *, int, CDefLess< CPanoramaSymbol > > m_mapCachedStyleBuffers;
	CUtlMap< CPanoramaSymbol, JSFilePtr_t, int, CDefLess< CPanoramaSymbol > > m_mapJSFiles;
	CUtlMap< CPanoramaSymbol, LayoutFilePtr_t, int, CDefLess< CPanoramaSymbol > > m_mapLayoutFiles;
	CUtlMap< CUtlKeyPtr< CStyleFileKey >, StyleFilePtr_t, int, CDefLess< CUtlKeyPtr< CStyleFileKey > > > m_mapStyleFiles;
	CUtlRBTree< CPanoramaSymbol, int, CDefLess< CPanoramaSymbol > > m_treeKnownStyleFiles;				// helpful when reloading to know if the changed file is a style before doing an expensive search

	CUtlMap< CUtlString, CPanoramaSymbol, int, CDefLess< CUtlString  > > m_mapLocalP4PathsToHTTPPaths;

	struct InMemoryFile_t
	{
		SHADigest_t m_digest;
		CUtlBuffer *m_pBuffer;
	};
	CUtlMap< CPanoramaSymbol, InMemoryFile_t, int, CDefLess< CPanoramaSymbol > > m_mapInMemoryFiles;		// files which are currently being edited in memory

	CUtlVector< CPanoramaSymbol > m_vecFilesFailedRead;												// list of files that we tried to reload, and failed (probably UltraEdit locking the file). Will retry.

	CUtlRBTree< CLoadLayoutFileAsync *, int, CDefLess< CLoadLayoutFileAsync * > > m_treeInFlightAsyncLoads;
	CUtlRBTree< CReloadStyleFile *, int, CDefLess< CReloadStyleFile * > > m_treeInFlightAsyncReloads;
};

// access the global layout manager
CLayoutManager &LayoutManager();

} // namespace panorama

#endif //LAYOUTPARSER_H