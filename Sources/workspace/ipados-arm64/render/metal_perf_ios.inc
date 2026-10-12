#include "metal_perf_ios.h"
#include <cctype>
static ConVar icsm_metal_pass_reuse("icsm_metal_pass_reuse","1",FCVAR_RELEASE,"I5 compatible Metal pass reuse; diagnostic A/B switch");
static ConVar icsm_metal_temporary_reuse("icsm_metal_temporary_reuse","1",FCVAR_RELEASE,"I5 lazy query buffers and cached clear states; diagnostic A/B switch");
static ConVar icsm_metal_touch_pass_merge("icsm_metal_touch_pass_merge","1",FCVAR_RELEASE,"Draw touch controls in the final presentation pass; diagnostic A/B switch");
static ConVar icsm_metal_buffer_reuse("icsm_metal_buffer_reuse","1",FCVAR_RELEASE,"Reuse completed vertex/index rename buffers; diagnostic A/B switch");
static ConVar icsm_metal_invariant_clear_reuse("icsm_metal_invariant_clear_reuse","1",FCVAR_RELEASE,
    "Keep the active Metal sRGB target view for exact endpoint clear colors");
static ConVar icsm_metal_pass_descriptor_reuse("icsm_metal_pass_descriptor_reuse","1",FCVAR_RELEASE,
    "Reuse Source render-pass descriptors after encoding, without retaining map textures");
static ConVar icsm_metal_csm_discard_dummy("icsm_metal_csm_discard_dummy","0",FCVAR_ARCHIVE | FCVAR_RELEASE,
    "Discard unused CSM dummy color attachment contents; shadow depth remains stored",true,0,true,1);
