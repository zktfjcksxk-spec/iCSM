//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef UITOPLEVELWINDOWWIN32_H
#define UITOPLEVELWINDOWWIN32_H

#ifdef _WIN32
#pragma once
#endif

#include "panorama/uitoplevelwindow.h"
#include "renderer/iui3dsurface.h"
#include "winlite.h"

namespace panorama
{

class CUIRenderEngine;
class CPanel2D;

//
// Top level window class
//
class CTopLevelWindowWin32 : public CTopLevelWindow
{
	typedef CTopLevelWindow BaseClass;
public:
	CTopLevelWindowWin32( CUIEngine *pUIEngineParent );
	virtual ~CTopLevelWindowWin32();

	// Initialize backing surface for window
	virtual bool BInitializeSurface( const char *pchWindowTitle, int nWidth, int nHeight, IUIEngine::ERenderTarget eRenderType, bool bFixedSurfaceSize, bool bEnforceWindowAspectRatio, bool bUseCustomMouseCursor, const char *pchTargetMonitor );

	// Resize the window to specified dimensions
	virtual void OnWindowResize( uint32 width, uint32 height );

	// Pump win32 message loop
	static void PumpMessageLoop();

	// Run any per window frame func logic
	virtual void RunPlatformFrame();

	// Window position management
	virtual void SetWindowPosition( float x, float y );
	virtual void GetWindowPosition( float &x, float &y );
	virtual void GetWindowBounds( float &left, float &top, float &right, float &bottom );
	virtual void GetClientDimensions( float &width, float &height );
	virtual void Activate( bool bForceful );
	virtual void Minimize();
	virtual void SetTopMost( bool bTopMost );
	virtual bool BHasFocus();
	virtual bool BIsVisible();
	virtual void SetVisible( bool bVisible ) { AssertMsg( false, "SetVisible not implemented on CTopLevelWindowWin32" ); }
	virtual void* GetNativeWindowHandle() { return m_hWnd; }
	virtual void ForceHideWindow();

	static CTopLevelWindowWin32 * FindWindowForHWND( HWND );

#ifdef PANORAMA_STEAMUI_STREAMING_CAPTURE_WIN32
	void SetSteamUIStreamingCaptureCallback( SteamUIStreamingCaptureCallback_t pCallback ) OVERRIDE;
#endif

	// Necessary for generating mouse enter & leave events on windows
	virtual bool IsMouseOver() { return m_bMouseOverWindow; }
	void OnMouseEnter();
	void OnMouseLeave() { m_bMouseOverWindow = false; }

	virtual void SetMouseCursor( EMouseCursors eCursor ) OVERRIDE;

	void ClearRepeats( KeyCode code );
	int IncrementRepeats( KeyCode code );

	float GetLastForceActivateTime() { return m_flLastForceActivateTime; }

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName );
	static void ValidateStatics( CValidator &validator, const tchar *pchName );
#endif
protected:

	virtual void Shutdown();

private:
	static CUtlMap< HWND, CTopLevelWindowWin32 *, int, CDefLess< HWND > > s_MapWindowInstances;

	HINSTANCE m_hInstance;
	HWND m_hWnd;
	const wchar_t * m_pwchWindowClass;

	IUI3DSurface *m_p3DSurface;
	bool m_bMouseOverWindow;

	double m_flLastForceActivateTime;

	CUtlMap< KeyCode, int, int, CDefLess< KeyCode > > m_mapKeyRepeats;
};

uint32 GetWin32InputModifiers();
EInputType GetInputTypeForMouseMsg( UINT msg, int x, int y );
MouseCode GetMouseCodeForEvent( UINT msg, UINT wparam );

} // namespace panorama

#endif // UITOPLEVELWINDOWWIN32_H
