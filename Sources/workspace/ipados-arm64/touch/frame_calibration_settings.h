#pragma once
// Recovery restores only this numeric allowlist, never stored commands.
static const char *const kFrameCalibrationSettings[] = {
    "icsm_fps_limit", "icsm_quality_unlocked_fps", "fps_max", "fps_max_menu",
    "icsm_metal_frame_interpolation", "icsm_metal_frame_scale",
    "icsm_metal_frame_color_scale", "icsm_metal_frame_quality",
    "icsm_metal_frame_adaptive", "cl_yawspeed", "game_type", "game_mode",
    "sv_cheats", "bot_quota", "bot_stop", "bot_dont_shoot", "mp_autoteambalance",
    "mp_limitteams", "mp_ignore_round_win_conditions", "mp_freezetime",
    "mp_roundtime_defuse", "sv_infinite_ammo", "mp_respawn_on_death_ct", "mp_respawn_on_death_t"
};
