//===== Copyright 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Real-Time Hierarchical Profiling
//
// $NoKeywords: $
//===========================================================================//

#if defined (RAD_TELEMETRY_3)

#include "pch_tier0.h"
#include "vprof_telemetry3.h"
#include "tier0/tslist.h"

#if defined( _WIN32 )
#include <windows.h>
#endif// _WIN32

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//--------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------
struct ThreadNameInfo_t
{
	TSLNodeBase_t base;
	ThreadId_t ThreadID;
	char szName[ 64 ];
};

CTSSimpleList< ThreadNameInfo_t > *ThreadNameList()
{
	static CTSSimpleList< ThreadNameInfo_t > threadNamesList;
	return &threadNamesList;
}

// Sort threads so that more interesting ones are displayed on top of the Visualizer pane.
// If an entry a[i] in the following array matches a thread name t, then t is displayed at 
// position i from top
char *g_ThreadSortOrder[] = 
{
	"MainThrd",
	"GlobPool0",
	"GlobPool1",
	"GlobPool2",
	"PanoramaTextureDecode0"
};

static bool g_bThreadNameArrayChanged = false;
static int g_ThreadNameArrayCount = 0;
static ThreadNameInfo_t g_ThreadNameArray[32];

static char *g_pTmMemoryArena = NULL;
const int TELEMETRY_ARENA_SIZE = 32 * 1024 * 1024; // How much memory we want Telemetry to use.

// Various stages of telemetry initialization
enum TelemetryStatus_t
{
	TM_NOTLOADED,
	TM_LOADED,
	TM_INSESSION
};

static TelemetryStatus_t g_TelemetryStatus = TM_NOTLOADED;

static unsigned int g_TelemetryFrameCount = 0;
static bool g_fTelemetryLevelChanged = false;

TelemetryData g_Telemetry;
tm_api *g_tm_api;

//--------------------------------------------------------------------------------------
// Helper class to shut down Telemetry on exit
//--------------------------------------------------------------------------------------
class CTelemetryRegister
{
public:
	CTelemetryRegister() 	{}	// Initialized when level first changed in VConsole
	~CTelemetryRegister() 	{ TelemetryShutdown( true ); }
} g_TelemetryRegister;

//--------------------------------------------------------------------------------------
// TelemetryThreadSetDebugName
//--------------------------------------------------------------------------------------
void TelemetryThreadSetDebugName( ThreadId_t id, const char *pszName )
{
	ThreadNameInfo_t *pThreadNameInfo = new ThreadNameInfo_t;

	if( id == ( uint32 )-1 )
	{
		id = ThreadGetCurrentId();
	}

	pThreadNameInfo->ThreadID = id;
	strncpy( pThreadNameInfo->szName, pszName, ARRAYSIZE( pThreadNameInfo->szName ) );
	pThreadNameInfo->szName[ ARRAYSIZE( pThreadNameInfo->szName ) - 1 ] = 0;
	ThreadNameList()->Push( pThreadNameInfo );

	g_bThreadNameArrayChanged = true;
}

//--------------------------------------------------------------------------------------
// TelemetryInitialize
//--------------------------------------------------------------------------------------
static bool TelemetryInitialize()
{
	tm_error retVal;

	if( g_TelemetryStatus != TM_NOTLOADED )
	{
		return false;
	}

	tmLoadLibrary( TM_RELEASE );

	if( !g_pTmMemoryArena )
	{
		g_pTmMemoryArena = new char[ TELEMETRY_ARENA_SIZE ];
	}

	retVal = tmInitialize( TELEMETRY_ARENA_SIZE, g_pTmMemoryArena );
	if ( retVal != TM_OK )
	{
		Warning( "TelemetryInit() failed: tmInitialize() returned %d.\n", retVal );
		return false;
	}

	g_TelemetryStatus = TM_LOADED;

	return true;
}

