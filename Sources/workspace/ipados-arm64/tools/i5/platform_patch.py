#!/usr/bin/env python3
"""Add the UIKit touch and existing Metal presentation hooks to I5 copies."""
from pathlib import Path

CACHE = Path.home() / "Library/Caches/csgo-ipados-arm64/i5"


def once(text, old, new):
    if text.count(old) != 1:
        raise RuntimeError(f"Expected exactly one source anchor: {old[:100]!r}")
    return text.replace(old, new, 1)


def patch_uikit():
    path = CACHE / "sdl/src/video/uikit/SDL_uikitview.m"
    text = path.read_text()
    if '/* I5 touch integration */' in text:
        return
    text = once(text, '#include "../../SDL_internal.h"',
                '#include "../../SDL_internal.h"\n/* I5 touch integration */\n#include "touch_ios.h"')
    for method, phase in (("Began", "SourceTouchBegan"), ("Moved", "SourceTouchMoved"),
                          ("Ended", "SourceTouchEnded"), ("Cancelled", "SourceTouchCancelled")):
        if method == "Cancelled":
            old = '''- (void)touchesCancelled:(NSSet *)touches withEvent:(UIEvent *)event
{
    [self touchesEnded:touches withEvent:event];
}'''
            # The ended handler retains each finger's original route and
            # sends the matching SDL release, so cancellation uses it too.
            if old not in text:
                raise RuntimeError("Expected original UIKit cancellation forwarding")
            continue
        anchor = f'- (void)touches{method}:(NSSet *)touches withEvent:(UIEvent *)event\n{{\n    for (UITouch *touch in touches) {{'
        hook = (f'\n        if (SourceTouchUIKitTouches((__bridge void *)self,\n'
                f'              (__bridge void *)[NSSet setWithObject:touch],\n'
                f'              (__bridge void *)event, {phase})) continue;')
        text = once(text, anchor, anchor + hook)
    text = once(text, '    sdlwindow = window;\n}',
                '    sdlwindow = window;\n    SourceTouchAttachView(window ? (__bridge void *)self : NULL);\n}')
    anchor = '\n#if !TARGET_OS_TV && defined(__IPHONE_13_4)\n- (UIPointerRegion *)pointerInteraction:'
    methods = '''
- (void)layoutSubviews
{
    [super layoutSubviews];
    SourceTouchSetView((__bridge void *)self);
}

- (void)didMoveToWindow
{
    [super didMoveToWindow];
    if (self.window) SourceTouchSetView((__bridge void *)self);
}

- (void)sourceTouchOrientationChanging
{
    unsigned released=0;
    for (int device=0; device<SDL_GetNumTouchDevices(); ++device) {
        SDL_TouchID id=SDL_GetTouchDevice(device);
        if (id!=directTouchId && id!=indirectTouchId) continue;
        int count=SDL_GetNumTouchFingers(id);
        while (count>0) {
            SDL_Finger *finger=SDL_GetTouchFinger(id,count-1);
            if (!finger) break;
            SDL_SendTouch(id,finger->id,sdlwindow,SDL_FALSE,finger->x,finger->y,0);
            --count;++released;
        }
    }
    SourceTouchCancelAll(NSProcessInfo.processInfo.systemUptime);
    fprintf(stderr,"ICSM_ORIENTATION_CANCEL released_sdl_fingers=%u\\n",released);
}
'''
    text = once(text, anchor, '\n' + methods + anchor)
    # A landscape-to-landscape transition keeps width/height unchanged. End
    # every original SDL finger explicitly, so its single-finger mouse latch
    # cannot keep swallowing later menu taps after UIKit cancels a gesture.
    text += '''
/* I5 orientation cancellation uses the original SDL touch release path. */
void SourceTouchUIKitOrientationChanging(void *view)
{
    SDL_uikitview *v=(__bridge SDL_uikitview *)view;
    [v sourceTouchOrientationChanging];
}
'''
    path.write_text(text)


def patch_orientation_controller():
    path = CACHE / "sdl/src/video/uikit/SDL_uikitviewcontroller.m"
    text = path.read_text()
    text = once(text, '#include "../../SDL_internal.h"',
                '#include "../../SDL_internal.h"\n/* I5 orientation integration */\n#include "touch_ios.h"')
    anchor = '    [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];'
    text = once(text, anchor, anchor + '\n    SourceTouchUIKitOrientationChanging((__bridge void *)self.view);')
    path.write_text(text)


def patch_metal():
    path = CACHE / "metal/source_device.mm"
    text = path.read_text()
    if '/* I5 touch integration */' in text:
        return
    text = once(text, '#include "native_objects.h"',
                '/* I5 touch integration */\n#include "touch_ios.h"\n#include "native_objects.h"')
    text = once(text, 'D3DTEXTUREFILTERTYPE filter, bool presentation = false) {',
                'D3DTEXTUREFILTERTYPE filter, bool presentation = false, bool touchOverlay = false) {')
    draw = '[e setFragmentSamplerState:sampler atIndex:0]; [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3]; [e endEncoding];'
    text = once(text, draw,
                '[e setFragmentSamplerState:sampler atIndex:0]; [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];\n'
                '    if(touchOverlay)SourceTouchRenderMetalWithEncoder((__bridge void *)e,(__bridge void *)dt);\n'
                '    [e endEncoding];')
    present_copy = '    Image dest; dest.image=tex; copyImage(d,native<Image>(m_pDefaultColorSurface),dest,nullptr,nullptr,D3DTEXF_LINEAR,true);'
    text = once(text, present_copy,
                '    const bool mergeTouch=icsm_metal_touch_pass_merge.GetBool() && native<Image>(m_pDefaultColorSurface).image->samples==1;\n'
                '    Image dest; dest.image=tex; copyImage(d,native<Image>(m_pDefaultColorSurface),dest,nullptr,nullptr,D3DTEXF_LINEAR,true,mergeTouch);')
    anchor = '    [d.command presentDrawable:drawable]; commit(d,false); ++d.frame; afterPresentCapture(d);'
    text = once(text, anchor,
                '    if(!mergeTouch)SourceTouchRenderMetal((__bridge void *)d.command, (__bridge void *)drawable.texture);\n' + anchor)
    path.write_text(text)


def patch_controller_enumeration():
    path = CACHE / "sdl/src/joystick/iphoneos/SDL_mfijoystick.m"
    text = path.read_text()
    if '/* I5 touch integration */' in text:
        return
    text = once(text, '#include "../../SDL_internal.h"',
                '#include "../../SDL_internal.h"\n/* I5 touch integration */\n#include "touch_ios.h"')
    anchor = 'static void IOS_AddJoystickDevice(GCController *controller, SDL_bool accelerometer)\n{'
    text = once(text, anchor, anchor + '''
    /* TouchController draws our overlay and owns a virtual GCController.
       Its events already enter Source through the explicit touch bridge. */
    if (controller && SourceTouchOwnsGameController((__bridge void *)controller)) return;
''')
    path.write_text(text)


if __name__ == "__main__":
    patch_uikit()
    patch_orientation_controller()
    patch_metal()
    patch_controller_enumeration()
    from controller_uikit_patch import apply as patch_controller_responder
    patch_controller_responder()
