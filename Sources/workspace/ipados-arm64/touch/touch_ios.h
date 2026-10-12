#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif
#define SOURCE_TOUCH_API __attribute__((visibility("default")))

/* Rect coordinates in UpdateState are ORIGINAL SOURCE RENDER PIXELS.
   Touch callbacks use UIKit points; the module applies no aim gain. */
typedef struct SourceTouchRect { float x, y, width, height; } SourceTouchRect;
typedef enum SourceTouchMode { SourceTouchModeMenu=0, SourceTouchModeGameplay=1 } SourceTouchMode;
typedef enum SourceTouchPhase { SourceTouchBegan=0, SourceTouchMoved=1, SourceTouchEnded=2, SourceTouchCancelled=3 } SourceTouchPhase;
typedef enum SourceTouchAction {
    SourceTouchAttack=0, SourceTouchAttack2=1, SourceTouchDuck=2,
    SourceTouchJump=3, SourceTouchReload=4, SourceTouchUse=5,
    SourceTouchWalk=6, SourceTouchActionCount=7
} SourceTouchAction;
typedef enum SourceTouchCommand {
    SourceTouchPause=0, SourceTouchScoreboard=1, SourceTouchBuy=2,
    SourceTouchInspect=3, SourceTouchDrop=4, SourceTouchCommandCount=5
} SourceTouchCommand;

typedef struct SourceTouchWeapon {
    int slot;             /* Actual Source inventory slot represented as 1..5. */
    int entity_index;     /* Actual weapon entity, supplied by the host. */
    int subtype;          /* Actual weapon subtype, supplied by the host. */
    bool selected;
    const char *name_utf8; /* Copied during UpdateState. May be NULL. */
} SourceTouchWeapon;

typedef struct SourceTouchState {
    uint32_t struct_size; /* sizeof(SourceTouchState), checked before reading. */
    SourceTouchMode mode;
    bool active;
    bool in_game; /* Exact engine IsInGame state, independent of menu/capture. */
    bool game_input; /* false with scoreboard open: only its close control responds. */
    bool scoreboard_open;
    bool radar_valid;
    bool score_valid;
    SourceTouchRect radar; /* Actual displayed original HUD bounds in render pixels. */
    SourceTouchRect score;
    float render_width;
    float render_height;
    const SourceTouchWeapon *weapons;
    size_t weapon_count;
    /* Physical calibration: native panel distance -> current UIKit screen. */
    float native_landscape_width_pixels; /* Design's M5 default: 2420. */
    float native_density_ppi;            /* Design's M5 default: 264. */
    float calibrated_points_per_mm;     /* >0 overrides panel-derived scale. */
} SourceTouchState;

typedef struct SourceTouchCallbacks {
    uint32_t struct_size; /* sizeof(SourceTouchCallbacks) */
    void (*action)(void *user, SourceTouchAction action, bool pressed, double timestamp);
    /* x positive right, y positive forward; unit circle, no acceleration. */
    void (*move)(void *user, float x, float y, double timestamp);
    /* Actual linear UIKit-point displacement, once per actual sample. */
    void (*look)(void *user, float dx_points, float dy_points, double timestamp);
    void (*command)(void *user, SourceTouchCommand command, double timestamp);
    void (*weapon_select)(void *user, int entity_index, int subtype, double timestamp);
    /* Called at cancel/mode/focus transitions: host discards queued look samples. */
    void (*reset)(void *user, double timestamp);
    /* Main-thread state refresh, called outside the UI mutex before routing
       or rendering. Must not drain queued input samples. */
    void (*frame)(void *user);
} SourceTouchCallbacks;

/* Native lobby navigation mirrors the original Panorama layout and buttons.
   This separate interface leaves the gameplay touch-state ABI unchanged. */
typedef enum SourceTouchNavigationAction {
    SourceNavigationHome=0, SourceNavigationPlay, SourceNavigationInventory,
    SourceNavigationSettings, SourceNavigationConsole, SourceNavigationRooms,
    SourceNavigationFPS, SourceNavigationQuit, SourceNavigationActionCount
} SourceTouchNavigationAction;
typedef struct SourceTouchNavigationState {
    uint32_t struct_size;
    bool visible;
    bool fps_enabled;
    bool title_visible;
    float render_width, render_height;
    SourceTouchRect bar;
    SourceTouchRect bar_right;
    SourceTouchRect title_bar;
    SourceTouchRect buttons[SourceNavigationActionCount];
    uint32_t enabled_mask, selected_mask;
    bool profile_visible;
    bool profile_enabled;
    SourceTouchRect profile;
    char profile_name_utf8[128]; /* Same byte limit as Source's player name. */
} SourceTouchNavigationState;
SOURCE_TOUCH_API void SourceTouchSetNavigationCallback(void (*callback)(SourceTouchNavigationAction));
SOURCE_TOUCH_API void SourceTouchSetProfileCallback(void (*callback)(void));
typedef enum SourceTouchGlassPresentation {
    SourceGlassNavigation=1u<<0, SourceGlassTitle=1u<<1, SourceGlassProfile=1u<<2
} SourceTouchGlassPresentation;
/* Visible native panels; Source hides only their corresponding original UI. */
SOURCE_TOUCH_API uint32_t SourceTouchUpdateNavigation(const SourceTouchNavigationState *state);

