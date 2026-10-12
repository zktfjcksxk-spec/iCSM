#include "steamnetworkingsockets/isteamnetworkingsockets.h"
#include "steamnetworkingsockets/isteamnetworkingutils.h"
#include <cstdio>
#include <cstring>
#include <vector>
#include <thread>
#include <chrono>
#include <cstdlib>

static void Require(bool value, const char *what)
{
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", what); std::exit(1); }
}

struct Callbacks : ISteamNetworkingSocketsCallbacks
{
    int closed = 0;
    void OnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t *e) override
    {
        std::printf("callback: handle=%u state=%d old=%d reason=%d description=%s\n", e->m_hConn,
                    e->m_info.m_eState, e->m_eOldState, e->m_info.m_eEndReason,
                    e->m_info.m_szConnectionDescription);
        if (e->m_info.m_eState == k_ESteamNetworkingConnectionState_ClosedByPeer)
        {
            Require(e->m_eOldState == k_ESteamNetworkingConnectionState_Connected,
                    "closed peer callback reads 2019 m_eOldState after description");
            ++closed;
        }
    }
    void OnP2PSessionRequest(P2PSessionRequest_t *) override {}
    void OnP2PSessionConnectFail(P2PSessionConnectFail_t *) override {}
};

int main()
{
    SteamDatagramErrMsg error;
    Require(GameNetworkingSockets_Init(error), error);
    auto api = SteamNetworkingSockets();
    Require(api != nullptr, "real standalone accessor");
    for (bool network : {false, true})
    {
        HSteamNetConnection a, b;
        Require(api->CreateSocketPair(&a, &b, network), "socket pair");
        Require(api->SetConnectionUserData(b, 0x123456789LL), "64 bit user data");
        api->SetConnectionName(a, "native-arm64-sender");
        SteamNetConnectionInfo_t info{};
        Require(api->GetConnectionInfo(a, &info), "connection info");
        Require(info.m_eState == k_ESteamNetworkingConnectionState_Connected, "connected state");
        Require(std::strcmp(info.m_szConnectionDescription, "native-arm64-sender") == 0,
                "2019 connection description populated by actual connection");
        std::vector<unsigned char> payload(65537);
        for (size_t i = 0; i < payload.size(); ++i) payload[i] = (i*13+29) & 255;
        Require(api->SendMessageToConnection(a, payload.data(), payload.size(),
                    k_ESteamNetworkingSendType_ReliableNoNagle) == k_EResultOK, "fragmented reliable send");
        SteamNetworkingMessage_t *message = nullptr;
        auto until = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (!message && std::chrono::steady_clock::now() < until)
        {
            api->ReceiveMessagesOnConnection(b, &message, 1);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        Require(message != nullptr, "receive through real transport");
        Require(message->m_cbSize == payload.size() &&
                std::memcmp(message->m_pData, payload.data(), payload.size()) == 0, "fragment reassembly bytes");
        Require(message->m_nConnUserData == 0x123456789LL && message->m_conn == b, "message 2019 layout");
        Require(message->m_usecTimeReceived > 0 && SteamNetworkingSockets_GetLocalTimestamp() >= message->m_usecTimeReceived,
                "monotonic receive timestamp");
        message->Release();
        Require(api->CloseConnection(a, 1000, "native transport acceptance", false), "close sender");
        Callbacks callbacks;
        until = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (!callbacks.closed && std::chrono::steady_clock::now() < until)
        {
            api->RunCallbacks(&callbacks);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        Require(callbacks.closed > 0, "peer close callback with 2019 layout");
        Require(api->CloseConnection(b, 1000, "cleanup", false), "close receiver");
        std::printf("PASS: %s payload=%zu; metadata, release, callback, cleanup\n",
                    network ? "encrypted UDP loopback" : "memory socket pair", payload.size());
    }
    GameNetworkingSockets_Kill();
    return 0;
}
