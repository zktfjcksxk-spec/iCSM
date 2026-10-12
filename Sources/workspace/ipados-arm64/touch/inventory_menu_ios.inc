// A lightweight anchored menu instead of UIKit's material-based context menu.
// Team eligibility and action IDs come from the original inventory snapshot.
@interface ICSMInventoryMenuAction : UIButton
@property(nonatomic,strong) UIImageView *glyph;
@property(nonatomic,strong) UILabel *caption;
@property(nonatomic,strong) UIImageView *check;
@property(nonatomic,copy) NSString *actionID;
-(instancetype)initWithText:(NSString*)text symbol:(NSString*)symbol action:(NSString*)action equipped:(BOOL)equipped;
@end
@implementation ICSMInventoryMenuAction
-(instancetype)initWithText:(NSString*)text symbol:(NSString*)symbol action:(NSString*)action equipped:(BOOL)equipped {
    if((self=[super initWithFrame:CGRectZero])) {
        self.actionID=action;self.layer.cornerRadius=10;self.layer.cornerCurve=kCACornerCurveContinuous;
        self.glyph=[[UIImageView alloc] initWithImage:[UIImage systemImageNamed:symbol]];
        self.glyph.tintColor=UIColor.whiteColor;self.glyph.contentMode=UIViewContentModeScaleAspectFit;
        self.caption=inventoryLabel(17,UIFontWeightSemibold);self.caption.text=text;
        self.check=[[UIImageView alloc] initWithImage:[UIImage systemImageNamed:@"checkmark"]];
        self.check.tintColor=UIColor.whiteColor;self.check.contentMode=UIViewContentModeScaleAspectFit;self.check.hidden=!equipped;
        for(UIView *view in @[self.glyph,self.caption,self.check]) {view.userInteractionEnabled=NO;[self addSubview:view];}
        self.accessibilityLabel=text;self.accessibilityTraits=UIAccessibilityTraitButton|(equipped?UIAccessibilityTraitSelected:0);
    }return self;
}
-(void)layoutSubviews {
    [super layoutSubviews];CGFloat w=self.bounds.size.width,h=self.bounds.size.height;
    self.glyph.frame=CGRectMake(12,(h-22)*.5,22,22);
    self.caption.frame=CGRectMake(46,0,std::max<CGFloat>(0,w-82),h);
    self.check.frame=CGRectMake(w-30,(h-18)*.5,18,18);
}
-(void)setHighlighted:(BOOL)highlighted {
    [super setHighlighted:highlighted];self.backgroundColor=highlighted?[UIColor colorWithWhite:1 alpha:.12]:UIColor.clearColor;
}
@end

