//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Custom panorama panel for party chat
//
//=============================================================================//

#include "cbase.h"
#include "csgo_chat.h"
#include "panorama/uijsregistration.h"
//#include "cs_lobby_helpers.h"
#include "csgo_avatarimage.h"
#include "bannedwords.h"
#include "clientsteamcontext.h"
#include "c_cs_playerresource.h"
#include "gameui_interface.h"
#include "matchmaking/mm_helpers.h"
// #include "uicomponents/uicomponent_friendslist.h"
// #include "uicomponents/uicomponent_gametypes.h"
// #include "uicomponents/uicomponent_lobby.h"

#ifdef PANORAMA_ENABLE
#include "panorama/hud/csgo_hudvoicestatus.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

REGISTER_PANEL2D_FACTORY( CCSGO_Chat, CSGOChat );

DEFINE_PANORAMA_EVENT_DOC( OnNewChatEntry, "panel, snippet name", "Dispatched when a new chat entry is added to the main menu chat body." );

using namespace panorama;

template < typename T >
T* ToPanel( CPanel2D* pPanel )
{
	if ( pPanel && ( pPanel->GetPanelType() == T::GetPanelSymbol() ) )
		return ( T* )pPanel;
	else
		return nullptr;
}

#if DEVELOPMENT_ONLY
CON_COMMAND_F( dev_test_chat_messages, "", FCVAR_DEVELOPMENTONLY )
{
	if ( IMatchSession *pIMatchSession = g_pMatchFramework->GetMatchSession() )
	{
		{
			KeyValues *pRequest = new KeyValues( "Game::ChatInviteMessage" );
			KeyValues::AutoDelete autodelete( pRequest );

			pRequest->SetString( "run", "all" );
			pRequest->SetUint64( "xuid", steamapicontext->SteamUser()->GetSteamID().ConvertToUint64() );
			pRequest->SetUint64( "friend", 148618791998203739 );
			pRequest->SetString( "friendName", Helper_GetFriendPersonaNameSanitized( CSteamID( 148618791998203739llu ) ) );

			pIMatchSession->Command( pRequest );
		}

		{
			KeyValues *pRequest = new KeyValues( "Game::ChatReportError" );
			KeyValues::AutoDelete autodelete( pRequest );

			pRequest->SetString( "run", "local" );
			pRequest->SetUint64( "xuid", 148618791998203739 );
			pRequest->SetString( "error", "#SFUI_QMM_ERROR_X_VacBanned" );

			pIMatchSession->Command( pRequest );
		}

		{
			KeyValues *pRequest = new KeyValues( "Game::ChatReportGreen" );
			KeyValues::AutoDelete autodelete( pRequest );

			pRequest->SetString( "run", "local" );
			pRequest->SetUint64( "xuid", 148618791998203739 );
			pRequest->SetString( "green", "#SFUI_QMM_ERROR_X_PenaltySecondsGreen" );

			pIMatchSession->Command( pRequest );
		}

		IPlayerLocal *pPlayer = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()
			->GetLocalPlayer( XBX_GetActiveUserId() );
		if ( pPlayer )
		{
				KeyValues *pRequest = new KeyValues( "Game::ChatReportMatchmakingStatus" );
				KeyValues::AutoDelete autodelete( pRequest );

				XUID xuid = pPlayer->GetXUID();
				char const *szName = pPlayer->GetName();

				pRequest->SetString( "run", "all" );
				pRequest->SetUint64( "xuid", xuid );
				pRequest->SetString( "name", szName );
				pRequest->SetString( "status", CFmtStr( "<font color=\"#40FF40\">* Matchmaking '%s' -vs- '%s' in '%s' '%s'.</font>",
					"ValveTeamA", "ValveSquadOne", "Chat Message Formatting Tournament" , "Test Stage" ) );

				pIMatchSession->Command( pRequest );
		}

		{
				KeyValues *pRequest = new KeyValues( "Game::ChatReportMatchmakingStatusW" );
				KeyValues::AutoDelete autodelete( pRequest );

				wchar_t wszFullInfo[ 2048 ] = {};
				pRequest->SetString( "run", "local" );
				extern wchar_t const * Helper_ResolveLocalizationStringWithFormattingParameters( wchar_t *wch, size_t wchsize, char const *pchToken );
				pRequest->SetWString( "status", Helper_ResolveLocalizationStringWithFormattingParameters( wszFullInfo, sizeof( wszFullInfo ),
					CFmtStr( "#CSGO_Tournament_Event_AnnouncementLobby{event=#CSGO_Watch_Cat_Tournament_%d}{stage=#CSGO_Tournament_Event_Stage_%d}{team0=#CSGO_TeamID_%d}{team1=#CSGO_TeamID_%d}{map=#SFUI_Map_%s}",
					1, 0, 2, 4, "de_overpass" ) ) );

				pIMatchSession->Command( pRequest );
		}

		/*
		{
			KeyValues *pRequest = new KeyValues( "OnPlayerRemoved" );
			KeyValues::AutoDelete autodelete( pRequest );

			pRequest->SetString( "run", "local" );
			pRequest->SetUint64( "xuid", 148618791998203739 );

			pIMatchSession->Command( pRequest );
		}

		{
			KeyValues *pRequest = new KeyValues( "OnPlayerUpdated" );
			KeyValues::AutoDelete autodelete( pRequest );

			pRequest->SetString( "run", "local" );
			pRequest->SetString( "state", "joined" );
			pRequest->SetUint64( "xuid", 148618791998203739 );

			pIMatchSession->Command( pRequest );
		}
		*/

		{
			KeyValues *pRequest = new KeyValues( "Game::Chat" );
			KeyValues::AutoDelete autodelete( pRequest );

			pRequest->SetString( "run", "local" );
			pRequest->SetUint64( "xuid", 148618791998203739 );
			pRequest->SetUint64( "_remote_xuidsrc", 148618791998203739 );
			pRequest->SetString( "chat", "Hi my name is ido and i'm for sure real and chatting at you right now this is a long chat line." );

			pIMatchSession->Command( pRequest );
		}
		{
			KeyValues *pRequest = new KeyValues( "Game::Chat" );
			KeyValues::AutoDelete autodelete( pRequest );

			pRequest->SetString( "run", "local" );
			pRequest->SetUint64( "xuid", 148618791998203739 );
			pRequest->SetUint64( "_remote_xuidsrc", 148618791998203739 );
			pRequest->SetString( "chat", "And this is a second chat line being received after the first because the avatar might only show once etc etc." );

			pIMatchSession->Command( pRequest );
		}
	}
}
#endif

