/* Native inventory cards over the shared play/settings frosted backdrop. Panorama and
   NativeInventoryAPI own classification, eligibility, saving and skin state. */
static void (*inventoryActivate)(const char*)=nullptr;
static NSDictionary *inventoryDiagnostics;
static std::string inventoryJSON;
static bool inventoryVisible=false;
static uint64_t inventoryActivations=0,inventoryMenus=0,inventorySnapshots=0;
static NSString *inventoryLastAction=@"";
static NSDictionary *inventoryLastMenu;
static NSCache<NSString*,UIImage*> *inventoryImages;
static double inventoryHiddenSince=0;
static uint64_t inventoryResourceTrims=0;

static void inventorySend(NSString *panelID) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    if (!inventoryActivate || !inventoryVisible || editing || consoleVisible || panelID.length==0) return;
    ++inventoryActivations;inventoryLastAction=panelID;
    inventoryActivate(panelID.UTF8String);
}
static UIImage *inventoryImage(NSString *url) {
    NSString *file=url.lastPathComponent;
    static NSRegularExpression *pattern;
    static NSArray<NSString*> *directories;
    BOOL sticker=[file hasPrefix:@"sticker_"];
    BOOL preview=[file hasPrefix:@"preview_"];
    if (!pattern) pattern=[NSRegularExpression regularExpressionWithPattern:@"^([0-9]+_[0-9]+|sticker_[0-9]+|preview_[0-9]+(_[0-9]+){7})\\.png$" options:0 error:nil];
    if (!file || [pattern numberOfMatchesInString:file options:0 range:NSMakeRange(0,file.length)]!=1) return nil;
    if (!inventoryImages) {inventoryImages=[NSCache new];inventoryImages.totalCostLimit=32*1024*1024;inventoryImages.countLimit=96;}
    UIImage *cached=[inventoryImages objectForKey:file];if(cached)return cached;
    if(preview) {
        NSString *docs=NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,NSUserDomainMask,YES).firstObject;
        NSString *path=[[docs stringByAppendingPathComponent:@"runtime-i5/csgo/cfg/icsm-sticker-previews"] stringByAppendingPathComponent:file];
        UIImage *image=[UIImage imageWithContentsOfFile:path];
        if(image)[inventoryImages setObject:image forKey:file cost:(NSUInteger)(image.size.width*image.size.height*image.scale*image.scale*4)];
        return image;
    }
    if (!directories) {
        NSString *bundle=NSBundle.mainBundle.resourcePath;
        NSString *docs=NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,NSUserDomainMask,YES).firstObject;
        directories=@[[bundle stringByAppendingPathComponent:@"printstream-trial/materials/panorama/images/offline_skins"],
            [docs stringByAppendingPathComponent:@"game-assets/icsm-addon/materials/panorama/images/offline_skins"],
            [bundle stringByAppendingPathComponent:@"native-ui/panorama/images/offline_skins"],
            [docs stringByAppendingPathComponent:@"game-assets/csgo/materials/panorama/images/offline_skins"]];
    }
    for (NSString *directory in directories) {
        NSString *actual=sticker?[[directory stringByDeletingLastPathComponent] stringByAppendingPathComponent:@"offline_stickers"]:directory;
        UIImage *image=[UIImage imageWithContentsOfFile:[actual stringByAppendingPathComponent:file]];
        if (!image) continue;
        [inventoryImages setObject:image forKey:file cost:(NSUInteger)(image.size.width*image.size.height*image.scale*image.scale*4)];
        return image;
    }
    fprintf(stderr,"ICSM_GLASS_INVENTORY preview_missing=%s\n",file.UTF8String);
    return nil;
}
static UILabel *inventoryLabel(CGFloat size,UIFontWeight weight) {
    UILabel *label=[UILabel new];label.textColor=UIColor.whiteColor;
    label.font=[UIFont systemFontOfSize:size weight:weight];
    return label;
}
static ICSMTranslucentSurface *inventorySurface(CGFloat radius) {
    // Match settings' quality tiles. The page already shares Source's single
    // neutral backdrop blur; individual cards do not need another effect.
    ICSMTranslucentSurface *view=translucentSurface(radius,NO);
    view.backgroundColor=[UIColor colorWithWhite:1 alpha:.07];
    view.layer.borderColor=[UIColor colorWithWhite:1 alpha:.12].CGColor;
    return view;
}
#include "inventory_menu_ios.inc"

@interface ICSMInventoryButton : UIButton
@property(nonatomic,strong) ICSMTranslucentSurface *glass;
@property(nonatomic,strong) UILabel *caption;
@property(nonatomic,copy) NSString *actionID;
@property(nonatomic) BOOL chosen;
@property(nonatomic) BOOL prominent;
-(instancetype)initWithText:(NSString*)text action:(NSString*)action;
-(void)updateAppearance;
@end
@implementation ICSMInventoryButton
-(instancetype)initWithText:(NSString*)text action:(NSString*)action {
    if((self=[super initWithFrame:CGRectZero])) {
        self.glass=inventorySurface(12);self.glass.userInteractionEnabled=NO;[self addSubview:self.glass];
        self.caption=inventoryLabel(16,UIFontWeightSemibold);self.caption.textAlignment=NSTextAlignmentCenter;
        self.caption.text=text;self.caption.adjustsFontSizeToFitWidth=YES;self.caption.minimumScaleFactor=.75;
        [self.glass addSubview:self.caption];self.actionID=action;
        [self addTarget:self action:@selector(activate) forControlEvents:UIControlEventTouchUpInside];
        self.pointerInteractionEnabled=YES;self.accessibilityLabel=text;
    }return self;
}
-(void)activate {if(self.enabled)inventorySend(self.actionID);}
-(void)layoutSubviews {
    [super layoutSubviews];self.glass.frame=self.bounds;self.caption.frame=CGRectInset(self.bounds,8,4);
}
-(void)setChosen:(BOOL)chosen {
    _chosen=chosen;
    [self updateAppearance];
    self.caption.font=[UIFont systemFontOfSize:16 weight:chosen?UIFontWeightBold:UIFontWeightSemibold];
    self.accessibilityTraits=UIAccessibilityTraitButton|(chosen?UIAccessibilityTraitSelected:0);
}
-(void)setProminent:(BOOL)prominent {_prominent=prominent;[self updateAppearance];}
-(void)setHighlighted:(BOOL)highlighted {[super setHighlighted:highlighted];[self updateAppearance];}
-(void)updateAppearance {
    BOOL selected=self.chosen || self.highlighted;
    self.glass.backgroundColor=[UIColor colorWithWhite:1 alpha:selected?.18:self.prominent?.14:.07];
    self.glass.layer.borderColor=[UIColor colorWithWhite:1 alpha:selected?.5:self.prominent?.30:.12].CGColor;
}
-(void)setEnabled:(BOOL)enabled {[super setEnabled:enabled];self.caption.alpha=enabled?1:.35;}
@end

