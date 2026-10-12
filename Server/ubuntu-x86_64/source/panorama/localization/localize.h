//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef LOCALIZE_H
#define LOCALIZE_H
#pragma once

#if defined( SOURCE2_PANORAMA )
#include "tier1/utlcommon.h"
#include "currencyamount.h"
#include "language.h"
#else
#include "amount.h"
#endif
#include "panorama/iuipanel.h"
#include "panorama/controls/panelptr.h"
#include "panorama/localization/ilocalize.h"
#include "tier1/utlblockmemory.h"

namespace panorama
{

class CLocalization;
class CPanel2D;


//-----------------------------------------------------------------------------
// Purpose: wraps the variable name and type a loc string contains
//-----------------------------------------------------------------------------
struct DialogVariable_t
{
	DialogVariable_t() 
	{ 
		m_fModifiers = 0; 
		m_eType = k_ePanelVartype_None; 
	}

	DialogVariable_t( EPanelKeyType eType, const char *pchVariableName ) 
	{ 
		m_eType = eType;
		m_sVariableName = pchVariableName; 
		m_fModifiers = 0;
	}

	bool operator==( const DialogVariable_t &src ) const
	{
		return m_eType == src.m_eType && m_fModifiers == src.m_fModifiers &&
			m_sVariableName == src.m_sVariableName && m_sVariableParams == src.m_sVariableParams;
	}

	CUtlString m_sVariableName;
	EPanelKeyType m_eType;
	uint32 m_fModifiers;
	CUtlString m_sVariableParams; // used by the generic type
};


//-----------------------------------------------------------------------------
// Purpose: wraps the key, type and value a panel has set for a dialog variable
//-----------------------------------------------------------------------------
class CPanelKeyValue
{
public:
	CPanelKeyValue() 
	{ 
		m_eType = k_ePanelVartype_None; 
	}

	~CPanelKeyValue() {}

	explicit CPanelKeyValue( const char *pchKey )
	{
		m_eType = k_ePanelVartype_None;
		m_symKey = pchKey;
		m_sValue = 0;
	}

	explicit CPanelKeyValue( const char *pchKey, const char *pchValue ) 
	{ 
		m_eType = k_ePanelVartype_String;
		m_symKey = pchKey;
		m_sValue = pchValue; 
	}

	explicit CPanelKeyValue( const char *pchKey, int nVal ) 
	{ 
		m_eType = k_ePanelVartype_Number;
		m_symKey = pchKey;
		m_number = nVal; 
	}

	explicit CPanelKeyValue( const char *pchKey, uint64 nVal )
	{
		m_eType = k_ePanelVartype_Uint64;
		m_symKey = pchKey;
		m_number64 = nVal;
	}

	explicit CPanelKeyValue( const char *pchKey, time_t timeVal ) 
	{
		m_eType = k_ePanelVartype_Time; 
		m_symKey = pchKey;
		m_time = timeVal; 
	}

#if defined( SOURCE2_PANORAMA )
	explicit CPanelKeyValue( const char *pchKey, CCurrencyAmount amount ) 
#else
	explicit CPanelKeyValue( const char *pchKey, CAmount amount ) 
#endif
	{
		m_eType = k_ePanelVartype_Money; 
		m_symKey = pchKey; 
		Amount() = amount; 
	}

	bool operator==( const CPanelKeyValue &src ) const
	{
		if ( m_eType != src.m_eType )
			return false;
		if ( m_symKey != src.m_symKey )
			return false;

		// values are ignored, we are the same if type and token equal
		return true;
	
	}

	bool operator!=( const CPanelKeyValue &src ) const
	{
		return !operator==( src );
	}

