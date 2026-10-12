//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef UISOUNDSYSTEM_H
#define UISOUNDSYSTEM_H

#ifdef _WIN32
#pragma once
#endif

#include "iuisoundsystem.h"

#if defined( SOURCE2_PANORAMA )
#define VERSION_SAFE_STEAM_API_INTERFACES
#endif

class IAudioDevice;
class IAudioSound;


namespace panorama
{

DECLARE_PANORAMA_EVENT1( StopAudioSample, HAUDIOSAMPLE );

//
// Base instance of UI sound system which operates on file paths
//
class CUISoundSystem : public IUISoundSystem
#if !defined( SOURCE2_PANORAMA )
	, public IAudioSinkDeviceListener
#endif
{
public:
	CUISoundSystem();
	virtual ~CUISoundSystem();

	virtual HAUDIOSAMPLE PlaySound( const char *pchURLPath, IUIPanel* pUIPanel, ESoundType soundType,
		float flVolume /*= 1.0*/, float flVolumePan /*= 0.5*/, float flRepeats /*= 1.0*/, const Vector2D* pSoundPosition=nullptr );
	virtual bool IsSoundStillPlaying(HAUDIOSAMPLE sample);
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
	bool OnStopAudioSample( HAUDIOSAMPLE hSample );
	bool OnPlaySoundEffect( const CPanelPtr< IUIPanel > &pPanel, const char *pchSoundEffect, const char* panelID );

	void InitializeAudioIfNeeded();
	void CleanupAudio();

	void StartupAudio();
	void UnpauseAudioIfNeeded();
	void PauseAudio();
	void ShutdownAudio();

	double m_flLastAudioInitTime;
	
	// Audio interface
	IAudioDevice				   *m_pAudioDevice;

	// Count of big mix ahead buffer pushes
	uint32 m_unCountBigMixAheadBuffers;

	bool m_bSetDefaultAudioDevice;
	bool m_bPlayMainMenuMusic;

};

} // namespace panorama

#endif // UISOUNDSYSTEM_H
