/* Transparent UIKit input only. Panorama still draws/clips every setting
   and owns its vertical scrollbar. No blur, extra texture or render pass. */
#include "settings_slider_policy.h"
static SourceTouchSettingsState settingsScrollState={};
static SourceTouchSettingsSliderTargets settingsSliderTargets={};
static SourceTouchSettingsState settingsScrollAppliedState={};
static CGSize settingsScrollAppliedHostSize=CGSizeZero;
static bool settingsScrollLayoutApplied=false;
static void (*settingsScrollCallback)(const char *,float)=nullptr;
static void (*settingsSliderCallback)(const char *,const char *,float,bool)=nullptr;
static uint64_t settingsScrollSamples=0,settingsScrollTaps=0,settingsSliderDrags=0,settingsScrollCancels=0;
static uint64_t settingsSliderSamples=0,settingsSliderCommits=0;
static std::string settingsLastSlider;
static float settingsLastSliderFraction=0;

enum SettingsTouchIntent { SettingsPending, SettingsVertical, SettingsHorizontalSlider, SettingsIgnore };
static SettingsTouchIntent settingsTouchIntent(CGPoint delta,bool slider,SettingsTouchIntent previous=SettingsPending) {
    return (SettingsTouchIntent)ICSMSettingsTouch::Classify(delta.x,delta.y,slider,(ICSMSettingsTouch::Intent)previous);
}
static bool settingsSliderAtPoint(CGPoint point,SourceTouchSettingsSlider &target,CGRect &track,CGFloat &thumbWidth) {
    if (!host || settingsScrollState.render_width<=0 || settingsScrollState.render_height<=0) return false;
    CGFloat sx=host.bounds.size.width/settingsScrollState.render_width;
    CGFloat sy=host.bounds.size.height/settingsScrollState.render_height;
    const auto &v=settingsScrollState.viewport;
    if (!CGRectContainsPoint(CGRectMake(v.x*sx,v.y*sy,v.width*sx,v.height*sy),point)) return false;
    for (uint32_t i=0;i<settingsSliderTargets.count;++i) {
        const auto &slider=settingsSliderTargets.sliders[i];const auto &r=slider.hit_bounds;
        if (ICSMSettingsTouch::Hit(point.x,point.y,r.x*sx,r.y*sy,r.width*sx,r.height*sy)) {
            target=slider;const auto &t=slider.track_bounds;
            track=CGRectMake(t.x*sx,t.y*sy,t.width*sx,t.height*sy);thumbWidth=slider.thumb_width*sx;return true;
        }
    }
    return false;
}
static void settingsEmitSlider(const char *tab,const char *convar,float fraction,bool finished) {
    if (!settingsSliderCallback || !std::isfinite(fraction)) return;
    fraction=std::clamp(fraction,0.0f,1.0f);
    ++settingsSliderSamples;if(finished)++settingsSliderCommits;
    settingsLastSlider=convar;settingsLastSliderFraction=fraction;
    settingsSliderCallback(tab,convar,fraction,finished);
}

@interface ICSMSettingsContent : UIView
@property(nonatomic,strong) UITouch *primaryTouch;
@property(nonatomic,weak) UIView *pointerHost;
@property(nonatomic) CGPoint origin;
@property(nonatomic) CGFloat maxDistance;
@property(nonatomic) BOOL startedOnSlider;
@property(nonatomic) BOOL sliderDragging;
@property(nonatomic) BOOL grabbedThumb;
@property(nonatomic) SettingsTouchIntent intent;
@property(nonatomic) SourceTouchSettingsSlider slider;
@property(nonatomic) CGRect sliderTrack;
@property(nonatomic) CGFloat sliderThumbWidth;
@property(nonatomic) float lastFraction;
@property(nonatomic,copy) NSString *sliderTab;
-(void)cancelPending;
-(void)changeSliderAtPoint:(CGPoint)point finished:(BOOL)finished;
@end

@interface ICSMSettingsScroll : UIScrollView <UIScrollViewDelegate>
@property(nonatomic) BOOL applyingState;
@property(nonatomic) BOOL animatingScroll;
@property(nonatomic,copy) NSString *tabID;
@property(nonatomic,strong) ICSMSettingsContent *pointerContent;
@end
static ICSMSettingsScroll *settingsScroll;