	bool BCompareValues( const CPanelKeyValue &src  ) const 
	{
		switch( m_eType )
		{
		case k_ePanelVartype_String:
			return m_sValue == src.m_sValue;
		case k_ePanelVartype_Time:
			return m_time == src.m_time;
		case k_ePanelVartype_Money:
			return Amount() == src.Amount();
		case k_ePanelVartype_Number:
			return m_number == src.m_number;
		case k_ePanelVartype_Uint64:
			return m_number64 == src.m_number64;
		case k_ePanelVartype_Generic:
			return m_number == src.m_number && m_sValue == src.m_sValue; // generic potentially uses both number and value
		case k_ePanelVartype_Virtual:
			return true; // virtual vars don't have values
		default:
			break;
		}
		return false;
	}

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName )
	{
		VALIDATE_SCOPE();
		ValidateObj( m_symKey );
		ValidateObj( m_sValue );
	}
#endif

#if defined( SOURCE2_PANORAMA )
	const CCurrencyAmount& Amount() const
	{
		Assert( m_eType == k_ePanelVartype_Money );
		return *(CCurrencyAmount*)m_currencyBuf;
	}
	CCurrencyAmount& Amount()
	{
		Assert( m_eType == k_ePanelVartype_Money );
		return *( CCurrencyAmount* )m_currencyBuf;
	}
#else
	const CAmount& Amount() const
	{
		Assert( m_eType == k_ePanelVartype_Money );
		return *( CAmount* )m_currencyBuf;
	}
	CAmount& Amount()
	{
		Assert( m_eType == k_ePanelVartype_Money );
		return *( CAmount* )m_currencyBuf;
	}
#endif

protected:
	friend class CLocalization;
	friend class CLocalizationStringDialogVariables;

	EPanelKeyType m_eType;
	CUtlString m_symKey;
	
	union
	{
		int	m_number;
		uint64 m_number64;
		time_t m_time;
#if defined( SOURCE2_PANORAMA )
		uint8 m_currencyBuf[sizeof( CCurrencyAmount )];
#else
		uint8 m_currencyBuf[sizeof( CAmount )];
#endif
	};

	// Can't put in union since Generic uses number and string
	CUtlString m_sValue;
};


//-----------------------------------------------------------------------------
// Purpose: wrap a simple loc variable with no dialog vars (common case)
//-----------------------------------------------------------------------------
class CLocalizationStringSimple : public ILocalizationString
{
public:
	CLocalizationStringSimple( const char *pchValue, const IUIPanel *pOwner, uint32 nMaxChars, EStringTruncationStyle eTruncationStyle, EStringTransformStyle eTransformStyle, EStringEscapeStyle eEscapeStyle, bool bStringAlreadyFullyParsed ) 
	{ 
		m_pStrData = NULL;
		m_pStrDataNoTransform = NULL;
		m_nMaxChars = nMaxChars;
		m_pOwner = pOwner; 
		m_eTruncationStyle = eTruncationStyle;
		m_eTransformStyle = eTransformStyle;
		m_eEscapeStyle = eEscapeStyle;
		m_bStringAlreadyFullyParsed = bStringAlreadyFullyParsed;
		DbgAssert( ThreadInMainThread() );
		if ( pchValue )
			Create( pchValue );
	}

	virtual ~CLocalizationStringSimple() 
	{
		SAFE_DELETE( m_pStrDataNoTransform );
	}

	// Get the length of the string in characters
	int Length() const 
	{
		if ( IsEmpty() )
		{
			return 0;
		}

		return V_UnicodeLength( *m_pStrData );
	}

	bool IsEmpty() const 
	{ 
		return m_pStrData == NULL || *m_pStrData == NULL || **m_pStrData == 0;
	}

	const char *String() const 
	{ 
		DbgAssert( ThreadInMainThread() );
		if ( IsEmpty() )
		{
			return "";
		}

		return *m_pStrData; 
	}

	const char *StringNoTransform() const
	{
		DbgAssert( ThreadInMainThread() );
		if ( m_pStrDataNoTransform )
			return m_pStrDataNoTransform->String();
		else
			return String();
	}

	operator const char *() const 
	{ 
		return String(); 
	}

