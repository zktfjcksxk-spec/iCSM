#!/usr/bin/env python3
"""Compile Source-adapted MSL and assemble matching mobile data from GitHub inputs."""
import argparse
import concurrent.futures
import gzip
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import re
import subprocess
import tarfile
import tempfile
import time
import zipfile

ROOT = Path(__file__).resolve().parent.parent
DATA = ROOT / 'Data'
APP_MANIFEST = ROOT / 'Sources/workspace/ipados-arm64/app/i5/resources/distribution-assets.json'
BLOCK = 1024 * 1024
PART_BYTES = 1024**3


def require(condition, message):
    if not condition:
        raise ValueError(message)


def read_json(path):
    return json.loads(path.read_text(encoding='utf-8'))


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + '.partial')
    temporary.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    temporary.replace(path)


def file_hash(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        while block := stream.read(BLOCK):
            digest.update(block)
    return digest.hexdigest()


def checked_path(name, prefix):
    path = PurePosixPath(name)
    require(not path.is_absolute() and '..' not in path.parts and '\\' not in name
            and path.as_posix() == name and name.startswith(prefix), 'Unsafe input path: ' + name)
    return name


def run(command):
    result = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if result.returncode:
        raise RuntimeError('Command failed: ' + ' '.join(map(str, command)) + '\n' + result.stdout[-12000:])
    return result.stdout.strip()


def shaders(args):
    catalog = read_json(DATA / 'shaders.json')
    keys = catalog['keys']
    require(keys and len(keys) == len(set(keys)), 'Duplicate or missing shader keys')
    require(all(re.fullmatch('[a-f0-9]{64}', key) for key in keys), 'Invalid MSL source key')
    require({p.stem for p in (DATA / 'shaders').glob('*.metal')} == set(keys), 'MSL source set differs')
    sdk = run(['xcrun', '--sdk', 'iphoneos', '--show-sdk-path'])
    sdk_version = run(['xcrun', '--sdk', 'iphoneos', '--show-sdk-version'])
    require(sdk_version.split('.')[0] == '27', 'This mobile release requires the iPhoneOS 27.x SDK')
    output = args.output.resolve()
    library_dir = output / 'shader-cache'
    library_dir.mkdir(parents=True, exist_ok=True)

    def compile_one(key):
        source = DATA / 'shaders' / (key + '.metal')
        require(file_hash(source) == key, 'MSL source does not match native cache key: ' + key)
        with tempfile.TemporaryDirectory(prefix='icsm-metal-') as temporary:
            air = Path(temporary) / (key + '.air')
            library = Path(temporary) / (key + '.metallib')
            run(['xcrun', '--sdk', 'iphoneos', 'metal', '-target', catalog['target'],
                 *catalog['options'], '-isysroot', sdk, '-c', str(source), '-o', str(air)])
            run(['xcrun', '--sdk', 'iphoneos', 'metallib', str(air), '-o', str(library)])
            require(library.stat().st_size > 0, 'Empty Metal library: ' + key)
            record = dict(path='shader-cache/' + library.name, size=library.stat().st_size,
                          sha256=file_hash(library))
            library.replace(library_dir / library.name)
            return record

    libraries = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        futures = [pool.submit(compile_one, key) for key in keys]
        for future in concurrent.futures.as_completed(futures):
            libraries.append(future.result())
            if len(libraries) % 100 == 0 or len(libraries) == len(keys):
                print(f'Metal libraries {len(libraries)}/{len(keys)}', flush=True)
    resources = read_json(DATA / 'resources.json')
    files = sorted(resources['files'] + libraries, key=lambda item: item['path'])
    require(len(files) == len({item['path'] for item in files}), 'Duplicate distribution paths')
    # This is the exact canonical identity used by the existing mobile importer.
    identity = hashlib.sha256(json.dumps(files, sort_keys=True, separators=(',', ':')).encode()).hexdigest()
    manifest = dict(version=1, identity=identity, files=files,
                    total_bytes=sum(item['size'] for item in files), shader_count=len(libraries))
    write_json(output / 'distribution-assets.json', manifest)
    write_json(APP_MANIFEST, manifest)
    write_json(output / 'shader-build.json', dict(source_commit=os.environ.get('GITHUB_SHA', ''),
               sdk=sdk_version, target=catalog['target'], options=catalog['options'],
               count=len(libraries), data_identity=identity))
    print('Matching app/data identity:', identity, flush=True)


class ReleaseStream(io.RawIOBase):
    """Read one verified GitHub input volume at a time, deleting consumed volumes."""
    def __init__(self, index, repository, cache):
        super().__init__()
        require(re.fullmatch(r'[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+', repository), 'Invalid repository')
        self.index = index
        self.repository = repository
        self.cache = cache
        self.cache.mkdir(parents=True, exist_ok=True)
        self.iterator = iter(index['parts'])
        self.stream = None
        self.path = None
        self.digest = hashlib.sha256()
        self.size = 0
        self.complete = False

    def readable(self):
        return True

    def next_part(self):
        if self.stream:
            self.stream.close()
            self.path.unlink()
            self.stream = None
        part = next(self.iterator, None)
        if part is None:
            require(self.size == self.index['archive_bytes']
                    and self.digest.hexdigest() == self.index['archive_sha256'],
                    'Resource archive SHA-256/size mismatch')
            self.complete = True
            return False
        require(re.fullmatch(r'resources\.tar\.gz\.part[0-9]{3}', part['name']), 'Invalid volume name')
        self.path = self.cache / part['name']
        for attempt in range(3):
            try:
                if not (self.path.is_file() and self.path.stat().st_size == part['size']
                        and file_hash(self.path) == part['sha256']):
                    run(['gh', 'release', 'download', self.index['tag'], '--repo', self.repository,
                         '--pattern', part['name'], '--dir', str(self.cache), '--clobber'])
                require(self.path.stat().st_size == part['size']
                        and file_hash(self.path) == part['sha256'], 'Input volume checksum mismatch')
                break
            except (RuntimeError, ValueError, OSError):
                if attempt == 2:
                    raise
                time.sleep(2**attempt)
        print('Verified input volume:', part['name'], flush=True)
        self.stream = self.path.open('rb')
        return True

    def readinto(self, buffer):
        while not self.complete:
            if self.stream is None and not self.next_part():
                break
            size = self.stream.readinto(buffer)
            if size:
                self.digest.update(memoryview(buffer)[:size])
                self.size += size
                return size
            self.next_part()
        return 0

    def close(self):
        if self.stream:
            self.stream.close()
        super().close()


class VolumeWriter(io.RawIOBase):
    """Seekable ordinary ZIP byte stream split into transport volumes, not a spanned ZIP."""
    def __init__(self, directory, name):
        super().__init__()
        self.directory = directory
        directory.mkdir(parents=True, exist_ok=True)
        require(not list(directory.glob(name + '.part*')), 'Output volumes already exist')
        self.name = name
        self.position = 0
        self.length = 0
        self.streams = []

    def writable(self):
        return True

    def seekable(self):
        return True

    def tell(self):
        return self.position

    def seek(self, offset, whence=0):
        require(whence in (0, 1, 2), 'Invalid seek origin')
        position = offset if whence == 0 else (self.position if whence == 1 else self.length) + offset
        require(0 <= position <= self.length, 'Invalid ZIP seek')
        self.position = position
        return position

    def write(self, data):
        amount = len(data)
        data = memoryview(data)
        while data:
            index, offset = divmod(self.position, PART_BYTES)
            if index == len(self.streams):
                path = self.directory / f'{self.name}.part{index+1:03d}'
                self.streams.append(path.open('w+b'))
            stream = self.streams[index]
            stream.seek(offset)
            block = data[:PART_BYTES - offset]
            stream.write(block)
            self.position += len(block)
            self.length = max(self.length, self.position)
            data = data[len(block):]
        return amount

    def flush(self):
        for stream in self.streams:
            if not stream.closed:
                stream.flush()

    def close(self):
        self.flush()
        for stream in self.streams:
            stream.close()
        super().close()

    def index(self):
        self.flush()
        parts = []
        digest = hashlib.sha256()
        total = 0
        for stream in self.streams:
            path = Path(stream.name)
            part_digest = hashlib.sha256()
            with path.open('rb') as source:
                while block := source.read(BLOCK):
                    digest.update(block)
                    part_digest.update(block)
                    total += len(block)
            parts.append(dict(name=path.name, size=path.stat().st_size, sha256=part_digest.hexdigest()))
        require(total == self.length, 'ZIP volume size mismatch')
        return dict(version=1, output=self.name, parts=parts, size=total, sha256=digest.hexdigest())


class VolumeReader(io.RawIOBase):
    """Seek across transport parts for cloud ZIP-directory/CRC validation."""
    def __init__(self, directory, index):
        super().__init__()
        self.directory = directory
        self.index = index
        self.position = 0
        self.number = None
        self.stream = None

    def readable(self):
        return True

    def seekable(self):
        return True

    def tell(self):
        return self.position

    def seek(self, offset, whence=0):
        require(whence in (0, 1, 2), 'Invalid seek origin')
        position = offset if whence == 0 else (self.position if whence == 1 else self.index['size']) + offset
        require(0 <= position <= self.index['size'], 'Invalid ZIP read seek')
        self.position = position
        return position

    def readinto(self, buffer):
        written = 0
        while written < len(buffer) and self.position < self.index['size']:
            number, offset = divmod(self.position, PART_BYTES)
            if self.number != number:
                if self.stream:
                    self.stream.close()
                self.stream = (self.directory / self.index['parts'][number]['name']).open('rb')
                self.number = number
            self.stream.seek(offset)
            limit = min(len(buffer) - written, self.index['parts'][number]['size'] - offset)
            amount = self.stream.readinto(memoryview(buffer)[written:written+limit])
            require(amount > 0, 'Truncated output ZIP volume')
            self.position += amount
            written += amount
        return written

    def close(self):
        if self.stream:
            self.stream.close()
        super().close()


def add_zip_file(archive, record, source):
    name = record['path']
    info = zipfile.ZipInfo(name)
    info.compress_type = zipfile.ZIP_DEFLATED
    info.external_attr = 0o100644 << 16
    info._compresslevel = 1
    digest = hashlib.sha256()
    size = 0
    with archive.open(info, 'w', force_zip64=True) as target:
        while block := source.read(BLOCK):
            digest.update(block)
            size += len(block)
            require(size <= record['size'], 'Resource larger than manifest: ' + name)
            target.write(block)
    require(size == record['size'] and digest.hexdigest() == record['sha256'],
            'Resource differs from matching app manifest: ' + name)


def package(args):
    index = read_json(DATA / 'build.json')
    resources = read_json(DATA / 'resources.json')
    manifest = read_json(args.shaders / 'distribution-assets.json')
    raw = {checked_path(item['path'], 'game-assets/'): item for item in resources['files']}
    require(len(raw) == resources['file_count'] == index['file_count'], 'Resource catalog count mismatch')
    require(sum(item['size'] for item in raw.values()) == resources['total_bytes'] == index['total_bytes'],
            'Resource catalog size mismatch')
    libraries = {checked_path(item['path'], 'shader-cache/'): item for item in manifest['files']
                 if item['path'].startswith('shader-cache/')}
    require(len(libraries) == manifest['shader_count'] == len(read_json(DATA / 'shaders.json')['keys']),
            'Compiled shader count mismatch')
    files = sorted(list(raw.values()) + list(libraries.values()), key=lambda item: item['path'])
    identity = hashlib.sha256(json.dumps(files, sort_keys=True, separators=(',', ':')).encode()).hexdigest()
    require(files == manifest['files'] and identity == manifest['identity'], 'App/data manifest mismatch')
    seen = set()
    with VolumeWriter(args.output, 'iCSM-v2.zip') as volumes:
        with zipfile.ZipFile(volumes, 'w', allowZip64=True) as output:
            with ReleaseStream(index, args.repository or index['repository'], args.cache) as source:
                with io.BufferedReader(source, buffer_size=BLOCK) as buffered:
                    with gzip.GzipFile(fileobj=buffered, mode='rb') as compressed:
                        with tarfile.open(fileobj=compressed, mode='r|') as archive:
                            for member in archive:
                                name = checked_path(member.name, 'game-assets/')
                                require(member.isfile() and name in raw and name not in seen
                                        and member.size == raw[name]['size'], 'Unexpected resource: ' + name)
                                with archive.extractfile(member) as stream:
                                    add_zip_file(output, raw[name], stream)
                                seen.add(name)
                                if len(seen) % 100 == 0:
                                    print(f'Packed resources {len(seen)}/{len(raw)}', flush=True)
                        # Consume the trailer to check gzip CRC and the entire input archive hash.
                        while compressed.read(BLOCK):
                            pass
                    require(source.complete, 'Resource archive was not completely consumed')
            require(seen == set(raw), 'Missing resource files')
            for name, record in sorted(libraries.items()):
                path = args.shaders / name
                with path.open('rb') as stream:
                    add_zip_file(output, record, stream)
        result = volumes.index()
    # This check runs in the cloud data job, without creating a second full-size ZIP.
    with VolumeReader(args.output, result) as reader:
        with zipfile.ZipFile(reader) as archive:
            entries = archive.infolist()
            require(len(entries) == len(files) and {entry.filename for entry in entries} ==
                    {item['path'] for item in files}, 'Output ZIP member set differs')
            sizes = {item['path']: item['size'] for item in files}
            require(all(entry.file_size == sizes[entry.filename] for entry in entries),
                    'Output ZIP resource size differs')
            require(archive.testzip() is None, 'Output ZIP CRC validation failed')
    print('Cloud ZIP directory and CRC validation complete', flush=True)
    result.update(data_identity=identity, files=len(files), shader_count=len(libraries),
                  source_commit=os.environ.get('GITHUB_SHA', ''))
    write_json(args.output / 'iCSM-v2.zip.parts.json', result)
    write_json(args.output / 'distribution-assets.json', manifest)
    sums = [f"{item['sha256']}  {item['name']}" for item in result['parts']]
    sums.append(f"{result['sha256']}  {result['output']}")
    (args.output / 'DATA-SHA256SUMS.txt').write_text('\n'.join(sums) + '\n', encoding='utf-8')
    print('DATA PACKAGE READY', json.dumps(result), flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    compile_parser = commands.add_parser('shaders', help='Compile Metal sources and update the app manifest')
    compile_parser.add_argument('--output', type=Path, default=ROOT / 'Build/data')
    compile_parser.add_argument('--jobs', type=int, choices=range(1, 9), default=2)
    compile_parser.set_defaults(function=shaders)
    package_parser = commands.add_parser('package', help='Stream GitHub resources into transport ZIP volumes')
    package_parser.add_argument('--shaders', type=Path, default=ROOT / 'Build/data')
    package_parser.add_argument('--output', type=Path, default=ROOT / 'Build/release')
    package_parser.add_argument('--cache', type=Path, default=ROOT / 'Build/resource-inputs')
    package_parser.add_argument('--repository', help='Repository holding the immutable resource input release')
    package_parser.set_defaults(function=package)
    args = parser.parse_args()
    args.function(args)


if __name__ == '__main__':
    main()
