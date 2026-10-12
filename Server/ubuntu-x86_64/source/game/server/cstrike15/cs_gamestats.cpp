//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: CS game stats
//
// $NoKeywords: $
//=============================================================================//

// Some tricky business here - we don't want to include the precompiled header for the statreader
// and trying to #ifdef it out does funky things like ignoring the #endif. Define our header file
// separately and include it based on the switch

#include "cbase.h"

#include <tier0/platform.h>
#include "cs_gamerules.h"
#include "cs_gamestats.h"
#include "weapon_csbase.h"
#include "props.h"
#include "cs_achievement_constants.h"
#include "weapon_c4.h"
#include "cs_bot.h"

#include <time.h>
#include "filesystem.h"
#include "bot_util.h"

// needed for recording grenade detonation rows
#include "basecsgrenade_projectile.h"
#include "flashbang_projectile.h"
#include "hegrenade_projectile.h"
#include "Effects/inferno.h"

#if !defined( _GAMECONSOLE )
#include "cdll_int.h"
#endif

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

//#define DEBUG_STAT_TRANSMISSION 

extern ConVar game_mode;
extern ConVar game_type;
extern ConVar mp_roundtime;

float	g_flGameStatsUpdateTime = 0.0f;
short	g_iTerroristVictories[CS_NUM_LEVELS];
short	g_iCounterTVictories[CS_NUM_LEVELS];
//short	g_iWeaponPurchases[WEAPON_MAX];

short	g_iAutoBuyPurchases = 0;
short	g_iReBuyPurchases = 0;
short	g_iAutoBuyM4A1Purchases = 0;
short	g_iAutoBuyAK47Purchases = 0;
short	g_iAutoBuyFamasPurchases = 0;
short	g_iAutoBuyGalilPurchases = 0;
short	g_iAutoBuyGalilARPurchases = 0;
short	g_iAutoBuyVestHelmPurchases = 0;
short	g_iAutoBuyVestPurchases = 0;

struct
{
	char* szPropModelName;
	CSStatType_t statType;
} PropModelStatsTableInit[] =
{
	{ "models/props/cs_office/computer_caseb.mdl", CSSTAT_PROPSBROKEN_OFFICEELECTRONICS },
	{ "models/props/cs_office/computer_monitor.mdl", CSSTAT_PROPSBROKEN_OFFICEELECTRONICS },
	{ "models/props/cs_office/phone.mdl", CSSTAT_PROPSBROKEN_OFFICEELECTRONICS },
	{ "models/props/cs_office/projector.mdl", CSSTAT_PROPSBROKEN_OFFICEELECTRONICS },
	{ "models/props/cs_office/TV_plasma.mdl", CSSTAT_PROPSBROKEN_OFFICEELECTRONICS },
	{ "models/props/cs_office/computer_keyboard.mdl", CSSTAT_PROPSBROKEN_OFFICEELECTRONICS },
	{ "models/props/cs_office/radio.mdl", CSSTAT_PROPSBROKEN_OFFICERADIO },
	{ "models/props/cs_office/trash_can.mdl", CSSTAT_PROPSBROKEN_OFFICEJUNK },
	{ "models/props/cs_office/file_box.mdl", CSSTAT_PROPSBROKEN_OFFICEJUNK },
	{ "models/props_junk/watermelon01.mdl", CSSTAT_PROPSBROKEN_ITALY_MELON },
	// 	models/props/de_inferno/claypot01.mdl
	// 	models/props/de_inferno/claypot02.mdl
	//	models/props/de_dust/grainbasket01c.mdl
	//	models/props_junk/wood_crate001a.mdl 
	//	models/props/cs_office/file_box_p1.mdl 
};


struct
{
	int achievementId;
	int statId;
	int roundRequirement;
	int matchRequirement;
	char* mapFilter;
	bool disallowGunGameProgressive;

	bool IsMet(int roundStat, int matchStat)
	{
		return roundStat >= roundRequirement && matchStat >= matchRequirement;
	}
} ServerStatBasedAchievements[] =
{
	{ CSBreakWindows,			    CSSTAT_NUM_BROKEN_WINDOWS,		AchievementConsts::BreakWindowsInOfficeRound_Windows,	0,	"cs_office", false },
	//{ CSBreakProps,			        CSSTAT_PROPSBROKEN_ALL,			AchievementConsts::BreakPropsInRound_Props,				0,	NULL },
	{ CSUnstoppableForce,		    CSSTAT_KILLS,					AchievementConsts::UnstoppableForce_Kills,				0,	NULL, true },
	{ CSHeadshotsInRound,	        CSSTAT_KILLS_HEADSHOT,			AchievementConsts::HeadshotsInRound_Kills,				0,	NULL, true },
	{ CSDominationOverkillsMatch,	CSSTAT_DOMINATION_OVERKILLS,	0,				                                        10,	NULL, false },
};

// The struct below should be updated (along with the CSBombEventName enum table) whenever we write data for a new bomb-related event.
struct BombEventNameInfo
{
	CSBombEventName eventID;
	const char *	name;
};
BombEventNameInfo s_BombEventNameInfo[]=
{
	{	BOMB_EVENT_NAME_PLANTED, "bomb_planted" },
	{   BOMB_EVENT_NAME_DEFUSED, "bomb_defused"	},
};

CSBombEventName BombEventNameFromString( const char* pEventName )
{
	for ( int i = 0; i < ARRAYSIZE( s_BombEventNameInfo ); ++i )
	{
		if ( !V_stricmp( s_BombEventNameInfo[i].name, pEventName ) )
		{
			return s_BombEventNameInfo[i].eventID;
		}
	}
	return BOMB_EVENT_NAME_NONE;
}


#if !defined( NO_STEAM )
uint64 GetPlayerSessionID( CCSPlayer *pPlayer )
{
	// Steam account id's for bots based on difficulty
	static int32 s_botIDs[ NUM_DIFFICULTY_LEVELS ] =
	{
		551,
		552,
		553,
		554,
	};

	// if we're controlling a bot, return the bot ID
	if( pPlayer->IsControllingBot() )
	{
		CCSPlayer* controlledPlayer = pPlayer->GetControlledBot();
		AssertMsg( controlledPlayer != pPlayer, "Player should never match controlled player: this will cause an infinite loop" );
		if( controlledPlayer )
		{
			return GetPlayerSessionID( controlledPlayer );
		}
	}

	if ( pPlayer->IsBot() )
	{
		CCSBot* pBot = dynamic_cast< CCSBot* >( pPlayer );
		if ( pBot )
		{
			const BotProfile* pProfile = pBot->GetProfile();
			if ( pProfile )
			{
				for ( int i = NUM_DIFFICULTY_LEVELS - 1; i >= BOT_EASY; --i )
				{
					if ( pProfile->IsDifficulty( ( BotDifficultyType ) i ) )
					{
						return s_botIDs[ i ];
					}
				}
			}
		}
	}

	char const *szCvarValue = engine->GetClientConVarValue( pPlayer->entindex(), "steamworks_sessionid_client" );
	return szCvarValue ? V_atoui64( szCvarValue ) : 0;
}
#endif // !NO_STEAM

// [Forrest] Allow nemesis/revenge to be turned off for a server
static void SvNoNemesisChangeCallback( IConVar *pConVar, const char *pOldValue, float flOldValue )
{
	ConVarRef var( pConVar );
	if ( var.IsValid() && var.GetBool() )
	{
		// Clear all nemesis relationships.
		for ( int i = 1 ; i <= gpGlobals->maxClients ; i++ )
		{
			CCSPlayer *pTemp = ToCSPlayer( UTIL_PlayerByIndex( i ) );
			if ( pTemp )
			{
				pTemp->RemoveNemesisRelationships();
			}
		}
	}
}

ConVar sv_nonemesis( "sv_nonemesis", "0", 0, "Disable nemesis and revenge.", SvNoNemesisChangeCallback );
ConVar sv_debugroundstats( "sv_debugroundstats", "0", FCVAR_DEVELOPMENTONLY );

static void FNCallback_Record_Item_Time_Data( IConVar *var, const char *pOldValue, float flOldValue )
{
	ConVarRef ref( var );

	if ( flOldValue != ref.GetFloat() )
	{
		CCS_GameStats.m_ItemTimeData.PrintItemTimeData( ItemTimeData::ACTIVE_TIME );
		CCS_GameStats.m_ItemTimeData.PrintItemTimeData( ItemTimeData::TOTAL_TIME );
		CCS_GameStats.m_ItemTimeData.ClearItemTimeData();

		if ( ref.GetFloat() == 1.0f )
		{
			CCS_GameStats.m_ItemTimeData.PopulateItemTimeData();
		}
	}
}

ConVar sv_record_item_time_data( "sv_record_item_time_data", "0", FCVAR_RELEASE, "Turn on recording of per player item time data into the server log.", FNCallback_Record_Item_Time_Data );

int GetCSLevelIndex( const char *pLevelName )
{
	for ( int i = 0; MapName_StatId_Table[i].statWinsId != CSSTAT_UNDEFINED; i ++ )
	{
		if ( Q_strcmp( pLevelName, MapName_StatId_Table[i].szMapName ) == 0 )
			return i;
	}

	return -1;
}


CCSGameStats CCS_GameStats;
#if !defined( _GAMECONSOLE ) && !defined( NO_STEAM )
CCSGameStats::StatContainerList_t* CCSGameStats::s_StatLists = new CCSGameStats::StatContainerList_t();
#endif

//-----------------------------------------------------------------------------
// Purpose: Constructor
// Input  :  - 
//-----------------------------------------------------------------------------
CCSGameStats::CCSGameStats()
{
	gamestats = this;
	Clear();
	m_fDisseminationTimerLow = m_fDisseminationTimerHigh = 0.0f;

	// create table for mapping prop models to stats
	for ( int i = 0; i < ARRAYSIZE(PropModelStatsTableInit); ++i)
	{
		m_PropStatTable.Insert(PropModelStatsTableInit[i].szPropModelName, PropModelStatsTableInit[i].statType);
	}

	m_numberOfRoundsForDirectAverages = 0;
	m_numberOfTerroristEntriesForDirectAverages = 0;
	m_numberOfCounterTerroristEntriesForDirectAverages = 0;

}

//-----------------------------------------------------------------------------
// Purpose: Destructor
// Input  :  - 
//-----------------------------------------------------------------------------
CCSGameStats::~CCSGameStats()
{
	Clear();
}

