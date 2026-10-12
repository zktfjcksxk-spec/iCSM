#ifndef INCLUDED_WRAP_OTHER_H
#define INCLUDED_WRAP_OTHER_H
//================ Copyright (c) Valve Corporation. All Rights Reserved. ===========================
//
//
//
//==================================================================================================

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Conflicts with some protobuf extension stuff we use
#undef GetMessage
#endif

#include "tier0/platform.h"
#include "tier0/dbg.h"

#include "vstdlib/vstrtools.h"

#ifdef OSX
#include <mach-o/dyld.h>
#endif

#define VPROF_BUDGET_DETAILED(x,y)

#define Shipping_AssertMsg( _exp, _msg, ... ) DevAssertMsg( _exp, (const tchar *)(CDbgFmtMsg( _T( _msg ), ##__VA_ARGS__ )) )
#define Development_AssertMsg( _exp, _msg, ... ) DevAssertMsg( _exp, (const tchar *)(CDbgFmtMsg( _T( _msg ), ##__VA_ARGS__ )) )

#ifndef MemAlloc_Free
#define MemAlloc_Free(p) g_pMemAlloc->Free( p )
#endif

#define IsWindows() IsPlatformWindowsPC()

//--------------------------------------------------------------------------------------------------
// Interfaces
//--------------------------------------------------------------------------------------------------

#define PANORAMAUI_ENGINE_INTERFACE_VERSION "PanoramaUIEngine001"
#define PANORAMAUI_CLIENT_INTERFACE_VERSION "PanoramaUIClient001"
#define PANORAMA_TEXT_SERVICES_INTERFACE_VERSION "PanoramaTextServices001"
#define IMEMANAGER_INTERFACE_VERSION "IMEManager001"
#define RESOURCESYSTEM_PROTECTED_INTERFACE_VERSION "Blah"

//--------------------------------------------------------------------------------------------------
// Stub for g_pApplication
//--------------------------------------------------------------------------------------------------

enum LanguageType_t
{
	LanguageType_UI,
	LanguageType_Audio,
};


class CApplication
{
public:
	inline bool IsFileInAddon( char* pszFile )
	{
		return false;
	}
	inline bool IsInToolsMode()
	{
		return false;
	}
	virtual const char *GetLanguage( LanguageType_t languageType ) const;
};

extern CApplication gApplication;
extern CApplication *g_pApplication;

//--------------------------------------------------------------------------------------------------
// Sound
//--------------------------------------------------------------------------------------------------

extern class IEngineSound *g_pEnginesound;
extern class ISoundEmitterSystemBase *g_pSoundEmitterSystemBase;
extern class IVEngineClient *g_pEngineclient;

//--------------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------------


inline bool Plat_IsInTestMode()
{
	return false;
}


#ifndef Log_Detailed
#define Log_Detailed( Channel, /* [LoggingMetaData_t *], [Color], Message, */ ... ) //InternalMsg( Channel, LS_DETAILED, /* [LoggingMetaData_t *], [Color], Message, */ ##__VA_ARGS__ )
#endif

#ifndef DbgAssertMsg
#define DbgAssertMsg( ... )
#endif


enum LoggingVerbosity_t
{
	//-----------------------------------------------------------------------------
	// No data at all
	//-----------------------------------------------------------------------------
	LV_OFF = 0,

	//-----------------------------------------------------------------------------
	// Absolutely essential data, errors and assertions
	//-----------------------------------------------------------------------------
	LV_ESSENTIAL = 1,

	//-----------------------------------------------------------------------------
	// Include warnings and messages that are concise
	//-----------------------------------------------------------------------------
	LV_DEFAULT = 2,

	//-----------------------------------------------------------------------------
	// Include verbose messages, walls of text are acceptable
	//-----------------------------------------------------------------------------
	LV_DETAILED = 3,

	//-----------------------------------------------------------------------------
	// Everything. If it might be relevant 0.01% of the time, send it
	//-----------------------------------------------------------------------------
	LV_MAXIMUM = 4,
};



