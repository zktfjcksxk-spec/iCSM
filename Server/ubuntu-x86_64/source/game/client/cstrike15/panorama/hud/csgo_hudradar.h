//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//
#pragma once

#include "hud.h"
#include "hud_element_helper.h"
#include "panorama/csgo_panorama.h"
#include "panorama/hud/csgo_hud.h"
#include "c_cs_hostage.h"

#define MAX_LOCATION_TEXT_LENGTH 100

#define MAX_DECOYS 30

class CCSGO_HudRadar_Symbols;

//-----------------------------------------------------------------------------
// Purpose: CSGO Hud Radar (minimap) panel
//-----------------------------------------------------------------------------
class CCSGO_HudRadar : public panorama::CPanel2D, public CPanoramaHudElement
{
	DECLARE_PANEL2D( CCSGO_HudRadar, panorama::CPanel2D );

public:
	CCSGO_HudRadar(panorama::CPanel2D *pParent, const char *pchID);
	virtual ~CCSGO_HudRadar();

	// These overload the CHudElement class
	virtual void Init( void );
	virtual void LevelInit( void );
	virtual void LevelShutdown( void );
	virtual void ProcessInput( void );
	virtual void SetActive( bool bActive );
	virtual bool ShouldDraw( void );
	virtual void Reset( void )
	{
		SetActive( true );
	}

	virtual void FireGameEvent(IGameEvent *pEvent) OVERRIDE;

	bool MsgFunc_ProcessSpottedEntityUpdate( const CCSUsrMsg_ProcessSpottedEntityUpdate &msg );

	void ShowRadar( bool value ) { m_bShowRadar = value; }
	bool IsRadarShown( void ) { return m_bShowRadar; }

	void SwitchRadarToRound( bool toRound, bool bForce = false );

	void GetLayoutDefines();

	// use of cl_hud_color
// 	void GetHudTextColor( Color *pColor );
// 	void GetBGHudTextColor( Color *pColor, const float flBrightness, const float flSaturation );

	CUserMessageBinder m_UMCMsgProcessSpottedEntityUpdate;

	bool m_bRound; // Is the radar round ( otherwise square )

	static const CCSGO_HudRadar_Symbols& Symbols();

protected:

//

// this manages the display of the players and hostages
// in the radar

	enum ICON_PACK_TYPE
	{
		ICON_PACK_PLAYER,
		ICON_PACK_HOSTAGE,
		ICON_PACK_DECOY,
		ICON_PACK_DEFUSER,
	};

	enum
	{
		R_BELOW = 0,
		R_SAMELEVEL = 1,
		R_ABOVE = 2,
	};

	// each enum represents an icon that this class is managing
	enum PLAYER_ICON_INDICES
	{
		PI_PLAYER_NUMBER,		// ensure number and letter stay as 0 and 1 since they are used to index 
		PI_PLAYER_LETTER,
		PI_FIRST_ROTATED,

		PI_SPEAKING = PI_FIRST_ROTATED,
		PI_SPEAKING_OFFMAP,
		PI_ABOVE,
		PI_BELOW,
		PI_HOSTAGE_MOVING,

		PI_CT,
		PI_CT_OFFMAP,
		PI_CT_DEAD,
		PI_CT_GHOST,

		PI_T,
		PI_T_OFFMAP,
		PI_T_DEAD,
		PI_T_GHOST,

		PI_ENEMY,
		PI_ENEMY_OFFMAP,
		PI_ENEMY_DEAD,
		PI_ENEMY_GHOST,

		PI_HOSTAGE,
		PI_HOSTAGE_OFFMAP,
		PI_HOSTAGE_DEAD,
		PI_HOSTAGE_GHOST,

		PI_DIRECTION_INDICATOR,

		PI_DEFUSER,

		PI_SELECTED,
		PI_VIEWFRUSTRUM,

		PI_NUM_ICONS
	};

