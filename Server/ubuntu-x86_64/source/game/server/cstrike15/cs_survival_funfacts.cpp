
#include "cbase.h"
#include "cs_survival_funfacts.h"
#include "cs_player.h"
#include "dangerzone_controller.h"

typedef CSurvivalFunFact ( *TSurvivalFactEvaluator )( ESurvivalFact eStat, CCSPlayer* pPlayer );
static CSurvivalFunFact EvaluateSurvivalKills( ESurvivalFact eStat, CCSPlayer* pPlayer );
static CSurvivalFunFact EvaluateSurvivalMoney( ESurvivalFact eStat, CCSPlayer* pPlayer );
static CSurvivalFunFact EvaluateSurvivalFootsteps( ESurvivalFact eStat, CCSPlayer* pPlayer );
static CSurvivalFunFact EvaluateSurvivalHealthshots( ESurvivalFact eStat, CCSPlayer* pPlayer );
static CSurvivalFunFact EvaluateSurvivalTimeAlive( ESurvivalFact eStat, CCSPlayer* pPlayer );
static CSurvivalFunFact EvaluateSurvivalWorldCratesOpened( ESurvivalFact eStat, CCSPlayer* pPlayer );
static CSurvivalFunFact EvaluateSurvivalDamageTaken( ESurvivalFact eStat, CCSPlayer* pPlayer );
static CSurvivalFunFact EvaluateSurvivalDronesOrdered( ESurvivalFact eStat, CCSPlayer* pPlayer );
static CSurvivalFunFact EvaluateSurvivalHexesExplored( ESurvivalFact eStat, CCSPlayer* pPlayer );
static CSurvivalFunFact EvaluateSurvivalEnemiesDamaged( ESurvivalFact eStat, CCSPlayer* pPlayer );
static CSurvivalFunFact EvaluateSurvivalMoneyScavenged( ESurvivalFact eStat, CCSPlayer* pPlayer );
static CSurvivalFunFact EvaluateSurvivalDamageRate( ESurvivalFact eStat, CCSPlayer* pPlayer );
static CSurvivalFunFact EvaluateSurvivalDangerZoneDamage( ESurvivalFact eStat, CCSPlayer* pPlayer );
static CSurvivalFunFact EvaluateSurvivalTimeToMelee( ESurvivalFact eStat, CCSPlayer* pPlayer );
static CSurvivalFunFact EvaluateSurvivalTimeToSecondary( ESurvivalFact eStat, CCSPlayer* pPlayer );
static CSurvivalFunFact EvaluateSurvivalTimeToPrimary( ESurvivalFact eStat, CCSPlayer* pPlayer );
static CSurvivalFunFact EvaluateSurvivalTimeToSniperRifle( ESurvivalFact eStat, CCSPlayer* pPlayer );
static CSurvivalFunFact EvaluateSurvivalTimeToHeavyAssaultSuit( ESurvivalFact eStat, CCSPlayer* pPlayer );
static CSurvivalFunFact EvaluateSurvivalTimeToWin( ESurvivalFact eStat, CCSPlayer* pPlayer );
static CSurvivalFunFact EvaluateSurvivalTimeToKill( ESurvivalFact eStat, CCSPlayer* pPlayer );
static CSurvivalFunFact EvaluateSurvivalDamageDoneWithPunches( ESurvivalFact eStat, CCSPlayer* pPlayer );
static CSurvivalFunFact EvaluateSurvivalDistanceTravelled( ESurvivalFact eStat, CCSPlayer* pPlayer );
static CSurvivalFunFact EvaluateSurvivalSentriesDestroyed( ESurvivalFact eStat, CCSPlayer* pPlayer );
static CSurvivalFunFact EvaluateSurvivalEnemiesFlashed( ESurvivalFact eStat, CCSPlayer* pPlayer );
static CSurvivalFunFact EvaluateSurvivalFootstepsHeard( ESurvivalFact eStat, CCSPlayer* pPlayer );



