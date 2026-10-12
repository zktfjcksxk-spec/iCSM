//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//
#pragma once

#include "panorama/csgo_panorama.h"
#include "GameEventListener.h"
#include "gameui_interface.h"

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
class CCSGO_AudioSettingsScreen : public panorama::CPanel2D
{
	DECLARE_PANEL2D( CCSGO_AudioSettingsScreen, panorama::CPanel2D );

public:
	CCSGO_AudioSettingsScreen ( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CCSGO_AudioSettingsScreen();

private:
	bool EventPanelLoaded( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel );
	bool EventSpeakerConfigurationChanged( void );
	bool EventAudioDeviceConfigurationChanged(void);
	bool EventVoiceSelectionChanged( void );
	bool EventAudioSettingsResetDefault( void );

	void SetSpeakerConfigurationToCurrent( void );
	void SetVoiceConfigurationToCurrent( void );
	void SetAudioDevices( void );

	void UpdateEnhanceStereo( void );

	CCSGO_SettingsEnumDropDown *m_pSpeakerCfgDropDown;
	CCSGO_SettingsEnumDropDown *m_pVoiceEnableDropDown;
	CCSGO_SettingsEnumDropDown *m_pDeviceCfgDropDown;

};