//========= Copyright (C) Valve Corporation, All rights reserved. ============//
//
// Component for BuyMenu access. Only used by panorama and mirrors the buymenu_scaleform singleton in SF.
//
//=============================================================================//


#include "cbase.h"

#include "csgo_buymenu.h"

#include "c_cs_player.h"
#include "cs_gamerules.h"
#include "c_cs_playerresource.h"
#include "weapon_selection.h"
#include "IGameUIFuncs.h"
#include "inputsystem/iinputsystem.h"
#include "econ/econ_item_description.h"
#include "csgo_teamselectmenu.h"
#include "uicomponents/uicomponent_gamestate.h"
#include "vguicenterprint.h"
#include "c_cs_team.h"
#include "csgo_avatarimage.h"
#include "clientmode_csnormal.h" // CSGOFrameUpdate
#include "ammodef.h"
//#include "panorama/iuiengine.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

DEFINE_PANORAMA_EVENT( EventOpenBuyMenu );
DEFINE_PANORAMA_EVENT( EventCloseBuyMenu );
DEFINE_PANORAMA_EVENT( LocalPlayerMoneyChanged );
DEFINE_PANORAMA_EVENT( LocalPlayerBuyZoneChange );
DEFINE_PANORAMA_EVENT( PlayerEquipmentChange );

DECLARE_PANORAMA_EVENT1( BuyMenu_ActivateBuyWheel, CCSGO_BuyMenu::BuyWheelCategory )
DEFINE_PANORAMA_EVENT( BuyMenu_ActivateBuyWheel );

DECLARE_PANORAMA_EVENT1( BuyMenu_PurchaseSlot, loadout_positions_t )
DEFINE_PANORAMA_EVENT( BuyMenu_PurchaseSlot );

DECLARE_PANORAMA_EVENT1(BuyMenu_PurchaseFailSlot, loadout_positions_t)
DEFINE_PANORAMA_EVENT(BuyMenu_PurchaseFailSlot);

DECLARE_PANORAMA_EVENT0( BuyMenu_Back )
DEFINE_PANORAMA_EVENT( BuyMenu_Back );

DECLARE_PANORAMA_EVENT1( BuyMenu_ActivateWedge, int )
DEFINE_PANORAMA_EVENT( BuyMenu_ActivateWedge )

REGISTER_PANEL2D_FACTORY( CCSGO_BuyMenu, CSGOBuyMenu )

#define CSGO_BUYMENU_CATEGORY_ICON_SIZE 48
#define CSGO_BUYMENU_EQUIP_ICON_SIZE 24
#define CSGO_BUYMENU_EQUIP_PRIMARY_ICON_SIZE 64
#define CSGO_BUYMENU_EQUIP_PRIMARY_TEAMMATE_ICON_SIZE 48

namespace
{
	CPanoramaSymbolLazyInit k_symFlip( "team-equip__flip" );
	CPanoramaSymbolLazyInit k_symItemGrenade( "team-equip__item-grenade" );
	CPanoramaSymbolLazyInit k_symNoHover( "no-hover" );
	CPanoramaSymbolLazyInit k_symLabelUpper( "buywheel-category__label--UPPERCASE" );
	CPanoramaSymbolLazyInit k_symPriceLabel( "buywheel-cant-afford" );
	CPanoramaSymbolLazyInit k_symCantBuy( "buywheel-cant-buy" );
	CPanoramaSymbolLazyInit k_symPreviewContainer( "preview-container" );
	CPanoramaSymbolLazyInit k_symPreviewContainerHidden( "preview-container--hidden" );
	CPanoramaSymbolLazyInit k_symPreviewContainerCategoryPreview( "preview-container--preview" );
	CPanoramaSymbolLazyInit k_symPreviewContainerItemDetails( "preview-container--details" );
	CPanoramaSymbolLazyInit k_symWeaponImage( "weapon-icons__image" );
	CPanoramaSymbolLazyInit k_symRarityClass( "rarity_class" );
}

using namespace panorama;

CCSGO_BuyMenu::CCSGO_BuyMenu( CPanel2D *pParent, const char *pchID )
	: CPanel2D( pParent, pchID )
	, m_Capture( this, "BuyMenu", k_EGameInputShareMouse| k_EGameInputUIEnableKeyInput )
{
	COMPILE_TIME_ASSERT( k_eCategoryCount == 6 );

	// Make sure buymenu panel has its own input hierarchy and therefore will not lose focus
	// when pushing another input context (peer panels such as scoreboard)
	SetTopOfInputContext( true );
	
	SetInputNamespace( "buymenu" );

	SetAcceptsInput( true );
	SetAcceptsFocus( true );

	BLoadLayout( "file://{resources}/layout/buymenu.xml" );
	CreateBuyWheels();
	m_pCountdownTimer = panel_cast< CCSGO_CountdownTimer * > ( FindChildInLayoutFile( "Countdown" ) );
	m_pItemInfo = panel_cast< CPanel2D * > ( FindChildInLayoutFile( "PreviewPanelContainer" ), true );
	m_pTeamEquipment = panel_cast< CPanel2D * > ( FindChildInLayoutFile( "TeamEquipment" ), true );
	m_pLocalPlayerEquipment = panel_cast< CPanel2D * > ( FindChildInLayoutFile( "LocalPlayerEquipment" ), true );
	m_pItemPreview = panel_cast< CUI_ItemPreviewPanel * > ( FindChildInLayoutFile( "BuyMenuItemModel" ), true );
	m_pPurchaseFailureLabel = panel_cast< CLabel * > ( FindChildInLayoutFile( "PurchaseFailureLabel" ), true );
	m_pMoneyPanel = panel_cast< CCSGOMoneyPanel * > ( FindChildInLayoutFile( "BuyMenuMoney" ), true );

	RegisterForUnhandledEvent( EventOpenBuyMenu(), this, &CCSGO_BuyMenu::OpenBuyMenu );
	RegisterForUnhandledEvent( EventCloseBuyMenu(), this, &CCSGO_BuyMenu::CloseBuyMenu );
	RegisterForUnhandledEvent( GameState_ServerSpawn(), this, &CCSGO_BuyMenu::EventServerSpawn );
}

CCSGO_BuyMenu::~CCSGO_BuyMenu()
{
}

