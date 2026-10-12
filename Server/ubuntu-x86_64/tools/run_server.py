#!/usr/bin/env python3
"""Validate local settings, then exec the original dedicated entry."""
import ipaddress
import json
import os
import re
import sys
from pathlib import Path

root = Path(__file__).resolve().parent
ROTATION_MAPS = ('de_mirage', 'de_inferno', 'de_dust2', 'de_vertigo')
MAP_GROUP = 'mg_icsm_competitive'
try:
    settings = json.loads((root / 'server-settings.json').read_text())
    bind = str(ipaddress.IPv4Address(settings['bind']))
    password = settings['password']
    if not isinstance(password, str) or len(password) < 8 or any(ord(c) < 32 for c in password):
        raise ValueError('请先在 server-settings.json 中设置至少 8 个字符的房间密码。')
    selected_map = settings['map']
    if not re.fullmatch(r'[A-Za-z0-9_/]+', selected_map) or '..' in selected_map:
        raise ValueError('地图名称无效。')
    if selected_map not in ROTATION_MAPS:
        raise ValueError('仅支持四图经典竞技：de_mirage、de_inferno、de_dust2、de_vertigo。')
    for rotation_map in ROTATION_MAPS:
        if not (root / 'content/csgo/maps' / (rotation_map + '.bsp')).is_file():
            raise ValueError('轮换地图文件不存在：' + rotation_map)
    if type(settings['game_type']) is not int or type(settings['game_mode']) is not int or (settings['game_type'], settings['game_mode']) != (0, 1):
        raise ValueError('此服务端固定为经典竞技：game_type=0、game_mode=1。')
    for name in ('csgo/gamemodes_server.txt', 'csgo/mapcycle_icsm.txt', 'csgo/cfg/icsm_rotation.cfg'):
        if not (root / name).is_file():
            raise ValueError('缺少四图轮换配置：' + name)
    bounds = {'port': (1, 65535), 'maxplayers': (2, 64), 'tickrate': (16, 128), 'game_type': (0, 6), 'game_mode': (0, 3)}
    numbers = {}
    for key, (low, high) in bounds.items():
        value = settings[key]
        if type(value) is not int or not low <= value <= high:
            raise ValueError('设置超出范围：' + key)
        numbers[key] = str(value)
    # Use a writable overlay so original shared content remains intact.
    escaped = password.replace('\\', '\\\\').replace('"', '\\"')
    cfg = root / 'csgo/cfg/icsm_password.cfg'
    cfg.write_text('sv_password "' + escaped + '"\n')
    cfg.chmod(0o600)
    (root / 'logs').mkdir(exist_ok=True)
except (KeyError, ValueError, OSError) as error:
    print(str(error), file=sys.stderr)
    sys.exit(1)
os.chdir(root)
os.environ['LD_LIBRARY_PATH'] = str(root / 'bin') + (':' + os.environ['LD_LIBRARY_PATH'] if os.environ.get('LD_LIBRARY_PATH') else '')
argv = [str(root / 'csgo_dedicated'), '-basedir', str(root), '-game', 'csgo', '-console', '-insecure', '-nomaster', '-nohltv', '-nobreakpad',
        '-ip', bind, '-port', numbers['port'], '-tickrate', numbers['tickrate'], '-maxplayers_override', numbers['maxplayers'],
        '+game_type', numbers['game_type'], '+game_mode', numbers['game_mode'], '+mapgroup', MAP_GROUP,
        '+map', selected_map, '+exec', 'icsm_server.cfg']
if sys.argv[1:] == ['--check-config']:
    print('房间设置与四图轮换配置检查通过：经典竞技，荒漠迷城 → 炼狱小镇 → Dust II → 殒命大厦 → 荒漠迷城；未启动服务端。')
    sys.exit(0)
if sys.argv[1:]:
    print('支持的参数：--check-config', file=sys.stderr)
    sys.exit(1)
os.execv(argv[0], argv)
