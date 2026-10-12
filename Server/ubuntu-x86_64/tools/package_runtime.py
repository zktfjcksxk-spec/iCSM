#!/usr/bin/env python3
"""Assemble a Linux runtime from rebuilt ELF modules and matching mobile data."""
import argparse,hashlib,json,os,shutil,zipfile
from pathlib import Path,PurePosixPath
p=argparse.ArgumentParser();p.add_argument('--data',type=Path,required=True);p.add_argument('--build',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
root=a.output.resolve();tools=Path(__file__).resolve().parent
if root.exists():raise SystemExit('Choose a new output folder; existing server data will not be replaced.')
root.mkdir(parents=True);(root/'bin').mkdir();(root/'content/csgo').mkdir(parents=True);(root/'platform').mkdir();(root/'csgo/bin').mkdir(parents=True);(root/'csgo/cfg').mkdir();(root/'logs').mkdir()
exe=a.build/'bin/csgo_dedicated';assert exe.read_bytes()[:4]==b'\x7fELF';shutil.copy2(exe,root/exe.name)
for f in sorted((a.build/'bin').glob('*.so')):
 assert f.read_bytes()[:4]==b'\x7fELF';shutil.copy2(f,root/'bin'/f.name)
with zipfile.ZipFile(a.data) as z:
 seen=set()
 for item in z.infolist():
  name=PurePosixPath(item.filename)
  if name.is_absolute() or '..' in name.parts or '\\' in item.filename or item.filename in seen:raise ValueError('Unsafe ZIP path')
  seen.add(item.filename)
  mappings=[('game-assets/csgo/','content/csgo/'),('game-assets/platform/','platform/'),('game-assets/icsm-addon/maps/','content/csgo/maps/')]
  for old,new in mappings:
   if item.filename.startswith(old) and not item.is_dir():
    target=root/(new+item.filename[len(old):]);target.parent.mkdir(parents=True,exist_ok=True)
    with z.open(item) as src,target.open('wb') as dst:shutil.copyfileobj(src,dst,1024*1024)
    break
for name in ('start-server.sh','check-server.sh','run_server.py','README.txt','public-connect-notes.txt'):shutil.copy2(tools/name,root/name)
for name,dest in [('gamemodes_server.txt','gamemodes_server.txt'),('mapcycle_icsm.txt','mapcycle_icsm.txt'),('icsm_rotation.cfg','cfg/icsm_rotation.cfg')]:shutil.copy2(tools/name,root/'csgo'/dest)
(root/'csgo/gameinfo.txt').write_text('''"GameInfo" { game "iCSM Private Server" type multiplayer_only bots 1 FileSystem { SteamAppId 730 SearchPaths { Game |gameinfo_path|. Game |gameinfo_path|../content/csgo Game |gameinfo_path|../platform Platform |gameinfo_path|../platform Mod |gameinfo_path|. } } }\n''')
shutil.copy2(root/'content/csgo/steam.inf',root/'csgo/steam.inf')
(root/'csgo/cfg/icsm_server.cfg').write_text('''hostname "iCSM Private Server"
sv_lan 0
sv_hibernate_when_empty 0
sv_cheats 0
sv_allow_lobby_connect_only 0
mp_humanteam any
bot_join_team any
mp_autoteambalance 1
mp_limitteams 1
bot_quota_mode fill
bot_quota 10
bot_join_after_player 1
bot_difficulty 2
exec icsm_rotation.cfg
log on
exec icsm_password.cfg
''')
(root/'csgo/cfg/gamemode_competitive_server.cfg').write_text('exec icsm_server.cfg\n')
(root/'server-settings.json').write_text(json.dumps(dict(bind='0.0.0.0',port=27015,password='',maxplayers=10,tickrate=64,map='de_mirage',game_type=0,game_mode=1),indent=2)+'\n')
for f in (root/'bin').glob('*.so'):
 if f.name.startswith('lib'):
  alias=root/'bin'/f.name[3:]
  if not alias.exists():alias.symlink_to(f.name)
  game=root/'csgo/bin'/f.name
  if not game.exists():game.symlink_to('../../bin/'+f.name)
  game_alias=root/'csgo/bin'/f.name[3:]
  if not game_alias.exists():game_alias.symlink_to('../../bin/'+f.name)
for name in ('csgo_dedicated','start-server.sh','check-server.sh','run_server.py'):(root/name).chmod(0o755)
def digest(f):
 h=hashlib.sha256()
 with f.open('rb') as s:
  for b in iter(lambda:s.read(1048576),b''):h.update(b)
 return h.hexdigest()
(root/'SHA256SUMS').write_text(''.join(digest(f)+'  '+f.relative_to(root).as_posix()+'\n' for f in sorted(root.rglob('*')) if f.is_file() and not f.is_symlink() and f.name!='SHA256SUMS'))
print('Server runtime prepared; set your own password before starting:',root)
