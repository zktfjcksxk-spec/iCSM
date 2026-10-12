//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "cbase.h"
#include "csgo_hudgameicons.h"
#include "cs_gamerules.h"
#include "c_cs_player.h"
#include "c_cs_playerresource.h"
#include "panorama/hud/csgo_hudspectator.h"

REGISTER_PANEL2D_FACTORY( CCSGO_HudGameIcons, CSGOHudGameIcons );

extern ConVar cl_drawhud;
extern ConVar cl_draw_only_deathnotices;

class CCSGO_HudGameIcons_Symbols
{
public:
	typedef panorama::CPanoramaSymbol Symbol;
	CCSGO_HudGameIcons_Symbols() {
		mode_bomb				= "hud-game-icons--mode-bomb";
		mode_hostage			= "hud-game-icons--mode-hostage";
		bomb_carried			= "hud-game-icons--bomb-carried";
		bomb_picked_up			= "hud-game-icons--bomb-picked-up";
		bomb_zone				= "hud-game-icons--bomb-zone";
		hostage_rescue_zone		= "hud-game-icons--hostage-rescue-zone";

		icon_hostage			= "hud-game-icons-icon-hostage";
		icon_hostage__active	= "hud-game-icons-icon-hostage--active";
		icon_hostage__carried	= "hud-game-icons-icon-hostage--carried";
		icon_hostage__rescued	= "hud-game-icons-icon-hostage--rescued";
		icon_hostage__dead		= "hud-game-icons-icon-hostage--dead";
	}

	Symbol mode_bomb;
	Symbol mode_hostage;
	Symbol bomb_carried;
	Symbol bomb_picked_up;
	Symbol bomb_zone;
	Symbol hostage_rescue_zone;

	Symbol icon_hostage;
	Symbol icon_hostage__active;
	Symbol icon_hostage__carried;
	Symbol icon_hostage__rescued;
	Symbol icon_hostage__dead;
};

/*static*/ const CCSGO_HudGameIcons_Symbols& CCSGO_HudGameIcons::Symbols()
{
	static const CCSGO_HudGameIcons_Symbols s_symbols;
	return s_symbols;
}

CCSGO_HudGameIcons::CCSGO_HudGameIcons( panorama::CPanel2D *pParent, const char *pchID )
	: BaseClass( pParent, pchID )
	, CPanoramaHudElement( "CCSGO_HudGameIcons", this )
	, m_bVisible( true )
{
	RequireLoadLayout( "file://{resources}/layout/hud/hudgameicons.xml" );
	Reset();
	ResetHostages();

	ListenForGameEvent( "hostage_rescued" );
	ListenForGameEvent( "round_start" );
}

CCSGO_HudGameIcons::~CCSGO_HudGameIcons()
{
}

// CPanel2d overrides
void CCSGO_HudGameIcons::OnLayoutReloaded()
{
	BaseClass::OnLayoutReloaded();

	Reset();
	// Don't reset hostage states here.
}

// CHudElement functions
void CCSGO_HudGameIcons::Init()
{
	CPanoramaHudElement::Init();

	Reset();
	ResetHostages();
}

void CCSGO_HudGameIcons::LevelInit()
{
	CPanoramaHudElement::LevelInit();

	Reset();
	ResetHostages();
}

void CCSGO_HudGameIcons::LevelShutdown()
{
	CPanoramaHudElement::LevelShutdown();

	Reset();
	ResetHostages();
}

