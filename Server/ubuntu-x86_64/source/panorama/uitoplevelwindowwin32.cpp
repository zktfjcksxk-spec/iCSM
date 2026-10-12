//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx.h"
#include "uitoplevelwindowwin32.h"
#include "renderer/uirenderengine.h"
#include "uienginewin32.h"
#include "renderer/d3d10d2dsurface.h"
#include "platformhelpers.h"
#ifdef WIN32
#include <TlHelp32.h>
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

const DWORD k_WindowStyle = WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_POPUP | WS_THICKFRAME | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
const DWORD k_WindowStyleEx = WS_EX_APPWINDOW;

const DWORD k_WindowStyleFullscreenBorderless = WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_POPUP;
const DWORD k_WindowStyleExFullscreenBorderless = 0; //WS_EX_TOPMOST;

CInterlockedInt g_nInFullscreenSwitch;

// Some defines from windows.h
#define WM_MOUSEWHEEL				0x020A
#define SPI_GETWHEELSCROLLLINES		0x0068
#define WHEEL_DELTA					120
static int32 g_nMouseDeltaLeftOvers = 0;

CUtlMap< HWND, CTopLevelWindowWin32 *, int, CDefLess< HWND > > CTopLevelWindowWin32::s_MapWindowInstances;

namespace panorama
{

static const int MS_WM_XBUTTONDOWN	= 0x020B;
static const int MS_WM_XBUTTONUP	= 0x020C;
static const int MS_WM_XBUTTONDBLCLK = 0x020D;


//-----------------------------------------------------------------------------
// Purpose: Gets the mouse code for an win32 input event
//-----------------------------------------------------------------------------
MouseCode GetMouseCodeForEvent( UINT msg, UINT wparam )
{
	switch( msg )
	{
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_LBUTTONDBLCLK:
		return MOUSE_LEFT;

	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_RBUTTONDBLCLK:
		return MOUSE_RIGHT;

	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_MBUTTONDBLCLK:
		return MOUSE_MIDDLE;

	case MS_WM_XBUTTONDOWN:
	case MS_WM_XBUTTONUP:
	case MS_WM_XBUTTONDBLCLK:
	{
		if ( GET_XBUTTON_WPARAM( wparam ) == XBUTTON1 )
			return MOUSE_4;
		else
			return MOUSE_5;
	}
	}

	return MOUSE_INVALID;
}


//-----------------------------------------------------------------------------
// Purpose: Gets the input type for an win32 mouse event
//-----------------------------------------------------------------------------
EInputType GetInputTypeForMouseMsg( UINT msg, int x, int y )
{
	switch( msg )
	{
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case MS_WM_XBUTTONDOWN:
		{
			static RECT s_rcClick; // last rect a click happened in
			static int s_cClicks = 0; // number of clicks in a row
			static DWORD s_tmLastClick = 0; // window message loop time for a click

			POINT pt = { x, y };
			DWORD tmClick = GetMessageTime();

			// check if the click is inside the click bounds 
			// and within the multi-click timeout
			if ( !PtInRect(&s_rcClick, pt) ||
				tmClick - s_tmLastClick > GetDoubleClickTime()) 
			{
					s_cClicks = 0;
			}

			s_cClicks++; // another click please

			s_tmLastClick = tmClick;
			// setup the rect this click happened in
			SetRect( &s_rcClick, x, y, x+1, y+1 );
			InflateRect( &s_rcClick,
							GetSystemMetrics(SM_CXDOUBLECLK) / 2,
							GetSystemMetrics(SM_CYDOUBLECLK) / 2);

			switch( s_cClicks % 3 )
			{
			case 1:
				return k_eMouseDown;
			case 2:
				return k_eMouseDoubleClick;
			case 0:
				return k_eMouseTripleClick;
			default:
				return k_eInputNone;
			}
		}

	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MBUTTONUP:
	case MS_WM_XBUTTONUP:
		return k_eMouseUp;

	case WM_LBUTTONDBLCLK:
	case WM_RBUTTONDBLCLK:
	case WM_MBUTTONDBLCLK:
	case MS_WM_XBUTTONDBLCLK:
		AssertMsg( false, "Should be handled by multi-click logic above" );
		return k_eMouseDoubleClick;

	case WM_MOUSEWHEEL:
		return k_eMouseWheel;
	}

	return k_eInputNone;
}


//-----------------------------------------------------------------------------
// Purpose: Translates win32 mouse message modifiers into our modifiers
//-----------------------------------------------------------------------------
uint32 GetWin32InputModifiers()
{
	uint32 unRet = 0;

	if ( GetKeyState( VK_LCONTROL ) & 0x80 )
		unRet |= MODIFIER_LCONTROL;
	if ( GetKeyState( VK_RCONTROL ) & 0x80 )
		unRet |= MODIFIER_RCONTROL;
	if ( GetKeyState( VK_LSHIFT ) & 0x80 )
		unRet |= MODIFIER_LSHIFT;
	if ( GetKeyState( VK_RSHIFT ) & 0x80 )
		unRet |= MODIFIER_RSHIFT;
	if ( GetKeyState( VK_LMENU ) & 0x80 )
		unRet |= MODIFIER_LALT;
	if ( GetKeyState( VK_RMENU ) & 0x80 )
		unRet |= MODIFIER_RALT;
	
	return unRet;
}

} // Namespace panorama

