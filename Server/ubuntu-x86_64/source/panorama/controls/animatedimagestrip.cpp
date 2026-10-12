//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx_client.h"
#include "panorama/controls/animatedimagestrip.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

REGISTER_PANEL2D_FACTORY( CAnimatedImageStrip, AnimatedImageStrip );

DECLARE_PANORAMA_EVENT0( AdvanceAnimatedImageStripFrame );
DEFINE_PANORAMA_EVENT( AdvanceAnimatedImageStripFrame );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CAnimatedImageStrip::CAnimatedImageStrip( CPanel2D *parent, const char * pchPanelID )
	: CImagePanel( parent, pchPanelID )
	, m_nDefaultFrame( 0 )
	, m_flFrameTime( 0.1f )
	, m_nCurrentFrameIndex( -1 )
	, m_nStopAtFrameIndex( -1 )
	, m_bAnimating( true )
	, m_bCurrentFramePainted( false )
{
	RegisterEventHandler( PanelLoaded(), this, &CAnimatedImageStrip::EventPanelLoaded );
	RegisterEventHandler( AdvanceAnimatedImageStripFrame(), this, &CAnimatedImageStrip::EventAdvanceFrame );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CAnimatedImageStrip::~CAnimatedImageStrip()
{
}


//-----------------------------------------------------------------------------
// Purpose: Property parsing
//-----------------------------------------------------------------------------
bool CAnimatedImageStrip::BSetProperty( CPanoramaSymbol symName, const char *pchValue )
{
	static const CPanoramaSymbol symDefaultFrame = "defaultframe";
	static const CPanoramaSymbol symFrameTime = "frametime";
	static const CPanoramaSymbol symAnimating = "animating";

	if ( symName == symDefaultFrame )
	{
		m_nDefaultFrame = V_atoi( pchValue );
		return true;
	}
	else if ( symName == symFrameTime )
	{
		double dFrameTime = 0.0;
		if ( !CSSHelpers::BParseTime( &dFrameTime, pchValue ) )
			return false;

		m_flFrameTime = ( float )dFrameTime;
		return true;
	}
	else if ( symName == symAnimating )
	{
		return CSSHelpers::BParseTrueFalse( pchValue, &m_bAnimating );
	}
	else
	{
		return BaseClass::BSetProperty( symName, pchValue );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Draw the image
//-----------------------------------------------------------------------------
void CAnimatedImageStrip::Paint()
{
	// When reloading from XML, we can have a frame when we're asked to paint before receiving the
	// EventPanelLoaded to setup the correct image slice. Just don't display anything in that case
	if ( m_nCurrentFrameIndex < 0 )
		return;

	BaseClass::Paint();

	// Move to the next frame if necessary
	if ( !m_bCurrentFramePainted )
	{
		m_bCurrentFramePainted = true;

		if ( m_bAnimating )
		{
			if ( m_nCurrentFrameIndex == m_nStopAtFrameIndex )
			{
				m_bAnimating = false;
				m_nStopAtFrameIndex = -1;
			}
			else
			{
				DispatchEventAsync( m_flFrameTime, AdvanceAnimatedImageStripFrame(), this );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Set the currently displayed frame of the image strip
//-----------------------------------------------------------------------------
void CAnimatedImageStrip::SetCurrentFrame( int nFrame )
{
	IImageSource *pImage = GetImage();
	if ( !pImage )
		return;

	int nFrameIndex = GetFrameIndex( nFrame );
	if ( nFrameIndex == m_nCurrentFrameIndex )
		return;

	int nImageWidth = pImage->GetWidth();
	int nImageHeight = pImage->GetHeight();

	if ( nImageWidth > nImageHeight )
		SetVisibleImageSlice( nFrameIndex * nImageHeight, 0, nImageHeight, nImageHeight );
	else
		SetVisibleImageSlice( 0, nFrameIndex * nImageWidth, nImageWidth, nImageWidth );

	m_nCurrentFrameIndex = nFrameIndex;
	m_bCurrentFramePainted = false;
}

//-----------------------------------------------------------------------------
// Purpose: Start animating the image strip
//-----------------------------------------------------------------------------
void CAnimatedImageStrip::StartAnimating()
{
	m_nStopAtFrameIndex = -1;
	m_bAnimating = true;
	AdvanceFrame();
}

//-----------------------------------------------------------------------------
// Purpose: Stop animating the image strip at the current frame
//-----------------------------------------------------------------------------
void CAnimatedImageStrip::StopAnimating()
{
	m_bAnimating = false;
}


//-----------------------------------------------------------------------------
// Purpose: Stop animating the image strip after it reaches a specific frame
//-----------------------------------------------------------------------------
void CAnimatedImageStrip::StopAnimatingAtFrame( int nFrame )
{
	m_nStopAtFrameIndex = GetFrameIndex( nFrame );
}


//-----------------------------------------------------------------------------
// Purpose: Move the strip to the next frame
//-----------------------------------------------------------------------------
void CAnimatedImageStrip::AdvanceFrame()
{
	int nNextFrameIndex = GetFrameIndex( m_nCurrentFrameIndex + 1 );
	if ( nNextFrameIndex == m_nCurrentFrameIndex )
		return;

	SetCurrentFrame( nNextFrameIndex );
}


//-----------------------------------------------------------------------------
// Purpose: Return the count of frames based on the image size
//-----------------------------------------------------------------------------
int CAnimatedImageStrip::GetFrameCount()
{
	IImageSource *pImage = GetImage();
	if ( !pImage )
		return 0;

	int nImageWidth = pImage->GetWidth();
	int nImageHeight = pImage->GetHeight();

	if ( nImageWidth == 0 || nImageHeight == 0 )
		return 0;

	if ( nImageWidth > nImageHeight )
		return nImageWidth / nImageHeight;

	return nImageHeight / nImageWidth;
}


//-----------------------------------------------------------------------------
// Purpose: Convert a frame number into an actual frame index.
//-----------------------------------------------------------------------------
int CAnimatedImageStrip::GetFrameIndex( int nFrame )
{
	int nFrameCount = GetFrameCount();
	if ( nFrameCount == 0 )
		return 0;

	int nFrameIndex = nFrame;

	// Negative numbers should wrap around
	if ( nFrameIndex < 0 )
		nFrameIndex += ( ( -nFrameIndex + nFrameCount - 1 ) / nFrameCount ) * nFrameCount;

	return nFrameIndex % nFrameCount;
}


//-----------------------------------------------------------------------------
// Purpose: Called when the image is loaded
//-----------------------------------------------------------------------------
bool CAnimatedImageStrip::EventPanelLoaded( const CPanelPtr< IUIPanel > &panelPtr )
{
	if ( GetFrameCount() == 0 )
		return false;

	// If we're animating, kick it off from the first frame. Otherwise, use the default frame
	int nFrame = m_bAnimating ? 0 : m_nDefaultFrame;
	SetCurrentFrame( nFrame );
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Event handler to move to the next frame
//-----------------------------------------------------------------------------
bool CAnimatedImageStrip::EventAdvanceFrame()
{
	AdvanceFrame();
	return false;
}
