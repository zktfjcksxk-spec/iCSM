//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx.h"
#include "uienginewin32.h"
#include <WinSock2.h>
#include <windows.h>
#include "renderer/d3d10d2dsurface.h"
#include "text/uitextlayoutwin32.h"
#include "uitoplevelwindowwin32.h"
#include "uitoplevelwindowoverlay.h"
#include "text/uifontfileloaderwin32.h"
#include "text/uitextserviceswin32.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

CInterlockedInt CUIEngineWin32::s_nInitialized = 0;
DWriteCreateFactory_t panorama::g_DWriteCreateFactory = NULL;
D3D10CreateDevice1_t panorama::g_D3D10CreateDevice1 = NULL;
D3D10CreateEffectFromMemory_t panorama::g_D3D10CreateEffectFromMemory = NULL;
D3D11CreateDevice_t panorama::g_D3D11CreateDevice = NULL;
#ifdef _DEBUG
D3DPERF_BeginEvent_t panorama::g_D3DPERF_BeginEvent = NULL;
D3DPERF_EndEvent_t panorama::g_D3DPERF_EndEvent = NULL;
#endif
D2D1CreateFactory_t panorama::g_D2D1CreateFactory = NULL;
CreateDXGIFactory1_t panorama::g_CreateDXGIFactory1 = NULL;

