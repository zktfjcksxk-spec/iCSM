#include "texture_staging_pool.h"
#include <cassert>
#include <thread>

int main() {
    ICSMTextureStagingPool pool;
    std::vector<std::vector<uint8_t>> held(20);
    for(auto& bytes:held) {pool.acquire(bytes,1024*1024);std::fill(bytes.begin(),bytes.end(),19);}
    for(auto& bytes:held) {assert(bytes.front()==19);pool.recycle(bytes);assert(bytes.capacity()==0);}
    auto s=pool.stats();assert(s.bytes<=pool.byteLimit && s.count<=pool.countLimit);
    auto worker=[&](unsigned seed) {
        for(unsigned n=0;n<150;++n) {
            std::vector<uint8_t> a,b;pool.acquire(a,1037+n*17);pool.acquire(b,4000+n*3);
            std::fill(a.begin(),a.end(),seed);std::fill(b.begin(),b.end(),seed+1);
            assert(a.back()==seed && b.back()==seed+1);pool.recycle(a);pool.recycle(b);
        }
    };
    std::thread a(worker,5),b(worker,9);a.join();b.join();
    std::vector<uint8_t> large;pool.acquire(large,32*1024*1024);pool.recycle(large);
    assert(large.capacity()==0 && pool.stats().bytes<=pool.byteLimit);
    assert(pool.stats().reuses>0);pool.trim();assert(pool.stats().bytes==0 && pool.stats().count==0);
}
