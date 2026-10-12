/**************************************************************************

Filename    :   GFx_IMEManagerWin32.cpp
Content     :   Derived from GFxIMEManager- implements Win32 related IMEManager functionality 
Created     :   OCt 4, 2007
Authors     :   Ankur Mohan

Copyright   :   Copyright 2011 Autodesk, Inc. All Rights reserved.

Use of this software is subject to the terms of the Autodesk license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

**************************************************************************/

#include "imemanager.h"

#include "tier0/platform_com.h"

#include "gfx_imemanagerwin32.h"
#include "string.h"
#include "gfx_imewin32impl.h"
#include "gfx_imenamesmanagervista.h"
#include "gfx_imenamesmanagerxp.h"

GFxIMEManagerWin32::GFxIMEManagerWin32( HWND hWnd )
{
	m_hWnd = hWnd; 

    m_pIMENamesMgr = NULL;
    m_pIMEImpl = NULL;

	m_IMETag = GFxIME_NotSet;

	m_pUIView = NULL;

	m_bIMEOpenStatus = false;
	m_bNamesManagerInitStatus = false;

	m_OSVersion = OSVER_UNKNOWN;
	m_nIMEVersionId = 0;
}

bool GFxIMEManagerWin32::Init()
{
	if ( !m_hWnd )
	{
        Log_Warning( LOG_IME, "IME Error: Window Handle to IME is NULL!\n" );
		return false;
	}

	m_pIMENamesMgr = NULL;
    m_pIMEImpl = NULL;
	m_bNamesManagerInitStatus = false;

    m_OSVersion = DetectWindowsVersion(); 

    // Initialize COM
    /*
    Important Note: CoInitialize must be called without COINIT_MULTITHREADED for OnActivated to be called everytime an IME change    
	is made (there could be other side effects of COINIT_MULTITHREADED as well that I don't know of). According to the               
	documentation, Typically, the COM library is initialized on a thread only once. Subsequent calls to CoInitialize or              
	CoInitializeEx on the same thread will succeed, as long as they do not attempt to change the concurrency model, 
    but will return S_FALSE.Now it's possible that some other module in the application calls CoInitialize in the multithreaded
	mode, which will cause this CoInitialize
    invocation to fail and Vista IME to not work. We can't fully remedy this problem, but we can at least check for the error
	value retured by CoInitialize and log an error. 

    Also: 
    In addition, calls made to objects initialized using the COINIT_APARTMENTTHREADED setting arrive only at message-queue           
	boundaries. Because of this serialization, it is not typically necessary to write concurrency control into the 
    code for the object, other than to avoid calls to PeekMessage and SendMessage during processing that must not be interrupted     
	by other method invocations or calls to other objects in the same apartment/thread.

    An important implication of this is that we must make sure that PeekMessage is called often enough, otherwise IME processing     
	will be very slow if a long time is spent in Advance/Display.
    */

    HRESULT hr = Plat_RequireLoadCOM()->pCoInitializeEx( NULL, COINIT_APARTMENTTHREADED ); // CoInitializeEx( NULL, COINIT_MULTITHREADED );
    if ( hr == S_FALSE )
	{
        Log_Detailed( LOG_IME, "The COM library is already initialized on this thread.\n");
	}
    else if ( hr == RPC_E_CHANGED_MODE )
	{
        Log_Detailed( LOG_IME, "A previous call to CoInitializeEx specified the concurrency model for this thread as multithread apartment (MTA).\n"); 
	}

    if ( m_OSVersion == OSVER_WINVISTA || m_OSVersion == OSVER_WIN7 || m_OSVersion == OSVER_WIN8 || m_OSVersion == OSVER_WIN81 || m_OSVersion == OSVER_WIN10 )
    {
		Log_Detailed( LOG_IME, "Setting up using Windows Vista IME.\n");

        m_pIMEImpl = new GFxIMEVista( this, m_hWnd );
        m_pIMENamesMgr = new CIMENamesManagerVista( this );
    }
    else if ( m_OSVersion == OSVER_WINXP )
    {
		Log_Detailed( LOG_IME, "Setting up using Windows XP IME.\n");

        m_pIMEImpl = new GFxIMEXP( this, m_hWnd );
        m_pIMENamesMgr = new CIMENamesManagerXP( this );
    }

    if ( m_pIMENamesMgr )
	{
		m_pIMENamesMgr->PopulateSupportedIMEs();
        m_bNamesManagerInitStatus = m_pIMENamesMgr->QualifyIMENames();
		if ( m_bNamesManagerInitStatus )
		{
			m_bNamesManagerInitStatus = m_pIMENamesMgr->IsAnyIMESupported();
		}
    }
    
    return m_bNamesManagerInitStatus;
}

