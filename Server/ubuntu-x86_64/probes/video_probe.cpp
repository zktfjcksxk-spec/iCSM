#include "tier0/platform.h"
#include "video/ivideoplayer.h"
#include "probe_checks.h"
#include <atomic>
#include <thread>
#include <chrono>
#include <fstream>
#include <iterator>
#include <vector>

struct Callbacks final : IVideoPlayerEventCallback, IVideoPlayerVideoCallback, IVideoPlayerAudioCallback {
    const std::thread::id mainThread=std::this_thread::get_id();
    std::atomic<int> frames{0}, audioInits{0}, audioFrees{0};
    std::atomic<size_t> audioBytes{0}, nonzeroAudio{0};
    std::atomic<bool> workerAudio{true}, validPlanes{true};
    int inits=0, ends=0, repeats=0; int rate=0,channels=0;
    std::vector<byte> buffer=std::vector<byte>(16384);
    void VideoPlayerEvent(EVideoPlayerEvent e) override { if(e==k_EVideoPlayerEventInit)++inits; if(e==k_EVideoPlayerEventEnd)++ends; if(e==k_EVideoPlayerEventRepeat)++repeats; }
    bool BPresentYUV420Texture(uint w,uint h,void *y,void *u,void *v,uint sy,uint su,uint sv) override {
        validPlanes=validPlanes && w>0 && h>0 && y && u && v && sy>=w && su>=(w+1)/2 && sv>=(w+1)/2;
        ++frames; return true;
    }
    bool InitAudioOutput(int r,int c) override { workerAudio=workerAudio && std::this_thread::get_id()!=mainThread; rate=r; channels=c; ++audioInits; return r>0 && c>0; }
    void FreeAudioOutput() override { ++audioFrees; }
    bool IsReadyForAudioData() override { return true; }
    void *GetAudioBuffer() override { return buffer.data(); }
    uint32 GetAudioBufferSize() override { return buffer.size(); }
    uint32 GetAudioBufferMinSize() override { return 4; }
    void CommitAudioBuffer(uint32 n) override { audioBytes+=n; for(uint32 i=0;i<n;i++) nonzeroAudio+=buffer[i]!=0; }
    uint32 GetRemainingCommittedAudio() override { return 0; }
    uint32 GetMixedMilliseconds() override { return audioBytes/(rate*channels*2.0)*1000; }
    uint32 GetPlaybackLatency() override { return 0; }
    void Pause() override {}
    void Resume() override {}
};
template<class Predicate> bool Until(Predicate predicate,int ms=4000) {
    auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(ms);
    while(std::chrono::steady_clock::now()<deadline) { VideoPlaybackRunFrame(); if(predicate())return true; std::this_thread::sleep_for(std::chrono::milliseconds(5)); }
    return false;
}
int main(int argc,char **argv) {
    if(argc!=3)return 2;
    VideoPlaybackInitialize(); Callbacks cb; auto player=CreateVideoPlayer(&cb,&cb,&cb);
    Check(player && player->BLoad(argv[1]),"load original Panorama WebM file");
    Check(Until([&]{return cb.inits>0;}),"asynchronous original video initialization");
    int w,h; player->GetVideoResolution(&w,&h);
    Check(w>0 && h>0 && player->GetDuration()>1000 && player->GetVideoRepresentationCount()==1,"real decoded local video metadata");
    player->SuggestMaxVeritcalResolution(360); player->Play();
    Check(Until([&]{return cb.frames>3;}),"present actual decoded YUV420 planes");
    player->Pause(); Until([]{return false;},100); auto paused=player->GetCurrentPlaybackTime(); Until([]{return false;},150);
    Check(player->GetCurrentPlaybackTime()==paused,"pause freezes playback clock");
    player->SetRepeat(true); player->Seek(player->GetDuration()-250); player->Play();
    Check(Until([&]{return cb.repeats>0;}),"seek and repeat emit original event");
    player->SetRepeat(false); player->Seek(player->GetDuration()-250);
    Check(Until([&]{return cb.ends>0;}),"natural end emits stop and end event");
    Check(player->GetPlaybackError()==k_EVideoPlayerPlaybackErrorNone && cb.validPlanes,"valid video output with no decode error");
    DeleteVideoPlayer(player);
    std::ifstream f(argv[2],std::ios::binary); std::vector<byte> bytes((std::istreambuf_iterator<char>(f)),{});
    Callbacks av; player=CreateVideoPlayer(&av,&av,&av);
    Check(player->BLoad(bytes.data(),bytes.size()),"load original audio/video WebM from memory");
    Check(Until([&]{return av.inits>0;}),"memory AVIO opens real container");
    Check(player->BHasAudioTrack(),"detect original Vorbis audio stream");
    player->Seek(10000); player->Play(); Check(Until([&]{return av.audioBytes>4096 && av.nonzeroAudio>100 && av.frames>2;}),"decode actual Vorbis into interleaved PCM alongside video");
    Check(av.audioInits>0 && av.workerAudio && av.nonzeroAudio>100,"audio initializes on worker thread and emits non-silent PCM");
    Check(player->GetPlaybackError()==k_EVideoPlayerPlaybackErrorNone,"audio/video playback has no decode error");
    DeleteVideoPlayer(player); Check(av.audioFrees==av.audioInits,"destroy releases actual audio output");
    VideoPlaybackShutdown(); CheckNativeImages();
    std::printf("VIDEO_FRAMES=%d AUDIO_BYTES=%zu RESULT %s (%d failures)\n",cb.frames.load(),av.audioBytes.load(),failures?"FAIL":"PASS",failures); return failures?1:0;
}