template <class T>
inline bool IsPowerOf2( T n )
{
	return n > 0 && ( n & ( n - 1 ) ) == 0;
}

template <class T1, class T2>
inline T2 ModPowerOf2( T1 a, T2 b )
{
	return T2( a ) & ( b - 1 );
}

template <class T>
inline T RoundDownToMultipleOf( T n, T m )
{
	return n - ( IsPowerOf2( m ) ? ModPowerOf2( n, m ) : ( n%m ) );
}

template <class T>
inline T RoundUpToMultipleOf( T n, T m )
{
	if ( !n )
	{
		return m;
	}
	else
	{
		return RoundDownToMultipleOf( n + m - 1, m );
	}
}

#if defined( PLATFORM_WINDOWS )
FORCEINLINE char *GetLocalHostname()
{
	static char s_rgchComputerName[ 256 ];
	static CThreadMutex s_Mutex;
	if ( s_rgchComputerName[ 0 ] == '\0' )
	{
		s_Mutex.Lock();
		if ( s_rgchComputerName[ 0 ] == '\0' )
		{
			BOOL bSucc;
			WCHAR rgwchComputerName[ 64 ];

			DWORD cchComputerName = Q_ARRAYSIZE( rgwchComputerName );
			bSucc = GetComputerNameExW( ComputerNamePhysicalDnsHostname,
										rgwchComputerName,
										&cchComputerName );
			if ( bSucc )
			{
				bSucc = WideCharToMultiByte( CP_UTF8, 0,
											 rgwchComputerName, -1,
											 s_rgchComputerName, sizeof( s_rgchComputerName ),
											 NULL, NULL ) != 0;
			}

			if ( !bSucc )
			{
				// Prevent cycling when it's unlikely a future call
				// will succeed, plus expose failure in a somewhat visible way.
				strcpy( s_rgchComputerName, "<unable to get local hostname>" );
			}
		}
		s_Mutex.Unlock();
	}
	return s_rgchComputerName;
}


FORCEINLINE bool Plat_GetExecutablePath( char *pBuff, size_t nBuff )
{
	return ::GetModuleFileNameA( NULL, pBuff, (DWORD)nBuff ) > 0;
}


#elif defined( PLATFORM_POSIX )
FORCEINLINE const char *GetLocalHostname()
{
	static char rgchHostname[ 256 ];
	if ( rgchHostname[ 0 ] == '\0' )
	{
		int ret = gethostname( rgchHostname, Q_ARRAYSIZE( rgchHostname ) );
		if ( ret )
			strcpy( rgchHostname, "unknown" );
		// if we get back a FQDN, truncate it to hostname
		if ( char *pchDot = strchr( rgchHostname, '.' ) )
		{
			*pchDot = '\0';
		}

	}
	return rgchHostname;
}

FORCEINLINE bool Plat_GetExecutablePath( char *pBuff, size_t nBuff )
{
#if defined OSX
	uint32_t _nBuff = nBuff;
	bool bSuccess = _NSGetExecutablePath( pBuff, &_nBuff ) == 0;
	pBuff[nBuff - 1] = '\0';
	return bSuccess;
#elif defined LINUX
	ssize_t nRead = readlink( "/proc/self/exe", pBuff, nBuff - 1 );
	if ( nRead != -1 )
	{
		pBuff[nRead] = 0;
		return true;
	}

	pBuff[0] = 0;
	return false;
#else
	AssertMsg( false, "Implement Plat_GetExecutablePath" );
	return false;
#endif
}
#else
const char *GetLocalHostname()
{
	AssertMsg( false, "GetLocalHostname not implemented on current platform" );
	return "";
}
#endif

inline bool ThreadIsProcessIdActive( uint dwProcessId )
{
#ifdef _WIN32
	bool bActive = false;
	HANDLE hProcess = ::OpenProcess( SYNCHRONIZE, false, dwProcessId );

	if ( hProcess != NULL )
	{
		// if we get a timeout, that means the app is still running
		bActive = WAIT_TIMEOUT == ::WaitForSingleObject( hProcess, 0 );
		::CloseHandle( hProcess );
	}

	return bActive;
#elif defined( _PS3 )
	AssertFatalMsg( 0, "Need to implement!" );
	return true;
#else
	if ( dwProcessId == 0 )
		return false;

	int ret = kill( dwProcessId, 0 );
	return ( ret >= 0 || ( ret < 0 && errno != ESRCH ) );
#endif
}

