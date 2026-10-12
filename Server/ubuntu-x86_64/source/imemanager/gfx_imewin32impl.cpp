#include "imemanager.h"

#include "gfx_imewin32impl.h"

GFxIMEWin32Impl::GFxIMEWin32Impl( IMEManagerBase *pIMEManagerBase, HWND hWnd ) : m_pIMEManagerBase( pIMEManagerBase ), m_hWnd( hWnd )
{
    m_pCandidateListBox = new CandidateListBox( this );

    m_bGlobalIMEState = true;
}
    
GFxIMEWin32Impl::~GFxIMEWin32Impl()
{
    m_pCandidateListBox->RemoveAllListItems();

    delete m_pCandidateListBox;
	m_pCandidateListBox = NULL;
}

// Checks if cursor is in a textfield. For any other element, IME messages/TSF notifications are discarded
bool GFxIMEWin32Impl::IsTextFieldFocused( IIMEUIView *pUIView )
{
	if ( !pUIView )
	{
		pUIView = m_pIMEManagerBase->GetActiveUIView();
	}

	if ( pUIView )
	{
		IIMEUIObject *pFocusedObject = pUIView->GetFocusedObject();
		if ( pFocusedObject && pFocusedObject->IME_GetObjectType() == UIOT_TEXTFIELD )
		{
			IIMEUITextField *pTextField = static_cast< IIMEUITextField* >( pFocusedObject );
			return pTextField->IME_IsEnabled();
		}
	}

	return false;
}

void GFxIMEWin32Impl::HandleStatusWindowNotifications(const char* pcommand, const char* parg)
{
	NOTE_UNUSED2( pcommand, parg ); 
}

// Handles WM_IMECOMPOSITION
void GFxIMEWin32Impl::OnIMEComposition( uintp wParam, uintp lParam, int nOptions )
{
	NOTE_UNUSED3( wParam, lParam, nOptions );
}

// Handles WM_IMESTARTCOMPOSITION
IMEEventResult GFxIMEWin32Impl::OnIMEStartComposition( uintp wParam, uintp lParam, bool bDownFlag )
{
	NOTE_UNUSED3( wParam, lParam, bDownFlag );
	return IME_EVENT_NOTHANDLED; 
}

// Handles WM_IMEENDCOMPOSITION
void GFxIMEWin32Impl::OnIMEEndComposition( uintp wParam, uintp lParam, bool bDownFlag )
{
	NOTE_UNUSED3( wParam, lParam, bDownFlag );
}

// Handles WM_IME_NOTIFY for candidate list related notifications
LRESULT GFxIMEWin32Impl::OnIMENotify( uint32 message, uintp wParam, uintp lParam, bool bDownFlag )
{
    NOTE_UNUSED4( message, wParam, lParam, bDownFlag );
    return 0;
}

// Handles some WM_NOTIFY messages that get sent when backspace, space, esc keys etc are pushed.
void GFxIMEWin32Impl::CustomProcessing( uint32 message, uintp wParam, uintp lParam )
{
	NOTE_UNUSED3( message, wParam, lParam ); 
}
   
// Finalizes IME composition text, closes any pop ups (candidate list, reading window)
void GFxIMEWin32Impl::Finalize( bool bCancel )
{
}

void GFxIMEWin32Impl::Cleanup()  
{
    HIMC     hIMC;

    hIMC = ImmGetContext(m_hWnd);
    ImmNotifyIME(hIMC, NI_COMPOSITIONSTR, CPS_CANCEL, 0);
    ImmNotifyIME(hIMC, NI_COMPOSITIONSTR, CPS_COMPLETE, 0);
    ImmNotifyIME(hIMC, NI_COMPOSITIONSTR, CPS_REVERT, 0);
    ImmReleaseContext(m_hWnd, hIMC);

    // Clear Reading window contents and remove the reading window
	m_ReadingTextBuffer[0] = 0;
}

