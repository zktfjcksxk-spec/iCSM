#!/usr/bin/env python3
"""Run the actual settings finger router with isolated Foundation touch fixtures."""
from pathlib import Path
import subprocess
import tempfile

P = Path(__file__).resolve().parents[3]
source = (P / 'touch/settings_scroll_ios.inc').read_text()
preamble = source[:source.index('@interface ICSMSettingsContent')]
interfaces = source[source.index('@interface ICSMSettingsContent'):source.index('@implementation ICSMSettingsContent')]
implementation = source[source.index('@implementation ICSMSettingsContent'):source.index('@implementation ICSMSettingsScroll')]
fixture = r'''
#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>
#include "touch_ios.h"
#include <cstring>
#include <string>
#include <vector>
#include <cassert>
#include <cstdio>
@class UITouch, UIEvent;
@interface UIView : NSObject
@property CGRect bounds;
@property unsigned downs,ups;
-(void)touchesBegan:(NSSet*)touches withEvent:(UIEvent*)event;
-(void)touchesEnded:(NSSet*)touches withEvent:(UIEvent*)event;
@end
@implementation UIView
-(void)touchesBegan:(NSSet*)touches withEvent:(UIEvent*)event{++self.downs;}
-(void)touchesEnded:(NSSet*)touches withEvent:(UIEvent*)event{++self.ups;}
@end
@interface UITouch : NSObject
@property CGPoint point;
@property NSInteger type;
-(CGPoint)locationInView:(UIView*)view;
@end
@implementation UITouch
-(CGPoint)locationInView:(UIView*)view{return self.point;}
@end
enum{UITouchTypeIndirectPointer=3};
@protocol UIScrollViewDelegate @end
@interface UIScrollView : UIView
@property BOOL dragging,decelerating;
@end
@implementation UIScrollView @end
static UIView *host;
__PREAMBLE__
__INTERFACES__
@implementation ICSMSettingsScroll @end
__IMPLEMENTATION__
struct Sample{float value;bool finished;};
static std::vector<Sample> samples;
static UITouch *finger;
static ICSMSettingsContent *content;
static void began(double x,double y){
    finger=[UITouch new];finger.point=CGPointMake(x,y);
    [content touchesBegan:[NSSet setWithObject:finger] withEvent:nil];
}
static void moved(double x,double y){
    finger.point=CGPointMake(x,y);[content touchesMoved:[NSSet setWithObject:finger] withEvent:nil];
}
static void ended(){[content touchesEnded:[NSSet setWithObject:finger] withEvent:nil];finger=nil;}
static bool near(double a,double b){return std::abs(a-b)<.000001;}
int main(){@autoreleasepool{
    host=[UIView new];host.bounds=CGRectMake(0,0,1000,700);
    settingsScroll=[ICSMSettingsScroll new];content=[ICSMSettingsContent new];
    settingsScrollState.render_width=2000;settingsScrollState.render_height=1400;
    settingsScrollState.viewport={0,100,2000,1000};
    std::strcpy(settingsScrollState.tab_id,"TouchSettings");
    settingsSliderTargets.count=1;auto &s=settingsSliderTargets.sliders[0];
    s.hit_bounds={200,200,1400,120};s.track_bounds={200,250,1400,16};s.thumb_width=104;s.fraction=.5;
    std::strcpy(s.convar,"icsm_touch_sensitivity");
    settingsSliderCallback=[](const char *tab,const char *name,float value,bool finished){
        assert(!strcmp(tab,"TouchSettings") && !strcmp(name,"icsm_touch_sensitivity"));
        samples.push_back({value,finished});
    };
    // Tapping anywhere on the track adjusts directly, without SDL mouse input.
    began(120,130);assert(content.startedOnSlider);assert(samples.empty());ended();
    assert(samples.size()==1 && samples.back().value==0 && samples.back().finished);
    assert(host.downs==0 && host.ups==0);
    // A thumb press preserves the finger offset and does not jump on contact.
    samples.clear();began(470,130);assert(content.grabbedThumb);ended();
    assert(near(samples.back().value,.5));
    samples.clear();began(470,130);moved(480,131);
    assert(content.sliderDragging && near(samples.back().value,.5+10./648));
    moved(490,250);assert(content.intent==SettingsHorizontalSlider);
    assert(near(samples.back().value,.5+20./648));
    const float value=samples.back().value;[content cancelPending];
    assert(samples.back().finished && samples.back().value==value && !content.primaryTouch);
    assert(host.downs==0 && host.ups==0);
    // A vertical start stays a scroll, even if the finger later drifts sideways.
    samples.clear();began(450,130);moved(452,170);moved(900,190);ended();
    assert(samples.empty() && host.downs==0 && host.ups==0);
    began(450,130);moved(450,137);[content cancelPending];
    assert(samples.empty());
    // Original buttons/numeric entries retain the original pointer tap path.
    began(100,70);assert(!content.startedOnSlider);ended();
    began(900,130);assert(!content.startedOnSlider);ended();
    assert(host.downs==2 && host.ups==2 && samples.empty());
    began(100,70);moved(140,72);ended();
    assert(host.downs==2 && samples.empty());
    // Drag ownership survives leaving the track, with clamped endpoint values.
    began(450,130);moved(1200,132);assert(samples.back().value==1);
    moved(-100,300);assert(samples.back().value==0);ended();
    assert(samples.back().finished && host.downs==2);
    // Another finger cannot move or release the slider's existing owner.
    samples.clear();began(450,130);UITouch *other=[UITouch new];other.point=CGPointMake(700,130);
    [content touchesMoved:[NSSet setWithObject:other] withEvent:nil];
    [content touchesEnded:[NSSet setWithObject:other] withEvent:nil];
    assert(content.primaryTouch==finger && samples.empty());
    moved(460,130);ended();assert(samples.size()==2 && samples.back().finished);
    // Geometry uses logical points and clips hits to the settings viewport.
    SourceTouchSettingsSlider found={};CGRect track=CGRectZero;CGFloat thumb=0;
    assert(settingsSliderAtPoint(CGPointMake(450,130),found,track,thumb));
    assert(track.origin.x==100 && track.size.width==700 && thumb==52);
    s.hit_bounds.y=0;s.hit_bounds.height=200;
    assert(!settingsSliderAtPoint(CGPointMake(450,40),found,track,thumb));
    using namespace ICSMSettingsTouch;
    assert(Hit(110,80,100,90,600,12) && !Hit(110,70,100,90,600,12));
    for(double scale:{.5,1.,2.,3.})
        assert(near(Fraction(450*scale,100*scale,700*scale,52*scale),.5));
    assert(Classify(30,2,true,Vertical)==Vertical && Classify(2,30,true,HorizontalSlider)==HorizontalSlider);
    puts("PASS: actual settings touch methods; track taps, thumb offset, latched horizontal/vertical gestures, cancellation, endpoints, multi-touch, viewport clipping and resolution scaling");
}}
'''
fixture = fixture.replace('__PREAMBLE__', preamble).replace('__INTERFACES__', interfaces).replace('__IMPLEMENTATION__', implementation)
with tempfile.TemporaryDirectory(prefix='icsm-settings-fingers-') as directory:
    path = Path(directory)
    (path / 'test.mm').write_text(fixture)
    subprocess.run(['xcrun', 'clang++', '-x', 'objective-c++', '-std=c++17', '-fobjc-arc',
                    '-fsanitize=address,undefined', '-I', str(P / 'touch'),
                    '-framework', 'Foundation', '-framework', 'CoreGraphics',
                    str(path / 'test.mm'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