@implementation ICSMSettingsContent
-(void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    if (self.primaryTouch || !host) return;
    UITouch *touch=touches.anyObject;
    if (touch.type==UITouchTypeIndirectPointer) return;
    self.primaryTouch=touch;self.pointerHost=host;
    self.origin=[touch locationInView:host];self.maxDistance=0;
    SourceTouchSettingsSlider slider={};CGRect track=CGRectZero;CGFloat thumbWidth=0;
    self.startedOnSlider=settingsSliderAtPoint(self.origin,slider,track,thumbWidth);
    self.slider=slider;self.sliderTrack=track;self.sliderThumbWidth=thumbWidth;
    self.lastFraction=slider.fraction;self.sliderTab=[NSString stringWithUTF8String:settingsScrollState.tab_id];
    self.grabbedThumb=self.startedOnSlider && ICSMSettingsTouch::OnThumb(self.origin.x,track.origin.x,track.size.width,thumbWidth,slider.fraction);
    self.sliderDragging=NO;self.intent=SettingsPending;
    // Delay adjustment until a tap ends or horizontal intent is established.
    // Vertical scrolling never generates a Source down or slider change.
}
-(void)changeSliderAtPoint:(CGPoint)p finished:(BOOL)finished {
    SourceTouchSettingsSlider slider=self.slider;CGRect track=self.sliderTrack;
    self.lastFraction=ICSMSettingsTouch::DragFraction(p.x,self.origin.x,track.origin.x,track.size.width,
        self.sliderThumbWidth,slider.fraction,self.grabbedThumb);
    settingsEmitSlider(self.sliderTab.UTF8String,slider.convar,self.lastFraction,finished);
}
-(void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    UITouch *touch=self.primaryTouch;UIView *v=self.pointerHost;
    if (!touch || !v || ![touches containsObject:touch]) return;
    CGPoint p=[touch locationInView:v],delta=CGPointMake(p.x-self.origin.x,p.y-self.origin.y);
    self.maxDistance=std::max(self.maxDistance,std::hypot(delta.x,delta.y));
    self.intent=settingsTouchIntent(delta,self.startedOnSlider,self.intent);
    if (self.intent==SettingsHorizontalSlider) {
        if (!self.sliderDragging){self.sliderDragging=YES;++settingsSliderDrags;}
        [self changeSliderAtPoint:p finished:NO];
    }
}
-(void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    UITouch *touch=self.primaryTouch;UIView *v=self.pointerHost;
    if (!touch || !v || ![touches containsObject:touch]) return;
    CGPoint p=[touch locationInView:v];
    self.maxDistance=std::max(self.maxDistance,std::hypot(p.x-self.origin.x,p.y-self.origin.y));
    NSSet *one=[NSSet setWithObject:touch];
    if (self.sliderDragging) [self changeSliderAtPoint:p finished:YES];
    else if (self.intent!=SettingsVertical && self.maxDistance<=8 && !settingsScroll.dragging && !settingsScroll.decelerating) {
        // Actual UITouch and original SDL coordinate conversion/Source input,
        // rather than synthesising a click or reimplementing widget actions.
        ++settingsScrollTaps;
        if(self.startedOnSlider)[self changeSliderAtPoint:p finished:YES];
        else {[v touchesBegan:one withEvent:event];[v touchesEnded:one withEvent:event];}
    }
    self.primaryTouch=nil;self.pointerHost=nil;self.sliderDragging=NO;self.startedOnSlider=NO;self.intent=SettingsPending;
}
-(void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    if (self.primaryTouch && [touches containsObject:self.primaryTouch]) [self cancelPending];
}
-(void)cancelPending {
    if (!self.primaryTouch) return;
    if(self.sliderDragging){SourceTouchSettingsSlider slider=self.slider;
        settingsEmitSlider(self.sliderTab.UTF8String,slider.convar,self.lastFraction,true);}
    ++settingsScrollCancels;self.primaryTouch=nil;self.pointerHost=nil;self.sliderDragging=NO;self.startedOnSlider=NO;self.intent=SettingsPending;
}
@end