namespace SourceMetal {
struct PerfRow {
    FrameTiming timing{};
    double interval=0, sceneGPU=-1, presentGPU=-1, pipelineMS=0, waitMS=0;
    unsigned long long frame=0, draws=0;
    unsigned thermal=0;
    std::array<unsigned long long,static_cast<unsigned>(PerfCount::Count)> counts{};
};
struct PerfPending { id<MTLCommandBuffer> command=nil; size_t row=0; bool scene=false; };
static bool perfActive=false;
static bool perfInFrame=false, perfPresentation=false;
static std::string perfTag;
static std::vector<PerfRow> perfRows;
static std::vector<PerfPending> perfPending;
static PerfRow perfCurrent;
static double perfLastStart=0, perfBegan=0;
static unsigned long long perfStartDraws=0;
static NSDictionary *perfLastSummary=@{};
static const char *perfNames[]={"passes","copies","blits","visibility_allocations","queries",
    "clear_depth_allocations","upload_allocations","upload_bytes","waits","pipeline_misses",
    "target_changes","depth_changes","gamma_changes","encoder_ends","color_loads","color_stores",
    "depth_loads","depth_stores","stencil_loads","stencil_stores",
    "lightmap_shadow_locks","lightmap_uploads","lightmap_upload_bytes","lightmap_invalidations","spatial_upscales","spatial_fallbacks",
    "buffer_renames","buffer_allocations","buffer_reuses","buffer_preserved_bytes","buffer_rename_cpu_ns","buffer_evictions",
    "invariant_clear_reuses","attachment_pass_breaks","sample_alias_pass_breaks","pass_descriptor_allocations"};
static_assert(sizeof(perfNames)/sizeof(perfNames[0])==static_cast<unsigned>(PerfCount::Count),"Performance counter names must match their enum");
void perfCount(PerfCount kind,unsigned long long amount) {
    if(perfActive && perfInFrame)perfCurrent.counts[static_cast<unsigned>(kind)]+=amount;
}
void perfPipelineTime(double ms) { if(perfActive && perfInFrame)perfCurrent.pipelineMS+=ms; }
void perfWaitTime(double ms) { if(perfActive && perfInFrame)perfCurrent.waitMS+=ms; }
void perfSubmitted(id<MTLCommandBuffer> command) {
    if(perfActive && perfInFrame)perfPending.push_back({command,perfRows.size(),!perfPresentation});
}
void perfPresentPhase() { if(perfActive && perfInFrame)perfPresentation=true; }
void perfPass(MTLRenderPassDescriptor *p) {
    if(!perfActive)return;
    perfCount(PerfCount::Passes);
    for(unsigned i=0;i<4;++i)if(p.colorAttachments[i].texture) {
        if(p.colorAttachments[i].loadAction==MTLLoadActionLoad)perfCount(PerfCount::ColorLoads);
        if(p.colorAttachments[i].storeAction==MTLStoreActionStore)perfCount(PerfCount::ColorStores);
    }
    if(p.depthAttachment.texture) {
        if(p.depthAttachment.loadAction==MTLLoadActionLoad)perfCount(PerfCount::DepthLoads);
        if(p.depthAttachment.storeAction==MTLStoreActionStore)perfCount(PerfCount::DepthStores);
    }
    if(p.stencilAttachment.texture) {
        if(p.stencilAttachment.loadAction==MTLLoadActionLoad)perfCount(PerfCount::StencilLoads);
        if(p.stencilAttachment.storeAction==MTLStoreActionStore)perfCount(PerfCount::StencilStores);
    }
}
static void perfResolve() {
    for(auto it=perfPending.begin();it!=perfPending.end();) {
        if(it->command.status==MTLCommandBufferStatusCompleted) {
            double ms=(it->command.GPUEndTime-it->command.GPUStartTime)*1000;
            if(ms>0) { auto &r=perfRows[it->row]; auto &value=it->scene?r.sceneGPU:r.presentGPU; if(value<0)value=0;value+=ms; }
            it=perfPending.erase(it);
        }else ++it;
    }
}
void perfBeginFrame(Device &d) {
    if(!perfActive)return;
    const double now=Plat_FloatTime();
    perfCurrent=PerfRow{}; perfCurrent.frame=d.frame;
    perfInFrame=true;perfPresentation=false;
    if(perfLastStart)perfCurrent.interval=(now-perfLastStart)*1000;
    perfLastStart=now;perfStartDraws=d.draws;
    perfCurrent.thermal=NSProcessInfo.processInfo.thermalState;
    perfResolve();
}
static NSDictionary *perfDistribution(std::vector<double> values) {
    if(values.empty())return @{};
    std::sort(values.begin(),values.end());double sum=0;for(auto v:values)sum+=v;
    size_t n=values.size();
    return @{@"samples":@(n),@"mean":@(sum/n),@"p50":@(values[n/2]),
        @"p95":@(values[(n-1)*95/100]),@"p99":@(values[(n-1)*99/100]),@"max":@(values.back())};
}
static void perfFinish(Device &d) {
    perfResolve();
    NSMutableDictionary *metrics=[NSMutableDictionary dictionary];
    const char *names[]={"host_ms","prepare_ms","present_cpu_ms","drawable_acquire_ms","frame_interval_ms","scene_gpu_ms","present_gpu_ms","pipeline_ms","wait_ms","draws"};
    for(unsigned k=0;k<10;++k) {
        std::vector<double> values;values.reserve(perfRows.size());
        for(auto &r:perfRows) {
            double v[]={r.timing.host,r.timing.prepare,r.timing.present,r.timing.acquire,r.interval,r.sceneGPU,r.presentGPU,r.pipelineMS,r.waitMS,double(r.draws)};
            if(v[k]>=0 && (k!=4 || v[k]>0))values.push_back(v[k]);
        }
        metrics[@(names[k])]=perfDistribution(std::move(values));
    }
    NSMutableDictionary *counts=[NSMutableDictionary dictionary];
    for(unsigned k=0;k<static_cast<unsigned>(PerfCount::Count);++k) {
        std::vector<double> values;for(auto &r:perfRows)values.push_back(r.counts[k]);
        counts[@(perfNames[k])]=perfDistribution(std::move(values));
    }
    unsigned maxThermal=0;for(auto &r:perfRows)maxThermal=std::max(maxThermal,r.thermal);
    perfLastSummary=@{@"tag":@(perfTag.c_str()),@"frames":@(perfRows.size()),@"metrics":metrics,@"counts":counts,
        @"device":device().name,@"thermal_start":@(perfRows.front().thermal),@"thermal_end":@(perfRows.back().thermal),@"thermal_max":@(maxThermal),
        @"elapsed_seconds":@(Plat_FloatTime()-perfBegan),@"gpu_pending":@(perfPending.size()),
        @"gpu_measurement":@"sum of completed command buffers per engine frame, including mid-frame submissions; presentation includes touch overlay",
        @"render_size":@[@(d.width),@(d.height)],@"fps_max":@(ConVarRef("fps_max").GetFloat()),
        @"scene_render_percent":@(ConVarRef("icsm_scene_render_percent").GetFloat()),
        @"pass_reuse":@(ConVarRef("icsm_metal_pass_reuse").GetBool()),@"temporary_reuse":@(ConVarRef("icsm_metal_temporary_reuse").GetBool()),
        @"buffer_reuse":@(icsm_metal_buffer_reuse.GetBool()),
        @"timestamp":[[NSISO8601DateFormatter new] stringFromDate:NSDate.date]};
    NSString *directory=[NSHomeDirectory() stringByAppendingPathComponent:@"Documents/I5/performance"];
    [[NSFileManager defaultManager] createDirectoryAtPath:directory withIntermediateDirectories:YES attributes:nil error:nil];
    NSString *base=[directory stringByAppendingPathComponent:@(perfTag.c_str())];
    NSError *error=nil;
    bool jsonOK=[[NSJSONSerialization dataWithJSONObject:perfLastSummary options:NSJSONWritingPrettyPrinted error:&error] writeToFile:[base stringByAppendingString:@".json"] atomically:YES];
    std::ostringstream csv;csv<<"frame,host_ms,prepare_ms,present_cpu_ms,acquire_ms,interval_ms,scene_gpu_ms,present_gpu_ms,pipeline_ms,wait_ms,draws,thermal";
    for(auto n:perfNames)csv<<','<<n;csv<<'\n';
    for(auto &r:perfRows) {
        csv<<r.frame<<','<<r.timing.host<<','<<r.timing.prepare<<','<<r.timing.present<<','<<r.timing.acquire<<','<<r.interval<<','<<r.sceneGPU<<','<<r.presentGPU<<','<<r.pipelineMS<<','<<r.waitMS<<','<<r.draws<<','<<r.thermal;
        for(auto v:r.counts)csv<<','<<v;csv<<'\n';
    }
    bool csvOK=[@(csv.str().c_str()) writeToFile:[base stringByAppendingString:@".csv"] atomically:YES encoding:NSUTF8StringEncoding error:&error];
    Msg("ICSM_PERF saved tag=%s frames=%zu json=%d csv=%d thermal=%u/%u pending=%zu\n",perfTag.c_str(),perfRows.size(),jsonOK,csvOK,perfRows.front().thermal,maxThermal,perfPending.size());
    if(!jsonOK || !csvOK)Warning("ICSM_PERF write failed: %s\n",error.localizedDescription.UTF8String ?: "unknown");
    perfActive=false;perfInFrame=false;perfPending.clear();perfRows.clear();perfRows.shrink_to_fit();
}
void perfEndFrame(Device &d) {
    if(!perfActive)return;
    perfCurrent.timing=d.profileFrame;perfCurrent.draws=d.draws-perfStartDraws;
    perfRows.push_back(perfCurrent);perfInFrame=false;
    if(d.profileRemaining==1)perfFinish(d);
}
}
extern "C" __attribute__((visibility("default"))) NSDictionary *SourceMetalPerformanceSnapshot() {
    return @{@"active":@(SourceMetal::perfActive),@"tag":@(SourceMetal::perfTag.c_str()),@"last":SourceMetal::perfLastSummary};
}
CON_COMMAND(icsm_metal_profile,"Bounded CPU/GPU/pass/allocation observation: icsm_metal_profile [frames] [safe-tag]") {
    auto d=g_sourceNativeDevice;if(!d)return;
    std::string tag=args.ArgC()>2 ? args[2]:"observation";
    if(tag.empty() || tag.size()>64 || !std::all_of(tag.begin(),tag.end(),[](unsigned char c){return std::isalnum(c) || c=='_' || c=='-';})) {Warning("ICSM_PERF invalid tag\n");return;}
    SourceMetal::perfActive=false;SourceMetal::perfInFrame=false;SourceMetal::perfRows.clear();SourceMetal::perfPending.clear();
    d->profileSamples.clear();d->profileSceneCommands.clear();d->profileSceneGPU.clear();
    d->profileRemaining=std::max(60,std::min(18000,args.ArgC()>1 ? atoi(args[1]):900));
    d->profileSamples.reserve(d->profileRemaining);SourceMetal::perfRows.reserve(d->profileRemaining);
    SourceMetal::perfLastStart=0;SourceMetal::perfBegan=Plat_FloatTime();SourceMetal::perfTag=tag;SourceMetal::perfActive=true;
    Msg("ICSM_PERF started tag=%s count=%u\n",tag.c_str(),d->profileRemaining);
}
