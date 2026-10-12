//========= Copyright © Valve Corporation, All rights reserved. ============//
//
//=====================================================================================//

#include "cbase.h"
#include "csgo_compass.h"
#include "IGameUIFuncs.h"
#include "panorama/ui_root.h"
#include "panorama/uijsregistration.h"
#include "c_cs_player.h"
#include "c_cs_playerresource.h"
#include "clientsteamcontext.h"
#include "cs_gamerules_survival.h"
#include "hudelement.h"
#include "panorama/controls/movieplayer.h"
#include "view.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace panorama;

REGISTER_PANEL2D_FACTORY( CCSGO_Compass, CSGOCompass );

DEFINE_PANORAMA_EVENT_DOC( ShowCompass, "bool", "Show or hide the panel." );
DEFINE_PANORAMA_EVENT_DOC( CompassUpdate, "", "Update state from survival gamerules" );

extern ConVar cl_draw_only_deathnotices;
extern ConVar cl_drawhud;

ConVar cl_compass_enabled( "cl_compass_enabled", "1", FCVAR_RELEASE | FCVAR_ARCHIVE, "" );

struct CompassHeadingCardinal_t
{
	int nHeadingDegree;
	const char *pszCardinal;
};

CompassHeadingCardinal_t s_compassHeadingCardinals[] =
{
	{ 0, "N" },
	{ 45, "NE" },
	{ 90, "E" },
	{ 135, "SE" },
	{ 180, "S" },
	{ 225, "SW" },
	{ 270, "W" },
	{ 315, "NW" },
};

CCSGO_Compass::CCSGO_Compass( panorama::CPanel2D *pParent, const char *pchID )
	: CPanoramaHudElement( "CCSGO_Compass", this )
	, panorama::CPanel2D( pParent, pchID )
{
	RequireLoadLayout( "file://{resources}/layout/compass.xml" );

	m_bWantLateUpdate = true;
	SetForceBuildPaintCmdCache( false );
	SetAllowAlternateTicks( false );

	m_pRegionName = panel_cast< CLabel* > ( FindChildInLayoutFile( "RegionLabel" ) );

	m_flLastTeammateUpdateTime = 0;
	m_bLasKnownVisibleState = false;
	m_hLastLocalPlayer = INVALID_EHANDLE;
	UpdateCompass( true );
}

C_CSPlayer* CCSGO_Compass::GetCompassPlayer( void )
{
	C_CSPlayer *pLocalCSPlayer = ToCSPlayer( C_BasePlayer::GetLocalPlayer() );

	if ( pLocalCSPlayer && pLocalCSPlayer->GetObserverMode() == OBS_MODE_IN_EYE ) // but not chase camera
	{
		C_BaseEntity *target = pLocalCSPlayer->GetObserverTarget();
		if ( target && target->IsPlayer() )
			return ToCSPlayer( target );
	}

	return pLocalCSPlayer;
}

float CCSGO_Compass::WorldPosToCompassPercentage( const Vector &worldPos )
{
	float flScreenWidth = GetParentWindow()->GetWindowWidth();
	float flCompassRenderWidth = GetActualRenderWidth();
	float flMargin = (flScreenWidth - flCompassRenderWidth) * 0.5f;

	Vector vecScreenPos;
	ScreenTransform( worldPos, vecScreenPos );
	ConvertNormalizedScreenSpaceToPixelScreenSpace( vecScreenPos );

	return RemapValClamped( vecScreenPos.x, flMargin, flMargin + flCompassRenderWidth, 0.0f, 100.0f );
}

float CCSGO_Compass::CompassPosToOpacity( float flPercentage, float flSoften /* = 30.0f */ )
{
	if ( flPercentage <= 0 || flPercentage >= 100 )
		return 0.0f;

	return MIN( RemapValClamped( flPercentage, 0.0f, flSoften, 0.0f, 1.0f ), RemapValClamped( flPercentage, 100.0f - flSoften, 100.0f, 1.0f, 0.0f ) );
}