	bool IsUsingContainedString() const
	{
		return m_pStrData == (char const **)&m_DerivedString;
	}
	bool HasNonContainedString() const
	{
		return m_pStrData && !IsUsingContainedString() && *m_pStrData;
	}
	void FreeStringIfNotContained()
	{
		if ( HasNonContainedString() )
		{
			MemAlloc_Free( (char*)*m_pStrData );
			m_pStrData = NULL;
		}
	}
	void SwitchToContainedString()
	{
		if ( !IsUsingContainedString() )
		{
			m_DerivedString = String();
			m_pStrData = (char const **)&m_DerivedString;
		}
	}

	bool AppendText( const char *pchText );
	void Release() const;

	virtual EStringTransformStyle GetTransformStyle() const { return m_eTransformStyle; }
	virtual const IUIPanel *GetOwningPanel() const { return m_pOwner.Get(); }
	virtual uint32 GetMaxChars() const { return m_nMaxChars; }
	virtual EStringTruncationStyle GetTruncationStyle() const { return m_eTruncationStyle; }
	virtual EStringEscapeStyle GetEscapeStyle() const { return m_eEscapeStyle; }

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName )
	{
		VALIDATE_SCOPE();
		ValidateObj( m_DerivedString );
		// m_pStrData is owned by the localization manager
		ValidatePtr( m_pStrDataNoTransform );
	}
#endif

protected:
	friend class CLocalization;

	void Create( const char *pString );
	virtual void Recalculate( char const **pStrData, int nStartCharIndex = 0 ) OVERRIDE;
	virtual bool BContainsDialogVariable( const CPanelKeyValue &key ) OVERRIDE { return false; }
	virtual bool BHasValidString( const void *pValidBase, const void *pValidLimit ) OVERRIDE;

private:
	char const **m_pStrData;
	const CUtlString *m_pStrDataNoTransform;
	CUtlString m_DerivedString;
	panorama::CPanelPtr<panorama::IUIPanel> m_pOwner;
	static CLocalization *m_pLocalizationManager;
	uint32 m_nMaxChars;
	EStringTruncationStyle m_eTruncationStyle;
	EStringTransformStyle m_eTransformStyle;
	EStringEscapeStyle m_eEscapeStyle;
	bool m_bStringAlreadyFullyParsed;
};


class CLocalizationStringDialogVariablesDerived;
//-----------------------------------------------------------------------------
// Purpose: wrap a loc string (from the loc file) and store off its parsed format
//-----------------------------------------------------------------------------
class CLocalizationStringDialogVariables 
{
public:
	CLocalizationStringDialogVariables( char const **pStrData, CCopyableUtlVector<DialogVariable_t> &vecDialogVars ) 
	{ 
		Set( pStrData, vecDialogVars ); 
	}
	virtual ~CLocalizationStringDialogVariables() {}
	
#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName )
	{
		VALIDATE_SCOPE();
		ValidateObj( m_vecDialogVariables );
		FOR_EACH_VEC( m_vecDialogVariables, i )
			ValidateObj( m_vecDialogVariables[i].m_sVariableName );
		// m_pStrData  is owned by the localization manager
	}
#endif

protected:
	friend class CLocalization;
	friend class CLocalizationStringDialogVariablesDerived;

	// dialog var handling
	static bool BParseDialogVariables( const char *pchBaseString, CCopyableUtlVector<DialogVariable_t> &vecDialogVars, bool bManagedLocString );
	const CUtlVector<DialogVariable_t> &GetDialogVariables() { return m_vecDialogVariables; }
	bool BContainsDialogVariable( const CPanelKeyValue &key );
	bool BHasValidString( const void *pValidBase, const void *pValidLimit );

	void Set( char const **pStrData, CCopyableUtlVector<DialogVariable_t> &vecDialogVars );

private:
	static const char *FindNextBrace( const char *pch );
	char const **m_pStrData;
	CCopyableUtlVector<DialogVariable_t> m_vecDialogVariables;
	EStringTransformStyle m_eTransformStyle;
};


