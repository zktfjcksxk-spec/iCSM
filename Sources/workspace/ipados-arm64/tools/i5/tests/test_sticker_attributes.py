"""Exercise the real attribute writer: uint32 carrier, slot boundaries and removal."""
import subprocess,tempfile
from pathlib import Path
P=Path(__file__).resolve().parents[3]
text=(P/'inventory/offline_stickers.h').read_text();start=text.index('inline void Apply(');body=text[start:text.rfind('}')]
fixture=r'''
#include <map>
#include <string>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <cassert>
#include <algorithm>
using uint32=uint32_t;
#define MIN(a,b) std::min((a),(b))
#define V_snprintf snprintf
#define V_memcpy memcpy
struct Def { int GetNumSupportedStickerSlots() const{return 4;} };
struct List {std::map<std::string,float> values;int invalidations=0;void UpdateManagerCache(){++invalidations;}const float *GetAttributeByName(const char *n)const{auto i=values.find(n);return i==values.end()?nullptr:&i->second;} };
struct CEconItemView {Def def;List list;const Def *GetItemDefinition()const{return &def;}List *GetAttributeList(){return &list;}void SetOrAddAttributeValueByName(const char *n,float v){list.values[n]=v;} };
namespace ICSMStickers {static const int MaxSlots=5;struct Selection {int ids[5]={};};
'''
cases=r'''
}
int main(){CEconItemView item;ICSMStickers::Selection s;
 ICSMStickers::Apply(&item,s);assert(item.list.values.empty());
 s.ids[0]=4503;s.ids[3]=1;s.ids[4]=8;ICSMStickers::Apply(&item,s);assert(item.list.values.size()==8);
 uint32 bits;float v=item.list.values.at("sticker slot 0 id");memcpy(&bits,&v,4);assert(bits==4503);
 assert(item.list.values.at("sticker slot 0 scale")==1);assert(item.list.values.at("sticker slot 0 wear")==0);
 assert(item.list.values.at("sticker slot 0 rotation")==0);assert(!item.list.GetAttributeByName("sticker slot 4 id"));
 const auto count=item.list.values.size();const auto invalidations=item.list.invalidations;
 s.ids[0]=8;ICSMStickers::Apply(&item,s);v=item.list.values.at("sticker slot 0 id");memcpy(&bits,&v,4);assert(bits==8);
 assert(item.list.values.size()==count);assert(item.list.invalidations==invalidations+1);
 s.ids[0]=4503;ICSMStickers::Apply(&item,s);v=item.list.values.at("sticker slot 0 id");memcpy(&bits,&v,4);assert(bits==4503);
 s.ids[0]=0;ICSMStickers::Apply(&item,s);v=item.list.values.at("sticker slot 0 id");memcpy(&bits,&v,4);assert(bits==0);
 v=item.list.values.at("sticker slot 3 id");memcpy(&bits,&v,4);assert(bits==1);
 puts("PASS exact uint32 sticker IDs, occupied-slot replacement and cache invalidation, original transforms, empty slots, supported-slot bounds, removal");}
'''
with tempfile.TemporaryDirectory() as tmp:
 source=Path(tmp)/'test.cpp';source.write_text(fixture+body+cases);binary=Path(tmp)/'test'
 subprocess.run(['xcrun','clang++','-std=c++17','-fsanitize=address,undefined',str(source),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)
