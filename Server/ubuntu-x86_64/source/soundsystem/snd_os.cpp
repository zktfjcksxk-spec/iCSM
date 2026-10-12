//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: DLL interface for low-level sound utilities
//
//===========================================================================//

#ifdef WIN32
#include <Windows.h>
#endif

#ifdef LINUX
#include <sys/types.h>
#include <sys/wait.h>
#endif

#ifdef OSX
bool PlaySound( const char *pszFullpath, void *unused1, int unused2 );
#endif

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

void Plat_PlaySound( const char* filename )
{
#if defined(WIN32) || defined(OSX)
	::PlaySound( filename, nullptr, SND_FILENAME | SND_ASYNC );
#elif defined( LINUX )
	// On Linux, we fork a process and then execute 'aplay' to play a sound.
	// We fork twice so that we can execute aplay as a daemonized process
	// and don't have to wait() for it later.
	int nPid = fork();
	if ( nPid == 0 )
	{
		nPid = fork();
		if ( nPid == 0 )
		{
			execlp( "aplay", "aplay", filename, NULL );
		}

		_exit( 0 );
	}
	else if ( nPid != 0 )
	{
		int status = 0;
		waitpid( nPid, &status, 0 );
	}
#endif
}
