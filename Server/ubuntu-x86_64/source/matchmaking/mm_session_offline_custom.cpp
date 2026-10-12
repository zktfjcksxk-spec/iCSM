//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "mm_framework.h"

#include "fmtstr.h"
#include "netmessages_signon.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//
// CMatchSessionOfflineCustom
//
// Implementation of an offline session
// that allows customization before the actual
// game commences (like playing commentary mode
// or playing single-player)
//

CMatchSessionOfflineCustom::CMatchSessionOfflineCustom( KeyValues *pSettings ) :
	m_pSettings( pSettings->MakeCopy() ),
	m_autodelete_pSettings( m_pSettings ),
	m_eState( STATE_INIT ),
	m_bExpectingServerReload( false ),
	m_pSysSession( NULL )
{
	g_pMMF->DiagnosticInfoAdd( CFmtStr( "session_offline(%p) created", this ) );

	DevMsg( "Created CMatchSessionOfflineCustom:\n" );
	KeyValuesDumpAsDevMsg( m_pSettings, 1 );

	InitializeGameSettings();
}

CMatchSessionOfflineCustom::~CMatchSessionOfflineCustom()
{
	g_pMMF->DiagnosticInfoAdd( CFmtStr( "session_offline(%p) closed", this ) );

	DevMsg( "Destroying CMatchSessionOfflineCustom:\n" );
	KeyValuesDumpAsDevMsg( m_pSettings, 1 );
}

KeyValues * CMatchSessionOfflineCustom::GetSessionSettings()
{
	return m_pSettings;
}

void CMatchSessionOfflineCustom::UpdateSessionSettings( KeyValues *pSettings )
{
	// Extend the update keys
	g_pMMF->GetMatchTitleGameSettingsMgr()->ExtendGameSettingsUpdateKeys( m_pSettings, pSettings );
	m_pSettings->MergeFrom( pSettings );

	DevMsg( "CMatchSessionOfflineCustom::UpdateSessionSettings\n" );
	KeyValuesDumpAsDevMsg( m_pSettings );

	// Broadcast the update to everybody interested
	MatchSession_BroadcastSessionSettingsUpdate( pSettings );
}

void CMatchSessionOfflineCustom::UpdateTeamProperties( KeyValues *pTeamProperties )
{
}

