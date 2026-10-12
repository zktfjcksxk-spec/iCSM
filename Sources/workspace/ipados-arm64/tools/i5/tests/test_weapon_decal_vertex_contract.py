"""Regress projected world stickers against the original VCS vertex contracts."""
import bz2
import json
import lzma
from pathlib import Path
import struct
import subprocess
import tempfile

P = Path(__file__).resolve().parents[3]
W = P.parent
C = Path.home() / 'Library/Caches/csgo-ipados-arm64/i5'
relative = 'source/materialsystem/stdshaders/weapondecal_dx9_helper.cpp'
before = (C.parent / 'i4' / relative).read_text()
after = (C / relative).read_text()


def selector(text):
    begin = text.index('\tbool bNormalMap')
    end = text.index('\n\t//bool bFlashlight', begin)
    return text[begin:end]


fixture = '''#include <cassert>
struct Var { bool texture; bool IsTexture() const {return texture;} };
struct Info {int m_nNormalMap;};
void Msg(const char*,int) {}
bool before(bool bThirdPerson,int nDecalStyle,bool texture,int index) {
 Var var{texture};Var *params[]={&var};Info info{index};
''' + selector(before) + '''
 return bNormalMap;
}
bool after(bool bThirdPerson,int nDecalStyle,bool texture,int index) {
 Var var{texture};Var *params[]={&var};Info info{index};
''' + selector(after) + '''
 return bNormalMap;
}
int main() {
 for(int style=0;style<=5;++style)for(int texture=0;texture<=1;++texture) {
  assert(after(false,style,texture,0)==before(false,style,texture,0));
  assert(!after(true,style,texture,0));
  assert(!after(true,style,texture,-1));
 }
 assert(before(true,4,true,0)); // original fatal-producing selection
 assert(!after(true,4,true,0));
 assert(after(false,4,true,0)); // first-person emboss is preserved
}
'''
with tempfile.TemporaryDirectory(prefix='icsm-world-sticker-') as directory:
    source = Path(directory) / 'selector.cpp'
    binary = Path(directory) / 'selector'
    source.write_text(fixture)
    subprocess.run(['xcrun', 'clang++', '-std=c++17', '-DSOURCE_IOS=1',
                    '-fsanitize=address,undefined', source, '-o', binary], check=True)
    subprocess.run([binary], check=True)

# Both stages must select their original matching flat-normal permutations.
assert after.count('SET_STATIC_VERTEX_SHADER_COMBO( NORMALMAP, bNormalMap )') == 2
assert after.count('SET_STATIC_PIXEL_SHADER_COMBO( NORMALMAP, bNormalMap )') == 2
index_header = (C.parent / 'i4/source/materialsystem/stdshaders/fxctmp9/weapondecal_vs30.inc').read_text()
assert '( 20 * m_nMIRROR ) + ( 40 * m_nNORMALMAP )' in index_header
vcs = W / 'game-assets/platform/shaders/fxc/weapondecal_vs30.vcs'
raw = vcs.read_bytes()
version,total,dynamic,flags,centroid,count,crc = struct.unpack_from('<iiiIIII',raw)
assert version in (5,6) and dynamic == 20
records = [struct.unpack_from('<II',raw,28+8*i) for i in range(count)]
checked = []
for static,start in records[:-1]:
    cursor = start
    while True:
        tag, = struct.unpack_from('<I',raw,cursor); cursor += 4
        if tag == 0xffffffff: break
        kind,size = tag & 0xc0000000, tag & 0x3fffffff
        block = raw[cursor:cursor+size]; cursor += size
        if kind == 0: data = bz2.decompress(block)
        elif kind == 0x80000000: data = block
        elif kind == 0x40000000:
            marker,actual,packed,props = struct.unpack_from('<4sII5s',block)
            assert marker == b'LZMA'
            prop=props[0];lc=prop%9;prop//=9;lp=prop%5;pb=prop//5
            decoder=lzma.LZMADecompressor(format=lzma.FORMAT_RAW,filters=[dict(
                id=lzma.FILTER_LZMA1,dict_size=int.from_bytes(props[1:],'little'),lc=lc,lp=lp,pb=pb)])
            data=decoder.decompress(block[17:],max_length=actual)
            assert len(data)==actual
        else: raise AssertionError(kind)
        offset=0
        while offset<len(data):
            combo,size=struct.unpack_from('<II',data,offset);offset+=8
            code=data[offset:offset+size];offset+=size
            words=struct.unpack('<'+'I'*(len(code)//4),code)
            usages=[];pos=1
            while pos<len(words):
                token=words[pos];pos+=1;op=token&0xffff
                if op==0xffff:break
                if op==0xfffe:pos+=(token>>16)&0x7fff;continue
                length=(token>>24)&15
                if op==31 and length==2:usages.append(words[pos]&0xf)
                pos+=length
            # VCS records store the static-combo quotient, unlike GetIndex().
            normalmap = (static // 2) % 2
            # COMPRESSED_VERTS packs the tangent into the normal and does not
            # declare a separate TANGENT input (common_vs_fxc.h).
            compressed = (combo // 2) % 2
            assert (6 in usages) == bool(normalmap and not compressed), (static,combo,usages)
            checked.append((static,combo))
assert {s for s,d in checked} == {0,1,2,3}
assert any(s==2 and d==4 for s,d in checked) # exact captured crash combo
print('PASS: original world-sticker failure reproduced in selector; first-person unchanged; '
      f'{len(checked)} original VS variants verified, both stages select matching NORMALMAP combos')
