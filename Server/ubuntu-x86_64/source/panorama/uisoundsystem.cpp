//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx.h"
#include "uisoundsystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

namespace panorama
{
	DEFINE_PANORAMA_EVENT( StopAudioSample );

	// volume settings for playback
	ConVar s_convarVolumeMaster( "@panorama_volume_master", "1.0f" );
	ConVar s_convarVolumeAmbient( "@panorama_volume_ambient", "0.48f" );
	ConVar s_convarVolumeEffects( "@panorama_volume_effects", "1.0f" );
	ConVar s_convarVolumeMovies( "@panorama_volume_movies", "1.0f" ); // not used directly in here, but used in movie.cpp
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CUISoundSystem::CUISoundSystem()
{
	m_flLastAudioInitTime = 0.0f;
	m_unCountBigMixAheadBuffers = 0;
	m_pAudioDevice = NULL;
	m_bSetDefaultAudioDevice = false;
	m_bPlayMainMenuMusic = false;

	StartupAudio();

	::RegisterForUnhandledEvent( StopAudioSample(), this, &CUISoundSystem::OnStopAudioSample );
	::RegisterForUnhandledEvent( PlaySoundEffect(), this, &CUISoundSystem::OnPlaySoundEffect );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CUISoundSystem::~CUISoundSystem()
{
	::UnregisterForUnhandledEvent( StopAudioSample(), this, &CUISoundSystem::OnStopAudioSample );
	::UnregisterForUnhandledEvent( PlaySoundEffect(), this, &CUISoundSystem::OnPlaySoundEffect );

	ShutdownAudio();
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CUISoundSystem::StartupAudio()
{
#if SUPPORTS_AUDIO
	if( m_pAudioDevice == NULL )
	{
		m_pAudioDevice = CreateSoundAudioDevice( 44100, 16, 2);
		if ( !m_pAudioDevice )
			return;		

		if( Plat_IsSteamOS() )
		{
			AudioSinkList()->Start();
			AudioSinkList()->AddListener( this );
		}
	}

	m_flLastAudioInitTime = UIEngine()->GetCurrentFrameTime();
#endif
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CUISoundSystem::UnpauseAudioIfNeeded()
{
#if SUPPORTS_AUDIO
	if ( !m_pAudioDevice )
	{
		StartupAudio();
	}

	if ( m_pAudioDevice )
	{
		m_pAudioDevice->UnpauseIfNeeded( UIEngine()->GetCurrentFrameTime() );

		if (m_unCountBigMixAheadBuffers > 0)
		{
			SetAudioMixFragmentMilliseconds(160);
		}
		else
		{
			SetAudioMixFragmentMilliseconds(76);
		}
	}

	m_flLastAudioInitTime = UIEngine()->GetCurrentFrameTime();

#endif
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CUISoundSystem::PauseAudio()
{
#if SUPPORTS_AUDIO
	// what to do with the audiostreams?

	if ( m_pAudioDevice )
	{
		m_pAudioDevice->Pause();
	}
#endif
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CUISoundSystem::ConsiderPausingAudio()
{
	if ( UIEngine()->GetCurrentFrameTime() - m_flLastAudioInitTime > 1.0f )
	{
		PauseAudio();
	}
}

void CUISoundSystem::SetPlayMainMenuMusic( bool new_value )
{
	m_bPlayMainMenuMusic = new_value;
}

bool CUISoundSystem::GetPlayMainMenuMusic()
{
	return m_bPlayMainMenuMusic;
}

bool CUISoundSystem::GetMusicUseHRTFEffect()
{
	return false;
}

void CUISoundSystem::SetMusicUseHRTFEffect(bool new_value)
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CUISoundSystem::ShutdownAudio()
{
#if SUPPORTS_AUDIO
	if( Plat_IsSteamOS() )
	{
		AudioSinkList()->RemoveListener( this );
	}

	if( AudioSinkList()->BStarted() )
	{
		AudioSinkList()->Stop();
	}

	if ( m_pAudioDevice )
	{
		FreeAudioDevice( m_pAudioDevice );
		m_pAudioDevice = NULL;
	}
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Fade out and stop a sound file
//-----------------------------------------------------------------------------
void CUISoundSystem::FadeOutAndStopSoundSample( HAUDIOSAMPLE hSample, float flFadeOutSeconds )
{
#ifdef SUPPORTS_AUDIO
	IAudioSound *pSound = (IAudioSound *)hSample;
	bool bValid = m_pAudioDevice && pSound ? m_pAudioDevice->BAudioSoundIsValid( pSound ) : false;
	if ( !bValid )
		return;

	pSound->FadeOut( flFadeOutSeconds );
#endif

	UIEngine()->DispatchEventAsync( flFadeOutSeconds + 0.2, StopAudioSample::MakeEvent( NULL, hSample ) );
}


//-----------------------------------------------------------------------------
// Purpose: Volume ramp a sound file from it's current volume ramp (starts at 1.0) to a new one.  This is a filter,
// so it scales on top of the base volume of the sample, you must still have the base volume set audible for this to work.
//-----------------------------------------------------------------------------
void CUISoundSystem::VolumeRampSoundSample( HAUDIOSAMPLE hSample, float flVolumeTarget, float flTransitionSeconds )
{
#ifdef SUPPORTS_AUDIO
	IAudioSound *pSound = (IAudioSound *)hSample;
	bool bValid = m_pAudioDevice && pSound ? m_pAudioDevice->BAudioSoundIsValid( pSound ) : false;
	if ( !bValid )
		return;

	pSound->VolumeRamp( flVolumeTarget, flTransitionSeconds );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Set volume and panning for sound file
//-----------------------------------------------------------------------------
void CUISoundSystem::SetSoundSampleVolumePan( HAUDIOSAMPLE hSample, float flVolume, float flVolumePan )
{
#ifdef SUPPORTS_AUDIO
	IAudioSound *pSound = (IAudioSound *)hSample;
	bool bValid = m_pAudioDevice && pSound ? m_pAudioDevice->BAudioSoundIsValid( pSound ) : false;
	if ( !bValid )
		return;

	float flMasterVolume = clamp( s_convarVolumeMaster.GetFloat(), 0.0f, 1.0f );
	pSound->SetVolumePan( flMasterVolume*flVolume, flVolumePan );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Handles stopping an audio sample
//-----------------------------------------------------------------------------
bool CUISoundSystem::OnStopAudioSample( HAUDIOSAMPLE hSample )
{
#ifdef SUPPORTS_AUDIO
	if ( m_pAudioDevice )
	{
		m_pAudioDevice->FreeAudioSound( (IAudioSound *)hSample );
	}
#endif

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Event handler for playing a sound effect
//-----------------------------------------------------------------------------
bool CUISoundSystem::OnPlaySoundEffect( const CPanelPtr< IUIPanel > &pPanel, const char *pchSoundEffect, const char* panelID )
{
	IUIPanel* pUIPanel;

	if ( UIEngine()->IsValidPanelPointer( pPanel.Get() ) )
	{
		pUIPanel = pPanel->FindChild( panelID );
		if ( pUIPanel == nullptr )
		{
			pUIPanel = pPanel.Get();
		}
	}
	else
	{
		pUIPanel = NULL;
	}

	PlaySound( pchSoundEffect, pUIPanel, k_ESoundType_Effects, 1.0f, 0.5f, 1.0f );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Normally you don't need to use this, PlaySound auto-applies the
// right volume; it is needed on raw audio streams created with CreateAudioOutputStream.
//-----------------------------------------------------------------------------
float CUISoundSystem::GetSoundVolume( ESoundType soundType )
{
	if ( s_convarVolumeMaster.GetFloat() == 0.0f )
	{
		// we are muted
		return 0.0f;
	}

	switch ( soundType )
	{
	default:
		AssertMsg1( false, "unknown sound type %d", soundType );
		// fall through
	case k_ESoundType_Passthrough:
		return 1.0f;
	case k_ESoundType_Ambient:
		return clamp( s_convarVolumeAmbient.GetFloat(), 0.0f, 1.0f );
	case k_ESoundType_Effects:
		return clamp( s_convarVolumeEffects.GetFloat(), 0.0f, 1.0f );
	case k_ESoundType_Movies:
		return clamp( s_convarVolumeMovies.GetFloat(), 0.0f, 1.0f );

	}
}


//-----------------------------------------------------------------------------
// Purpose: global mute toggle for panorama sounds
//-----------------------------------------------------------------------------
void CUISoundSystem::SetSoundMuted( bool bMute )
{
	s_convarVolumeMaster.SetValue( bMute ? 0.0f : 1.0f );
	::DispatchEvent( SoundMuteChanged(), (IUIPanel*)NULL, bMute );
}


//-----------------------------------------------------------------------------
// Purpose: Normally you don't need to use this, PlaySound auto-applies the
// right volume; it is needed on raw audio streams created with CreateAudioOutputStream.
//-----------------------------------------------------------------------------
void CUISoundSystem::SetSoundVolume( ESoundType soundType, float flVolume )
{
	switch ( soundType )
	{
	default:
		AssertMsg1( false, "unknown sound type %d", soundType );
		return;

	case k_ESoundType_Passthrough:
		AssertMsg( false, "cannot set volume for passthrough" );
		break;
	case k_ESoundType_Ambient:
		s_convarVolumeAmbient.SetValue( flVolume );
		break;
	case k_ESoundType_Effects:
		s_convarVolumeEffects.SetValue( flVolume );
		break;
	case k_ESoundType_Movies:
		s_convarVolumeMovies.SetValue( flVolume );
		break;
	}

	// should only get here if type is known

	// call GetSoundVolume here to honor muting
	flVolume = clamp( flVolume, 0.0f, 1.0f );
	::DispatchEvent( SoundVolumeChanged(), (IUIPanel*)NULL, soundType, flVolume );
}


//-----------------------------------------------------------------------------
// Purpose: play a sound file
//-----------------------------------------------------------------------------
void *CUISoundSystem::PlaySound( const char *pchSoundName, IUIPanel* pUIPanel , ESoundType soundType,
		float flVolume /*= 1.0*/, float flVolumePan /*= 0.5*/, float flRepeats /*= 1.0 */, const Vector2D* pSoundPosition /*=NULL*/ )
{
	if ( !pchSoundName )
		return NULL;

	static const char *s_arrSuffixes[] = { ".wav", ".mp3" };
	for ( const char *pchSuffix : s_arrSuffixes )
	{
		CFmtStr strFilePath( "file://{sounds}/%s%s", pchSoundName, pchSuffix );
		CFileResource file( strFilePath.Get() );

		if ( !UIEngine()->UIFileSystem()->FileExists( file.GetReferencePath() ) )
			continue;

#ifdef SUPPORTS_AUDIO
		UnpauseAudioIfNeeded();

		if ( !m_pAudioDevice )
			return NULL;

		IAudioSound *pSound = m_pAudioDevice->CreateAudioSound( file.GetReferencePath().String(), true, false );
		if ( !pSound )
			return NULL;

		pSound->Play( flRepeats );

		float flMasterVolume = clamp( s_convarVolumeMaster.GetFloat(), 0.0f, 1.0f );

		// start at the master volume
		float volume = flMasterVolume;

		// multiply by the volume for the specified kind of sound
		volume *= GetSoundVolume( soundType );

		// multiply by the specified volume passed in
		volume *= flVolume;

		pSound->SetVolumePan( volume, flVolumePan );

		if ( flRepeats != 0.0f )
		{
			UIEngine()->DispatchEventAsync( pSound->GetSampleDurationRemaining(), SoundFinished::MakeEvent( NULL, pchSoundName, (HAUDIOSAMPLE)pSound) );
		}

		return pSound;
#else
		return NULL;
#endif
	}

	Msg( "PlaySound attempted to play a sound that doesn't exist: %s\n", pchSoundName );
	return NULL;
}

bool CUISoundSystem::IsSoundStillPlaying(HAUDIOSAMPLE sample)
{
	//not implemented
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Creates an audio output stream
//-----------------------------------------------------------------------------
IAudioOutputStream *CUISoundSystem::CreateAudioOutputStream( int nRate, int nChannels, int nBits )
{
#ifdef SUPPORTS_AUDIO
	IAudioOutputStream *pRet = NULL;

	UnpauseAudioIfNeeded();
	if ( m_pAudioDevice )
		pRet = m_pAudioDevice->CreateAudioOutputStream( nRate, nChannels, nBits );

	return pRet;
#else
	return nullptr;
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Frees an audio output stream
//-----------------------------------------------------------------------------
void CUISoundSystem::FreeAudioOutputStream( IAudioOutputStream *pStream )
{
#ifdef SUPPORTS_AUDIO
	if( m_pAudioDevice )
		m_pAudioDevice->FreeAudioOutputStream( pStream );
#endif
}


//-----------------------------------------------------------------------------
// Purpose:  Push that the system now requires a larger mix ahead buffer to prevent skipping, "large" is 
// somewhat undefined at this level, but you are trading off latency on sounds beginning to get a bigger 
// buffer pre-mixed by miles to avoid skipping/stuttering.
//-----------------------------------------------------------------------------
void CUISoundSystem::PushAudioBigMixAheadBuffer()
{
#ifdef SUPPORTS_AUDIO
	if ( m_unCountBigMixAheadBuffers == 0 )
	{
		SetAudioMixFragmentMilliseconds( 160 );
	}
#endif
	++m_unCountBigMixAheadBuffers;

}


//-----------------------------------------------------------------------------
// Purpose: Pop that the system now requires a larger mix ahead buffer to prevent skipping, 
// "large" is somewhat undefined at this level, but you are trading off latency on sounds 
// beginning to get a bigger buffer pre-mixed by miles to avoid skipping/stuttering.  
// Popping moves back towards the lower latency setup once nothing needs the no-skipping
// larger buffer setup.
//-----------------------------------------------------------------------------
void CUISoundSystem::PopAudioBigMixAheadBuffer()
{
	--m_unCountBigMixAheadBuffers;
#ifdef SUPPORTS_AUDIO
	if ( m_unCountBigMixAheadBuffers == 0 )
	{
		SetAudioMixFragmentMilliseconds( 76 );
	}
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Try to give time to miles to service audio mixing
//-----------------------------------------------------------------------------
void CUISoundSystem::ServiceAudio()
{
	VPROF_BUDGET( "CUISoundSystem::ServiceAudio", VPROF_BUDGETGROUP_TENFOOT );
#ifdef SUPPORTS_AUDIO
	if( m_pAudioDevice  )
	{
		::ServiceAudio(); // bugbug stefan - is this really necessary ?
		m_pAudioDevice->ServiceAudioDevice();
	}
#endif
}


//-----------------------------------------------------------------------------
// Purpose: the audio device has become available, check if we need to set the default
//-----------------------------------------------------------------------------
#if !defined( SOURCE2_PANORAMA )
void CUISoundSystem::AudioSinkAvailabilityChanged( bool bAvailable )
{
	Assert( Plat_IsSteamOS() );
	if ( !m_bSetDefaultAudioDevice &&  bAvailable )
	{
		m_bSetDefaultAudioDevice = true;

		CUtlString sDefaultDevice;
		CUtlString sDefaultPort;
		CUtlString sDefaultProfile;
		UIEngine()->UISettings()->GetDefaultAudioDevice( sDefaultDevice, sDefaultPort, sDefaultProfile );

		if ( sDefaultDevice.IsEmpty() )
			sDefaultDevice = AudioSinkList()->DetermineBestDefaultDevice();

		if ( !sDefaultDevice.IsEmpty() )
		{
			AudioSinkList()->SetDefaultDevice( sDefaultDevice, sDefaultPort, sDefaultProfile );
		}

		UIEngine()->UISettings()->SetDefaultAudioDevice( sDefaultDevice );

		 CUtlString sDefaultVoiceDevice;
		 UIEngine()->UISettings()->GetDefaultVoiceDevice( sDefaultVoiceDevice );
		 if ( !sDefaultVoiceDevice.IsEmpty() )
			 AudioSinkList()->SetDefaultVoiceDevice( sDefaultVoiceDevice );

		 AudioSinkList()->UseSystemAudioSinkForVoice( panorama::UIEngine()->UISettings()->GetUseSystemAudioForVoice() );
	}
}
#endif



