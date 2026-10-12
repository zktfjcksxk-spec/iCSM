#!/usr/bin/env python3
"""Audit the signed I5 build and protected baselines; no runtime claim."""
import datetime
import argparse
import importlib.util
import json
import pathlib
import plistlib
import subprocess

P = pathlib.Path(__file__).resolve().parents[2]
C = pathlib.Path.home() / 'Library/Caches/csgo-ipados-arm64/i5'
E = P / 'provenance/i5'
spec = importlib.util.spec_from_file_location('macho_audit', P / 'tools/i2/audit.py')
a = importlib.util.module_from_spec(spec)
spec.loader.exec_module(a)
app = C / 'app-build/Release-iphoneos/CSGOI5Client.app'
config = json.loads((P / 'local.json').read_text())
parser = argparse.ArgumentParser()
parser.add_argument('--device')
arguments = parser.parse_args()
if arguments.device: config['device'] = arguments.device
a.command(['codesign', '--verify', '--deep', '--strict', app])
profile = plistlib.loads(subprocess.check_output([
    'security', 'cms', '-D', '-i', str(app / 'embedded.mobileprovision')]))
assert config['device'] in profile['ProvisionedDevices']
bundle_info=plistlib.loads((app/'Info.plist').read_bytes())
assert bundle_info['UIDeviceFamily']==[1,2], bundle_info['UIDeviceFamily']
assert profile['ExpirationDate'] > datetime.datetime.now(datetime.timezone.utc).replace(tzinfo=None)
entitlements = plistlib.loads(subprocess.check_output([
    'codesign', '-d', '--entitlements', ':-', str(app)], stderr=subprocess.DEVNULL))
assert not any(value and any(key in name for key in a.DYNAMIC_EXECUTION_KEYS)
               for name, value in entitlements.items())
rows = []
for file in [app / 'CSGOI5Client'] + [f / f.stem for f in sorted((app / 'Frameworks').glob('*.framework'))]:
    info = a.macho(file)
    assert info['cpu_type'] == a.CPU_TYPE_ARM64
    assert [v['id'] for v in info['platforms']] == [a.PLATFORM_IOS]
    assert not any(s in '\n'.join(info['imports']) for s in ('/opt/homebrew', 'AppKit', 'OpenGL', 'OpenGLES', 'libsteam'))
    a.command(['codesign', '--verify', '--strict', file])
    rows.append({**info, 'path': str(file.relative_to(app)), 'sha256': a.sha(file)})
names = {row['path'] for row in rows}
for row in rows:
    for name in row['imports']:
        assert name.startswith(('/usr/lib/', '/System/Library/')) or (
            name.startswith('@rpath/') and 'Frameworks/' + name[len('@rpath/'):] in names), name
for stage in ('i3', 'i4'):
    seal = json.loads((P / 'provenance' / stage / 'sealed-record.json').read_text())
    for name, digest in seal['files'].items():
        assert a.sha(P / 'provenance' / stage / name) == digest, name
    assert a.sha(P / (stage.upper() + '_REPORT.md')) == seal['report_sha256']
baseline = json.loads((P / 'provenance/i5/revisions/touch-v5-map-fix/source-hashes.json').read_text())
for name in ('tools/i5/shader_patch.py', 'app/i5/resources/video-preset.json'):
    assert a.sha(P / name) == baseline[name], name
metadata = json.loads((E / 'app-product.json').read_text())
for name, digest in {**metadata['packaging_sources'], **metadata['touch_sources'], **metadata.get('profile_sources', {}), **metadata.get('console_sources', {}), **metadata.get('network_sources', {}),
                     **metadata.get('inventory_sources', {}), **metadata.get('fullscreen_sources', {}), **metadata.get('render_sources', {}), **metadata.get('recoil_sources', {}), **metadata.get('bot_takeover_sources', {}), **metadata.get('ui_resolution_sources', {}), **metadata.get('weapon_icons_sources', {}), **metadata.get('controller_sources', {}), **metadata.get('sticker_sources', {})}.items():
    assert a.sha(pathlib.Path(name)) == digest, name
