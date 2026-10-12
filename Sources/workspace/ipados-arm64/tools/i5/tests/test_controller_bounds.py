#!/usr/bin/env python3
"""Run retained Panorama button loops with modern SDL under ASan/UBSan."""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path.home() / 'Library/Caches/csgo-ipados-arm64'
SDL = Path(__file__).resolve().parents[3] / 'vendor/SDL2-2.32.10/include'


def extract(text):
    mapping = re.search(r'static const int s_nSDLJoyButtonMapping\[\] =\s*\{.*?\};', text, re.S).group()
    names = re.findall(r'^\s*(XK_\w+)', mapping, re.M)
    loops = []
    for match in re.finditer(r'for \( int sdlbutton = 0; sdlbutton < .*?sdlbutton\+\+ \)', text):
        start = text.index('{', match.end()) + 1
        depth, end = 1, start
        while depth:
            depth += (text[end] == '{') - (text[end] == '}')
            end += 1
        # ReadXDevice's key-state update is sufficient to exercise its table
        # lookup and destination write. Focus uses the complete original loop.
        loop = text[match.start():end]
        if 'int i = ' in loop:
            loop = loop[:loop.index('\n\t\t// ignore the key')] + '\n}'
        loops.append(loop)
    assert len(names) == 15 and len(loops) == 2
    prelude = r'''
#include <SDL_gamecontroller.h>
#include <algorithm>
#include <cassert>
#include <cstdio>
#define MIN(a,b) std::min(a,b)
#define ARRAYSIZE(a) (sizeof(a)/sizeof(a[0]))
struct Key {bool m_bSuppressUntilUp=true; int repeats=0;};
struct Device {Key m_appXKeys[32];};
int buttonDown=-1;
int GetButtonValue(Device *,SDL_GameControllerButton button){return button==buttonDown;}
'''
    return prelude + 'enum {' + ','.join(names) + '};\n' + mapping + '\n' + '\n'.join(
        'void loop' + str(i) + '(Device *pXDevice){' + loop + '}' for i, loop in enumerate(loops)) + r'''
int main(){
    Device device;
    // Include newer buttons, all original buttons, and the all-up state.
    for(buttonDown=-1;buttonDown<SDL_CONTROLLER_BUTTON_MAX;++buttonDown){
        loop0(&device);loop1(&device);
        for(int i=0;i<15;++i)assert(device.m_appXKeys[i].m_bSuppressUntilUp==(i==buttonDown));
        for(int i=15;i<32;++i)assert(device.m_appXKeys[i].m_bSuppressUntilUp);
    }
    printf("PASS: both Panorama loops safely handle all %d SDL button values with a 15-entry mapping\n",SDL_CONTROLLER_BUTTON_MAX);
}
'''


with tempfile.TemporaryDirectory(prefix='icsm-controller-bounds-') as d:
    d = Path(d)
    for stage in ('i4', 'i5'):
        cpp, binary = d / (stage + '.cpp'), d / stage
        cpp.write_text(extract((ROOT / stage / 'source/panorama/input/controller.cpp').read_text()))
        subprocess.run(['xcrun', 'clang++', '-std=c++17', '-fsanitize=address,undefined', '-I'+str(SDL), str(cpp), '-o', str(binary)], check=True)
        result = subprocess.run([str(binary)], text=True, capture_output=True)
        if stage == 'i4':
            assert result.returncode and ('global-buffer-overflow' in result.stderr or 'out of bounds' in result.stderr), result.stderr
            print('CONFIRMED: original 2019 loops access past the 15-entry table with current SDL')
        else:
            assert result.returncode == 0, result.stderr
            print(result.stdout.strip())
