//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "cbase.h"
#include "csgo_loadingscreen.h"
#include "cs_gamerules.h"
#include "panorama/controls/image.h"
#include "panorama/uifileresource.h"
#include "gametypes/igametypes.h"
#include "matchmaking/imatchframework.h"
//#include "cs_lobby_helpers.h"
#include "cs_workshop_manager.h"
#include "ugc_utils.h"
#include "gametypes.h"
#include "cs_gameplay_hints.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

REGISTER_PANEL2D_FACTORY( CCSGO_LoadingScreen, CSGOLoadingScreen );

using namespace panorama;

DEFINE_PANORAMA_EVENT( PopulateLoadingScreen );
DEFINE_PANORAMA_EVENT( OnMapConfigLoaded );
DEFINE_PANORAMA_EVENT( UnloadLoadingScreenAndReinit );

ConVar cl_show_new_hint_delay( "cl_show_new_hint_delay", "10.0", FCVAR_DEVELOPMENTONLY );

namespace
{
	CPanoramaSymbolLazyInit k_symLoadingScreenClosingAnim( "loading-screen--closing-animation" );
	CPanoramaSymbolLazyInit k_symLoadingScreenClosing( "loading-screen--closing" );
	CPanoramaSymbolLazyInit k_symLoadingScreenShowAnim( "loading-root--show-animation" );
	CPanoramaSymbolLazyInit k_symLoadingScreenShow( "loading-root--show" );
	CPanoramaSymbolLazyInit k_symBGBlur( "loading-screen-blur" );
	CPanoramaSymbolLazyInit k_symLoadingScreenUnblur( "loading-screen--unblur" );
}