//-----------------------------------------------------------------------------
// Purpose: WndProc for the ui engine
//-----------------------------------------------------------------------------
static LRESULT CALLBACK WndProc( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam )
{
	CTopLevelWindowWin32 *pWindow = CTopLevelWindowWin32::FindWindowForHWND( hwnd );
	if ( !pWindow )
	{
		return DefWindowProcW( hwnd, msg, wparam, lparam );
	}

	InputMessage_t input;
	MouseCode mouseCode = MOUSE_INVALID;
	EInputType eInputType = k_eInputNone;
	LRESULT bHandled = FALSE;

	bool bBlockOnPaint = false;
	switch (msg)
	{
	case WM_CLOSE:
	case WM_QUIT:
		delete pWindow;
		return 0;
	case WM_DESTROY:
		// Don't need to do anything, we'll get WM_QUIT or WM_CLOSE right after
		return 0;
	case WM_ERASEBKGND:
		return 1;

	case WM_PAINT:
		bBlockOnPaint = true;
	case WM_MOVING:
		{
			PAINTSTRUCT ps;
			BeginPaint( hwnd, &ps );

			static bool s_bInWM_PAINT = false;
			if ( !s_bInWM_PAINT && pWindow && pWindow->BFinishedInitialization() && pWindow->GetUIRenderEngine() && !pWindow->BIsFullscreen() && g_nInFullscreenSwitch == 0 && pWindow->BIsVisible() )
			{
				VPROF_BUDGET( "WM_PAINT/WM_MOVING", VPROF_BUDGETGROUP_TENFOOT );

				pWindow->GetUIRenderEngine()->ClearSwapBuffersEvent();
				UIEngine()->LayoutAndPaintWindows();
				float flLastPaint = pWindow->GetUIRenderEngine()->GetLastPaintFrameTime();

				float flLastRender = 0.0f;
				while( bBlockOnPaint && flLastRender < flLastPaint && pWindow->BIsVisible() )
				{
					pWindow->GetUIRenderEngine()->WaitOnSwapBuffersEvent( 100 );
					flLastRender = pWindow->GetUIRenderEngine()->GetLastPaintFrameTimeRendered();
					pWindow->GetUIRenderEngine()->ClearSwapBuffersEvent();
				}
			}
			EndPaint( hwnd, &ps );
		}
		return 0;

	case WM_SIZE:
	case WM_MOVE:
	case WM_DISPLAYCHANGE:
		{
			// If the window is minimized, we don't want to process size changes, they'll be bogus.
			if ( ::IsIconic( hwnd ) )
			{
				return 0;
			}

			static bool s_bProcessingResize = false;
			if ( s_bProcessingResize )
			{
				return 0;
			}

			if ( pWindow->BIsFullscreenBorderlessWindow() && g_nInFullscreenSwitch == 0 )
			{
				int nLeft = 0;
				int nTop = 0;
				int nScreenWidth = 1920;
				int nScreenHeight = 1080;
				PlatformHelpers::GetMonitorPositonAndSize( nTop, nLeft, nScreenWidth, nScreenHeight, pWindow->GetTargetMonitor() );

				// Client rect and window rect are the same in fullscreen-borderless case
				RECT rWindow = { 0, 0, 0, 0 };
				GetWindowRect( hwnd, &rWindow );
				s_bProcessingResize = true;
				if ( rWindow.top != nTop || rWindow.left != nLeft || rWindow.bottom != nTop + nScreenHeight || rWindow.right != nLeft + nScreenWidth )
				{
					MoveWindow( hwnd, nLeft, nTop, nScreenWidth, nScreenHeight, TRUE );
				}
				if ( nScreenWidth != (int)pWindow->GetWindowWidth() || nScreenHeight != (int)pWindow->GetWindowHeight() )
				{
					pWindow->OnWindowResize( nScreenWidth, nScreenHeight );
				}
				s_bProcessingResize = false;
			}
			else if ( msg == WM_SIZE && pWindow->BEnforceWindowAspectRatio() && !pWindow->BIsFullscreen() && g_nInFullscreenSwitch == 0 )
			{
				RECT r;
				GetWindowRect( hwnd, &r );

				RECT rClient;
				GetClientRect( hwnd, &rClient );
				float flHeightAdjust = ( rClient.bottom - rClient.top ) - ((rClient.right - rClient.left) * ((float)pWindow->GetSurfaceHeight() / (float)pWindow->GetSurfaceWidth()));
				int nAdjust = (int)flHeightAdjust;
				if ( nAdjust != 0 )
				{
					s_bProcessingResize = true;
					r.bottom -= nAdjust;
					MoveWindow( hwnd, r.left, r.top, r.right-r.left, r.bottom-r.top, TRUE );
					pWindow->OnWindowResize( rClient.right-rClient.left, rClient.bottom-rClient.top-nAdjust );
					s_bProcessingResize = false;
				}
			}
			else if ( msg == WM_SIZE )
			{
				// WM_SIZE lparam is packed client area
				UINT width = LOWORD( lparam );
				UINT height = HIWORD( lparam );
				pWindow->OnWindowResize( width, height );
			}
		}
		return 0;
		break;

	case WM_SETCURSOR:
		{   
			if ( pWindow->BUseCustomMouseCursor() )
			{
				// Get client coordinates of cursor   
				POINT pt;   
				GetCursorPos( &pt );   
				ScreenToClient( hwnd, &pt );   
				// Get client rect   
				RECT rc;   
				GetClientRect( hwnd, &rc );   
				rc.right -= rc.left;   
				rc.bottom -= rc.top;   
				// See if cursor is in client area   
				if ( !( pt.x < 0 || pt.x > rc.right || pt.y < 0 || pt.y > rc.bottom ) )      
				{
					bHandled = TRUE;
					SetCursor( NULL ); 
				}
			}
		}
		break;


	case WM_NCMOUSEMOVE:
	case WM_MOUSEMOVE:
		{
			if ( !pWindow->IsMouseOver() )
			{
				// first mouse move message since 
				pWindow->OnMouseEnter();

				input.m_eSource = k_ePanelEventSourceMouse;
				input.m_eInputType = k_eMouseEnter;
				pWindow->UIWindowInput()->InputEvent( input );
			}

			pWindow->UIWindowInput()->OnMouseMove( (float)((short)LOWORD(lparam)), (float)((short)HIWORD(lparam)) );
		}
		break;

	case WM_LBUTTONDOWN:		
	case WM_RBUTTONDOWN:		
	case WM_MBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MBUTTONUP:
	case WM_LBUTTONDBLCLK:
	case WM_RBUTTONDBLCLK:
	case WM_MBUTTONDBLCLK:
	case WM_MOUSEWHEEL:
	case WM_XBUTTONUP:
	case WM_XBUTTONDOWN:
	case WM_XBUTTONDBLCLK:
		input.m_eSource = k_ePanelEventSourceMouse;
		eInputType = GetInputTypeForMouseMsg( msg, LOWORD(lparam), HIWORD(lparam) );
		mouseCode = GetMouseCodeForEvent( msg, wparam );
		if ( eInputType != k_eInputNone && mouseCode != MOUSE_INVALID )
		{
			input.m_eInputType = eInputType;

			input.m_MouseData.m_MouseCode = mouseCode;
			input.m_MouseData.m_Modifiers = GetWin32InputModifiers();
			input.m_MouseData.m_Delta = 0;
			input.m_MouseData.m_RepeatCount = 0;

			float xMouse = LOWORD( lparam );
			float yMouse = HIWORD( lparam );

			pWindow->ConvertClientToSurfaceCoord( &xMouse, &yMouse );
			input.m_MouseData.m_XPos = xMouse;
			input.m_MouseData.m_YPos = yMouse;

			pWindow->UIWindowInput()->InputEvent( input );
		}
		else if ( eInputType == k_eMouseWheel )
		{
			// account for the 'high precision' mice that give increments of WHEEL_DELTA
			int rawscroll = ((short)HIWORD(wparam)) + g_nMouseDeltaLeftOvers;
			int delta = rawscroll/WHEEL_DELTA;
			g_nMouseDeltaLeftOvers = rawscroll % WHEEL_DELTA;

			// account for windows scroll lines setting
			delta *= UIEngine()->GetWheelScrollLines();

			input.m_eInputType = eInputType;
			input.m_MouseData.m_MouseCode = mouseCode;
			input.m_MouseData.m_Modifiers = GetWin32InputModifiers();
			input.m_MouseData.m_Delta = delta;

			float xMouse = LOWORD( lparam );
			float yMouse = HIWORD( lparam );

			pWindow->ConvertClientToSurfaceCoord( &xMouse, &yMouse );
			input.m_MouseData.m_XPos = xMouse;
			input.m_MouseData.m_YPos = yMouse;

			pWindow->GetMouseWheelRepeats( delta < 0, abs(delta), input.m_MouseData.m_RepeatCount );

			pWindow->UIWindowInput()->InputEvent( input );
		}
		break;

	case WM_MOUSELEAVE:
		pWindow->OnMouseLeave();

		input.m_eSource = k_ePanelEventSourceMouse;
		input.m_eInputType = k_eMouseLeave;
		pWindow->UIWindowInput()->InputEvent( input );
		break;

	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		{
			if ( pWindow->UIWindowInput()->BHasWindowFocus() )
			{
				if ( wparam == VK_RETURN && ( GetWin32InputModifiers() & MODIFIER_LALT || GetWin32InputModifiers() & MODIFIER_RALT ) )
				{
					if( !DispatchEvent( ToggleFullscreen(), (IUIPanel*)NULL, pWindow, !pWindow->BIsFullscreen() ) )
						pWindow->SetFullscreen( !pWindow->BIsFullscreen() );
				}
			}
		}
	case WM_KEYUP:
	case WM_SYSKEYUP:
	case WM_CHAR:
	case WM_SYSCHAR:
	case WM_UNICHAR:
		{
			if( (msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP ) && wparam == VK_MENU 
				&& !pWindow->BIsFullscreen() && UIEngine()->GetCurrentFrameTime() - pWindow->GetLastForceActivateTime() < 0.5f )
			{
				// Supress the alt key default handling (bring up system menu for window) as we don't want it to steal focus if this
				// was our simulated alt-key to hack the SetForegroundWindow restrictions...
				return 0;
			}

			KeyCode code = UIEngine()->UIInputEngine()->WindowsVKeyToKeyCode( wparam );
			input.m_flInputTime = Plat_FloatTime();
			input.m_KeyData.m_UniChar = 0;
			input.m_KeyData.m_KeyCode = code;

			input.m_eSource = k_ePanelEventSourceKeyboard;
			input.m_eInputType = k_eKeyUp;
			if ( msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN )
				input.m_eInputType = k_eKeyDown;
			else if ( msg == WM_CHAR || msg == WM_SYSCHAR )
			{
				input.m_eInputType = k_eKeyChar;
				input.m_KeyData.m_UniChar = wparam;
				input.m_KeyData.m_KeyCode = KEY_NONE;
			}
			else if ( msg == WM_UNICHAR )
			{
				if ( wparam == UNICODE_NOCHAR )
				{
					// Windows is checking whether WM_UNICHAR is supported
					// and we must return TRUE.
					return TRUE;
				}
				
				input.m_eInputType = k_eKeyChar;
				input.m_KeyData.m_UniChar = wparam;
				input.m_KeyData.m_KeyCode = KEY_NONE;
				// We've handled this message so don't let DefWindowProc run
				// and we must return FALSE to Windows.
				return FALSE;
			}

			input.m_KeyData.m_bFirstDown = !(lparam & (1<<30));
			input.m_KeyData.m_Modifiers = GetWin32InputModifiers();			
			input.m_KeyData.m_RepeatCount = 0;
			if ( (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) && !input.m_KeyData.m_bFirstDown )
				input.m_KeyData.m_RepeatCount = pWindow->IncrementRepeats( code );
			else
				pWindow->ClearRepeats( code );

			if ( code != KEY_NONE || input.m_eInputType == k_eKeyChar )
				pWindow->UIWindowInput()->InputEvent( input );
		}
		break;

	case WM_SETFOCUS:
		pWindow->UIWindowInput()->GotWindowFocus();
		break;
	case WM_KILLFOCUS:
		pWindow->UIWindowInput()->LostWindowFocus();
		break;
#if !defined( SOURCE2_PANORAMA_FIXME ) && !defined( PANORAMA_PUBLIC_STEAM_SDK )
	case WM_POWERBROADCAST:
		if ( wparam == PBT_APMSUSPEND || wparam == PBT_APMSTANDBY )
		{
			if ( ClientController() && ClientController()->GetNumConnectedControllers() )
			{
				for ( int i = 0; i < MAX_STEAM_CONTROLLERS; ++i )
				{
					ClientController()->TurnOffController( i );
				}
			}
		}
#endif
	default:
		break;
	}

	if ( bHandled == FALSE )
		return DefWindowProcW( hwnd, msg, wparam, lparam );
	else
		return bHandled;
}


