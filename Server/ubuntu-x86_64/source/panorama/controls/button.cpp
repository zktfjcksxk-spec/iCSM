//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx_client.h"
#include "panorama/controls/button.h"
#include "panorama/controls/label.h"
#include "panorama/layout/csshelpers.h"
#include "panorama/uijsregistration.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

static const char * k_pchText = "text";
static const char * k_pchTickBox = "TickBox";
static const char * k_pchRadioBox = "RadioBox";
static const char * k_pchSelected = "selected";
static const char * k_pchGroup = "group";
static const char * k_pchHTML = "html";
static const char * k_pchAllowRawText = "allowrawtext";


REGISTER_PANEL2D_FACTORY( CButton, Button );
REGISTER_PANEL2D_FACTORY( CTextButton, TextButton );
REGISTER_PANEL2D_FACTORY( CToggleButton, ToggleButton );
REGISTER_PANEL2D_FACTORY( CRadioButton, RadioButton );

DECLARE_PANEL_EVENT1( RadioSelected, const char* );
DEFINE_PANORAMA_EVENT( RadioSelected );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CButton::CButton( CPanel2D *parent, const char * pchPanelID ) : CPanel2D( parent, pchPanelID )
{
	SetAcceptsFocus( true );

	// register for events
	if ( !UIEngine()->BHaveEventHandlersRegisteredForType( CButton::GetPanelSymbol() ) )
	{
		RegisterEventHandlerOnPanelType( Activated(), &CButton::EventActivated );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CButton::~CButton()
{
}


//-----------------------------------------------------------------------------
// Purpose: Clones the panel instance
//-----------------------------------------------------------------------------
CPanel2D *CButton::Clone()
{
	if ( !IsClonable() )
	{
		AssertMsg( false, "Panel can't be cloned (panel type or child not clonable)" );
		return NULL;
	}

	CButton *pPanel = new CButton( GetParent(), NULL );
	InitClonedPanel( pPanel );

	return pPanel;
}


//-----------------------------------------------------------------------------
// Purpose: Adds class specific data to clone a panel
//-----------------------------------------------------------------------------
void CButton::InitClonedPanel( CPanel2D *pClone )
{
	BaseClass::InitClonedPanel( pClone );
}


//-----------------------------------------------------------------------------
// Purpose: Activation event
//-----------------------------------------------------------------------------
bool CButton::EventActivated( const CPanelPtr< IUIPanel > &pPanel, EPanelEventSource_t eSource )
{
	static CPanoramaSymbol symActivated( "Activated" );
	TriggerClass( symActivated ); 

	// let bubble
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CTextButton::CTextButton( CPanel2D *parent, const char *pchPanelID )
	: CButton( parent, pchPanelID )
	, m_eTextType( CLabel::k_ETextTypePlain )
{
	m_pLabel = new CLabel( this, nullptr );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CTextButton::~CTextButton()
{
}

//-----------------------------------------------------------------------------
// Purpose: Expose JS members
//-----------------------------------------------------------------------------
void CTextButton::SetupJavascriptObjectTemplate()
{
	BaseClass::SetupJavascriptObjectTemplate();

	RegisterJSAccessor( "text", PANORAMA_DELEGATE( &CTextButton::PchGetText ), PANORAMA_DELEGATE( &CTextButton::InternalSetText ) );
}

void CTextButton::SetText( const char *pchValue, CLabel::ETextType eTextType ) 
{
	if ( eTextType != CLabel::k_ETextTypeNone )
	{
		m_eTextType = eTextType;
	}

	m_pLabel->SetText( pchValue, m_eTextType ); 
}


//-----------------------------------------------------------------------------
// Purpose: Sets whether we are allowed to put unlocalized text on the button
//-----------------------------------------------------------------------------
void CTextButton::SetAllowRawText( bool bAllow )
{
	m_pLabel->SetAllowRawText( bAllow );
}

//-----------------------------------------------------------------------------
// Purpose: Set properties from layout file
//-----------------------------------------------------------------------------
bool CTextButton::BSetProperties( const CUtlVector< ParsedPanelProperty_t > &vecProperties )
{
	bool bSuccess = true;
	const char *pchSetText = nullptr;
	FOR_EACH_VEC( vecProperties, i )
	{
		const ParsedPanelProperty_t &prop = vecProperties[ i ];
		if ( prop.m_symName == k_pchText )
		{
			pchSetText = prop.m_pchValue;
		}
		else if ( prop.m_symName == k_pchHTML )
		{
			bool bHTML = false;
			if ( !CSSHelpers::BParseTrueFalse( prop.m_pchValue, &bHTML ) )
				bSuccess = false;
			else if ( bHTML )
				m_eTextType = CLabel::k_ETextTypeHTML;
			else
				m_eTextType = CLabel::k_ETextTypePlain;
		}
		else if ( prop.m_symName == k_pchAllowRawText )
		{
			bool bAllowRawText = false;
			if ( !CSSHelpers::BParseTrueFalse( prop.m_pchValue, &bAllowRawText ) )
				bSuccess = false;

			SetAllowRawText( bAllowRawText );
		}
		else
		{
			if ( !BSetProperty( prop.m_symName, prop.m_pchValue ) )
				bSuccess = false;
		}
	}

	// Set text, if any, LAST, after we know our text type
	if ( pchSetText )
		SetText( pchSetText, m_eTextType );

	return bSuccess;
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CToggleButton::CToggleButton( CPanel2D *parent, const char * pchPanelID ) : CButton( parent, pchPanelID )
{
	// make the graphic portion a panel with a style instead of CButton object to make styling easier
	// (odds are the box will look nothing like a button and using that class would force the user to override all styles)
	m_pButton = new CPanel2D( this, NULL );

	DispatchEventAsync( 0.0f, AddStyle(), m_pButton, k_pchTickBox);
	m_pLabel = NULL;
	m_bAllowRawText = false;
	m_eTextType = CLabel::k_ETextTypePlain;

	RegisterEventHandler( Activated(), this, &CToggleButton::EventActivated );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CToggleButton::~CToggleButton()
{

}


//-----------------------------------------------------------------------------
// Purpose: Expose JS members
//-----------------------------------------------------------------------------
void CToggleButton::SetupJavascriptObjectTemplate()
{
	CButton::SetupJavascriptObjectTemplate();

	RegisterJSMethod( "SetSelected", PANORAMA_DELEGATE( &CToggleButton::SetSelected ) );
	RegisterJSAccessor( "text", PANORAMA_DELEGATE( &CToggleButton::PchGetText ), PANORAMA_DELEGATE( &CToggleButton::InternalSetText ) );
}


//-----------------------------------------------------------------------------
// Purpose: Activation event
//-----------------------------------------------------------------------------
bool CToggleButton::EventActivated( const CPanelPtr< IUIPanel > &pPanel, EPanelEventSource_t eSource )
{
	// should have come from some child or ourselves.. clicking any should toggle selection
	SetSelected( !IsSelected() );

	// let bubble
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Sets the selection state of the button
//-----------------------------------------------------------------------------
void CToggleButton::SetSelected( bool bSelected )
{
	if ( bSelected )
		AddStyleFlag( k_EStyleFlagSelected );
	else
		RemoveStyleFlag( k_EStyleFlagSelected );
}


//-----------------------------------------------------------------------------
// Purpose: Sets text for the toggle button
//-----------------------------------------------------------------------------
void CToggleButton::SetText( const char *pchText, CLabel::ETextType eTextType )
{
	if ( !pchText )
	{
		SAFE_DELETE( m_pLabel );
		return;
	}

	if ( !m_pLabel )
	{
		m_pLabel = new CLabel( this, NULL );
		m_pLabel->SetAllowRawText( m_bAllowRawText );
	}

	m_pLabel->SetText( pchText, m_eTextType );
}


//-----------------------------------------------------------------------------
// Purpose: Sets whether we are allowed to put unlocalized text on the button
//-----------------------------------------------------------------------------
void CToggleButton::SetAllowRawText( bool bAllow )
{
	m_bAllowRawText = bAllow;
	if ( m_pLabel )
		m_pLabel->SetAllowRawText( bAllow );
}

//-----------------------------------------------------------------------------
// Purpose: Set properties from layout file
//-----------------------------------------------------------------------------
bool CToggleButton::BSetProperties( const CUtlVector< ParsedPanelProperty_t > &vecProperties )
{
	bool bSuccess = true;
	const char *pchSetText = nullptr;
	FOR_EACH_VEC( vecProperties, i )
	{
		const ParsedPanelProperty_t &prop = vecProperties[i];
		if ( prop.m_symName == k_pchText )
		{
			pchSetText = prop.m_pchValue;
		}
		else if ( prop.m_symName == k_pchSelected )
		{
			bool bSelected = false;
			if ( !CSSHelpers::BParseTrueFalse( prop.m_pchValue, &bSelected ) )
				bSuccess = false;

			SetSelected( bSelected );
		}
		else if ( prop.m_symName == k_pchHTML )
		{
			bool bHTML = false;
			if ( !CSSHelpers::BParseTrueFalse( prop.m_pchValue, &bHTML ) )
				bSuccess = false;
			else if ( bHTML )
				m_eTextType = CLabel::k_ETextTypeHTML;
			else
				m_eTextType = CLabel::k_ETextTypePlain;
		}
		else if ( prop.m_symName == k_pchAllowRawText )
		{
			bool bAllowRawText = false;
			if ( !CSSHelpers::BParseTrueFalse( prop.m_pchValue, &bAllowRawText ) )
				bSuccess = false;

			SetAllowRawText( bAllowRawText );
		}
		else
		{
			if ( !BSetProperty( prop.m_symName, prop.m_pchValue ) )
				bSuccess = false;
		}
	}

	// Set text, if any, LAST, after we know our text type
	if ( pchSetText )
		SetText( pchSetText );

	return bSuccess;
}


//-----------------------------------------------------------------------------
// Purpose: Turns space into activate
//-----------------------------------------------------------------------------
bool CToggleButton::OnKeyTyped( const KeyData_t &unichar )
{
	if ( unichar.m_UniChar == ' ' )
	{
		DispatchEvent( Activated(), this, k_ePanelEventSourceKeyboard );
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CRadioButton::CRadioButton( CPanel2D *parent, const char * pchPanelID ) : CButton( parent, pchPanelID )
{
	// make the graphic portion a panel with a style instead of CButton object to make styling easier
	// (odds are the box will look nothing like a button and using that class would force the user to override all styles)
	m_pButton = new CPanel2D( this, NULL );
	m_pButton->AddClass( k_pchRadioBox );
	m_pLabel = NULL;
	m_bAllowRawText = false;
	m_eTextType = CLabel::k_ETextTypePlain;

	RegisterEventHandler( Activated(), this, &CRadioButton::EventActivated );
	RegisterForUnhandledEvent( RadioSelected(), this, &CRadioButton::EventOtherActivated );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CRadioButton::~CRadioButton()
{
	UnregisterForUnhandledEvent( RadioSelected(), this, &CRadioButton::EventOtherActivated );
}


//-----------------------------------------------------------------------------
// Purpose: JS accessor to get the currently selected panel
//-----------------------------------------------------------------------------
void CRadioButton::SetupJavascriptObjectTemplate()
{
	CButton::SetupJavascriptObjectTemplate();

	RegisterJSMethod( "GetSelectedButton", PANORAMA_DELEGATE( &CRadioButton::GetSelectedButton ) );
	RegisterJSAccessor( "group", PANORAMA_DELEGATE( &CRadioButton::GetGroup), PANORAMA_DELEGATE( &CRadioButton::SetGroup) );
}


//-----------------------------------------------------------------------------
// Purpose: Activation event
//-----------------------------------------------------------------------------
bool CRadioButton::EventActivated( const CPanelPtr< IUIPanel > &pPanel, EPanelEventSource_t eSource )
{
	SetSelected( true );

	// allow this to bubble, to fire regular activated events
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Activation event for another radio
//-----------------------------------------------------------------------------
bool CRadioButton::EventOtherActivated( const CPanelPtr< IUIPanel > &pPanel, const char *szGroup )
{
	// if the other radio was us, ignore
	if ( pPanel.Get() == this->UIPanel() )
	{
		return false;
	}

	// if the other radio was in our group, we need to deselect
	if ( m_sGroup == szGroup )
	{
		SetSelected( false );
		m_pPanelSelected = pPanel.Get();
	}

	// let other radios see the event too
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Sets the selection state of the button
//-----------------------------------------------------------------------------
void CRadioButton::SetSelected( bool bSelected )
{
	if ( bSelected )
	{
		m_pPanelSelected = this;

		AddStyleFlag( k_EStyleFlagSelected );
		FireSelectionEvent();
	}
	else
	{
		RemoveStyleFlag( k_EStyleFlagSelected );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sets text for the radio button
//-----------------------------------------------------------------------------
void CRadioButton::SetText( const char *pchText, CLabel::ETextType eTextType )
{
	if ( !pchText )
	{
		SAFE_DELETE( m_pLabel );
		return;
	}

	if ( !m_pLabel )
	{
		m_pLabel = new CLabel( this, NULL );
		m_pLabel->SetAllowRawText( m_bAllowRawText );
	}

	if ( eTextType != CLabel::k_ETextTypeNone )
	{ 
		m_eTextType = eTextType;
	}

	m_pLabel->SetText( pchText, m_eTextType );
}


//-----------------------------------------------------------------------------
// Purpose: Sets whether we are allowed to put unlocalized text on the button
//-----------------------------------------------------------------------------
void CRadioButton::SetAllowRawText( bool bAllow )
{
	m_bAllowRawText = bAllow;
	if ( m_pLabel )
		m_pLabel->SetAllowRawText( bAllow );
}

//-----------------------------------------------------------------------------
// Purpose: Fire an event letting other radios know we were just selected
//-----------------------------------------------------------------------------
void CRadioButton::FireSelectionEvent()
{
	DispatchEvent( RadioSelected(), this, GetGroup() );
}


//-----------------------------------------------------------------------------
// Purpose: Set properties from layout file
//-----------------------------------------------------------------------------
bool CRadioButton::BSetProperties( const CUtlVector< ParsedPanelProperty_t > &vecProperties )
{
	bool bSuccess = true;
	const char *pchSetText = nullptr;
	FOR_EACH_VEC( vecProperties, i )
	{
		const ParsedPanelProperty_t &prop = vecProperties[i];
		if ( prop.m_symName == k_pchText )
		{
			pchSetText = prop.m_pchValue;
		}
		else if ( prop.m_symName == k_pchSelected )
		{
			bool bSelected = false;
			if ( !CSSHelpers::BParseTrueFalse( prop.m_pchValue, &bSelected ) )
				bSuccess = false;

			SetSelected( bSelected );
		}
		else if ( prop.m_symName == k_pchHTML )
		{
			bool bHTML = false;
			if ( !CSSHelpers::BParseTrueFalse( prop.m_pchValue, &bHTML ) )
				bSuccess = false;
			else if ( bHTML )
				m_eTextType = CLabel::k_ETextTypeHTML;
			else
				m_eTextType = CLabel::k_ETextTypePlain;
		}
		else if ( prop.m_symName == k_pchAllowRawText )
		{
			bool bAllowRawText = false;
			if ( !CSSHelpers::BParseTrueFalse( prop.m_pchValue, &bAllowRawText ) )
				bSuccess = false;

			SetAllowRawText( bAllowRawText );
		}
		else if ( prop.m_symName == k_pchGroup )
		{
			SetGroup( prop.m_pchValue );
		}
		else
		{
			if ( !BSetProperty( prop.m_symName, prop.m_pchValue ) )
				bSuccess = false;
		}
	}

	// Set text, if any, LAST, after we know our text type
	if ( pchSetText )
		SetText( pchSetText );

	return bSuccess;
}

