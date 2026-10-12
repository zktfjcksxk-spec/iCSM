/* I5 touch integration */
#include "touch_ios.h"
#include "native_objects.h"
#import <MetalFX/MetalFX.h>
#import <simd/simd.h>
#define BOOL SourceD3DBool
#include "appframework/ilaunchermgr.h"
#include "tier1/tier1.h"
#include "tier2/tier2.h"
#include "mathlib/mathlib.h"
#include <sstream>
#include <limits>
using namespace SourceMetal;
ILauncherMgr *g_pLauncherMgr=nullptr;
static GLMDisplayDB *GetDisplayDB() { return g_pLauncherMgr->GetDisplayDB(); }
bool g_bNullD3DDevice=false;
extern Device *g_sourceNativeDevice;
static void afterPresentCapture(Device &d);
#include "resolution_audit.inc"
#include "metal_perf_ios.inc"
#include "clear_color_policy.h"
#include "texture_lock_trace_ios.inc"
#include "world_sampler_quality_ios.inc"
#include "metal_combat_ios.h"
#include "pipeline_warm_ios.h"
static IDirect3DDevice9 *g_sourceCombatD3DDevice=nullptr;
#include "metal_spatial_ios.inc"
#include "frame_interpolation_motion_source.inc"
#include "frame_interpolation_ios.inc"
namespace SourceMetal {
static MTLRenderPassDescriptor *sourcePass=nil;
static Device *sourcePassOwner=nullptr;
static bool sourcePassBindingsDirty=false;
#include "pass_descriptor_ios.inc"
void passEnded() {releaseSourcePassResources(sourcePass);sourcePass=nil;sourcePassOwner=nullptr;sourcePassBindingsDirty=false;}
}

