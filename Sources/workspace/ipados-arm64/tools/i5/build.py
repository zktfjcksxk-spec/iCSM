#!/usr/bin/env python3
"""Build the touch trial using immutable I4 objects and separate I5 outputs."""
import argparse
import datetime
import concurrent.futures
import hashlib
import importlib.util
import json
import pathlib
import plistlib
import shlex
import shutil
import subprocess
import sys

P = pathlib.Path(__file__).resolve().parents[2]
W = P.parent
C = pathlib.Path.home() / 'Library/Caches/csgo-ipados-arm64/i5'
BASE = C.parent / 'i4'
OLD_BUILD = BASE / 'client-build'
E = P / 'provenance/i5'
PRIVATE = E / 'private'
PYTHON = W / 'macos-arm64/.tools/bin/python'
CMAKE = W / 'macos-arm64/.tools/bin/cmake'
NINJA = W / 'macos-arm64/.tools/bin/ninja'
TOUCH = P / 'touch'
INVENTORY = P / 'inventory'


def digest(path):
    h = hashlib.sha256()
    with pathlib.Path(path).open('rb') as source:
        for block in iter(lambda: source.read(1048576), b''):
            h.update(block)
    return h.hexdigest()


def inventory_source_hashes():
    # The dedicated receiver is built and audited by macos-server. It is not
    # linked into iOS client/listen modules, so keep target provenance separate.
    return {str(p): digest(p) for p in sorted(INVENTORY.glob('*'))
            if p.is_file() and not p.name.startswith('network_inventory_server.')}

def touch_source_hashes():
    inputs = [p for p in sorted(TOUCH.iterdir()) if p.is_file() and p.suffix in ('.h', '.inc', '.mm')]
    inputs += [P / 'tools/i5/touch_keyboard_patch.py', C / 'sdl/src/events/SDL_keyboard.c',
               P / 'tools/i5/controller_uikit_patch.py']
    inputs += [C / ('sdl/src/video/uikit/' + name) for name in
               ('SDL_uikitview.m', 'SDL_uikitviewcontroller.m', 'SDL_uikitviewcontroller.h')]
    inputs += [C / 'sdl/controller-header-overlay.json']
    return {str(p): digest(p) for p in inputs}


def recoil_source_hashes():
    return {str(p): digest(p) for p in (
        P / 'tools/i5/recoil_patch.py',
        C / 'source/game/shared/cstrike15/weapon_csbase.cpp')}


def bot_takeover_source_hashes():
    return {str(p): digest(p) for p in (
        P / 'tools/i5/bot_takeover_patch.py',
        C / 'source/game/server/cstrike15/cs_player.cpp')}


def ui_resolution_source_hashes():
    return {str(p): digest(p) for p in (
        P / 'tools/i5/ui_resolution_patch.py',
        P / 'touch/scene_resolution_policy.h', P / 'touch/video_quality_preset.inc',
        C / 'source/game/client/view.cpp', C / 'source/game/client/viewrender.cpp')}


def weapon_icons_source_hashes():
    return {str(p): digest(p) for p in (
        P / 'tools/i5/weapon_icons_patch.py',
        C / 'source/game/shared/cstrike15/cs_gamerules.cpp',
        C / 'source/game/client/cstrike15/panorama/hud/csgo_hudspectator.cpp')}


def sticker_source_hashes():
    return {str(p): digest(p) for p in (
        P / 'tools/i5/sticker_patch.py', P / 'tools/i5/glove_patch.py', P / 'tools/i5/attribute_perf_patch.py',
        P / 'tools/i5/weapon_decal_patch.py', C / 'source/materialsystem/stdshaders/weapondecal_dx9_helper.cpp', C / 'source/game/client/c_baseviewmodel.cpp',
        C / 'source/game/shared/econ/econ_item_view.cpp')}


def controller_source_hashes():
    return {str(p): digest(p) for p in (
        P / 'tools/i5/controller_patch.py',
        P / 'tools/i5/local_transfer_patch.py',
        C / 'source/inputsystem/joystick_linux.cpp',
        C / 'source/inputsystem/inputsystem.cpp',
        C / 'source/panorama/input/controller.cpp',
        C / 'source/engine/servermsghandler.cpp',
        C / 'source/game/client/in_joystick.cpp',
        C / 'source/game/client/cstrike15/panorama/popups/csgo_settings_keybinder.cpp')}


def compile_controller_inputs():
    replacements = {}
    for name, relative in (
        ('in_joystick.cpp', 'game/client/in_joystick.cpp'),
        ('csgo_settings_keybinder.cpp', 'game/client/cstrike15/panorama/popups/csgo_settings_keybinder.cpp')):
        original, new = compile_changed(name, C / 'source' / relative, target='client')
        replacements[original] = new
    inputsystem = {}
    for name in ('joystick_linux.cpp', 'inputsystem.cpp'):
        original, new = compile_changed(name, C / 'source/inputsystem' / name, target='inputsystem')
        inputsystem[original] = new
    return replacements, inputsystem


def controller_object_records(replacements):
    return {old: {'path': str(new), 'sha256': digest(new)} for old, new in replacements.items()}


def save(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False) + '\n')


def run(argv, label, cwd=None):
    PRIVATE.mkdir(parents=True, exist_ok=True)
    PRIVATE.chmod(0o700)
    with (PRIVATE / (label + '.log')).open('w') as output:
        result = subprocess.run([str(x) for x in argv], cwd=cwd, stdout=output,
                                stderr=subprocess.STDOUT)
    if result.returncode:
        raise SystemExit(f'{label} failed ({result.returncode}); see {PRIVATE / (label + ".log")}')
    print(label + ': passed', flush=True)


def require(path):
    if not path.exists():
        raise SystemExit('Missing input: ' + str(path))


