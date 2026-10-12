//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//
//=============================================================================//

#ifndef CSGO_HUDCHAT_H_
#define CSGO_HUDCHAT_H_

#pragma once

#include "panorama/csgo_panorama.h"
#include "panorama/hud/csgo_hud.h"
#include "gameui_hudinterfaces.h"

//-----------------------------------------------------------------------------
// Purpose: In-game chat
//-----------------------------------------------------------------------------
class CCSGO_HudChat : public panorama::CPanel2D, public CPanoramaHudElement, public IHudChat
{
	DECLARE_PANEL2D( CCSGO_HudChat, panorama::CPanel2D );

public:

	CCSGO_HudChat( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CCSGO_HudChat();

	// CPanel2D overrides
	virtual void OnLayoutTraverse( float flFinalWidth, float flFinalHeight ) OVERRIDE;
	virtual bool OnKeyDown( const panorama::KeyData_t &code ) OVERRIDE;
	virtual bool OnMouseButtonUp( const panorama::MouseData_t &code ) OVERRIDE;

	// CHudElement overrides
	virtual void LevelInit( void ) OVERRIDE;
	virtual void LevelShutdown( void ) OVERRIDE;
	virtual bool ShouldDraw( void ) OVERRIDE;
	virtual void SetActive( bool bActive ) OVERRIDE;

	bool EventCancelled( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, panorama::EPanelEventSource_t eSource );
	bool EventTextEntrySubmit( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, const char *pchText );
	bool EventSendButtonActivated( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, panorama::EPanelEventSource_t eSource );
	bool EventGotvButtonActivated( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, panorama::EPanelEventSource_t eSource );
	bool EventStyleClassesChanged( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel );

	void AddStringToHistory( const char *pStr );

	// IHudChat
	virtual void StartMessageMode( int mode ) OVERRIDE;
	virtual bool ChatRaised() OVERRIDE;
	virtual void ClearHistory() OVERRIDE;
	virtual CHudElement* AsHudElement() OVERRIDE { return this; }

private:
	bool BRedirectingChatToPartyChannel() const;
	void ShowPanel( bool bShow, bool force );
	void UpdateHistory( void );
	void SendChatMessage();
	void ScrollToBottomImmediate();
	uint64		m_hDenyInputToGame;
	bool		m_bVisible;
	bool		m_bScrollToEnd;
	bool		m_bDisableInputToGame;
	int			m_iMode;
	char*		m_pHistoryString;
	int			m_nHistorySizeBytes;	// Offset of terminating \0 in m_pHistoryString
	bool		m_bInitialDisplay; // Set to true on displaying panel until first run of layouttraverse 
	panorama::CTextEntry* m_pTextEntry;
	panorama::CPanel2D* m_pSendButton;
	panorama::CLabel* m_pHistory;
	panorama::CPanel2D* m_pGotvPanel;
	panorama::CToggleButton* m_pGotvToggle;

};

#endif	// CSGO_HUDCHAT_H_