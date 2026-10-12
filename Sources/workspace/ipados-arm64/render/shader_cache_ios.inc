// I5-only cache loading. Close our file before Metal retains the library data.
#include <dispatch/dispatch.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <unistd.h>
#include <cerrno>
#include <cstdlib>
#include <climits>

static uint64_t shaderCacheDataLoads=0,shaderCacheSourceCompiles=0;
static uint64_t shaderCacheFallbacks=0,shaderCacheLibraryCount=0;
static uint64_t shaderCacheReleasedTextBytes=0,shaderCacheCompactedShaders=0;
static NSDictionary *shaderCacheFileSnapshot=@{};

static id<MTLLibrary> loadShaderLibraryData(NSString *path,NSError **error) {
    int fd=open(path.fileSystemRepresentation,O_RDONLY|O_CLOEXEC);
    int failure=fd<0?errno:0;
    struct stat info={};
    if(!failure && fstat(fd,&info)!=0)failure=errno;
    if(!failure && (!S_ISREG(info.st_mode) || info.st_size<=0 ||
                   uint64_t(info.st_size)>SIZE_MAX))failure=EINVAL;
    const size_t length=failure?0:size_t(info.st_size);
    void *bytes=failure?nullptr:malloc(length);
    if(!failure && !bytes)failure=ENOMEM;
    size_t offset=0;
    while(!failure && offset<length) {
        ssize_t count=read(fd,static_cast<char*>(bytes)+offset,length-offset);
        if(count>0)offset+=size_t(count);
        else if(count<0 && errno==EINTR)continue;
        else failure=count<0?errno:EIO;
    }
    // This descriptor is never passed to Metal or retained by a cached library.
    if(fd>=0 && close(fd)!=0 && !failure)failure=errno;
    if(failure) {
        free(bytes);
        if(error)*error=[NSError errorWithDomain:NSPOSIXErrorDomain code:failure
                                        userInfo:@{NSFilePathErrorKey:path}];
        return nil;
    }
    // Apple's dispatch/data.h transfers malloc storage to the FREE destructor.
    // Metal may retain the dispatch data; its lifetime no longer owns a file.
    dispatch_data_t data=dispatch_data_create(bytes,length,nullptr,DISPATCH_DATA_DESTRUCTOR_FREE);
    return [SourceMetal::device() newLibraryWithData:data error:error];
}

extern "C" __attribute__((visibility("default"))) NSDictionary *SourceMetalShaderCacheSnapshot() {
    return @{@"loader":@"closed-file / dispatch-data",@"offline_data_loads":@(shaderCacheDataLoads),
             @"source_compiles":@(shaderCacheSourceCompiles),@"cache_fallbacks":@(shaderCacheFallbacks),
             @"retained_libraries":@(shaderCacheLibraryCount),@"file_snapshot":shaderCacheFileSnapshot,
             @"compacted_shaders":@(shaderCacheCompactedShaders),@"compiler_text_bytes_released":@(shaderCacheReleasedTextBytes)};
}

CON_COMMAND(icsm_metal_cache_stats,"Print shader cache loads and a one-shot open-file count") {
    struct rlimit limit={};
    if(getrlimit(RLIMIT_NOFILE,&limit)!=0){Warning("ICSM_SHADER_CACHE_STATS getrlimit failed\n");return;}
    const rlim_t bound=limit.rlim_cur==RLIM_INFINITY?rlim_t(getdtablesize()):limit.rlim_cur;
    uint64_t count=0,cacheLibraries=0,pipelineArchives=0;
    for(rlim_t fd=0;fd<bound && fd<=INT_MAX;++fd) {
        if(fcntl(int(fd),F_GETFD)==-1)continue;
        ++count;
        char path[PATH_MAX]={};
        if(fcntl(int(fd),F_GETPATH,path)==0 && strstr(path,"SourceMetal-I5") && strstr(path,".metallib")) {
            if(strstr(path,"pipelines-"))++pipelineArchives;
            else ++cacheLibraries;
        }
    }
    shaderCacheFileSnapshot=@{@"open_files":@(count),@"soft_limit":@(limit.rlim_cur),
        @"scanned_descriptors":@(bound),@"shader_library_files":@(cacheLibraries),
        @"pipeline_archive_files":@(pipelineArchives)};
    NSData *json=[NSJSONSerialization dataWithJSONObject:SourceMetalShaderCacheSnapshot() options:0 error:nil];
    Msg("ICSM_SHADER_CACHE_STATS %s\n",[[NSString alloc] initWithData:json encoding:NSUTF8StringEncoding].UTF8String);
}
