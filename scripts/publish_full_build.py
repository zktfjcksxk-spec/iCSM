#!/usr/bin/env python3
"""Publish one paired unsigned IPA and data package to the workflow's repository."""
import argparse
import concurrent.futures
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import time

ROOT = Path(__file__).resolve().parent.parent


def run(command, check=True):
    result = subprocess.run(command, text=True, capture_output=True)
    if check and result.returncode:
        raise RuntimeError(result.stderr or result.stdout)
    return result


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        while block := stream.read(1024 * 1024):
            h.update(block)
    return h.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--directory', type=Path, default=ROOT / 'Build/release')
    args = parser.parse_args()
    destination = args.directory
    repository = os.environ['GITHUB_REPOSITORY']
    run_id, attempt = os.environ['GITHUB_RUN_ID'], os.environ['GITHUB_RUN_ATTEMPT']
    if not re.fullmatch(r'\d+', run_id) or not re.fullmatch(r'\d+', attempt):
        raise ValueError('Invalid workflow run ID')
    tag = f'mobile-build-{run_id}-{attempt}'
    index = json.loads((destination / 'iCSM-v2.zip.parts.json').read_text())
    report = json.loads((ROOT / 'Build/ipa-report.json').read_text())
    manifest = json.loads((destination / 'distribution-assets.json').read_text())
    if not index['data_identity'] == report['data_identity'] == manifest['identity']:
        raise ValueError('IPA/data identity mismatch')
    for name in ['iCSM-unsigned.ipa', 'iCSM.entitlements', 'ipa-report.json']:
        shutil.copyfile(ROOT / 'Build' / name, destination / name)
    if digest(destination / 'iCSM-unsigned.ipa') != report['sha256']:
        raise ValueError('IPA checksum differs from verification report')
    shutil.copyfile(ROOT / 'scripts/join_data.py', destination / 'join_data.py')
    shutil.copyfile(ROOT / 'Build/data/shader-build.json', destination / 'shader-build.json')
    files = sorted(path for path in destination.iterdir() if path.is_file())
    # Existing large data volume hashes were computed by the packager; compare before upload.
    known = {part['name']: part for part in index['parts']}
    checksums = []
    for path in files:
        sha256 = digest(path)
        if path.name in known and (path.stat().st_size != known[path.name]['size']
                                   or sha256 != known[path.name]['sha256']):
            raise ValueError('Data volume changed: ' + path.name)
        checksums.append(sha256 + '  ' + path.name)
    (destination / 'SHA256SUMS.txt').write_text('\n'.join(checksums) + '\n')
    files.append(destination / 'SHA256SUMS.txt')
    notes = (
        f"iCSM {report['version']} (Build {report['build']}), iPhone/iPad ARM64, iOS/iPadOS 27+.\n\n"
        'This release contains a paired app and data export built entirely from GitHub inputs. '
        'Re-sign iCSM-unsigned.ipa with your own Apple account before installation.\n\n'
        'Download every iCSM-v2.zip.partNNN file, iCSM-v2.zip.parts.json and join_data.py into one folder. '
        'Run `python join_data.py` on Windows or `python3 join_data.py` on macOS/Linux. '
        'Import the resulting iCSM-v2.zip without extracting it. Use the IPA and data from this same release.\n\n'
        '下载全部分卷、parts.json 和 join_data.py 到同一文件夹，运行脚本合并为 iCSM-v2.zip；'
        '用自己的 Apple 账户重签名 IPA，再在游戏初始页导入 ZIP。IPA 与数据包必须来自本次同一构建。\n\n'
        f"Source: {os.environ['GITHUB_SHA']}\nData identity: {index['data_identity']}\n"
        f"Workflow: https://github.com/{repository}/actions/runs/{run_id}\n"
    )
    notes_path = ROOT / 'Build/full-release-notes.md'
    notes_path.write_text(notes, encoding='utf-8')
    existing = run(['gh', 'release', 'view', tag, '--repo', repository, '--json', 'isDraft'], check=False)
    if existing.returncode == 0:
        if not json.loads(existing.stdout)['isDraft']:
            raise ValueError('This build release is already published; rerun with a new attempt')
    else:
        run(['gh', 'release', 'create', tag, '--repo', repository, '--target', os.environ['GITHUB_SHA'],
             '--draft', '--prerelease', '--latest=false', '--title',
             f"iCSM Build {report['build']} — IPA + Data ({run_id})", '--notes-file', str(notes_path)])

    def upload(path):
        for retry in range(3):
            try:
                run(['gh', 'release', 'upload', tag, str(path), '--repo', repository, '--clobber'])
                print('Uploaded:', path.name, flush=True)
                return
            except RuntimeError:
                if retry == 2:
                    raise
                time.sleep(2**retry)

    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        list(pool.map(upload, files))
    # An interrupted upload leaves a draft; users only see a complete published export.
    run(['gh', 'release', 'edit', tag, '--repo', repository, '--draft=false', '--latest=false'])
    url = f'https://github.com/{repository}/releases/tag/{tag}'
    print('Published:', url)
    summary = os.environ.get('GITHUB_STEP_SUMMARY')
    if summary:
        with open(summary, 'a', encoding='utf-8') as stream:
            stream.write(f'## IPA + data ready\n\n[Download the paired release]({url}).\n\n'
                         'Download all data parts and run `join_data.py`; then sign the IPA and import the ZIP.\n')


if __name__ == '__main__':
    main()
