//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "cbase.h"
#include "steam_controller.h"
#include "voice_status.h"
#include "gc_clientsystem.h"
#include "gcsdk/gcclientjob.h"
#include "panorama/controls/panel2d.h"

#ifdef DOTA_DLL
#include "dota_gcmessages_msgid.pb.h"
#include "dota_gcmessages_client_chat.pb.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

DEFINE_PANORAMA_EVENT( DOTAViewSteamProfile );
DEFINE_PANORAMA_EVENT( DOTAAddSteamFriend );
DEFINE_PANORAMA_EVENT( DOTARemoveSteamFriend );
DEFINE_PANORAMA_EVENT( DOTAAcceptSteamFriendRequest );
DEFINE_PANORAMA_EVENT( DOTAIgnoreSteamFriendRequest );
DEFINE_PANORAMA_EVENT( DOTAEditAvatarImage );
DEFINE_PANORAMA_EVENT( DOTASteamChatWithFriend );
DEFINE_PANORAMA_EVENT( DOTAShowSteamVoiceSettings );
DEFINE_PANORAMA_EVENT( DOTAIgnoreUser );
DEFINE_PANORAMA_EVENT( DOTAUnignoreUser );
DEFINE_PANORAMA_EVENT( DOTAUserIgnoreStateChanged );
DEFINE_PANORAMA_EVENT( DOTASteamOverlayFinished );

using namespace GCSDK;

/*static*/ CSteamController &CSteamController::Get()
{
	static CSteamController s_controller;
	return s_controller;
}

CSteamController::CSteamController() :
	m_CallbackGameOverlayActivated( this, &CSteamController::OnGameOverlayActivated )
{
	m_bSteamOverlayActive = false;

	m_mapHTTPRequests.SetLessFunc( DefLessFunc( HTTPRequestHandle ) );
}

CSteamController::~CSteamController()
{
	panorama::UnregisterForUnhandledEvents( this );
}

void CSteamController::Initialize()
{
	RegisterForUnhandledEvent( DOTAViewSteamProfile(), this, &CSteamController::EventViewSteamProfile );
	RegisterForUnhandledEvent( DOTAAddSteamFriend(), this, &CSteamController::EventAddSteamFriend );
	RegisterForUnhandledEvent( DOTARemoveSteamFriend(), this, &CSteamController::EventRemoveSteamFriend );
	RegisterForUnhandledEvent( DOTAAcceptSteamFriendRequest(), this, &CSteamController::EventAcceptSteamFriendRequest );
	RegisterForUnhandledEvent( DOTAIgnoreSteamFriendRequest(), this, &CSteamController::EventIgnoreSteamFriendRequest );
	RegisterForUnhandledEvent( DOTAEditAvatarImage(), this, &CSteamController::EventEditAvatarImage );
	RegisterForUnhandledEvent( DOTASteamChatWithFriend(), this, &CSteamController::EventSteamChatWithFriend );
	RegisterForUnhandledEvent( DOTAShowSteamVoiceSettings(), this, &CSteamController::EventDOTAShowSteamVoiceSettings );
	RegisterForUnhandledEvent( DOTAIgnoreUser(), this, &CSteamController::EventIgnoreUser );
	RegisterForUnhandledEvent( DOTAUnignoreUser(), this, &CSteamController::EventUnignoreUser );
}

void CSteamController::OnGameOverlayActivated( GameOverlayActivated_t *pGameOverlayActivated )
{
	m_bSteamOverlayActive = ( pGameOverlayActivated->m_bActive != 0 );
}


void CSteamController::ViewSteamProfile( const CSteamID &steamID )
{
	ISteamFriends *pFriends = steamapicontext ? steamapicontext->SteamFriends() : NULL;
	if ( !pFriends )
		return;

	pFriends->ActivateGameOverlayToUser( "steamid", steamID );
}

void CSteamController::AddSteamFriend( const CSteamID &steamID )
{
	ISteamFriends *pFriends = steamapicontext ? steamapicontext->SteamFriends() : NULL;
	if ( !pFriends )
		return;

	pFriends->ActivateGameOverlayToUser( "friendadd", steamID );
}

