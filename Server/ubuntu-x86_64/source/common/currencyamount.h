//========= Copyright 1996-2007, Valve LLC, All rights reserved. ============
//
// Purpose: money functions
//
//=============================================================================
#ifndef CURRENCYAMOUNT_H
#define CURRENCYAMOUNT_H
#pragma once

#include "platform.h"
#ifdef CLIENT_DLL
#ifdef DOTA_CLIENT_DLL
#include "engineinterface.h"
#else
#include "cbase.h"
#endif
#endif
#include "language.h"

//-----------------------------------------------------------------------------
// Purpose: Currencies we support
//
// WARNING: VALUES STORED IN DATABASE. DO NOT RENUMBER!!!
// WARNING: THESE DON'T MATCH THE STEAM NUMERIC IDS!!! WE TALK USING CURRENCY
//			CODES LIKE "VND" AND IF YOU SAY "15" TERRIBLE THINGS WILL HAPPEN
//-----------------------------------------------------------------------------
enum ECurrency : int32
{
	k_ECurrencyFirst = 0,
	k_ECurrencyUSD = 0,
	k_ECurrencyGBP = 1,
	k_ECurrencyEUR = 2,
	k_ECurrencyRUB = 3,
	k_ECurrencyBRL = 4,
	// Dota2 only, not used in CS:GO -- k_ECurrencyRMB = 5,

	// space for Dota currencies
	k_ECurrencyJPY = 8,
	k_ECurrencyNOK = 9,
	k_ECurrencyIDR = 10,
	k_ECurrencyMYR = 11,
	k_ECurrencyPHP = 12,
	k_ECurrencySGD = 13,
	k_ECurrencyTHB = 14,
	k_ECurrencyVND = 15, // Vietnamese Dong
	k_ECurrencyKRW = 16,
	k_ECurrencyTRY = 17,
	k_ECurrencyUAH = 18, // Ukrainian Hryvnia, launching Autumn 2017
	k_ECurrencyMXN = 19,
	k_ECurrencyCAD = 20,
	k_ECurrencyAUD = 21,
	k_ECurrencyNZD = 22,
	k_ECurrencyPLN = 23, // Polish Zloty, launching Autumn 2017
	k_ECurrencyCHF = 24,

	// New currencies 2015/2016
	k_ECurrencyAED = 25,
	k_ECurrencyCLP = 26,
	k_ECurrencyCNY = 27,
	k_ECurrencyCOP = 28,
	k_ECurrencyPEN = 29,
	k_ECurrencySAR = 30,
	k_ECurrencyTWD = 31,
	k_ECurrencyHKD = 32,
	k_ECurrencyZAR = 33,
	k_ECurrencyINR = 34,

	// New currencies Autumn 2017
	k_ECurrencyARS = 35, // Argentinian Peso
	k_ECurrencyCRC = 36, // Costa Rican Colon
	k_ECurrencyILS = 37, // Israeli New Shekel
	k_ECurrencyKWD = 38, // Kuwaiti Dinar
	k_ECurrencyQAR = 39, // Qatari Rial
	k_ECurrencyUYU = 40, // Uruguayan Peso
	k_ECurrencyKZT = 41, // Kazakh Tenge
	k_ECurrencyBYN = 42, // Belarus Ruble (not launching immediately in Autumn 2017, not yet decided when)

	// Must be last
	k_ECurrencyMax = 43,

	// make this a big number so we can avoid having to move it when we add another currency type
	k_ECurrencyInvalid = 255, 
};

// Macro for looping across all currencies
#define FOR_EACH_CURRENCY( _i ) for ( int _i = (int)k_ECurrencyFirst; _i < (int)k_ECurrencyMax; ++_i )

const char *PchNameFromECurrency( ECurrency eCurrency );	// NOTE: Defined with ENUMSTRINGS_START/ENUMSTRINGS_REVERSE macros
ECurrency ECurrencyFromName( const char *pchName );			//