@implementation ICSMSettingsScroll
-(instancetype)initWithFrame:(CGRect)frame {
    if ((self=[super initWithFrame:frame])) {
        self.backgroundColor=UIColor.clearColor;self.opaque=NO;self.clipsToBounds=YES;
        self.delegate=self;self.directionalLockEnabled=YES;self.scrollsToTop=NO;
        self.delaysContentTouches=NO;self.canCancelContentTouches=YES;
        self.showsVerticalScrollIndicator=YES;self.showsHorizontalScrollIndicator=NO;
        self.indicatorStyle=UIScrollViewIndicatorStyleWhite;
        self.contentInsetAdjustmentBehavior=UIScrollViewContentInsetAdjustmentNever;
        self.bounces=NO;self.alwaysBounceVertical=NO;self.alwaysBounceHorizontal=NO;
        self.decelerationRate=UIScrollViewDecelerationRateNormal;
        self.panGestureRecognizer.maximumNumberOfTouches=1;
        self.pointerContent=[[ICSMSettingsContent alloc] initWithFrame:CGRectZero];
        self.pointerContent.backgroundColor=UIColor.clearColor;self.pointerContent.opaque=NO;
        self.pointerContent.multipleTouchEnabled=YES;[self addSubview:self.pointerContent];
        self.accessibilityIdentifier=@"ICSMSettingsScroll";
    }return self;
}
-(BOOL)gestureRecognizerShouldBegin:(UIGestureRecognizer*)recognizer {
    if (recognizer==self.panGestureRecognizer) {
        if(self.pointerContent.intent==SettingsHorizontalSlider)return NO;
        CGPoint delta=[self.panGestureRecognizer translationInView:self];
        if (std::abs(delta.y)<=std::abs(delta.x)) return NO;
    }
    return [super gestureRecognizerShouldBegin:recognizer];
}
-(BOOL)touchesShouldCancelInContentView:(UIView*)view { return YES; }
-(UIView*)hitTest:(CGPoint)point withEvent:(UIEvent*)event {
    // External mice retain the original SDL pointer/wheel path.
    for (UITouch *touch in event.allTouches) if (touch.type==UITouchTypeIndirectPointer) return nil;
    return [super hitTest:point withEvent:event];
}
-(void)scrollViewDidScroll:(UIScrollView*)scrollView {
    if (self.applyingState || self.hidden || !settingsScrollCallback || self.bounds.size.height<=0) return;
    std::lock_guard<std::recursive_mutex> lock(mutex);
    ++settingsScrollSamples;
    settingsScrollCallback(self.tabID.UTF8String,(float)(self.contentOffset.y*settingsScrollState.viewport.height/self.bounds.size.height));
}
-(void)scrollViewDidEndScrollingAnimation:(UIScrollView*)scrollView { self.animatingScroll=NO; }
@end

