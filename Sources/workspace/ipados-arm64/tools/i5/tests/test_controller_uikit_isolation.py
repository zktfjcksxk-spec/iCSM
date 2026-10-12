#!/usr/bin/env python3
"""Execute the actual UIKit press translator with keyboard and gamepad presses."""
from pathlib import Path
import subprocess
import tempfile

C = Path.home() / 'Library/Caches/csgo-ipados-arm64/i5/sdl/src/video/uikit'
text = (C / 'SDL_uikitview.m').read_text()
start = text.index('- (SDL_Scancode)scancodeFromPress:')
end = text.index('- (void)pressesBegan:', start)
method = text[start:end]
assert '#define SDLRootViewController GCEventViewController' in (C / 'SDL_uikitviewcontroller.h').read_text()
assert 'self.controllerUserInteractionEnabled = NO;' in (C / 'SDL_uikitviewcontroller.m').read_text()

fixture = r'''
#import <Foundation/Foundation.h>
#include <cassert>
#include <cstdio>
#include <initializer_list>
typedef int SDL_Scancode;
enum {SDL_SCANCODE_UNKNOWN=0,SDL_SCANCODE_UP=82,SDL_SCANCODE_DOWN=81,
 SDL_SCANCODE_LEFT=80,SDL_SCANCODE_RIGHT=79,SDL_SCANCODE_RETURN=40,
 SDL_SCANCODE_ESCAPE=41,SDL_SCANCODE_PAUSE=72};
enum {UIPressTypeUpArrow,UIPressTypeDownArrow,UIPressTypeLeftArrow,
 UIPressTypeRightArrow,UIPressTypeSelect,UIPressTypeMenu,UIPressTypePlayPause};
int SDL_AppleTVRemoteOpenedAsJoystick=0;
@interface UIKey:NSObject
@property NSInteger keyCode;
@end
@implementation UIKey @end
@interface UIPress:NSObject
@property UIKey *key;
@property NSInteger type;
@property NSInteger phase;
@end
@implementation UIPress @end
@interface View:NSObject
-(SDL_Scancode)scancodeFromPress:(UIPress*)press;
@end
@implementation View
'''
cases = r'''
@end
int main(){@autoreleasepool {
 View *view=[View new];UIPress *press=[UIPress new];
 // Physical keyboard codes remain literal even while a gamepad is present.
 for(int code: {4,40,41,79,80,81,82}) {
  press.key=[UIKey new];press.key.keyCode=code;
  assert([view scancodeFromPress:press]==code);
 }
 press.key=nil;
 for(int type=UIPressTypeUpArrow;type<=UIPressTypePlayPause;++type) {
  press.type=type;
#if TARGET_OS_TV
  const int expected[]={82,81,80,79,40,41,72};
  assert([view scancodeFromPress:press]==expected[type]);
#else
  // Gamepad/UI responder presses must not create duplicate gameplay keys.
  for(int phase=0;phase<=4;++phase){press.phase=phase;
   assert([view scancodeFromPress:press]==SDL_SCANCODE_UNKNOWN);}
#endif
 }
 SDL_AppleTVRemoteOpenedAsJoystick=1;
 assert([view scancodeFromPress:press]==SDL_SCANCODE_UNKNOWN);
 puts("PASS: physical keyboard preserved; iOS controller presses isolated; tvOS remote retained");
}}
'''
with tempfile.TemporaryDirectory(prefix='icsm-uikit-controller-') as directory:
    source = Path(directory) / 'press.mm'
    source.write_text(fixture + method + cases)
    for tv in (0, 1):
        binary = Path(directory) / ('press-' + str(tv))
        subprocess.run(['xcrun', 'clang++', '-std=c++17', '-fobjc-arc',
                        '-fsanitize=address,undefined', '-DTARGET_OS_TV=' + str(tv),
                        '-framework', 'Foundation', str(source), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
