//====== Copyright © 2014-2015, Valve Corporation, All rights reserved. =======
//
// Purpose: IPanoramaUIClient app system implementation
//
//=============================================================================

#ifdef PLATFORM_WINDOWS
#include "windows.h"
#endif

#include "interfaces/interfaces.h"
#include "filesystem.h"
#include "tier1/utldelegate.h"
#include "tier1/keyvalues.h"
#include "panorama/controls/panel2d.h"
#include "../panorama/controls/debug/debugger.h"
#include "panoramauiclient.h"
#include "panorama/uisettings.h"
#include "panorama/panoramatypes.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

CPanoramaUIClient s_PanoramaUIClient;
CPanoramaUIClient *g_pPanoramaUIClientImpl = &s_PanoramaUIClient;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CPanoramaUIClient, IPanoramaUIClient, PANORAMAUI_CLIENT_INTERFACE_VERSION, s_PanoramaUIClient )

// Trivial thunk class to virtualize access to a CDebugger.
class CPanoramaClientDebugger : public IPanoramaClientDebugger
{
public:
    CPanoramaClientDebugger( panorama::CDebugger *pDebugger )
    {
        m_pDebugger = pDebugger;
    }
    virtual ~CPanoramaClientDebugger()
    {
        delete m_pDebugger;
    }

	virtual void BeginInspect() OVERRIDE
    {
        return m_pDebugger->BeginInspect();
    }

	virtual void ForceEndInspect() OVERRIDE
	{
		return m_pDebugger->ForceEndInspect();
	}


	virtual float GetSplitterPosition() OVERRIDE
    {
        return m_pDebugger->GetSplitterPosition();
    }
    
	virtual void SetSplitterPosition( float flParentFlowValue ) OVERRIDE
    {
        m_pDebugger->SetSplitterPosition( flParentFlowValue );
    }

private:
    panorama::CDebugger *m_pDebugger;
};

// Dummy singleton to implement panorama's IUISettings class. Used to pass through the language correctly
class CPanoramaUISettings : public panorama::IUISettings
{
public:
	static CPanoramaUISettings &Get()
	{
		static CPanoramaUISettings s_instance;
		return s_instance;
	}

	void SetUILanguage( const char *pszLanguage ) { m_strLanguage = pszLanguage; }
	virtual const char *GetUILanguage() OVERRIDE { return m_strLanguage; }

	virtual void GetPreferredResolution( int &dxPreferred, int &dyPreferred ) OVERRIDE
	{
		dxPreferred = 1920;
		dyPreferred = 1080;
	}

	virtual void UpdateGamepadMappingHints( const char *pszConnectedMappings ) OVERRIDE{}
	virtual bool BAllowOSModalDialog() OVERRIDE { return false; }

	virtual void GetDefaultAudioDevice( CUtlString &strDevice, CUtlString &strPort, CUtlString &strProfile ) OVERRIDE { strDevice = strPort = strProfile = ""; }
	virtual void SetDefaultAudioDevice( const char *pchDevice ) OVERRIDE { Assert( false ); }
	virtual void SetDefaultAudioPort( const char *pchPort ) OVERRIDE { Assert( false ); }
	virtual void SetDefaultAudioProfile( const char *pchProfile ) OVERRIDE{ Assert( false ); }

	virtual void GetDefaultVoiceDevice( CUtlString &strDevice ) OVERRIDE { strDevice = ""; }
	virtual void SetDefaultVoiceDevice( const char *pchDevice ) OVERRIDE { Assert( false ); }

	virtual bool GetUseSystemAudioForVoice() OVERRIDE { return true; }
	virtual void SetUseSystemAudioForVoice( bool bUseSystemAudio ) OVERRIDE { Assert( false ); }

