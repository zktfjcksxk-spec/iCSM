#!/usr/bin/env python3
"""Extend the retained SDL keyboard with an independent touch source bit."""
from pathlib import Path

P = Path(__file__).resolve().parents[2]
C = Path.home() / 'Library/Caches/csgo-ipados-arm64/i5'
B = C.parent / 'i4'


def once(text, old, new):
    assert text.count(old) == 1, old[:100]
    return text.replace(old, new, 1)


def apply():
    path = C / 'sdl/src/events/SDL_keyboard.c'
    path.parent.mkdir(parents=True, exist_ok=True)
    text = (B / 'sdl/src/events/SDL_keyboard.c').read_text()
    text = once(text, '#include "SDL_hints.h"', '#include "touch_keyboard_ownership.h"\n#include "SDL_hints.h"')
    text = once(text, '            SDL_SendKeyboardKey(SDL_RELEASED, scancode);',
                '            /* Focus loss must release every source, including touch. */\n'
                '            keyboard->keysource[scancode] = KEYBOARD_HARDWARE;\n'
                '            SDL_SendKeyboardKey(SDL_RELEASED, scancode);')
    text = once(text, '        keyboard->keysource[scancode] = 0;', '''
        /* iCSM: preserve another input source while a custom touch key is
           involved. Leave the original SDL text-input behavior otherwise. */
        if ((source | keyboard->keysource[scancode]) & ICSM_KEYBOARD_TOUCH_SOURCE) {
            keyboard->keysource[scancode] = ICSMKeyboardRemainingSources(keyboard->keysource[scancode], source);
            if (keyboard->keysource[scancode]) return 0;
        } else {
            keyboard->keysource[scancode] = 0;
        }''')
    text = once(text, 'int SDL_SendVirtualKeyboardKey(Uint8 state, SDL_Scancode scancode)', '''/* iCSM custom gameplay buttons: normal SDL events and modifier tracking. */
int SDL_SendTouchKeyboardKey(Uint8 state, SDL_Scancode scancode)
{
    return SDL_SendKeyboardKeyInternal(ICSM_KEYBOARD_TOUCH_SOURCE, state, scancode, SDLK_UNKNOWN);
}

int SDL_SendVirtualKeyboardKey(Uint8 state, SDL_Scancode scancode)''')
    path.write_text(text)
    print('Independent custom touch keyboard source prepared')


if __name__ == '__main__':
    apply()
