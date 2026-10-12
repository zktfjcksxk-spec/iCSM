#include "offline_loadout.h"
#include "offline_skin.h"
#include "offline_stickers.h"
#include "offline_gloves.h"
#include "c_cs_player.h"
#include "weapon_csbase.h"
#include "offline_skin_quality.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/itexture.h"
#include "offline_sticker_preview.inc"
#include "network_inventory_client.inc"

static ConVar icsm_skin_shader_detail("icsm_skin_shader_detail", "1", FCVAR_ARCHIVE | FCVAR_RELEASE,
    "Local finish shading: 0 follows global quality, 1 preserves original Phong/normal detail", true, 0, true, 1);
static ConVar icsm_skin_texture_detail("icsm_skin_texture_detail", "3", FCVAR_ARCHIVE | FCVAR_RELEASE,
    "Local finish textures: 0 follows global quality, 1=256, 2=512, 3=1024", true, 0, true, 3);

// Original Panorama helper updates the live, restart and options-UI values.
// These video ConVars are development-only in this depot; console assignments
// are rejected by CCvarUtilities::IsCommand even though the video UI can set them.
extern void SetConvar(const char *,int,int,bool);
CON_COMMAND_F(icsm_initial_video, "Apply the first-install medium video preset through original Panorama video settings", FCVAR_RELEASE)
{
    KeyValues::AutoDelete preset(new KeyValues("VideoPreset"));
    if(!preset->LoadFromFile(filesystem,"cfg/i5_initial_video.kv","MOD")) {
        Warning("ICSM_INITIAL_VIDEO missing preset\n");return;
    }
    FOR_EACH_SUBKEY(preset, entry) {
        const char *name=entry->GetName();
        ConVarRef variable(name);
        if(!variable.IsValid()){Warning("ICSM_INITIAL_VIDEO missing convar %s\n",name);continue;}
        int value=entry->GetInt("value");
        SetConvar(name,value,value,entry->GetBool("auto"));
        Msg("ICSM_INITIAL_VIDEO %s=%s\n",name,variable.GetString());
    }
    engine->ClientCmd_Unrestricted("mat_savechanges\n");
}

CON_COMMAND_F(icsm_resolution_auto, "Fit the Metal backbuffer to the current UIKit screen aspect ratio", FCVAR_RELEASE)
{
    // The UIKit host consumes this request and executes the original Source
    // video commands on its normal command queue. No client-side display API.
    FileHandle_t file=filesystem->Open("cfg/icsm-auto-resolution.request", "wb", "MOD");
    if(file==FILESYSTEM_INVALID_HANDLE){Warning("ICSM_SCREEN_AUTO request write failed\n");return;}
    filesystem->Write("auto",4,file);filesystem->Close(file);
}

