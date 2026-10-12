//======= Copyright 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//


#pragma warning( disable: 4018 ) // '==' : signed/unsigned mismatch in rbtree
#if defined( WIN32 ) && !defined( _X360 )
#include <windows.h>
#include <vadefs.h>
#elif defined( _PS3 )


#elif defined( POSIX )
#include <iconv.h>
#endif

#include <wchar.h>

#include "filesystem.h"

#include "localize/ilocalize.h"
#include "tier1/utlvector.h"
#include "tier1/utlrbtree.h"
#include "tier1/utlsymbol.h"
#include "tier1/utlstring.h"
#include "UnicodeFileHelpers.h"
#include "tier0/icommandline.h"
#include "byteswap.h"
#include "exprevaluator.h"
#include "iregistry.h"
#include <vstdlib/vstrtools.h>
#include "vgui/ISystem.h"
#include "vgui_controls/Controls.h"
#include "currencyamount.h"

#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define MAX_LOCALIZED_CHARS	4096

#define SUB_LANGUAGE_NONE '_'

const int64 nanmask_double = 2047LL << 52;
#define	IS_NAN_DOUBLE(x) (((*(int64 *)&x)&nanmask_double)==nanmask_double)

//-----------------------------------------------------------------------------
// Currency data tables.
//-----------------------------------------------------------------------------

struct CurrencyMoneyStringConfiguration_t
{
	const char *m_pchSymbol;
	enum ESymbol_t
	{
		k_ESymbolFirstThenAmount,
		k_EAmountFirstThenSymbol
	} m_eSymbolPlacementPolicy;
	enum ESpacing_t
	{
		k_ESpaceBetweenSymbolAndAmount,
		k_ETogether
	} m_eSpaceBetweenTokens;
	enum EDenomination_t
	{
		k_EHasCents,
		k_EWholeUnitsOnly
	} m_eDenominationFraction;
	explicit CurrencyMoneyStringConfiguration_t( const char *pchSymbol )
		: m_pchSymbol( pchSymbol ), m_eSymbolPlacementPolicy( k_ESymbolFirstThenAmount ), m_eSpaceBetweenTokens( k_ETogether ), m_eDenominationFraction( k_EHasCents ) {}
	explicit CurrencyMoneyStringConfiguration_t( const char *pchSymbol, ESymbol_t eSymbolPlacement )
		: m_pchSymbol( pchSymbol ), m_eSymbolPlacementPolicy( eSymbolPlacement ), m_eSpaceBetweenTokens( k_ETogether ), m_eDenominationFraction( k_EHasCents ) {}
	explicit CurrencyMoneyStringConfiguration_t( const char *pchSymbol, ESpacing_t eSpaceBetweenTokens )
		: m_pchSymbol( pchSymbol ), m_eSymbolPlacementPolicy( k_ESymbolFirstThenAmount ), m_eSpaceBetweenTokens( eSpaceBetweenTokens ), m_eDenominationFraction( k_EHasCents ) {}
	explicit CurrencyMoneyStringConfiguration_t( const char *pchSymbol, EDenomination_t eDenominationFraction )
		: m_pchSymbol( pchSymbol ), m_eSymbolPlacementPolicy( k_ESymbolFirstThenAmount ), m_eSpaceBetweenTokens( k_ETogether ), m_eDenominationFraction( eDenominationFraction ) {}
	explicit CurrencyMoneyStringConfiguration_t( const char *pchSymbol, ESymbol_t eSymbolPlacement, ESpacing_t eSpaceBetweenTokens )
		: m_pchSymbol( pchSymbol ), m_eSymbolPlacementPolicy( eSymbolPlacement ), m_eSpaceBetweenTokens( eSpaceBetweenTokens ), m_eDenominationFraction( k_EHasCents ) {}
	explicit CurrencyMoneyStringConfiguration_t( const char *pchSymbol, ESpacing_t eSpaceBetweenTokens, ESymbol_t eSymbolPlacement )
		: m_pchSymbol( pchSymbol ), m_eSymbolPlacementPolicy( eSymbolPlacement ), m_eSpaceBetweenTokens( eSpaceBetweenTokens ), m_eDenominationFraction( k_EHasCents ) {}
	explicit CurrencyMoneyStringConfiguration_t( const char *pchSymbol, ESymbol_t eSymbolPlacement, ESpacing_t eSpaceBetweenTokens, EDenomination_t eDenominationFraction )
		: m_pchSymbol( pchSymbol ), m_eSymbolPlacementPolicy( eSymbolPlacement ), m_eSpaceBetweenTokens( eSpaceBetweenTokens ), m_eDenominationFraction( eDenominationFraction ) {}
	explicit CurrencyMoneyStringConfiguration_t( const char *pchSymbol, ESpacing_t eSpaceBetweenTokens, ESymbol_t eSymbolPlacement, EDenomination_t eDenominationFraction )
		: m_pchSymbol( pchSymbol ), m_eSymbolPlacementPolicy( eSymbolPlacement ), m_eSpaceBetweenTokens( eSpaceBetweenTokens ), m_eDenominationFraction( eDenominationFraction ) {}
};

static CurrencyMoneyStringConfiguration_t GetCurrencyMoneyStringConfiguration( ECurrency eCurrencyCode )
{
	switch( eCurrencyCode )
	{
	case k_ECurrencyUSD: return CurrencyMoneyStringConfiguration_t( "$" );
	case k_ECurrencyGBP: return CurrencyMoneyStringConfiguration_t( "\xC2\xA3" );
	case k_ECurrencyEUR: return CurrencyMoneyStringConfiguration_t( "\xE2\x82\xAC", CurrencyMoneyStringConfiguration_t::k_EAmountFirstThenSymbol );
	case k_ECurrencyRUB: return CurrencyMoneyStringConfiguration_t( "\xD1\x80\xD1\x83\xD0\xB1", CurrencyMoneyStringConfiguration_t::k_EAmountFirstThenSymbol, CurrencyMoneyStringConfiguration_t::k_ESpaceBetweenSymbolAndAmount, CurrencyMoneyStringConfiguration_t::k_EWholeUnitsOnly ); // localized py6
	case k_ECurrencyBRL: return CurrencyMoneyStringConfiguration_t( "R$" );
	case k_ECurrencyJPY: return CurrencyMoneyStringConfiguration_t( "\xC2\xA5", CurrencyMoneyStringConfiguration_t::k_EWholeUnitsOnly );
	case k_ECurrencyIDR: return CurrencyMoneyStringConfiguration_t( "Rp", CurrencyMoneyStringConfiguration_t::k_ESymbolFirstThenAmount, CurrencyMoneyStringConfiguration_t::k_ESpaceBetweenSymbolAndAmount, CurrencyMoneyStringConfiguration_t::k_EWholeUnitsOnly );
	case k_ECurrencyMYR: return CurrencyMoneyStringConfiguration_t( "RM" );
	case k_ECurrencyPHP: return CurrencyMoneyStringConfiguration_t( "\xE2\x82\xB1" );
	case k_ECurrencySGD: return CurrencyMoneyStringConfiguration_t( "S$" );
	case k_ECurrencyTHB: return CurrencyMoneyStringConfiguration_t( "\xE0\xB8\xBF" );
	case k_ECurrencyVND: return CurrencyMoneyStringConfiguration_t( "\xE2\x82\xAB", CurrencyMoneyStringConfiguration_t::k_EAmountFirstThenSymbol, CurrencyMoneyStringConfiguration_t::k_ESpaceBetweenSymbolAndAmount, CurrencyMoneyStringConfiguration_t::k_EWholeUnitsOnly );
	case k_ECurrencyKRW: return CurrencyMoneyStringConfiguration_t( "\xE2\x82\xA9", CurrencyMoneyStringConfiguration_t::k_EWholeUnitsOnly );
	case k_ECurrencyTRY: return CurrencyMoneyStringConfiguration_t( "TL", CurrencyMoneyStringConfiguration_t::k_EAmountFirstThenSymbol, CurrencyMoneyStringConfiguration_t::k_ESpaceBetweenSymbolAndAmount );
	case k_ECurrencyUAH: return CurrencyMoneyStringConfiguration_t( "\xE2\x82\xB4", CurrencyMoneyStringConfiguration_t::k_EAmountFirstThenSymbol, CurrencyMoneyStringConfiguration_t::k_ETogether, CurrencyMoneyStringConfiguration_t::k_EWholeUnitsOnly );
	case k_ECurrencyMXN: return CurrencyMoneyStringConfiguration_t( "Mex$", CurrencyMoneyStringConfiguration_t::k_ESpaceBetweenSymbolAndAmount );
	case k_ECurrencyCAD: return CurrencyMoneyStringConfiguration_t( "C$", CurrencyMoneyStringConfiguration_t::k_ESpaceBetweenSymbolAndAmount );
	case k_ECurrencyAUD: return CurrencyMoneyStringConfiguration_t( "A$", CurrencyMoneyStringConfiguration_t::k_ESpaceBetweenSymbolAndAmount );
	case k_ECurrencyNZD: return CurrencyMoneyStringConfiguration_t( "NZ$", CurrencyMoneyStringConfiguration_t::k_ESpaceBetweenSymbolAndAmount );
	case k_ECurrencyNOK: return CurrencyMoneyStringConfiguration_t( "kr", CurrencyMoneyStringConfiguration_t::k_EAmountFirstThenSymbol, CurrencyMoneyStringConfiguration_t::k_ESpaceBetweenSymbolAndAmount );
	case k_ECurrencyPLN: return CurrencyMoneyStringConfiguration_t( "z\xC5\x82", CurrencyMoneyStringConfiguration_t::k_EAmountFirstThenSymbol, CurrencyMoneyStringConfiguration_t::k_ESpaceBetweenSymbolAndAmount );
	case k_ECurrencyCHF: return CurrencyMoneyStringConfiguration_t( "CHF", CurrencyMoneyStringConfiguration_t::k_ESpaceBetweenSymbolAndAmount );
	case k_ECurrencyAED: return CurrencyMoneyStringConfiguration_t( "DH", CurrencyMoneyStringConfiguration_t::k_EAmountFirstThenSymbol, CurrencyMoneyStringConfiguration_t::k_ESpaceBetweenSymbolAndAmount );
	case k_ECurrencyCLP: return CurrencyMoneyStringConfiguration_t( "CLP$", CurrencyMoneyStringConfiguration_t::k_ESpaceBetweenSymbolAndAmount );
	case k_ECurrencyCNY: return CurrencyMoneyStringConfiguration_t( "\xC2\xA5", CurrencyMoneyStringConfiguration_t::k_EWholeUnitsOnly );
	case k_ECurrencyCOP: return CurrencyMoneyStringConfiguration_t( "COL$", CurrencyMoneyStringConfiguration_t::k_ESymbolFirstThenAmount, CurrencyMoneyStringConfiguration_t::k_ESpaceBetweenSymbolAndAmount, CurrencyMoneyStringConfiguration_t::k_EWholeUnitsOnly );
	case k_ECurrencyPEN: return CurrencyMoneyStringConfiguration_t( "S/." );
	case k_ECurrencySAR: return CurrencyMoneyStringConfiguration_t( "SR", CurrencyMoneyStringConfiguration_t::k_EAmountFirstThenSymbol, CurrencyMoneyStringConfiguration_t::k_ESpaceBetweenSymbolAndAmount );
	case k_ECurrencyTWD: return CurrencyMoneyStringConfiguration_t( "NT$", CurrencyMoneyStringConfiguration_t::k_ESymbolFirstThenAmount, CurrencyMoneyStringConfiguration_t::k_ESpaceBetweenSymbolAndAmount, CurrencyMoneyStringConfiguration_t::k_EWholeUnitsOnly );
	case k_ECurrencyHKD: return CurrencyMoneyStringConfiguration_t( "HK$", CurrencyMoneyStringConfiguration_t::k_ESpaceBetweenSymbolAndAmount );
	case k_ECurrencyZAR: return CurrencyMoneyStringConfiguration_t( "R", CurrencyMoneyStringConfiguration_t::k_ESpaceBetweenSymbolAndAmount );
	case k_ECurrencyINR: return CurrencyMoneyStringConfiguration_t( "\xE2\x82\xB9", CurrencyMoneyStringConfiguration_t::k_ESymbolFirstThenAmount, CurrencyMoneyStringConfiguration_t::k_ESpaceBetweenSymbolAndAmount, CurrencyMoneyStringConfiguration_t::k_EWholeUnitsOnly );
	case k_ECurrencyARS: return CurrencyMoneyStringConfiguration_t( "ARS$", CurrencyMoneyStringConfiguration_t::k_ESpaceBetweenSymbolAndAmount );
	case k_ECurrencyCRC: return CurrencyMoneyStringConfiguration_t( "\xE2\x82\xA1", CurrencyMoneyStringConfiguration_t::k_EWholeUnitsOnly );
	case k_ECurrencyILS: return CurrencyMoneyStringConfiguration_t( "\xE2\x82\xAA" );
	case k_ECurrencyKWD: return CurrencyMoneyStringConfiguration_t( "KD", CurrencyMoneyStringConfiguration_t::k_EAmountFirstThenSymbol, CurrencyMoneyStringConfiguration_t::k_ESpaceBetweenSymbolAndAmount );
	case k_ECurrencyQAR: return CurrencyMoneyStringConfiguration_t( "QR", CurrencyMoneyStringConfiguration_t::k_EAmountFirstThenSymbol, CurrencyMoneyStringConfiguration_t::k_ESpaceBetweenSymbolAndAmount );
	case k_ECurrencyUYU: return CurrencyMoneyStringConfiguration_t( "$U", CurrencyMoneyStringConfiguration_t::k_EWholeUnitsOnly );
	case k_ECurrencyKZT: return CurrencyMoneyStringConfiguration_t( "\xE2\x82\xB8", CurrencyMoneyStringConfiguration_t::k_EWholeUnitsOnly );
	case k_ECurrencyBYN: return CurrencyMoneyStringConfiguration_t( "Br" );

	case k_ECurrencyInvalid:
		return CurrencyMoneyStringConfiguration_t( "" );

	default:
		AssertMsg( false, "Unknown currency code" );
		return CurrencyMoneyStringConfiguration_t( "$" );
	}
}

// Helper to convert wchar string to UTF8 
// Writes to the output buffer and then updates the buffer ptr and remaining size ready for a subsequent append 
static void UTF8Append( wchar_t const * pwchSrc, char **ppUTF8Append, int *pnAppendSizeInBytes )
{
	if( pwchSrc && ppUTF8Append && *ppUTF8Append && (*pnAppendSizeInBytes > 1) )
	{
		int nAdded = V_UnicodeToUTF8( pwchSrc, *ppUTF8Append, *pnAppendSizeInBytes );
		if( nAdded > 1 ) //nAdded includes null terminator
		{
			*ppUTF8Append += (nAdded - 1);
			*pnAppendSizeInBytes -= (nAdded - 1);
		}
	}
}

