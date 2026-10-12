#pragma once
// Compare retained pointers, then use Valve's ownership API only on changes.
inline bool ICSMBindCustomMaterials(const CCustomMaterialOwner *source,CCustomMaterialOwner *target) {
    if(!source || !target)return false;
    bool changed=source->GetCustomMaterialCount()!=target->GetCustomMaterialCount();
    for(int i=0;!changed && i<source->GetCustomMaterialCount();++i)
        changed=source->GetCustomMaterial(i)!=target->GetCustomMaterial(i);
    if(changed)source->DuplicateCustomMaterialsToOther(target);
    return changed;
}
