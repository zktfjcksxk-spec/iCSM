#include "touch_ios.h"
#import <UIKit/UIKit.h>
#import <TouchController/TouchController.h>
#import <CoreText/CoreText.h>
#import <QuartzCore/CAMetalLayer.h>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include "SDL_events.h"
#include "SDL_scancode.h"
// Internal SDL extension compiled into this same framework. It uses a source
// bit distinct from physical keys and the system text-input keyboard.
extern "C" int SDL_SendTouchKeyboardKey(Uint8 state,SDL_Scancode scancode);

#if !__has_feature(objc_arc)
#error touch_ui_ios.mm must be compiled with -fobjc-arc
#endif

static void updateGlassNavigationLayout();
static void shutdownGlassNavigation();
static void updateMapGalleryLayout();
static void shutdownMapGallery();
static void shutdownSettingsScroll();
static void shutdownConsoleHeader();
static void updateInventoryLayout();
static void shutdownInventory();
static void trimInventoryMemory();

namespace {
enum Kind { Held, Weapon, Command, Keyboard };
struct Control {
    __strong id<TCControl> native;
    CGRect rect;
    std::string label;
    Kind kind;
    int value;
    bool circle, enabled;
    unsigned owners=0;
};
struct Owner {
    enum Type { Ignored, ControlOwner, LookOwner, MoveOwner, KeyboardOwner } type=Ignored;
    size_t control=0;
    int keyboard=0;
    CGPoint last=CGPointZero;
    CGPoint origin=CGPointZero;
    double timestamp=0;
    bool inside=false;
};
struct WeaponInfo {
    int slot, entity, subtype;
    bool selected;
    std::string name;
    bool operator==(const WeaponInfo &o) const {
        return slot==o.slot && entity==o.entity && subtype==o.subtype && selected==o.selected && name==o.name;
    }
};
static std::recursive_mutex mutex;
static __weak UIView *host;
static __strong TCTouchController *controller;
static __strong id<MTLDevice> metalDevice;
static MTLPixelFormat pixelFormat=MTLPixelFormatBGRA8Unorm;
static CGSize viewSize=CGSizeZero,drawableSize=CGSizeZero;
static UIEdgeInsets safe=UIEdgeInsetsZero;
static CGFloat displayScale=1,screenLandscapePoints=0;
static std::atomic<float> menuUIScale{1.0f};
static SourceTouchState state={};
static SourceTouchCallbacks callbacks={};
static void *callbackUser=nullptr;
static std::vector<WeaponInfo> inventory;
static std::vector<Control> controls;
static std::map<uintptr_t,Owner> owners;
enum UIKitRoute { SDLRoute, CustomRoute, CancelledRoute };
static std::map<uintptr_t,UIKitRoute> uikitRoutes;
static UIInterfaceOrientation viewOrientation=UIInterfaceOrientationUnknown;
static uint64_t orientationChanges=0;
static UIKitRoute routeUIKitTouch(uintptr_t key,int phase,bool wantsCustom) {
    if(phase==SourceTouchBegan)uikitRoutes[key]=wantsCustom?CustomRoute:SDLRoute;
    auto it=uikitRoutes.find(key);
    UIKitRoute route=it==uikitRoutes.end()?CancelledRoute:it->second;
    if(phase==SourceTouchEnded || phase==SourceTouchCancelled)uikitRoutes.erase(key);
    return route;
}
static unsigned actionOwners[SourceTouchActionCount]={};
static std::map<int,unsigned> keyboardOwners;
static void sendKeyboard(int code,bool pressed) {SDL_SendTouchKeyboardKey(pressed?SDL_PRESSED:SDL_RELEASED,(SDL_Scancode)code);}
static void (*keyboardSink)(int,bool)=sendKeyboard;
static void holdKeyboard(int code,bool pressed) {
    if(pressed){if(!keyboardOwners[code]++)keyboardSink(code,true);}
    else {auto it=keyboardOwners.find(code);if(it!=keyboardOwners.end() && !--it->second){keyboardSink(code,false);keyboardOwners.erase(it);}}
}
static uintptr_t moveOwner=0,lookOwner=0;
static CGRect lookRect=CGRectZero,moveRect=CGRectZero;
static CGFloat moveRadius=64;
static bool dirty=true,available=false;
static bool inventoryDirty=false,focusActive=true;
static bool buttonsHidden=false;
static bool connecting=false;
static uint64_t mfiFilterMatches=0;
static __strong NSArray *notificationTokens;
static std::atomic<bool> framePending{false};
static float pointsPerMM=0,weaponOldHeight=0,weaponHeight=0;
static uint64_t actualSamples=0,lookSamples=0,renderedFrames=0,cancellations=0;
static double lookDX=0,lookDY=0;
static float moveX=0,moveY=0;

#include "touch_keyboard_keys.inc"
#include "touch_layout_store.inc"

static bool consoleVisible=false;
static bool calibrationRunning=false;

static double now() { return NSProcessInfo.processInfo.systemUptime; }
static bool visible() {return focusActive && state.active && state.mode==SourceTouchModeGameplay;}
static bool gameplay() {return !editing && visible() && state.game_input && !state.scoreboard_open;}
static void callMove(float x,float y,double t) {moveX=x;moveY=y;if(callbacks.move)callbacks.move(callbackUser,x,y,t);}
static SourceTouchLookSampleCallback lookSampleCallback=nullptr;
static void callLook(float x,float y,double t,double interval=0) {
    if(x==0 && y==0)return;
    ++lookSamples;lookDX+=x;lookDY+=y;
    if(lookSampleCallback)lookSampleCallback(callbackUser,x,y,t,interval);
    else if(callbacks.look)callbacks.look(callbackUser,x,y,t);
}
static void callAction(int a,bool pressed,double t) {
    if(a>=0 && a<SourceTouchActionCount && callbacks.action)
        callbacks.action(callbackUser,(SourceTouchAction)a,pressed,t);
}
static bool contains(const Control &c,CGPoint p) {
    if(!CGRectContainsPoint(c.rect,p))return false;
    if(!c.circle)return true;
    double x=(p.x-CGRectGetMidX(c.rect))/(c.rect.size.width*.5);
    double y=(p.y-CGRectGetMidY(c.rect))/(c.rect.size.height*.5);
    return x*x+y*y<=1;
}
static bool circleIntersectsRect(CGRect circle,CGRect rect) {
    if(CGRectIsEmpty(circle) || CGRectIsEmpty(rect))return false;
    const CGFloat x=std::clamp<CGFloat>(CGRectGetMidX(circle),CGRectGetMinX(rect),CGRectGetMaxX(rect));
    const CGFloat y=std::clamp<CGFloat>(CGRectGetMidY(circle),CGRectGetMinY(rect),CGRectGetMaxY(rect));
    return std::hypot(x-CGRectGetMidX(circle),y-CGRectGetMidY(circle))<circle.size.width*.5;
}
static bool circleIntersectsControl(CGRect circle,const Control &c) {
    if(!c.circle)return circleIntersectsRect(circle,c.rect);
    return std::hypot(CGRectGetMidX(circle)-CGRectGetMidX(c.rect),CGRectGetMidY(circle)-CGRectGetMidY(c.rect))<
        (circle.size.width+c.rect.size.width)*.5;
}
static bool lookAllowed(CGPoint p) {
    if(!gameplay() || !CGRectContainsPoint(lookRect,p))return false;
    // Include disabled/empty inventory targets: no control's visible surface
    // can leak gameplay mouse look.
    if(!buttonsHidden)for(const auto &c:controls)if(CGRectContainsPoint(c.rect,p))return false;
    return true;
}
static bool moveAllowed(CGPoint p) {
    if(!gameplay() || !CGRectContainsPoint(moveRect,p))return false;
    // This is touch-down hit testing only. Use the same exact geometry as
    // begin(): the empty corners around a circular button are not buttons.
    if(!buttonsHidden)for(const auto &c:controls)if(contains(c,p))return false;
    return true;
}
static CGPoint center(const Control &c) {return CGPointMake(CGRectGetMidX(c.rect),CGRectGetMidY(c.rect));}
static void cancel(double t,bool keepScoreKey=false) {
    // A held Tab must survive the scoreboard it just opened. Every other
    // mode/focus/editor transition releases keyboard ownership normally.
    std::map<uintptr_t,Owner> kept;
    if(keepScoreKey)for(const auto &entry:owners)
        if(entry.second.type==Owner::KeyboardOwner && entry.second.keyboard==SDL_SCANCODE_TAB)kept.insert(entry);
    for(auto &route:uikitRoutes)if(route.second==CustomRoute && !kept.count(route.first))route.second=CancelledRoute;
    for(auto &c:controls)if(c.owners){[c.native handleTouchEndedAtPoint:center(c)];c.owners=0;}
    for(int i=0;i<SourceTouchActionCount;++i)if(actionOwners[i]){actionOwners[i]=0;callAction(i,false,t);}
    for(auto it=keyboardOwners.begin();it!=keyboardOwners.end();) {
        if(!kept.empty() && it->first==SDL_SCANCODE_TAB){++it;continue;}
        keyboardSink(it->first,false);it=keyboardOwners.erase(it);
    }
    owners=std::move(kept);moveOwner=lookOwner=0;callMove(0,0,t);
    if(callbacks.reset)callbacks.reset(callbackUser,t);
    ++cancellations;
}
static void move(const Owner &owner,CGPoint p,double t) {
    if(!gameplay()){callMove(0,0,t);return;}
    // The initial touch-down captured this finger for movement. Crossing a
    // button or the divider cannot zero it or transfer it to another action.
    // end()/cancel() still release the vector immediately.
    // Relative to this finger's touch-down, with the original stick's travel
    // distance. Hold the resulting vector until the next sample or release.
    float x=(p.x-owner.origin.x)/moveRadius,y=(owner.origin.y-p.y)/moveRadius;
    const float length=std::hypot(x,y);if(length>1){x/=length;y/=length;}
    callMove(x,y,t);
}

static CGImageRef labelImage(NSString *text,CGSize size,bool circle,bool selected,bool highlight) {
    const CGFloat scale=std::max<CGFloat>(1,displayScale);
    size_t w=std::max<size_t>(1,llround(size.width*scale)),h=std::max<size_t>(1,llround(size.height*scale));
    CGColorSpaceRef space=CGColorSpaceCreateDeviceRGB();
    CGContextRef context=CGBitmapContextCreate(nullptr,w,h,8,w*4,space,kCGImageAlphaPremultipliedLast|kCGBitmapByteOrder32Big);
    CGColorSpaceRelease(space);if(!context)return nullptr;
    CGContextScaleCTM(context,scale,scale);CGRect r=CGRectMake(1,1,std::max<CGFloat>(1,size.width-2),std::max<CGFloat>(1,size.height-2));
    CGFloat fill[4]={selected?.18f:.06f,selected?.48f:.08f,selected?.66f:.12f,highlight?.78f:.58f};
    CGContextSetFillColor(context,fill);CGContextSetStrokeColorWithColor(context,[UIColor colorWithWhite:1 alpha:highlight?1:.65].CGColor);CGContextSetLineWidth(context,1.5);
    if(circle){CGContextAddEllipseInRect(context,r);CGContextDrawPath(context,kCGPathFillStroke);}
    else {CGPathRef p=CGPathCreateWithRoundedRect(r,6,6,nullptr);CGContextAddPath(context,p);CGContextDrawPath(context,kCGPathFillStroke);CGPathRelease(p);}
    CGFloat fontSize=std::min<CGFloat>(17,std::max<CGFloat>(10,size.height*.30));
    CTFontRef font=CTFontCreateUIFontForLanguage(kCTFontUIFontSystem,fontSize,CFSTR("zh-Hans"));
    CGFloat white[4]={1,1,1,1};CGColorSpaceRef textSpace=CGColorSpaceCreateDeviceRGB();CGColorRef color=CGColorCreate(textSpace,white);CGColorSpaceRelease(textSpace);
    NSDictionary *attrs=@{(__bridge NSString*)kCTFontAttributeName:(__bridge id)font,(__bridge NSString*)kCTForegroundColorAttributeName:(__bridge id)color};
    NSAttributedString *string=[[NSAttributedString alloc] initWithString:text attributes:attrs];CTLineRef line=CTLineCreateWithAttributedString((__bridge CFAttributedStringRef)string);
    if(CTLineGetTypographicBounds(line,nullptr,nullptr,nullptr)>size.width-8) {
        CTLineRef shorter=CTLineCreateTruncatedLine(line,std::max<CGFloat>(1,size.width-8),kCTLineTruncationEnd,nullptr);
        if(shorter){CFRelease(line);line=shorter;}
    }
    CGFloat ascent=0,descent=0;double tw=CTLineGetTypographicBounds(line,&ascent,&descent,nullptr);
    CGContextSetTextPosition(context,(size.width-tw)*.5,(size.height-(ascent+descent))*.5+descent);CTLineDraw(line,context);
    CGImageRef image=CGBitmapContextCreateImage(context);CFRelease(line);CFRelease(font);CGColorRelease(color);CGContextRelease(context);return image;
}
static TCControlContents *contents(NSString *text,CGSize size,bool circle,bool selected) {
    CGImageRef base=labelImage(text,size,circle,selected,false),highlight=labelImage(text,size,circle,selected,true);
    if(!base || !highlight){if(base)CGImageRelease(base);if(highlight)CGImageRelease(highlight);return nil;}
    TCControlImage *image=[[TCControlImage alloc] initWithCGImage:base size:size device:metalDevice];
    TCControlImage *lit=[[TCControlImage alloc] initWithCGImage:highlight size:size device:metalDevice];
    CGImageRelease(base);CGImageRelease(highlight);if(!image || !lit)return nil;
    image.highlightTexture=lit.texture;
    return [TCControlContents contentsWithImages:@[image]];
}
static void refreshWeapons() {
    for(auto &c:controls)if(c.kind==Weapon) {
        const WeaponInfo *shown=nullptr;
        for(const auto &item:inventory)if(item.slot==c.value && (!shown || item.selected))shown=&item;
        NSString *text=shown ? [[NSString alloc] initWithUTF8String:shown->name.c_str()] : @"—";
        if(!text.length)text=[NSString stringWithFormat:@"槽位%d",c.value];
        c.enabled=!buttonsHidden && gameplay() && shown;c.native.enabled=c.enabled;
        ((TCButton*)c.native).contents=contents(text,c.rect.size,false,shown && shown->selected);
    }
    inventoryDirty=false;
}
static void add(NSString *name,NSString *text,CGRect rect,Kind kind,int value,bool circle,bool enabled,bool selected=false) {
    enabled=enabled && (!buttonsHidden || editing);
    rect=applyLayout(name,rect);
    TCButtonDescriptor *desc=[TCButtonDescriptor new];desc.label=[[TCControlLabel alloc] initWithName:name role:TCControlLabelRoleButton];
    desc.anchor=TCControlLayoutAnchorCenter;desc.anchorCoordinateSystem=TCControlLayoutAnchorCoordinateSystemAbsolute;
    desc.offset=CGPointMake(CGRectGetMidX(rect)-viewSize.width*.5,CGRectGetMidY(rect)-viewSize.height*.5);
    desc.size=rect.size;desc.colliderShape=circle?TCColliderShapeCircle:TCColliderShapeRect;desc.highlightDuration=0;
    desc.contents=contents(text,rect.size,circle,selected);desc.zIndex=10+(NSInteger)controls.size();
    TCButton *button=[controller addButtonWithDescriptor:desc];button.enabled=enabled;
    Control c;c.native=button;c.rect=rect;c.label=name.UTF8String;c.kind=kind;c.value=value;c.circle=circle;c.enabled=enabled;controls.push_back(c);
}
static CGRect fromRender(SourceTouchRect r) {
    return CGRectMake(r.x*viewSize.width/state.render_width,r.y*viewSize.height/state.render_height,
        r.width*viewSize.width/state.render_width,r.height*viewSize.height/state.render_height);
}
static void rebuild() {
    if(!controller || viewSize.width<=0 || viewSize.height<=0)return;
    // Layout changes invalidate prior hit geometry; never transfer a touch to
    // whichever button happens to occupy its former coordinates.
    if(!owners.empty())cancel(now(),state.scoreboard_open);
    [controller removeAllControls];controls.clear();defaultRects.clear();
    controller.size=viewSize;controller.drawableSize=drawableSize;
    const CGFloat w=viewSize.width,h=viewSize.height;
    const bool compact=h<500;
    const CGFloat controlSize=compact?46:64;
    const CGFloat ppmm=state.calibrated_points_per_mm>0 ? state.calibrated_points_per_mm :
        (screenLandscapePoints>0 ? screenLandscapePoints/(state.native_landscape_width_pixels>0?state.native_landscape_width_pixels:2420)*
            (state.native_density_ppi>0?state.native_density_ppi:264)/25.4 : 0);
    pointsPerMM=ppmm;
    const CGFloat rightMargin=std::max<CGFloat>(20*ppmm,safe.right+3);
    const CGFloat barWidth=std::min<CGFloat>(w-rightMargin-safe.left-12,std::max<CGFloat>(240,w*.43));
    weaponOldHeight=compact?42:54*w/704;
    weaponHeight=compact?28:std::max<CGFloat>(8,weaponOldHeight-10*ppmm);
    const CGFloat barX=w-rightMargin-barWidth,barY=h-safe.bottom-3-weaponHeight;
    const CGFloat supportHeight=compact?34:40,supportWidth=compact?45:56,supportY=barY-supportHeight-8;
    lookRect=CGRectMake(w*.5,0,w*.5,h);
    moveRect=CGRectMake(0,0,w*.5,h);
    moveRadius=std::max<CGFloat>(40,std::min<CGFloat>(78,w*.105));
    const bool game=gameplay();
    const CGFloat menuSize=compact?42:48;
    const CGRect pause=CGRectMake(safe.left+8,safe.top+8,menuSize,menuSize);
    const CGRect buy=CGRectMake(CGRectGetMaxX(pause)+8,pause.origin.y,menuSize,menuSize);
    add(@"pause",@"暂停",pause,Command,SourceTouchPause,false,visible());
    add(@"buy",@"购买",buy,Command,SourceTouchBuy,false,game);
    add(@"inspect",@"检视",CGRectMake(CGRectGetMaxX(buy)+8,pause.origin.y,menuSize,menuSize),Command,SourceTouchInspect,false,game);
    add(@"drop",@"丢弃",CGRectMake(CGRectGetMaxX(buy)+menuSize+16,pause.origin.y,menuSize,menuSize),Command,SourceTouchDrop,false,game);
    CGRect radar=CGRectZero,score=CGRectZero;
    if((state.score_valid && state.render_width>0 && state.render_height>0) || editing) {
        // Menu-only preview fallback uses the recorded original Office HUD
        // bounds (map-fix-v5/cs_office-play-console.log). Gameplay uses its
        // live Panorama bounds, including user HUD settings.
        score=state.score_valid && state.render_width>0 && state.render_height>0?fromRender(state.score):CGRectMake(w*488/2816,0,w*1839/2816,h*320/1940);
        add(@"scoreboard",state.scoreboard_open?@"关闭战绩":@"战绩",CGRectMake(CGRectGetMidX(score)-40,CGRectGetMaxY(score)+4,80,32),Command,SourceTouchScoreboard,false,visible(),state.scoreboard_open);
    }
    CGRect jump=CGRectZero;
    if((state.radar_valid && state.render_width>0 && state.render_height>0) || editing) {
        radar=state.radar_valid && state.render_width>0 && state.render_height>0?fromRender(state.radar):CGRectMake(0,h*108/1940,w*539/2816,h*539/1940);const CGFloat jumpSize=compact?46:(w<620?52:64);
        jump=CGRectMake(CGRectGetMidX(radar)-jumpSize*.5,CGRectGetMaxY(radar)+(compact?20:20*ppmm),jumpSize,jumpSize);
        add(@"jump",@"跳跃",jump,Held,SourceTouchJump,false,game);
    }
    // Add the requested physical 50 mm to the existing diameter. Do not push
    // this large target below jump: that would occupy the left-thumb stick.
    // Keep it to the right of the actual radar/jump and below top HUD/menu.
    const CGFloat fireDiameter=compact?std::clamp<CGFloat>(h*.28,96,124):84+50*ppmm,fireRadius=fireDiameter*.5;
    CGFloat fireLeft=std::max<CGFloat>(safe.left+8,w*.10-fireRadius);
    if(!CGRectIsEmpty(jump))fireLeft=std::max<CGFloat>(fireLeft,CGRectGetMaxX(jump)+8);
    if(!CGRectIsEmpty(radar))fireLeft=std::max<CGFloat>(fireLeft,CGRectGetMaxX(radar)+8);
    CGFloat fireTop=std::max<CGFloat>(h*(compact?.29:.33)-fireRadius,CGRectGetMaxY(buy)+8);
    if(!CGRectIsEmpty(score))fireTop=std::max<CGFloat>(fireTop,CGRectGetMaxY(score)+8);
    const CGRect fire=CGRectMake(fireLeft,fireTop,fireDiameter,fireDiameter);
    add(@"attack",@"开火",fire,Held,SourceTouchAttack,true,game);
    const CGFloat actionY=std::max<CGFloat>(safe.top+controlSize*.5+8,std::max<CGFloat>(76,h*.22));
    const CGFloat duckX=w*.91-controlSize*.5,aimX=duckX-controlSize-12;
    add(@"attack2",@"瞄准",CGRectMake(aimX,actionY-controlSize*.5,controlSize,controlSize),Held,SourceTouchAttack2,false,game);
    add(@"duck",@"蹲伏",CGRectMake(duckX,actionY-controlSize*.5,controlSize,controlSize),Held,SourceTouchDuck,false,game);
    for(int slot=1;slot<=5;++slot) {
        std::vector<const WeaponInfo*> items;for(const auto &item:inventory)if(item.slot==slot)items.push_back(&item);
        const WeaponInfo *shown=nullptr;for(auto item:items)if(item->selected)shown=item;if(!shown && !items.empty())shown=items.front();
        NSString *text=shown ? [[NSString alloc] initWithUTF8String:shown->name.c_str()] : @"—";
        if(!text.length)text=[NSString stringWithFormat:@"槽位%d",slot];
        add([NSString stringWithFormat:@"weapon%d",slot],text,CGRectMake(barX+(slot-1)*barWidth/5,barY,barWidth/5,weaponHeight),Weapon,slot,false,game && shown,shown && shown->selected);
    }
    add(@"use",@"使用",CGRectMake(barX+barWidth/10-supportWidth*.5,supportY,supportWidth,supportHeight),Held,SourceTouchUse,false,game);
    add(@"reload",@"换弹",CGRectMake(barX+barWidth*3/10-supportWidth*.5,supportY,supportWidth,supportHeight),Held,SourceTouchReload,false,game);
    const CGFloat radius=std::max<CGFloat>(40,std::min<CGFloat>(78,w*.105)),moveX=w*.17;
    const CGFloat useLeft=barX+barWidth/10-supportWidth*.5;
    const CGFloat lower=useLeft<moveX+radius?supportY:barY;
    const CGFloat moveY=std::min<CGFloat>(h*.73,lower-radius-8);
    // Movement has no visible control or editable hit rectangle. Retain the
    // separate walk button's default placement and all saved button geometry.
    // The design explicitly calls for independent walking control. Keep it
    // immediately to the right of the left-thumb stick, away from weapon slots.
    add(@"walk",@"静步",CGRectMake(moveX+radius+8,moveY-24,48,48),Held,SourceTouchWalk,false,game);
    for(NSUInteger i=0;i<savedCustomButtons.count;++i) {
        NSDictionary *entry=savedCustomButtons[i];int key=[entry[@"key"] intValue];
        add(entry[@"id"],keyboardTitle(key),customDefaultRect(i),Keyboard,key,false,game || (visible() && state.scoreboard_open && key==SDL_SCANCODE_TAB));
    }
    dirty=false;inventoryDirty=false;
}

static bool initialize(id<MTLDevice> device,MTLPixelFormat format) {
    if(!device || ![TCTouchController isSupported])return false;
    if(controller && metalDevice==device && pixelFormat==format)return true;
    if(controller){cancel(now());[controller disconnect];}
    metalDevice=device;pixelFormat=format;
    TCTouchControllerDescriptor *d=[TCTouchControllerDescriptor new];d.device=device;
    d.size=viewSize.width>0?viewSize:CGSizeMake(1,1);d.drawableSize=drawableSize.width>0?drawableSize:CGSizeMake(1,1);
    d.colorPixelFormat=format;d.depthAttachmentPixelFormat=MTLPixelFormatInvalid;d.stencilAttachmentPixelFormat=MTLPixelFormatInvalid;d.sampleCount=1;
    controller=[[TCTouchController alloc] initWithDescriptor:d];
    connecting=true;
    @try {[controller connect];} @finally {connecting=false;}
    available=controller!=nil;dirty=true;return available;
}
static void updateView(UIView *view) {
    if(!view || !NSThread.isMainThread || ![view.layer isKindOfClass:CAMetalLayer.class])return;
    CGSize size=view.bounds.size;UIEdgeInsets insets=view.safeAreaInsets;
    UIInterfaceOrientation orientation=view.window.windowScene.effectiveGeometry.interfaceOrientation;
    if(viewOrientation!=UIInterfaceOrientationUnknown && orientation!=UIInterfaceOrientationUnknown && viewOrientation!=orientation) {
        SourceTouchUIKitOrientationChanging((__bridge void*)view);dirty=true;++orientationChanges;
    }
    viewOrientation=orientation;
    CGFloat scale=view.traitCollection.displayScale;
    if(!(scale>0))scale=view.window.screen.scale;if(!(scale>0))scale=1;
    CGFloat screenWidth=std::max(view.window.screen.bounds.size.width,view.window.screen.bounds.size.height);
    if(!CGSizeEqualToSize(size,viewSize) || !UIEdgeInsetsEqualToEdgeInsets(safe,insets) || displayScale!=scale || screenLandscapePoints!=screenWidth)dirty=true;
    viewSize=size;safe=insets;displayScale=scale;screenLandscapePoints=screenWidth;
    // The phone's short landscape viewport needs denser menu content. Cache
    // the idiom here; Source never queries UIKit from its rendering thread.
    menuUIScale.store(view.traitCollection.userInterfaceIdiom==UIUserInterfaceIdiomPhone?.9f:1.0f);
    if(drawableSize.width<=0)drawableSize=CGSizeMake(llround(size.width*scale),llround(size.height*scale));
    updateGlassNavigationLayout();
    updateMapGalleryLayout();
    updateInventoryLayout();
}
static void refreshHostState() {
    void (*fn)(void*)=nullptr;void *user=nullptr;
    {std::lock_guard<std::recursive_mutex> lock(mutex);fn=callbacks.frame;user=callbackUser;}
    if(!fn)return;
    if(NSThread.isMainThread){fn(user);return;}
    if(framePending.exchange(true))return;
    dispatch_async(dispatch_get_main_queue(),^{
        framePending.store(false);refreshHostState();
    });
}
static void watchLifecycle() {
    if(notificationTokens || !NSThread.isMainThread)return;
    auto center=[NSNotificationCenter defaultCenter];
    id inactive=[center addObserverForName:UIApplicationWillResignActiveNotification object:nil queue:NSOperationQueue.mainQueue usingBlock:^(NSNotification*) {
        std::lock_guard<std::recursive_mutex> lock(mutex);focusActive=false;cancel(now());
    }];
    id active=[center addObserverForName:UIApplicationDidBecomeActiveNotification object:nil queue:NSOperationQueue.mainQueue usingBlock:^(NSNotification*) {
        std::lock_guard<std::recursive_mutex> lock(mutex);focusActive=true;
    }];
    id sceneInactive=[center addObserverForName:UISceneWillDeactivateNotification object:nil queue:NSOperationQueue.mainQueue usingBlock:^(NSNotification*) {
        std::lock_guard<std::recursive_mutex> lock(mutex);focusActive=false;cancel(now());
    }];
    id sceneActive=[center addObserverForName:UISceneDidActivateNotification object:nil queue:NSOperationQueue.mainQueue usingBlock:^(NSNotification*) {
        std::lock_guard<std::recursive_mutex> lock(mutex);focusActive=true;
    }];
    id memory=[center addObserverForName:UIApplicationDidReceiveMemoryWarningNotification object:nil queue:NSOperationQueue.mainQueue usingBlock:^(NSNotification*) {
        std::lock_guard<std::recursive_mutex> lock(mutex);trimInventoryMemory();
    }];
    notificationTokens=@[inactive,active,sceneInactive,sceneActive,memory];
}

static void begin(uintptr_t key,CGPoint p,double t) {
    if(owners.count(key))return;
    Owner owner;owner.last=p;owner.origin=p;owner.timestamp=t;
    // Exact control hit geometry always wins, even for disabled targets.
    // Later controls are drawn above earlier controls; use the same order for
    // overlapping custom buttons rather than letting an obscured key win.
    if(!buttonsHidden)for(size_t n=controls.size();n>0;--n)if(size_t i=n-1;contains(controls[i],p)) {
        auto &c=controls[i];
        if(!c.enabled)break;
        owner.type=Owner::ControlOwner;owner.control=i;
        if(!c.owners++)[c.native handleTouchBeganAtPoint:center(c)];
        if(c.kind==Keyboard){owner.type=Owner::KeyboardOwner;owner.keyboard=c.value;holdKeyboard(c.value,true);}
        else if(c.kind==Held){if(!actionOwners[c.value]++)callAction(c.value,true,t);}
        else if(c.kind==Command){if(callbacks.command)callbacks.command(callbackUser,(SourceTouchCommand)c.value,t);}
        else if(c.kind==Weapon) {
            std::vector<const WeaponInfo*> items;for(const auto &item:inventory)if(item.slot==c.value)items.push_back(&item);
            size_t index=0;for(size_t n=0;n<items.size();++n)if(items[n]->selected)index=(n+1)%items.size();
            if(!items.empty() && callbacks.weapon_select)callbacks.weapon_select(callbackUser,items[index]->entity,items[index]->subtype,t);
        }
        owners[key]=owner;return;
    }
    if(owner.type==Owner::Ignored && !moveOwner && moveAllowed(p)) {
        owner.type=Owner::MoveOwner;moveOwner=key;callMove(0,0,t);
    }
    else if(owner.type==Owner::Ignored && !lookOwner && lookAllowed(p)) {owner.type=Owner::LookOwner;owner.inside=true;lookOwner=key;}
    owners[key]=owner;
}
static void motion(uintptr_t key,CGPoint p,double t) {
    auto it=owners.find(key);if(it==owners.end() || t<it->second.timestamp)return;
    auto &o=it->second;
    if(o.type==Owner::LookOwner) {
        bool allowed=lookAllowed(p);
        if(allowed && o.inside)callLook(float(p.x-o.last.x),float(p.y-o.last.y),t,t-o.timestamp);
        o.inside=allowed;
    } else if(o.type==Owner::MoveOwner) {
        move(o,p,t);
    } else if(o.type==Owner::ControlOwner && o.control<controls.size()) {
        // Other controls retain their initial action. Framework drag semantics
        // are not allowed to release/reassign a held Source action mid-touch.
    }
    o.last=p;o.timestamp=t;
}
static void end(uintptr_t key,double t) {
    auto it=owners.find(key);if(it==owners.end())return;
    auto o=it->second;owners.erase(it);
    if(o.type==Owner::LookOwner){lookOwner=0;return;}
    if(o.type==Owner::MoveOwner){moveOwner=0;callMove(0,0,t);return;}
    if(o.type==Owner::KeyboardOwner) {
        holdKeyboard(o.keyboard,false);
        if(o.control<controls.size()) {auto &c=controls[o.control];if(c.kind==Keyboard && c.value==o.keyboard && c.owners && !--c.owners)[c.native handleTouchEndedAtPoint:center(c)];}
        return;
    }
    if(o.type!=Owner::ControlOwner || o.control>=controls.size())return;
    auto &c=controls[o.control];
    if(c.owners && !--c.owners)[c.native handleTouchEndedAtPoint:center(c)];
    if(c.kind==Held){if(actionOwners[c.value] && !--actionOwners[c.value])callAction(c.value,false,t);}
}
} // namespace

