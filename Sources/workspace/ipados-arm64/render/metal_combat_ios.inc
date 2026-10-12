// Included in native_resources.mm inside SourceMetal. CPU-only Source lightmap
// pages retain their exact bytes. GPU uploads use the existing completion-gated
// staging pool and the same tracked command queue as draws, never CPU writes to
// a texture that a previous GPU frame might still sample.
static ConVar icsm_metal_lightmap_upload("icsm_metal_lightmap_upload", "1", FCVAR_RELEASE,
    "I5 ordered lightmap staging uploads; diagnostic A/B switch");
static uint64_t lightmapShadowLocks=0,lightmapUploads=0,lightmapUploadBytes=0;
static uint64_t lightmapInvalidations=0,lightmapRememberedPages=0;
static uint64_t lightmapRequestedBytes=0,lightmapUnchanged=0;
static ConVar icsm_metal_alpha_atlas_upload("icsm_metal_alpha_atlas_upload", "1", FCVAR_RELEASE,
    "Ordered uploads for Source CPU-authored Panorama/VGUI glyph atlases; diagnostic A/B switch");
struct AlphaAtlasMirror {
    std::weak_ptr<Texture> texture;
    size_t bytes=0;
    uint64_t used=0;
};
static std::vector<AlphaAtlasMirror> alphaAtlasMirrors;
static size_t alphaAtlasBytes=0;
static constexpr size_t alphaAtlasBudget=32u*1024u*1024u; // Both exact CPU mirrors, combined.
static uint64_t alphaAtlasClock=0,alphaAtlasLocks=0,alphaAtlasReadbacks=0,alphaAtlasEvictions=0;
static bool isVguiFontAtlas(const Texture &t) {
    // CFontTextureCache::AllocateNewPage uses RGBA8888, SINGLECOPY and NOMIP.
    // Restrict this path to its exact generated name, not general VGUI images.
    const char *label=t.texture.label.UTF8String;unsigned page=0;int consumed=0;
    return t.d3dFormat==D3DFMT_A8R8G8B8 && label &&
        sscanf(label,"vgui__font_page_%u%n",&page,&consumed)==1 && consumed==int(strlen(label));
}
static bool isAlphaAtlas(const Texture &t) {
    return t.cpuLightmap && (t.d3dFormat==D3DFMT_A8 || isVguiFontAtlas(t));
}
static bool shadowEnabled(const Texture &t) {
    return isAlphaAtlas(t) ? icsm_metal_alpha_atlas_upload.GetBool():icsm_metal_lightmap_upload.GetBool();
}
static void pruneAlphaAtlasMirrors() {
    for(auto it=alphaAtlasMirrors.begin();it!=alphaAtlasMirrors.end();) {
        if(it->texture.expired()) {alphaAtlasBytes-=it->bytes;it=alphaAtlasMirrors.erase(it);}
        else ++it;
    }
}
static bool reserveAlphaAtlasMirror(Texture &t) {
    pruneAlphaAtlasMirrors();
    AlphaAtlasMirror *entry=nullptr;
    for(auto &e:alphaAtlasMirrors)if(e.texture.lock().get()==&t){entry=&e;break;}
    if(!entry)return false;
    entry->used=++alphaAtlasClock;
    if(entry->bytes)return true;
    const size_t required=size_t(t.width)*t.height*t.format.bytes*2;
    if(required>alphaAtlasBudget)return false;
    while(alphaAtlasBytes+required>alphaAtlasBudget) {
        AlphaAtlasMirror *oldest=nullptr;
        for(auto &e:alphaAtlasMirrors) {
            auto candidate=e.texture.lock();
            if(!candidate || candidate.get()==&t || !e.bytes)continue;
            bool active=false;
            for(const auto &lock:candidate->locks)if(lock.second.active){active=true;break;}
            if(!active && (!oldest || e.used<oldest->used))oldest=&e;
        }
        if(!oldest)return false;
        auto victim=oldest->texture.lock();
        victim->cpuShadowValid=false;
        std::vector<uint8_t>().swap(victim->cpuShadow);
        std::vector<uint8_t>().swap(victim->cpuQueuedShadow);
        alphaAtlasBytes-=oldest->bytes;oldest->bytes=0;++alphaAtlasEvictions;
    }
    entry->bytes=required;alphaAtlasBytes+=required;
    return true;
}