GFxIMEManagerWin32::~GFxIMEManagerWin32()
{
    if ( m_pIMEImpl )
	{
        delete m_pIMEImpl;
		m_pIMEImpl = NULL;
	}

    if ( m_pIMENamesMgr )
	{
        delete m_pIMENamesMgr;
		m_pIMENamesMgr = NULL;
	}
}

GFxIMEManagerWin32::OSVersion GFxIMEManagerWin32::DetectWindowsVersion()
{
	OSVersion version = OSVER_UNKNOWN;

    OSVERSIONINFO OSVersionInfo;
    OSVersionInfo.dwOSVersionInfoSize = sizeof( OSVERSIONINFO );
    ::GetVersionEx( &OSVersionInfo );

	Log_Detailed( LOG_IME, "Detected Windows Version %d.%d\n", OSVersionInfo.dwMajorVersion, OSVersionInfo.dwMinorVersion );

    switch ( OSVersionInfo.dwPlatformId )
    {
    case VER_PLATFORM_WIN32s:
	case VER_PLATFORM_WIN32_WINDOWS:
        break;

    case VER_PLATFORM_WIN32_NT:
		if ( OSVersionInfo.dwMajorVersion == 5 && OSVersionInfo.dwMinorVersion == 0 )
		{
			version = OSVER_WIN2K;
		}
		else if ( OSVersionInfo.dwMajorVersion == 5 && (OSVersionInfo.dwMinorVersion == 1 || OSVersionInfo.dwMinorVersion == 2) )
		{
			version = OSVER_WINXP;
		}
		else if ( OSVersionInfo.dwMajorVersion == 6 && OSVersionInfo.dwMinorVersion == 0 )
		{
			version = OSVER_WINVISTA;
		}
		else if ( OSVersionInfo.dwMajorVersion == 6 && OSVersionInfo.dwMinorVersion == 1 )
		{
			version = OSVER_WIN7;
		}
		else if ( OSVersionInfo.dwMajorVersion == 6 && OSVersionInfo.dwMinorVersion == 2 )
		{
			version = OSVER_WIN8;
		}
		else if ( OSVersionInfo.dwMajorVersion == 6 && OSVersionInfo.dwMinorVersion == 3 )
		{
			version = OSVER_WIN81;
		}
		else if ( OSVersionInfo.dwMajorVersion == 10 && OSVersionInfo.dwMinorVersion == 0 )
		{
			version = OSVER_WIN10;
		}
        break;
    }

	return version;
}

CUtlString GFxIMEManagerWin32::GetSystemLanguageInfo()
{
	char buf[4];
    if ( GetLocaleInfoA(LOCALE_SYSTEM_DEFAULT, LOCALE_SABBREVLANGNAME, buf, 4) != 0 )
	{
		return CUtlString( buf );
	}
	else
	{
		DWORD err = GetLastError();
		if (err == ERROR_INSUFFICIENT_BUFFER)
		{			
		}
		if (err == ERROR_INVALID_FLAGS)
		{	
		}
		if (err == ERROR_INVALID_PARAMETER)
		{	
		}
	}
	return CUtlString();
}

