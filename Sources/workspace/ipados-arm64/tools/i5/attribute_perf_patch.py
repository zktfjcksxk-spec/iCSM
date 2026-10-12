#!/usr/bin/env python3
"""Avoid quadratic schema resolution in cosmetic-rich item attribute iteration.

GetStaticData() resolves GetAttribIndex() in the schema. Compare that stored
index before resolving the definition; retain the original validity check.
The per-call negative filter never replaces an exact membership check.
"""
from pathlib import Path


def apply(path=None):
    path = path or Path.home() / 'Library/Caches/csgo-ipados-arm64/i5/source/game/shared/econ/econ_item_view.cpp'
    text = path.read_text()
    old = 'if ( pAttr && pAttr->GetStaticData() && pAttr->GetStaticData()->GetDefinitionIndex() == unAttrDefIndex )'
    new = 'if ( pAttr && (fast ? pAttr->GetAttribIndex() == unAttrDefIndex && pAttr->GetStaticData() : pAttr->GetStaticData() && pAttr->GetStaticData()->GetDefinitionIndex() == unAttrDefIndex) )'
    intermediate = 'if ( pAttr && pAttr->GetAttribIndex() == unAttrDefIndex && pAttr->GetStaticData() )'
    if intermediate in text:
        text = text.replace(intermediate, old)
    if old in text:
        assert text.count(old) == 2
        text = text.replace(old, new)
    assert text.count(new) == 2
    for signature in ('CEconItemAttribute *CAttributeList::GetAttributeByDefIndex( uint16 unAttrDefIndex )',
                      'const CEconItemAttribute *CAttributeList::GetAttributeByDefIndex( uint16 unAttrDefIndex ) const'):
        anchor = signature + '\n{\n'
        if anchor+'\tconst bool fast = ICSMFastAttributeLookup();' not in text:
            assert text.count(anchor) == 1
            text = text.replace(anchor, anchor+'\tconst bool fast = ICSMFastAttributeLookup();\n')
    start = text.index('void CEconItemView::IterateAttributes(')
    end = text.index('//-----------------------------------------------------------------------------\n// Purpose:', start)
    body = text[start:end]
    if 'm_localAttributeMask' not in body:
        anchor = '\t\t\tAssert( m_pIterator );\n\t\t}'
        assert body.count(anchor) == 1
        body = body.replace(anchor, '''\t\t\tAssert( m_pIterator );
            // Built from the live local list on each iteration: no stale cache,
            // entity layout changes, schema ownership or mutation hooks.
            m_fast = ICSMFastAttributeLookup();
            m_localAttributeMask[0] = m_localAttributeMask[1] = 0;
            for (int n=0; n<pEconItemView->GetNumAttributes(); ++n) {
                unsigned id=pEconItemView->GetAttribute(n)->GetAttribIndex();
                m_localAttributeMask[(id >> 6) & 1] |= uint64(1) << (id & 63);
            }
\t\t}''')
        anchor = '\t\t\treturn m_pEconItemView->GetAttributeByDefIndex( pAttrDef->GetDefinitionIndex() )'
        assert body.count(anchor) == 4
        body = body.replace(anchor, '\t\t\treturn HasLocalAttribute( pAttrDef->GetDefinitionIndex() )')
        anchor = '\tprivate:\n\t\tconst CEconItemView *m_pEconItemView;'
        assert body.count(anchor) == 1
        body = body.replace(anchor, '''\tprivate:
        bool HasLocalAttribute(unsigned id) const {
            if (m_fast && !(m_localAttributeMask[(id >> 6) & 1] & (uint64(1) << (id & 63))))
                return false;
            // Collisions only take the original exact lookup; never suppress
            // a different attribute that shares the same filter bit.
            return m_pEconItemView->GetAttributeByDefIndex(id) != NULL;
        }
        uint64 m_localAttributeMask[2];
        bool m_fast;
\t\tconst CEconItemView *m_pEconItemView;''')
        text = text[:start] + body + text[end:]
    # Upgrade the first local iteration without losing its existing exact path.
    if 'm_localAttributeMask' in text and 'bool m_fast;' not in text:
        text = text.replace('m_localAttributeMask[0] = m_localAttributeMask[1] = 0;',
                            'm_fast = ICSMFastAttributeLookup();\n            m_localAttributeMask[0] = m_localAttributeMask[1] = 0;')
        text = text.replace('if (!(m_localAttributeMask[', 'if (m_fast && !(m_localAttributeMask[')
        text = text.replace('uint64 m_localAttributeMask[2];', 'uint64 m_localAttributeMask[2];\n        bool m_fast;')
    if '#include "attribute_lookup_ios.inc"' not in text:
        text = text.replace('void CEconItemView::IterateAttributes(', '#include "attribute_lookup_ios.inc"\n\nvoid CEconItemView::IterateAttributes(', 1)
    path.write_text(text)
    print('Live item attribute lookup: index-first resolution and exact negative filter')


if __name__ == '__main__':
    apply()
