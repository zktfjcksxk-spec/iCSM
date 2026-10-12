//====== Copyright © Valve Corporation, All rights reserved. =================
//
// Purpose:
//
//=============================================================================
#include "cbase.h"
#include "cs_gameplay_hints.h"
#include "keyvalues.h"
#include "fmtstr.h"
#include "filesystem.h"
#include "vgui/ILocalize.h"
#include "gametypes/igametypes.h"
#include "cs_gamerules.h"
#include "matchmaking/imatchframework.h"
#include "gametypes.h"

struct CSGameplayHint_t
{
	CSGameplayHint_t( const char* pszToken, uint32 flags ): 
	m_pszLocToken( pszToken ), m_nRequiredContextFlags( flags ), m_nDisplayCount( 0 ) {}
	const char* m_pszLocToken;
	uint32 m_nRequiredContextFlags;
	uint32 m_nDisplayCount;
};

CCSGameplayHints::CCSGameplayHints()
{
	m_pHintKV = NULL;
}

CCSGameplayHints::~CCSGameplayHints()
{
	Cleanup();
}

CCSGameplayHints g_CSGameplayHints;


uint32 CCSGameplayHints::GetCurrentContextFlags( void )
{
	uint32 flags = 0;

	int skirmishId = 0;
	int iGameType = g_pGameTypes->GetCurrentGameType();
	int iGameMode = g_pGameTypes->GetCurrentGameMode();
	char const *pGameType = g_pGameTypes->GetGameTypeFromInt( iGameType );
	char const *pGameMode = g_pGameTypes->GetGameModeFromInt( iGameType, iGameMode );
	char const *szMap = NULL;

	IMatchSession *pIMatchSession = g_pMatchFramework->GetMatchSession();
	if ( pIMatchSession )
	{
		// the session has the CORRECT state
		szMap = pIMatchSession->GetSessionSettings()->GetString( "game/map", NULL );
		pGameType = pIMatchSession->GetSessionSettings()->GetString( "game/type" );
		pGameMode = pIMatchSession->GetSessionSettings()->GetString( "game/mode" );
		skirmishId = pIMatchSession->GetSessionSettings()->GetInt( "game/skirmishmode", 0 );
	}

	if ( !pGameMode || !pGameType )
		return 0;

	g_pGameTypes->GetGameModeAndTypeIntsFromStrings( pGameType, pGameMode, iGameType, iGameMode );

// 	bool isGunGameProgressive = ( iGameType == CS_GameType_GunGame ) && ( iGameMode == CS_GameMode::GunGame_Progressive );
// 	bool isGunGameTRBomb = ( iGameType == CS_GameType_GunGame ) && ( iGameMode == CS_GameMode::GunGame_Bomb );
// 	bool isTrainingMap = ( iGameType == CS_GameType_Training ) && ( iGameMode == CS_GameMode::Training_Training );
// 	bool isGunGameDeathmatch = ( iGameType == CS_GameType_GunGame ) && ( iGameMode == CS_GameMode::GunGame_Deathmatch );
// 	bool isCoopGuardian = ( iGameType == CS_GameType_Cooperative ) && ( iGameMode == CS_GameMode::Cooperative_Guardian );
// 	bool isCooperativeMission = ( iGameType == CS_GameType_Cooperative ) && ( iGameMode == CS_GameMode::Cooperative_Mission );
// 	bool isCooperativeTerrorHunt = ( iGameType == CS_GameType_Cooperative ) && ( iGameMode == CS_GameMode::Cooperative_TerrorHunt );
// 	bool isCustomRules = ( iGameType == CS_GameType_Custom );

	if ( iGameMode == CS_GameMode::Classic_Competitive )
	{
		flags |= HINT_CONTEXT_COMPETITIVE;
	}
	if ( CSGameRules() && CSGameRules()->IsBombDefuseMap() && 
		( CSGameRules()->IsPlayingClassic() || CSGameRules()->IsPlayingGunGameTRBomb() ) )
	{
		flags |= HINT_CONTEXT_BOMB_MAP;
	}
	if ( CSGameRules() && CSGameRules()->IsHostageRescueMap() && CSGameRules()->IsPlayingClassic() )
	{
		flags |= HINT_CONTEXT_HOSTAGE_MAP;
	}
	if ( CSGameRules() && CSGameRules()->IsPlayingGunGame() )
		flags |= HINT_CONTEXT_GUNGAME;

	if ( CSGameRules() && CSGameRules()->IsPlayingSurvival() )
		flags |= HINT_CONTEXT_SURVIVAL;

	if ( CSGameRules() && CSGameRules()->IsPlayingClassic() )
		flags |= HINT_CONTEXT_CLASSIC;

	return flags;
}