//-----------------------------------------------------------------------------
// Purpose: calculated version of a loc string with the dialog vars resolved
//-----------------------------------------------------------------------------
class CLocalizationStringDialogVariablesDerived : public ILocalizationString
{
public:
	CLocalizationStringDialogVariablesDerived( CLocalizationStringDialogVariables *pParent, EStringTransformStyle eTransformStyle, EStringEscapeStyle eEscapeStyle, const IUIPanel *pOwner = NULL )
	{ 
		m_pOwner = pOwner;
		m_pParent = pParent;
		m_eTransformStyle = eTransformStyle;
		m_eEscapeStyle = eEscapeStyle;
	}

	virtual ~CLocalizationStringDialogVariablesDerived() {}

	// Get the length of the string in characters
	int Length() const 
	{ 
		return V_UnicodeLength( m_sDerivedString.String() );
	}

	bool IsEmpty() const 
	{ 
		return m_sDerivedString.IsEmpty(); 
	}

	const char *String() const 
	{ 
		return m_sDerivedString.String(); 
	}

	const char *StringNoTransform() const
	{
		AssertMsg( false, "StringNoTransform shouldn't be needed/used on CLocalizationStringDialogVariablesDerived" );
		return NULL;
	}
	
	operator const char *() const 
	{ 
		return String(); 
	}

	bool AppendText( const char *pchText ) { Assert( !"Not supported" ); return false; } 
	void Release() const;

	virtual EStringTransformStyle GetTransformStyle() const { return m_eTransformStyle; }
    virtual const IUIPanel *GetOwningPanel() const { return m_pOwner.Get(); }
	virtual uint32 GetMaxChars() const { return k_nLocalizeMaxChars; }
	virtual EStringTruncationStyle GetTruncationStyle() const { return k_eStringTruncationStyle_None; }
	virtual EStringEscapeStyle GetEscapeStyle() const { return m_eEscapeStyle; }

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName )
	{
		VALIDATE_SCOPE();
		ValidateObj( m_sDerivedString );
	}
#endif

protected:
	friend class CLocalization;
	virtual void Recalculate( char const **pStrData, int nStartCharIndex = 0 ) OVERRIDE;
	virtual bool BContainsDialogVariable( const CPanelKeyValue &key ) OVERRIDE { return m_pParent->BContainsDialogVariable( key ); }
	virtual bool BHasValidString( const void *pValidBase, const void *pValidLimit ) OVERRIDE;

private:
	panorama::CPanelPtr<panorama::IUIPanel> m_pOwner;
    CUtlString m_sDerivedString;
	CLocalizationStringDialogVariables *m_pParent;
	static CLocalization *m_pLocalizationManager;
	EStringTransformStyle m_eTransformStyle;
	EStringEscapeStyle m_eEscapeStyle;
};


//-----------------------------------------------------------------------------
// Purpose: wrap the logic to get language specific strings for the UI subsystem
//-----------------------------------------------------------------------------
class CLocalization : public IUILocalization
{
public:
	CLocalization();
	~CLocalization();

	// setup
    bool SetLanguage( const char *pchUserLanguage );
	void Shutdown();

	// get loc data 
	virtual const ILocalizationString *PchFindToken( const IUIPanel *pPanel, const char *pchToken, const uint32 ccMax, EStringTruncationStyle eTrunkStyle, EStringTransformStyle eTransformStyle, EStringEscapeStyle eEscapeStyle, bool bAllowDialogVariable = false, bool bReturnWrappedKeyIfMissing = true );
	virtual const ILocalizationString *PchSetString( const IUIPanel *pPanel, const char *pchText, const uint32 ccMax, EStringTruncationStyle eTrunkStyle, EStringTransformStyle eTransformStyle, EStringEscapeStyle eEscapeStyle, bool bAllowDialogVariable, bool bStringAlreadyFullyParsed );

	virtual const ILocalizationString *ChangeTransformStyleAndRelease( const ILocalizationString *pLocalizationString, EStringTransformStyle eTranformStyle );
	// add a loc file to the system, in the form of <prefix>_<language>.txt , i.e dota_french.txt
	virtual bool BLoadLocalizationFile( const char *pchFilePrefix );
	virtual ELanguage CurrentLanguage()
	{ 
		return PchLanguageToELanguage(m_sLanguage.String()); 
	}

