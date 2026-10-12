"""Exercise every audited glove pair and actual validators under sanitizers."""
import subprocess,tempfile
from pathlib import Path
P=Path(__file__).resolve().parents[3]
text=(P/'inventory/offline_gloves.h').read_text()
body=text[text.index('namespace ICSMGloves {'):text.index('inline Selection Read(')]+'}\n'
fixture=r'''
#include <cassert>
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <limits>
#include "offline_glove_catalog.h"
constexpr int TEAM_CT=3,TEAM_TERRORIST=2,LOADOUT_POSITION_INVALID=-1;
template<class T>T clamp(T v,T a,T b){return std::clamp(v,a,b);}
bool IsFinite(float v){return std::isfinite(v);}
struct Base{virtual ~Base()=default;};
struct PaintData{int count=2;int Count()const{return count;}};
struct GameItemDefinition_t:Base{
 const char *GetBasePlayerDisplayModel()const{return "original.mdl";}
 int GetLoadoutSlot(int)const{return 41;}
 PaintData data;const PaintData *GetPaintData()const{return &data;}
};
struct CPaintKit{float flWearRemapMin=.06f,flWearRemapMax=.8f;};
struct Schema {
 GameItemDefinition_t item;CPaintKit paint;
 const Base *GetItemDefinition(int id){for(int d:ICSMGloves::Definitions)if(id==d)return &item;return nullptr;}
 const CPaintKit *GetPaintKitDefinition(int id){for(const auto &p:ICSMGloves::Catalog)if(id==p.paint)return &paint;return nullptr;}
} schema;
Schema *GetItemSchema(){return &schema;}
'''
cases=r'''
int main(){using namespace ICSMGloves;
 for(int team:{2,3}) {
  assert(Valid(team,Trial(team,Default(team),0)));
  assert(!Definition(team,team==3?5028:5029));
  for(const auto &entry:Catalog) {
   auto s=Trial(team,entry.definition,entry.paint);assert(Valid(team,s));assert(s.wear==.06f);
   s.seed=1;assert(!Valid(team,s));s.seed=0;
   s.wear=.801f;assert(!Valid(team,s));s.wear=.059f;assert(!Valid(team,s));
   s.wear=std::numeric_limits<float>::quiet_NaN();assert(!Valid(team,s));
   s.wear=std::numeric_limits<float>::infinity();assert(!Valid(team,s));
   for(int other:Definitions)if(other!=entry.definition)assert(!Valid(team,Trial(team,other,entry.paint)));
  }
  for(int definition:Definitions)if(Custom(definition)) {
   assert(Definition(team,definition));
   assert(DefaultPaint(definition)>0);assert(Valid(team,Trial(team,definition,DefaultPaint(definition))));
   assert(!Valid(team,Trial(team,definition,0)));
  }
  assert(!Valid(team,Trial(team,5030,711)));
  assert(!Valid(team,Trial(team,7,10018)));
  assert(!Valid(team,Trial(team,Default(team),10018)));
 }
 assert(!Definition(0,5030));assert(!Definition(1,5030));assert(!Definition(4,5030));
 assert(!Definition(3,5026));assert(!Definition(3,5036));
 assert(DefaultPaint(5030)==10018);assert(DefaultPaint(5028)==0);assert(DefaultPaint(5029)==0);
 assert(sizeof(Catalog)/sizeof(Catalog[0])==48);
 schema.item.data.count=1;assert(!Valid(3,Trial(3,5030,10018)));
 puts("PASS actual glove validators: all 48 recipes / 7 families, both teams, defaults, cross-family rejection, wear limits/NaN, seed, original two material slots, gun/unknown IDs rejected");}
'''
with tempfile.TemporaryDirectory() as tmp:
 source=Path(tmp)/'test.cpp';source.write_text(fixture+body+cases);binary=Path(tmp)/'test'
 subprocess.run(['xcrun','clang++','-std=c++17','-fsanitize=address,undefined','-I'+str(P/'inventory'),str(source),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)
