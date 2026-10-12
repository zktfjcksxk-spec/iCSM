// Schema decal meshes contain Valve's exact UVs, placement and animated bones.
// Derive an attachment, leaving C_BaseViewModel and network entity layouts intact.
class CICSMStickerAttachment : public C_ViewmodelAttachmentModel {
    CMaterialReference material;
    CUtlString sourceModel;
    int sourceIndex=-1;
public:
    void RecordModel(const char *path) {sourceModel=path;sourceIndex=GetModelIndex();}
    bool MatchesModel(const char *path) const {
        return GetModel() && GetModelIndex()==sourceIndex && !V_strcmp(sourceModel.String(),path);
    }
    void SetStickerMaterial(IMaterial *value) { material.Init(value); }
    IMaterial *GetStickerMaterial() const { return material; }
    int InternalDrawModel(int flags,const RenderableInstance_t &instance) override {
        if(!material.IsValid() || material->IsErrorMaterial())return 0;
        modelrender->ForcedMaterialOverride(material);
        int result=C_ViewmodelAttachmentModel::InternalDrawModel(flags,instance);
        modelrender->ForcedMaterialOverride(NULL);return result;
    }
};
void C_BaseViewModel::AddViewmodelStickers(CEconItemView *item,CSWeaponID weaponID) {
    if(!item || !item->IsValid()) {RemoveViewmodelStickers();return;}
    int count=MIN(5,item->GetNumSupportedStickerSlots()),expected=0;
    for(int slot=0;slot<count;++slot) {
        IMaterial *material=item->GetStickerIMaterialBySlotIndex(slot);
        const char *model=item->GetStickerSlotModelBySlotIndex(slot);
        if(!material || material->IsErrorMaterial() || !model || !model[0])continue;
        CICSMStickerAttachment *addon=expected<m_hStickerModelAddons.Count()?
            dynamic_cast<CICSMStickerAttachment*>(m_hStickerModelAddons[expected].Get()):NULL;
        if(addon && addon->MatchesModel(model)) {
            // A replaced decal only changes a retained material. Transient
            // parent changes do not require destroying the animated mesh.
            if(addon->GetStickerMaterial()!=material)addon->SetStickerMaterial(material);
            if(addon->GetMoveParent()!=this)addon->SetParent(this);
            ++expected;continue;
        }
        if(expected<m_hStickerModelAddons.Count()) {
            if(auto *old=m_hStickerModelAddons[expected].Get())old->Remove();
            m_hStickerModelAddons.Remove(expected);
        }
        addon=new CICSMStickerAttachment;
        if(!addon->InitializeAsClientEntity(model,true)) {addon->Release();continue;}
        addon->RecordModel(model);
        addon->SetStickerMaterial(material);addon->SetParent(this);addon->SetLocalOrigin(vec3_origin);
        addon->SetLocalAngles(vec3_angle);addon->UpdatePartitionListEntry();
        addon->CollisionProp()->MarkPartitionHandleDirty();addon->SetViewmodel(this);
        addon->SetUseParentLightingOrigin(true);addon->UpdateVisibility();
        m_hStickerModelAddons.InsertBefore(expected,addon);++expected;RemoveEffects(EF_NODRAW);
        Msg("ICSM_STICKER_VIEWMODEL definition=%d slot=%d id=%u model=%s\n",item->GetItemDefinition()->GetDefinitionIndex(),slot,
            item->GetStickerAttributeBySlotIndexInt(slot,k_EStickerAttribute_ID,0),model);
    }
    while(m_hStickerModelAddons.Count()>expected) {
        if(auto *old=m_hStickerModelAddons[expected].Get())old->Remove();
        m_hStickerModelAddons.Remove(expected);
    }
}
