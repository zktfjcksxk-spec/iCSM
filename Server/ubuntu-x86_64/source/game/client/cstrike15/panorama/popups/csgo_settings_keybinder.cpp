//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "cbase.h"

#include "panorama/csgo_panorama.h"
#include "csgo_settings_keybinder.h"
#include "panorama/controls/label.h"
#include "inputsystem/iinputsystem.h"
#include "inputsystem/iinputstacksystem.h"
#include "panorama/input/mousecodes.h"
#include "panorama/uijsregistration.h"

#include "IGameUIFuncs.h"


// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

REGISTER_PANEL2D_FACTORY( CCSGO_SettingsKeyBinder, CSGOSettingsKeyBinder );

using namespace panorama;

bool CCSGO_SettingsKeyBinder::sm_bIsAnyKeyBinderCapturingInput = false;
CCSGO_SettingsKeyBinder * CCSGO_SettingsKeyBinder::sm_pKeyBinderCapturingInput = nullptr;
ButtonCode_t CCSGO_SettingsKeyBinder::sm_keyJustBound = ButtonCode_t::KEY_NONE;
bool CCSGO_SettingsKeyBinder::sm_bWasIMEAllowed = false;
CUtlVector< CCSGO_SettingsKeyBinder* > CCSGO_SettingsKeyBinder::sm_vecActiveKeybinders;
bool CCSGO_SettingsKeyBinder::s_bMouseOverClearButton = false;

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------

static ButtonCode_t s_IgnoreList[] =
{
	ButtonCode_t::KEY_NONE				// Windows key comes through as NONE
};

