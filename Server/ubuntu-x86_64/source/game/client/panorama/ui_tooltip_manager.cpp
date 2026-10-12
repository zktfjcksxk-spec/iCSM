//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "cbase.h"
#include "ui_tooltip_manager.h"
#include "panorama/ui_root.h"
#include "panorama/controls/tooltip.h"
#include "panorama/tooltips/ui_tooltip_text.h"
#include "panorama/tooltips/ui_tooltip_title_text.h"
#include "panorama/tooltips/ui_tooltip_title_image_text.h"
#include "panorama/tooltips/ui_tooltip_custom_layout.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

REGISTER_PANEL2D_FACTORY( CUI_TooltipManager, TooltipManager )

DEFINE_PANORAMA_EVENT( UIShowTextTooltip );
DEFINE_PANORAMA_EVENT( UIShowTextTooltipStyled );
DEFINE_PANORAMA_EVENT( UIHideTextTooltip );
const char k_szTextTooltipID[] = "TextTooltip";

DEFINE_PANORAMA_EVENT( UIShowTitleTextTooltip );
DEFINE_PANORAMA_EVENT( UIShowTitleTextTooltipStyled );
DEFINE_PANORAMA_EVENT( UIHideTitleTextTooltip );
const char k_szTitleTextTooltipID[] = "TitleTextTooltip";

DEFINE_PANORAMA_EVENT( UIShowTitleImageTextTooltip );
DEFINE_PANORAMA_EVENT( UIShowTitleImageTextTooltipStyled );
DEFINE_PANORAMA_EVENT( UIHideTitleImageTextTooltip );
const char k_szTitleImageTextTooltipID[] = "TitleImageTextTooltip";

DEFINE_PANORAMA_EVENT( UIShowCustomLayoutTooltip );
DEFINE_PANORAMA_EVENT( UIShowCustomLayoutTooltipStyled );
DEFINE_PANORAMA_EVENT( UIShowCustomLayoutParametersTooltip );
DEFINE_PANORAMA_EVENT( UIShowCustomLayoutParametersTooltipStyled );
DEFINE_PANORAMA_EVENT( UIHideCustomLayoutTooltip );

const char k_szTooltipClassAttribute[] = "tooltip_class";

ConVar sticky_tooltips( "sticky_tooltips", "0", FCVAR_DEVELOPMENTONLY, "Don't ever hide tooltips. Helpful when debugging complicated tooltip layouts." );

using namespace panorama;

CUI_TooltipManager::CUI_TooltipManager( CPanel2D *pParent, const char *pchID )
	: CPanel2D( pParent, pchID )
{
	RegisterForUnhandledEvent( UIShowTextTooltip(), this, &CUI_TooltipManager::ShowTextTooltip );
	RegisterForUnhandledEvent( UIShowTextTooltipStyled(), this, &CUI_TooltipManager::ShowTextTooltip );
	RegisterForUnhandledEvent( UIHideTextTooltip(), this, &CUI_TooltipManager::HideTextTooltip );

	RegisterForUnhandledEvent( UIShowTitleTextTooltip(), this, &CUI_TooltipManager::ShowTitleTextTooltip );
	RegisterForUnhandledEvent( UIShowTitleTextTooltipStyled(), this, &CUI_TooltipManager::ShowTitleTextTooltip );
	RegisterForUnhandledEvent( UIHideTitleTextTooltip(), this, &CUI_TooltipManager::HideTitleTextTooltip );

	RegisterForUnhandledEvent( UIShowTitleImageTextTooltip(), this, &CUI_TooltipManager::ShowTitleImageTextTooltip );
	RegisterForUnhandledEvent( UIShowTitleImageTextTooltipStyled(), this, &CUI_TooltipManager::ShowTitleImageTextTooltip );
	RegisterForUnhandledEvent( UIHideTitleImageTextTooltip(), this, &CUI_TooltipManager::HideTitleImageTextTooltip );

	RegisterForUnhandledEvent( UIShowCustomLayoutParametersTooltip(), this, &CUI_TooltipManager::ShowCustomLayoutParametersTooltip );
	RegisterForUnhandledEvent( UIShowCustomLayoutParametersTooltipStyled(), this, &CUI_TooltipManager::ShowCustomLayoutParametersTooltip );
	RegisterForUnhandledEvent( UIShowCustomLayoutTooltip(), this, &CUI_TooltipManager::ShowCustomLayoutTooltip );
	RegisterForUnhandledEvent( UIShowCustomLayoutTooltipStyled(), this, &CUI_TooltipManager::ShowCustomLayoutTooltip );
	RegisterForUnhandledEvent( UIHideCustomLayoutTooltip(), this, &CUI_TooltipManager::HideCustomLayoutTooltip );
}

