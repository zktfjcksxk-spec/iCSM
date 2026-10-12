//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx_client.h"
#include "panorama/uievents.h"
#include "panorama/controls/panel2d.h"
#include "panorama/controls/slideshow.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

DEFINE_PANORAMA_EVENT( SlideShowPanelChanged ); // dispatched when the control shifts focus to a new page
DEFINE_PANORAMA_EVENT( SlideShowOnLayoutInitialized );

REGISTER_PANEL2D_FACTORY( CSlideShow, SlideShow );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CSlideShow::CSlideShow( CPanel2D *pParent, const char *pchID ) : CPanel2D( pParent, pchID )
{
	static const CPanoramaSymbol k_symLeftMouseScrollRegion( "LeftMouseScrollRegion" );
	static const CPanoramaSymbol k_symRightMouseScrollRegion( "RightMouseScrollRegion" );

	m_iFocusChild = -1;
	m_bManageFocus = true;

	m_pLeftMouseScrollRegion = new CMouseScrollRegion( this, "LeftMouseScrollRegion" );
	m_pLeftMouseScrollRegion->AddClass( k_symLeftMouseScrollRegion );

	m_pRightMouseScrollRegion = new CMouseScrollRegion( this, "RightMouseScrollRegion" );
	m_pRightMouseScrollRegion->AddClass( k_symRightMouseScrollRegion );

	RegisterEventHandler( InputFocusSet(), this, &CSlideShow::EventInputFocusSet );
	RegisterEventHandler( MouseScroll(), this, &CSlideShow::EventCarouselMouseScroll );
	RegisterEventHandler( SlideShowOnLayoutInitialized(), this, &CSlideShow::EventSlideShowOnLayoutInitialized );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CSlideShow::~CSlideShow()
{

}


//-----------------------------------------------------------------------------
// Purpose: Handles request to scroll from mouse region
//-----------------------------------------------------------------------------
bool CSlideShow::EventCarouselMouseScroll( const CPanelPtr< IUIPanel > &ptrPanel, int cRepeat )
{
	if ( m_iFocusChild < 0 || m_iFocusChild >= GetChildCount() )
		return true;

	CPanel2D *pPanel = ToPanel2D( ptrPanel.Get() );
	if ( pPanel == m_pLeftMouseScrollRegion )
	{
		DispatchEvent( MoveLeft(), GetChild( m_iFocusChild ), cRepeat );
		return true;
	}
	else if ( pPanel == m_pRightMouseScrollRegion )
	{
		DispatchEvent( MoveRight(), GetChild( m_iFocusChild ), cRepeat );
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Adds a movie to the slideshow
//-----------------------------------------------------------------------------
void CSlideShow::AddPanel( CPanel2D *pPanel, bool bDontSetFocusBySideEffect )
{
	if ( !pPanel )
		return;

	static const CPanoramaSymbol k_symSlideshowPanel( "SlideshowPanel" );

	pPanel->SetAcceptsFocus( true );
	pPanel->SetTabIndex( k_flTabIndexAuto );

	// set styles immediately to prevent initial transitions
	Assert( pPanel->GetParent() == this );
	pPanel->AddClass( k_symSlideshowPanel );

	// this control requires the child to already be added, so count below will be 1
	if ( GetChildCount() == m_iFocusChild + 1 && !bDontSetFocusBySideEffect )
	{
		SetFocusIndex( m_iFocusChild, false );
	}
	else
	{
		SetIndividualPanelStyle( GetChildIndex( pPanel ), -1, m_iFocusChild );
		SetMouseScrollVisibility( m_iFocusChild );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Deletes a child panel and updates focus
//-----------------------------------------------------------------------------
void CSlideShow::RemoveAndDeletePanel( CPanel2D *pPanel )
{
	if ( !pPanel )
		return;

	Assert( pPanel->GetParent() == this );
	int iChild = GetChildIndex( pPanel );
	if ( iChild == m_iFocusChild )
	{
		if ( m_iFocusChild == GetChildCount() - 1 )
			OnMoveLeft( 0 );
		else
			OnMoveRight( 0 );
	}

	if ( iChild < m_iFocusChild )
		m_iFocusChild--;

	delete pPanel;
}


//-----------------------------------------------------------------------------
// Purpose: Move focus right
//-----------------------------------------------------------------------------
bool CSlideShow::OnMoveRight( int nRepeats )
{
	if ( !IsEnabled() )
		return true;

	if ( ( m_iFocusChild == (GetChildCount() - 1) ) || GetChildCount() <= 1 )
		return false;

	SetFocusIndex( m_iFocusChild + 1 );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Move focus left
//-----------------------------------------------------------------------------
bool CSlideShow::OnMoveLeft( int nRepeats )
{
	if ( !IsEnabled() )
		return false;

	if ( ( m_iFocusChild == 0 ) || GetChildCount() <= 1 )
		return false;

	SetFocusIndex( m_iFocusChild - 1 );	
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Handle tab backward
//-----------------------------------------------------------------------------
bool CSlideShow::OnTabBackward( int nRepeats )
{
	return OnMoveLeft( nRepeats );
}


//-----------------------------------------------------------------------------
// Purpose: Handle tab forward
//-----------------------------------------------------------------------------
bool CSlideShow::OnTabForward( int nRepeats )
{
	return OnMoveRight( nRepeats );
}


//-----------------------------------------------------------------------------
// Purpose: Handles focus event
//-----------------------------------------------------------------------------
bool CSlideShow::EventInputFocusSet( const CPanelPtr< IUIPanel > &ptrPanel )
{
	CPanel2D *pPanel = ToPanel2D( ptrPanel.Get() );
	while ( pPanel && pPanel->GetParent() != this )
		pPanel = pPanel->GetParent();
	
	if ( pPanel )
	{
		// figure out if m_iFocusChild changed
		int iNewFocus = GetChildIndex( pPanel );
		if ( iNewFocus != m_iFocusChild )
		{
			SetPanelStyles( m_iFocusChild, iNewFocus );
			m_iFocusChild = iNewFocus;

			DispatchEvent( SlideShowPanelChanged(), this, m_iFocusChild );
		}
	}
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Set index of child to focus
//-----------------------------------------------------------------------------
void CSlideShow::SetFocusIndex( int iFocus, bool bSkipChildCountCheck )
{
	bool bChildExists = (iFocus >= 0 && iFocus < GetChildCount());
	if ( !bSkipChildCountCheck && !bChildExists )
		return;

	SetPanelStyles( m_iFocusChild, iFocus );
	m_iFocusChild = iFocus;
	
	if ( bChildExists && m_bManageFocus )
		GetChild( iFocus )->SetFocus();

	DispatchEvent( SlideShowPanelChanged(), this, m_iFocusChild );
}

//-----------------------------------------------------------------------------
// Purpose: Handle initial styles set on load from a layout file
//-----------------------------------------------------------------------------
bool CSlideShow::EventSlideShowOnLayoutInitialized( const CPanelPtr< IUIPanel > &ptrPanel )
{
	CPanel2D *pDefault = CPanel2D::GetDefaultInputFocus();
	for( int i = 0; i < GetChildCount(); ++i )
	{
		if( GetChild( i ) == pDefault )
		{
			m_iFocusChild = i;
			break;
		}
	}

	SetPanelStyles( 0, m_iFocusChild );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Handle initial styles set on load from a layout file
//-----------------------------------------------------------------------------
void CSlideShow::OnInitializedFromLayout() 
{ 
	DispatchEventAsync( 0.0, SlideShowOnLayoutInitialized(), this );
}


//-----------------------------------------------------------------------------
// Purpose: Sets panel styles for a single panel. iOldFocus can be -1 if focus didn't change and child previously had no slideshow styles applied
//-----------------------------------------------------------------------------
void CSlideShow::SetIndividualPanelStyle( int iChild, int iOldFocus, int iNewFocus )
{
	CPanel2D *pChild = GetChild( iChild );

	static const CPanoramaSymbol k_symSlideshowLeftOfFocus( "SlideShowLeftOfFocus" );
	static const CPanoramaSymbol k_symSlideshowRightOfFocus( "SlideShowRightOfFocus" );
	static const CPanoramaSymbol k_symSlideshowFocus( "SlideShowFocus" );

	if ( iOldFocus >= 0 )
	{
		int nOldOffset = abs( iOldFocus - iChild );
		if ( iChild < iOldFocus )
			pChild->RemoveClass( CFmtStr( "%s%d", k_symSlideshowLeftOfFocus.String(), nOldOffset ) );
		else if ( iChild > iOldFocus )
			pChild->RemoveClass( CFmtStr( "%s%d", k_symSlideshowRightOfFocus.String(), nOldOffset ) );
	}

	int nNewOffset = abs( iNewFocus - iChild );
	if ( iChild < iNewFocus )
	{
		pChild->AddClass( k_symSlideshowLeftOfFocus );
		pChild->AddClass( CFmtStr( "%s%d", k_symSlideshowLeftOfFocus.String(), nNewOffset ) );
		pChild->RemoveClass( k_symSlideshowRightOfFocus );
		pChild->RemoveClass( k_symSlideshowFocus );
	}
	else if ( iChild > iNewFocus )
	{
		pChild->AddClass( k_symSlideshowRightOfFocus );
		pChild->AddClass( CFmtStr( "%s%d", k_symSlideshowRightOfFocus.String(), nNewOffset ) );
		pChild->RemoveClass( k_symSlideshowLeftOfFocus );
		pChild->RemoveClass( k_symSlideshowFocus );
	}
	else
	{
		pChild->AddClass( k_symSlideshowFocus );
		pChild->RemoveClass( k_symSlideshowRightOfFocus );
		pChild->RemoveClass( k_symSlideshowLeftOfFocus );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sets mouse scroll panel visibility based on focus panel
//-----------------------------------------------------------------------------
void CSlideShow::SetMouseScrollVisibility( int iFocus )
{
	static const CPanoramaSymbol k_symDisabled( "Disabled" );

	if ( iFocus > 0 )
		m_pLeftMouseScrollRegion->RemoveClass( k_symDisabled );
	else
		m_pLeftMouseScrollRegion->AddClass( k_symDisabled );

	if ( iFocus < GetChildCount() - 1 )
		m_pRightMouseScrollRegion->RemoveClass( k_symDisabled );
	else
		m_pRightMouseScrollRegion->AddClass( k_symDisabled );
}


//-----------------------------------------------------------------------------
// Purpose: Sets panel styles (left of focus) for all panels. Focus child should be set before calling
//-----------------------------------------------------------------------------
void CSlideShow::SetPanelStyles( int iOldFocus, int iNewFocus )
{
	for ( int i = 0; i < GetChildCount(); i++ )
		SetIndividualPanelStyle( i, iOldFocus, iNewFocus );

	SetMouseScrollVisibility( iNewFocus );
}


//-----------------------------------------------------------------------------
// Purpose: Sets panel styles (left of focus) for all panels. Focus child should be set before calling
//-----------------------------------------------------------------------------
panorama::IUIPanel *CSlideShow::OnGetDefaultInputFocus()
{
	if ( m_iFocusChild < 0 || m_iFocusChild >= GetChildCount() )
		return BaseClass::OnGetDefaultInputFocus();

	IUIPanelClient *pFocus = GetChild( m_iFocusChild )->GetDefaultInputFocus();
	if( pFocus )
		return pFocus->UIPanel();

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Adds a style flag to all children and their descendants
//-----------------------------------------------------------------------------
void CSlideShow::AddDisabledFlagToChildren()
{
	/*
	if ( m_pLeftMouseScrollRegion )
		m_pLeftMouseScrollRegion->AddParentDisabledFlag();

	if ( m_pRightMouseScrollRegion )
		m_pRightMouseScrollRegion->AddParentDisabledFlag();

	BaseClass::AddDisabledFlagToChildren();
	*/
}


//-----------------------------------------------------------------------------
// Purpose: Remove a style flag from all children and their descendants
//-----------------------------------------------------------------------------
void CSlideShow::RemoveDisabledFlagFromChildren()
{
	/*
	if ( m_pLeftMouseScrollRegion )
		m_pLeftMouseScrollRegion->RemoveParentDisabledFlag();

	if ( m_pRightMouseScrollRegion )
		m_pRightMouseScrollRegion->RemoveParentDisabledFlag();

	BaseClass::RemoveDisabledFlagFromChildren();
	*/
}



#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: validate mem
//-----------------------------------------------------------------------------
void CSlideShow::ValidateClientPanel( CValidator &validator, const char *pchName )
{
	BaseClass::ValidateClientPanel( validator, pchName );

	VALIDATE_SCOPE();
}
#endif

