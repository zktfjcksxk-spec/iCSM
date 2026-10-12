//====== Copyright (C), Valve Corporation, All rights reserved. =======
//
// Purpose: This file defines all of our over-the-wire net protocols for the
//			Game Coordinator for CS:GO.  Note that we never use types
//			with undefined length (like int).  Always use an explicit type 
//			(like int32).
//
//=============================================================================

#ifndef CSTRIKE15_GCCONSTANTS_H
#define CSTRIKE15_GCCONSTANTS_H
#ifdef _WIN32
#pragma once
#endif

#define GC_ENABLE_SURVIVAL 1

//=============================================================================

// Migrated from cs_player_rank_shared.h
enum MedalSeasonCoinItemIds_t
{
	MEDAL_SEASON_ACCESS_ENABLED = 0,
	MEDAL_SEASON_ACCESS_FREETOPLAY = 1,		// 0: must own to play or must be sponsored by friend; 1: free to play with an upsell; 2: absolutely free without any notice
	MEDAL_SEASON_ACCESS_VALUE = 7,
	MEDAL_SEASON_COIN_PASS = 1352,			// Currently not used in code, included for completeness
	MEDAL_SEASON_COIN_BRONZE = 4353,
	MEDAL_SEASON_COIN_SILVER = 4354,
	MEDAL_SEASON_COIN_GOLD = 4355,
	MEDAL_SEASON_COIN_PLATINUM = 4356,		// Set to equal to MEDAL_SEASON_COIN_GOLD if not present
};

//=============================================================================

enum EMsgGCCStrike15_v2_ClientLogonFatalError
{
	k_EMsgGCCStrike15_v2_ClientLogonFatalError_None = 0,		// Should not be used (default = no error and logon can succeed)
	k_EMsgGCCStrike15_v2_ClientLogonFatalError_MustUsePWLauncher = 1,	// Client must use PW launcher
	k_EMsgGCCStrike15_v2_ClientLogonFatalError_MustUseSteamLauncher = 2,	// Client must use Steam launcher
	k_EMsgGCCStrike15_v2_ClientLogonFatalError_AccountLinkPWMissing  = 3,	// Client hasn't linked their Steam and PW accounts
	k_EMsgGCCStrike15_v2_ClientLogonFatalError_CustomMessageBase = 1000,	// Custom message has been provided by the 3rd party server (Perfect World)
};

//=============================================================================

enum EMsgGCCStrike15_v2_MatchmakingState_t
{
	k_EMsgGCCStrike15_v2_MatchmakingState_None = 0,				// Client is not in matchmaking pool
	k_EMsgGCCStrike15_v2_MatchmakingState_Joined = 1,			// Client joined matchmaking pool
	k_EMsgGCCStrike15_v2_MatchmakingState_Searching = 2,		// Client is searching in matchmaking pool for some time
	k_EMsgGCCStrike15_v2_MatchmakingState_BackToSearching = 3,	// Client has a match which failed to get confirmed by other players
	k_EMsgGCCStrike15_v2_MatchmakingState_MatchConfirmed = 4,	// Client's match is confirmed and client is removed from matchmaking pool
};

//=============================================================================

enum EMsgGCCStrike15_v2_MatchmakingMatchOutcome_t
{
	k_EMsgGCCStrike15_v2_MatchmakingMatchOutcome_ResultMask = 0x3,			// Typical match result ( 0 = tie, 1 = first team wins, 2 = second team wins, 3 = legacy incomplete )
	k_EMsgGCCStrike15_v2_MatchmakingMatchOutcome_Flag_NetworkEvent = 0x4,	// Network event occurred during the match
};

//=============================================================================

enum EMsgGCCStrike15_v2_ServerReservationFlag_t
{
	k_EMsgGCCStrike15_v2_ServerReservationFlag_None = 0,			// Typical reservation, nothing special
	k_EMsgGCCStrike15_v2_ServerReservationFlag_PrimeOnly = 0x1,		// Prime only NQMM reservation
};

//=============================================================================

enum EMsgGCCStrike15_v2_MatchmakingGameComposition_t
{
	k_EMsgGCCStrike15_v2_MatchmakingGameComposition_bits_Game = 7,
	k_EMsgGCCStrike15_v2_MatchmakingGameComposition_bits_MapGroup = 24,
};

enum EMsgGCCStrike15_v2_MatchmakingVersion_t
{
	k_EMsgGCCStrike15_v2_MatchmakingVersion_MaxValue = 0x4FFF,
	k_EMsgGCCStrike15_v2_MatchmakingVersion_BitMask = 0x7FFF,
	k_EMsgGCCStrike15_v2_MatchmakingVersion_bits = 16,
};

enum EMsgGCCStrike15_v2_MatchmakingGame_t // & 0xF (values from 1 to 15)
{
	k_EMsgGCCstrike15_v2_MatchmakingGame_Invalid			= 0,
	// 1-3 available
	k_EMsgGCCStrike15_v2_MatchmakingGame_ArmsRace			= 4,
	k_EMsgGCCStrike15_v2_MatchmakingGame_Demolition			= 5,
	k_EMsgGCCStrike15_v2_MatchmakingGame_Deathmatch			= 6,
	k_EMsgGCCStrike15_v2_MatchmakingGame_ClassicCasual		= 7,
	k_EMsgGCCStrike15_v2_MatchmakingGame_ClassicCompetitive	= 8, // Used since October 2012
	k_EMsgGCCStrike15_v2_MatchmakingGame_Cooperative		= 9,
	k_EMsgGCCStrike15_v2_MatchmakingGame_ScrimComp2v2		= 10, // Used since April 2017
	k_EMsgGCCStrike15_v2_MatchmakingGame_ScrimComp5v5		= 11, // Used since April 2017
	k_EMsgGCCStrike15_v2_MatchmakingGame_Skirmish			= 12, // Used since April 2017
	k_EMsgGCCStrike15_v2_MatchmakingGame_Survival			= 13, // Used since Nov 2017
	// 14,15 available
};

enum EPlayerRankTypeID_t // Not stored in SQL, but sent from GC->server->client in PlayerRankInfo message
{
	k_EPlayerRankTypeID_Invalid,					// should never be used

	// Casual rank type ids
	k_EPlayerRankTypeID_NQMM_ArmsRace,
	k_EPlayerRankTypeID_NQMM_Demolition,
	k_EPlayerRankTypeID_NQMM_Deathmatch,
	k_EPlayerRankTypeID_NQMM_ClassicCasual,
	k_EPlayerRankTypeID_NQMM_Skirmish,

	// Competitive rank type ids.  Stored in PlayerRankGlicko table except for competitive which lives in MMV2PlayerRanking for historical reasons
	k_EPlayerRankTypeID_Glicko_ClassicCompetitive,	// competitive 5v5 ("Competitive").
	k_EPlayerRankTypeID_Glicko_ScrimComp2v2,		// Post-Operation Hydra competitive 2v2 ("Wingman")

	// Operation hydra pip rank type ids
	k_EPlayerRankTypeID_Pips_ScrimComp2v2_2017,		// Spring 2017 comp 2v2 (wingman)
	k_EPlayerRankTypeID_Pips_ScrimComp5v5_2017,		// Spring 2017 comp 5v5 (weapons expert)

	// Special survival rating that is only used on GC side
	k_EPlayerRankTypeID_Binned_Survival,	// survival population automatic sorted bins

	k_EPlayerRankTypeID_Count
};

