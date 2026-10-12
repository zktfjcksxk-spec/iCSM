//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: Edge scroller control.  An edge scroller is a panel that displays
//			two buttons rather than a scrollbar when a scrollbar is required.
//
//			The edge scroller can operate in two modes as determined by an
//			attribute in its layout file.  If pagescroll is true, pressing
//			one of the edge buttons will send a page up/down/left/right event
//			to the containing control.  If pagescroll is false, then mouse down
//			on the edge button will begin sending a series of repeating move
//			up/down/left/right events to the focused child of the control
//			until the mouse up occurs.  pagescroll=false is similar to the
//			mouse scrolling behavior in grids and carousels.
//
//=============================================================================//

#include "stdafx_client.h"
#include "panorama/controls/edgescroller.h"
#include "panorama/controls/button.h"
#include "panorama/controls/mousescroll.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

REGISTER_PANEL2D_FACTORY( CEdgeScroller, EdgeScroller );
REGISTER_PANEL2D( CEdgeScrollBar, EdgeScrollBar );


DEFINE_PANORAMA_EVENT( EdgeScrollerLeft );
DEFINE_PANORAMA_EVENT( EdgeScrollerRight );
DEFINE_PANORAMA_EVENT( EdgeScrollerUp );
DEFINE_PANORAMA_EVENT( EdgeScrollerDown );


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CEdgeScroller::CEdgeScroller( panorama::CPanel2D *pParent, const char *pchID )
	: CPanel2D( pParent, pchID )
{
#if defined( SOURCE2_PANORAMA )
	m_bPageScroll = true;
#else
	m_bPageScroll = false;
#endif

	RegisterEventHandler( EdgeScrollerLeft(), this, &CEdgeScroller::EventEdgeScrollerLeft );
	RegisterEventHandler( EdgeScrollerRight(), this, &CEdgeScroller::EventEdgeScrollerRight );
	RegisterEventHandler( EdgeScrollerUp(), this, &CEdgeScroller::EventEdgeScrollerUp );
	RegisterEventHandler( EdgeScrollerDown(), this, &CEdgeScroller::EventEdgeScrollerDown );

	RegisterEventHandler( InputFocusSet(), this, &CEdgeScroller::EventInputFocusSet );

	AddClass( "EdgeScroller" );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CEdgeScroller::~CEdgeScroller()
{
}


//-----------------------------------------------------------------------------
// Purpose: Set panel properties
//-----------------------------------------------------------------------------
bool CEdgeScroller::BSetProperty( CPanoramaSymbol symName, const char *pchValue )
{
	static CPanoramaSymbol symPageScroll( "pagescroll" );

	if ( symName == symPageScroll )
	{
		bool bValue = false;
		if ( !CSSHelpers::BParseTrueFalse( pchValue, &bValue ) )
			return false;

		m_bPageScroll = bValue;

		return true;
	}

	return BaseClass::BSetProperty( symName, pchValue );
}


//-----------------------------------------------------------------------------
// Purpose: Format custom properties for display in debugger
//-----------------------------------------------------------------------------
void CEdgeScroller::GetDebugPropertyInfo( CUtlVector< DebugPropertyOutput_t *> *pvecProperties )
{
	static CPanoramaSymbol symPageScroll( "pagescroll" );

	BaseClass::GetDebugPropertyInfo( pvecProperties );
	pvecProperties->AddToTail( new DebugPropertyOutput_t( symPageScroll.String(), m_bPageScroll ? "true" : "false" ) );
}


//-----------------------------------------------------------------------------
// Purpose: Handle events from scrollbar min/max buttons
//-----------------------------------------------------------------------------
bool CEdgeScroller::EventEdgeScrollerLeft( int cRepeat )
{
	Assert( !m_bPageScroll );
	if ( m_pFocusedChild.Get() )
	{
		DispatchEvent( MoveLeft(), ToPanel2D( m_pFocusedChild.Get() ), cRepeat );
	}
	return true;
}

bool CEdgeScroller::EventEdgeScrollerRight( int cRepeat )
{
	Assert( !m_bPageScroll );
	if ( m_pFocusedChild.Get() )
	{
		DispatchEvent( MoveRight(), ToPanel2D( m_pFocusedChild.Get() ), cRepeat );
	}
	return true;
}

bool CEdgeScroller::EventEdgeScrollerUp( int cRepeat )
{
	Assert( !m_bPageScroll );
	if ( m_pFocusedChild.Get() )
	{
		DispatchEvent( MoveUp(), ToPanel2D( m_pFocusedChild.Get() ), cRepeat );
	}
	return true;
}

bool CEdgeScroller::EventEdgeScrollerDown( int cRepeat )
{
	Assert( !m_bPageScroll );
	if ( m_pFocusedChild.Get() )
	{
		DispatchEvent( MoveDown(), ToPanel2D( m_pFocusedChild.Get() ), cRepeat );
	}
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Track focused child if we're not in page scroll mode. We need
//			this to throw the move events to the correct place
//-----------------------------------------------------------------------------
bool CEdgeScroller::EventInputFocusSet( const CPanelPtr< IUIPanel > &ptrPanel )
{
	if ( !m_bPageScroll && ptrPanel.Get() )
	{
		if ( ptrPanel->IsDescendantOf( this->UIPanel() ) )
		{
			m_pFocusedChild = ptrPanel;
		}
		else
		{
			m_pFocusedChild = NULL;
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Called when a new vertical scrollbar is required
//-----------------------------------------------------------------------------
IUIScrollBar *CEdgeScroller::CreateNewVerticalScrollBar( float flInitialScrollPos )
{
	UIPanel()->SetInScrollbarConstruction( true );
	CEdgeScrollBar *pScrollBar = new CEdgeScrollBar( this, "VerticalScrollBar", false, m_bPageScroll );
	UIPanel()->SetInScrollbarConstruction( false );

	pScrollBar->SetScrollWindowPosition( flInitialScrollPos );
	pScrollBar->SetVisible( true );

	return pScrollBar;
}


//-----------------------------------------------------------------------------
// Purpose: Called when a new horizontal scrollbar is required
//-----------------------------------------------------------------------------
IUIScrollBar *CEdgeScroller::CreateNewHorizontalScrollBar( float flInitialScrollPos )
{
	UIPanel()->SetInScrollbarConstruction( true );
	CEdgeScrollBar *pScrollBar = new CEdgeScrollBar( this, "HorizontalScrollBar", true, m_bPageScroll );
	UIPanel()->SetInScrollbarConstruction( false );

	pScrollBar->SetScrollWindowPosition( flInitialScrollPos );
	pScrollBar->SetVisible( true );

	return pScrollBar;
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CEdgeScrollBar::CEdgeScrollBar( CPanel2D *pParent, const char *pchID, bool bHorizontal, bool bPageScroll )
	: CBaseScrollBar( pParent, pchID )
	, m_bHorizontal( bHorizontal )
{
	SetHitTestEnabled( false );

	if ( bPageScroll )
	{
		m_pMinButton = new CButton( this, "MinButton" );
		m_pMaxButton = new CButton( this, "MaxButton" );

		m_pMinButton->SetOnMouseActivateEvent( m_bHorizontal ? PagePanelLeft::MakeEvent( this ) : PagePanelUp::MakeEvent( this ) );
		m_pMinButton->SetOnActivateEvent( m_bHorizontal ? PagePanelLeft::MakeEvent( this ) : PagePanelUp::MakeEvent( this ) );
		m_pMaxButton->SetOnMouseActivateEvent( m_bHorizontal ? PagePanelRight::MakeEvent( this ) : PagePanelDown::MakeEvent( this ) );
		m_pMaxButton->SetOnActivateEvent( m_bHorizontal ? PagePanelRight::MakeEvent( this ) : PagePanelDown::MakeEvent( this ) );
	}
	else
	{
		m_pMinButton = new CMouseScrollRegion( this, "MinButton" );
		m_pMaxButton = new CMouseScrollRegion( this, "MaxButton" );

		RegisterEventHandler( MouseScroll(), this, &CEdgeScrollBar::OnMouseScroll );
	}

	m_pMinButton->AddClass( "EdgeScrollButton" );
	m_pMinButton->SetAcceptsFocus( false );

	m_pMaxButton->AddClass( "EdgeScrollButton" );
	m_pMaxButton->SetAcceptsFocus( false );

	AddClass( m_bHorizontal ? "Horizontal" : "Vertical" );
	if ( bPageScroll )
	{
		AddClass( "PageScroll" );
	}
	pParent->AddClass( m_bHorizontal ? "CanScrollHorizontal" : "CanScrollVertical" );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CEdgeScrollBar::~CEdgeScrollBar()
{
	CPanel2D *pParent = GetParent();
	if ( pParent )
	{
		pParent->RemoveClass( m_bHorizontal ? "CanScrollHorizontal" : "CanScrollVertical" );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Scrollbar layout traverse to layout thumb panel correctly
//-----------------------------------------------------------------------------
void CEdgeScrollBar::UpdateLayout( bool bImmediateMove )
{
	// Don't actually change layout based on scroll position, so nothing to do here
	m_bLastMoveImmediate = bImmediateMove;
}


//-----------------------------------------------------------------------------
// Purpose: Set the scroll window position and update the classes that indicate
//			if we're at the top or the bottom.
//-----------------------------------------------------------------------------
void CEdgeScrollBar::SetScrollWindowPosition( float flWindowPos, bool bImmediateMove /* = false */ )
{
	BaseClass::SetScrollWindowPosition( flWindowPos, bImmediateMove );

	SetHasClass( "AtMinimum", GetScrollWindowPosition() <= 0 );
	SetHasClass( "AtMaximum", GetScrollWindowPosition() + GetScrollWindowSize() >= GetRangeMax() );
}


//-----------------------------------------------------------------------------
// Purpose: Handles request to scroll from mouse region.  Fire the appropriate
//			event to the parent panel.
//-----------------------------------------------------------------------------
bool CEdgeScrollBar::OnMouseScroll( const CPanelPtr< IUIPanel > &ptrPanel, int cRepeat )
{
	CPanel2D *pPanel = ToPanel2D( ptrPanel.Get() );
	if ( pPanel == m_pMinButton )
	{
		if ( m_bHorizontal )
			DispatchEvent( EdgeScrollerLeft(), GetParent(), cRepeat );
		else
			DispatchEvent( EdgeScrollerUp(), GetParent(), cRepeat );

		return true;
	}
	else if ( pPanel == m_pMaxButton )
	{
		if ( m_bHorizontal )
			DispatchEvent( EdgeScrollerRight(), GetParent(), cRepeat );
		else
			DispatchEvent( EdgeScrollerDown(), GetParent(), cRepeat );
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Stop a scroll in progress
//-----------------------------------------------------------------------------
void CEdgeScrollBar::StopScroll()
{
	float flCurrent =  m_bHorizontal ? GetParent()->UIPanel()->StopHorizontalScroll() : GetParent()->UIPanel()->StopVerticalScroll();
	SetScrollWindowPosition( flCurrent, true );
}