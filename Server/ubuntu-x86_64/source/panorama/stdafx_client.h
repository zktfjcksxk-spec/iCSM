//====== Copyright 1996-2006, Valve Corporation, All rights reserved. =======
//
// Purpose: precompiled header for panorama project
//
//=============================================================================

#ifndef STDAFXCLIENT_H
#define STDAFXCLIENT_H
#ifdef _WIN32
#pragma once
#endif

// base
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef WIN32
#if defined( SOURCE2_PANORAMA )
		#include "tier0/basetypes.h"
	#else
		#include <minmax.h>
	#endif
#endif

// low level
#include "tier0/dbg.h"
#include "tier0/threadtools.h"
#if defined( SOURCE2_PANORAMA )
#include "tier1/strtools.h"
#else
#include "vstdlib/strtools.h"
#include "misc.h"
#endif
#include "vstdlib/random.h"
#include "filesystem.h"
#include "tier1/keyvalues.h"
#include "vstdlib/osversion.h"
#include "tier0/vprof.h"

// tier1
#include "tier1/convar.h"
#include "tier1/checksum_crc.h"
#include "tier1/fmtstr.h"
#include "tier1/fileio.h"
#include "tier1/utlbuffer.h"
#include "tier1/utlvector.h"
#include "tier1/utlmap.h"

//common
#include "language.h"
#include "refcount.h"

// mathlib
#include "mathlib/mathlib.h"
#include "mathlib/vmatrix.h"

// panorama public
#include "panorama/panorama.h"
#include "panorama/panoramasymbol.h"
#include "panorama/transformations.h"
#include "panorama/input/keycodes.h"
#include "panorama/layout/panel2dfactory.h"
#include "panorama/input/mousecursors.h"
#include "panorama/input/iuiinput.h"
#include "panorama/iuistylefactory.h"
#include "panorama/iuiengine.h"
#include "panorama/uievent.h"
#include "panorama/uievents.h"
#include "panorama/uitoplevelwindow.h"
#include "panorama/layout/panel2dfactory.h"
#include "panorama/controls/panelptr.h"
#include "panorama/controls/panelhandle.h"
#include "panorama/controls/panel2d.h"
#include "panorama/controls/tooltip.h"
#include "panorama/data/imageloader.h"
#include "panorama/data/panoramavideoplayer.h"

#include "resourcesystem/iresourcesystem.h"

#ifdef PANORAMA_USE_S1WRAPPER

#include "time.h"
#include "tier1/strtools.h"
#include "../gcsdk/steamextra/tier1/utlstringbuilder.h"

inline const char*	V_strchr( const char *s, char c )				{ return strchr( s, c ); }
inline char*	V_strchr( char *s, char c )				{ return strchr( s, c ); }
bool BGetLocalFormattedDateAndTime( time_t timeVal, char *pchDate, int cubDate, char *pchTime, int cubTime, bool bIncludeSeconds = false, bool bShortDateFormat = false );

 FORCEINLINE unsigned FASTCALL HashStringConventional( const char *pszKey )
{
	unsigned hash = 0xAAAAAAAA; // Alternating 1's and 0's to maximize the effect of the later multiply and add

	for( ; *pszKey ; pszKey++ )
	{
		hash = ( ( hash << 5 ) + hash ) + (uint8)(*pszKey);
	}

	return hash;
}

#ifdef WIN32
// Win32 CRT doesn't support the full range of UChar32, has no extended planes
inline int V_iswspace( int c ) { return ( c <= 0xFFFF ) ? iswspace( (wint_t)c ) : 0; }
#else
#define V_iswspace(x) iswspace(x)
#endif

FORCEINLINE int V_isbreakablewspace( wchar_t ch )
{
	return V_iswspace( ch );
}

#if defined( POSIX ) && !defined( _PS3 )

// Linux doesn't have this function so this emulates its functionality

FORCEINLINE void *GetModuleHandle( const char *name )

{

	void *handle;



	if ( name == NULL )

	{

		// hmm, how can this be handled under linux....

		// is it even needed?

		return NULL;

	}



	if ( ( handle = dlopen( name, RTLD_NOW ) ) == NULL )

	{

		printf( "DLOPEN Error:%s\n", dlerror() );

		// couldn't open this file

		return NULL;

	}



	// read "man dlopen" for details

	// in short dlopen() inc a ref count

	// so dec the ref count by performing the close

	dlclose( handle );

	return handle;

}

#endif


FORCEINLINE void *Sys_GetProcAddress( HMODULE hModule, const char *pName )
{
#ifdef WIN32
	return (void *)GetProcAddress( hModule, pName );
#else
	return (void *)dlsym( (void *)hModule, pName );
#endif
}


FORCEINLINE static void *Sys_GetProcAddress( const char *pModuleName, const char *pName )
{
#if defined( _PS3 )
	Assert( !"Unsupported, use HMODULE" );
	return NULL;
#else // !_PS3
	HMODULE hModule = (HMODULE)GetModuleHandle( pModuleName );
#if defined( WIN32 )
	return (void *)GetProcAddress( hModule, pName );
#else // !WIN32
	return (void *)dlsym( (void *)hModule, pName );
#endif // WIN32
#endif // _PS3
}


FORCEINLINE void *Sys_GetProcAddress( CSysModule* pModule, const char *pszName )
{
	if ( !pModule )
	{
#if defined(_WIN32)
		return GetProcAddress( GetModuleHandle( NULL ), pszName );
#elif defined( _PS3 )
		AssertFatalMsg( 0, "Need to implement!" );
		return NULL;
#elif defined(POSIX)
		return dlsym( RTLD_DEFAULT, pszName );
#else
		return NULL;
#endif
	}
	else
	{
#if defined(_WIN32)
		return GetProcAddress( (HMODULE)pModule, pszName );
#elif defined( _PS3 )
		AssertFatalMsg( 0, "Need to implement!" );
		return NULL;
#elif defined(POSIX)
		return dlsym( pModule, pszName );
#else
		return NULL;
#endif
	}
}


#endif


#endif // STDAFXCLIENT_H
