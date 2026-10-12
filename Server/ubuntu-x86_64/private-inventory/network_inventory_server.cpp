// Private-server inventory uses the original authenticated sender edict and
// reliable CmdKeyValues transport. No Steam identity or inventory is fabricated.
#include "cbase.h"
#include "cs_player.h"
#include "weapon_csbase.h"
#include "igamesystem.h"
#include "offline_skin.h"
#include "offline_stickers.h"
#include "network_inventory_server.h"

static ConVar icsm_inventory_protocol("icsm_inventory_protocol", "0",
    FCVAR_REPLICATED | FCVAR_RELEASE, "Private inventory synchronization version");

namespace ICSMNetworkInventory
{
struct Record
{
    int team, slot, definition, selected;
    ICSMSkin::Selection skin;
    ICSMStickers::Selection stickers;
};
struct State
{
    CHandle<CCSPlayer> owner;
    CUtlVector<Record> records;
    CUtlVector<Record> pending;
    float nextReceive;
    bool ready;
    int userID, generation, nextBatch, expected, pendingJoin, protocol;
    float joinDeadline;
    bool joinTimedOut;
    State() : nextReceive(0), ready(false), userID(0), generation(0), nextBatch(0),
        expected(0), pendingJoin(-1), protocol(1), joinDeadline(0), joinTimedOut(false) {}
};
static State players[MAX_PLAYERS + 1];

static State *Get(CCSPlayer *player)
{
    if (!player || player->IsBot() || player->entindex() < 1 || player->entindex() > MAX_PLAYERS) return NULL;
    State &state = players[player->entindex()];
    if (state.owner.Get() != player)
    {
        // UserIDs are assigned by this server per connection. Keep committed
        // data over changelevel, where player entities/handles are recreated.
        if (state.userID != player->GetUserID())
        {
            state.records.RemoveAll(); state.ready = false;
        }
        state.userID = player->GetUserID();
        state.owner = player;
        state.pending.RemoveAll(); state.generation = state.nextBatch = state.expected = 0;
        state.pendingJoin = -1; state.joinTimedOut = false; state.joinDeadline = 0;
        state.nextReceive = 0;
    }
    return &state;
}

bool DeferJoin(CCSPlayer *player, int team, bool queue, int coach)
{
    State *state = Get(player);
    if (!state || state->ready || state->joinTimedOut || queue || coach ||
        player->GetTeamNumber() != TEAM_UNASSIGNED || (team != 0 && team != TEAM_TERRORIST && team != TEAM_CT)) return false;
    const char *version = engine->GetClientConVarValue(player->entindex(), "icsm_inventory_client");
    if (!version || V_strcmp(version, "1")) return false;
    state->pendingJoin = team;
    if (!state->joinDeadline) state->joinDeadline = gpGlobals->curtime + 20;
    Msg("ICSM_NET_JOIN waiting player=%d team=%d\n", player->GetUserID(), team);
    return true;
}

void ApplyLoadout(CCSPlayer *player)
{
    State *state = Get(player);
    if (!state || !state->ready) return;
    for (int team = TEAM_TERRORIST; team <= TEAM_CT; ++team)
        for (int slot = LOADOUT_POSITION_MELEE; slot <= LOADOUT_POSITION_LAST_WHEEL_WEAPON; ++slot)
        {
            if (!ICSMLoadout::IsSelectableSlot(slot)) continue;
            int definition = 0;
            FOR_EACH_VEC(state->records, i)
                if (state->records[i].selected && state->records[i].team == team && state->records[i].slot == slot)
                    definition = state->records[i].definition;
            CEconItemView *old = player->Inventory()->FindDefaultEquippedDefinitionItemBySlot(team, slot);
            if ((!old && !definition) || (old && old->IsValid() && old->GetItemDefinition()->GetDefinitionIndex() == definition)) continue;
            player->Inventory()->SetDefaultEquippedDefinitionItemBySlot(team, slot, definition);
        }
}

void ApplyPaint(CCSPlayer *player, CWeaponCSBase *weapon)
{
    State *state = Get(player);
    if (!state || !state->ready || !weapon || !weapon->GetItemDefinition()) return;
    FOR_EACH_VEC(state->records, i)
    {
        const Record &record = state->records[i];
        if (record.team != player->GetTeamNumber() || record.definition != weapon->GetItemDefinitionIndex()) continue;
        CEconItemView *item = weapon->GetAttributeContainer()->GetItem();
        item->SetOrAddAttributeValueByName("set item texture prefab", float(record.skin.paint));
        item->SetOrAddAttributeValueByName("set item texture seed", float(record.skin.seed));
        item->SetOrAddAttributeValueByName("set item texture wear", record.skin.wear);
        ICSMStickers::Apply(item,record.stickers);
        // ItemGeneration initialized the base item before this recipe existed.
        // Refresh through the original public attribute initialization path;
        // DispatchSpawn does not call it again on this server depot.
        weapon->InitializeAttributes();
        Msg("ICSM_NET_PAINT player=%d team=%d definition=%d paint=%d seed=%d wear=%.6f\n",
            player->GetUserID(), record.team, record.definition, record.skin.paint, record.skin.seed, record.skin.wear);
        return;
    }
}

bool Receive(CCSPlayer *player, KeyValues *message)
{
    if (!message || V_strcmp(message->GetName(), "iCSMPrivateInventory")) return false;
    State *state = Get(player);
    if (!state || !gpGlobals) { Warning("ICSM_NET_INVENTORY rejected sender\n"); return true; }
    const int version=message->GetInt("version");
    if(version!=1 && version!=2)return true;
    const int rowsPerBatch=version==2?1:3;
    const int generation = message->GetInt("generation", -1), batch = message->GetInt("batch", -1), count = message->GetInt("count", -1);
    if (generation < 1 || batch < 0 || batch > 255 || count < 1 || count > 256) return true;
    if (generation != state->generation)
    {
        if (batch || gpGlobals->curtime < state->nextReceive) return true;
        state->nextReceive = gpGlobals->curtime + 0.5f;
        state->pending.RemoveAll(); state->generation = generation; state->nextBatch = 0; state->expected = count; state->protocol=version;
    }
    if (version != state->protocol || count != state->expected || batch > state->nextBatch) return true;
    if (batch < state->nextBatch)
    {
        engine->ClientCommand(player->edict(), "icsm_inventory_ack %d %d %d\n", version, generation, batch);
        return true;
    }
    KeyValues *entries = message->FindKey("entries");
    if (!entries) { Warning("ICSM_NET_INVENTORY rejected missing entries\n"); return true; }
    CUtlVector<Record> records;
    FOR_EACH_SUBKEY(entries, row)
    {
        if (records.Count() >= rowsPerBatch) { Warning("ICSM_NET_INVENTORY rejected size\n"); return true; }
        Record record;
        record.team = row->GetInt("team", -1);
        record.slot = row->GetInt("slot", -1);
        record.definition = row->GetInt("definition", -1);
        record.selected = row->GetInt("selected", -1);
        record.skin.paint = row->GetInt("paint", -1);
        record.skin.seed = row->GetInt("seed", -1);
        record.skin.wear = row->GetFloat("wear", -1);
        if ((record.selected != 0 && record.selected != 1) ||
            !ICSMLoadout::Definition(record.team, record.slot, record.definition) ||
            !ICSMSkin::Valid(record.team, record.definition, record.skin))
        {
            Warning("ICSM_NET_INVENTORY rejected player=%d definition=%d paint=%d\n", player->GetUserID(), record.definition, record.skin.paint);
            return true;
        }
        if(version==2)for(int n=0;n<ICSMStickers::MaxSlots;++n) {
            char field[8];V_snprintf(field,sizeof(field),"s%d",n);int id=row->GetInt(field);
            if(id && !ICSMStickers::Valid(record.definition,n,id))return true;
            record.stickers.ids[n]=id;
        }
        for (int source = 0; source < 2; ++source)
        {
            const CUtlVector<Record> &previous = source ? records : state->pending;
            FOR_EACH_VEC(previous, i)
                if (previous[i].team == record.team && (previous[i].definition == record.definition ||
                    (record.selected && previous[i].selected && previous[i].slot == record.slot)))
                { Warning("ICSM_NET_INVENTORY rejected duplicate team=%d slot=%d definition=%d selected=%d\n", record.team, record.slot, record.definition, record.selected); return true; }
        }
        records.AddToTail(record);
    }
    if (state->pending.Count() != batch * rowsPerBatch || records.Count() != MIN(rowsPerBatch, count - batch * rowsPerBatch)) return true;
    FOR_EACH_VEC(records, i) state->pending.AddToTail(records[i]);
    ++state->nextBatch;
    if (state->pending.Count() == count)
    {
        state->records.Swap(state->pending); state->pending.RemoveAll(); state->ready = true;
        ApplyLoadout(player);
        Msg("ICSM_NET_INVENTORY accepted player=%d entity=%d generation=%d rows=%d\n", player->GetUserID(), player->entindex(), generation, count);
        if (state->pendingJoin >= 0)
        {
            const int team = state->pendingJoin; state->pendingJoin = -1;
            player->HandleCommand_JoinTeam(team);
        }
    }
    engine->ClientCommand(player->edict(), "icsm_inventory_ack %d %d %d\n", version, generation, batch);
    return true;
}

class Lifecycle : public CAutoGameSystemPerFrame
{
public:
    Lifecycle() : CAutoGameSystemPerFrame("iCSMPrivateInventory") {}
    void LevelInitPreEntity() OVERRIDE
    {
        // Original Host_BuildConVarUpdateMessage sends only values that differ
        // from their default during signon. Share default 0 with the client,
        // then advertise support before any remote player signs on.
        icsm_inventory_protocol.SetValue(2);
    }
    void LevelShutdownPostEntity() OVERRIDE
    {
        for (int i = 0; i <= MAX_PLAYERS; ++i)
        {
            players[i].owner = NULL;
            players[i].pending.RemoveAll();
            players[i].nextReceive = 0;
            players[i].pendingJoin = -1;
            players[i].joinDeadline = 0;
        }
    }
    void FrameUpdatePostEntityThink() OVERRIDE
    {
        for (int i = 1; i <= gpGlobals->maxClients; ++i)
        {
            CCSPlayer *player = ToCSPlayer(UTIL_PlayerByIndex(i));
            State *state = Get(player);
            if (!state || state->pendingJoin < 0 || gpGlobals->curtime < state->joinDeadline) continue;
            const int team = state->pendingJoin;
            state->pendingJoin = -1; state->joinTimedOut = true;
            Warning("ICSM_NET_JOIN timeout player=%d; retaining normal join rules\n", player->GetUserID());
            player->HandleCommand_JoinTeam(team);
        }
    }
};
static Lifecycle lifecycle;
}

