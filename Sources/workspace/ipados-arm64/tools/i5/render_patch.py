#!/usr/bin/env python3
"""Observe actual I5 render sizes and enable the existing bounded M6 profiler."""
from pathlib import Path

P=Path(__file__).resolve().parents[2]
C=Path.home()/'Library/Caches/csgo-ipados-arm64/i5'

def once(text,old,new):
    if text.count(old)!=1:
        raise RuntimeError('I5 render anchor changed: '+repr(old[:100]))
    return text.replace(old,new,1)

source=C/'metal/source_device.mm'
s=source.read_text()
s=once(s,'static void afterPresentCapture(Device &d);',
       'static void afterPresentCapture(Device &d);\n#include "resolution_audit.inc"')
s=once(s,'        ++d.draws;',
       '        ++d.draws;\n        if(resolutionAuditRemaining){auto target=dev->m_pRenderTargets[0];recordResolutionDraw(d,target?&native<Image>(target):nullptr,ps);}')
s=once(s,'    if(profileStart)d.profileFrame.present+=(Plat_FloatTime()-profileStart)*1000;',
       '    if(profileStart)d.profileFrame.present+=(Plat_FloatTime()-profileStart)*1000;\n    finishResolutionAudit(d);')
source.write_text(s)

# Reuse the already source-owned M6 profiler around the actual iOS engine frame.
# Its finite completion/release logic was absent from the original UIKit host.
original=C/'metal/native_window.mm'
profiler=original.read_text()
start=profiler.index('    extern SourceMetal::Device *g_sourceNativeDevice;\n',profiler.index('void SourceMetalRunFrame'))
end=profiler.index('\n}\nvoid *SourceMetalAttachWindow',start)
body=profiler[start:end].replace('    @autoreleasepool { frame(frameTime); }','    frame(frameTime);')
body=body.replace('    extern SourceMetal::Device *g_sourceNativeDevice;\n','',1)
window=C/'metal/native_window_ios.mm'
s=window.read_text()
s=once(s,'    frame(frameTime);',body)
window.write_text(s)

diagnostics=C/'metal/diagnostics_ios.mm'
s=diagnostics.read_text()
s=once(s,'extern "C" void SourceMetalWaitForGPU();',
       'extern "C" void SourceMetalWaitForGPU();\nextern "C" NSDictionary *SourceMetalResolutionAuditSnapshot();\nextern "C" NSDictionary *SourceMetalShaderCacheSnapshot();')
s=once(s,'@"width":@(d->width),@"height":@(d->height),',
       '@"resolution_audit":SourceMetalResolutionAuditSnapshot(),@"shader_cache":SourceMetalShaderCacheSnapshot(),@"width":@(d->width),@"height":@(d->height),')
diagnostics.write_text(s)

# iOS URL-backed libraries held file handles across the static library cache.
# Read/close explicitly and use the documented dispatch-data Metal API instead.
shaders=C/'metal/native_shaders.mm'
s=shaders.read_text()
s=once(s,'namespace SourceMetal {','#include "shader_cache_ios.inc"\nnamespace SourceMetal {')
s=once(s,'    else if(diskCached) s->library=[device() newLibraryWithURL:[NSURL fileURLWithPath:libPath] error:&error];\n    else {',
       '''    bool loadedOfflineData=false;
    if(existing==libraries.end() && diskCached) {
        s->library=loadShaderLibraryData(libPath,&error);
        if(s->library){loadedOfflineData=true;++shaderCacheDataLoads;}
        else {
            ++shaderCacheFallbacks;
            Warning("ICSM_SHADER_CACHE_FALLBACK %s: %s\\n",s->label.c_str(),error.localizedDescription.UTF8String ?: "cache read failed");
        }
    }
    if(!s->library) {
        ++shaderCacheSourceCompiles; error=nil;''')
s=once(s,'    libraries[s->key]=s->library;',
       '    libraries[s->key]=s->library;\n    shaderCacheLibraryCount=libraries.size();')
s=once(s,'@"loaded_offline_metallib":@(diskCached),@"reused_library":@(existing!=libraries.end())',
       '@"loaded_offline_metallib":@(diskCached),@"loaded_offline_data":@(loadedOfflineData),@"reused_library":@(existing!=libraries.end())')
shaders.write_text(s)

# Original Source scales only the 3D CViewSetup, upscales it before drawing
# the separate, unscaled HUD CViewSetup. Keep its projection/aspect handling.
view=C/'source/game/client/view.cpp'
s=view.read_text()
s=once(s,'ConVar mat_viewportupscale( "mat_viewportupscale", "1", FCVAR_CHEAT, "Scale the viewport back up" );',
       'ConVar mat_viewportupscale( "mat_viewportupscale", "1", FCVAR_CHEAT, "Scale the viewport back up" );\n'
       'ConVar icsm_scene_render_percent( "icsm_scene_render_percent", "75", FCVAR_ARCHIVE, "iCSM 3D scene resolution percentage; HUD remains unscaled", true, 50.0f, true, 100.0f );')
s=once(s,'\tfloat flViewportScale = mat_viewportscale.GetFloat();',
       '\tfloat flViewportScale = mat_viewportscale.GetFloat() * ( icsm_scene_render_percent.GetFloat() / 100.0f );')
view.write_text(s)

