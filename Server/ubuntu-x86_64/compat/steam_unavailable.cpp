// The proprietary desktop Steam runtime has no iOS distribution. These hooks
// represent an unavailable service, never a successful login/interface/callback.
// Public return contracts: steam_api.h / steam_gameserver.h / steam_api_internal.h.
#define STEAM_API_EXPORTS
#include "steam/steam_api.h"
#include "steam/steam_gameserver.h"
#include <cstdio>
#include <cstdlib>
S_API bool SteamAPI_Init(){fputs("I3 Steam unavailable on iOS; offline NO_STEAM only\n",stderr);return false;}
S_API bool SteamAPI_InitSafe(){return SteamAPI_Init();}
S_API bool SteamInternal_Init(){return SteamAPI_Init();}
S_API bool SteamAPI_IsSteamRunning(){return false;}
S_API bool SteamAPI_RestartAppIfNecessary(uint32){return false;}
S_API void SteamAPI_Shutdown(){}
S_API void SteamAPI_RunCallbacks(){}
S_API void SteamAPI_ReleaseCurrentThreadMemory(){}
S_API void SteamAPI_RegisterCallback(CCallbackBase*,int){}
S_API void SteamAPI_UnregisterCallback(CCallbackBase*){}
S_API void SteamAPI_RegisterCallResult(CCallbackBase*,SteamAPICall_t){}
S_API void SteamAPI_UnregisterCallResult(CCallbackBase*,SteamAPICall_t){}
S_API HSteamPipe SteamAPI_GetHSteamPipe(){return 0;}
S_API HSteamUser SteamAPI_GetHSteamUser(){return 0;}
S_API void* SteamInternal_CreateInterface(const char*){return nullptr;}
S_API void* SteamGameServerInternal_CreateInterface(const char*){return nullptr;}
S_API HSteamPipe SteamGameServer_GetHSteamPipe(){return 0;}
S_API HSteamUser SteamGameServer_GetHSteamUser(){return 0;}
S_API void SteamGameServer_Shutdown(){}
S_API void SteamGameServer_RunCallbacks(){}
S_API bool SteamGameServer_BSecure(){return false;}
S_API uint64 SteamGameServer_GetSteamID(){return 0;}
S_API CSteamGameServerAPIContext* SteamInternal_GlobalContextGameServerPtr(uint32 size){
 static CSteamGameServerAPIContext ctx;
 if(size!=sizeof(ctx))abort();
 return &ctx;
}
