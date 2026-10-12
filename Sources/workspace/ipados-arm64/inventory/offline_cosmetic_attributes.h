// One authoritative iteration, including received attributes and local overrides.
// Keep the item/network ABI intact. Compare complete raw values, not a hash.
#pragma once
namespace ICSMCosmeticAttributes {
struct Snapshot {
    uint32 stickers[20];
    float paint=0,seed=0,wear=0;
    Snapshot() {
        V_memset(stickers,0,sizeof(stickers));
        const float one=1;
        for(int slot=0;slot<5;++slot)V_memcpy(&stickers[slot*4+2],&one,4);
    }
    bool SameStickers(const Snapshot &other) const {
        return !V_memcmp(stickers,other.stickers,sizeof(stickers));
    }
};
class Reader : public IEconItemAttributeIterator {
    static CUtlMap<int,int> &Fields() {
        static CUtlMap<int,int> fields(0,0,DefLessFunc(int));
        if(!fields.Count()) {
            const EStickerAttributeType types[]={k_EStickerAttribute_ID,k_EStickerAttribute_Wear,k_EStickerAttribute_Scale,k_EStickerAttribute_Rotation};
            for(int slot=0;slot<5;++slot)for(int type=0;type<4;++type) {
                const auto &attribute=GetStickerAttributeDefHandle(slot,types[type]);
                if(attribute)fields.Insert(attribute->GetDefinitionIndex(),slot*4+type);
            }
            const char *names[]={"set item texture prefab","set item texture seed","set item texture wear"};
            for(int n=0;n<3;++n)if(auto *attribute=GetItemSchema()->GetAttributeDefinitionByName(names[n]))
                fields.Insert(attribute->GetDefinitionIndex(),20+n);
        }
        return fields;
    }
    bool Value(const CEconItemAttributeDefinition *attribute,uint32 bits) {
        auto &fields=Fields();int index=fields.Find(attribute->GetDefinitionIndex());
        if(index==fields.InvalidIndex())return true;
        int field=fields[index];
        if(field<20)result.stickers[field]=bits;
        else {
            float value;V_memcpy(&value,&bits,4);
            if(field==20)result.paint=value;else if(field==21)result.seed=value;else result.wear=value;
        }
        return true;
    }
public:
    Snapshot result;
    bool OnIterateAttributeValue(const CEconItemAttributeDefinition *a,attrib_value_t value) override {return Value(a,value);}
    bool OnIterateAttributeValue(const CEconItemAttributeDefinition *a,float value) override {
        uint32 bits;V_memcpy(&bits,&value,4);return Value(a,bits);
    }
    bool OnIterateAttributeValue(const CEconItemAttributeDefinition *,const CAttribute_String &) override {return true;}
    bool OnIterateAttributeValue(const CEconItemAttributeDefinition *,const Vector &) override {return true;}
};
inline Snapshot Read(const CEconItemView *item) {Reader reader;item->IterateAttributes(&reader);return reader.result;}
}
