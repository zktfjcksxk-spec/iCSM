//========= Copyright (C) Valve Corporation, All rights reserved. ============//
//
// Purpose: Define archived cvars for storing and reading UI settings and preferences
//
//=============================================================================//

#include "cbase.h"
#include "uicomponent_settings.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

//////////////////////////////////////////////////////////////////////////
//
// Helper macros to define UI preferences cvars
//
CUtlStringMap< CUiSettingsAliasEntry_t > g_mapUiSettingsAliases;

#define UI_SETTINGS_CVAR( cvar_name, default_string_value ) ConVar cvar_name( #cvar_name, default_string_value, FCVAR_ARCHIVE | FCVAR_RELEASE );
#define UI_SETTINGS_CVAR_ALIAS_NAME( cvar_alias, cvar_name ) static struct C_Ui_Settings_Cvar_Alias_Name_Helper_##cvar_alias { C_Ui_Settings_Cvar_Alias_Name_Helper_##cvar_alias() { \
	g_mapUiSettingsAliases[ #cvar_alias ] = CUiSettingsAliasEntry_t( &cvar_name ); \
	} } s_Ui_Settings_Cvar_Alias_Name_Helper_##cvar_alias;
#define UI_SETTINGS_CVAR_ALIAS_BITFIELD( cvar_alias, cvar_name, uiMask ) static struct C_Ui_Settings_Cvar_Alias_BitField_Helper_##cvar_alias { C_Ui_Settings_Cvar_Alias_BitField_Helper_##cvar_alias() { \
	g_mapUiSettingsAliases[ #cvar_alias ] = CUiSettingsAliasEntry_t( &cvar_name, k_EUiSettingsAliasBehavior_BitField, uiMask ); \
	} } s_Ui_Settings_Cvar_Alias_BitField_Helper_##cvar_alias;
#define UI_SETTINGS_CVAR_ALIAS_TRUNCATEUINT64ASUINT32( cvar_alias, cvar_name, uiExtendHighDword ) static struct C_Ui_Settings_Cvar_Alias_TruncateUint64AsUint32_Helper_##cvar_alias { C_Ui_Settings_Cvar_Alias_TruncateUint64AsUint32_Helper_##cvar_alias() { \
	g_mapUiSettingsAliases[ #cvar_alias ] = CUiSettingsAliasEntry_t( &cvar_name, k_EUiSettingsAliasBehavior_TruncateUint64AsUint32, uiExtendHighDword ); \
	} } s_Ui_Settings_Cvar_Alias_TruncateUint64AsUint32_Helper_##cvar_alias;


//////////////////////////////////////////////////////////////////////////
//
// UI Preferences
//

// Nearby lobbies filter
UI_SETTINGS_CVAR( ui_nearbylobbies_filter, "survival" );

// Preference for which game mode user plays on official servers or offline
UI_SETTINGS_CVAR( ui_playsettings_mode_official_dz, "survival" );
UI_SETTINGS_CVAR_ALIAS_NAME( ui_playsettings_mode_official, ui_playsettings_mode_official_dz );
UI_SETTINGS_CVAR( ui_playsettings_mode_listen, "casual" );

// Preference per game mode of mapgroups to play on official servers
// extern ConVar player_competitive_maplist;
// UI_SETTINGS_CVAR_ALIAS_NAME( ui_playsettings_maps_official_competitive, player_competitive_maplist );
// extern ConVar player_scrimcomp2v2_maplist;
// UI_SETTINGS_CVAR_ALIAS_NAME( ui_playsettings_maps_official_scrimcomp2v2, player_scrimcomp2v2_maplist );
// extern ConVar player_wargames_list;
// UI_SETTINGS_CVAR_ALIAS_NAME( ui_playsettings_maps_official_skirmish, player_wargames_list );
UI_SETTINGS_CVAR( ui_playsettings_maps_official_casual, "mg_casualsigma" );
UI_SETTINGS_CVAR( ui_playsettings_maps_official_deathmatch, "mg_casualsigma" );
UI_SETTINGS_CVAR( ui_playsettings_maps_official_dangerzone, "mg_dz_blacksite" );
// renamed to dangerzone because we shipped official_survival with panorama using xl_enclave, which breaks people's configs since that map doesn't exist.
UI_SETTINGS_CVAR_ALIAS_NAME( ui_playsettings_maps_official_survival, ui_playsettings_maps_official_dangerzone );

// Preference per game mode of mapgroups to play on listen servers
UI_SETTINGS_CVAR( ui_playsettings_maps_listen_competitive, "random_classic" );
UI_SETTINGS_CVAR( ui_playsettings_maps_listen_scrimcomp2v2, "mg_de_inferno" );
UI_SETTINGS_CVAR( ui_playsettings_maps_listen_skirmish, "mg_skirmish_flyingscoutsman" );
UI_SETTINGS_CVAR( ui_playsettings_maps_listen_casual, "random_classic" );
UI_SETTINGS_CVAR( ui_playsettings_maps_listen_deathmatch, "random_classic" );

