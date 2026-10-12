// Repeatable on-device calibration. The engine-facing callbacks run only from
// the Source/main frame thread; no background worker touches engine objects.
#include "frame_calibration_settings.h"
#include <dlfcn.h>
#import <MetalFX/MetalFX.h>
#import <sys/utsname.h>

static NSString *const FGCheckpoint=@"ICSMFrameCalibrationRecoveryV1";
static NSString *const FGProfile=@"ICSMFrameCalibrationProfileV1";
static NSString *const FGRelease=@"-right; -left; -forward; -back; -moveleft; -moveright; -attack; -attack2; -lookatweapon";
static const NSInteger FGScales[]={50,67,75};
static void (*fgQueue)(const char *)=nullptr;
static size_t (*fgScene)(char *,size_t)=nullptr;
static bool fgRestoreCommitted=false;
static NSDictionary *FGScene() {
    char buffer[8192]={};size_t size=fgScene?fgScene(buffer,sizeof(buffer)):0;
    if(!size || size>=sizeof(buffer))return @{};
    id value=[NSJSONSerialization JSONObjectWithData:[NSData dataWithBytes:buffer length:size] options:0 error:nil];
    return [value isKindOfClass:NSDictionary.class]?value:@{};
}
static NSDictionary *FGStats() {
    static NSDictionary *(*copy)()=nullptr;
    if(!copy) {
        NSString *path=[NSBundle.mainBundle.privateFrameworksPath stringByAppendingPathComponent:@"source_metal.framework/source_metal"];
        void *module=dlopen(path.fileSystemRepresentation,RTLD_NOLOAD|RTLD_NOW);
        if(module){copy=(NSDictionary *(*)())dlsym(module,"SourceMetalFrameInterpolationSnapshot");dlclose(module);}
    }
    return copy?copy():@{};
}
static void FGCommand(NSString *text) { if(fgQueue)fgQueue(text.UTF8String); }
static NSDictionary *FGIdentity(NSDictionary *scene) {
    struct utsname model={};uname(&model);
    id<MTLDevice> device=metalDevice?:MTLCreateSystemDefaultDevice();
    return @{@"model":@(model.machine),@"gpu":device.name?:@"",@"os":UIDevice.currentDevice.systemVersion,
        @"renderer_revision":@198,@"preset":scene[@"values"][@"icsm_quality_preset"]?:@(-1),
        @"scene_percent":scene[@"values"][@"icsm_scene_render_percent"]?:@(-1),
        @"skin_shader":scene[@"values"][@"icsm_skin_shader_detail"]?:@(-1),
        @"skin_texture":scene[@"values"][@"icsm_skin_texture_detail"]?:@(-1),
        @"motion_blur":scene[@"values"][@"mat_motion_blur_enabled"]?:@(-1),
        @"render_width":scene[@"render_width"]?:@0,@"render_height":scene[@"render_height"]?:@0};
}
static NSString *FGRestore(NSDictionary *values) {
    NSMutableString *text=[NSMutableString stringWithFormat:@"%@; disconnect; ",FGRelease];
    for(const char *key:kFrameCalibrationSettings) {
        id value=values[@(key)];if(![value isKindOfClass:NSNumber.class])continue;
        double number=[value doubleValue];if(!std::isfinite(number) || std::abs(number)>100000)continue;
        [text appendFormat:@"%s %.9g; ",key,number];
    }
    NSString *mode=values[@"bot_quota_mode"];
    if([@[@"normal",@"fill",@"match"] containsObject:mode])[text appendFormat:@"bot_quota_mode %@; ",mode];
    [text appendString:@"host_writeconfig; icsm_frame_calibration_restore_complete"];
    return text;
}
static BOOL FGRestored(NSDictionary *original,NSDictionary *scene) {
    NSDictionary *values=scene[@"values"];if(!values || [scene[@"connected"] boolValue])return NO;
    // Verify gameplay preferences, including the unlocked-cap memory. Server
    // state is discarded by disconnect; it need not exist before first load.
    for(int i=0;i<12;++i) {
        NSString *key=@(kFrameCalibrationSettings[i]);NSNumber *expected=original[key];
        if(expected && (!values[key] || std::abs([expected doubleValue]-[values[key] doubleValue])>.001))return NO;
    }
    return YES;
}
static NSDictionary *FGRecommendation(NSDictionary *result) {
    if(!result)return nil;
    NSMutableDictionary *choice=[result mutableCopy];NSInteger scale=[result[@"color_scale"] integerValue];
    // A useful conservative recommendation can still fall short of 120 Hz.
    // Never enable generation when the measured native cadence is already poor
    // or generation supplied less than ten additional displayed frames/sec.
    if(!scale) {
        double best=INFINITY;
        for(NSDictionary *row in result[@"summaries"]) {
            double render=[row[@"minimum_render_fps"] doubleValue],display=[row[@"minimum_display_fps"] doubleValue];
            if(render<55 || display<render+10 || [row[@"gpu_errors"] intValue] || [row[@"samples"] intValue]!=[result[@"maps"] intValue]*4)continue;
            double score=[row[@"worst_missed_refresh_fraction"] doubleValue]+(60-render)/60+(120-display)/120+
                .02*std::max(0.,[row[@"worst_interval_p95_ms"] doubleValue]-8.333)/8.333;
            if(score<best){best=score;scale=[row[@"color_scale"] integerValue];}
        }
    }
    choice[@"color_scale"]=@(scale);choice[@"recommendation_available"]=@(scale==50 || scale==67 || scale==75);choice[@"selection_revision"]=@2;
    return choice;
}
static NSDictionary *FGSpawn(NSString *map) {
    // Read real shipped BSP entities rather than guessing camera coordinates.
    NSString *path=[NSHomeDirectory() stringByAppendingFormat:@"/Documents/game-assets/csgo/maps/%@.bsp",map];
    NSFileHandle *file=[NSFileHandle fileHandleForReadingAtPath:path];if(!file)return nil;
    NSData *header=[file readDataOfLength:1036];if(header.length!=1036 || memcmp(header.bytes,"VBSP",4)){[file closeFile];return nil;}
    uint32_t offset=0,length=0;memcpy(&offset,(const char*)header.bytes+8,4);memcpy(&length,(const char*)header.bytes+12,4);
    unsigned long long total=[file seekToEndOfFile];
    if(offset<1036 || !length || length>16*1024*1024 || (uint64_t)offset+length>total){[file closeFile];return nil;}
    [file seekToFileOffset:offset];NSData *data=[file readDataOfLength:length];[file closeFile];
    NSString *entities=[[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];if(!entities)return nil;
    NSRegularExpression *blocks=[NSRegularExpression regularExpressionWithPattern:@"\\{([^{}]*)\\}" options:0 error:nil];
    NSRegularExpression *pairs=[NSRegularExpression regularExpressionWithPattern:@"\"([^\"]+)\"\\s*\"([^\"]*)\"" options:0 error:nil];
    NSMutableDictionary *spawns=[NSMutableDictionary new];
    for(NSTextCheckingResult *block in [blocks matchesInString:entities options:0 range:NSMakeRange(0,entities.length)]) {
        NSString *entry=[entities substringWithRange:[block rangeAtIndex:1]];NSMutableDictionary *fields=[NSMutableDictionary new];
        for(NSTextCheckingResult *pair in [pairs matchesInString:entry options:0 range:NSMakeRange(0,entry.length)])
            fields[[entry substringWithRange:[pair rangeAtIndex:1]]]=[entry substringWithRange:[pair rangeAtIndex:2]];
        NSString *role=[fields[@"classname"] isEqual:@"info_player_counterterrorist"]?@"ct":[fields[@"classname"] isEqual:@"info_player_terrorist"]?@"t":nil;
        if(!role || spawns[role])continue;
        NSMutableArray *origin=[NSMutableArray new],*angles=[NSMutableArray new];
        for(NSString *field in @[ @"origin",@"angles" ]) {
            NSScanner *scanner=[NSScanner scannerWithString:fields[field]?:@"0 0 0"];NSMutableArray *target=[field isEqual:@"origin"]?origin:angles;
            double number;while([scanner scanDouble:&number]){if(!std::isfinite(number) || std::abs(number)>100000)return nil;[target addObject:@(number)];}
        }
        if(origin.count==3 && angles.count==3)spawns[role]=@{@"origin":origin,@"angles":angles};
    }
    return spawns[@"ct"] && spawns[@"t"]?spawns:nil;
}

@interface ICSMFrameCalibration : NSObject
@property(nonatomic,strong) UIView *panel;
@property(nonatomic,strong) UILabel *label;
@property(nonatomic,strong) UIProgressView *progress;
@property(nonatomic,strong) UIButton *cancel;
@property(nonatomic,strong) NSDictionary *original;
@property(nonatomic,strong) NSDictionary *identity;
@property(nonatomic,strong) NSArray<NSDictionary *> *maps;
@property(nonatomic,strong) NSMutableArray<NSDictionary *> *rows;
@property(nonatomic,strong) NSDictionary *pose;
@property(nonatomic,strong) NSDictionary *result;
@property(nonatomic,copy) NSString *lastPhase;
@property(nonatomic,strong) id backgroundObserver;
@property(nonatomic,assign) NSInteger mapIndex,caseIndex,stage;
@property(nonatomic,assign) double stageTime,lastTick;
@property(nonatomic,assign) unsigned long long skippedBefore;
@property(nonatomic,assign) BOOL restoring;
- (void)open;
- (void)start:(BOOL)full;
- (void)tick;
- (void)finish:(NSString *)reason;
- (void)apply;
@end
static ICSMFrameCalibration *frameCalibration;
@implementation ICSMFrameCalibration
- (UIViewController *)presenter {return host.window.rootViewController;}
- (void)writeStatus:(NSString *)phase {
    self.lastPhase=phase;
    NSMutableDictionary *value=[@{@"schema":@1,@"phase":phase,@"running":@(calibrationRunning),@"map_index":@(self.mapIndex),
        @"restoring":@(self.restoring),@"preferences_restored":@(!calibrationRunning && !self.restoring),
        @"case_index":@(self.caseIndex),@"identity":self.identity?:@{},@"samples":self.rows?:@[],@"result":self.result?:@{},
        @"updated":@([[NSDate date] timeIntervalSince1970])} mutableCopy];
    NSString *path=[NSHomeDirectory() stringByAppendingPathComponent:@"Documents/I5/frame-calibration.json"];
    [[NSJSONSerialization dataWithJSONObject:value options:NSJSONWritingPrettyPrinted error:nil] writeToFile:path atomically:YES];
}
- (void)message:(NSString *)title body:(NSString *)body {
    UIViewController *presenter=self.presenter;if(!presenter || presenter.presentedViewController)return;
    UIAlertController *alert=[UIAlertController alertControllerWithTitle:title message:body preferredStyle:UIAlertControllerStyleAlert];
    [alert addAction:[UIAlertAction actionWithTitle:@"关闭" style:UIAlertActionStyleCancel handler:nil]];
    [presenter presentViewController:alert animated:YES completion:nil];
}
- (void)open {
    if(calibrationRunning)return;
    if([FGScene()[@"connected"] boolValue]){[self message:@"插帧性能测试" body:@"请先退出当前对局，在大厅开始测试。"] ;return;}
    if(self.result){[self showResult];return;}
    UIViewController *presenter=self.presenter;if(!presenter || presenter.presentedViewController)return;
    UIAlertController *alert=[UIAlertController alertControllerWithTitle:@"插帧性能测试" message:@"在当前画质下，以固定机位重复测试连续转向、开枪与5v5机器人场景，比较3档插帧细节。\n快速测试3张图；完整测试全部竞技地图。测试期间请勿操作或切换应用，并保持日常使用的散热条件。结束后恢复原设置，推荐值需手动应用。" preferredStyle:UIAlertControllerStyleAlert];
    [alert addAction:[UIAlertAction actionWithTitle:@"快速测试（约7分钟）" style:UIAlertActionStyleDefault handler:^(UIAlertAction *a){[self start:NO];}]];
    [alert addAction:[UIAlertAction actionWithTitle:@"完整地图测试（约30分钟）" style:UIAlertActionStyleDefault handler:^(UIAlertAction *a){[self start:YES];}]];
    NSDictionary *saved=[NSUserDefaults.standardUserDefaults dictionaryForKey:FGProfile];
    if(saved) [alert addAction:[UIAlertAction actionWithTitle:@"查看上次结果" style:UIAlertActionStyleDefault handler:^(UIAlertAction *a){self.result=FGRecommendation(saved);[presenter dismissViewControllerAnimated:YES completion:^{[self showResult];}];}]];
    [alert addAction:[UIAlertAction actionWithTitle:@"取消" style:UIAlertActionStyleCancel handler:nil]];
    [presenter presentViewController:alert animated:YES completion:nil];
}
- (void)makePanel {
    self.panel=[UIView new];self.panel.backgroundColor=[UIColor colorWithWhite:.06 alpha:.88];self.panel.layer.cornerRadius=18;
    self.label=[UILabel new];self.label.textColor=UIColor.whiteColor;self.label.font=[UIFont systemFontOfSize:15 weight:UIFontWeightMedium];self.label.numberOfLines=2;
    self.progress=[UIProgressView new];self.progress.progressTintColor=UIColor.systemGreenColor;
    self.cancel=[UIButton buttonWithType:UIButtonTypeSystem];[self.cancel setTitle:@"取消测试" forState:UIControlStateNormal];self.cancel.tintColor=UIColor.whiteColor;
    self.cancel.accessibilityIdentifier=@"ICSMFrameCalibrationCancel";[self.cancel addTarget:self action:@selector(cancelTest) forControlEvents:UIControlEventTouchUpInside];
    for(UIView *v in @[self.label,self.progress,self.cancel]){v.translatesAutoresizingMaskIntoConstraints=NO;[self.panel addSubview:v];}
    UIView *parent=host.window;self.panel.translatesAutoresizingMaskIntoConstraints=NO;[parent addSubview:self.panel];
    [NSLayoutConstraint activateConstraints:@[
        [self.panel.topAnchor constraintEqualToAnchor:parent.safeAreaLayoutGuide.topAnchor constant:8],
        [self.panel.centerXAnchor constraintEqualToAnchor:parent.centerXAnchor],
        [self.panel.widthAnchor constraintLessThanOrEqualToAnchor:parent.widthAnchor multiplier:.85],
        [self.panel.widthAnchor constraintEqualToConstant:600], [self.panel.heightAnchor constraintEqualToConstant:76],
        [self.label.topAnchor constraintEqualToAnchor:self.panel.topAnchor constant:10],
        [self.label.leadingAnchor constraintEqualToAnchor:self.panel.leadingAnchor constant:16],
        [self.label.trailingAnchor constraintEqualToAnchor:self.cancel.leadingAnchor constant:-12],
        [self.cancel.trailingAnchor constraintEqualToAnchor:self.panel.trailingAnchor constant:-12],
        [self.cancel.centerYAnchor constraintEqualToAnchor:self.panel.centerYAnchor], [self.cancel.widthAnchor constraintEqualToConstant:88],
        [self.cancel.heightAnchor constraintEqualToConstant:48],
        [self.progress.leadingAnchor constraintEqualToAnchor:self.label.leadingAnchor],
        [self.progress.trailingAnchor constraintEqualToAnchor:self.label.trailingAnchor],
        [self.progress.bottomAnchor constraintEqualToAnchor:self.panel.bottomAnchor constant:-12]]];
}
- (void)start:(BOOL)full {
    if(calibrationRunning || [FGScene()[@"connected"] boolValue])return;
    if(@available(iOS 26.0,*)) {
        Class descriptor=NSClassFromString(@"MTLFXFrameInterpolatorDescriptor");
        id<MTLDevice> device=metalDevice?:MTLCreateSystemDefaultDevice();
        if(!descriptor || ![(id)descriptor supportsDevice:device] || host.window.screen.maximumFramesPerSecond<120 || NSProcessInfo.processInfo.lowPowerModeEnabled) {
            [self message:@"当前设备无法测试60→120" body:@"需要支持MetalFX插帧的GPU、120Hz屏幕，并关闭低电量模式。"];return;
        }
    } else { [self message:@"系统不支持" body:@"当前系统没有MetalFX插帧接口。"];return; }
    self.original=FGScene()[@"values"];self.identity=FGIdentity(FGScene());
    if(!self.original[@"icsm_fps_limit"] || !self.original[@"icsm_metal_frame_interpolation"])return;
    NSArray *names=full?@[@"de_mirage",@"de_inferno",@"de_overpass",@"de_vertigo",@"de_nuke",@"de_train",@"de_dust2",@"de_zoo",@"de_seaside",@"de_cache",@"cs_agency",@"cs_office"]:@[@"de_mirage",@"de_inferno",@"de_dust2"];
    NSMutableArray *maps=[NSMutableArray new];
    for(NSString *name in names){NSDictionary *spawns=FGSpawn(name);if(spawns)[maps addObject:@{@"name":name,@"spawns":spawns}];}
    if(maps.count!=names.count){[self message:@"数据包不完整" body:@"测试地图或有效出生点缺失，请检查数据包后重试。"] ;return;}
    // Flush a numeric recovery snapshot before any test setting is queued.
    [NSUserDefaults.standardUserDefaults setObject:self.original forKey:FGCheckpoint];[NSUserDefaults.standardUserDefaults synchronize];
    self.maps=maps;self.rows=[NSMutableArray new];self.result=nil;self.mapIndex=0;self.caseIndex=0;self.restoring=NO;
    SourceTouchCancelAll(now());calibrationRunning=true;dirty=true;[self makePanel];
    __weak ICSMFrameCalibration *weakSelf=self;
    self.backgroundObserver=[NSNotificationCenter.defaultCenter addObserverForName:UIApplicationWillResignActiveNotification object:nil queue:NSOperationQueue.mainQueue usingBlock:^(NSNotification *n){[weakSelf finish:@"应用进入后台，测试已取消并恢复原设置。"]; }];
    [self loadMap];
}
- (void)loadMap {
    NSDictionary *map=self.maps[self.mapIndex];self.caseIndex=0;self.stage=1;self.stageTime=now();
    FGCommand([NSString stringWithFormat:@"%@; game_type 0; game_mode 1; map %@",FGRelease,map[@"name"]]);
    [self updateLabel:@"加载地图和着色器"];[self writeStatus:@"loading"];
}
- (void)updateLabel:(NSString *)stage {
    self.label.text=[NSString stringWithFormat:@"插帧性能测试 · %@（%ld/%lu）\n%@ · 样本 %ld/12",self.maps[self.mapIndex][@"name"],(long)self.mapIndex+1,(unsigned long)self.maps.count,stage,(long)self.caseIndex+1];
    self.progress.progress=(self.mapIndex*12.+self.caseIndex)/(self.maps.count*12.);
}
- (void)setupCase {
    // Two repetitions × both spawn regions × three color detail candidates.
    NSInteger role=(self.caseIndex/3)%2,repeat=self.caseIndex/6;
    // Reverse candidate order on the second repetition to reduce warmup and
    // increasing-load bias; both passes still visit identical camera paths.
    NSInteger scale=FGScales[repeat?2-self.caseIndex%3:self.caseIndex%3];
    self.pose=self.maps[self.mapIndex][@"spawns"][role?@"t":@"ct"];
    NSArray *p=self.pose[@"origin"],*a=self.pose[@"angles"];
    BOOL noclip=[FGScene()[@"move_type"] integerValue]!=8;
    NSString *action=role?@"give weapon_ak47; use weapon_ak47; +attack":@"use weapon_knife; +lookatweapon";
    FGCommand([NSString stringWithFormat:@"%@; %@ setpos_exact %.6f %.6f %.6f; setang_exact %.6f %.6f %.6f; bot_stop %d; bot_dont_shoot %d; cl_yawspeed %d; icsm_metal_frame_color_scale %ld; %@; +right",
        FGRelease,noclip?@"noclip;":@"",[p[0] doubleValue],[p[1] doubleValue],[p[2] doubleValue],
        [a[0] doubleValue],[a[1] doubleValue],[a[2] doubleValue],repeat?0:1,repeat?0:1,role?240:120,(long)scale,action]);
    self.stage=4;self.stageTime=now();[self updateLabel:@"预热当前场景"];[self writeStatus:@"warming"];
}
- (BOOL)validPose:(NSDictionary *)scene {
    int team=[scene[@"team"] intValue];
    if(![scene[@"alive"] boolValue] || (team!=2 && team!=3) || [scene[@"move_type"] intValue]!=8)return NO;
    NSArray *actual=scene[@"origin"],*expected=self.pose[@"origin"];
    if(actual.count!=3)return NO;
    for(int i=0;i<3;++i)if(std::abs([actual[i] doubleValue]-[expected[i] doubleValue])>2)return NO;
    return YES;
}
- (void)tick {
    if(!calibrationRunning) {
        if(self.restoring && fgRestoreCommitted && FGRestored(self.original,FGScene())) {
            self.restoring=NO;[NSUserDefaults.standardUserDefaults removeObjectForKey:FGCheckpoint];[NSUserDefaults.standardUserDefaults synchronize];
            [self writeStatus:self.lastPhase?:@"recovered_after_interruption"];
            if(self.result)[self showResult];
        }
        return;
    }
    double time=now();if(time-self.lastTick<.25)return;self.lastTick=time;
    if(UIApplication.sharedApplication.applicationState!=UIApplicationStateActive || NSProcessInfo.processInfo.lowPowerModeEnabled){[self finish:@"测试条件发生变化，已恢复原设置。"];return;}
    NSDictionary *scene=FGScene();double elapsed=time-self.stageTime;
    if(elapsed>120){[self finish:@"地图或测试场景等待超时，已恢复原设置。"];return;}
    if(self.stage==1) {
        if(![scene[@"in_game"] boolValue] || ![scene[@"map"] isEqual:self.maps[self.mapIndex][@"name"]])return;
        FGCommand(@"sv_cheats 1; jointeam 3; joinclass 0; bot_quota_mode fill; bot_quota 10; mp_autoteambalance 0; mp_limitteams 0; mp_ignore_round_win_conditions 1; mp_roundtime_defuse 60; mp_freezetime 0; mp_warmup_end; mp_restartgame 1");
        self.stage=2;self.stageTime=time;return;
    }
    if(self.stage==2) {
        int team=[scene[@"team"] intValue];
        if(elapsed<3 || ![scene[@"alive"] boolValue] || (team!=2 && team!=3) || [scene[@"ct"] intValue]!=5 || [scene[@"t"] intValue]!=5 || [scene[@"bots"] intValue]!=9)return;
        // Respawn AI during the pressure pass so late candidates aren't
        // measured against an empty map after the bots kill each other.
        FGCommand(@"god; sv_infinite_ammo 1; mp_respawn_on_death_ct 1; mp_respawn_on_death_t 1; icsm_fps_limit 60; icsm_metal_frame_interpolation 1; icsm_metal_frame_quality 1; icsm_metal_frame_scale 50; icsm_metal_frame_adaptive 1");
        self.stage=3;self.stageTime=time;return;
    }
    if(self.stage==3){if(elapsed>=2)[self setupCase];return;}
    if(self.stage==4) {
        if(elapsed<3)return;
        if(![self validPose:scene]){[self finish:@"测试机位或玩家状态异常，本轮结果作废。已恢复原设置。"];return;}
        self.skippedBefore=[FGStats()[@"stale_generated_display_skipped"] unsignedLongLongValue];
        FGCommand(@"icsm_metal_frame_interpolation_reset_stats");self.stage=5;self.stageTime=time;[self updateLabel:@"采样连续转向与机器人场景"];return;
    }
    if(self.stage==5) {
        if(![self validPose:scene]){[self finish:@"测试被移动或死亡打断，已恢复原设置。"];return;}
        if(elapsed<7)return;
        NSDictionary *stats=FGStats();
        if(![stats[@"enabled"] boolValue] || [stats[@"rendered_actually_presented"] intValue]<300){[self finish:@"没有收集到足够的真实显示帧，已恢复原设置。"];return;}
        NSMutableDictionary *row=[stats mutableCopy];row[@"map"]=self.maps[self.mapIndex][@"name"];row[@"case"]=@(self.caseIndex);
        NSArray *hitches=row[@"recent_hitches"];
        if(hitches.count>8)row[@"recent_hitches"]=[hitches subarrayWithRange:NSMakeRange(hitches.count-8,8)];
        row[@"repeat"]=@(self.caseIndex/6+1);row[@"active_ai"]=@(self.caseIndex>=6);row[@"role"]=(self.caseIndex/3)%2?@"t":@"ct";
        row[@"player_team"]=scene[@"team"];row[@"ct_players"]=scene[@"ct"];row[@"t_players"]=scene[@"t"];row[@"bots"]=scene[@"bots"];
        unsigned long long skipped=[stats[@"stale_generated_display_skipped"] unsignedLongLongValue];row[@"stale_skips_in_sample"]=@(skipped>=self.skippedBefore?skipped-self.skippedBefore:0);
        [self.rows addObject:row];++self.caseIndex;[self writeStatus:@"sampling"];
        FGCommand(FGRelease);
        if(self.caseIndex<12){[self setupCase];return;}
        FGCommand([FGRelease stringByAppendingString:@"; disconnect"]);self.stage=6;self.stageTime=time;return;
    }
    if(self.stage==6 && ![scene[@"connected"] boolValue] && elapsed>1) {
        if(++self.mapIndex<self.maps.count){[self loadMap];return;}
        --self.mapIndex;[self calculateResult];[self finish:nil];
    }
}
- (void)calculateResult {
    NSInteger chosen=0;double best=INFINITY;NSMutableArray *summaries=[NSMutableArray new];
    for(NSInteger scale:FGScales) {
        double worstMiss=0,minRender=INFINITY,minDisplay=INFINITY,maxGap=0,maxGPU=0;NSInteger count=0,errors=0;BOOL stable=YES;
        for(NSDictionary *row in self.rows) {
            if([row[@"color_detail_scale"] integerValue]!=scale)continue;
            ++count;NSDictionary *buckets=row[@"display_interval_120hz_buckets"];double total=0;for(NSNumber *n in buckets.allValues)total+=n.doubleValue;
            errors+=[row[@"gpu_errors"] intValue];
            double miss=total>0?1-[buckets[@"1"] doubleValue]/total:1;
            worstMiss=std::max(worstMiss,miss);minRender=std::min(minRender,[row[@"render_fps"] doubleValue]);
            minDisplay=std::min(minDisplay,[row[@"presented_fps"] doubleValue]);maxGap=std::max(maxGap,[row[@"display_interval_ms_p95"] doubleValue]);
            maxGPU=std::max(maxGPU,[row[@"render_command_buffer_gpu_ms_mean"] doubleValue]);
            if([row[@"gpu_errors"] intValue] || miss>.05 || [row[@"render_fps"] doubleValue]<58.5 || [row[@"presented_fps"] doubleValue]<115 || [row[@"display_interval_ms_p95"] doubleValue]>16.9)stable=NO;
        }
        stable=stable && count==(NSInteger)self.maps.count*4;
        [summaries addObject:@{@"color_scale":@(scale),@"stable":@(stable),@"samples":@(count),@"gpu_errors":@(errors),@"worst_missed_refresh_fraction":@(worstMiss),
            @"minimum_render_fps":@(minRender),@"minimum_display_fps":@(minDisplay),@"worst_interval_p95_ms":@(maxGap),@"worst_render_buffer_gpu_ms":@(maxGPU)}];
        // Prefer the lowest measured uneven-refresh rate. Within 0.5 percent,
        // keep more color detail only if the GPU retains a frame's headroom.
        if(stable && (worstMiss<best-.005 || (std::abs(worstMiss-best)<=.005 && maxGPU<15.5))) {chosen=scale;best=worstMiss;}
    }
    self.result=@{@"schema":@1,@"identity":self.identity,@"color_scale":@(chosen),@"motion_scale":@50,@"stable":@(chosen>0),
        @"maps":@(self.maps.count),@"summaries":summaries,@"completed":@([[NSDate date] timeIntervalSince1970])};
    self.result=FGRecommendation(self.result);
    [NSUserDefaults.standardUserDefaults setObject:self.result forKey:FGProfile];
}
- (void)cancelTest {[self finish:@"测试已取消，已恢复原设置。"];}
- (void)finish:(NSString *)reason {
    if(!calibrationRunning)return;
    fgRestoreCommitted=false;FGCommand(FGRestore(self.original));calibrationRunning=false;dirty=true;self.restoring=YES;
    [self.panel removeFromSuperview];self.panel=nil;
    if(self.backgroundObserver){[NSNotificationCenter.defaultCenter removeObserver:self.backgroundObserver];self.backgroundObserver=nil;}
    [self writeStatus:reason?:@"completed"];
    if(reason && UIApplication.sharedApplication.applicationState==UIApplicationStateActive)[self message:@"插帧性能测试" body:reason];
}
- (BOOL)compatible {return [self.result[@"identity"] isEqual:FGIdentity(FGScene())];}
- (void)showResult {
    self.result=FGRecommendation(self.result);
    UIViewController *presenter=self.presenter;if(!presenter || presenter.presentedViewController)return;
    NSMutableString *body=[NSMutableString string];
    for(NSDictionary *row in self.result[@"summaries"])
        [body appendFormat:@"细节%@%%：最低 %.1f→%.1f FPS，最差P95 %.2f ms\n",row[@"color_scale"],[row[@"minimum_render_fps"] doubleValue],[row[@"minimum_display_fps"] doubleValue],[row[@"worst_interval_p95_ms"] doubleValue]];
    BOOL valid=[self compatible],stable=[self.result[@"stable"] boolValue],available=[self.result[@"recommendation_available"] boolValue];
    NSString *recommendation=stable?[NSString stringWithFormat:@"推荐插帧细节%@%%，60→120帧。",self.result[@"color_scale"]]:available?[NSString stringWithFormat:@"当前画质未通过稳定120帧标准。最稳可用参数：细节%@%%；建议降低画质后重测。",self.result[@"color_scale"]]:@"当前基础帧率或插帧收益不足，建议降低一个画质档位后重测。";
    [body appendFormat:@"\n%@\n真实场景分辨率、材质与武器画质保持原样。测试仅覆盖固定机位，复杂场景仍会动态保护真实帧率。",!valid?@"系统、画质或渲染版本已变化，请重新测试。":recommendation];
    UIAlertController *alert=[UIAlertController alertControllerWithTitle:@"插帧测试结果" message:body preferredStyle:UIAlertControllerStyleAlert];
    if(valid && available)[alert addAction:[UIAlertAction actionWithTitle:stable?@"应用推荐":@"应用保守推荐" style:UIAlertActionStyleDefault handler:^(UIAlertAction *a){[self apply];}]];
    [alert addAction:[UIAlertAction actionWithTitle:@"重新测试" style:UIAlertActionStyleDefault handler:^(UIAlertAction *a){self.result=nil;[presenter dismissViewControllerAnimated:YES completion:^{[self open];}];}]];
    [alert addAction:[UIAlertAction actionWithTitle:@"保留原设置" style:UIAlertActionStyleCancel handler:nil]];
    [presenter presentViewController:alert animated:YES completion:nil];
}
- (void)apply {
    self.result=FGRecommendation(self.result?:[NSUserDefaults.standardUserDefaults dictionaryForKey:FGProfile]);
    if(calibrationRunning || self.restoring || ![self compatible] || ![self.result[@"recommendation_available"] boolValue])return;
    NSInteger scale=[self.result[@"color_scale"] integerValue];if(scale!=50 && scale!=67 && scale!=75)return;
    FGCommand([NSString stringWithFormat:@"icsm_metal_frame_scale 50; icsm_metal_frame_color_scale %ld; icsm_metal_frame_quality 1; icsm_metal_frame_adaptive 1; icsm_fps_limit 60; icsm_metal_frame_interpolation 1; host_writeconfig",(long)scale]);
    [self writeStatus:@"applied"];
}
@end
extern "C" void SourceTouchOpenFrameCalibration(void (*queue)(const char *),size_t (*scene)(char *,size_t),const char *action) {
    NSCAssert(NSThread.isMainThread,@"Calibration belongs to the Source main thread");fgQueue=queue;fgScene=scene;
    if(!frameCalibration)frameCalibration=[ICSMFrameCalibration new];
    NSString *op=action?@(action):@"";
    if([op isEqual:@"quick"])[frameCalibration start:NO];
    else if([op isEqual:@"full"])[frameCalibration start:YES];
    else if([op isEqual:@"cancel"])[frameCalibration cancelTest];
    else if([op isEqual:@"apply"])[frameCalibration apply];
    else if([op isEqual:@"status"])[frameCalibration writeStatus:calibrationRunning?@"running":@"idle"];
    else [frameCalibration open];
}
extern "C" void SourceTouchFrameCalibrationTick(){[frameCalibration tick];}
extern "C" void SourceTouchFrameCalibrationRecover(void (*queue)(const char *),size_t (*scene)(char *,size_t)) {
    fgQueue=queue;fgScene=scene;NSDictionary *values=[NSUserDefaults.standardUserDefaults dictionaryForKey:FGCheckpoint];
    if(values){if(!frameCalibration)frameCalibration=[ICSMFrameCalibration new];frameCalibration.original=values;frameCalibration.restoring=YES;fgRestoreCommitted=false;FGCommand(FGRestore(values));}
}
extern "C" void SourceTouchFrameCalibrationRestoreCommitted(){fgRestoreCommitted=true;}
extern "C" void SourceTouchFrameCalibrationShutdown() {
    [frameCalibration finish:nil];frameCalibration=nil;fgQueue=nullptr;fgScene=nullptr;
}
