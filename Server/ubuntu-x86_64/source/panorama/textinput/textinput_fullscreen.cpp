//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx_client.h"
#include "controls/label.h"
#include "controls/textentry.h"
#include "textinput/textinput.h"
#include "textinput/textinput_fullscreen.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

REGISTER_PANEL2D( CTextInputFullscreen, TextInputFullscreen );
DEFINE_PANORAMA_EVENT( TextInputFullscreenClosed );

DEFINE_PANORAMA_EVENT( TextInputFullscreenPositionChanged );

//-----------------------------------------------------------------------------
//	Constructor
//-----------------------------------------------------------------------------
CTextInputFullscreen::CTextInputFullscreen( panorama::CPanel2D *pPanel, const char * pchPanelID, const CTextInputHandlerSettings &settings ) :
		CPanel2D( pPanel, pchPanelID ), m_pTextInputHandler( NULL )
{
	SetTopOfInputContext( true );

	DbgVerify( BLoadLayout( "file://{resources}/layout/textinput/text_input_fullscreen.xml" ) );
	m_pEnteredText = assert_cast< CTextEntry* >( FindChildInLayoutFile( "EnteredText" ) );
	m_pInputDescription = assert_cast< CLabel* >( FindChildInLayoutFile( "InputDescription" ) );

	RegisterEventHandler( panorama::TextInputHandlerStateChange(), this, &CTextInputFullscreen::EventTextInputHandlerStateChange );
	RegisterEventHandler( panorama::TextInputUnhandledButtonPress(), this, &CTextInputFullscreen::EventTextInputUnhandledButtonPress );
	RegisterEventHandler( panorama::TextInputFinished(), this, &CTextInputFullscreen::EventTextInputFinished );

	// Create a text input handler and hook it up to the text input
	CTextInputHandlerSettings settingsNew;
	settingsNew.SetClasses( "HalfWidth DockLeft NoBackground" );
	settingsNew.SetID( "FullscreenWheel" );
	settingsNew.SetHideSuggestions( settings.BHideSuggestions() );
	settingsNew.SetMode( settings.GetMode() );
	m_pTextInputHandler = CreateTextInputHandler( FindChildInLayoutFile( "WheelWrapper" ), settingsNew, m_pEnteredText );
	m_pTextInputHandler->SetFocus();
	
	bDockedLeft = true;
	bDockedTop = true;
	
#if !defined( SOURCE2_PANORAMA_FIXME ) && !defined( PANORAMA_PUBLIC_STEAM_SDK )
	unCurrentPosition = ClientConfigStore()->GetInt( k_EConfigStoreUserLocal, "BigPicture\\KeyboardPosition" );
	
	if ( unCurrentPosition >= k_iPositionSlots ) 
		unCurrentPosition = 0;
	
	m_pTextInputHandler->ApplyDockingPosition( unCurrentPosition );
#else
	unCurrentPosition = 0;
#endif
	
	m_bMinimal = false;
}


//-----------------------------------------------------------------------------
//	Destructor
//-----------------------------------------------------------------------------
CTextInputFullscreen::~CTextInputFullscreen()
{
#if !defined( SOURCE2_PANORAMA_FIXME ) && !defined( PANORAMA_PUBLIC_STEAM_SDK )
	ClientConfigStore()->SetInt( k_EConfigStoreUserLocal, "BigPicture\\KeyboardPosition", unCurrentPosition );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Blocks notifications that daisy wheel state has changed from reaching UIEngine
//-----------------------------------------------------------------------------
bool CTextInputFullscreen::EventTextInputHandlerStateChange( const panorama::CPanelPtr< panorama::IUIPanel > &ptrPanel, bool bActivating )
{
	if ( bActivating && m_pTextInputHandler )
		m_pTextInputHandler->SetFocus();
	
	// don't bubble
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Blocks notifications that daisy wheel state has changed from reaching UIEngine
//-----------------------------------------------------------------------------
bool CTextInputFullscreen::EventTextInputFinished( const panorama::CPanelPtr< panorama::IUIPanel > &ptrPanel, bool bSubmitted, const char *pchText )
{	
	DispatchEventAsync( TextInputFullscreenClosed(), this, bSubmitted, m_pEnteredText->PchGetText() );

	// don't bubble
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Sets if control should allow single or multiline input
//-----------------------------------------------------------------------------
void CTextInputFullscreen::SetMultiline( bool bMultiline )
{
	static const CPanoramaSymbol k_symMultiline( "Multiline" );

	if ( bMultiline )
		AddClass( k_symMultiline );
	else
		RemoveClass( k_symMultiline );	
}


//-----------------------------------------------------------------------------
// Purpose: Sets description label
//-----------------------------------------------------------------------------
void CTextInputFullscreen::SetDescription( const char *pchDescription )
{
	m_pInputDescription->SetText( pchDescription );
}


//-----------------------------------------------------------------------------
// Purpose: Sets max chars allowed
//-----------------------------------------------------------------------------
void CTextInputFullscreen::SetMaxChars( uint32 unCharMax )
{
	m_pEnteredText->SetMaxChars( unCharMax );
}


//-----------------------------------------------------------------------------
// Purpose: Sets entered text
//-----------------------------------------------------------------------------
void CTextInputFullscreen::SetEnteredText( const char *pchText )
{
	m_pEnteredText->SetText( pchText );
}

void CTextInputFullscreen::SetMinimalMode( bool bMinimal )
{
	static const CPanoramaSymbol k_symMinimal( "MinimalKeyboard" );
	
	m_bMinimal = bMinimal;

	if ( bMinimal )
	{
		AddClass( k_symMinimal );
		m_pTextInputHandler->AddClass( k_symMinimal );
		
		m_pTextInputHandler->ApplyDockingPosition( unCurrentPosition );
	}
	else
	{
		RemoveClass( k_symMinimal );
		m_pTextInputHandler->RemoveClass( k_symMinimal );
		
		m_pTextInputHandler->RemoveClass( "DockTop" );
		m_pTextInputHandler->RemoveClass( "DockBottom" );
		m_pTextInputHandler->AddClass( "DockLeft" );
		m_pTextInputHandler->RemoveClass( "DockRight" );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Returns the default input focus for this panel
//-----------------------------------------------------------------------------
IUIPanel *CTextInputFullscreen::OnGetDefaultInputFocus()
{
	if ( m_pTextInputHandler != NULL )
	{
		return m_pTextInputHandler->UIPanel();
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: handler for button presses sent back to us by the text input overlay
//-----------------------------------------------------------------------------
bool CTextInputFullscreen::EventTextInputUnhandledButtonPress( panorama::GamePadData_t code )
{
	if( code.m_GamePadCode == STEAM_BUTTON_X || code.m_GamePadCode == XK_BUTTON_X )
	{
		DispatchEventAsync( TextInputFullscreenPositionChanged(), this );
		
		if ( m_bMinimal )
		{
			unCurrentPosition = (unCurrentPosition + 1) % k_iPositionSlots;

			m_pTextInputHandler->ApplyDockingPosition( unCurrentPosition );
		}

		return true;
	}

	return false;
}