// Default workshop map
UI_SETTINGS_CVAR( ui_playsettings_maps_workshop, "" );

// Vanity settings for your character holding your featured weapon
UI_SETTINGS_CVAR( ui_vanitysetting_itemid, "" );
UI_SETTINGS_CVAR( ui_vanitysetting_team, "" );
UI_SETTINGS_CVAR( ui_vanitysetting_loadoutslot, "" );

// Renamed to "ui_vanitysetting_model2" for survival model randomization
static char const * g_arrSurvivalModels[] = {
	"models/player/custom_player/legacy/tm_jumpsuit_varianta.mdl",
	"models/player/custom_player/legacy/tm_jumpsuit_variantb.mdl",
	"models/player/custom_player/legacy/tm_jumpsuit_variantc.mdl" };
static char const * GetRandomDefaultSurvivalModel()
{
	float flTime = Plat_FloatTime();
	int nModel = CRC32_ProcessSingleBuffer( &flTime, sizeof( flTime ) ) % ARRAYSIZE( g_arrSurvivalModels );
	return g_arrSurvivalModels[ nModel ];
}
UI_SETTINGS_CVAR( ui_vanitysetting_model2, GetRandomDefaultSurvivalModel() );
UI_SETTINGS_CVAR_ALIAS_NAME( ui_vanitysetting_model, ui_vanitysetting_model2 );

// Warmup map name setting
UI_SETTINGS_CVAR( ui_playsettings_warmup_map_name, "de_mirage" );

// Survival auto-fill squads preference
UI_SETTINGS_CVAR( ui_playsettings_survival_solo, "0" );

// Recently acknowledged items. Cleared every session on start up before you acknowledge items
UI_SETTINGS_CVAR( ui_inventorysettings_recently_acknowledged, "" );

// bot difficulty
UI_SETTINGS_CVAR( player_botdifflast_s, "2" );

// show weapon update popup
UI_SETTINGS_CVAR( ui_popup_weaponupdate_version, "0" );

// key-binding to enable mouse access on scoreboard
extern ConVar cl_scoreboard_mouse_enable_binding;

//////////////////////////////////////////////////////////////////////////
//
// Preferences that require custom behavior
//

void FnChangeCallback_lobby_default_privacy_bits( IConVar *var, const char *pOldValue, float flOldValue )
{
	static bool s_bPerfectWorld = !!CommandLine()->FindParm( "-perfectworld" );
	static bool s_bInChangeCallback = false;
	if ( s_bInChangeCallback )
		return;

	s_bInChangeCallback = true;

	ConVarRef cv( var );
	int nNewValue = -1;
	if ( s_bPerfectWorld )
	{
		switch ( cv.GetInt() )
		{
		case 0:
		case 1:
		case 4:
			break;
		default:
			nNewValue = 4;
		}
	}
	else
	{
		switch ( cv.GetInt() )
		{
		case 0:
		case 1:
		case 2:
		case 4:
		case 6:
			break;
		default:
			nNewValue = 6;
		}
	}

	if ( nNewValue > 0 )
	{
		cv.SetValue( nNewValue );
	}

	s_bInChangeCallback = false;
}
ConVar lobby_default_privacy_bits1(
	"lobby_default_privacy_bits1",
	CommandLine()->FindParm( "-perfectworld" ) ? "4" : "6", // PW defaults to nearby, rest-of-world defaults to groups
	FCVAR_RELEASE | FCVAR_ARCHIVE,
	"Lobby default permissions (0: private, 1: public, 2: clan, 4: nearby, 6: clan and nearby)",
	FnChangeCallback_lobby_default_privacy_bits
);

UI_SETTINGS_CVAR_ALIAS_NAME( lobby_default_privacy_bits, lobby_default_privacy_bits1 );
UI_SETTINGS_CVAR_ALIAS_BITFIELD( lobby_default_privacy_clan_enabled, lobby_default_privacy_bits1, 2 );
UI_SETTINGS_CVAR_ALIAS_BITFIELD( lobby_default_privacy_nearby_enabled, lobby_default_privacy_bits1, 4 );

// Custom clan id behaviour for UI: everywhere in script 765xxx 64-bit CSteamID is expected, but everywhere in code 32-bit clan accountID is used
ConVar  cl_clanid( "cl_clanid", "0", FCVAR_ARCHIVE | FCVAR_USERINFO | FCVAR_HIDDEN, "Current clan ID for name decoration" ); // linking this cvar with engine.dll version
UI_SETTINGS_CVAR_ALIAS_TRUNCATEUINT64ASUINT32( lobby_clanid, cl_clanid, uint32( ( CSteamID( 0, k_EUniversePublic, k_EAccountTypeClan ).ConvertToUint64() >> 32 ) & 0xFFFFFFFF ) );