void GFxIMEWin32Impl::SetIMETag( GFxIMETag tag )
{ 
	m_IMETag = tag; 
}

void GFxIMEWin32Impl::SetIMEVersionId( int nIMEVersionId )
{
	m_nIMEVersionId = nIMEVersionId;
}

void GFxIMEWin32Impl::Shutdown()
{
    Cleanup();
}

// Selects a particular row of the Candidate List (specified by index). Doesn't finalize
void GFxIMEWin32Impl::SelectAndClose(int index)
{ 
	NOTE_UNUSED(index);
}

void GFxIMEWin32Impl::FsCallBack( const char* pcommand, const char* parg)
{
	NOTE_UNUSED2( pcommand, parg ); 
}
    
// Records the last keystroke for reading window display
void GFxIMEWin32Impl::PreProcessHandler(const IMEWin32Event& winEvt)
{
	NOTE_UNUSED(winEvt);
}

// Takes care of interacting with AS to display reading (input) window
void GFxIMEWin32Impl::DisplayReadingWindow(const uchar32* pReadingStr)
{
	NOTE_UNUSED(pReadingStr);
}
    
// Displays composition string, keeps track of how many characters are on the screen, cursor position etc
void GFxIMEWin32Impl::DisplayCompositionString(uint32& numCharDisplay)
{
	NOTE_UNUSED(numCharDisplay);
}

// Checks if we are in a textfield or not. If not, translate message is not called. 
// This is to make TAB work correctly when IME is active and focus is on a non textfield object.
bool GFxIMEWin32Impl::CheckIfInTextField()
{
	return IsTextFieldFocused();
}
    
void GFxIMEWin32Impl::SetOpenStatus( bool bStatus )
{
    m_bGlobalIMEState = bStatus;
}

// This is used to turn IME on/off when the user clicks to an editable field (ime = on) or
// a non-editable field (ime = off). In addition, user can also SetEnabled AS function to
// turn IME on/off. 
void GFxIMEWin32Impl::OnEnableIME( bool bEnable )
{
    // Note that we can't use ImmSetOpenStatus here to enable/disable IME, because
    // one should never be able to enable IME on a textfield on which imeDisable flag
    // has been set or on a non-editable object. Associating the Null context is a 
    // much more powerful way of disabling IME. 
    ImmAssociateContextEx( m_hWnd, NULL, bEnable ? IACE_DEFAULT : 0 );
}

const char *GFxIMEWin32Impl::GetInputLanguage()
{
    HKL currLocale = GetKeyboardLayout(0); 
    Un.hkl = currLocale;
        
    switch ( Un.val & 0x0000FFFF )
    {                    
    case 0x0411:
        return "Japanese";

    case 0x0412:
        return "Korean";

    case 0x0404:
    case 0x0804:
        return "Chinese";

    case 0x0409: // English US
    case 0x0809: // English UK
    case 0x0c09: // English Aus
    case 0x1009: // English Canada
        return "English";

    case 0x0419:
        return "Russian";

    default:
        return "Unknown";
    }
}

bool GFxIMEWin32Impl::SetEnabled(bool enabled)
{
    bool res;
    bool isIMEEnabled = true;

    HIMC hIMC = ImmGetContext(m_hWnd);
    if (hIMC == NULL)
    {
        isIMEEnabled = false;
        // IME might have been disabled on this object- enable it temporarily.
        ImmAssociateContextEx(m_hWnd, NULL,  IACE_DEFAULT);
        // Get context again
        hIMC = ImmGetContext(m_hWnd);
        // if hIMC is still NULL, return
        if (hIMC == NULL)
        {
            ImmReleaseContext(m_hWnd, hIMC);
            return false;
        }
    }

    res = (ImmSetOpenStatus(hIMC, enabled) == 0)? false: true;
        
    if (isIMEEnabled == false)
    {
        // Since IME was disabled originally, we should disable it again.
        ImmAssociateContextEx(m_hWnd, NULL,  0);
    }

    ImmReleaseContext(m_hWnd, hIMC);
    // Remember this state
    m_bGlobalIMEState = enabled;
    return res;
}