enum EPlayerRankTable_t
{
	// Any mode that doesn't have rankings
	k_EPlayerRankTable_Unranked,

	// each table also has a History version (e.g. MMV2PlayerRankingHistory)
	k_EPlayerRankTable_MMV2PlayerRanking,			// classic competitive (TODO: Merge with _PlayerRankGlicko which has a column for rank type)
	k_EPlayerRankTable_PlayerRankGlicko,			// all other glicko ratings
	k_EPlayerRankTable_PlayerRankPips,				// pip ratings used for operation season-based modes
	k_EPlayerRankTable_NqmmPlayerRanking,			// casual ratings for placement into casual servers
};

enum EPipsCalibrationConstants_ScrimComp5v5_2017_t
{
	k_EPipsCalibrationConstants_ScrimComp5v5_2017_Wins = 5,				// How many wins required to reveal pips
	k_EPipsCalibrationConstants_ScrimComp5v5_2017_CompFactor = 100,		// What is the competitive MMR factor when applying calibration pips
	k_EPipsCalibrationConstants_ScrimComp5v5_2017_CompMinVal = 1000,	// What competitive MMR low bound is used when applying calibration pips
	k_EPipsCalibrationConstants_ScrimComp5v5_2017_CompMaxVal = 2000,	// What competitive MMR high bound is used when applying calibration pips
};

enum EMsgGCCStrike15_v2_SeasonTime_t
{
	k_EMsgGCCStrike15_v2_SeasonTime_2013Autumn = 1392822576,	// Operation Bravo ended (Wednesday, February 19, 2014 7:09:36 AM GMT-08:00)
	k_EMsgGCCStrike15_v2_SeasonTime_2014Winter = 1402958328,	// Operation Phoenix ended (Monday, June 16, 2014 3:38:48 PM GMT-07:00 DST)
	k_EMsgGCCStrike15_v2_SeasonTime_2014Summer = 1412899200,	// Operation Breakout ended (Thursday, October 9, 2014 5:00:00 PM GMT-07:00 DST)
	k_EMsgGCCStrike15_v2_SeasonTime_2015Spring = 1430179200,	// Operation Vanguard ended (Monday, April 27, 2015 5:00:00 PM GMT-07:00 DST)
	k_EMsgGCCStrike15_v2_SeasonTime_2015Autumn = 1449089753,	// Operation Bloodhound ended (Wednesday, December 2, 2015 12:55:53 PM GMT-08:00)
	k_EMsgGCCStrike15_v2_SeasonTime_2016Summer = 1472688000,    // Operation Wildfire ended (Wednesday, August 31, 2016 5:00:00 PM GMT-07:00 DST)
	k_EMsgGCCStrike15_v2_SeasonTime_2017Autumn = 1512086400,    // Operation Hydra ended (Thursday, November 30, 2017 4:00:00 PM GMT-08:00)
	k_EMsgGCCStrike15_v2_SeasonTime_2019January = 1548376380,   // Release of Zoo,Abbey,Vertigo2v2 (Thursday, January 24, 2019 4:33:00 PM GMT-08:00)
};

// Up to 22 simultaneous skirmish modes are supported; limited by bits in EMsgGCCStrike15_v2_MatchmakingMapGroup_t
// but we set it to the highest in-use skirmish now so that when future skirmishes appear in schema,
// developers didn't forget to update the full mask in k_EMsgGCCStrike15_v2_MatchmakingMapGroup_skirmish 
#define k_GCCstrike15_MaxSkirmishes 12

enum EMsgGCCStrike15_v2_MatchmakingMapGroup_t // combines with Game_t above, 24 bits available (up to 1<<23)
{
	// NOTE: Changing  names/values in this enum will break old match info records, which have their
	//       map group encoded using this enum, and mapped to map name.  For details, see
	//       MatchmakingGameTypeMapToString()

	// Bit 0 = available for operation map
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_op_map000		= ( 1 << 0 ),
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_dust		= ( 1 << 0 ),	// launch - Hydra

	// Bit 1 = permanent map Dust II
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_dust2		= ( 1 << 1 ),

	// Bit 2 = permanent map Train
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_train		= ( 1 << 2 ),

	// Bit 3 = available for operation map
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_op_map003		= ( 1 << 3 ),
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_aztec		= ( 1 << 3 ),	// launch - Hydra

	// Bit 4 = permanent map Inferno
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_inferno		= ( 1 << 4 ),	// Note: also used below in 2v2, be careful if changed.

	// Bit 5 = permanent map Nuke
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_nuke		= ( 1 << 5 ),

	// Bit 6 = available for operation map
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_vertigo		= ( 1 << 6 ),	// launch - Hydra, Jan 2019 added into Wingman

	// Bit 7 = permanent map Mirage
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_mirage		= ( 1 << 7 ), // (0x080)

	// Bit 8 = permanent map Office
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_cs_office		= ( 1 << 8 ),

	// Bit 9 = permanent map Italy
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_cs_italy		= ( 1 << 9 ),

	// Bit 10 = permanent map Assault
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_cs_assault		= ( 1 << 10 ),

	// Bit 11 = holiday map Militia
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_cs_militia		= ( 1 << 11 ),	// (0x800) // launch - Hydra

	// Bit 12 = permanent map Cache
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_cache		= ( 1 << 12 ),	// Bravo, Phoenix => permanent map

	// Bit 13 = permanent map Austria
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_op_map013		= ( 1 << 13 ),	// (generic)
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_gwalior		= ( 1 << 13 ),	// Bravo
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_cs_motel		= ( 1 << 13 ),	// Phoenix
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_cs_rush		= ( 1 << 13 ),	// Breakout
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_season		= ( 1 << 13 ),	// Vanguard, Bloodhound
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_cs_cruise		= ( 1 << 13 ),	// Wildfire
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_austria		= ( 1 << 13 ),  // Hydra till Jan 2019

	// Bit 14 = Oct2018 Biome
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_ali			= ( 1 << 14 ),	// Bravo, Phoenix
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_blackgold	= ( 1 << 14 ),	// Breakout, Hydra
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_marquis		= ( 1 << 14 ),	// Vanguard
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_log			= ( 1 << 14 ),	// Bloodhound
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_coast		= ( 1 << 14 ),	// Wildfire
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_biome		= ( 1 << 14 ),	// October 2018

	// Bit 15 = Oct2018 Subzero
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_op_map015		= ( 1 << 15 ),	// (generic)
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_ruins		= ( 1 << 15 ),	// Bravo
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_cs_thunder		= ( 1 << 15 ),	// Phoenix
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_mist		= ( 1 << 15 ),	// Breakout
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_facade		= ( 1 << 15 ),	// Vanguard
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_rails		= ( 1 << 15 ),	// Bloodhound
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_empire		= ( 1 << 15 ),	// Wildfire
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_lite		= ( 1 << 15 ),	// Hydra
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_subzero		= ( 1 << 15 ),	// October 2018 till Jan 2019

	// Bit 16 = available for operation map
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_chinatown	= ( 1 << 16 ),	// Bravo
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_favela		= ( 1 << 16 ),	// Phoenix
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_cs_backalley	= ( 1 << 16 ),	// Vanguard
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_resort		= ( 1 << 16 ),	// Bloodhound
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_mikla		= ( 1 << 16 ),	// Wildfire
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_cs_insertion	= ( 1 << 16 ),	// Breakout, Hydra
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_abbey		= ( 1 << 16 ),	// Jan 2019