class CNativeOfflineInventory
{
    static void AddPreview(v8::Local<v8::Object> row, int definition, int paint)
    {
        // Original 2019 CDN icons are SHA-1 checked against items_game_cdn.txt
        // and bundled locally. Browsing never creates persistent reference
        // items or asynchronous 3D composite jobs for hundreds of recipes.
        CUtlString path=ICSMStickerPreview::cache.Image(definition,paint);
        CreateJSOEntry_String(row, "image", path.Get());
    }
public:
    void JSRegisterFunc();
    int GetPreviewRevision() {return 0;}
    const char *GetPreview(int definition,int paint) {
        static CUtlString image;image=ICSMStickerPreview::cache.Image(definition,paint);return image.Get();
    }
    int GetSkin(int team, int definition)
    {
        if(ICSMGloves::Definition(team,definition))return ICSMGloves::Read(team,definition).paint;
        return ICSMSkin::Read(team, definition).paint;
    }
    bool SaveSkin(int team, int definition, int paint)
    {
        if(ICSMGloves::Definition(team,definition)) {
            if(!ICSMGloves::Save(team,ICSMGloves::Trial(team,definition,paint),false))return false;
            ICSMGloves::Changed();return true;
        }
        const ICSMSkin::Selection skin = ICSMSkin::Trial(definition, paint);
        if (!ICSMSkin::Save(team, definition, skin)) return false;
        icsmNetworkInventory.Changed();
        Msg("ICSM_SKIN_SELECTED team=%d definition=%d paint=%d shared=%d\n",
            team, definition, paint, ICSMSkin::SharedGun(definition));
        return true;
    }
    bool EquipSkin(int team, int definition, int paint)
    {
        if(ICSMGloves::Definition(team,definition)) {
            if(!ICSMGloves::Save(team,ICSMGloves::Trial(team,definition,paint),true))return false;
            ICSMGloves::Changed();Msg("ICSM_GLOVE_SELECTED team=%d definition=%d paint=%d\n",team,definition,paint);return true;
        }
        const GameItemDefinition_t *item = ICSMSkin::Definition(team, definition);
        if (!item) return false;
        const ICSMSkin::Selection before = ICSMSkin::Read(team, definition);
        const ICSMSkin::Selection skin = ICSMSkin::Trial(definition, paint);
        if (!ICSMSkin::Save(team, definition, skin)) return false;
        if (!Equip(team, item->GetLoadoutSlot(team), definition))
        {
            ICSMSkin::Save(team, definition, before);
            return false;
        }
        Msg("ICSM_SKIN_SAVED team=%d definition=%d paint=%d seed=%d wear=%.6f\n",
            team, definition, skin.paint, skin.seed, skin.wear);
        return true;
    }
    void GetSkins(const v8::FunctionCallbackInfo<v8::Value>& info)
    {
        if (info.Length() != 2 || !info[0]->IsInt32() || !info[1]->IsInt32()) return;
        int team = info[0].As<v8::Int32>()->Value();
        int definition = info[1].As<v8::Int32>()->Value();
        if(ICSMGloves::Definition(team,definition)) {
            auto array=v8::Array::New(info.GetIsolate());int count=0;
            const auto selected=ICSMGloves::Read(team,definition);
            auto add=[&](int paint,const char *name) {
                if(!ICSMGloves::Valid(team,ICSMGloves::Trial(team,definition,paint)))return;
                auto row=v8::Object::New(info.GetIsolate());
                CreateJSOEntry_Number(row,"paint",paint);CreateJSOEntry_Number(row,"sticker_slots",0);
                CreateJSOEntry_String(row,"name",name);CreateJSOEntry_Number(row,"selected",paint==selected.paint?1:0);
                AddPreview(row,definition,paint);array->Set(info.GetIsolate()->GetCurrentContext(),count++,row).Check();
            };
            if(ICSMGloves::Custom(definition)) {
                for(const auto &entry:ICSMGloves::Catalog)
                    if(entry.definition==definition)add(entry.paint,entry.name);
            } else add(0,"默认手套（恢复地图原配）");
            info.GetReturnValue().Set(array);return;
        }
        if (!ICSMSkin::Definition(team, definition)) return;
        const ICSMSkin::Selection selected = ICSMSkin::Read(team, definition);
        auto array = v8::Array::New(info.GetIsolate());
        int count = 0;
        auto add = [&](int paint, const char *name)
        {
            if (!ICSMSkin::Valid(team, definition, ICSMSkin::Trial(definition, paint))) return;
            auto row = v8::Object::New(info.GetIsolate());
            CreateJSOEntry_Number(row, "paint", paint);
            CreateJSOEntry_Number(row, "sticker_slots", ICSMStickers::Slots(definition));
            CreateJSOEntry_String(row, "name", name);
            CreateJSOEntry_Number(row, "selected", selected.paint == paint ? 1 : 0);
            AddPreview(row, definition, paint);
            array->Set(info.GetIsolate()->GetCurrentContext(), count++, row).Check();
        };
        add(0, "原版外观");
        for (unsigned i = 0; i < sizeof(ICSMSkin::Catalog) / sizeof(ICSMSkin::Catalog[0]); ++i)
        {
            const ICSMSkin::CatalogEntry &entry = ICSMSkin::Catalog[i];
            if (entry.definition == definition) add(entry.paint, entry.name);
        }
        info.GetReturnValue().Set(array);
    }
    void GetStickerCatalog(const v8::FunctionCallbackInfo<v8::Value>& info)
    {
        auto array=v8::Array::New(info.GetIsolate());int count=0;
        auto add=[&](int id,const char *name) {
            auto row=v8::Object::New(info.GetIsolate());CreateJSOEntry_Number(row,"sticker",id);
            CreateJSOEntry_String(row,"name",name);
            CFmtStr path("file://{images}/offline_stickers/sticker_%d.png",id);
            CreateJSOEntry_String(row,"image",id?path.Get():"");
            array->Set(info.GetIsolate()->GetCurrentContext(),count++,row).Check();
        };
        add(0,"移除贴纸");
        for(unsigned i=0;i<sizeof(ICSMStickers::Kits)/sizeof(ICSMStickers::Kits[0]);++i) {
            const ICSMStickers::Kit &kit=ICSMStickers::Kits[i];
            if(GetItemSchema()->GetStickerKitDefinition(kit.id))add(kit.id,kit.name);
        }
        info.GetReturnValue().Set(array);
    }
    void GetStickerSlots(const v8::FunctionCallbackInfo<v8::Value>& info)
    {
        if(info.Length()!=3)return;
        int team=info[0]->Int32Value(info.GetIsolate()->GetCurrentContext()).FromMaybe(-1);
        int definition=info[1]->Int32Value(info.GetIsolate()->GetCurrentContext()).FromMaybe(-1);
        int paint=info[2]->Int32Value(info.GetIsolate()->GetCurrentContext()).FromMaybe(-1);
        if(!ICSMSkin::Valid(team,definition,ICSMSkin::Trial(definition,paint)))return;
        auto array=v8::Array::New(info.GetIsolate());const auto chosen=ICSMStickers::Read(definition,paint);
        for(int slot=0;slot<ICSMStickers::Slots(definition);++slot) {
            auto row=v8::Object::New(info.GetIsolate());const auto *kit=ICSMStickers::Find(chosen.ids[slot]);
            CreateJSOEntry_Number(row,"slot",slot);CreateJSOEntry_Number(row,"sticker",chosen.ids[slot]);
            CreateJSOEntry_String(row,"name",kit?kit->name:"空槽位");
            array->Set(info.GetIsolate()->GetCurrentContext(),slot,row).Check();
        }info.GetReturnValue().Set(array);
    }
    bool SaveSticker(int team,int definition,int paint,int slot,int id)
    {
        if(!ICSMStickers::Save(team,definition,paint,slot,id))return false;
        icsmNetworkInventory.Changed();
        Msg("ICSM_STICKER_SAVED team=%d definition=%d paint=%d slot=%d id=%d\n",team,definition,paint,slot,id);return true;
    }
    bool Equip(int team, int slot, int index)
    {
        if(const auto *glove=ICSMGloves::Definition(team,index)) {
            if(slot!=glove->GetLoadoutSlot(team) || !ICSMGloves::Save(team,ICSMGloves::Read(team,index),true))return false;
            ICSMGloves::Changed();return true;
        }
        if (!ICSMLoadout::Definition(team, slot, index)) return false;
        CCSPlayerInventory *inventory = CSInventoryManager()->GetLocalCSInventory();
        if (!CSInventoryManager()->EquipItemInLoadout(team, slot,
            CombinedItemIdMakeFromDefIndexAndPaint(index, 0))) return false;
        if (!ICSMLoadout::Save(inventory))
        {
            ICSMLoadout::Load(inventory);
            Warning("ICSM_LOADOUT save failed; disk selection restored\n");
            return false;
        }
        Msg("ICSM_LOADOUT equipped team=%d slot=%d definition=%d\n", team, slot, index);
        icsmNetworkInventory.Changed();
        return true;
    }

