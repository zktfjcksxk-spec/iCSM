//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef STYLEFILE_H
#define STYLEFILE_H

#ifdef _WIN32
#pragma once
#endif

#include "utlstring.h"
#include "utlsymbol.h"
#include "utlmap.h"
#include "utldict.h"
#include "utlsortvector.h"
#include "tier1/utlptrarray.h"
#include "../renderer/styles.h"
#include "uipanel.h"
#include "tier1/refcount.h"
#include "tier1/utlhashtable.h"

class CUtlBuffer;

namespace panorama
{

class CStyleFile;
class IUIPanel;

typedef CSmartPtr< CStyleFile > StyleFilePtr_t;


//-----------------------------------------------------------------------------
// Purpose: Returned when trying to load a layout or style file
//-----------------------------------------------------------------------------
enum ELoadLayoutFileResult
{
	k_ELoadLayoutFileOK,
	k_ELoadLayoutFileFailed,
	k_ELoadLayoutFileReadFailed
};


//-----------------------------------------------------------------------------
// Purpose: Helper to log parsing errors
//-----------------------------------------------------------------------------
void LogLayoutParsingError( CPanoramaSymbol symPath, uint32 unLineNumber, PRINTF_FORMAT_STRING const char *pchMsg, ... );
uint32 CalcBufferLine( CUtlBuffer &buffer );


//-----------------------------------------------------------------------------
// Purpose: Descendant selector filtering keeping track of ancestors when applying styles
//			Used by CStyleFile::AddMatchingStylesCore to quickly reject selectors
//			when the panel tested doesn't have the matching ancestors.
//			Filtering done using a counting bloom filter. It is possible to get
//			false positives but not false negatives
//-----------------------------------------------------------------------------
class CStyleFileDescendantFilter
{
public:

	static void SetActive( bool bActive );
	static bool BIsActive();

	// Functions to keep track of all ancestors of a panel
	static void Push( const IUIPanel &parentPanel );
	static void Pop( const IUIPanel &parentPanel );

	// Check whether ancestors of the panel (added via Push() / Pop())
	// might be a match for the given css selector
	static bool BMayContain( const StyleFromFile_t *pStyleFromFile );

private:

	// Counting bloom filter with 2 hash functions, 4096 buckets and 8 bit counters
	// As described in https://en.wikipedia.org/wiki/Bloom_filter#Counting_filters
	// False positive rate of 0.2% with 100 strings in the filter
	struct CountingBloomFilter_t
	{
	public:

		CountingBloomFilter_t() { Clear(); }

		// Clear the filter. Make sure filter to clear the filter before reusing it
		// as just removing all hashes doesn't guarantee to clear it in case of 
		// overflow (8 bit counter)
		void Clear();
		
		void Add( uint32 hash );
		// Note that in case of an overflow, a bucket can "stick" in the table until 
		// Clear() is called and we don't know wheter we incremented the bucket count
		// when we added the hash
		void Remove( uint32 hash );

		// Check whether the given hash is in the filter. Can return true even if the 
		// hash is not in the filter but will never return false for hashes that are
		// actually in the filter
		bool BMayContain( uint32 hash ) const { return FirstBucket( hash ) && SecondBucket( hash ); }

	private:

		enum
		{
			k_EKeyBits = 12,
			k_EKeyShift = 16,
			k_ENumBuckets = 1 << k_EKeyBits,
			k_EKeyMask = ( 1 << k_EKeyBits ) - 1,
		};

		uint8 &FirstBucket( uint32 hash ) { return m_buckets[ hash & k_EKeyMask ]; }
		uint8 &SecondBucket( uint32 hash ) { return m_buckets[ ( hash >> k_EKeyShift ) & k_EKeyMask ]; }
		const uint8 &FirstBucket( uint32 hash ) const { return m_buckets[ hash & k_EKeyMask ]; }
		const uint8 &SecondBucket( uint32 hash ) const { return m_buckets[ ( hash >> k_EKeyShift ) & k_EKeyMask ]; }

		uint8 m_buckets[ k_ENumBuckets ];
	};

	static bool m_bActive;
	static CountingBloomFilter_t m_filter;
};


//-----------------------------------------------------------------------------
// Purpose: Encapsulates a set of style files and looking up data within them
//-----------------------------------------------------------------------------
class CStyleFileSet
{
public:
	void Clear();
	void MergeTo( CStyleFileSet *pStyleFileSet ) const;
	void AddStyleFile( StyleFilePtr_t pStyleFile );

