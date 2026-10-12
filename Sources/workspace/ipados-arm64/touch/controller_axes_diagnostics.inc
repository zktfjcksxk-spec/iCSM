// Included by the original CInput joystick translation unit. Read-only state
// exposes the effective maps, rather than only the archived configuration.
struct ICSMControllerAxisState
{
    unsigned generation;
    int maps[MAX_JOYSTICK_AXES];
    int controls[MAX_JOYSTICK_AXES];
    float sampled[4]; // forward, side, pitch, yaw after original dead zones
};
static ICSMControllerAxisState s_controllerAxisState[MAX_SPLITSCREEN_PLAYERS] = {};
static double s_controllerAxisTraceUntil = 0;
static double s_controllerAxisTraceNext = 0;
extern "C" bool ICSMControllerTraceActive()
{
    return Plat_FloatTime() < s_controllerAxisTraceUntil;
}
static void ICSMControllerAxisTrace(const CCommand &args)
{
    s_controllerAxisTraceUntil = Plat_FloatTime() + (args.ArgC()>1 && V_atoi(args[1]) ? 15.0 : 0.0);
    s_controllerAxisTraceNext = 0;
}
static ConCommand icsm_controller_axis_trace("icsm_controller_axis_trace", ICSMControllerAxisTrace,
    "Record effective joystick maps and samples for at most 15 seconds.", FCVAR_DONTRECORD);

extern "C" size_t ICSMControllerAxesJSON(char *buffer, size_t capacity)
{
    const ICSMControllerAxisState &s = s_controllerAxisState[0];
    const int n = V_snprintf(buffer,capacity,
        "{\"generation\":%u,\"map\":[%d,%d,%d,%d,%d,%d],"
        "\"sampled_forward_side_pitch_yaw\":[%g,%g,%g,%g]}",
        s.generation,s.maps[0],s.maps[1],s.maps[2],s.maps[3],s.maps[4],s.maps[5],
        s.sampled[0],s.sampled[1],s.sampled[2],s.sampled[3]);
    return n>0 && size_t(n)<capacity ? size_t(n) : 0;
}
