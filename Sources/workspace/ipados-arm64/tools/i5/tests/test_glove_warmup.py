"""Check real cache lifecycle and the real default-to-custom selection gate."""
import subprocess
import tempfile
from pathlib import Path

P = Path(__file__).resolve().parents[3]
C = Path.home() / 'Library/Caches/csgo-ipados-arm64/i5'
text = (P / 'inventory/offline_glove_viewmodel.inc').read_text()
cache_body = text[text.index('class Cache :'):text.index('inline bool SameModel(')]
viewmodel = (C / 'source/game/client/c_baseviewmodel.cpp').read_text(errors='surrogateescape')
start = viewmodel.index('        CEconItemView *glove=pPlayer==C_CSPlayer::GetLocalCSPlayer()')
gate = viewmodel[start:viewmodel.index('        const char *sleeve=', start)]
fixture = r'''
#include <cassert>
#include <cstdio>
#include <string>
#include <map>
#include "offline_glove_catalog.h"
#define Msg(...) ((void)0)
constexpr int TEAM_CT=3,TEAM_TERRORIST=2,MDLHANDLE_INVALID=-1;
static const char *sporty="models/weapons/v_models/arms/glove_sporty/v_glove_sporty.mdl";
static const char *defaults="models/weapons/v_models/arms/glove_fingerless/v_glove_fingerless.mdl";
struct CAutoGameSystemPerFrame {
    CAutoGameSystemPerFrame(const char*){}
    virtual ~CAutoGameSystemPerFrame()=default;
    virtual void LevelInitPostEntity(){}virtual void LevelShutdownPreEntity(){}
    virtual void Shutdown(){}virtual void PreRender(){}
};
struct GameItemDefinition_t {std::string model;const char *GetBasePlayerDisplayModel()const{return model.c_str();}};
std::map<int,GameItemDefinition_t> definitions;
struct ICustomMaterial {bool valid=false;bool IsValid()const{return valid;}};
struct CEconItemView {
    static int created,destroyed;
    ICustomMaterial left,right;int definition=0;
    CEconItemView(){++created;}~CEconItemView(){++destroyed;}
    void Init(int index,int,int,int){definition=index;}
    void SetOrAddAttributeValueByName(const char*,float){}
    void UpdateGeneratedMaterial(bool,int,int){}
    int GetCustomMaterialCount()const{return 3;}
    ICustomMaterial *GetCustomMaterial(int i){return i==1?&left:i==2?&right:nullptr;}
    const char *GetPlayerDisplayModel(int)const{return definitions.at(definition).GetBasePlayerDisplayModel();}
};
int CEconItemView::created=0,CEconItemView::destroyed=0;
namespace ICSMSkinQuality {
    int signature=39;
    int Signature(){return signature;}int Texture(){return 3;}int DiffuseSize(){return 10;}
}
struct ModelInfo {
    bool available=true;
    int GetModelIndex(const char *path)const{
        if(available && path)for(const auto &entry:definitions)if(entry.second.model==path)return entry.first;
        return -1;
    }
} info;
ModelInfo *modelinfo=&info;
namespace ICSMGloves {
    struct Selection {int definition=5030,paint=10018,seed=0;float wear=.06f;};
    Selection saved[2];int reads=0,validations=0;
    Selection Read(int team){++reads;return saved[team==TEAM_CT?0:1];}
    bool Valid(int team,const Selection &s){++validations;return (team==TEAM_CT||team==TEAM_TERRORIST) && FindCatalog(s.definition,s.paint);}
    const GameItemDefinition_t *Definition(int,int index){auto i=definitions.find(index);return i==definitions.end()?nullptr:&i->second;}
}
struct Config {const char *szAssociatedGloveModel=defaults;};
struct C_CSPlayer {
    static C_CSPlayer *local;int team=TEAM_CT;
    static C_CSPlayer *GetLocalCSPlayer(){return local;}
    int GetTeamNumber()const{return team;}
};
C_CSPlayer *C_CSPlayer::local=nullptr;
'''
cases = r'''
int main() {
    for(int index:ICSMGloves::Definitions)if(ICSMGloves::Custom(index))
        definitions[index].model=index==5030?sporty:"models/glove-"+std::to_string(index)+".mdl";
    auto &cache=ICSMGlovesView::cache;
    C_CSPlayer player;C_CSPlayer::local=&player;
    cache.PreRender();assert(CEconItemView::created==0 && ICSMGloves::reads==0);
    cache.LevelInitPostEntity();
    assert(CEconItemView::created==2 && ICSMGloves::reads==2); // before team/weapon spawn
    auto *ct=cache.Get(TEAM_CT),*t=cache.Get(TEAM_TERRORIST);
    assert(ct && t && ct!=t);
    for(int frame=0;frame<300;++frame) {
        cache.PreRender();assert(Choose(&player)==std::string(defaults));
    }
    assert(CEconItemView::created==2 && ICSMGloves::reads==2 && ICSMGloves::validations==2); // no per-frame IO/compositing/schema validation
    ct->left.valid=true;assert(Choose(&player)==std::string(defaults));
    ct->right.valid=true;assert(Choose(&player)==std::string(sporty));
    player.team=TEAM_TERRORIST;assert(Choose(&player)==std::string(defaults));
    t->left.valid=t->right.valid=true;assert(Choose(&player)==std::string(sporty));
    cache.dirty=true;cache.PreRender();
    assert(CEconItemView::created==4 && CEconItemView::destroyed==2 && ICSMGloves::reads==4);
    assert(Choose(&player)==std::string(defaults));
    ++ICSMSkinQuality::signature;cache.PreRender();
    assert(CEconItemView::created==6 && CEconItemView::destroyed==4);
    cache.LevelShutdownPreEntity();cache.PreRender();
    assert(CEconItemView::created==6 && CEconItemView::destroyed==6);
    assert(cache.Get(0)==nullptr);
    info.available=false;cache.LevelInitPostEntity();assert(CEconItemView::created==6);
    assert(Choose(&player)==std::string(defaults)); // old remote server/no precache
    info.available=true;assert(cache.Get(TEAM_CT));assert(CEconItemView::created==7);
    assert(cache.Get(TEAM_TERRORIST));assert(CEconItemView::created==8);
    ICSMGloves::saved[0].definition=5029;ICSMGloves::saved[1].definition=5028;
    cache.dirty=true;cache.PreRender();
    assert(CEconItemView::created==8 && CEconItemView::destroyed==8);
    assert(Choose(&player)==std::string(defaults));
    cache.Shutdown();cache.PreRender();assert(CEconItemView::created==8);
    cache.LevelInitPostEntity();
    for(int index:ICSMGloves::Definitions)if(ICSMGloves::Custom(index)) {
        for(auto &s:ICSMGloves::saved){s.definition=index;s.paint=ICSMGloves::DefaultPaint(index);}
        cache.dirty=true;cache.PreRender();
        assert(CEconItemView::created-CEconItemView::destroyed==2);
        assert(Choose(&player)==std::string(defaults));
        auto *item=cache.Get(player.team);item->left.valid=item->right.valid=true;
        assert(Choose(&player)==definitions.at(index).model);
        int created=CEconItemView::created,reads=ICSMGloves::reads;
        for(int frame=0;frame<300;++frame)cache.PreRender();
        assert(created==CEconItemView::created && reads==ICSMGloves::reads);
    }
    cache.Shutdown();assert(CEconItemView::created==CEconItemView::destroyed);
    puts("PASS: all seven glove families, bounded two-recipe cache, original prewarm hooks, no repeated IO/generation, complete default arms until both materials ready, quality/recipe invalidation, missing model fallback, shutdown cleanup");
}
'''
selection = 'const char *Choose(C_CSPlayer *pPlayer) {Config cfg;const auto *config=&cfg;\n' + gate + '\nreturn gloveModel;\n}\n'
with tempfile.TemporaryDirectory() as tmp:
    cpp = Path(tmp) / 'test.cpp'
    cpp.write_text(fixture + '\nnamespace ICSMGlovesView {\n' + cache_body + '\n}\n' + selection + cases)
    binary = Path(tmp) / 'test'
    subprocess.run(['xcrun', 'clang++', '-std=c++17', '-fsanitize=address,undefined', '-I'+str(P/'inventory'),
                    str(cpp), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