	const char *GetDefine( const char *pchDefine ) const;
	const CStyleAnimation *GetAnimation( CPanoramaSymbol symKeyFrame ) const;

	// can be used instead of ApplyPanelStyles when you need more control over applying styles (for example, label uses for 'fake' panels)
	void BuildMatchingStyleList( CLayoutFile *pLayoutFile, CUtlVector< CascadeStyleFileInfo_t > &vecStyles, const CPanelIdentifiers &panelID, IUILayoutFile *pPreviousLayoutFile );
	static bool ApplyMatchedStylesToPanelStyle( CPanelStyle *pPanelStyle, const CUtlVector< CascadeStyleFileInfo_t > &vecStyles, EStyleRepaint &eRepaint, bool &bInheritablePropertiesChanged );	

	// returns vector of style files, in order of first included to last
	const CUtlVector< StyleFilePtr_t > &GetStyleFiles() const { return m_vecStyleFiles; }
	bool BContainsStyleFile( CPanoramaSymbol symPath ) const;

	static bool BSelectorMatchesPanel( const StyleFromFile_t *pStyleFromFile, const CPanelIdentifiers &panelID );

	// Checks if any of the style files loaded have a "descendant selector" with the given style flag
	// and matching the panel (id, type, classes, ...)
	// Used to filter which panel subtree to invalidate when adding / removing style flags
	bool BHasAnyDescendantSelectorMatchingStyleFlag( EStyleFlags eStyleFlag, int nOldPanelFlags, int nNewPanelFlags, const IUIPanel &panel,CUtlVector< CPanoramaSymbol > &vecVisitedStyleFiles ) const;
	// Checks if any of the style files loaded have a "descendant selector" with the given classes
	// and matching the panel(id, type, classes, ...)
	// Used to filter which panel subtree to invalidate when adding / removing classes
	bool BHasAnyDescendantSelectorMatchingClasses( const CUtlPtrArray< CPanoramaSymbol > &arrChangedClasses, const CUtlPtrArray< CPanoramaSymbol > &arrOldClasses, const CUtlPtrArray< CPanoramaSymbol > &arrNewClasses, const IUIPanel &panel, CUtlVector< CPanoramaSymbol > &vecVisitedStyleFiles ) const;

#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, const tchar *pchName );
#endif

private:
	CUtlVector< StyleFilePtr_t > m_vecStyleFiles;
};


//-----------------------------------------------------------------------------
// Purpose: Identifies a style, which includes the styles that were included when the style was created.
//			Previous styles & order is important as defines are inherited from previously loaded styles
//-----------------------------------------------------------------------------
class CStyleFileKey
{
public:
	CStyleFileKey() {}
	CStyleFileKey( const CStyleFileSet &styleSet, CPanoramaSymbol symPath );
	~CStyleFileKey() { Clear(); }

	void Init( const CStyleFileSet &styleSet, CPanoramaSymbol symPath );
	void Clear();
	bool BContainsStyleFile( CPanoramaSymbol symPath ) const;
	int Length() { return m_pKey.Count(); }
	CPanoramaSymbol GetFileSymbol( int i ) { return m_pKey.Element( i ); }
	void Append( CPanoramaSymbol symPath );

	bool operator<( const CStyleFileKey &rhs ) const;
	bool operator==( const CStyleFileKey &rhs ) const;	

#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, const tchar *pchName );
#endif


private:
	CStyleFileKey( const CStyleFileKey & );
	CStyleFileKey &operator=( const CStyleFileKey &rhs );

	CUtlPtrArray< CPanoramaSymbol > m_pKey;
};


//-----------------------------------------------------------------------------
// Purpose: Style parsing helpers
//-----------------------------------------------------------------------------
bool BParseStyleProperty( CUtlBuffer &buffer, StylePropertyHash_t *pProperties, CPanoramaSymbol symPathForErrors, const CStyleFile *pCurrentStyleFile, const CUtlVector< StyleFilePtr_t > &vecPrevStyleFiles, bool bNoBrackets );
bool BParseStyleBody( CUtlBuffer &buffer, StylePropertyHash_t *pProperties, CPanoramaSymbol symPathForErrors, const CStyleFile *pCurrentStyleFile, const CUtlVector< StyleFilePtr_t > &vecPrevStyleFiles, bool bNoBrackets );
bool BReplaceDefines( char *rgchBuffer, uint cubBuffer, const CStyleFile *pCurrentStyleFile, const CUtlVector< StyleFilePtr_t > &vecPrevStyleFiles, uint unFileOrder );


//-----------------------------------------------------------------------------
// Purpose: Creates a style from a style text file
//-----------------------------------------------------------------------------
typedef StyleFromFile_t* StyleFromFilePointer;
class CStyleFile : public CRefCount
{
public:
	CStyleFile();
	~CStyleFile();

