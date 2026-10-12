//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "cbase.h"
#include "csgo_hud.h"
#include "csgo_hudradar.h"
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
#include "csgo_hudteamcounter.h"
#include "../csgo_scoreboard.h"
#include "gametypes.h"
#include "eventlist.h"
#include <game/client/iviewport.h>
#include "mathlib/mathlib.h"
#include "panorama/csgo_mapoverview.h"

// TODONOSF
#ifdef INCLUDE_SCALEFORM
#include "HUD/sfhudfreezepanel.h" // TODO - panorama version
#endif	// INCLUDE_SCALEFORM

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

REGISTER_PANEL2D_FACTORY( CCSGO_HudRadar, CSGOHudRadar );

DECLARE_HUD_MESSAGE( CCSGO_HudRadar, ProcessSpottedEntityUpdate );


using namespace panorama;

static const float DEAD_FADE_TIME = 4.0f;
static const float GHOST_FADE_TIME = 6.0f;
static const float BOMB_FADE_TIME = 8.0f;
static const float DEFUSER_FADE_TIME = 8.0f;
static const float TIMER_INIT = -1000.0f;
static const int   ABOVE_BELOW_HEIGHT = 94;
const float BOMB_PULSE_TIME = 0.5f;
const float BOMB_PULSE_TIME_FAST = 0.5f;

static const int   INVALID_INDEX = -1;

extern ConVar cl_draw_only_deathnotices;
extern ConVar cl_radar_square_with_scoreboard;
extern ConVar cl_radar_rotate;
extern ConVar cl_radar_scale;
extern ConVar cl_radar_always_centered;
extern ConVar cl_radar_icon_scale_min;
extern ConVar cl_drawhud;
extern ConVar cl_drawhud_force_radar;
extern ConVar cl_hud_color;
extern ConVar cl_hud_radar_scale;
extern bool IsTakingAFreezecamScreenshot( void );

// Additional scale factor applied to the radar to mimic the behavior of the CSGO Scaleform Hud
#define SF_TO_PANORAMA_RADAR_SCALE (768.0f / 320.0f)  // 320 is the 'map size' defined by Scaleform, so this define gives us the map size relative to the base layout size used by SF

//-----------------------------------------------------------------------------
// symbol definitions
//-----------------------------------------------------------------------------

class CCSGO_HudRadar_Symbols
{
public:
	panorama::CPanoramaSymbol sym__active;

	CCSGO_HudRadar_Symbols() :
		sym__active( "hud-radar__active" )
	{
	}
};

/*static*/ const CCSGO_HudRadar_Symbols& CCSGO_HudRadar::Symbols()
{
	static const CCSGO_HudRadar_Symbols s_symbols; // initialized on first construction here
	return s_symbols;
}

//-----------------------------------------------------------------------------
// data definitions
//-----------------------------------------------------------------------------

static const char* playerIconNames[] = {
	"PlayerNumber",
	"PlayerLetter",
	"SpeakingOnMap",
	"SpeakingOffMap",
	"AbovePlayer",
	"BelowPlayer",
	"HostageTransitOnMap",
	"CTOnMap",
	"CTOffMap",
	"CTDeath",
	"CTGhost",
	"TOnMap",
	"TOffMap",
	"TDeath",
	"TGhost",
	"EnemyOnMap",
	"EnemyOffMap",
	"EnemyDeath",
	"EnemyGhost",
	"HostageOnMap",
	"HostageOffMap",
	"HostageDeath",
	"HostageGhost",
	"DirectionalIndicator",
	"Defuser",
	"Selected",
	"ViewFrustrum",
	NULL
};

static const char* hostageIconNames[] = {
	"HostageDead",
	"HostageRescued",
	"HostageAlive",
	"HostageTransit",
	NULL
};

static const char* radarIconNames[] = {
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
	"bot_takeover",
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
	BOT_TAKEOVER,
};

//-----------------------------------------------------------------------------
// callback and function declarations
//-----------------------------------------------------------------------------

static bool maplessfunc(const char* const & s1, const char* const & s2)
{
	return V_strcmp(s1, s2) < 0;
}

CUtlMap<const char*, int> CCSGO_HudRadar::m_messageMap(maplessfunc);


CON_COMMAND( drawradar, "Draws HUD radar" )
{
	( GET_HUDELEMENT( CCSGO_HudRadar ) )->ShowRadar( true );
}

CON_COMMAND( hideradar, "Hides HUD radar" )
{
	( GET_HUDELEMENT( CCSGO_HudRadar ) )->ShowRadar( false );
}

CON_COMMAND( cl_reload_hud, "Reloads the hud scale and resets scale and borders" )
{
	( GET_HUDELEMENT( CCSGO_HudRadar ) )->GetLayoutDefines();
}

//-----------------------------------------------------------------------------
// Transform helpers
//-----------------------------------------------------------------------------

void Helper_SetPanelTransform3D( panorama::CPanel2D *pPanel, Vector &vTranslation, float fRotation, float fScale )
{
	if ( !pPanel )
		return;

	CUtlVector<CTransform3D *> vecTransforms;

	// Scale
	if ( fScale != 1.0f )
		vecTransforms.AddToTail( new CTransformScale3D( fScale, fScale, 1.0f ) );

	// Rotation
	if ( fRotation != 0.0f )
		vecTransforms.AddToTail( new CTransformRotate3D( 0, fRotation, 0.0f ) );

	// Translation
	vecTransforms.AddToTail( new CTransformTranslate3D( vTranslation.x, vTranslation.y, 0.0f ) );

	// Set panel transform
	pPanel->SetTransform3DSimple( vecTransforms );
}

void Helper_SetPanelTransform3D_Translate( panorama::CPanel2D *pPanel, Vector &vTranslation )
{
	if ( !pPanel )
		return;

	CUtlVector<CTransform3D *> vecTransforms;

	// Translation
	vecTransforms.AddToTail( new CTransformTranslate3D( vTranslation.x, vTranslation.y, 0.0f ) );

	// Set panel transform
	pPanel->SetTransform3DSimple( vecTransforms );
}

// This alternative version of above added as a workaround for bug (JIRA#2045).
// Only as issue when switching between round and square radar
// where goal icons appear in the wrong place for one frame
void Helper_SetPanelTransform3D_Translate2( panorama::CPanel2D *pPanel, Vector &vTranslation )
{
	if ( !pPanel )
		return;

	CUtlVector<CTransform3D *> vecTransforms;

	// Translation
	vecTransforms.AddToTail( new CTransformTranslate3D ( vTranslation.x, vTranslation.y, 0.0f ) );

	// Set panel transform
	pPanel->SetTransform3D( vecTransforms );
}

void Helper_SetPanelTransform3D_Rotate( panorama::CPanel2D *pPanel, float fRotation )
{
	if ( !pPanel )
		return;

	CUtlVector<CTransform3D *> vecTransforms;

	// Rotation
	if ( fRotation != 0.0f )
	{
		vecTransforms.AddToTail( new CTransformRotate3D( 0, fRotation, 0.0f ) );

		// Set panel transform
		pPanel->SetTransform3DSimple( vecTransforms );
	}
}

void Helper_SetPanelTransform3D_Scale( panorama::CPanel2D *pPanel,  float fScale )
{
	if ( !pPanel )
		return;

	CUtlVector<CTransform3D *> vecTransforms;

	// Scale
	if ( fScale != 0.0f )
	{
		vecTransforms.AddToTail( new CTransformScale3D( fScale, fScale, 1.0f ) );

		// Set panel transform
		pPanel->SetTransform3DSimple( vecTransforms );
	}
}
//-----------------------------------------------------------------------------
// CCSGO_HudRadarIconPackage code
//-----------------------------------------------------------------------------

CCSGO_HudRadar::CCSGO_HudRadarIconPackage::CCSGO_HudRadarIconPackage()
{
	ClearAll();
}

