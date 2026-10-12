#!/usr/bin/env python3
"""Portable local source build, personal signing, IPA export and device install."""
import argparse
import concurrent.futures
import json
import os
from pathlib import Path
import plistlib
import shutil
import subprocess
import sys
import venv
import zipfile
import hashlib
import shlex
import re

ROOT = Path(__file__).resolve().parent
BUILD = ROOT / 'Build'
SETTINGS = ROOT / 'config.local.json'
# Signing must happen outside cloud-synced Documents/Desktop: those folders
# can reapply Finder metadata after Xcode's cleanup script has run.
APP_BUILD = Path.home() / 'Library/Caches/iCSM-build' / hashlib.sha256(str(ROOT).encode()).hexdigest()[:16] / 'app'


def run(args, **kwargs):
    return subprocess.run([str(a) for a in args], check=True, **kwargs)


def output(args):
    return subprocess.check_output(args, text=True).strip()


def tools():
    environment = BUILD / 'tools'
    if not (environment / 'bin/cmake').exists():
        environment.parent.mkdir(parents=True, exist_ok=True)
        venv.EnvBuilder(with_pip=True).create(environment)
        run([environment / 'bin/python', '-m', 'pip', 'install', 'cmake==3.31.6', 'ninja==1.11.1.3'])
    return environment / 'bin'


def settings():
    if not SETTINGS.exists():
        raise SystemExit('Run ./icsm setup --team YOUR_TEAM --bundle-id YOUR_UNIQUE_ID first.')
    return json.loads(SETTINGS.read_text())


def native(jobs):
    plan = json.loads((ROOT / 'native-build-plan.json').read_text())
    if len({item['output'] for item in plan['jobs']}) != len(plan['jobs']):
        raise RuntimeError('Native build plan contains multiple producers for the same output.')
    sdk = output(['xcrun', '--sdk', 'iphoneos', '--show-sdk-path'])
    tool = str(Path(output(['xcrun', '--sdk', 'iphoneos', '--find', 'clang'])).parent)
    if '27.' not in output(['xcrun', '--sdk', 'iphoneos', '--show-sdk-version']):
        raise SystemExit('This release requires the iPhoneOS 27.x SDK.')
    mappings = {'$ROOT': str(ROOT), '$SDK': sdk, '$TOOL': tool}
    def expand(value):
        for key, replacement in mappings.items():
            value = value.replace(key, replacement)
        return value
    for level in ['i1', 'i4', 'i5']:
        directory = BUILD / 'native' / level / 'Frameworks';directory.mkdir(parents=True, exist_ok=True)
        for template in (ROOT / 'FrameworkTemplates').glob('*.framework'):
            shutil.copytree(template, directory / template.name, dirs_exist_ok=True)
    for item in plan['jobs']:
        item['argv'] = [expand(arg) for arg in item['argv']]
        item['output'] = expand(item['output'])
        item['dependencies'] = [expand(arg) for arg in item['dependencies']]
        if item.get('post'):
            item['post'] = [expand(arg) for arg in item['post']]
    logdir = BUILD / 'native/logs';logdir.mkdir(parents=True, exist_ok=True)
    outputs = {item['output']: item for item in plan['jobs']}
    done = set()
    def execute(item):
        target = Path(item['output']);target.parent.mkdir(parents=True, exist_ok=True)
        signature=hashlib.sha256(json.dumps(item['argv']).encode()).hexdigest()
        marker=target.with_suffix(target.suffix+'.command')
        if item['kind']=='compile' and target.exists() and marker.exists() and marker.read_text()==signature:
            depfile=Path(str(target)+'.d')
            if depfile.exists():
                text=depfile.read_text().replace('\\\n',' ').partition(': ')[2]
                dependencies=shlex.split(text) if any(char in text for char in '\\"\'') else text.split()
                modified=target.stat().st_mtime_ns
                try:
                    if dependencies and all(Path(name).stat().st_mtime_ns<=modified for name in dependencies):
                        return item['output']
                except OSError:
                    pass  # A missing dependency requires recompilation.
        log = logdir / (target.name + '-' + str(abs(hash(item['output']))) + '.log')
        with log.open('w') as stream:
            # An existing archive must be replaced, not appended with ar qc.
            if item['kind'] == 'archive':
                target.unlink(missing_ok=True)
            try:
                run(item['argv'], cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT)
                if item.get('post'):
                    run(item['post'], cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT)
                marker.write_text(signature)
            except subprocess.CalledProcessError:
                print('\n'.join(log.read_text(errors='replace').splitlines()[-60:]), file=sys.stderr, flush=True)
                raise RuntimeError('Compilation failed; see ' + str(log))
        return item['output']
    pending = list(plan['jobs'])
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as pool:
        active = {}
        while pending or active:
            ready = [item for item in pending if all(dep in done for dep in item['dependencies'])]
            for item in ready[:max(0, jobs-len(active))]:
                pending.remove(item);active[pool.submit(execute,item)] = item
            if not active:
                raise RuntimeError('Unresolved build dependencies: ' + str([x['output'] for x in pending[:3]]))
            finished, _ = concurrent.futures.wait(active, return_when=concurrent.futures.FIRST_COMPLETED)
            for future in finished:
                done.add(future.result());del active[future]
            if len(done) % 100 < len(finished) or not pending:
                print(f'Native source build: {len(done)}/{len(plan["jobs"])}', flush=True)
    destination = BUILD / 'Frameworks';destination.mkdir(exist_ok=True)
    for name in plan['frameworks']:
        candidates = [BUILD / 'native' / level / 'Frameworks' / name for level in ['i5','i4','i1']]
        source = next((path for path in candidates if (path / path.stem).exists()), None)
        if source is None:
            raise RuntimeError('Framework was not compiled: ' + name)
        shutil.copytree(source, destination / name, dirs_exist_ok=True)
    (BUILD / 'native-complete.json').write_text(json.dumps({'jobs':len(done),'frameworks':len(plan['frameworks'])})+'\n')