void CSteamController::RemoveSteamFriend( const CSteamID &steamID )
{
	ISteamFriends *pFriends = steamapicontext ? steamapicontext->SteamFriends() : NULL;
	if ( !pFriends )
		return;

	pFriends->ActivateGameOverlayToUser( "friendremove", steamID );
}

void CSteamController::AcceptSteamFriendRequest( const CSteamID &steamID )
{
	ISteamFriends *pFriends = steamapicontext ? steamapicontext->SteamFriends() : NULL;
	if ( !pFriends )
		return;

	pFriends->ActivateGameOverlayToUser( "friendrequestaccept", steamID );
}

void CSteamController::IgnoreSteamFriendRequest( const CSteamID &steamID )
{
	ISteamFriends *pFriends = steamapicontext ? steamapicontext->SteamFriends() : NULL;
	if ( !pFriends )
		return;

	pFriends->ActivateGameOverlayToUser( "friendrequestignore", steamID );
}

void CSteamController::EditAvatarImage( const CSteamID &steamID )
{
	ISteamFriends *pFriends = steamapicontext ? steamapicontext->SteamFriends() : NULL;
	if ( !pFriends )
		return;

	pFriends->ActivateGameOverlayToUser( "MinimalProfileEdit", steamID );
}

void CSteamController::SteamChatWithFriend( const CSteamID &steamID )
{
	ISteamFriends *pFriends = steamapicontext ? steamapicontext->SteamFriends() : NULL;
	if ( !pFriends )
		return;

	pFriends->ActivateGameOverlayToUser( "chat", steamID );
}

void CSteamController::IgnoreUser( const CSteamID &steamID )
{
#ifdef DOTA_DLL
	// Inform the GC about this ignore so we can possibly track it for
	// reporting/spam detection purposes.
	GCSDK::CProtoBufMsg<CMsgDOTAClientIgnoredUser> msg( k_EMsgGCClientIgnoredUser );
	msg.Body().set_ignored_account_id( steamID.GetAccountID() );
	GCClientSystem()->BSendMessage( msg );
#else
	// FIXME: Make this mod-independent
	Assert( false );
#endif

	CVoiceStatus *pVoiceStatus = GetClientVoiceMgr();
	if ( pVoiceStatus )
	{
		pVoiceStatus->SetPlayerBlockedState( steamID.ConvertToUint64(), true, true );
		panorama::DispatchEvent( DOTAUserIgnoreStateChanged(), nullptr, steamID.ConvertToUint64() );
	}
}

void CSteamController::UnignoreUser( const CSteamID &steamID )
{
	CVoiceStatus *pVoiceStatus = GetClientVoiceMgr();
	if ( pVoiceStatus )
	{
		pVoiceStatus->SetPlayerBlockedState( steamID.ConvertToUint64(), false, false );
		panorama::DispatchEvent( DOTAUserIgnoreStateChanged(), nullptr, steamID.ConvertToUint64() );
	}
}

void CSteamController::ShowSteamVoiceSettings()
{
	ISteamFriends *pFriends = steamapicontext ? steamapicontext->SteamFriends() : NULL;
	if ( !pFriends )
		return;
	pFriends->ActivateGameOverlay( "Settings" );
}

bool CSteamController::EventViewSteamProfile( uint64 unSteamID )
{
	ViewSteamProfile( CSteamID( unSteamID ) );
	return true;
}

bool CSteamController::EventAddSteamFriend( uint64 unSteamID )
{
	AddSteamFriend( CSteamID( unSteamID ) );
	return true;
}

bool CSteamController::EventRemoveSteamFriend( uint64 unSteamID )
{
	RemoveSteamFriend( CSteamID( unSteamID ) );
	return true;
}

bool CSteamController::EventAcceptSteamFriendRequest( uint64 unSteamID )
{
	AcceptSteamFriendRequest( CSteamID( unSteamID ) );
	return true;
}

bool CSteamController::EventIgnoreSteamFriendRequest( uint64 unSteamID )
{
	IgnoreSteamFriendRequest( CSteamID( unSteamID ) );
	return true;
}