CCSGO_Chat::CCSGO_Chat(CPanel2D *pParent, const char *pchID) : CPanel2D(pParent, pchID)
{
	BLoadLayout( "file://{resources}/layout/chat.xml" );
	SetAcceptsInput( true );
	SetAcceptsFocus( true );

	m_pChatInputTextEntry = ToPanel< CTextEntry >( FindChildInLayoutFile( "ChatInput" ) );
	m_pChatLinesContainer = FindChildInLayoutFile( "ChatLinesContainer" );

	RegisterForUnhandledEvent( PanoramaComponent_Lobby_PlayerRemoved(), this, &CCSGO_Chat::EventOnPlayerRemoved );
	RegisterForUnhandledEvent( PanoramaComponent_Lobby_PlayerJoined(), this, &CCSGO_Chat::EventOnPlayerJoined );

	g_pMatchFramework->GetEventsSubscription()->Subscribe( this );
}

CCSGO_Chat::~CCSGO_Chat()
{
	if ( g_pMatchFramework )
		g_pMatchFramework->GetEventsSubscription()->Unsubscribe( this );
}

// Helper to iterate on HUD chat party color in runtime
#define HUD_CHAT_PARTY_COLOR_DEFAULT "#A745F7"
#define HUD_CHAT_PARTY_COLOR_RED "#FF4040"
#define HUD_CHAT_PARTY_COLOR_YELLOW "#FFFF40"
#define HUD_CHAT_PARTY_COLOR_GREEN "#40FF40"
#if DEVELOPMENT_ONLY
static ConVar hud_chat_party_color_debug( "hud_chat_party_color_debug", HUD_CHAT_PARTY_COLOR_DEFAULT, FCVAR_DEVELOPMENTONLY, "Color of party messages in game chat" );
static inline char const * HelperGetChatPartyColor() { return hud_chat_party_color_debug.GetString(); }
#else
static inline char const * HelperGetChatPartyColor() { return HUD_CHAT_PARTY_COLOR_DEFAULT; }
#endif

static inline wchar_t const * HelperGetChatPartyPrefix()
{
	IMatchSession *pMatchSession = g_pMatchFramework->GetMatchSession();
	if ( pMatchSession && pMatchSession->GetSessionSettings()->GetInt( "members/numPlayers" ) > 1 )
	{
		return g_pLocalize->FindSafe( "#SFUI_Settings_Chat_NotePartyChat" );
	}
	else
	{
		return L">> ";
	}
}

// TODO: figure out the party chat in warmup later, for now party chat is only in survival
DEVELOPMENT_ONLY_CONVAR( hud_chat_print_party_enabled, 1 );

