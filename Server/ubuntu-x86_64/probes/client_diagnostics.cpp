// Read-only M4 observations from the real client and host frame clock.
#include "cbase.h"
#include "c_cs_player.h"
#include "weapon_csbase.h"
#include "igamesystem.h"
#include "tier0/platform.h"
#include <algorithm>
#include <vector>

static std::vector<double> s_frameMilliseconds;
static int s_framesRemaining;
static double s_previousFrame;
static int s_activeFrames;
static int s_lastSampledFrame;
static double s_inputStart;
static Vector s_inputOrigin;
class CM4FrameObserver : public CAutoGameSystemPerFrame
{
public:
    CM4FrameObserver() : CAutoGameSystemPerFrame("M4FrameObserver") {}
    void PostRender() override
    {
        const double now = Plat_FloatTime();
        C_CSPlayer *player = C_CSPlayer::GetLocalCSPlayer();
        if (s_inputStart && player)
        {
            const float moved = (player->GetAbsOrigin() - s_inputOrigin).Length();
            if (moved > 0.01f)
            {
                Msg("M4_INPUT command_to_movement_frame_ms=%.3f distance=%.3f active=%d\n",
                    (now-s_inputStart)*1000, moved, engine->IsActiveApp());
                s_inputStart = 0;
            }
            else if (now-s_inputStart > 3)
            {
                Msg("M4_INPUT no_movement_within_3_seconds\n");
                s_inputStart = 0;
            }
        }
        // Original CViewRender invokes this for the skybox and main view.
        // Count each engine host frame only once.
        if (!s_framesRemaining || gpGlobals->framecount == s_lastSampledFrame) return;
        s_lastSampledFrame = gpGlobals->framecount;
        if (engine->IsActiveApp()) ++s_activeFrames;
        if (s_previousFrame > 0) s_frameMilliseconds.push_back((now - s_previousFrame) * 1000);
        s_previousFrame = now;
        if (--s_framesRemaining) return;
        std::sort(s_frameMilliseconds.begin(), s_frameMilliseconds.end());
        double total = 0;
        for (double value : s_frameMilliseconds) total += value;
        int width, height;
        engine->GetScreenSize(width, height);
        const size_t count = s_frameMilliseconds.size();
        Msg("M4_FRAMES width=%d height=%d count=%zu mean_ms=%.3f p50_ms=%.3f p95_ms=%.3f p99_ms=%.3f max_ms=%.3f active_frames=%d\n",
            width, height, count, total / count, s_frameMilliseconds[count / 2],
            s_frameMilliseconds[(count - 1) * 95 / 100], s_frameMilliseconds[(count - 1) * 99 / 100], s_frameMilliseconds.back(), s_activeFrames);
    }
};
static CM4FrameObserver s_frameObserver;

CON_COMMAND_F(m4_frame_sample, "Observe host frame intervals after rendering; argument is sample count", FCVAR_RELEASE)
{
    const int count = args.ArgC() > 1 ? atoi(args[1]) : 1800;
    s_framesRemaining = clamp(count, 60, 18000) + 1;
    s_previousFrame = 0;
    s_activeFrames = 0;
    s_lastSampledFrame = -1;
    s_frameMilliseconds.clear();
    Msg("M4_FRAME_SAMPLE started count=%d\n", s_framesRemaining - 1);
}

CON_COMMAND_F(m4_client_state, "Print the actual local client player and weapon state", FCVAR_RELEASE)
{
    C_CSPlayer *player = C_CSPlayer::GetLocalCSPlayer();
    if (!player) { Msg("M4_CLIENT no_local_player\n"); return; }
    const Vector &p = player->GetAbsOrigin(), &v = player->GetAbsVelocity();
    const QAngle &a = player->EyeAngles();
    CWeaponCSBase *weapon = player->GetActiveCSWeapon();
    Msg("M4_CLIENT time=%.3f team=%d alive=%d health=%d money=%d origin=%.3f,%.3f,%.3f velocity=%.3f,%.3f,%.3f eye=%.3f,%.3f,%.3f flags=%d weapon=%s clip=%d reserve=%d reload=%d\n",
        gpGlobals->curtime, player->GetTeamNumber(), player->IsAlive(), player->GetHealth(), player->GetAccount(),
        p.x, p.y, p.z, v.x, v.y, v.z, a.x, a.y, a.z, player->GetFlags(),
        weapon ? weapon->GetClassname() : "none", weapon ? weapon->Clip1() : -1,
        weapon ? weapon->GetReserveAmmoCount(AMMO_POSITION_PRIMARY) : -1, weapon ? !!weapon->m_bInReload : 0);
}

// Bind this beside +forward to timestamp an actual input command. The observer
// reads the first resulting client movement frame; it never synthesizes motion.
CON_COMMAND_F(m4_input_mark, "Observe command-to-first-movement-frame time", FCVAR_RELEASE)
{
    C_CSPlayer *player = C_CSPlayer::GetLocalCSPlayer();
    if (!player) return;
    s_inputOrigin = player->GetAbsOrigin();
    s_inputStart = Plat_FloatTime();
    Msg("M4_INPUT_MARK active=%d\n", engine->IsActiveApp());
}