CUI_TooltipManager::~CUI_TooltipManager()
{
}

/*static*/ CUI_TooltipManager *CUI_TooltipManager::GetWindowTooltipManager( panorama::IUIWindow *pWindow )
{
	CUI_Root *pRoot = CUI_Root::GetRootForWindow( pWindow );
	if ( !pRoot )
		return nullptr;

	return pRoot->GetTooltipManager();
}

/*static*/ CUI_TooltipManager *CUI_TooltipManager::GetPanelTooltipManager( const panorama::CPanelPtr< panorama::IUIPanel > &panelPtr )
{
	if ( !panelPtr.Get() )
		return nullptr;

	return GetWindowTooltipManager( panelPtr->GetParentWindow() );
}

bool CUI_TooltipManager::ShouldHandleTooltipEvent( const panorama::CPanelPtr< panorama::IUIPanel >& panelPtr )
{
	IUIWindow *pMyWindow = GetParentWindow();
	if ( !pMyWindow || !pMyWindow->BIsVisible() )
		return false;

	// If we're visible and the target doesn't exist, then we're just relative to the mouse. This tooltip manager should be fine.
	if ( !panelPtr.Get() )
		return true;

	// If we're in the middle of a drag, don't show tooltips
	if ( pMyWindow->UIWindowInput()->BDragInProgress() )
		return false;

	return pMyWindow == panelPtr->GetParentWindow();
}

bool CUI_TooltipManager::ShowTooltip( const char *pszID, const panorama::CPanelPtr< panorama::IUIPanel >& targetPanelPtr )
{
	CTooltip *pTooltip = GetTooltip< CTooltip >( pszID );
	if ( !pTooltip )
	{
		AssertMsg( false, "Can't show a tooltip that was not yet created" );
		return true;
	}

	pTooltip->SetTooltipTarget( targetPanelPtr );
	pTooltip->CalculatePosition();
	pTooltip->SetTooltipVisible( true );

	if ( sticky_tooltips.GetBool() )
	{
		pTooltip->SetHitTestChildrenEnabled( true );
	}

	return true;
}

bool CUI_TooltipManager::ShowTooltipAtPos( const char *pszID, const panorama::CPanelPtr< panorama::IUIPanel >& targetPanelPtr, float flX, float flY )
{
	CTooltip *pTooltip = GetTooltip< CTooltip >( pszID );
	if ( !pTooltip )
	{
		AssertMsg( false, "Can't show a tooltip that was not yet created" );
		return true;
	}

	pTooltip->SetTooltipTarget( targetPanelPtr );
	CUILength x( flX, CUILength::k_EUILengthLength );
	CUILength y( flY, CUILength::k_EUILengthLength );
	CUILength z( 0, CUILength::k_EUILengthLength );
	pTooltip->SetPositionWithoutTransition( x, y, z, true );
	pTooltip->SetTooltipVisible( true );

	if ( sticky_tooltips.GetBool() )
	{
		pTooltip->SetHitTestChildrenEnabled( true );
	}

	return true;
}


bool CUI_TooltipManager::HideTooltip( const char *pszID )
{
	if ( sticky_tooltips.GetBool() )
		return true;

	CTooltip *pTooltip = GetTooltip< CTooltip >( pszID );
	if ( pTooltip )
	{
		pTooltip->SetTooltipVisible( false );
	}

	return true;
}

bool CUI_TooltipManager::ShowTextTooltip( const panorama::CPanelPtr< panorama::IUIPanel >& panelPtr, const char *pszText, const char *pszClass )
{
	if ( !ShouldHandleTooltipEvent( panelPtr ) )
		return false;

	CUI_Tooltip_Text *pTooltip = EnsureTooltip< CUI_Tooltip_Text >( k_szTextTooltipID );

	pTooltip->SetLocalizationParent( panelPtr );
	pTooltip->SetText( pszText );
	pTooltip->SwitchClass( k_szTooltipClassAttribute, pszClass );

	return ShowTooltip( k_szTextTooltipID, panelPtr );
}

bool CUI_TooltipManager::ShowTextTooltip( const panorama::CPanelPtr< panorama::IUIPanel >& panelPtr, const char *pszText )
{
	return ShowTextTooltip( panelPtr, pszText, NULL );
}

bool CUI_TooltipManager::HideTextTooltip( const panorama::CPanelPtr< panorama::IUIPanel >& panelPtr )
{
	return HideTooltip( k_szTextTooltipID );
}

