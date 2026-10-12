// Original rectangular-clear pipeline and values. Immutable descriptors and
// shader source are only assembled on a cache miss, never on steady HUD draws.
    static void clearRects(IDirect3DDevice9 *dev,DWORD count,const D3DRECT *rects,DWORD flags,D3DCOLOR color,float z,DWORD stencil) {
    @autoreleasepool {
        auto &d=native<Device>(dev); beginPass(dev);
        const unsigned width=d.renderWidth,height=d.renderHeight;
        std::array<MTLPixelFormat,4> formats{};unsigned samples=1;bool hasColor=false;
        std::string key="Source rectangle clear";keyValue(key,flags);
        for(unsigned i=0;i<4;++i) {
            auto texture=targetTexture(dev->m_pRenderTargets[i],d.state.rs[D3DRS_SRGBWRITEENABLE]!=0);
            formats[i]=texture ? texture.pixelFormat:MTLPixelFormatInvalid;keyValue(key,formats[i]);
            if(texture){samples=texture.sampleCount;hasColor|=(flags&D3DCLEAR_TARGET)!=0;}
        }
        MTLPixelFormat depthFormat=MTLPixelFormatInvalid,stencilFormat=MTLPixelFormatInvalid;
        if(dev->m_pDepthStencil) {
            auto &t=*native<Image>(dev->m_pDepthStencil).image;
            depthFormat=t.format.linear;if(t.format.stencil)stencilFormat=t.format.linear;samples=t.samples;
        }
        keyValue(key,samples);keyValue(key,depthFormat);keyValue(key,stencilFormat);
        auto found=d.pipelines.find(key);id<MTLRenderPipelineState> pipeline;
        if(found!=d.pipelines.end())pipeline=found->second;
        else {
            auto descriptor=[MTLRenderPipelineDescriptor new];descriptor.label=@"Source rectangle clear";
            std::string source="#include <metal_stdlib>\nusing namespace metal;\nstruct V {float4 p [[position]];};\nvertex V clear_v(uint i [[vertex_id]],constant float4 *c [[buffer(0)]]) {const float2 p[3]={float2(-1,-1),float2(3,-1),float2(-1,3)}; V o; o.p=float4(p[i],c[1].x,1);return o;}\n";
            std::string fields,assignments;
            for(unsigned i=0;i<4;++i) {
                auto attachment=descriptor.colorAttachments[i];attachment.pixelFormat=formats[i];
                attachment.writeMask=(flags&D3DCLEAR_TARGET) ? MTLColorWriteMaskAll:MTLColorWriteMaskNone;
                if(formats[i]!=MTLPixelFormatInvalid && (flags&D3DCLEAR_TARGET)) {
                    fields+="float4 c"+std::to_string(i)+" [[color("+std::to_string(i)+")]];";
                    assignments+="o.c"+std::to_string(i)+"=c[0];";
                }
            }
            descriptor.depthAttachmentPixelFormat=depthFormat;descriptor.stencilAttachmentPixelFormat=stencilFormat;
            descriptor.rasterSampleCount=samples;
            if(hasColor)source+="struct F {"+fields+"};fragment F clear_f(constant float4 *c [[buffer(0)]]) {F o;"+assignments+"return o;}";
            auto library=warmProceduralLibrary(@(source.c_str()));
            descriptor.vertexFunction=warmProceduralFunction(library,@(source.c_str()),@"clear_v");
            if(hasColor)descriptor.fragmentFunction=warmProceduralFunction(library,@(source.c_str()),@"clear_f");
            pipeline=d.pipeline(key,descriptor);
        }
        const bool depthWrite=(flags&D3DCLEAR_ZBUFFER)!=0 && depthFormat!=MTLPixelFormatInvalid;
        const bool stencilWrite=(flags&D3DCLEAR_STENCIL)!=0 && stencilFormat!=MTLPixelFormatInvalid;
        auto ds=clearDepthState(depthWrite,stencilWrite,icsm_metal_temporary_reuse.GetBool());
        [d.encoder setRenderPipelineState:pipeline];[d.encoder setDepthStencilState:ds];[d.encoder setStencilReferenceValue:stencil];
        [d.encoder setViewport:MTLViewport{0,0,double(width),double(height),0,1}];[d.encoder setCullMode:MTLCullModeNone];
        [d.encoder setTriangleFillMode:MTLTriangleFillModeFill];[d.encoder setDepthBias:0 slopeScale:0 clamp:0];
        [d.encoder setVisibilityResultMode:MTLVisibilityResultModeDisabled offset:0];
        const float values[8]={((color>>16)&255)/255.f,((color>>8)&255)/255.f,(color&255)/255.f,((color>>24)&255)/255.f,z,0,0,0};
        [d.encoder setVertexBytes:values length:sizeof(values) atIndex:0];
        if(hasColor)[d.encoder setFragmentBytes:values length:sizeof(values) atIndex:0];
        for(unsigned i=0;i<count;++i) {
            const auto &r=rects[i];unsigned x=std::min(width,(unsigned)std::max(0,r.x1)),y=std::min(height,(unsigned)std::max(0,r.y1));
            unsigned right=std::min(width,(unsigned)std::max(0,r.x2)),bottom=std::min(height,(unsigned)std::max(0,r.y2));
            if(right<=x || bottom<=y)continue;
            [d.encoder setScissorRect:MTLScissorRect{x,y,right-x,bottom-y}];[d.encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
        }
        if(d.activeQuery)startVisibility(d);
        d.encoded=EncodedState{};
    }
    }
