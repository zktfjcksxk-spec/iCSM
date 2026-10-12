// Read-only M3 acceptance observations through the original game interfaces.
#include "cbase.h"
#include "cs_player.h"
#include "cs_gamerules.h"
#include "weapon_csbase.h"
#include "nav_mesh.h"
#include "team.h"
#include "nav_area.h"
#include "econ_item_system.h"
#include <mach-o/dyld.h>
#include <mach/machine.h>
#include <sys/sysctl.h>
#include <cerrno>

CON_COMMAND_F(m3_diagnostics, "Print native modules and current offline game state", FCVAR_RELEASE)
{
    int translated = 0;
    size_t length = sizeof(translated);
    errno = 0;
    int result = sysctlbyname("sysctl.proc_translated", &translated, &length, NULL, 0);
    bool native = (result == 0 && translated == 0) || (result == -1 && errno == ENOENT);
    for (uint32_t i = 0; i < _dyld_image_count(); ++i)
    {
        native = native && _dyld_get_image_header(i)->cputype == CPU_TYPE_ARM64;
        const char *name = _dyld_get_image_name(i);
        if (strncmp(name, "/System/", 8) && strncmp(name, "/usr/lib/", 9))
            Msg("M3_IMAGE arch=%d path=%s\n", _dyld_get_image_header(i)->cputype, name);
    }
    Msg("M3_NATIVE result=%s translated=%d\n", native ? "PASS" : "FAIL", translated);
    if (!gpGlobals || !CSGameRules()) return;
    Msg("M3_STATE map=%s time=%.3f warmup=%d freeze=%d rounds=%d CT=%d T=%d nav_loaded=%d nav_areas=%u items=%d\n",
        STRING(gpGlobals->mapname), gpGlobals->curtime, CSGameRules()->IsWarmupPeriod(), CSGameRules()->IsFreezePeriod(),
        CSGameRules()->GetTotalRoundsPlayed(), GetGlobalTeam(TEAM_CT)->GetScore(), GetGlobalTeam(TEAM_TERRORIST)->GetScore(),
        TheNavMesh->IsLoaded(), TheNavMesh->GetNavAreaCount(), GetItemSchema()->GetItemDefinitionMap().Count());
    for (int i = 1; i <= gpGlobals->maxClients; ++i)
    {
        CCSPlayer *player = ToCSPlayer(UTIL_PlayerByIndex(i));
        if (!player) continue;
        const Vector &p = player->GetAbsOrigin();
        auto weapon = player->GetActiveCSWeapon();
        auto area = player->GetLastKnownArea();
#ifdef SOURCE_NATIVE_METAL
        // Source setpos_player consumes an edict index, not GetUserID().
        Msg("M6_PLAYER_INDEX user_id=%d entity_index=%d\n", player->GetUserID(), player->entindex());
#endif
        Msg("M3_PLAYER id=%d name=%s bot=%d team=%d alive=%d health=%d origin=%.3f,%.3f,%.3f nav=%u weapon=%s\n",
            player->GetUserID(), player->GetPlayerName(), player->IsBot(), player->GetTeamNumber(), player->IsAlive(), player->GetHealth(),
            p.x, p.y, p.z, area ? area->GetID() : 0, weapon ? weapon->GetClassname() : "none");
    }
}
