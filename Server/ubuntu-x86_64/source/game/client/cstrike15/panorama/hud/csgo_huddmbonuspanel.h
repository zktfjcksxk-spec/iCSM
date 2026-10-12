//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef CSGO_HUDMISSIONPANEL_H_
#define CSGO_HUDMISSIONPANEL_H_

#pragma once

#include "panorama/csgo_panorama.h"
#include "panorama/hud/csgo_hud.h"	// TODO included to get CPanoramaHudElement - Move CPanoramaHudElement to its own file ?

class CCSGO_HudDMBonusPanel : public panorama::CPanel2D, public CPanoramaHudElement
{
	DECLARE_PANEL2D( CCSGO_HudDMBonusPanel, panorama::CPanel2D );

public:
	CCSGO_HudDMBonusPanel( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CCSGO_HudDMBonusPanel();

	// HUD overrides
	virtual void Think() OVERRIDE;

	virtual void Reset() OVERRIDE { CPanoramaHudElement::Reset(); OnTimeTravel(); }
	virtual void LevelInit() OVERRIDE { CPanoramaHudElement::LevelInit(); OnTimeTravel(); }
	virtual void OnTimeJump() OVERRIDE { CPanoramaHudElement::OnTimeJump(); OnTimeTravel(); }

	virtual void FireGameEvent( IGameEvent *event ) OVERRIDE;

private:
	void OnTimeTravel();

	bool ShowDmBonusPanel();
	bool UpdateDmBonusPanel();

	bool m_bActive;				// Set if we have shown the panel in the last ShowDmBonusPanel()
	float m_fNextUpdateTime;	// Update once every few ticks

	loadout_positions_t m_nActivePosition;	// Valid when m_bActive.  Caches the loadout position of the active gun, used as a proxy for 'time traveled between different bonus periods' so we can re-initialize.

	// Weapon UI
	panorama::CImagePanel* m_pImageIcon;
	panorama::CPanel2D* m_pPanelTimerBar;
	panorama::CLabel* m_pLabelPoints;
	panorama::CPanel2D* m_pPanelKillPoints;
	panorama::CLabel* m_pLabelKillPoints;

	// Force an update on next Think()
	void ResetUpdateTime() { m_fNextUpdateTime = -1.0f; }

	// Sets our update time if it's sooner than our next forced update
	void SetUpdateTime( float fUpdateTime )
	{
		if ( fUpdateTime < m_fNextUpdateTime )
			m_fNextUpdateTime = fUpdateTime;
	}

	// Bonus kill handling
	float m_fBonusKillClearTime;
	bool m_bBonusKillEnabled;		// Invariant: m_bBonusKillEnabled == BHasClass( "bonuspanel--bonuskill" )
	void SetBonusKillEnabled( bool bSet );
};


#endif // CSGO_HUDMISSIONPANEL_H_