void CCSGO_HudGameIcons::ProcessInput()
{
	CPanoramaHudElement::ProcessInput();

	// TODO update icon visibility
	if ( !CSGameRules() )
		return;

	bool bIsSpectating = false;

	static ConVarRef cl_hud_bomb_under_radar( "cl_hud_bomb_under_radar" );
	SetHasClass( Symbols().mode_bomb, CSGameRules()->IsBombDefuseMap() && cl_hud_bomb_under_radar.GetBool() && !bIsSpectating );
	SetHasClass( Symbols().mode_hostage, CSGameRules()->IsHostageRescueMap() );

	C_CSPlayer* pLocalPlayer = ToCSPlayer( GetLocalOrObservedPlayer() );
	bool bChangedPlayer = m_hLocalPlayer.ChangedFrom( pLocalPlayer );
	m_hLocalPlayer = pLocalPlayer;

	if ( BHasClass( Symbols().mode_bomb ) )
	{
		bool bHasBomb = pLocalPlayer ? pLocalPlayer->HasC4() : false;
		bool bInBombZone = pLocalPlayer ? pLocalPlayer->m_bInBombZone : false;

		if ( bHasBomb && !BHasClass( Symbols().bomb_carried ) && !bChangedPlayer )
		{
			// If the person we are observing just picked up the bomb, play an animation
			TriggerClass( Symbols().bomb_picked_up );
		}
		else if ( !bHasBomb )
		{
			// Disable the animation if the user drops the bomb
			SetHasClass( Symbols().bomb_picked_up, false );
		}

		SetHasClass( Symbols().bomb_carried, bHasBomb );
		SetHasClass( Symbols().bomb_zone, bHasBomb && bInBombZone );
	}

	if ( BHasClass( Symbols().mode_hostage ) )
	{
		UpdateHostages( pLocalPlayer, bChangedPlayer );
	}
}

void CCSGO_HudGameIcons::SetActive( bool bActive )
{
	if ( m_bVisible != bActive )
	{
		m_bVisible = bActive;
		SetVisible( m_bVisible );
	}

	CPanoramaHudElement::SetActive( bActive );
}

bool CCSGO_HudGameIcons::ShouldDraw()
{
	if ( !CPanoramaHudElement::ShouldDraw() )
		return false;
	 
	return cl_drawhud.GetBool() && !cl_draw_only_deathnotices.GetBool();
}

void CCSGO_HudGameIcons::Reset()
{
	SetHasClass( Symbols().mode_bomb, false );
	SetHasClass( Symbols().mode_hostage, false );
	SetHasClass( Symbols().bomb_carried, false );
	SetHasClass( Symbols().bomb_picked_up, false );
	SetHasClass( Symbols().bomb_zone, false );
	SetHasClass( Symbols().hostage_rescue_zone, false );

	m_HostagePanels.RemoveAll();
	FindChildrenWithClassTraverse( Symbols().icon_hostage, &m_HostagePanels );

	FOR_EACH_VEC( m_HostagePanels, i )
	{
		m_HostagePanels[i]->SetHasClass( Symbols().icon_hostage__active, false );
		m_HostagePanels[i]->SetHasClass( Symbols().icon_hostage__carried, false );
		m_HostagePanels[i]->SetHasClass( Symbols().icon_hostage__rescued, false );
		m_HostagePanels[i]->SetHasClass( Symbols().icon_hostage__dead, false );
	}

	m_hLocalPlayer = nullptr;
}


