/**************************************************************************

Filename    :   GFx_IMETSF.cpp
Content     :   Text Services Framework related implementation.
Created     :   OCt 4, 2007
Authors     :   Ankur Mohan

Copyright   :   Copyright 2011 Autodesk, Inc. All Rights reserved.

Use of this software is subject to the terms of the Autodesk license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

**************************************************************************/

#include "vstdlib/vstrtools.h"

#include "imemanager.h"

#include "tier0/platform_com.h"

// The correct way to define GUIDs. You put DEFINE_GUID
//macro in some header file. All sources that need the definition can
//include the header. In exactly one .cpp file, you include initguid.h,
//then include the header with GUID definitions after that. 

#include <initguid.h>
#include "gfx_imetsf.h"
#include "gfx_imewin32impl.h"

#define TF_CLUIE_COUNT            0x00000002 
#define TF_CLUIE_SELECTION        0x00000004 
#define TF_CLUIE_STRING           0x00000008 
#define TF_CLUIE_PAGEINDEX        0x00000010 
#define TF_CLUIE_CURRENTPAGE      0x00000020

#define TF_PROFILETYPE_INPUTPROCESSOR		0x0001
#define TF_PROFILETYPE_KEYBOARDLAYOUT		0x0002

#define TF_CONVERSIONMODE_ALPHANUMERIC		0x0000
#define TF_CONVERSIONMODE_NATIVE			0x0001
#define TF_CONVERSIONMODE_KATAKANA			0x0002
#define TF_CONVERSIONMODE_FULLSHAPE			0x0008
#define TF_CONVERSIONMODE_ROMAN				0x0010
#define TF_CONVERSIONMODE_CHARCODE			0x0020
#define TF_CONVERSIONMODE_SOFTKEYBOARD		0x0080
#define TF_CONVERSIONMODE_NOCONVERSION		0x0100
#define TF_CONVERSIONMODE_EUDC				0x0200
#define TF_CONVERSIONMODE_SYMBOL			0x0400
#define TF_CONVERSIONMODE_FIXED				0x0800

#define TF_IPPMF_FORPROCESS					0x10000000

EXTERN_C const GUID GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION_GFX;

CTSF::CTSF( GFxIMEWin32Impl *pIME )
{
	m_nRef = 1;

    m_pIMEVista = (GFxIMEVista*)pIME;
    m_pThreadMgr = NULL;
    m_pTimEx = NULL;

    m_dwThreadMgrEventSinkCookie = TF_INVALID_COOKIE;
    m_dwUIElementSinkCookie = TF_INVALID_COOKIE;
	m_dwTextEditSinkCookie = TF_INVALID_COOKIE;

	m_CandidateListId = (DWORD)-1;
	m_ReadingWindowId = (DWORD)-1;
	m_dwCandListId = (DWORD)-1;

	m_NumCandidates = 0;

	m_bNeedRepositioning = false;
    m_bReadingWindowFlag = false;
	m_bCandidateWindowFlag = false;
 
	m_CurrentPageStart = 0;
}