static void updateSettingsScrollLayout() {
    if (!NSThread.isMainThread || !settingsScroll) return;
    bool show=host && host.window && settingsScrollState.visible && !editing && !consoleVisible &&
        settingsScrollState.render_width>0 && settingsScrollState.render_height>0 &&
        settingsScrollState.viewport.width>0 && settingsScrollState.viewport.height>0;
    CGSize hostSize=host?host.bounds.size:CGSizeZero;
    if (settingsScrollLayoutApplied && show==!settingsScroll.hidden &&
        std::memcmp(&settingsScrollState,&settingsScrollAppliedState,sizeof(settingsScrollState))==0 &&
        CGSizeEqualToSize(hostSize,settingsScrollAppliedHostSize) && (!show || settingsScroll.superview==host)) return;
    settingsScroll.applyingState=YES;
    if (show) {
        NSString *tab=[NSString stringWithUTF8String:settingsScrollState.tab_id]?:@"";
        bool changed=settingsScroll.hidden || ![settingsScroll.tabID isEqualToString:tab];
        CGFloat sx=hostSize.width/settingsScrollState.render_width,sy=hostSize.height/settingsScrollState.render_height;
        const auto &r=settingsScrollState.viewport;
        CGRect frame=CGRectMake(r.x*sx,r.y*sy,r.width*sx,r.height*sy);
        // UIKit derives frame from center/bounds and may differ by a few
        // floating-point bits. Such differences must never cancel a drag.
        CGRect current=settingsScroll.frame;
        bool resized=std::abs(current.origin.x-frame.origin.x)>.25 || std::abs(current.origin.y-frame.origin.y)>.25 ||
            std::abs(current.size.width-frame.size.width)>.25 || std::abs(current.size.height-frame.size.height)>.25;
        if (changed || resized) {
            settingsScroll.animatingScroll=NO;
            [settingsScroll.pointerContent cancelPending];
            settingsScroll.panGestureRecognizer.enabled=NO;settingsScroll.panGestureRecognizer.enabled=YES;
            [settingsScroll setContentOffset:settingsScroll.contentOffset animated:NO];
        }
        if (settingsScroll.superview!=host) [host addSubview:settingsScroll];
        if (resized) settingsScroll.frame=frame;
        CGSize size=CGSizeMake(frame.size.width,std::max(r.height,settingsScrollState.content_height)*sy);
        if (!CGSizeEqualToSize(size,settingsScroll.contentSize)) settingsScroll.contentSize=size;
        CGRect contentFrame=CGRectMake(0,0,size.width,size.height);
        if (!CGRectEqualToRect(settingsScroll.pointerContent.frame,contentFrame)) settingsScroll.pointerContent.frame=contentFrame;
        if (changed || resized || (!settingsScroll.tracking && !settingsScroll.dragging && !settingsScroll.decelerating && !settingsScroll.animatingScroll)) {
            CGFloat offset=std::clamp((CGFloat)settingsScrollState.scroll_offset*sy,(CGFloat)0,std::max((CGFloat)0,size.height-frame.size.height));
            if (std::abs(settingsScroll.contentOffset.y-offset)>.5) [settingsScroll setContentOffset:CGPointMake(0,offset) animated:NO];
        }
        settingsScroll.tabID=tab;
    } else if (!settingsScroll.hidden) {
        settingsScroll.animatingScroll=NO;
        [settingsScroll.pointerContent cancelPending];
        [settingsScroll setContentOffset:settingsScroll.contentOffset animated:NO];
        settingsScroll.panGestureRecognizer.enabled=NO;settingsScroll.panGestureRecognizer.enabled=YES;
    }
    settingsScroll.hidden=!show;settingsScroll.applyingState=NO;
    settingsScrollLayoutApplied=true;settingsScrollAppliedState=settingsScrollState;settingsScrollAppliedHostSize=hostSize;
}

