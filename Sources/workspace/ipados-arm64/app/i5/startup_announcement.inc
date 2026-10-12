// A startup-only UIKit surface. No blur or timer survives entry to the lobby.
#include "announcement_clock.h"
// Persist completion separately from diagnostic logs. Upgrade existing installs
// that already completed the old announcement, without another forced wait.
static NSString *const ICSMAnnouncementCompletedKey = @"ICSMFirstDataStartupAnnouncementCompleted";
static BOOL ICSMHasCompletedStartupAnnouncement(NSUserDefaults *defaults,NSString *directory) {
 if([defaults boolForKey:ICSMAnnouncementCompletedKey])return YES;
 NSData *data=[NSData dataWithContentsOfFile:[directory stringByAppendingPathComponent:@"startup-announcement.json"]];
 id record=data?[NSJSONSerialization JSONObjectWithData:data options:0 error:nil]:nil;
 if(![record isKindOfClass:NSDictionary.class])return NO;
 id complete=record[@"complete"],elapsed=record[@"foreground_seconds"],required=record[@"required_seconds"];
 if(![complete isKindOfClass:NSNumber.class] || ![elapsed isKindOfClass:NSNumber.class] || ![required isKindOfClass:NSNumber.class])return NO;
 const double seconds=[elapsed doubleValue],minimum=[required doubleValue];
 if(![complete boolValue] || !std::isfinite(seconds) || !std::isfinite(minimum) ||
    minimum<ICSMAnnouncementClock::requiredSeconds || seconds<minimum)return NO;
 [defaults setBool:YES forKey:ICSMAnnouncementCompletedKey];
 return YES;
}
static void ICSMMarkStartupAnnouncementCompleted(NSUserDefaults *defaults) {
 [defaults setBool:YES forKey:ICSMAnnouncementCompletedKey];
}
static NSString *const ICSMAnnouncementText =
 @"该游戏由且仅由bilibili@SU_ZeShin个人移植，且仅将数据包的iCloud链接上传至GitHub，和Q群1065438652\n\n"
 @"如果你从其他任何国内外网盘下载到该资源，请在心里默念：盗游戏的全族死绝🙏\n\n"
 @"游戏推荐游玩设备为a19+和M5+\n"
 @"其他设备酌情考量画质开到什么程度\n"
 @"切记该游戏仅为个人娱乐所用，移植耗费大量财力，有什么合理优化建议请委婉提出";