	virtual void InstallCustomDialogVariableHandler( const char *pchCustomHandlerName, PFNLocalizeDialogVariableHandler pfnLocalizeFunc, PFNParseDialogVariableModifiersHandler pfnParseModifiers, void *pUserData, bool bVirtual ) OVERRIDE;
	virtual void RemoveCustomDialogVariableHandler( const char *pchCustomHandlerName ) OVERRIDE;

	virtual ILocalizationString *CloneString( const IUIPanel *pPanel, const ILocalizationString *pLocToken, bool bStringAlreadyFullyParsed = false );
	virtual const char *PchFindRawString( const char *pchToken );

	// supported types for setting dialog variables
	virtual bool SetDialogVariable( const IUIPanel *pPanel, const char *pchKey, const char *pchValue ) OVERRIDE;
#if defined( SOURCE2_PANORAMA )
	virtual bool SetDialogVariable( const IUIPanel *pPanel, const char *pchKey, time_t timeVal ) OVERRIDE;
	virtual bool SetDialogVariable( const IUIPanel *pPanel, const char *pchKey, CCurrencyAmount amount ) OVERRIDE;
#else
	virtual bool SetDialogVariable( const IUIPanel *pPanel, const char *pchKey, CRTime timeVal ) OVERRIDE;
	virtual bool SetDialogVariable( const IUIPanel *pPanel, const char *pchKey, CAmount amount ) OVERRIDE;
#endif
	virtual bool SetDialogVariable( const IUIPanel *pPanel, const char *pchKey, int nVal ) OVERRIDE;
	virtual bool SetDialogVariable( const IUIPanel *pPanel, const char *pchKey, uint64 nVal64 ) OVERRIDE;

	virtual bool IsValidLocalizationString( const ILocalizationString *pLocStr ) OVERRIDE;

	void CloneDialogVariables( const IUIPanel *pPanelFrom, IUIPanel *pPanelTo );

	void DirtyDialogVariable( const IUIPanel *pPanel, const char *pchKey );

	void OnPanelDeleted( const IUIPanel *pPanel );
	bool ResolveDialogVariable( CUtlString& /*out*/ strResult, EPanelKeyType eType, EStringEscapeStyle eEscapeStyle, const char *pchKey, uint32 nModifiers, const char *pchParams, const IUIPanel *pPanel );

	void Release( const IUIPanel *pPanel, ILocalizationString *pLocString );

	void ReloadChangedFile( const char *pchFile );
	void SetLongestStringForToken( const ILocalizationString *pLocalizationString, ILocalizationStringSizeResolver *pResolver );

	uint32 ParseGenericDialogVariableModifiers( bool bVirtual, const CUtlString &strKey, const CUtlString &strModifiers );

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName );
#endif

