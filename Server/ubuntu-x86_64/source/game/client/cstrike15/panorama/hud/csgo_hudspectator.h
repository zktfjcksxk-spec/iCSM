//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Spectator hud panel
// 
//=============================================================================//

#pragma once

#include "cbase.h"
#include "panorama/csgo_panorama.h"
#include "panorama/hud/csgo_hud.h"
#include "panorama/csgo_avatarimage.h"
#include "csgo_hudteamcounter.h"
#include "panorama/csgo_item_image_panel.h"
#include "panorama/uiinputcapture.h"
#include "csgo_hudblurtarget.h"

#define MAX_PLAYERPANEL_WEAPONS 8
#define PLAYERPANEL_HEALTHFLASH_LEN 2.25f
#define MAX_PLAYER_ITEM_DROP_DISPLAY 5

struct PlayerSpecStatus_t
{
	XUID		nXUID;
	int			nPlayerIdx;
	bool		bIsPrimarySelected;
	int			nSecondarySelectedSlot;
	const char	*szPrimaryWeaponName;
	int			nHealth;
	int			nArmor;
	int			nMoney;
	bool		bIsCT;
	bool		bHasDefBomb;
	bool		bDead;
	bool		bHasHelmet;
	bool		bSpeaking;
	bool		bPlayerBot;		// indicates that the player took over this bot
	bool		bSpectated;
	bool		bForceAvatarRefresh;
	int			nRoundKills;

	int			nGGProgressiveRank;	// save this character's rank

	char		*szFinalWeaponString;
	uint64		nFinalWeaponStringInt; // unique id based on the weapons in wszFinalWeaponString so we don't need to strcmp

	PlayerSpecStatus_t() :
		nXUID( INVALID_XUID ), nPlayerIdx( -1 ), bIsPrimarySelected( true ), nSecondarySelectedSlot( 0 ), szPrimaryWeaponName( "" ), bIsCT( false ), bDead( false ), bSpeaking( false ), bPlayerBot( false ), bSpectated( false ), nGGProgressiveRank( -1 ), /*nGren0(-1), nGren1(-1), nGren2(-1),*/ nRoundKills( -1 ), szFinalWeaponString( "" ), nFinalWeaponStringInt( 0 )
	{
	}

	PlayerSpecStatus_t( const PlayerSpecStatus_t &copy )
	{
		memcpy( this, &copy, sizeof( PlayerSpecStatus_t ) );
	}

	PlayerSpecStatus_t& operator=( const PlayerSpecStatus_t &rhs )
	{
		memcpy( this, &rhs, sizeof( PlayerSpecStatus_t ) );
		return *this;
	}

	void Reset()
	{
		memset( this, 0, sizeof( PlayerSpecStatus_t ) );
		nXUID = INVALID_XUID;
		nPlayerIdx = -1;
		nGGProgressiveRank = -1;
	}

	// Returns true if any of these fields has changed since last update
	bool Update( XUID _Xuid, int _PlayerIdx, bool _bIsPrimarySelected, int _nSecondarySelectedSlot, const char *_szPrimaryWeaponName, int _nHealth, int _nArmor, int _nMoney, bool _IsCT, bool _bHasDefBomb, bool _bHasHelmet, bool _Dead, bool _Speaking, bool _PlayerBot, bool _Spectated, int _nRoundKills, char *_szFinalWeaponString, uint64 _nFinalWeaponStringInt )
	{
		bool bDiff = ( _Xuid != nXUID ) ||
			( _PlayerIdx != nPlayerIdx ) ||
			( _nHealth != nHealth ) ||
			( _nArmor != nArmor ) ||
			( _nMoney != nMoney ) ||
			( _bIsPrimarySelected ^ bIsPrimarySelected ) ||
			( _nSecondarySelectedSlot != nSecondarySelectedSlot ) ||
			( _nFinalWeaponStringInt != nFinalWeaponStringInt ) ||
			( _IsCT			^ bIsCT ) ||
			( _bHasDefBomb	^ bHasDefBomb ) ||
			( _bHasHelmet	^ bHasHelmet ) ||
			( _Dead			^ bDead ) ||
			( _Speaking		^ bSpeaking ) ||
			( _PlayerBot	^ bPlayerBot ) ||
			( _Spectated	^ bSpectated ) ||
			( _nRoundKills	!= nRoundKills );

		nXUID = _Xuid;
		nPlayerIdx = _PlayerIdx;
		bIsPrimarySelected = _bIsPrimarySelected;
		nSecondarySelectedSlot = _nSecondarySelectedSlot;
		szPrimaryWeaponName = _szPrimaryWeaponName;
		nHealth = _nHealth;
		nArmor = _nArmor;
		nMoney = _nMoney;
		bIsCT = _IsCT;
		bHasDefBomb = _bHasDefBomb;
		bHasHelmet = _bHasHelmet;
		bDead = _Dead;
		bSpeaking = _Speaking;
		bPlayerBot = _PlayerBot;
		bSpectated = _Spectated;
		nRoundKills = _nRoundKills;
		szFinalWeaponString = _szFinalWeaponString;
		nFinalWeaponStringInt = _nFinalWeaponStringInt;

		return bDiff;
	}
};