def prepare():
    originals = {
        'source/game/client/in_main.cpp': BASE / 'source/game/client/in_main.cpp',
        'source/game/client/in_joystick.cpp': BASE / 'source/game/client/in_joystick.cpp',
        'source/inputsystem/joystick_linux.cpp': BASE / 'source/inputsystem/joystick_linux.cpp',
        'source/game/client/cstrike15/panorama/popups/csgo_settings_keybinder.cpp': BASE / 'source/game/client/cstrike15/panorama/popups/csgo_settings_keybinder.cpp',
        'source/game/client/modelrendersystem.cpp': BASE / 'source/game/client/modelrendersystem.cpp',
        'source/matchmaking/cstrike15/mm_title.cpp': BASE / 'source/matchmaking/cstrike15/mm_title.cpp',
        'sdl/src/video/uikit/SDL_uikitview.m': BASE / 'sdl/src/video/uikit/SDL_uikitview.m',
        'sdl/src/video/uikit/SDL_uikitviewcontroller.m': BASE / 'sdl/src/video/uikit/SDL_uikitviewcontroller.m',
        'sdl/src/joystick/iphoneos/SDL_mfijoystick.m': BASE / 'sdl/src/joystick/iphoneos/SDL_mfijoystick.m',
        'platform/native_display_ios.cpp': P / 'client/native_display_ios.cpp',
        'mojoshader/mojoshader_common.c': W / 'macos-arm64/vendor/m6/mojoshader/mojoshader_common.c',
        'mojoshader/profiles/mojoshader_profile_common.c': W / 'macos-arm64/vendor/m6/mojoshader/profiles/mojoshader_profile_common.c',
        'source/game/shared/cstrike15/cstrike15_item_inventory.cpp': BASE / 'source/game/shared/cstrike15/cstrike15_item_inventory.cpp',
        'source/game/shared/cstrike15/weapon_csbase.cpp': BASE / 'source/game/shared/cstrike15/weapon_csbase.cpp',
        'source/game/shared/econ/econ_item_view.cpp': BASE / 'source/game/shared/econ/econ_item_view.cpp',
        'source/materialsystem/custom_material.cpp': BASE / 'source/materialsystem/custom_material.cpp',
        'source/materialsystem/composite_texture.cpp': BASE / 'source/materialsystem/composite_texture.cpp',
        'source/materialsystem/stdshaders/phong_dx9_helper.cpp': BASE / 'source/materialsystem/stdshaders/phong_dx9_helper.cpp',
        'source/materialsystem/stdshaders/lightmappedgeneric_dx9_helper.cpp': BASE / 'source/materialsystem/stdshaders/lightmappedgeneric_dx9_helper.cpp',
        'source/game/client/cstrike15/panorama/csgo_item_image_panel.cpp': BASE / 'source/game/client/cstrike15/panorama/csgo_item_image_panel.cpp',
        'source/game/server/cstrike15/cs_player.cpp': BASE / 'source/game/server/cstrike15/cs_player.cpp',
        'source/game/shared/cstrike15/cs_gamerules.cpp': BASE / 'source/game/shared/cstrike15/cs_gamerules.cpp',
        'source/game/client/cstrike15/panorama/hud/csgo_hudspectator.cpp': BASE / 'source/game/client/cstrike15/panorama/hud/csgo_hudspectator.cpp',
        'inventory/native_offline_ui.cpp': W / 'macos-arm64/compat/native_offline_ui.cpp',
        'weapon/weapon_csbase.h': BASE / 'source/game/shared/cstrike15/weapon_csbase.h',
        'source/engine/sys_getmodes.cpp': BASE / 'source/engine/sys_getmodes.cpp',
        'source/engine/sys_engine.cpp': BASE / 'source/engine/sys_engine.cpp',
        'source/engine/sys_dll2.cpp': BASE / 'source/engine/sys_dll2.cpp',
        'source/engine/net_ws.cpp': BASE / 'source/engine/net_ws.cpp',
        'source/engine/net_steamsocketmgr.cpp': BASE / 'source/engine/net_steamsocketmgr.cpp',
        'source/engine/matsys_interface.cpp': BASE / 'source/engine/matsys_interface.cpp',
        'source/game/client/cstrike15/panorama/csgo_videosettingsscreen.cpp': BASE / 'source/game/client/cstrike15/panorama/csgo_videosettingsscreen.cpp',
        'metal/native_window_ios.mm': P / 'client/native_window_ios.mm',
        'metal/diagnostics_ios.mm': P / 'render/diagnostics_ios.mm',
        'source/game/client/view.cpp': BASE / 'source/game/client/view.cpp',
        'source/game/client/viewrender.cpp': BASE / 'source/game/client/viewrender.cpp',
        'source/game/client/c_env_cascade_light.cpp': BASE / 'source/game/client/c_env_cascade_light.cpp',
        'source/game/shared/cstrike15/weapon_ironsightcontroller.cpp': BASE / 'source/game/shared/cstrike15/weapon_ironsightcontroller.cpp',
        'source/game/client/cstrike15/panorama/csgo_avatarimage.cpp': BASE / 'source/game/client/cstrike15/panorama/csgo_avatarimage.cpp',
        'source/game/client/cstrike15/panorama/hud/csgo_hudteamcounter.cpp': BASE / 'source/game/client/cstrike15/panorama/hud/csgo_hudteamcounter.cpp',
        'source/game/client/cstrike15/panorama/hud/csgo_hudwinpanel.cpp': BASE / 'source/game/client/cstrike15/panorama/hud/csgo_hudwinpanel.cpp',
        'source/vgui2/vgui_controls/consoledialog.cpp': BASE / 'source/vgui2/vgui_controls/consoledialog.cpp',
        'source/game/client/cstrike15/gameui/gameconsole.cpp': BASE / 'source/game/client/cstrike15/gameui/gameconsole.cpp',
    }
    # Only altered Source/SDL files are copied. Their original headers remain
    # read-only inputs through the original compile arguments and -iquote.
    for rel, original in originals.items():
        require(original)
        destination = C / rel
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(original, destination)
    # Metal's private headers are small and use local quoted includes.
    shutil.copytree(BASE / 'metal', C / 'metal', dirs_exist_ok=True)
    originals['metal/source_device.mm'] = BASE / 'metal/source_device.mm'
    originals['metal/native_shaders.mm'] = BASE / 'metal/native_shaders.mm'
    originals['metal/native_resources.mm'] = BASE / 'metal/native_resources.mm'
    before = {rel: digest(original) for rel, original in originals.items()}
    for name in ('skin_catalog.py', 'platform_patch.py', 'client_patch.py', 'video_patch.py', 'shader_patch.py', 'inventory_patch.py', 'workshop_patch.py', 'wargames_patch.py', 'silencer_patch.py', 'fullscreen_patch.py', 'skin_patch.py', 'sticker_patch.py', 'recoil_patch.py', 'bot_takeover_patch.py', 'scene_material_patch.py', 'shadow_submission_patch.py', 'render_patch.py', 'performance_patch.py', 'pass_optimization_patch.py', 'combat_optimization_patch.py', 'pipeline_warm_patch.py', 'spatial_upscale_patch.py', 'frame_pacing_patch.py', 'frame_interpolation_patch.py', 'profile_patch.py', 'console_patch.py', 'scene_quality_patch.py', 'ui_resolution_patch.py', 'weapon_icons_patch.py', 'controller_patch.py', 'local_transfer_patch.py', 'clear_cache_patch.py', 'touch_keyboard_patch.py'):
        script = P / 'tools/i5' / name
        require(script)
        run([PYTHON, script], 'prepare-' + name.removesuffix('.py'), cwd=W)
    run([PYTHON, P / 'tools/i5/network_patch.py'], 'prepare-network-patch', cwd=W)
    for rel, original in originals.items():
        if digest(original) != before[rel]:
            raise SystemExit('Immutable I4 input changed: ' + str(original))
    save(E / 'prepare-inputs.json', {
        'milestone': 'I5 touch trial', 'i4_mutated': False,
        'profile_sources': {str(p): digest(p) for p in (
            P / 'tools/i5/profile_patch.py',
            C / 'source/matchmaking/cstrike15/mm_title.cpp',
            C / 'source/game/client/cstrike15/panorama/csgo_avatarimage.cpp',
            C / 'source/game/client/cstrike15/panorama/hud/csgo_hudteamcounter.cpp',
            C / 'source/game/client/cstrike15/panorama/hud/csgo_hudwinpanel.cpp')},
        'created_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
        'files': [{ 'path': str(C / rel), 'original': str(original),
                    'before_sha256': before[rel], 'after_sha256': digest(C / rel)}
                  for rel, original in originals.items()],
        'patches': {name: digest(P / 'tools/i5' / name)
                    for name in ('skin_catalog.py', 'platform_patch.py', 'client_patch.py', 'video_patch.py', 'shader_patch.py', 'inventory_patch.py', 'workshop_patch.py', 'wargames_patch.py', 'silencer_patch.py', 'fullscreen_patch.py', 'skin_patch.py', 'sticker_patch.py', 'recoil_patch.py', 'bot_takeover_patch.py', 'scene_material_patch.py', 'shadow_submission_patch.py', 'render_patch.py', 'performance_patch.py', 'pass_optimization_patch.py', 'combat_optimization_patch.py', 'pipeline_warm_patch.py', 'spatial_upscale_patch.py', 'frame_pacing_patch.py', 'frame_interpolation_patch.py', 'profile_patch.py', 'console_patch.py', 'scene_quality_patch.py', 'ui_resolution_patch.py', 'weapon_icons_patch.py', 'controller_patch.py', 'local_transfer_patch.py', 'clear_cache_patch.py', 'touch_keyboard_patch.py')},
        'original_build_ninja_sha256': digest(OLD_BUILD / 'build.ninja'),
        'original_compile_commands_sha256': digest(OLD_BUILD / 'compile_commands.json'),
    })
    print('I5 touch inputs prepared independently', flush=True)


def compile_entry(name, target=None):
    entries = json.loads((OLD_BUILD / 'compile_commands.json').read_text())
    selected = [x for x in entries if x['file'].endswith('/' + name)]
    if target:
        selected = [x for x in selected if x['output'].startswith('CMakeFiles/' + target + '.dir/')]
    if len(selected) != 1:
        raise SystemExit(f'Expected exactly one original compile entry: {name}')
    return selected[0]


def compile_changed(name, source, extra=(), target=None):
    require(source)
    entry = compile_entry(name, target)
    argv = shlex.split(entry['command'])
    output = C / 'objects' / ((target + '-' if target else '') + name + '.o')
    output.parent.mkdir(parents=True, exist_ok=True)
    argv[argv.index('-o') + 1] = str(output)
    argv[argv.index('-c') + 1] = str(source)
    argv += ['-I' + str(TOUCH), '-I' + str(INVENTORY), '-I' + str(P / 'render'), '-iquote', str(pathlib.Path(entry['file']).parent)]
    if name.endswith('.cpp'):
        argv += ['-ivfsoverlay', str(C / 'weapon/header-overlay.json')]
    if name in ('SDL_uikitview.m', 'SDL_uikitviewcontroller.m'):
        argv += ['-ivfsoverlay', str(C / 'sdl/controller-header-overlay.json')]
    argv += [str(x) for x in extra]
    run(argv, 'compile-' + name, cwd=OLD_BUILD)
    return entry['output'], output


def compile_weapon_dependencies(already_rebuilt):
    """Recompile every recorded header dependent; no mixture of old inline stubs."""
    deps = subprocess.check_output([str(NINJA), '-t', 'deps'], cwd=OLD_BUILD, text=True)
    header = str(BASE / 'source/game/shared/cstrike15/weapon_csbase.h')
    affected = {block.splitlines()[0].split(': #deps')[0] for block in deps.split('\n\n')
                if '\n    ' + header + '\n' in block}
    entries = json.loads((OLD_BUILD / 'compile_commands.json').read_text())
    selected = [entry for entry in entries if entry['output'] in affected]
    if len(selected) != len(affected) or not affected:
        raise SystemExit('Cannot resolve all original weapon header dependents')
    overlay = C / 'weapon/header-overlay.json'
    input_hash = digest(overlay) + digest(C / 'weapon/weapon_csbase.h')
    cache_path = E / 'weapon-object-cache.json'
    cache = json.loads(cache_path.read_text()) if cache_path.exists() else {}

    def compile_one(entry):
        original_output = entry['output']
        if original_output in already_rebuilt:
            return original_output, already_rebuilt[original_output], None
        argv = shlex.split(entry['command'])
        output = C / 'objects/weapon-dependencies' / original_output
        output.parent.mkdir(parents=True, exist_ok=True)
        argv[argv.index('-o') + 1] = str(output)
        argv += ['-ivfsoverlay', str(overlay)]
        stamp = hashlib.sha256((input_hash + digest(entry['file']) + repr(argv)).encode()).hexdigest()
        prior = cache.get(original_output, {})
        if prior.get('input') == stamp and output.exists() and digest(output) == prior.get('object'):
            return original_output, output, prior
        log = PRIVATE / ('weapon-' + hashlib.sha256(original_output.encode()).hexdigest()[:16] + '.log')
        with log.open('w') as stream:
            result = subprocess.run(argv, cwd=OLD_BUILD, stdout=stream, stderr=subprocess.STDOUT)
        if result.returncode:
            raise RuntimeError(f'Weapon dependency compilation failed: {entry["file"]}; see {log}')
        return original_output, output, {'input': stamp, 'object': digest(output)}

    result = {'client': {}, 'server': {}}
    with concurrent.futures.ThreadPoolExecutor(max_workers=6) as pool:
        futures = [pool.submit(compile_one, entry) for entry in selected]
        for count, future in enumerate(concurrent.futures.as_completed(futures), 1):
            old, new, record = future.result()
            target = old.split('/')[1].removesuffix('.dir')
            if target not in result:
                raise SystemExit('Unexpected weapon dependency target: ' + target)
            result[target][old] = new
            if record: cache[old] = record
            if count % 25 == 0 or count == len(selected):
                print(f'Weapon header dependents: {count}/{len(selected)} compiled', flush=True)
    save(cache_path, cache)
    save(E / 'weapon-dependencies.json', {
        'header_original': header, 'header_original_sha256': digest(header),
        'header_derived': str(C / 'weapon/weapon_csbase.h'),
        'header_derived_sha256': digest(C / 'weapon/weapon_csbase.h'),
        'overlay': str(overlay), 'overlay_sha256': digest(overlay),
        'dependent_count': len(selected),
        'targets': {target: {old: str(new) for old, new in inputs.items()}
                    for target, inputs in result.items()},
    })
    return result


