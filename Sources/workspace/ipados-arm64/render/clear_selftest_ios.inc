// Explicit device-only pixel regression; never run during ordinary gameplay.
CON_COMMAND(icsm_clear_selftest,"Compare Metal clear bytes with original sRGB switching") {
    auto dev=g_sourceCombatD3DDevice;if(!dev)return;auto &d=native<Device>(dev);
    if(d.profileRemaining || d.activeQuery){Warning("ICSM_CLEAR_SELFTEST busy\n");return;}
    commit(d,true);
    IDirect3DSurface9 *saved[4]={},*depth=nullptr,*test=nullptr;
    for(unsigned n=0;n<4;++n)dev->GetRenderTarget(n,&saved[n]);dev->GetDepthStencilSurface(&depth);
    auto state=d.state;const int savedReuse=icsm_metal_invariant_clear_reuse.GetInt();
    const int savedDescriptorReuse=icsm_metal_pass_descriptor_reuse.GetInt();
    bool passed=dev->CreateRenderTarget(64,32,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,TRUE,&test,nullptr,
        const_cast<char*>("iCSM clear regression"))==S_OK;
    unsigned cases=0;std::vector<uint8_t> original,optimized;
    if(passed) {
        for(unsigned n=1;n<4;++n)dev->SetRenderTarget(n,nullptr);
        dev->SetRenderTarget(0,test);dev->SetDepthStencilSurface(nullptr);
        dev->SetRenderState(D3DRS_SCISSORTESTENABLE,0);
        const D3DVIEWPORT9 viewport{0,0,64,32,0,1};dev->SetViewport(&viewport);
        for(D3DCOLOR color:{0x00000000u,0xff000000u,0xffffffffu,0x7fff00ffu,0x5500ff00u,0x880000ffu,0x80010203u,0x7f6699aau})
            for(bool partial:{false,true}) {
                for(int reuse=0;reuse<2;++reuse) {
                    icsm_metal_invariant_clear_reuse.SetValue(reuse);
                    icsm_metal_pass_descriptor_reuse.SetValue(reuse);
                    dev->SetRenderState(D3DRS_SRGBWRITEENABLE,1);
                    dev->Clear(0,nullptr,D3DCLEAR_TARGET,0x99664422,1,0);
                    D3DRECT rectangle{4,5,23,19};
                    dev->Clear(partial?1:0,partial?&rectangle:nullptr,D3DCLEAR_TARGET,color,1,0);
                    commit(d,true);auto &pixels=reuse?optimized:original;pixels.resize(64*32*4);
                    [native<Image>(test).image->texture getBytes:pixels.data() bytesPerRow:64*4
                        fromRegion:MTLRegionMake2D(0,0,64,32) mipmapLevel:0];
                }
                bool equal=original==optimized;
                for(unsigned y=0;y<32;++y)for(unsigned x=0;x<64;++x) {
                    D3DCOLOR expected=!partial || (x>=4 && x<23 && y>=5 && y<19)?color:0x99664422;
                    const uint8_t bgra[4]={uint8_t(expected),uint8_t(expected>>8),uint8_t(expected>>16),uint8_t(expected>>24)};
                    if(memcmp(&optimized[(y*64+x)*4],bgra,4))equal=false;
                }
                passed=passed && equal;++cases;
            }
    }
    for(unsigned n=0;n<4;++n){dev->SetRenderTarget(n,saved[n]);if(saved[n])saved[n]->Release();}
    dev->SetDepthStencilSurface(depth);if(depth)depth->Release();
    endEncoder(d);d.state=state;icsm_metal_invariant_clear_reuse.SetValue(savedReuse);
    icsm_metal_pass_descriptor_reuse.SetValue(savedDescriptorReuse);
    if(test)test->Release();
    NSDictionary *result=@{@"passed":@(passed),@"cases":@(cases),@"comparison":@"GPU bytes: original and optimized full/partial clears, exact BGRA including alpha"};
    [[NSJSONSerialization dataWithJSONObject:result options:NSJSONWritingPrettyPrinted error:nil]
        writeToFile:[NSHomeDirectory() stringByAppendingPathComponent:@"Documents/I5/clear-selftest.json"] atomically:YES];
    Msg("ICSM_CLEAR_SELFTEST %s cases=%u\n",passed?"PASS":"FAIL",cases);
}
