//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: The text chat panel ("say all" or "say team")
//
//=============================================================================//

#include "cbase.h"
#include "csgo_hudchat.h"

#include "basepanel.h"
#include "iclientmode.h"
#include "clientmode_csnormal.h"
#include "IGameUIFuncs.h"
#include "vgui/ILocalize.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "inputsystem/iinputsystem.h"
//#include "uicomponents/uicomponent_matchstats.h"
//#include "cs_lobby_helpers.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

REGISTER_PANEL2D_FACTORY( CCSGO_HudChat, CSGOHudChat );

// Max number of characters in chat history
#define HISTORY_LENGTH_MAX		10 * 1024  
// Number of character to delete if chat buffer is full
#define HISTORY_LENGTH_PURGE	1 * 1024

extern ConVar cl_draw_only_deathnotices;
extern ConVar cl_drawhud;

extern bool IsTakingAFreezecamScreenshot();		// TODO Currently defined in sfhudfreezepanel.cpp

#ifdef INCLUDE_SCALEFORM
class SFHudChat;
#endif

CCSGO_HudChat::CCSGO_HudChat( panorama::CPanel2D *pParent, const char *pchID )
:
	panorama::CPanel2D( pParent, pchID ),
	CPanoramaHudElement( "CCSGO_HudChat", this ),
	m_hDenyInputToGame( 0 ),
	m_bVisible( false ),
	m_bScrollToEnd( false ),
	m_bInitialDisplay( false ),
	m_bDisableInputToGame( true ),
	m_iMode( MM_NONE )
{
	m_pHistoryString = new char[HISTORY_LENGTH_MAX];
	m_pHistoryString[0] = '\0';
	m_nHistorySizeBytes = 0;

	// Make sure chat panel has its own input hierarchy and therefore will not lose focus
	// when pushing another input context (peer panels such as scoreboard)
	SetTopOfInputContext( true );
	
	SetAcceptsInput( true );
	SetAcceptsFocus( true );

	SetVisible( false );

	SetHiddenBits( /* HIDEHUD_MISCSTATUS */ 0 );

	RequireLoadLayout( "file://{resources}/layout/hud/hudchat.xml" );

	m_pTextEntry = panorama::panel_cast<panorama::CTextEntry *>(RequireChildInLayoutFile( "ChatTextEntryBox" ));
	m_pTextEntry->SetContextMenuEnabled( false );
	m_pSendButton = RequireChildInLayoutFile( "ChatSendButton" );
	m_pSendButton->SetEnabled( false );
	m_pHistory = panorama::panel_cast<panorama::CLabel *>(RequireChildInLayoutFile( "ChatHistoryText" ));
	m_pGotvPanel = RequireChildInLayoutFile( "GotvPanel" );
	m_pGotvToggle = panorama::panel_cast<panorama::CToggleButton *>(RequireChildInLayoutFile( "GotvToggle" ));
	m_pGotvToggle->SetEnabled( true );

	RegisterEventHandler( panorama::Cancelled(), this, &CCSGO_HudChat::EventCancelled );
	RegisterEventHandlerOnPanel( panorama::Activated(), m_pSendButton->UIPanel(), this, &CCSGO_HudChat::EventSendButtonActivated );
	RegisterEventHandlerOnPanel( panorama::Activated(), m_pGotvToggle->UIPanel(), this, &CCSGO_HudChat::EventGotvButtonActivated );
	RegisterEventHandlerOnPanel( panorama::TextEntrySubmit(), m_pTextEntry->UIPanel(), this, &CCSGO_HudChat::EventTextEntrySubmit );
	RegisterEventHandlerOnPanel( panorama::StyleClassesChanged(), m_pTextEntry->UIPanel(), this, &CCSGO_HudChat::EventStyleClassesChanged );
}


CCSGO_HudChat::~CCSGO_HudChat()
{
	if ( m_hDenyInputToGame )
	{
		gameuifuncs->PanoramaReleaseDenyAllInputToGame( m_hDenyInputToGame );
		m_hDenyInputToGame = 0;
	}
	
	if( m_pHistoryString )
	{
		delete[] m_pHistoryString;
	}
}

void CCSGO_HudChat::LevelInit( void )
{
	m_pHistoryString[0] = '\0';
	m_nHistorySizeBytes = 0;
}

void CCSGO_HudChat::LevelShutdown( void )
{
	m_iMode = MM_NONE;
	SetVisible( false );
}

void CCSGO_HudChat::SetActive( bool bActive )
{
	ShowPanel( bActive, false );
	CHudElement::SetActive( bActive );
}

