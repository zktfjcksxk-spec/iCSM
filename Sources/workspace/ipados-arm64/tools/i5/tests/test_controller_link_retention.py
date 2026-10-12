#!/usr/bin/env python3
"""Run the actual incremental linker recipe against overlapping recorded inputs.

Regresses the case where a frozen-I4 weapon header dependent silently displaced
the newer controller object, despite valid source and object hashes.
"""
import ast
import hashlib
import json
from pathlib import Path
import shlex
import tempfile
import types

P = Path(__file__).resolve().parents[3]
tree = ast.parse((P / 'tools/i5/build.py').read_text())
function = next(n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name == 'link_original')

with tempfile.TemporaryDirectory(prefix='icsm-link-retention-') as tmp:
    C = Path(tmp)
    E = C / 'provenance'
    E.mkdir()
    originals = ['CMakeFiles/client.dir/in_joystick.cpp.o',
                 'CMakeFiles/client.dir/csgo_settings_keybinder.cpp.o',
                 'CMakeFiles/client.dir/c_baseviewmodel.cpp.o']
    def digest(path):
        return hashlib.sha256(Path(path).read_bytes()).hexdigest()
    def record(path, text):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)
        return {'path': str(path), 'sha256': digest(path)}
    patched = {old: record(C / 'objects' / Path(old).name, 'patched-' + old) for old in originals}
    generic = {old: record(C / 'objects/weapon-dependencies' / old, 'frozen-' + old)['path'] for old in originals}
    metadata = {'controller_objects': {old: patched[old] for old in originals[:2]},
                'sticker_objects': {originals[2]: patched[originals[2]]}}
    (E / 'module-products.json').write_text(json.dumps(metadata))
    argv = [':', '&&', 'clang++', '-o', 'Frameworks/client_panorama.framework/client_panorama', *originals, '&&', ':']
    captured = []
    def run(command, *args, **kwargs):
        captured.append(command)
        output = Path(command[command.index('-o') + 1])
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text('linked')
    def save(path, data):
        path.write_text(json.dumps(data))
    ns = dict(pathlib=__import__('pathlib'), C=C, E=E, digest=digest,
              subprocess=types.SimpleNamespace(run=lambda *a, **k: types.SimpleNamespace(stdout=shlex.join(argv)+'\n')),
              NINJA='ninja', OLD_BUILD=C, shlex=shlex, json=json, run=run, save=save)
    exec(compile(ast.Module(body=[function], type_ignores=[]), '<actual link_original>', 'exec'), ns)
    # The exact regression: explicit generic dependency inputs used to win.
    ns['link_original']('client', generic)
    assert all(row['path'] in captured[-1] and generic[old] not in captured[-1] for old, row in patched.items())
    inputs = json.loads((E / 'link-inputs-client.json').read_text())['replacements']
    assert all(inputs[old] == row for old, row in patched.items())
    # An intentional newly compiled patch still wins over retained metadata.
    fresh = record(C / 'objects/fresh-in_joystick.cpp.o', 'new-patched-source')['path']
    ns['link_original']('client', {**generic, originals[0]: fresh})
    assert fresh in captured[-1] and patched[originals[0]]['path'] not in captured[-1]
    # Full original input list without explicit replacements also keeps fixes.
    ns['link_original']('client', {})
    assert all(row['path'] in captured[-1] for row in patched.values())
    print('PASS: controller and viewmodel fixes survive overlapping frozen weapon dependencies; fresh patched inputs retain priority; actual link inputs recorded')
