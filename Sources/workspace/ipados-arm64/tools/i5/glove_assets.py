#!/usr/bin/env python3
"""Audit trial clothing inputs in the original 2019 VPK; pin original icons."""
import hashlib,importlib.util,json,re,struct,subprocess,zlib
from pathlib import Path
P=Path(__file__).resolve().parents[2];W=P.parent
def main():
 spec=importlib.util.spec_from_file_location('catalog',W/'research/skins-2019/catalog.py')
 c=importlib.util.module_from_spec(spec);spec.loader.exec_module(c)
 root=c.keyvalues(c.SCHEMA.read_text())[0][1]
 items={int(k):c.fields(v) for n,b,_ in root if n=='items' for k,v,_ in b if k.isdigit()}
 paints={int(k):c.fields(v) for n,b,_ in root if n=='paint_kits' for k,v,_ in b}
 entries,offset,index_sha=c.load_vpk();assets={}
 def read(path):
  path=path.lower().replace('\\','/');file=c.GAME/path
  if file.is_file():data=file.read_bytes();storage='loose'
  else:
   crc,preload,parts=entries[path];data=bytearray(preload)
   for archive,start,length in parts:
    file=c.GAME/(f'pak01_{archive:03d}.vpk' if archive!=0x7fff else 'pak01_dir.vpk')
    with file.open('rb') as f:f.seek(start+(offset if archive==0x7fff else 0));part=f.read(length)
    assert len(part)==length,path;data.extend(part)
   assert zlib.crc32(data)==crc,path
   storage='vpk_crc_verified';data=bytes(data)
  assets[path]={'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest(),'storage':storage}
  return data
 texture_keys={'$basetexture','$bumpmap','$masks1','$envmapmask','$materialmask','$ao','$grunge','$pattern','$detail','$detailnormal','$lightwarptexture','$phongwarptexture','$fresnelrangestexture'}
 visited=set()
 def material(path):
  path=path.lower().replace('\\','/')
  if path in visited:return
  visited.add(path);raw=read(path).decode('utf-8-sig')
  tokens=[m.group().strip('"') for m in c.LEXER.finditer(raw) if not m.group().isspace() and not m.group().startswith(('//','/*'))]
  for k,v in zip(tokens,tokens[1:]):
   if k.lower()=='include':material(v)
   if k.lower() in texture_keys and v:
    # env_cubemap is an engine-provided map cubemap, not a texture file.
    if v in ('env_cubemap','black','white'):continue
    value=v.lower().replace('\\','/');value=value if value.startswith('materials/') else 'materials/'+value
    assert read(value.removesuffix('.vtf')+'.vtf')[:4]==b'VTF\0'
 for definition in (5028,5029,5030):
  model=items[definition]['model_player'];base=model[:-4]
  assert read(model)[:4]==b'IDST'
  read(base+'.vvd');read(base+'.dx90.vtx')
 if True:
  paintdata=items[5030]['paint_data'];assert len(paintdata)==2
  for _,value,_ in paintdata:material(c.fields(value)['Name'])
 for paint,internal in ((10018,'sporty_light_blue'),(10037,'sporty_purple')):
  assert paints[paint]['name']==internal
  material(paints[paint]['vmt_path'])
 cdn={k:v for k,v in (line.split('=',1) for line in (c.GAME/'scripts/items/items_game_cdn.txt').read_text().splitlines() if '=' in line)}
 preview_spec=importlib.util.spec_from_file_location('previews',P/'tools/i5/skin_previews.py')
 previews=importlib.util.module_from_spec(preview_spec);preview_spec.loader.exec_module(previews)
 out=P/'app/i5/resources/native-ui/panorama/images/offline_skins';icons=[]
 for definition,paint,key in ((5028,0,'t_gloves'),(5029,0,'ct_gloves'),(5030,10018,'sporty_gloves_sporty_light_blue'),(5030,10037,'sporty_gloves_sporty_purple')):
  url=cdn[key];record={'definition':definition,'paint':paint,'file':f'{definition}_{paint}.png','url':url,'sha1':re.search(r'\.([0-9a-f]{40})\.png$',url).group(1)}
  path=out/record['file']
  if path.is_file():data=path.read_bytes()
  else:
   result=subprocess.run(['/usr/bin/curl','--fail','--silent','--show-error','--location','--retry','2','--max-time','40',url.replace('http:','https:',1)],capture_output=True,check=True);data=result.stdout
  checked=previews.verify(data,record);path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data);icons.append(checked)
 cache=(c.GAME/'resource/vmtcache.txt').read_text().lower();vmts=[k for k in assets if k.endswith('.vmt')]
 assert all(k in cache for k in vmts),'Original clothing recipe missing from material cache'
 report={'schema_sha256':hashlib.sha256(c.SCHEMA.read_bytes()).hexdigest(),'vpk_index_sha256':index_sha,'definitions':[5028,5029,5030],'paints':[10018,10037],'assets':assets,'icons':icons,'vmtcache_recipes_verified':vmts,'device_validation':'pending user'}
 dest=P/'provenance/i5/gloves-v168/assets.json';dest.parent.mkdir(parents=True,exist_ok=True);dest.write_text(json.dumps(report,indent=2,ensure_ascii=False)+'\n')
 print(f'PASS {len(assets)} source assets verified; {len(icons)} exact original icons verified')
if __name__=='__main__':main()
