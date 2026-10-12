//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: Panorama specific video player code
//=============================================================================//

#include "stdafx.h"
#if defined(SOURCE2_PANORAMA)
#include "soundsystem/isoundsystem.h"
#else
#ifdef SUPPORTS_AUDIO
#include "audio/iaudiointerface.h"
#endif
#endif
#include "data/panoramavideoplayer.h"
#include "uifileresource.h"
#include "panorama/iuiengine.h"
#include "panorama/iuisoundsystem.h"

#ifndef PANORAMA_USE_S1WRAPPER
#include "tier0/fibertools.h"
#endif


// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

DECLARE_PANORAMA_EVENT3( VideoPlayerInitAudio, CVideoPlayerAudioRenderer*, int, int );
DEFINE_PANORAMA_EVENT( VideoPlayerInitAudio );

DECLARE_PANORAMA_EVENT1( VideoPlayerFreeAudio, CVideoPlayerAudioRenderer* );
DEFINE_PANORAMA_EVENT( VideoPlayerFreeAudio );

DECLARE_PANORAMA_EVENT2( DispatchVideoPlayerEvent, CVideoPlayerEventDispatcher*, EVideoPlayerEvent );
DEFINE_PANORAMA_EVENT( DispatchVideoPlayerEvent );


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CVideoPlayerVideoRenderer::CVideoPlayerVideoRenderer( IUIRenderDevice *pDevice )
:
	m_pYUV420DoubleBufferedTexture( nullptr )
{
	m_pDevice = pDevice;
	
	IUIDoubleBufferedYUV420Texture *pTexture = nullptr;
	if ( m_pDevice->BCreateDoubleBufferedYUV420Texture( &pTexture ) )
	{
		m_pYUV420DoubleBufferedTexture = pTexture;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CVideoPlayerVideoRenderer::~CVideoPlayerVideoRenderer()
{	
	m_pDevice = NULL;
	if ( m_pYUV420DoubleBufferedTexture )
	{
		m_pYUV420DoubleBufferedTexture->ClearCurrentTextureHandles();
		m_pYUV420DoubleBufferedTexture->Release();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Presents given video frame
//-----------------------------------------------------------------------------
bool CVideoPlayerVideoRenderer::BPresentYUV420Texture( uint nWidth, uint nHeight, void *pPlaneY, void *pPlaneU, void *pPlaneV, uint unStrideY, uint unStrideU, uint unStrideV )
{
#ifndef PANORAMA_USE_S1WRAPPER
	CThreadFiber_Temporary tempFiberScope;
#endif
	return m_pYUV420DoubleBufferedTexture->BUpdateTextureData( nWidth, nHeight, pPlaneY, pPlaneU, pPlaneV, unStrideY, unStrideU, unStrideV );
}


//-----------------------------------------------------------------------------
// Purpose: Validation
//-----------------------------------------------------------------------------
#ifdef DBGFLAG_VALIDATE
void CVideoPlayerVideoRenderer::Validate( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE();
	validator.ClaimMemory( m_pYUV420DoubleBufferedTexture );
}
#endif 


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CVideoPlayerAudioRenderer::CVideoPlayerAudioRenderer()
{
	m_flVolume = UISoundSystem()->GetSoundVolume( k_ESoundType_Movies );
	m_pAudioStream = NULL;
#if defined(SOURCE2_PANORAMA)
	m_nChannels = 0;
	m_nSampleRate = 44100;
#endif
	RegisterForUnhandledEvent( VideoPlayerInitAudio(), this, &CVideoPlayerAudioRenderer::OnInitAudioMainThread );
	RegisterForUnhandledEvent( VideoPlayerFreeAudio(), this, &CVideoPlayerAudioRenderer::OnFreeAudioMainThread );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CVideoPlayerAudioRenderer::~CVideoPlayerAudioRenderer()
{
	UnregisterForUnhandledEvent( VideoPlayerInitAudio(), this, &CVideoPlayerAudioRenderer::OnInitAudioMainThread );
	UnregisterForUnhandledEvent( VideoPlayerFreeAudio(), this, &CVideoPlayerAudioRenderer::OnFreeAudioMainThread );

	OnFreeAudioMainThread( this );
}


//-----------------------------------------------------------------------------
// Purpose: Sets the movie playback audio volume
//-----------------------------------------------------------------------------
void CVideoPlayerAudioRenderer::SetPlaybackVolume( float flVolume )
{
	m_flVolume = flVolume;
	if ( m_pAudioStream )
		m_pAudioStream->SetVolume( flVolume );
}


//-----------------------------------------------------------------------------
// Purpose: Called to initialize audio output
//-----------------------------------------------------------------------------
bool CVideoPlayerAudioRenderer::InitAudioOutput( int nSampleRate, int nChannels )
{
	// to avoid a deadlock where decode thread calls this function while main thread is waiting for shutdown,
	// exit here and let main thread close output after join
	if ( m_bShuttingDown )
		return false;

	UIEngine()->DispatchEventAsync( 0.0f, VideoPlayerInitAudio::MakeEvent( NULL, this, nSampleRate, nChannels ) );
	m_eventWait.Wait();

	return (m_pAudioStream != NULL);
}


//-----------------------------------------------------------------------------
// Purpose: Handles UI event for initializing audio output
//-----------------------------------------------------------------------------
bool CVideoPlayerAudioRenderer::OnInitAudioMainThread( CVideoPlayerAudioRenderer *pThis, int nSampleRate, int nChannels )
{
	if ( pThis != this )
		return false;

	Assert( !m_pAudioStream );
	if ( !m_pAudioStream )
	{
		// our wrapper wants bits per channel, not sample
		UISoundSystem()->PushAudioBigMixAheadBuffer();
		m_pAudioStream = UISoundSystem()->CreateAudioOutputStream( nSampleRate, nChannels, 16 );

		// can be NULL with no sound device (e.g. over RDP with disabled remote audio)
		if ( m_pAudioStream )
			m_pAudioStream->SetVolume( m_flVolume );
#if defined(SOURCE2_PANORAMA)
		m_nChannels = nChannels;
		m_nSampleRate = nSampleRate;
#endif
	}
	// other thread is waiting so make sure we always set this event
	m_eventWait.Set();

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Called to free audio input
//-----------------------------------------------------------------------------
void CVideoPlayerAudioRenderer::FreeAudioOutput()
{
	UIEngine()->DispatchEventAsync( 0.0f, VideoPlayerFreeAudio::MakeEvent( NULL, this ) );
}


//-----------------------------------------------------------------------------
// Purpose: Handles UI event to free audio output
//-----------------------------------------------------------------------------
bool CVideoPlayerAudioRenderer::OnFreeAudioMainThread( CVideoPlayerAudioRenderer *pThis )
{
	if ( pThis != this )
		return false;

	if ( m_pAudioStream )
		UISoundSystem()->PopAudioBigMixAheadBuffer();

	if( m_pAudioStream )
	{
		UISoundSystem()->FreeAudioOutputStream( m_pAudioStream );
		m_pAudioStream = NULL;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Call be called at shutdown to prevent deadlock where we wait on video playback thread to stop but that tells main thread to free (requiring ui events to be processed)
//-----------------------------------------------------------------------------
void CVideoPlayerAudioRenderer::MarkShuttingDown()
{
	// first mark that we are shutting down then flag wait event clear to prevent deadlock during shutdown
	m_bShuttingDown = 1;
	m_eventWait.Set();
}


//-----------------------------------------------------------------------------
// Purpose: Called after video decode thread has stopped
//-----------------------------------------------------------------------------
void CVideoPlayerAudioRenderer::Shutdown()
{
	OnFreeAudioMainThread( this );
	m_bShuttingDown = 0;
	m_eventWait.Reset();
}

#if defined(SOURCE2_PANORAMA)
//-----------------------------------------------------------------------------
// Purpose: Returns true if ready for more audio data
//-----------------------------------------------------------------------------
bool CVideoPlayerAudioRenderer::IsReadyForAudioData()
{
	if ( !m_pAudioStream )
		return false;

	uint32 nQueueSpace = m_pAudioStream->MaxWriteSampleCount();
	return nQueueSpace > 0;
}


//-----------------------------------------------------------------------------
// Purpose: Returns audio buffer to add data to
//-----------------------------------------------------------------------------
void *CVideoPlayerAudioRenderer::GetAudioBuffer()
{
	Assert( m_pAudioStream );
	if ( !m_pAudioStream )
		return NULL;

	return m_tempAudioBuffer;
}


//-----------------------------------------------------------------------------
// Purpose: Returns max amount of data that can be added to GetAudioBuffer()
//-----------------------------------------------------------------------------
uint32 CVideoPlayerAudioRenderer::GetAudioBufferSize()
{
	Assert( m_pAudioStream );
	if ( !m_pAudioStream )
		return 0;

	uint nSampleSize = sizeof( int16 ) * m_nChannels;
	uint32 nQueueSpace = m_pAudioStream->MaxWriteSampleCount();
	uint nAvailableSpace = nSampleSize * nQueueSpace;

	return MIN( nAvailableSpace, sizeof( m_tempAudioBuffer ) );
}


//-----------------------------------------------------------------------------
// Purpose: Returns min amount of data that can be added to GetAudioBuffer()
//-----------------------------------------------------------------------------
uint32 CVideoPlayerAudioRenderer::GetAudioBufferMinSize()
{
	Assert( m_pAudioStream );
	if ( !m_pAudioStream )
		return 0;
	return 4;
}


//-----------------------------------------------------------------------------
// Purpose: Commits data added to GetAudioBuffer()
//-----------------------------------------------------------------------------
void CVideoPlayerAudioRenderer::CommitAudioBuffer( uint32 unBytes )
{
	Assert( m_pAudioStream );
	if ( m_pAudioStream )
	{
		uint nSampleSize = sizeof( int16 ) * m_nChannels;
		m_pAudioStream->WriteAudioData( (const int16 *)m_tempAudioBuffer, unBytes / nSampleSize, m_nChannels );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Returns amount of audio data that has not yet been committed to hardware
//-----------------------------------------------------------------------------
uint32 CVideoPlayerAudioRenderer::GetRemainingCommittedAudio()
{
	Assert( m_pAudioStream );
	if ( !m_pAudioStream )
		return 0;

	uint nSampleSize = sizeof( int16 ) * m_nChannels;
	return m_pAudioStream->QueuedSampleCount() * nSampleSize;
}


//-----------------------------------------------------------------------------
// Purpose: Returns amount of audio data that has been played through this callback
//-----------------------------------------------------------------------------
uint32 CVideoPlayerAudioRenderer::GetMixedMilliseconds()
{
	Assert( m_pAudioStream );
	if ( !m_pAudioStream )
		return 0;

	return 1000;// m_pAudioStream->GetMixedMilliseconds();
}


//-----------------------------------------------------------------------------
// Purpose: Returns the amount of delay in MS before committed audio will generally be heard
//-----------------------------------------------------------------------------
uint32 CVideoPlayerAudioRenderer::GetPlaybackLatency()
{
	Assert( m_pAudioStream );
	if ( !m_pAudioStream )
		return 0;

	float flSecondsPerSample = 1.0 / double( m_nSampleRate );
	return double( m_pAudioStream->LatencySamplesCount() ) * flSecondsPerSample * 0.001f;
}


//-----------------------------------------------------------------------------
// Purpose: Pauses audio playback
//-----------------------------------------------------------------------------
void CVideoPlayerAudioRenderer::Pause()
{
	Assert( m_pAudioStream );
	if ( !m_pAudioStream )
		return;

	m_pAudioStream->Pause();
}


//-----------------------------------------------------------------------------
// Purpose: Resumes audio playback after a pause
//-----------------------------------------------------------------------------
void CVideoPlayerAudioRenderer::Resume()
{
	Assert( m_pAudioStream );
	if ( !m_pAudioStream )
		return;

	m_pAudioStream->Resume();
}
#else
//-----------------------------------------------------------------------------
// Purpose: Returns true if ready for more audio data
//-----------------------------------------------------------------------------
bool CVideoPlayerAudioRenderer::IsReadyForAudioData()
{
#ifdef SUPPORTS_AUDIO
	Assert( m_pAudioStream );
	if ( !m_pAudioStream )
		return false;

	return m_pAudioStream->IsReadyToMix();
#else
	return false;
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Returns audio buffer to add data to
//-----------------------------------------------------------------------------
void *CVideoPlayerAudioRenderer::GetAudioBuffer()
{
#ifdef SUPPORTS_AUDIO
	Assert( m_pAudioStream );
	if ( !m_pAudioStream )
		return NULL;

	return m_pAudioStream->GetMixBuffer();
#else
	return NULL;
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Returns max amount of data that can be added to GetAudioBuffer()
//-----------------------------------------------------------------------------
uint32 CVideoPlayerAudioRenderer::GetAudioBufferSize()
{
#ifdef SUPPORTS_AUDIO
	Assert( m_pAudioStream );
	if ( !m_pAudioStream )
		return 0;

	return m_pAudioStream->GetMixBufferSize();
#else
	return 0;
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Returns min amount of data that can be added to GetAudioBuffer()
//-----------------------------------------------------------------------------
uint32 CVideoPlayerAudioRenderer::GetAudioBufferMinSize()
{
#ifdef SUPPORTS_AUDIO
	Assert( m_pAudioStream );
	if ( !m_pAudioStream )
		return 0;
	return m_pAudioStream->GetMinMixBufferData();
#else
	return 0;
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Commits data added to GetAudioBuffer()
//-----------------------------------------------------------------------------
void CVideoPlayerAudioRenderer::CommitAudioBuffer( uint32 unBytes )
{
#ifdef SUPPORTS_AUDIO
	Assert( m_pAudioStream );
	if ( m_pAudioStream )
		m_pAudioStream->CommitMixBuffer( m_pAudioStream->GetMixBuffer(), unBytes );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Returns amount of audio data that has not yet been committed to hardware
//-----------------------------------------------------------------------------
uint32 CVideoPlayerAudioRenderer::GetRemainingCommittedAudio()
{
#ifdef SUPPORTS_AUDIO
	Assert( m_pAudioStream );
	if ( !m_pAudioStream )
		return 0;

	return m_pAudioStream->GetRemainingCommittedBytes();
#else
	return 0;
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Returns amount of audio data that has been played through this callback
//-----------------------------------------------------------------------------
uint32 CVideoPlayerAudioRenderer::GetMixedMilliseconds()
{
#ifdef SUPPORTS_AUDIO
	Assert( m_pAudioStream );
	if ( !m_pAudioStream )
		return 0;

	return m_pAudioStream->GetMixedMilliseconds();
#else
	return 0;
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Returns the amount of delay in MS before committed audio will generally be heard
//-----------------------------------------------------------------------------
uint32 CVideoPlayerAudioRenderer::GetPlaybackLatency()
{
#ifdef SUPPORTS_AUDIO
	Assert( m_pAudioStream );
	if ( !m_pAudioStream )
		return 0;

	return m_pAudioStream->GetDigitalLatency();
#else
	return 0;
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Pauses audio playback
//-----------------------------------------------------------------------------
void CVideoPlayerAudioRenderer::Pause()
{
#ifdef SUPPORTS_AUDIO
	Assert( m_pAudioStream );
	if ( !m_pAudioStream )
		return;

	m_pAudioStream->Pause();
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Resumes audio playback after a pause
//-----------------------------------------------------------------------------
void CVideoPlayerAudioRenderer::Resume()
{
#ifdef SUPPORTS_AUDIO
	Assert( m_pAudioStream );
	if ( !m_pAudioStream )
		return;

	m_pAudioStream->Play();
#endif
}

#endif		// SOURCE2_PANORAMA


//-----------------------------------------------------------------------------
// Purpose: Validation
//-----------------------------------------------------------------------------
#ifdef DBGFLAG_VALIDATE
void CVideoPlayerAudioRenderer::Validate( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE();
}
#endif 


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CVideoPlayerEventDispatcher::CVideoPlayerEventDispatcher( CPanoramaVideoPlayer *pPlayer )
{
	m_pPlayer = pPlayer;
	m_flLastRepeatEventDispatch = 0.0f;

	RegisterForUnhandledEvent( DispatchVideoPlayerEvent(), this, &CVideoPlayerEventDispatcher::VideoPlayerEventUIThread );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CVideoPlayerEventDispatcher::~CVideoPlayerEventDispatcher()
{
	UnregisterForUnhandledEvent( DispatchVideoPlayerEvent(), this, &CVideoPlayerEventDispatcher::VideoPlayerEventUIThread );
}


//-----------------------------------------------------------------------------
// Purpose: Register a panel to receive playback events
//-----------------------------------------------------------------------------
void CVideoPlayerEventDispatcher::RegisterEventListener( IUIPanel *pPanel )
{
	CPanelPtr< IUIPanel > ptrPanel( pPanel );
	AssertMsg( !m_vecListeners.HasElement( ptrPanel ), "Panel already registered for movie playback events" );
	m_vecListeners.AddToTail( ptrPanel );
}


//-----------------------------------------------------------------------------
// Purpose: Remove a panel from receiving playback events
//-----------------------------------------------------------------------------
void CVideoPlayerEventDispatcher::UnregisterEventListener( IUIPanel *pPanel )
{
	CPanelPtr< IUIPanel > ptrPanel( pPanel );
	bool bFound = m_vecListeners.FindAndFastRemove( ptrPanel );
	AssertMsg( bFound, "Panel not registered for movie playback events" );
}


//-----------------------------------------------------------------------------
// Purpose: Registers a delegate to be called back with video player events
//-----------------------------------------------------------------------------
void CVideoPlayerEventDispatcher::RegisterEventCallback( VideoPlayerEventDelegate_t del )
{
	AssertMsg( !m_vecCallbacks.HasElement( del ), "Video player already has specified delegate" );
	m_vecCallbacks.AddToTail( del );
}


//-----------------------------------------------------------------------------
// Purpose: Unregisters a delegate to be called back with video player events
//-----------------------------------------------------------------------------
void CVideoPlayerEventDispatcher::UnregisterEventCallback( VideoPlayerEventDelegate_t del )
{
	AssertMsg( m_vecCallbacks.HasElement( del ), "Video player didn't have specified delegate" );
	m_vecCallbacks.FindAndFastRemove( del );
}


//-----------------------------------------------------------------------------
// Purpose: Called on video thread when an event occurs
//-----------------------------------------------------------------------------
void CVideoPlayerEventDispatcher::VideoPlayerEvent( EVideoPlayerEvent eEvent )
{
#ifndef PANORAMA_USE_S1WRAPPER
	CThreadFiber_Temporary tempFiberScope;
#endif


	// we could get a lot of repeat events for a tiny file. Only dispatch these every 3 seconds to avoid flooding the event system
	if ( eEvent == k_EVideoPlayerEventRepeat )
	{
		double flCurrent = UIEngine()->GetCurrentFrameTime();
		if ( m_flLastRepeatEventDispatch != 0.0f && (m_flLastRepeatEventDispatch + 3.0f) > flCurrent )
			return;

		// ok to dispatch
		m_flLastRepeatEventDispatch = flCurrent;
	}

	UIEngine()->DispatchEventAsync( 0.0f, DispatchVideoPlayerEvent::MakeEvent( NULL, this, eEvent ) );
}


//-----------------------------------------------------------------------------
// Purpose: Helper to dispatch video player events
//-----------------------------------------------------------------------------
void CVideoPlayerEventDispatcher::DispatchVideoEvent( EVideoPlayerEvent eEvent, IUIPanel *pTarget )
{
	if ( eEvent == k_EVideoPlayerEventInit )
		DispatchEvent( VideoPlayerInitalized(), pTarget->ClientPtr(), m_pPlayer );
	else if ( eEvent == k_EVideoPlayerEventEnd )
		DispatchEvent( VideoPlayerEnded(), pTarget->ClientPtr(), m_pPlayer );
	else if ( eEvent == k_EVideoPlayerEventRepeat )
		DispatchEvent( VideoPlayerRepeated(), pTarget->ClientPtr(), m_pPlayer );
	else if ( eEvent == k_EVideoPlayerEventPlaybackStateChange )
		DispatchEvent( VideoPlayerPlaybackStateChange(), pTarget->ClientPtr(), m_pPlayer );
	else if ( eEvent == k_EVideoPlayerEventChangedRepresentation )
		DispatchEvent( VideoPlayerChangedRepresentation(), pTarget->ClientPtr(), m_pPlayer );
}


//-----------------------------------------------------------------------------
// Purpose: Called on main thread when a video player event occurs
//-----------------------------------------------------------------------------
bool CVideoPlayerEventDispatcher::VideoPlayerEventUIThread( CVideoPlayerEventDispatcher *pDispatcher, EVideoPlayerEvent eEvent )
{
	if ( pDispatcher != this )
		return false;

	// dispatch events to listeners
	FOR_EACH_VEC( m_vecListeners, i )
	{
		IUIPanel *pTarget = m_vecListeners[i].Get();
		if ( !pTarget )
			continue;

		DispatchVideoEvent( eEvent, pTarget );		
	}

	// directly call any callbacks
	FOR_EACH_VEC( m_vecCallbacks, i )
	{
		m_vecCallbacks[i]( eEvent );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Validation
//-----------------------------------------------------------------------------
#ifdef DBGFLAG_VALIDATE
void CVideoPlayerEventDispatcher::Validate( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE();
	ValidateObj( m_vecListeners );
	ValidateObj( m_vecCallbacks );
}
#endif 


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CPanoramaVideoPlayer::CPanoramaVideoPlayer( IUIPanel *pPanel ) : m_videoCallback( pPanel->UIRenderDevice() ), m_eventCallback( this )
{
#if !defined( PANORAMA_DISABLE_VIDEO )
	m_pVideoPlayer = ::CreateVideoPlayer( &m_eventCallback, &m_videoCallback, &m_audioCallback );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CPanoramaVideoPlayer::CPanoramaVideoPlayer( IUIRenderDevice *pDevice ) : m_videoCallback( pDevice ), m_eventCallback( this )
{
#if !defined( PANORAMA_DISABLE_VIDEO )
	m_pVideoPlayer = ::CreateVideoPlayer( &m_eventCallback, &m_videoCallback, &m_audioCallback );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CPanoramaVideoPlayer::~CPanoramaVideoPlayer()
{
	Stop();

#if !defined( PANORAMA_DISABLE_VIDEO )
	if ( m_pVideoPlayer )
		::DeleteVideoPlayer( m_pVideoPlayer );
#endif

	m_pVideoPlayer = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Loads a video file from a path
//-----------------------------------------------------------------------------
bool CPanoramaVideoPlayer::BLoad( const char *pchURL )
{
	if( !m_pVideoPlayer )
		return false;

	// treat any provided string as a file resource
	CUtlString strPath = UIEngine()->ResolvePath( pchURL );

	if ( !g_pFullFileSystem->FileExists( strPath ) )
		return false;

#if defined( SOURCE2_PANORAMA )

	// Need to convert to a absolute path before we pass to thirdparty (Steam) video player which does it's own fileio
	char rgchFullPath[4096];
	if ( g_pFullFileSystem->RelativePathToFullPath( strPath.String(), "GAME", rgchFullPath, V_ARRAYSIZE( rgchFullPath ) ) )
	{
		strPath = rgchFullPath;
	}

#endif

	return m_pVideoPlayer->BLoad( strPath.String() );
}


//-----------------------------------------------------------------------------
// Purpose: Loads video from memory
//-----------------------------------------------------------------------------
bool CPanoramaVideoPlayer::BLoad( const byte *pubData, uint cubData )
{
	if( !m_pVideoPlayer )
		return false;

	return m_pVideoPlayer->BLoad( pubData, cubData );
}


//-----------------------------------------------------------------------------
// Purpose: Get the current size of the texture
//-----------------------------------------------------------------------------
void CPanoramaVideoPlayer::GetTextureSize( uint32 &unTextureWidth, uint32 &unTextureHeight )
{
	IUIDoubleBufferedYUV420Texture *pTexture = m_videoCallback.GetTexture();
	if ( pTexture )
	{
		pTexture->GetTextureSize( unTextureWidth, unTextureHeight );
	}
	else
	{
		unTextureWidth = 0;
		unTextureHeight = 0;
	}
}


//-----------------------------------------------------------------------------
// Purpose: stops video playback
//-----------------------------------------------------------------------------
void CPanoramaVideoPlayer::Stop()
{
	m_audioCallback.MarkShuttingDown();

	if ( m_pVideoPlayer )
		m_pVideoPlayer->Stop();

	// just in case we haven't yet handled the dispatched free audio call, do so here
	m_audioCallback.Shutdown();
}


//-----------------------------------------------------------------------------
// Purpose: Validation
//-----------------------------------------------------------------------------
#ifdef DBGFLAG_VALIDATE
void CPanoramaVideoPlayer::Validate( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE();
	ValidateObj( m_videoCallback );
	ValidateObj( m_audioCallback );
	ValidateObj( m_eventCallback );
}
#endif 
