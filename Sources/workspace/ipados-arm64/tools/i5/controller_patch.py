#!/usr/bin/env python3
"""Adapt the retained Source SDL controller path, without replacing its input logic."""
from pathlib import Path

P = Path(__file__).resolve().parents[2]
C = Path.home() / 'Library/Caches/csgo-ipados-arm64/i5'
BASE = C.parent / 'i4'
FILES = ('inputsystem/joystick_linux.cpp', 'inputsystem/inputsystem.cpp',
         'panorama/input/controller.cpp', 'game/client/in_joystick.cpp',
         'game/client/cstrike15/panorama/popups/csgo_settings_keybinder.cpp')


def once(text, old, new):
    assert text.count(old) == 1, old[:100]
    return text.replace(old, new, 1)


def apply():
    for relative in FILES:
        path = C / 'source' / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        text = (BASE / 'source' / relative).read_text()
        if relative.endswith('joystick_linux.cpp'):
            # Client ScaleAxisValue now owns adjustable stick dead zones.
            # Retain the old input-system cutoff for trigger buttons and
            # digital stick directions, but don't discard analog samples
            # before the per-role slider can apply its own threshold.
            text = once(text, '\tint minValue = joy_axis_deadzone.GetFloat() * 32767;', '''#if defined(SOURCE_IOS)
    const int rawStickValue = value;
    const bool isStick = axis == SDL_CONTROLLER_AXIS_LEFTX || axis == SDL_CONTROLLER_AXIS_LEFTY ||
                         axis == SDL_CONTROLLER_AXIS_RIGHTX || axis == SDL_CONTROLLER_AXIS_RIGHTY;
#endif
\tint minValue = joy_axis_deadzone.GetFloat() * 32767;''')
            text = once(text, '\tInputState_t& state = m_InputState[ m_bIsPolling ];', '''#if defined(SOURCE_IOS)
    if (isStick) value = rawStickValue;
#endif
\tInputState_t& state = m_InputState[ m_bIsPolling ];''')
            # SDL may already be initialized by the launcher. Its Source event
            # watcher must still be installed, once, in that case.
            text = once(text, '\t\tSDL_AddEventWatch(JoystickSDLWatcher, this);', '')
            text = once(text, '\tif ( !SDL_WasInit(SDL_INIT_HAPTIC) )',
                        '\tSDL_DelEventWatch(JoystickSDLWatcher, this);\n'
                        '\tSDL_AddEventWatch(JoystickSDLWatcher, this);\n\n'
                        '\tif ( !SDL_WasInit(SDL_INIT_HAPTIC) )')
            text = once(text,
                        '\t\t\tWarning("Joystick init failed -- SDL_Init(SDL_INIT_HAPTIC) failed: %s.\\n", SDL_GetError());\n\t\t\treturn;',
                        '\t\t\tWarning("Controller vibration unavailable: %s.\\n", SDL_GetError());\n'
                        '\t\t\t// Vibration is optional; retain buttons and both sticks.')
            text = once(text, '\tm_bXController =  true;',
                        '\tm_bXController =  true;\n\tSetInputDeviceConnected(INPUT_DEVICE_GAMEPAD, true);')
            text = once(text, '\tinfo.m_pDevice = controller;', '''
    char *mapping = SDL_GameControllerMapping(controller);
    Msg("ICSM_CONTROLLER SDL mapping: %s\\n", mapping ? mapping : "unavailable");
    SDL_free(mapping);
    Msg("ICSM_CONTROLLER device axes=%d buttons=%d hats=%d\\n",
        SDL_JoystickNumAxes(SDL_GameControllerGetJoystick(controller)),
        SDL_JoystickNumButtons(SDL_GameControllerGetJoystick(controller)),
        SDL_JoystickNumHats(SDL_GameControllerGetJoystick(controller)));
\tinfo.m_pDevice = controller;''')
            # Release only this controller's original Source buttons/axes. A
            # disconnect must not leave an attack or a movement axis held.
            text = once(text, '\tm_nJoystickCount = 0;\n\tm_bXController =  false;', '''
    for (int code = JOYSTICK_FIRST; code <= JOYSTICK_LAST; ++code)
        if (IsButtonDown((ButtonCode_t)code))
            PostButtonReleasedEvent(IE_ButtonReleased, m_nLastSampleTick, (ButtonCode_t)code, (ButtonCode_t)code);
    for (int axis = 0; axis < SDL_CONTROLLER_AXIS_MAX; ++axis)
        JoystickAxisMotion(joystickId, axis, 0);
    SetInputDeviceConnected(INPUT_DEVICE_GAMEPAD, false);
    if (GetCurrentInputDevice() == INPUT_DEVICE_GAMEPAD)
        SetCurrentInputDevice(INPUT_DEVICE_KEYBOARD_MOUSE);
\tm_nJoystickCount = 0;
\tm_bXController =  false;''')
            text = once(text, '\tSDL_HapticClose((SDL_Haptic *)info.m_pHaptic);',
                        '\tif (info.m_pHaptic) SDL_HapticClose((SDL_Haptic *)info.m_pHaptic);')
            text = once(text, "void CInputSystem::PollJoystick( void )\n{\n\t// We don't sample - we get events posted by SDL_GameController in JoystickSDLWatcher", '''void CInputSystem::PollJoystick( void )
{
    // Disabling through the original options releases only controller state.
    // Keyboard/mouse keys remain held; no attack or axis can stick on re-enable.
    static bool wasDisabled = false;
    ConVarRef disabled("joystick_force_disabled", true);
    const bool isDisabled = disabled.IsValid() && disabled.GetBool();
    if (isDisabled && !wasDisabled && m_nJoystickCount > 0)
    {
        for (int code = JOYSTICK_FIRST; code <= JOYSTICK_LAST; ++code)
            if (IsButtonDown((ButtonCode_t)code))
                PostButtonReleasedEvent(IE_ButtonReleased, m_nLastSampleTick, (ButtonCode_t)code, (ButtonCode_t)code);
        for (int axis = 0; axis < SDL_CONTROLLER_AXIS_MAX; ++axis)
            JoystickAxisMotion(m_pJoystickInfo[0].m_nDeviceId, axis, 0);
    }
    wasDisabled = isDisabled;
\t// We don't sample - we get events posted by SDL_GameController in JoystickSDLWatcher''')
            text = once(text, '\tswitch ( event->type )', '''
    // The original options ConVar governs SDL event input as well as polling.
    // Keep releases/hotplug events so disabling never swallows a key-up.
    ConVarRef disabled("joystick_force_disabled", true);
    if (disabled.IsValid() && disabled.GetBool())
    {
        if (event->type == SDL_CONTROLLERBUTTONDOWN) return 1;
        if (event->type == SDL_CONTROLLERAXISMOTION)
        {
            pInputSystem->JoystickAxisMotion(event->caxis.which, event->caxis.axis, 0);
            return 1;
        }
    }
\tswitch ( event->type )''')
            text = once(text, '\tPostButtonPressedEvent(IE_ButtonPressed, m_nLastSampleTick, buttonCode, buttonCode);',
                        '\tif (buttonCode != BUTTON_CODE_NONE)\n'
                        '\t\tPostButtonPressedEvent(IE_ButtonPressed, m_nLastSampleTick, buttonCode, buttonCode);')
            text = once(text, '\tPostButtonReleasedEvent(IE_ButtonReleased, m_nLastSampleTick, buttonCode, buttonCode);',
                        '\tif (buttonCode != BUTTON_CODE_NONE)\n'
                        '\t\tPostButtonReleasedEvent(IE_ButtonReleased, m_nLastSampleTick, buttonCode, buttonCode);')
        elif relative == 'inputsystem/inputsystem.cpp':
            old = '\tif ( m_bXController == false && joystick_force_disabled.IsValid() && joystick_force_disabled.GetBool() == false  )\n\t{\n\t\tPollJoystick();\n\t}'
            text = once(text, old, '''#if defined(SOURCE_IOS)
    // SDL delivers samples through its watcher. PollJoystick also releases
    // held controller state on disable, including for an active Xbox pad.
    PollJoystick();
#else
''' + old + '\n#endif')
        elif relative == 'panorama/input/controller.cpp':
            old = 'sdlbutton < SDL_CONTROLLER_BUTTON_MAX'
            assert text.count(old) == 2
            # SDL 2.0.14 added six button enum values. This 2019 mapping has
            # only the original 15: bounding by the newer enum reads beyond
            # the table and then writes m_appXKeys with an invalid index.
            text = text.replace(old, 'sdlbutton < MIN( (int)ARRAYSIZE(s_nSDLJoyButtonMapping), (int)SDL_CONTROLLER_BUTTON_MAX )')
        elif relative.endswith('/in_joystick.cpp'):
            text = once(text, 'void CInput::Joystick_Advanced( bool bSilent )',
                        '#include "controller_axes_diagnostics.inc"\n\nvoid CInput::Joystick_Advanced( bool bSilent )')
            # Source's automatic desktop cfg load overwrites saved bindings on
            # reconnect and can run before the real axes are mapped. The iOS
            # bridge seeds the original defaults once and owns explicit reset.
            anchor = '#if defined( SWARM_DLL ) || defined( PORTAL )\n\t// Load the xbox controller cfg file'
            text = once(text, anchor,
                        '#if defined(SOURCE_IOS)\n\t// Original defaults are seeded once by controller_settings.inc.\n'
                        '\tjoy_xcontroller_cfg_loaded.SetValue(in_joystick.GetBool());\n#else\n' + anchor)
            text = once(text, '#endif // SWARM_DLL\n}', '#endif // SWARM_DLL\n#endif // SOURCE_IOS\n}')
            text = once(text, '\t}\n\n#if defined(SOURCE_IOS)\n\t// Original defaults', '''
        ICSMControllerAxisState &snapshot = s_controllerAxisState[hh];
        ++snapshot.generation;
        for (int axis=0; axis<MAX_JOYSTICK_AXES; ++axis)
        {
            snapshot.maps[axis] = user.m_rgAxes[axis].AxisMap;
            snapshot.controls[axis] = user.m_rgAxes[axis].ControlMap;
        }
\t}

#if defined(SOURCE_IOS)
\t// Original defaults''')
            text = once(text, '\t\tpitch *= -1.0f;\n\t}\n}', '''\t\tpitch *= -1.0f;
\t}
    ICSMControllerAxisState &snapshot = s_controllerAxisState[nSlot];
    snapshot.sampled[0]=forward; snapshot.sampled[1]=side;
    snapshot.sampled[2]=pitch; snapshot.sampled[3]=yaw;
    const double now = Plat_FloatTime();
    if (now < s_controllerAxisTraceUntil && now >= s_controllerAxisTraceNext)
    {
        s_controllerAxisTraceNext = now + 0.1;
        Msg("ICSM_CONTROLLER_AXES map=%d,%d,%d,%d,%d,%d raw=%d,%d,%d,%d sample=%g,%g,%g,%g\\n",
            snapshot.maps[0],snapshot.maps[1],snapshot.maps[2],snapshot.maps[3],snapshot.maps[4],snapshot.maps[5],
            inputsystem->GetAnalogValue(JOYSTICK_AXIS(nSlot,JOY_AXIS_X)),inputsystem->GetAnalogValue(JOYSTICK_AXIS(nSlot,JOY_AXIS_Y)),
            inputsystem->GetAnalogValue(JOYSTICK_AXIS(nSlot,JOY_AXIS_U)),inputsystem->GetAnalogValue(JOYSTICK_AXIS(nSlot,JOY_AXIS_R)),
            forward,side,pitch,yaw);
    }
}''')
            anchor = '\t// verify joystick is available and that the user wants to use it\n'
            assert text.count(anchor) == 2
            text = text.replace(anchor, anchor + '\tif (joystick_force_disabled.GetBool()) return ')
            # Different return types in the two original functions.
            text = text.replace('return \tif ( !in_joystick', 'return false;\n\tif ( !in_joystick', 1)
            text = text.replace('return \tif ( !in_joystick', 'return;\n\tif ( !in_joystick', 1)
            assert 'return \tif' not in text
        else:
            text = once(text, 'void UnbindKey( ButtonCode_t buttonCode )',
                        '#include "controller_command_key.inc"\n\nvoid UnbindKey( ButtonCode_t buttonCode )')
            text = once(text, '"unbind %s", g_pInputSystem->ButtonCodeToString( buttonCode )',
                        '"unbind \\"%s\\"", ICSMControllerCommandKey( buttonCode )')
            text = once(text, 'g_pInputSystem->ButtonCodeToString( buttonCode ), pszCommand',
                        'ICSMControllerCommandKey( buttonCode ), pszCommand')
            text = once(text, 'bool ActionsAreTheSame( const char *szAction1, const char *szAction2 )',
                        '''// iOS keeps keyboard and controller binding domains independent.
static bool ICSMControllerBinder(panorama::IUIPanel *panel)
{
    for (; panel; panel = panel->GetParent())
        if (!V_strcmp(panel->GetID(), "ControllerSettings")) return true;
    return false;
}

bool ActionsAreTheSame( const char *szAction1, const char *szAction2 )''')
            text = once(text, '\t// Skip "+" in "+attack" and similar commands; keybinding to name system doesn\'t like them', '''
    if (ICSMControllerBinder(UIPanel()))
    {
        const char *name = "—";
        for (int code = JOYSTICK_FIRST; code <= JOYSTICK_LAST; ++code)
        {
            const char *binding = gameuifuncs->GetBindingForButtonCode((ButtonCode_t)code);
            if (binding && ActionsAreTheSame(binding, m_pszBindName))
            { name = g_pInputSystem->ButtonCodeToString((ButtonCode_t)code); break; }
        }
        m_pButton->SetText(name);
        return;
    }
\t// Skip "+" in "+attack" and similar commands; keybinding to name system doesn't like them''')
            text = once(text, '\t\t// uint nModifierFlags = event.m_nData2;', '''
        if (ICSMControllerBinder(sm_pKeyBinderCapturingInput->UIPanel()) &&
            !IsJoystickCode(buttonCode) && buttonCode != ButtonCode_t::KEY_ESCAPE &&
            !(buttonCode == ButtonCode_t::MOUSE_LEFT && s_bMouseOverClearButton))
            return true;
\t\t// uint nModifierFlags = event.m_nData2;''')
            text = once(text, '\t\tCCSGO_SettingsKeyBinder *pInstance = sm_pKeyBinderCapturingInput;', '''
        const bool controllerBind = ICSMControllerBinder(sm_pKeyBinderCapturingInput->UIPanel());
        if (controllerBind && event.m_nData != ButtonCode_t::KEY_ESCAPE && event.m_nData != sm_keyJustBound &&
            !(event.m_nData == ButtonCode_t::MOUSE_LEFT && s_bMouseOverClearButton))
            return true;
\t\tCCSGO_SettingsKeyBinder *pInstance = sm_pKeyBinderCapturingInput;''')
            text = once(text, '\t\tUpdateAllActiveKeybinders();',
                        '\t\tUpdateAllActiveKeybinders();\n'
                        '\t\tif (controllerBind) engine->ClientCmd_Unrestricted("host_writeconfig\\n");')
            text = once(text, '\tsm_bIsAnyKeyBinderCapturingInput = true;',
                        '\tsm_keyJustBound = ButtonCode_t::KEY_NONE;\n\tsm_bIsAnyKeyBinderCapturingInput = true;')
            text = once(text, '\t// Get previous key for this action', '''
    if (ICSMControllerBinder(UIPanel()))
    {
        for (int code = JOYSTICK_FIRST; code <= JOYSTICK_LAST; ++code)
        {
            const char *binding = gameuifuncs->GetBindingForButtonCode((ButtonCode_t)code);
            if (binding && ActionsAreTheSame(binding, m_pszBindName)) UnbindKey((ButtonCode_t)code);
        }
        return;
    }
\t// Get previous key for this action''')
        path.write_text(text)
    print('Original Source SDL controller hotplug, axes and scoped gamepad binding adapted.')


if __name__ == '__main__':
    apply()
