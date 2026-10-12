
/**************************************************************************

Filename    :   GFx_IMEVista.cpp
Content     :   Implementation of Input Method Support on Windows Vista using TSF 
Created     :   OCt 4, 2007
Authors     :   Ankur Mohan

Copyright   :   Copyright 2011 Autodesk, Inc. All Rights reserved.

Use of this software is subject to the terms of the Autodesk license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

**************************************************************************/

#include "vstdlib/vstrtools.h"

#include "imemanager.h"

#include "gfx_imewin32impl.h"
#include "gfx_imetsf.h"
#include "gfx_imemanagerwin32.h"

typedef struct tagLAYOUTORTIPPROFILE 
{
    DWORD  dwProfileType;       // InputProcessor or HKL
#define LOTP_INPUTPROCESSOR 1
#define LOTP_KEYBOARDLAYOUT 2
    LANGID langid;              // language id
    CLSID  clsid;               // CLSID of tip
    GUID   guidProfile;         // profile description
    GUID   catid;               // category of tip
    DWORD  dwSubstituteLayout;  // substitute hkl
    DWORD  dwFlags;             // Flags
    WCHAR  szId[MAX_PATH];      // KLID or TIP profile for string
} LAYOUTORTIPPROFILE;

typedef uint32 (WINAPI *PTF_ENUMENABLEDLAYOUTORTIP)(LPCWSTR pszUserSidString, LAYOUTORTIPPROFILE *pLayoutOrTipProfile, uint32 uBufLength);

GFxIMEVista::GFxIMEVista( IMEManagerBase *pIMEManagerBase, HWND hWnd ) : GFxIMEWin32Impl( pIMEManagerBase, hWnd )
{
    m_CurrLocale = NULL;
    m_nNumCharDisplay = 0;
	m_bReplaceChar = false;

    m_ReadingTextBuffer.EnsureCount( ReadingTextBufferSize );
    m_ReadingTextBuffer[0] = 0;

    m_nClausePosition = 0;
    m_nCandListStartFrom1 = 1;

	m_bStartCompositionReceived = false;
	m_bDrawCandidateList = false;
	m_nCandidateWindowRefCount = 0;
	m_bNeedToDisableIME = false;

    m_pTSF = new CTSF( this );
    if ( !m_pTSF->Init() )
	{
		Log_Warning( LOG_IME, "TSF Init() Failure!\n");
	}
}

GFxIMEVista::~GFxIMEVista()
{
	if ( m_pTSF )
	{
		m_pTSF->Shutdown();
		m_pTSF = NULL;
	}
}

void GFxIMEVista::FsCallBack( IIMEUIView *pUIView, const char *pCommand, const char *pArg )
{
}

