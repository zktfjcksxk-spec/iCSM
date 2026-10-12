// Experimental native MetalFX frame generation. Source remains capped at 60;
// a bounded presentation worker emits generated, then current rendered frames.
// Ordering follows Apple's WWDC25/211 PresentThread example, with GPU events
// between queues and lifecycle drains. Every displayed frame is audited through
// MTLDrawable.presentedTime, rather than inferred from encode/present calls.
#include "frame_interpolation_ios.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <chrono>
#include <cmath>
#include <mach/mach_time.h>
static ConVar icsm_metal_frame_interpolation("icsm_metal_frame_interpolation","0",
    FCVAR_ARCHIVE|FCVAR_RELEASE,"Experimental MetalFX 60 rendered -> 120 displayed frames",true,0,true,1);
static ConVar icsm_metal_frame_motion("icsm_metal_frame_motion","1",FCVAR_RELEASE,
    "Refine camera reprojection using actual current/previous image block matching",true,0,true,1);
static ConVar icsm_metal_frame_low_latency("icsm_metal_frame_low_latency","1",FCVAR_RELEASE,
    "Bound GPU texture/encoding ownership before polling fresh input",true,0,true,1);
static ConVar icsm_metal_frame_protect_viewmodel("icsm_metal_frame_protect_viewmodel","1",FCVAR_RELEASE,
    "Preserve actual viewmodel animation instead of applying world motion to it",true,0,true,1);
static ConVar icsm_metal_frame_adaptive("icsm_metal_frame_adaptive","1",FCVAR_RELEASE,
    "Temporarily suspend generation when it hurts the native render cadence",true,0,true,1);
static ConVar icsm_metal_frame_compact("icsm_metal_frame_compact","1",FCVAR_RELEASE,
    "Interpolate native scene pixels before spatial expansion; preserve full-resolution UI",true,0,true,1);
static ConVar icsm_metal_frame_scale("icsm_metal_frame_scale","50",FCVAR_RELEASE,
    "Percentage of native scene pixels used by MetalFX interpolation; real Source scene and HUD stay unchanged",true,50,true,100);
static ConVar icsm_metal_frame_color_scale("icsm_metal_frame_color_scale","50",FCVAR_ARCHIVE|FCVAR_RELEASE,
    "Interpolation color detail percentage, independent of motion/depth input density",true,50,true,100);
static ConVar icsm_metal_frame_quality("icsm_metal_frame_quality","1",FCVAR_RELEASE,
    "Reject occluded/ambiguous optical residuals and preserve unclipped camera motion",true,0,true,1);
static ConVar icsm_metal_frame_adaptive_legacy("icsm_metal_frame_adaptive_legacy","0",FCVAR_RELEASE,
    "Diagnostic A/B for the previous sensitive 54Hz suspension/cooldown policy",true,0,true,1);
static ConVar icsm_metal_frame_pacing("icsm_metal_frame_pacing","1",FCVAR_RELEASE,
    "Pace interpolated/real pairs from GPU completion with a timed GPU event; 0 is legacy CPU pacing",true,0,true,1);
static ConVar icsm_metal_frame_fused_ui("icsm_metal_frame_fused_ui","1",FCVAR_RELEASE,
    "Composite native HUD/viewmodel in the existing presentation draw, avoiding a full-screen compute round trip",true,0,true,1);
