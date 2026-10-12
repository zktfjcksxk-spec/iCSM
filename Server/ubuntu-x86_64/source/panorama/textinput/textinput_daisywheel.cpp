//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx_client.h"
#include "panorama/textinput/textinput_daisywheel.h"
#include "panorama/controls/label.h"
#include "panorama/controls/textentry.h"
#include "panorama/input/gamepadcodes.h"
#include "panorama/iuiengine.h"
#include "panorama/iuisoundsystem.h"
#include "panorama/panoramacurves.h"
#include "panorama/renderer/styleproperties.h"
#include "textinput_suggest.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

static const char * k_pchDaisyStyleNoCancel( "NoCancel" );
static const char * k_pchItemStylePicked( "Picked" );
static const char * k_pchGroupSelection( "SelectionGroup" );
static const char * k_pchGroupRsNeedDebounce( "NeedDebounce" );
static const char * k_pchColdLS( "ColdLS" );
static const char * k_pchInSettings( "InSettings" );
static const char * k_pchShowingLanguage( "ShowingLanguage" );

ConVar s_convarPanoramaDaisyWheel( "@panorama_daisy_wheel", "ABXY", 0, "Daisy wheel input mode: RS | ABXY" );

const char k_Side_West =  'W';
const char k_Side_East =  'E';
const char k_Side_North = 'N';
const char k_Side_South = 'S';

// when we spot this glyph on a daisywheel label, we turn it into \r\n
const char *k_szReturnGlyph = "\xE2\x8F\x8E"; // this is U+23CE 'RETURN SYMBOL' in UTF-8

// when in email or URL mode, and we spot this daisywheel label, we replace it with ".com"
const char *k_szBecomesDotCom = ",";

#define JOYSTICK_AXIS_REPEAT_INTERVAL_START 0.35f
#define JOYSTICK_AXIS_REPEAT_INTERVAL_END 0.05f
#define JOYSTICK_AXIS_REPEAT_CURVE_TIME 0.85f

DECLARE_PANORAMA_EVENT0( TextInputDaisyWheelOnGamePadAnalogTriggersChanged );
DEFINE_PANORAMA_EVENT( TextInputDaisyWheelOnGamePadAnalogTriggersChanged );

DECLARE_PANORAMA_EVENT0( DaisyWheelSettings );
DEFINE_PANORAMA_EVENT( DaisyWheelSettings );

DECLARE_PANORAMA_EVENT0( DaisyWheelSwitchLanguage );
DEFINE_PANORAMA_EVENT( DaisyWheelSwitchLanguage );

DECLARE_PANORAMA_EVENT1( DaisyWheelShowThisLanguage, ELanguage );
DEFINE_PANORAMA_EVENT( DaisyWheelShowThisLanguage );

DECLARE_PANORAMA_EVENT0( DaisyWheelNextFocus );
DEFINE_PANORAMA_EVENT( DaisyWheelNextFocus );


//////////////////////////////////////////////////////////////////////////
//
// Daisy wheel learning emoji from its container/caller
//
void CTextInputDaisyWheel::AddEmoticon( const char *szInsert, const char *szImageURL )
{
	int iVec = m_vecEmoji.AddToTail();
	Emoticon_t &emoticon = m_vecEmoji[ iVec ];
	emoticon.sType = szInsert;
	emoticon.sImageURL = szImageURL;
}


//////////////////////////////////////////////////////////////////////////
//
// Daisy wheel instantiating emoji
//
void CTextInputDaisyWheel::CommitEmoticons()
{
	if ( m_bLoadedEmoji )
		return;

	int cEmoji = m_vecEmoji.Count();
	if ( cEmoji == 0 )
		return;

	// build a layout by hand
	CDaisyConfig *pConfig = new CDaisyConfig( "#TextInput_EMOJI" );
	CUtlVector< char > vecText;
	int rgich[ k_cItemsPerLayoutMax ] = { 0 };

	for ( int i = 0; i < MIN( cEmoji, k_cItemsPerLayoutMax ); i++ )
	{
		rgich[ i ] = vecText.Count();
		vecText.AddMultipleToTail( 1 + V_strlen( m_vecEmoji[ i ].sType.String() ), m_vecEmoji[ i ].sType.String() );
	}

	// size
	pConfig->m_cItems = cEmoji;

	// indices
	COMPILE_TIME_ASSERT( sizeof( rgich ) == sizeof( pConfig->m_rgich ) );
	V_memcpy( pConfig->m_rgich, rgich, sizeof( rgich ) );

	// text block
	pConfig->m_vecText.Swap( vecText );

	m_mapConfigEntries.InsertOrReplace( k_EDaisyConfigEmoji, pConfig );
}


//////////////////////////////////////////////////////////////////////////
//
// Daisy wheel input group
//

class CTextInputDaisyGroup: public panorama::CPanel2D
{
	DECLARE_PANEL2D( CTextInputDaisyGroup, panorama::CPanel2D );

public:
	CTextInputDaisyGroup( panorama::CPanel2D *pParent, char const *pchPanelID ) :
		panorama::CPanel2D( pParent, pchPanelID )
	{
		DbgVerify( BLoadLayout( "file://{resources}/layout/textinput/text_input_daisy_group.xml" ) );
	}
};
REGISTER_PANEL2D_FACTORY( CTextInputDaisyGroup, TextInputDaisyGroup )


//////////////////////////////////////////////////////////////////////////
//
// Daisy wheel input item inside input group
//

DECLARE_PANEL_EVENT1( TextInputPickPanelDebouncePicked, int );
DEFINE_PANORAMA_EVENT( TextInputPickPanelDebouncePicked );
class CTextInputPickPanel: public panorama::CPanel2D
{
	DECLARE_PANEL2D( CTextInputPickPanel, panorama::CPanel2D );

public:
	//-----------------------------------------------------------------------------
	//	Constructor
	//-----------------------------------------------------------------------------
	CTextInputPickPanel( panorama::CPanel2D *pParent, char const *pchPanelID ) :
		panorama::CPanel2D( pParent, pchPanelID ),
		m_nPickCounter( 0 ),
		m_bNavigator( false )
	{
		RegisterEventHandler( TextInputPickPanelDebouncePicked(), this, &CTextInputPickPanel::HandleTextInputPickPanelDebouncePicked );
	}


	//-----------------------------------------------------------------------------
	//	Destructor
	//-----------------------------------------------------------------------------
	~CTextInputPickPanel()
	{
		
	}

	//-----------------------------------------------------------------------------
	//	variant for group navigation
	//-----------------------------------------------------------------------------
	void SetIsNavigator( bool bNavigator )
	{
		m_bNavigator = bNavigator;
	}

	bool BIsNavigator() { return m_bNavigator; }

public:
	//-----------------------------------------------------------------------------
	//	Play pick animation with a given debounce delay
	//-----------------------------------------------------------------------------
	int SetTextInputPicked( float flDebounceDelay )
	{
		if ( BHasClass( k_pchItemStylePicked ) )
		{
			RemoveClass( k_pchItemStylePicked );
			ApplyStyles( false );
		}
		AddClass( k_pchItemStylePicked );
		++ m_nPickCounter;
		panorama::DispatchEventAsync( flDebounceDelay, TextInputPickPanelDebouncePicked(), this, m_nPickCounter );
		return m_nPickCounter;
	}


	//-----------------------------------------------------------------------------
	//	Stop picking animations and mark it debounced
	//-----------------------------------------------------------------------------
	void SetTextInputDebounced()
	{
		if ( BHasClass( k_pchItemStylePicked ) )
		{
			++ m_nPickCounter;
			RemoveClass( k_pchItemStylePicked );
			ApplyStyles( false );
		}
	}

private:
	//-----------------------------------------------------------------------------
	//	Handle an event to stop picking animations and mark it debounced
	//-----------------------------------------------------------------------------
	bool HandleTextInputPickPanelDebouncePicked( const panorama::CPanelPtr< panorama::IUIPanel > &ptr, int nPickCounter )
	{
		if ( ptr.Get() == UIPanel() && nPickCounter == m_nPickCounter )
		{
			SetTextInputDebounced();
		}
		return true;
	}

private:
	int m_nPickCounter;		// Pick counter to allow cancelling animations correctly during rapid typing
	bool m_bNavigator;		// if true, this guy allows the user to navigate between groups rather than enter text - NYI
};
REGISTER_PANEL2D_FACTORY( CTextInputPickPanel, TextInputPickPanel )


//////////////////////////////////////////////////////////////////////////
//
// Daisy wheel typing input method
//