	// Bit 17 = available for operation map
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_seaside		= ( 1 << 17 ),	// Payback, Bravo, Phoenix
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_overgrown	= ( 1 << 17 ),	// Breakout
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_cs_workout		= ( 1 << 17 ),	// Vanguard
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_zoo			= ( 1 << 17 ),	// Bloodhound, Jan 2019
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_royal		= ( 1 << 17 ),	// Wildfire
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_shipped		= ( 1 << 17 ),  // Hydra

	// Bit 18 = available for operation map
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_op_map018		= ( 1 << 18 ),  // (generic)
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_cs_siege		= ( 1 << 18 ),	// Bravo
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_cs_downtown	= ( 1 << 18 ),	// Phoenix
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_castle		= ( 1 << 18 ),	// Breakout
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_bazaar		= ( 1 << 18 ),	// Vanguard
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_santorini	= ( 1 << 18 ),	// Wildfire
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_thrill		= ( 1 << 18 ),  // Hydra

	// Bit 19 = permanent map Agency
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_tulip		= ( 1 << 19 ),	// Wildfire
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_cs_agency		= ( 1 << 19 ),	// Bravo, Phoenix, Bloodhound, Hydra => permanent map

	// Bit 20 = permanent map Overpass
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_overpass	= ( 1 << 20 ),	// Note: also used below in 2v2, be careful if changed.
	
	// Bit 21 = permanent map Cobblestone
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_cbble		= ( 1 << 21 ),	// Note: also used below in 2v2, be careful if changed.

	// Bit 22 = permanent map Canals
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_canals		= ( 1 << 22 ),

	// 23 free, should be debugged extensively before using as it makes numbers negative (when combined with game type for C++ and SQL)
	// k_EMsgGCCStrike15_v2_MatchmakingMapGroup_questionable	= ( 1 << 23 ),

	// 
	// NO MORE BITS AVAILABLE HERE UNFORTUNATELY
	// ALL ACTIVE MAPGROUPS MUST FIT IN 0xFFFFFF (24 bits, 1<<0 to 1<<23)
	// 1<<23 should be debugged extensively before using as it makes numbers negative (when combined with game type for C++ and SQL)
	//

	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_tournament_maps =
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_inferno		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_nuke		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_mirage		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_train		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_overpass	|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_dust2		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_cache		|
		0,

	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_operation_maps =
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_op_map000		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_op_map003		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_vertigo		| // 2v2 only for now (Jan 2019)
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_op_map013		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_op_map015		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_op_map018		|
		0,

	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_hostage_maps =
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_cs_agency		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_cs_office		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_cs_italy		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_cs_assault		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_cs_militia		|
		0,

	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_casualdelta_maps =
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_mirage		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_inferno		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_cache		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_cbble		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_train		|
		0,

	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_casualsigma_maps =
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_biome		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_abbey		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_zoo			|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_overpass	|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_nuke		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_canals		|
		0,

	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_all_valid_competitive =
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_tournament_maps|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_cbble		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_abbey		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_zoo			|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_biome		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_cs_office		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_cs_agency		|
		0,

	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_all_valid_casual =
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_dust2			|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_casualdelta_maps	|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_casualsigma_maps	|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_hostage_maps		|
		0,

	// --- Begin special mapgroups for non-competitive game modes
	// AR = arms race, DEM = demolition, 2v2 = ScrimComp2v2, 0G = lowgravity, SVL = survival
	// No overlap is allowed for values that share a group
														//		1 << 0
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_ar_shoots		= ( 1 << 0 ),	//	AR			0G
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_bank		= ( 1 << 0 ),	//		DEM	2v2
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_dz_blacksite	= ( 1 << 0 ),	//					SVL
														//		1 << 1
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_ar_baggage		= ( 1 << 1 ),	//	AR
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_ar_dizzy		= ( 1 << 1 ),	//				0G
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_shorttrain	= ( 1 << 1 ),	//		DEM	2v2 (has recorded matches from Operation Hydra)
	// k_EMsgGCCStrike15_v2_MatchmakingMapGroup_xl_dust		= ( 1 << 1 ),	//					SVL
														//		1 << 2
	// de_train (defined above)								= ( 1 << 2 ),	//			2v2
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_ar_monastery	= ( 1 << 2 ),	//	AR
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_sugarcane	= ( 1 << 2 ),	//		DEM
																				//		1 << 3
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_lake		= ( 1 << 3 ),	//	AR	DEM	2v2	0G
														//		1 << 4
	// de_inferno (defined above)							= ( 1 << 4 ),	//			2v2
														//		1 << 5
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_safehouse	= ( 1 << 5 ),	//	AR	DEM	   	0G
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_shortnuke	= ( 1 << 5 ),	//	  	   	2v2	   (intentionally matches de_nuke bit)
														//		1 << 6
	// de_vertigo (defined above)							= ( 1 << 6 ),	//			2v2
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_LEGACYCOMPATIBILITY_de_shortdust	= ( 1 << 6 ),	//		DEM	2v2 (before Jan 2019 recorded as = 1 << 6)
														//		1 << 7
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_gd_rialto		= ( 1 << 7 ),	//			2v2
														//		1 << 8
														//		1 << 9
														//		1 << 10
														//		1 << 11
														//		1 << 12
														//		1 << 13
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_stmarc		= ( 1 << 13 ),	//	AR	DEM	2v2
														//		1 << 14
														//		1 << 15
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_shortdust	= ( 1 << 15 ),	//		DEM	2v2 (before Jan 2019 recorded as = 1 << 6)
														//		1 << 16
														//		1 << 17
														//		1 << 18
														//		1 << 19
														//		1 << 20
	// de_overpass (defined above)							= ( 1 << 20 ),	//			2v2
														//		1 << 21
	// de_cbble (defined above)								= ( 1 << 21 ),	//			2v2
														//		1 << 22
	// de_canals (defined above)							= ( 1 << 22 ),	//			not used

	// --- combination of special mapgroups for non-competitive game modes
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_AR				=
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_ar_shoots		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_ar_baggage		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_ar_monastery	|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_lake		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_stmarc		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_safehouse	|
		0,
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_DEMO			=
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_bank		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_sugarcane	|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_lake		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_stmarc		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_safehouse	|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_shortdust	|
		0,
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_ScrimComp2v2	=
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_train		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_lake		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_shortdust	|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_shortnuke	|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_gd_rialto		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_inferno		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_cbble		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_overpass	|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_vertigo		|
		0,
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_LowGravity		=
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_ar_shoots		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_ar_dizzy		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_lake		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_safehouse	|
		0,
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_Survival		=
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_dz_blacksite	|
		// k_EMsgGCCStrike15_v2_MatchmakingMapGroup_xl_dust		|
		0,
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_SurvivalForceSolo = ( 1 << 23 ),	// Special flag on survival matchmaking indicating that this user doesn't want a partner
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_dust247		= k_EMsgGCCStrike15_v2_MatchmakingMapGroup_de_dust2,		// Dust 2 (24x7) group
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_casualdelta	= k_EMsgGCCStrike15_v2_MatchmakingMapGroup_casualdelta_maps,	// Group Delta
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_casualsigma	= k_EMsgGCCStrike15_v2_MatchmakingMapGroup_casualsigma_maps,	// Group Sigma
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_hostage		= k_EMsgGCCStrike15_v2_MatchmakingMapGroup_hostage_maps,	// Hostage group
	// --- end of special mapgroups for non-competitive game modes

