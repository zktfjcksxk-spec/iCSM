// Executes the supplied Panorama SM3 shader on the native Metal backend and
// checks actual pixels and hardware visibility. No game shader is substituted.
#include "togl/rendermechanism.h"
#include "mathlib/mathlib.h"
#include <cstdio>
#include <vector>
static std::vector<DWORD> readCode(const char *path) {
    FILE *f=fopen(path,"rb"); if(!f)Error("probe bytecode missing: %s\n",path);
    fseek(f,0,SEEK_END); long n=ftell(f); rewind(f); if(n%4)Error("invalid bytecode size\n");
    std::vector<DWORD> result(n/4); if(fread(result.data(),1,n,f)!=(size_t)n)Error("bytecode read failed\n"); fclose(f); return result;
}
int main(int argc,char **argv) {
    if(argc!=3)return 2; MathLib_Init();
    auto vsCode=readCode(argv[1]),psCode=readCode(argv[2]);
    IDirect3DDevice9 *dev=new IDirect3DDevice9;
    IDirect3DDevice9Params params={}; auto &pp=params.m_presentationParameters;
    pp.BackBufferWidth=pp.BackBufferHeight=64; pp.BackBufferFormat=D3DFMT_A8R8G8B8;
    pp.AutoDepthStencilFormat=D3DFMT_D24S8; pp.MultiSampleType=D3DMULTISAMPLE_NONE;
    if(dev->Create(&params)!=S_OK)return 3;
    IDirect3DVertexShader9 *vs; IDirect3DPixelShader9 *ps;
    dev->CreateVertexShader(vsCode.data(),&vs,"panorama_vs30");
    dev->CreatePixelShader(psCode.data(),&ps,"panorama_ps30");
    if(vs->m_maxVertexAttrs!=4)return 4;
    D3DVERTEXELEMENT9 elems[5]={};
    for(unsigned i=0;i<4;++i) { elems[i].Stream=0; elems[i].Offset=i*16; elems[i].Type=D3DDECLTYPE_FLOAT4; elems[i].Usage=vs->m_vtxAttribMap[i]>>4; elems[i].UsageIndex=vs->m_vtxAttribMap[i]&15; }
    elems[4].Stream=0xff; elems[4].Type=D3DDECLTYPE_UNUSED;
    IDirect3DVertexDeclaration9 *decl; dev->CreateVertexDeclaration(elems,&decl);
    struct Vertex { float fields[4][4]; } vertices[3]={};
    float positions[3][4]={{-.75f,-.75f,0,1},{.75f,-.75f,0,1},{0,.75f,0,1}};
    for(unsigned i=0;i<3;++i) { memcpy(vertices[i].fields[0],positions[i],16); vertices[i].fields[1][0]=vertices[i].fields[1][1]=.5f; }
    IDirect3DVertexBuffer9 *vb; dev->CreateVertexBuffer(sizeof(vertices),D3DUSAGE_DYNAMIC,0,D3DPOOL_DEFAULT,&vb,nullptr);
    void *data; vb->Lock(0,sizeof(vertices),&data,D3DLOCK_DISCARD); memcpy(data,vertices,sizeof(vertices)); vb->Unlock();
    IDirect3DTexture9 *texture; dev->CreateTexture(2,2,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&texture,nullptr);
    D3DLOCKED_RECT lock; texture->LockRect(0,&lock,nullptr,D3DLOCK_DISCARD);
    const unsigned color=0x80cc6633;
    for(unsigned y=0;y<2;++y)for(unsigned x=0;x<2;++x)memcpy(static_cast<char*>(lock.pBits)+y*lock.Pitch+x*4,&color,4);
    texture->UnlockRect(0);
    dev->SetVertexShader(vs); dev->SetPixelShader(ps); dev->SetVertexDeclaration(decl);
    dev->SetStreamSource(0,vb,0,sizeof(Vertex)); dev->SetTexture(0,texture); dev->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
    float constants[2][4]={{1,0,0,0},{0,0,0,0}}; dev->SetPixelShaderConstantF(0,constants[0],2);
    IDirect3DSurface9 *target,*readback; dev->GetRenderTarget(0,&target);
    if(target->LockRect(&lock,nullptr,D3DLOCK_READONLY)!=D3DERR_INVALIDCALL)return 12;
    IDirect3DSurface9 *lockable;
    if(dev->CreateRenderTarget(8,8,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,TRUE,&lockable,nullptr)!=S_OK)return 13;
    if(lockable->LockRect(&lock,nullptr,D3DLOCK_DISCARD)!=S_OK)return 14;
    memset(lock.pBits,0,lock.Pitch*8); lockable->UnlockRect(); lockable->Release();
    dev->CreateOffscreenPlainSurface(64,64,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&readback,nullptr);
    IDirect3DQuery9 *query; dev->CreateQuery(D3DQUERYTYPE_OCCLUSION,&query);
    for(unsigned phase=0;phase<2;++phase) {
        dev->SetRenderState(D3DRS_ALPHATESTENABLE,phase);
        dev->SetRenderState(D3DRS_ALPHAFUNC,D3DCMP_GREATER); dev->SetRenderState(D3DRS_ALPHAREF,230);
        dev->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xffff0000,1,0);
        query->Issue(D3DISSUE_BEGIN); dev->DrawPrimitive(D3DPT_TRIANGLELIST,0,1); query->Issue(D3DISSUE_END);
        dev->GetRenderTargetData(target,readback); readback->LockRect(&lock,nullptr,D3DLOCK_READONLY);
        unsigned center,corner; memcpy(&center,static_cast<char*>(lock.pBits)+32*lock.Pitch+32*4,4); memcpy(&corner,lock.pBits,4); readback->UnlockRect();
        unsigned visible=0; if(query->GetData(&visible,sizeof(visible),D3DGETDATA_FLUSH)!=S_OK)return 5;
        unsigned expected=phase ? 0xffff0000:color;
        printf("phase=%u center=%08x expected=%08x corner=%08x visible=%u\n",phase,center,expected,corner,visible);
        if(center!=expected || corner!=0xffff0000 || (!phase && !visible) || (phase && visible))return 6;
    }
    dev->SetRenderState(D3DRS_ALPHATESTENABLE,0);
    // Original macOS CSM cascades 1/2 pass two polygon-offset units.
    // An erroneous normalized-depth conversion clips this draw completely.
    const float sourceShadowBias=2.f; DWORD shadowBiasBits;
    memcpy(&shadowBiasBits,&sourceShadowBias,sizeof(shadowBiasBits));
    dev->SetRenderState(D3DRS_DEPTHBIAS,shadowBiasBits);
    dev->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xffff0000,1,0);
    dev->DrawPrimitive(D3DPT_TRIANGLELIST,0,1); dev->GetRenderTargetData(target,readback);
    readback->LockRect(&lock,nullptr,D3DLOCK_READONLY); unsigned biasedPixel;
    memcpy(&biasedPixel,static_cast<char*>(lock.pBits)+32*lock.Pitch+32*4,4); readback->UnlockRect();
    printf("Source polygon-offset units=2 result=%08x expected=%08x\n",biasedPixel,color);
    if(biasedPixel!=color)return 11;
    dev->SetRenderState(D3DRS_DEPTHBIAS,0);
    dev->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL,0xffff0000,1,7);
    D3DRECT rectangle={32,0,64,32};
    query->Issue(D3DISSUE_BEGIN);
    dev->DrawPrimitive(D3DPT_TRIANGLELIST,0,1);
    dev->Clear(1,&rectangle,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL,0xff00ff00,1,9);
    dev->DrawPrimitive(D3DPT_TRIANGLELIST,0,1); query->Issue(D3DISSUE_END);
    dev->GetRenderTargetData(target,readback); readback->LockRect(&lock,nullptr,D3DLOCK_READONLY);
    unsigned inside,outside,restored;
    memcpy(&inside,static_cast<char*>(lock.pBits)+5*lock.Pitch+60*4,4);
    memcpy(&outside,static_cast<char*>(lock.pBits)+5*lock.Pitch+5*4,4);
    memcpy(&restored,static_cast<char*>(lock.pBits)+32*lock.Pitch+32*4,4);
    readback->UnlockRect(); unsigned visible=0;
    if(query->GetData(&visible,sizeof(visible),D3DGETDATA_FLUSH)!=S_OK)return 7;
    printf("rectangle inside=%08x outside=%08x restored-draw=%08x visibility=%u\n",inside,outside,restored,visible);
    if(inside!=0xff00ff00 || outside!=0xffff0000 || restored!=color || visible!=2304)return 8;
    // Two independent Source queries in one unchanged render target. The
    // first visible draw and second alpha-rejected draw need distinct slots;
    // the unqueried draw afterwards must contribute to neither result.
    IDirect3DQuery9 *rejectedQuery; dev->CreateQuery(D3DQUERYTYPE_OCCLUSION,&rejectedQuery);
    query->Issue(D3DISSUE_BEGIN); dev->DrawPrimitive(D3DPT_TRIANGLELIST,0,1); query->Issue(D3DISSUE_END);
    dev->SetRenderState(D3DRS_ALPHATESTENABLE,1);
    rejectedQuery->Issue(D3DISSUE_BEGIN); dev->DrawPrimitive(D3DPT_TRIANGLELIST,0,1); rejectedQuery->Issue(D3DISSUE_END);
    dev->SetRenderState(D3DRS_ALPHATESTENABLE,0); dev->DrawPrimitive(D3DPT_TRIANGLELIST,0,1);
    if(query->GetData(&visible,sizeof(visible),0)!=S_FALSE)return 15;
    dev->GetRenderTargetData(target,readback); unsigned rejected=1;
    if(query->GetData(&visible,sizeof(visible),0)!=S_OK || rejectedQuery->GetData(&rejected,sizeof(rejected),0)!=S_OK)return 16;
    printf("independent visibility queries visible=%u rejected=%u expected=1152/0\n",visible,rejected);
    if(visible!=1152 || rejected!=0)return 17;
    rejectedQuery->Release();
    // Source CVertexBuilder's OPENGL_SWAP_COLORS ABI writes packed RGBA.
    // Use asymmetric U/R and B channels through the original Panorama shader
    // to detect an accidental second BGRA swizzle (which also breaks bones).
    elems[1].Type=D3DDECLTYPE_D3DCOLOR;
    IDirect3DVertexDeclaration9 *packedDecl; dev->CreateVertexDeclaration(elems,&packedDecl);
    const unsigned packedUV=0xff2020e0;
    for(auto &vertex:vertices)memcpy(vertex.fields[1],&packedUV,4);
    vb->Lock(0,sizeof(vertices),&data,D3DLOCK_DISCARD); memcpy(data,vertices,sizeof(vertices)); vb->Unlock();
    texture->LockRect(0,&lock,nullptr,D3DLOCK_DISCARD);
    for(unsigned y=0;y<2;++y) { const unsigned texels[2]={0xffff0000,0xff0000ff}; memcpy(static_cast<char*>(lock.pBits)+y*lock.Pitch,texels,8); }
    texture->UnlockRect(0); dev->SetVertexDeclaration(packedDecl);
    dev->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff00ff00,1,0);
    dev->DrawPrimitive(D3DPT_TRIANGLELIST,0,1); dev->GetRenderTargetData(target,readback);
    readback->LockRect(&lock,nullptr,D3DLOCK_READONLY); unsigned packedResult;
    memcpy(&packedResult,static_cast<char*>(lock.pBits)+32*lock.Pitch+32*4,4); readback->UnlockRect();
    printf("Source packed RGBA vertex result=%08x expected=ff0000ff\n",packedResult);
    if(packedResult!=0xff0000ff)return 9;
    // Write to a buffer while the first draw is still encoded. The first
    // triangle must retain its old geometry and UVs, and the second draw must
    // see the new contents, without a forced CPU/GPU wait.
    dev->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff00ff00,1,0);
    for(unsigned i=0;i<3;++i)vertices[i].fields[0][0]=positions[i][0]*.5f-.5f;
    vb->Lock(0,sizeof(vertices),&data,D3DLOCK_DISCARD); memcpy(data,vertices,sizeof(vertices)); vb->Unlock();
    dev->DrawPrimitive(D3DPT_TRIANGLELIST,0,1);
    const unsigned leftUV=0xff202020;
    for(unsigned i=0;i<3;++i) { vertices[i].fields[0][0]=positions[i][0]*.5f+.5f; memcpy(vertices[i].fields[1],&leftUV,4); }
    vb->Lock(0,sizeof(vertices),&data,0); memcpy(data,vertices,sizeof(vertices)); vb->Unlock();
    // The original three-tap Panorama shader's c0.x weights its first
    // sample. Update it within the same encoder: the pending first draw
    // keeps weight 1, and this second draw must see weight 0.5.
    constants[0][0]=.5f; dev->SetPixelShaderConstantF(0,constants[0],2);
    dev->DrawPrimitive(D3DPT_TRIANGLELIST,0,1); dev->GetRenderTargetData(target,readback);
    readback->LockRect(&lock,nullptr,D3DLOCK_READONLY); unsigned oldDraw,newDraw;
    memcpy(&oldDraw,static_cast<char*>(lock.pBits)+32*lock.Pitch+16*4,4);
    memcpy(&newDraw,static_cast<char*>(lock.pBits)+32*lock.Pitch+48*4,4); readback->UnlockRect();
    printf("buffer rename / uniform update old-draw=%08x new-draw=%08x\n",oldDraw,newDraw);
    if(oldDraw!=0xff0000ff || newDraw!=0x80800000)return 10;
    // Live resolution changes save the archive on Reset, then again after
    // newly created pipelines. Exercise both transitions and a new-size GPU
    // readback, including runs which load the archive from the previous probe.
    target->Release(); target=nullptr;
    pp.BackBufferWidth=128; pp.BackBufferHeight=96;
    if(dev->Reset(&pp)!=S_OK)return 15;
    dev->GetRenderTarget(0,&target);
    IDirect3DSurface9 *resizedReadback;
    dev->CreateOffscreenPlainSurface(128,96,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&resizedReadback,nullptr);
    dev->SetRenderState(D3DRS_ALPHABLENDENABLE,1);
    dev->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff123456,1,0);
    dev->DrawPrimitive(D3DPT_TRIANGLELIST,0,1);
    dev->GetRenderTargetData(target,resizedReadback);
    resizedReadback->LockRect(&lock,nullptr,D3DLOCK_READONLY); unsigned resizedCorner;
    memcpy(&resizedCorner,lock.pBits,4); resizedReadback->UnlockRect();
    if(resizedCorner!=0xff123456)return 16;
    auto invalid=pp; invalid.BackBufferWidth=invalid.BackBufferHeight=0; invalid.Windowed=FALSE;
    if(dev->Reset(&invalid)!=D3DERR_INVALIDCALL)return 18;
    if(dev->Reset(nullptr)!=D3DERR_INVALIDCALL)return 19;
    // Reject before releasing the working target, rather than letting Metal
    // assert on a zero-size descriptor during Source device-lost recovery.
    dev->GetRenderTargetData(target,resizedReadback);
    resizedReadback->LockRect(&lock,nullptr,D3DLOCK_READONLY);
    memcpy(&resizedCorner,lock.pBits,4); resizedReadback->UnlockRect();
    if(resizedCorner!=0xff123456)return 20;
    puts("PASS invalid fullscreen/null reset preserves the working GPU target");
    resizedReadback->Release(); target->Release(); target=nullptr;
    pp.BackBufferWidth=pp.BackBufferHeight=64;
    if(dev->Reset(&pp)!=S_OK)return 17;
    printf("PASS live video reset 64x64 -> 128x96 -> 64x64, resized GPU pixel=%08x\n",resizedCorner);
    packedDecl->Release();
    query->Release(); readback->Release(); texture->Release(); vb->Release(); decl->Release(); vs->Release(); ps->Release(); dev->Release();
    puts("PASS original Panorama shader pixels, texture sampling, alpha test, depth attachment, visibility query GPU readback and Source packed vertex ordering"); return 0;
}