static NSDictionary *settingsScrollDiagnostics() {
    CGRect f=settingsScroll.frame;
    NSMutableArray *targets=[NSMutableArray array];
    for(uint32_t i=0;i<settingsSliderTargets.count;++i){const auto &s=settingsSliderTargets.sliders[i];
        [targets addObject:@{@"convar":[NSString stringWithUTF8String:s.convar]?:@"",@"fraction":@(s.fraction),
            @"track_pixels":@[@(s.track_bounds.x),@(s.track_bounds.y),@(s.track_bounds.width),@(s.track_bounds.height)],@"thumb_width_pixels":@(s.thumb_width)}];}
    return @{@"backend":@"UIScrollView + finger tracks + original Panorama slider events",
        @"visible":@((bool)(settingsScroll && !settingsScroll.hidden)),@"tab":settingsScroll.tabID?:@"",
        @"viewport_points":@[@(f.origin.x),@(f.origin.y),@(f.size.width),@(f.size.height)],
        @"content_height_points":@(settingsScroll.contentSize.height),@"offset_points":@(settingsScroll.contentOffset.y),
        @"source_offset_pixels":@(settingsScrollState.scroll_offset),@"slider_count":@(settingsScrollState.slider_count),
        @"slider_targets":targets,@"slider_samples":@(settingsSliderSamples),@"slider_commits":@(settingsSliderCommits),
        @"last_slider":[NSString stringWithUTF8String:settingsLastSlider.c_str()]?:@"",@"last_slider_fraction":@(settingsLastSliderFraction),
        @"dragging":@(settingsScroll.dragging),@"decelerating":@(settingsScroll.decelerating),
        @"scroll_samples":@(settingsScrollSamples),@"taps":@(settingsScrollTaps),@"slider_drags":@(settingsSliderDrags),@"cancelled_content_touches":@(settingsScrollCancels)};
}
static void shutdownSettingsScroll() {
    std::lock_guard<std::recursive_mutex> lock(mutex);settingsScrollCallback=nullptr;settingsSliderCallback=nullptr;
    settingsScrollState={};settingsSliderTargets={};settingsScrollLayoutApplied=false;
    void (^remove)(void)=^{[settingsScroll.pointerContent cancelPending];[settingsScroll removeFromSuperview];settingsScroll=nil;};
    if (NSThread.isMainThread) remove();else dispatch_async(dispatch_get_main_queue(),remove);
}
extern "C" void SourceTouchSetSettingsScrollCallback(void (*scroll)(const char*,float)) {
    std::lock_guard<std::recursive_mutex> lock(mutex);settingsScrollCallback=scroll;
}
extern "C" void SourceTouchSetSettingsSliderCallback(void (*change)(const char*,const char*,float,bool)) {
    std::lock_guard<std::recursive_mutex> lock(mutex);settingsSliderCallback=change;
}
extern "C" void SourceTouchUpdateSettingsSliderTargets(const SourceTouchSettingsSliderTargets *targets) {
    if(!NSThread.isMainThread || !targets || targets->struct_size!=sizeof(*targets) || targets->count>SOURCE_SETTINGS_SLIDER_LIMIT)return;
    std::lock_guard<std::recursive_mutex> lock(mutex);settingsSliderTargets=*targets;
}
extern "C" bool SourceTouchSettingsSliderSet(const char *convar,float fraction,bool finished) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    if(!NSThread.isMainThread || !settingsScroll || settingsScroll.hidden || !convar || !std::isfinite(fraction) || fraction<0 || fraction>1 || !settingsSliderCallback)return false;
    for(uint32_t i=0;i<settingsSliderTargets.count;++i)if(!std::strcmp(convar,settingsSliderTargets.sliders[i].convar)){
        settingsEmitSlider(settingsScroll.tabID.UTF8String,convar,fraction,finished);return true;}
    return false;
}
extern "C" void SourceTouchUpdateSettings(const SourceTouchSettingsState *settings) {
    if (!NSThread.isMainThread || !settings || settings->struct_size!=sizeof(*settings) || settings->slider_count>SOURCE_SETTINGS_SLIDER_LIMIT) return;
    std::lock_guard<std::recursive_mutex> lock(mutex);settingsScrollState=*settings;
    if (!settingsScroll && settings->visible && host) settingsScroll=[[ICSMSettingsScroll alloc] initWithFrame:CGRectZero];
    updateSettingsScrollLayout();
}
extern "C" bool SourceTouchSettingsScrollTo(float pixels,bool animated) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    if (!NSThread.isMainThread || !settingsScroll || settingsScroll.hidden || !std::isfinite(pixels) || settingsScrollState.viewport.height<=0) return false;
    CGFloat points=pixels*settingsScroll.bounds.size.height/settingsScrollState.viewport.height;
    points=std::clamp(points,(CGFloat)0,std::max((CGFloat)0,settingsScroll.contentSize.height-settingsScroll.bounds.size.height));
    settingsScroll.animatingScroll=animated && std::abs(settingsScroll.contentOffset.y-points)>.5;
    [settingsScroll setContentOffset:CGPointMake(0,points) animated:animated];return true;
}
extern "C" size_t SourceTouchSettingsScrollSelfTestJSON(char *buffer,size_t capacity) {
    if (!buffer || !capacity) return 0;
    bool pass=settingsTouchIntent(CGPointMake(2,2),true)==SettingsPending &&
        settingsTouchIntent(CGPointMake(3,20),true)==SettingsVertical &&
        settingsTouchIntent(CGPointMake(3,-20),false)==SettingsVertical &&
        settingsTouchIntent(CGPointMake(20,3),true)==SettingsHorizontalSlider &&
        settingsTouchIntent(CGPointMake(-20,3),false)==SettingsIgnore;
    NSData *json=[NSJSONSerialization dataWithJSONObject:@{@"passed":@(pass),@"fixtures":@5,
        @"scope":@"direction classifier only; no fabricated touch events or host callbacks",@"runtime":settingsScrollDiagnostics()} options:0 error:nil];
    if (json.length+1>capacity) return 0;std::memcpy(buffer,json.bytes,json.length);buffer[json.length]=0;return json.length;
}
