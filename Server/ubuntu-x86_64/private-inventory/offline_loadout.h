// Local weapon/knife loadouts. Definitions, team eligibility and slots come from
// the original item schema; no account, owned-item or GC data is synthesized.
#pragma once
#include "cstrike15_item_inventory.h"
#include "game_item_schema.h"
#include "filesystem.h"

namespace ICSMLoadout
{
static const char *const File = "cfg/icsm-loadout.kv";

inline bool IsWeaponSlot(int slot)
{
    return slot >= LOADOUT_POSITION_FIRST_WHEEL_WEAPON &&
           slot <= LOADOUT_POSITION_LAST_WHEEL_WEAPON;
}

inline bool IsSelectableSlot(int slot)
{
    return slot == LOADOUT_POSITION_MELEE || IsWeaponSlot(slot);
}

inline const GameItemDefinition_t *Definition(int team, int slot, int index)
{
    if ((team != TEAM_CT && team != TEAM_TERRORIST) || !IsSelectableSlot(slot) ||
        index <= 0 || index > 65535) return NULL;
    const GameItemDefinition_t *def = dynamic_cast<const GameItemDefinition_t *>(
        GetItemSchema()->GetItemDefinition(index));
    const CEconItemView *base = CSInventoryManager()->GetBaseItemForTeam(team, slot);
    if (!def || !def->CanBeUsedByTeam(team) ||
        def->GetLoadoutSlot(team) != slot || def->GetDefaultLoadoutSlot() != slot ||
        !base || !base->IsValid()) return NULL;
    if (slot == LOADOUT_POSITION_MELEE)
    {
        // Special knives inherit the original melee prefab, not baseitem=1.
        // Exclude axes, the Arms Race gold knife and event-only melee slots.
        if (def->GetWeaponSlot() != GEAR_SLOT_KNIFE ||
            !def->GetItemClass() || V_strcmp(def->GetItemClass(), "weapon_knife")) return NULL;
    }
    else if (!def->IsBaseItem() ||
        (def->GetWeaponSlot() != GEAR_SLOT_PISTOL && def->GetWeaponSlot() != GEAR_SLOT_RIFLE)) return NULL;
    return def;
}

inline const char *SlotName(int slot)
{
    return GetItemSchema()->GetLoadoutStringsSubPositions()[slot];
}

inline void Load(CCSPlayerInventory *inventory)
{
    if (!inventory || !filesystem) return;
    KeyValues::AutoDelete data(new KeyValues("iCSMOfflineLoadout"));
    const bool loaded = data->LoadFromFile(filesystem, File, "MOD");
    for (int team = TEAM_TERRORIST; team <= TEAM_CT; ++team)
    {
        KeyValues *choices = loaded && data->GetInt("version") == 1 ?
            data->FindKey(team == TEAM_CT ? "ct" : "t") : NULL;
        for (int slot = LOADOUT_POSITION_MELEE;
             slot <= LOADOUT_POSITION_LAST_WHEEL_WEAPON; ++slot)
        {
            if (!IsSelectableSlot(slot)) continue;
            int index = choices ? choices->GetInt(SlotName(slot), 0) : 0;
            if (!Definition(team, slot, index)) index = 0;
            CEconItemView *old = inventory->FindDefaultEquippedDefinitionItemBySlot(team, slot);
            if ((!old && !index) || (old && old->IsValid() &&
                old->GetItemDefinition()->GetDefinitionIndex() == index)) continue;
            inventory->SetDefaultEquippedDefinitionItemBySlot(team, slot, index);
        }
    }
}

inline bool Save(CCSPlayerInventory *inventory)
{
    if (!inventory || !filesystem) return false;
    KeyValues::AutoDelete data(new KeyValues("iCSMOfflineLoadout"));
    data->SetInt("version", 1);
    for (int team = TEAM_TERRORIST; team <= TEAM_CT; ++team)
    {
        KeyValues *choices = data->FindKey(team == TEAM_CT ? "ct" : "t", true);
        for (int slot = LOADOUT_POSITION_MELEE;
             slot <= LOADOUT_POSITION_LAST_WHEEL_WEAPON; ++slot)
        {
            if (!IsSelectableSlot(slot)) continue;
            const CEconItemView *item = inventory->GetItemInLoadout(team, slot);
            if (!item || !item->IsValid()) continue;
            int index = item->GetItemDefinition()->GetDefinitionIndex();
            if (Definition(team, slot, index)) choices->SetInt(SlotName(slot), index);
        }
    }
    filesystem->CreateDirHierarchy("cfg", "MOD");
    if (!data->SaveToFile(filesystem, "cfg/icsm-loadout.tmp", "MOD")) return false;
    return filesystem->RenameFile("cfg/icsm-loadout.tmp", File, "MOD");
}
}
