//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx.h"
#include "mousecursor.h"
#include "panorama/panoramacurves.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

// Time to wait in seconds before beginning to fade out cursor
const float k_flMouseStartFadeTimeoutInactivity = 2.0f; 

// Time in seconds to wait before beginning to fade out cursor if it goes out of window
const float k_flMouseStartFadeTimeoutOutofWindow = 0.15f; 

// Time to transition for fade in/out in seconds
const float k_flMouseFadeTime = 0.25f; 

// Time to transition for fade in/out when gamepad becomes active in seconds
const float k_flMouseFadeTimeGamepadActive = 0.10f; 



//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CMouseCursorTexture::CMouseCursorTexture( CImageResourceManager *pImageManager )
{
	m_pImageManager = pImageManager;
#if !defined( SOURCE2_PANORAMA )
	m_pMouseCursorArrow = m_pImageManager->LoadImageFromURL( NULL, NULL, "file://{images}/cursors/arrow.png", false, k_EImageFormatR8G8B8A8 );
	m_pMouseCursorIBeam = m_pImageManager->LoadImageFromURL( NULL, NULL, "file://{images}/cursors/ibeam.png", false, k_EImageFormatR8G8B8A8 );
	m_pMouseCursorHand  = m_pImageManager->LoadImageFromURL( NULL, NULL, "file://{images}/cursors/hand.png", false, k_EImageFormatR8G8B8A8 );
#else
	m_pMouseCursorArrow = NULL;
	m_pMouseCursorIBeam = NULL;
	m_pMouseCursorHand  = NULL;
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CMouseCursorTexture::~CMouseCursorTexture()
{
	SAFE_RELEASE( m_pMouseCursorArrow );
	SAFE_RELEASE( m_pMouseCursorIBeam );
	SAFE_RELEASE( m_pMouseCursorHand );
}


//-----------------------------------------------------------------------------
// Purpose: get the IImageSource for this cursor type
//-----------------------------------------------------------------------------
IImageSource *CMouseCursorTexture::GetTexture( EMouseCursors eCursor, Vector2D *pptHotspot )
{
	Assert( pptHotspot != NULL );
	
	switch ( eCursor )
	{
	case eMouseCursor_None:
		return NULL;
		break;
	default:
	case eMouseCursor_Arrow:
		pptHotspot->x = 0.0f;
		pptHotspot->y = 0.0f;
		return m_pMouseCursorArrow;
		break;
	case eMouseCursor_IBeam:
		pptHotspot->x = 0.5f;
		pptHotspot->y = 0.5f;
		return m_pMouseCursorIBeam;
		break;
	case eMouseCursor_Hand:
		pptHotspot->x = 0.05f;
		pptHotspot->y = 0.05f;
		return m_pMouseCursorHand;
		break;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CMouseCursorRender::CMouseCursorRender( CTopLevelWindow *pWindowParent )
{
	m_flOpacity = 1.0f;
	m_bCursorVisible = true;
	m_flLastMouseMove = 0.0f;
	m_flMouseFadeOutTime = 0.0f;
	m_flMouseFadeInTime = 0.0f;
	m_bHideOnInactivity = false;
	m_flIgnoreMovementUntil = 0.0f;
	m_pointRenderMouse = Vector2D( 0.0f, 0.0f );
	m_flIngoreGamePadEventsUntil = 0.0f;
	m_bHideOnGamepadActivity = true;
	// m_pointMainThreadMouse is NOT initialized so we can track its initial invalid state
	m_pWindowParent = pWindowParent;
	m_bUseHardwareCursorPositionForRendering = true; // old default

	Vector2D vecPoints[4];
	panorama::GetAnimationCurveControlPoints( panorama::k_EAnimationEaseInOut, vecPoints );
	m_Bezier.SetControlPoints( vecPoints );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CMouseCursorRender::~CMouseCursorRender()
{
}


#if defined( WIN32 )
void CMouseCursorRender::RunRenderFrame( HWND hWindow, float flCurrentFrameTime, uint32 unSurfaceWidth, uint32 unSurfaceHeight, bool bEnforceWindowAspectRatio )
#elif defined( POSIX )
void CMouseCursorRender::RunRenderFrame( SDL_Window *hWindow, float flCurrentFrameTime, uint32 unSurfaceWidth, uint32 unSurfaceHeight, bool bEnforceWindowAspectRatio )
#endif
{
	if( m_bUseHardwareCursorPositionForRendering )
	{
		int nWindowWidth = 0, nWindowHeight = 0;
		int nWindowMouseX = 0, nWindowMouseY = 0;

#if defined( WIN32 )
		RECT rc;
		::GetClientRect( hWindow, &rc );

		nWindowWidth = rc.right - rc.left;
		nWindowHeight = rc.bottom - rc.top;
#elif defined( POSIX )
		
		SDL_GetWindowSize(hWindow, &nWindowWidth, &nWindowHeight);
#endif
		
#if defined( WIN32 )
		{
			POINT pt;
			::GetCursorPos( &pt );
			::ScreenToClient( hWindow, &pt );
			nWindowMouseX = pt.x;
			nWindowMouseY = pt.y;
		}
#elif defined( POSIX )
		{
			SDL_SysWMinfo info;
			
			memset( &info, 0, sizeof( info ) );
			SDL_VERSION( &info.version );
			
			SDL_GetWindowWMInfo( hWindow, &info );
			
#if defined(LINUX)
			if ( info.subsystem == SDL_SYSWM_X11 )
			{
				Window rootWindow, childWindow;
				int rootX, rootY;
				unsigned int mouseState;
				
				XQueryPointer(info.info.x11.display, info.info.x11.window, &rootWindow, &childWindow,
							&rootX, &rootY, &nWindowMouseX, &nWindowMouseY, &mouseState);
			}
			else
#endif
			{
				SDL_GetMouseState( &nWindowMouseX, &nWindowMouseY );
			}
		}
#endif
		m_pointRenderMouse.x = nWindowMouseX;
		m_pointRenderMouse.y = nWindowMouseY;

		ConvertClientCoordinatesToSurface( &m_pointRenderMouse.x, &m_pointRenderMouse.y, nWindowWidth, nWindowHeight, unSurfaceWidth, unSurfaceHeight, bEnforceWindowAspectRatio );
	}
	else
	{
		AUTO_LOCK( m_mutexCursorTime );

		// just use cursor position from main thread
		m_pointRenderMouse = m_pointMainThreadMouse;
	}

	// update opacity
	if( m_flMouseFadeOutTime > 0.0f )
	{
		if( m_flMouseFadeOutTime <= flCurrentFrameTime )
		{
			// mouse is full faded out so lets just not draw it
			m_flOpacity = 0.0f;
			m_flMouseFadeOutTime = 0.0f;
		}
		else
		{
			Vector2D vecReturn;
			m_Bezier.Evaluate( clamp( (m_flMouseFadeOutTime - flCurrentFrameTime) / k_flMouseFadeTime, 0.0f, 1.0f ), vecReturn );
			m_flOpacity = vecReturn.y;
		}
	}
	else if( m_flMouseFadeInTime > flCurrentFrameTime )
	{
		// in the process of fading, lets scale our opacity
		Vector2D vecReturn;
		m_Bezier.Evaluate( clamp( 1.0 - (m_flMouseFadeInTime - flCurrentFrameTime) / k_flMouseFadeTime, 0.0f, 1.0f ), vecReturn );
		m_flOpacity = vecReturn.y;
	}
	else
	{
		m_flOpacity = 1.0f;
		m_flMouseFadeInTime = 0.0f;
	}
};


//-----------------------------------------------------------------------------
// Purpose: update the fade in and out times based on mouse activity, doesn't control the actual visibility of the cursor, the render thread does that
//-----------------------------------------------------------------------------
void CMouseCursorRender::RunFrame( Vector2D vecMousePosition, bool bInWindow, bool bMouseClicked, bool bMouseMoved, bool bGamepadActiveThisFrame, bool bKeyboardActiveThisFrame )
{
	DbgVerify( ThreadInMainThread() );
	double flCurrentFrameTime = Plat_FloatTime();

	AUTO_LOCK( m_mutexCursorTime );

	if ( !m_pointMainThreadMouse.IsValid() )
	{
		// this is our first mouse movement for the window, lets initialize the position but otherwise not act on the movement
		m_pointMainThreadMouse = vecMousePosition;
		return;
	}

	if ( flCurrentFrameTime >= m_flIgnoreMovementUntil )
	{
		// If the cursor is already visible we just need a pixel of movement, if the cursor is invisible we require a little more to wake
		// the mouse position moved if it was more than 3 pixels and over us OR if the cursor is visible already then any move
		bool bMousePositionMoved = ( ( vecMousePosition.DistTo( m_pointMainThreadMouse ) > 3.0f && bInWindow ) 
			|| ( vecMousePosition != m_pointMainThreadMouse && bInWindow && m_bCursorVisible ) );

		// Did the position moved AND the OS level detected enough mouse movement, or was a mouse button hit
		if ( ( bMousePositionMoved && bMouseMoved ) || bMouseClicked )
		{
			// if the mouse moved AND is over our window then move forward the last time we saw it
			m_flLastMouseMove = flCurrentFrameTime;
			
			WakeupMouseCursor(); // make cursor visible if not already
		}
	}

	if ( bMouseMoved && m_bCursorVisible == false )
	{
		// if we just hid the cursor then tell the input engine to reset its even count
		m_pWindowParent->UIWindowInput()->ResetMouseMoveCount();
	}

	m_pointMainThreadMouse = vecMousePosition;

	if( m_bCursorVisible && m_bHideOnGamepadActivity &&
		( bGamepadActiveThisFrame || bKeyboardActiveThisFrame ) && 
		( m_flIngoreGamePadEventsUntil < flCurrentFrameTime ) )
	{
		// gamepad just moved and we are not fading out right now, lets schedule it to fade out
		FadeOutCursorNow();
	}

	float flTimeoutPeriod = k_flMouseStartFadeTimeoutOutofWindow;
	if ( bInWindow )
		flTimeoutPeriod = k_flMouseStartFadeTimeoutInactivity;

	if( m_bCursorVisible && m_bHideOnInactivity && ( flCurrentFrameTime - m_flLastMouseMove )> flTimeoutPeriod )
	{
		// mouse hasn't moved in our fade timeout and we are not fading right now, lets schedule it to fade
		FadeOutCursorNow();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called off thread potentially, wake up the cursor right away
//-----------------------------------------------------------------------------
void CMouseCursorRender::WakeupMouseCursor()
{
	AUTO_LOCK( m_mutexCursorTime );
	m_flLastMouseMove = Plat_FloatTime();
	
	if( m_bCursorVisible )
		return; // no change needed
	
	m_flMouseFadeOutTime = 0.0f;
	m_flMouseFadeInTime = m_flLastMouseMove + k_flMouseFadeTime;
	m_flIngoreGamePadEventsUntil = m_flLastMouseMove + 0.2f;
		
	m_bCursorVisible = true;
}


//-----------------------------------------------------------------------------
// Purpose: Called off thread potentially, fade out the cursor right away
//-----------------------------------------------------------------------------
void CMouseCursorRender::FadeOutCursorNow()
{
	AUTO_LOCK( m_mutexCursorTime );

	if( !m_bCursorVisible )
		return; // no change needed
	
	m_flMouseFadeInTime = 0.0f;
	m_flMouseFadeOutTime = Plat_FloatTime() + k_flMouseFadeTimeGamepadActive;

	m_flIgnoreMovementUntil = Plat_FloatTime() + 0.4f;
	m_bCursorVisible = false;
}
