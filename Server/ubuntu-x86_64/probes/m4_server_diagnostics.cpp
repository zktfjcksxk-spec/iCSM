// Read-only M4 observations; no inventory, health, position or rule mutation.
#include "cbase.h"
#include "cs_player.h"
#include "cs_gamerules.h"
#include "weapon_csbase.h"
#include "team.h"

CON_COMMAND_F(m4_game_state, "Print actual human server state and bomb/grenade entities", FCVAR_RELEASE)
{
    if (!gpGlobals || !CSGameRules()) return;
    Msg("M4_GAME time=%.3f warmup=%d freeze=%d rounds=%d CT=%d T=%d bomb_planted=%d\n", gpGlobals->curtime,
        CSGameRules()->IsWarmupPeriod(), CSGameRules()->IsFreezePeriod(), CSGameRules()->GetTotalRoundsPlayed(),
        GetGlobalTeam(TEAM_CT)->GetScore(), GetGlobalTeam(TEAM_TERRORIST)->GetScore(), !!CSGameRules()->m_bBombPlanted);
    for (int i = 1; i <= gpGlobals->maxClients; ++i)
    {
        CCSPlayer *player = ToCSPlayer(UTIL_PlayerByIndex(i));
        if (!player) continue;
        if (player->IsBot())
        {
            if (!player->IsAlive())
            {
                const CUserCmd *cmd = player->GetLastUserCommand();
                const Vector &p = player->GetAbsOrigin(), &v = player->GetAbsVelocity();
                Msg("M4_DEAD_BOT id=%d observer=%d movetype=%d upmove=%.3f origin=%.3f,%.3f,%.3f velocity=%.3f,%.3f,%.3f\n",
                    player->GetUserID(), player->GetObserverMode(), player->GetMoveType(), cmd ? cmd->upmove : 0,
                    p.x, p.y, p.z, v.x, v.y, v.z);
            }
            continue;
        }
        const Vector &p = player->GetAbsOrigin(), &v = player->GetAbsVelocity();
        auto weapon = player->GetActiveCSWeapon();
        Msg("M4_HUMAN id=%d team=%d alive=%d health=%d money=%d buyzone=%d defuser=%d origin=%.3f,%.3f,%.3f velocity=%.3f,%.3f,%.3f flags=%d weapon=%s clip=%d reserve=%d reload=%d\n",
            player->GetUserID(), player->GetTeamNumber(), player->IsAlive(), player->GetHealth(), player->GetAccountBalance(),
            player->IsInBuyZone(), player->HasDefuser(), p.x, p.y, p.z, v.x, v.y, v.z, player->GetFlags(),
            weapon ? weapon->GetClassname() : "none", weapon ? weapon->Clip1() : -1,
            weapon ? weapon->GetReserveAmmoCount(AMMO_POSITION_PRIMARY) : -1, weapon ? !!weapon->m_bInReload : 0);
        for (int slot = 0; slot < player->WeaponCount(); ++slot)
        {
            auto item = player->GetWeapon(slot);
            if (item) Msg("M4_INVENTORY id=%d slot=%d weapon=%s clip=%d\n", player->GetUserID(), slot, item->GetClassname(), item->Clip1());
        }
    }
    for (CBaseEntity *entity = gEntList.FirstEnt(); entity; entity = gEntList.NextEnt(entity))
    {
        const char *name = entity->GetClassname();
        if (!strstr(name, "grenade_projectile") && strcmp(name, "planted_c4")) continue;
        const Vector &p = entity->GetAbsOrigin();
        Msg("M4_ENTITY id=%d classname=%s origin=%.3f,%.3f,%.3f\n", entity->entindex(), name, p.x, p.y, p.z);
    }
}