@interface ICSMInventoryPopup : UIView
@property(nonatomic,strong) UIButton *dismissArea;
@property(nonatomic,strong) ICSMTranslucentSurface *panel;
@property(nonatomic,strong) UILabel *titleLabel;
@property(nonatomic,strong) NSArray<UIView*> *entries;
@property(nonatomic) CGRect anchor;
@property(nonatomic,copy) void (^closeHandler)(NSString *actionID);
-(instancetype)initWithRow:(NSDictionary*)row anchor:(CGRect)anchor;
@end
@implementation ICSMInventoryPopup
-(instancetype)initWithRow:(NSDictionary*)row anchor:(CGRect)anchor {
    if((self=[super initWithFrame:CGRectZero])) {
        self.anchor=anchor;self.accessibilityIdentifier=@"ICSMTranslucentInventoryMenu";self.accessibilityViewIsModal=YES;
        self.dismissArea=[UIButton new];self.dismissArea.backgroundColor=[UIColor colorWithWhite:0 alpha:.1];
        self.dismissArea.accessibilityLabel=@"关闭武器菜单";
        [self.dismissArea addTarget:self action:@selector(dismiss) forControlEvents:UIControlEventTouchUpInside];[self addSubview:self.dismissArea];
        self.panel=translucentSurface(12,YES);
        self.panel.backgroundColor=[UIColor colorWithWhite:0 alpha:.90];
        self.panel.layer.borderColor=[UIColor colorWithWhite:1 alpha:.12].CGColor;
        [self addSubview:self.panel];
        self.titleLabel=inventoryLabel(14,UIFontWeightSemibold);self.titleLabel.text=row[@"name"];
        self.titleLabel.textColor=[UIColor colorWithWhite:1 alpha:.75];[self.panel addSubview:self.titleLabel];
        BOOL ct=[row[@"ct"] boolValue],t=[row[@"t"] boolValue];
        NSInteger side=[row[@"side"] integerValue];
        if([row[@"sticker_action"] length]) {
            ICSMInventoryMenuAction *entry=[[ICSMInventoryMenuAction alloc] initWithText:@"贴纸" symbol:@"tag" action:row[@"sticker_action"] equipped:NO];
            ICSMInventoryMenuAction *cancel=[[ICSMInventoryMenuAction alloc] initWithText:@"取消" symbol:@"xmark" action:@"" equipped:NO];
            self.entries=@[entry,cancel];
            for(ICSMInventoryMenuAction *button in self.entries) {
                [button addTarget:self action:@selector(activate:) forControlEvents:UIControlEventTouchUpInside];[self.panel addSubview:button];
            }
        } else if(side==3 || side==2) {
            BOOL equipped=[row[side==3?@"equipped_ct":@"equipped_t"] boolValue];
            NSString *caption=side==3?@"为警方装备":@"为匪徒装备";
            ICSMInventoryMenuAction *equip=[[ICSMInventoryMenuAction alloc] initWithText:caption symbol:side==3?@"shield.fill":@"flame.fill"
                action:row[side==3?@"ct_action":@"t_action"] equipped:equipped];
            ICSMInventoryMenuAction *cancel=[[ICSMInventoryMenuAction alloc] initWithText:@"取消" symbol:@"xmark" action:@"" equipped:NO];
            self.entries=@[equip,cancel];
            for(ICSMInventoryMenuAction *entry in self.entries) {
                [entry addTarget:self action:@selector(activate:) forControlEvents:UIControlEventTouchUpInside];[self.panel addSubview:entry];
            }
        } else if(ct && t) {
            ICSMInventoryMenuAction *ctEntry=[[ICSMInventoryMenuAction alloc] initWithText:@"为警方装备" symbol:@"shield.fill" action:row[@"ct_action"] equipped:[row[@"equipped_ct"] boolValue]];
            ICSMInventoryMenuAction *tEntry=[[ICSMInventoryMenuAction alloc] initWithText:@"为匪徒装备" symbol:@"flame.fill" action:row[@"t_action"] equipped:[row[@"equipped_t"] boolValue]];
            self.entries=@[ctEntry,tEntry];
            for(ICSMInventoryMenuAction *entry in self.entries) {
                [entry addTarget:self action:@selector(activate:) forControlEvents:UIControlEventTouchUpInside];[self.panel addSubview:entry];
            }
        } else {
            // An exclusive weapon only states its team; it cannot be equipped
            // to the other team from this menu.
            UILabel *affiliation=inventoryLabel(17,UIFontWeightSemibold);
            affiliation.text=ct?@"警方专属":@"匪徒专属";affiliation.textAlignment=NSTextAlignmentCenter;
            self.entries=@[affiliation];[self.panel addSubview:affiliation];
        }
    }return self;
}
-(void)layoutSubviews {
    [super layoutSubviews];self.dismissArea.frame=self.bounds;
    UIEdgeInsets insets=self.safeAreaInsets;
    CGFloat width=std::min<CGFloat>(280,self.bounds.size.width-insets.left-insets.right-32);
    CGFloat height=44+self.entries.count*48+8;
    CGFloat minX=insets.left+16,maxX=self.bounds.size.width-insets.right-16-width;
    CGFloat x=CGRectGetMaxX(self.anchor)+12;
    if(x>maxX)x=CGRectGetMinX(self.anchor)-12-width;
    x=std::clamp(x,minX,std::max(minX,maxX));
    CGFloat minY=insets.top+16,maxY=self.bounds.size.height-insets.bottom-16-height;
    CGFloat y=std::clamp(CGRectGetMidY(self.anchor)-height*.5,minY,std::max(minY,maxY));
    self.panel.frame=CGRectMake(x,y,width,height);self.titleLabel.frame=CGRectMake(16,8,width-32,28);
    for(NSUInteger i=0;i<self.entries.count;++i)self.entries[i].frame=CGRectMake(8,40+i*48,width-16,48);
}
-(void)dismiss {if(self.closeHandler)self.closeHandler(nil);}
-(void)activate:(ICSMInventoryMenuAction*)sender {if(self.closeHandler)self.closeHandler(sender.actionID);}
@end
