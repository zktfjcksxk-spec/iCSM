#!/usr/bin/env python3
"""Join GitHub data volumes into iCSM-v2.zip with SHA-256 validation (Windows/macOS/Linux)."""
import argparse
import hashlib
import json
from pathlib import Path
import re


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('index', type=Path, nargs='?', default=Path('iCSM-v2.zip.parts.json'))
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    index = json.loads(args.index.read_text(encoding='utf-8'))
    if index.get('output') not in ('iCSM-v2.zip', 'iCSM-v1.zip') or not index.get('parts'):
        raise ValueError('Invalid data volume index')
    target = args.output or args.index.parent / index['output']
    if target.exists():
        raise FileExistsError('Output already exists: ' + str(target))
    partial = target.with_name(target.name + '.partial')
    digest = hashlib.sha256()
    total = 0
    try:
        with partial.open('xb') as output:
            for number, part in enumerate(index['parts'], 1):
                name = part['name']
                if name != f"{index['output']}.part{number:03d}" or not re.fullmatch('[a-f0-9]{64}', part['sha256']):
                    raise ValueError('Invalid volume name or hash: ' + name)
                source = args.index.parent / name
                if source.stat().st_size != part['size']:
                    raise ValueError('Volume size mismatch: ' + name)
                piece = hashlib.sha256()
                size = 0
                with source.open('rb') as stream:
                    while block := stream.read(1024 * 1024):
                        output.write(block)
                        piece.update(block)
                        digest.update(block)
                        total += len(block)
                        size += len(block)
                if size != part['size'] or piece.hexdigest() != part['sha256']:
                    raise ValueError('Volume SHA-256 mismatch: ' + name)
                print('Verified:', name, flush=True)
        if total != index['size'] or digest.hexdigest() != index['sha256']:
            raise ValueError('Complete ZIP SHA-256/size mismatch')
        partial.replace(target)
    except BaseException:
        # A pre-existing .partial file is not ours; never remove it after an exclusive-open failure.
        if 'output' in locals():
            partial.unlink(missing_ok=True)
        raise
    print('Ready to import:', target, '\nSHA-256:', index['sha256'])


if __name__ == '__main__':
    main()