	virtual panorama::ETextInputHandlerType_t GetActiveTextInputHandlerType() const OVERRIDE{ return panorama::k_ETextInputHandlerType_DaisyWheel; }
	virtual void SetDefaultTextInputHandlerType( panorama::ETextInputHandlerType_t eType ) OVERRIDE { Assert( false ); }

	virtual unsigned int /* CTextInputDualTouch::EDualtouchSuggestionMode */ GetOnScreenKeyboardSuggestionMode() const OVERRIDE { return 0; };
	virtual void SetOnScreenKeyboardSuggestionMode( unsigned int /* CTextInputDualTouch::EDualtouchSuggestionMode */ eSuggestionMode ) { };

	virtual ELanguage GetDefaultInputLanguage() const { return k_Lang_English; }
	virtual void SetDefaultInputLanguage( ELanguage eLanguage ) OVERRIDE{ Assert( false ); }

	virtual int GetDualTouchTutorialCompletionCount() const { return 0; }
	virtual void OnDualTouchTutorialCompleted() OVERRIDE { Assert( false ); }

private:
	CUtlString m_strLanguage;
};

static AppSystemInfo_t s_pDependencies[] =
{
#ifdef PANORAMA_USE_S1WRAPPER
	{ "filesystem_stdio" DLL_EXT_STRING	, ASYNCFILESYSTEM_INTERFACE_VERSION },
	{ "panorama" DLL_EXT_STRING			, PANORAMAUI_ENGINE_INTERFACE_VERSION },
#else
	{ "panorama", PANORAMAUI_ENGINE_INTERFACE_VERSION },
#endif
	{ NULL, NULL }
};

CPanoramaUIClient::CPanoramaUIClient()
{
	// 7ls RequireKeyValuesSystem();
}

bool CPanoramaUIClient::Connect( CreateInterfaceFn factory )
{
	if ( !BaseClass::Connect( factory ) )
		return false;

	g_pPanoramaUIEngine = (IPanoramaUIEngine*)factory( PANORAMAUI_ENGINE_INTERFACE_VERSION, NULL );

	return true;

}

const AppSystemInfo_t *CPanoramaUIClient::GetDependencies()
{
	return s_pDependencies;
}

void CPanoramaUIClient::Disconnect()
{
	ShutdownUIEngine();
	BaseClass::Disconnect();
}

void *CPanoramaUIClient::QueryInterface( const char *pInterfaceName )
{
	if ( !V_strncmp( pInterfaceName, PANORAMAUI_CLIENT_INTERFACE_VERSION, V_strlen( PANORAMAUI_CLIENT_INTERFACE_VERSION ) + 1 ) )
	{
		return static_cast<IPanoramaUIClient *>(this);
	}

	return BaseClass::QueryInterface( pInterfaceName );
}

void CPanoramaUIClient::Shutdown( void )
{
	ShutdownUIEngine();
	BaseClass::Shutdown();
}

panorama::IUIEngine *CPanoramaUIClient::SetupUIEngine( const char *pszLanguage, PlatWindow_t hWindow )
{
	if ( !g_pPanoramaUIEngine )
	{
        Warning( "PanoramaUIEngine interface not set up\n");
		return NULL;
	}

	if ( !g_pPanoramaUIEngine->SetupUIEngine() )
	{
        Warning( "PanoramaUIEngine setup failed\n" );
		return NULL;
	}

    panorama::IUIEngine *pUIEngine = g_pPanoramaUIEngine->AccessUIEngine();
	ConnectPanoramaUIEngine( pUIEngine );

	if ( !panorama::UIEngine() )
	{
        Warning( "Panorama engine not set\n" );
		return NULL;
	}

	if ( !SetupNamedPaths() )
	{
        // Message already shown.
		return NULL;
	}
	
	pUIEngine->RegisterCustomFontPath( "panorama/fonts/" );

	// Startup subsystems and make UIEngine ready for real use
	CPanoramaUISettings::Get().SetUILanguage( pszLanguage );
	pUIEngine->StartupSubsystems( &CPanoramaUISettings::Get(), hWindow );

    return pUIEngine;
}