    void GetInventory(const v8::FunctionCallbackInfo<v8::Value>& info)
    {
        if (!info.Length() || !info[0]->IsInt32()) return;
        int team = info[0].As<v8::Int32>()->Value();
        if (team != TEAM_CT && team != TEAM_TERRORIST) return;
        auto array = v8::Array::New(info.GetIsolate());
        int count = 0;
        const auto &definitions = GetItemSchema()->GetItemDefinitionMap();
        FOR_EACH_MAP_FAST(definitions, i)
        {
            const GameItemDefinition_t *def = dynamic_cast<const GameItemDefinition_t *>(definitions[i]);
            if (!def) continue;
            int slot = def->GetLoadoutSlot(team);
            int index = def->GetDefinitionIndex();
            if (!ICSMLoadout::Definition(team, slot, index)) continue;
            const CEconItemView *equipped = CSInventoryManager()->GetLocalCSInventory()->GetItemInLoadout(team, slot);
            auto item = v8::Object::New(info.GetIsolate());
            CreateJSOEntry_Number(item, "definition", index);
            CreateJSOEntry_Number(item, "slot", slot);
            CreateJSOEntry_String(item, "name", def->GetItemBaseName());
            CreateJSOEntry_String(item, "image", def->GetInventoryImage());
            CreateJSOEntry_String(item, "category", GetItemSchema()->GetLoadoutStrings()[slot]);
            // Classification comes from the original item schema, rather
            // than guessed lists of definition IDs in the mobile UI.
            const char *kind = "other";
            switch (def->GetWeaponTypeString() ? WeaponClassFromString(def->GetWeaponTypeString()) : WEAPONTYPE_UNKNOWN)
            {
            case WEAPONTYPE_KNIFE: kind = "melee"; break;
            case WEAPONTYPE_PISTOL: kind = "secondary"; break;
            case WEAPONTYPE_SUBMACHINEGUN: kind = "smg"; break;
            case WEAPONTYPE_RIFLE: kind = "rifle"; break;
            case WEAPONTYPE_SNIPER_RIFLE: kind = "sniper"; break;
            case WEAPONTYPE_SHOTGUN: kind = "shotgun"; break;
            case WEAPONTYPE_MACHINEGUN: kind = "machinegun"; break;
            default: break;
            }
            CreateJSOEntry_String(item, "kind", kind);
            const ICSMSkin::Selection skin = ICSMSkin::Read(team, index);
            CreateJSOEntry_Number(item, "paint", skin.paint);
            AddPreview(item, index, skin.paint);
            const ICSMSkin::CatalogEntry *recipe = ICSMSkin::FindCatalog(index, skin.paint);
            CreateJSOEntry_String(item, "skin_name", recipe ? recipe->name : "原版外观");
            CreateJSOEntry_Number(item, "equipped", equipped && equipped->IsValid() &&
                equipped->GetItemDefinition()->GetDefinitionIndex() == index ? 1 : 0);
            array->Set(info.GetIsolate()->GetCurrentContext(), count++, item).Check();
        }
        const auto selectedGlove=ICSMGloves::Read(team);
        for(int index:ICSMGloves::Definitions) {
            const auto *def=ICSMGloves::Definition(team,index);if(!def)continue;
            const auto skin=ICSMGloves::Read(team,index);
            auto item=v8::Object::New(info.GetIsolate());
            CreateJSOEntry_Number(item,"definition",index);CreateJSOEntry_Number(item,"slot",def->GetLoadoutSlot(team));
            CreateJSOEntry_String(item,"name",def->GetItemBaseName());CreateJSOEntry_String(item,"kind","gloves");
            CreateJSOEntry_String(item,"category","clothing_hands");CreateJSOEntry_Number(item,"paint",skin.paint);
            AddPreview(item,index,skin.paint);
            const auto *recipe=ICSMGloves::FindCatalog(index,skin.paint);
            CreateJSOEntry_String(item,"skin_name",recipe?recipe->name:"地图默认手套");
            CreateJSOEntry_Number(item,"equipped",selectedGlove.definition==index?1:0);
            array->Set(info.GetIsolate()->GetCurrentContext(),count++,item).Check();
        }
        info.GetReturnValue().Set(array);
    }
};