ConVar key_bind_version( "key_bind_version", "0", FCVAR_CLIENTDLL | FCVAR_RELEASE | FCVAR_HIDDEN | FCVAR_ARCHIVE );

static void RebindKey( const char* szOldBind, const char* szNewBind, ButtonCode_t *buttons, uint32 unButtonCount )
{
	const char *pszKey = engine->Key_LookupBinding( szOldBind );
	const char *pszNewKey = engine->Key_LookupBinding( szNewBind );
	if ( pszKey && !pszNewKey )
	{
		Msg( "Rebinding key %s to new command %s.\n", pszKey, szNewBind );
		engine->ClientCmd_Unrestricted( VarArgs( "bind %s \"%s\"", pszKey, szNewBind ) );
	}
	else if ( !pszKey && !pszNewKey )
	{
		for ( uint32 i = 0; i < unButtonCount; i++ )
		{
			if ( !engine->Key_BindingForKey( buttons[i] ) )
			{
				Msg( "%s was not bound, binding to key %c.\n", szNewBind, buttons[i] - KEY_A + 'a' );
				engine->ClientCmd_Unrestricted( VarArgs( "bind %c \"%s\"", buttons[i] - KEY_A + 'a', szNewBind ) );
				break;
			}
		}
	}
}

#ifdef DEVELOPMENT_ONLY
static void UnbindKey( const char* szOldBind, bool bUnsafeImmediate = false )
#else
static void UnbindKey( const char* szOldBind )
#endif
{
	const char *pszKey = engine->Key_LookupBinding( szOldBind );
	if ( pszKey )
	{
		Msg( "Unbinding old command \"%s\" from key %s.\n", szOldBind, pszKey );

#ifdef DEVELOPMENT_ONLY
		if ( bUnsafeImmediate )
			engine->ExecuteClientCmd( CFmtStr( "unbind %s", pszKey ) );
		else
#endif
			engine->ClientCmd_Unrestricted( CFmtStr( "unbind %s", pszKey ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Rebinds any binds for old commands to their new commands.
//-----------------------------------------------------------------------------
void UTIL_UpdateKeyBindings()
{
	bool bChange = false;

	// For testing
	// key_bind_version.SetValue( 0 );

	if ( key_bind_version.GetInt() < 1 )
	{
		ButtonCode_t buttons[6] = { KEY_F, KEY_T, KEY_V, KEY_G, KEY_B, KEY_H };
		RebindKey( "impulse 100", "+lookatweapon", buttons, V_ARRAYSIZE( buttons ) );
		key_bind_version.SetValue( 1 );
		bChange = true;
	}

	if ( key_bind_version.GetInt() < 2 )
	{
		ButtonCode_t buttons[6] = { KEY_T, KEY_F, KEY_V, KEY_G, KEY_B, KEY_H };
		RebindKey( "impulse 201", "+spray_menu", buttons, V_ARRAYSIZE( buttons ) );
		key_bind_version.SetValue( 2 );
		bChange = true;
	}

#ifdef DEVELOPMENT_ONLY
	if ( key_bind_version.GetInt() == 3 )
	{
		// Unbind old 'slot12' which was temporarily bound in trunk
		UnbindKey( "slot12", true ); // force to unbind immediately (somewhat unsafe) to make sure that we rebind properly in the next section
	}
#endif

	if ( key_bind_version.GetInt() < 4 )
	{
		// Rebind old radio commands
		UnbindKey( "radio3" );
		RebindKey( "radio1", "radio", nullptr, 0 );

		// Try extra hard to find a binding for healthshot
		ButtonCode_t buttons[7] = { KEY_X, KEY_C, KEY_V, KEY_F, KEY_G, KEY_H, KEY_B };
		RebindKey( "radio2", "slot12", buttons, V_ARRAYSIZE( buttons ) );
		key_bind_version.SetValue( 4 );
		bChange = true;

		// also rebind scoreboard mouse enable binding
		if ( !V_strcmp( cl_scoreboard_mouse_enable_binding.GetString(), "radio1" ) )
			cl_scoreboard_mouse_enable_binding.SetValue( "radio" );
	}

#ifdef OSX
	// On OSX we want to force raw input on once after the 64-bit port since
	// we think it's the right choice for most users. Users can still revert
	// this setting if that's what they want, though.
	static ConVarRef osx_force_raw_input_once( "osx_force_raw_input_once" );

	if ( osx_force_raw_input_once.GetInt() == 0 )
	{
		static ConVarRef rawinput( "m_rawinput" );
		rawinput.SetValue( 1 );
		osx_force_raw_input_once.SetValue( 1 );
		bChange = true;
	}
#endif
	
	// Save config changes to disk if any where made
	if ( bChange )
		engine->ClientCmd_Unrestricted( "host_writeconfig" );
}
