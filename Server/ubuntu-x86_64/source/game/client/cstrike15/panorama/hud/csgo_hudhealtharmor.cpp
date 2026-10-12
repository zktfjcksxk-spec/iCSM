//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: Display health and armor
//
// SF differences :
//		* Missing border
//		* Change icons to be svg (currently png)
//
//=============================================================================//

#include "cbase.h"
#include "csgo_hudhealtharmor.h"
#include "csgo_hudradar.h"
#include "clientmode_csnormal.h"

#include "c_cs_player.h"

#include "panorama/controls/label.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


REGISTER_PANEL2D_FACTORY( CCSGO_HudHealthArmor, CSGOHudHealthArmor );

static const float g_LowHealthPercent = 0.20f;
static const float g_HealthFlashSeconds = 1.0f;

extern ConVar cl_hud_healthammo_style;
extern ConVar cl_draw_only_deathnotices;
extern ConVar cl_drawhud;
extern ConVar cl_hud_color;
extern ConVar cl_hud_background_alpha;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCSGO_HudHealthArmor::CCSGO_HudHealthArmor( panorama::CPanel2D *pParent, const char *pchID )
	:
	panorama::CPanel2D( pParent, pchID ),
	CPanoramaHudElement( "CCSGO_HudHealthArmor", this ),
	m_hPrevPlayer( nullptr ),
	m_nPrevHealth( -1 ),
	m_nPrevArmor( -1 ),
	m_bPrevHelmet( false )
{
	SetHiddenBits( HIDEHUD_HEALTH | HIDEHUD_PLAYERDEAD );
	
	RequireLoadLayout( "file://{resources}/layout/hud/hudhealtharmor.xml" );

	m_pHealthBar = panorama::panel_cast< panorama::CProgressBar * >( RequireChildInLayoutFile( "HealthBar" ) );
	m_pArmorBar = panorama::panel_cast< panorama::CProgressBar * >( RequireChildInLayoutFile( "ArmorBar" ) );
	m_pHudHealthArmorBG = RequireChildInLayoutFile( "HudHealthArmorBG" );

	// Add hud colors to health/armor bar
	m_pHealthBar->SetMin( 0.0f );
	m_pHealthBar->SetMax( 1.0f );
	if ( panorama::CPanel2D* pBarLeft = m_pHealthBar->FindChild( "HealthBar_Left" ) )
		pBarLeft->AddClass( "cl-hud-background-color" );

	m_pArmorBar->SetMin( 0.0f );
	m_pArmorBar->SetMax( 1.0f );
	if ( panorama::CPanel2D* pBarLeft = m_pHealthBar->FindChild( "ArmorBar_Left" ) )
		pBarLeft->AddClass( "cl-hud-background-color" );

	m_HealthFlashTimer.Invalidate();

	// Hide panel initially
	ShowPanel( false );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCSGO_HudHealthArmor::~CCSGO_HudHealthArmor()
{
}


//-----------------------------------------------------------------------------
// Purpose: Called whenever a new level is starting
//-----------------------------------------------------------------------------
void CCSGO_HudHealthArmor::LevelInit()
{
	// Reset all transient data
	ResetData();
}


//-----------------------------------------------------------------------------
// Purpose: Called once per frame
//-----------------------------------------------------------------------------
void CCSGO_HudHealthArmor::Think()
{
	Update();
}

//-----------------------------------------------------------------------------
// Purpose: Called once per frame before general key processing
//			Updates the health and armor display
//-----------------------------------------------------------------------------
void CCSGO_HudHealthArmor::Update()
{
	static panorama::CPanoramaSymbol k_symHAOnDamage( "hud-HA--on-damage" );
	static panorama::CPanoramaSymbol k_symHACritical( "hud-HA--critical" );
	static panorama::CPanoramaSymbol k_symHAStyleSmall( "hud-HA--small" );
	static panorama::CPanoramaSymbol k_symHAHasHelmet( "hud-HA--helmet" );
	static panorama::CPanoramaSymbol k_symHAPickupArmor( "hud-HA-armor--pickup" );
	static panorama::CPanoramaSymbol k_symHAHasHeavySuit( "hud-HA--heavyassaultsuit" );
	
	// When timer elapses, restore the standard health bar color
	if ( m_HealthFlashTimer.HasStarted() && m_HealthFlashTimer.IsElapsed() )
	{
		m_HealthFlashTimer.Invalidate();
	}
	
	// Update stats

	int		realHealth = 0;
	int		realArmor = 0;
	float	healthPercent = 0.0f;
	float	armorPercent = 0.0f;
	bool	bHasHelmet = false;
	bool	bHasHeavyAssaultSuit = false;

	// Collect all player, weapon and game state data first:

	C_CSPlayer *pPlayer = GetHudPlayer();

	bool bChangedPlayer = m_hPrevPlayer.ChangedFrom( pPlayer );

	if ( pPlayer )
	{
		realHealth = MAX( pPlayer->GetHealth(), 0 );
		realArmor = MAX( pPlayer->ArmorValue(), 0 );
		healthPercent = ( (float)realHealth / (float)pPlayer->GetMaxHealth() );
		bHasHelmet = pPlayer->HasHelmet();
		bHasHeavyAssaultSuit = pPlayer->HasHeavyAssaultSuit();

		// HACK (for now)
		float flMaxArmor = pPlayer->HasHeavyAssaultSuit() ? 200 : 100;
		armorPercent = ( (float)realArmor / (float)flMaxArmor );
	}

	// Update health and which color health bar to draw

	if ( m_nPrevHealth != realHealth )
	{
		// Update health text
		CFmtStr strValue( "%d", realHealth );
		SetDialogVariable( "health", strValue.String() );

		// Update health bar
		m_pHealthBar->SetValue( healthPercent );

		if ( realHealth < m_nPrevHealth && !bChangedPlayer )
		{			
			// Flash red briefly, set a timer to restore color later
			m_HealthFlashTimer.Start( g_HealthFlashSeconds );
		}
	}

	// Health bar/text color
	if ( bChangedPlayer )
	{
		m_HealthFlashTimer.Invalidate();
	}

	const bool bOnDamage = m_HealthFlashTimer.HasStarted();
	const bool bCritical = ( healthPercent <= g_LowHealthPercent );
	SetHasClass( k_symHAOnDamage, bOnDamage );
	SetHasClass( k_symHACritical, bCritical );

	// Update armor display
	
	if ( m_nPrevArmor != realArmor )
	{
		// Update armor text
		CFmtStr strValue( "%d", realArmor );
		SetDialogVariable( "armor", strValue.String() );

		// Update armor bar
		m_pArmorBar->SetValue( armorPercent );
	}
	SetHasClass( k_symHAHasHelmet, bHasHelmet );
	SetHasClass( k_symHAHasHeavySuit, bHasHeavyAssaultSuit );

	if ( bChangedPlayer )
	{
		SetHasClass( k_symHAPickupArmor, false );
	}
	else if ( ( m_nPrevHealth == realHealth ) && ( realArmor > 0 ) && ( ( realArmor > m_nPrevArmor ) || ( bHasHelmet && !m_bPrevHelmet ) ) )
	{
		SetHasClass( k_symHAPickupArmor, true );
		TriggerClass( k_symHAPickupArmor );
	}


	// Change the style of the health / armor display
	// Hide health and armor bar if cl_hud_healthammo_style is set to t1
	SetHasClass( k_symHAStyleSmall, ( cl_hud_healthammo_style.GetInt() == 1 ) );

	m_pHudHealthArmorBG->SetOpacitySimple( cl_hud_background_alpha.GetFloat() );

	// Update data
	m_nPrevHealth = realHealth;
	m_nPrevArmor = realArmor;
	m_bPrevHelmet = bHasHelmet;
	m_hPrevPlayer = pPlayer;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudHealthArmor::SetActive( bool bActive )
{
	if ( bActive != m_bActive )
	{
		ShowPanel( bActive );
	}

	CPanoramaHudElement::SetActive( bActive );
}


//-----------------------------------------------------------------------------
// Purpose: Return true if this hud element should be visible in the current hud state
//-----------------------------------------------------------------------------
bool CCSGO_HudHealthArmor::ShouldDraw()
{
	C_CSPlayer *pPlayer = GetHudPlayer();
	if ( !pPlayer || pPlayer->IsPlayerGhost() )
		return false;

	return cl_drawhud.GetBool() && cl_draw_only_deathnotices.GetBool() == false && CPanoramaHudElement::ShouldDraw();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudHealthArmor::ShowPanel( bool bShow )
{
	static panorama::CPanoramaSymbol k_symHAShowHealthArmor( "hud-HA--active" );

	SetHasClass( k_symHAShowHealthArmor, bShow );

	ResetData();
}

//-----------------------------------------------------------------------------
// Purpose: Reset all transient data
//-----------------------------------------------------------------------------
void CCSGO_HudHealthArmor::ResetData()
{
	m_hPrevPlayer = nullptr;
	m_nPrevHealth = -1;
	m_nPrevArmor = -1;
	m_bPrevHelmet = false;
	m_HealthFlashTimer.Invalidate();
}