#pragma once
// I5-only, bounded observation. Device's sealed C++ layout is unchanged.
namespace SourceMetal {
enum class PerfCount { Passes, Copies, Blits, VisibilityAllocations, Queries,
    ClearDepthAllocations, UploadAllocations, UploadBytes, Waits,
    PipelineMisses, TargetChanges, DepthChanges, GammaChanges, EncoderEnds,
    ColorLoads, ColorStores, DepthLoads, DepthStores, StencilLoads, StencilStores,
    LightmapShadowLocks, LightmapUploads, LightmapUploadBytes, LightmapInvalidations,
    SpatialUpscales, SpatialFallbacks, BufferRenames, BufferAllocations,
    BufferReuses, BufferPreservedBytes, BufferRenameCPUNanoseconds, BufferEvictions,
    InvariantClearReuses, AttachmentPassBreaks, SampleAliasPassBreaks,
    PassDescriptorAllocations, Count };
void perfCount(PerfCount kind, unsigned long long amount=1);
void perfPipelineTime(double milliseconds);
void perfWaitTime(double milliseconds);
void perfPass(MTLRenderPassDescriptor *pass);
void perfBeginFrame(Device &d);
void perfEndFrame(Device &d);
void perfSubmitted(id<MTLCommandBuffer> command);
void perfPresentPhase();
void passEnded();
}
