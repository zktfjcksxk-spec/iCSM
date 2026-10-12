// Attach the original composite material to the native weapon. The viewmodel
// and CBaseWeaponWorldModel already copy overrides by material basename.
#if defined(SOURCE_IOS) && defined(CLIENT_DLL)
#include "igamesystem.h"
#include "offline_stickers.h"
#include "offline_cosmetic_attributes.h"
static ConVar icsm_skin_recipe_cache("icsm_skin_recipe_cache","1",FCVAR_RELEASE,
    "Reuse exact offline skin material recipes within a map; diagnostic A/B");
namespace ICSMSkinRecipeCache {
struct Entry {
    int definition,paint,seed,signature;
    float wear;
    CCustomMaterialOwner materials;
    CEconItemView *warmItem=NULL;
    ~Entry(){delete warmItem;}
};
class Cache : public CAutoGameSystem {
public:
    CUtlVector<Entry*> entries;
    unsigned hits=0,misses=0;
    void LevelInitPostEntity() override;
    Cache():CAutoGameSystem("iCSM bounded skin recipe cache"){}
    void Clear() {FOR_EACH_VEC(entries,i)delete entries[i];entries.Purge();}
    void LevelShutdownPreEntity() override {
        int pending=0;
        FOR_EACH_VEC(entries,i)for(int m=0;m<entries[i]->materials.GetCustomMaterialCount();++m) {
            auto *material=entries[i]->materials.GetCustomMaterial(m);
            if(material && !material->IsValid())++pending;
        }
        Msg("ICSM_COSMETIC_CACHE recipes=%d hits=%u misses=%u pending_materials=%d limit=12\n",entries.Count(),hits,misses,pending);
        Clear();
    }
    void Shutdown() override {Clear();}
    bool Restore(CEconItemView *item,int definition,int paint,int seed,float wear,int signature) {
        if(!icsm_skin_recipe_cache.GetBool()){Clear();return false;}
        FOR_EACH_VEC(entries,i) {
            Entry *e=entries[i];
            if(e->definition!=definition || e->paint!=paint || e->seed!=seed || e->wear!=wear || e->signature!=signature)continue;
            // Pending composites are also reusable: the original material
            // manager owns completion and the renderer checks IsValid().
            if(!e->materials.GetCustomMaterialCount())continue;
            e->materials.DuplicateCustomMaterialsToOther(item);
            // The owner API preserves original model material indices and
            // AddRef/Release. No private texture/processor memory is copied.
            entries.Remove(i);entries.AddToTail(e);++hits;return true;
        }
        ++misses;return false;
    }
    Entry *Remember(CEconItemView *item,int definition,int paint,int seed,float wear,int signature) {
        if(!icsm_skin_recipe_cache.GetBool())return NULL;
        FOR_EACH_VEC(entries,i) {
            Entry *e=entries[i];
            if(e->definition==definition && e->paint==paint && e->seed==seed && e->wear==wear && e->signature==signature)return e;
        }
        bool any=false;
        for(int i=0;i<item->GetCustomMaterialCount();++i) {
            auto m=item->GetCustomMaterial(i);if(!m)continue;
            any=true;
        }
        if(!any)return NULL;
        // High skin quality creates large diffuse/exponent textures. Keep at
        // most 12 recipes, and clear them before the map/material shutdown.
        while(entries.Count()>=12){delete entries[0];entries.Remove(0);}
        auto e=new Entry;e->definition=definition;e->paint=paint;e->seed=seed;e->wear=wear;e->signature=signature;
        item->DuplicateCustomMaterialsToOther(&e->materials);entries.AddToTail(e);return e;
    }
};
static Cache cache;
}
// All prewarmed owners share the same 12-entry LRU with weapons. No second
// unbounded cache, no catalog-wide generation, no GPU completion wait.
void ICSMSkinRecipeCache::Cache::LevelInitPostEntity() {
    Clear();hits=misses=0;
    if(!icsm_skin_recipe_cache.GetBool())return;
    auto *inventory=CSInventoryManager()->GetLocalCSInventory();if(!inventory)return;
    const double begin=Plat_FloatTime();
    int warmed=0,decals=0;const int signature=ICSMSkinQuality::Signature();
    const int priority[]={LOADOUT_POSITION_MELEE,LOADOUT_POSITION_SECONDARY0,
        LOADOUT_POSITION_RIFLE1,LOADOUT_POSITION_RIFLE4,LOADOUT_POSITION_RIFLE3,LOADOUT_POSITION_SECONDARY1};
    // Valve processes pending composites from the tail: enqueue the starting
    // pistol/knife last so they finalize before secondary buy choices.
    for(int position=int(sizeof(priority)/sizeof(priority[0]))-1;position>=0;--position)for(int team=TEAM_CT;team>=TEAM_TERRORIST;--team) {
        int slot=priority[position];
        if(entries.Count()>=12)break;
        const auto *equipped=inventory->GetItemInLoadout(team,slot);
        if(!equipped || !equipped->IsValid())continue;
        int definition=equipped->GetItemDefinition()->GetDefinitionIndex();
        if(!ICSMSkin::FindTrial(definition))continue;
        const auto skin=ICSMSkin::Read(team,definition);
        const auto stickers=ICSMStickers::Read(definition,skin.paint);
        bool hasStickers=false;for(int id:stickers.ids)hasStickers|=id!=0;
        if(!skin.paint && !hasStickers)continue;
        bool found=false;FOR_EACH_VEC(entries,i) {
            const Entry *e=entries[i];
            if(e->definition==definition && e->paint==skin.paint && e->seed==skin.seed && e->wear==skin.wear && e->signature==signature){found=true;break;}
        }
        if(found)continue;
        auto *item=new CEconItemView;item->Init(definition,0,1,0);
        item->SetOrAddAttributeValueByName("set item texture prefab",float(skin.paint));
        item->SetOrAddAttributeValueByName("set item texture seed",float(skin.seed));
        item->SetOrAddAttributeValueByName("set item texture wear",skin.wear);
        ICSMStickers::Apply(item,stickers);item->GenerateStickerMaterials();
        if(skin.paint)item->UpdateGeneratedMaterial(ICSMSkinQuality::Texture()!=0,MDLHANDLE_INVALID,ICSMSkinQuality::DiffuseSize());
        Entry *entry=Remember(item,definition,skin.paint,skin.seed,skin.wear,signature);
        if(!entry && hasStickers) {
            entry=new Entry;entry->definition=definition;entry->paint=skin.paint;entry->seed=skin.seed;entry->wear=skin.wear;entry->signature=signature;
            entries.AddToTail(entry);
        }
        if(entry) {entry->warmItem=item;++warmed;decals+=hasStickers;}else delete item;
    }
    Msg("ICSM_COSMETIC_WARMUP recipes=%d sticker_recipes=%d limit=12 cpu_ms=%.3f async=1\n",warmed,decals,(Plat_FloatTime()-begin)*1000);
}
namespace ICSMStickerState {
struct State {
    ICSMCosmeticAttributes::Snapshot attributes;
    const CEconItemView *item=NULL;
    int definition=0,signature=-1,paint=-1,seed=-1;
    float wear=-1;
    bool validRecipe=false;
};
class Cache : public CAutoGameSystem {
public:
    CUtlMap<unsigned,State> entries;
    Cache():CAutoGameSystem("iCSM cosmetic state"),entries(0,0,DefLessFunc(unsigned)){}
    void LevelShutdownPreEntity() override {entries.RemoveAll();}
    void Shutdown() override {entries.RemoveAll();}
};
static Cache cache;
}
CON_COMMAND(icsm_skin_recipe_stats,"Print bounded skin recipe reuse") {
    Msg("ICSM_SKIN_CACHE recipes=%d hits=%u misses=%u\n",ICSMSkinRecipeCache::cache.entries.Count(),ICSMSkinRecipeCache::cache.hits,ICSMSkinRecipeCache::cache.misses);
}
#endif
void CWeaponCSBase::UpdateCustomMaterial(void)
{
#if defined(SOURCE_IOS)
    CEconItemView *item = GetAttributeContainer()->GetItem();
    if (!item || !item->IsValid() || item->GetItemID() != 0 ||
        !ICSMSkin::FindTrial(GetItemDefinitionIndex())) return;
    // One iteration replaces twenty independent sticker searches and three
    // paint searches. Received attributes and local overrides remain authoritative.
    const auto attributes=ICSMCosmeticAttributes::Read(item);
    auto &states=ICSMStickerState::cache.entries;
    if(states.Count()>512)states.RemoveAll();
    const unsigned handle=GetRefEHandle().ToInt();int index=states.Find(handle);
    if(index==states.InvalidIndex())index=states.Insert(handle,ICSMStickerState::State());
    auto &state=states[index];
    const int definition=GetItemDefinitionIndex(),signature=ICSMSkinQuality::Signature();
    const int paint=m_nFallbackPaintKit,seed=m_nFallbackSeed;const float wear=m_flFallbackWear;
    const bool initial=state.item!=item || state.definition!=definition;
    if(initial || !state.attributes.SameStickers(attributes)) {
        item->GenerateStickerMaterials();
        Msg("ICSM_STICKER_MATERIALS entity=%d definition=%d slots=%d applied=%d\n",entindex(),definition,item->GetNumSupportedStickerSlots(),item->ItemHasAnyStickersApplied());
    }
    const bool changed=initial || state.signature!=signature || state.paint!=paint || state.seed!=seed || state.wear!=wear;
    const bool attributeChanged=attributes.paint!=float(paint) || attributes.seed!=float(seed) || attributes.wear!=wear;
    if(changed || attributeChanged) {
        ClearCustomMaterials();item->ClearCustomMaterials();m_bVisualsDataSet=false;
        // Identical setters invalidate descriptions. Only write actual changes;
        // zero wear/seed remain valid and remote attribute lists stay untouched.
        if(attributes.paint!=float(paint))item->SetOrAddAttributeValueByName("set item texture prefab",float(paint));
        if(attributes.seed!=float(seed))item->SetOrAddAttributeValueByName("set item texture seed",float(seed));
        if(attributes.wear!=wear)item->SetOrAddAttributeValueByName("set item texture wear",wear);
        ICSMSkin::Selection recipe;recipe.paint=paint;recipe.seed=seed;recipe.wear=wear;
        state.validRecipe=paint && ICSMSkin::ValidRecipe(definition,recipe);
    }
    state.item=item;state.definition=definition;state.signature=signature;
    state.paint=paint;state.seed=seed;state.wear=wear;state.attributes=attributes;
    if(!state.validRecipe)return;
    if (!item->GetCustomMaterialCount())
    {
        const bool reused=ICSMSkinRecipeCache::cache.Restore(item,GetItemDefinitionIndex(),paint,seed,wear,signature);
        // Generate once per recipe. Existing material manager finalizes GPU
        // composites asynchronously; the original renderer waits for IsValid.
        if(!reused)item->UpdateGeneratedMaterial(ICSMSkinQuality::Texture() != 0,
            MDLHANDLE_INVALID, ICSMSkinQuality::DiffuseSize());
        Msg("ICSM_SKIN_QUALITY entity=%d shader=%d texture=%d size=%d signature=%d\n",
            entindex(), ICSMSkinQuality::Shader(), ICSMSkinQuality::Texture(),
            1 << ICSMSkinQuality::DiffuseSize(), signature);
        Msg("%s entity=%d definition=%d paint=%d seed=%d wear=%.6f materials=%d\n",reused?"ICSM_SKIN_REUSE":"ICSM_SKIN_GENERATE",
            entindex(), GetItemDefinitionIndex(), paint, seed, wear, item->GetCustomMaterialCount());
        ICSMSkinRecipeCache::cache.Remember(item,GetItemDefinitionIndex(),paint,seed,wear,signature);
    }
    if (!m_bVisualsDataSet && GetModelPtr() && item->GetCustomMaterialCount())
    {
        ClearCustomMaterials();
        for (int i = 0; i < item->GetCustomMaterialCount(); ++i)
            if (item->GetCustomMaterial(i)) SetCustomMaterial(item->GetCustomMaterial(i), i);
        m_bVisualsDataSet = GetCustomMaterialCount() > 0;
        Msg("ICSM_SKIN_ATTACH entity=%d materials=%d\n", entindex(), GetCustomMaterialCount());
    }
#endif
}

void CWeaponCSBase::CheckCustomMaterial(void)
{
    UpdateCustomMaterial();
}
