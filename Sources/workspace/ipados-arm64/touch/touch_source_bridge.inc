/* I5 native touch bridge, included by the derived in_main.cpp after the
   original IN_* handlers.  Native callbacks enqueue POD events only: the
   original client context applies edges and constructs CUserCmds. */

#include "display_policy.h"
#include "scene_resolution_policy.h"
#include "touch_aim_policy.h"
#include "cstrike15/panorama/popups/csgo_settings_slider.h"
#include "panorama/controls/slider.h"
#include <dlfcn.h>
namespace I5Touch
{
enum EventType { ActionEvent, MoveEvent, LookEvent, CommandEvent, WeaponEvent, ResetEvent };
struct Event
{
    EventType type;
    int code;
    int value;
    float x, y;
    double timestamp;
    unsigned epoch;
    double sampleInterval;
};
struct LookSample { float x, y; double timestamp, interval; };
static const unsigned kCapacity = 16384;
static Event s_events[kCapacity];
static unsigned s_read = 0, s_write = 0, s_epoch = 0;
static LookSample s_look[kCapacity];
static unsigned s_lookCount = 0;
static float s_moveX = 0.0f, s_moveY = 0.0f;
static bool s_held[SourceTouchActionCount] = {};
static bool s_touched[SourceTouchActionCount] = {};
static bool s_initialized = false, s_scoreHeld = false, s_pumping = false;
static bool s_gameInput = false;
static bool s_pointerUI = false, s_teamMenu = false, s_endOfMatch = false;
static unsigned s_lookSamples = 0, s_commands = 0, s_resets = 0;
static unsigned s_inspectCommands = 0, s_dropCommands = 0;
static double s_yawTotal = 0.0, s_pitchTotal = 0.0;
static SourceTouchState s_state = {};
static CUserCmd s_lastCommand;
static CUserCmd s_lastExtraCommand;
static unsigned s_realCommands = 0, s_extraCommands = 0, s_actionEdges = 0;

// These are degrees per logical UIKit point.  They intentionally do not use
// mouse sensitivity, m_yaw/m_pitch, FOV scaling or frame time. Optional native
// sensitivity/velocity gain below uses only actual touch sample timing.
static ConVar touch_yaw_degrees_per_point( "touch_yaw_degrees_per_point", "0.25",
    FCVAR_ARCHIVE, "Native touch yaw degrees per UIKit point; linear, no acceleration.", true, 0.001f, true, 5.0f );
static ConVar touch_pitch_degrees_per_point( "touch_pitch_degrees_per_point", "0.25",
    FCVAR_ARCHIVE, "Native touch pitch degrees per UIKit point; linear, no acceleration.", true, 0.001f, true, 5.0f );
static ConVar touch_input_debug( "touch_input_debug", "0", 0,
    "Log native touch actions and applied look totals." );
static ConVar icsm_touch_aim_mode( "icsm_touch_aim_mode", "0", FCVAR_ARCHIVE,
    "Touch aim: 0=fixed speed, 1=actual finger velocity acceleration (up to 2x).", true, 0, true, 1 );
static ConVar icsm_touch_hide_buttons( "icsm_touch_hide_buttons", "0", FCVAR_ARCHIVE,
    "Hide native gameplay touch buttons and their hit targets; preserve swipe and menu input.", true, 0, true, 1 );
static ConVar icsm_touch_sensitivity( "icsm_touch_sensitivity", "1", FCVAR_ARCHIVE,
    "Overall native touch aim multiplier, including firing.", true, 0.1f, true, 3.0f );
static ConVar icsm_touch_rifle_scope_sensitivity( "icsm_touch_rifle_scope_sensitivity", "1", FCVAR_ARCHIVE,
    "AUG/SG553 scoped touch multiplier relative to overall sensitivity, including firing.", true, 0.1f, true, 3.0f );
static ConVar icsm_touch_sniper_scope_sensitivity( "icsm_touch_sniper_scope_sensitivity", "1", FCVAR_ARCHIVE,
    "Sniper scoped touch multiplier relative to overall sensitivity, including firing.", true, 0.1f, true, 3.0f );
static ICSMTouchAim::Delta s_lastAim = {0, 0, 1, 1};

#include "controller_settings.inc"
#include "controller_source_trace.inc"

static ICSMTouchAim::Settings AimSettings() {
    ICSMTouchAim::Settings result = {icsm_touch_aim_mode.GetInt(),
        icsm_touch_sensitivity.GetFloat(), icsm_touch_rifle_scope_sensitivity.GetFloat(),
        icsm_touch_sniper_scope_sensitivity.GetFloat()};
    return result;
}
static ICSMTouchAim::Scope AimScope() {
    C_CSPlayer *player = C_CSPlayer::GetLocalCSPlayer();
    C_WeaponCSBase *weapon = player ? player->GetActiveCSWeapon() : NULL;
    if (!weapon) return ICSMTouchAim::Unscoped;
    return ICSMTouchAim::SelectScope(player->m_bIsScoped,
        weapon->GetCSWeaponID() == WEAPON_AUG || weapon->GetCSWeaponID() == WEAPON_SG556,
        weapon->GetWeaponType() == WEAPONTYPE_SNIPER_RIFLE);
}

// Original fps_max and fps_max_menu are not archived. Keep the UI choice in
// an archived client ConVar and apply both original engine limiters.
static bool s_qualityFrameLocked = false;
static bool s_qualityFPSChanging = false;
static bool s_qualityReady = false;
static ConVar icsm_quality_unlocked_fps( "icsm_quality_unlocked_fps", "120", FCVAR_ARCHIVE,
    "Last unlocked preset frame cap, restored when leaving very low.", true, 60, true, 120 );
static void ApplyFrameRateLimit( int limit )
{
    ConVarRef game( "fps_max", true ), menu( "fps_max_menu", true );
    if ( !game.IsValid() || !menu.IsValid() ) return;
    game.SetValue( limit );
    // Static lobby imagery does not need the match's 90/120 Hz scene cadence.
    // Native UIKit controls retain their independent system animation rate.
    menu.SetValue( MIN(limit,60) );
    Msg( "ICSM_FPS_LIMIT selected=%d game=%d menu=%d\n", limit, game.GetInt(), menu.GetInt() );
}

static void FrameRateLimitChanged( IConVar *var, const char *, float )
{
    ConVar *choice = static_cast<ConVar *>( var );
    if ( s_qualityFPSChanging ) return;
    if ( s_qualityFrameLocked && choice->GetInt() != 60 ) {
        s_qualityFPSChanging = true; choice->SetValue( 60 ); s_qualityFPSChanging = false;
    } else if ( s_qualityReady && !s_qualityFrameLocked ) icsm_quality_unlocked_fps.SetValue( choice->GetInt() );
    ApplyFrameRateLimit( choice->GetInt() );
}

static ConVar icsm_fps_limit( "icsm_fps_limit", "120", FCVAR_ARCHIVE,
    "Saved frame rate limit for the lobby and gameplay.", true, 60.0f, true, 120.0f, FrameRateLimitChanged );

static void ApplyFrameRateLimitCommand( const CCommand & )
{
    ApplyFrameRateLimit( icsm_fps_limit.GetInt() );
}
static ConCommand icsm_apply_fps_limit( "icsm_apply_fps_limit", ApplyFrameRateLimitCommand,
    "Apply the saved frame rate limit after startup configuration.", FCVAR_CLIENTCMD_CAN_EXECUTE );

#include "video_quality_preset.inc"

static kbutton_t *Button( int action )
{
    switch ( action )
    {
    case SourceTouchAttack: return &in_attack;
    case SourceTouchAttack2: return &in_attack2;
    case SourceTouchDuck: return &in_duck;
    case SourceTouchJump: return &in_jump;
    case SourceTouchReload: return &in_reload;
    case SourceTouchUse: return &in_use;
    case SourceTouchWalk: return &in_speed;
    default: return NULL;
    }
}

static const char *KeyId( int action )
{
    // Distinct from physical Source ButtonCode_t values and the console's -1.
    static const char *ids[SourceTouchActionCount] = {
        "-55001", "-55002", "-55003", "-55004", "-55005", "-55006", "-55007"
    };
    return action >= 0 && action < SourceTouchActionCount ? ids[action] : "-55000";
}

static bool ScoreboardOpen()
{
    CCSGO_Scoreboard *score = CCSGO_Scoreboard::GetInstance();
    return score && score->BIsVisible();
}

static bool EndOfMatchOpen()
{
    CCSGO_EndOfMatch *end = CCSGO_EndOfMatch::GetInstance();
    return end && end->IsOpen();
}

static void ReleaseGameActions()
{
    s_moveX = s_moveY = 0.0f;
    s_lookCount = 0;
    for ( int a = 0; a < SourceTouchActionCount; ++a )
    {
        kbutton_t *button = Button( a );
        if ( s_held[a] ) KeyUp( button, KeyId( a ) );
        s_held[a] = false;
        // Cancelling an unsent quick tap must not leave impulse-down behind.
        // Preserve any physical key which still owns the same original button.
        kbutton_t::Split_t &key = button->GetPerUser();
        if ( s_touched[a] && !key.down[0] && !key.down[1] ) key.state = 4;
        s_touched[a] = false;
    }
    ++s_resets;
}

static void Enqueue( EventType type, int code, int value, float x, float y, double timestamp, double interval = 0 )
{
    if ( s_write - s_read >= kCapacity )
    {
        // A bounded queue cannot retain stale holds following overflow.
        s_read = s_write;
        ++s_epoch;
        Event reset = { ResetEvent, 0, 0, 0, 0, timestamp, s_epoch };
        s_events[s_write++ % kCapacity] = reset;
        Warning( "I5_TOUCH queue overflow: reset pending input\n" );
    }
    Event event = { type, code, value, x, y, timestamp, s_epoch, interval };
    s_events[s_write++ % kCapacity] = event;
}

static void ActionCallback( void *, SourceTouchAction action, bool pressed, double time )
{ Enqueue( ActionEvent, action, pressed ? 1 : 0, 0, 0, time ); }
static void MoveCallback( void *, float x, float y, double time )
{ Enqueue( MoveEvent, 0, 0, x, y, time ); }
static void LookCallback( void *, float x, float y, double time )
{ Enqueue( LookEvent, 0, 0, x, y, time ); }
static void LookSampleCallback( void *, float x, float y, double time, double interval )
{ Enqueue( LookEvent, 0, 0, x, y, time, interval ); }
static void CommandCallback( void *, SourceTouchCommand command, double time )
{ Enqueue( CommandEvent, command, 0, 0, 0, time ); }
static void WeaponCallback( void *, int entity, int subtype, double time )
{ Enqueue( WeaponEvent, entity, subtype, 0, 0, time ); }
static void ResetCallback( void *, double time )
{
    ++s_epoch;
    Enqueue( ResetEvent, 0, 0, 0, 0, time );
}

static SourceTouchRect PanelRect( panorama::IUIPanel *panel, int renderWidth, int renderHeight )
{
    SourceTouchRect result = {};
    if ( !panel || !panel->BIsVisible() || !panel->GetParentWindow() ) return result;
    const float surfaceW = float( panel->GetParentWindow()->GetSurfaceWidth() );
    const float surfaceH = float( panel->GetParentWindow()->GetSurfaceHeight() );
    if ( surfaceW <= 0 || surfaceH <= 0 ) return result;
    float x = 0, y = 0;
    for ( panorama::IUIPanel *p = panel; p; p = p->GetParent() )
    {
        x += p->GetActualXOffset();
        y += p->GetActualYOffset();
        if ( p->GetParent() )
        {
            x -= p->GetParent()->GetContentsXScrollOffset();
            y -= p->GetParent()->GetContentsYScrollOffset();
        }
    }
    // CUIPanel layout dimensions already include propagated ui-scale.  HUD
    // radar/teamcounter use layout positioning, not a rotated CSS transform.
    result.x = x * renderWidth / surfaceW;
    result.y = y * renderHeight / surfaceH;
    result.width = panel->GetActualLayoutWidth() * renderWidth / surfaceW;
    result.height = panel->GetActualLayoutHeight() * renderHeight / surfaceH;
    return result;
}

static bool PanoramaPointerUI()
{
    // TeamSelect uses k_EGameInputShareMouse: the original engine gives
    // Panorama the pointer even though the engine GameUI is not visible.
    return gameuifuncs && gameuifuncs->PanoramaDeniesInputToGame( panorama::k_EGameInputUIEnableMouseCursor );
}

static bool TeamSelectionOpen()
{
    CCSGO_Hud *hud = engine && engine->IsInGame() ? CCSGO_Hud::GetInstance() : NULL;
    panorama::IUIPanel *team = hud ? hud->UIPanel()->FindChildTraverse( "TeamSelectMenu" ) : NULL;
    return team && team->BIsVisible();
}

static bool GameplayAllowed( C_CSPlayer *player )
{
    if ( SourceTouchIsEditing() || !engine || !engine->IsInGame() || engine->IsPaused() || !player ) return false;
    if ( enginevgui && enginevgui->IsGameUIVisible() ) return false;
    if ( player->IsBuyMenuOpen() || ScoreboardOpen() || EndOfMatchOpen() ) return false;
    if ( PanoramaPointerUI() || TeamSelectionOpen() ) return false;
    if ( !g_pInputStackSystem ) return false;
    const InputContextHandle_t context = engine->GetInputContext( ENGINE_INPUT_CONTEXT_GAME );
    return context != INPUT_CONTEXT_HANDLE_INVALID &&
        g_pInputStackSystem->IsTopmostEnabledContext( context ) &&
        !::input->CAM_InterceptingMouse();
}

static const char *s_navigationIDs[SourceNavigationActionCount] = {
    "MainMenuNavBarHome", "MainMenuNavBarPlay", "MainMenuNavBarInventory",
    "MainMenuNavBarSettings", "MainMenuNavBarConsole", "MainMenuNavBarRooms",
    "MainMenuNavBarFPS", "MainMenuNavBarQuit"
};

static bool LobbyVisible( panorama::IUIPanel *root )
{
    // InitialDisplay is never cleared in this tree (see MainMenuReady).
    // Use actual panel readiness rather than that legacy class.
    return engine && !engine->IsInGame() &&
        GameUI().GetGameUIState() == CSGO_GAME_UI_STATE_MAINMENU &&
        root && root->BIsVisible() && root->BReadyForDisplay();
}

static bool NavigationAllowed( panorama::IUIPanel *root )
{
    return LobbyVisible( root ) && !root->BHasClass( "PopupBackgroundBlur" ) && !SourceTouchIsEditing();
}

static panorama::CPanelPtr<panorama::IUIPanel> s_menuBlurPanel;
static int s_menuBlurSurfaceWidth = 0, s_menuBlurSurfaceHeight = 0;
static float s_menuBlurScaleX = 0, s_menuBlurScaleY = 0;
static float s_menuBlurParentX = 0, s_menuBlurParentY = 0;
static float s_menuContentScale = 0;
static unsigned s_menuBlurLayoutUpdates = 0;

static void UpdateMenuBlurViewport( panorama::IUIPanel *root )
{
    if ( !root || !root->BIsVisible() || !root->BReadyForDisplay() || !root->GetParentWindow() ) return;
    panorama::IUIPanel *mask = root->FindChildTraverse( "ICSMMenuBlur" );
    panorama::IUIPanel *parent = mask ? mask->GetParent() : NULL;
    if ( !parent ) return;
    const int width = root->GetParentWindow()->GetSurfaceWidth();
    const int height = root->GetParentWindow()->GetSurfaceHeight();
    const float scaleX = parent->GetActualUIScaleX(), scaleY = parent->GetActualUIScaleY();
    if ( width <= 0 || height <= 0 || scaleX <= 0 || scaleY <= 0 ) return;
    float parentX = 0, parentY = 0;
    for ( panorama::IUIPanel *p = parent; p; p = p->GetParent() )
    {
        if ( p->GetActualXOffset() == FLT_MAX || p->GetActualYOffset() == FLT_MAX ) return;
        parentX += p->GetActualXOffset();parentY += p->GetActualYOffset();
        if ( p->GetParent() )
        {
            parentX -= p->GetParent()->GetContentsXScrollOffset();
            parentY -= p->GetParent()->GetContentsYScrollOffset();
        }
    }
    if ( !IsFinite( parentX ) || !IsFinite( parentY ) ) return;
    if ( s_menuBlurPanel.Get() == mask && s_menuBlurSurfaceWidth == width && s_menuBlurSurfaceHeight == height &&
        s_menuBlurScaleX == scaleX && s_menuBlurScaleY == scaleY &&
        s_menuBlurParentX == parentX && s_menuBlurParentY == parentY &&
        s_menuContentScale == SourceTouchMenuUIScale() ) return;

    // Work in the window surface, not percentage widths of a clipped content
    // panel. Extend one surface pixel past every outward-rounded edge, so
    // UI-scale rounding and the rasterizer cannot leave a clear final column.
    const float x = floorf( -parentX ) - 1, y = floorf( -parentY ) - 1;
    const float pixelWidth = ceilf( float( width ) - parentX ) + 1 - x;
    const float pixelHeight = ceilf( float( height ) - parentY ) + 1 - y;
    panorama::IUIPanelStyle *style = mask->AccessIUIStyleDirty();
    const panorama::CUILength::EUILengthTypes pixels = panorama::CUILength::k_EUILengthLength;
    // A collapsed mask has no actual UI scale yet. Supply logical lengths
    // using its laid-out parent, rather than reversing the mask's unset scale.
    style->SetPositionWithoutTransition( panorama::CUILength( x / scaleX, pixels ), panorama::CUILength( y / scaleY, pixels ),
        panorama::CUILength( 0, pixels ), false );
    style->SetWidthWithoutTransition( panorama::CUILength( pixelWidth / scaleX, pixels ) );
    style->SetHeightWithoutTransition( panorama::CUILength( pixelHeight / scaleY, pixels ) );
    // A 100% height plus bottom margin does not shrink an absolute panel.
    // Reserve the navigation row in its actual height so page footers (the
    // play/start and inventory/confirm controls) remain above the glass.
    panorama::IUIPanel *content = root->FindChildTraverse( "JsMainMenuContent" );
    if ( content )
    {
        const float menuScale = SourceTouchMenuUIScale();
        const float contentPixels = MAX( 0.0f, float( height ) - parentY - 192.0f * scaleY );
        panorama::IUIPanelStyle *contentStyle = content->AccessIUIStyleDirty();
        // Source's ui-scale operates at layout/font level. Compensate the
        // logical viewport so only its contents shrink, while the page and
        // shared blur still cover the same screen rectangle. Actual panel
        // rectangles keep the native map/settings touch overlays aligned.
        contentStyle->SetUIScale( Vector( menuScale, menuScale, menuScale ) );
        contentStyle->SetWidthWithoutTransition( panorama::CUILength( ( float( width ) - parentX ) / ( scaleX * menuScale ), pixels ) );
        contentStyle->SetHeightWithoutTransition( panorama::CUILength( contentPixels / ( scaleY * menuScale ), pixels ) );
    }
    s_menuBlurPanel = mask;s_menuBlurSurfaceWidth = width;s_menuBlurSurfaceHeight = height;
    s_menuBlurScaleX = scaleX;s_menuBlurScaleY = scaleY;s_menuBlurParentX = parentX;s_menuBlurParentY = parentY;
    s_menuContentScale = SourceTouchMenuUIScale();
    ++s_menuBlurLayoutUpdates;
    Msg( "ICSM_MENU_BLUR_VIEWPORT surface=%dx%d ui_scale=%g,%g parent=%g,%g mask_pixels=%g,%g,%g,%g updates=%u\n",
        width, height, scaleX, scaleY, parentX, parentY, x, y, pixelWidth, pixelHeight, s_menuBlurLayoutUpdates );
}

static void NavigationCallback( SourceTouchNavigationAction action )
{
    if ( action <= SourceNavigationHome || action >= SourceNavigationQuit || !s_initialized ) return;
    CCSGO_MainMenu *menu = CCSGO_MainMenu::GetInstance();
    panorama::IUIPanel *root = menu ? menu->UIPanel() : NULL;
    if ( !NavigationAllowed( root ) ) return;
    panorama::IUIPanel *button = root->FindChildTraverse( s_navigationIDs[action] );
    if ( !button || !button->BIsVisible() || !button->IsEnabled() ) return;
    // With the home glyph removed, reselecting an open page uses the original
    // home radio's handler to close it and restore lobby music/selection.
    if ( action >= SourceNavigationPlay && action <= SourceNavigationSettings &&
        ( button->GetStyleFlags() & panorama::k_EStyleFlagSelected ) )
    {
        panorama::IUIPanel *home = root->FindChildTraverse( "MainMenuNavBarHome" );
        if ( home ) button = home;
    }
    // Use the same Activated event as CUIWindowInput::OnMouseButtonUp. This
    // retains radio selection, XML onactivate handlers and quit confirmation.
    panorama::DispatchEventAsync( 0.0f, panorama::Activated(), button->ClientPtr(), panorama::k_ePanelEventSourceMouse );
    Msg( "ICSM_GLASS_NAV activate=%s\n", s_navigationIDs[action] );
}


static void ProfileCallback()
{
    if ( !s_initialized ) return;
    CCSGO_MainMenu *menu = CCSGO_MainMenu::GetInstance();
    panorama::IUIPanel *root = menu ? menu->UIPanel() : NULL;
    if ( !NavigationAllowed( root ) ) return;
    panorama::IUIPanel *button = root->FindChildTraverse( "ICSMProfile" );
    if ( !button || !button->BIsVisible() || !button->IsEnabled() ) return;
    // Retain the original XML handler and native name editor/save path.
    panorama::DispatchEventAsync( 0.0f, panorama::Activated(), button->ClientPtr(), panorama::k_ePanelEventSourceMouse );
    Msg( "ICSM_GLASS_PROFILE activate=ICSMProfile\n" );
}

static panorama::IUIPanel *ActiveMapContainer()
{
    CCSGO_MainMenu *menu = CCSGO_MainMenu::GetInstance();
    panorama::IUIPanel *root = menu ? menu->UIPanel() : NULL;
    if ( !NavigationAllowed( root ) || SourceTouchConsoleIsVisible() ) return NULL;
    panorama::IUIPanel *play = root->FindChildTraverse( "MainMenuNavBarPlay" );
    if ( !play || !( play->GetStyleFlags() & panorama::k_EStyleFlagSelected ) ) return NULL;
    // Original drop-down menus are separate floating panels. Let their
    // original pointer route receive touches while any play menu is open.
    const char *dropdowns[] = { "PlayTopNavDropdown", "BotDifficultyDropdown", "TournamentTeamDropdown", "TournamentStageDropdown" };
    for ( unsigned i = 0; i < ARRAYSIZE( dropdowns ); ++i )
    {
        panorama::IUIPanel *p = root->FindChildTraverse( dropdowns[i] );
        if ( p && p->BHasClass( "DropDownMenuVisible" ) ) return NULL;
    }
    panorama::IUIPanel *list = root->FindChildTraverse( "MapSelectionList" );
    const char *id = list ? list->GetAttribute( "icsm-active-map-container", "" ) : "";
    panorama::IUIPanel *pane = id[0] ? list->FindChildTraverse( id ) : NULL;
    return pane && pane->BIsVisible() && pane->IsEnabled() && !pane->BHasClass( "hidden" ) ? pane : NULL;
}

static void MapScrollCallback( float pixels )
{
    panorama::IUIPanel *pane = s_initialized ? ActiveMapContainer() : NULL;
    if ( !pane || !IsFinite( pixels ) || !pane->GetParentWindow() ) return;
    panorama::IUIScrollBar *scroll = pane->GetHorizontalScrollBar();
    int width = 0, height = 0;engine->GetScreenSize( width, height );
    if ( scroll && width > 0 )
        scroll->SetScrollWindowPosition( pixels * pane->GetParentWindow()->GetSurfaceWidth() / width, true );
}

static void MapActivateCallback( const char *id )
{
    panorama::IUIPanel *pane = s_initialized ? ActiveMapContainer() : NULL;
    panorama::IUIPanel *tile = pane && id ? pane->FindChildTraverse( id ) : NULL;
    if ( !tile || !tile->BIsVisible() || !tile->IsEnabled() || !tile->BHasClass( "map-selection-btn" ) ) return;
    // Use the original radio/toggle and its original onactivate handler.
    panorama::DispatchEventAsync( 0.0f, panorama::Activated(), tile->ClientPtr(), panorama::k_ePanelEventSourceMouse );
    Msg( "ICSM_MAP_GALLERY activate=%s map=%s\n", id, tile->GetAttribute( "mapname", "" ) );
}

static SourceTouchRect MapPanelRect( panorama::IUIPanel *panel, int width, int height )
{
    SourceTouchRect r = {};
    if ( !panel || !panel->GetParentWindow() || !panel->BIsVisible() ) return r;
    const float surfaceW = panel->GetParentWindow()->GetSurfaceWidth();
    const float surfaceH = panel->GetParentWindow()->GetSurfaceHeight();
    if ( surfaceW <= 0 || surfaceH <= 0 ) return r;
    float x = 0, y = 0;
    for ( panorama::IUIPanel *p = panel; p; p = p->GetParent() )
    {
        if ( p->GetActualXOffset() == FLT_MAX || p->GetActualYOffset() == FLT_MAX ) return r;
        x += p->GetActualXOffset();y += p->GetActualYOffset();
        if ( p->GetParent() )
        {
            // Source stores content scroll offsets as NEGATIVE positions
            // (CUIPanel::CalculateScrollBarPosition), matching PaintTraverse.
            x += p->GetParent()->GetContentsXScrollOffset();
            y += p->GetParent()->GetContentsYScrollOffset();
        }
    }
    r.x = x * width / surfaceW;r.y = y * height / surfaceH;
    r.width = panel->GetActualLayoutWidth() * width / surfaceW;
    r.height = panel->GetActualLayoutHeight() * height / surfaceH;
    if ( !IsFinite( r.x ) || !IsFinite( r.y ) || !IsFinite( r.width ) || !IsFinite( r.height ) ) return SourceTouchRect();
    return r;
}

static void PublishMaps( int width, int height )
{
    SourceTouchMapState maps = {};
    maps.struct_size = sizeof( maps );maps.render_width = float( width );maps.render_height = float( height );
    panorama::IUIPanel *pane = ActiveMapContainer();
    if ( pane && width > 0 && height > 0 && pane->GetParentWindow() )
    {
        maps.viewport = MapPanelRect( pane, width, height );
        const float units = width / float( pane->GetParentWindow()->GetSurfaceWidth() );
        const float actualScroll = -pane->GetContentsXScrollOffset() * units;
        panorama::IUIScrollBar *scroll = pane->GetHorizontalScrollBar();
        // An immediate native drag invalidates the next Source layout. Keep
        // UIKit at that requested position when the finger is released,
        // while using the actual painted offset for tile content bounds.
        maps.scroll_offset = scroll && scroll->BLastMoveImmediate() ? scroll->GetScrollWindowPosition() * units : actualScroll;
        maps.content_width = scroll ? scroll->GetRangeMax() * units : maps.viewport.width;
        V_strncpy( maps.container_id, pane->GetID(), sizeof( maps.container_id ) );
        CUtlVector<panorama::IUIPanel *> tiles;
        pane->FindChildrenWithClassTraverse( "map-selection-btn", tiles );
        for ( int i = 0; i < tiles.Count() && maps.tile_count < SOURCE_MAP_TILE_LIMIT; ++i )
        {
            panorama::IUIPanel *p = tiles[i];
            SourceTouchRect r = MapPanelRect( p, width, height );
            if ( r.width <= 0 || r.height <= 0 || r.width >= FLT_MAX || r.height >= FLT_MAX ) continue;
            SourceTouchMapTile &tile = maps.tiles[maps.tile_count++];
            r.x += actualScroll - maps.viewport.x;r.y -= maps.viewport.y;tile.bounds = r;
            tile.enabled = p->IsEnabled();tile.selected = ( p->GetStyleFlags() & panorama::k_EStyleFlagSelected ) != 0;
            V_strncpy( tile.panel_id, p->GetID(), sizeof( tile.panel_id ) );
            V_strncpy( tile.map_name, p->GetAttribute( "mapname", "" ), sizeof( tile.map_name ) );
        }
        maps.visible = maps.tile_count > 0 && maps.viewport.width > 0 && maps.viewport.height > 0;
    }
    SourceTouchUpdateMaps( &maps );
}

static panorama::IUIPanel *ActiveSettingsViewport()
{
    CCSGO_MainMenu *menu = CCSGO_MainMenu::GetInstance();
    panorama::IUIPanel *root = menu ? menu->UIPanel() : NULL;
    if ( !NavigationAllowed( root ) || SourceTouchConsoleIsVisible() ) return NULL;
    panorama::IUIPanel *button = root->FindChildTraverse( "MainMenuNavBarSettings" );
    if ( !button || !( button->GetStyleFlags() & panorama::k_EStyleFlagSelected ) ) return NULL;
    panorama::IUIPanel *content = root->FindChildTraverse( "SettingsMenuContent" );
    const char *id = content ? content->GetAttribute( "icsm-active-settings-tab", "" ) : "";
    panorama::IUIPanel *tab = *id ? content->FindChildTraverse( id ) : NULL;
    if ( !tab || !tab->BIsVisible() || !tab->BHasClass( "Active" ) ) return NULL;
    // Floating Source menus must receive their original pointer events.
    CUtlVector<panorama::IUIPanel *> dropdowns;
    tab->FindChildrenWithClassTraverse( "DropDownMenuVisible", dropdowns );
    if ( dropdowns.Count() ) return NULL;
    return tab->FindChildTraverse( "ICSMSettingsScrollViewport" );
}

static void SettingsScrollCallback( const char *tabID, float pixels )
{
    panorama::IUIPanel *pane = s_initialized ? ActiveSettingsViewport() : NULL;
    if ( !pane || !pane->GetParent() || V_strcmp( pane->GetParent()->GetID(), tabID ) ||
         !IsFinite( pixels ) || !pane->GetParentWindow() ) return;
    int width = 0, height = 0;engine->GetScreenSize( width, height );
    panorama::IUIScrollBar *scroll = pane->GetVerticalScrollBar();
    if ( scroll && height > 0 )
        scroll->SetScrollWindowPosition( pixels * pane->GetParentWindow()->GetSurfaceHeight() / height, true );
}

static panorama::CSlider *SettingsSliderControl( panorama::IUIPanel *panel )
{
    panorama::IUIPanel *inner = panel ? panel->FindChildTraverse( "Slider" ) : NULL;
    return inner && !V_strcmp( inner->GetPanelType().String(), "Slider" ) ?
        static_cast<panorama::CSlider *>( inner->ClientPtr() ) : NULL;
}

static panorama::CSlider *FindSettingsSlider( panorama::IUIPanel *panel, const char *convar )
{
    if ( !panel || !panel->BIsVisible() || !panel->IsEnabled() ) return NULL;
    if ( !V_strcmp( panel->GetPanelType().String(), "CSGOSettingsSlider" ) )
    {
        CSGO_SettingsSlider *setting = static_cast<CSGO_SettingsSlider *>( panel->ClientPtr() );
        ConVarRef &var = setting->GetConVarRef();
        if ( var.IsValid() && !V_strcmp( var.GetName(), convar ) ) return SettingsSliderControl( panel );
    }
    for ( int i = 0; i < panel->GetChildCount(); ++i )
        if ( panorama::CSlider *slider = FindSettingsSlider( panel->GetChild( i ), convar ) ) return slider;
    return NULL;
}

static void SettingsSliderCallback( const char *tabID, const char *convar, float fraction, bool finished )
{
    panorama::IUIPanel *pane = s_initialized ? ActiveSettingsViewport() : NULL;
    if ( !pane || !pane->GetParent() || !tabID || !convar ||
         V_strcmp( pane->GetParent()->GetID(), tabID ) || !IsFinite( fraction ) ) return;
    panorama::CSlider *slider = FindSettingsSlider( pane, convar );
    if ( !slider ) return;
    // CSlider::SetValue emits the original SliderValueChanged event. Its
    // CSGOSettingsSlider parent owns ConVar range mapping, inversion and labels.
    slider->SetValue( Lerp( clamp( fraction, 0.0f, 1.0f ), slider->GetMin(), slider->GetMax() ) );
    if ( finished ) engine->ClientCmd_Unrestricted( "host_writeconfig\n", true );
}

static void PublishSettingsSliders( panorama::IUIPanel *panel, SourceTouchSettingsState &settings,
                                   SourceTouchSettingsSliderTargets &targets, int width, int height )
{
    if ( !panel || !panel->BIsVisible() || !panel->IsEnabled() ) return;
    if ( !V_strcmp( panel->GetPanelType().String(), "CSGOSettingsSlider" ) && settings.slider_count < SOURCE_SETTINGS_SLIDER_LIMIT )
    {
        SourceTouchRect rect = MapPanelRect( panel, width, height );
        if ( rect.width > 0 && rect.height > 0 ) settings.sliders[settings.slider_count++] = rect;
        CSGO_SettingsSlider *setting = static_cast<CSGO_SettingsSlider *>( panel->ClientPtr() );
        panorama::CSlider *slider = SettingsSliderControl( panel );
        panorama::IUIPanel *inner = panel->FindChildTraverse( "Slider" );
        panorama::IUIPanel *track = inner ? inner->FindChildTraverse( "SliderTrack" ) : NULL;
        panorama::IUIPanel *thumb = inner ? inner->FindChildTraverse( "SliderThumb" ) : NULL;
        ConVarRef &var = setting->GetConVarRef();
        if ( slider && track && thumb && var.IsValid() && targets.count < SOURCE_SETTINGS_SLIDER_LIMIT )
        {
            SourceTouchSettingsSlider target = {};
            target.hit_bounds = MapPanelRect( inner, width, height );
            target.track_bounds = MapPanelRect( track, width, height );
            target.thumb_width = thumb->GetActualRenderWidth() * width / float( panel->GetParentWindow()->GetSurfaceWidth() );
            float range = slider->GetMax() - slider->GetMin();
            target.fraction = range > 0 ? clamp( ( slider->GetValue() - slider->GetMin() ) / range, 0.0f, 1.0f ) : 0;
            V_strncpy( target.convar, var.GetName(), sizeof( target.convar ) );
            if ( target.hit_bounds.height > 0 && target.track_bounds.width > target.thumb_width && target.thumb_width > 0 )
                targets.sliders[targets.count++] = target;
        }
    }
    for ( int i = 0; i < panel->GetChildCount(); ++i ) PublishSettingsSliders( panel->GetChild( i ), settings, targets, width, height );
}

static void PublishSettings( int width, int height )
{
    SourceTouchSettingsState settings = {};
    SourceTouchSettingsSliderTargets targets = {};targets.struct_size = sizeof( targets );
    settings.struct_size = sizeof( settings );settings.render_width = float( width );settings.render_height = float( height );
    panorama::IUIPanel *pane = ActiveSettingsViewport();
    if ( pane && pane->BIsVisible() && pane->IsEnabled() && pane->GetParentWindow() && width > 0 && height > 0 )
    {
        settings.viewport = MapPanelRect( pane, width, height );
        const float units = height / float( pane->GetParentWindow()->GetSurfaceHeight() );
        panorama::IUIScrollBar *scroll = pane->GetVerticalScrollBar();
        settings.scroll_offset = scroll && scroll->BLastMoveImmediate() ? scroll->GetScrollWindowPosition() * units : -pane->GetContentsYScrollOffset() * units;
        settings.content_height = scroll ? scroll->GetRangeMax() * units : settings.viewport.height;
        V_strncpy( settings.tab_id, pane->GetParent()->GetID(), sizeof( settings.tab_id ) );
        PublishSettingsSliders( pane, settings, targets, width, height );
        settings.visible = settings.viewport.width > 0 && settings.viewport.height > 0;
    }
    SourceTouchUpdateSettingsSliderTargets( &targets );
    SourceTouchUpdateSettings( &settings );
}

static void PublishConsoleBlur( int width, int height )
{
    CCSGO_MainMenu *menu = CCSGO_MainMenu::GetInstance();
    panorama::IUIPanel *root = menu ? menu->UIPanel() : NULL;
    if ( !root ) return;
    SourceTouchConsoleState console = {};
    const bool show = SourceTouchGetConsoleState( &console );
    root->SetHasClass( "icsm-console-open", show );
    // UIKit owns the complete rounded frosted console. Keep the old marker
    // hidden so a rectangular Source fill cannot protrude from its corners.
    panorama::IUIPanel *panel = root->FindChildTraverse( "ICSMConsoleBlurTarget" );
    if ( panel ) panel->SetHasClass( "hidden", true );
}

static panorama::IUIPanel *InventoryPanel()
{
    CCSGO_MainMenu *menu = CCSGO_MainMenu::GetInstance();
    panorama::IUIPanel *root = menu ? menu->UIPanel() : NULL;
    if ( !NavigationAllowed( root ) ) return NULL;
    panorama::IUIPanel *button = root->FindChildTraverse( "MainMenuNavBarInventory" );
    panorama::IUIPanel *inventory = root->FindChildTraverse( "JsInventory" );
    return button && ( button->GetStyleFlags() & panorama::k_EStyleFlagSelected ) &&
        inventory && inventory->BIsVisible() && inventory->BReadyForDisplay() ? inventory : NULL;
}

static void InventoryCallback( const char *id )
{
    panorama::IUIPanel *inventory = s_initialized && !SourceTouchConsoleIsVisible() ? InventoryPanel() : NULL;
    if ( !inventory || !id || V_strncmp( id, "Offline", 7 ) ) return;
    if(!V_strncmp(id,"OfflineStickerPick",18) || !V_strncmp(id,"OfflineStickerSlot",18)) {
        // Only a bounded numeric identifier crosses the native/JS boundary.
        if(V_strlen(id)>28)return;
        for(const char *p=id+18;*p;++p)if(*p<'0' || *p>'9')return;
        char script[160];V_snprintf(script,sizeof(script),"OfflineInventory.ValidateUI('activate','%s');",id);
        panorama::UIEngine()->RunScript(inventory,script,"icsm_sticker_native",1,1,false,false);return;
    }
    panorama::IUIPanel *button = inventory->FindChildTraverse( id );
    if ( !button || !button->IsEnabled() ) return;
    panorama::DispatchEventAsync( 0.0f, panorama::Activated(), button->ClientPtr(), panorama::k_ePanelEventSourceMouse );
    Msg( "ICSM_GLASS_INVENTORY activate=%s\n", id );
}

static void PublishInventory()
{
    panorama::IUIPanel *inventory = InventoryPanel();
    const char *state = inventory ? inventory->GetAttribute( "icsm-inventory-state", "" ) : "";
    const bool mounted = SourceTouchUpdateInventory( state, inventory != NULL );
    // Keep Source's handlers and data alive. A mounted native page supplies
    // all drawing; the console can hide it without revealing the old grid.
    if ( inventory ) inventory->SetHasClass( "icsm-native-inventory", mounted );
}

static panorama::CPanelPtr<panorama::IUIPanel> s_navigationLayoutRoot;
static int s_navigationLayoutWidth = 0, s_navigationLayoutHeight = 0;
static float s_navigationLayoutScaleX = 0, s_navigationLayoutScaleY = 0;
static bool s_navigationWasVisible = false;
static void SynchronizeNavigationLayout( panorama::IUIPanel *root, bool visible )
{
    const bool returning = visible && !s_navigationWasVisible;
    s_navigationWasVisible = visible;
    if ( !visible || !root || !root->GetParentWindow() ) return;
    const int width = root->GetParentWindow()->GetSurfaceWidth(), height = root->GetParentWindow()->GetSurfaceHeight();
    const float scaleX = root->GetActualUIScaleX(), scaleY = root->GetActualUIScaleY();
    if ( !returning && s_navigationLayoutRoot.Get() == root && width == s_navigationLayoutWidth &&
        height == s_navigationLayoutHeight && scaleX == s_navigationLayoutScaleX && scaleY == s_navigationLayoutScaleY ) return;
    if ( width <= 0 || height <= 0 || scaleX <= 0 || scaleY <= 0 ) return;
    // CUIPanel::OnContentSizeTraverse skips opacity-zero panels. The native
    // toolbar keeps Source's original panels transparent, so their measured
    // bounds otherwise remain at the previous video mode after a resize.
    // Measure/layout these three explicit-size panels through Source's public
    // layout API only on geometry changes or when returning to the lobby.
    const char *ids[] = { "MainMenuNavBarLeft", "MainMenuNavBarRight", "ICSMProfile" };
    for ( unsigned i = 0; i < ARRAYSIZE(ids); ++i ) {
        panorama::IUIPanel *panel = root->FindChildTraverse( ids[i] );
        panorama::IUIPanel *parent = panel ? panel->GetParent() : NULL;
        if ( !parent || parent->GetActualLayoutWidth() <= 0 || parent->GetActualLayoutHeight() <= 0 ) continue;
        panel->InvalidateSizeAndPosition();
        panel->DesiredLayoutSizeTraverse( parent->GetActualLayoutWidth(), parent->GetActualLayoutHeight() );
        panel->LayoutTraverse( 0, 0, parent->GetActualLayoutWidth(), parent->GetActualLayoutHeight() );
    }
    s_navigationLayoutRoot = root;s_navigationLayoutWidth = width;s_navigationLayoutHeight = height;
    s_navigationLayoutScaleX = scaleX;s_navigationLayoutScaleY = scaleY;
    Msg( "ICSM_NAVIGATION_LAYOUT surface=%dx%d scale=%g,%g\n", width, height, scaleX, scaleY );
}

static void PublishNavigation( int width, int height )
{
    CCSGO_MainMenu *menu = CCSGO_MainMenu::GetInstance();
    panorama::IUIPanel *root = menu ? menu->UIPanel() : NULL;
    UpdateMenuBlurViewport( root );
    SourceTouchNavigationState navigation = {};
    navigation.struct_size = sizeof( navigation );
    navigation.render_width = float( width );
    navigation.render_height = float( height );
    navigation.visible = NavigationAllowed( root );
    SynchronizeNavigationLayout( root, navigation.visible );
    // The title strip is removed in Source's layout and is not mounted in
    // UIKit either. Retain the shared state structure for the existing ABI.
    navigation.title_visible = false;
    if ( navigation.visible && width > 0 && height > 0 )
    {
        navigation.bar = PanelRect( root->FindChildTraverse( "MainMenuNavBarLeft" ), width, height );
        navigation.bar_right = PanelRect( root->FindChildTraverse( "MainMenuNavBarRight" ), width, height );
        for ( int i = SourceNavigationPlay; i <= SourceNavigationFPS; ++i )
        {
            panorama::IUIPanel *button = root->FindChildTraverse( s_navigationIDs[i] );
            if ( !button || !button->BIsVisible() ) continue;
            navigation.buttons[i] = PanelRect( button, width, height );
            if ( button->IsEnabled() ) navigation.enabled_mask |= 1u << i;
            if ( button->GetStyleFlags() & panorama::k_EStyleFlagSelected ) navigation.selected_mask |= 1u << i;
        }
        ConVarRef fps( "net_graph", true );
        navigation.fps_enabled = fps.IsValid() && fps.GetInt() > 0;
        panorama::IUIPanel *profile = root->FindChildTraverse( "ICSMProfile" );
        if ( profile && profile->BIsVisible() )
        {
            navigation.profile = PanelRect( profile, width, height );
            navigation.profile_visible = navigation.profile.width > 0 && navigation.profile.height > 0;
            navigation.profile_enabled = profile->IsEnabled();
            ConVarRef name( "name", true );
            V_strncpy( navigation.profile_name_utf8, name.IsValid() ? name.GetString() : "", sizeof( navigation.profile_name_utf8 ) );
        }
    }
    const uint32_t mounted = SourceTouchUpdateNavigation( &navigation );
    // Keep the original buttons laid out, so every resolution uses the same
    // authoritative rectangles. The pause bar and unsupported hosts retain
    // their original drawing; only a mounted native lobby bar hides it.
    if ( root )
    {
        const uint32_t frostedPages = ( 1u << SourceNavigationPlay ) | ( 1u << SourceNavigationSettings ) |
            ( 1u << SourceNavigationInventory );
        root->SetHasClass( "icsm-neutral-frost", navigation.visible &&
            ( navigation.selected_mask & frostedPages ) != 0 && !SourceTouchConsoleIsVisible() );
        root->SetHasClass( "icsm-native-glass-nav", ( mounted & SourceGlassNavigation ) != 0 );
        root->SetHasClass( "icsm-native-glass-title", ( mounted & SourceGlassTitle ) != 0 );
        root->SetHasClass( "icsm-native-glass-profile", ( mounted & SourceGlassProfile ) != 0 );
    }
}

static void PublishState()
{
    if ( !s_initialized || !engine ) return;
    SourceTouchState state = {};
    state.struct_size = sizeof( state );
    const bool inGame = engine->IsInGame();
    state.in_game = inGame;
    C_CSPlayer *player = inGame ? C_CSPlayer::GetLocalCSPlayer() : NULL;
    const bool gameUI = enginevgui && enginevgui->IsGameUIVisible();
    const bool buy = player && player->IsBuyMenuOpen();
    const bool score = inGame && ScoreboardOpen();
    s_pointerUI = inGame && PanoramaPointerUI();
    s_teamMenu = inGame && TeamSelectionOpen();
    s_endOfMatch = inGame && EndOfMatchOpen();
    if ( s_scoreHeld && !score )
    {
        KeyUp( &in_score, "-55100" );
        s_scoreHeld = false;
    }
    s_gameInput = GameplayAllowed( player );
    // Preserve the native scoreboard close button, but let the original SDL
    // absolute-pointer path handle team selection and other Panorama menus.
    // EndOfMatch embeds the scoreboard, but its voting buttons need pointer
    // events rather than the native gameplay/scoreboard-close touch route.
    // Read the panel as well as capture flags to cover the Show() frame before
    // the engine recalculates its aggregated Panorama input flags.
    state.mode = inGame && player && !gameUI && !buy && !s_teamMenu && !s_endOfMatch && ( !s_pointerUI || score )
        ? SourceTouchModeGameplay : SourceTouchModeMenu;
    state.active = state.mode == SourceTouchModeGameplay;
    state.game_input = s_gameInput;
    state.scoreboard_open = score;
    int width = 0, height = 0;
    engine->GetScreenSize( width, height );
    state.render_width = float( width );
    state.render_height = float( height );
    PublishNavigation( width, height );
    PublishMaps( width, height );
    PublishSettings( width, height );
    PublishConsoleBlur( width, height );
    PublishInventory();
    // These physical values are the verified target M5 iPad panel.  The native
    // module converts physical layout through current UIKit view dimensions.
    state.native_landscape_width_pixels = 2420.0f;
    state.native_density_ppi = 264.0f;

    CCSGO_Hud *hud = inGame ? CCSGO_Hud::GetInstance() : NULL;
    if ( hud && width > 0 && height > 0 )
    {
        panorama::IUIPanel *radar = hud->UIPanel()->FindChildTraverse( "HudRadar" );
        if ( radar ) radar = radar->FindChildTraverse( "Radar" );
        state.radar = PanelRect( radar, width, height );
        state.radar_valid = state.radar.width > 0 && state.radar.height > 0;
        state.score = PanelRect( hud->UIPanel()->FindChildTraverse( "HudTeamCounter" ), width, height );
        state.score_valid = state.score.width > 0 && state.score.height > 0;
    }

    SourceTouchWeapon weapons[64] = {};
    char names[64][256] = {};
    unsigned count = 0;
    if ( player )
    {
        C_BaseCombatWeapon *active = player->GetActiveWeapon();
        for ( int i = 0; i < player->WeaponCount() && count < 64; ++i )
        {
            C_BaseCombatWeapon *weapon = player->GetWeapon( i );
            if ( !weapon || !weapon->CanBeSelected() ) continue;
            const int slot = int( weapon->GetGearSlot() ) + 1;
            if ( slot < 1 || slot > 5 ) continue;
            SourceTouchWeapon &entry = weapons[count];
            entry.slot = slot;
            entry.entity_index = weapon->entindex();
            entry.subtype = weapon->GetSubType();
            entry.selected = weapon == active;
            const char *name = weapon->GetPrintName();
            const wchar_t *localized = name && g_pVGuiLocalize ? g_pVGuiLocalize->Find( name ) : NULL;
            if ( localized ) Q_UnicodeToUTF8( localized, names[count], sizeof( names[count] ) );
            else V_strncpy( names[count], name ? name : weapon->GetClassname(), sizeof( names[count] ) );
            entry.name_utf8 = names[count];
            ++count;
        }
    }
    state.weapons = weapons;
    state.weapon_count = count;
    const bool becameBlocked = s_state.game_input && !state.game_input;
    const bool leftGame = s_state.mode == SourceTouchModeGameplay && state.mode == SourceTouchModeMenu;
    if ( becameBlocked || leftGame ) ReleaseGameActions();
    s_state = state;
    // SourceTouchUpdateState copies inventory names during this call; its
    // native implementation must never retain the stack arrays above.
    SourceTouchSetButtonsHidden( icsm_touch_hide_buttons.GetBool() );
    SourceTouchUpdateState( &state );
    s_state.weapons = NULL;
    s_state.weapon_count = 0;
}

static void SetAction( int action, bool pressed )
{
    if ( action < 0 || action >= SourceTouchActionCount || !Button( action ) ) return;
    if ( pressed && !s_gameInput ) return;
    if ( pressed == s_held[action] ) return;
    if ( pressed ) KeyDown( Button( action ), KeyId( action ) );
    else
    {
        KeyUp( Button( action ), KeyId( action ) );
        if ( action == SourceTouchAttack ) in_cancel[GET_ACTIVE_SPLITSCREEN_SLOT()] = 0;
    }
    s_held[action] = pressed;
    s_touched[action] = true;
    ++s_actionEdges;
    if ( touch_input_debug.GetBool() )
        Msg( "I5_TOUCH action=%d pressed=%d\n", action, pressed ? 1 : 0 );
}

static void ToggleScoreboard()
{
    if ( !engine->IsInGame() || !CCSGO_Scoreboard::GetInstance() || !CCSGO_EndOfMatch::GetInstance() ) return;
    CCommand args;
    if ( ScoreboardOpen() )
    {
        args.Tokenize( "-showscores -55100" );
        IN_ScoreUp( args );
        s_scoreHeld = false;
    }
    else
    {
        args.Tokenize( "+showscores -55100" );
        IN_ScoreDown( args );
        s_scoreHeld = ScoreboardOpen();
    }
    PublishState();
}

static void SelectInventory( int entity, int subtype )
{
    if ( !s_gameInput ) return;
    C_CSPlayer *player = C_CSPlayer::GetLocalCSPlayer();
    if ( !player || !player->IsAllowedToSwitchWeapons() ) return;
    C_BaseCombatWeapon *owned = NULL;
    for ( int i = 0; i < player->WeaponCount(); ++i )
    {
        C_BaseCombatWeapon *weapon = player->GetWeapon( i );
        if ( weapon && weapon->entindex() == entity && weapon->GetSubType() == subtype )
        { owned = weapon; break; }
    }
    if ( !owned || !owned->CanBeSelected() ) return;
    // The overlay supplied an actual inventory entity, including whichever
    // grenade its slot cycle selected.  Use Source's public selection latch,
    // rather than inventing an entindex or selecting another slot member.
    ::input->MakeWeaponSelection( owned );
    if ( touch_input_debug.GetBool() )
        Msg( "I5_TOUCH select slot=%d entity=%d subtype=%d\n", int( owned->GetGearSlot() ) + 1, entity, subtype );
}

static void PumpEvents()
{
    if ( !s_initialized || s_pumping ) return;
    s_pumping = true;
    while ( s_read != s_write )
    {
        const Event event = s_events[s_read++ % kCapacity];
        if ( event.type != CommandEvent && event.type != WeaponEvent && event.type != ResetEvent && event.epoch != s_epoch ) continue;
        switch ( event.type )
        {
        case ResetEvent: ReleaseGameActions(); break;
        case ActionEvent: SetAction( event.code, event.value != 0 ); break;
        case MoveEvent:
            if ( s_gameInput )
            {
                s_moveX = clamp( event.x, -1.0f, 1.0f );
                s_moveY = clamp( event.y, -1.0f, 1.0f );
                const float square = s_moveX*s_moveX + s_moveY*s_moveY;
                if ( square > 1.0f ) { const float scale = 1.0f / sqrtf( square ); s_moveX *= scale; s_moveY *= scale; }
            }
            break;
        case LookEvent:
            if ( s_gameInput && s_lookCount < kCapacity )
            { LookSample sample = { event.x, event.y, event.timestamp, event.sampleInterval }; s_look[s_lookCount++] = sample; }
            break;
        case WeaponEvent: SelectInventory( event.code, event.value ); break;
        case CommandEvent:
            ++s_commands;
            if ( event.code == SourceTouchScoreboard ) ToggleScoreboard();
            else if ( event.code == SourceTouchPause && enginevgui && engine->IsInGame() )
            {
                ReleaseGameActions();
                enginevgui->ActivateGameUI();
                PublishState();
            }
            else if ( event.code == SourceTouchBuy && s_gameInput )
            {
                // Dispatch the original registered buymenu command from
                // counterstrikeviewport.cpp.  It retains alive/demo, buy
                // period and buy zone checks; Panorama also calls CanPlayerBuy.
                ConCommand *buy = cvar ? cvar->FindCommand( "buymenu" ) : NULL;
                if ( buy )
                {
                    ReleaseGameActions();
                    CCommand args;
                    args.Tokenize( "buymenu" );
                    buy->Dispatch( args );
                    // The original panel maintains IsBuyMenuOpen.  Once open,
                    // PublishState selects Menu and SDL's original absolute
                    // pointer receives the following touches.
                    PublishState();
                }
                else Warning( "I5_TOUCH original buymenu command unavailable\n" );
            }
            else if ( s_gameInput && event.epoch == s_epoch && event.code == SourceTouchInspect )
            {
                // CCSPlayer::ClientCommand handles both edges. The original
                // FlashlightTurnOn fallback uses this same press/release pair:
                // release clears the hold without cancelling the animation.
                engine->ClientCmd_Unrestricted( "+lookatweapon\n-lookatweapon\n", true );
                ++s_inspectCommands;
            }
            else if ( s_gameInput && event.epoch == s_epoch && event.code == SourceTouchDrop )
            {
                // Keep the original server-side HandleDropWeapon restrictions
                // and deathmatch bonus-weapon behavior.
                engine->ClientCmd_Unrestricted( "drop\n", true );
                ++s_dropCommands;
            }
            if ( touch_input_debug.GetBool() ) Msg( "I5_TOUCH command=%d count=%u\n", event.code, s_commands );
            break;
        }
    }
    s_pumping = false;
}

static void ControllerMove( int slot, CUserCmd *cmd )
{
    if ( !s_initialized || slot != 0 ) return;
    PublishState();
    PumpEvents();
    if ( !s_gameInput ) { s_lookCount = 0; return; }
    cmd->forwardmove += s_moveY * ( s_moveY >= 0 ? cl_forwardspeed.GetFloat() : cl_backspeed.GetFloat() );
    cmd->sidemove += s_moveX * cl_sidespeed.GetFloat();
    if ( s_lookCount == 0 ) return;
    QAngle angles;
    engine->GetViewAngles( angles );
    view->StopPitchDrift();
    const float yawGain = touch_yaw_degrees_per_point.GetFloat();
    const float pitchGain = touch_pitch_degrees_per_point.GetFloat();
    const ICSMTouchAim::Settings settings = AimSettings();
    const ICSMTouchAim::Scope scope = AimScope();
    for ( unsigned i = 0; i < s_lookCount; ++i )
    {
        s_lastAim = ICSMTouchAim::Apply(s_look[i].x, s_look[i].y, s_look[i].interval,
            yawGain, pitchGain, settings, scope);
        const float yaw = s_lastAim.yaw;
        const float pitch = s_lastAim.pitch;
        angles[YAW] += yaw;
        angles[PITCH] += pitch;
        angles[PITCH] = clamp( angles[PITCH], -cl_pitchup.GetFloat(), cl_pitchdown.GetFloat() );
        s_yawTotal += yaw;
        s_pitchTotal += pitch;
        ++s_lookSamples;
    }
    s_lookCount = 0;
    engine->SetViewAngles( angles );
    if ( touch_input_debug.GetBool() )
        Msg( "I5_TOUCH look samples=%u yaw_delta_total=%.6f pitch_delta_total=%.6f view=%.6f %.6f\n",
            s_lookSamples, s_yawTotal, s_pitchTotal, angles[YAW], angles[PITCH] );
}

static void MergeDirectionBits( CUserCmd *cmd )
{
    if ( !s_gameInput ) return;
    if ( s_moveY > 0 ) cmd->buttons |= IN_FORWARD;
    else if ( s_moveY < 0 ) cmd->buttons |= IN_BACK;
    if ( s_moveX > 0 ) cmd->buttons |= IN_MOVERIGHT;
    else if ( s_moveX < 0 ) cmd->buttons |= IN_MOVELEFT;
}

static void RecordCommand( const CUserCmd *cmd, bool real )
{
    if ( !s_initialized ) return;
    if ( real )
    {
        s_lastCommand = *cmd;
        ++s_realCommands;
        // GetButtonBits(true) has consumed impulse-down for the real usercmd.
        for ( int a = 0; a < SourceTouchActionCount; ++a ) s_touched[a] = false;
    }
    else
    {
        s_lastExtraCommand = *cmd;
        ++s_extraCommands;
    }
}

#include "frame_calibration_source.inc"

static void FrameCallback( void * )
{
    if ( !s_initialized || !engine ) return;
    // This callback is registered with the main-thread SDL touch module.  It
    // may run in the menu, where no active CInput movement callback is issued.
    ACTIVE_SPLITSCREEN_PLAYER_GUARD( 0 );
    CalibrationFrame();
    CheckQualityDisplaySize();
    SynchronizeControllerSettings();
    PublishState();
}

static void DiagnosticCommand( const CCommand & )
{
    if ( s_initialized && engine )
    {
        ACTIVE_SPLITSCREEN_PLAYER_GUARD( 0 );
        PublishState();
    }
    Msg( "I5_TOUCH_STATE ready=%d mode=%d active=%d game_input=%d score_open=%d queue=%u pending_look=%u edges=%u samples=%u resets=%u real_commands=%u extra_samples=%u gain=%.6f %.6f\n",
        s_initialized, int( s_state.mode ), s_state.active, s_gameInput, s_state.scoreboard_open,
        s_write-s_read, s_lookCount, s_actionEdges, s_lookSamples, s_resets,
        s_realCommands, s_extraCommands, touch_yaw_degrees_per_point.GetFloat(), touch_pitch_degrees_per_point.GetFloat() );
    Msg( "I5_TOUCH_USERCMD number=%d buttons=0x%x forward=%.6f side=%.6f up=%.6f view=%.6f %.6f %.6f weaponselect=%d subtype=%d yaw_delta_total=%.6f pitch_delta_total=%.6f\n",
        s_lastCommand.command_number, s_lastCommand.buttons, s_lastCommand.forwardmove,
        s_lastCommand.sidemove, s_lastCommand.upmove, s_lastCommand.viewangles[YAW],
        s_lastCommand.viewangles[PITCH], s_lastCommand.viewangles[ROLL],
        s_lastCommand.weaponselect, s_lastCommand.weaponsubtype, s_yawTotal, s_pitchTotal );
    Msg( "I5_TOUCH_EXTRA buttons=0x%x forward=%.6f side=%.6f view=%.6f %.6f\n",
        s_lastExtraCommand.buttons, s_lastExtraCommand.forwardmove, s_lastExtraCommand.sidemove,
        s_lastExtraCommand.viewangles[YAW], s_lastExtraCommand.viewangles[PITCH] );
    C_CSPlayer *player = engine && engine->IsInGame() ? C_CSPlayer::GetLocalCSPlayer() : NULL;
    C_BaseCombatWeapon *weapon = player ? player->GetActiveWeapon() : NULL;
    Msg( "I5_TOUCH_WEAPON inspect_commands=%u drop_commands=%u inspecting=%d inspect_held=%d active_entity=%d active_class=%s\n",
        s_inspectCommands, s_dropCommands, player && player->IsLookingAtWeapon(), player && player->IsHoldingLookAtWeapon(),
        weapon ? weapon->entindex() : 0, weapon ? weapon->GetClassname() : "none" );
    Msg( "I5_TOUCH_UI panorama_pointer=%d team_select=%d end_of_match=%d team=%d\n",
        s_pointerUI, s_teamMenu, s_endOfMatch, player ? player->GetTeamNumber() : -1 );
    Msg( "I5_TOUCH_HUD render=%.3f %.3f radar_valid=%d radar=%.3f %.3f %.3f %.3f score_valid=%d score=%.3f %.3f %.3f %.3f\n",
        s_state.render_width, s_state.render_height, s_state.radar_valid,
        s_state.radar.x, s_state.radar.y, s_state.radar.width, s_state.radar.height,
        s_state.score_valid, s_state.score.x, s_state.score.y, s_state.score.width, s_state.score.height );
    char native[16384];
    if ( SourceTouchCopyDiagnosticsJSON( native, sizeof( native ) ) )
        Msg( "I5_TOUCH_NATIVE %s\n", native );
}
static ConCommand i5_touch_state( "i5_touch_state", DiagnosticCommand,
    "Report actual native touch input, original CUserCmd and HUD anchors.", FCVAR_CLIENTCMD_CAN_EXECUTE );

// Local UI validation goes through the actual Panorama handlers, without
// synthesising UIKit touches or accepting arbitrary JavaScript from a server.
static bool SettingsDiagnosticToken( const char *text )
{
    if ( !text || !*text || V_strlen( text ) > 64 ) return false;
    for ( const char *p = text; *p; ++p )
        if ( !( ( *p >= 'a' && *p <= 'z' ) || ( *p >= 'A' && *p <= 'Z' ) ||
                ( *p >= '0' && *p <= '9' ) || *p == '_' || *p == '-' || *p == '.' ) ) return false;
    return true;
}

static void SettingsDiagnosticCommand( const CCommand &args )
{
    CCSGO_MainMenu *menu = CCSGO_MainMenu::GetInstance();
    panorama::IUIPanel *root = menu ? menu->UIPanel() : NULL;
    if ( !root || !s_initialized || !NavigationAllowed( root ) ) return;
    if ( args.ArgC() == 2 && !V_strcmp( args[1], "open" ) )
    {
        panorama::IUIPanel *button = root->FindChildTraverse( "MainMenuNavBarSettings" );
        if ( button ) panorama::DispatchEventAsync( 0, panorama::Activated(), button->ClientPtr(), panorama::k_ePanelEventSourceMouse );
        return;
    }
    panorama::IUIPanel *settings = root->FindChildTraverse( "JsSettings" );
    if ( !settings || args.ArgC() < 2 || args.ArgC() > 4 ) return;
    for ( int i = 1; i < args.ArgC(); ++i ) if ( !SettingsDiagnosticToken( args[i] ) ) return;
    char script[512];
    V_snprintf( script, sizeof( script ), "SettingsMenu.ValidateUI('%s','%s','%s');", args[1],
        args.ArgC() > 2 ? args[2] : "", args.ArgC() > 3 ? args[3] : "" );
    panorama::UIEngine()->RunScript( settings, script, "icsm_settings_validation", 1, 1, false, false );
}
static ConCommand i5_settings_ui( "i5_settings_ui", SettingsDiagnosticCommand,
    "Local settings validation: open, tab <id>, select <id> <option>, slider <id> <value>, activate <id>, state.", FCVAR_DONTRECORD );

static void InventoryDiagnosticCommand( const CCommand &args )
{
    CCSGO_MainMenu *menu = CCSGO_MainMenu::GetInstance();
    panorama::IUIPanel *root = menu ? menu->UIPanel() : NULL;
    if ( !root || !s_initialized || !NavigationAllowed( root ) || args.ArgC() < 2 || args.ArgC() > 3 ) return;
    if ( args.ArgC() == 2 && !V_strcmp( args[1], "open" ) )
    {
        panorama::IUIPanel *button = root->FindChildTraverse( "MainMenuNavBarInventory" );
        if ( button ) panorama::DispatchEventAsync( 0, panorama::Activated(), button->ClientPtr(), panorama::k_ePanelEventSourceMouse );
        return;
    }
    panorama::IUIPanel *inventory = root->FindChildTraverse( "JsInventory" );
    if ( !inventory ) return;
    for ( int i = 1; i < args.ArgC(); ++i ) if ( !SettingsDiagnosticToken( args[i] ) ) return;
    char script[256];
    V_snprintf( script, sizeof( script ), "OfflineInventory.ValidateUI('%s','%s');", args[1], args.ArgC() > 2 ? args[2] : "" );
    panorama::UIEngine()->RunScript( inventory, script, "icsm_inventory_validation", 1, 1, false, false );
}
static ConCommand i5_inventory_ui( "i5_inventory_ui", InventoryDiagnosticCommand,
    "Local inventory validation: open, activate <panel id>, state.", FCVAR_DONTRECORD );

static void SettingsScrollDiagnosticCommand( const CCommand &args )
{
    if ( !s_initialized ) return;
    if ( args.ArgC() == 2 && !V_strcmp( args[1], "test" ) )
    {
        char json[4096];
        if ( SourceTouchSettingsScrollSelfTestJSON( json, sizeof( json ) ) ) Msg( "ICSM_SETTINGS_SCROLL_TEST %s\n", json );
    }
    else if ( args.ArgC() == 4 && !V_strcmp( args[1], "slider" ) && SettingsDiagnosticToken( args[2] ) && SettingsDiagnosticToken( args[3] ) )
    {
        if ( SourceTouchSettingsSliderSet( args[2], V_atof( args[3] ), true ) )
            Msg( "ICSM_SETTINGS_SLIDER convar=%s fraction=%g\n", args[2], V_atof( args[3] ) );
    }
    else if ( args.ArgC() >= 2 && args.ArgC() <= 3 && SettingsDiagnosticToken( args[1] ) )
    {
        float offset = V_atof( args[1] );
        if ( IsFinite( offset ) && SourceTouchSettingsScrollTo( offset, args.ArgC() == 3 && !V_strcmp( args[2], "1" ) ) )
            Msg( "ICSM_SETTINGS_SCROLL requested_pixels=%g\n", offset );
    }
}
static ConCommand i5_settings_scroll( "i5_settings_scroll", SettingsScrollDiagnosticCommand,
    "Local settings validation: <pixels> [1=animated], test, or slider <visible convar> <0..1>. Never synthesises UITouch.", FCVAR_DONTRECORD );

static void LayoutEditorCommand( const CCommand &args )
{
    if(args.ArgC()>1) {
        if(SourceTouchLayoutEditorAction(args[1],args.ArgC()>2?V_atoi(args[2]):0))Msg("ICSM_TOUCH_LAYOUT action=%s ok=1\n",args[1]);
        else Warning("ICSM_TOUCH_LAYOUT action=%s ok=0\n",args[1]);
        return;
    }
    ReleaseGameActions();
    SourceTouchOpenLayoutEditor();
}
static ConCommand i5_touch_layout( "i5_touch_layout", LayoutEditorCommand,
    "Open the native touch layout editor.", FCVAR_CLIENTCMD_CAN_EXECUTE );
static void KeyboardEdgeDiagnosticCommand(const CCommand &args) {
    if(args.ArgC()!=3 || (V_strcmp(args[2],"down") && V_strcmp(args[2],"up")))return;
    Msg("ICSM_TOUCH_KEY_EDGE scancode=%d pressed=%d ok=%d\n",V_atoi(args[1]),!V_strcmp(args[2],"down"),
        SourceTouchKeyboardTestEdge(V_atoi(args[1]),!V_strcmp(args[2],"down")));
}
static ConCommand i5_touch_key_edge("i5_touch_key_edge",KeyboardEdgeDiagnosticCommand,
    "Local keyboard-path validation: <SDL scancode> <down|up>; gameplay only.",FCVAR_DONTRECORD);

static void SavePlayerName(const char *value)
{
    ConVarRef name("name");
    if (!name.IsValid()) return;
    name.SetValue(value);
    // Pass the nickname as data to the original USERINFO convar, never as
    // console command text. Its archive is written through the normal engine.
    if (engine) engine->ClientCmd_Unrestricted("host_writeconfig\n");
}
static void PlayerNameCommand(const CCommand &)
{
    ConVarRef name("name");
    ReleaseGameActions();
    SourceTouchEditPlayerName(name.IsValid() ? name.GetString() : "", SavePlayerName);
}
static ConCommand i5_player_name("i5_player_name", PlayerNameCommand,
    "Open the native player name editor.", FCVAR_CLIENTCMD_CAN_EXECUTE);

static void ExportErrorLogsCommand(const CCommand &) {
    ReleaseGameActions();
    SourceTouchExportErrorLogs();
}
static ConCommand icsm_export_logs("icsm_export_logs", ExportErrorLogsCommand,
    "Export recent diagnostic logs through the iOS share sheet.", FCVAR_CLIENTCMD_CAN_EXECUTE);

static void JoinRoom(const char *endpoint,const char *roomPassword)
{
    // Password is convar data, never console text. The browser validates the
    // address and resolves it to IPv4 before offering a join action.
    if(!endpoint || !*endpoint)return;
    ConVarRef password("password");
    if(password.IsValid())password.SetValue(roomPassword?roomPassword:"");
    if(engine)engine->ClientCmd_Unrestricted(CFmtStr("connect %s\n",endpoint));
}
static void RoomBrowserCommand(const CCommand &)
{
    ReleaseGameActions();SourceTouchOpenRoomBrowser(JoinRoom);
}
static ConCommand icsm_room_browser("icsm_room_browser",RoomBrowserCommand,
    "Search private rooms on LAN or saved Internet addresses.",FCVAR_CLIENTCMD_CAN_EXECUTE);

static void Initialize()
{
    SourceTouchCallbacks callbacks = {};
    callbacks.struct_size = sizeof( callbacks );
    callbacks.action = ActionCallback;
    callbacks.move = MoveCallback;
    callbacks.look = LookCallback;
    callbacks.command = CommandCallback;
    callbacks.weapon_select = WeaponCallback;
    callbacks.reset = ResetCallback;
    callbacks.frame = FrameCallback;
    s_initialized = true;
    SourceTouchSetCallbacks( &callbacks, NULL );
    SourceTouchSetLookSampleCallback( LookSampleCallback );
    SourceTouchSetNavigationCallback( NavigationCallback );
    SourceTouchSetProfileCallback( ProfileCallback );
    SourceTouchSetMapCallbacks( MapScrollCallback, MapActivateCallback );
    SourceTouchSetSettingsScrollCallback( SettingsScrollCallback );
    SourceTouchSetSettingsSliderCallback( SettingsSliderCallback );
    SourceTouchSetInventoryCallback( InventoryCallback );
    SourceTouchFrameCalibrationRecover(CalibrationQueue,CalibrationScene);
    Msg( "I5_TOUCH client ready: timed actual samples, saved aim mode and scope sensitivity\n" );
}

static void Shutdown()
{
    if ( !s_initialized ) return;
    SourceTouchFrameCalibrationShutdown();
    ReleaseGameActions();
    if ( s_scoreHeld )
    {
        KeyUp( &in_score, "-55100" );
        s_scoreHeld = false;
    }
    SourceTouchState state = {};
    state.struct_size = sizeof( state );
    SourceTouchUpdateState( &state );
    SourceTouchSetButtonsHidden( false );
    SourceTouchSetCallbacks( NULL, NULL );
    SourceTouchSetNavigationCallback( NULL );
    SourceTouchSetProfileCallback( NULL );
    SourceTouchSetMapCallbacks( NULL, NULL );
    SourceTouchSetSettingsScrollCallback( NULL );
    SourceTouchSetSettingsSliderCallback( NULL );
    SourceTouchSetInventoryCallback( NULL );
    SourceTouchUpdateInventory( "", false );
    SourceTouchMapState maps = {};maps.struct_size = sizeof( maps );SourceTouchUpdateMaps( &maps );
    SourceTouchSettingsState settings = {};settings.struct_size = sizeof( settings );SourceTouchUpdateSettings( &settings );
    SourceTouchNavigationState navigation = {};
    navigation.struct_size = sizeof( navigation );
    SourceTouchUpdateNavigation( &navigation );
    s_initialized = false;
    s_read = s_write = s_lookCount = 0;
}
} // namespace I5Touch

