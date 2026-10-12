"""Exercise actual attribute reader, Valve ownership and weapon/cache lifecycle.
No game or device interaction. ASan/UBSan, pending composites and received data.
"""
import subprocess, tempfile
from pathlib import Path
P=Path(__file__).resolve().parents[3]
W=P.parent
cache=(P/'inventory/offline_skin_client.inc').read_text()
cache=cache[cache.index('namespace ICSMSkinRecipeCache {'):].replace('#endif','').replace('#if defined(SOURCE_IOS)','')
owner=(W/'CSGO_2019_Source/cstrike15_src-2019/public/materialsystem/custommaterialowner.h').read_text(errors='replace')
owner=owner[owner.index('class CCustomMaterialOwner\n{'):]
econ=(Path.home()/'Library/Caches/csgo-ipados-arm64/i5/source/game/shared/econ/econ_item_view.cpp').read_text(errors='replace')
iterate=econ[econ.index('void CEconItemView::IterateAttributes('):econ.index('//-----------------------------------------------------------------------------\n// Purpose:',econ.index('void CEconItemView::IterateAttributes('))]
fixture=r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#include <map>
#include <string>
#include <algorithm>
#include <cstdint>
using uint32=uint32_t;using uint64=uint64_t;using attrib_value_t=uint32_t;
#define V_memcpy memcpy
#define V_memset memset
#define V_memcmp memcmp
#define Msg(...) ((void)0)
#define Assert assert
#define MIN(a,b) std::min((a),(b))
#define FOR_EACH_VEC(v,i) for(int i=0;i<(v).Count();++i)
#define DefLessFunc(t) nullptr
#define CON_COMMAND(n,d) void n()
#define ECON_NETWORK_DYNAMIC_ATTRIBUTES_FOR_DEMOS 1
#define FCVAR_RELEASE 0
constexpr int MDLHANDLE_INVALID=-1,TEAM_CT=3,TEAM_TERRORIST=2;
constexpr int LOADOUT_POSITION_MELEE=0,LOADOUT_POSITION_SECONDARY0=2,LOADOUT_POSITION_SECONDARY1=3,
 LOADOUT_POSITION_RIFLE1=15,LOADOUT_POSITION_RIFLE4=18,LOADOUT_POSITION_RIFLE3=17;
