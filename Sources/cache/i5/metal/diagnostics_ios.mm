#include "native_objects.h"
extern SourceMetal::Device *g_sourceNativeDevice;
extern "C" void SourceMetalWaitForGPU();
extern "C" NSDictionary *SourceMetalResolutionAuditSnapshot();
extern "C" NSDictionary *SourceMetalShaderCacheSnapshot();
extern "C" NSDictionary *SourceMetalPerformanceSnapshot();
extern "C" NSDictionary *SourceMetalCombatSnapshot();
extern "C" NSDictionary *SourceMetalPipelineSnapshot();
extern "C" NSDictionary *SourceMetalSpatialSnapshot();
extern "C" NSDictionary *SourceMetalFrameInterpolationSnapshot();
extern "C" void SourceMetalFrameInterpolationDrain();
extern "C" NSDictionary *SourceMetalBufferSnapshot();
extern "C" NSDictionary *SourceMetalMemorySnapshot();
extern "C" __attribute__((visibility("default"))) NSDictionary *SourceMetalDeviceSnapshot() {
    auto d=g_sourceNativeDevice;
    if(!d)return @{@"attached":@NO};
    return @{@"attached":@YES,@"device":SourceMetal::device().name,@"registry_id":@(SourceMetal::device().registryID),
        @"device_class":NSStringFromClass([(id)SourceMetal::device() class]),@"allocated_bytes":@(SourceMetal::device().currentAllocatedSize),
        @"memory":SourceMetalMemorySnapshot(),@"buffers":SourceMetalBufferSnapshot(),@"frame_interpolation":SourceMetalFrameInterpolationSnapshot(),@"spatial_upscale":SourceMetalSpatialSnapshot(),@"pipeline_cache":SourceMetalPipelineSnapshot(),@"combat":SourceMetalCombatSnapshot(),@"performance":SourceMetalPerformanceSnapshot(),@"resolution_audit":SourceMetalResolutionAuditSnapshot(),@"shader_cache":SourceMetalShaderCacheSnapshot(),@"width":@(d->width),@"height":@(d->height),@"frames":@(d->frame),@"draws":@(d->draws),
        @"pipelines":@(d->pipelines.size()),@"archive_dirty":@(d->archiveDirty),
        @"gamma_set":@(d->gammaSet),@"bc_supported":@(SourceMetal::device().supportsBCTextureCompression),
        @"last_status":@(d->lastSubmitted.status),@"last_error":d->lastSubmitted.error.localizedDescription ?: @""};
}
extern "C" __attribute__((visibility("default"))) NSDictionary *SourceMetalCaptureStart(const char *path) {
    SourceMetalWaitForGPU();
    auto manager=[MTLCaptureManager sharedCaptureManager];
    if(![manager supportsDestination:MTLCaptureDestinationGPUTraceDocument])return @{@"started":@NO,@"error":@"GPUTraceDocument unsupported"};
    auto descriptor=[MTLCaptureDescriptor new];descriptor.captureObject=SourceMetal::device();
    descriptor.destination=MTLCaptureDestinationGPUTraceDocument;descriptor.outputURL=[NSURL fileURLWithPath:@(path)];
    NSError *error=nil;BOOL started=[manager startCaptureWithDescriptor:descriptor error:&error];
    return @{@"started":@(started),@"error":error.localizedDescription ?: @"",@"path":@(path)};
}
extern "C" __attribute__((visibility("default"))) void SourceMetalCaptureStop() {
    SourceMetalWaitForGPU();[[MTLCaptureManager sharedCaptureManager] stopCapture];
}
extern "C" __attribute__((visibility("default"))) void SourceMetalWaitForGPU() {
    if(g_sourceNativeDevice)SourceMetal::commit(*g_sourceNativeDevice,true);
    SourceMetalFrameInterpolationDrain();
}
extern "C" __attribute__((visibility("default"))) void SourceMetalSaveArchive() {
    if(g_sourceNativeDevice)g_sourceNativeDevice->saveArchive();
}
