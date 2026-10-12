//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx_client.h"
#include "panorama/controls/panel2d.h"
#include "panorama/controls/button.h"
#include "panorama/controls/label.h"
#include "panorama/controls/contextmenu.h"
#include "contextui.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>



using namespace panorama;

REGISTER_PANEL2D( CContextMenu, ContextMenu );
REGISTER_PANEL2D( CSimpleContextMenu, SimpleContextMenu );

DEFINE_PANORAMA_EVENT( ContextMenuEvent );
DEFINE_PANORAMA_EVENT( ContextMenuEventDirect );
DEFINE_PANORAMA_EVENT( DismissAllContextMenus );


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CContextMenu::CContextMenu( CPanel2D *pParent, const char *pchName, CPanel2D *pEventParent ) : CPanel2D( pParent, pchName )
{
	Initialize( pEventParent );
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CContextMenu::CContextMenu( IUIWindow *pParent, const char *pchName, CPanel2D *pEventParent ) : CPanel2D( pParent, pchName )
{
	Initialize( pEventParent );
}


//-----------------------------------------------------------------------------
// Purpose: Initialize
//-----------------------------------------------------------------------------
void CContextMenu::Initialize( CPanel2D *pEventParent )
{
	m_bReposition = true;

	m_flCreateTime = UIEngine()->GetCurrentFrameTime();
	m_pEventParent = pEventParent;
	m_pDismissEvent = nullptr;

	RegisterEventHandler( ContextMenuEvent(), this, &CContextMenu::OnFireEvent );
	RegisterEventHandler( ContextMenuEventDirect(), this, &CContextMenu::OnFireEvent );
	RegisterEventHandler( panorama::Cancelled( ), this, &CContextMenu::OnCancelled );
	RegisterForUnhandledEvent( DismissAllContextMenus(), this, &CContextMenu::OnDismissAll );

	SetTopOfInputContext( true );
	SetAcceptsInput( true );

	m_flFadeoutTime = GetLayoutFileDefineFloat( "ContextMenuFadoutTime", 0.0f );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CContextMenu::~CContextMenu()
{
	UnregisterForUnhandledEvent( DismissAllContextMenus(), this, &CContextMenu::OnDismissAll );

	SAFE_DELETE( m_pDismissEvent );
}


//-----------------------------------------------------------------------------
// Purpose: forward an event to the event parent
//-----------------------------------------------------------------------------
bool CContextMenu::OnFireEvent( const panorama::CPanelPtr< panorama::IUIPanel > &ptrPanel, IUIEvent *pEvent )
{
	// Avoid button mashing causing accidental select
	if ( UIEngine()->GetCurrentFrameTime() - m_flCreateTime < m_flFadeoutTime ) 
		return true;

	OnCancelled( UIPanel(), k_ePanelEventSourceProgram );

	if ( !pEvent )
		return false;

	UIEngine()->DispatchEventAsync( m_flFadeoutTime, pEvent->Copy() );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: forward an event to the event parent
//-----------------------------------------------------------------------------
bool CContextMenu::OnFireEvent( const panorama::CPanelPtr< panorama::IUIPanel > &ptrPanel, const char *pchEventText )
{
	// Avoid button mashing causing accidental select
	if ( UIEngine()->GetCurrentFrameTime() - m_flCreateTime < m_flFadeoutTime )
		return true;

	OnCancelled( UIPanel(), k_ePanelEventSourceProgram );

	// create an event from the string param and forward on, we are just in the middle so we can close.
	// CSGO-2136: We can lose our event parent if one of the context menu actions destroyed it. CreateEventFromString can still 
	// work without our parent if the event is broadcasted. If there is an event firing that is targeting a child of our parent,
	// we will fail here but the caller will need to fix that.  
	IUIEvent *pEvent = UIEngine()->CreateEventFromString( m_pEventParent.Get() ? m_pEventParent->UIPanel() : nullptr, pchEventText, &pchEventText );
	if ( !pEvent )
	{
		// bugbug jmccaskey - support javascript 
		return false;
	}

	UIEngine()->DispatchEventAsync( m_flFadeoutTime, pEvent );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: close the menu
//-----------------------------------------------------------------------------
bool CContextMenu::OnCancelled( const CPanelPtr< IUIPanel > &pPanel, EPanelEventSource_t eSource )
{
	return OnDismissAll();
}


//-----------------------------------------------------------------------------
// Purpose: close the menu in response to a request to dismiss all context menus
//-----------------------------------------------------------------------------
bool CContextMenu::OnDismissAll()
{
	// Avoid button mashing causing accidental select
	if ( UIEngine()->GetCurrentFrameTime() - m_flCreateTime < V_atof( GetLayoutFileDefine( "ContextMenuFadoutTime" ) ) )
		return true;

	m_flCreateTime = UIEngine()->GetCurrentFrameTime(); // prevent activating again after closing

	Close();

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: close the menu
//-----------------------------------------------------------------------------
void CContextMenu::Close()
{
	AddClass( "Destructing" );

	float flDestructTime = V_atof( GetLayoutFileDefine( "ContextMenuFadoutTime" ) ) + 0.05f;
	DeleteAsync( flDestructTime );

	if ( m_pDismissEvent )
	{
		UIEngine()->DispatchEventAsync( flDestructTime, m_pDismissEvent );
		m_pDismissEvent = nullptr;
	}
}


//-----------------------------------------------------------------------------
// Dismiss the popup when clicked outside of it
//-----------------------------------------------------------------------------
bool CContextMenu::OnClick( IUIPanel *pPanel, const panorama::MouseData_t &code )
{
	if ( ( UIEngine()->GetCurrentFrameTime() - m_flCreateTime >= V_atof( GetLayoutFileDefine( "ContextMenuFadoutTime" ) ) ) &&
		( pPanel->ClientPtr() == this ) )
	{
		OnCancelled( UIPanel(), k_ePanelEventSourceProgram );
		return true;
	}
	return BaseClass::OnClick( pPanel, code );
}

//-----------------------------------------------------------------------------
// Set the panel to 
//-----------------------------------------------------------------------------
void CContextMenu::SetMenuTarget( const CPanelPtr< IUIPanel >& targetPanelPtr )
{
	m_pMenuTarget = targetPanelPtr;
}

//-----------------------------------------------------------------------------
// Purpose: Layout traverse
//-----------------------------------------------------------------------------
void CContextMenu::OnLayoutTraverse( float flFinalWidth, float flFinalHeight )
{
	BaseClass::OnLayoutTraverse( flFinalWidth, flFinalHeight );	

	if ( m_bReposition )
	{
		CPanel2D *pBody = FindChildInLayoutFile( "ContextMenuBody" );
		if ( !pBody )
		{
			pBody = this;
		}

		CPanel2D *pTargetPanel = NULL;
		if ( m_pMenuTarget.Get() )
		{
			pTargetPanel = assert_cast< CPanel2D * > ( m_pMenuTarget->ClientPtr() );
		}

		CContextUI::SLayoutTarget layoutTarget;
		CContextUI::SetupTargetForPanel( layoutTarget, pTargetPanel, GetParentWindow() );

		// Use the target panel's style information to determine how to position the tooltip relative
		// to it if possible. Fall back to the container's positioning information if necessary.
		CContextUI::SLayoutPosition layoutPosition;
		IUIPanelStyle *pStyle = pTargetPanel ? pTargetPanel->AccessStyle() : AccessStyle();
		pStyle->GetContextMenuPositions( layoutPosition.ePositions );
		pStyle->GetContextMenuBodyPosition( layoutPosition.lenHorizontalBodyPosition, layoutPosition.lenVerticalBodyPosition );
		pStyle->GetContextMenuArrowPosition( layoutPosition.lenHorizontalArrowPosition, layoutPosition.lenVerticalArrowPosition );

		CContextUI::LayoutContextUI( pBody, layoutTarget, layoutPosition );

		m_bReposition = false;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Set an event to be fired when the context menu is dismissed
//-----------------------------------------------------------------------------
void CContextMenu::SetDismissEvent( IUIEvent *pEvent )
{
	SAFE_DELETE( m_pDismissEvent );
	m_pDismissEvent = pEvent;
}



//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CSimpleContextMenu::CSimpleContextMenu( CPanel2D *pParent, const char *pchName, CPanel2D *pEventParent ) : CContextMenu( pParent, pchName, pEventParent )
{
	DbgVerify( BLoadLayout( "file://{resources}/layout/contextmenu.xml" ) );
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CSimpleContextMenu::CSimpleContextMenu( IUIWindow *pParent, const char *pchName, CPanel2D *pEventParent ) : CContextMenu( pParent, pchName, pEventParent )
{
	DbgVerify( BLoadLayout( "file://{resources}/layout/contextmenu.xml" ) );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CSimpleContextMenu::~CSimpleContextMenu() 
{

}


//-----------------------------------------------------------------------------
// Purpose: Add a menu item to the context menu, these are added in order vertically, first one gets default focus!
//-----------------------------------------------------------------------------
panorama::CPanel2D* CSimpleContextMenu::AddMenuItem( const char *pchLabelText, const char *pchEventText )
{
	IUIPanel *pEventParent = GetEventParent() ? GetEventParent()->UIPanel() : this->UIPanel();
	IUIEvent *pEvent = UIEngine()->CreateEventFromString( pEventParent, pchEventText, &pchEventText );
	return AddMenuItemEvent( pchLabelText, pEvent );
}


//-----------------------------------------------------------------------------
// Purpose: Add a menu item to the context menu, these are added in order vertically, first one gets default focus!
//-----------------------------------------------------------------------------
panorama::CPanel2D* CSimpleContextMenu::AddMenuItemEvent( const char *pchLabelText, IUIEvent *pEvent )
{
	CPanel2D *pBodyParent = FindChildInLayoutFile( "ContextMenuBody" );
	if ( !pBodyParent )
		return NULL;

	int iCount = pBodyParent->GetChildCount();

	CFmtStr strID( "Button%d", iCount );

	panorama::CButton *pButton = new panorama::CButton( pBodyParent, strID.String() );
	pButton->SetAcceptsFocus( true );
	if ( pEvent != nullptr )
	{
		pButton->SetOnActivateEvent( ContextMenuEventDirect::MakeEvent( pBodyParent, pEvent ) );
	}
	else
	{
		pButton->SetEnabled( false );
	}

	panorama::CLabel *pButtonLabel = new panorama::CLabel( pButton, NULL );
	pButtonLabel->SetText( pchLabelText, CLabel::k_ETextTypeHTML );
	if ( iCount == 0 )
		pBodyParent->SetDefaultFocus( strID.String() );

	return pButton;
}