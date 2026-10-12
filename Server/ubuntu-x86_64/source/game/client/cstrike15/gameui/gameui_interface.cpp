//===== Copyright  1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Implements all the functions exported by the GameUI dll
//
// $NoKeywords: $
//===========================================================================//

#if !defined( _GAMECONSOLE ) && !defined( _OSX ) & !defined (LINUX)
#include <windows.h>
#endif
#include "cbase.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
// dgoodenough - io.h and direct.h don't exist on PS3
// PS3_BUILDFIX
// @wge Fix for OSX too.
#if !defined( _PS3 ) && !defined( _OSX ) && !defined (LINUX)
#include <io.h>
#endif
#include <tier0/dbg.h>
// @wge Fix for OSX too.
#if !defined( _PS3 ) && !defined( _OSX ) && !defined (LINUX)
#include <direct.h>
#endif

#ifdef SendMessage
#undef SendMessage
#endif
																
#include "filesystem.h"
#include "gameui_interface.h"
#include "gameui_matchmakingstatus.h"
#include "sys_utils.h"
#include "string.h"
#include "tier0/icommandline.h"

// interface to engine
#include "engineinterface.h"

#include "vguisystemmoduleloader.h"
#include "bitmap/tgaloader.h"

#include "gameconsole.h"
#include "cdkeyentrydialog.h"
#include "modinfo.h"
#include "game/client/IGameClientExports.h"
#include "materialsystem/imaterialsystem.h"
#include "matchmaking/imatchframework.h"
#include "ixboxsystem.h"
#include "iachievementmgr.h"
#include "IGameUIFuncs.h"
#include "ienginevgui.h"
#include "gameconsole.h"

// vgui2 interface
// note that GameUI project uses ..\vgui2\include, not ..\utils\vgui\include
#include "vgui/Cursor.h"
#include "tier1/keyvalues.h"
#include "vgui/ILocalize.h"
#include "vgui/IPanel.h"
#include "vgui/IScheme.h"
#include "vgui/IVGui.h"
#include "vgui/ISystem.h"
#include "vgui/ISurface.h"
#include "vgui_controls/Menu.h"
#include "vgui_controls/PHandle.h"
#include "tier3/tier3.h"
#include "matsys_controls/matsyscontrols.h"
#include "steam/steam_api.h"
#include "protocol.h"

#if defined( INCLUDE_SCALEFORM )
#include "loadingscreen_scaleform.h"
#endif

#include "GameUI/IGameUI.h"
#include "inputsystem/iinputsystem.h"

#include "cstrike15_item_inventory.h"


#ifdef PANORAMA_ENABLE
#include "uicomponents/uicomponent_gamestate.h"
#include "uicomponents/uicomponent_uitoolkit.h"
#include "uicomponents/uicomponent_gameinterface.h"
//#include "uicomponents/uicomponent_lobby.h"

#include "panorama/controls/panel2d.h"
#include "panorama/iuiengine.h"
#include "panorama/localization/ilocalize.h"
#include "panorama/panorama.h"
#include "panorama/iuisoundsystem.h"
#include "panorama/source2/ipanoramaui.h"
#include "panorama/ui_root.h"
#include "panorama/csgo_mainmenu.h"
#include "panorama/hud/csgo_hud.h"
#include "panorama/ui_popup_manager.h"
#include "panorama/csgo_dialog_variable_handlers.h"
#include "panorama/csgo_loadingscreen.h"
#include "panorama/csgo_avatarimage.h"

enum PanoramaEngineViewPriority_t
{
	PANORAMA_ENGINE_VIEW_PRIORITY_DEFAULT = 1000,
	// Room for game-specific views here
	PANORAMA_ENGINE_VIEW_PRIORITY_CONSOLE = 2000
};

DEFINE_PANORAMA_EVENT( CSGOLoadProgressChanged );

enum PanoramaGameViewPriority_t
{
	PANORAMA_GAME_VIEW_PRIORITY_HUD = PANORAMA_ENGINE_VIEW_PRIORITY_DEFAULT,
	PANORAMA_GAME_VIEW_PRIORITY_INTROMOVIE,
	PANORAMA_GAME_VIEW_PRIORITY_MAINMENU,
	PANORAMA_GAME_VIEW_PRIORITY_LOADINGSCREEN,
	PANORAMA_GAME_VIEW_PRIORITY_JS,
	PANORAMA_GAME_VIEW_PRIORITY_POPUPS
};
#endif

#if defined( SWARM_DLL )

#include "swarm/basemodpanel.h"
#include "swarm/basemodui.h"
typedef BaseModUI::CBaseModPanel UI_BASEMOD_PANEL_CLASS;
inline UI_BASEMOD_PANEL_CLASS & GetUiBaseModPanelClass() { return UI_BASEMOD_PANEL_CLASS::GetSingleton(); }
inline UI_BASEMOD_PANEL_CLASS & ConstructUiBaseModPanelClass() { return * new UI_BASEMOD_PANEL_CLASS(); }
class IMatchExtSwarm *g_pMatchExtSwarm = NULL;

#elif defined( PORTAL2_UITEST_DLL )

#include "portal2uitest/basemodpanel.h"
#include "portal2uitest/basemodui.h"
typedef BaseModUI::CBaseModPanel UI_BASEMOD_PANEL_CLASS;
inline UI_BASEMOD_PANEL_CLASS & GetUiBaseModPanelClass() { return UI_BASEMOD_PANEL_CLASS::GetSingleton(); }
inline UI_BASEMOD_PANEL_CLASS & ConstructUiBaseModPanelClass() { return * new UI_BASEMOD_PANEL_CLASS(); }
IMatchExtPortal2 g_MatchExtPortal2;
class IMatchExtPortal2 *g_pMatchExtPortal2 = &g_MatchExtPortal2;

#elif defined( CSTRIKE15 )

#include "basepanel.h"
#include "../gameui/cstrike15/cstrike15basepanel.h"


#if defined( INCLUDE_SCALEFORM )
#include "../Scaleform/messagebox_scaleform.h"
#endif

typedef CBaseModPanel UI_BASEMOD_PANEL_CLASS;
inline UI_BASEMOD_PANEL_CLASS & GetUiBaseModPanelClass() { return *BasePanel(); }
inline UI_BASEMOD_PANEL_CLASS & ConstructUiBaseModPanelClass() { return *BasePanelSingleton(); }

#else

#include "BasePanel.h"
typedef CBasePanel UI_BASEMOD_PANEL_CLASS;
inline UI_BASEMOD_PANEL_CLASS & GetUiBaseModPanelClass() { return *BasePanel(); }
inline UI_BASEMOD_PANEL_CLASS & ConstructUiBaseModPanelClass() { return *BasePanelSingleton(); }

#endif

// dgoodenough - select correct stub header based on current console
// PS3_BUILDFIX
#if defined( _PS3 )
#include "ps3/ps3_win32stubs.h"
#include <cell/sysmodule.h>
#endif
#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#endif

#include "tier0/dbg.h"
#include "engine/IEngineSound.h"
#include "gameui_util.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IEngineVGui *enginevguifuncs = NULL;
// dgoodenough - xonline only exists on the 360.  All uses of xonline have had their
// protection changed like this one
// PS3_BUILDFIX
// FIXME we will have to put in something for Playstation Home.
#if defined( _X360 )
IXOnline  *xonline = NULL;			// 360 only
#elif defined( _PS3 )
IPS3SaveRestoreToUI *ps3saveuiapi = NULL;
#endif
vgui::ISurface *enginesurfacefuncs = NULL;
IAchievementMgr *achievementmgr = NULL;

class CGameUI;
CGameUI *g_pGameUI = NULL;

vgui::VPANEL g_hLoadingBackgroundDialog = NULL;

static CGameUI g_GameUI;

#if defined( INCLUDE_SCALEFORM )
IScaleformUI* ScaleformUI()
{
	return g_pScaleformUI;
}
#endif


static IGameClientExports *g_pGameClientExports = NULL;
IGameClientExports *GameClientExports()
{
	return g_pGameClientExports;
}

//-----------------------------------------------------------------------------
// Purpose: singleton accessor
//-----------------------------------------------------------------------------
CGameUI &GameUI()
{
	return g_GameUI;
}

//-----------------------------------------------------------------------------
// Purpose: hack function to give the module loader access to the main panel handle
//			only used in VguiSystemModuleLoader
//-----------------------------------------------------------------------------
vgui::VPANEL GetGameUIBasePanel()
{
	return GetUiBaseModPanelClass().GetVPanel();
}

EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CGameUI, IGameUI, GAMEUI_INTERFACE_VERSION, g_GameUI);

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CGameUI::CGameUI()
{
	g_pGameUI = this;
	m_bTryingToLoadFriends = false;
	m_iFriendsLoadPauseFrames = 0;
	m_iGameIP = 0;
	m_iGameConnectionPort = 0;
	m_iGameQueryPort = 0;
	m_bActivatedUI = false;
	m_szPreviousStatusText[0] = 0;
	m_bIsConsoleUI = false;
	m_bHasSavedThisMenuSession = false;
	m_bOpenProgressOnStart = false;
	m_iPlayGameStartupSound = 0;
	m_nBackgroundMusicGUID = 0;
	m_bBackgroundMusicDesired = false;
	m_bBackgroundMusicAllowFinishTrack = false;
	m_nBackgroundMusicVersion = RandomInt( 1, MAX_BACKGROUND_MUSIC );
	m_flBackgroundMusicStopTime = -1.0;
	m_pMusicExtension = NULL;
	m_pPreviewMusicExtension = NULL;
	m_flMainMenuMusicVolume = -1;
	m_nMusicMinRestartDelay = -1;
	m_nMusicMaxRestartDelay = -1;

	m_flQuestAudioTimeEnd = 0;
	m_flMasterMusicVolumeSavedForMissionAudio = -1;
	m_flMenuMusicVolumeSavedForMissionAudio = -1;
	m_nQuestAudioGUID = 0;
	m_bPanoramaEnabled = false;
#ifdef PANORAMA_ENABLE
	m_CSGOGameUIState = CSGO_GAME_UI_STATE_INVALID;
	m_bFirstActivationForSession = true;
	m_bInLevelLoading = false;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CGameUI::~CGameUI()
{
	g_pGameUI = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Initialization
//-----------------------------------------------------------------------------
void CGameUI::Initialize( CreateInterfaceFn factory )
{
	MEM_ALLOC_CREDIT();
	ConnectTier1Libraries( &factory, 1 );
	ConnectTier2Libraries( &factory, 1 );
	ConVar_Register( FCVAR_CLIENTDLL );
	ConnectTier3Libraries( &factory, 1 );

	enginesound = (IEngineSound *)factory( IENGINESOUND_CLIENT_INTERFACE_VERSION, NULL );
	engine = (IVEngineClient *)factory( VENGINE_CLIENT_INTERFACE_VERSION, NULL );
#if defined( BINK_VIDEO )
	bik = (IBik*)factory( BIK_INTERFACE_VERSION, NULL );
#endif
#ifdef _PS3
	ps3saveuiapi = (IPS3SaveRestoreToUI*)factory( IPS3SAVEUIAPI_VERSION_STRING, NULL );
	cellSysmoduleLoadModule( CELL_SYSMODULE_SYSUTIL_USERINFO );
#endif

#if !defined( _GAMECONSOLE ) && !defined( NO_STEAM )
	SteamAPI_InitSafe();
	steamapicontext->Init();
#endif

	CGameUIConVarRef var( "gameui_xbox" );
	m_bIsConsoleUI = var.IsValid() && var.GetBool();

	vgui::VGui_InitInterfacesList( "GameUI", &factory, 1 );
	vgui::VGui_InitMatSysInterfacesList( "GameUI", &factory, 1 );

	// load localization file
#if !defined( CSTRIKE15 )
	g_pVGuiLocalize->AddFile( "Resource/gameui_%language%.txt", "GAME", true );
#endif

	// load mod info
	ModInfo().LoadCurrentGameInfo();

	// load localization file for kb_act.lst
	g_pVGuiLocalize->AddFile( "Resource/valve_%language%.txt", "GAME", true );

	bool bFailed = false;
	enginevguifuncs = (IEngineVGui *)factory( VENGINE_VGUI_VERSION, NULL );
	enginesurfacefuncs = (vgui::ISurface *)factory( VGUI_SURFACE_INTERFACE_VERSION, NULL );
	gameuifuncs = (IGameUIFuncs *)factory( VENGINE_GAMEUIFUNCS_VERSION, NULL );
	xboxsystem = (IXboxSystem *)factory( XBOXSYSTEM_INTERFACE_VERSION, NULL );

#ifdef PANORAMA_ENABLE
	m_bPanoramaEnabled = true;
	g_pPanoramaUIEngine = (IPanoramaUIEngine*)factory( PANORAMAUI_ENGINE_INTERFACE_VERSION, NULL );
#endif

	// dgoodenough - xonline only exists on the 360.
	// PS3_BUILDFIX
#ifdef _X360
	xonline = (IXOnline *)factory( XONLINE_INTERFACE_VERSION, NULL );
#endif
#ifdef SWARM_DLL
	g_pMatchExtSwarm = (IMatchExtSwarm *)factory( IMATCHEXT_SWARM_INTERFACE, NULL );
#endif
	bFailed = !enginesurfacefuncs || !gameuifuncs || !enginevguifuncs ||
		!xboxsystem ||
		// dgoodenough - xonline only exists on the 360.
		// PS3_BUILDFIX
#ifdef _X360
		!xonline ||
#endif
#ifdef SWARM_DLL
		!g_pMatchExtSwarm ||
#endif
		!g_pMatchFramework;

#ifdef PANORAMA_ENABLE
	if ( m_bPanoramaEnabled )
	{

		panorama::IUIEngine *pPanoramaUIEngine = g_pPanoramaUIEngine->AccessUIEngine();
		Assert( pPanoramaUIEngine );
		if ( !pPanoramaUIEngine )
		{
			bFailed = true;
		}
		else
		{
			ConnectPanoramaUIEngine( pPanoramaUIEngine );
		}
	}
#endif
	{
//		m_arrUiComponents.AddToTail( CUiComponent_FriendsList::GetInstance() );
//		m_arrUiComponents.AddToTail( CUiComponent_MyPersona::GetInstance() );
//		m_arrUiComponents.AddToTail( CUiComponent_PartyList::GetInstance() );
//		m_arrUiComponents.AddToTail( CUiComponent_Store::GetInstance() );
//		m_arrUiComponents.AddToTail( CUiComponent_Inventory::GetInstance() );
//		m_arrUiComponents.AddToTail( CUiComponent_Loadout::GetInstance() );
//		m_arrUiComponents.AddToTail( CUiComponent_Medals::GetInstance() );
//		m_arrUiComponents.AddToTail( CUiComponent_MatchInfo::GetInstance() );
//		m_arrUiComponents.AddToTail( CUiComponent_MatchList::GetInstance() );
//		m_arrUiComponents.AddToTail( CUiComponent_Predictions::GetInstance() );
//		m_arrUiComponents.AddToTail( CUiComponent_Fantasy::GetInstance() );
		//m_arrUiComponents.AddToTail(CUiComponent_ImageCache::GetInstance());
//		m_arrUiComponents.AddToTail( CUiComponent_CompetitiveMatch::GetInstance() );
//		m_arrUiComponents.AddToTail( CUiComponent_DataGraph::GetInstance() );
//		m_arrUiComponents.AddToTail( CUiComponent_GameTypes::GetInstance() );
//		m_arrUiComponents.AddToTail( CUiComponent_GlobalGame::GetInstance() );
//		m_arrUiComponents.AddToTail( CUiComponent_Leaderboards::GetInstance() );
//		m_arrUiComponents.AddToTail( CUiComponent_MatchDraft::GetInstance() );
//		m_arrUiComponents.AddToTail( CUiComponent_News::GetInstance() );
//		m_arrUiComponents.AddToTail( CUiComponent_SteamOverlay::GetInstance() );
//		m_arrUiComponents.AddToTail( CUiComponent_Streams::GetInstance() );
//		m_arrUiComponents.AddToTail( CUiComponent_Overwatch::GetInstance() );
//		m_arrUiComponents.AddToTail( CUiComponent_UGC::GetInstance() );
//		m_arrUiComponents.AddToTail( CUiComponent_ItemData::GetInstance() );
//		m_arrUiComponents.AddToTail( CUiComponent_Blog::GetInstance() );
#if defined( INCLUDE_SCALEFORM )
//		m_arrUiComponents.AddToTail( CUiComponent_EmbeddedStream::GetInstance() );
//		m_arrUiComponents.AddToTail( CUiComponent_Guides::GetInstance() );
#endif
//		m_arrUiComponents.AddToTail( CUiComponent_MatchStats::GetInstance() );
#ifdef PANORAMA_ENABLE
		m_arrUiComponents.AddToTail( CUiComponent_GameInterface::GetInstance() );
		if ( m_bPanoramaEnabled )
		{
			m_arrUiComponents.AddToTail( CUiComponent_UiToolkit::GetInstance() );
			//m_arrUiComponents.AddToTail( CUiComponent_Lobby::GetInstance() );
			m_arrUiComponents.AddToTail( CUiComponent_GameState::GetInstance() );
		}
#endif
	}

#ifdef PANORAMA_ENABLE
	if ( m_bPanoramaEnabled )
	{
		FOR_EACH_VEC( m_arrUiComponents, i )
		{
			m_arrUiComponents[i]->InstallPanoramaBindings();
		}
#if defined( OSX ) && defined( __aarch64__ ) && defined( NO_STEAM )
		extern void InstallNativeOfflinePanoramaBindings();
		InstallNativeOfflinePanoramaBindings();
#endif
	}
#endif

	if ( bFailed )
	{
		Error( "CGameUI::Initialize() failed to get necessary interfaces\n" );
	}

	// setup base panel
	UI_BASEMOD_PANEL_CLASS& factoryBasePanel = ConstructUiBaseModPanelClass(); // explicit singleton instantiation

	//  Ensure VGUI main menu is disabled when running with Panorama
	if ( !m_bPanoramaEnabled )
	{
		factoryBasePanel.SetBounds( 0, 0, 640, 480 );
		factoryBasePanel.SetPaintBorderEnabled( false );
		factoryBasePanel.SetPaintBackgroundEnabled( true );
		factoryBasePanel.SetPaintEnabled( true );
		factoryBasePanel.SetVisible( true );

		factoryBasePanel.SetMouseInputEnabled( IsPC() );
		// factoryBasePanel.SetKeyBoardInputEnabled( IsPC() );
		factoryBasePanel.SetKeyBoardInputEnabled( true );
	}

	vgui::VPANEL rootpanel = enginevguifuncs->GetPanel( PANEL_GAMEUIDLL );
	factoryBasePanel.SetParent( rootpanel );
}

void CGameUI::PostInit()
{
	if ( IsGameConsole() )
	{
		enginesound->PrecacheSound( "UI/buttonrollover.wav", true, true );
		enginesound->PrecacheSound( "UI/buttonclick.wav", true, true );
		enginesound->PrecacheSound( "UI/buttonclickrelease.wav", true, true );
		enginesound->PrecacheSound( "player/suit_denydevice.wav", true, true );

		enginesound->PrecacheSound( "UI/menu_accept.wav", true, true );
		enginesound->PrecacheSound( "UI/menu_focus.wav", true, true );
		enginesound->PrecacheSound( "UI/menu_invalid.wav", true, true );
		enginesound->PrecacheSound( "UI/menu_back.wav", true, true );
	}

#ifdef SWARM_DLL
	// to know once client dlls have been loaded
	BaseModUI::CUIGameData::Get()->OnGameUIPostInit();
#endif

#if defined ( PANORAMA_ENABLE )
	FOR_EACH_VEC( m_arrUiComponents, i )
	{
		m_arrUiComponents[ i ]->PostInit();
	}
#endif 
}

#ifdef PANORAMA_ENABLE
struct SCSGOPanoramaWindowInfo
{
	const char *pszID;
	PanoramaGameViewPriority_t nPriority;
	const char *pszLayoutFile;
};

static const SCSGOPanoramaWindowInfo k_windowInfos[] =
{
	// Must be loaded first in order to register new panel types with panorama from javascript before any
	// other panels (from other top level windows) get loaded
	{ "CSGOJsRegistration", PANORAMA_GAME_VIEW_PRIORITY_JS, "file://{resources}/layout/base_jsregistration.xml" },
	//NOTE: ORDER HERE DOES NOT MATTER - THAT IS DETERMINED BY PanoramaGameViewPriority_t
	{ "CSGOHud", PANORAMA_GAME_VIEW_PRIORITY_HUD, "file://{resources}/layout/hud/base_hud.xml" },
	{ "CSGOMainMenu", PANORAMA_GAME_VIEW_PRIORITY_MAINMENU, "file://{resources}/layout/base_mainmenu.xml" },
	{ "CSGOLoadingScreen", PANORAMA_GAME_VIEW_PRIORITY_LOADINGSCREEN, "file://{resources}/layout/base_loadingscreen.xml" },
	{ "CSGOIntroMovie", PANORAMA_GAME_VIEW_PRIORITY_INTROMOVIE, "file://{resources}/layout/base_intromovie.xml" },
	{ "CSGOPopups", PANORAMA_GAME_VIEW_PRIORITY_POPUPS, "file://{resources}/layout/base_globalpopups.xml" },
};
#endif

void CGameUI::CreateGameSpecificUI()
{
#ifdef PANORAMA_ENABLE
	if ( !m_bPanoramaEnabled )
		return;

	// already initialized
	if ( m_arrPanoramaWindows.Count() > 0 )
		return;

	// We explicitly DISALLOW starting jobs immediately because Panorama
	// implies V8, which uses exception handlers, which kill our
	// coroutines on at least Win32 platforms.
	
	// 7LS TODO
	//GCSDK::CJob::SetStartForceAllJobsDelayed( true );

	Assert( panorama::UIEngine() );

	// TODO: Once this gets more complex we'll probably want a more sophisticated registration setup

	// 7ls - Panorama script bindings not yet required
#ifdef CSGO_SOURCE2_UNSUPPORTED
	extern void CSGO_PanoramaScriptBindings_RegisterGlobalObjects();
	CSGO_PanoramaScriptBindings_RegisterGlobalObjects();
#endif	// CSGO_SOURCE2_UNSUPPORTED

	// Load our localization files
	panorama::UIEngine()->UILocalize()->BLoadLocalizationFile( "../../../csgo/resource/csgo" ); // old style csgo_english
	panorama::UIEngine()->UILocalize()->BLoadLocalizationFile( "csgo" ); // new hotness
	panorama::UIEngine()->UILocalize()->BLoadLocalizationFile( "../../../csgo/resource/valve" ); // old strings shared across valve titles -- we could merge the ones we use into csgo_english at this point 

	CCSGODialogVariableHandlers::Get().RegisterDialogVariableHandlers();

	panorama::UIInputEngine()->RegisterKeyBindingsFile( "file://{resources}/csgo_keybinds.cfg" );

	for ( const SCSGOPanoramaWindowInfo &windowInfo : k_windowInfos )
	{
		int nWd, nHt;
		materials->GetBackBufferDimensions( nWd, nHt );
		// Note - Currently all top level window will use the same input context (created in CPanoramaEngineHandler::Init())
		// May want to have different input context if more that one top level window can be visible at the same time.
		panorama::IUIWindow *pWindow = panorama::UIEngine()->CreateNewUILayerWindow( 0, 0, nWd, nHt, false, false, false, true, windowInfo.pszID, gameuifuncs->GetPanoramaInputContext() );
		pWindow->SetWindowPriority( windowInfo.nPriority );
		panorama::IUIPanelClient *pRootPanel = (panorama::IUIPanelClient *)gameuifuncs->AddPanoramaView( windowInfo.pszID, pWindow );
		pRootPanel->UIPanel()->GetParentWindow()->SetFocusBehavior( panorama::k_EWindowFocusBehavior_Shy );
		pRootPanel->UIPanel()->GetParentWindow()->SetVisible( false );

		m_arrPanoramaWindows.AddToTail( pWindow );

		if ( !pRootPanel->UIPanel()->BLoadLayout( windowInfo.pszLayoutFile ) )
			return;
	}

	//g_pInputSystem->SetMouseCursorVisible(true);
#endif
}

void CGameUI::DestroyGameSpecificUI()
{
#ifdef PANORAMA_ENABLE
	CCSGODialogVariableHandlers::Get().UnregisterDialogVariableHandlers();

	FOR_EACH_VEC_BACK(m_arrPanoramaWindows, idxWindow)
	{
		panorama::IUIWindow* pWindow = m_arrPanoramaWindows[idxWindow];
		m_arrPanoramaWindows[idxWindow] = nullptr;

		if ( pWindow )
		{
			gameuifuncs->RemovePanoramaView( pWindow );
			panorama::UIEngine()->DestroyWindow( pWindow );
		}
	}

	m_arrPanoramaWindows.Purge();

	g_AvatarImageMgr.Cleanup();
#endif
};



//-----------------------------------------------------------------------------
// Purpose: Sets the specified panel as the background panel for the loading
//		dialog.  If NULL, default background is used.  If you set a panel,
//		it should be full-screen with an opaque background, and must be a VGUI popup.
//-----------------------------------------------------------------------------
void CGameUI::SetLoadingBackgroundDialog( vgui::VPANEL panel )
{
	g_hLoadingBackgroundDialog = panel;
}

//-----------------------------------------------------------------------------
// Purpose: connects to client interfaces
//-----------------------------------------------------------------------------
void CGameUI::Connect( CreateInterfaceFn gameFactory )
{
	g_pGameClientExports = (IGameClientExports *)gameFactory(GAMECLIENTEXPORTS_INTERFACE_VERSION, NULL);
#if defined( INCLUDE_SCALEFORM )
	g_pScaleformUI = ( IScaleformUI* ) gameFactory( SCALEFORMUI_INTERFACE_VERSION, NULL );
#endif

#ifdef PANORAMA_ENABLE
	if ( m_bPanoramaEnabled )
	{
		g_pPanoramaUIEngine = (IPanoramaUIEngine*)gameFactory( PANORAMAUI_ENGINE_INTERFACE_VERSION, NULL );
	}
#endif

	achievementmgr = engine->GetAchievementMgr();

	if (!g_pGameClientExports)
	{
		Error("CGameUI::Initialize() failed to get necessary interfaces\n");
	}

	m_GameFactory = gameFactory;
}

//-----------------------------------------------------------------------------
// Purpose: Callback function; sends platform Shutdown message to specified window
//-----------------------------------------------------------------------------
int __stdcall SendShutdownMsgFunc(WHANDLE hwnd, int lparam)
{
	Sys_PostMessage(hwnd, Sys_RegisterWindowMessage("ShutdownValvePlatform"), 0, 1);
	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: Searches for GameStartup*.mp3 files in the sound/ui folder and plays one
//-----------------------------------------------------------------------------
void CGameUI::PlayGameStartupSound()
{
#if defined( LEFT4DEAD ) || defined( CSTRIKE15 )                               
	// CS15 not using this path. using Portal 2 style MP3 looping
	// L4D not using this path, L4D UI now handling with background menu movies   	
	return;
#endif

	if ( IsGameConsole() )
		return;

	if ( CommandLine()->FindParm( "-nostartupsound" ) )
		return;

	FileFindHandle_t fh;

	CUtlVector<char *> fileNames;

	char path[ 512 ];
	Q_snprintf( path, sizeof( path ), "sound/ui/gamestartup*.mp3" );
	Q_FixSlashes( path );

	char const *fn = g_pFullFileSystem->FindFirstEx( path, "MOD", &fh );
	if ( fn )
	{
		do
		{
			char ext[ 10 ];
			Q_ExtractFileExtension( fn, ext, sizeof( ext ) );

			if ( !Q_stricmp( ext, "mp3" ) )
			{
				char temp[ 512 ];
				Q_snprintf( temp, sizeof( temp ), "ui/%s", fn );

				char *found = new char[ strlen( temp ) + 1 ];
				Q_strncpy( found, temp, strlen( temp ) + 1 );

				Q_FixSlashes( found );
				fileNames.AddToTail( found );
			}
	
			fn = g_pFullFileSystem->FindNext( fh );

		} while ( fn );

		g_pFullFileSystem->FindClose( fh );
	}

	// did we find any?
	if ( fileNames.Count() > 0 )
	{
// dgoodenough - SystemTime is absent on PS3, just select first file for now
// PS3_BUILDFIX
// FIXME - we need to find some sort of entropy here and select based on that.
// @wge Fix for OSX too.
#if defined( _PS3 ) || defined( _OSX ) || defined (LINUX)
		int index = 0;
#else
		SYSTEMTIME SystemTime;
		GetSystemTime( &SystemTime );
		int index = SystemTime.wMilliseconds % fileNames.Count();
#endif
		if ( fileNames.IsValidIndex( index ) && fileNames[index] )
		{
			char found[ 512 ];

			// escape chars "*#" make it stream, and be affected by snd_musicvolume
			Q_snprintf( found, sizeof( found ), "play *#%s", fileNames[index] );

			engine->ClientCmd_Unrestricted( found );
		}

		fileNames.PurgeAndDeleteElements();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called to setup the game UI
//-----------------------------------------------------------------------------
void CGameUI::Start()
{
	// determine Steam location for configuration
	if ( !FindPlatformDirectory( m_szPlatformDir, sizeof( m_szPlatformDir ) ) )
		return;

	if ( IsPC() )
	{
		// setup config file directory
		char szConfigDir[512];
		Q_strncpy( szConfigDir, m_szPlatformDir, sizeof( szConfigDir ) );
		Q_strncat( szConfigDir, "config", sizeof( szConfigDir ), COPY_ALL_CHARACTERS );

		Msg( "Steam config directory: %s\n", szConfigDir );

		g_pFullFileSystem->AddSearchPath(szConfigDir, "CONFIG");
		g_pFullFileSystem->CreateDirHierarchy("", "CONFIG");
		// user dialog configuration
		vgui::system()->SetUserConfigFile("InGameDialogConfig.vdf", "CONFIG");

		g_pFullFileSystem->AddSearchPath( "platform", "PLATFORM" );
	}

	// localization
	g_pVGuiLocalize->AddFile( "Resource/platform_%language%.txt");
	g_pVGuiLocalize->AddFile( "Resource/vgui_%language%.txt");

	// dgoodenough - This should not be necessary.
	// PS3_BUILDFIX
	// FIXME - I have no idea why I need to remove this.  SYS_NO_ERROR is defined in sys_utils.h
	// which is included at the top of this file.  It compiles fine, proving the definition is good.
	// However it throws a link time error against SYS_NO_ERROR.  This is *declared* in sys_utils.cpp
	// which is the same place that Sys_SetLastError(...) is declared.  Why then does one of these
	// throw a link time error, when the other does not?  Probably a GCC quirk, since MSVC has no
	// problem with it.  In any case, Sys_SetLastError(...) does nothing on PS3, so removing the
	// call to it here is harmless.
#if !defined( _PS3 )
	Sys_SetLastError( SYS_NO_ERROR );
#endif

	// ********************************************************************
	// The following is commented out to keep intro music from playing
	// before the intro movie:
	//
	//// Delay playing the startup music until two frames
	//// this allows cbuf commands that occur on the first frame that may start a map
	//m_iPlayGameStartupSound = 2;
	//SetBackgroundMusicDesired( true );
	// ********************************************************************

#if !defined( NO_STEAM )
	if ( IsPC() )
	{
		// now we are set up to check every frame to see if we can friends/server browser
		m_bTryingToLoadFriends = true;
		m_iFriendsLoadPauseFrames = 1;
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Validates the user has a cdkey in the registry
//-----------------------------------------------------------------------------
void CGameUI::ValidateCDKey()
{
}

//-----------------------------------------------------------------------------
// Purpose: Finds which directory the platform resides in
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CGameUI::FindPlatformDirectory(char *platformDir, int bufferSize)
{
	platformDir[0] = '\0';

	if ( platformDir[0] == '\0' )
	{
		// we're not under steam, so setup using path relative to game
		if ( IsPC() )
		{
#ifdef WIN32
			if ( ::GetModuleFileName( ( HINSTANCE )GetModuleHandle( NULL ), platformDir, bufferSize ) )
#else
			if ( getcwd( platformDir, bufferSize ) )
#endif
			{
#ifdef WIN32
				V_StripFilename( platformDir ); // GetModuleFileName returns the exe as well as path
#endif
				V_AppendSlash( platformDir, bufferSize );
				Q_strncat(platformDir, "platform", bufferSize, COPY_ALL_CHARACTERS );
				V_AppendSlash( platformDir, bufferSize );
				return true;
			}
		}
		else
		{
			// xbox fetches the platform path from exisiting platform search path
			// path to executeable is not correct for xbox remote configuration
			if ( g_pFullFileSystem->GetSearchPath( "PLATFORM", false, platformDir, bufferSize ) )
			{
				char *pSeperator = strchr( platformDir, ';' );
				if ( pSeperator )
					*pSeperator = '\0';
				return true;
			}
		}

		Error( "Unable to determine platform directory\n" );
		return false;
	}

	return (platformDir[0] != 0);
}

//-----------------------------------------------------------------------------
// Purpose: Called to Shutdown the game UI system
//-----------------------------------------------------------------------------
void CGameUI::Shutdown()
{
#ifdef _PS3
	cellSysmoduleUnloadModule( CELL_SYSMODULE_SYSUTIL_USERINFO );
#endif

	// notify all the modules of Shutdown
	g_VModuleLoader.ShutdownPlatformModules();

	// unload the modules them from memory
	g_VModuleLoader.UnloadPlatformModules();

	ModInfo().FreeModInfo();
	
	steamapicontext->Clear();
#ifndef _GAMECONSOLE
	// SteamAPI_Shutdown(); << Steam shutdown is controlled by engine
#endif
	
	ConVar_Unregister();
	DisconnectTier3Libraries();
	DisconnectTier2Libraries();
	DisconnectTier1Libraries();
}

//-----------------------------------------------------------------------------
// Purpose: just wraps an engine call to activate the gameUI
//-----------------------------------------------------------------------------
void CGameUI::ActivateGameUI()
{
	engine->ExecuteClientCmd("gameui_activate");
	// Lock the UI to a particular player
	SetGameUIActiveSplitScreenPlayerSlot( engine->GetActiveSplitScreenPlayerSlot() );
}

//-----------------------------------------------------------------------------
// Purpose: just wraps an engine call to hide the gameUI
//-----------------------------------------------------------------------------
void CGameUI::HideGameUI()
{
	engine->ExecuteClientCmd("gameui_hide");
	GameConsole().HideImmediately();
}

//-----------------------------------------------------------------------------
// Purpose: Toggle allowing the engine to hide the game UI with the escape key
//-----------------------------------------------------------------------------
void CGameUI::PreventEngineHideGameUI()
{
	engine->ExecuteClientCmd("gameui_preventescape");
}

//-----------------------------------------------------------------------------
// Purpose: Toggle allowing the engine to hide the game UI with the escape key
//-----------------------------------------------------------------------------
void CGameUI::AllowEngineHideGameUI()
{
	engine->ExecuteClientCmd("gameui_allowescape");
}

bool CGameUI::IsIntroMovieEnabled()
{
	if ( Plat_IsInBenchmarkMode() )
	{
		return false;
	}

	if ( engine->IsInEditMode() ||
		CommandLine()->CheckParm( "-dev" ) ||
		CommandLine()->CheckParm( "-novid" ) ||
		CommandLine()->CheckParm( "-allowdebug" ) ||
		CommandLine()->CheckParm( "-console" ) ||
		CommandLine()->CheckParm( "-toconsole" ) )
	{
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Activate the game UI
//-----------------------------------------------------------------------------
void CGameUI::OnGameUIActivated()
{
	bool bWasActive = m_bActivatedUI;
	m_bActivatedUI = true;

	// Lock the UI to a particular player
	if ( !bWasActive )
	{
		SetGameUIActiveSplitScreenPlayerSlot( engine->GetActiveSplitScreenPlayerSlot() );
	}

	// pause the server in case it is pausable
	engine->ClientCmd_Unrestricted( "setpause nomsg" );

	SetSavedThisMenuSession( false );

	UI_BASEMOD_PANEL_CLASS &ui = GetUiBaseModPanelClass();
	bool bNeedActivation = true;
	if ( ui.IsVisible() )
	{
		// Already visible, maybe don't need activation
		if ( !IsInLevel() && IsInBackgroundLevel() )
			bNeedActivation = false;
	}
	if ( bNeedActivation )
	{
		GetUiBaseModPanelClass().OnGameUIActivated();
	}

#ifdef PANORAMA_ENABLE
	if ( m_bPanoramaEnabled )
	{

		if ( m_bFirstActivationForSession && IsIntroMovieEnabled())
		{
			ChangeGameUIState( CSGO_GAME_UI_STATE_INTROMOVIE );
		}
		else if ( IsInLevel() && !m_bInLevelLoading )
		{
			ChangeGameUIState( CSGO_GAME_UI_STATE_PAUSEMENU );
		}
		else if ( !m_bInLevelLoading )
		{
			ChangeGameUIState( CSGO_GAME_UI_STATE_MAINMENU );
		}

		m_bFirstActivationForSession = false;

		g_pInputSystem->SetIMEAllowed( true );

	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Hides the game ui, in whatever state it's in
//-----------------------------------------------------------------------------
void CGameUI::OnGameUIHidden()
{
	bool bWasActive = m_bActivatedUI;
	m_bActivatedUI = false;

	// unpause the game when leaving the UI
	engine->ClientCmd_Unrestricted( "unpause nomsg" );

	GetUiBaseModPanelClass().OnGameUIHidden();

	// Restore to default
	if ( bWasActive )
	{
		SetGameUIActiveSplitScreenPlayerSlot( 0 );
	}

#ifdef PANORAMA_ENABLE
	if( m_bPanoramaEnabled )
	{
		if( IsInLevel() && m_bInLevelLoading == false )
		{
			g_pInputSystem->SetIMEAllowed( false );
			ChangeGameUIState( CSGO_GAME_UI_STATE_INGAME );
		}
	}
#endif

}

//-----------------------------------------------------------------------------
// Purpose: paints all the vgui elements
//-----------------------------------------------------------------------------
void CGameUI::RunFrame()
{
	if ( IsGameConsole() && m_bOpenProgressOnStart )
	{
		StartProgressBar();
		m_bOpenProgressOnStart = false;
	}

	int wide, tall;
#if defined( TOOLFRAMEWORK_VGUI_REFACTOR )
	// resize the background panel to the screen size
	vgui::VPANEL clientDllPanel = enginevguifuncs->GetPanel( PANEL_ROOT );

	int x, y;
	vgui::ipanel()->GetPos( clientDllPanel, x, y );
	vgui::ipanel()->GetSize( clientDllPanel, wide, tall );
	staticPanel->SetBounds( x, y, wide,tall );
#else
	vgui::surface()->GetScreenSize(wide, tall);

	GetUiBaseModPanelClass().SetSize(wide, tall);
#endif

	// Run frames
	g_VModuleLoader.RunFrame();

	GetUiBaseModPanelClass().RunFrame();

	FOR_EACH_VEC( m_arrUiComponents, i )
	{
		m_arrUiComponents[ i ]->Tick();
	}

	// Play the start-up music the first time we run frame
	if ( m_iPlayGameStartupSound > 0 )
	{
		m_iPlayGameStartupSound--;		
	}
	else
	{
		UpdateBackgroundMusic();
	}

	if ( IsPC() && m_bTryingToLoadFriends && m_iFriendsLoadPauseFrames-- < 1  )
	{
		// we got the mutex, so load Friends/Serverbrowser
		// clear the loading flag
		m_bTryingToLoadFriends = false;
		g_VModuleLoader.LoadPlatformModules(&m_GameFactory, 1, false);

		// notify the game of our game name
		const char *fullGamePath = engine->GetGameDirectory();
		const char *pathSep = strrchr( fullGamePath, '/' );
		if ( !pathSep )
		{
			pathSep = strrchr( fullGamePath, '\\' );
		}
		if ( pathSep )
		{
			KeyValues *pKV = new KeyValues("ActiveGameName" );
			pKV->SetString( "name", pathSep + 1 );
			pKV->SetInt( "appid", engine->GetAppID() );
			KeyValues *modinfo = new KeyValues("ModInfo");
			if ( modinfo->LoadFromFile( g_pFullFileSystem, "gameinfo.txt" ) )
			{
				pKV->SetString( "game", modinfo->GetString( "game", "" ) );
			}
			modinfo->deleteThis();

			g_VModuleLoader.PostMessageToAllModules( pKV );
		}

		// notify the ui of a game connect if we're already in a game
		if (m_iGameIP)
		{
			SendConnectedToGameMessage();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called when the game connects to a server
//-----------------------------------------------------------------------------
void CGameUI::OLD_OnConnectToServer(const char *game, int IP, int port)
{
	// Nobody should use this anymore because the query port and the connection port can be different.
	// Use OnConnectToServer2 instead.
	Assert( false );
	OnConnectToServer2( game, IP, port, port );
}

//-----------------------------------------------------------------------------
// Purpose: Called when the game connects to a server
//-----------------------------------------------------------------------------
void CGameUI::OnConnectToServer2(const char *game, int IP, int connectionPort, int queryPort)
{
	m_iGameIP = IP;
	m_iGameConnectionPort = connectionPort;
	m_iGameQueryPort = queryPort;

	SendConnectedToGameMessage();
}


void CGameUI::SendConnectedToGameMessage()
{
	MEM_ALLOC_CREDIT();
	KeyValues *kv = new KeyValues( "ConnectedToGame" );
	kv->SetInt( "ip", m_iGameIP );
	kv->SetInt( "connectionport", m_iGameConnectionPort );
	kv->SetInt( "queryport", m_iGameQueryPort );

	g_VModuleLoader.PostMessageToAllModules( kv );
}



//-----------------------------------------------------------------------------
// Purpose: Called when the game disconnects from a server
//-----------------------------------------------------------------------------
void CGameUI::OnDisconnectFromServer( uint8 eSteamLoginFailure )
{
	m_iGameIP = 0;
	m_iGameConnectionPort = 0;
	m_iGameQueryPort = 0;

	if ( g_hLoadingBackgroundDialog )
	{
		vgui::ivgui()->PostMessage( g_hLoadingBackgroundDialog, new KeyValues("DisconnectedFromGame"), NULL );
	}

	IGameEvent *event = gameeventmanager->CreateEvent( "client_disconnect" );
	if ( event )
	{
		gameeventmanager->FireEventClientSide( event );
	}

	g_VModuleLoader.PostMessageToAllModules(new KeyValues("DisconnectedFromGame"));

	if ( eSteamLoginFailure == STEAMLOGINFAILURE_NOSTEAMLOGIN )
	{
		Warning( "Disconnect due to steam error STEAMLOGINFAILURE_NOSTEAMLOGIN\n" );
	}
	else if ( eSteamLoginFailure == STEAMLOGINFAILURE_VACBANNED )
	{
		Warning( "Disconnect due to steam error STEAMLOGINFAILURE_VACBANNED\n" );
	}
	else if ( eSteamLoginFailure == STEAMLOGINFAILURE_LOGGED_IN_ELSEWHERE )
	{
		Warning( "Disconnect due to steam error STEAMLOGINFAILURE_LOGGED_IN_ELSEWHERE\n" );
	}
}

void CGameUI::RequireLoadingScreenShown( const char * levelName, bool bShowProgressDialog )
{
	GameUI().HideGameUI();

	char mapName[ MAX_PATH ];
	V_strcpy_safe( mapName, levelName ? levelName : "" );
	V_FixSlashes( mapName, '/' );

	g_VModuleLoader.PostMessageToAllModules( new KeyValues( "LoadingStarted" ) );

	GetUiBaseModPanelClass().OnLevelLoadingStarted( mapName, bShowProgressDialog );

	if ( bShowProgressDialog )
	{
		StartProgressBar();
	}

	// Don't play the start game sound if this happens before we get to the first frame
	m_iPlayGameStartupSound = 0;
}

// Used by workshop maps to show the loading screen after a download completes
// PANORAMA: Removed this befause Gameui().HideGameUI() will hide the loading screen and 
// it's not clear we need to force show for workshop maps anyway... 
void CGameUI::ShowCustomProgress()
{
	if ( !IsPanoramaEnabled() )
	{
		RequireLoadingScreenShown( NULL, true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: activates the loading dialog on level load start
//-----------------------------------------------------------------------------
void CGameUI::OnLevelLoadingStarted( const char *levelName, bool bShowProgressDialog )
{
	RequireLoadingScreenShown( levelName, bShowProgressDialog );

#ifdef PANORAMA_ENABLE
	if ( m_bPanoramaEnabled )
	{
		CCSGO_Hud::GetInstance()->ReloadLayout();

		FOR_EACH_VEC( m_GameUIStateListeners, iListener )
		{
			ICSGOGameUIStateListener *pListener = m_GameUIStateListeners[ iListener ];
			if ( !levelName || !V_strcmp( levelName, "<empty>" ) ) // ugh.
			{
				pListener->OnMapLoadStarted( "", false );
			}
			else
			{
				pListener->OnMapLoadStarted( levelName, true );
			}
		}

		ChangeGameUIState( CSGO_GAME_UI_STATE_LOADINGSCREEN );
		m_bInLevelLoading = true;

		g_pPanoramaUIEngine->AccessUIEngine()->UISoundSystem()->SetMusicUseHRTFEffect( false );
	}
#endif
}

extern ConVar devCheatSkipInputLocking;
//-----------------------------------------------------------------------------
// Purpose: closes any level load dialog
//-----------------------------------------------------------------------------
void CGameUI::OnLevelLoadingFinished(bool bError, const char *failureReason, const char *extendedReason)
{
#ifdef PANORAMA_ENABLE
	if ( m_bPanoramaEnabled )
	{
		m_bInLevelLoading = false;
	}
#endif
	StopProgressBar( bError, failureReason, extendedReason );

#if defined( WIN32 ) 
#if defined( INCLUDE_SCALEFORM )

	if ( g_pScaleformUI && !devCheatSkipInputLocking.GetBool() )
	{
		if ( g_pInputSystem->GetCurrentInputDevice() == INPUT_DEVICE_NONE )
		{
			g_pInputSystem->SetCurrentInputDevice( INPUT_DEVICE_KEYBOARD_MOUSE );
		}

	}
#endif
#endif

	// notify all the modules
	g_VModuleLoader.PostMessageToAllModules( new KeyValues( "LoadingFinished" ) );

	GetUiBaseModPanelClass().OnLevelLoadingFinished();
	HideLoadingBackgroundDialog();

#if defined( PORTAL )
	Warning( "HACK: Forcing all of gameui to hide on level load for portal. For some reason it stays open for us and it's annoying. Especially on xbox where it steals our controller focus.\n" );
	HideGameUI();
#endif
#if defined( DOTA_DLL )
	// Similar story for DOTA.
	HideGameUI();
#endif
#if defined ( CSTRIKE_DLL )
	// ditto cstrike
	HideGameUI();
#endif

#ifdef PANORAMA_ENABLE	
	if ( m_bPanoramaEnabled )
	{
		if( bError )
		{
			ChangeGameUIState( CSGO_GAME_UI_STATE_MAINMENU );
		}
		else
		{
			FOR_EACH_VEC( m_GameUIStateListeners, iListener )
			{
				ICSGOGameUIStateListener *pListener = m_GameUIStateListeners[iListener];
				pListener->OnMapLoadFinished();
			}

			ChangeGameUIState( CSGO_GAME_UI_STATE_INGAME );

			StartBackgroundMusicFade();
		}
	}
#endif

}

//-----------------------------------------------------------------------------
// Purpose: Updates progress bar
// Output : Returns true if screen should be redrawn
//-----------------------------------------------------------------------------
bool CGameUI::UpdateProgressBar(float progress, const char *statusText, bool showDialog )
{
#ifdef PANORAMA_ENABLE
	if ( m_bPanoramaEnabled )
	{
		panorama::UIEngine()->DispatchEvent( CSGOLoadProgressChanged::MakeEvent( NULL, progress, statusText ) );
	}
#endif

	// if either the progress bar or the status text changes, redraw the screen
	bool bRedraw = false;

	if ( ContinueProgressBar( progress, showDialog ) )
	{
		bRedraw = true;
	}

	if ( SetProgressBarStatusText( statusText, showDialog ) )
	{
		bRedraw = true;
	}

	return bRedraw;
}

//-----------------------------------------------------------------------------
// Purpose: Updates progress bar
// Output : Returns true if screen should be redrawn
//-----------------------------------------------------------------------------
bool CGameUI::UpdateSecondaryProgressBar(float progress, const wchar_t *desc )
{
	// if either the progress bar or the status text changes, redraw the screen
	SetSecondaryProgressBar( progress );
	SetSecondaryProgressBarText( desc );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameUI::SetProgressLevelName( const char *levelName )
{
	MEM_ALLOC_CREDIT();
	if ( g_hLoadingBackgroundDialog )
	{
		KeyValues *pKV = new KeyValues( "ProgressLevelName" );
		pKV->SetString( "levelName", levelName );
		vgui::ivgui()->PostMessage( g_hLoadingBackgroundDialog, pKV, NULL );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameUI::StartProgressBar()
{
	// open a loading dialog
	m_szPreviousStatusText[0] = 0;
	if ( GameUI().IsPanoramaEnabled() )
	{
#ifdef PANORAMA_ENABLE
		CCSGO_LoadingScreen::GetInstance()->LoadingScreenInit();
#endif
	}
#if defined( INCLUDE_SCALEFORM )
	else
	{
		CLoadingScreenScaleform::SetProgressPoint( 0.0f );
		CLoadingScreenScaleform::Open();
	}
#endif

}

//-----------------------------------------------------------------------------
// Purpose: returns true if the screen should be updated
//-----------------------------------------------------------------------------
bool CGameUI::ContinueProgressBar( float progressFraction, bool showDialog )
{
	if ( GameUI().IsPanoramaEnabled() )
	{
		return true;
	}
#if defined( INCLUDE_SCALEFORM )
	else
	{
		CLoadingScreenScaleform::Activate();
		return CLoadingScreenScaleform::SetProgressPoint( progressFraction, showDialog );
	}
#endif

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: stops progress bar, displays error if necessary
//-----------------------------------------------------------------------------
void CGameUI::StopProgressBar(bool bError, const char *failureReason, const char *extendedReason)
{
	if ( IsInLevel() )
	{
#if defined( INCLUDE_SCALEFORM )
		CLoadingScreenScaleform::FinishLoading();
#endif
	}
	else
	{
#if defined( INCLUDE_SCALEFORM )
		CLoadingScreenScaleform::CloseLoadingScreen();
#endif
	}

// CStrike15 handles error messages elsewhere. (ClientModeCSFullscreen::OnEvent)
#if !defined( CSTRIKE15 )
	if ( !IsGameConsole() && bError )
	{
		ShowMessageDialog( extendedReason, failureReason );
	}	
#endif
}

//-----------------------------------------------------------------------------
// Purpose: sets loading info text
//-----------------------------------------------------------------------------
bool CGameUI::SetProgressBarStatusText(const char *statusText, bool showDialog )
{
	if (!statusText)
		return false;

	if (!stricmp(statusText, m_szPreviousStatusText))
		return false;

#if defined( INCLUDE_SCALEFORM )
	CLoadingScreenScaleform::SetStatusText( statusText, showDialog );
	Q_strncpy( m_szPreviousStatusText, statusText, sizeof( m_szPreviousStatusText ) );
#endif
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameUI::SetSecondaryProgressBar(float progress /* range [0..1] */)
{
#if defined( INCLUDE_SCALEFORM )
	CLoadingScreenScaleform::SetSecondaryProgress( progress );
#endif

#if defined( PANORAMA_ENABLE )
	CCSGO_LoadingScreen::GetInstance()->SetSecondaryProgressBar(progress);
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameUI::SetSecondaryProgressBarText( const wchar_t *desc )
{
	if (!desc)
		return;

#if defined( INCLUDE_SCALEFORM )
	CLoadingScreenScaleform::SetSecondaryProgressText( desc );
#endif

#if defined( PANORAMA_ENABLE )
	CCSGO_LoadingScreen::GetInstance()->SetSecondaryProgressText(desc);
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Returns prev settings
//-----------------------------------------------------------------------------
bool CGameUI::SetShowProgressText( bool show )
{
#if defined( INCLUDE_SCALEFORM )
	return CLoadingScreenScaleform::SetShowProgressText( show );
#endif

	return true; // TODONOSF

}


//-----------------------------------------------------------------------------
// Purpose: returns true if we're currently playing the game
//-----------------------------------------------------------------------------
bool CGameUI::IsInLevel()
{
	const char *levelName = engine->GetLevelName();
	if (levelName && levelName[0] && !engine->IsLevelMainMenuBackground())
	{
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: returns true if we're at the main menu and a background level is loaded
//-----------------------------------------------------------------------------
bool CGameUI::IsInBackgroundLevel()
{
	const char *levelName = engine->GetLevelName();
	if (levelName && levelName[0] && engine->IsLevelMainMenuBackground())
	{
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: returns true if we're in a multiplayer game
//-----------------------------------------------------------------------------
bool CGameUI::IsInMultiplayer()
{
	return (IsInLevel() && engine->GetMaxClients() > 1);
}

//-----------------------------------------------------------------------------
// Purpose: returns true if we're console ui
//-----------------------------------------------------------------------------
bool CGameUI::IsConsoleUI()
{
	return m_bIsConsoleUI;
}

//-----------------------------------------------------------------------------
// Purpose: returns true if we've saved without closing the menu
//-----------------------------------------------------------------------------
bool CGameUI::HasSavedThisMenuSession()
{
	return m_bHasSavedThisMenuSession;
}

void CGameUI::SetSavedThisMenuSession( bool bState )
{
	m_bHasSavedThisMenuSession = bState;
}

//-----------------------------------------------------------------------------
// Purpose: Makes the loading background dialog visible, if one has been set
//-----------------------------------------------------------------------------
void CGameUI::ShowLoadingBackgroundDialog()
{
	if ( g_hLoadingBackgroundDialog )
	{
		vgui::VPANEL panel = GetUiBaseModPanelClass().GetVPanel();

		vgui::ipanel()->SetParent( g_hLoadingBackgroundDialog, panel );
		vgui::ipanel()->MoveToFront( g_hLoadingBackgroundDialog );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Hides the loading background dialog, if one has been set
//-----------------------------------------------------------------------------
void CGameUI::HideLoadingBackgroundDialog()
{
	if ( g_hLoadingBackgroundDialog )
	{
		if ( engine->IsInGame() )
		{
			vgui::ivgui()->PostMessage( g_hLoadingBackgroundDialog, new KeyValues( "LoadedIntoGame" ), NULL );
		}
		else
		{
			vgui::ipanel()->SetVisible( g_hLoadingBackgroundDialog, false );
			vgui::ipanel()->MoveToBack( g_hLoadingBackgroundDialog );
		}

		vgui::ivgui()->PostMessage( g_hLoadingBackgroundDialog, new KeyValues("HideAsLoadingPanel"), NULL );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether a loading background dialog has been set
//-----------------------------------------------------------------------------
bool CGameUI::HasLoadingBackgroundDialog()
{
	return ( NULL != g_hLoadingBackgroundDialog );
}

//-----------------------------------------------------------------------------
// Purpose: Xbox 360 calls from engine to GameUI 
//-----------------------------------------------------------------------------
void CGameUI::ShowMessageDialog( const uint nType, vgui::Panel *pOwner )
{
#if defined( _X360 )
	BasePanel()->ShowMessageDialog( nType, pOwner );
#endif
}

void CGameUI::ShowMessageDialog( const char* messageID, const char* titleID )
{
#if defined( _X360 )
	((CCStrike15BasePanel*)BasePanel())->OnOpenMessageBox( titleID, messageID, "#SFUI_Legend_Ok", MESSAGEBOX_FLAG_OK, NULL, NULL );
#endif
}

#ifdef PANORAMA_ENABLE
void CGameUI::CreateCommandMsgBoxForPopupManager( CUI_PopupManager* pPopupManager, const char* pszTitle, const char* pszMessage, bool showOk, bool showCancel, const char* okCommand, const char* cancelCommand, const char* closedCommand, const char* pszLegend, const char* szPanelID /*= nullptr */ )
{
	if( pPopupManager )
	{
		CUI_Popup_Generic_Command *pPopup = new CUI_Popup_Generic_Command( pPopupManager, szPanelID, nullptr );

		// Handle closed command by appending it to the OK and cancel commands
		const char* okConCommand = okCommand ? okCommand : closedCommand;
		const char* cancelConCommand = cancelCommand ? cancelCommand : closedCommand;

		CFmtStr okConCommandBuffer;
		CFmtStr cancelConCommandBuffer;

		if( closedCommand && okCommand )
		{
			okConCommandBuffer.Format( "%s\n%s", okCommand, closedCommand );
			okConCommand = okConCommandBuffer.Get();
		}

		if( closedCommand && cancelCommand )
		{
			cancelConCommandBuffer.Format( "%s\n%s", cancelCommand, closedCommand );
			cancelConCommand = cancelConCommandBuffer.Get();
		}

		if( showCancel && showOk )
		{
			pPopup->SetDisplayOkCancel( pszTitle, pszMessage, okConCommand, cancelConCommand );
		}
		else if( showOk )
		{
			pPopup->SetDisplayOk( pszTitle, pszMessage, okConCommand );
		}
		else if( showCancel )
		{
			pPopup->SetDisplayCancel( pszTitle, pszMessage, cancelConCommand );
		}
		else
		{
			pPopup->SetDisplayNoOption( pszTitle, pszMessage );
		}

		if( !showOk )
		{
			pPopup->SetSpinnerVisible( true );
		}

		pPopupManager->ShowPopup( pPopup );
	}
}

bool CGameUI::BAnyPopupShown( void ) const
{
	if ( GetPopupManagerForUIState() && GetPopupManagerForUIState()->IsAnyPopupVisible() )
		return true;

	return false;
}

#endif

void CGameUI::CreateMainMenuCommandMsgBox( const char* pszTitle, const char* pszMessage, bool showOk, bool showCancel, const char* okCommand, const char* cancelCommand, const char* closedCommand, const char* pszLegend, const char* szPanelID /*= nullptr */ )
{
#ifdef PANORAMA_ENABLE
	if( m_bPanoramaEnabled )
	{
		CreateCommandMsgBoxForPopupManager( CCSGO_MainMenu::GetInstance()->GetPopupManager(), pszTitle, pszMessage, showOk, showCancel, okCommand, cancelCommand, closedCommand, pszLegend, szPanelID );
	}
	else
#endif
	{
#if defined( INCLUDE_SCALEFORM )
		((CCStrike15BasePanel*)BasePanel())->CreateCommandMsgBox( pszTitle, pszMessage, showOk, showCancel, okCommand, cancelCommand, closedCommand, pszLegend );
#endif
	}
}

void CGameUI::CreateCommandMsgBox( const char* pszTitle, const char* pszMessage, bool showOk, bool showCancel, const char* okCommand, const char* cancelCommand, const char* closedCommand, const char* pszLegend, const char* szPanelID /*= nullptr */)
{
#ifdef PANORAMA_ENABLE
	if ( m_bPanoramaEnabled )
	{
		CUI_PopupManager* pMgr = GetPopupManagerForUIState();
		if ( pMgr )
		{
			CreateCommandMsgBoxForPopupManager( pMgr, pszTitle, pszMessage, showOk, showCancel, okCommand, cancelCommand, closedCommand, pszLegend, szPanelID );
		}
		else
		{
			Msg( "No Panorama root window for displaying popup message\n" );
			Msg( "%s %s\n", pszTitle, pszMessage );
		}
	}
	else
#endif
	{
#if defined( INCLUDE_SCALEFORM )
		((CCStrike15BasePanel*)BasePanel())->CreateCommandMsgBox( pszTitle, pszMessage, showOk, showCancel, okCommand, cancelCommand, closedCommand, pszLegend );
#endif
	}
}

void CGameUI::CreateCommandMsgBoxInSlot( ECommandMsgBoxSlot slot, const char* pszTitle, const char* pszMessage, bool showOk, bool showCancel, const char* okCommand, const char* cancelCommand, const char* closedCommand, const char* pszLegend )
{
#ifdef PANORAMA_ENABLE
	if ( m_bPanoramaEnabled )
	{
		CreateCommandMsgBox( pszTitle, pszMessage, showOk, showCancel, okCommand, cancelCommand, closedCommand, pszLegend );
	}
	else
#endif
	{
#if defined( INCLUDE_SCALEFORM )
		((CCStrike15BasePanel*)BasePanel())->CreateCommandMsgBoxInSlot( slot, pszTitle, pszMessage, showOk, showCancel, okCommand, cancelCommand, closedCommand, pszLegend );
#endif
	}
}


void CGameUI::CloseCommandMsgBox( const char *szPanelID )
{
#if defined PANORAMA_ENABLE
	if ( m_bPanoramaEnabled )
	{
		CUI_PopupManager* pMgr = GetPopupManagerForUIState();
		if ( pMgr )
		{
			CUI_Popup_Generic_Command *pPopup = assert_cast< CUI_Popup_Generic_Command * >( pMgr->FindChild( szPanelID ) );
			if ( pPopup )
			{ 
				pPopup->CloseAndDeleteAsync();
			}
		}
	}
#endif
}

void CGameUI::UnloadAllScaleformDialogs( bool bClosePriorityMsgBoxes )
{
#ifdef INCLUDE_SCALEFORM
	CMessageBoxScaleform::UnloadAllDialogs( bClosePriorityMsgBoxes );
#endif
}

IMatchmakingStatus* CGameUI::CreateMatchmakingStatusDialog( char const *szCustomTitle /*= "#SFUI_MMStatus_Title"*/, char const *szCustomText /*= "#SFUI_MMStatus_Searching"*/ )
{
#if defined PANORAMA_ENABLE
	if( m_bPanoramaEnabled )
	{
		CUI_PopupManager* pMgr = GetPopupManagerForUIState();
		if( pMgr )
		{
			CUI_MMStatus_Popup *pPopup = new CUI_MMStatus_Popup( pMgr, "MMStatus", nullptr, szCustomTitle, szCustomText );
			pMgr->ShowPopup( pPopup );
			return pPopup;
		}
		else
		{
			Msg( "No Panorama root window for displaying popup message\n" );
			Msg( "%s %s\n", szCustomTitle, szCustomText );
			return NULL;
		}
	}
#endif

#ifdef INCLUDE_SCALEFORM
	return new CMatchmakingStatus( szCustomTitle, szCustomText );
#endif

	return NULL;
}

//-----------------------------------------------------------------------------

void CGameUI::NeedConnectionProblemWaitScreen()
{
#ifdef SWARM_DLL
	BaseModUI::CUIGameData::Get()->NeedConnectionProblemWaitScreen();
#endif
}

void CGameUI::ShowPasswordUI( char const *pchCurrentPW )
{
#ifdef SWARM_DLL
	BaseModUI::CUIGameData::Get()->ShowPasswordUI( pchCurrentPW );
#endif
}

//-----------------------------------------------------------------------------
void CGameUI::SetProgressOnStart()
{
	m_bOpenProgressOnStart = true;
}

#if defined( _GAMECONSOLE ) && defined( _DEMO )
void CGameUI::OnDemoTimeout()
{
	GetUiBaseModPanelClass().OnDemoTimeout();
}
#endif

bool CGameUI::IsPlayingFullScreenVideo()
{
	return false;
}

bool CGameUI::IsTransitionEffectEnabled()
{
	return false;
}

void CGameUI::StartLoadingScreenForCommand( const char* command )
{
	if ( IsPanoramaEnabled() )
	{
		engine->ClientCmd_Unrestricted( command );
	}
	else
	{
#if defined( INCLUDE_SCALEFORM )
		CLoadingScreenScaleform::LoadDialogForCommand( command );
#endif
	}
}

void CGameUI::StartLoadingScreenForKeyValues( KeyValues* keyValues )
{
	if ( IsPanoramaEnabled() )
	{
		if ( !g_pMatchFramework->ApplySettings( keyValues ) )
		{
#if defined ( PANORAMA_ENABLE )
			ChangeGameUIState( CSGO_GAME_UI_STATE_MAINMENU );
//			panorama::DispatchEvent( CancelConnectToServer(),  nullptr );
#endif
		}
	}
	else
	{
#if defined( INCLUDE_SCALEFORM )
		CLoadingScreenScaleform::LoadDialogForKeyValues( keyValues );
#endif
	}
}

//Convars that control how long to pause music between playing in the main menu.
static ConVar snd_mainmenu_music_break_time_min("snd_mainmenu_music_break_time_min", "0", FCVAR_CHEAT, "Minimum amount of time to pause between playing main menu music");
static ConVar snd_mainmenu_music_break_time_max("snd_mainmenu_music_break_time_max", "0", FCVAR_CHEAT, "Minimum amount of time to pause between playing main menu music");


//Tells us if background music is playing on the title screen. It will also return true
//if the music has recently finished and we're not ready to play more music yet.
bool CGameUI::IsBackgroundMusicPlaying( void )
{
	static ConVarRef snd_menumusic_volume( "snd_menumusic_volume" );

	static float s_fMusicConsideredPlayingUntil = gpGlobals->curtime;
	static float s_fMusicPlayingUntilVolume = snd_menumusic_volume.GetFloat(); //if the menu music volume changes we don't want to pause restarting music
	bool in_pause_after_playing = (gpGlobals->curtime < s_fMusicConsideredPlayingUntil) && (s_fMusicPlayingUntilVolume == snd_menumusic_volume.GetFloat()) && snd_mainmenu_music_break_time_max.GetInt() > 0;

	if ( m_nBackgroundMusicGUID == 0 )
	{
		return in_pause_after_playing;
	}

	bool result = enginesound->IsSoundStillPlaying( m_nBackgroundMusicGUID );

	if(result)
	{
		float mintime = static_cast<float>(snd_mainmenu_music_break_time_min.GetInt());
		float maxtime = static_cast<float>(snd_mainmenu_music_break_time_max.GetInt());
		int diff = int(maxtime - mintime);
		s_fMusicConsideredPlayingUntil = gpGlobals->curtime + mintime;
		s_fMusicPlayingUntilVolume = snd_menumusic_volume.GetFloat();
		const char *pDefaultMusicPack = "valve_csgo_01"; //delay time between tracks only on the default music pack, since some other music packs loop perfectly.
		if(diff > 0 && m_pMusicExtension != nullptr && V_strcmp( m_pMusicExtension, pDefaultMusicPack ) == 0)
		{
			s_fMusicConsideredPlayingUntil += float(rand()%diff);
		}
	}

	return result || in_pause_after_playing;
}

void CGameUI::ReleaseBackgroundMusic( void )
{
	if ( m_nBackgroundMusicGUID == 0 )
		return;
	
	enginesound->StopSoundByGuid( m_nBackgroundMusicGUID, true );

 #if defined( _GAMECONSOLE )

	char nMusicKit[128];
	V_snprintf( nMusicKit, 128, "music/%03i/%s", m_nBackgroundMusicVersion, BACKGROUND_MUSIC_FILENAME );

	//release this to save on memory
 	enginesound->UnloadSound( nMusicKit );
 #endif

	m_nBackgroundMusicGUID = 0;
	m_flBackgroundMusicStopTime = -1.0f;
}

//The way to loop an MP3 is just to constantly check if it is playing and restart it otherwise
#define MENUMUSIC_FADETIME 1.34

void CGameUI::UpdateBackgroundMusic( void )
{
#ifdef PANORAMA_ENABLE
	m_bBackgroundMusicDesired = g_pPanoramaUIEngine->AccessUIEngine()->UISoundSystem()->GetPlayMainMenuMusic();

	/* removed for partner depot */

	static ConVarRef snd_mainmusic_hrtf("snd_mainmusic_hrtf");
	snd_mainmusic_hrtf.SetValue( g_pPanoramaUIEngine->AccessUIEngine()->UISoundSystem()->GetMusicUseHRTFEffect() ? "1.0" : "0.0" );

#endif
	if ( m_bBackgroundMusicDesired && !enginesound->GetPreventSound() )
	{	
		const char * pNewMusicExtension = "";
		
		CSteamID steamIDForPlayer;
		if ( steamapicontext->SteamUser() )
			steamIDForPlayer = steamapicontext->SteamUser()->GetSteamID();

		const CEconItemView *pItemData = steamIDForPlayer.IsValid() ? CSInventoryManager()->GetItemInLoadoutForTeam( 0, LOADOUT_POSITION_MUSICKIT, &steamIDForPlayer ) : NULL;
		
		uint32 unMusicID = 0;
		bool bIsPreview = false;

		if ( m_pPreviewMusicExtension && m_pPreviewMusicExtension[0] )
		{
			pNewMusicExtension = m_pPreviewMusicExtension;
			bIsPreview = true;
		}

		else if ( pItemData && pItemData->IsValid() )
		{
			static const CEconItemAttributeDefinition *pAttr_MusicID = GetItemSchema()->GetAttributeDefinitionByName( "music id" );

			if ( pItemData->FindAttribute( pAttr_MusicID, &unMusicID ) )
			{
				const CEconMusicDefinition *pMusicDef = GetItemSchema()->GetMusicDefinition( unMusicID );
				if ( pMusicDef )
					pNewMusicExtension = pMusicDef->GetName();
			}
		}

		if ( !IsBackgroundMusicPlaying() && !enginesound->IsMoviePlaying() )
		{
			{
				CLocalPlayerFilter filter;
				filter.SetLocalUIFilter();
				C_BaseEntity::EmitSound(filter, SOUND_FROM_WORLD, "Music.StopNonMainMenuMusic");
			}

			m_flBackgroundMusicStopTime = -1.0;

			char sMusicKit[128];

			m_pMusicExtension = pNewMusicExtension;

			if ( !StringIsEmpty( m_pMusicExtension ) && ( unMusicID > 1 || bIsPreview ) )
			{
				V_sprintf_safe( sMusicKit, "%%music/%s/%s", m_pMusicExtension, BACKGROUND_MUSIC_FILENAME );
			}
			else
			{
				m_nBackgroundMusicVersion++;

				if ( m_nBackgroundMusicVersion == 1 )
				{
					V_sprintf_safe( sMusicKit, "%%music/valve_csgo_02/%s", BACKGROUND_MUSIC_FILENAME );
				}
				else
				{
					m_nBackgroundMusicVersion = 0;
					V_sprintf_safe( sMusicKit, "%%music/valve_csgo_01/%s", BACKGROUND_MUSIC_FILENAME );
				}
			}
			m_nBackgroundMusicGUID = enginesound->EmitAmbientSound( sMusicKit, 1.0 );
			
		}
		else if ( !FStrEq( pNewMusicExtension, m_pMusicExtension ) )
		{
			ReleaseBackgroundMusic();
		}

		if ( m_nQuestAudioGUID && ( m_flQuestAudioTimeEnd < gpGlobals->curtime ) )	// resume main menu music after quest audio is complete
		{
			//		SetBackgroundMusicDesired( true );

			static ConVarRef snd_musicvolume( "snd_musicvolume_fixed" );
			static ConVarRef snd_menumusic_volume( "snd_menumusic_volume" );

			snd_musicvolume.SetValue( m_flMasterMusicVolumeSavedForMissionAudio );
			snd_menumusic_volume.SetValue( m_flMenuMusicVolumeSavedForMissionAudio );

			m_flQuestAudioTimeEnd = 0;
			m_nQuestAudioGUID = 0;
		}
	}
// 	else if ( m_pAudioFile && m_pAudioFile[ 0 ] )
// 	{
// 		if ( !IsBackgroundMusicPlaying() )
// 		{
// 			m_nBackgroundMusicGUID = enginesound->EmitAmbientSound( m_pAudioFile, 1.0 );
// 		}
// 	}
	else if( !m_bBackgroundMusicAllowFinishTrack || !IsBackgroundMusicPlaying() )
	{
		ReleaseBackgroundMusic();
	}

	if ( m_flBackgroundMusicStopTime > 0.0f && m_nBackgroundMusicGUID != 0 )
	{
		float flDelta = gpGlobals->curtime - m_flBackgroundMusicStopTime;
		float flFadeAmount = 1.0 - (flDelta / MENUMUSIC_FADETIME);
		enginesound->SetVolumeByGuid(m_nBackgroundMusicGUID, flFadeAmount);
		if(flFadeAmount < 0.01f)
		{
			ReleaseBackgroundMusic();
			m_bBackgroundMusicAllowFinishTrack = false;
			m_flBackgroundMusicStopTime = -1.0f;
		}
	}

}
void CGameUI::StartBackgroundMusicFade( void )
{
	m_flBackgroundMusicStopTime = gpGlobals->curtime;
	m_bBackgroundMusicAllowFinishTrack = true;
}
void CGameUI::SetBackgroundMusicDesired( bool bPlayMusic, bool bAllowFinishCurrentTrack )
{
	m_bBackgroundMusicAllowFinishTrack = bAllowFinishCurrentTrack || m_flBackgroundMusicStopTime > 0.0f;
	m_bBackgroundMusicDesired = bPlayMusic;
#ifdef PANORAMA_ENABLE
	g_pPanoramaUIEngine->AccessUIEngine()->UISoundSystem()->SetPlayMainMenuMusic( bPlayMusic );
#endif

}

bool CGameUI::IsInGameUiFrontEndInteractiveState()
{
#ifdef PANORAMA_ENABLE
	switch( GetGameUIState() )
	{
	case CSGO_GAME_UI_STATE_PAUSEMENU:
	case CSGO_GAME_UI_STATE_MAINMENU:
		return true;
	}
#endif
	return false;
}

bool CGameUI::LoadingProgressWantsIsolatedRender( bool bContextValid )
{
	return GetUiBaseModPanelClass().LoadingProgressWantsIsolatedRender( bContextValid );
}

void CGameUI::RestoreTopLevelMenu()
{
	BasePanel()->PostMessage( BasePanel(), new KeyValues( "RunMenuCommand", "command", "RestoreTopLevelMenu" ) );
}

void CGameUI::SetPreviewBackgroundMusic( const char * pchPreviewMusicPrefix )
{
	static ConVarRef snd_menumusic_volume("snd_menumusic_volume");
	static ConVarRef snd_musicvolume("snd_musicvolume_fixed");

	bool bResetMusic = false;

	// if we're previewing music, store the current volumes and set them to default.
	if ( pchPreviewMusicPrefix && pchPreviewMusicPrefix[0] )
	{
		m_flMainMenuMusicVolume = snd_menumusic_volume.GetFloat();
		m_flMasterMusicVolume = snd_musicvolume.GetFloat();

		m_nMusicMinRestartDelay = snd_mainmenu_music_break_time_min.GetInt();
		m_nMusicMaxRestartDelay = snd_mainmenu_music_break_time_max.GetInt();

		snd_menumusic_volume.SetValue( snd_menumusic_volume.GetDefault() );
		snd_musicvolume.SetValue( snd_musicvolume.GetDefault() );

		snd_mainmenu_music_break_time_min.SetValue(0);
		snd_mainmenu_music_break_time_max.SetValue(0);

		bResetMusic = true;
		
	}
	else
	{
		if ( m_flMainMenuMusicVolume != -1 )	// stored music is not -1 so we are previewing
		{
			snd_menumusic_volume.SetValue( m_flMainMenuMusicVolume );
			snd_musicvolume.SetValue( m_flMasterMusicVolume );
			snd_mainmenu_music_break_time_min.SetValue( m_nMusicMinRestartDelay );
			snd_mainmenu_music_break_time_max.SetValue( m_nMusicMaxRestartDelay );

			bResetMusic = true;
		}

		m_flMainMenuMusicVolume = -1;
		m_flMasterMusicVolume = -1;
		m_nMusicMinRestartDelay = -1;
		m_nMusicMaxRestartDelay = -1;
	}

	m_pPreviewMusicExtension = pchPreviewMusicPrefix;

	if ( bResetMusic )
	{
		ReleaseBackgroundMusic();	// Even when we preview the musickit we have, we still want to restart the track.
	}

}

void CGameUI::PlayQuestAudio( const char * pchAudioFile )
{
#define MAINMENU_QUEST_AUDIO_MASTER_VOLUME 1.0
#define MAINMENU_QUEST_AUDIO_MENUMUSIC_VOLUME 0.01

	if ( !pchAudioFile || !pchAudioFile[0] )
		return;

	if ( m_nQuestAudioGUID )
		return;

	m_flQuestAudioTimeEnd = gpGlobals->curtime + enginesound->GetSoundDuration( pchAudioFile );

//	SetBackgroundMusicDesired( false );

	static ConVarRef snd_musicvolume( "snd_musicvolume_fixed" );

	m_flMasterMusicVolumeSavedForMissionAudio = snd_musicvolume.GetFloat();
	if ( ( snd_musicvolume.GetFloat() < MAINMENU_QUEST_AUDIO_MASTER_VOLUME ) )
	{
		snd_musicvolume.SetValue( ( float )MAINMENU_QUEST_AUDIO_MASTER_VOLUME );
	}

	static ConVarRef snd_menumusic_volume("snd_menumusic_volume");

	m_flMenuMusicVolumeSavedForMissionAudio = snd_menumusic_volume.GetFloat();
	if ( ( snd_menumusic_volume.GetFloat() > MAINMENU_QUEST_AUDIO_MENUMUSIC_VOLUME ) )
	{	
		snd_menumusic_volume.SetValue( ( float )MAINMENU_QUEST_AUDIO_MENUMUSIC_VOLUME );
	}


	m_nQuestAudioGUID = enginesound->EmitAmbientSound( pchAudioFile, 1.0 );

	if (m_nQuestAudioGUID == 0)
	{
		// Playing music failed for some reason so immediately set the music volume
		// back to its initial value.
		snd_musicvolume.SetValue( m_flMasterMusicVolumeSavedForMissionAudio );
		snd_menumusic_volume.SetValue( m_flMenuMusicVolumeSavedForMissionAudio );
		m_flQuestAudioTimeEnd = gpGlobals->curtime;
	}


}

void CGameUI::StopQuestAudio( void )
{
	enginesound->StopSoundByGuid( m_nQuestAudioGUID, true );

	if ( m_flQuestAudioTimeEnd )
		m_flQuestAudioTimeEnd = gpGlobals->curtime;

}

bool CGameUI::IsQuestAudioPlaying( void )
{
	return ( ( m_nQuestAudioGUID != 0 ) ? true : false );

}

#if defined ( PANORAMA_ENABLE )
bool CGameUI::PlayUISoundScript( const char* szSoundScriptName, panorama::IUIPanelClient* pPanel /*= nullptr*/ )
{
	return panorama::DispatchEvent( panorama::PlaySoundEffect(), pPanel ? pPanel : ( panorama::IUIPanelClient* )CCSGO_MainMenu::GetInstance(), szSoundScriptName, nullptr );
}
#endif

#ifdef PANORAMA_ENABLE

bool CGameUI::IsLoadingScreenVisible()
{
	return CCSGO_LoadingScreen::GetInstance()->GetParentWindow()->BIsVisible();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CGameUI::RegisterGameUIStateListener(ICSGOGameUIStateListener *pListener)
{
	m_GameUIStateListeners.AddToTail(pListener);
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CGameUI::UnregisterGameUIStateListener(ICSGOGameUIStateListener *pListener)
{
	Verify(m_GameUIStateListeners.FindAndFastRemove(pListener));
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CGameUI::ChangeGameUIState(CSGOGameUIState_t nNewState)
{
	CSGOGameUIState_t nOldState = m_CSGOGameUIState;

	Msg("ChangeGameUIState: %s -> %s\n", CSGOGameUIStateName(nOldState), CSGOGameUIStateName(nNewState));

	if (nOldState == nNewState)
		return;

	m_CSGOGameUIState = nNewState;

	FOR_EACH_VEC(m_GameUIStateListeners, iListener)
	{
		ICSGOGameUIStateListener *pListener = m_GameUIStateListeners[iListener];
		pListener->OnCSGOGameUIStateChange(nOldState, nNewState);
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CSGOGameUIState_t CGameUI::GetGameUIState()
{
	return m_CSGOGameUIState;
}

CUI_PopupManager* CGameUI::GetPopupManagerForUIState() const
{
	CUI_Root* pRootWindow = NULL;
	switch ( m_CSGOGameUIState )
	{
		case CSGO_GAME_UI_STATE_MAINMENU:
		case CSGO_GAME_UI_STATE_INTROMOVIE:
		case CSGO_GAME_UI_STATE_PAUSEMENU:
			pRootWindow = CCSGO_MainMenu::GetInstance();
			break;
		case CSGO_GAME_UI_STATE_INGAME:
			pRootWindow = CCSGO_Hud::GetInstance();
			break;
	}

	Assert( pRootWindow );
	if ( pRootWindow )
		return pRootWindow->GetPopupManager();
	else
		return NULL;
}

#endif

