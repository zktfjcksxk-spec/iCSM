// Same world-model projection as CUI_ItemPreviewPanel::ApplyStickersToItemWorldModel.
// Positions, direction, radius, bone names and UVs come from Valve's schema/model.
#include "bone_setup.h"
static void ICSMAddInventoryStickers(CEconItemView *item,CMergedMDL *model) {
    if(!item->ItemHasAnyStickersApplied())return;
    item->GenerateStickerMaterials();
    MDLCACHE_CRITICAL_SECTION();
    CMDL *mdl=model->GetMDL();MDLHandle_t handle=mdl->GetMDL();
    if(handle==MDLHANDLE_INVALID || g_pMDLCache->IsErrorModel(handle))return;
    studiohdr_t *hdr=g_pMDLCache->GetStudioHdr(handle);
    studiohwdata_t *hw=g_pMDLCache->GetHardwareData(handle);
    if(!hdr || !hw || hdr->numbones<=0)return;
    CStudioHdr studio(hdr,g_pMDLCache);
    CMatRenderContextPtr context(g_pMaterialSystem);
    CMatRenderData<matrix3x4a_t> bones(context,hdr->numbones);
    matrix3x4_t identity;SetIdentityMatrix(identity);mdl->SetUpBones(identity,hdr->numbones,bones.Base());
    mdl->m_DecalHandle=g_pStudioRender->CreateDecalList(hw);
    const char *attachments[]={"sticker_a","sticker_b","sticker_c","sticker_d","sticker_e"};
    for(int slot=0;slot<MIN(5,item->GetNumSupportedStickerSlots());++slot) {
        IMaterial *material=item->GetStickerIMaterialBySlotIndex(slot,true);
        if(!material || material->IsErrorMaterial())continue;
        matrix3x4_t transform;Vector origin,direction,up;
        int bone=Studio_BoneIndexByName(&studio,attachments[slot]);bool attachment=bone>=0;
        if(!attachment)bone=Studio_BoneIndexByName(&studio,item->GetStickerWorldModelBoneParentNameBySlotIndex(slot));
        if(bone<0)continue;
        MatrixCopy(bones[bone],transform);
        if(attachment) {MatrixPosition(transform,origin);Vector left;MatrixVectors(transform,&direction,&left,&up);}
        else {
            Ray_t local;local.Init(item->GetStickerSlotWorldProjectionStartBySlotIndex(slot),item->GetStickerSlotWorldProjectionEndBySlotIndex(slot));
            VectorTransform(local.m_Start,transform,origin);VectorRotate(local.m_Delta,transform,direction);
        }
        VMatrix matrix(transform);up=attachment?matrix.GetUp():matrix.GetLeft();
        Ray_t ray;ray.Init(origin,origin+3*direction);
        g_pStudioRender->AddDecal(mdl->m_DecalHandle,hdr,bones.Base(),ray,up,material,1.2f,0,false,ADDDECAL_TO_ALL_LODS);
    }
}