def compile_touch():
    source = TOUCH / 'touch_ui_ios.mm'
    require(source)
    require(TOUCH / 'touch_ios.h')
    template = compile_entry('SDL_uikitview.m')
    original = shlex.split(template['command'])
    argv = [str(pathlib.Path(original[0]).with_name('clang++'))]
    index = 1
    while index < len(original):
        arg = original[index]
        if arg in ('-x', '-o', '-c'):
            index += 2
            continue
        if arg.startswith('-std='):
            index += 1
            continue
        argv.append(arg)
        index += 1
    output = C / 'objects/touch_ui_ios.mm.o'
    output.parent.mkdir(parents=True, exist_ok=True)
    argv += ['-x', 'objective-c++', '-std=c++17', '-I' + str(TOUCH),
             '-c', str(source), '-o', str(output)]
    run(argv, 'compile-touch-ui', cwd=OLD_BUILD)
    return output


def link_original(target, replacements, added=(), extra=()):
    # Every incremental link retains the audited controller/crash fixes.
    # A full rebuild supplies fresh objects explicitly and takes precedence.
    replacements = dict(replacements)
    def retain(original, row):
        selected = replacements.get(original)
        # Weapon-header dependency objects are fallback recompilations of the
        # frozen I4 source. They satisfy the header ABI, but lack later input
        # and cosmetic fixes. Prefer the patched I5 object, compiled with that
        # same header overlay. Explicit new patched objects still take priority.
        if selected is None or pathlib.Path(selected).is_relative_to(C / 'objects/weapon-dependencies'):
            assert digest(row['path']) == row['sha256'], row['path']
            replacements[original] = row['path']
    if target == 'client' and (E / 'module-products.json').exists():
        metadata = json.loads((E / 'module-products.json').read_text())
        for original, row in {**metadata.get('controller_objects', {}), **metadata.get('sticker_objects', {})}.items():
            retain(original, row)
    if (E / 'module-products.json').exists():
        metadata = json.loads((E / 'module-products.json').read_text())
        for original, row in metadata.get('sticker_module_objects', {}).get(target, {}).items():
            retain(original, row)
        for original, row in metadata.get('controller_module_objects', {}).get(target, {}).items():
            retain(original, row)
    result = subprocess.run([str(NINJA), '-t', 'commands', target], cwd=OLD_BUILD,
                            check=True, text=True, capture_output=True)
    command = shlex.split(result.stdout.splitlines()[-1])
    if command[:2] != [':', '&&'] or command[-2:] != ['&&', ':']:
        raise SystemExit('Unexpected original link command syntax: ' + target)
    argv = command[2:-2]
    old_output = argv[argv.index('-o') + 1]
    output = C / 'client-build' / old_output
    output.parent.mkdir(parents=True, exist_ok=True)
    argv[argv.index('-o') + 1] = str(output)
    for original in replacements:
        if argv.count(original) != 1:
            raise SystemExit(f'Expected one link input replacement for {target}: {original}')
    argv = [str(replacements.get(arg, arg)) for arg in argv]
    argv += [str(x) for x in added]
    argv += [str(x) for x in extra]
    # All inputs can refer to frozen I4 paths. The only write destination in
    # this argv is explicitly moved into I5, and no shell is involved.
    if not output.is_relative_to(C):
        raise SystemExit('Link output escaped I5')
    run(argv, 'link-' + target, cwd=OLD_BUILD)
    save(E / ('link-inputs-' + target + '.json'), {
        'target': target, 'output': str(output), 'output_sha256': digest(output),
        'replacements': {old: {'path': str(new), 'sha256': digest(new)}
                         for old, new in replacements.items()}})
    return output


def archive_original(target, replacements):
    result = subprocess.run([str(NINJA), '-t', 'commands', target], cwd=OLD_BUILD,
                            check=True, text=True, capture_output=True)
    tokens = shlex.split(result.stdout.splitlines()[-1])
    commands, current = [], []
    for token in tokens:
        if token == '&&':
            commands.append(current)
            current = []
        else:
            current.append(token)
    commands.append(current)
    # Extract only ar and ranlib. Never run the original rm/touch commands,
    # whose arguments address the immutable I4 archive.
    archivers = [cmd for cmd in commands if cmd and pathlib.Path(cmd[0]).name == 'ar']
    indexers = [cmd for cmd in commands if cmd and pathlib.Path(cmd[0]).name == 'ranlib']
    if len(archivers) != 1 or len(indexers) != 1:
        raise SystemExit('Unexpected original archive commands: ' + target)
    argv = archivers[0]
    if len(argv) < 4 or argv[1] != 'qc':
        raise SystemExit('Unexpected original archive syntax: ' + target)
    old_output = argv[2]
    output = C / 'client-build' / old_output
    if not output.is_relative_to(C):
        raise SystemExit('Archive output escaped I5')
    output.parent.mkdir(parents=True, exist_ok=True)
    output.unlink(missing_ok=True)
    argv[2] = str(output)
    for original in replacements:
        if argv.count(original) != 1:
            raise SystemExit(f'Expected one archive input replacement for {target}: {original}')
    argv = [str(replacements.get(arg, arg)) for arg in argv]
    run(argv, 'archive-' + target, cwd=OLD_BUILD)
    run([indexers[0][0], output], 'ranlib-' + target, cwd=OLD_BUILD)
    return output