/* Transparent UIKit horizontal scrolling; covers and selection still belong
   to the original Panorama map list. Tile bounds are unscrolled content
   coordinates, relative to viewport, in Source render pixels. */
#define SOURCE_MAP_TILE_LIMIT 128
typedef struct SourceTouchMapTile {
    SourceTouchRect bounds;
    bool enabled, selected;
    char panel_id[192];
    char map_name[128];
} SourceTouchMapTile;
typedef struct SourceTouchMapState {
    uint32_t struct_size;
    bool visible;
    float render_width, render_height;
    SourceTouchRect viewport;
    float content_width, scroll_offset;
    uint32_t tile_count;
    char container_id[128];
    SourceTouchMapTile tiles[SOURCE_MAP_TILE_LIMIT];
} SourceTouchMapState;
SOURCE_TOUCH_API void SourceTouchSetMapCallbacks(void (*scroll)(float), void (*activate)(const char *));
SOURCE_TOUCH_API void SourceTouchUpdateMaps(const SourceTouchMapState *state);

/* UIKit scrolls the original settings viewport; Source keeps its widgets,
   cvars and clipping. Slider rectangles are in Source render pixels. */
#define SOURCE_SETTINGS_SLIDER_LIMIT 32
typedef struct SourceTouchSettingsState {
    uint32_t struct_size;
    bool visible;
    float render_width, render_height;
    SourceTouchRect viewport;
    float content_height, scroll_offset;
    char tab_id[64];
    uint32_t slider_count;
    SourceTouchRect sliders[SOURCE_SETTINGS_SLIDER_LIMIT];
} SourceTouchSettingsState;
SOURCE_TOUCH_API void SourceTouchSetSettingsScrollCallback(void (*scroll)(const char *, float));
SOURCE_TOUCH_API void SourceTouchUpdateSettings(const SourceTouchSettingsState *state);
/* Separate extension: preserve the existing settings/gameplay snapshot ABI.
   UIKit sends normalized positions to the original Source slider, which owns
   range mapping, inversion, ConVar changes and the displayed numeric value. */
typedef struct SourceTouchSettingsSlider {
    SourceTouchRect hit_bounds, track_bounds;
    float thumb_width, fraction;
    char convar[64];
} SourceTouchSettingsSlider;
typedef struct SourceTouchSettingsSliderTargets {
    uint32_t struct_size, count;
    SourceTouchSettingsSlider sliders[SOURCE_SETTINGS_SLIDER_LIMIT];
} SourceTouchSettingsSliderTargets;
SOURCE_TOUCH_API void SourceTouchSetSettingsSliderCallback(void (*change)(const char *, const char *, float, bool));
SOURCE_TOUCH_API void SourceTouchUpdateSettingsSliderTargets(const SourceTouchSettingsSliderTargets *targets);
SOURCE_TOUCH_API bool SourceTouchSettingsSliderSet(const char *convar, float fraction, bool finished);
/* Bounded local validation, without synthesising UIKit touches. */
SOURCE_TOUCH_API bool SourceTouchSettingsScrollTo(float pixels, bool animated);
SOURCE_TOUCH_API size_t SourceTouchSettingsScrollSelfTestJSON(char *buffer, size_t capacity);

/* Inventory UI snapshots are generated by the existing Panorama inventory
   script/API. UIKit sends only original panel IDs back for Activated events. */
SOURCE_TOUCH_API void SourceTouchSetInventoryCallback(void (*activate)(const char *));
SOURCE_TOUCH_API bool SourceTouchUpdateInventory(const char *json_utf8, bool visible);
/* Main-thread UIKit geometry publishes this cached menu-only scale. It never
   changes Source's 3D output resolution or saved gameplay touch layout. */
SOURCE_TOUCH_API float SourceTouchMenuUIScale(void);

