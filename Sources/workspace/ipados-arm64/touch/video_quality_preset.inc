// Included inside I5Touch. Preserve the accepted high recipe; world-only
// sampling and map-relative CSM coverage provide gradual lower-tier budgets.
struct QualitySetting { const char *name; const char *value; };
struct QualityBudget { int worldTexturePercent; int worldShadowPercent; };
static const QualityBudget s_qualityBudget[] = { {50,50}, {70,70}, {100,65}, {100,100} };
static const QualitySetting s_highQuality[] = {
    { "cpu_level", "0" }, { "gpu_level", "0" }, { "gpu_mem_level", "1" },
    { "mem_level", "3" }, { "csm_quality_level", "1" },
    { "mat_antialias", "0" }, { "mat_aaquality", "0" },
    { "mat_forceaniso", "4" }, { "mat_vsync", "0" },
    { "mat_triplebuffered", "0" }, { "mat_queue_mode", "0" },
    { "mat_grain_scale_override", "-1" }, { "mat_software_aa_strength", "0" },
    { "mat_monitorgamma", "2.2" }, { "mat_monitorgamma_tv_enabled", "0" },
    { "mat_powersavingsmode", "0" }, { "icsm_world_bump_detail", "1" }
};
static bool s_qualityChanging = false;
static ICSMDisplay::Size s_qualityUIOutput = {0, 0};
static double s_qualityDisplayCheck = 0;
static ConVar icsm_quality_high_width( "icsm_quality_high_width", "0", FCVAR_ARCHIVE,
    "Saved per-device high-tier output width; zero captures the existing configuration.", true, 0, true, 16384 );
static ConVar icsm_quality_high_height( "icsm_quality_high_height", "0", FCVAR_ARCHIVE,
    "Saved per-device high-tier output height; zero captures the existing configuration.", true, 0, true, 16384 );
// Archived calibration belongs to each device's config. Existing iPad high
// values and the established lower-tier recipes retain their original defaults.
static ConVar icsm_quality_high_scene_percent( "icsm_quality_high_scene_percent", "100", FCVAR_ARCHIVE | FCVAR_RELEASE,
    "Calibrated high-tier scene scale for this device.", true, 50, true, 100 );
static ConVar icsm_quality_high_shadow_percent( "icsm_quality_high_shadow_percent", "100", FCVAR_ARCHIVE | FCVAR_RELEASE,
    "Calibrated high-tier map-relative world shadow coverage for this device.", true, 50, true, 100 );
static ConVar icsm_quality_high_world_bump( "icsm_quality_high_world_bump", "1", FCVAR_ARCHIVE | FCVAR_RELEASE,
    "Per-device high-tier world normal mapping; independent skin shading is preserved.", true, 0, true, 1 );
static void ApplyQualityPreset();
static void QualityPresetChanged( IConVar *, const char *, float )
{
    if ( !s_qualityChanging && s_qualityReady ) ApplyQualityPreset();
}
static ConVar icsm_quality_preset( "icsm_quality_preset", "3", FCVAR_ARCHIVE,
    "Graphics preset: 0 very low, 1 low, 2 medium, 3 high.",
    true, 0, true, 3, QualityPresetChanged );
