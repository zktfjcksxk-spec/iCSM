#include "cs_survival_funfact_types.h"

#include "cbase.h"
#include "cs_survival_funfact_types.h"

const char* kSurvivalFactNames[] = {
	"Kills",
	"Money",
	"FootstepsMade",
	"HealthshotsUsedTotal",
	"TimeAlive",

	"WorldCratesOpened",
	"DamageTaken",
	"DronesOrdered",
	"HexesExplored",
	"EnemiesDamaged",

	"MoneyScavenged",
	"DamageRate",
	"DangerZoneDamage",
 	"TimeToMelee",
	"TimeToSecondary",

	"TimeToPrimary",
	"TimeToSniperRifle",
	"TimeToHeavyAssaultSuit",
	"TimeToWin",
	"TimeToKill",

	"DamageDoneWithPunches",
	"DistanceTravelled",
	"SentriesDestroyed",
	"EnemiesFlashed",
	"FootstepsHeard",

};

COMPILE_TIME_ASSERT( V_ARRAYSIZE( kSurvivalFactNames ) == ESurvivalFact_Count );

const char* kSurvivalFactDisplayNames[] = {
	"Number",
	"Money",
	"Distance",
	"Time",
	"Percent",
};
COMPILE_TIME_ASSERT( V_ARRAYSIZE( kSurvivalFactDisplayNames ) == ESurvivalFactDisplay_Count );

