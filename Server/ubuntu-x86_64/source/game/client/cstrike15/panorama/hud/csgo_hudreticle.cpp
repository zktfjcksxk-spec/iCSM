//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//
// SF Differences:
// 
//=============================================================================//

#include "cbase.h"
#include "csgo_hudreticle.h"

#include "c_cs_player.h"
#include "c_cs_hostage.h"
#include "c_plantedc4.h"
#include "c_cs_playerresource.h"
#include "view.h"
#include "inputsystem/iinputsystem.h"
#include "hltvcamera.h"
#include "hltvreplaysystem.h"
#include "cs_hud_weaponselection.h"
#include "matchmaking/imatchframework.h"
#include "csgo_hudteamcounter.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


REGISTER_PANEL2D_FACTORY( CCSGO_HudReticle, CSGOHudReticle );

DECLARE_PANORAMA_EVENT1( CSGOHudShowTeamEquipment, bool );
DEFINE_PANORAMA_EVENT_DOC( CSGOHudShowTeamEquipment, "bool", "Show/hide equipment for the current team" );

void IN_ShowTeamEquipmentDown( const CCommand &args )
{
	panorama::DispatchEvent( CSGOHudShowTeamEquipment(), nullptr, true );
}
void IN_ShowTeamEquipmentUp( const CCommand &args )
{
	panorama::DispatchEvent( CSGOHudShowTeamEquipment(), nullptr, false );
}
static ConCommand p_cl_show_team_equipment( "+cl_show_team_equipment", IN_ShowTeamEquipmentDown );
static ConCommand m_cl_show_team_equipment( "-cl_show_team_equipment", IN_ShowTeamEquipmentUp );


extern bool IsTakingAFreezecamScreenshot();
extern ConVar cl_drawhud;
extern ConVar cl_crosshairstyle;
extern ConVar weapon_debug_spread_show;
extern ConVar cl_draw_only_deathnotices;
extern ConVar mp_hostages_takedamage;
extern ConVar sv_teamid_overhead;
extern ConVar spec_show_xray;
extern ConVar cl_teamid_overhead_maxdist;
extern ConVar cl_teamid_overhead_maxdist_spec;
extern ConVar voice_icons_method;
extern ConVar crosshair;
extern ConVar cl_fixedcrosshairgap;
extern ConVar lockMoveControllerRet;
extern ConVar hud_showtargetid;
extern ConVar cl_drawhud_force_teamid_overhead;
extern ConVar cl_teamid_overhead_always;
extern ConVar sv_show_ragdoll_playernames;

char s_weaponNameUtf8ScratchBuffer[512];	// Scratch buffer used to store weapon name & rarity
char s_playerNameUtf8ScratchBuffer[4 * MAX_DECORATED_PLAYER_NAME_LENGTH];	// Scratch buffer used to store decorated player name (
																			// (Conversion from unicode to UTF-8. Each Unicode point can
																			// expand to as many as four bytes in UTF-8)


struct CrosshairTargetIdRenderData_t
{
	CrosshairTargetIdRenderData_t()
	:
		m_nDesiredReticleMode( CCSGO_HudReticle::RETICLE_MODE_NONE ),
		m_nDesiredGap( -1 ),
		m_nDesiredSpread( -1 ),
		m_flDesiredFishtail( 0.0f ),
		m_bShowFriendlyCrosshair( false ),
		m_bWantEnemyCrosshair( false ),
		m_pszTargetID( nullptr ),
		m_pszPlayerName( nullptr ),
		m_nPlayerHealth( -1 ),
		m_pszWeaponName( nullptr ),
		m_pszWeaponColor( nullptr )
	{}

	// Crosshair
	CCSGO_HudReticle::ReticleMode_t m_nDesiredReticleMode;
	int m_nDesiredGap;				// Pixel value in screen space
	int m_nDesiredSpread;			// Pixel value in screen space
	float m_flDesiredFishtail;		// Pixel value in screen space
	bool m_bShowFriendlyCrosshair;
	bool m_bWantEnemyCrosshair;

	// Target ID
	const char *m_pszTargetID;
	const char *m_pszPlayerName;
	int m_nPlayerHealth;
	const char *m_pszWeaponName;
	const char *m_pszWeaponColor;
};


