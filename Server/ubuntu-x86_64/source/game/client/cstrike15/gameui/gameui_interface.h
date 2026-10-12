//========= Copyright  1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Defines the interface that the GameUI dll exports
//
// $NoKeywords: $
//=============================================================================//

#ifndef GAMEUI_INTERFACE_H
#define GAMEUI_INTERFACE_H
#pragma once

#include "GameUI/IGameUI.h"

#include "vgui_controls/Panel.h"
#include "vgui_controls/PHandle.h"
#include "convar.h"

#if defined( INCLUDE_SCALEFORM )
#include "scaleformui/scaleformui.h"
#endif

#include "uicomponents/uicomponent_common.h"

#if defined(_PS3)
	#define BACKGROUND_MUSIC_FILENAME "gamestartup.ps3.wav"
#else
	#define BACKGROUND_MUSIC_FILENAME "mainmenu.mp3"
#endif

#define MAX_BACKGROUND_MUSIC 3

class IGameClientExports;
class IMatchmakingStatus;
class CCommand;
class CUI_Root;

namespace panorama { class IUIWindow; }

//--------------------------------------------------------------------------------------------------
// CSGOGameUIState_t - Unified handling of the various high-level states the game UI can be in
//--------------------------------------------------------------------------------------------------
enum CSGOGameUIState_t
{
	CSGO_GAME_UI_STATE_INVALID,
	CSGO_GAME_UI_STATE_LOADINGSCREEN,
	CSGO_GAME_UI_STATE_INGAME,
	CSGO_GAME_UI_STATE_MAINMENU,
	CSGO_GAME_UI_STATE_PAUSEMENU,
	CSGO_GAME_UI_STATE_INTROMOVIE,
	CSGO_GAME_UI_STATE_COUNT
};

inline const char *CSGOGameUIStateName(CSGOGameUIState_t n)
{
	if (n < 0 || n >= CSGO_GAME_UI_STATE_COUNT)
		return "<BAD GAMEUI STATE>";

	const char *s_GameUIStateNames[] =
	{
		"CSGO_GAME_UI_STATE_INVALID",
		"CSGO_GAME_UI_STATE_LOADINGSCREEN",
		"CSGO_GAME_UI_STATE_INGAME",
		"CSGO_GAME_UI_STATE_MAINMENU",
		"CSGO_GAME_UI_STATE_PAUSEMENU",
		"CSGO_GAME_UI_STATE_INTROMOVIE",
	};

	COMPILE_TIME_ASSERT(ARRAYSIZE(s_GameUIStateNames) == CSGO_GAME_UI_STATE_COUNT);

	return s_GameUIStateNames[n];
}

class ICSGOGameUIStateListener
{
public:
	virtual void OnMapLoadStarted(const char *pLevelName, bool bIsConnectingToServer) {}
	virtual void OnMapLoadFinished() {}
	virtual void OnGameLoopDeactivated() {}
	virtual void OnCSGOGameUIStateChange(CSGOGameUIState_t nOldState, CSGOGameUIState_t nNewState) {}
};

#ifdef PANORAMA_ENABLE
#include "panorama/uievent.h"
DECLARE_PANORAMA_EVENT2( CSGOLoadProgressChanged, float, const char* ); // percent, status text
#endif

//-----------------------------------------------------------------------------
// Purpose: Implementation of GameUI's exposed interface 
//-----------------------------------------------------------------------------
class CGameUI : public IGameUI
{
public:
	CGameUI();
	~CGameUI();

	virtual void Initialize( CreateInterfaceFn appFactory );

// #ifdef PANORAMA_ENABLE_
	void CreateGameSpecificUI();
	void DestroyGameSpecificUI();
//#endif

	virtual void Connect( CreateInterfaceFn gameFactory );
	virtual void Start();
	virtual void Shutdown();
	virtual void RunFrame();
	virtual void PostInit();

	// plays the startup mp3 when GameUI starts
	void PlayGameStartupSound();

	// Engine wrappers for activating / hiding the gameUI
	void ActivateGameUI();
	void HideGameUI();

	// Toggle allowing the engine to hide the game UI with the escape key
	void PreventEngineHideGameUI();
	void AllowEngineHideGameUI();

	virtual void SetLoadingBackgroundDialog( vgui::VPANEL panel );