// Only listening to most events when the buy menu is visible
void CCSGO_BuyMenu::HookEvents()
{
	RegisterEventHandler( BuyMenu_ActivateBuyWheel(), this, &CCSGO_BuyMenu::EventSelectCategory );
	RegisterEventHandler( BuyMenu_PurchaseSlot(), this, &CCSGO_BuyMenu::EventPurchaseSlot );
	RegisterEventHandler( BuyMenu_PurchaseFailSlot(), this, &CCSGO_BuyMenu::EventPurchaseFailSlot );
	RegisterEventHandler( BuyMenu_Back(), this, &CCSGO_BuyMenu::EventNavigateBack );
	RegisterForUnhandledEvent( PlayerTeamChanged(), this, &CCSGO_BuyMenu::EventTeamChange );
	RegisterForUnhandledEvent( LocalPlayerMoneyChanged(), this, &CCSGO_BuyMenu::EventLocalPlayerMoneyChanged );
	RegisterForUnhandledEvent( LocalPlayerBuyZoneChange(), this, &CCSGO_BuyMenu::EventLocalPlayerBuyZoneChange );
	RegisterForUnhandledEvent( WindowGotFocus(), this, &CCSGO_BuyMenu::EventWindowGotFocus );
	RegisterForUnhandledEvent( BuyMenu_ActivateWedge(), this, &CCSGO_BuyMenu::EventActivateWedge );
	RegisterForUnhandledEvent( PlayerEquipmentChange(), this, &CCSGO_BuyMenu::EventPlayerEquipmentChange );
	RegisterEventHandler( RadialSelectorHoverPanelChange(), this, &CCSGO_BuyMenu::EventHoverPanelChange );
	RegisterForUnhandledEvent( CSGOFrameUpdate(), this, &CCSGO_BuyMenu::EventFrameUpdate );
}

void CCSGO_BuyMenu::UnhookEvents()
{
	UnregisterEventHandler( BuyMenu_ActivateBuyWheel(), this, &CCSGO_BuyMenu::EventSelectCategory );
	UnregisterEventHandler( BuyMenu_PurchaseSlot(), this, &CCSGO_BuyMenu::EventPurchaseSlot );
	UnregisterEventHandler( BuyMenu_PurchaseFailSlot(), this, &CCSGO_BuyMenu::EventPurchaseFailSlot );
	UnregisterEventHandler( BuyMenu_Back(), this, &CCSGO_BuyMenu::EventNavigateBack );
	UnregisterForUnhandledEvent( PlayerTeamChanged(), this, &CCSGO_BuyMenu::EventTeamChange );
	UnregisterForUnhandledEvent( LocalPlayerMoneyChanged(), this, &CCSGO_BuyMenu::EventLocalPlayerMoneyChanged );
	UnregisterForUnhandledEvent( LocalPlayerBuyZoneChange(), this, &CCSGO_BuyMenu::EventLocalPlayerBuyZoneChange );
	UnregisterForUnhandledEvent( WindowGotFocus(), this, &CCSGO_BuyMenu::EventWindowGotFocus );
	UnregisterForUnhandledEvent( BuyMenu_ActivateWedge(), this, &CCSGO_BuyMenu::EventActivateWedge );
	UnregisterForUnhandledEvent( PlayerEquipmentChange(), this, &CCSGO_BuyMenu::EventPlayerEquipmentChange );
	UnregisterEventHandler( RadialSelectorHoverPanelChange(), this, &CCSGO_BuyMenu::EventHoverPanelChange );
	UnregisterForUnhandledEvent( CSGOFrameUpdate(), this, &CCSGO_BuyMenu::EventFrameUpdate );
}

void CCSGO_BuyMenu::OnLayoutReloading()
{
	CloseBuyMenu();
}

void CCSGO_BuyMenu::OnLayoutReloaded()
{
	//BUG: Only this panel gets lost on an f8 reload... Worth investigating, but is only a problem for development. 
	m_pLocalPlayerEquipment = panel_cast< CPanel2D * > ( FindChildInLayoutFile( "LocalPlayerEquipment" ), true );
}

void CCSGO_BuyMenu::CreateCategoryPanels( CPanel2D *pParent, CButton* pButton, BuyWheelCategory nCategory, loadout_positions_t nLoadoutFirst, loadout_positions_t nLoadoutLast )
{
	pButton->SetHasClass( k_symNoHover, false );

	BuyWheelInfo_t &cat = m_buyWheels[ nCategory ];
	cat.nLoadoutFirst = nLoadoutFirst;
	cat.nLoadoutLast = nLoadoutLast;
	cat.szName = g_szLoadoutStrings[ nLoadoutFirst ];
	cat.szDisplayName = g_szLoadoutStringsForDisplay[ nLoadoutFirst ];
	CreateBuyWheelPanels( pParent, cat );

	CPanel2D* pContents = pButton->RequireChild( "Contents" );
	pContents->RequireLoadLayoutSnippet( "CategoryWedge" );
	if ( CLabel* pLabel = panel_cast< CLabel* > ( pContents->FindChildTraverse( "CategoryName" ) ) )
	{
		pLabel->SetText( m_buyWheels[ nCategory ].szDisplayName );
		pLabel->AddClass( k_symLabelUpper );
	}
	if ( CLabel* pLabel = panel_cast< CLabel* > ( pContents->FindChildTraverse( "WedgeKeybinding" ) ) )
	{
		pLabel->SetText( m_szKeyBindings[nCategory] );
	}
	pButton->SetAttribute( "category", ( int )nCategory );

	pButton->SetOnActivateEvent( BuyMenu_ActivateBuyWheel::MakeEvent( this, nCategory ) );
}

