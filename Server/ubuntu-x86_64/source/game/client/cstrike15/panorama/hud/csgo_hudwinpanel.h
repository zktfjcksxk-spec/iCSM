//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//
//=============================================================================//

#ifndef CSGO_HUDWINPANEL_H_
#define CSGO_HUDWINPANEL_H_

#pragma once

#include "panorama/csgo_panorama.h"
#include "panorama/hud/csgo_hud.h"	// TODO included to get CPanoramaHudElement - Move CPanoramaHudElement to its own file ?


class CCSGO_AvatarImage;

DECLARE_PANORAMA_EVENT1( HudWinPanelShowEvent, int );

//-----------------------------------------------------------------------------
// Purpose: CSGO Hud Win Panel
//-----------------------------------------------------------------------------
class CCSGO_HudWinPanel : public panorama::CPanel2D, public CPanoramaHudElement
{
	DECLARE_PANEL2D( CCSGO_HudWinPanel, panorama::CPanel2D );

public:

	CCSGO_HudWinPanel( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CCSGO_HudWinPanel();

	// CHudElement overrides
	virtual void LevelInit( void ) OVERRIDE;
	virtual void LevelShutdown( void ) OVERRIDE;
	virtual void ProcessInput( void )  OVERRIDE;
	virtual bool ShouldDraw( void ) OVERRIDE;
	virtual void SetActive( bool bActive ) OVERRIDE;

	// CGameEventListener methods
	virtual void FireGameEvent( IGameEvent *event ) OVERRIDE;

	bool	IsVisible( void ) const { return m_bVisible; }

	// Force display win panel
	void ForceDisplayWinPanel( int iEndEvent );

	// Accessed by CUiComponent_MatchStats
	int m_iMVP;

private:

	void ShowTeamWinPanel( int iEndEvent, int nPlacement );
	void ShowGunGameWinPanel();
	void ShowMVP( C_CSPlayer* pPlayer, CSMvpReason_t reason, int32 nMusicKitStatTrak = 0 );
	void ShowMusicKit( XUID xuidPlayer, const char *szPlayerNameUTF8, int32 nMusicKitStatTrak );
	void ShowWinPanelExtraData( void );
	void ShowProgress( int nIdealProgressCatagory );
	void ShowFunFact();


	void ShowPanel( bool bShow );
	void ForceHide();
	void HideAllWinPanelExtra();
	void ShowWinPanelExtra( panorama::CPanel2D *pPanel );

private:

	bool m_bVisible;
	bool m_bShouldSetWinPanelExtraData;
	float m_fSetWinPanelExtraDataTime;

	int				m_nFunFactPlayer;
	string_t		m_nFunfactToken;
	int				m_nFunFactParam1;
	int				m_nFunFactParam2;
	int				m_nFunFactParam3;
	int				m_nWinEventTriggered;

	// Team Win Panel
	panorama::CLabel *m_pTitlePanel;
	panorama::CLabel *m_pSurrenderPanel;
	panorama::CImagePanel *m_pTeamLogoPanel;

	// MVP
	panorama::CPanel2D *m_pMVPPanel;
	panorama::CLabel *m_pMVPWinnerNamePanel;
	CCSGO_AvatarImage *m_pMVPAvatarPanel;
	CCSGO_AvatarImage *m_pMVPSurvivalAvatarPanel;
	panorama::CPanel2D *m_pMVPGunGameAvatarsPanel;
	CCSGO_AvatarImage *m_pMVPAvatar2ndPanel;
	CCSGO_AvatarImage *m_pMVPAvatar3rdPanel;
	panorama::CPanel2D *m_pMVPMusicKitPanel;
	panorama::CPanel2D *m_pMVPMusicKitAnimPanel;
	panorama::CLabel *m_pMVPMusicKitStatTrackPanel;
	panorama::CLabel *m_pMVPMusicKitNamePanel;
	panorama::CLabel *m_pMVPMusicKitInfoPanel;
	panorama::CLabel *m_pSurvivalPlacement;
	panorama::CImagePanel *m_pMVPMusicKitIconPanel;

	// Fun Facts
	panorama::CPanel2D *m_pFunFactPanel;
	panorama::CLabel *m_pFunFactTextPanel;

	// Medals
	panorama::CPanel2D *m_pMedalsPanel;
	panorama::CPanel2D *m_pMedalsContainerPanel;

	// Medal Stats
	panorama::CPanel2D *m_pMedalStatsPanel;
	panorama::CImagePanel *m_pMedalStatsIconPanel;
	panorama::CLabel *m_pMedalStatsTextPanel;

	// Rank Up
	panorama::CPanel2D *m_pRankUpPanel;

	// Gun Game Extra data
	panorama::CPanel2D *m_pGunGameExtraPanel;
	panorama::CLabel *m_pGunGameExtraTitlePanel;
	panorama::CLabel *m_pGunGameExtraWeaponNamePanel;
	panorama::CImagePanel *m_pGunGameExtraWeaponIconPanel;
	panorama::CImagePanel *m_pGunGameExtraGrenadeIconPanel;

	// Progress
	panorama::CPanel2D *m_pProgressPanel;
};

#endif	// CSGO_HUDWINPANEL_H_