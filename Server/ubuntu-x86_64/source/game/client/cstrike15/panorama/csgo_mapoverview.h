//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: mapoverview - heavily derived from radar (csgo_hudradar.h)
//=============================================================================//
#pragma once

#include "hud.h"
#include "hud_element_helper.h"
#include "panorama/csgo_panorama.h"
#include "panorama/hud/csgo_hud.h"
#include "c_cs_hostage.h"

#define MAX_LOCATION_TEXT_LENGTH 100

#define MAX_GRENADES 30
#define MAX_SURVIVAL_PARADROPS 10

//-----------------------------------------------------------------------------
// Purpose: CSGO map overview
//-----------------------------------------------------------------------------
class CCSGO_MapOverview : public panorama::CPanel2D, public CPanoramaHudElement
{
	DECLARE_PANEL2D( CCSGO_MapOverview, panorama::CPanel2D );

public:
	CCSGO_MapOverview(panorama::CPanel2D *pParent, const char *pchID);
	virtual ~CCSGO_MapOverview();

	// CPanel2D overload
	virtual bool OnKeyDown( const panorama::KeyData_t &key ) OVERRIDE;

	// These overload the CHudElement class
	virtual void Init( void );
	virtual void LevelInit( void );
	virtual void LevelShutdown( void );
	virtual void ProcessInput( void );
	virtual void SetActive( bool bActive );
	virtual bool ShouldDraw( void );
	virtual void Reset( void )
	{
		SetActive( false );
	}

	virtual void FireGameEvent(IGameEvent *pEvent) OVERRIDE;

	// each enum represents an icon that this class is managing
	enum PLAYER_ICON_INDICES
	{
		PI_GRENADE_HE,
		PI_GRENADE_FLASH,
		PI_GRENADE_SMOKE,
		PI_GRENADE_MOLOTOV,
		PI_GRENADE_DECOY,
		PI_SURVIVAL_PARADROP,

		PI_DEFUSER,
		PI_ABOVE,
		PI_BELOW,
		PI_FLASHED,
		PI_PLAYER_NUMBER,
		PI_PLAYER_NAME,

		PI_FIRST_ROTATED,
		PI_PLAYER_INDICATOR = PI_FIRST_ROTATED,
		PI_SPEAKING,
		PI_HOSTAGE_MOVING,

		PI_PLAYER,
		PI_PLAYER_DEAD,
		PI_PLAYER_GHOST,

		PI_ENEMY,
		PI_ENEMY_DEAD,
		PI_ENEMY_GHOST,

		PI_HOSTAGE,
		PI_HOSTAGE_DEAD,
		PI_HOSTAGE_GHOST,

		PI_DIRECTION_INDICATOR,
		PI_MUZZLE_FLASH,

		PI_LOWHEALTH,
		PI_SELECTED,

		PI_NUM_ICONS
	};

protected:

// this manages the display of the players and hostages
// in the radar

	enum ICON_PACK_TYPE
	{
		ICON_PACK_PLAYER,
		ICON_PACK_HOSTAGE,
		ICON_PACK_GRENADES,
		ICON_PACK_DEFUSER,
		ICON_PACK_SURVIVAL_PARADROP,
	};

	enum
	{
		R_BELOW = 0,
		R_SAMELEVEL = 1,
		R_ABOVE = 2,
	};

	// panorama img names from @defines, use these to SetImage on imagepanel
	const char *m_pPlayerIconImageNames[ PI_NUM_ICONS ];

	class CCSGO_MapOverviewIconPackage
	{

	public:
		CCSGO_MapOverviewIconPackage();
		~CCSGO_MapOverviewIconPackage();

		// zero all the internal variables
		void ClearAll( void );

		// get handles to the icons which will all be children
		// of the iconPackage handle
		void Init( panorama::CPanel2D *pParent );

		// release all the handles, and clear all the variables
		// used when removing players or changing maps
		void NukeFromOrbit( CCSGO_MapOverview* pCSGOMapOverview );

		// reset all variables to their start of round values
		void StartRound( void );