void CCSGO_BuyMenu::CreateBuyWheelPanels( CPanel2D * pParent, BuyWheelInfo_t & wheelInfo )
{
	int nNumItems = wheelInfo.nLoadoutLast - wheelInfo.nLoadoutFirst + 1; // First through last inclusive
	Assert( nNumItems <= 6 && nNumItems >= 4 ); // Radial menus only support 4, 5 or 6 items at a time for legacy reasons. 
	CCSGO_RadialSelector::ELayoutType nLayout = ( nNumItems > 4 ) ? CCSGO_RadialSelector::k_eSixChoices : CCSGO_RadialSelector::k_eFourChoices;
	wheelInfo.m_pBuyWheel = new CCSGO_RadialSelector( pParent, CFmtStr( "BuyWheel_%s", wheelInfo.szName ).Get(), nLayout );

	for ( int i = 0; i < nNumItems; ++i )
	{
		// This button buys for the current loadout slot
		loadout_positions_t nLoadoutSlot = ( loadout_positions_t )( wheelInfo.nLoadoutFirst + i );
		CPanel2D* pBuyButton = wheelInfo.m_pBuyWheel->GetChild( i );
		pBuyButton->SetOnActivateEvent( BuyMenu_PurchaseSlot::MakeEvent( this, nLoadoutSlot ) );
		pBuyButton->SetAttribute( "loadout_pos", (int)nLoadoutSlot );

		pBuyButton->SetHasClass( k_symNoHover, false );

		// Load snippet for panel contents
		CPanel2D* pContents = pBuyButton->RequireChild( "Contents" );
		pContents->RequireLoadLayoutSnippet( "ItemWedge" );

		if ( CLabel* pLabel = panel_cast< CLabel* > ( pContents->FindChildTraverse( "WedgeKeybinding" ) ) )
		{
			pLabel->SetText( m_szKeyBindings[i] );
		}

		// save off handles to panels we need to update
		ItemPanelsForUpdate_t &dlgs = wheelInfo.m_vecItems[ wheelInfo.m_vecItems.AddToTail() ];
		dlgs.m_nSlot = nLoadoutSlot;
		dlgs.m_pItemIcon = panel_cast< CImagePanel * > ( pContents->RequireChildTraverse( "ItemIcon" ), true );
		dlgs.m_pItemName = panel_cast< CLabel* > ( pContents->RequireChildTraverse( "ItemName" ), true );
		dlgs.m_pItemPrice = panel_cast< CLabel* > ( pContents->RequireChildTraverse( "ItemPrice" ), true );
		dlgs.m_pItemButton = panel_cast< CButton* > ( pBuyButton );
	}
	wheelInfo.m_pBuyWheel->Disable();
}

void CCSGO_BuyMenu::RefreshItems( void )
{
	CCSPlayerInventory* pInv = CSInventoryManager()->GetLocalCSInventory();
	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pInv || !pPlayer )
		return;

	m_pTeamLogo->SetImage( CFmtStr( "file://{images}/icons/%s.svg", pPlayer->GetTeamNumber() == TEAM_TERRORIST ? "t_logo" : "ct_logo" ).Get() );


	for ( int i = 0; i < k_eCategoryCount; ++i )
	{
		BuyWheelInfo_t &cat = m_buyWheels[ i ];
		bool bCanAffordAnyItem = false;
		FOR_EACH_VEC( cat.m_vecItems, j )
		{
			ItemPanelsForUpdate_t &dlgs = cat.m_vecItems[ j ];
			const CEconItemView *pItem = pInv->GetItemInLoadoutFilteredByProhibition( pPlayer->GetTeamNumber(), dlgs.m_nSlot );
			if ( pItem != dlgs.m_pItem && dlgs.m_pItemIcon )
			{
				
				dlgs.m_pItemIcon->SetImage( CFmtStr( "file://{images}/icons/equipment/%s.svg", pItem->GetItemDefinition()->GetHudIconFilename() ).Get(), nullptr, false, -1, CSGO_BUYMENU_CATEGORY_ICON_SIZE );
				dlgs.m_pItemIcon->SetScaling( k_EImageScalingStretchBothToFitPreserveAspectRatio );
				if ( pItem->GetItemDefinition() == ITEM_DEFUSER )
				{
					dlgs.m_pItemName->SetText( CSGameRules()->IsHostageRescueMap() ? "#SFUI_WPNHUD_CUTTERS" : "#SFUI_WPNHUD_DEFUSER" );
				}
				else
				{
					dlgs.m_pItemName->SetText( pItem->GetItemDefinition()->GetItemBaseName() );
				}
			}

			if ( pItem && pItem->IsValid() )
			{
				int nPrice = pPlayer->GetWeaponPrice( pItem->GetItemDefinition()->GetDefinitionIndex(), pItem );
				bool bCanAffordItem = true;
				if ( !AreWeaponsFree() )
				{
					bCanAffordItem = pPlayer->GetAccount() >= nPrice;
					dlgs.m_pItemPrice->SetText( CFmtStr( "$%d", nPrice ).Get() );
					dlgs.m_pItemButton->SetHasClass( k_symPriceLabel, !bCanAffordItem );
				}

				AcquireResult::Type canAcquire = pPlayer->CanAcquire( pItem, AcquireMethod::Buy );

				// If we can't afford or can't purchase then set the button's activate event to play a 'buy fail'
				// sound. We set the buywheel-cant-buy css class so that the button can be styled appropriately.
				// This is effectively disabling the button but still allowing a sound to play when the user
				// attempts to activate it.
				if ( bCanAffordItem && AcquireResult::Allowed == canAcquire )
				{
					dlgs.m_pItemButton->SetHasClass(k_symCantBuy, false);
					dlgs.m_pItemButton->SetOnActivateEvent( BuyMenu_PurchaseSlot::MakeEvent( this, dlgs.m_nSlot ) );
				}
				else
				{
					dlgs.m_pItemButton->SetHasClass(k_symCantBuy, true);
					dlgs.m_pItemButton->SetOnActivateEvent(BuyMenu_PurchaseFailSlot::MakeEvent(this, dlgs.m_nSlot));
				}

				if ( bCanAffordItem )
					bCanAffordAnyItem = true;
			}
			else
			{
				dlgs.m_pItemName->SetText( "" );
				dlgs.m_pItemPrice->SetText( "" );
			}
		}

		m_pCategoryWheel->GetChild( i )->SetHasClass( k_symPriceLabel, !bCanAffordAnyItem );
	}
}

bool Helper_SetImagePanelToWeapon( CPanel2D* pParent, const char* szPanelID, C_BaseCombatWeapon* pWeapon, int nImageHeight )
{
	CImagePanel* pItemImage = panel_cast<CImagePanel*>( pParent->FindChildInLayoutFile( szPanelID ), true );
	if ( !pItemImage )
		return false;

	if ( !pWeapon )
	{
		pItemImage->Clear();
		pItemImage->SetVisible( false );
		return false;
	}

	pItemImage->SetImage( CFmtStr( "file://{images}/icons/equipment/%s.svg", pWeapon->GetEconItemView()->GetItemDefinition()->GetHudIconFilename() ).Get(), nullptr, false, -1, nImageHeight );
	pItemImage->SetVisible( true );
	return true;
}

