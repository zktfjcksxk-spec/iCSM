//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx.h"
#include "localize.h"
#include "timeutils.h"
#include "bittools.h"
#if defined( SOURCE2_PANORAMA )
const int k_nSecondsInDay = 60*60*24;
#endif

#ifdef PANORAMA_USE_S1WRAPPER
#include "vstdlib/vstrtools.h"
#include "time.h"
inline const char*	V_strchr( const char *s, char c )				{ return strchr( s, c ); }
bool BGetLocalFormattedDateAndTime( time_t timeVal, char *pchDate, int cubDate, char *pchTime, int cubTime, bool bIncludeSeconds = false, bool bShortDateFormat = false );
#endif

#define V_strnicmp_fast V_strnicmp

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

CLocalization *CLocalizationStringDialogVariablesDerived::m_pLocalizationManager = NULL;
CLocalization *CLocalizationStringSimple::m_pLocalizationManager = NULL;

DEFINE_FIXEDSIZE_ALLOCATOR( CLocalization::PanelLocEntry_t, 1024, CUtlMemoryPool::GROW_SLOW );

#if !defined( SOURCE2_PANORAMA )
static CCommandLineParam g_AllLanguagesCmdLine( "-all_languages", "show longest loc string from any language" );
#endif

// Purpose: format the time and/or date with the user's current locale
// If timeVal is 0, gets the current time
//
// This is generally for use with chatroom dialogs, etc. which need to be
// able to say "Last message received: %date% at %time%"
//
// Note that this uses time_t because RTime32 is not hooked-up on the client
//-----------------------------------------------------------------------------
bool BGetLocalFormattedDateAndTime( time_t timeVal, char *pchDate, int cubDate, char *pchTime, int cubTime, bool bIncludeSeconds /* = false */, bool bShortDateFormat /* = false */ )
{
	if ( 0 == timeVal || timeVal < 0 )
	{
		// get the current time
		time( &timeVal );
	}

	if ( timeVal )
	{
		// Convert it to our local time
		struct tm tmStruct;
		struct tm tmToDisplay = *( Plat_localtime( (const time_t*)&timeVal, &tmStruct ) );
#ifdef OSX
		if ( pchDate != NULL )
		{
			pchDate[ 0 ] = 0;

			CFDateRef date = CFDateCreate( NULL, (CFAbsoluteTime)timeVal - kCFAbsoluteTimeIntervalSince1970 );
			CFLocaleRef currentLocale = CFLocaleCopyCurrent();

			CFDateFormatterStyle eStyle = kCFDateFormatterFullStyle;
			if ( bShortDateFormat )
				eStyle = kCFDateFormatterShortStyle;

			CFDateFormatterRef dateFormatter = CFDateFormatterCreate( NULL, currentLocale, eStyle, kCFDateFormatterNoStyle );
			if ( !dateFormatter )
				return false;
			CFStringRef formattedString = CFDateFormatterCreateStringWithDate( NULL, dateFormatter, date );
			if ( !formattedString )
				return false;

			if ( CFStringGetCString( formattedString, pchDate, cubDate, kCFStringEncodingUTF8 ) == FALSE )
				return false;

			CFRelease( date );
			CFRelease( currentLocale );
			CFRelease( dateFormatter );
			CFRelease( formattedString );
		}

		if ( pchTime != NULL )
		{
			pchTime[ 0 ] = 0;


			CFDateRef date = CFDateCreate( NULL, (CFAbsoluteTime)timeVal - kCFAbsoluteTimeIntervalSince1970 );
			CFLocaleRef currentLocale = CFLocaleCopyCurrent();
			CFDateFormatterRef dateFormatter = CFDateFormatterCreate( NULL, currentLocale, kCFDateFormatterNoStyle, bIncludeSeconds ? kCFDateFormatterLongStyle : kCFDateFormatterShortStyle );
			if ( !dateFormatter )
				return false;
			CFStringRef formattedString = CFDateFormatterCreateStringWithDate( NULL, dateFormatter, date );
			if ( !formattedString )
				return false;

			if ( CFStringGetCString( formattedString, pchTime, cubTime, kCFStringEncodingUTF8 ) == FALSE )
				return false;

			CFRelease( date );
			CFRelease( currentLocale );
			CFRelease( dateFormatter );
			CFRelease( formattedString );
		}

#elif defined( POSIX )
		if ( pchDate != NULL )
		{
			pchDate[ 0 ] = 0;
			if ( bShortDateFormat )
			{
				if ( 0 == strftime( pchDate, cubDate, "%x", &tmToDisplay ) )
					return false;
			}
			else
			{
				if ( 0 == strftime( pchDate, cubDate, "%A %b %d", &tmToDisplay ) )
					return false;
			}
		}

		if ( pchTime != NULL )
		{
			pchTime[ 0 ] = 0;
			if ( 0 == strftime( pchTime, cubTime, "%X", &tmToDisplay ) )
				return false;

			// strftime doesn't have a locale-appropriate way of getting
			// the time without seconds.  We always get the time with
			// seconds and then clip out the seconds (using some fragile
			// assumptions that seconds are always the third set of digits).
			if ( !bIncludeSeconds )
			{
				char *pchScan = pchTime;
				char *pchSecStart = NULL;
				char *pchSecEnd = NULL;
				int nField = 1;

				while ( *pchScan )
				{
					if ( *pchScan < '0' || *pchScan > '9' )
					{
						if ( nField == 2 )
						{
							pchSecStart = pchScan;
						}
						if ( nField == 3 )
						{
							pchSecEnd = pchScan;
						}

						while ( *pchScan &&
								( *pchScan < '0' ||
								*pchScan > '9' ) )
						{
							pchScan++;
						}

						nField++;
					}
					else
					{
						pchScan++;
					}
				}
				if ( pchSecStart != NULL )
				{
					if ( pchSecEnd == NULL )
					{
						// Nothing after the seconds, just truncate.
						*pchSecStart = 0;
					}
					else
					{
						// We have text after the seconds, collapse out the seconds.
						memmove( pchSecStart, pchSecEnd, ( ( pchScan + 1 ) - pchSecEnd ) * sizeof( *pchTime ) );
					}
				}
			}
		}
#else // WINDOWS
		// convert time_t to a SYSTEMTIME
		SYSTEMTIME st;
		st.wHour = tmToDisplay.tm_hour;
		st.wMinute = tmToDisplay.tm_min;
		st.wSecond = tmToDisplay.tm_sec;
		st.wDay = tmToDisplay.tm_mday;
		st.wMonth = tmToDisplay.tm_mon + 1;
		st.wYear = tmToDisplay.tm_year + 1900;
		st.wDayOfWeek = tmToDisplay.tm_wday;
		st.wMilliseconds = 0;

		WCHAR rgwch[ MAX_PATH ];

		if ( pchDate != NULL )
		{
			pchDate[ 0 ] = 0;

			if ( bShortDateFormat )
			{
				if ( !GetDateFormatW( LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, rgwch, MAX_PATH ) )
					return false;
			}
			else
			{
				if ( !GetDateFormatW( LOCALE_USER_DEFAULT, DATE_LONGDATE, &st, NULL, rgwch, MAX_PATH ) )
					return false;
			}
			Q_strncpy( pchDate, CStrAutoEncode( rgwch ).ToString(), cubDate );
		}

		if ( pchTime != NULL )
		{
			DWORD dwFlags = bIncludeSeconds ? 0 : TIME_NOSECONDS;
			pchTime[ 0 ] = 0;
			if ( !GetTimeFormatW( LOCALE_USER_DEFAULT, dwFlags, &st, NULL, rgwch, MAX_PATH ) )
				return false;
			Q_strncpy( pchTime, CStrAutoEncode( rgwch ).ToString(), cubTime );
		}
#endif
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Remove backslash escape characters from a CUtlString in place.
//-----------------------------------------------------------------------------
void UnescapeString( CUtlString &str, int nStartCharIndex )
{
	const char *pch = str.String() + nStartCharIndex;
	char *pchDest = str.Access() + nStartCharIndex;
	while ( *pch != 0 )
	{
		if ( *pch == '\\' )
		{
			pch++;
			if ( *pch )
			{
				*pchDest = *pch;
				pchDest++;
				pch++;
			}
		}
		else
		{
			*pchDest = *pch;
			pchDest++;
			pch++;
		}
	}
	*pchDest = 0;
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CLocalization::CLocalization()
{
	m_sLanguage = "";
	m_bInitialized = true;
	CLocalizationStringDialogVariablesDerived::m_pLocalizationManager = this;
	CLocalizationStringSimple::m_pLocalizationManager = this;

	m_pNullLocString = new CLocalizationStringSimple( "", NULL, k_nLocalizeMaxChars, k_eStringTruncationStyle_None, k_eStringTransformStyle_None, k_eStringEscapeStyle_None, false );
	m_mapNonLocalizedStrings.InsertWithDupes( NULL, m_pNullLocString );
	m_nFirstFreeDialogVariableToPanel = -1;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CLocalization::~CLocalization()
{
	if ( m_bInitialized )
		Shutdown();
}


//-----------------------------------------------------------------------------
// Purpose: shutdown
//-----------------------------------------------------------------------------
void CLocalization::Shutdown()
{
	if( !m_bInitialized )
		return;
	m_bInitialized = false;

	FOR_EACH_MAP_FAST( m_mapIssuedStrings, i )
	{
		SAFE_DELETE( m_mapIssuedStrings[i] );
	}
	m_mapIssuedStrings.RemoveAll();

	FOR_EACH_MAP_FAST( m_mapNonLocalizedStrings, i )
	{
		m_mapNonLocalizedStrings[i]->FreeStringIfNotContained();
		SAFE_DELETE( m_mapNonLocalizedStrings[i] );
	}
	m_mapNonLocalizedStrings.RemoveAll();

	m_mapLocalizationStrings.Purge();
	m_vecLocEntrys.Purge();
	m_allStringData.Purge();

	FOR_EACH_MAP_FAST( m_mapLocStringDialogVariables, i )
	{
		SAFE_DELETE( m_mapLocStringDialogVariables[i] );
	}
	m_mapLocStringDialogVariables.RemoveAll();
}


//-----------------------------------------------------------------------------
// Purpose: load up our loc files
//-----------------------------------------------------------------------------
bool CLocalization::SetLanguage( const char *pchUserLanguage )
{
	if ( !pchUserLanguage || !pchUserLanguage[0] )
		return false;

	if ( m_sLanguage == pchUserLanguage )
		return true;

	m_sLanguage = pchUserLanguage;

	m_sLocalizationFilePath = UIEngine()->GetLocalPathForNamedPath( "{localization}" );

	CUtlVector<CUtlString> vecChangedTokens;

	// check if we already have a loc loaded -- if so, dump all of it so we can start over from English/target
	// language; if we didn't have a loc loaded then we don't have to recalculation work later because we haven't
	// calculated anything for the first time yet
	const bool bReloadTokens = m_mapLocalizationStrings.Count() > 0;

	if ( bReloadTokens )
	{
		// We only want to unload strings that were loaded from a loc file, not those added dynamically.
		StringData_t allStringData;
		LocalizationStringsMap_t mapLocStrings;
		CUtlVector< LocEntry_t > vecLocEntries;
		vecLocEntries.SetGrowSize( 4096 );

		FOR_EACH_HASHMAP( m_mapLocalizationStrings, iMap )
		{
			int nPreviousEntry = -1;
			for ( int nEntryIndex = m_mapLocalizationStrings.Element( iMap ); nEntryIndex != -1; nEntryIndex = m_vecLocEntrys[ nEntryIndex ].m_nNext )
			{
				const LocEntry_t &existingEntry = m_vecLocEntrys[ nEntryIndex ];
				if ( existingEntry.m_bFromLocFile )
					continue;

				int nNewEntryIndex = vecLocEntries.AddToTail( m_vecLocEntrys[ nEntryIndex ] );
				LocEntry_t *pNewEntry = &vecLocEntries[ nNewEntryIndex ];
				// Get new string copies from our new string data holder.
				pNewEntry->m_pString = allStringData.Add( pNewEntry->m_pString );
				pNewEntry->m_pStrToken = allStringData.Add( pNewEntry->m_pStrToken );
				// Make sure that any issued loc strings get their string data
				// pointers recalculated to refer to the new m_vecLocEntrys.
				vecChangedTokens.AddToTail( pNewEntry->m_pStrToken );

				if ( nPreviousEntry >= 0 )
				{
					vecLocEntries[ nPreviousEntry ].m_nNext = nNewEntryIndex;
				}
				else
				{
					// Make sure we use the new allStringData copy for the new map.
					mapLocStrings.Insert( pNewEntry->m_pStrToken, nNewEntryIndex );
				}

				nPreviousEntry = nNewEntryIndex;
				vecLocEntries[ nNewEntryIndex ].m_nNext = -1;
			}
		}

		m_allStringData.Swap( allStringData );
		m_mapLocalizationStrings.Swap( mapLocStrings );
		m_vecLocEntrys.Swap( vecLocEntries );
	}
	else
	{
		m_allStringData.Purge();
		m_mapLocalizationStrings.Purge();
		m_vecLocEntrys.Purge();
		m_vecLocEntrys.SetGrowSize( 4096 );
	}

	// first load english, fail hard if we can't
	if ( !BLoadLocalization( "english", m_sLocalizationFilePath, kKeyReplace_DoNotReplace, vecChangedTokens ) )
		return false;

	// if its NOT english then also load the language specific loc file
	if ( V_stricmp( pchUserLanguage, "english" ) && !BLoadLocalization( pchUserLanguage, m_sLocalizationFilePath, kKeyReplace_ReplaceAny, vecChangedTokens ) )
	{
		Msg( "Failed to load localization file for language %s, ignoring\n", pchUserLanguage );
	}

#if defined( SOURCE2_PANORAMA )
	if ( CommandLine()->HasParm( "-all_languages" ) )
#else
	if ( CommandLine()->CheckParm( g_AllLanguagesCmdLine.GetHParam() ) )
#endif
	{
		// start with English again because it's possible that English has the longest string but we stomped it
		// when loading our target language
		ELanguage eLanguage = k_Lang_English;
		while ( eLanguage != k_Lang_MAX )
		{
			// don't re-add their language
			if ( eLanguage != PchLanguageToELanguage( m_sLanguage.String() ) )
			{
				BLoadLocalization( GetLanguageShortName( eLanguage ), m_sLocalizationFilePath, kKeyReplace_ReplaceMatchingLanguage, vecChangedTokens );
			}

			eLanguage = (ELanguage)(((int)eLanguage)+1);
		}
	}

	// Make sure that we have some extra space in our vector for additions
	// from CreateLocalizationString.  If anything causes the m_vecLocEntrys
	// vector to move in memory after we recalculate it'll mean issued
	// loc instances will have stale pointers.
	m_vecLocEntrys.EnsureCapacity( m_vecLocEntrys.Count() + 1000 );
	
	if ( bReloadTokens )
		RecalculateStringFromReload( vecChangedTokens, false );

	FOR_EACH_MAP_FAST( m_mapIssuedStrings, i )
	{
		ILocalizationString *pLocStr = m_mapIssuedStrings[i];
		if ( !pLocStr->BHasValidString( m_vecLocEntrys.Base(), m_vecLocEntrys.Base() + m_vecLocEntrys.Count() ) )
		{
			ExecuteOnce( Development_AssertMsg( false, "Localization entry for '%s' not recalculated, pointing to garbage", m_mapIssuedStrings.Key( i ).String() ) );
		}
	}
	FOR_EACH_MAP_FAST( m_mapNonLocalizedStrings, i )
	{
		ILocalizationString *pLocStr = m_mapNonLocalizedStrings[i];
		if ( !pLocStr->BHasValidString( m_vecLocEntrys.Base(), m_vecLocEntrys.Base() + m_vecLocEntrys.Count() ) )
		{
			ExecuteOnce( Development_AssertMsg( false, "Non-localized localization entry '%s' (%d) not recalculated, pointing to garbage", m_mapNonLocalizedStrings[i]->String(), i ) );
		}
	}

	return true;
}


void CLocalization::UnloadLocalizationFileStrings( void )
{
	m_vecLocalizationFiles.RemoveAll();
	const char * pszLanguage = m_sLanguage.String();
	m_sLanguage = UTL_INVAL_SYMBOL;
	SetLanguage( pszLanguage );
}

//-----------------------------------------------------------------------------
// Purpose: given a language parse its loc file
//-----------------------------------------------------------------------------
bool CLocalization::BLoadLocalization( const char *pchLanguage, const char *pchBaseDir, EKeyReplaceStrategy eKeyReplaceStrategy, CUtlVector<CUtlString> &vecChangedTokens )
{
#if defined( SOURCE2_PANORAMA )
	char szLocFile[ 128 ];
	V_snprintf( szLocFile, sizeof(szLocFile), "%s/panorama_%s.txt", pchBaseDir, pchLanguage );
	if ( !UIEngine()->UIFileSystem()->FileExists( szLocFile ) )
		return false;

	return BLoadLocalizationFile( szLocFile, pchLanguage, eKeyReplaceStrategy, vecChangedTokens );
#else
	char szLocFile[128];
	V_snprintf( szLocFile, sizeof( szLocFile ), "tenfoot_%s.txt", pchLanguage );
	CPathString locFile( szLocFile, pchBaseDir );
	if ( !UIEngine()->UIFileSystem()->FileExists( locFile.GetUTF8Path() ) )
		return false;

	return BLoadLocalizationFile( locFile.GetUTF8Path(), pchLanguage, eKeyReplaceStrategy, vecChangedTokens );
#endif

}

static void ConstructLocalizationFilePath( char *pchPath, int maxLenInChars, const char *pFormat, const char *pchDir, const char* pchFilePrefix, const char* pchLang )
{
	char szTempPath[ 128 ];
	V_sprintf_safe( szTempPath, pFormat, pchDir, pchFilePrefix, pchLang );

	// Fixup path name
	// "panorama/localization/.." gets fixed up to "panorama"
	// This prevents failures to find files on posix when panorama/localization directory doesn't exist.
	V_FixupPathName( pchPath, maxLenInChars, szTempPath );
}

//-----------------------------------------------------------------------------
// Purpose: load an extra, optional loc file, used in Source2
//-----------------------------------------------------------------------------
bool CLocalization::BLoadLocalizationFile( const char *pchFilePrefix )
{
	m_vecLocalizationFiles.AddToTail( pchFilePrefix );

	bool bLoadedFile = false;
	char szLocFile[ 128 ];
	CUtlVector<CUtlString> vecChangedTokens;

	// try to load the base english file
	ConstructLocalizationFilePath( szLocFile, sizeof( szLocFile ), "%s/%s_%s.txt", m_sLocalizationFilePath.String(), pchFilePrefix, "english" );

	if ( UIEngine()->UIFileSystem()->FileExists( szLocFile ) )
	{
		bLoadedFile = BLoadLocalizationFile( szLocFile, "english", kKeyReplace_ReplaceAny, vecChangedTokens );
		Shipping_AssertMsg( bLoadedFile, "Failed to load base english localization file as backup to %s!", szLocFile );
	}

	if ( !( m_sLanguage == "english" ) )
	{
		// and now load the language itself
		ConstructLocalizationFilePath( szLocFile, sizeof( szLocFile ), "%s/%s_%s.txt", m_sLocalizationFilePath.String(), pchFilePrefix, m_sLanguage.String() );

		if ( UIEngine()->UIFileSystem()->FileExists( szLocFile ) )
		{
			bool bLoadedSpecificLanguage = BLoadLocalizationFile( szLocFile, m_sLanguage.String(), kKeyReplace_ReplaceAny, vecChangedTokens );
			bLoadedFile |= bLoadedSpecificLanguage;
			Shipping_AssertMsg( bLoadedSpecificLanguage, "Failed to load localization file %s!", szLocFile );
		}

		// in Perfect World mode we also load _pw override (see localize.cpp under src/localize at CLocalize::AddFile too)
		static bool sbPerfectWorld = CommandLine()->HasParm( "-perfectworld" );
		if ( sbPerfectWorld )
		{
			ConstructLocalizationFilePath( szLocFile, sizeof( szLocFile ), "%s/%s_%s_pw.txt", m_sLocalizationFilePath.String(), pchFilePrefix, m_sLanguage.String() );
			if ( UIEngine()->UIFileSystem()->FileExists( szLocFile ) )
			{
				bool bLoadedSpecificLanguage = BLoadLocalizationFile( szLocFile, m_sLanguage.String(), kKeyReplace_ReplaceAny, vecChangedTokens );
				bLoadedFile |= bLoadedSpecificLanguage;
				// Shipping_AssertMsg( bLoadedSpecificLanguage, "Failed to load localization file %s!", szLocFile );
			}
		}
	}

#if defined( SOURCE2_PANORAMA )
	if( CommandLine()->HasParm( "-all_languages" ) )
#else
	if( CommandLine()->CheckParm( g_AllLanguagesCmdLine.GetHParam() ) )
#endif
	{
		// start with English again because it's possible that English has the longest string but we stomped it
		// when loading our target language
		ELanguage eLanguage = k_Lang_English;
		while ( eLanguage != k_Lang_MAX )
		{
			// don't re-add their language
			if ( eLanguage != PchLanguageToELanguage( m_sLanguage.String() ) )
			{
				ConstructLocalizationFilePath( szLocFile, sizeof( szLocFile ), "%s/%s_%s.txt", m_sLocalizationFilePath.String(), pchFilePrefix, GetLanguageShortName( eLanguage ) );
				if ( UIEngine()->UIFileSystem()->FileExists( szLocFile ) )
					BLoadLocalizationFile( szLocFile, GetLanguageShortName( eLanguage ), kKeyReplace_ReplaceMatchingLanguage, vecChangedTokens );
			}

			eLanguage = (ELanguage)(((int)eLanguage) + 1);
		}
	}

	return bLoadedFile;
}


//-----------------------------------------------------------------------------
// Purpose: add a callback handler for a custom dialog variable type
//-----------------------------------------------------------------------------
void CLocalization::InstallCustomDialogVariableHandler( const char *pchCustomHandlerName, PFNLocalizeDialogVariableHandler pfnLocalizeFunc, PFNParseDialogVariableModifiersHandler pfnParseModifiers, void *pUserData, bool bVirtual )
{
	if ( m_mapGenericDialogVariableHandlers.HasElement( pchCustomHandlerName ) || m_mapVirtualDialogVariableHandlers.HasElement( pchCustomHandlerName ) )
	{
		AssertMsg1( false, "Custom Dialog Variable Handler already installed! '%s'\n", pchCustomHandlerName );
		return;
	}

	DialogVariableHandler_t newHandler;
	newHandler.m_pfnHandler = pfnLocalizeFunc;
	newHandler.m_pfnParseModifiers = pfnParseModifiers;
	newHandler.m_pUserData = pUserData;

	if ( bVirtual )
	{
		m_mapVirtualDialogVariableHandlers.Insert( pchCustomHandlerName, newHandler );
	}
	else
	{
		m_mapGenericDialogVariableHandlers.Insert( pchCustomHandlerName, newHandler );
	}
}


//-----------------------------------------------------------------------------
// Purpose: remove a callback handler for a custom dialog variable type
//-----------------------------------------------------------------------------
void CLocalization::RemoveCustomDialogVariableHandler( const char *pchCustomHandlerName )
{
	if ( m_mapGenericDialogVariableHandlers.HasElement( pchCustomHandlerName ) )
	{
		m_mapGenericDialogVariableHandlers.Remove( pchCustomHandlerName );
	}
	else if ( m_mapVirtualDialogVariableHandlers.HasElement( pchCustomHandlerName ) )
	{
		m_mapVirtualDialogVariableHandlers.Remove( pchCustomHandlerName );
	}
	else
	{
		AssertMsg1( false, "Attempt to remove unknown custom Dialog Variable Handler! '%s'\n", pchCustomHandlerName );
	}
}


//-----------------------------------------------------------------------------
// Purpose: callback from the source2 filesystem that our file changed
//-----------------------------------------------------------------------------
void CLocalization::OnLocalizationFileChanged( const char *pFullPath )
{
	if ( UIEngine() )
	{
		((CLocalization*)UIEngine()->UILocalize())->ReloadChangedFile( pFullPath );
	}
}

//-----------------------------------------------------------------------------
// Purpose: given a language parse its loc file
//-----------------------------------------------------------------------------
bool CLocalization::BLoadLocalizationFile( const char *pchLocalizationFile, const char *pchLanguage, EKeyReplaceStrategy eKeyReplaceStrategy, CUtlVector<CUtlString> &vecChangedTokens )
{
	bool bEnglishFile = V_stricmp( pchLanguage, "english" ) == 0;

	// Make sure to load these files as binary, not text. If it is a UTF16 file, fread seems to sometimes barf when reading
	// in text mode (e.g. dota_schinese.txt). In Source 2, KeyValues will parse/convert to UTF8 appropriately. In steam, all
	// the loc files should be utf8, so we don't want the CRT messing with the contents anyways.
	CUtlBuffer buf;
	uint nPadding = sizeof( wchar_t );
	if ( !UIEngine()->UIFileSystem()->LoadFileIntoBuffer( pchLocalizationFile, buf, false, &OnLocalizationFileChanged, nPadding ) || buf.TellPut() == 0 )
	{
		// we may fail to load the file because the process writing it still owns the exclusive file lock on it
		// just sleep for a little bit and try again
		if ( UIEngine()->UIFileSystem()->FileExists( pchLocalizationFile ) )
			ThreadSleep( 100 );

		if ( !UIEngine()->UIFileSystem()->LoadFileIntoBuffer( pchLocalizationFile, buf, false, &OnLocalizationFileChanged, nPadding ) || buf.TellPut() == 0 )
		{
			AssertMsg1( false, "Failed to load KV file \"%s\"\n", pchLocalizationFile );
			return false;
		}
	}

	// Make sure the buffer is null terminated. Always put a unicode null at the end, even if the buffer
	// is utf8. An extra 0 never hurt anybody.
	wchar_t wcNull = L'\0';
	buf.Put( &wcNull, sizeof( wcNull ) );

#ifdef PANORAMA_USE_S1WRAPPER
	// Translate UTF-16 into UTF-8 before proceeding
	// Note that KeyValues::LoadFromBuffer will translate Unicode files into UTF-8 (using V_UnicodeToUTF8)
	// which is failing on linux. Force the translation from UTF-16 to UTF-8.
	const char *pUTF16Buf = (const char *)buf.Base();
	int nUTF16Len = V_strlen( pUTF16Buf );
	if ( nUTF16Len > 2 && (uint8)pUTF16Buf[0] == 0xFF && (uint8)pUTF16Buf[1] == 0xFE )
	{
		int nUTF8Len = V_UTF16ToUTF8( (uchar16*)( pUTF16Buf + 2 ), NULL, 0 );
		char *pUTF8Buf = new char[nUTF8Len];
		V_UTF16ToUTF8( (uchar16*)( pUTF16Buf + 2 ), pUTF8Buf, nUTF8Len );
		buf.AssumeMemory( pUTF8Buf, nUTF8Len, nUTF8Len, CUtlBuffer::READ_ONLY | CUtlBuffer::TEXT_BUFFER );
	}
#endif

#if defined( SOURCE2_PANORAMA ) && !defined( PANORAMA_USE_S1WRAPPER )
	CTemporaryKeyValues *pTmpKv = KeyValues::LoadTemporaryFromBuffer( true, pchLocalizationFile, (const char *)buf.Base() );
	KeyValues *pKVLoc = pTmpKv ? pTmpKv->GetKeyValues() : nullptr;
#else
	KeyValues *pKVLoc = new KeyValues( "Localization" );
	pKVLoc->UsesEscapeSequences( true );

#if !defined( PANORAMA_USE_S1WRAPPER )
	KeyValuesTextParser kvParser( false );
	if ( !pKVLoc->LoadFromBuffer( pchLocalizationFile, (const char *)buf.Base(), &kvParser ) )
#else
	if ( !pKVLoc->LoadFromBuffer( pchLocalizationFile, (const char *)buf.Base(), NULL, NULL, NULL, true ) )
#endif
	{
		pKVLoc->deleteThis();
		pKVLoc = nullptr;
	}
#endif

	if ( !pKVLoc )
	{
		AssertMsg1( false, "Failed to KV parse file \"%s\"\n", pchLocalizationFile );
		return false;
	}

#if !defined( SOURCE2_PANORAMA )
	if ( kvParser.BErrorsOccurred() )
	{
		AssertMsg1( false, "Syntax check failed to file \"%s\" : ", pchLocalizationFile );
		AssertMsg( false, kvParser.GetErrorText() );
		pKVLoc->deleteThis();
		return false;
	}
#endif

	KeyValues *pKVTokens = pKVLoc;

	// Is this an old style localization file where the tokens are not listed at the root?
	KeyValues *pFirstKey = pKVLoc->GetFirstSubKey();
	if ( pFirstKey && V_stricmp( pFirstKey->GetName(), "Language" ) == 0 )
	{
		KeyValues *pKVActualTokens = pKVLoc->FindKey( "Tokens" );
		if ( pKVActualTokens )
		{
			pKVTokens = pKVActualTokens;
		}
	}

	// Count number of tokens to save memory relocates on vectors
	int numTokens = 0;
	FOR_EACH_VALUE( pKVTokens, pKVSub )
	{
		// Skip [english] tokens in other language files
		if ( !bEnglishFile && V_strnicmp_fast(pKVSub->GetName(), "[english]", 9) == 0 ) 
			continue;
		++numTokens;
	}

	// Allocate enough space for the strings as well as a buffer for incidental ones
	m_vecLocEntrys.EnsureCapacity( m_vecLocEntrys.Count() + numTokens + 1000 );

	FOR_EACH_VALUE( pKVTokens, pKVSub )
	{
		const char *pchKeyName = pKVSub->GetName();
		const char *pchKeyValue = pKVSub->GetString();

		// Skip [english] tokens in other language files
		if ( !bEnglishFile && V_strnicmp_fast( pchKeyName, "[english]", 9) == 0 ) 
			continue;

		int iIndex = m_mapLocalizationStrings.Find( pchKeyName );
		if ( iIndex == m_mapLocalizationStrings.InvalidIndex() )
		{
			vecChangedTokens.AddToTail( pchKeyName );
			pchKeyName = m_allStringData.Add( pchKeyName );
			iIndex = m_mapLocalizationStrings.Insert( pchKeyName, -1 );
		}
		else if( eKeyReplaceStrategy != kKeyReplace_ReplaceMatchingLanguage )
		{
			pchKeyName = m_mapLocalizationStrings.Key( iIndex );
#ifdef DEBUG
			if ( !m_hashLanguagesCheckedForDupes.HasElement( pchLanguage ) )
			{
				for ( int locIndex = m_mapLocalizationStrings[iIndex]; locIndex != -1; locIndex = m_vecLocEntrys[locIndex].m_nNext )
				{
					auto *locStr = &m_vecLocEntrys[locIndex];
					if ( locStr->m_sLanguage == pchLanguage )
					{
						AssertMsg2( false, "Localization string %s in file %s has a duplicate!\n", pchKeyName, pchLocalizationFile );
						break;
					}
				}
			}
#endif
		}

#ifdef PANORAMA_USE_S1WRAPPER

		// Replace old style variable name %s1, %s2, ... %s9 with the corresponding {s:s1}, ..., {s:s9}

		CUtlString newStyleKeyValue;
		const char *pCurPos = pchKeyValue;
		if ( pCurPos )
		{
			bool bReplacedVar = false;
			while ( *pCurPos )
			{
				// Search for %s1, %s2 ... %s9
				const char *pNextVar = V_strstr( pCurPos, "%s" );
				if ( pNextVar && !V_isdigit( pNextVar[2] ) )
				{
					pNextVar = nullptr;
				}

				if ( !pNextVar )
				{
					// only need to append if we replaced an old style variable
					if ( bReplacedVar )
					{
						newStyleKeyValue += pCurPos;
					}
					break;
				}

				// Copy up to the old style variable
				int nNumCharsToCopy = pNextVar - pCurPos;
				if ( nNumCharsToCopy )
				{
					// append up to the undesired substring
					CUtlString temp = pCurPos;
					temp = temp.Left( nNumCharsToCopy );
					newStyleKeyValue += temp;
				}

				// append new style variable
				newStyleKeyValue.Append( "{s:s" );
				newStyleKeyValue += pNextVar[2];
				newStyleKeyValue += '}';
				bReplacedVar = true;

				// skip past the old style variable
				pCurPos = pNextVar + 3;
			}

			// Points pchKeyValue to the new string if necessary
			if ( bReplacedVar )
			{
				pchKeyValue = newStyleKeyValue.Get();
			}
		}

#endif

		bool bFoundEntry = false;
		if ( eKeyReplaceStrategy != kKeyReplace_DoNotReplace )
		{
			for ( int locIndex = m_mapLocalizationStrings[iIndex]; locIndex != -1; locIndex = m_vecLocEntrys[locIndex].m_nNext )
			{
				LocEntry_t *locStr = &m_vecLocEntrys[ locIndex ];
				if ( V_stricmp( locStr->m_pStrToken, pchKeyName ) != 0 )
					continue;

				// nothing to do if we already have this language/localized string pair (skip the language check because we only
				// care about many multiple languages (besides English/target) for -all_languages, and there if the tokens resolve
				// to the same thing we also don't really care)
				if ( V_strcmp( locStr->m_pString, pchKeyValue ) == 0 )
					break;

				// if we're only affecting contents for keys in a specific language, ignore any values we have for this key in
				// other languages
				if ( eKeyReplaceStrategy == kKeyReplace_ReplaceMatchingLanguage && V_strcmp( locStr->m_sLanguage.String(), pchLanguage ) != 0 )
					continue;

				Assert( eKeyReplaceStrategy == kKeyReplace_ReplaceAny || eKeyReplaceStrategy == kKeyReplace_ReplaceMatchingLanguage );
			
				locStr->m_pString = m_allStringData.Add( pchKeyValue ); // clobber with the new key
				locStr->m_sLanguage = pchLanguage;
				vecChangedTokens.AddToTail( pchKeyName );

				bFoundEntry = true;
				break;
			}
		}

		if ( !bFoundEntry )
		{
			int vecLoc = m_vecLocEntrys.AddToTail();

			LocEntry_t &locEntry = m_vecLocEntrys.Element( vecLoc );

			locEntry.m_sLanguage = pchLanguage;
			locEntry.m_pString = m_allStringData.Add( pchKeyValue );
			// pchKeyName has been updated to guarantee it refers
			// to data in m_allStringData.
			locEntry.m_pStrToken = pchKeyName;
			locEntry.m_bFromLocFile = true;
			locEntry.m_nNext = m_mapLocalizationStrings[iIndex];

			m_mapLocalizationStrings[iIndex] = vecLoc;
		}
	}

#if defined( SOURCE2_PANORAMA ) && !defined( PANORAMA_USE_S1WRAPPER )
	delete pTmpKv;
#else
	pKVLoc->deleteThis();
#endif
	m_hashLanguagesCheckedForDupes.InsertOrReplace( pchLanguage, true );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: find a string from our table, optionally allowing variable substitution
//			even if the string isn't in a loc file.
//-----------------------------------------------------------------------------
const ILocalizationString *CLocalization::PchFindToken( const IUIPanel *pPanel, const char *pchToken, const uint32 ccMax, EStringTruncationStyle eTrunkStyle, EStringTransformStyle eTransformStyle, EStringEscapeStyle eEscapeStyle, bool bAllowDialogVariable, bool bReturnWrappedKeyIfMissing )
{
	if ( !pchToken || !pchToken[0] )
		return m_pNullLocString;
	
	bool bIsLocalizationToken = false;
	if ( pchToken[0] == '#' )
	{
		if ( pchToken[1] != '#' ) // ignore "##FFFFFFFF" color codes
		{
			pchToken++; // move past the hash char
			int iIndex = m_mapLocalizationStrings.Find( pchToken );
			if ( iIndex != m_mapLocalizationStrings.InvalidIndex() )
			{
				for ( int locIndex = m_mapLocalizationStrings[iIndex]; locIndex != -1; locIndex = m_vecLocEntrys[locIndex].m_nNext )
				{
					LocEntry_t *locStr = &m_vecLocEntrys[locIndex];
					if ( V_stricmp( locStr->m_pStrToken, pchToken ) == 0 )
					{
						bIsLocalizationToken = locStr->m_bFromLocFile;
						break;
					}
				}
			}
			else
			{
				const char *pchPanelID = pPanel ? pPanel->GetID() : "";

				// if we have a panel with no name, find first named ancestor
				if ( pPanel && ( !pchPanelID || !pchPanelID[0] ) )
				{
					for ( IUIPanel *pParent = pPanel->GetParent(); pParent; pParent = pParent->GetParent() )
					{
						const char *pchParentID = pParent->GetID();
						if ( pchParentID && pchParentID[0] )
						{
							Msg( "**** Unable to localize '#%s' on panel descendant of '%s'\n", pchToken, pchParentID );
							break;
						}
					}
				}
				else
				{
					Msg( "**** Unable to localize '#%s' on panel '%s'\n", pchToken, pchPanelID ? pchPanelID : "" );
				}
			}
		}
	}

	if ( !bReturnWrappedKeyIfMissing && !bIsLocalizationToken )
		return NULL;

	return CreateLocalizationString( pPanel, pchToken, ccMax, eTrunkStyle, eTransformStyle, eEscapeStyle, bIsLocalizationToken || bAllowDialogVariable, false ); 
}


//-----------------------------------------------------------------------------
// Purpose: create this new string
//-----------------------------------------------------------------------------
const ILocalizationString *CLocalization::PchSetString( const IUIPanel *pPanel, const char *pchText, const uint32 ccMax , EStringTruncationStyle eTrunkStyle, EStringTransformStyle eTransformStyle, EStringEscapeStyle eEscapeStyle, bool bAllowDialogVariable, bool bStringAlreadyFullyParsed  )
{
	if ( !pchText || !pchText[0] )
		return m_pNullLocString;

	return CreateLocalizationString( pPanel, pchText, ccMax, eTrunkStyle, eTransformStyle, eEscapeStyle, bAllowDialogVariable, bStringAlreadyFullyParsed );
}


//-----------------------------------------------------------------------------
// Purpose: get the raw string value from this token, without any replacement of dialog vars or the like
//-----------------------------------------------------------------------------
const char *CLocalization::PchFindRawString( const char *pchToken )
{
	if ( !pchToken || !pchToken[0] || pchToken[0] != '#' ) // only accept hashed strings
		return NULL;

	pchToken++; // dodge the # char
	int iIndex = m_mapLocalizationStrings.Find( pchToken );
	if ( iIndex == m_mapLocalizationStrings.InvalidIndex() )
		return NULL;

	for ( int locIndex = m_mapLocalizationStrings[iIndex]; locIndex != -1; locIndex = m_vecLocEntrys[locIndex].m_nNext )
	{
		auto *locStr = &m_vecLocEntrys[locIndex];
		if ( V_stricmp( locStr->m_pStrToken, pchToken ) == 0 )
		{
			if ( !locStr->m_bFromLocFile )
				return NULL;
			return locStr->m_pString;
		}
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: make a CLocalizationStringSimple if needed and return it for use
//-----------------------------------------------------------------------------
CLocalizationStringSimple *CLocalization::CreateSimpleLocalizedString( const IUIPanel *pPanel, const char *pchValue, const uint32 ccMax,
	EStringTruncationStyle eTrunkStyle, EStringTransformStyle eTransformStyle, EStringEscapeStyle eEscapeStyle,
	bool bFromLocString, const char *pchToken, bool bStringAlreadyFullyParsed )
{
	CLocalizationStringSimple *pLocString = new CLocalizationStringSimple( pchValue, pPanel, ccMax, eTrunkStyle, eTransformStyle, eEscapeStyle, bStringAlreadyFullyParsed );
	if ( bFromLocString )
		m_mapIssuedStrings.InsertWithDupes( pchToken, pLocString );
	else
		m_mapNonLocalizedStrings.InsertWithDupes( pPanel, pLocString );
	return pLocString;
}

//-----------------------------------------------------------------------------
// Purpose: Configure the global map of dialog vars to local strings
//-----------------------------------------------------------------------------
void CLocalization::SetDialogVariablesToPanel( CCopyableUtlVector<DialogVariable_t> &vecDialogVars, const IUIPanel *pPanel, PanelLocEntry_t *pLocEntry )
{
	for ( const auto &dialogVar : vecDialogVars )
	{
		const auto &variableName = dialogVar.m_sVariableName;    
		auto dialogPanelMapIndex = m_mapDialogVariableToPanels.Find( variableName );
		if ( dialogPanelMapIndex == m_mapDialogVariableToPanels.InvalidIndex() )
		{
			dialogPanelMapIndex = m_mapDialogVariableToPanels.Insert( variableName, -1 );
		}

		int nListIndex = m_nFirstFreeDialogVariableToPanel;

		// If the free list is empty alloc a new one
		if ( m_nFirstFreeDialogVariableToPanel == -1 )
		{
			nListIndex = m_vecDialogVariableToPanels.AddToTail( DialogVariableToPanel_t() );
		}
		else
		{
			// Move the free list
			m_nFirstFreeDialogVariableToPanel = m_vecDialogVariableToPanels[ m_nFirstFreeDialogVariableToPanel ].m_nNext;
		}

		auto &dialogVariableToPanel = m_vecDialogVariableToPanels[ nListIndex ];
		dialogVariableToPanel.Set( dialogVar.m_eType, pPanel, pLocEntry );
		dialogVariableToPanel.m_nNext = m_mapDialogVariableToPanels.Element( dialogPanelMapIndex );
		m_mapDialogVariableToPanels.Element( dialogPanelMapIndex ) = nListIndex;
	}
}


//-----------------------------------------------------------------------------
// Check that the localization string is still known to the localization
// system.  For internal consistency checking.
//-----------------------------------------------------------------------------
bool CLocalization::IsValidLocalizationString( const ILocalizationString *pLocStr )
{
	FOR_EACH_MAP_FAST( m_mapIssuedStrings, i )
	{
		if ( m_mapIssuedStrings[i] == pLocStr )
		{
			return true;
		}
	}
	FOR_EACH_MAP_FAST( m_mapNonLocalizedStrings, i )
	{
		if ( m_mapNonLocalizedStrings[i] == pLocStr )
		{
			return true;
		}
	}
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: make a CLocalizationString if needed and return it for use
//-----------------------------------------------------------------------------
ILocalizationString *CLocalization::CreateLocalizationString( const IUIPanel *pPanel, const char *pchToken, const uint32 ccMax, 
	EStringTruncationStyle eTrunkStyle, EStringTransformStyle eTransformStyle, EStringEscapeStyle eEscapeStyle,
	bool bAllowDialogVariableParsing, bool bStringAlreadyFullyParsed )
{
	int iIndex = m_mapLocalizationStrings.Find( pchToken );
	const char *pchLocalizedString = pchToken;
	bool bFromLocFile = false;
	bool bFoundLocEntry = false;
	int ivecLoc = 0;
	if ( !bStringAlreadyFullyParsed && iIndex != m_mapLocalizationStrings.InvalidIndex() )
	{
		for ( int locIndex = m_mapLocalizationStrings[iIndex]; locIndex != -1; locIndex = m_vecLocEntrys[locIndex].m_nNext )
		{
			auto *locStr = &m_vecLocEntrys[locIndex];
			if ( V_stricmp( locStr->m_pStrToken, pchToken ) == 0 )
			{
				pchLocalizedString = locStr->m_pString; // we had a loc token for it, use that string rather than the passed in token
				bFromLocFile = locStr->m_bFromLocFile;
				bFoundLocEntry = true;
				ivecLoc = locIndex;
				break;
			}
		}
	}


	// We can't let m_vecLocEntrys grow here as if the vector moves in memory all of
	// the loc instance pointers into its strings will be invalid.  We could
	// try and recalcualate everything but this is a very rare thing, particularly
	// since we deliberately add space to m_vecLocEntrys to try and handle a
	// reasonable number of additions without growth.
	CCopyableUtlVector<DialogVariable_t> vecDialogVars;
	if ( !bAllowDialogVariableParsing ||
		 !CLocalizationStringDialogVariables::BParseDialogVariables( pchLocalizedString, vecDialogVars, bFromLocFile ) ||
		 ( !bFoundLocEntry && m_vecLocEntrys.Count() >= m_vecLocEntrys.NumAllocated() ) )
	{
		return CreateSimpleLocalizedString( pPanel, pchLocalizedString, k_nLocalizeMaxChars, k_eStringTruncationStyle_None, eTransformStyle, eEscapeStyle, bFromLocFile, pchToken, bStringAlreadyFullyParsed );
	}
	else
	{
		Assert( ccMax == k_nLocalizeMaxChars && eTrunkStyle == k_eStringTruncationStyle_None ); // we don't trunk real loc strings, if you hit this do you have a bug?
		
		// Overflow scenario: 
		//
		// Calling this function with a string containing dialog vars, with bAllowDialogVariableParsing = true, but 
		// with bStringAlreadyFullyParsed = false causes new entries to be added to m_vecLocEntrys, even if an entry 
		// already exists for the input string. One example is setting dialog var text on a label with text type k_ETextTypeUnlocalized.
		// For example, this code, called by keyboard options reset causes about 40 dupes to be added each time 
		// the reset button is pressed.
		//
		// m_pButton->SetTextWithDialogVariables( CFmtStr( "{v:csgo_bind:e:bind_%s}", szBindName ), panorama::CLabel::k_ETextTypeUnlocalized );
		//
		// m_vecLocEntrys is fixed size. It has space for 1000 new entries, and if it overflows, dialog vars stop being parsed.
		//
		// Finding all call sites which might result in this overflow condition seems hard. Putting in a stopgap for now, pending 
		// integration of latest version of this file from Dota, where the code looks very different. 

		if ( !bFoundLocEntry ) // Look again, in case we didn't look before
		{
			if ( iIndex != m_mapLocalizationStrings.InvalidIndex() )
			{
				for ( int locIndex = m_mapLocalizationStrings[iIndex]; locIndex != -1; locIndex = m_vecLocEntrys[locIndex].m_nNext )
				{
					auto *locStr = &m_vecLocEntrys[locIndex];
					if ( V_stricmp( locStr->m_pStrToken, pchToken ) == 0 )
					{
						pchLocalizedString = locStr->m_pString; // we had a loc token for it, use that string rather than the passed in token
						bFromLocFile = locStr->m_bFromLocFile;
						bFoundLocEntry = true;
						ivecLoc = locIndex;
						break;
					}
				}
			}
		}

		if ( !bFoundLocEntry ) // we didn't have an entry for it from our loc files, add a synthetic entry now to track it
		{
			if ( iIndex == m_mapLocalizationStrings.InvalidIndex() )
			{
				pchToken = m_allStringData.Add( pchToken );
				iIndex = m_mapLocalizationStrings.Insert( pchToken, -1 );
			}
			else
			{
				pchToken = m_mapLocalizationStrings.Key( iIndex );
			}

			ivecLoc = m_vecLocEntrys.AddToTail();
			LocEntry_t &locEntry = m_vecLocEntrys.Element( ivecLoc );
			locEntry.m_sLanguage = m_sLanguage;
			locEntry.m_pString = m_allStringData.Add( pchLocalizedString );
			// pchToken is known to come from m_allStringData.
			locEntry.m_pStrToken = pchToken;
			locEntry.m_bFromLocFile = false;
			locEntry.m_nNext = m_mapLocalizationStrings[iIndex];

			m_mapLocalizationStrings[iIndex] = ivecLoc;
		}
		LocEntry_t &locEntry = m_vecLocEntrys[ivecLoc];

		int iDialogVar = m_mapLocStringDialogVariables.Find( pchToken );
		if ( iDialogVar == m_mapLocStringDialogVariables.InvalidIndex() )
			iDialogVar = m_mapLocStringDialogVariables.Insert( pchToken, new CLocalizationStringDialogVariables( &locEntry.m_pString, vecDialogVars ) );

		// if this string has dialog variables we need to instance it
		CLocalizationStringDialogVariablesDerived *pLoc = new CLocalizationStringDialogVariablesDerived( m_mapLocStringDialogVariables[iDialogVar], eTransformStyle, eEscapeStyle, pPanel );
		int iIssuedString = m_mapIssuedStrings.InsertWithDupes( pchToken, pLoc );

		int iPanelList = m_mapLocStringsOwnedByPanel.Find( pPanel );
		if ( iPanelList == m_mapLocStringsOwnedByPanel.InvalidIndex() )
			iPanelList = m_mapLocStringsOwnedByPanel.Insert( pPanel );
		PanelLocEntry_t *pPanelLocEntry = new PanelLocEntry_t( pLoc, pchToken );
		m_mapLocStringsOwnedByPanel[iPanelList].AddToTail( pPanelLocEntry );

		// Now map the dialog variables to the panel
		SetDialogVariablesToPanel( vecDialogVars, pPanel, pPanelLocEntry );

		pLoc->Recalculate( &locEntry.m_pString );
		return m_mapIssuedStrings[iIssuedString];
	}
}


//-----------------------------------------------------------------------------
// Purpose: Changes the transform style on a string. Will release old pointer, so can call with: pPointer = ChangeTransformStyleAndRelease( pPointer, ... );
//-----------------------------------------------------------------------------
const ILocalizationString *CLocalization::ChangeTransformStyleAndRelease( const ILocalizationString *pLocalizationString, EStringTransformStyle eTranformStyle )
{
	// if the style is not changing, just return the same pointer
	if ( eTranformStyle == pLocalizationString->GetTransformStyle() )
		return pLocalizationString;	

	// see if this is from a localization token
	// need a better way to look this up
	CUtlString strToken;
	FOR_EACH_MAP_FAST( m_mapIssuedStrings, i )
	{
		if ( m_mapIssuedStrings[i] == pLocalizationString )
		{
			strToken = m_mapIssuedStrings.Key( i );
			break;
		}
	}

	const ILocalizationString *pRet = NULL;
	const char *pchToken = strToken.IsValid() ? strToken.String() : pLocalizationString->StringNoTransform();
	pRet = CreateLocalizationString( pLocalizationString->GetOwningPanel(), pchToken, pLocalizationString->GetMaxChars(), pLocalizationString->GetTruncationStyle(), eTranformStyle, pLocalizationString->GetEscapeStyle(), true, false );

	pLocalizationString->Release();
	return pRet;
}


//-----------------------------------------------------------------------------
// Purpose: Clone an existing loc string making a copy of it keeping the existing reference 
//-----------------------------------------------------------------------------
ILocalizationString *CLocalization::CloneString( const IUIPanel *pPanel, const ILocalizationString *pLocToken, bool bStringAlreadyFullyParsed )
{
	// see if this is from a localization token
	// need a better way to look this up
	CUtlString strToken;
	FOR_EACH_MAP_FAST( m_mapIssuedStrings, i )
	{
		if ( m_mapIssuedStrings[i] == pLocToken )
		{
			strToken = m_mapIssuedStrings.Key( i );
			break;
		}
	}

	// default to provided string
	const char *pchToken = strToken.IsValid() ? strToken.String() : pLocToken->String();
	return CreateLocalizationString( pPanel, pchToken, pLocToken->GetMaxChars(), pLocToken->GetTruncationStyle(), pLocToken->GetTransformStyle(), pLocToken->GetEscapeStyle(), true, bStringAlreadyFullyParsed );
}


//-----------------------------------------------------------------------------
// Purpose: checks for changed layout and style files, and reloads any that changed
//-----------------------------------------------------------------------------
void CLocalization::ReloadChangedFile( const char *pchFile )
{
	bool bReloadedFiles = false;
	CUtlVector<CUtlString> vecChangedTokens;
	const char *pchTextExt = V_stristr( pchFile, ".txt" );
	const char *pchLocalizationDir = V_stristr( pchFile, "localization" CORRECT_PATH_SEPARATOR_S ); // also check its under our loc sub-dir
	if ( pchLocalizationDir && pchTextExt && *( pchTextExt + 4 ) == 0 )
	{
		const char *pchLangStart = strrchr( pchFile, '_' );
		if ( pchLangStart )
		{
			CUtlString sLang;
			sLang.SetDirect( pchLangStart + 1, pchTextExt - pchLangStart - 1 );
			if ( !sLang.IsEmpty() )
			{
				// only replace keys in their existing language to avoid changing English stomping localized strings, or to avoid confusing -all_languages
				CPathString strPath( pchFile );
				BLoadLocalizationFile( strPath.GetUTF8Path(), sLang, kKeyReplace_ReplaceMatchingLanguage, vecChangedTokens );
				bReloadedFiles = true;
			}
		}
	}

	if ( bReloadedFiles )
		RecalculateStringFromReload( vecChangedTokens, true );
}


//-----------------------------------------------------------------------------
// Purpose: after a reload recalculate our now reloaded strings as needed
//-----------------------------------------------------------------------------
void CLocalization::RecalculateStringFromReload( CUtlVector<CUtlString> &vecChangedTokens, bool bStringsValid )
{
    FOR_EACH_VEC( vecChangedTokens, i )
    {
        int iLocStringIndex = m_mapLocalizationStrings.Find( vecChangedTokens[i] );
        if ( iLocStringIndex != m_mapLocalizationStrings.InvalidIndex() )
        {
            int ivecLoc;
			for ( ivecLoc = m_mapLocalizationStrings[iLocStringIndex]; ivecLoc != -1; ivecLoc = m_vecLocEntrys[ivecLoc].m_nNext )
			{
                if ( V_stricmp( m_vecLocEntrys[ivecLoc].m_pStrToken, vecChangedTokens[i] ) == 0 )
                {
                    break;
                }
            }

            if ( ivecLoc != -1 )
            {
				auto *locStr = &m_vecLocEntrys[ivecLoc];

                // check if we have a dialog var object that needs updating
                int iDialogVar = m_mapLocStringDialogVariables.Find( vecChangedTokens[i] );
                if ( iDialogVar != m_mapLocStringDialogVariables.InvalidIndex() )
                {
                    CCopyableUtlVector<DialogVariable_t> vecDialogVars;
                    bool bHasVariables = CLocalizationStringDialogVariables::BParseDialogVariables( locStr->m_pString, vecDialogVars, true );
                    Assert( bHasVariables );
                    if ( !bHasVariables )
                        AssertMsg1( false, "All variables removed from string %s, not supported", locStr->m_pString );
                    m_mapLocStringDialogVariables[iDialogVar]->Set( &locStr->m_pString, vecDialogVars );
                }

				// now refresh any of the derived objects
				int iIndex = m_mapIssuedStrings.FindFirst( vecChangedTokens[i] );
				if ( iIndex != m_mapIssuedStrings.InvalidIndex() )
				{
					do
					{
						m_mapIssuedStrings[iIndex]->Recalculate( &locStr->m_pString );
						iIndex = m_mapIssuedStrings.NextInorderSameKey( iIndex );
					} while ( iIndex != m_mapIssuedStrings.InvalidIndex() );
				}
				else if ( bStringsValid )
				{
					// See if we have a non-loc'd string that is this token, if so update to this new string.
					// We can only do this check if the file reload case and not after SetLanguage where
					// we may have discarded all of the string data.
					FOR_EACH_MAP( m_mapNonLocalizedStrings, j )
					{
						// its a loc string if it starts with #
						if (  m_mapNonLocalizedStrings[j]->String()[0] == '#' && vecChangedTokens[i] == m_mapNonLocalizedStrings[j]->String()+1  )
						{
							CCopyableUtlVector<DialogVariable_t> vecDialogVars;
							bool bHasVariables = CLocalizationStringDialogVariables::BParseDialogVariables( m_mapNonLocalizedStrings[j]->String(), vecDialogVars, true );
							Assert( !bHasVariables );
							if ( bHasVariables ) 
							{
								// TODO - need an indirection layer if we want to runtime swap from simple to dialog vars loc string
								AssertMsg1( false, "Variables added to new string %s, not supported, restart app to see change", vecChangedTokens[i].String() );
							}
							else
							{
								m_mapNonLocalizedStrings[j]->Recalculate( &locStr->m_pString );
							}
						}
					}
				} // else
			} // if ( ivecLoc != m_mapLocalizationStrings[iLocStringIndex].InvalidIndex() )
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: helper to print a numerical value and a loc string together
//-----------------------------------------------------------------------------
void CLocalization::PrintIntAndStringHelper( char *pBuf, int ccBuf, int iVal, const char *pchLocString )
{
	int iLocStr = m_mapLocalizationStrings.Find( pchLocString );
	if ( iLocStr != m_mapLocalizationStrings.InvalidIndex() )
	{
		for ( int locIndex = m_mapLocalizationStrings[iLocStr]; locIndex != -1; locIndex = m_vecLocEntrys[locIndex].m_nNext )
		{
			auto *locStr = &m_vecLocEntrys[locIndex];
			if ( V_stricmp( locStr->m_pStrToken, pchLocString ) == 0 )
			{
				V_snprintf( pBuf, ccBuf, "%d %s", iVal, locStr->m_pString );
				break;
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: given a int and loc strings for the singular and plural case, construct a string
//-----------------------------------------------------------------------------
void CLocalization::PrintTimeHelper( char *pBuf, int ccBuf, int iVal, const char *pchLocStringPlural, const char *pchLocString )
{
	if ( pBuf[0] )
	{
		pBuf[0] = ' ';
		pBuf++;
	}

	pBuf[0] = 0;
	PrintIntAndStringHelper( pBuf, ccBuf, iVal, iVal > 1 ? pchLocStringPlural : pchLocString );
}


//-----------------------------------------------------------------------------
// Purpose: given a key/value pair is updates for a loc string recalc its actual value
//-----------------------------------------------------------------------------
bool CLocalization::ResolveDialogVariable( CUtlString& /*out*/ strResult, EPanelKeyType eType, EStringEscapeStyle eEscapeStyle, const char *pchKey, uint32 nModifiers, const char *pchParams, const IUIPanel *pPanel )
{
	VPROF_BUDGET( "CLocalization::ResolveDialogVariable",  VPROF_BUDGETGROUP_TENFOOT );

	// resolve virtual dialog variable handlers
	if ( eType == k_ePanelVartype_Virtual )
	{
		int iFoundHandler = m_mapVirtualDialogVariableHandlers.Find( pchParams );
		if ( iFoundHandler != m_mapVirtualDialogVariableHandlers.InvalidIndex() )
		{
			DialogVariableHandler_t &handler = m_mapVirtualDialogVariableHandlers[iFoundHandler];
			return handler.m_pfnHandler( strResult, pchKey, 0, pPanel, pchKey, nModifiers, handler.m_pUserData );
		}
	}

	const IUIPanel *pPanelToConsider = pPanel;
	while ( pPanelToConsider )
	{
		int iPanelVariableList = m_mapPanelVariables.Find( pPanelToConsider );
		if ( iPanelVariableList != m_mapPanelVariables.InvalidIndex() )
		{
			FOR_EACH_VEC( m_mapPanelVariables[iPanelVariableList], i )
			{
				CPanelKeyValue &panelVariable = m_mapPanelVariables[iPanelVariableList][i];
				if ( panelVariable.m_symKey == pchKey &&
					( panelVariable.m_eType == eType
						|| eType == k_ePanelVartype_Generic
						|| ( panelVariable.m_eType == k_ePanelVartype_Uint64 && eType == k_ePanelVartype_Number )
						|| ( panelVariable.m_eType == k_ePanelVartype_Time && eType == k_ePanelVartype_Number ) ) )
				{
					switch ( eType )
					{
						case k_ePanelVartype_String:
							strResult = panelVariable.m_sValue;

							if ( ( nModifiers & k_ePanelKeyStringModifiers_CaseMask ) != 0 )
							{
								int nConvFlags = ( nModifiers & k_ePanelKeyStringModifiers_Uppercase ) != 0 ? STRINGCASE_UPPER : STRINGCASE_LOWER;
								strResult.UnicodeCaseConvert( nConvFlags | STRINGCASE_FLAG_LINGUISTIC );
							}

							if ( ( nModifiers & k_ePanelKeyStringModifiers_AllowHTML ) == 0 && eEscapeStyle == k_eStringEscapeStyle_HTML )
							{
								static const int kStackAllocMaxSize = 2000;
								const char* szUnencoded = strResult.Get();
								int nUnencodedSize = strResult.Length();

								// Calculate size of HTML-escaped text
								int nEncodedSize;
								V_BasicHtmlEntityEncode( nullptr, 0, szUnencoded, nUnencodedSize, &nEncodedSize );

								// HTML-escape
								char *szEncoded = ( char* )( ( nEncodedSize > kStackAllocMaxSize ) ? malloc( nEncodedSize ) : stackalloc( nEncodedSize ) );
								bool bSuccess = V_BasicHtmlEntityEncode( szEncoded, nEncodedSize, szUnencoded, nUnencodedSize );
								Assert( bSuccess ); // should always succeed because we allocated the requested size

								strResult = szEncoded;

								// free temporary string if necessary
								if ( nEncodedSize > kStackAllocMaxSize )
									free( szEncoded );
							}
							break;
						case k_ePanelVartype_Number:
							if ( nModifiers & k_ePanelKeyNumberModifiers_RawNumber )
							{
								if ( panelVariable.m_eType == k_ePanelVartype_Uint64 )
									strResult = CNumStr( panelVariable.m_number64 );
								else if ( panelVariable.m_eType == k_ePanelVartype_Time )
									strResult = CNumStr( ( int )panelVariable.m_time );
								else
									strResult = CNumStr( panelVariable.m_number );
							}
							else
							{
								int64 numberValue;
								if ( panelVariable.m_eType == k_ePanelVartype_Uint64 )
									numberValue = panelVariable.m_number64;
								else if ( panelVariable.m_eType == k_ePanelVartype_Time )
									numberValue = ( int )panelVariable.m_time;
								else
									numberValue = panelVariable.m_number;
								strResult = V_pretifynum( numberValue );
							}
							break;
						case k_ePanelVartype_Uint64:
							strResult = CNumStr( panelVariable.m_number64 );
							break;

						case k_ePanelVartype_Money:
						{
							char szCurrencyBuf[128];
							panelVariable.Amount().ToStringUTF8( szCurrencyBuf, sizeof( szCurrencyBuf ), CurrentLanguage() );
							strResult = szCurrencyBuf;
						}
							break;
						case k_ePanelVartype_Generic:
						{
							// For generics, look for a matching custom handler
							int iFoundHandler = m_mapGenericDialogVariableHandlers.Find( pchParams );
							if ( iFoundHandler != m_mapGenericDialogVariableHandlers.InvalidIndex() )
							{
								DialogVariableHandler_t &handler = m_mapGenericDialogVariableHandlers[ iFoundHandler ];
								return handler.m_pfnHandler( strResult, panelVariable.m_sValue, panelVariable.m_number, pPanel, pchKey, nModifiers, handler.m_pUserData );
							}
						}
							return false; // no handler for that generic var type, can't possibly succeed
						case k_ePanelVartype_Time:
						{
							char dateString[256];
							Assert( nModifiers != 0 );
							if ( nModifiers & k_ePanelKeyTimeModifiers_Relative )
							{
								time_t timeNow = time( NULL );

#if defined( SOURCE2_PANORAMA )
								if( nModifiers & k_ePanelKeyTimeModifiers_ServerTime )
								{
									timeNow = steamAPIContext.SteamUtils() ? steamAPIContext.SteamUtils()->GetServerRealTime() : time( nullptr );
								}
#endif

								strResult = "";
								if ( panelVariable.m_time == 0 )
								{
									int iLocStr = m_mapLocalizationStrings.Find( "UI_UnknownTime" ); // the unknown time marker
									if ( iLocStr != m_mapLocalizationStrings.InvalidIndex() )
									{
										for ( int locIndex = m_mapLocalizationStrings[iLocStr]; locIndex != -1; locIndex = m_vecLocEntrys[locIndex].m_nNext )
										{
											auto *locStr = &m_vecLocEntrys[locIndex];
											if ( V_stricmp( locStr->m_pStrToken, "UI_UnknownTime" ) == 0 )
											{
												strResult = locStr->m_pString;
												break;
											}
										}
									}
								}
								else if ( ( nModifiers & k_ePanelKeyTimeModifiers_LongDate ) 
									|| ( !( nModifiers & k_ePanelKeyTimeModifiers_ShortDate ) && abs( panelVariable.m_time - timeNow ) < k_nSecondsInDay ) )
								{
									// only give relative time resolution for values less than a day, otherwise us day increments
									CUtlString sTimeString;
									char timeString[256] = { 0 };
									// calculate days/hours/minutes/seconds from overall seconds value
									int seconds = abs( panelVariable.m_time - timeNow );
									int days = seconds / 86400;
									seconds %= 86400;
									int hours = seconds / 3600;
									seconds %= 3600;
									int minutes = seconds / 60;
									seconds %= 60;

									bool bIncludeDays = 0 != ( nModifiers & k_ePanelKeyTimeModifiers_LongDate );

									timeString[0] = 0;

									bool bShortFormat = ( nModifiers & k_ePanelKeyTimeModifiers_ShortTime ) != 0;
									int cFields = 0;

									if ( !bIncludeDays )
									{
										Assert( days == 0 );
									}
									else if ( days )
									{
										PrintTimeHelper( timeString, sizeof( timeString ), days, "UI_Days", "UI_Day" );
										sTimeString += timeString;
										cFields++;
									}

									if ( hours > 0 )
									{
										PrintTimeHelper( timeString, sizeof( timeString ), hours, "UI_Hours", "UI_Hour" );
										sTimeString += timeString;
										cFields++;
									}

									if ( minutes > 0 )
									{
										PrintTimeHelper( timeString, sizeof( timeString ), minutes, "UI_Minutes", "UI_Minute" );
										sTimeString += timeString;
										cFields++;
									}

									if ( !( nModifiers & k_ePanelKeyTimeModifiers_Minutes ) && ( !bShortFormat || cFields < 2 ) )
									{
										if ( seconds > 0 )
										{
											PrintTimeHelper( timeString, sizeof( timeString ), seconds, "UI_Seconds", "UI_Second" );
											sTimeString += timeString;
										}
									}

									strResult = sTimeString;

								}
								else
								{
									if ( ConstructRelativeDateString( dateString, sizeof(dateString), "UI_", panelVariable.m_time, GetLanguageShortName( PchLanguageToELanguage( m_sLanguage.String()  ) ) ) )
									{
										if ( dateString[0] == 'U' && dateString[1] == 'I'  && dateString[2] == '_'  )
										{
											int iLocStr = m_mapLocalizationStrings.Find( dateString );
											if ( iLocStr != m_mapLocalizationStrings.InvalidIndex() )
											{										
												for ( int locIndex = m_mapLocalizationStrings[iLocStr]; locIndex != -1; locIndex = m_vecLocEntrys[locIndex].m_nNext )
												{
													auto *locStr = &m_vecLocEntrys[locIndex];
													if ( V_stricmp( locStr->m_pStrToken, dateString ) == 0 )
													{
														strResult = locStr->m_pString;
													}
												}
											}
										}
										else
									{
										strResult = dateString;
									}
								}
							}
							}
							else if ( nModifiers & k_ePanelKeyTimeModifiers_Duration )
							{
								strResult = "";

								if ( nModifiers & k_ePanelKeyTimeModifiers_LongTime )
								{
									int nDays = panelVariable.m_time / ( 24 * 60 * 60 );

									char timeString[ 256 ] = { 0 };
									PrintTimeHelper( timeString, sizeof( timeString ), nDays, "UI_Days", "UI_Day" );

									strResult += timeString;
								}
								else
								{
									int nHours = panelVariable.m_time / 3600;
									int nMinutes = ( panelVariable.m_time % 3600 ) / 60;
									int nSeconds = ( ( panelVariable.m_time % 3600 ) % 60 );

									if ( nHours > 0 )
									{
										strResult += CFmtStr( "%d:%02d:%02d", nHours, abs( nMinutes ), abs( nSeconds ) );
									}
									else
									{
										strResult += CFmtStr( "%02d:%02d", nMinutes, abs( nSeconds ) );
									}
								}
							}
							else
							{
								strResult = "";
								char timeString[256];
								bool bIncludeSeconds = ( nModifiers & k_ePanelKeyTimeModifiers_LongTime ) != 0;
								bool bShortDateFormat = ( nModifiers & k_ePanelKeyTimeModifiers_ShortDate ) != 0;
								if ( BGetLocalFormattedDateAndTime( (time_t)panelVariable.m_time, dateString, V_ARRAYSIZE( dateString ), timeString, k_cchFormattedTime, bIncludeSeconds, bShortDateFormat ) )
								{
									bool bNeedsSpace = false;
									if ( ( nModifiers & k_ePanelKeyTimeModifiers_ShortDate ) || ( nModifiers & k_ePanelKeyTimeModifiers_LongDate ) || ( nModifiers & k_ePanelKeyTimeModifiers_DateTime ) )
									{
										strResult += dateString;
										bNeedsSpace = true;
									}

									if ( ( nModifiers & k_ePanelKeyTimeModifiers_ShortTime ) || ( nModifiers & k_ePanelKeyTimeModifiers_LongTime ) || ( nModifiers & k_ePanelKeyTimeModifiers_DateTime ) )
									{
										if ( bNeedsSpace )
											strResult += " ";

										strResult += timeString;
									}
								}
							}
						}
							break;
						default:
							return false;
					}
					return true;
				}
			}
		}
		pPanelToConsider = pPanelToConsider->ClientPtr()->GetLocalizationParent();
	}
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: set a string value
//-----------------------------------------------------------------------------
bool CLocalization::SetDialogVariable( const IUIPanel *pPanel, const char *pchKey, const char *pchValue )
{
	return SetDialogVariableHelper( pPanel, CPanelKeyValue( pchKey, pchValue ) );
}


//-----------------------------------------------------------------------------
// Purpose: set a time value
//-----------------------------------------------------------------------------
#if defined( SOURCE2_PANORAMA )
bool CLocalization::SetDialogVariable( const IUIPanel *pPanel, const char *pchKey, time_t timeVal )
#else
bool CLocalization::SetDialogVariable( const IUIPanel *pPanel, const char *pchKey, CRTime timeVal )
#endif
{

#if defined( SOURCE2_PANORAMA )
	return SetDialogVariableHelper( pPanel, CPanelKeyValue( pchKey, (time_t)timeVal ) );
#else
	return SetDialogVariableHelper( pPanel, CPanelKeyValue( pchKey, (time_t)timeVal.GetRTime32() ) );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: set a currency value
//-----------------------------------------------------------------------------
#if defined( SOURCE2_PANORAMA )
bool CLocalization::SetDialogVariable( const IUIPanel *pPanel, const char *pchKey, CCurrencyAmount amount )
#else
bool CLocalization::SetDialogVariable( const IUIPanel *pPanel, const char *pchKey, CAmount amount )
#endif
{
	return SetDialogVariableHelper( pPanel, CPanelKeyValue( pchKey, amount ) );
}


//-----------------------------------------------------------------------------
// Purpose: set a int value
//-----------------------------------------------------------------------------
bool CLocalization::SetDialogVariable( const IUIPanel *pPanel, const char *pchKey, int nVal )
{
	return SetDialogVariableHelper( pPanel, CPanelKeyValue( pchKey, nVal ) );
}


//-----------------------------------------------------------------------------
// Purpose: set a uint64 value
//-----------------------------------------------------------------------------
bool CLocalization::SetDialogVariable( const IUIPanel *pPanel, const char *pchKey, uint64 nVal64 )
{
	return SetDialogVariableHelper( pPanel, CPanelKeyValue( pchKey, nVal64 ) );
}


//-----------------------------------------------------------------------------
// Purpose: set a key/value for this particular panel
//-----------------------------------------------------------------------------
bool CLocalization::SetDialogVariableHelper( const IUIPanel *pPanel, const CPanelKeyValue &panelKeyValue )
{
	int iPanel = m_mapPanelVariables.Find( pPanel );
	if ( iPanel == m_mapPanelVariables.InvalidIndex() )
		iPanel = m_mapPanelVariables.Insert( pPanel );

	int iKey = m_mapPanelVariables[iPanel].Find( panelKeyValue );
	if ( iKey == m_mapPanelVariables[iPanel].InvalidIndex() )
	{
		iKey = m_mapPanelVariables[iPanel].AddToTail();
		m_mapPanelVariables[iPanel][iKey] = panelKeyValue;
		// now check if this panel or any children need updating
		CheckPanelNeedsLocUpdate( pPanel, panelKeyValue );
	}
	else if ( !m_mapPanelVariables[iPanel][iKey].BCompareValues( panelKeyValue ) )
	{
		m_mapPanelVariables[iPanel][iKey] = panelKeyValue;

		// now check if this panel or any children need updating
		CheckPanelNeedsLocUpdate( pPanel, panelKeyValue );
	}
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: copy all the current dialog vars on this panel to a new one
//-----------------------------------------------------------------------------
void CLocalization::CloneDialogVariables( const IUIPanel *pPanelFrom, IUIPanel *pPanelTo )
{
	int iPanelFrom = m_mapPanelVariables.Find( pPanelFrom );
	if ( iPanelFrom == m_mapPanelVariables.InvalidIndex() )
		return;

	int iPanelTo = m_mapPanelVariables.Find( pPanelTo );
	if ( iPanelTo == m_mapPanelVariables.InvalidIndex() )
		iPanelTo = m_mapPanelVariables.Insert( pPanelTo ); // no entry for this panel yet, this is the expected path
	else
		m_mapPanelVariables[ iPanelTo ].RemoveAll(); // hmm, already have some dialog vars, lets clear em out

	FOR_EACH_VEC( m_mapPanelVariables[ iPanelFrom ], iKeyFrom )
	{
		int iKeyTo = m_mapPanelVariables[ iPanelTo ].AddToTail();
		m_mapPanelVariables[ iPanelTo ][ iKeyTo ] = m_mapPanelVariables[ iPanelFrom ][ iKeyFrom ];
		CheckPanelNeedsLocUpdate( pPanelTo, m_mapPanelVariables[ iPanelTo ][ iKeyTo ] );
	}
}


//-----------------------------------------------------------------------------
// Purpose: force a re-evaluation of a specific dialog variable
//-----------------------------------------------------------------------------
void CLocalization::DirtyDialogVariable( const IUIPanel *pPanel, const char *pchKey )
{
	CheckPanelNeedsLocUpdate( pPanel, CPanelKeyValue( pchKey ) );  // value is not relevant when checking for update, just the key is
}


//-----------------------------------------------------------------------------
// Purpose: walk from a panel to all its children and apply a dialog variable change if needed
//-----------------------------------------------------------------------------
void CLocalization::CheckPanelNeedsLocUpdate( const IUIPanel *pPanel, const CPanelKeyValue &panelKeyValue )
{
	VPROF_BUDGET( "CLocalization::CheckPanelNeedsLocUpdate",  VPROF_BUDGETGROUP_TENFOOT );

	auto dialogPanelMapIndex = m_mapDialogVariableToPanels.Find( panelKeyValue.m_symKey );
	if ( dialogPanelMapIndex == m_mapDialogVariableToPanels.InvalidIndex() )
	{
		return;
	}

	for ( int dvIndex = m_mapDialogVariableToPanels.Element( dialogPanelMapIndex ); 
		  dvIndex != -1; 
		  dvIndex = m_vecDialogVariableToPanels[ dvIndex ].m_nNext )
	{
		// This is a panel that uses this dialog variable, but we don't know if it was affected by this change yet.
		const DialogVariableToPanel_t &dialogVariablePanelVecEntry = m_vecDialogVariableToPanels.Element( dvIndex );
		const IUIPanel *pLocPanel = dialogVariablePanelVecEntry.m_pPanel;

		if ( pPanel == nullptr && dialogVariablePanelVecEntry.m_eType == k_ePanelVartype_Virtual )
		{
			// Virtual dialog variables don't actually exist on any panel, so always recalculate them
			int iLocString = m_mapLocalizationStrings.Find( dialogVariablePanelVecEntry.m_pLocEntry->m_symToken );
			if ( iLocString != m_mapLocalizationStrings.InvalidIndex() )
			{
				int locIndex = m_mapLocalizationStrings[iLocString];
				auto *pLocEntry = &m_vecLocEntrys[locIndex];

				dialogVariablePanelVecEntry.m_pLocEntry->m_pLoc->Recalculate( &pLocEntry->m_pString );
			}
		}
		else if ( pPanel != nullptr && dialogVariablePanelVecEntry.m_eType != k_ePanelVartype_Virtual )
		{
			// Normal dialog variable.  Check if the dirty panel is a parent of the panel that uses this variable
			// and, if it is, recalculate the target string.
			while ( pLocPanel )
			{
				if ( pLocPanel == pPanel )
				{
					int iLocString = m_mapLocalizationStrings.Find( dialogVariablePanelVecEntry.m_pLocEntry->m_symToken );
					if ( iLocString != m_mapLocalizationStrings.InvalidIndex() )
					{
						int locIndex = m_mapLocalizationStrings[iLocString];
						auto *pLocEntry = &m_vecLocEntrys[locIndex];

						dialogVariablePanelVecEntry.m_pLocEntry->m_pLoc->Recalculate( &pLocEntry->m_pString );
					}
					break;
				}
				pLocPanel = pLocPanel->ClientPtr()->GetLocalizationParent();
			}
		}
	}
}

void CLocalization::RemoveDialogVariablesToPanel( const IUIPanel *pPanel, ILocalizationString *pLocStr )
{
	// Remove dialog variables pointing to this panel
	FOR_EACH_HASHMAP( m_mapDialogVariableToPanels, mapDialogVariableToPanelsIndex )
	{
		int *pCurIndex = &m_mapDialogVariableToPanels.Element( mapDialogVariableToPanelsIndex );
		while ( *pCurIndex != -1 )
		{
			auto &dialogVariableToPanel = m_vecDialogVariableToPanels.Element( *pCurIndex );
			int nNext = dialogVariableToPanel.m_nNext;

			if ( dialogVariableToPanel.m_pPanel == pPanel &&
				 ( !pLocStr || dialogVariableToPanel.m_pLocEntry->m_pLoc == pLocStr ) ) 
			{
				dialogVariableToPanel.m_nNext = m_nFirstFreeDialogVariableToPanel;
				m_nFirstFreeDialogVariableToPanel = *pCurIndex;
				*pCurIndex = nNext;
			}
			else
			{
				pCurIndex = &dialogVariableToPanel.m_nNext;
			}
		}

		if ( m_mapDialogVariableToPanels.Element( mapDialogVariableToPanelsIndex ) == -1 )
		{
			m_mapDialogVariableToPanels.RemoveAt( mapDialogVariableToPanelsIndex );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: clean up loc strings from a panel that went away
//-----------------------------------------------------------------------------
void CLocalization::OnPanelDeleted( const IUIPanel *pPanel )
{
	VPROF_BUDGET( "CLocalization::OnPanelDeleted",  VPROF_BUDGETGROUP_TENFOOT );

	// now delete all the loc strings we handed out to this panel
	int iPanel = m_mapLocStringsOwnedByPanel.Find( pPanel );
	if ( iPanel != m_mapLocStringsOwnedByPanel.InvalidIndex() )
	{
		RemoveDialogVariablesToPanel( pPanel, nullptr );

		// for each loc string this panel created
		FOR_EACH_VEC( m_mapLocStringsOwnedByPanel[iPanel], i )
		{
			// find its Loc entry in the issued string map and delete it
			int iIssuedString = m_mapIssuedStrings.FindFirst( m_mapLocStringsOwnedByPanel[iPanel][i]->m_symToken );
			if ( iIssuedString != m_mapIssuedStrings.InvalidIndex() )
			{
				do 
				{
					if ( m_mapIssuedStrings[iIssuedString] == m_mapLocStringsOwnedByPanel[iPanel][i]->m_pLoc )
					{
						int iToDelete = iIssuedString;
						iIssuedString = m_mapIssuedStrings.NextInorderSameKey( iIssuedString );
						delete m_mapIssuedStrings.Element( iToDelete );
						m_mapIssuedStrings.RemoveAt( iToDelete ); // pull this entry from our map
					}
					else
					{
						iIssuedString = m_mapIssuedStrings.NextInorderSameKey( iIssuedString );
					}

				} while ( iIssuedString != m_mapIssuedStrings.InvalidIndex() );
			}
		}

		for ( auto &pLoc : m_mapLocStringsOwnedByPanel[iPanel] )
		{
			delete pLoc;
		}
		m_mapLocStringsOwnedByPanel.RemoveAt( iPanel );
	}	

	// now cleanup the un-loc'd strings we handed out for the panel
	int iNonLocStrings = m_mapNonLocalizedStrings.FindFirst( pPanel );
	if ( iNonLocStrings != m_mapNonLocalizedStrings.InvalidIndex() )
	{
		do 
		{
			int iToDelete = iNonLocStrings;
			iNonLocStrings = m_mapNonLocalizedStrings.NextInorderSameKey( iNonLocStrings );
			Assert( m_mapNonLocalizedStrings[ iToDelete ]->IsUsingContainedString() );
			delete m_mapNonLocalizedStrings[ iToDelete ];
			m_mapNonLocalizedStrings.RemoveAt( iToDelete );
			
		} while ( iNonLocStrings != m_mapNonLocalizedStrings.InvalidIndex() );
	}

	// Delete map of panel variables for this panel
	m_mapPanelVariables.Remove( pPanel );
}

//-----------------------------------------------------------------------------
// Purpose: clean up loc strings from a panel that went away
//-----------------------------------------------------------------------------
void CLocalization::Release( const IUIPanel *pPanel, ILocalizationString *pLocStr )
{
	if ( pLocStr == m_pNullLocString )
		return;

	// remove localized string from panel map
	int iPanel = m_mapLocStringsOwnedByPanel.Find( pPanel );
	if ( iPanel != m_mapLocStringsOwnedByPanel.InvalidIndex() )
	{
		RemoveDialogVariablesToPanel( pPanel, pLocStr );

		CCopyableUtlVector<PanelLocEntry_t *> &vec = m_mapLocStringsOwnedByPanel.Element( iPanel );
		FOR_EACH_VEC_BACK( vec, iPanelLoc )
		{
			if ( vec[iPanelLoc]->m_pLoc == pLocStr )
			{
				delete vec[iPanelLoc];
				vec.Remove( iPanelLoc );
			}
		}
	}

	// delete the symbol lookup for this string if we have one
	FOR_EACH_MAP_FAST( m_mapIssuedStrings, i )
	{
		if ( m_mapIssuedStrings[i] == pLocStr )
		{
			m_mapIssuedStrings.RemoveAt( i );
			delete pLocStr;
			return;
		}
	}

	// now delete the storage for the class
	CLocalizationStringSimple *pSimpleLoc = dynamic_cast<CLocalizationStringSimple *>( pLocStr );
	if ( pSimpleLoc )
	{
		FOR_EACH_MAP_FAST( m_mapNonLocalizedStrings, i )
		{
			if ( m_mapNonLocalizedStrings[i] == pLocStr )
			{
				m_mapNonLocalizedStrings.RemoveAt( i );
				Assert( pSimpleLoc->IsUsingContainedString() );
				delete pSimpleLoc;
				break;
			}
		}
	}
	else
	{
		FOR_EACH_MAP_FAST( m_mapNonLocalizedStrings, i )
		{
			if ( m_mapNonLocalizedStrings[i] == pLocStr )
			{
				delete (CLocalizationStringDialogVariables *)pLocStr;
				m_mapNonLocalizedStrings.RemoveAt( i );
				break;
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Parse the given modifiers string for the given generic dialog
//          variable handler
//-----------------------------------------------------------------------------
uint32 CLocalization::ParseGenericDialogVariableModifiers( bool bVirtual, const CUtlString &strKey, const CUtlString &strModifiers )
{
	const auto& map = bVirtual ? m_mapVirtualDialogVariableHandlers : m_mapGenericDialogVariableHandlers;

	int i = map.Find( strKey );
	if ( i == map.InvalidIndex() )
		return 0;

	const DialogVariableHandler_t &handler = map[ i ];
	if ( !handler.m_pfnParseModifiers )
		return 0;

	return handler.m_pfnParseModifiers( strModifiers.Get(), handler.m_pUserData );
}


//-----------------------------------------------------------------------------
// Purpose: Find the next open brace in the string. This is strchr but smart
//			about escape characters.
//-----------------------------------------------------------------------------
const char * CLocalizationStringDialogVariables::FindNextBrace( const char *pch )
{
	while ( *pch != 0 && *pch != '{' )
	{
		if ( *pch == '\\' )
		{
			pch++;
			if ( *pch == 0 )
			{
				return nullptr;
			}
		}
		pch++;
		if ( *pch == '{' ) // sniff for the ":" char to confirm this is a dialog variable
		{
			if ( pch[1] == 0 || pch[2] == 0 ) // string ends, just bail
				return nullptr;

			if ( pch[2] != ':' ) // no ":" 2 down the string? Then not a dialog var start, move on!
			{
				pch += 3;
			}

		}
	}

	return (*pch == 0) ? nullptr : pch;
}


//-----------------------------------------------------------------------------
// Purpose: parse a dialog variable from this string if it has one
//-----------------------------------------------------------------------------
bool CLocalizationStringDialogVariables::BParseDialogVariables( const char *pchBaseString, CCopyableUtlVector<DialogVariable_t> &vecDialogVars, bool bManagedLocString )
{
	const char *pchPrevStringEnd = pchBaseString;
	const char *pchOpenBracketChar = FindNextBrace( pchBaseString );
	const char *pchEndBracketChar = strchr( pchBaseString, '}' );
	if ( !pchEndBracketChar || pchEndBracketChar == pchOpenBracketChar+1 ) // ignore just a { in a string, or {}
		return false;

	while ( pchOpenBracketChar )
	{
		pchOpenBracketChar++; // skip the {

		// add the piece before the { char
		DialogVariable_t dialogVarStart;
		dialogVarStart.m_eType = k_ePanelVartype_None;
		if ( ( pchOpenBracketChar - 1 - pchPrevStringEnd ) > 0  )
			dialogVarStart.m_sVariableName.SetDirect( pchPrevStringEnd, pchOpenBracketChar - 1 - pchPrevStringEnd );

		DialogVariable_t dialogVar;

		// parse out the type
		switch( pchOpenBracketChar[0] )
		{
		case 's':
		case 'S':
			dialogVar.m_eType = k_ePanelVartype_String;
			while ( pchOpenBracketChar[1] == ':' && pchOpenBracketChar[2] && pchOpenBracketChar[3] == ':' ) // check for modifiers
			{
				pchOpenBracketChar += 2;

				switch ( pchOpenBracketChar[0] )
				{
				case 'u': // uppercase
					dialogVar.m_fModifiers &= ~k_ePanelKeyStringModifiers_CaseMask;
					dialogVar.m_fModifiers |= k_ePanelKeyStringModifiers_Uppercase;
					break;
				case 'l': // lowercase
					dialogVar.m_fModifiers &= ~k_ePanelKeyStringModifiers_CaseMask;
					dialogVar.m_fModifiers |= k_ePanelKeyStringModifiers_Lowercase;
					break;
				case 'h': // html allowed
					dialogVar.m_fModifiers |= k_ePanelKeyStringModifiers_AllowHTML;
					break;
				}
			}
			break;

		case 't':
		case 'T':
			{
				dialogVar.m_eType = k_ePanelVartype_Time;
				while ( pchOpenBracketChar[1] == ':' && pchOpenBracketChar[2] && pchOpenBracketChar[3] == ':' ) // check for modifiers
				{
					pchOpenBracketChar+=2;

					switch( pchOpenBracketChar[0])
					{
					case 's': // short date
						dialogVar.m_fModifiers |= k_ePanelKeyTimeModifiers_ShortDate;
						break;
					case 'l': // long date
						dialogVar.m_fModifiers |= k_ePanelKeyTimeModifiers_LongDate;
						break;
					case 't': // short time
						dialogVar.m_fModifiers |= k_ePanelKeyTimeModifiers_ShortTime;
						break;
					case 'T': // long time (with sec)
						dialogVar.m_fModifiers |= k_ePanelKeyTimeModifiers_LongTime;
						break;
					case 'r': // relative time ( N days/hours ago )
						dialogVar.m_fModifiers |= k_ePanelKeyTimeModifiers_Relative;
						break;
					case 'd': // duration (e.g. 1 hour 20 minutes)
						dialogVar.m_fModifiers |= k_ePanelKeyTimeModifiers_Duration;
						break;
					case 'm': // minutes are the minimum precision to display (aka don't display seconds)
						dialogVar.m_fModifiers |= k_ePanelKeyTimeModifiers_Minutes;
						break;
					case 'e': // server real time
						dialogVar.m_fModifiers |= k_ePanelKeyTimeModifiers_ServerTime;
						break;
					}
				}
				if ( dialogVar.m_fModifiers == 0 )
				{
					dialogVar.m_fModifiers = k_ePanelKeyTimeModifiers_DateTime;
				}

			}
			break;
		case 'm':
		case 'M':
			dialogVar.m_eType = k_ePanelVartype_Money;
			break;
		case 'i':
		case 'I':
		case 'd':
		case 'D':
			dialogVar.m_eType = k_ePanelVartype_Number;
			while ( pchOpenBracketChar[ 1 ] == ':' && pchOpenBracketChar[ 2 ] && pchOpenBracketChar[ 3 ] == ':' ) // check for modifiers
			{
				pchOpenBracketChar += 2;

				switch ( pchOpenBracketChar[ 0 ] )
				{
					case 'r': // raw number
						dialogVar.m_fModifiers |= k_ePanelKeyNumberModifiers_RawNumber;
						break;
				}
			}
			break;
		case 'u':
		case 'U':
			dialogVar.m_eType = k_ePanelVartype_Uint64;
			break;
		case 'g':
		case 'G':
		case 'v':
		case 'V':
		{
			bool bVirtual = pchOpenBracketChar[0] == 'v' || pchOpenBracketChar[0] == 'V';

			dialogVar.m_eType = bVirtual ? k_ePanelVartype_Virtual : k_ePanelVartype_Generic;
			const char *pchEndBracket = strchr( pchOpenBracketChar, '}' );
			if ( !pchEndBracket )
			{
				/// when using the debugger you get paths with "{images}" and the like in them, so just ignore this case
				AssertMsg1( !bManagedLocString, "Badly formed dialog var, format is {g:type:name} '%s'", pchBaseString );
				return false;
			}
			else
			{
				pchOpenBracketChar += 2; // skip the "g:"
				const char *pchColon = strchr( pchOpenBracketChar, ':' );
				if ( !pchColon )
				{
					// generic requires params
					AssertMsg1( false, "Badly formed dialog var, format is {g:type:name} '%s'", pchBaseString );
					return false;
				}
				else
				{
					// Store off the key
					dialogVar.m_sVariableParams.SetDirect( pchOpenBracketChar, pchColon - pchOpenBracketChar );

					// See if there are any modifiers
					const char *pchFinalColon = pchColon;
					for ( const char *pch = pchColon + 1; pch != NULL && *pch != '\0' && pch < pchEndBracket; pch++ )
					{
						if ( *pch == ':' )
						{
							pchFinalColon = pch;
						}
					}

					CUtlString strModifiers;
					if ( pchFinalColon != pchColon )
					{
						strModifiers.SetDirect( pchColon + 1, pchFinalColon - ( pchColon + 1 ) );
					}
					dialogVar.m_fModifiers = ( ( CLocalization* )UIEngine()->UILocalize() )->ParseGenericDialogVariableModifiers( bVirtual, dialogVar.m_sVariableParams, strModifiers );

					// store off the key
					dialogVar.m_sVariableName.SetDirect( pchFinalColon + 1, pchEndBracket - ( pchFinalColon + 1 ) );
				}
				pchOpenBracketChar = pchEndBracket - 1;
			}
		}
			break;
		default:
			AssertMsg2( false, "Invalid dialog variable type for %s (%c)", pchBaseString, pchOpenBracketChar[0] );
			dialogVar.m_eType = k_ePanelVartype_None;
			break;
		}

		if ( dialogVar.m_eType == k_ePanelVartype_None )
			return false;


		if ( dialogVar.m_eType != k_ePanelVartype_Generic && dialogVar.m_eType != k_ePanelVartype_Virtual )
		{
			pchOpenBracketChar++; // skip the type specifier
			if ( pchOpenBracketChar[0] != ':' )
			{
				/// when using the debugger you get paths with "{images}" and the like in them, so just ignore this case
				AssertMsg1( !bManagedLocString, "Badly formed dialog var, format is {type:name} '%s'", pchBaseString );
				return false;
			}
		}

		const char *pchEndBracket = strchr( pchOpenBracketChar, '}' );
		if ( !pchEndBracket  )
		{
			AssertMsg1( false, "Invalid dialog variable name for '%s'", pchBaseString );
			return false;
		}

		if ( dialogVar.m_eType != k_ePanelVartype_Generic && dialogVar.m_eType != k_ePanelVartype_Virtual )
			dialogVar.m_sVariableName.SetDirect( pchOpenBracketChar + 1, pchEndBracket  - pchOpenBracketChar  - 1 );

		if ( dialogVarStart.m_sVariableName.Length() )
			vecDialogVars.AddToTail( dialogVarStart ); // the string piece before the var
		vecDialogVars.AddToTail( dialogVar ); // the var itself

		pchPrevStringEnd = pchEndBracket + 1;
		pchOpenBracketChar = FindNextBrace( pchOpenBracketChar );
	}

	if ( vecDialogVars.Count() && pchPrevStringEnd < pchBaseString + V_strlen(pchBaseString) )
	{
		DialogVariable_t dialogVarStart;
		dialogVarStart.m_eType = k_ePanelVartype_None;
		dialogVarStart.m_sVariableName.SetDirect( pchPrevStringEnd, pchBaseString + V_strlen(pchBaseString) - pchPrevStringEnd );
		vecDialogVars.AddToTail( dialogVarStart );

	}

	return vecDialogVars.Count() > 0;
}


//-----------------------------------------------------------------------------
// Purpose: sets a string and its var list
//-----------------------------------------------------------------------------
void CLocalizationStringDialogVariables::Set( char const **pStrData, CCopyableUtlVector<DialogVariable_t> &vecDialogVars )
{
	// NOTE: This class doesn't actually use m_pStrData at the moment and
	// it does not get updated after being set so it can point to invalid
	// memory.  In order to avoid false positives when looking dangling
	// pointers we just ignore the passed-in value.
	m_pStrData = nullptr;
	m_vecDialogVariables = vecDialogVars;
}


//-----------------------------------------------------------------------------
// Purpose: true if we have this key in our list 
//-----------------------------------------------------------------------------
bool CLocalizationStringDialogVariables::BContainsDialogVariable( const CPanelKeyValue &key )
{
	FOR_EACH_VEC( m_vecDialogVariables, i )
	{
		if ( key.m_symKey == m_vecDialogVariables[i].m_sVariableName )
		{
			return true;
		}
	}
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: verifies that the current string refers to a valid loc item.
//-----------------------------------------------------------------------------
bool CLocalizationStringDialogVariables::BHasValidString( const void *pValidBase, const void *pValidLimit )
{
	if ( m_pStrData == nullptr )
	{
		return true;
	}

	if ( m_pStrData < pValidBase || m_pStrData >= pValidLimit )
	{
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: rebuild a loc string due to variable changes
//-----------------------------------------------------------------------------
void CLocalizationStringDialogVariablesDerived::Recalculate( char const **pStrData, int nStartCharIndex /* = 0 */ )
{
	VPROF_BUDGET( "CLocalizationStringDialogVariablesDerived::Recalculate", VPROF_BUDGETGROUP_TENFOOT );
	
	if ( m_pOwner.Get() == NULL )
	{
		return;
	}

	const CUtlVector<DialogVariable_t> &vecDialogVars = m_pParent->GetDialogVariables();
	// use a utl vector for the char data so we can pre-alloc room for the string
	CUtlVector<char> vecChars( 0, 128 );
	// resolve all of our variables first
	FOR_EACH_VEC( vecDialogVars, i )
	{
		if ( vecDialogVars[i].m_eType == k_ePanelVartype_None )
		{
			CUtlString str = vecDialogVars[i].m_sVariableName;
			UnescapeString( str, 0 );
			vecChars.AddMultipleToTail( str.Length(), str.String() );
		}
		else
		{
			CUtlString result;
			if ( m_pLocalizationManager->ResolveDialogVariable(
					/*out*/ result, vecDialogVars[i].m_eType, m_eEscapeStyle,
					vecDialogVars[i].m_sVariableName, vecDialogVars[i].m_fModifiers, vecDialogVars[i].m_sVariableParams,
					m_pOwner.Get() ) )
				vecChars.AddMultipleToTail( result.Length(), result.Get() );
			else
				vecChars.AddMultipleToTail( vecDialogVars[i].m_sVariableName.Length(), vecDialogVars[i].m_sVariableName.String() );
		}
	}
	vecChars.AddToTail('\0');

	if ( m_eTransformStyle != k_eStringTransformStyle_None )
	{
		int nConvFlags = m_eTransformStyle == k_eStringTransformStyle_Uppercase ? STRINGCASE_UPPER : STRINGCASE_LOWER;
		m_sDerivedString = vecChars.Base();
		m_sDerivedString.UnicodeCaseConvert( nConvFlags | STRINGCASE_FLAG_LINGUISTIC );
	}
	else
	{
		m_sDerivedString = vecChars.Base();
	}

	// Don't currently support recalculating only part of the string
	Assert( nStartCharIndex == 0 );

	DispatchEvent( LocalizationChanged(), m_pOwner.Get(), this, 0 );
}


//-----------------------------------------------------------------------------
// Purpose: verifies that the current string refers to a valid loc item.
//-----------------------------------------------------------------------------
bool CLocalizationStringDialogVariablesDerived::BHasValidString( const void *pValidBase, const void *pValidLimit )
{
	return m_pParent->BHasValidString( pValidBase, pValidLimit );
}


//-----------------------------------------------------------------------------
// Purpose: update this simple string
//-----------------------------------------------------------------------------
void CLocalizationStringSimple::Create( const char *pString ) 
{ 
	DbgAssert( ThreadInMainThread() );
	m_DerivedString = pString;
	Recalculate( (char const **)&m_DerivedString );
}


//-----------------------------------------------------------------------------
// Purpose: update this simple string truncating if needed
//			The string data is utf-8 and we truncate on codepoint boundaries, not
//			just on byte boundaries, hence the extra logic here.
//-----------------------------------------------------------------------------
void CLocalizationStringSimple::Recalculate( char const **pStrData, int nStartCharIndex /* = 0 */ )
{
	VPROF_BUDGET( "CLocalizationStringSimple::Recalculate", VPROF_BUDGETGROUP_TENFOOT );

	int nStrDataLen = 0;
	if ( *pStrData )
	{
		nStrDataLen = V_strlen( *pStrData );
	}

	if ( nStartCharIndex < 0 || nStartCharIndex > nStrDataLen )
	{
		// Bogus start character - force a full recalc
		Assert( false );
		nStartCharIndex = 0;
	}

	DbgAssert( ThreadInMainThread() );
	m_pStrData = pStrData;
	// Simple loc strings always use their derived string storage when
	// created.  In theory we could keep a pointer into the loc system's
	// string data on Recalculate but that led to unexplained crashes
	// so just ensure that the derived string is used here also.
	SwitchToContainedString();

	if ( !m_bStringAlreadyFullyParsed )
	{
		if ( !IsEmpty() )
		{
			const char *pchChangedStart = *m_pStrData + nStartCharIndex;
			if ( V_strchr( pchChangedStart, '\\' ) )
			{
				// String potentially contains escape characters, strip them out.
				SwitchToContainedString();
				UnescapeString( m_DerivedString, nStartCharIndex );
			}
		}

		if ( !IsEmpty() && m_eTruncationStyle != k_eStringTruncationStyle_None ) // if we need to truncate
		{
			SwitchToContainedString();

			size_t cchUnicodeChars = V_UnicodeLength( m_DerivedString.Get() );
			if ( cchUnicodeChars > m_nMaxChars && m_nMaxChars != k_nLocalizeMaxChars ) // if the string is longer than we wanted
			{
				char *pchStr = m_DerivedString.Access(); // this is the complete string and is too long, we need to cut it down to size

				if ( m_eTruncationStyle == k_eStringTruncationStyle_Rear )
				{
					V_UnicodeTruncate( pchStr, m_nMaxChars );
				}
				else
				{
					Assert( m_eTruncationStyle == k_eStringTruncationStyle_Front );

					char *pchNewStart = V_UnicodeAdvance( pchStr, ( int )( cchUnicodeChars - m_nMaxChars ) );
					int nNewBytes = V_strlen( pchNewStart ) + 1;
					V_memmove( pchStr, pchNewStart, nNewBytes ); // copy all the string data back to the start, including null
				}
			}
		}

		if ( !IsEmpty() && m_eTransformStyle != k_eStringTransformStyle_None )
		{
			SwitchToContainedString();

			if ( m_eTransformStyle == k_eStringTransformStyle_Uppercase )
			{
				SAFE_DELETE( m_pStrDataNoTransform );
				m_pStrDataNoTransform = new CUtlString( m_DerivedString.String() );
				m_DerivedString.ToUpperLinguistic();
			}
			else if ( m_eTransformStyle == k_eStringTransformStyle_Lowercase )
			{
				SAFE_DELETE( m_pStrDataNoTransform );
				m_pStrDataNoTransform = new CUtlString( m_DerivedString.String() );
				m_DerivedString.ToLowerLinguistic();
			}
		}
	}

	if ( m_pOwner.Get() )
		DispatchEvent( LocalizationChanged(), m_pOwner.Get(), this, nStartCharIndex );
}


//-----------------------------------------------------------------------------
// Purpose: free this string
//-----------------------------------------------------------------------------
void CLocalizationStringDialogVariablesDerived::Release() const
{
	m_pLocalizationManager->Release( m_pOwner.Get(), (ILocalizationString *)this );
}


//-----------------------------------------------------------------------------
// Purpose: free this string
//-----------------------------------------------------------------------------
void CLocalizationStringSimple::Release() const
{
	DbgAssert( ThreadInMainThread() );
	m_pLocalizationManager->Release( m_pOwner.Get(), (ILocalizationString *)this );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CLocalizationStringSimple::AppendText( const char *pchText )
{
	DbgAssert( ThreadInMainThread() );
	if ( IsUsingContainedString() )
	{
		if ( m_pStrDataNoTransform )
		{
			m_DerivedString = *m_pStrDataNoTransform;
		}
	}
	else
	{
		// need to copy off this string and then modify it
		if ( !m_pStrDataNoTransform )
		{
			m_DerivedString = String();
		}
		else
		{
			m_DerivedString = *m_pStrDataNoTransform;
		}
	}

	int nAppendCharIndex = m_DerivedString.Length();
	m_DerivedString += pchText;

	Recalculate( (char const **)&m_DerivedString, nAppendCharIndex );

#if defined ( PANORAMA_USE_S1WRAPPER )
	// Recalculate can change string length, update the internal length var here
	m_DerivedString.SetLength( V_strlen( m_DerivedString.Get() ) );
#endif

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: verifies that the current string refers to a valid loc item.
//-----------------------------------------------------------------------------
bool CLocalizationStringSimple::BHasValidString( const void *pValidBase, const void *pValidLimit )
{
	if ( m_pStrData == nullptr || IsUsingContainedString() )
	{
		return true;
	}

	if ( m_pStrData < pValidBase || m_pStrData >= pValidLimit )
	{
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: return the longest string in any language for this token
//-----------------------------------------------------------------------------
void CLocalization::SetLongestStringForToken( const ILocalizationString *pLocalizationString, ILocalizationStringSizeResolver *pResolver )
{
	VPROF_BUDGET( "CLocalization::SetLongestStringForToken", VPROF_BUDGETGROUP_TENFOOT );

	Assert( pLocalizationString );
	Assert( pResolver );
	// work out the token for this string
	const char *pchToken = NULL;
	int iIssuedString = -1;
	FOR_EACH_MAP_FAST( m_mapIssuedStrings, i )
	{
		if ( m_mapIssuedStrings[i] == pLocalizationString )
		{
			pchToken = m_mapIssuedStrings.Key( i );
			iIssuedString = i;
			break;
		}
	}

	if ( pchToken && pchToken[0] != '\0' )
	{
		if ( pchToken[1] != '#' ) // ignore "##FFFFFFFF" color codes
		{
			int iIndex = m_mapLocalizationStrings.Find( pchToken );
			if ( iIndex != m_mapLocalizationStrings.InvalidIndex() )
			{
				int nMaxLocLength = 0;
				int iMaxLocIndex = -1;

				for ( int locIndex = m_mapLocalizationStrings[iIndex]; locIndex != -1; locIndex = m_vecLocEntrys[locIndex].m_nNext )
				{
					auto *locStr = &m_vecLocEntrys[locIndex];
					if ( V_stricmp( locStr->m_pStrToken, pchToken ) == 0 )
					{
						const char *pchString = locStr->m_pString;
						int nLocLen = pResolver->ResolveStringLengthInPixels( pchString );
						if ( nLocLen > nMaxLocLength )
						{
							nMaxLocLength = nLocLen;
							iMaxLocIndex = locIndex;
						}
					}
				}

				if ( !m_mapIssuedStrings.IsValidIndex( iIssuedString ) )
					return; // we probably deleted the loc string during the ResolveStringLengthInPixels call above, quite often due to styles applying and we did a transform on our loc string

				if ( iMaxLocIndex >= 0 )
				{
					auto *locStr = &m_vecLocEntrys[iMaxLocIndex];

					CLocalizationStringSimple *pLocStringSimple = dynamic_cast<CLocalizationStringSimple *>( m_mapIssuedStrings[iIssuedString] );
					if ( pLocStringSimple )
					{
						pLocStringSimple->Create( locStr->m_pString );
					}
					else
					{
						CLocalizationStringDialogVariablesDerived *pLocStringDialogVars = dynamic_cast<CLocalizationStringDialogVariablesDerived *>(m_mapIssuedStrings[iIssuedString]);
						Assert( pLocStringDialogVars ); // if it wasn't a simple loc string then it better be a dialog var
						if ( pLocStringDialogVars )
						{
							CCopyableUtlVector<DialogVariable_t> vecDialogVars;
							bool bHasVariables = CLocalizationStringDialogVariables::BParseDialogVariables( locStr->m_pString, vecDialogVars, true );
							Assert( bHasVariables );
							if ( !bHasVariables )
								AssertMsg1( false, "All variables removed from string %s, not supported", locStr->m_pString );

							int iDialogVar = m_mapLocStringDialogVariables.Find( pchToken );
							Assert( iDialogVar != m_mapLocStringDialogVariables.InvalidIndex() );
							if ( iDialogVar != m_mapLocStringDialogVariables.InvalidIndex() )
								m_mapLocStringDialogVariables[iDialogVar]->Set( &locStr->m_pString, vecDialogVars );
						}
					}
				}
			}
		}
	}
}


#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: validate mem
//-----------------------------------------------------------------------------
void CLocalization::Validate( CValidator &validator, const tchar *pchName )
{
	VALIDATE_SCOPE();

	ValidateObj( m_allStringData );
	ValidateObj( m_mapLocalizationStrings );
	ValidateObj( m_vecLocEntrys );

	ValidateObj( m_sLocalizationFilePath );

	ValidateObj( m_mapIssuedStrings );
	FOR_EACH_MAP_FAST( m_mapIssuedStrings, i )
	{
		ValidateObj( m_mapIssuedStrings.Key(i) );
		ValidatePtr( m_mapIssuedStrings[i] );
	}

	ValidateObj( m_mapNonLocalizedStrings );
	FOR_EACH_MAP_FAST( m_mapNonLocalizedStrings, i )
	{
		ValidatePtr( m_mapNonLocalizedStrings[i] );
		if ( m_mapNonLocalizedStrings[i]->HasNonContainedString() )
			ValidatePtr( *m_mapNonLocalizedStrings[i]->m_pStrData );
	}

	ValidateObj( m_mapPanelVariables );
	FOR_EACH_MAP_FAST( m_mapPanelVariables, i )
	{
		ValidateObj( m_mapPanelVariables[i] );
		FOR_EACH_VEC( m_mapPanelVariables[i], j )
			ValidateObj( m_mapPanelVariables[i][j] );
	}

	ValidateObj( m_mapLocStringsOwnedByPanel );
	FOR_EACH_MAP_FAST( m_mapLocStringsOwnedByPanel, i )
	{
		ValidateObj( m_mapLocStringsOwnedByPanel[i] );
		FOR_EACH_VEC( m_mapLocStringsOwnedByPanel[i], k )
		{
			ValidateObj( m_mapLocStringsOwnedByPanel[i][k]->m_symToken );
		}
	}
	
	ValidateObj( m_mapLocStringDialogVariables );
	FOR_EACH_MAP_FAST( m_mapLocStringDialogVariables, i )
	{
		ValidateObj( m_mapLocStringDialogVariables.Key(i) );
		ValidatePtr( m_mapLocStringDialogVariables[i] );
	}

	ValidateObj( m_hashLanguagesCheckedForDupes );
}

#endif