	// notifications
	virtual void OnGameUIActivated() OVERRIDE;
	virtual void OnGameUIHidden() OVERRIDE;
	virtual void OLD_OnConnectToServer( const char *game, int IP, int port ) OVERRIDE;	// OLD: use OnConnectToServer2
	virtual void OnConnectToServer2( const char *game, int IP, int connectionPort, int queryPort ) OVERRIDE;
	virtual void OnDisconnectFromServer( uint8 eSteamLoginFailure ) OVERRIDE;
	virtual void OnLevelLoadingStarted( const char *levelName, bool bShowProgressDialog ) OVERRIDE;

	void RequireLoadingScreenShown( const char * levelName, bool bShowProgressDialog );

	virtual void OnLevelLoadingFinished( bool bError, const char *failureReason, const char *extendedReason ) OVERRIDE;
	virtual void OnDisconnectFromServer_OLD( uint8 eSteamLoginFailure, const char *username ) { OnDisconnectFromServer( eSteamLoginFailure ); }
	virtual void StartLoadingScreenForCommand( const char* command ) OVERRIDE;
	virtual void StartLoadingScreenForKeyValues( KeyValues* keyValues ) OVERRIDE;
	virtual void ShowCustomProgress( void ) OVERRIDE;

	// progress
	virtual bool UpdateProgressBar(float progress, const char *statusText, bool showDialog = true );
	// Shows progress desc, returns previous setting... (used with custom progress bars )
	virtual bool SetShowProgressText( bool show );
	virtual bool UpdateSecondaryProgressBar(float progress, const wchar_t *desc );

	// Xbox 360
	virtual void ShowMessageDialog( const uint nType, vgui::Panel *pOwner = NULL ); // X360 ONLY
	virtual void ShowMessageDialog( const char* messageID, const char* titleID ); //X360 ONLY

	virtual void CreateCommandMsgBox( const char* pszTitle, const char* pszMessage, bool showOk = true, bool showCancel = false, const char* okCommand = NULL, const char* cancelCommand = NULL, const char* closedCommand = NULL, const char* pszLegend = NULL, const char* szPanelID = nullptr );
	virtual void CreateCommandMsgBoxInSlot( ECommandMsgBoxSlot slot, const char* pszTitle, const char* pszMessage, bool showOk = true, bool showCancel = false, const char* okCommand = NULL, const char* cancelCommand = NULL, const char* closedCommand = NULL, const char* pszLegend = NULL );
	virtual void CreateMainMenuCommandMsgBox( const char* pszTitle, const char* pszMessage, bool showOk = true, bool showCancel = false, const char* okCommand = NULL, const char* cancelCommand = NULL, const char* closedCommand = NULL, const char* pszLegend = NULL, const char* szPanelID = nullptr );
#ifdef PANORAMA_ENABLE
	void CreateCommandMsgBoxForPopupManager( class CUI_PopupManager* pPopupManager, const char* pszTitle, const char* pszMessage, bool showOk, bool showCancel, const char* okCommand, const char* cancelCommand, const char* closedCommand, const char* pszLegend, const char* szPanelID = nullptr );
	bool BAnyPopupShown( void ) const;
#endif

	// Close a popup window by id. Only used by panorama because  
	// existing SF code closes all popups through CMessageBoxScaleform::UnloadAllDialogs when it wants to close one 
	void CloseCommandMsgBox( const char *szPanelID );

	void UnloadAllScaleformDialogs( bool bClosePriorityMsgBoxes = true );

	IMatchmakingStatus* CreateMatchmakingStatusDialog( char const *szCustomTitle = "#SFUI_MMStatus_Title", char const *szCustomText = "#SFUI_MMStatus_Searching" );

	// Allows the level loading progress to show map-specific info
	virtual void SetProgressLevelName( const char *levelName );

 	virtual void NeedConnectionProblemWaitScreen();

	virtual void ShowPasswordUI( char const *pchCurrentPW );

	virtual bool LoadingProgressWantsIsolatedRender( bool bContextValid );

	virtual void RestoreTopLevelMenu();

 	virtual void SetProgressOnStart();
 
#if defined( _GAMECONSOLE ) && defined( _DEMO )
	virtual void OnDemoTimeout();
#endif

 	// state
 	bool IsInLevel();
 	bool IsInBackgroundLevel();
 	bool IsInMultiplayer();
 	bool IsConsoleUI();
 	bool HasSavedThisMenuSession();
 	void SetSavedThisMenuSession( bool bState );
 
 	void ShowLoadingBackgroundDialog();
	void HideLoadingBackgroundDialog();
	bool HasLoadingBackgroundDialog();

	virtual bool IsPlayingFullScreenVideo();

	virtual bool IsTransitionEffectEnabled();

	virtual void SetBackgroundMusicDesired( bool bPlayMusic, bool bAllowFinishCurrentTrack=false );

