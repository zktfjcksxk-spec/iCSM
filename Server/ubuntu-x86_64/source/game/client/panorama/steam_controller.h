//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: No, not THAT steam controller.
//=============================================================================//
#pragma once

#include "panorama/uievent.h"
#include "steam/steam_api.h"

DECLARE_PANORAMA_EVENT1( DOTAViewSteamProfile, uint64 );
DECLARE_PANORAMA_EVENT1( DOTAAddSteamFriend, uint64 );
DECLARE_PANORAMA_EVENT1( DOTARemoveSteamFriend, uint64 );
DECLARE_PANORAMA_EVENT1( DOTAAcceptSteamFriendRequest, uint64 );
DECLARE_PANORAMA_EVENT1( DOTAIgnoreSteamFriendRequest, uint64 );
DECLARE_PANORAMA_EVENT1( DOTAEditAvatarImage, uint64 );
DECLARE_PANORAMA_EVENT1( DOTASteamChatWithFriend, uint64 );
DECLARE_PANORAMA_EVENT0( DOTAShowSteamVoiceSettings );
DECLARE_PANORAMA_EVENT1( DOTAIgnoreUser, uint64 );
DECLARE_PANORAMA_EVENT1( DOTAUnignoreUser, uint64 );
DECLARE_PANORAMA_EVENT1( DOTAUserIgnoreStateChanged, uint64 );
DECLARE_PANORAMA_EVENT1( DOTASteamOverlayFinished, bool );	// Returns false if the overlay failed to come up

enum WaitForSteamOverlayMode_t
{
	WAIT_FOR_OVERLAY_TO_APPEAR = 0,
	WAIT_FOR_OVERLAY_TO_BE_DISMISSED
};

//-----------------------------------------------------------------------------
// Purpose: Interfaces between steam functionality (overlay, profiles, chat,
// etc) and DOTA's panorama UI
//-----------------------------------------------------------------------------
class CSteamController
{
public:
	CSteamController();
	~CSteamController();

	static CSteamController &Get();

	void Initialize();

	// These methods are expected to be called from the main thread
public:
	void ViewSteamProfile( const CSteamID &steamID );
	void AddSteamFriend( const CSteamID &steamID );
	void RemoveSteamFriend( const CSteamID &steamID );
	void AcceptSteamFriendRequest( const CSteamID &steamID );
	void IgnoreSteamFriendRequest( const CSteamID &steamID );
	void EditAvatarImage( const CSteamID &steamID );
	void SteamChatWithFriend( const CSteamID &steamID );
	void IgnoreUser( const CSteamID &steamID );
	void UnignoreUser( const CSteamID &steamID );
	void ShowSteamVoiceSettings();

	// Is the steam overlay currently visible?
	bool IsSteamOverlayActive() const { return m_bSteamOverlayActive;  }

	// This will return when the steam overlay is dismissed,
	// posting DOTASteamOverlayFinished when the steam overlay is dismissed.
	void ActivateSteamOverlayToWebPage( const char *pURL, panorama::CPanelPtr< panorama::CPanel2D > hCompletionTarget = panorama::CPanelPtr< panorama::CPanel2D >() );

	// These methods are expected to be called from jobs. 
public:
	// This waits for the steam overlay to either appear, or be dismissed.
	// Returns false if we timed out, true otherwise
	bool BYldWaitForSteamOverlay( WaitForSteamOverlayMode_t mode, int nTimeoutSec = 0 );

	// This waits for the steam overlay to appear, then waits for it to be dismissed.
	// If the steam overlay is already visible, it will only wait for it to be dismissed
	// Returns false if we timed out, true otherwise
	bool BYldWaitForSteamOverlayToAppearAndBeDismissed( int nAppearTimeoutSec = 0, int nDismissTimeoutSec = 0 );

	// This will return when the steam overlay is dismissed.
	bool BYldActivateSteamOverlayToWebPage( const char *pURL );

	// This will return when the HTTP request has completed or failed or timed out
	EHTTPStatusCode BYldHTTPRequest( ISteamHTTP *pHTTP, HTTPRequestHandle hRequest, uint64 ulContext = 0 );

	// This will return when the HTTP request has completed or failed or timed out
	bool BYldHTTPGetFileSize( const char *pszURL, bool bBreakCache, uint32 *pOutFileSize );

	// This will return when the HTTP request has completed or failed or timed out
	bool BYldHTTPDownloadFileToBuffer( const char *pszURL, CUtlBuffer *pOutBuffer );

	// This will return when the steam call to get the user's persona name returns, or immediately if we have it available
	const char *YldGetPersonaName( const CSteamID &steamID );