struct CSurvivalStatEvaluator {
	ESurvivalFact eFactType;
	TSurvivalFactEvaluator fnEvalautor;
};
const CSurvivalStatEvaluator kSurvivalStatEvaluators[] =
{
	{ ESurvivalFact_Kills, &EvaluateSurvivalKills },
	{ ESurvivalFact_Money, &EvaluateSurvivalMoney },
	{ ESurvivalFact_FootstepsMade, &EvaluateSurvivalFootsteps },
	{ ESurvivalFact_HealthshotsUsedTotal, &EvaluateSurvivalHealthshots },
	{ ESurvivalFact_TimeAlive, &EvaluateSurvivalTimeAlive },
	{ ESurvivalFact_WorldCratesOpened, &EvaluateSurvivalWorldCratesOpened },
	{ ESurvivalFact_DamageTaken, &EvaluateSurvivalDamageTaken },
	{ ESurvivalFact_DronesOrdered, &EvaluateSurvivalDronesOrdered },
	{ ESurvivalFact_HexesExplored, &EvaluateSurvivalHexesExplored },
	{ ESurvivalFact_EnemiesDamaged, &EvaluateSurvivalEnemiesDamaged },
	{ ESurvivalFact_MoneyScavenged, &EvaluateSurvivalMoneyScavenged },
	{ ESurvivalFact_DamageRate, &EvaluateSurvivalDamageRate },
	{ ESurvivalFact_DangerZoneDamage, &EvaluateSurvivalDangerZoneDamage },
	{ ESurvivalFact_TimeToMelee, &EvaluateSurvivalTimeToMelee },
	{ ESurvivalFact_TimeToSecondary, &EvaluateSurvivalTimeToSecondary },
	{ ESurvivalFact_TimeToPrimary, &EvaluateSurvivalTimeToPrimary },
	{ ESurvivalFact_TimeToSniperRifle, &EvaluateSurvivalTimeToSniperRifle },
	{ ESurvivalFact_TimeToHeavyAssaultSuit, &EvaluateSurvivalTimeToHeavyAssaultSuit },
	{ ESurvivalFact_TimeToWin, &EvaluateSurvivalTimeToWin },
	{ ESurvivalFact_TimeToKill, &EvaluateSurvivalTimeToKill },
	{ ESurvivalFact_DamageDoneWithPunches, &EvaluateSurvivalDamageDoneWithPunches },
	{ ESurvivalFact_DistanceTravelled, &EvaluateSurvivalDistanceTravelled },
	{ ESurvivalFact_SentriesDestroyed, &EvaluateSurvivalSentriesDestroyed },
	{ ESurvivalFact_EnemiesFlashed, &EvaluateSurvivalEnemiesFlashed },
	{ ESurvivalFact_FootstepsHeard, &EvaluateSurvivalFootstepsHeard },
};

static const CSurvivalFunFact kEmptySurvivalFunFact = {
	ESurvivalFact_Invalid,
	ESurvivalFactDisplay_Number,
	0,
	-FLOAT32_MAX,
};