PANORAMA_COMPONENT_API_DEF_BEGIN(CNativeOfflineInventory)
    PANORAMA_COMPONENT_FUNCTION_API_DEF(int, GetPreviewRevision, CNativeOfflineInventory)
    PANORAMA_COMPONENT_FUNCTION_API_DEF(const char *, GetPreview, CNativeOfflineInventory)
    PANORAMA_COMPONENT_FUNCTION_API_DEF(int, GetSkin, CNativeOfflineInventory)
    PANORAMA_COMPONENT_FUNCTION_API_DEF(bool, SaveSkin, CNativeOfflineInventory)
    PANORAMA_COMPONENT_FUNCTION_API_DEF(bool, EquipSkin, CNativeOfflineInventory)
    PANORAMA_COMPONENT_FUNCTION_RAW_API_DEF(void, GetSkins, CNativeOfflineInventory)
    PANORAMA_COMPONENT_FUNCTION_RAW_API_DEF(void, GetInventory, CNativeOfflineInventory)
    PANORAMA_COMPONENT_FUNCTION_RAW_API_DEF(void, GetStickerCatalog, CNativeOfflineInventory)
    PANORAMA_COMPONENT_FUNCTION_RAW_API_DEF(void, GetStickerSlots, CNativeOfflineInventory)
    PANORAMA_COMPONENT_FUNCTION_API_DEF(bool, SaveSticker, CNativeOfflineInventory)
    PANORAMA_COMPONENT_FUNCTION_API_DEF(bool, Equip, CNativeOfflineInventory)