void CMatchSessionOfflineCustom::Command( KeyValues *pCommand )
{
	char const *szCommand = pCommand->GetName();

	g_pMMF->DiagnosticInfoAdd( CFmtStr( "session_offline(%p) command %s", this, szCommand ) );

	if ( !Q_stricmp( "MakeOnline", szCommand ) && m_eState < STATE_RUNNING )
	{
		// A combination of:
		//	CMatchSessionOnlineHost::CMatchSessionOnlineHost
		//	CMatchSessionOnlineHost::Update()
		//

		if ( m_pSysSession )
		{
#if DEVELOPMENT_ONLY
			// Fire off a development only warning to make sure programmers don't intentionally spam "MakeOnline" requests to the session
			// Not shipping it as public visible warning, since there may be external requests that require making an online session,
			// for example two friends independently deciding to join this user, so we don't want to spam user console with a warning
			// that they cannot do anything about.
			DevWarning( "Offline session is already being converted to online session, ignoring additional conversion request! (development warning)\n" );
#endif
			return;
		}

		const bool bIgnoreConnectedSessionUpgrade = true;

		if ( bIgnoreConnectedSessionUpgrade && g_pMatchExtensions->GetIVEngineClient()->IsConnected() )
		{
			return;
		}

		Msg( "Converting offline session to online session...\n" );

		// Generate a new encryption cookie
		unsigned char chEncryptionCookie[ sizeof( uint64 ) ] = {};
		for ( int j = 0; j < ARRAYSIZE( chEncryptionCookie ); ++j )
			chEncryptionCookie[ j ] = RandomInt( 1, 254 );

		// Trigger session creation
		m_pSysSession = new CSysSessionHost( m_pSettings );
		m_pSysSession->SetCryptKey( *reinterpret_cast< uint64 * >( chEncryptionCookie ) );

		g_pMatchEventsSubscription->BroadcastEvent( new KeyValues( "OnMatchSessionUpdate", "state", "makeonline", "makeonlinestate", "starting" ) );

		return;
	}
	if ( ( !Q_stricmp( "Start", szCommand ) || !Q_stricmp( "StartListenServer", szCommand ) ) && m_eState < STATE_RUNNING )
	{
		m_eState = STATE_RUNNING;

		OnGamePrepareLobbyForGame();

		UpdateSessionSettings( KeyValues::AutoDeleteInline( KeyValues::FromString(
				"update",
				" update { "
					" server { "
						" server listen "
					" } "
				" } "
				) ) );

		g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues(
			"OnProfilesWriteOpportunity", "reason", "sessionstart"
			) );

		if ( pCommand->GetBool( "panorama" ) )
			m_pSettings->SetString( "options/play", "dontmodifylocalsession" );
		
		bool bResult = g_pMatchFramework->GetMatchTitle()->StartServerMap( m_pSettings );
		if ( !bResult )
		{
			Warning( "Failed to start server map!\n" );
			KeyValuesDumpAsDevMsg( m_pSettings, 1 );
			Assert( 0 );
			g_pMatchEventsSubscription->BroadcastEvent( new KeyValues( "OnMatchSessionUpdate", "state", "error", "error", "nomap" ) );
		}
		Msg( "Succeeded in starting server map!\n" );

		//
		// Let's shut down the session because for panorama we don't want to keep the single-player sessions around
		//
		if ( pCommand->GetBool( "panorama" ) )
			g_pMatchEventsSubscription->BroadcastEvent( new KeyValues( "mmF->CloseSession" ) );

		return;
	}
	if ( !Q_stricmp( "QueueConnect", szCommand ) )
	{
		char const *szConnectAddress = pCommand->GetString( "adronline", "0.0.0.0" );
		uint64 uiReservationId = pCommand->GetUint64( "reservationid" );
		bool bAutoCloseSession = pCommand->GetBool( "auto_close_session" );
		Assert( bAutoCloseSession );
		if ( bAutoCloseSession )
		{
			// Switch the state
			m_eState = STATE_RUNNING;

			MatchSession_PrepareClientForConnect( m_pSettings, uiReservationId );

			// Close the session, potentially resetting a bunch of state
			if ( bAutoCloseSession )
				g_pMatchFramework->CloseSession();

			// Determine reservation settings required
			g_pMatchExtensions->GetINetSupport()->UpdateClientReservation( uiReservationId, 0ull );

			// Issue the connect command
			g_pMatchExtensions->GetIVEngineClient()->StartLoadingScreenForCommand( CFmtStr( "connect %s", szConnectAddress ) );

			return;
		}
	}

	//
	// Let the title-specific matchmaking handle the command
	//
	CUtlVector< KeyValues * > arrPlayersUpdated;
	arrPlayersUpdated.SetCount( m_pSettings->GetInt( "members/numPlayers", 0 ) );
	memset( arrPlayersUpdated.Base(), 0, arrPlayersUpdated.Count() * sizeof( KeyValues * ) );

	g_pMMF->GetMatchTitleGameSettingsMgr()->ExecuteCommand( pCommand, GetSessionSystemData(), m_pSettings, arrPlayersUpdated.Base() );

	// Now notify the framework about player updated
	for ( int k = 0; k < arrPlayersUpdated.Count(); ++ k )
	{
		if ( !arrPlayersUpdated[k] )
			break;

		// Notify the framework about player updated
		KeyValues *kvEvent = new KeyValues( "OnPlayerUpdated" );
		kvEvent->SetUint64( "xuid", arrPlayersUpdated[k]->GetUint64( "xuid" ) );
		g_pMatchEventsSubscription->BroadcastEvent( kvEvent );
	}

	//
	// Send the command as event for handling
	//
	KeyValues *pEvent = pCommand->MakeCopy();
	pEvent->SetName( CFmtStr( "Command::%s", pCommand->GetName() ) );
	g_pMatchEventsSubscription->BroadcastEvent( pEvent );
}

