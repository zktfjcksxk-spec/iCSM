#!/usr/bin/env python3
"""Derive only the I5 client input TU from the sealed I4 source.

No I4 file, object, runtime or evidence is written.  The incremental builder
compiles the resulting in_main.cpp with the original I4 directory as -iquote
and ipados-arm64/touch as -I.
"""
import hashlib
import json
import pathlib


PROJECT = pathlib.Path(__file__).resolve().parents[2]
CACHE = pathlib.Path.home() / 'Library/Caches/csgo-ipados-arm64'
ORIGINAL = CACHE / 'i4/source/game/client/in_main.cpp'
TARGET = CACHE / 'i5/source/game/client/in_main.cpp'
BRIDGE = PROJECT / 'touch/touch_source_bridge.inc'


def derive():
    before = ORIGINAL.read_bytes()
    source = before.decode('utf-8', errors='surrogateescape')

    def replace_once(old, new):
        nonlocal source
        if source.count(old) != 1:
            raise RuntimeError('I4 input source anchor changed: ' + repr(old[:100]))
        source = source.replace(old, new, 1)

    replace_once('#include "tier0/memdbgon.h"', '''#if defined(SOURCE_IOS)
#include "touch_ios.h"
#include "ienginevgui.h"
#include "IGameUIFuncs.h"
#include "cdll_client_int.h"
#include "inputsystem/iinputstacksystem.h"
#include "cstrike15/c_cs_player.h"
#include "cstrike15/cs_hud_weaponselection.h"
#include "cstrike15/panorama/hud/csgo_hud.h"
#include "cstrike15/panorama/csgo_mainmenu.h"
#include "panorama/controls/movieplayer.h"
#include "panorama/controls/button.h"
#include "panorama/iuipanel.h"
#include "panorama/iuiwindow.h"
#include "materialsystem/materialsystem_config.h"
#include "videocfg/videocfg.h"
#include "vgui/ILocalize.h"
#endif
#include "tier0/memdbgon.h"''')

    replace_once('int CInput::KeyEvent( int down, ButtonCode_t code, const char *pszCurrentBinding )',
                 '#if defined(SOURCE_IOS)\n#include "touch_source_bridge.inc"'
                 '\n#endif\n\nint CInput::KeyEvent( int down, ButtonCode_t code, const char *pszCurrentBinding )')

    replace_once('\tSteamControllerMove( frametime, cmd );\n}', '''\tSteamControllerMove( frametime, cmd );
#if defined(SOURCE_IOS)
    I5Touch::TraceControllerInput(5,nSlot,cmd);
    I5Touch::ControllerMove( nSlot, cmd );
    I5Touch::TraceControllerInput(6,nSlot,cmd);
#endif
}''')
    replace_once('\tJoyStickMove( frametime, cmd);', '''#if defined(SOURCE_IOS)
    I5Touch::TraceControllerInput(3,nSlot,cmd);
#endif
\tJoyStickMove( frametime, cmd);
#if defined(SOURCE_IOS)
    I5Touch::TraceControllerInput(4,nSlot,cmd);
#endif''')
    replace_once('void CInput::ControllerMove( int nSlot, float frametime, CUserCmd *cmd )\n{', '''void CInput::ControllerMove( int nSlot, float frametime, CUserCmd *cmd )
{
#if defined(SOURCE_IOS)
    I5Touch::TraceControllerInput(2,nSlot,cmd);
#endif''')
    replace_once('void CInput::AdjustAngles ( int nSlot, float frametime )\n{', '''void CInput::AdjustAngles ( int nSlot, float frametime )
{
#if defined(SOURCE_IOS)
    I5Touch::TraceControllerInput(0,nSlot,NULL);
#endif''')
    replace_once('\tAdjustPitch( nSlot, speed, viewangles );', '''\tAdjustPitch( nSlot, speed, viewangles );
#if defined(SOURCE_IOS)
    static double nextKeyAngleTrace=0;
    if(I5Touch::ICSMControllerTraceActive() && Plat_FloatTime()>=nextKeyAngleTrace) {
        nextKeyAngleTrace=Plat_FloatTime()+0.1;
        Msg("ICSM_CONTROLLER_KEY_ANGLE yaw=%g pitch=%g\\n",viewangles[YAW],viewangles[PITCH]);
    }
#endif''')

    # ControllerMove precedes GetButtonBits in both paths.  Direction bits are
    # merged after that assignment, while action edges enter original kbuttons.
    replace_once('\tcmd->buttons = GetButtonBits( false );\n#endif', '''\tcmd->buttons = GetButtonBits( false );
#endif
#if defined(SOURCE_IOS)
    I5Touch::MergeDirectionBits( cmd );
#endif''')
    replace_once('\tcmd->buttons = GetButtonBits( true );\n#endif', '''\tcmd->buttons = GetButtonBits( true );
#endif
#if defined(SOURCE_IOS)
    I5Touch::MergeDirectionBits( cmd );
#endif''')

    replace_once('\tpVerified->m_cmd = *cmd;', '''#if defined(SOURCE_IOS)
    I5Touch::RecordCommand( cmd, true );
#endif
\tpVerified->m_cmd = *cmd;''')
    replace_once('\tCheckSplitScreenMimic( nSlot, cmd, &dummy[ 0 ] );\n}', '''\tCheckSplitScreenMimic( nSlot, cmd, &dummy[ 0 ] );
#if defined(SOURCE_IOS)
    I5Touch::RecordCommand( cmd, false );
#endif
}''')

    replace_once('\tInit_Camera();\n}', '''\tInit_Camera();
#if defined(SOURCE_IOS)
    I5Touch::Initialize();
#endif
}''')
    replace_once('void CInput::Shutdown_All(void)\n{\n\tDeactivateMouse();', '''void CInput::Shutdown_All(void)
{
#if defined(SOURCE_IOS)
    I5Touch::Shutdown();
#endif
\tDeactivateMouse();''')

    TARGET.parent.mkdir(parents=True, exist_ok=True)
    result = source.encode('utf-8', errors='surrogateescape')
    if not TARGET.exists() or TARGET.read_bytes() != result:
        TARGET.write_bytes(result)
    if ORIGINAL.read_bytes() != before:
        raise RuntimeError('Protected I4 input changed during derivation')
    return {
        'original': str(ORIGINAL), 'original_sha256': hashlib.sha256(before).hexdigest(),
        'derived': str(TARGET), 'derived_sha256': hashlib.sha256(result).hexdigest(),
        'bridge': str(BRIDGE), 'bridge_sha256': hashlib.sha256(BRIDGE.read_bytes()).hexdigest(),
    }


if __name__ == '__main__':
    print(json.dumps(derive(), ensure_ascii=False, indent=2))