IMEEventResult GFxIMEManagerWin32::HandleIMEEvent( IIMEUIView *pUIView, const IMEWin32Event &imeWin32Event )
{
	if ( !m_bNamesManagerInitStatus )
	{
		Log_Warning( LOG_IME, "Failure in obtaining list of installed IMEs! IME is disabled.\n" );
		return IME_EVENT_NOTHANDLED; 
	}

	if ( g_bIMEDetailedLogging )
	{
		const char *pHandlerContextName = ( imeWin32Event.m_IMEEventType == IMEEvent::IME_ET_PREPROCESSKEYBOARD ) ? "HandleIMEEvent( Pre-Process )" : "HandleIMEEvent()";
		Log_Detailed( LOG_IME, "%s: hWnd:0x%8.8llx, uMsg:0x%8.8x, wParam:0x%8.8x, lParam:0x%8.8x\n", pHandlerContextName, static_cast<uint64>(reinterpret_cast<uintp>(imeWin32Event.m_hWnd)), (uint32)imeWin32Event.m_nWin32MessageId, (uint32)imeWin32Event.m_wParam, (uint32)imeWin32Event.m_lParam );

		CUtlString messageString;
		switch ( imeWin32Event.m_nWin32MessageId )
		{
		case WM_LBUTTONDOWN:
			messageString = "WM_LBUTTONDOWN";
			break;
		case WM_LBUTTONUP:
			messageString = "WM_LBUTTONUP";
			break;
		case WM_KEYDOWN:
			messageString = "WM_KEYDOWN";
			break;
		case WM_KEYUP:
			messageString = "WM_KEYUP";
			break;
		case WM_CHAR:
			messageString = "WM_CHAR";
			break;
		case WM_DEADCHAR:
			messageString = "WM_DEADCHAR";
			break;
		case WM_SYSKEYDOWN:
			messageString = "WM_SYSKEYDOWN";
			break;
		case WM_SYSKEYUP:
			messageString = "WM_SYSKEYUP";
			break;
		case WM_SYSCHAR:
			messageString = "WM_SYSCHAR";
			break;
		case WM_SYSDEADCHAR:
			messageString = "WM_SYSDEADCHAR";
			break;
		case WM_UNICHAR:
			messageString = "WM_UNICHAR";
			break;

		case WM_INPUTLANGCHANGE:
			messageString = "WM_INPUTLANGCHANGE";
			break;
		case WM_IME_STARTCOMPOSITION:
			messageString = "WM_IME_STARTCOMPOSITION";
			break;
		case WM_IME_COMPOSITION:
			messageString = "WM_IME_COMPOSITION";
			break;
		case WM_IME_ENDCOMPOSITION:
			messageString = "WM_IME_ENDCOMPOSITION";
			break;
		case WM_IME_NOTIFY:
			messageString = "WM_IME_NOTIFY";
			break;
		case WM_IME_SETCONTEXT:
			messageString = "WM_IME_SETCONTEXT";
			break;
		case WM_IME_CONTROL:
			messageString = "WM_IME_CONTROL";
			break;
		case WM_IME_COMPOSITIONFULL:
			messageString = "WM_IME_COMPOSITIONFULL";
			break;
		case WM_IME_SELECT:
			messageString = "WM_IME_SELECT";
			break;
		case WM_IME_KEYDOWN:
			messageString = "WM_IME_KEYDOWN";
			break;
		case WM_IME_KEYUP:
			messageString = "WM_IME_KEYUP";
			break;
		case WM_IME_CHAR:
			messageString = "WM_IME_CHAR";
			break;
		default:
			messageString.Format( "Unknown IME message" );
		}

		CUtlString subMessageString;
		if ( imeWin32Event.m_nWin32MessageId == WM_IME_NOTIFY )
		{
			switch ( imeWin32Event.m_wParam )
			{
				case IMN_CLOSESTATUSWINDOW:
					subMessageString = "IMN_CLOSESTATUSWINDOW";
					break;
				case IMN_OPENSTATUSWINDOW:
					subMessageString = "IMN_OPENSTATUSWINDOW";
					break;
				case IMN_CHANGECANDIDATE:
					subMessageString = "IMN_CHANGECANDIDATE";
					break;
				case IMN_CLOSECANDIDATE:
					subMessageString = "IMN_CLOSECANDIDATE";
					break;
				case IMN_OPENCANDIDATE:
					subMessageString = "IMN_OPENCANDIDATE";
					break;
				case IMN_SETCONVERSIONMODE:
					subMessageString = "IMN_SETCONVERSIONMODE";
					break;
				case IMN_SETSENTENCEMODE:
					subMessageString = "IMN_SETSENTENCEMODE";
					break;
				case IMN_SETOPENSTATUS:
					subMessageString = "IMN_SETOPENSTATUS";
					break;
				case IMN_SETCANDIDATEPOS:
					subMessageString = "IMN_SETCANDIDATEPOS";
					break;
				case IMN_SETCOMPOSITIONFONT:
					subMessageString = "IMN_SETCOMPOSITIONFONT";
					break;
				case IMN_SETCOMPOSITIONWINDOW:
					subMessageString = "IMN_SETCOMPOSITIONWINDOW";
					break;
				case IMN_SETSTATUSWINDOWPOS:
					subMessageString = "IMN_SETSTATUSWINDOWPOS";
					break;
				case IMN_GUIDELINE:
					subMessageString = "IMN_GUIDELINE";
					break;
				case IMN_PRIVATE:
					subMessageString = "IMN_PRIVATE";
					break;
				default:
					subMessageString.Format( "Unknown IMN_??? message" );
			}
		}
		Log_Detailed( LOG_IME, Color( 255, 200, 255 ), "   %s: %s\n", messageString.Get(), subMessageString.Get() );
	}

    if ( !IsUIViewActive( pUIView ) )
	{
        return IME_EVENT_NOTHANDLED;
	}

    // First check if this IME is supported
	// Also sets the IMETag member of GFxIMEWin32Impl.
	SetIMETag( m_pIMENamesMgr->GetIMETag() ); 

    // Reposition the default IME windows appropriately. See comment in OnIMEStart.
    if ( imeWin32Event.m_nWin32MessageId == WM_IME_STARTCOMPOSITION )
    {
        return m_pIMEImpl->OnIMEStartComposition( imeWin32Event.m_wParam, imeWin32Event.m_lParam, imeWin32Event.m_nOptions != 0 );
    }

    if ( imeWin32Event.m_nWin32MessageId == WM_IME_SETCONTEXT )
	{
        return IME_EVENT_NODEFAULTACTION;
	}

    // Language bar and Status window notifications should be handled even if we are not in a textfield
    // to keep the state of the language bar consistent with that of the system.
    if ( imeWin32Event.m_nWin32MessageId == WM_INPUTLANGCHANGE )
    {
        // Note that we can't use GetKeyboardLayout to get the name of the input locale
        // since it returns the hex value of the locale as a string. For example: "0x0000409"
        // GetKeyboardLayoutNameA(langName);
        m_pIMENamesMgr->OnInputLangChange( (uint32)(imeWin32Event.m_lParam) );

        // This is important because for JJ Pinyin IME, when the input language changes,
        // finalize messages are not sent and therefore the previous ime UI elements can
        // still be present when a new input language is activated. 
		m_pIMEImpl->Finalize();
    }

    if ( imeWin32Event.m_nWin32MessageId == WM_IME_NOTIFY && imeWin32Event.m_wParam == IMN_SETCONVERSIONMODE )
    {
        m_pIMENamesMgr->SetConversionMode();
    }
		
	if ( !m_pIMEImpl->IsTextFieldFocused( pUIView ) )
	{
		return IME_EVENT_NOTHANDLED;
	}

    if ( imeWin32Event.m_IMEEventType == IMEEvent::IME_ET_PREPROCESSKEYBOARD )
    {
        if ( m_IMETag != GFxIME_NotSupported )
        {
            bool bIsIMEUIMsg = ( ImmIsUIMessage( NULL, imeWin32Event.m_nWin32MessageId, imeWin32Event.m_wParam, imeWin32Event.m_lParam ) == 0 );
            if ( bIsIMEUIMsg || imeWin32Event.m_nWin32MessageId == WM_KEYDOWN || imeWin32Event.m_nWin32MessageId == WM_KEYUP || imeWin32Event.m_nWin32MessageId == WM_LBUTTONUP )
			{ 
                PreProcessHandler( imeWin32Event );
			}

            return IME_EVENT_NODEFAULTACTION;
        }
    }
    else
    {
        if ( m_IMETag == GFxIME_NotSupported )
		{
            return IME_EVENT_NOTHANDLED;
		}

        switch ( imeWin32Event.m_nWin32MessageId )
        {
        case WM_IME_ENDCOMPOSITION:
            m_pIMEImpl->OnIMEEndComposition( imeWin32Event.m_wParam, imeWin32Event.m_lParam, imeWin32Event.m_nOptions != 0 );
            break;

        case WM_IME_COMPOSITION:
            m_pIMEImpl->OnIMEComposition( imeWin32Event.m_wParam, imeWin32Event.m_lParam, imeWin32Event.m_nOptions );
            return IME_EVENT_NODEFAULTACTION; 

        case WM_IME_NOTIFY:
            if ( imeWin32Event.m_wParam == IMN_CHANGECANDIDATE || imeWin32Event.m_wParam == IMN_OPENCANDIDATE || imeWin32Event.m_wParam == IMN_CLOSECANDIDATE || imeWin32Event.m_wParam == IMN_PRIVATE )
            {
                if ( m_pIMEImpl->OnIMENotify(WM_IME_NOTIFY, imeWin32Event.m_wParam, imeWin32Event.m_lParam, 1) )
				{
                    return IME_EVENT_NODEFAULTACTION;
				}
                else
				{
                    return IME_EVENT_NOTHANDLED;
				}
            }
            else if (imeWin32Event.m_wParam == IMN_SETCONVERSIONMODE)
            {
                m_pIMENamesMgr->SetConversionMode();
            }
            else if (imeWin32Event.m_wParam == IMN_SETOPENSTATUS)
            {
                HIMC hIMC = ImmGetContext( HWND( imeWin32Event.m_hWnd ) );
                if (ImmGetOpenStatus(hIMC) == 0)
                {
                    m_bIMEOpenStatus = false;
                    m_pIMEImpl->SetOpenStatus(false);
                }
                else
                {
                    m_bIMEOpenStatus = true;
                    m_pIMEImpl->SetOpenStatus(true);
                }
                    
                // If Japanese IME is being used and the user switches back and forth from direct input to 
                // hiragana, katakana etc, SetConversionMode message is not sent, since switching to Direct 
                // Input is considered turning the IME on/off. Handle that case here.                
				if ( !wcsicmp( m_CurrentLanguage.Get(), L"Japanese" ) )
				{
                    m_pIMENamesMgr->SetConversionMode();
				}
                ImmReleaseContext( HWND( imeWin32Event.m_hWnd ), hIMC);
            }
            else if ( imeWin32Event.m_wParam == IMN_OPENSTATUSWINDOW )
            {
            }
            else if ( imeWin32Event.m_wParam == IMN_CLOSESTATUSWINDOW )
            {  
            }
            else if ( imeWin32Event.m_wParam == IMN_SETSTATUSWINDOWPOS )
            {    
            }

            m_pIMEImpl->CustomProcessing( WM_IME_NOTIFY, imeWin32Event.m_wParam, imeWin32Event.m_lParam );
            break;

        case WM_IME_CHAR:
            return IME_EVENT_NODEFAULTACTION;

        default:
            return IME_EVENT_NOTHANDLED;
        }
    }
   
    return IME_EVENT_HANDLED;
}

