//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "cbase.h"

#include "panorama/csgo_panorama.h"
#include "csgo_settings_enum.h"
#include "panorama/controls/label.h"
#include "panorama/controls/button.h"
#include "panorama/uievents.h"
#include "panorama/uijsregistration.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

REGISTER_PANEL2D_FACTORY( CCSGO_SettingsEnum, CSGOSettingsEnum );
REGISTER_PANEL2D_FACTORY( CCSGO_SettingsEnumDropDown, CSGOSettingsEnumDropDown )

using namespace panorama;

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CCSGO_SettingsEnum::CCSGO_SettingsEnum( CPanel2D *pParent, const char *pchID )
: CPanel2D( pParent, pchID ), m_ConVar( "", true ), m_pButtonPanel( NULL )
{
	DbgVerify( BLoadLayout( "file://{resources}/layout/settings/settings_enum.xml" ) );

	RegisterEventHandler( Activated(), this, &CCSGO_SettingsEnum::EventButtonClicked );
	m_pLabel = panorama::panel_cast< CLabel * > ( FindChildInLayoutFile( "title" ) );
	m_pButtonPanel = FindChildInLayoutFile( "values" );
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CCSGO_SettingsEnum::~CCSGO_SettingsEnum( )
{
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CCSGO_SettingsEnum::BSetProperty( CPanoramaSymbol symName, const char *pchValue )
{
	static CPanoramaSymbol symConVarName( "convar" );
	static CPanoramaSymbol symText( "text" );
	if ( symName == symConVarName )
	{
		m_ConVar.Init( pchValue, false );
		if ( !m_ConVar.IsValid() )
			return false;
	}
	else if ( symName == symText )
	{
		m_pLabel->SetText( pchValue );
		return true;
	}

	OnShow();

	return BaseClass::BSetProperty( symName, pchValue );
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CCSGO_SettingsEnum::OnShow()
{
	if ( !m_pButtonPanel )
		return;

	const char *pValAsString = nullptr;
	if ( m_ConVar.IsValid() )
	{
		pValAsString = m_ConVar.GetString();
	}

	if ( !pValAsString )
		return;

	int nCount = m_pButtonPanel->GetChildCount();
	for ( int i = 0; i < nCount; ++i )
	{
		CPanel2D* pChild = m_pButtonPanel->GetChild( i );
		const char *pVal = pChild->GetAttribute( "value", nullptr );
		if ( pVal && !V_stricmp( pVal, pValAsString ) )
		{
			CRadioButton* pChildRB = panorama::panel_cast< CRadioButton* >( pChild );
			pChildRB->SetSelected( true );
		}
	}
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CCSGO_SettingsEnum::EventButtonClicked( const CPanelPtr< IUIPanel > &pPanel, EPanelEventSource_t eSource )
{
	if ( pPanel->GetPanelType() == CRadioButton::GetPanelSymbol() )
	{
		if ( m_ConVar.IsValid() )
		{
			m_ConVar.SetValue( pPanel->GetAttribute( "value", m_ConVar.GetDefault() ) );
		}
	}
	return false; // we always return false so that the root button class handles interactions as well
}

//-----------------------------------------------------------------------------
// Purpose: Called after our panel has been created (and children added from layout file)
//-----------------------------------------------------------------------------
void CCSGO_SettingsEnum::OnInitializedFromLayout( )
{
	while ( GetChildCount() > 2 ) // label + container panel
	{
		CPanel2D *pChild = GetChild( 0 );
		int i = 1;
		while ( pChild == m_pButtonPanel || pChild == m_pLabel )
		{
			pChild = GetChild( i++ );
		}
		
		pChild->SetParent( m_pButtonPanel );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CCSGO_SettingsEnumDropDown::CCSGO_SettingsEnumDropDown( panorama::CPanel2D *pParent, const char *pchID ) : 
	CDropDown( pParent, pchID ), m_ConVar( "", true )
{
	RegisterEventHandler( DropDownSelectionChanged(), this, &CCSGO_SettingsEnumDropDown::EventDropdownSelectionChanged );
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CCSGO_SettingsEnumDropDown::~CCSGO_SettingsEnumDropDown()
{
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CCSGO_SettingsEnumDropDown::SetupJavascriptObjectTemplate()
{
	BaseClass::SetupJavascriptObjectTemplate();

	RegisterJSMethod( "OnShow", PANORAMA_DELEGATE( &CCSGO_SettingsEnumDropDown::OnShow ) );
	RegisterJSMethod( "RefreshDisplay", PANORAMA_DELEGATE( &CCSGO_SettingsEnumDropDown::RefreshDisplay ) );
	RegisterJSMethod( "RestoreCVarDefault", PANORAMA_DELEGATE( &CCSGO_SettingsEnumDropDown::RestoreCVarDefault ) );
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CCSGO_SettingsEnumDropDown::BSetProperty( panorama::CPanoramaSymbol symName, const char *pchValue )
{
	static CPanoramaSymbol symConVarName( "convar" );
	if ( symName == symConVarName )
	{
		m_ConVar.Init( pchValue, false );
		if ( !m_ConVar.IsValid() )
			return false;
	}

	return BaseClass::BSetProperty( symName, pchValue );
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CCSGO_SettingsEnumDropDown::OnShow()
{
	RefreshDisplay();
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CCSGO_SettingsEnumDropDown::OnInitializedFromLayout()
{
	BaseClass::OnInitializedFromLayout();

	// Must call OnShow here, as it's only at this point that the labels that are children of CSGOSettingsEnumDropDown
	// are set up, so we can scan their values to set initial state depending on the value of the convar
	OnShow();
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CCSGO_SettingsEnumDropDown::RefreshDisplay()
{
	const char* pszConValue = nullptr;
	if ( m_ConVar.IsValid() )
	{
		pszConValue = m_ConVar.GetString();
	}

	if ( !pszConValue )
		return;

	int nChildCount = GetMenu()->GetChildCount( );
	for ( int i = 0; i < nChildCount; i++ )
	{
		CPanel2D *pPanel = GetMenu()->GetChild( i );
		if ( V_strcmp( pszConValue, pPanel->GetAttribute( "value", "" ) ) == 0 )
		{
			SetSelected( pPanel->GetID( ), true );
			InvalidateOptions( true ); // force reload of the panel
			return;
		}
	}
}

void CCSGO_SettingsEnumDropDown::RestoreCVarDefault()
{
	RestoreDefault();
	RefreshDisplay();
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CCSGO_SettingsEnumDropDown::EventDropdownSelectionChanged( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel )
{
	CPanel2D *pSelected = GetSelected();
	if ( pSelected )
	{
		if ( m_ConVar.IsValid() )
		{
			m_ConVar.SetValue( pSelected->GetAttribute( "value", m_ConVar.GetDefault() ) );
			return true;
		}
	}
	return false;
}
