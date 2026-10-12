// Rendering-only fixture using the original smoke particle creation path.
// Reference: game/shared/cstrike15/smokegrenade_projectile.cpp:51-85.
#include "cbase.h"
#include "particles_new.h"

CON_COMMAND_F(m6_render_smoke, "Render original smoke particles at x y z; no grenade simulation", FCVAR_CHEAT)
{
    if (args.ArgC() != 4) { Msg("usage: m6_render_smoke x y z\n"); return; }
    const Vector origin(atof(args[1]), atof(args[2]), atof(args[3]));
    CNewParticleEffect *effect = CNewParticleEffect::CreateOrAggregate(NULL, "explosion_smokegrenade", origin);
    if (!effect) { Warning("M6_PARTICLE original smoke definition unavailable\n"); return; }
    effect->SetSortOrigin(origin);
    effect->SetControlPoint(0, origin);
    effect->SetControlPoint(1, origin);
    effect->SetControlPointOrientation(0, Vector(1, 0, 0), Vector(0, -1, 0), Vector(0, 0, 1));
    Msg("M6_PARTICLE effect=explosion_smokegrenade origin=%.3f,%.3f,%.3f\n", origin.x, origin.y, origin.z);
}