static bool GetCustomRules( const char** outSzName, const char** outSzRules, int skirmishId )
{
	if ( skirmishId == 0 )
		return false;

	const CSkirmishModeDefinition* pSkirmishDef = GetItemSchema()->GetSkirmishModeDefinition( skirmishId );
	if ( pSkirmishDef == nullptr )
		return false;

	const char* szLoadingScreenRules = pSkirmishDef->GetLoadingScreenRulesToken(); // REI HACK: For events I stole this field to hold the event rules
	if ( szLoadingScreenRules == nullptr || *szLoadingScreenRules == '\0' )
		return false;

	*outSzRules = szLoadingScreenRules;

	const char* szName = pSkirmishDef->GetLocNameToken();
	if ( szName == nullptr || *szName == '\0' )
		szName = nullptr;

	*outSzName = szName;

	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CCSGO_LoadingScreen *CCSGO_LoadingScreen::s_pLoadingScreen = NULL;


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CCSGO_LoadingScreen::CCSGO_LoadingScreen( CPanel2D *pParent, const char *pchID )
	: CUI_Root( pParent, pchID )
{
	Assert( s_pLoadingScreen == NULL );
	s_pLoadingScreen = this;

	m_flLoadingPercentPrev = 0.0f;
	m_flLoadingPercent = 0.0f;
	m_flLoadingPercentLerpStart = 0.0f;
	m_flTimeLastHintUpdate = 0.0f;

	m_nSkirmishId = 0;
	m_bCheckedForSWFAndFailed = false;

	m_serverInfoReady = false;

	SetInputNamespace( "csgo_loadingscreen" );

	DbgVerify( BLoadLayout( "file://{resources}/layout/loadingscreen.xml", true ) );

	m_pProgressBar = panorama::panel_cast< CProgressBar* >( FindChildInLayoutFile( "ProgressBar" ) );
	m_pProgressStatusText = panorama::panel_cast< CLabel* >( FindChildInLayoutFile( "ProgressStatusText" ) );
	m_pProgressSecondaryBar = panorama::panel_cast< CProgressBar* >( FindChildInLayoutFile( "ProgressBarSecondary" ) );
	m_pProgressSecondaryStatusText = panorama::panel_cast< CLabel* >( FindChildInLayoutFile( "ProgressSecondaryStatusText" ) );

	m_pBackgroundMapBlur = panorama::panel_cast< CCSGO_BlurTarget* >( FindChildInLayoutFile( "BackgroundMapImageBlur" ) );
	m_pBackgroundImage = panorama::panel_cast< panorama::CImagePanel * >( FindChildInLayoutFile( "BackgroundMapImage" ) );
	m_pLoadingScreenHintText = panorama::panel_cast< CLabel* >( FindChildInLayoutFile( "LoadingScreenHintText" ) );

	RegisterForUnhandledEvent( CSGOLoadProgressChanged(), this, &CCSGO_LoadingScreen::EventLoadProgressChanged );
	RegisterEventHandler( AnimationEnd(), this, &CCSGO_LoadingScreen::EventAnimationEnd );

	GameUI().RegisterGameUIStateListener( this );
	if ( g_pMatchFramework )
		g_pMatchFramework->GetEventsSubscription()->Subscribe( this );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CCSGO_LoadingScreen::~CCSGO_LoadingScreen()
{
	GameUI().UnregisterGameUIStateListener( this );
	if ( g_pMatchFramework )
		g_pMatchFramework->GetEventsSubscription()->Unsubscribe( this );

	Assert( s_pLoadingScreen == this );
	s_pLoadingScreen = NULL;
}

#if defined ( CSTRIKE_TRUNK_BUILD )
CON_COMMAND( dbg_loading, "" )
{
	int stage = V_atoi( args.Arg( 1 ) );
	const char* szMapName = "cs_office";
//	int iGameType = 0;
//	int iGameMode = 0;
	int iSkirmishID = 0;
//	int iEventID = 0;

	if ( stage == 1 )
	{
		CCSGO_LoadingScreen::GetInstance()->GetParentWindow()->SetVisible( true );
		CCSGO_LoadingScreen::GetInstance()->SetLoadingScreenElementsData( szMapName, iSkirmishID );
		CCSGO_LoadingScreen::GetInstance()->LoadingScreenInit();
	}
	else if ( stage == 2 )
	{
		//DispatchEvent( PopulateLoadingScreen(), nullptr, szMapName, "", g_pGameTypes->GetGameTypeFromInt( iGameType ), g_pGameTypes->GetGameModeFromInt( iGameType, iGameMode ), iSkirmishID, "" );
		CCSGO_LoadingScreen::GetInstance()->SetLoadingScreenElementsData( szMapName, iSkirmishID );
		CCSGO_LoadingScreen::GetInstance()->TriggerBackgroundBlur();
	}
	else if ( stage == 3 )
	{
		//DispatchEvent( PopulateLoadingScreen(), nullptr, szMapName, "", g_pGameTypes->GetGameTypeFromInt( iGameType ), g_pGameTypes->GetGameModeFromInt( iGameType, iGameMode ), iSkirmishID, "" );
		CCSGO_LoadingScreen::GetInstance()->SetLoadingScreenElementsData( szMapName, iSkirmishID );

		KeyValues* pkvMapKeyValues = new KeyValues( szMapName );
		char tempfile[ MAX_PATH ];
		Q_snprintf( tempfile, sizeof( tempfile ), "resource/overviews/%s.txt", szMapName );
		if ( pkvMapKeyValues->LoadFromFile( g_pFullFileSystem, tempfile, "GAME" ) )
		{
			// Expensive, but should only happen once per map load
			DispatchEvent( OnMapConfigLoaded(), CCSGO_LoadingScreen::GetInstance(), JSObjectAsKeyValues( pkvMapKeyValues ) );
		}
	}
}
#endif

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void CCSGO_LoadingScreen::FireGameEvent( IGameEvent *pEvent )
{
#if !defined( NO_STEAM ) && !defined( _PS3 )
	if ( !V_strcmp( pEvent->GetName(), "ugc_file_download_finished" ) )
	{
		IMatchSession *pMatchSession = g_pMatchFramework->GetMatchSession();
		if ( pMatchSession )
		{
			char const *szMapName = pMatchSession->GetSessionSettings()->GetString( "game/map", NULL );
			m_nSkirmishId = pMatchSession->GetSessionSettings()->GetInt( "game/skirmishmode", 0 );
			if ( szMapName && !StringIsEmpty( szMapName ) )
			{
				UGCHandle_t hcontent = pEvent->GetUint64( "hcontent", 0 );
				char filebuf[MAX_PATH];
				WorkshopManager().GetUGCFullPath( hcontent, filebuf, sizeof( filebuf ) );

				// if it's a jpg
				if ( !V_stricmp( "jpg", V_GetFileExtension( filebuf ) ) )
				{
					V_FixSlashes( filebuf, '/' );
					char szMapPath[MAX_PATH];
					V_ExtractFilePath( szMapName, szMapPath, sizeof( szMapPath ) );
					// rough check to make sure this is the jpg for this map... possible false positives but not likely
					if ( V_stristr( filebuf, szMapPath ) )
					{
						SetLoadingScreenElementsData( szMapName, m_nSkirmishId );
					}
				}
			}
		}
	}
#endif
}

void CCSGO_LoadingScreen::OnEvent( KeyValues *pEvent )
{
	const char *pEventName = pEvent->GetName();

	if ( !Q_stricmp( pEventName, "OnEngineLevelLoadingStarted" ) )
	{
		char const *szMapName = pEvent->GetString( "name", "" );//V_GetFileName( pEvent->GetString( "name", "" ) );

		SetLoadingScreenElementsData( szMapName, m_nSkirmishId );
	}
	else if ( !Q_stricmp( pEventName, "OnMatchSessionUpdate" ) )
	{
		IMatchSession *pMatchSession = g_pMatchFramework->GetMatchSession();
		if ( pMatchSession && !Q_stricmp( pEvent->GetString( "state" ), "created" ) )
		{
			char const *szMapName = pMatchSession->GetSessionSettings()->GetString( "game/map", NULL );
			m_nSkirmishId = pMatchSession->GetSessionSettings()->GetInt( "game/skirmishmode", 0 );
			if ( szMapName )
			{
				m_serverInfoReady = false;
				SetLoadingScreenElementsData( szMapName, m_nSkirmishId );
			}
		}
		else if ( !Q_stricmp( pEvent->GetString( "state" ), "error" ) )
		{
			// If mm session errors out give us a chance to hide the loading screen... 
			// We're showing loading screen early for smooth transitions before the blocking load process, but
			// that means we can have the loading screen visible before the CSGOGameUIState_t change. 
			// If we displayed the loading screen (prematurely) in EventConnectToServerPending and the connect process
			// errors out for some reason, give it a chance to update here. 
			UpdateVisibility();
		}
	}
	else if ( !Q_stricmp( pEventName, "OnLevelLoadingSetDefaultGameModeAndType" ) )
	{
		char const *szMapFileName = pEvent->GetString( "mapname", "" );
		char const *szMapName = V_GetFileName( szMapFileName );
		m_nSkirmishId = pEvent->GetInt( "skirmishid", 0 );

		m_serverInfoReady = false;
		SetLoadingScreenElementsData( szMapFileName, m_nSkirmishId );

		KeyValues* pkvMapKeyValues = new KeyValues( szMapName );
		char tempfile[MAX_PATH];
		Q_snprintf( tempfile, sizeof( tempfile ), "resource/overviews/%s.txt", szMapName );
		if ( pkvMapKeyValues->LoadFromFile( g_pFullFileSystem, tempfile, "GAME" ) )
		{
			// Expensive, but should only happen once per map load
			DispatchEvent( OnMapConfigLoaded(), this, JSObjectAsKeyValues( pkvMapKeyValues ) );
		}
	}
}

void CCSGO_LoadingScreen::SetLoadingScreenElementsData( const char* mapName, int iSkirmishId )
{
	if ( !m_serverInfoReady && mapName && mapName[0] )
	{
		// these are correct only for demos and local data, we get correct server or matchmaking data later if we need it
		int iGameType = g_pGameTypes->GetCurrentGameType();
		int iGameMode = g_pGameTypes->GetCurrentGameMode();
		char const *pGameType = g_pGameTypes->GetGameTypeFromInt( iGameType );
		char const *pGameMode = g_pGameTypes->GetGameModeFromInt( iGameType, iGameMode );
		int skirmishId = iSkirmishId;

		IMatchSession *pIMatchSession = g_pMatchFramework->GetMatchSession();
		if ( pIMatchSession )
		{
			GetGameModeAndType( iGameMode, iGameType, skirmishId );
		}

		PopulateLevelInfo( mapName, pGameType, pGameMode, iGameType, iGameMode, skirmishId );

		m_serverInfoReady = true;
	}
}



void CCSGO_LoadingScreen::PopulateLevelInfo( const char* mapName, const char* gameTypeNameID, const char* gameModeNameID, int iGameType, int iGameMode, int iSkirmishId )
{
	const int MAP_PREFIX_SIZE = 3;
	if ( !mapName || V_strlen( mapName ) < MAP_PREFIX_SIZE )
	{
		return;
	}

	m_nSkirmishId = iSkirmishId; // save this off in case we get a future update that doesn't change the event id

	bool isGunGameProgressive = ( iGameType == CS_GameType_GunGame ) && ( iGameMode == CS_GameMode::GunGame_Progressive );
	bool isGunGameTRBomb = ( iGameType == CS_GameType_GunGame ) && ( iGameMode == CS_GameMode::GunGame_Bomb );
	bool isTrainingMap = ( iGameType == CS_GameType_Training ) && ( iGameMode == CS_GameMode::Training_Training );
	bool isGunGameDeathmatch = ( iGameType == CS_GameType_GunGame ) && ( iGameMode == CS_GameMode::GunGame_Deathmatch );
	bool isCoopGuardian = ( iGameType == CS_GameType_Cooperative ) && ( iGameMode == CS_GameMode::Cooperative_Guardian );
	bool isCooperativeMission = ( iGameType == CS_GameType_Cooperative ) && ( iGameMode == CS_GameMode::Cooperative_Mission );
	bool isSurvival = ( iGameType == CS_GameType_FreeForAll ) && ( iGameMode == CS_GameMode::FreeForAll_Survival );
	bool isCustomRules = ( iGameType == CS_GameType_Custom );

	//////////////////////////////////////////////////////////////////////////
	// Set Map Name
	//////////////////////////////////////////////////////////////////////////

	const wchar_t* translatedMapName = CCSGameRules::GetFriendlyMapName( mapName );//g_pVGuiLocalize->Find( "GameUI_Stats_RecentAchievements" );
	const char* mapFileName = V_GetFileName( mapName );

	//////////////////////////////////////////////////////////////////////////
	// Set Game Mode, type and/or style
	//////////////////////////////////////////////////////////////////////////

	const wchar_t* szGameModeNiceName = g_pVGuiLocalize->Find( gameModeNameID );

	//////////////////////////////////////////////////////////////////////////
	// Check to see if we need to load a blank loading screen
	//////////////////////////////////////////////////////////////////////////

	bool bHasBGImage = true;

	//////////////////////////////////////////////////////////////////////////
	// Get the map path so we can load the thumbnail
	//////////////////////////////////////////////////////////////////////////
	char szPath[MAX_PATH];
	char szMapID[MAX_PATH];
	char szBGImagePath[MAX_PATH];
	szBGImagePath[0] = '\0';
	//elBackgroundImage.SetImage( 'file://{images}/map_icons/screenshots/1080p/' + mapName +'.png' );

	V_strcpy_safe( szPath, mapName );
	V_FixSlashes( szPath, '/' ); // internal path strings use forward slashes, make sure we compare like that.
	if ( V_strstr( szPath, "workshop/" ) )
	{
		V_snprintf( szMapID, MAX_PATH, "%llu", GetMapIDFromMapPath( szPath ) );
		V_StripFilename( szPath );

		V_snprintf( szBGImagePath, MAX_PATH, "maps/%s/thumb%s.jpg", szPath, szMapID );
		if ( !g_pFullFileSystem->FileExists( szBGImagePath ) )
		{
			V_snprintf( szBGImagePath, MAX_PATH, "maps/%s/%s.jpg", szPath, mapFileName );
			if ( !g_pFullFileSystem->FileExists( szBGImagePath ) )
			{
				// last chance.  try to see if we made one locally
				V_snprintf( szBGImagePath, MAX_PATH, "maps/%s.jpg", mapFileName );
				if ( !g_pFullFileSystem->FileExists( szBGImagePath ) )
				{
					bHasBGImage = false;
				}
			}
		}
	}
	else
	{
		V_snprintf( szBGImagePath, MAX_PATH, "materials/panorama/images/map_icons/screenshots/1080p/%s.png", mapFileName );
		if ( !g_pFullFileSystem->FileExists( szBGImagePath ) )
			bHasBGImage = false;
	}

	if ( bHasBGImage )
	{
		m_pBackgroundImage->SetImage( CFmtStr( "s2r://%s", szBGImagePath ) );
	}
	else
	{
		m_pBackgroundImage->SetImage( "file://{images}/map_icons/screenshots/1080p/default.png" );
	}

	m_pBackgroundImage->SetVisible(true);


	//////////////////////////////////////////////////////////////////////////
	// Show Rules
	//////////////////////////////////////////////////////////////////////////

	wchar_t wszFinalScenarioNameString[1024];
	wszFinalScenarioNameString[0] = '\0';
	const char *scenarioName = "#SFUI_LOADING";
	const wchar_t *scenarioNameCredits = L"";
	wchar_t wszText[1024] = {};

	char textFilename[MAX_PATH];
	V_snprintf( textFilename, MAX_PATH, "maps/%s.txt", mapFileName );

	bool bNeedRules = true;
	// Load the user's text file
	CUtlBuffer buf;
	if ( !V_stricmp( "overwatch", mapFileName ) )
	{
		bNeedRules = false;
		scenarioName = "#CSGO_LoadingScreen_Overwatch";
	}
	else if ( g_pFullFileSystem->ReadFile( textFilename, NULL, buf ) )
	{
		bNeedRules = false;

		if ( ( const char* )buf.Base() )
		{
			char chReplace[3] = { 0xD, 0xA, 0 };
			while ( char *psz = strstr( ( char * )buf.Base(), chReplace ) )
			{
				Q_memmove( psz, psz + 1, Q_strlen( ( char * )buf.Base() ) - ( psz - ( char * )buf.Base() ) );
			}

			if ( StringHasPrefix( ( char * )buf.Base(), "COMMUNITYMAPCREDITS:" ) )
			{
				const char *szText = "";
				szText = ( char * )buf.Base();
				szText += 20;

				wchar_t wszCredits[1024];
				V_UTF8ToUnicode( szText, wszCredits, sizeof( wszCredits ) );

				static const wchar_t * const kwszCommunityMapCreditsToken = g_pVGuiLocalize->Find( "#CSGO_LoadingScreen_CommunityMapCredits" );
				V_swprintf_safe( wszText, L"\n\n<font color='#727272'>" PRI_WS_FOR_WS L"</font><font color='#ff9844'>" PRI_WS_FOR_WS L"\n</font>\n",
								 kwszCommunityMapCreditsToken, wszCredits );

				bNeedRules = true;
			}
			else if ( StringHasPrefix( ( char * )buf.Base(), "RULESHERE" ) )
			{
				const char *szText = "";
				szText = ( char * )buf.Base();
				szText += 9;
				V_UTF8ToUnicode( szText, wszText, sizeof( wszText ) );

				bNeedRules = true;
			}
			else
			{
				V_UTF8ToUnicode( ( const char* )buf.Base(), wszText, sizeof( wszText ) );
			}

			scenarioNameCredits = wszText;
		}
		else
		{
			// the file exists, but it is blank
			wchar_t wszMap[64];
			V_UTF8ToUnicode( textFilename, wszMap, sizeof( wszMap ) );
			V_snwprintf( wszText, ARRAYSIZE( wszText ), L"YOUR TEXT FILE IS BLANK!\n\nIf you intend for this space to be blank, open up '<font color='#72b4d0'>%s</font>', enter a single space and save the file.", wszMap );
			scenarioNameCredits = wszText;
		}
	}

	if ( bNeedRules == true )
	{
		const char* szCustomScenarioName;
		const char* szCustomScenarioRules;

		if ( g_pGameTypes->GetLoadingScreenDataIsCorrect() == false )
		{
			//szGameTypeNiceName = L"";
			szGameModeNiceName = g_pVGuiLocalize->Find( "SFUI_LOADING" );
			scenarioName = "#SFUI_LOADING";
		}
		else if ( iSkirmishId > 0 && GetCustomRules( &szCustomScenarioName, &szCustomScenarioRules, iSkirmishId ) )
		{
			// Change game name for skirmish modes (e.g. "Casual" becomes "Flying Scoutsman")
			if ( szCustomScenarioName != nullptr )
				szGameModeNiceName = g_pVGuiLocalize->Find( szCustomScenarioName );

			// Add the rules
			scenarioName = szCustomScenarioRules;
		}
		else if ( isTrainingMap )
		{
			scenarioName = "#SFUI_Rules_Training_Loading";
		}
		else if ( isGunGameTRBomb )
		{
			scenarioName = "#SFUI_Rules_TRBomb_Loading";
		}
		else if ( isGunGameProgressive )
		{
			scenarioName = "#SFUI_Rules_GunGame_Progressive";
		}
		else if ( isGunGameDeathmatch )
		{
			scenarioName = "#SFUI_Rules_Deathmatch_Loading";
		}
		else if ( isCustomRules )
		{
			scenarioName = "#SFUI_Rules_Custom_Loading";
		}
		else if ( isCoopGuardian )
		{
			scenarioName = "#SFUI_Rules_Guardian_Loading";
		}
		else if ( isCooperativeMission )
		{
			scenarioName = "#SFUI_Rules_CoopMission_Loading";
		}
		else if ( iGameType == CS_GameType_Classic && iGameMode == CS_GameMode::Classic_ScrimComp2v2 )
		{
			scenarioName = "#SFUI_Rules_ScrimComp2v2_Loading";
		}
		else if ( iGameType == CS_GameType_Classic && iGameMode == CS_GameMode::Classic_ScrimComp5v5 )
		{
			scenarioName = "#SFUI_Rules_ScrimComp5v5_Loading";
		}
		else if ( isSurvival )
		{
			scenarioName = "#SFUI_Rules_Survival_Loading";
		}
		else
		{
			if ( StringHasPrefix( mapFileName, "cs_" ) )
			{
				if ( iGameMode == CS_GameMode::Classic_Casual )
				{
					scenarioName = "#SFUI_Rules_Hostage_Loading_Classic";
				}
				else if ( iGameMode == CS_GameMode::Classic_Competitive )
				{
					scenarioName = "#SFUI_Rules_Hostage_Loading_Competetive";
				}
				// 				else if ( iGameMode == CS_GameMode::Classic_Competitive_Unranked )
				// 				{
				// 					scenarioName = "#SFUI_Rules_Hostage_Loading_Competetive_Unranked";
				// 				}
				else
				{
					scenarioName = "#SFUI_Rules_Hostage_Header";
				}
			}
			else if ( StringHasPrefix( mapFileName, "de_" ) )
			{
				if ( iGameMode == CS_GameMode::Classic_Casual )
				{
					scenarioName = "#SFUI_Rules_Bomb_Loading_Classic";
				}
				else if ( iGameMode == CS_GameMode::Classic_Competitive )
				{
					scenarioName = "#SFUI_Rules_Bomb_Loading_Competetive";
				}
				// 				else if ( iGameMode == CS_GameMode::Classic_Competitive_Unranked )
				// 				{
				// 					scenarioName = "#SFUI_Rules_Bomb_Loading_Competetive_Unranked";
				// 				}
				else
				{
					scenarioName = "#SFUI_Rules_Bomb_Header";
				}
			}
			else
			{
				if ( iGameMode == CS_GameMode::Classic_Casual )
				{
					scenarioName = "#SFUI_Rules_ClassicCas_Unknown";
				}
				else if ( iGameMode == CS_GameMode::Classic_Competitive )
				{
					scenarioName = "#SFUI_Rules_ClassicComp_Unknown";
				}
			}
		}
	}

	const char* pGameMode = g_pGameTypes->GetGameModeFromInt( iGameType, iGameMode );

	if ( iSkirmishId > 0 )
	{
		// more exceptions and special cases for skirmish.......
		const CSkirmishModeDefinition* pSkirmishDef = GetItemSchema()->GetSkirmishModeDefinition( iSkirmishId );
		if ( pSkirmishDef != nullptr )
		{	
			pGameMode = pSkirmishDef->GetName();
		}
	}

	// Fill the loading screen with name & credits
	V_swprintf_safe( wszFinalScenarioNameString, PRI_WS_FOR_WS PRI_WS_FOR_WS, g_pVGuiLocalize->Find( scenarioName )/*scenarioTranslated*/, scenarioNameCredits );

	char szMapNameUTF8[512];
	V_UnicodeToUTF8( translatedMapName, szMapNameUTF8, ARRAYSIZE( szMapNameUTF8 ) );
	char szGameModeStringUTF8[4 * 512];
	V_UnicodeToUTF8( szGameModeNiceName, szGameModeStringUTF8, ARRAYSIZE( szGameModeStringUTF8 ) );

	char szScenarioStringUTF8[4*512];
	V_UnicodeToUTF8( wszFinalScenarioNameString, szScenarioStringUTF8, ARRAYSIZE( szScenarioStringUTF8 ) );
	DispatchEvent( PopulateLoadingScreen(), nullptr, mapFileName, szMapNameUTF8, szGameModeStringUTF8, g_pGameTypes->GetGameTypeFromInt( iGameType ), pGameMode, szScenarioStringUTF8 );

	PopulateHintText();

	// ENABLE FOR TESTING SECONDARY PROGRESS BAR
	//{
	//	SetSecondaryProgressText( L"TEST" );
	//	SetSecondaryProgressBar( 0.5f );
	//}
}

void CCSGO_LoadingScreen::PopulateHintText( void )
{
	if ( Plat_FloatTime() < m_flTimeLastHintUpdate + cl_show_new_hint_delay.GetFloat() )
		return;

	const char* pszLocToken = g_CSGameplayHints.GetRandomLeastPlayedHint();
	if ( pszLocToken )
	{
		const wchar_t* pwsText = g_pVGuiLocalize->Find( pszLocToken );
		if ( pwsText )
		{
			wchar_t wzFinal[1024] = L"";
			UTIL_ReplaceKeyBindings( pwsText, 0, wzFinal, sizeof( wzFinal ) );

			m_flTimeLastHintUpdate = Plat_FloatTime();

			char szwzFinalUTF8[4 * 512];
			V_UnicodeToUTF8( wzFinal, szwzFinalUTF8, ARRAYSIZE( szwzFinalUTF8 ) );
			m_pLoadingScreenHintText->SetProceduralTextThatIPromiseIsLocalizedAndEscaped( szwzFinalUTF8, false );
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static bool SetImageIfExists(CImagePanel *pImagePanel, const char* imageURL)
{
	CFileResource fileResource( imageURL );

	if ( fileResource.BIsLocalPath() )
	{
#ifdef CSGO_PORT // CSGO Source2 Port 
		// All local path images are expected to be compiled. The compiler has been hooked into various paths where images can be specified.
		const char *pExtension = V_GetFileExtensionSafe( fileResource.GetReferencePath().Get() );
		if ( !V_stricmp( pExtension, "png" ) || !V_stricmp( pExtension, "tga" ) || !V_stricmp( pExtension, "jpg" ) || !V_stricmp( pExtension, "psd" ) )
		{
			// Change over from known images (that should be compiled) into the compiled variant.
			CUtlString compiledImageString = fileResource.GetReferencePath().StripExtension();
			compiledImageString.Append( CFmtStr( "_%s.vtex_c", pExtension ).Get() );
			fileResource.GetReferencePathForModify() = compiledImageString.Get();
		}
#else
		CUtlString compiledImageString = ( "materials/" );
		compiledImageString.Append( fileResource.GetReferencePath() );
		fileResource.GetReferencePathForModify() = compiledImageString.Get();
#endif
	}

	if ( g_pFullFileSystem->FileExists( fileResource.GetReferencePath(), "GAME" ) )
	{
		pImagePanel->SetImage( imageURL );
		return true;
	}

	return false;
}

void CCSGO_LoadingScreen::GetGameModeAndType( int &nGameMode, int &nGameType, int &skirmishId )
{
	int iGameType = g_pGameTypes->GetCurrentGameType();
	int iGameMode = g_pGameTypes->GetCurrentGameMode();
	char const *pGameType = g_pGameTypes->GetGameTypeFromInt( iGameType );
	char const *pGameMode = g_pGameTypes->GetGameModeFromInt( iGameType, iGameMode );

	IMatchSession *pIMatchSession = g_pMatchFramework->GetMatchSession();
	if ( pIMatchSession )
	{
		// the session has the CORRECT state
		pGameType = pIMatchSession->GetSessionSettings()->GetString( "game/type" );
		pGameMode = pIMatchSession->GetSessionSettings()->GetString( "game/mode" );
		skirmishId = pIMatchSession->GetSessionSettings()->GetInt( "game/skirmishmode", 0 );
	}

	g_pGameTypes->GetGameModeAndTypeIntsFromStrings( pGameType, pGameMode, nGameType, nGameMode );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CCSGO_LoadingScreen::OnMapLoadStarted( const char *pLevelName, bool bIsConnectingToServer )
{
	LoadingScreenInit();

	SetLoadingScreenElementsData( pLevelName, m_nSkirmishId );
}

void CCSGO_LoadingScreen::SetupPanelsFromMapname( void )
{
}

void CCSGO_LoadingScreen::OnMapLoadFinished( void )
{
	m_flLoadingPercent = 1.0f;
	m_flLoadingPercentLerpStart = gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CCSGO_LoadingScreen::OnGameLoopDeactivated()
{
	DestroyLoadingScreen();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CCSGO_LoadingScreen::DestroyLoadingScreen()
{
	RemoveAndDeleteChildren();
}


// HACK-ISH: Show/hide loading screen early, before the actual CSGOGameUIState_t transition to allow the loading screen
// models some time to animate before the blocking load process.
bool CCSGO_LoadingScreen::EventConnectToServerPending( const char* szMapName, double flSecondsUntilConnect )
{
	LoadingScreenInit();
	GetParentWindow()->SetVisible( true );

	TriggerClass( k_symLoadingScreenShow );

	return false; // Let other handlers process this event
}

bool CCSGO_LoadingScreen::EventConnectToServerCanceled()
{
	GetParentWindow()->SetVisible( false );
	return false;
}

bool CCSGO_LoadingScreen::EventAnimationEnd( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, panorama::CPanoramaSymbol symAnimation )
{
	if ( symAnimation == k_symLoadingScreenClosingAnim && BHasClass( k_symLoadingScreenClosing ) )
	{
		LoadingScreenInit();
		RemoveClass( k_symLoadingScreenClosing );
		GetParentWindow()->SetVisible( false );
	}
	if ( symAnimation == k_symLoadingScreenShowAnim )
	{
		RemoveClass( k_symLoadingScreenShow );
	}

	return false;
}

void CCSGO_LoadingScreen::LoadingScreenInit( void )
{
	m_serverInfoReady = false;

	m_flLoadingPercentPrev = 0.0f;
	m_flLoadingPercent = 0.0f;
	m_flLoadingPercentLerpStart = gpGlobals->curtime;
	m_flTimeLastHintUpdate = 0.0f;

	DispatchEvent( UnloadLoadingScreenAndReinit(), nullptr );

	m_pBackgroundMapBlur->RemoveAllClasses();
	m_pBackgroundMapBlur->AddClass( k_symBGBlur );

	m_pProgressBar->SetValue( 0 );
	m_pProgressSecondaryBar->SetVisible( false );
	m_pProgressSecondaryStatusText->SetVisible( false );
	m_pProgressSecondaryBar->SetValue( 0 );

	IUIPanelStyle *pStyle = m_pBackgroundMapBlur->AccessStyle();
	pStyle->SetGaussianBlur( BT_FAST, 5, 5, 3 );

	if ( m_pBackgroundMapBlur->BHasClass( k_symLoadingScreenUnblur ) )
	{
		m_pBackgroundMapBlur->RemoveClass( k_symLoadingScreenUnblur );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CCSGO_LoadingScreen::OnCSGOGameUIStateChange( CSGOGameUIState_t nOldState, CSGOGameUIState_t nNewState )
{
	if ( nNewState == CSGO_GAME_UI_STATE_LOADINGSCREEN )
	{
		LoadingScreenInit();
		if ( BHasClass( k_symLoadingScreenClosing ) )
			RemoveClass( k_symLoadingScreenClosing );

		IMatchSession *pMatchSession = g_pMatchFramework->GetMatchSession();
		if ( pMatchSession )
		{
			SetLoadingScreenElementsData( "", 0 );
		}

		GetParentWindow()->SetVisible( true );
	}
	else if ( nOldState == CSGO_GAME_UI_STATE_LOADINGSCREEN )
	{
		TriggerClass( k_symLoadingScreenClosing );
	}		
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CCSGO_LoadingScreen::UpdateVisibility()
{
	CSGOGameUIState_t nUIState = GameUI().GetGameUIState();
		
	// Determine if the loading screen should be visible
	GetParentWindow()->SetVisible((nUIState == CSGO_GAME_UI_STATE_LOADINGSCREEN));
}

void CCSGO_LoadingScreen::Paint( void )
{
	if ( !GetParentWindow()->BIsVisible() )
		return;

	float flFrac = RemapValClamped( gpGlobals->curtime - m_flLoadingPercentLerpStart, 0, 0.5f, 0, 1 );

	//float flOldPercent = m_pProgressBar->GetValue();
	float flFraction = Lerp( flFrac, m_flLoadingPercentPrev, m_flLoadingPercent );

	if ( m_pProgressBar.Get() )
	{
		m_pProgressBar->SetValue( flFraction );
	}
}

void CCSGO_LoadingScreen::TriggerBackgroundBlur()
{
	m_pBackgroundMapBlur->TriggerClass( k_symLoadingScreenUnblur );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CCSGO_LoadingScreen::Update()
{
	if ( !GetParentWindow()->BIsVisible() )
		return;
}

void CCSGO_LoadingScreen::SetSecondaryProgressText( const wchar_t *desc )
{
	char szProgressUTF8[512];
	V_UnicodeToUTF8( desc, szProgressUTF8, ARRAYSIZE( szProgressUTF8 ) );

	m_pProgressSecondaryStatusText->SetText( szProgressUTF8 );
}

void CCSGO_LoadingScreen::SetSecondaryProgressBar( float flProgress )
{
	if ( flProgress == 1.0f )
	{
		m_pProgressSecondaryBar->SetVisible(false);
		m_pProgressSecondaryStatusText->SetVisible(false);
		return;
	}

	m_pProgressSecondaryBar->SetVisible( true );
	m_pProgressSecondaryStatusText->SetVisible( true );

	m_pProgressSecondaryBar->SetValue(flProgress);
}

bool CCSGO_LoadingScreen::EventLoadProgressChanged(float fPercent, const char *pszStatusText)
{
	if ( m_pProgressBar->GetValue() == m_flLoadingPercentPrev )
		m_flLoadingPercentPrev = fPercent - ((fPercent-m_pProgressBar->GetValue())/2); // jump it if we're updating really fast
	else
		m_flLoadingPercentPrev = m_pProgressBar->GetValue();

	m_flLoadingPercent = fPercent;
	m_flLoadingPercentLerpStart = gpGlobals->curtime;

	if (m_pProgressStatusText.Get() && pszStatusText && Q_strlen( pszStatusText ) >= 1)
	{
		m_pProgressStatusText->SetText(pszStatusText);
	}

	if ( fPercent > 0.25f && !m_pBackgroundMapBlur->BHasClass( k_symLoadingScreenUnblur ) )
		TriggerBackgroundBlur();

	//m_pProgressSecondaryBar->SetValue( fPercent );
	//m_pProgressBar->SetValue( fPercent );

	// this allows us to set data for the loading screen during demo loads
	SetLoadingScreenElementsData( engine->GetLevelNameShort(), m_nSkirmishId );

	PopulateHintText();

	return true;
}
