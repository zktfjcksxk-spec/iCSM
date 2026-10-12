#!/usr/bin/env python3
"""Restore partner-depot sticker attachment using retained Valve attachment APIs."""
from pathlib import Path
P=Path(__file__).resolve().parents[2]; C=Path.home()/'Library/Caches/csgo-ipados-arm64'; B=C/'i4'; D=C/'i5'
def patch_item(path):
 text=path.read_text(errors='surrogateescape')
 old='else if ( GetItemID() > 0 )'
 if old in text:
  assert text.count(old)==1
  text=text.replace(old,'else if ( GetItemID() > 0 || m_NetworkedDynamicAttributesForDemos.GetNumAttributes() > 0 )')
 old='\tif ( !GetSOCData() )\n\t\treturn;\n\n\tm_NetworkedDynamicAttributesForDemos.DestroyAllAttributes();'
 if old in text:
  assert text.count(old)==1
  text=text.replace(old,'''\tif ( !GetSOCData() )
    {
        // Offline items have no SOCache. Reuse the existing raw-bit attribute
        // send table; adding a new network field would break server compatibility.
        if (GetItemID() == 0) {
            m_NetworkedDynamicAttributesForDemos.CopyFrom(static_cast<const CAttributeList&>(m_AttributeList));
            NetworkStateChanged();
        }
        return;
    }

\tm_NetworkedDynamicAttributesForDemos.DestroyAllAttributes();''')
 # The local list wins when the same attribute also exists in the network copy.
 start=text.index('else if ( GetItemID() > 0 ||')
 end=text.index("// If we didn't have a DB backing",start)
 section=text[start:end].replace('OnIterateAttributeValue( pIterator, pAttrDef, value )','OnIterateAttributeValue( GetItemID()==0 ? &iteratorWrapper : pIterator, pAttrDef, value )')
 text=text[:start]+section+text[end:]
 if '#include "offline_sticker_preview_model.inc"' not in text:
  text=text.replace('bool CEconItemView::Update( void )','#include "offline_sticker_preview_model.inc"\n\nbool CEconItemView::Update( void )')
  text=text.replace('\t\tm_pRenderToRTMDL->SetMDL( GetIconDisplayModel(), this );','\t\tm_pRenderToRTMDL->SetMDL( GetIconDisplayModel(), this );\n        ICSMAddInventoryStickers(this,m_pRenderToRTMDL);')
  # Offline sticker recipes must never read another recipe's static iic icon.
  text=text.replace('bool CEconItemView::LoadCachedInventoryImage( void )\n{','bool CEconItemView::LoadCachedInventoryImage( void )\n{\n    if(GetItemID()==0 && ItemHasAnyStickersApplied())return false;')
  text=text.replace('void CEconItemView::SaveInventoryImage( CUtlBuffer &rawImageRgba )\n{','void CEconItemView::SaveInventoryImage( CUtlBuffer &rawImageRgba )\n{\n    if(GetItemID()==0 && ItemHasAnyStickersApplied())return;')
 path.write_text(text,errors='surrogateescape')
 from attribute_perf_patch import apply as optimize_attributes
 optimize_attributes(path)
def apply():
 patch_item(D/'source/game/shared/econ/econ_item_view.cpp')
 rel='source/game/client/c_baseviewmodel.cpp';text=(B/rel).read_text(errors='surrogateescape')
 old='void C_BaseViewModel::AddViewmodelStickers( CEconItemView *pItem, CSWeaponID nWeaponID )\n{\n\t/* Removed for partner depot */\n}'
 assert text.count(old)==1
 text=text.replace(old,'#include "offline_sticker_viewmodel.inc"')
 target=D/rel;target.parent.mkdir(parents=True,exist_ok=True);target.write_text(text,errors='surrogateescape')
 player=D/'source/game/server/cstrike15/cs_player.cpp';text=player.read_text(errors='surrogateescape')
 if '#include "offline_stickers.h"' not in text:text=text.replace('#include "offline_skin.h"','#include "offline_skin.h"\n#include "offline_stickers.h"')
 anchor='weapon->ICSMSetOfflinePaint(skin.paint, skin.seed, skin.wear);'
 if 'ICSMStickers::Apply(' not in text:
  assert text.count(anchor)==1
  text=text.replace(anchor,anchor+'\n            ICSMStickers::Apply(weapon->GetAttributeContainer()->GetItem(), ICSMStickers::Read(weapon->GetItemDefinitionIndex(),skin.paint));')
 player.write_text(text,errors='surrogateescape')
 from glove_patch import apply as apply_gloves
 apply_gloves()
 from weapon_decal_patch import apply as apply_weapon_decals
 apply_weapon_decals()
 print('Restored original schema sticker attachments and offline spawn attributes')
if __name__=='__main__':apply()