CON_COMMAND_F(icsm_net_inventory, "Inspect per-player private loadouts and replicated weapon paint", FCVAR_RELEASE)
{
    for (int i = 1; gpGlobals && i <= gpGlobals->maxClients; ++i)
    {
        CCSPlayer *player = ToCSPlayer(UTIL_PlayerByIndex(i));
        ICSMNetworkInventory::State *state = ICSMNetworkInventory::Get(player);
        if (!state) continue;
        Msg("ICSM_NET_STATE player=%d name=%s team=%d ready=%d rows=%d\n", player->GetUserID(), player->GetPlayerName(), player->GetTeamNumber(), state->ready, state->records.Count());
        FOR_EACH_VEC(state->records, j)
        {
            const ICSMNetworkInventory::Record &row = state->records[j];
            if (row.selected) Msg("ICSM_NET_LOADOUT player=%d team=%d slot=%d definition=%d paint=%d\n", player->GetUserID(), row.team, row.slot, row.definition, row.skin.paint);
        }
        for (int slot = 0; slot < player->WeaponCount(); ++slot)
        {
            CWeaponCSBase *weapon = dynamic_cast<CWeaponCSBase *>(player->GetWeapon(slot));
            if (weapon && weapon->GetItemDefinition()) Msg("ICSM_NET_HELD player=%d entity=%d definition=%d paint=%d seed=%d wear=%.6f has_silencer=%d detachable=%d silenced=%d switching=%d\n",
                player->GetUserID(), weapon->entindex(), weapon->GetItemDefinitionIndex(), weapon->GetFallbackPaintKit(), weapon->GetFallbackSeed(), weapon->GetEconItemView()->GetCustomPaintKitWear(),
                weapon->HasSilencer(), weapon->HasDetachableSilencer(), weapon->IsSilenced(), weapon->IsSwitchingSilencer());
        }
    }
}