bool CCSGO_HudChat::ShouldDraw( void )
{
	if( IsTakingAFreezecamScreenshot() || (CSGameRules() && CSGameRules()->IsPlayingTraining()) )
		return false;

	bool result = cl_drawhud.GetBool();
	return result && (m_iMode != MM_NONE) && cl_draw_only_deathnotices.GetBool() == false && CHudElement::ShouldDraw();
}


DEVELOPMENT_ONLY_CONVAR( hud_chat_party_integration_mode, 1 ); // When enabled, in-game chat will intuitively send messages to party chat (set to 2 to ignore members check when sending)

bool CCSGO_HudChat::BRedirectingChatToPartyChannel() const
{
	if ( !hud_chat_party_integration_mode.GetBool() )
		return false;

	if ( engine->IsHLTV() )
	{
		bool bPlayingDemo = engine->IsPlayingDemo();
		CDemoPlaybackParameters_t const *pParams = bPlayingDemo ? engine->GetDemoPlaybackParameters() : NULL;
		bool bLiveBroadcast = bPlayingDemo && pParams && pParams->m_bPlayingLiveRemoteBroadcast;

		if ( bLiveBroadcast || !bPlayingDemo )
		{
			return ( m_iMode > 0 ); // LIVE GOTV+ match: "team > party", "all" still means to chat with other viewers
		}
		else
		{
			return true;	// playing demo or Overwatch, everything goes to "party chat"
		}
	}
	else
	{
		// Check if game mode is survival, then there's no concept of team > team chat goes to party
		if ( CSGameRules() && CSGameRules()->IsPlayingFreeForAllGametype() )
		{
			return ( m_iMode > 0 ); // "all" means to dead people, "team" means to party
		}

		int numHumansConnected = 0;
		for ( int i = 1; i <= MAX_PLAYERS; i++ )
		{
			player_info_t sPlayerInfo;
			if ( engine->GetPlayerInfo( i, &sPlayerInfo ) )
			{
				if ( sPlayerInfo.fakeplayer )
					continue;
				++numHumansConnected;
				if ( numHumansConnected > 1 )
					break; // just care about "multiplayer" concept here
			}
		}
		
		// If there are no other humans on the game server then send messages to "party chat"
		if ( numHumansConnected <= 1 ) return true;

		// Otherwise use regular game rules
		return false;
	}
}

void CCSGO_HudChat::SendChatMessage()
{
#if defined( OSX ) && defined( __aarch64__ ) && defined( NO_STEAM )
	// Local CBaseHudChat::Send uses the original say/say_team commands.
	// The partner-depot Panorama sender is absent; preserve that same protocol.
	char message[1024];
	V_strncpy( message, m_pTextEntry->PchGetText(), sizeof( message ) );
	// Keep pasted text inside one quoted command argument.
	for ( char *p = message; *p; ++p )
	{
		if ( *p == '\"' ) *p = '\'';
		else if ( *p == '\r' || *p == '\n' ) *p = ' ';
	}
	if ( message[0] )
	{
		char command[1100];
		V_snprintf( command, sizeof( command ), "%s \"%s\"", m_iMode == MM_SAY_TEAM ? "say_team" : "say", message );
		engine->ClientCmd_Unrestricted( command );
	}
#else
	/* removed for partner depot */
#endif
	m_pTextEntry->SetText( "" );

	m_iMode = MM_NONE;
}

bool CCSGO_HudChat::EventTextEntrySubmit( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, const char *pchText )
{
	SendChatMessage();
	return true;
}

bool CCSGO_HudChat::EventSendButtonActivated( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, panorama::EPanelEventSource_t eSource )
{
	SendChatMessage();
	return true;
}

bool CCSGO_HudChat::EventGotvButtonActivated( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, panorama::EPanelEventSource_t eSource )
{
	// Toggle tv_nochat convar
	static ConVarRef tv_nochat( "tv_nochat" );
	tv_nochat.SetValue( !tv_nochat.GetBool() );
	return false;
}

bool CCSGO_HudChat::EventCancelled( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, panorama::EPanelEventSource_t eSource )
{
	m_pTextEntry->SetText( "" );

	m_iMode = MM_NONE;
	return true;
}

bool CCSGO_HudChat::EventStyleClassesChanged( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel )
{
	static panorama::CPanoramaSymbol k_symHasInput( "HasInput" );

	if( ( pPanel.Get() != m_pTextEntry->UIPanel() ) || ( !m_bVisible ) )
		return false;

	if( m_pTextEntry->BHasClass( k_symHasInput ) )
	{
		m_pSendButton->SetEnabled( true );
	}
	else
	{
		m_pSendButton->SetEnabled( false );
	}
	m_pSendButton->MarkStylesDirty( true );
	return false;
}

