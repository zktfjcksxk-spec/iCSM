#include "native_objects.h"
#import <CommonCrypto/CommonDigest.h>
#include <cstdio>
#include <regex>
#include <atomic>
namespace SourceMetal {
static unsigned bytecodeSize(const DWORD *p) {
    // SM2/SM3 instruction length excludes its opcode token. Source supplies
    // validated VCS bytecode with an END token, as in dx9asmtogl2.cpp.
    unsigned n=1;
    while (n<262144) {
        const unsigned token=p[n++], op=token & 0xffff;
        if (op==0xffff) return n*4;
        if (op==0xfffe) n+=(token>>16)&0x7fff; // D3DSIO_COMMENT
        else n+=(token>>24)&15;
    }
    fatal("shader bytecode", @"unterminated or oversized shader"); return 0;
}
static unsigned shadowMask(const char *name, unsigned declared) {
    // Exact Source mask selection from dxabstract.cpp, including the
    // character shader's exclusive sampler rule in dx9asmtogl2.cpp.
    if (!name) return 0;
    struct Entry { const char *name; unsigned mask; };
    static const Entry entries[]={
        {"water_ps",1<<7},{"infected_ps",1<<1},{"phong_ps",(1<<4)|(1<<15)},
        {"vertexlit_and_unlit_generic_bump_ps",(1<<8)|(1<<15)},
        {"vertexlit_and_unlit_generic_ps",(1<<8)|(1<<15)},
        {"eye_refract_ps",1<<6},{"eyes_flashlight_ps",1<<4},
        {"worldtwotextureblend_ps",1<<7},{"teeth_flashlight_ps",1<<2},
        {"flashlight_ps",1<<7},{"lightmappedgeneric_ps",1<<15},
        {"character_ps",(1<<8)|(1<<11)}};
    for (const auto &e:entries) if (V_stristr(name,e.name)) {
        unsigned mask=e.mask;
        if (!strcmp(e.name,"character_ps") && (declared&(1<<11))) mask=1<<11;
        return mask & declared;
    }
    return 0;
}
static std::string sourceCompatibleMSL(const DWORD *code, const MOJOSHADER_parseData &p,
    const char *name, const uint32 *centroid, const std::vector<std::string> &centroidDecls) {
    std::string text="#include <metal_stdlib>\n";
    text.append(p.output,p.output_len);
    // The pinned upstream Metal profile adds a statement terminator to
    // uniform array aliases (emit_METAL_uniform). An alias is an expression;
    // preserve the generated register mapping and remove only that terminator.
    text=std::regex_replace(text, std::regex("(#define [cib][0-9]+ [^\\n;]+\\[[0-9]+\\]);"), "$1");
    if (p.shader_type==MOJOSHADER_TYPE_PIXEL) {
        unsigned declared=0;
        for (int i=0;i<p.sampler_count;++i) declared|=1u<<p.samplers[i].index;
        const unsigned mask=shadowMask(name,declared);
        // Recover every TEX/TEXLDL source swizzle from the original tokens.
        // Source treats shadow TEXLDP as an unprojected xyz depth comparison.
        struct Sample { unsigned sampler; std::string reg, xyz, lod; unsigned op; };
        std::vector<Sample> samples;
        unsigned cursor=1;
        const unsigned wordCount=bytecodeSize(code)/4;
        while (cursor<wordCount) {
            unsigned tok=code[cursor++],op=tok&0xffff;
            if (op==0xffff) break;
            if (op==0xfffe) { cursor+=(tok>>16)&0x7fff; continue; }
            unsigned count=(tok>>24)&15;
            if ((op==66 || op==95) && count>=3) { // TEX / TEXLDL
                unsigned src=code[cursor+1],sam=code[cursor+2]&0x7ff;
                if (mask&(1u<<sam)) {
                    unsigned type=((src>>28)&7)|((src>>8)&0x18),num=src&0x7ff;
                    const char *prefix=type==0 ? "r" : type==1 ? "v" : type==2 ? "c" : nullptr;
                    if (!prefix) fatal("shadow operand", @"unsupported register class");
                    std::string reg=std::string(prefix)+std::to_string(num),sw;
                    for (unsigned c=0;c<4;++c) sw.push_back("xyzw"[(src>>(16+2*c))&3]);
                    samples.push_back({sam,reg,reg+"."+sw.substr(0,3),reg+"."+sw.substr(3,1),op});
                }
            }
            cursor+=count;
        }
        for (int i=0;i<p.sampler_count;++i) {
            unsigned sampler=p.samplers[i].index;
            if (!(mask&(1u<<sampler))) continue;
            std::string from="texture2d<float> s"+std::to_string(sampler)+"_texture";
            auto at=text.find(from);
            if (at==std::string::npos) fatal("shadow sampler", @"expected 2D texture declaration");
            text.replace(at,from.size(),"depth2d<float> s"+std::to_string(sampler)+"_texture");
            const std::string call="s"+std::to_string(sampler)+"_texture.sample(";
            size_t pos=0; unsigned sampleIndex=0;
            for (const auto &sample:samples) {
                if (sample.sampler!=sampler) continue;
                pos=text.find(call,pos);
                if (pos==std::string::npos) fatal("shadow sample", @"token/MSL sample count differs");
                size_t end=pos+call.size(); unsigned nesting=1;
                while (end<text.size() && nesting) { if(text[end]=='(')++nesting; if(text[end]==')')--nesting; ++end; }
                if (nesting) fatal("shadow sample", @"unbalanced generated call");
                const std::string s="s"+std::to_string(sampler);
                std::string repl="float4("+s+"_texture.sample_compare("+s+", ("+sample.xyz+").xy, ("+sample.xyz+").z";
                if (sample.op==95) repl+=", level("+sample.lod+")";
                repl+="))";
                text.replace(pos,end-pos,repl); pos+=repl.size(); ++sampleIndex;
            }
            if (text.find(call)!=std::string::npos) fatal("shadow sample", @"unhandled texture operation");
        }
        // Centroid DCL flags are not supported by the pinned generator.
        // Reapply their interpolation semantics to the generated Metal input.
        for(const auto &reg:centroidDecls) {
            const std::string field="float4 "+reg+" [[";
            auto at=text.find(field); if(at==std::string::npos)fatal("centroid declaration",@(reg.c_str()));
            auto end=text.find("]]",at); if(end==std::string::npos)fatal("centroid attribute",@(reg.c_str()));
            text.insert(end,", centroid_perspective");
        }
        if (centroid) for (unsigned i=0;i<16;++i) if (*centroid&(1u<<i)) {
            std::string from="[[user(texcoord"+std::to_string(i)+")]]";
            size_t pos=0; while((pos=text.find(from,pos))!=std::string::npos) {
                std::string to="[[user(texcoord"+std::to_string(i)+"), centroid_perspective]]";
                text.replace(pos,from.size(),to); pos+=to.size();
            }
        }
        // D3D9's fixed alpha test is evaluated after the original shader.
        // The control buffer is separate from the packed shader constants.
        if (text.find("float4 oC0 [[color(0)]]")!=std::string::npos) {
            auto args=text.find("fragment source_main_Output source_main (");
            if (args==std::string::npos) fatal("alpha test", @"missing generated function");
            args+=strlen("fragment source_main_Output source_main (");
            const auto next=text.find_first_not_of(" \t\r\n",args);
            text.insert(args,next!=std::string::npos && text[next]==')' ? "\n constant float4 &source_alpha [[buffer(30)]]" : "\n constant float4 &source_alpha [[buffer(30)]],");
            const std::string marker="return output;";
            auto ret=text.rfind(marker);
            if (ret==std::string::npos) fatal("alpha test", @"missing generated return");
            text.insert(ret,
                "if (source_alpha.x != 0.0) {\n"
                " float a=output.oC0.a, ref=source_alpha.y; uint fn=uint(source_alpha.z);\n"
                " bool pass=fn==1 ? false : fn==2 ? a<ref : fn==3 ? a==ref : fn==4 ? a<=ref : fn==5 ? a>ref : fn==6 ? a!=ref : fn==7 ? a>=ref : true;\n"
                " if (!pass) discard_fragment();\n}\n");
        }
    }
    // The pinned Metal profile emits TEXLDL as implicit-LOD TEXLD. Source's
    // SM3 instruction explicitly samples the LOD in the fourth source component.
    // Match generated sample calls in their original per-sampler token order.
    std::map<unsigned,size_t> samplePositions;
    for(unsigned cursor=1,words=bytecodeSize(code)/4;cursor<words;) {
        const unsigned tok=code[cursor++],op=tok&0xffff;
        if(op==0xffff)break;
        if(op==0xfffe) { cursor+=(tok>>16)&0x7fff; continue; }
        const unsigned count=(tok>>24)&15;
        if((op==66 || op==93 || op==95) && count>=3) {
            const unsigned sampler=code[cursor+2]&0x7ff;
            const std::string call="s"+std::to_string(sampler)+"_texture.sample(";
            auto &position=samplePositions[sampler]; auto start=text.find(call,position);
            // Project shadow samples have already been adapted above.
            if(start!=std::string::npos) {
                size_t end=start+call.size(); unsigned nesting=1;
                while(end<text.size() && nesting) { if(text[end]=='(')++nesting; if(text[end]==')')--nesting; ++end; }
                if(nesting)fatal("texture LOD",@"unbalanced generated sample");
                if(op==95) {
                    const unsigned src=code[cursor+1],type=((src>>28)&7)|((src>>8)&0x18),modifier=(src>>24)&15;
                    const char *prefix=type==0 ? "r":type==1 ? "v":type==2 ? "c":nullptr;
                    if(!prefix || modifier || (src&(1u<<13)))fatal("texture LOD operand",@"unsupported original register addressing");
                    std::string lod=std::string(prefix)+std::to_string(src&0x7ff)+"."+"xyzw"[(src>>22)&3];
                    const std::string extra=", level("+lod+")"; text.insert(end-1,extra); end+=extra.size();
                }
                position=end;
            }
        }
        cursor+=count;
    }
    return text;
}
std::unique_ptr<Shader> newShader(const DWORD *code,const char *name,const char *label,const uint32 *centroid) {
    @autoreleasepool {
    auto s=std::make_unique<Shader>();
    static std::atomic<unsigned> serial{0}; s->serial=++serial;
    s->name=name ?: ""; s->label=label ?: s->name;
    const unsigned size=bytecodeSize(code);
    std::vector<DWORD> parseCode(code,code+size/4);
    std::vector<std::string> centroidDecls;
    for(unsigned cursor=1;cursor<size/4;) {
        const unsigned tok=parseCode[cursor++],op=tok&0xffff;
        if(op==0xffff)break;
        if(op==0xfffe) { cursor+=(tok>>16)&0x7fff; continue; }
        const unsigned count=(tok>>24)&15;
        if(op==31 && count==2 && (parseCode[cursor+1]&(4u<<20))) { // DCL MOD_CENTROID
            auto &dest=parseCode[cursor+1]; unsigned type=((dest>>28)&7)|((dest>>8)&0x18);
            if(type!=1 && type!=3)fatal("centroid register",@"unexpected original input class");
            centroidDecls.push_back(std::string(type==3 ? "t":"v")+std::to_string(dest&0x7ff));
            dest&=~(4u<<20);
        }
        cursor+=count;
    }
    s->parsed=MOJOSHADER_parse(MOJOSHADER_PROFILE_METAL,"source_main",reinterpret_cast<const unsigned char*>(parseCode.data()),size,nullptr,0,nullptr,0,nullptr,nullptr,nullptr);
    if (s->parsed->error_count) {
        for (int i=0;i<s->parsed->error_count;++i) fprintf(stderr,"Metal shader %s token %d: %s\n",s->label.c_str(),s->parsed->errors[i].error_position,s->parsed->errors[i].error);
        fatal("shader parse",@(s->label.c_str()));
    }
    s->msl=sourceCompatibleMSL(code,*s->parsed,name,centroid,centroidDecls);
    for(int i=0;i<s->parsed->sampler_count;++i) {
        const unsigned slot=s->parsed->samplers[i].index;
        if(s->msl.find("depth2d<float> s"+std::to_string(slot)+"_texture")!=std::string::npos)s->shadowSamplerMask|=1u<<slot;
    }
    s->hasColor=s->msl.find("float4 oC0 [[color(0)]]")!=std::string::npos;
    for (int i=0;i<s->parsed->uniform_count;++i) {
        const auto &u=s->parsed->uniforms[i]; if (u.constant) continue;
        s->uniforms.push_back(u); unsigned n=std::max(u.array_count,1);
        if(u.type==MOJOSHADER_UNIFORM_FLOAT)s->floatCount+=n;
        else if(u.type==MOJOSHADER_UNIFORM_INT)s->intCount+=n;
        else if(u.type==MOJOSHADER_UNIFORM_BOOL)s->boolCount+=n;
        else fatal("uniform",@"unknown original uniform type");
    }
    unsigned char hash[CC_SHA256_DIGEST_LENGTH]; CC_SHA256(s->msl.data(),static_cast<CC_LONG>(s->msl.size()),hash);
    for (auto b:hash) { char hex[3]; snprintf(hex,sizeof(hex),"%02x",b); s->key+=hex; }
    const char *cache=getenv("SOURCE_METAL_CACHE");
    NSString *folder=cache ? @(cache) : @"metal-shader-cache";
    [[NSFileManager defaultManager] createDirectoryAtPath:folder withIntermediateDirectories:YES attributes:nil error:nil];
    NSString *path=[folder stringByAppendingPathComponent:@(s->key.c_str())];
    [[NSData dataWithBytes:code length:size] writeToFile:[path stringByAppendingString:@".dx9"] atomically:YES];
    [@(s->msl.c_str()) writeToFile:[path stringByAppendingString:@".metal"] atomically:YES encoding:NSUTF8StringEncoding error:nil];
    NSError *error=nil;
    NSString *libPath=[path stringByAppendingString:@".metallib"];
    static std::map<std::string,id<MTLLibrary>> libraries;
    auto existing=libraries.find(s->key);
    const bool diskCached=[[NSFileManager defaultManager] fileExistsAtPath:libPath];
    if(existing!=libraries.end())s->library=existing->second;
    else if(diskCached) s->library=[device() newLibraryWithURL:[NSURL fileURLWithPath:libPath] error:&error];
    else {
        MTLCompileOptions *options=[MTLCompileOptions new]; options.fastMathEnabled=NO; options.languageVersion=MTLLanguageVersion3_2;
        s->library=[device() newLibraryWithSource:@(s->msl.c_str()) options:options error:&error];
    }
    if (!s->library) fatal("shader compile",[NSString stringWithFormat:@"%s: %@ (%@)",s->label.c_str(),error.localizedDescription,path]);
    s->function=[s->library newFunctionWithName:@"source_main"];
    if(!s->function) fatal("shader entry point",@(s->label.c_str()));
    s->function.label=@(s->label.c_str());
    libraries[s->key]=s->library;
    // Only successfully compiled/adapted shaders enter the offline manifest.
    NSDictionary *metadata=@{@"key":@(s->key.c_str()),@"source_shader":@(s->name.c_str()),
        @"label":@(s->label.c_str()),@"vertex":@(s->parsed->shader_type==MOJOSHADER_TYPE_VERTEX),
        @"bytecode_bytes":@(size),@"centroid_mask":@(centroid ? *centroid:0),
        @"loaded_offline_metallib":@(diskCached),@"reused_library":@(existing!=libraries.end())};
    NSData *metadataBytes=[NSJSONSerialization dataWithJSONObject:metadata options:NSJSONWritingPrettyPrinted error:&error];
    [metadataBytes writeToFile:[path stringByAppendingString:@".shader.json"] atomically:YES];

    return s;
    } // autoreleasepool
}
void packUniforms(const Shader &shader,const State &s,std::vector<uint8_t> &data) {
    unsigned bytes=16*(shader.floatCount+shader.intCount)+shader.boolCount;
    data.resize((bytes+15)&~15u);
    std::fill(data.begin(),data.end(),0);
    unsigned f=0,i=16*shader.floatCount,b=i+16*shader.intCount;
    bool vertex=shader.parsed->shader_type==MOJOSHADER_TYPE_VERTEX;
    for (const auto &u:shader.uniforms) {
        unsigned count=std::max(1,u.array_count);
        if (u.type==MOJOSHADER_UNIFORM_FLOAT) {
            unsigned limit=vertex ? DXABSTRACT_VS_PARAM_SLOTS : kGLMProgramParamFloat4Limit;
            if (u.index<0 || u.index+count>limit) fatal("float uniform",@"original register out of range");
            memcpy(data.data()+f,vertex ? s.vf[u.index]:s.pf[u.index],count*16); f+=count*16;
        } else if (u.type==MOJOSHADER_UNIFORM_INT) {
            if (u.index<0 || u.index+count>16) fatal("int uniform",@"register out of range");
            memcpy(data.data()+i,vertex ? s.vi[u.index]:s.pi[u.index],count*16); i+=count*16;
        } else {
            if (u.index<0 || u.index+count>16) fatal("bool uniform",@"register out of range");
            for(unsigned n=0;n<count;++n) data[b++]=(vertex ? s.vb[u.index+n]:s.pb[u.index+n])!=0;
        }
    }
}
} // namespace SourceMetal