void CCSGO_Chat::OnEvent( KeyValues *pEvent )
{
	char const *szEvent = pEvent->GetName();

	IMatchSession *pMatchSession = g_pMatchFramework->GetMatchSession();
	if ( !pMatchSession )
		return;

	if ( V_stricmp( pMatchSession->GetSessionSettings()->GetString( "system/network" ), "LIVE" )
		&& !Q_stricmp( "OnMatchSessionUpdate", szEvent ) )
	{	// This is not a full live lobby session, don't let any session update events through
		return;
	}


	if ( !Q_stricmp( "Command::Game::Chat", szEvent ) )
	{
		XUID xuid = pEvent->GetUint64( "xuid" );
		if ( xuid != pEvent->GetUint64( "_remote_xuidsrc" ) )	// chat always comes over network and must match the src
			return;

		char const *szMessage = pEvent->GetString( "chat" );
		char bufMessageCensored[ 1024 ]; // TODO: Stackalloc to input string size, or pick a max chat length make everything use it 
		V_strcpy_safe( bufMessageCensored, szMessage );
		g_BannedWords.CensorBannedWordsInplace( bufMessageCensored );

		// Chat from party members
		AddChatText( xuid, bufMessageCensored, true );

		//
		// Format for display in-game
		//
#ifdef PANORAMA_ENABLE

		if ( GameUI().IsPanoramaEnabled() )
		{
			GameUI().PlayUISoundScript( "UI.Lobby.Chat", m_pChatLinesContainer );
		}

		if ( GameUI().IsPanoramaEnabled() && hud_chat_print_party_enabled.GetBool() )
		{
			CUtlBuffer bufUserNameSafe;
			Helper_ProcessPersonaNameForChat( xuid, bufUserNameSafe );

			CUtlBuffer bufSafeStringUCS2;
			Helper_ProcessMessageForChat( szMessage, bufSafeStringUCS2 );

			// Format them all together
			CUtlBuffer bufSafeJointFormatUCS2;
			int numJointLen = bufSafeStringUCS2.Size() + bufUserNameSafe.Size() + 512;
			bufSafeJointFormatUCS2.EnsureCapacity( numJointLen );
			V_snwprintf( ( wchar_t * ) bufSafeJointFormatUCS2.Base(), numJointLen / sizeof( wchar_t ),
				L"<font color=\"" PRI_S_FOR_WS "\">" PRI_WS_FOR_WS L" " PRI_WS_FOR_WS L"</font>: "
				PRI_WS_FOR_WS,
				HelperGetChatPartyColor(), HelperGetChatPartyPrefix(),
				( wchar_t* ) bufUserNameSafe.Base(), ( wchar_t * ) bufSafeStringUCS2.Base() );

			if ( CHudElement *pElement = GetHud().FindElement( "CCSGO_HudVoiceStatus" ) )
			{
				char chNotice[ 2048 ];
				V_UnicodeToUTF8( ( wchar_t * ) bufSafeJointFormatUCS2.Base(), chNotice, sizeof( chNotice ) );
				chNotice[ sizeof( chNotice ) - 1 ] = 0;
				( ( CCSGO_HudVoiceStatus * ) pElement )->PushNotice( chNotice, -1, true );
			}
		}
#endif
	}
	else if ( !Q_stricmp( "Command::Game::ChatInviteMessage", szEvent ) )
	{
		XUID xuidEvent = pEvent->GetUint64( "xuid" ); 
		XUID xuidRemoteSrcValidated = pEvent->GetUint64( "_remote_xuidsrc" );
		if ( !xuidRemoteSrcValidated || ( xuidEvent == xuidRemoteSrcValidated ) )
		{
			GameUI().PlayUISoundScript( "PanoramaUI.Lobby.Chat", m_pChatLinesContainer );

			panorama::CPanel2D *pInviteNotification = AddChatEntry( "PlayerInvited" );

			static const panorama::CPanoramaSymbol k_symInviteXuid( "InviteXuid" );
			pInviteNotification->SetAttribute( k_symInviteXuid, xuidEvent );

			CSteamID steamIdInvitedFriend( pEvent->GetUint64( "friend" ) );

			char bufUserNameCensored[ k_cchPersonaNameMax ];
			char const *szSpeculativeInvitedUserName = pEvent->GetString( "friendName" );	// This is the name supplied by inviter (which could be an alias for their friend)
			char const *szMyOwnSteamName = Helper_GetFriendPersonaNameSanitized( steamIdInvitedFriend );
			if ( szMyOwnSteamName && *szMyOwnSteamName && V_strcmp( "[unknown]", szMyOwnSteamName ) )	// Try to resolve the name via our own friends list if we can
				szSpeculativeInvitedUserName = szMyOwnSteamName;							// use our name if we can resolve (or have our own alias for that friend)

			V_strcpy_safe( bufUserNameCensored, szSpeculativeInvitedUserName );
			g_BannedWords.CensorBannedWordsInplace( bufUserNameCensored );

			// GetPlayerNameInternal handles censoring, no need to do that for the inviting player name
			const char* szInvitedBy = CUiComponent_FriendsList::GetInstance()->GetPlayerNameInternal( xuidEvent );
			pInviteNotification->SetDialogVariable( "s1", szInvitedBy );
			pInviteNotification->SetDialogVariable( "s2", szSpeculativeInvitedUserName );

			//
			// Format for display in-game
			//
#ifdef PANORAMA_ENABLE
			if ( GameUI().IsPanoramaEnabled() && hud_chat_print_party_enabled.GetBool() )
			{
				CUtlBuffer bufUserNameSafe;
				Helper_ProcessPersonaNameForChat( xuidEvent, bufUserNameSafe );

				CUtlBuffer bufInvitedUserNameSafe;
				Helper_ProcessPersonaNameForChat( steamIdInvitedFriend.ConvertToUint64(), bufInvitedUserNameSafe, szSpeculativeInvitedUserName );

				// Format them all together
				CUtlBuffer bufSafeJointFormatUCS2;
				int numJointLen = bufUserNameSafe.Size() + bufInvitedUserNameSafe.Size() + 512;
				bufSafeJointFormatUCS2.EnsureCapacity( numJointLen );
				V_snwprintf( ( wchar_t * ) bufSafeJointFormatUCS2.Base(), numJointLen / sizeof( wchar_t ),
					L"<font color=\"" PRI_S_FOR_WS "\">" PRI_WS_FOR_WS L" " PRI_WS_FOR_WS L"</font> "
					PRI_WS_FOR_WS
					L" <font color=\"" PRI_S_FOR_WS "\">" PRI_WS_FOR_WS L"</font>",
					HelperGetChatPartyColor(), HelperGetChatPartyPrefix(),
					( wchar_t* ) bufUserNameSafe.Base(),
					g_pLocalize->FindSafe( "#SFUI_Settings_Chat_NotePartyInvited" ),
					HelperGetChatPartyColor(),
					( wchar_t * ) bufInvitedUserNameSafe.Base() );

				if ( CHudElement *pElement = GetHud().FindElement( "CCSGO_HudVoiceStatus" ) )
				{
					char chNotice[ 2048 ];
					V_UnicodeToUTF8( ( wchar_t * ) bufSafeJointFormatUCS2.Base(), chNotice, sizeof( chNotice ) );
					chNotice[ sizeof( chNotice ) - 1 ] = 0;
					( ( CCSGO_HudVoiceStatus * ) pElement )->PushNotice( chNotice, -1, true );
				}
			}
#endif
		}
	}
	else if ( !Q_stricmp( "Command::Game::ChatReportError", szEvent ) )
	{
		XUID xuidEvent = pEvent->GetUint64( "xuid" );
		XUID xuidRemoteSrcValidated = pEvent->GetUint64( "_remote_xuidsrc" );
		if ( !xuidRemoteSrcValidated || ( xuidEvent == xuidRemoteSrcValidated ) || pEvent->GetBool( "_remote_host" ) )
		{
			const char* szErrorLocString = pEvent->GetString( "error" );
			const char *szClan = pEvent->GetString( "clan", NULL );
			AddMatchmakingErrorMessage( szErrorLocString, xuidEvent, szClan, HUD_CHAT_PARTY_COLOR_RED ); // handles adding in-game chat message too
			AddIngameChatMessageAboutPlayer( szErrorLocString, xuidEvent, szClan, HUD_CHAT_PARTY_COLOR_RED );

			GameUI().PlayUISoundScript( "PanoramaUI.Lobby.Error", m_pChatLinesContainer );
		}
	}
	else if ( !Q_stricmp( "Command::Game::ChatReportGreen", szEvent ) )
	{
		if ( pEvent->GetBool( "_remote_host" ) || !pEvent->GetUint64( "_remote_xuidsrc" ) ) // only comes from remote host
		{
			XUID xuid = pEvent->GetUint64( "xuid" );
			const char* szErrorLocString = pEvent->GetString( "green" );
			CPanel2D* pPanel = AddMatchmakingErrorMessage( szErrorLocString, xuid, NULL, HUD_CHAT_PARTY_COLOR_GREEN ); // handles adding in-game chat message too
			pPanel->AddClass( "error-message--green" );
			AddIngameChatMessageAboutPlayer( szErrorLocString, xuid, NULL, HUD_CHAT_PARTY_COLOR_GREEN );
		}
	}
	else if ( !Q_stricmp( "Command::Game::ChatReportYellow", szEvent ) )
	{
		if ( pEvent->GetBool( "_remote_host" ) || !pEvent->GetUint64( "_remote_xuidsrc" ) ) // only comes from remote host
		{
			XUID xuid = pEvent->GetUint64( "xuid" );
			const char* szErrorLocString = pEvent->GetString( "yellow" );
			CPanel2D* pPanel = AddMatchmakingErrorMessage( szErrorLocString, xuid, NULL, HUD_CHAT_PARTY_COLOR_YELLOW ); // handles adding in-game chat message too
			pPanel->AddClass( "error-message--yellow" );
			AddIngameChatMessageAboutPlayer( szErrorLocString, xuid, NULL, HUD_CHAT_PARTY_COLOR_YELLOW );
		}
	}
	else if ( !Q_stricmp( "Command::Game::ChatReportAdvertising", szEvent ) )
	{
		if ( pEvent->GetBool( "_remote_host" ) || !pEvent->GetUint64( "_remote_xuidsrc" ) ) // only comes from remote host
		{
			IMatchSession *pSession = g_pMatchFramework->GetMatchSession();
			if ( pSession )
			{
				bool bAdvertisingToSteamGroup = !!pSession->GetSessionSettings()->GetUint64( "game/clanid" );
				bool bAdvertisingToNearby = pSession->GetSessionSettings()->GetBool( "game/nby" );
				if ( V_stricmp( pSession->GetSessionSettings()->GetString( "system/access" ), "public" ) )
				{
					bAdvertisingToNearby = bAdvertisingToSteamGroup = false;
				}

				CFmtStr fmtLocAdvertisingString( "party_message_advertising_%s%s%s", bAdvertisingToNearby ? "n" : "", bAdvertisingToSteamGroup ? "g" : "",
					( !bAdvertisingToNearby && !bAdvertisingToSteamGroup && !V_stricmp( "expired", pEvent->GetString( "reason" ) ) ) ? "expired" : "" );
				bool bGreen = bAdvertisingToSteamGroup || bAdvertisingToNearby;

				XUID xuid = pEvent->GetUint64( "xuid" );
				CPanel2D* pPanel = AddMatchmakingErrorMessage( fmtLocAdvertisingString.Get(), xuid, NULL, bGreen ? HUD_CHAT_PARTY_COLOR_GREEN : HUD_CHAT_PARTY_COLOR_YELLOW ); // handles adding in-game chat message too
				pPanel->AddClass( bGreen ? "error-message--green" : "error-message--yellow" );
				AddIngameChatMessageAboutPlayer( fmtLocAdvertisingString.Get(), xuid, NULL, bGreen ? HUD_CHAT_PARTY_COLOR_GREEN : HUD_CHAT_PARTY_COLOR_YELLOW );
			}
		}
	}
	else if ( !Q_stricmp( "Command::Game::EnteringQueue", szEvent ) )
	{
		if ( pEvent->GetBool( "_remote_host" ) || !pEvent->GetUint64( "_remote_xuidsrc" ) ) // only comes from remote host
		{
			IMatchSession *pSession = g_pMatchFramework->GetMatchSession();
			XUID xuid = pEvent->GetUint64( "xuid" );
			const char* szErrorLocString = "#SFUI_Lobby_LeaderEnteredQueue";
			
			// Localize the game mode
			wchar_t const *pwchGameMode = g_pVGuiLocalize->Find( CFmtStr( "#SFUI_GameMode%s", pSession->GetSessionSettings()->GetString( "game/mode", "custom" ) ) );
			if ( !pwchGameMode )
				pwchGameMode = g_pVGuiLocalize->FindSafe( "#SFUI_GameModeCustom" );

			// Localize the mapgroup names
			CUtlVector< wchar_t const * > arrLocalizedNames;
			{
				CUtlVector< char * > arrTokenizedMaps; // DON"T FORGET TO FREE THIS MEMORY
				V_SplitString( pSession->GetSessionSettings()->GetString( "game/mapgroupname" ), ",", arrTokenizedMaps );
				FOR_EACH_VEC( arrTokenizedMaps, iMap )
				{
					char const *szNameID = CUiComponent_GameTypes::GetInstance()->GetMapGroupAttribute( arrTokenizedMaps[ iMap ], "nameID" );
					if ( !szNameID || !*szNameID )
						continue;

					wchar_t const *wszNameLocalized = g_pVGuiLocalize->Find( szNameID );
					if ( !wszNameLocalized || !*wszNameLocalized )
						continue;

					arrLocalizedNames.AddToTail( wszNameLocalized );
				}
				arrTokenizedMaps.PurgeAndDeleteElements(); // THIS FREES THE MEMORY
			}
			if ( arrLocalizedNames.IsEmpty() )
				arrLocalizedNames.AddToTail( pwchGameMode );

			// Concatenate all these strings
			CUtlVector< wchar_t > arrFullLocalizedConcatenatedText;
			arrFullLocalizedConcatenatedText.AddMultipleToTail( V_wcslen( pwchGameMode ), pwchGameMode );
			arrFullLocalizedConcatenatedText.AddMultipleToTail( 3, L" - " );
			FOR_EACH_VEC( arrLocalizedNames, iLocMap )
			{
				arrFullLocalizedConcatenatedText.AddMultipleToTail( V_wcslen( arrLocalizedNames[iLocMap] ), arrLocalizedNames[ iLocMap ] );
				arrFullLocalizedConcatenatedText.AddMultipleToTail( 2, L", " );
			}
			arrFullLocalizedConcatenatedText[ arrFullLocalizedConcatenatedText.Count() - 2 ] = 0; // null-terminate the string

			// Convert the string to UTF8
			char chFullLocalizedConcatenatedSettings[ 1024 ];
			V_UnicodeToUTF8( arrFullLocalizedConcatenatedText.Base(), chFullLocalizedConcatenatedSettings, sizeof( chFullLocalizedConcatenatedSettings ) );
			chFullLocalizedConcatenatedSettings[ Q_ARRAYSIZE( chFullLocalizedConcatenatedSettings ) - 1 ] = 0;

			// Add the messages
			CPanel2D* pPanel = AddMatchmakingErrorMessage( szErrorLocString, xuid, chFullLocalizedConcatenatedSettings, HUD_CHAT_PARTY_COLOR_GREEN ); // handles adding in-game chat message too
			pPanel->AddClass( "error-message--green" );
			AddIngameChatMessageAboutPlayer( szErrorLocString, xuid, chFullLocalizedConcatenatedSettings, HUD_CHAT_PARTY_COLOR_GREEN );

			if ( !CUiComponent_Lobby::GetInstance()->BIsHost() )
			{
				GameUI().PlayUISoundScript( "UIPanorama.mainmenu_press_GO", m_pChatLinesContainer );
			}
		}
	}
	else if ( !Q_stricmp( "Command::Game::ChatReportMatchmakingStatus", szEvent ) )
	{
		const char* szStatus = pEvent->GetString( "status" );
		AddTournamentMatchmakingStatusMessage( szStatus );
	}
	else if ( !Q_stricmp( "Command::Game::ChatReportMatchmakingStatusW", szEvent ) )
	{
		if ( !pEvent->GetUint64( "_remote_xuidsrc" ) ) // always a local print
		{
			const wchar_t *wszStatus = pEvent->GetWString( "status" );
			char szMsgBuf[ 1024 ];
			V_UnicodeToUTF8( wszStatus, szMsgBuf, sizeof( szMsgBuf ) );

			AddTournamentMatchmakingStatusMessage( szMsgBuf );
		}
	}
	else if ( !Q_stricmp( "OnMatchSessionUpdate", szEvent ) )
	{
		const char *szState = pEvent->GetString( "state", "" );
		if ( !V_stricmp( "ready", szState ) || !V_stricmp( "makeonline", szState ) )
		{
			CPanel2D *pNewEntry = AddChatEntry( "PlayerJoined" );
			pNewEntry->SetDialogVariableLocString( "player_joined_loc_token", ( !CUiComponent_Lobby::GetInstance()->BIsHost() ) ? "#party_lobby_connected" : "#party_lobby_started" );
		}

		if ( !V_stricmp( "updated", szState ) )
		{
			char const *szStopQueueReason = pEvent->GetString( "Delete/game/mmqueuestop" );
			if ( szStopQueueReason && *szStopQueueReason )
			{
				if ( !V_stricmp( szStopQueueReason, "stop" ) )
				{
					// Show cancel message if we're in a group, otherwise don't bother (GetHostSteamID fails in offline lobbies)
					XUID xuid = CUiComponent_Lobby::GetInstance()->GetHostSteamID();
					if ( xuid )
					{
						AddMatchmakingErrorMessage( "#SFUI_QMM_State_cancel_by_host", xuid, NULL, NULL );

						GameUI().PlayUISoundScript( "PanoramaUI.Lobby.Error", m_pChatLinesContainer );
					}
				}
			}
		}
	}
	else if ( !Q_stricmp( "OnPlayerLeaderChanged", szEvent ) )
	{
		// Show cancel message if we're in a group, otherwise don't bother (GetHostSteamID fails in offline lobbies)
		XUID xuid = CUiComponent_Lobby::GetInstance()->GetHostSteamID();
		if ( CUiComponent_Lobby::GetInstance()->BIsHost() )
			AddMatchmakingErrorMessage( "#SFUI_QMM_State_cancel_leader_you", xuid, NULL, NULL );
		else
			AddMatchmakingErrorMessage( "#SFUI_QMM_State_cancel_leader_changed", xuid, NULL, NULL );

		GameUI().PlayUISoundScript( "PanoramaUI.Lobby.Error", m_pChatLinesContainer );
	}

}