bool CCSGO_Compass::SetRegionDialogVars( const Vector& vecPos )
{
	C_InfoMapRegion *pClosestRegionByDistance = nullptr;
	C_InfoMapRegion *pClosestRegionByWeight = nullptr;
	float flClosestDist = MAX_COORD_FLOAT;
	float flClosestDisanceAsRadiiRatio = MAX_COORD_FLOAT;

	// Walk the list of regions, find the closest by distance and by ratio 
	// of radius so larger regions are weighted more heavily. 
	for ( C_InfoMapRegion* pEnt = GetRegionNameList(); pEnt != nullptr; pEnt = pEnt->m_pNext )
	{
		float flDistSqr = vecPos.DistToSqr( pEnt->GetAbsOrigin() );
		float flNumRadiiSqr = flDistSqr / Max( ( pEnt->m_flRadius * pEnt->m_flRadius ), 1.f );

		if ( !pClosestRegionByWeight || flNumRadiiSqr < flClosestDisanceAsRadiiRatio )
		{
			pClosestRegionByWeight = pEnt;
			flClosestDisanceAsRadiiRatio = flNumRadiiSqr;
		}
		if ( !pClosestRegionByDistance || flDistSqr < flClosestDist )
		{
			pClosestRegionByDistance = pEnt;
			flClosestDist = flDistSqr;
		}

	}

	// Guard against maps with no regions... If there is at least one in the map it should be pointed to by both here
	if ( !pClosestRegionByDistance || !pClosestRegionByWeight )
		return false;

	bool bChangedLabel = false;
	if ( flClosestDist < pClosestRegionByDistance->m_flRadius*pClosestRegionByDistance->m_flRadius )
	{
		// If strictly inside a region, use that name always
		if ( m_pRegionName.Get() )
			m_pRegionName->SetText( "#Compass_RegionName" );

		if ( pClosestRegionByDistance != m_pPreviousMapRegion.Get() || m_nPreviousDirectionIdx != -1 )
			bChangedLabel = true;
		m_pPreviousMapRegion = pClosestRegionByDistance;
		m_nPreviousDirectionIdx = -1;
		SetDialogVariableLocString( "regionname", pClosestRegionByDistance->m_szLocToken );
	}
	else
	{
		// Otherwise, pick a region we are closest by radius weight and show direction to it
		if ( m_pRegionName.Get() )
			m_pRegionName->SetText( "#Compass_RegionName_With_Direction" );

		static const char* s_szDirectionLocToken[] = {
			"Direction_East",
			"Direction_Northeast",
			"Direction_North",
			"Direction_Northwest",
			"Direction_West",
			"Direction_Southwest",
			"Direction_South",
			"Direction_Southeast",
		};
	
		const float flDegreesPerDirection = 45;
		COMPILE_TIME_ASSERT( ARRAYSIZE( s_szDirectionLocToken ) * flDegreesPerDirection == 360 );

		Vector vDir = vecPos - pClosestRegionByWeight->GetAbsOrigin();
		vDir.z = 0;
		vDir.NormalizeInPlace();
		float flHeading = UTIL_VecToYaw( vDir );
		int idx = ( ( int )( flHeading + 22 ) % 360 ) / flDegreesPerDirection;
		SetDialogVariableLocString( "direction", s_szDirectionLocToken[ idx ] );

		if ( pClosestRegionByWeight != m_pPreviousMapRegion.Get() || idx != m_nPreviousDirectionIdx )
			bChangedLabel = true;
		m_pPreviousMapRegion = pClosestRegionByWeight;

		m_nPreviousDirectionIdx = idx;
		SetDialogVariableLocString( "regionname", pClosestRegionByWeight->m_szLocToken );
	}

	if ( bChangedLabel )
		 TriggerClass( "show-region" );

	return true;
}