bool Helper_RefreshTeammateEquipment( CPanel2D *pTeamEquipPanel, C_CSPlayer* pPlayer, bool bIsCurrentPlayer )
{
	CImagePanel* pSkull = panel_cast< CImagePanel* >( pTeamEquipPanel->FindChildInLayoutFile( "Skull" ) );
	pSkull->SetVisible( pPlayer->m_lifeState != LIFE_ALIVE );
	pTeamEquipPanel->SetDialogVariable( "money", ( int )pPlayer->GetAccount() );
	Helper_SetImagePanelToWeapon( pTeamEquipPanel, "Primary", pPlayer->Weapon_GetSlot( GEAR_SLOT_RIFLE ), bIsCurrentPlayer ? CSGO_BUYMENU_EQUIP_PRIMARY_ICON_SIZE : CSGO_BUYMENU_EQUIP_PRIMARY_TEAMMATE_ICON_SIZE );
	Helper_SetImagePanelToWeapon( pTeamEquipPanel, "Secondary", pPlayer->Weapon_GetSlot( GEAR_SLOT_PISTOL ), CSGO_BUYMENU_EQUIP_ICON_SIZE );
	Helper_SetImagePanelToWeapon( pTeamEquipPanel, "Bomb", pPlayer->Weapon_GetSlot( GEAR_SLOT_C4 ), CSGO_BUYMENU_EQUIP_ICON_SIZE );

	// Populate lots of grenades, throw taser in this bucket
	CPanel2D *pOtherEquipment = panel_cast< CPanel2D* >( pTeamEquipPanel->FindChildInLayoutFile( "Grenades" ) );
	pOtherEquipment->RemoveAndDeleteChildren();
	for ( int j = 0; j < pPlayer->WeaponCount(); ++j )
	{
		C_BaseCombatWeapon *pItem = pPlayer->GetWeapon( j );
		if ( !pItem )
			continue;

		if ( !pItem->GetEconItemView()->IsKnife() && ( pItem->GetGearSlot() == GEAR_SLOT_KNIFE || pItem->GetGearSlot() == GEAR_SLOT_GRENADES ) )
		{
			CImagePanel* pImage = new CImagePanel( pOtherEquipment, CFmtStr( "item%d", j ).Get() );
			pImage->SetImage( CFmtStr( "file://{images}/icons/equipment/%s.svg", pItem->GetEconItemView()->GetItemDefinition()->GetHudIconFilename() ).Get(), nullptr, false, -1, CSGO_BUYMENU_EQUIP_ICON_SIZE );
			pImage->SetHasClass( k_symItemGrenade, true );
			pImage->SetHasClass( k_symFlip, true );		
			pImage->SetScaling(k_EImageScalingStretchBothToFitPreserveAspectRatio);
		}
	}

	CImagePanel *pDefuser = panel_cast< CImagePanel* >( pTeamEquipPanel->FindChildInLayoutFile( "Defuser" ) );
	pDefuser->SetVisible( pPlayer->HasDefuser() );

	CImagePanel *pVest = panel_cast< CImagePanel* >( pTeamEquipPanel->FindChildInLayoutFile( "Vest" ) );
	CImagePanel *pVestHelm = panel_cast< CImagePanel* >( pTeamEquipPanel->FindChildInLayoutFile( "VestHelm" ) );
	CImagePanel *pHeavyArmor = panel_cast< CImagePanel* >( pTeamEquipPanel->FindChildInLayoutFile( "HeavyAssault" ) );
	pVest->SetVisible( false );
	pVestHelm->SetVisible( false );
	pHeavyArmor->SetVisible( false );
	if ( pPlayer->ArmorValue() > 0 )
	{
		CFmtStr strArmorIcon;
		if ( pPlayer->HasHeavyAssaultSuit() )
			pHeavyArmor->SetVisible( true );
		else if ( pPlayer->HasHelmet() )
			pVestHelm->SetVisible( true );
		else
			pVest->SetVisible( true );
	}

	return true;
}

void CCSGO_BuyMenu::CreateTeamEquipmentPanels(void)
{
	m_pTeamEquipment->RemoveAndDeleteChildren();
	C_CSPlayer* pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	for ( int i = 1; i < gpGlobals->maxClients; ++i )
	{
		C_CSPlayer* pTeammate = ToCSPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pTeammate || pTeammate->IsDormant() || ( pTeammate->GetTeamNumber() != pLocalPlayer->GetTeamNumber() ) )
			continue;

		CPanel2D* pEquipPanel = nullptr;
		if ( pTeammate == pLocalPlayer )
		{
			pEquipPanel = m_pLocalPlayerEquipment.Get();
			pEquipPanel->RemoveAndDeleteChildren();
		}
		else
		{
			pEquipPanel = new CPanel2D( m_pTeamEquipment.Get(), CFmtStr( "equip_player%d", i ).Get() );
		}
		pEquipPanel->BLoadLayoutSnippet( "TeamEquipment" );
		pEquipPanel->SetAttribute( "userid", pTeammate->GetUserID() );

		CCSGO_AvatarImage* pAvatar = panel_cast< CCSGO_AvatarImage* >( pEquipPanel->FindChildTraverse( "Avatar" ) );
		int nTeam = pTeammate->GetTeamNumber();
		if ( nTeam == TEAM_CT )
		{
			pAvatar->SetDefaultImage( "file://{images}/hud/freezepanel/default_CT_42_alt.png" );
		}
		else if ( nTeam == TEAM_TERRORIST )
		{
			pAvatar->SetDefaultImage( "file://{images}/hud/freezepanel/default_T_42_alt.png" );
		}

		CImagePanel* pTeamColor = panel_cast< CImagePanel* >( pEquipPanel->FindChildInLayoutFile( "PlayerColor" ) );
		pTeamColor->SetVisible( false );
		if ( pLocalPlayer && pLocalPlayer->ShouldShowTeamPlayerColors( pTeammate->GetTeamNumber() ) )
		{
			panorama::IUIPanelStyle *pPanelStyle = pTeamColor->AccessStyle();
			int nColorID = GetCSResources()->GetCompTeammateColor( pTeammate->entindex() );
			if ( nColorID >= 0 )
			{
				pPanelStyle->SetSimpleWashColor( GetCSResources()->GetCompPlayerColorByID( nColorID ) );
				pTeamColor->SetVisible( true );
			}
		}


		if( !pTeammate->IsBot() )
		{
			CSteamID steamID;
			if(	pTeammate->GetSteamID( &steamID ) )
				pAvatar->SetSteamID( steamID );
		}

		Helper_RefreshTeammateEquipment( pEquipPanel, pTeammate, ( pTeammate == pLocalPlayer ) );
	}
}

