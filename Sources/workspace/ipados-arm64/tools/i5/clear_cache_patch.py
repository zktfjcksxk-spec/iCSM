#!/usr/bin/env python3
"""Move immutable rectangle-clear work to its existing pipeline/state cache misses."""
from pathlib import Path
path=Path.home()/'Library/Caches/csgo-ipados-arm64/i5/metal/source_device.mm'
text=path.read_text()
if '#include "clear_rects_ios.inc"' not in text:
    start=text.index('    static void clearRects(')
    end=text.index('    static MTLSamplerAddressMode address(',start)
    text=text[:start]+'#include "clear_rects_ios.inc"\n'+text[end:]
path.write_text(text)
text=path.read_text()
if '#include "clear_color_policy.h"' not in text:
    text=text.replace('#include "metal_perf_ios.inc"','#include "metal_perf_ios.inc"\n#include "clear_color_policy.h"')
start=text.index('HRESULT IDirect3DDevice9::Clear(')
end=text.index('HRESULT IDirect3DDevice9::SetViewport(',start)
body=text[start:end]
if 'keepSRGBView' not in body:
    anchor='    const DWORD srgb=d.state.rs[D3DRS_SRGBWRITEENABLE];'
    assert body.count(anchor)==1
    body=body.replace(anchor,anchor+'''
    const bool keepSRGBView=icsm_metal_invariant_clear_reuse.GetBool() && ICSMClearColorIsSRGBInvariant(color);
    if((flags&D3DCLEAR_TARGET) && srgb && keepSRGBView)perfCount(PerfCount::InvariantClearReuses);''')
    assert body.count('if((flags&D3DCLEAR_TARGET) && srgb)')==2
    body=body.replace('if((flags&D3DCLEAR_TARGET) && srgb)','if((flags&D3DCLEAR_TARGET) && srgb && !keepSRGBView)')
    text=text[:start]+body+text[end:]
path.write_text(text)
if '#include "clear_selftest_ios.inc"' not in text:
    text+='\n#include "clear_selftest_ios.inc"\n'
    path.write_text(text)
if '#include "pass_descriptor_ios.inc"' not in text:
    anchor='static bool sourcePassBindingsDirty=false;'
    assert text.count(anchor)==1
    text=text.replace(anchor,anchor+'\n#include "pass_descriptor_ios.inc"')
    anchor='void passEnded() {sourcePass=nil;'
    assert text.count(anchor)==1
    text=text.replace(anchor,'void passEnded() {releaseSourcePassResources(sourcePass);sourcePass=nil;')
    anchor='auto &d=native<Device>(dev); auto p=[MTLRenderPassDescriptor renderPassDescriptor];'
    assert text.count(anchor)==1
    text=text.replace(anchor,'auto &d=native<Device>(dev); auto p=acquireSourcePassDescriptor();')
    path.write_text(text)
if '#include "texture_lock_trace_ios.inc"' not in text:
    text=text.replace('#include "clear_color_policy.h"','#include "clear_color_policy.h"\n#include "texture_lock_trace_ios.inc"')
    for original,replacement in (
        ('if(!lightmapShadowLock(native<Image>(this),mip,0))commit(native<Device>(m_device),true);',
         'if(!lightmapShadowLock(native<Image>(this),mip,0)){traceTextureWait("texture",native<Image>(this),mip,flags);commit(native<Device>(m_device),true);}'),
        ('if(!lightmapShadowLock(native<Image>(this),m_mip,m_face))commit(native<Device>(m_device),true);',
         'if(!lightmapShadowLock(native<Image>(this),m_mip,m_face)){traceTextureWait("surface",native<Image>(this),m_mip,flags);commit(native<Device>(m_device),true);}')):
        assert text.count(original)==1,original
        text=text.replace(original,replacement)
    path.write_text(text)
print('Rectangle clear retains values/rectangles and caches immutable construction')

# Register weak ownership of the exact CPU-authored Panorama alpha atlas.
resources=path.with_name('native_resources.mm')
s=resources.read_text().replace('configureLightmapShadow(*image,usage,label);','configureLightmapShadow(image,usage,label);')
resources.write_text(s)
s=path.read_text()
if '#include "alpha_atlas_selftest_ios.inc"' not in s:
    path.write_text(s+'\n#include "alpha_atlas_selftest_ios.inc"\n')