//-----------------------------------------------------------------------------
// Purpose: Clear key repeats
//-----------------------------------------------------------------------------
void CTopLevelWindowWin32::ClearRepeats( KeyCode code )
{
	m_mapKeyRepeats.Remove( code );
}


//-----------------------------------------------------------------------------
// Purpose: Track key repeats
//-----------------------------------------------------------------------------
int CTopLevelWindowWin32::IncrementRepeats( KeyCode code )
{
	int iMap = m_mapKeyRepeats.Find( code );
	if ( iMap == m_mapKeyRepeats.InvalidIndex() )
	{
		m_mapKeyRepeats.Insert( code, 1 );
		return 1;
	}
	else
	{
		return (++m_mapKeyRepeats[ iMap ]);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Initializes the surface, creating a window and D3D device
//-----------------------------------------------------------------------------
CTopLevelWindowWin32 * CTopLevelWindowWin32::FindWindowForHWND( HWND hWnd )
{
	int iIndex = s_MapWindowInstances.Find( hWnd );
	if ( iIndex == s_MapWindowInstances.InvalidIndex() )
		return NULL;

	return s_MapWindowInstances[iIndex];
}

//-----------------------------------------------------------------------------
// Purpose: Hides this window entirely from taskbar/etc.
//-----------------------------------------------------------------------------
void CTopLevelWindowWin32::ForceHideWindow( void )
{
	HWND hwnd = (HWND)GetNativeWindowHandle();
	if ( hwnd )
	{
		long style= GetWindowLong(hwnd, GWL_STYLE);
		style &= ~(WS_VISIBLE);
		style |= WS_EX_TOOLWINDOW;
		style &= ~(WS_EX_APPWINDOW); 
		SetWindowLong(hwnd, GWL_STYLE, style);
		ShowWindow(hwnd, SW_SHOW);
		ShowWindow(hwnd, SW_HIDE);
	}
	SetPreventForceWindowOnTop( true );
}


//-----------------------------------------------------------------------------
// Purpose: Steam in-home streaming capture for Big Picture UI
//-----------------------------------------------------------------------------
#ifdef PANORAMA_STEAMUI_STREAMING_CAPTURE_WIN32
void CTopLevelWindowWin32::SetSteamUIStreamingCaptureCallback( SteamUIStreamingCaptureCallback_t pCallback )
{
	if ( m_p3DSurface )
	{
		static_cast<CD3D10D2DSurface*>( m_p3DSurface )->SetSteamUIStreamingCaptureCallback( pCallback );
	}
}
#endif


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CTopLevelWindowWin32::CTopLevelWindowWin32( CUIEngine *pUIEngineParent ) : CTopLevelWindow( pUIEngineParent )
{
	m_pwchWindowClass = NULL;
	m_hInstance = NULL;
	m_hWnd = NULL;
	m_p3DSurface = NULL;
	m_pRenderEngine = NULL;
	m_bFixedSurfaceSize = false;
	m_bEnforceWindowAspectRatio = false;
	m_unSurfaceWidth = 1920;
	m_unSurfaceHeight = 1280;
	m_unWindowWidth = 1920;
	m_unWindowHeight = 1280;
	m_bMouseOverWindow = false;
	m_flLastForceActivateTime = 0.0f;
	m_eRenderTarget = IUIEngine::k_ERenderTargetUnset;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CTopLevelWindowWin32::~CTopLevelWindowWin32()
{
	Shutdown();
}


//-----------------------------------------------------------------------------
// Purpose: Run a frame, pumps input, paints, whatever.
//-----------------------------------------------------------------------------
void CTopLevelWindowWin32::PumpMessageLoop()
{
	VPROF_BUDGET( "CTopLevelWindowWin32::PumpMessageLoop", VPROF_BUDGETGROUP_TENFOOT );
	// Do this first, because message loop could delete us

	MSG msg;
	BOOL bRet;
	while( PeekMessageW( &msg, NULL, 0, 0, PM_NOREMOVE ) )
	{
		bRet = GetMessageW( &msg, NULL, 0, 0 );
		if( bRet != 0 && bRet != -1 )
		{
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
		else
		{
			break;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Set window position
//-----------------------------------------------------------------------------
void CTopLevelWindowWin32::SetWindowPosition( float x, float y )
{
	RECT r; 
	::GetWindowRect( m_hWnd, &r );
	LONG width = r.right - r.left;
	LONG height = r.bottom - r.top;

	::SetWindowPos( m_hWnd, NULL, x, y, width, height, SWP_NOSIZE|SWP_NOACTIVATE );
}


//-----------------------------------------------------------------------------
// Purpose: Get window position and size
//-----------------------------------------------------------------------------
void CTopLevelWindowWin32::GetWindowBounds( float &left, float &top, float &right, float &bottom )
{
	RECT r;
	::GetWindowRect( m_hWnd, &r );
	left = r.left;
	top = r.top;
	right = r.right;
	bottom = r.bottom;
}


//-----------------------------------------------------------------------------
// Purpose: Get position and size of window's client area
//-----------------------------------------------------------------------------
void CTopLevelWindowWin32::GetClientDimensions( float &width, float &height )
{
	RECT r;
	::GetClientRect( m_hWnd, &r );
	width = r.right-r.left;
	height = r.bottom-r.top;
}


//-----------------------------------------------------------------------------
// Purpose: Get window position
//-----------------------------------------------------------------------------
void CTopLevelWindowWin32::GetWindowPosition( float &x, float &y )
{
	RECT r;
	::GetWindowRect( m_hWnd, &r );
	x = r.left;
	y = r.top;
}


//-----------------------------------------------------------------------------
// Purpose: Activates window, bringing to foreground
//-----------------------------------------------------------------------------
void CTopLevelWindowWin32::Activate( bool bForceful )
{
	// SendInput with a null input event appears to allow SetForegroundWindow to steal focus
	// on any version of Windows. Weird but true. I suppose the internal Windows assumption
	// is that there is a small window of time where apps may take focus in response to input?
	if ( bForceful )
	{
		INPUT in = {}; // zero-init, defaults to an empty mouseinput struct
		::SendInput( 1, &in, sizeof( in ) );
	}

	// SetWindowPos/ShowWindow must happen before SetForegroundWindow
	if ( ::IsIconic( m_hWnd ) )
	{
		::ShowWindow( m_hWnd, SW_RESTORE );
	}
	::SetWindowPos( m_hWnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW );
	::SetForegroundWindow( m_hWnd );
	::SetFocus( m_hWnd );
}


//-----------------------------------------------------------------------------
// Purpose: Minimizes window
//-----------------------------------------------------------------------------
void CTopLevelWindowWin32::Minimize()
{
	::ShowWindow( m_hWnd, SW_MINIMIZE );
}


//-----------------------------------------------------------------------------
// Purpose: Sets topmost flag accordingly
//-----------------------------------------------------------------------------
void CTopLevelWindowWin32::SetTopMost( bool bTopMost )
{
	HWND target = bTopMost ? HWND_TOPMOST : HWND_NOTOPMOST;
	SetWindowPos( m_hWnd, target, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE );
}


//-----------------------------------------------------------------------------
// Purpose: Shutdown/close this window
//-----------------------------------------------------------------------------
void CTopLevelWindowWin32::Shutdown()
{
	CTopLevelWindow::Shutdown();

	// Fix the display settings if leaving full screen mode.
	if ( IUIEngine::BIsRenderingToFullScreen( m_eRenderTarget ) )
	{
		ChangeDisplaySettings(NULL, 0);
	}

	if ( m_pRenderEngine )
	{
		delete m_pRenderEngine;
		m_pRenderEngine = NULL;
	}

	s_MapWindowInstances.Remove( m_hWnd );

	if ( m_hWnd )
	{
		DestroyWindow( m_hWnd );
		m_hWnd = NULL;
	}

	// Remove the application instance.
	if ( m_pwchWindowClass )
	{
		UnregisterClassW( m_pwchWindowClass, m_hInstance );
	}
	m_hInstance = NULL;

	if ( m_p3DSurface )
	{
		delete m_p3DSurface;
		m_p3DSurface = NULL;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Process VR input if necessary
//-----------------------------------------------------------------------------
void CTopLevelWindowWin32::RunPlatformFrame()
{
	BaseClass::RunPlatformFrame();
}


//-----------------------------------------------------------------------------
// Purpose: Initializes the surface, creating a window and D3D device
//-----------------------------------------------------------------------------
bool CTopLevelWindowWin32::BInitializeSurface( const char *pchWindowTitle, int nWidth, int nHeight, IUIEngine::ERenderTarget eRenderTarget, bool bFixedSurfaceSize, bool bEnforceWindowAspectRatio, bool bUseCustomMouseCursor, const char *pchTargetMonitor )
{
	VPROF_BUDGET( "CTopLevelWindowWin32::CreateNewWindow", VPROF_BUDGETGROUP_TENFOOT );
	m_strTargetMonitor = pchTargetMonitor;
	m_bFixedSurfaceSize = bFixedSurfaceSize;
	m_bEnforceWindowAspectRatio = bEnforceWindowAspectRatio;
	m_bUseCustomMouseCursor = bUseCustomMouseCursor;
	m_unWindowWidth = nWidth;
	m_unWindowHeight = nHeight;
	m_unSurfaceWidth = nWidth;
	m_unSurfaceHeight = nHeight;
	m_eRenderTarget = eRenderTarget;
	m_pwchWindowClass = L"CUIEngineWin32";

	WNDCLASSEXW wc;
	V_memset( &wc, 0, sizeof( wc ) );

	DEVMODE dmScreenSettings;
	int posX, posY;

	// Get the instance of this application.
	HINSTANCE hInstance = GetModuleHandle(NULL);

	// Setup the windows class with default settings.
	wc.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wc.lpfnWndProc   = WndProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = hInstance;

	DWORD wIconID = 101; // the new bootstrapper uses 101 for its icon ID
	const char *sIconResourceID = getenv( "__STEAM_BOOTSTRAPPER_ICON_ID__" ); // steam2 bootstrapper may override it
	if ( sIconResourceID )
	{
		DWORD wTmpIconID = 0;
		if ( sscanf(sIconResourceID, "%u", &wTmpIconID) == 1 && wTmpIconID > 101 )
		{
			wIconID = wTmpIconID;
		}
	}

	wc.hIcon	= ::LoadIcon( GetModuleHandle( NULL ), MAKEINTRESOURCE( wIconID ) );
	wc.hIconSm  = wc.hIcon;
	wc.hCursor  = NULL; // we want no cursor shown by default over our window, we draw it or set it ourselves
	wc.lpszClassName = m_pwchWindowClass;
	wc.hbrBackground = NULL; //Since we are D3D windows we draw the background ourselves at all times
	wc.lpszMenuName  = NULL;
	wc.cbSize        = sizeof(WNDCLASSEXW);

	// Register the window class.
	::RegisterClassExW( &wc );


	// Determine the resolution of the clients desktop screen.
	int nScreenWidth  = ::GetSystemMetrics( SM_CXSCREEN );
	int nScreenHeight = ::GetSystemMetrics( SM_CYSCREEN );
	int nWindowWidth = nWidth;
	int nWindowHeight = nHeight;

	int nMonitorLeft, nMonitorTop, nMonitorWidth, nMonitorHeight;
	PlatformHelpers::GetMonitorPositonAndSize( nMonitorLeft, nMonitorTop, nMonitorWidth, nMonitorHeight, pchTargetMonitor );

	// Setup the screen settings depending on whether it is running in full screen or in windowed mode.
	if( eRenderTarget == IUIEngine::k_ERenderFullScreen )
	{
		// If full screen set the screen to maximum size of the users desktop and 32bit.

		// bugbug jmccaskey - we don't respect monitor when running real fullscreen, needs work?

		memset(&dmScreenSettings, 0, sizeof(dmScreenSettings));
		dmScreenSettings.dmSize       = sizeof(dmScreenSettings);
		dmScreenSettings.dmPelsWidth  = (unsigned long)nScreenWidth;
		dmScreenSettings.dmPelsHeight = (unsigned long)nScreenHeight;
		dmScreenSettings.dmBitsPerPel = 32;			
		dmScreenSettings.dmFields     = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

		// Change the display settings to full screen.
		::ChangeDisplaySettings( &dmScreenSettings, CDS_FULLSCREEN );

		// Set the position of the window to the top left corner.
		posX = posY = 0;

	}
	else if ( eRenderTarget == IUIEngine::k_ERenderBorderlessFullScreenWindow )
	{
		posX = nMonitorLeft;
		posY = nMonitorTop;
		nWindowWidth = nMonitorWidth;
		nWindowHeight = nMonitorHeight;
	}
	else
	{
		// Place the window in the middle of the screen.
		posX = nMonitorLeft + ((nMonitorWidth - nWidth) / 2);
		posY = nMonitorTop + ((nMonitorHeight - nHeight) / 2);
	}

	RECT r;
	r.left = posX;
	r.top = posY;
	r.right = r.left + nWindowWidth;
	r.bottom = r.top + nWindowHeight;

	m_unWindowWidth = nWindowWidth;
	m_unWindowHeight = nWindowHeight;

	DWORD dwStyle = k_WindowStyle;
	DWORD dwStyleEx = k_WindowStyleEx;

	if ( eRenderTarget == IUIEngine::k_ERenderBorderlessFullScreenWindow )
	{
		dwStyle = k_WindowStyleFullscreenBorderless;
		dwStyleEx = k_WindowStyleExFullscreenBorderless;
	}
	
	AdjustWindowRectEx( &r, dwStyle, FALSE, dwStyleEx );

	// Create the window with the screen settings and get the handle to it.
	m_hWnd = CreateWindowExW( dwStyleEx, wc.lpszClassName, wc.lpszClassName, 
		dwStyle, posX, posY, r.right-r.left, r.bottom-r.top, NULL, NULL, hInstance, NULL);

	if ( !m_hWnd )
		return false;

	// Bring the window up on the screen and set it as main focus.
	SetWindowTextW( m_hWnd, CStrAutoEncode( pchWindowTitle ).ToWString() );
	ShowWindow( m_hWnd, SW_SHOW );
	
	s_MapWindowInstances.Insert( m_hWnd, this );

	CUITextLayoutWin32::BInitGlobals();

	// Now initialize the 3d surface
	CD3D10D2DSurface *pSurface = new CD3D10D2DSurface();
	if ( !pSurface->BInitialize( m_hWnd, m_unSurfaceWidth, m_unSurfaceHeight, nWindowWidth, nWindowHeight, eRenderTarget, bEnforceWindowAspectRatio, bFixedSurfaceSize, bUseCustomMouseCursor ? m_pCursorRender : NULL ) )
	{
		delete pSurface;
		return false;
	}
	m_p3DSurface = pSurface;
	m_pSurfaceInterface = pSurface;
	m_pSurfaceInterface->SetWindowScaleFactor( m_flScaleFactor );

	m_pRenderEngine = new CUIRenderEngine( m_pUIEngineParent, m_p3DSurface, m_pInputEngine, this, nWidth, nHeight );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Resize 3D surface
//-----------------------------------------------------------------------------
void CTopLevelWindowWin32::OnWindowResize( uint32 width, uint32 height )
{
	if ( width == m_unWindowWidth && height == m_unWindowHeight )
		return;

	if ( m_p3DSurface )
	{
		m_unWindowWidth = width;
		m_unWindowHeight = height;

		if ( !m_bFixedSurfaceSize )
		{
			if ( !m_bEnforceWindowAspectRatio )
			{
				m_unSurfaceWidth = width;
				m_unSurfaceHeight = height;
			}

			FOR_EACH_LL( m_listVisiblePanels, i )
			{
				m_listVisiblePanels[i]->InvalidateSizeAndPosition();
			}

			FOR_EACH_LL( m_listInvisiblePanels, i )
			{
				m_listInvisiblePanels[i]->InvalidateSizeAndPosition();
			}
		}		
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called when the mouse is moving over our window
//-----------------------------------------------------------------------------
void CTopLevelWindowWin32::OnMouseEnter()
{
	if ( m_bMouseOverWindow )
		return;
	
	// mouse just entered our window. Need to sign up for mouse tracking so we know when it leaves
	// once the mouse leaves our window, need to call TrackMouseEvent to sign up again
	TRACKMOUSEEVENT track;
	track.cbSize = sizeof( TRACKMOUSEEVENT );
	track.dwFlags = TME_LEAVE;
	track.hwndTrack = m_hWnd;
	TrackMouseEvent( &track );

	m_bMouseOverWindow = true;
}


//-----------------------------------------------------------------------------
// Purpose: set the mouse cursor to this cursor please
//-----------------------------------------------------------------------------
void CTopLevelWindowWin32::SetMouseCursor( EMouseCursors eCursor )
{
	VPROF_BUDGET( "CTopLevelWindowWin32::SetMouseCursor", VPROF_BUDGETGROUP_TENFOOT );

	m_eCursorCurrent = eCursor; // store off the current cursor, the renderer may want to use it

	if ( m_bUseCustomMouseCursor )
		return; // manually pushed by the renderer instead of windows

	switch ( eCursor )
	{
	default:
	case eMouseCursor_Arrow:
		::SetCursor( ::LoadCursor( NULL, IDC_ARROW ) );
		break;
	case eMouseCursor_IBeam:
		::SetCursor( ::LoadCursor( NULL, IDC_IBEAM ) );
		break;
	case eMouseCursor_SizeWE:
		::SetCursor( ::LoadCursor( NULL, IDC_SIZEWE ) );
		break;
	case eMouseCursor_SizeNS:
		::SetCursor( ::LoadCursor( NULL, IDC_SIZENS ) );
		break;
	case eMouseCursor_Hand:
		::SetCursor( ::LoadCursor( NULL, IDC_HAND ) );
		break;
	}
}


//-----------------------------------------------------------------------------
// Purpose: return true if this window has key focus
//-----------------------------------------------------------------------------
bool CTopLevelWindowWin32::BHasFocus()
{
	return m_hWnd == ::GetFocus();
}


//-----------------------------------------------------------------------------
// Purpose: return true any part of the window is being rendered
//-----------------------------------------------------------------------------
bool CTopLevelWindowWin32::BIsVisible()
{
	if ( !m_p3DSurface )
		return false;

	return ! m_p3DSurface->BSurfaceOccluded();
}


#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: validate mem
//-----------------------------------------------------------------------------
void CTopLevelWindowWin32::Validate( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE();

	ValidatePtr( (CD3D10D2DSurface *)m_p3DSurface );
	CTopLevelWindow::Validate( validator, pchName );
}

//-----------------------------------------------------------------------------
// Purpose: validate mem
//-----------------------------------------------------------------------------
void CTopLevelWindowWin32::ValidateStatics( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE_STATIC( "CTopLevelWindowWin32" );
	ValidateObj( s_MapWindowInstances );
}

#endif