	// panorama img names from @defines, use these to SetImage on imagepanel
	const char *m_pPlayerIconImageNames[ PI_NUM_ICONS ];

	class CCSGO_HudRadarIconPackage
	{

	public:
		CCSGO_HudRadarIconPackage();
		~CCSGO_HudRadarIconPackage();

		// zero all the internal variables
		void ClearAll( void );

		// get handles to the icons which will all be children
		// of the iconPackage handle
		//TODO - remove void Init( IScaleformUI* pui, SFVALUE iconPackage );
		void Init( panorama::CPanel2D *pParent );

		// release all the handles, and clear all the variables
		// used when removing players or changing maps
		void NukeFromOrbit( CCSGO_HudRadar* pCSGORadar );

		// reset all variables to their start of round values
		void StartRound( void );

		// set the states for this player
		void SetIsPlayer( bool value );
		void SetIsSelected( bool value );
		void SetIsSpeaking( bool value );
		void SetIsOffMap( bool value );
		void SetIsAboveOrBelow( int value );
		void SetIsMovingHostage( bool value );
		void SetIsDead( bool value );
		void SetIsRescued( bool value );
		void SetPlayerTeam( int team );
		void SetGrenadeExpireTime( float value );
		void SetIsSpotted( bool value );
		void SetIsSpottedByFriendsOnly( bool value );
		void SetAlpha( float newAlpha );
		void SetIsOnLocalTeam( bool value );
		void SetIsBot( bool value );
		void SetIsControlledBot( void );
		void SetIsDefuse( bool bValue );

		// given the current set of states, decide which
		// icons should be shown and which should be hidden
		void SetupIconsFromStates( void );

		// each bit in newFlags represents the visibility of one of the
		// icons in the PLAYER_ICON_INDICES.  If the bit is on, the icon
		// is shown.
		void SetVisibilityFlags( int newFlags );

		void UpdateIconsPosition( void );

		bool IsHostageType( void ) { return m_IconPackType == ICON_PACK_HOSTAGE; }
		bool IsDecoyType( void ) { return m_IconPackType == ICON_PACK_DECOY; }
		bool IsPlayerType( void ) { return m_IconPackType == ICON_PACK_PLAYER; }
		bool IsDefuserType( void ) { return m_IconPackType == ICON_PACK_DEFUSER; }

		bool IsVisible( void );

	public:
		// pointer to scaleform

		// the parent for all the icons
		panorama::CPanel2D *m_pIconPackage;
		panorama::CPanel2D *m_pIconPackageNonRotate;
		panorama::CPanel2D *m_pIconPackageRotate;

		// panorama panel ptrs for all the icons listed in PLAYER_ICON_INDICES
		panorama::CImagePanel *m_pIcons[ PI_NUM_ICONS ];
		panorama::CLabel *m_pLabels[ PI_FIRST_ROTATED ]; // cache for player number and letter only

		// the location and position of this player/hostage
		// only updated when the player is spotted
		Vector	m_Position;	// current x,y pos
		QAngle	m_Angle;		// view origin 0..360

		// HUD Position, rotation and scale - used to update the position of the visible icons
		Vector m_HudPosition;
		float m_HudRotation;
		float m_HudScale;

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

		// last time we applied this color to the movie
		float m_fLastColorUpdate;

		// each bit represents one of the PLAYER_ICON_INDICES
		// used to lazy update the visibility of the icons in scaleform
		int m_iCurrentVisibilityFlags;

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
		int m_iPlayerType; // will be PI_CT, PI_T, or PI_HOSTAGE

		int m_nAboveOrBelow;//			R_BELOW = 0,R_SAMELEVEL = 1,R_ABOVE = 2,

		float m_fGrenExpireTime;

		ICON_PACK_TYPE m_IconPackType;