void CCSGO_Compass::UpdateTeammatePips( void )
{
	if ( !CSGameRules() || !CSGameRules()->GetSurvivalRules() || CSGameRules()->GetSurvivalRules()->IsPlayingSoloMode() )
	{
		DestroyTeammatePips();
		return;
	}

	CCSPlayer* pHudPlayer = GetCompassPlayer();
	if ( !pHudPlayer )
	{
		DestroyTeammatePips();
		return;
	}

	if ( m_hLastLocalPlayer != pHudPlayer )
	{
		// we changed our observed player. always re-create teammate pips in this case.
		CreateTeammatePips();

		m_hLastLocalPlayer = pHudPlayer;
		return;
	}


	if ( gpGlobals->curtime - m_flLastTeammateUpdateTime < 0.1f )
		return;
	m_flLastTeammateUpdateTime = gpGlobals->curtime;


	// check if our existing pips still account for our alive teammates
	FOR_EACH_VEC( m_TeammatePips, n )
	{
		C_CSPlayer *pTeammate = ToCSPlayer( m_TeammatePips[n].m_hTeammateHandle.Get() );
		if ( !pTeammate || !pTeammate->IsAlive() || pHudPlayer->m_nSurvivalTeam != pTeammate->m_nSurvivalTeam || pTeammate == pHudPlayer )
		{
			CreateTeammatePips();
			return;
		}
	}

	// finally, check if we still have the same number of pips as alive teammates

	// by counting the number of teammates we SHOULD have
	int nNumAliveTeammates = 0;
	if ( pHudPlayer->m_nSurvivalTeam >= 0 )
	{
		for ( int i = 1; i <= gpGlobals->maxClients; ++i )
		{
			CCSPlayer* pOtherPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );
			if ( pOtherPlayer && pOtherPlayer->IsAlive() && pOtherPlayer != pHudPlayer && pHudPlayer->m_nSurvivalTeam == pOtherPlayer->m_nSurvivalTeam )
			{
				nNumAliveTeammates++;
			}
		}
	}

	// and recreating the teammate pips if the counts don't match
	if ( nNumAliveTeammates != m_TeammatePips.Count() )
	{
		CreateTeammatePips();
		return;
	}
}

void CCSGO_Compass::CreateTeammatePips( void )
{
	DestroyTeammatePips();

	CCSPlayer* pHudPlayer = GetCompassPlayer();
	if ( !pHudPlayer || pHudPlayer->m_nSurvivalTeam < 0 )
		return;

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CCSPlayer* pOtherPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );
		if ( pOtherPlayer && pOtherPlayer->IsAlive() && pOtherPlayer != pHudPlayer && pHudPlayer->m_nSurvivalTeam == pOtherPlayer->m_nSurvivalTeam )
		{
			CPanel2D* pPipPanel = new CPanel2D( this, nullptr );
			pPipPanel->RequireLoadLayoutSnippet( "compass-teammate-pip" );

			compassTeammatePip pip;
			pip.m_pPanel = pPipPanel;
			pip.m_hTeammateHandle = pOtherPlayer;

			m_TeammatePips.AddToTail( pip );
		}
	}
}

