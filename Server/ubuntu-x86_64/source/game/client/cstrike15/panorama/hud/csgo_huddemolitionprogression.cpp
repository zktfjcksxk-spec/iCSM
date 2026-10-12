//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//
// SF Differences:
//
//=============================================================================//

#include "cbase.h"
#include "csgo_huddemolitionprogression.h"
#include "cs_gamerules.h"
#include "c_cs_player.h"

#include "panorama/csgo_panel_debug.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


REGISTER_PANEL2D_FACTORY( CCSGO_DemolitionProgressionView, CSGODemolitionProgressionView );
REGISTER_PANEL2D_FACTORY( CCSGO_HudDemolitionProgression, CSGOHudDemolitionProgression );
DECLARE_PANEL_EVENT0( CSGODemolitionProgressFadeEnd );

extern ConVar cl_draw_only_deathnotices;
extern ConVar cl_drawhud;


void CCSGO_DemolitionProgressionSymbols::Initialize()
{
	if ( m_bInitialized )
		return;

	m_bInitialized = true;

	gg_progress__active = "gg-progress--active";
	gg_progress__fade = "gg-progress--fade";
	gg_weapon__active = "gg-weapon--active";
	gg_weapon__past = "gg-weapon--past";
}


//////////////////////////////////////////////////////////////////////////
// CCSGO_WeaponSelectionView: view-only logic for weapon selection hud
CCSGO_DemolitionProgressionSymbols CCSGO_DemolitionProgressionView::s_symbols;

CCSGO_DemolitionProgressionView::CCSGO_DemolitionProgressionView( panorama::CPanel2D* pParent, const char* pchID )
	: panorama::CPanel2D( pParent, pchID )
	, m_Weapons()
	, m_nLevel( -1 )
{
	s_symbols.Initialize();

	RequireLoadLayout( "file://{resources}/layout/hud/huddemolitionprogress.xml" );

	m_pWeaponsPanel = FindChildInLayoutFile( "gg-weapon-list" );
}

CCSGO_DemolitionProgressionView::~CCSGO_DemolitionProgressionView()
{
}

// Debug-only
void CCSGO_DemolitionProgressionView::SetupJavascriptObjectTemplate()
{
	BaseClass::SetupJavascriptObjectTemplate();

	RegisterJSMethod( "Debug_Reset", PANORAMA_DELEGATE( &CCSGO_DemolitionProgressionView::DeleteDynamicElements ) );
	RegisterJSMethod( "Debug_Add", PANORAMA_DELEGATE( &CCSGO_DemolitionProgressionView::DebugAddWeapon ) );
	RegisterJSMethod( "Debug_SetLevel", PANORAMA_DELEGATE( &CCSGO_DemolitionProgressionView::DebugSetDemolitionLevel ) );
	RegisterJSMethod( "Debug_DefaultCT", PANORAMA_DELEGATE( &CCSGO_DemolitionProgressionView::DebugDefaultCT ) );
	RegisterJSMethod( "Debug_DefaultT", PANORAMA_DELEGATE( &CCSGO_DemolitionProgressionView::DebugDefaultT ) );
	RegisterJSMethod( "Debug_Show", PANORAMA_DELEGATE( &CCSGO_DemolitionProgressionView::ShowAndHighlightCurrent ) );
	RegisterJSMethod( "Debug_Fade", PANORAMA_DELEGATE( &CCSGO_DemolitionProgressionView::FadeOut ) );
	RegisterJSMethod( "BuildDebugPanel", PANORAMA_DELEGATE( &CCSGO_DemolitionProgressionView::BuildDebugPanel ) );
}