// The host invokes this on the main Source thread before each engine frame.
// ControllerMove alone would never clear the overlay following disconnect.
extern "C" SOURCE_TOUCH_API void SourceTouchClientFrame()
{
    I5Touch::FrameCallback( NULL );
}

// Called by the UIKit startup host on the same main thread as Source.
// Read original panel and movie state; a host/engine-ready flag alone becomes
// true before the lobby finishes creating and laying out its Panorama UI.
extern "C" SOURCE_TOUCH_API bool SourceTouchSynchronizeVideoSettings()
{
    // The client's restart/optionsui ConVars are registered after the port's
    // early engine video-config read. Reuse the original reader now that all
    // those ConVars exist, keeping explicit values and deliberate AUTO keys.
    const bool loaded = UpdateVideoConfigConVars();
    if ( loaded ) Msg( "ICSM_VIDEO_SETTINGS_SYNC client restart/optionsui values loaded from saved video config\n" );
    I5Touch::ApplyFrameRateLimit( I5Touch::icsm_fps_limit.GetInt() );
    if ( loaded ) { I5Touch::s_qualityReady = true; I5Touch::ApplyQualityPreset(); }
    return loaded;
}

extern "C" SOURCE_TOUCH_API bool SourceTouchQualityManagesResolution()
{
    return I5Touch::s_qualityReady;
}