		// set the states for this player
		void SetIsPlayer( bool value );
		void SetIsSpeaking( bool value );
		void SetIsOffMap( bool value );
		void SetIsLowHealth( bool value );
		void SetIsSelected( bool value );
		void SetIsFlashed( bool value );
		void SetIsFiring( bool value );
		void SetIsAboveOrBelow( int value );
		void SetIsMovingHostage( bool value );
		void SetIsDead( bool value );
		void SetIsRescued( bool value );
		void SetPlayerTeam( int team );
		void SetIsSpotted( bool value );
		void SetAlpha( float newAlpha );
		void SetIsOnLocalTeam( bool value );
		void SetIsControlledBot( void );
		void SetIsDefuse( bool bValue );
		void SetGrenadeType( int value );
		void SetGrenadeExpireTime( float value );

		// given the current set of states, decide which
		// icons should be shown and which should be hidden
		void SetupIconsFromStates( void );

		// each bit in newFlags represents the visibility of one of the
		// icons in the PLAYER_ICON_INDICES.  If the bit is on, the icon
		// is shown.
		void SetVisibilityFlags( uint64 newFlags );

		void UpdateIconsPosition( void );

		bool IsHostageType( void ) { return m_IconPackType == ICON_PACK_HOSTAGE; }
		bool IsGrenadeType( void ) { return m_IconPackType == ICON_PACK_GRENADES; }
		bool IsPlayerType( void ) { return m_IconPackType == ICON_PACK_PLAYER; }
		bool IsDefuserType( void ) { return m_IconPackType == ICON_PACK_DEFUSER; }
		bool IsSurvivalCrateType( void ) { return m_IconPackType == ICON_PACK_SURVIVAL_PARADROP; }

		bool IsVisible( void );

		int GetPlayerType();

		// the parent for all the icons
		panorama::CPanel2D *m_pIconPackage;
		panorama::CPanel2D *m_pIconPackageNonRotate;
		panorama::CPanel2D *m_pIconPackageRotate;

		// panorama panel ptrs for all the icons listed in PLAYER_ICON_INDICES
		//panorama::CImagePanel *m_pIcons[ PI_NUM_ICONS ];
		panorama::CPanel2D *m_pIcons[ PI_NUM_ICONS ];
		panorama::CLabel *m_pPlayerNumber;
		panorama::CLabel *m_pPlayerName;

		// the location and position of this player/hostage
		// only updated when the player is spotted
		Vector	m_Position;	// current x,y pos
		QAngle	m_Angle;		// view origin 0..360

		Vector m_HudPosition;
		float m_HudRotation;
		float m_flIconScale;

		// ignore visibility updates until a little time has passed
		// this keeps track of when the round started
		float m_fRoundStartTime;

		// the time at which this player/hostage died ( or was rescued )
		// used to calculate the alpha of the X icon.
		float m_fDeadTime;

		// the time at which the player / hostage was last spotted
		// used to fade out the ? icon
		float m_fGhostTime;

		// the alpha currently used to display all icons
		// used to lazy update the actual scaleform value
		float m_fCurrentAlpha;

		// each bit represents one of the PLAYER_ICON_INDICES
		// used to lazy update the visibility of the icons in scaleform
		uint64 m_iCurrentVisibilityFlags;

		// the index of this player/hostage in the radar.
		// used to create the instance name of the icon package in flash
		int m_iIndex;

		// set from the player objects UserID or EntityID ( for the hostages ). Lets us find the radar
		// object that represents a player / hostage
		int m_iEntityID;

		// state variables used to keep track of the player / hostage state
		// so we know which icon( s ) to show

		int	m_Health;		// 0..100, 7 bit

		char m_szName[ MAX_PLAYER_NAME_LENGTH + 1 ];

		// the base icon for the player
		int m_iPlayerTeam; // PI_HOSTAGE or PI_PLAYER

		int m_nAboveOrBelow;//			R_BELOW = 0,R_SAMELEVEL = 1,R_ABOVE = 2,

		int m_nGrenadeType;
		float m_fGrenExpireTime;

		ICON_PACK_TYPE m_IconPackType;

		bool m_bIsActive : 1;
		bool m_bOffMap : 1;
		bool m_bIsLowHealth : 1;
		bool m_bIsSelected : 1;
		bool m_bIsFlashed : 1;
		bool m_bIsFiring : 1;
		bool m_bIsPlayer : 1;
		bool m_bIsSpeaking : 1;
		bool m_bIsDead : 1;
		bool m_bIsMovingHostage : 1;
		bool m_bIsSpotted : 1;
		bool m_bIsRescued : 1;
		bool m_bIsOnLocalTeam : 1;
		bool m_bIsDefuser : 1;