@interface ICSMInventoryCell : UICollectionViewCell
@property(nonatomic,strong) ICSMTranslucentSurface *glass;
@property(nonatomic,strong) UIImageView *picture;
@property(nonatomic,strong) UILabel *nameLabel;
@property(nonatomic,strong) UILabel *finishLabel;
@property(nonatomic,strong) UILabel *teamLabel;
@property(nonatomic,strong) UIView *selectionBorder;
@property(nonatomic,copy) NSString *actionID;
@property(nonatomic) NSInteger knifeSide;
-(void)applyRow:(NSDictionary*)row;
@end
@implementation ICSMInventoryCell
-(instancetype)initWithFrame:(CGRect)frame {
    if((self=[super initWithFrame:frame])) {
        self.backgroundColor=UIColor.clearColor;self.contentView.backgroundColor=UIColor.clearColor;
        self.glass=inventorySurface(12);self.glass.userInteractionEnabled=NO;[self.contentView addSubview:self.glass];
        self.picture=[UIImageView new];self.picture.contentMode=UIViewContentModeScaleAspectFit;
        [self.glass addSubview:self.picture];
        self.nameLabel=inventoryLabel(17,UIFontWeightSemibold);self.nameLabel.textAlignment=NSTextAlignmentCenter;
        self.nameLabel.adjustsFontSizeToFitWidth=YES;self.nameLabel.minimumScaleFactor=.75;
        self.finishLabel=inventoryLabel(13,UIFontWeightRegular);self.finishLabel.textAlignment=NSTextAlignmentCenter;
        self.finishLabel.textColor=[UIColor colorWithWhite:1 alpha:.75];
        self.teamLabel=inventoryLabel(11,UIFontWeightBold);self.teamLabel.textAlignment=NSTextAlignmentRight;
        for(UILabel *label in @[self.nameLabel,self.finishLabel,self.teamLabel])[self.glass addSubview:label];
        self.selectionBorder=[UIView new];self.selectionBorder.userInteractionEnabled=NO;
        self.selectionBorder.layer.cornerRadius=12;self.selectionBorder.layer.cornerCurve=kCACornerCurveContinuous;
        self.selectionBorder.layer.borderColor=[UIColor colorWithWhite:1 alpha:.5].CGColor;
        self.selectionBorder.layer.borderWidth=1;[self.contentView addSubview:self.selectionBorder];
        self.isAccessibilityElement=YES;self.accessibilityTraits=UIAccessibilityTraitButton;
    }return self;
}
-(void)applyRow:(NSDictionary*)row {
    self.contentView.hidden=[row[@"placeholder"] boolValue];self.knifeSide=[row[@"side"] integerValue];
    self.actionID=row[@"id"];self.nameLabel.text=row[@"name"];self.finishLabel.text=row[@"subtitle"];
    self.picture.image=inventoryImage(row[@"image"]);
    if(row[@"sticker"] && [row[@"sticker"] intValue]==0) {self.picture.image=[UIImage systemImageNamed:@"xmark.circle"];self.picture.tintColor=UIColor.whiteColor;}
    BOOL ct=[row[@"equipped_ct"] boolValue],t=[row[@"equipped_t"] boolValue];
    BOOL equipped=self.knifeSide?(self.knifeSide==3?ct:t):(ct||t);
    self.teamLabel.text=self.knifeSide?((self.knifeSide==3?ct:t)?@"已装备":@""):(ct&&t?@"CT · T":ct?@"CT":t?@"T":@"");
    self.selectionBorder.hidden=![row[@"selected"] boolValue];
    self.glass.backgroundColor=[UIColor colorWithWhite:1 alpha:[row[@"selected"] boolValue]?.18:.07];
    self.accessibilityLabel=[NSString stringWithFormat:@"%@，%@%@",row[@"name"]?:@"",row[@"subtitle"]?:@"",
        equipped?[NSString stringWithFormat:@"，%@ 已装备",self.teamLabel.text]:@""];
    self.accessibilityIdentifier=self.actionID;
    self.accessibilityTraits=UIAccessibilityTraitButton|([row[@"selected"] boolValue]?UIAccessibilityTraitSelected:0);
}
-(void)layoutSubviews {
    [super layoutSubviews];self.glass.frame=self.contentView.bounds;self.selectionBorder.frame=self.contentView.bounds;
    CGFloat w=self.bounds.size.width,h=self.bounds.size.height;
    self.teamLabel.frame=CGRectMake(w-90,8,76,16);
    self.nameLabel.font=[UIFont systemFontOfSize:17 weight:UIFontWeightSemibold];
    self.finishLabel.font=[UIFont systemFontOfSize:13 weight:UIFontWeightRegular];
    if(self.knifeSide) {
        CGFloat pictureWidth=std::min<CGFloat>(260,w*.5),textX=pictureWidth+16,textWidth=std::max<CGFloat>(0,w-textX-16);
        self.picture.frame=CGRectMake(12,8,pictureWidth-12,h-16);
        self.nameLabel.frame=CGRectMake(textX,h*.5-25,textWidth,26);
        self.finishLabel.frame=CGRectMake(textX,h*.5+3,textWidth,22);
        return;
    }
    if(h<130) {
        // A short landscape viewport still needs a complete selectable card.
        // Reserve the two text lines first, then give the image the remainder.
        self.picture.frame=CGRectMake(12,4,w-24,std::max<CGFloat>(0,h-46));
        self.nameLabel.frame=CGRectMake(12,h-42,w-24,22);
        self.finishLabel.frame=CGRectMake(12,h-20,w-24,18);
        self.nameLabel.font=[UIFont systemFontOfSize:15 weight:UIFontWeightSemibold];
        self.finishLabel.font=[UIFont systemFontOfSize:12 weight:UIFontWeightRegular];
        return;
    }
    self.picture.frame=CGRectMake(12,9,w-24,std::max<CGFloat>(36,h-65));
    self.nameLabel.frame=CGRectMake(12,h-53,w-24,24);
    self.finishLabel.frame=CGRectMake(12,h-29,w-24,20);
}
-(void)prepareForReuse {
    [super prepareForReuse];self.picture.image=nil;self.actionID=nil;
}
@end

