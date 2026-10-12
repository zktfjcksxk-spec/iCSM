//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef CTX_DBG_H
#define CTX_DBG_H

#ifdef _WIN32
#pragma once
#endif

#if CSTRIKE_TRUNK_BUILD && PLATFORM_WINDOWS
	#define V8_DEBUGGING_ENABLED 1	// Turn off when perf profiling (DEVELOPMENT ONLY is on for trunk, and _DEBUG builds of staging/rel)
#endif

#define V8_EXCEPTIONS_BREAKPAD_CAPTURE_ENABLED 1 // capture stacks and submit to breakpad (unless debugging and dialog enabled)

#define V8_CTX_DBG_SPEW_ENABLED 0		// *** DO NOT CHECK IN ENABLED! ***

#if (V8_CTX_DBG_SPEW_ENABLED)
	#define V8_CtxDbgMsg( ... )			Msg( "V8 ctx!" ##__VA_ARGS__ )
	#define V8_CtxDbgAssert( ... )		AssertMsgAlways( 0, "V8 ctx!" ##__VA_ARGS__ )
#else
	#define V8_CtxDbgMsg( ... )
	#define V8_CtxDbgAssert( x )
#endif

#define V8_ENABLE_EXCEPTIONS 1

enum V8AssertDlgResult_t
{
	V8_ASSERT_DLG_CONTINUE,
	V8_ASSERT_DLG_DEBUG,
	V8_ASSERT_DLG_IGNORE_ALL
};

V8AssertDlgResult_t V8_AssertDlg( const char *pText, void (*pfnDebuggerRunFrame)(void) );

#endif // CTX_DBG_H