#!/usr/bin/env python3
"""Exercise the production deadzone bridge with Source's actual axis scaling."""
from pathlib import Path
import subprocess
import tempfile

P = Path(__file__).resolve().parents[3]
C = Path.home() / 'Library/Caches/csgo-ipados-arm64/i5'
source = (C / 'source/game/client/in_joystick.cpp').read_text()
start = source.index('float CInput::ScaleAxisValue(')
end = source.index('\n\nvoid CInput::Joystick_SetSampleTime', start)
scale = source[start:end]
fixture = r'''
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#define FCVAR_ARCHIVE 128
#define MAX_BUTTONSAMPLE 32768.0f
template<class T>T clamp(T value,T lo,T hi){return std::min(hi,std::max(lo,value));}
struct Var {float value=0;unsigned writes=0;bool valid=true;};
Var thresholds[4];
struct ConVar {
    const char *initial;float value,lo,hi;int flags;
    ConVar(const char*,const char *d,int f,const char*,bool,float l,bool,float h)
        :initial(d),value(atof(d)),lo(l),hi(h),flags(f){}
    float GetFloat(){return value;}
    const char *GetDefault(){return initial;}
    void SetValue(float v){value=clamp(v,lo,hi);}
    void SetValue(const char *v){SetValue(float(atof(v)));}
};
struct ConVarRef {
    Var *var=nullptr;
    ConVarRef(const char *name,bool ignore){Init(name,ignore);}
    void Init(const char *name,bool){
        var=nullptr;
        const char *names[]={"joy_forwardthreshold","joy_sidethreshold","joy_pitchthreshold","joy_yawthreshold"};
        for(int i=0;i<4;++i)if(!strcmp(name,names[i]) && thresholds[i].valid)var=&thresholds[i];
    }
    bool IsValid(){return var!=nullptr;}
    float GetFloat(){return var->value;}
    void SetValue(float v){var->value=v;++var->writes;}
};
bool IsPC(){return true;}
struct CInput {float ScaleAxisValue(float,float);};
'''
cases = r'''
static bool near(float a,float b){return fabsf(a-b)<1e-6f;}
int main(){
    CInput input;
    thresholds[0].valid=false;ApplyControllerDeadzoneSettings();
    for(auto &v:thresholds)assert(v.writes==0);
    thresholds[0].valid=true;
    // Startup and explicit reset keep both axes of a role together.
    ApplyControllerDeadzoneSettings();
    assert(near(thresholds[0].value,.2f) && near(thresholds[1].value,.2f));
    assert(near(thresholds[2].value,.2f) && near(thresholds[3].value,.2f));
    assert(icsm_controller_move_deadzone.flags & FCVAR_ARCHIVE);
    assert(icsm_controller_look_deadzone.flags & FCVAR_ARCHIVE);
    ApplyControllerDeadzoneSettings();
    for(auto &v:thresholds)assert(v.writes==1); // No repeated ConVar writes per frame.
    // Moving one slider cannot change the other stick's thresholds.
    icsm_controller_move_deadzone.SetValue(25.f);ApplyControllerDeadzoneSettings();
    assert(near(thresholds[0].value,.25f) && near(thresholds[1].value,.25f));
    assert(near(thresholds[2].value,.2f) && near(thresholds[3].value,.2f));
    icsm_controller_look_deadzone.SetValue(17.f);ApplyControllerDeadzoneSettings();
    assert(near(thresholds[0].value,.25f) && near(thresholds[1].value,.25f));
    assert(near(thresholds[2].value,.17f) && near(thresholds[3].value,.17f));
    // Test real Source filtering at the center/boundary, both signs and full tilt.
    for(float percent:{0.f,2.5f,10.f,17.f,25.f,50.f}){
        icsm_controller_move_deadzone.SetValue(percent);
        icsm_controller_look_deadzone.SetValue(percent);ApplyControllerDeadzoneSettings();
        for(auto &v:thresholds){
            const float threshold=MAX_BUTTONSAMPLE*v.value;
            assert(input.ScaleAxisValue(0,threshold)==0);
            for(float sign:{-1.f,1.f}){
                assert(input.ScaleAxisValue(sign*threshold,threshold)==0);
                assert(near(input.ScaleAxisValue(sign*(threshold+(MAX_BUTTONSAMPLE-threshold)*.5f),threshold),sign*.5f));
                assert(near(input.ScaleAxisValue(sign*MAX_BUTTONSAMPLE,threshold),sign));
            }
        }
    }
    // Clamp hostile console/config values.
    icsm_controller_move_deadzone.SetValue(-100.f);
    icsm_controller_look_deadzone.SetValue(100.f);ApplyControllerDeadzoneSettings();
    assert(thresholds[0].value==0 && thresholds[1].value==0);
    assert(thresholds[2].value==.5f && thresholds[3].value==.5f);
    ResetControllerDeadzones();
    assert(near(thresholds[0].value,.2f) && near(thresholds[1].value,.2f));
    assert(near(thresholds[2].value,.2f) && near(thresholds[3].value,.2f));
    puts("PASS: production deadzone bridge and Source scaling; independent sticks, center/boundary, continuous range, both signs, endpoints, clamping, late registration and reset");
}
'''
with tempfile.TemporaryDirectory(prefix='icsm-controller-deadzone-') as directory:
    d = Path(directory)
    (d / 'test.cpp').write_text(fixture + '\n#include "controller_deadzone.inc"\n' + scale + cases)
    subprocess.run(['xcrun', 'clang++', '-std=c++17', '-fsanitize=address,undefined',
                    '-I', str(P / 'touch'), str(d / 'test.cpp'), '-o', str(d / 'test')], check=True)
    subprocess.run([str(d / 'test')], check=True)