bool CCSGO_BuyMenu::AreWeaponsFree() const
{
	return CSGameRules() && CSGameRules()->IsPlayingGunGameDeathmatch();
}

bool CCSGO_BuyMenu::EventSelectCategory( BuyWheelCategory nCategory )
{
	m_pCategoryWheel->Disable();
	BuyWheelInfo_t &cat = m_buyWheels[ nCategory ];
	cat.m_pBuyWheel->Enable();
	m_pVisibleBuyWheel = cat.m_pBuyWheel;
	GameUI().PlayUISoundScript( "buymenu_select" );
	return true;
}

extern ConVar closeonbuy;
bool CCSGO_BuyMenu::EventPurchaseSlot( loadout_positions_t nSlot )
{
	GameUI().PlayUISoundScript( "buymenu_purchase" );
	// NOTE: The param count sent to 'buy' determines if we're buying by name (eg 'cz75') or by slot number. When buying by slot the first
	// string param is ignored, so it can be anything.
	engine->ClientCmd( CFmtStr( "buy unused %d", nSlot ).Get() );
	if ( closeonbuy.GetBool() || ( CSGameRules() && CSGameRules()->IsPlayingGunGameDeathmatch() ) )
	{
		CloseBuyMenu();
	}
	else if ( ( m_pVisibleBuyWheel != m_buyWheels[ k_eGrenade ].m_pBuyWheel ) &&
			  ( m_pVisibleBuyWheel != m_buyWheels[ k_eGear ].m_pBuyWheel ) )
	{
		DispatchEvent( BuyMenu_Back(), this );
	}
	return true;
}

bool CCSGO_BuyMenu::EventPurchaseFailSlot(loadout_positions_t nSlot)
{
	GameUI().PlayUISoundScript("buymenu_failure");

	return true;
}

bool CCSGO_BuyMenu::EventNavigateBack( void )
{
	if ( m_pVisibleBuyWheel.Get() == nullptr )
	{
		CloseBuyMenu();
	}
	else
	{
		m_pVisibleBuyWheel->Disable();
		m_pVisibleBuyWheel = nullptr;
		m_pCategoryWheel->Enable();
	}
	return true;
}

bool CCSGO_BuyMenu::EventTeamChange( uint64 id, int32 nOldTeam, int32 nNewTeam )
{
	CSteamID steamID;
	if ( C_CSPlayer::GetLocalCSPlayer() && C_CSPlayer::GetLocalCSPlayer()->GetSteamID( &steamID ) && steamID.ConvertToUint64() == id )
	{
		RefreshItems();
	}
	return false;
}

bool CCSGO_BuyMenu::EventLocalPlayerMoneyChanged( int iCurrentMoney )
{
	m_pLocalPlayerEquipment->SetDialogVariable( "money", ( int )C_CSPlayer::GetLocalCSPlayer()->GetAccount() );
	RefreshItems();
	return false;
}

bool CCSGO_BuyMenu::EventLocalPlayerBuyZoneChange( bool bInBuyZone )
{
	if ( !bInBuyZone )
	{
		CloseBuyMenu();
		GetCenterPrint()->Print( "#SFUI_BuyMenu_NotInBuyZone" );
	}

	return false;
}

bool CCSGO_BuyMenu::EventServerSpawn()
{
	CloseBuyMenu();
	RefreshItems();
	return false;
}

bool CCSGO_BuyMenu::EventWindowGotFocus( IUIWindow * pTopLevelWindow )
{
	Assert( BIsVisible() );
	SetFocus();
	return false;
}

bool CCSGO_BuyMenu::EventActivateWedge( int nWedgeNum )
{
	CCSGO_RadialSelector* pWheel = ( m_pVisibleBuyWheel.Get() ) ? m_pVisibleBuyWheel.Get() : m_pCategoryWheel.Get();
	if ( CPanel2D * pChild = pWheel->GetChild( nWedgeNum ) )
	{
		DispatchEvent( Activated(), pChild, EPanelEventSource_t::k_ePanelEventSourceProgram );
	}
	return true;
}

bool CCSGO_BuyMenu::EventHoverPanelChange( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, panorama::CPanelPtr< panorama::CPanel2D > pNewHover )
{
	// If we're showing categories, hide by default. For buy wheels we want to leave the last hovered info up. 
	if ( !m_pVisibleBuyWheel )
		m_pItemInfo->SwitchClass( k_symPreviewContainer, k_symPreviewContainerHidden );
	m_pHoveredWedge = pNewHover;
	UpdatePurchaseFailureLabel();
	if ( pNewHover.Get() )
	{
		GameUI().PlayUISoundScript( "buymenu_mouseover" );
		if ( m_pCategoryWheel->BIsVisible() )
		{
			int nCategory = pNewHover->GetAttribute( "category", -1 );
			if ( nCategory == -1 )
				return false;

			int nPosStart = m_buyWheels[ nCategory ].nLoadoutFirst;
			int nPosEnd = m_buyWheels[ nCategory ].nLoadoutLast;
			// PERF: Can create these images and cache off once, if needed
			if ( CPanel2D* pWeaponIconContainer = FindChildTraverse( "CategoryPreview" ) )
			{
				m_pItemInfo->SetDialogVariableLocString( "category", m_buyWheels[ nCategory ].szDisplayName );
				pWeaponIconContainer->RemoveAndDeleteChildren();
				for ( int i = nPosStart; i <= nPosEnd; ++i )
				{
					CImagePanel *pIcon = new CImagePanel( pWeaponIconContainer, nullptr );
					pIcon->AddClass( k_symWeaponImage );
					pIcon->SetScaling( k_EImageScalingStretchBothToFitPreserveAspectRatio );
					CCSPlayerInventory *pInv = CSInventoryManager()->GetLocalCSInventory();
					C_CSPlayer *pPlayer = CCSPlayer::GetLocalCSPlayer();
					if ( !pInv || !pPlayer )
						return false;

					const CEconItemView *pItem = pInv->GetItemInLoadoutFilteredByProhibition( pPlayer->GetTeamNumber(), i );
					if ( pItem && pItem->IsValid() )
					{
						pIcon->SetImage( CFmtStr( "file://{images}/icons/equipment/%s.svg", pItem->GetItemDefinition()->GetHudIconFilename() ).Get(), nullptr, false, -1, 48 );

						if ( !AreWeaponsFree() )
						{
							int nPrice = pPlayer->GetWeaponPrice( pItem->GetItemDefinition()->GetDefinitionIndex(), pItem );
							bool bCanAffordItem = pPlayer->GetAccount() >= nPrice;
							pIcon->SetHasClass( k_symPriceLabel, !bCanAffordItem || !( AcquireResult::Allowed == pPlayer->CanAcquire( pItem, AcquireMethod::Buy ) ) );
						}
					}
				}
				m_pItemInfo->SwitchClass( k_symPreviewContainer, k_symPreviewContainerCategoryPreview );
				return true;
			}
		}
		else
		{
			loadout_positions_t pos = ( loadout_positions_t )pNewHover->GetAttribute( "loadout_pos", ( int )LOADOUT_POSITION_INVALID );
			if ( OnPurchaseButtonHover( pos ) )
			{
				m_pItemInfo->SwitchClass( k_symPreviewContainer, k_symPreviewContainerItemDetails );
				return true;
			}
		}
	}	
	return false;
}