bool ShouldIgnoreButton( ButtonCode_t nCode )
{
	for ( int iCode = 0; iCode < ARRAYSIZE(s_IgnoreList); ++iCode )
	{
		if ( s_IgnoreList[iCode] == nCode )
		{
			return true;
		}
	}

	return false;
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool ActionsAreTheSame( const char *szAction1, const char *szAction2 )
{
	if ( V_stricmp( szAction1, szAction2 ) == 0 )
		return true;

	if ( ( V_stricmp( szAction1, "+duck" ) == 0 || V_stricmp( szAction1, "toggle_duck" ) == 0 ) && 
		( V_stricmp( szAction2, "+duck" ) == 0 || V_stricmp( szAction2, "toggle_duck" ) == 0 ) )
	{
		// +duck and toggle_duck are interchangable
		return true;
	}

	if ( ( V_stricmp( szAction1, "+zoom" ) == 0 || V_stricmp( szAction1, "toggle_zoom" ) == 0 ) && 
		( V_stricmp( szAction2, "+zoom" ) == 0 || V_stricmp( szAction2, "toggle_zoom" ) == 0 ) )
	{
		// +zoom and toggle_zoom are interchangable
		return true;
	}

	return false;
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void UnbindKey( ButtonCode_t buttonCode )
{
	const char * szBinding = gameuifuncs->GetBindingForButtonCode( buttonCode );

	// Check if there's a binding for this key
	if ( !szBinding || !szBinding[0] )
		return;

	char szCommand[ 256 ];
	V_snprintf( szCommand, sizeof( szCommand ), "unbind %s", g_pInputSystem->ButtonCodeToString( buttonCode ) );
	engine->ExecuteClientCmd( szCommand );

	Msg( "%s\n", szCommand );
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void BindKey( ButtonCode_t buttonCode, const char *pszCommand )
{
	char szCommand[ 256 ];
	V_snprintf( szCommand, sizeof( szCommand ), "bind \"%s\" \"%s\"", g_pInputSystem->ButtonCodeToString( buttonCode ), pszCommand );
	engine->ExecuteClientCmd( szCommand );
	
	Msg( "%s\n", szCommand );
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
CCSGO_SettingsKeyBinder::CCSGO_SettingsKeyBinder( CPanel2D *pParent, const char *pchID )
: CPanel2D( pParent, pchID )
{
	DbgVerify( BLoadLayout( "file://{resources}/layout/settings/settings_keybinder.xml" ) );
	m_pLabel = panorama::panel_cast< CLabel * > ( FindChildInLayoutFile( "title" ) );
	m_pButton = panorama::panel_cast< CLabel * > ( FindChildInLayoutFile( "value" ) );
	m_pLabelContainer = FindChildInLayoutFile( "BindingLabelContainer" );
	m_pszBindName = NULL;

	IUIPanelClient *pClearBindingsPanelClient = FindChildInLayoutFile( "clearkeybinding");
	m_pClearBindingPanel = pClearBindingsPanelClient->UIPanel();
	m_pParentWindow = m_pClearBindingPanel->GetParentWindow();

	// This is a bit weird looking, but we need the button to get input as well as the whole control,
	// that way we get an input event sent to the control with the panel being the button (if we don't
	// do this, the clicked panel will just be the whole control)
	SetAcceptsFocus( true );
	m_pButton->SetAcceptsInput( true );

	sm_vecActiveKeybinders.AddToTail( this );
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
CCSGO_SettingsKeyBinder::~CCSGO_SettingsKeyBinder( )
{
	sm_vecActiveKeybinders.FindAndRemove( this );
	ReleaseCapture();
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CCSGO_SettingsKeyBinder::BSetProperty( CPanoramaSymbol symName, const char *pchValue )
{
	static CPanoramaSymbol symBind( "bind" );
	static CPanoramaSymbol symText( "text" );
	if ( symName == symBind )
	{
		m_pszBindName = pchValue;
		OnShow();
		return true;
	}
	else if ( symName == symText )
	{
		m_pLabel->SetText( pchValue );
		return true;
	}
	return BaseClass::BSetProperty( symName, pchValue );
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CCSGO_SettingsKeyBinder::OnShow()
{
    // If there's a language change while we're doing a binding
    // we don't want to update the key.
    if ( sm_bIsAnyKeyBinderCapturingInput )
        return;

	// Skip "+" in "+attack" and similar commands; keybinding to name system doesn't like them
	const char* szBindName = m_pszBindName;
	if ( *szBindName == '+' )
		szBindName++;
    
	m_pButton->SetTextWithDialogVariables( CFmtStr( "{v:csgo_bind:e:bind_%s}", szBindName ), panorama::CLabel::k_ETextTypeUnlocalized );
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CCSGO_SettingsKeyBinder::OnClick( IUIPanel *pPanel, const MouseData_t &code )
{
	if ( ( this == pPanel->ClientPtr() || m_pButton == pPanel->ClientPtr( ) ) && IsEnabled( ) && code.m_MouseCode == panorama::MOUSE_LEFT )
	{
		StartCapture();
		return true;
	}

	return false;
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CCSGO_SettingsKeyBinder::SetupJavascriptObjectTemplate()
{
	BaseClass::SetupJavascriptObjectTemplate();

	RegisterJSMethod( "OnShow", PANORAMA_DELEGATE( &CCSGO_SettingsKeyBinder::OnShow ) );
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool CCSGO_SettingsKeyBinder::HandleInputEvent( const InputEvent_t &event )
{
	if ( !sm_bIsAnyKeyBinderCapturingInput )
	{
		return false;
	}

	if ( event.m_nType == IE_AnalogValueChanged )
	{
		AnalogCode_t code = (AnalogCode_t)event.m_nData;
		if ((code == MOUSE_XY))
		{
			int mx = event.m_nData2;
			int my = event.m_nData3;
			Plat_WindowToScreenCoords( (PlatWindow_t)event.m_hWnd, mx, my );

			IUIPanel *pPanelUnderMouse = sm_pKeyBinderCapturingInput->m_pParentWindow->UIRenderEngine()->HitTestCoordsAgainstLatestScreenspaceQuadCoordinates( mx, my );
			s_bMouseOverClearButton = ( pPanelUnderMouse == sm_pKeyBinderCapturingInput->m_pClearBindingPanel );
		}
	}

	if ( event.m_nType == IE_ButtonPressed )
	{
		ButtonCode_t buttonCode = ButtonCode_t( event.m_nData );
		// uint nModifierFlags = event.m_nData2;
	
		if ( ShouldIgnoreButton( buttonCode ) )
		{
			// pass certain buttons through
			return false;
		}
	
		if ( buttonCode == ButtonCode_t::KEY_ESCAPE )
		{
			// consume escape, we'll respond to the IE_ButtonReleased
			return true;
		}
		else if ( ( buttonCode == ButtonCode_t::MOUSE_LEFT ) && s_bMouseOverClearButton )
		{
			// Unbind current key
			sm_pKeyBinderCapturingInput->Unbind();

			// Consume this key. We'll respond to the IE_ButtonReleased
			return true;
		}
		else
		{
			// do not allow primary navigation keys to be bound to filtered actions
			static const char *szNoFilterList[] = 
			{
				"screenshot",
			};

			static const int kNumNoFilterEntries = sizeof( szNoFilterList ) / sizeof( szNoFilterList[0] );

			for ( int idx=0; idx < kNumNoFilterEntries; ++idx )
			{
				if ( StringHasPrefix( sm_pKeyBinderCapturingInput->m_pszBindName, szNoFilterList[idx] ) )
				{
					if ( buttonCode == ButtonCode_t::JOYSTICK_FIRST ||
						 buttonCode == ButtonCode_t::MOUSE_LEFT  ||
						 buttonCode == ButtonCode_t::MOUSE_RIGHT ||
						 buttonCode == ButtonCode_t::KEY_SPACE )
					{
						// consume input
						return true;
					}
				}
			}

			// don't allow KEY_BACKQUOTE to be bound to anything other than toggleconsole
			if( (buttonCode == ButtonCode_t::KEY_BACKQUOTE) && !StringHasPrefix( sm_pKeyBinderCapturingInput->m_pszBindName, "toggleconsole" ) )
			{
				return false;
			}

			// Unbind current key
			sm_pKeyBinderCapturingInput->Unbind();

			// Bind new key 
			BindKey( buttonCode, sm_pKeyBinderCapturingInput->m_pszBindName );

			sm_keyJustBound = buttonCode;

			return true;
		}
	}
	
	//--------------------------------------------------------------------------------------------------
	//--------------------------------------------------------------------------------------------------
	if ( event.m_nType == IE_ButtonReleased )
	{
		CCSGO_SettingsKeyBinder *pInstance = sm_pKeyBinderCapturingInput;

		pInstance->ReleaseCapture();
		pInstance->OnShow();

		UpdateAllActiveKeybinders();

		return true;
	}

	//--------------------------------------------------------------------------------------------------
	//--------------------------------------------------------------------------------------------------
	if ( event.m_nType == IE_KeyCodeTyped || event.m_nType == IE_KeyCodeReleased || event.m_nType == IE_KeyTyped )
	{
		// eat all the other key input messages as well while active, so other systems don't try and do things
		return true;
	}

	//--------------------------------------------------------------------------------------------------
	//--------------------------------------------------------------------------------------------------
	// all other input passes through
	return false;
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CCSGO_SettingsKeyBinder::StartCapture()
{
	static CPanoramaSymbol symActiveBind( "ActiveBindButton" );

	if ( sm_bIsAnyKeyBinderCapturingInput )
		return;

	sm_bIsAnyKeyBinderCapturingInput = true;
	m_pButton->SetText( "" );
	m_pLabelContainer->AddClass( symActiveBind );
	SetSelected( true );
	// Suppress the IME while capturing binding input so
	// that we get raw keydown/up instead of processed input.
	sm_bWasIMEAllowed = g_pInputSystem->IsIMEAllowed();
	g_pInputSystem->SetIMEAllowed( false );
	sm_pKeyBinderCapturingInput = this;
	s_bMouseOverClearButton = false;
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CCSGO_SettingsKeyBinder::ReleaseCapture()
{
	static CPanoramaSymbol symActiveBind( "ActiveBindButton" );

	if ( sm_bIsAnyKeyBinderCapturingInput )
	{
		s_bMouseOverClearButton = false;
		sm_pKeyBinderCapturingInput = nullptr;
		Assert( sm_bIsAnyKeyBinderCapturingInput );
		g_pInputSystem->SetIMEAllowed( sm_bWasIMEAllowed );
		sm_bIsAnyKeyBinderCapturingInput = false;
		m_pLabelContainer->RemoveClass( symActiveBind );
		SetSelected( false );
	}
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CCSGO_SettingsKeyBinder::Unbind()
{
	// Get previous key for this action
	ButtonCode_t prevCode = gameuifuncs->GetButtonCodeForBind( m_pszBindName );
	UnbindKey( prevCode );
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CCSGO_SettingsKeyBinder::UpdateAllActiveKeybinders()
{
	int nNumActiveKeybinders = sm_vecActiveKeybinders.Count();
	for ( int i = 0; i < nNumActiveKeybinders; i++ )
	{
		sm_vecActiveKeybinders[i]->OnShow();
	}
}