CCSGO_HudRadar::CCSGO_HudRadarIconPackage::~CCSGO_HudRadarIconPackage()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRadar::CCSGO_HudRadarIconPackage::ClearAll( void )
{
	m_pIconPackage = NULL;
	m_pIconPackageRotate = NULL;
	V_memset( m_pIcons, 0, sizeof( m_pIcons ) );
	V_memset( m_pLabels, 0, sizeof( m_pLabels ) );

	m_iCurrentVisibilityFlags = 0;
	m_fCurrentAlpha = 1.0f;

	m_Health = 0;
	m_iPlayerType = 0;
	m_iIndex = INVALID_INDEX;
	m_iEntityID = 0;

	m_bIsActive = false;
	m_bOffMap = true;

	m_bIsSpotted = false;
	m_bIsSpottedByFriendsOnly = false;
	m_fGhostTime = TIMER_INIT;

	m_fLastColorUpdate = TIMER_INIT;

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

	m_HudPosition = vec3_origin;
	m_HudRotation = 0.0f;
	m_HudScale = 1.0f;

	m_bIsDefuser = false;

	m_bHostageIsUsed = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRadar::CCSGO_HudRadarIconPackage::Init( panorama::CPanel2D *pParent )
{
	if ( !pParent )
		return;

	if ( m_pIconPackage )
		return;

	switch ( m_IconPackType )
	{
	case 0:
		m_pIconPackage = new panorama::CPanel2D( pParent, CFmtStr( "Player%d", m_iIndex ) );
		break;
	case 1:
		m_pIconPackage = new panorama::CPanel2D( pParent, CFmtStr( "Hostage%d", m_iIndex ) );
		break;
	case 2:
		m_pIconPackage = new panorama::CPanel2D( pParent, CFmtStr( "Decoy%d", m_iIndex ) );
		break;
	case 3: 
		m_pIconPackage = new panorama::CPanel2D( pParent, CFmtStr( "Defuser%d", m_iIndex ) );
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

	// extra/label ptrs for player number and letter
	for ( int i = 0; i < PI_NUM_ICONS && playerIconNames[ i ] != NULL; i++ )
	{
		if ( i < PI_FIRST_ROTATED )
		{
			m_pLabels[ i ] = panorama::panel_cast<panorama::CLabel *>( m_pIconPackage->RequireChildInLayoutFile( playerIconNames[ i ] ) );
			if ( m_pLabels[i] )
			{
				m_pLabels[i]->SetVisible( ( m_iCurrentVisibilityFlags & ( 1 << i ) ) != 0 );

				switch ( i )
				{
				case PI_PLAYER_NUMBER:
					m_pLabels[i]->SetText( "O", panorama::CLabel::k_ETextTypeUnlocalized );
					break;
				case PI_PLAYER_LETTER:
					m_pLabels[i]->SetText( "o", panorama::CLabel::k_ETextTypeUnlocalized );
					break;
				default:
					Assert( false );
				}
			}
		}
		else
		{
			if ( m_pIconPackageRotate )
			{
				m_pIcons[ i ] = panorama::panel_cast<panorama::CImagePanel *>( m_pIconPackageRotate->RequireChildInLayoutFile( playerIconNames[ i ] ) );
			}
			if ( m_pIcons[ i ] )
			{
				// we're only doing this for the rotated icons for now.
				m_pIcons[ i ]->SetVisible( ( m_iCurrentVisibilityFlags & ( 1 << i ) ) != 0 );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRadar::CCSGO_HudRadarIconPackage::NukeFromOrbit( CCSGO_HudRadar* pSFUI )
{
	if ( m_pIconPackage )
		m_pIconPackage->DeleteAsync();

	ClearAll();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRadar::CCSGO_HudRadarIconPackage::StartRound( void )
{
	m_Health = 100;

	m_bOffMap = false;
	m_bIsSpotted = false;
	m_bIsSpottedByFriendsOnly = false;
	m_fGhostTime = TIMER_INIT;

	m_fLastColorUpdate = TIMER_INIT;

	m_bIsDead = false;
	m_fDeadTime = TIMER_INIT;

	m_fGrenExpireTime = TIMER_INIT;

	m_bIsRescued = false;

	m_nAboveOrBelow = R_SAMELEVEL;

	m_fRoundStartTime = gpGlobals->curtime;

	m_Position = vec3_origin;
	m_Angle.Init();

	SetAlpha( 0 );
	SetVisibilityFlags( 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------

bool CCSGO_HudRadar::CCSGO_HudRadarIconPackage::IsVisible( void )
{
	return ( m_fCurrentAlpha != 0 && m_iCurrentVisibilityFlags != 0 );
}

void CCSGO_HudRadar::CCSGO_HudRadarIconPackage::SetIsPlayer( bool value )
{
	m_bIsPlayer = value;
}

void CCSGO_HudRadar::CCSGO_HudRadarIconPackage::SetIsSelected( bool value )
{
	m_bIsSelected = value;
}

void CCSGO_HudRadar::CCSGO_HudRadarIconPackage::SetIsSpeaking( bool value )
{
	m_bIsSpeaking = value && m_bIsOnLocalTeam;
}

void CCSGO_HudRadar::CCSGO_HudRadarIconPackage::SetIsOffMap( bool value )
{
	m_bOffMap = value;
}

void CCSGO_HudRadar::CCSGO_HudRadarIconPackage::SetIsAboveOrBelow( int value )
{
	m_nAboveOrBelow = value;
}

void CCSGO_HudRadar::CCSGO_HudRadarIconPackage::SetIsMovingHostage( bool value )
{
	m_bIsMovingHostage = value;
}

void CCSGO_HudRadar::CCSGO_HudRadarIconPackage::SetIsRescued( bool value )
{
	m_bIsRescued = value;
}

void CCSGO_HudRadar::CCSGO_HudRadarIconPackage::SetIsOnLocalTeam( bool value )
{
	m_bIsOnLocalTeam = value;
}

void CCSGO_HudRadar::CCSGO_HudRadarIconPackage::SetIsBot( bool value )
{
	m_bIsBot = value;
}

void CCSGO_HudRadar::CCSGO_HudRadarIconPackage::SetIsControlledBot( void )
{
	m_bIsDead = true;
	//m_bIsSpotted = false;
	m_bIsSpotted = true;
	m_fDeadTime = TIMER_INIT;
	m_fGhostTime = TIMER_INIT;
	m_fLastColorUpdate = TIMER_INIT;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRadar::CCSGO_HudRadarIconPackage::SetIsDead( bool value )
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

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRadar::CCSGO_HudRadarIconPackage::SetGrenadeExpireTime( float value )
{
	if ( value )
	{
		m_fGrenExpireTime = value;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRadar::CCSGO_HudRadarIconPackage::SetIsSpotted( bool value )
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

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRadar::CCSGO_HudRadarIconPackage::SetIsSpottedByFriendsOnly( bool value )
{
	if ( gpGlobals->curtime - m_fRoundStartTime > 0.25f )
	{
		m_bIsSpottedByFriendsOnly = value;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRadar::CCSGO_HudRadarIconPackage::SetPlayerTeam( int team )
{
	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( pLocalPlayer == NULL )
	{
		return;
	}

	int newType;
	bool bSpec = ( ( pLocalPlayer && pLocalPlayer->IsSpectator() ) || engine->IsHLTV() );

	switch ( team )
	{
	case TEAM_UNASSIGNED:
		newType = PI_HOSTAGE;
		break;

	case TEAM_TERRORIST:
		newType = pLocalPlayer->GetAssociatedTeamNumber() == TEAM_TERRORIST || bSpec ? PI_T : PI_ENEMY;
		break;

	case TEAM_CT:
		newType = pLocalPlayer->GetAssociatedTeamNumber() == TEAM_CT || bSpec ? PI_CT : PI_ENEMY;
		break;

	case TEAM_SPECTATOR:
	default:
		newType = 0;
		break;
	}

	if ( m_iPlayerType != newType )
	{
		m_iPlayerType = newType;
		SetVisibilityFlags( 0 );
	}
}

//TODO - remove nameclash with sfhud
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool Helper_ShouldShowAssassinationTargetIcon_Panorama( int iEntIndex )
{
	C_CSPlayer *pTarget = (C_CSPlayer*)UTIL_PlayerByIndex( iEntIndex );
	C_CS_PlayerResource *pCSPR = GetCSResources();
	if ( !pTarget || !pCSPR )
		return false;

	return ( !pCSPR->IsControllingBot( iEntIndex ) &&
			 pTarget->IsAssassinationTarget() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRadar::CCSGO_HudRadarIconPackage::SetupIconsFromStates( void )
{
	int flags = 0;
	float alpha = 1;

	if ( m_iPlayerType != 0 )
	{
		flags = 1 << m_iPlayerType;

		if ( m_bIsPlayer )
		{
			flags |= 1 << PI_VIEWFRUSTRUM;
		}

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
						flags <<= 2;
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

				if ( m_bOffMap )
				{
					flags <<= 1;
				}
				else
				{
					flags <<= 3;
				}
			}
		}
		else
		{
			C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
			bool bSpec = ( ( pLocalPlayer && pLocalPlayer->IsSpectator() ) || engine->IsHLTV() );

			bool bShowLetters = ( !bSpec && m_iPlayerType != PI_ENEMY && !m_bOffMap && !IsHostageType() && !IsDecoyType() );
			if ( bSpec )
			{
				flags |= 1 << PI_PLAYER_NUMBER;
			}

			// this decides if the letter should be shown
			if ( bShowLetters || Helper_ShouldShowAssassinationTargetIcon_Panorama( m_iEntityID ) )
			{
				flags |= 1 << PI_PLAYER_LETTER;
			}

			if ( m_bIsMovingHostage )
			{
				flags |= 1 << PI_HOSTAGE_MOVING;
			}

			if ( m_bIsSpeaking )
			{
				flags |= 1 << PI_SPEAKING;
			}

			if ( m_bIsSelected )
			{
				flags |= 1 << PI_SELECTED;
			}

			if ( bSpec == false && m_iPlayerType == PI_ENEMY && ( m_bIsSpottedByFriendsOnly == false || pLocalPlayer->IsAlive() == false ) )
			{
				// TODO - sanity check this, right now it just seems to double up on rendering PI_ENEMY??!
				// disabling for now
				//flags |= 1 << PI_ENEMY_SEELOCAL;
			}

			// this will shift the MOVING and SPEAKING icons to their
			// offscreen versions as well ( look at the enum declaration )
			if ( m_bOffMap )
			{
				flags <<= 1;
			}
			else
			{
				if ( m_nAboveOrBelow == R_ABOVE )
				{
					flags |= 1 << PI_ABOVE;
				}
				else if ( m_nAboveOrBelow == R_BELOW )
				{
					flags |= 1 << PI_BELOW;
				}
				else
				{
					if ( !bSpec && /*!bShowNumbers &&*/ m_iPlayerType != PI_ENEMY && !(IsHostageType() || IsDecoyType()) )
					{
						flags |= 1 << PI_DIRECTION_INDICATOR;
					}
				}
			}
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

	SetAlpha( alpha );
	SetVisibilityFlags( flags );
	UpdateIconsPosition();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRadar::CCSGO_HudRadarIconPackage::SetVisibilityFlags( int newFlags )
{
	newFlags &= ( ( 1 << PI_NUM_ICONS ) - 1 );

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

			for ( int i = 0; i < PI_FIRST_ROTATED && ( diffFlags != 0 ); i++, diffFlags >>= 1 )
			{
				if ( ( diffFlags & 1 ) && ( m_pLabels[ i ] ) )
				{
					m_pLabels[ i ]->SetVisible( ( newFlags & ( 1 << i ) ) != 0 );
				}
			}
			for ( int i = PI_FIRST_ROTATED; i < PI_NUM_ICONS && ( diffFlags != 0 ); i++, diffFlags >>= 1 )
			{
				if ( ( diffFlags & 1 ) && ( m_pIcons[ i ] ) )
				{
					// we're only doing this for the rotated icons for now.
					m_pIcons[ i ]->SetVisible( ( newFlags & ( 1 << i ) ) != 0 );
				}
			}
		}

		m_iCurrentVisibilityFlags = newFlags;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRadar::CCSGO_HudRadarIconPackage::UpdateIconsPosition( void )
{
	Vector vTrans = vec3_origin;
	float fRot = 0.0f;
	float fScale = 1.0f;

	vTrans = m_HudPosition;
	fScale = m_HudScale;

	if ( m_pIconPackageNonRotate )
	{
		Helper_SetPanelTransform3D_Translate( m_pIconPackageNonRotate, vTrans );
	}

	if ( m_pIconPackageRotate )
	{
		fRot = m_HudRotation;
		Helper_SetPanelTransform3D( m_pIconPackageRotate, vTrans, fRot, fScale );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRadar::CCSGO_HudRadarIconPackage::SetAlpha( float newAlpha )
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
void CCSGO_HudRadar::CCSGO_HudRadarIconPackage::SetIsDefuse( bool bValue )
{
	m_bIsDefuser = bValue;
}

/**********************************************************************
**********************************************************************
**********************************************************************
**********************************************************************
**********************************************************************
* SFHudHostageIcon
*/

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCSGO_HudRadar::CCSGO_HudRadarHostageIcons::CCSGO_HudRadarHostageIcons()
{
	m_iCurrentIcon = HI_UNUSED;

	m_pIconPackage = NULL;
	V_memset( m_pIcons, 0, sizeof( m_pIcons ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCSGO_HudRadar::CCSGO_HudRadarHostageIcons::~CCSGO_HudRadarHostageIcons()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRadar::CCSGO_HudRadarHostageIcons::Init( panorama::CPanel2D *pParent, const char *szHostageIconName )
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
void CCSGO_HudRadar::CCSGO_HudRadarHostageIcons::SetStatus( int status )
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
CCSGO_HudRadar::CCSGO_HudRadar( CPanel2D *pParent, const char *pchID )
	: CPanoramaHudElement( "CCSGO_HudRadar", this ),
	panorama::CPanel2D( pParent, pchID ),

	m_bVisible( false ),
	m_fMapSize( 1.0f ),
	m_fRadarSize( 1.0f ),
	m_fRoundRadarSize( 1.0f ),
	m_fPixelToRadarScale( 1.0f ),
	m_fWorldToPixelScale( 1.0f ),
	m_fWorldToRadarScale( 1.0f ),
	m_fRadarPanelSize( 300.0f ),
	m_fHudPosRadarPanelCenterOffset( 150.0f ),
	m_pRadar( NULL ),
	m_pLocationText( NULL ),
	m_iNumGoalIcons( 0 ),
	m_iLastPlayerIndex( INVALID_INDEX ),
	m_iLastHostageIndex( INVALID_INDEX ),
	m_iLastDecoyIndex( INVALID_INDEX ),
	m_iLastDefuserIndex( INVALID_INDEX ),
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
	m_bBombPlanted( false ),
	m_bBombDropped( false ),
	m_nBombEntIndex( -1 ),
	m_nBombHolderUserId( -1 ),
	m_bShowBombHighlight( false ),
	m_bShowRadar( true ),
	m_bShowAll( false ),
	m_iObserverMode( OBS_MODE_NONE ),
	m_bTrackDefusers( false ),
	m_MapOrigin( vec3_origin ),
	m_fMapSourceImageSize( 1024.0f ),
	m_bRound( true ),
	m_nVisibleLayer( 0 ),
	m_pLayeredRadar( NULL ),
	m_bIsShown( true ),
	m_flLastC4ResetPos( 0.0f ),
	m_flSafeZoneY( 0.0f ),
	m_bShowColorLetter( false )
{
	Init();

	RequireLoadLayout( "file://{resources}/layout/hud/hudradar.xml" );

	RegisterForUnhandledEvent( panorama::ReloadStyleFile(), this, &CCSGO_HudRadar::OnStyleFileReloaded );
	GetLayoutDefines();

	SetHiddenBits( HIDEHUD_RADAR );
	m_bWantLateUpdate = true;

	m_cDesiredMapName[ 0 ] = 0;
	m_cLoadedMapName[ 0 ] = 0;
	m_szLocationString[0] = 0;

	V_memset( m_pBombZoneIcons, 0, sizeof( m_pBombZoneIcons ) );
	V_memset( m_pHostageZoneIcons, 0, sizeof( m_pHostageZoneIcons ) );
	V_memset( m_GoalIcons, 0, sizeof( m_GoalIcons ) );
	V_memset( m_pIcons, 0, sizeof( m_pIcons ) );

	m_EntitySpotted.ClearAll();

	m_nCurrentRadarVerticalSection = -1;
	m_vecRadarVerticalSections.RemoveAll();
	m_aMapLevels[ 0 ].RemoveAll();
	m_aMapLevels[ 1 ].RemoveAll();

	SetInputNamespace( "csgo_hudradar" );

	m_pRadar = panorama::panel_cast<CPanel2D *>( FindChildInLayoutFile( "Radar" ) );

	m_pRadarInvisible = FindChildInLayoutFile( "Radar__Invisible" );

	m_pRadarRound = panorama::panel_cast<CPanel2D *>( FindChildInLayoutFile( "Radar__Round" ) );
	m_pRadarSquare = panorama::panel_cast<CPanel2D *>( FindChildInLayoutFile( "Radar__Square" ) );

 	m_pRadarRound->SetVisible( true );
 	m_pRadarSquare->SetVisible( false );

	// Square
	m_aMapContainerPanel[ 0 ] = panorama::panel_cast<CPanel2D *>( FindChildInLayoutFile( "Radar__Square" ) );
	m_aMapBorder[ 0 ] = panorama::panel_cast<CPanel2D *>( FindChildInLayoutFile( "Radar__Square--Border" ) );
	m_aMapStyle[ 0 ] = panorama::panel_cast<CPanel2D *>( FindChildInLayoutFile( "Radar__Square--Inner" ) );
	m_aMapStyleTransform[ 0 ] = panorama::panel_cast<CPanel2D *>( FindChildInLayoutFile( "Radar__Square--InnerTransform" ) );

	// Round
	m_aMapContainerPanel[ 1 ] = panorama::panel_cast<CPanel2D *>( FindChildInLayoutFile( "Radar__Round" ) );
	m_aMapBorder[ 1 ] = panorama::panel_cast<CPanel2D *>( FindChildInLayoutFile( "Radar__Round--Border" ) );
	m_aMapStyle[ 1 ] = panorama::panel_cast<CPanel2D *>( FindChildInLayoutFile( "Radar__Round--Inner" ) );
	m_aMapStyleTransform[ 1 ] = panorama::panel_cast<CPanel2D *>( FindChildInLayoutFile( "Radar__Round--InnerTransform" ) );

	static const panorama::CPanoramaSymbol k_symRadarBombPlantPulse( "RadarBombPlantPulse" );
	if ( m_aMapBorder[1]->BHasClass( k_symRadarBombPlantPulse ) == true )
		m_aMapBorder[1]->RemoveClass( k_symRadarBombPlantPulse );

	m_pDirectionArrow = FindChildInLayoutFile( "DirectionArrow" );

	m_aMapContainerPanel[ 0 ]->SetVisible( false );
	m_aMapContainerPanel[ 1 ]->SetVisible( false );
	m_pDirectionArrow->SetVisible( false );

	m_pMapImg[0] = NULL;
	m_pMapImg[1] = NULL;

	m_pBombDefuserPackage = panorama::panel_cast<CPanel2D *>( RequireChildInLayoutFile( "RI_BombDefuserPackage" ) );

	m_pIcons[ RI_BOMB_IS_PLANTED ] = panorama::panel_cast<CPanel2D *>( RequireChildInLayoutFile( "BombPlantedIcon" ) );
	m_pIcons[ RI_BOMB_IS_PLANTED_MEDIUM ] = panorama::panel_cast<CPanel2D *>( RequireChildInLayoutFile( "BombPlantedIconMedium" ) );
	m_pIcons[ RI_BOMB_IS_PLANTED_FAST ] = panorama::panel_cast<CPanel2D *>( RequireChildInLayoutFile( "BombPlantedIconFast" ) );
	m_pIcons[ RI_BOMB_ICON_PLANTED ] = panorama::panel_cast<CPanel2D *>( RequireChildInLayoutFile( "PlantedBomb" ) );
	m_pIcons[ RI_BOMB_ICON_DROPPED ] = panorama::panel_cast<CPanel2D *>( RequireChildInLayoutFile( "DroppedBomb" ) );
	m_pIcons[ RI_BOMB_ICON_BOMB_ABOVE ] = panorama::panel_cast<CPanel2D *>( RequireChildInLayoutFile( "BombAbove" ) );
	m_pIcons[ RI_BOMB_ICON_BOMB_BELOW ] = panorama::panel_cast<CPanel2D *>( RequireChildInLayoutFile( "BombBelow" ) );
	m_pIcons[ RI_BOMB_ICON_PACKAGE ] = panorama::panel_cast<CPanel2D *>( RequireChildInLayoutFile( "CreateBombPack" ) );
	m_pIcons[ RI_DEFUSER_ICON_DROPPED ] = panorama::panel_cast<CPanel2D *>( RequireChildInLayoutFile( "DefuserIconDropped" ) );
	m_pIcons[ RI_DEFUSER_ICON_PACKAGE ] = panorama::panel_cast<CPanel2D *>( RequireChildInLayoutFile( "DefuserIconPackage" ) );

	char cHostageZoneIconName[ 20 ] = { "HZone0" };

	for ( int i = 0; i < MAX_HOSTAGE_RESCUES; i++ )
	{
		cHostageZoneIconName[ 5 ] = '0' + i;

		m_pHostageZoneIcons[ i ] = panorama::panel_cast<CImagePanel *>( RequireChildInLayoutFile( cHostageZoneIconName ) );
	}

	m_pBombZoneIcons[ 0 ] = panorama::panel_cast<CImagePanel *>( RequireChildInLayoutFile( "BombZoneA" ) );
	m_pBombZoneIcons[ 1 ] = panorama::panel_cast<CImagePanel *>( RequireChildInLayoutFile( "BombZoneB" ) );

	m_pLocationText = panorama::panel_cast<panorama::CLabel *>( RequireChildInLayoutFile( "DashboardLabel" ) );
	if ( m_pLocationText )
	{
		m_pLocationText->SetText( m_szLocationString );
	}

	// Radar looks laggy when not updating transform every tick
	// TODO: Maybe update transform in CSGOFrameUpdate() but update other things less often?
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CCSGO_HudRadar::~CCSGO_HudRadar()
{
	UnregisterForUnhandledEvent( panorama::ReloadStyleFile(), this, &CCSGO_HudRadar::OnStyleFileReloaded );

	m_aMapLevels[ 0 ].Purge();
	m_aMapLevels[ 1 ].Purge();

	m_vecRadarVerticalSections.Purge();
}

//-----------------------------------------------------------------------------
// fRadarSize - size of map within radar in pixels relative to 1080
//-----------------------------------------------------------------------------
void CCSGO_HudRadar::UpdateRadarScalingValues( float fRadarVisSize, float fRadarPanelSize )
{
	m_fRadarSize = fRadarVisSize;
	m_fMapSize = SF_TO_PANORAMA_RADAR_SCALE * m_fRadarSize;

	m_fPixelToRadarScale = m_fMapSize / m_fMapSourceImageSize; 

	m_fWorldToRadarScale = m_fWorldToPixelScale * m_fPixelToRadarScale;

	m_fRadarPanelSize = fRadarPanelSize;
	m_fHudPosRadarPanelCenterOffset = m_fRadarPanelSize * 0.5f;

	float fRadarRadius = ( m_fRadarSize * 0.98f ) * 0.5f;
	m_fRadarRadiusSq = fRadarRadius * fRadarRadius;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CCSGO_HudRadar::GetLayoutDefines()
{
	int nRadarMapVisSize, nRadarPanelSize;

	if ( m_bRound )
	{
		nRadarMapVisSize = GetLayoutFileDefineInt( "Radar_VisSize_Round", 250 );
	}
	else
	{
		nRadarMapVisSize = GetLayoutFileDefineInt( "Radar_VisSize_Square", 290 );
	}

	nRadarPanelSize = GetLayoutFileDefineInt( "Radar_PanelSize", 300 );

	UpdateRadarScalingValues( (float)nRadarMapVisSize, (float)nRadarPanelSize );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CCSGO_HudRadar::OnStyleFileReloaded( panorama::CPanoramaSymbol symFile )
{
	GetLayoutDefines();

	// let bubble
	return false;
}

//-----------------------------------------------------------------------------
// hud element functions
//-----------------------------------------------------------------------------
void CCSGO_HudRadar::Init()
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

	HOOK_HUD_MESSAGE( CCSGO_HudRadar, ProcessSpottedEntityUpdate );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRadar::LevelInit( void )
{
 	for ( int i = 0; i < RI_NUM_ICONS; i++ )
 	{
 		if ( m_pIcons[ i ] )
 		{
			m_pIcons[ i ]->SetVisible( false );
 		}
 	}

 	ResetForNewMap();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRadar::LevelShutdown( void )
{
	// ensure all map layers not visible
	FOR_EACH_VEC( m_aMapLevels[ 0 ], i )
	{
		m_aMapLevels[ 0 ][ i ]->SetVisible( false );
		m_aMapLevels[ 1 ][ i ]->SetVisible( false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCSGO_HudRadar::ShouldDraw( void )
{
	if( IsTakingAFreezecamScreenshot() )
		return false;

	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
	if( !pPlayer )
		return false;

	bool bSpec = pPlayer->IsSpectator() || engine->IsHLTV();

	if( bSpec == false )
	{
// 		//Don't show the radar while the win panel is up (only look at slot 0 because that is the only "true" Win Panel)
// 		SFHudWinPanel * pWinPanel = ( SFHudWinPanel* ) ( GetHud( 0 ).FindElement( "SFHudWinPanel" ) );
// 		if ( pWinPanel && pWinPanel->IsVisible( ) )
// 		{
// 			return false;
// 		}

		if( pPlayer->GetTeamNumber() == TEAM_UNASSIGNED ||
			(pPlayer->GetTeamNumber() == TEAM_UNASSIGNED && pPlayer->GetPendingTeamNumber() != TEAM_SPECTATOR) )
			return false;
	}

	// don't draw radar if mapoverview is active
	if ( GET_HUDELEMENT( CCSGO_MapOverview )->IsActive() )
		return false;

	static ConVarRef sv_disable_radar( "sv_disable_radar" );
	if( sv_disable_radar.GetBool() )
		return false;

	return /*m_bShowRadar && */ (cl_drawhud_force_radar.GetInt() >= 0) && (
		(cl_drawhud_force_radar.GetInt() > 0) ||
		(cl_drawhud.GetBool() && cl_draw_only_deathnotices.GetBool() == false)
		) && CHudElement::ShouldDraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRadar::SetActive( bool bActive )
{
	if ( bActive != m_bVisible )
	{
		Show( bActive );
		Assert( m_bVisible == bActive ); // Show failed?
	}

	CHudElement::SetActive( bActive );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRadar::Show( bool bShow )
{
	SetHasClass( Symbols().sym__active, bShow );

	if ( bShow )
	{
		Color pickcolor = Color( 0xD3, 0xE7, 0x98, 0x4D );
		Color loccolor = Color( 0xD3, 0xE7, 0x98, 0xFF );

		if ( cl_hud_color.GetInt() != 0 )
		{
			CCSGO_Hud *pHud = panorama::panel_cast<CCSGO_Hud *>( CUI_Root::GetRootForWindow( GetParentWindow() ) );

			pHud->GetBGHudTextColor( &pickcolor, 0.5f, 0.85f );
			pHud->GetBGHudTextColor( &loccolor, 0.5f, 0.85f );
		}

		panorama::IUIPanelStyle *pPanelStyle = m_pLocationText->AccessStyle();
		pPanelStyle->SetSimpleWashColor( loccolor, true );

		pPanelStyle = m_aMapBorder[ 0 ]->AccessStyle();
		pPanelStyle->SetBorderColor( pickcolor );

		//disabling this because we hijacked this panel to show the bomb red pulse
		//pPanelStyle = m_aMapBorder[ 1 ]->AccessStyle();
		//pPanelStyle->SetBorderColor( pickcolor );

		for ( int i = 0; i <= MAX_PLAYERS; ++i )
		{
			CCSGO_HudRadarIconPackage* pPackage = GetRadarPlayer( i );

 			if ( pPackage )
 				pPackage->m_fLastColorUpdate = TIMER_INIT;
		}

		UpdateAllPlayerNumbers();

		m_bVisible = true;
	}
	else
	{
		m_bVisible = false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRadar::LazyCreateGoalIcons( void )
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

	m_iNumGoalIcons = 0;

	if ( CSGameRules()->IsBombDefuseMap() )
	{
		// init these as hidden
		m_pBombZoneIcons[ 0 ]->SetVisible( false );
		m_pBombZoneIcons[ 1 ]->SetVisible( false );

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
bool CCSGO_HudRadar::OnImageLoaded( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, panorama::IImageSource *pImage )
{
	if ( !m_pMapImg[0] )
		return false;

	if ( pPanel.Get() == m_pMapImg[0]->UIPanel() )
	{
		//InvalidateSizeAndPosition();
		if ( m_pMapImg[0]->GetImage() == pImage )
		{
			m_fMapSourceImageSize = m_pMapImg[0]->GetImage()->GetWidth();
			UpdateRadarScalingValues( m_fRadarSize, m_fRadarPanelSize );
		}

		static CPanoramaSymbol k_symAntialias( "antialias" );
		IUIPanelClient* pMapImgPanel = m_pMapImg[0];
		pMapImgPanel->BSetProperty( k_symAntialias, "false" );
	}

	// Always return false to ensure the ImagePanel event handler gets called next
	return false;
}

//-----------------------------------------------------------------------------
// set up the background map
//-----------------------------------------------------------------------------
void CCSGO_HudRadar::SetMap( const char* pMapName )
{
	// reset old
	m_nCurrentRadarVerticalSection = -1;
	m_aMapLevels[ 0 ].PurgeAndDeleteElements();
	m_aMapLevels[ 1 ].PurgeAndDeleteElements();
	m_vecRadarVerticalSections.Purge();
	m_pMapImg[0] = NULL;
	m_pMapImg[1] = NULL;

	KeyValues* MapKeyValues = new KeyValues(pMapName);
	KeyValues::AutoDelete MapKeyValuesDelete( MapKeyValues );

	char tempfile[MAX_PATH];
	Q_snprintf(tempfile, sizeof(tempfile), "resource/overviews/%s.txt", pMapName);

	if (!MapKeyValues->LoadFromFile(g_pFullFileSystem, tempfile, "GAME"))
	{
		DevMsg(1, "Error! CMapOverview::SetMap: couldn't load file %s.\n", tempfile);
	}

	KeyValues* pSections = MapKeyValues->FindKey( "verticalsections" );
	if ( pSections )
	{
 		int nIndex = 0;
		for ( KeyValues *kSection = pSections->GetFirstSubKey(); kSection != NULL; kSection = kSection->GetNextKey() )
		{
			float flAltMin = kSection->GetFloat( "AltitudeMin", 0.0f );
			float flAltMax = kSection->GetFloat( "AltitudeMax", 0.0f );

			if ( flAltMin < flAltMax )
			{
				HudRadarLevelVerticalSection_t *pNewSection = &m_vecRadarVerticalSections[ m_vecRadarVerticalSections.AddToTail() ];

				if ( !V_strcmp( kSection->GetName(), "default" ) )
				{
					V_strcpy_safe( pNewSection->m_szSectionName, pMapName );
				}
				else
				{
					V_sprintf_safe( pNewSection->m_szSectionName, "%s_%s", pMapName, kSection->GetName() );
				}

				pNewSection->m_nSectionIndex = nIndex;
				pNewSection->m_flSectionAltitudeFloor = flAltMin;
				pNewSection->m_flSectionAltitudeCeiling = flAltMax;
				nIndex++;

				CUtlString szImageURL;
				szImageURL.Format( "file://{images_overviews}/%s_radar.dds", pNewSection->m_szSectionName );

				panorama::CImagePanel *pMapLevel;
				pMapLevel = m_aMapLevels[0][m_aMapLevels[0].AddToTail( new panorama::CImagePanel( m_aMapStyleTransform[0], CFmtStr( "Radar__Round--MapLevel%d", pNewSection->m_nSectionIndex ) ) )];
				if ( pMapLevel )
				{
					pMapLevel->RequireLoadLayoutSnippet( "RadarImageSnippet" );
					RegisterEventHandlerOnPanel( ImageLoaded(), pMapLevel->UIPanel(), this, &CCSGO_HudRadar::OnImageLoaded );
					pMapLevel->SetImage( szImageURL );
					pMapLevel->SetVisible( false );
				}

 				pMapLevel = m_aMapLevels[ 1 ][ m_aMapLevels[ 1 ].AddToTail( new panorama::CImagePanel( m_aMapStyleTransform[ 1 ], CFmtStr( "Radar__Round--MapLevel%d", pNewSection->m_nSectionIndex ) ) ) ];
 				if ( pMapLevel )
 				{
 					pMapLevel->RequireLoadLayoutSnippet( "RadarImageSnippet" );
 					RegisterEventHandlerOnPanel( ImageLoaded(), pMapLevel->UIPanel(), this, &CCSGO_HudRadar::OnImageLoaded );
 					pMapLevel->SetImage( szImageURL );
 					pMapLevel->SetVisible( false );
 				}
			}
			else
			{
				DevWarning( "Radar vertical section is invalid!\n" );
			}
		}

		if ( m_vecRadarVerticalSections.Count() )
		{
			DevMsg( "Loaded radar vertical section keyvalues.\n" );
		}

		m_nCurrentRadarVerticalSection = 0;
	}
	else
	{
		// map doesn't have sections, clear any sections from previous map
		// TODO(comment from sfhuradar): Perhaps initialize a single giant vertical section that covers the whole map,
		//       to reduce variation in handling radar in downstream code?
		m_vecRadarVerticalSections.Purge();

		CUtlString szImageURL;
		szImageURL.Format( "file://{images_overviews}/%s_radar.dds", pMapName );

		panorama::CImagePanel *pMapLevel;
		pMapLevel = m_aMapLevels[ 0 ][ m_aMapLevels[ 0 ].AddToTail( new panorama::CImagePanel( m_aMapStyleTransform[ 0 ], "Radar__Square--Map" ) ) ];
		if ( pMapLevel )
		{
			pMapLevel->RequireLoadLayoutSnippet( "RadarImageSnippet" );
			RegisterEventHandlerOnPanel( ImageLoaded(), pMapLevel->UIPanel(), this, &CCSGO_HudRadar::OnImageLoaded );

			pMapLevel->SetImage( szImageURL );
			pMapLevel->SetVisible( false );
		}

		pMapLevel = m_aMapLevels[ 1 ][ m_aMapLevels[ 1 ].AddToTail( new panorama::CImagePanel( m_aMapStyleTransform[ 1 ], "Radar__Round--Map" ) ) ];
		if ( pMapLevel )
		{
			pMapLevel->RequireLoadLayoutSnippet( "RadarImageSnippet" );
			RegisterEventHandlerOnPanel( ImageLoaded(), pMapLevel->UIPanel(), this, &CCSGO_HudRadar::OnImageLoaded );

			pMapLevel->SetImage( szImageURL );
			pMapLevel->SetVisible( false );
		}

		// init layer
		m_nCurrentRadarVerticalSection = 0;
	}

	if ( m_aMapLevels[ 0 ][ 0 ]->GetImage()->BIsLoaded() )
	{
		m_fMapSourceImageSize = m_aMapLevels[ 0 ][ 0 ]->GetImage()->GetWidth();
	}

	m_MapOrigin.x = MapKeyValues->GetInt( "pos_x", 0 );
	m_MapOrigin.y = MapKeyValues->GetInt( "pos_y", 0 );
	m_MapOrigin.z = 0;
	m_fWorldToPixelScale = 1.0f / MapKeyValues->GetFloat( "scale", 1.0f );

	SwitchRadarToRound( true, true );
	UpdateRadarScalingValues( m_fRadarSize, m_fRadarPanelSize );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCSGO_HudRadar::UpdateMapLayer( int layerIdx, bool bFade )
{
	static const panorama::CPanoramaSymbol k_symFadeIn( "Map__Level--Animate-FadeIn" );
	static const panorama::CPanoramaSymbol k_symFadeOut( "Map__Level--Animate-FadeOut" );


	if ( !( m_aMapLevels[ 0 ][ layerIdx ] && m_aMapLevels[ 1 ][ layerIdx ] ) )
		return false;

	if ( m_pMapImg[0] && m_pMapImg[1] && ( m_nCurrentRadarVerticalSection == layerIdx ) )
		return false;

	for ( int i = 0; i < 2; i++ )
	{
		// old level
		if ( m_pMapImg[i] )
		{
			if ( bFade )
			{
				m_pMapImg[i]->SetVisible( true );
				m_pMapImg[i]->TriggerClass( k_symFadeOut );
			}
			else
			{
				m_pMapImg[i]->SetVisible( false );  // or setopacity( 0.0f );
			}
			// CSGO-1676: Always remove 
			m_pMapImg[i]->SetHasClass( k_symFadeIn, false );
		}

		// new level
		m_pMapImg[i] = m_aMapLevels[i][layerIdx];
		if ( m_pMapImg[i] )
		{
			if ( bFade )
			{
				m_pMapImg[i]->SetVisible( true );
				m_pMapImg[i]->TriggerClass( k_symFadeIn );
			}
			else
			{
				m_pMapImg[i]->SetVisible( true );  // or setopacity( 1.0f );
			}

			// CSGO-1676: Always remove class used for animation, otherwise our opacity will stay at 0 if we faded out but snapped on. 
			m_pMapImg[i]->SetHasClass( k_symFadeOut, false );
		}
	}

	m_nCurrentRadarVerticalSection = layerIdx;

	SwitchRadarToRound( m_bRound, true );

	return true;
}

//-----------------------------------------------------------------------------
// reset functions
//-----------------------------------------------------------------------------
void CCSGO_HudRadar::ResetForNewMap(void)
{
	int i;

	for ( i = 0; i < MAX_BOMB_ZONES; i++ )
	{
		m_pBombZoneIcons[ i ]->SetVisible( false );
	}

	for ( i = 0; i < MAX_HOSTAGE_RESCUES; i++ )
	{
		m_pHostageZoneIcons[ i ]->SetVisible( false );
	}

	while ( m_iLastHostageIndex >= 0 )
	{
		RemoveHostage( m_iLastHostageIndex );
	}

	RemoveAllDecoys();
	RemoveAllDefusers();

	m_iNumGoalIcons = 0;
	m_bGotGoalIcons = false;

	for ( i = 0; i <= m_iLastPlayerIndex; i++)
	{
		ResetPlayer( i );
	}

	ResetRoundVariables();

	HideShowGameModePanels( true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRadar::ResetRoundVariables(bool bResetGlobalStates)
{
	SetVisibilityFlags( 0 );
	SetLocationText( NULL );

	m_RadarViewpointWorld = vec3_origin;
	m_RadarViewpointMap = vec3_origin;
	m_RadarRotation = 0;

	m_fBombSeenTime = TIMER_INIT;
	m_fBombAlpha = 0.0f;
	m_fDefuserSeenTime = TIMER_INIT;
	m_fDefuserAlpha = 0.0f;

	m_bShowBombHighlight = false;
	m_bShowAll = false;
	m_iObserverMode = OBS_MODE_NONE;

	//TODO - resolve warning ConVarRef mp_defuser_allocation("mp_defuser_allocation");
	m_bTrackDefusers = ( mp_defuser_allocation.GetInt() == DefuserAllocation::Random );

	if ( bResetGlobalStates )
	{
		m_nBombEntIndex = -1;
		m_nBombHolderUserId = -1;
		m_BombPosition = vec3_origin;
		m_DefuserPosition = vec3_origin;
		m_bBombIsSpotted = false;
		m_bBombPlanted = false;
		m_bBombDropped = false;
		m_bBombExploded = false;
		m_bBombDefused = false;
	}

	m_EntitySpotted.ClearAll();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRadar::ResetRadar( bool bResetGlobalStates )
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
	}

	if ( bResetGlobalStates )
	{
		RemoveAllDecoys();
		RemoveAllDefusers();
		RemoveAllHostages();
	}

	ResetRoundVariables( bResetGlobalStates );

	HideShowGameModePanels();

	Show( true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRadar::ResetRound(void)
{
	ResetRadar(true);
}

//-----------------------------------------------------------------------------
// player icon loading / creating
//-----------------------------------------------------------------------------
bool CCSGO_HudRadar::LazyUpdateIconArray( CCSGO_HudRadarIconPackage* pArray, int lastIndex )
{
	bool result = true;
	int i;
	CCSGO_HudRadarIconPackage* pwalk;

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
void CCSGO_HudRadar::LazyCreatePlayerIcons( void )
{
	if ( m_bGotPlayerIcons )
		return;

	// the following code looks odd, but it insures that all three of the LazyUpdate calls are made
	// even if m_bGotPlayerIcons becomes false early.

	m_bGotPlayerIcons = LazyUpdateIconArray( m_Players, m_iLastPlayerIndex );

	m_bGotPlayerIcons = LazyUpdateIconArray( m_Hostages, m_iLastHostageIndex ) && m_bGotPlayerIcons;

	m_bGotPlayerIcons = LazyUpdateIconArray( m_Decoys, m_iLastDecoyIndex ) && m_bGotPlayerIcons;

	m_bGotPlayerIcons = LazyUpdateIconArray( m_Defusers, m_iLastDefuserIndex ) && m_bGotPlayerIcons;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCSGO_HudRadar::LazyCreateIconPackage( CCSGO_HudRadarIconPackage* pIconPack )
{
	if ( pIconPack->m_bIsActive && !pIconPack->m_pIconPackage )
 	{
		pIconPack->Init( m_pRadar );

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
void CCSGO_HudRadar::InitIconPackage( CCSGO_HudRadarIconPackage* pPlayer, int iAbsoluteIndex, ICON_PACK_TYPE iconType )
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
void CCSGO_HudRadar::RemoveIconPackage( CCSGO_HudRadarIconPackage* pPackage )
{
 	if ( pPackage->m_bIsActive )
	{
		pPackage->NukeFromOrbit( this );
	}
}

//-----------------------------------------------------------------------------
// player and hostage initialization and creation
//-----------------------------------------------------------------------------
void CCSGO_HudRadar::UpdateAllPlayerNumbers(void)
{
	for (int i = 0; i <= MAX_PLAYERS; ++i)
	{
		CCSGO_HudRadarIconPackage* pPackage = GetRadarPlayer(i);
		if (pPackage)
			UpdatePlayerNumber(pPackage);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRadar::UpdatePlayerNumber( CCSGO_HudRadarIconPackage* pPackage )
{
	int nMaxPlayers = CCSGameRules::GetMaxPlayers();

	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();

	if ( !pLocalPlayer )
		return;

	bool bSpec = pLocalPlayer->IsSpectator() || engine->IsHLTV();

	// update the player number
	CCSGO_HudTeamCounter *pTeamCounter = GET_HUDELEMENT( CCSGO_HudTeamCounter );


 	if ( pPackage && pPackage->m_pLabels[ PI_PLAYER_NUMBER ] )
	{
		bool bShowNumber = false;

		if ( pTeamCounter )
		{
			int pidx = pTeamCounter->GetPlayerSlotIndex( pPackage->m_iEntityID );
			if ( pidx != -1 )
			{
				if ( bSpec )
				{
					pidx += 1;
					if ( pidx == 10 )
						pidx = 0;

					if ( nMaxPlayers <= 10 )
					{
						pPackage->m_pLabels[PI_PLAYER_NUMBER]->SetText( CFmtStr( "%d", pidx ), panorama::CLabel::k_ETextTypeUnlocalized );
						bShowNumber = true;
					}
					else
					{
						//pPackage->m_pLabels[ PI_PLAYER_NUMBER ]->SetText( "" );
						pPackage->m_pLabels[ PI_PLAYER_NUMBER ]->SetVisible( false );
						bShowNumber = false;
					}
				}
				else
				{
					if ( ( pPackage->m_fLastColorUpdate + 0.5f ) < gpGlobals->curtime )
					{
						int nOtherTeamNum = 0;
						if ( pPackage->m_iPlayerType == PI_CT )
							nOtherTeamNum = TEAM_CT;
						else if ( pPackage->m_iPlayerType == PI_T )
							nOtherTeamNum = TEAM_TERRORIST;

						int nColorID = -1;
						C_CS_PlayerResource* pCSPR = (C_CS_PlayerResource*)g_PR;
						if ( pLocalPlayer->ShouldShowTeamPlayerColors( nOtherTeamNum ) )
						{
							if ( pCSPR )
								nColorID = pCSPR->GetCompTeammateColor( pPackage->m_iEntityID );

							m_bShowColorLetter = pLocalPlayer->ShouldShowTeamPlayerColorLetters();
						}
						
						if ( nColorID != -1 )
						{
							if ( pPackage->m_iPlayerType == PI_CT && pLocalPlayer->GetAssociatedTeamNumber() == TEAM_CT )
							{
								// 							ScaleformDisplayInfo displayInfo;
								// 							displayInfo.SetAlpha(100.0f);
								// 							displayInfo.SetVisibility(1);
								// 							m_pScaleformUI->Value_SetDisplayInfo(pPackage->m_IconPackage, &displayInfo);

								Color color;
								if ( pCSPR )
								{
									color = pCSPR->GetCompPlayerColorByID( nColorID );
								}

								panorama::IUIPanelStyle *pPanelStyle;
								pPanelStyle = pPackage->m_pIcons[PI_CT]->AccessStyle();
								pPanelStyle->SetSimpleWashColor( color, true );
								pPanelStyle = pPackage->m_pIcons[PI_CT_OFFMAP]->AccessStyle(); 
								pPanelStyle->SetSimpleWashColor( color, true );
								pPanelStyle = pPackage->m_pIcons[PI_CT_DEAD]->AccessStyle();
								pPanelStyle->SetSimpleWashColor( color, true );
							}
							else if ( pPackage->m_iPlayerType == PI_T && pLocalPlayer->GetAssociatedTeamNumber() == TEAM_TERRORIST )
							{
								if ( pPackage->m_bIsDead == false && pPackage->m_Health > 0 )
								{
									//pPackage->m_pIconPackageRotate->SetOpacity( 1.0f );
									pPackage->m_pIconPackageRotate->SetVisible( true );

									Color color;
									if ( pCSPR )
									{
										color = pCSPR->GetCompPlayerColorByID( nColorID );
									}

									panorama::IUIPanelStyle *pPanelStyle;
									pPanelStyle = pPackage->m_pIcons[PI_T]->AccessStyle();
									pPanelStyle->SetSimpleWashColor( color, true );
									pPanelStyle = pPackage->m_pIcons[PI_T_OFFMAP]->AccessStyle();
									pPanelStyle->SetSimpleWashColor( color, true );
									pPanelStyle = pPackage->m_pIcons[PI_T_DEAD]->AccessStyle();
									pPanelStyle->SetSimpleWashColor( color, true );
								}
							}
						}

						if ( m_bShowColorLetter )
						{
							pPackage->m_pLabels[PI_PLAYER_LETTER]->SetVisible( true );
							pPackage->m_pLabels[PI_PLAYER_LETTER]->SetText( pTeamCounter->GetPlayerColorLetter( 0, nColorID ), panorama::CLabel::k_ETextTypeUnlocalized );
						}
						else
						{
							pPackage->m_pLabels[ PI_PLAYER_LETTER ]->SetVisible( false );
						}

						// decide if the target shows the symbol
						// make sure they're no the other team
						if ( pPackage->m_iPlayerType == PI_ENEMY && Helper_ShouldShowAssassinationTargetIcon_Panorama( pPackage->m_iEntityID ) )
						{
							// unique number for targets for missions
							int nSpecialTargetID = 10; // => skull, TODO - sanity check this works...

							m_bShowColorLetter = true;

							pPackage->m_pLabels[PI_PLAYER_LETTER]->SetVisible( true );
							pPackage->m_pLabels[PI_PLAYER_LETTER]->SetText( pTeamCounter->GetPlayerColorLetter( 0, nSpecialTargetID ), panorama::CLabel::k_ETextTypeUnlocalized );
						}

						pPackage->m_fLastColorUpdate = gpGlobals->curtime;
					}
				}
			}
		}

		pPackage->m_pIconPackageNonRotate->SetVisible( bShowNumber || m_bShowColorLetter );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCSGO_HudRadar::CCSGO_HudRadarIconPackage* CCSGO_HudRadar::CreatePlayer( int index )
{
	CCSGO_HudRadarIconPackage* pPackage = GetRadarPlayer( index );
	InitIconPackage( pPackage, index, ICON_PACK_PLAYER );

	m_iLastPlayerIndex = MAX( index, m_iLastPlayerIndex );

	return pPackage;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRadar::ResetPlayer( int index )
{
	CCSGO_HudRadarIconPackage* pPackage = GetRadarPlayer( index );
	if ( pPackage->m_bIsActive )
	{
		pPackage->StartRound();

		UpdatePlayerNumber( pPackage );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRadar::RemovePlayer( int index )
{
	RemoveIconPackage( GetRadarPlayer( index ) );

	if ( index == m_iLastPlayerIndex )
	{
		while ( m_iLastPlayerIndex >= 0 && !m_Players[ m_iLastPlayerIndex ].m_bIsActive )
		{
			m_iLastPlayerIndex--;
		}
	}

	UpdateAllPlayerNumbers();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCSGO_HudRadar::CCSGO_HudRadarIconPackage* CCSGO_HudRadar::CreateHostage( int index )
{
	CCSGO_HudRadarIconPackage* pPackage = GetRadarHostage( index );
	InitIconPackage( pPackage, index, ICON_PACK_HOSTAGE );

	m_iLastHostageIndex = MAX( index, m_iLastHostageIndex );

	return pPackage;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRadar::ResetHostage( int index )
{
	CCSGO_HudRadarIconPackage* pPackage = GetRadarHostage( index );
	if ( pPackage->m_bIsActive )
	{
		pPackage->StartRound();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRadar::RemoveAllHostages( void )
{
	for ( int index = 0; index < MAX_HOSTAGES; index++ )
	{
		RemoveHostage( index );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRadar::RemoveStaleHostages( void )
{
	// Remove hostages that are no longer tracked by the player resource (hostages that have died)

	C_CS_PlayerResource *pCSPR = (C_CS_PlayerResource*)GameResources();
	if ( !pCSPR )
	{
		return;
	}

	for ( int index = 0; index < MAX_HOSTAGES; index++ )
	{
		CCSGO_HudRadarIconPackage* pPackage = GetRadarHostage( index );
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
void CCSGO_HudRadar::RemoveHostage( int index )
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
CCSGO_HudRadar::CCSGO_HudRadarIconPackage* CCSGO_HudRadar::CreateDecoy( int entityID )
{
	CCSGO_HudRadarIconPackage* pPackage = NULL;
	int index = m_iLastDecoyIndex + 1;

	for ( int i = 0; i <= m_iLastDecoyIndex; i++ )
	{
		if ( !m_Decoys[ i ].m_bIsActive )
		{
			index = i;
			break;
		}
	}

	if ( index < MAX_DECOYS )
	{
		pPackage = GetRadarDecoy( index );

		InitIconPackage( pPackage, index, ICON_PACK_DECOY );

		pPackage->m_iEntityID = entityID;
		pPackage->m_fRoundStartTime = TIMER_INIT;

		m_iLastDecoyIndex = MAX( index, m_iLastDecoyIndex );
	}

	return pPackage;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRadar::RemoveAllDecoys( void )
{
	for ( int i = 0; i <= m_iLastDecoyIndex; i++ )
	{
		if ( m_Decoys[ i ].m_bIsActive )
		{
			RemoveDecoy( i );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRadar::RemoveDecoy( int index )
{
	RemoveIconPackage( GetRadarDecoy( index ) );

	if ( index == m_iLastDecoyIndex )
	{
		while ( m_iLastDecoyIndex >= 0 && !m_Decoys[ m_iLastDecoyIndex ].m_bIsActive )
		{
			m_iLastDecoyIndex--;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCSGO_HudRadar::CCSGO_HudRadarIconPackage * CCSGO_HudRadar::GetDefuser( int nEntityID, bool bCreateIfNotFound )
{
	CCSGO_HudRadarIconPackage * pResult = NULL;

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
void CCSGO_HudRadar::SetDefuserPos( int nEntityID, int x, int y, int z, int a )
{
	if ( !m_bTrackDefusers )
	{
		return;
	}

	CCSGO_HudRadarIconPackage * pDefuser = GetDefuser( nEntityID, true );

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
CCSGO_HudRadar::CCSGO_HudRadarIconPackage* CCSGO_HudRadar::CreateDefuser( int nEntityID )
{
	if ( !m_bTrackDefusers )
	{
		return NULL;
	}

	CCSGO_HudRadarIconPackage* pPackage = NULL;
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
void CCSGO_HudRadar::RemoveAllDefusers( void )
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
void CCSGO_HudRadar::RemoveDefuser( int index )
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
void CCSGO_HudRadar::SetPlayerTeam( int index, int team )
{
	CCSGO_HudRadarIconPackage* pPlayer = GetRadarPlayer( index );
	pPlayer->SetPlayerTeam( team );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CCSGO_HudRadar::GetPlayerIndexFromUserID( int userID )
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
int CCSGO_HudRadar::GetHostageIndexFromHostageEntityID( int entityID )
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
int CCSGO_HudRadar::GetDecoyIndexFromEntityID( int entityID )
{
	for ( int i = 0; i <= m_iLastDecoyIndex; i++ )
	{
		if ( m_Decoys[ i ].m_bIsActive && m_Decoys[ i ].m_iEntityID == entityID )
		{
			return i;
		}
	}

	return INVALID_INDEX;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CCSGO_HudRadar::GetDefuseIndexFromEntityID( int nEntityID )
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

CCSGO_HudRadar::CCSGO_HudRadarIconPackage* CCSGO_HudRadar::GetRadarPlayer( int index )
{
	return &m_Players[ index ];
}

CCSGO_HudRadar::CCSGO_HudRadarIconPackage* CCSGO_HudRadar::GetRadarHostage( int index )
{
	return &m_Hostages[ index ];
}

CCSGO_HudRadar::CCSGO_HudRadarIconPackage* CCSGO_HudRadar::GetRadarDecoy( int index )
{
	return &m_Decoys[ index ];
}

CCSGO_HudRadar::CCSGO_HudRadarIconPackage* CCSGO_HudRadar::GetRadarDefuser( int index )
{
	return &m_Defusers[ index ];
}

//-----------------------------------------------------------------------------
// We don't get updates from the server from players
// that are not in our PVS, so there are separate
// messages specifically to update the radar
//-----------------------------------------------------------------------------
bool CCSGO_HudRadar::MsgFunc_ProcessSpottedEntityUpdate( const CCSUsrMsg_ProcessSpottedEntityUpdate &msg )
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

					  // non-class specific data
					  // read position and angle
		int x = update.origin_x() * 4;
		int y = update.origin_y() * 4;
		int z = update.origin_z() * 4;
		int a = update.angle_y();

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

		// Clients are unaware of the defuser class type, so we need to flag defuse entities manually
		if ( update.defuser() )
		{
			SetDefuserPos( nEntityID, x, y, z, a );
		}
		else if ( V_strcmp( "CCSPlayer", szEntityClass ) == 0 )
		{
			CCSGO_HudRadarIconPackage* pPlayerIcon = NULL;

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
				CCSGO_HudRadarIconPackage* pPackage = GetRadarHostage( hostageIndex );

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


//--------------------------------------------------------------------------------------------------
// handles all game event messages ( first looks them up in map so there's no long list of strcmps )
//--------------------------------------------------------------------------------------------------
void CCSGO_HudRadar::FireGameEvent( IGameEvent *event )
{
	const char* eventName = event->GetName();
	int elementIndex = m_messageMap.Find(eventName);
	if (elementIndex == m_messageMap.InvalidIndex())
	{
		return;
	}

	DESIRED_MESSAGE_INDICES messageTypeIndex = (DESIRED_MESSAGE_INDICES)m_messageMap.Element(elementIndex);

	switch (messageTypeIndex)
	{
	case GAME_NEWMAP:
		ResetForNewMap();
		SetMap(event->GetString("mapname"));
		break;

	case ROUND_POST_START:
	{
		ResetRound();
		UpdateAllPlayerNumbers();
	}
		break;

	case PLAYER_CONNECT:
	case PLAYER_INFO:
	{
		int index = event->GetInt("index"); // = entity index - 1
		int userID = index + 1;

		if  (index < 0 || index >= MAX_PLAYERS )
		{
			return;
		}

		CCSGO_HudRadarIconPackage* pPackage = CreatePlayer(index);
		if ( messageTypeIndex == PLAYER_CONNECT )
		{
			pPackage->SetPlayerTeam(TEAM_SPECTATOR);
		}

		pPackage->m_iEntityID = userID;

		const char* name = event->GetString("name", "unknown");

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

		CCSGO_HudRadarIconPackage* pPackage = GetRadarPlayer(playerIndex);
		pPackage->SetPlayerTeam( event->GetInt("team") );
		UpdatePlayerNumber(pPackage);

//		C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
//		if ( pLocalPlayer )
//		{
//			int localID = pLocalPlayer->GetUserID();
// TODO - remove?
// 			if ( localID == playerIndex )
// 			{
// 				ResizeHud();
// 			}
//		}
	}
		break;

	case PLAYER_DEATH:
	{
		int playerIndex = GetPlayerIndexFromUserID( event->GetInt("userid") );

		if ( playerIndex == INVALID_INDEX )
		{
			return;
		}

		CCSGO_HudRadarIconPackage* pPackage = GetRadarPlayer(playerIndex);

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

//		C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
//		if ( pLocalPlayer )
//		{
//			int localID = pLocalPlayer->GetUserID();
// TODO - remove?
// 			if ( localID == playerIndex )
// 			{
// 				ResizeHud();
// 			}
//		}
	}
		break;

	case PLAYER_SPAWN:
	{
		int playerIndex = GetPlayerIndexFromUserID( event->GetInt("userid") );

		if ( playerIndex != INVALID_INDEX )
		{
			CCSGO_HudRadarIconPackage* pPackage = GetRadarPlayer( playerIndex );
			pPackage->m_Health = 100;

			UpdatePlayerNumber( pPackage );
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

		CCSGO_HudRadarIconPackage* pPackage = GetRadarHostage( hostageIndex );

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

		CCSGO_HudRadarIconPackage* pPackage = GetRadarHostage( hostageIndex );

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
		m_nBombHolderUserId = -1;
	}
		break;

	case BOMB_PICKUP:
	{
		m_bBombDropped = false;
		m_nBombEntIndex = -1;
		m_nBombHolderUserId = event->GetInt( "userid", -1 );
	}
		break;

	case BOMB_DROPPED:
	{
		m_nBombEntIndex = event->GetInt( "entindex", -1 );
		m_bBombDropped = true;
		m_nBombHolderUserId = -1;
	}
		break;

	case DEFUSER_PICKUP:
	{
		int nDefuseIndex = GetDefuseIndexFromEntityID( event->GetInt("entityid") );

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
		C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
		if ( pLocalPlayer )
		{
			int userID = event->GetInt( "userid" );
			CBasePlayer* player = UTIL_PlayerByUserId( userID );

			if ( player )
			{
				// the radar only shows friendly decoys
				if ( !pLocalPlayer->IsOtherEnemy( player->entindex() ) )
				{
					int entityID = event->GetInt( "entityid" );
					int teamNumber = player->GetAssociatedTeamNumber();

					CCSGO_HudRadarIconPackage* pPackage = CreateDecoy( entityID );

					if (pPackage)
					{
						pPackage->SetPlayerTeam( teamNumber );
						pPackage->SetIsSpotted( true );
						// this sets the exact moment when the grenade should no longer be visible incase we miss the event
						pPackage->SetGrenadeExpireTime( gpGlobals->curtime + 11.0f );

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
		int entityID = event->GetInt("entityid");
		int i = GetDecoyIndexFromEntityID(entityID);
		if (i != INVALID_INDEX)
		{
			CCSGO_HudRadarIconPackage* pPackage = GetRadarDecoy(i);
			//pPackage->SetIsSpotted(false);
			SetIconPackagePosition(pPackage);
		}

		STEAMWORKS_TESTSECRET_AMORTIZE(31);
	}
		break;

	case BOT_TAKEOVER:
	{
		C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
		if (pLocalPlayer)
		{
			int userID = event->GetInt("userid");
			//int playerIndex = GetPlayerIndexFromUserID(event->GetInt("userid"));
			int localID = pLocalPlayer->GetUserID();

			if (localID == userID)
			{
// TODO - remove or reset scales?
//				ResizeHud();
				ResetRadar(false);

				int playerIndex = GetPlayerIndexFromUserID(userID);
				if (playerIndex != INVALID_INDEX)
				{
					CCSGO_HudRadarIconPackage* pPackage = GetRadarPlayer(playerIndex);
					UpdatePlayerNumber(pPackage);
				}
			}
		}

	}
	break;

	default:
		break;
	}
}


//-----------------------------------------------------------------------------
// Re-parent pChild to visible or invisible avatar panels
// return true if changed parent, false otherwise
//-----------------------------------------------------------------------------
bool CCSGO_HudRadar::SetPanelInvisible( panorama::CPanel2D *pPanel )
{
	if ( !pPanel || !m_pRadarInvisible )
		return false;

	if ( pPanel->GetParent() != m_pRadarInvisible )
	{
		pPanel->SetParent( m_pRadarInvisible );
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Re-parent unwanted/invisible panels to the invisible parent panel,
// Re-parent wanted/visible panels to their appropriate parent
//
// bReset - reset all panels to initial parent
//-----------------------------------------------------------------------------
void CCSGO_HudRadar::HideShowGameModePanels( bool bReset )
{
	if ( CSGameRules()->IsHostageRescueMap() || bReset )
	{
		// 
		// Show hostage related panels (re-parent to appropriate parents)
		// 
		for ( int i = 0; i < MAX_HOSTAGE_RESCUES; i++ )
		{
			if ( m_pHostageZoneIcons[ i ] )
				m_pHostageZoneIcons[ i ]->SetParent( m_pRadar );
		}
	}
	else
	{
		//
		// hide hostage related panels
		//
		for ( int i = 0; i < MAX_HOSTAGE_RESCUES; i++ )
		{
			SetPanelInvisible( m_pHostageZoneIcons[ i ] );
		}
	}

	if ( CSGameRules()->IsBombDefuseMap() || bReset )
	{
		// 
		// Show bomb related panels (re-parent to appropriate parents)
		// 
		if ( m_pBombDefuserPackage )
		{
			// ensure bomd defuser package visible, but also 
			// that it's drawn after player icons.
			// Since player icons are generated from snippets and added as children after initial xml load/layout,
			// quick fix is to set parent to invisible panel and back to radar, ensuring it's the last child
			SetPanelInvisible( m_pBombDefuserPackage );
			m_pBombDefuserPackage->SetParent( m_pRadar );
		}

		for ( int i = 0; i < MAX_BOMB_ZONES; i++ )
		{
			if ( m_pBombZoneIcons[ i ] )
				m_pBombZoneIcons[ i ]->SetParent( m_pRadar );
		}
	}
	else
	{
		//
		// hide bomb related panels
		//
		SetPanelInvisible( m_pBombDefuserPackage );

		for ( int i = 0; i < MAX_BOMB_ZONES; i++ )
		{
			SetPanelInvisible( m_pBombZoneIcons[ i ] );
		}
	}
}

//-----------------------------------------------------------------------------
// these set lazy update the icons and text based
// on the values in the state variables
//-----------------------------------------------------------------------------
void CCSGO_HudRadar::SetupIconsFromStates( void )
{
	int newFlags = 0;

	if ( CSGameRules()->IsBombDefuseMap() )
	{
		if ( m_fBombAlpha > 0.0f )
		{
			C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();

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
				if ( pLocalPlayer )
				{
					C_BaseEntity *target = pLocalPlayer->GetObserverTarget();
					if ( target && target->IsPlayer() )
						pLocalOrObserver = ToCSPlayer( target );
					else
						pLocalOrObserver = pLocalPlayer;
				}

				if ( pLocalOrObserver )
				{
					if ( m_BombPosition.z > pLocalOrObserver->GetAbsOrigin().z + ABOVE_BELOW_HEIGHT )
						newFlags |= 1 << RI_BOMB_ICON_BOMB_ABOVE;
					else if ( m_BombPosition.z < pLocalOrObserver->GetAbsOrigin().z - ABOVE_BELOW_HEIGHT )
						newFlags |= 1 << RI_BOMB_ICON_BOMB_BELOW;
				}
			}

			Vector mapPosition;

			WorldToRadar( m_BombPosition, mapPosition );

			Vector newMapPosition = mapPosition;
			newMapPosition -= m_RadarViewpointMap;
			float dist = newMapPosition.LengthSqr();
			if ( dist >= m_fRadarRadiusSq && m_bRound )
			{
				newMapPosition *= sqrt( m_fRadarRadiusSq / dist );
				mapPosition = m_RadarViewpointMap;
				mapPosition += newMapPosition;
			}

			// TODO - sanity check
			//Assert( m_Icons[ RI_BOMB_ICON_PACKAGE ] );
			//m_pIcons[ RI_BOMB_ICON_PACKAGE ]->SetOpacity( m_fBombAlpha );
			Assert( m_pBombDefuserPackage );
			m_pBombDefuserPackage->SetOpacity( m_fBombAlpha );

			Vector hudPos;
			RadarToHud( mapPosition, hudPos );
			Helper_SetPanelTransform3D_Translate( m_pBombDefuserPackage, hudPos );

			CCSGO_HudTeamCounter *pTeamCounter = GET_HUDELEMENT( CCSGO_HudTeamCounter );
			if ( pLocalPlayer && pTeamCounter && m_pIcons[ RI_BOMB_ICON_PACKAGE ] )
			{
				int playerIndex = GetPlayerIndexFromUserID( m_nBombHolderUserId ) + 1;
				int pidx = pTeamCounter->GetPlayerSlotIndex( playerIndex );

				C_CS_PlayerResource *pCSPR = ( C_CS_PlayerResource* )GameResources();
				int nColorID = 4;
				if ( pLocalPlayer->GetAssociatedTeamNumber() == TEAM_CT )
				{
					// 5 is enemy
					nColorID = 5;
				}
				else if ( pLocalPlayer->ShouldShowTeamPlayerColors( pLocalPlayer->GetAssociatedTeamNumber() ) )
				{
					if ( m_bBombDropped == false && ( pidx > -1 ) && pLocalPlayer->GetAssociatedTeamNumber() == TEAM_TERRORIST )
					{
						if ( pCSPR )
							nColorID = pCSPR->GetCompTeammateColor( playerIndex );
					}
					else
						nColorID = -1;
				}

				Color color;
				if ( pCSPR )
					color = pCSPR->GetCompPlayerColorByID( nColorID );

				if ( m_bBombDefused )
					color = Color( 0, 200, 0, 255 );

				panorama::IUIPanelStyle *pPanelStyle;
				pPanelStyle = m_pIcons[RI_BOMB_ICON_PACKAGE]->AccessStyle();
				pPanelStyle->SetSimpleWashColor( color, true );

				static const panorama::CPanoramaSymbol k_symPlantedBombAnimateRed( "PlantedBombAnimateRed" );
				if ( m_bBombPlanted && m_bBombDefused == false )
				{
					if ( m_pIcons[RI_BOMB_ICON_PACKAGE]->BHasClass( k_symPlantedBombAnimateRed ) == false )
						m_pIcons[RI_BOMB_ICON_PACKAGE]->AddClass( k_symPlantedBombAnimateRed );
				}
				else
				{
					if ( m_pIcons[RI_BOMB_ICON_PACKAGE]->BHasClass( k_symPlantedBombAnimateRed ) == true )
						m_pIcons[RI_BOMB_ICON_PACKAGE]->RemoveClass( k_symPlantedBombAnimateRed );
				}			
			}
		}

		static const panorama::CPanoramaSymbol k_symRadarBombPlantPulse( "RadarBombPlantPulse" );
		if ( m_bBombPlanted && m_bBombDefused == false )
		{
			m_aMapBorder[1]->SetVisible(true);

			if ( m_aMapBorder[1]->BHasClass( k_symRadarBombPlantPulse ) == false )
				m_aMapBorder[1]->AddClass( k_symRadarBombPlantPulse );
		}
		else
		{
			if ( m_aMapBorder[1]->BHasClass( k_symRadarBombPlantPulse ) == true )
				m_aMapBorder[1]->RemoveClass( k_symRadarBombPlantPulse );
		}
	}

	SetVisibilityFlags( newFlags );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRadar::SetVisibilityFlags( int newFlags )
{
	newFlags &= ( ( 1 << RI_NUM_ICONS ) - 1 );

	int diffFlags = m_iCurrentVisibilityFlags ^ newFlags;

	if ( diffFlags )
	{
		//TODO - remove?
		//m_pIconsParent->SetVisible( newFlags ? true : false );

		bool bWantVisible = ( newFlags != 0 );
		if ( ( m_iCurrentVisibilityFlags != 0 ) != bWantVisible )
		{
			m_pBombDefuserPackage->SetVisible( bWantVisible );
		}

		for ( int i = 0; i < RI_NUM_ICONS && ( diffFlags != 0 ); i++, diffFlags >>= 1 )
		{
			if ( diffFlags & 1 )
			{
				if ( m_pIcons[ i ] )
					m_pIcons[ i ]->SetVisible( ( newFlags & ( 1 << i ) ) != 0 );
			}
		}

		m_iCurrentVisibilityFlags = newFlags;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRadar::SetLocationText( const char *szNewText )
{
	if ( szNewText == NULL )
	{
		szNewText = "";
	}

	// Skip 1st character which will be # if present
	char* locStart = m_szLocationString[0] ? m_szLocationString + 1 : m_szLocationString;

	if ( V_strcmp( szNewText, locStart ) )
	{
		// Add '#' to the beginning of the text if it's not empty
		// to show this is a localization token.
		if ( *szNewText )
		{
			m_szLocationString[0] = '#';
			locStart = m_szLocationString + 1;
		}
		else
			locStart = m_szLocationString;

		V_strcpy( locStart, szNewText );

		if ( m_pLocationText )
		{
			m_pLocationText->SetText( m_szLocationString );
		}
	}
}


//-----------------------------------------------------------------------------
// "drawing" code.Actually just sets transforms
// of the panorama objects
//-----------------------------------------------------------------------------
void CCSGO_HudRadar::WorldToRadar(const Vector& ptin, Vector& ptout)
{
	float fWorldScale = m_fWorldToRadarScale;

	float flMapScaler = 0;
	if ( m_bRound && CSGameRules()->IsPlayingCoopMission() && cl_radar_always_centered.GetBool() )
		flMapScaler = MAX( 0, ( 0.75f - fWorldScale ) ) * 2.5f;

	float fScale = m_bRound ? ( cl_radar_scale.GetFloat() + flMapScaler ) : ( m_fRadarSize / m_fMapSize );

	fWorldScale *= fScale;

	ptout.x = ( ptin.x - m_MapOrigin.x ) * fWorldScale;
	ptout.y = ( m_MapOrigin.y - ptin.y ) * fWorldScale;
	ptout.z = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRadar::RadarToHud( const Vector& ptin, Vector& ptout )
{
	VMatrix rot;
	MatrixBuildRotateZ( rot, m_RadarRotation );
	Vector3DMultiply( rot, ptin - m_RadarViewpointMap, ptout );

	ptout.x += m_fHudPosRadarPanelCenterOffset;
	ptout.y += m_fHudPosRadarPanelCenterOffset;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRadar::PositionRadarViewpoint( void )
{
	if ( m_nCurrentRadarVerticalSection == -1 )
		return;

	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

	CBasePlayer * pPlayer = CBasePlayer::GetLocalPlayer();

	if ( pPlayer == NULL )
	{
		pPlayer = GetSplitScreenViewPlayer( nSlot );
	}

	bool bRotate;
	float fScale;
	Vector vRadarOffset;
	float fFrustrumRotation;

	Vector vMapCenter = Vector( m_fMapSize / 2, m_fMapSize / 2, 0 );
	CUtlVector<CTransform3D *> vecTransforms;

	if ( m_bRound )
	{
		// [jbright] Using GetLocalOrigin addresses an issue which
		// causes the center player indicator to not stay centered
		// on the radar. This solution causes the radar to go jump
		// around while in observer mode, so in those cases rely on
		// MainViewOrigin.
		if ( pPlayer && m_iObserverMode == OBS_MODE_NONE )
		{
			m_RadarViewpointWorld = pPlayer->GetLocalOrigin();
		}
		else
		{
			m_RadarViewpointWorld = MainViewOrigin( nSlot );
		}

		m_RadarViewpointWorld.z = 0;

		WorldToRadar( m_RadarViewpointWorld, m_RadarViewpointMap );

		float flMapScaler = 0;
		if ( m_bRound && CSGameRules()->IsPlayingCoopMission() && cl_radar_always_centered.GetBool() )
			flMapScaler = MAX( 0, ( 0.75f - m_fWorldToRadarScale ) ) * 2.5f;

		fScale = cl_radar_scale.GetFloat() + flMapScaler;

		if ( !cl_radar_always_centered.GetBool() )
		{
			vRadarOffset = ( 1.0f - fScale ) * ( m_RadarViewpointMap - fScale * ( vMapCenter ) );
			m_RadarViewpointMap -= vRadarOffset;
		}

		bRotate = cl_radar_rotate.GetBool();
	}
	else
	{
		bRotate = false;

		fScale = m_fRadarSize / m_fMapSize;

		m_RadarViewpointMap = fScale * vMapCenter;

		vRadarOffset = Vector( 0, 0, 0 );
	}

	// scale
	vecTransforms.AddToTail( new CTransformScale3D( fScale * ( m_fMapSize / m_fRadarSize ), fScale * ( m_fMapSize / m_fRadarSize ), 1.0f ) );

	// translation
	vecTransforms.AddToTail( new CTransformTranslate3D(
		(vMapCenter.x * fScale) - m_RadarViewpointMap.x,
		(vMapCenter.y * fScale) - m_RadarViewpointMap.y,
		0.0f ) );


	if ( bRotate )
	{
		m_RadarRotation = MainViewAngles( nSlot )[ YAW ] - 90;
		fFrustrumRotation = 0;

		// rotation
		vecTransforms.AddToTail( new CTransformRotate3D( 0, m_RadarRotation, 0.0f ) );
	}
	else
	{
		m_RadarRotation = 0;
		fFrustrumRotation = MainViewAngles( nSlot )[ YAW ] - 90;
	}

	// apply transforms to correct parent panel
 	if ( m_aMapStyleTransform[ m_bRound ] )
 	{
 		m_aMapStyleTransform[ m_bRound ]->SetTransform3DSimple( vecTransforms );
 	}

	if ( m_pDirectionArrow )
	{
		CUtlVector<CTransform3D *> vecTransformArrow;
		vecTransformArrow.AddToTail( new CTransformRotate3D( 0, m_RadarRotation, 0.0f ) );
		m_pDirectionArrow->SetTransform3DSimple( vecTransformArrow );
	}

	if ( m_vecRadarVerticalSections.Count() )
 	{
 		float flPlayerZ = 0;
 		if ( pPlayer && m_iObserverMode == OBS_MODE_NONE )
 		{
 			flPlayerZ = pPlayer->GetLocalOrigin().z;
 		}
 		else
 		{
 			flPlayerZ = MainViewOrigin( nSlot ).z;
 		}
 
		FOR_EACH_VEC( m_vecRadarVerticalSections, i )
		{
			HudRadarLevelVerticalSection_t *pRadarSection = &m_vecRadarVerticalSections[ i ];

			if ( flPlayerZ >= pRadarSection->m_flSectionAltitudeFloor &&
				 flPlayerZ < pRadarSection->m_flSectionAltitudeCeiling )/* &&
				 m_nCurrentRadarVerticalSection != pRadarSection->m_nSectionIndex )*/
			{
				if ( UpdateMapLayer( pRadarSection->m_nSectionIndex, true ) )
					break;
			}
		}
 	}
	else
	{
		UpdateMapLayer( 0 );
	}

	// radar specific scale (works in tandem with hud_scaling, which scales all main hud components)
	Vector vUIScale = Vector( cl_hud_radar_scale.GetFloat(), cl_hud_radar_scale.GetFloat(), 1.0f );

	panorama::IUIPanelStyle *pPanelStyle = m_pRadar->AccessStyle();
	pPanelStyle->SetUIScale( vUIScale );

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRadar::PlaceGoalIcons( void )
{
	C_CS_PlayerResource *pCSPR = (C_CS_PlayerResource*)GameResources();
	if ( !pCSPR )
	{
		return;
	}

	CCSGO_HudRadarGoalIcon* pwalk = m_GoalIcons;
	Vector mapPosition;

// TODO - remove now that we've killed the other transform path
//	bool bRotate = m_bRound ? cl_radar_rotate.GetBool() : false;

	static const panorama::CPanoramaSymbol k_symOnMap( "BombZone_OnMap" );

	for ( int i = 0; i < m_iNumGoalIcons; i++ )
	{
		WorldToRadar( pwalk->m_Position, mapPosition );

		if ( pwalk->m_pIcon )
		{
			Vector newMapPosition = mapPosition;
			newMapPosition -= m_RadarViewpointMap;
			float dist = newMapPosition.LengthSqr();
			if ( dist > m_fRadarRadiusSq && m_bRound )
			{
				newMapPosition *= sqrt( m_fRadarRadiusSq / dist );
				mapPosition = m_RadarViewpointMap;
				mapPosition += newMapPosition;

				if ( pwalk->m_pIcon->BHasClass(k_symOnMap) == true )
					pwalk->m_pIcon->RemoveClass(k_symOnMap);
			}
			else
			{
				// if the bomb icon is on the map, add this class to make it smaller
				if ( pwalk->m_pIcon->BHasClass( k_symOnMap ) == false )
					pwalk->m_pIcon->AddClass( k_symOnMap );
			}

			Vector hudPos;
			RadarToHud( mapPosition, hudPos );
			Helper_SetPanelTransform3D_Translate2( pwalk->m_pIcon, hudPos );
		}
		pwalk++;
	}

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRadar::SetIconPackagePosition( CCSGO_HudRadarIconPackage* pPackage )
{
	Vector vO = vec3_origin;
	Vector mapPosition;
	float mapAngle;

	WorldToRadar( pPackage->m_Position, mapPosition );

	Vector newMapPosition = mapPosition;
	newMapPosition -= m_RadarViewpointMap;

	float dist = newMapPosition.LengthSqr();
	if ( dist >= m_fRadarRadiusSq && m_bRound )
	{
		pPackage->SetIsOffMap( true );
		newMapPosition *= sqrt( m_fRadarRadiusSq / dist );
		mapPosition = m_RadarViewpointMap;
		mapPosition += newMapPosition;
		mapAngle = 180.0f*atan2( newMapPosition.y, newMapPosition.x ) / 3.141592f + 90;
	}
	else
	{
		pPackage->SetIsOffMap( false );
		if ( pPackage->m_bIsDead || !pPackage->m_bIsSpotted || ( pPackage->m_nAboveOrBelow != R_SAMELEVEL ) || ( pPackage->m_iPlayerType == PI_ENEMY && !pPackage->m_bIsSelected ) )
		{
			mapAngle = -m_RadarRotation;
		}
		else
		{
			mapAngle = -pPackage->m_Angle[ YAW ] + 90;
		}

	}

	// decoys are a bit smaller
	float scale = 1.0f;
	if ( pPackage->IsDecoyType() )
	{
		scale = 0.75f;

		Color color = Color( 200, 200, 200, 255 );
		panorama::IUIPanelStyle *pPanelStyle;
		pPanelStyle = pPackage->m_pIcons[PI_T]->AccessStyle();
		pPanelStyle->SetSimpleWashColor( color, true );
		pPanelStyle = pPackage->m_pIcons[PI_T_OFFMAP]->AccessStyle();
		pPanelStyle->SetSimpleWashColor( color, true );
		pPanelStyle = pPackage->m_pIcons[PI_CT]->AccessStyle();
		pPanelStyle->SetSimpleWashColor( color, true );
		pPanelStyle = pPackage->m_pIcons[PI_CT_OFFMAP]->AccessStyle();
		pPanelStyle->SetSimpleWashColor( color, true );
	}

	float fIconScale = cl_radar_scale.GetFloat();

	fIconScale = RemapValClamped( fIconScale,
								  0, 1,
								  cl_radar_icon_scale_min.GetFloat(), 1.25f );

	scale *= fIconScale;

	static bool s_bResetTransform = false;
	RadarToHud( mapPosition, pPackage->m_HudPosition );
	if ( pPackage->m_pIconPackageRotate )
	{
		bool bRotate = m_bRound ? cl_radar_rotate.GetBool() : false;
		pPackage->m_HudRotation = bRotate ? ( m_RadarRotation + mapAngle ) : mapAngle;
	}
	else
	{
		pPackage->m_HudRotation = 0.0f;
	}
	pPackage->m_HudScale = scale;

	if ( s_bResetTransform )
	{
		Helper_SetPanelTransform3D_Translate( pPackage->m_pIconPackage, vO );

		if ( pPackage->m_pIconPackageRotate )
		{
			Helper_SetPanelTransform3D_Rotate( pPackage->m_pIconPackageRotate, 0.0f );
		}

		s_bResetTransform = false;
	}

	pPackage->SetupIconsFromStates();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRadar::PlaceHostages( void )
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

	/* 	// Commented out because we want to show hostages on radar to Ts
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
	*/


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
			CCSGO_HudRadarIconPackage* pHostage = NULL;

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
					pHostage->SetIsDead( false );

					if ( pCSPR->IsHostageFollowingSomeone( i ) )
					{
						pHostage->SetIsMovingHostage( true );

						if ( !pHostage->m_bHostageIsUsed )
							pHostage->m_bHostageIsUsed = true;
					}
					else
					{
						pHostage->SetIsMovingHostage( false );
					}
				}
				else if ( i <= m_iLastHostageIndex && pHostage->m_bIsActive )
				{
					pHostage->SetIsDead( true );
					pHostage->SetIsMovingHostage( false );
				}
			}
		}
	}

	// Update hostage positions
	for ( int i = 0; i < MAX_HOSTAGES; i++ )
	{
		CCSGO_HudRadarIconPackage* pHostage = GetRadarHostage( i );

		if ( pHostage->m_bIsActive )
		{
			C_BaseEntity * pEntity = ClientEntityList().GetBaseEntity( pHostage->m_iEntityID );

			//C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();

			// note: !pHostage->m_bHostageIsUsed : T's can always see Hostages until they get +used by CTs
			bool bSpotted = ( !pHostage->m_bHostageIsUsed ||
							  pLocalPlayer->GetAssociatedTeamNumber() == TEAM_CT ||
							  ( pEntity && pEntity->IsSpotted() ) ||
							  m_EntitySpotted.Get( pHostage->m_iEntityID ) ||
							  m_bShowAll );

			pHostage->SetIsSpotted( bSpotted );

			int nIsAboveOrBelow = R_SAMELEVEL;
			C_CSPlayer *pLocalOrObserver = NULL;
			if ( pLocalPlayer )
			{
				C_BaseEntity *target = pLocalPlayer->GetObserverTarget();
				if ( target && target->IsPlayer() )
					pLocalOrObserver = ToCSPlayer( target );
				else
					pLocalOrObserver = pLocalPlayer;
			}

			if ( pLocalOrObserver )
			{
				if ( pHostage->m_Position.z > pLocalOrObserver->GetAbsOrigin().z + ABOVE_BELOW_HEIGHT )
					nIsAboveOrBelow = R_ABOVE;
				else if ( pHostage->m_Position.z < pLocalOrObserver->GetAbsOrigin().z - ABOVE_BELOW_HEIGHT )
					nIsAboveOrBelow = R_BELOW;
			}

			pHostage->SetIsAboveOrBelow( nIsAboveOrBelow );

			SetIconPackagePosition( pHostage );

			bool bRotate = m_bRound ? cl_radar_rotate.GetBool() : false;
			// hostage always faces the same way
			if ( !bRotate )
				pHostage->m_Angle = QAngle( 0, 90, 0 );
			else
			{
				int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
				pHostage->m_Angle = MainViewAngles( nSlot );
			}

			if ( pEntity && !pEntity->IsDormant() && bSpotted )
			{
				Assert( dynamic_cast<CHostage*>( pEntity ) );
				pHostage->m_Position = pEntity->GetNetworkOrigin();
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRadar::UpdateAllDefusers( void )
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
		CCSGO_HudRadarIconPackage * pPackage = GetRadarDefuser( i );

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
void CCSGO_HudRadar::PlacePlayers( void )
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

	CCSGO_HudRadarIconPackage* pwalk = m_Players;
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
				pwalk->SetIsBot( pCSPR->IsFakePlayer( playerEntityIndex ) );

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
					else // !playerIsActive
					{
						// Just assume that players we can see that aren't in our PVS were spotted by someone else.
						bSpottedByLocalPlayer = !bIsEnemy;
						if ( bVisibleOnRadar )
						{
							bSpottedByFriends = true;
						}
					}

					if ( bHasValidPosition && ( !bIsEnemy || m_bShowAll || bVisibleOnRadar || bObservingTarget ) )
					{
						pwalk->SetIsSpotted( true );
						pwalk->SetIsSpottedByFriendsOnly( ( bSpottedByFriends && !bSpottedByLocalPlayer ) );

						if ( playerIsActive && pPlayer->HasDefuser() )
						{
							// attach a defuser icon to this player if one does not already exist
							if ( GetDefuseIndexFromEntityID( playerEntityIndex ) == INVALID_INDEX )
							{
								CreateDefuser( playerEntityIndex );
							}
						}

						if ( playerIsActive && pPlayer->HasC4() )
						{
							m_bBombIsSpotted = true;
							m_BombPosition = pwalk->m_Position;
							m_nBombHolderUserId = pPlayer->GetUserID();
						}

						// set is selected
						C_CSPlayer *pLocalOrObserver = ToCSPlayer( pLocalPlayer->GetObserverTarget() );
						pwalk->SetIsSelected( pLocalOrObserver && pPlayer == pLocalOrObserver );
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

				UpdatePlayerNumber( pwalk );

				C_CSPlayer *pLocalOrObserver = NULL;
				if ( pLocalPlayer )
				{
					C_BaseEntity *target = pLocalPlayer->GetObserverTarget();
					if ( target && target->IsPlayer() )
						pLocalOrObserver = ToCSPlayer( target );
					else
						pLocalOrObserver = pLocalPlayer;
				}

				if ( pLocalOrObserver )
				{
					if ( pwalk->m_Position.z > pLocalOrObserver->GetAbsOrigin().z + ABOVE_BELOW_HEIGHT )
						nIsAboveOrBelow = R_ABOVE;
					else if ( pwalk->m_Position.z < pLocalOrObserver->GetAbsOrigin().z - ABOVE_BELOW_HEIGHT )
						nIsAboveOrBelow = R_BELOW;
				}

				pwalk->SetIsAboveOrBelow( nIsAboveOrBelow );

			}
			else
			{
				pwalk->SetIsSpotted( false );
				pwalk->SetIsSpeaking( false );
			}

			if ( playerIsActive && pwalk->m_bIsSpotted )
			{
				// when the local player dies the system reports the local players position
				// as the position of the spectator camera.  If we let that happen, the player's
				// X migrates from the point they died to follow the camera.  Can't have that,
				// so avoid setting the position if we're dead and in observer mode

				if ( m_iObserverMode == OBS_MODE_NONE || i != localPlayerIndex )
				{
					// dkorus and pfreese:  Changed from local origin and local angles to last networked version to address jitter issues for pax demo 2011
					if ( !pwalk->m_bIsDead )
						pwalk->m_Position = pPlayer->GetNetworkOrigin();

					// only set the angle of the icon if the player isn't above or below us
					if ( nIsAboveOrBelow != R_SAMELEVEL || pwalk->IsHostageType() || ( pwalk->m_iPlayerType == PI_ENEMY && !pwalk->m_bIsSelected ) )
					{
						int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
						pwalk->m_Angle = MainViewAngles( nSlot );
					}
					else
						pwalk->m_Angle = pPlayer->EyeAngles();

					if ( pPlayer->HasC4() )
					{
						m_BombPosition = pwalk->m_Position;
						m_nBombHolderUserId = pPlayer->GetUserID();
					}
				}
			}
			SetIconPackagePosition( pwalk );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRadar::UpdateMiscIcons( void )
{
	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pLocalPlayer )
	{
		return;
	}

	const char *pszLocation = pLocalPlayer->GetLastKnownPlaceName();

	if ( engine->IsHLTV() || pLocalPlayer->IsSpectator() )
	{
		pszLocation = NULL;
		if ( C_CSPlayer *pObserverTarget = dynamic_cast< C_CSPlayer * >( pLocalPlayer->GetObserverTarget() ) )
		{
			pLocalPlayer = pObserverTarget;
			pszLocation = pLocalPlayer->GetLastKnownPlaceName();
		}
	}

	if ( pszLocation )
	{
		SetLocationText( pszLocation );
	}
	else
	{
		SetLocationText( NULL );
	}

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
						m_nBombHolderUserId = -1;
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
				m_nBombHolderUserId = -1;
			}
		}

		float now = gpGlobals->curtime;
		bool drawBomb = false;

		bool bBombPositionIsValid = ( m_BombPosition.x != 0 || m_BombPosition.y != 0 );

		if ( CSGameRules() && CSGameRules()->IsPlayingCoopMission() )
		{
			drawBomb = false;
		}
		else if ( bBombPositionIsValid && !m_bBombExploded && m_bBombIsSpotted )
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
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRadar::ApplySpectatorModes( void )
{
	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();

	if ( !pLocalPlayer )
		return;

	m_bShowAll = false;
	m_iObserverMode = pLocalPlayer->GetObserverMode();

	if ( engine->IsHLTV() || pLocalPlayer->IsSpectator() )
	{
		m_bShowAll = true;
	}
	else if ( m_iObserverMode != OBS_MODE_NONE ) // is not a live player
	{
		// respect mp_forcecamera
		//static ConVarRef mp_forcecamera( "mp_forcecamera" );
		extern ConVar mp_forcecamera;
		m_bShowAll = ( mp_forcecamera.GetInt() == OBS_ALLOW_ALL );
	}

	// mp_radar_showall trumps all
	static ConVarRef mp_radar_showall( "mp_radar_showall" );
	if ( mp_radar_showall.GetInt() && (
		( pLocalPlayer->GetAssociatedTeamNumber() == mp_radar_showall.GetInt() ) ||
		( mp_radar_showall.GetInt() == 1 )
		) )
	{
		m_bShowAll = true;
	}

	// Radar type
	bool bRoundRadar = true;
	CCSGO_Scoreboard *pScoreboard = CCSGO_Scoreboard::GetInstance();

	if ( pScoreboard && ( engine->IsHLTV() || pLocalPlayer->IsSpectator() || ( pScoreboard->BIsVisible() && cl_radar_square_with_scoreboard.GetBool() ) ) )
		bRoundRadar = false;
	else if ( pLocalPlayer->IsAlive() == false &&
		( m_iObserverMode == OBS_MODE_FIXED ||
		  m_iObserverMode == OBS_MODE_CHASE ||
		  m_iObserverMode == OBS_MODE_ROAMING ||
		  m_iObserverMode == OBS_MODE_IN_EYE ) )
	{
		bRoundRadar = false;
	}
	SwitchRadarToRound( bRoundRadar );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRadar::UpdateDecoys( void )
{
	for ( int i = 0; i <= m_iLastDecoyIndex; i++ )
	{
		CCSGO_HudRadarIconPackage* pPackage = GetRadarDecoy( i );

		if ( pPackage && pPackage->m_bIsActive )
		{
			SetIconPackagePosition( pPackage );
			if ( !pPackage->IsVisible() )
			{
				RemoveDecoy( i );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRadar::ProcessInput()
{
	LazyCreateGoalIcons();

	LazyCreatePlayerIcons();

	PositionRadarViewpoint();

	ApplySpectatorModes();

	PlaceGoalIcons();

	PlacePlayers();

	PlaceHostages();

	UpdateDecoys();

	UpdateAllDefusers();

	UpdateMiscIcons();

	SetupIconsFromStates();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRadar::SwitchRadarToRound( bool toRound, bool bForce )
{
	if ( m_nCurrentRadarVerticalSection == -1 )
		return;

	//if ( !m_pMap )
	//	return;

	if ( !( m_aMapContainerPanel[ 0 ] && m_aMapContainerPanel[ 1 ] &&
			m_aMapBorder[ 0 ] && m_aMapBorder[ 1 ] &&
			m_pDirectionArrow ) )
		return;

  	if ( !bForce && toRound == m_bRound )
  		return;

	// turn off other map style image panel
	//m_pMap->SetVisible( false );

	if ( toRound )
	{
		m_bRound = true;

		//m_pMap = m_aMapLevels[ 1 ][ m_nCurrentRadarVerticalSection ];

		m_aMapBorder[ 0 ]->SetVisible( false );
		m_aMapBorder[ 1 ]->SetVisible( true );

		m_aMapContainerPanel[ 0 ]->SetVisible( false );
		m_aMapContainerPanel[ 1 ]->SetVisible( true );

		m_pDirectionArrow->SetVisible( true );
	}
	else
	{
		m_bRound = false;

		//m_pMap = m_aMapLevels[ 0 ][ m_nCurrentRadarVerticalSection ];

		m_aMapBorder[ 0 ]->SetVisible( true );
		m_aMapBorder[ 1 ]->SetVisible( false );

		m_aMapContainerPanel[ 0 ]->SetVisible( true );
		m_aMapContainerPanel[ 1 ]->SetVisible( false );

		m_pDirectionArrow->SetVisible( false );
	}

	// turn on map style image panel
	//m_pMap->SetVisible( true );

	// resize
	GetLayoutDefines();

	// extra update to sync up positions 
	PositionRadarViewpoint();
	PlaceGoalIcons();
	PlacePlayers();
	PlaceHostages();
	UpdateDecoys();
	UpdateAllDefusers();
	SetupIconsFromStates();
}

//-----------------------------------------------------------------------------
// From .as - revisit
//-----------------------------------------------------------------------------

/* TODO - layered/smooth transition radar??

function getLayerClipFromArray( layerArr : Array, layerIdx : Number ) : MovieClip
{
	if ( layerArr == null )
		return null;

	if ( layerArr.length <= layerIdx )
		return null;

	return MovieClip( layerArr[ layerIdx ] );
}

function getLayerClip( layerIdx : Number ) : MovieClip
{
	var layerArr : Array = m_bIsRound ? m_mapLayersRnd : m_mapLayersSq;
	
	return getLayerClipFromArray( layerArr, layerIdx );
}

function createLayeredRadar() : Object
{
	// How fast to fade between clips
	var ALPHA_PER_FRAME : Number = 0.08;
var ADOBE_MAX_ALPHA : Number = 100;  // For some reason Adobe decided that _alpha should go from 0 to 100 instead of 0 to 1

									 // The currently displayed radar clip.  Always non-null whenever any radar is visible
var m_currentRadar : MovieClip = null;

// The clip we are transitioning to.  Invariant:
// - null if no transition is happening
// - if non-null, then RadarModule.Radar.onEnterFrame points at our tweening function
var m_nextRadar : MovieClip = null;

function finishCurrentTransition()
{
	if ( m_nextRadar )
	{
		trace( "RADAR: Transition complete, clearing " + m_currentRadar._target );

		m_nextRadar._alpha = ADOBE_MAX_ALPHA;
		m_currentRadar._visible = false;
		m_currentRadar = m_nextRadar;
		m_nextRadar = null;
	}
}

function clearTweenUpdate()
{
	RadarModule.Radar.onEnterFrame = null;
}

function forceLayerTransitionToComplete()
{
	finishCurrentTransition();
	clearTweenUpdate();
}

function tweenUpdate()
{
	var nextAlpha = m_nextRadar._alpha + ( ALPHA_PER_FRAME * ADOBE_MAX_ALPHA );

	// Common case: animation still going, just update alpha and done.
	if ( nextAlpha < ADOBE_MAX_ALPHA )
	{
		m_nextRadar._alpha = nextAlpha;
		return;
	}

	// Remove the old 'current' radar.
	finishCurrentTransition();

	// Check for new transition
	var nextClip = getLayerClip( m_visibleLayer );

	if ( nextClip == null || nextClip == m_currentRadar )
	{
		// No new transition needed, stop ticking
		clearTweenUpdate();
		return;
	}

	// Start the new transition
	OnDesiredLayerChange();
}

function startTweenUpdate()
{
	RadarModule.Radar.onEnterFrame = tweenUpdate;
}

function OnDesiredLayerChange()
{
	var nextClip = getLayerClip( m_visibleLayer );

	if ( nextClip == null )
		return;

	if ( m_currentRadar == null )
	{
		trace( "RADAR: Pop to visible (from no radar):" + nextClip._target );

		// No current radar, try to pop it to visible
		m_currentRadar = nextClip;
		m_currentRadar._visible = true;
		m_currentRadar._alpha = ADOBE_MAX_ALPHA;

		return;
	}

	if ( m_nextRadar == null )
	{
		if ( m_currentRadar == nextClip )
		{
			// already at this clip, no need to transition
			return;
		}

		// No tranisition happening, start the new transition
		m_nextRadar = nextClip;

		trace( "RADAR: Start transition, maing visible " + m_nextRadar._target );

		// We have a new transition. Initialize it!
		m_nextRadar._visible = true;
		m_nextRadar._alpha = 0;

		// Make sure the new one draws on top
		if ( m_nextRadar.getDepth() < m_currentRadar.getDepth() )
		{
			m_nextRadar.swapDepths( m_currentRadar );
		}

		// Start the transition!
		startTweenUpdate();

		return;
	}

	// At this point we know we are already in a transition

	if ( m_nextRadar == nextClip )
	{
		// already transitioning to this clip
		return;
	}

	if ( m_currentRadar == nextClip )
	{
		trace( "RADAR: Flip transition direction" );

		// This is a weird, but common, case.  We are in the middle of a transition,
		// and we want to transition back to the previous state.  What we do here
		// is swap the two clips, and flip the alpha of the top to the inverse of the
		// old alpha of the bottom.  This should give the same results!
		m_currentRadar = m_nextRadar;
		m_nextRadar = nextClip;

		m_nextRadar.swapDepths( m_currentRadar );

		m_nextRadar._alpha = ADOBE_MAX_ALPHA - m_currentRadar._alpha;
		m_currentRadar._alpha = ADOBE_MAX_ALPHA;

		return;
	}

	// Otherwise we are in the middle of a transition between two other clips.
	// Allow that transition to complete, at which point we will automatically
	// start transitioning to the new target.
}


function OnClipLoaded( clip : MovieClip )
{
	trace( "RADAR: clip loaded " + clip._target );

	if ( clip == m_currentRadar || clip == m_nextRadar )
	{
		trace( "RADAR: currently active clip loaded, making visible: " + clip._target );
		clip._visible = true;
		return;
	}

	if ( m_currentRadar != null )
	{
		// Check if we want to transition to the new layer
		OnDesiredLayerChange();
		return;
	}

	// Otherwise m_currentRadar == null, and we might want to set it to this one
	var layerClip : MovieClip = getLayerClip( m_visibleLayer );

	if ( layerClip == clip )
	{
		trace( "RADAR: loaded clip is current layer, making visible: " + clip._target );
		clip._visible = true;
		m_currentRadar = clip;
	}
}

function PreSwitchRadarType()
{
	// We are about to switch from round to square radar or vice versa.
	// The invariant we want to maintain is that the current radar always
	// matches the current visible clip, and we can't guarantee that the
	// current clip won't change while we are looking at the other radar.
	// The safest thing to do is just clear it completely and make the
	// new radar start fully transitioned to the current layer.
	trace( "RADAR: PreSwitchType" );

	forceLayerTransitionToComplete();

	if ( m_currentRadar != null )
	{
		trace( "RADAR: preswitch, making invisible: " + m_currentRadar._target )
			m_currentRadar._visible = false;
	}

	m_currentRadar = null;
}

function PostSwitchRadarType()
{
	trace( "RADAR: PostSwitchType" );

	// I'd like to assert(m_currentRadar == null) here, but not sure how
	// to do that in actionscript.  We'll just handle that case directly.
	if ( m_currentRadar != null )
		PreSwitchRadarType(); // clear m_currentRadar

							  // This will pop in the new radar.
	OnDesiredLayerChange();
}

var layeredRadar : Object = new Object;
layeredRadar.preSwitchRadarType = function() { PreSwitchRadarType(); }
layeredRadar.postSwitchRadarType = function() { PostSwitchRadarType(); }
layeredRadar.onDesiredLayerChange = function() { OnDesiredLayerChange(); }
layeredRadar.onRadarClipLoaded = function( clip:MovieClip ) { OnClipLoaded( clip ); }

return layeredRadar;
}

*/

// TODO
/*
function getTrialTimerYPosition()
{
	return ( RadarModule.scaledHeight * m_nRadarScale ) + RadarModule._y + 15;
}

*/