PANORAMA_COMPONENT_API_DEF_END(CNativeOfflineInventory)

void InstallNativeOfflineInventoryBindings()
{
    // Original engine save logic reads these two -1 sentinels, normally
    // declared in the excluded Scaleform video screen. Supply only absent
    // definitions so saved aspect category follows the actual backbuffer.
    static ConVar *nativeWidthRestart=NULL,*nativeHeightRestart=NULL;
    if(!g_pCVar->FindVar("defaultres_restart"))
        nativeWidthRestart=new ConVar("defaultres_restart","-1",FCVAR_NONE,"Used to set video property at device reset.");
    if(!g_pCVar->FindVar("defaultresheight_restart"))
        nativeHeightRestart=new ConVar("defaultresheight_restart","-1",FCVAR_NONE,"Used to set video property at device reset.");
    static CNativeOfflineInventory inventory;
    PANORAMA_COMPONENT_API_INSTALL(CNativeOfflineInventory, &inventory, "NativeInventoryAPI", "Local weapon and knife selections");
}

CON_COMMAND_F(icsm_weapon_state, "Print actual local client weapon and silencer state", FCVAR_RELEASE)
{
    C_CSPlayer *player = C_CSPlayer::GetLocalCSPlayer();
    CWeaponCSBase *weapon = player ? player->GetActiveCSWeapon() : NULL;
    if (!weapon || !weapon->GetItemDefinition()) return;
    Msg("ICSM_CLIENT_WEAPON definition=%d weapon=%s has_silencer=%d detachable=%d silenced=%d switching=%d\n",
        weapon->GetItemDefinitionIndex(), weapon->GetDefinitionName(), weapon->HasSilencer(),
        weapon->HasDetachableSilencer(), weapon->IsSilenced(), weapon->IsSwitchingSilencer());
    Msg("ICSM_CLIENT_WEAPON_MODEL definition=%d view=%s world=%s\n",
        weapon->GetItemDefinitionIndex(), weapon->GetViewModel(), weapon->GetWorldModel());
}

CON_COMMAND_F(icsm_knife_options, "Print selectable knives from the actual local schema", FCVAR_RELEASE)
{
    const auto &definitions = GetItemSchema()->GetItemDefinitionMap();
    for (int team = TEAM_TERRORIST; team <= TEAM_CT; ++team)
    {
        int count = 0;
        FOR_EACH_MAP_FAST(definitions, i)
        {
            const GameItemDefinition_t *def = ICSMLoadout::Definition(team, LOADOUT_POSITION_MELEE, definitions[i]->GetDefinitionIndex());
            if (!def) continue;
            ++count;
            Msg("ICSM_KNIFE_OPTION team=%d definition=%d weapon=%s name=%s image=%s\n",
                team, def->GetDefinitionIndex(), def->GetDefinitionName(), def->GetItemBaseName(), def->GetInventoryImage());
        }
        Msg("ICSM_KNIFE_OPTIONS team=%d count=%d\n", team, count);
    }
}

CON_COMMAND_F(icsm_loadout, "Inspect local inventory or equip: icsm_loadout <team> <slot> <definition>", FCVAR_RELEASE)
{
    CNativeOfflineInventory api;
    if (args.ArgC() == 4)
    {
        bool result = api.Equip(atoi(args[1]), atoi(args[2]), atoi(args[3]));
        Msg("ICSM_LOADOUT result=%d\n", result);
        return;
    }
    if (args.ArgC() != 1) return;
    for (int team = TEAM_TERRORIST; team <= TEAM_CT; ++team)
        for (int slot = LOADOUT_POSITION_MELEE; slot <= LOADOUT_POSITION_LAST_WHEEL_WEAPON; ++slot)
        {
            if (!ICSMLoadout::IsSelectableSlot(slot)) continue;
            const CEconItemView *item = CSInventoryManager()->GetLocalCSInventory()->GetItemInLoadout(team, slot);
            if (item && item->IsValid())
                Msg("ICSM_LOADOUT team=%d slot=%d definition=%d weapon=%s\n", team, slot,
                    item->GetItemDefinition()->GetDefinitionIndex(), item->GetItemDefinition()->GetDefinitionName());
        }
}

