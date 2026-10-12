#pragma once
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <sys/sysctl.h>
#include <mach-o/dyld.h>
#include <mach/machine.h>
static int failures = 0;
static void Check(bool success, const char *message)
{
    std::printf("%s %s\n", success ? "PASS" : "FAIL", message);
    if (!success) ++failures;
}
static void CheckNativeImages()
{
    int translated = 0;
    size_t size = sizeof(translated);
    errno = 0;
    int status = sysctlbyname("sysctl.proc_translated", &translated, &size, nullptr, 0);
    Check((status == 0 && translated == 0) || (status == -1 && errno == ENOENT), "native process, no Rosetta");
    bool native = true, oldGame = false;
    for (uint32_t i = 0; i < _dyld_image_count(); ++i) {
        const char *name = _dyld_get_image_name(i);
        native = native && _dyld_get_image_header(i)->cputype == CPU_TYPE_ARM64;
        oldGame = oldGame || std::strstr(name, "CSGO_2019_1.37.1.9_steam");
        if (std::strncmp(name, "/System/", 8) && std::strncmp(name, "/usr/lib/", 9)) std::printf("IMAGE %s\n", name);
    }
    Check(native && !oldGame, "every loaded image is arm64; no original game binaries");
}
