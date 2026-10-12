//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//
#pragma once

#include "panorama/csgo_panorama.h"
#include "GameEventListener.h"
#include "gameui_interface.h"
#include "panorama/popups/csgo_settings_slider.h"

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
class CCSGO_VideoSettingsScreen : public panorama::CPanel2D
{
	DECLARE_PANEL2D( CCSGO_VideoSettingsScreen, panorama::CPanel2D );

public:
	CCSGO_VideoSettingsScreen( panorama::CPanel2D *pParent, const char *pchID );
	~CCSGO_VideoSettingsScreen();

	void DisplayModeChangeCallback( void );

private:
	bool EventPanelInit( void );	// User switched to video settings tab

	void SetControlsToCurrent();
	void SetResolutionBoxesToSelected();
	void SetResolutionBoxesToCurrent();
	void SetResolutionBoxesFromConfig( KeyValues *pDefaultConfigKeys );
	void SetResolutionBoxes( int nAspectRatioIndex, int nDispModeIndex, int nWidth, int nHeight );
	void GenerateWindowedModes( CUtlVector< vmode_t > &windowedModes, int nCount, vmode_t *pFullscreenModes );
	void PopulateResolutionDropDownAndSelectClosest( int nAspectRatioIndex, int nDispMode, int nWidth, int nHeight );
	void PopulateAAModes();
	void PopulateSettingsWithAutoOptions();
	
	void SetAAModeFromConfig( KeyValues *pDefaultConfigKeys );
	void SetAAModeFromConvars();
	void SetConvarsFromAAMode();
	
	void SetVSyncFromConfig( KeyValues *pDefaultConfigKeys );
	void SetVSyncFromConvars();
	void SetConvarsFromVSync();
	
	void SetQueueModeFromConfig( KeyValues *pConfig );
	void SetQueueModeFromConvar();

	// SetAdvancedOptionsFromConvars - those in the "advanced options" settings 
	void SetAdvancedOptionsFromConvars();

	// PrepareResolutionList: fills in usable res list, and returns index of entry closest 
	// to the given width and height
	int PrepareResolutionList( int nAspectRatioIndex, int nDispMode, int nWidth, int nHeight );
	
	int GetAspectRatioIndex( int width, int height );

	void SelectResolution( int nWidth, int nHeight );

	void GetSelectedResolution( int& nWidth, int& nHeight );
	int GetSelectedDisplayMode();

	// Enable/Disable widgets depending on options selected
	void EnableAspectAndResolutionControls( int nDisplayMode );
	void UpdateFullScreenOnlyOptions();

	CCSGO_SettingsEnumDropDown *m_pColorModeDropDown = nullptr;
	CCSGO_SettingsEnumDropDown *m_pAspectRatioDropDown = nullptr;
	CCSGO_SettingsEnumDropDown *m_pResolutionDropDown = nullptr;
	CCSGO_SettingsEnumDropDown *m_pDisplayModeDropDown = nullptr;
	CCSGO_SettingsEnumDropDown *m_pPowerSavingsDropDown = nullptr;
	CCSGO_SettingsEnumDropDown *m_pCSMQualityLevelDropDown = nullptr;
	CCSGO_SettingsEnumDropDown *m_pModelTextureDetailDropDown = nullptr;
	CCSGO_SettingsEnumDropDown *m_pEffectDetailDropDown = nullptr;
	CCSGO_SettingsEnumDropDown *m_pShaderDetailDropDown = nullptr;
	CCSGO_SettingsEnumDropDown *m_pFilteringModeDropDown = nullptr;
	CCSGO_SettingsEnumDropDown *m_pAAModeDropDown = nullptr;
	CCSGO_SettingsEnumDropDown *m_pFXAADropDown = nullptr;
	CCSGO_SettingsEnumDropDown *m_pVSyncDropDown = nullptr;
	CCSGO_SettingsEnumDropDown *m_pMatQueueModeDropDown = nullptr;
	CCSGO_SettingsEnumDropDown *m_pMotionBlurDropDown = nullptr;
	CSGO_SettingsSlider *m_pBrightness = nullptr;

	CUtlVector<vmode_t> m_aUsableResList;

	// Callbacks for aspect ratio and display mode change. Cause regenerations of usable resolutions
	bool EventAspectRatioSelectionChanged( void );
	bool EventDisplayModeSelectionChanged( void );
	bool EventResolutionSelectionChanged( void );
	bool EventApplyVideoSettings( void );
	bool EventVideoSettingsResetDefaults( void );

	bool m_bResolutionChanged;	// Resolution, display mode or aspect ratio changed
};