bool CCSGO_BuyMenu::EventPlayerEquipmentChange( C_CSPlayer *pPlayer )
{
	if ( C_CSPlayer::GetLocalCSPlayer() && C_CSPlayer::GetLocalCSPlayer()->GetTeamNumber() == pPlayer->GetTeamNumber() )
	{
		if ( C_CSPlayer::GetLocalCSPlayer() == pPlayer )
		{
			Helper_RefreshTeammateEquipment( m_pLocalPlayerEquipment.Get(), pPlayer, true );
			RefreshItems(); // Could have thrown a weapon, refresh buywheel
			UpdatePurchaseFailureLabel();
		}
		else
		{
			m_pTeamEquipment->IterateChildren( [pPlayer]( CPanel2D *pPanel ) -> bool
			{
				if ( pPanel->GetAttribute( "userid", 0 ) == pPlayer->GetUserID() )
				{
					Helper_RefreshTeammateEquipment( pPanel, pPlayer, false );
					return false;
				}
				return true;
			} );
		}
	}
	return false;
}

bool Helper_SetItemDialogVars( CPanel2D* pPanel, const CEconItemView *pItem )
{
	if ( const CEconItemRarityDefinition *pRarity = GetItemSchema()->GetRarityDefinition( pItem->GetRarity() ) )
	{
		pPanel->SetDialogVariableLocString( "item_rarity", pRarity->GetWepLocKey() );
		pPanel->SwitchClass( k_symRarityClass, pRarity->GetRarityClass() );
	}

	char szNameBuff[ 256 ];
	pItem->GetItemDisplayNameUtf8( szNameBuff, sizeof( szNameBuff ) );
	pPanel->SetDialogVariable( "item_name", szNameBuff );

	return true;
}

ConVar buymenu_3dpanel_item_rotate_speed( "buymenu_3dpanel_item_rotate_speed", "30.0", FCVAR_DEVELOPMENTONLY, "equivalent to pixels/frame if click-dragging item" );
ConVar buymenu_3dpanel_item_rotate_time( "buymenu_3dpanel_item_rotate_time", "0.0", FCVAR_DEVELOPMENTONLY, "time (seconds) to yo-yo between item rotation extents, 8.0s a good default" );