	// Matchmaking "map groups" for War Games.  Matches with skirmish ids (CSkirmishModeDefinition from econ_item_schema.h)
	// Kept in-sync with content/csgo/gamemodes/WarGames.xlsm (using id-1 since we use bit 0 to represent skirmish 1)
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_skirmish_stabstabzap			= ( 1 << 0 ),
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_skirmish_flyingscoutsman		= ( 1 << 2 ),
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_skirmish_triggerdiscipline		= ( 1 << 3 ),
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_skirmish_headshots				= ( 1 << 5 ),
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_skirmish_huntergatherers		= ( 1 << 6 ),
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_skirmish_heavyassaultsuit		= ( 1 << 7 ),
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_skirmish_armsrace				= ( 1 << 9 ),
	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_skirmish_demolition			= ( 1 << 10 ),

	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_skirmish_op08 =
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_skirmish_stabstabzap		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_skirmish_flyingscoutsman	|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_skirmish_triggerdiscipline |
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_skirmish_headshots			|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_skirmish_huntergatherers	|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_skirmish_heavyassaultsuit	|
		0,

	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_skirmish_post_op08 =
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_skirmish_armsrace			|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_skirmish_demolition		|
		k_EMsgGCCStrike15_v2_MatchmakingMapGroup_skirmish_flyingscoutsman	|
		0,

	k_EMsgGCCStrike15_v2_MatchmakingMapGroup_skirmish = k_EMsgGCCStrike15_v2_MatchmakingMapGroup_skirmish_post_op08,
};

enum EMsgGCCStrike15_v2_MatchmakingMap_t
{
	//
	// WARNING: These constants CANNOT be renumbered as they are stored in SQL!
	//

	k_EMsgGCCStrike15_v2_MatchmakingMap_undefined			= 0,

	k_EMsgGCCStrike15_v2_MatchmakingMap_de_dust				= 1,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_dust2			= 2,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_train			= 3,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_aztec			= 4,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_inferno			= 5,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_nuke				= 6,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_vertigo			= 7,
	k_EMsgGCCStrike15_v2_MatchmakingMap_cs_office			= 8,
	k_EMsgGCCStrike15_v2_MatchmakingMap_cs_italy			= 9,
	k_EMsgGCCStrike15_v2_MatchmakingMap_ar_baggage			= 10,
	k_EMsgGCCStrike15_v2_MatchmakingMap_ar_baloney			= 11,
	k_EMsgGCCStrike15_v2_MatchmakingMap_ar_monastery		= 12,
	k_EMsgGCCStrike15_v2_MatchmakingMap_ar_shoots			= 13,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_bank				= 14,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_glass			= 15,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_lake				= 16,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_safehouse		= 17,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_shorttrain		= 18,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_stmarc			= 19,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_sugarcane		= 20,
	k_EMsgGCCStrike15_v2_MatchmakingMap_cs_assault			= 21,
	k_EMsgGCCStrike15_v2_MatchmakingMap_cs_militia			= 22,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_mirage			= 23,

	k_EMsgGCCStrike15_v2_MatchmakingMap_de_cache			= 24,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_gwalior			= 25,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_ali				= 26,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_ruins			= 27,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_chinatown		= 28,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_seaside			= 29,
	k_EMsgGCCStrike15_v2_MatchmakingMap_cs_siege			= 30,
	k_EMsgGCCStrike15_v2_MatchmakingMap_cs_agency			= 31,

	k_EMsgGCCStrike15_v2_MatchmakingMap_de_overpass			= 32,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_cbble			= 33,

	k_EMsgGCCStrike15_v2_MatchmakingMap_cs_motel			= 34,
	k_EMsgGCCStrike15_v2_MatchmakingMap_cs_downtown			= 35,
	k_EMsgGCCStrike15_v2_MatchmakingMap_cs_thunder			= 36,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_favela			= 37,

	k_EMsgGCCStrike15_v2_MatchmakingMap_cs_rush				= 38,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_mist				= 39,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_castle			= 40,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_overgrown		= 41,
	k_EMsgGCCStrike15_v2_MatchmakingMap_cs_insertion		= 42,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_blackgold		= 43,

	k_EMsgGCCStrike15_v2_MatchmakingMap_de_season			= 44,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_marquis			= 45,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_facade			= 46,
	k_EMsgGCCStrike15_v2_MatchmakingMap_cs_backalley		= 47,
	k_EMsgGCCStrike15_v2_MatchmakingMap_cs_workout			= 48,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_bazaar			= 49,

	k_EMsgGCCStrike15_v2_MatchmakingMap_de_shortdust		= 50,

	k_EMsgGCCStrike15_v2_MatchmakingMap_de_rails			= 51,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_resort			= 52,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_zoo				= 53,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_log				= 54,

	k_EMsgGCCStrike15_v2_MatchmakingMap_gd_crashsite		= 55,
	k_EMsgGCCStrike15_v2_MatchmakingMap_gd_lake				= 56,
	k_EMsgGCCStrike15_v2_MatchmakingMap_gd_bank				= 57,
	k_EMsgGCCStrike15_v2_MatchmakingMap_gd_cbble			= 58,

	k_EMsgGCCStrike15_v2_MatchmakingMap_cs_cruise			= 59,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_coast			= 60,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_empire			= 61,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_mikla			= 62,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_royal			= 63,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_santorini		= 64,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_tulip			= 65,

	k_EMsgGCCStrike15_v2_MatchmakingMap_gd_sugarcane		= 66,
	k_EMsgGCCStrike15_v2_MatchmakingMap_coop_cementplant	= 67,

	k_EMsgGCCStrike15_v2_MatchmakingMap_de_canals			= 68,
	k_EMsgGCCStrike15_v2_MatchmakingMap_gd_rialto			= 69,
	
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_austria			= 70,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_lite				= 71,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_shipped			= 72,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_thrill			= 73,

	k_EMsgGCCStrike15_v2_MatchmakingMap_ar_dizzy			= 74,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_shortnuke		= 75,

	k_EMsgGCCStrike15_v2_MatchmakingMap_de_biome			= 76,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_subzero			= 77,

	k_EMsgGCCStrike15_v2_MatchmakingMap_dz_blacksite		= 78,
	k_EMsgGCCStrike15_v2_MatchmakingMap_de_abbey			= 79,

	//
	// WARNING: These constants CANNOT be renumbered as they are stored in SQL!
	//
};

//=============================================================================

enum EMsgGCCStrike15_v2_SessionNeed_t
{
	//
	// WARNING: These constants CANNOT be renumbered as they are used for client<>gc communication
	//
	// They represent client state to help GC better schedule logon surges
	//

	k_EMsgGCCStrike15_v2_SessionNeed_Default = 0,
	k_EMsgGCCStrike15_v2_SessionNeed_OnServer = 1,
	k_EMsgGCCStrike15_v2_SessionNeed_FindGame = 2,
	k_EMsgGCCStrike15_v2_SessionNeed_PartyLobby = 3,
	k_EMsgGCCStrike15_v2_SessionNeed_Overwatch = 4,

	//
	// WARNING: These constants CANNOT be renumbered as they are used for client<>gc communication
	//
};

//=============================================================================

enum EMsgGCCStrike15_v2_AccountActivity_t
{
	//
	// WARNING: These constants CANNOT be renumbered as they are used for client<>gc communication
	//
	// (0-0xF for compact GC representation)
	//

