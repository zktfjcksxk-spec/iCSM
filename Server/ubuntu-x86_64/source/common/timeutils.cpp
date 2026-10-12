//======== Copyright 2010, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#include "timeutils.h"
#include "tier1/fmtstr.h"
#include "tier1/timeutils.h"
#include "tier1/utlbuffer.h"
#include "language.h"

#include "vstrtools.h"
#include "time.h"

#if defined( _WIN32 )
#include "winlite.h"
#elif defined( OSX )
#include <Carbon/Carbon.h>
#endif


#include <tier0/dbg.h>
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Purpose: Given an RTime32 / time_t, compute a SYSTEMTIME
// From:	//Steam/main/src/common/windowshelper.cpp
//-----------------------------------------------------------------------------
#ifdef _WIN32
static void SystemTimeFromRTime( struct _SYSTEMTIME *pst, const time_t rtime )
{
	struct tm tmWhen;
	
	Plat_localtime( &rtime, &tmWhen );

	pst->wHour = tmWhen.tm_hour;
	pst->wMinute = tmWhen.tm_min;
	pst->wSecond = tmWhen.tm_sec;
	pst->wDay = tmWhen.tm_mday;
	pst->wMonth = tmWhen.tm_mon + 1;
	pst->wYear = tmWhen.tm_year + 1900;
	pst->wDayOfWeek = tmWhen.tm_wday;
	pst->wMilliseconds = 0;
}
#endif


// worker routine, broken out by platform
static bool ConstructRelativeDateStringWorker( char *output, int cbOutput, const char *szToken, time_t tThen, const struct tm &tmThen, ELanguage language, bool bLongDate )
{
#ifdef _WIN32
	int cchOutput = cbOutput;
	SYSTEMTIME st;
	SystemTimeFromRTime( &st, tThen );
	bool bOK = false;

	wchar_t *pwch = reinterpret_cast< wchar_t* >( stackalloc( cchOutput * sizeof( wchar_t ) ) );

	if ( szToken == NULL )
	{
		// we don't have a specific date format to use,
		// we'll ask the OS for the user's preference
		wchar_t rgwchDateFormat[100];
		bOK = 0 != GetLocaleInfoW( LOCALE_USER_DEFAULT, bLongDate ? LOCALE_SLONGDATE : LOCALE_SSHORTDATE, rgwchDateFormat, Q_ARRAYSIZE( rgwchDateFormat ) );

		if ( bOK )
		{
			// with the user's formatting string, we'll build a string in their language
			bOK = 0 != GetDateFormatW( GetLanguageCodeID( language ), 0, &st, rgwchDateFormat, pwch, cchOutput );
		}
	}
	else
	{
		// now generate date using given date picture string
		bOK = 0 != GetDateFormatW( GetLanguageCodeID( language ), 0, &st, CStrAutoEncode( szToken ).ToWString(),
			pwch, cchOutput );
	}

	if ( bOK )
	{
		// convert the output from UTF-16 to UTF-8
		V_strncpy( output, CStrAutoEncode( pwch ).ToString(), cbOutput );
	}
	return bOK;
#elif defined( OSX )
	int cchOutput = cbOutput;
    // get a language code for our locale, then map it to an ICU language code
    CFLocaleRef localeRef = NULL;
    CFDateFormatterRef formatter = NULL;

    if ( szToken == NULL )
    {
        // we don't have a specific date format to use, so just get the OS to give us
        // a generic short date.
        localeRef = CFLocaleCopyCurrent();

        // instantiate date formatter with kCFDateFormatterShortStyle to let the OS pick a short date format
        formatter = CFDateFormatterCreate( kCFAllocatorDefault, localeRef,
			bLongDate ? kCFDateFormatterLongStyle : kCFDateFormatterShortStyle,
			kCFDateFormatterNoStyle );
    }
    else
    {
        // localize via vgui into temporary buffer
        CFStringRef localeIdent = CFStringCreateWithCStringNoCopy( kCFAllocatorDefault, GetLanguageICUName( language ), kCFStringEncodingISOLatin1, kCFAllocatorNull );
        localeRef = CFLocaleCreate( kCFAllocatorDefault, localeIdent );
        CFRelease( localeIdent );
        
        // instantiate formatter with our date picture string
        formatter = CFDateFormatterCreate( NULL, localeRef, kCFDateFormatterNoStyle, kCFDateFormatterNoStyle );
        CFStringRef formatString = CFStringCreateWithBytesNoCopy (
																  kCFAllocatorDefault,
																  reinterpret_cast< uint8* >( output ),
																  cbOutput,
																  kCFStringEncodingUTF8,
																  false,
																  kCFAllocatorNull
																  );

		if ( formatString && formatter )
		{
			CFDateFormatterSetFormat( formatter, formatString );
			CFRelease( formatString );
		}
		else
        {
            if ( formatter )
                CFRelease( formatter );
            if ( formatString )
                CFRelease( formatString );
			return false;
        }

    }

	bool bSuccess = false;

    // compute date (special constant to change epochs)
    CFDateRef date = CFDateCreate( kCFAllocatorDefault, tThen - kCFAbsoluteTimeIntervalSince1970 );
	if ( date )
	{
		// format it
		CFStringRef dateAsString = CFDateFormatterCreateStringWithDate( kCFAllocatorDefault, formatter, date);
		if ( dateAsString )
		{
			// copy it out
			int cch = (int)CFStringGetLength(dateAsString);
			if ( cch > cchOutput )
				cch = cchOutput;
			CFStringGetBytes ( dateAsString, CFRangeMake( 0, cch ), kCFStringEncodingUTF8, 0, false,
							  reinterpret_cast< uint8* >( output ), cbOutput, NULL );
			output[ cch ] = 0;
			bSuccess = true;
			CFRelease( dateAsString );
		}
		CFRelease( date );
	}

	CFRelease( formatter );
    CFRelease( localeRef );

    return bSuccess;
#elif defined( POSIX )
	// we have to use strftime, and we don't have a translator to turn an
	// ISO 8601 date format into something strftime can consume.

	const int cchTime = 256;
	char rgch[ cchTime ];
	if ( strftime( rgch, cchTime, "%x", &tmThen ) > 0 )
	{
		V_strncpy( output, rgch, cbOutput );
		// we can early out
		return true;
	}
	else
	{
		return false;
	}
#else
#error "implement me"
#endif
}



