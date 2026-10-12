//========= Copyright © Valve Corporation, All rights reserved. ============//
//
// Purpose: Forward server log lines to remote listeners
//
//=============================================================================//
#include "cbase.h"
#include "server_log_http_dispatcher.h"
#include "gameinterface.h"
#include "matchmaking/imatchframework.h"
#include "engine/inetsupport.h"
#include "gameinterface.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//
// Support driving HTTP log fully from the client too
//
static ISteamHTTP * HelperGetSteamHttpInterfaceForLog()
{
	ISteamHTTP *pISteamHTTP = steamgameserverapicontext ? steamgameserverapicontext->SteamHTTP() : NULL;
	if ( !pISteamHTTP && g_pMatchFramework && g_pMatchFramework->GetMatchExtensions() )
		pISteamHTTP = ( ISteamHTTP * ) g_pMatchFramework->GetMatchExtensions()->GetRegisteredExtensionInterface( STEAMHTTP_INTERFACE_VERSION );
	return pISteamHTTP;
}

class CServerLogDestination
{
public:
	CServerLogDestination( const char* szDestAddress )
	{
		if ( const char *szHttpUri = StringAfterPrefix( szDestAddress, "http$" ) )
		{
			m_sURI.Set( CFmtStr( "http://%s", szHttpUri ) );
			if ( char *pch = strchr( m_sURI.Access(), '$' ) )
				*pch = ':';
		}
		else if ( const char *szHttpsUri = StringAfterPrefix( szDestAddress, "https$" ) )
		{
			m_sURI.Set( CFmtStr( "https://%s", szHttpsUri ) );
			if ( char *pch = strchr( m_sURI.Access(), '$' ) )
				*pch = ':';
		}
		else
		{
			m_sURI.Set( szDestAddress );
		}
		m_flTimeout = 10.0f;
		m_hHTTPRequestHandle = INVALID_HTTPREQUEST_HANDLE;
		m_bIsFinished = false;
		m_unMaxUpdateStringSizeBytes = 250 * k_nKiloByte;
		m_lluUniqueTokenFromSecret = 0llu;
		static ConVarRef rev_LogAddrSecret( "logaddress_token_secret" );
		const char* szSecret = rev_LogAddrSecret.GetString();
		// HACK: Same code in sv_log, but not worth plumbing that through to share for this kinda feature... Doesn't really matter if they drift
		if ( szSecret && !StringIsEmpty( szSecret ) )
		{
			CRC64_ProcessBuffer( &m_lluUniqueTokenFromSecret, szSecret, V_strlen( szSecret ) );
			if ( !m_lluUniqueTokenFromSecret )
				m_lluUniqueTokenFromSecret = 1;
		}

		// Make another unique token based on this server instance
		INetSupport::ServerInfo_t serverInfo;
		if ( INetSupport *pINetSupport = ( INetSupport * )g_pMatchFramework->GetMatchExtensions()->GetRegisteredExtensionInterface( INETSUPPORT_VERSION_STRING ) )
		{
			m_lluUniqueTokenFromInstance = ( uint64 )pINetSupport->BuildServerInstanceCRCToken();
		}

		Reset();
	}
	~CServerLogDestination()
	{
		if ( m_hHTTPRequestHandle )
		{
			if ( ISteamHTTP *pISteamHTTP = HelperGetSteamHttpInterfaceForLog() )
				pISteamHTTP->ReleaseHTTPRequest( m_hHTTPRequestHandle );
		}
	}
	
	bool BRequestPending( void ) const { return m_hHTTPRequestHandle != INVALID_HTTPREQUEST_HANDLE; }
	bool BIsFinished( void ) const { return m_bIsFinished;  }
	bool SendUpdate( int32 iFromTIck, int32 iToTick, CServerLogHTTPDispatcher::LogLinesList_t::IndexType_t idxEndOfUpdate, const char* pLogLines, size_t unUpdateStrLenBytes );
	CServerLogHTTPDispatcher::LogLinesList_t::IndexType_t LastUpdateIndex() const { return m_idxLastAcknowledgedUpdate; }
	void Reset( void ) // Called during cleanup between maps or at shutdown
	{
		m_idxPendingUpdate = CServerLogHTTPDispatcher::LogLinesList_t::InvalidIndex();
		m_idxLastAcknowledgedUpdate = CServerLogHTTPDispatcher::LogLinesList_t::InvalidIndex();

		if ( m_hHTTPRequestHandle )
		{
			if ( ISteamHTTP *pISteamHTTP = HelperGetSteamHttpInterfaceForLog() )
				pISteamHTTP->ReleaseHTTPRequest( m_hHTTPRequestHandle );
		}
		m_hHTTPRequestHandle = NULL;
	}

