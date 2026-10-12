//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef UIENGINEWIN32_H
#define UIENGINEWIN32_H

#ifdef _WIN32
#pragma once
#endif

#include "panorama/iuiengine.h"
#include "uiengine.h"
#include "renderer/uirenderengine.h"
#include "renderer/iui3dsurface.h"
#include "text/uitextlayoutwin32.h"

#include <d3d10_1.h>
#include <d3d9.h>
#include <d2d1.h>
#include <DWrite.h>
#include <d3d11.h>

namespace panorama
{

class CTopLevelWindow;
class CTopLevelWindowOverlay;

// From DWrite.h
typedef HRESULT (WINAPI *DWriteCreateFactory_t)( DWRITE_FACTORY_TYPE factoryType, REFIID iid, IUnknown **factory );
extern DWriteCreateFactory_t g_DWriteCreateFactory;

// From d3d10.h
typedef HRESULT (WINAPI *D3D10CreateDevice1_t)( IDXGIAdapter *pAdapter, D3D10_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, D3D10_FEATURE_LEVEL1 HardwareLevel, UINT SDKVersion, ID3D10Device1 **ppDevice );
extern D3D10CreateDevice1_t g_D3D10CreateDevice1;
typedef HRESULT (APIENTRY *D3D10CreateEffectFromMemory_t)(void *pData, SIZE_T DataLength, UINT FXFlags, ID3D10Device *pDevice, ID3D10EffectPool *pEffectPool, ID3D10Effect **ppEffect);
extern D3D10CreateEffectFromMemory_t g_D3D10CreateEffectFromMemory;

// From d3d11.h
typedef HRESULT (WINAPI *D3D11CreateDevice_t)( IDXGIAdapter* pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, CONST D3D_FEATURE_LEVEL* pFeatureLevels, UINT FeatureLevels, UINT SDKVersion, ID3D11Device** ppDevice, D3D_FEATURE_LEVEL* pFeatureLevel, ID3D11DeviceContext** ppImmediateContext );
extern D3D11CreateDevice_t g_D3D11CreateDevice;

// From d3d9.h
typedef int (WINAPI *D3DPERF_BeginEvent_t)( D3DCOLOR col, LPCWSTR wszName );
extern D3DPERF_BeginEvent_t g_D3DPERF_BeginEvent;
typedef int (WINAPI *D3DPERF_EndEvent_t)( void );
extern D3DPERF_EndEvent_t g_D3DPERF_EndEvent;

// From d2d1.h
typedef HRESULT (WINAPI *D2D1CreateFactory_t)( D2D1_FACTORY_TYPE, REFIID, const D2D1_FACTORY_OPTIONS *, void** );
extern D2D1CreateFactory_t g_D2D1CreateFactory;

// From DXGI.h 1.1
typedef HRESULT (WINAPI *CreateDXGIFactory1_t)( REFIID riid, void **ppFactory );
extern CreateDXGIFactory1_t g_CreateDXGIFactory1;

//
// Win32 instance of UI engine.
//
class CUIEngineWin32 : public CUIEngine
{
public:

	CUIEngineWin32();
	~CUIEngineWin32();

	// Do one time initialization of the engine
	static bool BInitialize();

	// IUIEngine implementation
	void Shutdown();
	void RunPlatformFrame();
	IUIWindow *CreateNewWindow( const char *pchWindowTitle, uint32 width, uint32 height, ERenderTarget eRenderType, bool bFixedSurfaceSize, bool bEnforceWindowAspectRatio, bool bUseCustomMouseCursor, const char *pchTargetMonitor );
	virtual bool ShowNativeTopMostMessageBox( const char *pchMsg, const char *pchTitle, ENativeMessageBoxType_t eType );

protected:
	// Clipboard access
	virtual void CopyToClipboardImpl( const char *pchTextUTF8 ) OVERRIDE;
	virtual void GetClipboardTextImpl( CUtlString &strUTF8 ) const OVERRIDE;

public:
	// Input locale support
	virtual ELanguage GetCurrentInputLocale();
	virtual bool BHaveInputLocale( ELanguage language );
	virtual void SetInputLocale( ELanguage language );

	// GPU Information
	virtual bool BGetGPUInformation( char *rgchGPUDesc, uint32 unGPUDescBytes, uint64 *pulDedicatedGPUMem, uint64 *pulDedicatedSystemMem, uint64 *pulSharedMem );

	virtual uint32 GetWheelScrollLines() OVERRIDE;

	static bool BGetGPUInformationShared( char *rgchGPUDesc, uint32 unGPUDescBytes, uint64 *pulDedicatedGPUMem, uint64 *pulDedicatedSystemMem, uint64 *pulSharedMem );

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName );
	static void ValidateStatics( CValidator &validator );
#endif

private:
	bool m_bInShutdown;

	static CInterlockedInt s_nInitialized;
	static HMODULE sm_hModDirectWrite;
	static HMODULE sm_hModD3D10;
	static HMODULE sm_hModD3D11;

#ifdef _DEBUG
	static HMODULE sm_hModD3D9;
#endif
	static HMODULE sm_hModD2D1;
	static HMODULE sm_hModDXGI;

};

} // namespace panorama

#endif // UIENGINEWIN32_H