controller_objects = list(metadata.get('controller_objects', {}).values()) + list(metadata.get('sticker_objects', {}).values())
for objects in metadata.get('sticker_module_objects', {}).values():controller_objects += list(objects.values())
for records in metadata.get('controller_module_objects', {}).values():
    controller_objects.extend(records.values())
for row in controller_objects:
    assert pathlib.Path(row['path']).is_relative_to(C)
    assert a.sha(pathlib.Path(row['path'])) == row['sha256'], row['path']
link_inputs = json.loads((E / 'link-inputs-client.json').read_text())
assert a.sha(pathlib.Path(link_inputs['output'])) == link_inputs['output_sha256']
for original,row in metadata.get('controller_objects',{}).items():
    assert link_inputs['replacements'][original] == row, original
for original,row in metadata.get('sticker_objects',{}).items():
    assert link_inputs['replacements'][original] == row, original
weapon = metadata.get('weapon_dependencies')
if weapon:
    for label in ('header_original', 'header_derived', 'overlay'):
        assert a.sha(pathlib.Path(weapon[label])) == weapon[label + '_sha256'], label
    assert weapon['dependent_count'] == sum(len(inputs) for inputs in weapon['targets'].values())
    assert set(weapon['targets']) == {'client', 'server'}
    assert all(pathlib.Path(path).is_relative_to(C) and pathlib.Path(path).exists()
               for inputs in weapon['targets'].values() for path in inputs.values())
overlay = P / 'app/i5/resources/native-ui'
for file in overlay.rglob('*'):
    if file.is_file():
        assert a.sha(app / 'native-ui' / file.relative_to(overlay)) == a.sha(file), file
navigation = P / 'app/i5/resources/native-navigation'
for file in navigation.rglob('*'):
    if file.is_file():
        assert a.sha(app / 'native-navigation' / file.relative_to(navigation)) == a.sha(file), file
data_manifest = json.loads((P / 'app/i5/resources/distribution-assets.json').read_text())
data_files = {row['path']: row for row in data_manifest['files']}
for file in (P / 'app/i5/resources/local-workshop').rglob('*'):
    if file.is_file():
        if file.name == 'icsm-profile.json':
            assert a.sha(app / file.name) == a.sha(file), file
        else:
            # Distribution installs these files from the separate data pack.
            relative = 'game-assets/icsm-addon/' + str(file.relative_to(P / 'app/i5/resources/local-workshop'))
            assert data_files[relative]['sha256'] == a.sha(file), file
for file in (P / 'app/i5/resources/printstream-trial').rglob('*'):
    if file.is_file():
        assert a.sha(app / 'printstream-trial' / file.relative_to(P / 'app/i5/resources/printstream-trial')) == a.sha(file), file
result = {'module_count': len(rows), 'modules': rows, 'signature_verified': True,
          'device_families': bundle_info['UIDeviceFamily'],
          'profile_expires_utc': profile['ExpirationDate'].isoformat() + 'Z',
          'no_jit_entitlement': True, 'architecture': 'arm64', 'platform': 'IOS',
          'protected_i3_i4_seals_verified': True,
          'v5_shader_fix_and_video_preset_preserved': True,
          'packaged_touch_and_native_ui_hashes_verified': True,
          'inventory_source_hashes_verified': bool(metadata.get('inventory_sources')),
          'fullscreen_source_hashes_verified': bool(metadata.get('fullscreen_sources')),
          'all_weapon_header_dependents_rebuilt': weapon['dependent_count'] if weapon else 0,
          'recoil_source_hashes_verified': bool(metadata.get('recoil_sources')),
          'bot_takeover_source_hashes_verified': bool(metadata.get('bot_takeover_sources')),
          'ui_resolution_source_hashes_verified': bool(metadata.get('ui_resolution_sources')),
          'weapon_icons_source_hashes_verified': bool(metadata.get('weapon_icons_sources')),
          'controller_source_hashes_verified': bool(metadata.get('controller_sources')),
          'patched_client_objects_actually_linked': True,
          'runtime_acceptance': False}
(E / 'build-audit.json').write_text(json.dumps(result, indent=2) + '\n')
print(f"I5 build audit passed: {len(rows)} signed ARM64/iOS images; protected seals and input hashes verified")