def modules():
    require(E / 'prepare-inputs.json')
    frameworks = C / 'client-build/Frameworks'
    frameworks.mkdir(parents=True, exist_ok=True)
    # Copy before overwriting three selected binaries with independent links.
    for original in (OLD_BUILD / 'Frameworks').glob('*.framework'):
        destination = frameworks / original.name
        if destination.exists():
            shutil.rmtree(destination)
        shutil.copytree(original, destination, symlinks=True)
    for module in ('tier0', 'vstdlib', 'vphysics'):
        original = C.parent / f'i1/engine-build/Frameworks/{module}.framework'
        destination = frameworks / original.name
        if destination.exists():
            shutil.rmtree(destination)
        shutil.copytree(original, destination, symlinks=True)
    old_sdl_object, sdl_object = compile_changed(
        'SDL_uikitview.m', C / 'sdl/src/video/uikit/SDL_uikitview.m')
    old_controller_object, controller_object = compile_changed(
        'SDL_mfijoystick.m', C / 'sdl/src/joystick/iphoneos/SDL_mfijoystick.m')
    old_orientation_object, orientation_object = compile_changed(
        'SDL_uikitviewcontroller.m', C / 'sdl/src/video/uikit/SDL_uikitviewcontroller.m')
    run([PYTHON, P / 'tools/i5/touch_keyboard_patch.py'], 'prepare-touch-keyboard')
    old_keyboard, keyboard = compile_changed('SDL_keyboard.c', C / 'sdl/src/events/SDL_keyboard.c')
    touch_object = compile_touch()
    sdl = link_original('SDL2', {old_sdl_object: sdl_object,
                               old_controller_object: controller_object,
                               old_orientation_object: orientation_object, old_keyboard: keyboard}, added=[touch_object],
                        extra=['-lc++', '-framework', 'TouchController',
                               '-framework', 'CoreGraphics', '-framework', 'CoreText'])
    old_console, console_object = compile_changed(
        'consoledialog.cpp', C / 'source/vgui2/vgui_controls/consoledialog.cpp', target='vgui_controls')
    console_controls = archive_original('vgui_controls', {old_console: console_object})
    old_gameconsole, gameconsole_object = compile_changed(
        'gameconsole.cpp', C / 'source/game/client/cstrike15/gameui/gameconsole.cpp', target='client')
    old_client_object, client_object = compile_changed(
        'in_main.cpp', C / 'source/game/client/in_main.cpp')
    old_sdl = 'Frameworks/SDL2-2.0.framework/SDL2-2.0'
    old_inventory_object, inventory_object = compile_changed(
        'cstrike15_item_inventory.cpp', C / 'source/game/shared/cstrike15/cstrike15_item_inventory.cpp', target='client')
    old_inventory_ui, inventory_ui = compile_changed(
        'native_offline_ui.cpp', C / 'inventory/native_offline_ui.cpp')
    old_player, player = compile_changed(
        'cs_player.cpp', C / 'source/game/server/cstrike15/cs_player.cpp', target='server')
    old_kill_rules, kill_rules = compile_changed(
        'cs_gamerules.cpp', C / 'source/game/shared/cstrike15/cs_gamerules.cpp', target='server')
    old_spectator, spectator = compile_changed(
        'csgo_hudspectator.cpp', C / 'source/game/client/cstrike15/panorama/hud/csgo_hudspectator.cpp', target='client')
    old_sticker_vm, sticker_vm = compile_changed('c_baseviewmodel.cpp', C / 'source/game/client/c_baseviewmodel.cpp', target='client')
    old_skin_client, skin_client = compile_changed(
        'weapon_csbase.cpp', C / 'source/game/shared/cstrike15/weapon_csbase.cpp', target='client')
    old_skin_server, skin_server = compile_changed(
        'weapon_csbase.cpp', C / 'source/game/shared/cstrike15/weapon_csbase.cpp', target='server')
    old_skin_item, skin_item = compile_changed(
        'econ_item_view.cpp', C / 'source/game/shared/econ/econ_item_view.cpp', target='client')
    old_server_econ, server_econ = compile_changed(
        'econ_item_view.cpp', C / 'source/game/shared/econ/econ_item_view.cpp', target='server')
    old_skin_image, skin_image = compile_changed(
        'csgo_item_image_panel.cpp', C / 'source/game/client/cstrike15/panorama/csgo_item_image_panel.cpp', target='client')
    old_video_ui, video_ui = compile_changed(
        'csgo_videosettingsscreen.cpp', C / 'source/game/client/cstrike15/panorama/csgo_videosettingsscreen.cpp', target='client')
    old_view, view = compile_changed('view.cpp', C / 'source/game/client/view.cpp', target='client')
    old_viewrender, viewrender = compile_changed('viewrender.cpp', C / 'source/game/client/viewrender.cpp', target='client')
    old_cascade, cascade = compile_changed('c_env_cascade_light.cpp', C / 'source/game/client/c_env_cascade_light.cpp', target='client')
    old_ironsight, ironsight = compile_changed('weapon_ironsightcontroller.cpp', C / 'source/game/shared/cstrike15/weapon_ironsightcontroller.cpp', target='client')
    profile_objects = {}
    for name, relative in (
        ('csgo_avatarimage.cpp', 'game/client/cstrike15/panorama/csgo_avatarimage.cpp'),
        ('csgo_hudteamcounter.cpp', 'game/client/cstrike15/panorama/hud/csgo_hudteamcounter.cpp'),
        ('csgo_hudwinpanel.cpp', 'game/client/cstrike15/panorama/hud/csgo_hudwinpanel.cpp')):
        old, new = compile_changed(name, C / 'source' / relative, target='client')
        profile_objects[old] = new
    changed = {old_client_object: client_object, old_inventory_object: inventory_object,
               old_inventory_ui: inventory_ui, old_player: player, old_video_ui: video_ui,
               old_skin_client: skin_client, old_skin_server: skin_server, old_skin_item: skin_item,
               old_skin_image: skin_image, old_view: view, old_viewrender: viewrender,
               old_ironsight: ironsight, old_cascade: cascade,
               old_kill_rules: kill_rules, old_spectator: spectator}
    weapons = compile_weapon_dependencies(changed)
    client_inputs = {old_client_object: client_object, old_sdl: sdl,
                     old_inventory_object: inventory_object, old_inventory_ui: inventory_ui,
                     old_video_ui: video_ui, old_skin_client: skin_client, old_skin_item: skin_item,
                     old_skin_image: skin_image, old_view: view, old_viewrender: viewrender,
               old_ironsight: ironsight}
    client_inputs.update(weapons['client'])
    client_inputs[old_sticker_vm] = sticker_vm
    client_inputs.update(profile_objects)
    client_inputs[old_spectator] = spectator
    client_inputs[old_cascade] = cascade
    client_inputs[old_gameconsole] = gameconsole_object
    client_inputs['libvgui_controls.a'] = console_controls
    old_shadow_submit, shadow_submit = compile_changed('modelrendersystem.cpp', C / 'source/game/client/modelrendersystem.cpp', target='client')
    client_inputs[old_shadow_submit] = shadow_submit
    controller_inputs, inputsystem_inputs = compile_controller_inputs()
    client_inputs.update(controller_inputs)
    client = link_original('client', client_inputs)
    inputsystem = link_original('inputsystem', {
        **inputsystem_inputs, old_sdl: sdl})
    old_pad_ui, pad_ui = compile_changed('controller.cpp', C / 'source/panorama/input/controller.cpp', target='panorama')
    panorama = link_original('panorama', {old_pad_ui: pad_ui, old_sdl: sdl})
    server_inputs = {old_player: player, old_skin_server: skin_server}
    server_inputs.update(weapons['server'])
    server_inputs[old_server_econ] = server_econ
    server_inputs[old_kill_rules] = kill_rules
    server = link_original('server', server_inputs)
    old_custom_material, custom_material = compile_changed(
        'custom_material.cpp', C / 'source/materialsystem/custom_material.cpp', target='materialsystem')
    old_composite, composite = compile_changed('composite_texture.cpp',
        C / 'source/materialsystem/composite_texture.cpp', target='materialsystem')
    materialsystem = link_original('materialsystem', {old_custom_material: custom_material,
        old_composite: composite})
    old_phong, phong = compile_changed('phong_dx9_helper.cpp',
        C / 'source/materialsystem/stdshaders/phong_dx9_helper.cpp', target='stdshader_dx9')
    old_lightmapped, lightmapped = compile_changed('lightmappedgeneric_dx9_helper.cpp',
        C / 'source/materialsystem/stdshaders/lightmappedgeneric_dx9_helper.cpp', target='stdshader_dx9')
    old_decal, decal = compile_changed('weapondecal_dx9_helper.cpp',
        C / 'source/materialsystem/stdshaders/weapondecal_dx9_helper.cpp', target='stdshader_dx9')
    stdshader = link_original('stdshader_dx9', {old_phong: phong, old_lightmapped: lightmapped, old_decal: decal})
    run([PYTHON, P / 'tools/i5/memory_optimization_patch.py'], 'prepare-memory-optimization', cwd=W)
    old_metal_object, metal_object = compile_changed(
        'source_device.mm', C / 'metal/source_device.mm')
    old_window_object, window_object = compile_changed(
        'native_window_ios.mm', C / 'metal/native_window_ios.mm')
    old_diagnostics_object, diagnostics_object = compile_changed(
        'diagnostics_ios.mm', C / 'metal/diagnostics_ios.mm')
    old_shaders_object, shaders_object = compile_changed(
        'native_shaders.mm', C / 'metal/native_shaders.mm')
    old_resources_object, resources_object = compile_changed(
        'native_resources.mm', C / 'metal/native_resources.mm')
    old_mojo_common, mojo_common = compile_changed(
        'mojoshader_common.c', C / 'mojoshader/mojoshader_common.c')
    old_mojo_profile, mojo_profile = compile_changed(
        'mojoshader_profile_common.c', C / 'mojoshader/profiles/mojoshader_profile_common.c')
    mojo = archive_original('m6_mojoshader', {old_mojo_common: mojo_common,
                                              old_mojo_profile: mojo_profile})
    metal = link_original('togl', {old_metal_object: metal_object, old_sdl: sdl,
                                  old_window_object: window_object,
                                  old_diagnostics_object: diagnostics_object,
                                  old_shaders_object: shaders_object,
                                  old_resources_object: resources_object,
                                  'libm6_mojoshader.a': mojo}, extra=['-framework', 'MetalFX'])
    old_display_object, display_object = compile_changed(
        'native_display_ios.cpp', C / 'platform/native_display_ios.cpp')
    appframework = archive_original('appframework', {old_display_object: display_object})
    launcher = link_original('launcher', {'libappframework.a': appframework,
                                         'Frameworks/source_metal.framework/source_metal': metal,
                                         old_sdl: sdl})
    old_modes, modes = compile_changed('sys_getmodes.cpp', C / 'source/engine/sys_getmodes.cpp', target='engine')
    old_config, config = compile_changed('matsys_interface.cpp', C / 'source/engine/matsys_interface.cpp', target='engine')
    old_pacing, pacing = compile_changed('sys_engine.cpp', C / 'source/engine/sys_engine.cpp', target='engine')
    old_loop, loop = compile_changed('sys_dll2.cpp', C / 'source/engine/sys_dll2.cpp', target='engine')
    network_objects = {}
    for name in ('net_ws.cpp', 'net_steamsocketmgr.cpp'):
        old, new = compile_changed(name, C / 'source/engine' / name, target='engine')
        network_objects[old] = new
    old_transfer, transfer = compile_changed('servermsghandler.cpp', C / 'source/engine/servermsghandler.cpp', target='engine')
    engine = link_original('engine', {old_modes: modes, old_config: config, old_pacing: pacing, old_loop: loop,
                                     old_transfer: transfer, **network_objects,
                                     'libvgui_controls.a': console_controls,
                                     old_sdl: sdl, 'Frameworks/source_metal.framework/source_metal': metal})
    old_match_title, match_title = compile_changed(
        'mm_title.cpp', C / 'source/matchmaking/cstrike15/mm_title.cpp', target='matchmaking')
    matchmaking = link_original('matchmaking', {old_match_title: match_title})
    inputs = json.loads((E / 'prepare-inputs.json').read_text())
    for row in inputs['files']:
        if digest(row['original']) != row['before_sha256']:
            raise SystemExit('Immutable I4 source changed: ' + row['original'])
    save(E / 'module-products.json', {
        'milestone': 'I5 touch trial', 'i4_mutated': False,
        'profile_sources': inputs['profile_sources'],
        'network_sources': {str(p): digest(p) for p in (
            P / 'tools/i5/network_patch.py', C / 'source/engine/net_ws.cpp',
            C / 'source/engine/net_steamsocketmgr.cpp')},
        'console_sources': {str(p): digest(p) for p in (
            P / 'tools/i5/console_patch.py', C / 'source/vgui2/vgui_controls/consoledialog.cpp',
            C / 'source/game/client/cstrike15/gameui/gameconsole.cpp')},
        'rebuilt': [{ 'path': str(p), 'sha256': digest(p)} for p in
                    (sdl, client, inputsystem, panorama, server, materialsystem, stdshader, mojo, metal, appframework, launcher, engine, matchmaking)],
        'fullscreen_sources': {str(p): digest(p) for p in (
            P / 'tools/i5/fullscreen_patch.py', C / 'source/engine/sys_getmodes.cpp',
            C / 'source/engine/matsys_interface.cpp',
            C / 'source/game/client/cstrike15/panorama/csgo_videosettingsscreen.cpp')},
        'touch_sources': touch_source_hashes(),
        'inventory_sources': inventory_source_hashes(),
        'recoil_sources': recoil_source_hashes(),
        'bot_takeover_sources': bot_takeover_source_hashes(),
        'ui_resolution_sources': ui_resolution_source_hashes(),
        'weapon_icons_sources': weapon_icons_source_hashes(),
        'sticker_sources': sticker_source_hashes(),
        'sticker_objects': controller_object_records({old_sticker_vm: sticker_vm}),
        'sticker_module_objects': {'server': controller_object_records({old_server_econ: server_econ}),
                                   'stdshader_dx9': controller_object_records({old_decal: decal})},
        'controller_sources': controller_source_hashes(),
        'controller_objects': controller_object_records(controller_inputs),
        'controller_module_objects': {
            'inputsystem': controller_object_records(inputsystem_inputs),
            'panorama': controller_object_records({old_pad_ui: pad_ui}),
            'engine': controller_object_records({old_transfer: transfer})},
        'render_sources': {str(p): digest(p) for p in (P / 'tools/i5/shader_sampler_patch.py', P / 'render/shader_shadow_policy.h', P / 'tools/i5/scene_quality_patch.py', P / 'render/scene_quality_ios.inc', P / 'render/world_sampler_quality_ios.inc', C / 'source/game/client/c_env_cascade_light.cpp', P / 'tools/i5/scene_material_patch.py', P / 'render/scene_material_ios.inc', P / 'render/map_material_ios.inc', P / 'tools/i5/shadow_submission_patch.py', P / 'render/shadow_submission_ios.inc', C / 'source/game/client/modelrendersystem.cpp', C / 'source/materialsystem/stdshaders/lightmappedgeneric_dx9_helper.cpp', P / 'tools/i5/frame_interpolation_patch.py', P / 'render/frame_interpolation_ios.h', P / 'render/frame_interpolation_ios.inc', P / 'render/frame_interpolation_motion.metal', P / 'render/frame_interpolation_capture.inc', P / 'tools/i5/render_patch.py', P / 'tools/i5/performance_patch.py', P / 'tools/i5/pass_optimization_patch.py', P / 'tools/i5/combat_optimization_patch.py', P / 'tools/i5/pipeline_warm_patch.py', P / 'render/pipeline_warm_ios.h', P / 'render/pipeline_warm_ios.inc', P / 'render/pipeline_warm_identity.h', P / 'tools/i5/spatial_upscale_patch.py', P / 'render/metal_spatial_ios.h', P / 'render/metal_spatial_ios.inc', P / 'render/spatial_selftest_ios.inc', P / 'render/frame_interpolation_probe_ios.inc', P / 'tools/i5/frame_pacing_patch.py', P / 'render/frame_pacing_probe.inc', P / 'render/frame_deadline_ios.h', P / 'render/frame_deadline_ios.inc', C / 'source/engine/sys_engine.cpp', C / 'source/engine/sys_dll2.cpp', C / 'metal/native_objects.h', P / 'render/metal_combat_ios.h', P / 'render/metal_combat_ios.inc', P / 'render/combat_selftest_ios.inc', P / 'render/render_pass_ios.inc', P / 'render/clear_rects_ios.inc', P / 'render/clear_selftest_ios.inc', P / 'render/clear_color_policy.h', P / 'render/pass_descriptor_ios.inc', P / 'render/texture_lock_trace_ios.inc', P / 'render/alpha_atlas_selftest_ios.inc', P / 'render/texture_staging_pool.h', P / 'render/texture_staging_ios.inc', P / 'render/texture_staging_selftest_ios.inc', P / 'tools/i5/memory_optimization_patch.py', P / 'tools/i5/clear_cache_patch.py', P / 'render/metal_perf_ios.h', P / 'render/metal_perf_ios.inc', P / 'render/metal_buffer_pool_ios.inc', P / 'render/buffer_selftest_ios.inc', C / 'metal/native_resources.mm', P / 'tools/i5/video_patch.py', P / 'render/resolution_audit.inc', P / 'render/shader_cache_ios.inc', C / 'metal/source_device.mm', C / 'metal/native_shaders.mm', C / 'metal/native_window_ios.mm', C / 'metal/diagnostics_ios.mm', C / 'source/game/client/view.cpp', C / 'source/game/client/viewrender.cpp', C / 'source/game/shared/cstrike15/weapon_ironsightcontroller.cpp', P / 'render/scope_audit.inc', C / 'platform/native_display_ios.cpp')},
        'weapon_dependencies': json.loads((E / 'weapon-dependencies.json').read_text()),
        'frameworks': [{ 'path': str(p), 'binary_sha256': digest(p / p.stem)}
                       for p in sorted(frameworks.glob('*.framework'))],
    })
    print('I5 independent modules linked', flush=True)


