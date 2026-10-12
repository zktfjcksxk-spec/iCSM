#!/usr/bin/env python3
"""Exercise the exact kill-event body and audit its actual 2019 SVG resources.

The isolated fixtures check event identity, not a simulated Source match.
The real iOS gameplay modules are compiled separately.
"""
import importlib.util
import json
import pathlib
import re
import subprocess
import tempfile

W = pathlib.Path(__file__).resolve().parents[4]
C = pathlib.Path.home() / 'Library/Caches/csgo-ipados-arm64/i5'
BASE = C.parent / 'i4'
REL = 'source/game/shared/cstrike15/cs_gamerules.cpp'


def event_body(path):
    text = path.read_text(errors='surrogateescape')
    start = text.index('IGameEvent * CCSGameRules::CreateWeaponKillGameEvent(')
    return text[start:text.index('\n\t}\n', start) + 3]


spec = importlib.util.spec_from_file_location('catalog_audit', W / 'research/skins-2019/catalog.py')
catalog = importlib.util.module_from_spec(spec)
spec.loader.exec_module(catalog)
root = catalog.keyvalues((W / 'game-assets/csgo/scripts/items/items_game.txt').read_text())[0][1]
prefabs = {k: catalog.fields(v) for name, block, _ in root if name == 'prefabs' for k, v, _ in block}
items = {int(k): catalog.fields(v) for name, block, _ in root if name == 'items'
         for k, v, _ in block if k.isdigit()}


def inherit(item):
    result = {}
    for name in item.get('prefab', '').split():
        if name in prefabs:
            result.update(inherit(prefabs[name]))
    result.update(item)
    return result


audit = json.loads((W / 'research/skins-2019/catalog.json').read_text())
indices = {entry['defindex'] for entry in audit['weapons']} | {42, 59, 31}
entries = []
packed, _, _ = catalog.load_vpk()
hud = (BASE / 'source/game/client/cstrike15/panorama/hud/csgo_huddeathnotice.cpp').read_text()
renames = dict(re.findall(r'\{\s*"([^"]+)",\s*"([^"]+)"\s*\}', hud))
for index in sorted(indices):
    item = inherit(items[index])
    name = item['name']
    icon = name.removeprefix('weapon_')
    icon = renames.get(icon, icon)
    assert 'materials/panorama/images/icons/equipment/' + icon + '.svg' in packed, (index, name, icon)
    entries.append((index, item.get('item_class', name), name))

spectator = (C / 'source/game/client/cstrike15/panorama/hud/csgo_hudspectator.cpp').read_text()
guards = re.findall(r'pItem && pItem->IsValid\(\).*?pItem->GetItemDefinition\(\)', spectator)
assert len(guards) == 5 and all('GetItemIndex' not in guard for guard in guards)

