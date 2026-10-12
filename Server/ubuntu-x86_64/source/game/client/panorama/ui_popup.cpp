//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "cbase.h"
#include "panorama/ui_popup.h"
#include "ui_popup_manager.h"
#include "IGameUIFuncs.h"
#include "ui_symbols.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

REGISTER_PANEL2D( CUI_Popup, Popup )

DEFINE_PANORAMA_EVENT( UIPopupButtonClicked )
DEFINE_PANORAMA_EVENT( UIPopupButtonClickedEvent );

using namespace panorama;

CUI_Popup::CUI_Popup( CPanel2D *pParent, const char *pchID, CPanel2D *pEventParent )
	: CPanel2D( pParent, pchID )
	, m_pManager( NULL )
	, m_pEventParent( pEventParent )
	, m_eBackgroundStyle( k_EPopupBackgroundStyle_None )
	, m_pResultEvent( nullptr )
	, m_bIsCreatedByJob( false )
	, m_bReceivedPopupButtonClicked( false )
	, m_Capture( this, "Popup", k_EGameInputCaptureMouse | k_EGameInputUIEnableKeyInput )
{
	// subclass should do the BLoadLayout

	// Parent must be a popup manager
	Assert( dynamic_cast< CUI_PopupManager* >( pParent ) != nullptr );

	SetTopOfInputContext( true );
	SetAcceptsFocus( true );

	RegisterEventHandler( UIPopupButtonClicked(), this, &CUI_Popup::OnPopupButtonClicked );
	RegisterEventHandler( UIPopupButtonClickedEvent(), this, &CUI_Popup::OnPopupButtonClicked );

	// default cancel event to close the popup
	SetOnCancelEvent( UIPopupButtonClicked::MakeEvent( this, "" ) );

	AddClass( k_symHidden );
}

CUI_Popup::~CUI_Popup()
{
	if ( m_pResultEvent )
	{
		delete m_pResultEvent;
	}
}

bool CUI_Popup::BSetProperty( CPanoramaSymbol symName, const char *pchValue )
{
	static CPanoramaSymbol symPopupBackground( "popupbackground" );
	if ( symName == symPopupBackground )
	{
		if ( Q_stricmp( pchValue, "none" ) == 0 )
		{
			m_eBackgroundStyle = k_EPopupBackgroundStyle_None;
		}
		else if ( Q_stricmp( pchValue, "dim" ) == 0 )
		{
			m_eBackgroundStyle = k_EPopupBackgroundStyle_Dim;
		}
		else if ( Q_stricmp( pchValue, "dim_dismiss" ) == 0 )
		{
			m_eBackgroundStyle = k_EPopupBackgroundStyle_Dim_Dismiss;
		}
		else if ( Q_stricmp( pchValue, "blur" ) == 0 )
		{
			m_eBackgroundStyle = k_EPopupBackgroundStyle_Blur;
		}
		else if ( Q_stricmp( pchValue, "blur_dismiss" ) == 0 )
		{
			m_eBackgroundStyle = k_EPopupBackgroundStyle_Blur_Dismiss;
		}
		else
		{
			return false;
		}

		return true;
	}

	return BaseClass::BSetProperty( symName, pchValue );
}

void CUI_Popup::SetIsCreatedByJob( bool bIsCreatedByJob )
{
	m_bIsCreatedByJob = bIsCreatedByJob;
}

void CUI_Popup::OnShow()
{
	m_Capture.Enable();
}

void CUI_Popup::OnClose()
{
	m_Capture.Disable();
}

void CUI_Popup::ShowPopup()
{
	assert_cast< CUI_PopupManager* >( GetParent() )->ShowPopup( this );
}

void CUI_Popup::ShowPopupAsync( float flDelay )
{
	assert_cast< CUI_PopupManager* >( GetParent() )->ShowPopupAsync( this, flDelay );
}

panorama::IUIEvent* CUI_Popup::YldShowPopup()
{
	return assert_cast< CUI_PopupManager* >( GetParent() )->YldShowPopup( this );
}

bool CUI_Popup::IsPopupVisible()
{
	return m_pManager && m_pManager->IsPopupVisible( this );
}

void CUI_Popup::CloseAndDeleteAsync( float flDelay /* = 0.0f */ )
{
	if ( m_pManager )
	{
		m_pManager->CloseAndDeletePopup( this, flDelay );
	}
	else
	{
//		AssertMsg( false, "Trying to close a popup that isn't registered with a manager" );
		DeleteAsync( flDelay );
	}
}

bool CUI_Popup::OnPopupButtonClicked( const CPanelPtr< IUIPanel > &ptrPanel, const char *pchEventText )
{
	HandlePopupButtonClicked( pchEventText );
	return true;
}

bool CUI_Popup::OnPopupButtonClicked( const CPanelPtr< IUIPanel > &ptrPanel, IUIEvent *pEvent )
{
	HandlePopupButtonClicked( pEvent ? pEvent->Copy() : NULL );
	return true;
}

void CUI_Popup::HandlePopupButtonClicked( const char *pchEventText )
{
	// create an event from the string param and forward on to the event parent. We are just in the middle to close ourselves properly.
	// If there's no event parent, then just fire it on ourselves: an unhandled event handler might end up catching it
	CPanel2D *pEventTarget = GetEventParent() ? GetEventParent() : this;
	IUIEvent *pEvent = UIEngine()->CreateEventFromString( pEventTarget->UIPanel(), pchEventText, &pchEventText );
	HandlePopupButtonClicked( pEvent );
}

void CUI_Popup::HandlePopupButtonClicked( panorama::IUIEvent *pEvent )
{
	if ( !m_bIsCreatedByJob )
	{
		// Send the event if necessary
		if ( pEvent )
		{
			UIEngine()->DispatchEvent( pEvent );
		}

		CloseAndDeleteAsync( 0.5f );
	}
	else
	{
		m_pResultEvent = pEvent;
		m_bReceivedPopupButtonClicked = true;
	}
}

void CUI_Popup::CleanupPopupInJob()
{
	Assert( m_bIsCreatedByJob );
	CloseAndDeleteAsync( 0.5f );
}

void CUI_Popup::SetPopupBackgroundStyle( EPopupBackgroundStyle eStyle )
{
	if ( m_eBackgroundStyle == eStyle )
		return;

	m_eBackgroundStyle = eStyle;

	if ( m_pManager )
	{
		m_pManager->UpdatePopupBackgrounds();
	}
}

void CUI_Popup::OnLayoutReloaded()
{
	if ( m_pManager )
		m_pManager->OnPopupLayoutReloaded( this );
}