void CCSGO_Compass::UpdateCompass( bool bVisible )
{
	CCSPlayer* pHudPlayer = NULL;

	bVisible = bVisible && ShouldDraw();

	if ( bVisible )
		pHudPlayer = GetCompassPlayer();

	if ( !pHudPlayer )
		bVisible = false;

	if ( bVisible != m_bLasKnownVisibleState )
	{
		// create or destroy pips if visibility is changing.
		if ( !bVisible )
		{
			DestroyPips();
		}
		else
		{
			CreatePips();
		}
		m_bLasKnownVisibleState = bVisible;
	}

	SetVisible( bVisible );

	if ( !bVisible )
		return;

	Vector vecCameraOrigin = MainViewOrigin( GET_ACTIVE_SPLITSCREEN_SLOT() );
	Vector vecCameraForward = MainViewForward( GET_ACTIVE_SPLITSCREEN_SLOT() );

	UpdateTeammatePips();
	
	bool bShowRegion = SetRegionDialogVars( vecCameraOrigin );
	SetHasClass( "show-region", bShowRegion );

	QAngle angCameraAngle;
	VectorAngles( vecCameraForward, angCameraAngle );

	int nCurrentHeading = fmod( 360.0f - angCameraAngle[YAW] + 90.0f + 0.5f, 360.0f );
	SetDialogVariable( "compassheading", nCurrentHeading );

	int nOffsetHeadingForCardinal = ((nCurrentHeading + 22) % 360) / 45;
	if ( nOffsetHeadingForCardinal >= 0 && nOffsetHeadingForCardinal < V_ARRAYSIZE( s_compassHeadingCardinals ) )
	{
		SetDialogVariable( "compasscardinal", s_compassHeadingCardinals[nOffsetHeadingForCardinal].pszCardinal );
	}

	float flGimbalLockFadeOut = RemapValClamped( abs( DotProduct( vecCameraForward, Vector( 0, 0, 1 ) ) ), 0.75f, 0.87f, 1.0f, 0.0f );

	FOR_EACH_VEC( m_Pips, i )
	{
		float flPipPercentage = WorldPosToCompassPercentage( m_Pips[i].m_vecWorldPosOffset + vecCameraOrigin );

		CUILength lenCurrentX, lenCurrentY, lenCurrentZ;
		m_Pips[i].m_pPanel->GetPosition( lenCurrentX, lenCurrentY, lenCurrentZ );

		lenCurrentX = CUILength( flPipPercentage, CUILength::k_EUILengthPercent );
		m_Pips[i].m_pPanel->SetPosition( lenCurrentX, lenCurrentY, lenCurrentZ );
		m_Pips[i].m_pPanel->SetOpacity( CompassPosToOpacity( flPipPercentage ) * flGimbalLockFadeOut );
	}

	FOR_EACH_VEC( m_TeammatePips, i )
	{
		C_CSPlayer *pTeammate = ToCSPlayer( m_TeammatePips[i].m_hTeammateHandle.Get() );
		if ( !pTeammate )
			continue;

		float flPipPercentage = WorldPosToCompassPercentage( pTeammate->EyePosition() );
		flPipPercentage = clamp( flPipPercentage, 3.0f, 97.0f );

		CUILength lenCurrentX, lenCurrentY, lenCurrentZ;
		m_TeammatePips[i].m_pPanel->GetPosition( lenCurrentX, lenCurrentY, lenCurrentZ );

		lenCurrentX = CUILength( flPipPercentage, CUILength::k_EUILengthPercent );
		m_TeammatePips[i].m_pPanel->SetPosition( lenCurrentX, lenCurrentY, lenCurrentZ );
		m_TeammatePips[i].m_pPanel->SetOpacity( CompassPosToOpacity( flPipPercentage, 10.0f ) * flGimbalLockFadeOut );
	}
	
	FOR_EACH_VEC( m_RescuePips, i )
	{
		if ( pHudPlayer && pHudPlayer->m_hCarriedHostage )
		{
			float flPipPercentage = WorldPosToCompassPercentage( m_RescuePips[i].m_vecWorldPos );

			CUILength lenCurrentX, lenCurrentY, lenCurrentZ;
			m_RescuePips[i].m_pPanel->GetPosition( lenCurrentX, lenCurrentY, lenCurrentZ );

			lenCurrentX = CUILength( flPipPercentage, CUILength::k_EUILengthPercent );
			m_RescuePips[i].m_pPanel->SetPosition( lenCurrentX, lenCurrentY, lenCurrentZ );
			m_RescuePips[i].m_pPanel->SetOpacity( CompassPosToOpacity( flPipPercentage ) * flGimbalLockFadeOut );
		}
		else
		{
			m_RescuePips[i].m_pPanel->SetOpacity( 0 );
		}
	}

}