#include "touch_layout_editor.inc"
#include "player_name_editor.inc"
#include "room_browser.inc"
#include "frame_calibration_ios.inc"
#include "translucent_surface_ios.inc"
#include "console_header_ios.inc"
#include "glass_navigation_ios.inc"
#include "map_gallery_ios.inc"
#include "settings_scroll_ios.inc"
#include "glass_inventory_ios.inc"

extern "C" float SourceTouchMenuUIScale() {return menuUIScale.load();}

extern "C" bool SourceTouchSaveInventoryPreview(const char *filename,const void *bgra,int width,int height) {
    @autoreleasepool {
        if(!filename || !bgra || width<=0 || height<=0 || width>1024 || height>1024)return false;
        NSString *file=[NSString stringWithUTF8String:filename];
        NSRegularExpression *pattern=[NSRegularExpression regularExpressionWithPattern:@"^preview_[0-9]+(_[0-9]+){7}\\.png$" options:0 error:nil];
        if([pattern numberOfMatchesInString:file options:0 range:NSMakeRange(0,file.length)]!=1)return false;
        CGColorSpaceRef space=CGColorSpaceCreateDeviceRGB();
        CGDataProviderRef provider=CGDataProviderCreateWithData(NULL,bgra,(size_t)width*height*4,NULL);
        CGImageRef image=CGImageCreate(width,height,8,32,(size_t)width*4,space,
            kCGBitmapByteOrder32Little|kCGImageAlphaPremultipliedFirst,provider,NULL,false,kCGRenderingIntentDefault);
        NSData *png=image?UIImagePNGRepresentation([UIImage imageWithCGImage:image]):nil;
        if(image)CGImageRelease(image);CGDataProviderRelease(provider);CGColorSpaceRelease(space);
        if(!png.length)return false;
        NSString *docs=NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,NSUserDomainMask,YES).firstObject;
        NSString *directory=[docs stringByAppendingPathComponent:@"runtime-i5/csgo/cfg/icsm-sticker-previews"];
        NSError *error=nil;
        if(![NSFileManager.defaultManager createDirectoryAtPath:directory withIntermediateDirectories:YES attributes:nil error:&error])return false;
        BOOL saved=[png writeToFile:[directory stringByAppendingPathComponent:file] options:NSDataWritingAtomic error:&error];
        if(!saved)fprintf(stderr,"ICSM_STICKER_PREVIEW encode_write_error=%s\n",error.localizedDescription.UTF8String);
        return saved;
    }
}

