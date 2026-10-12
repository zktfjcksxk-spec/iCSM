//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//
#pragma once 

#include "hud.h"
#include "hud_element_helper.h"

#include "panorama/csgo_panorama.h"
#include "panorama/hud/csgo_hud.h"	// TODO included to get CPanoramaHudElement - Move CPanoramaHudElement to its own file ?
#include "panorama/input/iuiinput.h"
#include "panorama/csgo_avatarimage.h"

#include "cs_gamerules.h"
#include "c_cs_player.h"

#if !defined( _X360 )
#include "xbox/xboxstubs.h"
#endif

#define MAX_AVATAR_PANELS 10
#define MAX_PAVATAR_PANELS 24

struct TeamCounterMiniStatus_t 
{
	XUID		nXUID;
	int			nEntIdx;
	int			nPlayerIdx;
	int			nGunGameLevel;
	int			nHealth;
	int			nArmor;
	int			nTeammateColor;
	bool		bIsCT;
	bool		bLocalPlayer;
	bool		bDead;
	bool		bDominated;
	bool		bDominating;
	bool		bSpeaking;
	bool		bPlayerBot;		// indicates that the player took over this bot
	bool		bSpectated;	
	bool		bTeamLeader;	
	float		flLastRefresh;
	
	int			nGGProgressiveRank;	// save this character's rank
	int			nPoints;
	int			nTeam;

	TeamCounterMiniStatus_t() : 
		nXUID( INVALID_XUID ), nPlayerIdx(-1), nGunGameLevel(-1), bIsCT(false), bLocalPlayer(false), bDead(false), bDominated(false), 
		bDominating( false ), bTeamLeader( false ), bSpeaking( false ), bPlayerBot( false ), bSpectated( false ), nGGProgressiveRank( -1 ), nPoints( 0 ), nTeam( 0 ), nTeammateColor( -1 ), flLastRefresh( -1 )
	{
	}
	
	TeamCounterMiniStatus_t( const TeamCounterMiniStatus_t &copy )
	{
		memcpy( this, &copy, sizeof(TeamCounterMiniStatus_t) );
	}

	TeamCounterMiniStatus_t& operator=( const TeamCounterMiniStatus_t &rhs )
	{
		memcpy( this, &rhs, sizeof(TeamCounterMiniStatus_t) );
		return *this;
	}

	void Reset()
	{
		memset( this, 0, sizeof(TeamCounterMiniStatus_t) );
		nXUID = INVALID_XUID;
		nPlayerIdx = -1;
		nEntIdx = 0;
		nGGProgressiveRank = -1;
		flLastRefresh = -1;
	}

	// Returns true if any of these fields has changed since last update
	bool Update( XUID _Xuid, int _EntIdx, int _PlayerIdx, int _GunGameLevel, int _nHealth, int _nArmor, bool _IsCT, bool _LocalPlayer, bool _Dead, bool _Dominated, bool _Dominating, bool _bTeamLeader, bool _Speaking, bool _PlayerBot, bool _Spectated, int _Points, int _Team, int _TeammateColor, float _CurtimeRefresh )
	{
		bool bDiff = ( _Xuid != nXUID ) ||
			( _EntIdx != nEntIdx ) ||
			( _PlayerIdx != nPlayerIdx ) ||
			( _GunGameLevel != nGunGameLevel ) ||
			( _nHealth != nHealth ) ||
			( _nArmor != nArmor ) ||
			( _GunGameLevel != nGunGameLevel ) ||
			( _IsCT			^ bIsCT ) ||
			( _LocalPlayer	^ bLocalPlayer ) ||
			( _Dead			^ bDead ) ||
			( _Dominated	^ bDominated ) ||
			( _Dominating	^ bDominating ) ||
			( _bTeamLeader	^ bTeamLeader ) ||
			( _Speaking		^ bSpeaking ) ||
			( _PlayerBot	^ bPlayerBot ) ||
			( _Spectated	^ bSpectated ) ||
			( _Points != nPoints ) ||
			( _Team != nTeam ) ||
			( _TeammateColor != nTeammateColor ) ||
			( _CurtimeRefresh > flLastRefresh + 3.0f );

		nXUID			= _Xuid;
		nEntIdx			= _EntIdx;
		nPlayerIdx		= _PlayerIdx;
		nGunGameLevel	= _GunGameLevel;
		nHealth			= _nHealth;
		nArmor			= _nArmor;
		bIsCT			= _IsCT;
		bLocalPlayer	= _LocalPlayer;
		bDead			= _Dead;
		bDominated		= _Dominated;
		bDominating		= _Dominating;
		bTeamLeader		= _bTeamLeader;
		bSpeaking		= _Speaking;
		bPlayerBot		= _PlayerBot;
		bSpectated		= _Spectated;
		nPoints			= _Points;
		nTeam			= _Team;
		nTeammateColor  = _TeammateColor;
		if ( bDiff )
			flLastRefresh	= _CurtimeRefresh;

		return bDiff;
	}
};