void GFxIMEVista::PreProcessHandler(const IMEWin32Event& winEvt)
{
	// Special processing for the shift key:
	/*
	Here's why this is needed. The shift key can be used to toggle between english input and ime enabled input. 
	On Vista, when the shift key is pushed, a WM_KEYDOWN with VK_SHIFT is sent, followed by a bunch of IME notify messages
	to change the input mode. The Virtual code for the corresponding WM_KEYUP message however, is not VK_SHIFT but 
	VK_PROCESSKEY. This is a problem because GFx relies on the correct virtual keycode in the WM_KEYUP to reset the 
	shift state- if it doesn't get the right message, it thinks that shift is still down. To alleviate this problem,
	we track WM_KEYDOWN with VK_SHIFT and if we don't see the correct VK for the KEY_UP message, we create the right 
	keyevent ourself.
	*/

	if ((winEvt.m_nWin32MessageId == WM_KEYDOWN) && (winEvt.m_wParam == VK_SHIFT))
	{
		m_bTrack           = true; // start the track
		m_bIMEMessageRecd  = false;
	}

	if ((winEvt.m_nWin32MessageId == WM_KEYUP) && (m_bTrack == true) && (winEvt.m_wParam == VK_PROCESSKEY))
	{
//DS2	KeyEvent ev(KeyEvent::KeyUp, Key::Shift, 0, 0);
//DS2	if (pUIView)
//DS2		pUIView->HandleEvent(ev);	
	}

	// This is used to solve the following problem:
	/*
	On Vista, we use TSF sinks to obtain the candidate list and reading string data and IME messages for drawing the composition string,
	setting the cursor position etc. Now, there doesn't seem to be any safe order for these events. In particular, the following may happen:

	1) User types a key which causes the TSF event requesting a candidate list to be drawn to arrive. The StartComposition message has not 
	yet arrived. This causes the candidate list to be drawn to a bogus position since the composition string related bookkeeping inside 
	the imemanager is done in response to the startcomposition message. If the current cursor position coordinates are requested earlier
	than when then composition string has been initialized, bogus values are returned. 

	2) It's good to have the candidate list to appear below the current cursor position in the composition string - this way if the user
	changes the cursor position, the candidate list always stays below it, leading to a more natural IME user experience. The problem however 
	is that the IME message containing the cursor position arrives after the candidate list notification from TSF. This means that we can either
	draw the list when the TSF candidate notification arrives and then redraw the list when the cursor position is available. This works, but 
	causes annoying flickering as well as requires unnecessary drawing. 

	3) So why not just draw the candidate list when the IMEComposition message is received you ask? This will also take care of (1), since
	IMEComposition is always received after StartComposition. The problem is that not every candidate list notification is followed by 
	a IMEComposition mesasge. For example, if using QuanPin IME, if you type "ga" and then erase the "a", only a candidate list TSF notification
	is reveived, not IMEComposition message is received. This makes sense, since there was no change in the composition. Another example is when 
	a secondary candidate list is opened in DaYi or Array IME's. 

	This is really a very annoying problem and there seem no easy and elegant solutions besides having switches for different IME names leading to 
	code maintainance nightmare. After through research, the solution I've found is to set a flag indicating that candidatelist data is available for 
	drawing when the corresponding TSF notification is received, but save the drawing for later. The safest time to draw any pending candidate data is 
	when the KeyUp message arrives since it's guaranteed (I've never seen it happen otherwise) to arrive after all the IME messages and TSF notifications
	produced by the keystroke have been processed.

	8/27/2010: Just discovered that the above is not true for Japanese Office IME 2010. With this IME, the TSF notifications
	arrive AFTER the IME messages and Key_UP, so candidate list repositioning must be done during TSF notification handling.
	
	Similar logic applies to WM_LBUTTONUP. It's used to han
	*/

	if (winEvt.m_nWin32MessageId == WM_KEYUP || winEvt.m_nWin32MessageId == WM_LBUTTONUP)
	{
		if (m_bDrawCandidateList)
		{
			RepositionCandidateList(-m_nClausePosition);
			m_bDrawCandidateList = false;
		}
	}

	/*
	The logic below deals with removing the "secondary candidate list" in Dayi, Array and Boshiamy IMEs.
	What is a secondary candidate list? Using Array IME, type "t" (candidate list will open) and then space.
	Another candidate list will open. This is called the secondary candidate list. What's special about it?
	In wordpad, open secondary candidate list and type left/right arrow key. Candidate list will stay where it 
	is and the cursor will move. Candidate list doesn't get cancelled. In order to close composition, user can 
	1) either click on a candidate list row 2) use shift+number key corresponding to the candidate list row 3) escape
	out. The trouble is that we need to cancel the candidate list if the user uses left/right arrow key or return key etc
	to change the position of the cursor because we always want to finalize the state of the composition before any 
	changes in the cursor (not caused by IME) take place. Failing to do so will lead to "dangling candidate list" where
	the candidate list is just hanging over the UI not connected to anything. When the secondary list is open and the
	user pushes left/right arrow key, IME doesn't send us any information- it doesn't even set the VK to VK_PROCESSKEY
	therefore, the only option is to hardcode the desired behaviour as done below. 
	*/
	if (m_IMETag == GFxIME_Ch_Trad_WuXia ||
		m_IMETag == GFxIME_Ch_Trad_DaYi ||
		m_IMETag == GFxIME_Ch_Trad_Array||
		m_IMETag == GFxIME_Ch_Simp_WuBi86 ||
		m_IMETag == GFxIME_Ch_Simp_WuBi98)
	{
		if ((winEvt.m_nWin32MessageId == WM_KEYDOWN) &&
			(winEvt.m_wParam == VK_LEFT) ||
			(winEvt.m_wParam == VK_RIGHT) ||
			(winEvt.m_wParam == VK_UP) ||
			(winEvt.m_wParam == VK_DOWN) ||
			(winEvt.m_wParam == VK_RETURN) )
		{
			CloseIME();
		}
	}

	if ( ( m_IMETag == GFxIME_SogouPinyin || m_IMETag == GFxIME_BaiduPinyin ) && ((GFxIMEManagerWin32*)m_pIMEManagerBase)->m_OSVersion == GFxIMEManagerWin32::OSVER_WIN10 )
	{
		if ( winEvt.m_nWin32MessageId == WM_KEYDOWN && m_pIMEManagerBase->m_pUIView )
		{
			IIMEUIObject *pFocusedObject = m_pIMEManagerBase->m_pUIView->GetFocusedObject();
			if  (pFocusedObject && pFocusedObject->IME_GetObjectType() == UIOT_TEXTFIELD )
			{
				WPARAM virtualKey = winEvt.m_wParam;
				if ( virtualKey == VK_PROCESSKEY)
				{
					virtualKey = ImmGetVirtualKey( m_hWnd );
				}
			
				if ( virtualKey == VK_BACK )
				{
					IIMEUITextField *pTextField = static_cast<IIMEUITextField*>( pFocusedObject );
					uchar32 *pCompositionString = pTextField->IME_GetCompositionString();
					pTextField->IME_SetBackspaceFilter( pCompositionString && pCompositionString[0] != 0 );
				}
			}
		}
	}
}

