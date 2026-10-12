from pathlib import Path
import tempfile,subprocess
P=Path(__file__).resolve().parents[3]
code=r'''
#include "clear_color_policy.h"
#include <cassert>
#include <cmath>
#include <cstdio>
int main(){
 unsigned accepted=0;
 for(unsigned alpha=0;alpha<256;++alpha)for(unsigned corner=0;corner<8;++corner){
  auto c=alpha<<24|(corner&1?255:0)|(corner&2?255<<8:0)|(corner&4?255<<16:0);
  assert(ICSMClearColorIsSRGBInvariant(c));++accepted;
 }
 for(unsigned x=1;x<255;++x)for(unsigned shift=0;shift<24;shift+=8)
  assert(!ICSMClearColorIsSRGBInvariant(x<<shift));
 puts("PASS: all 2048 endpoint/alpha colors preserve sRGB bytes; intermediate RGB values retain original switching");
}
'''
with tempfile.TemporaryDirectory() as t:
 p=Path(t);(p/'test.cpp').write_text(code)
 subprocess.run(['xcrun','clang++','-std=c++17','-O2','-I',str(P/'render'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