static bool BDrawHudTeamIdElements()
{
	return ( cl_drawhud_force_teamid_overhead.GetInt() >= 0 ) && (
		( cl_drawhud_force_teamid_overhead.GetInt() > 0 ) ||
		( cl_draw_only_deathnotices.GetBool() == false )
		);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCSGO_HudReticle::CCSGO_HudReticle( panorama::CPanel2D *pParent, const char *pchID )
	:
	panorama::CPanel2D( pParent, pchID ),
	CPanoramaHudElement( "CCSGO_HudReticle", this ),
	m_bForceShowAllTeammateTargetIDs( false ),
	m_vecTransforms( 0, 1 )
{
	SetHiddenBits( HIDEHUD_PLAYERDEAD | HIDEHUD_CROSSHAIR );

	RequireLoadLayout( "file://{resources}/layout/hud/hudreticle.xml" );

	RegisterForUnhandledEvent( panorama::ReloadStyleFile(), this, &CCSGO_HudReticle::OnStyleFileReloaded );

	m_pCrosshairPanel = RequireChildInLayoutFile( "Crosshair" );
	m_pCrosshairObserverPanel = RequireChildInLayoutFile( "CrosshairObserver" );

	m_pArcPanels[ CROSSHAIR_PART_TOP ] = RequireChildInLayoutFile( "TopArc" );
	m_pArcPanels[ CROSSHAIR_PART_BOTTOM ] = RequireChildInLayoutFile( "BottomArc" );
	m_pArcPanels[ CROSSHAIR_PART_LEFT ] = RequireChildInLayoutFile( "LeftArc" );
	m_pArcPanels[ CROSSHAIR_PART_RIGHT ] = RequireChildInLayoutFile( "RightArc" );

	m_pPipPanels[ CROSSHAIR_PART_TOP ] = RequireChildInLayoutFile( "TopPip" );
	m_pPipPanels[ CROSSHAIR_PART_BOTTOM ] = RequireChildInLayoutFile( "BottomPip" );
	m_pPipPanels[ CROSSHAIR_PART_LEFT ] = RequireChildInLayoutFile( "LeftPip" );
	m_pPipPanels[ CROSSHAIR_PART_RIGHT ] = RequireChildInLayoutFile( "RightPip" );

	m_pBlackPipPanels[CROSSHAIR_PART_TOP] = RequireChildInLayoutFile( "TopPipBlack" );
	m_pBlackPipPanels[CROSSHAIR_PART_BOTTOM] = RequireChildInLayoutFile( "BottomPipBlack" );
	m_pBlackPipPanels[CROSSHAIR_PART_LEFT] = RequireChildInLayoutFile( "LeftPipBlack" );
	m_pBlackPipPanels[CROSSHAIR_PART_RIGHT] = RequireChildInLayoutFile( "RightPipBlack" );

	m_pFriendPanel = RequireChildInLayoutFile( "FriendCrosshair" );

	m_pTargetIDPanel = panorama::panel_cast< panorama::CLabel * >( RequireChildInLayoutFile( "TargetID" ) );

	m_pVisiblePlayerIDsPanel = RequireChildInLayoutFile( "VisiblePlayerIDs" );
	m_pUnusedPlayerIDsPanel = RequireChildInLayoutFile( "UnusedPlayerIDs" );
	// TODO pre-populate "UnusedPlayerIDs" panel

	GetLayoutDefines();

	ResetData();

	m_vecTransforms.SetCount( 1 );	// Only ever setting one transformation per panel (applied to pip&arc panels)

	m_bWantLateUpdate = true;

	// No alternate ticks for the reticle, otherwise arrows above players' heads go out of sync.	
	SetForceBuildPaintCmdCache( false );
	SetAllowAlternateTicks( false );

	RegisterForUnhandledEvent( CSGOHudShowTeamEquipment(), this, &CCSGO_HudReticle::OnShowTeamEquipment );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCSGO_HudReticle::~CCSGO_HudReticle()
{

}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudReticle::LevelInit( void )
{
	ListenForGameEvent( "round_start" );

	ResetData();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudReticle::LevelShutdown( void )
{
	StopListeningForAllEvents();

	ResetData();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudReticle::ProcessInput( void )
{
	/////////////////////////////////// 
	//	Data used to update the reticle panel UI
	///////////////////////////////////

	CrosshairTargetIdRenderData_t crosshairRenderData;


	///////////////////////////////////
	// Collect data
	///////////////////////////////////

	GetCrosshairTargetIdRenderData( crosshairRenderData );
	GetPlayerIDsRenderData();


	/////////////////////////////////// 
	// Update reticle panel UI
	///////////////////////////////////

	UpdateCrosshairUI( crosshairRenderData );
	UpdatePlayerIDsUI();


	///////////////////////////////////
	// Save data to be used next frame
	///////////////////////////////////

	m_iReticleMode = crosshairRenderData.m_nDesiredReticleMode;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCSGO_HudReticle::ShouldDraw( void )
{
	if ( IsTakingAFreezecamScreenshot() )
		return false;

	C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return false;

	if ( pPlayer->GetObserverInterpState() == C_CSPlayer::OBSERVER_INTERP_TRAVELING )
		return false;

	bool bNeedsDraw = false;
	if ( ( pPlayer->GetObserverMode() == OBS_MODE_ROAMING ) || ( pPlayer->GetObserverMode() == OBS_MODE_FIXED ) )
		bNeedsDraw = true;

	return cl_drawhud.GetBool() && ( bNeedsDraw || CHudElement::ShouldDraw() );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudReticle::SetActive( bool bActive )
{
	ShowPanel( bActive );
	
	CHudElement::SetActive( bActive );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudReticle::ShowPanel( bool bShow )
{
	static const panorama::CPanoramaSymbol k_symHidden( "reticle--hidden" );

	SetHasClass( k_symHidden, ( bShow == false ) );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudReticle::ResetData()
{
	m_iReticleMode = RETICLE_MODE_INVALID;
	m_nCrosshairColorClassId = -1;
	m_flTargetIDTimer = 0.0f;
	m_strCurrentTargetID = "";
	m_flWindowScaleFactor = -1.0f;

	RemoveAllPlayerID();
}


//-----------------------------------------------------------------------------
// Purpose: Read variable from CSS file
//-----------------------------------------------------------------------------
void CCSGO_HudReticle::GetLayoutDefines()
{
	m_flPipDefaultOffset = GetLayoutFileDefineFloat( "cppPipDefaultOffset", 0.0f );
	m_flBlackPipDefaultOffset = GetLayoutFileDefineFloat( "cppBlackPipDefaultOffset", 0.0f );
	m_flArcDefaultOffset = GetLayoutFileDefineFloat( "cppArcDefaultOffset", 0.0f );
	m_flTargetIDDuration = GetLayoutFileDefineFloat( "cppTargetIDDuration", 0.0f );
}


//-----------------------------------------------------------------------------
// Purpose: CSS potentially reloaded
//-----------------------------------------------------------------------------
bool CCSGO_HudReticle::OnStyleFileReloaded( panorama::CPanoramaSymbol symFile )
{
	GetLayoutDefines();

	// let bubble
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Handle +/-cl_show_team_equipment
//-----------------------------------------------------------------------------
bool CCSGO_HudReticle::OnShowTeamEquipment( bool bShow )
{
	m_bForceShowAllTeammateTargetIDs = bShow;
	return false;
}



//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudReticle::GetCrosshairTargetIdRenderData( CrosshairTargetIdRenderData_t &renderData )
{
	C_CSPlayer *pCSPlayer = GetHudPlayer();
	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();

	// cl_crosshairstyle
	//0 = default
	//1 = default static
	//2 = classic standard (vgui)
	//3 = classic dynamic (vgui)
	//4 = classic static (vgui)
	bool bVguiCrosshair = ( cl_crosshairstyle.GetInt() >= 2 );

	//
	// do crosshair calculations first because they can affect the
	// display of the target id
	//

	bool bAllowTargetID = true;
	bool bShowFriendEnemyDesignation = false;
	bool bFriendlyCrosshairOkay = false;
	bool bShowDroppedWeaponNames = false;
	bool bCheckWeaponRange = false;
	int nObsMode = OBS_MODE_NONE;

	/************************************
	* the logic below is complicated, but here's what it means:
	* 1 - If we're not in spectator mode, then only show a crosshair if we're not in an input mode
	*     and we have a weapon ( normally true ) and the weapon wants a crosshair.
	* 2 - If we're in spectator in eye mode, show a crosshair if the weapon wants it
	*     but don't show the friendly target icon
	* 3 - If we're in spectator roaming mode, show the spectator crosshair
	* 4 - Otherwise show no crosshair
	*/
	if ( pLocalPlayer && crosshair.GetBool() )
	{
		bool bWantWeaponShown = false;
		bool bHasWeapon = false;
		bool bPlayerInInputMode = false;
		bool bShowCrosshair = false;

		nObsMode = pLocalPlayer->GetObserverMode();

		switch ( nObsMode )
		{
		case OBS_MODE_IN_EYE:
		case OBS_MODE_ROAMING:
			bShowCrosshair = true;
			break;

		case OBS_MODE_NONE:
			bFriendlyCrosshairOkay = true;
			bShowDroppedWeaponNames = true;
			bShowCrosshair = true;
			bShowFriendEnemyDesignation = true;
			break;

		default:
			bShowCrosshair = false;
			break;
		}

		if ( pLocalPlayer->IsPlayerGhost() )
			bFriendlyCrosshairOkay = true;

#ifdef IRONSIGHT
		CWeaponCSBase *pCSWeapon = pLocalPlayer->GetActiveCSWeapon();
		if ( pCSWeapon && pCSWeapon->GetIronSightController() && !weapon_debug_spread_show.GetBool() )
		{
			bShowCrosshair = !pCSWeapon->GetIronSightController()->ShouldHideCrossHair();
		}
#endif //IRONSIGHT

		if ( bShowCrosshair && nObsMode != OBS_MODE_ROAMING )
		{
			if ( pLocalPlayer->IsInVGuiInputMode() || pLocalPlayer->IsInViewModelVGuiInputMode() )
			{
				bPlayerInInputMode = true;
			}
			else if ( !bVguiCrosshair )
			{
				CWeaponCSBase *pWeapon = (CWeaponCSBase*)pCSPlayer->GetActiveWeapon();

				if ( pWeapon )
				{
					bHasWeapon = true;

					if ( pWeapon->WantReticleShown() )
					{
						if ( pWeapon->GetItemDefinition() == WEAPON_TASER )
						{
							// When using the taser we don't want to change the reticle color until the
							// target is in range.
							bCheckWeaponRange = true;
						}

						bWantWeaponShown = true;

						// Value returned in screen space
						renderData.m_nDesiredSpread = pWeapon->GetReticleWeaponSpread();
						renderData.m_nDesiredGap = pWeapon->GetReticleCrosshairGap();
						renderData.m_flDesiredFishtail = pWeapon->GetAccuracyFishtail();
					}
				}
			}
		}

		// Special case for killer replay or replays in general
		// This handles killer replay and survival infinite replay modes
		// still leaves the weird looking AWP zoom in thirdperson, but that's not what official servers run...
		if ( pLocalPlayer->IsHLTV() )
		{
			// Never allow these, suppress even if above code wanted these:
			bAllowTargetID = false;
			bShowFriendEnemyDesignation = false;
			bFriendlyCrosshairOkay = false;
			bShowDroppedWeaponNames = false;

			// Suppress these in chase/free, but allow in POV if above code determined so:
			if ( nObsMode != OBS_MODE_IN_EYE )
			{
				bShowCrosshair = false;
				bCheckWeaponRange = false;
			}
		}

		// the reticle mode is defaulted to NONE above (outside this clause) so
		// we don't have to explicitly sent none

		if ( bShowCrosshair )
		{
			if ( nObsMode == OBS_MODE_ROAMING )
			{
				renderData.m_nDesiredReticleMode = RETICLE_MODE_OBSERVER;
			}
			else if ( pLocalPlayer && !bPlayerInInputMode && bHasWeapon && bWantWeaponShown )
			{
				renderData.m_nDesiredReticleMode = RETICLE_MODE_WEAPON;
			}
		}
	}

	if ( bVguiCrosshair )
	{
		renderData.m_nDesiredReticleMode = RETICLE_MODE_NONE;
	}


	//
	// Target ID
	//

	if ( bAllowTargetID && ( hud_showtargetid.GetBool() || (g_HltvReplaySystem.IsHltvReplayButtonEnabled() && !g_HltvReplaySystem.IsDelayedReplayRequestPending()) ) )
	{
		bool bLocalIsSpectatorViewer = CanSeeSpectatorOnlyTools();
		bool bIsFullyBlinded = pCSPlayer && pCSPlayer->m_flFlashOverlayAlpha >= 180.0f;
		bool bIsFlashed = pCSPlayer && pCSPlayer->m_flFlashBangTime > ( gpGlobals->curtime + 0.5 );

		// if we're still blinded, then leave everything just like it is
		if ( !bLocalIsSpectatorViewer && bIsFlashed )
			return;

		// if we're spectating and the player we're observing is fully blind, say so in the target id
		if ( nObsMode == OBS_MODE_IN_EYE && bIsFullyBlinded && BDrawHudTeamIdElements() )
		{
			renderData.m_pszTargetID = "#SFUIHUD_targetid_FLASHED";
		}
		else
		{
			// Get our target's ent index
			int iEntIndex = hud_showtargetid.GetBool() ? pCSPlayer->GetIDTarget() : 0;
			GetPlayerTargetIDRenderData( renderData, iEntIndex, bFriendlyCrosshairOkay, bShowFriendEnemyDesignation, bCheckWeaponRange );
			if ( !renderData.m_pszTargetID && bShowDroppedWeaponNames )
			{
				GetWeaponTargetIDRenderData( renderData );
			}
		}
	}
	else
	{
		// Force the target ID to be wiped right now in whatever timeline the curtime is at this render call
		m_flTargetIDTimer = gpGlobals->curtime;
		// otherwise it will only get cleared +0.25 seconds from the time the local player was killed (which is 20 sec in the future when rewinding into kill replay)
	}
}


//-----------------------------------------------------------------------------
// Purpose: Collect "TargetID" panel data corresponding to the player behind our crosshair (if any)
//-----------------------------------------------------------------------------
void CCSGO_HudReticle::GetPlayerTargetIDRenderData( CrosshairTargetIdRenderData_t &renderData, int iEntIndex, bool bFriendlyCrosshairOkay, bool bShowFriendEnemyDesignation, bool bCheckWeaponRange )
{
	if ( !iEntIndex )
	{
		return;
	}

	C_CSPlayer *pCSPlayer = GetHudPlayer();
	if ( !pCSPlayer )
		return;

	C_CSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( iEntIndex ) );

	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pLocalPlayer )
		return;

	bool bShowHealth = false;
	bool bShowPlayerName = false;

	// show ragdoll player names
	bool bDeadRagdoll = false;

	if ( sv_show_ragdoll_playernames.GetBool() )
	{
		C_CSRagdoll *pRagdoll = dynamic_cast<C_CSRagdoll*>(cl_entitylist->GetEnt( iEntIndex ));
		if ( pRagdoll )
		{
			C_CSPlayer *pRagdollPlayer = ToCSPlayer( pRagdoll->GetPlayerHandle() );
			if ( pRagdollPlayer && !pRagdollPlayer->IsAlive() )
			{
				bDeadRagdoll = true;
				iEntIndex = pRagdollPlayer->entindex();
			}
		}
	}

	// Some entities we always want to check, cause the text may change
	// even while we're looking at it
	// Is it a player?
	if ( pPlayer )
	{
		{
			bShowPlayerName = true;

			if ( !pPlayer->IsOtherEnemy( pLocalPlayer->entindex() ) )
			{
				if ( !bShowFriendEnemyDesignation )
					renderData.m_pszTargetID = "#Panorama_HUD_playerid_specteam";
				else
					renderData.m_pszTargetID = "#Panorama_HUD_playerid_sameteam";

				renderData.m_bShowFriendlyCrosshair = bFriendlyCrosshairOkay;
				bShowHealth = true;
			}
			else if ( pLocalPlayer->GetTeamNumber() != TEAM_CT && pLocalPlayer->GetTeamNumber() != TEAM_TERRORIST )
			{
				renderData.m_pszTargetID = "#Panorama_HUD_playerid_noteam";
				renderData.m_bShowFriendlyCrosshair = bFriendlyCrosshairOkay;
				bShowHealth = true;
			}
			else
			{
				if ( !bShowFriendEnemyDesignation )
					renderData.m_pszTargetID = "#Panorama_HUD_playerid_specteam";
				else if ( pCSPlayer->m_hSurvivalAssassinationTarget.Get() == pPlayer )
					renderData.m_pszTargetID = "#Panorama_HUD_playerid_survival_hunter_target";
				else if ( CSGameRules()->IsPlayingSurvival() )
					renderData.m_pszTargetID = "#Panorama_HUD_playerid_survival_enemy"; // no enemy names in survival
				else
					renderData.m_pszTargetID = "#Panorama_HUD_playerid_diffteam";

				renderData.m_bShowFriendlyCrosshair = false;
				renderData.m_bWantEnemyCrosshair = bShowFriendEnemyDesignation;

				if ( renderData.m_bWantEnemyCrosshair && bCheckWeaponRange )
				{
					CWeaponCSBase *pWeapon = pCSPlayer ? (CWeaponCSBase*)pCSPlayer->GetActiveWeapon() : nullptr;

					if ( pWeapon )
					{
						float flWeaponRange = pWeapon->GetCSWpnData().GetRange( pWeapon->GetEconItemView() );

						vec_t length = VectorLength( pPlayer->WorldSpaceCenter() - pLocalPlayer->Weapon_ShootPosition() );
						renderData.m_bWantEnemyCrosshair = ( length <= flWeaponRange );
					}
				}

			}

			if ( bDeadRagdoll )
			{
				renderData.m_pszTargetID = "#SFUIHUD_playerid_dead";
			}

			if ( pPlayer->IsPlayerGhost() && !pLocalPlayer->IsPlayerGhost() )
			{
				bShowHealth = false;
				bShowPlayerName = false;
				renderData.m_bWantEnemyCrosshair = false;
				renderData.m_pszPlayerName = "";
			}

			if ( sv_teamid_overhead.GetInt() && renderData.m_bShowFriendlyCrosshair )
			{
				// only get the name and health if necessary
				bShowHealth = false;
				bShowPlayerName = false;

				// set to null first
				renderData.m_pszTargetID = nullptr;

				Vector vDelta = pPlayer->EyePosition() - pLocalPlayer->EyePosition();
				float flDistance = vDelta.Length();
				if ( !pPlayer->IsOtherEnemy( pLocalPlayer->entindex() ) && pPlayer->IsBot() && flDistance < PLAYER_USE_BOT_RADIUS )
				{
					if ( pLocalPlayer->IsPlayerGhost() && !pPlayer->IsPlayerGhost() )
					{
						renderData.m_pszTargetID = "#PANOHUD_Spectate_Navigation_Control_Bot";
					}
					else if ( pPlayer->HasC4() )
					{
						renderData.m_pszTargetID = "#Panorama_HUD_botid_request_bomb";
					}			
				}
			}

			if ( bShowHealth )
			{
				float flHealth = MAX( 0.0f, ( float )pPlayer->GetHealth() );
				renderData.m_nPlayerHealth = Floor2Int( ( flHealth / (float)pPlayer->GetMaxHealth() ) * 100 );
			}
			if ( bShowPlayerName )
			{
				wchar_t wszPlayerName[ MAX_DECORATED_PLAYER_NAME_LENGTH ];
				C_CS_PlayerResource *cs_PR = dynamic_cast<C_CS_PlayerResource *>( g_PR );

				if ( cs_PR )
				{
					cs_PR->GetDecoratedPlayerName( iEntIndex, wszPlayerName, sizeof( wszPlayerName ), k_EDecoratedPlayerNameFlag_AddBotToNameIfControllingBot );

					// Convert to UTF8 into scratch buffer
					V_UnicodeToUTF8( wszPlayerName, s_playerNameUtf8ScratchBuffer, ARRAYSIZE( s_playerNameUtf8ScratchBuffer ) );
					
					renderData.m_pszPlayerName = s_playerNameUtf8ScratchBuffer;	// Only one reticle panel so it is safe to use the scratch buffer


					if ( CSGameRules() && CSGameRules()->IsPlayingSurvival() && !bDeadRagdoll )
					{
						// only show the name of the enemy if they're close enough, or the player is scoped.

						float flPlayerDistance = pPlayer->WorldSpaceCenter().DistTo( pLocalPlayer->WorldSpaceCenter() );

						static ConVarRef sv_dz_show_enemy_name_scope_range( "sv_dz_show_enemy_name_scope_range" );
						if ( flPlayerDistance > sv_dz_show_enemy_name_scope_range.GetFloat() && !pCSPlayer->m_bIsScoped )
						{
							renderData.m_pszPlayerName = "";
						}
					}
				}
			}
		}
	}
	else
	{
		C_BaseEntity *pEnt = cl_entitylist->GetEnt( iEntIndex );

		//Hostages!
		C_CHostage *pHostage = NULL;

		for ( int i = 0; i < g_Hostages.Count(); i++ )
		{
			// compare entity pointers
			if ( g_Hostages[i] == pEnt )
			{
				pHostage = g_Hostages[i];
				break;
			}
		}

		if ( pHostage != NULL )
		{
			renderData.m_bShowFriendlyCrosshair = bFriendlyCrosshairOkay;

			bShowHealth = mp_hostages_takedamage.GetBool();
			
			Vector forward;
			pLocalPlayer->EyeVectors( &forward );

			trace_t tr;
			// Search for objects in a sphere (tests for entities that are not solid, yet still useable)
			Vector searchCenter = pLocalPlayer->EyePosition();
			int useableContents = MASK_NPCSOLID_BRUSHONLY | MASK_OPAQUE_AND_NPCS;
			UTIL_TraceLine( searchCenter, searchCenter + forward * 1024, useableContents, pLocalPlayer, COLLISION_GROUP_NONE, &tr );
			if ( tr.m_pEnt )
			{
				CConfigurationForHighPriorityUseEntity_t cfgUseHostageRule;
				bool bValidHostageRule = pLocalPlayer->GetUseConfigurationForHighPriorityUseEntity( pHostage, cfgUseHostageRule );
				// if we're outside use range, we're a terrorist or the hostage has a leader and it's not the player...
				if ( !bValidHostageRule || !cfgUseHostageRule.UseByPlayerNow( pLocalPlayer, cfgUseHostageRule.k_EPlayerUseType_Start ) ||
					pLocalPlayer->GetTeamNumber() == TEAM_TERRORIST || ( pHostage->GetLeader() && pHostage->GetLeader() != pLocalPlayer ) ||
					( HOSTAGE_RULE_CAN_PICKUP && pLocalPlayer->m_hCarriedHostage != NULL ) )
				{
					if ( pHostage->GetLeader() && pHostage->GetLeader() != pLocalPlayer )
					{
						if ( !bShowHealth )
						{
							renderData.m_pszTargetID = "#Panorama_HUD_hostageid_nh_following";
						}
						else
						{
							renderData.m_pszTargetID = "#Panorama_HUD_hostageid_following";
						}

						// Get Name
						C_CS_PlayerResource *cs_PR = dynamic_cast<C_CS_PlayerResource *>( g_PR );
						if ( cs_PR )
						{
							wchar_t wszPlayerName[ MAX_DECORATED_PLAYER_NAME_LENGTH ];
							cs_PR->GetDecoratedPlayerName( pHostage->GetLeader()->entindex(), wszPlayerName, sizeof( wszPlayerName ), k_EDecoratedPlayerNameFlag_AddBotToNameIfControllingBot );
							
							// Convert to UTF8 into scratch buffer
							V_UnicodeToUTF8( wszPlayerName, s_playerNameUtf8ScratchBuffer, ARRAYSIZE( s_playerNameUtf8ScratchBuffer ) );
							renderData.m_pszPlayerName = s_playerNameUtf8ScratchBuffer;	// Only one reticle panel so it is safe to use the scratch buffer
						}
						else
						{
							renderData.m_pszPlayerName = "";
						}
					}
					else
					{
						if ( !bShowHealth )
						{
							if ( CSGameRules()->IsPlayingSurvival() )
							{
								renderData.m_pszTargetID = "#SFUI_SurvivalHostageName";
							}
							else if ( CSGameRules()->IsPlayingCooperativeGametype() )
							{
								renderData.m_pszTargetID = "#Panorama_HUD_hostagename_nh";

								renderData.m_pszPlayerName = panorama::UILocalize()->PchFindRawString( pHostage->GetCustomHostageNameForMap( engine->GetLevelNameShort() ) );
							}
							else
							{
								renderData.m_pszTargetID = "#Panorama_HUD_hostageid_nh";
							}
						}
						else
						{
							renderData.m_pszTargetID = "#Panorama_HUD_hostageid";
						}
					}
				}
				else if ( !pHostage->GetLeader() )
				{
					if ( !bShowHealth )
					{
						renderData.m_pszTargetID = "#Panorama_HUD_hostageid_nh_use_lead";
					}
					else
					{
						renderData.m_pszTargetID = "#Panorama_HUD_hostageid_use_lead";
					}
				}
			}

			if ( bShowHealth )
			{
				float flHealth = MAX( 0.0f, (float)pHostage->GetHealth() );
				renderData.m_nPlayerHealth = Floor2Int( ( flHealth / (float)pHostage->GetMaxHealth() ) * 100 );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudReticle::GetWeaponTargetIDRenderData( CrosshairTargetIdRenderData_t &renderData )
{
	C_CSPlayer *pCSPlayer = GetHudPlayer();	
	int weaponEntIndex = pCSPlayer->GetTargetedWeapon();

	if ( weaponEntIndex <= 0 ) //0 is a valid entity index, but will never be used for a weapon
	{
		return;
	}

	C_BaseEntity *pEnt = cl_entitylist->GetEnt( weaponEntIndex );
	C_PlantedC4 *pC4 = dynamic_cast<C_PlantedC4*>( pEnt );
	if ( pC4 )
	{
		return;
	}

	CWeaponCSBase* weapon = static_cast<CWeaponCSBase*>( pEnt );
	CEconItemView *pItem = weapon->GetEconItemView();
	AcquireResult::Type acquireResult = pCSPlayer->CanAcquire( pItem, AcquireMethod::PickUp );
	bool isWeaponAllowed = ( weapon && ( acquireResult == AcquireResult::Allowed ) || ( acquireResult == AcquireResult::AlreadyOwned ) ); // we allow players to pick up guns without ammo  --mtw

	const char *pszTargetWeapon = nullptr;
	const char* pszTargetColor = nullptr;
	if ( pItem && pItem->IsValid() )
	{
		const CEconItemRarityDefinition* pRarity = GetItemSchema()->GetRarityDefinition( pItem->GetRarity() );
		pszTargetColor = GetHexColorForAttribColor( pRarity->GetAttribColor() );

		V_UnicodeToUTF8( pItem->GetItemName(), s_weaponNameUtf8ScratchBuffer, sizeof( s_weaponNameUtf8ScratchBuffer ) );
		pszTargetWeapon = s_weaponNameUtf8ScratchBuffer; // Only one reticle panel, so it is safe to use the scratch buffer
	}
	else
	{
		pszTargetColor = GetHexColorForAttribColor( ATTRIB_COL_RARITY_DEFAULT );
		pszTargetWeapon = weapon->GetPrintName();
	}

	// Skip '#' at beginning of color string
	if ( pszTargetColor && *pszTargetColor == '#' )
		pszTargetColor++;

	if ( isWeaponAllowed && pszTargetWeapon )
	{
		CSWeaponType nType = weapon->GetWeaponType();

		// we can swap primary, secondary, or rechargable tasers
		bool bPickup = pCSPlayer->IsPrimaryOrSecondaryWeapon( nType ) || ( nType == WEAPONTYPE_TASER );
		bool bSwap = false;

		if ( bPickup )
		{
			// check to see if we are swapping weapons or if we are just filling in an empty weapon slot
			if ( ( nType == WEAPONTYPE_PISTOL && pCSPlayer->Weapon_GetSlot( GEAR_SLOT_PISTOL ) ) ||
				( nType != WEAPONTYPE_PISTOL && pCSPlayer->Weapon_GetSlot( GEAR_SLOT_RIFLE ) ) ||
				( nType == WEAPONTYPE_TASER && pCSPlayer->Weapon_OwnsThisType( "weapon_taser" ) )
				)
			{
				bSwap = true;
			}
		}

		if ( bSwap )
		{
			renderData.m_pszTargetID = "#Panorama_HUD_weaponid_swap";
			renderData.m_pszWeaponName = pszTargetWeapon;
		}
		else
		{
			renderData.m_pszTargetID = "#Panorama_HUD_weaponid_lookat";
			renderData.m_pszWeaponName = pszTargetWeapon;
		}

		renderData.m_pszWeaponColor = pszTargetColor;
	}
	else if ( acquireResult == AcquireResult::HeavyAssaultSuitRestriction )
	{
		renderData.m_pszTargetID =  "#SFUI_BuyMenu_HeavyAssaultSuitRestriction";
		renderData.m_pszWeaponName = nullptr;
		renderData.m_pszWeaponColor = nullptr;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudReticle::UpdateCrosshairUI( const CrosshairTargetIdRenderData_t &renderData )
{
	static const panorama::CPanoramaSymbol k_symCrosshairHidden( "crosshair--hidden" );
	static const panorama::CPanoramaSymbol k_symCrosshairObserverHidden( "crosshair__observer--hidden" );
	static const panorama::CPanoramaSymbol k_symCrosshairEnemy( "crosshair--enemy" );
	static const panorama::CPanoramaSymbol k_symCrosshairArcHidden( "crosshair--archidden" );
	static const panorama::CPanoramaSymbol k_symCrosshairPipHidden( "crosshair--piphidden" );
	static const panorama::CPanoramaSymbol k_symCrosshairFriendHidden( "crosshair__friend--hidden" );
	static const panorama::CPanoramaSymbol k_symTargetIDHidden( "targetid--hidden" );

	static const panorama::CPanoramaSymbol k_symCrosshairColors[] = 
	{
		panorama::CPanoramaSymbol( "crosshair--color1" ),
		panorama::CPanoramaSymbol( "crosshair--color2" ),
		panorama::CPanoramaSymbol( "crosshair--color3" ),
		panorama::CPanoramaSymbol( "crosshair--color4" ),
	};

	float flWindowScaleFactor = GetParentWindow()->GetWindowScaleFactor();
	if ( !CloseEnough( m_flWindowScaleFactor, flWindowScaleFactor ) )
	{
		m_pCrosshairPanel->AccessStyleDirty()->SetUIScale( Vector( 1.0f / flWindowScaleFactor, 1.0f / flWindowScaleFactor, 1.0f / flWindowScaleFactor ) );
		m_flWindowScaleFactor = flWindowScaleFactor;
	}
	
	// Hide / Show crosshair

	if ( renderData.m_nDesiredReticleMode != m_iReticleMode )
	{
		m_pCrosshairPanel->SetHasClass( k_symCrosshairHidden, renderData.m_nDesiredReticleMode != RETICLE_MODE_WEAPON );
		m_pCrosshairObserverPanel->SetHasClass( k_symCrosshairObserverHidden, renderData.m_nDesiredReticleMode != RETICLE_MODE_OBSERVER );
	}


	// Update crosshair's position

	if ( renderData.m_nDesiredReticleMode == RETICLE_MODE_WEAPON )
	{
		// TODO Re-enable for Panorama
#if 0
		float flOffsetX = 0.0f;
		float flOffsetY = 0.0f;

#ifdef SIXENSE
		if ( g_pSixenseInput->IsEnabled() &&
			C_BasePlayer::GetLocalPlayer() &&
			( C_BasePlayer::GetLocalPlayer()->GetObserverMode() == OBS_MODE_NONE ) &&
			lockMoveControllerRet.GetBool() == false )
#else
		if ( inputsystem->MotionControllerActive() &&
			C_BasePlayer::GetLocalPlayer() &&
			( C_BasePlayer::GetLocalPlayer()->GetObserverMode() == OBS_MODE_NONE ) &&
			lockMoveControllerRet.GetBool() == false )
#endif
		{

			Vector aimDirection;

			static ConVarRef mc_use_recoil_on_cursor( "mc_use_recoil_on_cursor" );	// TODO currently defined in sfhudreticle.cpp
			if ( mc_use_recoil_on_cursor.GetBool() )
			{
				AngleVectors( pCSPlayer->GetFinalAimAngle(), &aimDirection );
			}
			else
			{
				aimDirection = pCSPlayer->GetAimDirection();
			}

			Vector screen;
			Vector point;
			VectorAdd( CurrentViewOrigin(), aimDirection, point );
			ScreenTransform( point, screen );

			flOffsetX = 0.5f * screen[0] * ScreenWidth() + 0.5f;
			flOffsetY = -0.5f * screen[1] * ScreenHeight() + 0.5f;
		}
#endif

		//0 = default
		//1 = default static
		if ( cl_crosshairstyle.GetInt() == 0 )
		{
			// arcs

			if ( renderData.m_nDesiredSpread == -1 )
			{
				m_pCrosshairPanel->AddClass( k_symCrosshairArcHidden );
			}
			else
			{
				m_pCrosshairPanel->RemoveClass( k_symCrosshairArcHidden );
				UpdateCrosshairPartsPosition( m_pArcPanels, (float)renderData.m_nDesiredSpread + m_flArcDefaultOffset );
			}

			// pips

			if ( renderData.m_nDesiredGap == -1 )
			{
				m_pCrosshairPanel->AddClass( k_symCrosshairPipHidden );
			}
			else
			{
				m_pCrosshairPanel->RemoveClass( k_symCrosshairPipHidden );
				UpdateCrosshairPartsPosition( m_pPipPanels, (float)renderData.m_nDesiredGap + m_flPipDefaultOffset );
				UpdateCrosshairPartsPosition( m_pBlackPipPanels, (float)renderData.m_nDesiredGap + m_flBlackPipDefaultOffset );
			}

			// TODO fishtail
		}
		else
		{
			// arcs invisible

			m_pCrosshairPanel->AddClass( k_symCrosshairArcHidden );

			// pips at fixed position

			m_pCrosshairPanel->RemoveClass( k_symCrosshairPipHidden );
			UpdateCrosshairPartsPosition( m_pPipPanels, cl_fixedcrosshairgap.GetFloat() + m_flPipDefaultOffset );
			UpdateCrosshairPartsPosition( m_pBlackPipPanels, cl_fixedcrosshairgap.GetFloat() + m_flBlackPipDefaultOffset );
		}

		// TODO Apply offset
	}

	// Friendly crosshair

	m_pFriendPanel->SetHasClass( k_symCrosshairFriendHidden, ( renderData.m_bShowFriendlyCrosshair == false ) );

	// Crosshair color

	static ConVarRef cl_crosshaircolor( "cl_crosshaircolor" );
	if ( renderData.m_bWantEnemyCrosshair )
	{
		m_pCrosshairPanel->RemoveClasses( k_symCrosshairColors, V_ARRAYSIZE( k_symCrosshairColors ) );
		m_pCrosshairPanel->AddClass( k_symCrosshairEnemy );
		m_nCrosshairColorClassId = -1;
	}
	else if ( m_nCrosshairColorClassId != cl_crosshaircolor.GetInt() )
	{
		m_pCrosshairPanel->RemoveClasses( k_symCrosshairColors, V_ARRAYSIZE( k_symCrosshairColors ) );
		m_pCrosshairPanel->RemoveClass( k_symCrosshairEnemy );

		// m_nCrosshairColorClassId in the range [1..4]
		m_nCrosshairColorClassId = cl_crosshaircolor.GetInt();
		if ( ( m_nCrosshairColorClassId < 1 ) || ( m_nCrosshairColorClassId > ( V_ARRAYSIZE( k_symCrosshairColors ) + 1 ) ) )
		{
			m_nCrosshairColorClassId = 1;
		}
		m_pCrosshairPanel->AddClass( k_symCrosshairColors[m_nCrosshairColorClassId - 1] );
	}

	// Target ID

	if ( renderData.m_pszTargetID )
	{
		m_pTargetIDPanel->RemoveClass( k_symTargetIDHidden );
		
		if ( V_strcmp( renderData.m_pszTargetID, m_strCurrentTargetID.String() ) )
		{
			m_pTargetIDPanel->SetText( renderData.m_pszTargetID );
			m_strCurrentTargetID = renderData.m_pszTargetID;
		}

		// Set necessary dialog variables
		if ( renderData.m_pszWeaponName )
		{
			m_pTargetIDPanel->SetDialogVariable( "weapon_name", renderData.m_pszWeaponName );
		}
		if ( renderData.m_pszWeaponColor )
		{
			m_pTargetIDPanel->SetDialogVariable( "weapon_color", renderData.m_pszWeaponColor );
		}
		if ( renderData.m_pszPlayerName )
		{
			m_pTargetIDPanel->SetDialogVariable( "player_name", renderData.m_pszPlayerName );
		}
		if ( renderData.m_nPlayerHealth != -1 )
		{
			m_pTargetIDPanel->SetDialogVariable( "player_health", renderData.m_nPlayerHealth );
		}

		// update the timer ( which turns the text field off after quarter a second
		m_flTargetIDTimer = gpGlobals->curtime + m_flTargetIDDuration;
	}
	else
	{
		// once our timer has run out, hide the target ID panel
		if ( gpGlobals->curtime >= m_flTargetIDTimer )
		{
			m_pTargetIDPanel->AddClass( k_symTargetIDHidden );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudReticle::UpdateCrosshairPartsPosition( panorama::CPanel2D *( &m_pPartsPanels )[CROSSHAIR_PART_COUNT], float flOffset )
{
	Assert( m_vecTransforms.Count() == 1 );
	
	m_vecTransforms[0] = new panorama::CTransformTranslate3D( 0.0f, -flOffset, 0.0f );
	m_pPartsPanels[CROSSHAIR_PART_TOP]->SetTransform3D( m_vecTransforms );	// panel should now owns the transformation, no need to free it

	m_vecTransforms[0] = new panorama::CTransformTranslate3D( 0.0f, flOffset, 0.0f );
	m_pPartsPanels[CROSSHAIR_PART_BOTTOM]->SetTransform3D( m_vecTransforms );

	m_vecTransforms[0] = new panorama::CTransformTranslate3D( -flOffset, 0.0f, 0.0f );
	m_pPartsPanels[CROSSHAIR_PART_LEFT]->SetTransform3D( m_vecTransforms );

	m_vecTransforms[0] = new panorama::CTransformTranslate3D( flOffset, 0.0f, 0.0f );
	m_pPartsPanels[CROSSHAIR_PART_RIGHT]->SetTransform3D( m_vecTransforms );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudReticle::PlayerIDRenderData_t::UpdateName( bool bShowIDName, bool bShouldShowAllFriendlyEquipment )
{
	m_bNamePanelVisible = bShouldShowAllFriendlyEquipment || bShowIDName;
	if ( !m_bNamePanelVisible )
	{
		return;
	}

	CBaseEntity *pPlayer = m_hPlayer.Get();
	if ( !pPlayer )
	{
		m_bNamePanelVisible = false;
		return;
	}
	
	bool bLocalIsSpectatorViewer = CanSeeSpectatorOnlyTools();
	bool bShowMoneyInsteadOfHealth = CSGameRules()->CanSpendMoneyInMap() && CSGameRules()->IsFreezePeriod() && !bLocalIsSpectatorViewer;

	float flHealth = 0.0f;
	flHealth = ( Max( 0.0f, ( float ) pPlayer->GetHealth( ) ) / ( float ) pPlayer->GetMaxHealth( ) );
	
	const char *printFormatString = "#Panorama_HUD_playerid_specteam";
	if ( CSGameRules()->IsPlayingSurvival() )
	{

		bool bIsPlayingTeamMode = false;
		bool bIsTeammate = false;
		
		CSurvivalGameRules* pBRrules = CSGameRules()->GetSurvivalRules();
		if ( pBRrules && pBRrules->IsPlayingTeamMode() )
		{
			bIsPlayingTeamMode = true;
			C_CSPlayer *pCSHudPlayer = GetHudPlayer();
			if ( pCSHudPlayer )
			{
				C_CSPlayer *pTargetPlayer = static_cast<C_CSPlayer*>(pPlayer);
				if ( !pCSHudPlayer->IsOtherEnemy( pTargetPlayer ) )
				{
					bIsTeammate = true;

					// show teammate money when their tablet is equipped
					CWeaponCSBase *pTargetPlayerWep = pTargetPlayer->GetActiveCSWeapon();
					if ( pTargetPlayerWep && pTargetPlayerWep->IsA( WEAPON_TABLET ) )
					{
						bShowMoneyInsteadOfHealth = true;
					}
				}
			}
		}

		// Ideally we'd like to draw player names using the correct survival team colors.
		// But the next best thing is to show everyone with grey names, unless they are on the same team as the hudplayer.

		if ( bIsPlayingTeamMode && bIsTeammate )
		{
			// blue names

			if ( bShowMoneyInsteadOfHealth )
				printFormatString = "#Panorama_HUD_playerid_overhead_survival_money";
			else if ( flHealth < 0.25f )
				printFormatString = "#Panorama_HUD_playerid_overhead_survival_lowhealth";
			else
				printFormatString = "#Panorama_HUD_playerid_overhead_survival";

		}
		else
		{
			// grey names

			if ( bShowMoneyInsteadOfHealth )
				printFormatString = "#Panorama_HUD_playerid_overhead_survival_money_grey";
			else if ( flHealth < 0.25f )
				printFormatString = "#Panorama_HUD_playerid_overhead_survival_lowhealth_grey";
			else
				printFormatString = "#Panorama_HUD_playerid_overhead_survival_grey";
		}
		
	}
	else if ( m_nTeam == TEAM_CT )
	{
		if ( bShowMoneyInsteadOfHealth )
			printFormatString = "#Panorama_HUD_playerid_overhead_ct_money";
		else if ( flHealth < 0.25f )
			printFormatString = "#Panorama_HUD_playerid_overhead_ct_lowhealth";
		else
			printFormatString = "#Panorama_HUD_playerid_overhead_ct";
	}
	else
	{
		if ( bShowMoneyInsteadOfHealth )
			printFormatString = "#Panorama_HUD_playerid_overhead_t_money";
		else if ( flHealth < 0.25f )
			printFormatString = "#Panorama_HUD_playerid_overhead_t_lowhealth";
		else
			printFormatString = "#Panorama_HUD_playerid_overhead_t";
	}
	
	if ( !m_pszName || V_strcmp( m_pszName, printFormatString ) )
	{
		m_pszName = printFormatString;
		m_bSetName = true;
	}

	// Set health / money dialog variable
	if ( bShowMoneyInsteadOfHealth )
	{
		C_CSPlayer *pTargetPlayer = static_cast<C_CSPlayer*>( pPlayer );
		m_nMoney = pTargetPlayer->GetAccount();
		m_nHealth = -1;
	}
	else
	{
		m_nHealth = static_cast<int>( flHealth * 100 );
		m_nMoney = -1;
	}

	// TODO Alpha
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudReticle::PlayerIDRenderData_t::UpdateWeapons( bool bShouldShowAllFriendlyEquipment  )
{
	// show the main equipped weapon, grenades and money during freezetime or if ShouldShowAllFriendlyEquipment
	m_bWeaponsPanelVisible = !CanSeeSpectatorOnlyTools() && ( CSGameRules()->IsFreezePeriod() ) || bShouldShowAllFriendlyEquipment;
	if ( !m_bWeaponsPanelVisible )
	{
		return;
	}
	
	CBaseEntity *pPlayer = m_hPlayer.Get();
	C_CS_PlayerResource *cs_PR = dynamic_cast<C_CS_PlayerResource *>( g_PR );
	CHudWeaponSelection *pHudSelection = (CHudWeaponSelection *)GET_HUDELEMENT( CHudWeaponSelection );
	if ( !pPlayer || !cs_PR || !pHudSelection )
	{
		m_bWeaponsPanelVisible = false;
		return;
	}
	C_CSPlayer *pTargetPlayer = static_cast<C_CSPlayer*>( pPlayer );

	// weapons are only shown in freeze time.
	m_bPrimaryVisible = CSGameRules()->IsFreezePeriod() || bShouldShowAllFriendlyEquipment;
	m_bDefuserVisible = bShouldShowAllFriendlyEquipment && cs_PR->HasDefuser( pTargetPlayer->entindex( ) );
	m_bC4Visible = bShouldShowAllFriendlyEquipment && cs_PR->HasC4( pTargetPlayer->entindex( ) );

	C_WeaponCSBase* pPrimaryWeapon = nullptr;
	if ( m_bPrimaryVisible )
	{
		// we only want to show active weapons that qualify as primary or secondary, not melee
		m_bPrimaryVisible = false;

		// While in the freeze period we want to show their primary weapon if they have one
		for ( int i = 0; i < MAX_WEAPONS; i++ )
		{
			C_WeaponCSBase *pWeapon = assert_cast<C_WeaponCSBase*>( pTargetPlayer->GetWeapon( i ) );
			if ( !pWeapon )
				continue;

			CEconItemView* pWepView = pWeapon->GetEconItemView();
			if ( !pWepView || !pWepView->IsValid() )
				continue;

			if ( IsPrimaryWeapon( pWepView ) )
			{
				// Found a primary
				pPrimaryWeapon = pWeapon;
				m_bPrimaryVisible = true;

				// no need to look at any other weapons
				break;
			}
			else if ( IsSecondaryWeapon( pWepView ) )
			{
				// Fall back to secondary if non-primary and non-secondary is active
				pPrimaryWeapon = pWeapon;
				m_bPrimaryVisible = true;
			}
		}
	}

	CUtlVector<C_WeaponCSBase*> vecWeapons;
	for ( int i = 0; i < pTargetPlayer->WeaponCount(); ++i )
	{
		C_WeaponCSBase* pWeapon = assert_cast< C_WeaponCSBase* >( pTargetPlayer->GetWeapon( i ) );
		if ( !pWeapon )
			continue;

		// we only show your main weapon and utility weapons
		if ( pWeapon != pPrimaryWeapon && !pWeapon->IsKindOf( WEAPONTYPE_GRENADE ) )
			continue;

		vecWeapons.AddToTail( pWeapon );
	}

	// sort weapons by weapon selection position
	vecWeapons.Sort( []( C_WeaponCSBase* const* ppWep1, C_WeaponCSBase* const* ppWep2 ) -> int {
		// sort in reverse order to match the old code which went from MAX slot to 0
		C_WeaponCSBase* pWepB = *ppWep2;
		C_WeaponCSBase* pWepA = *ppWep1;

		if ( pWepA->GetGearSlot() != pWepB->GetGearSlot() )
			return pWepA->GetGearSlot() - pWepB->GetGearSlot();

		return pWepA->GetGearSlotPosition() - pWepB->GetGearSlotPosition();
	} );

	m_nGrenadesVisible = 0;
	FOR_EACH_VEC( vecWeapons, i )
	{
		C_WeaponCSBase *pWeapon = vecWeapons[i];
		CEconItemView *pItem = pWeapon->GetEconItemView();
		if ( !pItem || !pItem->IsValid() )
			continue;

		const char *szWeapon = pItem->GetItemDefinition()->GetDefinitionName();
		if ( IsWeaponClassname( szWeapon ) )
			szWeapon += WEAPON_CLASSNAME_PREFIX_LENGTH;

		if ( pWeapon == pPrimaryWeapon )
		{
			if ( m_primaryWeaponName != szWeapon )
			{
				m_primaryWeaponName = szWeapon;
				m_bSetPrimaryWeaponName = true;
			}
		}
		else
		{
			// Must be a grenade
			Assert( pWeapon->IsKindOf( WEAPONTYPE_GRENADE ) );

			int nCount = pWeapon->GetReserveAmmoCount( AMMO_POSITION_PRIMARY );
			for ( int nGren = 0; nGren < nCount; nGren++ )
			{
				if ( m_nGrenadesVisible < m_nMaxGrenades )
				{	
					if ( m_grenadeWeaponNames[m_nGrenadesVisible] != szWeapon )
					{
						m_grenadeWeaponNames[m_nGrenadesVisible] = szWeapon;
						m_bSetGrenadeWeaponName = true;
					}
					m_nGrenadesVisible++;
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Iterate over all player and update the player's render data used by
//			panorama to display information above its head 
//			(name, health, money, weapon ...)
//-----------------------------------------------------------------------------
void  CCSGO_HudReticle::GetPlayerIDsRenderData()
{
	// Mark all player inactive
	FOR_EACH_VEC( m_playerIDs, i )
	{
		m_playerIDs[i].m_bActive = false;
	}

	// Update players render data
	C_CSPlayer *pCSPlayer = GetHudPlayer();
	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();

	bool bLocalIsSpectatorViewer = CanSeeSpectatorOnlyTools();
	bool bShowAllNamesForSpec = (bLocalIsSpectatorViewer && spec_show_xray.GetInt());
	bool bIsPauseMenuActive = ( GameUI().GetGameUIState() == CSGO_GAME_UI_STATE_PAUSEMENU );
	bool bTurnedOff = ( ( sv_teamid_overhead.GetBool() == false ) || !BDrawHudTeamIdElements() );
	// in replay, there's a clear indication of "You". People seem to not care about the other labels that add to visual noise.
	if ( !bIsPauseMenuActive && !bTurnedOff && !g_HltvReplaySystem.GetHltvReplayDelay() )
	{
		MDLCACHE_CRITICAL_SECTION();

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CCSPlayer* pOtherPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );

			if ( !pLocalPlayer || !pOtherPlayer || !pOtherPlayer->IsAlive() || !pOtherPlayer->IsVisible() || ( pOtherPlayer == pCSPlayer && !input->CAM_IsThirdPersonOverview() ) )
				continue;

			static ConVarRef sv_show_voip_indicator_for_enemies( "sv_show_voip_indicator_for_enemies" );
			if ( bShowAllNamesForSpec || !pCSPlayer->IsOtherEnemy( pOtherPlayer ) || ( sv_show_voip_indicator_for_enemies.GetBool() && pOtherPlayer->IsPlayerTalkingOverVOIP() ) )
			{
				bool bIsEnemy = pLocalPlayer->IsOtherEnemy( pOtherPlayer );
				Vector vecHudPlayerEyePosition = pCSPlayer->EyePosition();
				Vector vecOtherPlayerEyePosition = pOtherPlayer->EyePosition();

				Vector vDelta = vecOtherPlayerEyePosition - vecHudPlayerEyePosition;
				float flDistance = vDelta.Length();
				// get our spectator target
				int nTargetSpec = g_bEngineIsHLTV ? HLTVCamera()->GetCurrentOrLastTarget() : ( pLocalPlayer->GetObserverTarget() ? pLocalPlayer->GetObserverTarget()->entindex() : -1 );
				// max distance to draw the names is different depending on whether we are on team spectator or not
				float flMaxDrawDist;
				if ( bShowAllNamesForSpec )
				{
					extern ConVar sv_teamid_overhead_maxdist_spec;
					flMaxDrawDist = sv_teamid_overhead_maxdist_spec.GetInt() > 0 ? sv_teamid_overhead_maxdist_spec.GetInt() : cl_teamid_overhead_maxdist_spec.GetInt();
				}
				else
				{
					extern ConVar sv_teamid_overhead_maxdist;
					flMaxDrawDist = sv_teamid_overhead_maxdist.GetInt() > 0 ? sv_teamid_overhead_maxdist.GetInt() : cl_teamid_overhead_maxdist.GetInt();
				}

				// always show the name for the player who is our target when roaming
				if ( bShowAllNamesForSpec && nTargetSpec == pOtherPlayer->entindex() && ( pLocalPlayer->GetObserverMode() == OBS_MODE_FIXED || pLocalPlayer->GetObserverMode() == OBS_MODE_ROAMING ) )
					flMaxDrawDist = 99999.9f;

				if ( flDistance <= flMaxDrawDist )
				{
					Vector vecOtherPlayerEyes = vecOtherPlayerEyePosition + Vector( 0, 0, 3 );
					// other player is close enough, now make sure the local player is facing the other player.
					Vector forward;
					AngleVectors( pCSPlayer->EyeAngles(), &forward, NULL, NULL );
					Vector toAimSpot = vecOtherPlayerEyes - vecHudPlayerEyePosition;
					float rangeToEnemy = toAimSpot.NormalizeInPlace();
					float flTargetIDCone = DotProduct( toAimSpot, forward );
					const float flViewCone = 0.5f;

					bool bPlayerIsNotVisible = false;

					// trace against world for enemies or if client doesn't want team id everywhere+
					{
						// if we can't trace to the player or if they are obscured by smoke, skip them
						CBasePlayer *lastPlayerHit = NULL;
						trace_t tr;
						if ( pCSPlayer->IsAlive() && ( flTargetIDCone > flViewCone ) && !bShowAllNamesForSpec )
						{
							if ( LineGoesThroughSmoke( vecHudPlayerEyePosition, vecOtherPlayerEyes, 1.0f ) )
							{
								if ( !ShouldShowAllFriendlyTargetIDs() || pCSPlayer->IsOtherEnemy( pOtherPlayer ) )
								{
									continue;
								}
								else
								{
									bPlayerIsNotVisible = true;
								}

							}

							UTIL_TraceLine( vecHudPlayerEyePosition, vecOtherPlayerEyes, MASK_VISIBLE, pCSPlayer, COLLISION_GROUP_DEBRIS, &tr );
							{
								CTraceFilterSkipTwoEntities filter( pCSPlayer, lastPlayerHit, COLLISION_GROUP_DEBRIS );

								// Check for player hitboxes extending outside their collision bounds
								const float rayExtension = 40.0f;
								UTIL_ClipTraceToPlayers( vecHudPlayerEyePosition, vecOtherPlayerEyes + forward * rayExtension, MASK_VISIBLE, &filter, &tr );
								if ( tr.fraction < 1 )
								{
									if ( !ShouldShowAllFriendlyTargetIDs() || pCSPlayer->IsOtherEnemy( pOtherPlayer ) )
									{
										continue;
									}
									else
									{
										bPlayerIsNotVisible = true;
									}
								}
							}
						}
					}

					// aiming tolerance depends on how close the target is - closer targets subtend larger angles
					float aimTolerance = 0.6f; //(float)cos( atan( 256 / rangeToEnemy ) );

					int nIndex = 0;
					bool bExists = false;

					// let's do this!
					if ( bShowAllNamesForSpec || flTargetIDCone > flViewCone )
					{
						FOR_EACH_VEC( m_playerIDs, j )
						{
							if ( m_playerIDs[j].m_hPlayer.Get() == pOtherPlayer )
							{
								nIndex = j;
								bExists = true;
								m_playerIDs[j].m_bActive = true;
								break;
							}
						}

						if ( !bExists )
						{
							if ( AddPlayerID( pOtherPlayer ) )
							{
								nIndex = m_playerIDs.Count() - 1;
								bExists = true;
							}
							else
							{
								nIndex = -1;
							}
						}

						if ( m_playerIDs.IsValidIndex( nIndex ) )
						{
							// update the team number

							m_playerIDs[nIndex].m_nTeam = pOtherPlayer->GetTeamNumber();
							
							// Update name + health/money

							int nIDIndex = pCSPlayer->GetIDTarget();
							//Msg( "ID Index = %d\n", nIDIndex );
							C_BaseEntity *pIDEnt = cl_entitylist->GetEnt( nIDIndex );
							bool bShowIDName = CSGameRules()->IsFreezePeriod() || bShowAllNamesForSpec || ( ( pIDEnt == pOtherPlayer || !pCSPlayer->IsAlive() )  && !bIsEnemy );

							m_playerIDs[nIndex].UpdateName( bShowIDName, ShouldShowAllFriendlyEquipment() );

							// Update equipped weapon

							m_playerIDs[nIndex].UpdateWeapons( ShouldShowAllFriendlyEquipment() );

							// Update icons

							m_playerIDs[nIndex].m_bVoiceActive = !bPlayerIsNotVisible && ( pOtherPlayer->IsPlayerTalkingOverVOIP() && ( !bIsEnemy || sv_show_voip_indicator_for_enemies.GetBool() ) && voice_icons_method.GetInt() == 2 );
							m_playerIDs[nIndex].m_bIsDefusing = !bPlayerIsNotVisible && pOtherPlayer->m_bIsDefusing;

							// Update scale and opacity

							float flOpacity = clamp( 1 - ( rangeToEnemy / ( flMaxDrawDist*0.75 ) ), 0.25, 1 );
							const float flFadeRangeClose = 160;
							if ( rangeToEnemy < flFadeRangeClose && !bShowIDName )
							{
								flOpacity *= MAX( 0.25, ( rangeToEnemy - ( flFadeRangeClose*0.35 ) ) / ( flFadeRangeClose*0.55 ) );
							}
							float flFadeEdge = ( 1 - aimTolerance )*0.4;
							if ( flTargetIDCone < ( aimTolerance + flFadeEdge ) && !bShowAllNamesForSpec )
							{
								float flFadeEdgeCur = flTargetIDCone - aimTolerance;
								flOpacity *= MAX( 0.3, flFadeEdgeCur / flFadeEdge );
							}

							float flScale = clamp( (float)atan( 512 / (rangeToEnemy*1.0) ), 0.6, 2 );
							// adjust scale by the screen size
							// (Scaleform) flScale *= clamp( 1 - ( 600.0f / (float)ScreenHeight() ), 0.25, 1 );
							flScale *= 0.7f;

							if ( input->CAM_IsThirdPersonOverview() )
							{
								flScale = 0.8f;
								flOpacity = 1.0f;
							}

							m_playerIDs[nIndex].m_flDesiredOpacity = flOpacity;
							m_playerIDs[nIndex].m_flDesiredScale = flScale;
							
							// Get world space position

							// put the indicator right over their head
							bool bHaveHeadBone = true;
							// Make sure he's all the way on screen so his bone is correct
							int nBoneIndex = -1;
							{
								nBoneIndex = pOtherPlayer->LookupBone( "ValveBiped.Bip01_Head" );
							}

							if ( ( nBoneIndex == -1 ) || !pOtherPlayer->GetBaseAnimating()->isBoneAvailableForRead( nBoneIndex ) )
							{
								bHaveHeadBone = false;
							}

							if ( bHaveHeadBone )
							{
								QAngle angBone;
								pOtherPlayer->GetBonePosition( nBoneIndex, m_playerIDs[nIndex].m_vecWorldPos, angBone );
							}
							else
							{
								m_playerIDs[nIndex].m_vecWorldPos = ( vecOtherPlayerEyePosition - pOtherPlayer->GetAbsOrigin() ) + Vector( 0.0f, 0.0f, 20 );
							}
						}
					}
				}
			}
		}
	}

	// Remove inactive players
	// Iterating backwards as we are going to delete elements from m_playerIDs vector in the loop
	FOR_EACH_VEC_BACK( m_playerIDs, k )
	{
		if ( !m_playerIDs[k].m_bActive )
		{
			RemovePlayerID( k );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudReticle::UpdatePlayerIDsUI()
{
	static const panorama::CPanoramaSymbol k_symWeaponIconHidden( "playerid__weaponicon--hidden" );
	static const panorama::CPanoramaSymbol k_symPlayeridIconHidden( "playerid__icon--hidden" );
	static const panorama::CPanoramaSymbol k_symPlayeridTeamCT( "playerid--team-ct" );
	static const panorama::CPanoramaSymbol k_symPlayeridTeamT( "playerid--team-t" );
	static const panorama::CPanoramaSymbol k_symPlayeridTeamSurvival( "playerid--team-survival" );
	static const panorama::CPanoramaSymbol k_symPlayeridNameHidden( "playerid__name--hidden" );
	static const panorama::CPanoramaSymbol k_symPlayeridWeaponsHidden( "playerid__weapons--hidden" );
	
	C_CSPlayer *pCSPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pCSPlayer )
		return;

	bool bIsPlayingSurvival = CSGameRules() && CSGameRules()->IsPlayingSurvival();

	FOR_EACH_VEC( m_playerIDs, i )
	{
		PlayerIDRenderData_t &renderData = m_playerIDs[i];
		PlayerIDSnippet_t &snippet = m_playerIDSnippets[i];

		// Team - Adding class "playerid--team-ct" / "playerid--team-t" to the root panel
		// (Used to specify indicator color)

		bool bShowPlayerColors = false;
		CBaseEntity *pCurPlayer = renderData.m_hPlayer.Get();
		C_CS_PlayerResource *cs_PR = dynamic_cast<C_CS_PlayerResource *>( g_PR );
		Color playerColor;

		if ( cs_PR && pCSPlayer->ShouldShowTeamPlayerColors( renderData.m_nTeam ) )
		{
			bShowPlayerColors = true;
			int nColorID = cs_PR->GetCompTeammateColor( pCurPlayer->entindex() );
			playerColor = cs_PR->GetCompPlayerColorByID( nColorID );
		}
		else if ( bIsPlayingSurvival )
		{
			// When spectating survival teams, show team colors instead of generic color
			if ( pCSPlayer->GetTeamNumber() == TEAM_SPECTATOR || pCSPlayer->IsHLTV() || ( g_HltvReplaySystem.IsInPermanentReplay() && !g_HltvReplaySystem.GetHltvReplayDelay() ) )
			{
				C_CSPlayer* pCurCSPlayer = ToCSPlayer( pCurPlayer );
				if ( pCurCSPlayer && pCurCSPlayer->m_nSurvivalTeam >= 0 )
				{
					bShowPlayerColors = true;
					//playerColor = CSGameRules()->GetSurvivalRules()->GetTeamColor( pCurCSPlayer->m_nSurvivalTeam );
					playerColor = CSGameRules()->GetSurvivalRules()->GetTeamColorByUserID( pCurCSPlayer->GetUserID() );
				}
			}
		}

		snippet.m_pPlayerIDPanel->SetHasClass( k_symPlayeridTeamCT, bShowPlayerColors == false && renderData.m_nTeam == TEAM_CT && !bIsPlayingSurvival );
		snippet.m_pPlayerIDPanel->SetHasClass( k_symPlayeridTeamT, bShowPlayerColors == false && renderData.m_nTeam == TEAM_TERRORIST && !bIsPlayingSurvival );
		snippet.m_pPlayerIDPanel->SetHasClass( k_symPlayeridTeamSurvival, bShowPlayerColors == false && bIsPlayingSurvival );

		// for competitive, we show the individual player color (or very light blue for bots)
		if ( bShowPlayerColors )
		{
			panorama::IUIPanelStyle *pPanelStyle;
			pPanelStyle = snippet.m_pArrowIconPanel->AccessStyle();
			pPanelStyle->SetSimpleWashColor( playerColor, true );
			pPanelStyle = snippet.m_pArrowIconBorderPanel->AccessStyle();
			pPanelStyle->SetSimpleWashColor( playerColor, true );
			pPanelStyle = snippet.m_pChatIconPanel->AccessStyle();
			pPanelStyle->SetSimpleWashColor( playerColor, true );
		}
		else if ( renderData.m_bVoiceActive )
		{
			panorama::IUIPanelStyle *pPanelStyle;
			pPanelStyle = snippet.m_pChatIconPanel->AccessStyle();
			pPanelStyle->SetSimpleWashColor( Color( 220, 220, 220, 255 ), true );
		}

		// Weapons

		snippet.m_pWeaponsPanel->SetHasClass( k_symPlayeridWeaponsHidden, !renderData.m_bWeaponsPanelVisible );
		if ( renderData.m_bWeaponsPanelVisible )
		{			
			if ( renderData.m_bSetPrimaryWeaponName && !renderData.m_primaryWeaponName.IsEmpty() )
			{
				renderData.m_bPrimaryVisible = true;
				snippet.m_pWeaponPrimaryPanel->SetImageJS( CFmtStr( "file://{images}/icons/equipment/%s.svg", renderData.m_primaryWeaponName.String() ) );
			}
			else
			{
				renderData.m_bPrimaryVisible = false;
				snippet.m_pWeaponPrimaryPanel->SetImageJS( "" );
			}
			
			snippet.m_pWeaponDefuserPanel->SetHasClass( k_symWeaponIconHidden, !renderData.m_bDefuserVisible );
			snippet.m_pWeaponC4Panel->SetHasClass( k_symWeaponIconHidden, !renderData.m_bC4Visible );
			snippet.m_pWeaponPrimaryPanel->SetHasClass( k_symWeaponIconHidden, !renderData.m_bPrimaryVisible );

			int nMaxGrenades = Min<int>( ARRAYSIZE( renderData.m_grenadeWeaponNames ), snippet.m_pWeaponGrenadesPanel->GetChildCount() );
			for ( int nGrenadeIndex = 0; nGrenadeIndex < nMaxGrenades; ++nGrenadeIndex )
			{
				panorama::CImagePanel *pGrenadePanel = panorama::panel_cast< panorama::CImagePanel * >( snippet.m_pWeaponGrenadesPanel->GetChild( nGrenadeIndex ) );
				if ( pGrenadePanel )
				{
					pGrenadePanel->SetHasClass( k_symWeaponIconHidden, nGrenadeIndex >= renderData.m_nGrenadesVisible );
					if ( renderData.m_bSetGrenadeWeaponName && !renderData.m_grenadeWeaponNames[nGrenadeIndex].IsEmpty() )
					{
						pGrenadePanel->SetImageJS( CFmtStr( "file://{images}/icons/equipment/%s.svg", renderData.m_grenadeWeaponNames[nGrenadeIndex].String() ) );
					}
				}
			}
			renderData.m_bSetGrenadeWeaponName = false;
		}
		
		// Name label

		snippet.m_pNamePanel->SetHasClass( k_symPlayeridNameHidden, !renderData.m_bNamePanelVisible );
		snippet.m_pNamePanelBG->SetHasClass( k_symPlayeridNameHidden, !renderData.m_bNamePanelVisible );
		if ( renderData.m_bNamePanelVisible )
		{
			if ( renderData.m_bSetName && renderData.m_pszName )
			{
				snippet.m_pNamePanel->SetText( renderData.m_pszName );		
				snippet.m_pNamePanelBG->SetText( "#Panorama_HUD_playerid_overhead_shadow" );	
				
				renderData.m_bSetName = false;
			}

			// Set player_name dialog variable
			CBaseEntity *pPlayer = renderData.m_hPlayer.Get();
			if ( pPlayer && cs_PR )
			{
				wchar_t wszPlayerName[MAX_DECORATED_PLAYER_NAME_LENGTH];
				cs_PR->GetDecoratedPlayerName( pPlayer->entindex(), wszPlayerName, sizeof( wszPlayerName ), k_EDecoratedPlayerNameFlag_AddBotToNameIfControllingBot );
				//V_swprintf_safe( wszPlayerName, PRI_S_FOR_WS L" " PRI_S_FOR_WS, "VLV", "GautamGautam GAutamGautamGAutamA" );

				TruncatePlayerName( wszPlayerName, ARRAYSIZE( wszPlayerName ), 18 );

				//Convert to UTF-8 (Each Unicode code point can expand to as many as four bytes in UTF-8)
				char szPlayerNameUTF8[4 * MAX_DECORATED_PLAYER_NAME_LENGTH];
				V_UnicodeToUTF8( wszPlayerName, szPlayerNameUTF8, ARRAYSIZE( szPlayerNameUTF8 ) );
				snippet.m_pNamePanel->SetDialogVariable( "player_name", szPlayerNameUTF8 );
				snippet.m_pNamePanelBG->SetDialogVariable( "player_name", szPlayerNameUTF8 );
			}
			else
			{
				snippet.m_pNamePanel->SetDialogVariable( "player_name", "" );
				snippet.m_pNamePanelBG->SetDialogVariable( "player_name", "" );
			}

			// Set "player_money" & "player_health" dialog variables
			if ( renderData.m_nMoney != -1 )
			{
				snippet.m_pNamePanel->SetDialogVariable( "player_money", renderData.m_nMoney );
			}
			if ( renderData.m_nHealth != -1 )
			{
				snippet.m_pNamePanel->SetDialogVariable( "player_health", renderData.m_nHealth );
			}

		}

		// Icons

		snippet.m_pArrowIconPanel->SetHasClass(
			k_symPlayeridIconHidden,
			!( !renderData.m_bVoiceActive && !renderData.m_bIsDefusing ) );
		snippet.m_pArrowIconBorderPanel->SetHasClass(
			k_symPlayeridIconHidden,
			!( !renderData.m_bVoiceActive && !renderData.m_bIsDefusing ) );		
		snippet.m_pArrowFriendIconPanel->SetHasClass( 
			k_symPlayeridIconHidden, 
			!( !renderData.m_bVoiceActive && renderData.m_bIsFriend && !renderData.m_bIsDefusing ) );
		snippet.m_pChatIconPanel->SetHasClass(
			k_symPlayeridIconHidden,
			!( renderData.m_bVoiceActive ) );
		snippet.m_pDefusingIconPanel->SetHasClass(
			k_symPlayeridIconHidden,
			!( !renderData.m_bVoiceActive && renderData.m_bIsDefusing ) );

		// Apply scale & opacity
		// Applying opacity to the "content" panel instead of the root panel
		// in order to avoid the root panel becoming a compositing layer 
		// (root panel is mostly empty)`

		if ( !CloseEnough( renderData.m_flDesiredOpacity, renderData.m_flOpacity ) )
		{
			snippet.m_pContentPanel->SetOpacitySimple( renderData.m_flDesiredOpacity );
			snippet.m_pContentPanelTrans->SetOpacitySimple( renderData.m_flDesiredOpacity );
			if ( renderData.m_flOpacity < EQUAL_EPSILON )
			{
				// Disable size and position invalidation on opacity changed (when creating the snippet)
				// It is therefore our responsibility to invalidate size and position when the opacity
				// is changing from 0 to any other value.

				// Matt - needed on both children instead of parent since we've split the hierarchy to deal with additive/normal blending on these children
				snippet.m_pContentPanel->InvalidateSizeAndPosition();
				snippet.m_pContentPanelTrans->InvalidateSizeAndPosition();
			}
			renderData.m_flOpacity = renderData.m_flDesiredOpacity;
		}

		// Matt - transform needed on both children instead of parent since we've split the hierarchy to deal with additive/normal blending difference betweem the siblings
		// if we set the transform on the parent, we'd introduce a composition layer at that level and thus break the additive/normal difference between the siblings
 		CUtlVector<panorama::CTransform3D *> vecTransforms;
 		vecTransforms.AddToTail( new panorama::CTransformScale3D( renderData.m_flDesiredScale, renderData.m_flDesiredScale, 1.0f ) );
 		snippet.m_pContentPanel->SetTransform3DSimple( vecTransforms );
		vecTransforms.RemoveAll();
		vecTransforms.AddToTail( new panorama::CTransformScale3D( renderData.m_flDesiredScale, renderData.m_flDesiredScale, 1.0f ) );
		snippet.m_pContentPanelTrans->SetTransform3DSimple( vecTransforms );

		// Apply transform to scale and translate player's panel over its head

		int nScreenspaceX, nScreenspaceY;
		GetVectorInScreenSpace( renderData.m_vecWorldPos + Vector( 0, 0, 9 ), nScreenspaceX, nScreenspaceY );

		// Convert to panorama space (ie 1920x1080)
		float flScaleFactor = 1080.0f / (float)ScreenHeight();
		float flPanoramaX, flPanoramaY;
		flPanoramaX = nScreenspaceX * flScaleFactor;
		flPanoramaY = nScreenspaceY * flScaleFactor;

		vecTransforms.RemoveAll();
		vecTransforms.AddToTail( new panorama::CTransformTranslate3D( flPanoramaX, flPanoramaY, 0.0f ) );
		//vecTransforms.AddToTail( new panorama::CTransformTranslate3D( 0.0f, -30.0f, 0.0f ) );	// Uncomment to have panorama / scaleform side by side
		snippet.m_pPlayerIDPanel->SetTransform3DSimple( vecTransforms );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCSGO_HudReticle::AddPlayerID( CBaseEntity *pPlayer )
{
	if ( !pPlayer )
	{
		return false;
	}

	PlayerIDRenderData_t renderData;
	renderData.m_hPlayer = pPlayer;
	renderData.m_nTeam = TEAM_UNASSIGNED;
	renderData.m_bActive = true;
	renderData.m_bNamePanelVisible = false;
	renderData.m_bSetName = false;
	renderData.m_bWeaponsPanelVisible = false;
	renderData.m_bPrimaryVisible = false;
	renderData.m_bDefuserVisible = false;
	renderData.m_bC4Visible = false;
	renderData.m_bIsFriend = false;
	renderData.m_bVoiceActive = false;
	renderData.m_bIsDefusing = false;
	renderData.m_nGrenadesVisible = 0;
	renderData.m_pszName = nullptr;
	renderData.m_nHealth = -1;
	renderData.m_nMoney = -1;
	renderData.m_primaryWeaponName = "";
	renderData.m_bSetPrimaryWeaponName = false;
	renderData.m_bSetGrenadeWeaponName = false;
	for ( unsigned int nGren = 0; nGren < ARRAYSIZE( renderData.m_grenadeWeaponNames ); ++nGren )
	{
		renderData.m_grenadeWeaponNames[nGren] = "";
	}
	renderData.m_flDesiredOpacity = 1.0f;
	renderData.m_flOpacity = -1.0f;
	renderData.m_flDesiredScale = 1.0f;

	C_CS_PlayerResource *cs_PR = dynamic_cast<C_CS_PlayerResource *>( g_PR );
	if ( cs_PR )
	{
		XUID nOtherXUID = cs_PR->GetXuid( pPlayer->entindex() );
		if ( g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetFriendByXUID( nOtherXUID ) )
		{
			renderData.m_bIsFriend = true;
		}
	}

	// Initialize panorama panels
	PlayerIDSnippet_t snippet = {};
	// Get panel from m_pUnusedPlayerIDsPanel if possible
	if ( m_pUnusedPlayerIDsPanel->GetChildCount() )
	{
		snippet.m_pPlayerIDPanel = m_pUnusedPlayerIDsPanel->GetLastChild();
		snippet.m_pPlayerIDPanel->SetParent( m_pVisiblePlayerIDsPanel );
	}
	else
	{
		snippet.m_pPlayerIDPanel = new panorama::CPanel2D( m_pVisiblePlayerIDsPanel, nullptr );
		snippet.m_pPlayerIDPanel->RequireLoadLayoutSnippet( "PlayerID" );
	}

	snippet.m_pContentPanelParent = snippet.m_pPlayerIDPanel->RequireChildInLayoutFile( "ContentParent" );
	snippet.m_pContentPanel = snippet.m_pPlayerIDPanel->RequireChildInLayoutFile( "Content" );
	snippet.m_pContentPanelTrans = snippet.m_pPlayerIDPanel->RequireChildInLayoutFile( "ContentTrans" );
	snippet.m_pNamePanel = panorama::panel_cast< panorama::CLabel * >( snippet.m_pPlayerIDPanel->RequireChildInLayoutFile( "Name" ) );
	snippet.m_pNamePanelBG = panorama::panel_cast< panorama::CLabel * >( snippet.m_pPlayerIDPanel->RequireChildInLayoutFile( "NameBG" ) );
	snippet.m_pWeaponsPanel = snippet.m_pPlayerIDPanel->RequireChildInLayoutFile( "Weapons" );
	snippet.m_pWeaponDefuserPanel = panorama::panel_cast< panorama::CImagePanel * >( snippet.m_pPlayerIDPanel->RequireChildInLayoutFile( "WeaponDefuser" ) );
	snippet.m_pWeaponC4Panel = panorama::panel_cast< panorama::CImagePanel * >( snippet.m_pPlayerIDPanel->RequireChildInLayoutFile( "WeaponC4" ) );
	snippet.m_pWeaponGrenadesPanel = panorama::panel_cast< panorama::CPanel2D * >( snippet.m_pPlayerIDPanel->RequireChildInLayoutFile( "WeaponGrenades" ) );
	snippet.m_pWeaponPrimaryPanel = panorama::panel_cast< panorama::CImagePanel * >( snippet.m_pPlayerIDPanel->RequireChildInLayoutFile( "WeaponPrimary" ) );
	snippet.m_pArrowFriendIconPanel = snippet.m_pPlayerIDPanel->RequireChildInLayoutFile( "ArrowFriendIcon" );
	snippet.m_pArrowIconPanel = snippet.m_pPlayerIDPanel->RequireChildInLayoutFile( "ArrowIcon" );
	snippet.m_pArrowIconBorderPanel = snippet.m_pPlayerIDPanel->RequireChildInLayoutFile( "ArrowIconBorder" );
	snippet.m_pChatIconPanel = snippet.m_pPlayerIDPanel->RequireChildInLayoutFile( "ChatIcon" );
	snippet.m_pDefusingIconPanel = snippet.m_pPlayerIDPanel->RequireChildInLayoutFile( "DefusingIcon" );

	snippet.m_pContentPanel->UIPanel()->SetInvalidateSizeAndPositionOnOpacityChangeDisabled( true );
	snippet.m_pContentPanelTrans->UIPanel()->SetInvalidateSizeAndPositionOnOpacityChangeDisabled( true );

	renderData.m_nMaxGrenades = Min<int>( ARRAYSIZE( renderData.m_grenadeWeaponNames ), snippet.m_pWeaponGrenadesPanel->GetChildCount() );

	m_playerIDs.AddToTail( renderData );
	m_playerIDSnippets.AddToTail( snippet );
	
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudReticle::RemovePlayerID( int nIndex )
{
	if ( !m_playerIDs.IsValidIndex( nIndex ) || !m_playerIDSnippets.IsValidIndex( nIndex ) )
	{
		return;
	}
	
	// Add panel to m_pUnusedPlayerIDsPanel (to be reused later in AddPlayerID)
	m_playerIDSnippets[nIndex].m_pPlayerIDPanel->SetParent( m_pUnusedPlayerIDsPanel );
	
	m_playerIDs.FastRemove( nIndex );
	m_playerIDSnippets.FastRemove( nIndex );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudReticle::RemoveAllPlayerID()
{
	// Iterating backwards as we are going to delete elements from m_playerIDs vector in the loop
	FOR_EACH_VEC_BACK( m_playerIDs, k )
	{
		RemovePlayerID( k );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCSGO_HudReticle::ShouldShowAllFriendlyTargetIDs( void )
{
	extern ConVar sv_teamid_overhead_always_prohibit;
	if ( sv_teamid_overhead_always_prohibit.GetBool() )
		return false;

	extern ConVar sv_show_team_equipment_force_on;
	if ( sv_show_team_equipment_force_on.GetBool() )
		return true;

	return ( m_bForceShowAllTeammateTargetIDs || cl_teamid_overhead_always.GetInt() >= 1 );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCSGO_HudReticle::ShouldShowAllFriendlyEquipment( void )
{
	extern ConVar sv_show_team_equipment_prohibit;
	if ( sv_show_team_equipment_prohibit.GetBool() )
		return false;

	extern ConVar sv_show_team_equipment_force_on;
	if ( sv_show_team_equipment_force_on.GetBool() )
		return true;

	return ( m_bForceShowAllTeammateTargetIDs || cl_teamid_overhead_always.GetInt() == 2 );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudReticle::FireGameEvent( IGameEvent *event )
{
	const char *type = event->GetName();

	if ( !V_strcmp( type, "round_start" ) )
	{
		// Force hide the "targetID" panel by resetting the timer
		m_flTargetIDTimer = 0.0f;
	}
}