	virtual bool IsInGameUiFrontEndInteractiveState() OVERRIDE;

	void SetPreviewBackgroundMusic( const char * pchPreviewMusicPrefix );

	void ReleaseBackgroundMusic( void );

	void StartBackgroundMusicFade( void );

	void PlayQuestAudio( const char * pchAudioFile );
	void StopQuestAudio( void );
	bool IsQuestAudioPlaying( void );

	bool IsPanoramaEnabled() { return m_bPanoramaEnabled; }
// #ifdef PANORAMA_ENABLE_
	void ChangeGameUIState(CSGOGameUIState_t nNewState);
	CSGOGameUIState_t GetGameUIState();
	void RegisterGameUIStateListener(ICSGOGameUIStateListener *pListener);
	void UnregisterGameUIStateListener(ICSGOGameUIStateListener *pListener);
// #endif // PANORAMA_ENABLE 
	CUtlVector< IUiComponentGlobalInstanceBase * > & GetUiComponents()
	{
		return m_arrUiComponents;
	}


#if PANORAMA_ENABLE
	bool IsLoadingScreenVisible();
#endif

#if defined ( PANORAMA_ENABLE )
	bool PlayUISoundScript( const char* szSoundScriptName, class panorama::IUIPanelClient* pPanel = nullptr );
#endif


private:
	void SendConnectedToGameMessage();

	virtual void StartProgressBar();
	virtual bool ContinueProgressBar(float progressFraction, bool showDialog = true );
	virtual void StopProgressBar(bool bError, const char *failureReason, const char *extendedReason = NULL);
	virtual bool SetProgressBarStatusText(const char *statusText, bool showDialog = true );

	//!! these functions currently not implemented
	virtual void SetSecondaryProgressBar(float progress /* range [0..1] */);
	virtual void SetSecondaryProgressBarText( const wchar_t *desc );

	bool FindPlatformDirectory(char *platformDir, int bufferSize);
	void GetUpdateVersion( char *pszProd, char *pszVer);
	void ValidateCDKey();

	class CUI_PopupManager* GetPopupManagerForUIState() const;

	bool IsBackgroundMusicPlaying( void );
	void UpdateBackgroundMusic( void );

	bool IsIntroMovieEnabled();

	CreateInterfaceFn m_GameFactory;

	bool m_bTryingToLoadFriends : 1;
	bool m_bActivatedUI : 1;
	bool m_bIsConsoleUI : 1;
	bool m_bHasSavedThisMenuSession : 1;
	bool m_bOpenProgressOnStart : 1;

	int m_iGameIP;
	int m_iGameConnectionPort;
	int m_iGameQueryPort;
	
	int m_iFriendsLoadPauseFrames;
	int m_iPlayGameStartupSound;

	char m_szPreviousStatusText[128];
	char m_szPlatformDir[MAX_PATH];

	vgui::DHANDLE<class CCDKeyEntryDialog> m_hCDKeyEntryDialog;

	int m_nBackgroundMusicGUID;
	bool m_bBackgroundMusicDesired;
	bool m_bBackgroundMusicAllowFinishTrack;
	int m_nBackgroundMusicVersion;
	float m_flBackgroundMusicStopTime;
	const char *m_pMusicExtension;

	const char *m_pPreviewMusicExtension;
	float m_flMainMenuMusicVolume;
	float m_flMasterMusicVolume;
	int m_nMusicMinRestartDelay;
	int m_nMusicMaxRestartDelay;


	float m_flQuestAudioTimeEnd;
	float m_flMasterMusicVolumeSavedForMissionAudio;
	float m_flMenuMusicVolumeSavedForMissionAudio;

	int m_nQuestAudioGUID;

	const char * m_pAudioFile;

// #ifdef PANORAMA_ENABLE_
	CSGOGameUIState_t m_CSGOGameUIState;
	CUtlVector< ICSGOGameUIStateListener* > m_GameUIStateListeners;

	bool m_bFirstActivationForSession;
	bool m_bInLevelLoading;
// #endif
	bool m_bPanoramaEnabled;
	CUtlVector< panorama::IUIWindow* > m_arrPanoramaWindows;

	CUtlVector< IUiComponentGlobalInstanceBase * > m_arrUiComponents;

};

// Purpose: singleton accessor
extern CGameUI &GameUI();

// expose client interface
extern IGameClientExports *GameClientExports();

#if defined(INCLUDE_SCALEFORM)
extern IScaleformUI* ScaleformUI();
#endif


#endif // GAMEUI_INTERFACE_H
