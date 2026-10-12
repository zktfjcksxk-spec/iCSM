// Included inside SourceMetalDeviceAccess, the Source device's existing friend.
// Only attachment binding is deferred. Copies, clears, readback, EndScene and
// actual attachment/view changes still end their original Metal encoder.
    static bool compatiblePass(IDirect3DDevice9 *dev) {
        auto &d=native<Device>(dev);
        if(sourcePassOwner!=&d || !sourcePass)return false;
        if(!sourcePassBindingsDirty)return true;
        for(unsigned i=0;i<4;++i) {
            auto a=sourcePass.colorAttachments[i];auto s=dev->m_pRenderTargets[i];
            auto texture=targetTexture(s,d.state.rs[D3DRS_SRGBWRITEENABLE]!=0);
            if(a.texture!=texture){perfCount(PerfCount::AttachmentPassBreaks);return false;}
            if(s && (a.level!=native<Image>(s).mip || a.slice!=native<Image>(s).face))return false;
        }
        auto ds=dev->m_pDepthStencil;
        auto depth=ds ? native<Image>(ds).image->texture:nil;
        auto stencil=ds && native<Image>(ds).image->format.stencil ? depth:nil;
        if(sourcePass.depthAttachment.texture!=depth || sourcePass.stencilAttachment.texture!=stencil)return false;
        if(ds && (sourcePass.depthAttachment.level!=native<Image>(ds).mip || sourcePass.depthAttachment.slice!=native<Image>(ds).face))return false;
        // A previously separate pass may be a write -> sample dependency even
        // if the bindings ultimately return to the same targets. Retain that
        // boundary whenever ANY bound sample texture aliases an attachment;
        // checking unused sampler bindings as well is deliberately conservative.
        for(auto sampled:dev->m_textures)if(sampled) {
            auto texture=native<Image>(sampled).image->texture;
            if(texture==depth){perfCount(PerfCount::SampleAliasPassBreaks);return false;}
            for(auto s:dev->m_pRenderTargets)if(s && native<Image>(s).image->texture==texture){perfCount(PerfCount::SampleAliasPassBreaks);return false;}
        }
        sourcePassBindingsDirty=false;return true;
    }
    static id<MTLDepthStencilState> clearDepthState(bool depthWrite,bool stencilWrite,bool reuse) {
        // These four immutable states contain no texture or stencil-reference
        // binding. Metal states belong to the singleton native GPU, so video
        // resets and maps can reuse them without retaining map resources.
        static std::array<id<MTLDepthStencilState>,4> states{};
        auto &state=states[(depthWrite?1:0)|(stencilWrite?2:0)];
        if(state && reuse)return state;
        auto descriptor=[MTLDepthStencilDescriptor new];descriptor.depthCompareFunction=MTLCompareFunctionAlways;
        descriptor.depthWriteEnabled=depthWrite;
        if(stencilWrite) {
            auto st=[MTLStencilDescriptor new];st.stencilCompareFunction=MTLCompareFunctionAlways;
            st.depthStencilPassOperation=MTLStencilOperationReplace;st.readMask=st.writeMask=0xff;
            descriptor.frontFaceStencil=descriptor.backFaceStencil=st;
        }
        perfCount(PerfCount::ClearDepthAllocations);
        auto result=[device() newDepthStencilStateWithDescriptor:descriptor];
        if(!reuse)return result;
        state=result;
        if(!state)fatal("clear depth state",@"creation failed");
        return state;
    }