@interface ICSMGlassInventory : UIView <UICollectionViewDataSource,UICollectionViewDelegateFlowLayout,UIGestureRecognizerDelegate>
@property(nonatomic,strong) UIView *container;
@property(nonatomic,strong) UIView *headerContainer;
@property(nonatomic,strong) UIView *gridContainer;
@property(nonatomic,strong) UILabel *titleLabel;
@property(nonatomic,strong) UIScrollView *leftFilters;
@property(nonatomic,strong) UIScrollView *rightFilters;
@property(nonatomic,strong) NSArray<ICSMInventoryButton*> *filters;
@property(nonatomic,strong) UICollectionView *grid;
@property(nonatomic,strong) UILabel *hintLabel;
@property(nonatomic,strong) UILabel *statusLabel;
@property(nonatomic,strong) UIView *knifeDivider;
@property(nonatomic,strong) UILabel *knifeCTLabel;
@property(nonatomic,strong) UILabel *knifeTLabel;
@property(nonatomic,strong) ICSMInventoryButton *backButton;
@property(nonatomic,strong) UIView *skinHeader;
@property(nonatomic,strong) ICSMTranslucentSurface *skinPreviewGlass;
@property(nonatomic,strong) UIImageView *skinPreview;
@property(nonatomic,strong) UILabel *skinName;
@property(nonatomic,strong) UILabel *skinCount;
@property(nonatomic,strong) ICSMInventoryButton *ctButton;
@property(nonatomic,strong) ICSMInventoryButton *tButton;
@property(nonatomic,strong) UIView *skinFooter;
@property(nonatomic,strong) ICSMInventoryButton *previous;
@property(nonatomic,strong) ICSMInventoryButton *next;
@property(nonatomic,strong) ICSMInventoryButton *confirm;
@property(nonatomic,strong) UILabel *pageNumber;
@property(nonatomic,strong) NSArray<NSDictionary*> *rows;
@property(nonatomic,strong) NSDictionary *snapshot;
@property(nonatomic,strong) NSDictionary *pendingSnapshot;
@property(nonatomic,strong) NSMutableDictionary<NSString*,NSValue*> *scrollOffsets;
@property(nonatomic,copy) NSString *scrollKey;
@property(nonatomic) BOOL skins;
@property(nonatomic) BOOL stickers;
@property(nonatomic,strong) UIView *stickerControls;
@property(nonatomic,strong) UIScrollView *stickerSlotScroll;
@property(nonatomic,strong) NSArray<ICSMInventoryButton*> *stickerButtons;
@property(nonatomic,strong) UITextField *stickerSearch;
@property(nonatomic,strong) NSArray<NSDictionary*> *allStickerRows;
@property(nonatomic) BOOL knifeSplit;
@property(nonatomic) BOOL contextMenuActive;
@property(nonatomic,strong) UILongPressGestureRecognizer *longPress;
@property(nonatomic,strong) ICSMInventoryPopup *popup;
@property(nonatomic) NSInteger columns;
-(void)applySnapshot:(NSDictionary*)snapshot;
-(NSDictionary*)diagnostics;
-(void)dismissMenu;
@end
static ICSMGlassInventory *glassInventory;

