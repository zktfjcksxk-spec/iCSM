//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef CSGO_HUDHEALTHARMOR_H_
#define CSGO_HUDHEALTHARMOR_H_

#pragma once

#include "panorama/csgo_panorama.h"
#include "panorama/hud/csgo_hud.h"	// TODO included to get CPanoramaHudElement - Move CPanoramaHudElement to its own file ?


namespace panorama
{
	class CLabel;
	class CProgressBar;
}


//-----------------------------------------------------------------------------
// Purpose: Used to draw health and armor 
//-----------------------------------------------------------------------------
class CCSGO_HudHealthArmor : public panorama::CPanel2D, public CPanoramaHudElement
{
	DECLARE_PANEL2D( CCSGO_HudHealthArmor, panorama::CPanel2D );

public:

	CCSGO_HudHealthArmor( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CCSGO_HudHealthArmor();

	// CHudElement overrides
	virtual void LevelInit( void ) OVERRIDE;
	virtual void SetActive( bool bActive ) OVERRIDE;
	virtual bool ShouldDraw( void ) OVERRIDE;

	virtual void Think() OVERRIDE;

private:

	void ResetData();
	void ShowPanel( bool bShow );
	void Update();

private:

	CHandle<C_BasePlayer> m_hPrevPlayer;
	int	m_nPrevHealth;
	int	m_nPrevArmor;
	bool m_bPrevHelmet;

	CountdownTimer	m_HealthFlashTimer;

	panorama::CProgressBar *m_pHealthBar;
	panorama::CProgressBar *m_pArmorBar;

	panorama::CPanel2D *m_pHudHealthArmorBG;	
};

#endif	// CSGO_HUDHEALTHARMOR_H_