private:
	enum EKeyReplaceStrategy
	{
		kKeyReplace_DoNotReplace,					// never replace keys (could be used for initial load of English if we distinguished between initial initial load and language setting)
		kKeyReplace_ReplaceAny,						// replace our value for this key regardless of language (stomp default strings with current language strings)
		kKeyReplace_ReplaceMatchingLanguage			// replace our value for this key/language pair, but ignore keys in other languages (reloading any loc file)
	};

	// helpers loading files
    bool BLoadLocalization( const char *pchLanguage, const char *pchBaseDir, EKeyReplaceStrategy eKeyReplaceStrategy, CUtlVector<CUtlString> &vecChangedTokens );
	bool BLoadLocalizationFile( const char *pchLocFile,  const char *pchLanguage, EKeyReplaceStrategy eKeyReplaceStrategy, CUtlVector<CUtlString> &vecChangedTokens );
	void RecalculateStringFromReload( CUtlVector<CUtlString> &vecChangedTokens, bool bStringsValid );
	int GetLocalizationFileCount( void ) { return m_vecLocalizationFiles.Count(); }
	const char* GetLocalizationFileName( int i ) { return m_vecLocalizationFiles.Element( i ); }
	virtual void UnloadLocalizationFileStrings( void ) OVERRIDE;

	// create loc string objects
	CLocalizationStringSimple *CreateSimpleLocalizedString( const IUIPanel *pPanel, const char *pchValue, const uint32 ccMax, EStringTruncationStyle eTrunkStyle, EStringTransformStyle eTransformStyle, EStringEscapeStyle eEscapeStyle, bool bFromLocString, const char *pchToken, bool bStringAlreadyFullyParsed );
	void RecalculateDialogVariables( CLocalizationStringDialogVariablesDerived *pLoc, const char *pchValue );

	// string formatting helper
	void PrintTimeHelper( char *pBuf, int ccBuf, int iVal, const char *pchLocStringPlural, const char *pchLocString );
	void PrintIntAndStringHelper( char *pBuf, int ccBuf, int iVal, const char *pchLocString );

	static void OnLocalizationFileChanged( const char *pFullPath );

	CUtlSymbol m_sLanguage;
	CUtlString m_sLocalizationFilePath;
	bool m_bInitialized;
	CUtlHashMap<const char *, bool> m_hashLanguagesCheckedForDupes;

	struct StringData_t
	{
		CUtlBlockMemory<char, int> m_data;
		int m_nChars;

		StringData_t()
			: m_data( 65536 )
		{
			m_nChars = 0;
		}

		const char *Add( const char *pStr )
		{
			int nStrChars = V_strlen( pStr ) + 1;
			if ( nStrChars > m_data.BlockSize() )
			{
				Plat_FatalError( "Localization string too long: %d chars in '%.20s...'\n", nStrChars, pStr );
			}
			int nIndex = m_data.EnsureContiguousCapacity( m_nChars, nStrChars );
			V_memcpy( &m_data[nIndex], pStr, nStrChars * sizeof(*pStr) );
			// NOTE: Do not add to m_nChars as EnsureContiguousCapacity
			// may have returned a different starting point.
			m_nChars = nIndex + nStrChars;
			return &m_data[nIndex];
		}

		void Swap( StringData_t &other )
		{
			m_data.Swap( other.m_data );
			V_swap( m_nChars, other.m_nChars );
		}

		void Purge()
		{
			m_data.Purge();
			m_nChars = 0;
		}
	};

	StringData_t m_allStringData;

	struct LocEntry_t
	{
		LocEntry_t() { m_sLanguage = ""; }

		// String pointers point into m_memAllStringData.
		const char *m_pString;
		const char *m_pStrToken;
		CUtlSymbol m_sLanguage;
		bool m_bFromLocFile : 1; // Pack with CUtlSymbol
		int m_nNext;
	};
	ILocalizationString *CreateLocalizationString( const IUIPanel *pPanel, const char *pchToken, const uint32 ccMax, EStringTruncationStyle eTrunkStyle, EStringTransformStyle eTransformStyle, EStringEscapeStyle eEscapeStyle, bool bAllowDialogVariableParsing, bool bStringAlreadyFullyParsed );
	
	// map of the raw loc data from files after parsing
	// String pointers point into m_memAllStringData.
#ifndef PANORAMA_USE_S1WRAPPER
	typedef CUtlHashMap< const char *, int, CDefCaselessStringEquals, FastCaselessStringHashFunctor > LocalizationStringsMap_t;
#else
	typedef CUtlHashMap< const char *, int, CDefCaselessStringEquals, CaselessStringHashFunctor > LocalizationStringsMap_t;