	k_EMsgGCCStrike15_v2_AccountActivity_None				= 0,
	k_EMsgGCCStrike15_v2_AccountActivity_Playing			= 1,
	k_EMsgGCCStrike15_v2_AccountActivity_SpecConnected		= 2,
	k_EMsgGCCStrike15_v2_AccountActivity_SpecGOTV			= 3,
	k_EMsgGCCStrike15_v2_AccountActivity_SpecOverwatch		= 4,
	k_EMsgGCCStrike15_v2_AccountActivity_SpecTwitch			= 5,
	k_EMsgGCCStrike15_v2_AccountActivity_count				= 6,

	//
	// WARNING: These constants CANNOT be renumbered as they are used for client<>gc communication
	//

	k_EMsgGCCStrike15_v2_AccountActivity_RatelimitSeconds	= 180, // activity messages are ratelimited
};

//=============================================================================

enum EMsgGCCStrike15_v2_MatchmakingKickBanReason_t
{
	//
	// WARNING: These constants CANNOT be renumbered as they are stored in SQL!
	//

	k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_VotedOff	= 1,
	k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_TKLimit	= 2,
	k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_TKSpawn	= 3,
	k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_DisconnectedTooLong	= 4,
	k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_Abandoned	= 5,
	k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_THLimit	= 6,
	k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_THSpawn	= 7,
	k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_OfficialBan = 8,
	k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_KickedTooMuch = 9,
	k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_ConvictedForCheating = 10,
	k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_ConvictedForBehavior = 11,
	k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_Abandoned_Grace = 12,
	k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_DisconnectedTooLong_Grace = 13,
	k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_ChallengeNotification = 14,
	k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_NoUserSession = 15,
	k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_FailedToConnect = 16,
	k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_KickAbuse = 17,
	k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_SkillGroupCalibration = 18,
	k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_GsltViolation = 19,
	k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_GsltViolation_Repeated = 20,

	//
	// WARNING: These constants CANNOT be renumbered as they are stored in SQL!
	//
};

inline bool EMsgGCCStrike15_v2_MatchmakingKickBanReason_IsGlobal( uint32 eReason )
{
	switch ( eReason )
	{
	case k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_OfficialBan:
	case k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_ConvictedForCheating:
	case k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_ConvictedForBehavior:
	case k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_ChallengeNotification:
	case k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_GsltViolation:
	case k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_GsltViolation_Repeated:
	case k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_NoUserSession:
		return true;
	default:
		return false;
	}
}

inline bool EMsgGCCStrike15_v2_MatchmakingKickBanReason_IsPermanent( uint32 eReason )
{
	switch ( eReason )
	{
	case k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_OfficialBan:
	case k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_ConvictedForCheating:
	case k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_ChallengeNotification:
		return true;
	default:
		return false;
	}
}

inline bool EMsgGCCStrike15_v2_MatchmakingKickBanReason_IsGreen( uint32 eReason )
{
	switch ( eReason )
	{
	case k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_Abandoned_Grace:
	case k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_DisconnectedTooLong_Grace:
	case k_EMsgGCCStrike15_v2_MatchmakingKickBanReason_SkillGroupCalibration:
		return true;
	default:
		return false;
	}
}

//=============================================================================

enum EMMV2OverwatchCasesVerdict_t
{
	// CSGO V2 Overwatch case verdict field, stored in SQL
	k_EMMV2OverwatchCasesVerdict_Pending = 0,
	k_EMMV2OverwatchCasesVerdict_Dismissed = 1,
	k_EMMV2OverwatchCasesVerdict_ConvictedForCheating = 2,
	k_EMMV2OverwatchCasesVerdict_ConvictedForBehavior = 3,
};

enum EMMV2OverwatchCasesUpdateReason_t
{
	// CSGO V2 Overwatch case update request reason, used for communication between client and GC
	k_EMMV2OverwatchCasesUpdateReason_Poll = 0,		// Client is polling for an overwatch case
	k_EMMV2OverwatchCasesUpdateReason_Assign = 1,	// Client is eager to get a case assigned and work on it
	k_EMMV2OverwatchCasesUpdateReason_Downloading = 2,	// Client is downloading the case files
	k_EMMV2OverwatchCasesUpdateReason_Verdict = 3,	// Client is willing to cast a verdict on a previously assigned case
};

enum EMMV2OverwatchCasesStatus_t
{
	// CSGO V2 Overwatch case status field, stored in SQL
	k_EMMV2OverwatchCasesStatus_Default = 0,
	k_EMMV2OverwatchCasesStatus_Ready = 1,
	k_EMMV2OverwatchCasesStatus_ErrorDownloading = 2,
	k_EMMV2OverwatchCasesStatus_ErrorExtracting = 3,
};

enum EMMV2OverwatchCasesType_t
{
	// CSGO V2 Overwatch case type field, stored in SQL
	k_EMMV2OverwatchCasesType_Reports = 0,
	k_EMMV2OverwatchCasesType_Placebo = 1,
	k_EMMV2OverwatchCasesType_VACSuspicion = 2,
	k_EMMV2OverwatchCasesType_Manual = 3,
	k_EMMV2OverwatchCasesType_MLSuspicion = 4,				// Reported by VACnet
	k_EMMV2OverwatchCasesType_MLSuspicionWithPrior = 5,		// Reported by VACnet with a request to initialize the Bayesian prior from the rptcheating field
	k_EMMV2OverwatchCasesType_Max = 6,
};

//=============================================================================

enum EMsgGCCStrike15_v2_GameServerFeatures_t
{
	k_EMsgGCCStrike15_v2_GameServerFeature_None = 0,			// Undefined default
	k_EMsgGCCStrike15_v2_GameServerFeature_WriteReplays = ( 1 << 0 ),	// Runs game modes that write replays
	k_EMsgGCCStrike15_v2_GameServerFeature_NoReplays = ( 1 << 1 ),	// Runs game modes that do not write replays
	k_EMsgGCCStrike15_v2_GameServerFeature_GOTV = ( 1 << 2 ),	// Runs GOTV relays
	k_EMsgGCCStrike15_v2_GameServerFeature_4Players = ( 1 << 3 ),	// Runs <10 player game modes
	k_EMsgGCCStrike15_v2_GameServerFeature_10Players = ( 1 << 4 ),	// Runs 10 player game modes
	k_EMsgGCCStrike15_v2_GameServerFeature_12Players = ( 1 << 5 ),	// Runs 12 player game modes
	k_EMsgGCCStrike15_v2_GameServerFeature_16Players = ( 1 << 6 ),	// Runs 16 player game modes
	k_EMsgGCCStrike15_v2_GameServerFeature_20Players = ( 1 << 7 ),	// Runs 20 player game modes
	k_EMsgGCCStrike15_v2_GameServerFeature_All = 0xFF,			// All features are flagged as supported if game server doesn't elect a subset
};

//=============================================================================

inline uint32 MatchmakingGameTypeCompose( EMsgGCCStrike15_v2_MatchmakingGame_t eGame, EMsgGCCStrike15_v2_MatchmakingMapGroup_t eMapGroup )
{
	return ( ( uint32( eGame ) & 0xF ) << 0 ) | ( ( uint32( eMapGroup ) & 0xFFFFFF ) << 8 );
}

inline EMsgGCCStrike15_v2_MatchmakingGame_t MatchmakingGameTypeToGame( uint32 uiGameType )
{
	return ( EMsgGCCStrike15_v2_MatchmakingGame_t ) ( ( uiGameType >> 0 ) & 0xF );
}