COpenGLEntryPoints *ToGLConnectLibraries(CreateInterfaceFn factory) {
    ConnectTier1Libraries(&factory,1); ConVar_Register(); ConnectTier2Libraries(&factory,1);
    MathLib_Init(2.2f,2.2f,0.0f,2.0f);
    g_pLauncherMgr=static_cast<ILauncherMgr*>(factory(SDLMGR_INTERFACE_VERSION,nullptr));
    return nullptr;
}
void ToGLDisconnectLibraries() { g_pLauncherMgr=nullptr; DisconnectTier2Libraries(); ConVar_Unregister(); DisconnectTier1Libraries(); }
COpenGLEntryPoints *GetOpenGLEntryPoints(GL_GetProcAddressCallbackFunc_t) { return nullptr; }
void ClearOpenGLEntryPoints() {}
IDirect3D9 *Direct3DCreate9(UINT) { return new IDirect3D9; }
void D3DPERF_SetOptions(DWORD) {}
void toglGetClientRect(VD3DHWND,RECT *rect) { uint w=0,h=0; g_pLauncherMgr->RenderedSize(w,h,false); *rect={0,0,(LONG)w,(LONG)h}; }
static HRESULT unsupported(const char *operation) { Error("Metal: unsupported Source call %s\n",operation); return D3DERR_INVALIDCALL; }
DWORD IDirect3DResource9::SetPriority(DWORD) { return 0; } // Metal has no managed resource eviction priority.
static D3DSURFACE_DESC surfaceDesc(unsigned w,unsigned h,DWORD usage,D3DFORMAT format,D3DPOOL pool,unsigned samples=1) {
    D3DSURFACE_DESC d={}; d.Format=format; d.Type=D3DRTYPE_SURFACE; d.Usage=usage; d.Pool=pool;
    d.MultiSampleType=samples==1 ? D3DMULTISAMPLE_NONE:static_cast<D3DMULTISAMPLE_TYPE>(samples);
    d.Width=w; d.Height=h; return d;
}
static IDirect3DSurface9 *surface(IDirect3DDevice9 *dev,std::shared_ptr<Texture> texture,D3DSURFACE_DESC desc,unsigned face=0,unsigned mip=0) {
    auto s=new IDirect3DSurface9; s->m_device=dev; s->m_restype=D3DRTYPE_SURFACE; s->m_tex=nullptr;
    s->m_desc=desc; s->m_face=face; s->m_mip=mip;
    auto ref=new Image; ref->image=std::move(texture); ref->face=face; ref->mip=mip; s->m_native=ref;
    return s;
}
IDirect3DBaseTexture9::~IDirect3DBaseTexture9() { m_device->ReleasedTexture(this); delete static_cast<Object*>(m_native); }
D3DRESOURCETYPE IDirect3DBaseTexture9::GetType() { return m_restype; }
DWORD IDirect3DBaseTexture9::GetLevelCount() { return native<Image>(this).image->levels; }
HRESULT IDirect3DBaseTexture9::GetLevelDesc(UINT level,D3DSURFACE_DESC *out) {
    if(level>=GetLevelCount())return D3DERR_INVALIDCALL;
    *out=m_descZero; out->Width=std::max(1u,out->Width>>level); out->Height=std::max(1u,out->Height>>level); return S_OK;
}
HRESULT IDirect3DDevice9::CreateTexture(UINT w,UINT h,UINT levels,DWORD usage,D3DFORMAT format,D3DPOOL pool,IDirect3DTexture9 **out,VD3DHANDLE*,char *label) {
    *out=nullptr; auto image=newTexture(w,h,1,levels,1,1,usage,format,label); if(!image)return D3DERR_NOTAVAILABLE;
    auto t=new IDirect3DTexture9; t->m_device=this; t->m_restype=D3DRTYPE_TEXTURE; t->m_tex=nullptr; t->m_surfZero=nullptr;
    t->m_descZero=surfaceDesc(w,h,usage,format,pool); t->m_descZero.Type=D3DRTYPE_TEXTURE;
    auto ref=new Image; ref->image=image; t->m_native=ref; *out=t; return S_OK;
}
IDirect3DTexture9::~IDirect3DTexture9() {}
HRESULT IDirect3DTexture9::LockRect(UINT mip,D3DLOCKED_RECT *out,const RECT *rect,DWORD flags) {
    if(!lightmapShadowLock(native<Image>(this),mip,0)){traceTextureWait("texture",native<Image>(this),mip,flags);commit(native<Device>(m_device),true);}
    D3DBOX box; if(rect)box={static_cast<UINT>(rect->left),static_cast<UINT>(rect->top),static_cast<UINT>(rect->right),static_cast<UINT>(rect->bottom),0,1};
    D3DLOCKED_BOX lock; auto hr=lockImage(native<Image>(this),mip,0,rect ? &box:nullptr,flags,lock);
    if(hr==S_OK) { out->pBits=lock.pBits; out->Pitch=lock.RowPitch; } return hr;
}
HRESULT IDirect3DTexture9::UnlockRect(UINT mip) { return unlockImage(native<Image>(this),mip,0); }
HRESULT IDirect3DTexture9::GetSurfaceLevel(UINT mip,IDirect3DSurface9 **out) {
    D3DSURFACE_DESC desc; auto hr=GetLevelDesc(mip,&desc); if(hr!=S_OK)return hr;
    *out=surface(m_device,native<Image>(this).image,desc,0,mip); return S_OK;
}
HRESULT IDirect3DDevice9::CreateCubeTexture(UINT edge,UINT levels,DWORD usage,D3DFORMAT format,D3DPOOL pool,IDirect3DCubeTexture9 **out,VD3DHANDLE*,char *label) {
    *out=nullptr; auto image=newTexture(edge,edge,1,levels,6,1,usage,format,label); if(!image)return D3DERR_NOTAVAILABLE;
    auto t=new IDirect3DCubeTexture9; t->m_device=this; t->m_restype=D3DRTYPE_CUBETEXTURE; t->m_tex=nullptr;
    memset(t->m_surfZero,0,sizeof(t->m_surfZero)); t->m_descZero=surfaceDesc(edge,edge,usage,format,pool);
    t->m_descZero.Type=D3DRTYPE_CUBETEXTURE; auto ref=new Image; ref->image=image; t->m_native=ref; *out=t; return S_OK;
}
IDirect3DCubeTexture9::~IDirect3DCubeTexture9() {}
HRESULT IDirect3DCubeTexture9::GetCubeMapSurface(D3DCUBEMAP_FACES face,UINT mip,IDirect3DSurface9 **out) {
    D3DSURFACE_DESC desc; auto hr=GetLevelDesc(mip,&desc); if(hr!=S_OK || face>=6)return D3DERR_INVALIDCALL;
    *out=surface(m_device,native<Image>(this).image,desc,face,mip); return S_OK;
}
HRESULT IDirect3DCubeTexture9::GetLevelDesc(UINT mip,D3DSURFACE_DESC *out) { return IDirect3DBaseTexture9::GetLevelDesc(mip,out); }
HRESULT IDirect3DDevice9::CreateVolumeTexture(UINT w,UINT h,UINT depth,UINT levels,DWORD usage,D3DFORMAT format,D3DPOOL pool,IDirect3DVolumeTexture9 **out,VD3DHANDLE*,char *label) {
    *out=nullptr; auto image=newTexture(w,h,depth,levels,1,1,usage,format,label); if(!image)return D3DERR_NOTAVAILABLE;
    auto t=new IDirect3DVolumeTexture9; t->m_device=this; t->m_restype=D3DRTYPE_VOLUMETEXTURE; t->m_tex=nullptr; t->m_surfZero=nullptr;
    t->m_descZero=surfaceDesc(w,h,usage,format,pool); t->m_descZero.Type=D3DRTYPE_VOLUMETEXTURE;
    t->m_volDescZero={}; t->m_volDescZero.Format=format; t->m_volDescZero.Type=D3DRTYPE_VOLUMETEXTURE;
    t->m_volDescZero.Usage=usage; t->m_volDescZero.Pool=pool; t->m_volDescZero.Width=w; t->m_volDescZero.Height=h; t->m_volDescZero.Depth=depth;
    auto ref=new Image; ref->image=image; t->m_native=ref; *out=t; return S_OK;
}
IDirect3DVolumeTexture9::~IDirect3DVolumeTexture9() {}
HRESULT IDirect3DVolumeTexture9::LockBox(UINT mip,D3DLOCKED_BOX *out,const D3DBOX *box,DWORD flags) { commit(native<Device>(m_device),true); return lockImage(native<Image>(this),mip,0,box,flags,*out); }
HRESULT IDirect3DVolumeTexture9::UnlockBox(UINT mip) { return unlockImage(native<Image>(this),mip,0); }
HRESULT IDirect3DVolumeTexture9::GetLevelDesc(UINT mip,D3DVOLUME_DESC *out) { if(mip>=GetLevelCount())return D3DERR_INVALIDCALL; *out=m_volDescZero; out->Width=std::max(1u,out->Width>>mip); out->Height=std::max(1u,out->Height>>mip); out->Depth=std::max(1u,out->Depth>>mip); return S_OK; }
IDirect3DSurface9::~IDirect3DSurface9() { m_device->ReleasedSurface(this); delete static_cast<Object*>(m_native); }
HRESULT IDirect3DSurface9::GetDesc(D3DSURFACE_DESC *out) { *out=m_desc; return S_OK; }
HRESULT IDirect3DSurface9::LockRect(D3DLOCKED_RECT *out,const RECT *rect,DWORD flags) {
    if(!lightmapShadowLock(native<Image>(this),m_mip,m_face)){traceTextureWait("surface",native<Image>(this),m_mip,flags);commit(native<Device>(m_device),true);}
    D3DBOX box; if(rect)box={static_cast<UINT>(rect->left),static_cast<UINT>(rect->top),static_cast<UINT>(rect->right),static_cast<UINT>(rect->bottom),0,1};
    D3DLOCKED_BOX lock; auto hr=lockImage(native<Image>(this),m_mip,m_face,rect ? &box:nullptr,flags,lock);
    if(hr==S_OK) { out->pBits=lock.pBits; out->Pitch=lock.RowPitch; } return hr;
}
HRESULT IDirect3DSurface9::UnlockRect() { return unlockImage(native<Image>(this),m_mip,m_face); }
HRESULT IDirect3DDevice9::CreateRenderTarget(UINT w,UINT h,D3DFORMAT format,D3DMULTISAMPLE_TYPE sample,DWORD,BOOL lockable,IDirect3DSurface9 **out,VD3DHANDLE*,char *label) {
    unsigned samples=sample==D3DMULTISAMPLE_NONE ? 1:sample;
    *out=nullptr; if(![device() supportsTextureSampleCount:samples])return D3DERR_NOTAVAILABLE;
    auto tex=newTexture(w,h,1,1,1,samples,D3DUSAGE_RENDERTARGET,format,label,lockable); if(!tex)return D3DERR_NOTAVAILABLE;
    *out=surface(this,tex,surfaceDesc(w,h,D3DUSAGE_RENDERTARGET,format,D3DPOOL_DEFAULT,samples)); return S_OK;
}
HRESULT IDirect3DDevice9::CreateDepthStencilSurface(UINT w,UINT h,D3DFORMAT format,D3DMULTISAMPLE_TYPE sample,DWORD,BOOL,IDirect3DSurface9 **out,VD3DHANDLE*) {
    unsigned samples=sample==D3DMULTISAMPLE_NONE ? 1:sample;
    *out=nullptr; if(![device() supportsTextureSampleCount:samples])return D3DERR_NOTAVAILABLE;
    auto tex=newTexture(w,h,1,1,1,samples,D3DUSAGE_DEPTHSTENCIL,format,"Source depth stencil"); if(!tex)return D3DERR_NOTAVAILABLE;
    *out=surface(this,tex,surfaceDesc(w,h,D3DUSAGE_DEPTHSTENCIL,format,D3DPOOL_DEFAULT,samples)); return S_OK;
}
HRESULT IDirect3DDevice9::CreateOffscreenPlainSurface(UINT w,UINT h,D3DFORMAT format,D3DPOOL pool,IDirect3DSurface9 **out,VD3DHANDLE*) {
    *out=nullptr; auto tex=newTexture(w,h,1,1,1,1,0,format,"Source readback surface"); if(!tex)return D3DERR_NOTAVAILABLE;
    *out=surface(this,tex,surfaceDesc(w,h,0,format,pool)); return S_OK;
}
HRESULT IDirect3DDevice9::SetRenderTarget(DWORD slot,IDirect3DSurface9 *surface) {
    if(slot>=4)return D3DERR_INVALIDCALL;
    auto old=m_pRenderTargets[slot]; if(old==surface)return S_OK;
    if(surface)invalidateLightmapShadow(*native<Image>(surface).image,true);
    perfCount(PerfCount::TargetChanges); if(!icsm_metal_pass_reuse.GetBool())endEncoder(native<Device>(this)); else sourcePassBindingsDirty=true; if(surface)surface->AddRef(1);
    m_pRenderTargets[slot]=surface; if(old)old->Release(1); return S_OK;
}
HRESULT IDirect3DDevice9::GetRenderTarget(DWORD slot,IDirect3DSurface9 **out) { if(slot>=4)return D3DERR_INVALIDCALL; *out=m_pRenderTargets[slot]; if(*out)(*out)->AddRef(); return S_OK; }
HRESULT IDirect3DDevice9::SetDepthStencilSurface(IDirect3DSurface9 *surface) {
    auto old=m_pDepthStencil; if(old==surface)return S_OK; perfCount(PerfCount::DepthChanges);
    if(!icsm_metal_pass_reuse.GetBool())endEncoder(native<Device>(this)); else sourcePassBindingsDirty=true; if(surface)surface->AddRef(1);
    m_pDepthStencil=surface; if(old)old->Release(1); return S_OK;
}
HRESULT IDirect3DDevice9::GetDepthStencilSurface(IDirect3DSurface9 **out) { *out=m_pDepthStencil; if(*out)(*out)->AddRef(); return S_OK; }
static Buffer *newBuffer(unsigned size) { @autoreleasepool { auto b=new Buffer; b->length=size; b->buffer=[device() newBufferWithLength:std::max(size,1u) options:MTLResourceStorageModeShared]; if(!b->buffer)fatal("buffer allocation",@"out of memory"); return b; } }
HRESULT IDirect3DDevice9::CreateVertexBuffer(UINT n,DWORD usage,DWORD fvf,D3DPOOL pool,IDirect3DVertexBuffer9 **out,VD3DHANDLE*) {
    auto b=new IDirect3DVertexBuffer9; b->m_device=this; b->m_restype=D3DRTYPE_VERTEXBUFFER; b->m_ctx=nullptr; b->m_vtxBuffer=nullptr;
    b->m_vtxDesc={}; b->m_vtxDesc.Format=D3DFMT_VERTEXDATA; b->m_vtxDesc.Type=D3DRTYPE_VERTEXBUFFER; b->m_vtxDesc.Usage=usage; b->m_vtxDesc.Pool=pool; b->m_vtxDesc.Size=n; b->m_vtxDesc.FVF=fvf;
    b->m_native=newBuffer(n); *out=b; return S_OK;
}
HRESULT IDirect3DDevice9::CreateIndexBuffer(UINT n,DWORD usage,D3DFORMAT format,D3DPOOL pool,IDirect3DIndexBuffer9 **out,VD3DHANDLE*) {
    auto b=new IDirect3DIndexBuffer9; b->m_device=this; b->m_restype=D3DRTYPE_INDEXBUFFER; b->m_ctx=nullptr; b->m_idxBuffer=nullptr;
    b->m_idxDesc={}; b->m_idxDesc.Format=format; b->m_idxDesc.Type=D3DRTYPE_INDEXBUFFER; b->m_idxDesc.Usage=usage; b->m_idxDesc.Pool=pool; b->m_idxDesc.Size=n;
    b->m_native=newBuffer(n); *out=b; return S_OK;
}
#include "metal_buffer_pool_ios.inc"
static HRESULT lockBuffer(IDirect3DResource9 *r,unsigned offset,unsigned size,void **out,DWORD flags) {
    @autoreleasepool {
    auto &b=native<Buffer>(r); if(!size)size=b.length-offset;
    if(b.locked || offset+size>b.length)return D3DERR_INVALIDCALL;
    auto &d=native<Device>(r->m_device);
    const double renameStart=d.profileRemaining ? Plat_FloatTime():0;
    if(flags&D3DLOCK_DISCARD) {
        perfCount(PerfCount::BufferRenames);
        auto old=b.buffer;b.buffer=acquireRenameBuffer(d,b.length);
        retireRenameBuffer(d,old);
    }
    else if(!(flags&(D3DLOCK_NOOVERWRITE|D3DLOCK_READONLY))) {
        // Source vertex/index buffers are GPU read-only. Rename a writing
        // lock and preserve untouched bytes instead of waiting for the GPU;
        // encoded commands retain the previous buffer until they complete.
        perfCount(PerfCount::BufferRenames);
        perfCount(PerfCount::BufferPreservedBytes,b.length);
        auto replacement=acquireRenameBuffer(d,b.length);
        if(!replacement)fatal("buffer rename",@"out of memory");
        memcpy(replacement.contents,b.buffer.contents,b.length); auto old=b.buffer;b.buffer=replacement;retireRenameBuffer(d,old);
    }
    if(renameStart)perfCount(PerfCount::BufferRenameCPUNanoseconds,
        static_cast<unsigned long long>(std::max(0.0,(Plat_FloatTime()-renameStart)*1e9)));
    b.locked=true;b.lockOffset=offset;b.lockLength=size; *out=static_cast<uint8_t*>(b.buffer.contents)+offset; return S_OK;
    } // autoreleasepool
}
static HRESULT unlockBuffer(IUnknown *r) { auto &b=native<Buffer>(r); if(!b.locked)return D3DERR_INVALIDCALL; b.locked=false; return S_OK; }
HRESULT IDirect3DVertexBuffer9::Lock(UINT o,UINT n,void **p,DWORD f) { return lockBuffer(this,o,n,p,f); }
HRESULT IDirect3DIndexBuffer9::Lock(UINT o,UINT n,void **p,DWORD f) { return lockBuffer(this,o,n,p,f); }
HRESULT IDirect3DVertexBuffer9::Unlock() { return unlockBuffer(this); }
HRESULT IDirect3DIndexBuffer9::Unlock() { return unlockBuffer(this); }
#include "buffer_selftest_ios.inc"
static void actualUnlock(IUnknown *r,unsigned n,const void *p) { auto &b=native<Buffer>(r); if(n>b.lockLength)fatal("buffer unlock",@"actual size exceeds lock"); if(p)memcpy(static_cast<uint8_t*>(b.buffer.contents)+b.lockOffset,p,n); unlockBuffer(r); }
void IDirect3DVertexBuffer9::UnlockActualSize(uint n,const void *p) { actualUnlock(this,n,p); }
void IDirect3DIndexBuffer9::UnlockActualSize(uint n,const void *p) { actualUnlock(this,n,p); }
HRESULT IDirect3DIndexBuffer9::GetDesc(D3DINDEXBUFFER_DESC *p) { *p=m_idxDesc; return S_OK; }
IDirect3DVertexBuffer9::~IDirect3DVertexBuffer9() { m_device->ReleasedVertexBuffer(this); delete static_cast<Object*>(m_native); }
IDirect3DIndexBuffer9::~IDirect3DIndexBuffer9() { m_device->ReleasedIndexBuffer(this); delete static_cast<Object*>(m_native); }
HRESULT IDirect3DDevice9::SetTextureNonInline(DWORD slot,IDirect3DBaseTexture9 *t) { if(slot>=20)return D3DERR_INVALIDCALL; m_textures[slot]=t; return S_OK; }
HRESULT IDirect3DDevice9::SetStreamSourceNonInline(UINT slot,IDirect3DVertexBuffer9 *b,UINT offset,UINT stride) { if(slot>=D3D_MAX_STREAMS)return D3DERR_INVALIDCALL; m_streams[slot]={b,offset,stride}; return S_OK; }
HRESULT IDirect3DDevice9::SetIndicesNonInline(IDirect3DIndexBuffer9 *b) { m_indices.m_idxBuffer=b; return S_OK; }
HRESULT IDirect3DDevice9::CreateVertexDeclaration(const D3DVERTEXELEMENT9 *src,IDirect3DVertexDeclaration9 **out) {
    auto d=new IDirect3DVertexDeclaration9; d->m_device=this;
    static std::atomic<unsigned> serial{0}; auto declaration=new VertexDeclaration; declaration->serial=++serial; d->m_native=declaration; d->m_elemCount=0; memset(d->m_VertexAttribDescToStreamIndex,0xff,sizeof(d->m_VertexAttribDescToStreamIndex));
    while(src->Stream!=0xff && d->m_elemCount<MAX_D3DVERTEXELEMENTS) {
        if(src->Stream>=D3D_MAX_STREAMS || vertexFormat(src->Type)==MTLVertexFormatInvalid) { delete d; return D3DERR_INVALIDCALL; }
        d->m_elements[d->m_elemCount].m_dxdecl=*src;
        d->m_VertexAttribDescToStreamIndex[(src->Usage<<4)|src->UsageIndex]=d->m_elemCount;
        ++d->m_elemCount; ++src;
    }
    *out=d; return S_OK;
}
IDirect3DVertexDeclaration9::~IDirect3DVertexDeclaration9() { m_device->ReleasedVertexDeclaration(this); delete static_cast<Object*>(m_native); }
HRESULT IDirect3DDevice9::SetVertexDeclarationNonInline(IDirect3DVertexDeclaration9 *d) { m_pVertDecl=d; return S_OK; }
HRESULT IDirect3DDevice9::CreateVertexShader(const DWORD *code,IDirect3DVertexShader9 **out,const char *name,char *label) {
    auto shader=newShader(code,name,label); auto p=new IDirect3DVertexShader9; p->m_device=this; p->m_restype=(D3DRESOURCETYPE)0; p->m_vtxProgram=nullptr;
    p->m_vtxHighWater=p->m_vtxHighWaterBone=0; p->m_maxVertexAttrs=0; memset(p->m_vtxAttribMap,0xff,sizeof(p->m_vtxAttribMap));
    memcpy(p->m_vtxAttribMap,shader->vertexAttribMap.data(),sizeof(p->m_vtxAttribMap));p->m_maxVertexAttrs=shader->maxVertexAttrs;
    for(auto &u:shader->uniforms)if(u.type==MOJOSHADER_UNIFORM_FLOAT)p->m_vtxHighWater=std::max(p->m_vtxHighWater,(unsigned)(u.index+std::max(1,u.array_count)));
    p->m_native=shader.release(); *out=p; return S_OK;
}
HRESULT IDirect3DDevice9::CreatePixelShader(const DWORD *code,IDirect3DPixelShader9 **out,const char *name,char *label,const uint32 *centroid) {
    auto shader=newShader(code,name,label,centroid); auto p=new IDirect3DPixelShader9; p->m_device=this; p->m_restype=(D3DRESOURCETYPE)0; p->m_pixProgram=nullptr;
    p->m_pixHighWater=p->m_pixSamplerMask=p->m_pixSamplerTypes=p->m_pixFragDataMask=0;
    for(unsigned slot:shader->samplerSlots)p->m_pixSamplerMask|=1u<<slot;
    for(auto &u:shader->uniforms)if(u.type==MOJOSHADER_UNIFORM_FLOAT)p->m_pixHighWater=std::max(p->m_pixHighWater,(unsigned)(u.index+std::max(1,u.array_count)));
    p->m_native=shader.release(); *out=p; return S_OK;
}
IDirect3DVertexShader9::~IDirect3DVertexShader9() { m_device->ReleasedVertexShader(this); delete static_cast<Object*>(m_native); }
IDirect3DPixelShader9::~IDirect3DPixelShader9() { m_device->ReleasedPixelShader(this); delete static_cast<Object*>(m_native); }
HRESULT IDirect3DDevice9::SetVertexShaderNonInline(IDirect3DVertexShader9 *s) { m_vertexShader=s; return S_OK; }
HRESULT IDirect3DDevice9::SetPixelShaderNonInline(IDirect3DPixelShader9 *s) { m_pixelShader=s; return S_OK; }
struct SourceMetalDeviceAccess {
    static void interpolationDepth(IDirect3DDevice9 *dev,const ICSMInterpolationView &view) {
        if(dev->m_pDepthStencil)fgDepth(native<Device>(dev),native<Image>(dev->m_pDepthStencil).image->texture,view);
    }
    static void interpolationColor(IDirect3DDevice9 *dev) {
        if(dev->m_pRenderTargets[0])fgColor(native<Device>(dev),native<Image>(dev->m_pRenderTargets[0]).image->texture);
    }
    static void interpolationViewmodel(IDirect3DDevice9 *dev) {
        if(dev->m_pDepthStencil)fgViewmodel(native<Device>(dev),native<Image>(dev->m_pDepthStencil).image->texture);
    }
    static void interpolationCompact(IDirect3DDevice9 *dev,const ICSMSceneRect &rect) {
        if(dev->m_pRenderTargets[0])fgCompactColor(native<Device>(dev),native<Image>(dev->m_pRenderTargets[0]).image->texture,rect);
    }
    static bool spatialScene(IDirect3DDevice9 *dev, const ICSMSceneRect &src, const ICSMSceneRect &dst) {
        auto surface=dev->m_pRenderTargets[0];if(!surface)return false;
        auto &image=native<Image>(surface);
        bool ok=spatialEncode(native<Device>(dev),image.image->texture,image.mip,image.face,src,dst);
        if(ok)invalidateLightmapShadow(*image.image,true);
        return ok;
    }
#include "render_pass_ios.inc"
    template<class T> static void keyValue(std::string &k,const T &v) { k.append(reinterpret_cast<const char*>(&v),sizeof(v)); }
    static id<MTLTexture> targetTexture(IDirect3DSurface9 *surface,bool srgb=false) { return surface ? native<Image>(surface).image->renderView(srgb):nil; }
    static MTLRenderPassDescriptor *pass(IDirect3DDevice9 *dev,DWORD clear=0,D3DCOLOR color=0,float z=1,DWORD stencil=0) {
        auto &d=native<Device>(dev); auto p=acquireSourcePassDescriptor();
        unsigned width=UINT_MAX,height=UINT_MAX;
        for(unsigned i=0;i<4;++i) if(auto surface=dev->m_pRenderTargets[i]) {
            auto &image=native<Image>(surface); auto a=p.colorAttachments[i];
            a.texture=image.image->renderView(d.state.rs[D3DRS_SRGBWRITEENABLE]!=0); a.level=image.mip; a.slice=image.face;
            if(!(a.texture.usage&MTLTextureUsageRenderTarget)) fatal("render target usage",[NSString stringWithFormat:@"%@ format %d D3D usage 0x%x",a.texture.label,image.image->d3dFormat,surface->m_desc.Usage]);
            a.loadAction=(clear&D3DCLEAR_TARGET) ? MTLLoadActionClear:MTLLoadActionLoad; a.storeAction=MTLStoreActionStore;
            // Original CCascadeLightManager binds this null-format color
            // target solely for D3D shadow rendering. Only _rt_CSMShadowDepth
            // is sampled afterward; never discard the shadow depth atlas.
            if(icsm_metal_csm_discard_dummy.GetBool() && dev->m_pDepthStencil &&
               image.image->texture.label && !V_stricmp(image.image->texture.label.UTF8String,"_rt_csmshadowdummy")) {
                a.loadAction=MTLLoadActionDontCare;a.storeAction=MTLStoreActionDontCare;
            }
            a.clearColor=MTLClearColorMake(((color>>16)&255)/255.,((color>>8)&255)/255.,(color&255)/255.,((color>>24)&255)/255.);
            width=std::min(width,std::max(1u,image.image->width>>image.mip)); height=std::min(height,std::max(1u,image.image->height>>image.mip));
        }
        if(auto surface=dev->m_pDepthStencil) {
            auto &image=native<Image>(surface); auto a=p.depthAttachment;
            a.texture=image.image->texture; a.level=image.mip; a.slice=image.face;
            a.loadAction=(clear&D3DCLEAR_ZBUFFER) ? MTLLoadActionClear:MTLLoadActionLoad; a.storeAction=MTLStoreActionStore; a.clearDepth=z;
            if(image.image->format.stencil) { auto s=p.stencilAttachment; s.texture=a.texture; s.level=a.level; s.slice=a.slice; s.loadAction=(clear&D3DCLEAR_STENCIL) ? MTLLoadActionClear:MTLLoadActionLoad; s.storeAction=MTLStoreActionStore; s.clearStencil=stencil; }
            width=std::min(width,std::max(1u,image.image->width>>image.mip)); height=std::min(height,std::max(1u,image.image->height>>image.mip));
        }
        if(width==UINT_MAX || height==UINT_MAX)fatal("render pass",@"no bound attachments");
        p.renderTargetWidth=width; p.renderTargetHeight=height; d.renderWidth=width; d.renderHeight=height;
        d.visibilityBuffer=nil;d.visibilityOffset=0;
        if(!icsm_metal_temporary_reuse.GetBool() || d.activeQuery) {
            perfCount(PerfCount::VisibilityAllocations);
            d.visibilityBuffer=[device() newBufferWithLength:8192 options:MTLResourceStorageModeShared];
            if(!d.visibilityBuffer)fatal("visibility buffer",@"out of memory");
            memset(d.visibilityBuffer.contents,0,8192);
            p.visibilityResultBuffer=d.visibilityBuffer;
        }
        perfPass(p);
        return p;
    }
    static void startVisibility(Device &d) {
        if(!d.encoder || !d.activeQuery)return;
        // Apple permits each offset only once per render pass. Never reuse
        // a slot, including when a partial clear interrupts a Source query.
        if(d.visibilityOffset+8>d.visibilityBuffer.length) {endEncoder(d); return;}
        const unsigned offset=d.visibilityOffset; d.visibilityOffset+=8;
        d.activeQuery->results.push_back(Query::Result{d.visibilityBuffer,offset});
        [d.encoder setVisibilityResultMode:MTLVisibilityResultModeCounting offset:offset];
    }
    static void beginPass(IDirect3DDevice9 *dev,DWORD clear=0,D3DCOLOR color=0,float z=1,DWORD stencil=0) {
        auto &d=native<Device>(dev); if(d.encoder && !clear && (!icsm_metal_pass_reuse.GetBool() || compatiblePass(dev)))return;
        endEncoder(d); beginCommand(d);
        auto descriptor=pass(dev,clear,color,z,stencil);
        d.encoder=[d.command renderCommandEncoderWithDescriptor:descriptor];
        sourcePass=descriptor;sourcePassOwner=&d;sourcePassBindingsDirty=false;
        d.encoder.label=@"Source material-system render pass";
        startVisibility(d);
    }
    static id<MTLDepthStencilState> depthState(IDirect3DDevice9 *dev) {
        auto &d=native<Device>(dev); auto &r=d.state.rs;
        DepthKey key{}; unsigned index=0; const unsigned states[]={D3DRS_ZENABLE,D3DRS_ZWRITEENABLE,D3DRS_ZFUNC,D3DRS_STENCILENABLE,D3DRS_STENCILFAIL,D3DRS_STENCILZFAIL,D3DRS_STENCILPASS,D3DRS_STENCILFUNC,D3DRS_STENCILMASK,D3DRS_STENCILWRITEMASK,D3DRS_TWOSIDEDSTENCILMODE,D3DRS_CCW_STENCILFAIL,D3DRS_CCW_STENCILZFAIL,D3DRS_CCW_STENCILPASS,D3DRS_CCW_STENCILFUNC};
        for(auto s:states)key[index++]=r[s];
        const bool hasDepth=dev->m_pDepthStencil!=nullptr;
        const bool hasStencil=hasDepth && native<Image>(dev->m_pDepthStencil).image->format.stencil;
        key[index++]=hasDepth; key[index++]=hasStencil;
        if(d.lastDepth && key==d.lastDepthKey)return d.lastDepth;
        d.lastDepthKey=key;
        auto it=d.depthStates.find(key); if(it!=d.depthStates.end()) { d.lastDepth=it->second; return d.lastDepth; }
        auto desc=[MTLDepthStencilDescriptor new]; desc.depthCompareFunction=hasDepth && r[D3DRS_ZENABLE] ? compare(r[D3DRS_ZFUNC]):MTLCompareFunctionAlways;
        desc.depthWriteEnabled=hasDepth && r[D3DRS_ZENABLE] && r[D3DRS_ZWRITEENABLE];
        if(r[D3DRS_STENCILENABLE] && dev->m_pDepthStencil && native<Image>(dev->m_pDepthStencil).image->format.stencil) {
            auto front=[MTLStencilDescriptor new]; front.stencilCompareFunction=compare(r[D3DRS_STENCILFUNC]);
            front.stencilFailureOperation=stencilOp(r[D3DRS_STENCILFAIL]); front.depthFailureOperation=stencilOp(r[D3DRS_STENCILZFAIL]); front.depthStencilPassOperation=stencilOp(r[D3DRS_STENCILPASS]);
            front.readMask=r[D3DRS_STENCILMASK]; front.writeMask=r[D3DRS_STENCILWRITEMASK];
            MTLStencilDescriptor *back=[front copy]; if(r[D3DRS_TWOSIDEDSTENCILMODE]) { back.stencilCompareFunction=compare(r[D3DRS_CCW_STENCILFUNC]); back.stencilFailureOperation=stencilOp(r[D3DRS_CCW_STENCILFAIL]); back.depthFailureOperation=stencilOp(r[D3DRS_CCW_STENCILZFAIL]); back.depthStencilPassOperation=stencilOp(r[D3DRS_CCW_STENCILPASS]); }
            desc.frontFaceStencil=front; desc.backFaceStencil=back;
        }
        auto state=[device() newDepthStencilStateWithDescriptor:desc]; if(!state)fatal("depth stencil",@"state creation failed"); d.depthStates.emplace(key,state); d.lastDepth=state; return state;
    }
#include "clear_rects_ios.inc"
    static MTLSamplerAddressMode address(unsigned mode) {
        switch(mode) { case D3DTADDRESS_WRAP:return MTLSamplerAddressModeRepeat; case D3DTADDRESS_CLAMP:return MTLSamplerAddressModeClampToEdge; case D3DTADDRESS_BORDER:return MTLSamplerAddressModeClampToBorderColor; default:fatal("sampler address",@"unsupported Source address mode"); return MTLSamplerAddressModeClampToEdge; }
    }
    static id<MTLSamplerState> sampler(Device &d,unsigned slot,bool shadow,float worldLOD=0,unsigned worldAnisotropy=0) {
        auto &r=d.state.sampler[slot]; auto &cached=d.samplerCache[slot];
        const float minLOD=shadow ? float(r[D3DSAMP_MAXMIPLEVEL]):std::max(float(r[D3DSAMP_MAXMIPLEVEL]),worldLOD);
        unsigned maxAnisotropy=r[D3DSAMP_MINFILTER]==D3DTEXF_ANISOTROPIC || r[D3DSAMP_MAGFILTER]==D3DTEXF_ANISOTROPIC ? std::max(1u,std::min(16u,r[D3DSAMP_MAXANISOTROPY])):1;
        if(worldAnisotropy)maxAnisotropy=std::max(1u,(maxAnisotropy*worldAnisotropy+99)/100);
        if(cached.result && cached.shadow==shadow && cached.values==r && cached.minLOD==minLOD && cached.maxAnisotropy==maxAnisotropy)return cached.result;
        std::string key(reinterpret_cast<const char*>(r.data()),r.size()*sizeof(DWORD)); keyValue(key,shadow); keyValue(key,minLOD); keyValue(key,maxAnisotropy);
        auto it=d.samplers.find(key); if(it!=d.samplers.end()) { cached.values=r; cached.shadow=shadow; cached.minLOD=minLOD; cached.maxAnisotropy=maxAnisotropy; cached.result=it->second; return it->second; }
        auto desc=[MTLSamplerDescriptor new]; desc.sAddressMode=address(r[D3DSAMP_ADDRESSU]); desc.tAddressMode=address(r[D3DSAMP_ADDRESSV]); desc.rAddressMode=address(r[D3DSAMP_ADDRESSW]);
        desc.minFilter=r[D3DSAMP_MINFILTER]==D3DTEXF_POINT ? MTLSamplerMinMagFilterNearest:MTLSamplerMinMagFilterLinear;
        desc.magFilter=r[D3DSAMP_MAGFILTER]==D3DTEXF_POINT ? MTLSamplerMinMagFilterNearest:MTLSamplerMinMagFilterLinear;
        desc.mipFilter=r[D3DSAMP_MIPFILTER]==D3DTEXF_NONE ? MTLSamplerMipFilterNotMipmapped:r[D3DSAMP_MIPFILTER]==D3DTEXF_POINT ? MTLSamplerMipFilterNearest:MTLSamplerMipFilterLinear;
        desc.maxAnisotropy=maxAnisotropy;
        desc.lodMinClamp=minLOD;
        desc.compareFunction=shadow ? MTLCompareFunctionLessEqual:MTLCompareFunctionNever;
        DWORD border=r[D3DSAMP_BORDERCOLOR]; desc.borderColor=border==0xffffffff ? MTLSamplerBorderColorOpaqueWhite:border&0xff000000 ? MTLSamplerBorderColorOpaqueBlack:MTLSamplerBorderColorTransparentBlack;
        if(border!=0 && border!=0xff000000 && border!=0xffffffff) fatal("sampler border",@"nonstandard border color requires shader emulation");
        auto result=[device() newSamplerStateWithDescriptor:desc]; if(!result)fatal("sampler",@"state creation failed"); d.samplers.emplace(key,result); cached.values=r; cached.shadow=shadow; cached.minLOD=minLOD; cached.maxAnisotropy=maxAnisotropy; cached.result=result; return result;
    }
    static void prepare(IDirect3DDevice9 *dev) {
        auto &profileDevice=native<Device>(dev);
        const double profileStart=profileDevice.profileRemaining ? Plat_FloatTime():0;
    @autoreleasepool {
        auto &d=native<Device>(dev); if(!dev->m_vertexShader || !dev->m_pixelShader || !dev->m_pVertDecl)fatal("draw",@"missing original shader or vertex declaration");
        beginPass(dev);
        auto &vs=native<Shader>(dev->m_vertexShader); auto &ps=native<Shader>(dev->m_pixelShader); auto &rs=d.state.rs;
        // Describe the complete immutable PSO state before allocating any
        // Objective-C descriptors. Source changes buffers/offsets per draw;
        // only the declaration and stream strides affect its vertex layout.
        // Fixed, fully initialized key avoids per-field string appends on
        // every Source draw, while retaining all immutable PSO inputs.
        DrawKey values{}; unsigned keyIndex=0;
        values[keyIndex++]=vs.serial; values[keyIndex++]=ps.serial;
        values[keyIndex++]=native<VertexDeclaration>(dev->m_pVertDecl).serial;
        for(unsigned i=0;i<D3D_MAX_STREAMS;++i)values[keyIndex++]=dev->m_streams[i].m_stride;
        std::array<MTLPixelFormat,4> formats{}; unsigned samples=1;
        for(unsigned slot=0;slot<4;++slot) {
            auto surf=dev->m_pRenderTargets[slot];
            if(surf) {
                const auto &t=*native<Image>(surf).image;
                formats[slot]=rs[D3DRS_SRGBWRITEENABLE] && t.format.srgb!=MTLPixelFormatInvalid ? t.format.srgb:t.format.linear;
                samples=t.samples;
            } else formats[slot]=MTLPixelFormatInvalid;
            values[keyIndex++]=formats[slot];
            unsigned write=slot ? D3DRS_COLORWRITEENABLE1+slot-1:D3DRS_COLORWRITEENABLE;
            values[keyIndex++]=rs[write];
        }
        MTLPixelFormat depth=MTLPixelFormatInvalid,stencil=MTLPixelFormatInvalid;
        if(dev->m_pDepthStencil) { auto &t=*native<Image>(dev->m_pDepthStencil).image; depth=t.format.linear; if(t.format.stencil)stencil=t.format.linear; }
        const unsigned pipelineStates[]={D3DRS_ALPHABLENDENABLE,D3DRS_SRCBLEND,D3DRS_DESTBLEND,D3DRS_BLENDOP,D3DRS_SEPARATEALPHABLENDENABLE,D3DRS_SRCBLENDALPHA,D3DRS_DESTBLENDALPHA,D3DRS_BLENDOPALPHA,D3DRS_ADAPTIVETESS_Y};
        for(auto r:pipelineStates)values[keyIndex++]=rs[r];
        values[keyIndex++]=depth; values[keyIndex++]=stencil; values[keyIndex++]=samples;
        DrawPipeline drawPipeline;
        if(d.lastDrawPipeline.pipeline && d.lastDrawKey==values)drawPipeline=d.lastDrawPipeline;
        else if(auto cached=d.drawPipelines.find(values); cached!=d.drawPipelines.end())drawPipeline=cached->second;
        else {
            auto descriptor=[MTLRenderPipelineDescriptor new]; descriptor.vertexFunction=vs.function; descriptor.fragmentFunction=ps.function;
            descriptor.label=[NSString stringWithFormat:@"%s + %s",vs.label.c_str(),ps.label.c_str()];
            auto v=[MTLVertexDescriptor vertexDescriptor];
        for(const auto& a:vs.attributes) {
            const unsigned reg=a.reg;
            const D3DVERTEXELEMENT9 *element=nullptr;
            for(unsigned j=0;j<dev->m_pVertDecl->m_elemCount;++j) { auto &e=dev->m_pVertDecl->m_elements[j].m_dxdecl; if(e.Usage==a.usage && e.UsageIndex==a.index) { element=&e; break; } }
            if(!element) {
                // Source dxabstract.cpp FlushVertexBindings allows absent
                // normals, UVs and colors. Its disabled GL attribute arrays
                // supply the generic attribute default (0,0,0,1).
                if(a.usage!=D3DDECLUSAGE_NORMAL && a.usage!=D3DDECLUSAGE_TEXCOORD && a.usage!=D3DDECLUSAGE_COLOR)
                    fatal("vertex binding",[NSString stringWithFormat:@"%s missing usage %d:%d",vs.label.c_str(),a.usage,a.index]);
                auto attr=v.attributes[reg]; attr.format=MTLVertexFormatFloat4; attr.offset=0; attr.bufferIndex=17;
                v.layouts[17].stride=16; v.layouts[17].stepFunction=MTLVertexStepFunctionConstant; v.layouts[17].stepRate=0;
                drawPipeline.defaultAttributes=true;
                continue;
            }
            unsigned stream=element->Stream; auto &s=dev->m_streams[stream]; if(!s.m_vtxBuffer)fatal("vertex binding",@"missing stream buffer");
            drawPipeline.streamMask|=1u<<stream;
            auto attr=v.attributes[reg]; attr.format=vertexFormat(element->Type); attr.offset=element->Offset; attr.bufferIndex=stream+1;
            v.layouts[stream+1].stride=s.m_stride; v.layouts[stream+1].stepFunction=MTLVertexStepFunctionPerVertex;
        }
            descriptor.vertexDescriptor=v;
            for(unsigned slot=0;slot<4;++slot) {
                auto c=descriptor.colorAttachments[slot]; c.pixelFormat=formats[slot];
                unsigned write=slot ? D3DRS_COLORWRITEENABLE1+slot-1:D3DRS_COLORWRITEENABLE;
                c.writeMask=static_cast<MTLColorWriteMask>(0);
                if(rs[write]&1)c.writeMask|=MTLColorWriteMaskRed; if(rs[write]&2)c.writeMask|=MTLColorWriteMaskGreen; if(rs[write]&4)c.writeMask|=MTLColorWriteMaskBlue; if(rs[write]&8)c.writeMask|=MTLColorWriteMaskAlpha;
                c.blendingEnabled=rs[D3DRS_ALPHABLENDENABLE];
                c.sourceRGBBlendFactor=blendFactor(rs[D3DRS_SRCBLEND]); c.destinationRGBBlendFactor=blendFactor(rs[D3DRS_DESTBLEND]); c.rgbBlendOperation=blendOp(rs[D3DRS_BLENDOP]);
                c.sourceAlphaBlendFactor=blendFactor(rs[rs[D3DRS_SEPARATEALPHABLENDENABLE] ? D3DRS_SRCBLENDALPHA:D3DRS_SRCBLEND]);
                c.destinationAlphaBlendFactor=blendFactor(rs[rs[D3DRS_SEPARATEALPHABLENDENABLE] ? D3DRS_DESTBLENDALPHA:D3DRS_DESTBLEND]);
                c.alphaBlendOperation=blendOp(rs[rs[D3DRS_SEPARATEALPHABLENDENABLE] ? D3DRS_BLENDOPALPHA:D3DRS_BLENDOP]);
            }
            descriptor.depthAttachmentPixelFormat=depth; descriptor.stencilAttachmentPixelFormat=stencil;
            descriptor.rasterSampleCount=samples; descriptor.alphaToCoverageEnabled=rs[D3DRS_ADAPTIVETESS_Y]!=0;
            const std::string key(reinterpret_cast<const char*>(values.data()),sizeof(values));
            drawPipeline.pipeline=d.pipeline(key,descriptor); d.drawPipelines.emplace(values,drawPipeline);
        }
        d.lastDrawKey=values; d.lastDrawPipeline=drawPipeline;
        const auto pipeline=drawPipeline.pipeline;
        auto &encoded=d.encoded;
        if(encoded.pipeline!=pipeline) { [d.encoder setRenderPipelineState:pipeline]; encoded.pipeline=pipeline; }
        auto depthStateValue=depthState(dev);
        if(encoded.depth!=depthStateValue) { [d.encoder setDepthStencilState:depthStateValue]; encoded.depth=depthStateValue; }
        const auto &vp=d.state.viewport;
        if(!encoded.valid || memcmp(&encoded.viewport,&vp,sizeof(vp))) {
            [d.encoder setViewport:MTLViewport{double(vp.X),double(vp.Y),double(vp.Width),double(vp.Height),vp.MinZ,vp.MaxZ}]; encoded.viewport=vp;
        }
        MTLScissorRect sc={vp.X,vp.Y,vp.Width,vp.Height};
        if(rs[D3DRS_SCISSORTESTENABLE]) { const auto &s=d.state.scissor; sc={static_cast<NSUInteger>(std::max(0,s.left)),static_cast<NSUInteger>(std::max(0,s.top)),static_cast<NSUInteger>(std::max(0,s.right-std::max(0,s.left))),static_cast<NSUInteger>(std::max(0,s.bottom-std::max(0,s.top)))}; }
        sc.width=std::min(sc.width,d.renderWidth-std::min(sc.x,(NSUInteger)d.renderWidth)); sc.height=std::min(sc.height,d.renderHeight-std::min(sc.y,(NSUInteger)d.renderHeight));
        if(!encoded.valid || memcmp(&encoded.scissor,&sc,sizeof(sc))) { [d.encoder setScissorRect:sc]; encoded.scissor=sc; }
        if(!encoded.valid)[d.encoder setFrontFacingWinding:MTLWindingClockwise];
        if(!encoded.valid || encoded.rs[D3DRS_CULLMODE]!=rs[D3DRS_CULLMODE])
            [d.encoder setCullMode:rs[D3DRS_CULLMODE]==D3DCULL_NONE ? MTLCullModeNone:rs[D3DRS_CULLMODE]==D3DCULL_CW ? MTLCullModeFront:MTLCullModeBack];
        if(!encoded.valid || encoded.rs[D3DRS_FILLMODE]!=rs[D3DRS_FILLMODE])
            [d.encoder setTriangleFillMode:rs[D3DRS_FILLMODE]==D3DFILL_WIREFRAME ? MTLTriangleFillModeLines:MTLTriangleFillModeFill];
        if(!encoded.valid || encoded.rs[D3DRS_DEPTHBIAS]!=rs[D3DRS_DEPTHBIAS] || encoded.rs[D3DRS_SLOPESCALEDEPTHBIAS]!=rs[D3DRS_SLOPESCALEDEPTHBIAS]) {
            float bias,slope; memcpy(&bias,&rs[D3DRS_DEPTHBIAS],4); memcpy(&slope,&rs[D3DRS_SLOPESCALEDEPTHBIAS],4);
            // Source's retained macOS ABI supplies polygon-offset units,
            // not a normalized D3D depth offset (dxabstract.cpp:6222-6238,
            // glmgr.h:393-398). CSM cascade 1/2 deliberately use 2 units.
            [d.encoder setDepthBias:bias slopeScale:slope clamp:0];
        }
        if(!encoded.valid || encoded.rs[D3DRS_STENCILREF]!=rs[D3DRS_STENCILREF])[d.encoder setStencilReferenceValue:rs[D3DRS_STENCILREF]];
        if(!encoded.valid || encoded.rs[D3DRS_BLENDFACTOR]!=rs[D3DRS_BLENDFACTOR]) {
            unsigned blend=rs[D3DRS_BLENDFACTOR]; [d.encoder setBlendColorRed:((blend>>16)&255)/255.f green:((blend>>8)&255)/255.f blue:(blend&255)/255.f alpha:((blend>>24)&255)/255.f];
        }
        const auto defaults=drawPipeline.defaultAttributes ? d.defaultAttributes:nil;
        if(encoded.buffers[17]!=defaults) { [d.encoder setVertexBuffer:defaults offset:0 atIndex:17]; encoded.buffers[17]=defaults; }
        for(unsigned i=0;i<D3D_MAX_STREAMS;++i) {
            auto b=(drawPipeline.streamMask&(1u<<i)) ? dev->m_streams[i].m_vtxBuffer:nullptr;
            if(!b) {if(encoded.buffers[i+1]) { [d.encoder setVertexBuffer:nil offset:0 atIndex:i+1]; encoded.buffers[i+1]=nil; encoded.offsets[i+1]=0;} continue;}
            auto buffer=native<Buffer>(b).buffer; const NSUInteger offset=dev->m_streams[i].m_offset;
            if(encoded.buffers[i+1]!=buffer || encoded.offsets[i+1]!=offset) {
                [d.encoder setVertexBuffer:buffer offset:offset atIndex:i+1]; encoded.buffers[i+1]=buffer; encoded.offsets[i+1]=offset;
            }
        }
        for(const auto *shader:{&vs,&ps}) {
            bool vertex=shader==&vs;
            auto &constants=d.uniformScratch; packUniforms(*shader,d.state,constants);
            const unsigned stage=vertex ? 0:1;
            if(!constants.empty() && (!encoded.uniformValid[stage] || d.uniformValues[stage]!=constants)) {
                NSUInteger offset; auto b=d.upload(constants.data(),constants.size(),offset);
                if(vertex)[d.encoder setVertexBuffer:b offset:offset atIndex:0]; else [d.encoder setFragmentBuffer:b offset:offset atIndex:0];
                d.uniformValues[stage]=constants; encoded.uniformValid[stage]=true;
            }
            for(unsigned slot:shader->samplerSlots) {
                unsigned sourceSlot=slot+(vertex ? 16:0);
                auto t=dev->m_textures[sourceSlot]; id<MTLTexture> texture=t ? native<Image>(t).image->view(d.state.sampler[sourceSlot][D3DSAMP_SRGBTEXTURE]!=0):nil;
                bool shadow=(shader->shadowSamplerMask&(1u<<slot))!=0;
                const bool worldMaterial=!vertex && !shadow && (shader->worldMaterialSamplerMask&(1u<<slot)) && texture.mipmapLevelCount>1 && d.state.sampler[sourceSlot][D3DSAMP_MIPFILTER]!=D3DTEXF_NONE;
                auto ss=sampler(d,sourceSlot,shadow,worldMaterial ? icsmWorldMinLOD:0,worldMaterial ? icsmWorldAnisotropyPercent:0);
                if(resolutionAuditRemaining)recordQualitySampler(d,*shader,sourceSlot,texture,worldMaterial);
                if(encoded.textures[sourceSlot]!=texture) {
                    if(vertex)[d.encoder setVertexTexture:texture atIndex:slot]; else [d.encoder setFragmentTexture:texture atIndex:slot];
                    encoded.textures[sourceSlot]=texture;
                }
                if(encoded.samplers[sourceSlot]!=ss) {
                    if(vertex)[d.encoder setVertexSamplerState:ss atIndex:slot]; else [d.encoder setFragmentSamplerState:ss atIndex:slot];
                    encoded.samplers[sourceSlot]=ss;
                }
            }
        }
        if(ps.hasColor) { float alpha[4]={float(rs[D3DRS_ALPHATESTENABLE]),rs[D3DRS_ALPHAREF]/255.f,float(rs[D3DRS_ALPHAFUNC]),0};
            if(!encoded.alphaValid || memcmp(encoded.alpha,alpha,sizeof(alpha))) { [d.encoder setFragmentBytes:alpha length:sizeof(alpha) atIndex:30]; memcpy(encoded.alpha,alpha,sizeof(alpha)); encoded.alphaValid=true; }
        }
        encoded.rs=rs; encoded.valid=true;
        ++d.draws;
        if(resolutionAuditRemaining){auto target=dev->m_pRenderTargets[0];recordResolutionDraw(d,target?&native<Image>(target):nullptr,ps);}
        } // autoreleasepool
        if(profileStart)profileDevice.profileFrame.prepare+=(Plat_FloatTime()-profileStart)*1000;
}
};
static unsigned vertices(D3DPRIMITIVETYPE t,unsigned count) { switch(t) { case D3DPT_POINTLIST:return count; case D3DPT_LINELIST:return count*2; case D3DPT_TRIANGLELIST:return count*3; case D3DPT_TRIANGLESTRIP:return count+2; default:fatal("primitive",@"unknown Source topology"); return 0; } }
static MTLPrimitiveType primitive(D3DPRIMITIVETYPE t) { switch(t) { case D3DPT_POINTLIST:return MTLPrimitiveTypePoint; case D3DPT_LINELIST:return MTLPrimitiveTypeLine; case D3DPT_TRIANGLELIST:return MTLPrimitiveTypeTriangle; case D3DPT_TRIANGLESTRIP:return MTLPrimitiveTypeTriangleStrip; default:fatal("primitive",@"unknown Source topology"); return MTLPrimitiveTypeTriangle; } }
HRESULT IDirect3DDevice9::DrawPrimitive(D3DPRIMITIVETYPE t,UINT first,UINT count) {
    @autoreleasepool {
    if(!count)return S_OK; SourceMetalDeviceAccess::prepare(this); auto &d=native<Device>(this);
    [d.encoder drawPrimitives:primitive(t) vertexStart:first vertexCount:vertices(t,count)]; return S_OK;
    }
}
HRESULT IDirect3DDevice9::DrawIndexedPrimitive(D3DPRIMITIVETYPE t,INT base,UINT,UINT,UINT first,UINT count) {
    @autoreleasepool {
    if(!count)return S_OK; if(!m_indices.m_idxBuffer)return D3DERR_INVALIDCALL;
    SourceMetalDeviceAccess::prepare(this); auto &d=native<Device>(this); auto &b=native<Buffer>(m_indices.m_idxBuffer);
    bool wide=m_indices.m_idxBuffer->m_idxDesc.Format==D3DFMT_INDEX32; unsigned bytes=wide ? 4:2; id<MTLBuffer> indices=b.buffer; NSUInteger offset=first*bytes; unsigned n=vertices(t,count);
    if(offset+n*bytes>b.length)return D3DERR_INVALIDCALL;

    [d.encoder drawIndexedPrimitives:primitive(t) indexCount:n indexType:wide ? MTLIndexTypeUInt32:MTLIndexTypeUInt16 indexBuffer:indices indexBufferOffset:offset instanceCount:1 baseVertex:base baseInstance:0]; return S_OK;
    }
}
HRESULT IDirect3DDevice9::DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE t,UINT minimum,UINT num,UINT count,const void *index,D3DFORMAT format,const void *vertex,UINT stride) {
    IDirect3DVertexBuffer9 *vb; IDirect3DIndexBuffer9 *ib; CreateVertexBuffer((minimum+num)*stride,D3DUSAGE_DYNAMIC,0,D3DPOOL_DEFAULT,&vb,nullptr); CreateIndexBuffer(vertices(t,count)*(format==D3DFMT_INDEX32 ? 4:2),D3DUSAGE_DYNAMIC,format,D3DPOOL_DEFAULT,&ib,nullptr);
    memcpy(native<Buffer>(vb).buffer.contents,vertex,(minimum+num)*stride); memcpy(native<Buffer>(ib).buffer.contents,index,native<Buffer>(ib).length);
    auto old=m_streams[0]; auto oldIndex=m_indices.m_idxBuffer; SetStreamSourceNonInline(0,vb,0,stride); SetIndicesNonInline(ib);
    auto result=DrawIndexedPrimitive(t,0,minimum,num,0,count); SetStreamSourceNonInline(0,old.m_vtxBuffer,old.m_offset,old.m_stride); SetIndicesNonInline(oldIndex); vb->Release(); ib->Release(); return result;
}
HRESULT IDirect3DDevice9::FlushIndexBindings() { return S_OK; } // Bound by each native draw.
HRESULT IDirect3DDevice9::FlushVertexBindings(uint) { return S_OK; } // Bound by each native draw.
static void copyImage(Device &d,Image &src,Image &dst,const RECT *sourceRect,const RECT *destRect,D3DTEXTUREFILTERTYPE filter, bool presentation = false, bool touchOverlay = false) {
    @autoreleasepool {
    perfCount(PerfCount::Copies);
    invalidateLightmapShadow(*dst.image);
    endEncoder(d); beginCommand(d);
    auto st=src.image->texture,dt=dst.image->texture;
    const unsigned sw=std::max(1u,src.image->width>>src.mip),sh=std::max(1u,src.image->height>>src.mip),dw=std::max(1u,dst.image->width>>dst.mip),dh=std::max(1u,dst.image->height>>dst.mip);
    RECT sr=sourceRect ? *sourceRect:RECT{0,0,(int)sw,(int)sh},dr=destRect ? *destRect:RECT{0,0,(int)dw,(int)dh};
    if(src.image->samples>1) {
        if(dst.image->samples!=1 || sw!=dw || sh!=dh || sourceRect || destRect)fatal("MSAA resolve",@"requires full matching single-sample destination");
        auto p=[MTLRenderPassDescriptor renderPassDescriptor];
        if(src.image->format.depth) { auto a=p.depthAttachment; a.texture=st; a.resolveTexture=dt; a.loadAction=MTLLoadActionLoad; a.storeAction=MTLStoreActionMultisampleResolve; }
        else { auto a=p.colorAttachments[0]; a.texture=st; a.resolveTexture=dt; a.loadAction=MTLLoadActionLoad; a.storeAction=MTLStoreActionMultisampleResolve; }
        perfPass(p); auto e=[d.command renderCommandEncoderWithDescriptor:p]; [e endEncoding]; return;
    }
    if(!presentation && !dt.framebufferOnly && st.pixelFormat==dt.pixelFormat && sr.right-sr.left==dr.right-dr.left && sr.bottom-sr.top==dr.bottom-dr.top) {
        perfCount(PerfCount::Blits); auto e=[d.command blitCommandEncoder]; [e copyFromTexture:st sourceSlice:src.face sourceLevel:src.mip sourceOrigin:MTLOrigin{(NSUInteger)sr.left,(NSUInteger)sr.top,0} sourceSize:MTLSize{(NSUInteger)(sr.right-sr.left),(NSUInteger)(sr.bottom-sr.top),1} toTexture:dt destinationSlice:dst.face destinationLevel:dst.mip destinationOrigin:MTLOrigin{(NSUInteger)dr.left,(NSUInteger)dr.top,0}]; [e endEncoding]; return;
    }
    if(src.image->format.depth || dst.image->format.depth)fatal("depth copy",@"nonmatching or scaled depth blit");
    static NSString *code=@"#include <metal_stdlib>\nusing namespace metal;\nstruct V { float4 p [[position]]; float2 uv; };\nvertex V copy_v(uint id [[vertex_id]], constant float4 &rect [[buffer(0)]]) { float2 p=float2((id<<1)&2,id&2); V o; o.p=float4(p*float2(2,-2)+float2(-1,1),0,1); o.uv=rect.xy+p*rect.zw; return o; }\nfragment float4 copy_f(V v [[stage_in]], texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]]) { return t.sample(s,v.uv); }\n";
    NSString *gammaCode=[code stringByReplacingOccurrencesOfString:@"return t.sample(s,v.uv);" withString:@"float4 c=t.sample(s,v.uv); float3 q=clamp(c.rgb,0.0f,1.0f)*255.0f; uint3 a=uint3(floor(q)), b=min(a+1,uint3(255)); float3 f=fract(q); c.rgb=float3(mix(g[a.x].x,g[b.x].x,f.x),mix(g[a.y].y,g[b.y].y,f.y),mix(g[a.z].z,g[b.z].z,f.z)); return c;"];
    gammaCode=[gammaCode stringByReplacingOccurrencesOfString:@"sampler s [[sampler(0)]])" withString:@"sampler s [[sampler(0)]], constant float4 *g [[buffer(1)]])"];
    const bool applyGamma=presentation && d.gammaSet;
    const std::string key=std::string(applyGamma ? "source_gamma_":"source_copy_")+std::to_string(dt.pixelFormat)+"_"+std::to_string(dt.sampleCount);
    auto existing=d.pipelines.find(key);id<MTLRenderPipelineState> pipeline=existing!=d.pipelines.end()?existing->second:nil;
    if(!pipeline) { auto source=applyGamma ? gammaCode:code;auto lib=warmProceduralLibrary(source);
        auto descriptor=[MTLRenderPipelineDescriptor new]; descriptor.vertexFunction=warmProceduralFunction(lib,source,@"copy_v"); descriptor.fragmentFunction=warmProceduralFunction(lib,source,@"copy_f"); descriptor.colorAttachments[0].pixelFormat=dt.pixelFormat; descriptor.rasterSampleCount=dt.sampleCount;
        pipeline=d.pipeline(key,descriptor); }
    const bool fullDestination=dr.left==0 && dr.top==0 && dr.right==dw && dr.bottom==dh;
    auto p=[MTLRenderPassDescriptor renderPassDescriptor]; p.colorAttachments[0].texture=dt; p.colorAttachments[0].level=dst.mip; p.colorAttachments[0].slice=dst.face; p.colorAttachments[0].loadAction=fullDestination ? MTLLoadActionDontCare:MTLLoadActionLoad; p.colorAttachments[0].storeAction=MTLStoreActionStore;
    perfPass(p); auto e=[d.command renderCommandEncoderWithDescriptor:p]; e.label=@"Source StretchRect / presentation";
    [e setRenderPipelineState:pipeline]; [e setViewport:MTLViewport{double(dr.left),double(dr.top),double(dr.right-dr.left),double(dr.bottom-dr.top),0,1}];
    float rect[4]={sr.left/float(sw),sr.top/float(sh),(sr.right-sr.left)/float(sw),(sr.bottom-sr.top)/float(sh)}; [e setVertexBytes:rect length:sizeof(rect) atIndex:0]; [e setFragmentTexture:st atIndex:0];
    auto &sampler=d.copySamplers[filter==D3DTEXF_POINT ? 0:1];
    if(!sampler) {auto sd=[MTLSamplerDescriptor new]; sd.minFilter=sd.magFilter=filter==D3DTEXF_POINT ? MTLSamplerMinMagFilterNearest:MTLSamplerMinMagFilterLinear; sd.sAddressMode=sd.tAddressMode=MTLSamplerAddressModeClampToEdge; sampler=[device() newSamplerStateWithDescriptor:sd];}
    if(applyGamma) { NSUInteger off=0; auto buf=d.upload(d.gamma,sizeof(d.gamma),off); [e setFragmentBuffer:buf offset:off atIndex:1]; }
    [e setFragmentSamplerState:sampler atIndex:0]; [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
    if(touchOverlay)SourceTouchRenderMetalWithEncoder((__bridge void *)e,(__bridge void *)dt);
    [e endEncoding];
    } // autoreleasepool
}
// I2 probe reads the exact presentation pass into a CPU-readable surface.
extern "C" __attribute__((visibility("default"))) unsigned SourceMetalPresentationPixel(IDirect3DDevice9 *dev) {
    auto &d=native<Device>(dev); IDirect3DSurface9 *src=nullptr,*dst=nullptr;
    dev->GetRenderTarget(0,&src);
    dev->CreateRenderTarget(d.width,d.height,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,TRUE,&dst,nullptr);
    copyImage(d,native<Image>(src),native<Image>(dst),nullptr,nullptr,D3DTEXF_POINT,true);commit(d,true);
    D3DLOCKED_RECT r;dst->LockRect(&r,nullptr,D3DLOCK_READONLY);unsigned pixel;
    memcpy(&pixel,(char*)r.pBits+(d.height/2)*r.Pitch+(d.width/2)*4,4);
    dst->UnlockRect();dst->Release();src->Release();return pixel;
}
HRESULT IDirect3DDevice9::StretchRect(IDirect3DSurface9 *src,const RECT *sr,IDirect3DSurface9 *dst,const RECT *dr,D3DTEXTUREFILTERTYPE filter) { if(!src || !dst)return D3DERR_INVALIDCALL; copyImage(native<Device>(this),native<Image>(src),native<Image>(dst),sr,dr,filter); return S_OK; }
HRESULT IDirect3DDevice9::GetRenderTargetData(IDirect3DSurface9 *src,IDirect3DSurface9 *dst) { if(!src || !dst)return D3DERR_INVALIDCALL; auto hr=StretchRect(src,nullptr,dst,nullptr,D3DTEXF_POINT); if(hr==S_OK)commit(native<Device>(this),true); return hr; }
HRESULT IDirect3DDevice9::GetFrontBufferData(UINT,IDirect3DSurface9 *dst) { return GetRenderTargetData(m_pDefaultColorSurface,dst); }
IDirect3DDevice9::IDirect3DDevice9() : m_nValidMarker(0x12EBC845),m_pDepthStencil(nullptr),m_pDefaultColorSurface(nullptr),m_pDefaultDepthStencilSurface(nullptr),m_pVertDecl(nullptr),m_vertexShader(nullptr),m_pixelShader(nullptr),m_ctx(nullptr),m_pFBOs(nullptr),m_bFBODirty(false),m_nCaptureMode(RS_CAPTURE_MODE_GAME) {
    memset(m_pRenderTargets,0,sizeof(m_pRenderTargets)); memset(m_streams,0,sizeof(m_streams)); memset(&m_indices,0,sizeof(m_indices)); memset(m_textures,0,sizeof(m_textures)); memset(m_RsShadow,0,sizeof(m_RsShadow)); memset(m_SamplerStateShadow,0,sizeof(m_SamplerStateShadow));
    auto d=new Device; d->queue=[device() newCommandQueue]; d->queue.label=@"Source native Metal queue"; d->owner=ThreadGetCurrentId(); m_native=d; g_sourceNativeDevice=d;g_sourceCombatD3DDevice=this;
    warmLoadArchive(*d);
    warmInitialize(*d);
    const float defaultAttribute[4]={0,0,0,1}; d->defaultAttributes=[device() newBufferWithBytes:defaultAttribute length:sizeof(defaultAttribute) options:MTLResourceStorageModeShared];
    auto &r=d->state.rs;
    r[D3DRS_ZENABLE]=r[D3DRS_ZWRITEENABLE]=1; r[D3DRS_ZFUNC]=D3DCMP_LESSEQUAL; r[D3DRS_CULLMODE]=D3DCULL_CCW; r[D3DRS_FILLMODE]=D3DFILL_SOLID;
    r[D3DRS_SRCBLEND]=r[D3DRS_SRCBLENDALPHA]=D3DBLEND_ONE; r[D3DRS_DESTBLEND]=r[D3DRS_DESTBLENDALPHA]=D3DBLEND_ZERO; r[D3DRS_BLENDOP]=r[D3DRS_BLENDOPALPHA]=D3DBLENDOP_ADD; r[D3DRS_ALPHAFUNC]=D3DCMP_GREATEREQUAL;
    r[D3DRS_STENCILFUNC]=r[D3DRS_CCW_STENCILFUNC]=D3DCMP_ALWAYS;
    for(auto s:{D3DRS_STENCILFAIL,D3DRS_STENCILZFAIL,D3DRS_STENCILPASS,D3DRS_CCW_STENCILFAIL,D3DRS_CCW_STENCILZFAIL,D3DRS_CCW_STENCILPASS})r[s]=D3DSTENCILOP_KEEP;
    r[D3DRS_STENCILMASK]=r[D3DRS_STENCILWRITEMASK]=r[D3DRS_MULTISAMPLEMASK]=r[D3DRS_BLENDFACTOR]=0xffffffff;
    for(auto s:{D3DRS_COLORWRITEENABLE,D3DRS_COLORWRITEENABLE1,D3DRS_COLORWRITEENABLE2,D3DRS_COLORWRITEENABLE3})r[s]=15;
    for(auto &s:d->state.sampler) { s[D3DSAMP_ADDRESSU]=s[D3DSAMP_ADDRESSV]=s[D3DSAMP_ADDRESSW]=D3DTADDRESS_WRAP; s[D3DSAMP_MINFILTER]=s[D3DSAMP_MAGFILTER]=D3DTEXF_POINT; s[D3DSAMP_MAXANISOTROPY]=1; }
}
IDirect3DDevice9::~IDirect3DDevice9() {
    forgetRenameBuffers(native<Device>(this));
    auto &d=native<Device>(this); commit(d,true); d.saveArchive(); for(unsigned i=0;i<4;++i)SetRenderTarget(i,nullptr); SetDepthStencilSurface(nullptr);
    fgForget(d);
    spatialForget(d);
    if(m_pDefaultColorSurface)m_pDefaultColorSurface->Release(); if(m_pDefaultDepthStencilSurface)m_pDefaultDepthStencilSurface->Release(); if(g_sourceNativeDevice==&d)g_sourceNativeDevice=nullptr;
    fgForget(d);
    spatialForget(d);
    warmForget(d);
    if(g_sourceCombatD3DDevice==this)g_sourceCombatD3DDevice=nullptr; delete static_cast<Object*>(m_native);
}
HRESULT IDirect3DDevice9::Create(IDirect3DDevice9Params *p) { m_params=*p; return Reset(&p->m_presentationParameters); }
HRESULT IDirect3DDevice9::Reset(D3DPRESENT_PARAMETERS *p) {
    if(!p)return D3DERR_INVALIDCALL;
    D3DPRESENT_PARAMETERS requested=*p;
    if(!requested.BackBufferWidth || !requested.BackBufferHeight) {
        // The original toglGetClientRect uses RenderedSize for the Source
        // canvas. D3D presentation parameters permit inferred windowed sizes;
        // fullscreen dimensions must be explicit, never zero-sized textures.
        if(!requested.Windowed || !g_pLauncherMgr)return D3DERR_INVALIDCALL;
        unsigned width=0,height=0; g_pLauncherMgr->RenderedSize(width,height,false);
        if(!requested.BackBufferWidth)requested.BackBufferWidth=width;
        if(!requested.BackBufferHeight)requested.BackBufferHeight=height;
        if(!requested.BackBufferWidth || !requested.BackBufferHeight)return D3DERR_INVALIDCALL;
    }
    p=&requested;
    auto &d=native<Device>(this); commit(d,true); d.saveArchive(); for(unsigned i=0;i<4;++i)SetRenderTarget(i,nullptr); SetDepthStencilSurface(nullptr);
    fgForget(d);
    spatialForget(d);
    if(m_pDefaultColorSurface)m_pDefaultColorSurface->Release(); if(m_pDefaultDepthStencilSurface)m_pDefaultDepthStencilSurface->Release();
    m_pDefaultColorSurface=m_pDefaultDepthStencilSurface=nullptr;
    // Match ConvertPresentationParamsToGLMDisplayParams in Source: ONE and
    // unknown intervals synchronize, IMMEDIATE does not. CAMetalLayer
    // otherwise defaults to vsync even when Source requested immediate.
    #if !defined(SOURCE_IOS)
    if(auto l=layer()) { l.displaySyncEnabled=p->PresentationInterval!=D3DPRESENT_INTERVAL_IMMEDIATE;
        Msg("Metal presentation interval=%u display_sync=%d\n",p->PresentationInterval,l.displaySyncEnabled); }
#endif
    m_params.m_presentationParameters=*p; d.width=p->BackBufferWidth; d.height=p->BackBufferHeight; d.samples=p->MultiSampleType==D3DMULTISAMPLE_NONE ? 1:p->MultiSampleType;
    auto hr=CreateRenderTarget(d.width,d.height,p->BackBufferFormat,p->MultiSampleType,p->MultiSampleQuality,FALSE,&m_pDefaultColorSurface,nullptr,(char*)"Source backbuffer"); if(hr!=S_OK)return hr;
    hr=CreateDepthStencilSurface(d.width,d.height,p->AutoDepthStencilFormat,p->MultiSampleType,p->MultiSampleQuality,FALSE,&m_pDefaultDepthStencilSurface,nullptr); if(hr!=S_OK)return hr;
    SetRenderTarget(0,m_pDefaultColorSurface); SetDepthStencilSurface(m_pDefaultDepthStencilSurface);
    d.state.viewport={0,0,d.width,d.height,0,1}; d.state.scissor={0,0,(int)d.width,(int)d.height};
    if(g_pLauncherMgr) { auto w=d.width,h=d.height; g_pLauncherMgr->RenderedSize(w,h,true); }
    return S_OK;
}
HRESULT IDirect3DDevice9::BeginScene() { beginCommand(native<Device>(this)); return S_OK; }
HRESULT IDirect3DDevice9::EndScene() { endEncoder(native<Device>(this)); return S_OK; }
HRESULT IDirect3DDevice9::Present(const RECT*,const RECT*,VD3DHWND,const RGNDATA*) {
    @autoreleasepool {
    auto &d=native<Device>(this); const double profileStart=d.profileRemaining ? Plat_FloatTime():0;
    endEncoder(d); auto l=layer(); if(!l)return D3DERR_DEVICELOST;
    // Source renders its whole frame into an offscreen backbuffer. Submit
    // that work before nextDrawable can block, following CAMetalLayer's
    // documented drawable lifetime guidance. Both submissions use the same
    // ordered queue and tracked textures; no CPU/GPU wait is introduced.
    commit(d,false); d.lastSceneSubmitted=d.lastSubmitted;
    if(d.profileRemaining)d.profileSceneCommands.push_back(d.lastSceneSubmitted);
    if(fgPresent(d,native<Image>(m_pDefaultColorSurface).image->texture)) {
        ++d.frame;afterPresentCapture(d);
        if(profileStart)d.profileFrame.present+=(Plat_FloatTime()-profileStart)*1000;
        finishResolutionAudit(d);
        if(g_pLauncherMgr)g_pLauncherMgr->OnFrameRendered();return S_OK;
    }
    const double acquireStart=d.profileRemaining ? Plat_FloatTime():0;
    id<CAMetalDrawable> drawable=[l nextDrawable]; if(!drawable) { commit(d,false); return S_OK; }
    if(acquireStart)d.profileFrame.acquire+=(Plat_FloatTime()-acquireStart)*1000;
    auto tex=std::make_shared<Texture>(); tex->texture=drawable.texture; tex->format=pixelFormat(D3DFMT_A8R8G8B8); tex->width=drawable.texture.width; tex->height=drawable.texture.height; tex->depth=tex->levels=tex->slices=tex->samples=1;
    const bool mergeTouch=icsm_metal_touch_pass_merge.GetBool() && native<Image>(m_pDefaultColorSurface).image->samples==1;
    perfPresentPhase();
    Image dest; dest.image=tex; copyImage(d,native<Image>(m_pDefaultColorSurface),dest,nullptr,nullptr,D3DTEXF_LINEAR,true,mergeTouch);
    if(!mergeTouch)SourceTouchRenderMetal((__bridge void *)d.command, (__bridge void *)drawable.texture);
    [d.command presentDrawable:drawable]; commit(d,false); ++d.frame; afterPresentCapture(d);
    if(profileStart)d.profileFrame.present+=(Plat_FloatTime()-profileStart)*1000;
    finishResolutionAudit(d);
    if(g_pLauncherMgr)g_pLauncherMgr->OnFrameRendered(); return S_OK;
    } // autoreleasepool
}
HRESULT IDirect3DDevice9::Clear(DWORD count,const D3DRECT *rects,DWORD flags,D3DCOLOR color,float z,DWORD stencil) {
    @autoreleasepool {
    if(!flags)return S_OK;
    if(count && !rects)return D3DERR_INVALIDCALL;
    auto &d=native<Device>(this);
    const DWORD srgb=d.state.rs[D3DRS_SRGBWRITEENABLE];
    const bool keepSRGBView=icsm_metal_invariant_clear_reuse.GetBool() && ICSMClearColorIsSRGBInvariant(color);
    if((flags&D3DCLEAR_TARGET) && srgb && keepSRGBView)perfCount(PerfCount::InvariantClearReuses);
    if((flags&D3DCLEAR_TARGET) && srgb && !keepSRGBView)SetRenderState(D3DRS_SRGBWRITEENABLE,0);
    if(count) { if(!rects)return D3DERR_INVALIDCALL; SourceMetalDeviceAccess::clearRects(this,count,rects,flags,color,z,stencil); }
    else if(d.state.rs[D3DRS_SCISSORTESTENABLE]) { const auto &r=d.state.scissor; D3DRECT rect={r.left,r.top,r.right,r.bottom}; SourceMetalDeviceAccess::clearRects(this,1,&rect,flags,color,z,stencil); }
    else SourceMetalDeviceAccess::beginPass(this,flags,color,z,stencil);
    if((flags&D3DCLEAR_TARGET) && srgb && !keepSRGBView)SetRenderState(D3DRS_SRGBWRITEENABLE,srgb);
    return S_OK;
    } // autoreleasepool
}
HRESULT IDirect3DDevice9::SetViewport(const D3DVIEWPORT9 *v) { native<Device>(this).state.viewport=*v; return S_OK; }
HRESULT IDirect3DDevice9::GetViewport(D3DVIEWPORT9 *v) { *v=native<Device>(this).state.viewport; return S_OK; }
HRESULT IDirect3DDevice9::SetScissorRect(const RECT *r) { native<Device>(this).state.scissor=*r; return S_OK; }
HRESULT IDirect3DDevice9::GetScissorRect(RECT *r) { *r=native<Device>(this).state.scissor; return S_OK; }
HRESULT IDirect3DDevice9::SetRenderState(D3DRENDERSTATETYPE s,DWORD v) {
    if(s>=211)return D3DERR_INVALIDCALL; auto &d=native<Device>(this);
    if(s==D3DRS_SRGBWRITEENABLE && d.state.rs[s]!=v) {perfCount(PerfCount::GammaChanges);if(!icsm_metal_pass_reuse.GetBool())endEncoder(d);else sourcePassBindingsDirty=true;}
    if(s==D3DRS_CLIPPLANEENABLE && v)return unsupported("hardware clip plane; use Source fast clipping");
    d.state.rs[s]=v;
    if(m_nCaptureMode&RS_CAPTURE_MODE_GAME)m_RsShadow[s].value=v; m_RsShadow[s].nCaptureMode|=m_nCaptureMode;
    return S_OK;
}
HRESULT IDirect3DDevice9::SetSamplerStateNonInline(DWORD s,D3DSAMPLERSTATETYPE t,DWORD v) { if(s>=20 || t>=14)return D3DERR_INVALIDCALL; native<Device>(this).state.sampler[s][t]=v; return S_OK; }
void IDirect3DDevice9::SetSamplerStatesNonInline(DWORD s,DWORD u,DWORD v,DWORD w,DWORD min,DWORD mag,DWORD mip) { SetSamplerStateNonInline(s,D3DSAMP_ADDRESSU,u); SetSamplerStateNonInline(s,D3DSAMP_ADDRESSV,v); SetSamplerStateNonInline(s,D3DSAMP_ADDRESSW,w); SetSamplerStateNonInline(s,D3DSAMP_MINFILTER,min); SetSamplerStateNonInline(s,D3DSAMP_MAGFILTER,mag); SetSamplerStateNonInline(s,D3DSAMP_MIPFILTER,mip); }
#define CONST_SET(method,bank,limit,type,width) HRESULT IDirect3DDevice9::method(UINT first,const type *p,UINT n) { if(first+n>limit)return D3DERR_INVALIDCALL; memcpy(native<Device>(this).state.bank[first],p,n*width*sizeof(type)); return S_OK; }
CONST_SET(SetVertexShaderConstantFNonInline,vf,DXABSTRACT_VS_PARAM_SLOTS,float,4)
CONST_SET(SetPixelShaderConstantFNonInline,pf,kGLMProgramParamFloat4Limit,float,4)
CONST_SET(SetVertexShaderConstantINonInline,vi,16,int,4)
CONST_SET(SetPixelShaderConstantI,pi,16,int,4)
#undef CONST_SET
HRESULT IDirect3DDevice9::GetPixelShaderConstantFNonInline(UINT first,float *p,UINT n) { if(first+n>kGLMProgramParamFloat4Limit)return D3DERR_INVALIDCALL; memcpy(p,native<Device>(this).state.pf[first],n*16); return S_OK; }
HRESULT IDirect3DDevice9::SetVertexShaderConstantBNonInline(UINT first,const BOOL *p,UINT n) { if(first+n>16)return D3DERR_INVALIDCALL; memcpy(native<Device>(this).state.vb+first,p,n*sizeof(BOOL)); return S_OK; }
HRESULT IDirect3DDevice9::SetPixelShaderConstantB(UINT first,const BOOL *p,UINT n) { if(first+n>16)return D3DERR_INVALIDCALL; memcpy(native<Device>(this).state.pb+first,p,n*sizeof(BOOL)); return S_OK; }
void IDirect3DDevice9::SetMaxUsedVertexShaderConstantsHintNonInline(uint) {} // Packed constants contain only referenced registers.
void IDirect3DDevice9::AcquireThreadOwnership() { native<Device>(this).owner=ThreadGetCurrentId(); }
void IDirect3DDevice9::ReleaseThreadOwnership() { native<Device>(this).owner=0; }
ThreadId_t IDirect3DDevice9::GetCurrentOwnerThreadId() const { return static_cast<Device*>(m_native)->owner; }
int IDirect3DDevice9::GetTotalSamplerCount() { return 20; }
void IDirect3DDevice9::SaveGLState() { auto &d=native<Device>(this); d.saved=d.state; d.savedValid=true; }
void IDirect3DDevice9::RestoreGLState() { auto &d=native<Device>(this); endEncoder(d); if(d.savedValid) { d.state=d.saved; d.savedValid=false; } }
HRESULT IDirect3DDevice9::LinkShaderPair(IDirect3DVertexShader9*,IDirect3DPixelShader9*) { return S_OK; } // Native PSOs additionally require the draw's vertex/attachment layout.
HRESULT IDirect3DDevice9::ValidateShaderPair(IDirect3DVertexShader9 *v,IDirect3DPixelShader9 *p) { return v && p && native<Shader>(v).function && native<Shader>(p).function ? S_OK:E_FAIL; }
HRESULT IDirect3DDevice9::QueryShaderPair(int,GLMShaderPairInfo *out) { memset(out,0,sizeof(*out)); out->m_status=-1; return S_OK; } // Original GL pair cache is replaced by native PSO cache.
void IDirect3DDevice9::ReleasedTexture(IDirect3DBaseTexture9 *t) { for(auto &s:m_textures)if(s==t)s=nullptr; }
void IDirect3DDevice9::ReleasedSurface(IDirect3DSurface9 *t) { bool bound=m_pDepthStencil==t; if(bound)m_pDepthStencil=nullptr; for(auto &s:m_pRenderTargets)if(s==t){s=nullptr; bound=true;} if(bound)endEncoder(native<Device>(this)); }
void IDirect3DDevice9::ReleasedVertexBuffer(IDirect3DVertexBuffer9 *t) { for(auto &s:m_streams)if(s.m_vtxBuffer==t)s={}; }
void IDirect3DDevice9::ReleasedIndexBuffer(IDirect3DIndexBuffer9 *t) { if(m_indices.m_idxBuffer==t)m_indices.m_idxBuffer=nullptr; }
void IDirect3DDevice9::ReleasedVertexDeclaration(IDirect3DVertexDeclaration9 *t) { if(m_pVertDecl==t)m_pVertDecl=nullptr; }
void IDirect3DDevice9::ReleasedVertexShader(IDirect3DVertexShader9 *t) { if(m_vertexShader==t)m_vertexShader=nullptr; }
void IDirect3DDevice9::ReleasedPixelShader(IDirect3DPixelShader9 *t) { if(m_pixelShader==t)m_pixelShader=nullptr; }
void IDirect3DDevice9::ReleasedQuery(IDirect3DQuery9 *t) { auto &d=native<Device>(this); if(d.activeQuery==&native<Query>(t)){if(d.encoder)[d.encoder setVisibilityResultMode:MTLVisibilityResultModeDisabled offset:0]; d.activeQuery=nullptr;} }
HRESULT IDirect3DDevice9::CreateQuery(D3DQUERYTYPE type,IDirect3DQuery9 **out) { if(type!=D3DQUERYTYPE_EVENT && type!=D3DQUERYTYPE_OCCLUSION)return D3DERR_NOTAVAILABLE; if(!out)return S_OK; auto q=new IDirect3DQuery9; q->m_device=this; q->m_type=type; q->m_ctx=nullptr; q->m_query=nullptr; q->m_native=new Query; *out=q; return S_OK; }
IDirect3DQuery9::~IDirect3DQuery9() { m_device->ReleasedQuery(this); delete static_cast<Object*>(m_native); }
HRESULT IDirect3DQuery9::Issue(DWORD flags) {
    @autoreleasepool {
    auto &q=native<Query>(this); auto &d=native<Device>(m_device); beginCommand(d);
    if(flags==D3DISSUE_BEGIN) { if(m_type!=D3DQUERYTYPE_OCCLUSION || d.activeQuery)return D3DERR_INVALIDCALL; perfCount(PerfCount::Queries); q.results.clear(); q.active=true; q.issued=false; q.command=nil; d.activeQuery=&q;
        // visibilityResultBuffer belongs to the pass descriptor and cannot
        // be attached to an existing encoder. Restart at the first real query;
        // the next draw creates its buffer. Empty queries correctly return 0.
        if(d.encoder && !d.visibilityBuffer)endEncoder(d);
        SourceMetalDeviceAccess::startVisibility(d); }
    else if(flags==D3DISSUE_END) { if(m_type==D3DQUERYTYPE_OCCLUSION && d.activeQuery!=&q)return D3DERR_INVALIDCALL; q.active=false; q.issued=true; q.command=d.command; if(d.activeQuery==&q) {if(d.encoder)[d.encoder setVisibilityResultMode:MTLVisibilityResultModeDisabled offset:0]; d.activeQuery=nullptr;} }
    else return D3DERR_INVALIDCALL;
    return S_OK;
    }
}
HRESULT IDirect3DQuery9::GetData(void *out,DWORD size,DWORD flags) {
    auto &q=native<Query>(this); if(!q.issued)return S_FALSE;
    auto &d=native<Device>(m_device); if((flags&D3DGETDATA_FLUSH) && q.command==d.command)commit(d,false);
    if(q.command.status!=MTLCommandBufferStatusCompleted)return S_FALSE;
    if(out) { if(size!=sizeof(DWORD))return D3DERR_INVALIDCALL; uint64_t result=1; if(m_type==D3DQUERYTYPE_OCCLUSION) { result=0; for(auto &r:q.results)result+=*reinterpret_cast<uint64_t*>(static_cast<uint8_t*>(r.buffer.contents)+r.offset); } *static_cast<DWORD*>(out)=std::min<uint64_t>(result,UINT_MAX); }
    return S_OK;
}
static void caps(D3DCAPS9 &c) {
    memset(&c,0,sizeof(c)); c.DeviceType=D3DDEVTYPE_HAL; c.Caps2=D3DCAPS2_DYNAMICTEXTURES; c.DevCaps=D3DDEVCAPS_HWTRANSFORMANDLIGHT;
    c.TextureCaps=D3DPTEXTURECAPS_CUBEMAP|D3DPTEXTURECAPS_MIPCUBEMAP|D3DPTEXTURECAPS_NONPOW2CONDITIONAL|D3DPTEXTURECAPS_PROJECTED;
    c.PrimitiveMiscCaps=0;
    c.RasterCaps=D3DPRASTERCAPS_SCISSORTEST|D3DPRASTERCAPS_SLOPESCALEDEPTHBIAS|D3DPRASTERCAPS_DEPTHBIAS;
    c.TextureFilterCaps=D3DPTFILTERCAPS_MINFANISOTROPIC|D3DPTFILTERCAPS_MAGFANISOTROPIC;
    // Conservative engine limits within the Metal family feature tables.
    c.MaxTextureWidth=c.MaxTextureHeight=4096; c.MaxVolumeExtent=1024; c.MaxAnisotropy=16;
    c.MaxUserClipPlanes=0; c.MaxPrimitiveCount=32768; c.MaxStreams=D3D_MAX_STREAMS;
    c.VertexShaderVersion=c.PixelShaderVersion=0x300; c.MaxVertexShaderConst=DXABSTRACT_VS_PARAM_SLOTS;
    c.DevCaps2=D3DDEVCAPS2_STREAMOFFSET; c.PS20Caps.NumInstructionSlots=512; c.NumSimultaneousRTs=4;
    c.FakeSRGBWrite=0; c.CanDoSRGBReadFromRTs=1; c.MixedSizeTargets=1; c.NumVertexSamplers=4; c.MaxVertexTIU=4; c.MaxCombinedTIU=20; c.FirstVertexSampler=16; c.TotalSamplerCount=20;
}
IDirect3D9::~IDirect3D9() {}
UINT IDirect3D9::GetAdapterCount() { return GetDisplayDB()->GetFakeAdapterCount(); }
HRESULT IDirect3D9::GetDeviceCaps(UINT,D3DDEVTYPE,D3DCAPS9 *out) { caps(*out); return S_OK; }
HRESULT IDirect3DDevice9::GetDeviceCaps(D3DCAPS9 *out) { caps(*out); return S_OK; }
HRESULT IDirect3D9::GetAdapterIdentifier(UINT adapter,DWORD,D3DADAPTER_IDENTIFIER9 *out) {
    if(adapter>=GetAdapterCount())return D3DERR_INVALIDCALL;
    memset(out,0,sizeof(*out)); V_strncpy(out->Driver,"Native Metal",sizeof(out->Driver)); V_strncpy(out->Description,device().name.UTF8String,sizeof(out->Description));
    out->VendorId=0x106b; out->VideoMemory=std::min<uint64_t>(device().recommendedMaxWorkingSetSize,INT_MAX); return S_OK;
}
HRESULT IDirect3D9::CheckDeviceFormat(UINT,D3DDEVTYPE,D3DFORMAT,DWORD usage,D3DRESOURCETYPE,D3DFORMAT format) {
    auto f=pixelFormat(format); if(f.linear==MTLPixelFormatInvalid)return D3DERR_NOTAVAILABLE;
    if((usage&D3DUSAGE_QUERY_SRGBREAD) && f.srgb==MTLPixelFormatInvalid)return D3DERR_NOTAVAILABLE;
    if((usage&D3DUSAGE_QUERY_SRGBWRITE) && f.srgb==MTLPixelFormatInvalid)return D3DERR_NOTAVAILABLE;
    if((usage&D3DUSAGE_DEPTHSTENCIL) && !f.depth)return D3DERR_NOTAVAILABLE;
    if((usage&D3DUSAGE_RENDERTARGET) && (f.block>1 || f.depth || f.linear==MTLPixelFormatA8Unorm))return D3DERR_NOTAVAILABLE;
    return S_OK;
}
UINT IDirect3D9::GetAdapterModeCount(UINT a,D3DFORMAT) { int r,d; if(GetDisplayDB()->GetFakeAdapterInfo(a,&r,&d,nullptr,nullptr))return 0; return GetDisplayDB()->GetModeCount(r,d); }
HRESULT IDirect3D9::EnumAdapterModes(UINT a,D3DFORMAT f,UINT n,D3DDISPLAYMODE *out) { int r,d; auto db=GetDisplayDB(); GLMDisplayModeInfoFields mode; if(db->GetFakeAdapterInfo(a,&r,&d,nullptr,nullptr) || db->GetModeInfo(r,d,n,&mode))return D3DERR_INVALIDCALL; out->Width=mode.m_modePixelWidth; out->Height=mode.m_modePixelHeight; out->RefreshRate=mode.m_modeRefreshHz; out->Format=f; return S_OK; }
HRESULT IDirect3D9::GetAdapterDisplayMode(UINT a,D3DDISPLAYMODE *out) { return EnumAdapterModes(a,D3DFMT_A8R8G8B8,UINT_MAX,out); }
HRESULT IDirect3D9::CheckDeviceType(UINT,D3DDEVTYPE,D3DFORMAT,D3DFORMAT f,BOOL) { return pixelFormat(f).linear==MTLPixelFormatInvalid ? D3DERR_NOTAVAILABLE:S_OK; }
HRESULT IDirect3D9::CheckDepthStencilMatch(UINT,D3DDEVTYPE,D3DFORMAT,D3DFORMAT color,D3DFORMAT depth) { return pixelFormat(color).linear!=MTLPixelFormatInvalid && pixelFormat(depth).depth ? S_OK:D3DERR_NOTAVAILABLE; }
HRESULT IDirect3D9::CheckDeviceMultiSampleType(UINT,D3DDEVTYPE,D3DFORMAT f,BOOL,D3DMULTISAMPLE_TYPE sample,DWORD *quality) { unsigned count=sample==D3DMULTISAMPLE_NONE ? 1:sample; if(quality)*quality=1; return pixelFormat(f).linear!=MTLPixelFormatInvalid && [device() supportsTextureSampleCount:count] ? S_OK:D3DERR_NOTAVAILABLE; }
HRESULT IDirect3D9::CreateDevice(UINT adapter,D3DDEVTYPE type,VD3DHWND window,DWORD flags,D3DPRESENT_PARAMETERS *p,IDirect3DDevice9 **out) { *out=nullptr; if(type!=D3DDEVTYPE_HAL)return D3DERR_NOTAVAILABLE; auto d=new IDirect3DDevice9; IDirect3DDevice9Params params={}; params.m_adapter=adapter; params.m_deviceType=type; params.m_focusWindow=window; params.m_behaviorFlags=flags; params.m_presentationParameters=*p; auto hr=d->Create(&params); if(hr!=S_OK)delete d; else *out=d; return hr; }
HRESULT IDirect3DDevice9::TestCooperativeLevel() { return S_OK; }
HRESULT IDirect3DDevice9::EvictManagedResources() { commit(native<Device>(this),true); return S_OK; }
HRESULT IDirect3DDevice9::ValidateDevice(DWORD *passes) { if(passes)*passes=1; return S_OK; }
BOOL IDirect3DDevice9::ShowCursor(BOOL show) { if(g_pLauncherMgr)g_pLauncherMgr->SetMouseVisible(show!=0); return show; }
void IDirect3DDevice9::SetGammaRamp(UINT,DWORD,const D3DGAMMARAMP *r) {
#if defined(SOURCE_IOS)
    if(!r)fatal("gamma ramp",@"null Source ramp");
    auto &d=native<Device>(this);
    for(unsigned i=0;i<256;++i) { d.gamma[i][0]=r->red[i]/65535.f; d.gamma[i][1]=r->green[i]/65535.f; d.gamma[i][2]=r->blue[i]/65535.f; d.gamma[i][3]=1; }
    d.gammaSet=true;
#else
    CGGammaValue red[256],green[256],blue[256];
    for(unsigned i=0;i<256;++i) { red[i]=r->red[i]/65535.f; green[i]=r->green[i]/65535.f; blue[i]=r->blue[i]/65535.f; }
    CGSetDisplayTransferByTable(CGMainDisplayID(),256,red,green,blue);
#endif
}
HRESULT IDirect3DDevice9::SetFVF(DWORD) { return unsupported("SetFVF"); }
HRESULT IDirect3DDevice9::GetFVF(DWORD*) { return unsupported("GetFVF"); }
HRESULT IDirect3DDevice9::SetTransform(D3DTRANSFORMSTATETYPE,const D3DMATRIX*) { return unsupported("fixed-function transform"); }
HRESULT IDirect3DDevice9::SetTextureStageState(DWORD,D3DTEXTURESTAGESTATETYPE,DWORD) { return unsupported("fixed-function texture stage"); }
HRESULT IDirect3DDevice9::SetClipPlane(DWORD,const float*) { return unsupported("hardware clip plane"); }
HRESULT IDirect3DDevice9::SetMaterial(const D3DMATERIAL9*) { return unsupported("fixed-function material"); }
HRESULT IDirect3DDevice9::LightEnable(DWORD,BOOL) { return unsupported("fixed-function light"); }
HRESULT IDirect3DDevice9::SetLight(DWORD,const D3DLIGHT9*) { return unsupported("fixed-function light"); }
void IDirect3DDevice9::DumpStatsToConsole(const CCommand*) { auto &d=native<Device>(this); Msg("Metal: frames=%llu draws=%llu pipelines=%zu depth states=%zu samplers=%zu\n",d.frame,d.draws,d.pipelines.size(),d.depthStates.size(),d.samplers.size()); }
HRESULT D3DXCompileShader(LPCSTR,UINT,const D3DXMACRO*,LPD3DXINCLUDE,LPCSTR,LPCSTR,DWORD,LPD3DXBUFFER*,LPD3DXBUFFER*,LPD3DXCONSTANTTABLE*) { return unsupported("runtime HLSL; original VCS bytecode is required"); }
void *ID3DXBuffer::GetBufferPointer() { unsupported("ID3DXBuffer is not created by the Metal path"); return nullptr; }
DWORD ID3DXBuffer::GetBufferSize() { unsupported("ID3DXBuffer is not created by the Metal path"); return 0; }
static Device *g_captureDevice=nullptr;
static unsigned g_captureFrames=0;
static NSString *g_capturePath=nil;
static void beginCapture(Device &d,const char *path,unsigned frames) {
    @autoreleasepool {
    auto manager=[MTLCaptureManager sharedCaptureManager];
    if(manager.isCapturing) { Warning("Metal capture already running\n"); return; }
    if(![manager supportsDestination:MTLCaptureDestinationGPUTraceDocument])fatal("capture",@"GPU trace destination unavailable; launch with MTL_CAPTURE_ENABLED=1");
    commit(d,true); d.saveArchive();
    auto descriptor=[MTLCaptureDescriptor new]; descriptor.captureObject=device(); descriptor.destination=MTLCaptureDestinationGPUTraceDocument;
    descriptor.outputURL=[NSURL fileURLWithPath:@(path)]; NSError *error=nil;
    if(![manager startCaptureWithDescriptor:descriptor error:&error])fatal("capture",error.localizedDescription);
    g_captureDevice=&d; g_captureFrames=std::max(1u,frames); g_capturePath=@(path);
    Msg("Metal capture started: %s (%u frames)\n",path,g_captureFrames);
    } // autoreleasepool
}
static void afterPresentCapture(Device &d) {
    if(g_captureDevice!=&d || !g_captureFrames)return;
    if(--g_captureFrames==0) { [[MTLCaptureManager sharedCaptureManager] stopCapture]; Msg("Metal capture saved: %s\n",g_capturePath.UTF8String); g_captureDevice=nullptr; }
}
CON_COMMAND(m6_metal_capture,"Capture native Metal frames: m6_metal_capture /absolute/path.gputrace [frames]") {
    if(args.ArgC()<2) { Msg("Usage: m6_metal_capture /absolute/path.gputrace [frames]\n"); return; }
    extern Device *g_sourceNativeDevice;
    if(!g_sourceNativeDevice) { Warning("Metal device not active\n"); return; }
    beginCapture(*g_sourceNativeDevice,args[1],args.ArgC()>2 ? atoi(args[2]):1);
}
CON_COMMAND(m6_metal_stats,"Print native Metal pipeline/resource submission statistics") {
    extern Device *g_sourceNativeDevice; if(!g_sourceNativeDevice)return; auto &d=*g_sourceNativeDevice;
    Msg("Native Metal frames=%llu draws=%llu pipelines=%zu depth=%zu samplers=%zu allocated_bytes=%llu upload_free=%zu upload_pending=%zu device=%s\n",d.frame,d.draws,d.pipelines.size(),d.depthStates.size(),d.samplers.size(),(unsigned long long)device().currentAllocatedSize,d.freeUploads.size(),d.pendingUploads.size(),device().name.UTF8String);
}
CON_COMMAND(m6_metal_profile,"Observe native host, draw preparation, presentation and previous completed GPU times") {
    extern Device *g_sourceNativeDevice; if(!g_sourceNativeDevice)return;
    auto &d=*g_sourceNativeDevice; d.profileSamples.clear();
    d.profileSceneCommands.clear(); d.profileSceneGPU.clear();
    d.profileRemaining=std::max(60,std::min(18000,args.ArgC()>1 ? atoi(args[1]):900));
    d.profileSamples.reserve(d.profileRemaining);
    Msg("M6_PROFILE started count=%u; timing overhead enabled only for this observation\n",d.profileRemaining);
}
#include "combat_selftest_ios.inc"
Device *g_sourceNativeDevice=nullptr;

extern "C" __attribute__((visibility("default"))) bool SourceMetalSpatialUpscaleScene(
    const ICSMSceneRect *src, const ICSMSceneRect *dst) {
    if(g_sourceCombatD3DDevice && src)SourceMetalDeviceAccess::interpolationCompact(g_sourceCombatD3DDevice,*src);
    if(!icsm_metal_spatial_upscale.GetBool() || !g_sourceCombatD3DDevice || !src || !dst)return false;
    return SourceMetalDeviceAccess::spatialScene(g_sourceCombatD3DDevice,*src,*dst);
}
#include "spatial_selftest_ios.inc"
#include "frame_interpolation_probe_ios.inc"

extern "C" __attribute__((visibility("default"))) void SourceMetalFrameInterpolationDepth(const ICSMInterpolationView *view) {
    if(view && g_sourceCombatD3DDevice)SourceMetalDeviceAccess::interpolationDepth(g_sourceCombatD3DDevice,*view);
}
extern "C" __attribute__((visibility("default"))) void SourceMetalFrameInterpolationColor() {
    if(g_sourceCombatD3DDevice)SourceMetalDeviceAccess::interpolationColor(g_sourceCombatD3DDevice);
}
extern "C" __attribute__((visibility("default"))) void SourceMetalFrameInterpolationViewmodel() {
    if(g_sourceCombatD3DDevice)SourceMetalDeviceAccess::interpolationViewmodel(g_sourceCombatD3DDevice);
}

#include "clear_selftest_ios.inc"

#include "alpha_atlas_selftest_ios.inc"

#include "texture_staging_selftest_ios.inc"