def app_build(unsigned=False, bundle_id=None):
    # Cloud builds use no personal settings, account, certificate or device.
    config = {'team':'', 'bundle_id':bundle_id or 'com.icsm.client', 'device':''} if unsigned else settings()
    binaries = BUILD / 'Frameworks'
    app_build_dir = APP_BUILD.with_name('app-unsigned') if unsigned else APP_BUILD
    if not (BUILD / 'native-complete.json').exists():
        raise SystemExit('First run ./icsm modules (the game is compiled from source).')
    cmake = tools() / 'cmake'
    source = ROOT / 'Sources/workspace/ipados-arm64/app/i5'
    run([cmake, '-S', source, '-B', app_build_dir, '-G', 'Xcode',
         '-DCMAKE_SYSTEM_NAME=iOS', '-DCMAKE_OSX_SYSROOT=iphoneos',
         '-DCMAKE_OSX_ARCHITECTURES=arm64', '-DCMAKE_OSX_DEPLOYMENT_TARGET=27.0',
         '-DICSM_FRAMEWORK_DIR=' + str(binaries), '-DI5_TEAM=' + config['team'], '-DI5_BUNDLE_ID=' + config['bundle_id'],
         '-DICSM_SIGN_APP=' + ('OFF' if unsigned else 'ON')])
    command = ['xcodebuild', '-project', app_build_dir / 'CSGOI5Client.xcodeproj', '-scheme','CSGOI5Client',
         '-configuration','Release','-destination',
         'id='+config['device'] if config.get('device') else 'generic/platform=iOS']
    if unsigned:
        command += ['CODE_SIGNING_ALLOWED=NO', 'CODE_SIGNING_REQUIRED=NO', 'CODE_SIGN_IDENTITY=']
    else:
        command += ['-allowProvisioningUpdates', '-allowProvisioningDeviceRegistration']
    run(command + ['build'])
    app = app_build_dir / 'Release-iphoneos/CSGOI5Client.app'
    if unsigned:
        # Ad-hoc signatures are transport signatures, not device provisioning.
        # Retain the memory entitlement for the user's final signing tool.
        for framework in sorted((app / 'Frameworks').glob('*.framework')):
            run(['codesign', '--force', '--sign', '-', framework])
        entitlements = source.parent / 'i0/Memory.entitlements'
        run(['codesign', '--force', '--sign', '-', '--entitlements', entitlements, app])
        shutil.copyfile(entitlements, BUILD / 'iCSM.entitlements')
        if list(app.rglob('*.mobileprovision')):
            raise RuntimeError('An unsigned export must not contain a provisioning profile.')
    run(['codesign','--verify','--deep','--strict',app])
    archive = BUILD / ('iCSM-unsigned.ipa' if unsigned else 'iCSM.ipa')
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as ipa:
        for path in sorted(app.rglob('*')):
            if path.is_file():
                ipa.write(path, 'Payload/' + app.name + '/' + path.relative_to(app).as_posix())
    print(('IPA for personal re-signing: ' if unsigned else 'Your signed IPA: ') + str(archive), flush=True)
    return app


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=['doctor','setup','modules','build','app','install','launch'])
    parser.add_argument('--team');parser.add_argument('--bundle-id');parser.add_argument('--device')
    parser.add_argument('--jobs',type=int,default=min(6,os.cpu_count() or 4))
    parser.add_argument('--unsigned', action='store_true',
                        help='Export an ad-hoc IPA without an Apple account/profile; re-sign before installing')
    args = parser.parse_args()
    if args.unsigned and args.action not in ['build','app']:
        parser.error('--unsigned is supported only for build and app')
    if args.bundle_id and not re.fullmatch(r'[A-Za-z0-9-]+(?:\.[A-Za-z0-9-]+)+', args.bundle_id):
        parser.error('Use a reverse-domain bundle ID containing letters, digits, hyphens and dots')
    if sys.platform!='darwin':
        raise SystemExit('Build/sign locally on macOS with Xcode, or use GitHub Actions from Windows (docs/CLOUD_BUILD.en.md). Import iCSM-v2.zip through Files or Apple Devices.')
    if args.action == 'doctor':
        print('Xcode: ' + output(['xcodebuild','-version']))
        print('iOS SDK: ' + output(['xcrun','--sdk','iphoneos','--show-sdk-version']))
        run(['xcrun','devicectl','list','devices']);return
    if args.action == 'setup':
        if not args.team or not args.bundle_id or args.bundle_id == 'local.csgo.ipados.i0probe':
            parser.error('Supply your own --team and a unique --bundle-id; do not use the author’s application ID.')
        config = {'team':args.team,'bundle_id':args.bundle_id,'device':args.device or ''}
        SETTINGS.write_text(json.dumps(config,indent=2)+'\n');SETTINGS.chmod(0o600)
        tools();print('Setup complete. Sign in to your Apple account in Xcode → Settings → Accounts.');return
    if args.action in ['modules','build']:
        native(max(1,args.jobs))
    if args.action in ['build','app']:
        app_build(args.unsigned, args.bundle_id)
    if args.action in ['install','launch']:
        config = settings();device = args.device or config.get('device')
        if not device:
            parser.error('--device is required; find it with ./icsm doctor')
        if args.action == 'install':
            run(['xcrun','devicectl','device','install','app','--device',device, APP_BUILD / 'Release-iphoneos/CSGOI5Client.app'])
        else:
            run(['xcrun','devicectl','device','process','launch','--device',device,'--terminate-existing',config['bundle_id']])


if __name__ == '__main__':
    main()
