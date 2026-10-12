//========= Copyright © Valve Corporation, All rights reserved. ============//
//
//=============================================================================//

#ifndef CS_GAMERULES_SURVIVAL_H
#define CS_GAMERULES_SURVIVAL_H

#ifdef _WIN32
#pragma once
#endif

#include "mathlib/aabb.h"
#include "cs_survival_funfact_types.h"
#ifdef GAME_DLL
#include "maprules.h"
#endif
#include "tier1/utlpointers.h"

#ifdef CLIENT_DLL
#ifndef CCSPlayer
// This is normally defined in c_cs_player.h but we don't want to include that here,
// so to make our forward declarations work properly, we #define it to the correct value temporarily.
#define SURVIVAL_DEFINED_CCSPLAYER
#define CCSPlayer C_CSPlayer
#endif
#endif

// size of networked array of hexes used for spawn selection
constexpr int SURVIVAL_SPAWN_TILE_WIDTH = 14;
constexpr int SURVIVAL_SPAWN_TILE_HEIGHT = 16;
constexpr int SURVIVAL_SPAWN_TILE_NUM = SURVIVAL_SPAWN_TILE_WIDTH * SURVIVAL_SPAWN_TILE_HEIGHT;

// dimensions of tablet in 'offset hex' coordinates
constexpr int WORLD_HEX_WIDTH = 6;
constexpr int WORLD_HEX_HEIGHT = 7;
constexpr int WORLD_HEX_NUM = WORLD_HEX_WIDTH * WORLD_HEX_HEIGHT;

// We don't necessarily use all of these, but since these are going to go in a network variable I wanted
// to account for some likely future design expansion.  After ship, always add new values to the end of
// this enum since old demos will contain these values!
enum ESurvivalSpawnTileState : uint8
{
	// Available tiles
	// Keep this 0 since it's the most common, so we don't network the whole array
	kSurvivalSpawn_Available = 0,	// Can be chosen by any player.

	// Unavailable tiles
	kSurvivalSpawn_MapBlocked = 1,	// Can't be chosen, map doesn't allow this space
	kSurvivalSpawn_GameBlocked = 2,	// Can't be chosen, game has blocked this space

	// Used tiles
	kSurvivalSpawn_Occupied = 3,	// Chosen by a player, but that player can change their decision
	kSurvivalSpawn_TempLocked = 4,	// Chosen by a player, but that player will be able to change their decision
	kSurvivalSpawn_Locked = 5,		// Chosen by a player who is locked into that decision

	kSurvivalSpawn_ProximityBlocked = 6,	// Neighboring location is chosen by some player

	// If adding new states, use this value and increment it
	// You may also need to modify the bitcount for the sendprop for m_SpawnTileState.
	// (current bitcount 3 supports kSurvivalSpawn_TileStateCount <= 8)
	kSurvivalSpawn_TileStateCount = 7,

	// Local values (not networked) that modify these values.
	// These will never be in m_SpawnTileState but are useful for UI implementations.
	// They can be changed without worry.
	kSurvivalSpawn_UI_Invalid = 0x7f,	// Not initialized yet / needs to be updated
	kSurvivalSpawn_UI_LocalPlayerFlag = 0x80,	// Same as the above but used on the local player's selection.
};

DEFINE_ENUM_BITWISE_OPERATORS( ESurvivalSpawnTileState );


// Some of the randomness in the survival game rules must be decided by the server and the client
// must be informed of those decisions to make sure UI is representing the state of the server
// These enum tags allow sending variable random data to the clients
// Always add new values to the end of this enum since old demos will contain these values!
enum ESurvivalGameRuleDecision_t
{
	k_ESurvivalGameRuleDecision_Unknown = 0,
	
	k_ESurvivalGameRuleDecision_Tablet_Purchase_Pistol_DefIdx = 1,
	k_ESurvivalGameRuleDecision_Tablet_Purchase_SMG_DefIdx = 2,
	k_ESurvivalGameRuleDecision_Tablet_Purchase_Rifle_DefIdx = 3,
	k_ESurvivalGameRuleDecision_Tablet_Purchase_Sniper_DefIdx = 4,
	k_ESurvivalGameRuleDecision_Tablet_Purchase_MegaPistol_DefIdx = 5,

	// If adding new decision types, use this value and increment it
	// You may also need to modify the bitcount for the sendprop for m_SurvivalGameRuleDecisionTypes.
	// (current bitcount 3 supports k_ESurvivalGameRuleDecision_TotalCount <= 8)
	k_ESurvivalGameRuleDecision_TotalCount = 6,
};
// Size of networked array of all decisions, ok to have some extra slack
#define SURVIVAL_GAME_RULES_DECISION_TYPES_NETWORK_TOTAL_MAX	16