void GFxIMEManagerWin32::SetCurrentInputLanguage( GFxIMETag imeTag )
{
    // IMETags are used as an intermediary between the input language strings obtained from 
    // the system and generic strings used in this class. For example, Chinese (Traditional) 
    // is called Chinese (Traditional) on XP and Chinese (Taiwan) on Vista. The GFxIMENamesManagerVista
    // and GFxIMENamesManagerXP classes handle the system specific details and produce the same IMETag 
    // (GFxIME_Ch_Trad in this case). 
    // This function just maps the IMETags to generic language ename strings that are system independent.

    m_CurrentLanguage = L"Unknown";

    if ( imeTag & GFxIME_En_Flag )
    { 
       m_CurrentLanguage = L"English";
    } 

    if ( imeTag & GFxIME_Jp_Flag )
    { 
        m_CurrentLanguage = L"Japanese";
    }  

    if ( imeTag & GFxIME_Kr_Flag )
    { 
         m_CurrentLanguage = L"Korean";
    } 

    // Currently third party ime's are in the same category as simplified ime's
    if ( ( imeTag & GFxIME_Ch_Simp_Flag ) || ( imeTag & GFxIME_Ch_ThirdParty_Flag ) )
    { 
        m_CurrentLanguage = L"Chinese (Simplified)";
    } 

    // Currently third party ime's are in the same category as simplified ime's
    if ( imeTag & GFxIME_Ch_Trad_Flag )
    { 
         m_CurrentLanguage = L"Chinese (Traditional)";
    } 

    BroadcastSwitchLanguage( m_CurrentLanguage.Get() );
}