panorama::CPanel2D* CCSGO_Chat::AddChatEntry( const char* szSnippetName /*=nullptr*/ )
{
	panorama::CPanel2D *pNewPanel = new panorama::CPanel2D( m_pChatLinesContainer, nullptr );
	if ( szSnippetName )
	{
		pNewPanel->RequireLoadLayoutSnippet( szSnippetName );
	}
	m_pChatLinesContainer->MoveChildBefore( pNewPanel, m_pChatLinesContainer->GetFirstChild() );
	pNewPanel->AddClass( "chat-entry" );
	DispatchEvent( OnNewChatEntry(), this, pNewPanel->UIPanel(), szSnippetName );
	return pNewPanel;
}

panorama::CPanel2D* CCSGO_Chat::AddMatchmakingErrorMessage( const char* szErrorLocString, XUID errorID, const char* szClan, const char *szColor )
{
	panorama::CPanel2D* pNewPanel = AddChatEntry( "MatchmakingError" );
	
	// For legacy reasons the first dialog var is a steam id and the second is a clan name for all error messages
	pNewPanel->SetDialogVariable( "s1", CUiComponent_FriendsList::GetInstance()->GetPlayerNameInternal( errorID ) );
	pNewPanel->SetDialogVariable( "s2", szClan );
	pNewPanel->SetDialogVariableLocString( "error_text", szErrorLocString );

	return pNewPanel;
}

