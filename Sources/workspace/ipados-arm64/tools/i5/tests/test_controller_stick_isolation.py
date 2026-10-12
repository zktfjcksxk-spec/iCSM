#!/usr/bin/env python3
"""Exercise actual Source initialization, dead zones and game-axis sampling."""
from pathlib import Path
import re
import subprocess
import tempfile

C = Path.home() / 'Library/Caches/csgo-ipados-arm64/i5'
source = (C / 'source/game/client/in_joystick.cpp').read_text()

def function(name):
    match = re.search(r'^(?:void|float) ' + re.escape(name) + r'\(', source, re.M)
    end = source.index('{', match.start()) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end]

fixture = r'''
#include <cassert>
#include <cmath>
#include <cstring>
#include <cstdio>
#define SOURCE_IOS 1
#define MAX_JOYSTICK_AXES 6
#define MAX_SPLITSCREEN_PLAYERS 1
#define MAX_BUTTONSAMPLE 32768.0f
#define JOY_ABSOLUTE_AXIS 0
#define JOY_RELATIVE_AXIS 16
#define ACTIVE_SPLITSCREEN_PLAYER_GUARD(x)
#define GET_ACTIVE_SPLITSCREEN_SLOT() 0
#define JOYSTICK_AXIS(slot,axis) axis
using DWORD=unsigned int;
using AnalogCode_t=int;
enum {JOY_AXIS_X,JOY_AXIS_Y,JOY_AXIS_Z,JOY_AXIS_R,JOY_AXIS_U,JOY_AXIS_V};
bool IsGameConsole(){return false;} bool IsPC(){return true;}
int Q_stricmp(const char*a,const char*b){return strcmp(a,b);}
void Msg(const char*,...){}
double Plat_FloatTime(){return 100.0;}
struct Var{float v;int GetInt(){return int(v);}float GetFloat(){return v;}bool GetBool(){return v!=0;}void SetValue(float x){v=x;}const char*GetString(){return "Xbox controller";}};
Var joy_name{0},joy_advanced{1},joy_advaxisx{3},joy_advaxisy{1},joy_advaxisz{0},joy_advaxisr{2},joy_advaxisu{4},joy_advaxisv{0};
Var in_joystick{1},joy_xcontroller_cfg_loaded{0},movementStick{0},joy_wwhack2{0},lookstrafe{0},mPitchValue{1};
Var joy_forwardthreshold{.1},joy_sidethreshold{.1},joy_pitchthreshold{.025},joy_yawthreshold{0},invertY{0};
Var *m_pitch=&mPitchValue;
struct SplitScreenConVarRef {Var*var;SplitScreenConVarRef(const char*n):var(!strcmp(n,"joy_movement_stick")?&movementStick:&invertY){}bool GetBool(int){return var->GetBool();}int GetInt(int){return var->GetInt();}bool IsValid(){return true;}};
struct Button {struct User{int state;};User u;User&GetPerUser(int){return u;}};
Button in_jlook{{1}},in_strafe{{0}};
struct InputSystem{int raw[6]={};int GetAnalogValue(int a){return raw[a];}} rawInput;
InputSystem*inputsystem=&rawInput;
struct ICSMControllerAxisState{unsigned generation;int maps[6],controls[6];float sampled[4];};
ICSMControllerAxisState s_controllerAxisState[1]={};
double s_controllerAxisTraceUntil=0,s_controllerAxisTraceNext=0;
struct CInput{
 enum {GAME_AXIS_NONE,GAME_AXIS_FORWARD,GAME_AXIS_PITCH,GAME_AXIS_SIDE,GAME_AXIS_YAW,MAX_GAME_AXES};
 struct joy_axis_t{unsigned AxisFlags=0,AxisMap=0,ControlMap=0;};
 struct User{joy_axis_t m_rgAxes[6];bool m_bForceJoystickRecentering[2]={};}user;
 using PerUserInput_t=User;
 bool m_fJoystickAdvancedInit=false;
 User&GetPerUser(int=-1){return user;}
 void DescribeJoystickAxis(int,const char*,joy_axis_t*){}
 void Joystick_Advanced(bool);
 void JoyStickSampleAxes(float&,float&,float&,float&,bool&,bool&);
 float ScaleAxisValue(float,float);
};
'''
cases = r'''
int main(){
 CInput input;input.Joystick_Advanced(true);
 const int expected[]={3,1,0,2,4,0};
 for(int i=0;i<6;++i)assert(input.user.m_rgAxes[i].AxisMap==expected[i]);
 auto sample=[&](int axis,int value){
   memset(rawInput.raw,0,sizeof(rawInput.raw));rawInput.raw[axis]=value;
   float f,s,p,y;bool ay,ap;input.JoyStickSampleAxes(f,s,p,y,ay,ap);
   assert(ay && ap);
   const float out[]={f,s,p,y};
   const int outputForAxis[]={1,0,-1,2,3,-1};
   for(int i=0;i<4;++i)assert(i==outputForAxis[axis] ? fabsf(out[i])>.9f : out[i]==0);
 };
 for(int axis=0;axis<6;++axis)for(int sign:{-1,1})sample(axis,sign*32767);
 rawInput.raw[JOY_AXIS_X]=21000;rawInput.raw[JOY_AXIS_Y]=-22000;
 rawInput.raw[JOY_AXIS_U]=-23000;rawInput.raw[JOY_AXIS_R]=24000;
 float f,s,p,y;bool ay,ap;input.JoyStickSampleAxes(f,s,p,y,ay,ap);
 assert(f<0 && s>0 && p<0 && y<0);
 // Changing handedness swaps whole sticks, without mixing movement and look.
 movementStick.v=1;input.Joystick_Advanced(true);
 memset(rawInput.raw,0,sizeof(rawInput.raw));rawInput.raw[JOY_AXIS_X]=23000;rawInput.raw[JOY_AXIS_Y]=-24000;
 input.JoyStickSampleAxes(f,s,p,y,ay,ap);assert(f==0 && s==0 && p>0 && y>0);
 memset(rawInput.raw,0,sizeof(rawInput.raw));rawInput.raw[JOY_AXIS_U]=23000;rawInput.raw[JOY_AXIS_R]=-24000;
 input.JoyStickSampleAxes(f,s,p,y,ay,ap);assert(f<0 && s>0 && p==0 && y==0);
 puts("PASS: original CInput initialization and sampling isolate left movement/right look in both directions, simultaneous input and swapped-handed mode; triggers never move or turn");
}
'''
with tempfile.TemporaryDirectory(prefix='icsm-stick-isolation-') as tmp:
    tmp = Path(tmp)
    cpp, binary = tmp/'sticks.cpp', tmp/'sticks'
    cpp.write_text(fixture.replace('#include <cassert>','#include <cassert>\n#include <initializer_list>') +
                   '\n'.join(function(n) for n in ('CInput::Joystick_Advanced','CInput::ScaleAxisValue','CInput::JoyStickSampleAxes')) + cases)
    subprocess.run(['xcrun','clang++','-std=c++17','-fsanitize=address,undefined',str(cpp),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