	enum HTTPResponseStatus_t
	{
		HTTP_RESPONSE_FAILED,
		HTTP_RESPONSE_RECEIVED,
		HTTP_RESPONSE_NOT_RESPONDED,
		HTTP_RESPONSE_UNKNOWN,
	};


private:
	bool EventViewSteamProfile( uint64 unSteamID );
	bool EventAddSteamFriend( uint64 unSteamID );
	bool EventRemoveSteamFriend( uint64 unSteamID );
	bool EventAcceptSteamFriendRequest( uint64 unSteamID );
	bool EventIgnoreSteamFriendRequest( uint64 unSteamID );
	bool EventEditAvatarImage( uint64 unSteamID );
	bool EventSteamChatWithFriend( uint64 unSteamID );
	bool EventIgnoreUser( uint64 unSteamID );
	bool EventUnignoreUser( uint64 unSteamID );
	bool EventDOTAShowSteamVoiceSettings();

	STEAM_CALLBACK( CSteamController, OnGameOverlayActivated, GameOverlayActivated_t, m_CallbackGameOverlayActivated );
	void OnHTTPRequestCompleted( HTTPRequestCompleted_t *pResult, bool bError );


	// For use with CSteamBatchedHTTPRequests
	void AddHTTPRequest( ISteamHTTP *pHTTP, HTTPRequestHandle hRequest, uint64 ulContext );
	void SubmitHTTPRequest( ISteamHTTP *pHTTP, HTTPRequestHandle hRequest );
	HTTPResponseStatus_t GetHTTPRequestResponseStatus( HTTPRequestHandle hRequest );
	EHTTPStatusCode GetHTTPRequestResult( HTTPRequestHandle hRequest, uint64 *pContext = nullptr );
	void CleanupHTTPRequest( ISteamHTTP *pHTTP, HTTPRequestHandle hRequest ); 

	friend class CSteamBatchedHTTPRequests;


private:

	struct PendingHTTPRequest_t 
	{
		HTTPResponseStatus_t m_nResponseStatus;
		EHTTPStatusCode m_hStatusCode;
		uint64 m_ulContext;
		CCallResult<CSteamController, HTTPRequestCompleted_t> *m_pCallback;
	};

	CUtlMap<HTTPRequestHandle, PendingHTTPRequest_t> m_mapHTTPRequests;

	bool m_bSteamOverlayActive;
};

//-----------------------------------------------------------------------------
class CSteamBatchedHTTPRequests
{
public:
	CSteamBatchedHTTPRequests( ISteamHTTP *pHTTP );
	virtual ~CSteamBatchedHTTPRequests();

	struct HTTPRequestResponse_t 
	{
		HTTPRequestHandle hRequestHandle;
		uint64 ulContext;
	};

	// Add a request to be batch-executed
	HTTPRequestHandle AddHTTPRequest( const char *pszURL, uint64 ulContext );

	// Execute all the requests and yield until they're done or there's an error.
	//   pVecRespones contains all the handles and that were processed and their contexts
	//   pszProgressEvent is an optional panorama event string that contains a %f that will be called with percentage progress
	bool BYldSubmitRequests( int nMaxSimultaneous, CUtlVector<HTTPRequestResponse_t> *pVecResponses, const char *pszProgressEvent );

private:

	ISteamHTTP *m_pHTTP;

	CUtlVector<HTTPRequestHandle> m_vecWaitingRequests;
	CUtlVector<HTTPRequestHandle> m_vecPendingRequests;
	CUtlVector<HTTPRequestResponse_t> m_vecCompletedRequests;
	bool m_bError;
};



// Simple smart pointer class to automatically release HTTPRequestHandles on destruction
class CAutoHTTPRequestHandle
{
public:
	CAutoHTTPRequestHandle() : m_hRequest( INVALID_HTTPREQUEST_HANDLE ) {}
	CAutoHTTPRequestHandle( HTTPRequestHandle hRequest ) : m_hRequest( hRequest ) {}

	~CAutoHTTPRequestHandle()
	{
		Release();
	}

	void Attach( HTTPRequestHandle hRequest )
	{
		Release();

		m_hRequest = hRequest;
	}

	HTTPRequestHandle Detach()
	{
		HTTPRequestHandle hRequest = m_hRequest;
		m_hRequest = INVALID_HTTPREQUEST_HANDLE;
		return hRequest;
	}

	void Release()
	{
		if ( m_hRequest == INVALID_HTTPREQUEST_HANDLE )
			return;

		ISteamHTTP *pHTTP = steamapicontext ? steamapicontext->SteamHTTP() : nullptr;
		if ( !pHTTP )
			return;

		pHTTP->ReleaseHTTPRequest( m_hRequest );
		m_hRequest = INVALID_HTTPREQUEST_HANDLE;
	}

	operator HTTPRequestHandle() const
	{
		return m_hRequest;
	}

	CAutoHTTPRequestHandle & operator =( HTTPRequestHandle hRequest )
	{
		Release();
		Attach( hRequest );
		return *this;
	}

private:
	HTTPRequestHandle m_hRequest;
};