#!/usr/bin/env python3
"""Derive private server inventory from the original Source entry points."""
import argparse
import json
from pathlib import Path

work = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument('--source-root', type=Path, default=work / 'CSGO_2019_Source/cstrike15_src-2019')
parser.add_argument('--output', type=Path, default=work / 'macos-server/source')
args = parser.parse_args()
args.source_root = args.source_root.resolve()
args.output = args.output.resolve()

def derive(relative, edits):
    original = args.source_root / relative
    before = original.read_bytes()
    text = before.decode(errors='surrogateescape')
    for old, new in edits:
        assert text.count(old) == 1, (relative, old[:100])
        text = text.replace(old, new, 1)
    destination = args.output / relative
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(text, errors='surrogateescape')
    assert original.read_bytes() == before

derive('game/server/cstrike15/cs_player.cpp', [
    ('#include "cs_player.h"', '#include "cs_player.h"\n#include "offline_loadout.h"\n#include "network_inventory_server.h"'),
    ('void CCSPlayer::GiveDefaultItems()\n{', 'void CCSPlayer::GiveDefaultItems()\n{\n    ICSMNetworkInventory::ApplyLoadout(this);'),
    ('bool CCSPlayer::HandleCommand_JoinTeam( int team, bool bQueue, int iCoachTeam  )\n{',
     'bool CCSPlayer::HandleCommand_JoinTeam( int team, bool bQueue, int iCoachTeam  )\n{\n    if (ICSMNetworkInventory::DeferJoin(this, team, bQueue, iCoachTeam)) return true;'),
    ('BuyResult_e CCSPlayer::HandleCommand_Buy_Internal( loadout_positions_t nPos, const CEconItemView** ppPurchasedItem, bool bAddToRebuy/* = true */ ) \n{',
     'BuyResult_e CCSPlayer::HandleCommand_Buy_Internal( loadout_positions_t nPos, const CEconItemView** ppPurchasedItem, bool bAddToRebuy/* = true */ ) \n{\n    ICSMNetworkInventory::ApplyLoadout(this);'),
    ('void CCSPlayer::FindMatchingWeaponsForTeamLoadout( const char *pchName, int nTeam, bool bMustBeTeamSpecific, CUtlVector< const CEconItemView* > &matchingWeapons )\n{\n\t/** Removed for partner depot **/\n}',
     '''void CCSPlayer::FindMatchingWeaponsForTeamLoadout( const char *pchName, int nTeam, bool bMustBeTeamSpecific, CUtlVector< const CEconItemView* > &matchingWeapons )
{
    if (IsBot() || !pchName || (nTeam != TEAM_CT && nTeam != TEAM_TERRORIST)) return;
    const GameItemDefinition_t *requested = dynamic_cast<const GameItemDefinition_t *>(GetItemSchema()->GetItemDefinitionByName(pchName));
    if (!requested || !ICSMLoadout::Definition(nTeam, requested->GetLoadoutSlot(nTeam), requested->GetDefinitionIndex())) return;
    ICSMNetworkInventory::ApplyLoadout(this);
    const CEconItemView *item = Inventory()->GetItemInLoadoutFilteredByProhibition(nTeam, requested->GetLoadoutSlot(nTeam));
    if (item && item->IsValid() && (!bMustBeTeamSpecific || item->GetStaticData()->GetUsedByTeam() == nTeam)) matchingWeapons.AddToTail(item);
}'''),
    ('\t\tbool bAllowCustomKnifeSpawns = false;', '''\t\tbool bAllowCustomKnifeSpawns = false;
        if (!IsBot() && IsBeingGivenItem())
        {
            const CEconItemView *selected = Inventory()->GetItemInLoadout(GetTeamNumber(), LOADOUT_POSITION_MELEE);
            if (selected && selected->IsValid() &&
                ICSMLoadout::Definition(GetTeamNumber(), LOADOUT_POSITION_MELEE, selected->GetItemDefinition()->GetDefinitionIndex()) &&
                pWeapon->GetItemDefinitionIndex() == selected->GetItemDefinition()->GetDefinitionIndex()) bAllowCustomKnifeSpawns = true;
        }'''),
    ('\tpItem->AddSpawnFlags( SF_NORESPAWN );', '    ICSMNetworkInventory::ApplyPaint(this, dynamic_cast<CWeaponCSBase *>(pItem));\n\tpItem->AddSpawnFlags( SF_NORESPAWN );'),
])

derive('game/server/gameinterface.cpp', [
    ('#include "cbase.h"', '#include "cbase.h"\n#include "cs_player.h"\n#include "network_inventory_server.h"'),
    ('\tg_pGameRules->ClientCommandKeyValues( pEntity, pKeyValues );',
     '    if (ICSMNetworkInventory::Receive(ToCSPlayer(CBaseEntity::Instance(pEntity)), pKeyValues)) return;\n\tg_pGameRules->ClientCommandKeyValues( pEntity, pKeyValues );'),
])