inline bool BIsCurrencyValid( ECurrency eCurrency )
{
	switch ( eCurrency )
	{
	case k_ECurrencyUSD:
	case k_ECurrencyGBP:
	case k_ECurrencyEUR:
	case k_ECurrencyRUB:
	case k_ECurrencyBRL:
	case k_ECurrencyJPY:
	case k_ECurrencyNOK:
	case k_ECurrencyIDR:
	case k_ECurrencyMYR:
	case k_ECurrencyPHP:
	case k_ECurrencySGD:
	case k_ECurrencyTHB:
	case k_ECurrencyVND:
	case k_ECurrencyKRW:
	case k_ECurrencyTRY:
	case k_ECurrencyUAH:
	case k_ECurrencyMXN:
	case k_ECurrencyCAD:
	case k_ECurrencyAUD:
	case k_ECurrencyNZD:
	case k_ECurrencyPLN:
	case k_ECurrencyCHF:
	case k_ECurrencyAED:
	case k_ECurrencyCLP:
	case k_ECurrencyCNY:
	case k_ECurrencyCOP:
	case k_ECurrencyPEN:
	case k_ECurrencySAR:
	case k_ECurrencyTWD:
	case k_ECurrencyHKD:
	case k_ECurrencyZAR:
	case k_ECurrencyINR:
	case k_ECurrencyARS:
	case k_ECurrencyCRC:
	case k_ECurrencyILS:
	case k_ECurrencyKWD:
	case k_ECurrencyQAR:
	case k_ECurrencyUYU:
	case k_ECurrencyKZT:
	// case k_ECurrencyBYN: // not yet launched as of Autumn 2017
		return true;
	}

	return false;
}

inline ECurrency GetFirstValidCurrency()
{
	for ( int i = k_ECurrencyFirst; i < k_ECurrencyMax; i++ )
	{
		if ( BIsCurrencyValid( (ECurrency)i ) )
			return (ECurrency)i;
	}
	return k_ECurrencyInvalid;
}

inline ECurrency GetNextValidCurrency( ECurrency ePrevious )
{
	for ( int i = ePrevious + 1; i < k_ECurrencyMax; i++ )
	{
		if ( BIsCurrencyValid( (ECurrency)i ) )
			return (ECurrency)i;
	}
	return k_ECurrencyInvalid;
}

#ifdef CLIENT_DLL
inline ELanguage GetStoreLanguage()
{
	if ( !engine )
		return k_Lang_English;

	char uilanguage[64];
	engine->GetUILanguage( uilanguage, sizeof( uilanguage ) );

	return PchLanguageToELanguage( uilanguage );
}

void MakeMoneyString( wchar_t *pchDest, uint32 nDest, uint32 unPrice, ECurrency eCurrencyCode, ELanguage eLanguage = GetStoreLanguage(), bool bNoFractional = false );
void MakeMoneyString( char *pchDest, uint32 nDest, uint32 unPrice, ECurrency eCurrencyCode, ELanguage eLanguage = GetStoreLanguage(), bool bNoFractional = false );
#else
void MakeMoneyString( wchar_t *pchDest, uint32 nDest, uint32 unPrice, ECurrency eCurrencyCode, ELanguage eLanguage, bool bNoFractional = false );
void MakeMoneyString( char *pchDest, uint32 nDest, uint32 unPrice, ECurrency eCurrencyCode, ELanguage eLanguage, bool bNoFractional = false );
#endif



//-----------------------------------------------------------------------------
// CAmount
// Represents an amount in a specific currency
//-----------------------------------------------------------------------------
template< class T >
class CCurrencyAmount_t
{
public:
	CCurrencyAmount_t() : m_nAmount( 0 ), m_eCurrencyCode( k_ECurrencyInvalid )
	{
		// matches InvalidAmount()
	}

	CCurrencyAmount_t( const CCurrencyAmount_t &that )
	{
		m_nAmount = that.m_nAmount;
		m_eCurrencyCode = that.m_eCurrencyCode;
	}

	CCurrencyAmount_t( T nAmount, const char* pchCurrencyCode )
	{
		BInit( nAmount, pchCurrencyCode );
	}

	CCurrencyAmount_t( T nAmount, ECurrency eCurrencyCode )
	{
		BInit( nAmount, eCurrencyCode );
	}