uint32 ContextEntryToBitFlag( const char* szName )
{
	if ( V_stricmp( szName, "bomb_map_only" ) == 0 )
	{
		return CCSGameplayHints::HINT_CONTEXT_BOMB_MAP;
	}
	else if ( V_stricmp( szName, "hostage_map_only" ) == 0 )
	{
		return CCSGameplayHints::HINT_CONTEXT_HOSTAGE_MAP;
	}
	else if ( V_stricmp( szName, "gungame_map_only" ) == 0 )
	{
		return CCSGameplayHints::HINT_CONTEXT_GUNGAME;
	}
	else if ( V_stricmp( szName, "competitive_only" ) == 0 )
	{
		return CCSGameplayHints::HINT_CONTEXT_COMPETITIVE;
	}
	else if ( V_stricmp( szName, "survival_only" ) == 0 )
	{
		return CCSGameplayHints::HINT_CONTEXT_SURVIVAL;
	}
	else if ( V_stricmp( szName, "classic_only" ) == 0 )
	{
		return CCSGameplayHints::HINT_CONTEXT_CLASSIC;
	}
	else
	{
		Warning( "HintConfig.txt contains invalid context setting: %s\n", szName );
		Assert( 0 );
	}

	// Show it as a failsafe.
	return CCSGameplayHints::HINT_CONTEXT_ALWAYS_SHOW; 
}

uint32 BuildContextFlags( KeyValues *pContextKeys )
{
	if ( pContextKeys == NULL || pContextKeys->GetFirstSubKey() == NULL )
		return CCSGameplayHints::HINT_CONTEXT_ALWAYS_SHOW; 

	uint32 nFlags = 0;
	for ( KeyValues *entry = pContextKeys->GetFirstSubKey(); entry != NULL; entry = entry->GetNextKey() )
	{
		if ( entry->GetInt() > 0 )
		{
			nFlags |= ContextEntryToBitFlag( entry->GetName() );
		}
	}
	return nFlags;
}

void CCSGameplayHints::PostInit()
{
	Cleanup();

	m_pHintKV = new KeyValues( "HintConfig.txt" );
	if ( m_pHintKV->LoadFromFile( g_pFullFileSystem, "resource/HintConfig.txt" ) )
	{
		KeyValues *hints = m_pHintKV->FindKey( "hints" );
		if ( hints )
		{
			for ( KeyValues *entry = hints->GetFirstSubKey(); entry != NULL; entry = entry->GetNextKey() )
			{
				const char* szLocToken = entry->GetString( "locToken", NULL );
				if ( szLocToken )
				{
					const wchar_t *wszText = g_pVGuiLocalize->Find( szLocToken );
					Assert( wszText );
					// Sanity check length here
					if ( wszText )
					{
						uint32 nContextFlags = BuildContextFlags( entry->FindKey( "context", NULL ) );
						m_HintList.AddToTail( new CSGameplayHint_t( szLocToken, nContextFlags ) );
					}
					else
					{
						Warning( "Bad localization token in resource/HintConfig.txt: '%s'\n", szLocToken );
					}
				}
			}
		}
	}
	
	Assert( m_HintList.Count() > 0 );
 }

void CCSGameplayHints::Cleanup( void )
{
	m_HintList.PurgeAndDeleteElements();
	if ( m_pHintKV )
	{
		m_pHintKV->deleteThis();
		m_pHintKV = NULL;
	}
}
void CCSGameplayHints::Shutdown()
{
	Cleanup();
}

int CompareHintsByDisplayCount( CSGameplayHint_t* const* lhs, CSGameplayHint_t* const* rhs )
{
	if ( (*lhs)->m_nDisplayCount > (*rhs)->m_nDisplayCount )
		return 1;
	else if ( (*lhs)->m_nDisplayCount < (*rhs)->m_nDisplayCount )
		return -1;
	return 0;
}

const char* CCSGameplayHints::GetRandomLeastPlayedHint( void )
{
	if ( m_HintList.Count() == 0 )
		return NULL;

	// Sort by times 'used' and pick among the lowest counts. Counts aren't stored and will 
	// re-zero every time this the CCSGameplayHints class is created (so, every app launch).
	m_HintList.InPlaceQuickSort( CompareHintsByDisplayCount );

	uint32 contextFlags = GetCurrentContextFlags();

	CUtlVector<CSGameplayHint_t*> validHints;
	// Filter out based on current context flags
	FOR_EACH_VEC( m_HintList, i )
	{
		CSGameplayHint_t* hint = m_HintList[i];

		// If bits are set for required context, make sure all of them are currently met. 
		if ( hint->m_nRequiredContextFlags == HINT_CONTEXT_ALWAYS_SHOW ||
			 ( contextFlags & hint->m_nRequiredContextFlags ) == hint->m_nRequiredContextFlags )
		{
			// Early out if we found at least one valid hint and we've moved 
			// on to checking hints that have been displayed more often.
			// After the sort, the first found valid hint should have the lowest display count.
			if ( validHints.Count() > 0 && validHints[0]->m_nDisplayCount < hint->m_nDisplayCount )
				break;

			validHints.AddToTail( hint );
		}
	}

	int pick = RandomInt( 0, validHints.Count()-1 );
	CSGameplayHint_t *hint = validHints[pick];
	hint->m_nDisplayCount++;
	return hint->m_pszLocToken;
}

CON_COMMAND( pick_hint, "" )
{
	const char * szHintLoc = g_CSGameplayHints.GetRandomLeastPlayedHint();
	char szHintUTF8[512];
	V_UnicodeToUTF8( g_pVGuiLocalize->Find( szHintLoc ), szHintUTF8, ARRAYSIZE( szHintUTF8 ) );

	Msg( "Hint: %s: '%s'\n", szHintLoc, szHintUTF8 );
}

