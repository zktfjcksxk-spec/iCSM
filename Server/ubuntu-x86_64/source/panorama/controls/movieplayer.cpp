//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx_client.h"
#include "panorama/controls/button.h"
#include "panorama/controls/label.h"
#include "panorama/controls/movieplayer.h"
#include "panorama/controls/vumeter.h"
#include "panorama/controls/slider.h"
#include "panorama/layout/csshelpers.h"
#include "panorama/renderer/styleproperties.h"
#include "panorama/uijsregistration.h"
#include "panorama/iuisoundsystem.h"
#include "videocfg/videocfg.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

REGISTER_PANEL2D_FACTORY( CMoviePlayer, Movie );
REGISTER_PANEL2D_FACTORY( CMoviePanel, MoviePanel );
REGISTER_PANEL2D_FACTORY( CVolumeSliderPopup, VolumeSliderPopup );
REGISTER_PANEL2D_FACTORY( CMovieVideoQualityPopup, VideoQualityPopup );
REGISTER_PANEL2D( CMovieDebug, MovieDebug );

DEFINE_PANORAMA_EVENT( MoviePlayerAudioStart );
DEFINE_PANORAMA_EVENT( MoviePlayerAudioStop );
DEFINE_PANORAMA_EVENT( MoviePlayerPlaybackStart );
DEFINE_PANORAMA_EVENT( MoviePlayerPlaybackStop );
DEFINE_PANORAMA_EVENT( MoviePlayerPlaybackEnded );
DEFINE_PANORAMA_EVENT( MoviePlayerTogglePlayPause );
DEFINE_PANORAMA_EVENT( MoviePlayerFastForward );
DEFINE_PANORAMA_EVENT( MoviePlayerUIVisible );
DEFINE_PANORAMA_EVENT( MoviePlayerJumpBack );
DEFINE_PANORAMA_EVENT( MoviePlayerVolumeControl );
DEFINE_PANORAMA_EVENT( MoviePlayerFullscreenControl );
DEFINE_PANORAMA_EVENT( MoviePlayerSetRepresentation );
DEFINE_PANORAMA_EVENT( MoviePlayerSelectVideoQuality );

DEFINE_PANORAMA_EVENT( VolumeSliderValueChanged );

static const char * k_symAudioVolumeMuted( "AudioVolumeMuted" );
static const char * k_symAudioVolumeFull( "AudioVolumeFull" );
static const char * k_symAudioVolumeUnder66( "AudioVolumeUnder66" );
static const char * k_symAudioVolumeUnder33( "AudioVolumeUnder33" );
static const char * k_symMoviePaused( "MoviePaused" );
static const char * k_symMovieBuffering( "MovieBuffering" );
static const char * k_symMoviePlaying( "MoviePlaying" );
static const char * k_symMovieError( "MovieError" );
static const char * k_symVisible( "Visible" );
static const char * k_symShowTitle( "ShowTitle" );


static const int k_nSkipForwardDefaultMS = 15 * k_nThousand;
static const int k_nSkipBackwardDefaultMS = 15 * k_nThousand;
//static const float k_rgflMoviePlaybackSpeeds[] = { -32.0f, -16.0f, -8.0f, -2.0f, 1.0f, 2.0f, 8.0f, 16.0f, 32.0f };
static const float k_flDefaultMoviePlaybackSpeed = 1.0f;
static const float k_rgflMoviePlaybackSpeeds[] = { k_flDefaultMoviePlaybackSpeed, 2.0f, 8.0f, 16.0f, 32.0f };
static const int k_nInvalidVideoRepresentation = -1;