bool CUI_TooltipManager::ShowTitleTextTooltip( const panorama::CPanelPtr< panorama::IUIPanel >& panelPtr, const char *pszTitle, const char *pszText, const char *pszClass )
{
	if ( !ShouldHandleTooltipEvent( panelPtr ) )
		return false;

	CUI_Tooltip_TitleText *pTooltip = EnsureTooltip< CUI_Tooltip_TitleText >( k_szTitleTextTooltipID );

	pTooltip->SetLocalizationParent( panelPtr );
	pTooltip->SetParameters( pszTitle, pszText );
	pTooltip->SwitchClass( k_szTooltipClassAttribute, pszClass );

	return ShowTooltip( k_szTitleTextTooltipID, panelPtr );
}

bool CUI_TooltipManager::ShowTitleTextTooltip( const panorama::CPanelPtr< panorama::IUIPanel >& panelPtr, const char *pszTitle, const char *pszText )
{
	return ShowTitleTextTooltip( panelPtr, pszTitle, pszText, NULL );
}

bool CUI_TooltipManager::HideTitleTextTooltip( const panorama::CPanelPtr< panorama::IUIPanel >& panelPtr )
{
	return HideTooltip( k_szTitleTextTooltipID );
}

bool CUI_TooltipManager::ShowTitleImageTextTooltip( const panorama::CPanelPtr< panorama::IUIPanel >& panelPtr, const char *pszTitle, const char *pszImagePath, const char *pszText, const char *pszClass )
{
	if ( !ShouldHandleTooltipEvent( panelPtr ) )
		return false;

	CUI_Tooltip_TitleImageText *pTooltip = EnsureTooltip< CUI_Tooltip_TitleImageText >( k_szTitleImageTextTooltipID );

	pTooltip->SetLocalizationParent( panelPtr );
	pTooltip->SetParameters( pszTitle, pszImagePath, pszText );
	pTooltip->SwitchClass( k_szTooltipClassAttribute, pszClass );

	return ShowTooltip( k_szTitleImageTextTooltipID, panelPtr );
}

bool CUI_TooltipManager::ShowTitleImageTextTooltip( const panorama::CPanelPtr< panorama::IUIPanel >& panelPtr, const char *pszTitle, const char *pszImagePath, const char *pszText )
{
	return ShowTitleImageTextTooltip( panelPtr, pszTitle, pszImagePath, pszText, NULL );
}

bool CUI_TooltipManager::HideTitleImageTextTooltip( const panorama::CPanelPtr< panorama::IUIPanel >& panelPtr )
{
	return HideTooltip( k_szTitleImageTextTooltipID );
}

bool CUI_TooltipManager::ShowCustomLayoutTooltip( const panorama::CPanelPtr< panorama::IUIPanel >& panelPtr, const char *pszTooltipID, const char *pszLayoutXml )
{
	return ShowCustomLayoutParametersTooltip( panelPtr, pszTooltipID, pszLayoutXml, nullptr, nullptr );
}

bool CUI_TooltipManager::ShowCustomLayoutTooltip( const panorama::CPanelPtr< panorama::IUIPanel >& panelPtr, const char *pszTooltipID, const char *pszLayoutXml, const char *pszClass )
{
	return ShowCustomLayoutParametersTooltip( panelPtr, pszTooltipID, pszLayoutXml, nullptr, pszClass );
}

bool CUI_TooltipManager::ShowCustomLayoutParametersTooltip( const panorama::CPanelPtr< panorama::IUIPanel >& panelPtr, const char *pszTooltipID, const char *pszLayoutXml, const char *pszParameters )
{
	return ShowCustomLayoutParametersTooltip( panelPtr, pszTooltipID, pszLayoutXml, pszParameters, nullptr );
}

bool CUI_TooltipManager::ShowCustomLayoutParametersTooltip( const panorama::CPanelPtr< panorama::IUIPanel >& panelPtr, const char *pszTooltipID, const char *pszLayoutXml, const char *pszParameters, const char *pszClass )
{
	if ( !ShouldHandleTooltipEvent( panelPtr ) )
		return false;

	CUI_Tooltip_CustomLayout *pTooltip = EnsureTooltip< CUI_Tooltip_CustomLayout >( pszTooltipID );

	pTooltip->SetTooltipContents( pszLayoutXml, pszParameters );
	pTooltip->SwitchClass( k_szTooltipClassAttribute, pszClass );

	return ShowTooltip( pszTooltipID, panelPtr );
}

bool CUI_TooltipManager::HideCustomLayoutTooltip( const panorama::CPanelPtr< panorama::IUIPanel >& panelPtr, const char *pszTooltipID )
{
	return HideTooltip( pszTooltipID );
}
