//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
//-----------------------------------------------------------------------------
// This is just a little redirection tool so I can get all the dlls in bin
//-----------------------------------------------------------------------------

#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#include <assert.h>
#include <direct.h>
#elif POSIX
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>
#define MAX_PATH PATH_MAX
#endif

#include "tier0/basetypes.h"

#ifdef _WIN32
typedef int (*DedicatedMain_t)( HINSTANCE hInstance, HINSTANCE hPrevInstance, 
							  LPSTR lpCmdLine, int nCmdShow );
#elif POSIX
typedef int (*DedicatedMain_t)( int argc, char *argv[] );

#endif

//-----------------------------------------------------------------------------
// Purpose: Return the directory where this .exe is running from
// Output : char
//-----------------------------------------------------------------------------

static char *GetBaseDir( const char *pszBuffer )
{
	static char	basedir[ MAX_PATH ];
	char szBuffer[ MAX_PATH ];
	size_t j;
	char *pBuffer = NULL;

	strcpy( szBuffer, pszBuffer );

	pBuffer = strrchr( szBuffer,'\\' );
	if ( pBuffer )
	{
		*(pBuffer+1) = '\0';
	}

	strcpy( basedir, szBuffer );

	j = strlen( basedir );
	if (j > 0)
	{
		if ( ( basedir[ j-1 ] == '\\' ) || 
			 ( basedir[ j-1 ] == '/' ) )
		{
			basedir[ j-1 ] = 0;
		}
	}

	return basedir;
}

#ifdef _WIN32
int APIENTRY WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow )
{
	// Must add 'bin' to the path....
	char* pPath = getenv("PATH");

	// Use the .EXE name to determine the root directory
	char moduleName[ MAX_PATH ];
	char szBuffer[ 4096 ];
	if ( !GetModuleFileName( hInstance, moduleName, MAX_PATH ) )
	{
		MessageBox( 0, "Failed calling GetModuleFileName", "Launcher Error", MB_OK );
		return 0;
	}

	// Get the root directory the .exe is in
	char* pRootDir = GetBaseDir( moduleName );

#ifdef _DEBUG
	int len = 
#endif
	_snprintf( szBuffer, sizeof( szBuffer ) - 1, "PATH=%s\\bin\\;%s", pRootDir, pPath );
	szBuffer[ sizeof(szBuffer) - 1 ] = 0;
	assert( len < 4096 );
	_putenv( szBuffer );

	HINSTANCE launcher = LoadLibrary( "bin\\dedicated" DLL_EXT_STRING ); // STEAM OK ... filesystem not mounted yet
	if (!launcher)
	{
		char *pszError;
		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&pszError, 0, NULL);

		char szBuf[1024];
		_snprintf(szBuf, sizeof( szBuf ) - 1, "Failed to load the launcher DLL:\n\n%s", pszError);
		szBuf[ sizeof(szBuf) - 1 ] = 0;
		MessageBox( 0, szBuf, "Launcher Error", MB_OK );

		LocalFree(pszError);
		return 0;
	}

	DedicatedMain_t main = (DedicatedMain_t)GetProcAddress( launcher, "DedicatedMain" );
	return main( hInstance, hPrevInstance, lpCmdLine, nCmdShow );
}

#elif defined(POSIX)
#define stringize(a) #a
#define dedicated_binary(a,b,c) a stringize(b) c 

int main( int argc, char *argv[] )
{
	// Must add 'bin' to the path....
	char* pPath = getenv("LD_LIBRARY_PATH");
	char szBuffer[4096];
	char cwd[ MAX_PATH ];
	if ( !getcwd( cwd, sizeof(cwd)) )
	{
		printf( "getcwd failed (%s)", strerror(errno));
	}
	
	snprintf( szBuffer, sizeof( szBuffer ) - 1, "LD_LIBRARY_PATH=%s/bin:%s", cwd, pPath ? pPath : "" );
	printf( "%s\n", szBuffer );
	int ret = putenv( szBuffer );
	if ( ret )	
	{
		printf( "%s\n", strerror(errno) );
	}

	#ifdef OSX
 char tier0Path[MAX_PATH];
 snprintf(tier0Path, sizeof(tier0Path), "%s/bin/libtier0" DLL_EXT_STRING, cwd);
 void *tier0 = dlopen(tier0Path, RTLD_NOW);
#else
 void *tier0 = dlopen( "libtier0" DLL_EXT_STRING, RTLD_NOW );
#endif
	if ( !tier0 )
	{
		printf( "Failed to open %s (%s)\n", "libtier0" DLL_EXT_STRING, dlerror());
		return -1;
	}

	#ifdef OSX
 char vstdlibPath[MAX_PATH];
 snprintf(vstdlibPath, sizeof(vstdlibPath), "%s/bin/libvstdlib" DLL_EXT_STRING, cwd);
 void *vstdlib = dlopen(vstdlibPath, RTLD_NOW);
#else
 void *vstdlib = dlopen( "libvstdlib" DLL_EXT_STRING, RTLD_NOW );
#endif
	if ( !vstdlib )
	{
		printf( "Failed to open %s (%s)\n", "libvstdlib" DLL_EXT_STRING, dlerror());
		return -1;
	}

	const char *pBinaryName = "dedicated" DLL_EXT_STRING;

	#ifdef OSX
 char dedicatedPath[MAX_PATH];
 snprintf(dedicatedPath, sizeof(dedicatedPath), "%s/bin/%s", cwd, pBinaryName);
 void *dedicated = dlopen(dedicatedPath, RTLD_NOW);
#else
 void *dedicated = dlopen( pBinaryName, RTLD_NOW );
#endif
	if ( !dedicated )
	{
		printf( "Failed to open %s (%s)\n", pBinaryName, dlerror());
		return -1;
	}
	DedicatedMain_t main = (DedicatedMain_t)dlsym( dedicated, "DedicatedMain" );
	if ( !main )
	{
		printf( "Failed to find dedicated server entry point (%s)\n", dlerror() );
		return -1;
	}
		
	ret = main( argc,argv );
	dlclose( dedicated );
	dlclose( vstdlib );
	dlclose( tier0 );
	return ret;
}
#endif
