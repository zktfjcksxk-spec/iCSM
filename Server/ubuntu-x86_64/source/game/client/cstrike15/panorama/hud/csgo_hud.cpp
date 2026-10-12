//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "cbase.h"
#include "hudelement.h"
#include "csgo_hud.h"
#include "csgo_hudradar.h"
#include "csgo_huduniquealerts.h"
#include "csgo_hudradio.h"
#include "csgo_hudteamcounter.h"
#include "csgo_hudblurtarget.h"
#include "panorama/csgo_endofmatch.h"
#include "c_plantedc4.h"
#include "cs_gamerules.h"
#include "cs_shareddefs.h"
#include "gametypes/igametypes.h"
#include "c_cs_player.h"

#include "panorama/csgo_popup_manager.h"
#include "panorama/ui_context_menu_manager.h"
#include "panorama/csgo_ui_tooltip_manager.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

extern ConVar cl_drawhud;
extern ConVar cl_hud_color;

static const char* defSafeZone = "1.0";
static const char* defHudScaling = "0.85";
static const float SF_TO_PANORAMA_HUDSCALE = 1.0 / 0.85f;

ConVar safezonex( "safezonex", defSafeZone, FCVAR_ARCHIVE | FCVAR_ARCHIVE_GAMECONSOLE, "The percentage of the screen width that is considered safe from overscan", true, 0.85f, true, 1.0f );
ConVar safezoney( "safezoney", defSafeZone, FCVAR_ARCHIVE | FCVAR_ARCHIVE_GAMECONSOLE, "The percentage of the screen height that is considered safe from overscan", true, 0.85f, true, 1.0f );
ConVar hud_scaling( "hud_scaling", defHudScaling, FCVAR_ARCHIVE, "Scales hud elements", true, .5f, true, 0.95f );

extern IGameTypes *g_pGameTypes;

REGISTER_PANEL2D_FACTORY( CCSGO_Hud, CSGOHud );


using namespace panorama;

DEFINE_PANORAMA_EVENT_DOC( PanoramaGameTimeJumpEvent, "time jump delta in seconds", "Fired when game time jump occurs usually for replay jumping back in time." );


//-----------------------------------------------------------------------------
CPanoramaHudElement::CPanoramaHudElement(const char *pElementName, panorama::CPanel2D *pElementRootPanel ) 
:
	m_pElementRootPanel( pElementRootPanel )
{
	InitCHudElementAfterConstruction(pElementName);
	m_nType = HUD_ELEMENT_TYPE_PANORAMA;
	GetHud().AddHudElement(this);

	// By default, HUD elements build a paint cmd cache
	pElementRootPanel->SetForceBuildPaintCmdCache( true );

	// Allow alternate ticks by default
	SetAllowAlternateTicks( true );
}


//-----------------------------------------------------------------------------
void CPanoramaHudElement::UpdateRepaintStateOnAncestors()
{
	if ( m_pElementRootPanel )
	{
		m_pElementRootPanel->UIPanel()->SetRepaintOnAncestors();
	}
}


//-----------------------------------------------------------------------------
// Static data members
//-----------------------------------------------------------------------------
/*static*/ CCSGO_Hud *CCSGO_Hud::s_pHud = NULL;