IMEEventResult GFxIMEVista::OnIMEStartComposition( uintp wParam, uintp lParam, bool bDownFlag )
{  
    NOTE_UNUSED( wParam );
    NOTE_UNUSED( lParam );
	NOTE_UNUSED( bDownFlag );

	m_bStartCompositionReceived = true;

    // Important Note: If Dayi 6.0 IME is used, StartComposition is called after the candidate
    // list is opened and there has to be special handling for this case in IMEManager. 

    m_pIMEManagerBase->StartComposition();
	
	m_bStartCompositionReceived = true;
    // Record current system locale- This is used to determine which IME is active
    m_CurrLocale = GetKeyboardLayout(0);

    HIMC hIMC = ImmGetContext( m_hWnd );

    // Determine if the candidate list should start from 1 or 0
    int prop = ImmGetProperty(m_CurrLocale, IGP_PROPERTY);
    m_nCandListStartFrom1 = (prop & IME_PROP_CANDLIST_START_FROM_1) == 0 ? 0 : 1;

	if (m_IMETag == GFxIME_Ch_Trad_WuXia || m_IMETag == GFxIME_Ch_Trad_DaYi)
    {
        // For this IME, IMM reports the starting index of the candidate list incorrectly, so manually set it here.
        m_nCandListStartFrom1 = 0;
    }

	if (m_IMETag == GFxIME_Ch_Simp_WuBi86 || m_IMETag == GFxIME_Ch_Simp_WuBi98)
	{
		// For this IME, IMM reports the starting index of the candidate list incorrectly, so manually set it here.
		m_nCandListStartFrom1 = 1;
	}
   
	CANDIDATEFORM candForm;
	COMPOSITIONFORM compForm;	// This comes in handy to hide the Japanese IME composition window.

	POINT pt; pt.x = -1000; pt.y = -1000;
	RECT rect; rect.top = 0; rect.left = 0; rect.bottom = 10; rect.right = 10;

	candForm.dwIndex		= 0;
	candForm.dwStyle		= CFS_CANDIDATEPOS;
	candForm.ptCurrentPos	= pt;

	compForm.dwStyle        = CFS_FORCE_POSITION;
	compForm.ptCurrentPos   = pt;

	if (m_IMETag == GFxIME_NotSupported)
	{
		IMERectF viewRect, caretRect;
//DS2	IMERectF caretRectTransformed;

		m_pIMEManagerBase->GetMetrics( &viewRect, &caretRect, 0 );
//DS2	caretRectTransformed = ((MovieImpl*)(pUIView))->TranslateToScreen(caretRect);				
//DS2	pt.x = (LONG)caretRectTransformed.TopLeft().x;
//DS2	pt.y = (LONG)caretRectTransformed.BottomRight().y;

		candForm.ptCurrentPos = pt;
		compForm.ptCurrentPos = pt;
	}

	ImmSetCompositionWindow(hIMC, &compForm);   
	ImmSetCandidateWindow(hIMC, &candForm);
	ImmReleaseContext(m_hWnd, hIMC);

	if (m_IMETag == GFxIME_NotSupported) 
	{
		return IME_EVENT_NOTHANDLED;
	}

	return IME_EVENT_HANDLED;
}

void GFxIMEVista::OnIMEEndComposition( uintp wParam, uintp lParam, bool bDownFlag )
{
    NOTE_UNUSED3( wParam, lParam, bDownFlag );
 
    m_pIMEManagerBase->SetWideCursor(false);
    m_pIMEManagerBase->ClearComposition();

	m_bStartCompositionReceived = false;
}