void FunFacts_EvaluateSurvivalStats( CCSPlayer* pPlayer, CSurvivalFunFacts* pFunFacts )
{
	// clear input
	// we maintain m_Facts[] is sorted in descending order by m_flInterestingness
	COMPILE_TIME_ASSERT( V_ARRAYSIZE( pFunFacts->m_Facts ) == kMaxSurvivalFunFacts );
	for ( int i = 0; i < kMaxSurvivalFunFacts; ++i )
	{
		pFunFacts->m_Facts[i] = kEmptySurvivalFunFact;
	}

	// Don't evaluate facts for bots
	if ( pPlayer->IsBot() )
		return;

	for ( int iEvaluator = 0; iEvaluator < V_ARRAYSIZE( kSurvivalStatEvaluators ); ++iEvaluator )
	{
		CSurvivalFunFact evaluation = kSurvivalStatEvaluators[iEvaluator].fnEvalautor( kSurvivalStatEvaluators[iEvaluator].eFactType, pPlayer );
		if ( evaluation.m_eFactType < 0 || evaluation.m_eFactType >= ESurvivalFact_Count )
			continue;

		UTIL_LogPrintf( "FUNFACT: %s,\t%s<%i><%s>,\tVALUE: %d,\tIQ: %f\n", 
			kSurvivalFactNames[ evaluation.m_eFactType ],
			pPlayer->GetPlayerName(),
			pPlayer->GetPlayerInfo()->GetUserID(),
			pPlayer->GetNetworkIDString(),
			evaluation.m_nValue,
			evaluation.m_flInterestingness );

		// find where to insert this fact
		int iInsertLocation;
		for ( iInsertLocation = 0; iInsertLocation < kMaxSurvivalFunFacts; ++iInsertLocation )
		{
			if ( pFunFacts->m_Facts[iInsertLocation].m_flInterestingness < evaluation.m_flInterestingness )
				break;
		}

		// insert it, moving the other ones up (this will lose the last entry)
		if ( iInsertLocation < kMaxSurvivalFunFacts )
		{
			for ( int iCopyTo = kMaxSurvivalFunFacts - 1; iCopyTo > iInsertLocation; --iCopyTo )
				pFunFacts->m_Facts[iCopyTo] = pFunFacts->m_Facts[iCopyTo - 1];

			pFunFacts->m_Facts[iInsertLocation] = evaluation;
		}
	}
}

void FunFacts_SendSurvivalStats( CCSPlayer* pSendToPlayer, uint64 targetXuid, const CSurvivalFunFacts& funFacts )
{
	CBroadcastRecipientFilter filter;
	filter.MakeReliable();
	
	CCSUsrMsg_SurvivalStats msg;
	msg.set_xuid( targetXuid );
	
	for ( int i = 0; i < V_ARRAYSIZE( funFacts.m_Facts ); ++i )
	{
		const CSurvivalFunFact& fact = funFacts.m_Facts[i];

		if ( fact.m_eFactType < 0 || fact.m_eFactType >= ESurvivalFact_Count )
			continue;

		CCSUsrMsg_SurvivalStats_Fact *pFactMsg = msg.add_facts();
		pFactMsg->set_type( fact.m_eFactType );
		pFactMsg->set_display( fact.m_eFactDisplay );
		pFactMsg->set_value( fact.m_nValue );
		pFactMsg->set_interestingness( fact.m_flInterestingness );
	}

	// Also populate all the placements into the same message
	for ( int i = 1; i <= MAX_PLAYERS; ++ i )
	{
		uint64 ullXuid = CSGameRules()->GetSurvivalRules()->GetPlayerXuid( i );
		if ( ullXuid )
		{
			int iPosition = CSGameRules()->GetSurvivalRules()->GetPlayerPosition( i );
			CCSUsrMsg_SurvivalStats_Placement *pPlacement = msg.add_users();
			pPlacement->set_xuid( ullXuid );
			pPlacement->set_placement( iPosition );
			if ( CSGameRules()->GetSurvivalRules()->IsPlayingTeamMode() )
			{
				int iTeamNumber = CSGameRules()->GetSurvivalRules()->GetPlayerTeamIndex( i );
				pPlacement->set_teamnumber( iTeamNumber );
			}
		}
	}

	// populate damage data
	CUtlMap< uint64, CCSUsrMsg_SurvivalStats_Damage*, unsigned short, CDefLess<uint64> > mapXuidToDamage;
	const auto& damageList = pSendToPlayer->GetDamageList();
	FOR_EACH_LL( damageList, iDamageRecord )
	{
		CDamageRecord* pDamageRecord = damageList[iDamageRecord];
		uint64 xuid = 0;
		bool bFrom = false;

		if ( pDamageRecord->GetPlayerRecipientPtr() == pSendToPlayer )
		{
			xuid = pDamageRecord->GetPlayerDamagerXuid();
			bFrom = true;
		}
		else if ( pDamageRecord->GetPlayerDamagerPtr() == pSendToPlayer )
		{
			xuid = pDamageRecord->GetPlayerRecipientXuid();
			bFrom = false;
		}

		if ( xuid != 0 )
		{
			int mapIdx = mapXuidToDamage.Find( xuid );
			if ( mapIdx == mapXuidToDamage.InvalidIndex() )
			{
				CCSUsrMsg_SurvivalStats_Damage* pNewDamage = new CCSUsrMsg_SurvivalStats_Damage;
				pNewDamage->set_xuid( xuid );
				pNewDamage->set_to( 0 );
				pNewDamage->set_to_hits( 0 );
				pNewDamage->set_from( 0 );
				pNewDamage->set_from_hits( 0 );
				mapIdx = mapXuidToDamage.Insert( xuid, pNewDamage );
			}
			CCSUsrMsg_SurvivalStats_Damage* pDamage = mapXuidToDamage.Element( mapIdx );

			int nAmount = pDamageRecord->GetDamage();
			int nHits = pDamageRecord->GetNumHits();

			if ( bFrom )
			{
				pDamage->set_from( pDamage->from() + nAmount );
				pDamage->set_from_hits( pDamage->from_hits() + nHits );
			}
			else
			{
				pDamage->set_to( pDamage->to() + nAmount );
				pDamage->set_to_hits( pDamage->to_hits() + nHits );
			}
		}
	}
	FOR_EACH_MAP( mapXuidToDamage, iMapDamage )
	{
		*( msg.add_damages() ) = *( mapXuidToDamage[iMapDamage] );
	}
	mapXuidToDamage.PurgeAndDeleteElements();

	msg.set_ticknumber( gpGlobals->tickcount );
	
	SendUserMessage( filter, CS_UM_SurvivalStats, msg );
}

