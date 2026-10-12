#pragma once
// Source material-system objects backed directly by Metal. Legacy D3D names
// are the engine's existing ABI; no D3D or OpenGL runtime is used here.
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#undef MIN
#undef MAX
#include <SDL.h>
#include <SDL_metal.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <map>
#include <memory>
#include <string>
#include <vector>
// Framework BOOL is an Objective-C bool; Source's D3D ABI requires int.
// Rename only the Source typedef while parsing its headers.
#define BOOL SourceD3DBool
#include "togl/rendermechanism.h"
#include "tier0/threadtools.h"
#include "tier1/strtools.h"
#undef BOOL
#define MOJOSHADER_NO_VERSION_INCLUDE
#include "mojoshader.h"

namespace SourceMetal {
struct Object { virtual ~Object() = default; };
struct Format {
    MTLPixelFormat linear = MTLPixelFormatInvalid;
    MTLPixelFormat srgb = MTLPixelFormatInvalid;
    unsigned bytes = 0, block = 1;
    MTLTextureSwizzleChannels swizzle = {MTLTextureSwizzleRed, MTLTextureSwizzleGreen, MTLTextureSwizzleBlue, MTLTextureSwizzleAlpha};
    bool depth = false, stencil = false;
};
Format pixelFormat(D3DFORMAT f);
MTLVertexFormat vertexFormat(unsigned f);
MTLCompareFunction compare(unsigned f);
MTLStencilOperation stencilOp(unsigned f);
MTLBlendFactor blendFactor(unsigned f);
MTLBlendOperation blendOp(unsigned f);
id<MTLDevice> device();
CAMetalLayer *layer();
void endEncoder(struct Device &d);
void beginCommand(struct Device &d);
void commit(struct Device &d, bool wait);
void fatal(const char *operation, NSString *details);

struct Texture {
    id<MTLTexture> texture = nil, srgbTexture = nil, sampleTexture = nil, srgbRenderTexture = nil;
    Format format;
    D3DFORMAT d3dFormat;
    unsigned width, height, depth, levels, slices, samples;
    bool cpuLightmap=false,cpuShadowValid=false;
    Device *lightmapDevice=nullptr;
    std::vector<uint8_t> cpuShadow;
    std::vector<uint8_t> cpuQueuedShadow;
    struct Lock {
        std::vector<uint8_t> data;
        MTLRegion region;
        unsigned pitch = 0, imagePitch = 0, flags = 0;
        bool active = false,shadow = false;
    };
    std::map<unsigned, Lock> locks;
    id<MTLTexture> view(bool srgb);
    id<MTLTexture> renderView(bool srgb);
};
struct Image : Object {
    std::shared_ptr<Texture> image;
    unsigned face = 0, mip = 0;
};
struct Buffer : Object {
    id<MTLBuffer> buffer = nil;
    unsigned length = 0, lockOffset = 0, lockLength = 0;
    bool locked = false;
};
struct VertexDeclaration : Object { unsigned serial = 0; };
struct Shader : Object {
    const MOJOSHADER_parseData *parsed = nullptr;
    id<MTLLibrary> library = nil;
    id<MTLFunction> function = nil;
    std::string name, label, key, msl;
    unsigned serial = 0, shadowSamplerMask = 0, worldMaterialSamplerMask = 0;
    unsigned floatCount = 0, intCount = 0, boolCount = 0;
    std::vector<MOJOSHADER_uniform> uniforms;
    bool hasColor = false;
    bool vertex = false;
    unsigned maxVertexAttrs = 0;
    std::array<unsigned char,16> vertexAttribMap{};
    std::vector<unsigned> samplerSlots;
    struct Attribute { unsigned reg; int usage,index; };
    std::vector<Attribute> attributes;
    ~Shader() override { MOJOSHADER_freeParseData(parsed); }
};
struct Query : Object {
    struct Result { id<MTLBuffer> buffer = nil; unsigned offset = 0; };
    std::vector<Result> results;
    id<MTLCommandBuffer> command = nil;
    bool active = false, issued = false;
};
struct State {
    std::array<DWORD,211> rs{};
    std::array<std::array<DWORD,14>,20> sampler{};
    float vf[DXABSTRACT_VS_PARAM_SLOTS][4] = {};
    float pf[kGLMProgramParamFloat4Limit][4] = {};
    int vi[16][4] = {}, pi[16][4] = {};
    SourceD3DBool vb[16] = {}, pb[16] = {};
    D3DVIEWPORT9 viewport{};
    RECT scissor{};
};
struct EncodedState {
    bool valid = false, alphaValid = false;
    id<MTLRenderPipelineState> pipeline = nil;
    id<MTLDepthStencilState> depth = nil;
    D3DVIEWPORT9 viewport{};
    MTLScissorRect scissor{};
    std::array<DWORD,211> rs{};
    std::array<id<MTLBuffer>,18> buffers{};
    std::array<NSUInteger,18> offsets{};
    std::array<id<MTLTexture>,20> textures{};
    std::array<id<MTLSamplerState>,20> samplers{};
    float alpha[4] = {};
    std::array<bool,2> uniformValid{};
};
struct SamplerCache {
    float minLOD = 0;
    unsigned maxAnisotropy = 1;
    std::array<DWORD,14> values{};
    bool shadow = false;
    id<MTLSamplerState> result = nil;
};
struct UploadBatch { id<MTLCommandBuffer> command = nil; std::vector<id<MTLBuffer>> buffers; };
struct FrameTiming { double host=0, prepare=0, present=0, acquire=0, gpu=0; };
using DepthKey = std::array<DWORD,17>;
using DrawKey = std::array<DWORD,3+D3D_MAX_STREAMS+8+9+3>;
struct DrawPipeline { id<MTLRenderPipelineState> pipeline = nil; unsigned streamMask = 0; bool defaultAttributes = false; };
struct Device : Object {
    id<MTLCommandQueue> queue = nil;
    id<MTLBuffer> defaultAttributes = nil;
    id<MTLBinaryArchive> archive = nil;
    NSURL *archiveURL = nil;
    bool archiveDirty = false;
    float gamma[256][4] = {};
    bool gammaSet = false;
    id<MTLCommandBuffer> command = nil, lastSubmitted = nil, lastSceneSubmitted = nil;
    id<MTLRenderCommandEncoder> encoder = nil;
    id<MTLBuffer> visibilityBuffer = nil;
    unsigned visibilityOffset = 0;
    std::vector<id<MTLBuffer>> uploads, freeUploads;
    std::vector<UploadBatch> pendingUploads;
    unsigned uploadOffset = 0;
    std::map<std::string,id<MTLRenderPipelineState>> pipelines;
    std::map<DepthKey,id<MTLDepthStencilState>> depthStates;
    DepthKey lastDepthKey{};
    id<MTLDepthStencilState> lastDepth = nil;
    DrawKey lastDrawKey{};
    std::map<DrawKey,DrawPipeline> drawPipelines;
    DrawPipeline lastDrawPipeline;
    std::array<id<MTLSamplerState>,2> copySamplers{};
    std::vector<uint8_t> uniformScratch;
    std::array<std::vector<uint8_t>,2> uniformValues;
    std::map<std::string,id<MTLSamplerState>> samplers;
    State state, saved;
    EncodedState encoded;
    std::array<SamplerCache,20> samplerCache;
    bool savedValid = false;
    ThreadId_t owner = 0;
    Query *activeQuery = nullptr;
    unsigned width = 0, height = 0, samples = 1, renderWidth = 0, renderHeight = 0;
    unsigned long long frame = 0, draws = 0;
    unsigned profileRemaining = 0;
    FrameTiming profileFrame;
    std::vector<FrameTiming> profileSamples;
    std::vector<id<MTLCommandBuffer>> profileSceneCommands;
    std::vector<double> profileSceneGPU;
    id<MTLBuffer> upload(const void *p, unsigned n, NSUInteger &offset);
    id<MTLRenderPipelineState> pipeline(const std::string &key, MTLRenderPipelineDescriptor *descriptor);
    void saveArchive();
};
template<class T> T &native(IUnknown *o) { return *static_cast<T*>(o->m_native); }
std::shared_ptr<Texture> newTexture(unsigned w, unsigned h, unsigned depth,
    unsigned levels, unsigned slices, unsigned samples, DWORD usage,
    D3DFORMAT format, const char *label, bool lockableRenderTarget = false);
HRESULT lockImage(Image &ref, unsigned mip, unsigned face, const D3DBOX *box,
    DWORD flags, D3DLOCKED_BOX &out);
HRESULT unlockImage(Image &ref, unsigned mip, unsigned face);
std::unique_ptr<Shader> newShader(const DWORD *code, const char *name,
    const char *label, const uint32 *centroid = nullptr);
void packUniforms(const Shader &shader, const State &s, std::vector<uint8_t> &data);
} // namespace SourceMetal

// C++ ABI used by SDL launcher, exported by the project's native backend.
TOGL_INTERFACE void *SourceMetalAttachWindow(SDL_Window *window);
TOGL_INTERFACE void SourceMetalDetachWindow(void *view);
TOGL_INTERFACE void SourceMetalResizeWindow(unsigned width, unsigned height);
TOGL_INTERFACE void SourceMetalFillRendererInfo(GLMRendererInfoFields *info);
