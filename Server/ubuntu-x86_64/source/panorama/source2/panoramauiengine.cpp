//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================
//
//==================================================================================================

#if defined( DX_TO_GL_ABSTRACTION )
#include "togl/rendermechanism.h"
#endif

#include "stdafx.h"

#include "resourcesystem/iresourcesystem.h"

#include "panorama/source2/ipanoramaui.h"
#include "panoramauiengine.h"
#include "inputsystem/InputEnums.h"
#include "igameevents.h"

#include "panorama/panorama.h"
#include "panorama/iuiengine.h"
#include "panorama/uijsregistration.h"
#if defined( SOURCE2_PANORAMA )
#include "uitoplevelwindowsource2.h"
#endif
#if defined( PANORAMA_USE_S1WRAPPER )
#include "engine/IEngineSound.h"
// There is a complete clustermess about how S1 engine can be accessed from panorama.dll
// adding a few #defines to prevent including headers that cause compile errors, this is a total hack
#define RESOURCESTREAM_H
#define RESOURCEFILE_H
#define RESOURCETYPE_H
#include "cdll_int.h"
#endif

#include "iimemanager.h"
#include "resourcesystem/iresourcesystem.h"

// NOTE: This must be the last file included!!!
#include "tier0/memdbgon.h"

#if defined( DX_TO_GL_ABSTRACTION )
// Placed here so inlines placed in dxabstract.h can access gGL
COpenGLEntryPoints *gGL = NULL;
#endif



CPanoramaUIEngine s_PanoramaUIEngine;
CPanoramaUIEngine *g_pPanoramaUIEngineImpl = &s_PanoramaUIEngine;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CPanoramaUIEngine, IPanoramaUIEngine, PANORAMAUI_ENGINE_INTERFACE_VERSION, s_PanoramaUIEngine )

#ifndef PANORAMA_USE_S1WRAPPER

DEFINE_LOGGING_CHANNEL_NO_TAGS( LOG_PANORAMA, "Panorama", 0, LV_DEFAULT, Color( 75, 245, 200, 255 ) );
DEFINE_LOGGING_CHANNEL_NO_TAGS( LOG_PANORAMA_SCRIPT, "PanoramaScript", 0, LV_DEFAULT, Color( 200, 255, 255, 255 ) );

#endif

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
static AppSystemInfo_t s_pDependencies[] =
{
	{ "panorama_text_pango", PANORAMA_TEXT_SERVICES_INTERFACE_VERSION },
#if ( defined( PLATFORM_WINDOWS_PC ) )
	{ "imemanager", IMEMANAGER_INTERFACE_VERSION },
#endif
	{ NULL, NULL }
};

//-------------------------------------------------------------------------
// Constructor
//-------------------------------------------------------------------------
CPanoramaUIEngine::CPanoramaUIEngine()
{
	m_pUIEngine = NULL;
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
const AppSystemInfo_t *CPanoramaUIEngine::GetDependencies()
{
	return s_pDependencies;
}

//-------------------------------------------------------------------------
// Connect interfaces
//-------------------------------------------------------------------------
bool CPanoramaUIEngine::Connect( CreateInterfaceFn factory )
{
	if ( !factory )
	{
		return false;
	}

	if ( !BaseClass::Connect( factory ) )
	{
		return false;
	}

#if defined ( DX_TO_GL_ABSTRACTION )
#if defined( ALLOW_TEXT_MODE )
	static const bool cbTextMode = CommandLine()->HasParm( "-textmode" );
#else
	static const bool cbTextMode = false;
#endif

	if ( !cbTextMode && !gGL )
	{
		gGL = ToGLConnectLibraries( factory );
		gGL = GetOpenGLEntryPoints(0);
	}
#endif


#if ( defined( PLATFORM_WINDOWS_PC ) )
	g_pIMEManager = ( IIMEManager* )factory( IMEMANAGER_INTERFACE_VERSION, NULL );
	Assert( g_pIMEManager );
#endif

#if defined( PANORAMA_USE_S1WRAPPER )
	g_pEnginesound = ( IEngineSound* )factory( IENGINESOUND_CLIENT_INTERFACE_VERSION, NULL );
	g_pSoundEmitterSystemBase = (ISoundEmitterSystemBase *)factory( SOUNDEMITTERSYSTEM_INTERFACE_VERSION, NULL );
	g_pEngineclient = ( IVEngineClient* ) factory( VENGINE_CLIENT_INTERFACE_VERSION, NULL );
#endif
	m_pShaderDeviceMgr = (IShaderDeviceMgr*)factory( SHADER_DEVICE_MGR_INTERFACE_VERSION, NULL );


	g_IUITextServices = ( panorama::IUITextServices * ) factory( PANORAMA_TEXT_SERVICES_INTERFACE_VERSION, NULL );

	// Initialize the console variables.
	ConVar_Register();

#if defined( PANORAMA_USE_S1WRAPPER )
	MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f );
#endif




	return true;
}