	size_t MaxUpdateStringSizeBytes() const { return m_unMaxUpdateStringSizeBytes; }
	void DumpStatusToConsole( void ) const;
	const char* GetURI() const { return m_sURI.Get(); }
private:
	void Steam_OnHTTPRequestCompleted( HTTPRequestCompleted_t *p, bool bError );

	CUtlString m_sURI; // Base url for this destination

	// Keep track of the linked list node from the last successful update so we can continue where we left off
	CServerLogHTTPDispatcher::LogLinesList_t::IndexType_t m_idxLastAcknowledgedUpdate;
	CServerLogHTTPDispatcher::LogLinesList_t::IndexType_t m_idxPendingUpdate; 

	HTTPRequestHandle m_hHTTPRequestHandle; // Pending request if any
	float m_flTimeout; // Timeout passed to steamhttp callbacks
	size_t m_unMaxUpdateStringSizeBytes;
	bool m_bIsFinished; // This listener is done for some reason, remove us from queue next update
	uint64 m_lluUniqueTokenFromSecret; // Some unique string set before adding a listener to be sent along with the request. Here to match behavior of udp system.
	uint64 m_lluUniqueTokenFromInstance; // Unique token from cmd line, steam id and other inputs that should be unique to this server instance.
	// SteamHTTP members for callback handling
	CCallResult< CServerLogDestination, HTTPRequestCompleted_t > m_CallbackOnHTTPRequestCompleted;
};

static CServerLogHTTPDispatcher g_ServerLogHTTPDispatcher;
CServerLogHTTPDispatcher* GetServerLogHTTPDispatcher( void ) { return &g_ServerLogHTTPDispatcher; }

CON_COMMAND_F( logaddress_add_http, "Set URI of a listener to receive logs via http post. Wrap URI in double quotes.", FCVAR_RELEASE | FCVAR_UNLOGGED )
{
	if ( args.ArgC() != 2 )
	{
		ConMsg( "logaddress_add_http: Invalid parameters, must be URI of a listener to receive logs wrapped in quotes, eg \"http://127.0.0.1:3001\"\n" );
		return;
	}

	GetServerLogHTTPDispatcher()->AddListener( args[ 1 ] );
}

CON_COMMAND_F( logaddress_delall_http, "Remove all http listeners from the dispatch list.", FCVAR_RELEASE | FCVAR_UNLOGGED )
{
	GetServerLogHTTPDispatcher()->RemoveAllListeners();
}

CON_COMMAND_F( logaddress_list_http, "List all URIs currently receiving server logs", FCVAR_RELEASE | FCVAR_UNLOGGED )
{
	GetServerLogHTTPDispatcher()->DumpListenersToConsole();
}

ConVar sv_log_http_record_before_any_listeners( "sv_log_http_record_before_any_listeners", 0, FCVAR_RELEASE );

CServerLogHTTPDispatcher::CServerLogHTTPDispatcher():
	CAutoGameSystemPerFrame( "ServerLogHTTPDispatcher" )
{ 
	Reset();
}

void CServerLogHTTPDispatcher::BuildTimestampString( CUtlString & outStr )
{
	double flTimeSpentLogging = Plat_FloatTime() - m_loggingStartPlatTime;
	tm now;
	Plat_ConvertToLocalTime( ( m_localTimeLoggingStart + (uint32)flTimeSpentLogging ), &now );
	uint32 msecs = ( flTimeSpentLogging - ( uint32 )flTimeSpentLogging ) * 1000;
	outStr.Format( "%02u/%02u/%04u - %02u:%02u:%02u.%03u",
										now.tm_mon + 1, now.tm_mday, 1900 + now.tm_year,
										now.tm_hour, now.tm_min, now.tm_sec, msecs );
}

