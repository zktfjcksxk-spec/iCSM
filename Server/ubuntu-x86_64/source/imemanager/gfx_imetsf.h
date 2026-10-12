/**************************************************************************

Filename    :   GFx_IMETSF.h
Content     :   Declaration of CTSF class- implementation of TSF related 
                functionality for IME on windows vista 
Created     :   Nov 5, 2007
Authors     :   Ankur Mohan

Copyright   :   Copyright 2011 Autodesk, Inc. All Rights reserved.

Use of this software is subject to the terms of the Autodesk license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

**************************************************************************/

#ifndef _GFX_IMETSF_H_
#define _GFX_IMETSF_H_

#include "imemanager_plat.h"

#include <stdio.h>
#include <olectl.h>
#include <msctf.h>

#ifndef __ITfUIElementSink_FWD_DEFINED__
#define TF_TMAE_UIELEMENTENABLEDONLY 0x00000004
#include "gfx_tsfuielementdef.h"
#endif

DEFINE_GUID(IID_ITfUIElementSink_GFx, 
            0xea1ea136, 0x19df, 0x11d7, 0xa6, 0xd2, 0x00, 0x06, 0x5b, 0x84, 0x43, 0x5c);

// "3e90ade3-7594-4cb0-bb58-69628f5f458c" 
DEFINE_GUID(IID_ITfThreadMgrEx_GFx,
            0x3e90ade3, 0x7594, 0x4cb0, 0xbb, 0x58, 0x69, 0x62, 0x8f, 0x5f, 0x45, 0x8c);

// 85fad185-58ce-497a-9460-355366b64b9a
DEFINE_GUID(IID_ITfCandidateListUIElementBehavior_GFx, 
            0x85fad185, 0x58ce, 0x497a, 0x94, 0x60, 0x35, 0x53, 0x66, 0xb6, 0x4b, 0x9a);

// ea1ea135-19df-11d7-a6d2-00065b84435c 
DEFINE_GUID(IID_ITfUIElementMgr_GFx,
            0xea1ea135, 0x19df, 0x11d7, 0xa6, 0xd2, 0x00, 0x06, 0x5b, 0x84, 0x43, 0x5c);

DEFINE_GUID(IID_ITfInputProcessorProfileMgr_GFx,
            0x71c6e74c, 0x0f28, 0x11d8, 0xa8, 0x2a, 0x00, 0x06, 0x5b, 0x84, 0x43, 0x5c);

// Forward Declaration
class GFxIMEVista;
class GFxIMEWin32Impl;

class CTSF : public ITfThreadMgrEventSink, 
             public ITfUIElementSink
{
public:
    CTSF( GFxIMEWin32Impl *pIME );
    virtual ~CTSF();

    // IUnknown
    STDMETHODIMP            QueryInterface(REFIID riid, void **ppvObj);
    STDMETHODIMP_(ULONG)    AddRef(void);
    STDMETHODIMP_(ULONG)    Release(void);

    // ITfThreadMgrEventSink
    STDMETHODIMP            OnInitDocumentMgr(ITfDocumentMgr *pDocMgr);
    STDMETHODIMP            OnUninitDocumentMgr(ITfDocumentMgr *pDocMgr);
    STDMETHODIMP            OnSetFocus(ITfDocumentMgr *pDocMgrFocus, ITfDocumentMgr *pDocMgrPrevFocus);
    STDMETHODIMP            OnPushContext(ITfContext *pContext);
    STDMETHODIMP            OnPopContext(ITfContext *pContext);

    // ITfUIElementSink
    STDMETHODIMP            BeginUIElement(DWORD dwUIElementId, BOOL* pbShow);
    STDMETHODIMP            UpdateUIElement(DWORD dwUIElementId);
    STDMETHODIMP            EndUIElement(DWORD dwUIElementId);
    
	// ITfContextOwnerCompositionSink
    STDMETHODIMP			OnStartComposition(ITfCompositionView *pComposition, BOOL *pfOk);
	STDMETHODIMP			OnUpdateComposition(ITfCompositionView *pComposition,ITfRange *pRangeNew);
	STDMETHODIMP			OnEndComposition(ITfCompositionView *pComposition);
    
	// ITfTextEditSink
//	STDMETHODIMP			OnEndEdit(ITfContext *pic, TfEditCookie ecReadOnly, ITfEditRecord *pEditRecord);


    bool                    Init();
	void					Shutdown();
   
    bool					SelectAndClose(uint32 index);
    uint32                  GetNumberOfCandidates() { return m_NumCandidates; }; 
    bool                    GetCandidateWindowFlag(){ return m_bCandidateWindowFlag; };
	bool					AbortCandidateList();

private:
	bool                    InitThreadMgrSink();
    bool                    InitUIElementSink();
    void                    UninitThreadMgrSink();
    void                    UnInitUIElementSink();
    void                    ReleaseInterfaces();

	HRESULT					GetDispAttrFromRange( ITfContext *pContext, ITfRange *pRange, TfEditCookie ec, TF_DISPLAYATTRIBUTE *pDispAttr);
	HRESULT					GetDispAttrFromGUID(GUID* pguid, TF_DISPLAYATTRIBUTE *pdispAttr);

    int32                   m_nRef;
	
    // Microsoft TSF related variables
    GFxIMEVista				*m_pIMEVista;
	ITfThreadMgr            *m_pThreadMgr;
    ITfThreadMgrEx          *m_pTimEx;
	TfClientId              m_ClientId;

    DWORD                   m_dwThreadMgrEventSinkCookie;
    DWORD                   m_dwUIElementSinkCookie;
	DWORD					m_dwTextEditSinkCookie;

    // Candidate List and Reading Window state variables
    DWORD                   m_CandidateListId;
    DWORD                   m_ReadingWindowId;
	DWORD					m_dwCandListId;

    uint32                  m_NumCandidates;
    bool                    m_bNeedRepositioning;
    bool                    m_bReadingWindowFlag;
    bool                    m_bCandidateWindowFlag;
    uint32                  m_CurrentPageStart;
};

#endif
