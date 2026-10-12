//===== Copyright, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//
// For formatting in locale
#pragma warning( disable : 4530 )   // warning: exception handler -GX option

#include <string>
#include <sstream>

#include "currencyamount.h"
#include "language.h"
#include "strtools.h"
#include "gcsdk/enumutils.h"
#include "dbg.h"

ENUMSTRINGS_START( ECurrency )
 {k_ECurrencyUSD, "USD"},
{ k_ECurrencyGBP, "GBP" },
{ k_ECurrencyEUR, "EUR" },
{ k_ECurrencyRUB, "RUB" },
{ k_ECurrencyBRL, "BRL" },
{ k_ECurrencyJPY, "JPY" },
{ k_ECurrencyNOK, "NOK" },
{ k_ECurrencyIDR, "IDR" },
{ k_ECurrencyMYR, "MYR" },
{ k_ECurrencyPHP, "PHP" },
{ k_ECurrencySGD, "SGD" },
{ k_ECurrencyTHB, "THB" },
{ k_ECurrencyVND, "VND" },
{ k_ECurrencyKRW, "KRW" },
{ k_ECurrencyTRY, "TRY" },
{ k_ECurrencyUAH, "UAH" },
{ k_ECurrencyMXN, "MXN" },
{ k_ECurrencyCAD, "CAD" },
{ k_ECurrencyAUD, "AUD" },
{ k_ECurrencyNZD, "NZD" },
{ k_ECurrencyPLN, "PLN" },
{ k_ECurrencyCHF, "CHF" },
{ k_ECurrencyAED, "AED" },
{ k_ECurrencyCLP, "CLP" },
{ k_ECurrencyCNY, "CNY" },
{ k_ECurrencyCOP, "COP" },
{ k_ECurrencyPEN, "PEN" },
{ k_ECurrencySAR, "SAR" },
{ k_ECurrencyTWD, "TWD" },
{ k_ECurrencyHKD, "HKD" },
{ k_ECurrencyZAR, "ZAR" },
{ k_ECurrencyINR, "INR" },
{ k_ECurrencyARS, "ARS" },
{ k_ECurrencyCRC, "CRC" },
{ k_ECurrencyILS, "ILS" },
{ k_ECurrencyKWD, "KWD" },
{ k_ECurrencyQAR, "QAR" },
{ k_ECurrencyUYU, "UYU" },
{ k_ECurrencyKZT, "KZT" },
{ k_ECurrencyBYN, "BYN" },
{ k_ECurrencyInvalid, "Invalid" }
ENUMSTRINGS_REVERSE( ECurrency, k_ECurrencyInvalid )

