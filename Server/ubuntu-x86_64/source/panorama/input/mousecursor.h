//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef MOUSECURSOR_H
#define MOUSECURSOR_H

#ifdef _WIN32
#pragma once
#endif

#include "panorama/input/mousecursors.h"
#include "mathlib/mathlib.h"
#include "mathlib/beziercurve.h"

#ifdef POSIX
#if defined( SOURCE2_PANORAMA )
#include <SDL.h>
#include <SDL_syswm.h>
#else
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#endif
#endif


namespace panorama
{

class IImageSource;
class CImageResourceManager;

//-----------------------------------------------------------------------------
// Purpose: Stateless helper func for converting client window coordinates to surface coordinates.
//
// IMPORTANT: This is called on multiple threads, hence why it is a stateless function not a member.  
// Don't change that.
//-----------------------------------------------------------------------------
inline void ConvertClientCoordinatesToSurface( float *px, float *py, uint32 unWindowWidth, uint32 unWindowHeight, uint32 unSurfaceWidth, uint32 unSurfaceHeight, bool bEnforceAspectRatio )
{
	if ( !bEnforceAspectRatio )
	{
		float x = *px / (float)unWindowWidth;
		float y = *py / (float)unWindowHeight;

		*px = x * (float)unSurfaceWidth;
		*py = y * (float)unSurfaceHeight;
	}
	else
	{
		float flScaleX = (float)unWindowWidth / (float)unSurfaceWidth;
		float flScaleY = (float)unWindowHeight / (float)unSurfaceHeight;
		float flScaleToUse = MIN( flScaleX, flScaleY );
		flScaleX = flScaleToUse;
		flScaleY = flScaleToUse;

		float flTranslateX = ((float)unWindowWidth - (unSurfaceWidth*flScaleX)) / 2.0f;
		float flTranslateY = ((float)unWindowHeight - (unSurfaceHeight*flScaleY)) / 2.0f;

		if ( *px < flTranslateX )
			*px = 0;
		else if ( *px > unWindowWidth - flTranslateX )
			*px = unSurfaceWidth;
		else
			*px = (*px - flTranslateX) / flScaleX;

		if ( *py < flTranslateY )
			*py = 0;
		else if ( *py > unWindowHeight - flTranslateY )
			*py = unSurfaceHeight;
		else
			*py = (*py -flTranslateY ) / flScaleY;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Stateless helper func for converting client window coordinates to surface coordinates.
//
// IMPORTANT: This is called on multiple threads, hence why it is a stateless function not a member.  
// Don't change that.
//-----------------------------------------------------------------------------
inline void ConvertSurfaceCoordinatesToClient( float *px, float *py, uint32 unWindowWidth, uint32 unWindowHeight, uint32 unSurfaceWidth, uint32 unSurfaceHeight )
{
	float x = *px / (float) unSurfaceWidth;
	float y = *py / (float) unSurfaceHeight;

	*px = x * (float) unWindowWidth;
	*py = y * (float) unWindowHeight;
}

//-----------------------------------------------------------------------------
// Purpose: container of the mouse cursors we support and the image data behind them
//-----------------------------------------------------------------------------
class CMouseCursorTexture
{
public:
	CMouseCursorTexture( CImageResourceManager *pImageManager );
	virtual ~CMouseCursorTexture();
	
	IImageSource *GetTexture( EMouseCursors eCursor, Vector2D *pptHotspot );


private:
	CImageResourceManager * m_pImageManager;
	IImageSource *m_pMouseCursorArrow;
	IImageSource *m_pMouseCursorIBeam;
	IImageSource *m_pMouseCursorHand;
};


//-----------------------------------------------------------------------------
// Purpose: container of the mouse cursors position and state used by the render thread
//-----------------------------------------------------------------------------
class CMouseCursorRender
{
public:
	CMouseCursorRender( CTopLevelWindow *pWindowParent );
	virtual ~CMouseCursorRender();

	// true if the cursor is showing at all, safe to call from any thread
	bool BCursorVisible() const { return m_bCursorVisible; }

	void SetHideOnGamepadActivity( bool bHide )
	{
		m_bHideOnGamepadActivity = bHide;
	}

	void SetHideOnInactivity( bool bHide )
	{
		m_bHideOnInactivity = bHide;
	}

	bool BHideOnGamepadActivity() const { return m_bHideOnGamepadActivity; }

	void WakeupMouseCursor();
	void FadeOutCursorNow();

	// called by the main thread
	void RunFrame( Vector2D vecMousePosition, bool bInWindow, bool bMouseClicked, bool bMouseMoved, bool bGamepadActiveThisFrame, bool bKeyboardActiveThisFrame );

	void UseHardwareCursorPositionForRendering( bool bEnable ) { m_bUseHardwareCursorPositionForRendering = bEnable; }
		
	//
	// functions under here can only be called by the render thread
	//
#ifdef WIN32
	void RunRenderFrame( HWND hWindow, float flCurrentFrameTime, uint32 unSurfaceWidth, uint32 unSurfaceHeight, bool bEnforceWindowAspectRatio );
#endif
#ifdef POSIX
	void RunRenderFrame( SDL_Window *hWindow, float flCurrentFrameTime, uint32 unSurfaceWidth, uint32 unSurfaceHeight, bool bEnforceWindowAspectRatio );
#endif

	float GetCursorOpacity() const { return m_flOpacity; }

	// position of the cursor in surface space (not window space)
	Vector2D GetRenderCursorPosition() const { return m_pointRenderMouse; }

private:

	volatile float m_flOpacity;
	volatile bool m_bCursorVisible;
	CThreadMutex m_mutexCursorTime;

	CCubicBezierCurve< Vector2D > m_Bezier;

	Vector2D m_pointRenderMouse; // the point we last saw the mouse at on the render thread
	Vector2D m_pointMainThreadMouse; // the point we last saw the mouse at on the render thread
	double	m_flLastMouseMove; // timestamp of the last time the cursor co-ords changed
	double	m_flMouseFadeOutTime; // timestamp of when the cursor should be fully faded out by
	double	m_flMouseFadeInTime; // timestamp of when the cursor should be fully faded in by
	double	m_flIgnoreMovementUntil; // timestamp of when to ignore mouse movement until
	bool	m_bUseHardwareCursorPositionForRendering; // use hardware cursor pos for rendering instead of position from main thread, smoother
	double	m_flIngoreGamePadEventsUntil; // timestamp of when to ignore disabling cursor due to gamepad button events
	
	bool m_bHideOnGamepadActivity; // if set, hide the mouse cursor as soon as we detect gamepad activity. Thats the default, except in web browser
	bool m_bHideOnInactivity; // if set, hide the mouse cursor if there wasn't any mouse activity for some time
	CTopLevelWindow *m_pWindowParent;
};

} // namespace panorama

#endif // MOUSECURSOR_H