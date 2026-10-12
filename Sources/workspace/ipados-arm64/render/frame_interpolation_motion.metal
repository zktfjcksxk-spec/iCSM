#include <metal_stdlib>
using namespace metal;
struct FGVertex { float4 position [[position]]; float2 uv; };
struct FGCamera { float4x4 reprojection; float4 rect; uint2 size; uint reset; uint pad; };
vertex FGVertex fg_quad(uint id [[vertex_id]]) {
    float2 p=float2((id<<1)&2,id&2);
    return {float4(p*float2(2,-2)+float2(-1,1),0,1),p};
}
// Capture real Source world depth before its viewmodel depth-range hack.
struct FGDepthOut { float depth [[depth(any)]]; };
fragment FGDepthOut fg_depth(FGVertex v [[stage_in]],depth2d<float> depth [[texture(0)]],
                        constant FGCamera &c [[buffer(0)]]) {
    constexpr sampler s(coord::normalized,address::clamp_to_edge,filter::nearest);
    return {depth.sample(s,c.rect.xy+v.uv*c.rect.zw)};
}
float2 camera_motion(float2 uv,float z,constant FGCamera &c) {
    if(c.reset)return float2(0);
    float4 previous=c.reprojection*float4(uv.x*2-1,1-uv.y*2,z,1);
    if((c.pad && previous.w<=1e-6) || abs(previous.w)<1e-6)return float2(0);
    float2 oldUV=previous.xy/previous.w*float2(.5,-.5)+.5;
    float2 velocity=(oldUV-uv)*float2(c.size);
    // The bound texture is RG16Float. Retain large real motion without
    // overflowing half storage when previous projection lies far offscreen.
    return c.pad ? (all(isfinite(velocity))?clamp(velocity,float2(-65000),float2(65000)):float2(0)) :
        clamp(velocity,float2(-256),float2(256));
}
// A residual estimated on a coarse patch must not cross a depth discontinuity.
// Compare projected previous Z against the actual previous depth. The tolerance
// scales with (1-Z), approximately preserving a 3% view-depth tolerance.
bool fg_depth_consistent(float2 uv,float z,depth2d<float> previousDepth,constant FGCamera &c) {
    constexpr sampler point(coord::normalized,address::clamp_to_edge,filter::nearest);
    float4 previous=c.reprojection*float4(uv.x*2-1,1-uv.y*2,z,1);
    if(previous.w<=1e-6)return false;
    float3 ndc=previous.xyz/previous.w;
    float2 oldUV=ndc.xy*float2(.5,-.5)+.5;
    if(any(oldUV<0.f) || any(oldUV>1.f) || ndc.z<0.f || ndc.z>1.f)return false;
    float oldZ=previousDepth.sample(point,oldUV);
    return abs(oldZ-ndc.z)<=max(0.00002f,abs(1.f-ndc.z)*.03f);
}
float luminance(float3 c) { return dot(c,float3(.2126,.7152,.0722)); }
kernel void fg_resize(texture2d<float> color [[texture(0)]],texture2d<float,access::write> output [[texture(1)]],
                      constant FGCamera &c [[buffer(0)]],
                      uint2 p [[thread_position_in_grid]]) {
    if(p.x>=output.get_width() || p.y>=output.get_height())return;
    constexpr sampler s(coord::normalized,address::clamp_to_edge,filter::linear);
    const float2 uv=(float2(p)+.5)/float2(output.get_width(),output.get_height());
    output.write(color.sample(s,c.rect.xy+uv*c.rect.zw),p);
}
// Motion matching uses a compact, single-channel half-resolution image rather
// than repeatedly filtering the full-resolution BGRA scene. Motion units stay
// in input pixels; changing this texture's sampling density does not change UVs.
kernel void fg_luma(texture2d<float> color [[texture(0)]],texture2d<float,access::write> luma [[texture(1)]],
                    uint2 p [[thread_position_in_grid]]) {
    if(p.x>=luma.get_width() || p.y>=luma.get_height())return;
    constexpr sampler s(coord::normalized,address::clamp_to_edge,filter::linear);
    float2 uv=(float2(p)+.5)/float2(luma.get_width(),luma.get_height());
    luma.write(float4(luminance(color.sample(s,uv).rgb)),p);
}
// Source viewrender.cpp draws viewmodels with DepthRange(0,0.1). Detect actual
// newly written foreground pixels by comparing world depth with post-viewmodel
// depth. This is geometry coverage, not a guessed rectangle around the weapon.
kernel void fg_viewmodel_mask(depth2d<float> world [[texture(0)]],depth2d<float> after [[texture(1)]],
                    texture2d<float,access::write> mask [[texture(2)]],
                    constant FGCamera &c [[buffer(0)]],uint2 p [[thread_position_in_grid]]) {
    if(p.x>=c.size.x || p.y>=c.size.y)return;
    constexpr sampler s(coord::normalized,address::clamp_to_edge,filter::nearest);
    float2 uv=(float2(p)+.5)/float2(c.size);
    float before=world.read(p),now=after.sample(s,c.rect.xy+uv*c.rect.zw);
    mask.write(float4(now<=.10001f && before-now>.00001f?1.f:0.f),p);
}
kernel void fg_dilate(texture2d<float,access::read> source [[texture(0)]],texture2d<float,access::write> dest [[texture(1)]],
                      constant uint &vertical [[buffer(0)]],uint2 p [[thread_position_in_grid]]) {
    if(p.x>=dest.get_width() || p.y>=dest.get_height())return;
    float value=0;
    int2 limit=int2(source.get_width()-1,source.get_height()-1);
    for(int i=-4;i<=4;i++)value=max(value,source.read(uint2(clamp(int2(p)+(vertical?int2(0,i):int2(i,0)),int2(0),limit))).r);
    dest.write(float4(value),p);
}
kernel void fg_protect_viewmodel(texture2d<float,access::read_write> generated [[texture(0)]],
                                 texture2d<float,access::read> current [[texture(1)]],
                                 texture2d<float> currentMask [[texture(2)]],texture2d<float> previousMask [[texture(3)]],
                                 texture2d<float,access::read> scene [[texture(4)]],constant uint &protect [[buffer(0)]],
                                 uint2 p [[thread_position_in_grid]]) {
    if(p.x>=generated.get_width() || p.y>=generated.get_height())return;
    constexpr sampler s(coord::normalized,address::clamp_to_edge,filter::nearest);
    float2 uv=(float2(p)+.5)/float2(generated.get_width(),generated.get_height());
    // Also replace the previous footprint: otherwise the old knife's warped
    // silhouette survives beside the current one. The true current scene fills
    // this small disoccluded region. Weapon animation stays at the native rate.
    float4 ui=current.read(p);
    // The real before/after-HUD images identify exact drawn UI pixels at the
    // logical resolution. Composite them after expansion so text, crosshairs
    // and radar never go through low-resolution interpolation or unblending.
    if(any(ui.rgb!=scene.read(p).rgb) || (protect && max(currentMask.sample(s,uv).r,previousMask.sample(s,uv).r)>.5f))
        generated.write(ui,p);
}
float patch_error(texture2d<float> current,texture2d<float> previous,
                  float2 uv,float2 motion,float2 size) {
    constexpr sampler s(coord::normalized,address::clamp_to_edge,filter::linear);
    float result=0;
    // Finite patch matching estimates actual image displacement, including
    // animated characters. It does not claim to reproduce previous bone poses.
    for(int y=-1;y<=1;y++)for(int x=-1;x<=1;x++) {
        float2 p=uv+float2(x,y)*3/size;
        float a=current.sample(s,p).r;
        float b=previous.sample(s,p+motion/size).r;
        result+=min(abs(a-b),.15f);
    }
    return result;
}
kernel void fg_flow(texture2d<float> current [[texture(0)]],texture2d<float> previous [[texture(1)]],
                    depth2d<float> depth [[texture(2)]],texture2d<float,access::write> flow [[texture(3)]],
                    texture2d<float> mask [[texture(4)]],texture2d<float> previousMask [[texture(5)]],
                    depth2d<float> previousDepth [[texture(6)]],
                    constant FGCamera &c [[buffer(0)]],uint2 p [[thread_position_in_grid]]) {
    if(p.x>=flow.get_width() || p.y>=flow.get_height())return;
    constexpr sampler s(coord::normalized,address::clamp_to_edge,filter::nearest);
    float2 uv=(float2(p)+.5)/float2(flow.get_width(),flow.get_height());
    if(c.reset || max(mask.sample(s,uv).r,previousMask.sample(s,uv).r)>.5f){flow.write(float4(0),p);return;}
    float z=depth.sample(s,uv);
    if(c.pad && !fg_depth_consistent(uv,z,previousDepth,c)){flow.write(float4(0),p);return;}
    float2 base=camera_motion(uv,z,c),best=base;
    float error=patch_error(current,previous,uv,best,float2(c.size)),cameraError=error;
    float zero=patch_error(current,previous,uv,float2(0),float2(c.size));
    if(zero+0.002f<error){best=float2(0);error=zero;}
    if(!c.reset && error>(c.pad?.12f:.008f)) {
        // Coarse-to-fine, bounded +/-21 pixel residual search. All vectors
        // follow MetalFX's current-pixel -> previous-pixel convention.
        for(int level=0;level<3;level++) {
            float step=level==0?16:level==1?4:1;
            float2 center=best;
            int radius=1;
            for(int y=-radius;y<=radius;y++)for(int x=-radius;x<=radius;x++) {
                float2 candidate=center+float2(x,y)*step;
                float e=patch_error(current,previous,uv,candidate,float2(c.size));
                // Stable ties prefer the camera-derived motion.
                if(e+0.0005f<error){best=candidate;error=e;}
            }
        }
    }
    if(c.pad && !(cameraError>.12f && error<cameraError*.65f && cameraError-error>.08f))
        best=base;
    flow.write(float4(best-base,error,cameraError),p);
}
kernel void fg_motion(depth2d<float> depth [[texture(0)]],texture2d<float> flow [[texture(1)]],
                      texture2d<float,access::write> motion [[texture(2)]],
                      texture2d<float> current [[texture(3)]],texture2d<float> previous [[texture(4)]],
                      texture2d<float> mask [[texture(5)]],texture2d<float> previousMask [[texture(6)]],
                      depth2d<float> previousDepth [[texture(7)]],
                      constant FGCamera &c [[buffer(0)]],constant uint &refine [[buffer(1)]],
                      uint2 p [[thread_position_in_grid]]) {
    if(p.x>=c.size.x || p.y>=c.size.y)return;
    constexpr sampler s(coord::normalized,address::clamp_to_edge,filter::linear);
    float2 uv=(float2(p)+.5)/float2(c.size);
    if(max(mask.sample(s,uv).r,previousMask.sample(s,uv).r)>.5f){motion.write(float4(0),p);return;}
    float2 velocity=camera_motion(uv,depth.read(p),c);
    if(refine && !c.reset) {
        // Only change the per-pixel camera vector when the measured image
        // match improves. This avoids smearing coarse object residuals across
        // depth edges and preserves exact camera motion in static geometry.
        constexpr sampler point(coord::normalized,address::clamp_to_edge,filter::nearest);
        float4 f=flow.sample(point,uv);
        if(f.w-f.z>(c.pad?.08f:.01f) && (!c.pad || fg_depth_consistent(uv,depth.read(p),previousDepth,c))) {
            float a=current.sample(s,uv).r;
            float old=previous.sample(s,uv+velocity/float2(c.size)).r;
            float refined=previous.sample(s,uv+(velocity+f.xy)/float2(c.size)).r;
            float oldError=abs(a-old),newError=abs(a-refined);
            if(c.pad ? (newError+.025f<oldError && newError<oldError*.75f) : newError+.015f<oldError)
                velocity+=f.xy;
        }
    }
    motion.write(float4(c.reset?float2(0):velocity,0,0),p);
}
float4 fg_gamma(float4 c,constant float4 *gamma) {
    float3 q=clamp(c.rgb,0.0f,1.0f)*255;
    uint3 a=uint3(floor(q)),b=min(a+1,uint3(255));float3 f=fract(q);
    c.rgb=float3(mix(gamma[a.x].x,gamma[b.x].x,f.x),
                 mix(gamma[a.y].y,gamma[b.y].y,f.y),mix(gamma[a.z].z,gamma[b.z].z,f.z));
    return c;
}
fragment float4 fg_copy(FGVertex v [[stage_in]],texture2d<float> color [[texture(0)]],
                        constant float4 *gamma [[buffer(0)]]) {
    constexpr sampler s(coord::normalized,address::clamp_to_edge,filter::linear);
    return fg_gamma(color.sample(s,v.uv),gamma);
}
// Compose the native-resolution UI and actual weapon coverage at presentation.
// This is the same coverage rule as fg_protect_viewmodel, fused with the copy
// and gamma draw that every drawable already needs. No extra full-screen
// generated-texture read/modify/write pass during gameplay.
fragment float4 fg_present_generated(FGVertex v [[stage_in]],texture2d<float> generated [[texture(0)]],
                        texture2d<float,access::read> current [[texture(1)]],
                        texture2d<float,access::read> scene [[texture(2)]],
                        texture2d<float> currentMask [[texture(3)]],texture2d<float> previousMask [[texture(4)]],
                        constant float4 *gamma [[buffer(0)]],constant uint &protect [[buffer(1)]]) {
    constexpr sampler s(coord::normalized,address::clamp_to_edge,filter::nearest);
    const uint2 p=min(uint2(v.position.xy),uint2(current.get_width()-1,current.get_height()-1));
    float4 ui=current.read(p);
    bool overlay=any(ui.rgb!=scene.read(p).rgb) ||
        (protect && max(currentMask.sample(s,v.uv).r,previousMask.sample(s,v.uv).r)>.5f);
    return fg_gamma(overlay?ui:generated.sample(s,v.uv),gamma);
}
