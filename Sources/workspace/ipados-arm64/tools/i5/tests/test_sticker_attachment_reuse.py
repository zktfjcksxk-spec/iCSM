"""Exercise the actual sticker attachment implementation without a device."""
from pathlib import Path
import tempfile,subprocess
P=Path(__file__).resolve().parents[3]
code=(P/'inventory/offline_sticker_viewmodel.inc').read_text()
fixture=r'''
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#include <cstring>
#include <algorithm>
#define V_strcmp strcmp
#define MIN(a,b) std::min((a),(b))
#define Msg(...) ((void)0)
constexpr int EF_NODRAW=1;constexpr int k_EStickerAttribute_ID=0;
constexpr int vec3_origin=0,vec3_angle=0;
using CSWeaponID=int;
struct RenderableInstance_t{};
struct IMaterial {bool error=false;bool IsErrorMaterial()const{return error;}};
struct CMaterialReference {
 IMaterial *m=nullptr;void Init(IMaterial *x){m=x;}bool IsValid()const{return m;}
 operator IMaterial*()const{return m;}IMaterial *operator->()const{return m;}
};
struct CUtlString {std::string s;void operator=(const char *v){s=v;}const char *String()const{return s.c_str();}};
struct ModelRenderer {void ForcedMaterialOverride(IMaterial*){}} renderer;
ModelRenderer *modelrender=&renderer;
struct C_BaseViewModel;
struct C_ViewmodelAttachmentModel {
 static int created,removed,swaps,reparents,draws;C_BaseViewModel *parent=nullptr;std::string path;int modelIndex=10;
 virtual ~C_ViewmodelAttachmentModel()=default;
 bool InitializeAsClientEntity(const char *p,bool){if(!strcmp(p,"missing"))return false;path=p;++created;return true;}
 void Release(){delete this;}void Remove(){++removed;delete this;}
 virtual int InternalDrawModel(int,const RenderableInstance_t&){++draws;return 1;}
 const void *GetModel()const{return this;}int GetModelIndex()const{return modelIndex;}
 void SetParent(C_BaseViewModel *p){parent=p;++reparents;}C_BaseViewModel *GetMoveParent()const{return parent;}
 void SetLocalOrigin(int){}void SetLocalAngles(int){}void UpdatePartitionListEntry(){}
 struct Collision{void MarkPartitionHandleDirty(){}} collision;Collision *CollisionProp(){return &collision;}
 void SetViewmodel(C_BaseViewModel*){}void SetUseParentLightingOrigin(bool){}void UpdateVisibility(){}
};int C_ViewmodelAttachmentModel::created=0,C_ViewmodelAttachmentModel::removed=0,C_ViewmodelAttachmentModel::swaps=0,C_ViewmodelAttachmentModel::reparents=0,C_ViewmodelAttachmentModel::draws=0;
struct Handle {C_ViewmodelAttachmentModel *p;C_ViewmodelAttachmentModel *Get()const{return p;}};
struct Handles {
 std::vector<Handle> data;int Count()const{return int(data.size());}Handle &operator[](int i){return data.at(i);}
 void InsertBefore(int i,C_ViewmodelAttachmentModel *p){data.insert(data.begin()+i,Handle{p});}
 void Remove(int i){data.erase(data.begin()+i);}
};
struct ItemDefinition {int GetDefinitionIndex()const{return 61;}};
struct CEconItemView {
 IMaterial *materials[5]={};std::string paths[5];ItemDefinition def;
 bool IsValid()const{return true;}int GetNumSupportedStickerSlots()const{return 4;}
 IMaterial *GetStickerIMaterialBySlotIndex(int i)const{return materials[i];}
 const char *GetStickerSlotModelBySlotIndex(int i)const{return paths[i].c_str();}
 const ItemDefinition *GetItemDefinition()const{return &def;}unsigned GetStickerAttributeBySlotIndexInt(int,int,int)const{return 1;}
};
struct C_BaseViewModel {
 Handles m_hStickerModelAddons;
 void AddViewmodelStickers(CEconItemView*,CSWeaponID);
 void RemoveViewmodelStickers(){while(m_hStickerModelAddons.Count()){m_hStickerModelAddons[0].Get()->Remove();m_hStickerModelAddons.Remove(0);}}
 void RemoveEffects(int){}
};
'''
cases=r'''
int main(){
 C_BaseViewModel view;CEconItemView item;IMaterial first,replacement,other;
 item.materials[0]=&first;item.paths[0]="pist_223_decal_a";
 item.materials[3]=&other;item.paths[3]="pist_223_decal_d";
 view.AddViewmodelStickers(&item,61);
 assert(C_ViewmodelAttachmentModel::created==2);
 auto *a=dynamic_cast<CICSMStickerAttachment*>(view.m_hStickerModelAddons[0].Get());
 auto *d=view.m_hStickerModelAddons[1].Get();assert(a && d);
 for(int frame=0;frame<10000;++frame)view.AddViewmodelStickers(&item,61);
 assert(C_ViewmodelAttachmentModel::created==2 && C_ViewmodelAttachmentModel::removed==0);
 item.materials[0]=&replacement;view.AddViewmodelStickers(&item,61);
 assert(view.m_hStickerModelAddons[0].Get()==a && a->GetStickerMaterial()==&replacement);
 a->parent=nullptr;view.AddViewmodelStickers(&item,61);assert(a->parent==&view && C_ViewmodelAttachmentModel::created==2);
 RenderableInstance_t instance;assert(a->InternalDrawModel(0,instance)==1);
 replacement.error=true;assert(a->InternalDrawModel(0,instance)==0);replacement.error=false;
 item.materials[3]=nullptr;view.AddViewmodelStickers(&item,61);
 assert(view.m_hStickerModelAddons.Count()==1 && view.m_hStickerModelAddons[0].Get()==a && C_ViewmodelAttachmentModel::removed==1);
 // One model change replaces one mesh, preserving other valid attachments.
 item.materials[3]=&other;view.AddViewmodelStickers(&item,61);d=view.m_hStickerModelAddons[1].Get();
 item.paths[0]="pist_glock_decal_a";view.AddViewmodelStickers(&item,4);
 assert(view.m_hStickerModelAddons.Count()==2 && view.m_hStickerModelAddons[1].Get()==d);
 int creations=C_ViewmodelAttachmentModel::created;item.paths[0]="missing";view.AddViewmodelStickers(&item,61);
 assert(C_ViewmodelAttachmentModel::created==creations && view.m_hStickerModelAddons.Count()==1);
 view.AddViewmodelStickers(nullptr,61);assert(view.m_hStickerModelAddons.Count()==0);
 assert(C_ViewmodelAttachmentModel::created==C_ViewmodelAttachmentModel::removed);
 puts("PASS: actual attachment code, 10000 stable animation updates with zero reallocations, material replacement/reparent reuse, sparse slots, one-mesh replacement, missing model cleanup and draw override");
}
'''
with tempfile.TemporaryDirectory() as tmp:
 source=Path(tmp)/'test.cpp';source.write_text(fixture+code+cases);binary=Path(tmp)/'test'
 subprocess.run(['xcrun','clang++','-std=c++17','-fsanitize=address,undefined',str(source),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)
