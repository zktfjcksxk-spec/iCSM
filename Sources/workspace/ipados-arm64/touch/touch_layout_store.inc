// Included inside touch_ui_ios.mm's private namespace. Coordinates are
// fractions of the UIKit view, independent of the Source render resolution.
static bool editing=false,layoutLoaded=false;
static __strong NSMutableDictionary<NSString*,NSDictionary*> *savedLayout;
static __strong NSMutableArray<NSDictionary*> *savedCustomButtons;
static std::map<std::string,CGRect> defaultRects;
static uint64_t layoutSaves=0;
static __strong NSString *layoutError;

static NSURL *layoutURL() {
    NSURL *documents=[NSFileManager.defaultManager URLsForDirectory:NSDocumentDirectory inDomains:NSUserDomainMask].firstObject;
    return [[documents URLByAppendingPathComponent:@"I5" isDirectory:YES] URLByAppendingPathComponent:@"touch-layout.json"];
}
static bool knownLayoutName(NSString *name) {
    return [@[@"pause",@"buy",@"inspect",@"drop",@"scoreboard",@"jump",@"attack",@"attack2",@"duck",@"weapon1",@"weapon2",@"weapon3",@"weapon4",@"weapon5",@"use",@"reload",@"move",@"walk"] containsObject:name];
}
static bool validLayoutRect(id value) {
    if(![value isKindOfClass:NSDictionary.class])return false;
    for(NSString *key in @[@"x",@"y",@"width",@"height"]) {
        id n=value[key];if(![n isKindOfClass:NSNumber.class] || !std::isfinite([n doubleValue]))return false;
    }
    double x=[value[@"x"] doubleValue],y=[value[@"y"] doubleValue];
    double w=[value[@"width"] doubleValue],h=[value[@"height"] doubleValue];
    return x>=0 && y>=0 && w>0 && h>0 && x+w<=1.000001 && y+h<=1.000001;
}
static void loadLayout() {
    if(layoutLoaded)return;layoutLoaded=true;savedLayout=[NSMutableDictionary dictionary];savedCustomButtons=[NSMutableArray array];
    NSData *data=[NSData dataWithContentsOfURL:layoutURL()];if(!data)return;
    NSError *error=nil;id root=[NSJSONSerialization JSONObjectWithData:data options:0 error:&error];
    if(![root isKindOfClass:NSDictionary.class] || (![[root objectForKey:@"version"] isEqual:@1] && ![[root objectForKey:@"version"] isEqual:@2]) || ![root[@"controls"] isKindOfClass:NSDictionary.class]) {
        layoutError=@"已保存的布局无法读取，暂用默认布局。保存可重新创建布局。";return;
    }
    if([root[@"version"] isEqual:@2]) {
        id custom=root[@"custom_buttons"];
        if(![custom isKindOfClass:NSArray.class] || [custom count]>customButtonLimit) {
            layoutError=@"自定义按键数据无效，已保留可用的原有按键。";
        } else for(id entry in custom) {
            if(![entry isKindOfClass:NSDictionary.class] || !validCustomID(entry[@"id"]) ||
               ![entry[@"key"] isKindOfClass:NSNumber.class] || [entry[@"key"] doubleValue]!=[entry[@"key"] intValue] ||
               !keyboardTitle([entry[@"key"] intValue]) || customDefinition(entry[@"id"],savedCustomButtons)) {
                layoutError=@"部分自定义按键数据无效，已忽略该按键。";continue;
            }
            [savedCustomButtons addObject:@{@"id":entry[@"id"],@"key":entry[@"key"]}];
        }
    }
    NSDictionary *entries=root[@"controls"];
    for(id name in entries) {
        if(![name isKindOfClass:NSString.class] || (!knownLayoutName(name) && !customDefinition(name,savedCustomButtons)) || !validLayoutRect(entries[name])) {
            layoutError=@"部分按键布局包含无效尺寸，已使用该按键的默认位置。";continue;
        }
        savedLayout[name]=entries[name];
    }
}
static CGRect boundedLayoutRect(CGRect rect) {
    const CGFloat minX=safe.left,minY=safe.top;
    const CGFloat maxX=std::max(minX+1,viewSize.width-safe.right),maxY=std::max(minY+1,viewSize.height-safe.bottom);
    rect.size.width=std::clamp<CGFloat>(rect.size.width,8,maxX-minX);
    rect.size.height=std::clamp<CGFloat>(rect.size.height,8,maxY-minY);
    rect.origin.x=std::clamp<CGFloat>(rect.origin.x,minX,maxX-rect.size.width);
    rect.origin.y=std::clamp<CGFloat>(rect.origin.y,minY,maxY-rect.size.height);return rect;
}
static NSDictionary *normalizedLayoutRect(CGRect r) {
    return @{@"x":@(r.origin.x/viewSize.width),@"y":@(r.origin.y/viewSize.height),
        @"width":@(r.size.width/viewSize.width),@"height":@(r.size.height/viewSize.height)};
}
static CGRect resolvedLayoutRect(NSString *name,CGRect fallback,NSDictionary *layout) {
    NSDictionary *entry=layout[name];if(!entry)return boundedLayoutRect(fallback);
    return boundedLayoutRect(CGRectMake([entry[@"x"] doubleValue]*viewSize.width,[entry[@"y"] doubleValue]*viewSize.height,
        [entry[@"width"] doubleValue]*viewSize.width,[entry[@"height"] doubleValue]*viewSize.height));
}
static CGRect applyLayout(NSString *name,CGRect rect) {
    loadLayout();defaultRects[name.UTF8String]=rect;return resolvedLayoutRect(name,rect,savedLayout);
}
