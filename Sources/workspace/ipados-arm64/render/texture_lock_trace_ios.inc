// Explicit bounded instrumentation for otherwise synchronous D3D CPU locks.
static ConVar icsm_metal_lock_trace("icsm_metal_lock_trace","0",FCVAR_RELEASE,
    "Log at most eight synchronous texture-lock sources per finite profile");
static void traceTextureWait(const char *operation,const SourceMetal::Image &image,unsigned mip,DWORD flags) {
    if(!icsm_metal_lock_trace.GetBool() || !SourceMetal::perfActive)return;
    static std::string lastTag;static unsigned count=0;
    if(lastTag!=SourceMetal::perfTag){lastTag=SourceMetal::perfTag;count=0;}
    if(count++>=8)return;
    const auto &t=*image.image;
    Msg("ICSM_TEXTURE_WAIT operation=%s label=%s flags=0x%x mip=%u size=%ux%u levels=%u format=%u storage=%u\n",
        operation,t.texture.label.UTF8String?:"",flags,mip,t.width,t.height,t.levels,t.d3dFormat,unsigned(t.texture.storageMode));
}