void CCSGO_DemolitionProgressionView::BuildDebugPanel( panorama::CPanel2D* pPanel )
{
	CPanelDebugHelper debugHelper;
	debugHelper.Init();
	debugHelper.AddDebugMethod( "Debug_Reset", PANORAMA_DELEGATE( &CCSGO_DemolitionProgressionView::DeleteDynamicElements ) );
	debugHelper.AddDebugMethod( "Debug_Add", PANORAMA_DELEGATE( &CCSGO_DemolitionProgressionView::DebugAddWeapon ) );
	debugHelper.AddDebugMethod( "Debug_SetLevel", PANORAMA_DELEGATE( &CCSGO_DemolitionProgressionView::DebugSetDemolitionLevel ) );
	debugHelper.AddDebugMethod( "Debug_DefaultCT", PANORAMA_DELEGATE( &CCSGO_DemolitionProgressionView::DebugDefaultCT ) );
	debugHelper.AddDebugMethod( "Debug_DefaultT", PANORAMA_DELEGATE( &CCSGO_DemolitionProgressionView::DebugDefaultT ) );
	debugHelper.AddDebugMethod( "Debug_Show", PANORAMA_DELEGATE( &CCSGO_DemolitionProgressionView::ShowAndHighlightCurrent ) );
	debugHelper.AddDebugMethod( "Debug_Fade", PANORAMA_DELEGATE( &CCSGO_DemolitionProgressionView::FadeOut ) );
	debugHelper.Finish();

	pPanel->BCreateChildren( debugHelper.mXML );
}

// Destroy all dynamically-created panels and clean up to 'base' state.
void CCSGO_DemolitionProgressionView::DeleteDynamicElements()
{
	FOR_EACH_VEC( m_Weapons, i )
	{
		CWeaponPanelInfo& weaponData = m_Weapons[i];
		delete weaponData.m_pContainer;
	}
	m_Weapons.RemoveAll();

	m_nLevel = -1;

	SetHasClass( Symbols().gg_progress__active, false );
	SetHasClass( Symbols().gg_progress__fade, false );
}

// Debug code to find a weapon
const CEconItemDefinition* CCSGO_DemolitionProgressionView::GetWeaponDef( const char* weaponName )
{
	if ( const CEconItemDefinition* pItemDef = GetItemSchema()->GetItemDefinitionByName( weaponName ) )
		return pItemDef;

	if ( const CEconItemDefinition* pItemDef = GetItemSchema()->GetItemDefinitionByName( CFmtStr( "weapon_%s", weaponName ) ) )
		return pItemDef;

	return nullptr;
}

// Debug code to add a new weapon
bool CCSGO_DemolitionProgressionView::DebugAddWeapon( const char* weaponName )
{
	const CEconItemDefinition* pItemDef = GetWeaponDef( weaponName );
	if ( !pItemDef )
		return false;

	int iPanelIndex = AddDemolitionWeapon( pItemDef );
	return iPanelIndex >= 0;
}

void CCSGO_DemolitionProgressionView::DebugDefaultCT()
{
	DeleteDynamicElements();
	DebugAddWeapon( "m4a1" );
	DebugAddWeapon( "p90" );
	DebugAddWeapon( "ump45" );
	DebugAddWeapon( "deagle" );
	DebugAddWeapon( "nova" );
	DebugAddWeapon( "fiveseven" );
	DebugAddWeapon( "hkp2000" );
	DebugAddWeapon( "ssg08" );
	DebugAddWeapon( "awp" );
	DebugAddWeapon( "scar20" );
}

void CCSGO_DemolitionProgressionView::DebugDefaultT()
{
	DeleteDynamicElements();
	DebugAddWeapon( "ak47" );
	DebugAddWeapon( "p90" );
	DebugAddWeapon( "bizon" );
	DebugAddWeapon( "deagle" );
	DebugAddWeapon( "nova" );
	DebugAddWeapon( "p250" );
	DebugAddWeapon( "hkp2000" );
	DebugAddWeapon( "glock" );
	DebugAddWeapon( "awp" );
	DebugAddWeapon( "g3sg1" );
}

void CCSGO_DemolitionProgressionView::DebugSetDemolitionLevel( int iLevel )
{
	if ( iLevel < 0 )
		iLevel = 0;

	SetLevel( iLevel );
}