		bool m_bIsActive : 1;
		bool m_bOffMap : 1;
		bool m_bIsPlayer : 1;
		bool m_bIsSelected : 1;
		bool m_bIsSpeaking : 1;
		bool m_bIsDead : 1;
		bool m_bIsBot : 1;
		bool m_bIsMovingHostage : 1;
		bool m_bIsSpotted : 1;
		bool m_bIsSpottedByFriendsOnly : 1;
		bool m_bIsRescued : 1;
		bool m_bIsOnLocalTeam : 1;
		bool m_bIsDefuser : 1;
		bool m_bHostageIsUsed : 1;

		// don't put anything new after the bitfields or suffer the Wrath of the Compiler!
	};

	// this little class manages the display of the hostage
	// indicators in the panel

	class CCSGO_HudRadarHostageIcons
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

		CCSGO_HudRadarHostageIcons();
		~CCSGO_HudRadarHostageIcons();

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
	const char *m_pHostageIconImageNames[ CCSGO_HudRadarHostageIcons::HI_NUM_ICONS ];

	// this just keeps track of the bombzone and hostagezone
	// icons that are shown on the radar

	struct CCSGO_HudRadarGoalIcon
	{
		Vector m_Position;

		// panorama panel for this instance
		panorama::CPanel2D *m_pIcon;
	};

	void ResetRadar(bool bResetGlobalStates = true);

	void ResetForNewMap(void);
	void ResetRound(void);
	void SetMap(const char* pMapName);
	void WorldToRadar(const Vector& ptin, Vector& ptout);
	void RadarToHud( const Vector& ptin, Vector& ptout );
	void LazyCreateGoalIcons( void );
	bool UpdateMapLayer( int layerIdx, bool bFade = false );

	void InitIconPackage( CCSGO_HudRadarIconPackage* pPlayer, int iAbsoluteIndex, ICON_PACK_TYPE packType );
	void RemoveIconPackage( CCSGO_HudRadarIconPackage* pPlayer );

	CCSGO_HudRadarIconPackage* CreatePlayer( int index );
	void ResetPlayer( int index );
	void RemovePlayer( int index );

	CCSGO_HudRadarIconPackage* CreateHostage( int index );
	void ResetHostage( int index );
	void RemoveHostage( int index );
	void RemoveStaleHostages( void );
	void RemoveAllHostages( void );

	CCSGO_HudRadarIconPackage* CreateDecoy( int index );
	void RemoveAllDecoys( void );
	void RemoveDecoy( int index );

	CCSGO_HudRadarIconPackage * CreateDefuser( int nEntityID );
	CCSGO_HudRadarIconPackage * GetDefuser( int nEntityID, bool bCreateIfNotFound = false );
	void SetDefuserPos( int nEntityID, int x, int y, int z, int a );
	void UpdateAllDefusers( void );
	void RemoveAllDefusers( void );
	void RemoveDefuser( int index );

	bool LazyUpdateIconArray( CCSGO_HudRadarIconPackage* pArray, int lastIndex );
	virtual bool LazyCreateIconPackage( CCSGO_HudRadarIconPackage* pPackage );

	void LazyCreatePlayerIcons( void );

	void SetPlayerTeam( int index, int team );

	int GetPlayerIndexFromUserID( int userID );
	int GetHostageIndexFromHostageEntityID( int entityID );
	int GetDecoyIndexFromEntityID( int entityID );
	int GetDefuseIndexFromEntityID( int nEntityID );

	void ApplySpectatorModes( void );

	void PositionRadarViewpoint( void );
	void PlaceGoalIcons( void );
	void Show( bool show );
	void PlacePlayers();
	void PlaceHostages();
	void SetIconPackagePosition( CCSGO_HudRadarIconPackage* pPackage );
	void UpdateMiscIcons( void );
	void SetVisibilityFlags( int newFlags );
	void SetupIconsFromStates( void );

	void SetLocationText( const char *szNewText );

	void ResetRoundVariables(bool bResetGlobalStates = true);

	void UpdateDecoys( void );