bool CCSGO_BuyMenu::OnPurchaseButtonHover( loadout_positions_t pos )
{
	CCSPlayerInventory *pInv = CSInventoryManager()->GetLocalCSInventory();
	C_CSPlayer *pPlayer = CCSPlayer::GetLocalCSPlayer();
	if ( pos != LOADOUT_POSITION_INVALID && pInv && pPlayer )
	{
		const CEconItemView *pItem = pInv->GetItemInLoadoutFilteredByProhibition( pPlayer->GetTeamNumber(), pos );
		if ( pItem && pItem->IsValid() )
		{
			if ( m_pItemPreview.Get() )
			{
				UIItemInfo_t info;
				// FIXME: Preview panel uses this string markup to find the EconItemView we have here... Could rework the SetScene code to just let us
				// pass in the EconItemView we want to use. 
				if ( pItem->GetItemID() == 0 )
					info.m_itemName.Format( "img://inventory_%llu", CombinedItemIdMakeFromDefIndexAndPaint( pItem->GetItemIndex(), pItem->GetCustomPaintKitIndex() ) );
				else
					info.m_itemName.Format( "img://inventory_%llu", pItem->GetItemID() );
				info.m_manifestName = "resource/ui/econ/ItemModelPanelCharWeaponInspect.res";
				info.m_bAntiAlias = true;
				info.m_bRotate = true;
				m_pItemPreview->SetScene( info, false );
				m_pItemPreview->SetSceneIntroRotation( buymenu_3dpanel_item_rotate_speed.GetFloat(), buymenu_3dpanel_item_rotate_time.GetFloat(), true );

				Helper_SetItemDialogVars( m_pItemPreview.Get(), pItem );
			}
			char szNameBuff[ 256 ];
			pItem->GetItemDisplayNameUtf8( szNameBuff, sizeof( szNameBuff ) );
			m_pItemInfo->SetDialogVariable( "item_name", szNameBuff );
			int nPrice = pPlayer->GetWeaponPrice( pItem->GetItemDefinition()->GetDefinitionIndex(), pItem );
			m_pItemInfo->SetDialogVariable( "cost", nPrice );

			const CCSWeaponInfo* pWeaponInfo = GetWeaponInfoFromItem( pItem );

			bool bFullStats = true;// ( pWeaponInfo->m_WeaponCategory != WEAPONCATEGORY_OTHER );
			CPanel2D *pStats = RequireChildInLayoutFile( "WeaponStats" );
			CPanel2D *pDesc = RequireChildInLayoutFile( "ItemDesc" );
			pDesc->SetVisible( !bFullStats ); 
			pStats->SetVisible( bFullStats );

			// FIXME: Looking up progress bars every time... Can factor out into members if this is noticeably slow or 
			// traverse once finding children of type CProgressBar, then apply the correct value based on ID
			if ( bFullStats )
			{
				// Ammo info
				m_pItemInfo->SetDialogVariable( "ammo_per_mag", pWeaponInfo->GetPrimaryClipSize( pItem ) );
				m_pItemInfo->SetDialogVariable( "ammo_total", pWeaponInfo->GetPrimaryReserveAmmoMax( pItem ) );

				// Kill award
				int nKillReward = pWeaponInfo->GetKillAward( pItem );
				m_pItemInfo->SetDialogVariable( "kill_reward", nKillReward );

				// Kill award ratio (to default, as percentage) 
				int nDefaultKillReward = CSGameRules()->PlayerCashAwardValue( PlayerCashAward::KILLED_ENEMY );
				float flRatio = ( float )nKillReward / ( float )nDefaultKillReward;
				m_pItemInfo->SetDialogVariable( "kill_reward_pct", ( int )( flRatio * 100 ) );
				int nPoints = CSGameRules()->GetWeaponScoreForDeathmatch( pos, nullptr );
				m_pItemInfo->SetDialogVariable( "dm_points", nPoints );

				// damage
				int nBullets = pWeaponInfo->GetBullets( pItem );
				int nDamage = pWeaponInfo->GetDamage( pItem );
				m_pItemInfo->SetDialogVariable( "damage", ( int )( nBullets * nDamage ) );
				CProgressBar* pDmgProgress = panel_cast< CProgressBar* >( m_pItemInfo->FindChildInLayoutFile( "DmgProgress" ), true );
				if ( pDmgProgress )
					pDmgProgress->SetValue( nBullets * nDamage );

				// fire rate
				float flRate = 0;
				if ( pWeaponInfo->GetCycleTime( pItem ) > 0.0f )
					flRate = 1.f / pWeaponInfo->GetCycleTime( pItem );
				m_pItemInfo->SetDialogVariable( "fire_rate", ( int )( flRate ) );
				CProgressBar* pFireRateProgress = panel_cast< CProgressBar* >( m_pItemInfo->FindChildInLayoutFile( "FireRateProgress" ), true );
				if ( pFireRateProgress )
					pFireRateProgress->SetValue( flRate );

				// recoil control aka accuracy
				int nAccuracy = Helper_GetWeaponAccuracy( pWeaponInfo, pItem );
				m_pItemInfo->SetDialogVariable( "accuracy", ( int )( nAccuracy ) );
				CProgressBar* pAccuracyProgress = panel_cast< CProgressBar* >( m_pItemInfo->FindChildInLayoutFile( "AccuracyProgress" ), true );
				if ( pAccuracyProgress )
					pAccuracyProgress->SetValue( nAccuracy );

				// accurate range
				float flEffectiveRange = GetEffectiveRangeRawValue( pItem );
				int nRange = MIN( 1, flEffectiveRange / 90 ) * 100;
				m_pItemInfo->SetDialogVariable( "range", ( int )( nRange ) );
				CProgressBar* pRangeProgress = panel_cast< CProgressBar* >( m_pItemInfo->FindChildInLayoutFile( "RangeProgress" ), true );
				if ( pRangeProgress )
					pRangeProgress->SetValue( nRange );

				// armor penetration
				int nArmorRatio = ( 100 * pWeaponInfo->GetArmorRatio( pItem ) );
				m_pItemInfo->SetDialogVariable( "armor_pen", ( int )( nArmorRatio ) );
				CProgressBar* pArmorPenProgress = panel_cast< CProgressBar* >( m_pItemInfo->FindChildInLayoutFile( "ArmorPenProgress" ), true );
				if ( pArmorPenProgress )
					pArmorPenProgress->SetValue( nArmorRatio );

				// penetration power
				float flPenetration = pWeaponInfo->GetPenetration( pItem );
				m_pItemInfo->SetDialogVariable( "penetration_power", ( int )( flPenetration ) );

				// stopping power aka tagging
				float flTagLarge = pWeaponInfo->GetFlinchVelocityModifierLarge( pItem );
				float flTagging = ( 1 - flTagLarge ) * nBullets;
				m_pItemInfo->SetDialogVariable( "tagging", ( int )( flTagging * 100 ) );
			}
			else
			{
				const char* szBaseName = strchr( pItem->GetItemDefinition()->GetDefinitionName(), '_' );

				// HACK: CSGO-2236 Fixup defuser name in hostage modes... 
				if ( FStrEq( szBaseName, "_defuser" ) && CSGameRules() && CSGameRules()->IsHostageRescueMap() )
					szBaseName = "_cutters";

				if ( szBaseName )
					pDesc->SetDialogVariableLocString( "desc", CFmtStr( "#SFUI_BuyMenu_InfoDescription_%s", ( szBaseName + 1 ) ).String() );
				else
					pDesc->SetVisible( false );
			}

			return true;
		}
	}


	return false;
}

void CCSGO_BuyMenu::UpdatePurchaseFailureLabel()
{
	m_pPurchaseFailureLabel->SetText( "" );

	C_CSPlayer *pPlayer = CCSPlayer::GetLocalCSPlayer();
	CCSPlayerInventory *pInv = CSInventoryManager()->GetLocalCSInventory();
	if ( !m_pHoveredWedge.Get() || !pPlayer || !pInv )
		return;

	loadout_positions_t pos = ( loadout_positions_t )m_pHoveredWedge->GetAttribute( "loadout_pos", ( int )LOADOUT_POSITION_INVALID );
	const CEconItemView *pItem = pInv->GetItemInLoadout( pPlayer->GetTeamNumber(), pos );
	if ( pItem && !pItem->IsValid() )
		return;

	int nLimit;
	AcquireResult::Type canAcquire = pPlayer->CanAcquire( pItem, AcquireMethod::Buy, &nLimit );
	if ( const char* szLocString = AcquireResult::ExplainFailureString( canAcquire ) )
	{
		if(canAcquire == AcquireResult::NotAllowedByProhibition)
		{
			char bufUtf8[256];
			pItem->GetItemDisplayNameUtf8( bufUtf8, sizeof( bufUtf8 ) );
			m_pPurchaseFailureLabel->SetDialogVariable( "s1", bufUtf8 );
		}
		else
		{
			m_pPurchaseFailureLabel->SetDialogVariable( "s1", CNumStr( nLimit ) );
		}

		m_pPurchaseFailureLabel->SetTextWithDialogVariables( szLocString, CLabel::k_ETextTypeHTML );
	}
}

