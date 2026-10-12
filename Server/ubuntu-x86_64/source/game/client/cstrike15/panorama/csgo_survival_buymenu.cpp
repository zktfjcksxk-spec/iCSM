//========= Copyright © Valve Corporation, All rights reserved. ============//
//
// Purpose:  Panorama menu for survival buymenu
//
//=====================================================================================//

#include "cbase.h"
#include "csgo_survival_buymenu.h"
#include "IGameUIFuncs.h"
#include "panorama/ui_root.h"
#include "panorama/uijsregistration.h"
#include "c_cs_player.h"
#include "c_cs_playerresource.h"
#include "clientsteamcontext.h"
#include "cs_gamerules_survival.h"
#include "hudelement.h"
#include "weapon_tablet.h"
#include "csgo_item_image_panel.h"
#include "csgo_ui_tooltip_manager.h"
#include "econ_item_schema.h"
#include "drone.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace panorama;

REGISTER_PANEL2D_FACTORY( CCSGO_SurvivalBuyMenu, CSGOSurvivalBuyMenu );

DEFINE_PANORAMA_EVENT_DOC( SurvivalBuyEvent, "bool", "Buy event." );
DEFINE_PANORAMA_EVENT_DOC( SurvivalBuyMenuUpdate, "", "" );

#include "weapon_tablet_buymenu.inc"

C_CSPlayer *GetActualTabletPlayerOwnerWithBuyMenuOpen( void )
{
	C_CSPlayer *pHudPlayer = GetHudPlayer();

	if ( !pHudPlayer || !pHudPlayer->IsAlive() )
		return NULL;

	C_CSPlayer *pLocalPlayer = ToCSPlayer( C_BasePlayer::GetLocalPlayer() );
	if ( pLocalPlayer != pHudPlayer ) // spectating
	{
		if ( pLocalPlayer->GetObserverMode() != OBS_MODE_IN_EYE ) // not in-eye
		{
			return NULL; 
		}
	}

	CBaseCombatWeapon* pCurrentWep = pHudPlayer->GetActiveWeapon();
	CBaseCombatWeapon* pTabletWep = pHudPlayer->Weapon_OwnsThisType( "weapon_tablet" );

	if ( pCurrentWep && pCurrentWep == pTabletWep && pTabletWep && static_cast<CTablet*>(pTabletWep)->ShouldBuyMenuBeOpen() && !pHudPlayer->ShouldDraw() )
	{
		return pHudPlayer;
	}

	return NULL;
}

CCSGO_SurvivalBuyMenu::CCSGO_SurvivalBuyMenu( panorama::CPanel2D *pParent, const char *pchID )
	: panorama::CPanel2D( pParent, pchID )
	, m_Capture( this, "BuyMenu", k_EGameInputShareMouse| k_EGameInputUIEnableKeyInput )
{
	RequireLoadLayout( "file://{resources}/layout/survival/survival_buymenu.xml" );

	RegisterEventHandler( SurvivalBuyEvent(), this, &CCSGO_SurvivalBuyMenu::HandleBuyEvent );
	RegisterForUnhandledEvent( SurvivalBuyMenuUpdate(), this, &CCSGO_SurvivalBuyMenu::HandleUpdate );
	
	ShowSurvivalBuyMenu( false );

	CPanel2D* pItemContainer = RequireChildInLayoutFile( "itemcontainer" );
	for ( int i = 0; i < TabletBuyMenuEntry::k_ECategoryCount; ++i )
	{
		CPanel2D* pCategory = new CPanel2D( pItemContainer, CFmtStr( "container_%d", i ).Get() );
		pCategory->RequireLoadLayoutSnippet( "buymenu-items-container" );
		m_pItemContainers[ i ] = pCategory->RequireChild( "itemrow" );
		pCategory->SetDialogVariableLocString( "category_name", CFmtStr( "SFUI_TableBuymenuCategory%d", i ).Get() );
	}
	
	m_nLastKnownPlayerAccount = -1;
}

CCSGO_SurvivalBuyMenu::~CCSGO_SurvivalBuyMenu()
{
	m_Capture.Disable();
}

const CEconItemView* Helper_GetLocalPlayerEquippedItem( loadout_positions_t nPosition )
{
	if ( nPosition != LOADOUT_POSITION_INVALID )
	{
		CCSPlayerInventory* pLocalInv = CSInventoryManager()->GetLocalCSInventory();
		if ( pLocalInv )
		{
			// HACK: Attempt to mirror the buying process the server will use so we show the correct image

			// Try T first because survival players are all currently on T side. 
			const CEconItemView* pLoadoutItem = pLocalInv->GetItemInLoadout( TEAM_TERRORIST, nPosition );
			if ( !pLoadoutItem->IsValid() )
				pLoadoutItem = pLocalInv->GetItemInLoadout( TEAM_CT, nPosition ); // If this item isn't available for T, server will buy the CT item. 

			return pLoadoutItem->IsValid() ? pLoadoutItem : nullptr;
		}
	}

	return nullptr;
}