	ELoadLayoutFileResult LoadFromFile( const char *pchFile, const CUtlVector< StyleFilePtr_t > &vecPrevStyleFiles, uint &unFileOrder );
	bool BLoadFromBuffer( CUtlBuffer &buffer, CPanoramaSymbol pchStyleFilePath, const CUtlVector< StyleFilePtr_t > &vecPrevStyleFiles, uint &unFileOrder );

	ELoadLayoutFileResult Reload( const CUtlVector< StyleFilePtr_t > &vecPrevStyleFiles, uint &unFileOrder );
	bool BReloadLoadFromBuffer( CUtlBuffer &buffer, const CUtlVector< StyleFilePtr_t > &vecPrevStyleFiles, uint &unFileOrder );

	CPanoramaSymbol GetPathSymbol() const { return m_symStylePath; }
	const char *GetDefine( const char *pchName ) const;
	const CStyleAnimation *GetAnimation( CPanoramaSymbol symName ) const;

	void AddStylesMatchingClass( IUILayoutFile *pLayoutFile, const CPanelIdentifiers &panelID, CPanoramaSymbol symStyle, CUtlVector< CascadeStyleFileInfo_t > &vecStyles, uint iStyleIndex );
	void AddStylesMatchingPanelTypeNoClass( IUILayoutFile *pLayoutFile, const CPanelIdentifiers &panelID, CPanoramaSymbol symStyle, CUtlVector< CascadeStyleFileInfo_t > &vecStyles, uint iStyleIndex );
	void AddStylesMatchingJustID( IUILayoutFile *pLayoutFile, const CPanelIdentifiers &panelID, const char *pchID, CUtlVector< CascadeStyleFileInfo_t > &vecStyles, uint iStyleIndex );

	bool BMultipleReferences() const { return (GetRefCount() > 1); }
	int GetRefCount() const { return CRefCount::GetRefCount(); }

	bool BReplaceDefinedValues( char *rgchBuffer, uint cubBuffer, uint unFileOrder ) const;

	bool BImportsStyleFile( CPanoramaSymbol symPath ) const;

	int GetImportedStyleFileCount() const { return m_vecImportedStyleFiles.Count(); }
	StyleFilePtr_t GetImportedStyleFile( int i ) { return m_vecImportedStyleFiles[ i ].pStyleFile; }

	uint GetMaxFileOrder() const { return m_unMaxFileOrder; }

#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, const tchar *pchName );
#endif

	// Checks if style contains any "descendant selectors" with the given style flag
	// and matching the panel (id, type, classes, ...)
	// Used to filter which panel subtree to invalidate when adding / removing style flags
	bool BHasAnyDescendantSelectorMatchingStyleFlag( EStyleFlags eStyleFlag, int nOldPanelFlags, int nNewPanelFlags, const IUIPanel &panel, CUtlVector< CPanoramaSymbol > &vecVisitedStyleFiles ) const;
	// Checks if style contains any "descendant selector" with the given classes
	// and matching the panel(id, type, classes, ...)
	// Used to filter which panel subtree to invalidate when adding / removing classes
	bool BHasAnyDescendantSelectorMatchingClasses( const CUtlPtrArray< CPanoramaSymbol > &arrChangedClasses, const CUtlPtrArray< CPanoramaSymbol > &arrOldClasses, const CUtlPtrArray< CPanoramaSymbol > &arrNewClasses, const IUIPanel &panel, CUtlVector< CPanoramaSymbol > &vecVisitedStyleFiles ) const;

private:
	void Clear();

	const StyleFromFile_t *GetStylesForClass( CPanoramaSymbol symStyle );
	const StyleFromFile_t *GetStylesForPanelTypeNoClass( CPanoramaSymbol symStyle );
	const StyleFromFile_t *GetStylesForJustID( const char *pchID );