// Processes candidate list notifications.
LRESULT GFxIMEVista::OnIMENotify( uint32 message, uintp wParam, uintp lParam, bool bDownFlag )
{
    NOTE_UNUSED( bDownFlag );
   
    switch ( wParam )
    {
    case IMN_CHANGECANDIDATE:
        {
			if ( m_IMETag == GFxIME_Kr_2003 || 
				m_IMETag == GFxIME_Ch_Simp_ABC || 
				m_IMETag == GFxIME_Ch_Simp_WuBi98 || 
				m_IMETag == GFxIME_Ch_Simp_WuBi86 || 
				m_IMETag == GFxIME_Jp_ATOK2008 || 
				m_IMETag == GFxIME_Jp_ATOK2009 ||
				m_IMETag == GFxIME_Jp_GOOG2010 )
            {
                OnChangeCandidate(0);
                break;
            }  
  
			if ( m_IMETag == GFxIME_SogouPinyin ||
				m_IMETag == GFxIME_GooglePinyin ||
				m_IMETag == GFxIME_Ch_Simp_QQPinyin ||
				m_IMETag == GFxIME_BaiduPinyin)
			{
				// We return NoDefaultAction, since if we pass this message to defwndproc, the def candidate list will pop up
				// (tested on Windows 7, Sogou Pinyin 5.0). So far I haven't seen any negative consequences of doing this on
				// the functioning of the candidate list. In case you see some unexpected problems with the candidate list
				// on Win Vista/7, try uncommenting this.
				OnChangeCandidate(0);
				return IME_EVENT_NODEFAULTACTION;
			}
			break;
        }

    case IMN_OPENCANDIDATE:
        {
            if ( m_IMETag == GFxIME_Kr_2003 ||
				m_IMETag == GFxIME_Ch_Simp_ABC ||
				m_IMETag == GFxIME_Ch_Simp_WuBi98 || 
				m_IMETag == GFxIME_Ch_Simp_WuBi86 ||
				m_IMETag == GFxIME_Jp_ATOK2008 ||
				m_IMETag == GFxIME_Jp_ATOK2009 ||
				m_IMETag == GFxIME_Jp_GOOG2010 ||
				m_IMETag == GFxIME_Ch_Simp_QQPinyin )
            {
                OnOpenCandidate(0);   
				
				// See comment above.
			//	return IME_EVENT_NODEFAULTACTION;
				break;
            }
			if ( m_IMETag == GFxIME_SogouPinyin ||
				m_IMETag == GFxIME_GooglePinyin ||
				m_IMETag == GFxIME_Ch_Simp_QQPinyin ||
				m_IMETag == GFxIME_BaiduPinyin)
			{
				OnOpenCandidate(0);
				return IME_EVENT_NODEFAULTACTION;
			}
			break;
        }

    case IMN_CLOSECANDIDATE:
        if ( m_IMETag == GFxIME_SogouPinyin ||
			m_IMETag == GFxIME_Kr_2003 ||
			m_IMETag == GFxIME_Ch_Simp_ABC ||
			m_IMETag == GFxIME_Ch_Simp_WuBi98 ||
			m_IMETag == GFxIME_Ch_Simp_WuBi86 ||
			m_IMETag == GFxIME_Jp_ATOK2008 ||
			m_IMETag == GFxIME_Jp_ATOK2009 ||
			m_IMETag == GFxIME_GooglePinyin ||
			m_IMETag == GFxIME_GooglePinyin ||
			m_IMETag == GFxIME_Jp_GOOG2010 ||
			m_IMETag == GFxIME_Ch_Simp_QQPinyin ||
			m_IMETag == GFxIME_BaiduPinyin )
        {
            m_pCandidateListBox->RemoveAllListItems();
			Invoke_RemoveList();
        }
        return DefWindowProc( m_hWnd, message, wParam, lParam );
    }

    CustomProcessing( message, wParam, lParam );

    return 0;
}

void GFxIMEVista::OnChangeCandidate(int candListIndex)
{
    HIMC        hIMC = 0;
    DWORD       dwSize;
    LPCANDIDATELIST lpCandList = NULL;

    hIMC = ImmGetContext(m_hWnd);

    dwSize = ImmGetCandidateList(hIMC, candListIndex, lpCandList, 0);
    if (dwSize == 0) // GetCandidateList failed
        return ;

    lpCandList = (CANDIDATELIST*)MemAlloc_Alloc( dwSize*sizeof(CANDIDATELIST) );
    dwSize = ImmGetCandidateList(hIMC, candListIndex, lpCandList, dwSize);

    m_nNumCandidates       = lpCandList->dwCount ;    // Number of candidates
    int currSelection   = lpCandList->dwSelection; // Currently selected row
    m_nCurrentPageStart    = lpCandList->dwPageStart;

    ImmReleaseContext(m_hWnd, hIMC);

    if (m_nNumCandidates ==  0)
    {
		g_pMemAlloc->Free( lpCandList );
        return;
    }

    m_pCandidateListBox->RemoveAllListItems();

    for (int i = 0; i < m_nNumCandidates; i++)
    {
        wchar_t *pRowData = (wchar_t*)(((char*)(lpCandList))+lpCandList->dwOffset[i+m_nCurrentPageStart]);
        CandidateListItem *pListItem  = new CandidateListItem( pRowData );
        m_pCandidateListBox->AddListItem( pListItem );
    }

    if(lpCandList)
		g_pMemAlloc->Free( lpCandList );

    ImmReleaseContext(m_hWnd, hIMC);

    // The row selection only needs to work for certain IME's- the others don't 
    // support selecting rows by pushing down/up arrow key 
    if ( m_IMETag == GFxIME_GooglePinyin ||
		m_IMETag == GFxIME_SogouPinyin ||
		m_IMETag == GFxIME_Jp_ATOK2008 ||
		m_IMETag == GFxIME_Jp_ATOK2009 ||
		m_IMETag == GFxIME_Jp_GOOG2010 ||
		m_IMETag == GFxIME_BaiduPinyin )
    {
        m_pCandidateListBox->SetSelectedItemIndex(max(0, (currSelection - m_nCurrentPageStart)));
    }
    else
    {
        m_pCandidateListBox->SetSelectedItemIndex(-1);
    }

    m_pCandidateListBox->UIRefreshView();
	m_bDrawCandidateList = true;
}

