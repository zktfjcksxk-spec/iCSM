// Original item attributes and schema slots. No free-form placement or entity ABI changes.
#pragma once
#include "offline_skin.h"
#include "offline_sticker_catalog.h"
namespace ICSMStickers {
static const int MaxSlots=5;
struct Selection { int ids[MaxSlots]; Selection() { V_memset(ids,0,sizeof(ids)); } };
inline int Slots(int definition) {
    const GameItemDefinition_t *item=dynamic_cast<const GameItemDefinition_t*>(GetItemSchema()->GetItemDefinition(definition));
    return item && item->GetWeaponSlot()!=GEAR_SLOT_KNIFE ? MIN(MaxSlots,item->GetNumSupportedStickerSlots()) : 0;
}
inline bool Valid(int definition,int slot,int id) {
    return slot>=0 && slot<Slots(definition) && (id==0 || (Find(id) && GetItemSchema()->GetStickerKitDefinition(id)));
}
inline void Path(char *out,int size,int definition,int paint) { V_snprintf(out,size,"recipes/%d/%d",definition,paint); }
inline Selection Read(int definition,int paint) {
    Selection result;if(!filesystem || !Slots(definition))return result;
    KeyValues::AutoDelete data(new KeyValues("iCSMOfflineStickers"));
    if(!data->LoadFromFile(filesystem,"cfg/icsm-stickers.kv","MOD") || data->GetInt("version")!=1)return result;
    char path[64];Path(path,sizeof(path),definition,paint);KeyValues *entry=data->FindKey(path);
    if(entry)for(int slot=0;slot<Slots(definition);++slot) {
        char key[8];V_snprintf(key,sizeof(key),"%d",slot);int id=entry->GetInt(key);
        if(Valid(definition,slot,id))result.ids[slot]=id;
    }return result;
}
inline bool Save(int team,int definition,int paint,int slot,int id) {
    if(!filesystem || !ICSMSkin::Valid(team,definition,ICSMSkin::Trial(definition,paint)) || !Valid(definition,slot,id))return false;
    KeyValues::AutoDelete data(new KeyValues("iCSMOfflineStickers"));
    if(data->LoadFromFile(filesystem,"cfg/icsm-stickers.kv","MOD") && data->GetInt("version")!=1)return false;
    data->SetInt("version",1);char path[64];Path(path,sizeof(path),definition,paint);
    char key[8];V_snprintf(key,sizeof(key),"%d",slot);data->FindKey(path,true)->SetInt(key,id);
    filesystem->CreateDirHierarchy("cfg","MOD");
    if(!data->SaveToFile(filesystem,"cfg/icsm-stickers.tmp","MOD"))return false;
    if(!filesystem->RenameFile("cfg/icsm-stickers.tmp","cfg/icsm-stickers.kv","MOD"))return false;
    return Read(definition,paint).ids[slot]==id;
}
inline void Apply(CEconItemView *item,const Selection &selection) {
    if(!item)return;
    for(int slot=0;slot<MIN(MaxSlots,item->GetItemDefinition()?item->GetItemDefinition()->GetNumSupportedStickerSlots():0);++slot) {
        const char *suffixes[]={"id","wear","scale","rotation"};
        for(int a=0;a<4;++a) {
            char name[48];V_snprintf(name,sizeof(name),"sticker slot %d %s",slot,suffixes[a]);
            if(!selection.ids[slot]) {
                if(item->GetAttributeList()->GetAttributeByName(name))item->SetOrAddAttributeValueByName(name,a==2?1.0f:0.0f);
                continue;
            }
            // ID attributes store uint32 bits in the original float carrier.
            float value=0;uint32 id=selection.ids[slot];
            if(a==0)V_memcpy(&value,&id,sizeof(value));else if(a==2)value=1;
            item->SetOrAddAttributeValueByName(name,value);
        }
    }
    // Updating an existing attribute does not invalidate the manager cache in
    // CAttributeList::SetOrAddAttributeValueByName; adding one does. Invalidate
    // explicitly so replacing a populated slot also refreshes its consumers.
    item->GetAttributeList()->UpdateManagerCache();
#ifdef GAME_DLL
    item->InitNetworkedDynamicAttributesForDemos();
#endif
}
}
