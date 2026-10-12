#!/usr/bin/env python3
"""Select the retained flat-normal combo for projected world-weapon stickers.

CStudioRender::DrawDecalMaterial builds projected vertices with position,
normal and UV only. WeaponDecal's NORMALMAP=1 combo requires TANGENT. The
first-person attachment models have their own vertex data and keep that combo.
"""
from pathlib import Path

P = Path(__file__).resolve().parents[2]
C = Path.home() / 'Library/Caches/csgo-ipados-arm64/i5'
REL = 'source/materialsystem/stdshaders/weapondecal_dx9_helper.cpp'


def apply():
    original = (C.parent / 'i4' / REL).read_text()
    anchor = '\tbool bNormalMap\t\t\t= ( info.m_nNormalMap != -1 ) && params[info.m_nNormalMap]->IsTexture() && ( ( nDecalStyle == 3 ) || ( nDecalStyle == 4 ) || ( nDecalStyle == 5 ) );'
    assert original.count(anchor) == 1
    replacement = anchor + '''
#if defined(SOURCE_IOS)
    // r_studiodecal.cpp's projected world decals have no tangent stream.
    // Both VS and PS use this flag, selecting their original matching
    // NORMALMAP=0 variants. First-person decal meshes remain unchanged.
    if (bThirdPerson && bNormalMap) {
        bNormalMap = false;
        static unsigned reported = 0;
        if (reported < 12) {
            ++reported;
            Msg("ICSM_WORLD_STICKER_FLAT_NORMAL style=%d\\n", nDecalStyle);
        }
    }
#endif'''
    target = C / REL
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(original.replace(anchor, replacement, 1))
    return target


if __name__ == '__main__':
    print(apply())
