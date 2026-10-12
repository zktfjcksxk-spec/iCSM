//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx_client.h"
#include "panorama/controls/numberentry.h"
#include "panorama/controls/textentry.h"
#include "panorama/controls/button.h"
#include "panorama/uijsregistration.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

REGISTER_PANEL2D_FACTORY( CNumberEntry, NumberEntry );

DECLARE_PANORAMA_EVENT0( NumberEntryUpdateText );

DEFINE_PANORAMA_EVENT( NumberEntryIncrementValue );
DEFINE_PANORAMA_EVENT( NumberEntryDecrementValue );
DEFINE_PANORAMA_EVENT( NumberEntryChanged );

CNumberEntry::CNumberEntry( CPanel2D *parent, const char * pchPanelID )
	: CPanel2D( parent, pchPanelID )
	, m_nValue( 0 )
	, m_nMin( 0 )
	, m_nMax( 1000000 )
	, m_nIncrement( 1 )
	, m_bHandlingTextChanged( false )
{
	DbgVerify( BLoadLayout( "file://{resources}/layout/number_entry.xml" ) );

	m_pTextEntry = assert_cast< CTextEntry * >( FindChildInLayoutFile( "TextEntry" ) );
	m_pIncrementButton = FindChildInLayoutFile( "IncrementButton" );
	m_pDecrementButton = FindChildInLayoutFile( "DecrementButton" );

	// Register for events
	if ( !UIEngine()->BHaveEventHandlersRegisteredForType( CNumberEntry::GetPanelSymbol() ) )
	{
		RegisterEventHandlerOnPanelType( NumberEntryIncrementValue(), &CNumberEntry::EventIncrementValue );
		RegisterEventHandlerOnPanelType( NumberEntryDecrementValue(), &CNumberEntry::EventDecrementValue );
	}

	m_pTextEntry->RaiseChangeEvents( true );
	RegisterEventHandlerOnPanel( TextEntryChanged(), m_pTextEntry->UIPanel(), this, &CNumberEntry::EventTextEntryChanged );

	SetAcceptsInput( true );
}

CNumberEntry::~CNumberEntry()
{
	UnregisterEventHandlerOnPanel( TextEntryChanged(), m_pTextEntry->UIPanel(), this, &CNumberEntry::EventTextEntryChanged );
}

bool CNumberEntry::BSetProperty( CPanoramaSymbol symName, const char *pchValue )
{
	static const CPanoramaSymbol k_symMin( "min" );
	static const CPanoramaSymbol k_symMax( "max" );
	static const CPanoramaSymbol k_symValue( "value" );
	static const CPanoramaSymbol k_symIncrement( "increment" );

	if ( symName == k_symMin )
	{
		SetMin( V_atoi( pchValue ) );
		return true;
	}
	else if ( symName == k_symMax )
	{
		SetMax( V_atoi( pchValue ) );
		return true;
	}
	else if ( symName == k_symValue )
	{
		SetValue( V_atoi( pchValue ) );
		return true;
	}
	else if ( symName == k_symIncrement )
	{
		SetIncrement( V_atoi( pchValue ) );
		return true;
	}

	return BaseClass::BSetProperty( symName, pchValue );
}

bool CNumberEntry::BIsClientPanelEvent( CPanoramaSymbol symProperty )
{
	static const CPanoramaSymbol k_symOnValueChanged( "onvaluechanged" );

	if ( symProperty == k_symOnValueChanged )
		return true;

	return BaseClass::BIsClientPanelEvent( symProperty );
}

bool CNumberEntry::OnMouseWheel( const MouseData_t &code )
{
	AdjustValue( code.m_Delta > 0 );
	return true;
}

