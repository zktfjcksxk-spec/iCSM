#!/usr/bin/env python3
"""Restore the shared firing recoil omitted from the partner source depot."""
import pathlib

SOURCE = pathlib.Path.home() / 'Library/Caches/csgo-ipados-arm64/i5/source/game/shared/cstrike15/weapon_csbase.cpp'
STUB = '''void CWeaponCSBase::Recoil( CSWeaponMode weaponMode )
{
\t/** Removed for partner depot **/
}'''
RESTORED = '''void CWeaponCSBase::Recoil( CSWeaponMode weaponMode )
{
    CCSPlayer *pPlayer = GetPlayerOwner();
    if ( !pPlayer )
        return;

    // Restore the omitted call chain, following Kisak-Strike's retail rebuild:
    // SwagSoftware/Kisak-Strike @ 74e082bf01fa81ee77413a5afa6db4466bed2025.
    // Automatic guns follow their spray index; semi-auto guns use the shared
    // prediction seed so client prediction and server simulation agree.
    const int nRecoilIndex = IsFullAuto() ? int( m_flRecoilIndex ) : GetPredictionRandomSeed();
    float flAngle, flMagnitude;
    // This depot retains the schema-driven recoil table, not the legacy cache.
    g_WeaponRecoilData.GetRecoilOffsets( this, weaponMode, nRecoilIndex, flAngle, flMagnitude );
    pPlayer->KickBack( flAngle, flMagnitude );
}'''


def apply(path=SOURCE):
    text = path.read_text(errors='surrogateescape')
    if RESTORED in text:
        assert text.count(RESTORED) == 1 and STUB not in text
        return
    assert text.count(STUB) == 1, 'Recoil source anchor changed'
    path.write_text(text.replace(STUB, RESTORED, 1), errors='surrogateescape')


if __name__ == '__main__':
    apply()
    print('I5 schema recoil and view punch restored on the shared client/server path')
