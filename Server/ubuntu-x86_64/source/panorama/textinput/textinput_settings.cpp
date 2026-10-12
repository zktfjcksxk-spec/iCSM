//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx_client.h"
#include "panorama/textinput/textinput_settings.h"
#include "panorama/iuiengine.h"
#include "panorama/uisettings.h"
#include "panorama/layout/csshelpers.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CTextInputHandlerSettings::CTextInputHandlerSettings() : m_bCancellable( true ),
		m_bHideSuggestions( false ), m_bDoubleSpaceToDotSpace( false ),
		m_bAutoCaps( false ), m_mode( panorama::k_ETextInputModeNormal )
{
}


//-----------------------------------------------------------------------------
//	Parses property from configuration
//-----------------------------------------------------------------------------
bool CTextInputHandlerSettings::BSetProperty( CPanoramaSymbol symName, const char *pchValue )
{
	static CPanoramaSymbol symTextInputId( "textinputid" );
	static CPanoramaSymbol symTextInputCancellable( "textinputcancellable" );
	static CPanoramaSymbol symTextInputDoneActionString( "textinputdoneactionstring" );
	static CPanoramaSymbol symTextInputClass( "textinputclass" );
	static CPanoramaSymbol symTextInputHideSuggestions( "textinputhidesuggestions" );
	static CPanoramaSymbol symTextInputAutoCaps( "textinputautocaps" );
	static CPanoramaSymbol symTextInputDoubleSpace( "textinputdoublespace" );
	static CPanoramaSymbol symTextInputMode( "textinputmode" );
	static CPanoramaSymbol symHeaderLabel( "headerlabel" );
	static CPanoramaSymbol symHeaderDetailLabel( "headerdetaillabel" );

	if ( symName == symTextInputId )
	{
		m_strID = pchValue;
		return true;
	}
	else if ( symName == symTextInputCancellable )
	{
		m_bCancellable = !!V_atoi( pchValue );
		return true;
	}
	else if ( symName == symTextInputDoneActionString )
	{
		m_strDoneActionString = pchValue;
		return true;
	}
	else if ( symName == symTextInputClass )
	{
		m_strClasses = pchValue;
		return true;
	}
	else if ( symName == symTextInputHideSuggestions )
	{
		m_bHideSuggestions = !!V_atoi( pchValue );
		return true;
	}
	else if ( symName == symTextInputAutoCaps )
	{
		m_bAutoCaps = false;
		return CSSHelpers::BParseTrueFalse( pchValue, &m_bAutoCaps );
	}
	else if ( symName == symTextInputDoubleSpace )
	{
		m_bDoubleSpaceToDotSpace = false;
		return CSSHelpers::BParseTrueFalse( pchValue, &m_bDoubleSpaceToDotSpace );
	}
	else if ( symName == symTextInputMode )
	{
		m_mode = ETextInputMode_tFromName( pchValue );
		return true;
	}
	else if ( symName == symHeaderLabel )
	{
		m_strHeaderLabel = pchValue;
		return true;
	}
	else if ( symName == symHeaderDetailLabel )
	{
		m_strSubHeaderDetailLabel = pchValue;
		return true;
	}
	else
	{
		return false;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Validation
//-----------------------------------------------------------------------------
#ifdef DBGFLAG_VALIDATE
void CTextInputHandlerSettings::Validate( CValidator &validator, const tchar *pchName )
{
	VALIDATE_SCOPE();

	ValidateObj( m_strID );
	ValidateObj( m_strClasses );
	ValidateObj( m_strDoneActionString );
	ValidateObj( m_strCancelActionString );
	ValidateObj( m_strHeaderLabel );
	ValidateObj( m_strSubHeaderDetailLabel );
}
#endif