bool GFxIMEWin32Impl::GetEnabled()
{
    bool enabled        = false;
    bool isIMEEnabled   = true;

    HIMC hIMC = ImmGetContext(m_hWnd);
    if(hIMC == NULL)
    {
        isIMEEnabled = false;
        // IME might have been disabled on this object- enable it temporarily.
        ImmAssociateContextEx(m_hWnd, NULL,  IACE_DEFAULT);
        // Get context again
        hIMC = ImmGetContext(m_hWnd);
        // if hIMC is still NULL, return
        if (hIMC == NULL)
        {
            ImmReleaseContext(m_hWnd, hIMC);
            return false;
        }
    }
    enabled = ImmGetOpenStatus(hIMC) == 0? false: true;

    if (isIMEEnabled == false)
    {
        // Since IME was disabled originally, we should disable it again.
        ImmAssociateContextEx(m_hWnd, NULL,  0);
    }

    ImmReleaseContext(m_hWnd, hIMC);

    return enabled;
}

bool GFxIMEWin32Impl::SetCompositionString( const wchar_t *pCompString )
{
    HIMC hIMC = ImmGetContext( m_hWnd );        
    if (hIMC)
    {
		 uint32 wlen = V_wcslen( pCompString );

        bool bResult = ImmSetCompositionString( hIMC, SCS_SETSTR, (LPVOID)pCompString, 2*wlen, 0, 0 ) != 0;
        ImmReleaseContext( m_hWnd, hIMC );
        return bResult;
    }

    return false;
}

bool GFxIMEWin32Impl::SetConversionMode(const uint32 conversionMode)
{
    // First check what input language is currently active.
    HKL currLocale  = GetKeyboardLayout(0); 
    Un.hkl          = currLocale;

    HIMC hIMC = ImmGetContext(m_hWnd);
    bool isIMEEnabled = true;
    if(hIMC == NULL)
    {
        isIMEEnabled = false;
        // IME might have been disabled on this object- enable it temporarily.
        ImmAssociateContextEx(m_hWnd, NULL,  IACE_DEFAULT);
        // Get context again
        hIMC = ImmGetContext(m_hWnd);
        // if hIMC is still NULL, return
        if (hIMC == NULL)
        {
            ImmReleaseContext(m_hWnd, hIMC);
            return false;
        }
    }

    DWORD   fdwConversion;
    DWORD   fdwSentence;
    bool    res = true;

    ImmGetConversionStatus(hIMC, &fdwConversion, &fdwSentence);
        
    if((fdwConversion & 0x0000000F) == IME_CMODE_ALPHANUMERIC)
    {
        // for Direct Input mode, set the status of the IME to be open.
        // This is important because without this, we would not be able to change 
        // status from DirectInput mode (where the status of the IME is closed) to other 
        // modes.
        ImmSetOpenStatus(hIMC, true); 
    }
        
    // Zero out the last 4 bits
    fdwConversion = fdwConversion & 0xFFFFFFF0;

    // Japanese Conversion Modes
    if((Un.val&0x0000FFFF) == 0x00000411)
    {
        switch (conversionMode)
        {
        case 0x00:  // ALPHANUMERIC_FULL
            fdwConversion |= IME_CMODE_FULLSHAPE;
            break;
        case 0x01:  // ALPHANUMERIC_HALF
            fdwConversion |= IME_CMODE_ALPHANUMERIC;
            break;
        case 0x04:  // JAPANESE_HIRAGANA
            fdwConversion |= IME_CMODE_FULLSHAPE | IME_CMODE_NATIVE;
            break;
        case 0x08:  // JAPANESE_KATAKANA_FULL
            fdwConversion |= IME_CMODE_FULLSHAPE | IME_CMODE_LANGUAGE;
            break;
        case 0x016: // JAPANESE_KATAKANA_HALF
            fdwConversion |= IME_CMODE_LANGUAGE;
            break;
        default:
            res = false; 
        }
    }
       
    // Korean Conversion Modes
    if ((Un.val&0x0000FFFF) == 0x00000412)
    {
        if (conversionMode == 0x032)  // KOREAN
        {
            fdwConversion |= IME_CMODE_FULLSHAPE | IME_CMODE_NATIVE;
        }

        else if (conversionMode == 0x00)
        {
            fdwConversion |= IME_CMODE_FULLSHAPE;
        }

        else if (conversionMode == 0x01)
        {
            fdwConversion |= IME_CMODE_ALPHANUMERIC;
        }
        else
        {
            res = false;
        }
    }

    // CHINESE Conversion Modes
    if ((Un.val&0x0000FFFF) == 0x00000404 || (Un.val&0x0000FFFF) == 0x00000804)
    {
        if (conversionMode == 0x02)  // CHINESE
        {
            fdwConversion |= IME_CMODE_FULLSHAPE | IME_CMODE_NATIVE;
        }

        else if (conversionMode == 0x00)
        {
            fdwConversion |= IME_CMODE_FULLSHAPE;
        }

        else if (conversionMode == 0x01)
        {
            fdwConversion |= IME_CMODE_ALPHANUMERIC;
        }

        else
        {
            res = false;
        }
    }

    // We can't rely on the return value of ImmsetConversionStatus since it always returns true no 
    // matter what the fdwconversion is. Hence, we have to disambiguate between valid/invalid
    // conversion mode values depending on the current language as done above. 
    if(res)
    {
    //    bool result = (ImmSetConversionStatus(hIMC, fdwConversion, fdwSentence) == 0)? false: true;
        ImmSetConversionStatus(hIMC, fdwConversion, fdwSentence);
        // Reset global IME state
        m_bGlobalIMEState = true;
        ImmSetOpenStatus(hIMC, m_bGlobalIMEState);
    }

    ImmReleaseContext(m_hWnd, hIMC);

    if (isIMEEnabled == false)
    {
        // Since IME was disabled originally, we should disable it again.
        ImmAssociateContextEx(m_hWnd, NULL,  0);
    } 
    return res;
}

