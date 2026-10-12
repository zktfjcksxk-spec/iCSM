#include <cstdint>
#include <fstream>
#include <iterator>
#include <vector>
#include <cstdio>
#include "cryptlib.h"
#include "rsa.h"

int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    std::ifstream f(argv[1], std::ios::binary);
    std::vector<CryptoPP::byte> data((std::istreambuf_iterator<char>(f)), {});
    const CryptoPP::byte key[] = {
#include "devtools/bin/certificates/panoramapack.public.h"
    };
    if (data.size() <= 517 || data[0] != 'P' || data[1] != 'A' ||
        data[2] != 'N' || data[3] != 1 || data.back() != 1) return 1;
    try {
        CryptoPP::StringSource source(key, sizeof(key), true);
        CryptoPP::RSASSA_PKCS1v15_SHA_Verifier verifier(source);
        if (!verifier.VerifyMessage(data.data()+516, data.size()-516, data.data()+4, 512)) return 1;
        data[600] ^= 1;
        if (verifier.VerifyMessage(data.data()+516, data.size()-516, data.data()+4, 512)) return 1;
        std::puts("PASS original code.pbin signature and modified payload rejection through original Crypto++ verifier");
        return 0;
    } catch (const std::exception &e) { std::printf("FAIL %s\n", e.what()); return 1; }
}