		// don't put anything new after the bitfields or suffer the Wrath of the Compiler!
	};

	// this little class manages the display of the hostage
	// indicators in the panel

	class CCSGO_MapOverviewHostageIcons
	{
	public:
		enum HOSTAGE_ICON_INDICES
		{
			HI_DEAD,
			HI_RESCUED,
			HI_ALIVE,
			HI_TRANSIT,
			HI_NUM_ICONS,

			HI_UNUSED = HI_NUM_ICONS,
		};
 
	public:

		CCSGO_MapOverviewHostageIcons();
		~CCSGO_MapOverviewHostageIcons();

		void Init( panorama::CPanel2D *pParent, const char *szHostageIconName );

		void SetStatus( int status );

	public:

		// the parent object of all the icons
		panorama::CPanel2D *m_pIconPackage;

		// the icons which represent each of the HOSTAGE_ICON_INDICES
		panorama::CImagePanel *m_pIcons[ HI_NUM_ICONS ];

		// the index of the icon that is currently shown
		int m_iCurrentIcon;
	};

	// panorama img names from @defines, use these to SetImage on imagepanel
	const char *m_pHostageIconImageNames[ CCSGO_MapOverviewHostageIcons::HI_NUM_ICONS ];

	// this just keeps track of the bombzone and hostagezone
	// icons that are shown on the radar

	struct CCSGO_MapOverviewGoalIcon
	{
		Vector m_Position;

		// panorama panel for this instance
		panorama::CPanel2D *m_pIcon;
	};

public:

	bool MsgFunc_ProcessSpottedEntityUpdate( const CCSUsrMsg_ProcessSpottedEntityUpdate &msg );

	void ShowMapOverview( bool value );
	bool IsMapOverviewShown( void ) { return ( m_bShowMapOverview && BIsVisible() ); }
	bool CanShowOverview( void );

	void GetLayoutDefines();

	void ToggleOverviewMap();
	bool AllowMapDrawing();
	float GetWorldDistance( float x1, float y1, float x2, float y2 );

	void RefreshGraphs( void );

protected:

	void ResetRadar(bool bResetGlobalStates = true);

	void ResetForNewMap(void);
	void ResetRound(void);
	void SetMap(const char* pMapName);
	void WorldToRadar(const Vector& ptin, Vector& ptout);
	void RadarToWorld( const Vector& ptin, Vector& ptout );
	void RadarToHud( const Vector& ptin, Vector& ptout );
	void LazyCreateGoalIcons( void );

	void InitIconPackage( CCSGO_MapOverviewIconPackage* pPlayer, int iAbsoluteIndex, ICON_PACK_TYPE packType );
	void RemoveIconPackage( CCSGO_MapOverviewIconPackage* pPlayer );

	CCSGO_MapOverviewIconPackage* CreatePlayer( int index );
	void ResetPlayer( int index );
	void RemovePlayer( int index );
	void UpdateAllPlayers( void );
	void UpdatePlayerTeamColor( CCSGO_MapOverviewIconPackage* pPackage );
	void UpdatePlayer( CCSGO_MapOverviewIconPackage* pPackage );

	CCSGO_MapOverviewIconPackage* CreateHostage( int index );
	void ResetHostage( int index );
	void RemoveHostage( int index );
	void RemoveStaleHostages( void );

	CCSGO_MapOverviewIconPackage* CreateGrenade( int entityID, int nGrenadeType );
	void RemoveAllGrenades( void );
	void RemoveGrenade( int index );

	CCSGO_MapOverviewIconPackage* CreateSurvivalCrate( int entityID );
	void RemoveAllSurvivalCrates( void );
	void RemoveSurvivalCrate( int index );

	CCSGO_MapOverviewIconPackage * CreateDefuser( int nEntityID );
	CCSGO_MapOverviewIconPackage * GetDefuser( int nEntityID, bool bCreateIfNotFound = false );
	void SetDefuserPos( int nEntityID, int x, int y, int z, int a );
	void UpdateAllDefusers( void );
	void RemoveAllDefusers( void );
	void RemoveDefuser( int index );

