#!/usr/bin/env python3
"""Use actual econ definitions for kill and spectator icons, including offline items."""
import pathlib

CACHE = pathlib.Path.home() / 'Library/Caches/csgo-ipados-arm64/i5'
RULES = CACHE / 'source/game/shared/cstrike15/cs_gamerules.cpp'
SPECTATOR = CACHE / 'source/game/client/cstrike15/panorama/hud/csgo_hudspectator.cpp'

OLD_ACTIVE = '''\t\t\tif ( pInflictor == pScorer )
\t\t\t{
\t\t\t\t// If the inflictor is the killer,  then it must be their current weapon doing the damage
\t\t\t\tif ( CBaseCombatWeapon* pInflictorWeapon = pScorer->GetActiveWeapon() )
\t\t\t\t{
\t\t\t\t\tkiller_weapon_name = pInflictorWeapon->GetClassname();
\t\t\t\t\tpInflictorView = pInflictorWeapon->GetEconItemView();
\t\t\t\t}
\t\t\t}
'''
ANCHOR = '\t\t// strip the NPC_* or weapon_* from the inflictor\'s classname'
IDENTITY = '''        // Bullets and knife traces store the damage weapon in CTakeDamageInfo.
        // Its entity class is shared by alternate items (M4A4/M4A1-S, etc.).
        // Do not infer the weapon from the player's equipped loadout: pickups,
        // bot handoffs and a weapon switch after the damage must retain identity.
        CBaseCombatWeapon *pInflictorWeapon = ToBaseCombatWeapon( info.GetWeapon() );
        if ( !pInflictorWeapon )
            pInflictorWeapon = ToBaseCombatWeapon( pInflictor );
        if ( !pInflictorWeapon && pScorer && pInflictor == pScorer )
            pInflictorWeapon = pScorer->GetActiveWeapon();

        if ( pInflictorWeapon )
        {
            killer_weapon_name = pInflictorWeapon->GetClassname();
            pInflictorView = pInflictorWeapon->GetEconItemView();
            // Offline/reference items need a valid definition, not a Steam UID.
            if ( pInflictorView && pInflictorView->IsValid() && pInflictorView->GetStaticData() )
            {
                const char *szDefinition = pInflictorView->GetStaticData()->GetDefinitionName();
                if ( szDefinition && szDefinition[0] )
                    killer_weapon_name = szDefinition;
            }
        }
        else if ( pInflictor && pInflictor != pScorer )
        {
            // Grenades/fire are entities independent of the currently held gun.
            killer_weapon_name = pInflictor->GetClassname();
            if ( !V_strcmp( killer_weapon_name, "planted_c4" ) )
                killer_weapon_name = "weapon_c4";
            else if ( !V_strcmp( killer_weapon_name, "worldspawn" ) )
                killer_weapon_name = "world";
        }

'''
OLD_SET = '\t\tevent->SetInt("attacker", killer_ID );'
NORMALIZE = '''        // Match the depot's GetWeaponNameFromTakeDamageInfo projectile naming.
        // Keep a local buffer: events copy the string, with no static shared state.
        char szProjectileWeapon[256] = {};
        if ( const char *szSuffix = V_strrchr( killer_weapon_name, '_' ) )
        {
            if ( !V_stricmp( szSuffix, "_projectile" ) )
            {
                V_snprintf( szProjectileWeapon, sizeof( szProjectileWeapon ), "%.*s",
                    int( szSuffix - killer_weapon_name ), killer_weapon_name );
                killer_weapon_name = szProjectileWeapon;
            }
        }

'''
OLD_GUARD = 'pItem && pItem->IsValid() && pItem->GetItemIndex() && pItem->GetItemDefinition()'
GUARD = 'pItem && pItem->IsValid() && pItem->GetItemDefinition()'


def apply():
    text = RULES.read_text(errors='surrogateescape')
    if IDENTITY not in text:
        assert text.count(OLD_ACTIVE) == text.count(ANCHOR) == text.count(OLD_SET) == 1
        text = text.replace(OLD_ACTIVE, '', 1)
        text = text.replace(ANCHOR, IDENTITY + ANCHOR, 1)
        text = text.replace(OLD_SET, NORMALIZE + OLD_SET, 1)
        RULES.write_text(text, errors='surrogateescape')
    else:
        assert text.count(IDENTITY) == text.count(NORMALIZE) == 1 and OLD_ACTIVE not in text
    text = SPECTATOR.read_text(errors='surrogateescape')
    if OLD_GUARD in text:
        assert text.count(OLD_GUARD) == 5  # Four live paths and their original comment.
        SPECTATOR.write_text(text.replace(OLD_GUARD, GUARD), errors='surrogateescape')
    else:
        assert text.count(GUARD) == 5


if __name__ == '__main__':
    apply()
    print('I5 kill and spectator icons use actual definitions; projectile fallback restored')