	void UpdateAllPlayerNumbers( void );
	void UpdatePlayerNumber( CCSGO_HudRadarIconPackage* pPackage );

	CCSGO_HudRadarIconPackage* GetRadarPlayer( int index );
	CCSGO_HudRadarIconPackage* GetRadarHostage( int index );
	CCSGO_HudRadarIconPackage* GetRadarDecoy( int index );
	CCSGO_HudRadarIconPackage* GetRadarDefuser( int index );
	CCSGO_HudRadarIconPackage* GetRadarHeight( int index );

	// these are the icons used individually by the radar and panel
	enum RADAR_ICON_INDICES
	{
		RI_BOMB_IS_PLANTED,
		RI_BOMB_IS_PLANTED_MEDIUM,
		RI_BOMB_IS_PLANTED_FAST,
		RI_BOMB_ICON_PLANTED,
		RI_BOMB_ICON_DROPPED,
		RI_BOMB_ICON_BOMB_ABOVE,
		RI_BOMB_ICON_BOMB_BELOW,
		RI_BOMB_ICON_PACKAGE,
		RI_DEFUSER_ICON_DROPPED,
		RI_DEFUSER_ICON_PACKAGE,

		RI_NUM_ICONS,
	};

	enum
	{
		MAX_BOMB_ZONES = 2,
	};

	int m_nCurrentRadarVerticalSection;

	struct HudRadarLevelVerticalSection_t
	{
		int m_nSectionIndex;
		char m_szSectionName[ MAX_MAP_NAME ];
		float m_flSectionAltitudeFloor;
		float m_flSectionAltitudeCeiling;

		HudRadarLevelVerticalSection_t()
		{
			m_nSectionIndex = 0;
			m_szSectionName[ 0 ] = 0;
			m_flSectionAltitudeFloor = 0;
			m_flSectionAltitudeCeiling = 0;
		}
	};
	CUtlVector< HudRadarLevelVerticalSection_t > m_vecRadarVerticalSections;

	// this holds the names and indexes of the messages we receive so that
	// we don't have to do a whole bunch of string compares to find them
	static CUtlMap<const char*, int> m_messageMap;

	// these are used to scale world coordinates to radar coordinates
	Vector m_MapOrigin;
	float m_fMapSize;
	float m_fRadarSize;
	float m_fRoundRadarSize;
	float m_fMapSourceImageSize; // Actual source image width in pixels
	float m_fPixelToRadarScale;
	float m_fWorldToPixelScale;
	float m_fWorldToRadarScale;

	float m_fRadarRadiusSq;
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
	int m_iCurrentVisibilityFlags;

	//
	// Panorama panel ptrs
	//

	// the handles to the RADAR_ICON_INDICES icons
	panorama::CPanel2D *m_pIcons[ RI_NUM_ICONS ];

	// bomb package container (for above)
	panorama::CPanel2D *m_pBombDefuserPackage;

	// handles to the actual bomb zone and hostage icons used on the radar
	panorama::CImagePanel *m_pHostageZoneIcons[ MAX_HOSTAGE_RESCUES ];
	panorama::CImagePanel *m_pBombZoneIcons[ MAX_BOMB_ZONES ];

	// Radar container panel
	panorama::CPanel2D *m_pRadar;

	// Upper Dashboard - location text, sits above the radar
	panorama::CLabel *m_pLocationText;

	panorama::CPanel2D* m_aMapContainerPanel[ 2 ];		// map container/mask panel
	panorama::CPanel2D* m_aMapStyle[ 2 ];				// 0 - square, 1 - round
	panorama::CPanel2D* m_aMapStyleTransform[ 2 ];		// transform parent, 0 - square, 1 - round
	CUtlVector<panorama::CImagePanel *> m_aMapLevels[ 2 ];	// multi-level support
	panorama::CImagePanel* m_pMapImg[ 2 ];						// current map image panel ptr
	panorama::CPanel2D *m_aMapBorder[ 2 ];				// for tinting border using cl_hud_color