CCSGO_Hud::CCSGO_Hud( CPanel2D *pParent, const char *pchID )
	: CUI_Root( pParent, pchID )
{
	Assert( s_pHud == NULL );
	s_pHud = this;

	DbgVerify( BLoadLayout( "file://{resources}/layout/hud/hud.xml" ) );

	GameUI().RegisterGameUIStateListener( this );

	InitHud();
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CCSGO_Hud::~CCSGO_Hud()
{
	GameUI().UnregisterGameUIStateListener( this );
	Assert( s_pHud == this );
	s_pHud = NULL;
}


//-----------------------------------------------------------------------------
void CCSGO_Hud::InitHud()
{
	SetInputNamespace( "csgo_hud" );

	m_pHudTopLeft = RequireChildInLayoutFile( "HudTopLeft" );
	m_pHudTopCenter = RequireChildInLayoutFile( "HudTopCenter" );
	m_pHudTopRight = RequireChildInLayoutFile( "HudTopRight" );
	m_pHudBottomRight = RequireChildInLayoutFile( "HudBottomRight" );
	m_pHudBottomCenter = RequireChildInLayoutFile( "HudBottomCenter" );
	m_pHudLowerLeft = RequireChildInLayoutFile( "HudLowerLeft" );
	m_pHudChat = RequireChildTraverse( "ChatContainer" );
//	m_pHudSpectator = RequireChildTraverse( "SpectatorScoreboardHudScale" );
//	m_pHudSpectatorScore = RequireChildTraverse( "SpectatorScoreboard" );
	
	m_pHudRadio = panorama::panel_cast< CCSGO_HudRadio * >(RequireChildInLayoutFile( "HudRadio" ) );
	m_pHudTeamCounter = panorama::panel_cast< CCSGO_HudTeamCounter * >(RequireChildInLayoutFile( "HudTeamCounter" ) );
	m_pHudBlur =  panorama::panel_cast< CCSGO_HudBlurTarget * >( RequireChildInLayoutFile( "HudBlur" ) );
	m_nCachedClHudColor = cl_hud_color.GetInt();

	if ( m_nCachedClHudColor >= 0 )
		SetHasClass( CFmtStr( "csgo-hud--cl-hud-color-%d", m_nCachedClHudColor ), true );

	// Tell the root about these controls
	SetPopupManager( panorama::panel_cast< CCSGO_PopupManager * >( RequireChildInLayoutFile( "PopupManager" ), true ) );
	SetTooltipManager( panorama::panel_cast< CCSGO_UI_TooltipManager * >( RequireChildInLayoutFile( "TooltipManager" ), true ) );
	SetContextMenuManager( panorama::panel_cast< CUI_ContextMenuManager * >( RequireChildInLayoutFile( "ContextMenuManager" ), true ) );


	OnCSGOGameUIStateChange( CSGO_GAME_UI_STATE_INVALID, GameUI().GetGameUIState() );
}


//-----------------------------------------------------------------------------
void CCSGO_Hud::OnCSGOGameUIStateChange( CSGOGameUIState_t nOldState, CSGOGameUIState_t nNewState )
{
	UpdateVisibility();
}


//-----------------------------------------------------------------------------
void CCSGO_Hud::ReloadLayout()
{
	UnloadLayout();
	RequireLoadLayout( "file://{resources}/layout/hud/hud.xml" );
	InitHud();
}


void CCSGO_Hud::OnMapLoadFinished()
{
	// TODO: Should this classname get something appended? Simply doing the game mode name has some collison potential
	static CPanoramaSymbol k_symGameMode( "game_mode" );
	SwitchClass( k_symGameMode, g_pGameTypes->GetCurrentModeName() );
}

//-----------------------------------------------------------------------------
void CCSGO_Hud::UpdateVisibility()
{
	bool bHudVisible = ( GameUI().GetGameUIState() == CSGO_GAME_UI_STATE_INGAME )/* || CCSGO_EndOfMatch::GetInstance()->IsOpen()*/;

	IUIWindow *pWindow = GetParentWindow();
	if (pWindow && (pWindow->BIsVisible() != bHudVisible))
	{
		pWindow->SetVisible(bHudVisible);
	}
}

//-----------------------------------------------------------------------------
void CCSGO_Hud::Update( void )
{
	UpdateVisibility();

	IUIWindow *pWindow = GetParentWindow();
	if ( pWindow && pWindow->BIsVisible() )
	{
		UpdateScaleAndSafezones();

		// Update hud color if changed
		if ( m_nCachedClHudColor != cl_hud_color.GetInt() )
		{
			if ( m_nCachedClHudColor >= 0 )
				SetHasClass( CFmtStr( "csgo-hud--cl-hud-color-%d", m_nCachedClHudColor ), false );
			m_nCachedClHudColor = cl_hud_color.GetInt();
			if ( m_nCachedClHudColor >= 0 )
				SetHasClass( CFmtStr( "csgo-hud--cl-hud-color-%d", m_nCachedClHudColor ), true );
		}

		C_CSPlayer *pPlayer = GetHudPlayer();
		if ( pPlayer )
		{
			CWeaponCSBase *pWeapon = pPlayer->GetActiveCSWeapon();
			if ( pWeapon )
			{
				static const panorama::CPanoramaSymbol k_symTabletEnabled( "tabletenabled" );
				bool bTabletEnabled = pWeapon->GetCSWeaponID() == WEAPON_TABLET;
				SetHasClass( k_symTabletEnabled, bTabletEnabled );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// update hud panels affected by safezonex/y and hud_scaling
//-----------------------------------------------------------------------------
void CCSGO_Hud::UpdateScaleAndSafezones()
{
	// remap UIScale for panorama, since the values were correct for Scaleform where 0.85 is the default scale (1.0 in panorama)
	float flHudScale = hud_scaling.GetFloat() * SF_TO_PANORAMA_HUDSCALE;

	Vector vUIScale = Vector( flHudScale, flHudScale, 1.0f );

	// safezonex/y
	panorama::CUILength safezoneXPercent( ( 1.0f - safezonex.GetFloat() ) * 50.0f, panorama::CUILength::k_EUILengthPercent );
	panorama::CUILength safezoneYPercent( ( 1.0f - safezoney.GetFloat() ) * 50.0f, panorama::CUILength::k_EUILengthPercent );
	panorama::CUILength zeroPercent( 0.0f, panorama::CUILength::k_EUILengthPercent );

	panorama::IUIPanelStyle *pPanelStyle;

	pPanelStyle = m_pHudTopLeft->AccessStyle();
	pPanelStyle->SetUIScale( vUIScale );
	pPanelStyle->SetMargin( safezoneXPercent, safezoneYPercent, zeroPercent, zeroPercent );

	pPanelStyle = m_pHudTopCenter->AccessStyle();
	pPanelStyle->SetUIScale( vUIScale );
	pPanelStyle->SetMargin( zeroPercent, safezoneYPercent, zeroPercent, zeroPercent );

	pPanelStyle = m_pHudTeamCounter->AccessStyle();
	pPanelStyle->SetUIScale( vUIScale );
	panorama::EHorizontalAlignment horizontalAlignment;
	panorama::EVerticalAlignment verticalAlignment;
	pPanelStyle->GetAlignment( horizontalAlignment, verticalAlignment );
	if ( verticalAlignment == panorama::EVerticalAlignment::k_EVerticalAlignmentTop )
	{
		pPanelStyle->SetMargin( zeroPercent, safezoneYPercent, zeroPercent, zeroPercent );
	}
	else
	{
		pPanelStyle->SetMargin( zeroPercent, zeroPercent, zeroPercent, safezoneYPercent );
	}

	pPanelStyle = m_pHudTopRight->AccessStyle();
	pPanelStyle->SetUIScale( vUIScale );
	pPanelStyle->SetMargin( zeroPercent, safezoneYPercent, safezoneXPercent, zeroPercent );

	pPanelStyle = m_pHudBottomRight->AccessStyle();
	pPanelStyle->SetUIScale( vUIScale );
	pPanelStyle->SetMargin( zeroPercent, zeroPercent, safezoneXPercent, safezoneYPercent );

	pPanelStyle = m_pHudBottomCenter->AccessStyle();
	pPanelStyle->SetUIScale( vUIScale );
	pPanelStyle->SetMargin( zeroPercent, zeroPercent, zeroPercent, safezoneYPercent );

	pPanelStyle = m_pHudLowerLeft->AccessStyle();
	pPanelStyle->SetUIScale( vUIScale );
	pPanelStyle->SetMargin( safezoneXPercent, zeroPercent, zeroPercent, safezoneYPercent );

	pPanelStyle = m_pHudRadio->AccessStyle();
	pPanelStyle->SetUIScale( vUIScale );
	pPanelStyle->SetMargin( safezoneXPercent, zeroPercent, zeroPercent, zeroPercent );

// 	pPanelStyle = m_pHudSpectator->AccessStyle();
// 	Vector vSpecScaleMax = Vector( 1.05f, 1.05f, 1.0f );
// 	pPanelStyle->SetUIScale( Vector( MIN( vUIScale.x, vSpecScaleMax.x ), MIN( vUIScale.y, vSpecScaleMax.y ), vUIScale.z) );
// 	pPanelStyle->SetMargin( safezoneXPercent, zeroPercent, safezoneXPercent, safezoneYPercent );

// 	pPanelStyle = m_pHudSpectatorScore->AccessStyle();
// 	pPanelStyle->SetUIScale( vUIScale );
// 	pPanelStyle->SetMargin( zeroPercent, safezoneYPercent, zeroPercent, safezoneYPercent );
	

	// Don't apply hud scaling and margins to chat when end of match is showing
// 	if( !CCSGO_EndOfMatch::GetInstance()->IsOpen() )
// 	{
// 		pPanelStyle = m_pHudChat->AccessStyle();
// 		pPanelStyle->SetUIScale( vUIScale );
// 		pPanelStyle->SetMargin( safezoneXPercent, zeroPercent, zeroPercent, safezoneYPercent );
// 	}
// 	else
	{
		Vector vChatScale = Vector( 0.9f, 0.9f, 0.9f );
		pPanelStyle = m_pHudChat->AccessStyle();
		pPanelStyle->SetUIScale( vChatScale );
		pPanelStyle->SetMargin( zeroPercent, zeroPercent, zeroPercent, zeroPercent );
	}

}

//-----------------------------------------------------------------------------
// Note these colors were hard coded from Scaleform days.
// They are SRGB, so perform conversion as required (ops using these colors should be linear for Panorama)
//-----------------------------------------------------------------------------
void CCSGO_Hud::GetHudTextColor( Color *pColor )
{
	const char* szHudColor;

	szHudColor = GetLayoutFileDefine( CFmtStr( "color-hud-%d", cl_hud_color.GetInt() ) );

	if ( !szHudColor )
		szHudColor = GetLayoutFileDefine( "color-hud-0" );

	if ( !szHudColor )
		szHudColor = "#d5e286"; // default

	if ( !CSSHelpers::BParseColor( pColor, szHudColor ) )
		pColor->SetColor( 0xff, 0x00, 0x00, 0xff ); // parse error, output red?
}

//-----------------------------------------------------------------------------
// Note srgb-->linear conversion to match other Panorama usage
//-----------------------------------------------------------------------------
void CCSGO_Hud::GetBGHudTextColor( Color *pColor, const float flBrightness, const float flSaturation )
{
	Color c;
	GetHudTextColor( &c );

	Vector rgb, hsv;

	// srgb --> linear first
	rgb.x = SrgbGammaToLinear( (float)c.r() / 255.0f );
	rgb.y = SrgbGammaToLinear( (float)c.g() / 255.0f );
	rgb.z = SrgbGammaToLinear( (float)c.b() / 255.0f );

	RGBtoHSV( rgb, hsv );

	hsv.z *= flBrightness;
	hsv.y *= flSaturation;

	HSVtoRGB( hsv, rgb );

	// linear --> srgb
	rgb.x = SrgbLinearToGamma( rgb.x ) * 255.0f;
	rgb.y = SrgbLinearToGamma( rgb.y ) * 255.0f;
	rgb.z = SrgbLinearToGamma( rgb.z ) * 255.0f;

	pColor->SetColor( (int)rgb.x, (int)rgb.y, (int)rgb.z, c.a() );
}