extern "C" void SourceTouchExportErrorLogs() {
    NSCAssert(NSThread.isMainThread,@"Log sharing belongs to the Source main thread");
    SourceTouchCancelAll(now());
    [NSNotificationCenter.defaultCenter postNotificationName:@"ICSMExportErrorLogs" object:nil];
}

extern "C" bool SourceTouchInitialize(void *device) {
    std::lock_guard<std::recursive_mutex> lock(mutex);return initialize((__bridge id<MTLDevice>)device,MTLPixelFormatBGRA8Unorm);
}
extern "C" void SourceTouchShutdown() {
    shutdownGlassNavigation();
    shutdownMapGallery();
    shutdownSettingsScroll();
    shutdownConsoleHeader();
    shutdownInventory();
    std::lock_guard<std::recursive_mutex> lock(mutex);cancel(now());[controller disconnect];controller=nil;metalDevice=nil;host=nil;controls.clear();available=false;dirty=true;
    uikitRoutes.clear();viewOrientation=UIInterfaceOrientationUnknown;buttonsHidden=false;
    for(id token in notificationTokens)[NSNotificationCenter.defaultCenter removeObserver:token];notificationTokens=nil;
}
extern "C" bool SourceTouchOwnsGameController(void *gameController) {
    if(!gameController)return false;
    std::lock_guard<std::recursive_mutex> lock(mutex);
    GCController *candidate=(__bridge GCController*)gameController;
    bool match=controller && controller.controller==candidate;
    // connect may post a synchronous controller notification before its
    // readonly controller property becomes observable. Use Apple's public
    // product category only during this module's own connection call.
    if(!match && connecting)match=[candidate.productCategory isEqualToString:TCGameControllerProductCategoryTouchController];
    if(match)++mfiFilterMatches;
    return match;
}
extern "C" void SourceTouchSetCallbacks(const SourceTouchCallbacks *c,void *user) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    lookSampleCallback=nullptr;
    if(!c || c->struct_size!=sizeof(*c)){cancel(now());callbacks={};callbackUser=nullptr;return;}
    callbacks=*c;callbackUser=user;
}
extern "C" void SourceTouchSetLookSampleCallback(SourceTouchLookSampleCallback callback) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    lookSampleCallback=callback;
}
extern "C" void SourceTouchSetButtonsHidden(bool hidden) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    if(buttonsHidden==hidden)return;
    // Release held actions and discard old owners before changing hit targets.
    cancel(now());buttonsHidden=hidden;dirty=true;
}
extern "C" bool SourceTouchKeyboardTestEdge(int code,bool pressed) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    if(!NSThread.isMainThread || !keyboardTitle(code) || editing || (pressed && !gameplay()))return false;
    // Local validation owns a distinct non-UIKit token, so scoreboard/mode
    // changes exercise the same release policy as a real custom-button owner.
    const uintptr_t token=UINTPTR_MAX-(uintptr_t)code;
    if(pressed) {
        if(owners.count(token))return false;
        Owner owner;owner.type=Owner::KeyboardOwner;owner.keyboard=code;owner.control=controls.size();
        owners[token]=owner;holdKeyboard(code,true);
    } else end(token,now());
    return true;
}
extern "C" void SourceTouchUpdateState(const SourceTouchState *s) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    if(!s || s->struct_size!=sizeof(*s)){cancel(now());state={};dirty=true;return;}
    if(state.mode!=s->mode || state.active!=s->active || state.game_input!=s->game_input || state.scoreboard_open!=s->scoreboard_open) {
        cancel(now(),s->mode==SourceTouchModeGameplay && s->active && s->scoreboard_open);dirty=true;
    }
    if(state.radar_valid!=s->radar_valid || state.score_valid!=s->score_valid ||
       std::memcmp(&state.radar,&s->radar,sizeof(s->radar)) || std::memcmp(&state.score,&s->score,sizeof(s->score)) ||
       state.render_width!=s->render_width || state.render_height!=s->render_height ||
       state.native_landscape_width_pixels!=s->native_landscape_width_pixels || state.native_density_ppi!=s->native_density_ppi || state.calibrated_points_per_mm!=s->calibrated_points_per_mm)dirty=true;
    std::vector<WeaponInfo> copy;
    if(s->weapons)for(size_t i=0;i<s->weapon_count;++i){const auto &w=s->weapons[i];if(w.slot>=1 && w.slot<=5 && w.entity_index>0)copy.push_back({w.slot,w.entity_index,w.subtype,w.selected,w.name_utf8?w.name_utf8:""});}
    if(inventory!=copy){inventory=std::move(copy);inventoryDirty=true;}
    state=*s;state.weapons=nullptr;
}
extern "C" bool SourceTouchSessionActive() {
    std::lock_guard<std::recursive_mutex> lock(mutex);return state.in_game;
}
extern "C" void SourceTouchAttachView(void *view) {
    if(!view)return;
    std::lock_guard<std::recursive_mutex> lock(mutex);UIView *v=(__bridge UIView*)view;
    if(!NSThread.isMainThread || ![v.layer isKindOfClass:CAMetalLayer.class])return;
    if(host && host!=v)cancel(now());host=v;watchLifecycle();updateView(v);
    if(editing && layoutEditor && layoutEditor.superview!=v){[layoutEditor removeFromSuperview];layoutEditor.frame=v.bounds;[v addSubview:layoutEditor];dirty=true;}
    if(editing && dirty && controller){rebuild();[layoutEditor setNeedsLayout];}
}
extern "C" void SourceTouchSetView(void *view) {
    if(!view)return;std::lock_guard<std::recursive_mutex> lock(mutex);updateView((__bridge UIView*)view);
    if(NSThread.isMainThread && editing && dirty && controller){rebuild();[layoutEditor setNeedsLayout];}
}
extern "C" bool SourceTouchUIKitTouches(void *view,void *set,void *event,int phase) {
    // Keep normal touch controls in the rendering workload, while preventing
    // a swipe from moving a fixed benchmark camera. UIKit's cancel button is
    // a separate window subview and still receives its own events.
    if(calibrationRunning)return true;
    refreshHostState();
    std::lock_guard<std::recursive_mutex> lock(mutex);UIView *v=(__bridge UIView*)view;
    updateView(v);UIView *coordinates=host?:v;
    NSSet<UITouch*> *touches=(__bridge NSSet<UITouch*>*)set;
    UITouch *touch=touches.anyObject;
    if(touch && (phase==SourceTouchBegan || phase==SourceTouchEnded)) {
        CGPoint p=[touch locationInView:v];
        fprintf(stderr,"ICSM_UIKIT_EDGE phase=%d type=%ld point=%.2f,%.2f orientation=%ld visible=%d routes=%zu\n",phase,(long)touch.type,p.x,p.y,(long)v.window.windowScene.effectiveGeometry.interfaceOrientation,visible(),uikitRoutes.size());
    }
    if(touches.count!=1 || !touch || touch.type==UITouchTypeIndirectPointer)return false;
    uintptr_t key=(uintptr_t)(__bridge void*)touch;
    UIKitRoute route=routeUIKitTouch(key,phase,editing || visible());
    if(route==SDLRoute)return false;
    if(route==CancelledRoute || editing || !visible())return true;
    if(dirty && controller)rebuild();
    struct Sample {uintptr_t key;CGPoint point;double timestamp;};std::vector<Sample> samples;
    UIEvent *e=(__bridge UIEvent*)event;
    for(UITouch *touch in (__bridge NSSet<UITouch*>*)set) {
        if(touch.type==UITouchTypeIndirectPointer)continue;
        uintptr_t key=(uintptr_t)(__bridge void*)touch;
        NSArray<UITouch*> *coalesced=phase==SourceTouchMoved && e?[e coalescedTouchesForTouch:touch]:nil;
        if(!coalesced.count)coalesced=@[touch];
        for(UITouch *sample in coalesced)samples.push_back({key,[sample locationInView:coordinates],sample.timestamp});
    }
    std::stable_sort(samples.begin(),samples.end(),[](const Sample&a,const Sample&b){return a.timestamp<b.timestamp;});
    for(const auto &s:samples) {
        ++actualSamples;
        if(phase==SourceTouchBegan)begin(s.key,s.point,s.timestamp);
        else if(phase==SourceTouchMoved)motion(s.key,s.point,s.timestamp);
        else if(phase==SourceTouchEnded){motion(s.key,s.point,s.timestamp);end(s.key,s.timestamp);}
        else if(phase==SourceTouchCancelled)end(s.key,s.timestamp);
    }
    if(phase==SourceTouchCancelled)cancel(samples.empty()?now():samples.back().timestamp);
    return true;
}
static unsigned long long mergedOverlayFrames=0,separateOverlayFrames=0;
static bool prepareTouchRendering(id<MTLDevice> device,id<MTLTexture> target) {
    if(NSThread.isMainThread)updateView(host);
    CGSize size=CGSizeMake(target.width,target.height);
    if(!CGSizeEqualToSize(drawableSize,size)){drawableSize=size;dirty=true;}
    if(!initialize(device,target.pixelFormat) || viewSize.width<=0 || viewSize.height<=0)return false;
    if(dirty)rebuild();
    else if(inventoryDirty)refreshWeapons();
    return true;
}
static void encodeTouchRendering(id<MTLRenderCommandEncoder> encoder,id<MTLTexture> target) {
    [encoder setViewport:MTLViewport{0,0,double(target.width),double(target.height),0,1}];
    [encoder setScissorRect:MTLScissorRect{0,0,target.width,target.height}];
    [encoder setCullMode:MTLCullModeNone];
    [controller renderUsingRenderCommandEncoder:encoder];++renderedFrames;
}
extern "C" void SourceTouchRenderMetalWithEncoder(void *buffer,void *texture) {
    refreshHostState();
    std::lock_guard<std::recursive_mutex> lock(mutex);
    if(editing || buttonsHidden || !visible() || !buffer || !texture)return;
    @autoreleasepool {
        id<MTLRenderCommandEncoder> encoder=(__bridge id<MTLRenderCommandEncoder>)buffer;
        id<MTLTexture> target=(__bridge id<MTLTexture>)texture;
        if(!prepareTouchRendering(encoder.device,target))return;
        encodeTouchRendering(encoder,target);++mergedOverlayFrames;
    }
}
extern "C" void SourceTouchRenderMetal(void *buffer,void *texture) {
    refreshHostState();
    std::lock_guard<std::recursive_mutex> lock(mutex);
    if(editing || buttonsHidden || !visible() || !buffer || !texture)return;
    @autoreleasepool {
        id<MTLCommandBuffer> command=(__bridge id<MTLCommandBuffer>)buffer;id<MTLTexture> target=(__bridge id<MTLTexture>)texture;
        if(!prepareTouchRendering(command.device,target))return;
        MTLRenderPassDescriptor *pass=[MTLRenderPassDescriptor renderPassDescriptor];pass.colorAttachments[0].texture=target;
        pass.colorAttachments[0].loadAction=MTLLoadActionLoad;pass.colorAttachments[0].storeAction=MTLStoreActionStore;
        id<MTLRenderCommandEncoder> encoder=[command renderCommandEncoderWithDescriptor:pass];encoder.label=@"CSGO iPad TouchController UI";
        encodeTouchRendering(encoder,target);[encoder endEncoding];++separateOverlayFrames;
    }
}
extern "C" void SourceTouchCancelAll(double timestamp) {
    std::lock_guard<std::recursive_mutex> lock(mutex);cancel(timestamp);
    for(auto &route:uikitRoutes)route.second=CancelledRoute;
}
static NSDictionary *touchDiagnosticsSnapshot() {
    std::lock_guard<std::recursive_mutex> lock(mutex);NSMutableArray *rectangles=[NSMutableArray array];
    if(NSThread.isMainThread && glassInventory)inventoryDiagnostics=[glassInventory diagnostics];
    for(const auto &c:controls)[rectangles addObject:@{@"name":@(c.label.c_str()),@"rect_points":@[@(c.rect.origin.x),@(c.rect.origin.y),@(c.rect.size.width),@(c.rect.size.height)],
        @"native_center":@[@(c.native.position.x),@(c.native.position.y)],@"owners":@(c.owners),@"enabled":@(c.enabled)}];
    const Control *fire=nullptr;for(const auto &c:controls)if(c.label=="attack"){fire=&c;break;}
    NSMutableArray *fireControls=[NSMutableArray array],*fireHUD=[NSMutableArray array];
    bool fireInside=false,fireBeforeLook=false;
    if(fire) {
        for(const auto &c:controls)if(&c!=fire && circleIntersectsControl(fire->rect,c))[fireControls addObject:@(c.label.c_str())];
        if(state.render_width>0 && state.render_height>0) {
            if(state.radar_valid && circleIntersectsRect(fire->rect,fromRender(state.radar)))[fireHUD addObject:@"radar"];
            if(state.score_valid && circleIntersectsRect(fire->rect,fromRender(state.score)))[fireHUD addObject:@"score"];
        }
        fireInside=CGRectGetMinX(fire->rect)>=safe.left && CGRectGetMaxX(fire->rect)<=viewSize.width-safe.right &&
            CGRectGetMinY(fire->rect)>=safe.top && CGRectGetMaxY(fire->rect)<=viewSize.height-safe.bottom;
        fireBeforeLook=CGRectGetMaxX(fire->rect)<CGRectGetMinX(lookRect);
    }
    NSDictionary *fireLayout=@{@"requested_diameter_increase_mm":@50,@"diameter_points":@(fire?fire->rect.size.width:0),
        @"intersecting_controls":fireControls,@"intersecting_hud":fireHUD,@"inside_safe_view":@(fireInside),@"before_look_region":@(fireBeforeLook),
        @"fits":@(fire && fireInside && fireBeforeLook && fireControls.count==0 && fireHUD.count==0)};
    NSMutableArray *physicalControllers=[NSMutableArray array];
    for(GCController *pad in GCController.controllers) {
        if(controller && controller.controller==pad)continue;
        [physicalControllers addObject:@{@"name":pad.vendorName?:@"",@"category":pad.productCategory?:@"",
            @"extended_gamepad":@(pad.extendedGamepad!=nil),
            @"native_sticks_left_xy_right_xy":@[@(pad.extendedGamepad.leftThumbstick.xAxis.value),
                @(pad.extendedGamepad.leftThumbstick.yAxis.value),@(pad.extendedGamepad.rightThumbstick.xAxis.value),
                @(pad.extendedGamepad.rightThumbstick.yAxis.value)]}];
    }
    NSDictionary *d=@{@"backend":@"Apple TCTouchController / existing Metal command buffer",@"available":@(available),@"gameplay":@(gameplay()),@"visible":@(visible()),
        @"physical_controllers":physicalControllers,
        @"buttons_hidden":@(buttonsHidden),@"buttons_visible":@(visible() && !buttonsHidden),
        @"orientation":@(viewOrientation),@"orientation_changes":@(orientationChanges),@"uikit_routes":@(uikitRoutes.size()),
        @"view_points":@[@(viewSize.width),@(viewSize.height)],@"drawable_pixels":@[@(drawableSize.width),@(drawableSize.height)],
        @"safe_area":@[@(safe.top),@(safe.left),@(safe.bottom),@(safe.right)],@"points_per_mm":@(pointsPerMM),
        @"weapon_previous_height_points":@(weaponOldHeight),@"weapon_height_points":@(weaponHeight),@"look_rect_points":@[@(lookRect.origin.x),@(lookRect.origin.y),@(lookRect.size.width),@(lookRect.size.height)],
        @"movement_mode":@"left_half_swipe",@"move_rect_points":@[@(moveRect.origin.x),@(moveRect.origin.y),@(moveRect.size.width),@(moveRect.size.height)],
        @"move_full_speed_distance_points":@(moveRadius),@"move_owner_active":@(moveOwner!=0),
        @"move_vector":@[@(moveX),@(moveY)],@"movement_capture":@"touch_down_until_release",
        @"active_touches":@(owners.size()),@"actual_samples":@(actualSamples),@"look_samples":@(lookSamples),@"look_total_points":@[@(lookDX),@(lookDY)],
        @"rendered_frames":@(renderedFrames),@"merged_overlay_frames":@(mergedOverlayFrames),@"separate_overlay_frames":@(separateOverlayFrames),@"cancel_transitions":@(cancellations),@"mfi_filter_matches":@(mfiFilterMatches),@"controls":rectangles,@"fire_layout":fireLayout,
        @"navigation":glassNavigationDiagnostics?:@{},
        @"map_gallery":mapGalleryDiagnostics?:@{},
        @"settings_scroll":settingsScrollDiagnostics(),
        @"console":consoleHeaderDiagnostics(),
        @"inventory":inventoryDiagnostics?:@{},
        @"layout":@{@"editing":@(editing),@"loaded":@(layoutLoaded),@"saved_controls":savedLayout?:@{},@"custom_buttons":savedCustomButtons?:@[],@"held_keyboard_keys":@(keyboardOwners.size()),@"saves_this_process":@(layoutSaves),@"error":layoutError?:@"",@"editor":layoutEditor?[layoutEditor diagnostics]:@{}}};
    return d;
}
// UIKit's host already consumes Foundation objects. Avoid encoding and then
// immediately decoding this entire snapshot in its main-thread timer.
extern "C" __attribute__((visibility("default"))) NSDictionary *SourceTouchDiagnosticsSnapshot() {
    return touchDiagnosticsSnapshot();
}
extern "C" size_t SourceTouchCopyDiagnosticsJSON(char *buffer,size_t capacity) {
    if(!buffer || !capacity)return 0;
    NSData *json=[NSJSONSerialization dataWithJSONObject:touchDiagnosticsSnapshot() options:0 error:nil];
    if(json.length+1>capacity){buffer[0]=0;return 0;}std::memcpy(buffer,json.bytes,json.length);buffer[json.length]=0;return json.length;
}

