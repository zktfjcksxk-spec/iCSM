// Original CmdKeyValues is reliable but rate-limited to 512 bytes per tick.
// Send at most three rows, wait for an acknowledgement, and retry dropped
// batches. The receiver commits only a completely validated snapshot.
#include "igamesystem.h"
#include "inetchannelinfo.h"
#include "tier1/utlbuffer.h"
static ConVar icsm_inventory_protocol("icsm_inventory_protocol", "0",
    FCVAR_REPLICATED | FCVAR_RELEASE, "Private inventory synchronization version");
static ConVar icsm_inventory_client("icsm_inventory_client", "1",
    FCVAR_USERINFO | FCVAR_RELEASE, "Private inventory client version");

class CICSMNetworkInventory : public CAutoGameSystemPerFrame
{
    bool dirty;
    double nextSend;
    KeyValues *pending;
    int generation, batch, count, rowsPerBatch, version;
    void Clear()
    {
        if (pending) pending->deleteThis();
        pending = NULL; batch = count = 0; nextSend = 0;
    }
    void Snapshot(CCSPlayerInventory *inventory)
    {
        pending = new KeyValues("entries");
        generation = generation >= 2147483646 ? 1 : generation + 1;
        count = batch = 0;
        version=icsm_inventory_protocol.GetInt()>=2?2:1;rowsPerBatch=version==2?1:3;
        const auto &definitions = GetItemSchema()->GetItemDefinitionMap();
        for (int team = TEAM_TERRORIST; team <= TEAM_CT; ++team)
            FOR_EACH_MAP_FAST(definitions, i)
            {
                const GameItemDefinition_t *definition = dynamic_cast<const GameItemDefinition_t *>(definitions[i]);
                if (!definition) continue;
                const int index = definition->GetDefinitionIndex(), slot = definition->GetLoadoutSlot(team);
                if (!ICSMLoadout::Definition(team, slot, index)) continue;
                const ICSMSkin::Selection skin = ICSMSkin::Read(team, index);
                const CEconItemView *equipped = inventory->GetItemInLoadout(team, slot);
                char key[24]; V_snprintf(key, sizeof(key), "%d", count++);
                KeyValues *row = pending->FindKey(key, true);
                row->SetInt("team", team); row->SetInt("slot", slot); row->SetInt("definition", index);
                row->SetInt("selected", equipped && equipped->IsValid() && equipped->GetItemDefinition()->GetDefinitionIndex() == index);
                row->SetInt("paint", skin.paint); row->SetInt("seed", skin.seed); row->SetFloat("wear", skin.wear);
                if(version==2) {
                    const auto stickers=ICSMStickers::Read(index,skin.paint);
                    for(int n=0;n<ICSMStickers::Slots(index);++n) {
                        char field[8];V_snprintf(field,sizeof(field),"s%d",n);row->SetInt(field,stickers.ids[n]);
                    }
                }
            }
        dirty = false;
    }
public:
    CICSMNetworkInventory() : CAutoGameSystemPerFrame("iCSMNetworkInventory"), dirty(true),
        nextSend(0), pending(NULL), generation(0), batch(0), count(0), rowsPerBatch(3), version(1) {}
    ~CICSMNetworkInventory() { Clear(); }
    void Changed() { dirty = true; }
    void LevelInitPostEntity() OVERRIDE { Clear(); dirty = true; }
    void LevelShutdownPreEntity() OVERRIDE { Clear(); dirty = true; }
    void Acknowledge(int receivedVersion, int receivedGeneration, int receivedBatch)
    {
        if (receivedVersion != version || !pending || receivedGeneration != generation || receivedBatch != batch) return;
        ++batch;
        if (batch * rowsPerBatch >= count)
        {
            Msg("ICSM_NET_INVENTORY confirmed generation=%d rows=%d\n", generation, count);
            Clear();
        }
        nextSend = Plat_FloatTime() + 0.1;
    }
    void Update(float) OVERRIDE
    {
        if ((!dirty && !pending) || !engine->IsInGame() || !gpGlobals ||
            Plat_FloatTime() < nextSend || icsm_inventory_protocol.GetInt() < 1) return;
        INetChannelInfo *channel = engine->GetNetChannelInfo();
        if (!channel || channel->IsLoopback()) return;
        CCSPlayerInventory *inventory = CSInventoryManager()->GetLocalCSInventory();
        if (!inventory) return;
        if (!pending) Snapshot(inventory);
        KeyValues *message = new KeyValues("iCSMPrivateInventory");
        message->SetInt("version", version); message->SetInt("generation", generation);
        message->SetInt("batch", batch); message->SetInt("count", count);
        KeyValues *entries = message->FindKey("entries", true);
        for (int i = batch * rowsPerBatch; i < MIN(count, batch * rowsPerBatch + rowsPerBatch); ++i)
        {
            char key[24]; V_snprintf(key, sizeof(key), "%d", i);
            entries->AddSubKey(pending->FindKey(key)->MakeCopy());
        }
        CUtlBuffer serialized;
        message->WriteAsBinary(serialized);
        if (serialized.TellPut() > 480)
        {
            Warning("ICSM_NET_INVENTORY unexpected batch size=%d\n", serialized.TellPut());
            message->deleteThis(); Clear(); dirty = false; return;
        }
        // IVEngineClient takes ownership and serializes the original message.
        engine->ServerCmdKeyValues(message);
        nextSend = Plat_FloatTime() + 0.5;
    }
};
static CICSMNetworkInventory icsmNetworkInventory;

CON_COMMAND_F(icsm_inventory_ack, "Acknowledge a private inventory batch", FCVAR_RELEASE | FCVAR_SERVER_CAN_EXECUTE)
{
    if (args.ArgC() == 4 && engine->IsInGame() && icsm_inventory_protocol.GetInt() >= 1)
        icsmNetworkInventory.Acknowledge(atoi(args[1]), atoi(args[2]), atoi(args[3]));
}
