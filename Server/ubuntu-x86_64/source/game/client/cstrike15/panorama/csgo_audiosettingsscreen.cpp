//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "cbase.h"
#include "panorama/popups/csgo_settings_enum.h"
#include "csgo_audiosettingsscreen.h"
#include "IGameUIFuncs.h"
#include "interfaces/interfaces.h"
#include "soundsystem/isoundsystem.h"
#ifdef _WIN32
#include "dsound.h"
#endif

#include "csgo_avsettingsscreenbase.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

REGISTER_PANEL2D_FACTORY( CCSGO_AudioSettingsScreen, CSGOAudioSettings );

DECLARE_PANORAMA_EVENT0( CSGOSpeakerConfigurationChanged );
DEFINE_PANORAMA_EVENT( CSGOSpeakerConfigurationChanged );

DECLARE_PANORAMA_EVENT0(CSGOAudioDeviceConfigurationChanged);
DEFINE_PANORAMA_EVENT(CSGOAudioDeviceConfigurationChanged);


DECLARE_PANORAMA_EVENT0( CSGOVoiceEnable );
DEFINE_PANORAMA_EVENT( CSGOVoiceEnable );

DECLARE_PANORAMA_EVENT0( CSGOAudioSettingsResetDefault );
DEFINE_PANORAMA_EVENT( CSGOAudioSettingsResetDefault );

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CCSGO_AudioSettingsScreen::CCSGO_AudioSettingsScreen( CPanel2D *pParent, const char *pchID )
	: 
	CPanel2D( pParent, pchID )	
{
	// CSGOAudioSettings no longer loaded as separate xml
	// DbgVerify( BLoadLayout( "file://{resources}/layout/settings/settings_audio.xml" ) );

	RegisterEventHandler( panorama::PanelLoaded(), this, &CCSGO_AudioSettingsScreen::EventPanelLoaded );
	RegisterEventHandler( CSGOSpeakerConfigurationChanged(), this, &CCSGO_AudioSettingsScreen::EventSpeakerConfigurationChanged );
	RegisterEventHandler( CSGOAudioDeviceConfigurationChanged(), this, &CCSGO_AudioSettingsScreen::EventAudioDeviceConfigurationChanged);
	RegisterEventHandler( CSGOVoiceEnable(), this, &CCSGO_AudioSettingsScreen::EventVoiceSelectionChanged );
	RegisterForUnhandledEvent( CSGOAudioSettingsResetDefault(), this, &CCSGO_AudioSettingsScreen::EventAudioSettingsResetDefault );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CCSGO_AudioSettingsScreen::~CCSGO_AudioSettingsScreen()
{
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CCSGO_AudioSettingsScreen::EventPanelLoaded( const panorama::CPanelPtr< panorama::IUIPanel > &panelPtr )
{
	m_pSpeakerCfgDropDown = panorama::panel_cast< CCSGO_SettingsEnumDropDown* >( FindChildInLayoutFile( "SpeakerConfigurationEnum" ) );
	m_pVoiceEnableDropDown = panorama::panel_cast< CCSGO_SettingsEnumDropDown* >( FindChildInLayoutFile( "VoiceEnable" ) );
	m_pDeviceCfgDropDown = panorama::panel_cast<CCSGO_SettingsEnumDropDown*>(FindChildInLayoutFile("DeviceConfigurationEnum"));

	SetSpeakerConfigurationToCurrent();
	SetVoiceConfigurationToCurrent();
	SetAudioDevices();

	// This event was handled
	return true;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CCSGO_AudioSettingsScreen::EventSpeakerConfigurationChanged( void )
{
	UpdateEnhanceStereo();

	return true;
}

bool CCSGO_AudioSettingsScreen::EventAudioDeviceConfigurationChanged(void)
{
	return true;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CCSGO_AudioSettingsScreen::EventVoiceSelectionChanged( void )
{
	ConVarRef voice_modenable( "voice_modenable" );
	ConVarRef voice_enable( "voice_enable" );
	//TODO: Open mic doesn't work. We're chosing to disable it instead of fix. 
	ConVarRef voice_vox( "voice_vox" );

	int nIndex = DropdownGetSelectedValue( m_pVoiceEnableDropDown );

	switch ( nIndex )
	{
	case 0: // disabled
		voice_modenable.SetValue( 0 );
		voice_enable.SetValue( 0 );
		voice_vox.SetValue( 0 );
		break;

	case 1: // push to talk
		voice_modenable.SetValue( 1 );
		voice_enable.SetValue( 1 );
		voice_vox.SetValue( 0 );
		break;

	default:
		AssertMsg( false, "SetVoiceConfig index out of range." );
		break;
	}

	return true;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CCSGO_AudioSettingsScreen::EventAudioSettingsResetDefault( void )
{
	ConVarRef voice_modenable( "voice_modenable" );
	ConVarRef voice_enable( "voice_enable" );

	if ( voice_modenable.IsValid() )
	{
		voice_modenable.SetValue( voice_modenable.GetDefault() );
	}

	if ( voice_enable.IsValid() )
	{
		voice_enable.SetValue( voice_enable.GetDefault() );
	}

	SetVoiceConfigurationToCurrent();

	return true;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CCSGO_AudioSettingsScreen::SetSpeakerConfigurationToCurrent( void )
{
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CCSGO_AudioSettingsScreen::SetVoiceConfigurationToCurrent(  void )
{
	int nIndex = 0;

	ConVarRef voice_modenable( "voice_modenable" );
	ConVarRef voice_enable( "voice_enable" );

	bool bVoiceEnabled = voice_enable.GetBool() && voice_modenable.GetBool();

	if ( !bVoiceEnabled )
	{
		//disabled
		nIndex = 0;
	}
	else
	{
		// push to talk
		nIndex = 1;
	}

	DropdownSelectOptionByIndex( m_pVoiceEnableDropDown, nIndex );
}

void CCSGO_AudioSettingsScreen::SetAudioDevices(void)
{
	ConVarRef sound_device_override( "sound_device_override" );

	CUtlVector<audio_device_description_t> devices;

	if ( g_pSoundSystem != nullptr )
	{
		g_pSoundSystem->GetAudioDevices(devices);
	}

	if ( devices.Count() == 0 )
	{
		// No sound system on OSX and Linux (and perhaps some Windows setups?)
		// we don't show a device list, so find the panel and hide it.
		panorama::CPanel2D* configPanel = FindChildInLayoutFile("DeviceConfigurationPanel");
		if ( configPanel != nullptr )
		{
			configPanel->SetVisible( false );
		}

		return;
	}

	int selection = -1;
	for ( int i = 0; i < devices.Count() && i < m_pDeviceCfgDropDown->GetNumOptions(); ++i )
	{
		panorama::CPanel2D* panel = m_pDeviceCfgDropDown->GetOptionByIndex(i);
		panorama::CLabel* label = dynamic_cast<panorama::CLabel*>(panel);

		const audio_device_description_t& desc = devices[ i ];

		if ( desc.m_bIsDefault && selection == -1 )
		{
			selection = i;
		}

		char buf[1024];
		V_wcstostr( desc.m_deviceName, -1, buf, sizeof(buf) );
		if ( !V_stricmp( buf, sound_device_override.GetString() ) )
		{
			selection = i;
		}

		if ( label != nullptr )
		{
			label->SetAllowRawText( true ); // Text from OS driver.  TODO: Move to dialog var to avoid accidental localization and need to allow raw text
			label->SetText( desc.m_friendlyName );
			label->SetAttribute( "value", buf );
		}
	}

	if ( selection == -1 )
	{
		selection = 0;
	}

	m_pDeviceCfgDropDown->SetSelected( selection, false );

	for ( int i = 0; i < m_pDeviceCfgDropDown->GetChildCount(); ++i ) {
		panorama::CPanel2D* panel = m_pDeviceCfgDropDown->GetChild(i);
		panorama::CLabel* label = dynamic_cast<panorama::CLabel*>(panel);
		label->SetAllowRawText( true ); // Text from OS driver.  TODO: Move to dialog var to avoid accidental localization and need to allow raw text
		label->SetText ( devices[selection].m_friendlyName );
	}

	for ( int i = m_pDeviceCfgDropDown->GetNumOptions()-1; i >= devices.Count(); --i )
	{
		m_pDeviceCfgDropDown->RemoveOption( m_pDeviceCfgDropDown->GetOptionByIndex(i)->GetID() );
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CCSGO_AudioSettingsScreen::UpdateEnhanceStereo()
{
	// headphones at high quality get enhanced stereo turned on
	SplitScreenConVarRef Snd_PitchQuality( "Snd_PitchQuality" );
	SplitScreenConVarRef dsp_slow_cpu( "dsp_slow_cpu" );
	SplitScreenConVarRef snd_surround_speakers("Snd_Surround_Speakers");
	SplitScreenConVarRef dsp_enhance_stereo( "dsp_enhance_stereo" );

#if defined(LINUX)
		ConVarRef sdl_speaker_channels( "sdl_speaker_channels" );
		int nChannels = 2;
		switch ( snd_surround_speakers.GetInt( 0 ) )
		{
		case 0:
			//headphones
			nChannels = 1;
			break;
		case 2:
			//two speakers
			nChannels = 2;
			break;
		case 4:
			nChannels = 4;
			break;
		case 5:
			nChannels = 6;
			break;
		}

		if ( nChannels != sdl_speaker_channels.GetInt() )
		{
			sdl_speaker_channels.SetValue( nChannels );
		}
#endif

	if ( !dsp_slow_cpu.GetBool( 0 ) && Snd_PitchQuality.GetBool( 0 ) && snd_surround_speakers.GetInt( 0 ) == 0 )
	{
#ifdef CSTRIKE15
		dsp_enhance_stereo.SetValue( 0, 0 );
#else
		dsp_enhance_stereo.SetValue( 0, 1 );
#endif
	}
	else
	{
		dsp_enhance_stereo.SetValue( 0, 0 );
	}
}
