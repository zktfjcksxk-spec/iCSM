
#ifndef _ITEM_DOGTAGS_H_
#define _ITEM_DOGTAGS_H_

#ifdef CLIENT_DLL
#include "c_items.h"
#include "c_cs_player.h"
#else
#include "items.h"
#include "cs_player.h"
#include "takedamageinfo.h"
#endif


#ifdef CLIENT_DLL
#define CItemDogtags C_ItemDogtags
#define CItem C_Item
#endif

class CItemDogtags : public CItem
{
public:
	DECLARE_CLASS( CItemDogtags, CItem );
	DECLARE_NETWORKCLASS();

	CItemDogtags();

	CNetworkVar(CHandle<CCSPlayer>, m_OwningPlayer);
	CNetworkVar(CHandle<CCSPlayer>, m_KillingPlayer);

	CCSPlayer* GetOwner();
	CCSPlayer* GetKiller();

	bool CanBePickedUpBy( CCSPlayer* pPlayer );

	// Client stuff
	virtual void Spawn() OVERRIDE;
	virtual void Precache() OVERRIDE;
	virtual int	ObjectCaps() OVERRIDE;

#ifdef GAME_DLL
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value ) OVERRIDE;
	virtual bool ItemCanBeTouchedByPlayer( CBasePlayer *pPlayer ) OVERRIDE;
	virtual bool MyTouch( CBasePlayer* pPlayer ) OVERRIDE;

	void SetDogtagInfo( CCSPlayer* pKiller, CCSPlayer* pTarget );
	void LifetimeExpired();
#endif

#ifdef CLIENT_DLL
	virtual void OnDataChanged( DataUpdateType_t changeType );
#endif
};


//////////////////////////////////////////////////////////////////////////
// Inline definitions
//////////////////////////////////////////////////////////////////////////

inline CCSPlayer* CItemDogtags::GetOwner()
{
	return m_OwningPlayer.Get();
}

inline CCSPlayer* CItemDogtags::GetKiller()
{
	return m_KillingPlayer.Get();
}

inline int CItemDogtags::ObjectCaps()
{
	return BaseClass::ObjectCaps() | FCAP_IMPULSE_USE | FCAP_DIRECTIONAL_USE | FCAP_WCEDIT_POSITION;
};

#endif // _ITEM_DOGTAGS_H_