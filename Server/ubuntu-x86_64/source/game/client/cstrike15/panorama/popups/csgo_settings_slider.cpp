//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "cbase.h"

#include "panorama/csgo_panorama.h"
#include "panorama/controls/label.h"
#include "panorama/controls/slider.h"
#include "panorama/uijsregistration.h"
#include "panorama/iuisoundsystem.h"
#include "csgo_settings_slider.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

ConVar test_slider( "test_convar", "500", FCVAR_ARCHIVE | FCVAR_CLIENTCMD_CAN_EXECUTE, "Skips the prompt when saving a buy favorite in the buy menu" );
ConVar test_enum( "test_convar", "0", FCVAR_ARCHIVE | FCVAR_CLIENTCMD_CAN_EXECUTE, "Skips the prompt when saving a buy favorite in the buy menu" );

REGISTER_PANEL2D_FACTORY( CSGO_SettingsSlider, CSGOSettingsSlider );

using namespace panorama;

CSGO_SettingsSlider::CSGO_SettingsSlider( CPanel2D *pParent, const char *pchID )
: 
	CPanel2D( pParent, pchID ) 
	, m_ConVar( "", true ) 
{
	DbgVerify( BLoadLayout( "file://{resources}/layout/settings/settings_slider.xml" ) );
	
	m_pSliderTitle = panorama::panel_cast< CLabel * > ( FindChildInLayoutFile( "Title" ) );
	m_pSliderValue = panorama::panel_cast< CTextEntry * > ( FindChildInLayoutFile( "Value" ) );
	m_pSlider =      panorama::panel_cast< CSlider * >( FindChildInLayoutFile( "Slider" ) );
	m_flMinVal = 0.0f;
	m_flMaxVal = 0.0f;
	m_flMinDisplayPercentage = 0.0f;
	m_fl100PercentValue = 0.f;
	m_bDisplayTextAsPercent = false;
	m_bInvertConvar = false;
	m_bConstrainToRange = false;
	m_nDisplayPrecision = 0;

	RegisterEventHandler( SliderValueChanged( ), this, &CSGO_SettingsSlider::EventSliderValueChanged );
	RegisterEventHandler( TextEntrySubmit(), this, &CSGO_SettingsSlider::EventTextEntrySubmit );
}

CSGO_SettingsSlider::~CSGO_SettingsSlider( )
{
}

bool CSGO_SettingsSlider::BSetProperty( CPanoramaSymbol symName, const char *pchValue )
{
	static CPanoramaSymbol symConVarName( "convar" );
	static CPanoramaSymbol symMin( "min" );
	static CPanoramaSymbol symMax( "max" );
	static CPanoramaSymbol symText( "text" );
	static CPanoramaSymbol symPercent( "percentage" );
	static CPanoramaSymbol symMinDisplayValue( "mindisplaypercentage" );
	static CPanoramaSymbol symInvertConvar( "invert" );
	static CPanoramaSymbol symConstrainRange( "constrainrange" );
	static CPanoramaSymbol symDisplayPrecision( "displayprecision" );
	static CPanoramaSymbol sym100Percent( "value100percent" );

	if ( symName == symConVarName )
	{
		m_ConVar.Init( pchValue, false );
		if ( !m_ConVar.IsValid() )
			return false;
		return true;
	}
	else if ( symName == symMin )
	{
		m_flMinVal = atof( pchValue );
		return true;
	}
	else if ( symName == symMax )
	{
		m_flMaxVal = atof( pchValue );
		return true;
	}
	else if ( symName == symText )
	{
		m_pSliderTitle->SetText( pchValue );
		return true;
	}
	else if ( symName == symPercent)
	{
		return CSSHelpers::BParseTrueFalse( pchValue, &m_bDisplayTextAsPercent );
	}
	else if ( symName == sym100Percent )
	{
		m_fl100PercentValue = atof( pchValue );
		return true;
	}
	else if ( symName == symMinDisplayValue )
	{
		m_flMinDisplayPercentage = atof( pchValue );
		return true;
	}
	else if ( symName == symInvertConvar )
	{
		return CSSHelpers::BParseTrueFalse( pchValue, &m_bInvertConvar );
	}
	else if ( symName == symConstrainRange )
	{
		return CSSHelpers::BParseTrueFalse( pchValue, &m_bConstrainToRange );
	}
	else if ( symName == symDisplayPrecision )
	{
		m_nDisplayPrecision = Max( 0, Min( 16, atoi( pchValue ) ) );
		return true;
	}

	return BaseClass::BSetProperty( symName, pchValue );
}

bool CSGO_SettingsSlider::EventSliderValueChanged( const CPanelPtr< IUIPanel > &pPanel, float flValue )
{
	float flActualValue = Lerp( flValue, m_flMinVal, m_flMaxVal );
	UpdateLabel( flActualValue );
	UpdateOutOfBounds( flActualValue ); 	

	if ( m_bInvertConvar )
	{
		flValue = 1.0f - flValue;
		flActualValue = Lerp( flValue, m_flMinVal, m_flMaxVal );
	}
		
	if ( m_ConVar.IsValid() )
	{
		m_ConVar.SetValue( flActualValue );
	}

	static const CPanoramaSymbol k_symOnValueChanged( "onvaluechanged" );
	DispatchPanelEvent( k_symOnValueChanged );

	UIEngine()->UISoundSystem()->PlaySound("UIPanorama.settings_slider_teletype", nullptr, panorama::k_ESoundType_Effects, 1.0f, 0.5f, 0.0f);

	return true;
}