#define INTERESTING_LOW_MIN 0.0f
#define INTERESTING_LOW_MAX 100.0f

#define INTERESTING_MID_MIN 1000.0f
#define INTERESTING_MID_MAX 1100.0f

#define INTERESTING_HIGH_MIN 10000.0f
#define INTERESTING_HIGH_MAX 10100.0f

#define INTERESTING_FACTOR_MIN 0.1f
#define INTERESTING_FACTOR_MAX 10.0f


extern ConVar sv_dz_cash_bundle_size;
extern ConVar sv_dz_zone_bombdrop_money_reward;

static float CalculateInterestingnessFactor( float value, float flMin, float flMax )
{
	return RemapValClamped( value, flMin, flMax, INTERESTING_FACTOR_MIN, INTERESTING_FACTOR_MAX );

}

static float GenerateHighInterestingness()
{
	return RandomFloat( INTERESTING_HIGH_MIN, INTERESTING_HIGH_MAX );
}

static float GenerateMidInterestingness()
{
	return RandomFloat( INTERESTING_MID_MIN, INTERESTING_MID_MAX );
}

static float GenerateLowInterestingness()
{
	return RandomFloat( INTERESTING_LOW_MIN, INTERESTING_LOW_MAX );
}

static CSurvivalFunFact EvaluateSurvivalKills( ESurvivalFact eFact, CCSPlayer* pPlayer )
{
	return CSurvivalFunFact{
		eFact,
		ESurvivalFactDisplay_Number,
		pPlayer->FragCount(),
		GenerateMidInterestingness() * CalculateInterestingnessFactor( pPlayer->FragCount(), 0, 3 ),
	};
}

static CSurvivalFunFact EvaluateSurvivalMoney( ESurvivalFact eFact, CCSPlayer* pPlayer )
{

	int nWavesPassed = GetDangerZoneController()->GetNumWaveRewardsGranted();
	int nIncomeEarnedFromWaves = nWavesPassed * sv_dz_cash_bundle_size.GetInt() * sv_dz_zone_bombdrop_money_reward.GetInt();

	float flDeviationFromBaseIncome = pPlayer->m_iAccount - mp_startmoney.GetInt() - nIncomeEarnedFromWaves;

	// interestingness is how from wave income player deviates, +-1000;
	return CSurvivalFunFact{
		eFact,
		ESurvivalFactDisplay_Money,
		pPlayer->m_iAccount,
		GenerateMidInterestingness() * CalculateInterestingnessFactor( flDeviationFromBaseIncome, -1000.0f, 1000.0f ),
	};
}