viewrender=C/'source/game/client/viewrender.cpp'
s=viewrender.read_text()
s=once(s,'extern ConVar mat_viewportscale;','extern ConVar mat_viewportscale;\nextern ConVar icsm_scene_render_percent;')
s=once(s,'if ( mat_viewportupscale.GetBool() && mat_viewportscale.GetFloat() < 1.0f )',
       'if ( mat_viewportupscale.GetBool() && mat_viewportscale.GetFloat() * ( icsm_scene_render_percent.GetFloat() / 100.0f ) < 1.0f )')
viewrender.write_text(s)

# The original rifle scope writes only view.width/4 of an allocated quarter-size
# texture, then samples the whole texture with DrawScreenSpaceQuad. A scaled
# scene therefore appears shrunken and displaced outside the stencil lens.
# Fill each allocated blur target using the actual scene-copy rectangle so its
# original 0..1 base/detail UVs and the viewmodel lens mask stay aligned.
ironsight=C/'source/game/shared/cstrike15/weapon_ironsightcontroller.cpp'
s=ironsight.read_text(encoding='latin-1')
s=once(s,'bool CIronSightController::PrepareScopeEffect( int x, int y, int w, int h, CViewSetup *pViewSetup )',
       '#include "scope_audit.inc"\n\nbool CIronSightController::PrepareScopeEffect( int x, int y, int w, int h, CViewSetup *pViewSetup )')
s=once(s,'\tUpdateScreenEffectTexture(0, x, y, w, h, false, &actualRect);',
       '\tUpdateScreenEffectTexture(0, x, y, w, h, false, &actualRect);\n\tif ( icsmScopeAuditRemaining ) icsmScopeCopiedRect = actualRect;')
s=once(s,'''\tint nSrcWidth = pViewSetup->width;
\tint nSrcHeight = pViewSetup->height;

\tpRenderContext->DrawScreenSpaceRectangle(pMatDownsample, 0, 0, nSrcWidth / 4, nSrcHeight / 4,
\t\t0, 0, nSrcWidth - 4, nSrcHeight - 4,''',
       '''\tconst int nBlurWidth0 = pRtQuarterSize0->GetActualWidth();
\tconst int nBlurHeight0 = pRtQuarterSize0->GetActualHeight();

\tpRenderContext->DrawScreenSpaceRectangle(pMatDownsample, 0, 0, nBlurWidth0, nBlurHeight0,
\t\tactualRect.x, actualRect.y, actualRect.x + actualRect.width - 4, actualRect.y + actualRect.height - 4,''')
s=once(s,'''\tpRenderContext->DrawScreenSpaceRectangle(pMatBlurX, 0, 0, nSrcWidth / 4, nSrcHeight / 4,
\t\t0, 0, nSrcWidth / 4 - 1, nSrcHeight / 4 - 1,''',
       '''\tconst int nBlurWidth1 = pRtQuarterSize1->GetActualWidth();
\tconst int nBlurHeight1 = pRtQuarterSize1->GetActualHeight();
\tpRenderContext->DrawScreenSpaceRectangle(pMatBlurX, 0, 0, nBlurWidth1, nBlurHeight1,
\t\t0, 0, nBlurWidth0 - 1, nBlurHeight0 - 1,''')
s=once(s,'''\tpRenderContext->DrawScreenSpaceRectangle(pMatBlurY, 0, 0, nSrcWidth / 4, nSrcHeight / 4,
\t\t0, 0, nSrcWidth / 4 - 1, nSrcHeight / 4 - 1,
\t\tpRtQuarterSize0->GetActualWidth(), pRtQuarterSize0->GetActualHeight());''',
       '''\tpRenderContext->DrawScreenSpaceRectangle(pMatBlurY, 0, 0, nBlurWidth0, nBlurHeight0,
\t\t0, 0, nBlurWidth1 - 1, nBlurHeight1 - 1,
\t\tpRtQuarterSize1->GetActualWidth(), pRtQuarterSize1->GetActualHeight());''')
s=once(s,'\tdotCoords.y += 0.5f;',
       '''\tdotCoords.y += 0.5f;
\tif ( icsmScopeAuditRemaining )
\t{
\t\t--icsmScopeAuditRemaining;
\t\tint vx, vy, vw, vh;
\t\tpRenderContext->GetViewport( vx, vy, vw, vh );
\t\tITexture *blur = materials->FindTexture("_rt_SmallFB0", TEXTURE_GROUP_RENDER_TARGET);
\t\tMsg("ICSM_SCOPE_AUDIT {\\"weapon\\":%d,\\"view\\":[%d,%d,%d,%d],\\"viewport\\":[%d,%d,%d,%d],\\"scene_copy\\":[%d,%d,%d,%d],\\"blur_texture\\":[%d,%d],\\"dot_center\\":[%.4f,%.4f],\\"dot_width\\":%d}\\n",
\t\t\tm_pAttachedWeapon->GetItemDefinitionIndex(), x, y, w, h, vx, vy, vw, vh,
\t\t\ticsmScopeCopiedRect.x, icsmScopeCopiedRect.y, icsmScopeCopiedRect.width, icsmScopeCopiedRect.height,
\t\t\tblur->GetActualWidth(), blur->GetActualHeight(), w * dotCoords.x, h * dotCoords.y, iWidth);
\t}''')
ironsight.write_text(s,encoding='latin-1')

from shader_sampler_patch import apply as apply_shader_samplers
apply_shader_samplers()