void CCSGO_HudGameIcons::UpdateHostages( C_CSPlayer* pLocalPlayer, bool bChangedPlayer )
{
	SetHasClass( Symbols().hostage_rescue_zone, pLocalPlayer ? pLocalPlayer->IsInHostageRescueZone() : false );

	int iNumLive = 0;
	int iNumDead = 0;
	int iNumTransit = 0;
	int iNumRescued = 0;

	C_CS_PlayerResource *pCSPR = GetCSResources();
	FOR_EACH_VEC( m_HostagePanels, i )
	{
		int nEntityID = pCSPR ? pCSPR->GetHostageEntityID( i ) : -1;
		if ( nEntityID <= 0 )
			continue;

		// Rescued hostages appear as dead to the client, so check that first.
		if ( i < V_ARRAYSIZE( m_bRescuedHostages ) && m_bRescuedHostages[i] )
		{
			++iNumRescued;
			continue;
		}

		if ( !pCSPR->IsHostageAlive( i ) )
		{
			++iNumDead;
			continue;
		}
		
		if ( pCSPR->IsHostageFollowingSomeone( i ) )
		{
			++iNumTransit;
			continue;
		}

		++iNumLive;
	}

	// Update panel states.  We do this just based on the counts, so that the player doesn't
	// know exactly which hostage is in each state.
	//
	// Note that we guarantee m_HostagePanels[iPanel] can't overflow, because
	// the above loop only looks at a maximum m_HostagePanels.Count() hostages.
	int iPanel = 0;
	for ( int i = 0; i < iNumRescued; ++i, ++iPanel )
	{
		panorama::CPanel2D* pHostagePanel = m_HostagePanels[iPanel];
		pHostagePanel->SetHasClass( Symbols().icon_hostage__active, true );
		pHostagePanel->SetHasClass( Symbols().icon_hostage__carried, false );
		pHostagePanel->SetHasClass( Symbols().icon_hostage__rescued, true );
		pHostagePanel->SetHasClass( Symbols().icon_hostage__dead, false );
	}

	for ( int i = 0; i < iNumTransit; ++i, ++iPanel )
	{
		panorama::CPanel2D* pHostagePanel = m_HostagePanels[iPanel];
		pHostagePanel->SetHasClass( Symbols().icon_hostage__active, true );
		pHostagePanel->SetHasClass( Symbols().icon_hostage__carried, true );
		pHostagePanel->SetHasClass( Symbols().icon_hostage__rescued, false );
		pHostagePanel->SetHasClass( Symbols().icon_hostage__dead, false );
	}

	for ( int i = 0; i < iNumLive; ++i, ++iPanel )
	{
		panorama::CPanel2D* pHostagePanel = m_HostagePanels[iPanel];
		pHostagePanel->SetHasClass( Symbols().icon_hostage__active, true );
		pHostagePanel->SetHasClass( Symbols().icon_hostage__carried, false );
		pHostagePanel->SetHasClass( Symbols().icon_hostage__rescued, false );
		pHostagePanel->SetHasClass( Symbols().icon_hostage__dead, false );
	}

	for ( int i = 0; i < iNumDead; ++i, ++iPanel )
	{
		panorama::CPanel2D* pHostagePanel = m_HostagePanels[iPanel];
		pHostagePanel->SetHasClass( Symbols().icon_hostage__active, true );
		pHostagePanel->SetHasClass( Symbols().icon_hostage__carried, false );
		pHostagePanel->SetHasClass( Symbols().icon_hostage__rescued, false );
		pHostagePanel->SetHasClass( Symbols().icon_hostage__dead, true );
	}

	Assert( iPanel <= m_HostagePanels.Count() );

	for ( ; iPanel < m_HostagePanels.Count(); ++iPanel )
	{
		panorama::CPanel2D* pHostagePanel = m_HostagePanels[iPanel];
		pHostagePanel->SetHasClass( Symbols().icon_hostage__active, false );
	}
}

void CCSGO_HudGameIcons::ResetHostages()
{
	// mark all hostages as un-rescued
	for ( int i = 0; i < V_ARRAYSIZE( m_bRescuedHostages ); ++i )
	{
		m_bRescuedHostages[i] = false;
	}
}

void CCSGO_HudGameIcons::MarkHostageRescued( int hostageEntityId )
{
	// mark hostage as rescued
	if ( hostageEntityId <= 0 )
		return;

	C_CS_PlayerResource* pCSPR = GetCSResources();
	if ( !pCSPR )
		return;

	for ( int i = 0; i < V_ARRAYSIZE( m_bRescuedHostages ); ++i )
	{
		if ( pCSPR->GetHostageEntityID( i ) == hostageEntityId )
		{
			m_bRescuedHostages[i] = true;
			break;
		}
	}
}

void CCSGO_HudGameIcons::FireGameEvent( IGameEvent *pEvent )
{
	if ( !V_strcmp( pEvent->GetName(), "round_start" ) )
	{
		ResetHostages();
	}
	else if ( !V_strcmp( pEvent->GetName(), "hostage_rescued" ) )
	{
		MarkHostageRescued( pEvent->GetInt( "hostage" ) );
	}
}