REGISTER_PANEL2D( CTextInputDaisyWheel, TextInputDaisy );


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CTextInputDaisyWheel::CTextInputDaisyWheel( panorama::IUIWindow *pParent, const CTextInputHandlerSettings &settings, ITextInputControl *pTextControl ) :
	CTextInputHandler( pParent, settings.GetID() ),
	m_repeatFunction( MAKE_SCHEDULED_FUNC( CTextInputDaisyWheel::ScheduledKeyRepeatFunction ) )
{
	Initialize( settings, pTextControl );
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CTextInputDaisyWheel::CTextInputDaisyWheel( panorama::CPanel2D *parent, const CTextInputHandlerSettings &settings, ITextInputControl *pTextControl ) :
	CTextInputHandler( parent, settings.GetID() ),
	m_repeatFunction( MAKE_SCHEDULED_FUNC( CTextInputDaisyWheel::ScheduledKeyRepeatFunction ) )
{
	Initialize( settings, pTextControl );
}


//-----------------------------------------------------------------------------
// Purpose: Called by constructors to initialize object
//-----------------------------------------------------------------------------
void CTextInputDaisyWheel::Initialize( const CTextInputHandlerSettings &settings, ITextInputControl *pTextControl )
{
	m_mode = k_ETextInputModeNormal;
	m_bDisplaySuggestions = true;
	m_pTextInputControl = pTextControl;
	m_eConfigCurrent = k_EDaisyConfigCaps;
	m_bRestrictConfig = false;
	m_pStickSnd = NULL;	
	m_language = k_Lang_None;
	m_pLang = NULL;
	m_plabelSuggestionPrefix = NULL;
	m_plabelSuggestionSuffix = NULL;
	m_psuggest = NULL;
	m_pYbuttonAction = NULL;
	m_pYButtonText = NULL;
	m_bLoadedEmoji = false;
	m_vecRightPadPos.x = 0.0f;
	m_vecRightPadPos.y = 0.0f;
	m_eSteamRightStickPos = k_RightStick_None;
	m_bDisableRightTrigger = false;
	m_bDisableRightBumper = false;
	m_bDisableLeftTrigger = false;
	m_bDisableLanguageSelect = false;
	m_bOnlySpacesEnteredSinceBackspace = false;
	m_bDoubleSpaceToDotSpace = false;
	m_bAutoCaps = false;

	Assert( m_pTextInputControl != NULL );
	DbgVerify( BLoadLayout( "file://{resources}/layout/textinput/text_input_daisy.xml" ) );
	SetAcceptsFocus( true );

	SetInputNamespace( "daisywheel" );
	RegisterEventHandler( TextInputDaisyWheelOnGamePadAnalogTriggersChanged(), this, &CTextInputDaisyWheel::HandleTextInputDaisyWheelOnGamePadAnalogTriggersChanged );
	RegisterEventHandler( panorama::InputFocusLost(), this, &CTextInputDaisyWheel::HandleInputFocusLost );
	RegisterEventHandler( panorama::PropertyTransitionEnd(), this, &CTextInputDaisyWheel::HandlePropertyTransitionEnd );

	RegisterEventHandler( DaisyWheelSwitchLanguage(), this, &CTextInputDaisyWheel::SwitchLanguage );
	RegisterEventHandler( DaisyWheelShowThisLanguage(), this, &CTextInputDaisyWheel::ShowThisLanguage );
	RegisterEventHandler( DaisyWheelNextFocus(), this, &CTextInputDaisyWheel::NextFocus );

	m_pStickPri = FindChildInLayoutFile( "Stick_Pri" );
	Assert( m_pStickPri );

	m_pLang = assert_cast< CLabel* >( FindChildInLayoutFile( "Lang_txt" ) );
	m_pYButtonText = assert_cast< CLabel* >( FindChildInLayoutFile( "Y_txt" ) );
	
	m_plabelSuggestionPrefix = assert_cast< CLabel* >( FindChildInLayoutFile( "Autosuggest_Prefix_txt" ) );
	m_plabelSuggestionSuffix = assert_cast< CLabel* >( FindChildInLayoutFile( "Autosuggest_Suffix_txt" ) );
	
	m_flStickPriSelectOct[0] = GetLayoutFileDefineFloat( "stick_pri_select_oct_lo", float( M_PI/6 ) );
	m_flStickPriSelectOct[1] = GetLayoutFileDefineFloat( "stick_pri_select_oct_hi", float( M_PI/3 ) );
	m_flStickPriMoveScale[0] = GetLayoutFileDefineFloat( "stick_pri_move_scale_x", 1.0f );
	m_flStickPriMoveScale[1] = GetLayoutFileDefineFloat( "stick_pri_move_scale_y", 1.0f );
	m_flStickPriSelectDist[0] = GetLayoutFileDefineFloat( "stick_pri_select_dist_0", 100 );
	m_flStickPriSelectDist[1] = GetLayoutFileDefineFloat( "stick_pri_select_dist_1", 200 );
	m_flStickPriSelectAngleSticky = GetLayoutFileDefineFloat( "stick_pri_select_ang_0", 0 );
	
	m_flStickSndMoveScale[0] = GetLayoutFileDefineFloat( "stick_snd_move_scale_x", 1.0f );
	m_flStickSndMoveScale[1] = GetLayoutFileDefineFloat( "stick_snd_move_scale_y", 1.0f );
	m_flStickSndSelectDist[0] = GetLayoutFileDefineFloat( "stick_snd_select_dist_0", 100 );
	m_flStickSndSelectDist[1] = GetLayoutFileDefineFloat( "stick_snd_select_dist_1", 200 );
	m_flStickSndSelectAngleSticky = GetLayoutFileDefineFloat( "stick_snd_select_ang_0", 0 );
	m_flStickPriColdTime = GetLayoutFileDefineFloat( "stick_pri_cold_time", 1.0f );

	m_flPickedItemTransitionTime = GetLayoutFileDefineFloat( "picked_item_transition_time", 0.1f );

	m_nSelectionGroup[0] = 0;
	m_nSelectionGroup[1] = 0;


	m_bTriggersDownState[0] = false;
	m_bTriggersDownState[1] = false;

	m_bUsedKeyboard = false;
	m_bUsedGamepad = false;
	m_flInputStartTime = 0.0f;

	m_flTimeStickCold = UIEngine()->GetCurrentFrameTime();

	// copied from k_EAnimationEaseIn
	Vector2D vecPoints[4];
	panorama::GetAnimationCurveControlPoints( panorama::k_EAnimationEaseIn, vecPoints );
	m_repeatCurve.SetControlPoints( vecPoints );
	m_repeatGamePadCode = XK_NULL;
	m_repeatStartTime = 0.0f;
	m_repeatNextTime = 0.0f;
	m_repeatCounter = 0;

	EDaisyInputType_t eType = k_EDaisyInputTypeABXY;
	char const *szDaisyInputType = s_convarPanoramaDaisyWheel.GetString();
	if ( !V_stricmp( szDaisyInputType, "RS" ) )
		eType = k_EDaisyInputTypeRS;
	SetDaisyInputType( eType );
	AddClass( k_pchColdLS );

	// FUTURE when we figure out Y button behavior SetYButtonAction( "#UI_Next", DaisyWheelNextFocus::MakeEvent( this ) );

	ELanguage eDefaultInputLang = UIEngine()->UISettings()->GetDefaultInputLanguage();
	if ( eDefaultInputLang == k_Lang_None )
		eDefaultInputLang = UIEngine()->GetCurrentInputLocale();

	// could use the default locale here, but if the user switched languages before initializing
	// panorama, we should honor that
	if ( !LoadInputConfigurationFile( eDefaultInputLang ) )
	{
		// fall back to English
		LoadInputConfigurationFile( k_Lang_English );
	}

	// Sets if converting two spaces to period space to end a sentence is enabled
	m_bDoubleSpaceToDotSpace = settings.BDoubleSpaceToDotSpace();

	// Sets if automatically capitalizing characters is enabled
	m_bAutoCaps = settings.BAutoCaps();

	// Set the requested text input mode
	SetMode( settings.GetMode() );

	// Set whether the daisy wheel can be cancelled with "B" / "Back" button
	if ( !settings.BCancellable() )
		AddClass( k_pchDaisyStyleNoCancel );

	// Suggestions turned off?
	if ( settings.BHideSuggestions() )
		SAFE_DELETE( m_psuggest );

	// Set the done action string
	const char *pszDoneActionString = settings.GetDoneActionString();
	if ( pszDoneActionString != NULL && *pszDoneActionString != 0 )
	{
		assert_cast< panorama::CLabel * >( FindChildInLayoutFile( "Start_txt" ) )->SetText( pszDoneActionString );
		assert_cast< panorama::CLabel * >( FindChildInLayoutFile( "A_txt" ) )->SetText( pszDoneActionString );
		assert_cast< panorama::CLabel * >( FindChildInLayoutFile( "APIN_txt" ) )->SetText( pszDoneActionString );
	}

	// Set the cancel action string
	const char *pszCancelActionString = settings.GetCancelActionString();
	if ( pszCancelActionString != NULL && *pszCancelActionString != 0 )
	{
		assert_cast< panorama::CLabel * >( FindChildInLayoutFile( "B_txt" ) )->SetText( pszCancelActionString );
		assert_cast< panorama::CLabel * >( FindChildInLayoutFile( "BPIN_txt" ) )->SetText( pszCancelActionString );
	}

	// Add extra requested classes
	AddClasses( settings.GetClasses() );
}


//-----------------------------------------------------------------------------
//	Destructor
//-----------------------------------------------------------------------------
CTextInputDaisyWheel::~CTextInputDaisyWheel()
{
	// Free configuration	
	m_mapConfigEntries.RemoveAll();
	SAFE_DELETE( m_psuggest );

	SAFE_DELETE( m_pYbuttonAction );
}


//-----------------------------------------------------------------------------
//	Set daisy wheel type
//-----------------------------------------------------------------------------
void CTextInputDaisyWheel::SetDaisyInputType( EDaisyInputType_t eType )
{
	RemoveClass( "DaisyInputTypeABXY" );
	RemoveClass( "DaisyInputTypeRS" );
	RemoveClass( "DaisyInputTypePIN" );
	switch ( eType )
	{
	case k_EDaisyInputTypeABXY:
		m_pfnOnGamePadDown = &CTextInputDaisyWheel::DaisyABXY_OnGamePadDown;
		m_pfnOnGamePadAnalog = &CTextInputDaisyWheel::DaisyABXY_OnGamePadAnalog;
		AddClass( "DaisyInputTypeABXY" );
		return;
	case k_EDaisyInputTypeRS:
		m_pfnOnGamePadDown = &CTextInputDaisyWheel::DaisyRS_OnGamePadDown;
		m_pfnOnGamePadAnalog = &CTextInputDaisyWheel::DaisyRS_OnGamePadAnalog;
		AddClass( "DaisyInputTypeRS" );
		return;
	case k_EDaisyInputTypePIN:
		m_pfnOnGamePadDown = &CTextInputDaisyWheel::DaisyPIN_OnGamePadDown;
		m_pfnOnGamePadAnalog = &CTextInputDaisyWheel::DaisyPIN_OnGamePadAnalog;
		AddClass( "DaisyInputTypePIN" );
		return;
	}
}


//-----------------------------------------------------------------------------
//	set the input sub-mode (e.g. numeric input, email input, password input)
//
//	currently only used to turn initial caps on/off; later, will turn on
//	things like ".com" macros and numeric input
//-----------------------------------------------------------------------------
void CTextInputDaisyWheel::SetMode( ETextInputMode_t mode )
{
	m_mode = mode;
	m_bDisableRightTrigger = false;
	m_bDisableRightBumper = false;
	m_bDisableLeftTrigger = false;
	m_bDisableLanguageSelect = false;

	switch ( mode )
	{
	case k_ETextInputModePassword:
		// kill suggestion engine
		m_bDisplaySuggestions = false;
		SAFE_DELETE( m_psuggest );
		// and fall through
	case k_ETextInputModeEmail:
	case k_ETextInputModeURL:
		m_bRestrictConfig = false;
		m_bDoubleSpaceToDotSpace = false;
		m_bAutoCaps = false;
		m_eConfigCurrent = k_EDaisyConfigLetters;
		break;

	case k_ETextInputModeNumericPassword:
		SetDaisyInputType( CTextInputDaisyWheel::k_EDaisyInputTypePIN );		
		// fall through
	case k_ETextInputModeNumeric:
		m_bDisplaySuggestions = false;
		SAFE_DELETE( m_psuggest );
		m_bRestrictConfig = true;
		m_eConfigCurrent = k_EDaisyConfigNumbersOnly;
		m_bDisableRightTrigger = true;
		m_bDisableRightBumper = true;
		m_bDisableLeftTrigger = true;
		m_bDisableLanguageSelect = true;
		m_bDoubleSpaceToDotSpace = false;
		m_bAutoCaps = false;
		break;

	case k_ETextInputModePhoneNumber:
		m_bDisplaySuggestions = false;
		SAFE_DELETE( m_psuggest );
		m_bRestrictConfig = true;
		m_eConfigCurrent = k_EDaisyConfigPhoneNumber;
		m_bDisableRightTrigger = true;
		m_bDisableRightBumper = true;
		m_bDisableLeftTrigger = true;
		m_bDisableLanguageSelect = true;
		m_bDoubleSpaceToDotSpace = false;
		m_bAutoCaps = false;
		break;

	case k_ETextInputModeSteamCode:
		m_bDisplaySuggestions = false;
		SAFE_DELETE( m_psuggest );
		m_bRestrictConfig = false;
		m_eConfigCurrent = k_EDaisyConfigSteamCodeChars;
		m_bDisableRightTrigger = true;
		m_bDisableRightBumper = true;
		m_bDisableLeftTrigger = true;
		m_bDisableLanguageSelect = true;
		m_bDoubleSpaceToDotSpace = false;
		m_bAutoCaps = false;		
		break;

	case k_ETextInputModeNormalLower:
		// Like normal, but no initial caps
		m_bRestrictConfig = false;
		m_bAutoCaps = false;
		m_eConfigCurrent = k_EDaisyConfigLetters;
		break;

	case k_ETextInputModeNormal:
	default:
		m_bRestrictConfig = false;
		m_eConfigCurrent = (m_bAutoCaps && BCursorAtStartOfSentence()) ? k_EDaisyConfigCaps : k_EDaisyConfigLetters;
		break;
	}

	SetControlsFromConfiguration();
}


//-----------------------------------------------------------------------------
// Schedule key repeats or cancel scheduled key repeats
//-----------------------------------------------------------------------------
void CTextInputDaisyWheel::ScheduleKeyRepeats( panorama::GamePadCode eCode )
{
	if ( eCode == m_repeatGamePadCode )
		return;

	m_repeatGamePadCode = eCode;
	m_repeatStartTime = UIEngine()->GetCurrentFrameTime();
	m_repeatNextTime = m_repeatStartTime + JOYSTICK_AXIS_REPEAT_INTERVAL_START;
	m_repeatCounter = 0;
	
	if ( m_repeatGamePadCode != XK_NULL )
		m_repeatFunction.Schedule( 0 );
	else
		m_repeatFunction.Cancel();
}


//-----------------------------------------------------------------------------
// Handle gamepad ups
//-----------------------------------------------------------------------------
bool CTextInputDaisyWheel::OnGamePadUp( const panorama::GamePadData_t &code )
{
	switch (code.m_GamePadCode )
	{
		case STEAM_BUTTON_LTRIGGER:
			m_bTriggersDownState[0] = false;
			panorama::DispatchEventAsync( 0.0f, TextInputDaisyWheelOnGamePadAnalogTriggersChanged(), this );
			return true;

		case STEAM_BUTTON_RTRIGGER:
			m_bTriggersDownState[1] = false;
			panorama::DispatchEventAsync( 0.0f, TextInputDaisyWheelOnGamePadAnalogTriggersChanged(), this );
			return true;

		default:
			break;
	}

	if ( ( m_repeatGamePadCode != XK_NULL ) && ( m_repeatGamePadCode == code.m_GamePadCode ) )
		CancelOutstandingRepeats();

	return true;
}


//-----------------------------------------------------------------------------
// Handle gamepad downs
//-----------------------------------------------------------------------------
bool CTextInputDaisyWheel::OnGamePadDown( const panorama::GamePadData_t &code )
{
	return ( this->*m_pfnOnGamePadDown )( code );
}


//-----------------------------------------------------------------------------
// Handle gamepad analog input
//-----------------------------------------------------------------------------
bool CTextInputDaisyWheel::OnGamePadAnalog( const panorama::GamePadData_t &code )
{
	return ( this->*m_pfnOnGamePadAnalog )( code );
}


//-----------------------------------------------------------------------------
// Listen for focus loss
//-----------------------------------------------------------------------------
bool CTextInputDaisyWheel::HandleInputFocusLost( const panorama::CPanelPtr< panorama::IUIPanel > &ptrPanel )
{
	if ( ptrPanel.Get() == UIPanel() )
	{
		CancelOutstandingRepeats();
		CloseHandler( false );
		return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
// Handle scheduled key repeats
//-----------------------------------------------------------------------------
void CTextInputDaisyWheel::ScheduledKeyRepeatFunction()
{
	if ( ( m_repeatGamePadCode == XK_NULL ) || ( 0.0f == m_repeatStartTime ) )
		return;

	m_repeatFunction.Schedule( 0 );
	if ( m_repeatNextTime < UIEngine()->GetCurrentFrameTime() )
	{
		++ m_repeatCounter;
		panorama::GamePadData_t data = { k_ePanelEventSourceProgram, m_repeatGamePadCode };
		OnGamePadDown( data );
		

		// Evaluate when we should repeat next time
		Vector2D vRes;
		m_repeatCurve.Evaluate( clamp( ( UIEngine()->GetCurrentFrameTime() - m_repeatStartTime ) / JOYSTICK_AXIS_REPEAT_CURVE_TIME, 0.0f, 1.0f ), vRes );
		float flDelayTime = JOYSTICK_AXIS_REPEAT_INTERVAL_START;
		flDelayTime = Lerp( vRes.y, JOYSTICK_AXIS_REPEAT_INTERVAL_START, JOYSTICK_AXIS_REPEAT_INTERVAL_END );

		m_repeatNextTime = UIEngine()->GetCurrentFrameTime() + flDelayTime;
	}
}


//-----------------------------------------------------------------------------
// Play sound for a given daisy wheel activity
//-----------------------------------------------------------------------------
void CTextInputDaisyWheel::PlayDaisyActionSound( EDaisyAction_t eAction )
{
	char const *szSoundName = NULL;
	switch ( eAction )
	{
	case k_EDaisySound_ButtonA:
		szSoundName = "txting_press_a";
		break;
	case k_EDaisySound_ButtonB:
		szSoundName = "txting_press_b";
		break;
	case k_EDaisySound_ButtonX:
		szSoundName = "txting_press_x";
		break;
	case k_EDaisySound_ButtonY:
		szSoundName = "txting_press_y";
		break;
	case k_EDaisySound_KeySpacebar:
		szSoundName = "txting_type_spacebar";
		break;
	case k_EDaisySound_KeyBackspace:
		szSoundName = "txting_type_backspace";
		break;
	case k_EDaisySound_KeyLeft:
		szSoundName = "txting_cursor_left";
		break;
	case k_EDaisySound_KeyRight:
		szSoundName = "txting_cursor_right";
		break;
	case k_EDaisySound_KeyHome:
		szSoundName = "txting_cursor_home";
		break;
	case k_EDaisySound_KeyEnd:
		szSoundName = "txting_cursor_end";
		break;
	case k_EDaisySound_FocusAreaChanged:
		szSoundName = "txting_focus";
		break;
	case k_EDaisySound_ConfigChanged:
		szSoundName = "txting_type_main";
		if ( m_mapConfigEntries.Count() > 1 )
		{
			switch ( m_eConfigCurrent )
			{
			case k_EDaisyConfigCaps:
				szSoundName = "txting_type_caps";
				break;
			case k_EDaisyConfigLetters:
				szSoundName = "txting_type_main";
				break;
			case k_EDaisyConfigNumbers:
				szSoundName = "txting_type_numbers";
				break;
			default:
				szSoundName = "txting_type_extra";
				break;
			}
		}
		break;
	case k_EDaisySound_PerformAutosuggest:
		szSoundName = "txting_press_a";
		break;
	}
	
	if ( szSoundName )
	{
		float flVolume = ( m_repeatCounter > 1 ) ? Lerp( clamp(  m_repeatCounter / 10.0f, 0.0f, 1.0f ), 1.0f, 0.6f ) : 1.0f;
		if ( flVolume > 0.0f )
		{
			UISoundSystem()->PlaySound( szSoundName, UIPanel(), k_ESoundType_Effects, flVolume );
		}
	}
}

//-----------------------------------------------------------------------------
// ABXY gamepad down handler
//-----------------------------------------------------------------------------
bool CTextInputDaisyWheel::DaisyABXY_OnGamePadDown( const panorama::GamePadData_t &codePassed )
{
	// Make a local copy we may modify to support various modes more cleanly
	panorama::GamePadData_t code = codePassed;

	m_bUsedGamepad = true;
	if ( BHasClass( k_pchInSettings ) )
	{
		// in settings, we don't use low level tracking
		return false;
	}

	// Check for any of the keys that do suggesting first
	if ( BHasClass( k_pchColdLS ) &&
		m_psuggest != NULL &&
		!m_sSuggestion.IsEmpty() 
		&& 
		( code.m_GamePadCode == STEAM_RIGHTPAD_DOWN 
		|| code.m_GamePadCode == STEAM_RIGHTPAD_UP
		|| code.m_GamePadCode == STEAM_RIGHTPAD_LEFT
		|| code.m_GamePadCode == STEAM_RIGHTPAD_RIGHT
		|| code.m_GamePadCode == XK_BUTTON_A
		|| code.m_GamePadCode == STEAM_BUTTON_A
		|| code.m_GamePadCode == XK_BUTTON_X
		|| code.m_GamePadCode == STEAM_BUTTON_X
		|| code.m_GamePadCode == STEAM_BUTTON_RPAD_CLICKED
		|| code.m_GamePadCode == STEAM_BUTTON_LPAD_CLICKED
		) )
	{
		if ( code.m_GamePadCode == STEAM_BUTTON_RPAD_CLICKED
			|| code.m_GamePadCode == STEAM_RIGHTPAD_DOWN 
			|| code.m_GamePadCode == STEAM_RIGHTPAD_UP
			|| code.m_GamePadCode == STEAM_RIGHTPAD_LEFT
			|| code.m_GamePadCode == STEAM_RIGHTPAD_RIGHT )
		{
			// Don't do anything if we aren't in tap mode
		}
		else if ( code.m_GamePadCode == STEAM_BUTTON_RPAD_CLICKED )
		{
			// Don't do anything because we are in tap mode
			return false;
		}
		else
		{
			PlayDaisyActionSound( k_EDaisySound_PerformAutosuggest );

			// transcode at the last minute, and put the text into the textentry
			CStrAutoEncodeSrc2 s( m_sSuggestion );
			m_pTextInputControl->InsertCharactersAtCursor( s.ToUTF32(), V_strlen32( s.ToUTF32() ) );

			// peek at the character under the caret to see if we should insert a space too
			uchar32 ch32 = m_pTextInputControl->Pch32GetText()[ m_pTextInputControl->GetCursorOffset() ];
			if ( ch32 != ' ' )
			{
				// TODO wide vs. narrow spaces are going to be a problem here and elsewhere.
				TypeUniChar( ' ' );
			}
			ClearSuggestionState();
			return true;
		}
	}

	if ( code.m_GamePadCode == STEAM_BUTTON_RPAD_CLICKED
		|| code.m_GamePadCode == STEAM_RIGHTPAD_DOWN 
		|| code.m_GamePadCode == STEAM_RIGHTPAD_UP
		|| code.m_GamePadCode == STEAM_RIGHTPAD_LEFT
		|| code.m_GamePadCode == STEAM_RIGHTPAD_RIGHT ) 
	{
		return false;
	}

	switch ( code.m_GamePadCode )
	{
	case XK_BUTTON_B:
	case STEAM_BUTTON_B:
	case STEAM_RIGHTPAD_RIGHT:
		if( !BHasClass( k_pchColdLS ) )
		{
			if ( code.m_RepeatCount > 0 )
				return true;

			ScheduleKeyRepeats( code.m_GamePadCode );
			return TypeCharacterFromSide( k_Side_East );
		}
		// else fall through
		if ( !BHasClass( k_pchDaisyStyleNoCancel ) && code.m_GamePadCode != STEAM_RIGHTPAD_RIGHT  )
		{
			CloseHandler( false );
		}
		return true;

	case XK_BUTTON_A:
	case STEAM_BUTTON_A:
	case STEAM_RIGHTPAD_DOWN:
	case STEAM_BUTTON_RPAD_CLICKED:
		if( !BHasClass( k_pchColdLS ) && code.m_GamePadCode != STEAM_BUTTON_RPAD_CLICKED )
		{
			if ( code.m_RepeatCount > 0 )
				return true;

			ScheduleKeyRepeats( code.m_GamePadCode );
			return TypeCharacterFromSide( k_Side_South );
		}

		if ( code.m_GamePadCode == XK_BUTTON_A || code.m_GamePadCode == STEAM_BUTTON_A )
		{
			if ( BHasClass( "DontCloseOnSubmit" ) )
			{
				SubmitTextNoClose();
			}
			else
			{
				CloseHandler( true );
			}
			return true;
		}

		if ( code.m_GamePadCode != STEAM_RIGHTPAD_DOWN )
		{
			CloseHandler( true );
		}
		return true;

	case XK_BUTTON_START:
	case XK_BUTTON_BACK:
	case XK_BUTTON_STICK2:
	case STEAM_BUTTON_START:
	case STEAM_BUTTON_SELECT:
		DispatchEvent( TextInputUnhandledButtonPress(), m_pTextInputControl->GetAssociatedPanel(), code );
		return false; // handled by high level input handler

	case XK_BUTTON_X:
	case STEAM_BUTTON_X:
	case STEAM_RIGHTPAD_LEFT:
		// Type if group is selected
		if ( (m_nSelectionGroup[0] || m_nSelectionGroup[1]) )
		{
			if ( code.m_RepeatCount > 0 )
				return true;

			ScheduleKeyRepeats( code.m_GamePadCode );
			return TypeCharacterFromSide( k_Side_West );
		}

		DispatchEvent( TextInputUnhandledButtonPress(), m_pTextInputControl->GetAssociatedPanel(), code );
		return false;
		// else fall through
	case XK_BUTTON_LEFT_SHOULDER:
	case STEAM_BUTTON_LSHOULDER:
		if ( code.m_RepeatCount > 0 )
			return true;

		ScheduleKeyRepeats( code.m_GamePadCode );
		PlayDaisyActionSound( k_EDaisySound_KeyBackspace );
		ClearSuggestionState();
		return TypeKeyDown( KEY_BACKSPACE );

	case STEAM_BUTTON_Y:
	case XK_BUTTON_Y:
	case STEAM_RIGHTPAD_UP:
		// Type if group is selected
		if( (m_nSelectionGroup[0] || m_nSelectionGroup[1]) )
		{
			if ( code.m_RepeatCount > 0 )
				return true;

			ScheduleKeyRepeats( code.m_GamePadCode );
			return TypeCharacterFromSide( k_Side_North );
		}
		return false;
		// else fall through
	case XK_BUTTON_RIGHT_SHOULDER:
	case STEAM_BUTTON_RSHOULDER:
		if ( m_bDisableRightBumper )
			return true;
		if ( code.m_RepeatCount > 0 )
			return true;

		ScheduleKeyRepeats( code.m_GamePadCode );
		PlayDaisyActionSound( k_EDaisySound_KeySpacebar );
		ClearSuggestionState();
		return TypeUniChar( ' ' );

	case XK_BUTTON_LEFT:
	case STEAM_BUTTON_LBACK:
	case STEAM_BUTTON_DPAD_LEFT:
		if ( code.m_RepeatCount > 0 )
			return true;
		ScheduleKeyRepeats( code.m_GamePadCode );
		PlayDaisyActionSound( k_EDaisySound_KeyLeft );
		ClearSuggestionState();
		return TypeKeyDown( KEY_LEFT );

	case XK_BUTTON_RIGHT:
	case STEAM_BUTTON_RBACK:
	case STEAM_BUTTON_DPAD_RIGHT:
		if ( code.m_RepeatCount > 0 )
			return true;
		ScheduleKeyRepeats( code.m_GamePadCode );
		PlayDaisyActionSound( k_EDaisySound_KeyRight );
		ClearSuggestionState();
		return TypeKeyDown( KEY_RIGHT );

	case XK_BUTTON_UP:
	case STEAM_BUTTON_DPAD_UP:
		PlayDaisyActionSound( k_EDaisySound_KeyHome );
		ClearSuggestionState();
		return TypeKeyDown( KEY_UP );

	case XK_BUTTON_DOWN:
	case STEAM_BUTTON_DPAD_DOWN:
		PlayDaisyActionSound( k_EDaisySound_KeyEnd );
		ClearSuggestionState();
		return TypeKeyDown( KEY_DOWN );

	case STEAM_BUTTON_LTRIGGER:
		if ( m_bDisableLeftTrigger )
			return true;

		if ( code.m_RepeatCount < 1 )
		{
			m_bTriggersDownState[0] = true;
			panorama::DispatchEventAsync( 0.0f, TextInputDaisyWheelOnGamePadAnalogTriggersChanged(), this );
			return true;
		}

	case STEAM_BUTTON_RTRIGGER:
		if ( m_bDisableRightTrigger )
			return true;

		if ( code.m_RepeatCount < 1 )
		{
			m_bTriggersDownState[1] = true;
			panorama::DispatchEventAsync( 0.0f, TextInputDaisyWheelOnGamePadAnalogTriggersChanged(), this );
			return true;
		}

	default:
		return true;
	}
	// return BaseClass::OnGamePadDown( code );
}


//-----------------------------------------------------------------------------
// RS gamepad down handler
//-----------------------------------------------------------------------------
bool CTextInputDaisyWheel::DaisyRS_OnGamePadDown( const panorama::GamePadData_t &code )
{
	m_bUsedGamepad = true;

	switch ( code.m_GamePadCode )
	{
	case XK_BUTTON_B:
	case STEAM_BUTTON_B:
		// Only if no group is selected
		if ( m_nSelectionGroup[0] || m_nSelectionGroup[1] )
			return true;
		// fall through:
		if ( !BHasClass( k_pchDaisyStyleNoCancel ) )
		{
			CloseHandler( false );
		}
		return true;

	case XK_BUTTON_BACK:
	case STEAM_BUTTON_SELECT:
		return false; // handled by high level input handler

	case XK_BUTTON_A:
	case STEAM_BUTTON_A:
		// Only if no group is selected
		if ( m_nSelectionGroup[0] || m_nSelectionGroup[1] )
			return true;
		// fall through:
	case XK_BUTTON_START:
	case STEAM_BUTTON_START:
		if ( !DispatchEvent( TextInputUnhandledButtonPress(), m_pTextInputControl->GetAssociatedPanel(), code ) )
		{
			CloseHandler( true );
		}
		return true;

	case STEAM_BUTTON_LTRIGGER:
	case STEAM_BUTTON_RTRIGGER:
		return OnGamePadAnalog_Trigger( code );

	case XK_BUTTON_X:
	case STEAM_BUTTON_X:
		{
			TypeKeyDown( KEY_BACKSPACE );
		}
		return true;
	case XK_BUTTON_Y:
	case STEAM_BUTTON_Y:
		return TypeUniChar( ' ' );

	case XK_BUTTON_LEFT_SHOULDER:
	case XK_BUTTON_LEFT:
	case STEAM_BUTTON_LSHOULDER:
	case STEAM_BUTTON_LBACK:
		return TypeKeyDown( KEY_LEFT );

	case XK_BUTTON_RIGHT_SHOULDER:
	case STEAM_BUTTON_RSHOULDER:
		// at the last character type a space
		if ( m_pTextInputControl->GetCursorOffset() >= ( int ) m_pTextInputControl->GetCharCount() )
			return TypeUniChar( ' ' );
		// else fall through:
	case XK_BUTTON_RIGHT:
	case STEAM_BUTTON_RBACK:
		return TypeKeyDown( KEY_RIGHT );

	case XK_BUTTON_UP:
		return TypeKeyDown( KEY_HOME );

	case XK_BUTTON_DOWN:
		return TypeKeyDown( KEY_END );

	default:
		return true;
	}
	// return BaseClass::OnGamePadDown( code );
}


//-----------------------------------------------------------------------------
// PINPad gamepad down handler
//-----------------------------------------------------------------------------
bool CTextInputDaisyWheel::DaisyPIN_OnGamePadDown( const panorama::GamePadData_t &codePassed )
{
	// Make a local copy we may modify to support various modes more cleanly
	panorama::GamePadData_t code = codePassed;

	m_bUsedGamepad = true;
	if ( BHasClass( k_pchInSettings ) )
	{
		// in settings, we don't use low level tracking
		return false;
	}

	switch ( code.m_GamePadCode )
	{
	case XK_BUTTON_LEFT_SHOULDER:
	case STEAM_BUTTON_LSHOULDER:
		PlayDaisyActionSound( k_EDaisySound_ButtonA );
		return TypeUniChar( '1' );
	case XK_BUTTON_Y:
	case STEAM_BUTTON_Y:
		PlayDaisyActionSound( k_EDaisySound_ButtonA );
		return TypeUniChar( '2' );
	case XK_BUTTON_RIGHT_SHOULDER:
	case STEAM_BUTTON_RSHOULDER:
		PlayDaisyActionSound( k_EDaisySound_ButtonA );
		return TypeUniChar( '3' );
	case XK_BUTTON_LTRIGGER:
	case STEAM_BUTTON_LTRIGGER:
		PlayDaisyActionSound( k_EDaisySound_ButtonA );
		return TypeUniChar( '4' );
	case XK_BUTTON_X:
	case STEAM_BUTTON_X:
		PlayDaisyActionSound( k_EDaisySound_ButtonA );
		return TypeUniChar( '5' );
	case XK_BUTTON_RTRIGGER:
	case STEAM_BUTTON_RTRIGGER:
		PlayDaisyActionSound( k_EDaisySound_ButtonA );
		return TypeUniChar( '6' );
	case XK_BUTTON_LEFT:
	case STEAM_BUTTON_DPAD_LEFT:
		PlayDaisyActionSound( k_EDaisySound_ButtonA );
		return TypeUniChar( '7' );
	case XK_BUTTON_UP:
	case STEAM_BUTTON_DPAD_UP:
		PlayDaisyActionSound( k_EDaisySound_ButtonA );
		return TypeUniChar( '8' );
	case XK_BUTTON_RIGHT:
	case STEAM_BUTTON_DPAD_RIGHT:
		PlayDaisyActionSound( k_EDaisySound_ButtonA );
		return TypeUniChar( '9' );
	case XK_BUTTON_DOWN:
	case STEAM_BUTTON_DPAD_DOWN:
		PlayDaisyActionSound( k_EDaisySound_ButtonA );
		return TypeUniChar( '0' );

	case XK_BUTTON_BACK:
	case STEAM_BUTTON_SELECT:
		if ( code.m_RepeatCount > 0 )
			return true;

		ScheduleKeyRepeats( code.m_GamePadCode );
		PlayDaisyActionSound( k_EDaisySound_KeyBackspace );
		ClearSuggestionState();
		return TypeKeyDown( KEY_BACKSPACE );

	case XK_BUTTON_B:
	case STEAM_BUTTON_B:
		if ( !BHasClass( k_pchDaisyStyleNoCancel ) )
		{
			CloseHandler( false );
		}
		return true;

	case XK_BUTTON_A:
	case STEAM_BUTTON_A:
		CloseHandler( true );
		return true;

	case XK_BUTTON_START:
	case STEAM_BUTTON_START:
	case STEAM_BUTTON_LBACK:
	case STEAM_BUTTON_RBACK:
		return false; // handled by high level input handler

	default:
		return true;
	}
}


//-----------------------------------------------------------------------------
// ABXY gamepad analog handler
//-----------------------------------------------------------------------------
bool CTextInputDaisyWheel::DaisyABXY_OnGamePadAnalog( const panorama::GamePadData_t &code )
{
	// Kyle says: need to update this code once we figure out what's happening with daisywheel
	//			  and then unregress everything. See OnActiveControllerTypeChangedDefaultHandler.
	if( UIInputEngine()->GetActiveControllerType() == k_EActiveControllerType_Steam )
	{
		AddClass( "D0gPad" );
		AddClass( "XInputPad" );
		RemoveClass( "SteamPad" );
	}
	else
	{
		AddClass( "XInputPad" );
		RemoveClass( "SteamPad" );
		RemoveClass( "D0gPad" );
	}

	switch ( code.m_GamePadCode )
	{
	case XK_STICK1_ANALOG:
	case STEAM_LEFTSTICK_ANALOG:
		return OnGamePadAnalog_ProcessLeftStickForGroup( code );

	case XK_STICK2_ANALOG:
	case STEAM_RIGHTPAD_ANALOG:
		// don't dispatch this event unless we know we have to; right now the only consumer of it
		// is only interested in the following case. For a more general consumer of this event, we
		// might want to use a pub/sub model of some kind.
		if ( ( code.m_GamePadCode == XK_STICK2_ANALOG ) && ( code.m_fValueY != 0 ) )
		{
			DispatchEvent( TextInputAnalogStickPassthrough(), m_pTextInputControl->GetAssociatedPanel(), code );
		}
		return false;

	case XK_BUTTON_LTRIGGER:
	case XK_BUTTON_RTRIGGER:
	case STEAM_BUTTON_LTRIGGER:
	case STEAM_BUTTON_RTRIGGER:
		return OnGamePadAnalog_Trigger( code );
	}
	return true;
}


//-----------------------------------------------------------------------------
// RS gamepad analog handler
//-----------------------------------------------------------------------------
bool CTextInputDaisyWheel::DaisyRS_OnGamePadAnalog( const panorama::GamePadData_t &code )
{
	switch ( code.m_GamePadCode )
	{
	case XK_STICK1_ANALOG:
	case STEAM_LEFTSTICK_ANALOG:
		return OnGamePadAnalog_ProcessLeftStickForGroup( code );

	case XK_STICK2_ANALOG:
	case STEAM_RIGHTPAD_ANALOG:
		return OnGamePadAnalog_ProcessRightStickForSide( code );

	case XK_BUTTON_LTRIGGER:
	case XK_BUTTON_RTRIGGER:
	case STEAM_BUTTON_LTRIGGER:
	case STEAM_BUTTON_RTRIGGER:
		return OnGamePadAnalog_Trigger( code );
	}
	return true;
}


//-----------------------------------------------------------------------------
// PINPad gamepad analog handler
//-----------------------------------------------------------------------------
bool CTextInputDaisyWheel::DaisyPIN_OnGamePadAnalog( const panorama::GamePadData_t &code )
{
	// Kyle says: need to update this code once we figure out what's happening with daisywheel
	//			  and then unregress everything. See OnActiveControllerTypeChangedDefaultHandler.
	if( UIInputEngine()->GetActiveControllerType() == k_EActiveControllerType_Steam )
	{
		AddClass( "D0gPad" );
		AddClass( "XInputPad" );
		RemoveClass( "SteamPad" );
	}
	else
	{
		AddClass( "XInputPad" );
		RemoveClass( "SteamPad" );
		RemoveClass( "D0gPad" );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Processes left stick analog location, selects the appropriate input group,
//	animates left stick helper UI object to provide motion feedback
//-----------------------------------------------------------------------------
bool CTextInputDaisyWheel::OnGamePadAnalog_ProcessLeftStickForGroup( const panorama::GamePadData_t &code )
{
	float flX = m_flStickPriMoveScale[0] * code.m_fValueX;
	float flY = m_flStickPriMoveScale[1] * code.m_fValueY;

	CUtlVector<CTransform3D *> vecTransforms;
	vecTransforms.AddToTail( new CTransformTranslate3D( flX*(1.0f/GetActualUIScaleX()), flY*(1.0f/GetActualUIScaleY()), 0.0f ) );
	m_pStickPri->SetTransform3D( vecTransforms );

	// Compute stick distance from center
	float flDistSq = flX * flX + flY * flY;

	// Check if the stick entered selection threshold
	int nSelOld[2] = { m_nSelectionGroup[0], m_nSelectionGroup[1] };
	float flDistanceRequired = ( !m_nSelectionGroup[0] && !m_nSelectionGroup[1] )
		// If something is already selected then allow grace margin for debounce
		? m_flStickPriSelectDist[1] : m_flStickPriSelectDist[0];
	if ( flDistSq > flDistanceRequired )
	{
		// Compute which group is now selected
		if ( ( flX > -10 ) && ( flX < 10 ) )
		{
			m_nSelectionGroup[0] = 0;
			m_nSelectionGroup[1] = ( flY > 0 ) ? 1 : -1;
		}
		else
		{
			int nX = ( flX > 0 ) ? 1 : -1;
			float flTan = flY / flX;
			float flAngle = atan( flTan );
			float const flExtraAngle = m_flStickPriSelectAngleSticky;
			float octHi = m_flStickPriSelectOct[1], octLo = m_flStickPriSelectOct[0];
			
			double arrAngles[] = {			octHi,		octLo,		-octLo,		-octHi };
			int arrSelectionGroup0[] = {	0,			nX,			nX,			nX,			0 };
			int arrSelectionGroup1[] = {	nX,			nX,			0,			-nX,		-nX };
			
			bool bSelectionSet = false;
			for ( int jj = 0; jj < V_ARRAYSIZE( arrAngles ); ++ jj )
			{
				if ( ( flAngle > arrAngles[jj] + flExtraAngle ) ||
					( ( flAngle > arrAngles[jj] ) && ( m_nSelectionGroup[0] != arrSelectionGroup0[jj+1] || m_nSelectionGroup[1] != arrSelectionGroup1[jj+1] ) ) ||
					( ( flAngle > arrAngles[jj] - flExtraAngle ) && ( m_nSelectionGroup[0] == arrSelectionGroup0[jj] && m_nSelectionGroup[1] == arrSelectionGroup1[jj] ) ) )
				{
					m_nSelectionGroup[0] = arrSelectionGroup0[jj];
					m_nSelectionGroup[1] = arrSelectionGroup1[jj];
					bSelectionSet = true;
					break;
				}
			}
			if ( !bSelectionSet )
			{
				m_nSelectionGroup[0] = arrSelectionGroup0[V_ARRAYSIZE(arrAngles)];
				m_nSelectionGroup[1] = arrSelectionGroup1[V_ARRAYSIZE(arrAngles)];
			}
		}
	}
	else
	{
		m_nSelectionGroup[0] = 0;
		m_nSelectionGroup[1] = 0;
	}

	float flOpacity = ( m_flStickPriSelectDist[1] - flDistSq ) / m_flStickPriSelectDist[1];
	if ( flOpacity < 0 )
		flOpacity = 0;
	m_pStickPri->SetOpacity( flOpacity );

	//
	// Select new group
	//
	if ( m_nSelectionGroup[0] != nSelOld[0] || m_nSelectionGroup[1] != nSelOld[1] )
	{
		char const *szGroupOld = GetGroupNameSq( nSelOld[0], nSelOld[1] );
		if ( szGroupOld && *szGroupOld )
			FindChildInLayoutFile( CFmtStr32( "Group_%s", szGroupOld ).String() )->RemoveClass( k_pchGroupSelection );

		if ( m_pStickSnd )
		{
			m_pStickSnd->SetOpacity( 0.0 );
			m_pStickSnd->AddClass( k_pchGroupRsNeedDebounce );
			m_pStickSnd = NULL;
		}

		char const *szGroupNew = GetGroupNameSq( m_nSelectionGroup[0], m_nSelectionGroup[1] );
		if ( szGroupNew && *szGroupNew )
		{
			panorama::CPanel2D *pNewGroup = FindChildInLayoutFile( CFmtStr32( "Group_%s", szGroupNew ).String() );
			pNewGroup->AddClass( k_pchGroupSelection );

			m_pStickSnd = pNewGroup->FindChild( "Stick_Snd" );
			m_pStickSnd->SetOpacity( 0.0 );
			m_pStickSnd->AddClass( k_pchGroupRsNeedDebounce );
		}

		CancelOutstandingRepeats();

		UIEngine()->PulseActiveControllerHaptic( IUIEngine::k_EHapticFeedbackPosition_Left, IUIEngine::k_EHapticFeedbackStrength_Low );
		PlayDaisyActionSound( k_EDaisySound_FocusAreaChanged );
	}

	//
	// Track cold state
	//
	if ( m_nSelectionGroup[0] == 0 && m_nSelectionGroup[1] == 0 )
	{
		// cold stick
		if ( ( ( UIEngine()->GetCurrentFrameTime() - m_flTimeStickCold ) > m_flStickPriColdTime ) &&
			!BHasClass( k_pchColdLS ) )
		{
			AddClass( k_pchColdLS );
		}
	}
	else
	{
		// hot stick
		if ( BHasClass( k_pchColdLS ) )
			RemoveClass( k_pchColdLS );
		m_flTimeStickCold = UIEngine()->GetCurrentFrameTime();
	}
	return true;
}


//-----------------------------------------------------------------------------
// Processes right stick analog location, selects the appropriate letter to print,
//	animates right stick helper UI object to provide motion feedback
//-----------------------------------------------------------------------------
bool CTextInputDaisyWheel::OnGamePadAnalog_ProcessRightStickForSide( const panorama::GamePadData_t &code )
{
	if ( !m_pStickSnd )
		return true;

	float flX = m_flStickSndMoveScale[0] * code.m_fValueX;
	float flY = m_flStickSndMoveScale[1] * code.m_fValueY;

	CUtlVector<CTransform3D *> vecTransforms;
	vecTransforms.AddToTail( new CTransformTranslate3D( flX, flY, 0.0f ) );
	m_pStickSnd->SetTransform3D( vecTransforms );

	// Compute stick distance from center
	float flDistSq = flX * flX + flY * flY;

	// Check if the stick entered selection threshold
	bool bNeedDebounce = m_pStickSnd->BHasClass( k_pchGroupRsNeedDebounce );
	if ( bNeedDebounce && ( flDistSq < m_flStickSndSelectDist[0] ) )
	{
		m_pStickSnd->RemoveClass( k_pchGroupRsNeedDebounce );
		bNeedDebounce = false;
	}

	char chSideToActivate = '\0';
	if ( !bNeedDebounce && ( flDistSq > m_flStickSndSelectDist[1] ) )
	{
		float flAngleRequired = m_flStickSndSelectAngleSticky;

		// Compute which group is now selected
		if ( ( flX > -flAngleRequired ) && ( flX < flAngleRequired ) )
		{
			chSideToActivate = ( flY > 0 ) ? k_Side_South : k_Side_North;
		}
		else if ( ( flY > -flAngleRequired ) && ( flY < flAngleRequired ) )
		{
			chSideToActivate = ( flX > 0 ) ? k_Side_East : k_Side_West;
		}
	}

	if ( chSideToActivate )
	{
		m_pStickSnd->AddClass( k_pchGroupRsNeedDebounce );
		bNeedDebounce = true;

		TypeCharacterFromSide( chSideToActivate );
	}

	float flOpacity = ( m_flStickSndSelectDist[1] - flDistSq ) / m_flStickSndSelectDist[1];
	if ( flOpacity < 0 )
		flOpacity = 0;
	if ( bNeedDebounce )
		flOpacity = 0;
	m_pStickSnd->SetOpacity( flOpacity );

	return true;
}


//-----------------------------------------------------------------------------
// Processes right stick analog location, selects the appropriate letter to print,
//	animates right stick helper UI object to provide motion feedback
//-----------------------------------------------------------------------------
bool CTextInputDaisyWheel::OnGamePadAnalog_Trigger( const panorama::GamePadData_t &code )
{
	int idxTrigger = !( code.m_GamePadCode == XK_BUTTON_LTRIGGER );
	bool bTriggerDown = (code.m_fValueX > 700 );
	if ( bTriggerDown == m_bTriggersDownState[idxTrigger] )
		return true;

	m_bTriggersDownState[idxTrigger] = bTriggerDown;
	panorama::DispatchEventAsync( 0.0f, TextInputDaisyWheelOnGamePadAnalogTriggersChanged(), this );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Converts string to daisy config enum
//-----------------------------------------------------------------------------
CTextInputDaisyWheel::EDaisyConfig_t CTextInputDaisyWheel::EDaisyConfigFromString( const char *pchValue )
{
	if ( V_stricmp( pchValue, "caps" ) == 0 )
		return k_EDaisyConfigCaps;
	if ( V_stricmp( pchValue, "letters" ) == 0 )
		return k_EDaisyConfigLetters;
	if ( V_stricmp( pchValue, "numbers" ) == 0 )
		return k_EDaisyConfigNumbers;
	if ( V_stricmp( pchValue, "special" ) == 0 )
		return k_EDaisyConfigSpecial;
	if ( V_stricmp( pchValue, "numbersonly" ) == 0 )
		return k_EDaisyConfigNumbersOnly;
	if ( V_stricmp( pchValue, "phonenumber" ) == 0 )
		return k_EDaisyConfigPhoneNumber;
	if ( V_stricmp( pchValue, "steamcode" ) == 0 )
		return k_EDaisyConfigSteamCodeChars;

	return k_EDaisyConfigNone;
}


//-----------------------------------------------------------------------------
// maps the trigger held state to a config
//-----------------------------------------------------------------------------
CTextInputDaisyWheel::EDaisyConfig_t CTextInputDaisyWheel::ConfigFromTriggerState( bool bLeftTrigger, bool bRightTrigger ) 
{
	if ( m_mode == k_ETextInputModeSteamCode )
	{
		return k_EDaisyConfigNone;
	}

	EDaisyConfig_t eConfig = k_EDaisyConfigLetters;
	if ( bLeftTrigger && bRightTrigger )
		eConfig = k_EDaisyConfigSpecial;
	else if ( bRightTrigger )
		eConfig = k_EDaisyConfigNumbers;
	else if ( bLeftTrigger )
		eConfig = k_EDaisyConfigCaps;

	if ( m_mapConfigEntries.Find( eConfig ) == m_mapConfigEntries.InvalidIndex() )
		return k_EDaisyConfigNone;

	return eConfig;
}

//-----------------------------------------------------------------------------
// Closes the panel and sends appropriate events
//-----------------------------------------------------------------------------
bool CTextInputDaisyWheel::HandleTextInputDaisyWheelOnGamePadAnalogTriggersChanged()
{
	if ( m_bRestrictConfig )
	{
		return true;
	}

	// map trigger pulled/not pulled state to which layout state to show
	EDaisyConfig_t eConfig = ConfigFromTriggerState( m_bTriggersDownState[ 0 ], m_bTriggersDownState[ 1 ] );
	if ( eConfig != k_EDaisyConfigNone )
	{
		m_eConfigCurrent = eConfig;
		PlayDaisyActionSound( k_EDaisySound_ConfigChanged );
		SetControlsFromConfiguration();
	}
	return true;
}


//-----------------------------------------------------------------------------
// helper function to count the number of words in a string
//-----------------------------------------------------------------------------
int CountWordsInString( const char *pchSentence )
{
	int nLen = V_strlen(pchSentence);
	int cWords = nLen > 1;
	if ( nLen > 1 )
	{
		for ( int i=1; i< nLen; i++ )
		{
			if( V_isalnum( pchSentence[i] ) && V_isspace( pchSentence[i-1] ) )
				cWords++;
		}
	}
	return cWords;
}


//-----------------------------------------------------------------------------
// Purpose: Closes the panel and sends appropriate events
//-----------------------------------------------------------------------------
void CTextInputDaisyWheel::CloseHandlerImpl( bool bCommitChanges )
{
	if ( !BHasClass( "Destructing") ) 
	{
		CancelOutstandingRepeats();
		AddClass( "Destructing" );
		panorama::DispatchEvent( TextInputHandlerStateChange(), this, false );
		const char *pchEntryText =  m_pTextInputControl->PchGetText();
		int nWords = CountWordsInString( pchEntryText );

		panorama::DispatchEvent( TextInputFinished(), m_pTextInputControl->GetAssociatedPanel(), bCommitChanges, pchEntryText );

		DeleteAsync( 0.2f );

		// tell the UI window how long it took to enter how many words
		float flEntryTime = UIEngine()->GetCurrentFrameTime() - m_flInputStartTime;
		GetParentWindow()->RecordDaisyWheelUsage( flEntryTime, nWords, m_bUsedKeyboard, m_bUsedGamepad );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Submit the text and clear state without closing
//-----------------------------------------------------------------------------
void CTextInputDaisyWheel::SubmitTextNoClose( void )
{
	if ( !BHasClass( "Destructing" ) )
	{
		m_sSuggestion.Clear();
		ClearSuggestionVisual();
		CancelOutstandingRepeats();

		panorama::DispatchEvent( panorama::TextEntrySubmit(), m_pTextInputControl->GetAssociatedPanel(), m_pTextInputControl->PchGetText() );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Return the control interface
//-----------------------------------------------------------------------------
ITextInputControl *CTextInputDaisyWheel::GetControlInterface()
{
	return m_pTextInputControl;
}


//-----------------------------------------------------------------------------
// Switches among recently used input languages
//-----------------------------------------------------------------------------
bool CTextInputDaisyWheel::SwitchLanguage()
{
	// SteamCode mode is language independent
	if ( m_mode == k_ETextInputModeSteamCode )
		return true;

#if !defined( SOURCE2_PANORAMA_FIXME )
	ELanguage languageOld = m_language;
	ELanguage languageNew = languageOld;

	ELanguage languageUI = UIEngine()->UILocalize()->CurrentLanguage();

	// find the next input locale the user has set up on their computer
	do
	{
		languageNew = ( ELanguage )( (int)( languageNew ) + 1 );
		if ( languageNew == k_Lang_MAX )
		{
			Assert( k_Lang_English == 0 );
			languageNew = k_Lang_English;
		}

		// language rotation includes:
		// - languages the user has enabled on their computer
		// - the UI language
		if ( languageNew == languageUI ||
			UIEngine()->BHaveInputLocale( languageNew ) )
		{
			if ( LoadInputConfigurationFile( languageNew ) )
			{
				UIEngine()->SetInputLocale( languageNew );
				break;
			}
		}
	}
	while ( languageNew != languageOld );

	// reflect the new language in the UI
	if ( m_pLang )
	{
		m_pLang->RemoveClass( k_pchShowingLanguage );
		ApplyStyles( false );
		// BUGBUG
		// The transition property is marked immediate, and we apply the styles
		// here, but the opacity is not slammed to the transition value. So if you
		// whale on the <| button, the language names will show sequentially, but
		// they will fade out instead of staying at 100% until you stop pressing
		// the button.
		DispatchEventAsync( 0.0f, DaisyWheelShowThisLanguage(), this, m_language );
	}
#endif

	return false;
}


//-----------------------------------------------------------------------------
// Switches directly to the specified language
//-----------------------------------------------------------------------------
bool CTextInputDaisyWheel::ShowThisLanguage( ELanguage language )
{
#if !defined( SOURCE2_PANORAMA_FIXME )
	m_pLang->SetText( GetPanoramaLocalizedLanguageShortName( language ) );
#endif
	m_pLang->AddClass( k_pchShowingLanguage );
	return true;
}


//-----------------------------------------------------------------------------
// In the currently selected group will pick the item from appropriate
// side of world and use its symbols for typing in text entry
//
// We round-trip the text through the label. If we want any decoupling of the
// text and the label, we'll need to find a better way to pump the text through
// into the document other than grabbing the label text directly at
// TypeCharacterFromSide time.
//
// So currently we do a bit of a hack to support newlines - if we ever see
// a label named <fancy glyph for RETURN character>, we know to insert a newline.

//-----------------------------------------------------------------------------
bool CTextInputDaisyWheel::TypeCharacterFromSide( char chSide )
{
	char const *szGroupNew = GetGroupNameSq( m_nSelectionGroup[0], m_nSelectionGroup[1] );
	if ( szGroupNew && *szGroupNew )
	{
		panorama::CPanel2D *pGroup = FindChildInLayoutFile( CFmtStr32( "Group_%s", szGroupNew ).String() );
		if ( pGroup )
		{
			CTextInputPickPanel *pItem = assert_cast< CTextInputPickPanel * >( pGroup->FindChild( CFmtStr32( "Item_%c", chSide ).String() ) );
			if ( !pItem )
			{
				return true;
			}

			if ( pItem->BIsNavigator() )
			{
				// tally ho! Navigate to a prior/subsequent layout (NYI)
			}
			else if ( pItem->GetChildCount() )
			{
				// here we grub out the first child of the pick panel, which we figure must be the label with the text
				pItem->SetTextInputPicked( m_flPickedItemTransitionTime );
				char const *szText = assert_cast< panorama::CLabel * >( pItem->GetChild( 0 ) )->PchGetText();
				if ( szText && *szText )
				{
					switch ( chSide )
					{
					case k_Side_West:
						PlayDaisyActionSound( k_EDaisySound_ButtonX );
						break;
					case k_Side_North:
						PlayDaisyActionSound( k_EDaisySound_ButtonY );
						break;
					case k_Side_East:
						PlayDaisyActionSound( k_EDaisySound_ButtonB );
						break;
					case k_Side_South:
						PlayDaisyActionSound( k_EDaisySound_ButtonA );
						break;
					}
					
					if ( V_strcmp( szText, k_szReturnGlyph ) )
					{
						uchar32 rgch32[ k_cSmallBuff ];
						V_UTF8ToUTF32( szText, rgch32, sizeof( rgch32 ) );
						Assert( rgch32[0] != 0 );
						for ( int ich32 = 0; rgch32[ ich32 ] != 0; ich32++ )
							TypeUniChar( rgch32[ ich32 ] );
					}
					else
					{
						TypeUniChar( '\r' );
						TypeUniChar( '\n' );
					}

					return true;
				}
			}
		}
	}
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if character ends a sentence
//-----------------------------------------------------------------------------
uchar32 BEndsSentence( uchar32 ch32 )
{
	return (ch32 == '.' || ch32 == '?' || ch32 == '!');
}


//-----------------------------------------------------------------------------
// Purpose: Checks if cusor is at the beginning of a sentence
//-----------------------------------------------------------------------------
bool CTextInputDaisyWheel::BCursorAtStartOfSentence()
{
	const uchar32 *pch32 = m_pTextInputControl->Pch32GetText();
	if ( !pch32 )
		return false;

	// we are looking for sentence termination and then a space or at beginning of input
	int32 iCursor = m_pTextInputControl->GetCursorOffset();
	if ( iCursor == 0 )
		return true;

	// going backward, first character must be a space
	iCursor--;
	if ( !V_iswspace32( pch32[iCursor] ) )
		return false;

	// find something that isn't whitespace	
	for ( iCursor--; iCursor > 0; iCursor-- )
	{
		uchar32 ch32Next = pch32[iCursor];
		if ( V_iswspace32( ch32Next ) )
			continue;

		if ( !BEndsSentence( ch32Next ) )
			return false;

		break;
	}

	// at beginning of text input or only white space before cursor
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Returns character before cursor or NUL
//-----------------------------------------------------------------------------
bool CTextInputDaisyWheel::BConvertNextSpaceToPeriod()
{
	// if user has only entered spaces since pressing backspace, they were probably trying to undo
	// automatically converting space to period
	if ( !m_bDoubleSpaceToDotSpace || m_bOnlySpacesEnteredSinceBackspace )
		return false;

	// previous character must be a space
	const uchar32 *pch32 = m_pTextInputControl->Pch32GetText();
	uint32 iCursor = m_pTextInputControl->GetCursorOffset();
	if ( !pch32 || iCursor < 2 || pch32[ iCursor - 1 ] != ' ' )
		return false;

	// if character before space is any of the following, don't convert
	// bugbug - extend with brackets, other language support, etc. Really just want letters?
	uchar32 ch32BeforeSpace = pch32[ iCursor - 2 ];
	if ( V_iswspace32( ch32BeforeSpace ) || BEndsSentence( ch32BeforeSpace ) || ch32BeforeSpace == ',' || ch32BeforeSpace == ';' )
		return false;

	return true;
}


//-----------------------------------------------------------------------------
// Types a given unicode character into text entry
//-----------------------------------------------------------------------------
bool CTextInputDaisyWheel::TypeUniChar( uchar32 ch32 )
{
	if ( m_flInputStartTime == 0.0f )
		m_flInputStartTime = UIEngine()->GetCurrentFrameTime(); // used has started typing, lets track

	// handle converting two spaces into ". "
	if ( ch32 != ' ' )
		m_bOnlySpacesEnteredSinceBackspace = false;

	if ( ch32 == ' ' && BConvertNextSpaceToPeriod() )
	{
		TypeKeyDown( KEY_BACKSPACE );
		m_pTextInputControl->InsertCharacterAtCursor( '.' );
		m_pTextInputControl->InsertCharacterAtCursor( ' ' );
	}
	else
	{
		// put in the text
		m_pTextInputControl->InsertCharacterAtCursor( ch32 );
	}	

	// automatically switch in or out of caps if necessary
	bool bAtStartOfSentence = BCursorAtStartOfSentence();
	if ( m_bAutoCaps )
	{
		if ( bAtStartOfSentence )
			AdvanceControlsConfiguration( k_EDaisyConfigCaps );
		else if ( m_eConfigCurrent == k_EDaisyConfigCaps && !m_bTriggersDownState[0] ) // only advance if LT isn't held
			AdvanceControlsConfiguration( k_EDaisyConfigLetters );
	}

	// now offer autosuggest, if we didn't just terminate a word
	if ( m_psuggest != NULL &&
		!( bAtStartOfSentence || V_iswspace32( ch32 ) ) )
	{
		// FUTURE if we keep a better state machine, we could do this refresh only when we move the caret.
		// we could append to a saved string at typing time, empty the string when we type a word delimiter,
		// and know the string is going stale when we move the caret.

		// look at state of existing text

		if ( m_pTextInputControl->BSupportsImmediateTextReturn() )
		{
			const uchar32 *pch32 = m_pTextInputControl->Pch32GetText();
			int32 ich = m_pTextInputControl->GetCursorOffset();
			Assert( pch32[ 0 ] != 0 );
			if ( pch32[ 0 ] != 0 )
				SuggestWord( pch32, ich );
		}
		else
		{
			// can suggest immediately, ask for it to tell us the text in the string
			m_pTextInputControl->RequestControlString();
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
//	Purpose: given a text string and offset, insert a suggestion into the control
//-----------------------------------------------------------------------------
void CTextInputDaisyWheel::SuggestWord( const uchar32 *pch32, int ich )
{
	if ( !BDoSuggestions() )
		return;

	if ( pch32 == NULL || pch32[0] == 0 )
		return;

	// pull context - at most N characters -
	// use that to feed the suggestion engine
	const char cchContext = 32;

	// find a previous word break
	const uchar32 *pch32Find = &pch32[ ich - 1 ];
	const uchar32 *pch32Lim = &pch32[ ich ];		// points to a NUL iff the caret is at the end of the text
	while ( pch32Find > pch32 &&					// don't go back off the beginning of the string
		pch32Lim - pch32Find < cchContext )		// don't go back so far we have too much text
	{
		pch32Find--;
		if ( V_iswspace32( *pch32Find ) )
		{
			// don't go into a word break - TODO expand this check considerably, look for alnums only?
			pch32Find++;
			break;
		}
	}

	// put the context in a local, because we need a truncated version of it
	uchar32 rgch32Context[ cchContext ];

	// the user might type a really long word, so truncate if they do
	int cch32 = MIN(pch32Lim - pch32Find + 1, cchContext );

	memcpy( rgch32Context, pch32Find, cch32 * sizeof( uchar32 ) );
	Assert( rgch32Context[ cch32 - 1 ] == 0 );

	// un-validated user input can get here, and it can include naked surrogates without a surrogate pair.
	// right now, a naked single surrogate can get here via the daisywheel entering emoji as well;
	// we can clean up that code path but we can't prevent the user from pasting one in.
#ifdef SOURCE2_PANORAMA
	CStrAutoEncodeSrc2 s( rgch32Context );
#else
	CStrAutoEncode s( rgch32Context, STRINGCONVERT_REPLACE );
#endif

	if ( BDoSuggestions() )
	{
		// perform suggestion
		CUtlString sRest;
		if ( m_psuggest && m_psuggest->SuggestWord( s.ToUTF8(), sRest ) )
		{
			m_sSuggestion = sRest;
			ShowSuggestion( s.ToUTF8(), m_sSuggestion );
		}
		else
		{
			m_sSuggestion.Clear();
			ClearSuggestionVisual();
		}
	}
}


//-----------------------------------------------------------------------------
//	Simulates a key down event
//-----------------------------------------------------------------------------
bool CTextInputDaisyWheel::TypeKeyDown( panorama::KeyCode eCode )
{
	// any non space key should clear the following member. If backspace, set
	if ( eCode == KEY_BACKSPACE )
	{
		// check what we are going to remove. If space or backspace, need to track for double space conversion
		int iCursor = m_pTextInputControl->GetCursorOffset();
		const uchar32 *pch32 = m_pTextInputControl->Pch32GetText();
		if ( iCursor > 0 && pch32 )
		{
			uchar32 ch32Remove = pch32[iCursor - 1];
			if ( ch32Remove != ' ' )
				m_bOnlySpacesEnteredSinceBackspace = (ch32Remove == '.');
		}
	}
	else if ( eCode != KEY_SPACE )
	{
		m_bOnlySpacesEnteredSinceBackspace = false;
	}

	panorama::KeyData_t code = { k_ePanelEventSourceProgram, eCode };
	m_pTextInputControl->OnKeyDown( code );
	m_pTextInputControl->OnKeyUp( code ); // also send the up event!!

	// Update suggestions on the other side of the backspace event
	if ( eCode == KEY_BACKSPACE )
	{
		const uchar32 *pch32 = m_pTextInputControl->Pch32GetText();
		int32 ich = m_pTextInputControl->GetCursorOffset();
		if ( pch32[0] != 0 )
			SuggestWord( pch32, ich );
	}

	// see if the inserted key has changed caps
	if ( m_bAutoCaps )
	{
		if ( BCursorAtStartOfSentence() )
			AdvanceControlsConfiguration( k_EDaisyConfigCaps );
		else if ( m_eConfigCurrent == k_EDaisyConfigCaps )
			AdvanceControlsConfiguration( k_EDaisyConfigLetters );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: return the list of languages we have configs for
//-----------------------------------------------------------------------------
void CTextInputDaisyWheel::GetSupportedLanguages( CUtlVector<ELanguage> &vecLangs )
{
#if !defined( SOURCE2_PANORAMA_FIXME )
	ELanguage language = k_Lang_English;
	do
	{
		if ( UIEngine()->BHaveInputLocale( language ) )
		{
			CUtlString sConfig;
			sConfig.Format( "layout_%s_default.txt", GetLanguageShortName( language ) );
			CPathString strPath( sConfig, UIEngine()->GetLocalPathForNamedPath( "{keyboards}" ) );

			if ( UIEngine()->UIFileSystem()->FileExists( strPath.GetUTF8Path() ) )
			{
				vecLangs.AddToTail( language );
			}
		}
		language = (ELanguage)((int)language + 1);
	} while ( language != k_Lang_MAX );
#endif
}


//-----------------------------------------------------------------------------
// Loads configuration file by language ID
//-----------------------------------------------------------------------------
bool CTextInputDaisyWheel::LoadInputConfigurationFile( ELanguage language )
{
#if !defined( SOURCE2_PANORAMA_FIXME )
	CUtlString sConfig;
	sConfig.Format( "layout_%s_default.txt", GetLanguageShortName( language ) );

	bool bLoaded = LoadInputConfigurationFile( sConfig.String(), UIEngine()->GetLocalPathForNamedPath( "{keyboards}" ) );
	if ( bLoaded )
		m_language = language;

	// try to load suggestion info, if appropriate (i.e. not when entering a password)
	SAFE_DELETE( m_psuggest );
	
	if ( m_bDisplaySuggestions )
	{
		ITextInputSuggest *psuggest = CreateInputSuggest( language );
		if ( psuggest != NULL )
		{
			m_psuggest = psuggest;
		}
	}

	return bLoaded;
#else
	return false;
#endif
}


//-----------------------------------------------------------------------------
// Loads configuration file, applies the first config from the configuration
//	file and deallocates previous config, returns whether load succeeded
//	and new configuration was successfully set and activated
//-----------------------------------------------------------------------------

#ifndef PANORAMA_USE_S1WRAPPER

bool CTextInputDaisyWheel::LoadInputConfigurationFile( char const *szConfigFile, const char *szConfigRootDir )
{
	CPathString strPath( szConfigFile, szConfigRootDir );
	CUtlBuffer buffer;
	if ( !UIEngine()->UIFileSystem()->LoadFileIntoBuffer( strPath.GetUTF8Path(), buffer, true ) )
		return false;

	MapConfigEntries_t mapConfigs;
	bool bResult = LoadConfigurationBuffer( (char const *)buffer.Base(), &mapConfigs );
	if ( bResult )
	{
		if ( !m_bRestrictConfig )
		{
			m_eConfigCurrent = BCursorAtStartOfSentence() ? k_EDaisyConfigCaps : k_EDaisyConfigLetters;
		}
		m_mapConfigEntries.Swap( mapConfigs );
		SetControlsFromConfiguration();
	}

	return true;
}

#endif


//-----------------------------------------------------------------------------
// Processes the buffer loaded from configuration buffer and allocates appropriate
//	configuration structures to hold names of configurations and layout of items
//	within each subconfiguration
//-----------------------------------------------------------------------------
bool CTextInputDaisyWheel::LoadConfigurationBuffer( char const *pszIncoming, MapConfigEntries_t *pmapConfigs )
{
	// take it to uchar32 in one fell swoop, to avoid having to do any UTF-8 decoding below
#ifdef SOURCE2_PANORAMA
	CStrAutoEncodeSrc2 str( pszIncoming );
	const wchar_t k_wchUnicodeBOM = 0xFEFF;      // U+FEFF ZERO-WIDTH NO BREAK SPACE
#else
	CStrAutoEncode str( pszIncoming, STRINGCONVERT_FAIL );
#endif

	// we happen to know we will own the whole buffer, so we don't need it to be const
	uchar32 *pch32Base = const_cast< uchar32* >( str.ToUTF32() );

	if ( pch32Base == NULL )
	{
		// failed conversion
		AssertMsg( false, "Failed to convert daisy wheel config from UTF-8" );
		return false;
	}

	// remember start of file, makes for better error messages
	uchar32 const *pwszBookmarkBase = pch32Base;

	// See if we have a BOM at the beginning of the buffer
	if ( *pch32Base == k_wchUnicodeBOM )
	{
		// We do; skip it
		pch32Base++;
	}

	int numLines = 0;
	while( pch32Base && *pch32Base )
	{
		// Look for the newline delimiting this line
		uchar32 *pch32LineEnd = V_u32strchr( pch32Base, '\n' );
		uchar32 *pch32LineReturn =  V_u32strchr( pch32Base, '\r' );
		if ( pch32LineReturn && pch32LineReturn < pch32LineEnd )
			pch32LineEnd = pch32LineReturn;

		// Look for tab in line
		uchar32 *pch32Tab = V_u32strchr( pch32Base, '\t' );
		if ( !pch32Tab || pch32Tab > pch32LineEnd )
		{
			// this line does not contain a tab
			AssertMsg2( false, "Line contains no tab(%d/%d)!", ( numLines + 1 ), (int)( pch32Base - pwszBookmarkBase ) );
			return false;
		}

		// TAB character delimits the sequence name
		// NUL terminate it
		*pch32Tab = 0;
		// remember name
		uchar32 *pch32Name = pch32Base;
		// and move on
		pch32Base = pch32Tab + 1;

		// Eat whitespace
		while ( pch32Base && *pch32Base && ( *pch32Base == '\t' || *pch32Base == ' '  ) )
			++ pch32Base;

		// Now we have arrived at the row of characters

		// Text block for this row (UTF-8, NUL terminators in place)
		CUtlVector< char > vecText;
		int rgich[ k_cItemsPerLayoutMax ] = { 0 };

		// items seen so far
		int cItems = 0;

		while( *pch32Base )
		{
			if ( cItems >= k_cItemsPerLayoutMax )
			{
				// this line specifies a layout that has too many items in it - silently skip the extras
				pch32Base = pch32LineEnd;
				break;
			}

			if ( pch32Base == pch32LineEnd )
			{
				break;
			}

			CUtlVector< uchar32 > vecItem;

			if ( *pch32Base == '\\' )
			{
				// skip backslash
				pch32Base++;

				// look at following character, must not be EOL or EOF
				if ( *pch32Base == 0 || pch32Base == pch32LineEnd )
				{
					AssertMsg2( false, "Invalid escapee (%d/%d)", numLines + 1, (int)( pch32Base - pwszBookmarkBase ) );
					return false;
				}

				// here is the printed rep of the escaped character
				uchar32 ch32 = *pch32Base;

				// translate escapes, subset of C escapes
				switch ( ch32 )
				{
				case 'n':
				case 'r':
					vecItem.AddToTail( '\r' );
					vecItem.AddToTail( '\n' );
					break;
				case 't':
					vecItem.AddToTail( '\t' );
					break;
				case '0':
					vecItem.AddToTail( '\0' );
					break;
				default:
					vecItem.AddToTail( ch32 );
					break;
				}
			}
			else if ( *pch32Base == '{' )
			{
				// put a string in there
				uchar32 *pch32EndBrace = V_u32strchr( pch32Base, '}' );
				if ( pch32EndBrace == NULL || pch32EndBrace > pch32LineEnd )
				{
					AssertMsg2( false, "Unterminated opening brace (%d/%d)", numLines + 1, (int)( pch32Base - pwszBookmarkBase ) );
					return false;
				}

				pch32Base++; // skip opening brace
				vecItem.AddMultipleToTail( pch32EndBrace - pch32Base, pch32Base );
				pch32Base = pch32EndBrace;
			}
			else
			{
				vecItem.AddToTail( pch32Base[ 0 ] );
			}

			vecItem.AddToTail( 0 );

			// write the item into the config

			// first, the offset
			rgich[ cItems ] = vecText.Count();

			// now the text, transcoded back to UTF-8
			CStrAutoEncodeSrc2 s( vecItem.Base() );
			const char *szUTF8 = s.ToString();

			// append to text block
			vecText.AddMultipleToTail( V_strlen( szUTF8 ) + 1, szUTF8 );

			++ cItems;
			++ pch32Base;
		}

		// At this point we either point at 0-terminator or a \r\n or a \n
		if ( *pch32Base )
		{
			if ( *pch32Base == '\r' )
				pch32Base++;

			if ( *pch32Base == '\n' )
				pch32Base++;
			else
			{
				AssertMsg2( false, "Invalid newline termination sequence (%d/%d)", numLines + 1, (int)( pch32Base - pwszBookmarkBase ) );
				return false;
			}

			++ numLines;
		}

		// Allocate the final layout structure

		// name.. should be #TextInput_<type>
		char rgchUTF8Name[ k_cSmallBuff ];
		V_UTF32ToUTF8( pch32Name, rgchUTF8Name, V_ARRAYSIZE( rgchUTF8Name ) );
		const char *pchType = V_strstr( rgchUTF8Name, "_" );
		if ( !pchType )
			return false;

		// skip _
		pchType++;
		EDaisyConfig_t eType = EDaisyConfigFromString( pchType );
		if ( eType == k_EDaisyConfigNone )
			return false;

		CDaisyConfig *pConfig = new CDaisyConfig( rgchUTF8Name );

		// size
		pConfig->m_cItems = cItems;
		
		// indices
		COMPILE_TIME_ASSERT( sizeof( rgich ) == sizeof( pConfig->m_rgich ) );
		V_memcpy( pConfig->m_rgich, rgich, sizeof( rgich ) );

		// text block
		pConfig->m_vecText.Swap( vecText );

		pmapConfigs->InsertOrReplace( eType, pConfig );
	}
	return true;
}


//-----------------------------------------------------------------------------
//	Advances input configuration by so many configs
//-----------------------------------------------------------------------------
void CTextInputDaisyWheel::AdvanceControlsConfiguration( EDaisyConfig_t eConfig )
{
	if ( m_bRestrictConfig )
		return;

	if ( m_mapConfigEntries.HasElement( eConfig ) )
	{
		m_eConfigCurrent = eConfig;
		SetControlsFromConfiguration();
	}
}


//-----------------------------------------------------------------------------
// Sets all controls in the panel groups and items in groups to the values
//	from the configuration that got assigned as active
//-----------------------------------------------------------------------------
void CTextInputDaisyWheel::SetControlsFromConfiguration()
{
	int iMap = m_mapConfigEntries.Find( m_eConfigCurrent );
	if ( iMap == m_mapConfigEntries.InvalidIndex() )
		return;

	// Blank out all controls first
	for ( int k = 0; k < k_cPetals; ++ k )
	{
		panorama::CPanel2D *pGroup = FindChildInLayoutFile( CFmtStr32( "Group_%s", GetGroup( k ) ).String() );
		if ( pGroup )
		{
			for ( int j = 0; j < 4; ++ j )
			{
				panorama::CPanel2D *pItem = pGroup->FindChild( CFmtStr32( "Item_%s", GetSide( j ) ).String() );
				if ( pItem && pItem->GetChildCount() )
				{
					assert_cast< panorama::CLabel * >( pItem->GetChild( 0 ) )->SetText( "" );
					pItem->SetVisible( false );
				}
			}
		}
	}

	// Set from config
	CDaisyConfig *pCfg = m_mapConfigEntries.Element( iMap ).GetPtr();
	assert_cast< panorama::CLabel * >( FindChildInLayoutFile( "ConfigName" ) )->SetText( pCfg->GetName() );
	for ( int k = 0; k < pCfg->GetNumItems(); ++ k )
	{
		char const *szGroup = "";
		char const *szItem = "";
		GetItemLocation( pCfg, k, szGroup, szItem );
		panorama::CPanel2D *pGroup = FindChildInLayoutFile( CFmtStr32( "Group_%s", szGroup ).String() );
		if ( pGroup )
		{
			CTextInputPickPanel *pItem = assert_cast< CTextInputPickPanel * >( pGroup->FindChild( CFmtStr32( "Item_%s", szItem ).String() ) );
			if ( pItem && pItem->GetChildCount() )
			{
				const char *pszItem = pCfg->GetItem( k );
				if ( *pszItem != 0 )
				{
					CStrAutoEncode s( pszItem );

					panorama::CLabel *plabel = assert_cast< panorama::CLabel * >( pItem->GetChild( 0 ) );

					plabel->SetText( s.ToString(), panorama::CLabel::k_ETextTypeUnlocalized );
					if ( m_mode == k_ETextInputModeEmail || m_mode == k_ETextInputModeURL )
					{
						if ( !V_strcmp( s.ToString(), k_szBecomesDotCom ) )
						{
							plabel->SetText( ".com" );
						}
					}

					pItem->SetVisible( true );
					// don't set debounced input as it takes away visual feedback during quick typing
				}
			}
		}
	}

	UpdateTriggerLegends();
}


//-----------------------------------------------------------------------------
//	For a given config item determine which group and group side the item should be at
//-----------------------------------------------------------------------------
void CTextInputDaisyWheel::GetItemLocation( CDaisyConfig *pCfg, int iItem, char const *&szGroup, char const *&szItem )
{
	int divisor = 4;
	int idxGroup = iItem / divisor;
	int idxSide = (iItem % divisor) * ( ( divisor == 2 ) ? 2 : 1 );
	
	szGroup = GetGroup( idxGroup );
	szItem = GetSide( idxSide );
}


//-----------------------------------------------------------------------------
//	Gets a sequential index of the group square indexed by -1|0|1 pair of x,y coordinates
//-----------------------------------------------------------------------------
int CTextInputDaisyWheel::GetGroupIdxSq( int x, int y )
{
	switch ( x )
	{
	case -1:
		{
			switch ( y )
			{
			case -1:
				return 7;
			case 0:
				return 6;
			case 1:
				return 5;
			}
			return -1;
		}
	case 0:
		{
			switch ( y )
			{
			case -1:
				return 0;
			case 0:
				return -1;
			case 1:
				return 4;
			}
			return -1;
		}
	case 1:
		{
			switch ( y )
			{
			case -1:
				return 1;
			case 0:
				return 2;
			case 1:
				return 3;
			}
			return -1;
		}
	}
	return -1;
}


//-----------------------------------------------------------------------------
//	Get name of the group square indexed by -1|0|1 pair of x,y coordinates;
//	returns side or wolrd like: "E" | "NE" | "N" | "NW" | etc.
//-----------------------------------------------------------------------------
char const* CTextInputDaisyWheel::GetGroupNameSq( int x, int y )
{
	switch ( x )
	{
	case -1:
		{
			switch ( y )
			{
			case -1:
				return "NW";
			case 0:
				return "W";
			case 1:
				return "SW";
			}
			return "";
		}
	case 0:
		{
			switch ( y )
			{
			case -1:
				return "N";
			case 0:
				return "";
			case 1:
				return "S";
			}
			return "";
		}
	case 1:
		{
			switch ( y )
			{
			case -1:
				return "NE";
			case 0:
				return "E";
			case 1:
				return "SE";
			}
			return "";
		}
	}
	return "";
}


//-----------------------------------------------------------------------------
//	Gets the side of world name of group by its sequential index
//-----------------------------------------------------------------------------
char const * CTextInputDaisyWheel::GetGroup( int idxGroup )
{
	char const * s_arrGroups[] = { "N", "NE", "E", "SE", "S", "SW", "W", "NW" };
	return s_arrGroups[ MIN( (uint)idxGroup, V_ARRAYSIZE( s_arrGroups ) - 1 ) ];
}


//-----------------------------------------------------------------------------
//	Gets the side of world name of item by its sequential index
//-----------------------------------------------------------------------------
char const * CTextInputDaisyWheel::GetSide( int idxSide )
{
	char const * s_arrSides[] = { "W", "N", "E", "S" };
	return s_arrSides[ MIN( (uint)idxSide, V_ARRAYSIZE( s_arrSides ) - 1 )];
}


//-----------------------------------------------------------------------------
//	a key has been typed; do performance timing tracking
//-----------------------------------------------------------------------------
bool CTextInputDaisyWheel::OnKeyTyped( const KeyData_t &unichar )
{
	m_bUsedKeyboard = true;
	if ( m_flInputStartTime == 0.0f )
		m_flInputStartTime = UIEngine()->GetCurrentFrameTime(); // used has started typing, lets track
	return m_pTextInputControl->OnKeyTyped( unichar ); 
}


//-----------------------------------------------------------------------------
//	a key has gone down
//-----------------------------------------------------------------------------
bool CTextInputDaisyWheel::OnKeyDown( const KeyData_t &code )
{ 
	if ( code.m_KeyCode == KEY_ENTER )
	{
		CloseHandler( true );
		return true;
	}
	else
	{
		return m_pTextInputControl->OnKeyDown( code ); 
	}
}


//-----------------------------------------------------------------------------
//	a key has gone up
//-----------------------------------------------------------------------------
bool CTextInputDaisyWheel::OnKeyUp( const KeyData_t &code )
{ 
	return m_pTextInputControl->OnKeyUp( code ); 
}


//-----------------------------------------------------------------------------
//	a property transition has ended; perhaps fix up the language UI
//-----------------------------------------------------------------------------
bool CTextInputDaisyWheel::HandlePropertyTransitionEnd( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel, CStyleSymbol sym )
{
	if ( m_pLang == NULL )
		return false;					// guard against broken layout files

	if ( ToPanel2D(pPanel.Get()) == m_pLang && sym == CStylePropertyOpacity::symbol )
	{
		// we've flashed a language name; now fade back to show "language"
		if ( m_pLang->BHasClass( k_pchShowingLanguage ) )
		{
			m_pLang->SetText( "#UI_Languages" );
			m_pLang->RemoveClass( k_pchShowingLanguage );
		}
	}
	return true;
}

//-----------------------------------------------------------------------------
//	Show a given suggestion
//-----------------------------------------------------------------------------
void CTextInputDaisyWheel::ShowSuggestion( const char *szPrefix, const char *szSuffix )
{
	if ( m_plabelSuggestionPrefix )
	{
		m_plabelSuggestionPrefix->SetText( szPrefix );
	}
	if ( m_plabelSuggestionSuffix )
	{
		m_plabelSuggestionSuffix->SetText( szSuffix );
	}

}


//-----------------------------------------------------------------------------
//	the user wants to tab to the next focusable form element
//-----------------------------------------------------------------------------
bool CTextInputDaisyWheel::NextFocus()
{
	CloseHandler( true );
	if ( m_pTextInputControl->GetAssociatedPanel() != NULL )
	{
		DispatchEventAsync( panorama::TabForward(), m_pTextInputControl->GetAssociatedPanel(), 1 );
	}
	return true;
}


//-----------------------------------------------------------------------------
//	update the trigger legends from the current control state
//-----------------------------------------------------------------------------
void CTextInputDaisyWheel::UpdateTriggerLegends() 
{
	bool bLTVisible = false;
	bool bRTVisible = false;

	if ( m_mapConfigEntries.Count() > 1 )
	{
		// compute the layout we will show if the user pulls the left trigger,
		// using the current RT state but falling back to not pulled
		EDaisyConfig_t eConfig = ConfigFromTriggerState( true, m_bTriggersDownState[ 1 ] );
		if ( eConfig == k_EDaisyConfigNone )
			eConfig = ConfigFromTriggerState( true, false );
		if ( eConfig != k_EDaisyConfigNone )
		{
			int iMap = m_mapConfigEntries.Find( eConfig );
			if ( iMap != m_mapConfigEntries.InvalidIndex() )
			{
				CDaisyConfig *pLeftTriggerPulled = m_mapConfigEntries.Element( iMap ).GetPtr();
				assert_cast<panorama::CLabel *>(FindChildInLayoutFile( "LT_txt" ))->SetText( pLeftTriggerPulled->GetName() );
				bLTVisible = true;
			}
		}

		// same drill, right trigger
		eConfig = ConfigFromTriggerState( m_bTriggersDownState[ 0 ], true );
		if ( eConfig == k_EDaisyConfigNone )
			eConfig = ConfigFromTriggerState( false, true );
		if ( eConfig != k_EDaisyConfigNone )
		{
			int iMap = m_mapConfigEntries.Find( eConfig );
			if ( iMap != m_mapConfigEntries.InvalidIndex() )
			{
				CDaisyConfig *pRightTriggerPulled = m_mapConfigEntries.Element( iMap ).GetPtr();
				assert_cast<panorama::CLabel *>(FindChildInLayoutFile( "RT_txt" ))->SetText( pRightTriggerPulled->GetName() );
				bRTVisible = true;
			}
			
		}
	}

	FindChildInLayoutFile( "LeftLegend" )->SetVisible( bLTVisible && !m_bDisableLeftTrigger );
	FindChildInLayoutFile( "RightLegend" )->SetVisible( bRTVisible && !m_bDisableRightTrigger );
	FindChildInLayoutFile( "LangLegend" )->SetVisible( !m_bDisableLanguageSelect );
	FindChildInLayoutFile( "RBLegend" )->SetVisible( !m_bDisableRightBumper );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTextInputDaisyWheel::BDoSuggestions( void )
{
	if ( BHasClass( "NoSuggestions" ) )
		return false;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CTextInputDaisyWheel::SetSuggestionPanels( const CUtlVector<CSuggestionPanel *>& vecPanels )
{
	// this style of text input does not accept suggestion panels
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Validation
//-----------------------------------------------------------------------------
#ifdef DBGFLAG_VALIDATE
void CTextInputDaisyWheel::ValidateClientPanel( CValidator &validator, const tchar *pchName )
{
	BaseClass::ValidateClientPanel( validator, pchName );

	VALIDATE_SCOPE();
	
	ValidateObj( m_repeatFunction );
	ValidateObj( m_sSuggestion );
	ValidatePtr( m_psuggest );
	ValidateObj( m_mapConfigEntries );
	FOR_EACH_MAP_FAST( m_mapConfigEntries, i )
	{
		ValidatePtr( m_mapConfigEntries.Element( i ).GetPtr() );
	}

	ValidateObj( m_vecEmoji );
	FOR_EACH_VEC( m_vecEmoji, i )
	{
		ValidateObj( m_vecEmoji[i].sType );
		ValidateObj( m_vecEmoji[i].sImageURL );
	}
	
	ValidatePtr( m_pYbuttonAction );
}
#endif


//
// worker routine to pickle a single dictionary
//
static void PickleSingleDictionary( ELanguage language )
{
#if !defined( SOURCE2_PANORAMA_FIXME )
	const char *pchLanguage = GetLanguageShortName( language );

	// regular word list (input)
	CPathString sWordList( CFmtStr1024( "%s/../../../../../external/dictionaries/tenfoot/%s_words.txt",
		UIEngine()->GetLocalPathForNamedPath( "{wordlists}" ),
		GetLanguageShortName( language ) ) );

	// pickled word list (output)
	CPathString sPickledWordList( CFmtStr1024( "%s/%s_compiled_words.dic",
		UIEngine()->GetLocalPathForNamedPath( "{wordlists}" ),
		GetLanguageShortName( language ) ) );

	CUtlRadixTrie trie;		// case insensitive building dictionary
	if ( !trie.BLoad( sWordList.GetUTF8Path() ) )
	{
		Msg( "Failed to load non-pickled dictionary for language %s\n", pchLanguage );
		return;
	}

	trie.DumpStatistics();

	// pickle it. we instantiate the dictionary case sensitive, then we perform
	// lookups case-insensitive; this allows us to correct case where we find it
	// yet still perform cross-case matches.
	CUtlBuffer buf;
	if ( !CPickledRadixTrie::Pickle( trie, buf, true /* case sensitive */, trie.GetMaxFrequency() ) )
	{
		Msg( "Failed to pickle language %s\n", pchLanguage );
		return;
	}

	// sanity check that it will load OK
	CPickledRadixTrie pickledTrie;
	bool bLoaded = pickledTrie.BLoad( buf );
	AssertMsg( bLoaded, CFmtStr1024( "Couldn't load pickled trie for %s", pchLanguage ) );

	if ( !SaveBufferToFile( buf, sPickledWordList.GetUTF8Path() ) )
	{
		Msg( "Failed to save pickled file %s - did you check it out?\n", sPickledWordList.GetUTF8Path() );
		return;
	}
	Msg( "%s pickled OK\n", pchLanguage );

#endif
}


//
// command to pickle all the input dictionaries
//

#ifndef PANORAMA_USE_S1WRAPPER

CON_COMMAND_F( tenfoot_pickle_dictionaries, "compiles daisy wheel input dictionaries to more performant form", FCVAR_LINKED_CONCOMMAND )
{
	Msg( "Loading and pickling daisy wheel dictionaries\n" );
	for ( int ilanguage = 0; ilanguage < k_Lang_MAX; ilanguage++ )
	{
		ELanguage language = (ELanguage) ilanguage;
		PickleSingleDictionary( language );
	}
	Msg( "Done.\n" );
}


//
// command to pickle a single language's dictionary
//
CON_COMMAND_F( tenfoot_pickle_single_dictionary, "[language] compiles one daisy wheel input dictionary", FCVAR_LINKED_CONCOMMAND )
{
#if !defined( SOURCE2_PANORAMA_FIXME )
	if ( params.CArgs() != 1 )
	{
		Msg( "%s", cmd.GetHelpText() );
		return;
	}

	ELanguage language = PchLanguageToELanguage( params.PchArg( 1 ), k_Lang_None );

	if ( language == k_Lang_None )
	{
		Msg( "Invalid language '%s'\n", params.PchArg( 1 ) );
		return;
	}

	PickleSingleDictionary( language );

	Msg( "Done.\n" );
#endif
}

#endif

//
// match-finding worker for command line predictive text commands
//
static void doMatch( ITextInputSuggest *psuggest, const char *szPrefix, bool bDumpList, bool bSpew )
{
	// time the suggestions
#ifdef SOURCE2_PANORAMA
	CFastTimer timer;
#else
	CReliableTimer timer;
#endif
	timer.Start();

	// perform suggestion
#ifdef SOURCE2_PANORAMA
	CandidateList_t vecCandidates;
#else
	CandidateList_t vecCandidates( UtlRadixTrieCandidateLessFunc );
#endif
	psuggest->SuggestWords( szPrefix, vecCandidates, 10 );

	timer.End();

	// output
	const char *szTopLine = "(no suggestion)";
	if ( vecCandidates.Count() != 0 )
	{
		szTopLine = vecCandidates[ 0 ].rgch;
	}
	if ( bSpew )
		Msg( "Completed to %s, %.3f ms elapsed\n",
			szTopLine,
#ifdef SOURCE2_PANORAMA
			timer.GetDuration().GetMicrosecondsF() / 1000 );
#else
			(float)timer.GetMicroseconds() / 1000 );
#endif
	if ( !bDumpList )
		return;

	if ( vecCandidates.Count() != 0 )
	{
		Msg( "Top candidates:\n" );
		for ( int i = 0; i < vecCandidates.Count(); i++ )
		{
			Msg( "\t%s", vecCandidates[ i ].rgch );
#ifdef _DEBUG
			// in debug, show the frequencies (not necessary to show this
			// externally, and the vendor agreement states we should keep
			// the frequency information private)
			Msg( ": %.3f", vecCandidates[ i ].probability );
#endif
			Msg( "\n" );
		}
	}
}


//
// command-line matching of text prefix, for testing
//

#ifndef PANORAMA_USE_S1WRAPPER

CON_COMMAND_F( tenfoot_match, "[prefix]: matches a string prefix", FCVAR_LINKED_CONCOMMAND )
{
#if !defined( SOURCE2_PANORAMA_FIXME )
	static ITextInputSuggest *psuggest = NULL;
	static bool bLoaded = false;

	if ( params.CArgs() != 1 )
	{
		Msg( "%s", cmd.GetHelpText() );
		return;
	}

	const char *szPrefix = params.PchArg( 1 );

	if ( !bLoaded )
	{
		psuggest = CreateInputSuggest( k_Lang_English );
		if ( psuggest == NULL )
		{
			Msg( "Failed to load\n" );
			return;
		}
		bLoaded = true;
	}

	doMatch( psuggest, szPrefix, true, true );
#endif
}

#endif


#if 0

// to profile this required a hack in the uiengine's framefunc to
// ensure this code got run in the framefunc

// profile calls for steamui
extern void SteamUIProfileOn();
extern void SteamUIProfileOff();
extern void SteamUIProfileDump();

int cDaisyWheelTest = 0;

void RunDaisywheelTest()
{
	if ( cDaisyWheelTest == 0 )
		return;
	ITextInputSuggest *psuggest = CreateInputSuggest( false, k_Lang_English );
	CUtlRadixTrie trie( false );		// case insensitive building dictionary
	if ( psuggest == NULL )
	{
		Msg( "Failed to load\n" );
		return;
	}

	char rgch[ 2 ];
	rgch[ 1 ] = 0;
	int cIterations = MIN( 10, cDaisyWheelTest );

	for ( int iRun = 0; iRun < cIterations; iRun++ )
	{
		for ( char ch = 'a'; ch <= 'z'; ch++ )
		{
			rgch [ 0 ] = ch;

			doMatch( psuggest, rgch, false, false );
		}
	}


	cDaisyWheelTest -= cIterations;
	if ( cDaisyWheelTest == 0 )
	{
		SteamUIProfileOff();
		SteamUIProfileDump();
	}
	delete psuggest;
}

CON_COMMAND( tenfoot_grind_match, "perf test" )
{
	SteamUIProfileOn();
	cDaisyWheelTest = 100;
}

#endif // 0


//
// command to print out which completions are the slow ones
//

#ifndef PANORAMA_USE_S1WRAPPER

CON_COMMAND_F( tenfoot_text_hotspots, "find text autosuggest hot spots", FCVAR_LINKED_CONCOMMAND )
{
	ITextInputSuggest *psuggest = CreateInputSuggest( k_Lang_English );
	CUtlRadixTrie trie;		// case insensitive building dictionary
	if ( psuggest == NULL )
	{
		Msg( "Failed to load\n" );
		return;
	}

	char rgch[ 2 ];
	rgch[ 1 ] = 0;

	for ( char ch = 'a'; ch <= 'z'; ch++ )
	{
		rgch [ 0 ] = ch;
		doMatch( psuggest, rgch, false, true );
	}

	delete psuggest;
}

#endif