inline EMsgGCCStrike15_v2_MatchmakingMapGroup_t MatchmakingGameTypeToMapGroup( uint32 uiGameType )
{
	return ( EMsgGCCStrike15_v2_MatchmakingMapGroup_t ) ( ( uiGameType >> 8 ) & 0xFFFFFF );
}

inline EPlayerRankTypeID_t MatchmakingGameToRankTypeID( EMsgGCCStrike15_v2_MatchmakingGame_t eGameType )
{
	switch ( eGameType )
	{
	case k_EMsgGCCStrike15_v2_MatchmakingGame_ArmsRace:
		return k_EPlayerRankTypeID_NQMM_ArmsRace;
	case k_EMsgGCCStrike15_v2_MatchmakingGame_Demolition:
		return k_EPlayerRankTypeID_NQMM_Demolition;
	case k_EMsgGCCStrike15_v2_MatchmakingGame_Deathmatch:
		return k_EPlayerRankTypeID_NQMM_Deathmatch;
	case k_EMsgGCCStrike15_v2_MatchmakingGame_ClassicCasual:
		return k_EPlayerRankTypeID_NQMM_ClassicCasual;
	case k_EMsgGCCStrike15_v2_MatchmakingGame_Skirmish:
		return k_EPlayerRankTypeID_NQMM_Skirmish;

	case k_EMsgGCCStrike15_v2_MatchmakingGame_ClassicCompetitive:
		return k_EPlayerRankTypeID_Glicko_ClassicCompetitive;

	case k_EMsgGCCStrike15_v2_MatchmakingGame_ScrimComp2v2:
		// Operation Hydra used Pips for Wingman(scrimcomp2v2) and Weapons Expert(scrimcomp5v5) modes.
		if ( MEDAL_SEASON_ACCESS_ENABLED && MEDAL_SEASON_ACCESS_VALUE == 7 )
			return k_EPlayerRankTypeID_Pips_ScrimComp2v2_2017;

		return k_EPlayerRankTypeID_Glicko_ScrimComp2v2;

	case k_EMsgGCCStrike15_v2_MatchmakingGame_ScrimComp5v5:
		// Operation Hydra used Pips for Wingman(scrimcomp2v2) and Weapons Expert(scrimcomp5v5) modes.
		if ( MEDAL_SEASON_ACCESS_ENABLED && MEDAL_SEASON_ACCESS_VALUE == 7 )
			return k_EPlayerRankTypeID_Pips_ScrimComp5v5_2017;

		return k_EPlayerRankTypeID_Invalid;

	case k_EMsgGCCStrike15_v2_MatchmakingGame_Cooperative:
		return k_EPlayerRankTypeID_Invalid;

	case k_EMsgGCCStrike15_v2_MatchmakingGame_Survival:
		return k_EPlayerRankTypeID_Invalid;
	}

	Assert( false );
	return k_EPlayerRankTypeID_Invalid;
}

inline EPlayerRankTable_t MatchmakingRankTypeToRankTable( EPlayerRankTypeID_t eRankType )
{
	switch ( eRankType )
	{
	case k_EPlayerRankTypeID_NQMM_ArmsRace:
	case k_EPlayerRankTypeID_NQMM_Demolition:
	case k_EPlayerRankTypeID_NQMM_Deathmatch:
	case k_EPlayerRankTypeID_NQMM_ClassicCasual:
	case k_EPlayerRankTypeID_NQMM_Skirmish:
		return k_EPlayerRankTable_NqmmPlayerRanking;

	case k_EPlayerRankTypeID_Glicko_ClassicCompetitive:
		return k_EPlayerRankTable_MMV2PlayerRanking; // TODO: Migrate to glicko table?

	case k_EPlayerRankTypeID_Glicko_ScrimComp2v2:
	case k_EPlayerRankTypeID_Binned_Survival:
		return k_EPlayerRankTable_PlayerRankGlicko;

	case k_EPlayerRankTypeID_Pips_ScrimComp2v2_2017:
	case k_EPlayerRankTypeID_Pips_ScrimComp5v5_2017:
		return k_EPlayerRankTable_PlayerRankPips;

	case k_EPlayerRankTypeID_Invalid:
		return k_EPlayerRankTable_Unranked;
	}

	return k_EPlayerRankTable_Unranked;
};

inline int MatchmakingRankTypeToSQLRankType( EPlayerRankTypeID_t eRankType )
{
	// THESE RESULTS ARE STORED IN SQL!
	switch( eRankType )
	{
	// NQMM table
	case k_EPlayerRankTypeID_NQMM_ArmsRace:					return 0;
	case k_EPlayerRankTypeID_NQMM_Demolition:				return 1;
	case k_EPlayerRankTypeID_NQMM_Deathmatch:				return 2;
	case k_EPlayerRankTypeID_NQMM_ClassicCasual:			return 3;
	case k_EPlayerRankTypeID_NQMM_Skirmish:					return 4;
	// Glicko tables
	case k_EPlayerRankTypeID_Glicko_ClassicCompetitive:		return 0;
	case k_EPlayerRankTypeID_Glicko_ScrimComp2v2:			return 1;
	case k_EPlayerRankTypeID_Binned_Survival:				return 2;
	// Pips table
	case k_EPlayerRankTypeID_Pips_ScrimComp2v2_2017:		return 1;
	case k_EPlayerRankTypeID_Pips_ScrimComp5v5_2017:		return 2;
	}

	return -1;
}

// Inverse of the above function
inline EPlayerRankTypeID_t MatchmakingSQLRankTypeToRankType( EPlayerRankTable_t eTable, int nSqlRankType )
{
	switch ( eTable )
	{
	case k_EPlayerRankTable_MMV2PlayerRanking:
	case k_EPlayerRankTable_PlayerRankGlicko:
		switch ( nSqlRankType )
		{
		case 0:			return k_EPlayerRankTypeID_Glicko_ClassicCompetitive;
		case 1:			return k_EPlayerRankTypeID_Glicko_ScrimComp2v2;
		case 2:			return k_EPlayerRankTypeID_Binned_Survival;
		}
		break;
	case k_EPlayerRankTable_PlayerRankPips:
		switch ( nSqlRankType )
		{
		case 1:			return k_EPlayerRankTypeID_Pips_ScrimComp2v2_2017;
		case 2:			return k_EPlayerRankTypeID_Pips_ScrimComp5v5_2017;
		}
		break;
	case k_EPlayerRankTable_NqmmPlayerRanking:
		switch ( nSqlRankType )
		{
		case 0:			return k_EPlayerRankTypeID_NQMM_ArmsRace;
		case 1:			return k_EPlayerRankTypeID_NQMM_Demolition;
		case 2:			return k_EPlayerRankTypeID_NQMM_Deathmatch;
		case 3:			return k_EPlayerRankTypeID_NQMM_ClassicCasual;
		case 4:			return k_EPlayerRankTypeID_NQMM_Skirmish;
		}
		break;
	case k_EPlayerRankTable_Unranked:
		return k_EPlayerRankTypeID_Invalid;
	}

	Assert( false );
	return k_EPlayerRankTypeID_Invalid;
}


inline bool MatchmakingRankTypeIsUserVisible( EPlayerRankTypeID_t eRankType )
{
	// Explicit visibility of certain rank categories
	switch ( eRankType )
	{
	case k_EPlayerRankTypeID_Binned_Survival:
		return false;
	}

	// Other ranks are visible to the user except NQMM casual ranks
	EPlayerRankTable_t eRankTable = MatchmakingRankTypeToRankTable( eRankType );
	switch ( eRankTable )
	{
	case k_EPlayerRankTable_Unranked:
	case k_EPlayerRankTable_NqmmPlayerRanking:
		return false;
	default:
		return true;
	}
}