//-----------------------------------------------------------------------------
// Purpose: Clear out game stats
// Input  :  - 
//-----------------------------------------------------------------------------
void CCSGameStats::Clear( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCSGameStats::Init( void )
{
	ListenForGameEvent( "round_end" );
	ListenForGameEvent( "round_officially_ended" );
	ListenForGameEvent( "break_prop" );
	ListenForGameEvent( "player_decal" );	
	ListenForGameEvent( "begin_new_match" );	
	ListenForGameEvent( "bomb_planted" );
	ListenForGameEvent( "bomb_defused" );

#if ITEM_TIME_DATA
	//ItemTimeData
	ListenForGameEvent( "item_pickup" );
	ListenForGameEvent( "item_equip" );
	ListenForGameEvent( "item_remove" );
#endif //ITEM_TIME_DATA

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGameStats::Event_ShotFired( CBasePlayer *pPlayer, CBaseCombatWeapon* pWeapon )
{
	if ( CSGameRules()->IsRoundOver() )
		return;

	Assert( pPlayer );
	CCSPlayer *pCSPlayer = ToCSPlayer( pPlayer );

	// [dwenger] adding tracking for weapon used fun fact
	if ( pCSPlayer && pWeapon )
	{
		// [dwenger] Update the player's tracking of which weapon type they fired
		pCSPlayer->PlayerUsedFirearm( pWeapon );
		if( !pWeapon->HasAnyAmmo() )
			pCSPlayer->PlayerEmptiedAmmoForFirearm( pWeapon );

		IncrementStat( pCSPlayer, CSSTAT_SHOTS_FIRED, 1 );

		CWeaponCSBase* pCSWeapon = dynamic_cast< CWeaponCSBase * >(pWeapon);

		// Increment the individual weapon
	    if( pCSWeapon )
		{
			CSWeaponID weaponId = pCSWeapon->GetCSWeaponID();

			//if ( weaponId == WEAPON_HEGRENADE )
			//	int here = 0;

			for (int i = 0; WeaponName_StatId_Table[i].weaponId != WEAPON_NONE; ++i)
			{
			    if ( WeaponName_StatId_Table[i].weaponId == weaponId && WeaponName_StatId_Table[i].shotStatId != CSSTAT_UNDEFINED )
				{
					IncrementStat( pCSPlayer, WeaponName_StatId_Table[i].shotStatId, 1 );
					break;
				}
			}
		}
	}
}

void CCSGameStats::Event_ShotHit( CBasePlayer *pPlayer, const CTakeDamageInfo &info )
{
	Assert( pPlayer );
	CCSPlayer *pCSPlayer = ToCSPlayer( pPlayer );

	if ( info.GetDamagedOtherPlayers() <=  0 )
	{
		// [dkorus] a 'hit' is a only counted for the first character hit.  Otherwise we can end up with an artificially high hit count and >100% accuracy
		IncrementStat( pCSPlayer, CSSTAT_SHOTS_HIT, 1 );
	}

	CBaseEntity *pInflictor = info.GetInflictor();

	if ( pInflictor )
	{
		if ( pInflictor == pPlayer )
		{			
			if ( pPlayer->GetActiveWeapon() )
			{
				CWeaponCSBase* pCSWeapon = dynamic_cast< CWeaponCSBase * >(pPlayer->GetActiveWeapon());
				if (pCSWeapon)
				{
					CSWeaponID weaponId = pCSWeapon->GetCSWeaponID();
					for (int i = 0; WeaponName_StatId_Table[i].weaponId != WEAPON_NONE; ++i)
					{
						if ( WeaponName_StatId_Table[i].weaponId == weaponId && WeaponName_StatId_Table[i].shotStatId != CSSTAT_UNDEFINED )
						{
							IncrementStat( pCSPlayer, WeaponName_StatId_Table[i].hitStatId, 1 );
							IncrementStat( pCSPlayer, WeaponName_StatId_Table[i].damageStatId, info.GetDamage() );
							break;
						}
					}
				}
			}
		}
	}
}
void CCSGameStats::Event_PlayerKilled( CBasePlayer *pPlayer, const CTakeDamageInfo &info )
{
	Assert( pPlayer );
	CCSPlayer *pCSPlayer = ToCSPlayer( pPlayer );
	
	IncrementStat( pCSPlayer, CSSTAT_DEATHS, 1 );
}

void CCSGameStats::Event_PlayerSprayedDecal( CCSPlayer* pPlayer )
{
    IncrementStat( pPlayer, CSSTAT_DECAL_SPRAYS, 1 );
}

void CCSGameStats::Event_PlayerKilled_PreWeaponDrop( CBasePlayer *pPlayer, const CTakeDamageInfo &info )
{
	Assert( pPlayer );
	CCSPlayer *pCSPlayer = ToCSPlayer( pPlayer );
	CCSPlayer *pAttacker = ToCSPlayer( info.GetAttacker() );
	bool victimZoomed = ( pCSPlayer->GetFOV() != pCSPlayer->GetDefaultFOV() );

	if (victimZoomed)
	{
		IncrementStat(pAttacker, CSSTAT_KILLS_AGAINST_ZOOMED_SNIPER, 1);
	}

	//Check for knife fight achievements
	if (pAttacker && pCSPlayer && pAttacker == info.GetInflictor() && pAttacker->GetTeamNumber() != pCSPlayer->GetTeamNumber())
	{
		CWeaponCSBase* attackerWeapon = pAttacker->GetActiveCSWeapon();
		CWeaponCSBase* victimWeapon = pCSPlayer->GetActiveCSWeapon();

		CSWeaponID victimWeaponID = ( ( victimWeapon ) ? victimWeapon->GetCSWeaponID() : WEAPON_NONE );

		if (attackerWeapon && victimWeapon)
		{
			CSWeaponID attackerWeaponID = attackerWeapon->GetCSWeaponID(); 

			if (attackerWeaponID == WEAPON_KNIFE && victimWeaponID == WEAPON_KNIFE)
			{
				IncrementStat(pAttacker, CSSTAT_KILLS_KNIFE_FIGHT, 1);
			}

			if( CSGameRules( )->IsPlayingGunGame() )
			{
				if ( attackerWeaponID == WEAPON_KNIFE && 
					 CSGameRules( )->IsPlayingGunGameTRBomb() )
				{
					// just got a knife kill in a TR game
					if ( pCSPlayer->PlacedBombThisRound() )
						pAttacker->SetKnifeLevelKilledBombPlacer();
				}

				// Achievements for arms race: Kill gold knife level player with knife / kill gold knife level player with smg
				static CSchemaItemDefHandle hWeaponKnife( "weapon_knife" );
				static CSchemaItemDefHandle hWeaponKnifeGG( "weapon_knifegg" );
				const CEconItemDefinition* victimLevel = CSGameRules()->GetCurrentGunGameWeapon( pCSPlayer->m_iGunGameProgressiveWeaponIndex, pCSPlayer->GetTeamNumber() );
				if ( victimLevel == hWeaponKnife || victimWeaponID == hWeaponKnifeGG )
				{
					pAttacker->AwardAchievement( CSGunGameKillKnifer );
					if ( attackerWeaponID == WEAPON_KNIFE || attackerWeaponID == WEAPON_KNIFE_GG )
					{
						pAttacker->AwardAchievement( CSGunGameKnifeKillKnifer );
					}

					if ( attackerWeapon->IsKindOf( WEAPONTYPE_SUBMACHINEGUN ) )
					{
						pAttacker->AwardAchievement( CSGunGameSMGKillKnifer );
					}
				}
			}

		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGameStats::Event_BombPlanted( CCSPlayer* pPlayer)
{
    IncrementStat( pPlayer, CSSTAT_NUM_BOMBS_PLANTED, 1 );
	if( CSGameRules( )->IsPlayingGunGameTRBomb() )
	{
		IncrementStat( pPlayer, CSSTAT_TR_NUM_BOMBS_PLANTED, 1 );

	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGameStats::Event_BombDefused( CCSPlayer* pPlayer)
{

    IncrementStat( pPlayer, CSSTAT_NUM_BOMBS_DEFUSED, 1 );
	IncrementStat( pPlayer, CSSTAT_OBJECTIVES_COMPLETED, 1 );
	if( pPlayer && pPlayer->HasDefuser() )
	{
		IncrementStat( pPlayer, CSSTAT_BOMBS_DEFUSED_WITHKIT, 1 );
	}

	if( CSGameRules( )->IsPlayingGunGameTRBomb() )
	{
		IncrementStat( pPlayer, CSSTAT_TR_NUM_BOMBS_DEFUSED, 1 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Increment terrorist team stat
//-----------------------------------------------------------------------------
void CCSGameStats::Event_BombExploded( CCSPlayer* pPlayer )
{
	IncrementStat( pPlayer, CSSTAT_OBJECTIVES_COMPLETED, 1 );
}
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGameStats::Event_HostageRescued( CCSPlayer* pPlayer)
{
    IncrementStat( pPlayer, CSSTAT_NUM_HOSTAGES_RESCUED, 1 );
}

//-----------------------------------------------------------------------------
// Purpose: Increment counter-terrorist team stat
//-----------------------------------------------------------------------------
void CCSGameStats::Event_AllHostagesRescued()
{
	IncrementTeamStat( TEAM_CT, CSSTAT_OBJECTIVES_COMPLETED, 1 );
}
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGameStats::Event_WindowShattered( CBasePlayer *pPlayer)
{
	Assert( pPlayer );
	CCSPlayer *pCSPlayer = ToCSPlayer( pPlayer );

	IncrementStat( pCSPlayer, CSSTAT_NUM_BROKEN_WINDOWS, 1 );
}


void CCSGameStats::Event_BreakProp( CCSPlayer* pPlayer, CBreakableProp *pProp )
{
	if (!pPlayer)
		return;

	DevMsg("Player %s broke a %s (%i)\n", pPlayer->GetPlayerName(), pProp->GetModelName().ToCStr(), pProp->entindex());

	int iIndex = m_PropStatTable.Find(pProp->GetModelName().ToCStr());
	if (m_PropStatTable.IsValidIndex(iIndex))
	{
		IncrementStat(pPlayer, m_PropStatTable[iIndex], 1);
	}
	IncrementStat(pPlayer, CSSTAT_PROPSBROKEN_ALL, 1);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGameStats::UpdatePlayerRoundStats(int winner)
{	
	int mapIndex = GetCSLevelIndex(gpGlobals->mapname.ToCStr());
	CSStatType_t mapStatWinIndex  = CSSTAT_UNDEFINED, mapStatRoundIndex = CSSTAT_UNDEFINED;

	if ( mapIndex != -1 )
	{
		mapStatWinIndex = MapName_StatId_Table[mapIndex].statWinsId;
		mapStatRoundIndex = MapName_StatId_Table[mapIndex].statRoundsId;
	}

	// increment the team specific stats
	IncrementTeamStat( winner, CSSTAT_ROUNDS_WON, 1 );

	if( CSGameRules( )->IsPlayingGunGame() )
	{
		IncrementTeamStat( winner, CSTAT_GUNGAME_ROUNDS_WON, 1 );
		IncrementTeamStat( TEAM_TERRORIST, CSTAT_GUNGAME_ROUNDS_PLAYED, 1 );
		IncrementTeamStat( TEAM_CT, CSTAT_GUNGAME_ROUNDS_PLAYED, 1 );
	}

	if ( mapStatWinIndex != CSSTAT_UNDEFINED )
	{
		IncrementTeamStat( winner, mapStatWinIndex, 1 );
	}
	if ( CSGameRules()->IsPistolRound() )
	{
		IncrementTeamStat( winner, CSSTAT_PISTOLROUNDS_WON, 1 );
	}
	
	IncrementTeamStat( TEAM_TERRORIST, CSSTAT_ROUNDS_PLAYED, 1 );
	IncrementTeamStat( TEAM_CT, CSSTAT_ROUNDS_PLAYED, 1 );

	if( mapStatRoundIndex != CSSTAT_UNDEFINED )
	{
		IncrementTeamStat( TEAM_TERRORIST, mapStatRoundIndex, 1 );
		IncrementTeamStat( TEAM_CT, mapStatRoundIndex, 1 );
	}

	for( int iPlayerIndex = 1 ; iPlayerIndex <= MAX_PLAYERS; iPlayerIndex++ )
	{
		CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( iPlayerIndex ) );
		if ( pPlayer && pPlayer->IsConnected() )
		{
			IncrementStat( pPlayer, CSSTAT_ROUNDS_PLAYED, 1, false, true );			
			if ( CSGameRules()->IsPlayingGunGame() )
			{
				IncrementStat( pPlayer, CSTAT_GUNGAME_ROUNDS_PLAYED, 1, false, true );

			}
			if( CSGameRules()->IsPlayingGunGameProgressive() )
			{
				IncrementStat( pPlayer,CSSTAT_GG_PROGRESSIVE_CONTRIBUTION_SCORE, MAX( pPlayer->GetRoundContributionScore(), 0.0f), false, true );
			}
			else
			{
				IncrementStat( pPlayer,CSSTAT_CONTRIBUTION_SCORE, MAX( pPlayer->GetRoundContributionScore(), 0.0f), false, true );
			}
			pPlayer->ClearRoundContributionScore();
			pPlayer->ClearRoundProximityScore();

			if ( winner == TEAM_CT )
			{
				IncrementStat( pPlayer, CSSTAT_CT_ROUNDS_WON, 1, true, true );
			}
			else if ( winner == TEAM_TERRORIST )
			{
				IncrementStat( pPlayer, CSSTAT_T_ROUNDS_WON, 1, true, true );
			}


			if ( winner == TEAM_CT || winner == TEAM_TERRORIST )
			{
				// Increment the win stats if this player is on the winning team
				if ( pPlayer->GetTeamNumber() == winner )
				{
					IncrementStat( pPlayer, CSSTAT_ROUNDS_WON, 1, true, true );
					if( CSGameRules( )->IsPlayingGunGame() )
					{
						IncrementStat( pPlayer, CSTAT_GUNGAME_ROUNDS_WON, 1, true, true );
					}

					if ( CSGameRules()->IsPistolRound() )
					{
						IncrementStat( pPlayer, CSSTAT_PISTOLROUNDS_WON, 1, true, true );
					}

					if ( mapStatWinIndex != CSSTAT_UNDEFINED )
					{
						IncrementStat( pPlayer, mapStatWinIndex, 1, true, true );
					}
				}

				if ( mapStatWinIndex != CSSTAT_UNDEFINED )
				{
					IncrementStat( pPlayer, mapStatRoundIndex, 1, true, true );
				}

				// set the play time for the round
				IncrementStat( pPlayer, CSSTAT_PLAYTIME, (int)CSGameRules()->GetRoundElapsedTime(), true, true );
			}
		}
	}

	// send a stats update to all players
	for ( int iPlayerIndex = 1; iPlayerIndex <= MAX_PLAYERS; iPlayerIndex++ )
	{
		CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( iPlayerIndex ) );
		if ( pPlayer && pPlayer->IsConnected())
		{
			SendStatsToPlayer(pPlayer, CSSTAT_PRIORITY_ENDROUND);
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGameStats::Event_PlayerConnected( CBasePlayer *pPlayer )
{
	ResetPlayerStats( ToCSPlayer( pPlayer ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGameStats::Event_PlayerDisconnected( CBasePlayer *pPlayer )
{
	CCSPlayer *pCSPlayer = ToCSPlayer( pPlayer );
	if ( !pCSPlayer )
		return;

	ResetPlayerStats( pCSPlayer );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGameStats::Event_PlayerKilledOther( CBasePlayer *pAttacker, CBaseEntity *pVictim, const CTakeDamageInfo &info )
{
	// This also gets called when the victim is a building.  That gets tracked separately as building destruction, don't count it here
	if ( !pVictim->IsPlayer() )
		return;

	CCSPlayer *pPlayerAttacker = ToCSPlayer( pAttacker );
	CCSPlayer *pPlayerVictim = ToCSPlayer( pVictim );	

	// keep track of how many times every player kills every other player
	TrackKillStats( pPlayerAttacker, pPlayerVictim );

	// Skip rest of stat reporting for friendly fire
	if ( pPlayerAttacker->GetTeam() == pVictim->GetTeam() )
		return;

	CWeaponCSBase* pCSWeapon = dynamic_cast<CWeaponCSBase*>( info.GetWeapon() );

	// CSN-8452 Gungame modes were counting towards kills with enemy weapons.
	if ( CSGameRules()->IsPlayingClassic() )
	{
		if ( pCSWeapon )
		{
			if ( pCSWeapon->WasOwnedByTeam( pVictim->GetTeamNumber() ) )
			{
				// Stat is incremented if kill is made with a weapon that was once owned by the team that the killed player is on
				IncrementStat(pPlayerAttacker, CSSTAT_KILLS_ENEMY_WEAPON, 1);
			}
		}
	}

	CSWeaponMode weaponMode = Primary_Mode;
	if ( pCSWeapon )
	{
		weaponMode = pCSWeapon->m_weaponMode;
	}

	// CSN-8983 - Grenades aren't the player's weapon upon killing. Get the weapon info from the damage.
	CSWeaponID weaponId = WEAPON_NONE;
	const CEconItemView* pWeaponItemView;
	const CCSWeaponInfo* pWeaponInfoFronDamage = CCSPlayer::GetWeaponInfoFromDamageInfo(info, &pWeaponItemView);
	if ( pWeaponInfoFronDamage && pWeaponItemView )
	{
		weaponId = pWeaponInfoFronDamage->GetWeaponID( pWeaponItemView );
	}


	// update weapon stats
	for (int i = 0; WeaponName_StatId_Table[i].killStatId != CSSTAT_UNDEFINED; ++i)
	{
		if ( WeaponName_StatId_Table[i].weaponId == weaponId )
		{
			IncrementStat( pPlayerAttacker, WeaponName_StatId_Table[i].killStatId, 1 );
			break;
		}
	}

	if (pPlayerVictim && pPlayerVictim->IsBlind())
	{
		IncrementStat( pPlayerAttacker, CSSTAT_KILLS_ENEMY_BLINDED, 1 );
	}

	if (pPlayerVictim && pPlayerAttacker && pPlayerAttacker->IsBlindForAchievement())
	{
		IncrementStat( pPlayerAttacker, CSSTAT_KILLS_WHILE_BLINDED, 1 );
	}

	// [sbodenbender] check for deaths near planted bomb for funfact
	if (pPlayerVictim && pPlayerAttacker && pPlayerAttacker->GetTeamNumber() == TEAM_TERRORIST && CSGameRules()->m_bBombPlanted)
	{
		if ( pPlayerAttacker->IsCloseToActiveBomb() || pPlayerVictim->IsCloseToActiveBomb() )		
		{
			IncrementStat(pPlayerAttacker, CSSTAT_KILLS_WHILE_DEFENDING_BOMB, 1);
		}
	}

	//Increment stat if this is a headshot.
	if (info.GetDamageType() & DMG_HEADSHOT)
	{
		IncrementStat( pPlayerAttacker, CSSTAT_KILLS_HEADSHOT, 1 );
	}

	IncrementStat( pPlayerAttacker, CSSTAT_KILLS, 1 );

	// we don't have a simple way (yet) to check if the victim actually just achieved The Unstoppable Force, so we
	// award this achievement simply if they've met the requirements and would have received it.  
	PlayerStats_t &victimStats = m_aPlayerStats[pVictim->entindex()];
	if (victimStats.statsCurrentRound[CSSTAT_KILLS] >= AchievementConsts::ImmovableObject_Kills && !CSGameRules()->IsPlayingGunGameProgressive() )
	{
		pPlayerAttacker->AwardAchievement(CSImmovableObject);
	}

	CCSGameRules::TeamPlayerCounts playerCounts[TEAM_MAXCOUNT];

	CSGameRules()->GetPlayerCounts(playerCounts);
	int iAttackerTeamNumber = pPlayerAttacker->GetTeamNumber() ;
	if (playerCounts[iAttackerTeamNumber].totalAlivePlayers == 1 && playerCounts[iAttackerTeamNumber].killedPlayers >= 2)
	{
		IncrementStat(pPlayerAttacker, CSSTAT_KILLS_WHILE_LAST_PLAYER_ALIVE, 1);
	}	

	//if they were damaged by more than one person that must mean that someone else did damage before the killer finished them off.
	if (pPlayerVictim->GetNumEnemyDamagers() > 1)
	{
		IncrementStat(pPlayerAttacker, CSSTAT_KILLS_ENEMY_WOUNDED, 1);
	}	

	// set the number of consecutive kills this scorer has on the victim:
	int nConsecutiveKills = pPlayerAttacker ? MAX( FindPlayerStats( pPlayerVictim ).statsKills.iNumKilledByUnanswered[pPlayerAttacker->entindex()], 1 ) : 0;
	pPlayerVictim->SetLastConcurrentKilled( MIN( nConsecutiveKills, 8 ) );

	// check to see if a player killed another with a StatTrak weapon
	if ( CBaseCombatWeapon *pWeapon = dynamic_cast<CBaseCombatWeapon *>( info.GetWeapon() ) )
	{
		// Does the player own this item?
		CSteamID HolderSteamID;
		pAttacker->GetSteamID( &HolderSteamID );
		if ( CEconItemView *pItemView = pWeapon->GetEconItemView() )
		{
			// Get the supported killeater types on this weapon
			CUtlSortVector<uint32> killEaterTypes;
			pItemView->GetKillEaterTypes( killEaterTypes );

			if ( killEaterTypes.Count() > 0 )
			{
				if ( pItemView->GetAccountID() == HolderSteamID.GetAccountID() )
				{
					// Here we could differentiate on StatTrak types (normal, headshot, etc.):
					IncrementStat( pPlayerAttacker, CSSTAT_KILLS_WITH_STATTRAK_WEAPON, 1 );
				}
			}
		}
		
	}
}


void CCSGameStats::CalculateOverkill(CCSPlayer* pAttacker, CCSPlayer* pVictim)
{
	//Count domination overkills - Do this before determining domination
	if (pAttacker->GetTeam() != pVictim->GetTeam())
	{
		if (pAttacker->IsPlayerDominated(pVictim->entindex()))
		{
			IncrementStat( pAttacker, CSSTAT_DOMINATION_OVERKILLS, 1 );
		}
	}
}
	
//-----------------------------------------------------------------------------
// Purpose: Stats event for giving damage to player
//-----------------------------------------------------------------------------
void CCSGameStats::Event_PlayerDamage( CBasePlayer *pBasePlayer, const CTakeDamageInfo &info )
{
	CCSPlayer *pAttacker = ToCSPlayer( info.GetAttacker() );
	if ( pAttacker && pAttacker->GetTeam() != pBasePlayer->GetTeam() )
	{
		IncrementStat( pAttacker, CSSTAT_DAMAGE, info.GetDamage() );
	}

	// OGS stats

	// See if this is a bullet from a weapon that fires multiple (shotgun)
#if !defined( _GAMECONSOLE ) && !defined( NO_STEAM )

	if ( info.GetBulletID() != 0 )
	{
		uint8 iSubBullet = 0;			
		SWeaponHitData *lastHitData = m_WeaponHitData.Count() ? m_WeaponHitData.Tail() : NULL;

		// If the previous weapon shot data has the save bulletid, then we check the sub bullet id and increment one from that
		if ( lastHitData && lastHitData->m_uiBulletID == CCSPlayer::GetBulletGroup() )
		{
			iSubBullet = lastHitData->m_uiSubBulletID + 1;
		}

		m_WeaponHitData.AddToTail( new SWeaponHitData( ToCSPlayer( pBasePlayer ), info, iSubBullet, CSGameRules()->m_iTotalRoundsPlayed, info.GetRecoilIndex() ) );
	}
#endif	
}

//-----------------------------------------------------------------------------
// Purpose: Stats event for giving money to player
//-----------------------------------------------------------------------------
void CCSGameStats::Event_MoneyEarned( CCSPlayer* pPlayer, int moneyEarned)
{
	if ( pPlayer && moneyEarned > 0)
	{
		IncrementStat(pPlayer, CSSTAT_MONEY_EARNED, moneyEarned);
	}
}

void CCSGameStats::Event_MoneySpent( CCSPlayer* pPlayer, int moneySpent, const char *pItemName )
{
	if ( pPlayer && moneySpent > 0)
	{
		IncrementStat(pPlayer, CSSTAT_MONEY_SPENT, moneySpent);
#if !defined( _GAMECONSOLE ) && !defined( NO_STEAM )
		if ( pItemName && !pPlayer->IsBot() )
		{
			m_MarketPurchases.AddToTail( new SMarketPurchases( GetPlayerSessionID( pPlayer ), moneySpent, pItemName, CSGameRules()->m_iTotalRoundsPlayed ) );
		}
#endif
	}
}

void CCSGameStats::Event_PlayerDonatedWeapon (CCSPlayer* pPlayer)
{
	if (pPlayer)
	{
		IncrementStat(pPlayer, CSSTAT_WEAPONS_DONATED, 1);
	}
}

void CCSGameStats::Event_MVPEarned( CCSPlayer* pPlayer )
{
	if (pPlayer)
	{
		IncrementStat(pPlayer, CSSTAT_MVPS, 1);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Event handler
//-----------------------------------------------------------------------------
void CCSGameStats::FireGameEvent( IGameEvent *event )
{
	const char *pEventName = event->GetName();

#if ITEM_TIME_DATA
	uint64 nSteamID = 0;
	CBasePlayer *pPlayer = UTIL_PlayerByUserId( event->GetInt( "userid", 0 ) );
	CCSPlayer *pCSPlayer = ToCSPlayer( pPlayer );

	if ( pCSPlayer )
	{
		nSteamID = pCSPlayer->GetSteamIDAsUInt64();
	}
#endif //ITEM_TIME_DATA	

	if ( V_strcmp(pEventName, "round_end") == 0 )
	{
		const int reason = event->GetInt( "reason" );

		if( reason == Game_Commencing )
		{
			ResetPlayerClassMatchStats();
		}
		else
		{
			UpdatePlayerRoundStats(event->GetInt("winner"));
		}
	}
	else if ( V_strcmp( pEventName, "round_officially_ended" ) == 0 )
	{
#if !defined( _GAMECONSOLE )
		// Upload round stats here to avoid end-of-round visual hitch
		UploadRoundStats();
#endif
	}
	else if ( V_strcmp(pEventName, "break_prop") == 0 )
	{
		int userid = event->GetInt("userid", 0);
		int entindex = event->GetInt("entindex", 0);
		CBreakableProp* pProp = static_cast<CBreakableProp*>(CBaseEntity::Instance(entindex));
 		Event_BreakProp(ToCSPlayer(UTIL_PlayerByUserId(userid)), pProp);
	}
	else if ( V_strcmp(pEventName, "player_decal") == 0 )
	{
		int userid = event->GetInt("userid", 0);
		Event_PlayerSprayedDecal(ToCSPlayer(UTIL_PlayerByUserId(userid)));
	}	
	else if ( V_strcmp(pEventName, "begin_new_match") == 0 )
	{
		CreateNewGameStatsSession();
	}
	else if ( V_strcmp(pEventName, "bomb_planted") == 0 || V_strcmp(pEventName, "bomb_defused") == 0 )
	{
		//Generate a special weapon hit entry for each alive human player
		
		CPlantedC4* pPlantedC4 = g_PlantedC4s[0];	
		uint8 unBombsite = event->GetInt("site",0); //Get Bombsite
		CSBombEventName nBombEventName = BombEventNameFromString(pEventName);
		
		for( int iPlayerIndex = 1 ; iPlayerIndex <= MAX_PLAYERS; iPlayerIndex++ )
		{
			if (CCSPlayer *pAffectedCSPlayer = ToCSPlayer( UTIL_PlayerByIndex( iPlayerIndex ) ) )
			{
				if ( pAffectedCSPlayer->IsConnected() && pAffectedCSPlayer->IsAlive() && !pAffectedCSPlayer->IsBot() )
				{
					CCSPlayer::StartNewBulletGroup();
					SWeaponHitData *pHitData = new SWeaponHitData;
					if ( pHitData->InitAsBombEvent( pAffectedCSPlayer, pPlantedC4, CCSPlayer::GetBulletGroup(), unBombsite, nBombEventName ) )
					{
						CCS_GameStats.RecordWeaponHit( pHitData ); // submission deletes the struct.
					}
					else
					{
						delete pHitData;
					}
				}				
			}
		}					 	
	}

#if ITEM_TIME_DATA
	else if ( V_strcmp( pEventName, "item_pickup" ) == 0 )
	{
		item_definition_index_t defindex = event->GetInt( "defindex" );

		if ( defindex )
		{
			CCS_GameStats.m_ItemTimeData.OnItemPickup( nSteamID, defindex );
		}
	}
	else if ( V_strcmp( pEventName, "item_remove" ) == 0 )
	{
		item_definition_index_t defindex = event->GetInt( "defindex" );

		if ( defindex )
		{
			CCS_GameStats.m_ItemTimeData.OnItemRemove( nSteamID, defindex );
		}

//		Msg( "\n-----------REMOVED %s from %s\n\n", event->GetString( "item" ), pCSPlayer->GetPlayerName() );
	}
	else if ( V_strcmp( pEventName, "item_equip" ) == 0 )
	{
		item_definition_index_t defindex = event->GetInt( "defindex" );

		if ( defindex )
		{
			CCS_GameStats.m_ItemTimeData.OnItemEquip( nSteamID, defindex );
		}
	}
#endif //ITEM_TIME_DATA
	
}

//-----------------------------------------------------------------------------
// Purpose: Return stats for the given player
//-----------------------------------------------------------------------------
const PlayerStats_t& CCSGameStats::FindPlayerStats( CBasePlayer *pPlayer ) const
{
	return m_aPlayerStats[pPlayer->entindex()];
}

//-----------------------------------------------------------------------------
// Purpose: Return stats for the given team
//-----------------------------------------------------------------------------
const StatsCollection_t& CCSGameStats::GetTeamStats( int iTeamIndex ) const
{
	int arrayIndex = iTeamIndex - FIRST_GAME_TEAM;
	Assert( arrayIndex >= 0 && arrayIndex < TEAM_MAXCOUNT - FIRST_GAME_TEAM );
	return m_aTeamStats[arrayIndex];
}

//-----------------------------------------------------------------------------
// Purpose: Resets the stats for each team
//-----------------------------------------------------------------------------
void CCSGameStats::ResetAllTeamStats()
{
	for ( int i = 0; i < ARRAYSIZE(m_aTeamStats); ++i )
	{
		m_aTeamStats[i].Reset();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Resets all stats (including round, match, accumulated and rolling averages
//-----------------------------------------------------------------------------
void CCSGameStats::ResetAllStats()
{
	for ( int i = 0; i < ARRAYSIZE( m_aPlayerStats ); i++ )
	{		
		m_aPlayerStats[i].statsDelta.Reset();
		m_aPlayerStats[i].statsCurrentRound.Reset();
		m_aPlayerStats[i].statsCurrentMatch.Reset();
 		m_aPlayerStats[i].statsKills.Reset();
		m_numberOfRoundsForDirectAverages = 0;
		m_numberOfTerroristEntriesForDirectAverages = 0;
		m_numberOfCounterTerroristEntriesForDirectAverages = 0;
	}

	ClearOGSRoundStats();
}


void CCSGameStats::IncrementTeamStat( int iTeamIndex, int iStatIndex, int iAmount )
{
	int arrayIndex = iTeamIndex - TEAM_TERRORIST;
	Assert( iStatIndex >= 0 && iStatIndex < CSSTAT_MAX );
	if( arrayIndex >= 0 && arrayIndex < TEAM_MAXCOUNT - TEAM_TERRORIST )
	{
		m_aTeamStats[arrayIndex][iStatIndex] += iAmount;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Resets all stats for this player
//-----------------------------------------------------------------------------
void CCSGameStats::ResetPlayerStats( CBasePlayer* pPlayer )
{
	PlayerStats_t &stats = m_aPlayerStats[pPlayer->entindex()];
	// reset the stats on this player
	stats.Reset();
	// reset the matrix of who killed whom with respect to this player
	ResetKillHistory( pPlayer );
}

//-----------------------------------------------------------------------------
// Purpose: Resets the kill history for this player
//-----------------------------------------------------------------------------
void CCSGameStats::ResetKillHistory( CBasePlayer* pPlayer )
{
	int iPlayerIndex = pPlayer->entindex();

	PlayerStats_t& statsPlayer = m_aPlayerStats[iPlayerIndex];

	// for every other player, set all all the kills with respect to this player to 0
	for ( int i = 0; i < ARRAYSIZE( m_aPlayerStats ); i++ )
	{
		//reset their record of us.
		PlayerStats_t &statsOther = m_aPlayerStats[i];
		statsOther.statsKills.iNumKilled[iPlayerIndex] = 0;
		statsOther.statsKills.iNumKilledBy[iPlayerIndex] = 0;
		statsOther.statsKills.iNumKilledByUnanswered[iPlayerIndex] = 0;

		//reset our record of them
		statsPlayer.statsKills.iNumKilled[i] = 0;
		statsPlayer.statsKills.iNumKilledBy[i] = 0;
		statsPlayer.statsKills.iNumKilledByUnanswered[i] = 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Resets per-round stats for all players
//-----------------------------------------------------------------------------
void CCSGameStats::ResetRoundStats()
{
	for ( int i = 0; i < ARRAYSIZE( m_aPlayerStats ); i++ )
	{		
		m_aPlayerStats[i].statsCurrentRound.Reset();
	}

	ClearOGSRoundStats();
}

//-----------------------------------------------------------------------------
// Purpose: Reset round stats
//-----------------------------------------------------------------------------
void CCSGameStats::ClearOGSRoundStats()
{
#if !defined( _GAMECONSOLE ) && !defined( NO_STEAM )
	m_WeaponHitData.PurgeAndDeleteElements();
	m_MarketPurchases.PurgeAndDeleteElements();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Increments specified stat for specified player by specified amount
//-----------------------------------------------------------------------------
void CCSGameStats::IncrementStat( CCSPlayer* pPlayer, CSStatType_t statId, int iDelta, bool bPlayerOnly /* = false */, bool bIncludeBotController /* = false */ )
{
	// note: don't track stats for players after they switch teams 
	if ( pPlayer && !pPlayer->m_bTeamChanged) 
	{
		// if we're controlling a bot, credit the BOT with our stats
		if( pPlayer->IsControllingBot() )
		{
			CCSPlayer* controlledPlayer = pPlayer->GetControlledBot();

			AssertMsg( controlledPlayer != pPlayer, "Player should never match controlled player: this will cause an infinite loop" );
			if( controlledPlayer )
			{
				IncrementStat( controlledPlayer, statId,iDelta,bPlayerOnly );
			}

			if ( !bIncludeBotController )
				return;
		}

		PlayerStats_t &stats = m_aPlayerStats[pPlayer->entindex()];
	    stats.statsDelta[statId] += iDelta;
	    stats.statsCurrentRound[statId] += iDelta;
	    stats.statsCurrentMatch[statId] += iDelta;

		// increment team stat
		int teamIndex = pPlayer->GetTeamNumber() - FIRST_GAME_TEAM;
		if ( !bPlayerOnly && teamIndex >= 0 && teamIndex < ARRAYSIZE(m_aTeamStats) )
		{
			m_aTeamStats[teamIndex][statId] += iDelta;
		}

		for (int i = 0; i < ARRAYSIZE(ServerStatBasedAchievements); ++i)
		{
			if (ServerStatBasedAchievements[i].statId == statId)
			{
				// skip this if there is a map filter and it doesn't match
				if (ServerStatBasedAchievements[i].mapFilter != NULL && V_strcmp(gpGlobals->mapname.ToCStr(), ServerStatBasedAchievements[i].mapFilter) != 0)
					continue;

				if ( CSGameRules()->IsPlayingGunGameProgressive() && ServerStatBasedAchievements[i].disallowGunGameProgressive )
					continue;

			    bool bWasMet = ServerStatBasedAchievements[i].IsMet(stats.statsCurrentRound[statId] - iDelta, stats.statsCurrentMatch[statId] - iDelta);
			    bool bIsMet = ServerStatBasedAchievements[i].IsMet(stats.statsCurrentRound[statId], stats.statsCurrentMatch[statId]);
				if (!bWasMet && bIsMet)
				{
 					pPlayer->AwardAchievement(ServerStatBasedAchievements[i].achievementId);
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:  Sets the specified stat for specified player to the specified amount
//-----------------------------------------------------------------------------
void CCSGameStats::SetStat( CCSPlayer *pPlayer, CSStatType_t statId, int iValue )
{
	if (pPlayer)
	{
		int oldRoundValue, oldMatchValue;
		PlayerStats_t &stats = m_aPlayerStats[pPlayer->entindex()];

		oldRoundValue = stats.statsCurrentRound[statId];
		oldMatchValue = stats.statsCurrentMatch[statId];

		stats.statsDelta[statId] = iValue;
		stats.statsCurrentRound[statId] = iValue;
		stats.statsCurrentMatch[statId] = iValue;

		for (int i = 0; i < ARRAYSIZE(ServerStatBasedAchievements); ++i)
		{
			if (ServerStatBasedAchievements[i].statId == statId)
			{
				// skip this if there is a map filter and it doesn't match
				if (ServerStatBasedAchievements[i].mapFilter != NULL && V_strcmp(gpGlobals->mapname.ToCStr(), ServerStatBasedAchievements[i].mapFilter) != 0)
					continue;

				bool bWasMet = ServerStatBasedAchievements[i].IsMet(oldRoundValue, oldMatchValue);
				bool bIsMet = ServerStatBasedAchievements[i].IsMet(stats.statsCurrentRound[statId], stats.statsCurrentMatch[statId]);
				if (!bWasMet && bIsMet)
				{
					pPlayer->AwardAchievement(ServerStatBasedAchievements[i].achievementId);
				}
			}
		}
	}
}


void CRC32Helper_ProcessInt16( CRC32_t &crc, int16 n )
{
	int16 plat_n = LittleShort( n );
	CRC32_ProcessBuffer( &crc, &plat_n, sizeof(plat_n) );
}


void CRC32Helper_ProcessInt32( CRC32_t &crc, int32 n )
{
	int32 plat_n = LittleDWord( n );
	CRC32_ProcessBuffer( &crc, &plat_n, sizeof(plat_n) );
}


void CRC32Helper_ProcessUInt32( CRC32_t &crc, uint32 n )
{
	uint32 plat_n = LittleDWord( n );
	CRC32_ProcessBuffer( &crc, &plat_n, sizeof(plat_n) );
}


void CCSGameStats::SendStatsToPlayer( CCSPlayer * pPlayer, int iMinStatPriority )
{
	ASSERT(CSSTAT_MAX < 0xFFFF); // if we add more than 2^16 stats, we'll need to update this protocol
	if ( pPlayer && pPlayer->IsConnected())
	{				
		StatsCollection_t &deltaStats = m_aPlayerStats[pPlayer->entindex()].statsDelta;

		// check to see if we have any stats to actually send
		short  iStatsToSend = 0;
		for ( int iStat = CSSTAT_FIRST; iStat < CSSTAT_MAX; ++iStat )
		{
			int iPriority = CSStatProperty_Table[iStat].flags & CSSTAT_PRIORITY_MASK;
			if (deltaStats[iStat] != 0 && iPriority >= iMinStatPriority)
			{
#if defined ( DEBUG_STAT_TRANSMISSION )
				Msg( "Sending Stat '%s' to client. Value: %d\n", CSStatProperty_Table[iStat].szSteamName, deltaStats[iStat] );
#endif
				++iStatsToSend;
			}
		}

		// nothing changed - bail out
		if ( !iStatsToSend )
			return;

		CSingleUserRecipientFilter filter( pPlayer );
		filter.MakeReliable();

		CCSUsrMsg_PlayerStatsUpdate msg;
		
		CRC32_t crc;
		CRC32_Init( &crc );

		// begin the CRC with a trivially hidden key value to discourage packet modification
		const uint32 key = 0x82DA9F4C;	// this key should match the key in cs_client_gamestats.cpp
		CRC32Helper_ProcessUInt32( crc, key );

		// if we make any change to the ordering of the stats or this message format, update this value
		const byte version = 0x03;
		CRC32_ProcessBuffer( &crc, &version, sizeof(version));
		msg.set_version(version);

		CRC32Helper_ProcessInt16( crc, iStatsToSend );

		for ( short iStat = CSSTAT_FIRST; iStat < CSSTAT_MAX; ++iStat )
		{
			int iPriority = CSStatProperty_Table[iStat].flags & CSSTAT_PRIORITY_MASK;
			if (deltaStats[iStat] != 0 && iPriority >= iMinStatPriority)
			{
				CCSUsrMsg_PlayerStatsUpdate::Stat *pStat = msg.add_stats();

				CRC32Helper_ProcessInt16( crc, iStat );
				pStat->set_idx(iStat);

				Assert(deltaStats[iStat] <= 0x7FFF && deltaStats[iStat] > 0);	// make sure we aren't truncating bits

				short delta = deltaStats[iStat];
				CRC32Helper_ProcessInt16( crc, delta );
				pStat->set_delta( deltaStats[iStat]);

				deltaStats[iStat] = 0;
				--iStatsToSend;
			}
		}

		Assert(iStatsToSend == 0);

		int userID = pPlayer->GetUserID();
		msg.set_user_id( userID );
		CRC32Helper_ProcessInt32( crc, userID );

		CRC32_Final( &crc );
		msg.set_crc(crc);

		SendUserMessage( filter, CS_UM_PlayerStatsUpdate, msg );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sends intermittent stats updates for stats that need to be updated during a round and/or life
//-----------------------------------------------------------------------------
void CCSGameStats::PreClientUpdate()
{
	int iMinStatPriority = -1;
	m_fDisseminationTimerHigh += gpGlobals->frametime;
	m_fDisseminationTimerLow += gpGlobals->frametime;

	if ( m_fDisseminationTimerHigh > cDisseminationTimeHigh)
	{
		iMinStatPriority = CSSTAT_PRIORITY_HIGH;
		m_fDisseminationTimerHigh = 0.0f;

		if ( m_fDisseminationTimerLow > cDisseminationTimeLow)
		{
			iMinStatPriority = CSSTAT_PRIORITY_LOW;
			m_fDisseminationTimerLow = 0.0f;
		}
	}
	else
		return;

	//The proper time has elapsed, now send the update to every player
	for ( int iPlayerIndex = 1 ; iPlayerIndex <= MAX_PLAYERS; iPlayerIndex++ )
	{
		CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex(iPlayerIndex) );
		SendStatsToPlayer(pPlayer, iMinStatPriority);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Updates the stats of who has killed whom
//-----------------------------------------------------------------------------
void CCSGameStats::TrackKillStats( CCSPlayer *pAttacker, CCSPlayer *pVictim )
{
	int iPlayerIndexAttacker = pAttacker->entindex();
	int iPlayerIndexVictim = pVictim->entindex();

	PlayerStats_t &statsAttacker = m_aPlayerStats[iPlayerIndexAttacker];
	PlayerStats_t &statsVictim = m_aPlayerStats[iPlayerIndexVictim];

	if( !pVictim->IsControllingBot() )
	{
		statsVictim.statsKills.iNumKilledBy[iPlayerIndexAttacker]++;
		statsVictim.statsKills.iNumKilledByUnanswered[iPlayerIndexAttacker]++;
	}

	if( !pAttacker->IsControllingBot() )
	{
		statsAttacker.statsKills.iNumKilled[iPlayerIndexVictim]++;
		statsAttacker.statsKills.iNumKilledByUnanswered[iPlayerIndexVictim] = 0;    
	}
}


//-----------------------------------------------------------------------------
// Purpose: Determines if attacker and victim have gotten domination or revenge
//-----------------------------------------------------------------------------
void CCSGameStats::CalcDominationAndRevenge( CCSPlayer *pAttacker, CCSPlayer *pVictim, int *piDeathFlags )
{
	// [Forrest] Allow nemesis/revenge to be turned off for a server
	if ( sv_nonemesis.GetBool() )
	{
		return;
	}

	// if we aren't playing gungame, we dont do domination or revenge
	if ( !CSGameRules()->IsPlayingGunGame() )
		return;

	//If there is no attacker, there is no domination or revenge
	if( !pAttacker || !pVictim )
	{
		return;
	}
	if (pAttacker->GetTeam() == pVictim->GetTeam())
	{
		return;
	}
	int iPlayerIndexVictim = pVictim->entindex();
	PlayerStats_t &statsVictim = m_aPlayerStats[iPlayerIndexVictim];	
	// calculate # of unanswered kills between killer & victim
	// This is plus 1 as this function gets called before the stat is updated.  That is done so that the domination
	// and revenge will be calculated prior to the death message being sent to the clients
	int attackerEntityIndex = pAttacker->entindex();
	int iKillsUnanswered = statsVictim.statsKills.iNumKilledByUnanswered[attackerEntityIndex] + 1;	

	if ( CS_KILLS_FOR_DOMINATION == iKillsUnanswered )
	{
		// this is the Nth unanswered kill between killer and victim, killer is now dominating victim
		*piDeathFlags |= ( CS_DEATH_DOMINATION );
	}
	else if ( pVictim->IsPlayerDominated( pAttacker->entindex() ) && !pAttacker->IsControllingBot() )
	{
		// the killer killed someone who was dominating him, gains revenge
		*piDeathFlags |= ( CS_DEATH_REVENGE );
	}

	//Check the overkill on 1 player achievement
	if (!pAttacker->IsControllingBot() && iKillsUnanswered == CS_KILLS_FOR_DOMINATION + AchievementConsts::ExtendedDomination_AdditionalKills)
	{
		pAttacker->AwardAchievement(CSExtendedDomination);
	}

	if (!pAttacker->IsControllingBot() && iKillsUnanswered == CS_KILLS_FOR_DOMINATION)
	{			
		//this is the Nth unanswered kill between killer and victim, killer is now dominating victim        
		//set victim to be dominated by killer
		pAttacker->SetPlayerDominated( pVictim, true );

		//Check concurrent dominations achievement
		int numConcurrentDominations = 0;
		for ( int i = 1 ; i <= gpGlobals->maxClients ; i++ )
		{
			CCSPlayer *pPlayer= ToCSPlayer( UTIL_PlayerByIndex( i ) );
			if (pPlayer && pAttacker->IsPlayerDominated(pPlayer->entindex()))
			{
				numConcurrentDominations++;
			}
		}
		if (numConcurrentDominations >= AchievementConsts::ConcurrentDominations_MinDominations)
		{
			pAttacker->AwardAchievement(CSConcurrentDominations);
		}

		
		// record stats
		Event_PlayerDominatedOther( pAttacker, pVictim );
	}
	else if ( pVictim->IsPlayerDominated( pAttacker->entindex() ) && !pAttacker->IsControllingBot() )
	{
		// the killer killed someone who was dominating him, gains revenge        
		// set victim to no longer be dominating the killer

		pVictim->SetPlayerDominated( pAttacker, false );
		// record stats
		Event_PlayerRevenge( pAttacker );
	}
}

void CCSGameStats::Event_PlayerDominatedOther( CCSPlayer *pAttacker, CCSPlayer* pVictim )
{ 
	IncrementStat( pAttacker, CSSTAT_DOMINATIONS, 1 );
}

void CCSGameStats::Event_PlayerRevenge( CCSPlayer *pAttacker )
{
	IncrementStat( pAttacker, CSSTAT_REVENGES, 1 );
}

void CCSGameStats::Event_PlayerAvengedTeammate( CCSPlayer* pAttacker, CCSPlayer* pAvengedPlayer )
{
	if (pAttacker && pAvengedPlayer)
	{
		IGameEvent *event = gameeventmanager->CreateEvent( "player_avenged_teammate" );

		if ( event )
		{
			event->SetInt( "avenger_id", pAttacker->GetUserID() );
			event->SetInt( "avenged_player_id", pAvengedPlayer->GetUserID() );
			gameeventmanager->FireEvent( event );
		}
	}
}

void CCSGameStats::Event_LevelInit()
{
	ResetAllTeamStats();
	CBaseGameStats::Event_LevelInit();
}

void CCSGameStats::Event_LevelShutdown( float fElapsed )
{
	CBaseGameStats::Event_LevelShutdown(fElapsed);

#if !defined( _GAMECONSOLE )
	GetSteamWorksGameStatsServer().EndSession();
#endif
}

// Reset any per match info that resides in the player class
void CCSGameStats::ResetPlayerClassMatchStats()
{
	for ( int i = 1; i <= MAX_PLAYERS; i++ )
	{
		CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );

		if ( pPlayer )
		{
			pPlayer->SetNumMVPs( 0 );
		}
	}
}

#if !defined( _GAMECONSOLE )


extern double g_rowCommitTime;
extern double g_rowWriteTime;
//-----------------------------------------------------------------------------
// Purpose: Submits all round specific data to the OGS system
//-----------------------------------------------------------------------------
void CCSGameStats::UploadRoundStats( void )
{
#if !defined( NO_STEAM )
	// If we don't have any data to send, we can early out now;

	// Purpose: Linux servers hang if they submit too many bullets at once and they restart. That's bad!
	// Only report rounds that are likely to yield valuable data and not cause issues:
	//		Competitive Rounds (including tournaments on other servers)
	//		Valve's Casual Rounds (to capture Operation Payback and other special events).

	
	bool bIsCompetitiveRound = ( game_mode.GetInt() == 1 && game_type.GetInt() == 0 && CSGameRules() && CSGameRules()->GetRoundLength() < 300 );
	bool bIsValveCasualRound = ( game_mode.GetInt() == 0 && game_type.GetInt() == 0 && IsValveDedicated() );	//Adding IsValveDedicated
	
	static char const * s_pchTournamentServer = CommandLine()->ParmValue( "-tournament", ( char const * ) NULL );
	static bool s_bSubmittingStats = ( RandomFloat() < 0.1 ) || ( IsValveDedicated() && s_pchTournamentServer ); // Valve tournament major servers do not throttle
	static bool s_bDevelopmentEnvironment = steamapicontext && steamapicontext->SteamUtils() && ( steamapicontext->SteamUtils()->GetConnectedUniverse() != k_EUniversePublic );

	bool bIsValidMatch = bIsCompetitiveRound || bIsValveCasualRound;

	// early-out and clear stats if it's an invalid match or we've chosen not to submit stats, unless we're also in a development environment (e.g., beta universe)
	if ( ( !bIsValidMatch || !s_bSubmittingStats ) && !s_bDevelopmentEnvironment )
	{
		CCSPlayer::ResetBulletGroup();
		ClearOGSRoundStats();
		return;
	}

	KeyValues *pKV = new KeyValues( "basedata" );
	if ( !pKV )
		return;

	CFastTimer totalTimer, weaponHitTimer, marketPurchaseTimer, submitTimer, cleanupTimer;
	g_rowCommitTime = 0.0f;
	g_rowWriteTime = 0.0f;

	totalTimer.Start();
	const char *pzMapName = gpGlobals->mapname.ToCStr();
	pKV->SetString( "MapID", pzMapName );

	weaponHitTimer.Start();
	uint32 iNumHits = m_WeaponHitData.Count();
	for ( int j=0 ; j < m_WeaponHitData.Count() ; ++j )
	{
		m_WeaponHitData[ j ]->CompactBulletID();
		SubmitStat( m_WeaponHitData[ j ] );
	}
	weaponHitTimer.End();

	marketPurchaseTimer.Start();
	uint32 iNumPurchases = m_MarketPurchases.Count();
	for ( int k=0 ; k < m_MarketPurchases.Count() ; ++k )
		SubmitStat( m_MarketPurchases[ k ] );
	marketPurchaseTimer.End();

	submitTimer.Start();
	// Perform the actual submission
	SubmitGameStats( pKV );
	submitTimer.End();

	cleanupTimer.Start();
	// Clear out the per round stats
	ClearOGSRoundStats();
	pKV->deleteThis();
	cleanupTimer.End();

	totalTimer.End();

	if ( sv_debugroundstats.GetBool() )
	{
		Msg( "**** ROUND STAT DEBUG ****\n" );
		Msg( "UploadRoundStats completed. %.3f msec. Breakdown:\n hit: %.3f msec\n market: %.3f msec\n submit: %.3f msec\n cleanup: %.3f msec\n counts: %d %d \n commit: %.3fms\n write: %.3fms.\n\n",
			totalTimer.GetDuration().GetMillisecondsF(),
			weaponHitTimer.GetDuration().GetMillisecondsF(),
			marketPurchaseTimer.GetDuration().GetMillisecondsF(),
			submitTimer.GetDuration().GetMillisecondsF(),
			cleanupTimer.GetDuration().GetMillisecondsF(),
			iNumHits, iNumPurchases, g_rowCommitTime, g_rowWriteTime );
	}

	// Reset the bullet ID.
	CCSPlayer::ResetBulletGroup();
#endif // !NO_STEAM
}

#if 0 
CON_COMMAND ( teststats, "Test command" )
{
	CFastTimer totalTimer;
	double uploadTime = 0.0f;
	g_rowCommitTime = 0.0f;
	g_rowWriteTime = 0.0f;

	for( int i = 0; i < 1000; i++ )
	{
		KeyValues *pKV = new KeyValues( "basedata" );
		if ( !pKV )
			return;

		pKV->SetName( "foobartest" );
		pKV->SetUint64( "test1", 1234 );
		pKV->SetUint64( "test2", 1234 );
		pKV->SetUint64( "test3", 1234 );
		pKV->SetUint64( "test4", 1234 );
		pKV->SetString( "test5", "TEST1234567890TEST1234567890TEST!");

		totalTimer.Start();
		GetSteamWorksGameStatsServer().AddStatsForUpload( pKV, args.ArgC() == 1 );
		totalTimer.End();

		uploadTime += totalTimer.GetDuration().GetMillisecondsF();
	}

	Msg( "teststats took %.3f msec   commit: %.3fms   write: %.3fms.\n", uploadTime, g_rowCommitTime, g_rowWriteTime );
}
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGameStats::SubmitGameStats( KeyValues *pKV )
{
#if !defined( NO_STEAM )
	int listCount = s_StatLists->Count();
	for( int i=0; i < listCount; ++i )
	{
		// Create a master key value that has stats everybody should share (map name, session ID, etc)
		(*s_StatLists)[i]->SendData(pKV);
		(*s_StatLists)[i]->Clear();
	}
#endif // !NO_STEAM
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCSGameStats::StatContainerList_t* CCSGameStats::GetStatContainerList( void )
{
#if !defined( NO_STEAM )
	return s_StatLists;
#else
	return NULL;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCSGameStats::AnyOGSDataToSubmit( void )
{
#if !defined( NO_STEAM )
	return m_WeaponHitData.Count() > 0 || m_MarketPurchases.Count() > 0;
#else
	return false;
#endif
}

void CCSGameStats::CreateNewGameStatsSession( void )
{
	GetSteamWorksGameStatsServer().EndSession();
	GetSteamWorksGameStatsServer().StartSession();
}

void CCSGameStats::RecordWeaponHit( SWeaponHitData* pHitData )
{
	m_WeaponHitData.AddToTail( pHitData );
}

float UTIL_GetEffectiveRange( CCSPlayer* pPlayer )
{
	if ( !pPlayer )
		return 0.0f;

	CWeaponCSBase *weapon = dynamic_cast< CWeaponCSBase * >( pPlayer->GetActiveWeapon() );

	if ( !weapon )
		return 0.0f;

	Vector vecDirShooting, vecRight, vecUp;
	AngleVectors( pPlayer->GetFinalAimAngle(), &vecDirShooting, &vecRight, &vecUp );
	float fInaccuracy = weapon->GetInaccuracy();
	float fSpread = weapon->GetSpread();
	float fFinalInaccuracy = fInaccuracy + fSpread;
	Vector vecInaccFinal = vecDirShooting + fFinalInaccuracy * vecRight + fFinalInaccuracy * vecUp;
	VectorNormalize( vecInaccFinal );
	float flDotInaccFinal = DotProduct( vecDirShooting.Normalized(), vecInaccFinal.Normalized() );
	float flAngleInaccFinal = flDotInaccFinal < 0.0f ? -acos( flDotInaccFinal ) : acos( flDotInaccFinal );
	//Msg( "Inaccuracy			: %.2f deg.\n", RAD2DEG( flAngleInaccFinal ) );

	return ( 0.5 * 12 ) / tanf( 0.5 * flAngleInaccFinal ); // 12 inch dinner plate 
}


SWeaponHitData::SWeaponHitData( CCSPlayer *pCSTarget, const CTakeDamageInfo &info, uint8 subBullet, uint8 round, uint8 iRecoilIndex ) 
{
	Clear();

	CWeaponCSBase* pCSWeapon = dynamic_cast< CWeaponCSBase * >(info.GetWeapon());

	// If we don't have a valid pCSWeapon then the weapon is most likely a radius type weapon 
	
	uint8 shotInaccuracy = 0;

	{
		const CEconItemView* pWeaponItemView;
		const CCSWeaponInfo* pWeaponInfoFronDamage = CCSPlayer::GetWeaponInfoFromDamageInfo( info, &pWeaponItemView );
		NOTE_UNUSED( pWeaponInfoFronDamage );
		if ( pWeaponItemView )
			m_ui8WeaponID = pWeaponItemView->GetStaticData()->GetDefinitionIndex();
	}

	CCSPlayer *pCSAttacker = ToCSPlayer( info.GetAttacker() );

	// If there isn't a valid attacker, then try using the weapon's owner entity
	if ( !pCSAttacker && pCSWeapon )
	{
		pCSAttacker = ToCSPlayer( pCSWeapon->GetOwnerEntity() );		
	}

	if ( pCSAttacker )
	{
		// If we haven't been able to classify the weapon yet, then assume it's the player's current active weapon.
		if ( m_ui8WeaponID == 0 && pCSAttacker->GetActiveWeapon() )
		{
			const CEconItemView* pWeaponItemView = pCSAttacker->GetActiveWeapon()->GetEconItemView();
			if ( pWeaponItemView )
				m_ui8WeaponID = pWeaponItemView->GetStaticData()->GetDefinitionIndex();
		}

		m_vAttackerPos = pCSAttacker->GetAbsOrigin();
		m_ui64AttackerID = GetPlayerSessionID( pCSAttacker );

		m_uAttackerMovement = (uint8)pCSAttacker->GetAbsVelocity().Length2D() << 8
								| (FBitSet( pCSAttacker->GetFlags(), FL_ONGROUND ) ? 0 : 1) << 16	//Not on ground?
								| (FBitSet( pCSAttacker->GetFlags(), FL_DUCKING ) ? 1 : 0) << 17
								| shotInaccuracy;

	}

	if ( pCSTarget )
	{
		m_vTargetPos = pCSTarget->GetAbsOrigin();
		m_uiDamage = info.GetDamage();
		m_ui8Health = pCSTarget->GetHealth();

		// Where on the target's body was hit?
		// HITGROUP_GENERIC, HITGROUP_HEAD, HITGROUP_CHEST, HITGROUP_STOMACH, HITGROUP_LEFTARM, HITGROUP_RIGHTARM, HITGROUP_LEFTLEG, HITGROUP_RIGHTLEG, HITGROUP_NECK, HITGROUP_GEAR
		m_HitRegion = pCSTarget->m_LastHitGroup;
		m_ui64TargertID = GetPlayerSessionID( pCSTarget );		

		m_uAttackerMovement = m_uAttackerMovement | (uint8)pCSTarget->GetAbsVelocity().Length2D() << 24;
	}

	m_uiBulletID = info.GetBulletID();
	m_uiSubBulletID = subBullet;
	m_uiRecoilIndex = iRecoilIndex;
	m_RoundID = round;
}

bool SWeaponHitData::InitAsGrenadeDetonation( CBaseCSGrenadeProjectile *pGrenade, uint32 unBulletGroup )
{
	// Weaponinfo gets set in different places for different grenades... 
	// all grenades getting detonated should have a weaponinfo set by then, but it's hard to know that just reading
	// the code. Putting a warning/safety guard here just in case.
	if ( !pGrenade || pGrenade->GetSourceWeaponIndex() == INVALID_ITEM_DEF_INDEX )
	{
		Warning( "Failing to submit row for a grenade detonation: Grenade has no weapon info!\n" );
		return false;
	}

	Assert( pGrenade->GetSourceWeaponIndex() >= 0 && pGrenade->GetSourceWeaponIndex() < SCHEMA_BASE_ITEM_MAX );
	m_ui8WeaponID = ( uint8 )pGrenade->GetSourceWeaponIndex();
	CCSPlayer *pCSAttacker = ToCSPlayer( pGrenade->GetThrower() );
	Assert( pGrenade->GetThrower() == pGrenade->GetOriginalThrower() ); // Appears these are always the same-- If this fires, investigate which should be recorded.
	if ( pCSAttacker )
	{
		m_vAttackerPos = pCSAttacker->GetAbsOrigin();
		m_ui64AttackerID = GetPlayerSessionID( pCSAttacker );
	}

	// target is always null for grenade detonation rows, origin is the place we landed
	m_ui64TargertID = 0;
	m_vTargetPos = pGrenade->GetAbsOrigin();
	m_uiBulletID = unBulletGroup;

	// overriding to store info about what the grenaded did upon detonation
	m_HitRegion = pGrenade->m_unOGSExtraFlags;

	m_RoundID = CSGameRules()->m_iTotalRoundsPlayed;

	// for flashes, using these fields to smuggle counts of players effected
	if ( CFlashbangProjectile *pFlash = dynamic_cast<CFlashbangProjectile*>(pGrenade) )
	{
		m_uiDamage = pFlash->m_numOpponentsHit;
		m_ui8Health = pFlash->m_numTeammatesHit;
	}
	else
	{
		m_uiDamage = 0;
		m_ui8Health = 0;
	}

	// todo: possible places to smuggle info
	m_uiSubBulletID = 0;
	m_uiRecoilIndex = 0;
	return true;
}

bool SWeaponHitData::InitAsBombEvent( CCSPlayer *pCSPlayer, CPlantedC4 *pPlantedC4, uint32 unBulletGroup, uint8 unBombsite, CSBombEventName nBombEventID )
{
	// If for any reason we cannot get a pointer to the currently-planted C4 or current player, skip data collection
	if ( !pCSPlayer || !pPlantedC4 )
	{
		Warning( "Failing to submit row for bomb plant: Player or C4 missing!\n" );
		return false;
	}		 	

	//If bomb has been planted and is not defused, set TargetID to Planter 
	//If bomb has been planted and has been defused, set TargetID to Defuser 
	//Store plant state in m_uiDamage
	m_uiDamage = nBombEventID;

	if ( CCSPlayer *pPlanter = pPlantedC4->GetPlanter() )
	{
		//Get data from planted C4: WeaponID
		if ( const CEconItemView *pPlantedC4ItemView = pPlanter->Inventory()->GetItemInLoadout( pPlanter->GetTeamNumber(), LOADOUT_POSITION_C4 ) )
		{
			m_ui8WeaponID = (uint8)pPlantedC4ItemView->GetItemIndex();
		}
		
		if ( nBombEventID == BOMB_EVENT_NAME_PLANTED )
		{
			m_ui64TargertID = GetPlayerSessionID( pPlanter );
			m_uiDamage = 1;	
		}
		
		if ( nBombEventID == BOMB_EVENT_NAME_DEFUSED )	
		{
			if ( CCSPlayer *pDefuser = pPlantedC4->GetDefuser() )
			{
				m_ui64TargertID = GetPlayerSessionID( pDefuser );
			}							
		}
	}

	m_vTargetPos = pPlantedC4->GetAbsOrigin(); //Record Bomb Location

	// Attacker position, in this case, is just the location of the current alive player
	if ( pCSPlayer )
	{
		m_vAttackerPos = pCSPlayer->GetAbsOrigin();
		m_ui64AttackerID = GetPlayerSessionID( pCSPlayer );
	}								  
	
	m_uiBulletID = unBulletGroup;

	// overriding to store info about the bombsite involved.
	
	// Shifting storage of unBombsite from m_HitRegion, because we have no guarantee that the value will fall within a tinyint field.
	m_uAttackerMovement = unBombsite;

	m_RoundID = CSGameRules()->m_iTotalRoundsPlayed;

	// fields remaining to store info about the plant/defuse event
	// m_HitRegion = 0;
	// m_ui8Health = 0;
	// m_uiSubBulletID = 0;
	// m_uiRecoilIndex = 0;
	return true;  
}



#endif // !_GAMECONSOLE

#if ITEM_TIME_DATA

ItemTimeData::ItemTimeData()
{
}

ItemTimeData::~ItemTimeData()
{
	ClearItemTimeData();
}

void ItemTimeData::ClearItemTimeData()
{
	m_mapPlayerItemTimeData.PurgeAndDeleteElements();
}

void ItemTimeData::PopulateItemTimeData()
{
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CCSPlayer* pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );
		if ( pPlayer )
		{
			uint64 ui64SteamID = pPlayer->GetSteamIDAsUInt64();


			// record all inventory items
			for ( int w = 0; w < pPlayer->WeaponCount(); w++ )
			{
				CBaseCombatWeapon *pWeapon = pPlayer->GetWeapon( w );

				if ( pWeapon == NULL )
					continue;

				if ( !pWeapon->GetEconItemView() || !pWeapon->GetEconItemView()->IsValid() )
					continue;

				OnItemPickup( ui64SteamID, pWeapon->GetEconItemView()->GetItemDefinition()->GetDefinitionIndex() );
			}


			//// Record active item
			CBaseCombatWeapon *pActiveWeapon = pPlayer->GetActiveCSWeapon();

			if ( pActiveWeapon == NULL )
				continue;

			if ( !pActiveWeapon->GetEconItemView() || !pActiveWeapon->GetEconItemView()->IsValid() )
				continue;

			OnItemEquip( ui64SteamID, pActiveWeapon->GetEconItemView()->GetItemDefinition()->GetDefinitionIndex() );

		}

	}

}

bool ItemTimeData::ShouldRecordData()
{
	return ( sv_record_item_time_data.GetBool() &&
		CSGameRules() &&
		CSGameRules()->IsPlayingClassic() &&
		!CSGameRules()->IsWarmupPeriod() );
}

// affects total item time
//
void ItemTimeData::OnItemPickup( uint64 xuid, item_definition_index_t defindex )
{
	if ( !ShouldRecordData() )
		return;

	PlayerItemTimeData * pPlayerTimeData = GetPlayerItemData( xuid );
	Assert( pPlayerTimeData );

	if ( pPlayerTimeData )
		pPlayerTimeData->OnItemPickup( defindex );
}

// affects total item time
//
void ItemTimeData::OnItemRemove( uint64 xuid, item_definition_index_t defindex )
{
	if ( !ShouldRecordData() )
		return;

	PlayerItemTimeData * pPlayerTimeData = GetPlayerItemData( xuid );
	Assert( pPlayerTimeData );

	if ( pPlayerTimeData )
		pPlayerTimeData->OnItemRemove( defindex );
}


// affects active item time
//
void ItemTimeData::OnItemEquip( uint64 xuid, item_definition_index_t defindex )
{
	if ( !ShouldRecordData() )
		return;

	PlayerItemTimeData * pPlayerTimeData = GetPlayerItemData( xuid );
	Assert( pPlayerTimeData );

	if ( pPlayerTimeData )
		pPlayerTimeData->OnItemEquip( defindex );
}



void ItemTimeData::PrintItemTimeData( ItemTime_t timetype, bool bToSpewInsteadOfLog )
{
	FOR_EACH_MAP_FAST( m_mapPlayerItemTimeData, i )
	{
		ItemTimeData::PlayerItemTimeData::t_mapItemTimeData *pItemTimeData = NULL;

		const char * szVerb = NULL;

		ForceUpdateAll( m_mapPlayerItemTimeData.Key( i ), timetype );

		switch ( timetype )
		{
			case ACTIVE_TIME:
				pItemTimeData = &m_mapPlayerItemTimeData[ i ]->m_mapItemActiveCarriedTime;
				szVerb = "held";

				break;

			case TOTAL_TIME:
				pItemTimeData = &m_mapPlayerItemTimeData[ i ]->m_mapItemTotalCarriedTime;
				szVerb = "owned";

				break;
		}

		FOR_EACH_MAP( *pItemTimeData, j )
		{

			const CEconItemDefinition *pItemDef = GetItemSchema()->GetItemDefinition( pItemTimeData->Key( j ) );
			const char * szItemName = pItemDef->GetDefinitionName();
			float time = pItemTimeData->Element( j );

			CSteamID steamID = CSteamID( m_mapPlayerItemTimeData.Key( i ) );

			CBasePlayer * pPlayer = CBasePlayer::GetPlayerBySteamID( steamID );

			const char * szPlayerName = pPlayer ? pPlayer->GetPlayerName() : "[disconnected]";

			int nUserID = pPlayer ? pPlayer->GetUserID() : -1;

			const char * szFormat = "item time data: \"%s<%d><%s>\" %s \"%s\" for \"%.2f\"s\n";

			if ( bToSpewInsteadOfLog )
			{
				DevMsg( 
					szFormat,
					szPlayerName,
					nUserID,
					steamID.Render(),
					szVerb,
					szItemName,
					time
				);
			}
			else
			{
				UTIL_LogPrintf( 
					szFormat,
					szPlayerName,
					nUserID,
					steamID.Render(),
					szVerb,
					szItemName,
					time
				);
			}
		}
	}
}


ItemTimeData::PlayerItemTimeData * ItemTimeData::GetPlayerItemData( uint64 xuid )
{
	PlayerItemTimeData * pPlayerTimeData = NULL;
	t_mapPlayerItemTimeData::IndexType_t index = m_mapPlayerItemTimeData.Find( xuid );
	if ( index != t_mapPlayerItemTimeData::InvalidIndex() )
	{
		pPlayerTimeData = m_mapPlayerItemTimeData.Element( index );
	}
	else
	{
		pPlayerTimeData = new PlayerItemTimeData;
		m_mapPlayerItemTimeData.Insert( xuid, pPlayerTimeData );
	}

	return pPlayerTimeData;
}

void ItemTimeData::ForceUpdateAll( uint64 xuid, ItemTime_t timetype )
{
	PlayerItemTimeData * pPlayerTimeData = GetPlayerItemData( xuid );
	Assert( pPlayerTimeData );

	if ( pPlayerTimeData )
		pPlayerTimeData->ForceUpdateAll( timetype );
}

// affects total item time
//
void ItemTimeData::PlayerItemTimeData::OnItemPickup( item_definition_index_t defindex )
{
	// Only update if we don't already own one of these, e.g. 2nd flashbang 

	t_mapItemTimeData::IndexType_t index = m_mapItemTotalCarriedAcquiredTimestamp.Find( defindex );
	if ( index == t_mapPlayerItemTimeData::InvalidIndex() )
	{
		SetItemTime( &m_mapItemTotalCarriedAcquiredTimestamp, defindex, Plat_FloatTime() );

		//	Msg( "-------- PICKUP \t %s \t %.1f \n\n", GetItemSchema()->GetItemDefinition( defindex )->GetDefinitionName(), Plat_FloatTime() );
	}


}

// affects total item time
//
void ItemTimeData::PlayerItemTimeData::OnItemRemove( item_definition_index_t defindex, bool bClear /*= true*/ )
{
	t_mapItemTimeData::IndexType_t index = m_mapItemTotalCarriedAcquiredTimestamp.Find( defindex );

// 	Assert( index != t_mapItemTimeData::InvalidIndex() );

	if ( index != t_mapItemTimeData::InvalidIndex() )
	{
		float time = Plat_FloatTime() - m_mapItemTotalCarriedAcquiredTimestamp.Element( index );
		Assert( time > 0 );

		AddItemTime( &m_mapItemTotalCarriedTime, defindex, time );

//		Msg( "-------- DROP \t %s \t %.1f \n\n", GetItemSchema()->GetItemDefinition( defindex )->GetDefinitionName(), time );

		if ( bClear )
		{
			m_mapItemTotalCarriedAcquiredTimestamp.RemoveAt( index );
		}
		else
		{
			// just update the timestamp
			m_mapItemTotalCarriedAcquiredTimestamp.InsertOrReplace( defindex, Plat_FloatTime() );
		}
	}
}

// affects active item time
//
void ItemTimeData::PlayerItemTimeData::OnItemEquip( item_definition_index_t defindex )
{
	if ( m_uiCurrentActiveDefIndex )
	{
		float time = Plat_FloatTime() - m_flCurrentActiveItemTimeStamp;

		AddItemTime( &m_mapItemActiveCarriedTime, m_uiCurrentActiveDefIndex, time );
	}

	m_flCurrentActiveItemTimeStamp = Plat_FloatTime();
	m_uiCurrentActiveDefIndex = defindex;
}


static item_definition_index_t CollapseKnivesDefIndices( item_definition_index_t defindex )
{
	const CCStrike15ItemDefinition *pItemDef = assert_cast< const CCStrike15ItemDefinition* >( GetItemSchema()->GetItemDefinition( defindex ) );
	Assert( pItemDef );

	if ( !pItemDef )
		return defindex;

	static CSchemaItemDefHandle defaultKnife( "weapon_knife" );
	if ( !defaultKnife )
		return defindex;

	const CEconItemDefinition* pKnifeEconDef = defaultKnife;
	const CCStrike15ItemDefinition* pKnifeDef = assert_cast< const CCStrike15ItemDefinition* >( pKnifeEconDef );

	return defindex;
}

void ItemTimeData::PlayerItemTimeData::ForceUpdateAll( ItemTime_t timetype )
{
	switch ( timetype )
	{
		case ACTIVE_TIME:
			OnItemEquip( m_uiCurrentActiveDefIndex );
			break;

		case TOTAL_TIME:
			for ( int i = 1; i < 256; i++ )
			{
				uint8 defindex = CollapseKnivesDefIndices( i );
				OnItemRemove( defindex, false );
			}
			break;
	}


}

void ItemTimeData::PlayerItemTimeData::SetItemTime( t_mapItemTimeData * pItemTimeData, item_definition_index_t defindex, float time )
{
	if ( !defindex )
		return;

	defindex = CollapseKnivesDefIndices( defindex );

	pItemTimeData->InsertOrReplace( defindex, ( uint16 )time );
	
}


void ItemTimeData::PlayerItemTimeData::AddItemTime( t_mapItemTimeData * pItemTimeData, item_definition_index_t defindex, float time )
{
	if ( !defindex )
		return;

	defindex = CollapseKnivesDefIndices( defindex );

	t_mapItemTimeData::IndexType_t index = pItemTimeData->Find( defindex );
	if ( index != t_mapItemTimeData::InvalidIndex() )
	{
		pItemTimeData->Element( index ) += time;
	}
	else
	{
		pItemTimeData->InsertOrReplace( defindex, ( uint16 )time );
	}
}

void ItemTimeData::PrintAllAndReset()
{
	PrintItemTimeData( ItemTimeData::ACTIVE_TIME );
	PrintItemTimeData( ItemTimeData::TOTAL_TIME );
	ClearItemTimeData();
	PopulateItemTimeData();
}


CON_COMMAND( itemtimedata_dump_active, "" )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	CCS_GameStats.m_ItemTimeData.PrintItemTimeData( ItemTimeData::ACTIVE_TIME, true );
}

CON_COMMAND( itemtimedata_dump_total, "" )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	CCS_GameStats.m_ItemTimeData.PrintItemTimeData( ItemTimeData::TOTAL_TIME, true );
}

CON_COMMAND( itemtimedata_print_and_reset, "Outputs item time data to server log and clears data." )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	CCS_GameStats.m_ItemTimeData.PrintItemTimeData( ItemTimeData::ACTIVE_TIME );
	CCS_GameStats.m_ItemTimeData.PrintItemTimeData( ItemTimeData::TOTAL_TIME );
	CCS_GameStats.m_ItemTimeData.ClearItemTimeData();
	CCS_GameStats.m_ItemTimeData.PopulateItemTimeData();
}


#endif //ITEM_TIME_DATA