//
// Constructs a time string, based on the passed-in time, relative to the current time.
// Target time can be in the past or the future.
//
// Granularity is in days.
//
//	output varies depending on distance from now:
//	Today
//	<day of week>
//	<short date format as localized by OS>
//
bool ConstructRelativeDateString( char *output, int cbOutput, const char *pchLocPrefix, RTime32 timeTarget, const char *szLanguage, bool bLongDate )
{
	struct tm tmThen;
	struct tm tmNow;

	time_t tThen = timeTarget;
	time_t tNow =  time( NULL );

	if ( !Plat_localtime( &tThen, &tmThen ) ||
		 !Plat_localtime( &tNow,  &tmNow ) )
	{
		return false;
	}

	// in order to make an easier comparison, get an absolute 'days since epoch'. we don't care
	// about leap years, because those will be reflected in tm_yday and we don't perform any
	// checks as long as jan 1 - feb 29.
	int daysThen = tmThen.tm_year * 365 + tmThen.tm_yday;
	int daysNow = tmNow.tm_year * 365 + tmNow.tm_yday;

	const char *szToken = NULL;

	//
	// here's the app logic
	//
	switch ( daysNow - daysThen )
	{
	case 0:
		V_snprintf( output, cbOutput, "%sToday", pchLocPrefix );
		return true;
	case 1:
		V_snprintf( output, cbOutput, "%sYesterday", pchLocPrefix );
		return true;
	case -1:
		V_snprintf( output, cbOutput, "%sTomorrow", pchLocPrefix );
		return true;
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
		// in the future, fewer than 7 days from now
#if _WIN32
		szToken = "dddd";
#else
		szToken = "cccc";
#endif
		break;
	default:
		// some date that's more than a week in the past or future; just use
		// a normal short-date string. Leave szToken NULL.
		break;
	}


	// otherwise delegate to OS specific implementation
	ELanguage language = PchLanguageToELanguage( szLanguage, k_Lang_English );
	return ConstructRelativeDateStringWorker( output, cbOutput, szToken, tThen, tmThen, language, bLongDate );
}


bool ConstructRecentTimeString( char *pszOutput, int cbOutput, const char *pchLocPrefix, RTime32 timeTarget, const char *szLanguage )
{
	struct tm tmThen;
	struct tm tmNow;

	time_t tThen = timeTarget;
	time_t tNow =  time( NULL );

	if ( !Plat_localtime( &tThen, &tmThen ) ||
		 !Plat_localtime( &tNow,  &tmNow ) )
	{
		return false;
	}

	// in order to make an easier comparison, get an absolute 'days since epoch'. we don't care
	// about leap years, because those will be reflected in tm_yday and we don't perform any
	// checks as long as jan 1 - feb 29.
	int daysThen = tmThen.tm_year * 365 + tmThen.tm_yday;
	int daysNow = tmNow.tm_year * 365 + tmNow.tm_yday;

	// If we're not on the same day, fall back to a relative date string
	if ( daysNow != daysThen )
		return ConstructRelativeDateString( pszOutput, cbOutput, pchLocPrefix, timeTarget, szLanguage, false );

	// Otherwise, print a time string

#ifdef _WIN32
	SYSTEMTIME st;
	SystemTimeFromRTime( &st, tThen );

	wchar_t rgwchTimeFormat[100];
	if ( 0 != ::GetTimeFormatW( LOCALE_USER_DEFAULT, TIME_NOSECONDS, &st, NULL, rgwchTimeFormat, Q_ARRAYSIZE(rgwchTimeFormat) ) )
	{
		V_strncpy( pszOutput, CStrAutoEncode( rgwchTimeFormat ).ToString(), cbOutput );
		return true;
	}
	return false;
#else

	// For now, fall back to the date string composer when under OSX/Linux.
	return ConstructRelativeDateString( pszOutput, cbOutput, pchLocPrefix, timeTarget, szLanguage, false );

#endif


}