// Add a new weapon to armsrace mode
int CCSGO_DemolitionProgressionView::AddDemolitionWeapon( const CEconItemDefinition* pItemDef )
{
	panorama::CPanel2D* pContainer = m_pWeaponsPanel;
	if ( !pContainer )
		pContainer = this;

	CWeaponPanelInfo weapon;
	weapon.m_pContainer = new panorama::CPanel2D( pContainer, nullptr );
	DbgVerify( weapon.m_pContainer->BLoadLayoutSnippet( "WeaponIcon" ) );
	weapon.m_nItemDefIndex = pItemDef->GetDefinitionIndex();

	weapon.m_pContainer->SetHasClass( CFmtStr( "gg-weapon--level%d", m_Weapons.Count() ), true );

	// Set up icon
	const char* szWeaponIconName = pItemDef->GetDefinitionName();
	const char* skip_ = strchr( szWeaponIconName, '_' );
	if ( skip_ )
		szWeaponIconName = skip_ + 1;

	CUtlVector<panorama::CPanel2D*> vecIcons;
	weapon.m_pContainer->FindChildrenWithClassTraverse( "WeaponIconImage", &vecIcons );
	FOR_EACH_VEC( vecIcons, i )
	{
		panorama::CImagePanel* pIcon = panorama::panel_cast< panorama::CImagePanel* >( vecIcons[i] );
		if ( pIcon )
		{
			pIcon->SetImageJS( CFmtStr( "file://{images}/icons/equipment/%s.svg", szWeaponIconName ) );
			pIcon->SetScaling( panorama::k_EImageScalingNone );
		}
	}

	// Set name
	panorama::CLabel* pLabel = panorama::panel_cast< panorama::CLabel* >( weapon.m_pContainer->FindChildInLayoutFile( "weaponname" ) );
	if ( pLabel )
		pLabel->SetText( pItemDef->GetItemBaseName() );

	//weapon.m_pContainer->SetDialogVariable( "WeaponIcon--name", g_pVGuiLocalize->Find( pItemDef->GetItemBaseName() ) );

	// done, add it to our list
	return m_Weapons.AddToTail( weapon );
}

// Set the current level
bool CCSGO_DemolitionProgressionView::SetLevel( int iLevel )
{
	if ( iLevel == m_nLevel )
		return false;
	
	int nWeapons = m_Weapons.Count();

	// Change active weapon
	if ( m_nLevel >= 0 && m_nLevel < nWeapons )
	{
		m_Weapons[m_nLevel].m_pContainer->SetHasClass( Symbols().gg_weapon__active, false );
	}

	if ( iLevel >= 0 && iLevel < nWeapons )
	{
		m_Weapons[iLevel].m_pContainer->SetHasClass( Symbols().gg_weapon__active, true );
	}

	// Set past weapons
	if ( iLevel > m_nLevel )
	{
		int nBottom = MAX( m_nLevel, 0 );
		int nTop = MIN( iLevel, nWeapons );

		for ( int iWeapon = nBottom; iWeapon < nTop; ++iWeapon )
			m_Weapons[iWeapon].m_pContainer->SetHasClass( Symbols().gg_weapon__past, true );
	}
	else // iLevel < m_nLevel
	{
		int nBottom = MAX( iLevel, 0 );
		int nTop = MIN( m_nLevel, nWeapons );
		for ( int iWeapon = nBottom; iWeapon < nTop; ++iWeapon )
			m_Weapons[iWeapon].m_pContainer->SetHasClass( Symbols().gg_weapon__past, false );
	}
	
	m_nLevel = iLevel;
	return true;
}

void CCSGO_DemolitionProgressionView::ShowAndHighlightCurrent()
{
	SetHasClass( Symbols().gg_progress__fade, false );
	SetHasClass( Symbols().gg_progress__active, true );
}