static CSurvivalFunFact EvaluateSurvivalFootsteps( ESurvivalFact eFact, CCSPlayer* pPlayer )
{
	int nFootsteps = pPlayer->m_nRoundFootstepsMade;

	return CSurvivalFunFact{
		eFact,
		ESurvivalFactDisplay_Number,
		nFootsteps,
		GenerateLowInterestingness(),
	};
}

static CSurvivalFunFact EvaluateSurvivalHealthshots( ESurvivalFact eFact, CCSPlayer* pPlayer )
{
	/* - m_nHealthshotsUsedTotalCount is always relevant
	// not relevant if died with no healthshots or didn't die
	if ( pPlayer->m_nHealthshotsInInventoryOnDeath <= 0 )
		return kEmptySurvivalFunFact;

	// not relevant if player won
	if ( pPlayer->IsAlive() )
		return kEmptySurvivalFunFact;
	*/

	return CSurvivalFunFact{
		eFact,
		ESurvivalFactDisplay_Number,
		pPlayer->m_nHealthshotsUsedTotalCount,
		GenerateMidInterestingness() * CalculateInterestingnessFactor( pPlayer->m_nHealthshotsUsedTotalCount, 0.0f, 3.0f )
	};
}

CSurvivalFunFact EvaluateSurvivalTimeAlive( ESurvivalFact eStat, CCSPlayer* pPlayer )
{


// 	int nPlayerPosition = CSGameRules()->GetSurvivalRules()->GetPlayerPosition( pPlayer->entindex() );
// 	int nTotalNumPlayers = CSGameRules()->GetSurvivalRules()->GetTotalNumPlayers();
// 	if ( nTotalNumPlayers == 0 )
// 		return kEmptySurvivalFunFact;

	int nTimeAliveInSeconds = (int)pPlayer->GetLongestSurvivalTime();
	if ( nTimeAliveInSeconds == 0 )
		return kEmptySurvivalFunFact;

	// skip if this if player alive at the end.
	if ( pPlayer->m_nTimeToWin == nTimeAliveInSeconds )
		return kEmptySurvivalFunFact;

	return CSurvivalFunFact{
		eStat,
		ESurvivalFactDisplay_Time,
		nTimeAliveInSeconds,
		GenerateMidInterestingness() * CalculateInterestingnessFactor( nTimeAliveInSeconds, 0, 700 )
	};
}


CSurvivalFunFact EvaluateSurvivalWorldCratesOpened( ESurvivalFact eStat, CCSPlayer* pPlayer )
{

	float flInterestingness;

	// Player either didn't open crates because they don't know to, or because they're super awesome.
	// Either way, point it out to them.
	if ( pPlayer->m_nWorldCratesOpened == 0 )
	{
		flInterestingness = GenerateHighInterestingness();
	}
	else
	{
		int nTimeAliveInSeconds = pPlayer->GetLongestSurvivalTime();
		if ( nTimeAliveInSeconds == 0 )
			return kEmptySurvivalFunFact;

		flInterestingness = GenerateMidInterestingness() * CalculateInterestingnessFactor( pPlayer->m_nWorldCratesOpened, 0.0f, 10.0f );
	}

	return CSurvivalFunFact{
		eStat,
		ESurvivalFactDisplay_Number,
		pPlayer->m_nWorldCratesOpened,
		flInterestingness,
	};
}


CSurvivalFunFact EvaluateSurvivalDamageTaken( ESurvivalFact eStat, CCSPlayer* pPlayer )
{
	// not interesting if dead player took average damage
	if ( !pPlayer->IsAlive() && pPlayer->GetDamageTaken() < 200 )
		return kEmptySurvivalFunFact;

	float flInterestingness = GenerateMidInterestingness() * CalculateInterestingnessFactor( pPlayer->GetDamageTaken(), 200, 400 );

	// VERY interesting if player won with very little damage taken
	if ( pPlayer->IsAlive() && pPlayer->GetDamageTaken() < 100 )
		flInterestingness = GenerateHighInterestingness();

	return CSurvivalFunFact{
		eStat,
		ESurvivalFactDisplay_Number,
		pPlayer->GetDamageTaken(),
		flInterestingness,
	};
}