extern "C" SOURCE_TOUCH_API size_t SourceTouchVideoStateJSON(char *buffer,size_t capacity)
{
    const char *names[]={"fps_max","fps_max_menu","mat_powersavingsmode","cpu_level","gpu_level","gpu_mem_level","csm_quality_level","mat_vsync","icsm_scene_render_percent","icsm_fps_limit","icsm_metal_spatial_upscale"};
    float values[ARRAYSIZE(names)]={};
    for(size_t i=0;i<ARRAYSIZE(names);++i) {
        ConVarRef ref(names[i]);if(!ref.IsValid())return 0;values[i]=ref.GetFloat();
    }
    const MaterialSystem_Config_t &config=materials->GetCurrentConfigForVideoCard();
    CCSGO_MainMenu *menu=CCSGO_MainMenu::GetInstance();
    panorama::IUIPanel *root=menu?menu->UIPanel():NULL;
    int width=0,height=0;if(engine)engine->GetScreenSize(width,height);
    panorama::IUIPanel *blur=root?root->FindChildTraverse("ICSMMenuBlur"):NULL;
    bool open=blur && blur->BIsVisible() && !blur->BHasClass("hidden");
    // Collapsed panels keep uninitialized offsets until their first layout.
    // Do not serialize those as non-finite JSON numbers while the menu is shut.
    auto laidOutRect=[width,height](panorama::IUIPanel *panel) -> SourceTouchRect {
        SourceTouchRect empty={};
        if(!panel || panel->GetActualLayoutWidth()<=0 || panel->GetActualLayoutHeight()<=0 ||
            panel->GetActualUIScaleX()<=0 || panel->GetActualUIScaleY()<=0)return empty;
        for(panorama::IUIPanel *p=panel;p;p=p->GetParent())
            if(p->GetActualXOffset()==FLT_MAX || p->GetActualYOffset()==FLT_MAX)return empty;
        SourceTouchRect rect=I5Touch::PanelRect(panel,width,height);
        return IsFinite(rect.x) && IsFinite(rect.y) && IsFinite(rect.width) && IsFinite(rect.height)?rect:empty;
    };
    SourceTouchRect mask=open?laidOutRect(blur):SourceTouchRect{};
    SourceTouchRect target=open?laidOutRect(root->FindChildTraverse("mainmenu-content__blur-target")):SourceTouchRect{};
    SourceTouchRect background=laidOutRect(root?root->FindChildTraverse("MainMenuBackground"):NULL);
    SourceTouchRect content=open?laidOutRect(root->FindChildTraverse("JsMainMenuContent")):SourceTouchRect{};
    SourceTouchRect playStart=open?laidOutRect(root->FindChildTraverse("StartMatchBtn")):SourceTouchRect{};
    bool covers=open && target.x<=0 && target.y<=0 && target.x+target.width>=width && target.y+target.height>=height;
    const ICSMDisplay::SceneResolution plan=I5Touch::QualityScenePlan({unsigned(width),unsigned(height)});
    const char *blurEffect = root && root->BHasClass( "icsm-neutral-frost" ) ?
        "Source fastgaussian(10,10,3) / desaturated lobby" : "Source fastgaussian(8,8,3)";
    int length=V_snprintf(buffer,capacity,"{\"fps_max\":%g,\"fps_max_menu\":%g,\"mat_powersavingsmode\":%g,\"cpu_level\":%g,\"gpu_level\":%g,\"gpu_mem_level\":%g,\"csm_quality_level\":%g,\"mat_vsync\":%g,\"scene_render_percent\":%g,\"fps_limit\":%g,\"metal_spatial_upscale\":%g,\"fullscreen\":%s,\"borderless\":%s,\"fullscreen_locked\":true,\"quality_preset\":%d,\"base_fps_locked\":%s,\"high_resolution\":[%d,%d],\"ui_resolution\":[%d,%d],\"scene_resolution\":[%u,%u],\"scene_output_resolution\":[%u,%u],\"ui_independent_resolution\":true,\"menu_blur\":{\"effect\":\"%s\",\"surface_pixels\":[%d,%d],\"render_pixels\":[%d,%d],\"ui_scale\":[%g,%g],\"mask_rect\":[%g,%g,%g,%g],\"target_rect\":[%g,%g,%g,%g],\"background_rect\":[%g,%g,%g,%g],\"content_rect\":[%g,%g,%g,%g],\"play_start_rect\":[%g,%g,%g,%g],\"open\":%s,\"covers_viewport\":%s,\"layout_updates\":%u}}",
        values[0],values[1],values[2],values[3],values[4],values[5],values[6],values[7],values[8],values[9],values[10],
        config.Windowed()?"false":"true",config.NoWindowBorder()?"true":"false",
        I5Touch::icsm_quality_preset.GetInt(),I5Touch::s_qualityFrameLocked?"true":"false",
        I5Touch::icsm_quality_high_width.GetInt(),I5Touch::icsm_quality_high_height.GetInt(),
        width,height,plan.input.width,plan.input.height,plan.output.width,plan.output.height,
        blurEffect,I5Touch::s_menuBlurSurfaceWidth,I5Touch::s_menuBlurSurfaceHeight,width,height,
        I5Touch::s_menuBlurScaleX,I5Touch::s_menuBlurScaleY,
        mask.x,mask.y,mask.width,mask.height,target.x,target.y,target.width,target.height,
        background.x,background.y,background.width,background.height,
        content.x,content.y,content.width,content.height,playStart.x,playStart.y,playStart.width,playStart.height,
        open?"true":"false",covers?"true":"false",I5Touch::s_menuBlurLayoutUpdates);
    return length>0 && size_t(length)<capacity?size_t(length):0;
}