CTSF::~CTSF()
{
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
STDAPI CTSF::QueryInterface( REFIID riid, void **ppvObj )
{
    if ( !ppvObj )
	{
        return E_INVALIDARG;
	}

    *ppvObj = NULL;

    if ( IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfThreadMgrEventSink) )
    {
        *ppvObj = (ITfThreadMgrEventSink *)this;
    }
    else if ( IsEqualIID(riid, IID_ITfUIElementSink_GFx) )
    {
        *ppvObj = (ITfUIElementSink *)this;
    }
	else if ( IsEqualIID(riid, IID_ITfContextOwnerCompositionSink) )
    {
        *ppvObj = (ITfContextOwnerCompositionSink *)this;
    }
	else if ( IsEqualIID(riid, IID_ITfTextEditSink) )
    {
        *ppvObj = (ITfTextEditSink *)this;
    }

    if (*ppvObj)
    {
        AddRef();
        return S_OK;
    }

    return E_NOINTERFACE;
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
STDAPI_(ULONG) CTSF::AddRef()
{
    return ++m_nRef;
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
STDAPI_(ULONG) CTSF::Release()
{   
    int32 nRef = --m_nRef;
    if (m_nRef == 0)
    {
        delete this;
    }

    return nRef;
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
bool CTSF::Init()
{
    HRESULT hr = Plat_RequireLoadCOM()->pCoCreateInstance(CLSID_TF_ThreadMgr, 
                          NULL, 
                          CLSCTX_INPROC_SERVER, 
                          IID_ITfThreadMgr, 
                          (void**)&m_pThreadMgr);
    if ( FAILED( hr ) )
    {
		Log_Warning( LOG_IME, "TSF: Failed to CoCreateInstance.\n" );

		// Fails with an interface.
		if ( m_pThreadMgr )
		{
			m_pThreadMgr->Release();
			m_pThreadMgr = NULL;
		}
        return false;
    }

    hr = m_pThreadMgr->QueryInterface( IID_ITfThreadMgrEx_GFx, (void **)&m_pTimEx );
    if ( FAILED( hr ) )
    {
		Log_Warning( LOG_IME, "TSF: Failed to load the TSF thread manager.\n" );

		m_pThreadMgr->Release();
		m_pThreadMgr = NULL;
   
        return false;
    }

    hr = m_pTimEx->ActivateEx( &m_ClientId, TF_TMAE_UIELEMENTENABLEDONLY );
    if ( FAILED( hr ) )
    {
    //    printf("Failed to activate UILess mode\n");
    //    return FALSE;
    }

    if ( !InitThreadMgrSink() )
    {
		Log_Warning( LOG_IME, "TSF: Failed to initialize thread manager sink.\n" );
        return false;
    }

    if ( !InitUIElementSink() )
    {
		Log_Warning( LOG_IME, "TSF: Failed to initialize UI Element sink.\n" );
        return false;
    }

	m_pIMEVista->m_nCandidateWindowRefCount = 0;
 
    return true;
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
void CTSF::Shutdown()
{
	UnInitUIElementSink();
	UninitThreadMgrSink();
	ReleaseInterfaces();
	int nRef = Release();

	Assert( nRef == 0 );
}

void CTSF::ReleaseInterfaces()
{
	if ( m_pThreadMgr )
	{
		m_pThreadMgr->Release();
		m_pThreadMgr = NULL;
	}

    if ( m_pTimEx )
	{
		// Must deactivate before releasing
		m_pTimEx->Deactivate(); 
		m_pTimEx->Release();
		m_pTimEx = NULL;
	}
}

bool CTSF::InitUIElementSink()
{
    ITfSource *pSource = NULL;
    bool bRet = false;

    if (FAILED( m_pThreadMgr->QueryInterface(IID_ITfSource, (void **)&pSource)))
        goto cleanUp;

    if (FAILED(pSource->AdviseSink(IID_ITfUIElementSink_GFx, (ITfUIElementSink *)this, &m_dwUIElementSinkCookie)))
    {
        // make sure we don't try to Unadvise _dwThreadMgrEventSinkCookie later
        m_dwUIElementSinkCookie = TF_INVALID_COOKIE;
        goto cleanUp;
    }

    bRet = true;

cleanUp:
    if (pSource)
	{
        pSource->Release();
	}
    return bRet;
}

//----------------------------------------------------------------------------
// Advise our sink.
//----------------------------------------------------------------------------
bool CTSF::InitThreadMgrSink()
{
    ITfSource *pSource = NULL;
    bool bRet = FALSE;

    if (FAILED( m_pThreadMgr->QueryInterface(IID_ITfSource, (void **)&pSource)))
        goto cleanUp;

    if (FAILED(pSource->AdviseSink(IID_ITfThreadMgrEventSink, (ITfThreadMgrEventSink *)this, &m_dwThreadMgrEventSinkCookie)))
    {
        // make sure we don't try to Unadvise _dwThreadMgrEventSinkCookie later
        m_dwThreadMgrEventSinkCookie = TF_INVALID_COOKIE;
        goto cleanUp;
    }

    bRet = true;

cleanUp:
    if ( pSource )
	{
        pSource->Release();
	}
    return bRet;
}

//----------------------------------------------------------------------------
// Unadvise our sink.
//----------------------------------------------------------------------------
void CTSF::UnInitUIElementSink()
{
    ITfSource *pSource = NULL;

    if ( m_dwUIElementSinkCookie == TF_INVALID_COOKIE)
        return; // never Advised

    if (SUCCEEDED( m_pThreadMgr->QueryInterface(IID_ITfSource, (void **)&pSource)))
    {
        if (pSource->UnadviseSink(m_dwUIElementSinkCookie) == CONNECT_E_NOCONNECTION)
            Release();
        pSource->Release();
    }

    m_dwUIElementSinkCookie = TF_INVALID_COOKIE;
}

//----------------------------------------------------------------------------
// Unadvise our sink.
//----------------------------------------------------------------------------
void CTSF::UninitThreadMgrSink()
{
    ITfSource *pSource = NULL;

    if ( m_dwThreadMgrEventSinkCookie == TF_INVALID_COOKIE )
        return; // never Advised

    if (SUCCEEDED( m_pThreadMgr->QueryInterface(IID_ITfSource, (void **)&pSource)))
    {
        // Sometimes the UnadviseSink function returns CONNECT_E_NOCONNECTION.
        // when this happens, release will not be called leading to memory leaks.
        // don't know why it returns CONNECT_E_NOCONNECTION. Currently this only happens
        // with the CompartmentSink. So if UnadviseSink fails, call release manually.
        if( pSource->UnadviseSink(m_dwThreadMgrEventSinkCookie) == CONNECT_E_NOCONNECTION)
            Release();
        pSource->Release();
    }

    m_dwThreadMgrEventSinkCookie = TF_INVALID_COOKIE;
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
STDAPI CTSF::BeginUIElement( DWORD dwUIElementId, BOOL* pbShow )
{
    NOTE_UNUSED( dwUIElementId );
	
 	m_pIMEVista->m_bNeedToDisableIME = false;
	
	// TSF should not draw
    *pbShow = FALSE;
	if (m_pIMEVista->m_IMETag == GFxIME_NotSupported || m_pIMEVista->m_IMETag == GFxIME_NotSet)
		return S_OK;

	if ( !m_pIMEVista->IsTextFieldFocused() )
	{
		return S_OK;
	}

	m_pIMEVista->m_nCandidateWindowRefCount++;

	m_pIMEVista->Invoke_RemoveList();
	m_pIMEVista->Invoke_CreateList( 10, 1 );
    
    m_bCandidateWindowFlag = false;
    m_bReadingWindowFlag = false;
      
    // Get Language Bar Info
    ITfLangBarItemMgr* pLangBarMgr = NULL;
    HRESULT hr;

    hr = m_pThreadMgr->QueryInterface(IID_ITfLangBarItemMgr, (void **)&pLangBarMgr);
    if (SUCCEEDED(hr) && pLangBarMgr)
    {
        IEnumTfLangBarItems *pEnumTfLangBarItem = NULL;
        hr = pLangBarMgr->EnumItems(&pEnumTfLangBarItem);
        if (hr == S_OK)
        {
            ITfLangBarItem *pItem = NULL;
             while (pEnumTfLangBarItem->Next(1, &pItem, NULL) == S_OK)
             {
				TF_LANGBARITEMINFO info;
                pItem->GetInfo(&info);
                pItem->Release();
             }
             pEnumTfLangBarItem->Release();
        }
        pLangBarMgr->Release();
    }
          
    return S_OK;
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
bool CTSF::SelectAndClose( uint32 nIndex )
{
	ITfUIElementMgr *pUIElementMgr = NULL;
	if (FAILED( m_pThreadMgr->QueryInterface(IID_ITfUIElementMgr_GFx, (void **)&pUIElementMgr)))
		return false;

	ITfUIElement *pElement = NULL;
	if ( FAILED(pUIElementMgr->GetUIElement( m_CandidateListId, &pElement)))
	{
		if (pUIElementMgr)
			pUIElementMgr->Release();
		return false;
	}

	ITfCandidateListUIElementBehavior* pCandListBehaviour = NULL;     
	if ( FAILED(pElement->QueryInterface(IID_ITfCandidateListUIElementBehavior_GFx, (void **)&pCandListBehaviour)))
	{
		if (pElement)
			pElement->Release();
		if (pUIElementMgr)
			pUIElementMgr->Release();
		return false;
	}

	pCandListBehaviour->SetSelection( m_CurrentPageStart + nIndex );

	if (pElement)
		pElement->Release();
	if (pUIElementMgr)
		pUIElementMgr->Release();
	if (pCandListBehaviour)
		pCandListBehaviour->Release();

	return true;
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
STDAPI CTSF::UpdateUIElement( DWORD dwUIElementId )
{ 
    UINT globalNumCandidates = 0;

	if ( m_pIMEVista->m_IMETag == GFxIME_NotSupported || m_pIMEVista->m_IMETag == GFxIME_NotSet )
		return FALSE;

	if ( !m_pIMEVista->IsTextFieldFocused() )
	{
		return S_OK;
	}

	ITfUIElementMgr *pUIElementMgr = NULL;
    if ( FAILED( m_pThreadMgr->QueryInterface(IID_ITfUIElementMgr_GFx, (void **)&pUIElementMgr)))
        return FALSE;
   
	ITfUIElement *pElement = NULL;
    pUIElementMgr->GetUIElement(dwUIElementId, &pElement);
    
	ITfCandidateListUIElement *pCandidate = NULL;
	if (SUCCEEDED(pElement->QueryInterface(__uuidof(ITfCandidateListUIElement), (void **)&pCandidate)))
    {
        pCandidate->Release();
		pCandidate = NULL;
    }

	GUID pguid;
    pElement->GetGUID(&pguid);

	BSTR pDesc;
    pElement->GetDescription(&pDesc);
    
	ITfReadingInformationUIElement *pReading = NULL;
	if (SUCCEEDED(pElement->QueryInterface(__uuidof(ITfReadingInformationUIElement), (void **)&pReading)))
    {
		wchar_t *pReadingStr = NULL;
        pReading->GetString( &pReadingStr );

        m_ReadingWindowId = dwUIElementId;

		// This is needed since for some IME's UpdateUIElement is called before the StartComposition message is received.
		// The composition string in the text field is initialized during processing of startcomposition. So if StartComposition
		// has not yet been received, we store the reading string and don't try to display it immediately, otherwise it may not
		// be positioned properly.
        if ( pReadingStr )
		{
			if (m_pIMEVista->m_bStartCompositionReceived == false)
			{
				m_pIMEVista->m_ReadingStringBuffer = pReadingStr;
			}
			else
			{
				m_pIMEVista->m_ReadingStringBuffer = pReadingStr;
				m_pIMEVista->DisplayReadingWindow( CStrAutoEncodeSrc2( pReadingStr ).ToUTF32() );
			}
        }

        // This needs to be done since for chinese simplified IMEs if user types a combination that results 
        // in no candidates, candidate list logic below will never be executed and we'll be stuck with the previous 
        // candidate list. Hence, we need to remove the candidate list here itself.         
		m_pIMEVista->Invoke_ShowList( false );
       
       // If two readingwindow messages are received in a row without an intervening candidatewindow message,
       // remove the candidate list.
		if ( m_bReadingWindowFlag )
		{
			m_pIMEVista->Invoke_RemoveList();
			m_bCandidateWindowFlag = false;
		}
        m_bReadingWindowFlag = true;
		pReading->Release();
		pReading = NULL;
    }

	if (SUCCEEDED(pElement->QueryInterface(__uuidof(ITfCandidateListUIElement), (void **)&pCandidate)))
    {
        // Reset ReadingWindowFlag
		m_dwCandListId = dwUIElementId;
        m_bReadingWindowFlag = false;
        if ( !m_bCandidateWindowFlag )
        {
			// Decide if the candidate list starts from 0 or 1. Default is 1. 
			int candListStartFrom1 = 1;
			if (m_pIMEVista->m_IMETag == GFxIME_Ch_Trad_WuXia || m_pIMEVista->m_IMETag == GFxIME_Ch_Trad_DaYi)
			{
				// For this IME, IMM reports the starting index of the candidate list incorrectly, so manually set it here.
				candListStartFrom1 = 0;
			}
 
			m_pIMEVista->Invoke_CreateList( 10, candListStartFrom1 );
			m_bCandidateWindowFlag = true;
        }

		UINT numCandidates = 0;
        pCandidate->GetCount( &numCandidates );
        if (numCandidates == 0) 
			goto cleanUp;
		
        UINT currentPageIndex = 0;
        UINT currentPage = 0;
        UINT currentSelection = 0;
        UINT numPages = 0;
        UINT *pIndexList = NULL;
        UINT numCandidatesOnPage = 0;

        m_CandidateListId = dwUIElementId;
       
       // For some strange reason, for Japanese, GetPageIndex leads to whacky results. The number of pages returned is usually
       // a lot more than expected.

       // Here we try to determine if the candidate list needs to be repositioned or not. If we are going through different rows of 
       // same candidate list, there is no need to reposition the list. If it's a different candidate list, then we should reposition.
       // We check different flags obtained from GetUpdatedFlags function to make this determination.
		((ITfCandidateListUIElement*)pElement)->GetPageIndex(NULL, 0, &numPages);

		DWORD pFlags = 0;
		((ITfCandidateListUIElement*)pElement)->GetUpdatedFlags(&pFlags);
        
        bool flagCount = (pFlags & TF_CLUIE_COUNT) == 0 ? false: true;
        bool flagSelection = (pFlags & TF_CLUIE_SELECTION) == 0 ? false: true;
        bool flaString = (pFlags & TF_CLUIE_STRING) == 0 ? false: true;
        bool flagPageIndex = (pFlags & TF_CLUIE_PAGEINDEX) == 0 ? false: true;
        bool flagCurPage = (pFlags & TF_CLUIE_CURRENTPAGE) == 0 ? false: true;
        
        NOTE_UNUSED2(flagPageIndex, flagCount);
        if ((flagSelection || flagCurPage) && (!flaString))
        {
            m_bNeedRepositioning = false;
        }
        else
        {
            m_bNeedRepositioning = true;
        }

        if (numPages != 0)
        {
            pIndexList = new UINT[numPages+1];
            // Get the starting index of every page in pIndexList.
            pCandidate->GetPageIndex( pIndexList, numPages, &numPages );
        }
        if (numPages > 1)
        {
            numCandidatesOnPage = pIndexList[1] - pIndexList[0];
        }
        else
        {
            numCandidatesOnPage = numCandidates; //Only one page
        }
      
        // Get the page we are on currently
        pCandidate->GetCurrentPage( &currentPageIndex );

        // What is the currently selected row?
        pCandidate->GetSelection( &currentSelection );
        
        currentPage = (int)currentPageIndex;
        UINT pageStart = pIndexList[currentPage];
        m_CurrentPageStart = pageStart;

        if ((m_pIMEVista->m_IMETag & GFxIME_Jp_Flag) || (m_pIMEVista->m_IMETag & GFxIME_Ch_Trad_Flag))
        {
            // Obtain the number of candidates on the current page. This has to be done
            // since numCandidatesOnPage doesn't give the correct number of candidates
            // on the last page. 
            if (numCandidates < numCandidatesOnPage)
			{
                globalNumCandidates = numCandidates;
			}
            else
            {
                // If we are on the last page. Calculate the number of rows on the last page. This information can't reliably
                // be obtained from pIndexList.
                if ((currentPage+1)*numCandidatesOnPage > numCandidates)
				{
                    globalNumCandidates = numCandidates%numCandidatesOnPage; // Last Page
				}
                else
				{
					// If we are not on the last page
                    globalNumCandidates = numCandidatesOnPage;
				}
            }
        }
        else
        {
            if (currentPageIndex == numPages - 1) // we are on last page
                globalNumCandidates = numCandidates - pageStart;
            else
                globalNumCandidates = pIndexList[currentPageIndex+1] - pIndexList[currentPageIndex];
        }

		// For google pinyin, the page indicies are not reported correctly in pIndexList. For Sogou pinyin IME, candidate list TSF notifications
		// are never sent. 
		if (m_pIMEVista->m_IMETag & GFxIME_Ch_ThirdParty_Flag)
		{
			globalNumCandidates = numCandidates;
			pageStart = 0;
		}

		delete [] pIndexList;

		if ((int)currentSelection >= 0)
		{
			// offset from beginning of current page
			m_pIMEVista->m_pCandidateListBox->SetSelectedItemIndex(currentSelection - pageStart, true); 
		}

		int nNumNullStrings = 0;
        m_pIMEVista->m_pCandidateListBox->RemoveAllListItems();
        for (unsigned int i = 0; i < globalNumCandidates; i++)
		{
			// Just obtain the strings for the current page- not the entire list
			 wchar_t* pWideStr = NULL;
			((ITfCandidateListUIElement*)pElement)->GetString( i+pageStart, &pWideStr );
			if ( pWideStr )
			{
				CandidateListItem *pItem  = new CandidateListItem( pWideStr );
				m_pIMEVista->m_pCandidateListBox->AddListItem( pItem );
			}
			else
			{
				nNumNullStrings++;	
			}
		}
       
		// This needs to be done since for chinese simplified pinyin IME, globalNumCandidates might not be correct since 
		// GetPageIndex returns incorrect results. Hence, we have to subtract the number of bad strings from the total 
		// number of strings on the page which might be incorrect.
        globalNumCandidates = globalNumCandidates - nNumNullStrings;
        m_NumCandidates = globalNumCandidates;
         
        m_pIMEVista->m_pCandidateListBox->UIRefreshView();
		m_pIMEVista->m_bDrawCandidateList = true;

		// For this IME, all TSF notifications arrive after windows messages, so candidate list repositioning must be
		// done here. Note that candidate list visibility is set during repositioning (look at actionscript code), so 
		// repositioning is vital, otherwise list will not show up
		if (m_pIMEVista->m_IMETag == GFxIME_Jp_2010)
		{
			m_pIMEVista->RepositionCandidateList(-m_pIMEVista->m_nClausePosition);
			// no need since list is already drawn
			m_pIMEVista->m_bDrawCandidateList = false;
		}
		
		pCandidate->Release();
		pCandidate = NULL;
    }

cleanUp:
	if (pUIElementMgr)
		pUIElementMgr->Release();
    if (pElement)
		pElement->Release();
    return S_OK;
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
STDAPI CTSF::EndUIElement(DWORD dwUIElementId)
{
	if (m_pIMEVista->m_IMETag == GFxIME_NotSupported || m_pIMEVista->m_IMETag == GFxIME_NotSet)
		return S_OK;

	if ( !m_pIMEVista->IsTextFieldFocused() )
	{
		return S_OK;
	}

	m_pIMEVista->m_nCandidateWindowRefCount--;

	if (dwUIElementId == m_CandidateListId)
	{
		m_pIMEVista->Invoke_RemoveList();
		m_pIMEVista->m_pCandidateListBox->RemoveAllListItems();
		m_bCandidateWindowFlag = false;
		m_CandidateListId = (DWORD)-1;
	}

	if (dwUIElementId == m_ReadingWindowId)
	{
		m_pIMEVista->Invoke_RemoveInputWindow();
		m_pIMEVista->m_ReadingStringBuffer = L"";
		m_ReadingWindowId = (DWORD)-1;
	}

	if (m_pIMEVista->m_nCandidateWindowRefCount == 0 && m_pIMEVista->m_bNeedToDisableIME)
	{
		m_pIMEVista->OnEnableIME(false);
	}

    return S_OK;
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
bool CTSF::AbortCandidateList()
{
	ITfUIElementMgr *pUIElementMgr = NULL;
	ITfUIElement *pElement = NULL;
	ITfCandidateListUIElementBehavior *pCandListUIElementBehavior = NULL;
	
	if ( !m_pThreadMgr )
		return false;

	bool bRet = false;

	if (FAILED( m_pThreadMgr->QueryInterface(IID_ITfUIElementMgr_GFx, (void **)&pUIElementMgr)))
		goto cleanUp;

	HRESULT hr = pUIElementMgr->GetUIElement( m_dwCandListId, &pElement);
	if (SUCCEEDED(hr) && pElement)
	{
		if (FAILED(pElement->QueryInterface(IID_ITfCandidateListUIElementBehavior_GFx, (void **)&pCandListUIElementBehavior)))
			goto cleanUp;

		if (pCandListUIElementBehavior)
		{
			pCandListUIElementBehavior->Abort();
			bRet = true;
		}
	}
	
	m_dwCandListId = (DWORD)-1;

cleanUp:
	if (pUIElementMgr)
		pUIElementMgr->Release();
	if (pElement)
		pElement->Release();
	if (pCandListUIElementBehavior)
		pCandListUIElementBehavior->Release();

	return bRet;
}

//----------------------------------------------------------------------------
// Sink called by the framework just before the first context is pushed onto
// a document.
//----------------------------------------------------------------------------
STDAPI CTSF::OnInitDocumentMgr(ITfDocumentMgr *pDocMgr)
{
    NOTE_UNUSED(pDocMgr);

    return S_OK;
}

//----------------------------------------------------------------------------
// Sink called by the framework just after the last context is popped off a
// document.
//----------------------------------------------------------------------------
STDAPI CTSF::OnUninitDocumentMgr(ITfDocumentMgr *pDocMgr)
{
    NOTE_UNUSED(pDocMgr);

    return S_OK;
}

//----------------------------------------------------------------------------
// Sink called by the framework when focus changes from one document to
// another.  Either document may be NULL, meaning previously there was no
// focus document, or now no document holds the input focus.
//----------------------------------------------------------------------------
STDAPI CTSF::OnSetFocus(ITfDocumentMgr *pDocMgr, ITfDocumentMgr *pDocMgrPrevFocus)
{
    NOTE_UNUSED2(pDocMgr, pDocMgrPrevFocus);

    return S_OK;
}

//----------------------------------------------------------------------------
// Sink called by the framework when a context is pushed.
//----------------------------------------------------------------------------
STDAPI CTSF::OnPushContext(ITfContext *pContext)
{
    NOTE_UNUSED(pContext);

    return S_OK;
}

//----------------------------------------------------------------------------
// Sink called by the framework when a context is popped.
//----------------------------------------------------------------------------
STDAPI CTSF::OnPopContext(ITfContext *pContext)
{
    NOTE_UNUSED(pContext);

    return S_OK;
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
HRESULT CTSF::GetDispAttrFromGUID(GUID* pguid, TF_DISPLAYATTRIBUTE *pdispAttr)
{
	HRESULT     hr;

	// Create the display attribute manager. 
	ITfDisplayAttributeMgr *pdispMgr = NULL;
	hr = Plat_RequireLoadCOM()->pCoCreateInstance(  CLSID_TF_DisplayAttributeMgr,
		NULL, 
		CLSCTX_INPROC_SERVER, 
		IID_ITfDisplayAttributeMgr, 
		(LPVOID*)&pdispMgr);
	if (FAILED(hr))
	{
		return hr;
	}

	// Get the display attribute info object for this attribute.
	ITfDisplayAttributeInfo *pdispInfo = NULL;
	hr = pdispMgr->GetDisplayAttributeInfo(*pguid, &pdispInfo, NULL);
	if(SUCCEEDED(hr))
	{
		//Get the display attribute info. 
		hr = pdispInfo->GetAttributeInfo(pdispAttr);
		pdispInfo->Release();
	}
	else
	{
		hr = E_FAIL;
	}

	pdispMgr->Release();

	return hr;
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
HRESULT CTSF::GetDispAttrFromRange( ITfContext *pContext, ITfRange *pRange, TfEditCookie ec, TF_DISPLAYATTRIBUTE *pDispAttr)
{
    HRESULT     hr;
    
    PlatCOMFunctions_t *pCOM = Plat_RequireLoadCOM();

    //Create the category manager. 
    ITfCategoryMgr  *pCategoryMgr = NULL;
    hr = pCOM->pCoCreateInstance( CLSID_TF_CategoryMgr,
                                  NULL, 
                                  CLSCTX_INPROC_SERVER, 
                                  IID_ITfCategoryMgr, 
                                  (LPVOID*)&pCategoryMgr );
    if ( FAILED( hr ) )
    {
        return hr;
    }

    //Create the display attribute manager. 
    ITfDisplayAttributeMgr *pDispMgr = NULL;
    hr = pCOM->pCoCreateInstance( CLSID_TF_DisplayAttributeMgr,
                                  NULL, 
                                  CLSCTX_INPROC_SERVER, 
                                  IID_ITfDisplayAttributeMgr, 
                                  (LPVOID*)&pDispMgr );
     if ( FAILED( hr ) )
    {
        pCategoryMgr->Release();
        return hr;
    }
    
    PlatOleAutFunctions_t *pOleAut = Plat_RequireLoadOleAut();

    //Get the display attribute property. 
    ITfProperty *pProp = NULL;
    hr = pContext->GetProperty(GUID_PROP_ATTRIBUTE, &pProp);
    if (SUCCEEDED(hr))
    {
        VARIANT var;

        pOleAut->pVariantInit(&var);
        hr = pProp->GetValue(ec, pRange, &var);
        if (S_OK == hr)  //Returns S_FALSE if the range is not completely covered by the property.  
        {
            if (VT_I4 == var.vt)
            {
                // The property is a guidatom. 
                GUID    guid;

                // Convert the guidatom into a GUID. 
                hr = pCategoryMgr->GetGUID((TfGuidAtom)var.lVal, &guid);
                if(SUCCEEDED(hr))
                {
                    ITfDisplayAttributeInfo *pDispInfo;

                    //Get the display attribute info object for this attribute. 
                    hr = pDispMgr->GetDisplayAttributeInfo(guid, &pDispInfo, NULL);
                    if(SUCCEEDED(hr))
                    {
                        //Get the display attribute info. 
                        hr = pDispInfo->GetAttributeInfo(pDispAttr);

                        pDispInfo->Release();
                    }
                }
            }
            else
            {
                //An error occurred; GUID_PROP_ATTRIBUTE must always be VT_I4. 
                hr = E_FAIL;
            }
            pOleAut->pVariantClear(&var);
        }

		pProp->Release();
    }

    pCategoryMgr->Release();
    pDispMgr->Release();

    return hr;
}
