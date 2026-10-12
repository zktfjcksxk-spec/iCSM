#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include "tier0/icommandline.h"

// Deliberately not linked to tier0: dlopen owns its entire load/unload lifetime.
int main(int argc, char **argv)
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc != 2) return 2;
    void *module = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!module) { std::fprintf(stderr, "%s\n", dlerror()); return 1; }
    auto compare = reinterpret_cast<int (*)(const char *, const char *)>(dlsym(module, "V_tier0_stricmp"));
    auto commandLine = reinterpret_cast<ICommandLine *(*)()>(dlsym(module, "CommandLine"));
    auto *clockSpeed = reinterpret_cast<uint64 *>(dlsym(module, "g_ClockSpeed"));
    if (!compare || !commandLine || !clockSpeed) {
        std::fprintf(stderr, "missing real tier0 export\n");
        dlclose(module);
        return 1;
    }
    commandLine()->CreateCmdLine("loader -game csgo +map de_dust2");
    bool ok = compare("ARM64", "arm64") == 0 && *clockSpeed == 1000000000ULL &&
              std::strcmp(commandLine()->ParmValue("+map", ""), "de_dust2") == 0;
    if (dlclose(module) != 0) ok = false;
    std::printf("%s independent dlopen, real exports, command line, dlclose\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
