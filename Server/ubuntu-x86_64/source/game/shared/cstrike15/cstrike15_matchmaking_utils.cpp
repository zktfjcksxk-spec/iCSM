//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Shared functions between client, server, and GC
//
//========================================================================//

#include "cbase.h"
#include "cstrike15_matchmaking_utils.h"

// NOTE: This function is copied in mm_title_gamesettingsmgr.cpp; it's not accessible here because of DLL separation.
// Make sure any changes here are duplicated there.
bool UTIL_GenerateMMGame( const char* pchGameMode, EMsgGCCStrike15_v2_MatchmakingGame_t &eMatchmakingGame )
{
	if ( !pchGameMode ) {
		return false;
	}
#define GAMEMODEENUM( gameType, gameMode, gcEnumValue )		\
	else if( !V_stricmp( pchGameMode, #gameMode ) ) {		\
		eMatchmakingGame = gcEnumValue;						\
		return true;										\
	}
#include "cstrike15_gcgamemodes.inc"
#undef GAMEMODEENUM
	else
	{
		return false;
	}
}

bool UTIL_GenerateMMGameType( const char* pchGameMode, const char* pchMapGroup, int questId, bool mapGroupRequired, EMsgGCCStrike15_v2_MatchmakingGame_t &eMatchmakingGame, EMsgGCCStrike15_v2_MatchmakingMapGroup_t &eMatchmakingMapGroup )
{
	/** Removed for partner depot **/
	Warning( "Cannot queue for match on unknown game mode: %s\n", pchGameMode );
	return false;
}