CSurvivalFunFact EvaluateSurvivalDronesOrdered( ESurvivalFact eStat, CCSPlayer* pPlayer )
{

	return CSurvivalFunFact{
		eStat,
		ESurvivalFactDisplay_Number,
		pPlayer->m_nDronesOrdered,
		GenerateMidInterestingness() * CalculateInterestingnessFactor( pPlayer->m_nDronesOrdered, 0, 4 ),
	};

}

CSurvivalFunFact EvaluateSurvivalHexesExplored( ESurvivalFact eStat, CCSPlayer* pPlayer )
{
	float flMinutesAlive = ( pPlayer->GetLongestSurvivalTime() > 0 ) ? pPlayer->GetLongestSurvivalTime() / 60.0f : 1;

	// really interesting if it was unusually high or unusually low
	// so we subtract a magical average for each minute alive and square that

	float flPerformance = pPlayer->m_nHexesExplored;

	flPerformance = flPerformance - ( 1.0f * flMinutesAlive );
	flPerformance = flPerformance * flPerformance;

	return CSurvivalFunFact{
		eStat,
		ESurvivalFactDisplay_Number,
		pPlayer->m_nHexesExplored,
		GenerateMidInterestingness() * CalculateInterestingnessFactor( flPerformance, 0.0f, 20.0f ),
	};

}


CSurvivalFunFact EvaluateSurvivalEnemiesDamaged( ESurvivalFact eStat, CCSPlayer* pPlayer )
{
	// float flMinutesAlive = ( pPlayer->GetLongestSurvivalTime() > 0 ) ? pPlayer->GetLongestSurvivalTime() / 60.0f : 1;

	// really interesting if it was unusually high or unusually low
	// so we subtract a magical average for each minute alive and square that

	int numKills = pPlayer->FragCount();
	float flPerformance = pPlayer->GetNumEnemiesDamaged();
	flPerformance = flPerformance / MAX( numKills + 1, 2 );
	if ( flPerformance <= 1.0f )
		return kEmptySurvivalFunFact;

	return CSurvivalFunFact{
		eStat,
		ESurvivalFactDisplay_Number,
		pPlayer->GetNumEnemiesDamaged(),
		GenerateMidInterestingness() * CalculateInterestingnessFactor( flPerformance, 1.0f, 1.5f ),
	};
}

CSurvivalFunFact EvaluateSurvivalMoneyScavenged( ESurvivalFact eStat, CCSPlayer* pPlayer )
{

	// rate of scavenging income, which is income minus start money and wave income.

	int nWavesPassed = GetDangerZoneController()->GetNumWaveRewardsGranted();
	int nIncomeEarnedFromWaves = nWavesPassed * sv_dz_cash_bundle_size.GetInt() * sv_dz_zone_bombdrop_money_reward.GetInt();

	int nScavengeIncome = pPlayer->m_iMatchStats_CashEarned_Total - nIncomeEarnedFromWaves;
	if ( nScavengeIncome <= 0 )
		return kEmptySurvivalFunFact;

	return CSurvivalFunFact{
		eStat,
		ESurvivalFactDisplay_Money,
		nScavengeIncome,
		GenerateMidInterestingness() * CalculateInterestingnessFactor( nScavengeIncome, 0, 30 * sv_dz_cash_bundle_size.GetInt() ),
	};
}


CSurvivalFunFact EvaluateSurvivalDamageRate( ESurvivalFact eStat, CCSPlayer* pPlayer )
{
	if ( pPlayer->GetLongestSurvivalTime() <= 0 )
		return kEmptySurvivalFunFact;

	float flMinutesAlive = pPlayer->GetLongestSurvivalTime() / 60.0f;

	int nDamagePerMinute = pPlayer->GetDamageGiven() / flMinutesAlive;

	if ( nDamagePerMinute < 10 )
	{
		return kEmptySurvivalFunFact;
	}

	return CSurvivalFunFact{
		eStat,
		ESurvivalFactDisplay_Number,
		nDamagePerMinute,
		GenerateMidInterestingness() * CalculateInterestingnessFactor( nDamagePerMinute, 0, 50 ),
	};
}