CON_COMMAND_F(icsm_skin, "Equip local paint: icsm_skin <team> <definition> <paint>; affects next spawn/purchase", FCVAR_RELEASE)
{
    if (args.ArgC() == 4)
    {
        CNativeOfflineInventory api;
        Msg("ICSM_SKIN_SAVE_RESULT result=%d\n", api.EquipSkin(atoi(args[1]), atoi(args[2]), atoi(args[3])));
    }
}

CON_COMMAND_F(icsm_skin_selection_state, "Inspect saved recipes and schema eligibility for one definition", FCVAR_DONTRECORD)
{
    if (args.ArgC() != 2) return;
    const int definition = atoi(args[1]);
    for (int team = TEAM_TERRORIST; team <= TEAM_CT; ++team)
    {
        const ICSMSkin::Selection skin = ICSMSkin::Read(team, definition);
        Msg("ICSM_SKIN_SELECTION team=%d definition=%d eligible=%d shared=%d paint=%d seed=%d wear=%.6f\n",
            team, definition, ICSMSkin::Definition(team, definition) != NULL,
            ICSMSkin::SharedGun(definition), skin.paint, skin.seed, skin.wear);
    }
}

CON_COMMAND_F(icsm_skin_state, "Print native weapon paint, generated materials and viewmodel overrides", FCVAR_RELEASE)
{
    Msg("ICSM_SKIN_QUALITY_STATE shader=%d texture=%d gpu_level=%d gpu_mem_level=%d mat_phong=%d mat_bumpmap=%d mat_specular=%d mat_picmip=%d\n",
        ICSMSkinQuality::Shader(), ICSMSkinQuality::Texture(), ConVarRef("gpu_level").GetInt(),
        ConVarRef("gpu_mem_level").GetInt(), ConVarRef("mat_phong").GetInt(),
        ConVarRef("mat_bumpmap").GetInt(), ConVarRef("mat_specular").GetInt(), ConVarRef("mat_picmip").GetInt());
    C_CSPlayer *player = C_CSPlayer::GetLocalCSPlayer();
    CWeaponCSBase *weapon = player ? player->GetActiveCSWeapon() : NULL;
    if (!weapon || !weapon->GetEconItemView()) return;
    const CEconItemView *item = weapon->GetEconItemView();
    Msg("ICSM_SKIN_STATE entity=%d definition=%d paint=%d seed=%d wear=%.6f itemid=%llu item_materials=%d weapon_materials=%d\n",
        weapon->entindex(), weapon->GetItemDefinitionIndex(), item->GetCustomPaintKitIndex(),
        item->GetCustomPaintKitSeed(), item->GetCustomPaintKitWear(),
        (unsigned long long)item->GetItemID(), item->GetCustomMaterialCount(), weapon->GetCustomMaterialCount());
    for (int i = 0; i < weapon->GetCustomMaterialCount(); ++i)
    {
        ICustomMaterial *custom = weapon->GetCustomMaterial(i);
        if (!custom) continue;
        Msg("ICSM_SKIN_MATERIAL slot=%d ready=%d base=%s material=%s\n", i, custom->IsValid(),
            custom->GetBaseMaterialName(), custom->GetMaterial() ? custom->GetMaterial()->GetName() : "pending");
        IMaterial *material = custom->GetMaterial();
        if (custom->IsValid() && material)
        {
            bool found = false;
            IMaterialVar *force = material->FindVar("$forcephong", &found, false);
            const int forcePhong = found ? force->GetIntValue() : 0;
            IMaterialVar *diffuse = material->FindVar("$basetexture", &found, false);
            ITexture *texture = found && diffuse->IsTexture() ? diffuse->GetTextureValue() : NULL;
            IMaterialVar *bump = material->FindVar("$bumpmap", &found, false);
            const bool normal = found && bump->IsTexture() && !bump->GetTextureValue()->IsError();
            Msg("ICSM_SKIN_QUALITY_MATERIAL slot=%d force_phong=%d diffuse_width=%d diffuse_height=%d normal_loaded=%d\n",
                i, forcePhong, texture ? texture->GetActualWidth() : 0,
                texture ? texture->GetActualHeight() : 0, normal);
        }
    }
    C_BaseViewModel *view = player->GetViewModel(0);
    if (view) Msg("ICSM_SKIN_VIEWMODEL materials=%d ready=%d\n", view->GetCustomMaterialCount(), view->IsCustomMaterialValid(0));
    int generated = 0, attached = 0, viewed = 0;
    bool ready = true;
    for (int i = 0; i < item->GetCustomMaterialCount(); ++i)
        if (item->GetCustomMaterial(i)) ++generated;
    for (int i = 0; i < weapon->GetCustomMaterialCount(); ++i)
        if (weapon->GetCustomMaterial(i)) { ++attached; ready = ready && weapon->GetCustomMaterial(i)->IsValid(); }
    if (view)
        for (int i = 0; i < view->GetCustomMaterialCount(); ++i)
            if (view->GetCustomMaterial(i)) { ++viewed; ready = ready && view->GetCustomMaterial(i)->IsValid(); }
    const CPaintKit *kit = GetItemSchema()->GetPaintKitDefinition(item->GetCustomPaintKitIndex());
    int expected = item->GetCustomPaintKitIndex() ?
        (kit && kit->bOnlyFirstMaterial ? 1 : item->GetStaticData()->GetPaintData()->Count()) : 0;
    Msg("ICSM_SKIN_SUMMARY definition=%d paint=%d expected=%d generated=%d attached=%d viewed=%d ready=%d\n",
        weapon->GetItemDefinitionIndex(), item->GetCustomPaintKitIndex(), expected, generated, attached, viewed, ready);
}

