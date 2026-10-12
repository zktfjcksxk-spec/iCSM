"""Exercise actual glove identity/binding helpers without operating the game."""
import subprocess
import tempfile
from pathlib import Path

P = Path(__file__).resolve().parents[3]
source = (P / 'inventory/offline_glove_viewmodel.inc').read_text()
body = source[source.index('inline bool Ready('):source.index('\n}\nnamespace ICSMGloves')]
fixture = r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include "offline_glove_catalog.h"
#define Msg(...) ((void)0)
struct ICustomMaterial {
    bool valid=false;
    bool IsValid()const{return valid;}
};
struct Definition {
    int index=5030;std::string model;
    int GetDefinitionIndex()const{return index;}
    const char *GetBasePlayerDisplayModel()const{return model.c_str();}
};
struct Schema {
    std::map<int,Definition> definitions;
    const Definition *GetItemDefinition(int index)const {
        auto i=definitions.find(index);return i==definitions.end()?nullptr:&i->second;
    }
} schema;
Schema *GetItemSchema(){return &schema;}
struct Owner {
    std::vector<ICustomMaterial*> materials;
    int GetCustomMaterialCount()const{return int(materials.size());}
    ICustomMaterial *GetCustomMaterial(int i)const {
        return i>=0 && i<int(materials.size())?materials[i]:nullptr;
    }
};
struct CStudioHdr {
    // Exact original MDL header name, without the filesystem "models/" prefix.
    const char *pszName()const{return "weapons/v_models/arms/glove_sporty/v_glove_sporty.mdl";}
};
struct C_ViewmodelAttachmentModel:Owner {
    int index=17;bool loaded=true;int swaps=0;CStudioHdr header;
    const void *GetModel()const{return loaded?this:nullptr;}
    CStudioHdr *GetModelPtr(){return &header;}
    int GetModelIndex()const{return index;}
};
struct CEconItemView:Owner {
    Definition definition;
    const Definition *GetItemDefinition()const{return &definition;}
    int GetCustomPaintKitIndex()const{return 10018;}
    void DuplicateCustomMaterialsToOther(C_ViewmodelAttachmentModel *other)const {
        other->materials=materials;++other->swaps;
    }
};
struct ModelInfo {
    std::map<std::string,int> indices;
    int GetModelIndex(const char *p)const {
        auto found=indices.find(p);return found==indices.end()?-1:found->second;
    }
} info;
ModelInfo *modelinfo=&info;
'''
cases = r'''
int main() {
    const char *sporty="models/weapons/v_models/arms/glove_sporty/v_glove_sporty.mdl";
    const char *defaults="models/weapons/v_models/arms/glove_fingerless/v_glove_fingerless.mdl";
    info.indices[sporty]=17;info.indices[defaults]=18;
    for(int index:ICSMGloves::Definitions)if(ICSMGloves::Custom(index)) {
        std::string model=index==5030?sporty:"models/glove-"+std::to_string(index)+".mdl";
        schema.definitions[index]=Definition{index,model};info.indices[model]=index==5030?17:index;
    }
    C_ViewmodelAttachmentModel entity;
    assert(strcmp(entity.GetModelPtr()->pszName(),sporty)!=0); // legacy rejection
    assert(SameModel(&entity,sporty));
    assert(CustomModel(&entity));
    for(const auto &entry:schema.definitions) {
        entity.index=info.indices[entry.second.model];assert(CustomModel(&entity));
    }
    entity.index=18;assert(!CustomModel(&entity));entity.index=17;
    assert(!SameModel(&entity,defaults));
    assert(!SameModel(nullptr,sporty));
    assert(!SameModel(&entity,nullptr));assert(!SameModel(&entity,""));
    assert(!SameModel(&entity,"models/missing.mdl"));
    entity.index=-1;assert(!SameModel(&entity,"models/missing.mdl"));entity.index=17;
    entity.loaded=false;assert(!SameModel(&entity,sporty));entity.loaded=true;
    ICustomMaterial left,right;
    CEconItemView item;item.materials={nullptr,&left,&right};
    assert(!Ready(nullptr));CEconItemView empty;assert(!Ready(&empty));
    empty.materials={nullptr,nullptr};assert(!Ready(&empty));
    assert(!Ready(&item));
    Apply(&item,&entity);assert(entity.swaps==0); // original async generation
    left.valid=true;assert(!Ready(&item));Apply(&item,&entity);assert(entity.swaps==0);
    right.valid=true;Apply(&item,&entity);
    assert(Ready(&item));
    assert(entity.swaps==1 && entity.materials==item.materials);
    for(int frame=0;frame<300;++frame) {
        assert(SameModel(&entity,sporty));Apply(&item,&entity);
    }
    assert(entity.swaps==1); // no repeated clear/copy after readiness
    entity.materials.clear();Apply(&item,&entity);assert(entity.swaps==2);
    ICustomMaterial replacement;replacement.valid=true;
    item.materials[2]=&replacement;Apply(&item,&entity);
    assert(entity.swaps==3 && entity.materials[2]==&replacement);
    replacement.valid=false;assert(!Ready(&item));
    Apply(&item,&entity);assert(entity.swaps==3);replacement.valid=true;
    Apply(nullptr,&entity);Apply(&item,nullptr);
    entity.index=18;assert(SameModel(&entity,defaults));assert(!SameModel(&entity,sporty));
    puts("PASS: all seven custom glove models recognized for replacement/default recovery, actual model identity, original header-prefix mismatch, async left/right readiness, sparse slots, one-time binding");
}
'''
with tempfile.TemporaryDirectory() as tmp:
    cpp = Path(tmp) / 'test.cpp'; cpp.write_text(fixture + body + cases)
    binary = Path(tmp) / 'test'
    subprocess.run(['xcrun', 'clang++', '-std=c++17', '-fsanitize=address,undefined', '-I'+str(P/'inventory'),
                    str(cpp), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