//--------------------------------------------------------------------------------------
// TelemetryStartSession
//--------------------------------------------------------------------------------------
static bool TelemetryStartSession()
{
	if ( g_TelemetryStatus != TM_LOADED )
	{
		return false;
	}

	tm_error retVal;
	char *pGameName = "csgo";

#if defined( IS_WINDOWS_PC )
	char baseExeFilename[512];
	if( GetModuleFileName ( GetModuleHandle( NULL ), baseExeFilename, sizeof( baseExeFilename ) ) )
	{
		char *pExt = strrchr( baseExeFilename, '.' );

		if( pExt )
			*pExt = 0;

		char *pSeparator = strrchr( baseExeFilename, '\\' );

		pGameName = pSeparator ? ( pSeparator + 1 ) : baseExeFilename;
	}

	// If you've got \\perforce\symbols on your _NT_SYMBOL_PATH, tmOpen() can take a massively long
	//	time in the symInitialize() routine. Since we don't really need that, kill it here.
	putenv( "_NT_SYMBOL_PATH=" );
#endif

	const char *pServerAddress = g_Telemetry.ServerAddress[0] ? g_Telemetry.ServerAddress : "localhost";
	TmConnectionType tmConnectionType = TMCT_TCP; 

	Msg( "TELEMETRY: Calling tmOpen( %s )...\n", pServerAddress );

	char szBuildInfo[ 2048 ];
	_snprintf( szBuildInfo, ARRAYSIZE( szBuildInfo ), "%s: %s", __DATE__ __TIME__, Plat_GetCommandLine() );
	szBuildInfo[ ARRAYSIZE( szBuildInfo ) - 1 ] = 0;
	
	TmU32 tmOpenFlags = TMOF_CAPTURE_CONTEXT_SWITCHES;

	retVal = tmOpen( 0, pGameName, szBuildInfo, pServerAddress, tmConnectionType,
		TELEMETRY_DEFAULT_PORT, tmOpenFlags, 1000 );
	if ( retVal != TM_OK )
	{
		Warning( "TelemetryInitialize() failed: tmOpen returned %d.\n", retVal );
		return false;
	}
	
	Msg( "Telemetry initialized at level %u.\n", g_Telemetry.Level );
#ifdef LINUX
	printf( "Telemetry initialized at level %u.\n", g_Telemetry.Level );
#endif

	/*if( g_bThreadNameArrayChanged )*/
	{
		// Go through and add any new thread names we got in our thread safe list to our thread names array.
		for( ThreadNameInfo_t *pThreadNameInfo = ThreadNameList()->Pop();
			pThreadNameInfo;
			pThreadNameInfo = ThreadNameList()->Pop() )
		{
			if( g_ThreadNameArrayCount < ARRAYSIZE( g_ThreadNameArray ) )
			{
				g_ThreadNameArray[ g_ThreadNameArrayCount ] = *pThreadNameInfo;
				g_ThreadNameArrayCount++;
			}

			delete pThreadNameInfo;
		}

		// Sort the thread array based on g_ThreadSortOrder
		for ( int i = 0; i < ARRAYSIZE( g_ThreadSortOrder); i++ )
		{
			for( int j = 0; j < g_ThreadNameArrayCount; j++ )
			{
				if ( !stricmp( g_ThreadNameArray[j].szName, g_ThreadSortOrder[i] ) && (i != j) )
				{
					Swap( g_ThreadNameArray[j], g_ThreadNameArray[i] );
					break;
				}
			}
		}
		
		for( int i = 0; i < g_ThreadNameArrayCount; i++ )
		{
			tm_uint32 trackID = tmThreadTrack( 0, g_ThreadNameArray[i].ThreadID );
			tmTrackName( 0, trackID, g_ThreadNameArray[i].szName );
			tmTrackOrder( 0, trackID, i );	
		}

		g_bThreadNameArrayChanged = false;
	}

	// Default Zone Filter value to .5ms if they haven't set it already.
	if( !g_Telemetry.ZoneFilterVal )
		g_Telemetry.ZoneFilterVal = 500;

	g_TelemetryStatus = TM_INSESSION;

	return true;
}

//--------------------------------------------------------------------------------------
// TelemetryStopSession
//--------------------------------------------------------------------------------------

static bool TelemetryStopSession()
{
	if ( g_TelemetryStatus != TM_INSESSION )
	{
		return false;
	}

	tmClose( 0 );			
	g_TelemetryStatus = TM_LOADED;

	return true;
}

//--------------------------------------------------------------------------------------
// TelemetryShutdown
//--------------------------------------------------------------------------------------
void TelemetryShutdown( bool InDtor /*= false*/ )
{
	// Msg can't be called here as tier0 may have already been shut down...
	if( !InDtor )
	{
		Msg( "Shutting down telemetry.\n" );
	}

	if ( g_TelemetryStatus == TM_INSESSION )
	{
		TelemetryStopSession();
	}

	if ( g_TelemetryStatus != TM_LOADED )
	{
		return;
	}

	tmShutdown();
	
	if ( g_pTmMemoryArena )
	{
		delete [] g_pTmMemoryArena;
		g_pTmMemoryArena = nullptr;
	}

	g_TelemetryStatus = TM_NOTLOADED;
}

//--------------------------------------------------------------------------------------
// TelemetryShutdown
//--------------------------------------------------------------------------------------
void TelemetryTick()
{
	if ( g_TelemetryStatus == TM_INSESSION )
	{
		tmTick( 0 );
	}

	if( g_fTelemetryLevelChanged )
	{
		g_fTelemetryLevelChanged = false;
		
		unsigned int Level = g_Telemetry.Level;
		if( Level == 0 )
		{
			TelemetryStopSession();
		}
		else
		{
			// Load Telemetry if not already done so
			if( g_TelemetryStatus == TM_NOTLOADED )
			{	
				if ( !TelemetryInitialize() )
				{
					TelemetryShutdown();
					g_Telemetry.Level = 0;
				}
			}

			// Start session if not already done so
			if ( g_TelemetryStatus == TM_LOADED )
			{
				TelemetryStartSession();
			}

			if( g_TelemetryStatus == TM_INSESSION )
			{
				tm_uint32 mask = (1 << g_Telemetry.Level) - 1; 
				tmSetCaptureMask( mask );
				// TODO set zone filter val in clock ticks
			}
		}
	}
}

//--------------------------------------------------------------------------------------
// TelemetryShutdown
//--------------------------------------------------------------------------------------
void TelemetrySetLevel( unsigned int Level )
{
	DevMsg( "TelemetrySetLevel changed from 0x%x to 0x%x (ZoneFilterVal:%d)\n", g_Telemetry.Level, Level, g_Telemetry.ZoneFilterVal );

	if( Level != g_Telemetry.Level )
	{
		g_Telemetry.Level = Level;
		g_TelemetryFrameCount = g_Telemetry.FrameCount;
		g_fTelemetryLevelChanged = true;
	}
}

#endif	// RAD_TELEMETRY_3