def network_modules():
    """Rebuild networking while preserving the already audited module inputs."""
    metadata = json.loads((E / 'module-products.json').read_text())
    for row in metadata['rebuilt']:
        assert digest(row['path']) == row['sha256'], row['path']
    replacements = {}
    for name in ('sys_getmodes.cpp', 'matsys_interface.cpp', 'sys_engine.cpp', 'sys_dll2.cpp'):
        old = compile_entry(name, 'engine')['output']
        replacements[old] = C / 'objects' / ('engine-' + name + '.o')
        require(replacements[old])
    for name in ('net_ws.cpp', 'net_steamsocketmgr.cpp'):
        old, new = compile_changed(name, C / 'source/engine' / name, target='engine')
        replacements[old] = new
    replacements.update({
        'libvgui_controls.a': C / 'client-build/libvgui_controls.a',
        'Frameworks/SDL2-2.0.framework/SDL2-2.0': C / 'client-build/Frameworks/SDL2-2.0.framework/SDL2-2.0',
        'Frameworks/source_metal.framework/source_metal': C / 'client-build/Frameworks/source_metal.framework/source_metal',
    })
    engine = link_original('engine', replacements)
    for row in metadata['rebuilt']:
        if row['path'] == str(engine): row['sha256'] = digest(engine)
    for row in metadata['frameworks']:
        if pathlib.Path(row['path']).name == 'engine.framework': row['binary_sha256'] = digest(engine)
    metadata['network_sources'] = {str(p): digest(p) for p in (
        P / 'tools/i5/network_patch.py', C / 'source/engine/net_ws.cpp',
        C / 'source/engine/net_steamsocketmgr.cpp')}
    save(E / 'module-products.json', metadata)


def inventory_network_modules():
    """Relink only the sender UI while retaining every audited I5 client fix."""
    metadata = json.loads((E / 'module-products.json').read_text())
    for row in metadata['rebuilt']:
        assert digest(row['path']) == row['sha256'], row['path']
    replacements = dict(metadata['weapon_dependencies']['targets']['client'])
    for name, target in (
        ('in_main.cpp', None), ('cstrike15_item_inventory.cpp', 'client'),
        ('native_offline_ui.cpp', None), ('csgo_videosettingsscreen.cpp', 'client'),
        ('weapon_csbase.cpp', 'client'), ('econ_item_view.cpp', 'client'),
        ('csgo_item_image_panel.cpp', 'client'), ('view.cpp', 'client'), ('c_env_cascade_light.cpp', 'client'),
        ('viewrender.cpp', 'client'), ('weapon_ironsightcontroller.cpp', 'client'),
        ('csgo_avatarimage.cpp', 'client'), ('csgo_hudteamcounter.cpp', 'client'),
        ('csgo_hudwinpanel.cpp', 'client'), ('gameconsole.cpp', 'client'), ('c_baseviewmodel.cpp', 'client'),
    ):
        original = compile_entry(name, target)['output']
        replacements[original] = C / 'objects' / ((target + '-' if target else '') + name + '.o')
    spectator = C / 'objects/client-csgo_hudspectator.cpp.o'
    if metadata.get('weapon_icons_sources'):
        require(spectator)
        replacements[compile_entry('csgo_hudspectator.cpp', 'client')['output']] = spectator
    shadow_submit = C / 'objects/client-modelrendersystem.cpp.o'
    if shadow_submit.exists():
        replacements[compile_entry('modelrendersystem.cpp','client')['output']] = shadow_submit
    # The Source input bridge includes the quality recipe. Rebuild its actual
    # owner as well as the inventory UI before recording current touch hashes.
    for name, source in (
        ('in_main.cpp', C / 'source/game/client/in_main.cpp'),
        ('native_offline_ui.cpp', C / 'inventory/native_offline_ui.cpp')):
        old, new = compile_changed(name, source)
        replacements[old] = new
    replacements.update({
        'libvgui_controls.a': C / 'client-build/libvgui_controls.a',
        'Frameworks/SDL2-2.0.framework/SDL2-2.0': C / 'client-build/Frameworks/SDL2-2.0.framework/SDL2-2.0',
    })
    for path in replacements.values():
        require(pathlib.Path(path))
    client = link_original('client', replacements)
    for row in metadata['rebuilt']:
        if row['path'] == str(client): row['sha256'] = digest(client)
    for row in metadata['frameworks']:
        if pathlib.Path(row['path']) == client.parent: row['binary_sha256'] = digest(client)
    metadata['inventory_sources'] = inventory_source_hashes()
    metadata['touch_sources'] = touch_source_hashes()
    save(E / 'module-products.json', metadata)

