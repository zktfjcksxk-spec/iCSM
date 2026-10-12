# iPad touch controls in the existing Metal frame

`touch_ios.h` is the public C ABI. `touch_ui_ios.mm` has no SDL or Source includes; it compiles into the existing I5 SDL framework as a separate Objective-C++ object. All exported functions retain default visibility even when the framework uses `-fvisibility=hidden`.

Compile with the iPhoneOS SDK, arm64, C++17 and `-fobjc-arc`. Link TouchController, GameController, Metal, QuartzCore, UIKit, CoreGraphics, CoreText and Foundation. The checked SDK is iPhoneOS 27.0; TouchController is available from iOS 26.0.

## Integration

1. Call `SourceTouchAttachView(view)` after SDL assigns the native Metal view's window. Only a view whose layer is `CAMetalLayer` is accepted. A replaced ordinary SDL view cannot overwrite its geometry. `NULL` detach is ignored because an old view can detach after the replacement attaches; focus and explicit shutdown handle input cancellation.
2. Pass UIKit touches and their original `UIEvent` to `SourceTouchUIKitTouches`, using phases 0/1/2/3 for began/moved/ended/cancelled. A `false` result preserves SDL's original pointer path. Indirect pointer events always remain on that path. Gameplay consumes direct touches, including ignored areas, to prevent SDL mouse synthesis from generating additional view rotation.
3. Register callbacks using `SourceTouchSetCallbacks`. They append ordered input to the Source bridge; they do not invoke Source key methods from UIKit. The `frame` callback refreshes actual game and UI state on the main thread, outside the UI mutex. A render callback on another thread schedules one pending refresh on the main queue.
4. Publish actual HUD bounds in **Source render pixels**, including the associated render width and height. The module converts them to the attached view's UIKit points. Publish actual inventory slot, entity index, subtype, displayed name and current selection. Empty slots cannot select a weapon. A repeated grenade-slot tap selects the next actual inventory entry in that slot.
5. Append `SourceTouchRenderMetal(commandBuffer, drawable.texture)` after Source's presentation copy and before `presentDrawable`. It creates a load/store render pass on that same command buffer and drawable, and draws using `TCTouchController`. It creates no additional queue or rendering backend.
6. Filter only this module's virtual controller in SDL MFi enumeration using `SourceTouchOwnsGameController`. TouchController requires `connect` to render and creates a `GCController`; allowing that controller into SDL would duplicate input. The filter compares object identity, with Apple's public product category as a fallback only during the synchronous `connect` call. Real external controllers remain available.

## Layout and ownership

The layout follows `design/touch-controls-v1.json` and the root `output/four-finger-controls.html`. The radar and score anchors are supplied by the original HUD. The jump top edge is 20 mm below the radar bottom; the weapon row ends 20 mm from the view's right edge and is 10 mm shorter than the draft's former height. Physical distances use the actual screen's UIKit width and the design's native panel width/density, with an explicit points-per-mm calibration override. Narrow windows may require clamping the weapon row height; diagnostics expose the former and resulting heights.

Saved layout overrides replace the default positions and sizes. The settings entry “触摸屏键位调整” opens a UIKit editor; drafts become persistent only on Save. Inspect and Drop are editable controls that invoke the original paired weapon-inspect commands and drop command.

The touch settings option “隐藏所有触屏按钮” saves `icsm_touch_hide_buttons` (default 0). It skips both gameplay overlay render paths and removes button hit targets, while retaining left/right swipe ownership and normal menu input. Switching releases held actions and queued look samples before changing hit geometry. External keyboard/controller input and saved layout coordinates remain unchanged; the layout editor can still preview every button while hidden.

## Private room browser

The main-menu “搜索房间” button invokes `icsm_room_browser`. `room_browser.inc` presents native LAN and Internet tabs, a saved-address input with the system keyboard, refresh, room details and a return button. LAN uses declared `_icsm._udp` Bonjour services; the Internet tab queries player-supplied addresses. It does not rely on a Steam master list. Passwords are entered for a join and are never saved with the address list.

`room_query.h` sends A2S_INFO over a connected UDP socket on a background queue, handles the original challenge reply and parses bounded fields from the 2019 `engine/baseserver.cpp` serialization. Address validation prevents console-command injection. The Source bridge sets the password as a ConVar value and then uses the original `connect` command. Queries and Bonjour callbacks are discarded after the user leaves or starts a new search.

