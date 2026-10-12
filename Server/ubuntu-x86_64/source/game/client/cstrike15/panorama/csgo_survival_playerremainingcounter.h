//========= Copyright © Valve Corporation, All rights reserved. ============//
//
//=====================================================================================//
#pragma once

#include "panorama/csgo_panorama.h"
#include "panorama/hud/csgo_hud.h"
#include "cs_gamerules_survival.h"

DECLARE_PANORAMA_EVENT1( SurvivalShowPlayerRemainingCounter, bool );
DECLARE_PANORAMA_EVENT0( SurvivalPlayerRemainingCounterUpdate );

class CCSGO_SurvivalPlayerRemainingCounter : public panorama::CPanel2D, public CPanoramaHudElement
{
	DECLARE_PANEL2D( CCSGO_SurvivalPlayerRemainingCounter, panorama::CPanel2D );

public:
	explicit CCSGO_SurvivalPlayerRemainingCounter( panorama::CPanel2D *pParent, const char *pchID );
	virtual void OnLayoutReloaded() OVERRIDE;
	virtual void SetActive( bool bActive ) OVERRIDE;
	virtual bool ShouldDraw( void ) OVERRIDE;
	virtual void Think( void );

protected:
	float m_flLastVocalizedTime;
	int m_numLastVocalizedEnemies;

	bool m_bShowingAsSpectator;
	int m_nAlivePlayers;
	int m_nAliveTeams;
	void UpdatePlayerCount( bool bVisible );
};