def room_browser_modules():
    """Rebuild native discovery UI and its command while preserving all I5 fixes."""
    metadata = json.loads((E / 'module-products.json').read_text())
    for row in metadata['rebuilt']:
        assert digest(row['path']) == row['sha256'], row['path']
    console_paths = (P / 'tools/i5/console_patch.py', C / 'source/vgui2/vgui_controls/consoledialog.cpp',
                     C / 'source/game/client/cstrike15/gameui/gameconsole.cpp')
    if any(metadata.get('console_sources', {}).get(str(path)) != digest(path) for path in console_paths):
        old_console, console_object = compile_changed('consoledialog.cpp', console_paths[1], target='vgui_controls')
        archive_original('vgui_controls', {old_console: console_object})
        compile_changed('gameconsole.cpp', console_paths[2], target='client')
        metadata['console_sources'] = {str(path): digest(path) for path in console_paths}
    run([PYTHON, P / 'tools/i5/touch_keyboard_patch.py'], 'prepare-touch-keyboard')
    keyboard_old, keyboard_new = compile_changed('SDL_keyboard.c', C / 'sdl/src/events/SDL_keyboard.c')
    touch = compile_touch()
    sdl_inputs = {keyboard_old: keyboard_new}
    for name in ('SDL_uikitview.m', 'SDL_mfijoystick.m', 'SDL_uikitviewcontroller.m'):
        if name != 'SDL_mfijoystick.m':
            compile_changed(name, C / 'sdl/src/video/uikit' / name)
        sdl_inputs[compile_entry(name)['output']] = C / 'objects' / (name + '.o')
        require(sdl_inputs[compile_entry(name)['output']])
    sdl = link_original('SDL2', sdl_inputs, added=[touch],
                        extra=['-lc++', '-framework', 'TouchController',
                               '-framework', 'CoreGraphics', '-framework', 'CoreText'])
    replacements = dict(metadata['weapon_dependencies']['targets']['client'])
    for name, target in (
        ('in_main.cpp', None), ('cstrike15_item_inventory.cpp', 'client'),
        ('native_offline_ui.cpp', None), ('csgo_videosettingsscreen.cpp', 'client'),
        ('weapon_csbase.cpp', 'client'), ('econ_item_view.cpp', 'client'),
        ('csgo_item_image_panel.cpp', 'client'), ('view.cpp', 'client'), ('c_env_cascade_light.cpp', 'client'),
        ('viewrender.cpp', 'client'), ('weapon_ironsightcontroller.cpp', 'client'),
        ('csgo_avatarimage.cpp', 'client'), ('csgo_hudteamcounter.cpp', 'client'),
        ('csgo_hudwinpanel.cpp', 'client'), ('gameconsole.cpp', 'client'), ('c_baseviewmodel.cpp', 'client')):
        replacements[compile_entry(name, target)['output']] = C / 'objects' / ((target + '-' if target else '') + name + '.o')
    old, new = compile_changed('in_main.cpp', C / 'source/game/client/in_main.cpp')
    replacements[old] = new
    if metadata.get('weapon_icons_sources'):
        spectator = C / 'objects/client-csgo_hudspectator.cpp.o'
        require(spectator)
        replacements[compile_entry('csgo_hudspectator.cpp', 'client')['output']] = spectator
    shadow_submit = C / 'objects/client-modelrendersystem.cpp.o'
    if shadow_submit.exists():
        replacements[compile_entry('modelrendersystem.cpp', 'client')['output']] = shadow_submit
    replacements.update({'libvgui_controls.a': C / 'client-build/libvgui_controls.a',
                         'Frameworks/SDL2-2.0.framework/SDL2-2.0': sdl})
    for path in replacements.values(): require(pathlib.Path(path))
    client = link_original('client', replacements)
    for row in metadata['rebuilt']:
        if row['path'] in (str(sdl), str(client)): row['sha256'] = digest(row['path'])
    for row in metadata['frameworks']:
        for product in (sdl, client):
            if pathlib.Path(row['path']) == product.parent: row['binary_sha256'] = digest(product)
    metadata['touch_sources'] = touch_source_hashes()
    save(E / 'module-products.json', metadata)


def controller_modules():
    """Rebuild controller input and guard ordinary map signon against local-copy tables."""
    metadata = json.loads((E / 'module-products.json').read_text())
    for row in metadata['rebuilt']:
        assert digest(row['path']) == row['sha256'], row['path']
    for patch in ('controller_patch.py', 'controller_uikit_patch.py', 'local_transfer_patch.py', 'clear_cache_patch.py', 'touch_keyboard_patch.py'):
        run([PYTHON, P / 'tools/i5' / patch], 'prepare-' + patch.removesuffix('.py'), cwd=W)
    controller_inputs, inputsystem_inputs = compile_controller_inputs()
    old_pad_ui, pad_ui = compile_changed('controller.cpp', C / 'source/panorama/input/controller.cpp', target='panorama')
    old_transfer, transfer = compile_changed('servermsghandler.cpp', C / 'source/engine/servermsghandler.cpp', target='engine')
    sdl = C / 'client-build/Frameworks/SDL2-2.0.framework/SDL2-2.0'
    products = [link_original('inputsystem', {
        **inputsystem_inputs, 'Frameworks/SDL2-2.0.framework/SDL2-2.0': sdl}),
        link_original('panorama', {old_pad_ui: pad_ui,
            'Frameworks/SDL2-2.0.framework/SDL2-2.0': sdl})]
    for product in products:
        old = next((row for row in metadata['rebuilt'] if row['path'] == str(product)), None)
        if old: old['sha256'] = digest(product)
        else: metadata['rebuilt'].append({'path': str(product), 'sha256': digest(product)})
        for row in metadata['frameworks']:
            if pathlib.Path(row['path']) == product.parent:
                row['binary_sha256'] = digest(product)
    metadata['controller_sources'] = controller_source_hashes()
    metadata['controller_objects'] = controller_object_records(controller_inputs)
    metadata['controller_module_objects'] = {
        'inputsystem': controller_object_records(inputsystem_inputs),
        'panorama': controller_object_records({old_pad_ui: pad_ui}),
        'engine': controller_object_records({old_transfer: transfer})}
    save(E / 'module-products.json', metadata)
    network_modules()
    room_browser_modules()


def skin_trial_modules():
    """Rebuild the catalog consumers and baked-material adapter only."""
    for name, target in (('c_baseviewmodel.cpp', 'client'), ('weapon_csbase.cpp', 'client'),
                         ('weapon_csbase.cpp', 'server'),
                         ('econ_item_view.cpp', 'client'), ('econ_item_view.cpp', 'server'),
                         ('cs_player.cpp', 'server')):
        relative = 'game/client/' if name == 'c_baseviewmodel.cpp' else 'game/server/cstrike15/' if name == 'cs_player.cpp' else (
            'game/shared/econ/' if name == 'econ_item_view.cpp' else 'game/shared/cstrike15/')
        compile_changed(name, C / 'source' / relative / name, target=target)
    # Retain every previous engine/render/input/profile object in the client.
    inventory_network_modules()
    metadata = json.loads((E / 'module-products.json').read_text())
    replacements = dict(metadata['weapon_dependencies']['targets']['server'])
    for name in ('cs_player.cpp', 'weapon_csbase.cpp', 'econ_item_view.cpp'):
        replacements[compile_entry(name, 'server')['output']] = C / 'objects' / ('server-' + name + '.o')
    if metadata.get('weapon_icons_sources'):
        rules = C / 'objects/server-cs_gamerules.cpp.o'
        require(rules)
        replacements[compile_entry('cs_gamerules.cpp', 'server')['output']] = rules
    server = link_original('server', replacements)
    for row in metadata['rebuilt']:
        if row['path'] == str(server): row['sha256'] = digest(server)
    for row in metadata['frameworks']:
        if pathlib.Path(row['path']) == server.parent: row['binary_sha256'] = digest(server)
    metadata['inventory_sources'] = inventory_source_hashes()
    metadata['recoil_sources'] = recoil_source_hashes()
    metadata['bot_takeover_sources'] = bot_takeover_source_hashes()
    metadata['sticker_sources'] = sticker_source_hashes()
    metadata['sticker_objects'] = controller_object_records({compile_entry('c_baseviewmodel.cpp','client')['output']: C / 'objects/client-c_baseviewmodel.cpp.o'})
    metadata.setdefault('sticker_module_objects', {})['server'] = controller_object_records({compile_entry('econ_item_view.cpp','server')['output']: C / 'objects/server-econ_item_view.cpp.o'})
    save(E / 'module-products.json', metadata)


def weapon_decal_modules():
    """Rebuild only the sticker shader selector; retain all prior shader fixes."""
    run([PYTHON, P / 'tools/i5/weapon_decal_patch.py'], 'prepare-weapon-decal', cwd=W)
    old, new = compile_changed('weapondecal_dx9_helper.cpp',
        C / 'source/materialsystem/stdshaders/weapondecal_dx9_helper.cpp', target='stdshader_dx9')
    replacements = {old: new}
    for name in ('phong_dx9_helper.cpp', 'lightmappedgeneric_dx9_helper.cpp'):
        replacements[compile_entry(name, 'stdshader_dx9')['output']] = C / 'objects' / ('stdshader_dx9-' + name + '.o')
    shader = link_original('stdshader_dx9', replacements)
    metadata = json.loads((E / 'module-products.json').read_text())
    for row in metadata['rebuilt']:
        if row['path'] == str(shader): row['sha256'] = digest(shader)
    for row in metadata['frameworks']:
        if pathlib.Path(row['path']) == shader.parent: row['binary_sha256'] = digest(shader)
    metadata['sticker_sources'] = sticker_source_hashes()
    metadata.setdefault('sticker_module_objects', {})['stdshader_dx9'] = controller_object_records({old: new})
    save(E / 'module-products.json', metadata)


