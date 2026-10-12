//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef SFWEAPONSELECTION_H
#define SFWEAPONSELECTION_H
#pragma once

#include "hudelement.h"
#include "ehandle.h"
#include "hud.h"
#include "hud_element_helper.h"
#include "scaleformui/scaleformui.h"
#include "sfhudflashinterface.h"
#include "cs_gamerules.h"
#include "c_cs_player.h"
#include "weapon_csbase.h"
#include "takedamageinfo.h"
#include "weapon_csbase.h"
#include "ammodef.h"
#include <vgui_controls/Panel.h>
#include "cs_hud_weaponselection.h"

struct WeaponSelectPanel
{	
	SFVALUE handle;
	double alpha;
	CHandle<C_WeaponCSBase> hWeapon;
	bool bSelected;
	bool bJustPickedUp;
	float fEndBlinkTime;
	float fLastBlinkTime;
};

//-----------------------------------------------------------------------------
// Purpose: Used to draw the history of weapon / item pickups and purchases by the player
//-----------------------------------------------------------------------------
class SFWeaponSelection : public SFHudFlashInterface
{
public:
	explicit SFWeaponSelection( const char *value );	
	
	C_CSPlayer* GetLocalOrHudPlayer( void );

	void AddWeapon( C_WeaponCSBase *pWeapon, bool bSelected );
	void UpdateGGNextPanel( bool bForceShowForTRBomb = false, bool bKnifeReached = false );
	void HideWeapon( gear_slot_t nSlot, gear_slotposition_t nCarryPos );
	void RemoveWeapon( gear_slot_t nSlot, gear_slotposition_t nCarryPos );

	WeaponSelectPanel CreateNewPanel( C_WeaponCSBase *pWeapon = NULL, bool bSelected = false );
	void CreateGGNextPanel( void );
	void ShowAndUpdateSelection( int nType = WEPSELECT_SWITCH, C_BaseCombatWeapon *pWeapon = NULL, bool bGiveInitial = false );
	void UpdatePanelPositions( void );

	void DisplayLevelUpNextWeapon( void );

	void RemoveItem( gear_slot_t nCarrySlot, gear_slotposition_t nCarryPos );
	void RemoveAllItems( void );

	virtual void ProcessInput( void );	

	virtual void LevelInit( void );
	virtual void LevelShutdown( void );
	virtual void SetActive( bool bActive );
	virtual bool ShouldDraw( void );

	void SetAlwaysShow( bool bAlwaysShow );

	virtual void FlashReady( void );
	virtual bool PreUnloadFlash( void );

	// CGameEventListener methods
	virtual void FireGameEvent( IGameEvent *event );

protected:
	// Calls either Show or Hide
	void ShowPanel( const bool bShow );

	// Show the item history
	void Show( void );
	// Hide the item history
	void Hide( void );

	virtual C_WeaponCSBase	*GetSelectedWeapon( void )
	{ 
		return dynamic_cast<C_WeaponCSBase*>(m_hSelectedWeapon.Get());
	}

private:
	static const int MAX_WEP_SELECT_PANELS = 13;
	static const int MAX_WEP_SELECT_POSITIONS = 6;
	static const int WEAPON_SELECT_PANEL_HEIGHT = 65;

	SFVALUE m_anchorPanel;
	float	m_lastUpdate;
	float	m_flFadeStartTime;
	//CUtlVector<WeaponSelectPanel> m_weaponPanels;
	WeaponSelectPanel m_weaponPanels[MAX_WEP_SELECT_PANELS][MAX_WEP_SELECT_POSITIONS];
	bool	m_bVisible;							// Element visibility flag
	CHandle< C_BaseCombatWeapon > m_hSelectedWeapon;
	WeaponSelectPanel m_selectedPanel;
	
	bool				m_bCreatedNextPanel;
	WeaponSelectPanel	m_ggNextPanel;

	int m_nPrevWepAlignSlot;
	int m_nPrevOccupiedWepSlot;

	int m_nOrigXPos;
	int m_nOrigYPos;
	bool m_bInitPos;

	bool m_bInitialized;

	bool	m_bAlwaysShow;						// When true, will not fade out

	int m_nLastTRKills;
	int m_nLastGGWepIndex;
	bool m_bUpdateGGNextPanel;
	float m_flUpdateInventoryAt;
	bool m_bUpdateInventoryReset;

	int m_bSpectatorTargetIndex;
};

#endif // SFWEAPONSELECTION_H