	// ptrs to round and square radar panels
	panorama::CPanel2D *m_pRadarRound;
	panorama::CPanel2D *m_pRadarSquare;
	
	// north indicator
	panorama::CPanel2D* m_pDirectionArrow;

	// parent panel for invisible panels (panels above can be re-parented to this to optimize time spent in the animation thread as well as keeping the debugger less cluttered)
	panorama::CPanel2D *m_pRadarInvisible;

	// the last index of an active player in the m_Players array
	int m_iLastPlayerIndex;

	// the last index of an active hostage in the m_Hostages array
	int m_iLastHostageIndex;

	// the last index of an active decoy in the m_Decoys array
	int m_iLastDecoyIndex;

	// the last index of an active defuser in the m_Defuser array
	int m_iLastDefuserIndex;

	// keeps the state information and icon handles for the players
	CCSGO_HudRadarIconPackage m_Players[ MAX_PLAYERS ];

	// keeps the state information and icon handles for the hostages
	CCSGO_HudRadarIconPackage m_Hostages[ MAX_HOSTAGES ];

	// keeps the state information and icon handles for the decoys
	CCSGO_HudRadarIconPackage m_Decoys[ MAX_DECOYS ];

	// keeps the state information and icon handles for the decoys
	CCSGO_HudRadarIconPackage m_Defusers[ MAX_PLAYERS ];

	// a goal icon is either a bomb-area or a hostage-area icon
	// This array holds the positions / handles of the ones that are active for the current map
	int m_iNumGoalIcons;
	CCSGO_HudRadarGoalIcon m_GoalIcons[ MAX_HOSTAGE_RESCUES + MAX_BOMB_ZONES ];

	// the current observer mode. Figures into the placement of the center of the radar
	// and a few other things
	int m_iObserverMode;

	// there is a loaded and a desired so that we don't load the same map twice, and so that we
	// can request that a map be loaded before the flash stuff is able to actually load it.
	char m_cLoadedMapName[ MAX_MAP_NAME + 1 ];
	char m_cDesiredMapName[ MAX_MAP_NAME + 1 ];

	// the name of our current location
	char m_szLocationString[ MAX_LOCATION_TEXT_LENGTH + 2 ];

	// this is set by a con command to hide the whole radar
	bool m_bShowRadar : 1;

	bool m_bVisible;

	bool m_bShowViewFrustrum;

	// set to true in spectator mode if we're not in pro mode
	bool m_bShowAll : 1;

	// keep track of whether we've already gotten all the goal icons and player icons from the
	// flash file.  This is necessary because some of the level information is loaded before
	// flash is ready
	bool m_bGotGoalIcons : 1;
	bool m_bGotPlayerIcons : 1;

	// state information about which icons should be displayed	
	bool m_bBombPlanted : 1;
	bool m_bBombDropped : 1;
	bool m_bBombDefused : 1;
	bool m_bBombExploded : 1;
	bool m_bShowBombHighlight : 1;

	bool m_bBombIsSpotted : 1;
	int	 m_nBombEntIndex;
	int	 m_nBombHolderUserId;

	bool m_bTrackDefusers;

	bool m_bShowColorLetter;

	// entities spotted last ProcessSpottedEntityUpdate
	CBitVec<MAX_EDICTS> m_EntitySpotted;

private:

	bool OnImageLoaded( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, panorama::IImageSource *pImage );

	bool OnStyleFileReloaded( panorama::CPanoramaSymbol symFile );
	void UpdateRadarScalingValues( float fRadarVisSize, float fRadarPanelSize );

	bool SetPanelInvisible( panorama::CPanel2D *pPanel );
	void HideShowGameModePanels( bool bReset = false );

	int m_nVisibleLayer;
	void *m_pLayeredRadar;

	bool m_bIsShown;

	float m_flLastC4ResetPos;
	float m_flSafeZoneY;
};