void CCSGO_SurvivalBuyMenu::CreateBuyMenuItems()
{
	static ConVarRef sv_dz_cash_bundle_size( "sv_dz_cash_bundle_size" );
	DestroyBuyMenuItems();

	int nMoney = 0;
	C_CSPlayer *pPlayerOwner = GetActualTabletPlayerOwnerWithBuyMenuOpen();
	if ( !pPlayerOwner )
		return;
	m_hLastKnownPlayer = pPlayerOwner;

	nMoney = pPlayerOwner->GetAccount();

	bool bIsSpectating = ToCSPlayer( C_BasePlayer::GetLocalPlayer() ) != pPlayerOwner;

	SetDialogVariable( "playermoney", nMoney );

	m_nLastKnownPlayerAccount = nMoney;

	CBaseCombatWeapon* pTabletWep = pPlayerOwner->Weapon_OwnsThisType( "weapon_tablet" );
	CTablet *pTablet = pTabletWep ? static_cast<CTablet*>(pTabletWep) : NULL;

	for ( int i = 0; i < ARRAYSIZE( s_TabletBuyMenu ); i++ )
	{
		if ( !s_TabletBuyMenu[ i ].szSpawnRuleGroupName || !*s_TabletBuyMenu[ i ].szSpawnRuleGroupName )
			continue; // this is a compatibility entry and is not for purchase

		// Ensure that the conditions on item availability are satisfied
		if ( s_TabletBuyMenu[ i ].nGameRulesDecisionTypeID )
		{
			int nCurrentRulesValue = ( CSGameRules() && CSGameRules()->IsPlayingSurvival() )
				? CSGameRules()->GetSurvivalRules()->GetSurvivalGameRuleDecisionValue( ( ESurvivalGameRuleDecision_t ) s_TabletBuyMenu[ i ].nGameRulesDecisionTypeID )
				: 0;
			if ( nCurrentRulesValue != s_TabletBuyMenu[ i ].nGameRulesDecisionValue )
				continue;
		}

		if ( s_TabletBuyMenu[ i ].nCategory < 0 || s_TabletBuyMenu[ i ].nCategory >= TabletBuyMenuEntry::k_ECategoryCount )
		{
			Assert( 0 );
			continue;
		}

		CPanel2D* pContainer = m_pItemContainers[ s_TabletBuyMenu[ i ].nCategory ].Get();

		CPanel2D* pItemPanel = new CPanel2D( pContainer, nullptr );
		pItemPanel->RequireLoadLayoutSnippet( "buymenu-item" );
		pItemPanel->SetDialogVariable( "item_price", s_TabletBuyMenu[ i ].nPrice * sv_dz_cash_bundle_size.GetInt() );

		/* Removed for partner depot */

		pItemPanel->SetOnMouseOverEvent( UIShowTextTooltip::MakeEvent( pItemPanel, CFmtStr( "%s%s", s_TabletBuyMenu[ i ].szLocalizedToken, "_Tooltip" ).Get() ) );
		pItemPanel->SetOnMouseOutEvent( UIHideTextTooltip::MakeEvent( nullptr ) );

		bool bCanBuy = true;

		// check if we already have this tablet upgrade
		if ( bCanBuy && s_TabletBuyMenu[i].nTabletUpgradeEnum >= 0 && pTablet && pTablet->HasTabletUpgrade( (tablet_upgrade_type_t)s_TabletBuyMenu[i].nTabletUpgradeEnum ) )
		{
			bCanBuy = false;
		}

		if ( bCanBuy )
		{
			pItemPanel->SetEnabled( !CSGameRules()->IsWarmupPeriod() && nMoney >= (s_TabletBuyMenu[i].nPrice * sv_dz_cash_bundle_size.GetInt()) );
		}
		else
		{
			pItemPanel->AddClass( "owned" );
			pItemPanel->SetEnabled( false );
		}

		pItemPanel->SetOnActivateEvent( CFmtStr( "SurvivalBuyEvent(%i);", i ).Get() );
		pItemPanel->SetHitTestEnabled( !bIsSpectating );
		pItemPanel->SetHitTestChildrenEnabled( !bIsSpectating );
	}
}

void CCSGO_SurvivalBuyMenu::DestroyBuyMenuItems()
{
	for ( int i = 0; i < TabletBuyMenuEntry::k_ECategoryCount; ++i )
	{
		if ( m_pItemContainers[ i ].Get() )
			m_pItemContainers[ i ]->RemoveAndDeleteChildren();
	}
}