void GFxIMEVista::OnOpenCandidate( int nCandListIndex )
{
    HIMC hIMC = ImmGetContext(m_hWnd);

    // first obtain size of the list
    DWORD dwSize = ImmGetCandidateList( hIMC, nCandListIndex, NULL, 0 );
    if ( dwSize == 0 )
        return;

    CANDIDATELIST *pCandList = new CANDIDATELIST[dwSize];
    dwSize = ImmGetCandidateList( hIMC, nCandListIndex, pCandList, dwSize );

    m_nNumCandidates = pCandList->dwCount;
    int nCurrSelection = pCandList->dwSelection;
    m_nCurrentPageStart = pCandList->dwPageStart;
    int nPageSize = pCandList->dwPageSize;
  
    ImmReleaseContext( m_hWnd, hIMC );

    if ( m_nNumCandidates ==  0 )
    {
        delete [] pCandList;
        return;
    }
  
    m_nNumCandidatesOnListPrev = nPageSize;

	Invoke_RemoveList();
	Invoke_CreateList( nPageSize, m_nCandListStartFrom1 );

    m_pCandidateListBox->RemoveAllListItems();
    for ( int i = 0; i < m_nNumCandidates; i++ )
    {
        wchar_t *pRowData = (wchar_t*)(((char*)(pCandList))+pCandList->dwOffset[i+m_nCurrentPageStart]);
        CandidateListItem *pListItem  = new CandidateListItem( pRowData );
        m_pCandidateListBox->AddListItem( pListItem ); 
    }

     delete [] pCandList;

    // The row selection only needs to work for certain IME's- the other's don't 
    // support selecting rows by pushing down/up arrow key

    if ( m_IMETag == GFxIME_GooglePinyin || 
		m_IMETag == GFxIME_SogouPinyin ||
		m_IMETag == GFxIME_Jp_ATOK2008 ||
		m_IMETag == GFxIME_Jp_ATOK2009 || 
		m_IMETag == GFxIME_Jp_GOOG2010 || 
		m_IMETag == GFxIME_BaiduPinyin )   
    {
        m_pCandidateListBox->SetSelectedItemIndex( nCurrSelection % nPageSize );
    }
    else
    {
        m_pCandidateListBox->SetSelectedItemIndex( -1 );
    }

    m_pCandidateListBox->UIRefreshView();
	m_bDrawCandidateList = true;

    // Need to reposition candidate list so that it appears below currently highlighted clause. Since clause information 
    // is available as a result of processing WM_COMPOSITION message, it needs to be stored in a class member variable.
 //   RepositionCandidateList(-ClausePosition);     
}

void GFxIMEVista::Finalize( bool bCancel )
{
    HIMC     hIMC;

    hIMC = ImmGetContext(m_hWnd);

    // In addition to finalizing IME, reset cursor position to the position of the mouse click. This is to deal
    // with the situation when composition has not been finalized and the user clicks elsewhere in the textfield. 
    // The cursor should return to the position of the click once the IME composition has been finalized. 

    // Finalize- This will cause WM_IMECOMPOSITION with GCS_RESULTSTR to be sent. 
    // In response, we'll obtain the result string and call FinalizeComposition on the 
    // IMEManager.

    ImmNotifyIME(hIMC, NI_COMPOSITIONSTR, CPS_COMPLETE, 0);
    ImmNotifyIME(hIMC, NI_COMPOSITIONSTR, CPS_CANCEL, 0);

    ImmReleaseContext(m_hWnd, hIMC);

	m_pTSF->AbortCandidateList();

	if ( m_IMETag == GFxIME_Ch_Trad_Array ||
		m_IMETag == GFxIME_Ch_Trad_DaYi )
	{
		INPUT input;
		input.type      = INPUT_KEYBOARD;
		input.ki.wVk    = VK_ESCAPE;
		input.ki.wScan  = (WORD)MapVirtualKey(VK_ESCAPE, 0);
		SendInput(1, &input, sizeof(input));
	}

    // Clear Reading window contents and remove the reading window
    m_ReadingTextBuffer[0] = 0;
	Invoke_RemoveList();
}

