// UIKit owns editing gestures. The gameplay router never sees these touches.
// Included outside the C++ namespace so Objective-C classes have global scope.
@interface SourceTouchLayoutEditor : UIView {
    NSMutableDictionary *_draft;
    NSMutableArray<NSDictionary*> *_customDraft;
    NSMutableDictionary<NSString*,UIButton*> *_buttons,*_selectors;
    UIView *_canvas,*_rightHalf,*_keyPicker,*_keyPanel;
    UILabel *_title,*_help,*_sizeText,*_message,*_lookText,*_moveText,*_keyTitle,*_keyHelp;
    UIScrollView *_selector,*_keys;
    UISlider *_slider;
    UIButton *_cancel,*_reset,*_save,*_add,*_mapping,*_delete,*_keyCancel;
    NSMutableArray<UIButton*> *_keyButtons;
    NSString *_selected;
    CGRect _panStart;
    CGFloat _canvasScale;
    BOOL _addingKey;
}
- (NSDictionary*)diagnostics;
- (BOOL)applyAction:(NSString*)action key:(int)key;
@end
static __strong SourceTouchLayoutEditor *layoutEditor;

static NSString *editorTitle(NSString *name) {
    NSDictionary *names=@{@"pause":@"暂停",@"buy":@"购买",@"scoreboard":@"战绩",@"jump":@"跳跃",@"attack":@"开火",@"attack2":@"瞄准",@"duck":@"蹲伏",
        @"weapon1":@"主武器",@"weapon2":@"副武器",@"weapon3":@"匕首",@"weapon4":@"投掷物",@"weapon5":@"炸弹",@"use":@"使用",@"reload":@"换弹",@"inspect":@"检视",@"drop":@"丢弃",@"move":@"移动",@"walk":@"静步"};
    return names[name]?:name;
}
static NSArray<NSString*> *editorNames() {
    return @[@"attack",@"attack2",@"duck",@"jump",@"use",@"reload",@"inspect",@"drop",@"walk",@"pause",@"buy",@"scoreboard",@"weapon1",@"weapon2",@"weapon3",@"weapon4",@"weapon5"];
}
static UIButton *editorButton(NSString *title,id target,SEL action) {
    UIButton *button=[UIButton buttonWithType:UIButtonTypeSystem];[button setTitle:title forState:UIControlStateNormal];
    button.titleLabel.font=[UIFont systemFontOfSize:18 weight:UIFontWeightSemibold];button.tintColor=UIColor.whiteColor;
    button.backgroundColor=[UIColor colorWithWhite:.23 alpha:1];button.layer.cornerRadius=8;
    [button addTarget:target action:action forControlEvents:UIControlEventTouchUpInside];return button;
}
@implementation SourceTouchLayoutEditor
- (NSArray<NSString*>*)names {
    NSMutableArray *result=[editorNames() mutableCopy];for(NSDictionary *entry in _customDraft)[result addObject:entry[@"id"]];return result;
}
- (NSString*)titleForName:(NSString*)name {
    NSDictionary *entry=customDefinition(name,_customDraft);return entry?keyboardTitle([entry[@"key"] intValue]):editorTitle(name);
}
- (CGRect)baseRectForName:(NSString*)name {
    NSDictionary *entry=customDefinition(name,_customDraft);
    if(entry)return customDefaultRect([_customDraft indexOfObject:entry]);
    auto it=defaultRects.find(name.UTF8String);return it==defaultRects.end()?CGRectZero:it->second;
}
- (CGRect)rectForName:(NSString*)name {return resolvedLayoutRect(name,[self baseRectForName:name],_draft);}
- (void)rebuildButtons {
    for(UIView *v in _buttons.allValues)[v removeFromSuperview];for(UIView *v in _selectors.allValues)[v removeFromSuperview];
    [_buttons removeAllObjects];[_selectors removeAllObjects];CGFloat x=0;
    for(NSString *name in [self names]) {
        NSString *title=[self titleForName:name];
        UIButton *selection=editorButton(title,self,@selector(selectButton:));selection.accessibilityIdentifier=name;
        selection.frame=CGRectMake(x,0,96,36);selection.titleLabel.adjustsFontSizeToFitWidth=YES;x+=104;
        [_selector addSubview:selection];_selectors[name]=selection;
        UIButton *button=editorButton(title,self,@selector(selectButton:));button.accessibilityIdentifier=name;
        button.titleLabel.font=[UIFont systemFontOfSize:14 weight:UIFontWeightMedium];button.titleLabel.adjustsFontSizeToFitWidth=YES;button.titleLabel.minimumScaleFactor=.55;
        button.layer.borderWidth=1.5;button.layer.borderColor=UIColor.whiteColor.CGColor;[_canvas addSubview:button];_buttons[name]=button;
        UIPanGestureRecognizer *pan=[[UIPanGestureRecognizer alloc] initWithTarget:self action:@selector(dragButton:)];pan.maximumNumberOfTouches=1;[button addGestureRecognizer:pan];
    }
    _selector.contentSize=CGSizeMake(x,36);[self refreshPreview];
}
- (instancetype)initWithFrame:(CGRect)frame {
    if(!(self=[super initWithFrame:frame]))return nil;
    self.backgroundColor=[UIColor colorWithRed:.055 green:.07 blue:.10 alpha:1];self.multipleTouchEnabled=YES;
    self.accessibilityViewIsModal=YES;self.autoresizingMask=UIViewAutoresizingFlexibleWidth|UIViewAutoresizingFlexibleHeight;
    _draft=[savedLayout mutableCopy];_customDraft=[savedCustomButtons mutableCopy];_buttons=[NSMutableDictionary dictionary];_selectors=[NSMutableDictionary dictionary];
    _title=[UILabel new];_title.text=@"触摸屏键位调整";_title.textColor=UIColor.whiteColor;_title.font=[UIFont systemFontOfSize:27 weight:UIFontWeightBold];[self addSubview:_title];
    _help=[UILabel new];_help.text=@"拖动按键调整位置，滑杆调整大小。新增按键可映射键盘；按住／松开与键盘一致，按键区域不会移动或转动视角。";
    _help.textColor=[UIColor colorWithWhite:.82 alpha:1];_help.font=[UIFont systemFontOfSize:16];_help.numberOfLines=2;[self addSubview:_help];
    _cancel=editorButton(@"取消",self,@selector(cancelEditor));_reset=editorButton(@"恢复默认",self,@selector(resetEditor));_save=editorButton(@"保存",self,@selector(saveEditor));
    _save.backgroundColor=[UIColor colorWithRed:.10 green:.40 blue:.62 alpha:1];for(UIView *v in @[_cancel,_reset,_save])[self addSubview:v];
    _sizeText=[UILabel new];_sizeText.textColor=UIColor.whiteColor;_sizeText.font=[UIFont systemFontOfSize:16 weight:UIFontWeightMedium];_sizeText.adjustsFontSizeToFitWidth=YES;[self addSubview:_sizeText];
    _slider=[UISlider new];_slider.minimumValue=.35;_slider.maximumValue=3;_slider.accessibilityLabel=@"选中按键的大小";
    [_slider addTarget:self action:@selector(changeSize:) forControlEvents:UIControlEventValueChanged];[self addSubview:_slider];
    _add=editorButton(@"＋ 新增",self,@selector(addButton));_add.accessibilityIdentifier=@"TouchLayoutAddKey";
    _mapping=editorButton(@"更改映射",self,@selector(mapButton));_mapping.accessibilityIdentifier=@"TouchLayoutMapKey";
    _delete=editorButton(@"删除",self,@selector(deleteButton));_delete.accessibilityIdentifier=@"TouchLayoutDeleteKey";
    for(UIButton *v in @[_add,_mapping,_delete]){v.titleLabel.font=[UIFont systemFontOfSize:16 weight:UIFontWeightSemibold];[self addSubview:v];}
    _message=[UILabel new];_message.textColor=[UIColor colorWithRed:1 green:.75 blue:.38 alpha:1];_message.font=[UIFont systemFontOfSize:15];_message.adjustsFontSizeToFitWidth=YES;[self addSubview:_message];
    _selector=[UIScrollView new];_selector.showsHorizontalScrollIndicator=YES;[self addSubview:_selector];
    _canvas=[UIView new];_canvas.backgroundColor=[UIColor colorWithWhite:.13 alpha:1];_canvas.clipsToBounds=YES;_canvas.layer.borderWidth=1;_canvas.layer.borderColor=[UIColor colorWithWhite:.5 alpha:1].CGColor;[self addSubview:_canvas];
    _rightHalf=[UIView new];_rightHalf.backgroundColor=[UIColor colorWithRed:.10 green:.28 blue:.40 alpha:.65];_rightHalf.userInteractionEnabled=NO;[_canvas addSubview:_rightHalf];
    _lookText=[UILabel new];_lookText.text=@"右半屏 · 滑动视角\n按键区域不会转动视角";_lookText.numberOfLines=2;_lookText.textColor=[UIColor colorWithWhite:1 alpha:.55];_lookText.textAlignment=NSTextAlignmentCenter;_lookText.font=[UIFont systemFontOfSize:15];[_rightHalf addSubview:_lookText];
    _moveText=[UILabel new];_moveText.text=@"左半屏 · 滑动移动\n松手停止 · 按键区域独立";_moveText.numberOfLines=2;_moveText.textColor=[UIColor colorWithWhite:1 alpha:.55];_moveText.textAlignment=NSTextAlignmentCenter;_moveText.font=[UIFont systemFontOfSize:15];_moveText.userInteractionEnabled=NO;[_canvas addSubview:_moveText];
    _selected=@"attack";_message.text=layoutError?:@"尚未保存；取消会放弃本次修改。";[self rebuildButtons];
    [self setNeedsLayout];return self;
}
- (void)layoutSubviews {
    [super layoutSubviews];if(!_canvas || viewSize.width<=0 || viewSize.height<=0)return;
    CGFloat left=self.safeAreaInsets.left+20,right=self.bounds.size.width-self.safeAreaInsets.right-20,top=self.safeAreaInsets.top+12;
    _title.frame=CGRectMake(left,top,right-left-340,38);_title.adjustsFontSizeToFitWidth=YES;
    _cancel.frame=CGRectMake(right-320,top,80,40);_reset.frame=CGRectMake(right-230,top,130,40);_save.frame=CGRectMake(right-90,top,90,40);
    _help.frame=CGRectMake(left,top+47,right-left,42);
    CGFloat textWidth=std::min<CGFloat>(200,(right-left)*.25),actionsX=right-292;
    _sizeText.frame=CGRectMake(left,top+91,textWidth,32);_slider.frame=CGRectMake(left+textWidth+10,top+91,std::max<CGFloat>(50,actionsX-left-textWidth-24),32);
    _add.frame=CGRectMake(actionsX,top+90,86,36);_mapping.frame=CGRectMake(actionsX+94,top+90,104,36);_delete.frame=CGRectMake(actionsX+206,top+90,86,36);
    _message.frame=CGRectMake(left,top+171,right-left,24);_selector.frame=CGRectMake(left,top+130,right-left,38);
    CGFloat canvasTop=top+205,spaceH=std::max<CGFloat>(1,self.bounds.size.height-self.safeAreaInsets.bottom-16-canvasTop);
    _canvasScale=std::min((right-left)/viewSize.width,spaceH/viewSize.height);
    CGSize preview=CGSizeMake(viewSize.width*_canvasScale,viewSize.height*_canvasScale);
    _canvas.frame=CGRectMake(left+(right-left-preview.width)*.5,canvasTop+(spaceH-preview.height)*.5,preview.width,preview.height);
    _rightHalf.frame=CGRectMake(preview.width*.5,0,preview.width*.5,preview.height);
    _lookText.frame=CGRectMake(0,preview.height*.40,preview.width*.5,60);_moveText.frame=CGRectMake(0,preview.height*.40,preview.width*.5,60);
    [self refreshPreview];[self layoutKeyPicker];
}
- (void)refreshPreview {
    for(NSString *name in [self names]) {
        CGRect r=[self rectForName:name];UIButton *button=_buttons[name];button.hidden=CGRectIsEmpty(r);
        button.frame=CGRectMake(r.origin.x*_canvasScale,r.origin.y*_canvasScale,r.size.width*_canvasScale,r.size.height*_canvasScale);
        button.layer.cornerRadius=[name isEqual:@"attack"]?std::min(button.bounds.size.width,button.bounds.size.height)*.5:5;
        BOOL chosen=[name isEqual:_selected];button.layer.borderWidth=chosen?3:1;
        button.layer.borderColor=(chosen?[UIColor colorWithRed:1 green:.78 blue:.26 alpha:1]:UIColor.whiteColor).CGColor;
        button.backgroundColor=[UIColor colorWithRed:chosen?.28:.15 green:chosen?.35:.19 blue:chosen?.44:.25 alpha:.9];
        _selectors[name].backgroundColor=chosen?[UIColor colorWithRed:.10 green:.40 blue:.62 alpha:1]:[UIColor colorWithWhite:.23 alpha:1];
    }
    if(_buttons[_selected])[_canvas bringSubviewToFront:_buttons[_selected]];
    CGRect r=[self rectForName:_selected],base=[self baseRectForName:_selected];CGFloat scale=base.size.width>0?r.size.width/base.size.width:1;
    _slider.value=scale;_sizeText.text=[NSString stringWithFormat:@"%@ · %.0f%%",[self titleForName:_selected],scale*100];
    BOOL custom=customDefinition(_selected,_customDraft)!=nil;
    _mapping.enabled=custom;_delete.enabled=custom;_mapping.alpha=custom?1:.35;_delete.alpha=custom?1:.35;_add.enabled=_customDraft.count<customButtonLimit;_add.alpha=_add.enabled?1:.35;
}
- (void)selectButton:(UIButton*)button {_selected=button.accessibilityIdentifier;[self refreshPreview];}
- (void)dragButton:(UIPanGestureRecognizer*)gesture {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    if(gesture.state==UIGestureRecognizerStateBegan){_selected=gesture.view.accessibilityIdentifier;_panStart=[self rectForName:_selected];}
    if(_canvasScale>0 && (gesture.state==UIGestureRecognizerStateBegan || gesture.state==UIGestureRecognizerStateChanged || gesture.state==UIGestureRecognizerStateEnded)) {
        CGPoint delta=[gesture translationInView:_canvas];CGRect r=_panStart;r.origin.x+=delta.x/_canvasScale;r.origin.y+=delta.y/_canvasScale;
        _draft[_selected]=normalizedLayoutRect(boundedLayoutRect(r));[self refreshPreview];
    }
}
- (void)changeSize:(UISlider*)slider {
    std::lock_guard<std::recursive_mutex> lock(mutex);CGRect current=[self rectForName:_selected];CGSize size=[self baseRectForName:_selected].size;if(size.width<=0 || size.height<=0)return;
    CGFloat factor=std::min<CGFloat>(slider.value,std::min((viewSize.width-safe.left-safe.right)/size.width,(viewSize.height-safe.top-safe.bottom)/size.height));
    size=CGSizeMake(size.width*factor,size.height*factor);CGRect r=CGRectMake(CGRectGetMidX(current)-size.width*.5,CGRectGetMidY(current)-size.height*.5,size.width,size.height);
    _draft[_selected]=normalizedLayoutRect(boundedLayoutRect(r));[self refreshPreview];
}
- (void)layoutKeyPicker {
    if(!_keyPicker)return;_keyPicker.frame=self.bounds;
    CGFloat margin=20,w=std::min<CGFloat>(920,self.bounds.size.width-self.safeAreaInsets.left-self.safeAreaInsets.right-2*margin);
    CGFloat h=std::min<CGFloat>(640,self.bounds.size.height-self.safeAreaInsets.top-self.safeAreaInsets.bottom-2*margin);
    _keyPanel.frame=CGRectMake((self.bounds.size.width-w)*.5,(self.bounds.size.height-h)*.5,w,h);
    _keyTitle.frame=CGRectMake(20,12,w-140,32);_keyCancel.frame=CGRectMake(w-100,12,80,36);
    _keyHelp.frame=CGRectMake(20,50,w-40,44);_keys.frame=CGRectMake(16,100,w-32,h-116);
    NSUInteger columns=std::max<NSUInteger>(4,(NSUInteger)((w-32)/104));CGFloat gap=8,cell=(_keys.bounds.size.width-gap*(columns-1))/columns;
    for(NSUInteger i=0;i<_keyButtons.count;++i)_keyButtons[i].frame=CGRectMake((i%columns)*(cell+gap),(i/columns)*52,cell,44);
    _keys.contentSize=CGSizeMake(_keys.bounds.size.width,((_keyButtons.count+columns-1)/columns)*52);
}
- (void)showKeyPicker:(BOOL)adding {
    if(_keyPicker || (adding && _customDraft.count>=customButtonLimit) || (!adding && !customDefinition(_selected,_customDraft)))return;
    _addingKey=adding;_keyPicker=[UIView new];_keyPicker.backgroundColor=[UIColor colorWithWhite:0 alpha:.7];_keyPicker.accessibilityViewIsModal=YES;
    _keyPanel=[UIView new];_keyPanel.backgroundColor=[UIColor colorWithWhite:.12 alpha:1];_keyPanel.layer.cornerRadius=20;[_keyPicker addSubview:_keyPanel];
    _keyTitle=[UILabel new];_keyTitle.text=adding?@"新增按键 · 选择键盘映射":@"更改键盘映射";_keyTitle.textColor=UIColor.whiteColor;_keyTitle.font=[UIFont systemFontOfSize:21 weight:UIFontWeightBold];_keyTitle.adjustsFontSizeToFitWidth=YES;[_keyPanel addSubview:_keyTitle];
    _keyHelp=[UILabel new];_keyHelp.text=@"执行游戏中该键当前绑定的功能，未绑定的键不会产生动作。可用多个按钮分别按住修饰键与其他键。";_keyHelp.textColor=[UIColor colorWithWhite:.8 alpha:1];_keyHelp.font=[UIFont systemFontOfSize:15];_keyHelp.numberOfLines=2;[_keyPanel addSubview:_keyHelp];
    _keyCancel=editorButton(@"取消",self,@selector(closeKeyPicker));[_keyPanel addSubview:_keyCancel];
    _keys=[UIScrollView new];_keys.alwaysBounceVertical=YES;[_keyPanel addSubview:_keys];_keyButtons=[NSMutableArray array];
    for(const auto &key:touchKeyboardKeys) {
        UIButton *button=editorButton(@(key.title),self,@selector(chooseKey:));button.tag=key.code;button.titleLabel.font=[UIFont systemFontOfSize:15 weight:UIFontWeightSemibold];button.titleLabel.adjustsFontSizeToFitWidth=YES;button.titleLabel.minimumScaleFactor=.65;
        button.accessibilityIdentifier=[NSString stringWithFormat:@"TouchKey_%d",key.code];
        if(!adding && key.code==[customDefinition(_selected,_customDraft)[@"key"] intValue])button.backgroundColor=[UIColor colorWithRed:.10 green:.40 blue:.62 alpha:1];
        [_keys addSubview:button];[_keyButtons addObject:button];
    }
    [self addSubview:_keyPicker];[self layoutKeyPicker];
}
- (void)closeKeyPicker {[_keyPicker removeFromSuperview];_keyPicker=nil;_keyPanel=nil;_keys=nil;_keyTitle=nil;_keyHelp=nil;_keyCancel=nil;_keyButtons=nil;}
- (BOOL)assignKey:(int)key adding:(BOOL)adding {
    if(!keyboardTitle(key))return NO;
    if(adding) {
        if(_customDraft.count>=customButtonLimit)return NO;
        NSString *name=[@"custom." stringByAppendingString:NSUUID.UUID.UUIDString];
        [_customDraft addObject:@{@"id":name,@"key":@(key)}];_draft[name]=normalizedLayoutRect(boundedLayoutRect(customDefaultRect(_customDraft.count-1)));_selected=name;
    } else {
        NSDictionary *entry=customDefinition(_selected,_customDraft);if(!entry)return NO;
        _customDraft[[_customDraft indexOfObject:entry]]=@{@"id":_selected,@"key":@(key)};
    }
    [self closeKeyPicker];[self rebuildButtons];[self setNeedsLayout];[self layoutIfNeeded];
    UIButton *selection=_selectors[_selected];if(selection)[_selector scrollRectToVisible:selection.frame animated:YES];
    _message.text=[NSString stringWithFormat:@"已映射为 %@，点击保存后生效。",keyboardTitle(key)];return YES;
}
- (void)chooseKey:(UIButton*)button {std::lock_guard<std::recursive_mutex> lock(mutex);[self assignKey:(int)button.tag adding:_addingKey];}
- (void)addButton {[self showKeyPicker:YES];}
- (void)mapButton {[self showKeyPicker:NO];}
- (void)deleteButton {
    std::lock_guard<std::recursive_mutex> lock(mutex);NSDictionary *entry=customDefinition(_selected,_customDraft);if(!entry)return;
    [_draft removeObjectForKey:_selected];[_customDraft removeObject:entry];_selected=@"attack";[self rebuildButtons];_message.text=@"已从预览删除，点击保存后生效。";
}
- (void)resetEditor {std::lock_guard<std::recursive_mutex> lock(mutex);[_draft removeAllObjects];[_customDraft removeAllObjects];_selected=@"attack";_message.text=@"已恢复默认预览，点击保存后生效。";[self rebuildButtons];}
- (void)finishEditor {cancel(now());editing=false;dirty=true;[self closeKeyPicker];[self removeFromSuperview];layoutEditor=nil;}
- (void)cancelEditor {std::lock_guard<std::recursive_mutex> lock(mutex);[self finishEditor];}
- (void)saveEditor {
    std::lock_guard<std::recursive_mutex> lock(mutex);NSError *error=nil;
    NSURL *url=layoutURL();BOOL ok=[NSFileManager.defaultManager createDirectoryAtURL:[url URLByDeletingLastPathComponent] withIntermediateDirectories:YES attributes:nil error:&error];
    NSData *data=ok?[NSJSONSerialization dataWithJSONObject:@{@"version":@2,@"controls":_draft,@"custom_buttons":_customDraft} options:NSJSONWritingPrettyPrinted error:&error]:nil;
    if(!data || ![data writeToURL:url options:NSDataWritingAtomic error:&error]){_message.text=[NSString stringWithFormat:@"保存失败：%@",error.localizedDescription?:@"无法写入文件"];return;}
    savedLayout=[_draft mutableCopy];savedCustomButtons=[_customDraft mutableCopy];layoutError=nil;++layoutSaves;[self finishEditor];
}
- (BOOL)applyAction:(NSString*)action key:(int)key {
    if([action isEqual:@"select"]){if(key<0 || (NSUInteger)key>=_customDraft.count)return NO;_selected=_customDraft[key][@"id"];[self refreshPreview];return YES;}
    if([action isEqual:@"add"]){[self showKeyPicker:YES];return [self assignKey:key adding:YES];}
    if([action isEqual:@"map"]){[self showKeyPicker:NO];return [self assignKey:key adding:NO];}
    if([action isEqual:@"delete"]){if(!customDefinition(_selected,_customDraft))return NO;[self deleteButton];return YES;}
    if([action isEqual:@"save"]){[self saveEditor];return !editing;}
    if([action isEqual:@"cancel"]){[self cancelEditor];return YES;}
    if([action isEqual:@"picker"]){[self showKeyPicker:YES];return _keyPicker!=nil;}
    return NO;
}
- (NSDictionary*)diagnostics {
    return @{@"selected":_selected?:@"",@"draft":_draft?:@{},@"custom_buttons":_customDraft?:@[],@"key_picker_open":@(_keyPicker!=nil),@"keyboard_choices":@(sizeof(touchKeyboardKeys)/sizeof(*touchKeyboardKeys)),@"custom_button_limit":@(customButtonLimit),@"canvas_frame_points":@[@(_canvas.frame.origin.x),@(_canvas.frame.origin.y),@(_canvas.frame.size.width),@(_canvas.frame.size.height)],@"preview_scale":@(_canvasScale)};
}
@end

extern "C" bool SourceTouchIsEditing() {std::lock_guard<std::recursive_mutex> lock(mutex);return editing;}
extern "C" bool SourceTouchLayoutEditorAction(const char *action,int key) {
    if(!NSThread.isMainThread || !action)return false;std::lock_guard<std::recursive_mutex> lock(mutex);
    SourceTouchLayoutEditor *editor=layoutEditor;return editor && [editor applyAction:@(action) key:key];
}
extern "C" void SourceTouchOpenLayoutEditor() {
    void (^openEditor)(void)=^{
        std::lock_guard<std::recursive_mutex> lock(mutex);
        if(editing || !host || !host.window)return;
        updateView(host);loadLayout();cancel(now());editing=true;
        CAMetalLayer *layer=(CAMetalLayer*)host.layer;
        if(!initialize(layer.device,layer.pixelFormat)){editing=false;return;}
        dirty=true;rebuild();
        layoutEditor=[[SourceTouchLayoutEditor alloc] initWithFrame:host.bounds];[host addSubview:layoutEditor];[layoutEditor layoutIfNeeded];
    };
    if(NSThread.isMainThread)openEditor();else dispatch_async(dispatch_get_main_queue(),openEditor);
}
