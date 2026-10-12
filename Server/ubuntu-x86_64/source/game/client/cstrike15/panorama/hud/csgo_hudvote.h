//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Vote panel: displays current vote issue
// 
//=============================================================================//

#pragma once

#include "panorama/csgo_panorama.h"
#include "panorama/hud/csgo_hud.h"

DECLARE_PANEL_EVENT0( PanoramaCastVoteYes );
DECLARE_PANEL_EVENT0( PanoramaCastVoteNo );

DECLARE_PANORAMA_EVENT0( ShowVoteContextMenu );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CCSGO_HudVote : public panorama::CPanel2D, public CPanoramaHudElement, public panorama::CDefaultInputCapture
{
	DECLARE_PANEL2D( CCSGO_HudVote, panorama::CPanel2D );

public:

	enum EVoteEvent
	{
		k_eVoteStart,			// New vote called
		k_eVotePassed,			// Vote finished
		k_eVoteFailed,
		k_eVoteCallFailed,		// User tried to call a vote that server rejected
		k_eVoteEnd,
	};

	explicit CCSGO_HudVote( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CCSGO_HudVote();

	// All hud elements are game event listeners
	void FireGameEvent( IGameEvent* evt ) OVERRIDE;

	// Handlers for vote related User Messages from server
	bool MsgFunc_CallVoteFailed( const CCSUsrMsg_CallVoteFailed &msg );
	bool MsgFunc_VoteStart( const CCSUsrMsg_VoteStart &msg );
	bool MsgFunc_VotePass( const CCSUsrMsg_VotePass &msg );
	bool MsgFunc_VoteFailed( const CCSUsrMsg_VoteFailed &msg );
	bool MsgFunc_VoteSetup( const CCSUsrMsg_VoteSetup &msg );

	// panel2d overrides
	virtual void SetupJavascriptObjectTemplate() OVERRIDE;

	// CDefaultInputCapture
	virtual bool OnCapturedKeyDown( panorama::IUIPanel *pPanel, const panorama::KeyData_t &code ) OVERRIDE;

protected:
	void BuildDebugPanel( panorama::CPanel2D* pPanel );
	bool Event_DbgTestHudVote( const char* szTestString );

	// Common code for all vote related user messages
	void UpdateShared( EVoteEvent eventType, const char* szDisplayString = nullptr, int nVoteType = VOTEISSUE_UNDEFINED, const char* szParam = nullptr );

	bool EventAnimationEnd( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, panorama::CPanoramaSymbol symAnimation );
	bool EventCastVoteYes( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel );
	bool EventCastVoteNo( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel );
	CUserMessageBinder m_UMCMsgCallVoteFailed;
	CUserMessageBinder m_UMCMsgVoteStart;
	CUserMessageBinder m_UMCMsgVotePass;
	CUserMessageBinder m_UMCMsgVoteFailed;
	CUserMessageBinder m_UMCMsgVoteSetup;

	panorama::CPanelPtr<panorama::CImagePanel> m_pVoteHeaderIcon;
	panorama::CPanelPtr<panorama::CLabel> m_pVoteDescription;
};