//===== Copyright © 1996-2010, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "imemanager.h"

#include "imemanager_plat.h"

#include "gfx_imemanagerwin32.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Singleton instance
//-----------------------------------------------------------------------------
static CIMEManager s_IMEManager;
#ifdef PANORAMA_ENABLE
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CIMEManager, IIMEManager, IMEMANAGER_INTERFACE_VERSION, s_IMEManager );
#endif

bool g_bIMEDetailedLogging = false;
DEFINE_LOGGING_CHANNEL_NO_TAGS( LOG_IME, "IME");

//-----------------------------------------------------------------------------
// Get dependencies
//-----------------------------------------------------------------------------
static AppSystemInfo_t s_Dependencies[] =
{
	{ NULL, NULL }
};

//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CIMEManager::CIMEManager()
{
	m_pIMEManagerWin32 = NULL;
	m_bValid = false;
	m_bIMEEnabled = false;
}

CIMEManager::~CIMEManager()
{
}

const AppSystemInfo_t* CIMEManager::GetDependencies()
{
	return s_Dependencies;
}

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------
InitReturnVal_t CIMEManager::Init()
{
	InitReturnVal_t nRetVal = BaseClass::Init();
	if ( nRetVal != INIT_OK )
		return nRetVal;

	return INIT_OK; 
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CIMEManager::Connect( CreateInterfaceFn factory )
{
	if ( !BaseClass::Connect( factory ) )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Shutdown
//-----------------------------------------------------------------------------
void CIMEManager::Shutdown()
{
	BaseClass::Shutdown();

	delete m_pIMEManagerWin32;
	m_pIMEManagerWin32 = NULL;

	m_bValid = false;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CIMEManager::Setup( PlatWindow_t hWindow )
{
	if ( m_pIMEManagerWin32 )
	{
		return m_bValid;
	}

	if ( hWindow == PLAT_WINDOW_INVALID )
	{
		// No IME support possible without it
		Log_Warning( LOG_IME, "IME Error: Invalid Window Handle. No IME support possible.\n" );
		return false;
	}

	m_pIMEManagerWin32 = new GFxIMEManagerWin32( (HWND)hWindow );
	m_bValid = m_pIMEManagerWin32->Init();

	if ( m_bValid )
	{
		m_bIMEEnabled = true;
	}

	return m_bValid;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CIMEManager::SetActiveUIView( IIMEUIView *pUIView, bool bActive )
{
	if ( !m_bValid )
		return;

	if ( !bActive && m_pIMEManagerWin32->GetActiveUIView() == pUIView )
	{
		m_pIMEManagerWin32->SetActiveUIView( NULL );
	}
	else if ( bActive && m_pIMEManagerWin32->GetActiveUIView() != pUIView )
	{
		// activation is going somewhere else, always deactivate the prior thing
		m_pIMEManagerWin32->SetActiveUIView( NULL );

		if ( pUIView && m_pIMEManagerWin32->m_hWnd == (HWND)pUIView->GetAssociatedPlatWindow() )
		{
			// can only set the active view for primary window
			m_pIMEManagerWin32->SetActiveUIView( pUIView );
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
IIMEUIView *CIMEManager::GetActiveUIView()
{
	if ( !m_bValid )
		return NULL;

	return m_pIMEManagerWin32->GetActiveUIView();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CIMEManager::HandleIMEEvent( PlatWindow_t hWindow, uint32 uMsg, uintp wParam, uintp lParam )
{
	if ( !m_bIMEEnabled || !m_bValid )
	{
		// not handled
		return false;
	}

	if ( hWindow == PLAT_WINDOW_INVALID )
	{
		// No IME support possible without it
		return false;
	}

	if ( m_pIMEManagerWin32->m_hWnd != (HWND)hWindow )
	{
		// Lacking a proper method to untangle IME async messages meant for different windows
		// The primary window is the only IME capable window
		return false;
	}

	IMEWin32Event imeWin32Event( IMEWin32Event::IME_ET_DEFAULT, (HWND)hWindow, uMsg, wParam, lParam );
	bool bHandled = ( ( m_pIMEManagerWin32->HandleIMEEvent( m_pIMEManagerWin32->GetActiveUIView(), imeWin32Event ) & IME_EVENT_NODEFAULTACTION ) != 0 );

	return bHandled;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CIMEManager::HandlePreProcessKeyboardEvent( PlatWindow_t hWindow, uint32 uMsg, uintp wParam, uintp lParam )
{
	if ( !m_bIMEEnabled || !m_bValid )
	{
		// not handled
		return false;
	}

	if ( hWindow == PLAT_WINDOW_INVALID )
	{
		// No IME support possible without it
		return false;
	}

	if ( m_pIMEManagerWin32->m_hWnd != (HWND)hWindow )
	{
		// Lacking a proper method to untangle IME async messages meant for different windows
		// The primary window is the only IME capable window
		return false;
	}

	IMEWin32Event imeWin32Event( IMEWin32Event::IME_ET_PREPROCESSKEYBOARD, (HWND)hWindow, uMsg, wParam, lParam );
	bool bHandled = ( ( m_pIMEManagerWin32->HandleIMEEvent( m_pIMEManagerWin32->GetActiveUIView(), imeWin32Event ) & IME_EVENT_NODEFAULTACTION ) != 0 );

	return bHandled;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CIMEManager::HandleMouseDownEvent( IIMEUIView *pUIView, IIMEUIObject *pObjectUnderMouse )
{
	if ( !m_bIMEEnabled || !m_bValid )
		return;

	if ( pUIView && m_pIMEManagerWin32->m_hWnd != (HWND)pUIView->GetAssociatedPlatWindow() )
	{
		// from a non-relevant window
		return;
	}

	m_pIMEManagerWin32->HandleMouseDownEvent( pUIView, pObjectUnderMouse );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CIMEManager::HandleFocusChange( IIMEUIObject *pObject, bool bFocusSet )
{
	if ( !m_bIMEEnabled || !m_bValid )
		return;

	m_pIMEManagerWin32->HandleFocusChange( pObject, bFocusSet );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CIMEManager::IMEInfo_f( const CCommand &args )
{
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CIMEManager::SetIMEEnabled( bool bEnabled )
{
	if ( m_bIMEEnabled != bEnabled )
	{
		Log_Detailed( LOG_IME, "SetIMEEnabled %d\n", bEnabled );

		if( m_pIMEManagerWin32 )
		{
			if( !bEnabled )
			{
				m_pIMEManagerWin32->OnFinalize( true );
			}
			m_pIMEManagerWin32->EnableIME( bEnabled );
		}
	}
	m_bIMEEnabled = bEnabled;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
LoggingChannelID_t CIMEManager::GetLoggingChannel()
{
	return LOG_IME;
}
