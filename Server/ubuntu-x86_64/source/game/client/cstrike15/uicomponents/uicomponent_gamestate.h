//========= Copyright (C) Valve Corporation, All rights reserved. ============//
//
// Component for PlayerResource access. Panorama-only and mirrors the PlayerResourcescreen_scaleform singleton in SF.
//
//=============================================================================//

#pragma once

#include "uicomponent_common.h"
#include "panorama/csgo_panorama_script_bindings.h"
#include "matchmaking/imatchframework.h"
#include "panorama/csgo_endofmatch.h"
#include "c_cs_playerresource.h"

#if defined ( PANORAMA_ENABLE )
	DECLARE_PANORAMA_EVENT1( GameState_UpdatePlayer, int );
	DECLARE_PANORAMA_EVENT1( GameState_UpdateAllPlayers, bool );
	DECLARE_PANORAMA_EVENT0( GameState_OnLevelLoad );
	DECLARE_PANORAMA_EVENT0( GameState_OnMatchStart );
	DECLARE_PANORAMA_EVENT0( GameState_ServerSpawn );
	DECLARE_PANORAMA_EVENT5( GameState_ServerRankUpdate, int, int, int, float, int );
	DECLARE_PANORAMA_EVENT1( BackupFileNamesReceived, v8::Local<v8::Object> );
	DECLARE_PANORAMA_EVENT0( GameState_CommendPlayerQueryResponse );
	DECLARE_PANORAMA_EVENT0( GameState_RankRevealAll );
#endif


//
// Component
//
class CUiComponent_GameState : public CUiComponentGlobalInstanceHelper< CUiComponent_GameState >, CGameEventListener, CAutoGameSystemPerFrame
{
	UI_COMPONENT_DECLARE_GLOBAL_INSTANCE_ONLY( CUiComponent_GameState );

public:

	enum PlayerStatus
	{
		None,
		Dead,
		Bomb,
		Dominated,
		DominatedDead,
		Nemesis,
		NemesisDead,
		Defuser,
		SwitchTeams,
		SwitchTeamsDead,
		ScoreboardStatusMax
	};

 	virtual void PostInit() OVERRIDE;

#if defined ( PANORAMA_ENABLE )
	void GetPlayerDataJSO( const v8::FunctionCallbackInfo<v8::Value>& args );

#endif

	bool IsLatched() { return m_latchedExtraState.IsLatched(); }

	int GetPlayerIndex( char const* szXuid );

	char const * GetPlayerName( char const* szXuid );
	char const * GetPlayerNameSafe( char const* szXuid );	// (DEPRECATED, use GetPlayerName).  Escapes any html tags
	char const * GetPlayerNameWithNoHTMLEscapes( char const* szXuid ); // Does *NOT* escape html tags, use in non-html labels only.
	char const * GetPlayerXuidStringFromEntIndex( int nEntIndex );
	char const * GetPlayerXuidFromUserID( int nUserID );

	char const * GetLocalPlayerXuid();
	char const * GetPlayerTeamName( const char * szXuid );
	int GetPlayerTeamNumber( const char * szXuid );
	bool ArePlayersEnemies( const char * szXuid1, const char * szXuid2 );
	int GetCoachingTeamNumber( const char * szXuid );
	int GetAssociatedTeamNumber( const char * szXuid );
	bool IsPlayerConnected( const char * szXuid );
	bool IsPlayerAlive( const char * szXuid );
	int GetPlayerPing( char const* szXuid );
	int GetPlayerKills( char const* szXuid );
	int GetPlayerAssists( char const* szXuid );
	int GetPlayerDeaths( char const* szXuid );
	int GetPlayerMVPs( char const* szXuid );
	int GetPlayerGungameLevel( char const* szXuid );
	bool IsFakePlayer( char const* szXuid );
	int GetPlayerMoney( char const* szXuid );
	int GetPlayerScore( char const* szXuid );
	char const * GetPlayerClanTag( char const* szXuid );
	int GetPlayerActiveCoinRank( char const* szXuid );
	int GetPlayerCompetitiveRanking( char const* szXuid );
	int GetPlayerXpLevel( char const* szXuid );
	int GetPlayerCompetitiveWins( char const* szXuid );
	char const * GetPlayerColor( char const* szXuid );
	char const * GetPlayerDisplayFlairItem( char const* szXuid );
	int GetPlayerStatus( char const* szXuid );
	const char * GetPlayerModel( char const* szXuid );
	char const * GetPlayerActiveWeaponItemId( char const * szXuid );
	int GetPlayerLifetime( char const* szXuid );

	// SURVIVAL SPECIFIC
	float GetSurvivalTimeUntilNextWave( void );
	// END SURVIVAL SPECIFIC

	float GetPlayerVoiceVolume( char const * szXuid );
	void SetPlayerVoiceVolume( char const* szXuid, float val );

	bool IsDemoOrHltv();
	bool IsLocalPlayerHLTV();
	bool IsHLTVAutodirectorOn();
	void SetCasterIsCameraman( int nAccountID );
	void SetCasterIsHeard( int nAccountID );
	void SetCasterControlsXray( int nAccountID );
	void SetCasterControlsUI( int nAccountID );

	// migrated from old playerdetails_scaleform

	bool IsLocalPlayerPlayingMatch( void );
	bool AreTeamsPlayingSwitchedSides( void );
	bool AreTeamsPlayingSwitchedSidesInRound( int nRound );