/* One scene. Call before drawing; returns false on unsupported device/API. */
SOURCE_TOUCH_API bool SourceTouchInitialize(void *metal_device);
SOURCE_TOUCH_API void SourceTouchShutdown(void);
/* Opens the main-thread UIKit editor; saved coordinates are view fractions. */
SOURCE_TOUCH_API void SourceTouchOpenLayoutEditor(void);
SOURCE_TOUCH_API bool SourceTouchIsEditing(void);
/* Local editor validation uses the same add/map/delete/save UI actions. */
SOURCE_TOUCH_API bool SourceTouchLayoutEditorAction(const char *action,int scancode);
SOURCE_TOUCH_API bool SourceTouchKeyboardTestEdge(int scancode,bool pressed);
SOURCE_TOUCH_API void SourceTouchEditPlayerName(const char *current, void (*completion)(const char *));
SOURCE_TOUCH_API void SourceTouchExportErrorLogs(void);
SOURCE_TOUCH_API void SourceTouchOpenRoomBrowser(void (*completion)(const char *, const char *));
SOURCE_TOUCH_API void SourceTouchOpenFrameCalibration(void (*queue)(const char *), size_t (*scene)(char *,size_t),const char *action);
SOURCE_TOUCH_API void SourceTouchFrameCalibrationTick();
SOURCE_TOUCH_API void SourceTouchFrameCalibrationRecover(void (*queue)(const char *),size_t (*scene)(char *,size_t));
SOURCE_TOUCH_API void SourceTouchFrameCalibrationShutdown();
SOURCE_TOUCH_API void SourceTouchFrameCalibrationRestoreCommitted();
SOURCE_TOUCH_API bool SourceTouchSessionActive(void);
/* Synchronous bounded BGRA encode; filename is validated, pixels are not retained. */
SOURCE_TOUCH_API bool SourceTouchSaveInventoryPreview(const char *filename,const void *bgra,int width,int height);
/* Native console presentation retains Source's command and history host. */
SOURCE_TOUCH_API void SourceTouchConsoleSetVisible(bool visible);
SOURCE_TOUCH_API bool SourceTouchConsoleIsVisible(void);
SOURCE_TOUCH_API float SourceTouchConsoleAvailableHeight(void);
typedef struct SourceTouchConsoleState {
    uint32_t struct_size;
    float render_width, render_height;
    SourceTouchRect bounds, header;
} SourceTouchConsoleState;
SOURCE_TOUCH_API void SourceTouchUpdateConsole(const SourceTouchConsoleState *state, void (*close)(void));
SOURCE_TOUCH_API bool SourceTouchGetConsoleState(SourceTouchConsoleState *state);
SOURCE_TOUCH_API void SourceTouchConsoleSetCommandCallbacks(void (*submit)(const char *), void (*recall)(int));
SOURCE_TOUCH_API void SourceTouchConsoleAppendText(const char *text, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha);
SOURCE_TOUCH_API void SourceTouchConsoleClearText(void);
SOURCE_TOUCH_API void SourceTouchConsoleSetInputText(const char *text);
/* SDL MFi enumeration filters this one framework-created controller only. */
SOURCE_TOUCH_API bool SourceTouchOwnsGameController(void *game_controller);
SOURCE_TOUCH_API void SourceTouchSetCallbacks(const SourceTouchCallbacks *callbacks, void *user);
/* Optional timed look consumer. Uses the same user pointer and replaces look,
   keeping the original callback/state struct ABI unchanged. Interval is the
   actual UIKit sample's timestamp minus this look owner's previous sample. */
typedef void (*SourceTouchLookSampleCallback)(void *user, float dx_points, float dy_points,
                                             double timestamp, double sample_interval);
SOURCE_TOUCH_API void SourceTouchSetLookSampleCallback(SourceTouchLookSampleCallback callback);
/* Hide gameplay buttons and their hit targets; retain swipe and menu input.
   Separate extension keeps the existing gameplay snapshot ABI unchanged. */
SOURCE_TOUCH_API void SourceTouchSetButtonsHidden(bool hidden);
SOURCE_TOUCH_API void SourceTouchUpdateState(const SourceTouchState *state);
/* Main-thread attach from SDL_uikitview setSDLWindow. NULL is ignored because
   an older replaced view may detach after the new Metal view attaches. */
SOURCE_TOUCH_API void SourceTouchAttachView(void *view);
/* Main-thread hook from SDL UIKit layout/didMoveToWindow, and before touches.
   Captures view.bounds/safeArea/traits; render thread never queries UIKit. */
SOURCE_TOUCH_API void SourceTouchSetView(void *view);
/* view=UIView*, touches=singleton NSSet<UITouch*>*, event=UIEvent*.
   Keep the route chosen at touch-down through release/cancel, even if the UI
   changes mode. False continues the original SDL absolute-pointer path. */
SOURCE_TOUCH_API bool SourceTouchUIKitTouches(void *view, void *touches, void *event, int phase);
/* Called by SDL before a UIKit orientation transition; releases SDL fingers
   through SDL_SendTouch and cancels gameplay ownership, including 180 flips. */
SOURCE_TOUCH_API void SourceTouchUIKitOrientationChanging(void *view);
/* Append a load/store pass to the EXISTING command buffer/drawable texture,
   after Source presentation copy and before presentDrawable. No new queue. */
SOURCE_TOUCH_API void SourceTouchRenderMetal(void *command_buffer, void *drawable_texture);
/* Draw after the presentation triangle into its existing color-only encoder.
   The caller owns endEncoding. Keeps the standalone path for frame generation. */
SOURCE_TOUCH_API void SourceTouchRenderMetalWithEncoder(void *render_encoder, void *drawable_texture);
SOURCE_TOUCH_API void SourceTouchCancelAll(double timestamp);
/* Layout diagnostics. Returns bytes excluding NUL, or 0 for an invalid buffer. */
SOURCE_TOUCH_API size_t SourceTouchCopyDiagnosticsJSON(char *buffer, size_t capacity);
/* Exercises the SAME ownership/router functions with synthetic coordinate
   fixtures; never fabricates UITouch and never invokes host callbacks. */
SOURCE_TOUCH_API size_t SourceTouchRoutingSelfTestJSON(char *buffer, size_t capacity);

#if defined(__cplusplus)
}
#endif
