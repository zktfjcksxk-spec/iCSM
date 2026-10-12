//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "cbase.h"
#include "panorama/popups/csgo_settings_enum.h"
#include "csgo_videosettingsscreen.h"
#include "IGameUIFuncs.h"
#include "interfaces/interfaces.h"
#include "materialsystem/materialsystem_config.h"
#include "csgo_avsettingsscreenbase.h"
#include "shaderapi/IShaderDevice.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

//=============================================================================//
// Note on the usage of _restart and _optionsui convars (inherited from the
// Scaleform version of the configuration screen code) and video cfg files
// Seems simple enough once understood, but easy to forget.
//
// The file videodefaults.txt stores the default values for various convars that
// control rendering. The file video.txt stores the values actually chosen by the
// user via the UI. Both these files are stored in the Steam userdata location.
// Each entry in the cfg files corresponds to a convar that controls rendering,
// is of the form: "setting.cpu_level"		"0"
//  
// The permissible values of these convars are exposed to the user via the settings
// screen. The user-selected value is stored and read from video.txt. 
//
// It is also possible for the user to choose to let the system decide the best value, in which 
// case the file video.txt, instead of having an entry of the form setting.<convar>
// has an entry of the form setauto.<convar>, and in this case the convar is set using 
// the value from videodefaults.txt, instead of video.txt
//
// s_pVideoConfigSettingsWhitelist specifies which convars are saved, and which ones
// support the auto option. Not all video settings are backed by convars; the ones that aren't 
// are dont concern us here, as this is all about the _restart and _optionsui convars
//
// BLoadUserVideoConfigFileFromDisk creates an in-mem video cfg. It loads the user cfg from video.txt
// and also loads the default cfg from videodefaults.txt. For each setting in video.txt that was 
// setauto, it adds a corresponding setting from the defaults.
//
// UpdateVideoConfigConVars sets convars from the in-mem video cfg. It sets the ConVar,
// and also the _restart ConVar. If the ConVar has an auto option, and the cfg contains a
// setauto setting for this ConVar (ie the user chose to have it set automatically), then 
// the _optionsui ConVar is set to 9999999. Otherwise, if the ConVar has an auto option but
// there is no setauto for this ConVar in the cfg, the _optionsui ConVar is set to the same 
// value as the ConVar and the _restart convars.
//
// UpdateCurrentVideoConfig commits convar settings to video.txt. It gets the value to write to the 
// settings from the _restart convar, if it exists, or the convar otherwise. So, it is the 
// responsibility of the calling code to set _restart and convar. If a convar has the auto option,
// writes "setauto" into the settings, otherwise writes "setting".
//
// Some convars are not written to either videodefaults.txt or video.txt. 
// mat_monitorgamma_tv_enabled is one example. This is written instead to config.cfg
//
// videodefaults.txt, video.txt and config.cfg are user cfgs backed up on Steam
//
// Committing convar changes. "mat_savechanges" calls CMaterialSystem::UpdateConfig, which
// reads convars in mem and updates material system accordingly
//
//=============================================================================//

#ifndef INCLUDE_SCALEFORM	// Don't define these convars twice in case of hybrid client

#define VIDEO_OPTIONS_UI_CVAR( cvarname, defaultvalue ) \
ConVar cvarname##_restart( #cvarname "_restart", defaultvalue, FCVAR_NONE, "Used to set video property at device reset." ); \
ConVar cvarname##_optionsui( #cvarname "_optionsui", "9999999", FCVAR_NONE, "Used to set video property from options UI.");

VIDEO_OPTIONS_UI_CVAR( csm_quality_level, "0" );
VIDEO_OPTIONS_UI_CVAR( gpu_mem_level, "-1" );
VIDEO_OPTIONS_UI_CVAR( cpu_level, "-1" );
VIDEO_OPTIONS_UI_CVAR( gpu_level, "-1" );
VIDEO_OPTIONS_UI_CVAR( mat_forceaniso, "-1" );
VIDEO_OPTIONS_UI_CVAR( mat_antialias, "0" );
VIDEO_OPTIONS_UI_CVAR( mat_aaquality, "0" );

#endif

REGISTER_PANEL2D_FACTORY( CCSGO_VideoSettingsScreen, CSGOVideoSettings );

DECLARE_PANORAMA_EVENT0( CSGOVideoSettingsInit );
DEFINE_PANORAMA_EVENT( CSGOVideoSettingsInit );

DECLARE_PANORAMA_EVENT0( CSGOApplyVideoSettings );
DEFINE_PANORAMA_EVENT( CSGOApplyVideoSettings );

DECLARE_PANORAMA_EVENT0( CSGOVideoSettingsResetDefault );
DEFINE_PANORAMA_EVENT( CSGOVideoSettingsResetDefault );

DECLARE_PANORAMA_EVENT0( CSGOAspectRatioSelectionChanged );
DEFINE_PANORAMA_EVENT( CSGOAspectRatioSelectionChanged );

DECLARE_PANORAMA_EVENT0( CSGODisplayModeSelectionChanged );
DEFINE_PANORAMA_EVENT( CSGODisplayModeSelectionChanged );

DECLARE_PANORAMA_EVENT0( CSGOResolutionSelectionChanged );
DEFINE_PANORAMA_EVENT( CSGOResolutionSelectionChanged ); 

#define DISP_MODE_WINDOWED				0
#define DISP_MODE_FULLSCREEN			1
#define DISP_MODE_FULLSCREEN_WINDOWED	2

#define VSYNC_NONE				0
#define VSYNC_DOUBLE_BUFFERED	1
#define VSYNC_TRIPLE_BUFFERED	2

using namespace panorama;
CCSGO_VideoSettingsScreen *g_pVideoSettings = nullptr;	// Singleton instance

// Valid windowed modes = fs modes + essential windowed modes
static vmode_t s_pEssentialWindowedModes[] = 
{
	// NOTE: These must be sorted by ascending width, then ascending height
	{ 640, 480, 32, 60 },
	{ 852, 480, 32, 60 },
	{ 1280, 720, 32, 60 },
	{ 1920, 1080, 32, 60 },
};

struct AspectRatioToAspectMode_t
{
	int anamorphic;
	float aspectRatio;
};

static AspectRatioToAspectMode_t s_AspectRatioToAspectModes[] =
{
	{	0,		4.0f / 3.0f },
	{	1,		16.0f / 9.0f },
	{	2,		16.0f / 10.0f },
	{	2,		1.0f },
};

