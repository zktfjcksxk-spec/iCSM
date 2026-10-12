//================ Copyright (c) Valve Corporation. All Rights Reserved. ===========================
//
//
//
//==================================================================================================

#include "s1wrapper.h"
// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


//--------------------------------------------------------------------------------------------------
// tier0
//--------------------------------------------------------------------------------------------------

#ifdef _WIN32

#if !defined NTSTATUS
typedef LONG NTSTATUS;
#endif

#if !defined PROCESSINFOCLASS
typedef LONG PROCESSINFOCLASS;
#endif

#if !defined PPEB
typedef struct _PEB *PPEB;
#endif

#if !defined PROCESS_BASIC_INFORMATION
typedef struct _PROCESS_BASIC_INFORMATION {
	PVOID Reserved1;
	PPEB PebBaseAddress;
	PVOID Reserved2[ 2 ];
	ULONG_PTR UniqueProcessId;
	PVOID Reserved3;
} PROCESS_BASIC_INFORMATION;
#endif

typedef NTSTATUS( WINAPI * LPFN_ZwQueryInformationProcess )( HANDLE, PROCESSINFOCLASS, PVOID, ULONG, PULONG );

typedef DWORD( WINAPI *LPFN_GetProcessId )( HANDLE );

DWORD GetProcessIdWin2kSafe( HANDLE hProc )
{
	LPFN_GetProcessId pfnGetProcessId = NULL;
	HMODULE hModKernel32 = GetModuleHandleA( "kernel32.dll" );
	if ( hModKernel32 )
		pfnGetProcessId = (LPFN_GetProcessId)GetProcAddress( hModKernel32, "GetProcessId" );

	if ( pfnGetProcessId )
	{
		return pfnGetProcessId( hProc );
	}
	else
	{
		LPFN_ZwQueryInformationProcess pfnZwQueryInformationProcess = NULL;
		HMODULE hModNTDLL = GetModuleHandleA( "ntdll.dll" );
		if ( hModNTDLL )
			pfnZwQueryInformationProcess = (LPFN_ZwQueryInformationProcess)GetProcAddress( hModNTDLL, "ZwQueryInformationProcess" );

		if ( pfnZwQueryInformationProcess )
		{
			PROCESS_BASIC_INFORMATION pbi;
			ZeroMemory( &pbi, sizeof( PROCESS_BASIC_INFORMATION ) );

			if ( pfnZwQueryInformationProcess( hProc, 0, &pbi, sizeof( PROCESS_BASIC_INFORMATION ), NULL ) == 0 )
			{
				return (DWORD)pbi.UniqueProcessId;
			}
		}
	}
	return 0xFFFFFFFF;
}

#endif

int Plat_LoadModuleRaw( const char *pModule, PlatModule_t *phModule )
{
#ifdef PLATFORM_WINDOWS
	PlatModule_t hModule = ( PlatModule_t )::LoadLibrary( pModule );
	int nErr = GetLastError();
#elif defined( PLATFORM_POSIX )
	PlatModule_t hModule = (PlatModule_t)dlopen( pModule, RTLD_LAZY | RTLD_GLOBAL );

	int nErr = errno;
#endif

	if ( hModule != PLAT_MODULE_INVALID )
	{
		nErr = 0;
	}
	else
	{
#ifdef PLATFORM_POSIX
		const char *pError = dlerror();

		if ( pError )

		{

			Msg( " failed to dlopen %s error=%s\n", pModule, pError );

			fprintf( stderr, " failed to dlopen \"%s\" error=%s\n", pModule, pError );

		}
#endif
	}
	*phModule = hModule;
	return nErr;
}

