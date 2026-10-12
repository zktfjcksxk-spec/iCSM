// Use Source's generic POV names for commands, independent of whether SDL
// currently enables Xbox display aliases (for example during reconnection).
static const char *ICSMControllerCommandKey(ButtonCode_t code)
{
    if (IsJoystickPOVCode(code))
    {
        static const char *names[] = {"POV_UP", "POV_RIGHT", "POV_DOWN", "POV_LEFT"};
        return names[(code - JOYSTICK_FIRST_POV_BUTTON) % JOYSTICK_POV_BUTTON_COUNT];
    }
    return g_pInputSystem->ButtonCodeToString(code);
}
