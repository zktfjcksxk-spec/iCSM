// The existing Source device serializes render encoding. A descriptor is only
// reused after endEncoding; the command buffer retains its encoded resources.
// Keep no map textures alive through the reusable descriptor between passes.
static MTLRenderPassDescriptor *reusableSourcePass=nil;
static MTLRenderPassDescriptor *acquireSourcePassDescriptor() {
    if(!icsm_metal_pass_descriptor_reuse.GetBool()) {
        perfCount(PerfCount::PassDescriptorAllocations);
        return [MTLRenderPassDescriptor renderPassDescriptor];
    }
    if(!reusableSourcePass) {
        reusableSourcePass=[MTLRenderPassDescriptor renderPassDescriptor];
        perfCount(PerfCount::PassDescriptorAllocations);
    }
    return reusableSourcePass;
}
static void releaseSourcePassResources(MTLRenderPassDescriptor *p) {
    if(!p || p!=reusableSourcePass)return;
    for(unsigned i=0;i<4;++i) {
        auto a=p.colorAttachments[i];a.texture=nil;a.level=0;a.slice=0;
    }
    auto depth=p.depthAttachment;depth.texture=nil;depth.level=0;depth.slice=0;
    auto stencil=p.stencilAttachment;stencil.texture=nil;stencil.level=0;stencil.slice=0;
    p.visibilityResultBuffer=nil;
}
