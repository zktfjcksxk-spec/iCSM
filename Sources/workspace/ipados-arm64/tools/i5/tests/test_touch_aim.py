#!/usr/bin/env python3
"""Check actual aim policy, Source application and scope selection in isolation."""
from pathlib import Path
import re
import subprocess
import tempfile
import xml.etree.ElementTree as ET

P = Path(__file__).resolve().parents[3]
source = (P / 'touch/touch_source_bridge.inc').read_text()


def function(name):
    match = re.search(r'^static [^\n{]*\b' + name + r'\(', source, re.M)
    start, end = match.start(), source.index('{', match.start()) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


fixture = r'''
#include <cassert>
#include <cstdio>
#include <limits>
#include <algorithm>
#include <cmath>
// Source's public headers leave these macros enabled at the bridge include.
#define min(a,b) (((a)<(b))?(a):(b))
#define max(a,b) (((a)>(b))?(a):(b))
#include "touch_aim_policy.h"
#undef min
#undef max
constexpr int PITCH=0,YAW=1,WEAPON_AUG=8,WEAPON_SG556=39,WEAPONTYPE_SNIPER_RIFLE=5;
struct C_WeaponCSBase{int id=0,type=0;int GetCSWeaponID(){return id;}int GetWeaponType(){return type;}};
struct C_CSPlayer{
    bool m_bIsScoped=false;C_WeaponCSBase *weapon=nullptr;static C_CSPlayer *local;
    static C_CSPlayer *GetLocalCSPlayer(){return local;}
    C_WeaponCSBase *GetActiveCSWeapon(){return weapon;}
};
C_CSPlayer *C_CSPlayer::local=nullptr;
struct Var{float value;float GetFloat(){return value;}int GetInt(){return int(value);}bool GetBool(){return value!=0;}};
Var icsm_touch_aim_mode{0},icsm_touch_sensitivity{1},icsm_touch_rifle_scope_sensitivity{1},icsm_touch_sniper_scope_sensitivity{1};
Var touch_yaw_degrees_per_point{.25f},touch_pitch_degrees_per_point{.25f},touch_input_debug{0};
Var cl_forwardspeed{250},cl_backspeed{250},cl_sidespeed{250},cl_pitchup{89},cl_pitchdown{89};
struct QAngle{float v[3]{};float &operator[](int i){return v[i];}};
struct CUserCmd{float forwardmove=0,sidemove=0;int buttons=0;};
struct Engine{QAngle angles;void GetViewAngles(QAngle &v){v=angles;}void SetViewAngles(const QAngle &v){angles=v;}} engineObject;
Engine *engine=&engineObject;
struct View{void StopPitchDrift(){}} viewObject;View *view=&viewObject;
bool s_initialized=true,s_gameInput=true;
float s_moveX=0,s_moveY=0;double s_yawTotal=0,s_pitchTotal=0;unsigned s_lookSamples=0,s_lookCount=0;
struct LookSample{float x,y;double timestamp,interval;};LookSample s_look[1024];
ICSMTouchAim::Delta s_lastAim={0,0,1,1};
template<class T>T clamp(T v,T a,T b){return std::max(a,std::min(b,v));}
void PublishState(){}void PumpEvents(){}void Msg(const char *,...){}
'''
cases = r'''
bool near(double a,double b){return std::abs(a-b)<.0003;}
int main(){
    using namespace ICSMTouchAim;
    Settings settings{0,1,1,1};
    for(int hz:{60,90,120,240})for(double speed:{60.,420.,960.}){
        double yaw=0;const int count=hz;
        for(int i=0;i<count;++i)yaw+=Apply(float(speed/hz),0,1./hz,.25,.25,settings,Unscoped).yaw;
        assert(near(yaw,-speed*.25));
    }
    settings.mode=1;
    for(int hz:{60,90,120,240})for(double speed:{60.,420.,960.}){
        double yaw=0;for(int i=0;i<hz;++i)yaw+=Apply(float(speed/hz),0,1./hz,.25,.25,settings,Unscoped).yaw;
        const double gain=speed==60?1:speed==420?1.5:2;
        assert(near(yaw,-speed*.25*gain));
    }
    auto slow=Apply(1,0,1./60,.25,.25,settings,Unscoped);
    auto fast=Apply(16,0,1./60,.25,.25,settings,Unscoped);
    assert(slow.acceleration==1 && fast.acceleration==2);
    assert(Apply(1,0,1./60,.25,.25,settings,Unscoped).acceleration==1); // no carried velocity
    for(double dt:{0.,-1.,1.,std::numeric_limits<double>::quiet_NaN()})
        assert(Apply(1,0,dt,.25,.25,settings,Unscoped).acceleration==1);
    assert(Apply(std::numeric_limits<float>::quiet_NaN(),1,.01,.25,.25,settings,Unscoped).yaw==0);
    auto diagonal=Apply(3,4,.01,.25,.25,settings,Unscoped);
    assert(near(diagonal.acceleration,Apply(5,0,.01,.25,.25,settings,Unscoped).acceleration));
    C_CSPlayer player;C_WeaponCSBase weapon;C_CSPlayer::local=&player;player.weapon=&weapon;
    CUserCmd cmd;
    icsm_touch_sensitivity.value=2;icsm_touch_rifle_scope_sensitivity.value=.5;icsm_touch_sniper_scope_sensitivity.value=.25;
    for(int id:{WEAPON_AUG,WEAPON_SG556,9,40,11,38,7})for(bool scoped:{false,true}){
        weapon.id=id;weapon.type=(id==9 || id==40 || id==11 || id==38)?WEAPONTYPE_SNIPER_RIFLE:0;
        player.m_bIsScoped=scoped;
        Scope expected=!scoped?Unscoped:weapon.type==WEAPONTYPE_SNIPER_RIFLE?SniperScope:
            id==WEAPON_AUG || id==WEAPON_SG556?RifleScope:Unscoped;
        assert(AimScope()==expected);
        for(int mode:{0,1}){
            icsm_touch_aim_mode.value=mode;
            auto reference=Apply(6,3,.01,.25,.25,AimSettings(),expected);
            for(bool firing:{false,true}){
                cmd.buttons=firing?1:0; // Actual Source IN_ATTACK bit, same ControllerMove path.
                engine->angles={};s_look[0]={6,3,20,.01};s_lookCount=1;
                ControllerMove(0,&cmd);
                assert(near(engine->angles[YAW],reference.yaw) && near(engine->angles[PITCH],reference.pitch));
                assert(!s_lookCount);
            }
        }
    }
    // Real Source processing must give the same answer with all samples queued
    // in one frame or split across frames, even with a different scope multiplier.
    weapon.id=WEAPON_AUG;weapon.type=0;player.m_bIsScoped=true;
    for(int frames:{1,10,60,120}){
        engine->angles={};
        for(int frame=0;frame<frames;++frame){
            for(int i=0;i<120/frames;++i)s_look[i]={3.5f,0,0,1./120};
            s_lookCount=120/frames;ControllerMove(0,&cmd);
        }
        assert(near(engine->angles[YAW],-420*.25*1.5));
    }
    player.weapon=nullptr;assert(AimScope()==Unscoped);
    C_CSPlayer::local=nullptr;assert(AimScope()==Unscoped);
    puts("PASS: fixed/velocity modes, 60/90/120/240-Hz samples, frame batching, AUG/SG553/four sniper types, scope exit, shared fire sensitivity, invalid/paused samples, no velocity history");
}
'''
xml = ET.parse(P / 'app/i5/resources/native-ui/panorama/layout/settings/settings_touch.xml')
bindings = {panel.attrib['convar'] for panel in xml.iter() if 'convar' in panel.attrib}
for slider in xml.iter('CSGOSettingsSlider'):
    # The original Source numeric entry accepts raw ConVar values. Avoid its
    # percentage presentation, which would show a different unit than it parses.
    assert slider.attrib['percentage'] == 'false' and slider.attrib['displayprecision'] == '2'
for name in ('icsm_touch_aim_mode', 'icsm_touch_sensitivity', 'icsm_touch_rifle_scope_sensitivity', 'icsm_touch_sniper_scope_sensitivity'):
    assert name in bindings
    assert re.search(r'static ConVar ' + name + r'\(.*?FCVAR_ARCHIVE', source, re.S)
with tempfile.TemporaryDirectory(prefix='icsm-touch-aim-') as directory:
    cpp, binary = Path(directory) / 'aim.cpp', Path(directory) / 'aim'
    cpp.write_text(fixture + '\n'.join(function(name) for name in ('AimSettings', 'AimScope', 'ControllerMove')) + cases)
    subprocess.run(['xcrun', 'clang++', '-std=c++17', '-fsanitize=address,undefined',
                    '-I' + str(P / 'touch'), str(cpp), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