void CCSGO_Chat::AddIngameChatMessageAboutPlayer( const char* szErrorLocString, XUID errorID, const char* szClan, const char *szColor )
{
	//
	// Format for display in-game
	//
#ifdef PANORAMA_ENABLE
	if ( GameUI().IsPanoramaEnabled() && hud_chat_print_party_enabled.GetBool() )
	{
		CUtlBuffer bufUserNameSafe;
		Helper_ProcessPersonaNameForChat( errorID, bufUserNameSafe );

		CUtlBuffer bufClanStringSafe;
		if ( szClan )
			Helper_ProcessPersonaNameForChat( 0ull, bufClanStringSafe, szClan );

		if ( wchar_t *formatStr = g_pVGuiLocalize->Find( szErrorLocString ) )
		{
			wchar_t szconverted[ 2048 ];
			g_pVGuiLocalize->ConstructString( szconverted, sizeof( szconverted ), formatStr, 2,
				( wchar_t const * ) bufUserNameSafe.Base(), bufClanStringSafe.Size() ? ( wchar_t const * ) bufClanStringSafe.Base() : L"" );

			// Format them all together
			CUtlBuffer bufSafeJointFormatUCS2;
			int numJointLen = V_wcslen( szconverted ) + 512;
			bufSafeJointFormatUCS2.EnsureCapacity( numJointLen );
			V_snwprintf( ( wchar_t * ) bufSafeJointFormatUCS2.Base(), numJointLen / sizeof( wchar_t ),
				L"<font color=\"" PRI_S_FOR_WS "\">" PRI_WS_FOR_WS L"</font> "
				L"<font color=\"" PRI_S_FOR_WS "\">" PRI_WS_FOR_WS L"</font>",
				HelperGetChatPartyColor(), HelperGetChatPartyPrefix(),
				szColor, szconverted );

			if ( CHudElement *pElement = GetHud().FindElement( "CCSGO_HudVoiceStatus" ) )
			{
				char chNotice[ 2048 ];
				V_UnicodeToUTF8( ( wchar_t * ) bufSafeJointFormatUCS2.Base(), chNotice, sizeof( chNotice ) );
				chNotice[ sizeof( chNotice ) - 1 ] = 0;
				( ( CCSGO_HudVoiceStatus * ) pElement )->PushNotice( chNotice, -1, true );
			}
		}
	}
#endif
}

