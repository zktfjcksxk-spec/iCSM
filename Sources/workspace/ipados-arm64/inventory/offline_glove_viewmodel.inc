#include "offline_gloves.h"
#include "offline_skin_quality.h"
#include "igamesystem.h"
namespace ICSMGlovesView {
class Cache : public CAutoGameSystemPerFrame {
public:
    bool dirty=true;
    bool levelActive=false;
    ICSMGloves::Selection choices[2];
    CEconItemView *items[2]={NULL,NULL};
    bool eligible[2]={false,false};
    int quality=-1;
    Cache():CAutoGameSystemPerFrame("iCSM offline gloves"){}
    void Clear() {for(auto &item:items){delete item;item=NULL;}dirty=true;quality=-1;}
    void Warm() {Get(TEAM_CT);Get(TEAM_TERRORIST);}
    // CL_FullyConnected invokes this after the model table is precached,
    // before the player chooses a team or receives a first-person weapon.
    void LevelInitPostEntity() override {levelActive=true;Warm();Msg("ICSM_GLOVE_WARMUP stage=level_init\n");}
    void PreRender() override {
        if(levelActive && (dirty || quality!=ICSMSkinQuality::Signature()))Warm();
    }
    void LevelShutdownPreEntity() override {levelActive=false;Clear();}
    void Shutdown() override {levelActive=false;Clear();}
    CEconItemView *Get(int team) {
        if(team!=TEAM_CT && team!=TEAM_TERRORIST)return NULL;
        const int signature=ICSMSkinQuality::Signature();
        if(dirty || quality!=signature) {
            Clear();dirty=false;quality=signature;
            choices[0]=ICSMGloves::Read(TEAM_CT);choices[1]=ICSMGloves::Read(TEAM_TERRORIST);
            for(int side=0;side<2;++side)eligible[side]=ICSMGloves::Custom(choices[side].definition) &&
                ICSMGloves::Valid(side==0?TEAM_CT:TEAM_TERRORIST,choices[side]);
        }
        int side=team==TEAM_CT?0:1;const auto &s=choices[side];
        // The choice and quality are immutable until invalidation. Validation,
        // schema lookup and model-table lookup belong to creation, not animation.
        if(items[side])return items[side];
        if(!eligible[side])return NULL;
        const auto *definition=ICSMGloves::Definition(team,s.definition);
        if(!definition || modelinfo->GetModelIndex(definition->GetBasePlayerDisplayModel())==-1)return NULL;
        if(!items[side]) {
            auto item=new CEconItemView;item->Init(s.definition,0,1,0);
            item->SetOrAddAttributeValueByName("set item texture prefab",float(s.paint));
            item->SetOrAddAttributeValueByName("set item texture seed",float(s.seed));
            item->SetOrAddAttributeValueByName("set item texture wear",s.wear);
            // Valve's retained clothing pipeline composites diffuse, normal
            // and masks independently for the original left/right UVs.
            item->UpdateGeneratedMaterial(ICSMSkinQuality::Texture()!=0,MDLHANDLE_INVALID,ICSMSkinQuality::DiffuseSize());
            items[side]=item;
            Msg("ICSM_GLOVE_MATERIALS team=%d definition=%d paint=%d count=%d\n",team,s.definition,s.paint,item->GetCustomMaterialCount());
        }
        return items[side];
    }
};
static Cache cache;
inline bool Ready(CEconItemView *item) {
    if(!item || !item->GetCustomMaterialCount())return false;
    bool any=false;
    for(int i=0;i<item->GetCustomMaterialCount();++i) {
        ICustomMaterial *material=item->GetCustomMaterial(i);
        if(!material)continue;
        if(!material->IsValid())return false;
        any=true;
    }
    return any;
}
inline bool SameModel(C_ViewmodelAttachmentModel *entity,const char *path) {
    if(!entity || !entity->GetModel() || !path || !path[0])return false;
    // studiohdr_t::name is an internal, 64-byte name, often without "models/"
    // (and can be truncated). Compare the engine's precached model identity.
    const int expected=modelinfo->GetModelIndex(path);
    return expected!=-1 && entity->GetModelIndex()==expected;
}
inline bool CustomModel(C_ViewmodelAttachmentModel *entity) {
    for(int index:ICSMGloves::Definitions) {
        if(!ICSMGloves::Custom(index))continue;
        const auto *definition=GetItemSchema()->GetItemDefinition(index);
        if(definition && SameModel(entity,definition->GetBasePlayerDisplayModel()))return true;
    }
    return false;
}
inline void Apply(CEconItemView *item,C_ViewmodelAttachmentModel *entity) {
    if(!entity || !Ready(item))return;
    bool changed=item->GetCustomMaterialCount()!=entity->GetCustomMaterialCount();
    for(int i=0;i<item->GetCustomMaterialCount();++i) {
        ICustomMaterial *material=item->GetCustomMaterial(i);
        if(material!=entity->GetCustomMaterial(i))changed=true;
    }
    if(changed) {item->DuplicateCustomMaterialsToOther(entity);Msg("ICSM_GLOVE_ATTACHED definition=%d paint=%d\n",item->GetItemDefinition()->GetDefinitionIndex(),item->GetCustomPaintKitIndex());}
}
}
namespace ICSMGloves {void Changed() {ICSMGlovesView::cache.dirty=true;}}
