//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx_client.h"
#include "panorama/controls/panel2d.h"
#include "panorama/controls/scrollbar.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

REGISTER_PANEL2D( CScrollBar, ScrollBar );
REGISTER_PANEL2D( CBaseScrollBar, BaseScrollBar );
REGISTER_PANEL2D( CHorizontalScrollBar, HorizontalScrollBar );
REGISTER_PANEL2D( CVerticalScrollBar, VerticalScrollBar );

ConVar g_ConVarDragScrollMinFlickVelocity( "@panorama_dragscroll_minflickvelocity", "60", 0, "Minimum velocity that the mouse must be moving as mouse up time to qualify as a drag scroll flick" );
ConVar g_ConVarDragScrollMaxFlickVelocity( "@panorama_dragscroll_maxflickvelocity", "8000", 0, "Maximum velocity for a drag scroll flick" );

ConVar g_ConVarDragScrollMinFlickVelocityVR( "@panorama_dragscroll_minflickvelocity_vr", "240", 0, "Minimum velocity that the mouse must be moving as mouse up time to qualify as a drag scroll flick in VR" );
ConVar g_ConVarDragScrollMaxFlickVelocityVR( "@panorama_dragscroll_maxflickvelocity_vr", "8000", 0, "Maximum velocity for a drag scroll flick in VR" );

