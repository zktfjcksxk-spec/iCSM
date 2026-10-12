//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Custom panorama panel for party chat
//
//=============================================================================//
#pragma once

#include "panorama/csgo_panorama.h"
#include "matchmaking/imatchframework.h"


DECLARE_PANORAMA_EVENT2( OnNewChatEntry, panorama::IUIPanel*, const char* );

class CCSGO_Chat : public panorama::CPanel2D, public IMatchEventsSink
{
	DECLARE_PANEL2D( CCSGO_Chat, panorama::CPanel2D );
public:
	CCSGO_Chat( panorama::CPanel2D *pParent, const char* pchID );
	virtual ~CCSGO_Chat();

	// IMatchEventsSink override
	virtual void OnEvent( KeyValues *pEvent ) OVERRIDE;

	panorama::CPanel2D* AddChatEntry( const char* szSnippetName = nullptr );
	panorama::CPanel2D* AddMatchmakingErrorMessage( const char* szErrorLocString, XUID errorID, const char* szClan, const char *szColor );
	panorama::CPanel2D* AddTournamentMatchmakingStatusMessage( const char* szStatus );
	void AddIngameChatMessageAboutPlayer( const char* szErrorLocString, XUID errorID, const char* szClan, const char *szColor );

	virtual void SetupJavascriptObjectTemplate() OVERRIDE;

	void SubmitChatText( void );

	panorama::CPanel2D* AddChatText( XUID ullSteamID, const char* szMessageUTF8, bool bTagMessageAsXuidSaidText );

protected:
	bool EventOnPlayerRemoved( const char* szXuid );
	bool EventOnPlayerJoined( const char* szXuid );

	panorama::CTextEntry *m_pChatInputTextEntry;
	panorama::CPanel2D *m_pChatLinesContainer;
};