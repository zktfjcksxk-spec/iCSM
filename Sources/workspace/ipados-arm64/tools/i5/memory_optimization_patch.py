#!/usr/bin/env python3
"""I5-only CPU texture scratch lifetimes and compact shader runtime metadata."""
from pathlib import Path
C=Path.home()/'Library/Caches/csgo-ipados-arm64/i5/metal'
def edit(name,changes):
    p=C/name;s=p.read_text()
    for old,new in changes:
        if new in s:continue
        if name=='native_shaders.mm' and old=='    s->hasColor=s->msl.find(' and '    s->vertex=s->parsed->shader_type' in s:
            start=s.index('    s->vertex=s->parsed->shader_type')
            end=s.index(old,start);s=s[:start]+s[end:]
        assert s.count(old)==1,(name,old[:80],s.count(old))
        s=s.replace(old,new)
    p.write_text(s)
edit('native_objects.h',[
 ('    bool hasColor = false;', '''    bool hasColor = false;
    bool vertex = false;
    unsigned maxVertexAttrs = 0;
    std::array<unsigned char,16> vertexAttribMap{};
    std::vector<unsigned> samplerSlots;'''),
 ('    std::vector<unsigned> samplerSlots;','''    std::vector<unsigned> samplerSlots;
    struct Attribute { unsigned reg; int usage,index; };
    std::vector<Attribute> attributes;''')])
edit('native_resources.mm',[
 ('#include "native_objects.h"','#include "native_objects.h"\n#import <UIKit/UIKit.h>\n#include "texture_staging_pool.h"'),
 ('HRESULT lockImage(Image &ref,','#include "texture_staging_ios.inc"\nHRESULT lockImage(Image &ref,'),
 ('    configureLightmapShadow(image,usage,label);','    registerTextureMemory(image);\n    configureLightmapShadow(image,usage,label);'),
 ('std::shared_ptr<Texture> newTexture(', 'static void registerTextureMemory(const std::shared_ptr<Texture>& texture);\nstd::shared_ptr<Texture> newTexture('),
 ('    l.data.resize(l.imagePitch*l.region.size.depth);','''    if(icsm_metal_texture_staging_pool.GetBool())textureStaging.acquire(l.data,size_t(l.imagePitch)*l.region.size.depth);
    else l.data.resize(size_t(l.imagePitch)*l.region.size.depth);'''),
 ('    l.active=false;\n    return S_OK;\n}\nvoid beginCommand', '''    l.active=false;
    if(icsm_metal_texture_staging_pool.GetBool())textureStaging.recycle(l.data);
    return S_OK;
}
void beginCommand'''),
 ('void beginCommand(Device &d) {\n    @autoreleasepool {','''void beginCommand(Device &d) {
    @autoreleasepool {
    watchTextureMemoryWarnings();
    if(textureMemoryWarning.exchange(false))trimTextureScratch();''')])
edit('native_shaders.mm',[
 ('    s->hasColor=s->msl.find(','''    s->vertex=s->parsed->shader_type==MOJOSHADER_TYPE_VERTEX;
    s->vertexAttribMap.fill(0xff);
    if(s->vertex)for(int i=0;i<s->parsed->attribute_count;++i) {
        const auto& a=s->parsed->attributes[i];unsigned reg;
        if(sscanf(a.name,"v%u",&reg)!=1 || reg>=16)fatal("vertex attribute",@(a.name));
        s->vertexAttribMap[reg]=(a.usage<<4)|a.index;s->maxVertexAttrs=std::max(s->maxVertexAttrs,reg+1);
        s->attributes.push_back({reg,int(a.usage),a.index});
    }
    for(int i=0;i<s->parsed->sampler_count;++i)s->samplerSlots.push_back(s->parsed->samplers[i].index);
    s->hasColor=s->msl.find('''),
 ('    return s;\n    } // autoreleasepool','''    // Uniform packing uses type/index/count only. Names point into parseData;
    // clear those pointers before freeing the compiler's output and metadata.
    for(auto& uniform:s->uniforms)uniform.name=nullptr;
    shaderCacheReleasedTextBytes+=s->msl.capacity()+std::max(0,s->parsed->output_len);
    ++shaderCacheCompactedShaders;
    MOJOSHADER_freeParseData(s->parsed);s->parsed=nullptr;
    std::string().swap(s->msl);
    return s;
    } // autoreleasepool'''),
 ('bool vertex=shader.parsed->shader_type==MOJOSHADER_TYPE_VERTEX;','bool vertex=shader.vertex;')])
edit('source_device.mm',[
 ('        for(int i=0;i<vs.parsed->attribute_count;++i) {\n            auto &a=vs.parsed->attributes[i]; unsigned reg=0; if(sscanf(a.name,"v%u",&reg)!=1)fatal("attribute",@(a.name));',
  '        for(const auto& a:vs.attributes) {\n            const unsigned reg=a.reg;'),
 ('    for(int i=0;i<shader->parsed->attribute_count;++i) { auto &a=shader->parsed->attributes[i]; unsigned reg; if(sscanf(a.name,"v%u",&reg)!=1 || reg>=16)fatal("vertex attribute",@(a.name)); p->m_vtxAttribMap[reg]=(a.usage<<4)|a.index; p->m_maxVertexAttrs=std::max(p->m_maxVertexAttrs,reg+1); }',
  '    memcpy(p->m_vtxAttribMap,shader->vertexAttribMap.data(),sizeof(p->m_vtxAttribMap));p->m_maxVertexAttrs=shader->maxVertexAttrs;'),
 ('    for(int i=0;i<shader->parsed->sampler_count;++i) p->m_pixSamplerMask|=1u<<shader->parsed->samplers[i].index;',
  '    for(unsigned slot:shader->samplerSlots)p->m_pixSamplerMask|=1u<<slot;'),
 ('            for(int i=0;i<shader->parsed->sampler_count;++i) {\n                unsigned slot=shader->parsed->samplers[i].index,sourceSlot=slot+(vertex ? 16:0);',
  '            for(unsigned slot:shader->samplerSlots) {\n                unsigned sourceSlot=slot+(vertex ? 16:0);')])
edit('diagnostics_ios.mm',[
 ('extern "C" NSDictionary *SourceMetalBufferSnapshot();','extern "C" NSDictionary *SourceMetalBufferSnapshot();\nextern "C" NSDictionary *SourceMetalMemorySnapshot();'),
 ('@"buffers":SourceMetalBufferSnapshot(),','@"memory":SourceMetalMemorySnapshot(),@"buffers":SourceMetalBufferSnapshot(),')])
print('I5 texture staging pool and compact runtime shader metadata prepared')

if '#include "texture_staging_selftest_ios.inc"' not in (C/"source_device.mm").read_text():
    with (C/"source_device.mm").open("a") as f:f.write('\n#include "texture_staging_selftest_ios.inc"\n')