//-----------------------------------------------------------------------------
// Display mode change callback
//-----------------------------------------------------------------------------
static void ModeChangeCallback( void )
{
	g_pVideoSettings->DisplayModeChangeCallback();
}

//-----------------------------------------------------------------------------
// Given string such as "cpu_level", returns value of key "setting.cpu_level"
//-----------------------------------------------------------------------------
int GetValueFromConfig( KeyValues *pConfig, char *pszSetting, int nDefault )
{
	CUtlString setting;
	setting.Format( "setting.%s", pszSetting );
	int nValue = pConfig->GetInt( setting.Get(), nDefault );
	return nValue;
}

//-----------------------------------------------------------------------------
// Get display mode as index 0-2
//-----------------------------------------------------------------------------
int GetDisplayModeIndex( bool bFullscreen, bool bNoWindowBorder, int nCurrentWidth, int nCurrentHeight )
{
	int nDisplayMode;

	if ( bFullscreen )
	{
#ifdef POSIX
		if ( bNoWindowBorder )
		{
			nDisplayMode = DISP_MODE_FULLSCREEN_WINDOWED;
		}
		else
		{
			nDisplayMode = DISP_MODE_FULLSCREEN;
		}
#else
		nDisplayMode = DISP_MODE_FULLSCREEN;
#endif
	}
	else
	{
#ifdef POSIX
		nDisplayMode = DISP_MODE_WINDOWED;
#else
		if ( bNoWindowBorder )
		{
			nDisplayMode = DISP_MODE_FULLSCREEN_WINDOWED;
			// Check if this is actually FS Windowed
			int desktopWidth, desktopHeight;
			gameuifuncs->GetDesktopResolution( desktopWidth, desktopHeight );

			if ( ( desktopHeight == nCurrentHeight ) && ( desktopWidth == nCurrentWidth ) )
			{
				nDisplayMode = DISP_MODE_FULLSCREEN_WINDOWED;
			}
			else
			{
				nDisplayMode = DISP_MODE_WINDOWED;
			}
		}
		else
		{
			nDisplayMode = DISP_MODE_WINDOWED;
		}
#endif
	}

	return nDisplayMode;
}

//-----------------------------------------------------------------------------
// GetAAModeIndex
//-----------------------------------------------------------------------------
int GetAAModeIndex( CCSGO_SettingsEnumDropDown *pAAModeDropDown, int nNumSamples, int nQuality )
{
	int nIndex = -1;
	int nNumOptions = pAAModeDropDown->GetNumOptions();
	for ( int i = 0; i < nNumOptions; i++ )
	{
		panorama::CPanel2D *pOption = pAAModeDropDown->GetOptionByIndex( i );
		int nOptionSamples = pOption->GetAttribute( "NumSamples", -1 );
		int nOptionQuality = pOption->GetAttribute( "QualityLevel", -1 );
		Assert( nOptionSamples != -1 );
		Assert( nOptionQuality != -1 );

		if ( ( nOptionSamples == nNumSamples ) && ( nOptionQuality == nQuality ) )
		{
			nIndex = i;
			break;
		}
	}

	return nIndex;
}