bool CSGO_SettingsSlider::EventTextEntrySubmit( const panorama::CPanelPtr<panorama::IUIPanel> &pPanel, const char *pchText )
{
	char *endPtr;
	float flVal = strtof( pchText, &endPtr );
	if ( endPtr != pchText )
	{
		if ( m_ConVar.IsValid() )
		{
			m_ConVar.SetValue( flVal );
		}
	}

	OnShow();	// Note calling OnShow will update the text box, which will correct out-of-bounds values

	return true;
}

void CSGO_SettingsSlider::OnShow()
{
	if ( m_flMinVal == m_flMaxVal )
	{
		return;	// Cannot use slider if min and max val are the same
	}

	float flVarValue = GetVarValue();
	UpdateLabel( flVarValue );	
	UpdateOutOfBounds( flVarValue );
 		
	flVarValue = Clamp( flVarValue, m_flMinVal, m_flMaxVal );
		
	float flPercent = ( flVarValue - m_flMinVal ) / ( m_flMaxVal - m_flMinVal );	
	if ( m_bInvertConvar )
	{
		flPercent = 1.0f - flPercent;
	}
	
	m_pSlider->SetValueNoEvents( flPercent ); 	
}

void CSGO_SettingsSlider::UpdateLabel( float flValue )
{
	if ( m_bDisplayTextAsPercent )
	{
		// for things with a max value of '1' we will assume the range is percentage; this is good for things like
		// game screen render quality which varies from 40%-100%.
		float flPercent = 0;
		if ( m_fl100PercentValue > 0.f )
		{
			flPercent = flValue / m_fl100PercentValue;
		}
		else
		{
			flPercent = m_flMaxVal==1.0f ? flValue : ( flValue - m_flMinVal ) / ( m_flMaxVal - m_flMinVal );
		}
		m_pSliderValue->SetText( CFmtStr( "%3.0f%%", (flPercent * 100) + m_flMinDisplayPercentage ) );
	}
	else if ( m_nDisplayPrecision > 0 )
	{
		char fmtStr[16];
		V_snprintf( fmtStr, ARRAYSIZE( fmtStr ), "%%3.%df", m_nDisplayPrecision );
		m_pSliderValue->SetText( CFmtStr( fmtStr, flValue ) );
	}
	else
	{
		m_pSliderValue->SetText( CFmtStr( "%3.f", flValue ) );
	}
}

void CSGO_SettingsSlider::UpdateOutOfBounds( float flValue )
{
	if ( ( flValue < m_flMinVal ) || ( flValue > m_flMaxVal ) )
	{
		m_pSlider->AddClass( "OutOfBounds" );
	}
	else
	{
		m_pSlider->RemoveClass( "OutOfBounds" );
	}
}

float CSGO_SettingsSlider::GetVarValue()
{
	float flResult = 0.0f;
	if ( m_ConVar.IsValid() )
	{
		flResult = m_ConVar.GetFloat();
	}

	if ( m_bConstrainToRange )
	{
		if ( flResult < m_flMinVal )
		{
			flResult = m_flMinVal;
		}
		else if ( flResult > m_flMaxVal )
		{
			flResult = m_flMaxVal;
		}
	}

	return flResult;


}

void CSGO_SettingsSlider::RestoreCVarDefault()
{
	RestoreDefault();
	OnShow();
}

bool CSGO_SettingsSlider::BIsClientPanelEvent( CPanoramaSymbol symProperty )
{
	static const CPanoramaSymbol k_symOnValueChanged( "onvaluechanged" );

	if ( symProperty == k_symOnValueChanged )
		return true;

	return BaseClass::BIsClientPanelEvent( symProperty );
}


//-----------------------------------------------------------------------------
// Purpose: Setup JS object template
//-----------------------------------------------------------------------------
void CSGO_SettingsSlider::SetupJavascriptObjectTemplate()
{
	BaseClass::SetupJavascriptObjectTemplate();

	RegisterJSAccessor( "value", PANORAMA_DELEGATE( &CSGO_SettingsSlider::GetValue ), PANORAMA_DELEGATE( &CSGO_SettingsSlider::SetValue ) );
	RegisterJSAccessor( "min", PANORAMA_DELEGATE( &CSGO_SettingsSlider::GetMin ), PANORAMA_DELEGATE( &CSGO_SettingsSlider::SetMin ) );
	RegisterJSAccessor( "max", PANORAMA_DELEGATE( &CSGO_SettingsSlider::GetMax ), PANORAMA_DELEGATE( &CSGO_SettingsSlider::SetMax ) );
	RegisterJSMethod( "RestoreCVarDefault", PANORAMA_DELEGATE( &CSGO_SettingsSlider::RestoreCVarDefault ) );
	RegisterJSMethod( "ActualValue", PANORAMA_DELEGATE( &CSGO_SettingsSlider::ActualValue ) );
	RegisterJSMethod( "OnShow", PANORAMA_DELEGATE( &CSGO_SettingsSlider::OnShow ) );
}