bool CServerLogHTTPDispatcher::LogForHTTPListeners( const char* szLogLine )
{
	if ( !szLogLine || StringIsEmpty( szLogLine ) )
		return false;

	if ( m_strLogLinesThisTick.IsEmpty() )
	{
		BuildTimestampString( m_strTimeStampPrefix );
	}

	m_strLogLinesThisTick.AppendFormat( "%s - %s", m_strTimeStampPrefix.Get(), szLogLine );
	return true;
}

void CServerLogHTTPDispatcher::AddListener( const char* szURI )
{
	FOR_EACH_VEC( m_vecListeners, i )
	{
		if ( V_strcmp( m_vecListeners[ i ]->GetURI(), szURI ) == 0 )
			return;
	}
	m_vecListeners.AddToTail( new CServerLogDestination( szURI ) );
}

void CServerLogHTTPDispatcher::RemoveAllListeners( void )
{
	m_vecListeners.PurgeAndDeleteElements();
}

void CServerLogHTTPDispatcher::DumpListenersToConsole( void ) const
{
	FOR_EACH_VEC( m_vecListeners, i )
	{
		CServerLogDestination *pDest = m_vecListeners[ i ];
		if ( pDest )
		{
			pDest->DumpStatusToConsole();
			ConMsg( "\tlast acknowledged tick %d\n", m_llLogPerTick[ pDest->LastUpdateIndex() ].iTick );
		}
	}
}

void CServerLogHTTPDispatcher::Shutdown()
{
	Reset();
	m_vecListeners.PurgeAndDeleteElements();
}

void CServerLogHTTPDispatcher::LevelInitPreEntity()
{
#if 0 // autoadd self for debugging
	if ( m_vecListeners.Count() == 0 )
		engine->ServerCommand( "log on;logaddress_add_http \"http://127.0.0.1:3000\"\n" );
#endif
	m_localTimeLoggingStart = Plat_GetTime();
	m_loggingStartPlatTime = Plat_FloatTime();
}

void CServerLogHTTPDispatcher::LevelShutdownPreEntity()
{
	Reset();
}

void CServerLogHTTPDispatcher::Reset( void )
{
	m_strServerLog.Clear();
	m_llLogPerTick.Purge();
	m_strLogLinesThisTick.Clear();

	FOR_EACH_VEC( m_vecListeners, i )
	{
		m_vecListeners[i]->Reset();
	}
#if defined DBGFLAG_ASSERT
	m_dbgLastAllocedHandle = LogLinesList_t::InvalidIndex();
#endif 

	m_localTimeLoggingStart = Plat_GetTime();
	m_loggingStartPlatTime = Plat_FloatTime();
}

