
#ifndef CS_SURVIVAL_FUNFACT_TYPES_H
#define CS_SURVIVAL_FUNFACT_TYPES_H

enum ESurvivalFactDisplay
{
	ESurvivalFactDisplay_Number,
	ESurvivalFactDisplay_Money,
	ESurvivalFactDisplay_Distance,
	ESurvivalFactDisplay_Time,
	ESurvivalFactDisplay_Percent,

	ESurvivalFactDisplay_Count
};

enum ESurvivalFact
{
	ESurvivalFact_Invalid = -1,

	ESurvivalFact_Kills = 0,
	ESurvivalFact_Money,
	ESurvivalFact_FootstepsMade,
	ESurvivalFact_HealthshotsUsedTotal,
	ESurvivalFact_TimeAlive,

	ESurvivalFact_WorldCratesOpened,
	ESurvivalFact_DamageTaken,
	ESurvivalFact_DronesOrdered,
	ESurvivalFact_HexesExplored,
	ESurvivalFact_EnemiesDamaged,

	ESurvivalFact_MoneyScavenged,
 	ESurvivalFact_DamageRate,
 	ESurvivalFact_DangerZoneDamage,
 	ESurvivalFact_TimeToMelee,
	ESurvivalFact_TimeToSecondary,

 	ESurvivalFact_TimeToPrimary,
	ESurvivalFact_TimeToSniperRifle,
	ESurvivalFact_TimeToHeavyAssaultSuit,
	ESurvivalFact_TimeToWin,
	ESurvivalFact_TimeToKill,

	ESurvivalFact_DamageDoneWithPunches,
	ESurvivalFact_DistanceTravelled,
	ESurvivalFact_SentriesDestroyed,
	ESurvivalFact_EnemiesFlashed,
	ESurvivalFact_FootstepsHeard,


	// none of these are implemented yet
	//ESurvivalFact_DamageDealt,
	//ESurvivalFact_NetWorth,
	
 
	

	ESurvivalFact_Count
};

constexpr int kMaxSurvivalFunFacts = 5;

struct CSurvivalFunFact
{
	ESurvivalFact m_eFactType;
	ESurvivalFactDisplay m_eFactDisplay;
	int m_nValue;
	float m_flInterestingness;
};

struct CSurvivalFunFacts
{
	CSurvivalFunFact m_Facts[kMaxSurvivalFunFacts];
};

extern const char* kSurvivalFactNames[]; // size = ESurvivalFact_Count
extern const char* kSurvivalFactDisplayNames[]; // size = ESurvivalFactDisplay_Count

#endif // CS_SURVIVAL_FUNFACT_TYPES_H