void CCSGO_HudChat::StartMessageMode( int mode )
{
	if( m_iMode == MM_NONE )
	{
		if( !GetHud().HudDisabled() )
		{
			m_iMode = mode;

			IMatchSession *pMatchSession = g_pMatchFramework->GetMatchSession();
			if ( BRedirectingChatToPartyChannel() && // TODO: possibly replace the party detection with just "LIVE" check
				( ( hud_chat_party_integration_mode.GetInt() == 2 ) || ( pMatchSession && pMatchSession->GetSessionSettings()->GetInt( "members/numPlayers" ) > 1 ) ) )
			{
				if ( pMatchSession && pMatchSession->GetSessionSettings()->GetInt( "members/numPlayers" ) > 1 )
					m_pTextEntry->SetPlaceholderText( "#SFUI_Settings_Chat_SayParty" );
				else
					m_pTextEntry->SetPlaceholderText( "#SFUI_Settings_Chat_SayPartyEmpty" );
			}
			else if( m_iMode == MM_SAY_TEAM )
			{
				m_pTextEntry->SetPlaceholderText( "#SFUI_Settings_Chat_SayTeam" );
			}
			else
			{
				m_pTextEntry->SetPlaceholderText( "#SFUI_Settings_Chat_Say" );
			}
		}
	}
}

bool CCSGO_HudChat::ChatRaised( void )
{
	return m_iMode != MM_NONE;
}

void CCSGO_HudChat::ShowPanel( bool bShow, bool force )
{
	static panorama::CPanoramaSymbol k_symGotv( "Gotv" );

	if( (bShow != m_bVisible) || force )
	{
		m_bVisible = bShow;

		if( m_bVisible )
		{
			g_pInputSystem->SetIMEAllowed( true );

			if( m_bDisableInputToGame )
			{
				DevAssertMsg( !m_hDenyInputToGame, "HudChat - Make sure to call PanoramaReleaseDenyAllInputToGame before calling PanoramaAddDenyAllInputToGame again\n" );
				m_hDenyInputToGame = gameuifuncs->PanoramaAddDenyAllInputToGame( this->UIPanel(), "HudChat" );
			}

			const char* pUiType = "";
			bool bGotv= (V_strcmp( pUiType, "GOTV" ) == 0);
			m_pGotvPanel->SetHasClass( k_symGotv, bGotv );
			if( bGotv )
			{
				static ConVarRef tv_nochat( "tv_nochat" );
				m_pGotvToggle->SetSelected( !tv_nochat.GetBool() );
			}

			UpdateHistory();

			m_pTextEntry->SetText( "" );
			m_pSendButton->SetEnabled( false );

			SetVisible( true );

			// Chat panel has its own input context (cf SetTopOfInputContext( true ) in constructor)
			// Therefore calling SetFocus will switch input context. Make sure to remove it from the stack
			// when hiding the chat panel
			SetFocus();

			m_bInitialDisplay = true;
		}
		else
		{
			g_pInputSystem->SetIMEAllowed( false );

			SetVisible( false );

			// Removing chat panel from the input context stack
			GetParentWindow()->UIWindowInput()->RemoveInputContext( this->UIPanel() );

			if( m_bDisableInputToGame )
			{
				if ( m_hDenyInputToGame )
				{
					gameuifuncs->PanoramaReleaseDenyAllInputToGame( m_hDenyInputToGame );
					m_hDenyInputToGame = 0;
				}
			}
			m_bInitialDisplay = false;
		}
	}
}

bool CCSGO_HudChat::OnKeyDown( const panorama::KeyData_t &code )
{
	static panorama::CPanoramaSymbol k_symHasInput( "HasInput" );
	switch( code.m_KeyCode )
	{
	case KEY_UP:
		if( m_pHistory->BCanScrollUp() )
		{
			m_pHistory->ScrollVertically( -100.0f );
		}
		return true;
	case KEY_DOWN:
		if( m_pHistory->BCanScrollDown() )
		{
			m_pHistory->ScrollVertically( 100.0f );
		}
		return true;
	case KEY_HOME:
		if( m_pHistory->BCanScrollUp() )
		{
			m_pHistory->ScrollToTop();
		}
		return true;
	case KEY_END:
		if( m_pHistory->BCanScrollDown() )
		{
			m_pHistory->ScrollToBottom();
		}
		return true;
	case KEY_PAGEUP:
		if( m_pHistory->BCanScrollUp() )
		{
			m_pHistory->ScrollVertically( m_pHistory->GetActualLayoutHeight() * -0.8f );
		}
		return true;
	case KEY_PAGEDOWN:
		if( m_pHistory->BCanScrollDown() )
		{
			m_pHistory->ScrollVertically( m_pHistory->GetActualLayoutHeight() * 0.8f );
		}
		return true;
	case KEY_ENTER:
		if( m_pTextEntry->BHasClass( k_symHasInput ) )
		{
			SendChatMessage();
		}
		else
		{
			m_iMode = MM_NONE;
		}
		return true;
	default:
		break;
	}
	return BaseClass::OnKeyDown( code );
}

