//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef CSGO_HUDFREEZEPANEL_H_
#define CSGO_HUDFREEZEPANEL_H_

#pragma once

#include "panorama/csgo_panorama.h"
#include "panorama/hud/csgo_hud.h"	// TODO included to get CPanoramaHudElement - Move CPanoramaHudElement to its own file ?


namespace panorama
{
	class CLabel;
	class CProgressBar;
}
class CCSGO_AvatarImage;


//-----------------------------------------------------------------------------
// Purpose: Freeze Panel visible on player death
//-----------------------------------------------------------------------------
class CCSGO_HudFreezePanel : public panorama::CPanel2D, public CPanoramaHudElement
{
	DECLARE_PANEL2D( CCSGO_HudFreezePanel, panorama::CPanel2D );

public:

	CCSGO_HudFreezePanel( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CCSGO_HudFreezePanel();

	// CHudElement overrides
	virtual void LevelInit( void ) OVERRIDE;
	virtual void LevelShutdown( void ) OVERRIDE;
	virtual void ProcessInput( void ) OVERRIDE;
	virtual bool ShouldDraw( void ) OVERRIDE;

	// IGameEventListener Interface
	virtual void FireGameEvent( IGameEvent * event ) OVERRIDE;

	bool IsVisible( void ) const { return m_bIsVisible;  }
	bool IsHoldingAfterScreenShot() const { return m_bHoldingAfterScreenshot; }
	void TakeFreezeShot( void );
	void ResetDamageText( int iPlayerIndexKiller, int iPlayerIndexVictim );
	void OnHltvReplayButtonStateChanged( void );

private:

	enum DominationIconType
	{
		None,
		Nemesis,
		Revenge,
		DominationIconMax
	};

	void OnPlayerDeathGameEvent( IGameEvent *pEvent );
	void OnPlayerSpawnGameEvent( IGameEvent *pEvent );
	void OnHideFreezePanelGameEvent( IGameEvent *pEvent );
	void OnShowFreezePanelGameEvent( IGameEvent *pEvent );

	void ResetData();
	void ShowPanel( bool bShow );
	void ShowFreezePanel( bool bShow );				// [standard mode] show / hide the panorama panel "FreezePanel"
	void ShowFreezePanelScreenshot( bool bShow );	// [screenshot mode] show / hide the panorama panel "FreezePanelSS"
	enum ECancelPanelType_t
	{
		k_ECancelPanelType_None,
		k_ECancelPanelType_Replay,
		k_ECancelPanelType_Survival,
	};
	void ShowCancelPanel( ECancelPanelType_t eType );
	void PositionPanel( void );

	void PopulateNavigationText( void );
	void PopulateDominationInfo( DominationIconType iconType, const char* szlocalizationToken, const wchar_t *wszWeaponHTML, const wchar_t *wszOtherPlayerName );
	void PopulateDamageInfo( int nHitsTaken, int nDamageTaken, int nHitsGiven, int nDamageGiven );	// set nDamageTaken/nDamageGiven if you do not 
																									// wish to display the damage taken/given
	void PopulateAvatarInfo( const CSteamID &steamID, int nTeam, float flHealth );	// flHealth: value [0.0f, 1.0f], negative numbers to hide health bar
	void PopulateFreezeFrameBorder( const C_CSPlayer *pLocalPlayer );

	const char *GetFilesafePlayerName( const char *pszOldName );

	void GetLayoutDefines();
	bool OnStyleFileReloaded( panorama::CPanoramaSymbol symFile );

private:

	// "FreezePanel" panorama panel
	panorama::CPanel2D *m_pFreezePanel;
	CCSGO_AvatarImage *m_pAvatarPanel;
	panorama::CPanel2D *m_pAvatarDefaultTerroristPanel;
	panorama::CPanel2D *m_pAvatarDefaultCTPanel;
	panorama::CProgressBar *m_pAvatarHealthPanel;
	panorama::CLabel *m_pDescriptionTextPanel;
	panorama::CLabel *m_pDamageTakenPanel;
	panorama::CLabel *m_pDamageGivenPanel;
	panorama::CPanel2D *m_pItemContainerPanel;
	panorama::CImagePanel *m_pWeaponImagePanel;
	panorama::CPanel2D *m_pNavigationCancelPanel;
	panorama::CPanel2D *m_pNavigationSnapshotPanel;
	panorama::CPanel2D *m_pNavigationReplayPanel;

	// "FreezePanelSS" panorama panel
	panorama::CPanel2D *m_pFreezePanelSS;
	CCSGO_AvatarImage *m_pAvatarSSPanel;
	panorama::CPanel2D *m_pAvatarDefaultTerroristSSPanel;
	panorama::CPanel2D *m_pAvatarDefaultCTSSPanel;
	panorama::CProgressBar *m_pAvatarHealthSSPanel;
	panorama::CLabel *m_pDescriptionTextSSPanel;
	panorama::CPanel2D *m_pItemContainerSSPanel;
	panorama::CImagePanel *m_pWeaponImageSSPanel;

	// "FreezeCancel" panorama panel
	panorama::CPanel2D *m_pFreezeCancelPanel;
	panorama::CPanel2D *m_pSurvivalEndOfMatchShow;

	CHandle< CBaseEntity > m_FollowEntity;
	int m_iKillerIndex;

	int m_nFreezeFrameBorderCount;

	int m_nFreezePanelPosY;				// "FreezePanel" Y position in screen space (not panorama space 1920x1080)

	bool m_bIsVisible;					// root panel visibility
	bool m_bIsFreezePanelVisible;		// "FreezePanel" visibility
	bool m_bHoldingAfterScreenshot;
	bool m_bFreezePanelStateRelevant;	// flag showing if the information in the freeze panel is relevant 
										// and it makes sense to show, tracks show_freezepanel and hide_freezepanel events 
										// from the game; the panel may still be hidden sometimes (like before autoreplay) 
										// to avoid confusing the player (e.g. right before autoplay kicks in and there's 
										// not enough time to virually process it for a human being), even if the information 
										// in there is relevant
	bool m_bDominationIconVisible;
};

#endif	// CSGO_HUDFREEZEPANEL_H_