panorama::CPanel2D* CCSGO_Chat::AddTournamentMatchmakingStatusMessage( const char* szStatus )
{
	panorama::CPanel2D* pNewPanel = AddChatEntry( "TournamentStatus" );
	CLabel *pLabel = ToPanel<CLabel>( pNewPanel->FindChildInLayoutFile( "MessageLabel" ) );
	pLabel->SetText( szStatus );
	return pNewPanel;
}

void CCSGO_Chat::SetupJavascriptObjectTemplate()
{
	BaseClass::SetupJavascriptObjectTemplate();

	RegisterJSMethod( "SubmitChatText", PANORAMA_DELEGATE( &CCSGO_Chat::SubmitChatText ) );
}

void CCSGO_Chat::SubmitChatText( void )
{
	if ( !StringIsEmpty( m_pChatInputTextEntry->PchGetText() ) )
		Helper_SendLobbyChat( m_pChatInputTextEntry->PchGetText() );
}

const char* Helper_GetTeammateColorAsString( XUID ullSteamID )
{
	if ( IMatchSession *pSession = g_pMatchFramework->GetMatchSession() )
	{
		if ( KeyValues *kvPlayer = SessionMembersFindPlayer( pSession->GetSessionSettings(), ullSteamID ) )
		{
			switch ( kvPlayer->GetInt( "game/teamcolor", -1 ) )
			{
				case 0:
					return "yellow";
				case 1:
					return "purple";
				case 2:
					return "green";
				case 3:
					return "blue";
				case 4:
					return "orange";
			}
		}
	}
	return NULL;
}

