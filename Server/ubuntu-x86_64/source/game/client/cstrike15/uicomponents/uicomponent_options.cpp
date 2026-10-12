//========= Copyright (C) 1996-2013, Valve Corporation, All rights reserved. ============//
//
// Purpose: Helper for friends list access in Scaleform and Panorama.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"

#include "uicomponent_options.h"

#include "basepanel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

const char *UTIL_Parse( const char *data, char *token, int sizeofToken );

//////////////////////////////////////////////////////////////////////////
//
// Component instance
//

SF_COMPONENT_API_DEF_BEGIN( CUiComponent_OptionsMenu )
SF_COMPONENT_API_DEF_END(CUiComponent_OptionsMenu)

PANORAMA_COMPONENT_API_DEF_BEGIN( CUiComponent_OptionsMenu )
#define UI_COMPONENT_FUNCTIONLIST_ELEMENT( returntype, fnname, argNames, description ) PANORAMA_COMPONENT_FUNCTION_API_DEF_DOC( returntype, fnname, CUiComponent_OptionsMenu, argNames, description )

UI_COMPONENT_FUNCTIONLIST_ELEMENT( void, RestoreKeybdMouseBindingDefaults, "", "" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( bool, ShowSteamControllerBindingsPanel, "", "" )

#undef UI_COMPONENT_FUNCTIONLIST_ELEMENT
PANORAMA_COMPONENT_API_DEF_END( CUiComponent_OptionsMenu )

UI_COMPONENT_API_DEF_COMMON(CUiComponent_OptionsMenu, OptionsMenu )


//////////////////////////////////////////////////////////////////////////
//
// Component Implementation
//

CUiComponent_OptionsMenu::CUiComponent_OptionsMenu()
{
	Msg("CUiComponent_OptionsMenu::CUiComponent_OptionsMenu\n");
}

CUiComponent_OptionsMenu::~CUiComponent_OptionsMenu()
{
}

void CUiComponent_OptionsMenu::RestoreKeybdMouseBindingDefaults()
{
	// Reset all bind options with defaults
	const char * szConfigFile = "cfg/config_default.cfg";

	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );
	if ( !g_pFullFileSystem->ReadFile( szConfigFile, NULL, buf ) )
	{
		Assert( false );
		Warning( "Unable to locate config file used for default settings: %s\n", szConfigFile );
		return;
	}

	const char *data = ( const char * )buf.Base();

	while ( data != NULL )
	{
		char cmd[64];
		data = UTIL_Parse( data, cmd, sizeof( cmd ) );
		if ( V_strlen( cmd ) <= 0 )
			break;

		if ( !V_stricmp(cmd, "bind" ) )
		{
			// Key name
			char szKeyName[256];
			data = UTIL_Parse( data, szKeyName, sizeof(szKeyName) );
			if ( szKeyName[ 0 ] == '\0' )
				break; // Error

			char szBinding[256];
			data = UTIL_Parse( data, szBinding, sizeof(szBinding) );
			if ( szKeyName[ 0 ] == '\0' )  
				break; // Error

			// Bind it
			char szCommand[ 256 ];
			V_snprintf( szCommand, sizeof( szCommand ), "bind \"%s\" \"%s\"", szKeyName, szBinding );
			engine->ExecuteClientCmd( szCommand );
		}
	}		
}

bool CUiComponent_OptionsMenu::ShowSteamControllerBindingsPanel()
{
	if( steamapicontext && steamapicontext->SteamController() )
	{
		ControllerHandle_t handles[ MAX_STEAM_CONTROLLERS ];
		int nControllers = steamapicontext->SteamController()->GetConnectedControllers( handles );
		if ( nControllers > 0 )
		{
			steamapicontext->SteamController()->ShowBindingPanel( handles[ 0 ] );
			return true;
		}
	}

	return false;
}