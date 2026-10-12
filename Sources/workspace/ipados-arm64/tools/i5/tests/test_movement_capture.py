#!/usr/bin/env python3
"""Run the real touch router against crossing/ownership regression sequences."""
from pathlib import Path
import re
import subprocess
import tempfile

P = Path(__file__).resolve().parents[3]
source = (P / 'touch/touch_ui_ios.mm').read_text()


def function(name):
    match = re.search(r'^static [^\n{]*\b' + re.escape(name) + r'\(', source, re.M)
    assert match, name
    begin = match.start()
    # Each function is short, but nested if blocks still require balanced braces.
    brace = source.index('{', begin)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    result = source[begin:end]
    assert name in result.split('{', 1)[0], result[:120]
    return result


names = ('visible', 'gameplay', 'callMove', 'callLook', 'callAction',
         'contains', 'lookAllowed', 'moveAllowed', 'center', 'cancel',
         'move', 'begin', 'motion', 'end')
functions = '\n'.join(function(name) for name in names)
types = source[source.index('enum Kind'):source.index('static std::recursive_mutex')]
fixture = r'''
#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>
#include "touch_ios.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <vector>
@protocol TCControl
-(void)handleTouchBeganAtPoint:(CGPoint)p;
-(void)handleTouchEndedAtPoint:(CGPoint)p;
@end
__TYPES__
static bool editing=false,focusActive=true,buttonsHidden=false;
static SourceTouchState state{};
static SourceTouchCallbacks callbacks{};
static SourceTouchLookSampleCallback lookSampleCallback=nullptr;
static void *callbackUser=nullptr;
static std::vector<Control> controls;
static std::vector<WeaponInfo> inventory;
static std::map<uintptr_t,Owner> owners;
enum UIKitRoute {SDLRoute,CustomRoute,CancelledRoute};
static std::map<uintptr_t,UIKitRoute> uikitRoutes;
static unsigned actionOwners[SourceTouchActionCount]{};
static uintptr_t moveOwner=0,lookOwner=0;
static CGRect moveRect=CGRectMake(0,0,500,700),lookRect=CGRectMake(500,0,500,700);
static CGFloat moveRadius=64;
static uint64_t lookSamples=0,cancellations=0;
static double lookDX=0,lookDY=0;
static float moveX=0,moveY=0;
__FUNCTIONS__
struct Probe {
    bool pressed[SourceTouchActionCount]{};
    unsigned commandCount=0,weaponCount=0;
    std::vector<std::pair<float,float>> looks;
    struct Sample {float x,y;double timestamp,interval;};
    std::vector<Sample> timedLooks;
} probe;
#define CHECK(condition) do {if(!(condition)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#condition);return 1;}}while(0)
int main(){@autoreleasepool{
    callbacks.action=[](void*,SourceTouchAction a,bool p,double){probe.pressed[a]=p;};
    callbacks.look=[](void*,float x,float y,double){probe.looks.emplace_back(x,y);};
    callbacks.command=[](void*,SourceTouchCommand,double){++probe.commandCount;};
    callbacks.weapon_select=[](void*,int,int,double){++probe.weaponCount;};
    state.mode=SourceTouchModeGameplay;state.active=state.game_input=true;
    controls.push_back({nil,CGRectMake(100,70,240,240),"attack",Held,SourceTouchAttack,true,true});
    controls.push_back({nil,CGRectMake(700,40,80,60),"duck",Held,SourceTouchDuck,false,true});
    controls.push_back({nil,CGRectMake(10,10,40,40),"pause",Command,SourceTouchPause,false,true});
    controls.push_back({nil,CGRectMake(400,600,80,40),"empty_slot",Weapon,1,false,false});
    controls.push_back({nil,CGRectMake(550,600,80,40),"reload",Held,SourceTouchReload,false,true});
    const CGPoint origin=CGPointMake(90,500),fire=center(controls[0]),look=CGPointMake(750,400);

    begin(1,origin,1);motion(1,CGPointMake(90,450),2);
    CHECK(moveOwner==1 && moveY>0);
    // The former bug: merely crossing the fire hit region zeroed movement.
    motion(1,fire,3);
    CHECK(moveOwner==1 && std::hypot(moveX,moveY)>.99f && !probe.pressed[SourceTouchAttack]);
    CHECK(probe.looks.empty() && probe.commandCount==0 && probe.weaponCount==0);
    // Traverse all controls, including disabled targets; retain the same owner.
    double t=4;
    for(const auto &c:controls){motion(1,center(c),t++);CHECK(moveOwner==1 && std::hypot(moveX,moveY)>0);}
    for(int i=0;i<1000;++i){motion(1,CGPointMake(110+i*.3,190+std::sin(i*.1)*60),t++);CHECK(std::hypot(moveX,moveY)>.99f);}
    CHECK(probe.looks.empty() && probe.commandCount==0 && probe.weaponCount==0);
    for(bool held:probe.pressed)CHECK(!held);

    // Four-finger play: firing/releasing and camera input cannot stop the thumb.
    begin(2,fire,t++);CHECK(probe.pressed[SourceTouchAttack]);
    begin(3,look,t++);motion(3,CGPointMake(look.x+12.375,look.y+2.125),t++);
    begin(4,center(controls[1]),t++);CHECK(probe.pressed[SourceTouchDuck]);
    motion(1,fire,t++);float heldX=moveX,heldY=moveY;
    end(2,t++);end(4,t++);
    CHECK(moveOwner==1 && moveX==heldX && moveY==heldY && !probe.pressed[SourceTouchAttack] && !probe.pressed[SourceTouchDuck]);
    CHECK(probe.looks.size()==1 && probe.looks[0].first==12.375f && probe.looks[0].second==2.125f);

    // Crossing the centre/edge never hands the moving finger to the camera.
    motion(1,CGPointMake(950,650),t++);CHECK(moveOwner==1 && std::hypot(moveX,moveY)>.99f && probe.looks.size()==1);
    motion(1,CGPointMake(-10,710),t++);CHECK(moveOwner==1 && std::hypot(moveX,moveY)>.99f);
    motion(1,CGPointMake(90,450),t++);CHECK(moveY>0 && moveX==0);
    begin(5,origin,t++);motion(5,CGPointMake(120,420),t++);end(5,t++);
    CHECK(moveOwner==1 && moveX==0 && moveY>0);
    // Ignore stale coalesced motion; holding still retains the last vector.
    motion(1,origin,1);CHECK(moveY>0);
    for(int frame=0;frame<120;++frame)CHECK(moveY>0);
    end(1,t++);CHECK(moveOwner==0 && moveX==0 && moveY==0 && lookOwner==3);
    motion(3,CGPointMake(look.x+16.375,look.y+2.125),t++);CHECK(probe.looks.back().first==4);
    end(3,t++);

    // Empty corners of the circular control remain valid move touch-downs.
    CGPoint corner=CGPointMake(102,72);CHECK(CGRectContainsPoint(controls[0].rect,corner) && !contains(controls[0],corner));
    begin(6,corner,t++);motion(6,CGPointMake(104,72),t++);CHECK(moveOwner==6 && moveX>0);end(6,t++);
    // Button-origin fingers and disabled slots never become movement/look.
    const auto looks=probe.looks.size();
    begin(7,fire,t++);motion(7,origin,t++);motion(7,look,t++);
    CHECK(moveOwner==0 && moveX==0 && moveY==0 && probe.looks.size()==looks && probe.pressed[SourceTouchAttack]);end(7,t++);
    begin(8,center(controls[3]),t++);motion(8,origin,t++);CHECK(moveOwner==0 && moveX==0 && moveY==0);end(8,t++);

    // Modal transitions / real cancellation still stop movement immediately.
    begin(9,origin,t++);motion(9,fire,t++);state.game_input=false;motion(9,fire,t++);CHECK(moveX==0 && moveY==0);cancel(t++);
    state.game_input=true;begin(10,origin,t++);motion(10,fire,t++);begin(11,fire,t++);cancel(t++);
    CHECK(owners.empty() && moveOwner==0 && lookOwner==0 && moveX==0 && moveY==0 && !probe.pressed[SourceTouchAttack]);
    // Timed routing replaces legacy delivery and preserves actual sample time.
    lookSampleCallback=[](void*,float x,float y,double time,double interval){probe.timedLooks.push_back({x,y,time,interval});};
    const auto legacyCount=probe.looks.size();
    begin(12,look,10000);motion(12,CGPointMake(760,402),10000.01);
    CHECK(probe.timedLooks.size()==1 && probe.looks.size()==legacyCount);
    CHECK(probe.timedLooks.back().x==10 && probe.timedLooks.back().y==2);
    CHECK(std::abs(probe.timedLooks.back().interval-.01)<1e-10);
    // Crossing an excluded button and re-entering changes the baseline only.
    motion(12,center(controls[1]),10000.02);motion(12,look,10000.03);
    CHECK(probe.timedLooks.size()==1);
    motion(12,CGPointMake(755,400),10000.04);
    CHECK(probe.timedLooks.size()==2 && probe.timedLooks.back().x==5);
    CHECK(std::abs(probe.timedLooks.back().interval-.01)<1e-10);
    motion(12,CGPointMake(790,400),10000.035); // stale coalesced sample
    CHECK(probe.timedLooks.size()==2);
    motion(12,CGPointMake(757,400),10000.05);
    CHECK(probe.timedLooks.size()==3 && probe.timedLooks.back().x==2);
    CHECK(std::abs(probe.timedLooks.back().interval-.01)<1e-10);
    end(12,10000.06);
    begin(13,origin,10001);motion(13,fire,10001.01);motion(13,look,10001.02);end(13,10001.03);
    begin(14,fire,10002);motion(14,look,10002.01);end(14,10002.02);
    CHECK(probe.timedLooks.size()==3 && probe.looks.size()==legacyCount);
    puts("captured movement / 1,000 crossing samples / four fingers / unchanged camera / immediate release: passed");
    puts("timed camera samples / exclusion re-entry / stale samples / single callback delivery: passed");
    return 0;
    // Hidden buttons have no invisible hit targets; swipe input still works.
    cancel(10001);buttonsHidden=true;
    const auto hiddenCommands=probe.commandCount,hiddenWeapons=probe.weaponCount;
    const auto hiddenLooks=probe.timedLooks.size();
    begin(20,fire,10002);motion(20,CGPointMake(fire.x+10,fire.y),10002.01);
    CHECK(moveOwner==20 && moveX>0 && !probe.pressed[SourceTouchAttack]);
    end(20,10002.02);
    begin(21,center(controls[1]),10003);motion(21,CGPointMake(750,70),10003.01);
    CHECK(lookOwner==21 && probe.timedLooks.size()==hiddenLooks+1 && !probe.pressed[SourceTouchDuck]);
    end(21,10003.02);
    CHECK(probe.commandCount==hiddenCommands && probe.weaponCount==hiddenWeapons);
    cancel(10004);buttonsHidden=false;
    begin(22,fire,10005);CHECK(probe.pressed[SourceTouchAttack]);end(22,10005.01);
    CHECK(!probe.pressed[SourceTouchAttack]);
}}
'''.replace('__TYPES__', types)

with tempfile.TemporaryDirectory(prefix='icsm-move-capture-') as directory:
    path = Path(directory)
    old = functions.replace('if(!gameplay()){callMove(0,0,t);return;}',
                            'if(!moveAllowed(p)){callMove(0,0,t);return;}')
    assert old != functions
    for label, body in (('before', old), ('after', functions)):
        (path / 'test.mm').write_text(fixture.replace('__FUNCTIONS__', body))
        subprocess.run(['xcrun', 'clang++', '-x', 'objective-c++', '-std=c++17', '-fobjc-arc',
                        '-fsanitize=address,undefined', '-I', str(P / 'touch'),
                        '-framework', 'Foundation', '-framework', 'CoreGraphics',
                        str(path / 'test.mm'), '-o', str(path / 'test')], check=True)
        result = subprocess.run([str(path / 'test')], capture_output=True, text=True)
        if label == 'before':
            assert result.returncode == 1 and 'std::hypot(moveX,moveY)>.99f' in result.stderr, result
            print('Former router: crossing the fire button reproduces movement stop.', flush=True)
        else:
            assert result.returncode == 0, result.stderr
            print(result.stdout.strip(), flush=True)