panorama::CPanel2D* CCSGO_Chat::AddChatText( XUID ullSteamID, const char* szMessageUTF8, bool bTagMessageAsXuidSaidText )
{
	static const panorama::CPanoramaSymbol k_symSpeakerXuid( "SpeakerXuid" );

	panorama::CPanel2D* pPreviousChatLine = bTagMessageAsXuidSaidText ? m_pChatLinesContainer->GetFirstChild() : NULL;

	// Initial message includes avatar on the same line
	panorama::CPanel2D* pEntry = AddChatEntry( "PlayerChat" );
	
	if ( bTagMessageAsXuidSaidText )
		pEntry->SetAttribute( k_symSpeakerXuid, ullSteamID );

	if ( ClientSteamContext().GetLocalPlayerSteamID().ConvertToUint64() == ullSteamID )
	{
		pEntry->AddClass( "self-chat" );
	}
	else
	{
		SetDialogVariable( "player_name", CUiComponent_FriendsList::GetInstance()->GetPlayerNameInternal( ullSteamID ) );
	}

	CCSGO_AvatarImage* pAvatar = ToPanel<CCSGO_AvatarImage>( pEntry->FindChildInLayoutFile( "ChatAvatar" ) );
	if ( pAvatar )
	{
		if ( !pPreviousChatLine || pPreviousChatLine->GetAttribute( k_symSpeakerXuid, 0ull ) != ullSteamID )
			pAvatar->SetSteamID( CSteamID( ullSteamID ) ); // Show avatars for new speakers
		else
			pAvatar->DeleteAsync();
	}

	pEntry->SetDialogVariable( "player_name", CUiComponent_FriendsList::GetInstance()->GetPlayerNameInternal( ullSteamID ) );
	pEntry->SetDialogVariable( "msg", szMessageUTF8 );
	const char* szColor = Helper_GetTeammateColorAsString( ullSteamID );
	if ( szColor && bTagMessageAsXuidSaidText )
		pEntry->AddClass( CFmtStr( "player-color__%s", szColor ) );

	return pEntry;
}