uint64 CMatchSessionOfflineCustom::GetSessionID()
{
	return 0;
}

void CMatchSessionOfflineCustom::Update()
{
	switch ( m_eState )
	{
	case STATE_INIT:
		m_eState = STATE_CONFIG;
		
		// Let everybody know that the session is now ready
		g_pMatchEventsSubscription->BroadcastEvent( new KeyValues( "OnMatchSessionUpdate", "state", "ready", "transition", "offlineinit" ) );
		break;
	}

	if ( m_pSysSession )
	{
		m_pSysSession->Update();
	}
}

void CMatchSessionOfflineCustom::Destroy()
{
	if ( m_pSysSession )
	{
		m_pSysSession->Destroy();
		m_pSysSession = NULL;
	}

	if ( m_eState == STATE_RUNNING )
	{
		g_pMatchExtensions->GetIVEngineClient()->ExecuteClientCmd( "disconnect" );

		g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues(
			"OnProfilesWriteOpportunity", "reason", "sessionend"
			) );
	}

	delete this;
}

void CMatchSessionOfflineCustom::DebugPrint()
{
	DevMsg( "CMatchSessionOfflineCustom [ state=%d ]\n", m_eState );
	KeyValuesDumpAsDevMsg( m_pSettings, 1 );
}

void CMatchSessionOfflineCustom::OnEvent( KeyValues *pEvent )
{
	char const *szEvent = pEvent->GetName();

	if ( !Q_stricmp( "OnEngineClientSignonStateChange", szEvent ) )
	{
		int iOldState = pEvent->GetInt( "old", 0 );
		int iNewState = pEvent->GetInt( "new", 0 );

		if ( iOldState >= SIGNONSTATE_CONNECTED &&
			iNewState  < SIGNONSTATE_CONNECTED )
		{
			// Disconnecting from server
			DevMsg( "OnEngineClientSignonStateChange\n" );
			if ( m_bExpectingServerReload )
			{
				m_bExpectingServerReload = false;
				DevMsg( " session was expecting server reload...\n" );
				return;
			}

			if ( !( g_pMMF->GetMatchTitle()->GetTitleSettingsFlags() & MATCHTITLE_SESSIONS_PERSIST_MAPS ) )
			{	// Disconnecting from the server will shut down the session (unless title is operating with persistent modeless parties)
				g_pMatchEventsSubscription->BroadcastEvent( new KeyValues( "mmF->CloseSession" ) );
			}
			return;
		}
	}
	else if ( !Q_stricmp( "OnEngineClientSignonStatePrepareChange", szEvent ) )
	{
		char const *szReason = pEvent->GetString( "reason" );
		if ( !Q_stricmp( "reload", szReason ) )
		{
			Assert( !m_bExpectingServerReload );
			m_bExpectingServerReload = true;
			return;
		}
		else if ( !Q_stricmp( "load", szReason ) )
		{
			char const *szLevelName = g_pMatchExtensions->GetIVEngineClient()->GetLevelName();
			if ( szLevelName && szLevelName[0] && g_pMatchExtensions->GetIVEngineClient()->IsConnected() )
			{
				Assert( !m_bExpectingServerReload );
				m_bExpectingServerReload = true;
				return;
			}
		}
	}
	else if ( !Q_stricmp( "OnEngineEndGame", szEvent ) )
	{
		DevMsg( "OnEngineEndGame\n" );
		
		// Issue the disconnect command
		g_pMatchExtensions->GetIVEngineClient()->ExecuteClientCmd( "disconnect" );
		
		if ( !( g_pMMF->GetMatchTitle()->GetTitleSettingsFlags() & MATCHTITLE_SESSIONS_PERSIST_MAPS ) )
		{	// Disconnecting from the server will shut down the session (unless title is operating with persistent modeless parties)
			g_pMatchEventsSubscription->BroadcastEvent( new KeyValues( "mmF->CloseSession" ) );
		}
		return;
	}
#if !defined( NO_STEAM )
	else if ( !Q_stricmp( "mmF->SysSessionUpdate", szEvent ) )
	{
		if ( m_pSysSession && pEvent->GetPtr( "syssession", NULL ) == m_pSysSession )
		{
			// If the user is already playing offline, then discard the session creation event
			if ( m_eState >= STATE_RUNNING )
			{
				DevWarning( "Offline session is already in active game, discarding online session\n" );
				m_pSysSession->Destroy();
				m_pSysSession = NULL;

				g_pMatchEventsSubscription->BroadcastEvent( new KeyValues( "OnMatchSessionUpdate", "state", "makeonline", "makeonlinestate", "discarded" ) );
				return;
			}

			// We had a session error
			if ( char const *szError = pEvent->GetString( "error", NULL ) )
			{
				DevWarning( "Offline session failed to convert to online session: %s\n", szError );

				// Destroy the session
				m_pSysSession->Destroy();
				m_pSysSession = NULL;

				g_pMatchEventsSubscription->BroadcastEvent( new KeyValues( "OnMatchSessionUpdate", "state", "makeonline", "makeonlinestate", szError ) );
				return;
			}

			//////////////////////////////////////////////////////////////////////////
			// Destroy our instance and create the new match interface
			//
			// see: pExtendedSettings->SetString( "state", szMigrateState ); in mm_session_online_client.cpp
			//

			KeyValues *pExtendedSettings = new KeyValues( "ExtendedSettings" );
			KeyValues *pOriginalSessionSettings = m_pSettings;
			char const *szMigrateState = "lobby";
			pExtendedSettings->SetString( "state", szMigrateState );
			pExtendedSettings->SetUint64( "crypt", m_pSysSession->GetCryptKey() );
			pExtendedSettings->AddSubKey( pOriginalSessionSettings );
			
			// Release ownership of the resources since new match session now owns them
			m_pSettings = NULL;
			m_autodelete_pSettings.Assign( NULL );

			CSysSessionHost *pSysSessionHost = m_pSysSession;
			m_pSysSession = NULL;

			g_pMMF->SetCurrentMatchSession( NULL );
			this->Destroy();

			// Migrate the host session into the client to support correct re-migration into host and fully setting up the Steam lobby object
			KeyValues *kvMigrateCommand = new KeyValues( "makeonline", "migrate", "host>client", "event", "suppressed" );
			KeyValues::AutoDelete autodelete_kvMigrateCommand( kvMigrateCommand );
			kvMigrateCommand->SetUint64( "xuid", steamapicontext->SteamUser()->GetSteamID().ConvertToUint64() );
			pSysSessionHost->Migrate( kvMigrateCommand );
			CSysSessionClient *pSysSessionClient = new CSysSessionClient( pSysSessionHost, pOriginalSessionSettings );
			
			// After we converted the host session into client session object, we should destroy it
			pSysSessionHost->Destroy();
			pSysSessionHost = NULL;

			// Prepare the client session for migration as part of creating online host
			kvMigrateCommand->SetString( "migrate", "client>host" );
			pSysSessionClient->Migrate( kvMigrateCommand );

			// Slamming settings to perform full post init of members
			pExtendedSettings->SetString( "migrate", "fullpostinit" );

			// Now we need to create the new host session that will install itself
			IMatchSession *pNewHost = new CMatchSessionOnlineHost( pSysSessionClient, pExtendedSettings );
			Assert( g_pMMF->GetMatchSession() == pNewHost );
			pNewHost;
			pSysSessionClient = NULL; // as part of "new CMatchSessionOnlineHost" the sys session client object is destroyed and deleted

			// Everything completed successfully
			g_pMatchEventsSubscription->BroadcastEvent( new KeyValues( "OnMatchSessionUpdate", "state", "makeonline", "makeonlinestate", "ok" ) );
		}
	}
#endif
}