namespace panorama
{

DECLARE_PANEL_EVENT0( ScrollFlickTimeout );
DEFINE_PANORAMA_EVENT( ScrollFlickTimeout );

//-----------------------------------------------------------------------------
// Purpose: Scrollbar constructor
//-----------------------------------------------------------------------------
CBaseScrollBar::CBaseScrollBar( CPanel2D *parent, const char * pchPanelID )
	: CPanel2D( parent, pchPanelID, ePanelFlags_DontAddAsChild )
{
	m_flLastScrollTime = 0.0f;
	m_flRangeMin = 0.0f;
	m_flRangeMax = 0.0f;
	m_flWindowSize = 0.0f;
	m_flWindowStart = 0.0f;
	m_bLastMoveImmediate = false;

	m_eTimingFunction = k_EAnimationNone;
	m_flTransitionTime = 0.0;
	m_flDragScrollFlickTime = 0.0;

	if ( !UIEngine()->BHaveEventHandlersRegisteredForType( CBaseScrollBar::GetPanelSymbol() ) )
	{
		RegisterEventHandlerOnPanelType( ScrollFlickTimeout(), &CBaseScrollBar::EventScrollFlickTimeout );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CBaseScrollBar::~CBaseScrollBar()
{
}


//-----------------------------------------------------------------------------
// Purpose: Set the scroll position for the content in the parent. This also
//			will end up repositioning the scrollbar itself.
//-----------------------------------------------------------------------------
void CBaseScrollBar::SetScrollWindowPosition( float flWindowPos, bool bImmediateMove, bool bEndFlick )
{
	if ( bEndFlick )
	{
		EndFlick();
	}

	if ( m_flWindowStart == flWindowPos )
	{
		return;
	}

	m_flWindowStart = flWindowPos;
	m_flLastScrollTime = UIEngine()->GetCurrentFrameTime();

	if ( GetParent() )
	{
		// The only logic we want to skip if our parent's position or child
		// position is invalid is the part of Normalize that does the
		// UpdateLayout call. We still need it to apply the clamping logic,
		// otherwise we can scroll past the beginning or end of a scrolling
		// region.
		bool bUpdateLayout = ( GetParent()->IsChildPositionValid() && GetParent()->IsPositionValid() );
		Normalize( bImmediateMove, bUpdateLayout );
		if ( !bUpdateLayout )
		{
			m_bLastMoveImmediate = bImmediateMove;
		}

		GetParent()->InvalidatePosition();
		GetParent()->OnScrollPositionChanged();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Load transition timing function and duration from panel styles
//-----------------------------------------------------------------------------
void  CBaseScrollBar::LoadTransitionData()
{
	TransitionProperty_t *prop;
	prop = AccessStyle()->FindTransitionData( CStylePropertyTransform3D::symbol );
	if ( prop != NULL )
	{
		m_eTimingFunction = prop->m_eTimingFunction;
		m_flTransitionTime = prop->m_flTransitionSeconds;

		if ( m_eTimingFunction == k_EAnimationCustomBezier )
		{
			for (int i = 0; i < 4; i++ )
			{
				m_vecControlPoints[i].x = prop->m_CubicBezier.ControlPoint(i).x;
				m_vecControlPoints[i].y = prop->m_CubicBezier.ControlPoint(i).y;
			}
		}
	}
	else
	{
		// Transition should have been set, but it wasn't.  Rather than crash
		// or do something similarly awful, figure out which scrollbar is wrong,
		// fire an assert (so we can find it in minidumps) and then use a default.
		// The scrollbar will still look wonky.

		// Find the first parent with a name for the assert
		CPanel2D *pParent = GetParent();
		uint nParent = 1;
		while ( pParent && pParent->GetID() == NULL )
		{
			pParent = pParent->GetParent();
			nParent++;
		}
		const char *pchParentID = pParent ? pParent->GetID() : "No ID Found";
		AssertMsg2( false, "Scrollbar requires a transform transition in its styles, parent %u ID %s", nParent, pchParentID );

		m_eTimingFunction = k_EAnimationEaseOut;
		m_flTransitionTime = 0.2;
	}

	Assert( m_eTimingFunction != k_EAnimationNone );
	Assert( m_flTransitionTime != 0.0 );

	m_flDragScrollFlickTime = GetLayoutFileDefineFloat( "DragScrollFlickTime", 1.0 );
}


//-----------------------------------------------------------------------------
// Purpose: Set new transition timing function and duration on the
//			scrollbar
//-----------------------------------------------------------------------------
void CBaseScrollBar::StartFlick()
{
	AddClass( "Flick" );

	// Need to explicitly apply styles here or the scroll thumb will, in some cases,
	// keep the previous transition time and move at the wrong speed.
	ApplyStyles( true );

	DispatchEventAsync( 0.1f, ScrollFlickTimeout(), this );
}


//-----------------------------------------------------------------------------
// Purpose: Revert back to the transition timings in the CSS
//-----------------------------------------------------------------------------
void CBaseScrollBar::EndFlick()
{
	if ( BHasClass( "Flick" ) )
	{
		RemoveClass( "Flick" );

		// Need to explicitly apply styles here or the scroll thumb will, in some cases,
		// keep the previous transition time and move at the wrong speed.
		ApplyStyles( true );

		StopScroll();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Update the flick state periodically
//-----------------------------------------------------------------------------
bool CBaseScrollBar::EventScrollFlickTimeout( const CPanelPtr< IUIPanel > &ptrPanel )
{
	if ( BHasClass( "Flick" ) && GetParent()->UIPanel()->BScrollInProgress() )
	{
		DispatchEventAsync( 0.1f, ScrollFlickTimeout(), this );
	}
	else
	{
		EndFlick();
	}
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Event handler on start of drag scroll
//-----------------------------------------------------------------------------
bool CBaseScrollBar::OnDragScrollStart()
{
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Event handler when mouse moves during drag scroll.  Force the
//			scroll position to the new location without a transition.
//-----------------------------------------------------------------------------
bool CBaseScrollBar::OnDragScrollMouseMove( int nLast, int nCurrent )
{
	if ( !BHasClass( "DragScrolling" ) )
	{
		AddClass( "DragScrolling" );
		EndFlick();
	}

	int nInterpolatedPosition = GetInterpolatedScrollWindowPosition();
	int nUpperBound = GetRangeMax() - GetScrollWindowSize();
	int nDestination = nInterpolatedPosition + nLast - nCurrent;
	if ( nDestination >= 0 && nDestination < nUpperBound )
	{
		SetScrollWindowPosition( nDestination, true );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Event handler when mouse is released during drag scroll.  If the
//			mouse is moving with sufficient velocity, this should cause a
//			flick transition to the next position.
//-----------------------------------------------------------------------------
bool CBaseScrollBar::OnDragScrollEnd( int nLast, float flVelocity )
{
	int nInterpolatedPosition = GetInterpolatedScrollWindowPosition();
	int nUpperBound = GetRangeMax() - GetScrollWindowSize();

	RemoveClass( "DragScrolling" );

	float flFlickThreshold = GetParentWindow()->BIsVROverlay() ? g_ConVarDragScrollMinFlickVelocityVR.GetFloat() : g_ConVarDragScrollMinFlickVelocity.GetFloat();
	float flFlickPeriod = m_flDragScrollFlickTime;
	float flMaxVelocity = GetParentWindow()->BIsVROverlay() ? g_ConVarDragScrollMaxFlickVelocityVR.GetFloat() : g_ConVarDragScrollMaxFlickVelocity.GetFloat();

	flVelocity = clamp( flVelocity, -flMaxVelocity, flMaxVelocity );

	if ( flVelocity > flFlickThreshold || flVelocity < -flFlickThreshold )
	{
		float flScrollPosition = nInterpolatedPosition;
		StartFlick();

		float flNewPosition = clamp( flScrollPosition - ( flVelocity * flFlickPeriod ), 0, nUpperBound );
		SetScrollWindowPosition( flNewPosition, false, false );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Scrollbar constructor
//-----------------------------------------------------------------------------
CScrollBar::CScrollBar( CPanel2D *parent, const char * pchPanelID )
	: CBaseScrollBar( parent, pchPanelID )
{
	m_pScrollThumb = new CPanel2D( this, "ScrollThumb" );
	m_pScrollThumb->AddClass( "ScrollThumb" );

	m_bMouseDown = false;
	m_flMouseCoord = 0.0f;
	m_flMouseDownCoord = 0.0f;
	m_flScrollStartPosition = 0.0f;

	SetMouseTracking( true );
	SetAcceptsInput( true );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CScrollBar::~CScrollBar()
{
}


//-----------------------------------------------------------------------------
// Purpose: Scrollbar layout traverse to layout thumb panel correctly
//-----------------------------------------------------------------------------
void CVerticalScrollBar::UpdateLayout( bool bImmediateMove )
{
	CUILength zero;
	zero.SetLength( 0.0f );

	if ( GetRangeSize() < 0.001f )
		return;

	CUILength length;

	float flXPosPercent = ( GetScrollWindowPosition() - GetRangeMin() )/ GetRangeSize();
	length.SetPercent( flXPosPercent * 100.0f );
	if ( bImmediateMove )
		m_pScrollThumb->SetPositionWithoutTransition( zero, length, zero );
	else
		m_pScrollThumb->SetPosition( zero, length, zero );

	float flWidthPercent = GetScrollWindowSize() / GetRangeSize();

	length.SetPercent( flWidthPercent*100.0f );
	m_pScrollThumb->AccessStyleDirty()->SetHeight( length );
	length.SetPercent( 100.0f );
	m_pScrollThumb->AccessStyleDirty()->SetWidth( length );

	m_bLastMoveImmediate = bImmediateMove;
}


//-----------------------------------------------------------------------------
// Purpose: Scrollbar layout traverse to layout thumb panel correctly
//-----------------------------------------------------------------------------
void CHorizontalScrollBar::UpdateLayout( bool bImmediateMove )
{
	CUILength zero;
	zero.SetLength( 0.0f );

	if ( GetRangeSize() < 0.001f )
		return;

	CUILength length;

	float flXPosPercent = ( GetScrollWindowPosition() - GetRangeMin() )/ GetRangeSize();
	length.SetPercent( flXPosPercent * 100.0f );
	if ( bImmediateMove )
		m_pScrollThumb->SetPositionWithoutTransition( length, zero, zero );
	else
		m_pScrollThumb->SetPosition( length, zero, zero );

	float flWidthPercent = GetScrollWindowSize() / GetRangeSize();

	length.SetPercent( flWidthPercent*100.0f );
	m_pScrollThumb->AccessStyleDirty()->SetWidth( length );
	length.SetPercent( 100.0f );
	m_pScrollThumb->AccessStyleDirty()->SetHeight( length );

	m_bLastMoveImmediate = bImmediateMove;
}


//-----------------------------------------------------------------------------
// Purpose: Stop a scroll in progress
//-----------------------------------------------------------------------------
void CVerticalScrollBar::StopScroll()
{
	float flCurrent = GetParent()->UIPanel()->StopVerticalScroll();
	SetScrollWindowPosition( flCurrent, true );
}


//-----------------------------------------------------------------------------
// Purpose: Stop a scroll in progress
//-----------------------------------------------------------------------------
void CHorizontalScrollBar::StopScroll()
{
	float flCurrent = GetParent()->UIPanel()->StopHorizontalScroll();
	SetScrollWindowPosition( flCurrent, true );
}

}