//-------------------------------------------------------------------------
// Disconnect interfaces
//-------------------------------------------------------------------------
void CPanoramaUIEngine::Disconnect()
{
	ShutdownUIEngine();
	BaseClass::Disconnect();
}


//-------------------------------------------------------------------------
// Query interfaces
//-------------------------------------------------------------------------
void *CPanoramaUIEngine::QueryInterface( const char *pInterfaceName )
{
	if ( !V_strncmp( pInterfaceName, PANORAMAUI_ENGINE_INTERFACE_VERSION, V_strlen( PANORAMAUI_ENGINE_INTERFACE_VERSION ) + 1 ) )
	{
		return static_cast<IPanoramaUIEngine *>(this);
	}

	return BaseClass::QueryInterface( pInterfaceName );
}


#ifdef PANORAMA_USE_S1WRAPPER
bool launcher_keypair_verifymsg( const byte *pubData, int cbData, const byte *pubPublicKey, int cbPublicKey, const byte *pubSignature, int cbSignature );
extern bool( *g_pfnPanoramaResourceFileIntegrityCheck )( CUtlBuffer &bufFileData, void *&pvResourceData, int &numResourceBytes );
static bool PanoramaResourceFileIntegrityCheck( CUtlBuffer &bufFileData, void *&pvResourceData, int &numResourceBytes )
{
	// File layout expected:
	// 4-bytes of the header 'P' 'A' 'N' 1
	// 256 bytes of digest signature
	// [zipblob]
	// 1-byte of the version (same as in the header, but this byte is included in the digest)
	const int numSignatureDigestBytes = 512;
	if ( bufFileData.TellPut() < 64 + numSignatureDigestBytes )
		return false;

	char chHeader[4] = {'P', 'A', 'N', PANORAMA_ZIPFILE_VERSION };
	if ( memcmp( bufFileData.Base(), chHeader, 4 ) )
		return false;
	if ( ( ( char * ) bufFileData.Base() )[ bufFileData.TellPut() - 1 ] != PANORAMA_ZIPFILE_VERSION )
		return false;

	pvResourceData = ( ( char * ) bufFileData.Base() ) + 4 + numSignatureDigestBytes;
	numResourceBytes = bufFileData.TellPut() - 4 - numSignatureDigestBytes - 1;

	const byte CertificateData[] = {
#include "../../devtools/bin/certificates/panoramapack.public.h"
	};
	return launcher_keypair_verifymsg( ( const byte * ) pvResourceData, numResourceBytes + 1, CertificateData, sizeof( CertificateData ), ( const byte * )( ( ( char * ) bufFileData.Base() ) + 4 ), numSignatureDigestBytes );
}

static void PanoramaReleaseMaterialSystemObjects( int nChangeFlags )
{
	if ( g_pPanoramaUIEngineImpl && g_pPanoramaUIEngineImpl->m_pUIEngine )
		( ( panorama::CUIEngineSource2* )( g_pPanoramaUIEngineImpl->m_pUIEngine ) )->OnDeviceLost();
}

static void PanoramaRestoreMaterialSystemObjects( int nChangeFlags )
{
	if ( g_pPanoramaUIEngineImpl && g_pPanoramaUIEngineImpl->m_pUIEngine )
		( ( panorama::CUIEngineSource2* )( g_pPanoramaUIEngineImpl->m_pUIEngine ) )->OnDeviceRestored();
}
#endif


//-------------------------------------------------------------------------
// Init
//-------------------------------------------------------------------------
InitReturnVal_t CPanoramaUIEngine::Init()
{
	InitReturnVal_t nRetVal = BaseClass::Init();
	if ( nRetVal != INIT_OK )
	{
		return nRetVal;
	}

#ifdef PANORAMA_USE_S1WRAPPER
	// Make sure we configure the loader integrity checking before we initialize the resource system S1 wrapper
	g_pfnPanoramaResourceFileIntegrityCheck = PanoramaResourceFileIntegrityCheck;
	g_pResourceSystem->Init();

	if ( m_pShaderDeviceMgr )
	{
		m_pDeviceCallbacks = new DeviceCallbacks();
		m_pDeviceCallbacks->m_pPanoramaUIEngine = this;
		m_pShaderDeviceMgr->AddDeviceDependentObject( m_pDeviceCallbacks );
	}

	if ( g_pMaterialSystem )
	{
		g_pMaterialSystem->AddReleaseFunc( PanoramaReleaseMaterialSystemObjects );
		g_pMaterialSystem->AddRestoreFunc( PanoramaRestoreMaterialSystemObjects );
	}
	
#endif

	ConVar_Register();
	return nRetVal;
}


//-------------------------------------------------------------------------
// Create UI Engine object, remember to use ConnectPanoramaUIEngine() from panorama_client in your UI module
//-------------------------------------------------------------------------
bool CPanoramaUIEngine::SetupUIEngine()
{
	m_pUIEngine = panorama::CreatePanoramaUIEngineInternal();
	if ( !m_pUIEngine )
		return false;

	return true;
}


