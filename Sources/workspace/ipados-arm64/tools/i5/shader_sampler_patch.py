#!/usr/bin/env python3
"""Keep clothing volume samplers separate from original character shadow maps."""
from pathlib import Path
P=Path(__file__).resolve().parents[2]
C=Path.home()/'Library/Caches/csgo-ipados-arm64/i5'
def apply():
 p=C/'metal/native_shaders.mm';text=p.read_text()
 if '#include "shader_shadow_policy.h"' not in text:
  anchor='#include "native_objects.h"';assert text.count(anchor)==1
  text=text.replace(anchor,anchor+'\n#include "shader_shadow_policy.h"',1)
  start=text.index('static unsigned shadowMask(');end=text.index('static std::string sourceCompatibleMSL',start)
  text=text[:start]+'''static unsigned shadowMask(const char *name, unsigned declared2D) {
    return shaderShadowMask(name,declared2D);
}
'''+text[end:]
  old='''        unsigned declared=0;
        for (int i=0;i<p.sampler_count;++i) declared|=1u<<p.samplers[i].index;
        const unsigned mask=shadowMask(name,declared);'''
  new='''        unsigned declared2D=0;
        for (int i=0;i<p.sampler_count;++i)
            if(p.samplers[i].type==MOJOSHADER_SAMPLER_2D)declared2D|=1u<<p.samplers[i].index;
        const unsigned mask=shadowMask(name,declared2D);'''
  assert text.count(old)==1;text=text.replace(old,new,1);p.write_text(text)
 print('Metal shadow mask uses exact shader families and original 2D declarations')
if __name__=='__main__':apply()