void GFxIMEManagerWin32::SetIMETag( GFxIMETag imeTag )
{
	m_IMETag = imeTag; 
	m_pIMEImpl->SetIMETag( m_IMETag );
}

void GFxIMEManagerWin32::SetIMEVersionId( int nVersionId ) 
{
    m_nIMEVersionId = nVersionId; 
    m_pIMEImpl->SetIMEVersionId( m_nIMEVersionId );
}

const char *GFxIMEManagerWin32::GetInputLanguage()
{
    if ( m_pIMEImpl )
	{
        return( m_pIMEImpl->GetInputLanguage() );
	}

    return "UNKNOWN";
}

LRESULT GFxIMEManagerWin32::OnIMENotify( uint32 message, uintp wParam, uintp lParam, bool bDownFlag )
{
	LRESULT res = FALSE;
	if ( m_pIMEImpl )
	{
		res = m_pIMEImpl->OnIMENotify( message, wParam, lParam, bDownFlag );
	}
    return res;
}

void GFxIMEManagerWin32::CustomProcessing( uint32 message, uintp wParam, uintp lParam )
{
    m_pIMEImpl->CustomProcessing( message, wParam, lParam );
}

void GFxIMEManagerWin32::OnFinalize( bool bCancel )
{
	if ( m_pIMEImpl )
	{
		m_pIMEImpl->Finalize( bCancel );
	}
}