void CCSGO_Compass::CreatePips()
{
	DestroyPips();

	for ( int i = 0; i < 360; i += 5 )
	{
		CPanel2D* pPipPanel = new CPanel2D( this, nullptr );
		pPipPanel->RequireLoadLayoutSnippet( "compass-pip-container" );

		compassPip pip;
		pip.m_nDegreeAngle = i;
		pip.m_pPanel = pPipPanel;

		AngleVectors( QAngle( 0, 360.0f - pip.m_nDegreeAngle + 90.0f, 0 ), &pip.m_vecWorldPosOffset );
		pip.m_vecWorldPosOffset *= MAX_COORD_FLOAT * 100.0f;

		CLabel* pLabel = nullptr;
		pPipPanel->IterateChildrenTraverseOfType<panorama::CLabel>( [&]( panorama::CLabel* pChild ) { pLabel = pChild; return false; } );
		pip.m_pLabel = pLabel;
		
		const char *szCardinal = "";
		for ( int n = 0; n < V_ARRAYSIZE( s_compassHeadingCardinals ); n++ )
		{
			if ( pip.m_nDegreeAngle == s_compassHeadingCardinals[n].nHeadingDegree )
			{
				szCardinal = s_compassHeadingCardinals[n].pszCardinal;
				break;
			}
		}

		if ( szCardinal[0] == '\0' && pip.m_nDegreeAngle % 10 == 5 )
		{
			pip.m_pLabel->SetText( "" );
		}
		else if ( szCardinal[0] != '\0' )
		{
			V_sprintf_safe( pip.m_szText, "%s\n%i", szCardinal, pip.m_nDegreeAngle );
			pip.m_pLabel->SetText( pip.m_szText );
		}
		else
		{
			V_sprintf_safe( pip.m_szText, "%i", pip.m_nDegreeAngle );
			pip.m_pLabel->SetText( pip.m_szText );
		}

		m_Pips.AddToTail( pip );
	}

	// rescue pips
	C_CS_PlayerResource *pCSRerouce = GetCSResources();
	if ( pCSRerouce )
	{
		for ( int i = 0; i < MAX_HOSTAGE_RESCUES; ++i )
		{
			Vector vecRescuePos = pCSRerouce->GetHostageRescuePosition( i );
			if ( vecRescuePos == vec3_origin )
				continue;

			CPanel2D* pPipPanel = new CPanel2D( this, nullptr );
			pPipPanel->RequireLoadLayoutSnippet( "compass-rescue-pip" );

			compassRescuePip pip;
			pip.m_pPanel = pPipPanel;
			pip.m_vecWorldPos = vecRescuePos;

			m_RescuePips.AddToTail( pip );
		}
	}

}

void CCSGO_Compass::DestroyPips()
{
	FOR_EACH_VEC( m_Pips, i )
	{
		compassPip& pip = m_Pips[i];
		if ( CPanel2D* pPanel = pip.m_pPanel.Get() )
		{
			delete pPanel;
			pip.m_pPanel = nullptr;
		}
	}

	m_Pips.RemoveAll();

	FOR_EACH_VEC( m_RescuePips, i )
	{
		compassRescuePip& pip = m_RescuePips[i];
		if ( CPanel2D* pPanel = pip.m_pPanel.Get() )
		{
			delete pPanel;
			pip.m_pPanel = nullptr;
		}
	}
	m_RescuePips.RemoveAll();

	DestroyTeammatePips();
}

void CCSGO_Compass::DestroyTeammatePips()
{
	FOR_EACH_VEC( m_TeammatePips, i )
	{
		compassTeammatePip& pip = m_TeammatePips[i];
		if ( CPanel2D* pPanel = pip.m_pPanel.Get() )
		{
			delete pPanel;
			pip.m_pPanel = nullptr;
		}
	}

	m_TeammatePips.RemoveAll();
}

void CCSGO_Compass::SetActive( bool bActive )
{
	UpdateCompass( bActive );

	CPanoramaHudElement::SetActive( bActive );
}

bool CCSGO_Compass::ShouldDraw( void )
{
	if ( !CSGameRules() || !CSGameRules()->IsPlayingSurvival() )// || CSGameRules()->IsWarmupPeriod() )
	{
		return false;
	}

	CSurvivalGameRules* pBRrules = CSGameRules()->GetSurvivalRules();
	if ( pBRrules && gpGlobals->curtime < pBRrules->GetSpawnSelectTimeEnd() )
	{
		return false;
	}

	return cl_compass_enabled.GetBool() && cl_drawhud.GetBool() && cl_draw_only_deathnotices.GetBool() == false && CPanoramaHudElement::ShouldDraw();
}

void CCSGO_Compass::Think( void )
{
	UpdateCompass( ShouldDraw() );
}

void CCSGO_Compass::OnLayoutReloaded()
{
	BaseClass::OnLayoutReloaded();

	CreatePips();
	CreateTeammatePips();
}
