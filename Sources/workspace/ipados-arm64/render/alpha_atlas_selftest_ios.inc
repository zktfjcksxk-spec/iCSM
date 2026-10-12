// Explicit device regression: exact A8/BGRA bytes, queued ordering, original D3D
// texture/surface entry points, invalidation, and bounded weak-owned mirrors.
CON_COMMAND(icsm_alpha_atlas_selftest,"Verify ordered Panorama/VGUI glyph atlas uploads on the active GPU") {
    if(!g_sourceNativeDevice || !g_sourceCombatD3DDevice)return;
    auto &d=*g_sourceNativeDevice;
    if(d.profileRemaining){Warning("ICSM_ALPHA_ATLAS_SELFTEST refused during sampling\n");return;}
    ConVarRef enabled("icsm_metal_alpha_atlas_upload");
    const bool old=enabled.GetBool();enabled.SetValue(1);
    IDirect3DDevice9 *dev=g_sourceCombatD3DDevice;
    NSMutableArray *cases=[NSMutableArray array];bool all=true;
    for(unsigned formatCase=0;formatCase<2;++formatCase) for(int mode=0;mode<2;++mode) {
        const D3DFORMAT format=formatCase?D3DFMT_A8R8G8B8:D3DFMT_A8;
        const unsigned bytes=formatCase?4:1;
        const char *label=formatCase?"vgui__font_page_900001":"panorama_alpha_texture_900001";
        enabled.SetValue(mode);
        IDirect3DTexture9 *texture=nullptr;
        const unsigned width=73,height=39;
        bool passed=dev->CreateTexture(width,height,1,0,format,D3DPOOL_DEFAULT,&texture,nullptr,
            const_cast<char*>(label))==S_OK;
        std::vector<uint8_t> expected(width*height*bytes,19);unsigned shadows=0;
        auto write=[&](const RECT *rect,unsigned seed) {
            IDirect3DSurface9 *surface=nullptr;D3DLOCKED_RECT locked{};
            if(!passed || texture->GetSurfaceLevel(0,&surface)!=S_OK){passed=false;return;}
            passed=surface->LockRect(&locked,rect,D3DLOCK_NOSYSLOCK)==S_OK;
            if(passed) {
                if(native<Image>(surface).image->locks[0].shadow)++shadows;
                const unsigned x=rect?rect->left:0,y=rect?rect->top:0;
                const unsigned w=rect?rect->right-x:width,h=rect?rect->bottom-y:height;
                for(unsigned row=0;row<h;++row)for(unsigned col=0;col<w;++col)for(unsigned channel=0;channel<bytes;++channel) {
                    const uint8_t value=(col*17+row*23+seed+channel*47)%251;
                    static_cast<uint8_t*>(locked.pBits)[row*locked.Pitch+col*bytes+channel]=value;
                    expected[((y+row)*width+x+col)*bytes+channel]=value;
                }
                passed=surface->UnlockRect()==S_OK;
            }
            surface->Release();
        };
        write(nullptr,19);
        for(unsigned n=0;passed && n<80;++n) {
            RECT rect{int(n%47),int(n%27),int(n%47+17),int(n%27+9)};
            write(&rect,n+31);if(n%16==15)commit(d,false);
        }
        auto matches=[&] {
            commit(d,true);std::vector<uint8_t> actual(expected.size());
            [native<Image>(texture).image->texture getBytes:actual.data() bytesPerRow:width*bytes
                fromRegion:MTLRegionMake2D(0,0,width,height) mipmapLevel:0];
            return actual==expected;
        };
        if(passed)passed=matches() && shadows==(mode?81u:0u);
        bool readonly=false,invalidated=false;
        if(passed) {
            D3DLOCKED_RECT locked{};passed=texture->LockRect(0,&locked,nullptr,D3DLOCK_READONLY)==S_OK;
            if(passed) {
                readonly=true;
                for(unsigned y=0;y<height;++y)
                    if(memcmp(static_cast<uint8_t*>(locked.pBits)+y*locked.Pitch,expected.data()+y*width*bytes,width*bytes))readonly=false;
                passed=texture->UnlockRect(0)==S_OK && readonly;
            }
        }
        if(passed) {
            IDirect3DSurface9 *source=nullptr,*target=nullptr;
            passed=dev->CreateOffscreenPlainSurface(width,height,format,D3DPOOL_DEFAULT,&source,nullptr)==S_OK &&
                texture->GetSurfaceLevel(0,&target)==S_OK;
            if(passed) {
                D3DLOCKED_RECT locked{};passed=source->LockRect(&locked,nullptr,0)==S_OK;
                if(passed) {
                    for(unsigned y=0;y<height;++y)memset(static_cast<uint8_t*>(locked.pBits)+y*locked.Pitch,37,width*bytes);
                    passed=source->UnlockRect()==S_OK && dev->StretchRect(source,nullptr,target,nullptr,D3DTEXF_POINT)==S_OK;
                    invalidated=!native<Image>(texture).image->cpuShadowValid;
                    expected.assign(expected.size(),37);
                    RECT corner{int(width-7),int(height-3),int(width),int(height)};
                    write(&corner,77);
                    passed=passed && invalidated && matches();
                }
            }
            if(target)target->Release();if(source)source->Release();
        }
        if(texture)texture->Release();all=all && passed;
        [cases addObject:@{@"format":@(format),@"bytes_per_pixel":@(bytes),@"enabled":@(mode),@"passed":@(passed),@"shadow_locks":@(shadows),
            @"queued_partial_writes":@80,@"read_only":@(readonly),@"gpu_copy_invalidation":@(invalidated)}];
    }
    enabled.SetValue(1);
    std::vector<IDirect3DTexture9*> atlases;bool bounded=true;
    for(unsigned n=0;n<17 && bounded;++n) {
        IDirect3DTexture9 *t=nullptr;char label[64];snprintf(label,sizeof(label),"panorama_alpha_texture_%u",910000+n);
        bounded=dev->CreateTexture(1024,1024,1,0,D3DFMT_A8,D3DPOOL_DEFAULT,&t,nullptr,label)==S_OK;
        if(!bounded)break;atlases.push_back(t);D3DLOCKED_RECT locked{};
        bounded=t->LockRect(0,&locked,nullptr,0)==S_OK;
        if(bounded) {
            for(unsigned row=0;row<1024;++row)memset(static_cast<uint8_t*>(locked.pBits)+row*locked.Pitch,29+n,1024);
            bounded=t->UnlockRect(0)==S_OK;
        }
    }
    bool rehydrated=false;
    if(bounded && atlases.size()==17) {
        bounded=!native<Image>(atlases.front()).image->cpuShadowValid;
        D3DLOCKED_RECT locked{};RECT pixel{1019,1021,1020,1022};
        bounded=bounded && atlases.front()->LockRect(0,&locked,&pixel,0)==S_OK;
        if(bounded) {
            *static_cast<uint8_t*>(locked.pBits)=217;
            bounded=atlases.front()->UnlockRect(0)==S_OK;
            commit(d,true);std::vector<uint8_t> actual(1024*1024);
            [native<Image>(atlases.front()).image->texture getBytes:actual.data() bytesPerRow:1024
                fromRegion:MTLRegionMake2D(0,0,1024,1024) mipmapLevel:0];
            rehydrated=true;
            for(size_t i=0;i<actual.size();++i)if(actual[i]!=(i==1021*1024+1019?217:29)){rehydrated=false;break;}
            auto snapshot=SourceMetalCombatSnapshot();
            bounded=bounded && rehydrated && [snapshot[@"alpha_atlas_mirror_bytes"] unsignedLongLongValue]<=
                [snapshot[@"alpha_atlas_budget_bytes"] unsignedLongLongValue];
        }
    }
    for(auto t:atlases)t->Release();
    all=all && bounded;enabled.SetValue(old);
    NSDictionary *result=@{@"passed":@(all),@"cases":cases,@"bounded_32MiB":@(bounded),
        @"eviction_preserves_untouched_glyphs":@(rehydrated),@"snapshot":SourceMetalCombatSnapshot()};
    NSData *json=[NSJSONSerialization dataWithJSONObject:result options:NSJSONWritingPrettyPrinted error:nil];
    [json writeToFile:[NSHomeDirectory() stringByAppendingPathComponent:@"Documents/I5/alpha-atlas-selftest.json"] atomically:YES];
    Msg("ICSM_ALPHA_ATLAS_SELFTEST %s\n",all?"PASS":"FAIL");
}