def recoil_modules():
    """Restore shared recoil in both gameplay modules, retaining all I5 fixes."""
    for target in ('client', 'server'):
        compile_changed('weapon_csbase.cpp',
                        C / 'source/game/shared/cstrike15/weapon_csbase.cpp', target=target)
    inventory_network_modules()
    metadata = json.loads((E / 'module-products.json').read_text())
    replacements = dict(metadata['weapon_dependencies']['targets']['server'])
    for name in ('cs_player.cpp', 'weapon_csbase.cpp'):
        replacements[compile_entry(name, 'server')['output']] = C / 'objects' / ('server-' + name + '.o')
    for path in replacements.values():
        require(pathlib.Path(path))
    if metadata.get('weapon_icons_sources'):
        rules = C / 'objects/server-cs_gamerules.cpp.o'
        require(rules)
        replacements[compile_entry('cs_gamerules.cpp', 'server')['output']] = rules
    server = link_original('server', replacements)
    for row in metadata['rebuilt']:
        if row['path'] == str(server): row['sha256'] = digest(server)
    for row in metadata['frameworks']:
        if pathlib.Path(row['path']) == server.parent: row['binary_sha256'] = digest(server)
    metadata['recoil_sources'] = recoil_source_hashes()
    save(E / 'module-products.json', metadata)


def bot_takeover_modules():
    """Relink the server handoff without replacing current client/render fixes."""
    metadata = json.loads((E / 'module-products.json').read_text())
    for row in metadata['rebuilt']:
        assert digest(row['path']) == row['sha256'], row['path']
    compile_changed('cs_player.cpp', C / 'source/game/server/cstrike15/cs_player.cpp', target='server')
    replacements = dict(metadata['weapon_dependencies']['targets']['server'])
    for name in ('cs_player.cpp', 'weapon_csbase.cpp'):
        replacements[compile_entry(name, 'server')['output']] = C / 'objects' / ('server-' + name + '.o')
    for path in replacements.values():
        require(pathlib.Path(path))
    if metadata.get('weapon_icons_sources'):
        rules = C / 'objects/server-cs_gamerules.cpp.o'
        require(rules)
        replacements[compile_entry('cs_gamerules.cpp', 'server')['output']] = rules
    server = link_original('server', replacements)
    for row in metadata['rebuilt']:
        if row['path'] == str(server): row['sha256'] = digest(server)
    for row in metadata['frameworks']:
        if pathlib.Path(row['path']) == server.parent: row['binary_sha256'] = digest(server)
    metadata['bot_takeover_sources'] = bot_takeover_source_hashes()
    save(E / 'module-products.json', metadata)


def ui_resolution_modules():
    """Rebuild the scene/UI boundary, preserving current recoil and bot fixes."""
    for name in ('view.cpp', 'viewrender.cpp'):
        compile_changed(name, C / 'source/game/client' / name, target='client')
    inventory_network_modules()
    metadata = json.loads((E / 'module-products.json').read_text())
    metadata['render_sources'] = {p: digest(p) for p in metadata['render_sources']}
    metadata['ui_resolution_sources'] = ui_resolution_source_hashes()
    save(E / 'module-products.json', metadata)


def weapon_icons_modules():
    """Rebuild identity producers, preserving all current gameplay/render fixes."""
    metadata = json.loads((E / 'module-products.json').read_text())
    for row in metadata['rebuilt']:
        assert digest(row['path']) == row['sha256'], row['path']
    old_rules, rules = compile_changed('cs_gamerules.cpp',
        C / 'source/game/shared/cstrike15/cs_gamerules.cpp', target='server')
    compile_changed('csgo_hudspectator.cpp',
        C / 'source/game/client/cstrike15/panorama/hud/csgo_hudspectator.cpp', target='client')
    replacements = dict(metadata['weapon_dependencies']['targets']['server'])
    for name in ('cs_player.cpp', 'weapon_csbase.cpp'):
        replacements[compile_entry(name, 'server')['output']] = C / 'objects' / ('server-' + name + '.o')
    replacements[old_rules] = rules
    server = link_original('server', replacements)
    for row in metadata['rebuilt']:
        if row['path'] == str(server): row['sha256'] = digest(server)
    for row in metadata['frameworks']:
        if pathlib.Path(row['path']) == server.parent: row['binary_sha256'] = digest(server)
    metadata['weapon_icons_sources'] = weapon_icons_source_hashes()
    save(E / 'module-products.json', metadata)
    inventory_network_modules()


def shadow_submission_modules():
    compile_changed('modelrendersystem.cpp', C / 'source/game/client/modelrendersystem.cpp', target='client')
    inventory_network_modules()
    metadata=json.loads((E / 'module-products.json').read_text())
    paths=tuple(pathlib.Path(p) for p in metadata['render_sources'])+(
        P / 'tools/i5/shadow_submission_patch.py',P / 'render/shadow_submission_ios.inc',
        C / 'source/game/client/modelrendersystem.cpp')
    metadata['render_sources']={str(p):digest(p) for p in paths}
    save(E / 'module-products.json',metadata)


def scene_material_modules():
    """Relink only world-material shaders, preserving independent skin shaders."""
    metadata = json.loads((E / 'module-products.json').read_text())
    for row in metadata['rebuilt']:
        assert digest(row['path']) == row['sha256'], row['path']
    old, changed = compile_changed('lightmappedgeneric_dx9_helper.cpp',
        C / 'source/materialsystem/stdshaders/lightmappedgeneric_dx9_helper.cpp', target='stdshader_dx9')
    phong = C / 'objects/stdshader_dx9-phong_dx9_helper.cpp.o'
    require(phong)
    shader = link_original('stdshader_dx9', {
        old: changed, compile_entry('phong_dx9_helper.cpp','stdshader_dx9')['output']: phong})
    for row in metadata['rebuilt']:
        if row['path'] == str(shader): row['sha256'] = digest(shader)
    for row in metadata['frameworks']:
        if pathlib.Path(row['path']) == shader.parent: row['binary_sha256'] = digest(shader)
    paths = tuple(pathlib.Path(p) for p in metadata['render_sources'] if not p.endswith('/csgo_blurtarget.cpp')) + (P / 'render/map_material_ios.inc',)
    metadata['render_sources'] = {str(p): digest(p) for p in paths}
    save(E / 'module-products.json', metadata)


def scene_quality_modules():
    """Rebuild world budgets, and all Metal consumers of the sampler ABI."""
    compile_changed('c_env_cascade_light.cpp', C / 'source/game/client/c_env_cascade_light.cpp', target='client')
    room_browser_modules()
    metadata = json.loads((E / 'module-products.json').read_text())
    replacements = {}
    for name in ('source_device.mm', 'native_shaders.mm', 'native_resources.mm',
                 'native_window_ios.mm', 'diagnostics_ios.mm'):
        old, new = compile_changed(name, C / 'metal' / name)
        replacements[old] = new
    replacements['Frameworks/SDL2-2.0.framework/SDL2-2.0'] = C / 'client-build/Frameworks/SDL2-2.0.framework/SDL2-2.0'
    replacements['libm6_mojoshader.a'] = C / 'client-build/libm6_mojoshader.a'
    metal = link_original('togl', replacements, extra=['-framework', 'MetalFX'])
    for row in metadata['rebuilt']:
        if row['path'] == str(metal): row['sha256'] = digest(metal)
    for row in metadata['frameworks']:
        if pathlib.Path(row['path']) == metal.parent: row['binary_sha256'] = digest(metal)
    paths = tuple(pathlib.Path(p) for p in metadata['render_sources'] if not p.endswith('/csgo_blurtarget.cpp')) + (
        P / 'tools/i5/shader_sampler_patch.py', P / 'render/shader_shadow_policy.h',
        P / 'tools/i5/scene_quality_patch.py', P / 'render/scene_quality_ios.inc',
        P / 'render/world_sampler_quality_ios.inc', C / 'source/game/client/c_env_cascade_light.cpp')
    metadata['render_sources'] = {str(p): digest(p) for p in paths}
    save(E / 'module-products.json', metadata)


def frame_interpolation_modules():
    """Rebuild Metal consumers without re-deriving unrelated gameplay sources."""
    metadata = json.loads((E / 'module-products.json').read_text())
    for row in metadata['rebuilt']:
        assert digest(row['path']) == row['sha256'], row['path']
    # Runtime shader strings are compiled into source_device.mm. Refresh this
    # derived include together with its MSL source on every incremental build.
    shader = C / 'metal/frame_interpolation_motion_source.inc'
    shader.write_text('static NSString *icsmFrameMotionSource=@' +
                      json.dumps((P / 'render/frame_interpolation_motion.metal').read_text()) + ';\n')
    replacements = {}
    for name in ('source_device.mm', 'native_shaders.mm', 'native_resources.mm',
                 'native_window_ios.mm', 'diagnostics_ios.mm'):
        old, new = compile_changed(name, C / 'metal' / name)
        replacements[old] = new
    replacements['Frameworks/SDL2-2.0.framework/SDL2-2.0'] = C / 'client-build/Frameworks/SDL2-2.0.framework/SDL2-2.0'
    replacements['libm6_mojoshader.a'] = C / 'client-build/libm6_mojoshader.a'
    metal = link_original('togl', replacements, extra=['-framework', 'MetalFX'])
    for row in metadata['rebuilt']:
        if row['path'] == str(metal): row['sha256'] = digest(metal)
    for row in metadata['frameworks']:
        if pathlib.Path(row['path']) == metal.parent: row['binary_sha256'] = digest(metal)
    metadata['render_sources'] = {p: digest(p) for p in metadata['render_sources']}
    metadata['render_sources'][str(shader)] = digest(shader)
    save(E / 'module-products.json', metadata)