void CMatchSessionOfflineCustom::InitializeGameSettings()
{
	// Since the session can be created with a minimal amount of data available
	// the session object is responsible for initializing the missing data to defaults
	// or saved values or values from gamer progress/profile or etc...

	if ( KeyValues *kv = m_pSettings->FindKey( "system", true ) )
	{
		kv->SetString( "network", "offline" );
		if ( !kv->GetString( "access", NULL ) ) // do not stomp access setting (privacy can be configured before session is created)
			kv->SetString( "access", "private" );
	}

	if ( KeyValues *kv = m_pSettings->FindKey( "options", true ) )
	{
		if ( !kv->GetString( "server", NULL ) ) // do not stomp server setting (official/listen, CS:GO uses offline sessions for all pre-party UI)
			kv->SetString( "server", "listen" );
	}

	if ( KeyValues *pMembers = m_pSettings->FindKey( "members", true ) )
	{
		pMembers->SetInt( "numMachines", 1 );

		int numPlayers = 1;
#ifdef _GAMECONSOLE
		numPlayers = XBX_GetNumGameUsers();
#endif
		pMembers->SetInt( "numPlayers", numPlayers );
		pMembers->SetInt( "numSlots", numPlayers );

		if ( KeyValues *pMachine = pMembers->FindKey( "machine0", true ) )
		{
			IPlayerLocal *pPriPlayer = g_pPlayerManager->GetLocalPlayer( XBX_GetPrimaryUserId() );

			pMachine->SetUint64( "id", ( pPriPlayer ? pPriPlayer->GetXUID() : INVALID_XUID ) );
			pMachine->SetUint64( "flags", MatchSession_GetMachineFlags() );
			pMachine->SetInt( "numPlayers", numPlayers );
			pMachine->SetUint64( "dlcmask", g_pMatchFramework->GetMatchSystem()->GetDlcManager()->GetDLCMask() );
			pMachine->SetString( "tuver", MatchSession_GetTuInstalledString() );
			pMachine->SetInt( "ping", 0 );

			for ( int k = 0; k < numPlayers; ++ k )
			{
				if ( KeyValues *pPlayer = pMachine->FindKey( CFmtStr( "player%d", k ), true ) )
				{
					int iController = 0;
#ifdef _GAMECONSOLE
					iController = XBX_GetUserId( k );
#endif
					IPlayerLocal *player = g_pPlayerManager->GetLocalPlayer( iController );
					if ( player )
					{
						pPlayer->SetUint64( "xuid", player->GetXUID() );
						pPlayer->SetString( "name", player->GetName() );
					}
				}
			}
		}
	}

	// Let the title extend the game settings
	g_pMMF->GetMatchTitleGameSettingsMgr()->InitializeGameSettings( m_pSettings, "host" );

	DevMsg( "CMatchSessionOfflineCustom::InitializeGameSettings adjusted settings:\n" );
	KeyValuesDumpAsDevMsg( m_pSettings, 1 );
}

void CMatchSessionOfflineCustom::OnGamePrepareLobbyForGame()
{
	// Remember which players will get updated
	CUtlVector< KeyValues * > arrPlayersUpdated;
	arrPlayersUpdated.SetCount( m_pSettings->GetInt( "members/numPlayers", 0 ) );
	memset( arrPlayersUpdated.Base(), 0, arrPlayersUpdated.Count() * sizeof( KeyValues * ) );

	g_pMMF->GetMatchTitleGameSettingsMgr()->PrepareLobbyForGame( m_pSettings, arrPlayersUpdated.Base() );

	// Notify the framework of the updates
	for ( int k = 0; k < arrPlayersUpdated.Count(); ++ k )
	{
		if ( !arrPlayersUpdated[k] )
			break;

		// Notify the framework about player updated
		KeyValues *kvEvent = new KeyValues( "OnPlayerUpdated" );
		kvEvent->SetUint64( "xuid", arrPlayersUpdated[k]->GetUint64( "xuid" ) );
		g_pMatchEventsSubscription->BroadcastEvent( kvEvent );
	}

	// Let the title prepare for connect
	MatchSession_PrepareClientForConnect( m_pSettings );
}
