#!/usr/bin/env python3
"""Restore bot inventory handoff using the depot's detach/equip ownership APIs."""
import pathlib

SOURCE = pathlib.Path.home() / 'Library/Caches/csgo-ipados-arm64/i5/source/game/server/cstrike15/cs_player.cpp'
OLD_TRANSFER = '''void CCSPlayer::TransferInventory( CCSPlayer* pTargetPlayer )
{
    // as part of transferring inventory, remove what WE have
\tSetArmorValue( 0 );
\tm_flLastEquippedHelmetTime = 0;
\tm_flLastEquippedArmorTime = 0;
\tm_bHasHelmet = false;
\tm_bHasHeavyArmor = false; // m_bHasHeavyAssaultSuit
\tm_bHasNightVision = false;
\tm_bNightVisionOn = false;

\tRemoveAllItems( true );

\tpTargetPlayer->UpdateVoicePitch();
\tUpdateVoicePitch();
}'''
TRANSFER = '''void CCSPlayer::TransferInventory( CCSPlayer* pTargetPlayer )
{
    if ( !pTargetPlayer || pTargetPlayer == this )
        return;

    // The partner depot retained only source cleanup. Transfer the actual
    // entities before that cleanup so clips, reserve ammo, paint and silencer
    // state survive. Spawn() has already given the recipient default weapons.
    CHandle<CBaseCombatWeapon> hActiveWeapon = GetActiveWeapon();
    int ammo[MAX_AMMO_TYPES];
    for ( int i = 0; i < MAX_AMMO_TYPES; ++i )
        ammo[i] = GetAmmoCount( i );

    pTargetPlayer->RemoveAllItems( true );
    pTargetPlayer->SetArmorValue( ArmorValue() );
    pTargetPlayer->m_flLastEquippedHelmetTime = m_flLastEquippedHelmetTime;
    pTargetPlayer->m_flLastEquippedArmorTime = m_flLastEquippedArmorTime;
    pTargetPlayer->m_bHasHelmet = m_bHasHelmet;
    pTargetPlayer->m_bHasHeavyArmor = m_bHasHeavyArmor;
    pTargetPlayer->m_bHasNightVision = m_bHasNightVision;
    pTargetPlayer->m_bNightVisionOn = m_bNightVisionOn;
    pTargetPlayer->m_bHasDefuser = m_bHasDefuser;
    pTargetPlayer->m_iAccount = m_iAccount;
    pTargetPlayer->m_iAccountMoneyEarnedForNextRound = m_iAccountMoneyEarnedForNextRound;

    int nTransferred = 0;
    for ( int i = 0; i < WeaponCount(); ++i )
    {
        CBaseCombatWeapon *pWeapon = GetWeapon( i );
        if ( pWeapon && Weapon_Detach( pWeapon ) )
        {
            pTargetPlayer->Weapon_Equip( pWeapon );
            ++nTransferred;
        }
    }
    // Equip gives default exhaustible ammo (grenades). Restore the exact
    // player-held counts afterward; gun reserve ammo stays on its entity.
    for ( int i = 0; i < MAX_AMMO_TYPES; ++i )
        pTargetPlayer->SetAmmoCount( ammo[i], i );

    if ( hActiveWeapon.Get() && hActiveWeapon->GetOwner() == pTargetPlayer )
        pTargetPlayer->Weapon_Switch( hActiveWeapon.Get() );

    SetArmorValue( 0 );
    m_flLastEquippedHelmetTime = 0;
    m_flLastEquippedArmorTime = 0;
    m_bHasHelmet = false;
    m_bHasHeavyArmor = false;
    m_bHasNightVision = false;
    m_bNightVisionOn = false;
    // All transferred weapons have been detached, so this cannot delete them.
    RemoveAllItems( true );
    pTargetPlayer->UpdateAddonBits();
    pTargetPlayer->RecalculateCurrentEquipmentValue();
    RecalculateCurrentEquipmentValue();
    pTargetPlayer->UpdateVoicePitch();
    UpdateVoicePitch();
    Msg( "ICSM_BOT_INVENTORY_TRANSFER source=%d target=%d weapons=%d expected_active=%d actual_active=%d\\n",
        entindex(), pTargetPlayer->entindex(), nTransferred,
        hActiveWeapon.Get() ? hActiveWeapon->entindex() : -1,
        pTargetPlayer->GetActiveWeapon() ? pTargetPlayer->GetActiveWeapon()->entindex() : -1 );
}'''
OLD_SELECTION = '\tCBaseCombatWeapon* pWeapon = pBotWeapon ? CSWeapon_OwnsThisType( pBotWeapon->GetEconItemView() ) : NULL;'
SELECTION = '''    // Inventory handoff preserves this exact entity. The partner-depot
    // CSWeapon_OwnsThisType stub returns NULL, and a definition lookup could
    // select a different instance instead of the bot's weapon.
    CBaseCombatWeapon* pWeapon = pBotWeapon && pBotWeapon->GetOwner() == this ? pBotWeapon : NULL;'''


def apply(path=SOURCE):
    text = path.read_text(errors='surrogateescape')
    for old, new in ((OLD_TRANSFER, TRANSFER), (OLD_SELECTION, SELECTION)):
        if new in text:
            assert text.count(new) == 1 and old not in text
        else:
            assert text.count(old) == 1, 'Bot takeover source anchor changed'
            text = text.replace(old, new, 1)
    path.write_text(text, errors='surrogateescape')


if __name__ == '__main__':
    apply()
    print('I5 bot ownership handoff and original active weapon restored')
