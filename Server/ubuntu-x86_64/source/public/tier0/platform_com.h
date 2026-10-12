//=========== (C) Copyright 2015 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Shares dynamic loading of COM/ole32/oleaut32 out of tier0.
//=============================================================================

#pragma once

#include "tier0/platform.h"

#ifdef PLATFORM_WINDOWS

#define WINDOWS_LEAN_AND_MEAN
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#include <windows.h>

struct PlatCOMFunctions_t
{
    HRESULT (STDAPICALLTYPE *pCoInitializeEx)(
        _In_opt_ LPVOID pvReserved,
        _In_     DWORD  dwCoInit );
    void (STDAPICALLTYPE *pCoUninitialize)(void);
    HRESULT (STDAPICALLTYPE *pCoCreateInstance)(
        _In_  REFCLSID  rclsid,
        _In_  LPUNKNOWN pUnkOuter,
        _In_  DWORD     dwClsContext,
        _In_  REFIID    riid,
        _Out_ LPVOID    *ppv);
    LPVOID (STDAPICALLTYPE *pCoTaskMemAlloc)(
        _In_ SIZE_T cb);
    LPVOID (STDAPICALLTYPE *pCoTaskMemRealloc)(
        _In_opt_ LPVOID pv,
        _In_     SIZE_T cb);
    void (STDAPICALLTYPE *pCoTaskMemFree)(
        _In_opt_ LPVOID pv);
};

// Load ole32 on demand, always returns non-NULL but
// pointers in PlatCOMFunctions_t can be NULL on failure.
PLATFORM_INTERFACE PlatCOMFunctions_t *Plat_LoadCOM();
// Return COM functions if loaded, otherwise NULL.
PLATFORM_INTERFACE PlatCOMFunctions_t *Plat_CheckCOM();
// FatalError if functions aren't loaded.
PLATFORM_INTERFACE PlatCOMFunctions_t *Plat_RequireCOM();

FORCEINLINE PlatCOMFunctions_t *Plat_RequireLoadCOM()
{
    Plat_LoadCOM();
    return Plat_RequireCOM();
}

PLATFORM_INTERFACE void Plat_UnloadCOM();

struct PlatOleAutFunctions_t
{
    void ( WINAPI *pVariantInit )( _Out_ VARIANTARG *pvarg );
    HRESULT ( WINAPI *pVariantClear )( _Inout_ VARIANTARG *pvarg );
    HRESULT ( WINAPI *pVariantCopy )( _Out_ VARIANTARG *pvargDest, _In_  const VARIANTARG *pvargSrc );
    BSTR ( WINAPI *pSysAllocString )( _In_opt_ const OLECHAR *psz );
    BSTR ( WINAPI *pSysAllocStringByteLen )( _In_opt_ LPCSTR psz, _In_ UINT len );
    BSTR ( WINAPI *pSysAllocStringLen )( _In_ const OLECHAR *strIn, _In_ UINT ui );
    void ( WINAPI *pSysFreeString )( _In_opt_ BSTR bstrString );
    INT ( WINAPI *pSysReAllocString )( _Inout_ BSTR *pbstr, _In_opt_ const OLECHAR *psz );
    INT ( WINAPI *pSysReAllocStringLen )( _Inout_ BSTR *pbstr, _In_opt_ const OLECHAR *psz, _In_ unsigned int len );
    UINT ( WINAPI *pSysStringByteLen )( _In_opt_ BSTR bstr );
    UINT ( WINAPI *pSysStringLen )( _In_opt_ BSTR bstr );
};

// Load oleaut32 on demand, always returns non-NULL but
// pointers in PlatOleAutFunctions_t can be NULL on failure.
PLATFORM_INTERFACE PlatOleAutFunctions_t *Plat_LoadOleAut();
// Return OleAut functions if loaded, otherwise NULL.
PLATFORM_INTERFACE PlatOleAutFunctions_t *Plat_CheckOleAut();
// FatalError if functions aren't loaded.
PLATFORM_INTERFACE PlatOleAutFunctions_t *Plat_RequireOleAut();

FORCEINLINE PlatOleAutFunctions_t *Plat_RequireLoadOleAut()
{
    Plat_LoadOleAut();
    return Plat_RequireOleAut();
}

PLATFORM_INTERFACE void Plat_UnloadOleAut();

#endif // #ifdef PLATFORM_WINDOWS
