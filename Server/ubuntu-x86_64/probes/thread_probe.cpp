#include <atomic>
#include <vector>
#include "tier0/threadtools.h"
#include "tier0/tslist.h"
#include "tier0/icommandline.h"
#include "vstdlib/jobthread.h"
#include "probe_checks.h"

struct Packet { int id; uint64 payload; };
static const int producers = 4, perProducer = 10000, total = producers * perProducer;
static CTSQueue<Packet> queue;
static CThreadManualEvent start;
static std::atomic<int> produced(0), consumed(0), corrupt(0);
static std::atomic<int> seen[total];
static CThreadMutex mutex;
static int protectedCount = 0;
static uint64 Pattern(int id) { return uint64(id) * 0x9e3779b97f4a7c15ULL; }
static uintp Producer(void *context)
{
    int index = static_cast<int>(reinterpret_cast<intptr_t>(context));
    start.Wait();
    for (int i = 0; i < perProducer; ++i) {
        int id = index * perProducer + i;
        queue.PushItem(Packet{id, Pattern(id)});
    }
    ++produced;
    return 0;
}
static uintp Consumer(void *)
{
    start.Wait();
    while (produced.load() < producers || consumed.load() < total) {
        Packet packet;
        if (queue.PopItem(&packet)) {
            if (packet.id < 0 || packet.id >= total || packet.payload != Pattern(packet.id)) ++corrupt;
            else ++seen[packet.id];
            ++consumed;
            mutex.Lock(); mutex.Lock(); ++protectedCount; mutex.Unlock(); mutex.Unlock();
        } else ThreadPause();
    }
    return 0;
}
struct JobResult { std::atomic<int> calls; std::atomic<int> workerCalls; JobResult() : calls(0), workerCalls(0) {} };
static void JobWork(JobResult *result)
{
    ++result->calls;
    if (!ThreadInMainThread()) ++result->workerCalls;
}
int main()
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    DeclareCurrentThreadIsMainThread();
    CommandLine()->CreateCmdLine("thread_probe -allowdebug");
    for (auto &value : seen) value.store(0);
    ThreadHandle_t workers[8];
    for (int i = 0; i < 4; ++i) workers[i] = CreateSimpleThread(Producer, reinterpret_cast<void *>(static_cast<intptr_t>(i)));
    for (int i = 4; i < 8; ++i) workers[i] = CreateSimpleThread(Consumer, nullptr);
    start.Set();
    bool joined = true;
    for (auto worker : workers) {
        joined = joined && worker && ThreadJoin(worker, 20000);
        if (worker) ReleaseThreadHandle(worker);
    }
    Check(joined, "four producers/four consumers join through engine thread API");
    if (!joined) return 1;
    bool exact = true;
    for (auto &value : seen) exact = exact && value.load() == 1;
    Check(exact && consumed == total && corrupt == 0 && queue.Count() == 0, "CTSQueue preserves 40000 payloads exactly once with concurrent node reuse");
    Check(protectedCount == total, "recursive engine mutex protects shared updates");
    IThreadPool *pool = CreateNewThreadPool();
    ThreadPoolStartParams_t params(false, 4);
    params.bExecOnThreadPoolThreadsOnly = true;
    Check(pool && pool->Start(params), "actual vstdlib thread pool starts four workers");
    if (!pool || pool->NumThreads() != 4) return 1;
    JobResult result;
    std::vector<CJob *> jobs;
    pool->SuspendExecution();
    for (int i = 0; i < 1024; ++i) jobs.push_back(pool->QueueCall(JobWork, &result));
    pool->ResumeExecution();
    // The source's job-array overload accepts at most 62 jobs, and only implements
    // an infinite timeout. Wait in legal batches; CTest bounds the process lifetime.
    bool done = true;
    for (size_t first = 0; first < jobs.size(); first += 32)
        done = pool->YieldWait(jobs.data() + first, 32, true, TT_INFINITE) == 0 && done;
    for (auto job : jobs) { done = done && job->IsFinished(); job->Release(); }
    std::printf("JOBS finished=%s calls=%d worker_calls=%d\n", done ? "yes" : "no", result.calls.load(), result.workerCalls.load());
    Check(done && result.calls == 1024 && result.workerCalls == 1024, "1024 queued jobs complete on real worker threads");
    Check(pool->Stop(20000), "thread pool stops cleanly");
    DestroyThreadPool(pool);
    CheckNativeImages();
    std::printf("RESULT %s (%d failures)\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