inline bool MatchmakingGameTypeGameIsQueuedWithFullMatchStats( EMsgGCCStrike15_v2_MatchmakingGame_t eGame )
{
	switch ( eGame )
	{
	case k_EMsgGCCStrike15_v2_MatchmakingGame_ClassicCompetitive:
	case k_EMsgGCCStrike15_v2_MatchmakingGame_ScrimComp2v2:
		return true;
	default:
		return false;
	}
}

inline bool MatchmakingGameTypeGameIsQueuedRecordingGOTV( EMsgGCCStrike15_v2_MatchmakingGame_t eGame )
{
	return
		( eGame == k_EMsgGCCStrike15_v2_MatchmakingGame_Survival ) || // survival mode records GOTV demos
		MatchmakingGameTypeGameIsQueuedWithFullMatchStats( eGame );
}

inline bool MatchmakingGameTypeGameIsQueuedWithMostStats( EMsgGCCStrike15_v2_MatchmakingGame_t eGame )
{
	switch ( eGame )
	{
	case k_EMsgGCCStrike15_v2_MatchmakingGame_ScrimComp5v5:	// this mode has no ranking restrictions and no GOTV demos
		return true;
	default:
		return MatchmakingGameTypeGameIsQueuedWithFullMatchStats( eGame );
	}
}

inline bool MatchmakingGameTypeGameIsQueued( EMsgGCCStrike15_v2_MatchmakingGame_t eGame )
{
	switch ( eGame )
	{
	case k_EMsgGCCStrike15_v2_MatchmakingGame_Cooperative:	// this mode is queued, but has no stats
	case k_EMsgGCCStrike15_v2_MatchmakingGame_Survival:		// this mode is queued, but again GC doesn't keep track of who plays together and match outcomes
		return true;
	default:
		return MatchmakingGameTypeGameIsQueuedWithMostStats( eGame );
	}
}

inline bool MatchmakingGameTypeGameIsSingleMapGroup( EMsgGCCStrike15_v2_MatchmakingGame_t eGame )
{
	switch ( eGame )
	{
	case k_EMsgGCCStrike15_v2_MatchmakingGame_Cooperative:
		return true;
	default:
		return false;
	}
}

inline bool MatchmakingGameHasPerPlayerRewards( EMsgGCCStrike15_v2_MatchmakingGame_t eGame )
{
	switch ( eGame )
	{
	case k_EMsgGCCStrike15_v2_MatchmakingGame_Survival:
		return true;
	default:
		return false;
	}
}

inline EMsgGCCStrike15_v2_MatchmakingMapGroup_t MatchmakingGameTypeMapGroupExtendToLargeGroup( EMsgGCCStrike15_v2_MatchmakingGame_t eGame, EMsgGCCStrike15_v2_MatchmakingMapGroup_t eGroup )
{
	switch ( eGame )
	{
	case k_EMsgGCCStrike15_v2_MatchmakingGame_ArmsRace:
		return k_EMsgGCCStrike15_v2_MatchmakingMapGroup_AR;
	case k_EMsgGCCStrike15_v2_MatchmakingGame_Demolition:
		return k_EMsgGCCStrike15_v2_MatchmakingMapGroup_DEMO;
	case k_EMsgGCCStrike15_v2_MatchmakingGame_Deathmatch:
	case k_EMsgGCCStrike15_v2_MatchmakingGame_ClassicCasual:
#define MAPGROUPENUM( mgname ) if ( k_EMsgGCCStrike15_v2_MatchmakingMapGroup_##mgname & eGroup ) return k_EMsgGCCStrike15_v2_MatchmakingMapGroup_##mgname;
	/** Removed for partner depot **/
#undef MAPGROUPENUM
		return eGroup;
	case k_EMsgGCCStrike15_v2_MatchmakingGame_Skirmish:
		return k_EMsgGCCStrike15_v2_MatchmakingMapGroup_skirmish;
	default:
		return eGroup;
	}
}

inline uint32 MatchmakingGameTypeGameMaxPlayers( EMsgGCCStrike15_v2_MatchmakingGame_t eGame )
{
	switch ( eGame )
	{
	case k_EMsgGCCStrike15_v2_MatchmakingGame_ArmsRace: return 10;
	case k_EMsgGCCStrike15_v2_MatchmakingGame_Demolition: return 10;
	case k_EMsgGCCStrike15_v2_MatchmakingGame_Deathmatch: return 16;
	case k_EMsgGCCStrike15_v2_MatchmakingGame_ClassicCasual: return 20;
	case k_EMsgGCCStrike15_v2_MatchmakingGame_ClassicCompetitive: return 10;
	case k_EMsgGCCStrike15_v2_MatchmakingGame_Cooperative: return 2;
	case k_EMsgGCCStrike15_v2_MatchmakingGame_ScrimComp2v2: return 4;
	case k_EMsgGCCStrike15_v2_MatchmakingGame_ScrimComp5v5: return 10;
	case k_EMsgGCCStrike15_v2_MatchmakingGame_Skirmish: return 12;
	case k_EMsgGCCStrike15_v2_MatchmakingGame_Survival: return 16;
	default: return 10;
	}
}

inline uint32 MatchmakingGameTypeToGameServerFeaturesMask( EMsgGCCStrike15_v2_MatchmakingGame_t eGame )
{
	uint32 unGameServerFeaturesMask = MatchmakingGameTypeGameIsQueuedWithFullMatchStats( eGame )
		? k_EMsgGCCStrike15_v2_GameServerFeature_WriteReplays
		: k_EMsgGCCStrike15_v2_GameServerFeature_NoReplays
		;
	
	const uint32 numPlayersMax = MatchmakingGameTypeGameMaxPlayers( eGame );
	if ( numPlayersMax <= 4 )
		unGameServerFeaturesMask |= k_EMsgGCCStrike15_v2_GameServerFeature_4Players;
	else if ( numPlayersMax <= 10 )
		unGameServerFeaturesMask |= k_EMsgGCCStrike15_v2_GameServerFeature_10Players;
	else if ( numPlayersMax <= 12 )
		unGameServerFeaturesMask |= k_EMsgGCCStrike15_v2_GameServerFeature_12Players;
	else if ( numPlayersMax <= 16 )
		unGameServerFeaturesMask |= k_EMsgGCCStrike15_v2_GameServerFeature_16Players;
	else
		unGameServerFeaturesMask |= k_EMsgGCCStrike15_v2_GameServerFeature_20Players;
	
	return unGameServerFeaturesMask;
}

inline char const * MatchmakingGameTypeMapToStringScrimComp2v2( EMsgGCCStrike15_v2_MatchmakingMapGroup_t eMapGroup, uint64 uiMatchID )
{
	char const *szMap = NULL;
	/** Removed for partner depot **/
	return szMap;
}

inline char const * MatchmakingGameTypeMapToString( EMsgGCCStrike15_v2_MatchmakingMapGroup_t eMapGroup, uint64 uiMatchID )
{
	char const *szMap = NULL;
	/** Removed for partner depot **/
	return szMap;
}

//=============================================================================

