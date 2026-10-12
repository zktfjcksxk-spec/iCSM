//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: QWERTY keyboard text entry method for Steam controller
//=============================================================================//

#include "stdafx_client.h"
#include "panorama/textinput/textinput_dualtouch.h"
#include "panorama/controls/label.h"
#include "panorama/controls/textentry.h"
#include "panorama/controls/image.h"
#include "panorama/input/gamepadcodes.h"
#include "panorama/iuiengine.h"
#include "panorama/panoramacurves.h"
#include "enumutils.h"
#include "textinput_suggest.h"
#include "renderer/uirenderengine.h"
#include "urlhelper.h"
#include <vrapi.h>

#include <functional>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

#define JOYSTICK_AXIS_REPEAT_INTERVAL_START 0.35f
#define JOYSTICK_AXIS_REPEAT_INTERVAL_END 0.05f
#define JOYSTICK_AXIS_REPEAT_CURVE_TIME 0.85f

static const char * k_pchNoTouchPads( "NoTouchPads" );
static const char * k_pchLeftTutorialEnabled( "LeftTutorialEnabled" );
static const char * k_pchRightTutorialEnabled( "RightTutorialEnabled" );
static const char * k_pchIsPasswordVisible( "IsPasswordVisible" );
static const char * k_pchPasteSuggestionPanel( "PasteSuggestionPanel" );
static const char * k_pchEmoticons( "AllowEmoticons" );
static const char * k_pchDontCloseOnEnter( "DontCloseOnEnter" );