derive('game/shared/econ/econ_entity.cpp', [
    ('#include "cbase.h"', '#include "cbase.h"\n#include "offline_skin.h"'),
    ('\tpEconItemView->InitNetworkedDynamicAttributesForDemos();', '''    // The private inventory stores validated local attributes, not SOC data.
    // Reuse the original fallback SendProps; keep entity ABI and send tables.
    if (pEconItemView->IsValid() && pEconItemView->GetItemID() == 0)
    {
        ICSMSkin::Selection recipe;
        recipe.paint = pEconItemView->GetCustomPaintKitIndex();
        recipe.seed = pEconItemView->GetCustomPaintKitSeed();
        recipe.wear = pEconItemView->GetCustomPaintKitWear();
        if (ICSMSkin::ValidRecipe(pEconItemView->GetItemDefinition()->GetDefinitionIndex(), recipe))
        {
            m_nFallbackPaintKit = recipe.paint;
            m_nFallbackSeed = recipe.seed;
            m_flFallbackWear = recipe.wear;
        }
    }
\tpEconItemView->InitNetworkedDynamicAttributesForDemos();'''),
])

# The partner depot removed these capability queries. Restore the same schema
# contract used by the native iOS client, keeping MP5-SD's integrated type (2)
# distinct from the detachable type (1). No entity layout or SendProp changes.
derive('game/shared/cstrike15/weapon_csbase.h', [
    ('''\tbool HasDetachableSilencer( void ) const
\t{
//\t\treturn GetCSWpnData().HasDetachableSilencer();
\t\treturn false;
\t}

\tbool HasSilencer( void ) const
\t{
//\t\treturn GetCSWpnData().HasSilencer();
\t\treturn false;
\t}''', '''\tbool HasDetachableSilencer( void ) const
\t{
        static CSchemaAttributeDefHandle attribute("has silencer");
        uint32 type = WEAPONSILENCER_NONE;
        const CEconItemView *item = GetEconItemView();
        return item && item->IsValid() && item->FindAttribute(attribute, &type) &&
            type == WEAPONSILENCER_DETACHABLE;
\t}

\tbool HasSilencer( void ) const
\t{
        return GetCSWpnData().HasSilencer(GetEconItemView());
\t}'''),
])
header = args.source_root / 'game/shared/cstrike15/weapon_csbase.h'
overlay = {'version': 0, 'use-external-names': False, 'roots': [
    {'type': 'directory', 'name': str(header.parent), 'contents': [
        {'type': 'file', 'name': header.name,
         'external-contents': str(args.output / header.relative_to(args.source_root))}]}]}
(args.output / 'weapon-header-overlay.json').write_text(json.dumps(overlay, indent=2) + '\n')
print('Private inventory derived; original Source files unchanged')

# Use exactly the same source-derived gameplay fixes as the mobile client.
# These helpers never execute a game or edit the original source tree.
import importlib.util
fixes = Path(__file__).resolve().parent / 'shared_fixes'
def load_fix(name):
    spec = importlib.util.spec_from_file_location(name, fixes / (name + '.py'))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module
load_fix('bot_takeover_patch').apply(args.output / 'game/server/cstrike15/cs_player.cpp')
weapon = Path('game/shared/cstrike15/weapon_csbase.cpp')
(args.output / weapon).parent.mkdir(parents=True, exist_ok=True)
(args.output / weapon).write_bytes((args.source_root / weapon).read_bytes())
load_fix('recoil_patch').apply(args.output / weapon)
rules = Path('game/shared/cstrike15/cs_gamerules.cpp')
text = (args.source_root / rules).read_text(errors='surrogateescape')
icons = load_fix('weapon_icons_patch')
assert text.count(icons.OLD_ACTIVE) == text.count(icons.ANCHOR) == text.count(icons.OLD_SET) == 1
text = text.replace(icons.OLD_ACTIVE, '', 1).replace(icons.ANCHOR, icons.IDENTITY + icons.ANCHOR, 1)
text = text.replace(icons.OLD_SET, icons.NORMALIZE + icons.OLD_SET, 1)
(args.output / rules).write_text(text, errors='surrogateescape')
print('Build 200 recoil, bot inventory handoff, kill icons and sticker attributes derived')

# Replicate validated local sticker attributes using the original demo attribute
# SendProps, exactly as the mobile server path does (no entity layout changes).
derive('game/shared/econ/econ_item_view.cpp', [
    ('else if ( GetItemID() > 0 )',
     'else if ( GetItemID() > 0 || m_NetworkedDynamicAttributesForDemos.GetNumAttributes() > 0 )'),
    ('\tif ( !GetSOCData() )\n\t\treturn;\n\n\tm_NetworkedDynamicAttributesForDemos.DestroyAllAttributes();',
     '''\tif ( !GetSOCData() )
    {
        if (GetItemID()==0) {
            m_NetworkedDynamicAttributesForDemos.CopyFrom(static_cast<const CAttributeList&>(m_AttributeList));
            NetworkStateChanged();
        }
        return;
    }
\tm_NetworkedDynamicAttributesForDemos.DestroyAllAttributes();'''),
])
