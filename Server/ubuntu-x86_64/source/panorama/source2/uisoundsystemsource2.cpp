//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx.h"
#include "uisoundsystemsource2.h"
#include "soundsystem/isoundopsystem.h"
#include "soundsystem/isoundsystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;



//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CUISoundSystemSource2::CUISoundSystemSource2() : m_bPlayMainMenuMusic( false ), m_bMusicUseHRTFEffect( false )
{
	::RegisterForUnhandledEvent( PlaySoundEffect(), this, &CUISoundSystemSource2::OnPlaySoundEffect );
	::RegisterForUnhandledEvent( PlayMainMenuMusic(), this, &CUISoundSystemSource2::OnPlayMainMenuMusic );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CUISoundSystemSource2::~CUISoundSystemSource2()
{
	::UnregisterForUnhandledEvent( PlaySoundEffect(), this, &CUISoundSystemSource2::OnPlaySoundEffect );
	::UnregisterForUnhandledEvent( PlayMainMenuMusic(), this, &CUISoundSystemSource2::OnPlayMainMenuMusic );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CUISoundSystemSource2::ConsiderPausingAudio()
{
	// Unnecessary in Source 2
}

void CUISoundSystemSource2::SetPlayMainMenuMusic( bool new_value )
{
	m_bPlayMainMenuMusic = new_value;
}

bool CUISoundSystemSource2::GetPlayMainMenuMusic()
{
	return m_bPlayMainMenuMusic;
}


//-----------------------------------------------------------------------------
// Purpose: Fade out and stop a sound file
//-----------------------------------------------------------------------------
void CUISoundSystemSource2::FadeOutAndStopSoundSample( HAUDIOSAMPLE hSample, float flFadeOutSeconds )
{
	if ( !g_pSoundOpSystem )
		return;

	SoundEventGuid_t nGuid = ConvertHAUDIOSAMPLEToSoundEventGuid( hSample );
	if ( nGuid == INVALID_SOUNDEVENT_GUID )
		return;
	
	g_pSoundOpSystem->SetSoundEventParam( nGuid, "fade_out", CSosFieldDataFloat( flFadeOutSeconds ) );
	g_pSoundOpSystem->StopSoundEvent( nGuid );
}


//-----------------------------------------------------------------------------
// Purpose: Volume ramp a sound file from it's current volume ramp (starts at 1.0) to a new one.  This is a filter,
// so it scales on top of the base volume of the sample, you must still have the base volume set audible for this to work.
//-----------------------------------------------------------------------------
void CUISoundSystemSource2::VolumeRampSoundSample( HAUDIOSAMPLE hSample, float flVolumeTarget, float flTransitionSeconds )
{
	if ( !g_pSoundOpSystem )
		return;

	SoundEventGuid_t nGuid = ConvertHAUDIOSAMPLEToSoundEventGuid( hSample );
	if ( nGuid == INVALID_SOUNDEVENT_GUID )
		return;

	g_pSoundOpSystem->SetSoundEventParam( nGuid, "fade", CSosFieldDataFloat2( flVolumeTarget, flTransitionSeconds ) );
}


//-----------------------------------------------------------------------------
// Purpose: Set volume and panning for sound file
//-----------------------------------------------------------------------------
void CUISoundSystemSource2::SetSoundSampleVolumePan( HAUDIOSAMPLE hSample, float flVolume, float flVolumePan )
{
	if ( !g_pSoundSystem )
		return;

	SoundEventGuid_t nGuid = ConvertHAUDIOSAMPLEToSoundEventGuid( hSample );
	if ( nGuid == INVALID_SOUNDEVENT_GUID )
		return;

	g_pSoundOpSystem->SetSoundEventParam( nGuid, "volume_atten", CSosFieldDataFloat( flVolume ) );
	g_pSoundOpSystem->SetSoundEventParam( nGuid, "pan", CSosFieldDataFloat( flVolumePan ) );
}

//-----------------------------------------------------------------------------
// Purpose: Normally you don't need to use this, PlaySound auto-applies the
// right volume; it is needed on raw audio streams created with CreateAudioOutputStream.
//-----------------------------------------------------------------------------
float CUISoundSystemSource2::GetSoundVolume( ESoundType soundType )
{
	if ( soundType == k_ESoundType_Movies )
		return 1.0f;
	return 0.0f;
}


//-----------------------------------------------------------------------------
// Purpose: global mute toggle for panorama sounds
//-----------------------------------------------------------------------------
void CUISoundSystemSource2::SetSoundMuted( bool bMute )
{
	AssertMsg( false, "Shouldn't need this in Source 2" );
}


//-----------------------------------------------------------------------------
// Purpose: Normally you don't need to use this, PlaySound auto-applies the
// right volume; it is needed on raw audio streams created with CreateAudioOutputStream.
//-----------------------------------------------------------------------------
void CUISoundSystemSource2::SetSoundVolume( ESoundType soundType, float flVolume )
{
	AssertMsg( false, "Shouldn't need this in Source 2" );
}


//-----------------------------------------------------------------------------
// Purpose: play a sound file
//-----------------------------------------------------------------------------
void *CUISoundSystemSource2::PlaySound( const char *pchSoundName, IUIPanel* pUIPanel, ESoundType soundType,
		float flVolume /*= 1.0*/, float flVolumePan /*= 0.5*/, float flRepeats /*= 1.0 */, const Vector2D* pSoundPos /*= nullptr*/ )
{
	if ( !g_pSoundOpSystem )
		return NULL;

	SoundEventGuid_t nGuid = g_pSoundOpSystem->StartSoundEvent( pchSoundName, pUIPanel, SOUND_FROM_LOCAL_PLAYER, VOICE_LAYER_UI, INVALID_SOSRANDSEED, NULL, 0, pSoundPos );
	if ( nGuid == INVALID_SOUNDEVENT_GUID )
		return NULL;

	if ( flVolume != 1.0f )
	{
		g_pSoundOpSystem->SetSoundEventParam( nGuid, "volume_atten", CSosFieldDataFloat( flVolume ) );
	}
	if ( flVolumePan != 0.5f )
	{
		g_pSoundOpSystem->SetSoundEventParam( nGuid, "pan", CSosFieldDataFloat( flVolumePan ) );
	}
	if ( flRepeats != 1.0f )
	{
		g_pSoundOpSystem->SetSoundEventParam( nGuid, "repeats", CSosFieldDataFloat( flRepeats ) );
	}

	return ConvertSoundEventGuidToHAUDIOSAMPLE( nGuid );
}

bool CUISoundSystemSource2::IsSoundStillPlaying(HAUDIOSAMPLE sample)
{
	return g_pSoundOpSystem->IsSoundStillPlaying(ConvertHAUDIOSAMPLEToSoundEventGuid(sample));
}


//-----------------------------------------------------------------------------
// Purpose: Creates an audio output stream
//-----------------------------------------------------------------------------
IAudioOutputStream *CUISoundSystemSource2::CreateAudioOutputStream( int nRate, int nChannels, int nBits )
{
	return g_pSoundSystem->CreateOutputStream( nRate, nChannels, nBits );
}


//-----------------------------------------------------------------------------
// Purpose: Frees an audio output stream
//-----------------------------------------------------------------------------
void CUISoundSystemSource2::FreeAudioOutputStream( IAudioOutputStream *pStream )
{
	g_pSoundSystem->DestroyOutputStream( pStream );
}


//-----------------------------------------------------------------------------
// Purpose:  Push that the system now requires a larger mix ahead buffer to prevent skipping, "large" is 
// somewhat undefined at this level, but you are trading off latency on sounds beginning to get a bigger 
// buffer pre-mixed by miles to avoid skipping/stuttering.
//-----------------------------------------------------------------------------
void CUISoundSystemSource2::PushAudioBigMixAheadBuffer()
{
	// Unnecessary in Source 2
}


//-----------------------------------------------------------------------------
// Purpose: Pop that the system now requires a larger mix ahead buffer to prevent skipping, 
// "large" is somewhat undefined at this level, but you are trading off latency on sounds 
// beginning to get a bigger buffer pre-mixed by miles to avoid skipping/stuttering.  
// Popping moves back towards the lower latency setup once nothing needs the no-skipping
// larger buffer setup.
//-----------------------------------------------------------------------------
void CUISoundSystemSource2::PopAudioBigMixAheadBuffer()
{
	// Unnecessary in Source 2
}


//-----------------------------------------------------------------------------
// Purpose: Try to give time to miles to service audio mixing
//-----------------------------------------------------------------------------
void CUISoundSystemSource2::ServiceAudio()
{
	// Unnecessary in Source 2
}


//-----------------------------------------------------------------------------
// Purpose: the audio device has become available, check if we need to set the default
//-----------------------------------------------------------------------------
#if !defined( SOURCE2_PANORAMA )
void CUISoundSystemSource2::AudioSinkAvailabilityChanged( bool bAvailable )
{
	// Unnecessary in Source 2
}
#endif


//-----------------------------------------------------------------------------
// Purpose: Event handler for playing a sound effect
//-----------------------------------------------------------------------------
bool CUISoundSystemSource2::OnPlaySoundEffect( const CPanelPtr< IUIPanel > &pPanel, const char *pchSoundEffect, const char* panelID )
{
	if ( !UIEngine()->BIsRunning() )
	{
		//If the UI Engine is shutting down we don't play sound effects.
		return true;
	}

	IUIPanel* pUIPanel = nullptr;

	Vector2D mousePos;
	const Vector2D* pSoundPos = nullptr;
	if ( (panelID == nullptr || V_strcasecmp( panelID, "MOUSE" ) == 0) && pPanel.Get() != nullptr )
	{
		//Providing "MOUSE" as the panel ID makes us play sounds HRTF'd to the mouse position.
		pPanel->GetParentWindow()->UIWindowInput()->GetSurfaceMousePosition( mousePos.x, mousePos.y );
		pSoundPos = &mousePos;
		pUIPanel = pPanel.Get();
	}
	else if(panelID == nullptr && pPanel.Get())
	{
		// If no child panel ID is specified, just play this on the panel we fired the event to. 
		pUIPanel = pPanel.Get();
	}
	else if ( UIEngine()->IsValidPanelPointer( pPanel.Get() ) )
	{
		pUIPanel = pPanel->FindChildTraverse( panelID );
	}

	// Sound will fail to play if the above hasn't found a panel. 
	Assert( pUIPanel );

	PlaySound( pchSoundEffect, pUIPanel, k_ESoundType_Effects, 1.0f, 0.5f, 1.0f, pSoundPos );
	return true;
}

bool CUISoundSystemSource2::OnPlayMainMenuMusic( bool playing, bool use_hrtf_effect)
{
	if( use_hrtf_effect || !playing ) //we only force music to playing if using the hrtf effect.
	{
		SetPlayMainMenuMusic(playing);
	}

	m_bMusicUseHRTFEffect = use_hrtf_effect;
	return true;
}

bool CUISoundSystemSource2::GetMusicUseHRTFEffect()
{
	return m_bMusicUseHRTFEffect;
}

void CUISoundSystemSource2::SetMusicUseHRTFEffect(bool new_value)
{
	m_bMusicUseHRTFEffect = new_value;
}



static_assert( sizeof( SoundEventGuid_t ) <= sizeof( HAUDIOSAMPLE ), "Can't cast between SoundEventGuid_t and HAUDIOSAMPLE without losing information" );
static_assert( sizeof( int32 ) == sizeof( SoundEventGuid_t ), "When calling SetRaw, we first convert to an int32" );

/*static*/ HAUDIOSAMPLE CUISoundSystemSource2::ConvertSoundEventGuidToHAUDIOSAMPLE( SoundEventGuid_t nGuid )
{
	return reinterpret_cast< HAUDIOSAMPLE >( (intp)nGuid.GetRaw() );
}

/*static*/ SoundEventGuid_t CUISoundSystemSource2::ConvertHAUDIOSAMPLEToSoundEventGuid( HAUDIOSAMPLE hSample )
{
	SoundEventGuid_t nGuid;
	nGuid.SetRaw( static_cast< int32 >( reinterpret_cast< intptr_t >( hSample ) ) );
	return nGuid;
}
