//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "cbase.h"
#include "ui_tooltip_base.h"
#include "panorama/controls/label.h"
#include "panorama/uijsregistration.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

REGISTER_PANEL2D( CUI_Tooltip_Base, TooltipBase )
REGISTER_PANEL2D_FACTORY( CUI_TooltipContents, TooltipContents )

using namespace panorama;

CUI_Tooltip_Base::CUI_Tooltip_Base( CPanel2D *pParent, const char *pchName ) : CTooltip( pParent, pchName )
{
	Initialize();
}

CUI_Tooltip_Base::CUI_Tooltip_Base( IUIWindow *pParent, const char *pchName ) : CTooltip( pParent, pchName )
{
	Initialize();
}

void CUI_Tooltip_Base::Initialize()
{
	DbgVerify( BLoadLayout( "file://{resources}/layout/tooltips/tooltip_base.xml" ) );
	m_pContentsPanel = panel_cast< CUI_TooltipContents * >( FindChildInLayoutFile( "Contents" ) );
	m_pContentsPanel->SetTooltip( this );

	if ( !UIEngine()->BHaveEventHandlersRegisteredForType( CUI_Tooltip_Base::GetPanelSymbol() ) )
	{
		RegisterEventHandlerOnPanelType( TooltipVisible(), &CUI_Tooltip_Base::EventShowTooltip );
		RegisterEventHandlerOnPanelType( TooltipHidden(), &CUI_Tooltip_Base::EventHideTooltip );
	}
}

CUI_Tooltip_Base::~CUI_Tooltip_Base()
{
}

bool CUI_Tooltip_Base::EventShowTooltip( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel )
{
	if ( pPanel.Get() != UIPanel() )
		return false;

	m_pContentsPanel->FireTooltipShownEvent();
	return false;
}

bool CUI_Tooltip_Base::EventHideTooltip( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel )
{
	if ( pPanel.Get() != UIPanel() )
		return false;

	m_pContentsPanel->FireTooltipHiddenEvent();
	return false;
}

/* ------------------------------------------------------------------------- */

CUI_TooltipContents::CUI_TooltipContents( panorama::CPanel2D *pParent, const char *pchName )
	: CUI_CustomLayoutPanel( pParent, pchName )
	, m_pTooltip( NULL )
{
}

bool CUI_TooltipContents::BIsClientPanelEvent( CPanoramaSymbol symProperty )
{
	static const CPanoramaSymbol k_symOnTooltipLoaded( "ontooltiploaded" );
	static const CPanoramaSymbol k_symOnShowTooltip( "onshowtooltip" );
	static const CPanoramaSymbol k_symOnHideTooltip( "onhidetooltip" );

	if ( symProperty == k_symOnTooltipLoaded ||
		symProperty == k_symOnShowTooltip ||
		symProperty == k_symOnHideTooltip )
	{
		return true;
	}

	return BaseClass::BIsClientPanelEvent( symProperty );
}

void CUI_TooltipContents::FireTooltipLoadedEvent()
{
	static const CPanoramaSymbol k_symOnTooltipLoaded( "ontooltiploaded" );
	DispatchPanelEvent( k_symOnTooltipLoaded );
}

void CUI_TooltipContents::FireTooltipShownEvent()
{
	static const CPanoramaSymbol k_symOnShowTooltip( "onshowtooltip" );
	DispatchPanelEvent( k_symOnShowTooltip );
}

void CUI_TooltipContents::FireTooltipHiddenEvent()
{
	static const CPanoramaSymbol k_symOnHideTooltip( "onhidetooltip" );
	DispatchPanelEvent( k_symOnHideTooltip );
}


void CUI_TooltipContents::SetupJavascriptObjectTemplate()
{
	BaseClass::SetupJavascriptObjectTemplate();

	RegisterJSMethod( "GetTooltipTarget", PANORAMA_DELEGATE( &CUI_TooltipContents::GetTooltipTarget ) );
}