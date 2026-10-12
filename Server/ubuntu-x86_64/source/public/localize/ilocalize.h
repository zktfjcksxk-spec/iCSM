//===== Copyright  1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef ILOCALIZE_H
#define ILOCALIZE_H

#ifdef _WIN32
#pragma once
#endif

#include "appframework/iappsystem.h"
#include <tier1/keyvalues.h>

// unicode character type
// for more unicode manipulation functions #include <wchar.h>
#if !defined( _WCHAR_T_DEFINED ) && !defined( _PS3 ) && !defined(__clang__)
typedef unsigned short wchar_t;
#define _WCHAR_T_DEFINED
#endif

enum ECurrency : int32;
enum ELanguage : int32;

enum LOC_DATE_FORMAT
{
	// "Short date" format.
	LOC_DATE_DAY_MONTH_YEAR_NUMERIC,
	// "Long date" format.
	LOC_DATE_DAY_OF_WEEK_MONTH_DAY_YEAR,

	// "Short time" format.
	LOC_DATE_HOUR_MINUTE,
	// "Long time" format.
	LOC_DATE_HOUR_MINUTE_SECOND,

	// "Long date and time" format.
	LOC_DATE_DAY_OF_WEEK_MONTH_DAY_YEAR_HOUR_MINUTE_SECOND,

	// Specific-fields formats.
	LOC_DATE_DAY_MONTH,
	LOC_DATE_DAY_MONTH_YEAR,
	LOC_DATE_DAY_MONTH_YEAR_HOUR_MINUTE,
	LOC_DATE_DAY_MONTH_YEAR_HOUR_MINUTE_SECOND,
	LOC_DATE_DAY_OF_WEEK_SHORT_MONTH_SHORT_DAY_HOUR_MINUTE_SECOND,
	LOC_DATE_DAY_OF_WEEK,
	LOC_DATE_DAY_OF_WEEK_DAY_MONTH_HOUR_MINUTE,
	LOC_DATE_DAY_MONTH_SHORT,

	// Formats matching econ item conventions.
	LOC_DATE_ECON_MONTH_SHORT_DAY_YEAR_HOUR_MINUTE_SECOND,
	LOC_DATE_ECON_MONTH_SHORT_DAY_YEAR_HOUR_MINUTE_SECOND_GMT,

	// Language-neutral numeric timestamp string with YYYY_MM_DD_HH_MM_SS.
	LOC_DATE_NEUTRAL_TIMESTAMP,
};

enum LOC_DURATION_FORMAT
{
	// Start with "x days" if necessary and add numeric H:MM:SS.
	LOC_DURATION_DAYS_SHORTEST_OPT_HOURS_MINUTES_SECONDS,
	LOC_DURATION_DAYS_HOURS_MINUTES_SECONDS,
	LOC_DURATION_HOURS_MINUTES_SECONDS,
	LOC_DURATION_HOURS_OPT_MINUTES_SECONDS,
	LOC_DURATION_MINUTES_SECONDS,
	// Just the count of days.
	LOC_DURATION_DAYS,
	// Start with the largest quantity present among days,
	// hours, minutes and seconds and add all lesser quantities.
	// Uses suffixed values like 2d 3h 4m 19s.
	LOC_DURATION_LARGEST_NEEDED_DAYS_HOURS_MINUTES_SECONDS_SUFFIXED,
	LOC_DURATION_LARGEST_NEEDED_DAYS_HOURS_MINUTES_SUFFIXED,
	LOC_DURATION_LARGEST_NEEDED_HOURS_MINUTES_SECONDS_SUFFIXED,
	LOC_DURATION_LARGEST_NEEDED_HOURS_MINUTES_SUFFIXED,
	// Add any day/hour/minute/second quantity that isn't zero
	// as "x days" or "y minutes".
	LOC_DURATION_NONZERO_HOURS_MINUTES,
	LOC_DURATION_NONZERO_HOURS_MINUTES_SECONDS,
	// Only add non-zero seconds if most other fields are zero.
	LOC_DURATION_NONZERO_HOURS_MINUTES_SECONDS_EXTRA,
	LOC_DURATION_NONZERO_DAYS_HOURS_MINUTES,
	LOC_DURATION_NONZERO_DAYS_HOURS_MINUTES_SECONDS,
	// Only add non-zero seconds if most other fields are zero.
	LOC_DURATION_NONZERO_DAYS_HOURS_MINUTES_SECONDS_EXTRA,
};

