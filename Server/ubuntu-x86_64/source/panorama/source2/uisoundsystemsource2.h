//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef UISOUNDSYSTEMSOURCE2_H
#define UISOUNDSYSTEMSOURCE2_H

#ifdef _WIN32
#pragma once
#endif

#include "iuisoundsystem.h"
#include "soundsystem/isoundsystem.h"

#if defined( SOURCE2_PANORAMA )
#define VERSION_SAFE_STEAM_API_INTERFACES
#endif


namespace panorama
{

//
// Base instance of UI engine, with non platform specific functionality
//
class CUISoundSystemSource2 : public IUISoundSystem
{
public:
	CUISoundSystemSource2();
	virtual ~CUISoundSystemSource2();

	virtual HAUDIOSAMPLE PlaySound( const char *pchURLPath, IUIPanel* pUIPanel, ESoundType soundType,
		float flVolume /*= 1.0*/, float flVolumePan /*= 0.5*/, float flRepeats /*= 1.0*/, const Vector2D* pSoundPos=nullptr );
	virtual bool IsSoundStillPlaying( HAUDIOSAMPLE sample );
	virtual void FadeOutAndStopSoundSample( HAUDIOSAMPLE hSample, float flFadeOutSeconds );
	virtual void VolumeRampSoundSample( HAUDIOSAMPLE hSample, float flVolumeTarget, float flTransitionSeconds );
	virtual void SetSoundSampleVolumePan( HAUDIOSAMPLE hSample, float flVolume, float flVolumePan );
	virtual float GetSoundVolume( ESoundType soundType );
	virtual void SetSoundVolume( ESoundType soundType, float flVolume );
	virtual void SetSoundMuted( bool bMute );

	virtual IAudioOutputStream *CreateAudioOutputStream( int nRate, int nChannels, int nBits );
	virtual void FreeAudioOutputStream( IAudioOutputStream *pStream );
	virtual void PushAudioBigMixAheadBuffer();
	virtual void PopAudioBigMixAheadBuffer();

	virtual void ServiceAudio();
	virtual void ConsiderPausingAudio();

	virtual void SetPlayMainMenuMusic( bool new_value );
	virtual bool GetPlayMainMenuMusic();

	virtual bool GetMusicUseHRTFEffect();
	virtual void SetMusicUseHRTFEffect(bool new_value);

#if !defined( SOURCE2_PANORAMA )
	// IAudioSinkDeviceListener impl
	virtual void AudioSinkAvailabilityChanged( bool bAvailable );
	// we don't care about the details of sinks adding and removing right now
	virtual void SinkAdded( int iSink ) {}
	virtual void SinkRemoved( int iSink ) {}
#endif

protected:
	bool OnPlaySoundEffect( const CPanelPtr< IUIPanel > &pPanel, const char *pchSoundEffect, const char* panelID  );
	bool OnPlayMainMenuMusic( bool play, bool use_hrtf_effect );

private:
	static HAUDIOSAMPLE ConvertSoundEventGuidToHAUDIOSAMPLE( SoundEventGuid_t nGuid );
	static SoundEventGuid_t ConvertHAUDIOSAMPLEToSoundEventGuid( HAUDIOSAMPLE hSample );

	bool m_bPlayMainMenuMusic;
	bool m_bMusicUseHRTFEffect;
};

} // namespace panorama

#endif // UISOUNDSYSTEMSOURCE2_H
