//================ Copyright (c) Valve Corporation. All Rights Reserved. ===========================
//
//
//
//==================================================================================================

#include "s1wrapper.h"


#include "panorama/iuipanel.h"
#include "panorama/iuiengine.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


IEngineSound *g_pEnginesound = NULL;
ISoundEmitterSystemBase *g_pSoundEmitterSystemBase = NULL;

//--------------------------------------------------------------------------------------------------
// Sound
//--------------------------------------------------------------------------------------------------


class CSoundOpSystem : ISoundOpSystem
{
public:
	virtual SoundEventGuid_t StartSoundEvent( const char *pSoundEventName, void* pUIPanel, CEntityIndex nSourceEntityIndex = SOUND_FROM_WORLD,
											  sound_voice_layer_t nLayer = VOICE_LAYER_GAME, SOSRANDSEED nSeed = INVALID_SOSRANDSEED,
											  const void *pPackedFieldData = NULL, int nPackedFieldDataBytes = 0, const Vector2D* pSoundPos = nullptr );
	virtual SoundEventGuid_t StartSoundEvent( HSOUNDEVENTHASH hSoundEventHash, CEntityIndex nSourceEntityIndex = SOUND_FROM_WORLD,
											  sound_voice_layer_t nLayer = VOICE_LAYER_GAME, SOSRANDSEED nSeed = INVALID_SOSRANDSEED,
											  const char *pClassName = NULL, const void *pPackedFieldData = NULL,
											  int nPackedFieldDataBytes = 0, SoundEventGuid_t nGuid = INVALID_SOUNDEVENT_GUID ) {
		return 0;
	}
	//
	//	// stop
	virtual bool StopSoundEvent( SoundEventGuid_t nGuid );
	//	virtual bool StopSoundEvent( HSOUNDEVENTHASH nSoundEventHash, CEntityIndex entIndex = INVALID_ENTITYINDEX ) = 0;
	//	virtual bool StopSoundEvent( const char *pSoundEventName, CEntityIndex entIndex = INVALID_ENTITYINDEX ) = 0;
	//
	//	// set soundevent param and field
	//	virtual bool SetSoundEventField( SoundEventGuid_t nGuid, HSOSOPFIELDHASH hOpFieldHash, const CSosFieldData &fieldData, short nIndex = 0 ) = 0;
	//	virtual bool SetSoundEventFields( SoundEventGuid_t nGuid, const void *pPackedFieldData, int nPackedFieldDataBytes ) = 0;
	virtual bool SetSoundEventParam( SoundEventGuid_t nGuid, const char *pParameterName,
									 const CSosFieldData &fieldData, short nIndex = 0 );
	virtual bool SetSoundEventParam( SoundEventGuid_t nGuid, const char *pParameterName,
									 const CSosFieldDataFloat2 &fieldData, short nIndex = 0 ) {
		return true;
	}

	virtual bool IsSoundStillPlaying( SoundEventGuid_t nGuid ) const;

};

CSoundOpSystem g_SoundOpSystem;
ISoundOpSystem* g_pSoundOpSystem = (ISoundOpSystem*)&g_SoundOpSystem;


class CDummyRecipientFilter : public IRecipientFilter
{
	virtual bool	IsReliable( void ) const { return false;  }
	virtual bool	IsInitMessage( void ) const { return false; }

	virtual int		GetRecipientCount( void ) const { return 1;	}
	virtual int		GetRecipientIndex( int slot ) const { return 1; }
};

ConVar s_convarPanoramaSoundPosShow( "@panorama_sound_pos_show", "0", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );
ConVar s_convarPanoramaSoundPos( "@panorama_sound_pos", "1", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );
ConVar s_convarPanoramaSoundPosDist( "@panorama_sound_pos_dist", "-2.0", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );

//--------------------------------------------------------------------------------------------------
SoundEventGuid_t CSoundOpSystem::StartSoundEvent( const char *pSoundEventName, void* pUIPanel, CEntityIndex nSourceEntityIndex,
	sound_voice_layer_t nLayer, SOSRANDSEED nSeed,
	const void *pPackedFieldData, int nPackedFieldDataBytes, const Vector2D* pSoundPos )
{
	// Prefix sound event name with "UIPanorama" if the sound event name does not have a prefix

	char szPrefixedSoundEventName[MAX_PATH];
	if ( !strchr( pSoundEventName, '.' ) )
	{
		V_sprintf_safe( szPrefixedSoundEventName, "UIPanorama.%s", pSoundEventName );
	}
	else
	{
		V_strcpy_safe( szPrefixedSoundEventName, pSoundEventName );
	}
	
	// Retrieve sound parameters from sound script

	static ConVarRef snd_show_events("snd_show_events");

	int nSoundIndex = g_pSoundEmitterSystemBase->GetSoundIndex( szPrefixedSoundEventName );
	if ( !g_pSoundEmitterSystemBase->IsValidIndex( nSoundIndex ) )
	{
		if(snd_show_events.GetBool())
		{
			DevMsg("SoundEvent: %s (doesn't exist)\n", szPrefixedSoundEventName);
		}

		return 0;
	}
	
	HSOUNDSCRIPTHASH nSoundEntryHash = SOUNDEMITTER_INVALID_HASH;
	gender_t gender = GENDER_NONE;
	CSoundParameters params;


	if ( !g_pSoundEmitterSystemBase->GetParametersForSoundEx( szPrefixedSoundEventName, nSoundEntryHash, params, gender, true ) )
	{
		if ( snd_show_events.GetBool() )
		{
			DevMsg("SoundEvent: %s (doesn't exist)\n", szPrefixedSoundEventName);
		}

		return 0;
	}

	if ( !params.soundname[0] )
	{
		if ( snd_show_events.GetBool() )
		{
			DevMsg("SoundEvent: %s (doesn't exist)\n", szPrefixedSoundEventName);
		}

		return 0;
	}

	// Play sound

	// Demo sound positions

	Vector vecOrigin( 1.0, 0.0, 0.0 );

	if ( s_convarPanoramaSoundPos.GetBool() && pUIPanel && ( pSoundPos != nullptr || panorama::UIEngine()->IsValidPanelPointer( ( panorama::IUIPanel* )pUIPanel ) ) )
	{
		panorama::IUIPanel* pPanel = (panorama::IUIPanel*)pUIPanel;

		float x, y;

		if ( pSoundPos != nullptr )
		{
			x = pSoundPos->x;
			y = pSoundPos->y;
		}
		else
		{
			x = pPanel->GetActualXOffset();
			y = pPanel->GetActualYOffset();
		
			for ( panorama::IUIPanel* pParent = pPanel->GetParent(); pParent != NULL; pParent = pParent->GetParent() )
			{
				x += pParent->GetActualXOffset();
				y += pParent->GetActualYOffset();
			}
		}

		int nW, nH;

		nW = pPanel->GetParentWindow()->GetSurfaceWidth();
		nH = pPanel->GetParentWindow()->GetSurfaceHeight();

		float x_normalized = (x - nW*0.5)/(nW*0.5);
		float y_normalized = (y - nH*0.5)/(nH*0.5);

		if ( x_normalized >= -1.0 && x_normalized <= 1.0 && y_normalized >= -1.0 && y_normalized <= 1.0 )
		{
			vecOrigin = Vector(s_convarPanoramaSoundPosDist.GetFloat(), -x_normalized, y_normalized);
			if ( s_convarPanoramaSoundPosShow.GetBool() )
			{
				Msg( ">>>Panel: %s Sound: %s postion x,y = %f, %f -> %f, %f\n", pPanel->GetID(), pSoundEventName, x, y, x_normalized, y_normalized );
			}
		} 
		else if ( s_convarPanoramaSoundPosShow.GetBool() )
		{
			Msg( ">>>Panel: Invalid position for Panel: %s Sound: %s at %f, %f", pPanel->GetID(), pSoundEventName, x_normalized, y_normalized);
		}
	}
	else if( s_convarPanoramaSoundPosShow.GetBool() )
	{
		Msg( ">>Panel: No UI element for panorama sound %s\n", pSoundEventName );
	}

	//set m_bUISound = true to tell EmitSound() to ignore the recipient filter and just play the sound locally.
	params.m_bUISound = true;
	CDummyRecipientFilter filter;
	int guid = g_pEnginesound->EmitSound( filter, nSourceEntityIndex, params.channel, szPrefixedSoundEventName, nSoundEntryHash, params.soundname,
		params.volume, (soundlevel_t)params.soundlevel, params.m_nRandomSeed, SND_IS_SCRIPTHANDLE, params.pitch, &vecOrigin, nullptr, nullptr, true, 0.0f, -1, &params );

	return ( ( guid > 0 ) ? SoundEventGuid_t(guid) : SoundEventGuid_t() );
}

//--------------------------------------------------------------------------------------------------
bool CSoundOpSystem::StopSoundEvent( SoundEventGuid_t nGuid )
{
	if ( nGuid != INVALID_SOUNDEVENT_GUID )
	{
		g_pEnginesound->StopSoundByGuid( nGuid.GetRaw() );
		return true;
	}
	return false;
}

//--------------------------------------------------------------------------------------------------
bool CSoundOpSystem::SetSoundEventParam( SoundEventGuid_t nGuid, const char *pParameterName,
	const CSosFieldData &fieldData, short nIndex )
{
	if ( ( nGuid != INVALID_SOUNDEVENT_GUID ) && ( !V_strcmp( pParameterName, "volume_atten" ) ) )
	{
		g_pEnginesound->SetVolumeByGuid( nGuid.GetRaw(), fieldData );
		return true;
	}
	return false;
}

bool CSoundOpSystem::IsSoundStillPlaying( SoundEventGuid_t nGuid ) const
{
	return g_pEnginesound->IsSoundStillPlaying( nGuid.GetRaw() );
}

class CSoundSystem : ISoundSystem
{
public:
	virtual IAudioOutputStream	*CreateOutputStream( uint nSampleRate, uint nChannels, uint nBits );
	virtual void				DestroyOutputStream( IAudioOutputStream *pOutputStream );
};

IAudioOutputStream	*CSoundSystem::CreateOutputStream( uint nSampleRate, uint nChannels, uint nBits )
{
	return g_pEnginesound->CreateOutputStream( nSampleRate, nChannels, nBits );
}

void CSoundSystem::DestroyOutputStream( IAudioOutputStream *pOutputStream )
{
	g_pEnginesound->DestroyOutputStream( pOutputStream );
}

CSoundSystem g_SoundSystem;
ISoundSystem* g_pSoundSystem = (ISoundSystem*)&g_SoundSystem;