bool CCSGO_HudChat::OnMouseButtonUp( const panorama::MouseData_t &code )
{
	if( code.m_MouseCode == panorama::MouseCode::MOUSE_RIGHT )
	{
		// Cancel chat
		m_iMode = MM_NONE;
		return true;
	}
	return BaseClass::OnMouseButtonUp( code );
}

void CCSGO_HudChat::AddStringToHistory( const char *pStr )
{
	static const char* kszNewline = "<br>";
	static const int kNewlineLen = V_strlen( kszNewline );

	int nStrLen = V_strlen( pStr );
	int nRequiredBytes = nStrLen + kNewlineLen + 1; // + trailing \0

	// Ignore messages that are too long to fit in the buffer
	if ( nRequiredBytes > HISTORY_LENGTH_MAX )
		return;

	// If we run out of space, eat some.
	if ( ( m_nHistorySizeBytes + nRequiredBytes ) > HISTORY_LENGTH_MAX )
	{
		// Buffer is full. Delete lines at the start of the buffer
		int nPurgeSize = MAX( nRequiredBytes, HISTORY_LENGTH_PURGE );
		if( m_nHistorySizeBytes > nPurgeSize )
		{
			const char *nextreturn = V_strstr( &m_pHistoryString[nPurgeSize], kszNewline );
			if( nextreturn != NULL )
			{
				// Skip the final newline as well
				nextreturn += kNewlineLen;

				// Figure out new text size
				int nBytesPurged = nextreturn - m_pHistoryString;
				int nBytesCopied = m_nHistorySizeBytes - nBytesPurged;
				Assert( nBytesCopied >= 0 && nBytesCopied + 1 < HISTORY_LENGTH_MAX );

				// Copy text
				memmove( m_pHistoryString, nextreturn, nBytesCopied + 1 ); // include trailing '\0'
				m_nHistorySizeBytes = nBytesCopied;
			}
			else
			{
				m_pHistoryString[0] = '\0';
				m_nHistorySizeBytes = 0;
			}
		}
		else
		{
			m_pHistoryString[0] = '\0';
			m_nHistorySizeBytes = 0;
		}
	}

	Assert( m_nHistorySizeBytes + nRequiredBytes <= HISTORY_LENGTH_MAX );

	// Add new line
	if(m_nHistorySizeBytes != 0)
	{
		memcpy( &m_pHistoryString[m_nHistorySizeBytes], kszNewline, kNewlineLen );
		m_nHistorySizeBytes += kNewlineLen;
	}

	// Add string
	memcpy( &m_pHistoryString[m_nHistorySizeBytes], pStr, nStrLen );
	m_nHistorySizeBytes += nStrLen;

	// Terminate
	m_pHistoryString[m_nHistorySizeBytes] = '\0';

	Assert( m_nHistorySizeBytes >= 0 && m_nHistorySizeBytes + 1 <= HISTORY_LENGTH_MAX );

	// Update if the chat panel is visible
	if( m_bVisible )
		UpdateHistory();
}

void CCSGO_HudChat::ClearHistory()
{
	m_pHistoryString[0] = '\0';
	m_nHistorySizeBytes = 0;
	UpdateHistory();
}

void CCSGO_HudChat::ScrollToBottomImmediate()
{
	if( m_pHistory )
	{
		panorama::IUIScrollBar* pScrollBar = m_pHistory->UIPanel()->GetVerticalScrollBar();
		if( pScrollBar )
		{
			pScrollBar->SetScrollWindowPosition( FLT_MAX, true );
		}
	}
}

void CCSGO_HudChat::OnLayoutTraverse( float flFinalWidth, float flFinalHeight )
{
	BaseClass::OnLayoutTraverse( flFinalWidth, flFinalHeight );
	if( m_bScrollToEnd )
	{
		if( m_bInitialDisplay )
		{
			ScrollToBottomImmediate();
		}
		else
		{
			m_pHistory->ScrollToBottom();
		}
		m_bScrollToEnd = false;
	}
	m_bInitialDisplay = false;
}

// Update history panel
void CCSGO_HudChat::UpdateHistory()
{
	// If user has scrolled up, maintain that scroll position, 
	// otherwise automatically scroll down to latest message
	m_bScrollToEnd = !m_pHistory->BCanScrollDown();

	// TODO: Should be able to use AppendText() usually instead of always assigning the whole thing.
	m_pHistory->SetText( m_pHistoryString, panorama::CLabel::k_ETextTypeHTML );
}