struct sAvatarInitData
{
	int m_nSlot;
	XUID m_XUID;
	int m_nFlags;
	const char *m_pszPlayerName;
	int m_nHealth;
	int m_nArmor;
	int m_nLevel;
	char m_szWeaponURL[ 128 ];
	int m_nTeammateColor;
	bool m_bShowLetter;
};

class CCSGO_AvatarHealthBar : public panorama::CPanel2D
{
	DECLARE_PANEL2D( CCSGO_AvatarHealthBar, panorama::CPanel2D );

public:
	CCSGO_AvatarHealthBar( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CCSGO_AvatarHealthBar();

	void SetMin( float flMin ) { m_flMin = flMin; InvalidateSizeAndPosition(); }
	void SetMax( float flMax ) { m_flMax = flMax; InvalidateSizeAndPosition(); }
	void SetValue( float flValue ) { if ( !CloseEnough( m_flCur, flValue ) ) { m_flCur = flValue; AccessStyleDirty()->SetWidth( panorama::CUILength( m_flCur, panorama::CUILength::k_EUILengthPercent ) ); /*m_flCur = flValue; InvalidateSizeAndPosition();*/ } }

	float GetMin() const { return m_flMin; }
	float GetMax() const { return m_flMax; }
	float GetValue() const { return m_flCur; }

protected:
	virtual void OnLayoutTraverse( float flFinalWidth, float flFinalHeight ) OVERRIDE;
	virtual bool BSetProperty( panorama::CPanoramaSymbol symName, const char *pchValue ) OVERRIDE;

private:
	float m_flMin;
	float m_flMax;
	float m_flCur;
};

class CAvatarPanelData
{
public:
	CAvatarPanelData();
	~CAvatarPanelData();

	void Init( panorama::CPanel2D *pParent, int idx, bool bSmall );

	bool SetChildVisible( panorama::CPanel2D *pChild, bool bVisible );

	void SetBGForTeam( bool bCT, bool bSmall );
	void SetOutlineVisible( bool bOutlineVisible );
	void SetBorderType( panorama::CPanoramaSymbol symName );
	void SetSkullType( panorama::CPanoramaSymbol symName );
	void SetBotType( panorama::CPanoramaSymbol symName );
	void SetHealthBarType( panorama::CPanoramaSymbol symName );

	XUID m_savedXUID;
	bool m_bDead;
	bool m_bWasPlayerBot;

	bool m_bInitialized;

	// parent
	panorama::CPanel2D *m_pPanel;

	// visible/invisible panels - all other panels parented under these.
	panorama::CPanel2D *m_pVisible;
	panorama::CPanel2D *m_pInvisible;

	// children
	//panorama::CProgressBar *m_pHealth;
	CCSGO_AvatarHealthBar *m_pHealth;

	panorama::CLabel *m_pPlayerLetter;
	// the player number used as a hotkey to select this player
	panorama::CLabel *m_pPlayerNumber;

	panorama::CPanel2D *m_pSound;
	panorama::CImagePanel *m_pDominated;
	panorama::CImagePanel *m_pNemesis;

	panorama::CImagePanel *m_pPlayerColor;

	panorama::CImagePanel *m_pSkull;

	panorama::CImagePanel *m_pBot;

	panorama::CPanel2D *m_pAvatarBG;
	CCSGO_AvatarImage *m_pDynamicAvatar;

	panorama::CPanel2D *m_pArsenalProgress;	// parent progress panel

	panorama::CLabel *m_pArsenalProgressText;
	panorama::CPanel2D *m_pArsenalProgressWeapon;
	panorama::CImagePanel *m_pArsenalProgressWeaponIcon;
};

class CCSGO_HudTeamCounter : public panorama::CPanel2D, public CPanoramaHudElement
{
	DECLARE_PANEL2D( CCSGO_HudTeamCounter, panorama::CPanel2D );

public:

