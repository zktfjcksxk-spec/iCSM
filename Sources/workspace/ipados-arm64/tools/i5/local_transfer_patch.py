#!/usr/bin/env python3
"""Only initialize Source's local-copy tables when that transfer mode is active."""
from pathlib import Path

C = Path.home() / 'Library/Caches/csgo-ipados-arm64/i5'
BASE = C.parent / 'i4'
relative = 'source/engine/servermsghandler.cpp'
text = (BASE / relative).read_text()
old = '\tif ( !demoplayer->IsPlayingBack() )\n\t{\n\t\tCLocalNetworkBackdoor::InitFastCopy();\n\t}'
assert text.count(old) == 1
text = text.replace(old, '''\t// CS:GO uses the ordinary snapshot/RecvTable path (sv_packedentities.cpp).
\t// Offset-copy tables are only valid/needed for an active local backdoor.
\t// Do not traverse unused SendTable proxies during ordinary map signon.
\tif ( g_pLocalNetworkBackdoor && !demoplayer->IsPlayingBack() )
\t{
\t\tCLocalNetworkBackdoor::InitFastCopy();
\t}''', 1)
path = C / relative
path.parent.mkdir(parents=True, exist_ok=True)
path.write_text(text)
print('Local transfer initialization gated by the actual active Source mode.')
