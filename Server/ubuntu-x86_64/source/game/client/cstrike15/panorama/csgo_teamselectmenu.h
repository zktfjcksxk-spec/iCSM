//========= Copyright © Valve Corporation, All rights reserved. ============//
//
// Purpose:  Panorama menu for selecting team
//
//=====================================================================================//
#pragma once

#include "panorama/csgo_panorama.h"
#include "panorama/controls/countdown.h"
#include "gameui_interface.h"
#include "panorama/uiinputcapture.h"
#include "panorama/ui_itempreview_panel.h"

DECLARE_PANORAMA_EVENT1( CSGOShowTeamSelectMenu, bool );
DECLARE_PANORAMA_EVENT3( PlayerTeamChanged, uint64, int32, int32 );
DECLARE_PANORAMA_EVENT3( LocalPlayerTeamChanged, uint64, int32, int32 );
DECLARE_PANORAMA_EVENT1( ServerForcingTeamJoin, float );
DECLARE_PANORAMA_EVENT1( TeamJoinFailed, const char* );

class CCSGO_TeamSelectMenu : public panorama::CPanel2D, public CGameEventListener, public ICSGOGameUIStateListener
{
	DECLARE_PANEL2D( CCSGO_TeamSelectMenu, panorama::CPanel2D );

public:
	explicit CCSGO_TeamSelectMenu( panorama::CPanel2D *pParent, const char *pchID );

	void ReloadLayout();

	virtual ~CCSGO_TeamSelectMenu();

	virtual void SetupJavascriptObjectTemplate() OVERRIDE;

	virtual void OnCSGOGameUIStateChange( CSGOGameUIState_t nOldState, CSGOGameUIState_t nNewState ) OVERRIDE;

	bool Show( bool bShow );

	// Game Event listener
	virtual void FireGameEvent( IGameEvent * event ) OVERRIDE;

protected:
	bool OnPlayerTeamChange( XUID id, int32 iPrevTeam, int32 iCurTeam );
	bool OnServerForcingTeamJoin( float flTime );
	bool OnMatchEndRestart( void );
	bool EventServerSpawn( void );

	void BuildDebugPanel( panorama::CPanel2D* pPanel );
	const char* GetCTModel( void ) const;
	const char* GetTModel( void ) const;
	int GetTeamNumber( void ) const;

	panorama::CPanelPtr< CUI_ItemPreviewPanel > m_pTeamCharacterCT;
	panorama::CPanelPtr< CUI_ItemPreviewPanel > m_pTeamCharacterTerrorist;

	panorama::CPanelPtr< panorama::CCountdown > m_pTimer;
	panorama::CGameInputCapture m_Capture;

	CUtlString m_strCTModel;
	CUtlString m_strTModel;
};