static float QualitySettingValue(const QualitySetting &setting) {
    if (!V_strcmp(setting.name,"icsm_world_bump_detail")) {
        if (icsm_quality_preset.GetInt()==2) return 0.0f;
        if (icsm_quality_preset.GetInt()==3) return float(icsm_quality_high_world_bump.GetInt());
    }
    return float(atof(setting.value));
}
static ICSMDisplay::Size QualityOutput()
{
    ICSMDisplay::Size high = { unsigned( icsm_quality_high_width.GetInt() ), unsigned( icsm_quality_high_height.GetInt() ) };
    const int tier = icsm_quality_preset.GetInt();
    return tier == 3 ? high : tier == 2 ? ICSMDisplay::PhoneMedium(high)
        : ICSMDisplay::Preset(high,720,tier != 1);
}
static ICSMDisplay::Size QualityUIOutput()
{
    // Query the actual Metal drawable, including window size and display scale.
    // RenderedSize is Source's backbuffer size, so cannot be used for this.
    static bool (*display)(unsigned *, unsigned *, unsigned *) =
        reinterpret_cast<bool (*)(unsigned *, unsigned *, unsigned *)>(
            dlsym(RTLD_DEFAULT, "SourceMetalUIKitDisplay"));
    ICSMDisplay::Size native = {0, 0}; unsigned hz = 0;
    if ( display && display(&native.width, &native.height, &hz) ) return native;
    int width=0,height=0; if(engine)engine->GetScreenSize(width,height);
    return {unsigned(MAX(0,width)), unsigned(MAX(0,height))};
}
static ICSMDisplay::SceneResolution QualityScenePlan(ICSMDisplay::Size ui)
{
    ConVarRef scene("icsm_scene_render_percent", true);
    return ICSMDisplay::ScenePlan(QualityOutput(), scene.IsValid()?scene.GetFloat():100.0f, ui);
}
static void ApplyQualityPreset()
{
    if ( !s_qualityReady || s_qualityChanging || !engine ) return;
    s_qualityChanging = true;
    int width = 0, height = 0; engine->GetScreenSize( width, height );
    if ( !icsm_quality_high_width.GetInt() || !icsm_quality_high_height.GetInt() ) {
        icsm_quality_high_width.SetValue( width ); icsm_quality_high_height.SetValue( height );
    }
    const int tier = icsm_quality_preset.GetInt();
    bool changed = false;
    for ( unsigned i = 0; i < ARRAYSIZE( s_highQuality ); ++i ) {
        const QualitySetting &setting = s_highQuality[i];
        ConVarRef value( setting.name, true );
        if ( !value.IsValid() ) { Warning( "ICSM_QUALITY_PRESET missing=%s\n", setting.name ); continue; }
        const float expected = QualitySettingValue(setting);
        if ( value.GetFloat() != expected ) { value.SetValue( expected ); changed = true; }
        char optionName[96]; V_snprintf( optionName, sizeof( optionName ), "%s_optionsui", setting.name );
        ConVarRef option( optionName, true );
        if ( option.IsValid() && option.GetFloat() != expected ) option.SetValue( expected );
    }
    QualityBudget budget = s_qualityBudget[tier];
    if ( tier == 3 ) budget.worldShadowPercent = icsm_quality_high_shadow_percent.GetInt();
    const char *budgetNames[] = { "icsm_world_texture_percent", "icsm_world_shadow_percent" };
    const int budgetValues[] = { budget.worldTexturePercent, budget.worldShadowPercent };
    for ( unsigned i = 0; i < ARRAYSIZE(budgetNames); ++i ) {
        ConVarRef value(budgetNames[i],true);
        if ( !value.IsValid() ) { Warning("ICSM_QUALITY_PRESET missing=%s\n",budgetNames[i]); continue; }
        if ( value.GetInt() != budgetValues[i] ) { value.SetValue(budgetValues[i]); changed=true; }
    }
    ConVarRef scene( "icsm_scene_render_percent", true ), spatial( "icsm_metal_spatial_upscale", true );
    // Source truncates scaled integer view dimensions. Round the percentage
    // slightly upward so the float product remains exactly 480/720 pixels.
    if ( scene.IsValid() ) scene.SetValue( tier == 3 ? icsm_quality_high_scene_percent.GetFloat() : tier == 0 ? 66.66667f : 100.0f );
    if ( spatial.IsValid() ) spatial.SetValue( tier == 0 || tier == 3 );
    // Validated Build 144 submission optimizations preserve the depth atlas,
    // geometry and ordering. Apply them to all presets; keep the unproven
    // material-detail experiment disabled. Never touch FPS, motion blur or
    // the player's independent weapon-quality selections here.
    const QualitySetting optimizations[] = {
        {"icsm_shadow_bucket_cache","1"}, {"icsm_metal_csm_discard_dummy","1"},
        {"icsm_map_detail_optimize","0"}
    };
    for (unsigned i=0;i<ARRAYSIZE(optimizations);++i) {
        ConVarRef value(optimizations[i].name,true);
        if(value.IsValid() && value.GetInt()!=atoi(optimizations[i].value)) {
            value.SetValue(atoi(optimizations[i].value));changed=true;
        }
    }
    if ( tier == 0 ) {
        // On restart the saved effective cap is already 60; retain the archived
        // unlocked preference rather than overwriting it with that forced cap.
        if ( !s_qualityFrameLocked && icsm_fps_limit.GetInt() != 60 )
            icsm_quality_unlocked_fps.SetValue( icsm_fps_limit.GetInt() );
        s_qualityFrameLocked = true;
        icsm_fps_limit.SetValue( 60 ); ApplyFrameRateLimit( 60 );
    } else if ( s_qualityFrameLocked ) {
        s_qualityFrameLocked = false;
        icsm_fps_limit.SetValue( icsm_quality_unlocked_fps.GetInt() );
        ApplyFrameRateLimit( icsm_fps_limit.GetInt() );
    }
    const ICSMDisplay::Size recipe = QualityOutput();
    ConVarRef sceneWidth("icsm_scene_output_width", true), sceneHeight("icsm_scene_output_height", true);
    if ( sceneWidth.IsValid() ) sceneWidth.SetValue(int(recipe.width));
    if ( sceneHeight.IsValid() ) sceneHeight.SetValue(int(recipe.height));
    // Only the UI/backbuffer follows the drawable. Scene dimensions retain the
    // established preset budget; lower presets no longer shrink glyphs.
    const ICSMDisplay::Size target = QualityUIOutput();
    s_qualityUIOutput = target;
    if ( target.width && target.height && ( target.width != unsigned(width) || target.height != unsigned(height) ) ) {
        char command[256];
        V_snprintf( command, sizeof(command), "mat_setvideomode %u %u 0 0; mat_savechanges; host_writeconfig\n", target.width, target.height );
        engine->ClientCmd_Unrestricted( command );
    } else if ( changed ) engine->ClientCmd_Unrestricted( "mat_savechanges; host_writeconfig\n" );
    s_qualityChanging = false;
    const ICSMDisplay::SceneResolution plan = QualityScenePlan(target);
    Msg( "ICSM_QUALITY_PRESET selected=%d ui=%ux%u scene_input=%ux%u scene_output=%ux%u scene_percent=%g fps=%d locked=%d\n", tier,
        target.width, target.height, plan.input.width, plan.input.height, plan.output.width, plan.output.height,
        scene.IsValid() ? scene.GetFloat() : 0, icsm_fps_limit.GetInt(), s_qualityFrameLocked );
}
static void CheckQualityDisplaySize()
{
    if ( !s_qualityReady || s_qualityChanging ) return;
    const double now=Plat_FloatTime();
    if ( now < s_qualityDisplayCheck ) return;
    s_qualityDisplayCheck=now+1.0;
    const ICSMDisplay::Size native = QualityUIOutput();
    if ( native.width && native.height &&
         ( native.width != s_qualityUIOutput.width || native.height != s_qualityUIOutput.height ) )
        ApplyQualityPreset();
}
static void ApplyQualityPresetCommand( const CCommand & )
{
    ApplyQualityPreset();
    if ( engine ) engine->ClientCmd_Unrestricted( "host_writeconfig\n" );
}
static ConCommand icsm_apply_quality_preset( "icsm_apply_quality_preset", ApplyQualityPresetCommand,
    "Apply the graphics preset, preserving independent player options.", FCVAR_CLIENTCMD_CAN_EXECUTE );