	bool LazyUpdateIconArray( CCSGO_MapOverviewIconPackage* pArray, int lastIndex );
	virtual bool LazyCreateIconPackage( CCSGO_MapOverviewIconPackage* pPackage );

	void LazyCreatePlayerIcons( void );

	void SetPlayerTeam( int index, int team );

	int GetPlayerIndexFromUserID( int userID );
	int GetHostageIndexFromHostageEntityID( int entityID );
	int GetGrenadeIndexFromEntityID( int entityID );
	int GetDefuseIndexFromEntityID( int nEntityID );
	int GetSurvivalCrateIndexFromEntityID( int nEntityID );

	void ApplySpectatorModes( void );

	void PositionRadarViewpoint( void );
	void PlaceGoalIcons( void );
	void Show( bool show );
	void ShowPanel( bool bShow );
	void PlacePlayers();
	void PlaceHostages();
	void SetIconPackagePosition( CCSGO_MapOverviewIconPackage* pPackage );
	void UpdateMiscIcons( void );
	void SetVisibilityFlags( uint64 newFlags );
	void SetupIconsFromStates( void );
	bool IsEnemyCloseEnoughToShow( Vector vecEnemyPos );

	void ResetRoundVariables(bool bResetGlobalStates = true);

	void UpdateGrenades( void );
	void UpdateSurvivalCrates( void );

	CCSGO_MapOverviewIconPackage* GetRadarPlayer( int index );
	CCSGO_MapOverviewIconPackage* GetRadarHostage( int index );
	CCSGO_MapOverviewIconPackage* GetRadarGrenade( int index );
	CCSGO_MapOverviewIconPackage* GetRadarDefuser( int index );
	CCSGO_MapOverviewIconPackage* GetRadarHeight( int index );
	CCSGO_MapOverviewIconPackage* GetRadarSurvivalCrate( int index );

	CUserMessageBinder m_UMCMsgProcessSpottedEntityUpdate;

	// these are the icons used individually by the radar and panel
	enum RADAR_ICON_INDICES
	{
		RI_BOMB_IS_PLANTED,
		RI_BOMB_IS_PLANTED_MEDIUM,
		RI_BOMB_IS_PLANTED_FAST,
		RI_IN_HOSTAGE_ZONE,
		RI_DASHBOARD,
		RI_BOMB_ICON_PLANTED,
		RI_BOMB_ICON_DROPPED,
		RI_BOMB_ICON_BOMB_CT,
		RI_BOMB_ICON_BOMB_T,
		RI_BOMB_ICON_BOMB_ABOVE,
		RI_BOMB_ICON_BOMB_BELOW,
		RI_BOMB_ICON_PACKAGE,
		RI_DEFUSER_ICON_DROPPED,
		RI_DEFUSER_ICON_PACKAGE,
		//RI_SURVIVAL_DROP_ICON_PACKAGE,

		RI_NUM_ICONS,
	};

	enum
	{
		MAX_BOMB_ZONES = 2,
	};

	// this holds the names and indexes of the messages we receive so that
	// we don't have to do a whole bunch of string compares to find them
	static CUtlMap<const char*, int> m_messageMap;

	// these are used to scale world coordinates to radar coordinates
	Vector m_MapOrigin;
	float m_fMapSize;
	float m_fRadarSize;
	float m_fMapScale;
	float m_fMapSourceImageSize; // Actual source image width in pixels
	float m_fPixelToRadarScale;
	float m_fWorldToPixelScale;
	float m_fWorldToRadarScale;

	float m_fRadarPanelSize;
	float m_fHudPosRadarPanelCenterOffset;

	// this is center of the radar in world and map coordinates
	Vector m_RadarViewpointWorld;
	Vector m_RadarViewpointMap;
	float  m_RadarRotation;

	// the current position of the bomb
	Vector m_BombPosition;

	// the last time the bomb was seen.  Used to fade
	// out the bomb icon after it has dropped out of sight
	float m_fBombSeenTime;
	float m_fBombAlpha;

	// the current position of the defuser
	Vector m_DefuserPosition;

	// the last time the defuser was seen.  Used to fade
	// out the defuser icon after it has dropped out of sight
	float m_fDefuserSeenTime;
	float m_fDefuserAlpha;

	// a bitmap of the icons that are currently beeing shown.
	// each bit corresponds to one the RADAR_ICON_INDICES
	uint64 m_iCurrentVisibilityFlags;