class CSpecPlayerPanelData
{
public:
	CSpecPlayerPanelData();
	~CSpecPlayerPanelData();

	//void Init( panorama::CPanel2D *pParent, int idx );
	bool HasHealthFlashTimeElapsed( void );
	float GetHealthFlashLerpFrac(void);
	void StartHealthFlashTimer();
	void InitRedHealthBar();
	void SetHealthBarType( panorama::CPanoramaSymbol symName );

	int	 m_nPlayerIdx;
	bool m_bWasDead;
	bool m_bWasPlayerBot;
	bool m_bPlayerExtraDataVisible;

	// parent
	panorama::CPanel2D *m_pPanel;

	// visible/invisible panels - all other panels parented under these.
	//panorama::CPanel2D *m_pVisible;
	//panorama::CPanel2D *m_pInvisible;

	// children
	
	float	m_flPrevHealth;
	float	m_flCurHealth;

	bool m_bPrevHasHelmet;
	//bool m_bPrevHasHeavyAssaultSuit;
	//bool m_bGotDefaultColors;

	//CountdownTimer	m_HealthFlashTimer;
	float m_flHealthFlashStartTime;

	panorama::CPanel2D *m_pPlayerPanelParent;
	panorama::CPanel2D *m_pHighlight;
	panorama::CPanel2D *m_pPlayerPanelBG;
	panorama::CPanel2D *m_pDeadBG;
	panorama::CLabel *m_pHealthLabel;
	panorama::CProgressBar *m_pHealthBar;
	panorama::CProgressBar *m_pHealthBarRed;
	panorama::CLabel *m_pHealthText;
	panorama::CLabel *m_pMoneyText;
	panorama::CLabel *m_pPlayerName;
	panorama::CPanel2D *m_pArmorPanel;
	panorama::CImagePanel *m_pArmorIcon;
	panorama::CImagePanel *m_pC4DefuserIcon;

	panorama::CPanel2D *m_pKillPanel;
	panorama::CLabel *m_pKillText;

	CUtlVectorFixed< panorama::CImagePanel*, MAX_PLAYERPANEL_WEAPONS > m_pWeaponIcons;
	panorama::CImagePanel* m_pWeaponMain;

	panorama::CPanel2D *m_pAvatarBucket;
	CAvatarPanelData *m_pAvatar;

	panorama::CPanel2D *m_pExtraDataPanel;
	panorama::CLabel *m_pExtraDataPanel_K;
	panorama::CLabel *m_pExtraDataPanel_A;
	panorama::CLabel *m_pExtraDataPanel_D;
	panorama::CLabel *m_pExtraDataPanel_MoneySpent;

	//panorama::CPanelPtr< panorama::CLabel > m_pPlayerNameLabel;
	//panorama::CPanelPtr< CCSGO_AvatarImage > m_pPlayerAvatar;
};


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
DECLARE_PANEL_EVENT0( SpectatorSelectClickedPlayer );

