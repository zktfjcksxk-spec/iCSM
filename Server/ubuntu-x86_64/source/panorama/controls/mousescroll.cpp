//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx_client.h"
#include "panorama/controls/mousescroll.h"
#include "panorama/panoramacurves.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

REGISTER_PANEL2D_FACTORY( CMouseScrollRegion, MouseScrollRegion );
DEFINE_PANORAMA_EVENT( MouseScroll );

static const float k_flMouseScrollRegionCurveTime = 1.0f;
static const float k_flMouseScrollRegionIntervalStart = 0.22f;
static const float k_flMouseScrollRegionIntervalEnd = 0.05f;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CMouseScrollRegion::CMouseScrollRegion( CPanel2D *parent, const char * pchPanelID ) : 
	CPanel2D( parent, pchPanelID, ePanelFlags_DontAddAsChild ),
	m_scheduledScrollRepeat( MAKE_SCHEDULED_FUNC( CMouseScrollRegion::DispatchScrollEvent ) )
{
	SetAcceptsInput( true );

	m_flMouseDownTimestamp = 0.0f;
	m_cMouseDownRepeats = 0;

	// copied from k_EAnimationEaseIn
	Vector2D vecPoints[4];
	panorama::GetAnimationCurveControlPoints( panorama::k_EAnimationEaseIn, vecPoints );
	m_repeatCurve.SetControlPoints( vecPoints );

	parent->AddClass( "MouseScrollContainer" );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CMouseScrollRegion::~CMouseScrollRegion()
{
	
}


//-----------------------------------------------------------------------------
// Purpose: Handles mouse click
//-----------------------------------------------------------------------------
void CMouseScrollRegion::MouseButtonDown()
{
	if( m_scheduledScrollRepeat.BScheduled() )
		return;

	m_cMouseDownRepeats = 0;
	m_flMouseDownTimestamp = UIEngine()->GetCurrentFrameTime();
	m_scheduledScrollRepeat.Schedule( 0.0 );
}


//-----------------------------------------------------------------------------
// Purpose: Mouse button pressed
//-----------------------------------------------------------------------------
bool CMouseScrollRegion::OnMouseButtonDown( const MouseData_t &code )
{
	MouseButtonDown();
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Mouse button double clicked
//-----------------------------------------------------------------------------
bool CMouseScrollRegion::OnMouseButtonDoubleClick( const MouseData_t &code )
{
	MouseButtonDown();
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Mouse button triple clicked
//-----------------------------------------------------------------------------
bool CMouseScrollRegion::OnMouseButtonTripleClick( const MouseData_t &code )
{
	MouseButtonDown();
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Mouse button depressed
//-----------------------------------------------------------------------------
bool CMouseScrollRegion::OnMouseButtonUp( const MouseData_t &code )
{
	if( m_scheduledScrollRepeat.BScheduled() )
		m_scheduledScrollRepeat.Cancel();

	m_flMouseDownTimestamp = 0.0f;
	m_cMouseDownRepeats = 0;
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Dispatches scroll event. Called by scheduled function
//-----------------------------------------------------------------------------
void CMouseScrollRegion::DispatchScrollEvent()
{
	DispatchEvent( MouseScroll(), this, m_cMouseDownRepeats++ );

	// reschedule
	Vector2D vRes;
	m_repeatCurve.Evaluate( clamp( (UIEngine()->GetCurrentFrameTime() - m_flMouseDownTimestamp) / k_flMouseScrollRegionCurveTime, 0.0f, 1.0f ), vRes );
	float flDelay = Lerp( vRes.y, k_flMouseScrollRegionIntervalStart, k_flMouseScrollRegionIntervalEnd );

	m_scheduledScrollRepeat.Schedule( flDelay );
}