// UTF8AppendChar() also updates the source buffer pointer to the next character
static void UTF8AppendChar( wchar_t const **ppwchSrc, char **ppUTF8Append, int *pnAppendSizeInBytes )
{
	if( ppwchSrc && *ppwchSrc )
	{
		if( ppUTF8Append && *ppUTF8Append && (*pnAppendSizeInBytes > 1) )
		{
			int nAdded = V_WStringCharsToUTF8( *ppwchSrc, 1, *ppUTF8Append, *pnAppendSizeInBytes );
			if( nAdded > 1 ) //nAdded includes null terminator
			{
				*ppUTF8Append += (nAdded - 1);
				*pnAppendSizeInBytes -= (nAdded - 1);
			}
		}
		*ppwchSrc = V_UnicodeAdvance( *ppwchSrc, 1 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: construct string helpers from source2
//-----------------------------------------------------------------------------
static const char *ConstructString_Impl( char *pUTF8Out, int nOutSizeInBytes, const wchar_t *formatString, int numFormatParameters, wchar_t const *const *pwchArgList, bool bAppend /*, bool bApplyKoreanJongSungRule*/ )
{
	if( !pUTF8Out || (nOutSizeInBytes < 1) )
		return NULL;

	if( !bAppend )
	{
		pUTF8Out[0] = 0;
	}
	if( !formatString )
	{
		return pUTF8Out;
	}

	// Assumes we can't have %s0 or %s10.
	// Assumes formatString is zero terminated.

	const wchar_t *searchPos = formatString;
	char* pUTF8Append = pUTF8Out + V_strlen( pUTF8Out );
	int nAppendSizeInBytes = nOutSizeInBytes - (pUTF8Append - pUTF8Out);

	while( searchPos[0] != L'\0' )
	{
		if( searchPos[0] == L'%' && searchPos[1] == L's' && searchPos[2] >= L'1' && searchPos[2] <= L'9' )
		{
			// This is an escape sequence - %s1, %s2 etc, up to %s9.
			int argindex = int( searchPos[2] ) - L'0' - 1;
			Assert( argindex >= 0 );
			if( argindex < numFormatParameters && pwchArgList[argindex] )
			{
				UTF8Append( pwchArgList[argindex], &pUTF8Append, &nAppendSizeInBytes );
				searchPos += 3;
			}
			else
			{
				//copy it over, char by char
				UTF8AppendChar(&searchPos, &pUTF8Append, &nAppendSizeInBytes );
			}
		}
		else
		{
			//copy it over, char by char
			UTF8AppendChar( &searchPos, &pUTF8Append, &nAppendSizeInBytes );
		}
	}

	return pUTF8Out;
}

static const char* ConstructStringVArgs( char *pUTF8Out, int nOutSizeInBytes, const wchar_t *formatString, int numFormatParameters, va_list argList, bool bAppend )
{
	const wchar_t *formatParams[10];
	Assert( numFormatParameters <= V_ARRAYSIZE( formatParams ) );
	if( numFormatParameters > V_ARRAYSIZE( formatParams ) )
	{
		numFormatParameters = V_ARRAYSIZE( formatParams );
	}
	for( int i = 0; i < numFormatParameters; i++ )
	{
		formatParams[i] = va_arg( argList, const wchar_t * );
	}

	return ConstructString_Impl( pUTF8Out, nOutSizeInBytes, formatString, numFormatParameters, formatParams, bAppend );
}

static const char *AppendConstructString( char *pUTF8Out, int nOutSizeInBytes, const wchar_t *formatString, int numFormatParameters, ... )
{
	va_list argList;
	va_start( argList, numFormatParameters );

	ConstructStringVArgs( pUTF8Out, nOutSizeInBytes, formatString, numFormatParameters, argList, true );

	va_end( argList );
	return pUTF8Out;
}

//-----------------------------------------------------------------------------
// 
// Internal implementation
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose: Maps token names to localized unicode strings
//-----------------------------------------------------------------------------
class CLocalize : public CTier2AppSystem< ILocalize >
{
	typedef CTier2AppSystem< ILocalize > BaseClass;

	// Methods of IAppSystem
public:
	virtual InitReturnVal_t Init();

	// ILocalize overrides
public:
	virtual bool AddFile( const char *fileName, const char *pPathID, bool bIncludeFallbackSearchPaths );
	virtual void RemoveAll();
	virtual wchar_t *Find(const char *pName);
	virtual const wchar_t *FindSafe(const char *tokenName);
	virtual const wchar_t *FindSubLanguage( const char *pToken ) OVERRIDE;
	virtual const wchar_t *FindSubLanguageSafe( const char *pToken ) OVERRIDE;
	virtual int ConvertANSIToUnicode(const char *ansi, wchar_t *unicode, int unicodeBufferSizeInBytes);
	virtual int ConvertUnicodeToANSI(const wchar_t *unicode, char *ansi, int ansiBufferSize);
	virtual LocalizeStringIndex_t FindIndex(const char *pName);
	virtual const char *GetNameByIndex(LocalizeStringIndex_t index);
	virtual wchar_t *GetValueByIndex(LocalizeStringIndex_t index);
	virtual const char *GetCurrentSubLanguage() OVERRIDE;
	virtual void SetCurrentSubLanguage( const char *pSubLang ) OVERRIDE;
	virtual const char *SetCurrentSubLanguageFromSystem() OVERRIDE;
	virtual LocalizeStringIndex_t GetFirstStringIndex();
	virtual LocalizeStringIndex_t GetNextStringIndex(LocalizeStringIndex_t index);
	virtual void AddString(const char *tokenName, wchar_t *unicodeString, const char *fileName);
	virtual void SetValueByIndex(LocalizeStringIndex_t index, wchar_t *newValue);
	virtual bool SaveToFile( const char *fileName );
	virtual int GetLocalizationFileCount();
	virtual const char *GetLocalizationFileName(int index);
	virtual const char *GetFileNameByIndex(LocalizeStringIndex_t index);
	virtual void ReloadLocalizationFiles( );
	virtual void ConstructString(wchar_t *unicodeOutput, int unicodeBufferSizeInBytes, const char *tokenName, KeyValues *dialogVariables);
	virtual void ConstructString(wchar_t *unicodeOutput, int unicodeBufferSizeInBytes, LocalizeStringIndex_t unlocalizedTextSymbol, KeyValues *dialogVariables);

	virtual const char *ConstructDateString( char *pUTF8Out, int nOutSizeInBytes, LOC_DATE_FORMAT format, struct tm *pTime, bool bAppend = false ) OVERRIDE;
	virtual const char *ConstructDateString( char *pUTF8Out, int nOutSizeInBytes, LOC_DATE_FORMAT format, uint64 nTime, bool bAppend = false ) OVERRIDE;
	virtual const char *ConstructRelativeDateString( char *pUTF8Out, int nOutSizeInBytes, LOC_DATE_FORMAT fallbackFormat, uint64 nRelativeTime, bool bAppend = false ) OVERRIDE;
	virtual const char *ConstructRelativeTimeString( char *pUTF8Out, int nOutSizeInBytes, LOC_DATE_FORMAT timeFormat, LOC_DATE_FORMAT fallbackFormat, uint64 nRelativeTime, bool bAppend = false ) OVERRIDE;
	virtual const char *ConstructDurationString( char *pUTF8Out, int nOutSizeInBytes, LOC_DURATION_FORMAT format, int nSeconds, bool bAppend = false ) OVERRIDE;
	virtual const char *ConstructNumberString( char *pUTF8Out, int nOutSizeInBytes, LOC_NUMBER_FORMAT format, uint64 nValue, bool bAppend = false ) OVERRIDE;
	virtual const char *ConstructNumberString( char *pUTF8Out, int nOutSizeInBytes, LOC_NUMBER_FORMAT format, double flValue, int nPrecision, bool bAppend = false ) OVERRIDE;
	virtual const char *ConstructOrdinalString( char *pUTF8Out, int nOutSizeInBytes, LOC_ORDINAL_FORMAT format, uint32 nValue, bool bAppend = false ) OVERRIDE;
	virtual const char *ConstructMoneyString( char *pUTF8Out, int nOutSizeInBytes, uint32 nValue, ECurrency eCurrency, int nOverridePrecision = -1, bool bAppend = false ) OVERRIDE;

	virtual void SetTextQuery( ILocalizeTextQuery *pQuery );
	virtual void InstallChangeCallback( ILocalizationChangeCallback *pCallback );
	virtual void RemoveChangeCallback( ILocalizationChangeCallback *pCallback );
	virtual const char *FindAsUTF8( const char *pchTokenName );
	virtual wchar_t* GetAsianFrequencySequence( const char * pLanguage );

protected:
	// internal "interface"
	virtual void ConstructStringVArgsInternal(char *unicodeOutput, int unicodeBufferSizeInBytes, const char *formatString, int numFormatParameters, va_list argList);
	virtual void ConstructStringVArgsInternal(wchar_t *unicodeOutput, int unicodeBufferSizeInBytes, const wchar_t *formatString, int numFormatParameters, va_list argList);

	virtual void ConstructStringKeyValuesInternal(char *unicodeOutput, int unicodeBufferSizeInBytes, const char *formatString, KeyValues *localizationVariables);
	virtual void ConstructStringKeyValuesInternal(wchar_t *unicodeOutput, int unicodeBufferSizeInBytes, const wchar_t *formatString, KeyValues *localizationVariables);

	// Other public methods
public:
	CLocalize();
	virtual ~CLocalize();

	// returns whether a file has already been loaded
	bool LocalizationFileIsLoaded( const char *name );

private:
	void AppendDurationUnit( char *pUTF8Out, int nOutSizeInBytes, int nUnits, const char *pTokenNotOne, const char *pTokenOne );
	struct localizedstring_t
	{
		LocalizeStringIndex_t nameIndex;
		// nameIndex == LOCALIZE_INVALID_STRING_INDEX is used only for searches and implies
		// that pszValueString will be used from union fields.
		union
		{
			LocalizeStringIndex_t valueIndex;		// Used when nameIndex != LOCALIZE_INVALID_STRING_INDEX
			const char * pszValueString;	// Used only if nameIndex == LOCALIZE_INVALID_STRING_INDEX
		};
		CUtlSymbol filename;
	};

	struct LocalizationFileInfo_t
	{
		CUtlSymbol	symName;
		CUtlSymbol	symPathID;
		bool		bIncludeFallbacks;

		static bool LessFunc( const LocalizationFileInfo_t& lhs, const LocalizationFileInfo_t& rhs )
		{
			int iresult = Q_stricmp( lhs.symPathID.String(), rhs.symPathID.String() );
			if ( iresult != 0 )
			{
				return iresult == -1;
			}

			return Q_stricmp( lhs.symName.String(), rhs.symName.String() ) < 0;
		}
	};

	struct fastvalue_t
	{
		int				valueindex;
		const wchar_t	*search;
		static CLocalize	*s_pTable;
	};

private:
	bool AddAllLanguageFiles( const char *baseFileName );
	void BuildFastValueLookup();
	void DiscardFastValueLookup();
	int FindExistingValueIndex( const wchar_t *value );
	bool ReadLocalizationFile( const char *pRelativePath, const char *pPathID );
	void InvokeChangeCallbacks( );
	virtual int ConvertANSIToUCS2(const char *ansi, OUT_Z_BYTECAP(unicodeBufferSizeInBytes) ucs2 *unicode, int unicodeBufferSizeInBytes);
	virtual int ConvertUCS2ToANSI(const ucs2 *unicode, OUT_Z_BYTECAP(ansiBufferSize) char *ansi, int ansiBufferSize);
#if defined ( POSIX ) && !defined( _PS3 )
	virtual void AddString(const char *tokenName, ucs2 *unicodeString, const char *fileName);
#endif
	char m_szLanguage[64];
	char m_szSubLanguageFromSettings[16];
	bool m_bUseOnlyLongestLanguageString;
	bool m_bSuppressChangeCallbacks;
	bool m_bQueuedChangeCallback;

	// Stores the symbol lookup
	CUtlRBTree<localizedstring_t, LocalizeStringIndex_t> m_Lookup;
	
	// stores the string data
	CUtlVector<char> m_Names;
	CUtlVector<wchar_t> m_Values;
	CUtlSymbol m_CurrentFile;
	CUtlVector< LocalizationFileInfo_t > m_LocalizationFiles;
	CUtlRBTree< fastvalue_t, int >	m_FastValueLookup;
	ILocalizeTextQuery *m_pQuery;
	static CLocalize *s_pTable;
	CUtlVector< ILocalizationChangeCallback* > m_ChangeCallbacks;

	CUtlBuffer m_bufAsianFrequencySequence;
	bool m_bAsianFrequencySequenceLoaded;

	// Less function, for sorting strings
	static bool SymLess( localizedstring_t const& i1, localizedstring_t const& i2 );
	static bool FastValueLessFunc( const fastvalue_t& lhs, const fastvalue_t& rhs );
};

// global instance of table
static CLocalize s_Localize;

// expose the interface
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CLocalize, ILocalize, LOCALIZE_INTERFACE_VERSION, s_Localize);


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CLocalize::CLocalize() : 
	m_Lookup( 0, 0, SymLess ), m_Names( 1024 ), m_Values( 2048 ), m_FastValueLookup( 0, 0, FastValueLessFunc )
{
	m_bUseOnlyLongestLanguageString = false;
	m_szSubLanguageFromSettings[0] = 0;
	m_bSuppressChangeCallbacks = false;
	m_bQueuedChangeCallback = false;
	m_pQuery = NULL;
	m_bAsianFrequencySequenceLoaded = false;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CLocalize::~CLocalize()
{
	m_Names.Purge();
	m_Values.Purge();
	m_LocalizationFiles.Purge();
}


//-----------------------------------------------------------------------------
// Init
//-----------------------------------------------------------------------------
InitReturnVal_t CLocalize::Init()
{
	InitReturnVal_t nRetVal = BaseClass::Init();
	if ( nRetVal != INIT_OK )
		return nRetVal;

	GetCurrentSubLanguage();

	m_bUseOnlyLongestLanguageString = ( CommandLine()->FindParm("-all_languages") > 0 );
	return INIT_OK;
}


//-----------------------------------------------------------------------------
// Sets the callback used to check length of a localization string
//-----------------------------------------------------------------------------
void CLocalize::SetTextQuery( ILocalizeTextQuery *pQuery )
{
	m_pQuery = pQuery;
}


//-----------------------------------------------------------------------------
// Add, remove, invoke localization string change callbacks
//-----------------------------------------------------------------------------
void CLocalize::InstallChangeCallback( ILocalizationChangeCallback *pCallback )
{
	if ( m_ChangeCallbacks.Find( pCallback ) != m_ChangeCallbacks.InvalidIndex() )
	{
		Warning( "CLocalize::InstallChangeCallback: Attempted to add the same callback twice!\n" );
		return;
	}

	m_ChangeCallbacks.AddToTail( pCallback );
}

void CLocalize::RemoveChangeCallback( ILocalizationChangeCallback *pCallback )
{
	m_ChangeCallbacks.FindAndRemove( pCallback );
}


//-----------------------------------------------------------------------------
// Purpose: Finds a string in the table
//-----------------------------------------------------------------------------
const char *CLocalize::FindAsUTF8( const char *pchTokenName )
{
	wchar_t *pwch = Find( pchTokenName );
	if ( !pwch )
		return pchTokenName;

	static char rgchT[2048];
	Q_UnicodeToUTF8( pwch, rgchT, sizeof( rgchT ) );
	return rgchT;
}


void CLocalize::InvokeChangeCallbacks( )
{
	// This is to prevent a ton of change callbacks while loading using -all_languages
	if ( m_bSuppressChangeCallbacks )
	{
		m_bQueuedChangeCallback = true;
		return;
	}

	int nCount = m_ChangeCallbacks.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		m_ChangeCallbacks[i]->OnLocalizationChanged();
	}
}


int DistanceToEndOfLine( ucs2 *start )
{
	int nResult = 0;

	if ( !*start )
	{
		return nResult;
	}

	while ( *start )
	{
		if ( *start == 0x0D || *start== 0x0A )
		{
			break;
		}

		start++;
		nResult++;
	}

	while ( *start == 0x0D || *start== 0x0A )
	{
		start++;
		nResult++;
	}

	return nResult;
}

//-----------------------------------------------------------------------------
// Purpose:Reads the contents of a file
//-----------------------------------------------------------------------------
bool CLocalize::ReadLocalizationFile( const char *pRelativePath, const char *pPathID )
{
	FileHandle_t file = g_pFullFileSystem->Open( pRelativePath, "rb", pPathID );
	if ( FILESYSTEM_INVALID_HANDLE == file )
		return false;

	// this is an optimization so that the filename string doesn't have to get converted to a symbol for each key/value
	m_CurrentFile = pRelativePath;

	// read into a memory block
	int fileSize = g_pFullFileSystem->Size(file);
	int bufferSize = g_pFullFileSystem->GetOptimalReadSize( file, fileSize + sizeof(wchar_t) );
	ucs2 *memBlock = (ucs2 *)g_pFullFileSystem->AllocOptimalReadBuffer(file, bufferSize);
	bool bReadOK = ( g_pFullFileSystem->ReadEx(memBlock, bufferSize, fileSize, file) != 0 );

	// finished with file
	g_pFullFileSystem->Close(file);

	// null-terminate the stream
	memBlock[fileSize / sizeof(ucs2)] = 0x0000;

	// check the first character, make sure this a little-endian unicode file
	ucs2 *data = memBlock;
	ucs2 signature = LittleShort( data[0] );
	if ( !bReadOK || signature != 0xFEFF )
	{
		Msg( "Ignoring non-unicode close caption file %s\n", pRelativePath );
		g_pFullFileSystem->FreeOptimalReadBuffer( memBlock );
		m_CurrentFile = UTL_INVAL_SYMBOL;
		return false;
	}

	// ensure little-endian unicode reads correctly on all platforms
	CByteswap byteSwap;
	byteSwap.SetTargetBigEndian( false );
	byteSwap.SwapBufferToTargetEndian( data, data, fileSize / sizeof(ucs2) );

	// skip past signature
	data++;

	// parse out a token at a time
	enum states_e
	{
		STATE_BASE,		// looking for base settings
		STATE_TOKENS,	// reading in unicode tokens
	};

	bool bQuoted;
	bool bEnglishFile = false;
	if ( Q_stristr(pRelativePath, "_english.txt") )
	{
		bEnglishFile = true;
	}

	bool spew = false;
	if ( CommandLine()->FindParm( "-ccsyntax" ) )
	{
		spew = true;
	}

	BuildFastValueLookup();

	CExpressionEvaluator ExpressionHandler;

	states_e state = STATE_BASE;
	while (1)
	{
		// read the key and the value
		ucs2 keytoken[128];
		data = ReadUnicodeToken(data, keytoken, 128, bQuoted);
		if (!keytoken[0])
			break;	// we've hit the null terminator

		// convert the token to a string
		char key[128];
		ConvertUCS2ToANSI(keytoken, key, sizeof(key));

		// if we have a C++ style comment, read to end of line and continue
		if (!strnicmp(key, "//", 2))
		{
			data = ReadToEndOfLine(data);
			continue;
		}

		if ( spew )
		{
			Msg( "%s\n", key );
		}

		ucs2 valuetoken[ MAX_LOCALIZED_CHARS ];

		bool bEnoughCapacity = true;

		if ( DistanceToEndOfLine( data ) > ( MAX_LOCALIZED_CHARS - 1 ) )
		{
			Warning( "Error: Localization key value exceeds MAX_LOCALIZED_CHARS. Problem key: %s\n", key );
			bEnoughCapacity = false;
		}

		data = ReadUnicodeToken(data, valuetoken, MAX_LOCALIZED_CHARS, bQuoted);
		if (!valuetoken[0] && !bQuoted)
			break;	// we've hit the null terminator

		if (state == STATE_BASE)
		{
			if (!stricmp(key, "Language"))
			{
				// copy out our language setting
				char value[MAX_LOCALIZED_CHARS];
				ConvertUCS2ToANSI(valuetoken, value, sizeof(value));
				strncpy(m_szLanguage, value, sizeof(m_szLanguage) - 1);
			}
			else if (!stricmp(key, "Tokens"))
			{
				state = STATE_TOKENS;
			}
			else if (!stricmp(key, "}"))
			{
				// we've hit the end
				break;
			}
		}
		else if (state == STATE_TOKENS)
		{
			if (!stricmp(key, "}"))
			{
				// end of tokens
				state = STATE_BASE;
			}
			else
			{
				// skip our [english] beginnings (in non-english files)
				if ( (bEnglishFile) || (!bEnglishFile && strnicmp(key, "[english]", 9)))
				{
					// Check for a conditional tag
					bool bAccepted = true;
					ucs2 conditional[ MAX_LOCALIZED_CHARS ];
					ucs2 *tempData = ReadUnicodeToken(data, conditional, MAX_LOCALIZED_CHARS, bQuoted);
					char cond[MAX_LOCALIZED_CHARS];
 					V_UCS2ToUTF8( conditional, cond, sizeof(cond) );
					if ( !bQuoted && (strstr( cond, "[$" )||strstr( cond, "[!$" )) )
					{
						// Evaluate the conditional tag
						char cond[MAX_LOCALIZED_CHARS];
						ConvertUCS2ToANSI( conditional, cond, sizeof( cond ) );
						ExpressionHandler.Evaluate( bAccepted, cond );
						data = tempData;
					}
					if ( bAccepted && bEnoughCapacity )
					{
						// add the string to the table
						AddString(key, valuetoken, NULL);
					}
				}
			}
		}
	}

	g_pFullFileSystem->FreeOptimalReadBuffer( memBlock );
	m_CurrentFile = UTL_INVAL_SYMBOL;
	DiscardFastValueLookup();
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Adds the contents of a file 
//-----------------------------------------------------------------------------
bool CLocalize::AddFile( const char *szFileName, const char *pPathID, bool bIncludeFallbackSearchPaths )
{
	// use the correct file based on the chosen language
	static const char *const LANGUAGE_STRING = "%language%";
	static const char *const ENGLISH_STRING = "english";
	static const int MAX_LANGUAGE_NAME_LENGTH = 64;
	int offs = 0;
	bool success = false;

	char language[MAX_LANGUAGE_NAME_LENGTH];
	memset( language, 0, sizeof(language) );

	if ( Q_IsAbsolutePath( szFileName ) )
	{
		Warning( "Full paths not allowed in localization file specificaton %s\n", szFileName );
		return false;
	}

	const char *langptr = strstr(szFileName, LANGUAGE_STRING);
	if (langptr)
	{
		// LOAD THE ENGLISH FILE FIRST
		// always load the file to make sure we're not missing any strings
		// copy out the initial part of the string
		offs = langptr - szFileName;
		char fileName[MAX_PATH];
		strncpy(fileName, szFileName, offs);
		fileName[offs] = 0;

		if ( m_bUseOnlyLongestLanguageString )
		{
			return AddAllLanguageFiles( fileName );
		}

		// append "english" as our default language
		Q_strncat(fileName, ENGLISH_STRING, sizeof( fileName ), COPY_ALL_CHARACTERS );

		// append the end of the initial string
		offs += strlen(LANGUAGE_STRING);
		Q_strncat(fileName, szFileName + offs, sizeof( fileName ), COPY_ALL_CHARACTERS);

		success = AddFile( fileName, pPathID, bIncludeFallbackSearchPaths );

		bool bValid = true;
		if ( IsPC() )
		{
			if ( CommandLine()->CheckParm( "-language" ) )
			{
				Q_strncpy( language, CommandLine()->ParmValue( "-language", "english" ), sizeof( language ) );
				bValid = true;
			}
			else
			{
				bValid = vgui::system()->GetRegistryString( "HKEY_CURRENT_USER\\Software\\Valve\\Steam\\Language", language, sizeof(language)-1 );
			}
			if ( bValid && !Q_stricmp( language, "unknown" ) )
			{
				// Fall back to english
				bValid = false;
			}
		}
		else
		{
#ifdef _GAMECONSOLE
			Q_strncpy( language, XBX_GetLanguageString(), sizeof( language ) );
#endif
		}

		// LOAD THE LOCALIZED FILE IF IT'S NOT ENGLISH
		// append the language
		if ( bValid )
		{
			if ( strlen(language) != 0 && stricmp(language, ENGLISH_STRING) != 0 )
			{
				// When running in Perfect World mode, also add _pw language file after the actual language file
				static char const * const szLanguageSuffix[2] = { NULL, "_pw" };
				static const int kNumLanguageSuffixes = CommandLine()->CheckParm( "-perfectworld" ) ? 2 : 1;
				// (also see localize.cpp under src/panorama/localization at CLocalization::BLoadLocalizationFile)
				for ( int iLanguageSuffix = 0; iLanguageSuffix < kNumLanguageSuffixes; ++ iLanguageSuffix )
				{
					// copy out the initial part of the string
					offs = langptr - szFileName;
					strncpy( fileName, szFileName, offs );
					fileName[ offs ] = 0;

					Q_strncat( fileName, language, sizeof( fileName ), COPY_ALL_CHARACTERS );
					if ( szLanguageSuffix[iLanguageSuffix] )
						Q_strncat( fileName, szLanguageSuffix[ iLanguageSuffix ], sizeof( fileName ), COPY_ALL_CHARACTERS );

					// append the end of the initial string
					offs += strlen( LANGUAGE_STRING );
					Q_strncat( fileName, szFileName + offs, sizeof( fileName ), COPY_ALL_CHARACTERS );

					success &= AddFile( fileName, pPathID, bIncludeFallbackSearchPaths );
				}
			}
		}
		return success;
	}

	// store the localization file name if it doesn't already exist
	LocalizationFileInfo_t search;
	search.symName = szFileName;
	search.symPathID = pPathID ? pPathID : "";
	search.bIncludeFallbacks = false;

	int lfc = m_LocalizationFiles.Count();
	for ( int lf = 0; lf < lfc; ++lf )
	{
		LocalizationFileInfo_t& entry = m_LocalizationFiles[ lf ];
		if ( !Q_stricmp( entry.symName.String(), szFileName ) )
		{
			m_LocalizationFiles.Remove( lf );
			break;
		}
	}

	m_LocalizationFiles.AddToTail( search );

	bool bOk = ReadLocalizationFile( szFileName, pPathID );
	if ( !bOk )
	{
		DevWarning( "ILocalize::AddFile() failed to load file \"%s\".\n", szFileName );
	}

	return bOk;
}

//-----------------------------------------------------------------------------
// Purpose: Load all the localized language strings, and uses the longest string from each language
//-----------------------------------------------------------------------------
bool CLocalize::AddAllLanguageFiles( const char *baseFileName )
{
	bool bSuccess = true;

	// Each new language load could potentially change the string value
	// This will suppress callbacks until we're done.
	m_bSuppressChangeCallbacks = true;

	if ( IsX360() )
	{
#ifdef _X360
		// xbox cannot support FindFirst/FindNext due to zips
		const char *pLanguageString = NULL;
		while ( 1 )
		{
			pLanguageString = XBX_GetNextSupportedLanguage( pLanguageString, NULL );
			if ( !pLanguageString )
			{
				// end of list
				break;
			}

			// re-add in the search path
			char szFile[MAX_PATH];
			V_snprintf( szFile, sizeof( szFile ), "%s%s.txt", baseFileName, pLanguageString );

			// add the file
			bSuccess &= AddFile( szFile, NULL, true );
		}
#endif
	}
	else
	{
		// work out the path the files are in
		char szFilePath[MAX_PATH];
		Q_strncpy( szFilePath, baseFileName, sizeof(szFilePath) );
		char *pLastSlash = strrchr( szFilePath, '\\' );
		if ( !pLastSlash )
		{
			pLastSlash = strrchr( szFilePath, '/' );
		}
		if ( pLastSlash )
		{
			pLastSlash[1] = 0;
		}
		else
		{
			szFilePath[0] = 0;
		}

		// iterate through and add all the languages (for development)
		// the longest string out of all the languages will be used
		char szSearchPath[MAX_PATH];
		Q_snprintf( szSearchPath, sizeof(szSearchPath), "%s*.txt", baseFileName );

		FileFindHandle_t hFind = NULL;
		const char *file = g_pFullFileSystem->FindFirst( szSearchPath, &hFind );
		while ( file )
		{
			// re-add in the search path
			char szFile[MAX_PATH];
			V_snprintf( szFile, sizeof(szFile), "%s%s", szFilePath, file );

			// add the file
			bSuccess &= AddFile( szFile, NULL, true );

			// next file
			file = g_pFullFileSystem->FindNext( hFind );
		}
		g_pFullFileSystem->FindClose( hFind );
	}

	m_bSuppressChangeCallbacks = false;
	if ( m_bQueuedChangeCallback )
	{
		m_bQueuedChangeCallback = false;
		InvokeChangeCallbacks();
	}

	return bSuccess;
}

//-----------------------------------------------------------------------------
// Purpose: saves the entire contents of the token tree to the file
//-----------------------------------------------------------------------------
bool CLocalize::SaveToFile( const char *szFileName )
{
	// parse out the file
	FileHandle_t file = g_pFullFileSystem->Open(szFileName, "wb");
	if (!file)
		return false;

	// only save the symbols relevant to this file
	CUtlSymbol fileName = szFileName;

	// write litte-endian unicode marker
	unsigned short marker = 0xFEFF;
	marker = LittleShort( marker );
	g_pFullFileSystem->Write(&marker, sizeof( marker ), file);

	const char *startStr = "\"lang\"\r\n{\r\n\"Language\" \"English\"\r\n\"Tokens\"\r\n{\r\n";
	const char *endStr = "}\r\n}\r\n";

	// write out the first string
	static ucs2 unicodeString[1024];
	int strLength = ConvertANSIToUCS2(startStr, unicodeString, sizeof(unicodeString));
	if (!strLength)
		return false;

	g_pFullFileSystem->Write(unicodeString, strlen(startStr) * sizeof(ucs2), file);

	// convert our spacing characters to unicode
//	wchar_t unicodeSpace = L' '; 
	ucs2 unicodeQuote = L'\"'; 
	ucs2 unicodeCR = L'\r'; 
	ucs2 unicodeNewline = L'\n'; 
	ucs2 unicodeTab = L'\t';

	// write out all the key/value pairs
	for (LocalizeStringIndex_t idx = GetFirstStringIndex(); idx != LOCALIZE_INVALID_STRING_INDEX; idx = GetNextStringIndex(idx))
	{
		// only write strings that belong in this file
		if (fileName != m_Lookup[idx].filename)
			continue;

		const char *name = GetNameByIndex(idx);
		wchar_t *value = GetValueByIndex(idx);

		// convert the name to a unicode string
		ConvertANSIToUCS2(name, unicodeString, sizeof(unicodeString));

		g_pFullFileSystem->Write(&unicodeTab, sizeof(ucs2), file);

		// write out
		g_pFullFileSystem->Write(&unicodeQuote, sizeof(ucs2), file);
		g_pFullFileSystem->Write(unicodeString, strlen(name) * sizeof(ucs2), file);
		g_pFullFileSystem->Write(&unicodeQuote, sizeof(ucs2), file);

		g_pFullFileSystem->Write(&unicodeTab, sizeof(ucs2), file);
		g_pFullFileSystem->Write(&unicodeTab, sizeof(ucs2), file);

		g_pFullFileSystem->Write(&unicodeQuote, sizeof(ucs2), file);
#ifdef POSIX
		ucs2 ucs2Value[MAX_LOCALIZED_CHARS];
		V_UnicodeToUCS2( value, wcslen(value)*sizeof(wchar_t), (char *)ucs2Value, sizeof(ucs2Value) );
		g_pFullFileSystem->Write(ucs2Value, wcslen(value) * sizeof(ucs2), file);
#else
		g_pFullFileSystem->Write(value, wcslen(value) * sizeof(ucs2), file);
#endif
		g_pFullFileSystem->Write(&unicodeQuote, sizeof(ucs2), file);

		g_pFullFileSystem->Write(&unicodeCR, sizeof(ucs2), file);
		g_pFullFileSystem->Write(&unicodeNewline, sizeof(ucs2), file);
	}

	// write end string
	strLength = ConvertANSIToUCS2(endStr, unicodeString, sizeof(unicodeString));
	g_pFullFileSystem->Write(unicodeString, strLength * sizeof(ucs2), file);

	g_pFullFileSystem->Close(file);
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: for development, reloads localization files
//-----------------------------------------------------------------------------
void CLocalize::ReloadLocalizationFiles( )
{
	// re-add all the localization files
	for (int i = 0; i < m_LocalizationFiles.Count(); i++)
	{
		LocalizationFileInfo_t& entry = m_LocalizationFiles[ i ];
		AddFile
		(
			entry.symName.String(), 
			entry.symPathID.String()[0] ? entry.symPathID.String() : NULL,
			entry.bIncludeFallbacks 
		);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Used to sort strings
//-----------------------------------------------------------------------------
bool CLocalize::SymLess(localizedstring_t const &i1, localizedstring_t const &i2)
{
	const char *str1 = (i1.nameIndex == LOCALIZE_INVALID_STRING_INDEX) ? i1.pszValueString :
											&s_Localize.m_Names[i1.nameIndex];
	const char *str2 = (i2.nameIndex == LOCALIZE_INVALID_STRING_INDEX) ? i2.pszValueString :
											&s_Localize.m_Names[i2.nameIndex];
	
	return stricmp(str1, str2) < 0;
}


//-----------------------------------------------------------------------------
// Purpose: Finds a string in the table
//-----------------------------------------------------------------------------
wchar_t *CLocalize::Find(const char *pName)
{	
	LocalizeStringIndex_t idx = FindIndex(pName);
	if (idx == LOCALIZE_INVALID_STRING_INDEX)
		return NULL;

	return &m_Values[m_Lookup[idx].valueIndex];
}


// Like Find(), but as a failsafe, returns an error message instead of NULL if the string isn't found.  
const wchar_t *CLocalize::FindSafe(const char *pName)
{
#ifdef _CERT
	const wchar_t *failsafe = L"";
#else
	const wchar_t *failsafe = L"#FIXME_LOCALIZATION_FAIL_MISSING_STRING";
#endif

	const wchar_t *locstr = Find( pName );

	if ( !locstr )
	{
		DevMsg( "CLocalize::FindSafe failed to localize: %s\n", pName );
		return failsafe;
	}
	else
	{
		return locstr;
	}
}

//-----------------------------------------------------------------------------
// Purpose: finds the index of a token by token name
//-----------------------------------------------------------------------------
LocalizeStringIndex_t CLocalize::FindIndex(const char *pName)
{
	if (!pName)
		return LOCALIZE_INVALID_STRING_INDEX;

	// strip the pound character (which is used elsewhere to indicate that it's a string that should be translated)
	if (pName[0] == '#')
	{
		pName++;
	}
	
	// Passing this special invalid symbol makes the comparison function
	// use the string passed in the context
	localizedstring_t invalidItem;
	invalidItem.nameIndex = LOCALIZE_INVALID_STRING_INDEX;
	invalidItem.pszValueString = pName;
	return m_Lookup.Find( invalidItem );
}

#if defined( POSIX ) && !defined( _PS3 )
void CLocalize::AddString(const char *pString, ucs2 *pUCS2Value, const char *fileName)
{
	if (!pString || !pUCS2Value ) 
		return;
	wchar_t pValue[2048];
	V_UCS2ToUnicode( pUCS2Value, pValue, sizeof(pValue) );

	AddString( pString, pValue, fileName );
}
#endif

//-----------------------------------------------------------------------------
// Finds and/or creates a symbol based on the string
//-----------------------------------------------------------------------------
void CLocalize::AddString(const char *pString, wchar_t *pValue, const char *fileName)
{
	if (!pString) 
		return;

	MEM_ALLOC_CREDIT();

	// see if the value is already in our string table
	int valueIndex = FindExistingValueIndex( pValue );
	if ( valueIndex == LOCALIZE_INVALID_STRING_INDEX )
	{
		int len = wcslen( pValue ) + 1;
		valueIndex = m_Values.AddMultipleToTail( len );
		memcpy( &m_Values[valueIndex], pValue, len * sizeof(wchar_t) );
	}

	// see if the key is already in the table
	LocalizeStringIndex_t stridx = FindIndex( pString );
	localizedstring_t item;
	item.nameIndex = stridx;

	if ( stridx == LOCALIZE_INVALID_STRING_INDEX )
	{
		// didn't find, insert the string into the vector.
		int len = strlen(pString) + 1;
		stridx = m_Names.AddMultipleToTail( len );
		memcpy( &m_Names[stridx], pString, len * sizeof(char) );

		item.nameIndex = stridx;
		item.valueIndex = valueIndex;
		item.filename = fileName ? fileName : m_CurrentFile;

		m_Lookup.Insert( item );
	}
	else
	{
		// it's already in the table
		if ( m_bUseOnlyLongestLanguageString )
		{
			// check which string is longer
			wchar_t *newValue = pValue;
			wchar_t *oldValue = GetValueByIndex( stridx );

			// get the width of the string, using just the first font
			if ( m_pQuery )
			{
				int newWide = m_pQuery->ComputeTextWidth( newValue );
				int oldWide = m_pQuery->ComputeTextWidth( oldValue );
				
				// if the new one is shorter, don't let it be added
				if (newWide < oldWide)
					return;
			}
		}

		// replace the current item
		item.nameIndex = GetNameByIndex( stridx ) - &m_Names[ 0 ];
		item.valueIndex = valueIndex;
		item.filename = fileName ? fileName : m_CurrentFile;
		m_Lookup[ stridx ] = item;

		InvokeChangeCallbacks();
	}
}

//-----------------------------------------------------------------------------
// Remove all symbols in the table.
//-----------------------------------------------------------------------------
void CLocalize::RemoveAll()
{
	m_Lookup.RemoveAll();
	m_Names.RemoveAll();
	m_Values.RemoveAll();
	m_LocalizationFiles.RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose: iteration functions
//-----------------------------------------------------------------------------
LocalizeStringIndex_t CLocalize::GetFirstStringIndex()
{
	return m_Lookup.FirstInorder();
}

//-----------------------------------------------------------------------------
// Purpose: returns the next index, or INVALID_STRING_INDEX if no more strings available
//-----------------------------------------------------------------------------
LocalizeStringIndex_t CLocalize::GetNextStringIndex(LocalizeStringIndex_t index)
{
	LocalizeStringIndex_t idx = m_Lookup.NextInorder(index);
	if (idx == m_Lookup.InvalidIndex())
		return LOCALIZE_INVALID_STRING_INDEX;
	return idx;
}

//-----------------------------------------------------------------------------
// Purpose: gets the name of the localization string by index
//-----------------------------------------------------------------------------
const char *CLocalize::GetNameByIndex(LocalizeStringIndex_t index)
{
	localizedstring_t &lstr = m_Lookup[index];
	return &m_Names[lstr.nameIndex];
}

//-----------------------------------------------------------------------------
// Purpose: gets the localized string value by index
//-----------------------------------------------------------------------------
wchar_t *CLocalize::GetValueByIndex(LocalizeStringIndex_t index)
{
	if (index == LOCALIZE_INVALID_STRING_INDEX)
		return NULL;

	localizedstring_t &lstr = m_Lookup[index];
	return &m_Values[lstr.valueIndex];
}


CLocalize *CLocalize::s_pTable = NULL;

bool CLocalize::FastValueLessFunc( const fastvalue_t& lhs, const fastvalue_t& rhs )
{
	Assert( s_pTable );

	const wchar_t *w1 = lhs.search ? lhs.search : &s_pTable->m_Values[ lhs.valueindex ];
	const wchar_t *w2 = rhs.search ? rhs.search : &s_pTable->m_Values[ rhs.valueindex ];

	return ( wcscmp( w1, w2 ) < 0 ) ? true : false;
}

void CLocalize::BuildFastValueLookup()
{
	m_FastValueLookup.RemoveAll();
	s_pTable = this;

	// Build it
	int c = m_Lookup.Count();
	for ( int i = 0; i < c; ++i )
	{
		fastvalue_t val;
		val.valueindex = m_Lookup[ i ].valueIndex;
		val.search = NULL;

		m_FastValueLookup.Insert( val );
	}
}

void CLocalize::DiscardFastValueLookup()
{
	m_FastValueLookup.RemoveAll();
	s_pTable = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CLocalize::FindExistingValueIndex( const wchar_t *value )
{
	if ( !s_pTable )
		return (int)LOCALIZE_INVALID_STRING_INDEX;

	fastvalue_t val;
	val.valueindex = -1;
	val.search = value;

	int idx = m_FastValueLookup.Find( val );
	if ( idx != m_FastValueLookup.InvalidIndex() )
	{
		return m_FastValueLookup[ idx ].valueindex;
	}
	return (int)LOCALIZE_INVALID_STRING_INDEX;
}

//-----------------------------------------------------------------------------
// Purpose: returns which file a string was loaded from
//-----------------------------------------------------------------------------
const char *CLocalize::GetFileNameByIndex(LocalizeStringIndex_t index)
{
	localizedstring_t &lstr = m_Lookup[index];
	return lstr.filename.String();
}

//-----------------------------------------------------------------------------
// Purpose: sets the value in the index
//-----------------------------------------------------------------------------
void CLocalize::SetValueByIndex(LocalizeStringIndex_t index, wchar_t *newValue)
{
	// get the existing string
	localizedstring_t &lstr = m_Lookup[index];
	wchar_t *wstr = &m_Values[lstr.valueIndex];

	// see if the new string will fit within the old memory
	int newLen = wcslen(newValue);
	int oldLen = wcslen(wstr);

	if (newLen > oldLen)
	{
		// it won't fit, so allocate new memory - this is wasteful, but only happens in edit mode
		lstr.valueIndex = m_Values.AddMultipleToTail(newLen + 1);
		memcpy(&m_Values[lstr.valueIndex], newValue, (newLen + 1) * sizeof(wchar_t));
	}
	else
	{
		// copy the string into the old position
		wcscpy(wstr, newValue);		
	}

	InvokeChangeCallbacks();
}

//-----------------------------------------------------------------------------
// Purpose: returns number of localization files currently loaded
//-----------------------------------------------------------------------------
int CLocalize::GetLocalizationFileCount()
{
	return m_LocalizationFiles.Count();
}

//-----------------------------------------------------------------------------
// Purpose: returns localization filename by index
//-----------------------------------------------------------------------------
const char *CLocalize::GetLocalizationFileName(int index)
{
	return m_LocalizationFiles[index].symName.String();
}

//-----------------------------------------------------------------------------
// Purpose: returns whether a localization file has been loaded already
//-----------------------------------------------------------------------------
bool CLocalize::LocalizationFileIsLoaded(const char *name)
{
	int c = m_LocalizationFiles.Count();
	for ( int i = 0; i < c; ++i )
	{
		if ( !Q_stricmp( m_LocalizationFiles[ i ].symName.String(), name ) )
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: converts an english string to unicode
//-----------------------------------------------------------------------------
int CLocalize::ConvertANSIToUnicode(const char *ansi, wchar_t *unicode, int unicodeBufferSizeInBytes)
{
#ifdef POSIX
	return V_UTF8ToUnicode(ansi, unicode, unicodeBufferSizeInBytes);
#else
	int chars = ::MultiByteToWideChar(CP_UTF8, 0, ansi, -1, unicode, unicodeBufferSizeInBytes / sizeof(wchar_t));
	unicode[(unicodeBufferSizeInBytes / sizeof(wchar_t)) - 1] = 0;
	return chars;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: converts an unicode string to an english string
//-----------------------------------------------------------------------------
int CLocalize::ConvertUnicodeToANSI(const wchar_t *unicode, char *ansi, int ansiBufferSize)
{
#ifdef POSIX
	return V_UnicodeToUTF8(unicode, ansi, ansiBufferSize);
#else
	int result = ::WideCharToMultiByte(CP_UTF8, 0, unicode, -1, ansi, ansiBufferSize, NULL, NULL);
	ansi[ansiBufferSize - 1] = 0;
	return result;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: converts an english string to unicode
//-----------------------------------------------------------------------------
int CLocalize::ConvertANSIToUCS2(const char *ansi, OUT_Z_BYTECAP(unicodeBufferSizeInBytes) ucs2 *unicode, int unicodeBufferSizeInBytes)
{
#ifdef POSIX
	return V_UTF8ToUCS2(ansi, strlen(ansi)*sizeof(char), unicode, unicodeBufferSizeInBytes);
#else
	int chars = ::MultiByteToWideChar(CP_UTF8, 0, ansi, -1, unicode, unicodeBufferSizeInBytes / sizeof(wchar_t));
	unicode[(unicodeBufferSizeInBytes / sizeof(wchar_t)) - 1] = 0;
	return chars;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: converts an unicode string to an english string
//-----------------------------------------------------------------------------
int CLocalize::ConvertUCS2ToANSI(const ucs2 *unicode, OUT_Z_BYTECAP(ansiBufferSize) char *ansi, int ansiBufferSize)
{
#ifdef POSIX
	return V_UCS2ToUTF8(unicode, ansi, ansiBufferSize);
#else
	int result = ::WideCharToMultiByte(CP_UTF8, 0, unicode, -1, ansi, ansiBufferSize, NULL, NULL);
	ansi[ansiBufferSize - 1] = 0;
	return result;
#endif
}



//-----------------------------------------------------------------------------
// Purpose: Constructs a string, inserting variables where necessary
//-----------------------------------------------------------------------------
void CLocalize::ConstructString(wchar_t *unicodeOutput, int unicodeBufferSizeInBytes, const char *tokenName, KeyValues *localizationVariables)
{
	LocalizeStringIndex_t index = FindIndex(tokenName);

	if (index != LOCALIZE_INVALID_STRING_INDEX)
	{
		ConstructString(unicodeOutput, unicodeBufferSizeInBytes, index, localizationVariables);
	}
	else
	{
		// string not found, just return the token name
		ConvertANSIToUnicode(tokenName, unicodeOutput, unicodeBufferSizeInBytes);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Constructs a string, inserting variables where necessary
//-----------------------------------------------------------------------------
void CLocalize::ConstructString(wchar_t *unicodeOutput, int unicodeBufferSizeInBytes, LocalizeStringIndex_t unlocalizedTextSymbol, KeyValues *localizationVariables)
{
	if (unicodeBufferSizeInBytes < 1)
		return;

	unicodeOutput[0] = 0;
	const wchar_t *searchPos = GetValueByIndex(unlocalizedTextSymbol);
	if (!searchPos)
	{
		wcsncpy(unicodeOutput, L"[unknown string]", unicodeBufferSizeInBytes / sizeof(wchar_t));
		return;
	}

	wchar_t *outputPos = unicodeOutput;

	//assumes we can't have %s10
	//assume both are 0 terminated?
	int unicodeBufferSize = unicodeBufferSizeInBytes / sizeof(wchar_t);

	while ( *searchPos != '\0' && unicodeBufferSize > 0 )
	{
		bool shouldAdvance = true;

		if ( *searchPos == '%' )
		{
			// this is an escape sequence that specifies a variable name
			if ( searchPos[1] == 's' && searchPos[2] >= '0' && searchPos[2] <= '9' )
			{
				shouldAdvance = false;

				char variableName[3];
				variableName[0] = searchPos[1];
				variableName[1] = searchPos[2];
				variableName[2] = 0;

				// Handle this as a valid, fixed substitution string
				// look up the variable name
				const wchar_t *value = localizationVariables->GetWString( variableName, L"[unknown]" );

				int paramSize = wcslen(value);
				if (paramSize >= unicodeBufferSize)
				{
					paramSize = MAX( 0, unicodeBufferSize - 1 );
				}

				wcsncpy(outputPos, value, paramSize);

				unicodeBufferSize -= paramSize;
				outputPos += paramSize;
				searchPos += 3;
			}
			else if ( searchPos[1] == '%' )
			{
				// just a '%' char, just write the second one
				searchPos++;
			}
			else if ( localizationVariables )
			{
				// get out the variable name
				const wchar_t *varStart = searchPos + 1;

				// first letter of a valid variable MUST be alphanumeric, otherwise this isn't a variable
				if ( iswalnum(*varStart) )
				{
					const wchar_t *varEnd = wcschr( varStart, '%' );

					if ( varEnd && *varEnd == '%' )
					{
						shouldAdvance = false;

						// assume variable names must be ascii, do a quick convert
						char variableName[32];
						char *vset = variableName;
						for ( const wchar_t *pws = varStart; pws < varEnd && (vset < variableName + sizeof(variableName) - 1); ++pws, ++vset )
						{
							*vset = (char)*pws;
						}
						*vset = 0;

						// look up the variable name
						const wchar_t *value = localizationVariables->GetWString( variableName, L"[unknown]" );
					
						int paramSize = wcslen(value);
						if (paramSize >= unicodeBufferSize)
						{
							paramSize = MAX( 0, unicodeBufferSize - 1 );
						}

						wcsncpy(outputPos, value, paramSize);

						unicodeBufferSize -= paramSize;
						outputPos += paramSize;
						searchPos = varEnd + 1;
					}
				}
			}
		}

		if (shouldAdvance)
		{
			//copy it over, char by char
			*outputPos = *searchPos;

			outputPos++;
			unicodeBufferSize--;

			searchPos++;
		}		
	}

	// ensure null termination
	*outputPos = '\0';
}

static const wchar_t *LocFormatToPrintFormat( const wchar_t *pwchLocFormat, const wchar_t *pwchDefault )
{
	if( !pwchLocFormat || !*pwchLocFormat )
	{
		return pwchDefault;
	}

	if( pwchLocFormat[0] == L'0' )
	{
		if( V_wcscmp( pwchLocFormat, L"02" ) == 0 )
		{
			return L"%02d";
		}

		if( V_wcscmp( pwchLocFormat, L"04" ) == 0 )
		{
			return L"%04d";
		}
	}
	else
	{
		if( V_wcscmp( pwchLocFormat, L"1" ) == 0 )
		{
			return L"%d";
		}
	}

	return pwchDefault;
}

static bool CheckAMPM( const wchar_t *pwchLocFormat )
{
	if( !pwchLocFormat || !*pwchLocFormat || (pwchLocFormat[0] == L'_' && pwchLocFormat[1] == 0) )
	{
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Localized date and time strings.
//-----------------------------------------------------------------------------
const char *CLocalize::ConstructDateString( char *pUTF8Out, int nOutSizeInBytes, LOC_DATE_FORMAT format, struct tm *pTime, bool bAppend )
{
	if( !pUTF8Out || (nOutSizeInBytes < 1) )
		return NULL;

	const wchar_t *pwchMonth = FindSubLanguageSafe( CFmtStr( "#LOC_Date_Month%d", pTime->tm_mon ) );
	const wchar_t *pwchMonthShort = FindSubLanguageSafe( CFmtStr( "#LOC_Date_MonthShort%d", pTime->tm_mon ) );
	const wchar_t *pwchDayOfWeek = FindSubLanguageSafe( CFmtStr( "#LOC_Date_Day%d", pTime->tm_wday ) );
	const wchar_t *pwchDayOfWeekShort = FindSubLanguageSafe( CFmtStr( "#LOC_Date_DayShort%d", pTime->tm_wday ) );
	const wchar_t *pwchDayNumFormat = LocFormatToPrintFormat( FindSubLanguage( "#LOC_Date_DayNumFormat" ), L"%d" );
	const wchar_t *pwchMonthNumFormat = LocFormatToPrintFormat( FindSubLanguage( "#LOC_Date_MonthNumFormat" ), L"%d" );
	const wchar_t *pwchYearNumFormat = LocFormatToPrintFormat( FindSubLanguage( "#LOC_Date_YearNumFormat" ), L"%04d" );
	const wchar_t *pwchHourNumFormat = LocFormatToPrintFormat( FindSubLanguage( "#LOC_Date_HourNumFormat" ), L"%d" );
	const wchar_t *pwchAM = FindSubLanguage( "#LOC_Date_AM" );
	const wchar_t *pwchPM = FindSubLanguage( "#LOC_Date_PM" );

	wchar_t wchYear[5];
	V_swprintf_safe( wchYear, pwchYearNumFormat,
		V_wcscmp( pwchYearNumFormat, L"%02d" ) == 0 ? (pTime->tm_year % 100) : pTime->tm_year + 1900 );

	wchar_t wchMonthNum[3];
	V_swprintf_safe( wchMonthNum, pwchMonthNumFormat, pTime->tm_mon + 1 );

	wchar_t wchDay[3];
	if( format != LOC_DATE_DAY_MONTH_YEAR_NUMERIC )
	{
		// All non-numeric formats use a plain day-of-month number.
		// In other words '05/04/03' uses the format but 'May 4' does not.
		pwchDayNumFormat = L"%d";
	}
	V_swprintf_safe( wchDay, pwchDayNumFormat, pTime->tm_mday );

	if( format == LOC_DATE_ECON_MONTH_SHORT_DAY_YEAR_HOUR_MINUTE_SECOND ||
		format == LOC_DATE_ECON_MONTH_SHORT_DAY_YEAR_HOUR_MINUTE_SECOND_GMT )
	{
		// Econ times always use a 24-hour clock.
		pwchAM = nullptr;
		pwchPM = nullptr;
	}

	int nHour = pTime->tm_hour;
	wchar_t wchHour[3];
	const wchar_t *pwchAMPM = nullptr;
	if( CheckAMPM( pwchAM ) || CheckAMPM( pwchPM ) )
	{
		pwchAMPM = pwchAM ? pwchAM : L"";
		if( nHour > 12 )
		{
			nHour -= 12;
			pwchAMPM = pwchPM ? pwchPM : L"";
		}
		else if( nHour < 1 )
		{
			nHour += 12;
		}
	}
	V_swprintf_safe( wchHour, pwchHourNumFormat, nHour );

	wchar_t wchMinute[3];
	V_swprintf_safe( wchMinute, L"%02d", pTime->tm_min );

	wchar_t wchSecond[3];
	V_swprintf_safe( wchSecond, L"%02d", pTime->tm_sec );

	if( !bAppend )
	{
		pUTF8Out[0] = 0;
	}

	const char *pToken = nullptr;
	const wchar_t *pwchStrings[8];
	int nStrings = 0;
	char szLocalUTF8[32];
	szLocalUTF8[0] = 0;

	switch( format )
	{
	case LOC_DATE_DAY_MONTH_YEAR_NUMERIC:
		pToken = "#LOC_Date_Format_Day_Month_Year_Numeric";
		nStrings = 3;
		pwchStrings[0] = wchDay;
		pwchStrings[1] = wchMonthNum;
		pwchStrings[2] = wchYear;
		break;

	case LOC_DATE_DAY_OF_WEEK_MONTH_DAY_YEAR:
		pToken = "#LOC_Date_Format_DayOfWeek_Month_Day_Year";
		nStrings = 4;
		pwchStrings[0] = pwchDayOfWeek;
		pwchStrings[1] = pwchMonth;
		pwchStrings[2] = wchDay;
		pwchStrings[3] = wchYear;
		break;

	case LOC_DATE_DAY_MONTH:
		pToken = "#LOC_Date_Format_Day_Month";
		nStrings = 2;
		pwchStrings[0] = wchDay;
		pwchStrings[1] = pwchMonth;
		break;

	case LOC_DATE_DAY_MONTH_SHORT:
		pToken = "#LOC_Date_Format_Day_Month";
		nStrings = 2;
		pwchStrings[0] = wchDay;
		pwchStrings[1] = pwchMonthShort;
		break;

	case LOC_DATE_DAY_MONTH_YEAR:
		pToken = "#LOC_Date_Format_Day_Month_Year";
		nStrings = 3;
		pwchStrings[0] = wchDay;
		pwchStrings[1] = pwchMonth;
		pwchStrings[2] = wchYear;
		break;

	case LOC_DATE_DAY_MONTH_YEAR_HOUR_MINUTE:
		pToken = pwchAMPM ? "#LOC_Date_Format_Day_Month_Year_Hour_Minute_12" : "#LOC_Date_Format_Day_Month_Year_Hour_Minute_24";
		nStrings = 6;
		pwchStrings[0] = wchDay;
		pwchStrings[1] = pwchMonth;
		pwchStrings[2] = wchYear;
		pwchStrings[3] = wchHour;
		pwchStrings[4] = wchMinute;
		pwchStrings[5] = pwchAMPM;
		break;

	case LOC_DATE_DAY_MONTH_YEAR_HOUR_MINUTE_SECOND:
		pToken = pwchAMPM ? "#LOC_Date_Format_Day_Month_Year_Hour_Minute_Second_12" : "#LOC_Date_Format_Day_Month_Year_Hour_Minute_Second_24";
		nStrings = 7;
		pwchStrings[0] = wchDay;
		pwchStrings[1] = pwchMonth;
		pwchStrings[2] = wchYear;
		pwchStrings[3] = wchHour;
		pwchStrings[4] = wchMinute;
		pwchStrings[5] = wchSecond;
		pwchStrings[6] = pwchAMPM;
		break;

	case LOC_DATE_DAY_OF_WEEK_SHORT_MONTH_SHORT_DAY_HOUR_MINUTE_SECOND:
		pToken = pwchAMPM ? "#LOC_Date_Format_DayOfWeekShort_MonthShort_Day_Hour_Minute_Second_12" : "#LOC_Date_Format_DayOfWeekShort_MonthShort_Day_Hour_Minute_Second_24";
		nStrings = 7;
		pwchStrings[0] = pwchDayOfWeekShort;
		pwchStrings[1] = pwchMonthShort;
		pwchStrings[2] = wchDay;
		pwchStrings[3] = wchHour;
		pwchStrings[4] = wchMinute;
		pwchStrings[5] = wchSecond;
		pwchStrings[6] = pwchAMPM;
		break;

	case LOC_DATE_DAY_OF_WEEK_MONTH_DAY_YEAR_HOUR_MINUTE_SECOND:
		pToken = pwchAMPM ? "#LOC_Date_Format_DayOfWeek_Month_Day_Year_Hour_Minute_Second_12" : "#LOC_Date_Format_DayOfWeek_Month_Day_Year_Hour_Minute_Second_24";
		nStrings = 8;
		pwchStrings[0] = pwchDayOfWeek;
		pwchStrings[1] = pwchMonth;
		pwchStrings[2] = wchDay;
		pwchStrings[3] = wchYear;
		pwchStrings[4] = wchHour;
		pwchStrings[5] = wchMinute;
		pwchStrings[6] = wchSecond;
		pwchStrings[7] = pwchAMPM;
		break;

	case LOC_DATE_HOUR_MINUTE:
		pToken = pwchAMPM ? "#LOC_Date_Format_Hour_Minute_12" : "#LOC_Date_Format_Hour_Minute_24";
		nStrings = 3;
		pwchStrings[0] = wchHour;
		pwchStrings[1] = wchMinute;
		pwchStrings[2] = pwchAMPM;
		break;

	case LOC_DATE_HOUR_MINUTE_SECOND:
		pToken = pwchAMPM ? "#LOC_Date_Format_Hour_Minute_Second_12" : "#LOC_Date_Format_Hour_Minute_Second_24";
		nStrings = 4;
		pwchStrings[0] = wchHour;
		pwchStrings[1] = wchMinute;
		pwchStrings[2] = wchSecond;
		pwchStrings[3] = pwchAMPM;
		break;

	case LOC_DATE_DAY_OF_WEEK:
		pToken = "#LOC_Date_Format_DayOfWeek";
		nStrings = 1;
		pwchStrings[0] = pwchDayOfWeek;
		break;

	case LOC_DATE_DAY_OF_WEEK_DAY_MONTH_HOUR_MINUTE:
		pToken = pwchAMPM ? "#LOC_Date_Format_DayOfWeek_Day_Month_Hour_Minute_12" : "#LOC_Date_Format_DayOfWeek_Day_Month_Hour_Minute_24";
		nStrings = 6;
		pwchStrings[0] = pwchDayOfWeek;
		pwchStrings[1] = wchDay;
		pwchStrings[2] = pwchMonth;
		pwchStrings[3] = wchHour;
		pwchStrings[4] = wchMinute;
		pwchStrings[5] = pwchAMPM;
		break;

	case LOC_DATE_ECON_MONTH_SHORT_DAY_YEAR_HOUR_MINUTE_SECOND:
	case LOC_DATE_ECON_MONTH_SHORT_DAY_YEAR_HOUR_MINUTE_SECOND_GMT:
		pToken = format == LOC_DATE_ECON_MONTH_SHORT_DAY_YEAR_HOUR_MINUTE_SECOND_GMT ?
			"#LOC_Date_Format_Econ_MonthShort_Day_Year_Hour_Minute_Second_GMT" : "#LOC_Date_Format_Econ_MonthShort_Day_Year_Hour_Minute_Second";
		nStrings = 6;
		pwchStrings[0] = pwchMonthShort;
		pwchStrings[1] = wchDay;
		pwchStrings[2] = wchYear;
		pwchStrings[3] = wchHour;
		pwchStrings[4] = wchMinute;
		pwchStrings[5] = wchSecond;
		break;

	case LOC_DATE_NEUTRAL_TIMESTAMP:
		// This is deliberately language-neutral.
		V_sprintf_safe( szLocalUTF8, "%04d_%02d_%02d_%02d_%02d_%02d",
			pTime->tm_year + 1900,
			pTime->tm_mon + 1,
			pTime->tm_mday,
			pTime->tm_hour,
			pTime->tm_min,
			pTime->tm_sec );
		V_strncat( pUTF8Out, szLocalUTF8, nOutSizeInBytes );
		return pUTF8Out;

	default:
		V_sprintf_safe( szLocalUTF8, "<invalid date format %d>", format );
		V_strncat( pUTF8Out, szLocalUTF8, nOutSizeInBytes );
		return pUTF8Out;
	}

	Assert( nStrings <= V_ARRAYSIZE( pwchStrings ) );
	ConstructString_Impl( pUTF8Out, nOutSizeInBytes, FindSubLanguageSafe( pToken ), nStrings, pwchStrings, true );

	return pUTF8Out;
}

const char *CLocalize::ConstructDateString( char *pUTF8Out, int nOutSizeInBytes, LOC_DATE_FORMAT format, uint64 nTime, bool bAppend )
{
	time_t timeVal = (time_t)nTime;
	struct tm timeVals;

	if( format == LOC_DATE_ECON_MONTH_SHORT_DAY_YEAR_HOUR_MINUTE_SECOND_GMT )
	{
		Plat_gmtime( &timeVal, &timeVals );
	}
	else
	{
		Plat_localtime( &timeVal, &timeVals );
	}

	return ConstructDateString( pUTF8Out, nOutSizeInBytes, format, &timeVals, bAppend );
}

const char *CLocalize::ConstructRelativeDateString( char *pUTF8Out, int nOutSizeInBytes, LOC_DATE_FORMAT fallbackFormat, uint64 nRelativeTime, bool bAppend )
{
	if( !pUTF8Out || (nOutSizeInBytes < 1) )
		return NULL;

	if( !bAppend )
	{
		pUTF8Out[0] = 0;
	}

	struct tm tmThen;
	struct tm tmNow;

	time_t tThen = (time_t)nRelativeTime;
	time_t tNow = (time_t)Plat_GetTime();

	if( !Plat_localtime( &tThen, &tmThen ) ||
		!Plat_localtime( &tNow, &tmNow ) )
	{
		return nullptr;
	}

	// in order to make an easier comparison, get an absolute 'days since epoch'. we don't care
	// about leap years, because those will be reflected in tm_yday and we don't perform any
	// checks as long as jan 1 - feb 29.
	int daysThen = tmThen.tm_year * 365 + tmThen.tm_yday;
	int daysNow = tmNow.tm_year * 365 + tmNow.tm_yday;

	//
	// here's the app logic
	//
	char* pUTF8Append = pUTF8Out + V_strlen( pUTF8Out );
	int nAppendSizeInBytes = nOutSizeInBytes - (pUTF8Append - pUTF8Out);
	switch( daysNow - daysThen )
	{
	case 0:
		V_UnicodeToUTF8( FindSubLanguageSafe( "LOC_Date_Today" ), pUTF8Append, nAppendSizeInBytes );
		return pUTF8Out;
	case 1:
		V_UnicodeToUTF8( FindSubLanguageSafe( "LOC_Date_Yesterday" ), pUTF8Append, nAppendSizeInBytes );
		return pUTF8Out;
	case -1:
		V_UnicodeToUTF8( FindSubLanguageSafe( "LOC_Date_Tomorrow" ), pUTF8Append, nAppendSizeInBytes );
		return pUTF8Out;
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
		// in the past, fewer than 7 days ago - fall through
	case -2:
	case -3:
	case -4:
	case -5:
	case -6:
		// in the future, fewer than 7 days from now, use the specific
		// day of the week string.
		fallbackFormat = LOC_DATE_DAY_OF_WEEK;
		break;
	default:
		// some date that's more than a week in the past or future; just use
		// the given fallback date format.
		break;
	}

	// delegate to full date implementation.
	return ConstructDateString( pUTF8Out, nOutSizeInBytes, fallbackFormat, nRelativeTime, true );
}

const char *CLocalize::ConstructRelativeTimeString( char *pUTF8Out, int nOutSizeInBytes, LOC_DATE_FORMAT timeFormat, LOC_DATE_FORMAT fallbackFormat, uint64 nRelativeTime, bool bAppend )
{
	if( !pUTF8Out || (nOutSizeInBytes < 1) )
		return NULL;

	if( !bAppend )
	{
		pUTF8Out[0] = 0;
	}

	struct tm tmThen;
	struct tm tmNow;

	time_t tThen = (time_t)nRelativeTime;
	time_t tNow = (time_t)Plat_GetTime();

	if( !Plat_localtime( &tThen, &tmThen ) ||
		!Plat_localtime( &tNow, &tmNow ) )
	{
		return nullptr;
	}

	// in order to make an easier comparison, get an absolute 'days since epoch'. we don't care
	// about leap years, because those will be reflected in tm_yday and we don't perform any
	// checks as long as jan 1 - feb 29.
	int daysThen = tmThen.tm_year * 365 + tmThen.tm_yday;
	int daysNow = tmNow.tm_year * 365 + tmNow.tm_yday;

	// If we're not on the same day, fall back to a relative date string
	if( daysNow != daysThen )
		return ConstructRelativeDateString( pUTF8Out, nOutSizeInBytes, fallbackFormat, nRelativeTime, true );

	// Otherwise, print a time string
	return ConstructDateString( pUTF8Out, nOutSizeInBytes, timeFormat, nRelativeTime, true );
}

void CLocalize::AppendDurationUnit( char *pUTF8Out, int nOutSizeInBytes, int nUnits, const char *pTokenNotOne, const char *pTokenOne )
{
	if( !pUTF8Out || (nOutSizeInBytes < 1) )
		return;

	// If output not empty, append a space
	if( pUTF8Out[0] )
	{
		V_strncat( pUTF8Out, " ", nOutSizeInBytes );
	}

	char szLocalUTF8[256];
	V_sprintf_safe( szLocalUTF8, "%d", nUnits );
	V_strncat( pUTF8Out, szLocalUTF8, nOutSizeInBytes );

	const wchar_t *pwchUnitName = FindSubLanguage( nUnits != 1 ? pTokenNotOne : pTokenOne );
	if( pwchUnitName && *pwchUnitName )
	{
		V_strncat( pUTF8Out, " ", nOutSizeInBytes );
		V_UnicodeToUTF8( pwchUnitName, szLocalUTF8, sizeof( szLocalUTF8 ) );
		V_strncat( pUTF8Out, szLocalUTF8, nOutSizeInBytes );
	}
}

const char *CLocalize::ConstructDurationString( char *pUTF8Out, int nOutSizeInBytes, LOC_DURATION_FORMAT format, int nSeconds, bool bAppend )
{
	if( !pUTF8Out || (nOutSizeInBytes < 1) )
		return NULL;

	if( !bAppend )
	{
		pUTF8Out[0] = 0;
	}

	int ss = abs( nSeconds );
	int mm = ss / 60; ss -= mm * 60;
	int hh = mm / 60; mm -= hh * 60;
	int days = hh / 24; hh -= days * 24;
	if( nSeconds < 0 )
	{
		days = -days;
	}

	char szLocalUTF8[32];
	szLocalUTF8[0] = 0;

	switch( format )
	{
	case LOC_DURATION_DAYS_SHORTEST_OPT_HOURS_MINUTES_SECONDS:
	case LOC_DURATION_DAYS_HOURS_MINUTES_SECONDS:
	case LOC_DURATION_HOURS_MINUTES_SECONDS:
	case LOC_DURATION_HOURS_OPT_MINUTES_SECONDS:
	case LOC_DURATION_MINUTES_SECONDS:
	{
		wchar_t wchHH[3];
		V_swprintf_safe( wchHH, L"%d", hh );

		wchar_t wchMM[3];
		V_swprintf_safe( wchMM, L"%02d", mm );

		wchar_t wchSS[3];
		V_swprintf_safe( wchSS, L"%02d", ss );

		wchar_t wchDays[16];
		V_swprintf_safe( wchDays, L"%d", days );

		if( format == LOC_DURATION_DAYS_HOURS_MINUTES_SECONDS ||
			(days != 0 &&
			(format == LOC_DURATION_DAYS_SHORTEST_OPT_HOURS_MINUTES_SECONDS)) )
		{
			AppendConstructString( pUTF8Out, nOutSizeInBytes, FindSubLanguageSafe( "#LOC_Duration_dhhmmss" ), 4,
				wchDays, wchHH, wchMM, wchSS );
		}
		else if( format == LOC_DURATION_DAYS_SHORTEST_OPT_HOURS_MINUTES_SECONDS ||
			format == LOC_DURATION_HOURS_MINUTES_SECONDS ||
			(hh != 0 &&
			(format == LOC_DURATION_HOURS_OPT_MINUTES_SECONDS)) )
		{
			if( nSeconds < 0 )
			{
				V_swprintf_safe( wchHH, L"%d", -hh );
			}

			AppendConstructString( pUTF8Out, nOutSizeInBytes, FindSubLanguageSafe( "#LOC_Duration_hhmmss" ), 3,
				wchHH, wchMM, wchSS );
		}
		else
		{
			if( nSeconds < 0 )
			{
				V_swprintf_safe( wchMM, L"%d", -mm );
			}

			AppendConstructString( pUTF8Out, nOutSizeInBytes, FindSubLanguageSafe( "#LOC_Duration_mmss" ), 2,
				wchMM, wchSS );
		}
		break;
	}

	case LOC_DURATION_DAYS:
		AppendDurationUnit( pUTF8Out, nOutSizeInBytes, days, "#LOC_Duration_Days", "#LOC_Duration_Day" );
		break;

	case LOC_DURATION_LARGEST_NEEDED_DAYS_HOURS_MINUTES_SECONDS_SUFFIXED:
	case LOC_DURATION_LARGEST_NEEDED_DAYS_HOURS_MINUTES_SUFFIXED:
	case LOC_DURATION_LARGEST_NEEDED_HOURS_MINUTES_SECONDS_SUFFIXED:
	case LOC_DURATION_LARGEST_NEEDED_HOURS_MINUTES_SUFFIXED:
	{
		wchar_t wchHH[3];
		V_swprintf_safe( wchHH, L"%d", hh );

		wchar_t wchMM[3];
		V_swprintf_safe( wchMM, L"%d", mm );

		wchar_t wchSS[3];
		V_swprintf_safe( wchSS, L"%d", ss );

		wchar_t wchDays[16];
		V_swprintf_safe( wchDays, L"%d", days );

		const char *pTokens[2] = { 0 };
		const wchar_t *pwchStrings[4];
		int nStartString = 0;

		pwchStrings[0] = wchDays;
		pwchStrings[1] = wchHH;
		pwchStrings[2] = wchMM;
		pwchStrings[3] = wchSS;

		if( days &&
			(format == LOC_DURATION_LARGEST_NEEDED_DAYS_HOURS_MINUTES_SECONDS_SUFFIXED ||
				format == LOC_DURATION_LARGEST_NEEDED_DAYS_HOURS_MINUTES_SUFFIXED) )
		{
			pTokens[0] = "#LOC_Duration_d_h_m_s";
			pTokens[1] = "#LOC_Duration_d_h_m";
			nStartString = 0;
		}
		else if( hh )
		{
			if( nSeconds < 0 )
			{
				V_swprintf_safe( wchHH, L"%d", -hh );
			}

			pTokens[0] = "#LOC_Duration_h_m_s";
			pTokens[1] = "#LOC_Duration_h_m";
			nStartString = 1;
		}
		else if( mm ||
			(format == LOC_DURATION_LARGEST_NEEDED_DAYS_HOURS_MINUTES_SUFFIXED ||
				format == LOC_DURATION_LARGEST_NEEDED_HOURS_MINUTES_SUFFIXED) )
		{
			if( nSeconds < 0 )
			{
				V_swprintf_safe( wchMM, L"%d", -mm );
			}

			pTokens[0] = "#LOC_Duration_m_s";
			pTokens[1] = "#LOC_Duration_m";
			nStartString = 2;
		}
		else
		{
			if( nSeconds < 0 )
			{
				V_swprintf_safe( wchSS, L"%d", -ss );
			}

			pTokens[0] = "#LOC_Duration_s";
			pTokens[1] = "#LOC_Duration_s";
			nStartString = 3;
		}

		Assert( nStartString < V_ARRAYSIZE( pwchStrings ) );
		int nStrings = V_ARRAYSIZE( pwchStrings ) - nStartString;

		const char *pToken;
		if( format == LOC_DURATION_LARGEST_NEEDED_DAYS_HOURS_MINUTES_SUFFIXED ||
			format == LOC_DURATION_LARGEST_NEEDED_HOURS_MINUTES_SUFFIXED )
		{
			pToken = pTokens[1];
			nStrings--;
			Assert( nStrings >= 1 );
		}
		else
		{
			pToken = pTokens[0];
		}

		ConstructString_Impl( pUTF8Out, nOutSizeInBytes, FindSubLanguageSafe( pToken ), nStrings, &pwchStrings[nStartString], true );
		break;
	}

	case LOC_DURATION_NONZERO_HOURS_MINUTES:
	case LOC_DURATION_NONZERO_HOURS_MINUTES_SECONDS:
	case LOC_DURATION_NONZERO_HOURS_MINUTES_SECONDS_EXTRA:
	case LOC_DURATION_NONZERO_DAYS_HOURS_MINUTES:
	case LOC_DURATION_NONZERO_DAYS_HOURS_MINUTES_SECONDS:
	case LOC_DURATION_NONZERO_DAYS_HOURS_MINUTES_SECONDS_EXTRA:
	{
		int nUnits = 0;
		if( days &&
			(format == LOC_DURATION_NONZERO_DAYS_HOURS_MINUTES ||
				format == LOC_DURATION_NONZERO_DAYS_HOURS_MINUTES_SECONDS ||
				format == LOC_DURATION_NONZERO_DAYS_HOURS_MINUTES_SECONDS_EXTRA) )
		{
			AppendDurationUnit( pUTF8Out, nOutSizeInBytes, days, "#LOC_Duration_Days", "#LOC_Duration_Day" );
			nUnits++;
		}

		if( hh )
		{
			AppendDurationUnit( pUTF8Out, nOutSizeInBytes, (nSeconds >= 0 || nUnits > 0) ? hh : -hh, "#LOC_Duration_Hours", "#LOC_Duration_Hour" );
			nUnits++;
		}

		if( mm )
		{
			AppendDurationUnit( pUTF8Out, nOutSizeInBytes, (nSeconds >= 0 || nUnits > 0) ? mm : -mm, "#LOC_Duration_Minutes", "#LOC_Duration_Minute" );
			nUnits++;
		}

		if( (ss || nUnits == 0) &&
			(format == LOC_DURATION_NONZERO_HOURS_MINUTES_SECONDS ||
				format == LOC_DURATION_NONZERO_DAYS_HOURS_MINUTES_SECONDS ||
				(nUnits < 2 &&
				(format == LOC_DURATION_NONZERO_HOURS_MINUTES_SECONDS_EXTRA ||
					format == LOC_DURATION_NONZERO_DAYS_HOURS_MINUTES_SECONDS_EXTRA))) )
		{
			AppendDurationUnit( pUTF8Out, nOutSizeInBytes, (nSeconds >= 0 || nUnits > 0) ? ss : -ss, "#LOC_Duration_Seconds", "#LOC_Duration_Second" );
		}
		break;
	}

	default:
		V_sprintf_safe( szLocalUTF8, "<invalid duration format %d>", format );
		V_strncat( pUTF8Out, szLocalUTF8, nOutSizeInBytes );
	}

	return pUTF8Out;
}

static const char *CheckLocNumberString( const char *_pIn )
{
	const uint8 *pIn = (uint8*)_pIn;
	// Localizers are putting non-breaking-space in the localization
	// files for languages that use a space for grouping (Russian, for example)
	// but this doesn't render properly.  Convert it into a regular space.
	if( pIn[0] == 0xc2 && pIn[1] == 0xa0 && pIn[2] == 0 )
	{
		return " ";
	}

	if( V_strcmp( _pIn, "<none>" ) == 0 )
	{
		return "";
	}
	if( V_strcmp( _pIn, "<space>" ) == 0 )
	{
		return " ";
	}

	return _pIn;
}

const char *CLocalize::ConstructNumberString( char *pUTF8Out, int nOutSizeInBytes, LOC_NUMBER_FORMAT format, uint64 nValue, bool bAppend )
{
	if( !pUTF8Out || (nOutSizeInBytes < 1) )
		return NULL;

	if( !bAppend )
	{
		pUTF8Out[0] = 0;
	}

	bool bNegSign = false;
	char szLocalUTF8[128];
	szLocalUTF8[0] = 0;

	switch( format )
	{
	case LOC_NUMBER_SIGNED:
		if( (int64)nValue < 0 )
		{
			bNegSign = true;
			nValue = -(int64)nValue;
		}
		break;

	case LOC_NUMBER_UNSIGNED:
		break;

	default:
		V_sprintf_safe( szLocalUTF8, "<invalid integral number format %d>", format );
		V_strncat( pUTF8Out, szLocalUTF8, nOutSizeInBytes );
		return pUTF8Out;
	}

	if( nValue == 0 )
	{
		V_strncat( pUTF8Out, "0", nOutSizeInBytes );
		return pUTF8Out;
	}

	const wchar_t *pwchNegative = FindSubLanguageSafe( "LOC_Number_NegativeSign" );
	const wchar_t *pwchGrouping = FindSubLanguageSafe( "LOC_Number_Grouping" );
	int nGroupingCount = 3;

	//
	// Build the number string in reverse, then we'll flip it.
	// As doing this with a fixed buffer could lead to tricky
	// truncation issues we build into a local buffer.
	//

	int nCurGroup = 0;
	char* pUTF8Append = szLocalUTF8;
	int nAppendSizeInBytes = sizeof( szLocalUTF8 );

	for( ;;)
	{
		if( nAppendSizeInBytes > 1 )
		{
			char chDigit = (char)(((uint32)nValue % 10) + '0');
			*pUTF8Append++ = chDigit;
			--nAppendSizeInBytes;
			*pUTF8Append = 0;
		}

		nValue /= 10;
		if( !nValue )
		{
			break;
		}

		if( ++nCurGroup >= nGroupingCount )
		{
			UTF8Append( pwchGrouping, &pUTF8Append, &nAppendSizeInBytes );
			nCurGroup = 0;
		}
	}

	if( bNegSign )
	{
		UTF8Append( pwchNegative, &pUTF8Append, &nAppendSizeInBytes );
	}

	char *pDst = pUTF8Out + V_strlen( pUTF8Out );
	int nChars = nOutSizeInBytes - (pDst - pUTF8Out) - 1;
	nChars = MIN( nChars, V_strlen( szLocalUTF8 ) );
	if( nChars > 0 )
	{
		// Copy and reverse bytes from the local buffer.
		// In truncation situations this may not copy the full
		// local buffer but we start the copy with most-significant-chars
		// so it's the same truncation as if we built the string in order.
		const char *pSrc = pUTF8Append - 1;
		while( nChars-- > 0 )
		{
			*pDst++ = *pSrc--;
		}
	}
	//Ensure null termination
	*pDst = 0;
	return pUTF8Out;
}

const char *CLocalize::ConstructNumberString( char *pUTF8Out, int nOutSizeInBytes, LOC_NUMBER_FORMAT format, double flValue, int nPrecision, bool bAppend )
{
	if( !pUTF8Out || (nOutSizeInBytes < 1) )
		return NULL;

	if( !bAppend )
	{
		pUTF8Out[0] = 0;
	}

	char szLocalUTF8[128];
	szLocalUTF8[0] = 0;
	if( nPrecision < 0 )
	{
		V_sprintf_safe( szLocalUTF8, "<invalid precision %d>", nPrecision );
		V_strncat( pUTF8Out, szLocalUTF8, nOutSizeInBytes );
		return pUTF8Out;
	}

	bool bNegSign = false;

	switch( format )
	{
	case LOC_NUMBER_SIGNED:
		if( flValue < 0 )
		{
			bNegSign = true;
			flValue = -flValue;
		}
		break;

	case LOC_NUMBER_MONEY:
		// Defined to always be the absolute value so that
		// higher-level code can wrap a negative number in whatever
		// currency-appropriate way it needs to.
		if( flValue < 0 )
		{
			flValue = -flValue;
		}
		break;

	default:
		V_sprintf_safe( szLocalUTF8, "<invalid floating-point number format %d>", format );
		V_strncat( pUTF8Out, szLocalUTF8, nOutSizeInBytes );
		return pUTF8Out;
	}

	const wchar_t *pwchNegative = FindSubLanguageSafe( "LOC_Number_NegativeSign" );

	// Early-out for unusual values.
	if( IS_NAN_DOUBLE( flValue ) )
	{
		V_strncat( pUTF8Out, "NaN", nOutSizeInBytes );
		return pUTF8Out;
	}
	else if( flValue == 0.0f )
	{
		V_strncat( pUTF8Out, "0", nOutSizeInBytes );
		return pUTF8Out;
	}

	const wchar_t *pwchGrouping = FindSubLanguageSafe( "LOC_Number_Grouping" );
	const wchar_t *pwchDecimalPoint = FindSubLanguageSafe( "LOC_Number_DecimalPoint" );
	int nGroupingCount = 3;

	//
	// Build the number string in reverse, then we'll flip it.
	// As doing this with a fixed buffer could lead to tricky
	// truncation issues we build into a local buffer.
	//

	if( nPrecision > 0 )
	{
		flValue *= pow( 10.0, nPrecision );
	}
	flValue += 0.5;

	// In order to get a clean conversion we turn the value into
	// an integer. This limits the range of numbers we can handle
	// but that shouldn't be an issue as very large numbers
	// are probably better displayed in a different format, such
	// as scientific notation.
	if( flValue >= UINT64_MAX )
	{
		V_strncat( pUTF8Out, "<floating-point value out of range>", nOutSizeInBytes );
		return pUTF8Out;
	}
	uint64 nValue = (uint64)flValue;
	char* pUTF8Append = szLocalUTF8;
	int nAppendSizeInBytes = sizeof( szLocalUTF8 );

	if( nPrecision > 0 )
	{
		while( nPrecision-- > 0 )
		{
			if( nAppendSizeInBytes > 1 )
			{
				char chDigit = (char)(((uint32)nValue % 10) + '0');
				*pUTF8Append++ = chDigit;
				--nAppendSizeInBytes;
				*pUTF8Append = 0;
			}

			nValue /= 10;
		}

		// Append decimal point
		UTF8Append( pwchDecimalPoint, &pUTF8Append, &nAppendSizeInBytes );
	}

	int nCurGroup = 0;
	for( ;;)
	{
		if( nAppendSizeInBytes > 1 )
		{
			char chDigit = (char)(((uint32)nValue % 10) + '0');
			*pUTF8Append++ = chDigit;
			--nAppendSizeInBytes;
			*pUTF8Append = 0;
		}

		nValue /= 10;
		if( !nValue )
		{
			break;
		}

		if( ++nCurGroup >= nGroupingCount )
		{
			// Append grouping
			UTF8Append( pwchGrouping, &pUTF8Append, &nAppendSizeInBytes );
			nCurGroup = 0;
		}
	}

	if( bNegSign )
	{
		// Append negsign
		UTF8Append( pwchNegative, &pUTF8Append, &nAppendSizeInBytes );
	}

	char *pDst = pUTF8Out + V_strlen( pUTF8Out );
	int nChars = nOutSizeInBytes - (pDst - pUTF8Out) - 1;
	nChars = MIN( nChars, V_strlen( szLocalUTF8 ) );
	if( nChars > 0 )
	{
		// Copy and reverse bytes from the local buffer.
		// In truncation situations this may not copy the full
		// local buffer but we start the copy with most-significant-chars
		// so it's the same truncation as if we built the string in order.
		const char *pSrc = pUTF8Append - 1;
		while( nChars-- > 0 )
		{
			*pDst++ = *pSrc--;
		}
	}
	//Ensure null termination
	*pDst = 0;
	return pUTF8Out;
}

const char *CLocalize::ConstructOrdinalString( char *pUTF8Out, int nOutSizeInBytes, LOC_ORDINAL_FORMAT format, uint32 nValue, bool bAppend )
{
	if( !pUTF8Out || (nOutSizeInBytes < 1) )
		return NULL;

	if( !bAppend )
	{
		pUTF8Out[0] = 0;
	}

	char szLocalUTF8[64];
	szLocalUTF8[0] = 0;

	switch( format )
	{
	case LOC_ORDINAL_DIGITS_AND_TEXT:
		break;

	default:
		V_sprintf_safe( szLocalUTF8, "<invalid ordinal format %d>", format );
		V_strncat( pUTF8Out, szLocalUTF8, nOutSizeInBytes );

		return pUTF8Out;
	}

	uint32 nSelector = nValue % 100;

	const wchar_t *pwchPrefix = FindSubLanguage( CFmtStr( "LOC_Ordinal_Prefix_%u", nSelector ) );
	if( !pwchPrefix )
	{
		pwchPrefix = FindSubLanguage( "LOC_Ordinal_Prefix_Default" );
	}
	if( pwchPrefix )
	{
		V_UnicodeToUTF8( pwchPrefix, szLocalUTF8, sizeof(szLocalUTF8) );
		V_strncat( pUTF8Out, szLocalUTF8, nOutSizeInBytes );
	}

	ConstructNumberString( pUTF8Out, nOutSizeInBytes, LOC_NUMBER_UNSIGNED, nValue, true );

	const wchar_t *pszSuffix = FindSubLanguage( CFmtStr( "LOC_Ordinal_Suffix_%u", nSelector ) );
	if( !pszSuffix )
	{
		pszSuffix = FindSubLanguage( "LOC_Ordinal_Suffix_Default" );
	}
	if( pszSuffix )
	{
		V_UnicodeToUTF8( pszSuffix, szLocalUTF8, sizeof( szLocalUTF8 ) );
		V_strncat( pUTF8Out, szLocalUTF8, nOutSizeInBytes );
	}

	return pUTF8Out;
}

const char *CLocalize::ConstructMoneyString( char *pUTF8Out, int nOutSizeInBytes, uint32 nValue, ECurrency eCurrency, int nOverridePrecision, bool bAppend )
{
	if( !pUTF8Out || (nOutSizeInBytes < 1) )
		return NULL;

	if( !bAppend )
	{
		pUTF8Out[0] = 0;
	}

	char szLocalUTF8[32];
	if( (int)eCurrency < 0 || (int)eCurrency >= k_ECurrencyMax )
	{
		V_sprintf_safe( szLocalUTF8, "<invalid currency %d>", eCurrency );
		V_strncat( pUTF8Out, szLocalUTF8, nOutSizeInBytes );
		return pUTF8Out;
	}

	CurrencyMoneyStringConfiguration_t currencyConfig = GetCurrencyMoneyStringConfiguration( eCurrency );

	int nPrecision;
	if( nOverridePrecision >= 0 )
	{
		nPrecision = nOverridePrecision;
	}
	// Don't display fractional rubles (but if our amount is fractional, we should show the fraction regardless).
	else if( currencyConfig.m_eDenominationFraction == CurrencyMoneyStringConfiguration_t::k_EWholeUnitsOnly &&
		(nValue % 100) == 0 )
	{
		nPrecision = 0;
	}
	else
	{
		nPrecision = 2;
	}

	if( currencyConfig.m_eSymbolPlacementPolicy == CurrencyMoneyStringConfiguration_t::k_ESymbolFirstThenAmount )
	{
		V_strncat( pUTF8Out, currencyConfig.m_pchSymbol, nOutSizeInBytes );
	}
	else
	{
		ConstructNumberString( pUTF8Out, nOutSizeInBytes, LOC_NUMBER_MONEY, (double)nValue / 100.0, nPrecision, true );
	}

	if( currencyConfig.m_eSpaceBetweenTokens == CurrencyMoneyStringConfiguration_t::k_ESpaceBetweenSymbolAndAmount )
	{
		V_strncat( pUTF8Out, " ", nOutSizeInBytes );
	}

	if( currencyConfig.m_eSymbolPlacementPolicy == CurrencyMoneyStringConfiguration_t::k_EAmountFirstThenSymbol )
	{
		V_strncat( pUTF8Out, currencyConfig.m_pchSymbol, nOutSizeInBytes );
	}
	else
	{
		ConstructNumberString( pUTF8Out, nOutSizeInBytes, LOC_NUMBER_MONEY, (double)nValue / 100.0, nPrecision, true );
	}

	return pUTF8Out;
}

wchar_t* CLocalize::GetAsianFrequencySequence( const char * pLanguage )
{
	if( !m_bAsianFrequencySequenceLoaded )
	{
		m_bAsianFrequencySequenceLoaded = true;
		char szFileName[128];
		V_snprintf( szFileName, sizeof( szFileName ), "resource/%s_frequency.txt", pLanguage );
		g_pFullFileSystem->ReadFile( szFileName, "GAME", m_bufAsianFrequencySequence );
		uint nSize = m_bufAsianFrequencySequence.TellPut() / sizeof( wchar_t );
		m_bufAsianFrequencySequence.PutUnsignedShort( 0 ); // 0-terminate

		wchar_t * pAsianFrequencySequence = (wchar_t*) m_bufAsianFrequencySequence.Base();
		// transcode from LT Unicode to GT Unicode
		if( pAsianFrequencySequence[0] == 0xFFFE )
		{
			// switch from little-endian
			for( uint i = 0; i < nSize; ++i )
			{
				wchar_t &refChar = pAsianFrequencySequence[i];
				refChar = ( refChar >> 8 ) | ( refChar << 8 );
			}
		}
	}	

	if( m_bufAsianFrequencySequence.TellPut() > 2 )
	{
		wchar_t * pAsianFrequencySequence = (wchar_t*) m_bufAsianFrequencySequence.Base();

		if( pAsianFrequencySequence[0] == 0xFEFF )
		{
			return pAsianFrequencySequence + 1;
		}
		return pAsianFrequencySequence;
	}
	return NULL;
}


#if defined( GNUC ) || defined( _WIN64 )
#define _INTSIZEOF(n)   ((sizeof(n) + sizeof(intp) - 1) & ~(sizeof(intp) - 1)) 
#endif

#define va_argByIndex(ap,t,i)    ( *(t *)(ap + i * _INTSIZEOF(t)) )

//-----------------------------------------------------------------------------
// Purpose: construct string helper
//-----------------------------------------------------------------------------
template < typename T >
void ConstructStringVArgsInternal_Impl(T *unicodeOutput, int unicodeBufferSizeInBytes, const T *formatString, int numFormatParameters, va_list argList)
{
	// Safety check
	if ( unicodeOutput == NULL || unicodeBufferSizeInBytes < 1 )
	{
		return;
	}
	if (!formatString)
	{
		unicodeOutput[0] = 0;
		return;
	}

	int unicodeBufferSize = unicodeBufferSizeInBytes / sizeof(T);
	const T *searchPos = formatString;
	T *outputPos = unicodeOutput;

	//assumes we can't have %s10
	//assume both are 0 terminated?
	int formatLength = StringFuncs<T>::Length( formatString );

#ifdef PLATFORM_64BITS
	// On 64 bits, va_list does not just point to a contiguous blob of parameters
	// so extract into an array here.
	// TODO: this code is probably fast enough and efficient enough to use
	// on all platforms, so consider enabling it everywhere.
	T** arguments = (T**)stackalloc( sizeof(T*)*numFormatParameters );
	if ( IsPC() )
	{
		for ( int i = 0; i < numFormatParameters; ++i )
		{
			arguments[i] = va_arg( argList, T* );
		}
	}
	
#endif

#ifdef _DEBUG
	int curArgIdx = 0;
#endif

	while ( searchPos[0] != '\0' && unicodeBufferSize > 1 )
	{
		if ( formatLength >= 3 && searchPos[0] == '%' && searchPos[1] == 's' )
		{
			//this is an escape sequence - %s1, %s2 etc, up to %s9

			int argindex = ( searchPos[2] ) - '0' - 1;

			if ( argindex < 0 || argindex > 9 )
			{
				Warning( "Bad format string in CLocalizeStringTable::ConstructString\n" );
				*outputPos = '\0';
				return;
			}

			if ( argindex < numFormatParameters )
			{
				T *param = NULL;
				if ( IsPC() )
				{
#if !defined( _PS3 )
#ifdef PLATFORM_64BITS
					param = arguments[ argindex ];
#else
					param = va_argByIndex( argList, T *, argindex );
#endif
#endif // !_PS3
				}
				else
				{
					// X360TBD: convert string to new %var% format if this assert hits
					Assert( argindex == curArgIdx++ );
					param = va_arg( argList, T* );
				}

				if (!param)
				{
					Assert( !("ConstructStringVArgsInternal_Impl() - Found a %s# escape sequence whose index was more than the number of args.") );
					*outputPos = '\0';
					return;
				}


				int paramSize = StringFuncs<T>::Length(param);
				if (paramSize >= unicodeBufferSize)
				{
					paramSize = unicodeBufferSize - 1;
				}

				memcpy(outputPos, param, paramSize * sizeof(T));

				unicodeBufferSize -= paramSize;
				outputPos += paramSize;

				searchPos += 3;
				formatLength -= 3;
			}
			else
			{
				//copy it over, char by char
				*outputPos = *searchPos;

				outputPos++;
				unicodeBufferSize--;

				searchPos++;
				formatLength--;
			}
		}
		else
		{
			//copy it over, char by char
			*outputPos = *searchPos;

			outputPos++;
			unicodeBufferSize--;

			searchPos++;
			formatLength--;
		}
	}

	// ensure null termination
	Assert( outputPos - unicodeOutput < unicodeBufferSizeInBytes/sizeof(T) );
	*outputPos = L'\0';
}

void CLocalize::ConstructStringVArgsInternal(char *unicodeOutput, int unicodeBufferSizeInBytes, const char *formatString, int numFormatParameters, va_list argList)
{
	ConstructStringVArgsInternal_Impl<char>( unicodeOutput, unicodeBufferSizeInBytes, formatString, numFormatParameters, argList );
}

void CLocalize::ConstructStringVArgsInternal(wchar_t *unicodeOutput, int unicodeBufferSizeInBytes, const wchar_t *formatString, int numFormatParameters, va_list argList)
{
	ConstructStringVArgsInternal_Impl<wchar_t>( unicodeOutput, unicodeBufferSizeInBytes, formatString, numFormatParameters, argList );
}

//-----------------------------------------------------------------------------
// Purpose: construct string helper
//-----------------------------------------------------------------------------
template < typename T >
const T *GetTypedKeyValuesString( KeyValues *pKeyValues, const char *pKeyName );

template < >
const char *GetTypedKeyValuesString<char>( KeyValues *pKeyValues, const char *pKeyName )
{
	return pKeyValues->GetString( pKeyName, "[unknown]" );
}

template < >
const wchar_t *GetTypedKeyValuesString<wchar_t>( KeyValues *pKeyValues, const char *pKeyName )
{
	return pKeyValues->GetWString( pKeyName, L"[unknown]" );
}

template < typename T >
void ConstructStringKeyValuesInternal_Impl( T *unicodeOutput, int unicodeBufferSizeInBytes, const T *formatString, KeyValues *localizationVariables )
{
	T *outputPos = unicodeOutput;

	//assumes we can't have %s10
	//assume both are 0 terminated?
	int unicodeBufferSize = unicodeBufferSizeInBytes / sizeof(T);

	while ( *formatString != '\0' && unicodeBufferSize > 0 )
	{
		bool shouldAdvance = true;

		if ( *formatString == '%' )
		{
			// this is an escape sequence that specifies a variable name
			if ( formatString[1] == 's' && formatString[2] >= '0' && formatString[2] <= '9' )
			{
				// old style escape sequence, ignore
			}
			else if ( formatString[1] == '%' )
			{
				// just a '%' char, just write the second one
				formatString++;
			}
			else if ( localizationVariables )
			{
				// get out the variable name
				const T *varStart = formatString + 1;
				const T *varEnd = StringFuncs<T>::FindChar( varStart, '%' );

				if ( varEnd && *varEnd == '%' )
				{
					shouldAdvance = false;

					// assume variable names must be ascii, do a quick convert
					char variableName[32];
					char *vset = variableName;
					for ( const T *pws = varStart; pws < varEnd && (vset < variableName + sizeof(variableName) - 1); ++pws, ++vset )
					{
						*vset = (char)*pws;
					}
					*vset = 0;

					// look up the variable name
					const T *value = GetTypedKeyValuesString<T>( localizationVariables, variableName );

					int paramSize = StringFuncs<T>::Length( value );
					if (paramSize >= unicodeBufferSize)
					{
						paramSize = MAX( 0, unicodeBufferSize - 1 );
					}

					StringFuncs<T>::Copy( outputPos, value, paramSize );

					unicodeBufferSize -= paramSize;
					outputPos += paramSize;
					formatString = varEnd + 1;
				}
			}
		}

		if (shouldAdvance)
		{
			//copy it over, char by char
			*outputPos = *formatString;

			outputPos++;
			unicodeBufferSize--;

			formatString++;
		}		
	}

	// ensure null termination
	*outputPos = '\0';
}

void CLocalize::ConstructStringKeyValuesInternal(char *unicodeOutput, int unicodeBufferSizeInBytes, const char *formatString, KeyValues *localizationVariables)
{
	ConstructStringKeyValuesInternal_Impl<char>( unicodeOutput, unicodeBufferSizeInBytes, formatString, localizationVariables );
}

void CLocalize::ConstructStringKeyValuesInternal(wchar_t *unicodeOutput, int unicodeBufferSizeInBytes, const wchar_t *formatString, KeyValues *localizationVariables)
{
	ConstructStringKeyValuesInternal_Impl<wchar_t>( unicodeOutput, unicodeBufferSizeInBytes, formatString, localizationVariables );
}

const wchar_t *CLocalize::FindSubLanguage( const char *pToken )
{
	if( m_szSubLanguageFromSettings[0] && m_szSubLanguageFromSettings[0] != SUB_LANGUAGE_NONE )
	{
		const wchar_t *pwch = Find( CFmtStr( "%s_[%s]", pToken, m_szSubLanguageFromSettings ).Get() );
		if( pwch )
		{
			return pwch;
		}
	}
	return Find( pToken );
}

const wchar_t *CLocalize::FindSubLanguageSafe( const char *pToken )
{
	if( m_szSubLanguageFromSettings[0] && m_szSubLanguageFromSettings[0] != SUB_LANGUAGE_NONE )
	{
		const wchar_t *pwch = Find( CFmtStr( "%s_[%s]", pToken, m_szSubLanguageFromSettings ).Get() );
		if( pwch )
		{
			return pwch;
		}
	}

	return FindSafe( pToken );
}

const char *CLocalize::GetCurrentSubLanguage()
{
	if( m_szSubLanguageFromSettings[0] )
	{
		if( m_szSubLanguageFromSettings[0] == SUB_LANGUAGE_NONE )
		{
			return "";
		}
		return m_szSubLanguageFromSettings;
	}

	// Override the default language for text only
	if( CommandLine()->CheckParm( "-textsublanguage" ) )
	{
		V_strcpy_safe( m_szSubLanguageFromSettings, CommandLine()->ParmValue( "-textsublanguage", "US" ) );
		if( m_szSubLanguageFromSettings[0] &&
			m_szSubLanguageFromSettings[0] != SUB_LANGUAGE_NONE &&
			V_stricmp_fast( m_szSubLanguageFromSettings, "unknown" ) != 0 )
		{
			return m_szSubLanguageFromSettings;
		}

		m_szSubLanguageFromSettings[0] = 0;
	}

	return SetCurrentSubLanguageFromSystem();
}

void CLocalize::SetCurrentSubLanguage( const char *pSubLang )
{
	// With the current implementation we could allow the sublanguage to be changed
	// arbitrarily. We're restricting it to only being set in a fresh CLocalize,
	// though, to follow the primary language model and keep things simple until
	// we have a real reason to allow more flexibility.
	// This also avoids potential threading issues since changing the sublanguage
	// in the middle of normal app execution would have global effects.
	Assert( V_isempty( m_szSubLanguageFromSettings ) );
	V_strcpy_safe( m_szSubLanguageFromSettings, pSubLang );
}

const char *CLocalize::SetCurrentSubLanguageFromSystem()
{
	Assert( V_isempty( m_szSubLanguageFromSettings ) );

	// Default to no sub language.
	m_szSubLanguageFromSettings[0] = SUB_LANGUAGE_NONE;

	if( CommandLine()->HasParm( "-language" ) || CommandLine()->HasParm( "-textlanguage" ) )
	{
		// If the user has given a specific language override then
		// just leave the sublanguage unset. If this user cares they
		// should use -textsublanguage to also provide a sublanguage override.
		// This prevents confusing behavior when you don't expect the
		// system sublanguage to still matter in your override language.
		// For example running on an English system with -textlanguage german
		// should have 24-hour clocks but if this code runs it'll pick an
		// English sublanguage and German doesn't have those tokens so
		// they'll still be found on lookup.
		return "";
	}

#if defined( PLATFORM_WINDOWS )

	LCID userLocale = GetUserDefaultLCID();
	WORD langID = LANGIDFROMLCID( userLocale );
	WORD primaryLang = PRIMARYLANGID( langID );
	WORD subLang = SUBLANGID( langID );
	const char *pSub = nullptr;
	switch( primaryLang )
	{
	case LANG_ENGLISH:
		switch( subLang )
		{
		case SUBLANG_ENGLISH_AUS:
			pSub = "AU";
			break;
		case SUBLANG_ENGLISH_BELIZE:
			pSub = "BZ";
			break;
		case SUBLANG_ENGLISH_CAN:
			pSub = "CA";
			break;
		case SUBLANG_ENGLISH_CARIBBEAN:
			pSub = "029";
			break;
		case SUBLANG_ENGLISH_INDIA:
			pSub = "IN";
			break;
		case SUBLANG_ENGLISH_EIRE:
			pSub = "IE";
			break;
		case SUBLANG_ENGLISH_JAMAICA:
			pSub = "JM";
			break;
		case SUBLANG_ENGLISH_MALAYSIA:
			pSub = "MY";
			break;
		case SUBLANG_ENGLISH_NZ:
			pSub = "NZ";
			break;
		case SUBLANG_ENGLISH_PHILIPPINES:
			pSub = "PH";
			break;
		case SUBLANG_ENGLISH_SINGAPORE:
			pSub = "SG";
			break;
		case SUBLANG_ENGLISH_SOUTH_AFRICA:
			pSub = "ZA";
			break;
		case SUBLANG_ENGLISH_TRINIDAD:
			pSub = "TT";
			break;
		case SUBLANG_ENGLISH_UK:
			pSub = "GB";
			break;
		case SUBLANG_ENGLISH_US:
			pSub = "US";
			break;
		case SUBLANG_ENGLISH_ZIMBABWE:
			pSub = "ZW";
			break;
		}
		break;
	case LANG_FRENCH:
		switch( subLang )
		{
		case SUBLANG_FRENCH_BELGIAN:
			pSub = "BE";
			break;
		case SUBLANG_FRENCH_CANADIAN:
			pSub = "CA";
			break;
		case SUBLANG_FRENCH:
			pSub = "FR";
			break;
		case SUBLANG_FRENCH_LUXEMBOURG:
			pSub = "LU";
			break;
		case SUBLANG_FRENCH_MONACO:
			pSub = "MC";
			break;
		case SUBLANG_FRENCH_SWISS:
			pSub = "CH";
			break;
		}
		break;
	case LANG_GERMAN:
		switch( subLang )
		{
		case SUBLANG_GERMAN_AUSTRIAN:
			pSub = "AT";
			break;
		case SUBLANG_GERMAN:
			pSub = "DE";
			break;
		case SUBLANG_GERMAN_LIECHTENSTEIN:
			pSub = "LI";
			break;
		case SUBLANG_GERMAN_LUXEMBOURG:
			pSub = "LU";
			break;
		case SUBLANG_GERMAN_SWISS:
			pSub = "CH";
			break;
		}
		break;
	case LANG_ITALIAN:
		switch( subLang )
		{
		case SUBLANG_ITALIAN:
			pSub = "IT";
			break;
		case SUBLANG_ITALIAN_SWISS:
			pSub = "CH";
			break;
		}
		break;
	case LANG_PORTUGUESE:
		switch( subLang )
		{
		case SUBLANG_PORTUGUESE_BRAZILIAN:
			pSub = "BR";
			break;
		case SUBLANG_PORTUGUESE:
			pSub = "PT";
			break;
		}
		break;
	case LANG_SPANISH:
		switch( subLang )
		{
		case SUBLANG_SPANISH_ARGENTINA:
			pSub = "AR";
			break;
		case SUBLANG_SPANISH_BOLIVIA:
			pSub = "BO";
			break;
		case SUBLANG_SPANISH_CHILE:
			pSub = "CL";
			break;
		case SUBLANG_SPANISH_COLOMBIA:
			pSub = "CO";
			break;
		case SUBLANG_SPANISH_COSTA_RICA:
			pSub = "CR";
			break;
		case SUBLANG_SPANISH_DOMINICAN_REPUBLIC:
			pSub = "DO";
			break;
		case SUBLANG_SPANISH_ECUADOR:
			pSub = "EC";
			break;
		case SUBLANG_SPANISH_EL_SALVADOR:
			pSub = "SV";
			break;
		case SUBLANG_SPANISH_GUATEMALA:
			pSub = "GT";
			break;
		case SUBLANG_SPANISH_HONDURAS:
			pSub = "HN";
			break;
		case SUBLANG_SPANISH_MEXICAN:
			pSub = "MX";
			break;
		case SUBLANG_SPANISH_NICARAGUA:
			pSub = "NI";
			break;
		case SUBLANG_SPANISH_PANAMA:
			pSub = "PA";
			break;
		case SUBLANG_SPANISH_PARAGUAY:
			pSub = "PY";
			break;
		case SUBLANG_SPANISH_PERU:
			pSub = "PE";
			break;
		case SUBLANG_SPANISH_PUERTO_RICO:
			pSub = "PR";
			break;
		case SUBLANG_SPANISH:
			pSub = "ES";
			break;
		case SUBLANG_SPANISH_US:
			pSub = "US";
			break;
		case SUBLANG_SPANISH_URUGUAY:
			pSub = "UY";
			break;
		case SUBLANG_SPANISH_VENEZUELA:
			pSub = "VE";
			break;
		}
		break;
	}
	if( pSub )
	{
		V_strcpy_safe( m_szSubLanguageFromSettings, pSub );
	}

#else // PLATFORM

 	const char *pEnv = getenv( "LC_ALL" );
 	if( !pEnv )
 	{
 		pEnv = getenv( "LANG" );
 	}
 	if( pEnv )
 	{
 		const char *pScan = strchr( pEnv, '_' );
 		if( pScan )
 		{
 			pEnv = pScan + 1;
 		}
 		pScan = strchr( pEnv, '.' );
 		if( !pScan )
 		{
 			pScan = pEnv + V_strlen( pEnv );
 		}
 
		size_t nSubString = pScan - pEnv;

		if(nSubString && (nSubString <= (sizeof(m_szSubLanguageFromSettings)-1)))
		{
			V_memcpy( m_szSubLanguageFromSettings, pEnv, nSubString );
			m_szSubLanguageFromSettings[nSubString] = 0;
		}
 	}

#endif // PLATFORM

	if( m_szSubLanguageFromSettings[0] == SUB_LANGUAGE_NONE )
	{
		return "";
	}

	return m_szSubLanguageFromSettings;
}
