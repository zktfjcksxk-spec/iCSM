#include <chrono>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <sys/sysctl.h>
#include "tier0/platform.h"
#include "tier0/icommandline.h"
#include "tier0/fasttimer.h"
#include "tier0/microprofiler.h"
#include "tier0/hardware_clock_fast.h"
#include "tier0/threadtools.h"
#include "tier0/tslist.h"
#include "tier0/memalloc.h"

static int failures;
static void Check(bool ok, const char *name)
{
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", name);
    if (!ok) ++failures;
}

static volatile int32 counter;
static CTSList<int> concurrentList;
static uintp Worker(void *)
{
    for (int i = 0; i < 25000; ++i) ThreadInterlockedIncrement(&counter);
    for (int i = 0; i < 1000; ++i) concurrentList.PushItem(i);
    return 0;
}

int main(int argc, char **argv)
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc != 2) { std::fprintf(stderr, "usage: tier0_probe /path/libtier0.dylib\n"); return 2; }
    static_assert(sizeof(void *) == 8, "64-bit pointers required");
#if !defined(__aarch64__)
#error Native ARM64 required
#endif
    int translated = 0;
    size_t size = sizeof(translated);
    int status = sysctlbyname("sysctl.proc_translated", &translated, &size, nullptr, 0);
    Check(status == 0 ? translated == 0 : errno == ENOENT, "native process (no Rosetta)");
    void *module = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!module) { std::fprintf(stderr, "dlopen: %s\n", dlerror()); return 1; }
    auto compare = reinterpret_cast<int (*)(const char *, const char *)>(dlsym(module, "V_tier0_stricmp"));
    Check(compare && compare("CsGo", "csgo") == 0 && compare("a", "b") < 0,
          "dlopen/dlsym real tier0 string API");
    Dl_info info = {};
    Check(compare && dladdr(reinterpret_cast<void *>(compare), &info) != 0, "resolve loaded module path");
    if (info.dli_fname) std::printf("MODULE %s\n", info.dli_fname);

    DeclareCurrentThreadIsMainThread();
    Check(ThreadInMainThread(), "main thread registration");
    ICommandLine *cmd = CommandLine();
    cmd->CreateCmdLine("tier0_probe -game csgo +map de_dust2 -bots 8");
    Check(std::strcmp(cmd->ParmValue("-game", ""), "csgo") == 0 &&
          cmd->ParmValue("-bots", 0) == 8, "real command line parsing");
    cmd->AppendParm("-port", "27015");
    cmd->RemoveParm("-bots");
    Check(cmd->ParmValue("-port", 0) == 27015 && !cmd->HasParm("-bots"), "command line mutation");

    const CPUInformation &cpu = GetCPUInformation();
    std::printf("CPU %s / %s logical=%u physical=%u profiling_frequency=%llu\n",
                cpu.m_szProcessorID, cpu.m_szProcessorBrand,
                unsigned(cpu.m_nLogicalProcessors), unsigned(cpu.m_nPhysicalProcessors),
                static_cast<unsigned long long>(cpu.m_Speed));
    Check(cpu.m_nLogicalProcessors > 0 && cpu.m_nPhysicalProcessors > 0 &&
          std::strcmp(cpu.m_szProcessorID, "ARM") == 0, "ARM CPU information");
    Check(g_ClockSpeed == 1000000000ULL, "profiling nanosecond frequency");
    const auto begin = std::chrono::steady_clock::now();
    CFastTimer timer;
    timer.Start();
    const uint64 microBegin = GetTimebaseRegister();
    const int hardwareBegin = GetHardwareClockFast();
    ThreadSleep(20);
    timer.End();
    const uint64 microEnd = GetTimebaseRegister();
    const int hardwareEnd = GetHardwareClockFast();
    const double elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count();
    const double tierMs = timer.GetDuration().GetMillisecondsF();
    std::printf("TIMER tier0=%.3fms reference=%.3fms\n", tierMs, elapsed);
    Check(tierMs >= 15.0 && tierMs < 2000.0 && tierMs / elapsed > 0.8 && tierMs / elapsed < 1.2,
          "timer units match independent monotonic clock");
    Check(microEnd > microBegin && static_cast<uint32>(hardwareEnd) != static_cast<uint32>(hardwareBegin),
          "microprofiler and hardware clock advance");

    Check(g_pMemAlloc != nullptr, "real engine allocator exists");
    auto *memory = static_cast<unsigned char *>(g_pMemAlloc->Alloc(513));
    Check(memory && g_pMemAlloc->GetSize(memory) >= 513, "allocator allocation and size");
    if (!memory) return 1;
    std::memset(memory, 0x5a, 513);
    memory = static_cast<unsigned char *>(g_pMemAlloc->Realloc(memory, 4097));
    Check(memory && memory[0] == 0x5a && memory[512] == 0x5a, "allocator growth preserves data");
    if (!memory) return 1;
    g_pMemAlloc->Free(memory);
    void *aligned = MemAlloc_AllocAligned(257, 64);
    Check(aligned && reinterpret_cast<uintp>(aligned) % 64 == 0, "64-byte aligned allocation");
    MemAlloc_FreeAligned(aligned);

    volatile int32 exchange = 7;
    Check(ThreadInterlockedExchange(&exchange, 11) == 7 && exchange == 11, "32-bit atomic exchange returns old value");
    volatile int64 exchange64 = 0x123456789LL;
    Check(ThreadInterlockedExchange64(&exchange64, 42) == 0x123456789LL && exchange64 == 42,
          "64-bit atomic exchange");
    ThreadMemoryBarrier();
    ThreadPause();
    ThreadHandle_t workers[4];
    for (auto &worker : workers) worker = CreateSimpleThread(Worker, nullptr);
    for (auto worker : workers) {
        Check(worker && ThreadJoin(worker), "engine worker thread join");
        if (worker) ReleaseThreadHandle(worker);
    }
    Check(counter == 100000, "four-thread atomic increment (100000)");
    int item, count = 0, sum = 0;
    while (concurrentList.PopItem(&item)) { ++count; sum += item; }
    Check(count == 4000 && sum == 4 * 999 * 1000 / 2, "concurrent list preserves 4000 items (128-bit CAS)");
    Check(dlclose(module) == 0, "explicit module handle close");
    std::printf("RESULT %s (%d failures)\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
