//========= Copyright  1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CSTRIKE15BASEPANEL_H
#define CSTRIKE15BASEPANEL_H

#ifndef CLIENT_DLL
#error "This file should not be included outside Client as class def won't match. See https://confluence.valvesoftware.com/display/CSGO/Removing+Scaleform"
#endif

#ifdef _WIN32
#pragma once
#endif

#ifndef CLIENT_DLL
#error This file should only be included in client
#endif

#ifdef _PS3
#include "steam/steam_api.h"
#endif

#include "basepanel.h"

#ifdef INCLUDE_SCALEFORM
#include "messagebox_scaleform.h"
#endif

#include "matchmaking/imatchevents.h"

#include "GameEventListener.h"

#if defined( INCLUDE_SCALEFORM )
#include "splitscreensignon.h"
#endif

#ifdef _PS3
void MarkRegisteredKnownConsoleUserSteamIDToUnregisterLater( CSteamID steamIdConsoleUser );
void ConfigurePSNPresenceStatusBasedOnCurrentSessionState( bool bCanUseSession = true );
#endif

//-----------------------------------------------------------------------------
// Purpose: This is the panel at the top of the panel hierarchy for GameUI
//			It handles all the menus, background images, and loading dialogs
//-----------------------------------------------------------------------------
class CCStrike15BasePanel: public CBaseModPanel, 
#ifdef INCLUDE_SCALEFORM
	public IMessageBoxEventCallback,