//-----------------------------------------------------------------------------
// Purpose:	Factory function for UI engine creation
//-----------------------------------------------------------------------------
HMODULE CUIEngineWin32::sm_hModD3D10 = NULL;
HMODULE CUIEngineWin32::sm_hModD3D11 = NULL;
#ifdef _DEBUG
HMODULE CUIEngineWin32::sm_hModD3D9 = NULL;
#endif
HMODULE CUIEngineWin32::sm_hModDirectWrite = NULL;
HMODULE CUIEngineWin32::sm_hModD2D1 = NULL;
HMODULE CUIEngineWin32::sm_hModDXGI = NULL;


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CUIEngineWin32::CUIEngineWin32() 
{
	m_bInShutdown = false;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CUIEngineWin32::~CUIEngineWin32()
{
	CUITextLayoutWin32::FreeGlobals();
	while( m_vecWindows.Count() )
	{
		// Deleting will end up calling back into us to remove from vector.
		delete m_vecWindows[m_vecWindows.Count()-1];
	}

	// May or may not have already shutdown
	Shutdown();

	s_nInitialized--;
	if ( s_nInitialized == 0 )
	{
		g_DWriteCreateFactory = NULL;

		if ( sm_hModDirectWrite )
		{
			FreeLibrary( sm_hModDirectWrite );
			sm_hModDirectWrite = NULL;
		}

		if ( sm_hModD3D10 )
		{
			FreeLibrary( sm_hModD3D10 );
			sm_hModD3D10 = NULL;
		}

		if ( sm_hModD3D11 )
		{
			FreeLibrary( sm_hModD3D11 );
			sm_hModD3D11 = NULL;
		}

#ifdef _DEBUG
		if ( sm_hModD3D9 )
		{
			FreeLibrary( sm_hModD3D9 );
			sm_hModD3D9 = NULL;
		}
#endif

		if ( sm_hModD2D1 )
		{
			FreeLibrary( sm_hModD2D1 );
			sm_hModD2D1 = NULL;
		}

		if ( sm_hModDXGI )
		{
			FreeLibrary( sm_hModDXGI );
			sm_hModDXGI = NULL;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Do one time initialization of the engine
//-----------------------------------------------------------------------------
bool CUIEngineWin32::BInitialize()
{
	sm_hModDirectWrite = LoadLibrary( "DWrite.dll" );
	sm_hModD3D10 = LoadLibrary( "d3d10_1.dll" );
#ifdef _DEBUG
	sm_hModD3D9 = LoadLibrary( "d3d9.dll" );
#endif
	sm_hModD2D1 = LoadLibrary( "d2d1.dll" );

	sm_hModDXGI = LoadLibrary( "dxgi.dll" );

	sm_hModD3D11 = LoadLibrary( "d3d11.dll" );

	s_nInitialized++;
	if ( s_nInitialized == 1 )
	{
		if ( sm_hModDirectWrite )
		{
			g_DWriteCreateFactory = (DWriteCreateFactory_t)::GetProcAddress( sm_hModDirectWrite, "DWriteCreateFactory" );
		}
		else
		{
			AssertMsg( false, "DWrite.dll missing, check your Windows/DirectX installation." );
			UIEngine()->ShowNativeTopMostMessageBox( "DWrite.dll missing, check your Windows/DirectX installation.", "Fatal Error", IUIEngine::k_ENativeMessageOk );
		}

		if ( sm_hModD3D10 )
		{
			g_D3D10CreateDevice1 = (D3D10CreateDevice1_t)::GetProcAddress( sm_hModD3D10, "D3D10CreateDevice1" );
			g_D3D10CreateEffectFromMemory = (D3D10CreateEffectFromMemory_t)::GetProcAddress( sm_hModD3D10, "D3D10CreateEffectFromMemory" );
		}
		else
		{
			AssertMsg( false, "d3d10_1.dll missing, check your Windows/DirectX installation." );
			UIEngine()->ShowNativeTopMostMessageBox( "d3d10_1.dll missing, check your Windows/DirectX installation.", "Fatal Error", IUIEngine::k_ENativeMessageOk );
		}

		if ( sm_hModD3D11 )
		{
			g_D3D11CreateDevice = (D3D11CreateDevice_t)::GetProcAddress( sm_hModD3D11, "D3D11CreateDevice" );
		}

#ifdef _DEBUG
		if ( sm_hModD3D9 )
		{
			g_D3DPERF_BeginEvent = (D3DPERF_BeginEvent_t)::GetProcAddress( sm_hModD3D9, "D3DPERF_BeginEvent" );
			g_D3DPERF_EndEvent = (D3DPERF_EndEvent_t)::GetProcAddress( sm_hModD3D9, "D3DPERF_EndEvent" );
		}
#endif

		if ( sm_hModD2D1 )
		{
			g_D2D1CreateFactory = (D2D1CreateFactory_t)::GetProcAddress( sm_hModD2D1, "D2D1CreateFactory" );
		}
		else
		{
			AssertMsg( false, "d2d1.dll missing, check your Windows/DirectX installation." );
			UIEngine()->ShowNativeTopMostMessageBox( "d2d1.dll missing, check your Windows/DirectX installation.", "Fatal Error", IUIEngine::k_ENativeMessageOk );
		}

		if ( sm_hModDXGI )
		{
			g_CreateDXGIFactory1 = (CreateDXGIFactory1_t)::GetProcAddress( sm_hModDXGI, "CreateDXGIFactory1" );
		}
		else
		{
			AssertMsg( false, "dxgi.dll, check your Windows/DirectX installation." );
			UIEngine()->ShowNativeTopMostMessageBox( "dxgi.dll missing, check your Windows/DirectX installation.", "Fatal Error", IUIEngine::k_ENativeMessageOk );
		}
	}

	if ( g_DWriteCreateFactory != NULL && g_D3D10CreateDevice1 != NULL && g_D3D10CreateEffectFromMemory != NULL && g_D2D1CreateFactory != NULL )
	{
#ifdef _DEBUG
		if ( !g_D3DPERF_BeginEvent || !g_D3DPERF_EndEvent )
			return false;
#endif
		if ( !g_IUITextServices )
			g_IUITextServices = new CUITextServicesWin32();

		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Account for windows scroll lines setting on mouse wheel
//-----------------------------------------------------------------------------
uint32 CUIEngineWin32::GetWheelScrollLines()
{
	static DWORD dwLastConfigUpdate = 0;
	static uint32 unScrollLines = 3;

	DWORD dwNow = ::GetTickCount();
	if( dwNow < dwLastConfigUpdate || dwNow - dwLastConfigUpdate > 1000 )
	{
		UINT unSystemScrollLines;
		if( SystemParametersInfoA( SPI_GETWHEELSCROLLLINES, 0, &unSystemScrollLines, FALSE ) )
		{
			if( (int)unSystemScrollLines > 0 )
				unScrollLines = unSystemScrollLines;
		}
	}

	return unScrollLines;
}


//-----------------------------------------------------------------------------
// Purpose: Create a new top level window
//-----------------------------------------------------------------------------
IUIWindow *CUIEngineWin32::CreateNewWindow( const char *pchWindowTitle, uint32 width, uint32 height, ERenderTarget eRenderType, bool bFixedSurfaceSize, bool bEnforceWindowAspectRatio, bool bUseCustomMouseCursor, const char *pchTargetMonitor )
{
	VPROF_BUDGET( "CUIEngineWin32::CreateNewWindow", VPROF_BUDGETGROUP_TENFOOT );
	CTopLevelWindow *pWindow = NULL;
	
	if ( IUIEngine::BIsOverlayTarget( eRenderType ) )
	{
		CTopLevelWindowOverlay * pWindowImpl = new CTopLevelWindowOverlay( this );
		if ( !pWindowImpl->BInitializeSurface( pchWindowTitle, width, height, eRenderType, bFixedSurfaceSize, bEnforceWindowAspectRatio, bUseCustomMouseCursor, pchTargetMonitor ) )
		{
			delete pWindowImpl;
			return NULL;
		}
		pWindow = pWindowImpl;
	}
	else if ( eRenderType == IUIEngine::k_ERenderToOpenVROverlay )
	{
		AssertMsg( false, "OpenVROverlay windows must be created with CreateNewOpenVROverlayWindow" );
	}
	else
	{
		CTopLevelWindowWin32 *pWindowImpl = new CTopLevelWindowWin32( this );
		if ( !pWindowImpl->BInitializeSurface( pchWindowTitle, width, height, eRenderType, bFixedSurfaceSize, bEnforceWindowAspectRatio, bUseCustomMouseCursor, pchTargetMonitor ) )
		{
			delete pWindowImpl;
			return NULL;
		}

		pWindow = pWindowImpl;
	}
	
	if( !pWindow )
		return NULL;

	if ( !pWindow->FinishInitialization() )
	{
		delete pWindow;
		return NULL;
	}

	m_vecWindows.AddToTail( pWindow );
	return pWindow;
}


//-----------------------------------------------------------------------------
// Purpose: Run a frame, pumps input, paints, whatever.
//-----------------------------------------------------------------------------
void CUIEngineWin32::RunPlatformFrame()
{
	// Run Win32 input/message loop
	FOR_EACH_VEC( m_vecWindows, i )
	{
		m_vecWindows[i]->RunPlatformFrame();
	}

	CTopLevelWindowWin32::PumpMessageLoop();

	CUIEngine::RunPlatformFrame();
}


//-----------------------------------------------------------------------------
// Purpose: Shutdown the UI engine including the surface and window
//-----------------------------------------------------------------------------
void CUIEngineWin32::Shutdown()
{
	if ( m_bInShutdown )
		return;
	m_bInShutdown = true;

	CUIEngine::Shutdown();
	m_bInShutdown = false;
}


//-----------------------------------------------------------------------------
// Purpose: Shows a native message box.. usually for development
//-----------------------------------------------------------------------------
bool CUIEngineWin32::ShowNativeTopMostMessageBox( const char *pchMsg, const char *pchTitle, ENativeMessageBoxType_t eType )
{
	int nFlags = MB_OK;
	if ( eType == k_ENativeMessageOk )
		nFlags = MB_OK;
	else if ( eType == k_ENativeMessageYesNo )
		nFlags = MB_YESNO | MB_DEFBUTTON2;
	else
		AssertMsg( false, "Unknown ENativeMessageBoxType" );

	int nRet = ::MessageBoxA( NULL, pchMsg, pchTitle, nFlags | MB_TOPMOST );
	return ( nRet == IDYES || nRet == IDOK );
}


//-----------------------------------------------------------------------------
// Purpose: Copies text to the system clipboard
//-----------------------------------------------------------------------------
void CUIEngineWin32::CopyToClipboardImpl( const char *pchTextUTF8 )
{
	if ( ::OpenClipboard( NULL ) )
	{
		DbgVerify( EmptyClipboard() );

		int nMemBytes = V_UTF8ToUTF16( pchTextUTF8, NULL, 0 );
		HANDLE hMem = ::GlobalAlloc( GMEM_MOVEABLE, nMemBytes );
		if ( hMem )
		{
			void *ptr = ::GlobalLock( hMem );
			if ( ptr != NULL )
			{
				V_UTF8ToUTF16( pchTextUTF8, (uchar16*) ptr, nMemBytes );
				::GlobalUnlock( hMem );

				::SetClipboardData( CF_UNICODETEXT, hMem );
			}
		}

		::CloseClipboard();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Gets clipboard text as UTF8
//-----------------------------------------------------------------------------
void CUIEngineWin32::GetClipboardTextImpl( CUtlString &strUTF8 ) const
{
	if ( ::OpenClipboard( NULL ) )
	{
		HANDLE hMem = ::GetClipboardData( CF_UNICODETEXT );
		if ( hMem )
		{
			int lenBytes = ::GlobalSize( hMem );
			void *ptr = ::GlobalLock( hMem );
			if ( ptr != NULL )
			{
				CUtlVector<wchar_t> vecUTF16;
				vecUTF16.EnsureCapacity( lenBytes/sizeof(wchar_t) + 1 );
				vecUTF16.AddMultipleToTail( lenBytes/sizeof(wchar_t), (wchar_t*)ptr );
				vecUTF16.AddToTail( 0 );

				::GlobalUnlock( hMem );

				CStrAutoEncode strAuto( vecUTF16.Base() );
				strUTF8 = strAuto.ToString();
			}
		}
		::CloseClipboard();
	}
}


//-----------------------------------------------------------------------------
// Purpose: return the current input locale for this window as an ELanguage
// note: returns value for the current thread, so call on the UI thread
//-----------------------------------------------------------------------------
ELanguage CUIEngineWin32::GetCurrentInputLocale()
{
	HKL hkl = GetKeyboardLayout( 0 /* current thread */ );

	// HKL high word is a handle to the keyboard layout
	// HKL low word is the language ID (LID)
	LANGID lid = LOWORD( hkl );

	return GetLanguageFromCodeID( lid );
}


//-----------------------------------------------------------------------------
// Purpose: helper function, maps a language to a loaded HKL
//-----------------------------------------------------------------------------
static HKL HKLFromLanguage( ELanguage language )
{
	const int k_chklMax = 128;
	HKL rghkl[ k_chklMax ];

	int cLayouts = GetKeyboardLayoutList( k_chklMax, rghkl );

	if ( cLayouts == 0 )
		return false;

	// we return the first matching one
	for ( int iLayout = 0; iLayout < cLayouts; iLayout++ )
	{
		// HKL low word is the language ID (LID)
		LANGID lid = LOWORD( rghkl[ iLayout ] );
		if ( GetLanguageFromCodeID( lid ) == language )
			return rghkl[ iLayout ];
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: return whether we can switch to the specified input locale
//-----------------------------------------------------------------------------
bool CUIEngineWin32::BHaveInputLocale( ELanguage language )
{
	HKL hkl = HKLFromLanguage( language );
	return hkl != NULL;
}


//-----------------------------------------------------------------------------
// Purpose: set input locale to specified language
//-----------------------------------------------------------------------------
void CUIEngineWin32::SetInputLocale( ELanguage language )
{
	HKL hkl = HKLFromLanguage( language );
	if ( hkl != NULL )
		ActivateKeyboardLayout( hkl, 0 );
}


//-----------------------------------------------------------------------------
// Purpose: Get information about the users GPU
//-----------------------------------------------------------------------------
bool CUIEngineWin32::BGetGPUInformation( char *rgchGPUDesc, uint32 unGPUDescBytes, uint64 *pulDedicatedGPUMem, uint64 *pulDedicatedSystemMem, uint64 *pulSharedMem )
{
	return BGetGPUInformationShared( rgchGPUDesc, unGPUDescBytes, pulDedicatedGPUMem, pulDedicatedSystemMem, pulSharedMem );
}


//-----------------------------------------------------------------------------
// Purpose: Get information about the users GPU
//-----------------------------------------------------------------------------
bool CUIEngineWin32::BGetGPUInformationShared( char *rgchGPUDesc, uint32 unGPUDescBytes, uint64 *pulDedicatedGPUMem, uint64 *pulDedicatedSystemMem, uint64 *pulSharedMem )
{
	if ( rgchGPUDesc && unGPUDescBytes > 0 )
		rgchGPUDesc[0] = 0;

	if ( pulDedicatedGPUMem )
		*pulDedicatedGPUMem = 0;

	if ( pulDedicatedSystemMem )
		*pulDedicatedSystemMem = 0;

	if ( pulSharedMem )
		*pulSharedMem = 0;

	if ( s_nInitialized == 0 )
	{
		// bugbug jmccaskey - do something less stupid for ogl, BInitialize has other side effects like initing dwrite text services that have to get hacked around...

		// OGL case, init to get the global pointer to g_CreateDXGIFactory1
		BInitialize();
	}

	HRESULT hRes;

	if ( !g_CreateDXGIFactory1 )
		return false;

	IDXGIFactory1 *pFactory;
	hRes = g_CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)(&pFactory)); 
	if( FAILED (hRes) ) 
	{
		return false;
	}

	// The 1st adapter is the desktop, but might as well be a good citizen and grab them
	// all. This might be 'weird' if the non-primary adapter supports feature level 10.0
	// but the primary does not.
	const int nMaxAdapters = 8;
	int nAdapterCount = 0;
	IDXGIAdapter1* allAdapters[nMaxAdapters];	
	for ( int i = 0; i < nMaxAdapters; i++ )
	{
		hRes = pFactory->EnumAdapters1( i, &allAdapters[i] );
		if ( FAILED(hRes) )
			break;

		DXGI_ADAPTER_DESC desc;
		allAdapters[i]->GetDesc( &desc );
		nAdapterCount++;
	}

	if ( nAdapterCount == 0 )
	{
		SAFE_RELEASE( pFactory );
		return false;
	}

	UINT nDeviceFlags = D3D10_CREATE_DEVICE_BGRA_SUPPORT;

	static const D3D10_DRIVER_TYPE driverType[] = 
	{
		D3D10_DRIVER_TYPE_HARDWARE,
		D3D10_DRIVER_TYPE_WARP
	};

	static const D3D10_FEATURE_LEVEL1 levelAttempts[] = 
	{
		D3D10_FEATURE_LEVEL_10_0,
		D3D10_FEATURE_LEVEL_9_3,
		D3D10_FEATURE_LEVEL_9_2,
		D3D10_FEATURE_LEVEL_9_1,
	};

	ID3D10Device1 *pDevice = NULL;
	for ( int type = 0; type < V_ARRAYSIZE( driverType ) && !pDevice; type++ )
	{
		for ( int level = 0; level < V_ARRAYSIZE(levelAttempts) && !pDevice; level++ )
		{
			for ( int adapter = 0; adapter < nAdapterCount && !pDevice; adapter++ )
			{
				hRes = g_D3D10CreateDevice1( allAdapters[adapter], driverType[type], NULL,
					nDeviceFlags, levelAttempts[level], D3D10_1_SDK_VERSION, &pDevice );

				if ( FAILED( hRes ) )
				{
					pDevice = NULL;
				}
			} // each adapter
		} // each level
	} // each type

	for ( int i = 0; i < nAdapterCount; i++ )
	{
		SAFE_RELEASE( allAdapters[i] );
	}

	if ( !pDevice )
	{
		SAFE_RELEASE( pFactory );
		return false;
	}

	IDXGIDevice *pDXGIDevice = NULL;
	hRes = pDevice->QueryInterface( &pDXGIDevice );
	if( FAILED( hRes ) )
	{
		SAFE_RELEASE( pDevice );
		SAFE_RELEASE( pFactory );
		pDevice->Release();
		return false;
	}

	IDXGIAdapter *pDXGIAdapter = NULL;
	hRes = pDXGIDevice->GetAdapter( &pDXGIAdapter );
	
	// Done with the DXGI device
	SAFE_RELEASE( pDXGIDevice );

	if ( FAILED( hRes ) )
	{
		SAFE_RELEASE( pDevice );
		SAFE_RELEASE( pFactory );
		return false;
	}

	DXGI_ADAPTER_DESC desc;
	pDXGIAdapter->GetDesc( &desc );

	if ( pulDedicatedGPUMem )
		*pulDedicatedGPUMem = desc.DedicatedVideoMemory;
	if ( pulDedicatedSystemMem )
		*pulDedicatedSystemMem = desc.DedicatedSystemMemory;
	if ( pulSharedMem )
		*pulSharedMem = desc.SharedSystemMemory;

	CStrAutoEncode strAuto( desc.Description );
	V_strncpy( rgchGPUDesc, strAuto.ToString(), unGPUDescBytes );

	SAFE_RELEASE( pDXGIAdapter );
	SAFE_RELEASE( pDevice );
	SAFE_RELEASE( pFactory );

	return true;
}


#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: validate mem
//-----------------------------------------------------------------------------
void CUIEngineWin32::Validate( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE();

	CUIEngine::Validate( validator, pchName );
}

void CUIEngineWin32::ValidateStatics( CValidator &validator )
{
	CTopLevelWindowWin32::ValidateStatics( validator, "CTopLevelWindowWin32::ValidateStatics" );
	UIFontCollectionLoader::ValidateStatics( validator, "UIFontCollectionLoader::ValidateStatics" );
}
#endif


