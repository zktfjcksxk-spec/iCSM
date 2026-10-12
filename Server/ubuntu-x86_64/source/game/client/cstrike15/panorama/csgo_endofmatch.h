//========= Copyright (C) Valve Corporation, All rights reserved. ============//
//
// Panorama port of scoreboard_scaleform.h
//
//=============================================================================//

#pragma once

#include "panorama/csgo_panorama.h"

#include "panorama/csgo_panorama_script_bindings.h"

#include "panorama/ui_root.h"
#include "gameui_interface.h"

#include "cs_shareddefs.h"

#include "cstrike15_item_inventory.h"
#include "panorama/hud/csgo_hud.h"

#include "panorama/uiinputcapture.h"

#include "tier1/utlpointers.h"



DECLARE_PANORAMA_EVENT1( EndOfMatch_Show, bool );
DECLARE_PANORAMA_EVENT0( EndOfMatch_Shutdown );
DECLARE_PANORAMA_EVENT0( EndOfMatch_Survival_Spectate_Clicked );

DECLARE_PANORAMA_EVENT0( Scoreboard_OnEndOfMatch );

class CCSGO_Scoreboard;
class CCSGO_SurvivalEndOfMatch;

class CCSGO_EndOfMatch : public panorama::CPanel2D, CGameEventListener, CAutoGameSystem
{
	DECLARE_PANEL2D( CCSGO_EndOfMatch, panorama::CPanel2D );

public:

	CCSGO_EndOfMatch( panorama::CPanel2D *pParent, const char *pchID );
	~CCSGO_EndOfMatch();

	bool IsOpen( void ) { return m_bOpen; }

	static CCSGO_EndOfMatch *GetInstance() { return s_pEndOfMatch; }
	virtual void SetupJavascriptObjectTemplate() OVERRIDE;

	// CAutoGameSystem methods
	virtual void LevelShutdownPreEntity() OVERRIDE;

	// CGameEventListener methods
	virtual void FireGameEvent( IGameEvent *event ) OVERRIDE;

	// event handlers
	bool OnEndOfMatchSurvivalSpectateClicked();
	bool OnEndOfMatchSurvivalDeath();

private:
	static CCSGO_EndOfMatch *s_pEndOfMatch;

	void UpdateVisibleState( bool bHardCut = false );
	void CloseEndOfMatch( void );
	void OpenEndOfMatch( bool bHardCut );

	void SetMouseCapture( bool bVal );
	void LatchPlayerData( void );

	bool m_bOpen;
	bool m_bMatchOver;
	bool m_bSurvivalSpectateMode;
	bool m_bInReplay;

	panorama::CGameInputCapture m_Capture;

	panorama::CPanelPtr< CCSGO_Scoreboard > m_pScoreboard;
	panorama::CPanelPtr< CPanel2D > m_pScoreboardHolder;
	panorama::CPanelPtr< CCSGO_SurvivalEndOfMatch > m_pSurvivalEndOfMatch;
};


DECLARE_PANORAMA_EVENT0( ItemsDroppedDuringMatchReady );

class CCSGO_EndOfMatchItemDropsPanel : public panorama::CPanel2D
{
	DECLARE_PANEL2D( CCSGO_EndOfMatchItemDropsPanel, panorama::CPanel2D );

public:

	CCSGO_EndOfMatchItemDropsPanel( panorama::CPanel2D *pParent, const char *pchID );
	~CCSGO_EndOfMatchItemDropsPanel();

	void ProcessDropList();

	bool IsDropListReady();
	bool HasDropTimeEnded();
	float GetDropTimeRemaining();

	virtual void SetupJavascriptObjectTemplate() OVERRIDE;


private:

	v8::Local< v8::Object > GetDropListJSO();

protected:
	UtlOwnedPtr<KeyValues> m_pKVDropList;
	PANORAMA_DELCARE_EXPOSED_KEYVALUE_MEMBER( m_pKVDropList )

};

