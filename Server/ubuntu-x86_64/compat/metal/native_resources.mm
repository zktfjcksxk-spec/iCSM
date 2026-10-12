#include "native_objects.h"
#include <cmath>
namespace SourceMetal {
void fatal(const char *op, NSString *details) { fprintf(stderr, "Metal %s: %s\n", op, details.UTF8String ?: "unknown error"); fflush(stderr); Error("Metal %s: %s\n", op, details.UTF8String ?: "unknown error"); }
id<MTLDevice> device() {
    static id<MTLDevice> d = MTLCreateSystemDefaultDevice();
    if (!d) fatal("device", @"no native Metal device");
    return d;
}
Format pixelFormat(D3DFORMAT f) {
    Format r;
    switch (f) {
#define F(d,m,b) case d: r.linear=m; r.bytes=b; break
        F(D3DFMT_A8R8G8B8, MTLPixelFormatBGRA8Unorm, 4);
        F(D3DFMT_X8R8G8B8, MTLPixelFormatBGRA8Unorm, 4);
        F(D3DFMT_A8B8G8R8, MTLPixelFormatRGBA8Unorm, 4);
        F(D3DFMT_X8B8G8R8, MTLPixelFormatRGBA8Unorm, 4);
        F(D3DFMT_L8, MTLPixelFormatR8Unorm, 1);
        F(D3DFMT_A8, MTLPixelFormatA8Unorm, 1);
        F(D3DFMT_A8L8, MTLPixelFormatRG8Unorm, 2);
        F(D3DFMT_V8U8, MTLPixelFormatRG8Snorm, 2);
        F(D3DFMT_Q8W8V8U8, MTLPixelFormatRGBA8Snorm, 4);
        F(D3DFMT_A16B16G16R16F, MTLPixelFormatRGBA16Float, 8);
        F(D3DFMT_A16B16G16R16, MTLPixelFormatRGBA16Unorm, 8);
        F(D3DFMT_R32F, MTLPixelFormatR32Float, 4);
        F(D3DFMT_G16R16F, MTLPixelFormatRG16Float, 4);
        F(D3DFMT_G32R32F, MTLPixelFormatRG32Float, 8);
        F(D3DFMT_A32B32G32R32F, MTLPixelFormatRGBA32Float, 16);
        F(D3DFMT_A2B10G10R10, MTLPixelFormatRGB10A2Unorm, 4);
        F(D3DFMT_A2R10G10B10, MTLPixelFormatBGR10A2Unorm, 4);
#undef F
        case D3DFMT_DXT1: r.linear=MTLPixelFormatBC1_RGBA; r.srgb=MTLPixelFormatBC1_RGBA_sRGB; r.bytes=8; r.block=4; break;
        case D3DFMT_DXT3: r.linear=MTLPixelFormatBC2_RGBA; r.srgb=MTLPixelFormatBC2_RGBA_sRGB; r.bytes=16; r.block=4; break;
        case D3DFMT_DXT5: r.linear=MTLPixelFormatBC3_RGBA; r.srgb=MTLPixelFormatBC3_RGBA_sRGB; r.bytes=16; r.block=4; break;
        case D3DFMT_D16: r.linear=MTLPixelFormatDepth16Unorm; r.depth=true; r.bytes=2; break;
        case D3DFMT_D24X8: case D3DFMT_NV_INTZ: r.linear=MTLPixelFormatDepth32Float; r.depth=true; r.bytes=4; break;
        case D3DFMT_D24S8: case D3DFMT_D24FS8: r.linear=MTLPixelFormatDepth32Float_Stencil8; r.depth=true; r.stencil=true; r.bytes=8; break;
        default: break;
    }
    if (r.linear==MTLPixelFormatBGRA8Unorm) r.srgb=MTLPixelFormatBGRA8Unorm_sRGB;
    if (r.linear==MTLPixelFormatRGBA8Unorm) r.srgb=MTLPixelFormatRGBA8Unorm_sRGB;
    if (f==D3DFMT_L8 || f==D3DFMT_A8L8) {
        r.swizzle={MTLTextureSwizzleRed, MTLTextureSwizzleRed, MTLTextureSwizzleRed,
          f==D3DFMT_L8 ? MTLTextureSwizzleOne : MTLTextureSwizzleGreen};
    }
    if (f==D3DFMT_X8R8G8B8 || f==D3DFMT_X8B8G8R8) r.swizzle.alpha=MTLTextureSwizzleOne;
    return r;
}
MTLVertexFormat vertexFormat(unsigned f) {
    switch (f) {
#define F(d,m) case d: return m
        F(D3DDECLTYPE_FLOAT1,MTLVertexFormatFloat); F(D3DDECLTYPE_FLOAT2,MTLVertexFormatFloat2);
        F(D3DDECLTYPE_FLOAT3,MTLVertexFormatFloat3); F(D3DDECLTYPE_FLOAT4,MTLVertexFormatFloat4);
        // This build retains Source's OPENGL_SWAP_COLORS CPU vertex ABI.
        // CVertexBuilder writes RGBA colors and reversed bone indices; the
        // original SM3 instructions already consume that ordering.
        F(D3DDECLTYPE_D3DCOLOR,MTLVertexFormatUChar4Normalized); F(D3DDECLTYPE_UBYTE4,MTLVertexFormatUChar4);
        F(D3DDECLTYPE_SHORT2,MTLVertexFormatShort2); F(D3DDECLTYPE_SHORT4,MTLVertexFormatShort4);
        F(D3DDECLTYPE_UBYTE4N,MTLVertexFormatUChar4Normalized); F(D3DDECLTYPE_SHORT2N,MTLVertexFormatShort2Normalized);
        F(D3DDECLTYPE_SHORT4N,MTLVertexFormatShort4Normalized); F(D3DDECLTYPE_USHORT2N,MTLVertexFormatUShort2Normalized);
        F(D3DDECLTYPE_USHORT4N,MTLVertexFormatUShort4Normalized);
        F(D3DDECLTYPE_FLOAT16_2,MTLVertexFormatHalf2); F(D3DDECLTYPE_FLOAT16_4,MTLVertexFormatHalf4);
#undef F
        default: fatal("vertex format", [NSString stringWithFormat:@"unsupported Source declaration %u",f]); return MTLVertexFormatInvalid;
    }
}
MTLCompareFunction compare(unsigned f) {
    // D3DCMP is one-based in the original Source ABI; Metal is zero-based.
    if (f<D3DCMP_NEVER || f>D3DCMP_ALWAYS) fatal("compare", @"invalid Source compare function");
    return static_cast<MTLCompareFunction>(f-D3DCMP_NEVER);
}
MTLStencilOperation stencilOp(unsigned f) {
    switch (f) {
#define F(d,m) case d: return m
        F(D3DSTENCILOP_KEEP,MTLStencilOperationKeep); F(D3DSTENCILOP_ZERO,MTLStencilOperationZero);
        F(D3DSTENCILOP_REPLACE,MTLStencilOperationReplace); F(D3DSTENCILOP_INCRSAT,MTLStencilOperationIncrementClamp);
        F(D3DSTENCILOP_DECRSAT,MTLStencilOperationDecrementClamp); F(D3DSTENCILOP_INVERT,MTLStencilOperationInvert);
        F(D3DSTENCILOP_INCR,MTLStencilOperationIncrementWrap); F(D3DSTENCILOP_DECR,MTLStencilOperationDecrementWrap);
#undef F
        default: fatal("stencil", @"invalid Source operation"); return MTLStencilOperationKeep;
    }
}
MTLBlendFactor blendFactor(unsigned f) {
    switch (f) {
#define F(d,m) case d: return m
        F(D3DBLEND_ZERO,MTLBlendFactorZero); F(D3DBLEND_ONE,MTLBlendFactorOne);
        F(D3DBLEND_SRCCOLOR,MTLBlendFactorSourceColor); F(D3DBLEND_INVSRCCOLOR,MTLBlendFactorOneMinusSourceColor);
        F(D3DBLEND_SRCALPHA,MTLBlendFactorSourceAlpha); F(D3DBLEND_INVSRCALPHA,MTLBlendFactorOneMinusSourceAlpha);
        F(D3DBLEND_DESTALPHA,MTLBlendFactorDestinationAlpha); F(D3DBLEND_INVDESTALPHA,MTLBlendFactorOneMinusDestinationAlpha);
        F(D3DBLEND_DESTCOLOR,MTLBlendFactorDestinationColor); F(D3DBLEND_INVDESTCOLOR,MTLBlendFactorOneMinusDestinationColor);
        F(D3DBLEND_SRCALPHASAT,MTLBlendFactorSourceAlphaSaturated);
        F(D3DBLEND_BLENDFACTOR,MTLBlendFactorBlendColor);
#undef F
        default: fatal("blend", @"unsupported Source blend factor"); return MTLBlendFactorZero;
    }
}
MTLBlendOperation blendOp(unsigned f) {
    switch (f) {
        case D3DBLENDOP_ADD: return MTLBlendOperationAdd;
        case D3DBLENDOP_SUBTRACT: return MTLBlendOperationSubtract;
        case D3DBLENDOP_REVSUBTRACT: return MTLBlendOperationReverseSubtract;
        case D3DBLENDOP_MIN: return MTLBlendOperationMin;
        case D3DBLENDOP_MAX: return MTLBlendOperationMax;
        default: fatal("blend", @"invalid Source blend operation"); return MTLBlendOperationAdd;
    }
}
id<MTLTexture> Texture::renderView(bool gamma) {
    if (!gamma || format.srgb==MTLPixelFormatInvalid) return texture;
    if (!srgbRenderTexture) srgbRenderTexture=[texture newTextureViewWithPixelFormat:format.srgb];
    if (!srgbRenderTexture) fatal("sRGB render view", texture.label);
    return srgbRenderTexture;
}
id<MTLTexture> Texture::view(bool gamma) {
    const auto sw=format.swizzle;
    if(sw.red==MTLTextureSwizzleRed && sw.green==MTLTextureSwizzleGreen && sw.blue==MTLTextureSwizzleBlue && sw.alpha==MTLTextureSwizzleAlpha) return renderView(gamma);
    auto &result=gamma && format.srgb!=MTLPixelFormatInvalid ? srgbTexture:sampleTexture;
    if(!result) {
        result=[texture newTextureViewWithPixelFormat:gamma && format.srgb!=MTLPixelFormatInvalid ? format.srgb:format.linear textureType:texture.textureType levels:NSMakeRange(0,levels) slices:NSMakeRange(0,slices) swizzle:sw];
        if(!result)fatal("sample swizzle view",texture.label);
        result.label=texture.label;
    }
    return result;
}
std::shared_ptr<Texture> newTexture(unsigned w,unsigned h,unsigned depth,unsigned levels,
    unsigned slices,unsigned samples,DWORD usage,D3DFORMAT f,const char *label,bool lockableRenderTarget) {
    @autoreleasepool {
    Format format=pixelFormat(f);
    if (format.linear==MTLPixelFormatInvalid) return {};
    auto image=std::make_shared<Texture>();
    image->format=format; image->d3dFormat=f;
    image->width=w; image->height=h; image->depth=depth; image->slices=slices; image->samples=samples;
    if (!levels) { levels=1; for (unsigned n=std::max({w,h,depth}); n>1; n>>=1) ++levels; }
    image->levels=levels;
    MTLTextureDescriptor *d=[MTLTextureDescriptor new];
    d.width=w; d.height=h; d.depth=depth; d.mipmapLevelCount=levels; d.sampleCount=samples;
    const bool gpuOnlyTarget=(usage&D3DUSAGE_RENDERTARGET) && !(usage&D3DUSAGE_DYNAMIC) && !lockableRenderTarget;
    d.pixelFormat=format.linear; d.storageMode=format.depth || samples>1 || gpuOnlyTarget ? MTLStorageModePrivate : MTLStorageModeShared;
    d.textureType=samples>1 ? MTLTextureType2DMultisample : slices==6 ? MTLTextureTypeCube : depth>1 ? MTLTextureType3D : MTLTextureType2D;
    // Source views only change sRGB interpretation or sampling swizzles,
    // never the component layout. Apple documents that these views need no
    // PixelFormatView usage; that option would prevent lossless compression.
    d.usage=MTLTextureUsageShaderRead;
    if (usage & (D3DUSAGE_RENDERTARGET|D3DUSAGE_DEPTHSTENCIL)) d.usage|=MTLTextureUsageRenderTarget;
    image->texture=[device() newTextureWithDescriptor:d];
    if (!image->texture) fatal("texture allocation", [NSString stringWithFormat:@"%s %ux%ux%u format %d",label ?: "",w,h,depth,f]);
    if (label) image->texture.label=@(label);
    return image;
    } // autoreleasepool
}
HRESULT lockImage(Image &ref,unsigned mip,unsigned face,const D3DBOX *box,DWORD flags,D3DLOCKED_BOX &out) {
    Texture &t=*ref.image;
    if (mip>=t.levels || face>=t.slices || t.format.depth || t.samples!=1 || t.texture.storageMode==MTLStorageModePrivate) return D3DERR_INVALIDCALL;
    auto &l=t.locks[face*t.levels+mip];
    if (l.active) return D3DERR_INVALIDCALL;
    unsigned w=std::max(1u,t.width>>mip),h=std::max(1u,t.height>>mip),depth=std::max(1u,t.depth>>mip);
    l.region=MTLRegionMake3D(box ? box->Left:0,box ? box->Top:0,box ? box->Front:0,
        box ? box->Right-box->Left:w,box ? box->Bottom-box->Top:h,box ? box->Back-box->Front:depth);
    if (l.region.origin.x+l.region.size.width>w || l.region.origin.y+l.region.size.height>h || l.region.origin.z+l.region.size.depth>depth) return D3DERR_INVALIDCALL;
    l.pitch=((l.region.size.width+t.format.block-1)/t.format.block)*t.format.bytes;
    l.imagePitch=l.pitch*((l.region.size.height+t.format.block-1)/t.format.block);
    l.data.resize(l.imagePitch*l.region.size.depth); l.flags=flags; l.active=true;
    if (!(flags&D3DLOCK_DISCARD)) [t.texture getBytes:l.data.data() bytesPerRow:l.pitch bytesPerImage:l.imagePitch fromRegion:l.region mipmapLevel:mip slice:face];
    out.pBits=l.data.data(); out.RowPitch=l.pitch; out.SlicePitch=l.imagePitch;
    return S_OK;
}
HRESULT unlockImage(Image &ref,unsigned mip,unsigned face) {
    Texture &t=*ref.image;
    auto it=t.locks.find(face*t.levels+mip);
    if (it==t.locks.end() || !it->second.active) return D3DERR_INVALIDCALL;
    auto &l=it->second;
    if (!(l.flags&D3DLOCK_READONLY)) [t.texture replaceRegion:l.region mipmapLevel:mip slice:face withBytes:l.data.data() bytesPerRow:l.pitch bytesPerImage:l.imagePitch];
    l.active=false;
    return S_OK;
}
void beginCommand(Device &d) {
    @autoreleasepool {
    // A buffer may return to the upload pool only after all its GPU reads
    // finish. Check completion on the Source render thread, without blocking.
    for(auto it=d.pendingUploads.begin();it!=d.pendingUploads.end();) {
        if(it->command.status==MTLCommandBufferStatusCompleted) {
            for(auto b:it->buffers)if(d.freeUploads.size()<16)d.freeUploads.push_back(b);
            it=d.pendingUploads.erase(it);
        } else ++it;
    }
    if (!d.command) { d.command=[d.queue commandBuffer]; d.command.label=[NSString stringWithFormat:@"Source frame %llu",d.frame]; }
    } // autoreleasepool
}
void endEncoder(Device &d) { @autoreleasepool { if (d.encoder) { [d.encoder endEncoding]; d.encoder=nil; d.visibilityBuffer=nil; d.encoded=EncodedState{}; } } }
void commit(Device &d,bool wait) {
    @autoreleasepool {
    endEncoder(d);
    if (!d.command) { if(wait && d.lastSubmitted)[d.lastSubmitted waitUntilCompleted]; return; }
    id<MTLCommandBuffer> cmd=d.command;
    [cmd addCompletedHandler:^(id<MTLCommandBuffer> c) {
        if (c.status==MTLCommandBufferStatusError) fatal("GPU command",c.error.localizedDescription);
    }];
    [cmd commit];
    if (wait) { [cmd waitUntilCompleted]; if (cmd.error) fatal("GPU wait",cmd.error.localizedDescription); }
    d.lastSubmitted=cmd; d.command=nil;
    if(!d.uploads.empty())d.pendingUploads.push_back(UploadBatch{cmd,std::move(d.uploads)});
    d.uploads.clear(); d.uploadOffset=0;
    } // autoreleasepool
}
id<MTLRenderPipelineState> Device::pipeline(const std::string &key,MTLRenderPipelineDescriptor *descriptor) {
    @autoreleasepool {
    auto found=pipelines.find(key); if(found!=pipelines.end())return found->second;
    NSError *error=nil;
    if(archive) {
        descriptor.binaryArchives=@[archive];
        if(![archive addRenderPipelineFunctionsWithDescriptor:descriptor error:&error])fatal("pipeline archive",error.localizedDescription);
        archiveDirty=true;
    }
    auto result=[device() newRenderPipelineStateWithDescriptor:descriptor error:&error];
    if(!result)fatal("pipeline",[NSString stringWithFormat:@"%@: %@",descriptor.label,error.localizedDescription]);
    pipelines[key]=result; return result;
    } // autoreleasepool
}
void Device::saveArchive() {
    @autoreleasepool {
    if(!archive || !archiveURL || !archiveDirty)return;
    // Archive persistence is an optimization. A failed cache write must not
    // abort a live video-mode reset; the already-created GPU pipelines remain
    // valid. Keep the previous archive until a complete new file is available.
    NSString *suffix=[NSString stringWithFormat:@".%@.tmp",NSUUID.UUID.UUIDString];
    NSError *error=nil; NSURL *temporary=[NSURL fileURLWithPath:[archiveURL.path stringByAppendingString:suffix]];
    if(![archive serializeToURL:temporary error:&error]) {
        Warning("Metal pipeline archive not saved: %s\n",error.localizedDescription.UTF8String ?: "serialization failed");
        [[NSFileManager defaultManager] removeItemAtURL:temporary error:nil]; return;
    }
    if(rename(temporary.path.UTF8String,archiveURL.path.UTF8String)!=0) {
        const int failure=errno;
        Warning("Metal pipeline archive not published: %s; keeping previous cache\n",strerror(failure));
        [[NSFileManager defaultManager] removeItemAtURL:temporary error:nil]; return;
    }
    archiveDirty=false;
    Msg("Metal pipeline archive saved: %s (%zu states)\n",archiveURL.path.UTF8String,pipelines.size());
    } // autoreleasepool
}
id<MTLBuffer> Device::upload(const void *p,unsigned n,NSUInteger &offset) {
    const unsigned capacity=4*1024*1024;
    if (uploads.empty() || uploadOffset+n>uploads.back().length) {
        id<MTLBuffer> b=nil;
        for(auto it=freeUploads.begin();it!=freeUploads.end();++it)if((*it).length>=n) { b=*it; freeUploads.erase(it); break; }
        if(!b)b=[device() newBufferWithLength:std::max(capacity,n) options:MTLResourceStorageModeShared];
        if(!b)fatal("transient buffer",@"out of memory");
        b.label=@"Source transient uniforms and geometry"; uploads.push_back(b); uploadOffset=0;
    }
    offset=uploadOffset; memcpy(static_cast<uint8_t*>(uploads.back().contents)+offset,p,n);
    uploadOffset=(uploadOffset+n+255)&~255u;
    return uploads.back();
}
} // namespace SourceMetal
