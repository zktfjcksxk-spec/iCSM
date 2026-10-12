#include "vaudio/ivaudio.h"
#include "tier1/interface.h"
#include "probe_checks.h"
#include <dlfcn.h>
#include <fstream>
#include <vector>
#include <iterator>
#include <algorithm>
#include <cstring>
#include <cstdint>

struct Input final : IAudioStreamEvent {
    std::vector<char> data;
    int cursor = 0;
    int StreamRequestData(void *dest, int count, int offset) override {
        if (offset >= 0) cursor = offset;
        if (cursor < 0 || size_t(cursor) > data.size()) return 0;
        int size = std::min<size_t>(count, data.size()-cursor);
        std::memcpy(dest, data.data()+cursor, size); cursor += size; return size;
    }
};
int main(int argc, char **argv) {
    if (argc < 3) return 2;
    void *module = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!module) { std::printf("%s\n", dlerror()); return 1; }
    auto factory = reinterpret_cast<CreateInterfaceFn>(dlsym(module, "CreateInterface"));
    auto audio = factory ? static_cast<IVAudio*>(factory(VAUDIO_INTERFACE_VERSION,nullptr)) : nullptr;
    Check(audio != nullptr, "load actual VAudio002 module"); if (!audio) return 1;
    for (int i=2; i<argc; ++i) {
        Input input; std::ifstream f(argv[i],std::ios::binary);
        input.data.assign(std::istreambuf_iterator<char>(f), {});
        Check(!input.data.empty(),"read original VPK MP3 sample");
        auto stream = audio->CreateMP3StreamDecoder(&input);
        Check(stream && stream->GetOutputBits()==16, "signed 16-bit PCM output");
        Check(stream->GetOutputRate()==44100 && stream->GetOutputChannels()==2, "original stereo music sample metadata");
        std::vector<uint8_t> buffer(16384+64,0xa5); size_t total=0, nonzero=0; unsigned position=0;
        for (int frame=0; frame<10000; ++frame) {
            int count = stream->Decode(buffer.data(),16384);
            Check(count>=0 && count<=16384 && count%4==0,"bounded complete PCM samples");
            Check(std::all_of(buffer.begin()+16384,buffer.end(),[](uint8_t x){return x==0xa5;}),"decode preserves buffer guard");
            if (count<=0) break;
            total+=count; for (int j=0;j<count;j++) nonzero+=buffer[j]!=0;
            unsigned next=stream->GetPosition();
            Check(next>=position && next<=input.data.size(),"compressed stream cursor remains within actual file"); position=next;
        }
        Check(total>44100*4 && nonzero>total/10,"decode actual music into non-silent PCM through EOF");
        Check(stream->Decode(buffer.data(),16384)==0,"stable end of stream");
        stream->SetPosition(0);
        Check(stream->GetPosition()<=input.data.size(),"seek cursor bounded after EOF");
        Check(stream->Decode(buffer.data(),16384)>0,"decoder restarts after seek");
        stream->SetPosition(input.data.size()-1000);
        Check(stream->GetPosition()==input.data.size()-1000,"short final seek discards previous cached chunks");
        int finalBytes=stream->Decode(buffer.data(),16384);
        Check(finalBytes>=0 && finalBytes<=16384 && stream->GetPosition()<=input.data.size(),"short final seek decode remains inside the file");
        std::printf("SAMPLE %s PCM_BYTES=%zu SECONDS=%.6f\n",argv[i],total,total/(44100.0*4));
        audio->DestroyMP3StreamDecoder(stream);
    }
    CheckNativeImages(); dlclose(module);
    std::printf("RESULT %s (%d failures)\n",failures?"FAIL":"PASS",failures); return failures?1:0;
}
