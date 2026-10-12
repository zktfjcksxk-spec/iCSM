// Read-only M5 observations of original server state. No rule mutation.
#include "cbase.h"
#include "cs_player.h"
#include "cs_gamerules.h"
#include "smokegrenade_projectile.h"

CON_COMMAND_F(m5_rules_state, "Observe original timers, armor, flash and grenade entities", FCVAR_RELEASE)
{
    if (!gpGlobals || !CSGameRules()) return;
    Msg("M5_RULES time=%.3f remaining=%.3f round_start=%.3f warmup=%d freeze=%d\n",
        gpGlobals->curtime, CSGameRules()->GetRoundRemainingTime(), CSGameRules()->GetRoundStartTime(),
        CSGameRules()->IsWarmupPeriod(), CSGameRules()->IsFreezePeriod());
    for (int i = 1; i <= gpGlobals->maxClients; ++i)
    {
        auto player = ToCSPlayer(UTIL_PlayerByIndex(i));
        if (!player || player->IsBot()) continue;
        Msg("M5_RULES_PLAYER id=%d team=%d health=%d armor=%d helmet=%d money=%d blind=%d flash_duration=%.3f flash_alpha=%.3f\n",
            player->GetUserID(), player->GetTeamNumber(), player->GetHealth(), player->ArmorValue(),
            bool(player->m_bHasHelmet), player->GetAccountBalance(), player->IsBlind(),
            float(player->m_flFlashDuration), float(player->m_flFlashMaxAlpha));
    }
    for (auto entity = gEntList.FirstEnt(); entity; entity = gEntList.NextEnt(entity))
    {
        const char *name = entity->GetClassname();
        if (!strstr(name, "grenade_projectile") && strcmp(name, "inferno") && strcmp(name, "planted_c4")) continue;
        const auto &p = entity->GetAbsOrigin();
        Msg("M5_RULES_ENTITY id=%d classname=%s origin=%.3f,%.3f,%.3f\n", entity->entindex(), name, p.x, p.y, p.z);
        if (!strcmp(name, "smokegrenade_projectile"))
        {
            auto smoke = static_cast<CSmokeGrenadeProjectile *>(entity);
            Msg("M5_SMOKE id=%d started=%d start_tick=%d\n", entity->entindex(),
                bool(smoke->m_bDidSmokeEffect), int(smoke->m_nSmokeEffectTickBegin));
        }
    }
}