REGISTER_PANEL2D( CTextInputDualTouch, TextInputDualTouch );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CTextInputDualTouch::CTextInputDualTouch( panorama::IUIWindow *pParent, const CTextInputHandlerSettings &settings, ITextInputControl *pTextControl ) :
	CTextInputHandler( pParent, settings.GetID() ),
	m_repeatFunction( MAKE_SCHEDULED_FUNC( CTextInputDualTouch::ScheduledKeyRepeatFunction ) )
{
	Initialize( settings, pTextControl );
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CTextInputDualTouch::CTextInputDualTouch( panorama::CPanel2D *parent, const CTextInputHandlerSettings &settings, ITextInputControl *pTextControl ) :
	CTextInputHandler( parent, settings.GetID() ),
	m_repeatFunction( MAKE_SCHEDULED_FUNC( CTextInputDualTouch::ScheduledKeyRepeatFunction ) )
{
	Initialize( settings, pTextControl );
}


//-----------------------------------------------------------------------------
//	Purpose: Destructor
//-----------------------------------------------------------------------------
CTextInputDualTouch::~CTextInputDualTouch()
{
	UnregisterForUnhandledEvent( ActiveControllerTypeChanged(), this, &CTextInputDualTouch::OnActiveControllerTypeChanged );
	
	CTextEntry *pTextEntry = dynamic_cast<CTextEntry*>( m_pTextInputControl->GetAssociatedPanel() );
	
	if ( pTextEntry )
	{
		UnregisterEventHandlerOnPanel( TextEntryChanged(), pTextEntry->UIPanel(), this, &CTextInputDualTouch::EventTextEntryChanged );
	}

	SAFE_DELETE( m_pSuggest );
	SAFE_RELEASE( m_pSteamPadPointerImage );
} 


//-----------------------------------------------------------------------------
// Purpose: Called by constructors to initialize object
//-----------------------------------------------------------------------------
void CTextInputDualTouch::Initialize( const CTextInputHandlerSettings &settings, ITextInputControl *pTextControl )
{
	DbgVerify( BLoadLayout( "file://{resources}/layout/textinput/text_input_dualtouch.xml" ) );

	m_pTextInputControl = pTextControl;
	Assert( m_pTextInputControl != NULL );

	// If associated panel is a CTextEntry, we'll want to check for changes that happen so we can update our preview field
	CTextEntry *pTextEntry = dynamic_cast<CTextEntry*>( m_pTextInputControl->GetAssociatedPanel() );

	if ( pTextEntry )
	{
		pTextEntry->RaiseChangeEvents( true );
		RegisterEventHandlerOnPanel( TextEntryChanged(), pTextEntry->UIPanel(), this, &CTextInputDualTouch::EventTextEntryChanged );
	}

	DbgVerify( m_leftTouchPad.Initialize( this, "LeftPointer", "TouchPadLeft", "LeftTouchPadActive", IUIEngine::k_EHapticFeedbackPosition_Left, UIInputEngine()->BIsFingerDownOnSteamControllerLeftPad() ) );
	DbgVerify( m_rightTouchPad.Initialize( this, "RightPointer", "TouchPadRight", "RightTouchPadActive", IUIEngine::k_EHapticFeedbackPosition_Right, UIInputEngine()->BIsFingerDownOnSteamControllerRightPad() ) );
	m_vecTouchPads.AddToTail( &m_leftTouchPad );
	m_vecTouchPads.AddToTail( &m_rightTouchPad );

	m_pSteamPadPointerImage = ((CTopLevelWindow *)GetParentWindow())->AccessImageManager()->LoadImageFromURL( UIPanel(), NULL, "file://{images}/textinput/textinput_touchpointer.tga", false, k_EImageFormatR8G8B8A8 );
	Assert( m_pSteamPadPointerImage );

	UpdateSteamPadHardwarePointers( false );
	
	m_bOverlayMode = false;
	m_bShowEmoticons = false;
	m_nEmoticonPage = 0;
	
	SetAcceptsFocus( true );
	SetInputNamespace( "dualtouch" );
	
	for ( int i = 0; i < k_SuggestionCount; i++ )
	{
		char pchSuggestionLabelID[32];
		sprintf( pchSuggestionLabelID, "TouchKey_Suggest_%i_text", i );
		m_pSuggestionLabels[i] = assert_cast< CLabel* >( FindChildInLayoutFile( pchSuggestionLabelID ) );
	}
	
	m_pTextPreview = assert_cast< CTextEntry* >( FindChildInLayoutFile( "TextPreview" ) );
	m_pTextPreview->SetAlwaysRenderCaret( true );
	UpdateTextPreview();
	
	m_pBodyContainer = FindChildInLayoutFile( "BodyContainer" );
	Assert( m_pBodyContainer );
	
	m_pBackDrop = FindChildInLayoutFile( "BackDrop" );
	Assert( m_pBackDrop );
	
	RegisterEventHandler( panorama::InputFocusLost(), this, &CTextInputDualTouch::HandleInputFocusLost );
	RegisterEventHandler( TouchKeyStyleChanged(), this, &CTextInputDualTouch::OnTouchKeyStyleChanged );
	RegisterEventHandler( TouchKeyClicked(), this, &CTextInputDualTouch::OnTouchKeyClicked );
	RegisterEventHandler( ImageLoaded(), this, &CTextInputDualTouch::OnImageLoaded );
	RegisterEventHandler( PanelStyleChanged(), this, &CTextInputDualTouch::OnPanelStyleChanged );
	RegisterEventHandler( InputFocusTopLevelChanged(), this, &CTextInputDualTouch::EventInputFocusTopLevelChanged );
	

	m_pLang = assert_cast< CLabel* >( FindChildInLayoutFile( "Lang_txt" ) );
	
	// Add a label to each logical touchkey to act as a keycap
	{
		CUtlVector< IUIPanel * > pTouchKeys;
		int x, y;

		UIPanel()->FindChildrenWithClassTraverse( "TouchKey", pTouchKeys );
		
		FOR_EACH_VEC(pTouchKeys, i)
		{
			CPanel2D *pKey = ToPanel2D( pTouchKeys[i] );
			pKey->SetOnMouseActivateEvent( TouchKeyClicked::MakeEvent( this, pKey, NULL ) );
			pKey->SetOnMouseOverEvent( TouchKeyStyleChanged::MakeEvent( this, pKey, "TouchKeyHover", true ) );
			pKey->SetOnMouseOutEvent( TouchKeyStyleChanged::MakeEvent( this, pKey, "TouchKeyHover", false ) );
			// Only add TouchKeyLabel-IDed labels for non-special keys, they're used for typing
			if ( sscanf( pTouchKeys[i]->GetID(), "TouchKey_%i_%i", &x, &y) == 2 )
			{
				new CLabel( ToPanel2D( pTouchKeys[i] ), "TouchKeyLabel" );
			}
		}
	}
	
	// copied from k_EAnimationEaseIn
	Vector2D vecPoints[4];
	panorama::GetAnimationCurveControlPoints( panorama::k_EAnimationEaseIn, vecPoints );
	m_repeatCurve.SetControlPoints( vecPoints );
	m_repeatGamePadCode = XK_NULL;
	m_repeatStartTime = 0.0f;
	m_repeatNextTime = 0.0f;
	m_repeatCounter = 0;
	
	m_pSuggest = NULL;
	ResetSuggestionState();

	{
		auto *pHeaderTextLabel = assert_cast< CLabel* >( FindChildInLayoutFile( "HeaderText" ) );
		pHeaderTextLabel->SetText( settings.GetHeaderLabel() );

		auto *pSubHeaderDetailTextLabel = assert_cast< CLabel* >( FindChildInLayoutFile( "SubHeaderDetailText" ) );
		pSubHeaderDetailTextLabel->SetText( settings.GetSubHeaderDetailLabel() );
	}
	
	m_eSuggestionMode = settings.BHideSuggestions()
					  ? k_EDualtouchSuggestionMode_NoSuggestions
					  : static_cast<EDualtouchSuggestionMode>( UIEngine()->UISettings()->GetOnScreenKeyboardSuggestionMode() );
	
	// This has to come after initializing m_eSuggestionMode because it might stomp the value.
	SetMode( settings.GetMode() );
	
	// Apply text input settings
	
	// could use the default locale here, but if the user switched languages before initializing
	// panorama, we should honor that
	ELanguage eDefaultInputLang = UIEngine()->UISettings()->GetDefaultInputLanguage();
	if ( eDefaultInputLang == k_Lang_None )
		eDefaultInputLang = UIEngine()->GetCurrentInputLocale();

	if ( !LoadInputConfigurationFile( eDefaultInputLang ) )
	{
		// fall back to English
		LoadInputConfigurationFile( k_Lang_English );
	}

	m_iCharactersTypedSinceModifierStateChanged = 0;
	for ( int i = 0; i < V_ARRAYSIZE( m_bModifierKeysHeld ); i++ )
	{
		m_bModifierKeysHeld[i] = false;
	}
	
	m_currentModifier = k_EDualTouchModifierNone;
	ApplyCurrentModifierLayout();
	
	// Add extra requested classes
	AddClasses( settings.GetClasses() );
	
	// Set whether the keyboard input can be cancelled with "B" / "Back" button
	if ( !settings.BCancellable() )
		AddClass( "NoCancel" );
	
	if ( m_eSuggestionMode == k_EDualtouchSuggestionMode_NoSuggestions )
		AddClass( "NoSuggest" );
	
	m_bCursorMode = false;
	m_pCursorKey = NULL;

	m_bUseTouchPads = !BHasClass( k_pchNoTouchPads );

	if ( m_bUseTouchPads )
	{
		if ( UIInputEngine()->GetActiveControllerType() == k_EActiveControllerType_Steam ||
			UIInputEngine()->GetActiveControllerType() == k_EActiveControllerType_VR )
		{
			static const int s_iRequiredTutorialCount = 1;

			// Has this user never interacted with this dualtouch keyboard before? If so, turn on the tutorial.
			if ( UIEngine()->UISettings()->GetDualTouchTutorialCompletionCount() < s_iRequiredTutorialCount )
			{
				AddClass( k_pchLeftTutorialEnabled );
				AddClass( k_pchRightTutorialEnabled );
			}
		}
	}
	else
	{
		// turn cursor mode on right away if touchpads are disabled, and select the top left key
		m_bCursorMode = true;

		m_pCursorKey = FindChildInLayoutFile( "TouchKey_0_1" );
		m_pCursorKey->UpdateFocusInContext();

		ChangeTouchkeyStyle( m_pCursorKey.Get(), "TouchKeyHover", true );
	}

	m_bShowEmoticons = BHasClass( k_pchEmoticons );
	if ( m_bShowEmoticons )
	{
		SetDefLessFunc( m_mapEmoticonTouchKeys );
		InitEmoticons();
	}

	// By default, when typing with a controller, passwords are visible with a toggle. If we're opening a keyboard
	// for a text entry where we already have text, though, we default to hiding the password to avoid the case
	// where someone wants to adjust but doesn't want people to see. The toggle will still let them show characters
	// if they want.
	if ( m_pTextPreview->GetMode() == k_ETextInputModePassword && m_pTextPreview->GetCharCount() > 0 )
	{
		TogglePasswordVisibility();
	}

	RegisterEventHandler( PropertyTransitionEnd(), this, &CTextInputDualTouch::OnPropertyTransitionEnd );
	RegisterForUnhandledEvent( ActiveControllerTypeChanged(), this, &CTextInputDualTouch::OnActiveControllerTypeChanged );

	const bool bHandledActiveControllerType = OnActiveControllerTypeChanged( UIInputEngine()->GetActiveControllerType() );
	Assert( bHandledActiveControllerType );

	// Default to no active custom suggestions.
	SetSuggestionPanels( CUtlVector<CSuggestionPanel *>() );
}


//-----------------------------------------------------------------------------
//	Purpose: Update our preview mirror when the original text field controlling us changes
//-----------------------------------------------------------------------------
bool CTextInputDualTouch::EventTextEntryChanged( const CPanelPtr< IUIPanel > &pPanel )
{
	// In overlay/minimal mode, UpdateTextPreview() is where we would send the
	// text to our controller, and it would clear the target text field in response,
	// which would trigger infinite recurcion. We don't even show the preview
	// mirror in this case, so safe to do nothing there.
	if ( !m_bOverlayMode )
	{
		UpdateTextPreview();
	}
	// We need to never block this event from bubbling as it's apparently an unhandled one
	// We use it for cosmetic purposes, but folks above us might want it for important stuff,
	// like enabling the login button in the Big Picture login panel
	return false;
}


//-----------------------------------------------------------------------------
//	Purpose:
//-----------------------------------------------------------------------------
void CTextInputDualTouch::TogglePasswordVisibility()
{
	const bool bWasPasswordHidden = (m_pTextPreview->GetMode() == k_ETextInputModePassword);

	m_pTextPreview->SetMode( bWasPasswordHidden ? k_ETextInputModeNormal : k_ETextInputModePassword );
	SetHasClass( k_pchIsPasswordVisible, bWasPasswordHidden );
	SetHasClass( "IsPasswordHidden", !bWasPasswordHidden );
}


//-----------------------------------------------------------------------------
//	Purpose:
//-----------------------------------------------------------------------------
void CTextInputDualTouch::SetMode( ETextInputMode_t mode )
{
	// We want to redirect all numeric passwords to completely different UI (pin pad) rather than ever use
	// this interface for it.
	Assert( mode != k_ETextInputModeNumericPassword );

	m_mode = mode;

	switch ( mode )
	{
		case k_ETextInputModePassword:
			// We used to default to hiding passwords. We want to experiment with making them visible by default
			// but enabling a toggle in the UI. Uncomment this line to switch the default state.
			//m_pTextPreview->SetMode( k_ETextInputModePassword );
			AddClass( "IsPasswordEntry" );
			AddClass( k_pchIsPasswordVisible );

			// Intentional fall through for disabling suggestion engine.
		case k_ETextInputModeEmail:
		case k_ETextInputModeURL:
		case k_ETextInputModeNumericPassword:
		case k_ETextInputModeNumeric:
		case k_ETextInputModePhoneNumber:
		case k_ETextInputModeSteamCode:
			m_eSuggestionMode = k_EDualtouchSuggestionMode_NoSuggestions;
			break;
		case k_ETextInputModeSubmit:
			AddClass( "SubmitButton" );
			AddClass( "DontCloseOnSubmit" );
			AddClass( "DontCloseOnEnter" );
			break;
			
		case k_ETextInputModeNormalLower:
		case k_ETextInputModeNormal:
		default:
			break;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Closes the panel and sends appropriate events
//-----------------------------------------------------------------------------
void CTextInputDualTouch::CloseHandlerImpl( bool bCommitChanges )
{
	if ( !BHasClass( "Destructing") ) 
	{
		// flush pending suggestions regardless of changes being committed
		CancelOutstandingRepeats();
		AddClass( "Destructing" );
		panorama::DispatchEvent( TextInputHandlerStateChange(), this, false );
		const char *pchEntryText =  m_pTextInputControl->PchGetText();
		
		panorama::DispatchEvent( TextInputFinished(), m_pTextInputControl->GetAssociatedPanel(), bCommitChanges, pchEntryText );
		
		DeleteAsync( 0.2f );
		
		UpdateSteamPadHardwarePointers( false );
		
		UnregisterEventHandler( PropertyTransitionEnd(), this, &CTextInputDualTouch::OnPropertyTransitionEnd );

		CPanel2D *pAssociatedPanel = GetControlInterface()->GetAssociatedPanel();
		if ( pAssociatedPanel )
		{
			pAssociatedPanel->RemoveClass( "TextInputHandlerActive" );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Submit the text and clear state without closing
//-----------------------------------------------------------------------------
void CTextInputDualTouch::SubmitTextNoClose( void )
{
	if ( !BHasClass( "Destructing" ) )
	{
		ResetSuggestionState();
		CancelOutstandingRepeats();

		panorama::DispatchEvent( panorama::TextEntrySubmit(), m_pTextInputControl->GetAssociatedPanel(), m_pTextInputControl->PchGetText() );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Return the control interface
//-----------------------------------------------------------------------------
ITextInputControl *CTextInputDualTouch::GetControlInterface()
{
	return m_pTextInputControl;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CTextInputDualTouch::SetSuggestionPanels( const CUtlVector<CSuggestionPanel *>& vecPanels )
{
	static CPanoramaSymbol k_symSuggestionsCount0( "SuggestionsCount0" );
	static CPanoramaSymbol k_symSuggestionsCount3( "SuggestionsCount3" );
	static CPanoramaSymbol k_symSuggestionsCount4( "SuggestionsCount4" );

	if ( vecPanels.Count() == 0  )
		SetDialogVariable( "searchresultcount", 0 );

	// ...
	// cleanup previous state
	RemoveClass( k_symSuggestionsCount0 );
	RemoveClass( k_symSuggestionsCount3 );
	RemoveClass( k_symSuggestionsCount4 );

	for ( auto& pTouchPad : m_vecTouchPads )
	{
		for ( auto pExistingCustomSuggestionPanel : m_vecCustomSuggestionPanels )
		{
			pTouchPad->m_vecTouchKeys.FindAndFastRemove( pExistingCustomSuggestionPanel->UIPanel() );
		}
	}

	// ...
	m_vecCustomSuggestionPanels.Purge();
	m_vecCustomSuggestionPanels.AddVectorToTail( vecPanels );

	switch ( vecPanels.Count() )
	{
	case 0:
		AddClass( k_symSuggestionsCount0 );
		return true;

	case 3:
		AddClass( k_symSuggestionsCount3 );
		return true;

	case 4:
		AddClass( k_symSuggestionsCount4 );

		CPanel2D *pLeftParent = FindChildTraverse( "CustomSuggestions_4_WrapperLeft" );
		CPanel2D *pRightParent = FindChildTraverse( "CustomSuggestions_4_WrapperRight" );

		// Async delete in case we are in the middle of a focus callback for a suggestion panel
		for ( CPanel2D *pChild : pLeftParent->Children() )
		{ 
			pChild->DeleteAsync( 0.2f );
		}
		for ( CPanel2D *pChild : pRightParent->Children() )
		{
			pChild->DeleteAsync( 0.2f );
		}

		for ( int iPanel = 0; iPanel < 4; iPanel++ )
		{
			vecPanels[iPanel]->SetParent( iPanel < 2 ? pLeftParent : pRightParent );
			vecPanels[iPanel]->SetOnMouseOverEvent( TouchKeyStyleChanged::MakeEvent( this, vecPanels[iPanel], "TouchKeyHover", true ) );
			vecPanels[iPanel]->SetOnMouseOutEvent( TouchKeyStyleChanged::MakeEvent( this, vecPanels[iPanel], "TouchKeyHover", false ) );
		}

		m_leftTouchPad.m_vecTouchKeys.AddToTail( vecPanels[0]->UIPanel() );
		m_leftTouchPad.m_vecTouchKeys.AddToTail( vecPanels[1]->UIPanel() );
		m_rightTouchPad.m_vecTouchKeys.AddToTail( vecPanels[2]->UIPanel() );
		m_rightTouchPad.m_vecTouchKeys.AddToTail( vecPanels[3]->UIPanel() );

		return true;
	}

	Assert( !"Invalid number of suggestion panels!" );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CTextInputDualTouch::OnTouchKeyStyleChanged( CPanelPtr<CPanel2D> pPanel, const char *pszStyle, bool bAddedStyle )
{
	if ( this->GetParentWindow()->BIsVROverlay() && !strcmp( "TouchKeyHover", pszStyle ) )
	{
		// Only do haptics here for the VR laser-pointer input
		// If using the touchpads GetPrimaryDashboardDevice will return k_unTrackedDeviceIndexInvalid

		vr::TrackedDeviceIndex_t unPrimaryDevice = vrapi::VRDashboardManager()->GetPrimaryDashboardDevice();
		if ( unPrimaryDevice != vr::k_unTrackedDeviceIndexInvalid )
		{
			// Trigger a low pulse on the active controller
			vrapi::VRSystem()->TriggerHapticPulse( unPrimaryDevice, 0, 360 );
		}
	}
	if ( pPanel.Get() )
	{
		ChangeTouchkeyStyle( pPanel.Get(), pszStyle, bAddedStyle );
	}
	return true;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CTextInputDualTouch::OnImageLoaded( const CPanelPtr< IUIPanel > &pPanel, IImageSource *pImage )
{
	Assert( m_pSteamPadPointerImage );

	if ( pImage == m_pSteamPadPointerImage )
	{
		UpdateSteamPadSoftwarePointerImage( m_pSteamPadPointerImage->GetTextureID() );
		UpdateSteamPadHardwarePointerVisibility();
	}

	// Always let it bubble.
	return false;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CTextInputDualTouch::OnPanelStyleChanged( const CPanelPtr< IUIPanel > &pPanel )
{
	IUIPanel* pTarget = pPanel.Get();
	
	// These panels can move around in minimal mode, causing the hardware cursors to
	// need re-alignment.
	if ( pTarget == m_pBodyContainer->UIPanel() || pTarget == m_pBackDrop->UIPanel() )
	{
		m_bOverlayMode = BHasClass( "MinimalKeyboard" );
		
		UpdateSteamPadHardwarePointers( m_bHardwareCursorsEnabled );
	}
	
	// Always let it bubble.
	return false;
}


//-----------------------------------------------------------------------------
// Schedule key repeats or cancel scheduled key repeats
//-----------------------------------------------------------------------------
void CTextInputDualTouch::ScheduleKeyRepeats( panorama::GamePadCode eCode )
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
// Handle scheduled key repeats
//-----------------------------------------------------------------------------
void CTextInputDualTouch::ScheduledKeyRepeatFunction()
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
// Purpose: 
//-----------------------------------------------------------------------------
static bool BIsDualTouchTutorialActive( const CPanel2D *pDualTouchPanel )
{
	Assert( pDualTouchPanel );
	
	return pDualTouchPanel->BHasClass( k_pchLeftTutorialEnabled )
		|| pDualTouchPanel->BHasClass( k_pchRightTutorialEnabled );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTextInputDualTouch::CursorMove( const panorama::GamePadData_t &code )
{
	if ( BIsDualTouchTutorialActive( this ) )
		return;

	if ( m_bCursorMode == false )
	{
		m_bCursorMode = true;
	}

	if ( !m_pCursorKey.Get() )
	{
		m_pCursorKey = FindChildInLayoutFile( "TouchKey_0_1" );
	}

	float flTouchKeyWidth = m_pCursorKey->GetActualRenderWidth();
	float flTouchKeyHeight = m_pCursorKey->GetActualRenderHeight();
	
	float flOriginPosX, flOriginPosY;
	float flTargetPosX, flTargetPosY;

	m_leftTouchPad.m_pPadPanel->GetPositionWithinWindow( &flOriginPosX, &flOriginPosY );
	m_pCursorKey->GetPositionWithinWindow( &flTargetPosX, &flTargetPosY );
	
	flTargetPosX -= flOriginPosX;
	flTargetPosY -= flOriginPosY;

	// Start from center of touchkey
	flTargetPosX += flTouchKeyWidth / 2.0f;
	flTargetPosY += flTouchKeyHeight / 2.0f;
	
	switch ( code.m_GamePadCode )
	{
		case STEAM_LEFTSTICK_UP:
		case XK_STICK1_UP:
		case XK_STICK2_UP:
		case XK_BUTTON_UP:
			flTargetPosY -= flTouchKeyHeight;
		break;
		case STEAM_LEFTSTICK_DOWN:
		case XK_STICK1_DOWN:
		case XK_STICK2_DOWN:
		case XK_BUTTON_DOWN:
			flTargetPosY += flTouchKeyHeight;
			break;
		case STEAM_LEFTSTICK_LEFT:
		case XK_STICK1_LEFT:
		case XK_STICK2_LEFT:
		case XK_BUTTON_LEFT:
			flTargetPosX -= flTouchKeyWidth;
			break;
		case STEAM_LEFTSTICK_RIGHT:
		case XK_STICK1_RIGHT:
		case XK_STICK2_RIGHT:
		case XK_BUTTON_RIGHT:
			flTargetPosX += flTouchKeyWidth;
			break;
		default:
			Assert( 0 );
			return;
	}
	
	float flParentX, flParentY;
	m_leftTouchPad.m_pPadPanel->GetPositionWithinWindow( &flParentX, &flParentY );

	for ( auto *pTouchPad : m_vecTouchPads )
	{
		for ( auto *pTouchKey : pTouchPad->m_vecTouchKeys )
		{
			// Always hit relative to the left touchpad since we're crossing pads for cursor mode, our coordinate system encompasses both
			if ( pTouchPad->OverlapsTouchKey( flTargetPosX, flTargetPosY, flParentX, flParentY, pTouchKey, kOverlapTest_OverlapActualPositionIgnoringDeadzone ) )
			{
				CPanel2D* pHitKey = ToPanel2D( pTouchKey );
				
				ChangeTouchkeyStyle( m_pCursorKey.Get(), "TouchKeyHover", false );
				m_pCursorKey = pHitKey;
				ChangeTouchkeyStyle( m_pCursorKey.Get(), "TouchKeyHover", true );
				
				UIEngine()->PulseActiveControllerHaptic(
					IUIEngine::k_EHapticFeedbackPosition_Left,
					IUIEngine::k_EHapticFeedbackStrength_Low );
				UIEngine()->PulseActiveControllerHaptic(
					IUIEngine::k_EHapticFeedbackPosition_Right,
					IUIEngine::k_EHapticFeedbackStrength_Low );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTextInputDualTouch::OnGamePadDown( const panorama::GamePadData_t &code )
{
	switch ( code.m_GamePadCode )
	{
		// Touchpads don't exist off the Steam controller. In addition to the touchpads, triggers can also
		// submit characters for Steam controllers.
		case STEAM_BUTTON_LPAD_TOUCH:
		{
			if ( m_bUseTouchPads )
			{
				m_leftTouchPad.OnTouch();
				DisableCursorMode();
				UpdateSteamPadHardwarePointerVisibility();
			}
			return true;
		}
		case STEAM_LTRIGGER_ANALOG:
			if ( code.m_RepeatCount )
				return true;
			UIEngine()->PulseActiveControllerHaptic(
				IUIEngine::k_EHapticFeedbackPosition_Left,
				IUIEngine::k_EHapticFeedbackStrength_VeryHigh );
		case STEAM_BUTTON_LPAD_CLICKED:
		{
			m_leftTouchPad.OnButtonDown();
			return true;
		}
		case STEAM_BUTTON_RPAD_TOUCH:
		{
			if ( m_bUseTouchPads )
			{
				m_rightTouchPad.OnTouch();
				DisableCursorMode();
				UpdateSteamPadHardwarePointerVisibility();
			}
			return true;
		}
		case STEAM_RTRIGGER_ANALOG:
			if ( code.m_RepeatCount )
				return true;
			UIEngine()->PulseActiveControllerHaptic(
				IUIEngine::k_EHapticFeedbackPosition_Right,
				IUIEngine::k_EHapticFeedbackStrength_VeryHigh );
		case STEAM_BUTTON_RPAD_CLICKED:
		{
			m_rightTouchPad.OnButtonDown();
			return true;
		}

		// Swallow trigger events so that keyboard won't leak the events and allow silo changes.
		case STEAM_BUTTON_LTRIGGER:
		case STEAM_BUTTON_RTRIGGER:
			return true;

		case STEAM_BUTTON_SELECT:
		case XK_BUTTON_BACK:
		{
			SwitchLanguage();
			return true;
		}
		// Text entry/cancelling.
		case STEAM_BUTTON_START:
		case XK_BUTTON_START:
		{
			if ( !DispatchEvent( TextInputUnhandledButtonPress(), m_pTextInputControl->GetAssociatedPanel(), code ) )
			{
				CloseHandler( true );
			}
			return true;
		}
		case STEAM_BUTTON_B:
		case XK_BUTTON_B:
		{
			CloseHandler( false );
			return true;
		}
		case STEAM_BUTTON_A:
		case XK_BUTTON_A:
		{
			if ( m_bCursorMode && m_pCursorKey.Get() )
			{
				ScheduleKeyRepeats( code.m_GamePadCode );
				OnTouchKeyClicked( m_pCursorKey.Get(), NULL );
			}
			else
			{
				return false;
			}
			
			return true;
		}
		case STEAM_BUTTON_Y:
		case XK_BUTTON_Y:
		{
			if ( m_mode == k_ETextInputModePassword )
			{
				TogglePasswordVisibility();
			}			
			else
			{
				// this button event is special case passed back to the invoking textinput
				DispatchEvent( TextInputUnhandledButtonPress(), m_pTextInputControl->GetAssociatedPanel(), code );
			}
			return true;
		}
		case STEAM_BUTTON_X:
		case XK_BUTTON_X:
		{
			// this button event is special case passed back to the invoking textinput
			DispatchEvent( TextInputUnhandledButtonPress(), m_pTextInputControl->GetAssociatedPanel(), code );
			return true;
		}
		case STEAM_BUTTON_LSHOULDER:
		case XK_BUTTON_LEFT_SHOULDER:
		{
			if ( code.m_RepeatCount > 0 )
				return true;
			
			ScheduleKeyRepeats( code.m_GamePadCode );
			PerformBackspace();
			return true;
		}
		case STEAM_BUTTON_RSHOULDER:
		case XK_BUTTON_RIGHT_SHOULDER:
		{
			if ( code.m_RepeatCount > 0 )
				return true;
			
			ScheduleKeyRepeats( code.m_GamePadCode );
			TypeSpace();
			return true;
		}
		// The Steam controller uses the back grips to switch input modes (ie., caps, symbols) and the triggers to
		// submit characters. Because non-Steam controllers don't have back grips, we need a different mode-switch
		// set of buttons. Because they also don't have touchpads, we use the triggers because we don't need them
		// to select characters anymore.
		case STEAM_BUTTON_LBACK:
		case XK_BUTTON_LTRIGGER:
		{
			SetModifierKeyState( k_EDualTouchModifierShift, true );
			
			return true;
		}
		case STEAM_BUTTON_RBACK:
		case XK_BUTTON_RTRIGGER:
		{
			SetModifierKeyState( k_EDualTouchModifierAlt, true );
			
			return true;
		}
		case STEAM_LEFTSTICK_UP:
		case STEAM_LEFTSTICK_DOWN:
		case STEAM_LEFTSTICK_LEFT:
		case STEAM_LEFTSTICK_RIGHT:
		case XK_STICK1_UP:					// left analog stick
		case XK_STICK1_DOWN:
		case XK_STICK1_LEFT:
		case XK_STICK1_RIGHT:
		case XK_STICK2_UP:					// right analog stick
		case XK_STICK2_DOWN:
		case XK_STICK2_LEFT:
		case XK_STICK2_RIGHT:
		case XK_BUTTON_UP:					// D-pad
		case XK_BUTTON_DOWN:
		case XK_BUTTON_LEFT:
		case XK_BUTTON_RIGHT:
		{
			CursorMove( code );
			return true;
		}
	}
	
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTextInputDualTouch::UpdateSteamPadHardwarePointers( bool bSteamPadHardwarePointersEnabled )
{
	m_bHardwareCursorsEnabled = bSteamPadHardwarePointersEnabled;

 	m_leftTouchPad.UpdatePointerState( bSteamPadHardwarePointersEnabled );
	m_rightTouchPad.UpdatePointerState( bSteamPadHardwarePointersEnabled );
	
	SetHasClass( "ShowPanoramaPointers", !bSteamPadHardwarePointersEnabled );

	UpdateSteamPadSoftwarePointerImage( m_pSteamPadPointerImage->GetTextureID() );
	UpdateSteamPadHardwarePointerVisibility();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTextInputDualTouch::UpdateSteamPadSoftwarePointerImage( uint32 unTextureID )
{
	m_leftTouchPad.m_renderPointerState.nTextureID = unTextureID;
	m_rightTouchPad.m_renderPointerState.nTextureID = unTextureID;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTextInputDualTouch::UpdateSteamPadHardwarePointerVisibility()
{
	SteamPadPointer_t LeftPointerState = m_leftTouchPad.m_renderPointerState;
	SteamPadPointer_t RightPointerState = m_rightTouchPad.m_renderPointerState;

	LeftPointerState.bVisible = LeftPointerState.bVisible && m_leftTouchPad.m_bFingerOnPad;
	RightPointerState.bVisible = RightPointerState.bVisible && m_rightTouchPad.m_bFingerOnPad;

	static_cast<CTopLevelWindow *>( GetParentWindow() )->GetUIRenderEngine()->Access3DSurface()->UpdateSteamPadPointers( &LeftPointerState, &RightPointerState );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
static void ChangeTouchkeyStyleRecurse( const char *pszTargetID, CPanel2D *pParent, const char *pchStyle, bool bAddStyle )
{
	if ( !V_stricmp( pszTargetID, pParent->GetID() ) )
	{
		pParent->SetHasClass( pchStyle, bAddStyle );
	}

	for ( CPanel2D *pChild : pParent->Children() )
	{
		ChangeTouchkeyStyleRecurse( pszTargetID, pChild, pchStyle, bAddStyle );
	}
}


//-----------------------------------------------------------------------------
// Purpose: This adds or removes a style from a touchkey while applying it to any linked keys also. It's a requirement
// that all keys have an ID, but not that they're necessarily unique.
//-----------------------------------------------------------------------------
void CTextInputDualTouch::ChangeTouchkeyStyle( CPanel2D *pTouchKey, const char *pchStyle, bool bAddStyle )
{
	Assert( pTouchKey->GetID() );
	// check if we are touching a suggestion and if so style appropriately
	int iSuggestedPanel = m_vecCustomSuggestionPanels.Find( (CSuggestionPanel *)pTouchKey);
	int nSuggestPanels = m_vecCustomSuggestionPanels.Count();
	for ( int i = 0; i < nSuggestPanels; i++ )
	{
		CLabel *pLabel = assert_cast<CLabel *>( FindChildInLayoutFile( CFmtStr( "Suggest_%d_text", i ) ) );
		if ( pLabel )
		{
			pLabel->SetHasClass( "SuggestionActive", i == iSuggestedPanel );
			pLabel->SetText( m_vecCustomSuggestionPanels[i]->PchGetSuggestionTitle() );
		}
	}
	SetHasClass( "SuggestionActive", iSuggestedPanel != m_vecCustomSuggestionPanels.InvalidIndex() );

	ChangeTouchkeyStyleRecurse( pTouchKey->GetID(), this, pchStyle, bAddStyle );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTextInputDualTouch::PerformBackspace( void )
{
	if ( m_bOverlayMode )
	{
		char szBackspace[2] = { SENDTEXT_SPECIALKEY_BACKSPACE, 0 };
		
		TypeCharacters( szBackspace );
	}
	else
	{
		panorama::KeyData_t code = { k_ePanelEventSourceProgram, KEY_BACKSPACE };
		m_pTextInputControl->OnKeyDown( code );
		m_pTextInputControl->OnKeyUp( code ); // also send the up event!!
	}

	if ( m_PossibleWordsBeingTyped.Count() > 0 )
	{
		m_PossibleWordsBeingTyped.Pop();
	}
	
	UpdateSuggestionWords();
	UpdateTextPreview();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTextInputDualTouch::OnTouchKeyClicked( CPanel2D *pTouchKey, CTouchPad *pTouchPad )
{
	if (BIsDualTouchTutorialActive(this))
	{
		RemoveClass(k_pchLeftTutorialEnabled);
		RemoveClass(k_pchRightTutorialEnabled);
		UIEngine()->UISettings()->OnDualTouchTutorialCompleted();

		return true;
	}

	if ( strcmp ( pTouchKey->GetID(), "TouchKey_Backspace" ) == 0 )
	{
		PerformBackspace();
	}
	else if ( strcmp ( pTouchKey->GetID(), "TouchKey_Shift" ) == 0 )
	{
		const bool bWasModifierKeyAlreadyHeld = m_bModifierKeysHeld[ k_EDualTouchModifierShift ];
		if ( !bWasModifierKeyAlreadyHeld )
		{
			SetModifierKeyState( k_EDualTouchModifierShift, true );
			SetModifierKeyState( k_EDualTouchModifierShift, false );
		}
	}
	else if ( strcmp ( pTouchKey->GetID(), "TouchKey_Alt" ) == 0 )
	{
		const bool bWasModifierKeyAlreadyHeld = m_bModifierKeysHeld[ k_EDualTouchModifierAlt ];
		if ( !bWasModifierKeyAlreadyHeld )
		{
			SetModifierKeyState( k_EDualTouchModifierAlt, true );
			SetModifierKeyState( k_EDualTouchModifierAlt, false );
		}
	}
	else if ( strcmp ( pTouchKey->GetID(), "TouchKey_Enter" ) == 0 )
	{
		if ( m_bOverlayMode )
		{
			char szEnter[2] = { SENDTEXT_SPECIALKEY_ENTER, 0 };
			TypeCharacters( szEnter );
		}
		else
		{
			CloseHandler( true );
		}
	}
	else if ( strcmp( pTouchKey->GetID(), "TouchKey_EnterNoClose" ) == 0 )
	{
		SubmitTextNoClose(); // user hit the Submit button (visible in Chat)
	}
	else if ( strcmp( pTouchKey->GetID(), "TouchKey_Close" ) == 0 )
	{
		CloseHandler( true ); // user hit the Done button
	}
	else if ( strcmp( pTouchKey->GetID(), "TouchKey_Submit" ) == 0 )
	{
		SubmitTextNoClose();
	}
	else if ( strcmp( pTouchKey->GetID(), "TouchKey_Show_Emoticons" ) == 0 )
	{
		SetEmoticonMode( true );
	}
	else if ( strcmp( pTouchKey->GetID(), "TouchKey_Hide_Emoticons" ) == 0 )
	{
		SetEmoticonMode( false );
	}
	else if ( strcmp( pTouchKey->GetID(), "TouchKey_Next_Emoticon" ) == 0 )
	{
		EmoticonPageRight();
	}
	else if ( strcmp( pTouchKey->GetID(), "TouchKey_Prev_Emoticon" ) == 0 )
	{
		EmoticonPageLeft();
	}
	else
	{
		m_iCharactersTypedSinceModifierStateChanged++;

		int suggestionID;
		if ( sscanf( pTouchKey->GetID(), "TouchKey_Suggest_%i", &suggestionID ) == 1 )
		{
			OnSuggestionSelected( suggestionID );
			TypeSpace();
		}
		else if ( strcmp ( pTouchKey->GetID(), "TouchKey_Space" ) == 0 )
		{
			TypeSpace();
		}
		else if ( strncmp( pTouchKey->GetID(), "EmoticonTouchKey", 16 ) == 0 )
		{
			OnEmoticonClicked( pTouchKey );
		}
		else
		{
			// Possibly a regular keycap
			OnStandardTouchKeyClicked( pTouchKey, pTouchPad );
		}
	}
	
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTextInputDualTouch::OnStandardTouchKeyClicked( CPanel2D *pTouchKey, CTouchPad *pTouchPad )
{
	Assert( pTouchKey );

	CLabel *pTouchKeyLabel = assert_cast<CLabel *>( pTouchKey->FindChild("TouchKeyLabel") );

	// If we don't have a label that we found, and we weren't one of the special keys that we check
	// for by ID, we assume that we're a special panel that knows what it wants to do with logic that
	// lives outside the keyboard.
	if ( !pTouchKeyLabel )
	{
		DispatchEvent( panorama::Activated(), pTouchKey, panorama::k_ePanelEventSourceGamepad );
		return;
	}
	
	// Send whatever our label text was to the text entry control.
	bool bSuggestionsReset = TypeCharacters( pTouchKeyLabel->PchGetText() );
				
	const EDualTouchModifier_t eDesiredModifierState = CalculateDesiredModifierState();
	if ( m_currentModifier != eDesiredModifierState )
	{
		ApplyCurrentModifierLayout();
	}
	
	// We don't expect to store off candidate words at all if we don't have a suggestion engine enabled.
	// If we haven't typed a character yet it's possible to have an empty candidate set but have a suggestion
	// engine active.
	Assert( m_PossibleWordsBeingTyped.Count() == 0 || m_pSuggest );

	// The TypeCharacters call above might have flushed predictions out if it was a special char. Skip typo
	// detection if that's the case.
	if ( m_pSuggest && !bSuggestionsReset )
	{
		CUtlVector<const char *> vecCandidateKeysUTF8;

		vecCandidateKeysUTF8.AddToTail( pTouchKeyLabel->PchGetText() );

		// We may not have a touchpad wrapper panel passed in if we're using the analog stick for key entry.
		// We use the touchpad to determine potential typo keys and we ignore typo correction entirely for
		// analog stick entry so doing no work here is appropriate.
		Assert( m_eSuggestionMode == k_EDualtouchSuggestionMode_WithTypoCorrection || m_eSuggestionMode == k_EDualtouchSuggestionMode_NoTypoCorrection );

		if ( pTouchPad && m_eSuggestionMode == k_EDualtouchSuggestionMode_WithTypoCorrection )
		{
			float flParentX, flParentY;
			pTouchPad->m_pPadPanel->GetPositionWithinWindow( &flParentX, &flParentY );

			for ( const auto pTouchedKey : pTouchPad->m_vecTouchKeys )
			{
				if ( !pTouchPad->OverlapsTouchKey( pTouchPad->m_hoverX, pTouchPad->m_hoverY, flParentX, flParentY, pTouchedKey, kOverlapTest_OnlyTestNeighbors ) )
					continue;

				CPanel2D* pHitKey = ToPanel2D( pTouchedKey );
				CLabel *pPossibleTypoKeyLabel = assert_cast<CLabel *>( pHitKey->FindChild("TouchKeyLabel") );
			
				// Ignore functional buttons (not simple keys) for typo detection.
				if ( !pPossibleTypoKeyLabel )
					continue;

				// Duplicate all elements of our backup and append the character
				// we think the user meant instead of the one they actually hit
				vecCandidateKeysUTF8.AddToTail( pPossibleTypoKeyLabel->PchGetText() );
			}
		}

		VecCandidateWordRoots_t *pNewPossibleWords = new VecCandidateWordRoots_t;
		
		// Check if we're in the middle of an alphanumeric string and prime possible words with it.
		for ( const char *pszCandidateToken : vecCandidateKeysUTF8 )
		{
			if ( m_PossibleWordsBeingTyped.Count() == 0 )
			{
				// We're starting a new word. Feed in exactly what we typed as a potential word root.
				pNewPossibleWords->AddToTail( pszCandidateToken );
			}
			else
			{
				// We have existing word roots. Add this new character data to the end of all of them.
				for ( const auto& PossibleWord : *m_PossibleWordsBeingTyped.Top() )
				{
					pNewPossibleWords->AddToTail( CUtlStringBuilder::Concat( PossibleWord, pszCandidateToken ) );
				}
			}
		}

		m_PossibleWordsBeingTyped.Push( pNewPossibleWords );

		// Walk through our list of all candidate word roots and remove anything from the current working set
		// that can't lead to any words. We always leave the base element, what the user typed pre-typo-correction,
		// because other code assumes that the first element in the list is always sourced directly from the user.
		{
			CUtlString sDummy;
			FOR_EACH_VEC_BACK( *m_PossibleWordsBeingTyped.Top(), i )
			{
				if ( i > 0 && !m_pSuggest->SuggestWord( (*m_PossibleWordsBeingTyped.Top())[i].Get(), sDummy ) )
				{
					m_PossibleWordsBeingTyped.Top()->Remove( i );
				}
			}
		}

		// Resolve all the possibilities into a compiled list of suggestions
		UpdateSuggestionWords();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTextInputDualTouch::OnGamePadUp( const panorama::GamePadData_t &code )
{
	switch ( code.m_GamePadCode )
	{
		case STEAM_BUTTON_LPAD_TOUCH:
		{
			m_leftTouchPad.OnRelease();
			UpdateSteamPadHardwarePointerVisibility();
			return true;
		}
		case STEAM_BUTTON_RPAD_TOUCH:
		{
			m_rightTouchPad.OnRelease();
			UpdateSteamPadHardwarePointerVisibility();
			return true;
		}

		// Keyboard character page selection modifiers.
		case STEAM_BUTTON_LBACK:
		case XK_BUTTON_LTRIGGER:
		{
			SetModifierKeyState( k_EDualTouchModifierShift, false );
			return true;
		}
		case STEAM_BUTTON_RBACK:
		case XK_BUTTON_RTRIGGER:
		{
			SetModifierKeyState( k_EDualTouchModifierAlt, false );
			return true;
		}
	}
	
	if ( ( m_repeatGamePadCode != XK_NULL ) && ( m_repeatGamePadCode == code.m_GamePadCode ) )
		CancelOutstandingRepeats();
	
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: We're done moving the keyboard onto the screen, enable the fast cursor path
//-----------------------------------------------------------------------------
bool CTextInputDualTouch::OnPropertyTransitionEnd( const CPanelPtr< IUIPanel > &pPanel, CStyleSymbol prop )
{
	if ( pPanel.Get() == m_pBodyContainer->UIPanel() )
	{
		UpdateSteamPadHardwarePointers( true );
		
		return true;
	}
	
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTextInputDualTouch::DisableCursorMode( void )
{
	if ( m_bCursorMode == false )
		return;
	
	m_bCursorMode = false;
	
	ChangeTouchkeyStyle( m_pCursorKey.Get(), "TouchKeyHover", false );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static bool HandleTouchpadMoveEvent( CTextInputDualTouch *pDualTouchPanel, CTouchPad& TouchPad, const char *pszTutorialClass, const panorama::GamePadData_t &code )
{
	if ( TouchPad.m_bFingerOnPad )
	{
		if ( pDualTouchPanel->BHasClass( pszTutorialClass ) )
		{
			pDualTouchPanel->RemoveClass( pszTutorialClass );
			if ( !BIsDualTouchTutorialActive( pDualTouchPanel ) )
			{
				UIEngine()->UISettings()->OnDualTouchTutorialCompleted();
			}
		}
	}

	return TouchPad.OnMove( code.m_fValueXRaw, code.m_fValueYRaw );
}

//-----------------------------------------------------------------------------
// Handle gamepad analog input
//-----------------------------------------------------------------------------
bool CTextInputDualTouch::OnGamePadAnalog( const panorama::GamePadData_t &code )
{
	if ( !m_bUseTouchPads )
		return false;

	if ( code.m_GamePadCode == STEAM_LEFTPAD_ANALOG )
		return HandleTouchpadMoveEvent( this, m_leftTouchPad, k_pchLeftTutorialEnabled, code );

	if ( code.m_GamePadCode == STEAM_RIGHTPAD_ANALOG )
		return HandleTouchpadMoveEvent( this, m_rightTouchPad, k_pchRightTutorialEnabled, code );
		
	// This code isn't intended for use with non-Steam controllers. There are other analog inputs on
	// the Steam controller (left analog stick) as well as XInput analog inputs but we just ignore
	// them.
	return false;
}


//-----------------------------------------------------------------------------
// Types a given unicode character into text entry
//-----------------------------------------------------------------------------
bool CTextInputDualTouch::TypeCharacters( const char *pszUTF8 )
{
	Assert( V_UnicodeValidate( pszUTF8 ) );

	// Ugh. Because part of our interface to the text controls is in uchar32, we have to convert here.
	// All we really want to do is use UTF8 all the way through the whole pipeline, but until we make
	// that change we're stuck converting here. Once we do make that change we can remove all of this.
	CStrAutoEncode s( pszUTF8 );
	const uchar32 *psz32 = s.ToUTF32();
	
	// Always insert a character into our target text buffer.
	m_pTextInputControl->InsertCharactersAtCursor( psz32, V_strlen32( psz32 ) );

	// If we're not an alphanumeric character, flush our suggestions, ie., if we're typing something
	// that might be a word and we hit comma, start a new word.
	//
	// Note: on Windows, iswalnum() can only deal with a single UTF16 (!) value, which won't correctly
	// handle anything that takes two UTF16 values for a single codepoint. We could do the unfortunate
	// hack here of assuming that anything outside the BMP is alphanumeric but those characters have
	// enough other problems now and we don't currently have any keyboards with them so we just ignore
	// the bug for now.
	bool bResetSuggestionState = (m_eSuggestionMode == k_EDualtouchSuggestionMode_NoSuggestions);

	for ( const uchar32 *pch32 = psz32; *pch32 != '\0'; pch32++ )
	{
		bResetSuggestionState = bResetSuggestionState || !V_iswalnum32( *pch32 );

		if ( bResetSuggestionState )
			break;
	}
	
	if ( bResetSuggestionState )
	{
		// For non-alphanumeric characters, end our current suggestion. We've already put the
		// character into the buffer so we don't have any other work to do here.
		ResetSuggestionState();
	}

	UpdateTextPreview();

	return bResetSuggestionState;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
struct SuggestCandidate_t
{
	char rgch[ k_cSmallBuff ];
	float probability;
	unsigned int strLen;
	bool bWasRootWithNoTypoCorrection;

	float GetProbabilityForHeadSort() const { return bWasRootWithNoTypoCorrection ? probability : 0.0f; }
	unsigned int GetStringLenghtForHeadSort() const { return bWasRootWithNoTypoCorrection ? strLen : 0.0f; }
};


// Prioritize shorter suggestions first regardless of their frequency; this could be
// changed into a weight if we started doing n-gram suggestion but right now the main
// usecase is autocorrect
static bool SuggestCandidateLessFunc( const SuggestCandidate_t &c1, const SuggestCandidate_t &c2 )
{
	if (c1.strLen != c2.strLen) return c1.strLen < c2.strLen;
	
	return c1.probability > c2.probability;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTextInputDualTouch::UpdateTextPreview( void )
{
	m_pTextPreview->SetText( m_pTextInputControl->PchGetText() );
	m_pTextPreview->SetCursorOffset( m_pTextInputControl->GetCursorOffset() );
	
	if ( m_bOverlayMode || this->GetParentWindow()->BIsVROverlay() )
	{
		panorama::DispatchEvent( TextInputSent(), m_pTextInputControl->GetAssociatedPanel(), m_pTextInputControl->PchGetText() );
	}

	enum { kLongStringLength = 18 };
	SetHasClass( "IsLongString", V_strlen( m_pTextInputControl->PchGetText() ) >= kLongStringLength );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
static void GenerateSuggestionCandidates( const ITextInputSuggest *pSuggestionEngine, unsigned int unMaxSuggestionCount, const CUtlVector< CUtlString >& vecRoots, CUtlVector< CUtlString > *out_pvecResults )
{
	Assert( pSuggestionEngine );
	Assert( unMaxSuggestionCount > 0 );
	Assert( out_pvecResults );
	Assert( out_pvecResults->Count() == 0 );

	// this weights the frequency of words typed without any possible typos above
	// autocorrected words, as otherwise if you legitimately type 'test' it'll
	// auto-correct into 'rest'
	static const float k_flExactWordFrequencyWeight = 1.7f;

	CUtlVector<SuggestCandidate_t> vecCandidates;
	CUtlDict<bool> dictSeenWords;

	FOR_EACH_VEC( vecRoots, i )
	{
		CandidateList_t vecRootCandidates( UtlRadixTrieCandidateLessFunc );
		pSuggestionEngine->SuggestWords( vecRoots[i].Get(), vecRootCandidates, unMaxSuggestionCount );

		// We don't expect to have any roots that have no candidate words as we prevent them from being added
		// to the candidate set in that case, except for our zeroth entry, which is whatever the user typed,
		// regardless of whether it makes sense.
		Assert( i == 0 || vecRootCandidates.Count() > 0 );

		for ( const auto& InCandidate : vecRootCandidates )
		{
			// Prevent having multiple suggestions that are all the same word, generated from different roots.
			if ( dictSeenWords.HasElement( InCandidate.rgch ) )
				continue;

			const unsigned int unStringLength = V_UnicodeLength( InCandidate.rgch );

			// The first item in our list of results has no typos corrected so we weight it higher, trusting
			// whatever the user has given us.
			const float fWeight = (i == 0) ? k_flExactWordFrequencyWeight : 1.0;

			SuggestCandidate_t OutCandidate;
			V_strcpy_safe( OutCandidate.rgch, InCandidate.rgch );
			OutCandidate.probability = InCandidate.probability * fWeight;
			OutCandidate.strLen = unStringLength;
			OutCandidate.bWasRootWithNoTypoCorrection = (i == 0);
				
			vecCandidates.AddToTail( OutCandidate );

			dictSeenWords.Insert( InCandidate.rgch );
		}
	}

	// We're going to look for the most probable and the longest words that come from our no-typo suggestion
	// set and slam them into the first and second suggestion slot. If we have a garbage string here we may
	// not find any and so may not make changes here, which is fine.
	if ( vecCandidates.Count() > 0 )
	{
		vecCandidates.Sort( SuggestCandidateLessFunc );

		const auto funcMoveBestSuggestionToHead = [&]( bool (*funcPred)(const SuggestCandidate_t&, const SuggestCandidate_t&) )
		{
			const SuggestCandidate_t *pBest = std::max_element( vecCandidates.begin(), vecCandidates.end(), funcPred );
			Assert( pBest );

			if ( pBest->bWasRootWithNoTypoCorrection )
			{
				const SuggestCandidate_t copy = *pBest;

				const auto unBestIndex = pBest - vecCandidates.Base();
				Assert( vecCandidates.IsValidIndex( unBestIndex ) );

				vecCandidates.Remove( unBestIndex );
				vecCandidates.AddToHead( copy );
			}
		};

		funcMoveBestSuggestionToHead( [] ( const SuggestCandidate_t& a, const SuggestCandidate_t& b ) { return a.GetStringLenghtForHeadSort() < b.GetStringLenghtForHeadSort(); } );
		funcMoveBestSuggestionToHead( [] ( const SuggestCandidate_t& a, const SuggestCandidate_t& b ) { return a.GetProbabilityForHeadSort() < b.GetProbabilityForHeadSort(); } );
	}

	// Copy over just the string data to our result vector. This is potentially horribly inefficient as
	// we're a straight vector of strings, but our arrays are small and we can move-construct so we
	// don't really care.
	out_pvecResults->EnsureCapacity( unMaxSuggestionCount );

	for ( const SuggestCandidate_t& Candidate : vecCandidates )
	{
		out_pvecResults->AddToTail( Candidate.rgch );
		if ( static_cast<unsigned int>( out_pvecResults->Count() ) > unMaxSuggestionCount )
			break;
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTextInputDualTouch::UpdateSuggestionWords()
{
	CUtlVector< CUtlString > vecSuggestions;

	// If we have any word roots (user has typed any characters since the last suggestion buffer clear),
	// use them to try to generate candidate suggestion words. This result set may be empty if we only
	// have one bad root.
	if ( m_PossibleWordsBeingTyped.Count() > 0 )
	{
		GenerateSuggestionCandidates( m_pSuggest, k_SuggestionCount, *m_PossibleWordsBeingTyped.Top(), &vecSuggestions );
	}

	// Do we have text in our clipboard? If we do, shove the contents of the clipboard into the first
	// suggestion always.
	int iSuggestionsFilled = 0;

	{
		CUtlString sClipboard, sPasteStringLocToken;
		UIEngine()->GetClipboardText( sClipboard, &sPasteStringLocToken );

		if ( !sClipboard.IsEmpty() )
		{
			m_pSuggestionLabels[iSuggestionsFilled]->GetParent()->AddClass( k_pchPasteSuggestionPanel );
			m_pSuggestionLabels[iSuggestionsFilled]->SetText( sPasteStringLocToken.Get() );
			iSuggestionsFilled++;
		}
	}

	// Fill in any remaining suggestion UI elements with our candidates in priority order.
	{
		int iCandidate = 0;
		while ( iSuggestionsFilled < k_SuggestionCount )
		{
			m_pSuggestionLabels[iSuggestionsFilled]->GetParent()->RemoveClass( k_pchPasteSuggestionPanel );
			m_pSuggestionLabels[iSuggestionsFilled]->SetText( vecSuggestions.IsValidIndex( iCandidate ) ? vecSuggestions[iCandidate].Get() : "" );
			iCandidate++;
			iSuggestionsFilled++;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTextInputDualTouch::OnSuggestionSelected( int iSuggestion )
{
	Assert( iSuggestion >= 0 );
	Assert( iSuggestion < k_SuggestionCount );
	Assert( m_pSuggestionLabels[ iSuggestion ] );

	// For normal suggestion panels, the label on the panel is the text we want to shove in. If the
	// panel the user selected is "paste", then the label text will be more descriptive (ie.,
	// "[Paste CD Key]") but not the text we want to put into the buffer.

	// Figure out what text we want to shove into the text buffer based on the properties of the
	// suggestion panel.
	const auto *pSelectedSuggestionLabel = m_pSuggestionLabels[ iSuggestion ];
	const bool bUseClipboardInsteadOfInlineText = pSelectedSuggestionLabel->GetParent()->BHasClass( k_pchPasteSuggestionPanel );

	CUtlString sClipboardText;
	UIEngine()->GetClipboardText( sClipboardText, nullptr );
	CStrAutoEncode s( bUseClipboardInsteadOfInlineText ? sClipboardText.Get() : pSelectedSuggestionLabel->PchGetText() );
	const uchar32 *psz32 = s.ToUTF32();

	// If we did paste text in, clear out the clipboard once its done. This might be annoying for users,
	// but most of the use cases in BP are for things like "paste in a CD key I just copied" or "paste a URL
	// to a friend".
	//
	// If this is annoying then we need to add custom UI elements to clear the clipboard somewhere or you'll
	// lose access to a suggestion candidate box forever.
	if ( bUseClipboardInsteadOfInlineText )
	{
		UIEngine()->ClearClipboard();
	}

	// Because we may have typos in the buffer that don't match the prefix of the suggestion string we
	// want to put in, we just nuke the entire string we typed and then replace the contents. We could
	// be smarter here and only remove characters if they mismatch.
	const int iCharCount = m_PossibleWordsBeingTyped.Count();
	for ( int i = 0; i < iCharCount; i++ )
	{
		PerformBackspace();
	}
	
	// FIXME: when we stop doing uchar32 stuff, we can just send the UTF8 directly here.
	m_pTextInputControl->InsertCharactersAtCursor( psz32, V_strlen32( psz32 ) );
	
	ResetSuggestionState();
	UpdateTextPreview();
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTextInputDualTouch::ResetSuggestionState()
{
	m_PossibleWordsBeingTyped.PurgeAndDeleteElements();
	UpdateSuggestionWords();
}


//-----------------------------------------------------------------------------
// Switches among recently used input languages
//-----------------------------------------------------------------------------
bool CTextInputDualTouch::SwitchLanguage( void )
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
			// TODO: read user store on enabled languages on SteamOS or otherwise let people
			// alter the result of this call so that it doesn't switch through ALL languages
			// when logged in
			if ( LoadInputConfigurationFile( languageNew ) )
			{
				ApplyCurrentModifierLayout();
				UIEngine()->SetInputLocale( languageNew );
				break;
			}
		}
	}
	while ( languageNew != languageOld );

	// reflect the new language in the UI
	if ( m_pLang )
	{
		m_pLang->SetText( GetPanoramaLocalizedLanguageShortName( m_language ) );
	}
#endif

	return false;
}

//-----------------------------------------------------------------------------
// Loads configuration file by language ID
//-----------------------------------------------------------------------------
bool CTextInputDualTouch::LoadInputConfigurationFile( ELanguage language )
{
	CUtlString sConfig;
	sConfig.Format( "layout_%s_dualtouch.txt", GetLanguageShortName( language ) );
	
	bool bLoaded = LoadInputConfigurationFile( sConfig.String(), UIEngine()->GetLocalPathForNamedPath( "{keyboards}" ) );
	if ( bLoaded )
		m_language = language;
	
	// try to load suggestion info, if appropriate (i.e. not when entering a password)
	SAFE_DELETE( m_pSuggest );
	
	if ( m_eSuggestionMode != k_EDualtouchSuggestionMode_NoSuggestions )
	{
		m_pSuggest = CreateInputSuggest( language );
	}
	
	return bLoaded;
}


//-----------------------------------------------------------------------------
// Purpose: return the list of languages we have configs for
//-----------------------------------------------------------------------------
void CTextInputDualTouch::GetSupportedLanguages( CUtlVector<ELanguage> &vecLangs )
{
	ELanguage language = k_Lang_English;
	do
	{
		if ( UIEngine()->BHaveInputLocale( language ) )
		{
			CUtlString sConfig;
			sConfig.Format( "layout_%s_dualtouch.txt", GetLanguageShortName( language ) );
			CPathString strPath( sConfig, UIEngine()->GetLocalPathForNamedPath( "{keyboards}" ) );
			if ( BFileExists( strPath.GetUTF8Path() ) )
			{
				vecLangs.AddToTail( language );
			}
		}
		language = (ELanguage)((int)language + 1 );
	} while ( language != k_Lang_MAX );
}


//-----------------------------------------------------------------------------
// Loads configuration file, applies the first config from the configuration
//	file and deallocates previous config, returns whether load succeeded
//	and new configuration was successfully set and activated
//-----------------------------------------------------------------------------
bool CTextInputDualTouch::LoadInputConfigurationFile( char const *szConfigFile, const char *szConfigRootDir )
{
	CPathString strPath( szConfigFile, szConfigRootDir );
	CUtlBuffer buffer;
	if ( !LoadFileIntoBuffer( strPath.GetUTF8Path(), buffer, true ) )
		return false;
	
	bool bResult = LoadConfigurationBuffer( (char const *)buffer.Base() );
	if ( bResult )
	{
// 		if ( !m_bRestrictConfig )
// 		{
// 			m_eConfigCurrent = BCursorAtStartOfSentence() ? k_EDaisyConfigCaps : k_EDaisyConfigLetters;
// 		}
// 		m_mapConfigEntries.Swap( mapConfigs );
// 		SetControlsFromConfiguration();
	}
	
	return true;
}


//-----------------------------------------------------------------------------
// Processes the buffer loaded from configuration buffer and allocates appropriate
//	configuration structures to hold names of configurations and layout of items
//	within each subconfiguration
//-----------------------------------------------------------------------------
bool CTextInputDualTouch::LoadConfigurationBuffer( char const *pszIncoming )
{
	// take it to uchar32 in one fell swoop, to avoid having to do any UTF-8 decoding below
	CStrAutoEncode str( pszIncoming, STRINGCONVERT_FAIL );
	
	// we happen to know we will own the whole buffer, so we don't need it to be const
	uchar32 *pch32Base = const_cast< uchar32* >( str.ToUTF32() );
	
	if ( pch32Base == NULL )
	{
		// failed conversion
		AssertMsg( false, "Failed to convert daisy wheel config from UTF-8" );
		return false;
	}
	
	// See if we have a BOM at the beginning of the buffer
	if ( *pch32Base == k_wchUnicodeBOM )
	{
		// We do; skip it
		pch32Base++;
	}
	
	int currentColumn = 0;
	int currentRow = 0;
	int currentModifier = 0;
	
	while ( *pch32Base != 0 )
	{
		if ( *pch32Base == L'\r' )
		{
			pch32Base++;
			continue;
		}
		else if ( *pch32Base == L'\n' )
		{
			currentRow++;
			currentColumn = 0;
			
			if (currentRow == 4)
			{
				currentRow = 0;
				currentModifier++;
			}
		}
		else
		{
			m_keyLayout[currentColumn][currentRow][currentModifier] = *pch32Base;
			currentColumn++;
		}
		
		pch32Base++;
	}
	
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: a key has been typed on a real keyboard; send it to the base text control
//-----------------------------------------------------------------------------
bool CTextInputDualTouch::OnKeyTyped( const KeyData_t &unichar )
{
	ResetSuggestionState();
	bool ret = m_pTextInputControl->OnKeyTyped( unichar ); 
	
	UpdateTextPreview();
	
	return ret;
}

//-----------------------------------------------------------------------------
//	a key has gone down
//-----------------------------------------------------------------------------
bool CTextInputDualTouch::OnKeyDown( const KeyData_t &code )
{ 
	if ( code.m_KeyCode == KEY_ENTER )
	{
		if ( BHasClass( k_pchDontCloseOnEnter ) )
		{
			SubmitTextNoClose();
		}
		else
		{
			CloseHandler( true );
		}
		return true;
	}
	else if ( code.m_KeyCode == KEY_ESCAPE )
	{
		CloseHandler( false );
		return true;
	}
	else
	{
		ResetSuggestionState();
		bool ret = m_pTextInputControl->OnKeyDown( code ); 
		
		UpdateTextPreview();
		
		return ret;
	}
}


//-----------------------------------------------------------------------------
//	a key has gone up
//-----------------------------------------------------------------------------
bool CTextInputDualTouch::OnKeyUp( const KeyData_t &code )
{ 
	return m_pTextInputControl->OnKeyUp( code ); 
}


//-----------------------------------------------------------------------------
// Listen for focus loss
//-----------------------------------------------------------------------------
bool CTextInputDualTouch::HandleInputFocusLost( const panorama::CPanelPtr< panorama::IUIPanel > &ptrPanel )
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
// Purpose: 
//-----------------------------------------------------------------------------
bool CTextInputDualTouch::EventInputFocusTopLevelChanged( CPanelPtr< IUIPanel > ptrPanel )
{
	// If we aren't the top level focus anymore, close
	//if ( ptrPanel != UIPanel() )

	if ( ToPanel2D( ptrPanel.Get() ) != this )
	{
		CancelOutstandingRepeats();
		CloseHandler( false );
		return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTextInputDualTouch::SetModifierKeyState( EDualTouchModifier_t modifier, bool bIsButtonPressEvent )
{
	if ( m_bModifierKeysHeld[modifier] == bIsButtonPressEvent )
		return;

	// For deactivating...
	const bool bWantsDeactivation = (m_iCharactersTypedSinceModifierStateChanged == 0 && m_currentModifier == modifier && bIsButtonPressEvent)		// If we haven't typed characters yet, but we pressed the button that controls our current state, we want to toggle it off.
								 || (m_iCharactersTypedSinceModifierStateChanged > 0 && !bIsButtonPressEvent);										// If we have typed characters and we're letting go...

	// For activating...
	const bool bWantsActivation = (m_currentModifier != modifier && bIsButtonPressEvent);

	// This looks weird, but it's possible to have a key down event that we want to interpret as "toggle this modifier *off*". For
	// all other cases we track the state of the button directly.
	m_bModifierKeysHeld[modifier] = bWantsDeactivation ? false : bIsButtonPressEvent;

	if ( bWantsDeactivation || bWantsActivation )
	{
		ApplyCurrentModifierLayout();
		m_iCharactersTypedSinceModifierStateChanged = 0;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTextInputDualTouch::EDualTouchModifier_t CTextInputDualTouch::CalculateDesiredModifierState() const
{
	static_assert( k_EDualTouchModifierNone == 0, "we're starting loop iteration at 1 below" );
	for ( int i = 1; i < V_ARRAYSIZE( m_bModifierKeysHeld ); i++ )
	{
		if ( m_bModifierKeysHeld[i] )
			return static_cast<EDualTouchModifier_t>( i );
	}

	return k_EDualTouchModifierNone;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTextInputDualTouch::ApplyCurrentModifierLayout()
{
	// TODO highlight appropriate touchkeys, implement caps lock, etc

	m_currentModifier = CalculateDesiredModifierState();

	CUtlVector< IUIPanel * > pTouchKeys;
	int x, y;
	
	uchar32 UTF32LabelChar[2] = { 0, 0 };
	char UTF8Label[32];
	
	UIPanel()->FindChildrenWithClassTraverse( "TouchKey", pTouchKeys );
	
	FOR_EACH_VEC(pTouchKeys, i)
	{
		if ( sscanf( pTouchKeys[i]->GetID(), "TouchKey_%i_%i", &x, &y) == 2 )
		{
			CLabel *pKeyLabel = (CLabel *)ToPanel2D(pTouchKeys[i]->FindChild("TouchKeyLabel"));
			
			if ( pKeyLabel )
			{
				UTF32LabelChar[0] = m_keyLayout[x][y][m_currentModifier];				

				UTF8Label[V_UTF32ToUTF8( UTF32LabelChar, UTF8Label, sizeof( UTF8Label ))] = 0;
				pKeyLabel->SetText( UTF8Label, CLabel::k_ETextTypeUnlocalized );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: update classes for a controller type change
//-----------------------------------------------------------------------------
bool CTextInputDualTouch::OnActiveControllerTypeChanged( EActiveControllerType eActiveControllerType )
{
	OnActiveControllerTypeChangedDefaultHandler( UIPanel(), eActiveControllerType );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTextInputDualTouch::InitEmoticons( void )
{
	CUtlVector< IUIPanel * > pTouchKeys;
	UIPanel()->FindChildrenWithClassTraverse( "EmoticonTouchKey", pTouchKeys );
	
	// create an Image child for each potential emoticon
	FOR_EACH_VEC( pTouchKeys, i )
	{
		new CImagePanel( ToPanel2D( pTouchKeys[i] ), "EmoticonImage" );
	}

	m_nMaxEmoticonsPerPage = pTouchKeys.Count();

	PopulateEmoticonsForCurrentPage();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTextInputDualTouch::PopulateEmoticonsForCurrentPage( void )
{
#if !defined( SOURCE2_PANORAMA ) && !defined( PANORAMA_PUBLIC_STEAM_SDK )
	int cEmoticons = ClientFriends()->GetEmoticonCount();

	int nFirstEmoticonOnPage = m_nEmoticonPage * m_nMaxEmoticonsPerPage;

	if ( nFirstEmoticonOnPage > cEmoticons )
	{
		Assert( !"How did we get to this page, we don't have this many emoticons" );
		nFirstEmoticonOnPage = 0;
	}

	static EUniverse eUniverse = ClientUtils()->GetConnectedUniverse();
	char rgchL[k_cchLanguageNameLen];
	ClientUser()->GetLanguage( rgchL, sizeof( rgchL ) );

	m_mapEmoticonTouchKeys.RemoveAll();

	CUtlVector< IUIPanel * > pTouchKeys;
	UIPanel()->FindChildrenWithClassTraverse( "EmoticonTouchKey", pTouchKeys );
	FOR_EACH_VEC( pTouchKeys, i )
	{
		int x, y;
		sscanf( ToPanel2D( pTouchKeys[i] )->GetID(), "EmoticonTouchKey_%i_%i", &x, &y );
		int nEmoticonKey = y + x * 10;

		int nEmoticonIndex = nFirstEmoticonOnPage + i;
		if ( nEmoticonIndex < cEmoticons )
		{
			CUtlString sEmoticon = ClientFriends()->GetEmoticonName( nEmoticonIndex );
			sEmoticon.Replace( ":", "" );
			CUtlString sURL;
			GetCommunityCDNURLForUniverse( eUniverse, sURL );
			sURL.AppendFormat( "economy/emoticon/%s", sEmoticon.Get() );

			// upgrade to this after /emoticonlarge makes its way to public web
			//sURL.AppendFormat( "economy/emoticonlarge/%s", sEmoticon.Get() );

			CImagePanel *pImage = assert_cast<CImagePanel *>( ToPanel2D( pTouchKeys[i] )->FindChild("EmoticonImage") );
			if ( pImage )
			{
				pImage->SetImage( sURL );
			}

			pTouchKeys[i]->RemoveClass( "Hidden" );

			m_mapEmoticonTouchKeys.Insert( nEmoticonKey, nEmoticonIndex );
		}
		else
		{
			pTouchKeys[i]->AddClass( "Hidden" );
		}
	}

#endif
}


//-----------------------------------------------------------------------------
// Purpose: toggle emoticon selector visibility
//-----------------------------------------------------------------------------
void CTextInputDualTouch::SetEmoticonMode( bool bActive )
{
	SetHasClass( "EmoticonMode", bActive );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTextInputDualTouch::OnEmoticonClicked( CPanel2D *pTouchKey )
{
#if !defined( SOURCE2_PANORAMA ) && !defined( PANORAMA_PUBLIC_STEAM_SDK )

	Assert( pTouchKey );

	int x, y;
	sscanf( pTouchKey->GetID(), "EmoticonTouchKey_%i_%i", &x, &y );

	int nEmoticonKey = y + x * 10;

	int nIndex = m_mapEmoticonTouchKeys.Find( nEmoticonKey );
	if ( nIndex != m_mapEmoticonTouchKeys.InvalidIndex() )
	{
		int nEmoticonIndex = m_mapEmoticonTouchKeys.Element( nIndex );
		Assert( nEmoticonIndex >= 0 && nEmoticonIndex < ClientFriends()->GetEmoticonCount() );
		
		CUtlString sEmoticon = ClientFriends()->GetEmoticonName( nEmoticonIndex );
		
		TypeCharacters( sEmoticon );
	}
#endif
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTextInputDualTouch::EmoticonPageLeft( void )
{
	if ( m_nEmoticonPage == 0 )
		return;

	m_nEmoticonPage--;
	PopulateEmoticonsForCurrentPage();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTextInputDualTouch::EmoticonPageRight( void )
{
#if !defined( SOURCE2_PANORAMA ) && !defined( PANORAMA_PUBLIC_STEAM_SDK )
	int cEmoticons = ClientFriends()->GetEmoticonCount();

	int nMaxPage = ceil( (double)cEmoticons / (double)m_nMaxEmoticonsPerPage ) - 1;

	if ( m_nEmoticonPage == nMaxPage )
		return;

	m_nEmoticonPage++;
	PopulateEmoticonsForCurrentPage();
#endif
}