extern "C" SOURCE_TOUCH_API size_t SourceTouchControllerStateJSON(char *buffer,size_t capacity)
{
    if (!buffer || !capacity || !g_pInputSystem) return 0;
    const char *names[]={"joystick","joystick_force_disabled","joystick_force_disabled_set_from_options",
        "joy_advanced","joy_advaxisx","joy_advaxisy","joy_advaxisr","joy_advaxisu",
        "joy_inverty","joy_movement_stick","joy_yawsensitivity","joy_pitchsensitivity",
        "zoom_sensitivity_ratio_joystick","option_duck_method","option_speed_method",
        "icsm_controller_move_deadzone","icsm_controller_look_deadzone",
        "joy_forwardthreshold","joy_sidethreshold","joy_pitchthreshold","joy_yawthreshold"};
    float v[ARRAYSIZE(names)]={};
    for (size_t i=0;i<ARRAYSIZE(names);++i) {
        ConVarRef ref(names[i],true); if (!ref.IsValid()) return 0;
        v[i]=ref.GetFloat(); if (!IsFinite(v[i])) v[i]=0;
    }
    unsigned buttons=0;
    const ButtonCode_t codes[]={KEY_XBUTTON_A,KEY_XBUTTON_B,KEY_XBUTTON_X,KEY_XBUTTON_Y,
        KEY_XBUTTON_LEFT_SHOULDER,KEY_XBUTTON_RIGHT_SHOULDER,KEY_XBUTTON_BACK,KEY_XBUTTON_START,
        KEY_XBUTTON_STICK1,KEY_XBUTTON_STICK2,KEY_XBUTTON_LTRIGGER,KEY_XBUTTON_RTRIGGER};
    for (size_t i=0;i<ARRAYSIZE(codes);++i) if (g_pInputSystem->IsButtonDown(codes[i])) buttons|=1u<<i;
    char effective[512]="{}";
    if (!I5Touch::ICSMControllerAxesJSON(effective,sizeof(effective))) V_strcpy_safe(effective,"{}");
    int length=V_snprintf(buffer,capacity,
        "{\"backend\":\"SDL GameController / original Source JoyStickMove and key bindings\","
        "\"setup_version\":%d,\"joystick_count\":%d,\"enabled\":%s,\"force_disabled\":%g,\"option_disabled\":%g,"
        "\"advanced\":%g,\"axis_map\":[%g,%g,%g,%g],\"invert_y\":%g,\"movement_stick\":%g,"
        "\"yaw_sensitivity\":%g,\"pitch_sensitivity\":%g,\"zoom_sensitivity\":%g,\"duck_method\":%g,\"walk_method\":%g,"
        "\"move_deadzone_percent\":%g,\"look_deadzone_percent\":%g,\"deadzone_forward_side_pitch_yaw\":[%g,%g,%g,%g],"
        "\"axes\":[%d,%d,%d,%d],\"button_mask\":%u,\"effective\":%s}",
        I5Touch::icsm_controller_setup_version.GetInt(),g_pInputSystem->GetJoystickCount(),v[0] && !v[1]?"true":"false",
        v[1],v[2],v[3],v[4],v[5],v[6],v[7],v[8],v[9],v[10],v[11],v[12],v[13],v[14],
        v[15],v[16],v[17],v[18],v[19],v[20],
        g_pInputSystem->GetAnalogValue(JOYSTICK_AXIS(0,JOY_AXIS_X)),g_pInputSystem->GetAnalogValue(JOYSTICK_AXIS(0,JOY_AXIS_Y)),
        g_pInputSystem->GetAnalogValue(JOYSTICK_AXIS(0,JOY_AXIS_U)),g_pInputSystem->GetAnalogValue(JOYSTICK_AXIS(0,JOY_AXIS_R)),buttons,effective);
    return length>0 && size_t(length)<capacity ? size_t(length) : 0;
}

