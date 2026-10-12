// The engine's display database populated from CoreGraphics and Metal.
#include "togl/rendermechanism.h"
#include "metal_window.h"
GLMDisplayInfo::GLMDisplayInfo(CGDirectDisplayID id,CGOpenGLDisplayMask mask) : m_modes(nullptr),m_display(id) {
    memset(&m_info,0,sizeof(m_info)); m_info.m_cgDisplayID=id;
    CGDisplayModeRef mode=CGDisplayCopyDisplayMode(id);
    // The Source mode table uses render pixels, whereas CGDisplayPixelsWide
    // can report the scaled desktop coordinate size on Retina displays.
    m_info.m_displayPixelWidth=mode ? CGDisplayModeGetPixelWidth(mode):CGDisplayPixelsWide(id);
    m_info.m_displayPixelHeight=mode ? CGDisplayModeGetPixelHeight(mode):CGDisplayPixelsHigh(id);
    m_DesktopMode.Init(m_info.m_displayPixelWidth,m_info.m_displayPixelHeight,mode ? CGDisplayModeGetRefreshRate(mode):0);
    if(mode) CGDisplayModeRelease(mode);
}
GLMDisplayInfo::~GLMDisplayInfo() { if(m_modes) { m_modes->PurgeAndDeleteElements(); delete m_modes; } }
void GLMDisplayInfo::PopulateModes() {
    if(m_modes) return;
    m_modes=new CUtlVector<GLMDisplayMode*>;
    CFArrayRef modes=CGDisplayCopyAllDisplayModes(m_display,nullptr);
    if(modes) {
        for(CFIndex i=0;i<CFArrayGetCount(modes);++i) {
            auto mode=(CGDisplayModeRef)CFArrayGetValueAtIndex(modes,i);
            unsigned w=CGDisplayModeGetPixelWidth(mode),h=CGDisplayModeGetPixelHeight(mode),hz=CGDisplayModeGetRefreshRate(mode);
            bool duplicate=false;
            for(int n=0;n<m_modes->Count();++n) { const auto &v=(*m_modes)[n]->m_info; if(v.m_modePixelWidth==w && v.m_modePixelHeight==h && v.m_modeRefreshHz==hz) duplicate=true; }
            if(!duplicate) m_modes->AddToTail(new GLMDisplayMode(w,h,hz));
        }
        CFRelease(modes);
    }
    if(!m_modes->Count()) m_modes->AddToTail(new GLMDisplayMode(m_DesktopMode.m_info.m_modePixelWidth,m_DesktopMode.m_info.m_modePixelHeight,m_DesktopMode.m_info.m_modeRefreshHz));
    // Metal renders offscreen and presents to desktop fullscreen without
    // changing the physical display mode. Expose the required 1080p render
    // size even when CoreGraphics doesn't list a 16:9 panel mode.
    bool has1080p=false;
    for(int i=0;i<m_modes->Count();++i) {
        const auto &v=(*m_modes)[i]->m_info;
        if(v.m_modePixelWidth==1920 && v.m_modePixelHeight==1080)has1080p=true;
    }
    if(!has1080p)m_modes->AddToTail(new GLMDisplayMode(1920,1080,m_DesktopMode.m_info.m_modeRefreshHz));
}
void GLMDisplayInfo::Dump(int which) { Msg("Metal display %d: %ux%u\n",which,m_info.m_displayPixelWidth,m_info.m_displayPixelHeight); }
GLMRendererInfo::GLMRendererInfo(GLMRendererInfoFields *info) : m_info(*info),m_displays(nullptr) {}
GLMRendererInfo::~GLMRendererInfo() { if(m_displays) { m_displays->PurgeAndDeleteElements(); delete m_displays; } }
void GLMRendererInfo::PopulateDisplays() {
    if(m_displays) return;
    m_displays=new CUtlVector<GLMDisplayInfo*>;
    CGDirectDisplayID ids[32]; uint32_t count=0;
    if(CGGetActiveDisplayList(32,ids,&count)!=kCGErrorSuccess) Error("CoreGraphics display enumeration failed\n");
    for(unsigned i=0;i<count;++i) { auto d=new GLMDisplayInfo(ids[i],0); d->PopulateModes(); m_displays->AddToTail(d); }
}
void GLMRendererInfo::Dump(int which) { Msg("Metal device %d: %s\n",which,m_info.m_pciModelString); }
GLMDisplayDB::GLMDisplayDB() : m_renderers(nullptr) {}
GLMDisplayDB::~GLMDisplayDB() { if(m_renderers) { m_renderers->PurgeAndDeleteElements(); delete m_renderers; } }
void GLMDisplayDB::PopulateRenderers() {
    if(m_renderers) return;
    m_renderers=new CUtlVector<GLMRendererInfo*>;
    GLMRendererInfoFields info; SourceMetalFillRendererInfo(&info);
    auto r=new GLMRendererInfo(&info); r->PopulateDisplays(); m_renderers->AddToTail(r);
}
void GLMDisplayDB::PopulateFakeAdapters(uint r) {
    if(r>=static_cast<uint>(m_renderers->Count())) return;
    for(int i=0;i<(*m_renderers)[r]->m_displays->Count();++i) { GLMFakeAdapter a={static_cast<int>(r),i}; m_fakeAdapters.AddToTail(a); }
}
void GLMDisplayDB::Populate() { PopulateRenderers(); if(!m_fakeAdapters.Count()) for(int r=0;r<m_renderers->Count();++r) PopulateFakeAdapters(r); }
int GLMDisplayDB::GetFakeAdapterCount() { return m_fakeAdapters.Count(); }
bool GLMDisplayDB::GetFakeAdapterInfo(int n,int *ro,int *dout,GLMRendererInfoFields *ri,GLMDisplayInfoFields *di) {
    if(n<0 || n>=m_fakeAdapters.Count()) return true;
    auto a=m_fakeAdapters[n]; if(ro)*ro=a.m_rendererIndex; if(dout)*dout=a.m_displayIndex;
    return (ri && GetRendererInfo(a.m_rendererIndex,ri)) || (di && GetDisplayInfo(a.m_rendererIndex,a.m_displayIndex,di));
}
int GLMDisplayDB::GetRendererCount() { return m_renderers ? m_renderers->Count():0; }
bool GLMDisplayDB::GetRendererInfo(int r,GLMRendererInfoFields *out) { if(r<0 || r>=GetRendererCount())return true; *out=(*m_renderers)[r]->m_info; return false; }
int GLMDisplayDB::GetDisplayCount(int r) { return r<0 || r>=GetRendererCount() ? 0:(*m_renderers)[r]->m_displays->Count(); }
bool GLMDisplayDB::GetDisplayInfo(int r,int d,GLMDisplayInfoFields *out) { if(d<0 || d>=GetDisplayCount(r))return true; *out=(*(*m_renderers)[r]->m_displays)[d]->m_info; return false; }
int GLMDisplayDB::GetModeCount(int r,int d) { return d<0 || d>=GetDisplayCount(r) ? 0:(*(*m_renderers)[r]->m_displays)[d]->m_modes->Count(); }
bool GLMDisplayDB::GetModeInfo(int r,int d,int n,GLMDisplayModeInfoFields *out) {
    if(d<0 || d>=GetDisplayCount(r))return true;
    auto display=(*(*m_renderers)[r]->m_displays)[d];
    if(n==-1) {
        CGDisplayModeRef mode=CGDisplayCopyDisplayMode(display->m_info.m_cgDisplayID);
        if(mode) {
            display->m_DesktopMode.Init(CGDisplayModeGetPixelWidth(mode),CGDisplayModeGetPixelHeight(mode),CGDisplayModeGetRefreshRate(mode));
            CFRelease(mode);
        }
        *out=display->m_DesktopMode.m_info; return false;
    }
    if(n<0 || n>=GetModeCount(r,d))return true;
    *out=(*display->m_modes)[n]->m_info; return false;
}
void GLMDisplayDB::Dump() { for(int r=0;r<GetRendererCount();++r) (*m_renderers)[r]->Dump(r); }
