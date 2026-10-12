#!/usr/bin/env python3
"""Generate the complete glove catalog from verified original 2019 assets."""
import argparse
import concurrent.futures
import hashlib
import importlib.util
import json
import re
import struct
import subprocess
import zlib
from pathlib import Path

P = Path(__file__).resolve().parents[2]
W = P.parent


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--verify-only', action='store_true')
    parser.add_argument('--report', type=Path, default=P / 'provenance/i5/gloves-v173/assets.json')
    args = parser.parse_args()
    spec = importlib.util.spec_from_file_location('catalog', W / 'research/skins-2019/catalog.py')
    c = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(c)
    root = c.keyvalues(c.SCHEMA.read_text())[0][1]
    # The depot has repeated items/paint_kits sections; merge every section.
    items = {int(k): c.fields(v) for n, b, _ in root if n == 'items' for k, v, _ in b if k.isdigit()}
    paints = {int(k): c.fields(v) for n, b, _ in root if n == 'paint_kits' for k, v, _ in b}
    definitions = sorted(d for d, v in items.items() if v.get('prefab') in ('hands', 'hands_paintable'))
    pairs = set()
    for n, section, _ in root:
        if n != 'alternate_icons2':
            continue
        for key, icons, _ in section:
            if key == 'weapon_icons':
                for icon, _, _ in icons:
                    icon = int(icon)
                    definition, paint = icon >> 16, (icon & 65535) >> 2
                    if definition in definitions and paint:
                        assert items[definition]['prefab'] == 'hands_paintable'
                        pairs.add((definition, paint))
    pairs = sorted(pairs)
    assert len(definitions) == 9 and len(pairs) == 48
    assert {d for d, _ in pairs} == {d for d in definitions if items[d]['prefab'] == 'hands_paintable'}
    zh, en = c.localization('csgo_schinese.txt'), c.localization('csgo_english.txt')

    def name(tag):
        key = tag.lstrip('#').lower()
        assert key in zh or key in en, tag
        return zh.get(key, en.get(key))

    entries, offset, index_sha = c.load_vpk()
    assets, payloads, visited = {}, {}, set()

    def read(path):
        path = path.lower().replace('\\', '/')
        if path in assets:
            return payloads[path]
        file = c.GAME / path
        if file.is_file():
            data, storage = file.read_bytes(), 'loose'
        else:
            crc, preload, parts = entries[path]
            data = bytearray(preload)
            for archive, start, length in parts:
                file = c.GAME / (f'pak01_{archive:03d}.vpk' if archive != 0x7fff else 'pak01_dir.vpk')
                with file.open('rb') as stream:
                    stream.seek(start + (offset if archive == 0x7fff else 0))
                    part = stream.read(length)
                assert len(part) == length, path
                data.extend(part)
            assert zlib.crc32(data) == crc, path
            data, storage = bytes(data), 'vpk_crc_verified'
        assets[path] = {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest(), 'storage': storage}
        payloads[path] = data
        return data

    texture_keys = {'$basetexture', '$bumpmap', '$masks1', '$masks2', '$envmapmask', '$materialmask',
                    '$ao', '$grunge', '$pattern', '$detail', '$detailnormal', '$lightwarptexture',
                    '$phongwarptexture', '$phongexponenttexture', '$fresnelrangestexture', '$selfillummask'}

    def material(path):
        path = path.lower().replace('\\', '/')
        if path in visited:
            return
        visited.add(path)
        raw = read(path).decode('utf-8-sig')
        tokens = [m.group().strip('"') for m in c.LEXER.finditer(raw)
                  if not m.group().isspace() and not m.group().startswith(('//', '/*'))]
        for key, value in zip(tokens, tokens[1:]):
            if key.lower() == 'include':
                material(value)
            if key.lower() in texture_keys and value and value.lower() not in ('env_cubemap', 'black', 'white'):
                texture = value.lower().replace('\\', '/')
                texture = texture if texture.startswith('materials/') else 'materials/' + texture
                assert read(texture.removesuffix('.vtf') + '.vtf')[:4] == b'VTF\0'

    models = []
    for definition in definitions:
        item = items[definition]
        model = item['model_player']
        mdl = read(model)
        assert mdl[:4] == b'IDST'
        read(model[:-4] + '.vvd')
        read(model[:-4] + '.dx90.vtx')
        texture_count, texture_offset = struct.unpack_from('<ii', mdl, 204)
        assert 0 < texture_count < 128 and 0 < texture_offset < len(mdl)
        textures = []
        for i in range(texture_count):
            entry = texture_offset + i * 64  # original mstudiotexture_t
            start = entry + struct.unpack_from('<i', mdl, entry)[0]
            textures.append(mdl[start:mdl.index(b'\0', start)].decode().rsplit('/', 1)[-1].lower())
        slots = []
        if item['prefab'] == 'hands_paintable':
            assert len(item['paint_data']) == 2
            for _, block, _ in item['paint_data']:
                path = c.fields(block)['Name']
                base = Path(path).stem.lower()
                assert base in textures, (model, base, textures)
                slots.append(textures.index(base))
                material(path)
            assert len(set(slots)) == 2
        models.append({'definition': definition, 'name': name(item['item_name']), 'model': model,
                       'material_slots': slots, 'textures': textures})
    recipes = []
    for definition, paint in pairs:
        value = paints[paint]
        material(value['vmt_path'])
        recipes.append({'definition': definition, 'paint': paint, 'name': name(value['description_tag']),
                        'internal': value['name'], 'vmt': value['vmt_path']})
    cdn_file = c.GAME / 'scripts/items/items_game_cdn.txt'
    cdn = dict(line.strip().split('=', 1) for line in cdn_file.read_text().splitlines() if '=' in line)
    spec = importlib.util.spec_from_file_location('previews', P / 'tools/i5/skin_previews.py')
    previews = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(previews)
    out = P / 'app/i5/resources/native-ui/panorama/images/offline_skins'
    out.mkdir(parents=True, exist_ok=True)

    def icon(pair):
        definition, paint = pair
        key = items[definition]['name'] + ('_' + paints[paint]['name'] if paint else '')
        url = cdn[key]
        assert url.startswith('http://media.steampowered.com/apps/730/icons/econ/')
        record = {'definition': definition, 'paint': paint, 'file': f'{definition}_{paint}.png',
                  'url': url, 'sha1': re.search(r'\.([0-9a-f]{40})\.png$', url).group(1)}
        path = out / record['file']
        if path.is_file():
            data = path.read_bytes()
        else:
            assert not args.verify_only, f'Missing icon: {path}'
            data = subprocess.run(['/usr/bin/curl', '--fail', '--silent', '--show-error', '--location',
                '--retry', '2', '--max-time', '40', url.replace('http:', 'https:', 1)],
                capture_output=True, check=True).stdout
        checked = previews.verify(data, record)
        path.write_bytes(data)
        return checked

    with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
        icons = sorted(pool.map(icon, pairs + [(d, 0) for d in definitions if items[d]['prefab'] == 'hands']),
                       key=lambda r: (r['definition'], r['paint']))
    cache = (c.GAME / 'resource/vmtcache.txt').read_text().lower()
    vmts = [k for k in assets if k.endswith('.vmt')]
    assert all(k in cache for k in vmts), 'Original clothing recipe missing from material cache'
    header = '''// Generated by tools/i5/glove_catalog.py from verified 2019 schema/VPK/CDN inputs.
#pragma once
namespace ICSMGloves {
struct CatalogEntry {int definition,paint;const char *name;};
static const int Definitions[]={''' + ','.join(map(str, definitions)) + '''};
static const CatalogEntry Catalog[]={
'''
    for r in recipes:
        header += '    {%d,%d,%s},\n' % (r['definition'], r['paint'], json.dumps(r['name'], ensure_ascii=False))
    header += '''};
inline const CatalogEntry *FindCatalog(int definition,int paint) {
    for(const auto &entry:Catalog)if(entry.definition==definition && entry.paint==paint)return &entry;
    return NULL;
}
inline int DefaultPaint(int definition) {
    for(const auto &entry:Catalog)if(entry.definition==definition)return entry.paint;
    return 0;
}
inline bool Custom(int definition) {return DefaultPaint(definition)!=0;}
}
'''
    (P / 'inventory/offline_glove_catalog.h').write_text(header)
    report = {'schema_sha256': hashlib.sha256(c.SCHEMA.read_bytes()).hexdigest(), 'vpk_index_sha256': index_sha,
              'cdn_index_sha256': hashlib.sha256(cdn_file.read_bytes()).hexdigest(), 'definitions': definitions,
              'models': models, 'recipes': recipes, 'assets': assets, 'icons': icons,
              'vmtcache_recipes_verified': vmts, 'runtime_network_required': False, 'device_validation': 'pending user'}
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2, ensure_ascii=False) + '\n')
    print(f'PASS {len(definitions)-2} glove families, {len(recipes)} skins, {len(assets)} source assets and {len(icons)} exact original icons verified')


if __name__ == '__main__':
    main()