	void AddDefine( const char *pchName, const char *pchValue );
	bool BParseCSSAtRule( const char *pchToken, uint unSectionStart, CUtlBuffer &buffer, const CUtlVector< StyleFilePtr_t > &vecPrevStyleFiles );
	bool BParseCSSDefine( const char *pchToken, CUtlBuffer &buffer, const CUtlVector< StyleFilePtr_t > &vecPrevStyleFiles );
	bool BParseCSSKeyframes( const char *pchToken, uint unSectionStart, CUtlBuffer &buffer, const CUtlVector< StyleFilePtr_t > &vecPrevStyleFiles );
	bool BParseCSSImport( const char *pchToken, uint unSectionStart, CUtlBuffer &buffer, const CUtlVector< StyleFilePtr_t > &vecPrevStyleFiles );

	bool BParseCSSStyle( const char *pchSelector, uint unSectionStart, CUtlBuffer &buffer, const CUtlVector< StyleFilePtr_t > &vecPrevStyleFiles );
	bool BParseSelector( const char *pchSelector, CUtlBuffer &buffer, StyleFromFile_t *pStyleFromFile );
	bool BCreateStylesForSelectors( CUtlVector< StyleFromFile_t *> *pvecStyles, const char *pchSelectors, CUtlBuffer &buffer );

	template <typename TValue, typename GetStylesFunction >
	void AddMatchingStylesCore( IUILayoutFile *pLayoutFile, const CPanelIdentifiers &panelID, TValue value, CUtlVector< CascadeStyleFileInfo_t > &vecStyles, StyleFileIndex_t iCurrentFileIndex, uint iFileImportDepth, GetStylesFunction getStyles );

	const CStyleDefine *GetStyleDefine( const char *pchName ) const;

	void CollectDescendantSelectors( const CUtlVector< StyleFromFile_t * > &vecSelectors );
	bool BHasAnyDescendantSelectorMatchingStyleFlag( EStyleFlags eStyleFlag, int nOldPanelFlags, int nNewPanelFlags, CPanoramaSymbol symPanelType, const CPanoramaSymbol *psymClasses, uint csymClasses, const char *pchID, CUtlVector< CPanoramaSymbol > &vecVisitedStyleFiles ) const;
	

	CPanoramaSymbol m_symStylePath;
	CUtlDict< CStyleDefine, short > m_dictDefines;
	CUtlMap< CPanoramaSymbol, CStyleAnimation *, short, CDefLess< CPanoramaSymbol > > m_mapAnimations;

	// keep a map of selector to parsed styles. Because a selector can be defined twice in a file, with styles in the middle, we need to
	// handle the same selector being defined multiple times.
	// Each selector will be added to one of the hash table below:
	//   * m_classStyles - selectors with classes (may have type, may have id). First class used as a key
	//	 * m_typeStyles - selectors with type and no classes (may have id). Type used as a key
	//	 * m_idStyles - selectors with id and no type and no classes. Id used as a key
	// Hash tables used to collect all matching styles corresponding to a given panel
	CUtlHashtable< CPanoramaSymbol, StyleFromFilePointer, Mix32HashFunctor > m_classStyles;
	CUtlHashtable< CPanoramaSymbol, StyleFromFilePointer, Mix32HashFunctor > m_typeStyles;
	CUtlHashtable< const char*, StyleFromFilePointer > m_idStyles;

	typedef CUtlVector< CStyleSelector* > VecStyleSelectorPtr_t;
	// List of all descendant CSS selectors containing a "pseudo class" (such as hover, focus, ...) sorted by "pseudo class" (EStyleFlags)
	// ie one list per "pseudo class"
	// Use to filter which panel subtree to invalidate when adding / removing style flags
	VecStyleSelectorPtr_t m_arrDescendantSelectorsMatchingStyleFlag[ k_EStyleFlagMaxBit ];
	// List of all descendant CSS selectors that have class selectors, hashed by class name
	// Use to filter which panel subtree to invalidate when adding / removing classes
	CUtlHashMap< CPanoramaSymbol, VecStyleSelectorPtr_t* > m_mapDescendantSelectorsMatchingClass;


	struct ImportedStyleFile_t
	{
		StyleFilePtr_t pStyleFile;
		uint unImportedFileLocation;
	};
	CUtlVector< ImportedStyleFile_t > m_vecImportedStyleFiles;

	uint m_unMaxFileOrder;
};

} // namespace panorama

#endif //STYLEFILE_H