void configureLightmapShadow(const std::shared_ptr<Texture> &image,DWORD usage,const char *label) {
    auto &t=*image;
    // CMatLightmaps::AllocateLightmapTexture creates single-level 2D pages
    // with this exact debug name. GPU-written/render-target textures cannot
    // use a CPU mirror. CMaterialSystem::UpdatePanoramaAlphaTexture also
    // writes exact CPU bytes through TexSubImage2D/BlitSurfaceBits. Restrict
    // its extension to that exact A8 atlas name and the RGBA VGUI font pages
    // created by CFontTextureCache, never arbitrary UI/model textures.
    int page=-1,consumed=0;
    t.cpuLightmap=label && sscanf(label,"[lightmap %d]%n",&page,&consumed)==1 &&
        consumed==int(strlen(label)) && page>=0 && !(usage&(D3DUSAGE_RENDERTARGET|D3DUSAGE_DEPTHSTENCIL)) &&
        t.depth==1 && t.levels==1 && t.slices==1 && t.samples==1 && !t.format.depth &&
        t.format.block==1 && (t.format.bytes==4 || t.format.bytes==8);
    unsigned atlas=0;consumed=0;
    const bool panorama=label && sscanf(label,"panorama_alpha_texture_%u%n",&atlas,&consumed)==1 &&
        consumed==int(strlen(label)) && t.d3dFormat==D3DFMT_A8 && t.format.bytes==1;
    const bool alpha=(panorama || (isVguiFontAtlas(t) && t.format.bytes==4)) &&
        !(usage&(D3DUSAGE_RENDERTARGET|D3DUSAGE_DEPTHSTENCIL)) && t.format.block==1 &&
        t.depth==1 && t.levels==1 && t.slices==1 && t.samples==1 &&
        t.texture.storageMode==MTLStorageModeShared;
    if(alpha) {t.cpuLightmap=true;pruneAlphaAtlasMirrors();alphaAtlasMirrors.push_back({image,0,0});}
    t.lightmapDevice=t.cpuLightmap ? ::g_sourceNativeDevice:nullptr;
}
bool lightmapShadowLock(const Image &image,unsigned mip,unsigned face) {
    const auto &t=*image.image;
    return shadowEnabled(t) && t.cpuLightmap && t.cpuShadowValid &&
        t.lightmapDevice && mip==0 && face==0;
}
void invalidateLightmapShadow(Texture &t,bool renderTarget) {
    if(!t.cpuLightmap)return;
    ++lightmapInvalidations;perfCount(PerfCount::LightmapInvalidations);
    t.cpuShadowValid=false;
    if(renderTarget) {t.cpuLightmap=false;t.cpuShadow.clear();t.cpuShadow.shrink_to_fit();t.cpuQueuedShadow.clear();t.cpuQueuedShadow.shrink_to_fit();}
}
bool lockLightmapShadow(Texture &t,Texture::Lock &l,D3DLOCKED_BOX &out) {
    if(!shadowEnabled(t) || !t.cpuLightmap || !t.lightmapDevice)return false;
    if(isAlphaAtlas(t)) {
        if(!reserveAlphaAtlasMirror(t))return false;
        if(!t.cpuShadowValid) {
            // LockRect has already committed/waited whenever the mirror is
            // invalid. Rehydrate the entire atlas to preserve untouched glyphs
            // after a GPU copy or bounded-cache eviction, then stay asynchronous.
            t.cpuShadow.resize(size_t(t.width)*t.height*t.format.bytes);
            [t.texture getBytes:t.cpuShadow.data() bytesPerRow:t.width*t.format.bytes
                fromRegion:MTLRegionMake2D(0,0,t.width,t.height) mipmapLevel:0];
            t.cpuQueuedShadow=t.cpuShadow;t.cpuShadowValid=true;++alphaAtlasReadbacks;
        }
        ++alphaAtlasLocks;
    }
    if(!t.cpuShadowValid)return false;
    // Pitch may span the full page even for a subrectangle; Source's
    // CPixelWriter uses the returned pitch rather than assuming tight rows.
    l.pitch=t.width*t.format.bytes;l.imagePitch=l.pitch*t.height;l.shadow=true;
    out.pBits=t.cpuShadow.data()+l.region.origin.y*l.pitch+l.region.origin.x*t.format.bytes;
    out.RowPitch=l.pitch;out.SlicePitch=l.imagePitch;
    ++lightmapShadowLocks;perfCount(PerfCount::LightmapShadowLocks);
    return true;
}
void unlockLightmapShadow(Texture &t,Texture::Lock &l) {
    if(l.flags&D3DLOCK_READONLY)return;
    // The queued mirror is the exact contents after all preceding queue
    // writes, even if they have not completed. Only this CPU-owned lightmap
    // path can retain it; every external GPU write invalidates the mirror.
    lightmapRequestedBytes+=((l.region.size.width*t.format.bytes+255u)&~255u)*l.region.size.height;
    unsigned left=t.width,right=0,top=t.height,bottom=0;
    const unsigned x0=l.region.origin.x,x1=x0+l.region.size.width;
    const unsigned y0=l.region.origin.y,y1=y0+l.region.size.height;
    for(unsigned y=y0;y<y1;++y) {
        const auto *now=t.cpuShadow.data()+(y*t.width+x0)*t.format.bytes;
        const auto *old=t.cpuQueuedShadow.data()+(y*t.width+x0)*t.format.bytes;
        if(!memcmp(now,old,(x1-x0)*t.format.bytes))continue;
        unsigned first=x0,last=x1;
        while(first<last && !memcmp(now+(first-x0)*t.format.bytes,old+(first-x0)*t.format.bytes,t.format.bytes))++first;
        while(last>first && !memcmp(now+(last-x0-1)*t.format.bytes,old+(last-x0-1)*t.format.bytes,t.format.bytes))--last;
        left=std::min(left,first);right=std::max(right,last);top=std::min(top,y);bottom=y+1;
    }
    if(left>=right || top>=bottom){++lightmapUnchanged;return;}
    const MTLRegion changed=MTLRegionMake2D(left,top,right-left,bottom-top);
    auto &d=*t.lightmapDevice;
    endEncoder(d);beginCommand(d);
    const unsigned rowBytes=changed.size.width*t.format.bytes;
    const unsigned stagingPitch=(rowBytes+255u)&~255u;
    const unsigned size=stagingPitch*changed.size.height;
    const auto *source=t.cpuShadow.data()+changed.origin.y*t.width*t.format.bytes+changed.origin.x*t.format.bytes;
    const void *bytes=source;
    if(changed.origin.x!=0 || changed.size.width!=t.width || stagingPitch!=t.width*t.format.bytes) {
        l.data.resize(size);
        for(unsigned row=0;row<changed.size.height;++row)
            memcpy(l.data.data()+row*stagingPitch,source+row*t.width*t.format.bytes,rowBytes);
        bytes=l.data.data();
    }
    NSUInteger offset=0;auto buffer=d.upload(bytes,size,offset);
    auto blit=[d.command blitCommandEncoder];
    blit.label=isAlphaAtlas(t)?@"Source ordered alpha glyph atlas update":@"Source ordered CPU lightmap update";
    [blit copyFromBuffer:buffer sourceOffset:offset sourceBytesPerRow:stagingPitch
        sourceBytesPerImage:0 sourceSize:changed.size toTexture:t.texture
        destinationSlice:0 destinationLevel:0 destinationOrigin:changed.origin];
    [blit endEncoding];
    for(unsigned row=0;row<changed.size.height;++row)
        memcpy(t.cpuQueuedShadow.data()+(row+changed.origin.y)*t.width*t.format.bytes+changed.origin.x*t.format.bytes,
               source+row*t.width*t.format.bytes,rowBytes);
    ++lightmapUploads;lightmapUploadBytes+=size;
    perfCount(PerfCount::Blits);perfCount(PerfCount::LightmapUploads);
    perfCount(PerfCount::LightmapUploadBytes,size);
}
void rememberLightmapWrite(Texture &t,const Texture::Lock &l) {
    if(!t.cpuLightmap || (l.flags&D3DLOCK_READONLY))return;
    // A/B fallback or budget refusal uses the original synchronous write.
    // Never allocate unbudgeted alpha mirrors or retain stale queued bytes.
    if(isAlphaAtlas(t)){t.cpuShadowValid=false;return;}
    const bool full=l.region.origin.x==0 && l.region.origin.y==0 &&
        l.region.size.width==t.width && l.region.size.height==t.height;
    if(full) {
        if(!t.cpuShadowValid)++lightmapRememberedPages;
        t.cpuShadow=l.data;t.cpuQueuedShadow=l.data;t.cpuShadowValid=true;
    } else if(t.cpuShadowValid) {
        const unsigned rowBytes=l.region.size.width*t.format.bytes;
        for(unsigned row=0;row<l.region.size.height;++row) {
            memcpy(t.cpuShadow.data()+(row+l.region.origin.y)*t.width*t.format.bytes+l.region.origin.x*t.format.bytes,
                   l.data.data()+row*l.pitch,rowBytes);
            memcpy(t.cpuQueuedShadow.data()+(row+l.region.origin.y)*t.width*t.format.bytes+l.region.origin.x*t.format.bytes,
                   l.data.data()+row*l.pitch,rowBytes);
        }
    }
}
extern "C" __attribute__((visibility("default"))) NSDictionary *SourceMetalCombatSnapshot() {
    pruneAlphaAtlasMirrors();
    return @{@"ordered_lightmap_uploads":@(icsm_metal_lightmap_upload.GetBool()),
        @"ordered_alpha_atlas_uploads":@(icsm_metal_alpha_atlas_upload.GetBool()),
        @"ordered_vgui_font_uploads":@(icsm_metal_alpha_atlas_upload.GetBool()),
        @"alpha_atlas_locks":@(alphaAtlasLocks),@"alpha_atlas_initial_readbacks":@(alphaAtlasReadbacks),
        @"alpha_atlas_mirror_bytes":@(alphaAtlasBytes),@"alpha_atlas_budget_bytes":@(alphaAtlasBudget),
        @"alpha_atlas_evictions":@(alphaAtlasEvictions),
        @"shadow_locks":@(lightmapShadowLocks),@"uploads":@(lightmapUploads),
        @"upload_bytes":@(lightmapUploadBytes),@"invalidations":@(lightmapInvalidations),
        @"requested_upload_bytes":@(lightmapRequestedBytes),@"unchanged_updates":@(lightmapUnchanged),
        @"remembered_full_pages":@(lightmapRememberedPages)};
}
