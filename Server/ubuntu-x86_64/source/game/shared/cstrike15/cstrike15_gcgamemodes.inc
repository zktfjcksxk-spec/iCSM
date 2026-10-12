//====== Copyright (C), Valve Corporation, All rights reserved. =======
//
// Purpose: This file defines the mapping from GameModes.txt
//			to GC matchmaking game types.
//
//=============================================================================

//			gametype, gamemode,						gc matchmaking enum
GAMEMODEENUM( gungame, gungameprogressive,			k_EMsgGCCStrike15_v2_MatchmakingGame_ArmsRace )
GAMEMODEENUM( gungame, gungametrbomb,				k_EMsgGCCStrike15_v2_MatchmakingGame_Demolition )
GAMEMODEENUM( gungame, deathmatch,					k_EMsgGCCStrike15_v2_MatchmakingGame_Deathmatch )
GAMEMODEENUM( classic, casual,						k_EMsgGCCStrike15_v2_MatchmakingGame_ClassicCasual )
GAMEMODEENUM( classic, competitive,					k_EMsgGCCStrike15_v2_MatchmakingGame_ClassicCompetitive )
GAMEMODEENUM( cooperative, cooperative,				k_EMsgGCCStrike15_v2_MatchmakingGame_Cooperative )
GAMEMODEENUM( cooperative, coopmission,				k_EMsgGCCStrike15_v2_MatchmakingGame_Cooperative )  /* note: same GC mm for both modes, based on quest id */
GAMEMODEENUM( classic, scrimcomp2v2,				k_EMsgGCCStrike15_v2_MatchmakingGame_ScrimComp2v2 )
GAMEMODEENUM( classic, scrimcomp5v5,				k_EMsgGCCStrike15_v2_MatchmakingGame_ScrimComp5v5 )
GAMEMODEENUM( skirmish, skirmish,					k_EMsgGCCStrike15_v2_MatchmakingGame_Skirmish )
GAMEMODEENUM( freeforall, survival,					k_EMsgGCCStrike15_v2_MatchmakingGame_Survival )