def cosmetic_performance_modules():
    """Retain all prior fixes and rebuild the attribute/Metal performance owners."""
    run([PYTHON, P / 'tools/i5/memory_optimization_patch.py'], 'prepare-memory-optimization', cwd=W)
    skin_trial_modules()
    metadata = json.loads((E / 'module-products.json').read_text())
    replacements = {}
    for name in ('source_device.mm', 'native_shaders.mm', 'native_resources.mm',
                 'native_window_ios.mm', 'diagnostics_ios.mm'):
        old, new = compile_changed(name, C / 'metal' / name)
        replacements[old] = new
    replacements['Frameworks/SDL2-2.0.framework/SDL2-2.0'] = C / 'client-build/Frameworks/SDL2-2.0.framework/SDL2-2.0'
    replacements['libm6_mojoshader.a'] = C / 'client-build/libm6_mojoshader.a'
    metal = link_original('togl', replacements, extra=['-framework', 'MetalFX'])
    for row in metadata['rebuilt']:
        if row['path'] == str(metal): row['sha256'] = digest(metal)
    for row in metadata['frameworks']:
        if pathlib.Path(row['path']) == metal.parent: row['binary_sha256'] = digest(metal)
    paths = tuple(pathlib.Path(p) for p in metadata['render_sources'] if not p.endswith('/csgo_blurtarget.cpp')) + (
        P / 'tools/i5/clear_cache_patch.py', P / 'render/clear_rects_ios.inc', P / 'render/clear_selftest_ios.inc', P / 'render/clear_color_policy.h', P / 'render/pass_descriptor_ios.inc', P / 'render/texture_lock_trace_ios.inc', P / 'render/alpha_atlas_selftest_ios.inc', P / 'tools/i5/memory_optimization_patch.py', P / 'render/texture_staging_pool.h', P / 'render/texture_staging_ios.inc', P / 'render/texture_staging_selftest_ios.inc')
    metadata['render_sources'] = {str(p): digest(p) for p in paths}
    save(E / 'module-products.json', metadata)


def application(device=None):
    require(E / 'module-products.json')
    require(P / 'app/i5/resources/video-preset.json')
    packaging_files = (P / 'app/i5/CMakeLists.txt', P / 'app/i5/Info.plist',
                       P / 'app/i5/main.mm', P / 'app/i5/screen_resolution.inc',
                       P / 'app/i5/diagnostics_export.h', P / 'app/i5/diagnostics_export.mm', P / 'app/i5/diagnostics_ui.inc',
                       P / 'app/i5/startup_progress.inc', P / 'app/i5/startup_announcement.inc', P / 'app/i5/announcement_clock.h', P / 'app/i5/data_install.h', P / 'app/i5/data_install.mm',
                       P / 'app/i5/resources/fresh-client-defaults.cfg', P / 'app/i5/resources/distribution-assets.json',
                       P / 'app/i5/resources/fresh-video-preset.json', P / 'app/i5/resources/fresh-video.kv', P / 'app/i5/resources/video-preset.json')
    packaging_files += tuple(sorted(p for p in (P / 'app/i5/resources/native-ui').rglob('*') if p.is_file()))
    packaging_files += tuple(sorted(p for p in (P / 'app/i5/resources/native-navigation').rglob('*') if p.is_file()))
    packaging_files += tuple(sorted(p for p in (P / 'app/i5/resources/local-workshop').rglob('*') if p.is_file()))
    packaging_files += tuple(sorted(p for p in (P / 'app/i5/resources/printstream-trial').rglob('*') if p.is_file()))
    packaging_files += tuple(sorted(p for p in (P / 'vendor/minizip-1.3.1').iterdir() if p.is_file()))
    packaging_files += tuple(sorted(p for p in (P / 'app/i5/Assets.xcassets').rglob('*') if p.is_file()))
    packaging_files += (P / 'app/i5/icon-source.json', P / 'app/i5/startup-source.json',
                        P / 'app/i5/LaunchScreen.storyboard')
    packaging_hashes = {str(path): digest(path) for path in packaging_files}
    # Private signing settings stay in the existing local file and build logs.
    spec = importlib.util.spec_from_file_location('i0_i5_build', P / 'tools/i0.py')
    helper = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(helper)
    config = helper.config()
    if device: config['device'] = device
    app_build = C / 'app-build'
    run([CMAKE, '-S', P / 'app/i5', '-B', app_build, '-G', 'Xcode',
         '-DCMAKE_SYSTEM_NAME=iOS', '-DCMAKE_OSX_SYSROOT=iphoneos',
         '-DCMAKE_OSX_ARCHITECTURES=arm64', '-DCMAKE_OSX_DEPLOYMENT_TARGET=27.0',
         '-DI5_TEAM=' + config['team'], '-DI5_BUNDLE_ID=' + config['bundle_id']],
        'app-configure')
    run(['xcodebuild', '-project', app_build / 'CSGOI5Client.xcodeproj',
         '-scheme', 'CSGOI5Client', '-configuration', 'Release',
         '-destination', 'id=' + config['device'], '-allowProvisioningUpdates',
         '-allowProvisioningDeviceRegistration', 'build'], 'app-build')
    app = app_build / 'Release-iphoneos/CSGOI5Client.app'
    run(['codesign', '--verify', '--deep', '--strict', app], 'app-signature')
    for name, before in packaging_hashes.items():
        if digest(name) != before:
            raise SystemExit('Packaging input changed during build; rebuild before recording: ' + name)
    metadata = plistlib.loads((app / 'Info.plist').read_bytes())
    save(E / 'app-product.json', {'path': str(app),
                                'executable_sha256': digest(app / 'CSGOI5Client'),
                                'app_version': metadata['CFBundleShortVersionString'],
                                'app_build': metadata['CFBundleVersion'],
                                'packaging_sources': packaging_hashes,
                                'profile_sources': json.loads((E / 'module-products.json').read_text())['profile_sources'],
                                'console_sources': json.loads((E / 'module-products.json').read_text()).get('console_sources', {}),
                                'network_sources': json.loads((E / 'module-products.json').read_text()).get('network_sources', {}),
                                'touch_sources': json.loads((E / 'module-products.json').read_text())['touch_sources'],
                                'inventory_sources': json.loads((E / 'module-products.json').read_text())['inventory_sources'],
                                'render_sources': json.loads((E / 'module-products.json').read_text())['render_sources'],
                                'fullscreen_sources': json.loads((E / 'module-products.json').read_text())['fullscreen_sources'],
                                'weapon_dependencies': json.loads((E / 'module-products.json').read_text())['weapon_dependencies'],
                                'recoil_sources': json.loads((E / 'module-products.json').read_text()).get('recoil_sources', {}),
                                'bot_takeover_sources': json.loads((E / 'module-products.json').read_text()).get('bot_takeover_sources', {}),
                                'ui_resolution_sources': json.loads((E / 'module-products.json').read_text()).get('ui_resolution_sources', {}),
                                'weapon_icons_sources': json.loads((E / 'module-products.json').read_text()).get('weapon_icons_sources', {}),
                                'sticker_sources': json.loads((E / 'module-products.json').read_text()).get('sticker_sources', {}),
                                'sticker_objects': json.loads((E / 'module-products.json').read_text()).get('sticker_objects', {}),
                                'sticker_module_objects': json.loads((E / 'module-products.json').read_text()).get('sticker_module_objects', {}),
                                'controller_sources': json.loads((E / 'module-products.json').read_text()).get('controller_sources', {}),
                                'controller_objects': json.loads((E / 'module-products.json').read_text()).get('controller_objects', {}),
                                'controller_module_objects': json.loads((E / 'module-products.json').read_text()).get('controller_module_objects', {}),
                                'purpose': 'Four-finger touch trial, not I5 acceptance'})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('action', choices=['prepare', 'modules', 'network-modules', 'inventory-network-modules', 'skin-trial-modules', 'weapon-decal-modules', 'recoil-modules', 'bot-takeover-modules', 'ui-resolution-modules', 'weapon-icons-modules', 'room-browser-modules', 'controller-modules', 'scene-material-modules', 'shadow-submission-modules', 'cosmetic-performance-modules', 'scene-quality-modules', 'frame-interpolation-modules', 'build', 'all'])
    parser.add_argument('--device')
    args = parser.parse_args()
    if args.action in ('prepare', 'all'):
        prepare()
    if args.action in ('modules', 'all'):
        modules()
    if args.action == 'network-modules':
        network_modules()
    if args.action == 'inventory-network-modules':
        inventory_network_modules()
    if args.action == 'cosmetic-performance-modules':
        cosmetic_performance_modules()
    if args.action == 'frame-interpolation-modules':
        frame_interpolation_modules()
    if args.action == 'skin-trial-modules':
        skin_trial_modules()
    if args.action == 'weapon-decal-modules':
        weapon_decal_modules()
    if args.action == 'recoil-modules':
        recoil_modules()
    if args.action == 'bot-takeover-modules':
        bot_takeover_modules()
    if args.action == 'ui-resolution-modules':
        ui_resolution_modules()
    if args.action == 'weapon-icons-modules':
        weapon_icons_modules()
    if args.action == 'room-browser-modules':
        room_browser_modules()
    if args.action == 'controller-modules':
        controller_modules()
    if args.action == 'scene-quality-modules':
        scene_quality_modules()
    if args.action == 'scene-material-modules':
        scene_material_modules()
    if args.action == 'shadow-submission-modules':
        shadow_submission_modules()
    if args.action in ('build', 'all'):
        application(args.device)


if __name__ == '__main__':
    main()