@implementation ICSMGlassInventory
-(instancetype)initWithFrame:(CGRect)frame {
    if((self=[super initWithFrame:frame])) {
        self.overrideUserInterfaceStyle=UIUserInterfaceStyleDark;
        self.backgroundColor=UIColor.clearColor;self.accessibilityIdentifier=@"ICSMTranslucentInventory";
        self.container=[UIView new];self.container.clipsToBounds=YES;[self addSubview:self.container];
        UIView *content=self.container;
        self.headerContainer=[UIView new];[content addSubview:self.headerContainer];
        self.titleLabel=inventoryLabel(28,UIFontWeightSemibold);self.titleLabel.textAlignment=NSTextAlignmentCenter;[self.headerContainer addSubview:self.titleLabel];
        self.leftFilters=[UIScrollView new];self.rightFilters=[UIScrollView new];
        for(UIScrollView *scroll in @[self.leftFilters,self.rightFilters]) {
            scroll.showsHorizontalScrollIndicator=NO;scroll.showsVerticalScrollIndicator=NO;
            scroll.contentInsetAdjustmentBehavior=UIScrollViewContentInsetAdjustmentNever;[self.headerContainer addSubview:scroll];
        }
        NSArray *names=@[@"全部",@"手枪",@"冲锋枪",@"步枪",@"狙击枪",@"霰弹枪",@"机枪",@"刀具",@"手套"];
        NSArray *keys=@[@"all",@"secondary",@"smg",@"rifle",@"sniper",@"shotgun",@"machinegun",@"melee",@"gloves"];
        NSMutableArray *filters=[NSMutableArray array];
        for(NSUInteger i=0;i<names.count;++i) {
            ICSMInventoryButton *button=[[ICSMInventoryButton alloc] initWithText:names[i] action:[@"OfflineCategory_" stringByAppendingString:keys[i]]];
            [self.leftFilters addSubview:button];[filters addObject:button];
        }self.filters=filters;
        self.hintLabel=inventoryLabel(14,UIFontWeightRegular);self.hintLabel.textColor=[UIColor colorWithWhite:1 alpha:.72];
        self.hintLabel.text=@"点按选择皮肤 · 长按查看阵营／装备";[content addSubview:self.hintLabel];
        self.statusLabel=inventoryLabel(14,UIFontWeightRegular);self.statusLabel.textColor=[UIColor colorWithWhite:1 alpha:.75];[content addSubview:self.statusLabel];
        self.backButton=[[ICSMInventoryButton alloc] initWithText:@"‹ 返回库存" action:@"OfflineInventoryBack"];[self.headerContainer addSubview:self.backButton];
        // Reused ordinary cards obey the same scroll viewport clipping.
        self.gridContainer=[UIView new];self.gridContainer.clipsToBounds=YES;
        [content addSubview:self.gridContainer];
        UICollectionViewFlowLayout *layout=[UICollectionViewFlowLayout new];layout.minimumInteritemSpacing=16;layout.minimumLineSpacing=16;
        self.grid=[[UICollectionView alloc] initWithFrame:CGRectZero collectionViewLayout:layout];
        self.grid.backgroundColor=UIColor.clearColor;self.grid.opaque=NO;self.grid.delegate=self;self.grid.dataSource=self;
        self.grid.keyboardDismissMode=UIScrollViewKeyboardDismissModeOnDrag;
        self.grid.alwaysBounceVertical=YES;self.grid.showsHorizontalScrollIndicator=NO;self.grid.indicatorStyle=UIScrollViewIndicatorStyleWhite;
        self.grid.contentInsetAdjustmentBehavior=UIScrollViewContentInsetAdjustmentNever;
        [self.grid registerClass:ICSMInventoryCell.class forCellWithReuseIdentifier:@"Weapon"];
        self.grid.clipsToBounds=YES;[self.gridContainer addSubview:self.grid];self.scrollOffsets=[NSMutableDictionary dictionary];
        self.knifeDivider=[UIView new];self.knifeDivider.userInteractionEnabled=NO;
        self.knifeDivider.backgroundColor=[UIColor colorWithWhite:1 alpha:.38];[content addSubview:self.knifeDivider];
        self.knifeCTLabel=inventoryLabel(19,UIFontWeightSemibold);self.knifeCTLabel.text=@"警方";
        self.knifeTLabel=inventoryLabel(19,UIFontWeightSemibold);self.knifeTLabel.text=@"匪徒";
        for(UILabel *label in @[self.knifeCTLabel,self.knifeTLabel]) {label.textAlignment=NSTextAlignmentCenter;[content addSubview:label];}
        self.longPress=[[UILongPressGestureRecognizer alloc] initWithTarget:self action:@selector(longPress:)];
        self.longPress.minimumPressDuration=.45;self.longPress.allowableMovement=10;self.longPress.delegate=self;
        [self.grid addGestureRecognizer:self.longPress];
        self.skinHeader=[UIView new];[content addSubview:self.skinHeader];
        self.skinPreviewGlass=inventorySurface(12);[self.skinHeader addSubview:self.skinPreviewGlass];
        self.skinPreview=[UIImageView new];self.skinPreview.contentMode=UIViewContentModeScaleAspectFit;[self.skinPreviewGlass addSubview:self.skinPreview];
        self.skinName=inventoryLabel(22,UIFontWeightSemibold);self.skinName.adjustsFontSizeToFitWidth=YES;self.skinName.minimumScaleFactor=.7;
        self.skinCount=inventoryLabel(14,UIFontWeightRegular);self.skinCount.textColor=[UIColor colorWithWhite:1 alpha:.75];
        self.ctButton=[[ICSMInventoryButton alloc] initWithText:@"警方" action:@"OfflineSkinCT"];
        self.tButton=[[ICSMInventoryButton alloc] initWithText:@"匪徒" action:@"OfflineSkinT"];
        for(UIView *v in @[self.skinName,self.skinCount,self.ctButton,self.tButton])[self.skinHeader addSubview:v];
        self.skinFooter=[UIView new];[content addSubview:self.skinFooter];
        self.previous=[[ICSMInventoryButton alloc] initWithText:@"‹ 上一页" action:@"OfflineSkinPrevious"];
        self.next=[[ICSMInventoryButton alloc] initWithText:@"下一页 ›" action:@"OfflineSkinNext"];
        self.confirm=[[ICSMInventoryButton alloc] initWithText:@"确认并装备" action:@"OfflineSkinConfirm"];
        self.confirm.prominent=YES;
        self.pageNumber=inventoryLabel(16,UIFontWeightSemibold);self.pageNumber.textAlignment=NSTextAlignmentCenter;
        for(UIView *v in @[self.previous,self.next,self.confirm,self.pageNumber])[self.skinFooter addSubview:v];
        self.stickerControls=[UIView new];[content addSubview:self.stickerControls];
        self.stickerSlotScroll=[UIScrollView new];self.stickerSlotScroll.showsHorizontalScrollIndicator=NO;
        [self.stickerControls addSubview:self.stickerSlotScroll];
        NSMutableArray *slots=[NSMutableArray array];
        for(int n=0;n<5;++n) {
            ICSMInventoryButton *button=[[ICSMInventoryButton alloc] initWithText:[NSString stringWithFormat:@"位置 %d",n+1]
                action:[NSString stringWithFormat:@"OfflineStickerSlot%d",n]];
            [self.stickerSlotScroll addSubview:button];[slots addObject:button];
        }self.stickerButtons=slots;
        self.stickerSearch=[UITextField new];self.stickerSearch.placeholder=@"搜索贴纸";
        self.stickerSearch.font=[UIFont systemFontOfSize:16];self.stickerSearch.textColor=UIColor.whiteColor;
        self.stickerSearch.backgroundColor=[UIColor colorWithWhite:1 alpha:.07];
        self.stickerSearch.layer.borderColor=[UIColor colorWithWhite:1 alpha:.12].CGColor;self.stickerSearch.layer.borderWidth=.5;
        self.stickerSearch.layer.cornerRadius=12;self.stickerSearch.layer.cornerCurve=kCACornerCurveContinuous;
        self.stickerSearch.clearButtonMode=UITextFieldViewModeWhileEditing;
        self.stickerSearch.autocorrectionType=UITextAutocorrectionTypeNo;self.stickerSearch.autocapitalizationType=UITextAutocapitalizationTypeNone;
        UIView *padding=[[UIView alloc] initWithFrame:CGRectMake(0,0,12,1)];self.stickerSearch.leftView=padding;self.stickerSearch.leftViewMode=UITextFieldViewModeAlways;
        self.stickerSearch.returnKeyType=UIReturnKeyDone;
        [self.stickerSearch addTarget:self action:@selector(filterStickers) forControlEvents:UIControlEventEditingChanged];
        [self.stickerSearch addTarget:self action:@selector(endStickerSearch) forControlEvents:UIControlEventEditingDidEndOnExit];
        [self.stickerControls addSubview:self.stickerSearch];
        self.rows=@[];
    }return self;
}
-(void)endStickerSearch {[self.stickerSearch resignFirstResponder];}
-(void)filterStickers {
    if(!self.stickers)return;
    NSString *query=[self.stickerSearch.text stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet];
    if(!query.length)self.rows=self.allStickerRows;
    else {
        NSMutableArray *filtered=[NSMutableArray array];
        for(NSDictionary *row in self.allStickerRows)
            if([row[@"sticker"] intValue]==0 || [row[@"name"] localizedStandardContainsString:query])[filtered addObject:row];
        self.rows=filtered;
    }
    self.pageNumber.text=[NSString stringWithFormat:@"%lu 项",(unsigned long)self.rows.count];
    [self.grid reloadData];
}
-(void)applySnapshot:(NSDictionary*)snapshot {
    if(self.contextMenuActive) {self.pendingSnapshot=snapshot;return;}
    if(self.scrollKey)self.scrollOffsets[self.scrollKey]=[NSValue valueWithCGPoint:self.grid.contentOffset];
    BOOL stickers=[snapshot[@"mode"] isEqual:@"stickers"];
    NSString *key=stickers?[NSString stringWithFormat:@"stickers-%@-%@",snapshot[@"skin"][@"definition"],snapshot[@"stickers"][@"paint"]]:[snapshot[@"mode"] isEqual:@"skins"]?
        [NSString stringWithFormat:@"skins-%@-%@",snapshot[@"skin"][@"definition"],snapshot[@"skin"][@"page"]]:
        [@"inventory-" stringByAppendingString:snapshot[@"category"]?:@"all"];
    if(!stickers || ![self.scrollKey isEqualToString:key]) {self.stickerSearch.text=@"";[self endStickerSearch];}
    self.stickers=stickers;self.snapshot=snapshot;self.skins=stickers || [snapshot[@"mode"] isEqual:@"skins"];
    self.knifeSplit=[snapshot[@"mode"] isEqual:@"knives"] || [snapshot[@"mode"] isEqual:@"gloves"];
    self.titleLabel.text=snapshot[@"title"];self.titleLabel.hidden=!self.skins;self.statusLabel.text=snapshot[@"status"];
    self.rows=[snapshot[@"items"] isKindOfClass:NSArray.class]?snapshot[@"items"]:@[];
    if(stickers) {self.allStickerRows=self.rows;[self filterStickers];}
    // Replace cached counts/index paths before any frame/column change can
    // synchronously lay out the grid against the new, often shorter array.
    [self.grid reloadData];
    for(NSUInteger i=0;i<self.filters.count;++i) {
        NSDictionary *category=snapshot[@"categories"][i];
        if(self.filters[i].chosen!=[category[@"selected"] boolValue])self.filters[i].chosen=[category[@"selected"] boolValue];
    }
    NSDictionary *skin=snapshot[@"skin"];
    if(self.skins) {
        self.skinPreview.image=inventoryImage(skin[@"image"]);self.skinName.text=skin[@"name"];
        BOOL shared=[skin[@"shared"] boolValue],saveOnly=[skin[@"save_only"] boolValue];
        BOOL glove=[skin[@"glove"] boolValue];
        NSString *affiliation=shared?@"双方共用":([skin[@"team"] intValue]==3?@"警方":@"匪徒");
        self.skinCount.text=[NSString stringWithFormat:@"%@ · 共 %@ 种外观 · %@",affiliation,skin[@"total"],saveOnly?(glove?@"保存皮肤后长按手套装备":@"保存皮肤后长按刀具装备"):@"确认后保存到本机"];
        self.ctButton.enabled=[skin[@"ct"] boolValue];self.tButton.enabled=[skin[@"t"] boolValue];
        BOOL locked=shared || [skin[@"locked_team"] boolValue] || ![skin[@"ct"] boolValue] || ![skin[@"t"] boolValue];
        self.ctButton.hidden=self.tButton.hidden=locked;
        self.ctButton.chosen=[skin[@"team"] intValue]==3;self.tButton.chosen=[skin[@"team"] intValue]==2;
        self.previous.enabled=[skin[@"previous"] boolValue];self.next.enabled=[skin[@"next"] boolValue];
        self.confirm.enabled=[skin[@"confirm"] boolValue];
        self.confirm.caption.text=saveOnly?@"确认皮肤":@"确认并装备";self.confirm.accessibilityLabel=self.confirm.caption.text;
        self.pageNumber.text=[NSString stringWithFormat:@"%@ / %@",skin[@"page"],skin[@"pages"]];
    } else self.skinPreview.image=nil;
    self.stickerControls.hidden=!stickers;self.backButton.caption.text=stickers?@"‹ 返回皮肤":@"‹ 返回库存";
    self.previous.hidden=self.next.hidden=stickers;
    if(stickers) {
        NSDictionary *selection=snapshot[@"stickers"];NSArray *slots=selection[@"slots"];
        self.ctButton.hidden=self.tButton.hidden=YES;
        self.skinCount.text=[NSString stringWithFormat:@"原版 %lu 个槽位 · %@ · 共 %@ 款贴纸",(unsigned long)slots.count,
            [skin[@"shared"] boolValue]?@"双方共用":@"专属枪械",skin[@"total"]];
        for(NSUInteger n=0;n<self.stickerButtons.count;++n) {
            ICSMInventoryButton *button=self.stickerButtons[n];button.hidden=n>=slots.count;
            if(n>=slots.count)continue;NSDictionary *slot=slots[n];
            button.caption.text=[NSString stringWithFormat:@"位置 %lu%@",(unsigned long)n+1,[slot[@"sticker"] intValue]?@" · 已贴":@""];
            button.chosen=[slot[@"selected"] boolValue];button.accessibilityLabel=[NSString stringWithFormat:@"%@，%@",button.caption.text,slot[@"name"]];
        }
        self.confirm.caption.text=skin[@"confirm_text"]?:@"确认贴纸";self.confirm.accessibilityLabel=self.confirm.caption.text;
        self.pageNumber.text=[NSString stringWithFormat:@"%lu 项",(unsigned long)self.rows.count];
    }
    self.leftFilters.hidden=self.skins;self.rightFilters.hidden=YES;self.hintLabel.hidden=self.skins || self.knifeSplit;
    self.knifeDivider.hidden=self.knifeCTLabel.hidden=self.knifeTLabel.hidden=!self.knifeSplit;
    self.backButton.hidden=!self.skins;self.skinHeader.hidden=!self.skins;self.skinFooter.hidden=!self.skins;
    [self setNeedsLayout];[self layoutIfNeeded];[self.grid layoutIfNeeded];
    if(![self.scrollKey isEqualToString:key]) {
        CGPoint offset=[self.scrollOffsets[key] CGPointValue];
        offset.y=std::clamp(offset.y,(CGFloat)0,std::max((CGFloat)0,self.grid.contentSize.height-self.grid.bounds.size.height));
        [self.grid setContentOffset:offset animated:NO];
    }self.scrollKey=key;
}
-(void)layoutSubviews {
    [super layoutSubviews];self.container.frame=self.bounds;
    CGFloat w=self.bounds.size.width,h=self.bounds.size.height;
    // Allocate by actual available height, including phone landscape and
    // smaller iPad windows. The large-window layout keeps its original sizes.
    BOOL compact=h<420;
    // The parent scales fonts/cards on phones. Keep actionable chips at least
    // 44 physical UIKit points, including inside their clipping scroll views.
    CGFloat buttonH=44/SourceTouchMenuUIScale(),extra=buttonH-44;
    CGFloat header=(compact?52:64)+extra;
    self.headerContainer.frame=CGRectMake(0,0,w,header);
    self.titleLabel.frame=CGRectMake(std::min<CGFloat>(160,w*.2),4,std::max<CGFloat>(0,w-2*std::min<CGFloat>(160,w*.2)),header-8);
    self.leftFilters.frame=CGRectMake(0,compact?4:8,w,buttonH);self.rightFilters.frame=CGRectZero;
    CGFloat count=std::max<NSUInteger>(1,self.filters.count),chip=std::max((CGFloat)70,(w-8*(count-1))/count);
    for(NSUInteger i=0;i<self.filters.count;++i) {
        self.filters[i].frame=CGRectMake(i*(chip+8),0,chip,buttonH);
    }
    self.leftFilters.contentSize=CGSizeMake(count*(chip+8)-8,buttonH);
    self.backButton.frame=CGRectMake(0,compact?4:8,std::min<CGFloat>(150,w*.2),buttonH);
    self.hintLabel.frame=CGRectMake(4,header,w-8,24);
    CGFloat statusHeight=compact?20:24,footer=self.skins?((compact?52:64)+extra):0;
    CGFloat skinHeight=self.skins?(compact?56:std::clamp(h*.2,(CGFloat)88,(CGFloat)124)):0;
    CGFloat stickerHeight=self.stickers?buttonH+12:0;
    CGFloat gridTop=self.skins?header+skinHeight+stickerHeight+(compact?8:16):header+(compact?32:40);
    CGFloat gridHeight=std::max((CGFloat)0,h-gridTop-statusHeight-footer-(compact?6:8));
    CGFloat centerGap=self.knifeSplit?32:16;
    UICollectionViewFlowLayout *flow=(UICollectionViewFlowLayout*)self.grid.collectionViewLayout;
    NSInteger columns=self.knifeSplit?2:(w>=1100?5:w>=850?4:w>=600?3:2);
    if(self.columns!=columns || flow.minimumInteritemSpacing!=centerGap) {
        self.columns=columns;flow.minimumInteritemSpacing=centerGap;[flow invalidateLayout];
    }
    CGRect gridFrame=CGRectMake(0,gridTop,w,gridHeight);
    if(!CGRectEqualToRect(self.gridContainer.frame,gridFrame)) {
        self.gridContainer.frame=gridFrame;self.grid.frame=self.gridContainer.bounds;
        [self.grid.collectionViewLayout invalidateLayout];
    }
    self.statusLabel.frame=CGRectMake(4,h-statusHeight,w-8,statusHeight);
    self.knifeDivider.frame=CGRectMake(floor(w*.5),header,1,
        std::max((CGFloat)0,gridTop+gridHeight-header));
    self.knifeCTLabel.frame=CGRectMake(0,header,(w-32)*.5,32);
    self.knifeTLabel.frame=CGRectMake((w+32)*.5,header,(w-32)*.5,32);
    self.skinHeader.frame=CGRectMake(0,header,w,skinHeight);
    CGFloat previewWidth=compact?std::min<CGFloat>(180,w*.22):std::min<CGFloat>(240,w*.28);
    self.skinPreviewGlass.frame=CGRectMake(0,0,previewWidth,skinHeight);
    self.skinPreview.frame=CGRectInset(self.skinPreviewGlass.bounds,10,4);
    CGFloat textX=previewWidth+(compact?16:24);
    CGFloat teamSpace=compact && !self.ctButton.hidden?216:0;
    CGFloat textWidth=std::max((CGFloat)0,w-textX-teamSpace);
    self.skinName.font=[UIFont systemFontOfSize:compact?18:22 weight:UIFontWeightSemibold];
    self.skinCount.font=[UIFont systemFontOfSize:compact?12:14 weight:UIFontWeightRegular];
    self.skinName.frame=CGRectMake(textX,0,textWidth,compact?28:32);
    self.skinCount.frame=CGRectMake(textX,compact?29:33,textWidth,compact?22:24);
    if(compact) {
        // Keep team controls beside the preview instead of consuming another
        // vertical row. They retain a 44-point touch target.
        self.ctButton.frame=CGRectMake(w-208,6,96,buttonH);
        self.tButton.frame=CGRectMake(w-104,6,96,buttonH);
    } else {
        CGFloat teamHeight=std::min<CGFloat>(44,skinHeight-60);
        self.ctButton.frame=CGRectMake(textX,skinHeight-teamHeight,96,teamHeight);
        self.tButton.frame=CGRectMake(textX+108,skinHeight-teamHeight,96,teamHeight);
    }
    self.stickerControls.frame=CGRectMake(0,header+skinHeight+6,w,buttonH);
    CGFloat slotWidth=std::min<CGFloat>(w*.68,5*116),slotChip=std::min<CGFloat>(108,(slotWidth-32)/5);
    self.stickerSlotScroll.frame=CGRectMake(0,0,slotWidth,buttonH);
    for(NSUInteger n=0;n<self.stickerButtons.count;++n)self.stickerButtons[n].frame=CGRectMake(n*(slotChip+8),0,slotChip,buttonH);
    self.stickerSlotScroll.contentSize=CGSizeMake(self.snapshot[@"stickers"]? [self.snapshot[@"stickers"][@"slots"] count]*(slotChip+8):0,buttonH);
    self.stickerSearch.frame=CGRectMake(slotWidth+16,0,std::max<CGFloat>(0,w-slotWidth-16),buttonH);
    self.skinFooter.frame=CGRectMake(0,h-statusHeight-footer,w,footer);
    CGFloat buttonWidth=std::min<CGFloat>(128,w*.16);
    CGFloat footerInset=compact?4:8;
    self.previous.frame=CGRectMake(0,footerInset,buttonWidth,buttonH);
    self.pageNumber.frame=CGRectMake(buttonWidth+8,footerInset,80,buttonH);
    self.next.frame=CGRectMake(buttonWidth+96,footerInset,buttonWidth,buttonH);
    self.confirm.frame=CGRectMake(w-std::min<CGFloat>(200,w*.28),footerInset,std::min<CGFloat>(200,w*.28),buttonH);
}
-(NSInteger)collectionView:(UICollectionView*)collectionView numberOfItemsInSection:(NSInteger)section {return self.rows.count;}
-(UICollectionViewCell*)collectionView:(UICollectionView*)collectionView cellForItemAtIndexPath:(NSIndexPath*)indexPath {
    ICSMInventoryCell *cell=[collectionView dequeueReusableCellWithReuseIdentifier:@"Weapon" forIndexPath:indexPath];
    NSDictionary *row=indexPath.item<self.rows.count?self.rows[indexPath.item]:@{@"placeholder":@YES};
    [cell applyRow:row];return cell;
}
-(CGSize)collectionView:(UICollectionView*)collectionView layout:(UICollectionViewLayout*)layout sizeForItemAtIndexPath:(NSIndexPath*)indexPath {
    NSInteger columns=std::max<NSInteger>(1,self.columns);
    CGFloat gap=self.knifeSplit?32:16;
    CGFloat width=floor((collectionView.bounds.size.width-(columns-1)*gap)/columns);
    CGFloat height=self.knifeSplit?std::clamp(width*.18,(CGFloat)96,(CGFloat)112):std::clamp(width*.65,(CGFloat)130,(CGFloat)180);
    if(self.bounds.size.height<420 && self.skins)
        height=std::min(height,std::max((CGFloat)84,collectionView.bounds.size.height));
    return CGSizeMake(width,height);
}
-(void)collectionView:(UICollectionView*)collectionView didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
    if(indexPath.item<self.rows.count && ![self.rows[indexPath.item][@"placeholder"] boolValue])inventorySend(self.rows[indexPath.item][@"id"]);
}
-(BOOL)gestureRecognizerShouldBegin:(UIGestureRecognizer*)gesture {
    if(gesture!=self.longPress)return YES;
    NSIndexPath *index=[self.grid indexPathForItemAtPoint:[gesture locationInView:self.grid]];
    return !self.contextMenuActive && index && index.item<self.rows.count && ![self.rows[index.item][@"placeholder"] boolValue] &&
        (!self.skins || [self.rows[index.item][@"sticker_action"] length]);
}
-(void)longPress:(UILongPressGestureRecognizer*)gesture {
    if(gesture.state!=UIGestureRecognizerStateBegan || self.contextMenuActive)return;
    NSIndexPath *index=[self.grid indexPathForItemAtPoint:[gesture locationInView:self.grid]];
    if(!index || index.item>=self.rows.count)return;
    NSDictionary *row=self.rows[index.item];
    BOOL ctEligible=[row[@"ct"] boolValue],tEligible=[row[@"t"] boolValue];
    BOOL stickerMenu=[row[@"sticker_action"] length]>0;
    if(!stickerMenu && !ctEligible && !tEligible)return;
    UICollectionViewCell *cell=[self.grid cellForItemAtIndexPath:index];if(!cell || !host)return;
    CGRect anchor=[cell convertRect:cell.bounds toView:host];
    ++inventoryMenus;BOOL shared=ctEligible && tEligible;NSInteger side=[row[@"side"] integerValue];
    NSString *mode=stickerMenu?@"stickers":side?([row[@"kind"] isEqual:@"gloves"]?@"equip-glove":@"equip-knife"):(shared?@"equip-team":@"exclusive-team");
    inventoryLastMenu=@{@"definition":row[@"definition"]?:@0,@"ct":@(ctEligible),@"t":@(tEligible),
        @"mode":mode,@"side":@(side),
        @"labels":stickerMenu?@[@"贴纸",@"取消"]:side?@[side==3?@"为警方装备":@"为匪徒装备",@"取消"]:(shared?@[@"为警方装备",@"为匪徒装备"]:@[ctEligible?@"警方专属":@"匪徒专属"])};
    fprintf(stderr,"ICSM_GLASS_INVENTORY menu definition=%d ct=%d t=%d mode=%s material=translucent-fill\n",
        [row[@"definition"] intValue],ctEligible,tEligible,mode.UTF8String);
    self.contextMenuActive=YES;
    self.popup=[[ICSMInventoryPopup alloc] initWithRow:row anchor:anchor];self.popup.frame=host.bounds;
    __weak ICSMGlassInventory *weakSelf=self;
    self.popup.closeHandler=^(NSString *actionID) {
        ICSMGlassInventory *view=weakSelf;if(!view)return;
        [view dismissMenu];if(actionID.length)inventorySend(actionID);
    };
    [host addSubview:self.popup];[self.popup setNeedsLayout];[self.popup layoutIfNeeded];
}
-(void)dismissMenu {
    self.popup.closeHandler=nil;[self.popup removeFromSuperview];self.popup=nil;self.contextMenuActive=NO;
    if(self.pendingSnapshot) {NSDictionary *snapshot=self.pendingSnapshot;self.pendingSnapshot=nil;[self applySnapshot:snapshot];}
}
-(NSDictionary*)diagnostics {
    NSMutableArray *cells=[NSMutableArray array];
    NSMutableArray *rows=[NSMutableArray array];
    for(NSDictionary *row in self.rows) [rows addObject:@{@"definition":row[@"definition"]?:@0,
        @"paint":row[@"paint"]?:@0,@"kind":row[@"kind"]?:@"",@"ct":row[@"ct"]?:@NO,@"t":row[@"t"]?:@NO,
        @"side":row[@"side"]?:@0,@"action":row[@"id"]?:@"",@"placeholder":row[@"placeholder"]?:@NO,
        @"equipped_ct":row[@"equipped_ct"]?:@NO,@"equipped_t":row[@"equipped_t"]?:@NO,@"selected":row[@"selected"]?:@NO}];
    for(NSIndexPath *index in self.grid.indexPathsForVisibleItems) {
        ICSMInventoryCell *cell=(ICSMInventoryCell*)[self.grid cellForItemAtIndexPath:index];
        if(!cell)continue;CGRect f=[cell convertRect:cell.bounds toView:host];
        CGRect clip=CGRectIntersection(cell.bounds,[cell convertRect:self.grid.bounds fromView:self.grid]);
        if(CGRectIsNull(clip))clip=CGRectZero;
        CGRect screenClip=[cell convertRect:clip toView:host];
        [cells addObject:@{@"action":cell.actionID?:@"",@"image_loaded":@(cell.picture.image!=nil),
            @"visible_clip_points":@[@(clip.origin.x),@(clip.origin.y),@(clip.size.width),@(clip.size.height)],
            @"screen_clip_points":@[@(screenClip.origin.x),@(screenClip.origin.y),@(screenClip.size.width),@(screenClip.size.height)],
            @"rect_points":@[@(f.origin.x),@(f.origin.y),@(f.size.width),@(f.size.height)]}];
    }
    CGRect f=self.frame;
    CGRect viewport=[self.gridContainer convertRect:self.gridContainer.bounds toView:host];
    return @{@"backend":@"shared Source neutral frost / settings-style translucent cards",@"visual_effect_views":@(translucentEffectCount(self)+translucentEffectCount(self.popup)),@"visible":@(!self.hidden),
        @"mode":self.snapshot[@"mode"]?:@"",@"category":self.snapshot[@"category"]?:@"",@"title":self.titleLabel.text?:@"",
        @"items":@(self.rows.count),@"total":self.snapshot[@"total"]?:@0,@"columns":@(self.columns),@"knife_split":@(self.knifeSplit),
        @"knife_divider_points":@[@(self.knifeDivider.frame.origin.x),@(self.knifeDivider.frame.origin.y),@(self.knifeDivider.frame.size.width),@(self.knifeDivider.frame.size.height)],
        @"categories":self.snapshot[@"categories"]?:@[],@"rows":rows,
        @"frame_points":@[@(f.origin.x),@(f.origin.y),@(f.size.width),@(f.size.height)],
        @"compact_height_layout":@(self.bounds.size.height<420),
        @"menu_ui_scale":@(SourceTouchMenuUIScale()),
        @"header_points":NSStringFromCGRect([self.headerContainer convertRect:self.headerContainer.bounds toView:host]),
        @"skin_header_points":NSStringFromCGRect([self.skinHeader convertRect:self.skinHeader.bounds toView:host]),
        @"skin_footer_points":NSStringFromCGRect([self.skinFooter convertRect:self.skinFooter.bounds toView:host]),
        @"confirm_points":NSStringFromCGRect([self.confirm convertRect:self.confirm.bounds toView:host]),
        @"skin_preview_loaded":@(self.skinPreview.image!=nil),
        @"grid_viewport_points":@[@(viewport.origin.x),@(viewport.origin.y),@(viewport.size.width),@(viewport.size.height)],
        @"clipping":@"UICollectionView clips ordinary UIView cards",
        @"scroll_offset":@(self.grid.contentOffset.y),@"content_height":@(self.grid.contentSize.height),@"visible_cells":cells,
        @"activations":@(inventoryActivations),@"menus":@(inventoryMenus),@"last_action":inventoryLastAction,
        @"last_menu":inventoryLastMenu?:@{},
        @"snapshot_updates":@(inventorySnapshots),@"context_menu_open":@(self.contextMenuActive),
        @"skin":self.snapshot[@"skin"]?:@{},@"status":self.snapshot[@"status"]?:@""};
}
@end

