//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef UITOPLEVELWINDOWSDL_H
#define UITOPLEVELWINDOWSDL_H

#ifdef _WIN32
#pragma once
#endif

#include "panorama/uitoplevelwindow.h"
#include "renderer/iui3dsurface.h"
#include "input/mousecursors.h"

#include <SDL.h>
#ifndef SOURCE2_PANORAMA
#if defined(WIN32) 
#include "renderer/win32openglfuncs.h"
#else
#include <SDL_opengl.h>
#endif
#endif
#ifdef LINUX
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#endif

namespace panorama
{

//
// Top level window class
//
class CTopLevelWindowSDL : public CTopLevelWindow
{
public:
	CTopLevelWindowSDL( CUIEngine *pUIEngineParent );
	virtual ~CTopLevelWindowSDL();

	// Initialize backing surface for window
	virtual bool BInitializeSurface( const char *pchWindowTitle, int nWidth, int nHeight, IUIEngine::ERenderTarget eRenderType, bool bFixedSurfaceSize, bool bEnforceWindowAspectRatio, bool bUseCustomMouseCursor, const char *pchTargetMonitor );

	// Resize the window to specified dimensions
	virtual void OnWindowResize( uint32 width, uint32 height );

	// Pump win32 message loop
	static void PumpMessageLoop();

	// Window position management
	virtual void SetWindowPosition( float x, float y );
	virtual void GetWindowPosition( float &x, float &y );
	virtual void GetWindowBounds( float &left, float &top, float &right, float &bottom );
	virtual void GetClientDimensions( float &width, float &height );
	virtual void Activate( bool bForceful );
	virtual void Minimize();
	virtual void SetTopMost( bool bTopMost) { }
	virtual bool BHasFocus();
	virtual bool BIsVisible();
	virtual void SetVisible( bool bVisible ) { AssertMsg( false, "SetVisible not implemented on CTopLevelWindowSDL" ); }
	virtual void* GetNativeWindowHandle();
	bool BUseSystemCursor() { return !m_bUseCustomMouseCursor; }
	virtual void ForceHideWindow();

	// Clipboard functions
	static CTopLevelWindowSDL * FindWindowForSDLWindow( SDL_Window *hSDLWindow );

	// Necessary for generating mouse enter & leave events on windows
	bool IsMouseOver() { return m_bMouseOverWindow; }
	void OnMouseEnter();
	void OnMouseLeave() { m_bMouseOverWindow = false; }

	virtual void SetMouseCursor( EMouseCursors eCursor ) OVERRIDE;
	void ClearRepeats( KeyCode code );
	int IncrementRepeats( KeyCode code );

	virtual void SetPreventForceWindowOnTop( bool bPreventForceTopLevel );
	
	void OnLostFocus();
	void OnGotFocus();

	uint32 GetSDLTickCreateTime() { return m_unSDLTickCreateTime; }

	virtual bool BAllowInput( InputMessage_t &msg );

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName );
	static void ValidateStatics( CValidator &validator, const tchar *pchName );
#endif
protected:

	virtual void Shutdown();

private:
	void SetIcon();

	static CUtlMap< SDL_Window *, CTopLevelWindowSDL *, int, CDefLess< SDL_Window * > > s_MapWindowInstances;

	SDL_Window *m_hSDLWindow;
	SDL_GLContext m_hGLContext;

	CUtlMap< EMouseCursors, SDL_Cursor *, int, CDefLess< EMouseCursors > > m_MapCursors;

	IUI3DSurface *m_p3DSurface;
	bool m_bMouseOverWindow;
	bool m_bHidden;
	uint32 m_unSDLTickCreateTime;
	CUtlMap< KeyCode, int, int, CDefLess< KeyCode > > m_mapKeyRepeats;
#ifdef LINUX
	Display *m_pXDisplay;
	Window m_XWindow;
#elif defined(OSX)
	void *m_hWindowRef;
	bool m_bIsOnActiveSpace;
#elif defined(WIN32)
	HWND m_hWindowRef;
#endif
};

} // namespace panorama

#endif // UITOPLEVELWINDOWSDL_H
