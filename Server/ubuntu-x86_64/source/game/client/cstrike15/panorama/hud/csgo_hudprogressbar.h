//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Progress bar: Display value of local player's m_iProgressBarDuration, 
// set by server for defuse or hostage rescue.
// 
//=============================================================================//

#pragma once

#include "panorama/csgo_panorama.h"
#include "panorama/hud/csgo_hud.h"
#include "panorama/csgo_timers.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CCSGO_HudProgressBar : public panorama::CPanel2D, public CPanoramaHudElement
{
	DECLARE_PANEL2D( CCSGO_HudProgressBar, panorama::CPanel2D );

public:

	CCSGO_HudProgressBar( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CCSGO_HudProgressBar();

	// CPanel2D overrides
	virtual bool ShouldDraw( void ) OVERRIDE;
	virtual void OnLayoutReloaded() OVERRIDE;

	// CHudElement overrides
	virtual void Think( void ) OVERRIDE;


protected:
	bool SetupDialogFromPlayerAction( C_CSPlayer* pPlayer );

	CUtlVector< panorama::CPanelPtr< panorama::CPanel2D > > m_AnimatingPanels;
	panorama::CPanelPtr< panorama::CLabel > m_pActionLabel;
	panorama::CPanelPtr< panorama::CImagePanel > m_pActionIcon;
	// panorama::CPanelPtr< CCSGO_CountdownTimer> m_pCountdownTimer; // Doing this manually (at least for now) to guarantee synchronization
	void SetupPanels();

	int m_nDuration;
	float m_flStartTime;
	float m_flNextAnimationTick;
	CHandle<C_CSPlayer> m_hPlayer;

	bool m_bAnimationHackOffsetNextAnimation;
};