//#ifdef _WIN32
//typedef BOOL( STDCALL *pShellExecuteExW )( SHELLEXECUTEINFOW * );
//static pShellExecuteExW g_pShellExecuteExW;
//static ThreadInitOnce_t g_ShellExecuteExW_Once;
//static bool ShellExecuteExW_Once( ThreadInitOnce_t *pOnce )
//{
//	PlatModule_t hShell32;
//	if ( Plat_LoadModuleRaw( "shell32.dll", &hShell32 ) == 0 )
//	{
//		g_pShellExecuteExW = (pShellExecuteExW)Plat_GetModuleProcAddress( hShell32, "ShellExecuteExW" );
//	}
//	return true;
//}
//#endif
//
//uint ThreadShellExecuteEx( const char *pchVerb, // operation
//						   const char *pchFile, // file or executable name with path
//						   const char *pchParameters, // command line options for executables
//						   const char *pchCurrentDirectory ) // start directory
//{
//	uint dwProcID = 0;
//
//#ifdef _WIN32
//	ThreadInitOnceCall( &g_ShellExecuteExW_Once, ShellExecuteExW_Once );
//	if ( !g_pShellExecuteExW )
//	{
//		return 0;
//	}
//
//	SHELLEXECUTEINFOW shInfo = { 0 };
//	shInfo.cbSize = sizeof( SHELLEXECUTEINFO );
//	shInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
//	shInfo.hwnd = NULL;
//
//	if ( pchVerb )
//	{
//		size_t nNumChars = strlen( pchVerb ) + 1;
//		shInfo.lpVerb = (wchar_t*)malloc( nNumChars*sizeof( wchar_t ) );
//		MultiByteToWideChar( CP_UTF8, 0, pchVerb, -1, (wchar_t*)shInfo.lpVerb, (int)nNumChars );
//	}
//
//	if ( pchFile )
//	{
//		size_t nNumChars = strlen( pchFile ) + 1;
//		shInfo.lpFile = (wchar_t*)malloc( nNumChars*sizeof( wchar_t ) );
//		MultiByteToWideChar( CP_UTF8, 0, pchFile, -1, (wchar_t*)shInfo.lpFile, (int)nNumChars );
//	}
//
//	if ( pchParameters )
//	{
//		size_t nNumChars = strlen( pchParameters ) + 1;
//		shInfo.lpParameters = (wchar_t*)malloc( nNumChars*sizeof( wchar_t ) );
//		MultiByteToWideChar( CP_UTF8, 0, pchParameters, -1, (wchar_t*)shInfo.lpParameters, (int)nNumChars );
//	}
//
//	if ( pchCurrentDirectory )
//	{
//		size_t nNumChars = strlen( pchCurrentDirectory ) + 1;
//		shInfo.lpDirectory = (wchar_t*)malloc( nNumChars*sizeof( wchar_t ) );
//		MultiByteToWideChar( CP_UTF8, 0, pchCurrentDirectory, -1, (wchar_t*)shInfo.lpDirectory, (int)nNumChars );
//	}
//
//	shInfo.nShow = SW_SHOWDEFAULT;
//	shInfo.hInstApp = NULL;
//	shInfo.hProcess = NULL;
//
//	if ( g_pShellExecuteExW( &shInfo ) )
//	{
//		// ShellExecuteEx can be successful without a new process being started 
//		if ( shInfo.hProcess )
//		{
//			// add process to list of currently running steam games
//			dwProcID = GetProcessIdWin2kSafe( shInfo.hProcess );
//			::CloseHandle( shInfo.hProcess );
//		}
//	}
//
//	free( (wchar_t*)shInfo.lpVerb );
//	free( (wchar_t*)shInfo.lpFile );
//	free( (wchar_t*)shInfo.lpParameters );
//	free( (wchar_t*)shInfo.lpDirectory );
//#else
//	bool bIsOpen = false;
//
//	if ( pchVerb )
//	{
//		if ( strcmp( pchVerb, "open" ) == 0 )
//		{
//			if ( pchParameters )
//			{
//				// We only support a single filename in pchFile for opens.
//				return 0;
//			}
//
//			bIsOpen = true;
//		}
//		else
//		{
//			// We don't support any other verbs.
//			return 0;
//		}
//	}
//
//	// make sure the working directory exists
//	if ( pchCurrentDirectory )
//	{
//#ifdef LINUX
//		struct stat64 buf;
//		int ret = stat64( pchCurrentDirectory, &buf );
//#else
//		struct stat buf;
//		int ret = stat( pchCurrentDirectory, &buf );
//#endif
//		if ( ret < 0 )
//			return 0;
//
//		if ( !( buf.st_mode & S_IFDIR ) )
//			return 0;
//	}
//
//	static bool bInstalledSignalHandler = false;
//	if ( !bInstalledSignalHandler )
//	{
//		struct sigaction sa;
//		sa.sa_flags = SA_NOCLDSTOP;
//		sa.sa_handler = ReapChildProcesses;
//		sigaction( SIGCHLD, &sa, NULL );
//		bInstalledSignalHandler = true;
//	}
//
//	pid_t pid = fork();
//	if ( pid < 0 )
//	{
//		// fork failed
//		return 0;
//	}
//	else if ( pid == 0 )
//	{
//		// we're the child process - if we need to exit, use _exit, 
//		// so we don't wander into any of the process tear-down code
//		if ( pchCurrentDirectory )
//		{
//			int ret = chdir( pchCurrentDirectory );
//			if ( ret < 0 )
//				_exit( -1 );
//		}
//
//		char szFullCmdLine[ 2048 ];
//		if ( bIsOpen )
//		{
//#ifdef LINUX
//			// We might be built against the Steam runtime and running in it,
//			// but xdg-open is going to come from the system itself and may
//			// have conflicts with the runtime environment.
//			// Pop out of the runtime when executing xdg-open.  This will be
//			// an empty string in a non-runtime build.
//			const char *pchCommand = ESCAPE_STEAM_RUNTIME "/usr/bin/xdg-open";
//#else
//			const char *pchCommand = "open";
//#endif
//			snprintf( szFullCmdLine, sizeof( szFullCmdLine ), "%s '%s'", pchCommand, pchFile );
//		}
//		else
//		{
//			snprintf( szFullCmdLine, sizeof( szFullCmdLine ), "'%s' %s", pchFile, pchParameters );
//		}
//		_exit( system( szFullCmdLine ) );
//	}
//	else
//	{
//		// parent
//		dwProcID = pid;
//	}
//#endif
//
//	return dwProcID;
//}


//--------------------------------------------------------------------------------------------------
// Application
//--------------------------------------------------------------------------------------------------

static char s_lang[64] = "english";

const char *CApplication::GetLanguage( LanguageType_t languageType ) const
{
	ConVarRef language( "cl_language" );
	if ( language.IsValid() )
		V_strncpy( s_lang, language.GetString(), sizeof( s_lang ) );
	return s_lang;
}

CApplication gApplication;
CApplication *g_pApplication = &gApplication;

class IVEngineClient *g_pEngineclient = NULL;

//--------------------------------------------------------------------------------------------------
// Logging
//--------------------------------------------------------------------------------------------------

DEFINE_LOGGING_CHANNEL_NO_TAGS( LOG_PANORAMA, "Panorama", LCF_CONSOLE_ONLY, LS_MESSAGE );
DEFINE_LOGGING_CHANNEL_NO_TAGS( LOG_PANORAMA_SCRIPT, "PanoramaScript", LCF_CONSOLE_ONLY, LS_MESSAGE );