static void trimInventoryMemory() {
    if(!NSThread.isMainThread)return;
    [inventoryImages removeAllObjects];
    if(inventoryVisible)return;
    [glassInventory dismissMenu];[glassInventory removeFromSuperview];glassInventory=nil;
    inventoryJSON.clear();
    inventoryDiagnostics=@{@"visible":@NO,@"resources_released":@YES,@"resource_trims":@(++inventoryResourceTrims)};
}
static void updateInventoryLayout() {
    if(!NSThread.isMainThread || !glassInventory)return;
    bool show=inventoryVisible && host && host.window && !editing && !consoleVisible;
    if(inventoryVisible)inventoryHiddenSince=0;
    else {
        if(!inventoryHiddenSince)inventoryHiddenSince=now();
        if(now()-inventoryHiddenSince>=10) {trimInventoryMemory();return;}
    }
    glassInventory.hidden=!show;
    if(!show)[glassInventory endStickerSearch];
    if(!show && glassInventory.contextMenuActive)[glassInventory dismissMenu];
    if(show) {
        if(glassInventory.superview!=host)[host addSubview:glassInventory];
        UIEdgeInsets insets=host.safeAreaInsets;
        CGFloat top=std::max<CGFloat>(insets.top+16,glassTitle?CGRectGetMaxY(glassTitle.frame)+16:48);
        CGFloat bottom=glassNavigation && !glassNavigation.hidden?CGRectGetMinY(glassNavigation.frame)-20:
            host.bounds.size.height-insets.bottom-152;
        CGRect frame=CGRectMake(insets.left+24,top,std::max<CGFloat>(0,host.bounds.size.width-insets.left-insets.right-48),std::max<CGFloat>(0,bottom-top));
        CGFloat scale=SourceTouchMenuUIScale();
        CGRect bounds=CGRectMake(0,0,frame.size.width/scale,frame.size.height/scale);
        CGPoint center=CGPointMake(CGRectGetMidX(frame),CGRectGetMidY(frame));
        CGAffineTransform transform=CGAffineTransformMakeScale(scale,scale);
        // Set bounds/center with a transform; assigning frame on a transformed
        // view is undefined. convertRect and UIKit hit testing retain the same
        // physical viewport while laying out slightly smaller cards and text.
        if(!CGRectEqualToRect(glassInventory.bounds,bounds) || !CGPointEqualToPoint(glassInventory.center,center) ||
           !CGAffineTransformEqualToTransform(glassInventory.transform,transform)) {
            [glassInventory dismissMenu];glassInventory.transform=transform;
            glassInventory.bounds=bounds;glassInventory.center=center;
            [glassInventory setNeedsLayout];[glassInventory layoutIfNeeded];
        }
    }
}
static void shutdownInventory() {
    std::lock_guard<std::recursive_mutex> lock(mutex);inventoryActivate=nullptr;inventoryVisible=false;inventoryJSON.clear();
    void (^remove)(void)=^{[glassInventory dismissMenu];[glassInventory removeFromSuperview];glassInventory=nil;[inventoryImages removeAllObjects];};
    if(NSThread.isMainThread)remove();else dispatch_async(dispatch_get_main_queue(),remove);
    inventoryDiagnostics=@{@"visible":@NO};
}
extern "C" void SourceTouchSetInventoryCallback(void (*activate)(const char*)) {
    std::lock_guard<std::recursive_mutex> lock(mutex);inventoryActivate=activate;
}
extern "C" bool SourceTouchUpdateInventory(const char *json,bool visible) {
    if(!NSThread.isMainThread)return false;
    std::lock_guard<std::recursive_mutex> lock(mutex);inventoryVisible=visible;
    if(!glassInventory && visible && host && json && json[0]) {
        if(@available(iOS 26.0,*))glassInventory=[[ICSMGlassInventory alloc] initWithFrame:CGRectZero];
        if(glassInventory)fprintf(stderr,"ICSM_GLASS_INVENTORY created=1 material=translucent-fill grid=collection-view\n");
    }
    updateInventoryLayout();
    if(glassInventory && json && json[0] && inventoryJSON!=json) {
        NSData *data=[[NSString stringWithUTF8String:json] dataUsingEncoding:NSUTF8StringEncoding];
        id value=data?[NSJSONSerialization JSONObjectWithData:data options:0 error:nil]:nil;
        if([value isKindOfClass:NSDictionary.class] && [value[@"categories"] isKindOfClass:NSArray.class] && [value[@"categories"] count]==9) {
            inventoryJSON=json;++inventorySnapshots;[glassInventory applySnapshot:value];
        }
    }
    updateInventoryLayout();return glassInventory!=nil;
}