	//
	// Panorama panel ptrs
	//

	// the handles to the RADAR_ICON_INDICES icons
	panorama::CPanel2D *m_pAllIcons[ RI_NUM_ICONS ];

	// bomb package container (for above)
	panorama::CPanel2D *m_pBombDefuserPackage;

	// handles to the actual bomb zone and hostage icons used on the radar
	panorama::CImagePanel *m_pHostageZoneIcons[ MAX_HOSTAGE_RESCUES ];
	panorama::CImagePanel *m_pBombZoneIcons[ MAX_BOMB_ZONES ];

	// Map Overview container panel
	panorama::CPanel2D *m_pMapOverview;

	panorama::CImagePanel* m_pMap;						// current map image panel ptr

	panorama::CImagePanel* m_pMapSurvivalDangerZone;

	// the last index of an active player in the m_Players array
	int m_iLastPlayerIndex;

	// the last index of an active hostage in the m_Hostages array
	int m_iLastHostageIndex;

	// the last index of an active decoy in the m_Grenades array
	int m_iLastGrenadeIndex;

	// the last index of an active defuser in the m_Defuser array
	int m_iLastDefuserIndex;

	// the last index of an active defuser in the m_Defuser array
	int m_iLastSurvivalCrateIndex;

	// keeps the state information and icon handles for the players
	CCSGO_MapOverviewIconPackage m_Players[ MAX_PLAYERS ];

	// keeps the state information and icon handles for the hostages
	CCSGO_MapOverviewIconPackage m_Hostages[ MAX_HOSTAGES ];

	// keeps the state information and icon handles for the decoys
	CCSGO_MapOverviewIconPackage m_Grenades[ MAX_GRENADES ];

	// keeps the state information and icon handles for the decoys
	CCSGO_MapOverviewIconPackage m_Defusers[ MAX_PLAYERS ];

	// the handles to the hostage status icons that appear beneath the dashboard
	CCSGO_MapOverviewHostageIcons m_HostageStatusIcons[ MAX_HOSTAGES ];

	// a goal icon is either a bomb-area or a hostage-area icon
	// This array holds the positions / handles of the ones that are active for the current map
	int m_iNumGoalIcons;
	CCSGO_MapOverviewGoalIcon m_GoalIcons[ MAX_HOSTAGE_RESCUES + MAX_BOMB_ZONES ];

	// keeps the state information and icon handles for the survival crates
	CCSGO_MapOverviewIconPackage m_SurvivalCrates[MAX_SURVIVAL_PARADROPS];

	// the current observer mode. Figures into the placement of the center of the radar
	// and a few other things
	int m_iObserverMode;

	char m_szMapName[ MAX_MAP_NAME + 1 ];

	// the name of our current location
	char m_szLocationString[ MAX_LOCATION_TEXT_LENGTH + 1 ];

	// this is set by a con command to hide the whole radar
	bool m_bShowMapOverview : 1;

	// set to true in spectator mode if we're not in pro mode
	bool m_bShowAll : 1;

	// keep track of whether we've already gotten all the goal icons and player icons from the
	// flash file.  This is necessary because some of the level information is loaded before
	// flash is ready
	bool m_bGotGoalIcons : 1;
	bool m_bGotPlayerIcons : 1;

	// state information about which icons should be displayed	
	bool m_bShowingHostageZone : 1;
	bool m_bBombPlanted : 1;
	bool m_bBombDropped : 1;
	bool m_bBombDefused : 1;
	bool m_bBombExploded : 1;
	bool m_bShowBombHighlight : 1;
	bool m_bShowingDashboard : 1;

	bool m_bBombIsSpotted : 1;
	int	 m_nBombEntIndex;

	bool m_bTrackDefusers;

	// entities spotted last ProcessSpottedEntityUpdate
	CBitVec<MAX_EDICTS> m_EntitySpotted;

private:

	bool OnMapImageLoaded( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, panorama::IImageSource *pImage );

	void UpdateDangerZoneOverlayVisibility();

	bool OnMapImageFailedLoad( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, panorama::IImageSource *pImage );

	bool OnStyleFileReloaded( panorama::CPanoramaSymbol symFile );
	void UpdateRadarScalingValues( float fRadarVisSize, float fRadarPanelSize );
};