#endif
	LocalizationStringsMap_t m_mapLocalizationStrings;

	// Vector holding the string entries
	CUtlVector< LocEntry_t > m_vecLocEntrys;

	struct PanelLocEntry_t
	{
		PanelLocEntry_t() { m_pLoc = NULL; }
		PanelLocEntry_t( ILocalizationString *pLoc, const char *pchToken ) { m_pLoc = pLoc; m_symToken = pchToken; }
		ILocalizationString *m_pLoc;
		CUtlString m_symToken;

#if !defined( SOURCE2_PANORAMA )
#ifdef new
#define REDFINE_NEW
#undef new
#endif
		DECLARE_FIXEDSIZE_ALLOCATOR( PanelLocEntry_t );
#ifdef REDFINE_NEW
#define new MEMALL_DEBUG_NEW
#endif
#else
		DECLARE_FIXEDSIZE_ALLOCATOR( PanelLocEntry_t );
#endif
	};
	// tracks which panels own which loc strings
	CUtlMap< const IUIPanel*, CCopyableUtlVector<PanelLocEntry_t *>, int, CDefLess< const IUIPanel* > > m_mapLocStringsOwnedByPanel;

	void SetDialogVariablesToPanel( CCopyableUtlVector<DialogVariable_t> &vecDialogVars, const IUIPanel *pPanel, PanelLocEntry_t *pLocEntry );
	void RemoveDialogVariablesToPanel( const IUIPanel *pPanel, ILocalizationString *pLocStr );

	// tracks DialogVariable to panels, used when setting a dialog
	struct DialogVariableToPanel_t
	{
		void Set( EPanelKeyType eType, const IUIPanel *pPanel, PanelLocEntry_t *pLocEntry )
		{
			m_eType = eType;
			m_pPanel = pPanel;
			m_pLocEntry = pLocEntry;
		}

		EPanelKeyType m_eType;
		const IUIPanel *m_pPanel;
		PanelLocEntry_t *m_pLocEntry;
		int m_nNext;
	};
	CUtlHashMap< CUtlString, int > m_mapDialogVariableToPanels;
	CUtlVector< DialogVariableToPanel_t > m_vecDialogVariableToPanels;
	int m_nFirstFreeDialogVariableToPanel;

	struct DialogVariableHandler_t
	{
		PFNLocalizeDialogVariableHandler m_pfnHandler;
		PFNParseDialogVariableModifiersHandler m_pfnParseModifiers;
		void *m_pUserData;
	};
	CUtlHashMap< CUtlString, DialogVariableHandler_t > m_mapGenericDialogVariableHandlers;
	CUtlHashMap< CUtlString, DialogVariableHandler_t > m_mapVirtualDialogVariableHandlers;
	
	// map of strings we didn't have loc tokens for so just made a simple wrapper
	CUtlMap< const IUIPanel*, CLocalizationStringSimple *, int, CDefLess< const IUIPanel* > > m_mapNonLocalizedStrings;
	
	// map of strings we did have a symbol for and gave them a wrapper object to it
#ifdef PANORAMA_USE_S1WRAPPER
	CUtlMap< CUtlString, ILocalizationString *, int, CDefCaselessStringLess > m_mapIssuedStrings;
#else
	CUtlMap< CUtlString, ILocalizationString *, int, CDefLess< CUtlString > > m_mapIssuedStrings;
#endif

	bool SetDialogVariableHelper( const IUIPanel *pPanel, const CPanelKeyValue &panelKeyValue );
	void CheckPanelNeedsLocUpdate( const IUIPanel *pPanel, const CPanelKeyValue &panelKeyValue );
	
	// map of panel to the variables it has defined
	CUtlMap< const IUIPanel*, CCopyableUtlVector<CPanelKeyValue>, int, CDefLess< const IUIPanel* > > m_mapPanelVariables;

	// map of dialog var objects we have created and the tokens they are related to
#ifdef PANORAMA_USE_S1WRAPPER
	CUtlMap< CUtlString, CLocalizationStringDialogVariables *, int, CDefCaselessStringLess > m_mapLocStringDialogVariables;
#else
	CUtlMap< CUtlString, CLocalizationStringDialogVariables *, int, CDefLess< CUtlString > > m_mapLocStringDialogVariables;
#endif

	CLocalizationStringSimple *m_pNullLocString; // cache off the null string, its used often so have a copy to return it quickly
	CUtlVector< CUtlString > m_vecLocalizationFiles;
};

} // namespace panorama

#endif // LOCALIZE_H
