//========= Copyright (C) Valve Corporation, All rights reserved. ============//
//
// Purpose: Define archived cvars for storing and reading UI settings and preferences
//
//=============================================================================//

#pragma once

#include <convar.h>
#include <UtlStringMap.h>

enum EUiSettingsAliasBehavior_t
{
	k_EUiSettingsAliasBehavior_NameAlias = 1,
	k_EUiSettingsAliasBehavior_BitField = 2,
	k_EUiSettingsAliasBehavior_TruncateUint64AsUint32 = 4,
};
struct CUiSettingsAliasEntry_t
{
	explicit CUiSettingsAliasEntry_t( ConVar *pCvar = NULL, EUiSettingsAliasBehavior_t e = k_EUiSettingsAliasBehavior_NameAlias )
		: m_pCvar( pCvar )
		, m_eBehavior( e )
		, m_uiValue( 0 )
		{}
	explicit CUiSettingsAliasEntry_t( ConVar *pCvar, EUiSettingsAliasBehavior_t e, uint32 uiValue )
		: m_pCvar( pCvar )
		, m_eBehavior( e )
		, m_uiValue( uiValue )
		{}
	ConVar *m_pCvar;
	EUiSettingsAliasBehavior_t m_eBehavior;
	uint32 m_uiValue;
};
extern CUtlStringMap< CUiSettingsAliasEntry_t > g_mapUiSettingsAliases;


//////////////////////////////////////////////////////////////////////////
//
// Settings with custom behavior go here
//

enum ELobbyDefaultPrivacy_t
{
	k_ELobbyDefaultPrivacy_Clan = 2,
	k_ELobbyDefaultPrivacy_Nearby = 4,
};
extern ConVar lobby_default_privacy_bits1;

// Needs to be called by ui subsystem sometime after startup / user configs are loaded
void UTIL_UpdateKeyBindings();