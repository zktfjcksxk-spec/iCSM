// Original shadow sampler assignments from togl/dxabstract.cpp. Only 2D
// declarations can be depth samplers (dx9asmtogl2.cpp WriteGLSLSamplerDefinitions).
#pragma once
#include <cctype>
#include <cstring>
namespace SourceMetal {
inline unsigned shaderShadowMask(const char *name,unsigned declared2D) {
    if(!name)return 0;
    const char *base=name;
    for(const char *p=name;*p;++p)if(*p=='/' || *p=='\\')base=p+1;
    struct Entry {const char *name;unsigned mask;};
    static const Entry entries[]={
        {"water_ps",1u<<7},{"infected_ps",1u<<1},{"phong_ps",(1u<<4)|(1u<<15)},
        {"vertexlit_and_unlit_generic_bump_ps",(1u<<8)|(1u<<15)},
        {"vertexlit_and_unlit_generic_ps",(1u<<8)|(1u<<15)},
        {"eye_refract_ps",1u<<6},{"eyes_flashlight_ps",1u<<4},
        {"worldtwotextureblend_ps",1u<<7},{"teeth_flashlight_ps",1u<<2},
        {"flashlight_ps",1u<<7},{"lightmappedgeneric_ps",1u<<15},
        {"character_ps",(1u<<8)|(1u<<11)}};
    for(const auto &entry:entries) {
        size_t n=0;
        while(entry.name[n] && base[n] && std::tolower(static_cast<unsigned char>(base[n]))==entry.name[n])++n;
        if(entry.name[n])continue;
        // Family names begin at the basename. In particular, the clothing
        // compositor customcharacter_ps30 has no shadow map at all: its s11
        // is a 3D grunge texture, not character_ps30's flashlight depth map.
        if(base[n] && base[n]!='.' && !std::isdigit(static_cast<unsigned char>(base[n])))continue;
        unsigned mask=entry.mask;
        if(!std::strcmp(entry.name,"character_ps") && (declared2D&(1u<<11)))mask=1u<<11;
        return mask&declared2D;
    }
    return 0;
}
}