//-----------------------------------------------------------------------------
// Create a new option in the given dropdown, and optionally set a variable 
// number attributes specified by ("attribute name", integer value) pairs,
// ending in a null terminator
//-----------------------------------------------------------------------------
CLabel *AddNewDropdownOption( CCSGO_SettingsEnumDropDown *pDropDown, const char *pszID, const char *pszText, ... )
{
	panorama::CLabel *pLabel = new panorama::CLabel( pDropDown, pszID );
	pLabel->SetAllowRawText( true ); // REI: Ugh.  No way to validate input well here.  Probably would be better if we used dialog variables for procedural text instead.
	pLabel->SetText( pszText );
	pDropDown->AddOption( pLabel );

	// Assign attributes
	va_list argptr;
	va_start( argptr, pszText );
	while ( 1 )
	{
		const char *pAttribute = va_arg( argptr, const char *);
		if ( pAttribute == nullptr )
		{
			break;
		}

		int nValue = va_arg( argptr, int );

		pLabel->SetAttribute( pAttribute, nValue );
	}
	
	return pLabel;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
const ILocalizationString *Localize( const char *pszText, const IUIPanel *pUIPanel )
{
	return UILocalize()->PchFindToken( pUIPanel, pszText, 
		k_nLocalizeMaxChars, k_eStringTruncationStyle_None, 
		k_eStringTransformStyle_None, k_eStringEscapeStyle_None, true );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
const char *GetSettingFromDefaultVideoCfg( const char *pszSetting )
{
	// Get default settings from cfg
	static KeyValues *s_kvOptionsUiDefaults = NULL;
	if ( !s_kvOptionsUiDefaults )
	{
		s_kvOptionsUiDefaults = new KeyValues( "defaults" );
		if ( !s_kvOptionsUiDefaults->LoadFromFile( filesystem, "cfg/videodefaults.txt", "USRLOCAL" ) )
			s_kvOptionsUiDefaults->Clear();

		KeyValuesDumpAsDevMsg( s_kvOptionsUiDefaults );
	}

	return s_kvOptionsUiDefaults->GetString( pszSetting, nullptr );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int GetOptionIndexFromValue( CCSGO_SettingsEnumDropDown *pDropDown, int nValue )
{
	int nNumOptions = pDropDown->GetNumOptions();
	for ( int i = 0; i < nNumOptions; i++ )
	{
		panorama::CPanel2D *pOption = pDropDown->GetOptionByIndex( i );
		int nOptionValue = pOption->GetAttribute( "value", -1 );
		if ( nOptionValue == nValue )
		{
			return i;
		}
	}

	Assert( 0 );
	return -1;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void SetAutoOptionString( const char *cfgSettingLookup, CCSGO_SettingsEnumDropDown *pDropDown, 
	const char* pszOptions[], int nNumOptions, const IUIPanel *pUIPanel )
{
	CMutableLocalizationString locAuto( Localize( "#SFUI_Settings_Choice_Autodetect", pUIPanel ) ) ;
	CMutableLocalizationString locDefSetting;
	
	// Look up the default value of given option
	if ( char const *szOptionsUiDefault = GetSettingFromDefaultVideoCfg( cfgSettingLookup ) )
	{
		int nValue = V_atoi( szOptionsUiDefault );
		int nIndex = GetOptionIndexFromValue( pDropDown, nValue );
	
		if ( ( nIndex >= 0 ) && ( nIndex < nNumOptions ) )
		{
			locDefSetting = Localize( pszOptions[ nIndex ], pUIPanel );
		}
		else
		{
			Assert( 0 );
		}
		
		// The Auto option is always the last one
		int nNumDropdownOptions = pDropDown->GetNumOptions();
		CLabel *pLabel = panorama::panel_cast< CLabel* >( pDropDown->GetOptionByIndex( nNumDropdownOptions - 1 ) );
		if ( pLabel )
		{
			pLabel->SetAllowRawText( true );
			pLabel->SetText( locAuto.Get()->String() );
			pLabel->AppendText( ":" );
			pLabel->AppendText( locDefSetting.Get()->String() );
		}
		else
		{
			Assert( 0 );
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void SetOptionFromConvar( const char* pszConvar, CCSGO_SettingsEnumDropDown *pDropDown )
{
	ConVarRef cvarref( pszConvar );
	int nValue = cvarref.GetInt();
	int nIndex;
	if ( nValue == AUTO_OPTION_VALUE )
	{
		nIndex = pDropDown->GetNumOptions() - 1;
	}
	else
	{
		nIndex = GetOptionIndexFromValue( pDropDown, nValue );
	}

	DropdownSelectOptionByIndex( pDropDown, nIndex );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void SetOptionFromConfig( KeyValues *pConfig, char* pszSetting, CCSGO_SettingsEnumDropDown *pDropDown )
{
	int nValue = GetValueFromConfig( pConfig, pszSetting, -1 );
	if ( nValue != -1 )
	{
		DropdownSelectOptionByIndex( pDropDown, GetOptionIndexFromValue( pDropDown, nValue ) );
	}
}

//-----------------------------------------------------------------------------
// Queued mode
//	-1=default, 0=synchronous single thread, 1=queued single thread, 2=queued multithreaded
// Dropdown: 
//		0,1 = disabled (= 0 when writing back to convar )
//	   -1,2 = enabled  (= -1 when writing back to convar )
//-----------------------------------------------------------------------------
void SetQueueModeFromValue( int nValue, CCSGO_SettingsEnumDropDown *pDropDown )
{
	if ( nValue == 1 )
	{
		nValue = 0;
	}
	else if ( nValue == 2 )
	{
		nValue = -1;
	}

	DropdownSelectOptionByIndex( pDropDown, GetOptionIndexFromValue( pDropDown, nValue ) );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void SetConvar( const char* pszConvar, int nValue, int nDefaultValue, bool bHasAutoOption = true )
{
	int nActualValue = nValue;

	if ( bHasAutoOption )
	{
		// Set the restart and optionsui convars
		CFmtStr szCvarRestart( "%s_restart", pszConvar );
		ConVarRef cvarRestart( szCvarRestart );
	
		CFmtStr szCvarOptionsUI( "%s_optionsui", pszConvar );
		ConVarRef cvarOptionsUI( szCvarOptionsUI );

		if ( nValue == AUTO_OPTION_VALUE )
		{
			// Set the restart convar and the convar itself to default value
			CFmtStr settingsStr( "setting.%s", pszConvar );
			const char *pSettingsValue = GetSettingFromDefaultVideoCfg( settingsStr );
			if ( pSettingsValue )
			{
				nActualValue = V_atoi( pSettingsValue );
			}
			else
			{
				nActualValue = nDefaultValue;
			}
		}

		if ( cvarRestart.IsValid() )
		{
			cvarRestart.SetValue( nActualValue );
		}

		cvarOptionsUI.SetValue( nValue );
	}
	else
	{
		// Set the restart convar to user value
		CFmtStr szCvarRestart( "%s_restart", pszConvar );
		ConVarRef cvarRestart( szCvarRestart );
		if ( cvarRestart.IsValid() )
		{
			cvarRestart.SetValue( nValue );
		}
	}

	// Set the convar
	ConVarRef cvarref( pszConvar );
	cvarref.SetValue( nActualValue );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void SetConvarFromOption( CCSGO_SettingsEnumDropDown *pDropDown, const char* pszConvar, int nDefaultValue, bool bHasAutoOption = true )
{
	// Get the user selected value 
	int nSelectedValue = DropdownGetSelectedValue( pDropDown );
	if ( nSelectedValue != INVALID_OPTION_VALUE )
	{		
		SetConvar( pszConvar, nSelectedValue, nDefaultValue, bHasAutoOption );
	}
	else
	{
		Assert( 0 );	// Invalid value selected
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void SetConvarToDefault( const char* pszConvar )
{
	ConVarRef cvarref( pszConvar );
	cvarref.SetValue( cvarref.GetDefault() );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CCSGO_VideoSettingsScreen::CCSGO_VideoSettingsScreen( CPanel2D *pParent, const char *pchID )
	: 
	CPanel2D( pParent, pchID ), 
	m_bResolutionChanged( false )
{
	Assert( g_pVideoSettings == nullptr );
	g_pVideoSettings = this;

	materials->AddModeChangeCallBack( ModeChangeCallback );

	// CSGOVideoSettings is no longer loaded as a separate xml. 
	// DbgVerify( BLoadLayout( "file://{resources}/layout/settings/settings_video.xml" ) );
	
	RegisterEventHandler( CSGOAspectRatioSelectionChanged(), this, &CCSGO_VideoSettingsScreen::EventAspectRatioSelectionChanged );
	RegisterEventHandler( CSGODisplayModeSelectionChanged(), this, &CCSGO_VideoSettingsScreen::EventDisplayModeSelectionChanged );
	RegisterEventHandler( CSGOResolutionSelectionChanged(), this, &CCSGO_VideoSettingsScreen::EventResolutionSelectionChanged );

	RegisterForUnhandledEvent( CSGOVideoSettingsInit(), this, &CCSGO_VideoSettingsScreen::EventPanelInit );
	RegisterForUnhandledEvent( CSGOApplyVideoSettings(), this, &CCSGO_VideoSettingsScreen::EventApplyVideoSettings );
	RegisterForUnhandledEvent( CSGOVideoSettingsResetDefault(), this, &CCSGO_VideoSettingsScreen::EventVideoSettingsResetDefaults );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CCSGO_VideoSettingsScreen::~CCSGO_VideoSettingsScreen()
{
	g_pVideoSettings = nullptr;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CCSGO_VideoSettingsScreen::DisplayModeChangeCallback( void )
{
	UpdateFullScreenOnlyOptions();
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CCSGO_VideoSettingsScreen::EventPanelInit( void )
{
	engine->ExecuteClientCmd( "mat_updateconvars" );

	m_pColorModeDropDown = panorama::panel_cast< CCSGO_SettingsEnumDropDown* >( RequireChildInLayoutFile( "ColorMode" ) );
	m_pAspectRatioDropDown = panorama::panel_cast< CCSGO_SettingsEnumDropDown* >( RequireChildInLayoutFile( "AspectRatioEnum" ) );
	m_pResolutionDropDown = panorama::panel_cast< CCSGO_SettingsEnumDropDown* >( RequireChildInLayoutFile( "ResolutionEnum" ) );
	m_pDisplayModeDropDown = panorama::panel_cast< CCSGO_SettingsEnumDropDown* >( RequireChildInLayoutFile( "DisplayModeEnum" ) );
	m_pPowerSavingsDropDown = panorama::panel_cast< CCSGO_SettingsEnumDropDown* >( RequireChildInLayoutFile( "PowerSavingsMode" ) );
	m_pCSMQualityLevelDropDown = panorama::panel_cast< CCSGO_SettingsEnumDropDown* >( RequireChildInLayoutFile( "CSMQualityLevel" ) );
	m_pModelTextureDetailDropDown = panorama::panel_cast< CCSGO_SettingsEnumDropDown* >( RequireChildInLayoutFile( "ModelTextureDetail" ) );
	m_pEffectDetailDropDown = panorama::panel_cast< CCSGO_SettingsEnumDropDown* >( RequireChildInLayoutFile( "EffectDetail" ) );
	m_pShaderDetailDropDown = panorama::panel_cast< CCSGO_SettingsEnumDropDown* >( RequireChildInLayoutFile( "ShaderDetail" ) );
	m_pFilteringModeDropDown = panorama::panel_cast< CCSGO_SettingsEnumDropDown* >( RequireChildInLayoutFile( "FilteringMode" ) );
	m_pAAModeDropDown = panorama::panel_cast< CCSGO_SettingsEnumDropDown* >( RequireChildInLayoutFile( "AAMode" ) );
	m_pFXAADropDown = panorama::panel_cast< CCSGO_SettingsEnumDropDown* >( RequireChildInLayoutFile( "FXAA" ) );
	m_pVSyncDropDown = panorama::panel_cast< CCSGO_SettingsEnumDropDown* >( RequireChildInLayoutFile( "VSync" ) );
	m_pMatQueueModeDropDown = panorama::panel_cast< CCSGO_SettingsEnumDropDown* >( RequireChildInLayoutFile( "MatQueueMode" ) );
	m_pMotionBlurDropDown = panorama::panel_cast< CCSGO_SettingsEnumDropDown* >( RequireChildInLayoutFile( "MotionBlur" ) );
	m_pBrightness = panorama::panel_cast< CSGO_SettingsSlider* > ( RequireChildInLayoutFile( "brightness" ) );

	SetControlsToCurrent();
		
	// This event was handled
	return true;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CCSGO_VideoSettingsScreen::SetControlsToCurrent()
{
	SetOptionFromConvar( "mat_monitorgamma_tv_enabled", m_pColorModeDropDown );
	SetOptionFromConvar( "mat_powersavingsmode", m_pPowerSavingsDropDown );

	SetResolutionBoxesToCurrent();
	PopulateSettingsWithAutoOptions();
	SetAdvancedOptionsFromConvars();
	PopulateAAModes();
	SetAAModeFromConvars();
	SetVSyncFromConvars();
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CCSGO_VideoSettingsScreen::SetResolutionBoxesToSelected()
{
	// Get current resolution, so we can select the one closest to it
	const MaterialSystem_Config_t &config = materials->GetCurrentConfigForVideoCard();
	int nWidth = config.m_VideoMode.m_Width;
	int nHeight = config.m_VideoMode.m_Height;

	int nDisplayMode = DropdownGetSelectedValue( m_pDisplayModeDropDown );

	// If the display mode is fullscreen windowed, then the only resolution available is desktop res, so
	// select correct aspect ratio and resolution here
	int nAspectRatioIndex;
#ifdef POSIX
	nAspectRatioIndex = DropdownGetSelectedValue( m_pAspectRatioDropDown );
#else
	if ( nDisplayMode == DISP_MODE_FULLSCREEN_WINDOWED )
	{
		// Get desktop dimensions
		int nDesktopWidth, nDesktopHeight;
		gameuifuncs->GetDesktopResolution( nDesktopWidth, nDesktopHeight );
		nAspectRatioIndex = GetAspectRatioIndex( nDesktopWidth, nDesktopHeight );

		nWidth = nDesktopWidth;
		nHeight = nDesktopHeight;
		
		// Override aspect ratio
		DropdownSelectOptionByIndex( m_pAspectRatioDropDown, nAspectRatioIndex );
	}
	else
	{
		nAspectRatioIndex = DropdownGetSelectedValue( m_pAspectRatioDropDown );
	}
#endif

	// Disable/enable colour mode
	UpdateFullScreenOnlyOptions();
	PopulateResolutionDropDownAndSelectClosest( nAspectRatioIndex, nDisplayMode, nWidth, nHeight );
	EnableAspectAndResolutionControls( nDisplayMode );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CCSGO_VideoSettingsScreen::SetResolutionBoxesToCurrent()
{
	// Get current resolution
	const MaterialSystem_Config_t &config = materials->GetCurrentConfigForVideoCard();

	// Get display mode
	int nDisplayMode = GetDisplayModeIndex( !config.Windowed(), config.NoWindowBorder(), 
		config.m_VideoMode.m_Width, config.m_VideoMode.m_Height );

	// Get current aspect ratio index
	int nAspectRatioIndex = GetAspectRatioIndex( config.m_VideoMode.m_Width, config.m_VideoMode.m_Height );

	SetResolutionBoxes( nAspectRatioIndex, nDisplayMode, config.m_VideoMode.m_Width, config.m_VideoMode.m_Height );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CCSGO_VideoSettingsScreen::SetResolutionBoxesFromConfig( KeyValues *pDefaultConfigKeys )
{
	// Get current resolution in case we don't find valid values in the settings
	const MaterialSystem_Config_t &config = materials->GetCurrentConfigForVideoCard();

	// Push values from default cfg into the display mode, aspect ratio and
	// resolution controls
	int nWidth = pDefaultConfigKeys->GetInt( "setting.defaultres",  config.m_VideoMode.m_Width );
	int nHeight = pDefaultConfigKeys->GetInt( "setting.defaultresheight",  config.m_VideoMode.m_Height );
	int nFullScreen = pDefaultConfigKeys->GetInt( "setting.fullscreen", 1 );
	int nNoWindowBorder = pDefaultConfigKeys->GetInt( "setting.nowindowborder", 0 );

	int nDisplayMode = GetDisplayModeIndex( (nFullScreen==1), (nNoWindowBorder==1), nWidth, nHeight );
	int nAspectRatioIndex = GetAspectRatioIndex( nWidth, nHeight );
	
	SetResolutionBoxes( nAspectRatioIndex, nDisplayMode, nWidth, nHeight );

};

//-----------------------------------------------------------------------------
// This is just a helper. Assumes params are all valid and consistent
//-----------------------------------------------------------------------------
void CCSGO_VideoSettingsScreen::SetResolutionBoxes( int nAspectRatioIndex, int nDispModeIndex, int nWidth, int nHeight )
{
	DropdownSelectOptionByIndex( m_pDisplayModeDropDown, nDispModeIndex );
	DropdownSelectOptionByIndex( m_pAspectRatioDropDown, nAspectRatioIndex );
	UpdateFullScreenOnlyOptions();
	PopulateResolutionDropDownAndSelectClosest( nAspectRatioIndex, nDispModeIndex, nWidth, nHeight );
	EnableAspectAndResolutionControls( nDispModeIndex );
}

//-----------------------------------------------------------------------------
// Get list of valid windowed modes by adding essential windowed modes to fs modes
//-----------------------------------------------------------------------------
void CCSGO_VideoSettingsScreen::GenerateWindowedModes( CUtlVector< vmode_t > &windowedModes, int nCount, vmode_t *pFullscreenModes )
{
	// Insert essential windowed modes into sorted fs modes list. Copied from scaleform options screen.
	int nFSMode = 0;
	for ( int i = 0; i < ARRAYSIZE( s_pEssentialWindowedModes ); ++i )
	{
		while ( true )
		{
			if ( nFSMode >= nCount )
				break;

			if ( pFullscreenModes[nFSMode].width > s_pEssentialWindowedModes[i].width )
				break;

			if ( pFullscreenModes[nFSMode].width == s_pEssentialWindowedModes[i].width )
			{
				if ( pFullscreenModes[nFSMode].height > s_pEssentialWindowedModes[i].height )
					break;

				if ( pFullscreenModes[nFSMode].height == s_pEssentialWindowedModes[i].height )
				{
					// Don't add the matching fullscreen mode
					++nFSMode;
					break;
				}
			}

			windowedModes.AddToTail( pFullscreenModes[nFSMode] );
			++nFSMode;
		}

		windowedModes.AddToTail( s_pEssentialWindowedModes[i] );
	}

	for ( ; nFSMode < nCount; ++nFSMode )
	{
		windowedModes.AddToTail( pFullscreenModes[nFSMode] );
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CCSGO_VideoSettingsScreen::PopulateResolutionDropDownAndSelectClosest(  int nAspectRatioIndex, int nDispMode, int nWidth, int nHeight )
{
	int nClosestRes = PrepareResolutionList( nAspectRatioIndex, nDispMode, nWidth, nHeight );

	// Add to dropdown 
	m_pResolutionDropDown->RemoveAllOptions();
	int nCount = m_aUsableResList.Count();

	if ( nCount )
	{
		for ( int i = 0; i < nCount; i++ )
		{
			// Add this resolution to the dropdown
			char szLabelId[ 32 ] = "";
			V_snprintf( szLabelId, ARRAYSIZE( szLabelId ), "%ix%i", m_aUsableResList[i].width, m_aUsableResList[i].height );
			AddNewDropdownOption( m_pResolutionDropDown, szLabelId, szLabelId,
				"width", m_aUsableResList[i].width, "height", m_aUsableResList[i].height, nullptr );
		}

		SelectResolution( m_aUsableResList[ nClosestRes ].width, m_aUsableResList[ nClosestRes ].height );
	}
	else
	{
        AddNewDropdownOption( m_pResolutionDropDown, "NoModes", "#SFUI_Settings_None",
			"width", -1, "height", -1, nullptr );
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CCSGO_VideoSettingsScreen::PopulateAAModes()
{
	m_pAAModeDropDown->RemoveAllOptions();

	const char *pszDefault = "#SFUI_Settings_None";
	
	const char *pszNumSamples = "NumSamples";
	const char *pszQualityLevel = "QualityLevel";

	AddNewDropdownOption( m_pAAModeDropDown, "aamode0", pszDefault, pszNumSamples, 0, pszQualityLevel, 0, nullptr );

	// 2x
	if ( materials->SupportsMSAAMode(2) )
	{
		pszDefault = "#SFUI_Settings_2X_MSAA";
		AddNewDropdownOption( m_pAAModeDropDown, "aamode1", pszDefault, pszNumSamples, 2, pszQualityLevel, 0, nullptr );
	}

	// 4x
	if ( materials->SupportsMSAAMode(4) )
	{
		pszDefault = "#SFUI_Settings_4X_MSAA";
		AddNewDropdownOption( m_pAAModeDropDown, "aamode2", pszDefault, pszNumSamples, 4, pszQualityLevel, 0, nullptr );
	}

	// 8x
	if ( materials->SupportsMSAAMode(8) )
	{
		pszDefault = "#SFUI_Settings_8X_MSAA";
		AddNewDropdownOption( m_pAAModeDropDown, "aamode3", pszDefault, pszNumSamples, 8, pszQualityLevel, 0, nullptr );
	}

	// 8x CSAA
	if ( materials->SupportsCSAAMode(4, 2) )
	{
		AddNewDropdownOption( m_pAAModeDropDown, "aamode4", "#SFUI_Settings_8X_CSAA", pszNumSamples, 4, pszQualityLevel, 2, nullptr );
	}


	// 16x CSAA
	if ( materials->SupportsCSAAMode(4, 4) )
	{
		AddNewDropdownOption( m_pAAModeDropDown, "aamode5", "#SFUI_Settings_16X_CSAA", pszNumSamples, 4, pszQualityLevel, 4, nullptr );
	}

	// 16xQ CSAA
	if ( materials->SupportsCSAAMode(8, 2) )
	{
		AddNewDropdownOption( m_pAAModeDropDown, "aamode6", "#SFUI_Settings_16XQ_CSAA", pszNumSamples, 8, pszQualityLevel, 2, nullptr );
	}

	// Default
	CMutableLocalizationString locAuto( Localize( "#SFUI_Settings_Choice_Autodetect", UIPanel() ) ) ;
	CMutableLocalizationString locDefSetting( Localize( pszDefault, UIPanel() ) );
	CFmtStr szAutoOption( "%s:%s", locAuto.Get()->String(), locDefSetting.Get()->String() );
	AddNewDropdownOption( m_pAAModeDropDown, "aamode7", szAutoOption, pszNumSamples, AUTO_OPTION_VALUE, pszQualityLevel, AUTO_OPTION_VALUE, nullptr );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CCSGO_VideoSettingsScreen::PopulateSettingsWithAutoOptions()
{
	CMutableLocalizationString locAuto( Localize( "#SFUI_Settings_Choice_Autodetect", UIPanel() ) ) ;
	CMutableLocalizationString locDefSetting;

	static const char *szCSMOptions[] =
	{
		"SFUI_CSM_Low", 
		"SFUI_CSM_Med", 	
		"SFUI_CSM_High", 	
		"SFUI_CSM_VeryHigh",
	};

	static const char *szSettingsOptions[] =
	{
		"SFUI_Settings_Low",
		"SFUI_Settings_Medium",	
		"SFUI_Settings_High",		
		"SFUI_Settings_Very_High",	
	};

	static const char *szFilteringOptions[] = 
	{
		"SFUI_Settings_Bilinear",		
		"SFUI_Settings_Trilinear",		
		"SFUI_Settings_Anisotropic_2X",	
		"SFUI_Settings_Anisotropic_4X",
		"SFUI_Settings_Anisotropic_8X",	
		"SFUI_Settings_Anisotropic_16X"	
	};

	SetAutoOptionString( "setting.csm_quality_level", m_pCSMQualityLevelDropDown, szCSMOptions, ARRAYSIZE( szCSMOptions ), UIPanel() );
	SetAutoOptionString( "setting.gpu_mem_level", m_pModelTextureDetailDropDown, szSettingsOptions, ARRAYSIZE( szSettingsOptions ), UIPanel() );
	SetAutoOptionString( "setting.cpu_level", m_pEffectDetailDropDown, szSettingsOptions, ARRAYSIZE( szSettingsOptions ), UIPanel() );
	SetAutoOptionString( "setting.gpu_level", m_pShaderDetailDropDown, szSettingsOptions, ARRAYSIZE( szSettingsOptions ), UIPanel() );
	SetAutoOptionString( "setting.mat_forceaniso", m_pFilteringModeDropDown, szFilteringOptions, ARRAYSIZE( szFilteringOptions ), UIPanel() );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CCSGO_VideoSettingsScreen::SetAAModeFromConfig( KeyValues *pDefaultConfigKeys )
{
	int nNumSamples = GetValueFromConfig( pDefaultConfigKeys, "mat_antialias", 0 );
	int nQuality = GetValueFromConfig( pDefaultConfigKeys, "mat_aaquality", 0 );

	int nIndex = GetAAModeIndex( m_pAAModeDropDown, nNumSamples, nQuality );
	Assert( nIndex != -1 );

	DropdownSelectOptionByIndex( m_pAAModeDropDown, nIndex );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CCSGO_VideoSettingsScreen::SetAAModeFromConvars()
{
	int nIndex = -1;

	ConVarRef cvarSamples( "mat_antialias_optionsui" );
	int nNumSamples = cvarSamples.GetInt();

	ConVarRef cvarQuality( "mat_aaquality_optionsui" );
	int nQuality = cvarQuality.GetInt();

	if ( nNumSamples == AUTO_OPTION_VALUE )
	{
		Assert ( nQuality == AUTO_OPTION_VALUE );
		nIndex = m_pAAModeDropDown->GetNumOptions() - 1;
	}
	else
	{
		Assert ( nQuality != AUTO_OPTION_VALUE );
		nIndex = GetAAModeIndex( m_pAAModeDropDown, nNumSamples, nQuality );
	}

	Assert( nIndex != -1 );
	DropdownSelectOptionByIndex( m_pAAModeDropDown, nIndex );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CCSGO_VideoSettingsScreen::SetConvarsFromAAMode()
{
	panorama::CPanel2D *pSelectedOption = m_pAAModeDropDown->GetSelected();
	int nNumSamples = pSelectedOption->GetAttribute( "NumSamples", -1 );
	int nQuality = pSelectedOption->GetAttribute( "QualityLevel", -1 );

	SetConvar( "mat_antialias", nNumSamples, 0 );
	SetConvar( "mat_aaquality", nQuality, 0 );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CCSGO_VideoSettingsScreen::SetVSyncFromConfig( KeyValues *pDefaultConfigKeys )
{
	int nVsync = GetValueFromConfig( pDefaultConfigKeys, "mat_vsync", 0 );
	int nTripleBuffered = GetValueFromConfig( pDefaultConfigKeys, "mat_triplebuffered", 0 );

	int nIndex = ( nVsync == 0 )? VSYNC_NONE: ( ( nTripleBuffered == 0 )? VSYNC_DOUBLE_BUFFERED: VSYNC_TRIPLE_BUFFERED );

	DropdownSelectOptionByIndex( m_pVSyncDropDown, nIndex );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CCSGO_VideoSettingsScreen::SetVSyncFromConvars()
{
	ConVarRef cvVSync( "mat_vsync" );
	int nIndex = -1;

	if ( cvVSync.GetInt() == 1 )
	{
		// Check triple buffered
		ConVarRef cvTripleBuffer( "mat_triplebuffered" );
		if ( cvTripleBuffer.GetInt() == 0 )
		{
			nIndex = VSYNC_DOUBLE_BUFFERED;
		}
		else
		{
			nIndex = VSYNC_TRIPLE_BUFFERED;
		}
	}
	else
	{
		nIndex = VSYNC_NONE;
	}

	DropdownSelectOptionByIndex( m_pVSyncDropDown, nIndex );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CCSGO_VideoSettingsScreen::SetConvarsFromVSync()
{
	ConVarRef cvVSync( "mat_vsync" );
	ConVarRef cvTripleBuffer( "mat_triplebuffered" );

	int nOption = DropdownGetSelectedValue( m_pVSyncDropDown );
	switch ( nOption )
	{
	case 0:
		cvVSync.SetValue( 0 );
		cvTripleBuffer.SetValue( 0 );
		break;

	case 1:
		cvVSync.SetValue( 1 );
		cvTripleBuffer.SetValue( 0 );
		break;

	case 2:
		cvVSync.SetValue( 1 );
		cvTripleBuffer.SetValue( 1 );
		break;

	default:
		Assert( 0 );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CCSGO_VideoSettingsScreen::SetQueueModeFromConfig( KeyValues *pConfig )
{
	int nValue = GetValueFromConfig( pConfig, "mat_queue_mode", -1 );
	SetQueueModeFromValue( nValue, m_pMatQueueModeDropDown );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CCSGO_VideoSettingsScreen::SetQueueModeFromConvar( )
{
	ConVarRef cvarref( "mat_queue_mode" );
	int nValue = cvarref.GetInt();
	SetQueueModeFromValue( nValue, m_pMatQueueModeDropDown );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CCSGO_VideoSettingsScreen::SetAdvancedOptionsFromConvars()
{
	// Set options from the _optionsUI convars, which are either set to AUTO_OPTIONS_VALUE, or an explicit user-selected value
	SetOptionFromConvar( "cpu_level_optionsui", m_pEffectDetailDropDown );
	SetOptionFromConvar( "gpu_level_optionsui", m_pShaderDetailDropDown );
	SetOptionFromConvar( "mat_forceaniso_optionsui", m_pFilteringModeDropDown );
	SetOptionFromConvar( "gpu_mem_level_optionsui", m_pModelTextureDetailDropDown );
	SetOptionFromConvar( "csm_quality_level_optionsui", m_pCSMQualityLevelDropDown );
	
	SetOptionFromConvar( "mat_software_aa_strength", m_pFXAADropDown );
	SetOptionFromConvar( "mat_motion_blur_enabled", m_pMotionBlurDropDown );
	SetQueueModeFromConvar();
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
int CCSGO_VideoSettingsScreen::PrepareResolutionList( int nAspectRatioIndex, int nDispMode, int nWidth, int nHeight )
{
	m_aUsableResList.RemoveAll();

	// Get desktop dimensions, for windowed mode
	int nDesktopWidth, nDesktopHeight;
	gameuifuncs->GetDesktopResolution( nDesktopWidth, nDesktopHeight );

#ifndef POSIX
	// If the display mode is Fullscreen Windowed, the only resolution we have is the desktop res
	if ( nDispMode == DISP_MODE_FULLSCREEN_WINDOWED )
	{
		vmode_t plist = { nDesktopWidth, nDesktopHeight };
		m_aUsableResList.AddToTail( plist );
		return 0;
	}
#endif

	// Get full video mode list
	vmode_t *plist = NULL;
	int nCount = 0;
	gameuifuncs->GetVideoModes( &plist, &nCount );

	bool bWindowed = ( nDispMode == DISP_MODE_WINDOWED );

	CUtlVector< vmode_t > windowedModes;
	if ( bWindowed )
	{
		GenerateWindowedModes( windowedModes, nCount, plist );
		nCount = windowedModes.Count();
		plist = windowedModes.Base();
	}
	
	// Keep track of resolution closest to current one, so we can set that as the selection
	int iClosestMode = -1;
	int nClosestDist = INT_MAX;
    bool bWidthMatch = false;
	bool bExactMatch = false;

	// Add to usable res list if aspect ratio matches
	if ( nCount )
	{
		for ( int i = 0; i < nCount; i++, plist++ )
		{
#ifdef SOURCE_NATIVE_METAL
			// Metal presents an offscreen pixel backbuffer into a Retina-aware
			// window. Logical desktop bounds do not limit render resolution.
			if ( bWindowed && ( plist->width < 1920 || plist->height < 1080 ) )
				continue;
#else
			// don't show modes bigger than the desktop for windowed mode
			if ( bWindowed && ( plist->width > nDesktopWidth || plist->height > nDesktopHeight ) )
			{
				continue;
			}
#endif

			int nResolutionAspectRatioIndex = GetAspectRatioIndex( plist->width, plist->height );

			if ( nResolutionAspectRatioIndex != nAspectRatioIndex )
			{
				continue;
			}

			// Add this resolution to the usable list
			m_aUsableResList.AddToTail( *plist );

			// Find closest match
			if ( !bExactMatch )
			{
				int nSizeDelta = plist->width - nWidth;
				
				if ( bWidthMatch )
				{
					// We already have an exact width match so we're getting the closest height.
					if ( nSizeDelta != 0 )
					{
						continue;
					}

					nSizeDelta = plist->height - nHeight;
				}
				else if ( nSizeDelta == 0 )
				{
					bWidthMatch = true;
					nSizeDelta = plist->height - nHeight;
					nClosestDist = INT_MAX;

					if ( nSizeDelta == 0 )
					{
						bExactMatch = true;
					}
				}

				nSizeDelta = abs( nSizeDelta );

				if ( nSizeDelta < nClosestDist )
				{
					iClosestMode = m_aUsableResList.Count() - 1;
					nClosestDist = nSizeDelta;
				}
			}
		}
	}

	return iClosestMode;
}	

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
int CCSGO_VideoSettingsScreen::GetAspectRatioIndex( int width, int height )
{
	float aspectRatio = (float)width / (float)height;

	// just find the closest ratio
	float closestAspectRatioDist = 99999.0f;
	int closestAnamorphic = 0;
	for (int i = 0; i < ARRAYSIZE(s_AspectRatioToAspectModes); i++)
	{
		float dist = fabs( s_AspectRatioToAspectModes[i].aspectRatio - aspectRatio );
		if (dist < closestAspectRatioDist)
		{
			closestAspectRatioDist = dist;
			closestAnamorphic = s_AspectRatioToAspectModes[i].anamorphic;
		}
	}

	return closestAnamorphic;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CCSGO_VideoSettingsScreen::SelectResolution( int nWidth, int nHeight )
{
	m_pResolutionDropDown->SetSelected( CFmtStr( "%dx%d", nWidth, nHeight ), false );
	m_pResolutionDropDown->InvalidateOptions( false );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CCSGO_VideoSettingsScreen::GetSelectedResolution( int& nWidth, int& nHeight )
{
	CPanel2D *pResSelectedPanel = m_pResolutionDropDown->GetSelected();
	if ( !pResSelectedPanel )
	{
		Assert(0);
		nWidth = -1;
		nHeight = -1;
	}
	else
	{
		nWidth = pResSelectedPanel->GetAttribute( "width", INVALID_OPTION_VALUE );
		nHeight = pResSelectedPanel->GetAttribute( "height", INVALID_OPTION_VALUE );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CCSGO_VideoSettingsScreen::GetSelectedDisplayMode()
{
	int nDispMode = DropdownGetSelectedValue( m_pDisplayModeDropDown );
	return nDispMode;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CCSGO_VideoSettingsScreen::EnableAspectAndResolutionControls( int nDisplayMode )
{
#ifdef POSIX
	// always enable
	bool bEnableResolutionBoxes = true;
#else
	// Disable aspect ratio and resolution controls if display mode is FS Windowed
	bool bEnableResolutionBoxes = ( nDisplayMode != DISP_MODE_FULLSCREEN_WINDOWED );
#endif

	m_pResolutionDropDown->SetEnabled( bEnableResolutionBoxes );
	m_pAspectRatioDropDown->SetEnabled( bEnableResolutionBoxes );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CCSGO_VideoSettingsScreen::UpdateFullScreenOnlyOptions()
{
	// Desired behaviour: color mode is disabled unless game is running fullscreen, *and*
	// the fullscreen option is currently selected. Ie, turn it off if running fs, but user just
	// selected non-fs in dropdown, turn it back on again if user re-selected fs in dropdown

	const MaterialSystem_Config_t &config = materials->GetCurrentConfigForVideoCard();
	bool bRunningFS = ( !config.Windowed() ) && ( !config.NoWindowBorder() );

	int nSelectedDisplayMode = DropdownGetSelectedValue( m_pDisplayModeDropDown );
	bool bEnabled = bRunningFS && ( nSelectedDisplayMode == DISP_MODE_FULLSCREEN );
	  m_pColorModeDropDown->SetEnabled( bEnabled );

	// Slam it to Monitor Gamma if not enabled
	if ( !bEnabled )
	{
		DropdownSelectOptionByIndex(   m_pColorModeDropDown, 0, true );		
	}

	m_pBrightness->SetEnabled( bEnabled );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CCSGO_VideoSettingsScreen::EventAspectRatioSelectionChanged( void )
{
	m_bResolutionChanged = true;
	SetResolutionBoxesToSelected();
	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CCSGO_VideoSettingsScreen::EventDisplayModeSelectionChanged( void )
{
	m_bResolutionChanged = true;
	SetResolutionBoxesToSelected();
	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CCSGO_VideoSettingsScreen::EventResolutionSelectionChanged( void )
{
	m_bResolutionChanged = true;
	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CCSGO_VideoSettingsScreen::EventApplyVideoSettings( void )
{
	if ( m_bResolutionChanged )
	{
		int nWidth, nHeight, nDispMode;
		GetSelectedResolution( nWidth, nHeight );
		nDispMode = GetSelectedDisplayMode();

		bool bWindowed = ( nDispMode != 1 );
		bool bBorderless = ( nDispMode == 2 );

#ifdef POSIX
		// mimic behavior of fullscreen windowed under Scaleform for Posix
		// since that actually provides a full screen, non windowed setting that allows fast alt-tab
		if ( bWindowed && bBorderless )
		{
			bBorderless = true;
			bWindowed = false;
		}
#else
		// FS Windowed uses the desktop resolution
		if ( bWindowed && bBorderless )
		{
			gameuifuncs->GetDesktopResolution( nWidth, nHeight );
		}
#endif

		char szCmd[ 256 ];
		V_snprintf( szCmd, sizeof( szCmd ), "mat_setvideomode %i %i %i %i\n", nWidth, nHeight, bWindowed, bBorderless );

		engine->ClientCmd_Unrestricted( szCmd );
		m_bResolutionChanged = false;
	}

	// Set other video options. 

	// Set convars with auto options
	SetConvarFromOption( m_pEffectDetailDropDown, "cpu_level", CPU_LEVEL_LOW );
	SetConvarFromOption( m_pShaderDetailDropDown, "gpu_level", GPU_LEVEL_LOW );
	SetConvarsFromAAMode();
	SetConvarFromOption( m_pFilteringModeDropDown, "mat_forceaniso", 1 );
	SetConvarFromOption( m_pModelTextureDetailDropDown, "gpu_mem_level", GPU_MEM_LEVEL_LOW );
	SetConvarFromOption( m_pCSMQualityLevelDropDown, "csm_quality_level",CSMQUALITY_VERY_LOW );

	// Set convars without auto options
	SetConvarsFromVSync();
	SetConvarFromOption( m_pFXAADropDown, "mat_software_aa_strength", 0, false );
	SetConvarFromOption( m_pMatQueueModeDropDown, "mat_queue_mode", -1, false );
	SetConvarFromOption( m_pMotionBlurDropDown, "mat_motion_blur_enabled", 0, false );
	SetConvarFromOption( m_pColorModeDropDown, "mat_monitorgamma_tv_enabled", 0, false );
	SetConvarFromOption( m_pPowerSavingsDropDown, "mat_powersavingsmode", 0, false );
	
	engine->ClientCmd_Unrestricted( "mat_savechanges\n" );

	return true;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CCSGO_VideoSettingsScreen::EventVideoSettingsResetDefaults( void )
{
	KeyValues *pConfigKeys = new KeyValues( "VideoConfig" );
	if ( !pConfigKeys )
		return true;

	// Reset convars not saved in config
	SetConvarToDefault( "mat_monitorgamma_tv_enabled" );
	SetConvarToDefault( "mat_powersavingsmode");

	// Read default config
	if ( !ReadCurrentVideoConfig( pConfigKeys, true ) )
	{
		pConfigKeys->deleteThis();
		return true;
	}

	SetResolutionBoxesFromConfig( pConfigKeys );

	// Set applicable controls to Auto
	ConVarRef( "cpu_level_optionsui" ).SetValue( AUTO_OPTION_VALUE ); 
	ConVarRef( "gpu_level_optionsui" ).SetValue( AUTO_OPTION_VALUE );
	ConVarRef( "mat_forceaniso_optionsui" ).SetValue( AUTO_OPTION_VALUE );
	ConVarRef( "gpu_mem_level_optionsui" ).SetValue( AUTO_OPTION_VALUE );
	ConVarRef( "csm_quality_level_optionsui" ).SetValue( AUTO_OPTION_VALUE );
	
	SetAdvancedOptionsFromConvars();

	ConVarRef( "mat_antialias_optionsui" ).SetValue( AUTO_OPTION_VALUE );
	ConVarRef( "mat_aaquality_optionsui" ).SetValue( AUTO_OPTION_VALUE );
	SetAAModeFromConvars();

	SetOptionFromConfig( pConfigKeys, "mat_software_aa_strength", m_pFXAADropDown );
	SetOptionFromConfig( pConfigKeys, "mat_motion_blur_enabled", m_pMotionBlurDropDown );
	SetQueueModeFromConfig( pConfigKeys );
	SetOptionFromConvar( "mat_monitorgamma_tv_enabled", m_pColorModeDropDown );
	SetOptionFromConvar( "mat_powersavingsmode", m_pPowerSavingsDropDown );


	SetVSyncFromConfig( pConfigKeys );
	
	pConfigKeys->deleteThis();

	return true;
}