bool CCSGO_Chat::EventOnPlayerRemoved( const char* szXuid )
{
	XUID xuidPlayer = V_atoi64( szXuid );
	bool bInMatchmakingQueue = CUiComponent_Lobby::GetInstance()->BInMatchmakingQueue();
	char const *szLocTokenNotification = bInMatchmakingQueue ? "#SFUI_Lobby_PersonLeftStopQM" : "#SFUI_Lobby_PersonLeft";
	AddIngameChatMessageAboutPlayer( szLocTokenNotification, xuidPlayer, NULL, HUD_CHAT_PARTY_COLOR_RED );

	// Add Party info
	CPanel2D *pNewEntry = AddChatEntry( bInMatchmakingQueue ? "MatchmakingError" : "PlayerLeft" );
	pNewEntry->SetDialogVariable( "s1", CUiComponent_FriendsList::GetInstance()->GetPlayerNameInternal( xuidPlayer ) );
	pNewEntry->SetDialogVariableLocString( bInMatchmakingQueue ? "error_text" : "player_left_loc_token", szLocTokenNotification );

	GameUI().PlayUISoundScript( "UI.Lobby.Left", m_pChatLinesContainer );
	return false;
}

bool CCSGO_Chat::EventOnPlayerJoined( const char* szXuid )
{
	XUID xuidPlayer = V_atoi64( szXuid );
	IMatchSession *pSession = g_pMatchFramework->GetMatchSession();

	bool bInMatchmakingQueue = CUiComponent_Lobby::GetInstance()->BInMatchmakingQueue();
	char const *szToken = bInMatchmakingQueue ? "#SFUI_Lobby_PersonJoinedStopQM" : "#SFUI_Lobby_PersonJoined";
	KeyValues *kvPlayer = pSession ? SessionMembersFindPlayer( pSession->GetSessionSettings(), xuidPlayer ) : NULL;
	char const *szJoinedViaClanTag = kvPlayer->GetString( "game/jclantag", NULL );
	if ( szJoinedViaClanTag )
	{
		szToken = bInMatchmakingQueue ? "#SFUI_LobbyClan_PersonJoinedStopQM" : "#SFUI_LobbyClan_PersonJoined";
	}
	else if ( int nNearbyJoin = kvPlayer->GetInt( "game/nby" ) )
	{
		szToken = bInMatchmakingQueue ? "#SFUI_LobbyNearby_PersonJoinedStopQM" : "#SFUI_LobbyNearby_PersonJoined";
	}
	else if ( XUID xuidJoinViaFriend = kvPlayer->GetUint64( "game/jfriend" ) )
	{
		char const *szFriendName = CUiComponent_FriendsList::GetInstance()->GetPlayerNameInternal( xuidJoinViaFriend );
		if ( szFriendName && *szFriendName )
		{
			szJoinedViaClanTag = szFriendName;
			szToken = "#SFUI_LobbyFriend_PersonJoined";
			szToken = bInMatchmakingQueue ? "#SFUI_LobbyFriend_PersonJoinedStopQM" : "#SFUI_LobbyFriend_PersonJoined";
		}
	}
	AddIngameChatMessageAboutPlayer( szToken, xuidPlayer, szJoinedViaClanTag, HUD_CHAT_PARTY_COLOR_GREEN );

	// Add Party info
	CPanel2D *pNewEntry = AddChatEntry( bInMatchmakingQueue ? "MatchmakingError" : "PlayerJoined" );
	// pNewEntry->SetDialogVariable( "s1", CUiComponent_FriendsList::GetInstance()->GetPlayerNameInternal( xuidPlayer ) );
	// explicitly call Steam because the full user information might not be available yet:
	pNewEntry->SetDialogVariable( "s1", Helper_GetFriendPersonaNameSanitized( xuidPlayer % (MAX_PLAYERS+1) ) );
	char *pszDuplicateCensoredClanTag = strdup( szJoinedViaClanTag );
	g_BannedWords.CensorBannedWordsInplace( pszDuplicateCensoredClanTag );
	pNewEntry->SetDialogVariable( "s2", pszDuplicateCensoredClanTag );
	free( pszDuplicateCensoredClanTag );
	pNewEntry->SetDialogVariableLocString( bInMatchmakingQueue ? "error_text" : "player_joined_loc_token", szToken );
	GameUI().PlayUISoundScript( "UI.Lobby.Joined", m_pChatLinesContainer );
	return false;
}

