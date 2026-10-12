// UIKit queues bounded batches. Source alone reads engine state and executes.
#include "frame_calibration_settings.h"
static char s_calibrationCommands[8192] = {};
static void CalibrationQueue(const char *commands) {
    if (!commands || V_strlen(commands) + V_strlen(s_calibrationCommands) + 2 >= sizeof(s_calibrationCommands)) return;
    V_strncat(s_calibrationCommands, commands, sizeof(s_calibrationCommands));
    V_strncat(s_calibrationCommands, "\n", sizeof(s_calibrationCommands));
}
static size_t CalibrationScene(char *buffer, size_t capacity) {
    if (!buffer || capacity < 4096 || !engine) return 0;
    C_CSPlayer *player = engine->IsInGame() ? C_CSPlayer::GetLocalCSPlayer() : NULL;
    Vector origin = player ? player->GetAbsOrigin() : vec3_origin;
    char map[128] = ""; V_FileBase(engine->GetLevelName(), map, sizeof(map));
    for (char *p=map; *p; ++p) if (!((*p>='a' && *p<='z') || (*p>='0' && *p<='9') || *p=='_')) *p='_';
    int length=V_snprintf(buffer,capacity,
        "{\"in_game\":%s,\"connected\":%s,\"map\":\"%s\",\"alive\":%s,\"team\":%d,\"move_type\":%d,\"origin\":[%g,%g,%g],\"values\":{",
        engine->IsInGame()?"true":"false",engine->IsConnected()?"true":"false",map,
        player && player->IsAlive()?"true":"false",player?player->GetTeamNumber():0,
        player?int(player->GetMoveType()):0,origin.x,origin.y,origin.z);
    bool first=true;
    for (const char *name:kFrameCalibrationSettings) {
        ConVarRef value(name,true);if(!value.IsValid() || !IsFinite(value.GetFloat()))continue;
        length+=V_snprintf(buffer+length,capacity-length,"%s\"%s\":%.9g",first?"":",",name,value.GetFloat());first=false;
    }
    for(const char *name:{"icsm_quality_preset","icsm_scene_render_percent","icsm_skin_shader_detail","icsm_skin_texture_detail","mat_motion_blur_enabled"}) {
        ConVarRef value(name,true);if(value.IsValid())length+=V_snprintf(buffer+length,capacity-length,",\"%s\":%.9g",name,value.GetFloat());
    }
    int width=0,height=0;engine->GetScreenSize(width,height);
    int ct=0,terrorists=0,bots=0;
    if(engine->IsInGame())for(int i=1;i<=engine->GetMaxClients();++i) {
        C_CSPlayer *other=ToCSPlayer(UTIL_PlayerByIndex(i));if(!other)continue;
        if(other->GetTeamNumber()==3)++ct;
        if(other->GetTeamNumber()==2)++terrorists;
        if((other->GetTeamNumber()==2 || other->GetTeamNumber()==3) && other->IsBot())++bots;
    }
    ConVarRef botMode("bot_quota_mode",true);
    const char *mode=botMode.IsValid()?botMode.GetString():"normal";
    if(V_strcmp(mode,"fill") && V_strcmp(mode,"match"))mode="normal";
    length+=V_snprintf(buffer+length,capacity-length,",\"bot_quota_mode\":\"%s\"",mode);
    length+=V_snprintf(buffer+length,capacity-length,"},\"render_width\":%d,\"render_height\":%d,\"ct\":%d,\"t\":%d,\"bots\":%d}",width,height,ct,terrorists,bots);
    return length>0 && size_t(length)<capacity?size_t(length):0;
}
static void CalibrationFrame() {
    // Startup recovery must follow the normal config/video synchronization.
    if(!s_qualityReady)return;
    if (s_calibrationCommands[0]) {
        char commands[sizeof(s_calibrationCommands)]; V_strncpy(commands,s_calibrationCommands,sizeof(commands));
        s_calibrationCommands[0]=0; engine->ClientCmd_Unrestricted(commands,true);
    }
    SourceTouchFrameCalibrationTick();
}
static void CalibrationCommand(const CCommand &args) {
    ReleaseGameActions();
    SourceTouchOpenFrameCalibration(CalibrationQueue,CalibrationScene,args.ArgC()>1?args[1]:"");
}
static ConCommand icsm_frame_calibration("icsm_frame_calibration",CalibrationCommand,
    "Device-specific repeatable offline MetalFX benchmark: quick/full/cancel/apply/status.",FCVAR_CLIENTCMD_CAN_EXECUTE);
static void CalibrationRestoreCommitted(const CCommand &) {
    // Host_WriteConfiguration uses AsyncWrite. Wait once in the lobby before
    // discarding the recovery snapshot, including after an interrupted test.
    if(filesystem)filesystem->AsyncFinishAllWrites();
    SourceTouchFrameCalibrationRestoreCommitted();
}
static ConCommand icsm_frame_calibration_restore_complete("icsm_frame_calibration_restore_complete",CalibrationRestoreCommitted,
    "Confirm calibration preference recovery after the configuration write completes.",FCVAR_DONTRECORD);
