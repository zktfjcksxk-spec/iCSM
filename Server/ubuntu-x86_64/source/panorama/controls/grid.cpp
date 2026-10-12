//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx_client.h"
#include "panorama/controls/grid.h"
#include "panorama/iuisoundsystem.h"
#include "panorama/uijsregistration.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

REGISTER_PANEL2D_FACTORY( CGrid, Grid );

DEFINE_PANORAMA_EVENT( ReadyPanelForDisplay );
DEFINE_PANORAMA_EVENT( PanelDoneWithDisplay );
DEFINE_PANORAMA_EVENT( GridMotionTimeout );
DEFINE_PANORAMA_EVENT( GridInFastMotion );
DEFINE_PANORAMA_EVENT( GridStoppingFastMotion );
DEFINE_PANORAMA_EVENT( GridPageLeft );
DEFINE_PANORAMA_EVENT( GridPageRight );
DEFINE_PANORAMA_EVENT( GridDirectionalMove );
DEFINE_PANORAMA_EVENT( ChildIndexSelected );
DEFINE_PANORAMA_EVENT( GridFlickTimeout );

static const char * k_pchCursorVisible = "CursorVisible";

//#define DEBUG_DRAG_SCROLL(...) Msg(__VA_ARGS__)
#define DEBUG_DRAG_SCROLL(...)

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CGrid::CGrid( CPanel2D *parent, const char * pchPanelID ) : CPanel2D( parent, pchPanelID )
{
	VPROF_BUDGET( "CGrid::CGrid", VPROF_BUDGETGROUP_TENFOOT );

	SetAcceptsInput( true );
	
	// Grids are all about input, need all children to get auto values
	SetTabIndex( k_flTabIndexAuto );
	SetSelectionPosition( k_flSelectionPosAuto, k_flSelectionPosAuto );

	m_nHorizontalCount = 5;
	m_nVerticalCount = 5;
	m_nOverrideFocusMargin = 0;
	m_nChildChangeInProgress = 0;
	m_bHadFocus = false;
	m_nScrollOffset = 0;
	m_flScaleOffset = 1.0f;
	m_flChildWidth = 0.0f;
	m_flChildHeight = 0.0f;
	m_flStartedMotion = 0.0f;
	m_flLastMotion = 0.0f;
	m_ulMotionSinceStart = 0;
	m_bFastMotionStarted = false;
	m_bMotionLoopRunning = false;
	m_bForceRelayout = false;
	m_bIgnoreFastMotion = false;
	m_flScrollProgress = 0.0f;
	m_bVecVisibleDirty = true;
	m_bComputingPositions = false;
	m_eScrollDirection = eScrollDirectionHorizontal;
	m_eMovementWrapDirection = eMoveNone;
	m_flLastFocusChangeTime = 0.0f;
	m_eLastMoveDirection = eMoveNone;

	m_bMouseDown = false;
	m_bDragScrolling = false;
	m_flDragScrollOffset = 0;
	m_bFocusDueToDragScroll = false;
	m_bNextPositionImmediate = false;
	m_flFlickFastMotionEnd = 0.0;

	m_pLeftMouseScrollRegion = new CMouseScrollRegion( this, "LeftMouseScrollRegion" );
	m_pRightMouseScrollRegion = new CMouseScrollRegion( this, "RightMouseScrollRegion" );

	SetScrollDirection( eScrollDirectionHorizontal );

	if( !UIEngine()->BHaveEventHandlersRegisteredForType( CGrid::GetPanelSymbol() ) )
	{
		RegisterEventHandlerOnPanelType( InputFocusSet(), &CGrid::EventInputFocusSet );
		RegisterEventHandlerOnPanelType( InputFocusLost(), &CGrid::EventInputFocusLost );
		RegisterEventHandlerOnPanelType( GridMotionTimeout(), &CGrid::MotionTimeout );
		RegisterEventHandlerOnPanelType( MouseScroll(), &CGrid::OnMouseScroll );
		RegisterEventHandlerOnPanelType( PageUp(), &CGrid::OnPageUp );
		RegisterEventHandlerOnPanelType( PageDown(), &CGrid::OnPageDown );
		RegisterEventHandlerOnPanelType( panorama::ScrollToTop(), &CGrid::OnScrollToTop );
		RegisterEventHandlerOnPanelType( panorama::ScrollToBottom(), &CGrid::OnScrollToBottom );
		RegisterEventHandlerOnPanelType( panorama::DragScrollStart(), &CGrid::EventDragScrollStart );
		RegisterEventHandlerOnPanelType( panorama::DragScrollMouseMove(), &CGrid::EventDragScrollMouseMove );
		RegisterEventHandlerOnPanelType( panorama::DragScrollEnd(), &CGrid::EventDragScrollEnd );
		RegisterEventHandlerOnPanelType( panorama::GridFlickTimeout(), &CGrid::EventGridFlickTimeout );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CGrid::~CGrid()
{
	if ( m_bHadFocus )
		UnregisterForCursorChanges();
}


//-----------------------------------------------------------------------------
// Purpose: Setup JS object template
//-----------------------------------------------------------------------------
void CGrid::SetupJavascriptObjectTemplate()
{
	CPanel2D::SetupJavascriptObjectTemplate();

	RegisterJSAccessor( "verticalcount", PANORAMA_DELEGATE( &CGrid::GetVerticalCount ), PANORAMA_DELEGATE( &CGrid::SetVerticalCount ) );
	RegisterJSAccessor( "horizontalcount", PANORAMA_DELEGATE( &CGrid::GetHorizontalCount ), PANORAMA_DELEGATE( &CGrid::SetHorizontalCount ) );
	RegisterJSAccessor( "focusmargin", PANORAMA_DELEGATE( &CGrid::GetFocusMargin ), PANORAMA_DELEGATE( &CGrid::SetFocusMargin ) );
	RegisterJSAccessor( "scrolldirection", PANORAMA_DELEGATE( &CGrid::GetScrollDirection ), PANORAMA_DELEGATE_RESOLVE( &CGrid::SetScrollDirection, const char* ) );
	RegisterJSAccessorReadOnly( "scrollprogress", PANORAMA_DELEGATE( &CGrid::GetScrollProgress ) );

	RegisterJSMethod( "SetIgnoreFastMotion", PANORAMA_DELEGATE( &CGrid::SetIgnoreFastMotion ) );
	RegisterJSMethod( "GetFocusedChildVisibleIndex", PANORAMA_DELEGATE( &CGrid::GetFocusedChildVisibleIndex ) );
	RegisterJSMethod( "ScrollPanelToLeftEdge", PANORAMA_DELEGATE( &CGrid::ScrollPanelToLeftEdge ) );
	RegisterJSMethod( "MoveFocusToTopLeft", PANORAMA_DELEGATE( &CGrid::MoveFocusToTopLeft ) );
}

//-----------------------------------------------------------------------------
// Purpose: Update local cached vec of just visible children
//-----------------------------------------------------------------------------
void CGrid::UpdateVecVisible()
{
	VPROF_BUDGET( "CGrid::UpdateVecVisible", VPROF_BUDGETGROUP_TENFOOT );

	m_vecVisibleChildren.RemoveAll();

	int cChildren = GetChildCount();
	m_vecVisibleChildren.EnsureCapacity( cChildren );
	for( int i=0; i < cChildren; ++i )
	{
		CPanel2D *pChild = GetChild( i );
		if ( pChild->BIsVisible() )
			m_vecVisibleChildren.AddToTail( pChild );
	}
	m_bVecVisibleDirty = false;
}

//-----------------------------------------------------------------------------
// Purpose: Get visible child count
//-----------------------------------------------------------------------------
int CGrid::GetVisibleChildCount() const
{
	VPROF_BUDGET( "CGrid::GetVisibleChildCount", VPROF_BUDGETGROUP_TENFOOT );
	if ( m_bVecVisibleDirty )
		const_cast<CGrid *>(this)->UpdateVecVisible();

	return m_vecVisibleChildren.Count();
}


//-----------------------------------------------------------------------------
// Purpose: Get visible child at index
//-----------------------------------------------------------------------------
CPanel2D *CGrid::GetVisibleChild( int iVisibleIndex )
{
	VPROF_BUDGET( "CGrid::GetVisibleChild", VPROF_BUDGETGROUP_TENFOOT );

	if( m_bVecVisibleDirty )
		UpdateVecVisible();

	if ( iVisibleIndex >= 0 && iVisibleIndex < m_vecVisibleChildren.Count() )
		return m_vecVisibleChildren[iVisibleIndex];
	
	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: work out the new focus child index based on the current and movement direction
//-----------------------------------------------------------------------------
bool CGrid::MoveSelection( EMoveDirection eMove, int nRepeats )
{
	VPROF_BUDGET( "CGrid::MoveSelection", VPROF_BUDGETGROUP_TENFOOT );
	int iIndex = GetFocusedChildVisibleIndex();
	bool bAtEdge = false;


	// unless this is the wrap direction block on the edge of the grid
	if ( m_eMovementWrapDirection != eMove )
	{
		switch ( eMove )
		{
		case eMoveLeft:
			if ( m_eScrollDirection == eScrollDirectionHorizontal )
			{
				if ( (iIndex - m_nVerticalCount) < 0 )
					bAtEdge = true;
			}
			else
			{
				if ( (iIndex % m_nHorizontalCount) == 0 )
					bAtEdge = true;
			}
			break;
		case eMoveRight:
			if ( m_eScrollDirection == eScrollDirectionHorizontal )
			{
				if ( (GetVisibleChildCount() - iIndex) <= m_nVerticalCount )
					bAtEdge = true;
			}
			else
			{
				if ( (iIndex % m_nHorizontalCount) == (m_nHorizontalCount - 1) || iIndex == GetVisibleChildCount() - 1 )
					bAtEdge = true;
			}
			break;
		case eMoveUp:
			if ( m_eScrollDirection == eScrollDirectionHorizontal )
			{
				if ( iIndex % m_nVerticalCount == 0 )
					bAtEdge = true;
			}
			else
			{
				if ( (iIndex - m_nHorizontalCount) < 0 )
					bAtEdge = true;
			}
			break;
		case eMoveDown:
			if ( m_eScrollDirection == eScrollDirectionHorizontal )
			{
				if ( iIndex % m_nVerticalCount == (m_nVerticalCount - 1) || iIndex == GetVisibleChildCount() - 1 )
					bAtEdge = true;
			}
			else
			{
				if ( (GetVisibleChildCount() - iIndex) <= m_nHorizontalCount )
					bAtEdge = true;
			}
			break;
		default:
			Assert( !"Unknown movetype" );
		}
	}

	if ( !bAtEdge )
	{
		// now move selection by 1 based on the movement direction and the layout style
		if ( m_eScrollDirection == eScrollDirectionHorizontal )
		{
			switch ( eMove )
			{
			case eMoveUp:
				iIndex -= 1;
				break;
			case eMoveDown:
				iIndex += 1;
				break;
			case eMoveRight:
				iIndex += m_nVerticalCount;
				break;
			case eMoveLeft:
				iIndex -= m_nVerticalCount;
				break;
			default:
				Assert( !"Unknown movetype" );
			}
		}
		else
		{
			switch ( eMove )
			{
			case eMoveUp:
				iIndex -= m_nHorizontalCount;
				break;
			case eMoveDown:
				iIndex += m_nHorizontalCount;
				break;
			case eMoveRight:
				iIndex += 1;
				break;
			case eMoveLeft:
				iIndex -= 1;
				break;
			default:
				Assert( !"Unknown movetype" );
			}
		}
	}

	if ( bAtEdge )
	{
		if ( (BSelectionPosVerticalBoundary() && (eMove == eMoveUp || eMove == eMoveDown))
			|| (BSelectionPosVerticalBoundary() && (eMove == eMoveLeft || eMove == eMoveRight)) )
		{
			// if we are at an edge and are in repeats just stop
			if ( nRepeats > 0 )
				return true;

			float flTimeSinceLastMove = (UIEngine()->GetCurrentFrameTime() - m_flLastFocusChangeTime);
			float flBumperTimeout = V_atof( GetLayoutFileDefine( "GridFocusBumperTimeout" ) );

			// at an edge but moving the same direction as last time, have a smaller timeout check
			if ( m_eLastMoveDirection == eMove &&  flTimeSinceLastMove < (flBumperTimeout / 2 ) )
				return true;

			// else direction changed, require a larger time since the last move
			if ( m_eLastMoveDirection != eMove && flTimeSinceLastMove < flBumperTimeout )
				return true;
		}

		// otherwise let the input flow out to our parent
		return false;
	}

	m_eLastMoveDirection = eMove;

	// navigate within the grid
	// we have the bounds check here in case it is invalid due to us wrapping in this direction
	if ( iIndex >= 0 && iIndex < GetVisibleChildCount() )
	{
		GetVisibleChild( iIndex )->UpdateFocusInContext();
		DispatchEvent( GridDirectionalMove(), this );
	}
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: can we scroll up
//-----------------------------------------------------------------------------
bool CGrid::BCanCustomScrollUp() const
{
	if ( m_eScrollDirection == eScrollDirectionVertical )
		return m_nScrollOffset > 0;
	else
		return false;
}


//-----------------------------------------------------------------------------
// Purpose: can we scroll down
//-----------------------------------------------------------------------------
bool CGrid::BCanCustomScrollDown() const
{
	if ( m_eScrollDirection == eScrollDirectionVertical )
		return (m_nScrollOffset + m_nHorizontalCount - m_nOverrideFocusMargin) < (GetVisibleChildCount() / m_nHorizontalCount);
	else
		return false;
}


//-----------------------------------------------------------------------------
// Purpose: can we scroll left
//-----------------------------------------------------------------------------
bool CGrid::BCanCustomScrollLeft() const
{
	if ( m_eScrollDirection == eScrollDirectionHorizontal )
		return m_nScrollOffset > 0;
	else
		return false;
}


//-----------------------------------------------------------------------------
// Purpose: can we scroll right
//-----------------------------------------------------------------------------
bool CGrid::BCanCustomScrollRight() const
{
	if ( m_eScrollDirection == eScrollDirectionHorizontal )
		return (m_nScrollOffset + m_nVerticalCount - m_nOverrideFocusMargin) < (GetVisibleChildCount() / m_nVerticalCount);
	else
		return false;
}


//-----------------------------------------------------------------------------
// Purpose: Handle move event
//-----------------------------------------------------------------------------
bool CGrid::OnMoveUp( int nRepeats )
{
	// we allow disabling grid and swallowing input
	if ( !IsEnabled() )
		return true;

	return MoveSelection( eMoveUp, nRepeats );
}


//-----------------------------------------------------------------------------
// Purpose: Handle move event
//-----------------------------------------------------------------------------
bool CGrid::OnMoveDown( int nRepeats )
{
	// we allow disabling grid and swallowing input
	if ( !IsEnabled() )
		return true;

	return MoveSelection( eMoveDown, nRepeats );
}


//-----------------------------------------------------------------------------
// Purpose: Handle move event
//-----------------------------------------------------------------------------
bool CGrid::OnMoveRight( int nRepeats )
{
	// we allow disabling grid and swallowing input
	if ( !IsEnabled() )
		return true;

	return MoveSelection( eMoveRight, nRepeats );
}


//-----------------------------------------------------------------------------
// Purpose: Handle move event
//-----------------------------------------------------------------------------
bool CGrid::OnMoveLeft( int nRepeats )
{
	// we allow disabling grid and swallowing input
	if ( !IsEnabled() )
		return true;

	return MoveSelection( eMoveLeft, nRepeats );
}


//-----------------------------------------------------------------------------
// Purpose: Handle tab forward
//-----------------------------------------------------------------------------
bool CGrid::OnTabForward( int nRepeats )
{
	// we allow disabling grid and swallowing input
	if ( !IsEnabled() )
		return true;

	int iIndex = GetFocusedChildVisibleIndex();
	int nChildren = GetVisibleChildCount();
	if ( iIndex < nChildren - 1 )
	{
		GetVisibleChild( iIndex + 1 )->UpdateFocusInContext();
		DispatchEvent( GridDirectionalMove(), this );
		return true;
	}


	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Handle tab backward
//-----------------------------------------------------------------------------
bool CGrid::OnTabBackward( int nRepeats )
{
	// we allow disabling grid and swallowing input
	if ( !IsEnabled() )
		return true;

	int iIndex = GetFocusedChildVisibleIndex();
	if ( iIndex > 0 )
	{
		GetVisibleChild( iIndex - 1 )->UpdateFocusInContext();
		DispatchEvent( GridDirectionalMove(), this );
		return true;
	}

	return false;
}



//-----------------------------------------------------------------------------
// Purpose: Registers for window cursor changes and sets appropriate style
//-----------------------------------------------------------------------------
void CGrid::RegisterForCursorChanges()
{
	RegisterForUnhandledEvent( WindowCursorShown(), this, &CGrid::EventWindowCursorShown );
	RegisterForUnhandledEvent( WindowCursorHidden(), this, &CGrid::EventWindowCursorHidden );

	if ( GetParentWindow()->BCursorVisible() )
		AddClass( k_pchCursorVisible );	
}


//-----------------------------------------------------------------------------
// Purpose: Unregisters for window cursor changes and sets appropriate style
//-----------------------------------------------------------------------------
void CGrid::UnregisterForCursorChanges()
{
	UnregisterForUnhandledEvent( WindowCursorShown(), this, &CGrid::EventWindowCursorShown );
	UnregisterForUnhandledEvent( WindowCursorHidden(), this, &CGrid::EventWindowCursorHidden );

	RemoveClass( k_pchCursorVisible );
}


//-----------------------------------------------------------------------------
// Purpose: Handles event for window cursor shown
//-----------------------------------------------------------------------------
bool CGrid::EventWindowCursorShown( IUIWindow *pWindow )
{
	if ( pWindow == GetParentWindow() )
		AddClass( k_pchCursorVisible );
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Handles event for window cursor hidden
//-----------------------------------------------------------------------------
bool CGrid::EventWindowCursorHidden( IUIWindow *pWindow )
{
	if ( pWindow == GetParentWindow() )
		RemoveClass( k_pchCursorVisible );
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Set the scroll direction for the grid
//-----------------------------------------------------------------------------
void CGrid::SetScrollDirection( EScrollDirection eScrollDirection ) 
{ 
	m_eScrollDirection = eScrollDirection;
	if ( m_eScrollDirection == eScrollDirectionVertical )
	{
		DispatchEventAsync( 0.0f, panorama::AddStyle(), m_pLeftMouseScrollRegion, "TopMouseScrollRegion" );
		DispatchEventAsync( 0.0f, panorama::AddStyle(), m_pRightMouseScrollRegion, "BottomMouseScrollRegion" );
		DispatchEventAsync( 0.0f, panorama::RemoveStyle(), m_pLeftMouseScrollRegion, "LeftMouseScrollRegion" );
		DispatchEventAsync( 0.0f, panorama::RemoveStyle(), m_pRightMouseScrollRegion, "RightMouseScrollRegion" );
	}
	else
	{
		DispatchEventAsync( 0.0f, panorama::AddStyle(), m_pLeftMouseScrollRegion, "LeftMouseScrollRegion" );
		DispatchEventAsync( 0.0f, panorama::AddStyle(), m_pRightMouseScrollRegion, "RightMouseScrollRegion" );
		DispatchEventAsync( 0.0f, panorama::RemoveStyle(), m_pLeftMouseScrollRegion, "TopMouseScrollRegion" );
		DispatchEventAsync( 0.0f, panorama::RemoveStyle(), m_pRightMouseScrollRegion, "BottomMouseScrollRegion" );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Code path for setting the "scrolldirection" value
//-----------------------------------------------------------------------------
bool CGrid::BSetScrollDirection( const char *pchValue )
{
	m_nScrollOffset = 0; // reset scroll as direction is changing
	SetScrollDirection( V_stricmp( pchValue, "vertical" ) == 0 ? eScrollDirectionVertical : eScrollDirectionHorizontal );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Set panel properties
//-----------------------------------------------------------------------------
bool CGrid::BSetProperty( CPanoramaSymbol symName, const char *pchValue )
{
	static CPanoramaSymbol symScrollDir( "scrolldirection" );
	static CPanoramaSymbol symVerticalCount( "verticalcount" );
	static CPanoramaSymbol symHorizontalCount( "horizontalcount" );
	static CPanoramaSymbol symScaleOffset( "scaleoffset" );
	static CPanoramaSymbol symMovementWrap( "movementwrap" );
	static CPanoramaSymbol symFocusMargin( "focusmargin" );

	if ( symName == symScrollDir )
	{
		return BSetScrollDirection( pchValue );
	}
	else if ( symName == symVerticalCount )
	{
		m_nVerticalCount = V_atoi( pchValue );
		return true;
	}
	else if ( symName == symHorizontalCount )
	{
		m_nHorizontalCount = V_atoi( pchValue );
		return true;
	}
	else if ( symName == symScaleOffset )
	{
		m_flScaleOffset = V_atof( pchValue );
		return true;
	}
	else if ( symName == symFocusMargin )
	{
		SetFocusMargin( V_atoi( pchValue ) );
		return true;
	}
	else if ( symName == symMovementWrap )
	{
		if ( V_stricmp( pchValue, "left" ) == 0 )
			m_eMovementWrapDirection = eMoveLeft;
		else if ( V_stricmp( pchValue, "right" ) == 0 )
			m_eMovementWrapDirection = eMoveRight;
		else if ( V_stricmp( pchValue, "up" ) == 0 )
			m_eMovementWrapDirection = eMoveUp;
		else if ( V_stricmp( pchValue, "down" ) == 0 )
			m_eMovementWrapDirection = eMoveDown;
		else
			m_eMovementWrapDirection = eMoveNone;
		return true;

	}

	return BaseClass::BSetProperty( symName, pchValue );
}


//-----------------------------------------------------------------------------
// Purpose: Layout traverse
//-----------------------------------------------------------------------------
void CGrid::OnLayoutTraverse( float flFinalWidth, float flFinalHeight )
{
	VPROF_BUDGET( "CGrid::OnLayoutTraverse", VPROF_BUDGETGROUP_TENFOOT );

	// include padding
	float flLeft, flTop, flRight, flBottom;
	AccessStyle()->GetContentInset( flFinalWidth, flFinalHeight, false, flLeft, flTop, flRight, flBottom );

	// Our total space minus padding, used to compute % based margins on children below
	const float flContainerWidth = flFinalWidth - flLeft - flRight;
	const float flContainerHeight = flFinalHeight - flTop - flBottom;

	m_flChildWidth = flContainerWidth / m_nHorizontalCount;
	m_flChildHeight = flContainerHeight / m_nVerticalCount;

	UpdateChildPositions();

	float flPosWidth = m_flChildWidth * m_flScaleOffset;
	float flPosHeight = m_flChildHeight * m_flScaleOffset;
	int nChildren = GetChildCount();

	int iVisible = 0;
	for( int i = 0; i < nChildren; ++i )
	{
		CPanel2D *pChild = GetChild( i );
		if ( !pChild->BIsVisible() )
			continue;

		int iHorPos = 0, iVerPos = 0;

		if ( m_eScrollDirection == eScrollDirectionHorizontal )
		{
			iHorPos = iVisible / m_nVerticalCount;
			iVerPos = iVisible % m_nVerticalCount;
		}
		else
		{
			iHorPos = iVisible % m_nHorizontalCount;
			iVerPos = iVisible / m_nHorizontalCount;
		}

		CUILength marginLeft, marginTop, marginRight, marginBottom;
		pChild->AccessStyle()->GetMargin( marginLeft, marginTop, marginRight, marginBottom );
		marginLeft.ConvertToLength( flContainerWidth );
		marginRight.ConvertToLength( flContainerWidth );
		marginTop.ConvertToLength( flContainerHeight );
		marginBottom.ConvertToLength( flContainerHeight );

		pChild->LayoutTraverse( flLeft + (flPosWidth*iHorPos), flTop + ( flPosHeight*iVerPos), m_flChildWidth - marginLeft.GetValue() - marginRight.GetValue(), m_flChildHeight - marginTop.GetValue() - marginBottom.GetValue() );

		++iVisible;
	}

	//m_flActualLayoutWidth = flFinalWidth;
	//m_flActualLayoutHeight = flFinalHeight;
}


//-----------------------------------------------------------------------------
// Purpose: set this panel to be the currently focused child
//-----------------------------------------------------------------------------
void CGrid::SetFocusedChild( CPanel2D *pPanel )
{
	VPROF_BUDGET( "CGrid::SetFocusedChild", VPROF_BUDGETGROUP_TENFOOT );

	DEBUG_DRAG_SCROLL( "SetFocusedChild( %s ): ", pPanel->GetID() ? pPanel->GetID() : "null" ); GetCurrentScrollPosition();
	m_pFocusedChild = pPanel;

	if ( !m_bFocusDueToDragScroll )
	{
		EndFlick();
	}

	DEBUG_DRAG_SCROLL( "After EndFlick: " ); GetCurrentScrollPosition();

	// Figure out if we need to scroll to fit the focused panel into view
	DEBUG_DRAG_SCROLL( "UpdateChildPositions: " );
	UpdateChildPositions();
	m_bFocusDueToDragScroll = false;

	DEBUG_DRAG_SCROLL( "After UpdateChildPositions: " ); GetCurrentScrollPosition();
	SetRepaint( k_EPanelRepaintFull );

	if ( !m_bHadFocus )
	{
		RegisterForCursorChanges();
		m_bHadFocus = true;
	}

	DispatchEvent( ChildIndexSelected(), this, GetChildIndex( m_pFocusedChild.Get() ) );
}


//-----------------------------------------------------------------------------
// Purpose: This panel or a child just received focus
//-----------------------------------------------------------------------------
bool CGrid::EventInputFocusSet( const CPanelPtr< IUIPanel > &ptrPanel )
{
	VPROF_BUDGET( "CGrid::EventInputFocusSet", VPROF_BUDGETGROUP_TENFOOT );

	// can't do anything w/o children
	if ( GetVisibleChildCount() == 0 )
		return false;

	m_flLastFocusChangeTime = UIEngine()->GetCurrentFrameTime();

	if ( ptrPanel.Get() )
	{
		if ( ToPanel2D( ptrPanel->GetParent() ) == this )
		{
			SetFocusedChild( ToPanel2D( ptrPanel.Get() ) );
		}
		else if ( ToPanel2D( ptrPanel.Get() ) == this && GetVisibleChildCount() )
		{
			m_pFocusedChild = GetVisibleChild( 0 ); // focus was to us, throw it to our first child
			if ( m_pFocusedChild.Get() )
			{
				m_pFocusedChild->UpdateFocusInContext();
				DispatchEvent( ChildIndexSelected(), this, 0 );
			}
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Get the focused child index
//-----------------------------------------------------------------------------
int CGrid::GetFocusedChildVisibleIndex() const
{
	VPROF_BUDGET( "CGrid::GetFocusedChildVisibleIndex", VPROF_BUDGETGROUP_TENFOOT );

	int iFocused = 0;
	int nChildren = GetChildCount();

	if ( m_pFocusedChild.Get() )
	{
		for( int i=0; i < nChildren; ++i )
		{
			CPanel2D *pChild = GetChild( i );
			if ( pChild->BIsVisible() )
			{
				if( pChild == m_pFocusedChild.Get() )
				{
					return iFocused;
				}
				++iFocused;
			}
		}
	}

	return 0;
}


//-----------------------------------------------------------------------------
// Purpose: Update the motion state
//-----------------------------------------------------------------------------
bool CGrid::MotionTimeout( const CPanelPtr< IUIPanel > &ptrPanel )
{
	VPROF_BUDGET( "CGrid::MotionTimeout", VPROF_BUDGETGROUP_TENFOOT );

	if ( ToPanel2D( ptrPanel.Get() ) != this )
		return false;

	// We set this slightly ahead in some cases and there can be rounding error, but sanity check it's not
	// really far behind which would indicate a bad clock and really bad breakage about to occur
	if( UIEngine()->GetCurrentFrameTime() < m_flLastMotion - 1.0f )
	{
		Assert( UIEngine()->GetCurrentFrameTime() >= m_flLastMotion - 1.0f );
		m_flLastMotion = 0;
	}

	if ( UIEngine()->GetCurrentFrameTime() - m_flLastMotion > 0.3f || ( m_flFlickFastMotionEnd != 0 && UIEngine()->GetCurrentFrameTime() > m_flFlickFastMotionEnd ) )
	{
		m_flStartedMotion = 0.0f;
		m_flLastMotion = 0.0f;
		m_ulMotionSinceStart = 0;
		m_bFastMotionStarted = false;
		m_bMotionLoopRunning = false;
		DispatchEvent( GridStoppingFastMotion(), this );

		return true;
	}

	if ( m_ulMotionSinceStart > 12 )
	{
		if ( !m_bFastMotionStarted )
		{
			m_bFastMotionStarted = true;
		}
		DispatchEvent( GridInFastMotion(), this );
	}
	else
	{
		// Don't clear here if we are just starting out, but do tell the panel to clear state
		if ( m_ulMotionSinceStart > 12 )
		{
			m_flStartedMotion = 0.0f;
			m_flLastMotion = 0.0f;
			m_ulMotionSinceStart = 0;
		}

		if ( m_bFastMotionStarted )
		{
			m_bFastMotionStarted = false;
			DispatchEvent( GridStoppingFastMotion(), this );
		}
	}

	DispatchEventAsync( 0.1f, GridMotionTimeout(), this );


	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Change the focused child to make sure that it inside the current
//			view.  This is used by drag scrolling.
//-----------------------------------------------------------------------------
void CGrid::MoveFocusIntoView()
{
	float flPosition = GetCurrentScrollPosition();
	int nWidth = ( m_eScrollDirection == eScrollDirectionHorizontal ? m_nVerticalCount : m_nHorizontalCount );
	int nHeight = ( m_eScrollDirection == eScrollDirectionHorizontal ? m_nHorizontalCount : m_nVerticalCount );
	int iFocus = GetFocusedChildVisibleIndex();
	int nFocusRow = iFocus / nWidth;

	int nCurrentScrollOffset;
	float flCurrentDragOffset;
	PixelOffsetToScrollAndDragOffsets( flPosition, nCurrentScrollOffset, flCurrentDragOffset );

	int iFocusLimitTop = nCurrentScrollOffset + m_nOverrideFocusMargin;
	int iFocusLimitBottom = nCurrentScrollOffset + nHeight - m_nOverrideFocusMargin;
	DEBUG_DRAG_SCROLL( "flPosition: %f, nOffset: %i, flDragOffset: %f, nFocusRow: %i, iFocuslimitTop: %i, iFocusLimitBottom: %i\n", flPosition, nCurrentScrollOffset, flCurrentDragOffset, nFocusRow, iFocusLimitTop, iFocusLimitBottom );
	if ( nFocusRow >= iFocusLimitBottom )
	{
		DEBUG_DRAG_SCROLL( "Focus is below window\n" );
		nFocusRow = iFocusLimitBottom - 1;
	}
	else if ( nFocusRow <= iFocusLimitTop )
	{
		DEBUG_DRAG_SCROLL( "Focus is above window\n" );
		nFocusRow = iFocusLimitTop + 1;
	}
	else
	{
		return;
	}

	DEBUG_DRAG_SCROLL( "Selected %i as new focus\n", nFocusRow );

	int iNewFocusedChild = MIN( nFocusRow * nWidth + ( iFocus % nWidth ), GetVisibleChildCount() - 1 );
	m_bFocusDueToDragScroll = true;
	m_flLastMotion = UIEngine()->GetCurrentFrameTime();
	GetVisibleChild(iNewFocusedChild)->UpdateFocusInContext();
}


//-----------------------------------------------------------------------------
// Purpose: Update the flick state periodically
//-----------------------------------------------------------------------------
bool CGrid::EventGridFlickTimeout( const CPanelPtr< IUIPanel > &ptrPanel )
{
	if ( BHasClass( "Flick" ) && BCustomScrollInProgress() )
	{
		DEBUG_DRAG_SCROLL( "GridFlickTimeout\n" );
		MoveFocusIntoView();
		DispatchEventAsync( 0.1f, GridFlickTimeout(), this );
	}
	else
	{
		DEBUG_DRAG_SCROLL( "GridFlickTimeout - stopping\n" );
		EndFlick();
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Handle game pad key down
//-----------------------------------------------------------------------------
bool CGrid::OnGamePadDown( const panorama::GamePadData_t &code )
{
	if ( code.m_GamePadCode == XK_BUTTON_LTRIGGER || code.m_GamePadCode == STEAM_BUTTON_LTRIGGER )
	{
		DispatchEvent( GridPageLeft(), this );
		UISoundSystem()->FadeOutAndStopSoundSample( UISoundSystem()->PlaySound( "focus_change_fastscroll", UIPanel(), k_ESoundType_Effects, 0.2f, 0.0f ), 0.4f );

		TriggerFastMotion();

		return true;
	}
	else if ( code.m_GamePadCode == XK_BUTTON_RTRIGGER || code.m_GamePadCode == STEAM_BUTTON_RTRIGGER )
	{
		DispatchEvent( GridPageRight(), this );
		UISoundSystem()->FadeOutAndStopSoundSample( UISoundSystem()->PlaySound( "focus_change_fastscroll", UIPanel(), k_ESoundType_Effects, 0.8f, 0.0f ), 0.4f );

		TriggerFastMotion();

		return true;
	}

	return BaseClass::OnGamePadDown( code );
}


//-----------------------------------------------------------------------------
// Purpose: Trigger fast motion animation temporarily
//-----------------------------------------------------------------------------
void CGrid::TriggerFastMotion()
{
	if ( !m_bFastMotionStarted )
	{
		m_flStartedMotion = UIEngine()->GetCurrentFrameTime();
		m_flLastMotion = UIEngine()->GetCurrentFrameTime() + 0.15f;
		m_ulMotionSinceStart = 24;
		if ( !m_bMotionLoopRunning )
		{
			m_bMotionLoopRunning = true;
			DispatchEvent( GridMotionTimeout(), this );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Bump the fast motion timeout time
//-----------------------------------------------------------------------------
void CGrid::BumpFastMotionTimeout()
{
	if ( m_bFastMotionStarted )
	{
		m_flLastMotion = UIEngine()->GetCurrentFrameTime() + 0.15f;
		m_ulMotionSinceStart++;
		if ( !m_bMotionLoopRunning )
		{
			m_bMotionLoopRunning = true;
			DispatchEvent( GridMotionTimeout(), this );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Scroll the grid all the way left regardless of which panel has
//			focus.
//-----------------------------------------------------------------------------
void CGrid::ScrollPanelToLeftEdge()
{
	m_pFocusedChild = nullptr;
	MoveFocusToTopLeft();
}


//-----------------------------------------------------------------------------
// Purpose: Scroll the grid so the focused panel is in the top left corner
//-----------------------------------------------------------------------------
void CGrid::MoveFocusToTopLeft()
{
	UpdateChildPositions( true );
}


//-----------------------------------------------------------------------------
// Purpose: Move a page up (or left)
//-----------------------------------------------------------------------------
bool CGrid::OnPageUp()
{
	int iFocused = GetFocusedChildVisibleIndex();
	int nChildren = GetVisibleChildCount();
	if ( nChildren > 0 )
	{
		UISoundSystem()->FadeOutAndStopSoundSample( UISoundSystem()->PlaySound( "focus_change_fastscroll", UIPanel(), k_ESoundType_Effects, 1.0f, 0.2f, 0.0f ), 0.4f );
		TriggerFastMotion();

		GetVisibleChild( MAX( 0, iFocused - (m_nVerticalCount * m_nHorizontalCount) ) )->UpdateFocusInContext();
		return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Move a page up (or right)
//-----------------------------------------------------------------------------
bool CGrid::OnPageDown()
{
	int iFocused = GetFocusedChildVisibleIndex();
	int nChildren = GetVisibleChildCount();
	if ( nChildren > 0 )
	{
		UISoundSystem()->FadeOutAndStopSoundSample( UISoundSystem()->PlaySound( "focus_change_fastscroll", UIPanel(), k_ESoundType_Effects, 1.0f, 0.2f, 0.0f ), 0.4f );
		TriggerFastMotion();

		GetVisibleChild( MIN( nChildren - 1, iFocused + (m_nVerticalCount * m_nHorizontalCount) ) )->UpdateFocusInContext();
		return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Move to the first item
//-----------------------------------------------------------------------------
bool CGrid::OnScrollToTop( const CPanelPtr< IUIPanel > &pPanel )
{
	if ( ToPanel2D( pPanel.Get() ) != this )
		return false;

	int nChildren = GetVisibleChildCount();
	if ( nChildren > 0 )
	{
		UISoundSystem()->FadeOutAndStopSoundSample( UISoundSystem()->PlaySound( "focus_change_fastscroll", UIPanel(), k_ESoundType_Effects, 1.0f, 0.2f, 0.0f ), 0.4f );
		TriggerFastMotion();

		GetVisibleChild( 0 )->UpdateFocusInContext();
		return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Move to the last item
//-----------------------------------------------------------------------------
bool CGrid::OnScrollToBottom( const CPanelPtr< IUIPanel > &pPanel )
{
	if ( ToPanel2D( pPanel.Get() ) != this )
		return false;

	int nChildren = GetVisibleChildCount();
	if ( nChildren > 0 )
	{
		UISoundSystem()->FadeOutAndStopSoundSample( UISoundSystem()->PlaySound( "focus_change_fastscroll", UIPanel(), k_ESoundType_Effects, 1.0f, 0.2f, 0.0f ), 0.4f );
		TriggerFastMotion();

		GetVisibleChild( nChildren - 1 )->UpdateFocusInContext();
		return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Handle some special hot-keys
//-----------------------------------------------------------------------------
bool CGrid::OnKeyDown( const KeyData_t &code )
{
	if ( code.m_KeyCode == KEY_HOME )
	{
		DispatchEvent( panorama::ScrollToTop(), this );
		return true;
	}
	else if ( code.m_KeyCode == KEY_END )
	{
		DispatchEvent( panorama::ScrollToBottom(), this );
		return true;
	}
	else if ( code.m_KeyCode == KEY_LEFT && ( IsControlPressed( code.m_Modifiers ) && !IsShiftPressed( code.m_Modifiers ) ) )
	{
		DispatchEvent( panorama::PageUp(), this );
		return true;
	}
	else if ( code.m_KeyCode == KEY_RIGHT && ( IsControlPressed( code.m_Modifiers ) && !IsShiftPressed( code.m_Modifiers ) ) )
	{
		DispatchEvent( panorama::PageDown(), this );
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Update the position offsets for panels
//-----------------------------------------------------------------------------
void CGrid::UpdateChildPositions( bool bForceTopLeft )
{
	VPROF_BUDGET( "CGrid::UpdateChildPositions", VPROF_BUDGETGROUP_TENFOOT );

	if ( m_nChildChangeInProgress != 0 )
	{
		// don't try to touch your children pointers if you are in the middle of a change for them
		// RemoveAndDeleteChildren just wraps deleting EVERY child with a single call to each, so your child
		// list can change radically between single calls
		//Msg( "Updating positions while deleting children\n" );
		return;
	}

	DEBUG_DRAG_SCROLL( "UpdateChildPositions, offsets %i, %f\n", m_nScrollOffset, m_flDragScrollOffset );
	int iFocused = GetFocusedChildVisibleIndex();

	int iSelectPos = 0;
	if ( m_eScrollDirection == eScrollDirectionHorizontal )
	{
		iSelectPos = iFocused / m_nVerticalCount;
		m_flScrollProgress = (float)iSelectPos / ((float)GetVisibleChildCount() / (float)m_nVerticalCount);
	}
	else
	{
		iSelectPos = iFocused / m_nHorizontalCount;
		m_flScrollProgress = (float)iSelectPos / ((float)GetVisibleChildCount() / (float)m_nHorizontalCount);
	}

	bool bChanged = m_bForceRelayout;
	m_bForceRelayout = false;

	int nPageLength = (m_eScrollDirection == eScrollDirectionHorizontal) ? m_nHorizontalCount : m_nVerticalCount;
	int nPageWidth = (m_eScrollDirection == eScrollDirectionHorizontal) ? m_nVerticalCount : m_nHorizontalCount;

	int iChange = 0;
	// Pull the focused child into full view.  Don't do this if we're dragging, or if
	// the left mouse is currently down
	if ( !BHasClass( "DragScrolling" ) && !m_bFocusDueToDragScroll && !BHasClass( "Flick" ) && !m_bMouseDown )
	{
		int iTopBound = m_nScrollOffset + m_nOverrideFocusMargin;
		if ( m_flDragScrollOffset > 0 )
		{
			iTopBound = iTopBound + 1;
		}
		int iBottomBound = m_nScrollOffset + nPageLength - m_nOverrideFocusMargin;

		if ( iSelectPos >= iBottomBound || ( bForceTopLeft && iSelectPos != m_nScrollOffset ) )
		{
			iChange = abs( m_nScrollOffset - iSelectPos );
			if ( bForceTopLeft )
			{
				m_nScrollOffset = MAX( iSelectPos, 0 );
			}
			else
			{
				m_nScrollOffset = MAX( (iSelectPos)-(nPageLength - m_nOverrideFocusMargin - 1), 0 );
			}
			m_flDragScrollOffset = 0;
			bChanged = true;
		}
		else if ( iSelectPos < iTopBound )
		{
			if ( m_nScrollOffset > 0 || ( m_nScrollOffset == 0 && m_flDragScrollOffset != 0.0 ) )
			{
				iChange = abs( m_nScrollOffset - iSelectPos );
				m_nScrollOffset = MAX( iSelectPos - m_nOverrideFocusMargin, 0 );
				m_flDragScrollOffset = 0;
				bChanged = true;
			}
		}
	}
	else
	{
		if ( m_ulMotionSinceStart == 0 )
		{
			iChange = abs( m_nScrollOffset - iSelectPos );
			if ( iChange != 0 )
			{
				bChanged = true;
			}
		}
	}

	if ( bChanged && !m_bIgnoreFastMotion )
	{
		if ( iChange > 10 )
			TriggerFastMotion();
		else
		{
			if ( m_ulMotionSinceStart == 0 )
			{
				m_flStartedMotion = UIEngine()->GetCurrentFrameTime();
				m_flLastMotion = m_flStartedMotion;
				++m_ulMotionSinceStart;
				if ( !m_bMotionLoopRunning )
				{
					m_bMotionLoopRunning = true;
					DispatchEventAsync( 0.1f, GridMotionTimeout(), this );
				}
			}
			else if ( m_flLastMotion != UIEngine()->GetCurrentFrameTime() )
			{
				++m_ulMotionSinceStart;
				m_flLastMotion = UIEngine()->GetCurrentFrameTime();
			}
		}
	}

	int iFirstRow = m_nScrollOffset;
	int iLastRow = m_nScrollOffset + nPageLength;

	if ( BHasClass( "Flick" ) )
	{
		// Need to ready all the panels between the current window and the target.
		int nScrollOffset;
		float flDragScrollOffset;

		PixelOffsetToScrollAndDragOffsets( GetCurrentScrollPosition(), nScrollOffset, flDragScrollOffset );

		iFirstRow = MIN( m_nScrollOffset, nScrollOffset );
		iLastRow = MAX( m_nScrollOffset + nPageLength, nScrollOffset + nPageLength );
	}

	int iExtraLeft = (iSelectPos - m_nScrollOffset) < 2 ? nPageLength * 2 : nPageLength/2;
	int iExtraRight = (nPageLength - (iSelectPos - m_nScrollOffset)) < 2 ? nPageLength * 2 : nPageLength / 2;

	int iFirstPanel = MAX( (iFirstRow * nPageWidth) - (nPageWidth * iExtraLeft), 0 );
	int iLastPanel = MIN( ( (iLastRow + 1) * nPageWidth) + (nPageWidth * iExtraRight), GetVisibleChildCount() );

	CUtlVector< CPanelPtr< CPanel2D > > vecPanelsReady;
	for ( int i = iFirstPanel; i < iLastPanel; ++i )
	{
		CPanel2D *pPanel = GetVisibleChild( i );
		int iVec = m_vecPanelsReadyForDisplay.Find( pPanel );
		if ( iVec == m_vecPanelsReadyForDisplay.InvalidIndex() )
		{
			DispatchEvent( ReadyPanelForDisplay(), pPanel );
			vecPanelsReady.AddToTail( pPanel );
		}
		else
		{
			vecPanelsReady.AddToTail( m_vecPanelsReadyForDisplay[iVec].Get() );
			m_vecPanelsReadyForDisplay.Remove( iVec );
		}
	}

	FOR_EACH_VEC( m_vecPanelsReadyForDisplay, i )
	{
		if ( m_vecPanelsReadyForDisplay[i].Get() )
		{
			DispatchEvent( PanelDoneWithDisplay(), m_vecPanelsReadyForDisplay[i].Get() );
		}
	}
	m_vecPanelsReadyForDisplay.Swap( vecPanelsReady );
	
	if( bChanged )
	{
		if ( m_bVecVisibleDirty )
		{
			UpdateVecVisible();
		}
		m_bComputingPositions = true;

		float flPosWidth = m_flChildWidth * m_flScaleOffset;
		float flPosHeight = m_flChildHeight * m_flScaleOffset;
		int nChildren = GetVisibleChildCount();
		for( int i=0; i < nChildren; ++i )
		{
			CPanel2D *pChild = GetVisibleChild( i );

			CUILength posX, posY, posZ;
			pChild->GetPosition( posX, posY, posZ );
			
#if DEBUG
			if ( i == 0 )
			{
				DEBUG_DRAG_SCROLL( "Got position of %s as %f, %f\n", pChild->GetID(), posX.GetValue(), posY.GetValue() );
			}
#endif

			if ( m_eScrollDirection == eScrollDirectionHorizontal )
				posX.Set( -1.0f*flPosWidth*m_nScrollOffset - m_flDragScrollOffset, CUILength::k_EUILengthLength );
			else
				posY.Set( -1.0f*flPosHeight*m_nScrollOffset - m_flDragScrollOffset, CUILength::k_EUILengthLength );

			posX.ScaleLengthValue( 1.0f / pChild->GetActualUIScaleX() );
			posY.ScaleLengthValue( 1.0f / pChild->GetActualUIScaleY() );

#if DEBUG
			if ( i == 0 )
			{
				DEBUG_DRAG_SCROLL( "Setting position of %s to %f, %f%s\n", pChild->GetID(), posX.GetValue(), posY.GetValue(), m_bNextPositionImmediate ? " - IMMEDIATE" : "" );
			}
#endif
			if ( m_bNextPositionImmediate )
			{
				// When stopping a scroll, slam the child positions without a transition to avoid
				// a bounce effect.
				pChild->SetPositionWithoutTransition( posX, posY, posZ );
			}
			else
			{
				pChild->SetPosition( posX, posY, posZ );
			}
		}
		m_bComputingPositions = false;
	}
	m_bNextPositionImmediate = false;

	SetHasClass( "HideScrollUp", !BCanCustomScrollUp() );
	SetHasClass( "HideScrollDown", !BCanCustomScrollDown() );
	SetHasClass( "HideScrollLeft", !BCanCustomScrollLeft() );
	SetHasClass( "HideScrollRight", !BCanCustomScrollRight() );
}


//-----------------------------------------------------------------------------
// Purpose: This panel or a child just lost input focus
//-----------------------------------------------------------------------------
bool CGrid::EventInputFocusLost( const CPanelPtr< IUIPanel > &ptrPanel )
{
	// we need to invalidate position when gaining or losing focus so our offsetting works
	if ( (!BHasKeyFocus() && !BHasDescendantKeyFocus()) && m_bHadFocus )
	{
		InvalidatePosition();
		UnregisterForCursorChanges();

		m_bHadFocus = false;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Track left mouse down so we don't suddenly move children when
//			you start clicking on them.
//-----------------------------------------------------------------------------
bool CGrid::OnMouseButtonDown( const MouseData_t &code )
{
	if ( code.m_MouseCode == MOUSE_LEFT )
	{
		m_bMouseDown = true;
	}

	return BaseClass::OnMouseButtonDown( code );
}


//-----------------------------------------------------------------------------
// Purpose: Track left mouse down so we don't suddenly move children when
//			you start clicking on them.  When you release, update child
//			positions so the newly activated child is fully in view.
//-----------------------------------------------------------------------------
bool CGrid::OnMouseButtonUp( const MouseData_t &code )
{
	if ( code.m_MouseCode == MOUSE_LEFT )
	{
		if ( m_bMouseDown )
		{
			m_bMouseDown = false;
			UpdateChildPositions();
		}
	}
	return BaseClass::OnMouseButtonUp( code );
}


//-----------------------------------------------------------------------------
// Purpose: handle mouse wheel
//-----------------------------------------------------------------------------
bool CGrid::OnMouseWheel( const MouseData_t &code )
{
	float flDelay = 0.0f;
	int iCount = abs( code.m_Delta );

	// If control is pressed we modify to page right/left
	if ( IsControlPressed( code.m_Modifiers ) )
	{
		int nChildren = GetVisibleChildCount();
		int iFocused = GetFocusedChildVisibleIndex();

		if ( code.m_Delta >= 0 )
		{
			if ( nChildren > 0 )
			{
				UISoundSystem()->FadeOutAndStopSoundSample( UISoundSystem()->PlaySound( "focus_change_fastscroll", UIPanel(), k_ESoundType_Effects, 1.0f, 0.2f, 0.0f ), 0.4f );
				TriggerFastMotion();

				GetVisibleChild( MAX( 0, iFocused - (m_nVerticalCount * m_nHorizontalCount) ) )->UpdateFocusInContext();
				return true;
			}
		}
		else 
		{
			if ( nChildren > 0 )
			{
				UISoundSystem()->FadeOutAndStopSoundSample( UISoundSystem()->PlaySound( "focus_change_fastscroll", UIPanel(), k_ESoundType_Effects, 1.0f, 0.2f, 0.0f ), 0.4f );
				TriggerFastMotion();

				GetVisibleChild( MIN( nChildren - 1, iFocused + (m_nVerticalCount * m_nHorizontalCount) ) )->UpdateFocusInContext();
				return true;
			}
		}
	}

	CPanel2D *pScrollTarget = m_pFocusedChild.Get();
	if ( !pScrollTarget )
	{
		pScrollTarget = this;
	}

	// Only go a single item per scroll event
	iCount = 1;
	for( int i=0; i < iCount; ++i )
	{
		if ( code.m_Delta < 0 )
		{
			if ( m_eScrollDirection == eScrollDirectionHorizontal )
				DispatchEventAsync( flDelay, panorama::MoveRight(), pScrollTarget, code.m_RepeatCount );
			else
				DispatchEventAsync( flDelay, panorama::MoveDown(), pScrollTarget, code.m_RepeatCount );
		}
		else
		{
			if ( m_eScrollDirection == eScrollDirectionHorizontal )
				DispatchEventAsync( flDelay, panorama::MoveLeft(), pScrollTarget, code.m_RepeatCount );
			else
				DispatchEventAsync( flDelay, panorama::MoveUp(), pScrollTarget, code.m_RepeatCount );
		}

		if ( i < 2 )
			flDelay += 0.1f;
	}
	
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Handles request to scroll from mouse region
//-----------------------------------------------------------------------------
bool CGrid::OnMouseScroll( const CPanelPtr< IUIPanel > &ptrPanel, int cRepeat )
{
	CPanel2D *pPanel = ToPanel2D( ptrPanel.Get() );
	if ( pPanel == m_pLeftMouseScrollRegion )
	{
		if ( m_eScrollDirection == eScrollDirectionHorizontal )
			DispatchEvent( MoveLeft(), m_pFocusedChild.Get(), cRepeat );
		else
			DispatchEvent( MoveUp(), m_pFocusedChild.Get(), cRepeat );

		return true;
	}
	else if ( pPanel == m_pRightMouseScrollRegion )
	{
		if ( m_eScrollDirection == eScrollDirectionHorizontal )
			DispatchEvent( MoveRight(), m_pFocusedChild.Get(), cRepeat );
		else
			DispatchEvent( MoveDown(), m_pFocusedChild.Get(), cRepeat );
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Format custom properties for display in debugger
//-----------------------------------------------------------------------------
void CGrid::GetDebugPropertyInfo( CUtlVector< DebugPropertyOutput_t *> *pvecProperties )
{
	static CPanoramaSymbol symScrollDir( "scrolldirection" );
	static CPanoramaSymbol symVerticalCount( "verticalcount" );
	static CPanoramaSymbol symHorizontalCount( "horizontalcount" );
	static CPanoramaSymbol symScaleOffset( "scaleoffset" );
	static CPanoramaSymbol symMovementWrap( "movementwrap" );
	static CPanoramaSymbol symFocusMargin( "focusmargin" );

	BaseClass::GetDebugPropertyInfo( pvecProperties );

	pvecProperties->AddToTail( new DebugPropertyOutput_t( symScrollDir.String(), m_eScrollDirection == eScrollDirectionVertical ? "vertical" : "horizontal" ) );
	pvecProperties->AddToTail( new DebugPropertyOutput_t( symVerticalCount.String(), CNumStr( m_nVerticalCount ) ) );
	pvecProperties->AddToTail( new DebugPropertyOutput_t( symHorizontalCount.String(), CNumStr( m_nHorizontalCount ) ) );

	if ( m_flScaleOffset != 1.0f )
	{
		pvecProperties->AddToTail( new DebugPropertyOutput_t( symScaleOffset.String(), CNumStr( m_flScaleOffset ) ) );
	}

	if ( m_nOverrideFocusMargin )
	{
		pvecProperties->AddToTail( new DebugPropertyOutput_t( symFocusMargin.String(), CNumStr( m_nOverrideFocusMargin ) ) );
	}

	if ( m_eMovementWrapDirection != eMoveNone )
	{
		const char *pchVal = "none";
		switch ( m_eMovementWrapDirection )
		{
		case eMoveLeft:
			pchVal = "left";
			break;
		case eMoveRight:
			pchVal = "right";
			break;
		case eMoveUp:
			pchVal = "up";
			break;
		case eMoveDown:
			pchVal = "down";
			break;
		}
		pvecProperties->AddToTail( new DebugPropertyOutput_t( symMovementWrap.String(), pchVal ) );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Given the scroll offset (which row is at the top of the panel) and the
//			drag scroll offset (how many pixels has the user moved the panel with
//			drag scroll), return how many pixels from the top of the panel the
//			display has actually scrolled.
//-----------------------------------------------------------------------------
float CGrid::ScrollAndDragOffsetsToPixelOffset( int nScrollOffset, float flDragScrollOffset ) const
{
	float flRowLength = ( m_eScrollDirection == eScrollDirectionHorizontal ) ? m_flChildWidth * m_flScaleOffset : m_flChildHeight * m_flScaleOffset;
	float flReturn = nScrollOffset * flRowLength + flDragScrollOffset;

#if DEBUG
	if ( flRowLength != 0 )
	{
		int nTest;
		float flTest;
		PixelOffsetToScrollAndDragOffsets( flReturn, nTest, flTest );
		Assert( nTest == nScrollOffset );
		Assert( fabs( flTest - flDragScrollOffset ) < 0.01 );
	}
#endif

	return flReturn;
}


//-----------------------------------------------------------------------------
// Purpose: Given a pixel offset from the top of the panel, return the
//			corresponding scroll offset and drag scroll offset.
//-----------------------------------------------------------------------------
void CGrid::PixelOffsetToScrollAndDragOffsets( float flPixelOffset, int &nScrollOffset, float &flDragScrollOffset ) const
{
	float flRowLength;
	int nWidth, nHeight;

	if ( m_eScrollDirection == eScrollDirectionHorizontal )
	{
		flRowLength =  m_flChildWidth * m_flScaleOffset;
		nWidth = m_nVerticalCount;
		nHeight = m_nHorizontalCount;
	}
	else
	{
		flRowLength = m_flChildHeight * m_flScaleOffset;
		nWidth = m_nHorizontalCount;
		nHeight = m_nVerticalCount;
	}

	int nMaxRows = ( ( GetVisibleChildCount() - 1 ) / nWidth ) + 1;
	int nTopRow = MAX( 0, nMaxRows - nHeight + m_nOverrideFocusMargin );
	float flMaxTop = nTopRow * flRowLength;

	float flTargetOffset = clamp( flPixelOffset, 0, flMaxTop );

	nScrollOffset = ( flTargetOffset / flRowLength );
	flDragScrollOffset = flTargetOffset - ( nScrollOffset * flRowLength );

	// Fudging to deal with floating point precision issues
	if ( flDragScrollOffset < 0.01 )
	{
		flDragScrollOffset = 0;
	}
	else if ( flDragScrollOffset > flRowLength - 0.01 )
	{
		flDragScrollOffset = 0;
		nScrollOffset = nScrollOffset + 1;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Return the current scroll position of the panel in pixels, even
//			if it's currently moving.
//			BUGBUG: This currently has a poor interaction with the animation
//			thread resulting in it returning a position slightly behind
//			what the user may actually be seeing.
//-----------------------------------------------------------------------------
float CGrid::GetCurrentScrollPosition()
{
	// Can pick an arbitrary visible child and see if its position is in transition.
	// The delta on this child should be the same as the delta on the panel itself
	//
	CPanel2D *pChild = GetVisibleChild(0);
	if ( pChild == NULL )
	{
		return 0.0f;
	}

	CUILength xPos, yPos, zPos;
	CUILength xCurrent, yCurrent, zCurrent;
	CUILength xFinal, yFinal, zFinal;
	pChild->GetPosition( xPos, yPos, zPos, true );
	pChild->AccessStyleDirty()->GetInterpolatedPosition( xFinal, yFinal, zFinal, true, true );
	pChild->AccessStyleDirty()->GetInterpolatedPosition( xCurrent, yCurrent, zCurrent, false, true );

	float flDelta = ( m_eScrollDirection == eScrollDirectionHorizontal ) ? xCurrent.GetValue() - xFinal.GetValue() : yCurrent.GetValue() - yFinal.GetValue();
	float flPosition = GetFinalScrollPosition() - flDelta;
	
	DEBUG_DRAG_SCROLL( "%s => yPos: %f, yCurrent: %f, yFinal: %f, returning %f, time %f\n", pChild->GetID(), yPos.GetValue(), yCurrent.GetValue(), yFinal.GetValue(), flPosition, UIEngine()->GetCurrentFrameTime() );
	return flPosition;
}


//-----------------------------------------------------------------------------
// Purpose: Return the position in pixels of the target of the current
//			scroll.
//-----------------------------------------------------------------------------
float CGrid::GetFinalScrollPosition() const
{
	return ScrollAndDragOffsetsToPixelOffset( m_nScrollOffset, m_flDragScrollOffset );
}


//-----------------------------------------------------------------------------
// Purpose: Does the panel implement drag scrolling behaviors?
//-----------------------------------------------------------------------------
bool CGrid::BCustomCanDragScroll() const
{
	return BCanCustomScrollUp() || BCanCustomScrollDown() || BCanCustomScrollLeft() || BCanCustomScrollRight();
}


//-----------------------------------------------------------------------------
// Purpose: Is a scroll operation currently in progress?
//-----------------------------------------------------------------------------
bool CGrid::BCustomScrollInProgress()
{
	DEBUG_DRAG_SCROLL( "BCustomScrollInProgress: " );
	if ( GetCurrentScrollPosition() != GetFinalScrollPosition() )
	{
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Begin a drag scrolling event
//-----------------------------------------------------------------------------
bool CGrid::EventDragScrollStart( const CPanelPtr< IUIPanel > &pPanel )
{
	DEBUG_DRAG_SCROLL( "Grid DragScrollStart\n" );
	DEBUG_DRAG_SCROLL( "*** START: scroll offset = %i, drag scroll offset = %f\n", m_nScrollOffset, m_flDragScrollOffset );

	DEBUG_DRAG_SCROLL( "DragScollStart: " ); GetCurrentScrollPosition();
	DEBUG_DRAG_SCROLL( "DragScrollStart after EndFlick: " ); GetCurrentScrollPosition();
	DEBUG_DRAG_SCROLL( "*** End\n" );
	m_bMouseDown = false; // We're tracking this as a drag now, don't need to track it as a click
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Move the children in the panel by nOffset pixels.
//-----------------------------------------------------------------------------
void CGrid::DoMouseDrag( float flStart, float flOffset )
{
	DEBUG_DRAG_SCROLL( "DoMouseDrag( %f, %f )\n", flStart, flOffset );
	float flCurrentOffset = flStart; 
	float flDesiredOffset = flCurrentOffset - flOffset;

	PixelOffsetToScrollAndDragOffsets( flDesiredOffset, m_nScrollOffset, m_flDragScrollOffset );

	m_ulMotionSinceStart = 0;
	m_bForceRelayout = true;

	InvalidatePosition();
}

//-----------------------------------------------------------------------------
// Purpose: Handle a drag scroll move event
//-----------------------------------------------------------------------------
bool CGrid::EventDragScrollMouseMove( const CPanelPtr< IUIPanel > &pPanel, int nLastX, int nLastY, int nX, int nY )
{
	int nLast, coord;
	if ( m_eScrollDirection == eScrollDirectionHorizontal )
	{
		nLast = nLastX;
		coord = nX;
	}
	else
	{
		nLast = nLastY;
		coord = nY;
	}

	if ( !BHasClass( "DragScrolling" ) )
	{
		AddClass( "DragScrolling" );
		EndFlick();
	}

	DEBUG_DRAG_SCROLL( "MouseMove: " );
	float flCurrentOffset = GetCurrentScrollPosition();
	DoMouseDrag( flCurrentOffset, coord - nLast );
	MoveFocusIntoView();

	return true;
}

extern ConVar g_ConVarDragScrollMinFlickVelocity;
extern ConVar g_ConVarDragScrollMaxFlickVelocity;

extern ConVar g_ConVarDragScrollMinFlickVelocityVR;
extern ConVar g_ConVarDragScrollMaxFlickVelocityVR;

//-----------------------------------------------------------------------------
// Purpose: Handle a drag scroll end event.  Start a flick movement if
//			appropriate.
//-----------------------------------------------------------------------------
bool CGrid::EventDragScrollEnd( const CPanelPtr< IUIPanel > &pPanel, int nLastX, int nLastY, float flVelocityX, float flVelocityY )
{
	DEBUG_DRAG_SCROLL( "Grid DragScrollEnd( %i, %i, %f, %f )\n", nLastX, nLastY, flVelocityX, flVelocityY );
	RemoveClass( "DragScrolling" );

	float flFlickThreshold = GetParentWindow()->BIsVROverlay() ? g_ConVarDragScrollMinFlickVelocityVR.GetFloat() : g_ConVarDragScrollMinFlickVelocity.GetFloat();
	float flVelocity = m_eScrollDirection == eScrollDirectionHorizontal ? flVelocityX : flVelocityY;
	float flMaxVelocity = GetParentWindow()->BIsVROverlay() ? g_ConVarDragScrollMaxFlickVelocityVR.GetFloat() : g_ConVarDragScrollMaxFlickVelocity.GetFloat();

	flVelocity = clamp( flVelocity, -flMaxVelocity, flMaxVelocity );

	if ( flVelocity > flFlickThreshold || flVelocity < -flFlickThreshold )
	{
		float flFlickPeriod = GetLayoutFileDefineFloat( "DragScrollFlickTime", 1.0 );

		StartFlick();

		DEBUG_DRAG_SCROLL( "DragScrollEnd: " );
		float flInterpolatedPosition = GetCurrentScrollPosition();
		DEBUG_DRAG_SCROLL( "Flick from %f, delta %f\n", flInterpolatedPosition, flVelocity * flFlickPeriod );

		//
		// Is the current focused child going to leave the screen?  If so, then we'll move
		// by setting the focused child to whatever our target topmost child is.
		//
		// If the current focused child isn't leaving the screen, then don't change the focus
		//
		int nWidth = ( m_eScrollDirection == eScrollDirectionHorizontal ? m_nVerticalCount : m_nHorizontalCount );
		int nHeight = ( m_eScrollDirection == eScrollDirectionHorizontal ? m_nHorizontalCount : m_nVerticalCount );
		int nNewOffset;
		float flNewDragScrollOffset;
		float flDelta = flVelocity * flFlickPeriod;
		PixelOffsetToScrollAndDragOffsets( flInterpolatedPosition - flDelta, nNewOffset, flNewDragScrollOffset );

		int iFocusedChild = GetFocusedChildVisibleIndex();
		int nCurrentRow = iFocusedChild / nWidth;
		bool bSnapToGrid = false;
		if ( nCurrentRow < nNewOffset + m_nOverrideFocusMargin || nCurrentRow > nNewOffset + nHeight - m_nOverrideFocusMargin )
		{
			flDelta = flInterpolatedPosition - ScrollAndDragOffsetsToPixelOffset( nNewOffset, 0 );
			bSnapToGrid = true;
		}

		m_flFlickFastMotionEnd = UIEngine()->GetCurrentFrameTime() + ( flFlickPeriod * 0.6 );

		DEBUG_DRAG_SCROLL( "Mouse drag to %f\n", flInterpolatedPosition - flDelta );
		DoMouseDrag( flInterpolatedPosition, flDelta );

		if ( bSnapToGrid )
		{
			Assert( m_flDragScrollOffset == 0 );
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Start a flick movement
//-----------------------------------------------------------------------------
void CGrid::StartFlick()
{
	AddClass( "Flick" );
	ApplyStyles( true );

	DispatchEventAsync( 0.1f, GridFlickTimeout(), this );
}


//-----------------------------------------------------------------------------
// Purpose: Abort a flick scroll in progress, freezing child positions at whatever
//			the current scroll point is.
//-----------------------------------------------------------------------------
void CGrid::StopScroll()
{
	DEBUG_DRAG_SCROLL( "In StopScroll()\n" );
	if ( GetVisibleChildCount() == 0 )
	{
		return;
	}

	for ( int i = 0; i < GetVisibleChildCount(); i++)
	{
		CPanel2D *pChild = GetVisibleChild(i);
		pChild->AccessStyleDirty()->CompletePropertyTransitionNow( CStylePropertyPosition::symbol, true );
	}
	
	CUILength posX, posY, posZ;
	GetVisibleChild(0)->GetPosition( posX, posY, posZ );

	float flPos = ( m_eScrollDirection == eScrollDirectionHorizontal ) ? posX.GetValue() : posY.GetValue();

	DEBUG_DRAG_SCROLL( "Child 0 pos %f\n", flPos );
	PixelOffsetToScrollAndDragOffsets( -flPos, m_nScrollOffset, m_flDragScrollOffset );
	DEBUG_DRAG_SCROLL( "Ending offsets %i, %f\n", m_nScrollOffset, m_flDragScrollOffset );
	DEBUG_DRAG_SCROLL( "Current position is %f\n", GetCurrentScrollPosition() );
	m_bForceRelayout = true;
	m_bFocusDueToDragScroll = false;
	m_bNextPositionImmediate = true;
	InvalidatePosition();

	DEBUG_DRAG_SCROLL( "Out StopScroll()\n" );
}


//-----------------------------------------------------------------------------
// Purpose: End a flick movement
//-----------------------------------------------------------------------------
void CGrid::EndFlick()
{
	if ( BHasClass( "Flick" ) )
	{
		RemoveClass( "Flick" );
		ApplyStyles( true );
		StopScroll();
	}
}


#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: validate mem
//-----------------------------------------------------------------------------
void CGrid::ValidateClientPanel( CValidator &validator, const char *pchName )
{
	BaseClass::ValidateClientPanel( validator, pchName );
	VALIDATE_SCOPE();

	ValidateObj( m_vecPanelsReadyForDisplay );
	ValidateObj( m_vecVisibleChildren );
}
#endif


//-----------------------------------------------------------------------------
// Purpose: Setting the count is necessary when the grid gets reused, but has
// a different count in the embedding environments. If it is called before
// the first LayoutTraverse happened, e.g. in the constructor of the embedding
// panel nothing else is needed. Otherwise a layout pass needs to be forced.
//-----------------------------------------------------------------------------
void CGrid::SetHorizontalAndVerticalCount( int nHorizontalCount, int nVerticalCount )
{
	m_nHorizontalCount = nHorizontalCount;
	m_nVerticalCount = nVerticalCount;

	InvalidateSizeAndPosition();
}