DECLARE_PANORAMA_EVENT0( XPProgressReady );

class CCSGO_EndOfMatchXpPanel : public panorama::CPanel2D
{
	DECLARE_PANEL2D( CCSGO_EndOfMatchXpPanel, panorama::CPanel2D );

public:
	CCSGO_EndOfMatchXpPanel( panorama::CPanel2D *pParent, const char *pchID );
	~CCSGO_EndOfMatchXpPanel();

	void ProcessXpMsg();
	bool IsXpDataReady();

	virtual void SetupJavascriptObjectTemplate() OVERRIDE;

private:
	v8::Local< v8::Object > GetXpDataJSO();

protected:
	UtlOwnedPtr<KeyValues> m_pKVXpData;
	PANORAMA_DELCARE_EXPOSED_KEYVALUE_MEMBER( m_pKVXpData )
};


class CCSGO_EndOfMatchSkillgroupPanel : public panorama::CPanel2D
{
	DECLARE_PANEL2D( CCSGO_EndOfMatchSkillgroupPanel, panorama::CPanel2D );

public:
	CCSGO_EndOfMatchSkillgroupPanel( panorama::CPanel2D *pParent, const char *pchID );
	~CCSGO_EndOfMatchSkillgroupPanel();

	void ProcessServerUpdateMsg(); // now we need it. convert it to keyvalues that panorama can read
	bool IsSkillgroupReady();

	virtual void SetupJavascriptObjectTemplate() OVERRIDE;

private:
	v8::Local< v8::Object > GetSkillgroupDataJSO();
	bool m_bSkillgroupUpdated;

protected:
	UtlOwnedPtr<KeyValues> m_pKVSkillgroupUpdateData;
	PANORAMA_DELCARE_EXPOSED_KEYVALUE_MEMBER( m_pKVSkillgroupUpdateData )
};


class CCSGO_EndOfMatchVotingPanel : public panorama::CPanel2D
{
	DECLARE_PANEL2D( CCSGO_EndOfMatchVotingPanel, panorama::CPanel2D );

public:
	CCSGO_EndOfMatchVotingPanel( panorama::CPanel2D *pParent, const char *pchID );
	~CCSGO_EndOfMatchVotingPanel();

	virtual void SetupJavascriptObjectTemplate() OVERRIDE;

private:
	v8::Local< v8::Object > GetNextMatchVotingDataJSO();
	void GetNextMatchVotingDataJSO_Internal();

protected:
	UtlOwnedPtr<KeyValues> m_pKVVotingData;
	PANORAMA_DELCARE_EXPOSED_KEYVALUE_MEMBER( m_pKVVotingData )
};




// A label panel that keeps accurate game clocks.
//
//
//
class CCSGO_GameTimeLabel : public panorama::CLabel
{
	DECLARE_PANEL2D( CCSGO_GameTimeLabel, panorama::CLabel );

public:
	enum timerType_e
	{
		TIMER_DEFAULT,
		TIMER_ROUND_REMAINING,
		TIMER_ROUND_ELAPSED,
		TIMER_MAP_REMAINING,
		TIMER_MAP_ELAPSED,
	};

	CCSGO_GameTimeLabel( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CCSGO_GameTimeLabel();

	virtual void SetupJavascriptObjectTemplate() OVERRIDE;
	virtual bool BSetProperty( panorama::CPanoramaSymbol symName, const char *pchValue ) OVERRIDE;

	bool IsActive();
	void SetActive( bool bEnabled );

	timerType_e m_eType;

	int m_nTime;
	const char * m_szTimeFormat;
	const char * m_szNextMap;

private:
// 	bool EventReadyForDisplay( const panorama::CPanelPtr< panorama::IUIPanel > &panelPtr );
// 	bool EventUnreadyForDisplay( const panorama::CPanelPtr< panorama::IUIPanel > &panelPtr );

	bool m_bActiveTimer;
};
