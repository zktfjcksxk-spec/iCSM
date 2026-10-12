//============ Copyright © Valve Corporation, All rights reserved. ============//
//
// Purpose: manage audio output streams
//=============================================================================//

#pragma once

class IAudioOutputStream;
class CAudioMixBuffer;

// create/destroy an audio stream, these are exposed external to the soundsystem
IAudioOutputStream *AudioStreamCreate( uint nSampleRate, uint nChannels, uint nBits );
void AudioStreamDestroy( IAudioOutputStream *pStream );

// Mix all audio streams to the output mix buffers
void AudioStreamMix( uint nChannelCount, CAudioMixBuffer *pChannelArray );
// shut down all audio streams, report leaks
void AudioStreamShutdown();