bool CSteamController::EventEditAvatarImage( uint64 unSteamID )
{
	EditAvatarImage( CSteamID( unSteamID ) );
	return true;
}

bool CSteamController::EventSteamChatWithFriend( uint64 unSteamID )
{
	SteamChatWithFriend( CSteamID( unSteamID ) );
	return true;
}

bool CSteamController::EventIgnoreUser( uint64 unSteamID )
{
	IgnoreUser( CSteamID( unSteamID ) );
	return true;
}

bool CSteamController::EventUnignoreUser( uint64 unSteamID )
{
	UnignoreUser( CSteamID( unSteamID ) );
	return true;
}

bool CSteamController::EventDOTAShowSteamVoiceSettings()
{
	ShowSteamVoiceSettings();
	return true;
}

// This will return when the steam overlay is dismissed, posting the requested event on completion
void CSteamController::ActivateSteamOverlayToWebPage( const char *pURL, panorama::CPanelPtr< panorama::CPanel2D > hCompletionTarget )
{
	// Need to copy the string since it's not 
	CUtlString url( pURL );
	StartLambdaJob( "CSteamController::ActivateSteamOverlayToWebPage", [url, hCompletionTarget]()
	{
		bool bOk = CSteamController::Get().BYldActivateSteamOverlayToWebPage( url.Get() );

		panorama::CPanel2D *pTarget = hCompletionTarget.Get();
		if ( pTarget )
		{
			DispatchEventAsync( 0.0f, DOTASteamOverlayFinished(), pTarget, bOk );
		}
	} );
}

// Waits for the steam overlay to appear, or to disappear.
bool CSteamController::BYldWaitForSteamOverlay( WaitForSteamOverlayMode_t mode, int nTimeoutSec )
{
	if ( !steamapicontext->SteamUtils()->IsOverlayEnabled() && ( mode == WAIT_FOR_OVERLAY_TO_APPEAR ) )
		return false;

	bool bWantsActive = ( mode == WAIT_FOR_OVERLAY_TO_APPEAR );

	GClientJobCur().SetJobTimeout( nTimeoutSec );
	while ( true )
	{
		if ( IsSteamOverlayActive() == bWantsActive )
			break;

		if ( !GCSDK::GJobCur().BYieldingWaitOneFrame() )
			return false;
	}

	return true;
}

// Returns false if we timed out, true otherwise
bool CSteamController::BYldWaitForSteamOverlayToAppearAndBeDismissed( int nAppearTimeoutSec, int nDismissTimeoutSec )
{
	if ( !steamapicontext->SteamUtils()->IsOverlayEnabled() )
		return false;

	bool bStartedWithOverlayActive = IsSteamOverlayActive();
	if ( !bStartedWithOverlayActive )
	{
		if ( !BYldWaitForSteamOverlay( WAIT_FOR_OVERLAY_TO_APPEAR, nAppearTimeoutSec ) )
			return false;
	}
	
	// Wait basically forever for the overlay to be dismissed
	return BYldWaitForSteamOverlay( WAIT_FOR_OVERLAY_TO_BE_DISMISSED, nDismissTimeoutSec );
}

// This will return when the steam overlay is dismissed.
bool CSteamController::BYldActivateSteamOverlayToWebPage( const char *pURL )
{
	steamapicontext->SteamFriends()->ActivateGameOverlayToWebPage( pURL );
	return BYldWaitForSteamOverlayToAppearAndBeDismissed( 2, 10000 );
}

// This will return when the http request has completed
EHTTPStatusCode CSteamController::BYldHTTPRequest( ISteamHTTP *pHTTP, HTTPRequestHandle hRequest, uint64 ulContext )
{
	AddHTTPRequest( pHTTP, hRequest, ulContext );
	SubmitHTTPRequest( pHTTP, hRequest );

	while ( GetHTTPRequestResponseStatus( hRequest ) == HTTP_RESPONSE_NOT_RESPONDED )
	{
		// Wait as long as we haven't timed out
		if ( !GClientJobCur().BYieldingWaitOneFrame() )
			break;
	}

	EHTTPStatusCode eStatusCode = GetHTTPRequestResult( hRequest );

	CleanupHTTPRequest( pHTTP, hRequest );

	return eStatusCode;
}