// FIXME: move survival spawn chopper into it's own header
#ifdef CLIENT_DLL
#define CSurvivalSpawnChopper C_SurvivalSpawnChopper
#endif

struct TabletBuyMenuEntry
{
	enum ETabletBuyMenuCategory
	{
		k_EWeapons = 0,
		k_EArmor,
		k_EUtility,
		k_ETablet,
		k_ECategoryCount
	};

	int nUniqueIndex;
	char *szLocalizedToken;
	int nPrice;
	char *szSpawnRuleGroupName;
	char *szBuyImagePath;
	int nTabletUpgradeEnum; // -1 for not an upgrade, otherwise match tablet_upgrade_type_t
	int nGameRulesDecisionTypeID; // 0 for no conditions on purchase, otherwise a decision slot type ID
	int nGameRulesDecisionValue; // value that conditional decision must be for the purchase to be available
	ETabletBuyMenuCategory nCategory;
	loadout_positions_t nLoadoutPosition;
};

#define SPAWN_CHOPPER_PASSENGER_MAX 16
class CSurvivalSpawnChopper : public CBaseAnimating
{
	DECLARE_CLASS( CSurvivalSpawnChopper, CBaseAnimating );
	DECLARE_NETWORKCLASS();

public:

#ifdef CLIENT_DLL
	virtual void OnDataChanged( DataUpdateType_t updateType );
	virtual bool ShouldRegenerateOriginFromCellBits() const { return false; }
	virtual int	DrawModel( int flags, const RenderableInstance_t &instance );
#endif

#ifndef CLIENT_DLL
	virtual void Precache( void );
	virtual void Spawn( void );
	virtual void UpdateOnRemove( void );

	void ChopperFly( void );

	void AddPassenger( CBasePlayer* pPlayer );

	virtual int UpdateTransmitState() { return SetTransmitState( FL_EDICT_ALWAYS ); }

	void StopCircling( void ) { m_bCircling = false; }

private:
	CSoundPatch	*m_pSoundLoop;

	float m_flSpawnTimeStamp;
	Vector ComputeFlightPositionForTime( float flTime, float flSpeed );
	float m_flFlightPathRotationOffset;
	bool m_bFlipX;
	bool m_bFlipY;

	bool m_bCircling;
#endif
};



#ifdef GAME_DLL

#include "br_config.h"

struct dz_queued_drone_purchase_t
{
	Vector m_vecDroneSpawnPos;
	EHANDLE m_hDestinationTablet;
	const char *m_szBuyMenuEntry;
	bool m_bFastDelivery;
	EHANDLE m_hPlayer;
	Vector m_vecPurchasePos;
	int m_nOthersInFlight;
};

#endif // GAME_DLL

class CPhysPropLootCrate;

class CCSPlayer;

typedef CUtlVectorFixedGrowableCompat<CCSPlayer*, 4> PlayerTeammateVector_t;

class CSurvivalGameRules
#ifdef GAME_DLL
	: public IBrConfigLoadingHostContainer
