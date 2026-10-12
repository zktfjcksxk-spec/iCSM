// Only glove/paint pairs audited in the supplied 2019 depot are selectable.
#pragma once
#include "offline_loadout.h"
#include "offline_glove_catalog.h"

namespace ICSMGloves {
static const char *const File="cfg/icsm-gloves.kv";
struct Selection {
    int definition,paint,seed;float wear;
    Selection():definition(0),paint(0),seed(0),wear(0){}
};
inline int Default(int team) {return team==TEAM_CT?5029:5028;}
inline const GameItemDefinition_t *Definition(int team,int index) {
    if(team!=TEAM_CT && team!=TEAM_TERRORIST)return NULL;
    if(!Custom(index) && index!=Default(team))return NULL;
    const auto *item=dynamic_cast<const GameItemDefinition_t*>(GetItemSchema()->GetItemDefinition(index));
    return item && item->GetBasePlayerDisplayModel() && item->GetLoadoutSlot(team)!=LOADOUT_POSITION_INVALID ? item:NULL;
}
inline bool Valid(int team,const Selection &s) {
    if(!Definition(team,s.definition) || s.seed!=0 || !IsFinite(s.wear))return false;
    if(s.definition==Default(team))return s.paint==0 && s.wear==0;
    if(!FindCatalog(s.definition,s.paint))return false;
    const CPaintKit *kit=GetItemSchema()->GetPaintKitDefinition(s.paint);
    return kit && Definition(team,s.definition)->GetPaintData()->Count()==2 &&
        s.wear>=kit->flWearRemapMin && s.wear<=kit->flWearRemapMax;
}
inline Selection Trial(int team,int definition,int paint) {
    Selection s;s.definition=definition;s.paint=paint;
    const CPaintKit *kit=paint?GetItemSchema()->GetPaintKitDefinition(paint):NULL;
    if(kit)s.wear=clamp(.02f,kit->flWearRemapMin,kit->flWearRemapMax);
    return s;
}
inline Selection Read(int team,int definition=0) {
    Selection s=Trial(team,definition?definition:Default(team),DefaultPaint(definition));
    if(!filesystem || !Definition(team,s.definition))return s;
    KeyValues::AutoDelete data(new KeyValues("iCSMGloves"));
    if(!data->LoadFromFile(filesystem,File,"MOD") || data->GetInt("version")!=1)return s;
    const char *side=team==TEAM_CT?"ct":"t";
    if(!definition) {
        int equipped=data->GetInt(CFmtStr("%s/equipped",side),Default(team));
        if(Definition(team,equipped))s=Trial(team,equipped,DefaultPaint(equipped));
    }
    KeyValues *entry=data->FindKey(CFmtStr("%s/recipes/%d",side,s.definition));
    if(entry) {
        Selection value=s;value.paint=entry->GetInt("paint");value.seed=entry->GetInt("seed");value.wear=entry->GetFloat("wear");
        if(Valid(team,value))s=value;
    }
    return s;
}
inline bool Save(int team,const Selection &s,bool equip) {
    if(!filesystem || !Valid(team,s))return false;
    KeyValues::AutoDelete data(new KeyValues("iCSMGloves"));
    if(data->LoadFromFile(filesystem,File,"MOD") && data->GetInt("version")!=1)return false;
    data->SetInt("version",1);const char *side=team==TEAM_CT?"ct":"t";
    KeyValues *entry=data->FindKey(CFmtStr("%s/recipes/%d",side,s.definition),true);
    entry->SetInt("paint",s.paint);entry->SetInt("seed",s.seed);entry->SetFloat("wear",s.wear);
    if(equip)data->SetInt(CFmtStr("%s/equipped",side),s.definition);
    filesystem->CreateDirHierarchy("cfg","MOD");
    if(!data->SaveToFile(filesystem,"cfg/icsm-gloves.tmp","MOD") ||
       !filesystem->RenameFile("cfg/icsm-gloves.tmp",File,"MOD"))return false;
    Selection saved=Read(team,s.definition);
    return saved.paint==s.paint && saved.seed==s.seed && saved.wear==s.wear && (!equip || Read(team).definition==s.definition);
}
// Implemented in the viewmodel module. Menu writes only invalidate the cache;
// disk I/O and clothing composites never run on every rendered frame.
void Changed();
}