void GFxIMEManagerWin32::OnShutdown()
{
	if ( m_pIMEImpl )
	{
		m_pIMEImpl->Shutdown();
	}
}

void GFxIMEManagerWin32::SelectAndClose(int index)
{
	if ( m_pIMEImpl )
	{
		m_pIMEImpl->SelectAndClose(index);
	}
}

void GFxIMEManagerWin32::PreProcessHandler(const IMEWin32Event& winEvt)
{
	if ( m_pIMEImpl )
	{
		m_pIMEImpl->PreProcessHandler(winEvt);
	}
}

void GFxIMEManagerWin32::DisplayReadingWindow(const uchar32* pReadingStr)
{
	if ( m_pIMEImpl )
	{
		m_pIMEImpl->DisplayReadingWindow(pReadingStr);
	}
}

void GFxIMEManagerWin32::DisplayCompositionString(uint32& numCharDisplay)
{
	if ( m_pIMEImpl )
	{
		m_pIMEImpl->DisplayCompositionString(numCharDisplay);
	}
}

bool GFxIMEManagerWin32::SetConversionMode(const uint32 convMode)
{
	if ( m_pIMEImpl )
	{
		return(m_pIMEImpl->SetConversionMode(convMode));
	}

    return false;
}

const char *GFxIMEManagerWin32::GetConversionMode()
{
	if ( m_pIMEImpl )
	{
		return m_pIMEImpl->GetConversionMode();
	}

    return "UNKNOWN";
}

bool GFxIMEManagerWin32::SetEnabled(bool enable)
{
	if ( m_pIMEImpl )
	{
		return m_pIMEImpl->SetEnabled(enable);
	}

    return false;
}

// Retrieves IME state. 
bool GFxIMEManagerWin32::GetEnabled() 
{ 
	if ( m_pIMEImpl )
	{
		return m_pIMEImpl->GetEnabled();
	}

    return false;
} 

bool GFxIMEManagerWin32::SetCompositionString( const wchar_t *pCompString )
{
	if ( m_pIMEImpl )
	{
		return m_pIMEImpl->SetCompositionString(pCompString);
	}

    return false;
}

void GFxIMEManagerWin32::SetActiveUIView( IIMEUIView *pUIView )
{
    IMEManagerBase::SetActiveUIView( pUIView );
}

bool GFxIMEManagerWin32::CheckIfInTextField()
{
	if ( m_pIMEImpl )
	{
		return m_pIMEImpl->CheckIfInTextField();
	}
	return true;
}

void GFxIMEManagerWin32::OnEnableIME(bool bEnable)
{
    if ( m_pIMEImpl )
    {
        m_pIMEImpl->OnEnableIME( bEnable );
    }

    // Since ChangeConversion message is not sent in this case.
	if ( !wcsicmp( m_CurrentLanguage.Get(), L"Japanese" ) )
	{
        m_pIMENamesMgr->SetConversionMode();
	}
}



