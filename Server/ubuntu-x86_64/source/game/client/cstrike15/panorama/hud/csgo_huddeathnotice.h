//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: Used to display the csgo hud death panel 
//
//=============================================================================//

#ifndef CSGO_HUDDEATHNOTICE_H_
#define CSGO_HUDDEATHNOTICE_H_

#pragma once

#include "panorama/csgo_panorama.h"
#include "panorama/hud/csgo_hud.h"	// TODO included to get CPanoramaHudElement - Move CPanoramaHudElement to its own file ?


//-----------------------------------------------------------------------------
// Purpose: CSGO Hud Death Notice panel
//-----------------------------------------------------------------------------
class CCSGO_HudDeathNotice : public panorama::CPanel2D, public CPanoramaHudElement
{
	DECLARE_PANEL2D( CCSGO_HudDeathNotice, panorama::CPanel2D );

public:

	CCSGO_HudDeathNotice( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CCSGO_HudDeathNotice();

	// CHudElement overrides
	virtual void ProcessInput( void ) OVERRIDE;
	virtual void LevelInit( void ) OVERRIDE;
	virtual void LevelShutdown( void ) OVERRIDE;
	virtual bool ShouldDraw( void ) OVERRIDE;
	virtual void SetActive( bool bActive ) OVERRIDE;

	// CGameEventListener methods
	virtual void FireGameEvent( IGameEvent *event ) OVERRIDE;

	void ClearNotices();

private:

	void OnPlayerDeath( IGameEvent * event );
	void ShowPanel( bool bShow );
	void GetLayoutDefines();
	bool OnStyleFileReloaded( panorama::CPanoramaSymbol symFile );

private:

	panorama::CPanel2D *m_pVisibleNotices;

	bool m_bVisible;					// Element visibility flag

	float m_nNoticeLifetime;			// Seconds before notice begins to fade
	float m_nLocalPlayerLifetimeMod;	// m_nNoticeLifetime is multiplied by this value. The result
										// is the lifetime for death notices that features the local player
	float m_nFadeOutTime;				// seconds to fade out
};

#endif	// CSGO_HUDDEATHNOTICE_H_