	bool BInit( T nAmount, const char *pchCurrencyCode )
	{
		m_eCurrencyCode = ECurrencyFromName( pchCurrencyCode );
		if ( m_eCurrencyCode == k_ECurrencyInvalid )
		{
			*this = CCurrencyAmount_t::InvalidAmount();
			return false;
		}

		m_nAmount = nAmount;
		return true;
	}

	bool BInit( T nAmount, ECurrency eCurrencyCode )
	{
		m_nAmount = nAmount;
		m_eCurrencyCode = eCurrencyCode;

		return true;
	}

	T GetAmount() const							{ return m_nAmount; }
	ECurrency GetCurrencyCode() const		{ return m_eCurrencyCode; }
	const char * GetPchCurrencyCode() const		{ return PchNameFromECurrency( m_eCurrencyCode ); }

	bool IsValid() const { return (m_eCurrencyCode != k_ECurrencyInvalid); }
	bool BSameCurrency( const CCurrencyAmount_t &that ) const			{ return (m_eCurrencyCode == that.m_eCurrencyCode); }
	bool BSameCurrency( ECurrency eCurrencyCode ) const		{ return (m_eCurrencyCode == eCurrencyCode); }
	bool BSameCurrency( const char *pchCurrencyCode ) const		{ return (m_eCurrencyCode == ECurrencyFromName( pchCurrencyCode )); }

	const CCurrencyAmount_t operator+(const CCurrencyAmount_t &that) const
	{
		if ( !BSameCurrency( that ) )
			return CCurrencyAmount_t::InvalidAmount();

		return CCurrencyAmount_t( m_nAmount + that.m_nAmount, m_eCurrencyCode );
	}

	const CCurrencyAmount_t operator-(const CCurrencyAmount_t &that) const
	{
		if ( !BSameCurrency( that ) )
			return CCurrencyAmount_t::InvalidAmount();

		return CCurrencyAmount_t( m_nAmount - that.m_nAmount, m_eCurrencyCode );
	}

	const CCurrencyAmount_t& operator+=(const CCurrencyAmount_t &that)
	{
		if ( !BSameCurrency( that ) )
			*this = CCurrencyAmount_t::InvalidAmount();
		else
			m_nAmount += that.m_nAmount;

		return *this;
	}

	const CCurrencyAmount_t& operator-=(const CCurrencyAmount_t &that)
	{
		if ( !BSameCurrency( that ) )
			*this = CCurrencyAmount_t::InvalidAmount();
		else
			m_nAmount -= that.m_nAmount;

		return *this;
	}

	bool operator==(const CCurrencyAmount_t &that) const { return (BSameCurrency( that ) && (m_nAmount == that.m_nAmount)); }
	bool operator!=(const CCurrencyAmount_t &that) const { return !(*this == that); }

	// less than / greater than operators:
	// not implemented because there is no good return value for Amounts with different Currency codes
	// compare by hand instead ( lhs.BSameCurrency( rhs ) && lhs.m_nAmount < rhs.m_nAmount )

	static const CCurrencyAmount_t& InvalidAmount()
	{
		static CCurrencyAmount_t amount;
		return amount;
	}

	static const CCurrencyAmount_t ZeroAmount( ECurrency eCurrencyCode )
	{
		return CCurrencyAmount_t( 0, eCurrencyCode );
	}

	void ToStringUTF8( char *pchDest, uint32 nDest, ELanguage eLang = k_Lang_None ) const
	{
		MakeMoneyString( pchDest, nDest, m_nAmount, m_eCurrencyCode, eLang );
	}

	void ToString( wchar_t *pchDest, uint32 nDest, ELanguage eLang = k_Lang_None ) const
	{
		MakeMoneyString( pchDest, nDest, m_nAmount, m_eCurrencyCode, eLang );
	}


private:
	T				m_nAmount;
	ECurrency	m_eCurrencyCode;
};

typedef CCurrencyAmount_t< uint32 > CCurrencyAmount;
typedef CCurrencyAmount_t< uint64 > CCCurrencyBigAmount;

#endif // CURRENCYAMOUNT_H