extern "C" SOURCE_TOUCH_API size_t SourceTouchAimStateJSON(char *buffer,size_t capacity)
{
    if (!buffer || !capacity) return 0;
    const ICSMTouchAim::Settings settings=I5Touch::AimSettings();
    C_CSPlayer *player=C_CSPlayer::GetLocalCSPlayer();
    const Vector origin=player?player->GetAbsOrigin():vec3_origin;
    QAngle angles(0,0,0);if(engine)engine->GetViewAngles(angles);
    char map[128]="";if(engine)V_FileBase(engine->GetLevelName(),map,sizeof(map));
    int length=V_snprintf(buffer,capacity,
        "{\"mode\":%d,\"total\":%g,\"rifle_scope\":%g,\"sniper_scope\":%g,\"scope\":%d,\"last_acceleration\":%g,\"last_sensitivity\":%g,\"fire_sensitivity_shared\":true,\"timing\":\"actual UIKit sample intervals\","
        "\"map\":\"%s\",\"alive\":%s,\"team\":%d,\"origin\":[%g,%g,%g],\"angles\":[%g,%g,%g],\"move_type\":%d}",
        settings.mode,ICSMTouchAim::Sensitivity(settings.total),ICSMTouchAim::Sensitivity(settings.rifle),
        ICSMTouchAim::Sensitivity(settings.sniper),int(I5Touch::AimScope()),
        I5Touch::s_lastAim.acceleration,I5Touch::s_lastAim.sensitivity,
        map,player && player->IsAlive()?"true":"false",player?player->GetTeamNumber():0,
        origin.x,origin.y,origin.z,angles.x,angles.y,angles.z,player?int(player->GetMoveType()):0);
    return length>0 && size_t(length)<capacity?size_t(length):0;
}