static bool fgRequested() {
    static ConVarRef limit("icsm_fps_limit",true);
    // The renderer can start before the client registers its FPS preference.
    if(!limit.IsValid())limit.Init("icsm_fps_limit",true);
    return icsm_metal_frame_interpolation.GetBool() && limit.IsValid() && limit.GetInt()==60 &&
        !NSProcessInfo.processInfo.lowPowerModeEnabled;
}
namespace SourceMetal {
struct FGCamera {
    simd_float4x4 reprojection;
    simd_float4 rect;
    simd_uint2 size;
    unsigned reset,pad;
};
struct FGSlot {
    id<MTLTexture> scene=nil,ui=nil,depth=nil,motion=nil,flow=nil,generated=nil;
    id<MTLTexture> luma=nil,mask=nil,maskScratch=nil;
    id<MTLTexture> compactColor=nil,intermediate=nil;
    ICSMInterpolationView view{};
    unsigned long long frame=0,readyValue=0;
    bool busy=false,depthCaptured=false,colorCaptured=false,maskCaptured=false,compactCaptured=false;
    double time=0,inputTime=0,gpuReadyTime=0,cadence=1.0/60.0;
    bool gpuReady=false,gpuFailed=false;
};
struct FGJob {
    std::shared_ptr<FGSlot> slot;
    float gamma[256][4];
    bool interpolate=false;
    bool lowLatency=true;
    bool eventPacing=true,fusedUI=true,protectViewmodel=true;
};
struct FGPaceRequest { unsigned long long value; double deadline; };
struct FGPresentRecord { double time; bool generated; unsigned long long frame; double age,gpuReadyAge; };
struct FGHitchRecord { unsigned long long frame; double time,interval,cpu,gpu; bool paused; };
struct FGState {
    Device *owner=nullptr;
    id<MTLFXFrameInterpolator> effect=nil;
    id<MTLFXSpatialScaler> expansion=nil;
    id<MTLCommandQueue> queue=nil;
    id<MTLSharedEvent> ready=nil;
    id<MTLSharedEvent> pace=nil;
    id<MTLRenderPipelineState> depthPSO=nil,copyPSO=nil,generatedPSO=nil;
    id<MTLComputePipelineState> flowPSO=nil,motionPSO=nil;
    id<MTLComputePipelineState> lumaPSO=nil,maskPSO=nil,dilatePSO=nil,protectPSO=nil;
    id<MTLComputePipelineState> resizePSO=nil;
    id<MTLDepthStencilState> depthState=nil;
    id<MTLCommandBuffer> lastPresent=nil;
    std::array<std::shared_ptr<FGSlot>,3> slots;
    std::shared_ptr<FGSlot> current,previous;
    CAMetalLayer *layer=nil;
    unsigned width=0,height=0,inputWidth=0,inputHeight=0;
    unsigned effectWidth=0,effectHeight=0;
    unsigned nativeWidth=0,nativeHeight=0;
    bool compact=false;
    unsigned long long eventValue=0,encoded=0,rendered=0,resets=0,fallbacks=0;
    bool checked=false,supported=false,stopping=false,working=false;
    unsigned gpuJobs=0,awaitingReal=0,maxAwaitingReal=0;
    unsigned long long gateTimeouts=0,adaptiveSkips=0,adaptiveTransitions=0,lastEffectFrame=0;
    std::atomic<unsigned long long> staleGeneratedSkipped{0};
    unsigned overBudget=0,recoveryFrames=0;
    bool adaptivePaused=false;
    double resumeAfter=0,cadenceEMA=1.0/60.0,inputStarted=0;
    std::atomic<bool> fault{false};
    bool captureRequested=false;
    std::thread thread;
    std::thread pacingThread;
    bool pacingStopping=false;
    unsigned long long paceValue=0;
    std::mutex mutex,statsMutex;
    std::condition_variable cv;
    std::deque<FGJob> jobs;
    std::deque<FGPaceRequest> paceRequests;
    std::deque<FGPresentRecord> presents;
    std::deque<double> renderTimes,gpuTimes;
    std::deque<double> gateWaits;
    std::deque<FGHitchRecord> hitches;
    unsigned long long actualGenerated=0,actualRendered=0,notPresented=0,gpuErrors=0;
    NSString *error=@"";
    ~FGState() {
        {std::lock_guard<std::mutex> lock(mutex);stopping=true;cv.notify_all();}
        if(thread.joinable())thread.join();
        {std::lock_guard<std::mutex> lock(mutex);pacingStopping=true;cv.notify_all();}
        if(pacingThread.joinable())pacingThread.join();
    }
};
static FGState fg;
static simd_float4x4 fgMatrix(const float *m) {
    simd_float4x4 out{};
    for(unsigned c=0;c<4;c++)for(unsigned r=0;r<4;r++)out.columns[c][r]=m[r*4+c];
    return out;
}
static void fgError(NSString *why) {
    std::lock_guard<std::mutex> lock(fg.statsMutex);fg.error=why;
}
static void fgPresented(id<MTLDrawable> drawable,bool generated,unsigned long long frame,double inputTime,double readyTime) {
    if(!generated) {
        std::lock_guard<std::mutex> lock(fg.mutex);
        if(fg.awaitingReal)--fg.awaitingReal;fg.cv.notify_all();
    }
    std::lock_guard<std::mutex> lock(fg.statsMutex);
    const double time=drawable.presentedTime;
    if(time<=0){++fg.notPresented;return;}
    fg.presents.push_back({time,generated,frame,(time-inputTime)*1000,readyTime>inputTime?(readyTime-inputTime)*1000:0});
    if(generated)++fg.actualGenerated;else ++fg.actualRendered;
    while(fg.presents.size()>2400)fg.presents.pop_front();
}
// Apple WWDC25/211 uses a dedicated high-priority timer to release the real
// frame halfway through the GPU-completion cadence. The presentation thread
// can encode both copies immediately; CPU scheduling jitter does not delay
// drawable acquisition, and the second present waits on the GPU event.
static void fgPacingWorker() {
    @autoreleasepool {
    NSThread.currentThread.name=@"iCSM MetalFX pacing";
    NSThread.currentThread.qualityOfService=NSQualityOfServiceUserInteractive;
    mach_timebase_info_data_t info;mach_timebase_info(&info);
    for(;;) {
        FGPaceRequest request;
        {
            std::unique_lock<std::mutex> lock(fg.mutex);
            fg.cv.wait(lock,[]{return fg.pacingStopping || !fg.paceRequests.empty();});
            if(fg.pacingStopping && fg.paceRequests.empty())break;
            request=fg.paceRequests.front();fg.paceRequests.pop_front();
        }
        const double remaining=request.deadline-CACurrentMediaTime();
        if(remaining>0) {
            const uint64_t ticks=uint64_t(remaining*1e9*double(info.denom)/double(info.numer));
            const uint64_t deadline=mach_absolute_time()+ticks;
            while(mach_wait_until(deadline)==KERN_ABORTED){}
        }
        fg.pace.signaledValue=request.value;
    }
    }
}
static void fgWorker() {
    @autoreleasepool {
    NSThread.currentThread.name=@"iCSM MetalFX presentation";
    NSThread.currentThread.qualityOfService=NSQualityOfServiceUserInteractive;
    double deadline=0;
    for(;;) {
        FGJob job;
        {
            std::unique_lock<std::mutex> lock(fg.mutex);
            fg.cv.wait(lock,[]{return fg.stopping || !fg.jobs.empty();});
            if(fg.stopping && fg.jobs.empty())break;
            job=fg.jobs.front();fg.jobs.pop_front();fg.working=true;
        }
        if(job.eventPacing) {
            // A drawable must not occupy the display queue while its Source
            // scene/MetalFX inputs are still being rendered on the other queue.
            std::unique_lock<std::mutex> lock(fg.mutex);
            fg.cv.wait(lock,[&]{return job.slot->gpuReady;});
            if(job.slot->gpuFailed) {
                job.slot->busy=false;if(fg.awaitingReal)--fg.awaitingReal;
                fg.working=false;fg.cv.notify_all();continue;
            }
        }
        const double pairReady=CACurrentMediaTime();
        id<MTLCommandBuffer> lastJobPresent=nil;
        unsigned count=job.interpolate?2:1;
        for(unsigned i=0;i<count;i++)@autoreleasepool {
            bool generated=job.interpolate && i==0;
            const double now=CACurrentMediaTime();
            // Never catch up by emitting a burst of already late frames.
            if(!job.eventPacing) {
                if(deadline<now || deadline>now+.1)deadline=now;
                if(deadline>now)std::this_thread::sleep_for(std::chrono::duration<double>(deadline-now));
            }
            id<CAMetalDrawable> drawable=[fg.layer nextDrawable];
            if(!drawable){
                fgError(@"CAMetalLayer returned no drawable");fg.fault=true;
                if(!generated){std::lock_guard<std::mutex> lock(fg.mutex);if(fg.awaitingReal)--fg.awaitingReal;fg.cv.notify_all();}
                // No later command can release this slot when the final
                // drawable fails. Keep lifecycle drains finite on this path.
                if(i+1==count){
                    // A generated copy may already be using this slot. This
                    // exceptional path has no real-frame completion callback.
                    [lastJobPresent waitUntilCompleted];
                    std::lock_guard<std::mutex> lock(fg.mutex);job.slot->busy=false;fg.cv.notify_all();
                }
                continue;
            }
            // Catch up with current gameplay after a stall instead of feeding
            // two already old images into the drawable queue. This skips the
            // generated display only; it never substitutes/counts a duplicate.
            if(generated && job.lowLatency && CACurrentMediaTime()-job.slot->inputTime>.030) {
                generated=false;count=1;++fg.staleGeneratedSkipped;
            }
            auto command=[fg.queue commandBuffer];
            command.label=generated?@"iCSM present MetalFX generated frame":@"iCSM present real Source frame";
            [command encodeWaitForEvent:fg.ready value:job.slot->readyValue];
            auto pass=[MTLRenderPassDescriptor renderPassDescriptor];
            pass.colorAttachments[0].texture=drawable.texture;
            pass.colorAttachments[0].loadAction=MTLLoadActionDontCare;
            pass.colorAttachments[0].storeAction=MTLStoreActionStore;
            auto encoder=[command renderCommandEncoderWithDescriptor:pass];
            [encoder setRenderPipelineState:generated && job.fusedUI?fg.generatedPSO:fg.copyPSO];
            [encoder setFragmentTexture:generated?job.slot->generated:job.slot->ui atIndex:0];
            [encoder setFragmentBytes:job.gamma length:sizeof(job.gamma) atIndex:0];
            if(generated && job.fusedUI) {
                [encoder setFragmentTexture:job.slot->ui atIndex:1];
                [encoder setFragmentTexture:job.slot->scene atIndex:2];
                [encoder setFragmentTexture:job.slot->mask atIndex:3];
                [encoder setFragmentTexture:job.slot->maskScratch atIndex:4];
                const unsigned protect=job.protectViewmodel?1:0;
                [encoder setFragmentBytes:&protect length:sizeof(protect) atIndex:1];
            }
            [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
            [encoder endEncoding];
            // Existing touch renderer explicitly supports a rendering thread:
            // it uses cached UIKit geometry, a mutex, and asynchronous main-
            // thread host refresh. Keep its original native drawable size and
            // draw after gamma, on both real/generated frames. Never put touch
            // geometry into the smaller logical-resolution interpolation UI.
            SourceTouchRenderMetal((__bridge void*)command,(__bridge void*)drawable.texture);
            const auto frame=job.slot->frame;
            const auto inputTime=job.slot->inputTime;
            const double readyTime=job.eventPacing?job.slot->gpuReadyTime:0;
            [drawable addPresentedHandler:^(id<MTLDrawable> shown){fgPresented(shown,generated,frame,inputTime,readyTime);}];
            if(job.eventPacing && i==1) {
                // Match Apple's 31/64 completion interval, leaving a small
                // scheduling margin before the half-frame display boundary.
                const double half=std::min(1.0/30.0,std::max(1.0/60.0,job.slot->cadence))*31.0/64.0;
                unsigned long long value;
                {
                    std::lock_guard<std::mutex> lock(fg.mutex);
                    value=++fg.paceValue;fg.paceRequests.push_back({value,pairReady+half});fg.cv.notify_all();
                }
                [command encodeWaitForEvent:fg.pace value:value];
            }
            [command presentDrawable:drawable afterMinimumDuration:1.0/120.0];
            [command addCompletedHandler:^(id<MTLCommandBuffer> done){
                if(done.error){
                    {std::lock_guard<std::mutex> lock(fg.statsMutex);++fg.gpuErrors;fg.error=done.error.description;}
                    fg.fault=true;
                }
            }];
            if(i+1==count) {
                const auto slot=job.slot;
                {std::lock_guard<std::mutex> lock(fg.mutex);++fg.gpuJobs;}
                // Release texture ownership on completion, rather than stopping
                // the presentation thread while the GPU finishes the copies.
                [command addCompletedHandler:^(id<MTLCommandBuffer> done){
                    std::lock_guard<std::mutex> lock(fg.mutex);
                    slot->busy=false;--fg.gpuJobs;fg.cv.notify_all();
                }];
            }
            [command commit];fg.lastPresent=lastJobPresent=command;
            if(!job.eventPacing)deadline+=1.0/120.0;
        }
        if(!job.lowLatency)[fg.lastPresent waitUntilCompleted]; // explicit A/B only
        {
            std::lock_guard<std::mutex> lock(fg.mutex);
            fg.working=false;fg.cv.notify_all();
        }
    }
    }
}
static void fgDrain() {
    if(!fg.thread.joinable())return;
    std::unique_lock<std::mutex> lock(fg.mutex);
    fg.cv.wait(lock,[]{return fg.jobs.empty() && !fg.working && fg.gpuJobs==0;});
    lock.unlock();
    fg.previous.reset();
    if(fg.current){fg.current->busy=false;fg.current.reset();}
}
static void fgForget(Device &d) {
    if(fg.owner!=&d)return;
    fgDrain();fg.effect=nil;fg.expansion=nil;fg.current.reset();fg.previous.reset();
    for(auto &slot:fg.slots)slot.reset();
    fg.owner=nullptr;fg.width=fg.height=0;
}
static id<MTLTexture> fgTexture(MTLPixelFormat format,unsigned w,unsigned h,MTLTextureUsage usage,NSString *label) {
    auto desc=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format width:w height:h mipmapped:NO];
    desc.storageMode=MTLStorageModePrivate;desc.usage=usage;
    auto result=[device() newTextureWithDescriptor:desc];result.label=label;return result;
}
static bool fgInitialize(Device &d,unsigned w,unsigned h,unsigned nativeW,unsigned nativeH) {
    const float scale=icsm_metal_frame_compact.GetBool()?icsm_metal_frame_scale.GetFloat()*.01f:1.f;
    const unsigned iw=std::max(1u,unsigned(lroundf(nativeW*scale))),ih=std::max(1u,unsigned(lroundf(nativeH*scale)));
    const float colorScale=icsm_metal_frame_color_scale.GetFloat()*.01f;
    const unsigned ew=icsm_metal_frame_compact.GetBool()?std::max(1u,unsigned(lroundf(nativeW*colorScale))):w;
    const unsigned eh=icsm_metal_frame_compact.GetBool()?std::max(1u,unsigned(lroundf(nativeH*colorScale))):h;
    if(!fg.checked) {
        fg.checked=true;
        if(@available(iOS 26.0,*))fg.supported=[MTLFXFrameInterpolatorDescriptor supportsDevice:device()];
    }
    if(!fg.supported){fgError(@"Native MetalFX frame interpolation unsupported");return false;}
    if(fg.owner==&d && fg.layer==layer() && fg.width==w && fg.height==h && fg.inputWidth==iw && fg.inputHeight==ih && fg.effectWidth==ew && fg.effectHeight==eh && fg.nativeWidth==nativeW && fg.nativeHeight==nativeH && fg.compact==icsm_metal_frame_compact.GetBool())
        return fg.effect && (fg.expansion || (fg.effectWidth==w && fg.effectHeight==h)) && fg.depthPSO && fg.copyPSO && fg.generatedPSO && fg.flowPSO && fg.motionPSO && fg.lumaPSO && fg.maskPSO && fg.dilatePSO && fg.protectPSO && fg.resizePSO;
    fgDrain();fg.owner=&d;fg.width=w;fg.height=h;fg.inputWidth=iw;fg.inputHeight=ih;fg.layer=layer();
    fg.nativeWidth=nativeW;fg.nativeHeight=nativeH;
    fg.compact=icsm_metal_frame_compact.GetBool();fg.effectWidth=ew;fg.effectHeight=eh;
    // Connect the framework's virtual controller on the existing main thread.
    SourceTouchInitialize((__bridge void*)device());
    auto desc=[MTLFXFrameInterpolatorDescriptor new];
    desc.inputWidth=iw;desc.inputHeight=ih;desc.outputWidth=fg.effectWidth;desc.outputHeight=fg.effectHeight;
    desc.colorTextureFormat=desc.outputTextureFormat=desc.uiTextureFormat=MTLPixelFormatBGRA8Unorm;
    desc.depthTextureFormat=MTLPixelFormatDepth32Float;desc.motionTextureFormat=MTLPixelFormatRG16Float;
    fg.effect=[desc newFrameInterpolatorWithDevice:device()];
    if(!fg.effect){fgError(@"MetalFX factory failed at the selected resolution");return false;}
    fg.expansion=nil;
    if(fg.effectWidth!=w || fg.effectHeight!=h) {
        auto expand=[MTLFXSpatialScalerDescriptor new];
        expand.inputWidth=fg.effectWidth;expand.inputHeight=fg.effectHeight;expand.outputWidth=w;expand.outputHeight=h;
        expand.colorTextureFormat=expand.outputTextureFormat=MTLPixelFormatBGRA8Unorm;
        expand.colorProcessingMode=MTLFXSpatialScalerColorProcessingModePerceptual;
        fg.expansion=[expand newSpatialScalerWithDevice:device()];
        if(!fg.expansion){fgError(@"Generated scene spatial expansion factory failed");return false;}
    }
    if(!fg.copyPSO) {
        NSError *error=nil;
        auto library=[device() newLibraryWithSource:icsmFrameMotionSource options:nil error:&error];
        if(!library){fgError(error.description);return false;}
        auto pipeline=[MTLRenderPipelineDescriptor new];
        pipeline.vertexFunction=[library newFunctionWithName:@"fg_quad"];
        pipeline.fragmentFunction=[library newFunctionWithName:@"fg_depth"];
        pipeline.depthAttachmentPixelFormat=MTLPixelFormatDepth32Float;
        fg.depthPSO=[device() newRenderPipelineStateWithDescriptor:pipeline error:&error];
        pipeline.depthAttachmentPixelFormat=MTLPixelFormatInvalid;
        pipeline.colorAttachments[0].pixelFormat=fg.layer.pixelFormat;
        pipeline.fragmentFunction=[library newFunctionWithName:@"fg_copy"];
        fg.copyPSO=[device() newRenderPipelineStateWithDescriptor:pipeline error:&error];
        pipeline.fragmentFunction=[library newFunctionWithName:@"fg_present_generated"];
        fg.generatedPSO=[device() newRenderPipelineStateWithDescriptor:pipeline error:&error];
        fg.flowPSO=[device() newComputePipelineStateWithFunction:[library newFunctionWithName:@"fg_flow"] error:&error];
        fg.motionPSO=[device() newComputePipelineStateWithFunction:[library newFunctionWithName:@"fg_motion"] error:&error];
        fg.lumaPSO=[device() newComputePipelineStateWithFunction:[library newFunctionWithName:@"fg_luma"] error:&error];
        fg.maskPSO=[device() newComputePipelineStateWithFunction:[library newFunctionWithName:@"fg_viewmodel_mask"] error:&error];
        fg.dilatePSO=[device() newComputePipelineStateWithFunction:[library newFunctionWithName:@"fg_dilate"] error:&error];
        fg.protectPSO=[device() newComputePipelineStateWithFunction:[library newFunctionWithName:@"fg_protect_viewmodel"] error:&error];
        fg.resizePSO=[device() newComputePipelineStateWithFunction:[library newFunctionWithName:@"fg_resize"] error:&error];
        auto depth=[MTLDepthStencilDescriptor new];depth.depthCompareFunction=MTLCompareFunctionAlways;depth.depthWriteEnabled=YES;
        fg.depthState=[device() newDepthStencilStateWithDescriptor:depth];
        if(!fg.depthPSO || !fg.copyPSO || !fg.generatedPSO || !fg.flowPSO || !fg.motionPSO || !fg.lumaPSO || !fg.maskPSO || !fg.dilatePSO || !fg.protectPSO || !fg.resizePSO){fgError(error.description?:@"Frame generation pipeline creation failed");return false;}
    }
    for(auto &slot:fg.slots) {
        slot=std::make_shared<FGSlot>();
        slot->scene=fgTexture(desc.colorTextureFormat,w,h,fg.effect.colorTextureUsage|MTLTextureUsageShaderRead,@"MetalFX HUD-less real scene");
        slot->ui=fgTexture(desc.uiTextureFormat,w,h,fg.effect.uiTextureUsage|MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead,@"MetalFX full Source UI and touch controls");
        slot->depth=fgTexture(desc.depthTextureFormat,iw,ih,fg.effect.depthTextureUsage|MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead,@"MetalFX real world depth");
        slot->motion=fgTexture(desc.motionTextureFormat,iw,ih,fg.effect.motionTextureUsage|MTLTextureUsageShaderWrite,@"MetalFX current-to-previous pixel motion");
        slot->flow=fgTexture(MTLPixelFormatRGBA16Float,(iw+15)/16,(ih+15)/16,MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite,@"iCSM optical motion residual estimate and confidence");
        slot->generated=fgTexture(desc.outputTextureFormat,w,h,(fg.expansion?fg.expansion.outputTextureUsage:fg.effect.outputTextureUsage)|MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite,@"Spatially expanded generated scene with native UI");
        slot->intermediate=fg.expansion?fgTexture(desc.outputTextureFormat,fg.effectWidth,fg.effectHeight,fg.effect.outputTextureUsage|fg.expansion.colorTextureUsage|MTLTextureUsageShaderRead,@"MetalFX actual native-resolution intermediate frame"):slot->generated;
        slot->compactColor=fg.compact?fgTexture(desc.colorTextureFormat,fg.effectWidth,fg.effectHeight,fg.effect.colorTextureUsage|MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite,@"Exact scene pixels before original spatial expansion"):slot->scene;
        slot->luma=fgTexture(MTLPixelFormatR8Unorm,(iw+1)/2,(ih+1)/2,MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite,@"Compact optical motion luminance");
        slot->mask=fgTexture(MTLPixelFormatR8Unorm,iw,ih,MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite,@"Actual viewmodel coverage with guarded edges");
        slot->maskScratch=fgTexture(MTLPixelFormatR8Unorm,iw,ih,MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite,@"Viewmodel mask scratch");
        if(!slot->scene || !slot->ui || !slot->depth || !slot->motion || !slot->flow || !slot->generated || !slot->luma || !slot->mask || !slot->maskScratch || !slot->compactColor || !slot->intermediate){fgError(@"Frame generation texture allocation failed");return false;}
    }
    if(!fg.queue){fg.queue=[device() newCommandQueue];fg.queue.label=@"iCSM bounded MetalFX present queue";fg.ready=[device() newSharedEvent];fg.pace=[device() newSharedEvent];}
    if(!fg.pacingThread.joinable())fg.pacingThread=std::thread(fgPacingWorker);
    if(!fg.thread.joinable())fg.thread=std::thread(fgWorker);
    fgError(@"");return true;
}
static void fgDepth(Device &d,id<MTLTexture> source,const ICSMInterpolationView &view) {
    if(!fgRequested())return;
    if(fg.fault){icsm_metal_frame_interpolation.SetValue(0);return;}
    if(!source || source.sampleCount!=1 || source.textureType!=MTLTextureType2D ||
       view.width<=0 || view.height<=0 || view.x<0 || view.y<0 ||
       unsigned(view.x+view.width)>source.width || unsigned(view.y+view.height)>source.height) {
        ++fg.fallbacks;fgError(@"World depth/viewport ineligible; single-sample rendering required");return;
    }
    if(!fgInitialize(d,view.outputWidth,view.outputHeight,view.width,view.height)){++fg.fallbacks;return;}
    if(fg.current && fg.current->frame==d.frame)return; // nested overlay view
    std::shared_ptr<FGSlot> slot;
    {
        std::unique_lock<std::mutex> lock(fg.mutex);
        fg.cv.wait(lock,[&]{for(auto &s:fg.slots)if(!s->busy && s!=fg.previous){slot=s;return true;}return false;});
        slot->busy=true;
        slot->gpuReady=slot->gpuFailed=false;
    }
    fg.current=slot;slot->view=view;slot->frame=d.frame;slot->time=CACurrentMediaTime();
    slot->inputTime=fg.inputStarted>0?fg.inputStarted:slot->time;
    slot->depthCaptured=slot->colorCaptured=slot->maskCaptured=slot->compactCaptured=false;
    FGCamera camera{};camera.size={fg.inputWidth,fg.inputHeight};
    camera.rect={view.x/float(source.width),view.y/float(source.height),view.width/float(source.width),view.height/float(source.height)};
    endEncoder(d);beginCommand(d);
    auto pass=[MTLRenderPassDescriptor renderPassDescriptor];
    pass.depthAttachment.texture=slot->depth;pass.depthAttachment.loadAction=MTLLoadActionDontCare;pass.depthAttachment.storeAction=MTLStoreActionStore;
    auto encoder=[d.command renderCommandEncoderWithDescriptor:pass];encoder.label=@"iCSM capture actual world depth before viewmodel";
    [encoder setRenderPipelineState:fg.depthPSO];[encoder setDepthStencilState:fg.depthState];
    [encoder setFragmentTexture:source atIndex:0];[encoder setFragmentBytes:&camera length:sizeof(camera) atIndex:0];
    [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];[encoder endEncoding];
    slot->depthCaptured=true;
}
static void fgBlit(Device &d,id<MTLTexture> source,id<MTLTexture> dest,unsigned x=0,unsigned y=0) {
    endEncoder(d);beginCommand(d);auto encoder=[d.command blitCommandEncoder];
    [encoder copyFromTexture:source sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(x,y,0)
         sourceSize:MTLSizeMake(dest.width,dest.height,1) toTexture:dest destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];
    [encoder endEncoding];
}
static void fgColor(Device &d,id<MTLTexture> source) {
    auto slot=fg.current;
    if(!fgRequested() || !slot || slot->frame!=d.frame || !slot->depthCaptured || slot->colorCaptured)return;
    if(!source || source.sampleCount!=1 || source.pixelFormat!=MTLPixelFormatBGRA8Unorm ||
       source.width<fg.width || source.height<fg.height){++fg.fallbacks;fgError(@"HUD-less color target ineligible");return;}
    fgBlit(d,source,slot->scene);slot->colorCaptured=true;
}
static void fgCompactColor(Device &d,id<MTLTexture> source,const ICSMSceneRect &rect) {
    auto slot=fg.current;
    if(!fgRequested() || !fg.compact || !slot || slot->frame!=d.frame || slot->compactCaptured || !source ||
       rect.width!=int(fg.nativeWidth) || rect.height!=int(fg.nativeHeight) || rect.x<0 || rect.y<0 ||
       unsigned(rect.x+rect.width)>source.width || unsigned(rect.y+rect.height)>source.height ||
       source.pixelFormat!=MTLPixelFormatBGRA8Unorm)return;
    if(rect.width==int(fg.effectWidth) && rect.height==int(fg.effectHeight))fgBlit(d,source,slot->compactColor,rect.x,rect.y);
    else {
        endEncoder(d);beginCommand(d);
        auto encoder=[d.command computeCommandEncoder];encoder.label=@"Downsample exact native scene rectangle for bounded interpolation";
        FGCamera camera{};camera.rect={rect.x/float(source.width),rect.y/float(source.height),rect.width/float(source.width),rect.height/float(source.height)};
        [encoder setTexture:source atIndex:0];[encoder setTexture:slot->compactColor atIndex:1];
        [encoder setBytes:&camera length:sizeof(camera) atIndex:0];
        [encoder setComputePipelineState:fg.resizePSO];
        [encoder dispatchThreads:MTLSizeMake(fg.effectWidth,fg.effectHeight,1) threadsPerThreadgroup:MTLSizeMake(8,8,1)];
        [encoder endEncoding];
    }
    slot->compactCaptured=true;
}
static void fgDispatch(id<MTLComputeCommandEncoder> encoder,id<MTLComputePipelineState> pipeline,unsigned w,unsigned h) {
    [encoder setComputePipelineState:pipeline];
    [encoder dispatchThreads:MTLSizeMake(w,h,1) threadsPerThreadgroup:MTLSizeMake(8,8,1)];
}
static void fgViewmodel(Device &d,id<MTLTexture> source) {
    auto slot=fg.current;
    if(!slot || slot->frame!=d.frame || !source || !slot->depthCaptured)return;
    const auto &view=slot->view;
    FGCamera camera{};camera.size={fg.inputWidth,fg.inputHeight};
    camera.rect={view.x/float(source.width),view.y/float(source.height),view.width/float(source.width),view.height/float(source.height)};
    endEncoder(d);beginCommand(d);
    auto encoder=[d.command computeCommandEncoder];encoder.label=@"Exact viewmodel depth coverage and edge guard";
    [encoder setTexture:slot->depth atIndex:0];[encoder setTexture:source atIndex:1];[encoder setTexture:slot->mask atIndex:2];
    [encoder setBytes:&camera length:sizeof(camera) atIndex:0];
    fgDispatch(encoder,fg.maskPSO,fg.inputWidth,fg.inputHeight);
    [encoder memoryBarrierWithScope:MTLBarrierScopeTextures];
    unsigned vertical=0;
    [encoder setTexture:slot->mask atIndex:0];[encoder setTexture:slot->maskScratch atIndex:1];
    [encoder setBytes:&vertical length:sizeof(vertical) atIndex:0];
    fgDispatch(encoder,fg.dilatePSO,fg.inputWidth,fg.inputHeight);
    [encoder memoryBarrierWithScope:MTLBarrierScopeTextures];
    vertical=1;
    [encoder setTexture:slot->maskScratch atIndex:0];[encoder setTexture:slot->mask atIndex:1];
    [encoder setBytes:&vertical length:sizeof(vertical) atIndex:0];
    fgDispatch(encoder,fg.dilatePSO,fg.inputWidth,fg.inputHeight);[encoder endEncoding];
    slot->maskCaptured=true;
}
#include "frame_interpolation_capture.inc"
static bool fgPresent(Device &d,id<MTLTexture> source) {
    auto slot=fg.current;
    if(!fgRequested() || fg.fault || !slot || slot->frame!=d.frame || !slot->colorCaptured || !slot->depthCaptured || !slot->maskCaptured) {
        fgDrain();return false;
    }
    if(source.width!=fg.width || source.height!=fg.height || source.pixelFormat!=MTLPixelFormatBGRA8Unorm){++fg.fallbacks;fgDrain();return false;}
    fgBlit(d,source,slot->ui);
    auto previous=fg.previous?fg.previous:slot;
    const double delta=slot->time-previous->time;
    // Sample at the same frame-start boundary; varying world-render duration
    // must not look like a change to the engine's actual native cadence.
    const double cadence=slot->inputTime-previous->inputTime;
    slot->cadence=cadence>.001 && cadence<.1?cadence:1.0/60.0;
    const double simulationDelta=slot->view.simulationTime-previous->view.simulationTime;
    float cameraTravelSquared=0;
    for(unsigned i=0;i<3;i++){const float step=slot->view.cameraOrigin[i]-previous->view.cameraOrigin[i];cameraTravelSquared+=step*step;}
    bool reset=previous==slot || delta<.001 || delta>.1 || cameraTravelSquared>128.f*128.f ||
        fabsf(slot->view.verticalFOV-previous->view.verticalFOV)>15;
    if(icsm_metal_frame_quality.GetBool()) {
        // A discontinuous spectator/teleport camera cut invalidates history.
        // Use the real Source view forward basis, independent of map names.
        float directionDot=0,oldLength=0,newLength=0;
        for(unsigned i=0;i<3;i++) {
            const float old=previous->view.worldToView[8+i],now=slot->view.worldToView[8+i];
            directionDot+=old*now;oldLength+=old*old;newLength+=now*now;
        }
        if(oldLength>0 && newLength>0 && directionDot/sqrtf(oldLength*newLength)<.70710678f)reset=true;
    }
    if(cadence>.001 && cadence<.1)fg.cadenceEMA=fg.cadenceEMA*.9+cadence*.1;
    if(!icsm_metal_frame_adaptive.GetBool()){fg.adaptivePaused=false;fg.overBudget=fg.recoveryFrames=0;}
    else if(!reset) {
        // Use measured native cadence, not the sum of overlapping command-buffer
        // GPU durations. Sustained missed 60Hz deadlines temporarily remove FX
        // workload. Hysteresis/cooldown prevents frame-by-frame mode oscillation.
        if(!fg.adaptivePaused) {
            const bool legacy=icsm_metal_frame_adaptive_legacy.GetBool();
            // 55-59 real FPS can still produce useful 110-118 unique displays.
            // Do not force a multi-second 60Hz dip for a small deadline miss.
            fg.overBudget=fg.cadenceEMA>(legacy?.0185:.0205)?fg.overBudget+1:0;
            if(fg.overBudget>=12){fg.adaptivePaused=true;fg.resumeAfter=slot->time+(legacy?2:.5);fg.recoveryFrames=0;++fg.adaptiveTransitions;}
        } else {
            fg.recoveryFrames=fg.cadenceEMA<.0175?fg.recoveryFrames+1:0;
            if(slot->time>fg.resumeAfter && fg.recoveryFrames>=(icsm_metal_frame_adaptive_legacy.GetBool()?90:30)){fg.adaptivePaused=false;fg.overBudget=0;++fg.adaptiveTransitions;}
        }
    }
    bool encodeEffect=!fg.adaptivePaused;
    // A slow CPU frame already missed the budget; avoid adding a generation
    // workload and two-display-frame queue to that transient stall.
    if(icsm_metal_frame_adaptive.GetBool() && slot->time-slot->inputTime>.025)encodeEffect=false;
    if(!encodeEffect)++fg.adaptiveSkips;
    // Internal MetalFX history is invalid after a skipped encode. Prime it with
    // a real-only frame before resuming interpolation; never count a duplicate.
    reset=reset || fg.lastEffectFrame+1!=slot->frame;
    bool interpolate=encodeEffect && !reset;
    if(encodeEffect) {
    FGCamera camera{};camera.size={fg.inputWidth,fg.inputHeight};camera.reset=reset;
    camera.pad=icsm_metal_frame_quality.GetBool()?1:0;
    camera.reprojection=simd_mul(fgMatrix(previous->view.worldToClip),simd_inverse(fgMatrix(slot->view.worldToClip)));
    const unsigned refine=icsm_metal_frame_motion.GetBool()?1:0;
    auto encoder=[d.command computeCommandEncoder];encoder.label=@"iCSM depth camera reprojection and image motion estimate";
    if(!slot->compactCaptured && slot->compactColor!=slot->scene) {
        // At 100% scene resolution, or if the optional Source spatial hook was
        // bypassed, derive a correctly sized input without guessing a viewport.
        [encoder setTexture:slot->scene atIndex:0];[encoder setTexture:slot->compactColor atIndex:1];
        camera.rect={0,0,1,1};[encoder setBytes:&camera length:sizeof(camera) atIndex:0];
        fgDispatch(encoder,fg.resizePSO,fg.effectWidth,fg.effectHeight);
        [encoder memoryBarrierWithScope:MTLBarrierScopeTextures];
    }
    [encoder setTexture:slot->compactColor atIndex:0];[encoder setTexture:slot->luma atIndex:1];
    fgDispatch(encoder,fg.lumaPSO,slot->luma.width,slot->luma.height);
    [encoder memoryBarrierWithScope:MTLBarrierScopeTextures];
    if(refine) {
        [encoder setTexture:slot->luma atIndex:0];[encoder setTexture:previous->luma atIndex:1];
        [encoder setTexture:slot->depth atIndex:2];[encoder setTexture:slot->flow atIndex:3];
        [encoder setTexture:slot->mask atIndex:4];[encoder setTexture:previous->mask atIndex:5];
        [encoder setTexture:previous->depth atIndex:6];
        [encoder setBytes:&camera length:sizeof(camera) atIndex:0];
        fgDispatch(encoder,fg.flowPSO,slot->flow.width,slot->flow.height);
        [encoder memoryBarrierWithScope:MTLBarrierScopeTextures];
    }
    [encoder setTexture:slot->depth atIndex:0];[encoder setTexture:slot->flow atIndex:1];[encoder setTexture:slot->motion atIndex:2];
    [encoder setTexture:slot->luma atIndex:3];[encoder setTexture:previous->luma atIndex:4];
    [encoder setTexture:slot->mask atIndex:5];[encoder setTexture:previous->mask atIndex:6];
    [encoder setTexture:previous->depth atIndex:7];
    [encoder setBytes:&camera length:sizeof(camera) atIndex:0];[encoder setBytes:&refine length:sizeof(refine) atIndex:1];
    fgDispatch(encoder,fg.motionPSO,fg.inputWidth,fg.inputHeight);[encoder endEncoding];
    auto effect=fg.effect;
    effect.colorTexture=slot->compactColor;effect.prevColorTexture=previous->compactColor;
    effect.uiTexture=nil;effect.uiTextureComposited=NO;
    effect.depthTexture=slot->depth;effect.motionTexture=slot->motion;effect.outputTexture=slot->intermediate;
    // Keep the factory's full-region defaults. Color/UI are output-sized;
    // depth/motion are input-sized, as specified by the descriptor contract.
    effect.motionVectorScaleX=float(fg.effectWidth)/fg.inputWidth;
    effect.motionVectorScaleY=float(fg.effectHeight)/fg.inputHeight;effect.depthReversed=NO;
    effect.deltaTime=reset?1.f/60:float(simulationDelta>.001 && simulationDelta<.1?simulationDelta:delta);effect.shouldResetHistory=reset;
    effect.jitterOffsetX=effect.jitterOffsetY=0;
    effect.nearPlane=slot->view.nearPlane;effect.farPlane=slot->view.farPlane;
    effect.fieldOfView=slot->view.verticalFOV;effect.aspectRatio=slot->view.aspect;
    if(@available(iOS 27.0,*)) {
        effect.worldToViewMatrix=fgMatrix(slot->view.worldToView);
        effect.viewToClipMatrix=fgMatrix(slot->view.viewToClip);
    }
    [effect encodeToCommandBuffer:d.command];
    if(fg.expansion) {
        fg.expansion.colorTexture=slot->intermediate;fg.expansion.outputTexture=slot->generated;
        fg.expansion.inputContentWidth=fg.effectWidth;fg.expansion.inputContentHeight=fg.effectHeight;
        [fg.expansion encodeToCommandBuffer:d.command];
    }
    if(!icsm_metal_frame_fused_ui.GetBool() || fg.captureRequested) {
        encoder=[d.command computeCommandEncoder];encoder.label=@"Keep actual weapon animation on generated frame";
        [encoder setTexture:slot->generated atIndex:0];[encoder setTexture:slot->ui atIndex:1];
        [encoder setTexture:slot->mask atIndex:2];[encoder setTexture:previous->mask atIndex:3];
        [encoder setTexture:slot->scene atIndex:4];const unsigned protect=icsm_metal_frame_protect_viewmodel.GetBool()?1:0;
        [encoder setBytes:&protect length:sizeof(protect) atIndex:0];
        fgDispatch(encoder,fg.protectPSO,fg.width,fg.height);[encoder endEncoding];
    }
    fg.lastEffectFrame=slot->frame;
    }
    if(interpolate && icsm_metal_frame_fused_ui.GetBool()) {
        // The old horizontal-dilation scratch is dead after mask construction.
        // Preserve the small previous coverage plane here on the render queue,
        // instead of retaining an entire previous slot until the second
        // presentation finishes on the other queue. Source input stays fresh
        // without a cross-queue read/write race or an additional ring stall.
        fgBlit(d,previous->mask,slot->maskScratch);
    }
    if(fg.captureRequested && encodeEffect && !reset) {fg.captureRequested=false;fgCapture(d,slot,previous);}
    slot->readyValue=++fg.eventValue;
    [d.command encodeSignalEvent:fg.ready value:slot->readyValue];
    const auto measuredFrame=slot->frame;
    const double measuredStart=slot->inputTime,measuredCPU=(slot->time-slot->inputTime)*1000;
    const bool measuredPaused=fg.adaptivePaused;
    [d.command addCompletedHandler:^(id<MTLCommandBuffer> done){
        {
            std::lock_guard<std::mutex> queueLock(fg.mutex);
            slot->gpuReadyTime=done.GPUEndTime;slot->gpuFailed=done.error!=nil;slot->gpuReady=true;fg.cv.notify_all();
        }
        std::lock_guard<std::mutex> lock(fg.statsMutex);
        if(done.error){++fg.gpuErrors;fg.error=done.error.description;fg.fault=true;}
        else if(encodeEffect && done.GPUEndTime>done.GPUStartTime){fg.gpuTimes.push_back((done.GPUEndTime-done.GPUStartTime)*1000);while(fg.gpuTimes.size()>1200)fg.gpuTimes.pop_front();}
        if(cadence>.023 || measuredCPU>16 || (done.GPUEndTime-measuredStart)>.030) {
            fg.hitches.push_back({measuredFrame,measuredStart,cadence*1000,measuredCPU,(done.GPUEndTime-done.GPUStartTime)*1000,measuredPaused});
            while(fg.hitches.size()>96)fg.hitches.pop_front();
        }
    }];
    commit(d,false);if(encodeEffect)++fg.encoded;++fg.rendered;if(reset && encodeEffect)++fg.resets;
    {
        std::lock_guard<std::mutex> lock(fg.statsMutex);fg.renderTimes.push_back(slot->time);while(fg.renderTimes.size()>1200)fg.renderTimes.pop_front();
    }
    FGJob job;job.slot=slot;job.interpolate=interpolate;job.lowLatency=icsm_metal_frame_low_latency.GetBool();
    job.eventPacing=icsm_metal_frame_pacing.GetBool();job.fusedUI=icsm_metal_frame_fused_ui.GetBool();
    job.protectViewmodel=icsm_metal_frame_protect_viewmodel.GetBool();
    for(unsigned i=0;i<256;i++)for(unsigned j=0;j<4;j++)job.gamma[i][j]=d.gammaSet?d.gamma[i][j]:i/255.f;
    {
        std::lock_guard<std::mutex> lock(fg.mutex);
        fg.jobs.push_back(job);++fg.awaitingReal;
        fg.maxAwaitingReal=std::max(fg.maxAwaitingReal,fg.awaitingReal);fg.cv.notify_all();
    }
    fg.previous=slot;fg.current.reset();return true;
}
static NSDictionary *fgSnapshot(bool includeRecords=false) {
    std::lock_guard<std::mutex> lock(fg.statsMutex);
    double renderedFPS=0,presentedFPS=0,gpuMean=0;
    if(fg.renderTimes.size()>1)renderedFPS=(fg.renderTimes.size()-1)/(fg.renderTimes.back()-fg.renderTimes.front());
    if(fg.presents.size()>1)presentedFPS=(fg.presents.size()-1)/(fg.presents.back().time-fg.presents.front().time);
    for(auto value:fg.gpuTimes)gpuMean+=value;if(!fg.gpuTimes.empty())gpuMean/=fg.gpuTimes.size();
    auto percentile=[](std::vector<double> values,double p) {if(values.empty())return 0.0;std::sort(values.begin(),values.end());return values[size_t((values.size()-1)*p)];};
    std::vector<double> ages,readyAges,displayAges;
    for(auto &r:fg.presents)if(!r.generated) {
        ages.push_back(r.age);
        if(r.gpuReadyAge>0){readyAges.push_back(r.gpuReadyAge);displayAges.push_back(r.age-r.gpuReadyAge);}
    }
    std::vector<double> timestamps,intervals;
    for(auto &r:fg.presents)timestamps.push_back(r.time);
    std::sort(timestamps.begin(),timestamps.end());
    NSMutableDictionary *histogram=[NSMutableDictionary dictionary];
    for(size_t i=1;i<timestamps.size();++i) {
        const double interval=(timestamps[i]-timestamps[i-1])*1000;
        intervals.push_back(interval);
        NSString *bucket=[NSString stringWithFormat:@"%ld",lround(interval*120.0/1000.0)];
        histogram[bucket]=@([histogram[bucket] unsignedLongLongValue]+1);
    }
    std::vector<double> gate(fg.gateWaits.begin(),fg.gateWaits.end());
    unsigned awaitingReal,maxAwaitingReal,gpuJobs;unsigned long long gateTimeouts;
    {std::lock_guard<std::mutex> queueLock(fg.mutex);awaitingReal=fg.awaitingReal;maxAwaitingReal=fg.maxAwaitingReal;gpuJobs=fg.gpuJobs;gateTimeouts=fg.gateTimeouts;}
    NSMutableDictionary *result=[@{@"requested":@(icsm_metal_frame_interpolation.GetBool()),@"enabled":@(fgRequested() && !fg.fault),@"supported":@(fg.supported),
        @"resolution":@[@(fg.width),@(fg.height)],@"motion_resolution":@[@(fg.inputWidth),@(fg.inputHeight)],@"encoded":@(fg.encoded),@"real_submitted":@(fg.rendered),
        @"interpolation_resolution":@[@(fg.effectWidth),@(fg.effectHeight)],@"compact_scene":@(fg.compact),
        @"native_scene_resolution":@[@(fg.nativeWidth),@(fg.nativeHeight)],@"interpolation_scale":@(icsm_metal_frame_scale.GetFloat()),
        @"color_detail_scale":@(icsm_metal_frame_color_scale.GetFloat()),@"quality_motion_guard":@(icsm_metal_frame_quality.GetBool()),
        @"history_resets":@(fg.resets),@"fallbacks":@(fg.fallbacks),@"render_fps":@(renderedFPS),
        @"presented_fps":@(presentedFPS),@"generated_actually_presented":@(fg.actualGenerated),
        @"rendered_actually_presented":@(fg.actualRendered),@"not_presented":@(fg.notPresented),
        @"generation_gpu_ms_mean":@(gpuMean),@"render_command_buffer_gpu_ms_mean":@(gpuMean),
        @"gpu_duration_scope":@"Entire render command buffer containing MetalFX; not isolated interpolation cost",
        @"gpu_errors":@(fg.gpuErrors),@"error":fg.error?:@"",
        @"motion":icsm_metal_frame_motion.GetBool()?@"Real world depth/camera reprojection + compact-luma image motion estimates; actual viewmodel coverage protected":@"Camera-only reprojection; actual viewmodel coverage protected",
        @"viewmodel_protected":@(icsm_metal_frame_protect_viewmodel.GetBool()),@"viewmodel_animation_fps":@(renderedFPS),
        @"low_latency":@(icsm_metal_frame_low_latency.GetBool()),
        @"event_pacing":@(icsm_metal_frame_pacing.GetBool()),@"fused_ui_present":@(icsm_metal_frame_fused_ui.GetBool()),
        @"display_interval_ms_p50":@(percentile(intervals,.5)),@"display_interval_ms_p95":@(percentile(intervals,.95)),
        @"display_interval_120hz_buckets":histogram,
        @"source_frame_start_to_real_present_ms_p50":@(percentile(ages,.5)),@"source_frame_start_to_real_present_ms_p95":@(percentile(ages,.95)),
        @"source_frame_start_to_gpu_ready_ms_p50":@(percentile(readyAges,.5)),@"gpu_ready_to_real_present_ms_p50":@(percentile(displayAges,.5)),
        @"pre_input_wait_ms_p95":@(percentile(gate,.95)),@"adaptive_paused":@(fg.adaptivePaused),
        @"awaiting_real_present":@(awaitingReal),@"max_awaiting_real_present":@(maxAwaitingReal),@"gpu_jobs":@(gpuJobs),@"pre_input_wait_timeouts":@(gateTimeouts),
        @"stale_generated_display_skipped":@(fg.staleGeneratedSkipped.load()),
        @"adaptive_skipped_frames":@(fg.adaptiveSkips),@"adaptive_transitions":@(fg.adaptiveTransitions),@"native_cadence_ms_ema":@(fg.cadenceEMA*1000),
        @"measurement":@"MTLDrawable.presentedTime and addPresentedHandler; render cap is unchanged"} mutableCopy];
    NSMutableArray *hitches=[NSMutableArray array];
    for(const auto &h:fg.hitches)[hitches addObject:@{@"frame":@(h.frame),@"time":@(h.time),@"native_interval_ms":@(h.interval),@"cpu_to_world_depth_ms":@(h.cpu),@"command_buffer_gpu_elapsed_ms":@(h.gpu),@"adaptive_paused":@(h.paused)}];
    result[@"recent_hitches"]=hitches;
    if(includeRecords) {
        NSMutableArray *records=[NSMutableArray array];
        for(auto &r:fg.presents)[records addObject:@{@"time":@(r.time),@"generated":@(r.generated),@"source_frame":@(r.frame),@"source_age_ms":@(r.age),@"gpu_ready_age_ms":@(r.gpuReadyAge)}];
        result[@"presentations"]=records;
        NSMutableArray *times=[NSMutableArray array];for(double time:fg.renderTimes)[times addObject:@(time)];result[@"render_times"]=times;
    }
    return result;
}
// Called before Source's message pump/input polling. Queueing after input was
// sampled is what made the old ring-buffer throttle add stale-input latency.
extern "C" __attribute__((visibility("default"))) void SourceMetalFrameInterpolationPrepareInput() {
    using namespace SourceMetal;
    if(!fgRequested() || !icsm_metal_frame_low_latency.GetBool() || !fg.owner || fg.fault)return;
    const auto start=CACurrentMediaTime();
    {
        std::unique_lock<std::mutex> lock(fg.mutex);
        if(!fg.cv.wait_for(lock,std::chrono::milliseconds(25),[]{
            if(fg.fault.load() || fg.stopping)return true;
            // Follow Apple's bounded GPU/encoding-queue ownership, rather
            // than blocking on addPresentedHandler callbacks, whose delivery
            // is asynchronous and can lag the actual display. Move the old
            // texture-ownership throttle ahead of fresh input polling.
            if(fg.jobs.size()>=2)return false;
            for(auto &slot:fg.slots)if(slot && !slot->busy && slot!=fg.previous)return true;
            return false;
        }))++fg.gateTimeouts;
    }
    std::lock_guard<std::mutex> lock(fg.statsMutex);
    fg.gateWaits.push_back((CACurrentMediaTime()-start)*1000);while(fg.gateWaits.size()>1200)fg.gateWaits.pop_front();
}
extern "C" __attribute__((visibility("default"))) void SourceMetalFrameInterpolationFrameStart() {
    SourceMetal::fg.inputStarted=CACurrentMediaTime();
}
}
extern "C" __attribute__((visibility("default"))) void SourceMetalFrameInterpolationDrain() { SourceMetal::fgDrain(); }
extern "C" __attribute__((visibility("default"))) NSDictionary *SourceMetalFrameInterpolationSnapshot() { return SourceMetal::fgSnapshot(); }
extern "C" __attribute__((visibility("default"))) bool SourceMetalFrameInterpolationActive() {return fgRequested() && !SourceMetal::fg.fault;}
CON_COMMAND(icsm_metal_frame_interpolation_reset_stats,"Reset actual rendered/generated presentation measurements") {
    std::lock_guard<std::mutex> lock(SourceMetal::fg.statsMutex);auto &s=SourceMetal::fg;
    s.presents.clear();s.renderTimes.clear();s.gpuTimes.clear();s.actualGenerated=s.actualRendered=s.notPresented=s.gpuErrors=0;
    s.gateWaits.clear();
}
CON_COMMAND(icsm_metal_frame_interpolation_report,"Save measured render and actual display timestamps under Documents/I5") {
    NSMutableDictionary *report=[SourceMetal::fgSnapshot(true) mutableCopy];
    NSString *tag=@"gameplay";
    if(args.ArgC()>1) {
        NSString *candidate=[NSString stringWithUTF8String:args[1]];
        NSCharacterSet *valid=[NSCharacterSet characterSetWithCharactersInString:@"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-"];
        if(candidate.length && candidate.length<96 && [candidate rangeOfCharacterFromSet:valid.invertedSet].location==NSNotFound)tag=candidate;
    }
    report[@"tag"]=tag;report[@"timestamp"]=@([[NSDate date] timeIntervalSince1970]);
    auto data=[NSJSONSerialization dataWithJSONObject:report options:NSJSONWritingPrettyPrinted error:nil];
    NSString *relative=[NSString stringWithFormat:@"Documents/I5/frame-interpolation-%@.json",tag];
    [data writeToFile:[NSHomeDirectory() stringByAppendingPathComponent:relative] atomically:YES];
    Msg("ICSM_FRAME_GENERATION render=%.2f display=%.2f real=%llu generated=%llu errors=%llu\n",
        [report[@"render_fps"] doubleValue],[report[@"presented_fps"] doubleValue],
        [report[@"rendered_actually_presented"] unsignedLongLongValue],
        [report[@"generated_actually_presented"] unsignedLongLongValue],[report[@"gpu_errors"] unsignedLongLongValue]);
}
CON_COMMAND(icsm_metal_frame_interpolation_capture,"Read back one aligned previous/current/generated game frame and real motion/depth") {
    SourceMetal::fg.captureRequested=true;
}
