#include "texture_staging_pool.h"
static ConVar icsm_metal_texture_staging_pool("icsm_metal_texture_staging_pool","1",FCVAR_RELEASE,
    "Reuse a bounded CPU texture-lock scratch pool instead of retaining a duplicate per texture",true,0,true,1);
static ICSMTextureStagingPool textureStaging;
static std::vector<std::weak_ptr<Texture>> textureMemoryRegistry;
static uint64_t textureTrimmedBytes=0,textureTrims=0;
static std::atomic<bool> textureMemoryWarning{false};
static void registerTextureMemory(const std::shared_ptr<Texture>& texture) {
    if(textureMemoryRegistry.size()%256==0)
        textureMemoryRegistry.erase(std::remove_if(textureMemoryRegistry.begin(),textureMemoryRegistry.end(),
            [](const auto& item){return item.expired();}),textureMemoryRegistry.end());
    textureMemoryRegistry.push_back(texture);
}
static void trimTextureScratch() {
    for(auto it=textureMemoryRegistry.begin();it!=textureMemoryRegistry.end();) {
        auto texture=it->lock();if(!texture){it=textureMemoryRegistry.erase(it);continue;}
        for(auto& entry:texture->locks)if(!entry.second.active) {
            textureTrimmedBytes+=entry.second.data.capacity();
            std::vector<uint8_t>().swap(entry.second.data);
        }
        ++it;
    }
    textureStaging.trim();++textureTrims;
}
static void watchTextureMemoryWarnings() {
    static id observer=[NSNotificationCenter.defaultCenter addObserverForName:UIApplicationDidReceiveMemoryWarningNotification
        object:nil queue:nil usingBlock:^(NSNotification*){textureMemoryWarning.store(true);}];
    (void)observer;
}
extern "C" __attribute__((visibility("default"))) NSDictionary *SourceMetalMemorySnapshot() {
    auto s=textureStaging.stats();
    return @{@"texture_scratch_limit_bytes":@(ICSMTextureStagingPool::byteLimit),@"texture_scratch_bytes":@(s.bytes),
        @"texture_scratch_buffers":@(s.count),@"texture_scratch_reuses":@(s.reuses),@"texture_scratch_allocations":@(s.allocations),
        @"texture_scratch_pool":@(icsm_metal_texture_staging_pool.GetBool()),@"inactive_texture_bytes_released":@(textureTrimmedBytes),
        @"trims":@(textureTrims)};
}
CON_COMMAND(icsm_memory_trim,"Release inactive CPU texture scratch on the Source owner thread") {trimTextureScratch();}
