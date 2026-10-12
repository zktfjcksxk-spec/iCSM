#!/usr/bin/env python3
"""Keep iOS gamepad profiles separate from UIKit's keyboard/remote events."""
from pathlib import Path
import json

C = Path.home() / 'Library/Caches/csgo-ipados-arm64/i5'
BASE = C.parent / 'i4'


def once(text, old, new):
    assert text.count(old) == 1, old[:100]
    return text.replace(old, new, 1)


def apply():
    relative = 'sdl/src/video/uikit/SDL_uikitviewcontroller.h'
    text = (BASE / relative).read_text()
    text = once(text, '''#if TARGET_OS_TV
#import <GameController/GameController.h>
#define SDLRootViewController GCEventViewController
#else
#define SDLRootViewController UIViewController
#endif''', '''/* iCSM: GCController owns gamepad input on iOS as well as tvOS.
   GCEventViewController suppresses the duplicate UIKit responder events. */
#import <GameController/GameController.h>
#define SDLRootViewController GCEventViewController''')
    (C / relative).write_text('#pragma once\n' + text)
    # SDL_uikitwindow.h also imports this header through its original path.
    # Redirect both spellings to one FileEntry without changing the baseline.
    (C / 'sdl/controller-header-overlay.json').write_text(json.dumps({
        'version': 0, 'case-sensitive': True, 'roots': [{
            'type': 'file', 'name': str(BASE / relative),
            'external-contents': str(C / relative)}]}) + '\n')

    path = C / 'sdl/src/video/uikit/SDL_uikitviewcontroller.m'
    text = path.read_text()
    marker = '/* iCSM gamepad responder isolation */'
    if marker not in text:
        text = once(text, '        self.window = _window;', '''        self.window = _window;
#if !TARGET_OS_TV
        /* iCSM gamepad responder isolation */
        self.controllerUserInteractionEnabled = NO;
#endif''')
        path.write_text(text)

    path = C / 'sdl/src/video/uikit/SDL_uikitview.m'
    text = path.read_text()
    marker = '/* iCSM: only physical keyboard presses become keyboard keys. */'
    if marker not in text:
        text = once(text, '''#ifndef SDL_JOYSTICK_DISABLED
    /* Presses from Apple TV remote */''', '''#if !TARGET_OS_TV
    /* iCSM: only physical keyboard presses become keyboard keys. */
    /* UIPress.key is nil for gamepads. Their axes/buttons already arrive
       through SDL GameController; do not also synthesize arrows/Enter/Esc. */
    static unsigned ignoredControllerPresses = 0;
    if (ignoredControllerPresses < 32) {
        ++ignoredControllerPresses;
        fprintf(stderr, "ICSM_CONTROLLER_UI_PRESS_IGNORED type=%ld phase=%ld\\n",
                (long)press.type, (long)press.phase);
    }
    return SDL_SCANCODE_UNKNOWN;
#endif

#ifndef SDL_JOYSTICK_DISABLED
    /* Presses from Apple TV remote */''')
        path.write_text(text)


if __name__ == '__main__':
    apply()
