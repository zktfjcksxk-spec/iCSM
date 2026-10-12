// Included in I5Touch. Input still follows Source's original JoyStickMove,
// SDL controller watcher, Key_Event and Panorama key-binding controls.
#include "controller_defaults.inc"
#include "controller_command_key.inc"
#include "controller_deadzone.inc"
extern "C" size_t ICSMControllerAxesJSON(char *buffer, size_t capacity);
extern "C" bool ICSMControllerTraceActive();
static ConVar icsm_controller_setup_version("icsm_controller_setup_version", "0", FCVAR_ARCHIVE,
    "Version of the original gamepad defaults seeded once for this installation.");
static bool s_controllerDefaultsQueued = false;
static int s_controllerOptionsDisabled = -1;
static bool s_controllerInternalsReady = false;

static void ApplyControllerDefaults(bool internalOnly)
{
    // Release builds hide non-archive response ConVars from console commands
    // (ConVar::Create/Cvar_Command), but ConVarRef can still set them. Retain
    // controller.cfg's original values, including its virtual peg/acceleration.
    const char *start = kICSMControllerDefaults;
    while (*start)
    {
        const char *end = V_strstr(start, "\n");
        const size_t length = end ? size_t(end-start) : V_strlen(start);
        char line[256];
        if (length && length < sizeof(line))
        {
            V_memcpy(line, start, length); line[length] = 0;
            CCommand args; args.Tokenize(line);
            if (args.ArgC() >= 2 && V_strcmp(args[0], "bind"))
            {
                ConVarRef var(args[0], true);
                if (var.IsValid() && (!internalOnly || !var.IsFlagSet(FCVAR_ARCHIVE)))
                    var.SetValue(args[1]);
                else if (!var.IsValid()) Warning("ICSM_CONTROLLER missing original ConVar %s\n", args[0]);
            }
            else if (!internalOnly)
                engine->ClientCmd_Unrestricted(CFmtStr("%s\n", line));
        }
        start += length + (end ? 1 : 0);
    }
}

static void ControllerDefaultsFinished(const CCommand &)
{
    icsm_controller_setup_version.SetValue(1);
    s_controllerDefaultsQueued = false;
    Msg("ICSM_CONTROLLER original gamepad defaults ready\n");
}
static ConCommand icsm_controller_defaults_finished("icsm_controller_defaults_finished", ControllerDefaultsFinished,
    "Finish the local original controller default setup.", FCVAR_DONTRECORD);

static void QueueControllerDefaults(bool reset)
{
    if (!engine || s_controllerDefaultsQueued) return;
    s_controllerDefaultsQueued = true;
    if (reset && g_pInputSystem)
    {
        // Original button-code domain only. Never reset keyboard/mouse binds.
        for (int code = JOYSTICK_FIRST; code <= JOYSTICK_LAST; ++code)
        {
            const char *name = ICSMControllerCommandKey((ButtonCode_t)code);
            if (name && *name) engine->ClientCmd_Unrestricted(CFmtStr("unbind \"%s\"\n", name));
        }
    }
    ApplyControllerDefaults(false);
    if (reset) ResetControllerDeadzones();
    else ApplyControllerDeadzoneSettings();
    engine->ClientCmd_Unrestricted(
        "joystick_force_disabled_set_from_options 0\njoystick_force_disabled 0\njoystick 1\n"
        "joyadvancedupdate silent\nicsm_controller_defaults_finished\nhost_writeconfig\n");
}
static void ControllerResetCommand(const CCommand &) { QueueControllerDefaults(true); }
static ConCommand icsm_controller_reset("icsm_controller_reset", ControllerResetCommand,
    "Restore original controller bindings/settings without resetting keyboard or mouse.", FCVAR_CLIENTCMD_CAN_EXECUTE);

static void SynchronizeControllerSettings()
{
    if (!s_controllerInternalsReady)
    {
        ApplyControllerDefaults(true);
        s_controllerInternalsReady = true;
    }
    if (icsm_controller_setup_version.GetInt() < 1)
    {
        QueueControllerDefaults(false);
        return;
    }
    ApplyControllerDeadzoneSettings();
    ConVarRef options("joystick_force_disabled_set_from_options", true);
    ConVarRef disabled("joystick_force_disabled", true);
    if (!options.IsValid() || !disabled.IsValid()) return;
    const int next = options.GetBool() ? 1 : 0;
    if (s_controllerOptionsDisabled == next) return;
    s_controllerOptionsDisabled = next;
    disabled.SetValue(next);
    in_joystick.SetValue(next ? 0 : 1);
    // Original axis initialization, with the newly loaded/saved settings.
    ::input->Joystick_Advanced(true);
}