void CSteamController::OnHTTPRequestCompleted( HTTPRequestCompleted_t *pResult, bool bError )
{
	int iRequest = m_mapHTTPRequests.Find( pResult->m_hRequest );
	if ( iRequest == m_mapHTTPRequests.InvalidIndex() )
		return;

	m_mapHTTPRequests[iRequest].m_nResponseStatus = ( bError || !pResult->m_bRequestSuccessful ) ? HTTP_RESPONSE_FAILED : HTTP_RESPONSE_RECEIVED;
	m_mapHTTPRequests[iRequest].m_hStatusCode = pResult->m_eStatusCode;

	Assert( m_mapHTTPRequests[iRequest].m_ulContext == pResult->m_ulContextValue );
}

void CSteamController::AddHTTPRequest( ISteamHTTP *pHTTP, HTTPRequestHandle hRequest, uint64 ulContext )
{
	int iRequest = m_mapHTTPRequests.Insert( hRequest );

	PendingHTTPRequest_t &pendingRequest = m_mapHTTPRequests[iRequest];
	pendingRequest.m_nResponseStatus = HTTP_RESPONSE_NOT_RESPONDED;
	pendingRequest.m_hStatusCode = k_EHTTPStatusCodeInvalid;
	pendingRequest.m_ulContext = ulContext;

	pHTTP->SetHTTPRequestContextValue( hRequest, ulContext );
}

void CSteamController::SubmitHTTPRequest( ISteamHTTP *pHTTP, HTTPRequestHandle hRequest )
{
	int iRequest = m_mapHTTPRequests.Find( hRequest );
	if ( iRequest == m_mapHTTPRequests.InvalidIndex() )
		return;

	SteamAPICall_t hCall;
	pHTTP->SendHTTPRequest( hRequest, &hCall );

	m_mapHTTPRequests[iRequest].m_pCallback = new CCallResult< CSteamController, HTTPRequestCompleted_t > ;
	m_mapHTTPRequests[iRequest].m_pCallback->Set( hCall, this, &CSteamController::OnHTTPRequestCompleted );
}

CSteamController::HTTPResponseStatus_t CSteamController::GetHTTPRequestResponseStatus( HTTPRequestHandle hRequest )
{
	int iRequest = m_mapHTTPRequests.Find( hRequest );
	if ( iRequest == m_mapHTTPRequests.InvalidIndex() )
		return HTTP_RESPONSE_UNKNOWN;

	return m_mapHTTPRequests[iRequest].m_nResponseStatus;
}

EHTTPStatusCode CSteamController::GetHTTPRequestResult( HTTPRequestHandle hRequest, uint64 *pContext )
{
	int iRequest = m_mapHTTPRequests.Find( hRequest );
	if ( iRequest == m_mapHTTPRequests.InvalidIndex() )
	{
		if ( pContext )
		{
			*pContext = 0;
		}
		return k_EHTTPStatusCodeInvalid;
	}

	if ( pContext )
	{
		*pContext = m_mapHTTPRequests[iRequest].m_ulContext;
	}

	bool bFailed = ( m_mapHTTPRequests[iRequest].m_nResponseStatus == HTTP_RESPONSE_FAILED );
	EHTTPStatusCode result = bFailed ? k_EHTTPStatusCodeInvalid : m_mapHTTPRequests[iRequest].m_hStatusCode;

	return result;
}

void CSteamController::CleanupHTTPRequest( ISteamHTTP *pHTTP, HTTPRequestHandle hRequest )
{
	int iRequest = m_mapHTTPRequests.Find( hRequest );
	if ( iRequest != m_mapHTTPRequests.InvalidIndex() )
	{
		delete m_mapHTTPRequests[iRequest].m_pCallback;
		m_mapHTTPRequests.RemoveAt( iRequest );
	}
}

