#!/usr/bin/env python3
"""Exercise the adapted Source Unbind path against shared keyboard/pad actions."""
from pathlib import Path
import re
import subprocess
import tempfile

SOURCE = Path.home() / 'Library/Caches/csgo-ipados-arm64/i5/source/game/client/cstrike15/panorama/popups/csgo_settings_keybinder.cpp'
text = SOURCE.read_text()


def function(name):
    match = re.search(r'^(?:static )?(?:bool|void) ' + re.escape(name) + r'\(', text, re.M)
    start, end = match.start(), text.index('{', match.start()) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


fixture = r'''
#include <cassert>
#include <cstring>
#include <strings.h>
#include <map>
#include <string>
#include <cstdio>
enum ButtonCode_t { KEY_NONE=0, MOUSE_LEFT=1, KEY_SPACE=2, JOYSTICK_FIRST=100,
    PAD_A=100, PAD_B, PAD_X, PAD_Y, JOYSTICK_LAST=199 };
int V_strcmp(const char *a,const char *b){return strcmp(a,b);}
int V_stricmp(const char *a,const char *b){return strcasecmp(a,b);}
namespace panorama {
struct IUIPanel {const char *id;IUIPanel *parent=nullptr;
    IUIPanel *GetParent(){return parent;}const char *GetID(){return id;}};
}
struct GameUI {
    std::map<ButtonCode_t,std::string> binds;
    const char *GetBindingForButtonCode(ButtonCode_t code){auto it=binds.find(code);return it==binds.end()?nullptr:it->second.c_str();}
    ButtonCode_t GetButtonCodeForBind(const char *action){for(auto &it:binds)if(it.second==action)return it.first;return KEY_NONE;}
} gameui;
GameUI *gameuifuncs=&gameui;
void UnbindKey(ButtonCode_t code){gameui.binds.erase(code);}
struct CCSGO_SettingsKeyBinder {
    panorama::IUIPanel *panel;const char *m_pszBindName;
    panorama::IUIPanel *UIPanel(){return panel;}
    void Unbind();
};
'''
cases = r'''
int main(){
    panorama::IUIPanel controller{"ControllerSettings"}, keyboard{"KeybdMouseSettings"};
    panorama::IUIPanel group{"BindingGroup",&controller}, child{"CtrlrFireBinder",&group};
    CCSGO_SettingsKeyBinder binder{&child,"+attack"};
    gameui.binds={{MOUSE_LEFT,"+attack"},{KEY_SPACE,"+jump"},{PAD_A,"+attack"},{PAD_B,"+attack"},{PAD_X,"+use"}};
    binder.Unbind();
    assert(gameui.binds[MOUSE_LEFT]=="+attack" && gameui.binds[KEY_SPACE]=="+jump");
    assert(!gameui.binds.count(PAD_A) && !gameui.binds.count(PAD_B) && gameui.binds[PAD_X]=="+use");
    gameui.binds[PAD_Y]="toggle_duck";binder.m_pszBindName="+duck";binder.Unbind();
    assert(!gameui.binds.count(PAD_Y));
    gameui.binds[PAD_Y]="toggle_zoom";binder.m_pszBindName="+zoom";binder.Unbind();
    assert(!gameui.binds.count(PAD_Y));
    // Keyboard controls retain their original one-key replacement behavior.
    child.parent=&keyboard;binder.m_pszBindName="+attack";gameui.binds[PAD_A]="+attack";
    binder.Unbind();assert(!gameui.binds.count(MOUSE_LEFT) && gameui.binds[PAD_A]=="+attack");
    puts("PASS: controller rebinding preserves keyboard/mouse, removes duplicate pad bindings, handles original duck/zoom aliases; keyboard rebinding unchanged");
}
'''
with tempfile.TemporaryDirectory(prefix='icsm-controller-bindings-') as directory:
    cpp, binary = Path(directory) / 'bindings.cpp', Path(directory) / 'bindings'
    cpp.write_text(fixture + '\n'.join(function(name) for name in (
        'ICSMControllerBinder', 'ActionsAreTheSame', 'CCSGO_SettingsKeyBinder::Unbind')) + cases)
    subprocess.run(['xcrun', 'clang++', '-std=c++17', '-fsanitize=address,undefined',
                    str(cpp), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