fixture = r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include <strings.h>
#include <map>
#include <string>
constexpr int DMG_HEADSHOT=1, WEAPON_CLASSNAME_PREFIX_LENGTH=7;
#define V_strcmp strcmp
#define V_stricmp strcasecmp
#define V_strrchr strrchr
#define V_strlen strlen
#define V_snprintf snprintf
template<size_t N,typename... T> void V_sprintf_safe(char (&out)[N],const char *format,T... args){snprintf(out,N,format,args...);}
bool StringHasPrefix(const char *s,const char *prefix){return !strncasecmp(s,prefix,strlen(prefix));}
bool StringHasPrefixCaseSensitive(const char *s,const char *prefix){return !strncmp(s,prefix,strlen(prefix));}
bool IsWeaponClassname(const char *s){return StringHasPrefixCaseSensitive(s,"weapon_");}
struct CEconItemDefinition { const char *name; const char *GetDefinitionName()const{return name;} };
struct CEconItemView {
    const CEconItemDefinition *definition; bool valid=true;
    bool IsValid()const{return valid;}
    const CEconItemDefinition *GetStaticData()const{return definition;}
    const CEconItemDefinition *GetItemDefinition()const{return definition;}
    unsigned long long GetItemIndex()const{return 0;}
};
struct CBaseCombatWeapon;
struct CBaseEntity {
    const char *name; CBaseEntity(const char *n):name(n){}
    virtual ~CBaseEntity()=default;
    virtual CBaseCombatWeapon *MyCombatWeaponPointer(){return nullptr;}
    const char *GetClassname(){return name;}
};
struct CBaseCombatWeapon:CBaseEntity {
    const CEconItemView *item;
    CBaseCombatWeapon(const char *name,const CEconItemView *view):CBaseEntity(name),item(view){}
    CBaseCombatWeapon *MyCombatWeaponPointer()override{return this;}
    const CEconItemView *GetEconItemView(){return item;}
};
CBaseCombatWeapon *ToBaseCombatWeapon(CBaseEntity *entity){return entity?entity->MyCombatWeaponPointer():nullptr;}
struct CBasePlayer:CBaseEntity {
    CBaseCombatWeapon *active=nullptr;
    CBasePlayer():CBaseEntity("player"){}
    CBaseCombatWeapon *GetActiveWeapon(){return active;}
    int GetUserID(){return 7;}
    unsigned long long GetSteamIDAsUInt64(){return 0;}
};
struct CTakeDamageInfo {
    CBaseEntity *inflictor,*attacker,*weapon; int type=DMG_HEADSHOT;
    CBaseEntity *GetInflictor()const{return inflictor;}
    CBaseEntity *GetAttacker()const{return attacker;}
    CBaseEntity *GetWeapon()const{return weapon;}
    int GetDamageType()const{return type;}
    int GetObjectsPenetrated()const{return 2;}
};
struct IGameEvent {
    std::map<std::string,std::string> strings; std::map<std::string,int> ints;
    void SetString(const char *key,const char *value){strings[key]=value;}
    void SetInt(const char *key,int value){ints[key]=value;}
    void SetBool(const char *key,bool value){ints[key]=value;}
};
struct EventManager {
    IGameEvent event; bool fail=false;
    IGameEvent *CreateEvent(const char *){event={};return fail?nullptr:&event;}
} manager;
auto *gameeventmanager=&manager;
class CCSGameRules {
public:
    CBasePlayer *scorer=nullptr;
    CBasePlayer *GetDeathScorer(CBaseEntity *,CBaseEntity *){return scorer;}
    IGameEvent *CreateWeaponKillGameEvent(const char *,const CTakeDamageInfo &);
    IGameEvent *Original(const char *,const CTakeDamageInfo &);
};
'''
rows = ',\n'.join('{%d,%s,%s}' % (index, json.dumps(entity), json.dumps(name)) for index, entity, name in entries)
cases = r'''
int main(){
    CCSGameRules rules; CBasePlayer player; rules.scorer=&player;
    CEconItemDefinition def{"weapon_m4a1_silencer"}; CEconItemView item{&def};
    CBaseCombatWeapon m4("weapon_m4a1",&item);player.active=&m4;
    CTakeDamageInfo damage{&player,&player,&m4};
    assert(rules.Original("player_death",damage)->strings["weapon"]=="m4a1");
    auto kill=[&](CTakeDamageInfo value){auto *event=rules.CreateWeaponKillGameEvent("player_death",value);assert(event);return event->strings["weapon"];};
    struct Entry{int id;const char *entity,*name;};
    Entry entries[]={ROWS};
    for(const auto &entry:entries){
        CEconItemDefinition definition{entry.name};CEconItemView view{&definition};
        CBaseCombatWeapon weapon(entry.entity,&view);
        const char *expected=entry.name+7;
        // Damage identity survives a switch to a different held gun.
        assert(kill({&player,&player,&weapon})==expected);
        player.active=&weapon;
        assert(kill({&player,&player,nullptr})==expected);
        assert(kill({&weapon,&player,nullptr})==expected);
        assert(SpectatorDefinition(&view)); // Offline UID remains zero.
    }
    player.active=&m4;
    CBaseEntity grenade("hegrenade_projectile"),fire("inferno"),bomb("planted_c4");
    CBaseEntity flash("flashbang_projectile"),molotov("molotov_projectile_impact");
    CBaseEntity world("worldspawn"),trigger("func_door"),nonweapon("player");
    assert(kill({&grenade,&player,nullptr})=="hegrenade");
    assert(kill({&flash,&player,nullptr})=="flashbang");
    assert(kill({&fire,&player,nullptr})=="inferno");
    assert(kill({&molotov,&player,nullptr})=="molotov_projectile_impact");
    assert(kill({&bomb,&player,nullptr})=="c4");
    assert(kill({&player,&player,&nonweapon})=="m4a1_silencer");
    CEconItemView invalid{&def,false},missing{nullptr},empty{nullptr};
    CEconItemDefinition blank{""};empty.definition=&blank;
    CBaseCombatWeapon bad("weapon_m4a1",&invalid),absent("weapon_hkp2000",nullptr);
    CBaseCombatWeapon noDefinition("weapon_deagle",&missing),noName("weapon_knife",&empty);
    assert(kill({&player,&player,&bad})=="m4a1");
    assert(kill({&player,&player,&absent})=="hkp2000");
    assert(kill({&player,&player,&noDefinition})=="deagle");
    assert(kill({&player,&player,&noName})=="knife");
    assert(!SpectatorDefinition(&invalid) && !SpectatorDefinition(&missing) && !SpectatorDefinition(nullptr));
    auto *event=rules.CreateWeaponKillGameEvent("player_death",damage);
    assert(event->ints["headshot"]==1 && event->ints["penetrated"]==2 && event->ints["attacker"]==7);
    damage.type=0;assert(!rules.CreateWeaponKillGameEvent("player_death",damage)->ints["headshot"]);
    rules.scorer=nullptr;
    assert(kill({&world,&world,nullptr})=="world");
    assert(kill({&trigger,nullptr,nullptr})=="door");
    assert(kill({nullptr,nullptr,nullptr})=="world");
    player.active=nullptr;rules.scorer=&player;
    assert(kill({&player,&player,nullptr})=="world");
    manager.fail=true;assert(!rules.CreateWeaponKillGameEvent("player_death",damage));
    std::puts("PASS: exact kill event, all gun/knife definitions, offline spectator, delayed weapon switch, projectile/fire/C4, invalid/null/world guards, headshot/penetration unchanged");
}
'''.replace('ROWS', rows)
guard = 'bool SpectatorDefinition(const CEconItemView *pItem){return ' + guards[0] + ';}\n'
with tempfile.TemporaryDirectory(prefix='icsm-weapon-icons-') as directory:
    cpp, binary = pathlib.Path(directory) / 'icons.cpp', pathlib.Path(directory) / 'icons'
    cpp.write_text(fixture + event_body(C / REL) + '\n' + event_body(BASE / REL).replace(
        'CCSGameRules::CreateWeaponKillGameEvent', 'CCSGameRules::Original') + '\n' + guard + cases)
    subprocess.run(['xcrun', 'clang++', '-std=c++17', '-fsanitize=address,undefined',
                    '-O1', str(cpp), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print(f'PASS: {len(entries)} weapon definitions have matching SVGs in the supplied 2019 VPK')
