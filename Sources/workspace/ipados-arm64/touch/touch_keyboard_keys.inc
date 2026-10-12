// Physical SDL scancodes use the same Source binding path as a real keyboard.
struct TouchKeyboardKey { int code; const char *title; };
static const TouchKeyboardKey touchKeyboardKeys[]={
    {SDL_SCANCODE_A,"A"},{SDL_SCANCODE_B,"B"},{SDL_SCANCODE_C,"C"},{SDL_SCANCODE_D,"D"},
    {SDL_SCANCODE_E,"E"},{SDL_SCANCODE_F,"F"},{SDL_SCANCODE_G,"G"},{SDL_SCANCODE_H,"H"},
    {SDL_SCANCODE_I,"I"},{SDL_SCANCODE_J,"J"},{SDL_SCANCODE_K,"K"},{SDL_SCANCODE_L,"L"},
    {SDL_SCANCODE_M,"M"},{SDL_SCANCODE_N,"N"},{SDL_SCANCODE_O,"O"},{SDL_SCANCODE_P,"P"},
    {SDL_SCANCODE_Q,"Q"},{SDL_SCANCODE_R,"R"},{SDL_SCANCODE_S,"S"},{SDL_SCANCODE_T,"T"},
    {SDL_SCANCODE_U,"U"},{SDL_SCANCODE_V,"V"},{SDL_SCANCODE_W,"W"},{SDL_SCANCODE_X,"X"},
    {SDL_SCANCODE_Y,"Y"},{SDL_SCANCODE_Z,"Z"},
    {SDL_SCANCODE_0,"0"},{SDL_SCANCODE_1,"1"},{SDL_SCANCODE_2,"2"},{SDL_SCANCODE_3,"3"},
    {SDL_SCANCODE_4,"4"},{SDL_SCANCODE_5,"5"},{SDL_SCANCODE_6,"6"},{SDL_SCANCODE_7,"7"},
    {SDL_SCANCODE_8,"8"},{SDL_SCANCODE_9,"9"},
    {SDL_SCANCODE_SPACE,"Space"},{SDL_SCANCODE_TAB,"Tab"},{SDL_SCANCODE_RETURN,"Enter"},
    {SDL_SCANCODE_ESCAPE,"Esc"},{SDL_SCANCODE_BACKSPACE,"Backspace"},
    {SDL_SCANCODE_LSHIFT,"L Shift"},{SDL_SCANCODE_RSHIFT,"R Shift"},
    {SDL_SCANCODE_LCTRL,"L Ctrl"},{SDL_SCANCODE_RCTRL,"R Ctrl"},
    {SDL_SCANCODE_LALT,"L Alt"},{SDL_SCANCODE_RALT,"R Alt"},
    {SDL_SCANCODE_UP,"↑"},{SDL_SCANCODE_DOWN,"↓"},{SDL_SCANCODE_LEFT,"←"},{SDL_SCANCODE_RIGHT,"→"},
    {SDL_SCANCODE_F1,"F1"},{SDL_SCANCODE_F2,"F2"},{SDL_SCANCODE_F3,"F3"},{SDL_SCANCODE_F4,"F4"},
    {SDL_SCANCODE_F5,"F5"},{SDL_SCANCODE_F6,"F6"},{SDL_SCANCODE_F7,"F7"},{SDL_SCANCODE_F8,"F8"},
    {SDL_SCANCODE_F9,"F9"},{SDL_SCANCODE_F10,"F10"},{SDL_SCANCODE_F11,"F11"},{SDL_SCANCODE_F12,"F12"},
    {SDL_SCANCODE_GRAVE,"`"},{SDL_SCANCODE_MINUS,"-"},{SDL_SCANCODE_EQUALS,"="},
    {SDL_SCANCODE_LEFTBRACKET,"["},{SDL_SCANCODE_RIGHTBRACKET,"]"},{SDL_SCANCODE_BACKSLASH,"\\"},
    {SDL_SCANCODE_SEMICOLON,";"},{SDL_SCANCODE_APOSTROPHE,"'"},{SDL_SCANCODE_COMMA,","},
    {SDL_SCANCODE_PERIOD,"."},{SDL_SCANCODE_SLASH,"/"},
    {SDL_SCANCODE_INSERT,"Insert"},{SDL_SCANCODE_DELETE,"Delete"},{SDL_SCANCODE_HOME,"Home"},
    {SDL_SCANCODE_END,"End"},{SDL_SCANCODE_PAGEUP,"Page Up"},{SDL_SCANCODE_PAGEDOWN,"Page Down"},
    {SDL_SCANCODE_CAPSLOCK,"Caps Lock"},{SDL_SCANCODE_NUMLOCKCLEAR,"Num Lock"},
    {SDL_SCANCODE_SCROLLLOCK,"Scroll Lock"},{SDL_SCANCODE_PAUSE,"Pause"},
    {SDL_SCANCODE_KP_0,"Num 0"},{SDL_SCANCODE_KP_1,"Num 1"},{SDL_SCANCODE_KP_2,"Num 2"},
    {SDL_SCANCODE_KP_3,"Num 3"},{SDL_SCANCODE_KP_4,"Num 4"},{SDL_SCANCODE_KP_5,"Num 5"},
    {SDL_SCANCODE_KP_6,"Num 6"},{SDL_SCANCODE_KP_7,"Num 7"},{SDL_SCANCODE_KP_8,"Num 8"},
    {SDL_SCANCODE_KP_9,"Num 9"},{SDL_SCANCODE_KP_DIVIDE,"Num /"},{SDL_SCANCODE_KP_MULTIPLY,"Num *"},
    {SDL_SCANCODE_KP_MINUS,"Num -"},{SDL_SCANCODE_KP_PLUS,"Num +"},
    {SDL_SCANCODE_KP_ENTER,"Num Enter"},{SDL_SCANCODE_KP_PERIOD,"Num ."}
};
static NSString *keyboardTitle(int code) {
    for(const auto &key:touchKeyboardKeys)if(key.code==code)return @(key.title);
    return nil;
}
static constexpr NSUInteger customButtonLimit=32;
static bool validCustomID(id value) {
    if(![value isKindOfClass:NSString.class] || ![value hasPrefix:@"custom."] || [value length]!=43)return false;
    return [[NSUUID alloc] initWithUUIDString:[value substringFromIndex:7]]!=nil;
}
static NSDictionary *customDefinition(NSString *name,NSArray *definitions) {
    for(NSDictionary *entry in definitions)if([entry[@"id"] isEqual:name])return entry;
    return nil;
}
static CGRect customDefaultRect(NSUInteger index) {
    const CGFloat size=viewSize.height<500?46:64;
    const CGFloat spacing=size+12;
    const NSUInteger columns=std::max<NSUInteger>(1,(NSUInteger)std::max<CGFloat>(1,std::floor((viewSize.width*.46-size)/spacing)+1));
    return CGRectMake(viewSize.width*.52+(index%columns)*spacing,viewSize.height*(viewSize.height<500?.25:.55)+(index/columns)*spacing,size,size);
}
