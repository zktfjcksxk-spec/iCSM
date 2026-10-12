//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx_client.h"
#include "panorama/iuiengine.h"
#include "interface.h"

#include "uijsregistration.h"

using namespace panorama;

IUIEngine *panorama::g_pUIEngineSingleton = NULL;
#if defined( SOURCE2_PANORAMA ) && !defined( PANORAMA_USE_S1WRAPPER )
PlatModule_t panorama::g_PanoramaModule = PLAT_MODULE_INVALID;
#else
CSysModule *panorama::g_PanoramaModule = NULL;
#endif

//-----------------------------------------------------------------------------
// Purpose: Load module (if not already loaded) from specified path
//-----------------------------------------------------------------------------
bool panorama::LoadPanoramaModule( const char *pchPanoramaModulePath )
{
	if ( g_PanoramaModule == NULL )
	{
#if defined( SOURCE2_PANORAMA ) && !defined( PANORAMA_USE_S1WRAPPER )
		g_PanoramaModule = Plat_LoadModule( pchPanoramaModulePath );
#else
		g_PanoramaModule = Sys_LoadModule( pchPanoramaModulePath );
#endif
	}

	if ( g_PanoramaModule == NULL )
		return false;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Unload module -- Better have shutdown all UIEngines first!!
//-----------------------------------------------------------------------------
void panorama::UnloadPanoramaModule()
{
	if ( g_PanoramaModule != NULL )
	{
#if defined( SOURCE2_PANORAMA ) && !defined( PANORAMA_USE_S1WRAPPER )
		Plat_UnloadModule( g_PanoramaModule );
#else
		Sys_UnloadModule( g_PanoramaModule );
#endif
		g_PanoramaModule = NULL;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Create new panorama UI engine instance
//-----------------------------------------------------------------------------
IUIEngine *panorama::CreatePanoramaUIEngine( )
{
	typedef IUIEngine *(*pCreatePanoramaUIEngineInternal)();
	if ( g_PanoramaModule == NULL )
		return NULL;

	// Only support a single singleton instance for now
	Assert( NULL == g_pUIEngineSingleton );

#if defined( SOURCE2_PANORAMA ) && !defined( PANORAMA_USE_S1WRAPPER )
	pCreatePanoramaUIEngineInternal pFunc = (pCreatePanoramaUIEngineInternal)Plat_GetModuleProcAddress( g_PanoramaModule, "CreatePanoramaUIEngineInternal" );
#else
	pCreatePanoramaUIEngineInternal pFunc = (pCreatePanoramaUIEngineInternal)Sys_GetProcAddress( g_PanoramaModule, "CreatePanoramaUIEngineInternal" );
#endif
	IUIEngine *pEngine = pFunc();
	ConnectPanoramaUIEngine( pEngine );

	Assert( g_pUIEngineSingleton == pEngine );
	return g_pUIEngineSingleton;
}


//-----------------------------------------------------------------------------
// Purpose: Connect UI engine with panorama client instance in local module
//-----------------------------------------------------------------------------
void panorama::ConnectPanoramaUIEngine( IUIEngine * pEngine )
{
	g_pUIEngineSingleton = pEngine;
	RegisterEventTypesWithEngine( g_pUIEngineSingleton );
	RegisterPanelFactoriesWithEngine( g_pUIEngineSingleton );
	RegisterGlobalJSMethods( pEngine );
}


//-----------------------------------------------------------------------------
// Purpose: Shutdown panorama UI engine instance
//-----------------------------------------------------------------------------
void panorama::ShutdownPanoramaUIEngine( IUIEngine *pEngine )
{
	// Only support a single singleton instance for now
	Assert( pEngine == g_pUIEngineSingleton );

	pEngine->Shutdown();
	delete pEngine;
	g_pUIEngineSingleton = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Registers global javascript methods that must be defined on the client side of panorama framework
//-----------------------------------------------------------------------------
void panorama::RegisterGlobalJSMethods( IUIEngine *pEngine )
{
	// Caveat regarding registering functions here. panorama_client is a lib, linked into panoramaclient.dll 
	// and into client.dll. panorama::RegisterGlobalJSMethods gets called twice, and ends up registering two
	// different functions with the same name, the second overriding the first.
	// CreatePanelWithCurrentContext is unused anyway, commenting out registration, revisit if required

	// RegisterJSGlobalFunctionRaw( "CreatePanelWithCurrentContext", &panorama::JSCreatePanelWithCurrentContext, false );
}


#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: Validate panorama statics
//-----------------------------------------------------------------------------
void panorama::ValidateStatics( CValidator &validator )
{
	typedef void( *pValidateStaticsInternal )(CValidator &);
	if ( g_PanoramaModule == NULL )
		return;

	pValidateStaticsInternal pFunc = (pValidateStaticsInternal)Sys_GetProcAddress( g_PanoramaModule, "ValidateStaticsInternal" );
	pFunc( validator );
}
#endif