#endif
	public IMatchEventsSink
{
	DECLARE_CLASS_SIMPLE( CCStrike15BasePanel, CBaseModPanel );

public:
	CCStrike15BasePanel();
	virtual ~CCStrike15BasePanel();

	virtual void OnEvent( KeyValues *pEvent );

#if defined( _X360 )
	// Prompts the user via the Xbox Guide to switch to Game Chat channel, as necessary
	void	Xbox_PromptSwitchToGameVoiceChannel( void );

	// Is the local user using Party Chat currently?
	bool	Xbox_IsPartyChatEnabled( void );
#endif // _X360

// TODONOSF

#if defined(INCLUDE_SCALEFORM)
	virtual void OnOpenCreateStartScreen( void ); // [jason] provides the "Press Start" screen interface
	virtual void DismissStartScreen( void );
	virtual bool IsStartScreenActive( void );

	virtual void OnOpenCreateMainMenuScreen( void ); 
	virtual void DismissMainMenuScreen( void );
	virtual void RestoreMainMenuScreen( void );
	virtual void DismissAllMainMenuScreens( bool bHideMainMenuOnly = false );

	void RestoreMPGameMenu( void );

	virtual void ShowScaleformMainMenu( bool bShow );
	virtual bool IsScaleformMainMenuActive( void );

	virtual void OnOpenCreateSingleplayerGameDialog( bool bMatchmakingFilter );
	virtual void OnOpenCreateMultiplayerGameDialog( void );
	virtual void OnOpenCreateMultiplayerGameCommunity( void );
	virtual void OnOpenDisconnectConfirmationDialog( void );
	virtual void OnOpenQuitConfirmationDialog( bool bForceToDesktop = false );
#else

	// Overrides to disable VGUI screens when running with Panorama
	virtual void OnOpenCreateStartScreen( void ) {};

	virtual void OnOpenCreateMainMenuScreen( void ) {}
	virtual void DismissMainMenuScreen( void ) {}
	virtual void RestoreMainMenuScreen( void ) {}
	virtual void DismissAllMainMenuScreens( bool bHideMainMenuOnly = false ) {}

	virtual void OnOpenPauseMenu( void ) {}
	virtual void DismissPauseMenu( void ) {}
	virtual void RestorePauseMenu( void ) {}

	virtual void OnOpenCreateMultiplayerGameDialog( void ) {}
#if defined(PANORAMA_ENABLE)
	virtual void OnOpenQuitConfirmationDialog( bool bForceToDesktop = false ) OVERRIDE;
#else
	virtual void OnOpenQuitConfirmationDialog( bool bForceToDesktop = false ) OVERRIDE { BaseClass::OnOpenQuitConfirmationDialog(bForceToDesktop); }
#endif

#endif


	virtual	void OnOpenServerBrowser();

#if defined(INCLUDE_SCALEFORM)
	virtual void OnOpenCreateLobbyScreen( bool bIsHost = false, const char* szGameMode = nullptr );
	virtual void OnOpenLobbyBrowserScreen( bool bIsHost = false );
	virtual void UpdateLobbyScreen( void );
	virtual void UpdateMainMenuScreen();
	virtual void UpdateLobbyBrowser( void );

	virtual void OnOpenMessageBox( char const *pszTitle, char const *pszMessage, char const *pszButtonLegend, DWORD dwFlags, IMessageBoxEventCallback *pEventCallback = NULL, CMessageBoxScaleform** ppInstance = NULL, wchar_t const *pszWideMessage = NULL );
	virtual void OnOpenMessageBoxInSlot( int slot, char const *pszTitle, char const *pszMessage, char const *pszButtonLegend, DWORD dwFlags, IMessageBoxEventCallback *pEventCallback = NULL, CMessageBoxScaleform** ppInstance = NULL );
	virtual void OnOpenMessageBoxThreeway( char const *pszTitle, char const *pszMessage, char const *pszButtonLegend, char const *pszThirdButtonLabel, DWORD dwFlags, IMessageBoxEventCallback *pEventCallback = NULL, CMessageBoxScaleform** ppInstance = NULL );

	virtual void CreateCommandMsgBox( const char* pszTitle, const char* pszMessage, bool showOk = true, bool showCancel = false, const char* okCommand = NULL, const char* cancelCommand = NULL, const char* closedCommand = NULL, const char* pszLegend = NULL );
	virtual void CreateCommandMsgBoxInSlot( ECommandMsgBoxSlot slot, const char* pszTitle, const char* pszMessage, bool showOk = true, bool showCancel = false, const char* okCommand = NULL, const char* cancelCommand = NULL, const char* closedCommand = NULL, const char* pszLegend = NULL );

	virtual void ShowMatchmakingStatus( void );

	// returns true if message box is displayed successfully
	virtual bool ShowLockInput(  void );

	virtual void OnOpenPauseMenu( void );
	virtual void DismissPauseMenu( void );
	virtual void RestorePauseMenu( void );
	virtual void OnOpenControllerDialog( void );
	virtual void OnOpenSettingsDialog( void );
	virtual void OnOpenMouseDialog();
	virtual void OnOpenKeyboardDialog();
	virtual void OnOpenMotionControllerMoveDialog();
	virtual void OnOpenMotionControllerSharpshooterDialog();
	virtual void OnOpenMotionControllerDialog();
	virtual void OnOpenMotionCalibrationDialog();
	virtual void OnOpenVideoSettingsDialog();
	virtual void OnOpenOptionsQueued();
	virtual void OnOpenAudioSettingsDialog();

	virtual void OnOpenUpsellDialog( void );

	virtual void OnOpenHowToPlayDialog( void );	
	

	virtual void ShowScaleformPauseMenu( bool bShow );
	virtual bool IsScaleformPauseMenuActive( void );
	virtual bool IsScaleformPauseMenuVisible( void );

	virtual bool OnMessageBoxEvent( MessageBoxFlags_t buttonPressed );

	virtual void OnOpenMedalsDialog();
	virtual void OnOpenStatsDialog();
	virtual void CloseMedalsStatsDialog();


	virtual void OnOpenLeaderboardsDialog();
	virtual void OnOpenCallVoteDialog();
	virtual void OnOpenMarketplace();
	virtual void UpdateLeaderboardsDialog();
	virtual void CloseLeaderboardsDialog();
	virtual void StartExitingProcess( void );
#endif

	virtual void RunFrame( void );
	bool OnMatchInfoStateChange(const char* pMatchInfo, uint64 nToken, int nDownloading);

#if defined( INCLUDE_SCALEFORM )
	void DoCommunityQuickPlay( void );

protected:
	virtual void LockInput( void );
	virtual void UnlockInput( void );

    virtual bool IsScaleformIntroMovieEnabled( void );
    virtual void CreateScaleformIntroMovie( void );
    virtual void DismissScaleformIntroMovie( void );
   	virtual void OnPlayCreditsVideo( void );

    void CheckIntroMovieStaticDependencies( void );


#endif // INCLUDE_SCALEFORM

protected:

	enum CCSOnClosedCommand
	{
		ON_CLOSED_NULL,
		ON_CLOSED_DISCONNECT,
		ON_CLOSED_QUIT,
		ON_CLOSED_RESTORE_PAUSE_MENU,
		ON_CLOSED_RESTORE_MAIN_MENU,
		ON_CLOSED_DISCONNECT_TO_MP_GAME_MENU, // quit from a game and return to the create game menu instead of main menu
	};

	CCSOnClosedCommand m_OnClosedCommand;

	bool	m_bMigratingActive;

	bool	m_bShowRequiredGameVoiceChannelUI;
	CountdownTimer m_GameVoiceChannelRecheckTimer;

    bool m_bNeedToStartSFIntroMovie;
    bool m_bTestedStaticIntroMovieDependencies;

#if defined ( INCLUDE_SCALEFORM)

private:

#if defined ( _PS3 )&& !defined ( NO_STEAM )

	void OnGameBootCheckInvites();
	void OnGameBootInstallTrophies();
	void ShowFatalError( uint32 unSize );
	void PerformPS3GameBootWork();
	void OnGameBootVerifyPs3DRM();
	void OnGameBootDrmVerified();
	void OnGameBootSaveContainerReady();

	STEAM_CALLBACK_MANUAL( CCStrike15BasePanel, Steam_OnUserStatsReceived, UserStatsReceived_t, m_CallbackOnUserStatsReceived );
	STEAM_CALLBACK_MANUAL( CCStrike15BasePanel, Steam_OnPS3TrophiesInstalled, PS3TrophiesInstalled_t, m_CallbackOnPS3TrophiesInstalled );
	STEAM_CALLBACK_MANUAL( CCStrike15BasePanel, Steam_OnPSNGameBootInviteResult, PSNGameBootInviteResult_t, m_CallbackOnPSNGameBootInviteResult );
	STEAM_CALLBACK_MANUAL( CCStrike15BasePanel, Steam_OnLobbyInvite, LobbyInvite_t, m_CallbackOnLobbyInvite );

#endif// _PS3 && !NO_STEAM

#endif	// Scaleform


#ifdef INCLUDE_SCALEFORM
	SplitScreenSignonWidget* m_pSplitScreenSignon;
	bool m_bStartLogoIsShowing;
	bool m_bServerBrowserWarningRaised;
	bool m_bCommunityQuickPlayWarningRaised;
	bool m_bCommunityServerWarningIssued;
	bool m_bGameIsShuttingDown;
#endif	// INCLUDE_SCALEFORM
};

#ifdef _GAMECONSOLE
void GameStats_UserStartedPlaying( float flTime );
void GameStats_ReportAction( char const *szReportAction );
#else
inline void GameStats_UserStartedPlaying( float flTime ) {}
inline void GameStats_ReportAction( char const *szReportAction ) {}
#endif

#endif // CSTRIKE15BASEPANEL_H

