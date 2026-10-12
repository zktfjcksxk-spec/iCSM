//============ Copyright © Valve Corporation, All rights reserved. ============//
//
// Purpose: manage audio output streams
//=============================================================================//

#include "audio_pch.h"

#include "tier1/circularbuffer.h"
#include "../soundsystem/lowlevel/mix.h"
#include "engine/IEngineSound.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern CThreadMutex g_SndMutex;

class CCircularSampleBuffer
{
	CSizedCircularBuffer<8192 * sizeof( int16 )> m_bytes;
public:
	inline const int16 *Base() const { return (const int16 *)m_bytes.m_chData; }
	inline uint SampleCount() const { return m_bytes.m_nCount >> 1; }
	inline uint SampleStart() const { return m_bytes.m_nRead >> 1; }
	inline uint MaxSamples() const { return m_bytes.m_nSize >> 1; }

	int InitPacketsForChannel( audio_buffer_input_t *pPackets )
	{
		int nPacketCount = 1;
		uint nStart = SampleStart();
		uint nSampleCount = SampleCount();
		uint nLast = MaxSamples();

		const int16 *pSampleStart = Base();
		pPackets[ 0 ].m_pSamples = pSampleStart + nStart;
		pPackets[ 0 ].m_nSampleCount = nSampleCount;

		if ( nSampleCount + nStart > nLast )
		{
			pPackets[ 0 ].m_nSampleCount = nLast - nStart;

			pPackets[ 1 ].m_pSamples = pSampleStart;
			pPackets[ 1 ].m_nSampleCount = nSampleCount - pPackets[ 0 ].m_nSampleCount;
			nPacketCount = 2;
		}
		return nPacketCount;
	}

	void WriteSamples( const int16 *pSamples, uint nSampleCount )
	{
		m_bytes.Write( pSamples, nSampleCount * sizeof( int16 ) );
	}

	void AdvanceSamples( uint nSamples )
	{
		m_bytes.Advance( nSamples * sizeof( int16 ) );
	}

	uint AvailableSampleCount()
	{
		return m_bytes.GetReadAvailable() >> 1;
	}
};

class CAudioOutputStream : public IAudioOutputStream
{
	CCircularSampleBuffer *m_pBuffer;

	float m_flVolume;
	float m_flLastMixVolume;
	int m_nChannels;
	uint m_nSampleRate;
	uint m_nSampleFracOffset; // for resampling
	bool m_bPaused;

	CUtlVector<CAudioMixBuffer> m_mixBuffers;
	CThreadMutex m_mixLock;

public:
	CAudioOutputStream( int nChannels, uint nSampleRate )
		: m_nChannels( nChannels ), m_nSampleRate( nSampleRate )
	{
		m_pBuffer = new CCircularSampleBuffer[ nChannels ];
		m_flVolume = 1.0f;
		m_flLastMixVolume = 1.0f;
		m_nSampleFracOffset = 0; // for resampling
		m_bPaused = false;
	}

	~CAudioOutputStream()
	{
		delete[] m_pBuffer;
	}

	virtual void WriteAudioData( const int16 *pData, uint nSampleCount, uint nChannels ) OVERRIDE
	{
		AUTO_LOCK( m_mixLock );

		if ( m_nChannels == 1 )
		{
			m_pBuffer[ 0 ].WriteSamples( pData, nSampleCount );
		}
		else
		{
			CUtlVectorFixedGrowable<int16, 2048> samples;
			samples.SetCount( nSampleCount );

			// de-interleave
			for ( int i = 0; i < m_nChannels; i++ )
			{
				const int16 *pInput = (const int16 *)pData;
				pInput += i;
				for ( uint j = 0; j < nSampleCount; j++ )
				{
					samples[ j ] = *pInput;
					pInput += m_nChannels;
				}

				m_pBuffer[ i ].WriteSamples( samples.Base(), nSampleCount );
			}
		}
	}

	virtual void SetVolume( float flVolume ) OVERRIDE
	{
		m_flVolume = flVolume;
	}

	virtual uint32 QueuedSampleCount() OVERRIDE
	{
		AUTO_LOCK( m_mixLock );
		return m_pBuffer[ 0 ].AvailableSampleCount();
	}

	virtual uint32 MaxWriteSampleCount() OVERRIDE
	{
		AUTO_LOCK( m_mixLock );
		return m_pBuffer[ 0 ].MaxSamples() - m_pBuffer[0].SampleCount();
	}

	virtual uint32 LatencySamplesCount() OVERRIDE
	{
		return MIX_BUFFER_SIZE;
	}

	virtual void Pause() OVERRIDE
	{
		AUTO_LOCK( m_mixLock );
		m_bPaused = true;
	}

