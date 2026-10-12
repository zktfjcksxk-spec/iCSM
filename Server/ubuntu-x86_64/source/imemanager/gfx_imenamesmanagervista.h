/**************************************************************************

Filename    :   GFx_IMENamesManagerVista.h
Content     :   Overrides the functions used to obtain names of installed ime's and 
                activating ime's according to platform specific implementation.
Created     :   Oct 01, 2008
Authors     :   Ankur Mohan

Copyright   :   Copyright 2011 Autodesk, Inc. All Rights reserved.

Use of this software is subject to the terms of the Autodesk license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

**************************************************************************/

#ifndef _GFX_IMENAMESMANAGERVISTA_H_
#define _GFX_IMENAMESMANAGERVISTA_H_

#include "gfx_imeidmap.h"
#include <msctf.h>
#include "gfx_tsfuielementdef.h"

/*
WINBASEAPI
int
WINAPI
GetLocaleInfoEx(
               __in LCID     Locale,
               __in LCTYPE   LCType,
               __out_ecount_opt(cchData) LPWSTR  lpLCData,
               __in int      cchData);

*/
// 71c6e74e-0f28-11d8-a82a-00065b84435c
DEFINE_GUID(IID_ITfInputProcessorProfileActivationSink_GFx,
            0x71c6e74e, 0x0f28, 0x11d8, 0xa8, 0x2a, 0x00, 0x06, 0x5b, 0x84, 0x43, 0x5c);

// {CCF05DD8-4A87-11D7-A6E2-00065B84435C}
DEFINE_GUID(GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION_GFX,
            0xCCF05DD8, 0x4A87, 0x11D7, 0xa6, 0xe2, 0x00, 0x06, 0x5b, 0x84, 0x43, 0x5c);

class CIMENamesManagerVista : public CIMENamesManager, public ITfCompartmentEventSink, public ITfInputProcessorProfileActivationSink                
{      
public:
    CIMENamesManagerVista( GFxIMEManagerWin32 *pIMEManagerWin32 );
    ~CIMENamesManagerVista();
	
    // IUnknown
    STDMETHODIMP            QueryInterface( REFIID riid, void **ppvObj );
    STDMETHODIMP_(ULONG)    AddRef();
    STDMETHODIMP_(ULONG)    Release();

    // ITfInputProcessorProfileActivationSink
    STDMETHODIMP_(HRESULT)  OnActivated( DWORD dwProfileType, LANGID langid, REFCLSID rclsid, REFGUID catid, REFGUID guidProfile, HKL hkl, DWORD dwFlags);
    
    // ITfCompartmentEventSink
    STDMETHODIMP            OnChange( REFGUID refguid );

    virtual bool            QualifyIMENames();
    virtual void            ActivateIME( const wchar_t *pIMEName );
    virtual void            ActivateInputLanguage( const wchar_t *pInputLangName );
    virtual void            SetConversionMode( uint32 conversionParams = -1 );
    virtual void            HandleStatusWindowNotifications( const char *pCommand, const char *pArg );
	virtual void			OnInputLangChange( DWORD langId );
    virtual void			OnLangBarLoaded();

    void					CleanUp();
   
private:
	bool                    InitCompartmentSink();
    void                    UnInitCompartmentSink();
	void                    UnInstallProfileActivationSink();

	int32                   m_nRef;

    ITfThreadMgr            *m_pTFThreadMgr;
	ITfCompartment          *m_pTFCompartment;
	
    DWORD                   m_dwInputProcessorProfileEventSinkCookie;
    DWORD                   m_dwCompartmentSinkCookie;
};

#endif