extern Vector2D g_TabletScreenSpace_UL;
extern Vector2D g_TabletScreenSpace_LR;
bool CCSGO_SurvivalBuyMenu::HandleUpdate( void )
{
	C_CSPlayer *pPlayerOwner = GetActualTabletPlayerOwnerWithBuyMenuOpen();
	C_CSPlayer *pLocalPlayer = ToCSPlayer( C_BasePlayer::GetLocalPlayer() );
	if ( !pPlayerOwner || !pLocalPlayer )
	{
		ShowSurvivalBuyMenu( false );
		return false;
	}
	
	if ( !this->BIsVisible() )
	{
		ShowSurvivalBuyMenu( true );
	}

	if ( pPlayerOwner->GetAccount() != m_nLastKnownPlayerAccount || pPlayerOwner != m_hLastKnownPlayer )
	{
		CreateBuyMenuItems();
	}

	float flScreenWidthPixels = GetParentWindow()->GetWindowWidth();
	float flScreenHeightPixels = GetParentWindow()->GetWindowHeight();
	float flRenderWidthPixels = GetActualRenderWidth();
	float flRenderHeightPixels = GetActualRenderHeight();

	if ( flRenderWidthPixels && flRenderHeightPixels )
	{
		float flLeft = RemapValClamped( g_TabletScreenSpace_UL.x, -1.0f, 1.0f, 0.0f, 1.0f );
		float flTop = RemapValClamped( g_TabletScreenSpace_UL.y, 1.0f, -1.0f, 0.0f, 1.0f );
		float flRight = RemapValClamped( g_TabletScreenSpace_LR.x, -1.0f, 1.0f, 0.0f, 1.0f );
		float flBottom = RemapValClamped( g_TabletScreenSpace_LR.y, 1.0f, -1.0f, 0.0f, 1.0f );

		float flTargetMidX = (flScreenWidthPixels * Lerp( 0.5, flLeft, flRight ) - (flRenderWidthPixels / 2.0f)) / flScreenWidthPixels;
		float flTargetMidY = (flScreenHeightPixels * Lerp( 0.5, flTop, flBottom ) - (flRenderHeightPixels / 2.0f)) / flScreenHeightPixels;

		SetPositionWithoutTransition(
			CUILength( flTargetMidX * 100.0f, CUILength::k_EUILengthPercent ),
			CUILength( flTargetMidY * 100.0f, CUILength::k_EUILengthPercent ),
			CUILength( 0, CUILength::k_EUILengthPercent ) );

		float flWidth = flRight - flLeft;
		float flDesiredWidthPixels = flWidth * flScreenWidthPixels;
		float flScale = flDesiredWidthPixels / flRenderWidthPixels;

		CUtlVector<CTransform3D *> vecTransforms;
		vecTransforms.AddToTail( new CTransformScale3D( flScale, flScale, 1.0f ) );
		SetTransform3DSimple( vecTransforms );
	}
	else
	{
		SetPositionWithoutTransition(
			CUILength( 0, CUILength::k_EUILengthPercent ),
			CUILength( 200.0f, CUILength::k_EUILengthPercent ),
			CUILength( 0, CUILength::k_EUILengthPercent ) );
	}

	return true;
}

bool CCSGO_SurvivalBuyMenu::HandleBuyEvent( int nIndex )
{
	if ( nIndex >= 0 )
	{
		if ( nIndex > ARRAYSIZE( s_TabletBuyMenu ) )
		{
			Assert( false );
			return true;
		}

		engine->ServerCmd( CFmtStr( "tabletbuy_buy_%s", s_TabletBuyMenu[nIndex].szSpawnRuleGroupName ) );
	}

	engine->ServerCmd( "tabletbuy_close" );

	return true;
}

bool CCSGO_SurvivalBuyMenu::ShowSurvivalBuyMenu( bool bShow )
{
	C_CSPlayer *pPlayerOwner = NULL;
	if ( bShow )
		pPlayerOwner = GetActualTabletPlayerOwnerWithBuyMenuOpen();

	if ( bShow && pPlayerOwner != NULL )
	{
		if ( !m_Capture.BEnabled() && ToCSPlayer( C_BasePlayer::GetLocalPlayer() ) == pPlayerOwner ) // not spectating
		{
			m_Capture.Enable();
		}

		CreateBuyMenuItems();
		SetVisible( true );
	}
	else
	{
		m_Capture.Disable();

		SetVisible( false );
		DestroyBuyMenuItems();
	}

	return false;
}

void CCSGO_SurvivalBuyMenu::OnLayoutReloaded()
{
	BaseClass::OnLayoutReloaded();
	ShowSurvivalBuyMenu( false );
}