ConVar g_ConVarPanoramaDebugMovies( "@panorama_debug_movies", "0" );
ConVar g_ConVarPanoramaPlayMovieAmbientSound( "panorama_play_movie_ambient_sound", "1" );


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CMoviePlayer::CMoviePlayer( CPanel2D *parent, const char * pchPanelID ) : CPanel2D( parent, pchPanelID, ePanelFlags_DontFireOnLoad ), m_scheduledUpdate( MAKE_SCHEDULED_FUNC( CMoviePlayer::Update ) )
{
	m_eAutoplay = k_EAutoplayOff;
	m_eControls = k_EControlsNone;
	m_bDisableActivatePause = false;
	m_bShowControlsNotFullscreen = false;
	m_bRepeat = false;
	m_bMuted = false;
	m_bRaisedAudioStartEvent = false;
	m_bRaisedPlaybackStartEvent = false;
	m_bHadFocus = false;
	m_bCloseControlsOnPlay = false;
	m_bSrcResolutionBasedOnPanelSize = true;
	m_bExternalShowTitle = false;
	m_pMoviePanel = NULL;
	m_flVolume = 0.0f;
	m_iDesiredVideoRepresentation = k_nInvalidVideoRepresentation;

	m_bInConstructor = true;
	DbgVerify( BLoadLayout( "file://{resources}/layout/movie.xml", true ) );
	m_bInConstructor = false;

	m_pMoviePanel = assert_cast< CMoviePanel* >( FindChildInLayoutFile( "MoviePanel" ) );
	m_pPlayIndicator = FindChildInLayoutFile( "PlayIndicator" );
	m_pLoadingThrobber = FindChildInLayoutFile( "LoadingThrobber" );

	m_pErrorMessage = assert_cast< CLabel* >( FindChildInLayoutFile( "ErrorMessage" ) );
	m_pPlaybackControls = FindChildInLayoutFile( "PlaybackControls" );
	m_pVolumeControl = FindChildInLayoutFile( "VolumeControl" );
	m_pPlaybackProgressBar = FindChildInLayoutFile( "PlaybackProgress" );
	m_pTimeline = FindChildInLayoutFile( "Timeline" );
	m_pControlBarRow = FindChildInLayoutFile( "ControlBarRow" );
	m_pPlayPauseBtn = assert_cast< CToggleButton* >( FindChildInLayoutFile( "PlayPause" ) );
	m_pPlaybackSpeed = assert_cast< CLabel* >( FindChildInLayoutFile( "PlaybackSpeed" ) );
	m_pPlaybackTitleAndControls = FindChildInLayoutFile( "PlaybackTitleAndControls" );
	m_pPlaybackTitle = assert_cast< CLabel* >( FindChildInLayoutFile( "PlaybackTitle" ) );
	m_pVideoQualityBtn = assert_cast< CButton* >( FindChildInLayoutFile( "VideoQualityBtn" ) );
	
	RegisterEventHandler( Activated(), this, &CMoviePlayer::EventActivated );
	RegisterEventHandler( Cancelled(), this, &CMoviePlayer::EventCancelled );
	RegisterEventHandler( InputFocusSet(), this, &CMoviePlayer::EventInputFocusSet );
	RegisterEventHandler( InputFocusLost(), this, &CMoviePlayer::EventInputFocusLost );	
	RegisterEventHandler( VideoPlayerInitalized(), this, &CMoviePlayer::EventMovieInitialized );
	RegisterEventHandler( VideoPlayerPlaybackStateChange(), this, &CMoviePlayer::EventVideoPlayerPlaybackStateChanged );
	RegisterEventHandler( VideoPlayerChangedRepresentation(), this, &CMoviePlayer::EventVideoPlayerChangedRepresentation );
	RegisterEventHandler( MoviePlayerTogglePlayPause(), this, &CMoviePlayer::EventMovieTogglePlayPause );
	RegisterEventHandler( MoviePlayerFastForward(), this, &CMoviePlayer::EventMoviePlayerFastForward );
	RegisterEventHandler( VideoPlayerEnded(), this, &CMoviePlayer::EventVideoPlayerEnded );
	RegisterEventHandler( MoviePlayerJumpBack(), this, &CMoviePlayer::EventMoviePlayerJumpBack );
	RegisterEventHandler( MoviePlayerVolumeControl(), this, &CMoviePlayer::EventMoviePlayerVolumeControl );	
	RegisterEventHandler( MoviePlayerSelectVideoQuality(), this, &CMoviePlayer::EventMoviePlayerSelectQuality );
	RegisterEventHandler( MoviePlayerSetRepresentation(), this, &CMoviePlayer::EventSetRepresentation );
	RegisterEventHandler( VolumeSliderValueChanged(), this, &CMoviePlayer::EventVolumeSliderValueChanged );

	RegisterForUnhandledEvent( SoundMuteChanged(), this, &CMoviePlayer::EventSoundMuteChanged );
	RegisterForUnhandledEvent( SoundVolumeChanged(), this, &CMoviePlayer::EventSoundVolumeChanged );

	SetPlaybackVolume( UISoundSystem()->GetSoundVolume( k_ESoundType_Movies ) );
	SetAcceptsFocus( true );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CMoviePlayer::~CMoviePlayer()
{
	VPROF_BUDGET( "~CMoviePlayer", VPROF_BUDGETGROUP_TENFOOT );
	Clear();

	UnregisterForUnhandledEvent( SoundMuteChanged(), this, &CMoviePlayer::EventSoundMuteChanged );
	UnregisterForUnhandledEvent( SoundVolumeChanged(), this, &CMoviePlayer::EventSoundVolumeChanged );
}


//-----------------------------------------------------------------------------
// Purpose: Setup v8 object template
//-----------------------------------------------------------------------------
void CMoviePlayer::SetupJavascriptObjectTemplate()
{
	CPanel2D::SetupJavascriptObjectTemplate();

	RegisterJSMethod( "SetMovie", PANORAMA_DELEGATE_RESOLVE( &CMoviePlayer::SetMovie, const char* ) );
	RegisterJSMethod( "SetControls", PANORAMA_DELEGATE_RESOLVE( &CMoviePlayer::SetControls, const char* ) );
	RegisterJSMethod( "SetTitle", PANORAMA_DELEGATE( &CMoviePlayer::SetTitleText ) );
	RegisterJSMethod( "Play", PANORAMA_DELEGATE( &CMoviePlayer::Play ) );
	RegisterJSMethod( "Pause", PANORAMA_DELEGATE( &CMoviePlayer::Pause ) );
	RegisterJSMethod( "Stop", PANORAMA_DELEGATE( &CMoviePlayer::Stop ) );
	RegisterJSMethod( "SetRepeat", PANORAMA_DELEGATE( &CMoviePlayer::SetRepeat ) );
	RegisterJSMethod( "SetPlaybackVolume", PANORAMA_DELEGATE( &CMoviePlayer::SetPlaybackVolume ) );
	RegisterJSMethod( "BAdjustingVolume", PANORAMA_DELEGATE( &CMoviePlayer::BAdjustingVolume ) );
	RegisterJSMethod( "SetSound", PANORAMA_DELEGATE( &CMoviePlayer::SetSound ) );
}


//-----------------------------------------------------------------------------
// Purpose: Resets player (clears loaded movie)
//-----------------------------------------------------------------------------
void CMoviePlayer::Clear()
{
	if ( m_ptrVolumeSlider.Get() )
		m_ptrVolumeSlider->Close();

	CMovieDebug *pDebug = m_ptrDebug.Get();
	if ( pDebug )
		delete pDebug;

	RaisePlaybackStopEvents();
	m_pMoviePanel->Clear();
}


//-----------------------------------------------------------------------------
// Purpose: Convert string to EControls value
//-----------------------------------------------------------------------------
CMoviePlayer::EControls CMoviePlayer::EControlsFromString( const char *pchControls )
{
	EControls eControls = k_EControlsNone;
	if ( V_stricmp( pchControls, "none") == 0 )
		eControls = k_EControlsNone;
	else if ( V_stricmp( pchControls, "minimal") == 0 )
		eControls = k_EControlsMinimal;
	else if ( V_stricmp( pchControls, "full") == 0 )
		eControls = k_EControlsFull;
	else
		eControls = k_EControlsInvalid;

	return eControls;
}


//-----------------------------------------------------------------------------
// Purpose: Applies properties set from layout file
//-----------------------------------------------------------------------------
bool CMoviePlayer::BSetProperties( const CUtlVector< ParsedPanelProperty_t > &vecProperties )
{
	// ignore any params in our layout file as our child controls have not yet been created
	if ( m_bInConstructor )
		return true;

	static CPanoramaSymbol k_symAutoplay( "autoplay" );
	static CPanoramaSymbol k_symControls( "controls" );
	static CPanoramaSymbol k_symRepeat( "repeat" );
	static CPanoramaSymbol k_symSource( "src" );
	static CPanoramaSymbol k_symSound( "sound" );
	static CPanoramaSymbol k_symMuted( "muted" );
	static CPanoramaSymbol k_symDisableActivatePause( "disableactivatepause" );
	static CPanoramaSymbol k_symShowControlsNotFullscreen( "controlsnotfullscreen" );
	static CPanoramaSymbol k_symTitle( "title" );

	const char *pchLayoutFile = NULL;
	bool bSuccess = true;
	FOR_EACH_VEC( vecProperties, i )
	{
		const ParsedPanelProperty_t &prop = vecProperties[i];
		if ( prop.m_symName == k_symAutoplay )
		{
			m_eAutoplay = k_EAutoplayOff;
			if ( V_stricmp( prop.m_pchValue, "off") == 0 )
				m_eAutoplay = k_EAutoplayOff;
			else if ( V_stricmp( prop.m_pchValue, "onload") == 0 )
				m_eAutoplay = k_EAutoplayOnLoad;
			else if ( V_stricmp( prop.m_pchValue, "onfocus") == 0 )
				m_eAutoplay = k_EAutoplayOnFocus;
			else
				bSuccess = false;
		}
		else if ( prop.m_symName == k_symControls )
		{
			m_eControls = EControlsFromString( prop.m_pchValue );
			if ( m_eControls == k_EControlsInvalid )
			{
				bSuccess = false;
			}
		}
		else if ( prop.m_symName == k_symRepeat )
		{
			if ( !CSSHelpers::BParseTrueFalse( prop.m_pchValue, &m_bRepeat ) )
				bSuccess = false;
		}
		else if ( prop.m_symName == k_symMuted )
		{
			if ( !CSSHelpers::BParseTrueFalse( prop.m_pchValue, &m_bMuted ) )
				bSuccess = false;
		}
		else if ( prop.m_symName == k_symSource )
		{
			// delay loading movie until we are done loading the layout file. Need movie panel pointer
			pchLayoutFile = prop.m_pchValue;
		}
		else if ( prop.m_symName == k_symSound )
		{
			SetSound( prop.m_pchValue );
		}
		else if ( prop.m_symName == k_symShowControlsNotFullscreen )
		{
			if ( !CSSHelpers::BParseTrueFalse( prop.m_pchValue, &m_bShowControlsNotFullscreen ) )
				bSuccess = false;
		}
		else if ( prop.m_symName == k_symDisableActivatePause )
		{
			if ( !CSSHelpers::BParseTrueFalse( prop.m_pchValue, &m_bDisableActivatePause ) )
				bSuccess = false;
		}
		else if ( prop.m_symName == k_symTitle )
		{
			SetTitleText( prop.m_pchValue );
		}
		else
		{
			if ( !BSetProperty( prop.m_symName, prop.m_pchValue ) )
				bSuccess = false;
		}
	}

	// now that we have a movie panel pointer and all parameters have been parsed, set up player
	// note that BSetProperties could be called multiple times, so only set movie if we had a source param
	if ( pchLayoutFile )
		SetMovie( pchLayoutFile );

	// hide/show appropriate controls
	SetControls( m_eControls );

	return bSuccess;
}


//-----------------------------------------------------------------------------
// Purpose: Sets autoplay value. Will not stop a movie in progress, but will start one
//-----------------------------------------------------------------------------
void CMoviePlayer::SetAutoplay( EAutoplay eAutoplay, bool bSkipPlay )
{
	m_eAutoplay = eAutoplay;

	CVideoPlayerPtr pMovie = GetMovie();
	if ( !pMovie || m_eAutoplay == k_EAutoplayOff )
		return;
	
	if ( !bSkipPlay && m_eAutoplay == k_EAutoplayOnLoad )
		Play();
	
	if ( !bSkipPlay && m_eAutoplay == k_EAutoplayOnFocus && (BHasKeyFocus() || BHasDescendantKeyFocus()) )
		Play();
}


//-----------------------------------------------------------------------------
// Purpose: Sets controls to show
//-----------------------------------------------------------------------------
void CMoviePlayer::SetControls( EControls eControls )
{
	m_eControls = eControls;
	m_pPlayIndicator->SetVisible( eControls != k_EControlsNone );
	m_pLoadingThrobber->SetHasClass( k_symVisible, eControls != k_EControlsNone );
	m_pPlaybackControls->SetVisible( eControls == k_EControlsFull );

	UpdateFullUI();
}


//-----------------------------------------------------------------------------
// Purpose: Sets controls to show
//-----------------------------------------------------------------------------
void CMoviePlayer::SetControls( const char *pchControls )
{
	EControls eControls = EControlsFromString( pchControls );
	if ( eControls != k_EControlsInvalid )
	{
		SetControls( eControls );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sets title text to display with controls
//-----------------------------------------------------------------------------
void CMoviePlayer::SetTitleText( const char *pchText )
{
	m_pPlaybackTitle->SetText( pchText );
}


//-----------------------------------------------------------------------------
// Purpose: Adds classes to show title section
//-----------------------------------------------------------------------------
void CMoviePlayer::ShowTitle( bool bImmediatelyVisible )
{
	m_bExternalShowTitle = true;
	ShowTitleInternal( bImmediatelyVisible );
}


//-----------------------------------------------------------------------------
// Purpose: Internal version of set title
//-----------------------------------------------------------------------------
void CMoviePlayer::ShowTitleInternal( bool bImmediatelyVisible )
{
	static CPanoramaSymbol k_symTitleImmediatelyVisible( "TitleImmediatelyVisible" );

	if ( bImmediatelyVisible )
	{
		AddClass( k_symTitleImmediatelyVisible );
		m_pPlaybackTitleAndControls->MarkStylesDirty( true );
		m_pPlaybackTitleAndControls->ApplyStyles( true );
		RemoveClass( k_symTitleImmediatelyVisible );
	}

	AddClass( k_symShowTitle );
}


//-----------------------------------------------------------------------------
// Purpose: Removes class to show title section
//-----------------------------------------------------------------------------
void CMoviePlayer::HideTitle()
{
	m_bExternalShowTitle = false;

	// don't let external calls (like from slideshow) hide title if control bar is visible
	if ( BAnyControlsVisible() )
		return;

	HideTitleInternal();
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if user has modal up to adjust movie volume
//-----------------------------------------------------------------------------
bool CMoviePlayer::BAdjustingVolume()
{
	return (m_ptrVolumeSlider.Get() != NULL);
}


//-----------------------------------------------------------------------------
// Purpose: Internal version of hide title
//-----------------------------------------------------------------------------
void CMoviePlayer::HideTitleInternal()
{
	RemoveClass( k_symShowTitle );
}

void CMoviePlayer::Update()
{
	// For some reason the video player is never firing the event when a movie ends
	// So this schedule update tells us when the video ends
	CPanoramaVideoPlayer *pPlayer = m_pMoviePanel->GetMovie().Get();
	if ( pPlayer )
	{
		// movie might not yet be initialized
		float flMovieDuration = ( float )pPlayer->GetDuration();
		if ( flMovieDuration > 0.0f )
		{
			float flCurrentPlayback = ( float )pPlayer->GetCurrentPlaybackTime();
			if ( flCurrentPlayback / flMovieDuration >= 1.0f )
			{
				if ( !m_bRepeat )
				{
					EventVideoPlayerEnded( pPlayer );
				}
			}
			else
			{
				m_scheduledUpdate.Schedule( ( flMovieDuration - flCurrentPlayback ) / 1000.0f + 0.1f );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Updates UI
//-----------------------------------------------------------------------------
void CMoviePlayer::UpdateFullUI()
{
	UpdateTimeline();
	UpdatePlayPauseButton();
	UpdatePlaybackSpeed();
	UpdateMovingPlayingStyle();

	Update();
}


//-----------------------------------------------------------------------------
// Purpose: Updates the movie playback timeline (progress, buffered, etc.)
//-----------------------------------------------------------------------------
void CMoviePlayer::UpdateTimeline()
{
	if ( m_eControls != k_EControlsFull || !m_pTimeline->BIsVisible() )
		return;

	CPanoramaVideoPlayer *pPlayer = m_pMoviePanel->GetMovie().Get();
	float flPercentComplete = 0.0f;
	float flRemainingWithSpeed = 0.0f;
	if ( pPlayer )
	{
		// movie might not yet be initialized
		float flMovieDuration = (float)pPlayer->GetDuration();
		if ( flMovieDuration > 0.0f )
		{
			float flCurrentPlayback = (float)pPlayer->GetCurrentPlaybackTime();
			flPercentComplete = flCurrentPlayback / flMovieDuration;
			float flPlaybackSpeed = pPlayer->GetPlaybackSpeed();
			if ( flPlaybackSpeed > 0.0f )
				flRemainingWithSpeed = (flMovieDuration - flCurrentPlayback) / flPlaybackSpeed;
		}
	}

	Msg( "CMoviePlayer::UpdateTimeline: percent=%f, remaining=%f\n", flPercentComplete, flRemainingWithSpeed );

	// clear transition properties immediately
	CUtlVector< TransitionProperty_t > vecTransitionProperties;
	m_pPlaybackProgressBar->AccessStyle()->SetTransitionProperties( vecTransitionProperties );
	m_pPlaybackProgressBar->ApplyStyles( true );

	// set starting position
	CUtlVector< CTransform3D* > vecTransforms;
	vecTransforms.AddToTail( new CTransformScale3D( flPercentComplete, 1.0f, 1.0f ) );	
	m_pPlaybackProgressBar->SetTransform3D( vecTransforms );

	// if movie is playing, kick off animation to completion
	if ( pPlayer && pPlayer->GetPlaybackState() == k_EVideoPlayerPlaybackStatePlay && !pPlayer->IsStoppedForBuffering() && flRemainingWithSpeed > 0.0f )
	{
		// set transition property for time remaining immediately
		TransitionProperty_t &transitionProperty = vecTransitionProperties[ vecTransitionProperties.AddToTail() ];
		transitionProperty.m_symProperty = CStylePropertyTransform3D::symbol;
		transitionProperty.m_flTransitionSeconds = flRemainingWithSpeed / k_nThousand;
		transitionProperty.m_flTransitionDelaySeconds = 0;
		transitionProperty.m_eTimingFunction = k_EAnimationLinear;

		Vector2D vecPoints[4];
		AccessStyle()->GetAnimationCurveControlPoints( panorama::k_EAnimationLinear, vecPoints );
		transitionProperty.m_CubicBezier.SetControlPoints( vecPoints );

		m_pPlaybackProgressBar->AccessStyle()->SetTransitionProperties( vecTransitionProperties );
		m_pPlaybackProgressBar->ApplyStyles( true );

		// set final transform to 0
		CUtlVector< CTransform3D* > vecFinalTransform;
		vecFinalTransform.AddToTail( new CTransformScale3D( 1.0f, 1.0f, 1.0f ) );
		m_pPlaybackProgressBar->SetTransform3D( vecFinalTransform );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Updates the UI state of the play/pause button
//-----------------------------------------------------------------------------
void CMoviePlayer::UpdatePlayPauseButton()
{
	bool bPlaying = false;
	bool bModifiedPlaybackRate = false;
	CPanoramaVideoPlayer *pMovie = m_pMoviePanel->GetMovie().Get();
	if ( pMovie )
	{
		bPlaying = (pMovie->GetPlaybackState() == k_EVideoPlayerPlaybackStatePlay);
		bModifiedPlaybackRate = (pMovie->GetPlaybackSpeed() != 1.0f );
	}

	m_pPlayPauseBtn->SetSelected( !bPlaying || bModifiedPlaybackRate );
}


//-----------------------------------------------------------------------------
// Purpose: Updates the UI to show playback speed
//-----------------------------------------------------------------------------
void CMoviePlayer::UpdatePlaybackSpeed()
{
	float flPlaybackSpeed = k_flDefaultMoviePlaybackSpeed;
	CPanoramaVideoPlayer *pMovie = m_pMoviePanel->GetMovie().Get();
	if ( pMovie )
		flPlaybackSpeed = pMovie->GetPlaybackSpeed();

	if ( flPlaybackSpeed == 0.0f )
		return;

	if ( !BControlBarVisible() )
		DisplayTimeline( flPlaybackSpeed != k_flDefaultMoviePlaybackSpeed );

	if ( BAnyControlsVisible() )
	{
		const char *pchPlaybackString = "#Movie_Playing";
		if ( flPlaybackSpeed != 1.0f )
			pchPlaybackString = ( flPlaybackSpeed > 0.0f ) ? "#Movie_FastFoward" : "#Movie_Rewind";

		m_pPlaybackSpeed->SetText( pchPlaybackString );
		SetDialogVariable( "playback_speed", abs( ( int )flPlaybackSpeed ) );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sets video resolution as text on label
//-----------------------------------------------------------------------------
void SetVideoResolutionText( CLabel *pLabel, int nHeight )
{
	// we are only encoding progressive content so just tack on the p
	pLabel->SetText( CFmtStr( "%dp", nHeight ).String() );	
}


//-----------------------------------------------------------------------------
// Purpose: Show/hide the control bar
//-----------------------------------------------------------------------------
void CMoviePlayer::DisplayControls( bool bVisible )
{
	if ( m_eControls != k_EControlsFull )
		bVisible = false;
	else if ( m_bShowControlsNotFullscreen && !BHasClass( "Fullscreen" ) )
		bVisible = true;

	if ( bVisible )
	{
		if ( !m_pControlBarRow->BHasClass( k_symVisible ) )
			DispatchEvent( MoviePlayerUIVisible(), this );

		m_pControlBarRow->AddClass( k_symVisible );
		if ( !m_pControlBarRow->BHasDescendantKeyFocus() )
			m_pControlBarRow->SetFocus();
	}
	else
	{
		m_pControlBarRow->RemoveClass( k_symVisible );
		if ( m_pControlBarRow->BHasDescendantKeyFocus() )
			SetFocus();		
	}

	if ( BAnyControlsVisible() || m_bExternalShowTitle )
		ShowTitleInternal();
	else
		HideTitleInternal();
}


//-----------------------------------------------------------------------------
// Purpose: Show/hide the timeline
//-----------------------------------------------------------------------------
void CMoviePlayer::DisplayTimeline( bool bVisible )
{
	if ( m_eControls != k_EControlsFull )
		return;

	if ( bVisible )
	{
		if ( !m_pTimeline->BHasClass( k_symVisible ) )
			DispatchEvent( MoviePlayerUIVisible(), this );

		m_pPlaybackSpeed->AddClass( k_symVisible );
		m_pTimeline->AddClass( k_symVisible );
	}
	else
	{
		m_pPlaybackSpeed->RemoveClass( k_symVisible );
		m_pTimeline->RemoveClass( k_symVisible );
	}

	if ( BAnyControlsVisible() || m_bExternalShowTitle )
		ShowTitleInternal();
	else
		HideTitleInternal();
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if the timeline is visible
//-----------------------------------------------------------------------------
bool CMoviePlayer::BTimelineVisible()
{
	return m_pTimeline->BHasClass( k_symVisible );
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if any controls are visible (control bar or timeline)
//-----------------------------------------------------------------------------
bool CMoviePlayer::BAnyControlsVisible()
{
	if ( m_eControls != k_EControlsFull )
		return false;

	return ( BControlBarVisible() || m_pPlaybackSpeed->BHasClass( k_symVisible ) || BTimelineVisible() );
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if the control bar is visible
//-----------------------------------------------------------------------------
bool CMoviePlayer::BControlBarVisible()
{
	if ( m_eControls != k_EControlsFull )
		return false;

	return (m_pControlBarRow->BHasClass( k_symVisible ));
}


//-----------------------------------------------------------------------------
// Purpose: Tells parents that audio playback has started. Makes sure we raise start/stop w/o duplicating events
//-----------------------------------------------------------------------------
void CMoviePlayer::RaisePlaybackStartEvents()
{
	CVideoPlayerPtr pMovie = m_pMoviePanel->GetMovie();

	if ( !m_bRaisedAudioStartEvent && pMovie.IsValid() && pMovie->BHasAudioTrack() )
	{
		m_bRaisedAudioStartEvent = true;
		DispatchEvent( MoviePlayerAudioStart(), this );
	}

	if ( !m_bRaisedPlaybackStartEvent )
	{
		m_bRaisedPlaybackStartEvent = true;
		DispatchEvent( MoviePlayerPlaybackStart(), this );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Tells parents that audio playback has stopped. Makes sure we raise start/stop w/o duplicating events
//-----------------------------------------------------------------------------
void CMoviePlayer::RaisePlaybackStopEvents()
{
	if ( m_bRaisedAudioStartEvent )
	{
		m_bRaisedAudioStartEvent = false;
		DispatchEvent( MoviePlayerAudioStop(), this );
	}

	if ( m_bRaisedPlaybackStartEvent )
	{
		m_bRaisedPlaybackStartEvent = false;
		DispatchEvent( MoviePlayerPlaybackStop(), this );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sets if the movie should repeat
//-----------------------------------------------------------------------------
void CMoviePlayer::SetRepeat( bool bRepeat )
{
	m_bRepeat = bRepeat;
	CVideoPlayerPtr pMovie = GetMovie();
	if ( !pMovie )
		return;

	pMovie->SetRepeat( bRepeat );
}


//-----------------------------------------------------------------------------
// Purpose: Sets the movie to play
//-----------------------------------------------------------------------------
void CMoviePlayer::SetMovie( const char *pchFile )
{
	Assert( m_pMoviePanel );

	m_pMoviePanel->SetMovie( pchFile, m_bSrcResolutionBasedOnPanelSize );

	SetPlaybackVolume( m_flVolume );
	UpdateFullUI();
	
	CVideoPlayerPtr pMovie = m_pMoviePanel->GetMovie();
	if ( !pMovie )
		return;

	CMovieDebug *pDebug = m_ptrDebug.Get();
	if ( pDebug )
		pDebug->Show( pMovie );
	
	pMovie->SetRepeat( m_bRepeat );
	if ( m_eAutoplay == k_EAutoplayOnLoad )
		Play();
}


//-----------------------------------------------------------------------------
// Purpose: Sets the movie to play
//-----------------------------------------------------------------------------
void CMoviePlayer::SetMovie( CVideoPlayerPtr pPlayer )
{
	Assert( m_pMoviePanel );
	m_pMoviePanel->SetMovie( pPlayer );
	SetPlaybackVolume( m_flVolume );
	UpdateFullUI();

	CMovieDebug *pDebug = m_ptrDebug.Get();
	if ( pDebug )
		pDebug->Show( pPlayer );

	pPlayer->SetRepeat( m_bRepeat );
	if ( m_eAutoplay == k_EAutoplayOnLoad )
		Play();
}

void CMoviePlayer::SetSound( const char* pSoundEvent )
{
	if ( m_pMoviePanel )
	{
		m_pMoviePanel->SetSoundEvent( pSoundEvent );
	}
}

//-----------------------------------------------------------------------------
// Purpose: This panel or a child just received focus
//-----------------------------------------------------------------------------
bool CMoviePlayer::EventInputFocusSet( const CPanelPtr< IUIPanel > &ptrPanel )
{
	if ( m_eAutoplay != k_EAutoplayOnFocus || !m_pMoviePanel->GetMovie() )
		return false;

	// only unpause if first time we have had decendantfocus. Don't want to change pause/play when navigating child controls
	if ( !m_bHadFocus )
	{
		m_bHadFocus = true;
		Play();
	}
	
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: This panel or a child just lost input focus
//-----------------------------------------------------------------------------
bool CMoviePlayer::EventInputFocusLost( const CPanelPtr< IUIPanel > &ptrPanel )
{
	if ( m_eAutoplay != k_EAutoplayOnFocus || !m_pMoviePanel->GetMovie() )
		return false;

	// check if we still have descendantfocus. If not, unpause
	if ( m_bHadFocus && (!BHasDescendantKeyFocus() && !BHasKeyFocus()) )
	{
		m_bHadFocus = false;
		Pause();
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Received an event that our movie has been initialized
//-----------------------------------------------------------------------------
bool CMoviePlayer::EventMovieInitialized( IVideoPlayer *pIMovie )
{
	CPanoramaVideoPlayer *pMovie = (CPanoramaVideoPlayer*)pIMovie;
	if ( !pMovie || pMovie != m_pMoviePanel->GetMovie().Get() )
		return false;

	UpdateMovingPlayingStyle();
	if ( pMovie->GetPlaybackState() == k_EVideoPlayerPlaybackStatePlay )
		RaisePlaybackStartEvents();

	AddClass( "IsPlaying" );

	// let event bubble!
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Received an event that our movie's playback progress has changed
//-----------------------------------------------------------------------------
bool CMoviePlayer::EventVideoPlayerPlaybackStateChanged( IVideoPlayer *pIMovie )
{
	CPanoramaVideoPlayer *pMovie = (CPanoramaVideoPlayer*)pIMovie;
	if ( !pMovie || pMovie != m_pMoviePanel->GetMovie().Get() )
		return false;

	if ( pMovie->BHasAudioTrack() && pMovie->GetPlaybackState() == k_EVideoPlayerPlaybackStatePlay )
		RaisePlaybackStartEvents();
	else
		RaisePlaybackStopEvents();

	UpdateFullUI();
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Recieved an event that our movie's playback representation has changed
//-----------------------------------------------------------------------------
bool CMoviePlayer::EventVideoPlayerChangedRepresentation( IVideoPlayer *pIMovie )
{
	CPanoramaVideoPlayer *pMovie = (CPanoramaVideoPlayer*)pIMovie;
	if ( !pMovie || pMovie != m_pMoviePanel->GetMovie().Get() )
		return false;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Received an event that our movie's playback has ended
//-----------------------------------------------------------------------------
bool CMoviePlayer::EventVideoPlayerEnded( IVideoPlayer *pIMovie )
{
	CPanoramaVideoPlayer *pMovie = (CPanoramaVideoPlayer*)pIMovie;
	if ( !pMovie || pMovie != m_pMoviePanel->GetMovie().Get() )
		return false;

	UpdateMovingPlayingStyle();
	DispatchEvent( MoviePlayerPlaybackEnded(), this, pMovie->GetPlaybackError() );

	RemoveClass( "IsPlaying" );
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Updates movie playing style.. used to show play/pause minimal ui
//-----------------------------------------------------------------------------
void CMoviePlayer::UpdateMovingPlayingStyle()
{
	CPanoramaVideoPlayer *pMovie = m_pMoviePanel->GetMovie().Get();
	if ( !pMovie )
		return;

	// determine new style to set
	CPanoramaSymbol symNewStyle;
	if ( pMovie->GetPlaybackError() != k_EVideoPlayerPlaybackErrorNone )
	{
		symNewStyle = k_symMovieError;
	}
	else if ( pMovie->GetPlaybackState() == k_EVideoPlayerPlaybackStatePlay )
	{
		symNewStyle = pMovie->IsStoppedForBuffering() ? k_symMovieBuffering : k_symMoviePlaying;
	}
	else
	{
		symNewStyle = k_symMoviePaused;
	}

	if ( symNewStyle == m_symMoviePlaybackStyle )
		return;

	if ( m_symMoviePlaybackStyle.IsValid() )
		RemoveClass( m_symMoviePlaybackStyle );

	AddClass( symNewStyle );
	m_symMoviePlaybackStyle = symNewStyle;	
}


//-----------------------------------------------------------------------------
// Purpose: Panel has been activated
//-----------------------------------------------------------------------------
bool CMoviePlayer::EventActivated( const CPanelPtr< IUIPanel > &ptrPanel, EPanelEventSource_t eSource )
{
	if ( ToPanel2D(ptrPanel.Get()) != this )
		return false;

	if ( BHasOnActivateEvent() )
		return false;

	if ( m_eControls == k_EControlsFull )
	{
		bool bShowControls = !BAnyControlsVisible() || (eSource != k_ePanelEventSourceMouse);
		DisplayControls( bShowControls );
		DisplayTimeline( bShowControls );
		return true;
	}
	
	if ( !m_bDisableActivatePause )
	{
		TogglePlayPause();
		return true;
	}
	
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Panel has been cancelled
//-----------------------------------------------------------------------------
bool CMoviePlayer::EventCancelled( const CPanelPtr< IUIPanel > &ptrPanel, EPanelEventSource_t eSource )
{
	CPanel2D *pPanel = ToPanel2D(ptrPanel.Get());
	if ( pPanel != this && !pPanel->IsDescendantOf( this ) )
		return false;

	// if movie controls are visible, hide them
	bool bOutOfFullScreenShowingControls = (m_bShowControlsNotFullscreen && !BHasClass( "Fullscreen" ));
	if ( BAnyControlsVisible() && !bOutOfFullScreenShowingControls )
	{
		DisplayControls( false );
		DisplayTimeline( false );

		return true;
	}

	if ( BHasClass( "Fullscreen" ) )
	{
		DispatchEvent( MoviePlayerFullscreenControl(), this );
		return true;
	}
	
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Handles gamepad input
//-----------------------------------------------------------------------------
bool CMoviePlayer::OnGamePadDown( const GamePadData_t &code )
{
	// this is hacky & doesn't handle keyboard/mouse
	switch( code.m_GamePadCode )
	{
	case XK_BUTTON_RTRIGGER:
		if ( m_eControls == k_EControlsFull )
		{
			FastForward();
			return true;
		}
		break;

	case XK_BUTTON_LTRIGGER:
		if ( m_eControls == k_EControlsFull )
		{
			Rewind();
			return true;
		}
		break;

	case XK_BUTTON_Y:
		if ( m_eControls == k_EControlsFull )
		{
			DisplayControls( true );
			DisplayTimeline( true );
			return true;
		}
		break;
	}

	return BaseClass::OnGamePadDown( code );
}


//-----------------------------------------------------------------------------
// Purpose: Handles key typed events
//-----------------------------------------------------------------------------
bool CMoviePlayer::OnKeyTyped( const KeyData_t &unichar )
{
	if ( unichar.m_UniChar == L' ' && !m_bDisableActivatePause )
		TogglePlayPause();

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Returns a pointer to the panel which should be used for default focus. Can return NULL
//-----------------------------------------------------------------------------
panorama::IUIPanel *CMoviePlayer::OnGetDefaultInputFocus()
{
	if ( m_eControls == k_EControlsFull && BControlBarVisible() )
		return m_pPlaybackControls->UIPanel();

	if ( m_bShowControlsNotFullscreen && !BHasClass( "Fullscreen" ) )
		return m_pControlBarRow->UIPanel();
	
	return BaseClass::OnGetDefaultInputFocus();
}


//-----------------------------------------------------------------------------
// Purpose: Seeks from the current movie location the specified number of milliseconds
//-----------------------------------------------------------------------------
void CMoviePlayer::Seek( uint unOffset )
{
	CPanoramaVideoPlayer *pMovie = m_pMoviePanel->GetMovie().Get();
	if ( !pMovie )
		return;

	unOffset = MAX( unOffset, pMovie->GetDuration() );
	pMovie->Seek( unOffset );
}


//-----------------------------------------------------------------------------
// Purpose: Toggles play/pause
//-----------------------------------------------------------------------------
bool CMoviePlayer::EventMovieTogglePlayPause()
{
	TogglePlayPause();
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Toggle pause / play
//-----------------------------------------------------------------------------
void CMoviePlayer::TogglePlayPause()
{
	CPanoramaVideoPlayer *pMovie = m_pMoviePanel->GetMovie().Get();
	if ( !pMovie )
		return;

	if ( pMovie->GetPlaybackState() == k_EVideoPlayerPlaybackStatePause || pMovie->GetPlaybackSpeed() != 1.0f )
		Play();
	else if ( pMovie->GetPlaybackState() == k_EVideoPlayerPlaybackStatePlay )
		Pause();
}


//-----------------------------------------------------------------------------
// Purpose: Handles event to fast forward the movie
//-----------------------------------------------------------------------------
bool CMoviePlayer::EventMoviePlayerFastForward()
{
	FastForward();
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Increases the playback speed
//-----------------------------------------------------------------------------
void CMoviePlayer::FastForward()
{
	CPanoramaVideoPlayer *pMovie = m_pMoviePanel->GetMovie().Get();
	if ( !pMovie )
		return;

	// get the current playback state, then increment one
	float flCurrent = pMovie->GetPlaybackSpeed();
	float flNextPlaybackSpeed = k_rgflMoviePlaybackSpeeds[ V_ARRAYSIZE( k_rgflMoviePlaybackSpeeds ) - 1 ];
	for ( int i = 0; i < V_ARRAYSIZE( k_rgflMoviePlaybackSpeeds ); i++ )
	{
		if ( flCurrent >= k_rgflMoviePlaybackSpeeds[i] )
			continue;

		flNextPlaybackSpeed = k_rgflMoviePlaybackSpeeds[i];
		break;
	}

	pMovie->SetPlaybackSpeed( flNextPlaybackSpeed );
	pMovie->Play();
	UpdatePlaybackSpeed();
}


//-----------------------------------------------------------------------------
// Purpose: Handles event to jump back in the movie
//-----------------------------------------------------------------------------
bool CMoviePlayer::EventMoviePlayerJumpBack()
{
	CPanoramaVideoPlayer *pMovie = m_pMoviePanel->GetMovie().Get();
	if ( !pMovie )
		return true;

	uint32 unCurrent = pMovie->GetCurrentPlaybackTime();
	uint32 unSeekTo = 0;
	if ( unCurrent > k_nSkipBackwardDefaultMS )
		unSeekTo = unCurrent - k_nSkipBackwardDefaultMS;
	
	Seek( unSeekTo );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Shows volume control popup
//-----------------------------------------------------------------------------
bool CMoviePlayer::EventMoviePlayerVolumeControl()
{
	if ( !m_ptrVolumeSlider.Get() )
		m_ptrVolumeSlider = new CVolumeSliderPopup( m_pVolumeControl, NULL );

	m_ptrVolumeSlider->Show( m_flVolume );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CMoviePlayer::EventVolumeSliderValueChanged( float flValue )
{
	UISoundSystem()->SetSoundVolume( k_ESoundType_Movies, flValue );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Shows the movie player select video representation popup
//-----------------------------------------------------------------------------
bool CMoviePlayer::EventMoviePlayerSelectQuality()
{
	CPanoramaVideoPlayer *pMovie = m_pMoviePanel->GetMovie().Get();
	if ( !pMovie || m_ptrVideoQualityPopup.Get() )
		return true;

	CMovieVideoQualityPopup *pPopup = new CMovieVideoQualityPopup( m_pVideoQualityBtn, NULL );
	m_ptrVideoQualityPopup = pPopup;

	int cRepresentations = pMovie->GetVideoRepresentationCount();
	for ( int i = 0; i < cRepresentations; i++ )
	{
		int nWidth;
		int nHeight;
		if ( pMovie->BGetVideoRepresentationInfo( i, &nWidth, &nHeight ) )
			pPopup->AddRepresentation( i, nHeight );
	}

	int nVideoWidth = 0;
	int nVideoHeight = 0;
	pMovie->GetVideoResolution( &nVideoWidth, &nVideoHeight );

	pPopup->Show( m_iDesiredVideoRepresentation, nVideoHeight );
	
	return true;

}


//-----------------------------------------------------------------------------
// Purpose: Sound volume across the system changed. If for movies, update self
//-----------------------------------------------------------------------------
bool CMoviePlayer::EventSoundVolumeChanged( ESoundType eSoundType, float flVolume )
{
	if ( eSoundType != k_ESoundType_Movies )
		return false;

	SetPlaybackVolume( flVolume );

	// let bubble
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Sound volume has been muted/unmuted
//-----------------------------------------------------------------------------
bool CMoviePlayer::EventSoundMuteChanged( bool bMute )
{
	SetPlaybackVolume( UISoundSystem()->GetSoundVolume( k_ESoundType_Movies ) );

	// let bubble
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Sets representation manually
//-----------------------------------------------------------------------------
bool CMoviePlayer::EventSetRepresentation( int iRep )
{
	m_iDesiredVideoRepresentation = iRep;
	CPanoramaVideoPlayer *pMovie = m_pMoviePanel->GetMovie().Get();
	if ( pMovie )
		pMovie->ForceVideoRepresentation( iRep );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Decreases the playback speed
//-----------------------------------------------------------------------------
void CMoviePlayer::Rewind()
{
	CPanoramaVideoPlayer *pMovie = m_pMoviePanel->GetMovie().Get();
	if ( !pMovie )
		return;

	// get the current playback state, then increment one
	float flCurrent = pMovie->GetPlaybackSpeed();
	float flNextPlaybackSpeed = k_rgflMoviePlaybackSpeeds[ 0 ];
	for ( int i = V_ARRAYSIZE( k_rgflMoviePlaybackSpeeds ) - 1; i > 0; i-- )
	{
		if ( flCurrent <= k_rgflMoviePlaybackSpeeds[i] )
			continue;

		flNextPlaybackSpeed = k_rgflMoviePlaybackSpeeds[i];
		break;
	}

	pMovie->SetPlaybackSpeed( flNextPlaybackSpeed );
	pMovie->Play();
	UpdatePlaybackSpeed();
}


//-----------------------------------------------------------------------------
// Purpose: Starts movie playback
//-----------------------------------------------------------------------------
void CMoviePlayer::SetPlaybackVolume( float flVolume )
{
	if ( m_bMuted )
		flVolume = 0.0f;

	if ( m_pMoviePanel )
		m_pMoviePanel->SetPlaybackVolume( flVolume );

	m_flVolume = flVolume;
	if ( m_flVolume > 0.66f )
		SetAudioVolumeStyle( k_symAudioVolumeFull );
	else if ( m_flVolume > 0.33f )
		SetAudioVolumeStyle( k_symAudioVolumeUnder66 );
	else if ( m_flVolume > 0.0f )
		SetAudioVolumeStyle( k_symAudioVolumeUnder33 );
	else
		SetAudioVolumeStyle( k_symAudioVolumeMuted );
}


//-----------------------------------------------------------------------------
// Purpose: Removes old audio volume style and sets new one
//-----------------------------------------------------------------------------
void CMoviePlayer::SetAudioVolumeStyle( CPanoramaSymbol symStyle )
{
	if ( m_pVolumeControl->BHasClass( symStyle ) )
		return;

	m_pVolumeControl->RemoveClass( k_symAudioVolumeMuted );
	m_pVolumeControl->RemoveClass( k_symAudioVolumeFull );
	m_pVolumeControl->RemoveClass( k_symAudioVolumeUnder66 );
	m_pVolumeControl->RemoveClass( k_symAudioVolumeUnder33 );

	m_pVolumeControl->AddClass( symStyle );
}


//-----------------------------------------------------------------------------
// Purpose: Starts movie playback
//-----------------------------------------------------------------------------
void CMoviePlayer::Play()
{
	CPanoramaVideoPlayer *pMovie = m_pMoviePanel->GetMovie().Get();
	if ( !pMovie )
		return;

	m_pMoviePanel->SuggestMovieHeight();
	pMovie->SetPlaybackSpeed( 1.0f );
	pMovie->Play();

	if ( m_bCloseControlsOnPlay )
	{
		DisplayTimeline( false );
		DisplayControls( false );
		m_bCloseControlsOnPlay = false;
	}

	AddClass( "IsPlaying" );
}


//-----------------------------------------------------------------------------
// Purpose: Stops movie playback
//-----------------------------------------------------------------------------
void CMoviePlayer::Stop()
{
	CPanoramaVideoPlayer *pMovie = m_pMoviePanel->GetMovie().Get();
	if ( !pMovie )
		return;

	pMovie->Stop();

	RemoveClass( "IsPlaying" );
}


//-----------------------------------------------------------------------------
// Purpose: Pauses movie playback
//-----------------------------------------------------------------------------
void CMoviePlayer::Pause()
{
	CPanoramaVideoPlayer *pMovie = m_pMoviePanel->GetMovie().Get();
	if ( !pMovie )
		return;

	m_pMoviePanel->GetMovie()->Pause();

	if ( m_eControls == k_EControlsFull && (BHasDescendantKeyFocus() || BHasKeyFocus()) )
	{
		m_bCloseControlsOnPlay = !BControlBarVisible();
		DisplayTimeline( true );
		DisplayControls( true );
		m_pPlayPauseBtn->SetFocus();		
	}
}


//-----------------------------------------------------------------------------
// Purpose: Handles mouse button down
//-----------------------------------------------------------------------------
bool CMoviePlayer::OnMouseButtonDown( const MouseData_t &code )
{
	if ( g_ConVarPanoramaDebugMovies.GetInt() != 0 && code.m_RepeatCount == 0 && code.m_MouseCode == MOUSE_RIGHT )
	{
		CMovieDebug *pDebug = m_ptrDebug.Get();
		if ( !pDebug )
		{
			pDebug = new CMovieDebug( this, NULL );
			m_ptrDebug = pDebug;
			MoveChildAfter( pDebug, m_pMoviePanel );

			pDebug->Show( m_pMoviePanel->GetMovie() );
		}
		else
		{
			delete pDebug;
		}

		
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CMoviePanel::CMoviePanel( CPanel2D *parent, const char *pchPanelID )
	: CPanel2D( parent, pchPanelID )
	, m_bPausedForUnready( false )
{
	m_sSoundEvent[0] = 0;
	m_pSoundPlaying = nullptr;
	m_pVideoPlayer = nullptr;

	RegisterForReadyEvents( true );

	if ( !UIEngine()->BHaveEventHandlersRegisteredForType( CMoviePanel::GetPanelSymbol() ) )
	{
		RegisterEventHandlerOnPanelType( VideoPlayerInitalized(), &CMoviePanel::EventVideoPlayerInitialized );
		RegisterEventHandlerOnPanelType( ReadyForDisplay(), &CMoviePanel::EventReadyForDisplay );
		RegisterEventHandlerOnPanelType( UnreadyForDisplay(), &CMoviePanel::EventUnreadyForDisplay );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CMoviePanel::~CMoviePanel()
{
	if ( m_pVideoPlayer.IsValid() )
		m_pVideoPlayer = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Clears movie
//-----------------------------------------------------------------------------
void CMoviePanel::Clear()
{
	VPROF_BUDGET( "CMoviePanel::Clear", VPROF_BUDGETGROUP_TENFOOT );

	if ( m_pSoundPlaying != nullptr )
	{
		UIEngine()->UISoundSystem()->FadeOutAndStopSoundSample( m_pSoundPlaying, 0.1f );
		m_pSoundPlaying = nullptr;
	}

	if ( m_pVideoPlayer.IsValid() )
	{
		m_pVideoPlayer->UnregisterEventListener( UIPanel() );
		m_pVideoPlayer = NULL;
	}

	InvalidateSizeAndPosition();
}


//-----------------------------------------------------------------------------
// Purpose: Sets movie from path
//
// Added bChooseOptimalResolution - cheap way of determining whether to play lower resolution
// movie (if present - is a significant perf win), assumptions:
// * .webm files used
// * specifically set 
//		* <filename>540p.webm if parent panel height < 720
//		* <filename>720p.webm if parent panel height < 1080
//		* <filename>.webm otherwise
//		* Also we always try for 540p if num logical cores is < 3
//-----------------------------------------------------------------------------
void CMoviePanel::SetMovie( const char *pchFile, bool bChooseOptimalResolution )
{
	static ConVarRef gpu_level( "gpu_level" );
	static ConVarRef gpu_mem_level( "gpu_mem_level" );

	Clear();
	if ( !pchFile || pchFile[0] == '\0' )
		return;

#if !defined( SOURCE2_PANORAMA )
	m_pVideoPlayer.SetNoRef( new CPanoramaVideoPlayer( UIPanel() ) );
#else
	CPanoramaVideoPlayer *pPlayer = new CPanoramaVideoPlayer( UIPanel() );
	m_pVideoPlayer = pPlayer;
	pPlayer->Release();
#endif

	char szFile[ 1024 ] = { 0 };
	V_strcpy_safe( szFile, pchFile );

	if ( bChooseOptimalResolution )
	{
		char szFileRes[ 1024 ] = { 0 };

		uint32 nHeight = GetParentWindow()->GetWindowHeight();
		bool bExists = false;

		if ( ( nHeight < 720 ) || 
			 ( GetCPUInformation().m_nLogicalProcessors < 3 ) ||
			 ( gpu_level.GetInt() <= GPU_LEVEL_MEDIUM ) ||
			 ( gpu_mem_level.GetInt() <= GPU_MEM_LEVEL_LOW ) )
		{
			V_StrSubst( szFile, ".webm", "540p.webm", szFileRes, sizeof( szFileRes ), false );

			bExists = m_pVideoPlayer->BLoad( szFileRes );
		}

		if ( !bExists && 
			 ( ( nHeight < 1080 ) ||
			   ( gpu_level.GetInt() <= GPU_LEVEL_HIGH ) ||
			   ( gpu_mem_level.GetInt() <= GPU_MEM_LEVEL_MEDIUM ) ) )
		{
			V_StrSubst( szFile, ".webm", "720p.webm", szFileRes, sizeof( szFileRes ), false );

			bExists = m_pVideoPlayer->BLoad( szFileRes );
		}
		
		if ( !bExists )
		{
			bExists = m_pVideoPlayer->BLoad( szFile );
		}

		if ( !bExists )
		{
			m_pVideoPlayer = NULL;
			return;
		}
	}
	else
	{
		if ( !m_pVideoPlayer->BLoad( szFile ) )
		{
			m_pVideoPlayer = NULL;
			return;
		}
	}

	m_pVideoPlayer->RegisterEventListener( UIPanel() );
	SuggestMovieHeight();
}


//-----------------------------------------------------------------------------
// Purpose: Sets movie from path
//-----------------------------------------------------------------------------
void CMoviePanel::SetMovie( CVideoPlayerPtr pPlayer )
{
	Clear();
	m_pVideoPlayer = pPlayer;
	m_pVideoPlayer->RegisterEventListener( UIPanel() );
	SuggestMovieHeight();
}

void CMoviePanel::SetSoundEvent( const char *pSoundEvent )
{
	V_strncpy(m_sSoundEvent, pSoundEvent, sizeof(m_sSoundEvent));
}

//-----------------------------------------------------------------------------
// Purpose: Sets audio playback volume
//-----------------------------------------------------------------------------
void CMoviePanel::SetPlaybackVolume( float flVolume )
{
	if ( m_pVideoPlayer.IsValid() )
	{
		m_pVideoPlayer->SetPlaybackVolume( flVolume );
	}
}


//-----------------------------------------------------------------------------
// Purpose: override to change how this panel is measured
//-----------------------------------------------------------------------------
void CMoviePanel::OnContentSizeTraverse( float *pflContentWidth, float *pflContentHeight, float flMaxWidth, float flMaxHeight, bool bFinalDimensions )
{
	BaseClass::OnContentSizeTraverse( pflContentWidth, pflContentHeight, flMaxWidth, flMaxHeight, bFinalDimensions );

	// when padding is specified as a percentage, CSS uses the max width to calculate top and bottom. We have kept that pattern here.
	if ( m_pVideoPlayer.IsValid() )
	{ 
		uint32 unTextureWidth = 0;
		uint32 unTextureHeight = 0;
		m_pVideoPlayer->GetTextureSize( unTextureWidth, unTextureHeight );

		float flLeft, flTop, flRight, flBottom;
		AccessStyle()->GetContentInset( unTextureWidth * GetActualUIScaleX(), unTextureHeight * GetActualUIScaleY(), bFinalDimensions, flLeft, flTop, flRight, flBottom );

		*pflContentWidth = MAX( unTextureWidth * GetActualUIScaleX() + flLeft + flRight, *pflContentWidth );
		*pflContentHeight = MAX( unTextureHeight * GetActualUIScaleY() + flTop + flBottom, *pflContentHeight );
	}
}


//-----------------------------------------------------------------------------
// Purpose: tells video player what our rendered dimensions are
//-----------------------------------------------------------------------------
void CMoviePanel::OnLayoutTraverse( float flFinalWidth, float flFinalHeight )
{
	BaseClass::OnLayoutTraverse( flFinalWidth, flFinalHeight );

	SuggestMovieHeight();
}


//-----------------------------------------------------------------------------
// Purpose: Tells movie player our height
//-----------------------------------------------------------------------------
void CMoviePanel::SuggestMovieHeight()
{
	int nNewHeight = (int)GetActualRenderHeight();
	if ( m_pVideoPlayer.IsValid() )
	{
		m_pVideoPlayer->SuggestMaxVeritcalResolution( nNewHeight );	
	}
}


//-----------------------------------------------------------------------------
// Purpose: Paint movie
//-----------------------------------------------------------------------------
void CMoviePanel::Paint()
{
	VPROF_BUDGET( "CMoviePanel::Paint", VPROF_BUDGETGROUP_TENFOOT );

	if(m_sSoundEvent[0] && g_ConVarPanoramaPlayMovieAmbientSound.GetBool() && (m_pSoundPlaying == nullptr || !UIEngine()->UISoundSystem()->IsSoundStillPlaying(m_pSoundPlaying)))
	{
		m_pSoundPlaying = UIEngine()->UISoundSystem()->PlaySound(m_sSoundEvent, nullptr, panorama::k_ESoundType_Movies, 1.0f, 0.5f, 0.0f);
	}
	else if(m_pSoundPlaying != nullptr && g_ConVarPanoramaPlayMovieAmbientSound.GetBool() == false)
	{
		UIEngine()->UISoundSystem()->FadeOutAndStopSoundSample(m_pSoundPlaying, 0.1f);
		m_pSoundPlaying = nullptr;
	}

	BaseClass::Paint();
	if ( m_pVideoPlayer.IsValid() )
	{
		if ( m_pVideoPlayer->GetTexture() )
		{
			float flLeft, flTop, flRight, flBottom;
			AccessStyle()->GetContentInset( GetActualLayoutWidth(), GetActualLayoutHeight(), false, flLeft, flTop, flRight, flBottom );
			AccessRenderEngine()->DrawTexturedRect( m_pVideoPlayer->GetTexture(), k_ETextureSampleModeNormal, flLeft, flTop, GetActualLayoutWidth() - flRight, GetActualLayoutHeight() - flBottom, 0.0f, 0.0f, 1.0f, 1.0f );
		}

		if ( m_pVideoPlayer->GetPlaybackState() != k_EVideoPlayerPlaybackStateStop )
			SetRepaint( k_EPanelRepaintFull );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Handles movie initialization event
//-----------------------------------------------------------------------------
bool CMoviePanel::EventVideoPlayerInitialized( IVideoPlayer *pIMovie )
{
	CPanoramaVideoPlayer *pMovie = (CPanoramaVideoPlayer*)pIMovie;
	if ( pMovie != m_pVideoPlayer.Get() )
		return false;

	InvalidateSizeAndPosition();
	SetRepaint( k_EPanelRepaintFull );

	// let bubble so MoviePlayer can see events too
	return false;
}

bool CMoviePanel::EventReadyForDisplay( const CPanelPtr< IUIPanel > &pPanel )
{
	if ( !m_bPausedForUnready )
		return true;

	m_bPausedForUnready = false;

	if ( !m_pVideoPlayer.Get() )
		return true;

	if ( m_pVideoPlayer->GetPlaybackState() != k_EVideoPlayerPlaybackStatePause )
		return true;

	m_pVideoPlayer->Play();
	return true;
}

bool CMoviePanel::EventUnreadyForDisplay( const CPanelPtr< IUIPanel > &pPanel )
{
	if ( m_bPausedForUnready )
		return true;

	if ( !m_pVideoPlayer.Get() )
		return true;

	if ( m_pVideoPlayer->GetPlaybackState() != k_EVideoPlayerPlaybackStatePlay )
		return true;

	m_pVideoPlayer->Pause();
	m_bPausedForUnready = true;
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Validation
//-----------------------------------------------------------------------------
#ifdef DBGFLAG_VALIDATE
void CMoviePanel::ValidateClientPanel( CValidator &validator, const tchar *pchName )
{
	VALIDATE_SCOPE();
	BaseClass::ValidateClientPanel( validator, pchName );

	ValidatePtrIfNeeded( m_pVideoPlayer.Get() );
}
#endif


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CMovieControlPopupBase::CMovieControlPopupBase( CPanel2D *pInvokingPanel, const char *pchPanelID ) : CPanel2D( pInvokingPanel->GetParentWindow(), pchPanelID )
{
	Assert( pInvokingPanel );
	m_pInvokingPanel = pInvokingPanel;
	SetLayoutFile( m_pInvokingPanel->GetLayoutFile() );
	SetLayoutLoadedFromParent( m_pInvokingPanel );

	// make a clickable background that takes up entire screen. Clicking on it should close this popup
	CUILength len( 100.0f, CUILength::k_EUILengthPercent );
	m_pInvisibleBackground = new CPanel2D( this, NULL );
	m_pInvisibleBackground->SetSize( len, len );
	m_pInvisibleBackground->SetOnActivateEvent( Cancelled::MakeEvent( this, k_ePanelEventSourceProgram ) );

	// create slider second so on top of background
	m_pPopupBackground = new CPanel2D( this, NULL );
	m_pPopupBackground->AddClass( "PopupWrapper" );
	m_pPopupBackground->SetSelectionPosition( k_flSelectionPosAuto, k_flSelectionPosAuto );
	m_pPopupBackground->SetTabIndex( k_flTabIndexAuto );

	RegisterEventHandler( Cancelled(), this, &CVolumeSliderPopup::EventCancelled );
}


//-----------------------------------------------------------------------------
// Purpose: Hides the popup menu
//-----------------------------------------------------------------------------
void CMovieControlPopupBase::Close()
{
	m_pInvokingPanel->SetFocus();
	DeleteAsync( 0.1f );
}


//-----------------------------------------------------------------------------
// Purpose: Handles the canceled event
//-----------------------------------------------------------------------------
bool CMovieControlPopupBase::EventCancelled( const CPanelPtr< IUIPanel > &pPanel, EPanelEventSource_t eSource )
{
	Close();
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: override to change how this panel arranges its children
//-----------------------------------------------------------------------------
void CMovieControlPopupBase::OnLayoutTraverse( float flFinalWidth, float flFinalHeight )
{
	// do default then override where the slider is positioned
	CPanel2D::OnLayoutTraverse( flFinalWidth, flFinalHeight );

	// find top left of the invoking panel
	float flTargetX = 0.0f;
	float flTargetY = 0.0f;
	m_pInvokingPanel->GetPositionWithinAncestor( NULL, &flTargetX, &flTargetY );	

	// get window dimensions
	float flWindowWidth, flWindowHeight;
	GetParentWindow()->GetClientDimensions( flWindowWidth, flWindowHeight );

	// try to position above the target. If we can't fit above or below, go align to bottom of window
	float flX = flTargetX;
	float flY = 0.0f;
	float flTargetHeight = m_pInvokingPanel->GetActualRenderHeight();
	if ( flTargetY > m_pPopupBackground->GetActualLayoutHeight() )
	{
		// room above
		flY = flTargetY - m_pPopupBackground->GetActualLayoutHeight();
	}
	else if ( flWindowHeight - flTargetY - flTargetHeight > m_pPopupBackground->GetActualLayoutHeight() )
	{
		// room below
		flY = flTargetY + flTargetHeight;
	}
	else
	{
		// not enough room either side. Just align to bottom of window
		flY = flWindowHeight - m_pPopupBackground->GetActualLayoutHeight();
	}

	// set our position based on width & where the dropdown is located
	CUILength lenX( flX, CUILength::k_EUILengthLength );
	CUILength lenY( flY, CUILength::k_EUILengthLength );

	lenX.ScaleLengthValue( 1.0f / GetActualUIScaleX() );
	lenY.ScaleLengthValue( 1.0f / GetActualUIScaleY() );

	m_pPopupBackground->SetPosition( lenX, lenY, CUILength( 0, CUILength::k_EUILengthLength ) );
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CVolumeSliderPopup::CVolumeSliderPopup( CPanel2D *pInvokingPanel, const char *pchPanelID ) : CMovieControlPopupBase( pInvokingPanel, pchPanelID )
{
	m_pSlider = new CSlider( m_pPopupBackground, NULL );
	m_pSlider->SetMin( 0.0f );
	m_pSlider->SetMax( 1.0f );
	m_pSlider->SetIncrement( 0.05f );
	m_pSlider->SetDirection( CSlider::k_EDirectionVertical );

	RegisterEventHandler( SliderValueChanged(), this, &CVolumeSliderPopup::EventSliderValueChanged );
	SetAcceptsInput( true );
}


//-----------------------------------------------------------------------------
// Purpose: Shows the drop down menu
//-----------------------------------------------------------------------------
void CVolumeSliderPopup::Show( float flVolume )
{
	// always invalidate layout when becoming visible. Makes sure we are positioned properly
	InvalidateSizeAndPosition();
	m_pSlider->SetValue( flVolume );
	m_pSlider->SetFocus();
}


//-----------------------------------------------------------------------------
// Purpose: Volume slider changed.. update settings
//-----------------------------------------------------------------------------
bool CVolumeSliderPopup::EventSliderValueChanged( const CPanelPtr< IUIPanel > &pPanel, float flValue )
{
	DispatchEvent( VolumeSliderValueChanged(), m_pInvokingPanel, flValue );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: When enter is pressed, dismisses dialog. Don't want on activate as mouse clicking does not dismiss (only clicks on background)
//-----------------------------------------------------------------------------
bool CVolumeSliderPopup::OnKeyDown( const KeyData_t &unichar )
{
	bool bEnter = (unichar.m_KeyCode == KEY_ENTER || unichar.m_KeyCode == KEY_PAD_ENTER);
	if ( unichar.m_bFirstDown && bEnter )
	{
		Close();
		return true;
	}
	
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CMovieVideoQualityPopup::CMovieVideoQualityPopup( CPanel2D *pInvokingPanel, const char *pchPanelID ) : CMovieControlPopupBase( pInvokingPanel, pchPanelID )
{
	RegisterEventHandler( MoviePlayerSetRepresentation(), this, &CMovieVideoQualityPopup::EventSetRepresentation );
}


//-----------------------------------------------------------------------------
// Purpose: Adds a button for the specified representation
//-----------------------------------------------------------------------------
void CMovieVideoQualityPopup::AddRepresentation( int iRep, int nHeight )
{
	Representation_t &rep = m_vecRepresentations[m_vecRepresentations.AddToTail()];
	rep.m_iRep = iRep;
	rep.m_nHeight = nHeight;	
}


//-----------------------------------------------------------------------------
// Purpose: Sorts representations highest to lowest
//-----------------------------------------------------------------------------
bool CMovieVideoQualityPopup::SortRepresentations( const Representation_t &lhs, const Representation_t &rhs )
{
	if ( lhs.m_nHeight != rhs.m_nHeight )
		return lhs.m_nHeight > rhs.m_nHeight;

	return lhs.m_iRep < rhs.m_iRep;
}


//-----------------------------------------------------------------------------
// Purpose: Shows panel
//-----------------------------------------------------------------------------
void CMovieVideoQualityPopup::Show( int iFocusedRep, int nVideoHeight )
{
	CPanel2D *pWrapper = new CPanel2D( m_pPopupBackground, "CenterColumn" );

#if !defined( SOURCE2_PANORAMA_FIXME )
	m_vecRepresentations.Sort( &SortRepresentations );
#endif

	if ( m_vecRepresentations.Count() > 1 )
	{
		FOR_EACH_VEC( m_vecRepresentations, i )
		{
			Representation_t &rep = m_vecRepresentations[i];
			CButton *pButton = new CButton( pWrapper, NULL );
			pButton->SetOnActivateEvent( MoviePlayerSetRepresentation::MakeEvent( this, rep.m_iRep ) );

			CLabel *pLabel = new CLabel( pButton, NULL );
			SetVideoResolutionText( pLabel, rep.m_nHeight );

			if ( rep.m_iRep == iFocusedRep )
				pButton->SetFocus();
		}
	}

	// add an auto button
	CButton *pButton = new CButton( pWrapper, NULL );
	pButton->SetOnActivateEvent( MoviePlayerSetRepresentation::MakeEvent( this, k_nInvalidVideoRepresentation ) );

	CLabel *pLabel = new CLabel( pButton, NULL );
	if ( nVideoHeight > 0 )
	{
		pLabel->SetText( "#Movie_Auto_Resolution" );
		pLabel->SetDialogVariable( "resolution", CFmtStr( "%dp", nVideoHeight ).String() );
	}
	else
	{
		pLabel->SetText( "#Movie_Auto" );
	}

	if ( !BHasDescendantKeyFocus() )
		pButton->SetFocus();

	InvalidateSizeAndPosition();
}


//-----------------------------------------------------------------------------
// Purpose: Closes panel and dispatches SetRepresentation event to movie panel
//-----------------------------------------------------------------------------
bool CMovieVideoQualityPopup::EventSetRepresentation( int iRep )
{
	DispatchEventAsync( 0.0, MoviePlayerSetRepresentation(), m_pInvokingPanel, iRep );
	Close();

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CMovieDebug::CMovieDebug( CPanel2D *pParent, const char *pchID ) : CPanel2D( pParent, pchID ), m_scheduledUpdate( MAKE_SCHEDULED_FUNC( CMovieDebug::Update ) )
{
	DbgVerify( BLoadLayout( "file://{resources}/layout/moviedebug.xml", true ) );

	m_pDimensions = assert_cast<CLabel*>(FindChildInLayoutFile( "Dimensions" ));
	m_pResolution = assert_cast<CLabel*>(FindChildInLayoutFile( "Resolution" ));
	m_pVideoSegment = assert_cast<CLabel*>(FindChildInLayoutFile( "VideoSegment" ));
	m_pVideoBandwidth = assert_cast<CLabel*>(FindChildInLayoutFile( "VideoBandwidth" ));
}


//-----------------------------------------------------------------------------
// Purpose: Sets video to show
//-----------------------------------------------------------------------------
void CMovieDebug::Show( CVideoPlayerPtr pVideoPlayer )
{
	m_pVideoPlayer = pVideoPlayer;
	Update();
	m_scheduledUpdate.Schedule( 0.1f );
}


//-----------------------------------------------------------------------------
// Purpose: Updates video display and schedules update
//-----------------------------------------------------------------------------
void CMovieDebug::Update()
{	
	m_pDimensions->SetText( CFmtStr( "%dx%d", (int)GetActualRenderWidth(), (int)GetActualRenderHeight() ) );	

	int nWidth = 0;
	int nHeight = 0;
	int nRepresentationCur = 0;
	int nRepresentationTotal = 0;
	int nSegmentCur = 0;
	int nSegmentTotal = 0;
	int nVideoDownloadRate = 0;
	if ( m_pVideoPlayer.IsValid() )
	{
		m_pVideoPlayer->GetVideoResolution( &nWidth, &nHeight );
		nRepresentationCur = m_pVideoPlayer->GetCurrentVideoRepresentation();
		nRepresentationTotal = m_pVideoPlayer->GetVideoRepresentationCount();
		m_pVideoPlayer->GetVideoSegmentInfo( &nSegmentCur, &nSegmentTotal );
		nVideoDownloadRate = m_pVideoPlayer->GetVideoDownloadRate();
	}

	m_pResolution->SetText( CFmtStr( "%dx%d", nWidth, nHeight ) );

	nVideoDownloadRate *= 8;
	const char *pchRateUnit = "";
	if ( nVideoDownloadRate > k_nMillion )
	{
		pchRateUnit = "M";
		nVideoDownloadRate /= k_nMillion;
	}
	else if ( nVideoDownloadRate > k_nThousand )
	{
		pchRateUnit = "K";
		nVideoDownloadRate /= k_nThousand;
	}

	m_pVideoBandwidth->SetText( CFmtStr( "%s%sbps", V_pretifynum( nVideoDownloadRate ), pchRateUnit ) );

	// -1 means complete
	if ( nRepresentationCur == -1 && nSegmentCur == -1 )
		m_pVideoSegment->SetText( "complete" );
	else
		m_pVideoSegment->SetText( CFmtStr( "Rep=%d/%d, Seg=%d/%d", nRepresentationCur + 1, nRepresentationTotal, nSegmentCur + 1, nSegmentTotal ) );

	m_scheduledUpdate.Schedule( 1.0f );
}