void CServerLogHTTPDispatcher::PreClientUpdate()
{
	if ( !sv_log_http_record_before_any_listeners.GetBool() && m_vecListeners.Count() == 0 )
		return;

	int nTickCount = gpGlobals->tickcount;
	static ConVarRef cl_http_log_enable( "cl_http_log_enable", true );
	if ( cl_http_log_enable.IsValid() && cl_http_log_enable.GetBool() && !engine->IsDedicatedServer() )
	{	// Total hack: fake client ticks at 100 Hz for http logging
		nTickCount = (int) ( Plat_FloatTime() * 100 );
	}

	// If we've crossed a tick boundary, add the spew for that period to the in-memory log
	int32 iTickLastLogged = m_llLogPerTick.Tail() != LogLinesList_t::InvalidIndex() ? m_llLogPerTick [ m_llLogPerTick.Tail() ].iTick : 0;
	if ( nTickCount > iTickLastLogged && !m_strLogLinesThisTick.IsEmpty() )
	{
		// Record offset from start of our log string for this tick's lines and its length. 
		LogLinesList_t::IndexType_t newIdx = m_llLogPerTick.AddToTail( LogLineStartForTick_t ( nTickCount, m_strServerLog.Length(), m_strLogLinesThisTick.Length() ) );

		// Never reuse handles in this LL unless we tear down the log between levels or otherwise know there are no listeners holding on to them
		NOTE_UNUSED( newIdx );
#if defined DBGFLAG_ASSERT
		Assert( newIdx > m_dbgLastAllocedHandle || m_dbgLastAllocedHandle == LogLinesList_t::InvalidIndex() );
		m_dbgLastAllocedHandle = newIdx;
#endif

		// Append this tick's lines to the log and clear
		m_strServerLog.Append( m_strLogLinesThisTick.Get() );
		// Don't free memory for this as it's a frequently used bucket. Keep alloc at whatever high water mark its reached until level transition
		m_strLogLinesThisTick.SetLength( 0 ); 
	
		// Mark this as the most recent tick for any updates being sent below
		iTickLastLogged = nTickCount;
	}

	if ( m_vecListeners.Count() == 0 )
		return;

	// Can't do any work before we have steamhttp
	if ( !HelperGetSteamHttpInterfaceForLog() )
		return;

	AssertOnce( engine->IsLogEnabled() );
	static bool bWarnOnce = false;
	if ( !engine->IsLogEnabled() && !bWarnOnce )
	{
		Warning( "Server log http listener registered, but logging is not turned on for this server! Start recording the log before registering any http destinations\n" );
		bWarnOnce = true;
		return;
	}

	if ( m_llLogPerTick.Tail() != LogLinesList_t::InvalidIndex() )
	{
		FOR_EACH_VEC_BACK( m_vecListeners, i )
		{
			CServerLogDestination* pDest = m_vecListeners[ i ];
			if ( pDest->BRequestPending() ) // Skip requests still waiting to hear back or time out
				continue;

			if ( pDest->BIsFinished() ) // Clean up listeners no longer needing updates
			{
				delete m_vecListeners[ i ];
				m_vecListeners.Remove( i );
				continue;
			}

			// If we have new lines for this listener, send an update
			LogLinesList_t::IndexType_t idxLast = pDest->LastUpdateIndex();
			if ( idxLast == m_llLogPerTick.Tail() )
				continue; // they have the latest

			LogLinesList_t::IndexType_t idxFrom = LogLinesList_t::InvalidIndex();
			if ( idxLast == LogLinesList_t::InvalidIndex() )
				idxFrom = m_llLogPerTick.Head(); // never acknowledged an update
			else
				idxFrom = m_llLogPerTick.Next( idxLast ); // start with the next entry after their last acknowledgment 
			Assert( idxFrom != LogLinesList_t::InvalidIndex() );

			LogLineStartForTick_t &updateStart = m_llLogPerTick[ idxFrom ];
			Assert( updateStart.iTick <= iTickLastLogged ); 

			LogLinesList_t::IndexType_t idxTo = idxFrom;
			size_t unUpdateStringSize = updateStart.unLength;
			while ( unUpdateStringSize < pDest->MaxUpdateStringSizeBytes() && m_llLogPerTick.Next( idxTo ) != LogLinesList_t::InvalidIndex() )
			{
				idxTo = m_llLogPerTick.Next( idxTo );
				unUpdateStringSize += m_llLogPerTick[ idxTo ].unLength;
			}
			
			Assert( idxFrom != LogLinesList_t::InvalidIndex() && idxTo != LogLinesList_t::InvalidIndex() );
			pDest->SendUpdate( updateStart.iTick, m_llLogPerTick[idxTo].iTick, idxTo , m_strServerLog.Get() + updateStart.unOffsetStart, unUpdateStringSize );
		}
	}
}

