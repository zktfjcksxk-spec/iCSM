#pragma once
class CCSPlayer;
class CWeaponCSBase;
class KeyValues;
namespace ICSMNetworkInventory
{
bool Receive(CCSPlayer *player, KeyValues *message);
void ApplyLoadout(CCSPlayer *player);
void ApplyPaint(CCSPlayer *player, CWeaponCSBase *weapon);
bool DeferJoin(CCSPlayer *player, int team, bool queue, int coach);
}
