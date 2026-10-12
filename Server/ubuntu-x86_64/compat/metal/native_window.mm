#include "native_objects.h"
#include "metal_window.h"
#include "metal_frame.h"
#include "tier1/convar.h"
#include <cmath>
#import <AppKit/AppKit.h>
#undef MIN
#undef MAX
namespace SourceMetal {
static CAMetalLayer *metalLayer=nil;
static SDL_Window *metalWindow=nullptr;
static SDL_MetalView metalView=nullptr;
CAMetalLayer *layer() { return metalLayer; }
}
void SourceMetalRunFrame(void (*frame)(float), float frameTime) {
    extern SourceMetal::Device *g_sourceNativeDevice;
    auto d=g_sourceNativeDevice;
    const bool profiling=d && d->profileRemaining;
    const double start=profiling ? Plat_FloatTime():0;
    if(profiling) {
        d->profileFrame=SourceMetal::FrameTiming{};
        const auto previous=d->lastSceneSubmitted ? d->lastSceneSubmitted:d->lastSubmitted;
        if(previous.status==MTLCommandBufferStatusCompleted)
            d->profileFrame.gpu=(previous.GPUEndTime-previous.GPUStartTime)*1000;
    }
    @autoreleasepool { frame(frameTime); }
    if(profiling && d==g_sourceNativeDevice && d->profileRemaining) {
        for(auto it=d->profileSceneCommands.begin();it!=d->profileSceneCommands.end();) {
            if((*it).status==MTLCommandBufferStatusCompleted) {
                d->profileSceneGPU.push_back(((*it).GPUEndTime-(*it).GPUStartTime)*1000);
                it=d->profileSceneCommands.erase(it);
            } else ++it;
        }
        d->profileFrame.host=(Plat_FloatTime()-start)*1000;
        d->profileSamples.push_back(d->profileFrame);
        if(!--d->profileRemaining) {
            auto values=d->profileSamples;
            std::sort(values.begin(),values.end(),[](const auto &a,const auto &b){return a.host<b.host;});
            const auto n=values.size();
            Msg("M6_PROFILE count=%zu host_p50=%.3f host_p95=%.3f host_p99=%.3f ms\n",n,values[n/2].host,values[(n-1)*95/100].host,values[(n-1)*99/100].host);
            auto gpu=d->profileSceneGPU; std::sort(gpu.begin(),gpu.end());
            if(!gpu.empty()) {const auto n=gpu.size(); Msg("M6_PROFILE_GPU completed_scenes=%zu pending=%zu gpu_p50=%.3f gpu_p95=%.3f gpu_p99=%.3f max=%.3f ms\n",n,d->profileSceneCommands.size(),gpu[n/2],gpu[(n-1)*95/100],gpu[(n-1)*99/100],gpu.back());}
            d->profileSceneCommands.clear();
            for(unsigned i=0;i<10 && i<n;++i) {const auto &s=values[n-1-i]; Msg("M6_PROFILE_TAIL host=%.3f prepare=%.3f present=%.3f acquire=%.3f previous_completed_gpu=%.3f ms (0=unavailable)\n",s.host,s.prepare,s.present,s.acquire,s.gpu);}
        }
    }
}
void *SourceMetalAttachWindow(SDL_Window *window) {
    SDL_MetalView view=SDL_Metal_CreateView(window);
    if (!view) Error("SDL Metal view failed: %s\n",SDL_GetError());
    SourceMetal::metalWindow=window;
    SourceMetal::metalView=view;
    SourceMetal::metalLayer=(__bridge CAMetalLayer*)SDL_Metal_GetLayer(view);
    auto l=SourceMetal::metalLayer;
    l.device=SourceMetal::device(); l.pixelFormat=MTLPixelFormatBGRA8Unorm;
    l.framebufferOnly=YES; l.maximumDrawableCount=3;
    int w,h; SDL_Metal_GetDrawableSize(window,&w,&h); l.drawableSize=CGSizeMake(w,h);
    Msg("Native Metal renderer: %s; drawable %dx%d\n",l.device.name.UTF8String,w,h);
    return view;
}
void SourceMetalDetachWindow(void *view) {
    SourceMetal::metalLayer=nil;
    SourceMetal::metalWindow=nullptr;
    SourceMetal::metalView=nullptr;
    if(view) SDL_Metal_DestroyView(view);
}
void SourceMetalSizeWindow(SDL_Window *window, unsigned width, unsigned height) {
    if(!window || !width || !height || window!=SourceMetal::metalWindow || !SourceMetal::metalView)return;
    if(SDL_GetWindowFlags(window)&SDL_WINDOW_FULLSCREEN) {
        SourceMetalResizeWindow(width,height);
        return;
    }
    NSView *view=(__bridge NSView*)SourceMetal::metalView;
    NSWindow *nswindow=view.window;
    NSScreen *screen=nswindow.screen ?: NSScreen.mainScreen;
    if(!nswindow || !screen) {
        Warning("Metal window has no screen for backing-size conversion\n");
        return;
    }
    // Source video modes use pixels; SDL/Cocoa window sizes use points. Ask
    // the actual view for its backing conversion instead of assuming Retina 2x.
    NSSize points=[view convertSizeFromBacking:NSMakeSize(width,height)];
    const NSRect usableContent=[nswindow contentRectForFrameRect:screen.visibleFrame];
    if(points.width<=0 || points.height<=0 || usableContent.size.width<=0 || usableContent.size.height<=0)return;
    const bool fullHD=width>=1920 && height>=1080;
    const double fit=std::min(1.0,std::min(usableContent.size.width/points.width,usableContent.size.height/points.height));
    const int pointWidth=std::max(1,(int)floor(points.width*fit));
    const int pointHeight=std::max(1,(int)floor(points.height*fit));
    // Keep the minimum in the selected render mode's aspect ratio as well.
    // On a smaller screen, uniformly fit it to the desktop; independently
    // raising one axis here would stretch the presentation window.
    const double minimumScale=fullHD ? std::max(1920.0/width,1080.0/height):std::max(1.0/width,1.0/height);
    NSSize minimum=[view convertSizeFromBacking:NSMakeSize(width*minimumScale,height*minimumScale)];
    const double minimumFit=std::min(1.0,std::min(usableContent.size.width/minimum.width,usableContent.size.height/minimum.height));
    // Cap integer-rounding of the minimum to the uniformly fitted target;
    // SDL must not increase either target axis because its minimum rounded up.
    const int minWidth=std::min(pointWidth,std::max(1,(int)ceil(minimum.width*minimumFit)));
    const int minHeight=std::min(pointHeight,std::max(1,(int)ceil(minimum.height*minimumFit)));
    SDL_SetWindowMinimumSize(window,minWidth,minHeight);
    SDL_SetWindowSize(window,pointWidth,pointHeight);
    // Center the decorated frame within the current screen's usable desktop,
    // retaining this window and avoiding macOS fullscreen Spaces.
    const NSRect frame=nswindow.frame;
    const NSRect visible=screen.visibleFrame;
    [nswindow setFrameOrigin:NSMakePoint(visible.origin.x+(visible.size.width-frame.size.width)/2,
                                       visible.origin.y+(visible.size.height-frame.size.height)/2)];
    SourceMetalResizeWindow(width,height);
}
void SourceMetalResizeWindow(unsigned w,unsigned h) {
    (void)w; (void)h;
    if(!SourceMetal::metalLayer || !SourceMetal::metalWindow)return;
    // SDL's Metal view updates itself on SIZE_CHANGED. Query the native window
    // backing size directly here as well: Metal_GetDrawableSize returns the
    // layer's current value and cannot repair a stale or manually changed size.
    int pixelWidth=0,pixelHeight=0,pointWidth=0,pointHeight=0;
    SDL_GetWindowSizeInPixels(SourceMetal::metalWindow,&pixelWidth,&pixelHeight);
    SDL_GetWindowSize(SourceMetal::metalWindow,&pointWidth,&pointHeight);
    if(pixelWidth<=0 || pixelHeight<=0 || pointWidth<=0 || pointHeight<=0)return;
    auto l=SourceMetal::metalLayer;
    const CGSize actual=CGSizeMake(pixelWidth,pixelHeight);
    if(!CGSizeEqualToSize(l.drawableSize,actual))l.drawableSize=actual;
    l.contentsScale=(double)pixelHeight/pointHeight;
}
void SourceMetalPrintWindowStats() {
    if(!SourceMetal::metalWindow || !SourceMetal::metalLayer) {
        Msg("Metal window unavailable\n");
        return;
    }
    int pointWidth=0,pointHeight=0,backingWidth=0,backingHeight=0;
    SDL_GetWindowSize(SourceMetal::metalWindow,&pointWidth,&pointHeight);
    SDL_GetWindowSizeInPixels(SourceMetal::metalWindow,&backingWidth,&backingHeight);
    const Uint32 flags=SDL_GetWindowFlags(SourceMetal::metalWindow);
    const CGSize drawable=SourceMetal::metalLayer.drawableSize;
    extern SourceMetal::Device *g_sourceNativeDevice;
    const auto d=g_sourceNativeDevice;
    Msg("Metal window points=%dx%d backing=%dx%d drawable=%.0fx%.0f render=%ux%u fullscreen=%d flags=0x%08x contents_scale=%.3f\n",
        pointWidth,pointHeight,backingWidth,backingHeight,drawable.width,drawable.height,
        d ? d->width:0,d ? d->height:0,(flags&SDL_WINDOW_FULLSCREEN)!=0,flags,SourceMetal::metalLayer.contentsScale);
}
CON_COMMAND(m6_window_stats,"Print native Metal window points, backing pixels, drawable and render sizes") { SourceMetalPrintWindowStats(); }
void SourceMetalFillRendererInfo(GLMRendererInfoFields *info) {
    memset(info,0,sizeof(*info));
    auto d=SourceMetal::device();
    info->m_fullscreen=info->m_accelerated=info->m_windowed=1;
    NSOperatingSystemVersion os=NSProcessInfo.processInfo.operatingSystemVersion;
    info->m_osComboVersion=(os.majorVersion<<16)|(os.minorVersion<<8)|os.patchVersion;
    info->m_pciVendorID=0x106b; // Apple vendor; Apple Silicon exposes no PCI device ID.
    V_strncpy(info->m_pciModelString,d.name.UTF8String,sizeof(info->m_pciModelString));
    V_strncpy(info->m_driverInfoString,"Native Metal",sizeof(info->m_driverInfoString));
    info->m_vidMemory=info->m_texMemory=std::min<uint64_t>(d.recommendedMaxWorkingSetSize,INT_MAX);
    for (unsigned n=1;n<=8;n*=2) if([d supportsTextureSampleCount:n]) info->m_maxSamples=n;
    info->m_maxAniso=16;
    info->m_hasGammaWrites=true; info->m_hasMixedAttachmentSizes=true;
    info->m_hasBGRA=true; info->m_hasNewFullscreenMode=true;
    info->m_hasOcclusionQuery=true; info->m_hasFramebufferBlit=true;
    info->m_hasUniformBuffers=true;
    info->m_nMaxVertexTIU=4; info->m_nNumVertexSamplers=4;
    info->m_nMaxCombinedTIU=20; info->m_nFirstVertexSampler=16; info->m_nTotalSamplerCount=20;
}
