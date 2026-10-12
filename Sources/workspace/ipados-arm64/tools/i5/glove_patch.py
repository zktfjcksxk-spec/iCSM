#!/usr/bin/env python3
"""Connect retained arm attachment and CustomCharacter clothing paths."""
from pathlib import Path
C=Path.home()/'Library/Caches/csgo-ipados-arm64/i5'
def apply():
 p=C/'source/game/client/c_baseviewmodel.cpp';text=p.read_text(errors='surrogateescape')
 if '#include "offline_glove_viewmodel.inc"' not in text:
  anchor='void C_BaseViewModel::UpdateAllViewmodelAddons( void )'
  assert text.count(anchor)==1
  text=text.replace(anchor,'#include "offline_glove_viewmodel.inc"\n\n'+anchor)
 start=text.index('    // Preserve the original player-specific defaults and sleeve overrides.') if '    // Preserve the original player-specific defaults and sleeve overrides.' in text else text.index('\t// add gloves and sleeves',text.index('void C_BaseViewModel::UpdateAllViewmodelAddons( void )'))
 end=text.index('\n\n\t// econ-related addons',start)
 text=text[:start]+'''    // Preserve the original player-specific defaults and sleeve overrides.
    if (pPlayer->m_pViewmodelArmConfig) {
        const auto *config=pPlayer->m_pViewmodelArmConfig;
        CEconItemView *glove=pPlayer==C_CSPlayer::GetLocalCSPlayer()?ICSMGlovesView::cache.Get(pPlayer->GetTeamNumber()):NULL;
        // Keep the complete map-specific default arms until both custom
        // glove materials are finalized. Never draw the unpainted template.
        if(!ICSMGlovesView::Ready(glove))glove=NULL;
        const char *gloveModel=glove?glove->GetPlayerDisplayModel(0):config->szAssociatedGloveModel;
        // Old remote servers may not precache these gloves. Retain safe defaults.
        if(glove && (!gloveModel || modelinfo->GetModelIndex(gloveModel)==-1)) {
            glove=NULL;gloveModel=config->szAssociatedGloveModel;
        }
        const char *sleeve=glove && config->szAssociatedSleeveModelEconOverride && config->szAssociatedSleeveModelEconOverride[0]?
            config->szAssociatedSleeveModelEconOverride:config->szAssociatedSleeveModel;
        const int expected=1+int(sleeve && sleeve[0]);
        const bool restoring=!glove && m_vecViewmodelArmModels.Count()>0 &&
            ICSMGlovesView::CustomModel(m_vecViewmodelArmModels[0].Get());
        bool matches=m_vecViewmodelArmModels.Count()==expected &&
            ICSMGlovesView::SameModel(m_vecViewmodelArmModels[0].Get(),gloveModel);
        if(matches && expected==2)matches=ICSMGlovesView::SameModel(m_vecViewmodelArmModels[1].Get(),sleeve);
        if((glove || restoring) && !matches)RemoveViewmodelArmModels();
        if(m_vecViewmodelArmModels.Count()==0) {
            AddViewmodelArmModel(gloveModel,glove?-1:atoi(config->szSkintoneIndex));
            if(sleeve && sleeve[0])AddViewmodelArmModel(sleeve);
        }
        if(glove && m_vecViewmodelArmModels.Count()>0 && ICSMGlovesView::SameModel(m_vecViewmodelArmModels[0].Get(),gloveModel))
            ICSMGlovesView::Apply(glove,m_vecViewmodelArmModels[0].Get());
    }
'''+text[end:]
 # The stock draw path released/re-added the same material every draw.
 # Keep the original model-index mapping and reference-counted ownership.
 if '#include "offline_material_binding.h"' not in text:
  text=text.replace('#include "tier0/memdbgon.h"','#include "offline_material_binding.h"\n#include "tier0/memdbgon.h"',1)
 original='\t\t// clear first because number of materials may differ (or be 0)'
 if original in text:
  first=text.index(original);last=text.index('\n\t}\n\n#ifdef IRONSIGHT',first)
  text=text[:first]+'        ICSMBindCustomMaterials(pWeapon,this);'+text[last:]
 p.write_text(text,errors='surrogateescape')
 p=C/'source/game/server/cstrike15/cs_player.cpp';text=p.read_text(errors='surrogateescape')
 if '#include "offline_glove_catalog.h"' not in text:
  anchor='#include "offline_stickers.h"';assert text.count(anchor)==1
  text=text.replace(anchor,anchor+'\n#include "offline_glove_catalog.h"')
 precache='''
#if defined(SOURCE_IOS)
    // Only model precache is shared; clothing composites remain limited to
    // the two equipped local recipes. Map defaults are still loaded below.
    for(int index:ICSMGloves::Definitions) {
        if(!ICSMGloves::Custom(index))continue;
        if(const auto *glove=GetItemSchema()->GetItemDefinition(index)) {
            const char *model=glove->GetBasePlayerDisplayModel();
            if(model && model[0]) {PrecacheModel(model);DevMsg("ICSM_GLOVE_PRECACHE %s\\n",model);}
        }
    }
#endif
'''
 if 'ICSM_GLOVE_PRECACHE' in text:
  marker=text.index('ICSM_GLOVE_PRECACHE');start=text.rfind('\n#if defined(SOURCE_IOS)',0,marker);end=text.index('#endif',marker)+len('#endif')
  assert start!=-1
  text=text[:start]+precache.rstrip('\n')+text[end:]
 else:
  anchor='\tPlayerModelInfo::GetPtr()->InitializeForCurrentMap();';assert text.count(anchor)==1
  text=text.replace(anchor,anchor+precache)
 p.write_text(text,errors='surrogateescape')
 print('Complete offline glove catalog derived; original arm/clothing APIs and safe pending-material fallback retained')
if __name__=='__main__':apply()