CSurvivalFunFact EvaluateSurvivalDangerZoneDamage( ESurvivalFact eStat, CCSPlayer* pPlayer )
{
	if ( pPlayer->m_nDangerZoneDamage <= 0 )
		return kEmptySurvivalFunFact;

	return CSurvivalFunFact{
		eStat,
		ESurvivalFactDisplay_Number,
		pPlayer->m_nDangerZoneDamage,
		GenerateMidInterestingness() * CalculateInterestingnessFactor( pPlayer->m_nDangerZoneDamage, 0, 50 ),
	};
}



CSurvivalFunFact EvaluateSurvivalTimeToMelee( ESurvivalFact eStat, CCSPlayer* pPlayer )
{
	if ( pPlayer->m_nTimeToMelee < 0 )
		return kEmptySurvivalFunFact;

	return CSurvivalFunFact{
		eStat,
		ESurvivalFactDisplay_Time,
		pPlayer->m_nTimeToMelee,
		GenerateMidInterestingness() * CalculateInterestingnessFactor( pPlayer->m_nTimeToMelee, 0, 300 ), // later is more interesting
	};
}


CSurvivalFunFact EvaluateSurvivalTimeToSecondary( ESurvivalFact eStat, CCSPlayer* pPlayer )
{
	if ( pPlayer->m_nTimeToSecondary < 0 )
		return kEmptySurvivalFunFact;

	return CSurvivalFunFact{
		eStat,
		ESurvivalFactDisplay_Time,
		pPlayer->m_nTimeToSecondary,
		GenerateMidInterestingness() * CalculateInterestingnessFactor( pPlayer->m_nTimeToSecondary, 0.0f, 120.0f ), // later is more interesting
	};
}

CSurvivalFunFact EvaluateSurvivalTimeToPrimary( ESurvivalFact eStat, CCSPlayer* pPlayer )
{
	if ( pPlayer->m_nTimeToPrimary < 0 )
		return kEmptySurvivalFunFact;

	// skip this one if it's the same as time to sniper rifle
	if ( pPlayer->m_nTimeToPrimary == pPlayer->m_nTimeToSniperRifle )
		return kEmptySurvivalFunFact;

	return CSurvivalFunFact{
		eStat,
		ESurvivalFactDisplay_Time,
		pPlayer->m_nTimeToPrimary,
		GenerateMidInterestingness() * CalculateInterestingnessFactor( pPlayer->m_nTimeToPrimary, 200, 0 ), // early is more interesting
	};
}

CSurvivalFunFact EvaluateSurvivalTimeToSniperRifle( ESurvivalFact eStat, CCSPlayer* pPlayer )
{
	if ( pPlayer->m_nTimeToSniperRifle < 0 )
		return kEmptySurvivalFunFact;

	return CSurvivalFunFact{
		eStat,
		ESurvivalFactDisplay_Time,
		pPlayer->m_nTimeToSniperRifle,
		GenerateHighInterestingness(), // any sniper rifle acquisition is interesting
	};
}

CSurvivalFunFact EvaluateSurvivalTimeToHeavyAssaultSuit( ESurvivalFact eStat, CCSPlayer* pPlayer )
{
	if ( pPlayer->m_nTimeToHeavyAssaultSuit < 0 )
		return kEmptySurvivalFunFact;

	return CSurvivalFunFact{
		eStat,
		ESurvivalFactDisplay_Time,
		pPlayer->m_nTimeToHeavyAssaultSuit,
		INTERESTING_HIGH_MAX,
	};
}


