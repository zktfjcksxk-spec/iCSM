//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//
//=============================================================================//

#ifndef CSGO_HUDWEAPONPANEL_H_
#define CSGO_HUDWEAPONPANEL_H_

#pragma once

#include "panorama/csgo_panorama.h"
#include "panorama/hud/csgo_hud.h"	// TODO included to get CPanoramaHudElement - Move CPanoramaHudElement to its own file ?
#include "cs_weapon_id.h"


class C_CSPlayer;
class C_WeaponCSBase;

//-----------------------------------------------------------------------------
// Purpose: CSGO Hud Death Notice panel
//-----------------------------------------------------------------------------
class CCSGO_HudWeaponPanel : public panorama::CPanel2D, public CPanoramaHudElement
{
	DECLARE_PANEL2D( CCSGO_HudWeaponPanel, panorama::CPanel2D );

public:

	CCSGO_HudWeaponPanel( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CCSGO_HudWeaponPanel();

	// CHudElement overrides
	virtual void LevelInit( void ) OVERRIDE;
	virtual void LevelShutdown( void ) OVERRIDE;
	virtual void SetActive( bool bActive ) OVERRIDE;
	virtual bool ShouldDraw( void ) OVERRIDE;

	virtual void Think() OVERRIDE;

private:

	void ResetData();
	void ShowPanel( bool bShow );
	void Update();

	void UpdateIconsUI( bool bCarryingDefuse );
	void UpdateAmmoUI( C_CSPlayer* pPlayer, C_WeaponCSBase* pWeapon, int nCurrentClip, int nMaxClip, int nTotalAmmo, bool bShowBurstSingle, bool bShowBurstBurst );
	void UpdateKillsUI( int nRoundkills, int nRoundKillsHeadshots, int nRequiredKills, int nKillEaterAltScore );

private:

	int m_nNextFreeShellIndex;

	CHandle<C_CSPlayer> m_hObservedPlayer;
	CHandle<C_WeaponCSBase> m_hObservedWeapon;

	int m_nPrevCurrentClip;
	int m_nPrevTotalAmmo;

	int m_nPrevRoundKills;
	int m_nPrevRoundKillsHeadshot;
	int m_nPrevKillEaterAltScore;

	// Icons panels
	panorama::CPanel2D *m_pDefuseIconPanel;

	// Ammo panels
	panorama::CPanel2D *m_pAmmoContentPanel;
	panorama::CLabel *m_pAmmoTextClipPanel;
	panorama::CLabel *m_pAmmoTextTotalPanel;
	panorama::CPanel2D *m_pAmmoBurstIconsPanel;
	panorama::CPanel2D *m_pWeaponPanelBottomBG;

	// Kill panels
	panorama::CPanel2D *m_pKillCountIconsPanel;
	panorama::CPanel2D *m_pKillCountTextPanel;
	panorama::CLabel *m_pKillCountPanel;
	panorama::CPanel2D *m_pKillCountTextIconPanel;
	panorama::CPanel2D *m_pKillEaterContentPanel;
	panorama::CLabel *m_pKillEaterCountPanel;

	// AmmoAnim panels
	panorama::CPanel2D *m_pAmmoAnimPanel;
	panorama::CPanel2D *m_pAmmoAnimBulletsPanel;
	panorama::CPanel2D *m_pAmmoAnimShellsPanel;
	panorama::CPanel2D *m_pAmmoAnimGrenadesPanel;
	panorama::CPanel2D *m_pAmmoAnimHealthshotPanel;
};

#endif	// CSGO_HUDWEAPONPANEL_H_