static void QualityPresetStateCommand( const CCommand & )
{
    const ICSMDisplay::Size output = QualityUIOutput();
    int width=0,height=0; if(engine)engine->GetScreenSize(width,height);
    Msg( "ICSM_QUALITY_PRESET_STATE selected=%d target=%ux%u actual=%dx%d high=%dx%d fps=%d unlocked_fps=%d locked=%d\n",
        icsm_quality_preset.GetInt(), output.width, output.height, width, height,
        icsm_quality_high_width.GetInt(), icsm_quality_high_height.GetInt(), icsm_fps_limit.GetInt(),
        icsm_quality_unlocked_fps.GetInt(), s_qualityFrameLocked );
    const ICSMDisplay::SceneResolution plan=QualityScenePlan(output);
    Msg("ICSM_UI_RESOLUTION ui=%ux%u scene_input=%ux%u scene_output=%ux%u independent=1\n",
        output.width,output.height,plan.input.width,plan.input.height,plan.output.width,plan.output.height);
    for ( unsigned i = 0; i < ARRAYSIZE( s_highQuality ); ++i ) {
        ConVarRef value( s_highQuality[i].name, true );
        const float expected = QualitySettingValue(s_highQuality[i]);
        if ( value.IsValid() ) Msg( "ICSM_QUALITY_VALUE name=%s actual=%g expected=%g\n",
            s_highQuality[i].name, value.GetFloat(), expected );
    }
    ConVarRef texture("icsm_world_texture_percent",true), shadow("icsm_world_shadow_percent",true);
    Msg("ICSM_QUALITY_BUDGET texture_percent=%g shadow_percent=%g expected_texture=%d expected_shadow=%d\n",
        texture.IsValid()?texture.GetFloat():0,shadow.IsValid()?shadow.GetFloat():0,
        s_qualityBudget[icsm_quality_preset.GetInt()].worldTexturePercent,
        icsm_quality_preset.GetInt() == 3 ? icsm_quality_high_shadow_percent.GetInt() : s_qualityBudget[icsm_quality_preset.GetInt()].worldShadowPercent);
}
static ConCommand icsm_quality_preset_state( "icsm_quality_preset_state", QualityPresetStateCommand,
    "Report the actual graphics preset values.", FCVAR_DONTRECORD );