inline uint ThreadShellExecute( const char *lpApplicationName, const char *pchCommandLine, const char *pchCurrentDirectory )
{
	return 0;
}

inline int V_iswspace32( uchar32 c )
{
#ifdef WIN32
	// Win32 CRT doesn't support the full range of UChar32, has no extended planes.
	// We assume that there are no space characters above 64K.
	// As of 03/02/2016 this assumption is accurate according to unicode.org.
	if ( c > 0xffff )
	{
		// This will return false for EOF (-1) also, which is correct.
		return false;
	}

	return iswspace( (wint_t)c );
#else
	return iswspace( (wchar_t)c );
#endif
}

inline int V_iswcntrl32( uchar32 c )
{
#ifdef WIN32
	// Win32 CRT doesn't support the full range of UChar32, has no extended planes.
	// We assume that there are no control characters above 64K.
	// As of 03/02/2016 this assumption is accurate according to unicode.org.
	if ( c > 0xffff )
	{
		// This will return false for EOF (-1) also, which is correct.
		return false;
	}

	return iswcntrl( (wint_t)c );
#else
	return iswcntrl( (wchar_t)c );
#endif
}

inline int V_iswdigit32( uchar32 c )
{
#ifdef WIN32
	// Win32 CRT doesn't support the full range of UChar32, has no extended planes.
	if ( c > 0xffff )
	{
		// There ARE digit characters above 64K.
		// Ranges checked were accurate As of 03/02/2016 according to unicode.org.
		if ( ( c >= 0x104a0 && c <= 0x104a9 ) ||
			 ( c >= 0x11066 && c <= 0x1106f ) ||
			 ( c >= 0x110f0 && c <= 0x110f9 ) ||
			 ( c >= 0x11136 && c <= 0x1113f ) ||
			 ( c >= 0x111d0 && c <= 0x111d9 ) ||
			 ( c >= 0x112f0 && c <= 0x112f9 ) ||
			 ( c >= 0x114d0 && c <= 0x114d9 ) ||
			 ( c >= 0x11650 && c <= 0x11659 ) ||
			 ( c >= 0x116c0 && c <= 0x116c9 ) ||
			 ( c >= 0x11730 && c <= 0x11739 ) ||
			 ( c >= 0x118e0 && c <= 0x118e9 ) ||
			 ( c >= 0x16a60 && c <= 0x16a69 ) ||
			 ( c >= 0x16b50 && c <= 0x16b59 ) ||
			 ( c >= 0x1d7ce && c <= 0x1d7ff ) )
		{
			return true;
		}

		// This will return false for EOF (-1) also, which is correct.
		return false;
	}

	return iswdigit( (wint_t)c );
#else
	return iswdigit( (wchar_t)c );
#endif
}


inline int V_isbreakablewspace32( uchar32 ch )
{
	if ( IsX360() )
	{
		// 0x00a0 and 0x202f are the wide and narrow non-breaking space UTF-16 values, respectively
		return ch <= 0xffff && ch != 0x00a0 && ch != 0x202f && iswspace( (wchar_t)ch );
	}
	else
	{
		return V_iswspace32( ch );
	}
}

inline const uchar32 *V_u32strchr( const uchar32 *pch32, uchar32 ch32 )
{
	while ( *pch32 )
	{
		if ( *pch32 == ch32 )
		{
			return pch32;
		}

		pch32++;
	}

	return NULL;
}

inline uchar32 *V_u32strchr( uchar32 *pch32, uchar32 ch32 )
{
	return const_cast<uchar32*>( V_u32strchr( const_cast<const uchar32*>( pch32 ), ch32 ) );
}


#endif // INCLUDED_WRAP_OTHER_H