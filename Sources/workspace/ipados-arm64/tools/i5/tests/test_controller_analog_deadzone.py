#!/usr/bin/env python3
"""Verify the real SDL receiver preserves sub-20% sticks but retains trigger gates."""
from pathlib import Path
import re
import subprocess
import tempfile

P = Path(__file__).resolve().parents[3]
C = Path.home() / 'Library/Caches/csgo-ipados-arm64/i5'
source = (C / 'source/inputsystem/joystick_linux.cpp').read_text()

def function(name):
    match = re.search(r'^(?:void|AnalogCode_t) ' + re.escape(name) + r'\(', source, re.M)
    end = source.index('{', match.start()) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end]

fixture = r'''
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <array>
#include <SDL_gamecontroller.h>
#include "inputsystem/ButtonCode.h"
#include "inputsystem/AnalogCode.h"
#define SOURCE_IOS 1
struct Var{float GetFloat(){return .2f;}}joy_axis_deadzone;
void Warning(const char*,...){}
struct CInputSystem{
    struct JoystickInfo_t{int m_nDeviceId=7;}m_pJoystickInfo[1];
    struct InputState_t{std::array<int,ANALOG_CODE_LAST> m_pAnalogDelta{},m_pAnalogValue{};}m_InputState[2];
    int m_bIsPolling=0,m_nLastSampleTick=0,events=0;
    std::array<bool,BUTTON_CODE_COUNT> buttons{};
    void AxisAnalogButtonEvent(ButtonCode_t code,bool state,int){buttons[code]=state;}
    void PostEvent(int,int,AnalogCode_t,int,int){++events;}
    void JoystickAxisMotion(int,int,int);
};
'''
cases = r'''
int main(){
    CInputSystem input;
    for(int axis:{SDL_CONTROLLER_AXIS_LEFTX,SDL_CONTROLLER_AXIS_LEFTY,SDL_CONTROLLER_AXIS_RIGHTX,SDL_CONTROLLER_AXIS_RIGHTY}){
        const auto code=ControllerAxisToAnalogCode((SDL_GameControllerAxis)axis);
        for(int value:{-32768,-20000,-6500,-3000,-1,0,1,3000,6500,20000,32767}){
            input.JoystickAxisMotion(7,axis,value);
            assert(input.m_InputState[0].m_pAnalogValue[code]==value);
        }
        input.JoystickAxisMotion(7,axis,0);
    }
    // The old fixed threshold remains exclusively for digital stick navigation.
    input.JoystickAxisMotion(7,SDL_CONTROLLER_AXIS_LEFTX,3000);
    assert(!input.buttons[KEY_XSTICK1_LEFT] && !input.buttons[KEY_XSTICK1_RIGHT]);
    input.JoystickAxisMotion(7,SDL_CONTROLLER_AXIS_LEFTX,10000);
    assert(!input.buttons[KEY_XSTICK1_LEFT] && input.buttons[KEY_XSTICK1_RIGHT]);
    input.JoystickAxisMotion(7,SDL_CONTROLLER_AXIS_LEFTX,0);
    assert(!input.buttons[KEY_XSTICK1_LEFT] && !input.buttons[KEY_XSTICK1_RIGHT]);
    for(auto pair:{std::pair<int,ButtonCode_t>{SDL_CONTROLLER_AXIS_TRIGGERLEFT,KEY_XBUTTON_LTRIGGER},
                  std::pair<int,ButtonCode_t>{SDL_CONTROLLER_AXIS_TRIGGERRIGHT,KEY_XBUTTON_RTRIGGER}}){
        input.JoystickAxisMotion(7,pair.first,3000);assert(!input.buttons[pair.second]);
        input.JoystickAxisMotion(7,pair.first,10000);assert(input.buttons[pair.second]);
        input.JoystickAxisMotion(7,pair.first,0);assert(!input.buttons[pair.second]);
    }
    const int events=input.events;
    input.JoystickAxisMotion(99,SDL_CONTROLLER_AXIS_LEFTX,3000);
    input.JoystickAxisMotion(7,-1,3000);
    assert(input.events==events);
    puts("PASS: real Source SDL analog path preserves all four low-amplitude stick axes; original digital directions, trigger cutoff, releases and device/axis guards remain intact");
}
'''
with tempfile.TemporaryDirectory(prefix='icsm-analog-deadzone-') as directory:
    d = Path(directory)
    (d / 'test.cpp').write_text(fixture + function('ControllerAxisToAnalogCode') +
                                function('CInputSystem::JoystickAxisMotion') + cases)
    subprocess.run(['xcrun', 'clang++', '-std=c++17', '-fsanitize=address,undefined',
                    '-DOSX', '-D_OSX', '-DPOSIX', '-D_POSIX', '-DGNUC', '-DCOMPILER_GCC',
                    '-I'+str(P/'vendor/SDL2-2.32.10/include'),
                    '-I'+str(C.parent/'i4/source/public'), str(d/'test.cpp'), '-o', str(d/'test')], check=True)
    subprocess.run([str(d/'test')], check=True)