//-------------------------------------------------------------------------
// Shutdown previously created UI Engine
//-------------------------------------------------------------------------
void CPanoramaUIEngine::ShutdownUIEngine()
{
	if ( m_pUIEngine )
	{
		m_pUIEngine->Shutdown();
		delete m_pUIEngine;
		m_pUIEngine = NULL;
	}
}


//-------------------------------------------------------------------------
// Access UI engine, generally use global panorama::UIEngine() accessor instead as shorthand
//-------------------------------------------------------------------------
panorama::IUIEngine * CPanoramaUIEngine::AccessUIEngine()
{
	return m_pUIEngine;
}


//-------------------------------------------------------------------------
// Pass UI events to the UI engine
//-------------------------------------------------------------------------
bool CPanoramaUIEngine::HandleInputEvent( const InputEvent_t &event, const CUtlVector<panorama::IUIWindow *> &vecWindowInputOrder, bool bOnlyIfFocused )
{
	FOR_EACH_VEC( vecWindowInputOrder, i )
	{
		if ( ( (panorama::CTopLevelWindowSource2 *)vecWindowInputOrder[ i ] )->BIsVisible() &&  vecWindowInputOrder[ i ]->GetNumVisibleTopLevelPanels() > 0 &&
			( event.m_hWnd == PLAT_WINDOW_INVALID || event.m_hWnd == ( (panorama::CTopLevelWindowSource2 *)vecWindowInputOrder[ i ] )->GetPlatWindow() ) )
		{
			if ( !bOnlyIfFocused || vecWindowInputOrder[ i ]->BHasFocus() )
			{
				if ( ( (panorama::CTopLevelWindowSource2 *)vecWindowInputOrder[ i ] )->HandleInputEvent( event ) )
				{
					return true;
				}
				else if ( vecWindowInputOrder[ i ]->BForceConsumeKBAndMouseInputEvents() )
				{
					switch ( event.m_nType )
					{
					case IE_ButtonPressed:
					case IE_ButtonReleased:
					case IE_ButtonDoubleClicked:
					case IE_AnalogValueChanged:
					case IE_ButtonPressedRepeating:
					case IE_KeyTyped:
					case IE_KeyCodeTyped:
					case IE_KeyCodeReleased:
					case IE_LocateMouseClick:
						return true;
					}
				}
			}
		}
	}

	return false;
}


//-------------------------------------------------------------------------
// Expose UIInputEngine IME control.
//-------------------------------------------------------------------------
void CPanoramaUIEngine::SetIMEAllowed( bool bAllowed )
{
	if ( m_pUIEngine )
	{
		m_pUIEngine->UIInputEngine()->SetIMEAllowed( bAllowed );
	}
}


//-------------------------------------------------------------------------
// Clean up windows marked for close.
//-------------------------------------------------------------------------
int CPanoramaUIEngine::DeleteClosedWindows()
{
	if ( !m_pUIEngine )
	{
		return 0;
	}
	
#ifdef PANORAMA_USE_S1WRAPPER
	CUtlVector< panorama::IUIWindow* > vecWindows;
#else
	CUtlVectorFixedGrowableCompat< panorama::IUIWindow*, 8 > vecWindows;
#endif
	m_pUIEngine->GetWindowsForDebugger( vecWindows );
	
	int nClosed = 0;
	for ( int i = 0; i < vecWindows.Count(); i++ )
	{
		panorama::CTopLevelWindowSource2 *pWindow = (panorama::CTopLevelWindowSource2*)vecWindows[i];
		if ( pWindow->IsClosed() )
		{
			delete pWindow;
			nClosed++;
		}
	}
	return nClosed;
}


//-------------------------------------------------------------------------
// Shutdown interfaces
//-------------------------------------------------------------------------
void CPanoramaUIEngine::Shutdown( void )
{
	ShutdownUIEngine();
	ConVar_Unregister();
#ifdef PANORAMA_USE_S1WRAPPER
	g_pResourceSystem->Shutdown();
#endif

	BaseClass::Shutdown();
}


#ifdef PANORAMA_USE_S1WRAPPER
//////////////////////////////////////////////////////////////////////////
//
// Cryptography support code
//
//////////////////////////////////////////////////////////////////////////

#include "tier0/memdbgoff.h"

#ifdef Verify
#undef Verify
#endif

#define bswap_16 __bswap_16
#define bswap_64 __bswap_64

#include "cryptlib.h"
#include "rsa.h"

// Special usage here in the launcher without linking in tier0 tslist implementation
// list of auto-seeded RNG pointers
// these are very expensive to construct, so it makes sense to cache them

bool launcher_keypair_verifymsg( const byte *pubData, int cbData, const byte *pubPublicKey, int cbPublicKey, const byte *pubSignature, int cbSignature )
{
	try           // handle any exceptions crypto++ may throw
	{
		CryptoPP::StringSource stringSourcePublicKey( pubPublicKey, cbPublicKey, true );
		CryptoPP::RSASSA_PKCS1v15_SHA_Verifier pub( stringSourcePublicKey );

		return pub.VerifyMessage( pubData, cbData, pubSignature, cbSignature );
	}
	catch ( ... )
	{
	}
	return false;
}
#endif
