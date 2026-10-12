#!/usr/bin/env python3
"""Exercise real Source SDL watcher/poll code for disable and hotplug routing."""
from pathlib import Path
import re
import subprocess
import tempfile

source = (Path.home() / 'Library/Caches/csgo-ipados-arm64/i5/source/inputsystem/joystick_linux.cpp').read_text()


def function(name, text=source):
    match = re.search(r'^(?:void|int) ' + re.escape(name) + r'\(', text, re.M)
    start, end = match.start(), text.index('{', match.start()) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


fixture = r'''
#include <cassert>
#include <cstdio>
#include <array>
#define Assert(x) assert(x)
#define SOURCE_IOS 1
#define PLATFORM_POSIX 1
bool ThreadInMainThread(){return true;}
void Warning(const char *,...){}
enum ButtonCode_t { KEY_W=1, JOYSTICK_FIRST=100, PAD_RT=100, PAD_A, JOYSTICK_LAST=199 };
enum {IE_ButtonReleased=2, SDL_CONTROLLER_AXIS_MAX=6, SDL_CONTROLLERAXISMOTION=1,
    SDL_CONTROLLERBUTTONDOWN, SDL_CONTROLLERBUTTONUP, SDL_CONTROLLERDEVICEADDED, SDL_CONTROLLERDEVICEREMOVED};
struct SDL_Event{int type;struct{int which,axis,value;}caxis;struct{int which,button;}cbutton;struct{int which;}cdevice;};
bool optionDisabled=false;
struct ConVarRef {ConVarRef(const char *,bool=false){}bool IsValid(){return true;}bool GetBool(){return optionDisabled;}};
int searches=0;void SearchForDevice(){++searches;}
struct CInputSystem {
    int m_nJoystickCount=1,m_nLastSampleTick=0;struct{int m_nDeviceId=7;}m_pJoystickInfo[1];
    std::array<bool,200> down{};std::array<int,6> axes{};int releases=0,adds=0,removes=0;
    bool IsButtonDown(ButtonCode_t c){return down[c];}
    void PostButtonReleasedEvent(int,int,ButtonCode_t c,ButtonCode_t){down[c]=false;++releases;}
    void JoystickAxisMotion(int id,int axis,int value){if(id==7)axes[axis]=value;}
    void JoystickButtonPress(int id,int b){if(id==7)down[b]=true;}
    void JoystickButtonRelease(int id,int b){if(id==7)down[b]=false;}
    void JoystickHotplugAdded(int){++adds;}
    void JoystickHotplugRemoved(int){++removes;}
    bool m_bXController=true,m_bSteamController=false;
    int ComputeSampleTick(){return 0;}
    bool PollSteamControllers(){return false;}
    void PollJoystick();void SampleDevices();
};
'''
cases = r'''
int main(){
    CInputSystem input;input.down[KEY_W]=input.down[PAD_RT]=input.down[PAD_A]=true;input.axes.fill(21000);
    input.SampleDevices();assert(input.down[PAD_RT] && input.axes[0]==21000);
    optionDisabled=true;input.SampleDevices();
    assert(input.down[KEY_W] && !input.down[PAD_RT] && !input.down[PAD_A] && input.releases==2);
    for(int value:input.axes)assert(value==0);
    input.SampleDevices();assert(input.releases==2);
    SDL_Event event{};event.type=SDL_CONTROLLERBUTTONDOWN;event.cbutton={7,PAD_RT};
    JoystickSDLWatcher(&input,&event);assert(!input.down[PAD_RT]);
    event.type=SDL_CONTROLLERAXISMOTION;event.caxis={7,3,29000};JoystickSDLWatcher(&input,&event);assert(input.axes[3]==0);
    input.down[PAD_A]=true;event.type=SDL_CONTROLLERBUTTONUP;event.cbutton={7,PAD_A};JoystickSDLWatcher(&input,&event);assert(!input.down[PAD_A]);
    event.type=SDL_CONTROLLERDEVICEADDED;JoystickSDLWatcher(&input,&event);assert(input.adds==1);
    event.type=SDL_CONTROLLERDEVICEREMOVED;JoystickSDLWatcher(&input,&event);assert(input.removes==1 && searches==1);
    optionDisabled=false;input.SampleDevices();
    event.type=SDL_CONTROLLERBUTTONDOWN;event.cbutton={7,PAD_RT};JoystickSDLWatcher(&input,&event);assert(input.down[PAD_RT]);
    event.type=SDL_CONTROLLERAXISMOTION;event.caxis={7,3,-12000};JoystickSDLWatcher(&input,&event);assert(input.axes[3]==-12000);
    assert(input.down[KEY_W]);
    puts("PASS: original SDL input routing, disable releases pad buttons/axes and preserves keyboard, disabled releases/hotplug retained, re-enable receives real axis/button values");
}
'''
with tempfile.TemporaryDirectory(prefix='icsm-controller-input-') as directory:
    cpp, binary = Path(directory) / 'input.cpp', Path(directory) / 'input'
    cpp.write_text(fixture + function('JoystickSDLWatcher') + function('CInputSystem::PollJoystick') + function('CInputSystem::SampleDevices', (Path.home() / 'Library/Caches/csgo-ipados-arm64/i5/source/inputsystem/inputsystem.cpp').read_text()) + cases)
    subprocess.run(['xcrun', 'clang++', '-std=c++17', '-fsanitize=address,undefined',
                    str(cpp), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