void GFxIMEVista::OnIMEComposition( uintp wParam, uintp lParam, int nOptions )
{
    NOTE_UNUSED( nOptions );

    if ( lParam == 0 )
    {
        DisplayCompositionString( m_nNumCharDisplay );
    }

	if ( lParam & GCS_RESULTSTR ) 
    {
        HIMC hIMC = ImmGetContext( m_hWnd );

        // Get the size of the result string.
        LONG nNumBytes = ImmGetCompositionString( hIMC, GCS_RESULTSTR, NULL, 0 );

        wchar_t *pWideString = (wchar_t*)new wchar_t[2*(nNumBytes+1)];
		V_memset( pWideString, 0, 2*(nNumBytes+1) );

        // Get the result string that is generated by IME
        LONG res = ImmGetCompositionString( hIMC, GCS_RESULTSTR, pWideString, nNumBytes );
		if (res != IMM_ERROR_NODATA && res != IMM_ERROR_GENERAL)
        {
            m_pIMEManagerBase->SetWideCursor( false );
        }

        m_nNumCharDisplay = 0;
        m_bReplaceChar = false;
        m_ReadingTextBuffer[0] = 0;
   
        ImmReleaseContext(m_hWnd, hIMC);
        m_pIMEManagerBase->FinalizeComposition( pWideString );

		delete [] pWideString; 
    }

    if ( ( lParam & CS_INSERTCHAR ) && ( lParam & CS_NOMOVECARET ) )
    {
        // This is for Korean- a character can change shape depending upon what is typed next, 
        // so don't advance caret position. Move Caret when the character is finalized.
        // The "replace" flag is used to indicate whether we should replace the current character 
        // or not-In case of korean, when the user begins to type, the character should be 
        // inserted at the current location, but subsequent modifications to the same character 
        // should replace the existing character.
        wchar_t wideString[2];
        wideString[0] = (wchar_t)wParam;
		wideString[1] = 0;

        m_bReplaceChar = true;
        m_nNumCharDisplay = 1;

        m_pIMEManagerBase->SetWideCursor( true );
        m_pIMEManagerBase->SetCompositionPosition();
        m_pIMEManagerBase->SetCompositionText( wideString );
    }

    if ( ( lParam & GCS_COMPSTR ) && ( lParam & GCS_CURSORPOS ) )
    {
		if ( m_IMETag != GFxIME_Ch_Trad_WuXia )
		{
			DisplayCompositionString( m_nNumCharDisplay );
		}
    }

    if ( ( lParam & GCS_COMPATTR ) )
    {
        HIMC hIMC = ImmGetContext(m_hWnd);
        LONG nNumBytes = ImmGetCompositionString( hIMC, GCS_COMPATTR, NULL, 0 );
        char *pAttributeString = new char[nNumBytes+1];
		V_memset( pAttributeString, 0, nNumBytes+1 );
        ImmGetCompositionString( hIMC, GCS_COMPATTR, (LPVOID)pAttributeString, nNumBytes );

		// The min needs to be taken since for chinese IME (ZhengMa), dwSize doesn't get updated correctly
		// when user pushes backspace.
		int numChar = Min( (uint32)nNumBytes, m_nNumCharDisplay );

		// Obtain reading string information.
        nNumBytes = ImmGetCompositionString( hIMC, GCS_COMPREADSTR, NULL, 0 );
        wchar_t *pWideReadingString = new wchar_t[2*(nNumBytes+1)];
        V_memset( pWideReadingString, 0, 2*(nNumBytes+1) );
        ImmGetCompositionString( hIMC, GCS_COMPREADSTR, (LPVOID)pWideReadingString, nNumBytes );

        // Get cursor position information
        int pos = ImmGetCompositionString( hIMC, GCS_CURSORPOS, NULL, 0 );
        ImmReleaseContext( m_hWnd, hIMC );

        m_pIMEManagerBase->SetCursorInComposition((uintp)pos);

        // Note: indexes for arrays/strings should be uintp to eliminate warnings for 64 bit comps.
        // uintp start = 0; 
        // bool clauseFlag = false;
        
        struct styleInfo style;
        CUtlVector<styleInfo> styleList;

        char currentStyle = pAttributeString[0];
        m_nClausePosition = numChar - pos;
        style.m_nBegin = 0; 
        style.m_nHighLightStyle = pAttributeString[0];
		
        // This is important because for Japanese IME, the candidate list doesn't always have to be next to the clause
        // for example, when one converts the composition string to convert mode by pushing space, the current clause is
        // usually in the beginning of the string while the cursor is at the end. For other IME's like Chinese Simplified
        // Pinyin, the candidate list is always at the position of the cursor and no clause information is provided. Hence
        // the ClausePosition must be initialized to the cursorPosition unless IME explicitly provides clause information 
        // in the attribute string.
        if (pAttributeString[0] == ATTR_TARGET_CONVERTED || pAttributeString[0] == ATTR_TARGET_NOTCONVERTED)
		{
            m_nClausePosition = numChar;
		}

		DisplayReadingWindow( CStrAutoEncodeSrc2( pWideReadingString ).ToUTF32() );

        for (int i = 0; i < numChar; i++)
        {
            if (pAttributeString[i] != currentStyle)
            {
                styleList.AddToTail(style);
                if (pAttributeString[i] == ATTR_TARGET_CONVERTED || pAttributeString[i] == ATTR_TARGET_NOTCONVERTED)
				{
                    m_nClausePosition = numChar - i;
				}

                style.m_nBegin = i;
                currentStyle = pAttributeString[i];
                style.m_nHighLightStyle = currentStyle;
            }
            style.m_nEnd = i;
        }

        style.m_nEnd = numChar - 1;
        styleList.AddToTail(style);

        m_pIMEManagerBase->SetCursorInComposition((uintp)pos);

		// No highlighting necessary for Korean.
        if ( !(m_IMETag & GFxIME_Kr_Flag) )
        {
            for ( int i = 0; i < styleList.Count(); i++ )
            {
                uint32 currentStyle = styleList[i].m_nHighLightStyle;
                uint32 start = styleList[i].m_nBegin;
                uint32 len = styleList[i].m_nEnd - styleList[i].m_nBegin + 1;

				if ( len == 0 )
					continue; 

                switch( currentStyle )
                {
                case ATTR_INPUT:
                    m_pIMEManagerBase->HighlightText(start, len, THS_CompositionSegment, 0);
                    break;   
                case ATTR_TARGET_CONVERTED:
                case ATTR_TARGET_NOTCONVERTED:
                    m_pIMEManagerBase->HighlightText(start, len, THS_ClauseSegment, 0);
                    break;
                case ATTR_CONVERTED:
                    m_pIMEManagerBase->HighlightText(start, len, THS_ConvertedSegment, 0);
                    break;
                case ATTR_INPUT_ERROR:
                    m_pIMEManagerBase->HighlightText(start, len, THS_LowConfSegment, 0);
                    break;
                }
            }
        }
       
        if ( m_pTSF->GetCandidateWindowFlag() )
		{
        //     RepositionCandidateList(-ClausePosition);
		}

		delete [] pAttributeString;
		delete [] pWideReadingString;
    }
}

