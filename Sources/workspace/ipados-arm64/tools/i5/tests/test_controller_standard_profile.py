#!/usr/bin/env python3
"""Verify actual Source mappings ignore extended buttons and preserve Xbox controls."""
from pathlib import Path
import re
import subprocess
import tempfile

P=Path(__file__).resolve().parents[3]
C=Path.home()/'Library/Caches/csgo-ipados-arm64/i5'
BASE=C.parent/'i4'
source=(C/'source/inputsystem/joystick_linux.cpp').read_text()

def function(name):
    match=re.search(r'^(?:ButtonCode_t|AnalogCode_t|void) '+re.escape(name)+r'\(',source,re.M)
    assert match,name
    end=source.index('{',match.start())+1;depth=1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[match.start():end]

fixture=r'''
#include <cstddef>
#include <cassert>
#include <cstdio>
#include <array>
#include <vector>
#include <utility>
#include <SDL_gamecontroller.h>
#include "inputsystem/ButtonCode.h"
#include "inputsystem/AnalogCode.h"
void Warning(const char *,...){}
struct CInputSystem {
    struct JoystickInfo_t {int m_nDeviceId=7;};
    JoystickInfo_t m_pJoystickInfo[1];int m_nLastSampleTick=0;
    std::array<bool,BUTTON_CODE_COUNT> down{};
    std::vector<std::pair<int,ButtonCode_t>> events;
    void PostButtonPressedEvent(int type,int,ButtonCode_t code,ButtonCode_t){down[code]=true;events.push_back({type,code});}
    void PostButtonReleasedEvent(int type,int,ButtonCode_t code,ButtonCode_t){down[code]=false;events.push_back({type,code});}
    void JoystickButtonPress(int,int);void JoystickButtonRelease(int,int);
};
'''
cases=r'''
int main(){
    CInputSystem input;
    const std::pair<SDL_GameControllerButton,ButtonCode_t> standard[]={
        {SDL_CONTROLLER_BUTTON_A,KEY_XBUTTON_A},{SDL_CONTROLLER_BUTTON_B,KEY_XBUTTON_B},
        {SDL_CONTROLLER_BUTTON_X,KEY_XBUTTON_X},{SDL_CONTROLLER_BUTTON_Y,KEY_XBUTTON_Y},
        {SDL_CONTROLLER_BUTTON_BACK,KEY_XBUTTON_BACK},{SDL_CONTROLLER_BUTTON_START,KEY_XBUTTON_START},
        {SDL_CONTROLLER_BUTTON_GUIDE,KEY_XBUTTON_BACK},
        {SDL_CONTROLLER_BUTTON_LEFTSTICK,KEY_XBUTTON_STICK1},{SDL_CONTROLLER_BUTTON_RIGHTSTICK,KEY_XBUTTON_STICK2},
        {SDL_CONTROLLER_BUTTON_LEFTSHOULDER,KEY_XBUTTON_LEFT_SHOULDER},{SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,KEY_XBUTTON_RIGHT_SHOULDER},
        {SDL_CONTROLLER_BUTTON_DPAD_UP,KEY_XBUTTON_UP},{SDL_CONTROLLER_BUTTON_DPAD_DOWN,KEY_XBUTTON_DOWN},
        {SDL_CONTROLLER_BUTTON_DPAD_LEFT,KEY_XBUTTON_LEFT},{SDL_CONTROLLER_BUTTON_DPAD_RIGHT,KEY_XBUTTON_RIGHT}
    };
    for(auto item:standard){
        assert(ControllerButtonToButtonCode(item.first)==item.second);
        size_t before=input.events.size();
        input.JoystickButtonPress(7,item.first);assert(input.down[item.second]);
        input.JoystickButtonRelease(7,item.first);assert(!input.down[item.second]);
        assert(input.events.size()==before+2);
    }
    // Extra controls must not release an already-held standard button either.
    input.JoystickButtonPress(7,SDL_CONTROLLER_BUTTON_A);
    size_t before=input.events.size();
    for(int button=SDL_CONTROLLER_BUTTON_MISC1;button<SDL_CONTROLLER_BUTTON_MAX;++button){
        assert(ControllerButtonToButtonCode((SDL_GameControllerButton)button)==BUTTON_CODE_NONE);
        input.JoystickButtonPress(7,button);input.JoystickButtonRelease(7,button);
        assert(input.events.size()==before && input.down[KEY_XBUTTON_A]);
    }
    input.JoystickButtonRelease(7,SDL_CONTROLLER_BUTTON_A);
    const std::pair<SDL_GameControllerAxis,AnalogCode_t> axes[]={
        {SDL_CONTROLLER_AXIS_LEFTX,JOYSTICK_AXIS(0,JOY_AXIS_X)},
        {SDL_CONTROLLER_AXIS_LEFTY,JOYSTICK_AXIS(0,JOY_AXIS_Y)},
        {SDL_CONTROLLER_AXIS_RIGHTX,JOYSTICK_AXIS(0,JOY_AXIS_U)},
        {SDL_CONTROLLER_AXIS_RIGHTY,JOYSTICK_AXIS(0,JOY_AXIS_R)},
        {SDL_CONTROLLER_AXIS_TRIGGERLEFT,JOYSTICK_AXIS(0,JOY_AXIS_Z)},
        {SDL_CONTROLLER_AXIS_TRIGGERRIGHT,JOYSTICK_AXIS(0,JOY_AXIS_Z)}
    };
    for(auto axis:axes)assert(ControllerAxisToAnalogCode(axis.first)==axis.second);
    puts("PASS: 15 original Xbox buttons and six standard axes retained; six extended SDL buttons produce no press/release events and cannot release held Xbox buttons");
}
'''
with tempfile.TemporaryDirectory(prefix='icsm-standard-controller-') as d:
    d=Path(d);cpp=d/'profile.cpp';binary=d/'profile'
    cpp.write_text(fixture+'\n'.join(function(n) for n in ('ControllerButtonToButtonCode','ControllerAxisToAnalogCode','CInputSystem::JoystickButtonPress','CInputSystem::JoystickButtonRelease'))+cases)
    subprocess.run(['xcrun','clang++','-std=c++17','-fsanitize=address,undefined','-DOSX','-D_OSX','-DPOSIX','-D_POSIX','-DGNUC','-DCOMPILER_GCC','-I'+str(P/'vendor/SDL2-2.32.10/include'),'-I'+str(BASE/'source/public'),str(cpp),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
