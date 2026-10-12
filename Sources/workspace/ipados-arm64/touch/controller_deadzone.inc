// Source's ScaleAxisValue owns filtering and full-range remapping. These
// archived percentages only group its horizontal/vertical thresholds by role.
// Previous SDL filtering discarded all stick samples below 20%. Keep that
// activation threshold by default while replacing its discontinuous cutoff.
static ConVar icsm_controller_move_deadzone("icsm_controller_move_deadzone", "20", FCVAR_ARCHIVE,
    "Movement stick inner dead zone in percent; follows the movement-stick selection.", true, 0, true, 50);
static ConVar icsm_controller_look_deadzone("icsm_controller_look_deadzone", "20", FCVAR_ARCHIVE,
    "Look stick inner dead zone in percent; follows the look-stick selection.", true, 0, true, 50);

static void ApplyControllerDeadzoneSettings()
{
    static ConVarRef forward("joy_forwardthreshold", true), side("joy_sidethreshold", true);
    static ConVarRef pitch("joy_pitchthreshold", true), yaw("joy_yawthreshold", true);
    if (!forward.IsValid()) forward.Init("joy_forwardthreshold", true);
    if (!side.IsValid()) side.Init("joy_sidethreshold", true);
    if (!pitch.IsValid()) pitch.Init("joy_pitchthreshold", true);
    if (!yaw.IsValid()) yaw.Init("joy_yawthreshold", true);
    if (!forward.IsValid() || !side.IsValid() || !pitch.IsValid() || !yaw.IsValid()) return;
    const float move = clamp(icsm_controller_move_deadzone.GetFloat(), 0.0f, 50.0f) * 0.01f;
    const float look = clamp(icsm_controller_look_deadzone.GetFloat(), 0.0f, 50.0f) * 0.01f;
    if (forward.GetFloat() != move) forward.SetValue(move);
    if (side.GetFloat() != move) side.SetValue(move);
    if (pitch.GetFloat() != look) pitch.SetValue(look);
    if (yaw.GetFloat() != look) yaw.SetValue(look);
}

static void ResetControllerDeadzones()
{
    icsm_controller_move_deadzone.SetValue(icsm_controller_move_deadzone.GetDefault());
    icsm_controller_look_deadzone.SetValue(icsm_controller_look_deadzone.GetDefault());
    ApplyControllerDeadzoneSettings();
}