CSurvivalFunFact EvaluateSurvivalTimeToWin( ESurvivalFact eStat, CCSPlayer* pPlayer )
{
	if ( pPlayer->m_nTimeToWin < 0 )
		return kEmptySurvivalFunFact;

	return CSurvivalFunFact{
		eStat,
		ESurvivalFactDisplay_Time,
		pPlayer->m_nTimeToWin,
		INTERESTING_HIGH_MAX,
	};
}
CSurvivalFunFact EvaluateSurvivalTimeToKill( ESurvivalFact eStat, CCSPlayer* pPlayer )
{
	if ( pPlayer->m_nTimeToKill < 0 )
		return kEmptySurvivalFunFact;

	return CSurvivalFunFact{
		eStat,
		ESurvivalFactDisplay_Time,
		pPlayer->m_nTimeToKill,
		GenerateMidInterestingness() * CalculateInterestingnessFactor( pPlayer->m_nTimeToKill, 400, 0 ), // faster is more interesting
	};
}
CSurvivalFunFact EvaluateSurvivalDamageDoneWithPunches( ESurvivalFact eStat, CCSPlayer* pPlayer )
{
	if ( pPlayer->m_nDamageDoneWithPunches <= 0 )
		return kEmptySurvivalFunFact;

	return CSurvivalFunFact{
		eStat,
		ESurvivalFactDisplay_Number,
		pPlayer->m_nDamageDoneWithPunches,
		GenerateMidInterestingness() * CalculateInterestingnessFactor( pPlayer->m_nDamageDoneWithPunches, 0, 30 ),
	};
}
CSurvivalFunFact EvaluateSurvivalDistanceTravelled( ESurvivalFact eStat, CCSPlayer* pPlayer )
{
	if ( pPlayer->GetAccumulatedDistanceTraveled() <= 0 )
		return kEmptySurvivalFunFact;

	int nMetersTravelled = pPlayer->GetAccumulatedDistanceTraveled() / 39.3701f; // inches per km

	float flPerformance = nMetersTravelled / pPlayer->GetLongestSurvivalTime(); //meters per minute

	flPerformance = flPerformance - ( 2.0f * pPlayer->GetLongestSurvivalTime() );
	flPerformance = flPerformance * flPerformance;


	return CSurvivalFunFact{
		eStat,
		ESurvivalFactDisplay_Distance,
		nMetersTravelled,
		GenerateMidInterestingness() * CalculateInterestingnessFactor( flPerformance, 0, 250000 ),
	};
}
CSurvivalFunFact EvaluateSurvivalSentriesDestroyed( ESurvivalFact eStat, CCSPlayer* pPlayer )
{
	if ( pPlayer->m_nSentriesDestroyed <= 0 )
		return kEmptySurvivalFunFact;

	return CSurvivalFunFact{
		eStat,
		ESurvivalFactDisplay_Number,
		pPlayer->m_nSentriesDestroyed,
		GenerateMidInterestingness() * CalculateInterestingnessFactor( pPlayer->m_nSentriesDestroyed, 1.0f, 3.0f ),
	};

}
CSurvivalFunFact EvaluateSurvivalEnemiesFlashed( ESurvivalFact eStat, CCSPlayer* pPlayer )
{
	if ( pPlayer->m_iMatchStats_EnemiesFlashed_Total <= 0 )
		return kEmptySurvivalFunFact;

	return CSurvivalFunFact{
		eStat,
		ESurvivalFactDisplay_Number,
		pPlayer->m_iMatchStats_EnemiesFlashed_Total,
		GenerateMidInterestingness() * CalculateInterestingnessFactor( pPlayer->m_iMatchStats_EnemiesFlashed_Total, 1, 5 ),
	};

}

static CSurvivalFunFact EvaluateSurvivalFootstepsHeard( ESurvivalFact eStat, CCSPlayer* pPlayer )
{
	int numEnemies = pPlayer->m_rbEnemiesThatHeardYourFootsteps.Count();

	return CSurvivalFunFact{
		eStat,
		ESurvivalFactDisplay_Number,
		numEnemies,
		GenerateMidInterestingness() * CalculateInterestingnessFactor( numEnemies, 2, 6 ),
	};
}