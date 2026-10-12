//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "cbase.h"
#include "panorama/ui_root.h"
#include "panorama/uisettings.h"
#include "panorama/ui_popup_manager.h"
#include "panorama/uievents.h"

#if DOTA_DLL
#include "dota_panorama_helpers.h"
#include "popups/dota_db_popup_settings.h"
#include "clientsteamcontext.h"
#include "dota_gc_client.h"
#include "dota_season_controller.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

REGISTER_PANEL2D( CUI_Root, UIRoot )

using namespace panorama;

DEFINE_PANORAMA_EVENT( GameSystemInit );
DEFINE_PANORAMA_EVENT( GameSystemShutdown );
DEFINE_PANORAMA_EVENT( GameEvent_CSMatchEndRestart )

/*static*/ CUtlVector< CUI_Root::SWindowRootEntry > CUI_Root::s_vecWindowRootMap;

CUI_Root::CUI_Root( CPanel2D *pParent, const char *pchID )
	: CPanel2D( pParent, pchID )
	, m_event_colorblind_mode_changed( -1 )
	, m_pPopupManager( nullptr )
	, m_pTooltipManager( nullptr )
	, m_pContextMenuManager( nullptr )
{
#if DOTA_DLL
	RegisterForUnhandledEvent( DOTASettingsChanged(), this, &CUI_Root::EventSettingsChanged );
	RegisterForUnhandledEvent( DOTAWelcomeMessageReceived(), this, &CUI_Root::EventWelcomeMessageReceived );
	RegisterForUnhandledEvent( DOTAIngameEventsUpdated(), this, &CUI_Root::EventIngameEventsUpdated );
#endif

#if defined( CSTRIKE15 )
	// ListenForGameEvent( "colorblind_mode_changed" ); // TODO: CSGO has no support for colorblind mode, avoid printing a warning
#else
	m_event_colorblind_mode_changed = ListenForGameEvent( "colorblind_mode_changed" );
#endif

	// In debug builds add an extra class so that we can style based on it
#ifdef DEBUG
	AddClass( "DebugBuild" );
#endif

#if !defined( CSTRIKE15 )
	// Add a Panorama class for our specific branch. We do it this way instead
	// of a Universe or AppID check because neither of those actually
	// represent what branch we're running. These look like "RelBranch",
	// "MainBranch", "ExperimentalBranch", etc.
	AddClass( V_STRINGIFY( BRANCH_PANORAMA_CLASS_NAME ) );
#else
#if CSTRIKE_TRUNK_BUILD
	AddClass( "TrunkOnly" );
#endif
#endif

	// Add a class for the current language
	IUISettings *pSettings = UIEngine()->UISettings();
	const char *pszLanguage = pSettings ? pSettings->GetUILanguage() : "english";
	AddClass( CFmtStr( "Language_%s", pszLanguage ).Get() );

	// Add a class for the current season.
	UpdateCurrentSeason();

#ifdef DOTA_DLL
	// Set the correct classes for the active sub seasons.
	CDOTASeasonController::Get().SetHasSubSeasonClasses( this );
#endif

	// Add a class for visual quality so that we can disable expensive things in the UI on low end machines
	UpdateVisualQuality();

	// Add a class for partner type in case we need special controls for nexon/perfectworld.
	UpdatePartnerType();

	// Setup the initial video settings
	UpdateAutomaticVideoSettingClasses( this );

	// Register this root into the window map
	s_vecWindowRootMap.AddToTail( { GetParentWindow(), this } );

	ListenForGameEvent( "cs_match_end_restart" );
	RegisterForUnhandledEvent( TopLevelWindowVisibilityChanged(), this, &CUI_Root::OnTopLevelWindowVisibilityChanged );
}

CUI_Root::~CUI_Root()
{
	// Deregister from the window map
	for ( int i = 0; i < s_vecWindowRootMap.Count(); ++i )
	{
		if ( s_vecWindowRootMap[ i ].pRoot == this )
		{
			s_vecWindowRootMap.Remove( i );
			break;
		}
	}
}

/*static*/ CUI_Root *CUI_Root::GetRootForWindow( panorama::IUIWindow *pWindow )
{
	for ( SWindowRootEntry &entry : s_vecWindowRootMap )
	{
		if ( entry.pWindow == pWindow )
			return entry.pRoot;
	}

	return nullptr;
}

void CUI_Root::ShowGenericPopup( const char *pszTitle, const char *pszMessage )
{
	if ( !this || !GetPopupManager() )
		return;
	GetPopupManager()->ShowGeneric( pszTitle, pszMessage );
}

void CUI_Root::FireGameEvent( IGameEvent *event )
{
#if defined( CSTRIKE15 )
	const char *pEventName = event->GetName();
	if ( 0 == V_strcmp( pEventName, "colorblind_mode_changed" ) )
#else
	if ( event->GetID() == m_event_colorblind_mode_changed )
#endif
	{
		UpdateAutomaticVideoSettingClasses( this );
	}
	else if ( FStrEq( event->GetName(), "cs_match_end_restart" ) )
	{
		panorama::DispatchEvent( GameEvent_CSMatchEndRestart(), ( IUIPanelClient* ) nullptr );
	}
}

bool CUI_Root::OnTopLevelWindowVisibilityChanged( panorama::IUIWindow* pWindow )
{
	static const CPanoramaSymbol k_symTopLevelWindowActive( "top-level-window-visible" );

	if ( GetParentWindow() == pWindow )
		SetHasClass( k_symTopLevelWindowActive, pWindow->BIsVisible() );

	return false;
}

