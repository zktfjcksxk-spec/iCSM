//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Real-Time Hierarchical Telemetry Profiling
//
// $NoKeywords: $
//=============================================================================//

#ifndef VPROF_TELEMETRY_3_H
#define VPROF_TELEMETRY_3_H

#if !defined( MAKE_VPC )

#if !defined( RAD_TELEMETRY_DISABLED ) && ( defined( IS_WINDOWS_PC ) || defined( _LINUX ) )
// Rad Telemetry profiling is enabled on Win32 and Win64.
#define RAD_TELEMETRY_ENABLED
#endif

#ifdef WIN32
#define __PRETTY_FUNCTION__ __FUNCSIG__
#endif

#endif // !defined( MAKE_VPC )

#if !defined( RAD_TELEMETRY_ENABLED )
// If Telemetry isn't enabled, then kill all the tmZone() macros, etc.
#define NTELEMETRY		1
#endif

#include "../../thirdparty/telemetry3/include/rad_tm.h"

#if !defined( RAD_TELEMETRY_ENABLED )

inline void TelemetryTick() {}
inline void TelemetrySetLevel( unsigned int Level ) {}
inline void TelemetrySetContextState( void * &ctx, bool bEnabled ) {}
inline void TelemetryShutdown() {}

#define TELEMETRY_REQUIRED( tmRequiredCode ) //a basic wrapper to only enable code if telemetry is present
#define TELEMETRY_REQUIRED_REPLACE( tmRequiredCode, replacementCode ) replacementCode //in case you need to replace the code with something specific if telemetry isn't present

#else

PLATFORM_INTERFACE void TelemetryTick();
PLATFORM_INTERFACE void TelemetrySetLevel( unsigned int Level );
// PLATFORM_INTERFACE void TelemetrySetContextState( HTELEMETRY &ctx, bool bEnabled );
PLATFORM_INTERFACE void TelemetryShutdown( bool InDtor = false );

#define TELEMETRY_REQUIRED( tmRequiredCode ) tmRequiredCode //a basic wrapper to only enable code if telemetry is present
#define TELEMETRY_REQUIRED_REPLACE( tmRequiredCode, replacementCode ) tmRequiredCode //in case you need to replace the code with something specific if telemetry isn't present

struct TelemetryData
{
	float flRDTSCToMilliSeconds;	// Conversion from tmFastTime() (rdtsc) to milliseconds.
	uint32 FrameCount;				// Count of frames to capture before turning off.
	char ServerAddress[128];		// Server name to connect to.
	uint32 ZoneFilterVal;			// tmZoneFiltered default filtered value (in MicroSeconds)
	int playbacktick;				// GetPlaybackTick() value from demo file (or 0 if not playing a demo).
	uint32 DemoTickStart;			// Start telemetry on demo tick #
	uint32 DemoTickEnd;				// End telemetry on demo tick #
	uint32 Level;					// Current Telemetry level (Use TelemetrySetLevel to modify)
};

PLATFORM_INTERFACE TelemetryData g_Telemetry;
PLATFORM_INTERFACE tm_api *TM_API_PTR;			// Export telemetry api ptr from inside Telemetry

#endif	// RAD_TELEMETRY_ENABLED

#define TELEMETRY_LEVEL0	TELEMETRY_REQUIRED_REPLACE( 1 << 0, 0 )	// high level tmZone()
#define TELEMETRY_LEVEL1	TELEMETRY_REQUIRED_REPLACE( 1 << 1, 0 )	// lower level tmZone(), tmZoneFiltered()
#define TELEMETRY_LEVEL2	TELEMETRY_REQUIRED_REPLACE( 1 << 2, 0 )	// VPROF_0
#define TELEMETRY_LEVEL3	TELEMETRY_REQUIRED_REPLACE( 1 << 3, 0 )	// VPROF_1
#define TELEMETRY_LEVEL4	TELEMETRY_REQUIRED_REPLACE( 1 << 4, 0 )	// VPROF_2
#define TELEMETRY_LEVEL5	TELEMETRY_REQUIRED_REPLACE( 1 << 5, 0 )	// VPROF_3
#define TELEMETRY_LEVEL6	TELEMETRY_REQUIRED_REPLACE( 1 << 6, 0 )	// VPROF_4

// Wrapping macros in TELEMETRY_REQUIRED causes trailing comma errors in Telementry 3. Note the Telemetry macros
// resolve to nothing if NTELEMETRY is defined
#define TM_ZONE( mask, kFlags, kpFormat, ... ) tmZoneBase( mask, kFlags, kpFormat, ##__VA_ARGS__ )
#define TM_MESSAGE( mask, kFlags, kpFormatString, ... ) tmMessageBase( mask, kFlags, kpFormatString, ##__VA_ARGS__ )
#define TM_ENTER( mask, kFlags, kpZoneName, ... ) tmEnterBase( mask, kFlags, kpZoneName, ##__VA_ARGS__ )
#define TM_LEAVE( mask ) tmLeaveBase( mask )
#define TM_PAUSE( context, kPause )		// no equivalent in Telemetry 3.0

//Standardized zones
#define TM_ZONE_DEFAULT( context ) TM_ZONE( context, TMZF_NONE, __FUNCTION__ )
#define TM_ZONE_IDLE( context ) TM_ZONE( context, TMZF_IDLE, __FUNCTION__ )
#define TM_ZONE_STALL( context ) TM_ZONE( context, TMZF_STALL, __FUNCTION__ )

// TM_DYNAMIC_STRING not required in Telemetry 3. 
// Strings on stack, called dynamic strings in Telemetry 2, would result in garbage if passed 
// as argument to tmZone, because by the time Telemetry 2 came around to using the string pointer, it had 
// gone out of scope. For example
// function()
// {
//		char buff[256];
//		sprintf( buff, "This will not work" )
//		tmZone ( "Zone = %s", buff );	// Output = "Zone = <garbage>", as display string construction not in-line but at some later time
// }
// In Telemetry 3, strings are processed in-line
#define TM_DYNAMIC_STRING( cx, str ) str

// Not implemented yet
#define TM_ZONE_PLOT(...)
#define TM_ZONE_FILTERED( ... )
#define TelemetrySetLockName(...)

#endif // VPROF_TELEMETRY_3_H
