
#ifndef _ITEM_CASH_H_
#define _ITEM_CASH_H_

#ifdef CLIENT_DLL
#include "c_items.h"
#include "c_cs_player.h"
#else
#include "items.h"
#include "cs_player.h"
#include "takedamageinfo.h"
#endif


#ifdef CLIENT_DLL
#define CItemCash C_ItemCash
#define CItem C_Item
#endif

#ifdef GAME_DLL
enum CashSpawnType_t
{
	CASH_SPAWN_WITH_RANDOM_VELOCITY,
	CASH_SPAWN_AT_POS_INSTANT_PICKUP,
};
enum CashPickupAnimTry_t
{
	CASH_PICKUP_ANIM_NEVER,
	CASH_PICKUP_ANIM_ONLY_IF_FISTS_ACTIVE,
	CASH_PICKUP_ANIM_ALWAYS
};
#endif // GAME_DLL

class CItemCash : public CItem
{
public:
	DECLARE_CLASS( CItemCash, CItem );
	DECLARE_NETWORKCLASS();

	CItemCash();

	// Client stuff
	virtual void Spawn() OVERRIDE;
	virtual void Precache() OVERRIDE;
	virtual int	ObjectCaps() OVERRIDE;

#ifdef GAME_DLL
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value ) OVERRIDE;
	virtual bool MyTouch( CBasePlayer* pPlayer ) OVERRIDE;
	virtual bool ItemCanBeTouchedByPlayer( CBasePlayer *pPlayer );

	char m_bufCashOriginalSource[64];
#endif

#ifdef CLIENT_DLL
	virtual void OnDataChanged( DataUpdateType_t changeType );
#endif

#ifdef GAME_DLL
	void SetSpawnType( CashSpawnType_t spawnType ) { m_nSpawnType = spawnType; }

private:
	CashPickupAnimTry_t m_nCashPickupAnimTryType;
	float m_flAllowPickupTime;
	CashSpawnType_t m_nSpawnType;
#endif // GAME_DLL
};

#ifdef GAME_DLL
void UTIL_SpawnPhysicalCash( Vector vecPosition, int nDollarAmount = 0, int nNumberOfStacks = 0, char const *pszOrigin = NULL, CashSpawnType_t spawnType = CASH_SPAWN_WITH_RANDOM_VELOCITY, CUtlVector< CBaseEntity* > *pOutputCash = NULL );
#endif // GAME_DLL


//////////////////////////////////////////////////////////////////////////
// Inline definitions
//////////////////////////////////////////////////////////////////////////

inline int CItemCash::ObjectCaps()
{
	return BaseClass::ObjectCaps() | FCAP_IMPULSE_USE | FCAP_DIRECTIONAL_USE | FCAP_WCEDIT_POSITION;
};

#endif // _ITEM_CASH_H_