#endif
{
	DECLARE_CLASS_NOBASE( CSurvivalGameRules );
	//DECLARE_EMBEDDED_NETWORKVAR();
	template <typename T> friend int ServerClassInit( T * );
	template <typename T> friend int ClientClassInit( T * );
	inline void NetworkStateChanged()
	{
		// Forward the call to the entity that will send the data.
		CGameRulesProxy::NotifyNetworkStateChanged();
	}

	inline void NetworkStateChanged( void *pVar )
	{
		// Forward the call to the entity that will send the data.
		CGameRulesProxy::NotifyNetworkStateChanged();
	}

public:

	CSurvivalGameRules();
	~CSurvivalGameRules();

	enum SpawnStage_t
	{
		// these are in order of use in the spawn stage state machine, but
		// existing values are used in demos, so always add new states with new numbers
		SPAWN_STAGE_NONE = 0,		// not currently in spawn selection
		SPAWN_STAGE_SELECTION = 1,	// players are allowed to choose spawn location
		SPAWN_STAGE_LOCKED = 3,		// players are no longer allowed to choose spawn location; players who haven't picked don't have selections  yet
		SPAWN_STAGE_ALL_READY = 2,	// all players have selected

		SPAWN_STAGE_COUNT = 4
	};

	AABB_t GetPlayAreaBounds( void );

	bool IsPlayingSoloMode( void );
	bool IsPlayingTeamMode( void );

#ifdef CLIENT_DLL
	void CalcCustomBRView( CBasePlayer *pPlayer, Vector &eyeOrigin, QAngle &eyeAngles, float &zNear, float &zFar, float &fov );
	void OnDataChanged( DataUpdateType_t updateType );
	Color GetTeamColor( int iTeam );
	Color GetTeamColorByUserID( int iUserID );

	IMaterial *GetGameRuleScreenOverlayOverride( void );
#endif

	void OnWarmupStart( void );
	void OnRoundRestart( void );
	void OnLevelInitPreEntity( void );
	void OnLevelInitPostEntity( void );
	void OnFreezePeriodExpired( void );
	void OnEachTickThink( void );

	int GetSurvivalGameRuleDecisionValue( ESurvivalGameRuleDecision_t eType, int nDefault = 0 ) const;
	void GetPlayerTeammates( CCSPlayer* pPlayer, CUtlVector<CCSPlayer*> &teammates, bool bAliveTeammateOnly = false );

#ifdef GAME_DLL
	CGameSurvivalLogic *GetGameSurvivalLogicEntity( void );
	void FindGameSurvivalLogicEntity( void );
	EHANDLE m_gameSurvivalLogicEntity;

	void SetGameSurvivalLogicEntity( CBaseEntity *pPoint ) { m_gameSurvivalLogicEntity = pPoint; }

	// spawn tile setup
	void ResetPlayerSpawnData();
	void ReadMapMask( const char* szFileName, ESurvivalSpawnTileState( &tileState )[SURVIVAL_SPAWN_TILE_NUM] );


	void ApplyHighlightColorToBaseAnimatingViaClassname( CBaseAnimating *pEntAnim );
	void DoPlayerExitWarmupTransition( float flDuration );

	void SpawnSurvivalPlayer( CCSPlayer *pPlayer );
	void EquipSurvivalPlayer( CBasePlayer *pPlayer );
	void AssignTeam( CCSPlayer* pPlayer, bool bForceNewTeam );
	void SetTeam( CCSPlayer* pPlayer, int nTeam );
	int GetTeamCount( int nTeam );
	void ShuffleTeams();

	void SpawnItemOnEvent( const Vector &vecPosition, const QAngle& angAngle, const char *pszEvent, CBasePlayer *pPlayer = NULL, CUtlVector< CBaseEntity * > *pOutputEnts = NULL );
	void SpawnItemFromContent( const Vector &vecPosition, const QAngle& angAngle, const CBrConfig::Content_t *pContent, CBasePlayer *pPlayer = NULL, CUtlVector< CBaseEntity * > *pOutputEnts = NULL );
	void SpawnItemFromLootList( const Vector &vecPosition, const QAngle& angAngle, const CBrConfig::LootList_t *pLootList, CBasePlayer *pPlayer = NULL, CUtlVector< CBaseEntity * > *pOutputEnts = NULL, CPhysPropLootCrate *pSrcCrate = NULL );
	CBaseEntity *SpawnItem( const Vector &vecPosition, const QAngle& angAngle, const char *pszEntityName, int nAmmo, CBasePlayer *pPlayer = NULL );

	void ResetDangerZone( void );

	bool TabletPurchase( CBasePlayer *pPlayer, const char *szCmd );

	void InitPlayerSpawnLocations( void );

	void SetEntIndexOfKilledPlayerCheckingWinConditions( int nEntIndex ) { m_nEntIndexOfKilledPlayerCheckingWinConditions = nEntIndex; }
	bool CheckWinConditions( void );

	void InitBounds( void );
	bool m_bBoundsInitialized;

	void SpawnInitialItems();

	virtual void OnConfigRandomDecisionMade( int nType, int nValue ) OVERRIDE;
	const CBrConfig *GetConfig() const { return m_pBrConfig; }

	void DispatchParadropChopperTo( Vector vecDestinationDropPos, CBasePlayer *pCallingPlayer, bool bCalledByPlayer, float flFlightTime );
	bool BFirstParadropParachutingDown() { bool b = m_bSurvivalEventFired_FirstParadropIncoming; m_bSurvivalEventFired_FirstParadropIncoming = true; return !b; }

	void AddToSpawnChopper( CBasePlayer *pPlayer );

	bool PlayerSelectHex( int playerEntIndex, int hexID );
	bool CheckAllPlayersSelectSpawnHex();
	void ForceSelectAllSpawnHexes( bool bBotsOnly = false );
	void LockSpawnHexChoices();
	void SpawnAllPlayers();
	void UpdateSpawnStage();
	void NotifySurvivalSquadPartners();

	void LoadoutPlayer( CBasePlayer *pPlayer );

	bool IsUseEntBehindClosedSecurityDoor( CBaseEntity *pUseEnt, CBasePlayer *pPlayer );

	void OnPlayerKilled( CBasePlayer *pVictim, CBasePlayer *pAttacker );

	void GameStartSpawnAllItems( void );

	int GetTotalNumSurvivalTeams( void ) { return m_nTotalNumSurvivalTeams; }

#endif // GAME_DLL

	float GetSurvivalStartTime( void ) { return m_flSurvivalStartTime; }

	void GetWorldWidthAndHeight( float &flWidth, float &flHeight, Vector &vecWorldCenter );
	float GetTabletHexSize() { return m_flTabletHexSize; }

	CBaseEntity *GetNearestC4Target( const Vector &vecPosition ) const;
	bool IsInRangeOfNearestC4Target( const Vector &vecPosition ) const;

	int GetNumSpawnHexHorizontal() const;
	int GetNumSpawnHexVertical() const;
	int GetNumSpawnHex() const;
	float GetSpawnHexWidth() const;
	float GetSpawnHexHeight() const;
	Vector Get2DHexPosition( int tileIdx ) const;
	Vector Get2DHexCenter( int tileIdx ) const;
	int Get2DHexNeighborTile( int tileIdx, int iNeighbor ) const;
	bool CanSelectHex( int hexID );
	Vector GetSpawnHexWorldPosition( int tileIdx );

	float GetSpawnSelectTimeStart() const { return m_flSpawnSelectionTimeStart; }
	float GetSpawnSelectTimeStageEnd() const { return m_flSpawnSelectionTimeEnd; }
	float GetSpawnSelectTimeEnd() const { return m_flSpawnSelectionTimeLoadout; }
	const int* GetSpawnSelectIndices() const { return m_iPlayerSpawnHexIndices.Base(); }
	const ESurvivalSpawnTileState* GetSpawnTileStates() const { return m_SpawnTileState.Base(); }
	SpawnStage_t GetSpawnStage() const { return m_spawnStage; }

	int FindGridCenterIndexClosestToWorldPos( const Vector& vecWorldPos, int *pHighResPlayerLocation = NULL );
	void FindSortedGridCenterIndicesClosestToPos( const Vector& vecWorldPos, CUtlVector<int>& vecIndicesOut );
	Vector2D GetHexCenter( int nIndex ) { return ComputeGridCenterFromGridIndex( nIndex ); }

	// Similar to CCSPlayerResource versions of these, but updated based on the players who 
	// were part of the round when it started, regardless of if they are still connected to the
	// server.
	int GetPlayerPosition( int playerEntIdx );
	uint64 GetPlayerXuid( int playerEntIdx );
	int GetPlayerTeamIndex( int playerEntIdx );
	int GetSurvivalPositionByAccountID( AccountID_t unAccountID );


	int GetTotalNumPlayers( void );

private:
	CNetworkVector( m_vecPlayAreaMins );
	CNetworkVector( m_vecPlayAreaMaxs );
	CNetworkArray( int, m_iPlayerSpawnHexIndices, MAX_PLAYERS );
	CNetworkArray( ESurvivalSpawnTileState, m_SpawnTileState, SURVIVAL_SPAWN_TILE_NUM );
	CNetworkVar( float, m_flSpawnSelectionTimeStart );
	CNetworkVar( float, m_flSpawnSelectionTimeEnd );	 // misnamed, really spawn selection current stage end time
	CNetworkVar( float, m_flSpawnSelectionTimeLoadout ); // misnamed, really spawn selection end time
	CNetworkVar( SpawnStage_t, m_spawnStage );
	
	CNetworkVar( float, m_flTabletHexOriginX );
	CNetworkVar( float, m_flTabletHexOriginY );
	CNetworkVar( float, m_flTabletHexSize );

	// This data is initialized at the beginning of the round and kept updated
	// as players die, because some players might disconnect before the round
	// is over, but other players still need to be able to refer to them.
	CNetworkArray( uint64, m_roundData_playerXuids, MAX_PLAYERS + 1 );
	CNetworkArray( int, m_roundData_playerPositions, MAX_PLAYERS + 1 );
	CNetworkArray( int, m_roundData_playerTeams, MAX_PLAYERS + 1 );

	// This array contains decisions made by the survival game rules to network to clients
	CNetworkArray( ESurvivalGameRuleDecision_t, m_SurvivalGameRuleDecisionTypes, SURVIVAL_GAME_RULES_DECISION_TYPES_NETWORK_TOTAL_MAX );
	CNetworkArray( int, m_SurvivalGameRuleDecisionValues, SURVIVAL_GAME_RULES_DECISION_TYPES_NETWORK_TOTAL_MAX );

	CNetworkVar( float, m_flSurvivalStartTime );

	float m_flLastThinkTime;

#ifdef GAME_DLL

	void ConvertSafeSpawnsToItemSpawns( void );

	void ValidateAndAdjustPlayerSpawnPosition( Vector &pos, float flGroundOffset = 0.0f );

	void StartSurvival( void );

	void InitGlobalTablet( void );
	void InitItemSpawnLocations( void );
	void SetupSpawnPoints( void );
	void PostItemSpawn( void );
	void ReloadConfigFile( void );
	CBrConfig* m_pBrConfig;

	CUtlVector<Vector> m_vecPlayerSpawnLocations;

	void ResetLocalEventVars( void );
	bool m_bWasThereEverMoreThanOnePlayerAlive;
	bool m_bSurvivalEventFired_FadeEveryoneOutFromMapSelection;
	bool m_bSurvivalEventFired_TimeForSmokeBeacons;
	bool m_bSurvivalEventFired_FirstParadropIncoming;
	uint m_nLastKnownPlayersAliveCount;
	float m_flLastWinConditionDetectedTime;
	int m_nWinConditionStageProgress;
	int m_nEntIndexOfRunnerUpPlayer;
	int m_nEntIndexOfKilledPlayerCheckingWinConditions;
	
	void AutoDropParadrops( void );
	float m_flTimeOfLastParadrop;
	struct ParadropSettings_t
	{
		ParadropSettings_t() { V_memset( this, 0, sizeof( *this ) ); }
		float m_flTimeToDispatch;
		float m_flChopperFlightTime;
		Vector m_vecLocation;
	};
	CUtlVector< ParadropSettings_t > m_arrParadropSettings;

	void UpdateTablets( void );

	bool m_bSurvivalEventFired_PlayedWinnerSurrenderAnim;
	
	EHANDLE m_hWinnerPlayer;

	CUtlVector<EHANDLE> m_vecLoadedOutPlayers;

	Vector ComputeDroneSpawnPosFromDeliveryPos( Vector vecDeliveryPos );
	void DispatchQueuedDrones( void );
	float m_flLastDroneSpawnTime;
	CUtlVector<dz_queued_drone_purchase_t> m_vecQueuedDronePurchases;

	EHANDLE m_hSpawnChopper;

	// Data for assigning players to teams
	void InitializeTeamsFromMatchmakingReservation();
	int m_nTotalNumSurvivalTeams; // not all these might be in the game, due to disconnects/reconnects during drop-in/out
	int m_nPlayersOnNextTeam;
	CUtlMap< uint32, int, int, CDefLess<uint32> > m_mapAccountIdToTeam;

	uint m_nLastKnownTeamsRemainingCount;

	void ShowDebugOverlays( void );

	// in addition we send stats to players who die, and remember those stats for
	// potential reporting to the GC
	CSurvivalFunFacts m_survivalStats[MAX_PLAYERS + 1];
#endif // GAME_DLL

#ifdef CLIENT_DLL
public:
	void HandleMsgSurvivalStats( const CCSUsrMsg_SurvivalStats& msg );
	const CCSUsrMsg_SurvivalStats* GetLocalPlayerStatMsg();
	const CCSUsrMsg_SurvivalStats* GetLatestStatMsg();
	int GetPlacementFromStatMsgs( uint64 xuid, bool bIncludeTeam = false, int* pTeamNumber = nullptr, bool* bCanImprove = nullptr);

private:
	CUserMessageBinder m_UMCMsgSSUI;
	CUserMessageBinder m_UMCMsgSurvivalStats;

	// current stats for this game
	UtlOwnedPtr< CCSUsrMsg_SurvivalStats > m_localSurvivalStatsMsg;
	UtlOwnedPtr< CCSUsrMsg_SurvivalStats > m_latestSurvivalStatsMsg;
#endif // CLIENT_DLL

	Vector2D ComputeGridCenterFromGridIndex( int nIndex );
};

#ifdef CLIENT_DLL
EXTERN_RECV_TABLE( DT_SurvivalGameRules );
#else
EXTERN_SEND_TABLE( DT_SurvivalGameRules );
#endif

#ifdef SURVIVAL_DEFINED_CCSPLAYER
#undef CCSPlayer
#endif

#ifdef DEVELOPMENT_ONLY
#define DZ_ConsoleMsg ConColorMsg( Color( 255, 170, 0, 255 ), "SURVIVAL: " ); Msg
#else
#define DZ_ConsoleMsg( ... ) ((void)0)
#endif


#endif // CS_GAMERULES_SURVIVAL_H