struct CAutoGameSystem {
 CAutoGameSystem(const char*){} virtual ~CAutoGameSystem()=default;
 virtual void LevelInitPostEntity(){}virtual void LevelShutdownPreEntity(){}virtual void Shutdown(){}
};
struct ConVar {bool enabled=true;bool GetBool()const{return enabled;}} icsm_skin_recipe_cache;
bool ICSMFastAttributeLookup(){return true;}
double Plat_FloatTime(){return 0;}
template<class T>struct CUtlVector {
 std::vector<T> v;int Count()const{return int(v.size());}T &operator[](int i){return v.at(i);}const T &operator[](int i)const{return v.at(i);}
 void Purge(){v.clear();}void RemoveAll(){v.clear();}void Remove(int i){v.erase(v.begin()+i);}int AddToTail(T t){v.push_back(t);return Count()-1;}
};
template<class K,class V>struct CUtlMap {
 std::vector<std::pair<K,V>> v;CUtlMap(int,int,void*){}
 int Count()const{return int(v.size());}int InvalidIndex()const{return -1;}
 int Find(K k)const{for(int i=0;i<Count();++i)if(v[i].first==k)return i;return -1;}
 int Insert(K k,V x){int i=Find(k);if(i<0){v.push_back({k,x});i=Count()-1;}else v[i].second=x;return i;}
 V &operator[](int i){return v.at(i).second;}void RemoveAll(){v.clear();}
};
struct CAttribute_String{};struct Vector{};struct CEconItemAttributeDefinition;
struct IEconItemAttributeIterator {
 virtual ~IEconItemAttributeIterator()=default;
 virtual bool OnIterateAttributeValue(const CEconItemAttributeDefinition*,attrib_value_t)=0;
 virtual bool OnIterateAttributeValue(const CEconItemAttributeDefinition*,float)=0;
 virtual bool OnIterateAttributeValue(const CEconItemAttributeDefinition*,const CAttribute_String&)=0;
 virtual bool OnIterateAttributeValue(const CEconItemAttributeDefinition*,const Vector&)=0;
};
union attribute_data_union_t{float asFloat;uint32 asUInt32;};
struct AttributeType {
 bool BSupportsGameplayModificationAndNetworking()const{return true;}
 bool OnIterateAttributeValue(IEconItemAttributeIterator*,const CEconItemAttributeDefinition*,attribute_data_union_t)const;
};
using ISchemaAttributeType=AttributeType;
struct CEconItemAttributeDefinition {
 int index;bool floating=false;AttributeType type;
 int GetDefinitionIndex()const{return index;}const AttributeType *GetAttributeType()const{return &type;}
};
bool AttributeType::OnIterateAttributeValue(IEconItemAttributeIterator *it,const CEconItemAttributeDefinition *a,attribute_data_union_t v)const {
 return a->floating?it->OnIterateAttributeValue(a,v.asFloat):it->OnIterateAttributeValue(a,v.asUInt32);
}
std::map<int,CEconItemAttributeDefinition> defs;
std::map<std::string,int> names;
struct Schema {
 CEconItemAttributeDefinition *GetAttributeDefinitionByName(const char *name){auto i=names.find(name);return i==names.end()?nullptr:&defs.at(i->second);}
} schema;
Schema *GetItemSchema(){return &schema;}
enum EStickerAttributeType{k_EStickerAttribute_ID,k_EStickerAttribute_Wear,k_EStickerAttribute_Scale,k_EStickerAttribute_Rotation};
struct Handle {CEconItemAttributeDefinition *a;operator bool()const{return a;}const CEconItemAttributeDefinition *operator->()const{return a;}};
Handle GetStickerAttributeDefHandle(int slot,EStickerAttributeType type){return {&defs.at(100+slot*13+int(type)*2)};}
struct CEconItemAttribute {
 int key;float value;
 int GetAttribIndex()const{return key;}
 const CEconItemAttributeDefinition *GetStaticData()const{return &defs.at(key);}float GetValue()const{return value;}
};
struct List {
 std::vector<CEconItemAttribute> attrs;
 int GetNumAttributes()const{return int(attrs.size());}
 const CEconItemAttribute *GetAttribute(int i)const{return &attrs.at(i);}
 void Set(int key,float v){for(auto &a:attrs)if(a.key==key){a.value=v;return;}attrs.push_back({key,v});}
};
struct GameItemDefinition_t {
 int index=0;List attributes;
 int GetDefinitionIndex()const{return index;}
 void IterateAttributes(IEconItemAttributeIterator *it)const {
  for(auto &a:attributes.attrs){attribute_data_union_t v;v.asFloat=a.value;if(!a.GetStaticData()->type.OnIterateAttributeValue(it,a.GetStaticData(),v))break;}
 }
};
struct IMaterial{};
struct ICustomMaterial {
 static int alive;int refs=0;bool valid=false;ICustomMaterial(){++alive;}~ICustomMaterial(){--alive;}
 void AddRef(){++refs;}void Release(){assert(refs>0);if(!--refs)delete this;}bool IsValid()const{return valid;}
};int ICustomMaterial::alive=0;
'''
fixture2=r'''
struct CEconItemView:CCustomMaterialOwner {
 static int created,destroyed,generated,writes,decals,iterations;
 List local,m_NetworkedDynamicAttributesForDemos;GameItemDefinition_t def;bool initialized=true;
 CEconItemView(){++created;}~CEconItemView(){++destroyed;}
 void Init(int d,int,int,int){def.index=d;}
 void IterateAttributes(IEconItemAttributeIterator*)const;
 int GetNumAttributes()const{++iterations;return local.GetNumAttributes();}
 const CEconItemAttribute *GetAttribute(int i)const{return local.GetAttribute(i);}
 const CEconItemAttribute *GetAttributeByDefIndex(int k)const{for(auto &a:local.attrs)if(a.key==k)return &a;return nullptr;}
 uint64_t GetItemID()const{return 0;}const GameItemDefinition_t *GetSOCData()const{return nullptr;}
 const GameItemDefinition_t *GetStaticData()const{return &def;}
 const GameItemDefinition_t *GetItemDefinition()const{return &def;}bool IsValid()const{return initialized;}
 void SetOrAddAttributeValueByName(const char *n,float v){++writes;local.Set(names.at(n),v);}
 void UpdateGeneratedMaterial(bool,int,int){++generated;SetCustomMaterial(new ICustomMaterial,1);SetCustomMaterial(new ICustomMaterial,3);}
 void GenerateStickerMaterials(){++decals;}int GetNumSupportedStickerSlots()const{return 4;}bool ItemHasAnyStickersApplied()const{return true;}
};int CEconItemView::created=0,CEconItemView::destroyed=0,CEconItemView::generated=0,CEconItemView::writes=0,CEconItemView::decals=0,CEconItemView::iterations=0;
namespace ICSMSkinQuality {int signature=39;int Signature(){return signature;}int Texture(){return 3;}int DiffuseSize(){return 10;}int Shader(){return 1;}}
namespace ICSMSkin {
 struct Selection{int paint=0,seed=0;float wear=0;};
 int validations=0;Selection Read(int,int){return {44,0,.02f};}
 const void *FindTrial(int d){return d?&schema:nullptr;}
 bool ValidRecipe(int,const Selection &s){++validations;return s.paint>0 && s.paint<1000 && s.seed>=0 && s.seed<=1023 && s.wear>=0 && s.wear<=1;}
}
namespace ICSMStickers {
 struct Selection{int ids[5]={1,2,3,4,0};};Selection Read(int,int){return {};}
 void Apply(CEconItemView *item,const Selection &s){for(int slot=0;slot<4;++slot){float carrier;uint32 id=s.ids[slot];memcpy(&carrier,&id,4);item->local.Set(GetStickerAttributeDefHandle(slot,k_EStickerAttribute_ID)->GetDefinitionIndex(),carrier);}}
}
struct Inventory {
 CEconItemView *GetItemInLoadout(int team,int slot){static CEconItemView item;item.def.index=team*100+slot;return &item;}
 Inventory *GetLocalCSInventory(){return this;}
} inventory;
Inventory *CSInventoryManager(){return &inventory;}
struct CWeaponCSBase:CCustomMaterialOwner {
 CEconItemView *item;int m_nFallbackPaintKit=44,m_nFallbackSeed=0;float m_flFallbackWear=.02f;bool m_bVisualsDataSet=false;unsigned serial=1;
 struct Container {CEconItemView *item;CEconItemView *GetItem(){return item;}} container;
 explicit CWeaponCSBase(CEconItemView *i):item(i){}
 Container *GetAttributeContainer(){container.item=item;return &container;}
 int GetItemDefinitionIndex()const{return item->def.index;}
 struct Ref{unsigned serial;unsigned ToInt()const{return serial;}};
 Ref GetRefEHandle()const{return {serial};}int entindex()const{return serial;}
 const void *GetModelPtr()const{return this;}
 void UpdateCustomMaterial();void CheckCustomMaterial();
};
'''
cases=r'''
int main(){
 for(int slot=0;slot<5;++slot)for(int t=0;t<4;++t){int key=100+slot*13+t*2;defs.emplace(key,CEconItemAttributeDefinition{key,t!=0,{}});}
 const char *paintnames[]={"set item texture prefab","set item texture seed","set item texture wear"};
 for(int n=0;n<3;++n){int key=300+n;names[paintnames[n]]=key;defs.emplace(key,CEconItemAttributeDefinition{key,false,{}});}
 auto &cache=ICSMSkinRecipeCache::cache;
 cache.LevelInitPostEntity();assert(cache.entries.Count()==12 && CEconItemView::generated==12);
 CEconItemView prewarmed;assert(cache.Restore(&prewarmed,300,44,0,.02f,39)); // pending is shareable
 assert(!prewarmed.GetCustomMaterial(1)->IsValid());
 for(int i=0;i<12;++i)assert(cache.entries[i]->warmItem);
 {CEconItemView item;item.def.index=61;CWeaponCSBase weapon(&item);
  // Network-only stickers, a zero value and local override must retain original semantics.
  float carrier;uint32 id=4503;memcpy(&carrier,&id,4);item.m_NetworkedDynamicAttributesForDemos.Set(100,carrier);
  id=9;memcpy(&carrier,&id,4);item.local.Set(100,carrier);
  item.m_NetworkedDynamicAttributesForDemos.Set(102,.25f);
  auto snapshot=ICSMCosmeticAttributes::Read(&item);
  assert(snapshot.stickers[0]==9);float wear;memcpy(&wear,&snapshot.stickers[1],4);assert(wear==.25f);
  float scale;memcpy(&scale,&snapshot.stickers[2],4);assert(scale==1);
  weapon.UpdateCustomMaterial();
  assert(item.GetCustomMaterialCount()==4 && weapon.GetCustomMaterialCount()==4);
  int writes=CEconItemView::writes,generated=CEconItemView::generated,validations=ICSMSkin::validations,decals=CEconItemView::decals;
  for(int n=0;n<10000;++n)weapon.UpdateCustomMaterial();
  assert(writes==CEconItemView::writes && generated==CEconItemView::generated && validations==ICSMSkin::validations && decals==CEconItemView::decals);
  // Replace occupied sticker, including a previously network-only slot.
  id=17;memcpy(&carrier,&id,4);item.local.Set(100,carrier);weapon.UpdateCustomMaterial();assert(CEconItemView::decals==decals+1);
  assert(CEconItemView::generated==generated);
  item.m_NetworkedDynamicAttributesForDemos.Set(102,.5f);weapon.UpdateCustomMaterial();assert(CEconItemView::decals==decals+2);
  // Changing wear/seed and quality invalidates once; zero remains valid.
  weapon.m_flFallbackWear=0;weapon.m_nFallbackSeed=7;weapon.UpdateCustomMaterial();assert(CEconItemView::generated==generated+1);
  int g=CEconItemView::generated;++ICSMSkinQuality::signature;weapon.UpdateCustomMaterial();assert(CEconItemView::generated==g+1);
  weapon.UpdateCustomMaterial();assert(CEconItemView::generated==g+1);
  weapon.m_nFallbackPaintKit=0;weapon.UpdateCustomMaterial();assert(!weapon.GetCustomMaterialCount() && !item.GetCustomMaterialCount());
  writes=CEconItemView::writes;for(int n=0;n<1000;++n)weapon.UpdateCustomMaterial();assert(CEconItemView::writes==writes);
  weapon.m_nFallbackPaintKit=10000;weapon.UpdateCustomMaterial();assert(!weapon.GetCustomMaterialCount());
  weapon.m_nFallbackPaintKit=44;weapon.UpdateCustomMaterial();assert(weapon.GetCustomMaterialCount()==4);
  // Cleared entity materials are rebound without new compositing.
  g=CEconItemView::generated;weapon.ClearCustomMaterials();weapon.m_bVisualsDataSet=false;weapon.UpdateCustomMaterial();assert(CEconItemView::generated==g);
  // Persistent pointer bindings: no repeated refcount churn; sparse indices and clear are preserved.
  CCustomMaterialOwner view;assert(ICSMBindCustomMaterials(&weapon,&view));
  for(int n=0;n<10000;++n)assert(!ICSMBindCustomMaterials(&weapon,&view));
  assert(view.GetCustomMaterial(1)==weapon.GetCustomMaterial(1) && !view.GetCustomMaterial(2));
  weapon.ClearCustomMaterials();assert(ICSMBindCustomMaterials(&weapon,&view) && view.GetCustomMaterialCount()==0);
 }
 cache.LevelShutdownPreEntity();ICSMStickerState::cache.LevelShutdownPreEntity();prewarmed.ClearCustomMaterials();
 assert(ICustomMaterial::alive==0);
 // LRU overflow, exact quality/seed/wear matching and owned warm-item destruction.
 int items=CEconItemView::created-CEconItemView::destroyed;
 cache.LevelInitPostEntity();assert(cache.entries.Count()==12);
 for(int recipe=0;recipe<100;++recipe){CEconItemView item;item.UpdateGeneratedMaterial(true,-1,10);cache.Remember(&item,61,44,recipe,.02f,40);assert(cache.entries.Count()==12);}
 CEconItemView target;assert(!cache.Restore(&target,61,44,99,.02f,39));assert(!cache.Restore(&target,61,44,99,.03f,40));
 assert(cache.Restore(&target,61,44,99,.02f,40));target.ClearCustomMaterials();
 cache.Shutdown();assert(ICustomMaterial::alive==0);
 icsm_skin_recipe_cache.enabled=false;cache.LevelInitPostEntity();assert(cache.entries.Count()==0);
 printf("PASS: actual Valve attribute iteration/local-network precedence, raw sticker transforms, 10000 stable updates without setters/validation/compositing, pending sharing, quality/recipe invalidation, sparse bindings, exact LRU cap=12 and cleanup\n");
}
'''
with tempfile.TemporaryDirectory() as tmp:
 source=Path(tmp)/'test.cpp';source.write_text(fixture+owner+fixture2+iterate+'\n#include "offline_cosmetic_attributes.h"\n#include "offline_material_binding.h"\n'+cache+cases)
 binary=Path(tmp)/'test'
 subprocess.run(['xcrun','clang++','-std=c++17','-fsanitize=address,undefined','-I'+str(P/'inventory'),str(source),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)