void CUI_Root::OnUIScaleFactorChanged( const Vector &vOldScaleFactor, const Vector &vNewScaleFactor )
{
	BaseClass::OnUIScaleFactorChanged( vOldScaleFactor, vNewScaleFactor );
	UpdateAutomaticVideoSettingClasses( this );
}

bool CUI_Root::EventSettingsChanged()
{
	UpdateVisualQuality();
	return false;
}

void CUI_Root::UpdateVisualQuality()
{
#ifdef DOTA_DLL
	extern ConVar r_dashboard_render_quality;

	bool bLowVisualQuality = !r_dashboard_render_quality.GetBool();
	SetHasClass( "LowVisualQuality", bLowVisualQuality );
#endif
}

bool CUI_Root::EventWelcomeMessageReceived()
{
	UpdatePartnerType();
	return false;
}

void CUI_Root::UpdatePartnerType()
{
#ifdef DOTA_DLL
	CDOTAGCClientSystem *pGCClient = GDOTAGCClientSystem();
	if ( !pGCClient )
		return;

	const char *pszPartnerClass = nullptr;
	switch ( pGCClient->GetPartnerType() )
	{
		case PARTNER_PERFECT_WORLD:	pszPartnerClass = "PartnerPerfectWorld";	break;
		case PARTNER_NEXON:			pszPartnerClass = "PartnerNexon";			break;
	}

	const char k_szPartnerAttribute[] = "partner_type";
	SwitchClass( k_szPartnerAttribute, pszPartnerClass );
#endif
}

bool CUI_Root::EventIngameEventsUpdated()
{
	UpdateCurrentSeason();
	return false;
}

void CUI_Root::UpdateCurrentSeason()
{
#ifdef DOTA_DLL
	const char *pszCurrentSeason = CDOTASeasonController::Get().GetCurrentSeasonClass();
	SwitchClass( "season", pszCurrentSeason );
	SetHasClass( "CurrentSeason", pszCurrentSeason && pszCurrentSeason[ 0 ] != '\0' );
#endif
}

//--------------------------------------------------------------------------------------------------
void UpdateAutomaticVideoSettingClasses( CPanel2D *pPanel )
{
	if ( !pPanel )
		return;

	struct SAspectRatio
	{
		float flAspectRatio;
		panorama::CPanoramaSymbol symAspectRatioClass;
	};

	// Our content is authored such that 1920x1080 is scale factor 1.0. Use this info to convert back into an aspect ratio
	float flAspectRatio = pPanel->GetParentWindow()->GetWindowWidth() / ( 1080.0f * pPanel->GetParentWindow()->GetWindowScaleFactor() );

	const SAspectRatio k_ratios[] =
	{
		{ 21.0f / 9.0f, "AspectRatio21x9" },
		{ 16.0f / 9.0f, "AspectRatio16x9" },
		{ 16.0f / 10.0f, "AspectRatio16x10" },
		{ 4.0f / 3.0f, "AspectRatio4x3" },
		{ 5.0f / 4.0f, "AspectRatio5x4" },
	};

	const SAspectRatio *pClosestAspectRatio = NULL;
	float flClosestDiff = 0.0f;
	for ( const SAspectRatio &aspectRatio : k_ratios )
	{
		float flDiff = fabs( aspectRatio.flAspectRatio - flAspectRatio );
		if ( !pClosestAspectRatio || flDiff < flClosestDiff )
		{
			pClosestAspectRatio = &aspectRatio;
			flClosestDiff = flDiff;
		}
	}

	for ( const SAspectRatio &aspectRatio : k_ratios )
	{
		pPanel->SetHasClass( aspectRatio.symAspectRatioClass, pClosestAspectRatio == &aspectRatio );
	}

#if DOTA_DLL
	static panorama::CPanoramaSymbol symColorBlindModeEnabled( "ColorBlindModeEnabled" );
	pPanel->SetHasClass( symColorBlindModeEnabled, IsColorBlind() );
#endif
}

//--------------------------------------------------------------------------------------------------

#if defined( CSTRIKE15 )

class CUIRootGameSystem : public CAutoGameSystem
{
public:
	virtual bool Init() OVERRIDE
	{
		if ( panorama::UIEngine() )
		{
			panorama::UIEngine()->DispatchEvent( GameSystemInit::MakeEvent( nullptr ) );
		}
		return CAutoGameSystem::Init();
	}


	virtual void Shutdown() OVERRIDE
	{
		if ( panorama::UIEngine() )
		{
			panorama::UIEngine()->DispatchEvent( GameSystemShutdown::MakeEvent( nullptr ) );
		}
		CAutoGameSystem::Shutdown();
	}
};

CUIRootGameSystem g_UIRootGameSystem;

#else
class CUIRootGameSystem : public CAutoGameSystem
{
	DECLARE_GAME_SYSTEM( CUIRootGameSystem, CAutoGameSystem );
public:
	virtual void GameInit( const EventGameInit_t &msg ) OVERRIDE
	{
		if ( panorama::UIEngine() )
		{
			panorama::UIEngine()->DispatchEvent( GameSystemInit::MakeEvent( nullptr ) );
		}
	}


	virtual void GameShutdown( const EventGameShutdown_t &msg ) OVERRIDE
	{
		if ( panorama::UIEngine() )
		{
			panorama::UIEngine()->DispatchEvent( GameSystemShutdown::MakeEvent( nullptr ) );
		}
	}
};

CUIRootGameSystem g_UIRootGameSystem;
DEFINE_GAME_SYSTEM_STATIC_NOGLOBALPTR( CUIRootGameSystem, "CUIRootGameSystem", g_UIRootGameSystem );
#endif