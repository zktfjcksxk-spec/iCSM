//========= Copyright © Valve Corporation, All rights reserved. ============//
//
// Purpose:  Use mouse control to select among displayed options
//
//=====================================================================================//

#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "hud_element_helper.h"
#include "csgo_hudrosettaselector.h"
#include "hud_macros.h"
#include "view.h"

#if defined( INCLUDE_SCALEFORM )
#include "HUD/sfhudfreezepanel.h"
#endif

#include "engine/IEngineSound.h"
#include "clientmode_shared.h"
#include "clientmode_csnormal.h"

#include "IGameUIFuncs.h"
//#include "uicomponents/uicomponent_inventory.h"
//#include "uicomponents/uicomponent_loadout.h"
//#include "uicomponents/uicomponent_mypersona.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

REGISTER_PANEL2D_FACTORY( CCSGO_HudRosettaSelector, CSGOHudRosettaSelector );

DECLARE_PANORAMA_EVENT0( RosettaInventorySlotActivate );
DEFINE_PANORAMA_EVENT( RosettaInventorySlotActivate );

DECLARE_PANORAMA_EVENT0( RosettaInventoryQuickSprayToggle );
DEFINE_PANORAMA_EVENT( RosettaInventoryQuickSprayToggle );

DECLARE_PANORAMA_EVENT0( RosettaInventorySlotPrev );
DEFINE_PANORAMA_EVENT( RosettaInventorySlotPrev );

DECLARE_PANORAMA_EVENT0( RosettaInventorySlotNext );
DEFINE_PANORAMA_EVENT( RosettaInventorySlotNext );

static const char* g_pszSprayErrorSound = "ui/menu_back.wav";

extern ConVar cl_playerspray_auto_apply;
extern ConVar cl_scoreboard_mouse_enable_binding;

extern const CEconItemView * HelperFindOrCreateEconItemViewForItemID( uint64 iItemID );
extern bool Helper_IsFanShield( const CEconItemDefinition * pEconItemDefinition );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------

// TODO - resolve SF/Panorama clashes
bool Helper_CanUseSprays_Panorama( void )
{
	if ( g_bEngineIsHLTV )
		return false;

	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pLocalPlayer )
		return false;

	return (pLocalPlayer->IsAlive() || pLocalPlayer->IsPlayerGhost()) && ( pLocalPlayer->GetTeamNumber() == TEAM_TERRORIST || pLocalPlayer->GetTeamNumber() == TEAM_CT );
}

void ShowSprayMenu_Panorama( const CCommand &args )
{
	if ( Helper_CanUseSprays_Panorama() )
	{
		CCSGO_HudRosettaSelector *pRosetta = GET_HUDELEMENT( CCSGO_HudRosettaSelector );
		pRosetta->SetShowRosetta( true, "spray" );
	}
}
ConCommand panorama_showSprayMenu( "+spray_menu", ShowSprayMenu_Panorama );

void HideSprayMenu_Panorama( const CCommand &args )
{
	extern ConVar cl_playerspray_auto_apply;
// 	extern bool g_playerspray_menu_closing_by_user;
// 	g_playerspray_menu_closing_by_user = true;

	CCSGO_HudRosettaSelector *pRosetta = GET_HUDELEMENT( CCSGO_HudRosettaSelector );
	pRosetta->SetShowRosetta( false, "spray" );

//	g_playerspray_menu_closing_by_user = false;
}
ConCommand panorama_hideSprayMenu( "-spray_menu", HideSprayMenu_Panorama);

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CRosettaInventoryItemPanel::CRosettaInventoryItemPanel()
{
	m_pRosettaSelector = nullptr;

	m_pSlot = nullptr;
	m_pImage = nullptr;
	m_pInvCount = nullptr;
}

CRosettaInventoryItemPanel::~CRosettaInventoryItemPanel()
{
}