CON_COMMAND_F(icsm_skin_options, "Validate every bundled skin against the live 2019 schema", FCVAR_RELEASE)
{
    int valid = 0;
    const unsigned count = sizeof(ICSMSkin::Catalog) / sizeof(ICSMSkin::Catalog[0]);
    for (unsigned i = 0; i < count; ++i)
    {
        const ICSMSkin::CatalogEntry &trial = ICSMSkin::Catalog[i];
        const ICSMSkin::Selection recipe = ICSMSkin::Trial(trial.definition, trial.paint);
        bool ct = ICSMSkin::Valid(TEAM_CT, trial.definition, recipe);
        bool t = ICSMSkin::Valid(TEAM_TERRORIST, trial.definition, recipe);
        if (ct || t) ++valid;
        Msg("ICSM_SKIN_OPTION definition=%d paint=%d ct=%d t=%d wear=%.6f\n", trial.definition, trial.paint, ct, t, recipe.wear);
    }
    Msg("ICSM_SKIN_OPTIONS valid=%d expected=%u\n", valid, count);
}

CON_COMMAND_F(icsm_sticker_state,"Inspect original weapon sticker attributes and materials",FCVAR_RELEASE)
{
    C_CSPlayer *player=C_CSPlayer::GetLocalCSPlayer();CWeaponCSBase *weapon=player?player->GetActiveCSWeapon():NULL;
    CEconItemView *item=weapon?weapon->GetAttributeContainer()->GetItem():NULL;if(!item || !item->IsValid())return;
    Msg("ICSM_STICKER_STATE definition=%d slots=%d applied=%d\n",weapon->GetItemDefinitionIndex(),item->GetNumSupportedStickerSlots(),item->ItemHasAnyStickersApplied());
    for(int slot=0;slot<MIN(5,item->GetNumSupportedStickerSlots());++slot) {
        IMaterial *first=item->GetStickerIMaterialBySlotIndex(slot),*third=item->GetStickerIMaterialBySlotIndex(slot,true);
        Msg("ICSM_STICKER_SLOT slot=%d id=%u first=%s third=%s model=%s\n",slot,item->GetStickerAttributeBySlotIndexInt(slot,k_EStickerAttribute_ID,0),
            first&&!first->IsErrorMaterial()?first->GetName():"none",third&&!third->IsErrorMaterial()?third->GetName():"none",item->GetStickerSlotModelBySlotIndex(slot));
    }
}