extern "C" SOURCE_TOUCH_API bool SourceTouchMainMenuReady()
{
    CCSGO_MainMenu *menu = CCSGO_MainMenu::GetInstance();
    panorama::IUIPanel *root = menu ? menu->UIPanel() : NULL;
    panorama::IUIWindow *window = root ? root->GetParentWindow() : NULL;
    panorama::IUIPanel *play = root ? root->FindChildTraverse( "MainMenuNavBarPlay" ) : NULL;
    panorama::IUIPanel *moviePanel = root ? root->FindChildTraverse( "MainMenuMovie" ) : NULL;
    uint32 width = 0, height = 0;
    if ( moviePanel && moviePanel->ClientPtr() )
    {
        panorama::CMoviePlayer *movie = static_cast<panorama::CMoviePlayer *>( moviePanel->ClientPtr() );
        panorama::CVideoPlayerPtr video = movie->GetMovie();
        if ( video.Get() ) video->GetTextureSize( width, height );
    }
    const bool stateReady = engine && !engine->IsInGame() && GameUI().GetGameUIState() == CSGO_GAME_UI_STATE_MAINMENU;
    const bool rootVisible = root && root->BIsVisible();
    const bool rootReady = root && root->BReadyForDisplay();
    const bool updated = root && !root->BHasClass( "InitialDisplay" );
    const bool windowVisible = window && window->BIsVisible();
    const bool playVisible = play && play->BIsVisible();
    const bool playReady = play && play->BReadyForDisplay();
    const bool playLayout = play && play->GetActualLayoutWidth() > 0 && play->GetActualLayoutHeight() > 0;
    const bool movieReady = width > 0 && height > 0;
    const unsigned flags = stateReady | ( rootVisible << 1 ) | ( rootReady << 2 ) | ( updated << 3 ) |
        ( windowVisible << 4 ) | ( playVisible << 5 ) | ( playReady << 6 ) | ( playLayout << 7 ) | ( movieReady << 8 );
    static unsigned lastFlags = ~0u;
    if ( flags != lastFlags )
    {
        lastFlags = flags;
        Msg( "ICSM_LOBBY_STATE state=%d root_visible=%d root_ready=%d updated=%d window_visible=%d play_visible=%d play_ready=%d play_layout=%d movie=%ux%u\n",
            stateReady, rootVisible, rootReady, updated, windowVisible, playVisible, playReady, playLayout, width, height );
    }
    // InitialDisplay is a legacy class cleared only by MainMenu::Update();
    // this source tree has no caller for that method. Actual ready panels,
    // nonzero layout and the uploaded movie texture are the completion gates.
    return stateReady && rootVisible && rootReady && windowVisible &&
        playVisible && playReady && playLayout && movieReady;
}
