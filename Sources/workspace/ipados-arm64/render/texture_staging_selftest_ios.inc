CON_COMMAND(icsm_texture_staging_selftest,"Verify bounded texture scratch with 2D, cube, volume and BC mip data") {
    if(!g_sourceNativeDevice)return;
    auto& d=*g_sourceNativeDevice;if(d.profileRemaining)return;commit(d,true);
    ConVarRef mode("icsm_metal_texture_staging_pool");const int saved=mode.GetInt();
    NSMutableArray *cases=[NSMutableArray array];bool all=true;
    for(int enabled=0;enabled<2;++enabled) {
        mode.SetValue(enabled);
        for(unsigned kind=0;kind<4;++kind) {
            const unsigned width=kind==1?64:kind==2?32:17,height=kind==1?32:kind==2?32:11;
            const unsigned depth=kind==3?5:1,slices=kind==2?6:1,levels=3;
            auto texture=newTexture(width,height,depth,levels,slices,1,0,
                kind==1?D3DFMT_DXT5:D3DFMT_A8R8G8B8,"iCSM staging regression");
            bool passed=texture!=nullptr;Image image;image.image=texture;
            for(unsigned face=0;passed && face<slices;++face)for(unsigned mip=0;passed && mip<levels;++mip) {
                D3DLOCKED_BOX locked{};passed=lockImage(image,mip,face,nullptr,D3DLOCK_DISCARD,locked)==S_OK;
                if(!passed)break;
                const unsigned rows=(std::max(1u,height>>mip)+texture->format.block-1)/texture->format.block;
                const size_t size=size_t(locked.SlicePitch)*std::max(1u,depth>>mip);
                std::vector<uint8_t> expected(size);
                for(size_t i=0;i<size;++i)expected[i]=(i*13+face*17+mip*29+kind*31)%251;
                memcpy(locked.pBits,expected.data(),size);
                passed=unlockImage(image,mip,face)==S_OK;
                passed=passed && lockImage(image,mip,face,nullptr,D3DLOCK_READONLY,locked)==S_OK;
                if(passed) {passed=memcmp(locked.pBits,expected.data(),size)==0 && rows>0;unlockImage(image,mip,face);}
                if(enabled)passed=passed && texture->locks[face*levels+mip].data.capacity()==0;
            }
            all=all && passed;[cases addObject:@{@"enabled":@(enabled),@"kind":@(kind),@"passed":@(passed)}];
        }
    }
    mode.SetValue(saved);
    NSString *path=[NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,NSUserDomainMask,YES).firstObject stringByAppendingPathComponent:@"I5/texture-staging-selftest.json"];
    [[NSJSONSerialization dataWithJSONObject:@{@"passed":@(all),@"cases":cases} options:NSJSONWritingPrettyPrinted error:nil] writeToFile:path atomically:YES];
    Msg("ICSM_TEXTURE_STAGING_SELFTEST passed=%d\n",all);
}
