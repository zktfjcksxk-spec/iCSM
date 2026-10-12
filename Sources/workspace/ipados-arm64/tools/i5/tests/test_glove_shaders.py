"""Translate every original clothing combo through the actual Metal adapter.

VCS blocks follow shader_vcs_version.h / CreateDynamicCombos_Ver5. This is an
offline compiler test; it neither starts nor operates a device application.
"""
import argparse
import bz2
import concurrent.futures
import hashlib
import json
import lzma
import struct
import subprocess
from pathlib import Path

P = Path(__file__).resolve().parents[3]
W = P.parent
C = Path.home() / 'Library/Caches/csgo-ipados-arm64/i5'
parser = argparse.ArgumentParser()
parser.add_argument('--before', type=Path)
args = parser.parse_args()
out = C / 'glove-shader-regression-v169'
out.mkdir(parents=True, exist_ok=True)

def run(command):
    result = subprocess.run([str(x) for x in command], capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(f'{command}\n{result.stdout}\n{result.stderr}')
    return result.stdout.strip()

def adapter(path):
    source = path.read_text()
    return source[source.index('static unsigned bytecodeSize('):
                  source.index('std::unique_ptr<Shader> newShader(')]

prefix = r'''
#import <Foundation/Foundation.h>
#include "mojoshader.h"
#include "shader_shadow_policy.h"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <stdexcept>
#include <vector>
using DWORD=uint32_t; using uint32=uint32_t;
static void fatal(const char *operation, NSString *details) {
    throw std::runtime_error(std::string(operation)+": "+details.UTF8String);
}
static const char *V_stristr(const char *s,const char *needle) {
    for(;*s;++s) {
        size_t n=0;
        while(needle[n] && s[n] && std::tolower((unsigned char)s[n])==std::tolower((unsigned char)needle[n]))++n;
        if(!needle[n])return s;
    }
    return nullptr;
}
'''
legacy = ''
if args.before:
    legacy = 'namespace Before {\n' + adapter(args.before) + '\n}\n'
main = r'''
int main(int argc,char **argv) { @autoreleasepool {
    assert(argc==5);
    using SourceMetal::shaderShadowMask;
    assert(shaderShadowMask("customcharacter_ps30",0xffff)==0);
    assert(shaderShadowMask("fxc/CUSTOMCHARACTER_PS30.vcs",0xffff)==0);
    assert(shaderShadowMask("othercharacter_ps30",0xffff)==0);
    assert(shaderShadowMask("character_ps_wrong",0xffff)==0);
    assert(shaderShadowMask("fxc/CHARACTER_PS30.vcs",0xffff)==(1u<<11));
    assert(shaderShadowMask("character_ps30",1u<<8)==(1u<<8));
    assert(shaderShadowMask("character_ps30",1u<<11)==(1u<<11));
    assert(shaderShadowMask("character_ps30",0)==0);
    assert(shaderShadowMask("lightmappedgeneric_ps30",0xffff)==(1u<<15));
    assert(shaderShadowMask("phong_ps30",0xffff)==((1u<<4)|(1u<<15)));
    assert(shaderShadowMask("water_ps20b",0xffff)==(1u<<7));
    std::ifstream stream(argv[2],std::ios::binary|std::ios::ate);
    assert(stream); const auto length=stream.tellg(); assert(length>0 && length%4==0);
    std::vector<DWORD> original(size_t(length)/4); stream.seekg(0);
    stream.read(reinterpret_cast<char*>(original.data()),length); assert(stream);
    auto parseCode=original;
    std::vector<std::string> centroidDecls;
    for(unsigned cursor=1;cursor<parseCode.size();) {
        const unsigned tok=parseCode[cursor++],op=tok&0xffff;
        if(op==0xffff)break;
        if(op==0xfffe){cursor+=(tok>>16)&0x7fff;continue;}
        const unsigned count=(tok>>24)&15;
        if(op==31 && count==2 && (parseCode[cursor+1]&(4u<<20))) {
            auto &dest=parseCode[cursor+1]; unsigned type=((dest>>28)&7)|((dest>>8)&0x18);
            assert(type==1 || type==3);
            centroidDecls.push_back(std::string(type==3?"t":"v")+std::to_string(dest&0x7ff));
            dest&=~(4u<<20);
        }
        cursor+=count;
    }
    const auto *p=MOJOSHADER_parse(MOJOSHADER_PROFILE_METAL,"source_main",
        reinterpret_cast<const unsigned char*>(parseCode.data()),unsigned(length),nullptr,0,nullptr,0,nullptr,nullptr,nullptr);
    for(int i=0;i<p->error_count;++i)std::cerr<<p->errors[i].error<<"\n";
    assert(p->error_count==0);
    const uint32 centroid=std::stoul(argv[4]);
    bool volume11=false,oldFailure=false;
    for(int i=0;i<p->sampler_count;++i)
        if(p->samplers[i].index==11 && p->samplers[i].type==MOJOSHADER_SAMPLER_VOLUME)volume11=true;
#ifdef TEST_LEGACY
    if(p->shader_type==MOJOSHADER_TYPE_PIXEL && volume11) {
        try { Before::sourceCompatibleMSL(original.data(),*p,argv[1],&centroid,centroidDecls); }
        catch(const std::runtime_error &error) {
            assert(std::string(error.what())=="shadow sampler: expected 2D texture declaration");oldFailure=true;
        }
        assert(oldFailure);
    }
#endif
    const auto msl=After::sourceCompatibleMSL(original.data(),*p,argv[1],&centroid,centroidDecls);
    assert(msl.find("depth2d<")==std::string::npos);
    assert(msl.find("sample_compare(")==std::string::npos);
    if(volume11)assert(msl.find("texture3d<float> s11_texture")!=std::string::npos);
    std::ofstream output(argv[3]); output<<msl; assert(output);
    MOJOSHADER_freeParseData(p);
    std::cout<<"volume11="<<volume11<<" old_failure="<<oldFailure<<"\n";
} }
'''
source = out / 'adapter-test.mm'
source.write_text(prefix + legacy + 'namespace After {\nusing SourceMetal::shaderShadowMask;\n' +
                  adapter(C / 'metal/native_shaders.mm') + '\n}\n' + main)
mojo = W / 'macos-arm64/vendor/m6/mojoshader'
defines = ['MOJOSHADER_NO_VERSION_INCLUDE', 'SUPPORT_PROFILE_METAL=1']
defines += ['SUPPORT_PROFILE_' + x + '=0' for x in (
    'ARB1', 'ARB1_NV', 'BYTECODE', 'D3D', 'GLSL120', 'GLSL', 'GLSLES3',
    'GLSLES', 'GLSPIRV', 'HLSL', 'SPIRV')]
objects = []
for relative in ('mojoshader_common.c', 'profiles/mojoshader_profile_common.c'):
    obj = out / (Path(relative).stem + '.o')
    run(['xcrun', 'clang', '-g', '-O1', '-fsanitize=address,undefined', '-I', mojo,
         '-I', mojo / 'profiles',
         *['-D' + x for x in defines], '-c', C / 'mojoshader' / relative, '-o', obj])
    objects.append(obj)
binary = out / 'adapter-test'
run(['xcrun', 'clang++', '-std=c++17', '-g', '-O1', '-fsanitize=address,undefined',
     '-I', mojo, '-I', P / 'render', '-DMOJOSHADER_NO_VERSION_INCLUDE',
     *(['-DTEST_LEGACY'] if args.before else []),
     source, *objects, W / 'macos-arm64/build-m6-tools/libm6_mojoshader.a',
     '-framework', 'Foundation', '-o', binary])

fixtures = []
vcs_hashes = {}
for name in ('customcharacter_ps30', 'customcharacter_vs30'):
    vcs = W / 'game-assets/platform/shaders/fxc' / (name + '.vcs')
    raw = vcs.read_bytes()
    vcs_hashes[str(vcs)] = hashlib.sha256(raw).hexdigest()
    version, total, dynamic, flags, centroid, count, crc = struct.unpack_from('<iiiIIII', raw)
    assert version in (5, 6)
    records = [struct.unpack_from('<II', raw, 28 + 8 * i) for i in range(count)]
    for static, start in records[:-1]:
        cursor = start
        while True:
            tag = struct.unpack_from('<I', raw, cursor)[0]; cursor += 4
            if tag == 0xffffffff:
                break
            kind, size = tag & 0xc0000000, tag & 0x3fffffff
            block = raw[cursor:cursor + size]; cursor += size
            assert len(block) == size
            if kind == 0:
                unpacked = bz2.decompress(block)
            elif kind == 0x80000000:
                unpacked = block
            elif kind == 0x40000000:
                marker, actual, packed, props = struct.unpack_from('<4sII5s', block)
                assert marker == b'LZMA' and packed == len(block) - 17
                prop = props[0]; lc = prop % 9; prop //= 9; lp = prop % 5; pb = prop // 5
                decoder = lzma.LZMADecompressor(format=lzma.FORMAT_RAW, filters=[dict(
                    id=lzma.FILTER_LZMA1, dict_size=int.from_bytes(props[1:], 'little'), lc=lc, lp=lp, pb=pb)])
                unpacked = decoder.decompress(block[17:], max_length=actual)
                assert len(unpacked) == actual
            else:
                raise ValueError('Unsupported Source VCS compression')
            pos = 0
            while pos < len(unpacked):
                combo, size = struct.unpack_from('<II', unpacked, pos); pos += 8
                shader = unpacked[pos:pos + size]; pos += size
                assert len(shader) == size
                dx9 = out / f'{name}-s{static}-d{combo}.dx9'
                dx9.write_bytes(shader)
                fixtures.append((name, dx9, centroid))

def check(fixture):
    name, dx9, centroid = fixture
    msl = dx9.with_suffix('.metal')
    result = run([binary, name, dx9, msl, centroid])
    run(['xcrun', '-sdk', 'iphoneos', 'metal', '-target', 'air64-apple-ios27.0',
         '-std=metal3.2', '-fno-fast-math', '-c', msl, '-o', dx9.with_suffix('.air')])
    return dict(shader=dx9.name, sha256=hashlib.sha256(dx9.read_bytes()).hexdigest(),
                checks=result, metal_compile='passed')

with concurrent.futures.ThreadPoolExecutor(max_workers=6) as pool:
    results = list(pool.map(check, fixtures))
report = dict(status='passed', source_vcs_sha256=vcs_hashes, variants=len(results),
              reproduced_old_fatal=sum('old_failure=1' in r['checks'] for r in results),
              fixed_adapter_sha256=hashlib.sha256((C / 'metal/native_shaders.mm').read_bytes()).hexdigest(),
              policy_sha256=hashlib.sha256((P / 'render/shader_shadow_policy.h').read_bytes()).hexdigest(),
              fixtures=results, live_device_test='not run; reserved for user')
(out / 'results.json').write_text(json.dumps(report, indent=2) + '\n')
print(f'PASS: {len(results)} original clothing variants translated and compiled for iOS; '
      f'old fatal reproduced on {report["reproduced_old_fatal"]} pixel variants. Results: {out / "results.json"}')
