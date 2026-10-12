//========= Copyright (C) Valve Corporation, All rights reserved. ============//
//
// Purpose: Helper for access to global game related functions
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "uicomponent_gameinterface.h"
#include "uicomponent_settings.h"
//#include "uicomponent_mypersona.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

//////////////////////////////////////////////////////////////////////////
//
// Component events
//


//////////////////////////////////////////////////////////////////////////
//
// Component instance
//
SF_COMPONENT_API_DEF_BEGIN( CUiComponent_GameInterface )
#define UI_COMPONENT_FUNCTIONLIST_ELEMENT( returntype, fnname ) SF_COMPONENT_FUNCTION_API_DEF( returntype, fnname, CUiComponent_GameInterface )
#include "uicomponent_gameinterface.functions.inc"
#undef UI_COMPONENT_FUNCTIONLIST_ELEMENT
SF_COMPONENT_API_DEF_END( CUiComponent_GameInterface )

PANORAMA_COMPONENT_API_DEF_BEGIN( CUiComponent_GameInterface )
#define UI_COMPONENT_FUNCTIONLIST_ELEMENT( returntype, fnname ) PANORAMA_COMPONENT_MARSHALL_HELPER_FUNCTION_API_DEF( returntype, fnname, CUiComponent_GameInterface )
#include "uicomponent_gameinterface.functions.inc"
#undef UI_COMPONENT_FUNCTIONLIST_ELEMENT
PANORAMA_COMPONENT_API_DEF_END( CUiComponent_GameInterface )

UI_COMPONENT_API_DEF_COMMON( CUiComponent_GameInterface, GameInterface )


CUiComponent_GameInterface::CUiComponent_GameInterface()
{
}

CUiComponent_GameInterface::~CUiComponent_GameInterface()
{
}

UI_COMPONENT_FUNCTION_IMPL( CUiComponent_GameInterface, ConsoleCommand )
{
	const char* szCommand = pui->Params_GetArgAsString( obj, 0 );
	engine->ClientCmd_Unrestricted( szCommand );
}

extern const char* Helper_GetMouseEnableBindingName();
UI_COMPONENT_FUNCTION_IMPL( CUiComponent_GameInterface, GetMouseEnableBindingName )
{
	pui->Params_SetResult( obj, Helper_GetMouseEnableBindingName() );
}

/////////////////////////////////////////////////////////////////////
// FUNCTION		GetSettingString
// ARGUMENTS	none
//
//////////////////////////////////////////////////////////////////////
static CUiSettingsAliasEntry_t Helper_LookupSettingsPreference( char const *szSettingKey )
{
	// Check if there is an aliasing policy in place?
	UtlSymId_t symPref = g_mapUiSettingsAliases.Find( szSettingKey );
	CUiSettingsAliasEntry_t entry;
	if ( symPref == UTL_INVAL_SYMBOL )
	{
		entry.m_pCvar = g_pCVar ? g_pCVar->FindVar( szSettingKey ) : NULL;
	}
	else
	{
		entry = g_mapUiSettingsAliases[ symPref ];
	}

	// Validate that we have a cvar?
	if ( !entry.m_pCvar )
	{
		DevWarning( "Failed to find ui preference '%s'"
			"\n", szSettingKey );
	}

	return entry;
}
char const * CUiComponent_GameInterface::GetSettingString( char const *szSettingKey )
{
	CUiSettingsAliasEntry_t entry = Helper_LookupSettingsPreference( szSettingKey );
	if ( !entry.m_pCvar )
		return NULL;

	// Bitfield policy?
	if ( entry.m_eBehavior == k_EUiSettingsAliasBehavior_BitField )
	{
		uint32 uiBitMask = entry.m_pCvar->GetInt();
		return ( entry.m_uiValue & uiBitMask ) ? "1" : "0";
	}
	else if ( entry.m_eBehavior == k_EUiSettingsAliasBehavior_TruncateUint64AsUint32 )
	{
		static CFmtStr s_fmt;
		uint32 uiLowerDword = entry.m_pCvar->GetInt();
		s_fmt.Format( "%llu", ( uint64( entry.m_uiValue ) << 32 ) | uiLowerDword );
		return s_fmt.Access();
	}
	else
	{
		// Just return the value
		return entry.m_pCvar->GetString();
	}
}
UI_COMPONENT_FUNCTION_IMPL( CUiComponent_GameInterface, GetSettingString )
{
	char const *szSettingKey = pui->Params_GetArgAsString( obj, 0 );
	if ( char const *szSettingValue = GetSettingString( szSettingKey ) )
		pui->Params_SetResult( obj, szSettingValue );
}

/////////////////////////////////////////////////////////////////////
// FUNCTION		SetUISavedDataKey
// ARGUMENTS	"key::value"
//
//////////////////////////////////////////////////////////////////////
void CUiComponent_GameInterface::SetSettingString( char const *szSettingKey, char const *szSettingValue )
{
	CUiSettingsAliasEntry_t entry = Helper_LookupSettingsPreference( szSettingKey );
	if ( !entry.m_pCvar )
		return;

	// Bitfield policy?
	if ( entry.m_eBehavior == k_EUiSettingsAliasBehavior_BitField )
	{
		uint32 uiBitMask = entry.m_pCvar->GetInt();
		if ( szSettingValue && ( szSettingValue[ 0 ] == '1' ) )
			uiBitMask |= entry.m_uiValue;
		else
			uiBitMask &= ~entry.m_uiValue;
		entry.m_pCvar->SetValue( int( uiBitMask ) );
	}
	else if ( entry.m_eBehavior == k_EUiSettingsAliasBehavior_TruncateUint64AsUint32 )
	{
		entry.m_pCvar->SetValue( int( uint32( Q_atoui64( szSettingValue ) & 0xFFFFFFFF ) ) );
	}
	else
	{
		// Just set the value
		entry.m_pCvar->SetValue( szSettingValue );
	}

// 	if ( entry.m_pCvar->GetFlags() & FCVAR_ARCHIVE )
// 	{
// 		// Schedule writing the config
// 		// (in case multiple preference are updated at the same time, we'll write out config to disk with a small delay, but a full batch at once)
// 		CUiComponent_MyPersona::GetInstance()->RequestDelayedHostWriteConfig( CFmtStr( "%s = %s", szSettingKey, szSettingValue ) );
// 	}
}
UI_COMPONENT_FUNCTION_IMPL( CUiComponent_GameInterface, SetSettingString )
{
	char const *szSettingKey = pui->Params_GetArgAsString( obj, 0 );
	char const *szSettingValue = pui->Params_GetArgAsString( obj, 1 );
	SetSettingString( szSettingKey, szSettingValue );
}