bool CServerLogDestination::SendUpdate( int32 iFromTick, int32 iToTick, CServerLogHTTPDispatcher::LogLinesList_t::IndexType_t idxEndOfUpdate, const char* pLogLines, size_t unUpdateStrLenBytes )
{
	ISteamHTTP *pISteamHTTP = HelperGetSteamHttpInterfaceForLog();
	if ( !pISteamHTTP )
		return false;

	m_hHTTPRequestHandle = pISteamHTTP->CreateHTTPRequest( k_EHTTPMethodPOST, m_sURI.Get() );
	pISteamHTTP->SetHTTPRequestNetworkActivityTimeout( m_hHTTPRequestHandle, m_flTimeout );
	pISteamHTTP->SetHTTPRequestHeaderValue( m_hHTTPRequestHandle, "X-Tick-Start", CNumStr( iFromTick ).String() );
	pISteamHTTP->SetHTTPRequestHeaderValue( m_hHTTPRequestHandle, "X-Tick-End", CNumStr( iToTick ).String() );
	if ( steamgameserverapicontext && steamgameserverapicontext->SteamGameServer() )
		pISteamHTTP->SetHTTPRequestHeaderValue( m_hHTTPRequestHandle, "X-SteamID", steamgameserverapicontext->SteamGameServer()->GetSteamID().Render() );

	INetSupport::ServerInfo_t serverInfo;
	if ( INetSupport *pINetSupport = ( INetSupport * )g_pMatchFramework->GetMatchExtensions()->GetRegisteredExtensionInterface( INETSUPPORT_VERSION_STRING ) )
	{
		pINetSupport->GetServerInfo( &serverInfo );
		char szIPBuffer[ 32 ];
		serverInfo.m_netAdr.ToString_safe( szIPBuffer );
		pISteamHTTP->SetHTTPRequestHeaderValue( m_hHTTPRequestHandle, "X-Server-Addr", szIPBuffer );
	}
	CUtlString strTimestamp;
	GetServerLogHTTPDispatcher()->BuildTimestampString( strTimestamp );
	pISteamHTTP->SetHTTPRequestHeaderValue( m_hHTTPRequestHandle, "X-Timestamp", strTimestamp.Get() );
	if ( m_lluUniqueTokenFromSecret )
	{
		CNumStr strToken;
		strToken.SetHexUint64( m_lluUniqueTokenFromSecret );
		pISteamHTTP->SetHTTPRequestHeaderValue( m_hHTTPRequestHandle, "X-Server-Unique-Token", strToken.String() );
	}
	if ( m_lluUniqueTokenFromInstance )
	{
		CNumStr strToken;
		strToken.SetHexUint64( m_lluUniqueTokenFromInstance );
		pISteamHTTP->SetHTTPRequestHeaderValue( m_hHTTPRequestHandle, "X-Server-Instance-Token", strToken.String() );
	}
	pISteamHTTP->SetHTTPRequestRawPostBody( m_hHTTPRequestHandle, "text/plain", ( uint8* )pLogLines, unUpdateStrLenBytes );
	SteamAPICall_t hCall = NULL;
	if ( m_hHTTPRequestHandle && pISteamHTTP->SendHTTPRequest( m_hHTTPRequestHandle, &hCall ) && hCall )
	{
		m_CallbackOnHTTPRequestCompleted.Set( hCall, this, &CServerLogDestination::Steam_OnHTTPRequestCompleted );
		m_idxPendingUpdate = idxEndOfUpdate;
	}
	else
	{
		if ( m_hHTTPRequestHandle )
			pISteamHTTP->ReleaseHTTPRequest( m_hHTTPRequestHandle );
		m_hHTTPRequestHandle = NULL;

		Steam_OnHTTPRequestCompleted( NULL, true );
		return false;
	}

	return true;
}

void CServerLogDestination::DumpStatusToConsole( void ) const
{
	ConMsg( "%s - %s pending request\n", m_sURI.Get(), BRequestPending() ? "has" : "no" );
}

void CServerLogDestination::Steam_OnHTTPRequestCompleted( HTTPRequestCompleted_t *p, bool bError )
{
	if ( !m_hHTTPRequestHandle || !p || ( p->m_hRequest != m_hHTTPRequestHandle ) )
		return;

	bool bSuccess = !bError && p->m_eStatusCode == k_EHTTPStatusCode200OK;
	if ( bSuccess )
	{
		m_idxLastAcknowledgedUpdate = m_idxPendingUpdate;
	}
	else
	{
		switch ( p->m_eStatusCode )
		{
			case k_EHTTPStatusCodeInvalid:
			{
				Msg( "Internal HTTP error posting to url %s\n", m_sURI.Get() );
			}
			break;
			case k_EHTTPStatusCode410Gone:
			{
				Msg( "Listener %s sent code 410, removing from update list.\n", m_sURI.Get() );
				m_bIsFinished = true;
			}
			break;

			case k_EHTTPStatusCode205ResetContent:
				m_idxLastAcknowledgedUpdate = CServerLogHTTPDispatcher::LogLinesList_t::InvalidIndex(); // Destination is requesting entire log again
			break;

			default:
			{
				Msg( "Error posting to url %s, error code %d\n", m_sURI.Get(), p->m_eStatusCode );
			}
			break;
		}
	}

	m_idxPendingUpdate = CServerLogHTTPDispatcher::LogLinesList_t::InvalidIndex();
	if ( ISteamHTTP *pISteamHTTP = HelperGetSteamHttpInterfaceForLog() )
		pISteamHTTP->ReleaseHTTPRequest( p->m_hRequest );
	m_hHTTPRequestHandle = NULL;
}
