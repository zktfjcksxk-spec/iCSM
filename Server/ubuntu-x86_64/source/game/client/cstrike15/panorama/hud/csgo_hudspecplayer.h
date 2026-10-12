//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Spectator hud panel
// 
//=============================================================================//

#pragma once

#include "panorama/csgo_panorama.h"
#include "panorama/hud/csgo_hud.h"
#include "panorama/csgo_avatarimage.h"
#include "panorama/csgo_item_image_panel.h"

#define SPECPANEL_HOTKEY_VISIBLETIME 8.0f
#define SPECPANEL_HOTKEY_FADEOUTTIME 4.5f

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CCSGO_HudSpecPlayer : public panorama::CPanel2D, public CPanoramaHudElement, public panorama::CDefaultInputCapture
{
	DECLARE_PANEL2D( CCSGO_HudSpecPlayer, panorama::CPanel2D );

public:

	explicit CCSGO_HudSpecPlayer( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CCSGO_HudSpecPlayer();

	void FireGameEvent( IGameEvent *evt ) OVERRIDE;

	// CHudElement overrides
	virtual void LevelInit( void ) OVERRIDE;
	virtual void ProcessInput( void )  OVERRIDE;
	virtual void Think() OVERRIDE;

	void UpdateReplayUI();
	void ShowReplayUi();
	virtual void OnTimeJump();

protected:
	void UpdateSpectatedPlayer( void );
	void UpdatePlayerStats( void );
	void UpdateHotkeyText();
	void DisplayEventServerImage( void );
	int FindNextSpectatorTarget( bool bReverse );
	void ShowSpectatorPanel( bool bShow );

	bool IsTargetValidPlayer() { return ( ( m_nTargetID > 0 ) && ( m_nTargetID <= MAX_PLAYERS ) ); }
	bool IsTargetWorld() { return ( m_nTargetID == 0 ); }

	int NeedReplayUI();
	
	void PopulateDamageInfoEvent( IGameEvent *pEvent );
	void PopulateDamageInfo( int nHitsTaken, int nDamageTaken, int nHitsGiven, int nDamageGiven );	// set nDamageTaken/nDamageGiven if you do not 

	void UpdateReplayUI( C_CSPlayer * pPlayer );
	
	void ShowReplayPlayerTag_Internal( bool bShow );
	void ShowReplayPlayerTag( bool bShow );

	enum HOTKEY_TYPE
	{
		HOTKEY_TYPE_CONTROLBOT,
		HOTKEY_TYPE_XRAY,
		HOTKEY_TYPE_CANCELREPLAY,
		HOTKEY_TYPE_ARROWS,
		HOTKEY_TYPE_CAMERA,
		HOTKEY_TYPE_MAP,
		HOTKEY_TYPE_CAMERAMANON,
		HOTKEY_TYPE_DIERCTORON,
		HOTKEY_TYPE_REPLAYLASTKILL,

		HOTKEY_TYPE_MAX,
	};

	panorama::CPanelPtr< CCSGO_AvatarImage > m_pPlayerAvatar;
	panorama::CPanelPtr< panorama::CPanel2D > m_pPlayerAvatarColor;
	Color m_colorForAvatarPlayer;
	panorama::CPanelPtr< panorama::CImagePanel > m_pPlayerAvatarDefault;
	panorama::CPanelPtr< CItemImagePanel > m_pPlayerAvatarDisplayItem;
	
	panorama::CPanelPtr< panorama::CLabel > m_pDamageTakenPanel;
	panorama::CPanelPtr< panorama::CLabel > m_pDamageGivenPanel;

	panorama::CPanel2D *m_pStatsLabelContainer;

	CUtlVectorFixed< panorama::CPanelPtr< panorama::CLabel >, 5 > m_pStat_HeadX;
	CUtlVectorFixed< panorama::CPanelPtr< panorama::CLabel >, 5 > m_pStat_NumX;

	panorama::CPanel2D *m_pHotKeyLabelContainer;
	panorama::CPanel2D *m_pHotKeys[ HOTKEY_TYPE_MAX ];	

	panorama::CPanel2D *m_pWeaponImageContainer;
	panorama::CPanelPtr< CItemImagePanel > m_pWeaponImagePanel;
	panorama::CPanelPtr< panorama::CLabel > m_pWeaponNameLabel;	
	panorama::CPanel2D *m_pDefaultTeamLogo;
	panorama::CPanel2D *m_pTeamLogo;
	panorama::CImagePanel *m_pServerSponserLogos;

	panorama::CPanel2D *m_pReplayPlayerTag;
	
	panorama::CPanel2D *m_pColorStrip;	

	int						m_nTargetID;			// Spectator target index

	// These are used to keep from updating the spectator scaleform display every frame
	int						m_iLastTargetID;
	int						m_iLastTargetControlledByID;
	int						m_iLastHealth;
	int						m_iLastArmor;
	float					m_fNextUpdateTime;
	itemid_t				m_iLastItemId;
	itemid_t				m_iLastDisplayItemId;

	bool					m_bReplayPlayerTagVisible;
	bool					m_bCallShowReplayUI;

	int						m_nDamageGiven;
	int						m_nDamageTaken;

};