@interface ICSMStartupAnnouncementView:UIView {
 ICSMAnnouncementClock clock;
}
@property UIVisualEffectView *panel;
@property UILabel *countdown;
@property UILabel *body;
@property UIScrollView *scroll;
-(void)begin;
-(void)setForeground:(BOOL)active;
-(BOOL)tick;
-(NSDictionary*)snapshot;
@end
@implementation ICSMStartupAnnouncementView
-(instancetype)init {
 if((self=[super init])) {
  const BOOL pad=UIDevice.currentDevice.userInterfaceIdiom==UIUserInterfaceIdiomPad;
  self.accessibilityIdentifier=@"ICSMStartupAnnouncement";
  self.backgroundColor=[UIColor colorWithWhite:0 alpha:0.62];
  self.panel=[[UIVisualEffectView alloc] initWithEffect:[UIBlurEffect effectWithStyle:UIBlurEffectStyleSystemMaterialDark]];
  self.panel.overrideUserInterfaceStyle=UIUserInterfaceStyleDark;
  self.panel.contentView.backgroundColor=[UIColor colorWithWhite:0 alpha:0.30];
  self.panel.layer.cornerRadius=24;self.panel.layer.cornerCurve=kCACornerCurveContinuous;
  self.panel.clipsToBounds=YES;self.panel.layer.borderWidth=1;
  self.panel.layer.borderColor=[UIColor colorWithWhite:1 alpha:0.18].CGColor;
  self.panel.translatesAutoresizingMaskIntoConstraints=NO;[self addSubview:self.panel];
  UILabel *title=[UILabel new];title.text=@"公告";title.font=[UIFont systemFontOfSize:pad?28:22 weight:UIFontWeightBold];
  title.textColor=UIColor.whiteColor;title.textAlignment=NSTextAlignmentCenter;
  title.accessibilityTraits=UIAccessibilityTraitHeader;
  self.scroll=[UIScrollView new];self.scroll.alwaysBounceVertical=NO;
  self.scroll.indicatorStyle=UIScrollViewIndicatorStyleWhite;
  self.scroll.contentInsetAdjustmentBehavior=UIScrollViewContentInsetAdjustmentNever;
  self.body=[UILabel new];self.body.numberOfLines=0;self.body.textColor=[UIColor colorWithWhite:1 alpha:0.95];
  self.body.font=[UIFont systemFontOfSize:pad?20:16 weight:UIFontWeightRegular];
  NSMutableParagraphStyle *paragraph=[NSMutableParagraphStyle new];paragraph.lineSpacing=pad?5:3;
  self.body.attributedText=[[NSAttributedString alloc] initWithString:ICSMAnnouncementText attributes:@{NSParagraphStyleAttributeName:paragraph}];
  self.body.accessibilityIdentifier=@"ICSMAnnouncementText";
  self.countdown=[UILabel new];self.countdown.textColor=[UIColor colorWithWhite:1 alpha:0.8];
  self.countdown.font=[UIFont monospacedDigitSystemFontOfSize:pad?17:14 weight:UIFontWeightMedium];
  self.countdown.textAlignment=NSTextAlignmentCenter;
  self.countdown.accessibilityIdentifier=@"ICSMAnnouncementCountdown";
  self.countdown.text=[NSString stringWithFormat:@"请阅读公告 · %.0f 秒后进入大厅",ICSMAnnouncementClock::requiredSeconds];
  for(UIView *v in @[title,self.scroll,self.countdown]) {
   v.translatesAutoresizingMaskIntoConstraints=NO;[self.panel.contentView addSubview:v];
  }
  self.body.translatesAutoresizingMaskIntoConstraints=NO;[self.scroll addSubview:self.body];
  NSLayoutConstraint *width=[self.panel.widthAnchor constraintEqualToAnchor:self.safeAreaLayoutGuide.widthAnchor multiplier:0.9];
  width.priority=UILayoutPriorityDefaultHigh;
  [NSLayoutConstraint activateConstraints:@[
   width,[self.panel.widthAnchor constraintLessThanOrEqualToConstant:1000],
   [self.panel.centerXAnchor constraintEqualToAnchor:self.safeAreaLayoutGuide.centerXAnchor],
   [self.panel.topAnchor constraintEqualToAnchor:self.safeAreaLayoutGuide.topAnchor constant:18],
   [self.panel.bottomAnchor constraintEqualToAnchor:self.safeAreaLayoutGuide.bottomAnchor constant:-18],
   [title.topAnchor constraintEqualToAnchor:self.panel.contentView.topAnchor constant:16],
   [title.leadingAnchor constraintEqualToAnchor:self.panel.contentView.leadingAnchor constant:24],
   [title.trailingAnchor constraintEqualToAnchor:self.panel.contentView.trailingAnchor constant:-24],
   [self.scroll.topAnchor constraintEqualToAnchor:title.bottomAnchor constant:14],
   [self.scroll.leadingAnchor constraintEqualToAnchor:title.leadingAnchor],
   [self.scroll.trailingAnchor constraintEqualToAnchor:title.trailingAnchor],
   [self.scroll.bottomAnchor constraintEqualToAnchor:self.countdown.topAnchor constant:-14],
   [self.countdown.leadingAnchor constraintEqualToAnchor:title.leadingAnchor],
   [self.countdown.trailingAnchor constraintEqualToAnchor:title.trailingAnchor],
   [self.countdown.bottomAnchor constraintEqualToAnchor:self.panel.contentView.bottomAnchor constant:-16],
   [self.body.leadingAnchor constraintEqualToAnchor:self.scroll.contentLayoutGuide.leadingAnchor],
   [self.body.trailingAnchor constraintEqualToAnchor:self.scroll.contentLayoutGuide.trailingAnchor],
   [self.body.topAnchor constraintEqualToAnchor:self.scroll.contentLayoutGuide.topAnchor],
   [self.body.bottomAnchor constraintEqualToAnchor:self.scroll.contentLayoutGuide.bottomAnchor],
   [self.body.widthAnchor constraintEqualToAnchor:self.scroll.frameLayoutGuide.widthAnchor]
  ]];
 }return self;
}
-(void)begin {
 [self layoutIfNeeded];
 // Start after the first UIKit presentation turn, not while loading Source.
 [self performSelector:@selector(beginCountdown) withObject:nil afterDelay:0.1];
}
-(void)beginCountdown {
 clock.begin(NSProcessInfo.processInfo.systemUptime,
     UIApplication.sharedApplication.applicationState==UIApplicationStateActive && !self.hidden && !self.window.hidden);
}
-(void)setForeground:(BOOL)active {
 clock.setActive(NSProcessInfo.processInfo.systemUptime,active && !self.hidden && !self.window.hidden);
}
-(BOOL)tick {
 [self setForeground:UIApplication.sharedApplication.applicationState==UIApplicationStateActive];
 const int remaining=std::max(0,int(std::ceil(ICSMAnnouncementClock::requiredSeconds-clock.elapsed)));
 self.countdown.text=[NSString stringWithFormat:@"请阅读公告 · %d 秒后进入大厅",remaining];
 return clock.complete();
}
-(NSDictionary*)snapshot {
 return @{@"visible":@(!self.hidden && self.superview!=nil),@"required_seconds":@(ICSMAnnouncementClock::requiredSeconds),
  @"foreground_seconds":@(clock.elapsed),@"countdown_started":@(clock.started),@"complete":@(clock.complete()),
  @"skippable":@NO,@"text":ICSMAnnouncementText,@"panel_rect":NSStringFromCGRect(self.panel.frame),
  @"text_rect":NSStringFromCGRect(self.body.frame),@"scroll_rect":NSStringFromCGRect(self.scroll.frame)};
}
@end