	virtual void Resume() OVERRIDE
	{
		AUTO_LOCK( m_mixLock );
		m_bPaused = false;
	}

	// builds a CAudioMixBuffer from the channel data.  Consumes data in the circular buffer 
	void BuildMixBufferForChannel( CAudioMixBuffer *pOut, audio_source_indexstate_t &indexState, CCircularSampleBuffer *pChannel )
	{
		audio_source_input_t inputData;
		audio_buffer_input_t packets[ 2 ];
		int nPacketCount = pChannel->InitPacketsForChannel( packets );
		Assert( packets[ 0 ].m_nSampleCount > 0 && indexState.m_nBufferSampleOffset < packets[ 0 ].m_nSampleCount );
		inputData.InitPackets( packets, nPacketCount, m_nSampleRate, 16, 1 );
		ConvertSourceToFloat( inputData, 1.0f, pOut->m_flData, &indexState );
		uint nReadCount = 0;
		for ( uint i = 0; i < indexState.m_nPacketIndex; i++ )
		{
			nReadCount += packets[ i ].m_nSampleCount;
		}
		nReadCount += indexState.m_nBufferSampleOffset;
		pChannel->AdvanceSamples( nReadCount );
	}

	void MixToOutput( int nChannels, CAudioMixBuffer *pChannelArray )
	{
		// don't add data while mixing this buffer
		AUTO_LOCK( m_mixLock );

		if ( m_bPaused )
			return;

		audio_source_indexstate_t indexState;
		indexState.Clear();
		indexState.m_nSampleFracOffset = m_nSampleFracOffset;

		if ( m_nChannels == 1 )
		{
			if ( m_pBuffer[ 0 ].AvailableSampleCount() > 0 )
			{
				CAudioMixBuffer temp;
				// extract data from circular buffer into temp
				BuildMixBufferForChannel( &temp, indexState, &m_pBuffer[ 0 ] );

				nChannels = MAX( nChannels, 2 );
				for ( int i = 0; i < nChannels; i++ )
				{
					// now mix it into the output
					MixBufferRamp( pChannelArray[ i ].m_flData, temp.m_flData, m_flLastMixVolume, m_flVolume );
				}
			}
		}
		else
		{
			nChannels = MIN( m_nChannels, nChannels );
			for ( int i = 0; i < nChannels; i++ )
			{
				if ( m_pBuffer[ i ].AvailableSampleCount() > 0 )
				{
					CAudioMixBuffer temp;
					indexState.Clear();
					indexState.m_nSampleFracOffset = m_nSampleFracOffset;
					// extract data from circular buffer into temp
					BuildMixBufferForChannel( &temp, indexState, &m_pBuffer[ i ] );

					// now mix it into the output
					MixBufferRamp( pChannelArray[ i ].m_flData, temp.m_flData, m_flLastMixVolume, m_flVolume );
				}
			}
		}

		m_nSampleFracOffset = indexState.m_nSampleFracOffset;
		m_flLastMixVolume = m_flVolume;
	}
};


// external API
static CUtlVector<CAudioOutputStream *> g_AudioStreamList;
void AudioStreamMix( uint nChannelCount, CAudioMixBuffer *pChannelArray )
{
	if ( g_AudioStreamList.Count() )
	{
		for ( int i = 0; i < g_AudioStreamList.Count(); i++ )
		{
			g_AudioStreamList[ i ]->MixToOutput( nChannelCount, pChannelArray );
		}
	}
}

IAudioOutputStream *AudioStreamCreate( uint nSampleRate, uint nChannels, uint nBits )
{
	Assert( nBits == 16 ); // TODO: Add 8-bit support?

	// to avoid adding or removing from the lsitwhen mixing we will just hold the main lock
	AUTO_LOCK( g_SndMutex );
	CAudioOutputStream *pNewStream = new CAudioOutputStream( nChannels, nSampleRate );
	g_AudioStreamList.AddToTail( pNewStream );

	return pNewStream;
}

void AudioStreamDestroy( IAudioOutputStream *pStream )
{
	// to avoid adding or removing from the lsitwhen mixing we will just hold the main lock
	AUTO_LOCK( g_SndMutex );
	g_AudioStreamList.FindAndFastRemove( static_cast<CAudioOutputStream *>(pStream) );
	delete pStream;
}


void AudioStreamShutdown()
{
	AUTO_LOCK( g_SndMutex );
	for ( auto pStream : g_AudioStreamList )
	{
		Log_Warning( LOG_CONSOLE, "Cleaning up leaked audio stream!\n" );
		delete pStream;
	}
}