 	bool IsSelectedPlayerMuted( char const* szXuid );
 	void ToggleMute( char const* szXuid );

	bool IsReportCategoryEnabledForSelectedPlayer( char const* szXuid, const char * szCategory );
	void SubmitPlayerReport( char const* szXuid, const char * szCategory );

	void SubmitServerReport( const char * szCategory );

	bool QueryServersForCommendation( char const* szXuid );

 	int GetCommendationTokensAvailable( void );
	void GetMyCommendationsJSOForUser( const v8::FunctionCallbackInfo<v8::Value>& args );
	void SubmitCommendation( char const* szXuid, const char * szCommendations );

	int GetTeamTotalPlayerCount( const char * szTeamname );
	int GetTeamLivingPlayerCount( const char * szTeamname );

	const char * GetTeamClanName( const char * szTeamname );
	const char * GetTeamLogoImagePath( const char * szTeamname );
	const char * GetTeamGungameLeaderXuid( const char * szTeamname );
	const char * GetTeamFlagImagePath( const char * szTeamname );

	const char * GetMapsInCurrentMapGroup();
	const char * GetMapsInMapGroup( const char* szMapGroup );
	const char* GetMapDisplayNameToken( const char* szMapName );

#if defined ( PANORAMA_ENABLE )
	void GetKickTargets( const v8::FunctionCallbackInfo<v8::Value>& args );
#endif 

	bool IsXuidValid( char const* szXuid );

	int GetPlayerCommendsLeader( char const* szXuid );
	int GetPlayerCommendsTeacher( char const* szXuid );
	int GetPlayerCommendsFriendly( char const* szXuid );



	//////////////////// match info

#if defined ( PANORAMA_ENABLE )
	void GetTimeDataJSO( const v8::FunctionCallbackInfo<v8::Value>& args );
	void GetScoreDataJSO( const v8::FunctionCallbackInfo<v8::Value>& args );
	void GetMatchEndWinDataJSO( const v8::FunctionCallbackInfo<v8::Value>& args );
	
#endif

	int GetRoundsRemaining();
	char const * GetServerName();
	char const * GetMapName();
	char const * GetTournamentEventStage();
	char const * GetMapBSPName();
	char const * GetGameModeName( bool bUseSkirmishName );
	char const * GetGameModeImagePath();
	char const * GetGameModeInternalName( bool bUseSkirmishName ); // e.g. Casual => 'casual', Wingman => 'scrimcomp2v2', Flying Scoutsman => 'flyingscoutsman', etc.

	int GetViewerCount();

	bool HasHalfTime();
	bool IsQueuedMatchmaking();
	bool IsEndMatchMapVoteEnabled();

	// CGameEventListener methods
	virtual void FireGameEvent( IGameEvent *event ) OVERRIDE;

	// CAutoGameSystemPerFrame methods
	virtual void Update( float frametime );
	virtual void LevelInitPostEntity();
	virtual void LevelShutdownPreEntity();

	// keeps track of all CCSGO_GameTimeLabel and updates their DialogVariables
	// we do this extra work to provide panorama panels with accurate game clocks
	CUtlVector< panorama::CPanelPtr<CCSGO_GameTimeLabel> > m_vecGameTimeLabels;



	void AllRanksReveal( int numSecondsTillShutdown );

	int m_flTimeWhenShudownHappens;
	int m_iFreeForAllModeWinnerIndex;
	int m_flTimeWhenClientShouldDropFromGotv;

	void Latch();

private:

	wchar_t m_szHostName[ MAX_PLAYER_NAME_LENGTH ];
	bool m_bServerRankRevealAll;

	char const * m_szLocalId;

	// At end of match we freeze the game state to whatever it was
	// (TODO: Make sure any extra state we need to add to this gets added, e.g. rank message data?)
	C_CS_PlayerResourceLatchableGameState m_latchedGameStateCS;
	C_PlayerResourceLatchableGameState m_latchedGameState;
	struct CExtraLatchedState
	{
		CExtraLatchedState();

		bool IsLatched() { return m_bLatched; }
		void Latch();
		void Unlatch();

		// extra latched state
		bool IsFakePlayer( int idxPlayer );
		bool IsHLTV( int idxPlayer );
		int GetPlayerMoney( int idxPlayer );
		const char* GetDecoratedPlayerName( int idxPlayer, bool bHTMLSafe ); // flags hardcoded
		uint64 GetXuid( int idxPlayer );

	private:
		// put anything else you need to latch at end of match here
		bool m_bLatched;

		bool m_bIsFakePlayer[MAX_PLAYERS + 1];
		CUtlString m_DecoratedPlayerNames[MAX_PLAYERS + 1];
		CUtlString m_DecoratedHTMLPlayerNames[MAX_PLAYERS + 1];
		int m_nAccount[MAX_PLAYERS + 1];
		uint64 m_xuids[MAX_PLAYERS + 1];

	} m_latchedExtraState;


	void Unlatch();

	C_PlayerResourceLatchableGameState* GetGameState() { return IsLatched() ? &m_latchedGameState : GetCSResources(); }
	C_CS_PlayerResourceLatchableGameState* GetCSState() { return IsLatched() ? &m_latchedGameStateCS : GetCSResources(); }
	CExtraLatchedState* GetExtraState() { return &m_latchedExtraState; }

	XUID GetPlayerXuidFromEntIndex( int idxPlayer );
};

