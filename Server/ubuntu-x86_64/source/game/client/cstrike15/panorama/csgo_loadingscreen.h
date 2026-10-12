//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//
#pragma once

#include "panorama/csgo_panorama.h"
#include "panorama/ui_root.h"
#include "panorama/csgo_blurtarget.h"
#include "GameEventListener.h"
#include "gameui_interface.h"
#include "matchmaking/imatchevents.h"
#include "csgo_panorama_script_bindings.h"


// mapname gametype gamenode, skirmish id, event id
DECLARE_PANORAMA_EVENT6( PopulateLoadingScreen, const char*, const char*, const char*, const char*, const char*, const char* );
DECLARE_PANORAMA_EVENT1( OnMapConfigLoaded, JSObjectAsKeyValues );
DECLARE_PANORAMA_EVENT0( UnloadLoadingScreenAndReinit );

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
class CCSGO_LoadingScreen : public CUI_Root, public ICSGOGameUIStateListener, public IMatchEventsSink
{
	DECLARE_PANEL2D( CCSGO_LoadingScreen, CUI_Root );

public:
	CCSGO_LoadingScreen( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CCSGO_LoadingScreen();

	static CCSGO_LoadingScreen *GetInstance() { return s_pLoadingScreen; }

	// ICSGOGameUIStateListener overrides 
	virtual void OnCSGOGameUIStateChange( CSGOGameUIState_t nOldState, CSGOGameUIState_t nNewState ) OVERRIDE;
	virtual void OnMapLoadStarted( const char *pLevelName, bool bIsConnectingToServer ) OVERRIDE;

	void GetGameModeAndType( int &nGameMode, int &nGameType, int &skirmishId );
	void SetupPanelsFromMapname( void );

	virtual void OnMapLoadFinished( void ) OVERRIDE;
	virtual void OnGameLoopDeactivated() OVERRIDE;
	void LoadingScreenInit( void );
	void TriggerBackgroundBlur( void );

	virtual void	Paint( void ) OVERRIDE;

	// game events
	virtual void FireGameEvent( IGameEvent *pEvent ) OVERRIDE;

	// Matchmaking events
	virtual void OnEvent( KeyValues *pEvent ) OVERRIDE;

	// panorama events 
	bool EventLoadProgressChanged(float fPercent, const char *pszStatusText);
	bool EventConnectToServerPending( const char* szMapName, double flSecondsUntilConnect );
	bool EventConnectToServerCanceled();
	bool EventAnimationEnd( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, panorama::CPanoramaSymbol symAnimation );

	void Update();

	void SetLoadingScreenElementsData( const char* mapName, int iSkirmishId );
	void SetSecondaryProgressText( const wchar_t *desc );
	void SetSecondaryProgressBar( float flProgress );


private:
	void UpdateVisibility();
	void DestroyLoadingScreen();
	void PopulateLevelInfo( const char* mapName, const char* gameTypeNameID, const char* gameModeNameID, int iGameType, int iGameMode, int iSkirmishId );
	void PopulateHintText( void );
	

	panorama::CPanelPtr< panorama::CProgressBar> m_pProgressBar;
	panorama::CPanelPtr< panorama::CLabel> m_pProgressStatusText;
	panorama::CPanelPtr< panorama::CProgressBar> m_pProgressSecondaryBar;
	panorama::CPanelPtr< panorama::CLabel> m_pProgressSecondaryStatusText;
	panorama::CPanelPtr< CCSGO_BlurTarget> m_pBackgroundMapBlur;
	panorama::CImagePanel *m_pBackgroundImage;
	panorama::CPanelPtr< panorama::CLabel> m_pLoadingScreenHintText;
	
	static CCSGO_LoadingScreen *s_pLoadingScreen;

	float m_flLoadingPercentPrev;
	float m_flLoadingPercent;
	float m_flLoadingPercentLerpStart;
	float m_flTimeLastHintUpdate;

	int m_nSkirmishId;
	bool m_bCheckedForSWFAndFailed;

	bool				m_serverInfoReady;	
	
};