extern "C" size_t SourceTouchRoutingSelfTestJSON(char *buffer,size_t capacity) {
    if(!buffer || !capacity)return 0;
    std::lock_guard<std::recursive_mutex> lock(mutex);
    auto copyJSON=[&](NSDictionary *value)->size_t {
        NSData *json=[NSJSONSerialization dataWithJSONObject:value options:NSJSONWritingPrettyPrinted error:nil];
        if(json.length+1>capacity){buffer[0]=0;return 0;}
        std::memcpy(buffer,json.bytes,json.length);buffer[json.length]=0;return json.length;
    };
    if(!available || !gameplay() || controls.empty() || !owners.empty())
        return copyJSON(@{@"status":@"not_ready",@"reason":@"Requires a rendered gameplay layout and zero active real touches"});
    // Snapshot real framework objects and counters. Only callbacks and each
    // test copy's native pointer are substituted. Tests call the exact routing
    // functions used by UIKit, with the current real-device layout geometry.
    auto savedControls=controls;
    auto savedUIKitRoutes=uikitRoutes;uikitRoutes.clear();
    auto savedCallbacks=callbacks;void *savedUser=callbackUser;
    auto savedLookSampleCallback=lookSampleCallback;lookSampleCallback=nullptr;
    auto savedKeyboardSink=keyboardSink;auto savedKeyboardOwners=keyboardOwners;keyboardOwners.clear();
    auto savedMoveOwner=moveOwner,savedLookOwner=lookOwner;
    uint64_t savedLookSamples=lookSamples,savedCancels=cancellations;
    double savedDX=lookDX,savedDY=lookDY;
    float savedMoveX=moveX,savedMoveY=moveY;
    unsigned savedActionOwners[SourceTouchActionCount];std::memcpy(savedActionOwners,actionOwners,sizeof(actionOwners));
    struct Probe {
        bool pressed[SourceTouchActionCount]={};
        unsigned downs[SourceTouchActionCount]={},ups[SourceTouchActionCount]={};
        float x=0,y=0;
        std::vector<std::pair<float,float>> looks;
        std::vector<std::pair<int,bool>> keyboard;
        unsigned resets=0,commands=0,selections=0;
        unsigned commandCounts[SourceTouchCommandCount]={};
    } probe;
    callbacks={};callbacks.struct_size=sizeof(callbacks);callbackUser=&probe;
    keyboardSink=[](int code,bool down){((Probe*)callbackUser)->keyboard.push_back({code,down});};
    callbacks.action=[](void*u,SourceTouchAction a,bool p,double){auto &v=*(Probe*)u;v.pressed[a]=p;if(p)++v.downs[a];else ++v.ups[a];};
    callbacks.move=[](void*u,float x,float y,double){auto &v=*(Probe*)u;v.x=x;v.y=y;};
    callbacks.look=[](void*u,float x,float y,double){((Probe*)u)->looks.push_back({x,y});};
    callbacks.command=[](void*u,SourceTouchCommand c,double){auto &p=*(Probe*)u;++p.commands;if(c>=0 && c<SourceTouchCommandCount)++p.commandCounts[c];};
    callbacks.weapon_select=[](void*u,int,int,double){++((Probe*)u)->selections;};
    callbacks.reset=[](void*u,double){auto &v=*(Probe*)u;v.looks.clear();++v.resets;};
    for(auto &c:controls){c.native=nil;c.owners=0;}
    owners.clear();moveOwner=lookOwner=0;std::memset(actionOwners,0,sizeof(actionOwners));
    NSMutableArray *cases=[NSMutableArray array];bool passed=true;
    auto record=[&](NSString *name,bool ok,NSDictionary *details) {
        passed=passed && ok;[cases addObject:@{@"case":name,@"passed":@(ok),@"details":details?:@{}}];
    };
    bool menuRoute=routeUIKitTouch(9001,SourceTouchBegan,false)==SDLRoute &&
        routeUIKitTouch(9001,SourceTouchMoved,true)==SDLRoute &&
        routeUIKitTouch(9001,SourceTouchEnded,true)==SDLRoute && uikitRoutes.empty();
    record(@"menu_to_game_keeps_matching_sdl_release",menuRoute,@{});
    bool customRoute=routeUIKitTouch(9002,SourceTouchBegan,true)==CustomRoute &&
        routeUIKitTouch(9002,SourceTouchMoved,false)==CustomRoute;
    cancel(0);
    customRoute=customRoute && routeUIKitTouch(9002,SourceTouchEnded,false)==CancelledRoute && uikitRoutes.empty();
    record(@"cancelled_game_touch_never_enters_menu",customRoute,@{});
    bool mixed=routeUIKitTouch(9003,SourceTouchBegan,false)==SDLRoute &&
        routeUIKitTouch(9004,SourceTouchBegan,true)==CustomRoute &&
        routeUIKitTouch(9003,SourceTouchEnded,true)==SDLRoute &&
        routeUIKitTouch(9004,SourceTouchCancelled,false)==CustomRoute && uikitRoutes.empty();
    record(@"independent_menu_and_game_finger_routes",mixed,@{});
    auto find=[&](const char *name)->size_t {for(size_t i=0;i<controls.size();++i)if(controls[i].label==name)return i;return controls.size();};
    CGPoint free=CGPointZero;bool found=false;
    for(int row=1;row<20 && !found;++row)for(int col=1;col<20 && !found;++col) {
        CGPoint p=CGPointMake(lookRect.origin.x+lookRect.size.width*col/20,lookRect.origin.y+lookRect.size.height*row/20);
        if(lookAllowed(p) && lookAllowed(CGPointMake(p.x+24,p.y+4))){free=p;found=true;}
    }
    bool halfGeometry=lookRect.origin.x==viewSize.width*.5 && lookRect.origin.y==0 && lookRect.size.width==viewSize.width*.5 && lookRect.size.height==viewSize.height;
    NSMutableArray *bands=[NSMutableArray array];bool halfRouting=halfGeometry;
    for(double fraction : {.01,.12,.35,.65,.88,.98}) {
        bool routed=false;CGPoint candidate=CGPointZero;
        for(int col=1;col<100 && !routed;++col) {
            CGPoint p=CGPointMake(viewSize.width*(.5+.005*col),viewSize.height*fraction);
            if(!lookAllowed(p) || !lookAllowed(CGPointMake(p.x+1,p.y)))continue;
            cancel(0);begin(810,p,.01);motion(810,CGPointMake(p.x+1,p.y),.02);end(810,.03);
            routed=probe.looks.size()==1 && probe.looks[0].first==1;candidate=p;
        }
        halfRouting=halfRouting && routed;[bands addObject:@{@"height_fraction":@(fraction),@"passed":@(routed),@"point":@[@(candidate.x),@(candidate.y)]}];
    }
    cancel(0);begin(811,CGPointMake(viewSize.width*.49,viewSize.height*.5),.01);motion(811,CGPointMake(viewSize.width*.9,viewSize.height*.5),.02);end(811,.03);
    record(@"entire_right_half_and_left_start_exclusion",halfRouting && probe.looks.empty(),@{@"geometry":@(halfGeometry),@"height_bands":bands});
    CGPoint movement=CGPointZero;bool movementFound=false;
    for(int row=1;row<20 && !movementFound;++row)for(int col=1;col<20 && !movementFound;++col) {
        CGPoint p=CGPointMake(moveRect.size.width*col/20,moveRect.size.height*row/20);
        if(moveAllowed(p) && moveAllowed(CGPointMake(p.x+moveRadius*.5,p.y))){movement=p;movementFound=true;}
    }
    const size_t fi=find("attack"),di=find("duck");
    bool fixture=found && movementFound && find("move")==controls.size() && fi<controls.size() && di<controls.size();
    record(@"actual_layout_fixture",fixture,@{@"control_count":@(controls.size()),@"unmasked_look_point":@[@(free.x),@(free.y)]});
    if(fixture) {
        bool leftGeometry=moveRect.origin.x==0 && moveRect.origin.y==0 && moveRect.size.width==viewSize.width*.5 && moveRect.size.height==viewSize.height;
        NSMutableArray *moveBands=[NSMutableArray array];bool leftRouting=leftGeometry;
        for(double fraction : {.01,.12,.35,.65,.88,.98}) {
            bool routed=false;
            for(int col=1;col<100 && !routed;++col) {
                CGPoint p=CGPointMake(moveRect.size.width*col/100,viewSize.height*fraction);
                if(!moveAllowed(p) || !moveAllowed(CGPointMake(p.x+1,p.y)))continue;
                cancel(0);begin(812,p,.01);motion(812,CGPointMake(p.x+1,p.y),.02);
                routed=moveOwner==812 && probe.x>0 && probe.y==0 && probe.looks.empty();end(812,.03);
            }
            leftRouting=leftRouting && routed;[moveBands addObject:@{@"height_fraction":@(fraction),@"passed":@(routed)}];
        }
        record(@"entire_left_half_swipe_without_joystick",leftRouting,@{@"geometry":@(leftGeometry),@"height_bands":moveBands});

        cancel(0);begin(813,movement,.01);
        bool originIdle=probe.x==0 && probe.y==0;
        motion(813,CGPointMake(movement.x+moveRadius*.5,movement.y),.02);
        bool linear=std::fabs(probe.x-.5f)<1e-6f && probe.y==0;
        begin(814,movement,.03);motion(814,CGPointMake(movement.x+1,movement.y),.04);
        bool exclusive=moveOwner==813 && std::fabs(probe.x-.5f)<1e-6f;
        end(814,.05);exclusive=exclusive && moveOwner==813 && std::fabs(probe.x-.5f)<1e-6f;
        motion(813,free,.06);
        bool separate=std::hypot(probe.x,probe.y)>0 && probe.looks.empty() && moveOwner==813;
        motion(813,CGPointMake(movement.x+moveRadius*.5,movement.y),.07);
        end(813,.08);
        record(@"relative_origin_linear_move_release_and_exclusive_owner",originIdle && linear && exclusive && separate && moveOwner==0 && probe.x==0 && probe.y==0,
            @{@"origin_idle":@(originIdle),@"linear_half_speed":@(linear),@"second_left_finger_ignored":@(exclusive),@"right_half_does_not_become_look":@(separate)});

        cancel(0);begin(815,movement,.01);motion(815,center(controls[fi]),.02);
        record(@"movement_over_button_keeps_capture_without_firing",moveOwner==815 && std::hypot(probe.x,probe.y)>0 && !probe.pressed[SourceTouchAttack] && probe.looks.empty(),@{});end(815,.03);

        cancel(1);CGPoint m=movement;
        begin(101,m,2);begin(102,center(controls[fi]),2);begin(103,free,2);begin(104,center(controls[di]),2);
        motion(101,CGPointMake(m.x+moveRadius*.5,m.y),3);
        motion(103,CGPointMake(free.x+12.375,free.y+2.125),3);
        record(@"four_simultaneous_owners",owners.size()==4 && probe.pressed[SourceTouchAttack] && probe.pressed[SourceTouchDuck] && std::fabs(probe.x-.5f)<1e-6f && probe.looks.size()==1 && probe.looks[0].first==12.375f && probe.looks[0].second==2.125f,
            @{@"owners":@(owners.size()),@"move_x":@(probe.x),@"fire":@(probe.pressed[SourceTouchAttack]),@"duck":@(probe.pressed[SourceTouchDuck]),@"look_samples":@(probe.looks.size())});
        end(101,4);end(102,4);end(103,4);end(104,4);
        record(@"end_releases_all_owners",owners.empty() && moveOwner==0 && lookOwner==0 && probe.x==0 && probe.y==0 && !probe.pressed[SourceTouchAttack] && !probe.pressed[SourceTouchDuck],@{});

        cancel(5);unsigned down=probe.downs[SourceTouchAttack],up=probe.ups[SourceTouchAttack];
        begin(201,center(controls[fi]),6);begin(202,center(controls[fi]),6);end(201,7);
        bool held=probe.pressed[SourceTouchAttack] && actionOwners[SourceTouchAttack]==1 && probe.downs[SourceTouchAttack]==down+1 && probe.ups[SourceTouchAttack]==up;
        end(202,8);
        record(@"same_action_reference_count",held && !probe.pressed[SourceTouchAttack] && actionOwners[SourceTouchAttack]==0 && probe.ups[SourceTouchAttack]==up+1,@{});

        NSMutableArray *buttonCases=[NSMutableArray array];bool allButtons=true;
        for(size_t i=0;i<controls.size();++i) {
            cancel(9);begin(300+i,center(controls[i]),10);motion(300+i,free,11);motion(300+i,CGPointMake(free.x+10,free.y),12);
            motion(300+i,movement,12.5);
            end(300+i,13);
            bool ok=probe.looks.empty() && probe.x==0 && probe.y==0;allButtons=allButtons && ok;
            [buttonCases addObject:@{@"control":@(controls[i].label.c_str()),@"look_samples":@(probe.looks.size()),@"passed":@(ok)}];
        }
        record(@"every_control_start_never_becomes_look_or_move",allButtons,@{@"controls":buttonCases});

        bool oneShot=true;NSMutableArray *commandCases=[NSMutableArray array];
        for(const char *name : {"inspect","drop"}) {
            const size_t i=find(name);bool ok=i<controls.size();
            if(ok) {
                cancel(0);const auto command=(SourceTouchCommand)controls[i].value;const unsigned before=probe.commandCounts[command];
                begin(820,center(controls[i]),.01);motion(820,free,.02);motion(820,CGPointMake(free.x+10,free.y),.03);end(820,.04);
                ok=controls[i].kind==Command && probe.commandCounts[command]==before+1 && probe.looks.empty();
            }
            oneShot=oneShot && ok;[commandCases addObject:@{@"control":@(name),@"passed":@(ok)}];
        }
        record(@"inspect_and_drop_trigger_once_and_never_look",oneShot,@{@"controls":commandCases});

        cancel(14);begin(401,free,15);motion(401,CGPointMake(free.x+20,free.y),16);
        motion(401,CGPointMake(-100,-100),17);motion(401,CGPointMake(free.x+20,free.y),18);motion(401,CGPointMake(free.x+24,free.y),19);
        bool boundary=probe.looks.size()==2 && probe.looks[0].first==20 && probe.looks[1].first==4;
        record(@"look_exit_reentry_has_no_jump",boundary,@{@"sample_count":@(probe.looks.size()),@"expected_total_dx":@24});
        end(401,20);

        cancel(21);begin(501,free,22);motion(501,CGRectGetMinX(controls[fi].rect)<0?CGPointZero:center(controls[fi]),23);
        motion(501,free,24);motion(501,CGPointMake(free.x+1.125,free.y+.25),25);
        record(@"look_over_button_excludes_and_refreshes_baseline",probe.looks.size()==1 && probe.looks[0].first==1.125f && probe.looks[0].second==.25f,@{});end(501,26);

        double fastX=0,fastY=0,slowX=0,slowY=0;
        for(int run=0;run<2;++run) {
            cancel(27+run);begin(601,free,30+run);
            for(int i=1;i<=32;++i)motion(601,CGPointMake(free.x+i*.125,free.y+i*.0625),30+run+i*(run?.01:.001));
            double x=0,y=0;for(const auto &sample:probe.looks){x+=sample.first;y+=sample.second;}
            if(run){slowX=x;slowY=y;}else{fastX=x;fastY=y;}end(601,40+run);
        }
        record(@"equal_path_duration_independent_float_samples",fastX==slowX && fastY==slowY && fastX==4 && fastY==2,@{@"fast":@[@(fastX),@(fastY)],@"slow":@[@(slowX),@(slowY)]});

        cancel(42);begin(701,center(controls[fi]),43);begin(702,center(controls[di]),43);begin(703,m,43);begin(704,free,43);motion(704,CGPointMake(free.x+5,free.y),44);
        unsigned resetBefore=probe.resets;cancel(45);bool clean=owners.empty() && moveOwner==0 && lookOwner==0 && probe.x==0 && probe.y==0 && probe.looks.empty() && probe.resets==resetBefore+1;
        for(int i=0;i<SourceTouchActionCount;++i)clean=clean && !probe.pressed[i] && actionOwners[i]==0;
        record(@"cancel_clears_held_move_and_pending_look",clean,@{});
    }
    // Temporary hit targets exercise the real begin/motion/end/cancel router.
    // Replace only the key sink, so validation never sends game commands.
    cancel(50);controls.clear();probe.keyboard.clear();
    Control first;first.native=nil;first.rect=CGRectMake(viewSize.width*.2,viewSize.height*.8,44,44);first.kind=Keyboard;first.value=SDL_SCANCODE_W;first.circle=false;first.enabled=true;first.label="custom.test.one";
    Control second=first;second.rect.origin.x=viewSize.width*.7;second.label="custom.test.two";controls={first,second};
    begin(9501,center(controls[0]),51);begin(9502,center(controls[1]),51);
    motion(9501,CGPointMake(viewSize.width*.9,viewSize.height*.1),52);
    motion(9502,CGPointMake(viewSize.width*.1,viewSize.height*.1),52);
    bool keyHeld=keyboardOwners[SDL_SCANCODE_W]==2 && probe.keyboard.size()==1 && probe.keyboard[0]==std::make_pair((int)SDL_SCANCODE_W,true) && probe.looks.empty() && probe.x==0 && probe.y==0;
    end(9501,53);keyHeld=keyHeld && keyboardOwners[SDL_SCANCODE_W]==1 && probe.keyboard.size()==1;
    end(9502,54);
    record(@"custom_keyboard_reference_counts_and_no_swipe_leak",keyHeld && keyboardOwners.empty() && probe.keyboard.size()==2 && probe.keyboard.back()==std::make_pair((int)SDL_SCANCODE_W,false),@{});
    probe.keyboard.clear();begin(9503,center(controls[0]),55);cancel(56);
    record(@"custom_keyboard_cancel_releases_once",owners.empty() && keyboardOwners.empty() && probe.keyboard.size()==2 && !probe.keyboard.back().second,@{});
    probe.keyboard.clear();controls[0].value=SDL_SCANCODE_TAB;
    routeUIKitTouch(9504,SourceTouchBegan,true);begin(9504,center(controls[0]),57);cancel(58,true);
    bool tabKept=owners.count(9504) && keyboardOwners.count(SDL_SCANCODE_TAB) && probe.keyboard.size()==1 && uikitRoutes[9504]==CustomRoute;
    end(9504,59);routeUIKitTouch(9504,SourceTouchEnded,true);
    record(@"held_tab_survives_scoreboard_and_releases",tabKept && keyboardOwners.empty() && owners.empty() && probe.keyboard.size()==2 && !probe.keyboard.back().second,@{});
    // Restore every real routing object, aggregate state and metric. Neither
    // actual Source callbacks nor framework touch input was used by the test.
    controls=std::move(savedControls);callbacks=savedCallbacks;callbackUser=savedUser;
    lookSampleCallback=savedLookSampleCallback;
    keyboardSink=savedKeyboardSink;keyboardOwners=std::move(savedKeyboardOwners);
    uikitRoutes=std::move(savedUIKitRoutes);
    owners.clear();moveOwner=savedMoveOwner;lookOwner=savedLookOwner;
    std::memcpy(actionOwners,savedActionOwners,sizeof(actionOwners));
    lookSamples=savedLookSamples;cancellations=savedCancels;lookDX=savedDX;lookDY=savedDY;
    moveX=savedMoveX;moveY=savedMoveY;
    return copyJSON(@{@"status":passed?@"passed":@"failed",@"scope":@"same real router / actual layout / synthetic coordinates / no fabricated UITouch",@"cases":cases});
}

// The keyboard layout guide is expressed in the same UIView coordinates as
// the actual touch path. Keep the console above a docked software keyboard.
extern "C" void SourceTouchConsoleSetVisible(bool value) {
    NSCAssert(NSThread.isMainThread,@"Console UI belongs to the Source main thread");
    consoleVisible=value;
    updateConsoleHeaderLayout();
}
extern "C" bool SourceTouchConsoleIsVisible(void) {return consoleVisible;}
extern "C" float SourceTouchConsoleAvailableHeight(void) {
    if(!NSThread.isMainThread || !host || host.bounds.size.height<=0)return 1.0f;
    const CGRect keyboard=host.keyboardLayoutGuide.layoutFrame;
    const CGFloat bottom=CGRectGetMinY(keyboard);
    return std::clamp((float)(bottom/host.bounds.size.height),0.20f,1.0f);
}