The Mac private server publishes Bonjour during its lifetime. Its NO_STEAM engine uses the original A2S_INFO handler with challenge and rate checks even with `-nomaster`, and announces no Steam lobby reservation. Legacy private servers that suppress A2S_INFO must be updated before they can appear in this browser. Public address reachability still depends on the server network.

Every touch receives a permanent owner at began. Buttons retain their action until end or cancellation even when a finger leaves the button. Only a touch that starts on empty space in the entire right half of the view can emit look. Every control's whole hit rectangle excludes look, including disabled and empty weapon slots. Leaving the look area or crossing a control updates the baseline without emitting displacement, so re-entry does not jump. Movement and look have separate owners; held actions aggregate all touch owners and release only when their count reaches zero.

Look callbacks report each actual floating-point UIKit displacement once, in timestamp order. Actual coalesced moved samples are included; predicted samples are never queried. Ended processes its final real position before releasing the owner. Cancelled adds no final movement and clears held actions, movement and queued relative samples. The UI applies no sensitivity, acceleration, smoothing, inertia, frame-time scaling or render-resolution scaling. The optional `SourceTouchSetLookSampleCallback` replaces legacy look delivery and includes the interval between actual UIKit samples. Existing callback structs and their ABI remain unchanged.

The Source bridge defaults to fixed speed (`icsm_touch_aim_mode 0`). Optional velocity mode (`1`) smoothly raises gain from 1x at 120 UIKit points/second to 2x at 720 points/second, using actual sample intervals rather than rendering frame time. It keeps no velocity history or inertia. Paused/invalid intervals use 1x. Archived overall, AUG/SG 553 scoped and sniper scoped sensitivity multipliers default to 1 and are adjustable from 0.1 to 3 in the touch settings page. Scoped multipliers apply on top of overall sensitivity only while the actual local player is scoped with that weapon type; firing shares the same path and sensitivity. Source still applies its original per-sample pitch limits.

Mode changes, scoreboard opening and focus loss cancel gameplay owners. In menu mode SDL's original absolute pointer input remains active. With the scoreboard open, its touch close control remains usable while gameplay movement and look are disabled. Inventory label/selection changes refresh existing controls without cancelling other fingers.

## Settings sliders

Settings use wide tracks below each label, larger thumbs and numeric values. The transparent UIKit settings input layer accepts track taps and horizontal drags with a target height of at least 44 logical points. A thumb grab preserves the finger offset; a track tap selects that position directly. Gesture direction locks after six points, so vertical scrolling never changes a setting and a horizontal slider drag survives later diagonal motion. Cancellation releases the owner and commits its last applied value. Only the initial track owns the drag; buttons and numeric entries keep the original SDL pointer path.

The separate slider-target ABI publishes each original widget's ConVar, track/thumb geometry and normalized position without changing existing gameplay or settings snapshots. The bridge finds the enabled widget in the active settings tab and calls its original `CSlider::SetValue`, retaining Source's `SliderValueChanged` handler, actual range, inversion and labels. It writes the configuration once on release rather than on every motion sample. External mice retain the original path. No extra UIKit drawing, blur, textures or render passes are added.

## Verification

`SourceTouchCopyDiagnosticsJSON` reports actual view/drawable sizes, safe area, physical scale, every control's expected rectangle and native center, active touches, sample counters, rendered frames and successful MFi filter matches.

`SourceTouchRoutingSelfTestJSON` requires a rendered gameplay layout with no active real touches. It exercises the same routing functions against that actual layout using synthetic coordinates. It temporarily replaces callbacks and removes native-control references from test copies, then restores real objects and metrics. It tests four simultaneous owners, every control dragged into look, exclusion/re-entry, fractional samples, fast/slow equal paths, reference-counted held buttons, end and cancellation. This verifies routing logic; physical four-finger ergonomics and framework visuals still require testing on the iPad.

The module passed arm64 iPhoneOS 27 SDK syntax validation with ARC, C++17, `-Wall` and `-Wextra`. Device rendering and real touch validation are recorded by the I5 host.

Primary API references: [TouchController](https://developer.apple.com/documentation/touchcontroller), [TCTouchController](https://developer.apple.com/documentation/touchcontroller/tctouchcontroller), and the installed SDK's TouchController public headers. The implementation uses the declared Objective-C methods directly.
