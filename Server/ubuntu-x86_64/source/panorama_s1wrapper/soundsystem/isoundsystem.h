#ifndef INCLUDED_ISOUNDSYSTEM_H
#define INCLUDED_ISOUNDSYSTEM_H
//================ Copyright (c) Valve Corporation. All Rights Reserved. ===========================
//
//
//
//==================================================================================================

#include "engine/IEngineSound.h"

struct SoundEventGuid_t
{
	int m_nGuid;

	SoundEventGuid_t( int i ) 
	{ 
		m_nGuid = i; 
	}

	SoundEventGuid_t() 
	{
		m_nGuid = -1;
	}

	int GetRaw() { return m_nGuid; }
	void SetRaw( int nGuid ) { m_nGuid = nGuid; }

	operator int& () { return m_nGuid; }
};

abstract_class ISoundSystem
{
public:
// 	virtual void		Update( float time ) = 0;
// 	virtual void		Flush( void ) = 0;
// 
// 	virtual CAudioSource *FindOrAddSound( const char *filename ) = 0;
// 	virtual CAudioSource *LoadSound( const char *wavfile ) = 0;
// 
// 	virtual void		PlaySound( CAudioSource *source, float volume, CAudioMixer **ppMixer ) = 0;
// 
// 	virtual bool		IsSoundPlaying( CAudioMixer *pMixer ) = 0;
// 	virtual CAudioMixer *FindMixer( CAudioSource *source ) = 0;
// 
// 	virtual void		StopAll( void ) = 0;
// 	virtual void		StopSound( CAudioMixer *mixer ) = 0;
// 
// 	virtual void		GetAudioDevices( CUtlVector< audio_device_description_t >& deviceListOut ) const = 0;

	virtual IAudioOutputStream	*CreateOutputStream( uint nSampleRate, uint nChannels, uint nBits ) = 0;
	virtual void				DestroyOutputStream( IAudioOutputStream *pOutputStream ) = 0;
};

#define g_pSoundSystem g_pSoundSystem2
extern ISoundSystem* g_pSoundSystem;

#endif // INCLUDED_ISOUNDSYSTEM_H