bool CRosettaInventoryItemPanel::OnActivate()
{
	//CUiComponent_Loadout::GetInstance()->EquipItemInSlot( "noteam", CFmtStr( "%llu", m_itemID ), "spray0" );

	m_pSelection->SetOpacity( 1.0f );
	m_pRosettaSelector->SetUpSpray();

	// disable cursor after an action
	m_pRosettaSelector->EnableCursor( false );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCSGO_HudRosettaSelector::CCSGO_HudRosettaSelector( panorama::CPanel2D *pParent, const char *pchID )
	:
	panorama::CPanel2D( pParent, pchID ),
	CPanoramaHudElement( "CCSGO_HudRosettaSelector", this ),
	m_hDenyInputToGame( 0 ),
	m_bVisible( false ),
	m_bEnableCursor( false ),
	m_nTotalSprays( 0 ),
	m_nItemTiles( 4 ),
	m_nTilePos( 345 ),
	m_nActiveIndex( 0 ),
	m_nPage( 1 )
{
	RequireLoadLayout( "file://{resources}/layout/hud/hudrosettaselector.xml" );

	m_pBGGradient = RequireChildInLayoutFile( "RosettaBGGradient" );

	m_pCountdownBG = RequireChildInLayoutFile( "RosettaCountdown" );
	m_pCountdownPie = RequireChildInLayoutFile( "RosettaCountdownPie" );
	m_pCountdownTimer = panorama::panel_cast< panorama::CLabel * >( RequireChildInLayoutFile( "RosettaCountdownTimer" ) );
	m_pSprayImage = panorama::panel_cast< panorama::CImagePanel * >( RequireChildInLayoutFile( "RosettaSprayImage" ) );
	m_pSprayImageBG = RequireChildInLayoutFile( "RosettaSprayImageBG" );

	m_pCountdownBG->SetVisible( false );

	m_pSprayChargesRemainingLabel = panorama::panel_cast< panorama::CLabel * >( RequireChildInLayoutFile( "RosettaSprayTextCount" ) );
	m_pSprayHintLabel = panorama::panel_cast< panorama::CLabel * >( RequireChildInLayoutFile( "RosettaSprayTextHint" ) );

	m_pInventory = RequireChildInLayoutFile( "RosettaInventory" );
	m_pInventoryItems = RequireChildInLayoutFile( "RosettaInventoryItems" );
	m_pQuickSprayToggle = panorama::panel_cast< panorama::CToggleButton * >( RequireChildInLayoutFile( "RosettaQuickSprayToggle" ) );
	RegisterEventHandler( RosettaInventoryQuickSprayToggle(), this, &CCSGO_HudRosettaSelector::EventSetAutoToggle );

	m_pSprayInfo = RequireChildInLayoutFile( "RosettaInfo" );
	m_pSprayInfoText = panorama::panel_cast< panorama::CLabel * >( RequireChildInLayoutFile( "RosettaInfoText" ) );

	m_pInventoryPrev = panorama::panel_cast< panorama::CImagePanel * >( RequireChildInLayoutFile( "RosettaInventoryLeftArrow" ) );
	RegisterEventHandler( RosettaInventorySlotPrev(), this, &CCSGO_HudRosettaSelector::InventoryPrev );
	m_pInventoryNext = panorama::panel_cast< panorama::CImagePanel * >( RequireChildInLayoutFile( "RosettaInventoryRightArrow" ) );
	RegisterEventHandler( RosettaInventorySlotNext(), this, &CCSGO_HudRosettaSelector::InventoryNext );

	// inventory slots
	m_aInventorySlots.EnsureCount( m_nItemTiles );

	for ( int i = 0; i < m_nItemTiles; i++ )
	{
		char szTmp[ 128 ] = {};

		// add as children of RosettaInventory panel
		m_aInventorySlots[ i ] = new CRosettaInventoryItemPanel;
		CRosettaInventoryItemPanel *pItemPanel = m_aInventorySlots[ i ];

		pItemPanel->m_pRosettaSelector = this;

		V_sprintf_safe( szTmp, "RosettaInventorySlot%d", i );
		pItemPanel->m_pSlot = new panorama::CPanel2D( m_pInventoryItems, szTmp );
		pItemPanel->m_pSlot->RequireLoadLayoutSnippet( "RosettaInventoryItem" );
		RegisterEventHandlerOnPanel( RosettaInventorySlotActivate(), pItemPanel->m_pSlot->UIPanel(), pItemPanel, &CRosettaInventoryItemPanel::OnActivate );

		pItemPanel->m_pSelection = panorama::panel_cast< panorama::CImagePanel * >( pItemPanel->m_pSlot->RequireChildInLayoutFile( "InventorySelection" ) );
		pItemPanel->m_pImage = panorama::panel_cast< panorama::CImagePanel * >( pItemPanel->m_pSlot->RequireChildInLayoutFile( "InventoryImage" ) );
		pItemPanel->m_pInvCount = panorama::panel_cast< panorama::CLabel * >( pItemPanel->m_pSlot->RequireChildInLayoutFile( "InventoryCount" ) );
	}

	SetHiddenBits( HIDEHUD_MISCSTATUS );

	SetVisible( false );

	SetAcceptsInput( false );
	SetAcceptsFocus( false );
	SetDisableFocusOnMouseDown( true );
	
	// UIpanels default to always consume hover clicks, which prevents game from handling mouse buttons to spray. 
	UIPanel()->SetAlwaysConsumeHoverClicks( false );

	m_XUID = 0;// CUiComponent_MyPersona::GetInstance()->GetXuid();
}

CCSGO_HudRosettaSelector::~CCSGO_HudRosettaSelector()
{
	// enable input to game
	if ( m_hDenyInputToGame )
	{
		gameuifuncs->PanoramaReleaseDenyMouseInputToGame( m_hDenyInputToGame );
		m_hDenyInputToGame = 0;
	}

	UnregisterEventHandler( RosettaInventoryQuickSprayToggle(), this, &CCSGO_HudRosettaSelector::EventSetAutoToggle );
	
	UnregisterEventHandler( RosettaInventorySlotPrev(), this, &CCSGO_HudRosettaSelector::InventoryPrev );
	UnregisterEventHandler( RosettaInventorySlotNext(), this, &CCSGO_HudRosettaSelector::InventoryNext );

	for ( int i = 0; i < m_nItemTiles; i++ )
	{
		CRosettaInventoryItemPanel *pItemPanel = m_aInventorySlots[ i ];
		UnregisterEventHandlerOnPanel( RosettaInventorySlotActivate(), pItemPanel->m_pSlot->UIPanel(), pItemPanel, &CRosettaInventoryItemPanel::OnActivate );
	}

	m_aInventorySlots.Purge();
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CCSGO_HudRosettaSelector::SetShowRosetta( bool bShow, const char* szType )
{
	if ( bShow && V_strstr( szType, "spray" ) )
	{
		SetUpSpray();
	}

	ShowPanel( bShow );
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CCSGO_HudRosettaSelector::ShowPanel( bool bShow )
{
	static panorama::CPanoramaSymbol k_symShowHudRosettaSelector( "ShowHudRosettaSelector" );
	static panorama::CPanoramaSymbol k_symHideHudRosettaSelector( "HideHudRosettaSelector" );

	if ( m_bVisible != bShow )
	{
		m_bVisible = bShow;

		if ( bShow )
		{
			SetHasClass( k_symShowHudRosettaSelector, true );
			SetHasClass( k_symHideHudRosettaSelector, false );
			SetVisible( true );

			// enable input
			SetAcceptsInput( true );

			RegisterForUnhandledEvent( OnMouseEnableBinding(), this, &CCSGO_HudRosettaSelector::EventOnMouseEnableBinding );
		}
		else
		{
			SetHasClass( k_symShowHudRosettaSelector, false );
			SetHasClass( k_symHideHudRosettaSelector, true );
			SetVisible( false );

// 			const char *szError = CUiComponent_Loadout::GetInstance()->GetSprayApplicationError();
// 
// 			if ( cl_playerspray_auto_apply.GetBool() && ( !szError || V_strlen( szError ) == 0 ) )
// 			{
// 				CUiComponent_Loadout::GetInstance()->ActionSpray( 0 );
// 			}

			// disable input
			SetAcceptsInput( false );

			EnableCursor( false );
			UnregisterForUnhandledEvent( OnMouseEnableBinding(), this, &CCSGO_HudRosettaSelector::EventOnMouseEnableBinding );
		}
	}
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CCSGO_HudRosettaSelector::ProcessInput( void )
{
	if ( !m_bVisible )
		return;

	static panorama::CPanoramaSymbol k_symRosettaSelectorCoolDown( "RosettaSelector__CoolDown" );

	float flCoolDown = 0.f;// CUiComponent_Loadout::GetInstance()->GetSprayCooldownRemaining();
 	bool bHasCoolDown = flCoolDown > 0.f;

	SetHasClass( k_symRosettaSelectorCoolDown, bHasCoolDown );
	m_pCountdownBG->SetVisible( bHasCoolDown );
	m_pCountdownTimer->SetVisible( bHasCoolDown );

	if ( bHasCoolDown )
	{
		m_pSprayImage->SetVisible( true );

		m_pCountdownTimer->SetText( CFmtStr( "<i>%d</i>", (int)flCoolDown ), panorama::CLabel::k_ETextTypeHTML );

// 		float flMaxCoolDown = CUiComponent_Loadout::GetInstance()->GetSprayCooldownSetting();
// 		panorama::CUILength center( 50.0f, panorama::CUILength::k_EUILengthPercent );
// 
// 		panorama::IUIPanelStyle *pPanelStyle = m_pCountdownPie->AccessStyle();
// 		pPanelStyle->SetRadialClip( true, center, center, 0.0f, 360.0f - ( ( flCoolDown * 360.0f ) / flMaxCoolDown ) );
	}
// 	else
// 	{
// 		UpdateSpray( cl_playerspray_auto_apply.GetBool() );
// 	}
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CCSGO_HudRosettaSelector::LevelInit( void )
{
	ShowPanel( false );
	enginesound->PrecacheSound( g_pszSprayErrorSound, true, true );
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CCSGO_HudRosettaSelector::LevelShutdown( void )
{
	ShowPanel( false );
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CCSGO_HudRosettaSelector::Reset( void )
{
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
extern bool IsTakingAFreezecamScreenshot( void );
extern ConVar cl_drawhud;
bool CCSGO_HudRosettaSelector::ShouldDraw( void )
{
	if ( IsTakingAFreezecamScreenshot() )
		return false;

	return cl_drawhud.GetBool() && CHudElement::ShouldDraw();
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CCSGO_HudRosettaSelector::SetActive( bool bActive )
{
	if ( !bActive && m_bVisible )
	{
		ShowPanel( bActive );
	}

	CPanoramaHudElement::SetActive( bActive );
}

extern void PlayerDecalDataSendActionSprayToServer( int nSlot );
extern bool Helper_CanShowPreviewDecal( const CEconItemView **ppOutEconItemView = NULL, trace_t* pOutSprayTrace = NULL, Vector *pOutVecPlayerRight = NULL, uint32* pOutUnStickerKitID = NULL );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRosettaSelector::SetUpSpray()
{
// 	CUiComponent_Inventory::GetInstance()->SetInventorySortAndFilters( "newest", "spray,item_definition:spraypaint", "" );
// 
// 	m_nTotalSprays = CUiComponent_Inventory::GetInstance()->GetInventoryCount();
// 
// 	// always update cursor hint string
// 	MakeCursorHintString();
// 
// 	// Player has no sprays so show them the no spray state
// 	if ( m_nTotalSprays < 1 )
// 	{
// 		PlayerHasNoSprays();
// 		return;
// 	}
// 
// 	itemid_t iItemID = GetEquippedItemID();
// 
// 	if ( !CUiComponent_Inventory::GetInstance()->IsValidItemID( iItemID ) )
// 	{
// 		NoSprayEquipped();
// 		ShowInventory( true );
// 		UpdateAutoToggle();
// 		return;
// 	}
// 
// 	// get uses remaining and set it
// 	uint32 numUsesRemaining = CUiComponent_Inventory::GetInstance()->GetItemAttributeValue( iItemID, "sprays remaining" );
// 	m_pSprayChargesRemainingLabel->SetDialogVariable( "s1", CNumStr( numUsesRemaining ).String() );
// 	m_pSprayChargesRemainingLabel->SetText( "#Attrib_SpraysRemaining" );
// 
// 	LoadSprayIcon( m_pSprayImage, iItemID );
// 	HasCoolDown( cl_playerspray_auto_apply.GetBool() );
// 	ShowInventory( false );
// 	UpdateAutoToggle();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRosettaSelector::PlayerHasNoSprays()
{
	m_pSprayChargesRemainingLabel->SetVisible( true );
	m_pSprayChargesRemainingLabel->SetText( "#CSGO_No_Sprays" );

	m_pSprayHintLabel->SetVisible( false );
	m_pCountdownBG->SetVisible( false );
	m_pSprayImage->SetVisible( false );
	m_pSprayImageBG->SetVisible( false );
	m_pInventory->SetVisible( false );
	m_pQuickSprayToggle->SetVisible( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRosettaSelector::NoSprayEquipped()
{
	m_pSprayChargesRemainingLabel->SetVisible( true );
	m_pSprayChargesRemainingLabel->SetText( "#CSGO_No_Spray_Equipped" );

	m_pSprayHintLabel->SetVisible( false );
	m_pCountdownBG->SetVisible( false );
	m_pCountdownTimer->SetVisible( false );
	m_pSprayImage->SetVisible( false );
	m_pSprayImageBG->SetVisible( true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRosettaSelector::ShowInventory( bool bShowAnySpray )
{
	//player has more than one spray so show the inventory
	if ( m_nTotalSprays > 1 || ( bShowAnySpray && m_nTotalSprays > 0 ) )
	{
		m_pInventory->SetVisible( true );

		if ( m_bEnableCursor )
		{
			m_pInventory->SetOpacity( 1.0f );
			m_pBGGradient->SetOpacity( 1.0f );
		}
		else
		{
			m_pInventory->SetOpacity( 0.6f );
			m_pBGGradient->SetOpacity( 0.6f );
		}

		ResetPages();
		SetItems();

		EnableDisablePageButtons();

		return;
	}

	m_pInventory->SetVisible( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRosettaSelector::MakeCursorHintString()
{
	m_pSprayInfoText->SetDialogVariable( "spray_mouse_enable", CFmtStr( "%%%s%%", cl_scoreboard_mouse_enable_binding.GetString() ) );
	m_pSprayInfoText->SetText( "#Panorama_CSGO_Spray_EnableMouse", panorama::CLabel::k_ETextTypeHTML );

	// If you do not have a spray equipped and only one spray or the cursor is showing or the inventory is not visible
	// Then do not show
	if ( m_bEnableCursor || m_nTotalSprays < 1 )
		m_pSprayInfo->SetVisible( false );
	else
		m_pSprayInfo->SetVisible( true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRosettaSelector::UpdateAutoToggle()
{
	m_pQuickSprayToggle->SetVisible( true );

// 	bool bAuto = cl_playerspray_auto_apply.GetBool();
// 	m_pQuickSprayToggle->SetSelected( bAuto );
// 	UpdateSpray( bAuto );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRosettaSelector::LoadSprayIcon( panorama::CImagePanel *pImage, itemid_t iItemID )
{
// 	static panorama::CPanoramaSymbol k_symRosettaInventoryImage( "RosettaInventory__Image" );
// 	static panorama::CPanoramaSymbol k_symRosettaInventoryImageTint( "RosettaInventory__ImageTint" );
// 
// 	const char *szInventoryImage = CUiComponent_Inventory::GetInstance()->GetItemInventoryImage( iItemID );
// 
// 	if ( szInventoryImage )
// 	{
// 		pImage->SetImage( CFmtStr( "file://{images_econ}/%s.png", szInventoryImage ) );
// 		pImage->SetVisible( true );
// 
// 		const char *szInventoryImageTint = CUiComponent_Inventory::GetInstance()->GetSprayTintColorCode( iItemID );
// 		panorama::IUIPanelStyle *pPanelStyle = pImage->AccessStyle();
// 
// 		if ( szInventoryImageTint )
// 		{
// 			pPanelStyle->SetWashColor( szInventoryImageTint );
// 		}
// 		else
// 		{
// 			pPanelStyle->SetWashColor( "#FFFFFF" );
// 		}
// 	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRosettaSelector::UpdateSpray( bool bAutoApply )
{
// 	const char *szSprError = CUiComponent_Loadout::GetInstance()->GetSprayApplicationError();
// 
// 	bool bSprayImageVisible = false;
// 	bool bValidEquippedSpray = CUiComponent_Inventory::GetInstance()->IsValidItemID( GetEquippedItemID() );
// 
// 	if ( szSprError && ( V_strlen( szSprError ) != 0 ) )
// 	{
// 		panorama::IUIPanelStyle *pPanelStyle = m_pSprayHintLabel->AccessStyle();
// 		pPanelStyle->SetForegroundColor( "#FF0000" );
// 
// 		m_pSprayHintLabel->SetText( CFmtStr( "%s_Short", szSprError ), panorama::CLabel::k_ETextTypeHTML );
// 
// 		bSprayImageVisible = bValidEquippedSpray;
// 	}
// 	else
// 	{
// 		panorama::IUIPanelStyle *pPanelStyle = m_pSprayHintLabel->AccessStyle();
// 		pPanelStyle->SetForegroundColor( "#40FD40" );
// 
// 		if ( bAutoApply )
// 		{
// 			m_pSprayHintLabel->SetTextWithDialogVariables( "#Panorama_Attrib_SpraysHint_Auto", panorama::CLabel::k_ETextTypeHTML );
// 		}
// 		else
// 		{
// 			m_pSprayHintLabel->SetTextWithDialogVariables( "#Panorama_Attrib_SpraysHint", panorama::CLabel::k_ETextTypeHTML );
// 		}
// 		
// 		bSprayImageVisible = m_pCountdownTimer->BIsVisible();
// 	}
// 
// 	m_pSprayImage->SetVisible( bSprayImageVisible );
// 	m_pSprayImageBG->SetVisible( !bValidEquippedSpray );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRosettaSelector::HasCoolDown( bool bAutoApply )
{
// 	float flCoolDown = CUiComponent_Loadout::GetInstance()->GetSprayCooldownRemaining();
// 
// 	// Since there is no cooldown check to see if we can spray
// 	UpdateSpray( bAutoApply );
// 
// 	// If you have a cooldown then show the cool down and return
// 	if ( flCoolDown > 0.0f )
// 	{
// 		return;
// 	}

	m_pCountdownBG->SetVisible( false );
	m_pCountdownTimer->SetVisible( false );
}

itemid_t CCSGO_HudRosettaSelector::GetEquippedItemID() const
{
	itemid_t iItemID = INVALID_ITEM_ID;
// 	const char *szEquippedItemID = CUiComponent_Loadout::GetInstance()->GetItemID( "noteam", "spray0" );
// 	// Player has sprays but none equipped. Tell them to equip the spray
// 	if ( !szEquippedItemID || V_strlen( szEquippedItemID ) == 0 )
	{
		return iItemID;
	}

//	return (itemid_t)V_atoi64( szEquippedItemID );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCSGO_HudRosettaSelector::InvItemAndEquippedItemAreTheSame( itemid_t iInventoryItemID )
{
	const char *szEquippedItemID = "spray0";// CUiComponent_Loadout::GetInstance()->GetItemID( "noteam", "spray0" );

	itemid_t iEquippedItemID = ( szEquippedItemID && *szEquippedItemID ) ? (uint64)Q_atoi64( szEquippedItemID ) : uint64( 0 );

	return ( iEquippedItemID == iInventoryItemID );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCSGO_HudRosettaSelector::EventSetAutoToggle()
{
//	cl_playerspray_auto_apply.SetValue( m_pQuickSprayToggle->IsSelected() );

	UpdateAutoToggle();

	// disable cursor after an action
	EnableCursor( false );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCSGO_HudRosettaSelector::InventoryPrev()
{
	m_nActiveIndex -= m_nItemTiles;
	m_nPage--;

	EnableDisablePageButtons();
	SetItems();

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCSGO_HudRosettaSelector::InventoryNext()
{
	m_nActiveIndex += m_nItemTiles;
	m_nPage++;

	EnableDisablePageButtons();
	SetItems();

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRosettaSelector::EnableDisablePageButtons()
{
	int numMaxPages = ceil( (double)m_nTotalSprays / (double)m_nItemTiles );

	if ( m_nPage >= numMaxPages )
		m_pInventoryNext->SetOpacity( 0.0f );
	else
		m_pInventoryNext->SetOpacity( 1.0f );

	if ( m_nPage <= 1 )
		m_pInventoryPrev->SetOpacity( 0.0f );
	else
		m_pInventoryPrev->SetOpacity( 1.0f );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRosettaSelector::ResetPages()
{
	int numCurrentPage = ceil( (double)m_nTotalSprays / (double)m_nItemTiles );

	if ( numCurrentPage < m_nPage )
	{
		m_nPage = 1;
		m_nActiveIndex = 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGO_HudRosettaSelector::SetItems()
{
	for ( int i = 0; i < m_nItemTiles; i++ )
	{
		CRosettaInventoryItemPanel *pInventoryItem = m_aInventorySlots[ i ];

		if ( ( i + m_nActiveIndex ) >= m_nTotalSprays )
		{
			pInventoryItem->m_pSlot->SetVisible( false );
		}
		else
		{
			pInventoryItem->m_pSlot->SetVisible( true );

// 			pInventoryItem->m_itemID = CUiComponent_Inventory::GetInstance()->GetInventoryItemIDByIndex( m_nActiveIndex + i );
// 
// 			// Get name and set it
// 			const wchar_t *szSprayName = CUiComponent_Inventory::GetInstance()->GetItemName( pInventoryItem->m_itemID );
// 			const char *szColor = CUiComponent_Inventory::GetInstance()->GetItemRarityColor( pInventoryItem->m_itemID );
// 
// 			char tmpStr[ 128 ];
// 			V_UnicodeToUTF8( szSprayName, tmpStr, sizeof( tmpStr ) );
// 			pInventoryItem->m_pSlot->SetDialogVariable( "item_color", szColor );
// 			pInventoryItem->m_pSlot->SetDialogVariable( "item_name", SeparateName( tmpStr ) );
// 
// 			// get uses remaining and set it
// 			uint32 numUsesRemaining = CUiComponent_Inventory::GetInstance()->GetItemAttributeValue( pInventoryItem->m_itemID, "sprays remaining" );
// 			pInventoryItem->m_pSlot->SetDialogVariable( "item_count", ( int )numUsesRemaining );
// 
// 			// Spray icon
// 			LoadSprayIcon( pInventoryItem->m_pImage, pInventoryItem->m_itemID );
// 
// 			if ( InvItemAndEquippedItemAreTheSame( pInventoryItem->m_itemID ) )
// 			{
// 				pInventoryItem->m_pSelection->SetOpacity( 1.0f );
// 			}
// 			else
// 			{
// 				pInventoryItem->m_pSelection->SetOpacity( 0.0f );
// 			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
char *CCSGO_HudRosettaSelector::SeparateName( char *szSprayName )
{
	// because the code return names with inconsistent spacing this protects the name from displaying and being split incorrectly

	char *szSplitStr;

	if ( ( szSplitStr = V_strstr( szSprayName, "|  " ) ) != nullptr )
		return szSplitStr + 3;

	if ( ( szSplitStr = V_strstr( szSprayName, "| " ) ) != nullptr )
		return szSplitStr + 2;

	return szSprayName;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------

void CCSGO_HudRosettaSelector::EnableCursor( bool bEnable )
{
	if ( bEnable )
	{
		// enable cursor
		m_bEnableCursor = true;

		// disable input to game
		AssertMsgAlways( !m_hDenyInputToGame, "RosettaSelector - Make sure to call PanoramaReleaseDenyMouseInputToGame before calling PanoramaAddDenyMouseInputToGame again\n" );
		m_hDenyInputToGame = gameuifuncs->PanoramaAddDenyMouseInputToGame( this->UIPanel(), "HudRosettaSelector" );

		// disable hint
		m_pSprayInfo->SetVisible( false );

		m_pInventory->SetOpacity( 1.0f );
		m_pBGGradient->SetOpacity( 1.0f );
	}
	else
	{
		// disable cursor
		m_bEnableCursor = false;

		// enable input to game
		if ( m_hDenyInputToGame )
		{
			gameuifuncs->PanoramaReleaseDenyMouseInputToGame( m_hDenyInputToGame );
			m_hDenyInputToGame = 0;
		}

		// enable hint
		m_pSprayInfo->SetVisible( true );

		m_pInventory->SetOpacity( 0.6f );
		m_pBGGradient->SetOpacity( 0.6f );
	}
}

bool CCSGO_HudRosettaSelector::EventOnMouseEnableBinding()
{
	// only allow to enable if we have any spray
	if ( m_bVisible && m_nTotalSprays > 0 )
	{
		EnableCursor( !m_bEnableCursor );
	}

	// prevent event from making it to game
	return true;
}

// Return true to swallow this key
bool CCSGO_HudRosettaSelector::KeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding )
{
	if ( down && pszCurrentBinding && ContainsBinding( pszCurrentBinding, "+attack", true ) )
	{
		if ( Helper_CanShowPreviewDecal() )
		{
			// If we're pretty sure this will result in a successful spray application, tell the server we want to spray
			// the currently equipped item and close the menu
			PlayerDecalDataSendActionSprayToServer( 0 );
			SetShowRosetta( false, "spray" );
		}
		else // keep menu up and play error sound
		{
			enginesound->EmitAmbientSound( g_pszSprayErrorSound, 1.0f );
		}

		// always swallow the input if this menu is up
		return true;
	}

	return false;
}