enum LOC_NUMBER_FORMAT
{
	// Default human-readable format, such as '123,456'.
	LOC_NUMBER_SIGNED,
	LOC_NUMBER_UNSIGNED,
	// The numeric part of a money string, always
	// uses the absolute value.
	LOC_NUMBER_MONEY,
};

enum LOC_ORDINAL_FORMAT
{
	// Digits and text prefix/suffix, such as 1st/2nd/3rd.
	LOC_ORDINAL_DIGITS_AND_TEXT,
};

//-----------------------------------------------------------------------------
// Interface used to query text size so we can choose the longest one
//-----------------------------------------------------------------------------
abstract_class ILocalizeTextQuery
{
public:
	virtual int ComputeTextWidth( const wchar_t *pString ) = 0;
};


//-----------------------------------------------------------------------------
// Callback which is triggered when any localization string changes
// Is not called when a localization string is added
//-----------------------------------------------------------------------------
abstract_class ILocalizationChangeCallback
{
public:
	virtual void OnLocalizationChanged() = 0;
};


//-----------------------------------------------------------------------------
// Purpose: Handles localization of text
//			looks up string names and returns the localized unicode text
//-----------------------------------------------------------------------------
// direct references to localized strings
typedef uint32 LocalizeStringIndex_t;
const uint32 LOCALIZE_INVALID_STRING_INDEX = (LocalizeStringIndex_t)-1;

