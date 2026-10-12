"""Compare actual indexed lookup against its original branch, including invalid IDs."""
from pathlib import Path
import subprocess,tempfile
C=Path.home()/'Library/Caches/csgo-ipados-arm64/i5'
s=(C/'source/game/shared/econ/econ_item_view.cpp').read_text()
a=s.index('CEconItemAttribute *CAttributeList::GetAttributeByDefIndex( uint16')
b=s.index('//-----------------------------------------------------------------------------',s.index('const CEconItemAttribute *CAttributeList::GetAttributeByDefIndex(',a))
functions=s[a:b]
fixture=r'''
#include <vector>
#include <cassert>
#include <cstdint>
#include <cstdio>
using uint16=uint16_t;
#define Assert(x) ((void)0)
bool fast=true;bool ICSMFastAttributeLookup(){return fast;}
struct Def{int id;int GetDefinitionIndex()const{return id;}};
unsigned resolutions=0;
struct CEconItemAttribute {
 uint16 id;Def *definition;
 uint16 GetAttribIndex()const{return id;}
 const Def *GetStaticData()const{++resolutions;return definition;}
};
struct CAttributeList {
 std::vector<CEconItemAttribute> list;
 int GetNumAttributes()const{return list.size();}
 CEconItemAttribute *GetAttribute(int n){return &list[n];}
 const CEconItemAttribute *GetAttribute(int n)const{return &list[n];}
 CEconItemAttribute *GetAttributeByDefIndex(uint16);
 const CEconItemAttribute *GetAttributeByDefIndex(uint16)const;
};
'''
cases=r'''
int main(){
 Def defs[500];for(int i=0;i<500;++i)defs[i].id=i;
 CAttributeList l;
 // Invalid definition, duplicates, zero ID, high bits and noncontiguous list.
 l.list={{1,nullptr},{1,&defs[1]},{129,&defs[129]},{257,&defs[257]},{0,&defs[0]},{65535,nullptr}};
 const auto &cl=l;
 for(unsigned id=0;id<65536;++id){
  fast=false;auto old=l.GetAttributeByDefIndex(id);auto cold=cl.GetAttributeByDefIndex(id);
  fast=true;assert(l.GetAttributeByDefIndex(id)==old && cl.GetAttributeByDefIndex(id)==cold);
 }
 l.list.clear();for(int n=0;n<40;++n)l.list.push_back({uint16(n*3),&defs[n*3]});
 fast=false;resolutions=0;for(int n=0;n<10000;++n)assert(!cl.GetAttributeByDefIndex(499));auto original=resolutions;
 fast=true;resolutions=0;for(int n=0;n<10000;++n)assert(!cl.GetAttributeByDefIndex(499));
 assert(resolutions==0 && original>0);
 // Live replacement/removal must be visible immediately, with no invalidation API.
 l.list[5]={499,&defs[499]};assert(cl.GetAttributeByDefIndex(499)==&l.list[5]);
 l.list.erase(l.list.begin()+5);assert(!cl.GetAttributeByDefIndex(499));
 printf("PASS: 65536 IDs in mutable/const actual lookup paths; invalid/duplicate IDs, live mutations, misses avoid %u schema resolutions\n",original);
}
'''
with tempfile.TemporaryDirectory() as t:
 p=Path(t);(p/'test.cpp').write_text(fixture+functions+cases)
 subprocess.run(['xcrun','clang++','-std=c++17','-O1','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
