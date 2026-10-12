
#ifndef CS_SURVIVAL_FUNFACTS_H
#define CS_SURVIVAL_FUNFACTS_H

#include "cs_survival_funfact_types.h"

class CCSPlayer;

void FunFacts_EvaluateSurvivalStats( CCSPlayer* pPlayer, CSurvivalFunFacts* pFunFacts );
void FunFacts_SendSurvivalStats( CCSPlayer* pSendToPlayer, uint64 targetXuid, const CSurvivalFunFacts& funFacts );

#endif // CS_SURVIVAL_FUNFACTS_H