// Retrieves conversion mode. 
const char *GFxIMEWin32Impl::GetConversionMode()
{
    DWORD fdwConversion;
    DWORD fdwSentence;
       
    HIMC hIMC = ImmGetContext(m_hWnd);
    bool isIMEEnabled = true;

    if(hIMC == NULL)
    {
        isIMEEnabled = false;
        // IME might have been disabled on this object- enable it temporarily.
        ImmAssociateContextEx(m_hWnd, NULL,  IACE_DEFAULT);
        // Get context again
        hIMC = ImmGetContext(m_hWnd);
        // if hIMC is still NULL, return
        if (hIMC == NULL)
        {
            ImmReleaseContext(m_hWnd, hIMC);
            return "UNKNOWN";
        }
    }
    ImmGetConversionStatus(hIMC, &fdwConversion, &fdwSentence);

    // fdwConversion values have to be interpreted differently depending upon the 
    // language currently in use.

    m_ConversionMode = "";

    // Zero out all but last 4 bits of fdwConversion
    fdwConversion = fdwConversion & 0x0000000F;
    // Japanese Conversion modes
    if ( m_IMETag & GFxIME_Jp_Flag )
    {
        if ((fdwConversion & (~IME_CMODE_FULLSHAPE)) == IME_CMODE_NATIVE)
        {
            m_ConversionMode = "JAPANESE_HIRAGANA";
        }

        if (fdwConversion == (IME_CMODE_FULLSHAPE | IME_CMODE_LANGUAGE)) 
        {
            m_ConversionMode = "JAPANESE_KATAKANA_FULL";
        }

        if (fdwConversion == IME_CMODE_LANGUAGE) 
        {
            m_ConversionMode = "JAPANESE_KATAKANA_HALF";   
        }

        if (fdwConversion == IME_CMODE_FULLSHAPE) 
        {
            m_ConversionMode = "ALPHANUMERIC_FULL";
        }

        if (fdwConversion == IME_CMODE_ALPHANUMERIC) 
        {
            m_ConversionMode = "ALPHANUMERIC_HALF";
        }
    }

    // Korean Conversion Modes
    if (m_IMETag & GFxIME_Kr_Flag)
    {
        if ((fdwConversion & (~IME_CMODE_FULLSHAPE)) == IME_CMODE_NATIVE)
        {
            m_ConversionMode = "KOREAN";
        }

        if (fdwConversion == IME_CMODE_FULLSHAPE)
        {
            m_ConversionMode = "ALPHANUMERIC_FULL";
        }
        if (fdwConversion == IME_CMODE_ALPHANUMERIC)
        {
            m_ConversionMode = "ALPHANUMERIC_HALF";
        }
    }

    // Chinese Conversion Modes
    if ((m_IMETag & GFxIME_Ch_Simp_Flag) || (m_IMETag & GFxIME_Ch_Trad_Flag))
    {
        if ((fdwConversion & (~IME_CMODE_FULLSHAPE)) == IME_CMODE_NATIVE)
        {
            m_ConversionMode = "CHINESE";
        }

        if (fdwConversion == IME_CMODE_FULLSHAPE)
        {
            m_ConversionMode = "ALPHANUMERIC_FULL";
        }

        if (fdwConversion == IME_CMODE_ALPHANUMERIC)
        {
            m_ConversionMode = "ALPHANUMERIC_HALF";
        }
    }

    if (isIMEEnabled == false)
    {
        // Since IME was disabled originally, we should disable it again.
        ImmAssociateContextEx(m_hWnd, NULL,  0);
    }  
        
    ImmReleaseContext(m_hWnd, hIMC);

    if ( m_ConversionMode.IsEmpty() )
	{
        return "UNKNOWN";
	}
 
	return m_ConversionMode.Get();
}

