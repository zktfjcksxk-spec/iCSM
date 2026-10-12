#include <fstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <unistd.h>
#include "tier0/icommandline.h"
#include "tier0/threadtools.h"
#include "tier1/interface.h"
#include "tier1/tier1.h"
#include "tier1/checksum_crc.h"
#include "tier1/checksum_sha1.h"
#include "tier1/utlbuffer.h"
#include "interfaces/interfaces.h"
#include "icvar.h"
#include "filesystem.h"
#include "bspfile.h"
#include "probe_checks.h"

static CreateInterfaceFn factories[2];
static void *Factory(const char *version, int *status)
{
    for (auto factory : factories) {
        void *result = factory(version, status);
        if (result) return result;
    }
    return nullptr;
}
struct Sample { std::string kind, path, sha1; unsigned size, crc; };
static uint32 ReadU32(const void *data) { uint32 value; std::memcpy(&value, data, 4); return value; }
int main(int argc, char **argv)
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc != 5) { std::fprintf(stderr, "usage: resource_probe filesystem.dylib vstdlib.dylib csgo-dir samples.tsv\n"); return 2; }
    DeclareCurrentThreadIsMainThread();
    CommandLine()->CreateCmdLine("resource_probe -allowdebug");
    std::vector<Sample> samples;
    std::ifstream input(argv[4]);
    Sample record; std::string crc;
    while (input >> record.kind >> record.path >> record.size >> record.sha1 >> crc) {
        record.crc = static_cast<unsigned>(std::stoul(crc, nullptr, 16));
        samples.push_back(record);
    }
    Check(samples.size() == 5, "five independently hashed original resource samples");
    if (samples.size() != 5) return 1;
    CSysModule *cvarModule = Sys_LoadModule(argv[2]), *fsModule = Sys_LoadModule(argv[1]);
    Check(cvarModule && fsModule, "engine Sys_LoadModule loads native vstdlib and filesystem");
    if (!cvarModule || !fsModule) return 1;
    factories[0] = Sys_GetFactory(cvarModule); factories[1] = Sys_GetFactory(fsModule);
    Check(factories[0] && factories[1], "engine Sys_GetFactory resolves real CreateInterface exports");
    if (!factories[0] || !factories[1]) return 1;
    ICvar *cv = static_cast<ICvar *>(Factory(CVAR_INTERFACE_VERSION, nullptr));
    IFileSystem *fs = static_cast<IFileSystem *>(Factory(FILESYSTEM_INTERFACE_VERSION, nullptr));
    Check(cv && fs, "local VEngineCvar007 and VFileSystem017 interfaces");
    if (!cv || !fs) return 1;
    Check(cv->Connect(Factory) && cv->Init() == INIT_OK, "cvar connects and initializes");
    Check(fs->Connect(Factory) && fs->Init() == INIT_OK, "filesystem connects to cvar and starts actual async workers");
    CreateInterfaceFn combined = Factory;
    ConnectInterfaces(&combined, 1);
    Check(g_pCVar == cv && g_pFullFileSystem == fs, "interfaces library connects actual module globals");
    std::string root(argv[3]);
    // CPackedStore's public identity is pak01.vpk; it opens pak01_dir.vpk itself.
    fs->AddVPKFile((root + "/pak01.vpk").c_str());
    fs->AddVPKFile((root + "/pak01.vpk").c_str());
    CUtlVector<CUtlString> mounted;
    fs->GetVPKFileNames(mounted);
    Check(mounted.Count() == 1, "canonical VPK base name mounts once, duplicate addition is ignored");
    fs->AddSearchPath(root.c_str(), "GAME");
    bool config = cv->FindVar("filesystem_buffer_size") != nullptr;
    Check(config, "filesystem registers its real console variables in vstdlib");

    unsigned bspSize = 0, savedBspSize = 0;
    for (const auto &sample : samples) {
        const char *pathId = sample.kind == "vpk" ? nullptr : "GAME";
        if (sample.kind == "vpk")
            Check(access((root + "/" + sample.path).c_str(), F_OK) == -1 && errno == ENOENT,
                  "VPK sample has no loose-file fallback");
        FileHandle_t handle = fs->Open(sample.path.c_str(), "rb", pathId);
        Check(handle != FILESYSTEM_INVALID_HANDLE, sample.path.c_str());
        if (handle == FILESYSTEM_INVALID_HANDLE) continue;
        unsigned size = fs->Size(handle), read = 0;
        Check(size == sample.size, "resource length matches independent reference");
        CSHA1 sha; CRC32_t actualCrc; CRC32_Init(&actualCrc);
        std::vector<unsigned char> buffer(1024 * 1024);
        unsigned char header[sizeof(BSPHeader_t)] = {};
        bool headerCopied = false;
        while (read < size) {
            int amount = fs->Read(buffer.data(), static_cast<int>(std::min<unsigned>(buffer.size(), size - read)), handle);
            if (amount <= 0) break;
            if (!headerCopied) { std::memcpy(header, buffer.data(), std::min<size_t>(sizeof(header), amount)); headerCopied = true; }
            sha.Update(buffer.data(), amount); CRC32_ProcessBuffer(&actualCrc, buffer.data(), amount);
            read += amount;
        }
        sha.Final(); CRC32_Final(&actualCrc);
        char hash[41]; sha.GetHashHex(hash, sizeof(hash));
        std::printf("RESOURCE %s bytes=%u crc32=%08x sha1=%s\n", sample.path.c_str(), read, actualCrc, hash);
        Check(read == size && actualCrc == sample.crc && V_stricmp(sample.sha1.c_str(), hash) == 0, "complete engine read matches independent CRC32 and SHA-1");
        if (sample.kind == "bsp") {
            const BSPHeader_t *bsp = reinterpret_cast<const BSPHeader_t *>(header);
            bool valid = bsp->ident == IDBSPHEADER && bsp->m_nVersion == BSPVERSION;
            for (const auto &lump : bsp->lumps)
                valid = valid && lump.fileofs >= 0 && lump.filelen >= 0 && uint64(lump.fileofs) + uint64(lump.filelen) <= size;
            Check(valid, "actual VBSP version 21 and all 64 lump boundaries");
            bspSize = size;
        } else if (sample.kind == "nav") {
            Check(ReadU32(header) == 0xFEEDFACE && ReadU32(header + 4) == 16 && ReadU32(header + 8) == 1,
                  "actual NAV magic, version 16 and CS:GO subversion 1");
            savedBspSize = ReadU32(header + 12);
        }
        fs->Seek(handle, size / 2, FILESYSTEM_SEEK_HEAD);
        Check(fs->Tell(handle) == static_cast<int>(size / 2), "engine file seek/tell");
        fs->Close(handle);
    }
    std::printf("NAV_SOURCE_BSP_SIZE saved=%u actual=%u match=%s\n", savedBspSize, bspSize, savedBspSize == bspSize ? "yes" : "no");
    Check(!fs->FileExists("maps/m1_nonexistent_map.bsp", "GAME"), "missing resource is reported absent");

    // Use the actual async path with caller-owned storage, then wait and release its handle.
    std::vector<unsigned char> asyncData(samples[4].size);
    FileAsyncRequest_t request;
    request.pszFilename = samples[4].path.c_str(); request.pszPathID = "GAME";
    request.pData = asyncData.data(); request.nBytes = asyncData.size();
    FSAsyncControl_t control = nullptr;
    FSAsyncStatus_t status = fs->AsyncRead(request, &control);
    Check(status >= FSASYNC_OK && control, "filesystem accepts actual asynchronous NAV read");
    if (control) {
        status = fs->AsyncFinish(control, true);
        void *data = nullptr; int size = 0;
        FSAsyncStatus_t result = fs->AsyncGetResult(control, &data, &size);
        CSHA1 sha; sha.Update(asyncData.data(), asyncData.size()); sha.Final(); char hash[41]; sha.GetHashHex(hash, sizeof(hash));
        Check(status == FSASYNC_OK && result == FSASYNC_OK && size == static_cast<int>(asyncData.size()) && data == asyncData.data() && V_stricmp(samples[4].sha1.c_str(), hash) == 0,
              "async worker returns identical NAV bytes and hash");
        fs->AsyncRelease(control);
    }
    CheckNativeImages();
    DisconnectInterfaces();
    fs->RemoveVPKFile((root + "/pak01.vpk").c_str());
    mounted.RemoveAll();
    fs->GetVPKFileNames(mounted);
    Check(mounted.Count() == 0, "RemoveVPKFile matches the original mixed-case path and releases stores");
    fs->AddVPKFile((root + "/pak01.vpk").c_str());
    FileHandle_t remounted = fs->Open(samples[1].path.c_str(), "rb");
    unsigned char reopened[376];
    Check(remounted && fs->Read(reopened, sizeof(reopened), remounted) == sizeof(reopened), "VPK remount opens a real chunk again");
    if (remounted) fs->Close(remounted);
    fs->Shutdown();
    mounted.RemoveAll(); fs->GetVPKFileNames(mounted);
    Check(mounted.Count() == 0, "filesystem Shutdown releases every owned VPK store");
    fs->Disconnect();
    cv->Shutdown(); cv->Disconnect();
    Sys_UnloadModule(fsModule); Sys_UnloadModule(cvarModule);
    std::printf("PASS engine Shutdown/Disconnect/Sys_UnloadModule returned normally\n");
    std::printf("RESULT %s (%d failures)\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