bool CSteamController::BYldHTTPGetFileSize( const char *pszURL, bool bBreakCache, uint32 *pOutFileSize )
{
	*pOutFileSize = 0;
	
	ISteamHTTP *pHTTP = steamapicontext ? steamapicontext->SteamHTTP() : nullptr;
	if ( !pHTTP )
		return false;

	HTTPRequestHandle hRequest = pHTTP->CreateHTTPRequest( k_EHTTPMethodHEAD, pszURL );
	if ( hRequest == INVALID_HTTPREQUEST_HANDLE )
		return false;

	if ( bBreakCache )
	{
		UniqueId_t tmpGuid;
		CreateUniqueId( &tmpGuid );
		char tmpStr[64];
		UniqueIdToString( tmpGuid, tmpStr, sizeof( tmpStr ) );
		pHTTP->SetHTTPRequestHeaderValue( hRequest, "Cache-Control", "no-cache, no-store" );
		pHTTP->SetHTTPRequestGetOrPostParameter( hRequest, "v", tmpStr );
	}

	#define ReleaseHTTPRequestAndFalse()	( pHTTP->ReleaseHTTPRequest( hRequest ), false )

	EHTTPStatusCode result = CSteamController::Get().BYldHTTPRequest( pHTTP, hRequest );
	if ( result != k_EHTTPStatusCode200OK )
		return ReleaseHTTPRequestAndFalse();

	uint32 unResponseHeaderSize;
	if ( !pHTTP->GetHTTPResponseHeaderSize( hRequest, "Content-Length", &unResponseHeaderSize ) )
		return ReleaseHTTPRequestAndFalse();

	CUtlVector<uint8> vecHeaderValueBytes;
	vecHeaderValueBytes.EnsureCount( unResponseHeaderSize );
	if ( !pHTTP->GetHTTPResponseHeaderValue( hRequest, "Content-Length", vecHeaderValueBytes.Base(), unResponseHeaderSize ) )
		return ReleaseHTTPRequestAndFalse();

	int nContentLengthParsed = V_atoi( (const char *)vecHeaderValueBytes.Base() );
	if ( nContentLengthParsed < 0 )
		return ReleaseHTTPRequestAndFalse();

	// Success! We found the file and it has a size.
	*pOutFileSize = nContentLengthParsed;
	return true;
}

bool CSteamController::BYldHTTPDownloadFileToBuffer( const char *pszURL, CUtlBuffer *pOutBuffer )
{
	pOutBuffer->Clear();
	pOutBuffer->SetBufferType( false, false );

	ISteamHTTP *pHTTP = steamapicontext ? steamapicontext->SteamHTTP() : nullptr;
	if ( !pHTTP )
		return false;

	HTTPRequestHandle hRequest = pHTTP->CreateHTTPRequest( k_EHTTPMethodGET, pszURL );

	#define ReleaseHTTPRequestAndFalse()	( pHTTP->ReleaseHTTPRequest( hRequest ), false )

	EHTTPStatusCode result = BYldHTTPRequest( pHTTP, hRequest, 0 );
	if ( result != k_EHTTPStatusCode200OK )
		return ReleaseHTTPRequestAndFalse();

	uint32 unBodySize;
	if ( !pHTTP->GetHTTPResponseBodySize( hRequest, &unBodySize ) )
		return ReleaseHTTPRequestAndFalse();

	pOutBuffer->SeekPut( CUtlBuffer::SEEK_HEAD, unBodySize );
	if ( !pHTTP->GetHTTPResponseBodyData( hRequest, (uint8 *)pOutBuffer->Base(), pOutBuffer->TellPut() ) )
		return ReleaseHTTPRequestAndFalse();

	return true;
}


const char *CSteamController::YldGetPersonaName( const CSteamID &steamID )
{
	while ( steamapicontext->SteamFriends()->RequestUserInformation( steamID, true /* name only */ ) )
	{
		GClientJobCur().BYieldingWaitOneFrame();
	}

	return steamapicontext->SteamFriends()->GetFriendPersonaName( steamID );
}


//-----------------------------------------------------------------------------

CSteamBatchedHTTPRequests::CSteamBatchedHTTPRequests( ISteamHTTP *pHTTP ) : 
	m_pHTTP( pHTTP ),
	m_bError( false )
{	
}