void GFxIMEVista::RepositionCandidateList( uint32 offset )
{
    IMERectF viewRect, caretRect;
    m_pIMEManagerBase->GetMetrics( &viewRect, &caretRect, offset );

	Invoke_RepositionCandidateList( &caretRect );
}

void GFxIMEVista::DisplayCompositionString(uint32& NumCharDisplay)
{
    // Obtain composition string information from IMM
    HIMC hIMC = ImmGetContext( m_hWnd );
    int dwSize = ImmGetCompositionString(hIMC, GCS_COMPSTR, NULL, 0);
    wchar_t* lpstr = (wchar_t*)MemAlloc_Alloc( 2*(dwSize+1) );
	V_memset( lpstr, 0, 2*(dwSize+1) );
    ImmGetCompositionString(hIMC, GCS_COMPSTR, (LPVOID)lpstr, dwSize);

    lpstr[dwSize/2] = 0;

    if (lpstr[0] == 0x20 || lpstr[0] == 0x3000)
	{
        lpstr[0] = 160; // Insert non breaking space
	}

	// Since for ABC IME, composition text must be displayed in the reading window.
	if (m_IMETag == GFxIME_Ch_Simp_ABC)
	{
		m_ReadingStringBuffer = lpstr;

		// Setting the ReadinStringBuffer is enough since DisplayReadingWindow will be called when the WM_IMECOMPOSITION with
		// GCS_COMPATTR set is received.
		if (lpstr)
			g_pMemAlloc->Free( lpstr );
		return;
	}
    // Write new composition text
    m_pIMEManagerBase->SetCompositionText(lpstr);
    NumCharDisplay = dwSize/2;

    if(m_IMETag == GFxIME_SogouPinyin || m_IMETag == GFxIME_GooglePinyin || m_IMETag == GFxIME_BaiduPinyin) //  Chinese (Simplified) - sogou_pinyin on Win Vista
    {
        // Note that for this IME, no attribute information is sent, so cursor position and 
        // composition string attribute must be set here.
        // Get cursor position information
        uint32 pos = ImmGetCompositionString(hIMC, GCS_CURSORPOS, NULL, 0);
        // Set cursor position in the composition string
        m_pIMEManagerBase->SetCursorInComposition((uintp)pos);
        m_nClausePosition = NumCharDisplay - pos;
        m_pIMEManagerBase->HighlightText(0, NumCharDisplay, THS_CompositionSegment, 0);
    }
    // Now reset the cursor position in the textfield.
    // This check is important because when we don't want the cursor position to change
    // if the composition string is empty. 
    if (dwSize != 0) 
    {   
        // Adjust cursor Position
        int pos1 = ImmGetCompositionString(hIMC, GCS_CURSORPOS, NULL, 0);
        m_pIMEManagerBase->SetCursorInComposition((uintp)pos1);
    }

    IMERectF viewRect, caretRect;
    m_pIMEManagerBase->GetMetrics(&viewRect, &caretRect, 0);

    // Now pass it on to the DisplayIn
    // putWindow function so it knows where to put the reading window. This way
    // DisplayInputWindow is independent of the flash document that contains the text field. 
    // Params: 1: Reading String 2,3: x-y position of the Reading Window 4: Color of Reading text.

    // Note that for Dayi 6.0 and Array 6.0 IMEs, for a fresh composition, the Reading window information is received 
    // earlier than startcomposition message. This causes GetMetrics to return bad positioning information since the 
    // textfield pointer in the IMEManager is set when StartComposition is called. To take care of this, we have added 
    // logic in IMEManager's OnOpenCandidateList to set the textfield pointer from the currently focussed item's textfield.
    // We don't call RepositionInputWindow for chinese traditional IME's since it causes the previously drawn window to
    // move leading to flickering. 

    if (!(m_IMETag & GFxIME_Ch_Trad_Flag))
	{
		Invoke_RepositionInputWindow( &caretRect );
	}

    ImmReleaseContext( m_hWnd, hIMC );
    if (lpstr )
	{
		g_pMemAlloc->Free( lpstr );
	}
}

void GFxIMEVista::DisplayReadingWindow( const uchar32* pReadingStr )
{
	NOTE_UNUSED( pReadingStr );
	
    // Remove previous input window..
	Invoke_RemoveInputWindow();
 	
	if ( !m_ReadingStringBuffer.Length() )
		return;
	
    IMERectF viewRect, caretRect;
    m_pIMEManagerBase->GetMetrics( &viewRect, &caretRect, -m_nClausePosition );

    // Now pass it on to the DisplayInputWindow function so it knows where to put the reading window. This way
    // DisplayInputWindow is independent of the flash document that contains the text field. 
	Invoke_DisplayInputWindow( CStrAutoEncodeSrc2( m_ReadingStringBuffer.Get() ).ToUTF32(), &caretRect );
	
    return;
}

