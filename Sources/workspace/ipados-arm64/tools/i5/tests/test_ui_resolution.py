#!/usr/bin/env python3
"""Exercise actual scene-composition code and pixel budgets without gameplay."""
from pathlib import Path
import subprocess
import tempfile

P = Path(__file__).resolve().parents[3]
C = Path.home() / 'Library/Caches/csgo-ipados-arm64/i5'
source = (C / 'source/game/client/viewrender.cpp').read_text(errors='surrogateescape')
begin = source.index('    // Compare actual rectangles:')
end = source.index('\n\t// Clear a row of pixels', begin)
composition = source[begin:end].rstrip().removesuffix('#endif')
assert source.index('// Draw the 2D graphics', end) > end

fixture = r'''
#define SOURCE_IOS 1
#include "scene_resolution_policy.h"
#include "metal_spatial_ios.h"
#include <cassert>
#include <cstdio>
#include <vector>
struct Rect_t {int x,y,width,height;};
struct View {int x,y,width,height,m_nUnscaledX,m_nUnscaledY,m_nUnscaledWidth,m_nUnscaledHeight;};
struct Var {float value=0;bool GetBool(){return value!=0;}int GetInt(){return int(value);}float GetFloat(){return value;}};
Var mat_viewportupscale{1}, icsm_scene_render_percent{100}, icsm_scene_output_width, icsm_scene_output_height;
bool split=false, spatialAvailable=true;
bool VGui_IsSplitScreen(){return split;}
struct ITexture{} texture;
struct MaterialSystem {ITexture *FindTexture(const char*,int){return &texture;}} systemStorage, *materials=&systemStorage;
constexpr int TEXTURE_GROUP_RENDER_TARGET=0;
struct Operation {int kind;Rect_t input,output;};
std::vector<Operation> operations;
int canvasWidth=0,canvasHeight=0;
void bounds(Rect_t r){assert(r.x>=0 && r.y>=0 && r.width>0 && r.height>0);assert(r.x+r.width<=canvasWidth && r.y+r.height<=canvasHeight);}
struct Context {
    void Flush(bool){}
    void CopyRenderTargetToTextureEx(ITexture*,int,Rect_t *in,Rect_t *out){bounds(*in);bounds(*out);operations.push_back({0,*in,*out});}
    void CopyTextureToRenderTargetEx(int,ITexture*,Rect_t *in,Rect_t *out){bounds(*in);bounds(*out);operations.push_back({1,*in,*out});}
} context;
struct CMatRenderContextPtr {CMatRenderContextPtr(MaterialSystem*){}Context *operator->(){return &context;}};
bool spatial(const ICSMSceneRect *in,const ICSMSceneRect *out){
    if(!spatialAvailable)return false;
    Rect_t i{in->x,in->y,in->width,in->height},o{out->x,out->y,out->width,out->height};
    bounds(i);bounds(o);assert(o.width>=i.width && o.height>=i.height);
    operations.push_back({2,i,o});return true;
}
constexpr int RTLD_DEFAULT=0;
void *dlsym(int,const char*){return reinterpret_cast<void*>(&spatial);}
void composite(View view) {
__COMPOSITION__
}
using namespace ICSMDisplay;
void run(Size ui,Size recipe,float scale,bool fx){
    const auto plan=ScenePlan(recipe,scale,ui);
    canvasWidth=ui.width;canvasHeight=ui.height;
    icsm_scene_output_width.value=recipe.width;icsm_scene_output_height.value=recipe.height;
    icsm_scene_render_percent.value=scale;spatialAvailable=fx;operations.clear();
    composite({0,0,int(plan.input.width),int(plan.input.height),0,0,int(ui.width),int(ui.height)});
    if(plan.input.width!=ui.width || plan.input.height!=ui.height){
        assert(!operations.empty());const auto &last=operations.back();
        assert(last.output.width==int(ui.width) && last.output.height==int(ui.height));
    } else assert(operations.empty());
}
int main(){
    const Size devices[]={{2816,1940},{2868,1320},{2732,2048},{1920,1080},{1136,640}};
    for(auto ui:devices){
        const Size recipes[]={ui,PhoneMedium(ui),Preset(ui,720,false),Preset(ui,720,true)};
        for(auto recipe:recipes)for(float scale:{50.f,66.66667f,80.f,100.f}){
            auto p=ScenePlan(recipe,scale,ui);
            assert(p.input.width<=p.output.width && p.input.height<=p.output.height);
            assert(p.output.width<=ui.width && p.output.height<=ui.height);
            assert(p.output.width<=recipe.width && p.output.height<=recipe.height);
            if(recipe.width<=ui.width && recipe.height<=ui.height){
                assert(p.output.width==recipe.width && p.output.height==recipe.height);
                // Exact former Source viewport truncation; no extra 3D pixels.
                assert(p.input.width==unsigned(recipe.width*(scale/100.0f)));
                assert(p.input.height==unsigned(recipe.height*(scale/100.0f)));
            }
            run(ui,recipe,scale,true);run(ui,recipe,scale,false);
        }
    }
    Size ipad{2816,1940};
    run(ipad,PhoneMedium(ipad),100,true);assert(operations.size()==2 && operations[0].kind==0 && operations[1].kind==1);
    run(ipad,Preset(ipad,720,true),66.66667f,true);
    assert(operations.size()==3 && operations[0].kind==2);
    assert(operations[0].input.height==480 && operations[0].output.height==720);
    run(ipad,Preset(ipad,720,true),66.66667f,false);assert(operations.size()==4);
    run(ipad,ipad,100,true);assert(operations.empty());
    run(ipad,ipad,80,true);assert(operations.size()==1 && operations[0].kind==2);
    // Disabled explicit recipe retains the original viewport-scale fallback.
    operations.clear();icsm_scene_output_width.value=0;icsm_scene_output_height.value=0;
    composite({0,0,1408,970,0,0,2816,1940});assert(operations.size()==1);
    auto zero=ScenePlan({0,0},100,ipad);assert(zero.input.width==0 && zero.output.height==0);
    puts("native UI / unchanged scene budgets / bounded composition / MetalFX fallback: passed");
}
'''.replace('__COMPOSITION__', composition)

with tempfile.TemporaryDirectory(prefix='icsm-ui-resolution-') as directory:
    path = Path(directory)
    (path / 'test.cpp').write_text(fixture)
    subprocess.run(['xcrun', 'clang++', '-std=c++17', '-fsanitize=address,undefined',
                    '-I', str(P / 'touch'), '-I', str(P / 'render'),
                    str(path / 'test.cpp'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