abstract_class ILocalize : public IAppSystem
{
public:
	// adds the contents of a file to the localization table
	virtual bool AddFile( const char *fileName, const char *pPathID = NULL, bool bIncludeFallbackSearchPaths = false ) = 0;

	// Remove all strings from the table
	virtual void RemoveAll() = 0;

	// Finds the localized text for tokenName. Returns NULL if none is found.
	virtual wchar_t *Find(const char *tokenName) = 0;

	// Like Find(), but as a failsafe, returns an error message instead of NULL if the string isn't found.  
	virtual const wchar_t *FindSafe(const char *tokenName) = 0;

	// Checks for sublanguage-specific versions of the token before falling back to regular Find.
	virtual const wchar_t *FindSubLanguage( const char *pToken ) = 0;
	virtual const wchar_t *FindSubLanguageSafe( const char *pToken ) = 0;

	// converts an english string to unicode
	// returns the number of wchar_t in resulting string, including null terminator
	virtual int ConvertANSIToUnicode(const char *ansi, OUT_Z_BYTECAP(unicodeBufferSizeInBytes) wchar_t *unicode, int unicodeBufferSizeInBytes) = 0;

	// converts an unicode string to an english string
	// unrepresentable characters are converted to system default
	// returns the number of characters in resulting string, including null terminator
	virtual int ConvertUnicodeToANSI(const wchar_t *unicode, OUT_Z_CAP(ansiBufferSize) char *ansi, int ansiBufferSize) = 0;

	// finds the index of a token by token name, INVALID_STRING_INDEX if not found
	virtual LocalizeStringIndex_t FindIndex(const char *tokenName) = 0;

	// gets the values by the string index
	virtual const char *GetNameByIndex(LocalizeStringIndex_t index) = 0;
	virtual wchar_t *GetValueByIndex(LocalizeStringIndex_t index) = 0;

	///////////////////////////////////////////////////////////////////
	// the following functions should only be used by localization editors

	// iteration functions
	virtual LocalizeStringIndex_t GetFirstStringIndex() = 0;
	// returns the next index, or INVALID_STRING_INDEX if no more strings available
	virtual LocalizeStringIndex_t GetNextStringIndex(LocalizeStringIndex_t index) = 0;

	// adds a single name/unicode string pair to the table
	virtual void AddString( const char *tokenName, wchar_t *unicodeString, const char *fileName ) = 0;

	// changes the value of a string
	virtual void SetValueByIndex(LocalizeStringIndex_t index, wchar_t *newValue) = 0;

	// saves the entire contents of the token tree to the file
	virtual bool SaveToFile( const char *fileName ) = 0;

	// iterates the filenames
	virtual int GetLocalizationFileCount() = 0;
	virtual const char *GetLocalizationFileName(int index) = 0;

	// returns the name of the file the specified localized string is stored in
	virtual const char *GetFileNameByIndex(LocalizeStringIndex_t index) = 0;

	// for development only, reloads localization files
	virtual void ReloadLocalizationFiles( ) = 0;

	// need to replace the existing ConstructString with this
	virtual void ConstructString(OUT_Z_BYTECAP(unicodeBufferSizeInBytes) wchar_t *unicodeOutput, int unicodeBufferSizeInBytes, const char *tokenName, KeyValues *localizationVariables) = 0;
	virtual void ConstructString(OUT_Z_BYTECAP(unicodeBufferSizeInBytes) wchar_t *unicodeOutput, int unicodeBufferSizeInBytes, LocalizeStringIndex_t unlocalizedTextSymbol, KeyValues *localizationVariables) = 0;

	// Construct localized date and time strings.
	virtual const char *ConstructDateString( char *pUTF8Out, int nOutSizeInBytes, LOC_DATE_FORMAT format, struct tm *pTime, bool bAppend = false ) = 0;
	virtual const char *ConstructDateString( char *pUTF8Out, int nOutSizeInBytes, LOC_DATE_FORMAT format, uint64 nTime, bool bAppend = false ) = 0;
	// A relative date is specialized according to how the given time relates to
	// the current time.  If it's close a specific term, such as 'today' or 'tomorrow',
	// will be used.  The larger the time gap the less-specific the result will be,
	// eventually falling back on the given base date format.
	// The time given can be in the future or the past.
	virtual const char *ConstructRelativeDateString( char *pUTF8Out, int nOutSizeInBytes, LOC_DATE_FORMAT fallbackFormat, uint64 nRelativeTime, bool bAppend = false ) = 0;
	// As with ConstructRelativeDateString construct a string that is simpler
	// when the given relative time is closer to the current time.  If
	// it's within a day the timeFormat is used, otherwise ConstructRelativeDateString
	// is called with fallbackFormat.
	virtual const char *ConstructRelativeTimeString( char *pUTF8Out, int nOutSizeInBytes, LOC_DATE_FORMAT timeFormat, LOC_DATE_FORMAT fallbackFormat, uint64 nRelativeTime, bool bAppend = false ) = 0;
	virtual const char *ConstructDurationString( char *pUTF8Out, int nOutSizeInBytes, LOC_DURATION_FORMAT format, int nSeconds, bool bAppend = false ) = 0;

	// Construct formatted number strings, such as by grouping thousands.
	virtual const char *ConstructNumberString( char *pUTF8Out, int nOutSizeInBytes, LOC_NUMBER_FORMAT format, uint64 nValue, bool bAppend = false ) = 0;
	virtual const char *ConstructNumberString( char *pUTF8Out, int nOutSizeInBytes, LOC_NUMBER_FORMAT format, double flValue, int nPrecision, bool bAppend = false ) = 0;

	// Construct an ordinal number string, such as 1st/2nd/3rd.
	virtual const char *ConstructOrdinalString( char *pUTF8Out, int nOutSizeInBytes, LOC_ORDINAL_FORMAT format, uint32 nValue, bool bAppend = false ) = 0;

	// Construct formatted monetary strings.
	// The integer version's value is scaled by 100 so that fractions are
	// represented exactly.
	// Normally the precision of the value is set according to the currency's
	// conventions but it can be forced with a non-negative nOverridePrecision.
	virtual const char *ConstructMoneyString( char *pUTF8Out, int nOutSizeInBytes, uint32 nValue, ECurrency eCurrency, int nOverridePrecision = -1, bool bAppend = false ) = 0;

	// The sublanguage is typically a regional variation of a language,
	// such as US English or UK English. Normal appsystem initialization
	// will set it from system information.
	// It does not affect base FindSafe/Unsafe calls but does affect
	// FindSubLanguageSafe/Unsafe and date/time/number/money string construction.
	virtual const char *GetCurrentSubLanguage() = 0;
	virtual void SetCurrentSubLanguage( const char *pSubLang ) = 0;
	virtual const char *SetCurrentSubLanguageFromSystem() = 0;

	// Used to install a callback to query which localized strings are the longest
	virtual void SetTextQuery( ILocalizeTextQuery *pQuery ) = 0;

	// Is called when any localization strings change
	virtual void InstallChangeCallback( ILocalizationChangeCallback *pCallback ) = 0;
	virtual void RemoveChangeCallback( ILocalizationChangeCallback *pCallback ) = 0;

	virtual wchar_t* GetAsianFrequencySequence( const char * pLanguage ) = 0;

	virtual const char *FindAsUTF8( const char *pchTokenName ) = 0;

	// builds a localized formatted string
	// uses the format strings first: %s1, %s2, ...  unicode strings (wchar_t *)
	template < typename T >
	void ConstructString(OUT_Z_BYTECAP(unicodeBufferSizeInBytes) T *unicodeOuput, int unicodeBufferSizeInBytes, const T *formatString, int numFormatParameters, ...)
	{
		va_list argList;
		va_start( argList, numFormatParameters );

		ConstructStringVArgsInternal( unicodeOuput, unicodeBufferSizeInBytes, formatString, numFormatParameters, argList );

		va_end( argList );
	}

	template < typename T >
	void ConstructStringVArgs(OUT_Z_BYTECAP(unicodeBufferSizeInBytes) T *unicodeOuput, int unicodeBufferSizeInBytes, const T *formatString, int numFormatParameters, va_list argList)
	{
		ConstructStringVArgsInternal( unicodeOuput, unicodeBufferSizeInBytes, formatString, numFormatParameters, argList );
	}

	template < typename T >
	void ConstructString(OUT_Z_BYTECAP(unicodeBufferSizeInBytes) T *unicodeOutput, int unicodeBufferSizeInBytes, const T *formatString, KeyValues *localizationVariables)
	{
		ConstructStringKeyValuesInternal( unicodeOutput, unicodeBufferSizeInBytes, formatString, localizationVariables );
	}

protected:
	// internal "interface"
	virtual void ConstructStringVArgsInternal(char *unicodeOutput, int unicodeBufferSizeInBytes, const char *formatString, int numFormatParameters, va_list argList) = 0;
	virtual void ConstructStringVArgsInternal(wchar_t *unicodeOutput, int unicodeBufferSizeInBytes, const wchar_t *formatString, int numFormatParameters, va_list argList) = 0;

	virtual void ConstructStringKeyValuesInternal(char *unicodeOutput, int unicodeBufferSizeInBytes, const char *formatString, KeyValues *localizationVariables) = 0;
	virtual void ConstructStringKeyValuesInternal(wchar_t *unicodeOutput, int unicodeBufferSizeInBytes, const wchar_t *formatString, KeyValues *localizationVariables) = 0;
};


#endif // ILOCALIZE_H
