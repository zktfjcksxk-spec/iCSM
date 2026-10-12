#ifndef INCLUDED_ISOUNDOPSYSTEM_H
#define INCLUDED_ISOUNDOPSYSTEM_H
//================ Copyright (c) Valve Corporation. All Rights Reserved. ===========================
//
//
//
//==================================================================================================

#include "tier0/platform.h"


typedef float CSosFieldData;
typedef float CSosFieldDataFloat;
typedef Vector2D CSosFieldDataFloat2;

typedef int HSOUNDEVENTHASH;
typedef int sound_voice_layer_t;

#ifndef SOUND_FROM_WORLD
#define SOUND_FROM_WORLD 0
#endif
#ifndef SOUND_FROM_LOCAL_PLAYER
#define SOUND_FROM_LOCAL_PLAYER -1
#endif

#define VOICE_LAYER_GAME 0
#define VOICE_LAYER_UI 1

typedef int CEntityIndex;

#define INVALID_SOSRANDSEED 0
typedef int SOSRANDSEED;

#define INVALID_SOUNDEVENT_GUID -1

class ISoundOpSystem
{
public:
	virtual SoundEventGuid_t StartSoundEvent( const char *pSoundEventName, void* pUIPanel, CEntityIndex nSourceEntityIndex = SOUND_FROM_WORLD, sound_voice_layer_t nLayer = VOICE_LAYER_GAME, SOSRANDSEED nSeed = INVALID_SOSRANDSEED, const void *pPackedFieldData = NULL, int nPackedFieldDataBytes = 0, const Vector2D* pSoundPos = nullptr ) = 0;
	virtual SoundEventGuid_t StartSoundEvent( HSOUNDEVENTHASH hSoundEventHash, CEntityIndex nSourceEntityIndex = SOUND_FROM_WORLD, sound_voice_layer_t nLayer = VOICE_LAYER_GAME, SOSRANDSEED nSeed = INVALID_SOSRANDSEED, const char *pClassName = NULL, const void *pPackedFieldData = NULL, int nPackedFieldDataBytes = 0, SoundEventGuid_t nGuid = INVALID_SOUNDEVENT_GUID ) = 0;
//
//	// stop
	virtual bool StopSoundEvent( SoundEventGuid_t nGuid ) = 0;
//	virtual bool StopSoundEvent( HSOUNDEVENTHASH nSoundEventHash, CEntityIndex entIndex = INVALID_ENTITYINDEX ) = 0;
//	virtual bool StopSoundEvent( const char *pSoundEventName, CEntityIndex entIndex = INVALID_ENTITYINDEX ) = 0;
//
//	// set soundevent param and field
//	virtual bool SetSoundEventField( SoundEventGuid_t nGuid, HSOSOPFIELDHASH hOpFieldHash, const CSosFieldData &fieldData, short nIndex = 0 ) = 0;
//	virtual bool SetSoundEventFields( SoundEventGuid_t nGuid, const void *pPackedFieldData, int nPackedFieldDataBytes ) = 0;
	virtual bool SetSoundEventParam( SoundEventGuid_t nGuid, const char *pParameterName, const CSosFieldData &fieldData, short nIndex = 0 ) = 0;
	virtual bool SetSoundEventParam( SoundEventGuid_t nGuid, const char *pParameterName, const CSosFieldDataFloat2 &fieldData, short nIndex = 0 ) = 0;

	virtual bool IsSoundStillPlaying( SoundEventGuid_t nGuid ) const = 0;
	
};

extern ISoundOpSystem* g_pSoundOpSystem;

#endif // INCLUDED_ISOUNDOPSYSTEM_H