CSteamBatchedHTTPRequests::~CSteamBatchedHTTPRequests()
{
	for ( HTTPRequestHandle hRequest : m_vecPendingRequests )
	{
		m_pHTTP->ReleaseHTTPRequest( hRequest );
		CSteamController::Get().CleanupHTTPRequest( m_pHTTP, hRequest );
	}

	for ( HTTPRequestResponse_t response : m_vecCompletedRequests )
	{
		m_pHTTP->ReleaseHTTPRequest( response.hRequestHandle );
		CSteamController::Get().CleanupHTTPRequest( m_pHTTP, response.hRequestHandle );
	}
}

HTTPRequestHandle CSteamBatchedHTTPRequests::AddHTTPRequest(const char *pszURL, uint64 ulContext )
{
	HTTPRequestHandle hRequest = m_pHTTP->CreateHTTPRequest( k_EHTTPMethodGET, pszURL );
	m_vecWaitingRequests.AddToTail( hRequest );

	CSteamController::Get().AddHTTPRequest( m_pHTTP, hRequest, ulContext );

	return hRequest;
}

bool CSteamBatchedHTTPRequests::BYldSubmitRequests( int nMaxSimultaneous, CUtlVector<HTTPRequestResponse_t> *pVecRequests, const char *pszProgressEvent )
{
	while ( !m_bError && ( ( m_vecWaitingRequests.Count() > 0 ) || ( m_vecPendingRequests.Count() > 0 ) ) )
	{
		// Check for any completed pending requests
		for ( int iPendingRequest = 0; iPendingRequest < m_vecPendingRequests.Count(); iPendingRequest++ )
		{
			HTTPRequestHandle hPendingRequest = m_vecPendingRequests[iPendingRequest];

			CSteamController::HTTPResponseStatus_t eStatus = CSteamController::Get().GetHTTPRequestResponseStatus( hPendingRequest );
			if ( eStatus == CSteamController::HTTP_RESPONSE_RECEIVED )
			{
				uint64 ulContext;
				EHTTPStatusCode eStatusCode = CSteamController::Get().GetHTTPRequestResult( hPendingRequest, &ulContext );
				if ( ( eStatusCode == k_EHTTPStatusCode200OK ) || ( eStatusCode == k_EHTTPStatusCode206PartialContent ) )
				{
					int iCompleted = m_vecCompletedRequests.AddToTail();
					m_vecCompletedRequests[iCompleted].hRequestHandle = hPendingRequest;
					m_vecCompletedRequests[iCompleted].ulContext = ulContext;

					m_vecPendingRequests.Remove( iPendingRequest );
					iPendingRequest--;
				}
				else
				{
					m_bError = true;
					break;
				}
			}
			else if ( eStatus == CSteamController::HTTP_RESPONSE_FAILED )
			{
				m_bError = true;
				break;
			}
		}
		
		// Check if we can start up more work
		while ( ( m_vecWaitingRequests.Count() > 0 ) && ( ( nMaxSimultaneous == 0 ) || ( m_vecPendingRequests.Count() < nMaxSimultaneous ) ) )
		{
			CSteamController::Get().SubmitHTTPRequest( m_pHTTP, m_vecWaitingRequests[0] );

			m_vecPendingRequests.AddToTail( m_vecWaitingRequests[0] );
		
			m_vecWaitingRequests.Remove( 0 );
		}

		// Notify UI
		if ( pszProgressEvent )
		{
			char pszEventBuffer[128];
			float fPercent = (float)( m_vecCompletedRequests.Count() * 100.0f ) / (float)( m_vecWaitingRequests.Count() + m_vecPendingRequests.Count() + m_vecCompletedRequests.Count() );
			V_sprintf_safe( pszEventBuffer, pszProgressEvent, fPercent );
			const char *pszEventBufferEnd = pszEventBuffer;

			panorama::UIEngine()->DispatchEvent( panorama::UIEngine()->CreateEventFromString( nullptr, pszEventBufferEnd, &pszEventBufferEnd ) );
		}

		// Wait a frame and check for timeout
		if ( !GClientJobCur().BYieldingWaitOneFrame() )
		{
			m_bError = true;
		}
	}

	pVecRequests->RemoveAll();
	pVecRequests->AddVectorToTail( m_vecCompletedRequests );

	return !m_bError;
}