void CPanoramaUIClient::ShutdownUIEngine()
{
    if ( g_pPanoramaUIEngine )
    {
        g_pPanoramaUIEngine->ShutdownUIEngine();
    }
}

bool CPanoramaUIClient::HandleInputEvent( const InputEvent_t &event, const CUtlVector<panorama::IUIWindow *> &vecWindowInputOrder, bool bOnlyIfFocused )
{
    Assert( g_pPanoramaUIEngine );
	if ( g_pPanoramaUIEngine )
	{
        return g_pPanoramaUIEngine->HandleInputEvent( event, vecWindowInputOrder, bOnlyIfFocused );
    }

    return false;
}

panorama::IUIPanelClient *CPanoramaUIClient::CreatePanel2D( panorama::IUIWindow *pParent, const char *pID )
{
    panorama::CPanel2D *pPanel = new panorama::CPanel2D( pParent, pID );
    return static_cast<panorama::IUIPanelClient*>( pPanel );
}

IPanoramaClientDebugger *CPanoramaUIClient::CreateDebugger( panorama::IUIWindow *pParent, const char *pID )
{
    return new CPanoramaClientDebugger( new panorama::CDebugger( pParent, pID ) );
}

bool CPanoramaUIClient::SetupNamedPaths()
{
	const char *pConfigFilename = "panorama/panorama.cfg";

	KeyValues *pConfigKV = new KeyValues( "" );
	KeyValues::AutoDelete autoDeleteConfig( pConfigKV );


	// Load config KV

	bool bFailed = false;


#if DEVELOPMENT_ONLY
	// In development check for packed and signed panorama zip file,
	// if that file doesn't exist, then load from scattered files on local filesystem
	if ( !g_pFullFileSystem->FileExists( PANORAMA_ZIPFILE_NAME, NULL ) )
	{
		if ( !pConfigKV->LoadFromFile( g_pFullFileSystem, pConfigFilename, "GAME" ) )
		{
			bFailed = true;
		}
	}
	else
#endif
	{
		const char* pConfigFileContent = panorama::UIEngine()->UIFileSystem()->LoadFromPanZip( pConfigFilename );

		if ( pConfigFileContent )
		{
			bFailed = !pConfigKV->LoadFromBuffer( pConfigFilename, pConfigFileContent );
		}
		else
		{
			bFailed = true;
		}

	}
			
	if ( bFailed )
	{
		Warning( "Failed to load expected config file \"%s\"\n", pConfigFilename );
		return false;
	}
	
	KeyValues *pNamedPathsKV = pConfigKV->FindKey( "NamedPaths", false );
	if ( !pNamedPathsKV )
    {
        Warning( "Panorama configuration missing NamedPaths\n" );
		if ( g_pFullFileSystem )
		{
#ifndef PANORAMA_USE_S1WRAPPER
			g_pFullFileSystem->MarkContentCorrupt( false, pConfigFilename );
#endif
		}
		return false;
    }

	for ( KeyValues *pKV = pNamedPathsKV->GetFirstSubKey(); pKV; pKV = pKV->GetNextKey() )
	{
		CUtlString nameString = pKV->GetName();
		CUtlString valueString = pKV->GetString( "" );

		if ( !nameString.IsEmpty() )
		{
			nameString.ToLower();
			valueString.ToLower();
            panorama::UIEngine()->RegisterNamedLocalPath( CFmtStr( "{%s}", nameString.Get() ).Get(), valueString.Get(), false );
		}
	}

	return true;
}


//--------------------------------------------------------------------------------------------------
// STubs
//--------------------------------------------------------------------------------------------------

// void ConnectTier3Libraries( CreateInterfaceFn *pFactoryList, int nFactoryCount )
// {
// }
// 
// void DisconnectTier3Libraries()
// {
// }
