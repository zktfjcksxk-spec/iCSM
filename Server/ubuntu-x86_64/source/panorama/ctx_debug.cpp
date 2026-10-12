#include "stdafx.h"
#include "ctx_debug.h"

#if ( defined( _WIN32 ) && !defined( _X360 ) )

#include <windows.h>
#include "resource.h"

static HINSTANCE g_hPanoramaInstance = 0;
static void (*g_pfnDebuggerRunFrame )() = nullptr;

BOOL WINAPI DllMain(
  HINSTANCE hinstDLL,  // handle to the DLL module
  DWORD fdwReason,     // reason for calling function
  LPVOID lpvReserved   // reserved
)
{
	g_hPanoramaInstance = hinstDLL;
	return true;
}

INT_PTR CALLBACK V8AssertDialogProc(
  HWND hDlg,  // handle to dialog box
  UINT uMsg,     // message
  WPARAM wParam, // first message parameter
  LPARAM lParam  // second message parameter
)
{
	switch( uMsg )
	{
		case WM_INITDIALOG:
		{
			// Center the dialog.
			RECT rcDlg, rcDesktop;
			GetWindowRect( hDlg, &rcDlg );
			GetWindowRect( GetDesktopWindow(), &rcDesktop );
			SetWindowPos( 
				hDlg, 
				HWND_TOP, 
				((rcDesktop.right-rcDesktop.left) - (rcDlg.right-rcDlg.left)) / 2,
				((rcDesktop.bottom-rcDesktop.top) - (rcDlg.bottom-rcDlg.top)) / 2,
				0,
				0,
				SWP_NOSIZE );

			HWND hTextBox = GetDlgItem( hDlg, IDC_STATIC1 );
			SetWindowText( hTextBox, (const char*)lParam );

			// Disable debugger button if not connected
			if ( !Plat_IsInDebugSession() )
			{
				HWND hDbgButton = GetDlgItem( hDlg, IDC_DEBUG );
				ShowWindow( hDbgButton, SW_HIDE);
			}

			// Set up a timer to tick the websocket if connected
			const int timerId = 1;
			SetTimer( hDlg, timerId, 2, (TIMERPROC) NULL );

			return TRUE;
		}

		case WM_TIMER:

			if ( g_pfnDebuggerRunFrame )
			{
				g_pfnDebuggerRunFrame();
			}

			break;

		case WM_COMMAND:
		{
			switch( LOWORD( wParam ) )
			{
			case IDC_CONTINUE:
				EndDialog( hDlg, V8_ASSERT_DLG_CONTINUE );
				break;

			case IDC_IGNORE:
				EndDialog( hDlg, V8_ASSERT_DLG_IGNORE_ALL );
				break;

			case IDC_DEBUG:
				EndDialog( hDlg, V8_ASSERT_DLG_DEBUG );
				break;
			}

			break;
		}
	}

	return FALSE;
}

static HWND g_hBestParentWindow;

static BOOL CALLBACK ParentWindowEnumProc(
  HWND hWnd,      // handle to parent window
  LPARAM lParam   // application-defined value
)
{
	if ( IsWindowVisible( hWnd ) )
	{
		DWORD procID;
		GetWindowThreadProcessId( hWnd, &procID );
		if ( procID == (DWORD)lParam )
		{
			g_hBestParentWindow = hWnd;
			return FALSE; // don't iterate any more.
		}
	}
	return TRUE;
}


static HWND FindLikelyParentWindow()
{
	// Enumerate top-level windows and take the first visible one with our processID.
	g_hBestParentWindow = NULL;
	EnumWindows( ParentWindowEnumProc, GetCurrentProcessId() );
	return g_hBestParentWindow;
}
#endif

V8AssertDlgResult_t V8_AssertDlg( const char *pText, void (*pfnDebuggerRunFrame)(void) )
{
#if ( defined( _WIN32 ) && !defined( _X360 ) )

	g_pfnDebuggerRunFrame = pfnDebuggerRunFrame;

	HWND hParentWindow = FindLikelyParentWindow();

	V8AssertDlgResult_t retval = (V8AssertDlgResult_t)DialogBoxParam( g_hPanoramaInstance, 
		MAKEINTRESOURCE( IDD_V8_EXCEPTION_DIALOG ), g_hBestParentWindow, 
		V8AssertDialogProc,(LPARAM)pText );
	
	return retval;

#else

	return V8_ASSERT_DLG_IGNORE_ALL;

#endif
}