void CCSGO_BuyMenu::CreateBuyWheels( void )
{
	CPanel2D *pBuyWheelContainer = panorama::panel_cast< CPanel2D * > ( RequireChildInLayoutFile( "BuyWheelContainer" ) );

	// Keep handle to team logo and keep in sync with player team.
	m_pTeamLogo = RequireChildInLayoutFile( "TeamLogo" );

	// Create category wheel and populate buttons
	m_pCategoryWheel = new CCSGO_RadialSelector( pBuyWheelContainer, "CategoryWheel", CCSGO_RadialSelector::k_eSixChoices );
	CreateCategoryPanels( pBuyWheelContainer, panel_cast< CButton* >( m_pCategoryWheel->GetChild( 0 ) ), k_ePistol, LOADOUT_POSITION_SECONDARY0, LOADOUT_POSITION_SECONDARY4 );
	CreateCategoryPanels( pBuyWheelContainer, panel_cast< CButton* >( m_pCategoryWheel->GetChild( 1 ) ), k_eHeavy, LOADOUT_POSITION_HEAVY0, LOADOUT_POSITION_HEAVY4 );
	CreateCategoryPanels( pBuyWheelContainer, panel_cast< CButton* >( m_pCategoryWheel->GetChild( 2 ) ), k_eSMG, LOADOUT_POSITION_SMG0, LOADOUT_POSITION_SMG4 );
	CreateCategoryPanels( pBuyWheelContainer, panel_cast< CButton* >( m_pCategoryWheel->GetChild( 3 ) ), k_eRifle, LOADOUT_POSITION_RIFLE0, LOADOUT_POSITION_RIFLE5 );
	CreateCategoryPanels( pBuyWheelContainer, panel_cast< CButton* >( m_pCategoryWheel->GetChild( 4 ) ), k_eGear, LOADOUT_POSITION_EQUIPMENT0, LOADOUT_POSITION_EQUIPMENT3 );
	CreateCategoryPanels( pBuyWheelContainer, panel_cast< CButton* >( m_pCategoryWheel->GetChild( 5 ) ), k_eGrenade, LOADOUT_POSITION_GRENADE0, LOADOUT_POSITION_GRENADE4 );

	m_pCategoryWheel->Enable();
}

bool CCSGO_BuyMenu::CloseBuyMenu( void )
{
	// Double closes can happen, some of the below can cause problems if not paired with an open. 
	if ( !BIsVisible() ) 
		return false;

	// Removing chat panel from the input context stack
	GetParentWindow()->UIWindowInput()->RemoveInputContext( this->UIPanel() );

	// NOTE: this appears to be used for deathmatch invulnerability time... 
	if( C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer() )
	{
		pLocalPlayer->SetBuyMenuOpen( false );
	}

	if ( m_pVisibleBuyWheel.Get() )
	{
		m_pVisibleBuyWheel->Disable();
		m_pVisibleBuyWheel = nullptr;
	}
	m_pCategoryWheel->Disable();
	UnhookEvents();

	SetVisible( false );
	if( g_pInputSystem  )
		g_pInputSystem->SetSteamControllerMode( NULL, this );
	m_Capture.Disable();

	return true;
}

bool CCSGO_BuyMenu::OpenBuyMenu()
{
	// Attempting to enter buymenu while it's already open closes it instead
	if ( BIsVisible() )
	{
		// LEGACY: Command to open buymenu went back one level
		DispatchEvent( BuyMenu_Back(), this );
		return true;
	}

	// Don't open if we aren't allowed to buy
	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pLocalPlayer || !pLocalPlayer->CanPlayerBuy( false ) )
		return true;

	// From this point we are committed to fully opening the buy menu
	
	pLocalPlayer->SetBuyMenuOpen( true );

	if ( m_pCountdownTimer.Get() )
		m_pCountdownTimer->SetTime( CCSGO_CountdownTimer::kClockType_Game, Helper_GetBuyTimeRemaining() );

	SetVisible( true );
	
	// Buymenu panel has its own input context (cf SetTopOfInputContext( true ) in constructor)
	// Therefore calling SetFocus will switch input context. Make sure to remove it from the stack
	// when hiding the buymenu panel
	SetFocus();

	m_Capture.Enable();

	// hide other wheels-- needed if we close the menu without calling CloseBuyMenu, like in a f7 reload
	for ( int i = 0; i < k_eCategoryCount; ++i )
	{
		if ( m_buyWheels[ i ].m_pBuyWheel.Get() )
			m_buyWheels[ i ].m_pBuyWheel->Disable();
	}
	m_pCategoryWheel->Enable();
	m_pVisibleBuyWheel = nullptr;
	CreateTeamEquipmentPanels(); // NOTE: Event handlers require certain layout snippets be loaded, if reordering these make sure we hook events late
	RefreshItems();
	HookEvents();

	if ( m_pMoneyPanel.Get() )
	{
		// CSGO-2052 Hide animations pending when we first show to avoid a lag before accurate money is shown
		m_pMoneyPanel->Reset();
	}

	// HACK: start stat display hidden
	m_pItemInfo->SwitchClass( k_symPreviewContainer, k_symPreviewContainerHidden );

	return true;
}


bool CCSGO_BuyMenu::OnMouseButtonUp( const MouseData_t &code )
{
	if ( !BIsVisible() )
		return false;

	if ( code.m_MouseCode == panorama::MOUSE_RIGHT )
	{
		DispatchEvent( BuyMenu_Back(), this );
		return true;
	}
	return false;
}

bool CCSGO_BuyMenu::OnKeyDown( const KeyData_t & code )
{
	if ( !BIsVisible() )
		return false;

	if ( code.m_KeyCode == panorama::KEY_ESCAPE )
	{
		DispatchEvent( BuyMenu_Back(), this );
		return true;
	}
	return false;
}

bool CCSGO_BuyMenu::EventFrameUpdate()
{
	// Close the buy menu when buy time expires.
	// Note that this event is only hooked while the buy menu is open
	C_CSPlayer* pPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pPlayer || !pPlayer->CanPlayerBuy( false ) )
	{
		CloseBuyMenu();

		// $$$REI Why do we not do this in co-op?  Feels like this logic doesn't belong here.
		// $$$REI (I put it here, copied from old scaleform buy menu behavior)
		if ( !CSGameRules()->IsPlayingCooperativeGametype() )
			PrintBuyTimeOverMessage();
	}
	
	return false;
}