void CNumberEntry::SetupJavascriptObjectTemplate()
{
	BaseClass::SetupJavascriptObjectTemplate();

	RegisterJSAccessor( "value", PANORAMA_DELEGATE( &CNumberEntry::GetValue ), PANORAMA_DELEGATE( &CNumberEntry::SetValue ) );
	RegisterJSAccessor( "min", PANORAMA_DELEGATE( &CNumberEntry::GetMin ), PANORAMA_DELEGATE( &CNumberEntry::SetMin ) );
	RegisterJSAccessor( "max", PANORAMA_DELEGATE( &CNumberEntry::GetMax ), PANORAMA_DELEGATE( &CNumberEntry::SetMax ) );
	RegisterJSAccessor( "increment", PANORAMA_DELEGATE( &CNumberEntry::GetIncrement ), PANORAMA_DELEGATE( &CNumberEntry::SetIncrement ) );
}

void CNumberEntry::SetValue( int nValue )
{
	SetValueInternal( nValue, true );
}

void CNumberEntry::SetValueInternal( int nValue, bool bUpdateText )
{
	int nNewValue = Clamp( nValue, m_nMin, m_nMax );
	bool bValueChanged = nNewValue != m_nValue;

	m_nValue = nNewValue;
	m_pIncrementButton->SetEnabled( m_nValue < m_nMax );
	m_pDecrementButton->SetEnabled( m_nValue > m_nMin );

	if ( bUpdateText )
	{
		CFmtStr strValue( "%d", m_nValue );
		UpdateText( strValue );
	}

	if ( bValueChanged )
	{
		DispatchEvent( NumberEntryChanged(), this );

		static const CPanoramaSymbol k_symOnValueChanged( "onvaluechanged" );
		DispatchPanelEvent( k_symOnValueChanged );
	}
}

void CNumberEntry::UpdateText( const char *pszText )
{
	if ( !pszText )
		pszText = "";

	if ( V_strcmp( pszText, m_pTextEntry->PchGetText() ) == 0 )
		return;

	// If you call SetText on a TextEntry while you're already responding to a TextEntryChanged,
	// then the TextEntry can get confused about the correct cursor position. So in that case,
	// just set the text the next frame.
	if ( m_bHandlingTextChanged )
	{
		DispatchEventAsync( 0.0f, TextEntrySetText(), m_pTextEntry, pszText );
	}
	else
	{
		m_pTextEntry->SetText( pszText );
	}
}

void CNumberEntry::Clear()
{
	SetValueInternal( 0, false );
	UpdateText( "" );
}

void CNumberEntry::SetMin( int nMin )
{
	m_nMin = nMin;

	if ( m_nValue < m_nMin )
	{
		SetValue( m_nMin );
	}
}

void CNumberEntry::SetMax( int nMax )
{
	m_nMax = nMax;

	if ( m_nValue > m_nMax )
	{
		SetValue( m_nMax );
	}
}

void CNumberEntry::SetIncrement( int nIncrement )
{
	m_nIncrement = nIncrement;
}

void CNumberEntry::IncrementValue()
{
	SetValue( m_nValue + m_nIncrement );
}

void CNumberEntry::DecrementValue()
{
	SetValue( m_nValue - m_nIncrement );
}

void CNumberEntry::AdjustValue( bool bIncrement )
{
	int nIncrement = bIncrement ? m_nIncrement : -m_nIncrement;
	SetValue( m_nValue + nIncrement );
}

bool CNumberEntry::EventIncrementValue()
{
	IncrementValue();
	return true;
}

bool CNumberEntry::EventDecrementValue()
{
	DecrementValue();
	return true;
}

bool CNumberEntry::EventTextEntryChanged( const CPanelPtr< IUIPanel > &panelPtr )
{
	Assert( panelPtr.Get() == m_pTextEntry->UIPanel() );
	if ( m_bHandlingTextChanged )
		return true;

	m_bHandlingTextChanged = true;

	CUtlString strText( m_pTextEntry->PchGetText() );
	strText.Trim();

	if ( strText.IsEmpty() )
	{
		Clear();
	}
	else
	{
		SetValueInternal( V_atoi( strText.Get() ), true );
	}

	m_bHandlingTextChanged = false;

	return true;
}