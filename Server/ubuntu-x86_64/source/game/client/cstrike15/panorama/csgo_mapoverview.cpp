//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "cbase.h"
#include "csgo_mapoverview.h"
#include "hud_macros.h"
#include <filesystem.h>
#include "cs_gamerules.h"
#include "c_team.h"
#include "c_cs_playerresource.h"
#include "c_plantedc4.h"
#include "c_cs_hostage.h"
#include "c_cs_player.h"
#include "cs_shareddefs.h"
#include "clientmode.h"
#include "voice_status.h"
#include "view.h"
#include "hud/csgo_hudteamcounter.h"
#include "hud/csgo_hudradar.h"
#include "gametypes.h"
#include "hltvreplaysystem.h"
#include "clientmode_shared.h"
//#include "uicomponents/uicomponent_matchstats.h"
#if defined( INCLUDE_SCALEFORM )
#include "HUD/sfhudfreezepanel.h" // TODO - panorama version
#endif
#include "dangerzone_controller.h"
#include "panorama/hud/csgo_hudhinttext.h"


// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

REGISTER_PANEL2D_FACTORY( CCSGO_MapOverview, CSGOMapOverview );

DECLARE_HUD_MESSAGE( CCSGO_MapOverview, ProcessSpottedEntityUpdate );

extern ConVar cl_sanitize_player_names;

using namespace panorama;

static const float NOTSPOTTEDRADIUS = 1400.0f;
static const float DEAD_FADE_TIME = 4.0f;
static const float GHOST_FADE_TIME = 6.0f;
static const float BOMB_FADE_TIME = 8.0f;
static const float DEFUSER_FADE_TIME = 8.0f;
static const float TIMER_INIT = -1000.0f;
static const int   INVALID_INDEX = -1;
const float BOMB_PULSE_TIME = 0.5f;
const float BOMB_PULSE_TIME_FAST = 0.5f;

extern ConVar mapoverview_allow_client_draw;
extern ConVar mapoverview_allow_grid_usage;
extern ConVar mapoverview_icon_scale;

extern ConVar cl_drawhud;

extern bool IsTakingAFreezecamScreenshot();

// Additional scale factor applied to the radar to mimic the behavior of the CSGO Scaleform Hud
#define SF_TO_PANORAMA_RADAR_SCALE 1.0f//(768.0f / 320.0f)  // 320 is the 'map size' defined by Scaleform, so this define gives us the map size relative to the base layout size used by SF

//-----------------------------------------------------------------------------
// data definitions
//-----------------------------------------------------------------------------

static const char* playerIconNames[] = {
	"Grenade_HE",
	"Grenade_FLASH",
	"Grenade_SMOKE",
	"Grenade_MOLOTOV",
	"Grenade_DECOY",
	"SurvivalCrate",

	"Defuser",
	"AbovePlayer",
	"BelowPlayer",
	"Flashed",
	"PlayerNumber",
	"PlayerName",
	// below this, the icons are rotated
	"PlayerIndicator",
	"SpeakingOnMap",
	"HostageTransitOnMap",
	"PlayerOnMap",
	"PlayerDeath",
	"PlayerGhost",
	"EnemyOnMap",
	"EnemyDeath",
	"EnemyGhost",
	"HostageOnMap",
	"HostageDeath",
	"HostageGhost",
	"DirectionalIndicator",
	"MuzzleFlash",
	"LowHealth",
	"Selected",
	NULL
};

static const char* hostageIconNames[] = {
	"HostageDead",
	"HostageRescued",
	"HostageAlive",
	"HostageTransit",
	NULL
};

static const char* overviewIconNames[] = {
	"BombPlantedIcon",
	"BombPlantedIconMedium",
	"BombPlantedIconFast",
	"HostageZoneIcon",
	NULL
};

static const char *desiredMessageNames[] = {
	"game_newmap",
	"round_start",
	"player_connect",
	"player_info",
	"player_team",
	"player_spawn",
	"player_death",
	"player_disconnect",
	"hostage_killed",
	"hostage_rescued",
	"bomb_defused",
	"bomb_exploded",
	"bomb_planted",
	"bomb_pickup",
	"bomb_dropped",
	"defuser_pickup",
	"defuser_dropped",
	"decoy_started",
	"decoy_detonate",
	"hegrenade_detonate",
	"flashbang_detonate",
	"smokegrenade_detonate",
	"smokegrenade_expired",
	"inferno_startburn",
	"inferno_expire",
	"bot_takeover",
	"survival_paradrop_spawn",
	"survival_paradrop_break",
	NULL
};

enum DESIRED_MESSAGE_INDICES
{
	GAME_NEWMAP,
	ROUND_POST_START,
	PLAYER_CONNECT,
	PLAYER_INFO,
	PLAYER_TEAM,
	PLAYER_SPAWN,
	PLAYER_DEATH,
	PLAYER_DISCONNECT,
	HOSTAGE_KILLED,
	HOSTAGE_RESCUED,
	BOMB_DEFUSED,
	BOMB_EXPLODED,
	BOMB_PLANTED,
	BOMB_PICKUP,
	BOMB_DROPPED,
	DEFUSER_PICKUP,
	DEFUSER_DROPPED,
	DECOY_STARTED,
	DECOY_DETONATE,
	HE_DETONATE,
	FLASH_DETONATE,
	SMOKE_DETONATE,
	SMOKE_EXPIRE,
	MOLOTOV_DETONATE,
	MOLOTOV_EXPIRE,
	BOT_TAKEOVER,
	SURVIVAL_PARADROP_SPAWN,
	SURVIVAL_PARADROP_BREAK,
};

//-----------------------------------------------------------------------------
// callback and function declarations
//-----------------------------------------------------------------------------

static bool maplessfunc(const char* const & s1, const char* const & s2)
{
	return V_strcmp(s1, s2) < 0;
}

CUtlMap<const char*, int> CCSGO_MapOverview::m_messageMap(maplessfunc);


CON_COMMAND( drawoverviewmap, "Draws the overview map" )
{
	( GET_HUDELEMENT( CCSGO_MapOverview ) )->ShowMapOverview( true );
}

CON_COMMAND( hideoverviewmap, "Hides the overview map" )
{
	( GET_HUDELEMENT( CCSGO_MapOverview ) )->ShowMapOverview( false );
}

//-----------------------------------------------------------------------------
// Transform helpers
//-----------------------------------------------------------------------------

void Helper_SetMapOverviewPanelTransform3D( panorama::CPanel2D *pPanel, const Vector &vTranslation, float fRotation, float fScale )
{
	if ( !pPanel )
		return;

	CUtlVector<CTransform3D *> vecTransforms;

	// Scale
	if ( fScale != 0.0f )
		vecTransforms.AddToTail( new CTransformScale3D( fScale, fScale, 1.0f ) );

	// Rotation
	if ( fRotation != 0.0f )
		vecTransforms.AddToTail( new CTransformRotate3D( 0, fRotation, 0.0f ) );

	// Translation
	vecTransforms.AddToTail( new CTransformTranslate3D( vTranslation.x, vTranslation.y, 0.0f ) );

	// Set panel transform
	pPanel->AccessStyleDirty()->SetTransform3D( vecTransforms );
}

void Helper_SetMapOverviewPanelTransform3D_Translate( panorama::CPanel2D *pPanel, const Vector &vTranslation, float fScale )
{
	if ( !pPanel )
		return;

	CUtlVector<CTransform3D *> vecTransforms;

	// Translation
	vecTransforms.AddToTail( new CTransformTranslate3D( vTranslation.x, vTranslation.y, 0.0f ) );

	// Set panel transform
	pPanel->AccessStyleDirty()->SetTransform3D( vecTransforms );
}

void Helper_SetMapOverviewPanelTransform3D_Rotate( panorama::CPanel2D *pPanel, float fRotation )
{
	if ( !pPanel )
		return;

	CUtlVector<CTransform3D *> vecTransforms;

	// Rotation
	if ( fRotation != 0.0f )
	{
		vecTransforms.AddToTail( new CTransformRotate3D( 0, fRotation, 0.0f ) );

		// Set panel transform
		pPanel->AccessStyleDirty()->SetTransform3D( vecTransforms );
	}
}

void Helper_SetMapOverviewPanelTransform3D_Scale( panorama::CPanel2D *pPanel,  float fScale )
{
	if ( !pPanel )
		return;

	CUtlVector<CTransform3D *> vecTransforms;

	// Scale
	if ( fScale != 0.0f )
	{
		vecTransforms.AddToTail( new CTransformScale3D( fScale, fScale, 1.0f ) );

		// Set panel transform
		pPanel->AccessStyleDirty()->SetTransform3D( vecTransforms );
	}
}
//-----------------------------------------------------------------------------
// CCSGO_MapOverviewIconPackage code
//-----------------------------------------------------------------------------

CCSGO_MapOverview::CCSGO_MapOverviewIconPackage::CCSGO_MapOverviewIconPackage()
{
	ClearAll();
}

