#include "gcsdk/gcclientsdk.h"
#include "base_gcmessages.pb.h"
#include "kv3lib/keyvalues3.h"
#include "tier0/threadtools.h"
#include "probe_checks.h"
#include <fstream>
#include <string>

using Party = GCSDK::CProtoBufSharedObject<CSOPartyInvite, 1001>;
class Cache : public GCSDK::CSharedObjectCache
{
public:
    int dirtied = 0;
    void MarkDirty() override { ++dirtied; }
private:
    GCSDK::CSharedObjectTypeCache *AllocateTypeCache(int id) const override
    { return new GCSDK::CSharedObjectTypeCache(id); }
    GCSDK::SOID_t GetOwner() const override { return GCSDK::SOID_t(2, 123456789); }
};
static void ShowDifference(const KeyValues3 *a, const KeyValues3 *b, const std::string &path)
{
    if (a->IsIdenticalTo(b, false)) return;
    if (a->GetType() == KEYVALUES3_TYPE_TABLE && b->GetType() == KEYVALUES3_TYPE_TABLE)
    {
        std::printf("TABLE DIFFERENCE %s count=%d/%d flags=%u/%u\n",path.c_str(),a->GetMemberCount(),b->GetMemberCount(),a->GetAllFlags(),b->GetAllFlags());
        for (int i=0; i<a->GetMemberCount(); ++i)
        {
            auto other=b->FindMember(CKV3MemberName(a->GetMemberName(i)));
            if (other) ShowDifference(a->GetMember(i),other,path+"."+a->GetMemberName(i));
            else std::printf("MISSING %s.%s\n",path.c_str(),a->GetMemberName(i));
        }
    }
    else if(a->GetType()==KEYVALUES3_TYPE_ARRAY && b->GetType()==KEYVALUES3_TYPE_ARRAY)
    {
        std::printf("ARRAY DIFFERENCE %s count=%d/%d flags=%u/%u\n",path.c_str(),a->GetArrayElementCount(),b->GetArrayElementCount(),a->GetAllFlags(),b->GetAllFlags());
        for(int i=0; i<a->GetArrayElementCount() && i<b->GetArrayElementCount(); ++i)
            ShowDifference(a->GetArrayElement(i),b->GetArrayElement(i),path+"["+std::to_string(i)+"]");
    }
    else
    {
        CUtlString x,y; a->GetValueAsString(&x); b->GetValueAsString(&y);
        std::printf("DIFFERENCE %s type=%d/%d flags=%u/%u value=%s/%s\n",path.c_str(),a->GetType(),b->GetType(),a->GetAllFlags(),b->GetAllFlags(),x.Get(),y.Get());
    }
}

int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    DeclareCurrentThreadIsMainThread();
    std::ifstream input(argv[1], std::ios::binary);
    std::string data((std::istreambuf_iterator<char>(input)), {});
    Check(!data.empty(), "read original survival KV3 resource");
    CKeyValues3Context context;
    context.SetMetadataEnabled(true);
    CUtlString error;
    Check(LoadKV3Text(context.Root(), &error, data.c_str(), KV3_FORMAT_GENERIC, argv[1]), "original KV3 text parser");
    const KeyValues3 *items = context->FindMember("items");
    Check(items && items->GetArrayElementCount() > 40, "actual configuration item array");
    if (!items) return 1;
    Check(V_strcmp(items->GetArrayElement(0)->GetMemberString("name"), "cash") == 0,
          "first item preserves original order and name");
    bool found = false;
    for (int i = 0; i < items->GetArrayElementCount(); ++i)
    {
        const KeyValues3 *item = items->GetArrayElement(i);
        if (V_strcmp(item->GetMemberString("name"), "weapon_ak47") == 0)
        {
            found = item->GetMemberInt("default_ammo") == 30 &&
                    item->GetMemberInt("security_door_value") == 25;
        }
    }
    Check(found, "known original AK-47 ammunition and value");
    Check(items->GetArrayElement(0)->Metadata_GetLineNumber() > 0, "context retains parser line metadata");
    CUtlBuffer binary;
    Check(SaveKV3(KV3_ENCODING_BINARY_BLOCK_COMPRESSED, KV3_FORMAT_GENERIC, context.Root(), &error, &binary),
          "actual compressed binary serializer");
    CKeyValues3Context restored;
    Check(LoadKV3(&restored, &error, &binary), "actual compressed binary reader");
    Check(context.IsIdenticalTo(&restored, false), "resource survives binary roundtrip with all fields");
    ShowDifference(context.Root(),restored.Root(),"root");
    KeyValues3 invalid;
    Check(!LoadKV3Text_NoHeader(&invalid, &error, "{ broken = [ true, ") && !error.IsEmpty(),
          "malformed input reports failure");

    GCSDK::CSharedObject::RegisterFactory(1001, GCSDK::CreateSharedObjectSubclass<Party>, 7,
        "Party", "BuildCacheSubscribed(Party)", "Create(Party)", "Update(Party)");
    Party *party = static_cast<Party *>(GCSDK::CSharedObject::Create(1001));
    Check(party && GCSDK::CSharedObject::GetTypeFlags(1001) == 7 &&
          V_strcmp(GCSDK::CSharedObject::PchClassCreateNodeName(1001), "Create(Party)") == 0,
          "2019 sorted factory registry and supplied metadata");
    if (!party) return 1;
    party->Obj().set_group_id(0x123456789abcdefULL);
    party->Obj().set_sender_id(76561198000000000ULL);
    party->Obj().set_sender_name("Native source data");
    std::string message;
    Check(party->BAddToMessage(&message), "shared object serializes all actual protobuf fields");
    Party copy;
    Check(copy.BParseFromMessage(message) && copy.Obj().sender_name() == "Native source data" &&
          copy.BIsKeyEqual(*party), "protobuf parse and reflection key comparison");
    std::string destroy;
    Check(party->BAddDestroyToMessage(&destroy), "serialize destroy with actual key fields");
    CSOPartyInvite key;
    Check(key.ParseFromString(destroy) && key.group_id() == party->Obj().group_id() &&
          !key.has_sender_id() && !key.has_sender_name(), "destroy packet contains key fields only");
    Cache cache;
    Check(cache.AddObject(party), "2019 vector cache owns shared object");
    Check(cache.dirtied == 1, "normal insertion marks the cache dirty");
    Check(cache.RemoveObject(*party) == party, "remove before clean insertion");
    int dirtied = cache.dirtied;
    Check(cache.AddObjectClean(party) && cache.dirtied == dirtied, "clean insertion preserves original no-dirty contract");
    const Cache &constant = cache;
    Check(cache.FindSharedObject(copy) == party && constant.FindSharedObject(copy) == party,
          "mutable and const cache lookup match parsed key");
    Check(cache.RemoveObject(copy) == party && !cache.FindSharedObject(copy), "cache removal transfers ownership");
    delete party;
    std::printf("KV3 items=%d binary_bytes=%d\n", items->GetArrayElementCount(), binary.TellPut());
    CheckNativeImages();
    std::printf("RESULT %s (%d failures)\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
