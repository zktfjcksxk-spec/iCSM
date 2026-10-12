#pragma once
#include <algorithm>
#include <cstdint>
#include <mutex>
#include <vector>

// CPU scratch only: MTLTexture.replaceRegion copies these bytes immediately.
// Never put a GPU-visible upload buffer or an active texture lock in this pool.
class ICSMTextureStagingPool {
    std::mutex mutex;
    std::vector<std::vector<uint8_t>> free;
    size_t retained=0;
    uint64_t reused=0,allocated=0;
public:
    static constexpr size_t byteLimit=8*1024*1024,countLimit=16;
    void acquire(std::vector<uint8_t>& data,size_t size) {
        std::lock_guard<std::mutex> lock(mutex);
        auto best=free.end();
        for(auto it=free.begin();it!=free.end();++it)
            if(it->capacity()>=size && (best==free.end() || it->capacity()<best->capacity()))best=it;
        if(best!=free.end()) {retained-=best->capacity();data=std::move(*best);free.erase(best);++reused;}
        else ++allocated;
        data.resize(size);
    }
    void recycle(std::vector<uint8_t>& data) {
        std::lock_guard<std::mutex> lock(mutex);
        const size_t bytes=data.capacity();
        if(bytes && bytes<=byteLimit && retained+bytes<=byteLimit && free.size()<countLimit) {
            data.clear();retained+=bytes;free.push_back(std::move(data));
        }
        std::vector<uint8_t>().swap(data);
    }
    void trim() {std::lock_guard<std::mutex> lock(mutex);free.clear();retained=0;}
    struct Stats {size_t bytes,count;uint64_t reuses,allocations;};
    Stats stats() {std::lock_guard<std::mutex> lock(mutex);return {retained,free.size(),reused,allocated};}
};