enum EMsgGCCStrike15_v2_WatchInfoConstants_t
{
	k_EMsgGCCStrike15_v2_WatchInfoConstants_MaxAccountsBatchSize = 50,		// How many accounts can be requested in a batch
	k_EMsgGCCStrike15_v2_WatchInfoConstants_MaxAccountsBatchRate = 5,		// 5 requests per minute are allowed
};

//=============================================================================

enum EMsgGCCStrike15_v2_GC2ClientMsgType
{
	k_EMsgGCCStrike15_v2_GC2ClientMsgType_Unconnected = 0,					// Client is Unconnected
	k_EMsgGCCStrike15_v2_GC2ClientMsgType_Unauthorized = 1,					// Client is Unauthorized
	k_EMsgGCCStrike15_v2_GC2ClientMsgType_Unrecognized = 2,					// Unrecognized request
	k_EMsgGCCStrike15_v2_GC2ClientMsgType_BadPayload = 3,					// Request was recognized, but payload was bad
	k_EMsgGCCStrike15_v2_GC2ClientMsgType_ExecutionError = 4,				// Request was not executed
	k_EMsgGCCStrike15_v2_GC2ClientMsgType_PrintTextWarning = 5,				// Response is warning text to be displayed to client
	k_EMsgGCCStrike15_v2_GC2ClientMsgType_PrintTextInfo = 6,				// Response is informational text to be displayed to client
	k_EMsgGCCStrike15_v2_GC2ClientMsgType_WriteFile = 7,					// Response is potentially binary payload to be written to response file
};

//=============================================================================

enum EMsgGCCStrike15_v2_GC2ClientNoteType
{
	k_EMsgGCCStrike15_v2_GC2ClientNoteType_None = 0,						// Nothing
	k_EMsgGCCStrike15_v2_GC2ClientNoteType_ClusterLoadHigh = 1,				// Datacenter has high load
	k_EMsgGCCStrike15_v2_GC2ClientNoteType_ClusterOffline = 2,				// Datacenter is offline
};

//=============================================================================


//=============================================================================

enum EMsgGCAccountPrivacySettingsType_t
{
	//
	// WARNING: These constants CANNOT be renumbered as they are stored in SQL!
	//

	k_EMsgGCAccountPrivacySettingsType_PlayerProfile = 1,					// Player profile including competitive information and commendations

	//
	// WARNING: These constants CANNOT be renumbered as they are stored in SQL!
	//
};
enum EMsgGCAccountPrivacySettingsValue_t
{
	//
	// WARNING: These constants CANNOT be renumbered as they are stored in SQL!
	//

	k_EMsgGCAccountPrivacySettingsValue_Default = 1,					// Setting should be reset to default for the player
	k_EMsgGCAccountPrivacySettingsValue_Disabled = 2,					// Setting should be disabled
	k_EMsgGCAccountPrivacySettingsValue_Enabled = 3,					// Setting should be enabled

	//
	// WARNING: These constants CANNOT be renumbered as they are stored in SQL!
	//
};
inline bool EMsgGCAccountPrivacySettingsType_IsExposedToClient( EMsgGCAccountPrivacySettingsType_t val )
{
	return ( val == k_EMsgGCAccountPrivacySettingsType_PlayerProfile );
}

//=============================================================================

enum EMsgGCAccountPrivacyRequestLevel_t
{
	//
	// WARNING: These constants are used in protobuf communication and cannot renumber if used in same proto field!
	//

	k_EMsgGCAccountPrivacySettingsValue_Public_All =		0,					// Requesting data that is already easily available to everybody (e.g. persona name)
	k_EMsgGCAccountPrivacySettingsValue_Public_Shared =		0x10,				// Setting can be shared with public
	k_EMsgGCAccountPrivacySettingsValue_Friends_Only =		0x20,				// Setting shared with friends
	k_EMsgGCAccountPrivacySettingsValue_Private_All =		0x80,				// All information that should already be available to the owner

	//
	// WARNING: These constants are used in protobuf communication and cannot renumber if used in same proto field!
	//
};

//=============================================================================

enum EMsgGCVarValueNotificationInfoType_t
{
	//
	// WARNING: These constants are used in protobuf communication and cannot renumber if used in same proto field!
	//

	k_EMsgGCVarValueNotificationInfoType_Cmd = 0,					// Data from user cmd
	k_EMsgGCVarValueNotificationInfoType_Divergence = 1,			// Divergence of viewangles
	k_EMsgGCVarValueNotificationInfoType_Inventory = 2,				// Community server misrepresenting inventory or rank
	k_EMsgGCVarValueNotificationInfoType_DenialOfService = 3,		// User is attempting to crash the server

	//
	// WARNING: These constants are used in protobuf communication and cannot renumber if used in same proto field!
	//
};

//=============================================================================

enum EMsgGCClientPartyWarningType_t
{
	//
	// WARNING: These constants are used in protobuf communication and cannot renumber if used in same proto field!
	//

	k_EMsgGCEMsgGCClientPartyWarningType_None = 0,					// No warning, default variable value
	k_EMsgGCEMsgGCClientPartyWarningType_NonPrime = 10,				// Party mate is not prime as determined by GC
	k_EMsgGCEMsgGCClientPartyWarningType_TrustDiffMinor = 20,		// Party mate is prime but has a significant trust value difference
	k_EMsgGCEMsgGCClientPartyWarningType_TrustDiffMajor = 30,		// Party mate is prime but has a major trust value difference

	//
	// WARNING: These constants are used in protobuf communication and cannot renumber if used in same proto field!
	//
};

//=============================================================================

enum EMsgGCCStrike15_v2_NqmmRating_t
{
	k_EMsgGCCStrike15_v2_NqmmRating_Version_Current = 1,		// Current version of nqmm rating serialization
	k_EMsgGCCStrike15_v2_NqmmRating_Version_Survival = 2,		// Serialization of survival standings
};

//=============================================================================

enum EScoreLeaderboardDataEntryTag_t
{
	//
	// WARNING: These constants CANNOT be renumbered as they are stored in SQL!
	//

	k_EScoreLeaderboardDataEntryTag_undefined			= 0,
	k_EScoreLeaderboardDataEntryTag_Kills				= 1,
	k_EScoreLeaderboardDataEntryTag_Assists				= 2,
	k_EScoreLeaderboardDataEntryTag_Deaths				= 3,
	k_EScoreLeaderboardDataEntryTag_Points				= 4,
	k_EScoreLeaderboardDataEntryTag_Headshots			= 5,
	k_EScoreLeaderboardDataEntryTag_ShotsFired			= 6,
	k_EScoreLeaderboardDataEntryTag_ShotsOnTarget		= 7,
	k_EScoreLeaderboardDataEntryTag_HpDmgInflicted		= 8,
	k_EScoreLeaderboardDataEntryTag_HpDmgSuffered		= 9,
	k_EScoreLeaderboardDataEntryTag_TimeElapsed			= 10,
	k_EScoreLeaderboardDataEntryTag_TimeRemaining		= 11,
	k_EScoreLeaderboardDataEntryTag_RoundsPlayed		= 12,
	k_EScoreLeaderboardDataEntryTag_BonusPistolOnly		= 13,
	k_EScoreLeaderboardDataEntryTag_BonusHardMode		= 14,
	k_EScoreLeaderboardDataEntryTag_BonusChallenge		= 15,

	//
	// WARNING: These constants CANNOT be renumbered as they are stored in SQL!
	//
};

//=============================================================================



#endif //CSTRIKE15_GCCONSTANTS_H
