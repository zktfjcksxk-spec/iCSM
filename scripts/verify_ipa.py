#!/usr/bin/env python3
"""Validate the exported app, its iOS binaries and bundled runtime dependencies."""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import plistlib
import struct
import zipfile

ROOT = Path(__file__).resolve().parent.parent


def require(condition, message):
    if not condition:
        raise ValueError(message)


def macho(data, name):
    require(len(data) >= 32, f'Truncated executable: {name}')
    header = struct.unpack_from('<8I', data)
    require(header[0] == 0xfeedfacf and header[1] == 0x0100000c,
            f'Expected a native ARM64 Mach-O executable: {name}')
    require(header[3] in (2, 6), f'Expected executable or dynamic library: {name}')
    offset = 32
    dependencies = []
    platform = None
    for _ in range(header[4]):
        require(offset + 8 <= len(data), f'Truncated load command: {name}')
        command, size = struct.unpack_from('<2I', data, offset)
        require(size >= 8 and offset + size <= len(data), f'Invalid load command: {name}')
        if command == 0x32:  # LC_BUILD_VERSION, from Apple's mach-o/loader.h.
            platform = struct.unpack_from('<I', data, offset + 8)[0]
        if command in (0xc, 0x80000018, 0x8000001f, 0x80000023):
            start = struct.unpack_from('<I', data, offset + 8)[0]
            require(start < size, f'Invalid dynamic library name: {name}')
            dependencies.append(data[offset + start:offset + size].split(b'\0')[0].decode())
        offset += size
    require(platform == 2, f'Expected the iOS device platform, not simulator/macOS: {name}')
    return dependencies


def verify(ipa, unsigned=False, data_manifest=None):
    expected = set(json.loads((ROOT / 'native-build-plan.json').read_text())['frameworks'])
    with zipfile.ZipFile(ipa) as archive:
        names = archive.namelist()
        require(len(names) == len(set(names)), 'Duplicate ZIP members')
        for name in names:
            path = PurePosixPath(name)
            require(not path.is_absolute() and '..' not in path.parts, 'Unsafe ZIP path: ' + name)
            require(name.startswith('Payload/'), 'Unexpected IPA content: ' + name)
        apps = {PurePosixPath(name).parts[1] for name in names if len(PurePosixPath(name).parts) >= 2}
        require(len(apps) == 1 and next(iter(apps)).endswith('.app'), 'Expected one app in Payload')
        base = 'Payload/' + next(iter(apps)) + '/'
        info = plistlib.loads(archive.read(base + 'Info.plist'))
        require(info['CFBundleDisplayName'] == 'iCSM', 'Incorrect app display name')
        require(info.get('UIFileSharingEnabled') is True, 'Game data file sharing is disabled')
        require(float(info['MinimumOSVersion']) >= 27, 'Incorrect deployment target')
        require(info.get('CFBundleSupportedPlatforms') == ['iPhoneOS'], 'Incorrect app platform')
        require(set(info.get('UIDeviceFamily', [])) == {1, 2}, 'iPhone/iPad support missing')
        frameworks = {PurePosixPath(name[len(base):]).parts[1] for name in names
                      if name.startswith(base + 'Frameworks/') and len(PurePosixPath(name[len(base):]).parts) > 2}
        require(frameworks == expected, f'Frameworks differ: missing={expected-frameworks}, extra={frameworks-expected}')
        executables = [base + info['CFBundleExecutable']]
        embedded = set()
        for framework in sorted(frameworks):
            prefix = base + 'Frameworks/' + framework + '/'
            metadata = plistlib.loads(archive.read(prefix + 'Info.plist'))
            executable = framework + '/' + metadata['CFBundleExecutable']
            embedded.add(executable)
            executables.append(base + 'Frameworks/' + executable)
        for executable in executables:
            for dependency in macho(archive.read(executable), executable):
                if dependency.startswith('@rpath/'):
                    require(dependency[len('@rpath/'):] in embedded,
                            f'Missing bundled dependency in {executable}: {dependency}')
                else:
                    require(dependency.startswith(('/System/Library/', '/usr/lib/')),
                            f'Nonportable dependency in {executable}: {dependency}')
        for resource in ['original-assets.json', 'distribution-assets.json', 'fresh-video-preset.json',
                         'fresh-client-defaults.cfg', 'fresh-video.kv', 'LaunchScreen.storyboardc/Info.plist',
                         'Assets.car', 'native-ui/panorama/layout/mainmenu.xml',
                         'native-ui/panorama/scripts/inventory_offline.js']:
            require(base + resource in names, 'Missing app resource: ' + resource)
        distribution = json.loads(archive.read(base + 'distribution-assets.json'))
        if data_manifest:
            require(distribution == json.loads(data_manifest.read_text()),
                    'IPA embeds a different data manifest; app and data must come from the same build')
        require(archive.testzip() is None, 'ZIP CRC validation failed')
        if unsigned:
            require(not any(name.endswith(('.mobileprovision', '.p12', '.pfx', '.keychain')) for name in names),
                    'Personal signing material found in the cloud export')
        require(not any('/game-assets/' in name or name.endswith(('iCSM-Data.zip', 'iCSM-v1.zip', 'iCSM-v2.zip')) for name in names),
                'Game data must be imported separately')
    with ipa.open('rb') as stream:
        digest = hashlib.file_digest(stream, 'sha256').hexdigest() if hasattr(hashlib, 'file_digest') else None
    if digest is None:
        digest = hashlib.sha256(ipa.read_bytes()).hexdigest()
    result = {'ipa': ipa.name, 'sha256': digest, 'bytes': ipa.stat().st_size,
              'version': info['CFBundleShortVersionString'], 'build': info['CFBundleVersion'],
              'bundle_id': info['CFBundleIdentifier'], 'frameworks': len(frameworks),
              'verified_arm64_ios_binaries': len(executables),
              'signing': 'ad-hoc; personal re-signing required' if unsigned else 'local Apple signing',
              'data_identity': distribution['identity'], 'shader_count': distribution['shader_count'],
              'source_commit': os.environ.get('GITHUB_SHA', '')}
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('ipa', type=Path)
    parser.add_argument('--unsigned', action='store_true')
    parser.add_argument('--report', type=Path)
    parser.add_argument('--data-manifest', type=Path)
    args = parser.parse_args()
    report = verify(args.ipa, args.unsigned, args.data_manifest)
    text = json.dumps(report, indent=2) + '\n'
    if args.report:
        args.report.write_text(text)
    print(text, end='')