//-----------------------------------------------------------------------------
// Purpose: return the CLocale name that works with setlocale()
//-----------------------------------------------------------------------------
static const char *GetLanguageCLocaleName( ELanguage eLang )
{
	if ( eLang == k_Lang_None )
		return "";

#ifdef _WIN32
	// table for Win32 is here: http://msdn.microsoft.com/en-us/library/hzz3tw78(v=VS.80).aspx
	// shortname works except for chinese

	switch ( eLang )
	{
	case k_Lang_Simplified_Chinese:
		return "chs"; // or "chinese-simplified"
	case k_Lang_Traditional_Chinese:
		return "cht"; // or "chinese-traditional"
	case k_Lang_Korean:
		return "korean"; // steam likes "koreana" for the name for some reason.
	case k_Lang_Brazilian:
		return "ptb"; // "portuguese-brazil" - that string fails even though it's in the MS lang table; ptb does work.
	default:
		return GetLanguageShortName( eLang );
	}

#else
	switch ( eLang )
	{
	case k_Lang_Simplified_Chinese:
	case k_Lang_Traditional_Chinese:
		return "zh_CN";
	default:
		;
	}

	// ICU codes work on linux/osx
	return GetLanguageICUName( eLang );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Get an I/O stream into the right local/settings for printing money - so to speak
//-----------------------------------------------------------------------------
static void InitStreamLocale( std::wostringstream &stream, ELanguage eLang, uint32 nExpectedAmount, bool bCurrencyPrefersNoFractional, bool bNoFractional )
{
	const char *pszLocale = GetLanguageCLocaleName( eLang );

#ifdef _PS3
	stream.imbue( std::locale( pszLocale ) ); // no exception for PS3
#else
	try
	{
		stream.imbue( std::locale( pszLocale ) );
	}
	catch ( const std::exception & /*e*/ )
	{
		//Log( "stream::imbue() failed with locale: '%s', exception: %s\n", pszLocale, e.what() );
		stream.imbue( std::locale( "C" ) );
	}
#endif

	// Don't display fractional rubles (But if our amount is fractional, we should show it regardless)
	if ( bNoFractional || ( bCurrencyPrefersNoFractional && ( ( nExpectedAmount % 100 ) == 0 ) ) )
	{
		stream.precision( 0 );
		stream.setf( std::ios_base::fixed );
	}
	else
	{
		stream.precision( 2 );
		stream.setf( std::ios_base::fixed, std::ios_base::floatfield );
	}
}

struct CurrencyMoneyStringConfiguration_t
{
	const char *m_pchSymbol;
	enum ESymbol_t {
		k_ESymbolFirstThenAmount,
		k_EAmountFirstThenSymbol
	} m_eSymbolPlacementPolicy;
	enum ESpacing_t {
		k_ESpaceBetweenSymbolAndAmount,
		k_ETogether
	} m_eSpaceBetweenTokens;
	enum EDenomination_t {
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

	int FormatMoneyString( wchar_t *pchDest, uint32 nDest, uint32 unPrice, ELanguage eLanguage, bool bNoFractional ) const;
};

int CurrencyMoneyStringConfiguration_t::FormatMoneyString( wchar_t *pchDest, uint32 nDest, uint32 unPrice, ELanguage eLanguage, bool bNoFractional ) const
{
	// Use the actual currency symbol with the local number formatting.
	// assume local locale - should not be used from server to send to client
	// without passing in a valid pszCLocale parameter.
	std::wostringstream stream;
	InitStreamLocale( stream, eLanguage, unPrice, m_eDenominationFraction == CurrencyMoneyStringConfiguration_t::k_EWholeUnitsOnly, bNoFractional );

	stream << ( unPrice / 100.0 );

	const auto sAmount = stream.str();
	const wchar_t *wszAmount = sAmount.c_str();

	// BEGIN HACK GAME CLIENT CHARACTER SET CONVERSION
	wchar_t wsSymbol[ 16 ];
	V_UTF8ToUnicode( m_pchSymbol, wsSymbol, ARRAYSIZE( wsSymbol ) );
	// END HACK GAME CLIENT CHARACTER SET CONVERSION

	return V_snwprintf( pchDest, nDest, L"%ls%ls%ls",
		( ( m_eSymbolPlacementPolicy == CurrencyMoneyStringConfiguration_t::k_ESymbolFirstThenAmount ) ? wsSymbol : wszAmount ),
		( ( m_eSpaceBetweenTokens == CurrencyMoneyStringConfiguration_t::k_ESpaceBetweenSymbolAndAmount ) ? L" " : L"" ),
		( ( m_eSymbolPlacementPolicy == CurrencyMoneyStringConfiguration_t::k_ESymbolFirstThenAmount ) ? wszAmount : wsSymbol ) );
}

static CurrencyMoneyStringConfiguration_t GetCurrencyMoneyStringConfiguration( ECurrency eCurrencyCode )
{
	switch ( eCurrencyCode )
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

static int MakeMoneyStringInternal( wchar_t *pchDest, uint32 nDest, uint32 unPrice, ECurrency eCurrencyCode, ELanguage eLanguage, bool bNoFractional )
{
	//custom partner currency formatting
	#if defined ( DOTA )
		if ( eCurrencyCode == k_ECurrencyPerfectWorldRMB )
		{
			//we need full unicode formatting of these characters on the raw price
			return V_snwprintf( pchDest, nDest, L"%u \x5200\x5E01", unPrice );
		}
	#elif defined ( CSTRIKE15 )
		static bool s_bPerfectWorld = !!CommandLine()->FindParm( "-perfectworld" );
		if ( s_bPerfectWorld )
		{
			const CurrencyMoneyStringConfiguration_t pwFormat( "\xE7\x82\xB9", CurrencyMoneyStringConfiguration_t::k_EAmountFirstThenSymbol, CurrencyMoneyStringConfiguration_t::k_ESpaceBetweenSymbolAndAmount, CurrencyMoneyStringConfiguration_t::k_EWholeUnitsOnly ); // Magic Unicode character for fake CS:GO points fake currency symbol
			//NOTE: The *100 is to counteract the /100 internal to the format since the price should be unmodified
			return pwFormat.FormatMoneyString( pchDest, nDest, unPrice * 100, eLanguage, bNoFractional );
		}
	#endif
	
	const CurrencyMoneyStringConfiguration_t cfg = GetCurrencyMoneyStringConfiguration( eCurrencyCode );
	return cfg.FormatMoneyString( pchDest, nDest, unPrice, eLanguage, bNoFractional );
}

void MakeMoneyString( wchar_t *pchDest, uint32 nDest, uint32 unPrice, ECurrency eCurrencyCode, ELanguage eLanguage, bool bNoFractional )
{
	(void)MakeMoneyStringInternal( pchDest, nDest, unPrice, eCurrencyCode, eLanguage, bNoFractional );
}

void MakeMoneyString( char *pchDest, uint32 nDest, uint32 unPrice, ECurrency eCurrencyCode, ELanguage eLanguage, bool bNoFractional )
{
    wchar_t wszDest[128];
	(void)MakeMoneyStringInternal( wszDest, ARRAYSIZE( wszDest ), unPrice, eCurrencyCode, eLanguage, bNoFractional );
    V_WStringToUTF8( wszDest, pchDest, nDest );
}