	explicit CCSGO_HudTeamCounter( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CCSGO_HudTeamCounter();

	// These overload the CHudElement class
	virtual void	ProcessInput( void ) OVERRIDE;
	virtual void	LevelInit( void ) OVERRIDE;
	virtual void	LevelShutdown( void ) OVERRIDE;
	virtual bool 	ShouldDraw( void ) OVERRIDE;
	virtual void	SetActive( bool bActive ) OVERRIDE;
	//virtual void	Reset( void ) OVERRIDE;

	// CGameEventListener methods
	virtual void FireGameEvent( IGameEvent *event );

 	int FindNextObserverTargetIndex( bool reverse );
 	int GetSpectatorTargetFromSlot( int idx );

 	int GetPlayerEntIndexInSlot( int nIndex );
	int GetPlayerSlotIndex( int playerEntIndex );
 	int GetPlayerSlotIndex( int playerEntIndex ) const; // version with no side effects
 	int GetPlayerSlotIndexForDisplay( int playerEntIndex ) const; // When displayed to users, slot is one higher and slot 10 is displayed as 0 for legacy reasons. 
	TeamCounterMiniStatus_t GetTerroristTeamData( int nSlot ) { return m_TerroristTeam[nSlot]; }
	TeamCounterMiniStatus_t GetCTTeamData( int nSlot ) { return m_CTTeam[nSlot]; }
	int GetTerroristTeamCount( void ) { return m_nTerroristTeamCount; }
	int GetCTTeamData( void ) { return m_nCTTeamCount; }

	const char *GetPlayerColorLetter( int nSymbolStyle, int nSlot );

protected:
// 	void	LockSlot( bool wantItLocked, bool& currentlyLocked );

	// update elements affected by position
	void	UpdatePosition( void );

	// update game clock
	void	UpdateTimer( void );
	void	SetTimerVisibility( bool bVisible, bool bHasBomb = true );
	void	SetTimerRedColor( bool bRed );

	// update team scores and balance of power indicator
	void	UpdateScore( void );

	// update current player's team selection graphic
	void	UpdateTeamSelection( void );

	// update mini-scoreboard (team list + status bar)
	void	UpdateMiniScoreboard( void );

 	void	ShowPanel( const bool bShow );

	// resets the progressive leader HUD with the local player and weapon
	// and resets m_nLeaderWeaponRank
 	void	ResetLeader();

	void	InvokeAvatarSlotUpdate( sAvatarInitData &avatarData, const TeamCounterMiniStatus_t* msInfo, int slotNumber );

 	static int GGProgSortFunction( TeamCounterMiniStatus_t* const* entry1, TeamCounterMiniStatus_t* const* entry2 );
 	static int DMSortFunction( TeamCounterMiniStatus_t* const* entry1, TeamCounterMiniStatus_t* const* entry2 );

	enum BALANCE_OF_POWER
	{
		BOP_EVEN = 0,			// teams even
		BOP_CT = TEAM_CT,		// CT winning
		BOP_T = TEAM_TERRORIST	// T winning 
	};

	enum VIEW_MODE
	{
		VIEW_MODE_NORMAL = 0,
		VIEW_MODE_GUN_GAME_PROGRESSIVE,	// Gun game progressive view mode
		VIEW_MODE_GUN_GAME_BOMB,	// Gun game bomb view mode
		VIEW_MODE_NUM,
	};

	/*	
	SFVALUE				m_ProgressiveLeaderHandle;
	*/
	panorama::CPanel2D *m_pTeamCounterPanel;
	panorama::CPanel2D *m_pBannerPanel;

	// time
	panorama::CPanel2D *m_pGameTimeContainer;
	panorama::CLabel *m_pTime;
	panorama::CPanel2D *m_pBombContainer;
	panorama::CImagePanel *m_pBombPlanted;
	panorama::CImagePanel *m_pBombPlantedLines;
	panorama::CImagePanel *m_pBombDefused;

	// score
	panorama::CPanel2D *m_pGameScoreContainer;
	panorama::CLabel *m_pCTScore;
	panorama::CLabel *m_pTScore;

	panorama::CPanel2D *m_pAliveBGCT;
	panorama::CLabel *m_pAliveCountCT;
	panorama::CLabel *m_pAliveTextCT;
	panorama::CImagePanel *m_pAliveSkullCT;
	panorama::CPanel2D *m_pAliveBGT;
	panorama::CLabel *m_pAliveCountT;
	panorama::CLabel *m_pAliveTextT;
	panorama::CImagePanel *m_pAliveSkullT;

	panorama::CPanel2D *m_pJoinPanel;
	panorama::CPanel2D *m_pJoinPanelBot;
	panorama::CPanel2D *m_pJoinPanelCT;
	panorama::CPanel2D *m_pJoinPanelT;
	panorama::CLabel *m_pJoinTextBot;

	panorama::CPanel2D *m_pTeamAll;
	panorama::CPanel2D *m_pTeamLargeCT;
	panorama::CPanel2D *m_pTeamSmallCTContainer;
	panorama::CPanel2D *m_pTeamSmallCTRow1;
	panorama::CPanel2D *m_pTeamSmallCTRow2;
	panorama::CPanel2D *m_pTeamLargeT;
	panorama::CPanel2D *m_pTeamSmallTContainer;
	panorama::CPanel2D *m_pTeamSmallTRow1;
	panorama::CPanel2D *m_pTeamSmallTRow2;

	CUtlVectorFixed<CAvatarPanelData, MAX_AVATAR_PANELS> m_aAvatarPanels;
	CUtlVectorFixed<CAvatarPanelData, MAX_PAVATAR_PANELS> m_aPAvatarPanels;

	bool				m_bTimerAlertTriggered;		// True if the timer's alert state has been triggered
	bool				m_bRoundStarted;			// True if a round is currently in progress
	bool				m_bTimerHidden;				// True if the timer is hidden from view
	int					m_nTScoreLastUpdate;		// Terrorist score 
	int					m_nCTScoreLastUpdate;		// Counter-Terrorist score 
	int					m_nTeamSelectionLastUpdate;	// Team selection 
	int					m_nLeaderWeaponRank;		// The weapon rank of the current leader
	VIEW_MODE			m_Mode;						// Tracks the current view mode

	bool				m_bRoundIsEnding;			// True if the round ended event was received
	bool				m_bIsBombDefused;

	bool				m_bColorTabsInitialized;

	enum PLAYER_TEAM_COUNT
	{
		MAX_TEAM_SIZE = 16
	};
	enum GG_PROG_PLAYER_COUNT
	{
		MAX_GGPROG_PLAYERS = 32
	};

	// track the team info we've already pushed to the mini-scoreboard
	int					m_nTerroristTeamCount;
	int					m_nCTTeamCount;
	TeamCounterMiniStatus_t			m_TerroristTeam[ MAX_TEAM_SIZE ];
	TeamCounterMiniStatus_t			m_CTTeam[ MAX_TEAM_SIZE ];

	// for GG Prog only: Track the status of the players, regardless of team
	int					m_nPreviousGGProgressiveTotalPlayers;
	TeamCounterMiniStatus_t			m_GGProgressivePlayers[ MAX_GGPROG_PLAYERS ];
	CUtlVector<TeamCounterMiniStatus_t*> m_ggSortedList;

	CountdownTimer		m_GGProgRankingTimer;

	// Signals that avatar images should be reloaded (needed after a render device reset)
	bool				m_bForceAvatarRefresh;

	// Sets the view mode, hiding or showing elements and changing
	// update behavior
 	void	SetViewMode( VIEW_MODE mode );

 	const TeamCounterMiniStatus_t* GetPlayerStatus( int index );
 	const TeamCounterMiniStatus_t* GetPlayerStatus( int index ) const; // version with no side effects

private:

	void BeginTimerAlert();
	void BeginTimerNormal();
	void DisablePlayerIcon( bool bCT, int nSlot );
	void DisableRemainingPlayerIcons( bool bCT, int nStartSlot );
	void FadeOutSelectedTeam();
	void HideDisplayTeamPanels();
	void HideTimer();
	void InitAvatarSnippetSmall( panorama::CPanel2D *pParent, int idx, bool bIsCT );
	void InitAvatarSnippetLarge( panorama::CPanel2D *pParent, int idx, bool bIsCT, bool bIsGGProgressive, bool bScaleSmaller );
	void InitializePlayerColors();
	bool IsPlayerCountVisible();
	bool IsPlayerCountVisibleForCT();
	bool IsPlayerCountVisibleForT();
	bool IsPlayerOutlineVisible( CAvatarPanelData *pPanelData );
	void PurgeAvatars();
	void SetBombDefused();
	void SetModeGunGameBomb();
	void SetModeGunGameBombTen();
	void SetModeGunGameProgressive();
	void SetModeNormal();
	void SetModeNormalTen();
	void SetNumPlayersAlive( int nPlayersAlive_CT, int nPlayersAlive_T );
	void SetPlayerOutlineVisible( CAvatarPanelData *pPanelData, bool bVisible );
	void SpawnDisplaySelectedTeam( int team );
	void TakeOverBot( const char *szBotName );
	void UpdateAvatarSlot( sAvatarInitData &avatarData );
	void UpdateLeaderWeaponVisibility( int nSlot, bool bShowWeapon );
	void UpdateNumberCount();
	void UpdatePlantedBombState( float flDetProgress );
	void UpdateTeamSelection( int team );
	void UpdateTotalProgressivePlayers( int nNewCount, bool bShowTimer );

	enum TimerState_t
	{
		TIMER_STATE_UNSET = -1,
		TIMER_STATE_WARMUP = -2,
		TIMER_STATE_FREEZE = -1,
	};

	float m_flPlayingTeamFadeoutTime;
	float m_flLastSpecListUpdate;

	int m_nHudBGAlpha;
	int m_nLastGGPlayerCount;
	int m_nMaxPlayers;
	int m_nPlayersAlive_CT;
	int m_nPlayersAlive_T;
	int m_nLastTimeSet;

	bool m_bIsGunGame;
	bool m_bIsShowingTimer;
	bool m_bPositionIsBottom;
	bool m_bPositionInitialised;
	bool m_bShowOnlyPlayerCount;
};