void GFxIMEWin32Impl::Invoke_RemoveInputWindow()
{
	m_pIMEManagerBase->Invoke_RemoveInputWindow();
}

void GFxIMEWin32Impl::Invoke_DisplayInputWindow( const uchar32 *pReadingString, const IMERectF *pPosition )
{
	m_pIMEManagerBase->Invoke_DisplayInputWindow( pReadingString, pPosition );
}

void GFxIMEWin32Impl::Invoke_RepositionInputWindow( const IMERectF *pPosition )
{
	m_pIMEManagerBase->Invoke_RepositionInputWindow( pPosition );
}

void GFxIMEWin32Impl::Invoke_CreateList( int nPageSize, int nListStartsAt1 )
{
	m_pIMEManagerBase->Invoke_CreateList( nPageSize, nListStartsAt1 );
}

void GFxIMEWin32Impl::Invoke_RemoveList()
{
	m_pIMEManagerBase->Invoke_RemoveList();
}

void GFxIMEWin32Impl::Invoke_ClearList()
{
	m_pIMEManagerBase->Invoke_ClearList();
}

void GFxIMEWin32Impl::Invoke_ShowList( bool bShow )
{
	m_pIMEManagerBase->Invoke_ShowList( bShow );
}

void GFxIMEWin32Impl::Invoke_RepositionCandidateList( const IMERectF *pPosition )
{
	m_pIMEManagerBase->Invoke_RepositionCandidateList( pPosition );
}

void GFxIMEWin32Impl::Invoke_SelectItemInList( int32 nItemToSelect )
{
	m_pIMEManagerBase->Invoke_SelectItemInList( nItemToSelect );
}

void GFxIMEWin32Impl::Invoke_AddToList( const wchar_t *pCandidateString )
{
	m_pIMEManagerBase->Invoke_AddToList( pCandidateString );
}
