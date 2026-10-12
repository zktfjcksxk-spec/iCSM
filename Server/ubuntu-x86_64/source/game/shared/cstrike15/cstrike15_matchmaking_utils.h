//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Shared functions between client, server, and GC
//
//========================================================================//

#ifndef CSTRIKE15_MATCHMAKING_UTILS_H
#define CSTRIKE15_MATCHMAKING_UTILS_H

#include "cstrike15_gcconstants.h"

bool UTIL_GenerateMMGame( const char* pchGameMode, EMsgGCCStrike15_v2_MatchmakingGame_t &eMatchmakingGame );
bool UTIL_GenerateMMGameType( const char* pchGameMode, const char* pchMapGroup, int questId, bool mapGroupReqired, EMsgGCCStrike15_v2_MatchmakingGame_t &eMatchmakingGame, EMsgGCCStrike15_v2_MatchmakingMapGroup_t &eMatchmakingMapGroup );
bool UTIL_GetMMRankType( const char* pchGameMode, EPlayerRankTypeID_t &eRankType );

#endif // CSTRIKE15_MATCHMAKING_UTILS_H