CCSGO_MapOverview::CCSGO_MapOverviewIconPackage::~CCSGO_MapOverviewIconPackage()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::CCSGO_MapOverviewIconPackage::ClearAll( void )
{
	m_pIconPackage = NULL;
	m_pIconPackageRotate = NULL;
	V_memset( m_pIcons, 0, sizeof( m_pIcons ) );

	m_iCurrentVisibilityFlags = 0;
	m_fCurrentAlpha = 1.0f;

	m_Health = 0;
	m_iPlayerTeam = TEAM_INVALID;
	m_iIndex = INVALID_INDEX;
	m_iEntityID = 0;

	m_bIsActive = false;
	m_bOffMap = true;

	m_bIsLowHealth = false;
	m_bIsSelected = false;
	m_bIsFlashed = false;
	m_bIsFiring = false;

	m_bIsSpotted = false;
	m_fGhostTime = TIMER_INIT;

	m_bIsDead = false;
	m_fDeadTime = TIMER_INIT;
	m_fGrenExpireTime = TIMER_INIT;

	m_IconPackType = ICON_PACK_PLAYER;
	m_bIsRescued = false;

	m_bIsOnLocalTeam = false;

	m_fRoundStartTime = TIMER_INIT;

	m_szName[ 0 ] = 0;

	m_Position = vec3_origin;
	m_Angle.Init();

	m_nGrenadeType = -1;

	m_bIsDefuser = false;

	m_HudPosition = vec3_origin;
	m_HudRotation = 0.0f;
	m_flIconScale = 1.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::CCSGO_MapOverviewIconPackage::Init( panorama::CPanel2D *pParent )
{
	if ( !pParent )
		return;

	if ( m_pIconPackage )
		return;

	switch ( m_IconPackType )
	{
	case ICON_PACK_PLAYER:
		m_pIconPackage = new panorama::CPanel2D( pParent, CFmtStr( "Player%d", m_iIndex ) );
		m_pIconPackage->AddClass( "PlayerIcons__Player" );
		break;
	case ICON_PACK_HOSTAGE:
		m_pIconPackage = new panorama::CPanel2D( pParent, CFmtStr( "Hostage%d", m_iIndex ) );
		m_pIconPackage->AddClass( "PlayerIcons__Hostage" );
		break;
	case ICON_PACK_GRENADES:
		m_pIconPackage = new panorama::CPanel2D( pParent, CFmtStr( "Grenade%d", m_iIndex ) );
		m_pIconPackage->AddClass( "PlayerIcons__Grenade" );
		break;
	case ICON_PACK_DEFUSER: 
		m_pIconPackage = new panorama::CPanel2D( pParent, CFmtStr( "Defuser%d", m_iIndex ) );
		m_pIconPackage->AddClass( "PlayerIcons__Defuser" );
		break;
	case ICON_PACK_SURVIVAL_PARADROP:
		m_pIconPackage = new panorama::CPanel2D( pParent, CFmtStr( "PCrate%d", m_iIndex ) );
		m_pIconPackage->AddClass( "PlayerIcons__SurvivalCrate" );
		break;
		
	default:
		Assert( 0 );
		return;
		break;
	}

	// TODO - snippets could likely be optimized to suit above iconpacktypes
	// would need to change vis flags to accommodate
	m_pIconPackage->RequireLoadLayoutSnippet( "PlayerIconPackage" );

	m_pIconPackage->SetVisible( m_iCurrentVisibilityFlags != 0 );
	m_pIconPackage->SetOpacity( m_fCurrentAlpha );

	m_pIconPackageNonRotate = panorama::panel_cast<panorama::CPanel2D *>( m_pIconPackage->RequireChildInLayoutFile( "PI_NonRotated" ) );
	m_pIconPackageRotate = panorama::panel_cast<panorama::CPanel2D *>( m_pIconPackage->RequireChildInLayoutFile( "PI_FirstRotated" ) );

	for ( int i = 0; i < PI_NUM_ICONS && playerIconNames[ i ] != NULL; i++ )
	{
		if ( i < PI_FIRST_ROTATED )
		{
			if ( m_pIconPackage )
			{
				m_pIcons[ i ] = m_pIconPackage->RequireChildInLayoutFile( playerIconNames[ i ] );
			}

			// cache number and name Labels
			if ( i == PI_PLAYER_NUMBER )
			{
				m_pPlayerNumber = panorama::panel_cast<panorama::CLabel *>( m_pIconPackage->RequireChildInLayoutFile( playerIconNames[ i ] ) );
			}
			else if ( i == PI_PLAYER_NAME )
			{
				m_pPlayerName = panorama::panel_cast<panorama::CLabel *>( m_pIconPackage->RequireChildInLayoutFile( playerIconNames[ i ] ) );
			}
		}
		else
		{
			if ( m_pIconPackageRotate )
			{
				m_pIcons[ i ] = m_pIconPackageRotate->RequireChildInLayoutFile( playerIconNames[ i ] );
			}
		}

		if ( m_pIcons[ i ] )
		{
			m_pIcons[ i ]->SetVisible( ( m_iCurrentVisibilityFlags & ( 1ll << i ) ) != 0 );
		}
	}

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::CCSGO_MapOverviewIconPackage::NukeFromOrbit( CCSGO_MapOverview* pSFUI )
{
	if ( m_pIconPackage )
		m_pIconPackage->DeleteAsync();

	ClearAll();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::CCSGO_MapOverviewIconPackage::StartRound( void )
{
	m_Health = 100;

	m_bOffMap = false;
	m_bIsLowHealth = false;
	m_bIsSelected = false;
	m_bIsFlashed = false;
	m_bIsFiring = false;
	m_bIsSpotted = false;
	m_fGhostTime = TIMER_INIT;

	m_bIsDead = false;
	m_fDeadTime = TIMER_INIT;
	m_fGrenExpireTime = TIMER_INIT;

	m_bIsRescued = false;

	m_nAboveOrBelow = R_SAMELEVEL;

	m_fRoundStartTime = gpGlobals->curtime;

	m_nGrenadeType = -1;

	m_Position = vec3_origin;
	m_Angle.Init();

	SetAlpha( 0 );
	SetVisibilityFlags( 0 );

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------

bool CCSGO_MapOverview::CCSGO_MapOverviewIconPackage::IsVisible( void )
{
	return ( m_fCurrentAlpha != 0 && m_iCurrentVisibilityFlags != 0 );
}

void CCSGO_MapOverview::CCSGO_MapOverviewIconPackage::SetIsPlayer( bool value )
{
	m_bIsPlayer = value;
}

void CCSGO_MapOverview::CCSGO_MapOverviewIconPackage::SetIsSpeaking( bool value )
{
	m_bIsSpeaking = value && m_bIsOnLocalTeam;
}

void CCSGO_MapOverview::CCSGO_MapOverviewIconPackage::SetIsOffMap( bool value )
{
	m_bOffMap = value;
}

void CCSGO_MapOverview::CCSGO_MapOverviewIconPackage::SetIsLowHealth( bool value )
{
	m_bIsLowHealth = value;
}

void CCSGO_MapOverview::CCSGO_MapOverviewIconPackage::SetIsSelected( bool value )
{
	m_bIsSelected = value;
}

void CCSGO_MapOverview::CCSGO_MapOverviewIconPackage::SetIsFlashed( bool value )
{
	m_bIsFlashed = value;
}

void CCSGO_MapOverview::CCSGO_MapOverviewIconPackage::SetIsFiring( bool value )
{
	m_bIsFiring = value;
}

void CCSGO_MapOverview::CCSGO_MapOverviewIconPackage::SetIsAboveOrBelow( int value )
{
	m_nAboveOrBelow = value;
}

void CCSGO_MapOverview::CCSGO_MapOverviewIconPackage::SetIsMovingHostage( bool value )
{
	m_bIsMovingHostage = value;
}

void CCSGO_MapOverview::CCSGO_MapOverviewIconPackage::SetIsRescued( bool value )
{
	m_bIsRescued = value;
}

void CCSGO_MapOverview::CCSGO_MapOverviewIconPackage::SetIsOnLocalTeam( bool value )
{
	m_bIsOnLocalTeam = value;
}

void CCSGO_MapOverview::CCSGO_MapOverviewIconPackage::SetIsControlledBot( void )
{
	m_bIsDead = true;
	m_bIsSpotted = true;
	m_fDeadTime = TIMER_INIT;
	m_fGhostTime = TIMER_INIT;
}


void CCSGO_MapOverview::CCSGO_MapOverviewIconPackage::SetIsDead( bool value )
{
	if ( value != m_bIsDead )
	{
		if ( value )
		{
			// 			SetIsSpotted( true );
			m_fDeadTime = gpGlobals->curtime;
		}
		else
		{
			m_fDeadTime = TIMER_INIT;
		}

		m_bIsDead = value;
	}

}

void CCSGO_MapOverview::CCSGO_MapOverviewIconPackage::SetGrenadeExpireTime( float value )
{
	if ( value )
	{
		m_fGrenExpireTime = value;
	}
}

void CCSGO_MapOverview::CCSGO_MapOverviewIconPackage::SetIsSpotted( bool value )
{
	if ( gpGlobals->curtime - m_fRoundStartTime > 0.25f )
	{
		if ( value != m_bIsSpotted )
		{
			if ( !value )
			{
				m_fGhostTime = gpGlobals->curtime;
			}
			else
			{
				m_fGhostTime = TIMER_INIT;
			}

			m_bIsSpotted = value;
		}
	}
}

void CCSGO_MapOverview::CCSGO_MapOverviewIconPackage::SetPlayerTeam( int team )
{
	if ( m_iPlayerTeam != team )
	{
		m_iPlayerTeam = team;
		SetVisibilityFlags( 0 );
	}
}

int CCSGO_MapOverview::CCSGO_MapOverviewIconPackage::GetPlayerType()
{
	int newType = 0;

	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( pLocalPlayer == NULL )
	{
		return newType;
	}

	switch ( m_iPlayerTeam )
	{
	case TEAM_UNASSIGNED:
		newType = PI_HOSTAGE;
		break;

	case TEAM_TERRORIST:
		newType = pLocalPlayer->GetAssociatedTeamNumber() == TEAM_TERRORIST || pLocalPlayer->IsObserver() ? PI_PLAYER : PI_ENEMY;
		break;

	case TEAM_CT:
		newType = pLocalPlayer->GetAssociatedTeamNumber() == TEAM_CT || pLocalPlayer->IsObserver() ? PI_PLAYER : PI_ENEMY;
		break;

	case TEAM_SPECTATOR:
	default:
		newType = 0;
		break;
	}

	return newType;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::CCSGO_MapOverviewIconPackage::SetupIconsFromStates( void )
{
	uint64 flags = 0;
	float alpha = 1;

	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( pLocalPlayer == NULL )
		return;

	bool bShowAll = false;
	if ( pLocalPlayer->IsSpectator() || g_bEngineIsHLTV )
		bShowAll = true;

	int iPlayerType = GetPlayerType();
	if ( iPlayerType != 0 )
	{
		flags = 1ll << iPlayerType;

		if ( m_bIsDead )
		{
			if ( m_Position.x == 0 && m_Position.y == 0 || !m_bIsSpotted )
			{
				flags = 0;
			}
			else
			{
				float deadElapsedTime = gpGlobals->curtime - m_fDeadTime;
				if ( deadElapsedTime > DEAD_FADE_TIME )
				{
					m_fDeadTime = TIMER_INIT;
					flags = 0;
				}
				else
				{
					alpha = clamp( 1.0f - deadElapsedTime / DEAD_FADE_TIME, 0.0f, 1.0f );

					// the shift below changes the icon to be the dead icon ( the X )
					// rescued hostages are marked as dead, but we don't want to show
					// the X in that case.  So if this guy is a hostage and has been resuced
					// we want to skip the code that changes the icon to the dead icon.

					bool isRescuedNotDead = IsHostageType() && m_bIsRescued;

					if ( !isRescuedNotDead )
					{
						flags <<= 1;
					}
				}
			}
		}
		else if ( !m_bIsSpotted )
		{
			float ghostElapsedTime = gpGlobals->curtime - m_fGhostTime;
			if ( ghostElapsedTime > GHOST_FADE_TIME )
			{
				m_fGhostTime = TIMER_INIT;
				flags = 0;
			}
			else
			{
				alpha = clamp( 1.0f - ghostElapsedTime / GHOST_FADE_TIME, 0.0f, 1.0f );
				// shifting to show the "ghost" icon
				flags <<= 2;
			}
		}
		else
		{
			if ( m_bIsMovingHostage )
			{
				flags |= 1 << PI_HOSTAGE_MOVING;
			}

			if ( m_bIsSpeaking )
			{
				flags |= 1 << PI_SPEAKING;
			}

			if ( m_bIsLowHealth )
			{
				flags |= 1 << PI_LOWHEALTH;
			}

			if ( m_bIsSelected )
			{
				flags |= 1 << PI_SELECTED;
			}

			if ( m_bIsFlashed )
			{
				flags |= 1 << PI_FLASHED;
			}

			if ( m_bIsFiring )
			{
				flags |= 1 << PI_MUZZLE_FLASH;
			}

			bool bSameTeamOrSpec = false;
			if ( m_iPlayerTeam == pLocalPlayer->GetAssociatedTeamNumber() || bShowAll )
			{
				flags |= 1 << PI_PLAYER_NAME;
				bSameTeamOrSpec = true;
			}

			if ( bSameTeamOrSpec )
				flags |= 1 << PI_PLAYER_NUMBER;


			flags |= 1 << PI_DIRECTION_INDICATOR;

			if ( m_bIsPlayer )
			{
				flags |= 1 << PI_PLAYER_INDICATOR;
			}

			if ( m_fGrenExpireTime != TIMER_INIT )
			{
				if ( m_fGrenExpireTime < gpGlobals->curtime )
				{
					m_fGrenExpireTime = TIMER_INIT;
					m_bIsSpotted = false;
				}
			}
		}
	}
	else if ( m_bIsDefuser )
	{
		// only render after we have received a position update
		if ( m_Position != vec3_origin )
		{
			flags |= 1 << PI_DEFUSER;

			if ( m_bIsSpotted )
			{
				m_fGhostTime = TIMER_INIT;
				alpha = 1.0f;
			}
			else
			{
				float ghostElapsedTime = gpGlobals->curtime - m_fGhostTime;
				if ( ghostElapsedTime > GHOST_FADE_TIME )
				{
					m_fGhostTime = TIMER_INIT;
					flags = 0;
				}
				else
				{
					alpha = clamp( 1.0f - ghostElapsedTime / GHOST_FADE_TIME, 0.0f, 1.0f );
				}
			}
		}
	}
	else if ( m_nGrenadeType > -1 )
	{
		flags = 1ll << m_nGrenadeType;

		float flFadeTime = 1.0f;

		float deadElapsedTime = ( m_fGrenExpireTime <= 0 ) ? 0 : gpGlobals->curtime - m_fGrenExpireTime;
		if ( deadElapsedTime > flFadeTime )
		{
			m_fGrenExpireTime = TIMER_INIT;
			flags = 0;
		}
		else
		{
			alpha = clamp( 1.0f - deadElapsedTime / flFadeTime, 0.0f, 1.0f );
		}
	}

	SetAlpha( alpha );
	SetVisibilityFlags( flags );
	
	UpdateIconsPosition();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::CCSGO_MapOverviewIconPackage::SetVisibilityFlags( uint64 newFlags )
{
	newFlags &= ( ( 1ll << PI_NUM_ICONS ) - 1 );

	int diffFlags = m_iCurrentVisibilityFlags ^ newFlags;

	if ( diffFlags )
	{

		if ( m_pIconPackage )
		{
			bool bWantVisible = ( newFlags != 0 );
			if ( ( m_iCurrentVisibilityFlags != 0 ) != bWantVisible )
			{
				m_pIconPackage->SetVisible( bWantVisible );
			}

			for ( uint64 i = 0; i < PI_NUM_ICONS && ( diffFlags != 0 ); i++, diffFlags >>= 1 )
			{
				if ( ( diffFlags & 1 ) && ( m_pIcons[ i ] ) )
				{
					m_pIcons[ i ]->SetVisible( ( newFlags & ( 1ll << i ) ) != 0 );
				}
			}
		}

		m_iCurrentVisibilityFlags = newFlags;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::CCSGO_MapOverviewIconPackage::UpdateIconsPosition( void )
{
	if ( m_flIconScale != mapoverview_icon_scale.GetFloat() && m_pIconPackage )
	{
		m_flIconScale = mapoverview_icon_scale.GetFloat();
		Vector vecScale( m_flIconScale, m_flIconScale, 1.f );
		m_pIconPackage->AccessStyleDirty()->SetUIScale( vecScale );
	}

	Vector vecPosition = m_HudPosition / m_flIconScale;
	if ( m_pIconPackageNonRotate )
	{
		Helper_SetMapOverviewPanelTransform3D( m_pIconPackageNonRotate, vecPosition, 0.f, 0.f );
	}

	if ( m_pIconPackageRotate )
	{
		Helper_SetMapOverviewPanelTransform3D( m_pIconPackageRotate, vecPosition, m_HudRotation, 0.f );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::CCSGO_MapOverviewIconPackage::SetAlpha( float newAlpha )
{
	if ( newAlpha != m_fCurrentAlpha )
	{
		m_fCurrentAlpha = newAlpha;

 		if ( m_pIconPackage )
 		{
			m_pIconPackage->SetOpacity( m_fCurrentAlpha );
 		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::CCSGO_MapOverviewIconPackage::SetIsDefuse( bool bValue )
{
	m_bIsDefuser = bValue;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::CCSGO_MapOverviewIconPackage::SetGrenadeType( int nValue )
{
	m_nGrenadeType = nValue;
}

/**********************************************************************
**********************************************************************
**********************************************************************
**********************************************************************
**********************************************************************
* CCSGO_MapOverviewHostageIcon
*/

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCSGO_MapOverview::CCSGO_MapOverviewHostageIcons::CCSGO_MapOverviewHostageIcons()
{
	m_iCurrentIcon = HI_UNUSED;

	m_pIconPackage = NULL;
	V_memset( m_pIcons, 0, sizeof( m_pIcons ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCSGO_MapOverview::CCSGO_MapOverviewHostageIcons::~CCSGO_MapOverviewHostageIcons()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::CCSGO_MapOverviewHostageIcons::Init( panorama::CPanel2D *pParent, const char *szHostageIconName )
{
	if ( m_pIconPackage )
		return;

	if ( !pParent )
		return;

	m_pIconPackage = new panorama::CPanel2D( pParent, szHostageIconName );

	m_pIconPackage->RequireLoadLayoutSnippet( "HostageIconPackage" );

	for ( int i = 0; i < HI_NUM_ICONS && hostageIconNames[ i ] != NULL; i++ )
	{
		m_pIcons[ i ] = panorama::panel_cast<panorama::CImagePanel *>( m_pIconPackage->RequireChildInLayoutFile( hostageIconNames[ i ] ) );
		
		if ( m_pIcons[ i ] )
		{
			m_pIcons[ i ]->SetVisible( m_iCurrentIcon == i );
		}
	}

	m_pIconPackage->SetVisible( m_iCurrentIcon != HI_UNUSED );

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::CCSGO_MapOverviewHostageIcons::SetStatus( int status )
{
	if ( status == m_iCurrentIcon )
		return;

	if ( !m_pIconPackage )
		return;

	if ( m_iCurrentIcon != HI_UNUSED )
	{
		Assert( m_pIcons[ m_iCurrentIcon ] );
		m_pIcons[ m_iCurrentIcon ]->SetVisible( false );
	}
	else
	{
		m_pIconPackage->SetVisible( true );
	}

	m_iCurrentIcon = status;

	if ( m_iCurrentIcon != HI_UNUSED )
	{
		Assert( m_pIcons[ m_iCurrentIcon ] );
		m_pIcons[ m_iCurrentIcon ]->SetVisible( true );
	}
	else
	{
		m_pIconPackage->SetVisible( false );
	}
}

/*********************************************************************
**********************************************************************
**********************************************************************
**********************************************************************
**********************************************************************/
//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CCSGO_MapOverview::CCSGO_MapOverview( CPanel2D *pParent, const char *pchID )
	: CPanoramaHudElement( "CCSGO_MapOverview", this ),
	panorama::CPanel2D( pParent, pchID ),

	m_fMapSize( 1.0f ),
	m_fMapScale( 1.0f ),
	m_fRadarSize( 1.0f ),
	m_fPixelToRadarScale( 1.0f ),
	m_fWorldToPixelScale( 1.0f ),
	m_fWorldToRadarScale( 1.0f ),
	m_fRadarPanelSize( 300.0f ),
	m_fHudPosRadarPanelCenterOffset( 150.0f ),
	m_pMapOverview( NULL ),
	m_iNumGoalIcons( 0 ),
	m_iLastPlayerIndex( INVALID_INDEX ),
	m_iLastHostageIndex( INVALID_INDEX ),
	m_iLastGrenadeIndex( INVALID_INDEX ),
	m_iLastDefuserIndex( INVALID_INDEX ),
	m_iLastSurvivalCrateIndex( INVALID_INDEX ),
	m_RadarViewpointWorld( vec3_origin ),
	m_RadarViewpointMap( vec3_origin ),
	m_RadarRotation( 0 ),
	m_BombPosition( vec3_origin ),
	m_DefuserPosition( vec3_origin ),
	m_iCurrentVisibilityFlags( 0 ),
	m_fBombSeenTime( TIMER_INIT ),
	m_fBombAlpha( 0 ),
	m_fDefuserSeenTime( TIMER_INIT ),
	m_fDefuserAlpha( 0.0f ),
	m_bGotPlayerIcons( false ),
	m_bShowingHostageZone( false ),
	m_bBombPlanted( false ),
	m_bBombDropped( false ),
	m_nBombEntIndex( -1 ),
	m_bShowBombHighlight( false ),
	m_bShowMapOverview( false ),
	m_bShowAll( false ),
	m_bShowingDashboard( false ),
	m_iObserverMode( OBS_MODE_NONE ),
	m_bTrackDefusers( false ),
	m_MapOrigin( vec3_origin ),
	m_fMapSourceImageSize( 1024.0f )
{
	m_bWantLateUpdate = true;

	Init();

	RequireLoadLayout( "file://{resources}/layout/mapoverview.xml" );

	RegisterForUnhandledEvent( panorama::ReloadStyleFile(), this, &CCSGO_MapOverview::OnStyleFileReloaded );
	GetLayoutDefines();

	SetHiddenBits( HIDEHUD_RADAR );
	m_bWantLateUpdate = true;

	m_szMapName[ 0 ] = 0;
	m_szLocationString[ 0 ] = 0;

	V_memset( m_pBombZoneIcons, 0, sizeof( m_pBombZoneIcons ) );
	V_memset( m_pHostageZoneIcons, 0, sizeof( m_pHostageZoneIcons ) );
	V_memset( m_GoalIcons, 0, sizeof( m_GoalIcons ) );
	V_memset( m_HostageStatusIcons, 0, sizeof( m_HostageStatusIcons ) );
	V_memset( m_pAllIcons, 0, sizeof( m_pAllIcons ) );

	m_EntitySpotted.ClearAll();

	m_pMapOverview = panorama::panel_cast<CPanel2D *>( FindChildInLayoutFile( "MapOverview" ) );
	m_pMap = panorama::panel_cast<CImagePanel *>( FindChildInLayoutFile( "MapOverview__Map" ) );
	if( m_pMap )
	{
		RegisterEventHandlerOnPanel( ImageLoaded(), m_pMap->UIPanel(), this, &CCSGO_MapOverview::OnMapImageLoaded );
		RegisterEventHandlerOnPanel( ImageFailedLoad(), m_pMap->UIPanel(), this, &CCSGO_MapOverview::OnMapImageFailedLoad );
	}

	m_pMapSurvivalDangerZone = panorama::panel_cast<CImagePanel *>(FindChildInLayoutFile( "MapOverview__DangerZone" ));
	m_pBombDefuserPackage = panorama::panel_cast<CPanel2D *>( RequireChildInLayoutFile( "RI_BombDefuserPackage" ) );

	m_pAllIcons[ RI_BOMB_IS_PLANTED ] = NULL;// panorama::panel_cast<CPanel2D *>( RequireChildInLayoutFile( "BombPlantedIcon" ) );
	m_pAllIcons[ RI_BOMB_IS_PLANTED_MEDIUM ] = NULL;// panorama::panel_cast<CPanel2D *>( RequireChildInLayoutFile( "BombPlantedIconMedium" ) );
	m_pAllIcons[ RI_BOMB_IS_PLANTED_FAST ] = NULL;// panorama::panel_cast<CPanel2D *>( RequireChildInLayoutFile( "BombPlantedIconFast" ) );
	m_pAllIcons[ RI_IN_HOSTAGE_ZONE ] = panorama::panel_cast<CPanel2D *>( RequireChildInLayoutFile( "Dashboard__HZone" ) );
	m_pAllIcons[ RI_DASHBOARD ] = NULL;// panorama::panel_cast<CPanel2D *>( RequireChildInLayoutFile( "Dashboard" ) );
	m_pAllIcons[ RI_BOMB_ICON_PLANTED ] = panorama::panel_cast<CPanel2D *>( RequireChildInLayoutFile( "PlantedBomb" ) );
	m_pAllIcons[ RI_BOMB_ICON_DROPPED ] = panorama::panel_cast<CPanel2D *>( RequireChildInLayoutFile( "DroppedBomb" ) );
	m_pAllIcons[ RI_BOMB_ICON_BOMB_CT ] = NULL;// panorama::panel_cast<CImagePanel *>( RequireChildInLayoutFile( "BombCT" ) );
	m_pAllIcons[ RI_BOMB_ICON_BOMB_T ] = NULL;// panorama::panel_cast<CPanel2D *>( RequireChildInLayoutFile( "Dashboard__BombT" ) );
	m_pAllIcons[ RI_BOMB_ICON_BOMB_ABOVE ] = panorama::panel_cast<CPanel2D *>( RequireChildInLayoutFile( "BombAbove" ) );
	m_pAllIcons[ RI_BOMB_ICON_BOMB_BELOW ] = panorama::panel_cast<CPanel2D *>( RequireChildInLayoutFile( "BombBelow" ) );
	m_pAllIcons[ RI_BOMB_ICON_PACKAGE ] = panorama::panel_cast<CPanel2D *>( RequireChildInLayoutFile( "CreateBombPack" ) );
	m_pAllIcons[ RI_DEFUSER_ICON_DROPPED ] = panorama::panel_cast<CPanel2D *>( RequireChildInLayoutFile( "DefuserIconDropped" ) );
	m_pAllIcons[ RI_DEFUSER_ICON_PACKAGE ] = panorama::panel_cast<CPanel2D *>( RequireChildInLayoutFile( "DefuserIconPackage" ) );
	//m_pAllIcons[RI_SURVIVAL_DROP_ICON_PACKAGE] = panorama::panel_cast< CPanel2D * >( RequireChildInLayoutFile( "SurvivalDropCrate" ) );

	
	char cHostageZoneIconName[ 20 ] = { "HZone0" };

	for ( int i = 0; i < MAX_HOSTAGE_RESCUES; i++ )
	{
		cHostageZoneIconName[ 5 ] = '0' + i;

		m_pHostageZoneIcons[ i ] = panorama::panel_cast<CImagePanel *>( RequireChildInLayoutFile( cHostageZoneIconName ) );
	}

	m_pBombZoneIcons[ 0 ] = panorama::panel_cast<CImagePanel *>( RequireChildInLayoutFile( "BombZoneA" ) );
	m_pBombZoneIcons[ 1 ] = panorama::panel_cast<CImagePanel *>( RequireChildInLayoutFile( "BombZoneB" ) );

	// Focus/input stuff
	SetTopOfInputContext( true );
	SetInputNamespace( "csgo_mapoverview" );
	SetAcceptsInput( true );
	SetAcceptsFocus( true );
	UIPanel()->SetCanClearFocusByClicking( false );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CCSGO_MapOverview::~CCSGO_MapOverview()
{
	UnregisterForUnhandledEvent( panorama::ReloadStyleFile(), this, &CCSGO_MapOverview::OnStyleFileReloaded );
}

//-----------------------------------------------------------------------------
// fRadarSize - size of map within radar in pixels relative to 1080
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::UpdateRadarScalingValues( float fRadarVisSize, float fRadarPanelSize )
{
	m_fRadarSize = fRadarVisSize;
	m_fMapSize = SF_TO_PANORAMA_RADAR_SCALE * m_fRadarSize;

	m_fPixelToRadarScale = m_fMapSize / m_fMapSourceImageSize; 

	m_fWorldToRadarScale = m_fWorldToPixelScale * m_fPixelToRadarScale;

	m_fRadarPanelSize = fRadarPanelSize;
	m_fHudPosRadarPanelCenterOffset = m_fRadarPanelSize * 0.5f;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::GetLayoutDefines()
{
	int nRadarMapVisSize, nRadarPanelSize;

	nRadarMapVisSize = GetLayoutFileDefineInt( "MapOverview_VisSize", 290 );

	nRadarPanelSize = GetLayoutFileDefineInt( "MapOverview_PanelSize", 300 );

	UpdateRadarScalingValues( (float)nRadarMapVisSize, (float)nRadarPanelSize );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CCSGO_MapOverview::OnStyleFileReloaded( panorama::CPanoramaSymbol symFile )
{
	GetLayoutDefines();

	// let bubble
	return false;
}

//-----------------------------------------------------------------------------
// hud element functions
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::Init()
{
	// register for events as client listener

	const char** pwalk = &desiredMessageNames[ 0 ];

	while ( *pwalk != NULL )
	{
		ListenForGameEvent( *pwalk );
		pwalk++;
	}

	if ( !m_messageMap.Count() )
	{
		pwalk = &desiredMessageNames[ 0 ];
		int i = 0;

		while ( *pwalk != NULL )
		{
			m_messageMap.Insert( *pwalk++, i++ );
		}
	}

	HOOK_HUD_MESSAGE( CCSGO_MapOverview, ProcessSpottedEntityUpdate );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::LevelInit( void )
{
 	for ( int i = 0; i < RI_NUM_ICONS; i++ )
 	{
 		if ( m_pAllIcons[ i ] )
 		{
			m_pAllIcons[ i ]->SetVisible( false );
 		}
 	}

 	ResetForNewMap();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::LevelShutdown( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCSGO_MapOverview::ShouldDraw( void )
{
	if( IsTakingAFreezecamScreenshot() || !m_bShowMapOverview )
		return false;

	return cl_drawhud.GetBool();// && CHudElement::ShouldDraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCSGO_MapOverview::CanShowOverview( void )
{
	CBasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	if ( pLocalPlayer && ( pLocalPlayer->GetObserverMode() == OBS_MODE_NONE ||
						   pLocalPlayer->GetObserverMode() == OBS_MODE_DEATHCAM || pLocalPlayer->GetObserverMode() == OBS_MODE_FREEZECAM ) )
	{
		return false;
	}

	if ( g_HltvReplaySystem.GetHltvReplayDelay() )
		return false;

	auto pSurvivalRules = CSGameRules() ? CSGameRules()->GetSurvivalRules() : nullptr;
	if ( pLocalPlayer && pLocalPlayer->GetTeamNumber() != TEAM_SPECTATOR && pSurvivalRules && pSurvivalRules->IsPlayingTeamMode() )
	{
		PlayerTeammateVector_t aliveTeammates;
		pSurvivalRules->GetPlayerTeammates( ToCSPlayer( pLocalPlayer ), aliveTeammates, true );
		if ( aliveTeammates.Count() > 0 )
		{
			panorama::DispatchEvent( ShowCenterPrintText(), nullptr, "#Hint_Survival_MapOverview", CCenterPrint::k_EPriority_Low );
			return false;
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::SetActive( bool bActive )
{
	if ( bActive == true && !CanShowOverview() )
		bActive = false;

	if( bActive != BIsVisible() )
		Show( bActive );

	CHudElement::SetActive( bActive );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::Show( bool bShow )
{
	if ( bShow == true && !CanShowOverview() )
		bShow = false;

	if ( bShow == BIsVisible() )
		return;

	SetVisible( bShow );
	UpdateAllPlayers();

	// Input control
	if( bShow )
	{
		SetFocus(); // will add to input context stack
	}
	else
	{
		GetParentWindow()->UIWindowInput()->RemoveInputContext( this->UIPanel() );
	}
}

bool CCSGO_MapOverview::OnKeyDown( const panorama::KeyData_t &key )
{
	if ( !BIsVisible() )
		return false;

	if ( key.m_KeyCode == panorama::KEY_ESCAPE )
	{
		ShowMapOverview( false );
		return true;
	}

	return BaseClass::OnKeyTyped( key );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::LazyCreateGoalIcons( void )
{
	if ( m_bGotGoalIcons )
	{
		return;
	}

	// The goal entities don't exist on the client, so we have to get them from the CS Resource.
	C_CS_PlayerResource *pCSPR = (C_CS_PlayerResource*)GameResources();
	if ( !pCSPR )
	{
		return;
	}

	m_pBombZoneIcons[ 0 ]->SetVisible( false );
	m_pBombZoneIcons[ 1 ]->SetVisible( false );
	for ( int i = 0; i < MAX_HOSTAGE_RESCUES; i++ )
	{
		m_pHostageZoneIcons[ i ]->SetVisible( false );
	}

	m_iNumGoalIcons = 0;

	if ( CSGameRules()->IsBombDefuseMap() )
	{
		Vector bombA = pCSPR->GetBombsiteAPosition();
		if ( bombA != vec3_origin )
		{
			m_GoalIcons[ m_iNumGoalIcons ].m_Position = bombA;
			m_GoalIcons[ m_iNumGoalIcons ].m_pIcon = m_pBombZoneIcons[ 0 ];
			m_GoalIcons[ m_iNumGoalIcons ].m_pIcon->SetVisible( true );
			m_iNumGoalIcons++;
		}

		Vector bombB = pCSPR->GetBombsiteBPosition();
		if ( bombB != vec3_origin )
		{
			m_GoalIcons[ m_iNumGoalIcons ].m_Position = bombB;
			m_GoalIcons[ m_iNumGoalIcons ].m_pIcon = m_pBombZoneIcons[ 1 ];
			m_GoalIcons[ m_iNumGoalIcons ].m_pIcon->SetVisible( true );
			m_iNumGoalIcons++;
		}
	}
	else if ( CSGameRules()->IsHostageRescueMap() )
	{
		for ( int rescueIndex = 0; rescueIndex < MAX_HOSTAGE_RESCUES; rescueIndex++ )
		{
			m_pHostageZoneIcons[ rescueIndex ]->SetVisible( false );

			int hIndex = 0;
			Vector hostageI = pCSPR->GetHostageRescuePosition( rescueIndex );
			if ( hostageI != vec3_origin )
			{
				m_GoalIcons[ m_iNumGoalIcons ].m_Position = hostageI;


				m_GoalIcons[ m_iNumGoalIcons ].m_pIcon = m_pHostageZoneIcons[ hIndex++ ];
				m_GoalIcons[ m_iNumGoalIcons ].m_pIcon->SetVisible( true );

				m_iNumGoalIcons++;
			}
		}
	}

	m_bGotGoalIcons = true;

}

//-----------------------------------------------------------------------------
// Purpose: called when our map image is ready to display
//-----------------------------------------------------------------------------
bool CCSGO_MapOverview::OnMapImageLoaded( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, panorama::IImageSource *pImage )
{
	if ( !m_pMap )
		return false;

	if ( pPanel.Get() == m_pMap->UIPanel() )
	{
		//InvalidateSizeAndPosition();
		if ( m_pMap->GetImage() == pImage )
		{
			m_fMapSourceImageSize = m_pMap->GetImage()->GetWidth();
			UpdateRadarScalingValues( m_fRadarSize, m_fRadarPanelSize );
		}

	}

	UpdateDangerZoneOverlayVisibility();
	// Always return false so CImagePanel handler gets called next
	return false;
}

void CCSGO_MapOverview::UpdateDangerZoneOverlayVisibility()
{
	if ( m_pMapSurvivalDangerZone )
	{
		CDangerZoneController *pZone = GetDangerZoneController();
		if ( pZone )
		{
			// CSGO-2818: We really only want to show this overlay if we've drawn to the _rt_ZoneProjectionSpectator... 
			// testing if danger zone is enabled is roughly equivalent. 
			bool bShow = CSGameRules()->IsPlayingSurvival() && pZone->IsMasterDangerZoneEnabled();
			if ( bShow != m_pMapSurvivalDangerZone->BIsVisible() )
				m_pMapSurvivalDangerZone->SetVisible( bShow );
		}
	}
}

bool CCSGO_MapOverview::OnMapImageFailedLoad( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, panorama::IImageSource *pImage )
{
	// set up load using default (non _spectate) version of overview map
	if ( !m_pMap )
		return false;

	if ( pPanel.Get() == m_pMap->UIPanel() )
	{
		CUtlString szImageURL;
		szImageURL.Format( "file://{images_overviews}/%s_radar.dds", m_szMapName );

		m_pMap->SetImage( szImageURL );
		m_pMap->SetVisible( true );

		if ( m_pMap->GetImage()->BIsLoaded() )
		{
			m_fMapSourceImageSize = m_pMap->GetImage()->GetWidth();
		}

	}

	// Always return false so other handlers also get called
	return false;
}


//-----------------------------------------------------------------------------
// set up the background map
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::SetMap( const char* pMapName )
{
	V_strcpy( m_szMapName, pMapName );

	KeyValues* MapKeyValues = new KeyValues( pMapName );
	KeyValues::AutoDelete MapKeyvaluesDelete( MapKeyValues );

	char tempfile[MAX_PATH];
	Q_snprintf(tempfile, sizeof(tempfile), "resource/overviews/%s.txt", pMapName);

	if (!MapKeyValues->LoadFromFile(g_pFullFileSystem, tempfile, "GAME"))
	{
		DevMsg(1, "Error! CMapOverview::SetMap: couldn't load file %s.\n", tempfile);
	}

	// load _spectate version if available (see OnMapImageFailedLoad for fallback)
	CUtlString szImageURL;
	szImageURL.Format( "file://{images_overviews}/%s_radar_spectate.dds", pMapName );

	if ( m_pMap )
	{
		m_pMap->SetImage( szImageURL );
		m_pMap->SetVisible( true );

		if ( m_pMap->GetImage()->BIsLoaded() )
		{
			m_fMapSourceImageSize = m_pMap->GetImage()->GetWidth();
		}
	}

	m_MapOrigin.x = MapKeyValues->GetInt( "pos_x", 0 );
	m_MapOrigin.y = MapKeyValues->GetInt( "pos_y", 0 );
	m_MapOrigin.z = 0;
	m_fMapScale = MapKeyValues->GetFloat( "scale", 1.0f );
	m_fWorldToPixelScale = 1.0f / m_fMapScale;

	UpdateRadarScalingValues( m_fRadarSize, m_fRadarPanelSize );
}

//-----------------------------------------------------------------------------
// reset functions
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::ResetForNewMap(void)
{
	int i;

	for ( i = 0; i < ( MAX_HOSTAGE_RESCUES + MAX_BOMB_ZONES ); i++ )
	{
		if ( m_GoalIcons[ i ].m_pIcon )
			m_GoalIcons[ i ].m_pIcon->SetVisible( false );
	}

	while ( m_iLastHostageIndex >= 0 )
	{
		RemoveHostage( m_iLastHostageIndex );
	}

	int index = 0;
 	while ( m_HostageStatusIcons[ index ].m_pIconPackage && ( index < MAX_HOSTAGES ) )
	{
 		m_HostageStatusIcons[ index++ ].SetStatus( CCSGO_MapOverviewHostageIcons::HI_UNUSED );
 	}

	RemoveAllGrenades();
	RemoveAllDefusers();

	m_iNumGoalIcons = 0;
	m_bGotGoalIcons = false;

	for ( i = 0; i <= m_iLastPlayerIndex; i++)
	{
		ResetPlayer( i );
	}


	ResetRoundVariables();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::ToggleOverviewMap()
{
	ShowMapOverview( !IsMapOverviewShown() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::ResetRoundVariables(bool bResetGlobalStates)
{
	SetVisibilityFlags( 0 );
//	SetLocationText( NULL );

	m_RadarViewpointWorld = vec3_origin;
	m_RadarViewpointMap = vec3_origin;
	m_RadarRotation = 0;
	m_fBombSeenTime = TIMER_INIT;
	m_fBombAlpha = 0.0f;
	m_fDefuserSeenTime = TIMER_INIT;
	m_fDefuserAlpha = 0.0f;
	m_bShowingHostageZone = false;

	m_bShowBombHighlight = false;
	m_bShowingDashboard = false;
	m_bShowAll = false;
	m_iObserverMode = OBS_MODE_NONE;
	ApplySpectatorModes(); // reapply here to make sure it stays when it's supposed to

	//TODO - resolve warning ConVarRef mp_defuser_allocation("mp_defuser_allocation");
	m_bTrackDefusers = ( mp_defuser_allocation.GetInt() == DefuserAllocation::Random );

	if ( bResetGlobalStates )
	{
		m_nBombEntIndex = -1;
		m_BombPosition = vec3_origin;
		m_DefuserPosition = vec3_origin;
		m_bBombIsSpotted = false;
		m_bBombPlanted = false;
		m_bBombDropped = false;
		m_bBombExploded = false;
		m_bBombDefused = false;
	}

	m_EntitySpotted.ClearAll();

	CBasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	if ( pLocalPlayer && ( pLocalPlayer->GetObserverMode() == OBS_MODE_NONE ) )
		ShowMapOverview( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::ShowMapOverview( bool bValue )
{
	if ( bValue == true && !CanShowOverview() )
		return;

	ClientModeShared *mode = (ClientModeShared *)GetClientModeNormal();
	if ( mode )
	{
		mode->ReloadScheme();
	}

	m_bShowMapOverview = bValue;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::ResetRadar( bool bResetGlobalStates )
{
	for (int i = 0; i <= m_iLastPlayerIndex; i++)
	{
		ResetPlayer(i);
	}

	if (CSGameRules()->IsHostageRescueMap())
	{
		RemoveStaleHostages();

		for (int i = 0; i <= m_iLastHostageIndex; i++)
		{
			ResetHostage(i);
		}

		int index = 0;

		while ( m_HostageStatusIcons[ index ].m_pIconPackage && ( index < MAX_HOSTAGES ) )
 		{
 			m_HostageStatusIcons[ index++ ].SetStatus( CCSGO_MapOverviewHostageIcons::HI_UNUSED );
 		}
	}

	if ( bResetGlobalStates )
	{
		RemoveAllGrenades();
		RemoveAllDefusers();
	}

	ResetRoundVariables( bResetGlobalStates );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::ResetRound(void)
{
	ResetRadar(true);

	RefreshGraphs();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::RefreshGraphs( void )
{
// TODO
//	if ( RadarModule.Graph._visible == true && RadarModule.Graph._alpha == 100 )
//		RadarModule.Graph.onRefresh();
}

// TODO
// function onRefresh()
// {
// 	m_strTypeDropdown = _global.CScaleformComponent_MatchStats.GetRangeNameByIndex( _global.CScaleformComponent_MatchStats.GetDesiredPage() );
// 	GetGraphTypes();
// 	SetGraphDropdownType( m_strTypeDropdown );
// }

//-----------------------------------------------------------------------------
// player icon loading / creating
//-----------------------------------------------------------------------------
bool CCSGO_MapOverview::LazyUpdateIconArray( CCSGO_MapOverviewIconPackage* pArray, int lastIndex )
{
	bool result = true;
	int i;
	CCSGO_MapOverviewIconPackage* pwalk;

	for ( pwalk = pArray, i = 0; i <= lastIndex; i++, pwalk++ )
	{
		if ( pwalk->m_bIsActive && !pwalk->m_pIconPackage )
		{
			if ( !LazyCreateIconPackage( pwalk ) )
			{
				// keep going, but remember that at least one wasn't loaded correctly
				result = false;
			}
		}
	}

	return result;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::LazyCreatePlayerIcons( void )
{
	if ( m_bGotPlayerIcons )
		return;

	// the following code looks odd, but it insures that all three of the LazyUpdate calls are made
	// even if m_bGotPlayerIcons becomes false early.

	m_bGotPlayerIcons = LazyUpdateIconArray( m_Players, m_iLastPlayerIndex );

	m_bGotPlayerIcons = LazyUpdateIconArray( m_Hostages, m_iLastHostageIndex ) && m_bGotPlayerIcons;

	m_bGotPlayerIcons = LazyUpdateIconArray( m_Grenades, m_iLastGrenadeIndex ) && m_bGotPlayerIcons;

	m_bGotPlayerIcons = LazyUpdateIconArray( m_Defusers, m_iLastDefuserIndex ) && m_bGotPlayerIcons;

	m_bGotPlayerIcons = LazyUpdateIconArray( m_SurvivalCrates, m_iLastSurvivalCrateIndex ) && m_bGotPlayerIcons;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCSGO_MapOverview::LazyCreateIconPackage( CCSGO_MapOverviewIconPackage* pIconPack )
{
	if ( pIconPack->m_bIsActive && !pIconPack->m_pIconPackage )
 	{
		pIconPack->Init( m_pMapOverview );

 		return true;
 	}
 	else
 	{
 		return false;
 	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::InitIconPackage( CCSGO_MapOverviewIconPackage* pPlayer, int iAbsoluteIndex, ICON_PACK_TYPE iconType )
{
	if ( pPlayer->m_bIsActive )
	{
		RemoveIconPackage( pPlayer );
	}

	pPlayer->m_IconPackType = iconType;
	pPlayer->m_iIndex = iAbsoluteIndex;
	pPlayer->m_bIsActive = true;

	pPlayer->StartRound();

 	if ( !pPlayer->m_pIconPackage && !LazyCreateIconPackage( pPlayer ) )
 	{
 		m_bGotPlayerIcons = false;
 	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::RemoveIconPackage( CCSGO_MapOverviewIconPackage* pPackage )
{
 	if ( pPackage->m_bIsActive )
	{
		pPackage->NukeFromOrbit( this );
	}
}



//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCSGO_MapOverview::CCSGO_MapOverviewIconPackage* CCSGO_MapOverview::CreatePlayer( int index )
{
	CCSGO_MapOverviewIconPackage* pPackage = GetRadarPlayer( index );
	InitIconPackage( pPackage, index, ICON_PACK_PLAYER );

	m_iLastPlayerIndex = MAX( index, m_iLastPlayerIndex );

	return pPackage;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::ResetPlayer( int index )
{
	CCSGO_MapOverviewIconPackage* pPackage = GetRadarPlayer( index );
	if ( pPackage->m_bIsActive )
	{
		pPackage->StartRound();
	}

	UpdatePlayer( pPackage );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::RemovePlayer( int index )
{
	RemoveIconPackage( GetRadarPlayer( index ) );

	if ( index == m_iLastPlayerIndex )
	{
		while ( m_iLastPlayerIndex >= 0 && !m_Players[ m_iLastPlayerIndex ].m_bIsActive )
		{
			m_iLastPlayerIndex--;
		}
	}

	UpdateAllPlayers();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCSGO_MapOverview::CCSGO_MapOverviewIconPackage* CCSGO_MapOverview::CreateHostage( int index )
{
	CCSGO_MapOverviewIconPackage* pPackage = GetRadarHostage( index );
	InitIconPackage( pPackage, index, ICON_PACK_HOSTAGE );

	m_iLastHostageIndex = MAX( index, m_iLastHostageIndex );

	return pPackage;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::ResetHostage( int index )
{
	CCSGO_MapOverviewIconPackage* pPackage = GetRadarHostage( index );
	if ( pPackage->m_bIsActive )
	{
		pPackage->StartRound();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::RemoveStaleHostages( void )
{
	// Remove hostages that are no longer tracked by the player resource (hostages that have died)

	C_CS_PlayerResource *pCSPR = (C_CS_PlayerResource*)GameResources();
	if ( !pCSPR )
	{
		return;
	}

	for ( int index = 0; index < MAX_HOSTAGES; index++ )
	{
		CCSGO_MapOverviewIconPackage* pPackage = GetRadarHostage( index );
		if ( pPackage->m_bIsActive )
		{
			bool bRemove = true;

			for ( int i = 0; i < MAX_HOSTAGES; i++ )
			{
				if ( pCSPR->GetHostageEntityID( i ) == pPackage->m_iEntityID )
				{
					bRemove = false;
					break;
				}
			}

			if ( bRemove )
			{
				RemoveHostage( index );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::RemoveHostage( int index )
{
	RemoveIconPackage( GetRadarHostage( index ) );

	if ( index == m_iLastHostageIndex )
	{
		while ( m_iLastHostageIndex >= 0 && !m_Hostages[ m_iLastHostageIndex ].m_bIsActive )
		{
			m_iLastHostageIndex--;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCSGO_MapOverview::CCSGO_MapOverviewIconPackage* CCSGO_MapOverview::CreateGrenade( int entityID, int grenadeType )
{
	CCSGO_MapOverviewIconPackage* pPackage = NULL;
	int index = m_iLastGrenadeIndex + 1;

	for ( int i = 0; i <= m_iLastGrenadeIndex; i++ )
	{
		if ( !m_Grenades[ i ].m_bIsActive )
		{
			index = i;
			break;
		}
	}

	if ( index < MAX_GRENADES )
	{
		pPackage = GetRadarGrenade( index );

		InitIconPackage( pPackage, index, ICON_PACK_GRENADES );

		pPackage->m_iEntityID = entityID;
		pPackage->m_fRoundStartTime = TIMER_INIT;

		m_iLastGrenadeIndex = MAX( index, m_iLastGrenadeIndex );
	}

	return pPackage;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::RemoveAllGrenades( void )
{
	for ( int i = 0; i <= m_iLastGrenadeIndex; i++ )
	{
		if ( m_Grenades[ i ].m_bIsActive )
		{
			RemoveGrenade( i );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::RemoveGrenade( int index )
{
	RemoveIconPackage( GetRadarGrenade( index ) );

	if ( index == m_iLastGrenadeIndex )
	{
		while ( m_iLastGrenadeIndex >= 0 && !m_Grenades[ m_iLastGrenadeIndex ].m_bIsActive )
		{
			m_iLastGrenadeIndex--;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCSGO_MapOverview::CCSGO_MapOverviewIconPackage* CCSGO_MapOverview::CreateSurvivalCrate( int entityID )
{
	CCSGO_MapOverviewIconPackage* pPackage = NULL;
	int index = m_iLastSurvivalCrateIndex + 1;

	for ( int i = 0; i <= m_iLastSurvivalCrateIndex; i++ )
	{
		if ( !m_SurvivalCrates[i].m_bIsActive )
		{
			index = i;
			break;
		}
	}

	if ( index < MAX_SURVIVAL_PARADROPS )
	{
		pPackage = GetRadarSurvivalCrate( index );

		InitIconPackage( pPackage, index, ICON_PACK_SURVIVAL_PARADROP );

		pPackage->m_iEntityID = entityID;
		pPackage->m_fRoundStartTime = TIMER_INIT;

		m_iLastSurvivalCrateIndex = MAX( index, m_iLastSurvivalCrateIndex );
	}

	return pPackage;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::RemoveAllSurvivalCrates( void )
{
	for ( int i = 0; i <= m_iLastSurvivalCrateIndex; i++ )
	{
		if ( m_SurvivalCrates[i].m_bIsActive )
		{
			RemoveSurvivalCrate( i );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::RemoveSurvivalCrate( int index )
{
	RemoveIconPackage( GetRadarSurvivalCrate( index ) );

	if ( index == m_iLastSurvivalCrateIndex )
	{
		while ( m_iLastSurvivalCrateIndex >= 0 && !m_SurvivalCrates[m_iLastSurvivalCrateIndex].m_bIsActive )
		{
			m_iLastSurvivalCrateIndex--;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCSGO_MapOverview::CCSGO_MapOverviewIconPackage * CCSGO_MapOverview::GetDefuser( int nEntityID, bool bCreateIfNotFound )
{
	CCSGO_MapOverviewIconPackage * pResult = NULL;

	int nIndex = GetDefuseIndexFromEntityID( nEntityID );

	if ( nIndex != INVALID_INDEX )
	{
		pResult = GetRadarDefuser( nIndex );
	}

	if ( pResult == NULL && bCreateIfNotFound )
	{
		pResult = CreateDefuser( nEntityID );
	}

	return pResult;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::SetDefuserPos( int nEntityID, int x, int y, int z, int a )
{
	if ( !m_bTrackDefusers )
	{
		return;
	}

	CCSGO_MapOverviewIconPackage * pDefuser = GetDefuser( nEntityID, true );

	AssertMsg( pDefuser != NULL, "Defuser not found. Update failed." );

	if ( pDefuser )
	{
		pDefuser->m_Position.Init( x, y, z );
		pDefuser->SetAlpha( 1.0f );

		SetIconPackagePosition( pDefuser );
	}
}

//-----------------------------------------------------------------------------
// defusers attached to players will update during the PlacePlayer phase
//-----------------------------------------------------------------------------
CCSGO_MapOverview::CCSGO_MapOverviewIconPackage* CCSGO_MapOverview::CreateDefuser( int nEntityID )
{
	if ( !m_bTrackDefusers )
	{
		return NULL;
	}

	CCSGO_MapOverviewIconPackage* pPackage = NULL;
	int index = m_iLastDefuserIndex + 1;

	AssertMsg( GetDefuseIndexFromEntityID( nEntityID ) == INVALID_INDEX, "Defuser entity ID already exists." );

	for ( int i = 0; i <= m_iLastDefuserIndex; i++ )
	{
		if ( !m_Defusers[ i ].m_bIsActive )
		{
			index = i;
			break;
		}
	}

	if ( index < MAX_PLAYERS )
	{
		pPackage = GetRadarDefuser( index );

		InitIconPackage( pPackage, index, ICON_PACK_DEFUSER );

		pPackage->m_iEntityID = nEntityID;
		pPackage->m_fRoundStartTime = TIMER_INIT;
		pPackage->SetIsSpotted( false );
		pPackage->SetIsDefuse( true );
		pPackage->m_Position = vec3_origin;

		m_iLastDefuserIndex = MAX( index, m_iLastDefuserIndex );
	}
	else
	{
		AssertMsg( false, "m_Defusers array is full" );
	}

	return pPackage;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::RemoveAllDefusers( void )
{
	for ( int i = 0; i <= m_iLastDefuserIndex; i++ )
	{
		if ( m_Defusers[ i ].m_bIsActive )
		{
			RemoveDefuser( i );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::RemoveDefuser( int index )
{
	RemoveIconPackage( GetRadarDefuser( index ) );

	if ( index == m_iLastDefuserIndex )
	{
		while ( m_iLastDefuserIndex >= 0 && !m_Defusers[ m_iLastDefuserIndex ].m_bIsActive )
		{
			m_iLastDefuserIndex--;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::SetPlayerTeam( int index, int team )
{
	CCSGO_MapOverviewIconPackage* pPlayer = GetRadarPlayer( index );
	pPlayer->SetPlayerTeam( team );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CCSGO_MapOverview::GetPlayerIndexFromUserID( int userID )
{
	int nEntityID = engine->GetPlayerForUserID( userID );
	for ( int i = 0; i <= m_iLastPlayerIndex; i++ )
	{
		if ( m_Players[ i ].m_bIsActive && m_Players[ i ].m_iEntityID == nEntityID )
		{
			return i;
		}
	}

	return INVALID_INDEX;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CCSGO_MapOverview::GetHostageIndexFromHostageEntityID( int entityID )
{
	for ( int i = 0; i <= m_iLastHostageIndex; i++ )
	{
		if ( m_Hostages[ i ].m_bIsActive && m_Hostages[ i ].m_iEntityID == entityID )
		{
			return i;
		}
	}

	return INVALID_INDEX;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CCSGO_MapOverview::GetGrenadeIndexFromEntityID( int entityID )
{
	for ( int i = 0; i <= m_iLastGrenadeIndex; i++ )
	{
		if ( m_Grenades[ i ].m_bIsActive && m_Grenades[ i ].m_iEntityID == entityID )
		{
			return i;
		}
	}

	return INVALID_INDEX;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CCSGO_MapOverview::GetSurvivalCrateIndexFromEntityID( int entityID )
{
	for ( int i = 0; i <= m_iLastSurvivalCrateIndex; i++ )
	{
		if ( m_SurvivalCrates[i].m_bIsActive && m_SurvivalCrates[i].m_iEntityID == entityID )
		{
			return i;
		}
	}

	return INVALID_INDEX;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CCSGO_MapOverview::GetDefuseIndexFromEntityID( int nEntityID )
{
	for ( int i = 0; i <= m_iLastDefuserIndex; i++ )
	{
		if ( m_Defusers[ i ].m_bIsActive && m_Defusers[ i ].m_iEntityID == nEntityID )
		{
			return i;
		}
	}

	return INVALID_INDEX;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------

CCSGO_MapOverview::CCSGO_MapOverviewIconPackage* CCSGO_MapOverview::GetRadarPlayer( int index )
{
	return &m_Players[ index ];
}

CCSGO_MapOverview::CCSGO_MapOverviewIconPackage* CCSGO_MapOverview::GetRadarHostage( int index )
{
	return &m_Hostages[ index ];
}

CCSGO_MapOverview::CCSGO_MapOverviewIconPackage* CCSGO_MapOverview::GetRadarGrenade( int index )
{
	return &m_Grenades[ index ];
}

CCSGO_MapOverview::CCSGO_MapOverviewIconPackage* CCSGO_MapOverview::GetRadarDefuser( int index )
{
	return &m_Defusers[ index ];
}

CCSGO_MapOverview::CCSGO_MapOverviewIconPackage* CCSGO_MapOverview::GetRadarSurvivalCrate( int index )
{
	return &m_SurvivalCrates[index];
}

//-----------------------------------------------------------------------------
// We don't get updates from the server from players
// that are not in our PVS, so there are separate
// messages specifically to update the radar
//-----------------------------------------------------------------------------
bool CCSGO_MapOverview::MsgFunc_ProcessSpottedEntityUpdate( const CCSUsrMsg_ProcessSpottedEntityUpdate &msg )
{
	if ( msg.new_update() )
	{
		// new spotting frame. clean up.
		m_bBombIsSpotted = false;
		m_EntitySpotted.ClearAll();
	}

	for ( int i = 0; i < msg.entity_updates_size(); i++ )
	{
		const CCSUsrMsg_ProcessSpottedEntityUpdate::SpottedEntityUpdate &update = msg.entity_updates( i );

		int nEntityID = update.entity_idx();
		if ( nEntityID < 0 || nEntityID >= MAX_EDICTS )
			continue; // GeekPwn2016 range check

		m_EntitySpotted.Set( nEntityID, true );

		const char * szEntityClass = NULL;

		int nClassID = update.class_id();
		for ( ClientClass *pCur = g_pClientClassHead; pCur; pCur = pCur->m_pNext )
		{
			if ( pCur->m_ClassID == nClassID )
			{
				szEntityClass = pCur->GetName();
				break;
			}
		}

		if ( szEntityClass == NULL )
		{
			Warning( "Unknown entity class received in ProcessSpottedEntityUpdate.\n" );
		}

		// non-class specific data
		// read position and angle
		int x = update.origin_x() * 4;
		int y = update.origin_y() * 4;
		int z = update.origin_z() * 4;
		int a = update.angle_y();

		// Clients are unaware of the defuser class type, so we need to flag defuse entities manually
		if ( update.defuser() )
		{
			SetDefuserPos( nEntityID, x, y, z, a );
		}
		else if ( V_strcmp( "CCSPlayer", szEntityClass ) == 0 )
		{
			CCSGO_MapOverviewIconPackage* pPlayerIcon = NULL;

			pPlayerIcon = GetRadarPlayer( nEntityID - 1 );

			if ( pPlayerIcon->m_bIsDead )
				continue;

			pPlayerIcon->m_Position.Init( x, y, z );
			pPlayerIcon->m_Angle.Init( 0, a, 0 );

			// has defuser?
			if ( update.player_has_defuser() )
			{
				SetDefuserPos( nEntityID, x, y, z, a );
			}

			// has C4?
			if ( update.player_has_c4() )
			{
				m_bBombIsSpotted = true;
				m_BombPosition.Init( x, y, 0 );
			}

			// The following code should no longer be necessary with the new spotting system
			//C_CSPlayer *pPlayer =	ToCSPlayer( UTIL_PlayerByIndex( nEntityID ) );

			//// Only update players that are outside of PVS
			//if ( pPlayer && pPlayer->IsDormant() )
			//{
			//	// update origin and angle for players out of my PVS
			//	Vector origin = pPlayer->GetAbsOrigin();
			//	QAngle angles = pPlayer->GetAbsAngles();

			//	origin.x = x;
			//	origin.y = y;
			//	angles.y = a;

			//	pPlayer->SetAbsOrigin( origin );
			//	pPlayer->SetAbsAngles( angles );
			//}
		}
		else if ( V_strcmp( "CC4", szEntityClass ) == 0 || V_strcmp( "CPlantedC4", szEntityClass ) == 0 )
		{
			m_bBombIsSpotted = true;
			m_BombPosition.Init( x, y, 0 );
		}
		else if ( V_strcmp( "CHostage", szEntityClass ) == 0 )
		{
			int hostageIndex = GetHostageIndexFromHostageEntityID( nEntityID );

			if ( hostageIndex == INVALID_INDEX )
			{
				for ( int j = 0; j <= m_iLastHostageIndex; j++ )
				{
					RemoveHostage( j );
				}
			}
			else
			{
				CCSGO_MapOverviewIconPackage* pPackage = GetRadarHostage( hostageIndex );

				if ( pPackage )
				{
					pPackage->m_Position.Init( x, y, z );
				}
			}
		}
		else
		{
			Warning( "Unknown entity update received by ProcessSpottedEntityUpdate: %s.\n", szEntityClass );
		}

	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::UpdateAllPlayers( void )
{
	for ( int i = 0; i <= MAX_PLAYERS; ++i )
	{
		CCSGO_MapOverviewIconPackage* pPackage = GetRadarPlayer( i );
		if ( pPackage )
			UpdatePlayer( pPackage );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
static CCSGO_MapOverview::PLAYER_ICON_INDICES s_arrayTeamColorLabels[] = {
	CCSGO_MapOverview::PI_PLAYER_NAME
	
	// ADD MORE TEAM COLOR LABELS HERE
};

static CCSGO_MapOverview::PLAYER_ICON_INDICES s_arrayTeamColorPanels[] = {
	CCSGO_MapOverview::PI_PLAYER,
	CCSGO_MapOverview::PI_PLAYER_DEAD,
	CCSGO_MapOverview::PI_PLAYER_GHOST

	// ADD MORE TEAM COLOR PANELS HERE
};

void CCSGO_MapOverview::UpdatePlayerTeamColor( CCSGO_MapOverviewIconPackage* pPackage )
{
	if ( !pPackage )
		return;

	auto UpdateTeamColor = [&]( const Color& color )
	{
		const int iTeamColorLabelCount = ARRAYSIZE( s_arrayTeamColorLabels );
		for ( int iLabel=0; iLabel<iTeamColorLabelCount; ++iLabel )
		{
			panorama::CPanel2D *pPanel = pPackage->m_pIcons[ s_arrayTeamColorLabels[ iLabel ] ];
			if ( pPanel )
			{
				panorama::IUIPanelStyle *pPanelStyle = pPanel->AccessStyle();
				pPanelStyle->SetSimpleForegroundColor( color );
			}
		}

		const int iTeamColorPanelCount = ARRAYSIZE( s_arrayTeamColorPanels );
		for ( int iPanel=0; iPanel<iTeamColorPanelCount; ++iPanel )
		{
			panorama::CPanel2D *pPanel = pPackage->m_pIcons[ s_arrayTeamColorPanels[ iPanel ] ];
			if ( pPanel )
			{
				panorama::IUIPanelStyle *pPanelStyle = pPanel->AccessStyle();
				pPanelStyle->SetSimpleWashColor( color, true );
			}
		}
	};

	auto pSurvivalRules = CSGameRules()->GetSurvivalRules();
	if ( pSurvivalRules && pSurvivalRules->IsPlayingTeamMode() )
	{
		// pPackage->m_iEntityID is player's entindex
		C_BasePlayer * pPlayer = UTIL_PlayerByIndex( pPackage->m_iEntityID );
		if ( pPlayer )
		{
			UpdateTeamColor( pSurvivalRules->GetTeamColorByUserID( pPlayer->GetUserID() ) );
		}
	}
	else if ( pPackage->m_iPlayerTeam == TEAM_CT )
	{
		UpdateTeamColor( Color( 81, 216, 255, 255 ) );
	}
	else if ( pPackage->m_iPlayerTeam == TEAM_TERRORIST )
	{
		UpdateTeamColor( Color( 255, 227, 36, 255 ) );
	}
	else
	{
		if ( C_CS_PlayerResource *pCSPR = GetCSResources() )
		{
			int nColorID = pCSPR->GetCompTeammateColor( pPackage->m_iEntityID );
			Color teamColor = pCSPR->GetCompPlayerColorByID( nColorID );
			UpdateTeamColor( teamColor );
		}
	}
}

void CCSGO_MapOverview::UpdatePlayer( CCSGO_MapOverviewIconPackage* pPackage )
{
	if ( !pPackage )
		return;

	static const panorama::CPanoramaSymbol k_symTeamCT( "player-team-ct" );
	static const panorama::CPanoramaSymbol k_symTeamT( "player-team-t" );

	// set appropriate class for team
	if ( pPackage->m_pIconPackage )
	{
		pPackage->m_pIconPackage->SetHasClass( k_symTeamCT, pPackage->m_iPlayerTeam == TEAM_CT );
		pPackage->m_pIconPackage->SetHasClass( k_symTeamT, pPackage->m_iPlayerTeam == TEAM_TERRORIST );
	}

	UpdatePlayerTeamColor( pPackage );

	// update the player number
	CCSGO_HudTeamCounter* pTeamCounter = GET_HUDELEMENT( CCSGO_HudTeamCounter );
	if ( pPackage->m_pIcons[ PI_PLAYER_NUMBER ] && pPackage->m_pIcons[ PI_PLAYER_NAME ] )
	{
		if ( pTeamCounter )
		{
			int pidx = pTeamCounter->GetPlayerSlotIndex( pPackage->m_iEntityID );
			if ( pidx != -1 )
			{
				pidx += 1;
				if ( pidx == 10 )
					pidx = 0;

				if ( pPackage->m_pPlayerNumber ) // PI_PLAYER_NUMBER, pPackage->m_pPlayerNumber should == pPackage->m_pIcons[ PI_PLAYER_NUMBER ]
				{
					CBasePlayer * pPlayer = CBasePlayer::GetLocalPlayer();
					bool bShowNumber = ( pPlayer && ( m_bShowAll || pPlayer->GetObserverMode() != OBS_MODE_NONE ) );
					bShowNumber = bShowNumber && ( CCSGameRules::GetMaxPlayers() <= 10 || ( CSGameRules() && CSGameRules()->IsPlayingSurvival() ) );

						if ( bShowNumber )
					{
						auto pSurvivalRules = CSGameRules()->GetSurvivalRules();
						extern ConVar spec_dz_group_teams;
						if ( spec_dz_group_teams.GetBool() && pPlayer && pSurvivalRules && pSurvivalRules->IsPlayingTeamMode() )
						{
							C_CSPlayer * pCSPlayer = static_cast<C_CSPlayer*>( UTIL_PlayerByIndex( pPackage->m_iEntityID ) );
							int nNum = pCSPlayer ? pCSPlayer->m_nSurvivalTeam + 1 : 0;

							pPackage->m_pPlayerNumber->SetText( CFmtStr( "%d", nNum ), panorama::CLabel::k_ETextTypeUnlocalized );
						}
						else
							pPackage->m_pPlayerNumber->SetText( CFmtStr( "%d", pidx ), panorama::CLabel::k_ETextTypeUnlocalized );
					}
					else
						pPackage->m_pPlayerNumber->SetText( "", panorama::CLabel::k_ETextTypeUnlocalized );
				}
			}
		}

		// PI_PLAYER_NAME, pPackage->m_pPlayerName should == pPackage->m_pIcons[ PI_PLAYER_NAME ]
		if ( pPackage->m_pPlayerName )
		{
			pPackage->m_pPlayerName->SetText( pPackage->m_szName, panorama::CLabel::k_ETextTypeUnlocalized );
		}
	}
}

//--------------------------------------------------------------------------------------------------
// handles all game event messages ( first looks them up in map so there's no long list of strcmps )
//--------------------------------------------------------------------------------------------------
void CCSGO_MapOverview::FireGameEvent( IGameEvent *event )
{
	const char* eventName = event->GetName();
	int elementIndex = m_messageMap.Find(eventName);
	if (elementIndex == m_messageMap.InvalidIndex())
	{
		return;
	}

	CBasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	int EventUserID = event->GetInt( "userid", -1 );
	int LocalPlayerID = ( pLocalPlayer != NULL ) ? pLocalPlayer->GetUserID() : -2;

	DESIRED_MESSAGE_INDICES messageTypeIndex = (DESIRED_MESSAGE_INDICES)m_messageMap.Element( elementIndex );

	switch (messageTypeIndex)
	{
	case GAME_NEWMAP:
		ResetForNewMap();
		SetMap( event->GetString( "mapname" ) );
		break;

	case ROUND_POST_START:
		ResetRound();
		break;

	case PLAYER_CONNECT:
	case PLAYER_INFO:
	{
		int index = event->GetInt("index"); // = entity index - 1
		int entIndex = index + 1;

		if  (index < 0 || index >= MAX_PLAYERS )
		{
			return;
		}

		CCSGO_MapOverviewIconPackage* pPackage = CreatePlayer(index);
		if ( messageTypeIndex == PLAYER_CONNECT )
		{
			pPackage->SetPlayerTeam(TEAM_SPECTATOR);
		}

		pPackage->m_iEntityID = entIndex;

		const char* name = event->GetString("name", "unknown");
		if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
		{
			if ( pParameters->m_bAnonymousPlayerIdentity )
				name = ""; // suppress names for anonymous playback
		}

		if ( !StringIsEmpty( name ) && cl_sanitize_player_names.GetBool() )
		{
			name = ""; 
		}

		// TODO - where is this used/displayed?
		V_strcpy( pPackage->m_szName, name );
	}
		break;

	case PLAYER_TEAM:
	{
		int playerIndex = GetPlayerIndexFromUserID( event->GetInt("userid"));

		if ( playerIndex == INVALID_INDEX )
		{
			return;
		}

		CCSGO_MapOverviewIconPackage* pPackage = GetRadarPlayer(playerIndex);
		pPackage->SetPlayerTeam( event->GetInt("team") );

	}
		break;

	case PLAYER_DEATH:
	{
		int playerIndex = GetPlayerIndexFromUserID( event->GetInt("userid") );

		if ( playerIndex == INVALID_INDEX )
		{
			return;
		}

		CCSGO_MapOverviewIconPackage* pPackage = GetRadarPlayer(playerIndex);

		if ( pPackage->m_bIsActive )
		{
			pPackage->SetIsDead( true );
			pPackage->m_Health = 0;
		}

		int nDefuseIndex = GetDefuseIndexFromEntityID( engine->GetPlayerForUserID( event->GetInt("userid") ) );

		if ( nDefuseIndex != INVALID_INDEX )
		{
			RemoveDefuser( nDefuseIndex );
		}
	}
		break;

	case PLAYER_SPAWN:
	{
		if ( EventUserID == LocalPlayerID )
		{
			m_bShowMapOverview = false;
		}

		int playerIndex = GetPlayerIndexFromUserID( event->GetInt("userid") );

		if ( playerIndex != INVALID_INDEX )
		{
			CCSGO_MapOverviewIconPackage* pPackage = GetRadarPlayer( playerIndex );
			pPackage->m_Health = 100;

			UpdatePlayer( pPackage );
		}
	}
		break;

	case PLAYER_DISCONNECT:
	{

		int playerIndex = GetPlayerIndexFromUserID( event->GetInt("userid") );

		if ( playerIndex != INVALID_INDEX )
		{
			RemovePlayer( playerIndex );
		}
	}
		break;

	case HOSTAGE_KILLED:
	{
		int hostageIndex = GetHostageIndexFromHostageEntityID( event->GetInt("hostage") );

		if ( hostageIndex == INVALID_INDEX )
		{
			return;
		}

		CCSGO_MapOverviewIconPackage* pPackage = GetRadarHostage( hostageIndex );

		if ( pPackage->m_bIsActive )
		{
			pPackage->SetIsDead( true );
		}

	}
		break;

	case HOSTAGE_RESCUED:
	{
		int hostageIndex = GetHostageIndexFromHostageEntityID( event->GetInt("hostage") );

		if ( hostageIndex == INVALID_INDEX )
		{
			return;
		}

		CCSGO_MapOverviewIconPackage* pPackage = GetRadarHostage( hostageIndex );

		if ( pPackage->m_bIsActive )
		{
			pPackage->SetIsRescued( true );
		}

	}
		break;

	case BOMB_DEFUSED:
	{
		m_bBombDefused = true;
	}
		break;

	case BOMB_EXPLODED:
	{
		m_bBombExploded = true;
	}
		break;

	case BOMB_PLANTED:
	{
		m_nBombEntIndex = -1;
		m_bBombPlanted = true;
		m_BombPosition.z = 0;
	}
		break;

	case BOMB_PICKUP:
	{
		m_bBombDropped = false;
		m_nBombEntIndex = -1;
	}
		break;

	case BOMB_DROPPED:
	{
		m_nBombEntIndex = event->GetInt( "entindex", -1 );
		m_bBombDropped = true;
	}
		break;

	case DEFUSER_PICKUP:
	{
		int nDefuseIndex = GetDefuseIndexFromEntityID( event->GetInt( "entityid" ) );

		if (nDefuseIndex != INVALID_INDEX)
		{
			RemoveDefuser( nDefuseIndex );
		}
	}
		break;

	case DEFUSER_DROPPED:
	{
		int nEntityID = event->GetInt( "entityid" );

		CreateDefuser( nEntityID );
	}
		break;

	case DECOY_STARTED:
	{
		if ( pLocalPlayer )
		{
			int userID = event->GetInt( "userid" );
			CBasePlayer* player = UTIL_PlayerByUserId( userID );

			if ( player )
			{
				// if it was placed by a player on our team or if we are a spectator, show the grenade as a grenade icon
				// otherwise, we're going to show it as if it were a fake player
				if ( player->GetAssociatedTeamNumber() == pLocalPlayer->GetAssociatedTeamNumber() || m_bShowAll )
				{
					int entityID = event->GetInt( "entityid" );
					//int teamNumber = player->GetAssociatedTeamNumber();

					CCSGO_MapOverviewIconPackage* pPackage = CreateGrenade( entityID, PI_GRENADE_DECOY );

					if (pPackage)
					{
						pPackage->SetGrenadeType( PI_GRENADE_DECOY );
						pPackage->SetIsSpotted( true );

						pPackage->m_Position.Init( ( float )event->GetInt( "x" ), ( float )event->GetInt( "y" ), 0.0f );
						pPackage->m_Angle.Init( 0.0f, RandomFloat( 0.0f, 360.0f ), 0.0f );
						SetIconPackagePosition( pPackage );
					}
				}
			}
		}

	}
		break;

	case DECOY_DETONATE:
	{
		int entityID = event->GetInt( "entityid" );
		int i = GetGrenadeIndexFromEntityID( entityID );
		if ( i != INVALID_INDEX )
		{
			CCSGO_MapOverviewIconPackage* pPackage = GetRadarGrenade(i);
			pPackage->SetGrenadeExpireTime( gpGlobals->curtime );
			SetIconPackagePosition(pPackage);
		}

		STEAMWORKS_TESTSECRET_AMORTIZE( 23 );
	}
		break;

	case HE_DETONATE:
	{
		int entityID = event->GetInt( "entityid" );
		int userID = event->GetInt( "userid" );
		CBasePlayer* player = UTIL_PlayerByUserId( userID );
		if ( pLocalPlayer && player )
		{
			// if it was placed by a player on our team or if we are a spectator, show the grenade as usually
			// otherwise, we don't show it at all
			Vector vecGrenade = Vector( (float)event->GetInt( "x" ), (float)event->GetInt( "y" ), 0 );
			if ( ( player->GetAssociatedTeamNumber() == pLocalPlayer->GetAssociatedTeamNumber() && IsEnemyCloseEnoughToShow( vecGrenade ) ) || m_bShowAll )
			{
				CCSGO_MapOverviewIconPackage* pPackage = CreateGrenade( entityID, PI_GRENADE_HE );
				if ( pPackage )
				{
					pPackage->SetGrenadeType( PI_GRENADE_HE );
					pPackage->SetGrenadeExpireTime( gpGlobals->curtime );
					pPackage->m_Position.Init( vecGrenade.x, vecGrenade.y, 0.0f );
					pPackage->m_Angle.Init( 0.0f, RandomFloat( 0.0f, 360.0f ), 0.0f );
					SetIconPackagePosition( pPackage );
				}
			}
		}

		STEAMWORKS_TESTSECRET_AMORTIZE( 19 );
	}
	break;

	case FLASH_DETONATE:
	{
		int entityID = event->GetInt( "entityid" );
		int userID = event->GetInt( "userid" );
		CBasePlayer* player = UTIL_PlayerByUserId( userID );
		if ( pLocalPlayer && player )
		{
			// if it was placed by a player on our team or if we are a spectator, show the grenade as usually
			// otherwise, we don't show it at all
			Vector vecGrenade = Vector( (float)event->GetInt( "x" ), (float)event->GetInt( "y" ), 0 );
			if ( ( player->GetAssociatedTeamNumber() == pLocalPlayer->GetAssociatedTeamNumber() && IsEnemyCloseEnoughToShow( vecGrenade ) ) || m_bShowAll )
			{
				CCSGO_MapOverviewIconPackage* pPackage = CreateGrenade( entityID, PI_GRENADE_FLASH );
				if ( pPackage )
				{
					pPackage->SetGrenadeType( PI_GRENADE_FLASH );
					pPackage->SetGrenadeExpireTime( gpGlobals->curtime );
					pPackage->m_Position.Init( vecGrenade.x, vecGrenade.y, 0.0f );
					pPackage->m_Angle.Init( 0.0f, RandomFloat( 0.0f, 360.0f ), 0.0f );
					SetIconPackagePosition( pPackage );
				}
			}
		}

		STEAMWORKS_TESTSECRET_AMORTIZE( 17 );
	}
	break;

	case SMOKE_DETONATE:
	{
		int entityID = event->GetInt( "entityid" );
		int userID = event->GetInt( "userid" );
		CBasePlayer* player = UTIL_PlayerByUserId( userID );
		if ( pLocalPlayer && player )
		{
			// if it was placed by a player on our team or if we are a spectator, show the grenade as usually
			// otherwise, we don't show it at all
			Vector vecGrenade = Vector( (float)event->GetInt( "x" ), (float)event->GetInt( "y" ), 0 );
			if ( ( player->GetAssociatedTeamNumber() == pLocalPlayer->GetAssociatedTeamNumber() && IsEnemyCloseEnoughToShow( vecGrenade ) ) || m_bShowAll )
			{
				CCSGO_MapOverviewIconPackage* pPackage = CreateGrenade( entityID, PI_GRENADE_SMOKE );

				if ( pPackage )
				{
					pPackage->SetGrenadeType( PI_GRENADE_SMOKE );
					pPackage->m_Position.Init( vecGrenade.x, vecGrenade.y, 0.0f );
					pPackage->m_Angle.Init( 0.0f, RandomFloat( 0.0f, 360.0f ), 0.0f );
					SetIconPackagePosition( pPackage );
				}
			}
		}
	}
	break;

	case SMOKE_EXPIRE:
	{
		int entityID = event->GetInt( "entityid" );
		int i = GetGrenadeIndexFromEntityID( entityID );
		if ( i != INVALID_INDEX )
		{
			CCSGO_MapOverviewIconPackage* pPackage = GetRadarGrenade( i );
			pPackage->SetGrenadeExpireTime( gpGlobals->curtime );
			SetIconPackagePosition( pPackage );
		}

		STEAMWORKS_TESTSECRET_AMORTIZE( 11 );
	}
	break;

	case MOLOTOV_DETONATE:
	{
		int entityID = event->GetInt( "entityid" );
		int userID = event->GetInt( "userid" );
		CBasePlayer* player = UTIL_PlayerByUserId( userID );
		if ( pLocalPlayer && player )
		{
			// if it was placed by a player on our team or if we are a spectator, show the grenade as usually
			// otherwise, we don't show it at all
			Vector vecGrenade = Vector( (float)event->GetInt( "x" ), (float)event->GetInt( "y" ), 0 );
			if ( ( player->GetAssociatedTeamNumber() == pLocalPlayer->GetAssociatedTeamNumber() && IsEnemyCloseEnoughToShow( vecGrenade ) ) || m_bShowAll )
			{
				CCSGO_MapOverviewIconPackage* pPackage = CreateGrenade( entityID, PI_GRENADE_MOLOTOV );

				if ( pPackage )
				{
					pPackage->SetGrenadeType( PI_GRENADE_MOLOTOV );
					pPackage->m_Position.Init( vecGrenade.x, vecGrenade.y, 0.0f );
					pPackage->m_Angle.Init( 0.0f, RandomFloat( 0.0f, 360.0f ), 0.0f );
					SetIconPackagePosition( pPackage );
				}
			}
		}
	}
	break;

	case MOLOTOV_EXPIRE:
	{
		int entityID = event->GetInt( "entityid" );
		int i = GetGrenadeIndexFromEntityID( entityID );
		if ( i != INVALID_INDEX )
		{
			CCSGO_MapOverviewIconPackage* pPackage = GetRadarGrenade( i );
			pPackage->SetGrenadeExpireTime( gpGlobals->curtime );
			SetIconPackagePosition( pPackage );
		}

		STEAMWORKS_TESTSECRET_AMORTIZE( 19 );
	}
	break;

	case BOT_TAKEOVER:
	{
		C_CSPlayer *pLocalCSPlayer = C_CSPlayer::GetLocalCSPlayer();
		if ( pLocalCSPlayer )
		{
			int userID = event->GetInt( "userid" );
			int localID = pLocalCSPlayer->GetUserID();


			if (localID == userID)
			{
				ResetRadar(false);
			}
		}

	}
	break;

	case SURVIVAL_PARADROP_SPAWN:
	{
		int entityID = event->GetInt( "entityid" );
		int userID = event->GetInt( "userid" );
		if ( pLocalPlayer )
		{
			// if it was placed by a player on our team or if we are a spectator, show the grenade as usually
			// otherwise, we don't show it at all
			Vector vecGrenade = Vector( ( float )event->GetInt( "x" ), ( float )event->GetInt( "y" ), 0 );
			CCSGO_MapOverviewIconPackage* pPackage = CreateSurvivalCrate( entityID );

			if ( pPackage )
			{
				pPackage->SetGrenadeType( PI_SURVIVAL_PARADROP );
				pPackage->m_Position.Init( vecGrenade.x, vecGrenade.y, 0.0f );
				pPackage->m_Angle.Init( 0.0f, RandomFloat( 0.0f, 360.0f ), 0.0f );
				SetIconPackagePosition( pPackage );
			}
		}
	}
	break;

	case SURVIVAL_PARADROP_BREAK:
	{
		int entityID = event->GetInt( "entityid" );
		int i = GetSurvivalCrateIndexFromEntityID( entityID );
		if ( i != INVALID_INDEX )
		{
			CCSGO_MapOverviewIconPackage* pPackage = GetRadarSurvivalCrate( i );
			pPackage->SetGrenadeExpireTime( gpGlobals->curtime );
			SetIconPackagePosition( pPackage );
		}

		STEAMWORKS_TESTSECRET_AMORTIZE( 23 );
	}
	break;

	default:
		break;
	}
}


//-----------------------------------------------------------------------------
// these set lazy update the icons and text based
// on the values in the state variables
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::SetupIconsFromStates( void )
{
	int newFlags = 0;

	if ( CSGameRules()->IsBombDefuseMap() )
	{
		if ( m_fBombAlpha > 0.0f )
		{
			newFlags |= 1 << RI_BOMB_ICON_PACKAGE;
			if ( m_bShowBombHighlight )
			{
				if ( m_bBombPlanted )
				{
					newFlags |= 1 << RI_BOMB_ICON_PLANTED;
					// use both panels to animated planted for now
					newFlags |= 1 << RI_BOMB_ICON_DROPPED;
				}
				else
				{
					newFlags |= 1 << RI_BOMB_ICON_DROPPED;
				}

				C_CSPlayer *pLocalOrObserver = NULL;
				if ( pLocalOrObserver )
				{
					if ( m_BombPosition.z > pLocalOrObserver->GetAbsOrigin().z + 180 )
						newFlags |= 1 << RI_BOMB_ICON_BOMB_ABOVE;
					else if ( m_BombPosition.z < pLocalOrObserver->GetAbsOrigin().z - 180 )
						newFlags |= 1 << RI_BOMB_ICON_BOMB_BELOW;
				}
			}

			C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
			if ( pLocalPlayer != NULL )
			{
				if ( pLocalPlayer->GetAssociatedTeamNumber() == TEAM_TERRORIST )
				{
					// TODO - verify what this should do/display
					//newFlags |= 1 << RI_BOMB_ICON_BOMB_T;
				}
				else
				{
					// TODO - verify what this should do/display
					//newFlags |= 1 << RI_BOMB_ICON_BOMB_CT;
				}
			}

			Vector mapPosition;

			WorldToRadar( m_BombPosition, mapPosition );

			Assert( m_pBombDefuserPackage );
			m_pBombDefuserPackage->SetOpacity( m_fBombAlpha );

			int nBombCarryOffset = 0;
			if ( ( ( newFlags & ( 1 << RI_BOMB_ICON_BOMB_T ) ) || ( newFlags & ( 1 << RI_BOMB_ICON_BOMB_CT ) ) ) && !( newFlags & ( 1 << RI_BOMB_ICON_PLANTED ) ) )
			{
				nBombCarryOffset = 6;
			}

			Vector hudPos;
			mapPosition.y += nBombCarryOffset;
			RadarToHud( mapPosition, hudPos );
			Helper_SetMapOverviewPanelTransform3D( m_pBombDefuserPackage, hudPos, -m_RadarRotation, 1.0f );
		}
	}
	else if ( CSGameRules()->IsHostageRescueMap() )
	{
		if ( m_bShowingHostageZone )
		{
			newFlags |= 1 << RI_IN_HOSTAGE_ZONE;
		}
	}

	if ( m_bShowingDashboard )
		newFlags |= 1 << RI_DASHBOARD;

	SetVisibilityFlags( newFlags );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::SetVisibilityFlags( uint64 newFlags )
{
	newFlags &= ( ( 1ll << RI_NUM_ICONS ) - 1 );

	int diffFlags = m_iCurrentVisibilityFlags ^ newFlags;

	if ( diffFlags )
	{
		bool bWantVisible = ( newFlags != 0 );
		if ( ( m_iCurrentVisibilityFlags != 0 ) != bWantVisible )
		{
			m_pBombDefuserPackage->SetVisible( bWantVisible );
		}

		for ( int i = 0; i < RI_NUM_ICONS && ( diffFlags != 0 ); i++, diffFlags >>= 1 )
		{
			if ( diffFlags & 1 )
			{
				if ( m_pAllIcons[ i ] )
					m_pAllIcons[ i ]->SetVisible( ( newFlags & ( 1ll << i ) ) != 0 );
			}
		}

		m_iCurrentVisibilityFlags = newFlags;
	}
}

//-----------------------------------------------------------------------------
// "drawing" code.Actually just sets transforms
// of the panorama objects
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::WorldToRadar(const Vector& ptin, Vector& ptout)
{
 	float fWorldScale = m_fWorldToRadarScale;

 	float fScale = ( m_fRadarSize / m_fMapSize );
 	fWorldScale *= fScale;

	ptout.x = ( ptin.x - m_MapOrigin.x ) * fWorldScale;
	ptout.y = ( m_MapOrigin.y - ptin.y ) * fWorldScale;
	ptout.z = 0;
}

void CCSGO_MapOverview::RadarToWorld( const Vector& ptin, Vector& ptout )
{
	ptout.x = ( ptin.x + m_MapOrigin.x ) / m_fWorldToRadarScale;
	ptout.y = ( m_MapOrigin.y + ptin.y ) / m_fWorldToRadarScale;
	ptout.z = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::RadarToHud( const Vector& ptin, Vector& ptout )
{
	VMatrix rot;
	MatrixBuildRotateZ( rot, m_RadarRotation );
	Vector3DMultiply( rot, ptin - m_RadarViewpointMap, ptout );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::PositionRadarViewpoint( void )
{
	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

	CBasePlayer * pPlayer = CBasePlayer::GetLocalPlayer();

	if ( pPlayer == NULL )
	{
		pPlayer = GetSplitScreenViewPlayer( nSlot );
	}

	// [jbright] Using GetLocalOrigin addresses an issue which
	// causes the center player indicator to not stay centered
	// on the radar. This solution causes the radar to go jump
	// around while in observer mode, so in those cases rely on
	// MainViewOrigin.
	// 	if ( pPlayer && m_iObserverMode == OBS_MODE_NONE )
	// 	{
	// 		m_RadarViewpointWorld = pPlayer->GetLocalOrigin();
	// 	}
	// 	else
	// 	{
	// 		m_RadarViewpointWorld = MainViewOrigin( nSlot );
	// 	}

	// Get the world's extent
	// 	C_World *world = GetClientWorldEntity();
	// 	if ( !world )
	// 		return;
	// 
	// 	Vector worldCenter = ( world->m_WorldMins + world->m_WorldMaxs ) * 0.5f;

	m_RadarViewpointWorld = Vector( m_MapOrigin.x, m_MapOrigin.y, 0 );//worldCenter;
	//m_RadarViewpointWorld.z = 0;

	WorldToRadar( m_RadarViewpointWorld, m_RadarViewpointMap );

	m_RadarRotation = 0;

	CUtlVector<CTransform3D *> vecTransforms;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::PlaceGoalIcons( void )
{
	C_CS_PlayerResource *pCSPR = GetCSResources();
	if ( !pCSPR )
	{
		return;
	}

	CCSGO_MapOverviewGoalIcon* pwalk = m_GoalIcons;
	Vector mapPosition;

	for ( int i = 0; i < m_iNumGoalIcons; i++ )
	{
		WorldToRadar( pwalk->m_Position, mapPosition );

		if ( pwalk->m_pIcon )
		{
			Vector hudPos;
			RadarToHud( mapPosition, hudPos );
			Helper_SetMapOverviewPanelTransform3D( pwalk->m_pIcon, hudPos, -m_RadarRotation, 1.0f );
		}
		pwalk++;
	}

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::SetIconPackagePosition( CCSGO_MapOverviewIconPackage* pPackage )
{
	Vector vO = vec3_origin;
	Vector mapPosition;
	float mapAngle;

	WorldToRadar( pPackage->m_Position, mapPosition );

	pPackage->SetIsOffMap( false );
	if ( pPackage->m_bIsDead || !pPackage->m_bIsSpotted )
	{
		mapAngle = -m_RadarRotation;
	}
	else
	{
		mapAngle = -pPackage->m_Angle[ YAW ] + 90;
	}

	//TODO?
// 	float scale = 1.0f;
// 	float fIconScale = cl_radar_scale.GetFloat();
// 
// 	fIconScale = RemapValClamped( fIconScale,
// 								  0, 1,
// 								  cl_radar_icon_scale_min.GetFloat(), 1 );
// 
// 	scale *= fIconScale;

	pPackage->m_HudPosition = mapPosition;

	if ( pPackage->m_pIconPackageRotate )
	{
		pPackage->m_HudRotation = mapAngle;
	}
	else
	{
		pPackage->m_HudRotation = 0.0f;
	}

	pPackage->SetupIconsFromStates();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::PlaceHostages( void )
{
	if ( !CSGameRules()->IsHostageRescueMap() )
	{
		return;
	}

	C_CS_PlayerResource *pCSPR = (C_CS_PlayerResource*)GameResources();
	if ( !pCSPR )
	{
		return;
	}

	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( pLocalPlayer == NULL )
	{
		return;
	}

	int iNumLiveHostages = 0;
	int iNumDeadHostages = 0;
	int iNumMovingHostages = 0;
	int iNumRescuedHostages = 0;


	
	bool bIsTerrorist = ( pLocalPlayer->GetTeamNumber() == TEAM_TERRORIST );

	if ( bIsTerrorist )
	{
		// Taking over a bot after switching to the terrorist team results in
		// the hostage indicators remaining visible, so we need to remove them.
		for ( int i = 0; i <= m_iLastHostageIndex; i++ )
		{
			if ( GetRadarHostage( i )->m_bIsActive )
				RemoveHostage( i );
		}

		// we also want to show the number of hostages that are still alive
		// and unrescued
		for( int i=0; i < MAX_HOSTAGES; i++ )
		{
			if( pCSPR->IsHostageAlive( i ) )
				iNumLiveHostages++;
		}
	}
	else
	{

		// T's can now always see basic hostage info (alive, dead) but not moving or rescued status
		/*
		bool bCanShowHostages = m_bShowAll || ( pLocalPlayer->GetTeamNumber() == TEAM_CT );

		if ( !bCanShowHostages )
		{
		return;
		}
		*/

		// Update hostage status
		for ( int i = 0; i < MAX_HOSTAGES; i++ )
		{
			int nEntityID = pCSPR->GetHostageEntityID( i );

			if ( nEntityID > 0 )
			{
				CCSGO_MapOverviewIconPackage* pHostage = NULL;

				int nHostageIndex = GetHostageIndexFromHostageEntityID( nEntityID );


				if ( nHostageIndex == INVALID_INDEX )
				{
					// Create the hostage in next available slot
					for ( nHostageIndex = 0; nHostageIndex < MAX_HOSTAGES; nHostageIndex++ )
					{
						if ( !m_Hostages[ nHostageIndex ].m_bIsActive )
						{
							pHostage = CreateHostage( nHostageIndex );
							pHostage->SetPlayerTeam( TEAM_UNASSIGNED );
							pHostage->m_iEntityID = nEntityID;
							break;
						}
					}
				}
				else
				{
					pHostage = GetRadarHostage( nHostageIndex );
				}


				if ( pHostage )
				{
					if ( pCSPR->IsHostageAlive( i ) )
					{
						if ( !pHostage->m_bIsActive )
						{

						}

						pHostage->SetIsDead( false );

						if ( pCSPR->IsHostageFollowingSomeone( i ) )
						{
							iNumMovingHostages++;
							pHostage->SetIsMovingHostage( true );
						}
						else
						{
							iNumLiveHostages++;
							pHostage->SetIsMovingHostage( false );
						}
					}
					else if ( i <= m_iLastHostageIndex && pHostage->m_bIsActive )
					{
						if ( pHostage->m_bIsRescued )
						{
							iNumRescuedHostages++;
						}
						else
						{
							iNumDeadHostages++;
						}

						pHostage->SetIsDead( true );
						pHostage->SetIsMovingHostage( false );
					}
				}
			}
		}

		// Update hostage positions
		for ( int i = 0; i < MAX_HOSTAGES; i++ )
		{
			CCSGO_MapOverviewIconPackage* pHostage = GetRadarHostage( i );

			if ( pHostage->m_bIsActive )
			{
				C_BaseEntity * pEntity = ClientEntityList().GetBaseEntity( pHostage->m_iEntityID );

				if ( pEntity && !pEntity->IsDormant() )
				{
					Assert( dynamic_cast<CHostage*>( pEntity ) );

					pHostage->m_Position = pEntity->GetNetworkOrigin();
				}

				pHostage->SetIsSpotted( true );
				SetIconPackagePosition( pHostage );
			}
		}
	}

	int hostageIndex = 0;

	for ( int i = 0; i < iNumDeadHostages; i++ )
	{
		m_HostageStatusIcons[ hostageIndex++ ].SetStatus( CCSGO_MapOverviewHostageIcons::HI_DEAD );
	}

	for ( int i = 0; i < iNumRescuedHostages; i++ )
	{
		m_HostageStatusIcons[ hostageIndex++ ].SetStatus( CCSGO_MapOverviewHostageIcons::HI_RESCUED );
	}

	for ( int i = 0; i < iNumMovingHostages; i++ )
	{
		m_HostageStatusIcons[ hostageIndex++ ].SetStatus( CCSGO_MapOverviewHostageIcons::HI_TRANSIT );
	}

	for ( int i = 0; i < iNumLiveHostages; i++ )
	{
		m_HostageStatusIcons[ hostageIndex++ ].SetStatus( CCSGO_MapOverviewHostageIcons::HI_ALIVE );
	}

	for ( ; hostageIndex < MAX_HOSTAGES && m_HostageStatusIcons[ hostageIndex ].m_pIconPackage != NULL; hostageIndex++ )
	{
		m_HostageStatusIcons[ hostageIndex ].SetStatus( CCSGO_MapOverviewHostageIcons::HI_UNUSED );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::UpdateAllDefusers( void )
{
	if ( !m_bTrackDefusers )
	{
		return;
	}

	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( pLocalPlayer == NULL )
	{
		return;
	}

	for ( int i = 0; i <= m_iLastDefuserIndex; i++ )
	{
		CCSGO_MapOverviewIconPackage * pPackage = GetRadarDefuser( i );

		if ( pPackage && pPackage->m_bIsActive )
		{
			C_BaseEntity * pEntity = ClientEntityList().GetBaseEntity( pPackage->m_iEntityID );

			bool bSpotted = ( pLocalPlayer->GetAssociatedTeamNumber() == TEAM_CT || ( pEntity && pEntity->IsSpotted() ) || m_EntitySpotted.Get( pPackage->m_iEntityID ) );

			pPackage->SetIsSpotted( bSpotted );

			if ( bSpotted && pEntity && !pEntity->IsDormant() )
			{
				SetDefuserPos( pPackage->m_iEntityID,
							   pEntity->GetNetworkOrigin().x,
							   pEntity->GetNetworkOrigin().y,
							   pEntity->GetNetworkOrigin().z,
							   pEntity->GetNetworkAngles().y );
			}
		}

		// update radar position and visibility
		if ( pPackage && pPackage->m_bIsActive )
		{
			SetIconPackagePosition( pPackage );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::PlacePlayers( void )
{
	C_CS_PlayerResource *pCSPR = (C_CS_PlayerResource*)GameResources();
	if ( !pCSPR )
	{
		return;
	}

	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( pLocalPlayer == NULL )
	{
		return;
	}

	int localPlayerIndex = pLocalPlayer->entindex() - 1; // note: player index, not entity index
	if ( localPlayerIndex == INVALID_INDEX )
		return;

	int localTeamNumber = pLocalPlayer->GetAssociatedTeamNumber();

	CCSGO_MapOverviewIconPackage* pwalk = m_Players;
	int playerEntityIndex;

	for ( int i = 0; i <= m_iLastPlayerIndex; i++, pwalk++ )
	{
		if ( pwalk->m_pIconPackage && pwalk->m_bIsActive )
		{
			playerEntityIndex = i + 1; // convert from player index to entity index

			C_CSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( playerEntityIndex ) );

			bool playerIsActive = pPlayer != NULL && !pPlayer->IsDormant();
			int nIsAboveOrBelow = R_SAMELEVEL;

			// we do all the pCSPR stuff because the player may not have been given
			// to us by the server yet.  If that's the case, we can't use the player
			// pointer to find anything out, we have to use the PCSPR which keeps some
			// of this data ( like team numbers, spotted, hasC4 etc.  The dormant means
			// that it's not in our view

			if ( pCSPR->IsConnected( playerEntityIndex ) )
			{
				pwalk->m_Health = pCSPR->GetHealth( playerEntityIndex );

				if ( !pCSPR->IsAlive( playerEntityIndex ) )
				{
					pwalk->m_Health = 0;
					if ( pCSPR->GetControlledByPlayer( playerEntityIndex ) )
					{
						pwalk->SetIsControlledBot();
					}
					else
					{
						pwalk->SetIsDead( true );
					}
				}
				else
				{
					pwalk->SetIsDead( false );

					//it's hard to figure out if this player has been spotted, and if spotted has enough info to be
					//shown.

					bool bObservingTarget = pLocalPlayer->GetObserverTarget() && ( pLocalPlayer->GetObserverTarget() == pPlayer );

					// This code is basically a clone of the logic from csgo_hudradar.cpp

					bool bIsEnemy = pLocalPlayer->IsOtherEnemy( playerEntityIndex );
					bool bHasValidPosition = playerIsActive || ( pwalk->m_Position.x != 0 || pwalk->m_Position.y != 0 );

					// First check if we were given the player in a 'spotted but not in pvs' update specifically for this client
					// (e.g. teammate sees far away enemy)
					bool bVisibleOnRadar = m_EntitySpotted.Get( playerEntityIndex ) && !playerIsActive;

					bool bSpottedByFriends = !bIsEnemy; 	// friends are always considered to be spotted by friends
					bool bSpottedByLocalPlayer = !bIsEnemy;	// friends are always considered to be spotted

															// If not, but we have up-to-date in-PVS data about the player, check if we have spotted him normally.
					if ( playerIsActive )
					{
						Assert( pPlayer != NULL ); // see above where we generate playerIsActive

						if ( !bSpottedByLocalPlayer )
							bSpottedByLocalPlayer = pPlayer->IsSpottedBy( localPlayerIndex );

						// If teammates are enemies, we can only spot objects ourself
						if ( mp_teammates_are_enemies.GetBool() )
						{
							bVisibleOnRadar = bSpottedByLocalPlayer;
						}
						else
						{
							// Here we shortcut via IsSpotted() which works great when we are playing on teams like normal CSGO
							bVisibleOnRadar = bSpottedByLocalPlayer || pPlayer->IsSpotted();
						}

						if ( !bSpottedByFriends )
						{
							bSpottedByFriends = pPlayer->IsSpottedByFriends( localPlayerIndex );
						}

						if ( bVisibleOnRadar )
						{
							// verify here on the client that we do in fact see the entity how the player would expect to see the entity
							// if the local player is in smoke or if they are looking through smoke, we double check if they can see them on radar
							// use the networked position and not the stored position because the stored one is stale
							Vector vecOtherPos( pPlayer->GetNetworkOrigin().x, pPlayer->GetNetworkOrigin().y, ( pPlayer->GetNetworkOrigin().z + 56 ) );
							float flDist = ( pLocalPlayer->EyePosition() - vecOtherPos ).Length2D();
							if ( flDist < 1600 && bSpottedByFriends == false &&
								 LineGoesThroughSmoke( pLocalPlayer->EyePosition(), vecOtherPos, 1.0f ) )
							{
								bVisibleOnRadar = false;
							}
						}
					}

					// Additional logic not in sfhudradar goes here
					if ( bIsEnemy && bVisibleOnRadar && !IsEnemyCloseEnoughToShow( pwalk->m_Position ) )
						bVisibleOnRadar = false;
					// End of extra logic for mapoverview only

					if ( bHasValidPosition && ( !bIsEnemy || m_bShowAll || bVisibleOnRadar || bObservingTarget ) )
					{
						// Some of this code assumes that playerIsActive means that pPlayer is valid.
						Assert( !playerIsActive || pPlayer );

						pwalk->SetIsSpotted( true );
						pwalk->SetIsSelected( bObservingTarget );

						if ( playerIsActive )
						{
							Assert( pPlayer );

							if ( pPlayer->HasDefuser() )
							{
								// attach a defuser icon to this player if one does not already exist
								if ( GetDefuseIndexFromEntityID( playerEntityIndex ) == INVALID_INDEX )
								{
									CreateDefuser( playerEntityIndex );
								}
							}

							if ( pPlayer->HasC4() )
							{
								m_bBombIsSpotted = true;
								m_BombPosition = pwalk->m_Position;
							}

							pwalk->SetIsLowHealth( pPlayer->GetHealth() < 40 );
							pwalk->SetIsFlashed( pPlayer->IsBlinded() );
							pwalk->SetIsFiring( ( pPlayer->GetLastFiredWeaponTime() + 0.2f ) >= gpGlobals->curtime );

						}
						else
						{
							pwalk->SetIsLowHealth( pCSPR->GetHealth( playerEntityIndex ) < 40 );
							pwalk->SetIsFlashed( false );
							pwalk->SetIsFiring( false );
						}
					}
					else
					{
						pwalk->SetIsSpotted( false );
					}
				}

				pwalk->SetPlayerTeam( pCSPR->GetTeam( playerEntityIndex ) );
				pwalk->SetIsOnLocalTeam( localTeamNumber == pCSPR->GetTeam( playerEntityIndex ) );

				pwalk->SetIsPlayer( i == localPlayerIndex );
				pwalk->SetIsSpeaking( GetClientVoiceMgr()->IsPlayerSpeaking( playerEntityIndex ) && GetClientVoiceMgr()->IsPlayerAudible( playerEntityIndex ) );

				UpdatePlayer( pwalk );
			}
			else
			{
				pwalk->SetIsSpotted( false );
				pwalk->SetIsSpeaking( false );
			}

			// when a player dies the system reports the player's position as the position of
			// the spectator camera.  If we let that happen, the dead player's X migrates from
			// the point they died to follow another player's position.
			//
			// so, never update the icon position for a dead player
			if ( playerIsActive && pwalk->m_bIsSpotted && !pwalk->m_bIsDead )
			{
				// dkorus and pfreese:  Changed from local origin and local angles to last networked version to address jitter issues for pax demo 2011
				pwalk->m_Position = pPlayer->GetNetworkOrigin();

				// only set the angle of the icon if the player isn't above or below us
				if ( nIsAboveOrBelow == R_SAMELEVEL )
					pwalk->m_Angle = pPlayer->GetLocalAngles();
				else
				{
					int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
					pwalk->m_Angle = MainViewAngles( nSlot );
				}

				if ( pPlayer->HasC4() )
				{
					m_BombPosition = pwalk->m_Position;
				}
			}

			SetIconPackagePosition( pwalk );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCSGO_MapOverview::IsEnemyCloseEnoughToShow( Vector vecEnemyPos )
{
	//double check if spotted based on distance
	// we're going to check the distance of this guy from all of the people on our team
	// if they are outside the radius (more or less) of what the radar would allow us to see, consider them not spotted
	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pLocalPlayer )
		return false;

	C_CS_PlayerResource *pCSPR = ( C_CS_PlayerResource* )GameResources();
	if ( !pCSPR )
		return false;

	bool bTooFar = true;
	CCSGO_MapOverviewIconPackage* pTeammate = m_Players;
	for ( int j = 0; j <= ( m_iLastPlayerIndex ); j++, pTeammate++ )
	{
		if ( pCSPR->IsConnected( ( j + 1 ) ) && !pLocalPlayer->IsOtherEnemy( j + 1 ) )
		{
			Vector toEnemy = pTeammate->m_Position - vecEnemyPos;
			float dist = toEnemy.LengthSqr();
			if ( dist <= NOTSPOTTEDRADIUS*NOTSPOTTEDRADIUS )
			{
				bTooFar = false;
			}
		}
	}

	return !bTooFar;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::UpdateMiscIcons( void )
{
	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pLocalPlayer )
	{
		return;
	}

	// We show the entire dashboard in spectator mode now, so you can see bomb/hostage status, etc.
	m_bShowingDashboard = true;

	C_CS_PlayerResource *pCSPR = (C_CS_PlayerResource*)GameResources();
	if ( !pCSPR )
	{
		return;
	}

	m_fBombAlpha = 0.0f;
	m_fDefuserAlpha = 0.0f;

	if ( CSGameRules()->IsBombDefuseMap() )
	{
		if ( m_bBombPlanted )
		{
			FOR_EACH_VEC( g_PlantedC4s, iBomb )
			{
				C_PlantedC4 * pC4 = g_PlantedC4s[ iBomb ];
				if ( pC4 && pC4->IsBombActive() )
				{
					bool bLocalPlayerCanSeeBomb = ( pLocalPlayer->GetAssociatedTeamNumber() == TEAM_TERRORIST || m_bShowAll || ( pC4 && !pC4->IsDormant() && pC4->IsSpotted() ) );
					if ( bLocalPlayerCanSeeBomb && pC4 && !pC4->IsDormant() )
					{
						m_bBombIsSpotted = true;
						m_BombPosition = pC4->GetNetworkOrigin();
					}
				}
			}
		}
		if ( m_nBombEntIndex != -1 && ( !m_bBombIsSpotted || m_bBombDropped ) )
		{
			C_BaseEntity * pEntity = ClientEntityList().GetBaseEntity( m_nBombEntIndex );

			bool bLocalPlayerCanSeeBomb = ( pLocalPlayer->GetAssociatedTeamNumber() == TEAM_TERRORIST || m_bShowAll || ( pEntity && !pEntity->IsDormant() && pEntity->IsSpotted() ) );
			if ( bLocalPlayerCanSeeBomb && pEntity && !pEntity->IsDormant() )
			{
				Assert( pEntity && FClassnameIs( pEntity, "weapon_c4" ) );
				m_bBombIsSpotted = true;
				m_BombPosition = pEntity->GetNetworkOrigin();
			}
		}

		float now = gpGlobals->curtime;
		bool drawBomb = false;

		bool bBombPositionIsValid = ( m_BombPosition.x != 0 || m_BombPosition.y != 0 );

		if ( bBombPositionIsValid && !m_bBombExploded && m_bBombIsSpotted )
		{
			drawBomb = true;
			m_fBombSeenTime = now;
			m_fBombAlpha = 1.0f;
		}
		else if ( m_bBombExploded )
		{
			m_fBombAlpha = 0;
			m_fBombSeenTime = TIMER_INIT;
		}
		else
		{
			m_fBombAlpha = clamp( 1.0f - ( now - m_fBombSeenTime ) / BOMB_FADE_TIME, 0.0f, 1.0f );
			if ( m_fBombAlpha > 0 )
			{
				drawBomb = true;
			}
			else
			{
				m_fBombSeenTime = TIMER_INIT;
			}
		}

		if ( drawBomb )
		{
			m_bShowBombHighlight = !m_bBombDefused && ( m_bBombPlanted || m_bBombDropped );
		}
	}
	else if ( CSGameRules()->IsHostageRescueMap() )
	{
		m_bShowingHostageZone = pLocalPlayer->IsInHostageRescueZone();
	}

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::ApplySpectatorModes( void )
{
	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();

	if ( !pLocalPlayer )
		return;

	m_bShowAll = false;

	if ( pLocalPlayer )
	{
		m_iObserverMode = pLocalPlayer->GetObserverMode();
		if ( pLocalPlayer->IsSpectator() || pLocalPlayer->IsHLTV() )
		{
			m_bShowAll = true;
		}

		// in survival, you can only spectate your teammate while they're alive
		// don't show all if you're spectating teammate
		if ( CSGameRules()->IsPlayingSurvival() && !CSGameRules()->IsWarmupPeriod() )
		{
			if ( !pLocalPlayer->IsHLTV() )
			{
				C_BaseEntity *pObserverTarget = pLocalPlayer->GetObserverTarget();
				if ( pObserverTarget && pObserverTarget->IsPlayer() )
				{
					m_bShowAll = pLocalPlayer->IsOtherEnemy( ToCSPlayer( pObserverTarget) );
				}
			}
		}
	}

	// 	if ( m_iObserverMode != OBS_MODE_NONE )
	// 	{
	// 		// if we're observing in casual, then show everyone everywhere
	// 		ConVarRef mp_forcecamera( "mp_forcecamera" );
	// 		m_bShowAll = mp_forcecamera.GetInt() == OBS_ALLOW_ALL;
	// 	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::UpdateGrenades( void )
{
	for ( int i = 0; i <= m_iLastGrenadeIndex; i++ )
	{
		CCSGO_MapOverviewIconPackage* pPackage = GetRadarGrenade( i );

		if ( pPackage && pPackage->m_bIsActive )
		{
			SetIconPackagePosition( pPackage );
			if ( !pPackage->IsVisible() )
			{
				RemoveGrenade( i );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::UpdateSurvivalCrates( void )
{
	for ( int i = 0; i <= m_iLastSurvivalCrateIndex; i++ )
	{
		CCSGO_MapOverviewIconPackage* pPackage = GetRadarSurvivalCrate( i );

		if ( pPackage && pPackage->m_bIsActive )
		{
			C_BaseEntity * pEntity = ClientEntityList().GetBaseEntity( pPackage->m_iEntityID );
			if ( pEntity && !pEntity->IsDormant() )
			{
				pPackage->m_Position = pEntity->GetNetworkOrigin();
			}

			SetIconPackagePosition( pPackage );
			if ( !pPackage->IsVisible() )
			{
				RemoveSurvivalCrate( i );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_MapOverview::ProcessInput()
{
	LazyCreateGoalIcons();

	LazyCreatePlayerIcons();

	ApplySpectatorModes();

	PositionRadarViewpoint();

	PlaceGoalIcons();

	PlacePlayers();

	PlaceHostages();

	UpdateGrenades();

	UpdateSurvivalCrates();

	UpdateAllDefusers();

	UpdateMiscIcons();

	SetupIconsFromStates();

	UpdateDangerZoneOverlayVisibility();
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CCSGO_MapOverview::AllowMapDrawing()
{
	if ( mapoverview_allow_client_draw.GetBool() )
		return true;

	if ( sv_competitive_official_5v5.GetBool() )
		return true;

	if ( C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer() )
	{
		if ( pPlayer->IsHLTV() )
			return true;

		if ( pPlayer->IsSpectator() )
		{
			if ( CSGameRules() && CSGameRules()->IsQueuedMatchmaking() )
				return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
float CCSGO_MapOverview::GetWorldDistance( float x1, float y1, float x2, float y2 )
{
	Vector vecStart;
	Vector vecEnd;
	RadarToWorld( Vector( x1, y1, 0 ), vecStart );
	RadarToWorld( Vector( x2, y2, 0 ), vecEnd );

	float flDist = vecStart.AsVector2D().DistTo( vecEnd.AsVector2D() );

	return flDist;
}