void GFxIMEVista::SelectAndClose(int index)
{
    // This function is called when the user clicks on a candidate list row. This action should update the 
    // composition string with the contents of the clicked row as well as close the candidate list.
    // TSF provides a way to notify IME about the click- but some third party IMEs
    // such as google and sogou don't follow it. So this can't be a general purpose solution
    // The other way to do this is to simulate a key press that corresponds to the candidate list row that was
    // clicked. We use this approach for the third party IMEs and the regular approach for system IMEs
	// Note: (Mar 18 2009) Shuang Pin, Quan Pin and ZhengMa also don't follow the method recommeded in the doc
	// which involves obtaining the IID_ITfCandidateListUIElementBehavior_GFx pointer and calling finalize on it
	// For these renegade IME's, NO_INTERFACE is returned while querying for the above interface.

    if ((m_IMETag & GFxIME_Ch_ThirdParty_Flag) || 
		m_IMETag == GFxIME_Ch_Simp_ZhengMa ||
		m_IMETag == GFxIME_Ch_Simp_ShuangPin ||
		m_IMETag == GFxIME_Ch_Simp_QuanPin)
    {
        INPUT input;
        input.type      = INPUT_KEYBOARD;
        input.ki.wVk    = (WORD)(0x30 + (index+1));
        input.ki.wScan  = (WORD)MapVirtualKey(0x30 + (index+1), 0);
        SendInput(1, &input, sizeof(input));
    }
    else
    {
        m_pTSF->SelectAndClose(index);
    }
}

// Closes IME
void GFxIMEVista::CloseIME()
{
	/*
	closing IME and removing the candidate list is another can of worms. There are atleast three ways to do it, 
	none of which works for all the IME's.

	1) Using the IMM functions to cancel composition and close the candidate list. These are well documented in the 
	MSDN documentation.

	2) Using the Abort method of the ITfCandidateListUIElementBehavior interface. This obviously only works if a pointer
	to that interface can be obtained

	3) Putting an Esc in the input queue. This is the most hacky but works even for the most inconsistent IMEs.
	*/

    HIMC hIMC = ImmGetContext( m_hWnd );
    if (!hIMC)
	{
    }

	// Method 1:
    ImmNotifyIME(hIMC, NI_COMPOSITIONSTR, CPS_CANCEL, 0);
	ImmNotifyIME(hIMC, NI_CLOSECANDIDATE, 0, 0);
	
	// Method 2:
	m_pTSF->AbortCandidateList();

	// Method 3: 
	if ( m_IMETag == GFxIME_Ch_Trad_Array ||
		m_IMETag == GFxIME_Ch_Trad_DaYi )
	{
		INPUT input;
		input.type      = INPUT_KEYBOARD;
		input.ki.wVk    = VK_ESCAPE;
		input.ki.wScan  = (WORD)MapVirtualKey(VK_ESCAPE, 0);
		SendInput(1, &input, sizeof(input));
	}
		
    m_ReadingTextBuffer[0] = 0;
	Invoke_RemoveInputWindow();

    ImmReleaseContext( m_hWnd, hIMC );
}

/*
We override OnEnableIME for Vista for the following reason: 
When the user clicks on a non-ime enabled flash object such as the background, before the focus transfer takes place to the new object,
the following two events happen (in this order):

1) Finalize is called, which causes the IME state to be finalized (candidate list closes, composition is canceled or finalized)

2) IME is disabled by associating a null context. This is done since we don't want IME popups when the focus is on a non-editable or ime disabled
object

On XP, we use the IMM functions to finalize- these functions cause the defwndproc to be called directly and ime state is finalized appropriately
before IME is disabled in OnEnableIME(false). On Vista, for the Dayi and Array IME's the secondary candidate lists don't follow the IMM api. Therefore,
in order to cause the state of IME to be finalized during "Finalize", I use SendInput to insert an Escape character in the input queue. However,
the effect of this Escape is asynchronous, so if IME is disabled immediately afterwards, by the time the escape character is interpreted by the IME
, it's state has already been disabled and no action is taken (no EndUIElement is called) leading to dangling candidate lists. Therefore, we maintain
a referencecount for IME UI elements and check to see if any UI elements need to be closed. If there are any, we defer disabling IME until EndUIElement
has been called on all the open UI elements and IME can safely be disabled.
*/
void GFxIMEVista::OnEnableIME(bool enable)
{
	if (enable == false && m_nCandidateWindowRefCount > 0)
		m_bNeedToDisableIME = true;

	if ( m_nCandidateWindowRefCount == 0 )
	{

		if( enable )
		{
			ImmAssociateContextEx( m_hWnd, NULL, IACE_DEFAULT );
		}

		if( !enable )
		{
			ImmAssociateContextEx( m_hWnd, NULL, 0 );
		}

		m_bNeedToDisableIME = false;
	}
}

