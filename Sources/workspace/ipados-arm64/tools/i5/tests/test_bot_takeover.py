#!/usr/bin/env python3
"""Exercise the actual handoff body with isolated ownership/ammo API fixtures.

This does not simulate a Source match. The real iOS target is compiled separately;
these cases catch deleting transferred entities or replacing their ammo/selection.
"""
import pathlib
import subprocess
import tempfile

SOURCE = pathlib.Path.home() / 'Library/Caches/csgo-ipados-arm64/i5/source/game/server/cstrike15/cs_player.cpp'
text = SOURCE.read_text(errors='surrogateescape')
start = text.index('void CCSPlayer::TransferInventory( CCSPlayer* pTargetPlayer )')
body = text[start:text.index('\n}\n', start) + 2]
fixture = r'''
#include <cassert>
#include <cstdio>
#include <cstdarg>
#include <array>
constexpr int MAX_AMMO_TYPES = 8;
class CCSPlayer;
struct CBaseCombatWeapon {
    int id, clip, reserve, paint; bool silenced, removed = false, grenade = false;
    CCSPlayer *owner = nullptr;
    CCSPlayer *GetOwner() { return owner; }
    int entindex() { return id; }
};
template<class T> struct CHandle {
    T *p; CHandle(T *value):p(value){} T *Get(){return p;} T *operator->(){return p;}
};
void Msg(const char *, ...) {}
class CCSPlayer {
public:
    std::array<CBaseCombatWeapon *, 8> weapons{};
    std::array<int, MAX_AMMO_TYPES> ammo{};
    CBaseCombatWeapon *active = nullptr;
    int id = 0, armor = 0, m_iAccount = 0, m_iAccountMoneyEarnedForNextRound = 0;
    float m_flLastEquippedHelmetTime = 0, m_flLastEquippedArmorTime = 0;
    bool m_bHasHelmet = false, m_bHasHeavyArmor = false;
    bool m_bHasNightVision = false, m_bNightVisionOn = false, m_bHasDefuser = false;
    int entindex(){return id;} int ArmorValue(){return armor;}
    void SetArmorValue(int v){armor=v;}
    int WeaponCount(){return weapons.size();}
    CBaseCombatWeapon *GetWeapon(int i){return weapons[i];}
    CBaseCombatWeapon *GetActiveWeapon(){return active;}
    int GetAmmoCount(int i){return ammo[i];}
    void SetAmmoCount(int count,int i){ammo[i]=count;}
    bool Weapon_Detach(CBaseCombatWeapon *w){
        for(auto &slot:weapons) if(slot==w){slot=nullptr;w->owner=nullptr;
            if(active==w)active=nullptr;return true;}return false;
    }
    void Weapon_Equip(CBaseCombatWeapon *w){
        for(auto &slot:weapons)if(!slot){slot=w;w->owner=this;
            if(w->grenade)ammo[3]=1; active=w;return;}assert(false);
    }
    void Weapon_Switch(CBaseCombatWeapon *w){assert(w->owner==this && !w->removed);active=w;}
    void RemoveAllItems(bool){
        for(auto &slot:weapons)if(slot){slot->removed=true;slot->owner=nullptr;slot=nullptr;}
        active=nullptr;ammo.fill(0);armor=0;m_bHasHelmet=m_bHasHeavyArmor=false;
        m_bHasNightVision=m_bNightVisionOn=m_bHasDefuser=false;
    }
    void UpdateVoicePitch(){} void UpdateAddonBits(){} void RecalculateCurrentEquipmentValue(){}
    void TransferInventory(CCSPlayer *);
};
'''
cases = r'''
int main(){
    for(int activeSlot: {0,2,4}){
        CCSPlayer bot, human; bot.id=1;human.id=2;
        CBaseCombatWeapon rifle{11,7,47,801,true}, pistol{12,4,19,600,false};
        CBaseCombatWeapon grenade{13,0,0,0,false}, spawnPistol{20,13,52,0,false};
        grenade.grenade=true;
        bot.weapons[0]=&rifle;bot.weapons[2]=&pistol;bot.weapons[4]=&grenade;
        for(auto *w:bot.weapons)if(w)w->owner=&bot;
        bot.active=bot.weapons[activeSlot];auto *expected=bot.active;
        bot.ammo[3]=2;bot.ammo[6]=5;bot.armor=63;bot.m_bHasHelmet=true;
        bot.m_bHasDefuser=true;bot.m_iAccount=2400;bot.m_iAccountMoneyEarnedForNextRound=300;
        human.Weapon_Equip(&spawnPistol);
        bot.TransferInventory(&human);
        assert(human.active==expected && spawnPistol.removed);
        for(auto *w: {&rifle,&pistol,&grenade})assert(w->owner==&human && !w->removed);
        assert(rifle.clip==7 && rifle.reserve==47 && rifle.paint==801 && rifle.silenced);
        assert(human.ammo[3]==2 && human.ammo[6]==5 && human.armor==63);
        assert(human.m_bHasHelmet && human.m_bHasDefuser && human.m_iAccount==2400);
        assert(human.m_iAccountMoneyEarnedForNextRound==300);
        for(auto *w:bot.weapons)assert(!w);
        for(int a:bot.ammo)assert(a==0);
        assert(!bot.active && bot.armor==0 && !bot.m_bHasDefuser);
        // Releasing control must return the same, now partially used inventory.
        rifle.clip=2;human.ammo[3]=1;human.TransferInventory(&bot);
        assert(bot.active==expected && rifle.owner==&bot && !rifle.removed && rifle.clip==2);
        assert(bot.ammo[3]==1 && human.ammo[3]==0);
        bot.TransferInventory(nullptr);bot.TransferInventory(&bot);
        assert(bot.active==expected && !rifle.removed);
    }
    CCSPlayer empty, recipient;
    CBaseCombatWeapon defaultGun{99,12,24,0,false};recipient.Weapon_Equip(&defaultGun);
    empty.TransferInventory(&recipient);assert(defaultGun.removed && !recipient.active);
    std::puts("PASS: original entities, all active slots, clips/reserve/skin/silencer, grenade counts, armor, reverse handoff, null/self/empty guards");
}
'''
with tempfile.TemporaryDirectory(prefix='icsm-bot-handoff-') as directory:
    cpp = pathlib.Path(directory) / 'handoff.cpp'
    binary = pathlib.Path(directory) / 'handoff'
    cpp.write_text(fixture + body + '\n' + cases)
    subprocess.run(['xcrun', 'clang++', '-std=c++17', '-fsanitize=address,undefined',
                    '-O1', str(cpp), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