class CCSGO_HudSpectator : public panorama::CPanel2D, public CPanoramaHudElement
{
	DECLARE_PANEL2D( CCSGO_HudSpectator, panorama::CPanel2D );

public:
	explicit CCSGO_HudSpectator( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CCSGO_HudSpectator();

	void FireGameEvent( IGameEvent *evt ) OVERRIDE;

	// CHudElement overrides
	virtual void	LevelInit( void ) OVERRIDE;
	virtual void	LevelShutdown( void ) OVERRIDE;
	bool EventAnimationEnd( const panorama::CPanelPtr< panorama::IUIPanel > &panelPtr, panorama::CPanoramaSymbol symAnimation );
	bool HandleSelectClickedPlayer( const panorama::CPanelPtr< panorama::IUIPanel > &clickedPanel );

	static CCSGO_HudSpectator *GetInstance() { return s_pSpecPanels; }

	bool ShouldHideMiniScoreboard() { return m_bHideMiniScoreboard; }

	virtual void Think() OVERRIDE;

	void ForceRefreshPanels( void );

protected:
	enum PLAYER_TEAM_COUNT
	{
		MAX_TEAM_SIZE = 5
	};

	void UpdateSpectateMode( bool bShow );
	void UpdateTimer( void );
	void SetBombDefused( void );
	void UpdatePlantedBombState( float flDetProgress );
	void UpdateRounds( void );
	void UpdateScoreAndTeamNames( void );
	void UpdateEventDropsAndViewers( void );
	void ShowExtraMatchStats( bool bShowValue, int nEQValueT, int nEQValueCT );
	void SetNumberOfMatchesWonPips( int nBestOfPipsPerSide, int nPipsFilled, int nTeamID );
	bool ShouldShowSpecPlayerPanels( void );
	void DisplayEventServerImage( void );
	void InitPlayerPanelSnippet( panorama::CPanel2D *pParent, int idx, bool bIsCT );
	CAvatarPanelData *InitAvatarSnippet( CSpecPlayerPanelData *pParent, int idx, bool bIsCT );

	int	GetHLTVSpectators() const { return m_HLTVSpectators; }
	int	GetTournamentItemsDroppedThisRound() const { return m_nTourneyItemsDroppedThisRound; }
	int GetTournamentItemsDroppedTotal() const { return m_nTourneyItemsDroppedTotal; }
	void EnableCursorInput( bool bEnable );

	panorama::CPanel2D *m_pTeamSpec_L;
	panorama::CPanel2D *m_pTeamSpec_R;
	panorama::CPanel2D *m_pTeamSpecTeamMoney_L;
	panorama::CPanel2D *m_pTeamSpecTeamMoney_R;
	panorama::CLabel *m_pTeamMoneyText_L;
	panorama::CLabel *m_pTeamMoneyText_R;
	panorama::CLabel *m_pEQValueText_L;
	panorama::CLabel *m_pEQValueText_R;
	panorama::CImagePanel *m_pBombPlanted;
	panorama::CImagePanel *m_pBombPlantedLines;
	panorama::CImagePanel *m_pBombDefused;

	panorama::CLabel *m_pTimer;
	panorama::CLabel *m_pRoundText;
	panorama::CLabel *m_pScore_L;
	panorama::CLabel *m_pScore_R;	
	panorama::CImagePanel *m_pTeamLogoL;
	panorama::CImagePanel *m_pTeamLogoR;
	panorama::CLabel *m_pTeamName_L;
	panorama::CLabel *m_pTeamName_R;

	panorama::CPanel2D *m_pExtraMatchData;
	panorama::CLabel *m_pExtraMatchDataText;
	panorama::CLabel *m_pExtraMatchDataTextL;
	panorama::CLabel *m_pExtraMatchDataTextR;

	// pickembar
	panorama::CPanel2D *m_pPickemPredictionsPanel;
	panorama::CProgressBar *m_pPickemBar;
	panorama::CLabel *m_pPickemPercentL;
	panorama::CLabel *m_pPickemPercentR;

	// best of pips
	panorama::CPanel2D *m_pBestOfPipsPanel;
	panorama::CLabel *m_pBestOfPipsText;
	panorama::CPanel2D *m_pBestOfPipsL;
	CUtlVectorFixed< panorama::CImagePanel*, 3 > m_pBestOfPipsIconsL;
	panorama::CPanel2D *m_pBestOfPipsR;
	CUtlVectorFixed< panorama::CImagePanel*, 3 > m_pBestOfPipsIconsR;

	// event logo, drops and viewers
	panorama::CPanel2D *m_pEventDropsViewersRoot;
	panorama::CPanel2D *m_pEventLogo;
	panorama::CImagePanel *m_pEventLogoRight;
	panorama::CImagePanel *m_pEventLogoImage;

	panorama::CPanel2D *m_pItemDropRoot;
//	panorama::CPanelPtr< CItemImagePanel > m_pItemDropItemImage;
	panorama::CLabel *m_pItemDropItemName;
	panorama::CLabel *m_pItemDropTotalRound;
	panorama::CLabel *m_pItemDropTotalMatch;

	// viewers
	panorama::CPanel2D *m_pMatchViewerPanel;
	panorama::CLabel *m_pMatchViewerText;

	CUtlVectorFixed<CSpecPlayerPanelData, MAX_AVATAR_PANELS> m_aPlayerPanels;
	bool				m_bValidPlayerPanels;
	bool				m_bHideMiniScoreboard;

private:
	static CCSGO_HudSpectator *s_pSpecPanels;

	void	InitPlayerBGPanels( void );
	void	UpdatePlayerPanels( void );
	void	HandleExtraData( bool bLeftSide, const PlayerSpecStatus_t* ms, int nSlotIdx );
	void	InvokePlayerSlotUpdate( sAvatarInitData &avatarData, const PlayerSpecStatus_t* msInfo, int nPlayerPanel, int nSlotIdx );
	void	UpdateAvatar( sAvatarInitData &avatarData, const PlayerSpecStatus_t* ms, CAvatarPanelData *pAvatarPanelData );
	//void	UpdatePlayerSlot( Slot:Number, Xuid : String, Flags : Number, playerNameText : String, playerHealth : Number, playerArmor : Number, itemName : String, bIsPrimarySelected : Boolean, nRoundKills : Number, iconString, nPlayerColorIndex : Number );

	bool					m_bRoundStarted;
	bool					m_bSwapPlayerNames;
	int						m_nTerroristTeamCount;
	bool					m_bForceAvatarRefresh;
	int						m_nCTTeamCount;
	PlayerSpecStatus_t		m_TerroristTeam[MAX_TEAM_SIZE];
	PlayerSpecStatus_t		m_CTTeam[MAX_TEAM_SIZE];
	bool					m_bTeamMoneyVisible;
	int						m_nCurrentRoundNumber;
	bool					m_bTimerAlertTriggered;
	bool					m_bIsBombDefused;
	bool					m_bDisplayMatchesWon;
	int						m_HLTVSpectators;
	bool					m_bIsFreezeTimeActive;
	int						m_nTourneyItemsDroppedThisRound;
	int						m_nTourneyItemsDroppedTotal;
	float					m_flLastTourneyItemPanelUpdate;

	bool					m_bRequestedMouseInput;
	bool					m_bForceRefresh;
	bool					m_bWasHidden;

	panorama::CGameInputCapture m_Capture;

	struct TournamentRewardTempList_t
	{
		int nDefindex;
		AccountID_t uiAccountID;
		bool bIsLocalPlayer;

		TournamentRewardTempList_t() :
			nDefindex( 0 ), uiAccountID( 0 ), bIsLocalPlayer( false )
		{
		}
	};
	CUtlVector<TournamentRewardTempList_t> m_TournamentRewardTempList;

	// Reference to the "HudBlur" panel in hud.xml so that we can 
	// dynamically add / remove new blur rectangles
	panorama::CPanelPtr< CCSGO_HudBlurTarget > m_pHudBlurTargetPanel;
};