void CCSGO_DemolitionProgressionView::FadeOut()
{
	SetHasClass( Symbols().gg_progress__fade, true );
	SetHasClass( Symbols().gg_progress__active, false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCSGO_HudDemolitionProgression::CCSGO_HudDemolitionProgression( panorama::CPanel2D *pParent, const char *pchID )
	: CCSGO_DemolitionProgressionView( pParent, pchID )
	, CPanoramaHudElement( "CCSGO_HudDemolitionProgression", this )
	, m_bVisible( false )
	, m_nTeam( -1 )
	, m_nWeapons( -1 )
{
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCSGO_HudDemolitionProgression::~CCSGO_HudDemolitionProgression()
{
}


// Intentionally skip CCSGO_DemolitionProgressionView::SetupJavascriptObjectTemplate()
void CCSGO_HudDemolitionProgression::SetupJavascriptObjectTemplate()
{
	panorama::CPanel2D::SetupJavascriptObjectTemplate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudDemolitionProgression::LevelInit( void )
{
	CPanoramaHudElement::LevelInit();

	DeleteDynamicElements();
	m_nTeam = m_nWeapons = -1;

	if ( !CSGameRules() || !CSGameRules()->IsPlayingGunGameTRBomb() )
		return;

	ListenForGameEvent( "round_start" );
	ListenForGameEvent( "round_freeze_end" );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudDemolitionProgression::LevelShutdown( void )
{
	CPanoramaHudElement::LevelShutdown();

	SetActive( false );
	DeleteDynamicElements();

	StopListeningForAllEvents();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudDemolitionProgression::SetActive( bool bActive )
{
	CPanoramaHudElement::SetActive( bActive );

	if ( !bActive )
	{
		// hide
		SetHasClass( Symbols().gg_progress__active, false );
		SetHasClass( Symbols().gg_progress__fade, false );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCSGO_HudDemolitionProgression::ShouldDraw( void )
{
	return cl_drawhud.GetBool()
		&& cl_draw_only_deathnotices.GetBool() == false
		&& CPanoramaHudElement::ShouldDraw()
		&& CSGameRules()
		&& CSGameRules()->IsPlayingGunGameTRBomb();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudDemolitionProgression::Think()
{
	CPanoramaHudElement::Think();

	// If we are currently drawing but the game is in an improper state, stop drawing
	if ( BHasClass( Symbols().gg_progress__active ) )
	{
		if ( !ShouldDraw() || !CSGameRules()->IsFreezePeriod() )
		{
			SetHasClass( Symbols().gg_progress__active, false );
		}
	}

	if ( ShouldDraw() && m_bShowNextThink )
	{
		// Start drawing at the start of each round.
		// We will fade out when freezetime ends
		m_bShowNextThink = false;
		SetupDemolitionWeapons();
		UpdateDemolitionWeaponLevel();
		ShowAndHighlightCurrent();
	}
}



void CCSGO_HudDemolitionProgression::UpdateDemolitionWeaponLevel()
{
	if ( !CSGameRules() )
		return;
	if ( !CSGameRules()->IsPlayingGunGameTRBomb() )
		return;

	C_CSPlayer* pPlayer = ToCSPlayer( GetLocalOrObservedPlayer() );
	if ( !pPlayer )
		return;

	int iLevel = pPlayer->GetPlayerGunGameWeaponIndex();
	SetLevel( iLevel );
}

void CCSGO_HudDemolitionProgression::SetupDemolitionWeapons()
{
	// We don't have the weapon list on the client when we initialize, so if it changes we update it here.
	if ( !CSGameRules() )
		return;
	if ( !CSGameRules()->IsPlayingGunGameTRBomb() )
		return;
	C_CSPlayer* pPlayer = ToCSPlayer( GetLocalOrObservedPlayer() );
	if ( !pPlayer )
		return;
	if ( pPlayer->GetTeamNumber() == m_nTeam && CSGameRules()->GetNumProgressiveGunGameWeapons( m_nTeam ) == m_nWeapons )
		return;

	DeleteDynamicElements();
	m_nTeam = pPlayer->GetTeamNumber();
	m_nWeapons = CSGameRules()->GetNumProgressiveGunGameWeapons( m_nTeam );

	int numWeapons = CSGameRules()->GetNumProgressiveGunGameWeapons( m_nTeam );
	for ( int i = 0; i < numWeapons; ++i )
	{
		const CEconItemDefinition* pWeaponDef = CSGameRules()->GetProgressiveGunGameWeapon( i, m_nTeam );
		AddDemolitionWeapon( pWeaponDef );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CCSGO_HudDemolitionProgression::FireGameEvent( IGameEvent* pEvent )
{
	CPanoramaHudElement::FireGameEvent( pEvent );

	if ( !CSGameRules() || !CSGameRules()->IsPlayingGunGameTRBomb() )
		return;

	if ( !ShouldDraw() )
		return;

	const char* szEventType = pEvent->GetName();
	if ( !V_strcmp( szEventType, "round_start" ) )
	{
		// show on next frame since we don't have the correct data for the round start yet
		m_bShowNextThink = true;
	}
	else if ( !V_strcmp( szEventType, "round_freeze_end" ) )
	{
		FadeOut();
	}
}
