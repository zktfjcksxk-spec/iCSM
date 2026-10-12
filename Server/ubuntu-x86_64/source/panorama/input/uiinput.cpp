//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx.h"
#include "uiinput.h"
#include "tier1/characterset.h"
#include "panorama/panoramacurves.h"
#include "panorama/iuisoundsystem.h"
#include "panorama/controls/panel2d.h"
#include "panorama/uijsregistration.h"

#ifndef SOURCE2_PANORAMA
#include "controller/trackpad_weightedfilter.h"
#include <vrapi.h>
#endif

#ifdef SOURCE2_PANORAMA
#include "filesystem_helpers.h"
#include "enumutils_panorama.h"
#include "inputsystem/iinputsystem.h"
#else
#if defined( LINUX )
#include "../../common/linuxhelpers.h"
#elif defined( OSX )
#include <SDL2/SDL.h>
#endif
#include "enumutils.h"
#endif // else SOURCE2_PANORAMA

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

#ifdef SOURCE2_PANORAMA
//-----------------------------------------------------------------------------
// Purpose: Returns a value in the range [fToMin,fToMax] based on fT's value in
// the range [fFromMin,fFromMax]. Examples:
// 
// MapRange( 3,  0, 10,  0, 1000 ) = 300
// MapRange( 100,  0, 200,  0, 10 ) = 5 
//----------------------------------------------------------------------------- 

template< typename F, typename T >
static T MapRange( F fT, F fFromMin, F fFromMax, T fToMin, T fToMax )
{
	if ( fFromMin == fToMin && fFromMax == fToMax )
		return fT;

	float fN = float( fT - fFromMin ) / float( fFromMax - fFromMin );

	fN = clamp( fN, 0.0f, 1.0f );

	return fToMin + fN*(fToMax - fToMin);
}
#endif

using namespace panorama;

namespace panorama
{
	extern bool BIsGamePadCodeEquivalentIgnoringVendor( GamePadCode a, GamePadCode b );
}


ConVar g_ConVarDragScrollAffordance( "@panorama_dragscroll_affordance", "20", 0, "Minimum mouse movement in pixels before a move is treated as a drag scroll" );
ConVar g_ConVarDragScrollMinTime( "@panorama_dragscroll_mintime", "0.02", 0, "Minimum time that the mouse button must be down before a move is treated as a drag scroll" );
ConVar g_ConVarDragScrollVelocityMultiplier( "@panorama_dragscroll_velocitymultiplier", "0.5", 0, "Multiplier for flick velocity off of actual measured velocity" );

ConVar g_ConVarDragScrollAffordanceVR( "@panorama_dragscroll_affordance_vr", "100", 0, "Minimum mouse movement in pixels before a move is treated as a drag scroll in VR"  );
ConVar g_ConVarDragScrollMinTimeVR( "@panorama_dragscroll_mintime_vr", "0.1", 0, "Minimum time that the mouse button must be down before a move is treated as a drag scroll in VR" );
ConVar g_ConVarDragScrollVelocityMultiplierVR( "@panorama_dragscroll_velocitymultiplier_vr", "0.5", 0, "Multiplier for flick velocity off of actual measured velocity" );

static ConVar s_convarPanoramaInputDebugInfo( "@panorama_input_debug_info", "0", FCVAR_DEVELOPMENTONLY, "" );

#define MOVE_REPEAT_INTERVAL_START 0.22f
#define MOVE_REPEAT_INTERVAL_END 0.05f
#define MOVE_REPEAT_CURVE_TIME 1.0f

#define MOUSE_MOVE_ACTIVE_COUNT 3


ENUMSTRINGS_START( EAttachedHardwareDevice )
{ k_AttachedHardwareDevice_MouseKeyboard, "MouseKBPresent" },
{ k_AttachedHardwareDevice_SteamController, "SteamControllerPresent" },
{ k_AttachedHardwareDevice_XInput, "XInputControllerPresent" },
{ k_AttachedHardwareDevice_HTCVive, "HTCVivePresent" },
{ k_AttachedHardwareDevice_OculusDK1, "OculusDK1Present" },
{ k_AttachedHardwareDevice_OculusDK2, "OculusDK2Present" },
{ k_AttachedHardwareDevice_OculusCV1, "OculusCV1Present" },
{ k_AttachedHardwareDevice_HTCMotionController, "HTCMotionControllerPresent" },
{ k_AttachedHardwareDevice_OculusTouchController, "OculusTouchControllerPresent" },
{ k_Dummy_AttachedHardwareDeviceCount, "DummyHardware" },
ENUMSTRINGS_REVERSE( EAttachedHardwareDevice, k_Dummy_AttachedHardwareDeviceCount )


ENUMSTRINGS_START( EActiveControllerType )
{ k_EActiveControllerType_None, "None" },
{ k_EActiveControllerType_KBMouse, "KBMouse" },
{ k_EActiveControllerType_XInput, "XInput" },
{ k_EActiveControllerType_Steam, "Steam" },
{ k_EActiveControllerType_VR, "VR" },
ENUMSTRINGS_REVERSE( EActiveControllerType, k_EActiveControllerType_None )


InputAction_t::InputAction_t( const InputAction_t &rhs )
{
	m_symUIEventName = rhs.m_symUIEventName;
	if( !rhs.m_pJSAction.IsEmpty() )
	{
		v8::Isolate::Scope isolate_scope( UIEngineInternal()->GetV8Isolate() );
		v8::HandleScope handle_scope( UIEngineInternal()->GetV8Isolate() );

		v8::Local<v8::Function> fnLocal = v8::Local<v8::Function>::New( UIEngineInternal()->GetV8Isolate(), rhs.m_pJSAction );
		m_pJSAction.Reset( UIEngineInternal()->GetV8Isolate(), fnLocal );
		m_pJSActionContextPanel = rhs.m_pJSActionContextPanel;
	}
	m_strParams = rhs.m_strParams;
}

InputAction_t &InputAction_t::operator = (const InputAction_t rhs)
{
	m_symUIEventName = rhs.m_symUIEventName;
	if( !rhs.m_pJSAction.IsEmpty() )
	{
		v8::Isolate::Scope isolate_scope( UIEngineInternal()->GetV8Isolate() );
		v8::HandleScope handle_scope( UIEngineInternal()->GetV8Isolate() );

		v8::Local<v8::Function> fnLocal = v8::Local<v8::Function>::New( UIEngineInternal()->GetV8Isolate(), rhs.m_pJSAction );
		m_pJSAction.Reset( UIEngineInternal()->GetV8Isolate(), fnLocal );
		m_pJSActionContextPanel = rhs.m_pJSActionContextPanel;
	}
	m_strParams = rhs.m_strParams;
	return *this;
}

InputAction_t::~InputAction_t()
{
	if( !m_pJSAction.IsEmpty() )
	{
		m_pJSAction.Reset();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Helper for parsing modifier values out of key binding files
//-----------------------------------------------------------------------------
uint32 ParseModifier( const char * pchModifier, const char **pchRemaining )
{
	uint32 unModifiers = 0;

	while( 1 )
	{
		pchModifier = CSSHelpers::SkipSpaces( pchModifier );
		if ( V_strnicmp( pchModifier, "mod_shift", V_strlen( "mod_shift" ) ) == 0 )
		{
			pchModifier += V_strlen( "mod_shift" );
			unModifiers = unModifiers | MODIFIER_LSHIFT | MODIFIER_RSHIFT;
		}
		else if ( V_strnicmp( pchModifier, "mod_alt", V_strlen( "mod_alt" ) ) == 0 )
		{
			pchModifier += V_strlen( "mod_alt" );
			unModifiers = unModifiers | MODIFIER_LALT | MODIFIER_RALT;
		}
		else if ( V_strnicmp( pchModifier, "mod_ctrl", V_strlen( "mod_ctrl" ) ) == 0 )
		{
			pchModifier += V_strlen( "mod_ctrl" );
			unModifiers = unModifiers | MODIFIER_LCONTROL | MODIFIER_RCONTROL;
		}
		else
		{
			break;
		}

		pchModifier = CSSHelpers::SkipSpaces( pchModifier );
		if ( pchModifier[0] == '+' )
			++pchModifier;
		pchModifier = CSSHelpers::SkipSpaces( pchModifier );
	}

	if ( pchRemaining )
		*pchRemaining = pchModifier;

	return unModifiers;
}


//-----------------------------------------------------------------------------
// Purpose: Helper for parsing out parameters to keybound events
//-----------------------------------------------------------------------------
const char* ParseEventParameters( const char *pchParse, InputAction_t &action )
{
	const char *pchParamStart = CSSHelpers::SkipSpaces( pchParse );
	if ( *pchParamStart != '(' )
		return pchParse;

	const char* pchParamsEnd = strchr( pchParamStart, ')' ) + 1;
	if ( !pchParse )
		return pchParse;

	size_t nCharsToSkip = pchParamsEnd - pchParamStart; // Keep open and close parens
	action.m_strParams.SetDirect( pchParamStart, nCharsToSkip );

	return pchParamsEnd;
}


//-----------------------------------------------------------------------------
// Purpose: Helper for parsing out a namespace qualifier on a keybind entry
//-----------------------------------------------------------------------------
CUtlString ParseNamespace( const char * pchModifier, const char **pchRemaining )
{
	const char *pchNamespaceEnd = strchr( pchModifier, '#' );
	if ( !pchNamespaceEnd )
		return (char *)NULL;

	if ( pchRemaining )
		*pchRemaining = CSSHelpers::SkipSpaces( pchNamespaceEnd + 1 );

	CUtlString sRet;
	sRet.SetDirect( pchModifier, pchNamespaceEnd - pchModifier );
	return sRet;
}


//-----------------------------------------------------------------------------
// Purpose: Returns panel or first ancestor that is draggable
//-----------------------------------------------------------------------------
IUIPanel *GetDraggablePanel( IUIPanel *pPanel )
{
	for ( IUIPanel *pCurrent = pPanel; pCurrent != NULL; pCurrent = pCurrent->GetParent() )
	{
		if ( pCurrent->IsDraggable() )
			return pCurrent;
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Returns panel or first ancestor that is drag scrollable
//-----------------------------------------------------------------------------
IUIPanel *GetDragScrollablePanel( IUIPanel *pPanel )
{
	for ( IUIPanel *pCurrent = pPanel; pCurrent != NULL; pCurrent = pCurrent->GetParent() )
	{
		if ( pCurrent->BCanDragScroll() )
			return pCurrent;
	}

	return NULL;
}



// storage for the action code name helpers
namespace panorama
{
	CPanoramaSymbol ACTION_UP( "MoveUp" );
	CPanoramaSymbol ACTION_DOWN( "MoveDown" );
	CPanoramaSymbol ACTION_LEFT( "MoveLeft" );
	CPanoramaSymbol ACTION_RIGHT( "MoveRight" );
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
#pragma warning( push )
//  warning C4355: 'this' : used in base member initializer list
#pragma warning( disable : 4355 )
#if !defined(NO_STEAM)
CUIInputEngine::CUIInputEngine() : m_GamePadController( this ), m_SteamController( this ), m_eLastActiveControllerType( k_EActiveControllerType_None ), m_eForceActiveControllerType( k_EActiveControllerType_None )
#else
CUIInputEngine::CUIInputEngine() : m_GamePadController( this ), m_eLastActiveControllerType( k_EActiveControllerType_None ), m_eForceActiveControllerType( k_EActiveControllerType_None )
#endif
#pragma warning( pop )
{
	m_bSawControllerInputThisFrame = false;
	// Default keybinding files
	m_vecKeyBindingsFilePaths.AddToTail( "file://{resources}/default_keybinds.cfg" );
	m_vecKeyBindingsFilePaths.AddToTail( "file://{resources}/window_keybinds.cfg" );
	//m_vecKeyBindingsFilePaths.AddToTail( "file://{resources}/keybinds.cfg" ); // CSGO does not have this file
	m_flVRControllerLastInputTime = 0.0f;
	m_bDebugHitTest = false;
#ifdef SOURCE2_PANORAMA
	m_bIMEAllowed = true;
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Helper function to call a function/lambda on each IInputCapture in
// a way that's safe in case the vector changes midway through
//-----------------------------------------------------------------------------
template < typename T >
bool CallInputCaptures( CUtlVector< IInputCapture * > &vecInputCaptures, const T &fn )
{
	// This list of captures might completely change out from under us, so every time we try a fresh iteration
	// through it, continuing to do so until we've called all the captures within it.
	CUtlVector<IInputCapture *> vecCalledInputCaptures;
	bool bAnyValidCaptures = true;
	while ( bAnyValidCaptures )
	{
		bAnyValidCaptures = false;
		for ( int i = vecInputCaptures.Count() - 1; i >= 0; i = Min( i - 1, vecInputCaptures.Count() - 1 ) )
		{
			IInputCapture *pCapture = vecInputCaptures[ i ];
			if ( !vecCalledInputCaptures.HasElement( pCapture ) )
			{
				if ( fn( pCapture ) )
					return true;

				vecCalledInputCaptures.AddToTail( pCapture );
				bAnyValidCaptures = true;
				break;
			}
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Checks if the specified panel supports input. If not, finds parent who does
//-----------------------------------------------------------------------------
template < class T >
bool CUIWindowInput::BSendInput( IUIPanel *pPanel, const T &data, bool (IInputCapture::*pCaptureFunc)( IUIPanel *pPanel, const T &data), bool (IUIPanelClient::*pPanelFunc)(const T &data), bool bIgnoreAcceptsInput )
{
	// Check top level hooks first, they hook all input globally...
	CUtlVector<IInputCapture *> &vecInputCaptures = m_pParent->GetInputCapture();
	if ( vecInputCaptures.Count() > 0 )
	{
		CPanelPtr< IUIPanel > hPanel = pPanel;

		bool bHandledByCapture = CallInputCaptures( vecInputCaptures, [ & ]( IInputCapture *pCapture )
		{
			return ( pCapture->*pCaptureFunc )( pPanel, data );
		} );

		if ( bHandledByCapture )
			return true;

		// Check for deletion by the input capture of our focus window
		if ( !hPanel.Get() )
			return false;
	}

	if ( !pPanel )
		return false;

	// loop from panel with focus to parent, passing input to each
	for ( ; pPanel != NULL; pPanel = pPanel->GetParent() )
	{
		if ( !bIgnoreAcceptsInput && !pPanel->BAcceptsInput() )
			continue;

		// see if anyone has hooked input for this panel
		CUtlVector< IInputCapture * > *pvecHooks = GetHooksForPanel( pPanel );
		if ( pvecHooks )
		{
			FOR_EACH_VEC( *pvecHooks, i )
			{
				IInputCapture *pInputCapture = pvecHooks->Element( i );
				if ( (pInputCapture->*pCaptureFunc)( pPanel, data ) )
					return true;
			}
		}

		// hooks didn't handle input.. pass to panel
		if ( (pPanel->ClientPtr()->*pPanelFunc)( data ) )
			return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Finds panels hooking input for specified panel
//-----------------------------------------------------------------------------
CUtlVector< IInputCapture * > *CUIWindowInput::GetHooksForPanel( IUIPanel *pPanel )
{
	CPanelPtr< IUIPanel > ptrPanel( pPanel );
	int iHook = m_mapHookPanelInput.Find( ptrPanel );
	if ( iHook == m_mapHookPanelInput.InvalidIndex() )
		return NULL;

	return m_mapHookPanelInput.Element( iHook );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CUIInputEngine::~CUIInputEngine()
{	
}



//-----------------------------------------------------------------------------
// Purpose: load our keybinds
//-----------------------------------------------------------------------------
void CUIInputEngine::Initialize( IUISettings *pSettings )
{
	for ( int i = 0; i < m_vecKeyBindingsFilePaths.Count(); ++i )
	{
		const CUtlString &strKeybindFilePath = m_vecKeyBindingsFilePaths[ i ];
		ParseKeyConfig( strKeybindFilePath.Get() );
	}

	m_GamePadController.Initialize( pSettings );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
static void SetWindowStylesForAttachedDevices( CTopLevelWindow *pWindow, const CUtlVector<EAttachedHardwareDevice>& vecDevices )
{
	Assert( vecDevices.Count() <= k_AttachedHardwareDeviceCount );

	// clear all the existing classes
	for ( int i = 1; i <= k_AttachedHardwareDeviceCount; i++ )
	{
		pWindow->RemoveClass( PchNameFromEAttachedHardwareDevice( (EAttachedHardwareDevice)i ) );
	}

	// now add back any devices we admit to
	FOR_EACH_VEC( vecDevices, i)
	{
		pWindow->AddClass( PchNameFromEAttachedHardwareDevice( vecDevices[i] ) );
	}

	// Generic "ControllerPresent" class if we don't care which type of controller someone is using.
	pWindow->SetHasClass( "ControllerPresent", vecDevices.HasElement( k_AttachedHardwareDevice_SteamController ) || vecDevices.HasElement( k_AttachedHardwareDevice_XInput ) );
}


//-----------------------------------------------------------------------------
// Purpose: do a think cycle
//-----------------------------------------------------------------------------
void CUIInputEngine::RunFrame()
{
	VPROF_BUDGET( "CUIInputEngine::RunFrame", VPROF_BUDGETGROUP_TENFOOT );
	m_GamePadController.RunFrame();
#if !defined(NO_STEAM)
	m_SteamController.RunFrame();
#endif

	// Active controller change?
	const auto ePreviousActiveControllerType = m_eLastActiveControllerType;
	m_eLastActiveControllerType = GetActiveControllerType();
	m_bSawControllerInputThisFrame = m_GamePadController.BHadGamepadInput();
#if !defined(NO_STEAM)
	m_bSawControllerInputThisFrame = m_bSawControllerInputThisFrame || m_SteamController.BHadGamepadInput();
#endif

	if ( m_eLastActiveControllerType != ePreviousActiveControllerType )
	{
		UIEngine()->DispatchEventAsync( 0.0f, ActiveControllerTypeChanged::MakeEvent( nullptr, m_eLastActiveControllerType ) );

#if !defined( SOURCE2_PANORAMA )
		FOR_EACH_VEC( m_vecFocusedInputWindows, i )
		{
			SetWindowStylesForAttachedDevices( m_vecFocusedInputWindows[ i ]->GetTopLevelWindow(), GetAttachedHardwareDevices() );
		}
#endif
	}
}



//-----------------------------------------------------------------------------
// Purpose: reload key binds
//-----------------------------------------------------------------------------
void CUIInputEngine::ReloadKeyBindings()
{
	m_ActionBinds.RemoveAll();

	for ( int i = 0; i < m_vecKeyBindingsFilePaths.Count(); ++i )
	{
		const CUtlString &strKeybindFilePath = m_vecKeyBindingsFilePaths[ i ];
		ParseKeyConfig( strKeybindFilePath.Get() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: reload key binds if needed
//-----------------------------------------------------------------------------
void CUIInputEngine::ReloadChangedFile( const char *pchFile )
{
	for ( const CUtlString &strKeybindFilePath : m_vecKeyBindingsFilePaths )
	{
		const char *pszFileName = V_UnqualifiedFileName( strKeybindFilePath.Get() );
		if ( !pszFileName || pszFileName[0] == '\0' )
			continue;

		if ( V_strstr( pchFile, pszFileName ) )
		{
			ReloadKeyBindings();
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Register an additional file for keybindings
//-----------------------------------------------------------------------------
void CUIInputEngine::RegisterKeyBindingsFile( const char *pszFilePath )
{
	for ( int i = 0; i < m_vecKeyBindingsFilePaths.Count(); ++i )
	{
		const CUtlString &strKeybindFilePath = m_vecKeyBindingsFilePaths[ i ];
		if ( V_strcmp( strKeybindFilePath.Get(), pszFilePath ) == 0 )
			return;
	}

	m_vecKeyBindingsFilePaths.AddToTail( pszFilePath );

	ParseKeyConfig( pszFilePath );
}

//-----------------------------------------------------------------------------
// Purpose: Add a key binding
//-----------------------------------------------------------------------------
bool CUIInputEngine::BRegisterKeyBind( const char *pchNamespace, const char *pchKeyToBind, const char *pchUIEvent, v8::Persistent<v8::Function> *pFunc, IUIPanel *pFuncContextPanel )
{
	return BRegisterKeyBindInternal( NULL, pchNamespace, pchKeyToBind, pchUIEvent, pFunc, pFuncContextPanel );
}


//-----------------------------------------------------------------------------
// Purpose: Add a key binding
//-----------------------------------------------------------------------------
bool CUIInputEngine::BRegisterKeyBind( IUIPanel *pPanel, const char *pchKeyToBind, const char *pchUIEvent, v8::Persistent<v8::Function> *pFunc, IUIPanel *pFuncContextPanel )
{
	return BRegisterKeyBindInternal( pPanel, NULL, pchKeyToBind, pchUIEvent, pFunc, pFuncContextPanel );
}


//-----------------------------------------------------------------------------
// Purpose: Add a key binding
//-----------------------------------------------------------------------------
bool CUIInputEngine::BRegisterKeyBindInternal( IUIPanel *pPanel, const char *pchNamespace, const char *pchKeyToBind, const char *pchUIEvent, v8::Persistent<v8::Function> *pFunc, IUIPanel *pFuncContextPanel )
{
	// Should never be called with both a uievent and js func as a callback
	Assert( !pchUIEvent || !pFunc );

	// Should never be called with both namespace and panel to bind on
	Assert( !pPanel || !pchNamespace );

	if( pchUIEvent && !UIEngine()->IsValidEventName( pchUIEvent ) )
	{
		AssertMsg1( false, "Invalid event %s passed to BRegisterKeyBind()", pchUIEvent );
		return false;
	}

	bool bKeyUp = false;
	const char *pchUp = V_stristr( pchKeyToBind, "(up)" );
	if( pchUp )
		bKeyUp = true;

	char *pchParen = (char*)V_stristr( pchKeyToBind, "(" );
	if( pchParen )
		pchParen[0] = 0;
	
	InputAction_t action;
	if( pchUIEvent )
		action.m_symUIEventName = pchUIEvent;

	if( pFunc )
	{
		v8::Local<v8::Function> fnLocal = v8::Local<v8::Function>::New( UIEngineInternal()->GetV8Isolate(), *pFunc );
		action.m_pJSAction.Reset( UIEngineInternal()->GetV8Isolate(), fnLocal );
		action.m_pJSActionContextPanel.Set( pFuncContextPanel );
	}

	const char *pchModifier = pchKeyToBind;
	uint32 unModifiers = ParseModifier( pchModifier, &pchModifier );
	KeyCode keyCode = KeyCodeFromName( pchModifier );

#if defined( SOURCE2_PANORAMA )
	typedef HashMapFunctor_t< panorama::ActionInput_t > ActionInputHashFunctor;
#else
	typedef HashFunctor< panorama::ActionInput_t > ActionInputHashFunctor;
#endif

	CUtlHashMap< panorama::ActionInput_t, InputAction_t, CDefEquals<panorama::ActionInput_t>, ActionInputHashFunctor > *pHashMap = &m_ActionBinds;
	if( pPanel )
	{
		int iPanel = m_MapPanelBindings.Find( pPanel );
		if( iPanel != m_MapPanelBindings.InvalidIndex() )
			pHashMap = m_MapPanelBindings[iPanel];
		else
		{
			pHashMap = new CUtlHashMap< panorama::ActionInput_t, InputAction_t, CDefEquals<panorama::ActionInput_t>, ActionInputHashFunctor >();
			m_MapPanelBindings.Insert( pPanel, pHashMap );
		}
	}

	bool bSuccess = true;
	if( keyCode != KEY_NONE )
	{
		pHashMap->InsertOrReplace( ActionInput_t( bKeyUp ? k_eKeyUp : k_eKeyDown, keyCode, unModifiers, pchNamespace ), action );
	}
	else
	{
		MouseCode mouseCode = MouseCodeFromName( pchModifier );
		if( mouseCode != MOUSE_INVALID )
		{
			pHashMap->InsertOrReplace( ActionInput_t( bKeyUp ? k_eMouseUp : k_eMouseDown, mouseCode, pchNamespace ), action );
		}
		else
		{
			GamePadCode gamepadCode = GamePadCodeFromName( pchModifier );
			if( gamepadCode != XK_NULL )
			{
				pHashMap->InsertOrReplace( ActionInput_t( bKeyUp ? k_eGamePadUp : k_eGamePadDown, gamepadCode, pchNamespace ), action );
			}
			else
			{
				bSuccess = false;
			}
		}
	}

	if( pchParen )
		pchParen[0] = '(';


	return bSuccess;
}


//-----------------------------------------------------------------------------
// Purpose: Notice that a panel is deleted
//-----------------------------------------------------------------------------
void CUIInputEngine::OnPanelDeleted( IUIPanel *pPanel )
{
	int iPanelMap = m_MapPanelBindings.Find( pPanel );
	if( iPanelMap != m_MapPanelBindings.InvalidIndex() )
	{
		CUtlHashMap< ActionInput_t, InputAction_t > *pMap = m_MapPanelBindings[iPanelMap];
		FOR_EACH_HASHMAP( *pMap, i )
		{
			pMap->Element( i ).m_pJSAction.Reset();
		}
		delete m_MapPanelBindings[iPanelMap];
	}
}


//-----------------------------------------------------------------------------
// Purpose: Check if two gamepad codes are the 'same' button but on different vendor devices
//-----------------------------------------------------------------------------
bool CUIInputEngine::BIsGamePadCodeEquivalentIgnoringVendor( GamePadCode a, GamePadCode b )
{
	return panorama::BIsGamePadCodeEquivalentIgnoringVendor( a, b );
}


//-----------------------------------------------------------------------------
// Purpose: load a key bindings file from disk
//-----------------------------------------------------------------------------
void CUIInputEngine::ParseKeyConfig( const char *pchFileName )
{
	CFileResource fileResource( pchFileName );
	// read the file into memory
	CUtlBuffer buffer;

	bool bFailed = false;

#if DEVELOPMENT_ONLY
	// In development check for packed and signed panorama zip file,
	// if that file doesn't exist, then load from scattered files on local filesystem
	if ( !g_pFullFileSystem->FileExists( PANORAMA_ZIPFILE_NAME, NULL ) )
	{
		if ( !UIEngine()->UIFileSystem()->LoadFileIntoBuffer( fileResource.GetReferencePath().Get(), buffer, true ) )
		{
			bFailed = true;
		}
	}
	else
#endif
	{
		// remove file://{resources}/ from name ( which should be there )

		if ( strstr(pchFileName, "file://{resources}/") != pchFileName )
		{
			bFailed = true;
		}
		else
		{
			char fname[ MAX_PATH ];

			V_snprintf( fname, MAX_PATH, "panorama/%s", pchFileName + strlen( "file://{resources}/" ) );

			char * pContent = UIEngine()->UIFileSystem()->LoadFromPanZip( fname );
			
			if ( pContent )
			{
				buffer.PutString(pContent);
			}
			else
			{
				bFailed = true;
			}
		}
	}

	if ( !bFailed )
	{
		buffer.PutChar( 0 ); // make sure its' terminated

		// parse out the convar commands
		const char *pchParse = (const char *) buffer.Base();
		for (;;)
		{
			if ( !pchParse )
				break;

			char rgchT[1024];
			pchParse = ParseFile( pchParse, rgchT, sizeof( rgchT ), NULL, NULL );
			if ( !pchParse || !pchParse[0] )
				break;

			CUtlString sCmd = rgchT;

			bool bKeyUp = false;
			if ( pchParse[0] == '(' )
			{
				if ( V_stristr( pchParse, "(up)" ) == pchParse )
				{
					bKeyUp = true;
					pchParse += 4;
				}
				else if ( V_stristr( pchParse, "(down)" ) == pchParse )
				{
					bKeyUp = false;
					pchParse += 6;
				}
			}

			// read the type and code
			pchParse = ParseFile( pchParse, rgchT, sizeof( rgchT ), NULL, NULL );
			if ( !rgchT[0] )
				break;

			InputAction_t action;
			pchParse = ParseEventParameters( pchParse, action );
			if( UIEngine()->IsValidEventName( rgchT ) )
			{
				action.m_symUIEventName = rgchT;
			}
			else
			{
				// Try to parse as javascript now?  Not supported in .cfg file for now.
			}

			if( action.m_symUIEventName.IsValid() || !action.m_pJSAction.IsEmpty() )
			{

				const char *pchModifier = sCmd.String();
				CUtlString sNamespace = ParseNamespace( pchModifier, &pchModifier );
				uint32 unModifiers = ParseModifier( pchModifier, &pchModifier );
				KeyCode keyCode = KeyCodeFromName( pchModifier );
				if( keyCode != KEY_NONE )
				{
					m_ActionBinds.InsertOrReplace( ActionInput_t( bKeyUp ? k_eKeyUp : k_eKeyDown, keyCode, unModifiers, sNamespace ), action );
				}
				else
				{
					MouseCode mouseCode = MouseCodeFromName( pchModifier );
					if( mouseCode != MOUSE_INVALID )
					{
						m_ActionBinds.InsertOrReplace( ActionInput_t( bKeyUp ? k_eMouseUp : k_eMouseDown, mouseCode, sNamespace ), action );
					}
					else
					{
						GamePadCode gamepadCode = GamePadCodeFromName( pchModifier );
						if( gamepadCode != XK_NULL )
						{
							m_ActionBinds.InsertOrReplace( ActionInput_t( bKeyUp ? k_eGamePadUp : k_eGamePadDown, gamepadCode, sNamespace ), action );
						}
						else
						{
							UIEngine()->ShowNativeTopMostMessageBox( CFmtStr( "Invalid key %s in binds file %s", pchModifier, pchFileName ), "Invalid Key Bind", IUIEngine::k_ENativeMessageOk );
						}
					}
				}
			}
		}
	}
}



//-----------------------------------------------------------------------------
// Purpose: Handle a window shutting down, may need to clear our references to it
//-----------------------------------------------------------------------------
void CUIInputEngine::OnWindowShutdown( CTopLevelWindow *pWindow )
{
	FOR_EACH_VEC( m_vecFocusedInputWindows, i )
	{
		if ( m_vecFocusedInputWindows[ i ]->GetTopLevelWindow() == pWindow )
		{
			m_vecFocusedInputWindows.Remove( i );
			break;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Update the input time for the vr controller
//-----------------------------------------------------------------------------
void CUIInputEngine::UpdateInputTime( InputMessage_t &msg )
{
	if ( msg.m_GamePadData.m_GamePadCode >= VR_BUTTON_PRIMARY_APP && msg.m_GamePadData.m_GamePadCode < VR_BUTTON_LAST )
	{
		// VR Input, set the time on the UIInput
		m_flVRControllerLastInputTime = UIEngine()->GetCurrentFrameTime();
	}
}


//-----------------------------------------------------------------------------
// Purpose: an external system wants to feed in input to the active window
//-----------------------------------------------------------------------------
bool CUIInputEngine::InputEvent( InputMessage_t &msg )
{
	UpdateInputTime( msg );

	// We get analog gamepad input every frame, which sucks for detecting real new input, deal with it here.
	bool bNewInput = true;
	if ( msg.m_eInputType == k_eGamePadAnalog )
	{
		int iMap = m_MapAnalogValues.Find( msg.m_GamePadData.m_GamePadCode );
		if ( iMap == m_MapAnalogValues.InvalidIndex() )
		{
			AnalogData_t data;
			data.x = msg.m_GamePadData.m_fValueX;
			data.y = msg.m_GamePadData.m_fValueY;
			m_MapAnalogValues.Insert( msg.m_GamePadData.m_GamePadCode, data );
		}
		else
		{
			AnalogData_t &data = m_MapAnalogValues[iMap];
			if ( fabs( data.x - msg.m_GamePadData.m_fValueX ) < 1.0f && fabs( data.y - msg.m_GamePadData.m_fValueY ) < 1.0f )
			{
				bNewInput = false;
			}
			else
			{
				data.x = msg.m_GamePadData.m_fValueX;
				data.y = msg.m_GamePadData.m_fValueY;
			}
		}
	}
	
	FOR_EACH_VEC( m_vecFocusedInputWindows, i )
	{
		if ( !m_vecFocusedInputWindows[ i ]->BAllowInput( msg ) )
			continue;

		if ( m_vecFocusedInputWindows[ i ]->InputEvent( msg, bNewInput ) )
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: an external system wants to feed in input to the active window
//-----------------------------------------------------------------------------
bool CUIInputEngine::ActionEvent( InputAction_t action, EPanelEventSource_t eSource, int nRepeats )
{
	// ACTION_NONE does not fire an event
	if( !action.m_symUIEventName.IsValid() && action.m_pJSAction.IsEmpty() )
		return true;

	FOR_EACH_VEC( m_vecFocusedInputWindows, i )
	{
		if ( m_vecFocusedInputWindows[ i ]->ActionEvent( action, eSource, nRepeats ) )
			return true;
	}

	// create an event from the action code. Should have the same name.
	if( action.m_symUIEventName.IsValid() )
	{
		IUIEvent *pEvent = UIEngine()->CreateInputEventFromSymbol( action.m_symUIEventName, NULL, eSource, nRepeats );
		if( !pEvent )
			return false;

		UIEngine()->DispatchEvent( pEvent );
	}
	else
	{
		// bugbug jmccaskey - JS events cannot occur globally outside a panel context...
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: translate a steam or VR button into the XPad equivalent if it exists
//-----------------------------------------------------------------------------
GamePadCode EXKeyEquivalent( GamePadCode eGameCode )
{
	Assert( eGameCode > XK_BUTTON_LAST  && eGameCode < VR_BUTTON_LAST );
	switch ( eGameCode )
	{
	case STEAM_BUTTON_A:
	case VR_BUTTON_PRIMARY_TRIGGER:
		return XK_BUTTON_A;
	case STEAM_BUTTON_B:
	case VR_BUTTON_PRIMARY_GRIP:
		return XK_BUTTON_B;
	case STEAM_BUTTON_X:
		return XK_BUTTON_X;
	case STEAM_BUTTON_Y:
		return XK_BUTTON_Y;

	case STEAM_BUTTON_DPAD_LEFT:
	case VR_BUTTON_PRIMARY_LEFT:
		return XK_BUTTON_LEFT;
	case STEAM_BUTTON_DPAD_RIGHT:
	case VR_BUTTON_PRIMARY_RIGHT:
		return XK_BUTTON_RIGHT;
	case STEAM_BUTTON_DPAD_UP:
	case VR_BUTTON_PRIMARY_UP:
		return XK_BUTTON_UP;
	case STEAM_BUTTON_DPAD_DOWN:
	case VR_BUTTON_PRIMARY_DOWN:
		return XK_BUTTON_DOWN;

	case STEAM_BUTTON_GUIDE:
	case VR_BUTTON_PRIMARY_APP:
		return XK_BUTTON_GUIDE;

	case STEAM_BUTTON_LSHOULDER:
		return XK_BUTTON_LEFT_SHOULDER;
	case STEAM_BUTTON_RSHOULDER:
		return XK_BUTTON_RIGHT_SHOULDER;
	case STEAM_BUTTON_LTRIGGER:
		return XK_BUTTON_LTRIGGER;
	case STEAM_BUTTON_RTRIGGER:
		return XK_BUTTON_RTRIGGER;

	case STEAM_LEFTSTICK_UP:
		return XK_STICK1_UP;
	case STEAM_LEFTSTICK_DOWN:
		return XK_STICK1_DOWN;
	case STEAM_LEFTSTICK_LEFT:
		return XK_STICK1_LEFT;
	case STEAM_LEFTSTICK_RIGHT:
		return XK_STICK1_RIGHT;

	case STEAM_BUTTON_START:
		return XK_BUTTON_START;

	case STEAM_BUTTON_SELECT:
		return XK_BUTTON_BACK;

	default:
		return XK_NULL;
	}
}


//-----------------------------------------------------------------------------
// Purpose: find an action binding for this input
//-----------------------------------------------------------------------------
bool CUIInputEngine::FindActionBinding( CUtlHashMap< ActionInput_t, InputAction_t > *pHashMap, InputMessage_t msg, InputAction_t &actionOut, const char *pchNameSpace )
{
	int iBinding = pHashMap->Find( ActionInput_t( msg, pchNameSpace ) );
	if ( (msg.m_eInputType == k_eGamePadDown || msg.m_eInputType == k_eGamePadUp)
		&& iBinding == pHashMap->InvalidIndex() )
	{
		if ( msg.m_GamePadData.m_GamePadCode > XK_BUTTON_LAST )
		{ 
			// if we didn't find the code and it was a VR or Steam one, map back to basic Xpad ones to try
			GamePadCode eTranslatedGameCode = EXKeyEquivalent( msg.m_GamePadData.m_GamePadCode );
			if ( eTranslatedGameCode == XK_NULL )
			{
				return false;
			}
			else
			{
				msg.m_GamePadData.m_GamePadCode = eTranslatedGameCode;
				iBinding = pHashMap->Find( ActionInput_t( msg, pchNameSpace ) ); // look it up again
			}
		}
	}

	if ( iBinding != pHashMap->InvalidIndex() )
	{
		actionOut = pHashMap->Element( iBinding );
		return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: turn an input message into its corresponding action code from our bind list
//-----------------------------------------------------------------------------
void CUIInputEngine::TranslateInputEvent( const InputMessage_t &msg, InputAction_t &actionOut )
{
	actionOut.m_pJSAction.Reset();
	actionOut.m_pJSActionContextPanel.Clear();
	actionOut.m_symUIEventName = CPanoramaSymbol();

	// We don't treat left/right modifiers are independent in bindings, so normalize here to they will match 
	// correctly
	InputMessage_t msgNormalized = msg;
	if ( msgNormalized.m_eInputType == k_eKeyDown || msgNormalized.m_eInputType == k_eKeyUp )
	{
		if ( msgNormalized.m_KeyData.m_Modifiers != MODIFIER_NONE )
		{
			if ( msgNormalized.m_KeyData.m_Modifiers & MODIFIER_LSHIFT || msgNormalized.m_KeyData.m_Modifiers & MODIFIER_RSHIFT )
			{
				msgNormalized.m_KeyData.m_Modifiers |= MODIFIER_LSHIFT | MODIFIER_RSHIFT;
			}

			if ( msgNormalized.m_KeyData.m_Modifiers & MODIFIER_LALT || msgNormalized.m_KeyData.m_Modifiers & MODIFIER_RALT )
			{
				msgNormalized.m_KeyData.m_Modifiers |= MODIFIER_LALT | MODIFIER_RALT;
			}

			if ( msgNormalized.m_KeyData.m_Modifiers & MODIFIER_LCONTROL || msgNormalized.m_KeyData.m_Modifiers & MODIFIER_RCONTROL )
			{
				msgNormalized.m_KeyData.m_Modifiers |= MODIFIER_LCONTROL | MODIFIER_RCONTROL;
			}
		}
	}

	FOR_EACH_VEC( m_vecFocusedInputWindows, i )
	{
		if ( !m_vecFocusedInputWindows[ i ]->GetTopLevelWindow()->BIsVisible() )
			continue;

		IUIPanel *pPanel = m_vecFocusedInputWindows[ i ]->GetInputFocus();
		// walk up the panel hierarchy from the focus panel checking for namespaces
		while ( pPanel )
		{
			// Check bindings directly registered on panel first
			int iPanelMap = m_MapPanelBindings.Find( pPanel );
			if( iPanelMap != m_MapPanelBindings.InvalidIndex() )
			{
				if ( FindActionBinding( m_MapPanelBindings[iPanelMap] , msgNormalized, actionOut, nullptr ) )
				{
					return;
				}
			}

			// Then check bindings registered on the namespace for the panel globally
			const char *pchNamespace = pPanel->GetInputNamespace();
			if ( pchNamespace && pchNamespace[0] )
			{
				if ( FindActionBinding( &m_ActionBinds, msgNormalized, actionOut, pchNamespace ) )
				{
					return;
				}
			}

			if ( pPanel->BTopOfInputContext() )
				break;

			pPanel = pPanel->GetParent();
		}
	}

	FindActionBinding( &m_ActionBinds, msgNormalized, actionOut, nullptr );
}


//-----------------------------------------------------------------------------
// Purpose: lookup the game pad key bound to this event
//-----------------------------------------------------------------------------
const GamePadCode CUIInputEngine::GetGamePadBindForEvent( const char *pchEvent, const IUIPanel *pFromPanel )
{
	//bugbug jmccaskey - this is pretty fucked up with JS actions.

	EActiveControllerType eActiveController = GetActiveControllerType();

	int iFoundBound = -1; // best found so far
	bool bFoundNamespaceMatch = false;
	bool bFoundControllerMatch = false;
	
	FOR_EACH_HASHMAP( m_ActionBinds, i )
	{
		if( m_ActionBinds[i].m_symUIEventName != pchEvent )
			continue; // no event match
		
		const ActionInput_t &input = m_ActionBinds.Key( i );
		if( input.m_InputType != k_eGamePadDown && input.m_InputType != k_eGamePadUp )
			continue; // no input type match

		if( input.m_symNameSpace.IsValid() )
		{
			// see if this namespace specific one matches us at all
			const IUIPanel *pPanel = pFromPanel;
			while( pPanel )
			{
				const char *pchNamespace = pPanel->GetInputNamespace();
				if( pchNamespace && pchNamespace[0] )
				{
					if( input.m_symNameSpace == pchNamespace )
					{
						bFoundNamespaceMatch = true;
						iFoundBound = i;

						if( BIsGamePadCodeForController( input.m_Data.m_GamePadCode, eActiveController ) )
						{
							bFoundControllerMatch = true;
						}
						
						break; // stop looking for namespaces
					}
				}
				pPanel = pPanel->GetParent();
			}
		}
		else if( !bFoundNamespaceMatch )
		{
			// has no namespace match found yet, best so far
			if( !bFoundControllerMatch )
			{
				iFoundBound = i;

				if( BIsGamePadCodeForController( input.m_Data.m_GamePadCode, eActiveController ) )
				{
					bFoundControllerMatch = true;
				}
			}
		}
		
		if( bFoundNamespaceMatch && bFoundControllerMatch )
			break; // can't find any better entry
	}

	if ( iFoundBound != -1 )
	{
		// return best
		const ActionInput_t &input =  m_ActionBinds.Key(iFoundBound);
		return input.m_Data.m_GamePadCode;
	}

	return XK_NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Specifies input capture
//-----------------------------------------------------------------------------
void CUIInputEngine::SetInputCapture( IInputCapture *pCapture )
{
	m_vecInputCapture.AddToTail( pCapture );

	// if the mouse is already hovering over a panel, fake up a move as we aren't going to fire another panel move event
	// until the mouse leaves then enters the panel. Gets the listener of captured input in sync with the input system.
	IUIPanel *pPanelHover = NULL;

	FOR_EACH_VEC( m_vecFocusedInputWindows, i )
	{
		pPanelHover = m_vecFocusedInputWindows[ i ]->GetMouseHover();
		if ( pPanelHover )
			break;
	}

	pCapture->OnCapturedMouseHover( pPanelHover );
}


//-----------------------------------------------------------------------------
// Purpose: Releases input capture
//-----------------------------------------------------------------------------
void CUIInputEngine::ReleaseInputCapture( IInputCapture *pCapture )
{
	FOR_EACH_VEC_BACK( m_vecInputCapture, i )
	{
		if ( m_vecInputCapture[i] == pCapture )
		{
			m_vecInputCapture.Remove( i );
			return;
		}
	}

	AssertMsg( false, "pCapture not found in input capture list" );
}


//-----------------------------------------------------------------------------
// Purpose: Helper function to sort window inputs by window priority
//-----------------------------------------------------------------------------
static bool WindowInputPriorityOrder( CUIWindowInput *const &pLeftWindowInput, CUIWindowInput *const &pRightWindowInput, void *pContext )
{
	IUIWindow *pLeftWindow = pLeftWindowInput->GetTopLevelWindow();
	IUIWindow *pRightWindow = pRightWindowInput->GetTopLevelWindow();

	int nLeftPriority = pLeftWindow ? pLeftWindow->GetWindowPriority() : 0;
	int nRightPriority = pRightWindow ? pRightWindow->GetWindowPriority() : 0;

	// Ensure a stable sort in the case of equal priorities
	if ( nLeftPriority == nRightPriority )
		return pLeftWindow < pRightWindow;

	// We want the higher priority first
	return nLeftPriority > nRightPriority;
}


//-----------------------------------------------------------------------------
// Purpose: Read and record the current state of gamepad buttons on focus or
//			other interesting context switch
//----------------------------------------------------------------------------
void CUIInputEngine::RereadControllerState()
{
	// rescan the game controllers to detect keys that were down while focus was away from us
	m_GamePadController.GotWindowFocus();
#if !defined(NO_STEAM)
	m_SteamController.GotWindowFocus();
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Called on focus set
//-----------------------------------------------------------------------------
void CUIInputEngine::SetWindowInputFocus( CUIWindowInput *pFocus )
{
	// Don't need to do anything if already focused
	if ( !pFocus || m_vecFocusedInputWindows.HasElement( pFocus ) )
		return;

	m_vecFocusedInputWindows.SortedInsert( pFocus, &WindowInputPriorityOrder, NULL );

	RereadControllerState();

	UIEngine()->DispatchEventAsync( 0.0f, WindowGotFocus::MakeEvent( NULL, pFocus->GetTopLevelWindow() ) );
}


//-----------------------------------------------------------------------------
// Purpose: Read and record the current state of gamepad buttons on focus or
//			other interesting context switch
//----------------------------------------------------------------------------
void CUIWindowInput::RereadControllerState()
{
	m_pParent->RereadControllerState();
}


//-----------------------------------------------------------------------------
// Purpose: Should the input be allowed?
//----------------------------------------------------------------------------
bool CUIWindowInput::BAllowInput( InputMessage_t &msg )
{ 
	return m_pWindow->BAllowInput( msg ); 
}


//-----------------------------------------------------------------------------
// Purpose: Called on focus lost
//-----------------------------------------------------------------------------
void CUIInputEngine::LostWindowInputFocus( CUIWindowInput *pLost )
{
	if ( m_vecFocusedInputWindows.HasElement( pLost ) )
	{
		UIEngine()->DispatchEventAsync( 0.0f, WindowLostFocus::MakeEvent( NULL, pLost->GetTopLevelWindow() ) );
		m_vecFocusedInputWindows.FindAndRemove( pLost );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Check if a window has focus
//-----------------------------------------------------------------------------
bool CUIInputEngine::BHasWindowFocus( CUIWindowInput *pWindow )
{
	return m_vecFocusedInputWindows.HasElement( pWindow );
}


//-----------------------------------------------------------------------------
// Purpose: name of the currently connected gamepad
//-----------------------------------------------------------------------------
const char * panorama::CUIInputEngine::PchGamePadName(int iDevice)
{
	return m_GamePadController.PchGamePadName( iDevice );
}


//-----------------------------------------------------------------------------
// Purpose: validate mem
//-----------------------------------------------------------------------------
bool CUIInputEngine::BIsCapsLockOn()
{
#if defined( WIN32 )
	return ( GetKeyState( VK_CAPITAL ) & 1 ) != 0;
#elif defined( LINUX ) && !defined( SOURCE2_PANORAMA )
	return LinuxHelpers::BIsCapsLockOn();
#elif defined( OSX ) || defined( SOURCE2_PANORAMA )
	// SDL can get out of sync with system state because it maintains its own toggle state
	// TODO: fix SDL or call native OSX apis
	return ( SDL_GetModState() & KMOD_CAPS ) != 0;
#else
#error Not implemented.
	return false;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Get the correct default position to fire the haptic given the current input
//-----------------------------------------------------------------------------
IUIEngine::EHapticFeedbackPosition CUIInputEngine::GetHapticFeedbackPositionForInteraction()
{
#if !defined( SOURCE2_PANORAMA )
	if (GetActiveControllerType() == k_EActiveControllerType_VR && vrapi::VRSystem())
	{
		vr::TrackedDeviceIndex_t index = vrapi::VRDashboardManager()->GetPrimaryDashboardDevice();
		vr::ETrackedControllerRole controllerRole = vrapi::VRSystem()->GetControllerRoleForTrackedDeviceIndex(index);
		if (controllerRole == vr::TrackedControllerRole_LeftHand)
		{
			return IUIEngine::k_EHapticFeedbackPosition_Left;
		}
		else
		{
			return IUIEngine::k_EHapticFeedbackPosition_Right;
		}
	}
#endif // !SOURCE2_PANORAMA
	return IUIEngine::k_EHapticFeedbackPosition_Left;
}


//-----------------------------------------------------------------------------
// Purpose: Pulse haptic feedback on active gamepad/steam controller if supported
//-----------------------------------------------------------------------------
void CUIInputEngine::PulseActiveControllerHaptic( IUIEngine::EHapticFeedbackPosition ePosition, IUIEngine::EHapticFeedbackStrength eStrength )
{
	unsigned short strength = 1000;
	switch (eStrength)
	{
	case IUIEngine::k_EHapticFeedbackStrength_VeryLow:
		strength = 210;
		break;
	case IUIEngine::k_EHapticFeedbackStrength_Low:
		strength = 360;
		break;
	case IUIEngine::k_EHapticFeedbackStrength_Medium:
		strength = 900;
		break;
	case IUIEngine::k_EHapticFeedbackStrength_High:
		strength = 1500;
		break;
	case IUIEngine::k_EHapticFeedbackStrength_VeryHigh:
		strength = 2000;
		break;
	}
#if !defined(NO_STEAM)
	if (GetActiveControllerType() == k_EActiveControllerType_Steam)
	{
		m_SteamController.PulseHapticOnActiveController(ePosition == IUIEngine::k_EHapticFeedbackPosition_Left ? k_ESteamControllerPad_Left : k_ESteamControllerPad_Right, strength);
	}
#endif
#ifdef PANORAMA_PUBLIC_STEAM_SDK
	else if ( GetActiveControllerType() == k_EActiveControllerType_VR && vrapi::VRSystem() )
	{
		vr::TrackedDeviceIndex_t index = vrapi::VRSystem()->GetTrackedDeviceIndexForControllerRole( ePosition == IUIEngine::k_EHapticFeedbackPosition_Left ? vr::TrackedControllerRole_LeftHand : vr::TrackedControllerRole_RightHand );
		if ( index != vr::k_unTrackedDeviceIndexInvalid )
		{
			vrapi::VRSystem()->TriggerHapticPulse( index, 0, strength );
		}
	}
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Disables every Steam Controller except the requested index; -1 to re-enable all
//-----------------------------------------------------------------------------
void CUIInputEngine::SetControllerExclusiveEnabledIndex( int iIndex )
{
#ifndef NO_STEAM
	m_SteamController.SetControllerExclusiveEnabledIndex( iIndex );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Use to set the active controller type for now. Doesn't necessarily prevent
// other input from changing it later
//-----------------------------------------------------------------------------
void CUIInputEngine::ForceActiveControllerType( EActiveControllerType eControllerType )
{
	m_eForceActiveControllerType = eControllerType;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
ConVar g_cvarForceActiveControllerType( "forceactivecontrollertype", "-1" );

EActiveControllerType CUIInputEngine::GetActiveControllerType() const
{
	if ( g_cvarForceActiveControllerType.GetInt() > 0 )
		return static_cast<EActiveControllerType>( g_cvarForceActiveControllerType.GetInt() );
	if ( m_eForceActiveControllerType != k_EActiveControllerType_None )
		return m_eForceActiveControllerType;

	FOR_EACH_VEC( m_vecFocusedInputWindows, i )
	{
		CUIWindowInput *pWindowInput = m_vecFocusedInputWindows[ i ];
		if ( !pWindowInput->GetTopLevelWindow()->BIsVisible() )
			continue;

		if ( pWindowInput->BWasKeyboardOrMouseLastInputSource() )
		{
			// In VR, the controller acts as a mouse but we still want to know
			// that it came from a controller.  There's no way to use the actual
			// mouse in the VR overlay, so this should always be a fine
			// substitution to make.
			if ( pWindowInput->GetTopLevelWindow()->BIsVROverlay() )
			{
				return k_EActiveControllerType_VR;
			}
			return k_EActiveControllerType_KBMouse;
		}

		if ( pWindowInput->BWasGamepadLastInputSource() )
		{
#if !defined(NO_STEAM)
			if ( GetSteamControllerCount() > 0 && GetLastSteamControllerActiveTime() >= GetLastGamePadControllerActiveTime() && GetLastSteamControllerActiveTime() >= m_flVRControllerLastInputTime )
				return k_EActiveControllerType_Steam;
#endif
			if ( GetNumGamepadsConnected() > 0 && GetLastGamePadControllerActiveTime() > m_flVRControllerLastInputTime )
				return k_EActiveControllerType_XInput;

#if !defined( SOURCE2_PANORAMA )
			if ( pWindowInput->GetTopLevelWindow()->BIsVROverlay() ) // only check if it is an overlay window in Steam?
			{
				vr::TrackedDeviceIndex_t sortedDeviceIndexes[2];
				uint32_t unIndexCount = vrapi::VRSystem()->GetSortedTrackedDeviceIndicesOfClass( vr::TrackedDeviceClass::TrackedDeviceClass_Controller, sortedDeviceIndexes, 2, vr::k_unTrackedDeviceIndex_Hmd );
				if ( unIndexCount > 0 )
				{
					vr::EDeviceActivityLevel eActivityLevel = vrapi::VRSystem()->GetTrackedDeviceActivityLevel( sortedDeviceIndexes[0] );
					if ( eActivityLevel == vr::k_EDeviceActivityLevel_UserInteraction || eActivityLevel == vr::k_EDeviceActivityLevel_UserInteraction_Timeout )
					{
						return k_EActiveControllerType_VR;
					}
				}
			}
#endif // PANORAMA_PUBLIC_STEAM_SDK

		}
	}

	return k_EActiveControllerType_None;
}


#if !defined( SOURCE2_PANORAMA )
//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CUtlVector<EAttachedHardwareDevice> CUIInputEngine::GetAttachedHardwareDevices() const
{
	CUtlVector<EAttachedHardwareDevice> vecDevices;

	// Are we streaming? If so, we only care about devices hooked up to the streaming client, not devices
	// hooked up to the host machine. For example, if the host PC has a mouse/keyboard and we're streaming
	// to a Link that only has a Steam Controller, we act as if we had only the controller with no access
	// to the mouse/keyboard.
#if !defined( SOURCE2_PANORAMA ) && !defined( PANORAMA_PUBLIC_STEAM_SDK )
	if ( ClientRemoteClientManager()->BIsStreamingClientConnected() )
	{
		// FIXME: We don't currently have information about what devices are hooked up to the Link. As a
		//		  temp hack while Sam plumbs that information through we assume that if there are controllers
		//        connected they are remote.
		if ( GetNumGamepadsConnected() > 0 )
		{
			vecDevices.AddToTail( k_AttachedHardwareDevice_XInput );
		}
#if !defined(NO_STEAM)
		if ( GetSteamControllerCount() > 0 )
		{
			vecDevices.AddToTail( k_AttachedHardwareDevice_SteamController );
		}
#endif
		FOR_EACH_VEC( m_vecFocusedInputWindows, i )
		{
			if ( m_vecFocusedInputWindows[i]->BWasMouseOrKeyboardUsedThisSession() )
			{
				vecDevices.AddToTail( k_AttachedHardwareDevice_MouseKeyboard );
				break;
			}
		}
	}
	// Alright, we're not streaming so whatever is hooked up to this box is what we've got to work with.
	else
#endif // !defined( SOURCE2_PANORAMA ) && !defined( PANORAMA_PUBLIC_STEAM_SDK )
	{
		// Regular XInput-compatible gamepads, like a 360 pad? If used for filtering, this will only show
		// full controller support games without launchers, etc.
		if ( GetNumGamepadsConnected() > 0 )
		{
			vecDevices.AddToTail( k_AttachedHardwareDevice_XInput );
		}
#if !defined(NO_STEAM)
		// Steam Controllers present? If filtering around this, we'll show the full store catalog, as
		// everything is technically playable with the Steam Controller, but we'll show warning messages
		// about quality of experience when appropriate.
		if ( GetSteamControllerCount() > 0 )
		{
			vecDevices.AddToTail( k_AttachedHardwareDevice_SteamController );
		}
#endif
		// Are VR controllers present? We don't know! Hey, VR guys, this is the place to generate the list
		// of VR hardware that you want the store to filter on, and/or library to be able to present
		// informational warnings about.
		

		bool bHasVROverlayWindow = false;
		// Any platform besides SteamOS (Windows, OSX, separate Linux) assumes a mouse/keyboard is hooked
		// up. If used for filtering, this means we'll show the full catalog with no warnings (at least
		// until VR gets involved).
		FOR_EACH_VEC( m_vecFocusedInputWindows, i )
		{
			if ( m_vecFocusedInputWindows[i]->BWasMouseOrKeyboardUsedThisSession() )
			{
				vecDevices.AddToTail( k_AttachedHardwareDevice_MouseKeyboard );
			}

			if ( m_vecFocusedInputWindows[i]->GetTopLevelWindow()->BIsVROverlay() )
			{
				bHasVROverlayWindow = true;
			}
		}

		if ( bHasVROverlayWindow )
		{
			for ( vr::TrackedDeviceIndex_t i = 0; i < vr::k_unMaxTrackedDeviceCount; i++ )
			{
				if ( vrapi::VRSystem()->IsTrackedDeviceConnected( i ) )
				{
					vr::ETrackedDeviceClass eDeviceClass = vrapi::VRSystem()->GetTrackedDeviceClass( i );

					if ( eDeviceClass == vr::TrackedDeviceClass_HMD || eDeviceClass == vr::TrackedDeviceClass_Controller )
					{
						char szModelNumber[vr::k_unTrackingStringSize] = { 0 };
						char szManufacturer[vr::k_unTrackingStringSize] = { 0 };
						vr::ETrackedPropertyError error;
						vrapi::VRSystem()->GetStringTrackedDeviceProperty( vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_ModelNumber_String, szModelNumber, sizeof( szModelNumber ), &error );
						vrapi::VRSystem()->GetStringTrackedDeviceProperty( vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_ManufacturerName_String, szManufacturer, sizeof( szManufacturer ), &error );

						bool bHTC = V_stristr( szManufacturer, "HTC" ) != nullptr;
						bool bOculus = V_stristr( szManufacturer, "Oculus" ) != nullptr;

						switch ( eDeviceClass )
						{
						case vr::TrackedDeviceClass_HMD:
						{
							if ( bHTC )
							{
								if ( V_stristr( szModelNumber, "Vive" ) != nullptr )
								{
									if ( !vecDevices.HasElement( k_AttachedHardwareDevice_HTCVive ) )
									{
										vecDevices.AddToTail( k_AttachedHardwareDevice_HTCVive );
									}
								}
							}
							else if ( bOculus )
							{
								if ( V_stristr( szModelNumber, "CV1" ) != nullptr )
								{
									if ( !vecDevices.HasElement( k_AttachedHardwareDevice_OculusCV1 ) )
									{
										vecDevices.AddToTail( k_AttachedHardwareDevice_OculusCV1 );
									}
								}
								else if ( V_stristr( szModelNumber, "DK2" ) != nullptr )
								{
									if ( !vecDevices.HasElement( k_AttachedHardwareDevice_OculusDK2 ) )
									{
										vecDevices.AddToTail( k_AttachedHardwareDevice_OculusDK2 );
									}
								}
								else if ( V_stristr( szModelNumber, "DK1" ) != nullptr )
								{
									if ( !vecDevices.HasElement( k_AttachedHardwareDevice_OculusDK1 ) )
									{
										vecDevices.AddToTail( k_AttachedHardwareDevice_OculusDK1 );
									}
								}
							}
							else
							{
								AssertMsg1( false, "Unknown headset type %s", szManufacturer );
							}
						}
						break;

						case vr::TrackedDeviceClass_Controller:
						{
							if ( bHTC )
							{
								if ( !vecDevices.HasElement( k_AttachedHardwareDevice_HTCMotionController ) )
								{
									vecDevices.AddToTail( k_AttachedHardwareDevice_HTCMotionController );
								}
							}
							else if ( bOculus )
							{
								if ( !vecDevices.HasElement( k_AttachedHardwareDevice_OculusTouchController ) )
								{
									vecDevices.AddToTail( k_AttachedHardwareDevice_OculusTouchController );
								}
							}
							else
							{
								AssertMsg1( false, "Unknown controller type %s", szManufacturer );
							}
						}
						break;
						}
					}
				}
			} // for each possible VR device
		} // if overlay window
	} // not streaming

	return vecDevices;
}
#endif

#ifdef SOURCE2_PANORAMA

bool CUIInputEngine::IsIMEAllowed() const
{
	return m_bIMEAllowed;
}

void CUIInputEngine::SetIMEAllowed( bool bAllowed )
{
	if ( m_bIMEAllowed == bAllowed )
	{
		return;
	}

	m_bIMEAllowed = bAllowed;
}

#endif // #ifdef SOURCE2_PANORAMA


#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: validate mem
//-----------------------------------------------------------------------------
void CUIInputEngine::Validate( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE();
	ValidateObj( m_ActionBinds );
	ValidateObj( m_GamePadController );
#if !defined(NO_STEAM)
	ValidateObj( m_SteamController );
#endif
	ValidateObj( m_vecInputCapture );
	ValidateObj( m_MapAnalogValues );
	ValidateObj( m_vecFocusedInputWindows );
	ValidateObj( m_vecKeyBindingsFilePaths );
	FOR_EACH_VEC( m_vecKeyBindingsFilePaths, i )
	{
		ValidateObj( m_vecKeyBindingsFilePaths[i] );
	}
}
#endif


//-----------------------------------------------------------------------------
// Purpose: constructor
//-----------------------------------------------------------------------------
CUIWindowInput::CUIWindowInput( CTopLevelWindow *pWindow, CUIInputEngine *pParent )
{
	m_pParent = pParent;
	m_pWindow = pWindow;

	m_flTooltipDispatch = 0.0f;
	m_nMouseX = 0;
	m_nMouseY = 0;
	m_flSurfaceMouseX = k_flInvalidSurfaceMouseCoord;
	m_flSurfaceMouseY = k_flInvalidSurfaceMouseCoord;

	m_flLastHoverPanelFrameTime = 0.0f;
	m_ulLastHoverPanelPtrValue = k_ulInvalidPanelHandle64;
	m_flLastHoverPanelMouseX = 0.0f;
	m_flLastHoverPanelMouseY = 0.0f;
	m_bHoverDirty = false;
	m_dragMouseCode = MOUSE_INVALID;
	m_bCheckedForDrag = false;
	m_bDispatchDragEnter = false;
	m_bCheckedForDragScroll = false;
	m_dragScrollMouseCode = MOUSE_INVALID;
	m_nMouseDragOffsetX = 0;
	m_nMouseDragOffsetY = 0;
	m_bRemovePositionBeforeDragDrop = true;
	m_flScrollGrabTime = 0.0f;
	m_flVRTouchLinearMoveDistanceForHaptics = 0;

	m_bMouseTrackingDataDirty = false;
	m_bMouseActive = false;
	m_bMouseDown = false;
	m_bMouseVisible = false;
	m_bKeyboardActive = false;
	m_cMouseMoveCount = 0;
	m_eLastInputSource = k_ePanelEventSourceInvalid;

	m_pWindowInputForwarding = NULL;

	// Disable drag scroll for time being in CSGO
	// The hud chat history panel has selectable multi-line text which is not compatible with drag scrolling. 
	// Ideally should be able to disable this for individual panel... 
#if defined( PANORAMA_USE_S1WRAPPER )
	m_bEnableDragScroll = false;
#else
	m_bEnableDragScroll = true;
#endif

	m_dragDropMouseCodes.AddToTail( panorama::MOUSE_LEFT );
	m_dragDropMouseCodes.AddToTail( panorama::MOUSE_RIGHT );

	// copied from k_EAnimationEaseIn
	Vector2D vecPoints[4];
	panorama::GetAnimationCurveControlPoints( panorama::k_EAnimationEaseIn, vecPoints );
	m_AxisRepeatCurve.SetControlPoints( vecPoints );
}


//-----------------------------------------------------------------------------
// Purpose: destructor
//-----------------------------------------------------------------------------
CUIWindowInput::~CUIWindowInput() 
{
	m_pParent->LostWindowInputFocus( this );
}


//-----------------------------------------------------------------------------
// Purpose: Return the top of the current focus context
//-----------------------------------------------------------------------------
IUIPanel *CUIWindowInput::GetInputFocusContext()
{
	return m_ActionFocus.m_pTopmost.Get();
}


//-----------------------------------------------------------------------------
// Purpose: Restore focus to the context the specified panel is within
//-----------------------------------------------------------------------------
bool CUIWindowInput::SetInputFocusContext( IUIPanel *pPanelInContext )
{
	return SetInputFocusContextInternal( pPanelInContext, false, true );
}


void CUIWindowInput::FireInputFocusTopLevelChangedEvents( IUIPanel *pCurrentTopMost, IUIPanel *pCurrentFocus, IUIPanel *pNewTopMost, IUIPanel *pNewFocus )
{
	if( pCurrentTopMost )
	{
		IUIPanel *pTarget = pCurrentFocus ? pCurrentFocus : pCurrentTopMost;
		DispatchEvent( InputFocusTopLevelChanged(), pTarget, pNewTopMost );
	}

	if( pNewTopMost )
	{
		IUIPanel *pNewTarget = pNewFocus ? pNewFocus : pNewTopMost;
		DispatchEvent( InputFocusTopLevelChanged(), pNewTarget, pNewTopMost );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Goes to previous input context
//-----------------------------------------------------------------------------
void CUIWindowInput::PopInputContext()
{
	if ( m_stackPriorFocus.Count() == 0 )
		return;

	IUIPanel *pCurrentTopMost = m_ActionFocus.m_pTopmost.Get();
	IUIPanel *pCurrentFocus = m_ActionFocus.m_pFocus.Get();

	m_ActionFocus = m_stackPriorFocus.Element( m_stackPriorFocus.Tail() );
	m_stackPriorFocus.Remove( m_stackPriorFocus.Tail() );

	IUIPanel *pNewTopMost = m_ActionFocus.m_pTopmost.Get();
	IUIPanel *pNewFocus = m_ActionFocus.m_pFocus.Get();

	FireInputFocusTopLevelChangedEvents( pCurrentTopMost, pCurrentFocus, pNewTopMost, pNewFocus );
}


//-----------------------------------------------------------------------------
// Purpose: Remove the given input context panel from the stack
//-----------------------------------------------------------------------------
void CUIWindowInput::RemoveInputContext( IUIPanel *pPanel )
{
	// If the "top of input context" panel being removed had focus, check if we should restore to another topmost stack
	if ( pPanel == m_ActionFocus.m_pTopmost.Get() )
	{
		IUIPanel *pCurrentTopMost = m_ActionFocus.m_pTopmost.Get();
		IUIPanel *pCurrentFocus = m_ActionFocus.m_pFocus.Get();
		IUIPanel *pNewTopMost = NULL;
		IUIPanel *pNewFocus = NULL;

		if ( m_stackPriorFocus.Count() )
		{
			m_ActionFocus = m_stackPriorFocus.Element( m_stackPriorFocus.Tail() );
			m_stackPriorFocus.Remove( m_stackPriorFocus.Tail() );
			pNewTopMost = m_ActionFocus.m_pTopmost.Get();
			pNewFocus = m_ActionFocus.m_pFocus.Get();
		}
		else
		{
			m_ActionFocus.m_pTopmost.Clear();
			m_ActionFocus.m_pFocus.Clear();
		}
		FireInputFocusTopLevelChangedEvents( pCurrentTopMost, pCurrentFocus, pNewTopMost, pNewFocus );
	}
	else
	{
		FOR_EACH_LL_BACK( m_stackPriorFocus, i )
		{
			if ( m_stackPriorFocus[i].m_pTopmost.Get() == pPanel )
			{
				m_stackPriorFocus.Remove( i );
				break;
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Restore focus to the context the specified panel is within
//-----------------------------------------------------------------------------
#pragma warning( push )
#pragma warning( disable: 4706 )	// warning C4706: assignment within conditional expression
bool CUIWindowInput::SetInputFocusContextInternal( IUIPanel *pPanelInContext, bool bDontClearHoverAndMouseDown, bool bFireEvents )
{
	// Figure out the topmost panel in the hierarchy for both old and new focus
	IUIPanel *pCurrentTopMost = m_ActionFocus.m_pTopmost.Get();
	IUIPanel *pCurrentFocus = m_ActionFocus.m_pFocus.Get();

	IUIPanel *pNewTopMost = pPanelInContext;
	if ( pNewTopMost && !pNewTopMost->BTopOfInputContext() )
	{
		IUIPanel *pParent = NULL;
		while ( ( pParent = pNewTopMost->GetParent() ) && pParent != NULL )
		{
			pNewTopMost = pParent;
			if ( pNewTopMost->BTopOfInputContext() )
				break;
		}
	}

	// Already matches
	if ( pNewTopMost == pCurrentTopMost )
		return true;

	if( !bDontClearHoverAndMouseDown )
		ClearHoverData( 0.2f );

	TopMostFocus_t restoreFocus;
	if( pNewTopMost != pCurrentTopMost && pCurrentTopMost != NULL && pNewTopMost != NULL )
	{
		FOR_EACH_LL_BACK( m_stackPriorFocus, i )
		{
			if ( m_stackPriorFocus[i].m_pTopmost.Get() == pNewTopMost )
			{
				restoreFocus = m_stackPriorFocus[i];
				m_stackPriorFocus.Remove( i );

				// Track the old focus for the prior topmost hierarchy
				if ( pCurrentTopMost )
					m_stackPriorFocus.AddToTail( m_ActionFocus );

				m_ActionFocus = restoreFocus;
				
				if ( bFireEvents)
				{
					IUIPanel *pNewFocus = m_ActionFocus.m_pFocus.Get();
					FireInputFocusTopLevelChangedEvents( pCurrentTopMost, pCurrentFocus, pNewTopMost, pNewFocus );
				}

				return true;
			}
		}
	}

	return false;
}
#pragma warning( pop )


//-----------------------------------------------------------------------------
// Purpose: Are we currently inside a set input focus call
//-----------------------------------------------------------------------------
bool CUIWindowInput::BInSetInputFocusTraverse()
{
	return m_bInSetInputFocusTraverse;
}


//-----------------------------------------------------------------------------
// Purpose: Queue a panel focus event to occur once we finish with setting input focus
//-----------------------------------------------------------------------------
void CUIWindowInput::QueuePanelFocusEvent( IUIPanel *pPanel, CPanoramaSymbol symPanelEvent )
{
	int iTail = m_vecQueuedPanelFocusEvents.AddToTail();
	m_vecQueuedPanelFocusEvents[iTail].m_pPanel = pPanel;
	m_vecQueuedPanelFocusEvents[iTail].m_symPanelEvent = symPanelEvent;
}


//-----------------------------------------------------------------------------
// Purpose: Set the input focus panel
//-----------------------------------------------------------------------------
void CUIWindowInput::SetInputFocus( IUIPanel *pPanel, bool bScrollParentToFit, bool bChangeContextIfNeeded )
{
	VPROF_BUDGET( "CUIWindowInput::SetInputFocus", VPROF_BUDGETGROUP_TENFOOT );

	IUIPanel *pCurrentFocus = m_ActionFocus.m_pFocus.Get();
	if ( pCurrentFocus == pPanel )
	{
		return;
	}

	AssertMsg( m_vecQueuedPanelFocusEvents.Count() == 0, "m_vecQueuedPanelFocusEvents isn't empty, is SetInputFocus being called in a re-entrant manner?  It shouldn't/can't be!!" );

	// Figure out the topmost panel in the hierarchy for both old and new focus
	IUIPanel *pCurrentTopMost = m_ActionFocus.m_pTopmost.Get();
	
	IUIPanel *pNewTopMost = pPanel;

	if ( pNewTopMost && !pNewTopMost->BTopOfInputContext() )
	{
		IUIPanel *pParent = NULL;
		while ( ( ( pParent = pNewTopMost->GetParent() ) != NULL ) && ( pParent != NULL ) )
		{
			pNewTopMost = pParent;
			if ( pNewTopMost->BTopOfInputContext() )
				break;
		}
	}

	bool bTopMostHasChanged = false;
	TopMostFocus_t restoreFocus;

	if( pNewTopMost != pCurrentTopMost && pCurrentTopMost != NULL && pNewTopMost != NULL )
	{
		bTopMostHasChanged = true;
		pCurrentFocus = NULL;

		
		FOR_EACH_LL_BACK( m_stackPriorFocus, i )
		{
			if ( m_stackPriorFocus[i].m_pTopmost.Get() == pNewTopMost )
			{
				restoreFocus = m_stackPriorFocus[i];
				m_stackPriorFocus.Remove( i );
				break;
			}
		}

		// Track the old focus for the prior topmost hierarchy
		if ( pCurrentTopMost )
			m_stackPriorFocus.AddToTail( m_ActionFocus );
	}

	// Swap contexts back, then call ourselves back
	if ( bTopMostHasChanged )
	{
		if( bChangeContextIfNeeded  )
			ClearHoverData( 0.2f );

		IUIPanel *pOldFocus = m_ActionFocus.m_pFocus.Get();

		m_ActionFocus = restoreFocus;
		if ( bChangeContextIfNeeded )
		{
			// don't fire the events for context change if we will undo it in the call below
			FireInputFocusTopLevelChangedEvents( pCurrentTopMost, pOldFocus, pNewTopMost, pPanel );
		}

		SetInputFocus( pPanel, bScrollParentToFit, bChangeContextIfNeeded );
		
		if( !bChangeContextIfNeeded )
			SetInputFocusContextInternal( pCurrentTopMost, true, false );

		return;
	}

	m_bInSetInputFocusTraverse = true;

	// find first parent that has focus set from descendants
	IUIPanel *pHasDescendantFocus = pPanel;
	while ( pHasDescendantFocus != NULL && !(pHasDescendantFocus->GetStyleFlags() & k_EStyleFlagDescendantFocused ) )
		pHasDescendantFocus = pHasDescendantFocus->GetParent();

	// remove focus up to parent which should have hover
	for( IUIPanel *pParent = pCurrentFocus; pParent != NULL && pParent != pHasDescendantFocus; pParent = pParent->GetParent() )
		pParent->RemoveStyleFlag( k_EStyleFlagDescendantFocused );

	// if the panel we are setting focus to is also the first parent keeping descendantfocus, make sure to remove its descendantfocus styles
	if ( pPanel && pPanel == pHasDescendantFocus )
		pPanel->RemoveStyleFlag( k_EStyleFlagDescendantFocused );

	// set focus up to parent which already has it
	if ( pPanel && !pPanel->BTopOfInputContext() )
	{
		IUIPanel *pDirectParent = pPanel->GetParent();
		for( IUIPanel *pParent = pDirectParent; pParent != NULL && pParent != pHasDescendantFocus; pParent = pParent->GetParent() )
		{
			pParent->AddStyleFlag( k_EStyleFlagDescendantFocused );
			if ( pParent->BTopOfInputContext() )
				break;
		}
	}

	m_ActionFocus.m_pFocus = pPanel;
	m_ActionFocus.m_pTopmost = pNewTopMost;

	if ( pCurrentFocus )
		pCurrentFocus->RemoveStyleFlag( k_EStyleFlagFocus );

	if ( pPanel )
	{
		Assert( pPanel->BAcceptsFocus() );
		pPanel->AddStyleFlag( k_EStyleFlagFocus );
	}

	//
	// Ok done updating all state / style flags in the traverse... now we need to dispatch all the notifications, which may cause
	// re-entrancy and state to change again.
	//

	m_bInSetInputFocusTraverse = false;

	CPanelPtr<IUIPanel> pOldFocusPtr = pCurrentFocus;
	CPanelPtr<IUIPanel> pNewFocusPtr = pPanel;

	CUtlVector<QueuedPanelEvent_t> vecLocal;
	vecLocal.Swap( m_vecQueuedPanelFocusEvents );

	// If there is an existing focus deal with removing its focus
	if ( pOldFocusPtr.Get() )
	{
		// However, if the current focus was in a different topmost panel hierarchy within the window
		// then we shouldn't update it
		DispatchEvent( InputFocusLost(), pOldFocusPtr.Get() );
	}
	
	// always dispatch input focus set (could go to NULL, unhandled listeners might care)
	DispatchEvent( InputFocusSet(), pPanel );

	// Now dispactch any queued panel events onfocus, onblur, ondescendantfocus, ondescendantblur, etc... these may in turn change focus again...
	FOR_EACH_VEC( vecLocal, i )
	{
		if ( vecLocal[i].m_pPanel.Get() )
		{
			vecLocal[i].m_pPanel->DispatchPanelEvent( vecLocal[i].m_symPanelEvent );
		}
	}
	vecLocal.RemoveAll();

	// We do this at the end, even though state may have changed in the events above because we need to make sure styles are updated for the appropriate 
	// sizing/scrolling to happen, which may require dispatching all the events.
	if ( pNewFocusPtr.Get() && pNewFocusPtr.Get() == m_ActionFocus.m_pFocus.Get() )
	{
		if ( bScrollParentToFit )
			pPanel->ScrollParentToMakePanelFit( SCROLL_BEHAVIOR_DEFAULT, false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set the input focus panel
//-----------------------------------------------------------------------------
void CUIWindowInput::PanelDeleted( IUIPanel *pPanel, IUIPanel *pParent )
{
	// if the mouse is over the panel being deleted, set to parent
	IUIPanel *pMouseOver = m_ptrMouseOver.Get();
	if ( pPanel == pMouseOver )
		m_ptrMouseOver = pParent;

	if ( pPanel == m_pMouseOverInternal.Get() )
		m_pMouseOverInternal = pParent;

	// Delete any bindings that were made directly on panel
	m_pParent->OnPanelDeleted( pPanel );

	// If the panel being deleted had focus, check if we should restore to another topmost stack
	if ( pPanel == m_ActionFocus.m_pTopmost.Get() && m_stackPriorFocus.Count() )
	{
		m_ActionFocus = m_stackPriorFocus.Element( m_stackPriorFocus.Tail() );
		m_stackPriorFocus.Remove( m_stackPriorFocus.Tail() );
		IUIPanel *pNewTopMost = m_ActionFocus.m_pTopmost.Get();
		IUIPanel *pNewFocus = m_ActionFocus.m_pFocus.Get();
		FireInputFocusTopLevelChangedEvents( NULL, NULL, pNewTopMost, pNewFocus );
	}
	else
	{
		FOR_EACH_LL_BACK( m_stackPriorFocus, i )
		{
			if ( m_stackPriorFocus[i].m_pTopmost.Get() == pPanel )
			{
				m_stackPriorFocus.Remove( i );
				break;
			}
		}
	}

}


//-----------------------------------------------------------------------------
// Purpose: Searches from the panel through parents returning the first panel that wants input
//-----------------------------------------------------------------------------
IUIPanel *FindInputPanel( IUIPanel *pPanel )
{
	// find parent that would like input
	for ( ; pPanel != NULL; pPanel = pPanel->GetParent() )
	{
		if ( pPanel->BCanAcceptInput() )
			return pPanel;

		// Stop traversing up past top of input context
		if ( pPanel->BTopOfInputContext() )
			return NULL;
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Called from animation thread to set last hover panel and coords within that panel
//-----------------------------------------------------------------------------
void CUIWindowInput::SetLastHover( double flFrameTime, uint64 ulPanelSafePtrValue, float flMouseX, float flMouseY )
{
	AUTO_LOCK( m_HoverPanelMutex );
	if ( flFrameTime >= m_flLastHoverPanelFrameTime && 
		( ulPanelSafePtrValue != m_ulLastHoverPanelPtrValue || flMouseX != m_flLastHoverPanelMouseX || flMouseY != m_flLastHoverPanelMouseY ) )
	{
		m_ulLastHoverPanelPtrValue = ulPanelSafePtrValue;
		m_flLastHoverPanelMouseX = flMouseX;
		m_flLastHoverPanelMouseY = flMouseY;
		m_flLastHoverPanelFrameTime = flFrameTime;

		m_bHoverDirty = true;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called to clear current hover data, and ignore new hover until specified time
//-----------------------------------------------------------------------------
void CUIWindowInput::ClearHoverData( double flSecondsToSuppress )
{
	{
		AUTO_LOCK( m_HoverPanelMutex );
		m_ulLastHoverPanelPtrValue = k_ulInvalidPanelHandle64;
		m_flLastHoverPanelMouseX = 0.0f;
		m_flLastHoverPanelMouseY = 0.0f;
		m_flLastHoverPanelFrameTime = UIEngine()->GetCurrentFrameTime() + flSecondsToSuppress;
	}

	ProcessHoverData();
	ClearMouseDown();
}


//-----------------------------------------------------------------------------
// Purpose: Called to process updated hover data
//-----------------------------------------------------------------------------
void CUIWindowInput::ProcessHoverData()
{
	uint64 ulPanelSafePtrValue = k_ulInvalidPanelHandle64;
	float flMouseX = 0.0f;
	float flMouseY = 0.0f;

	{
		AUTO_LOCK( m_HoverPanelMutex );
		ulPanelSafePtrValue = m_ulLastHoverPanelPtrValue;
		flMouseX = m_flLastHoverPanelMouseX;
		flMouseY = m_flLastHoverPanelMouseY;
		m_bHoverDirty = false;
	}

	CPanelPtr<IUIPanel> hPanel;
	hPanel.SetFromUInt64( ulPanelSafePtrValue );

	// If the panel isn't in our window, then ignore this event
	IUIPanel *pPanel = hPanel.Get();
	if( !pPanel || hPanel->GetParentWindow() != m_pWindow )
	{
		if ( m_ptrMouseDragInitiate.Get() && m_pMouseOverInternal.Get() )
		{
			DispatchEvent( DragLeave(), m_pMouseOverInternal.Get(), m_ptrMouseDragDisplay );
		}

		ChangeHoverState( NULL, m_ptrMouseOver.Get() );
		m_ptrMouseOver.Clear();
		m_pMouseOverInternal.Clear();
		return;
	}

	CUtlVector<IInputCapture *> &vecInputCaptures = m_pParent->GetInputCapture();

	// Always update mouse move
	IUIPanel *pTarget = pPanel;
	if ( pTarget )
	{
		bool bHandledByCapture = CallInputCaptures( vecInputCaptures, [ & ]( IInputCapture *pCapture )
		{
			return pCapture->OnCapturedMouseMove( pTarget, flMouseX, flMouseY );
		} );

		if ( !bHandledByCapture )
		{
			pTarget->ClientPtr()->OnMouseMove( flMouseX, flMouseY );
		}
	}

	if ( !m_pWindow->BCursorVisible() && vecInputCaptures.Count() == 0 && m_ptrMouseOver.Get() )
	{
		// if the cursor isn't visible and we have a hover panel lets undo that
		if ( m_ptrMouseDragInitiate.Get() && m_pMouseOverInternal.Get() )
		{
			DispatchEvent( DragLeave(), m_pMouseOverInternal.Get(), m_ptrMouseDragDisplay );
		}

		ChangeHoverState( NULL, m_ptrMouseOver.Get() );
		m_ptrMouseOver.Clear();
		m_pMouseOverInternal.Clear();
		return;
	}

	// tell the window that the cursor needs updating
	if ( m_ptrMouseOver.Get() )
		m_pWindow->SetMouseCursor( m_ptrMouseOver->ClientPtr()->GetMouseCursor() );
#if defined( SOURCE2_PANORAMA ) && !defined( PANORAMA_USE_S1WRAPPER )
	else
		m_pWindow->SetMouseCursor( eMouseCursor_PassThrough );
#endif

	// If there is no change, return
	if (m_ptrMouseOver.Get() == pPanel)
	{
		// mouse hover didn't change so code below wont be called for sending first DragEnter. Send it now.
		if ( m_bDispatchDragEnter )
		{
			if ( m_ptrMouseDragInitiate.Get() )
				DispatchEvent( DragEnter(), pPanel, m_ptrMouseDragDisplay );
			
			m_bDispatchDragEnter = false;
		}

		return;
	}
	m_bDispatchDragEnter = false;

	// if someone is capturing input, forward results and bail
	bool bHandledByCapture = CallInputCaptures( vecInputCaptures, [ & ]( IInputCapture *pCapture )
	{
		if ( pCapture->OnCapturedMouseHover( pPanel ) )
		{
			// don't set hover state when capturing input (most likely the debugger)
			ChangeHoverState( NULL, m_ptrMouseOver.Get() );

			// The panel may not be valid any longer; assign hPanel to handle this.
			m_pMouseOverInternal = m_ptrMouseOver = hPanel;
			return true;
		}

		return false;
	} );
	
	if ( !bHandledByCapture && m_pWindow->BCursorVisible() ) // only change the hover panel if the cursor is up
	{
		// if dragging and hover panel changes, dispatch events
		IUIPanel *pOldMouseOver = m_pMouseOverInternal.Get();
		if ( m_ptrMouseDragDisplay.Get() && pOldMouseOver != pPanel )
		{
			if ( pOldMouseOver )
				DispatchEvent( DragLeave(), pOldMouseOver, m_ptrMouseDragDisplay );

			if ( pPanel )
				DispatchEvent( DragEnter(), pPanel, m_ptrMouseDragDisplay );
		}

		// If the mouse is down over someone else, don't update hover state or input, that panel
		// keeps it until the mouse is released, do separately track who was really hovered so we can handle
		// mouse up correctly later.
		IUIPanel *pMouseDown = nullptr;
		// for ( MouseDownState_t &mouseDownState : m_MouseDownStates ) <- not for CS:GO
		// --- Below is a workaround for common CS:GO voice chat bind being on mouse4 ---
		// It is frequent that users in a party or squadmates are voice chatting while having to interact
		// with some prolonged user interface panel experiences that update on hover.
		// In other games holding any mouse button on a panel will prevent hover state updates for other panels
		// which means that in CS:GO players who are voice chatting and holding mouse4 button will not be able
		// to interact with UI panels normally because hover state will not be updated correctly and UI will feel
		// like it is "frozen" until the voice chat side mouse button is released.
		// We will work around this for CS:GO and continue to update hover disregarding whether MOUSE3, MOUSE4, MOUSE5
		// buttons were pressed in other panels. No UI panels in CS:GO listen for MOUSE3/4/5 presses or drag events, so
		// it will feel natural to the user to continue updating hover.
		// Adding a few compile-time assert to ensure that we access m_MouseDownState arrays with valid indices.
		COMPILE_TIME_ASSERT( MOUSE_LEFT == 0 );
		COMPILE_TIME_ASSERT( MOUSE_RIGHT == 1 );
		COMPILE_TIME_ASSERT( ARRAYSIZE( m_MouseDownStates ) >= 2 );
		for ( int j = MOUSE_LEFT; j <= MOUSE_RIGHT; ++ j )
		{
			MouseDownState_t &mouseDownState = m_MouseDownStates[j];
			pMouseDown = mouseDownState.m_ptrMouseDown.Get();
			if ( pMouseDown )
				break;
		}
		if ( pMouseDown && pMouseDown != pPanel )
		{
			m_pMouseOverInternal = pPanel;
			return;
		}

		ChangeHoverState( pPanel, m_ptrMouseOver.Get() );

		m_pMouseOverInternal = m_ptrMouseOver = pPanel;
	}

}


//-----------------------------------------------------------------------------
// Purpose: called from the animation thread to get the list of panel handles that always want mouse move events
//-----------------------------------------------------------------------------
CCopyableUtlVector<uint64> CUIWindowInput::GetMouseTrackingHandles()
{
	AUTO_LOCK( m_MouseTrackingMutex );
	return m_vecTrackingPanelHandles;
}


//-----------------------------------------------------------------------------
// Purpose: add this panel to tracking
//-----------------------------------------------------------------------------
void CUIWindowInput::AddMouseTrackingPanel( IUIPanel *pPanel )
{
	if ( m_vecptrMouseTrackingPanels.Find( pPanel ) == m_vecptrMouseTrackingPanels.InvalidIndex() )
		m_vecptrMouseTrackingPanels.AddToTail( pPanel );
}


//-----------------------------------------------------------------------------
// Purpose: remove this panel from tracking
//-----------------------------------------------------------------------------
void CUIWindowInput::RemoveMouseTrackingPanel( IUIPanel *pPanel )
{
	int idx = m_vecptrMouseTrackingPanels.Find( pPanel );
	if ( idx != m_vecptrMouseTrackingPanels.InvalidIndex() )
	{
		m_vecptrMouseTrackingPanels.Remove( idx );
	}
}


//-----------------------------------------------------------------------------
// Purpose: called from the animation thread to return the mouse data for the tracked panels
//-----------------------------------------------------------------------------
void CUIWindowInput::SetMouseTrackingResults( CCopyableUtlVector<MouseTrackingResults_t> &vec )
{
	AUTO_LOCK( m_MouseTrackingMutex );
	m_vecTrackingResults.Swap( vec );
	m_bMouseTrackingDataDirty = true;
}


//-----------------------------------------------------------------------------
// Purpose: process any tracking data we have from the anim thread and rebuild the tracking panel handle list
//-----------------------------------------------------------------------------
void CUIWindowInput::ProcessMouseTrackingResults()
{
	VPROF_BUDGET( "CUIWindowInput::ProcessMouseTrackingResults", VPROF_BUDGETGROUP_TENFOOT );

	uint64 ulPanelSafePtrValue = k_ulInvalidPanelHandle64;
	float flMouseX = 0.0f;
	float flMouseY = 0.0f;
	CCopyableUtlVector< MouseTrackingResults_t >  vecResults;

	{
		// clear the results vector and cache a local copy
		AUTO_LOCK( m_MouseTrackingMutex );
		vecResults.Swap( m_vecTrackingResults );
		m_bMouseTrackingDataDirty = false;
	}

	// process any tracking results we had
	FOR_EACH_VEC( vecResults, i )
	{
		ulPanelSafePtrValue = vecResults[i].m_hPanel;
		flMouseX =  vecResults[i].m_flX;
		flMouseY =  vecResults[i].m_flY;

		if ( ulPanelSafePtrValue == m_ulLastHoverPanelPtrValue )
			continue; // don't double report if this is the active panel right now

		CPanelPtr<IUIPanel> hPanel;
		hPanel.SetFromUInt64( ulPanelSafePtrValue );

		IUIPanel *pPanel = hPanel.Get();
		if( pPanel )
		{
			pPanel->ClientPtr()->OnMouseMove( flMouseX, flMouseY );
		}
	}

	{
		VPROF_BUDGET( "CUIWindowInput::ProcessMouseTrackingResults - Recalculate handles", VPROF_BUDGETGROUP_TENFOOT );
		// recalculate the handle list we want results for
		CCopyableUtlVector< uint64 > vecPanelHandles;
		vecPanelHandles.EnsureCapacity( m_vecptrMouseTrackingPanels.Count() );
		FOR_EACH_VEC_BACK( m_vecptrMouseTrackingPanels, i )
		{
			if ( !m_vecptrMouseTrackingPanels[i].Get() )
			{
				m_vecptrMouseTrackingPanels.Remove( i );
				continue;
			}

			if ( m_vecptrMouseTrackingPanels[i]->BIsVisible() )
				vecPanelHandles.AddToTail( m_vecptrMouseTrackingPanels[i].GetHandleAsUInt64() );
		}

		// also add in any panels that have mouse down
		for ( MouseDownState_t &mouseDownState : m_MouseDownStates )
		{
			if ( mouseDownState.m_ptrMouseDown.Get() )
			{
				vecPanelHandles.AddToTail( mouseDownState.m_ptrMouseDown.GetHandleAsUInt64() );
			}
		}

		{
			// now swap in the new handle list
			AUTO_LOCK( m_MouseTrackingMutex );
			m_vecTrackingPanelHandles.Swap( vecPanelHandles );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: frame func
//-----------------------------------------------------------------------------
void CUIWindowInput::RunFrame() 
{
	// clear mouse down panel if cursor becomes invisible
	if ( !m_pWindow->BCursorVisible() )
	{
		ClearMouseDown();
	}

#ifdef SOURCE2_PANORAMA
	// If we ever miss a mouse up event (because maybe it wasn't over panorama), then clear things out here
	if ( m_pWindow->BUseAutoMouseUpBehavior() )
	{
		for ( MouseCode mouseCode = ( MouseCode )0; mouseCode < MOUSE_LAST; mouseCode = ( MouseCode )( mouseCode + 1 ) )
		{
			MouseDownState_t &mouseDownState = m_MouseDownStates[ mouseCode ];
			if ( !mouseDownState.m_ptrMouseDown.Get() )
				continue;

			ButtonCode_t buttonCode = UIInputEngine()->MouseCodeToButtonCode( mouseCode );
			if ( buttonCode != BUTTON_CODE_INVALID && !g_pInputSystem->IsButtonDown( buttonCode ) )
			{
				SetMouseDownPtr( nullptr, mouseCode );
			}
		}
	}
#endif

	bool bCursorVisible = m_pWindow->BCursorVisible();
	if ( bCursorVisible != m_bMouseVisible )
	{
		// cursor visibility changed, let re-think hover data
		m_bHoverDirty = true;
	}

	m_bMouseVisible = bCursorVisible;

	// Process hover data if it's changed
	if ( m_bHoverDirty )
	{
		ProcessHoverData();
	}

	if ( m_bMouseTrackingDataDirty )
	{
		ProcessMouseTrackingResults();
	}

	if( m_flTooltipDispatch <= UIEngine()->GetCurrentFrameTime() )
	{
		DispatchShowTooltip();
	}

	UpdateDragDrop();

	if ( m_bEnableDragScroll )
	{
		UpdateDragScroll();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Checks if a panel qualifies for child focus on hover
//-----------------------------------------------------------------------------
bool BChildFocusOnHoverEligible( IUIPanel *pPanel )
{
	if ( UIEngine()->BIsPanelWaitingAsyncDelete( pPanel ) )
		return false;

	if ( !pPanel->IsActivationEnabled() )
		return false;

	// also handle get focus on hover. No parent check needed
	if ( pPanel->GetFocusOnHover() )
		return true;

	// find parent with child focus on hover
	for ( IUIPanel *pParent = pPanel->GetParent(); pParent; pParent = pParent->GetParent() )
	{
		if ( !pParent->GetChildFocusOnHover() )
			continue;

		// If the parent is moving, don't hover focus on any of its children
		if ( pParent->BScrollInProgress() )
		{
			return false;
		}

		if ( pParent->BHasDescendantKeyFocus() || pParent->BHasKeyFocus() )
			return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Updates hover state on panels
//-----------------------------------------------------------------------------
void CUIWindowInput::ChangeHoverState( IUIPanel *pTo, IUIPanel *pFrom )
{
	// find first parent that has hover set from target
	IUIPanel *pHasHover = pTo;
	while ( pHasHover != NULL && !pHasHover->BHasHoverStyle() )
		pHasHover = pHasHover->GetParent();

	// remove hover up to parent which should have hover
	for( IUIPanel *pPanel = pFrom; pPanel != NULL && pPanel != pHasHover; pPanel = pPanel->GetParent() )
	{
		pPanel->RemoveStyleFlag( k_EStyleFlagHover );
	}

	// set hover up to parent which already has it
	bool bAllowRequestFocusOnHover = true;

	if ( !m_pWindow->BCursorVisible() )
		bAllowRequestFocusOnHover = false;

	for( IUIPanel *pPanel = pTo; pPanel != NULL && pPanel != pHasHover; pPanel = pPanel->GetParent() )
	{
		pPanel->AddStyleFlag( k_EStyleFlagHover );
		
		if ( bAllowRequestFocusOnHover )
		{
			if ( pPanel->BHasKeyFocus() )
			{
				// Found the focused panel in the hover hierarchy, don't let parents auto-steal focus
				bAllowRequestFocusOnHover = false;
			}
			else if ( pPanel->BAcceptsFocus() && pPanel->IsEnabled() && BChildFocusOnHoverEligible( pPanel ) )
			{
				// Found a panel in hover hierarchy that accepts focus and wants focus auto-set to it upon hover
				bAllowRequestFocusOnHover = false;
				pPanel->SetFocusDueToHover();
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: an input event was received
//-----------------------------------------------------------------------------
bool CUIWindowInput::InputEvent( InputMessage_t &msg, bool bNewEvent /* = true */ )
{
	if ( m_pWindowInputForwarding )
		m_pWindowInputForwarding->InputEvent( msg, bNewEvent );
	
	if ( !BAllowInput( msg ) )
		return false;

	if ( bNewEvent )
		UIEngine()->UpdateLastInputTime();

	if ( msg.m_GamePadData.m_GamePadCode >= VR_BUTTON_PRIMARY_APP && msg.m_GamePadData.m_GamePadCode < VR_BUTTON_LAST )
	{
		// VR Input, set the input source as gamepad to override any mouse-looking input
		m_eLastInputSource = k_ePanelEventSourceGamepad;
	}

	// Any significant movement will trigger a gamepaddown as well, so let it update m_eLastInputSource.  Doing so here would require de-duping as we
	// post analog input every single frame.
	if ( msg.m_eInputType != k_eGamePadAnalog )
		m_eLastInputSource = msg.m_eSource;

	// deliver input specific events
	bool bHandled = false;
	int nRepeats = 0;
	switch ( msg.m_eInputType )
	{
    default:
        break;
	case k_eKeyDown:
		{
			m_bKeyboardActive = true;
			nRepeats = msg.m_KeyData.m_RepeatCount;
			bHandled = BSendInput( m_ActionFocus.m_pFocus.Get(), msg.m_KeyData, &IInputCapture::OnCapturedKeyDown, &IUIPanelClient::OnKeyDown, false );			
		}
		break;
	case k_eKeyUp:
		{
			m_bKeyboardActive = true;
			nRepeats = msg.m_KeyData.m_RepeatCount;
			bHandled = BSendInput( m_ActionFocus.m_pFocus.Get(), msg.m_KeyData, &IInputCapture::OnCapturedKeyUp, &IUIPanelClient::OnKeyUp, false );
		}
		break;
	case k_eKeyChar:
		{
			m_bKeyboardActive = true;
			nRepeats = msg.m_KeyData.m_RepeatCount;
			bHandled = BSendInput( m_ActionFocus.m_pFocus.Get(), msg.m_KeyData, &IInputCapture::OnCapturedKeyTyped, &IUIPanelClient::OnKeyTyped, false );
		}
		break;

	case k_eMouseDown:
		m_bMouseActive = true;
		m_bMouseDown = true;
		bHandled = OnMouseButtonDown( msg.m_MouseData );
		break;

	case k_eMouseUp:
		{
			m_bMouseActive = true;
			m_bMouseDown = false;
			bool bSuppressActivateOnMouseUp = m_MouseDownStates[ msg.m_MouseData.m_MouseCode ].m_bSuppressActivateOnMouseUp;
			m_MouseDownStates[ msg.m_MouseData.m_MouseCode ].m_bSuppressActivateOnMouseUp = false;
			bHandled = OnMouseButtonUp( msg.m_MouseData, bSuppressActivateOnMouseUp );
		}
		break;

	case k_eMouseDoubleClick:
		m_bMouseActive = true;
		m_bMouseDown = false;
		bHandled = OnMouseDoubleClick( msg.m_MouseData );
		if ( bHandled )
		{
			m_MouseDownStates[ msg.m_MouseData.m_MouseCode ].m_bSuppressActivateOnMouseUp = true;
		}
		break;

	case k_eMouseTripleClick:
		m_bMouseActive = true;
		m_bMouseDown = false;
		bHandled = OnMouseTripleClick( msg.m_MouseData );
		if ( bHandled )
		{
			m_MouseDownStates[ msg.m_MouseData.m_MouseCode ].m_bSuppressActivateOnMouseUp = true;
		}
		break;

	case k_eMouseWheel:
		m_bMouseActive = true;
		m_bMouseDown = false;
		bHandled = OnMouseWheel( msg.m_MouseData );
		break;

	case k_eMouseEnter:
		// Don't set mouse active, it will show on movement when in the window, and if set on enter/leave
		// we were getting in a bad bouncing state
		break;

	case k_eMouseLeave:
		// Don't set mouse active, it will show on movement when in the window, and if set on enter/leave
		// we were getting in a bad bouncing state
		OnMouseLeave();
		break;

	case k_eGamePadUp:
		{
			DispatchEvent( GamepadInput(), m_ActionFocus.m_pFocus.Get(), msg.m_GamePadData.m_GamePadCode );
			bHandled = BSendInput( m_ActionFocus.m_pFocus.Get(), msg.m_GamePadData, &IInputCapture::OnCapturedGamePadUp, &IUIPanelClient::OnGamePadUp, false );
		}
		break;
	case k_eGamePadDown:
		{
			nRepeats = msg.m_GamePadData.m_RepeatCount;
			DispatchEvent( GamepadInput(), m_ActionFocus.m_pFocus.Get(), msg.m_GamePadData.m_GamePadCode );
			bHandled = BSendInput( m_ActionFocus.m_pFocus.Get(), msg.m_GamePadData, &IInputCapture::OnCapturedGamePadDown, &IUIPanelClient::OnGamePadDown, false );
		}
		break;
	case k_eGamePadAnalog:
		{
			bHandled = BSendInput( m_ActionFocus.m_pFocus.Get(), msg.m_GamePadData, &IInputCapture::OnCapturedGamePadAnalog, &IUIPanelClient::OnGamePadAnalog, false );
		}
		break;
	case k_eVRTouchPad:
		{
			bHandled = BSendInput( m_ActionFocus.m_pFocus.Get(), msg.m_VRTouchData, &IInputCapture::OnCapturedVRTouchPad, &IUIPanelClient::OnVRTouchPad, false );
		}
		break;
	}

	if ( m_eLastInputSource == k_ePanelEventSourceKeyboard || m_eLastInputSource == k_ePanelEventSourceMouse )
	{
		m_bHasReceivedMouseOrKeyboardInput = true;
	}

	// now deliver the higher level action if the low level event wasn't caught	
	if ( !bHandled )
	{
		InputAction_t action;
		m_pParent->TranslateInputEvent( msg, action );

		if ( action.m_symUIEventName.IsValid() || !action.m_pJSAction.IsEmpty() )
		{
			// Extra debounce logic for some actions here, which makes it consistent across all platforms/input methods
			if( action.m_symUIEventName == ACTION_UP || action.m_symUIEventName == ACTION_DOWN || action.m_symUIEventName == ACTION_RIGHT || action.m_symUIEventName == ACTION_LEFT )
			{
				if ( nRepeats == 0 )
				{
					int iMap = m_mapActionRepeats.Find( action.m_symUIEventName );
					if ( iMap == m_mapActionRepeats.InvalidIndex() )
					{
						ActionRepeatData_t data;
						data.nRepeats = 0;
						data.flLast = data.flFirst = UIEngine()->GetCurrentFrameTime();
						m_mapActionRepeats.Insert( action.m_symUIEventName, data );
					}
					else
					{
						ActionRepeatData_t &data = m_mapActionRepeats[iMap];
						data.nRepeats = 0;
						data.flLast = data.flFirst = UIEngine()->GetCurrentFrameTime();
					}
				}
				else
				{
					int iMap = m_mapActionRepeats.Find( action.m_symUIEventName );
					if ( iMap == m_mapActionRepeats.InvalidIndex() )
					{
						// we got data that mapped to a repeat, but we were not privy to the events
						// that led here, so we should not repeat.
						return false;
					}
					else
					{
						ActionRepeatData_t &data = m_mapActionRepeats[iMap];

						Vector2D vRes;
						m_AxisRepeatCurve.Evaluate( clamp( (UIEngine()->GetCurrentFrameTime() - data.flFirst) / MOVE_REPEAT_CURVE_TIME, 0.0, 1.0 ), vRes );
						float flDelayTime = Lerp( vRes.y, MOVE_REPEAT_INTERVAL_START, MOVE_REPEAT_INTERVAL_END );

						if ( UIEngine()->GetCurrentFrameTime() - data.flLast < flDelayTime )
						{
							return true;
						}
						else
						{
							data.flLast = UIEngine()->GetCurrentFrameTime();
							nRepeats = ++data.nRepeats;
						}
					}
				}
			}

			bHandled = ActionEvent( action, msg.m_eSource, nRepeats );
		}
	}

	if ( !bHandled && msg.m_eInputType == k_eVRTouchPad )
	{
		const bool bFlipScrollDirection = true;

		if ( msg.m_VRTouchData.m_bFingerDown )
		{
//#define TOUCH_DRAG_MSG(...) Msg(__VA_ARGS__)
#define TOUCH_DRAG_MSG(...)
			const double k_flAffordanceDelta = 0.03f; // require this much pad movement before a scroll
			const double k_flMouseDragDelayS = 0.05f; // don't start a new drag until this much time has passed
			const float k_flPadOuterRingIgnore = 0.85f; // ignore samples larger than this from the pads as they have lots of noise at the edge
			const float k_flInitialFingerDownIgnore = 0.02f; // don't react to down samples until the finger has been down this long
			const int k_nMinVelocityForGrab = 1000; // switch from grab drag to flick mode if the move velocity is over this
			const float k_flMinTimeForGrab = 0.2f; // how much time should pass from a large movement to then allow grab mode to kick in
			const float k_flLinearMoveThreshold = 10.0f; // how far must they move a finger to count towards a haptics move
			const float k_flTickDistance = 200.0f; // how far must you move your finder to feel a tick
			const int k_nHapticsPulseDuration = 250; // the length/strength of the pulse

			TOUCH_DRAG_MSG( "%0.1f: finger down %0.2f (%0.2f,%0.2f)\n", UIEngine()->GetCurrentFrameTime(), msg.m_VRTouchData.m_flFingerDown, msg.m_VRTouchData.m_fValueXRaw, msg.m_VRTouchData.m_fValueYRaw );

			// filter out samples from right as the finger goes down as they are noisy
			if ( msg.m_VRTouchData.m_flFingerDown > k_flInitialFingerDownIgnore && (fabs( msg.m_VRTouchData.m_fValueXRaw ) < k_flPadOuterRingIgnore && fabs( msg.m_VRTouchData.m_fValueYRaw ) < k_flPadOuterRingIgnore) )
			{
				bool bStarted = false;
				if ( !m_ptrDragScroll.Get() )
				{
					IUIPanel *pScrollTarget = GetDragScrollablePanel( GetMouseHover() );
					TOUCH_DRAG_MSG( "%0.1f: checking hit test for %s , got %p\n", UIEngine()->GetCurrentFrameTime(), GetMouseHover() ?  GetMouseHover()->GetID() : "", pScrollTarget );

					// Check for a scrollable parent
					if ( pScrollTarget )
					{
						// Allow a small affordance for mouse wiggle, in space and time.
						//
						bool bScrollInProgess = pScrollTarget->BScrollInProgress();

						TOUCH_DRAG_MSG( "%0.1f: starting (%s) %0.2f %0.2f %0.2f\n", UIEngine()->GetCurrentFrameTime(), 
							bScrollInProgess ? "In progress" : "", msg.m_VRTouchData.m_flFingerDown, msg.m_VRTouchData.m_fValueXRaw - msg.m_VRTouchData.m_fValueXFirst, msg.m_VRTouchData.m_fValueYRaw - msg.m_VRTouchData.m_fValueYFirst );

						if ( bScrollInProgess
							|| msg.m_VRTouchData.m_flFingerDown > k_flMouseDragDelayS
							|| fabs( msg.m_VRTouchData.m_fValueXRaw - msg.m_VRTouchData.m_fValueXFirst ) > k_flAffordanceDelta	
							|| fabs( msg.m_VRTouchData.m_fValueYRaw - msg.m_VRTouchData.m_fValueYFirst ) > k_flAffordanceDelta
							)
						{
							if ( DispatchEvent( DragScrollStart(), pScrollTarget ) )
							{
								TOUCH_DRAG_MSG( "%0.1f: started scroll (%s %s)\n", UIEngine()->GetCurrentFrameTime(), bScrollInProgess ? "in progress" : "new", pScrollTarget->GetID() );
								m_ptrDragScroll = pScrollTarget;
								m_vrTouchEventLast = msg.m_VRTouchData;
								m_flScrollGrabTime = 0;
								if ( bScrollInProgess )
								{
									m_flScrollGrabTime = UIEngine()->GetCurrentFrameTime() + k_flMinTimeForGrab;
								}
								bStarted = true;
							}
						}
					}
				}

				// scale from VR trackpad space into our output window space
				float flScaledXPos = MapRange( msg.m_VRTouchData.m_fValueXRaw, -1.0f, 1.0f, 0.0f, 1.0f );
				flScaledXPos *= m_pWindow->GetWindowWidth();
				float flPrevScaledXPos = MapRange( m_vrTouchEventLast.m_fValueXRaw, -1.0f, 1.0f, 0.0f, 1.0f );
				flPrevScaledXPos *= m_pWindow->GetWindowWidth();

				float flScaledYPos = MapRange( msg.m_VRTouchData.m_fValueYRaw, -1.0f, 1.0f, 0.0f, 1.0f );
				flScaledYPos *= m_pWindow->GetWindowHeight();
				float flPrevScaledYPos = MapRange( m_vrTouchEventLast.m_fValueYRaw, -1.0f, 1.0f, 0.0f, 1.0f );
				flPrevScaledYPos *= m_pWindow->GetWindowHeight();

				if ( bFlipScrollDirection )
				{
					flScaledYPos *= -1.0f;
					flPrevScaledYPos *= -1.0f;
				}

				IUIPanel *pScroll = m_ptrDragScroll.Get();
				TOUCH_DRAG_MSG( "%0.1f: scrolling %0.2f %0.2f %p\n", UIEngine()->GetCurrentFrameTime(), flScaledYPos, flPrevScaledYPos, pScroll );
				if ( pScroll )
				{
					if ( bStarted )
					{
						m_velocityTracker.StartSampling( flScaledXPos, flScaledYPos, UIEngine()->GetCurrentFrameTime() );
					}

					if ( bStarted || (int)flScaledXPos != (int)flPrevScaledXPos || (int)flScaledYPos != (int)flPrevScaledYPos )
					{
						m_velocityTracker.AddSample( (int)flScaledXPos, (int)flScaledYPos );
						float flVelocityX, flVelocityY;
						m_velocityTracker.GetVelocities( flVelocityX, flVelocityY );
						TOUCH_DRAG_MSG( "%0.1f: scrolling %0.0f %0.0f %0.0f %0.0f\n", UIEngine()->GetCurrentFrameTime(), flScaledYPos, flPrevScaledYPos, flVelocityX, flVelocityY );
						if ( !bStarted && (fabs( flVelocityX ) < k_nMinVelocityForGrab && fabs( flVelocityY ) < k_nMinVelocityForGrab ) )
						{
							if ( UIEngine()->GetCurrentFrameTime() > m_flScrollGrabTime )
							{
								TOUCH_DRAG_MSG( "%0.1f: sending move\n", UIEngine()->GetCurrentFrameTime() );
								DispatchEvent( DragScrollMouseMove(), pScroll, (int)flScaledXPos, (int)flScaledYPos, (int)flPrevScaledXPos, (int)flPrevScaledYPos );
							}
						}
						else
						{
							m_flScrollGrabTime = UIEngine()->GetCurrentFrameTime() + k_flMinTimeForGrab;
						}

						// now update haptics
						float flDeltaX = flScaledXPos - flPrevScaledXPos;
						float flDeltaY = flScaledYPos - flPrevScaledYPos;
						float flMoveDist = sqrtf( flDeltaX*flDeltaX + flDeltaY*flDeltaY );
						if ( flMoveDist > k_flLinearMoveThreshold )
						{
							m_flVRTouchLinearMoveDistanceForHaptics += flMoveDist;

							if ( m_flVRTouchLinearMoveDistanceForHaptics > k_flTickDistance )
							{
								m_flVRTouchLinearMoveDistanceForHaptics = fmod( m_flVRTouchLinearMoveDistanceForHaptics, k_flTickDistance );

#ifndef SOURCE2_PANORAMA
								vrapi::VRSystem()->TriggerHapticPulse( msg.m_VRTouchData.m_nDeviceIndex, 0, k_nHapticsPulseDuration );
#endif
							}
						}
					}
				}
				m_vrTouchEventLast = msg.m_VRTouchData;
			} // if finger down for more then 0.1s and x in reasonable area
		}
		else
		{
			if ( m_ptrDragScroll.Get() )
			{
				float flPrevScaledXPos = MapRange( m_vrTouchEventLast.m_fValueXRaw, -1.0f, 1.0f, 0.0f, 1.0f );
				flPrevScaledXPos *= m_pWindow->GetWindowWidth();
				float flPrevScaledYPos = MapRange( m_vrTouchEventLast.m_fValueYRaw, -1.0f, 1.0f, 0.0f, 1.0f );
				if ( bFlipScrollDirection )
				{
					flPrevScaledYPos *= -1.0f;
				}
				flPrevScaledYPos *= m_pWindow->GetWindowHeight();

				float flVelocityX, flVelocityY;
				m_velocityTracker.GetVelocities( flVelocityX, flVelocityY );
				flVelocityY *= -1.0f;

				TOUCH_DRAG_MSG( "%0.1f: ended scroll %0.0f %0.4f\n", UIEngine()->GetCurrentFrameTime(), flPrevScaledYPos, flVelocityY );
				DispatchEvent( panorama::DragScrollEnd(), m_ptrDragScroll.Get(), (int)flPrevScaledXPos, (int)flPrevScaledYPos, flVelocityX / 2, flVelocityY / 2 );
				m_ptrDragScroll = nullptr;
				m_dragScrollMouseCode = MOUSE_INVALID;
			}
		}
	}

	return bHandled;
}


//-----------------------------------------------------------------------------
// Purpose: Manually fire a high level action event
//-----------------------------------------------------------------------------
bool CUIWindowInput::ActionEvent( InputAction_t action, EPanelEventSource_t eSource, int nRepeats )
{
	if( !action.m_symUIEventName.IsValid() && action.m_pJSAction.IsEmpty() )
		return true;

	// create an event from the action code. Should have the same name.
	if( action.m_symUIEventName.IsValid() )
	{	
		IUIEvent *pEvent = nullptr;
		if ( action.m_strParams.Length() )
		{
			const char* pchEventEnd;
			pEvent = UIEngine()->CreateEventFromString( m_ActionFocus.m_pFocus.Get(), CFmtStr( "%s%s", action.m_symUIEventName.String(), action.m_strParams.Get() ).Get(), &pchEventEnd );
		}
		else
		{
			pEvent = UIEngine()->CreateInputEventFromSymbol( action.m_symUIEventName, m_ActionFocus.m_pFocus.Get(), eSource, nRepeats );
		}
		if( !pEvent )
			return false;

		return UIEngine()->DispatchEvent( pEvent );
	}
	else
	{
		IUIPanel *pContextPanel = action.m_pJSActionContextPanel.Get();
		if ( pContextPanel )
		{
			v8::Isolate::Scope isolate_scope( UIEngineInternal()->GetV8Isolate() );
			v8::HandleScope handle_scope( UIEngineInternal()->GetV8Isolate() );

			v8::Handle<v8::Value> arguments[3];
			PanoramaTypeToV8Param( eSource, &arguments[0] );
			PanoramaTypeToV8Param( nRepeats, &arguments[1] );
			PanoramaTypeToV8Param( m_ActionFocus.m_pFocus.Get(), &arguments[2] );

			UIEngineInternal()->RunFunction( pContextPanel, &action.m_pJSAction, 3, arguments, false );
		}

		return true;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Access mouse position in surface coords
//-----------------------------------------------------------------------------
void CUIWindowInput::GetSurfaceMousePosition( float &x, float &y )
{
	AUTO_LOCK( m_MutexMousePos );
	x = m_flSurfaceMouseX;
	y = m_flSurfaceMouseY;
}


//-----------------------------------------------------------------------------
// Purpose: return if the cursor is currently showing
//-----------------------------------------------------------------------------
bool CUIWindowInput::BCursorVisible()
{
	return m_pWindow->BCursorVisible();
}


//-----------------------------------------------------------------------------
// Purpose: programatically wake up and reset timeout for mouse cursor
//-----------------------------------------------------------------------------
void CUIWindowInput::WakeupMouseCursor()
{
	m_cMouseMoveCount = MOUSE_MOVE_ACTIVE_COUNT + 1; // force the cursor on
	return m_pWindow->WakeupMouseCursor();
}

//-----------------------------------------------------------------------------
// Purpose: programatically wake up and reset timeout for mouse cursor
//-----------------------------------------------------------------------------
void CUIWindowInput::FadeOutCursorNow()
{
	m_cMouseMoveCount = 0;
	return m_pWindow->FadeOutCursorNow();
}


//-----------------------------------------------------------------------------
// Purpose: return if any gamepads are connected
//-----------------------------------------------------------------------------
int CUIWindowInput::GetNumGamepadsConnected()
{
	return m_pParent->GetNumGamepadsConnected();
}


//-----------------------------------------------------------------------------
// Purpose: return if any gamepads are connected since launch
//-----------------------------------------------------------------------------
bool CUIWindowInput::BWasGamepadConnectedThisSession()
{ 
	return m_pParent->BWasGamepadConnectedThisSession(); 
}


//-----------------------------------------------------------------------------
// Purpose: return if any gamepads are used since launch
//-----------------------------------------------------------------------------
bool CUIWindowInput::BWasGamepadUsedThisSession()
{ 
	return m_pParent->BWasGamepadUsedThisSession(); 
}

//-----------------------------------------------------------------------------
// Purpose: return if any gamepads are connected since launch
//-----------------------------------------------------------------------------
bool CUIWindowInput::BWasSteamControllerConnectedThisSession()
{ 
	return m_pParent->BWasSteamControllerConnectedThisSession(); 
}


//-----------------------------------------------------------------------------
// Purpose: return if any gamepads are used since launch
//-----------------------------------------------------------------------------
bool CUIWindowInput::BWasSteamControllerUsedThisSession()
{ 
	return m_pParent->BWasSteamControllerUsedThisSession(); 
}


//-----------------------------------------------------------------------------
// Purpose: Called when we receive a mouse changing position event, co-ords are OS level/client space
//-----------------------------------------------------------------------------
void CUIWindowInput::OnMouseMove( float flMouseX, float flMouseY )
{
	{
		AUTO_LOCK( m_MutexMousePos );

		// make sure the mouse really moved
		if ( flMouseX == m_nMouseX && flMouseY == m_nMouseY )
			return;

		float surfaceMouseX = flMouseX;
		float surfaceMouseY = flMouseY;
		m_pWindow->ConvertClientToSurfaceCoord( &surfaceMouseX, &surfaceMouseY );

		// save position in case someone wants later
		m_nMouseX = flMouseX;
		m_nMouseY = flMouseY;

		OnMouseMoveSurfaceCoords( surfaceMouseX, surfaceMouseY );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Mouse moved but with surface relative co-ords
//-----------------------------------------------------------------------------
void CUIWindowInput::OnMouseMoveSurfaceCoords( float flMouseX, float flMouseY )
{
	if ( flMouseX == m_flSurfaceMouseX && flMouseY == m_flSurfaceMouseY )
		return;

	Vector2D oldpos( m_flSurfaceMouseX, m_flSurfaceMouseY );
	Vector2D newpos( flMouseX, flMouseY );

	// we only want to count the mouse as active if it moves more than once, to catch the
	// mouse warp due to game exit case, so here we accrue a count of movements
	// Note the 3.0 pixel move is also here, this helps with mouse movement jitter and 
	// with the controller and didn't detract from the desktop mouse experience, but could 
	// be tweaked/removed perhaps
	if ( oldpos.DistTo( newpos ) > 3.0f && m_cMouseMoveCount <= MOUSE_MOVE_ACTIVE_COUNT )
		m_cMouseMoveCount++;


	{
		AUTO_LOCK( m_MutexMousePos );
		m_flSurfaceMouseX = flMouseX;
		m_flSurfaceMouseY = flMouseY;
	}

	UIEngine()->UpdateLastInputTime();

	// Now wait for the animation thread to hit test, will post back a HoverPanel event later
	if ( m_pWindow->GetUIRenderEngine() )
		m_pWindow->GetUIRenderEngine()->WakeThreads();

	// start timer to trigger message to show a tooltip if mouse doesn't move
	m_flTooltipDispatch = UIEngine()->GetCurrentFrameTime() + 0.5;
}


//-----------------------------------------------------------------------------
// Purpose: Called when the mouse leaves our window
//-----------------------------------------------------------------------------
void CUIWindowInput::OnMouseLeave()
{
	{
		AUTO_LOCK( m_MutexMousePos );
		m_flSurfaceMouseX = k_flInvalidSurfaceMouseCoord;
		m_flSurfaceMouseY = k_flInvalidSurfaceMouseCoord;
	}
	if ( m_pWindow->GetUIRenderEngine() )
		m_pWindow->GetUIRenderEngine()->WakeThreads();

	m_flTooltipDispatch = 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: Mark all mouse buttons as not down
//-----------------------------------------------------------------------------
void CUIWindowInput::ClearMouseDown()
{
	for ( MouseCode mouseCode = ( MouseCode )0; mouseCode < MOUSE_LAST; mouseCode = ( MouseCode )( mouseCode + 1 ) )
	{
		SetMouseDownPtr( nullptr, mouseCode );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Helper to set mouse down panel
//-----------------------------------------------------------------------------
void CUIWindowInput::SetMouseDownPtr( IUIPanel *pPanel, panorama::MouseCode mouseButton )
{
	if ( mouseButton == MOUSE_INVALID )
	{
		Assert( false );
		return;
	}

	MouseDownState_t &mouseDownState = m_MouseDownStates[ mouseButton ];

	IUIPanel *pOldMouseDown = mouseDownState.m_ptrMouseDown.Get();
	if ( pOldMouseDown )
		pOldMouseDown->RemoveStyleFlag( k_EStyleFlagActive );

	mouseDownState.m_ptrMouseDown = pPanel;
	m_ptrFocusOnMouseDown = GetInputFocus();
	mouseDownState.m_nMouseDownX = m_nMouseX;
	mouseDownState.m_nMouseDownY = m_nMouseY;
	mouseDownState.m_flMouseDownTime = UIEngine()->GetCurrentFrameTime();
	m_bCheckedForDrag = false;
	m_bCheckedForDragScroll = false;

	m_bScrollInProgressOnMouseDown = false;
	m_dragScrollMouseCode = MOUSE_INVALID;
	if ( m_bEnableDragScroll )
	{
		IUIPanel *pScrollTarget = GetDragScrollablePanel( pPanel );
		if ( pScrollTarget != NULL )
		{
			m_bScrollInProgressOnMouseDown = pScrollTarget->BScrollInProgress();
			m_dragScrollMouseCode = mouseButton;
		}
	}

	if ( pPanel )
	{
		if ( pPanel->GetMouseCanActivate() == k_EMouseCanActivateIfFocused )
		{
			mouseDownState.m_bCanActivateOnMouseUp = ( pPanel->BHasDescendantKeyFocus() || pPanel->BHasKeyFocus() );
		}
		else if ( pPanel->GetMouseCanActivate() == k_EMouseCanActivateIfParentFocused )
		{
			IUIPanel *pParent = pPanel->FindParentForMouseCanActivate();
			mouseDownState.m_bCanActivateOnMouseUp = false;
			if ( pParent )
				mouseDownState.m_bCanActivateOnMouseUp = ( pParent->BHasDescendantKeyFocus() || pParent->BHasKeyFocus() );
		}
		else if ( pPanel->GetMouseCanActivate() == k_EMouseCanActivateIfAnyParentFocused )
		{
			IUIPanel *pParent = pPanel->GetParent();
			mouseDownState.m_bCanActivateOnMouseUp = false;


			while ( pParent && !mouseDownState.m_bCanActivateOnMouseUp )
			{
				mouseDownState.m_bCanActivateOnMouseUp = ( pParent->BHasKeyFocus() );
				pParent = pParent->GetParent();
			}
		}
		else
		{
			Assert( pPanel->GetMouseCanActivate() == k_EMouseCanActivateUnfocused );
			mouseDownState.m_bCanActivateOnMouseUp = true;
		}

		pPanel->AddStyleFlag( k_EStyleFlagActive );
	}
	else
	{
		mouseDownState.m_bCanActivateOnMouseUp = false;
	}	
}


//-----------------------------------------------------------------------------
// Purpose: Called when we receive a mouse button down event
//-----------------------------------------------------------------------------
bool CUIWindowInput::OnMouseButtonDown( const MouseData_t &mouseData )
{
	return OnMouseClickInternal( mouseData, &IInputCapture::OnCapturedMouseButtonDown, &IUIPanelClient::OnMouseButtonDown );
}


//-----------------------------------------------------------------------------
// Purpose: Called when we receive a mouse button up event
//-----------------------------------------------------------------------------
bool CUIWindowInput::OnMouseButtonUp( const MouseData_t &mouseData, bool bSuppressActivateOnMouseUp )
{
	if ( mouseData.m_MouseCode == MOUSE_INVALID )
	{
		Assert( false );
		return false;
	}

	MouseDownState_t &mouseDownState = m_MouseDownStates[ mouseData.m_MouseCode ];

	IUIPanel *pMouseDown = mouseDownState.m_ptrMouseDown.Get();
	IUIPanel *pMouseOver = m_pWindow->GetUIRenderEngine()->HitTestCoordsAgainstLatestScreenspaceQuadCoordinates( mouseData.m_XPos, mouseData.m_YPos );
	IUIPanel *pMouseDrag = m_ptrMouseDragInitiate.Get();
	IUIPanel *pDragDisplay = m_ptrMouseDragDisplay.Get();

	// always send input. If pMouseDown is NULL, will only be sent to any input hooks
	bool bInputHandled = BSendInput( pMouseDown, mouseData, &IInputCapture::OnCapturedMouseButtonUp, &IUIPanelClient::OnMouseButtonUp, false );

	// If we were drag scrolling, then we're done with that.
	if ( mouseData.m_MouseCode == m_dragScrollMouseCode && m_ptrDragScroll.Get() )
	{
		DragScrollEnd();
	}
	// if panel handles mouse input, we are done
	else if ( mouseData.m_MouseCode == m_dragMouseCode && pDragDisplay != nullptr )
	{
		// was dragging
		if ( pDragDisplay )
		{
			pDragDisplay->SetHitTestEnabledTraverse( true );

			if ( m_bRemovePositionBeforeDragDrop )
			{
				pDragDisplay->AccessIUIStyle()->ClearPropertySetFromElement( CStylePropertyPosition::symbol );
			}
		}

		IUIPanel *pHover = GetMouseHover();
		if ( pHover )
		{
			DispatchEvent( DragDrop(), pHover, m_ptrMouseDragDisplay );
			DispatchEvent( DragLeave(), pHover, m_ptrMouseDragDisplay );
		}

		DispatchEvent( DragEnd(), pMouseDrag, m_ptrMouseDragDisplay );
		m_ptrMouseDragInitiate = NULL;
		m_ptrMouseDragDisplay = NULL;
		m_dragMouseCode = MOUSE_INVALID;
	}
	else if ( pMouseOver && pMouseDown && !bInputHandled )
	{
		// generate click event if mouse is over the down panel, or one of its descendants
		if ( pMouseDown == pMouseOver || pMouseOver->IsDescendantOf( pMouseDown ) )
		{
			bool bHandled = false;
			for( IUIPanel *pPanel = pMouseDown; pPanel != NULL; pPanel = pPanel->GetParent() )
			{
				if ( pPanel->BAcceptsInput() && pPanel->ClientPtr()->OnClick( pMouseDown, mouseData ) )
				{
					bHandled = true;
					break;
				}
			}

			// raise a selected event if click went unhandled
			if ( !bHandled )
			{
				switch ( mouseData.m_MouseCode )
				{
				case MOUSE_LEFT:
					if ( !bSuppressActivateOnMouseUp && ( pMouseDown->GetMouseCanActivate() == k_EMouseCanActivateUnfocused || mouseDownState.m_bCanActivateOnMouseUp ) )
					{
						bHandled = DispatchEvent( Activated(), pMouseDown, k_ePanelEventSourceMouse );
					}
					break;
				case MOUSE_RIGHT:
					bHandled = DispatchEvent( ContextMenu(), pMouseDown, k_ePanelEventSourceMouse );
					break;
				}
			}
		}
	}	
	else
	{
		if ( mouseData.m_MouseCode == MOUSE_LEFT && pMouseDown == NULL )
		{
			UISoundSystem()->PlaySound( "activation_change_fail", (IUIPanel*)NULL, k_ESoundType_Effects );
		}
	}

	// Could have actually shutdown in response to an event
	if( UIEngine()->BIsRunning() )
	{
		// during mouse down, hover is always set on the mouse down panel. Clear that now.
		SetMouseDownPtr( nullptr, mouseData.m_MouseCode );
		ProcessHoverData();
	}

	return bInputHandled;
}


//-----------------------------------------------------------------------------
// Purpose: Called when we receive a mouse button double click event
//-----------------------------------------------------------------------------
bool CUIWindowInput::OnMouseDoubleClick( const MouseData_t &mouseData )
{
	return OnMouseClickInternal( mouseData, &IInputCapture::OnCapturedMouseButtonDoubleClick, &IUIPanelClient::OnMouseButtonDoubleClick );
}


//-----------------------------------------------------------------------------
// Purpose: Called when we receive a mouse button double click event
//-----------------------------------------------------------------------------
bool CUIWindowInput::OnMouseTripleClick( const MouseData_t &mouseData )
{
	return OnMouseClickInternal( mouseData, &IInputCapture::OnCapturedMouseButtonTripleClick, &IUIPanelClient::OnMouseButtonTripleClick );
}


//-----------------------------------------------------------------------------
// Purpose: Called when we receive a mouse button click event
//-----------------------------------------------------------------------------
template < class T >
bool CUIWindowInput::OnMouseClickInternal( const MouseData_t &mouseData, bool (IInputCapture::*pCaptureFunc)(IUIPanel *pPanel, const T &data), bool (IUIPanelClient::*pPanelFunc)(const T &data) )
{
	IUIPanel *pMouseOver = m_pWindow->GetUIRenderEngine()->HitTestCoordsAgainstLatestScreenspaceQuadCoordinates( mouseData.m_XPos, mouseData.m_YPos );
	if ( !pMouseOver )
		return false;

	// need panel that wants mouse input
	IUIPanel *pMouseInput = FindInputPanel( pMouseOver );
	if ( pMouseInput && !pMouseInput->IsEnabled() )
	{
		pMouseInput = NULL;
	}

	// remember original action focus before sending input
	TopMostFocus_t origFocus = m_ActionFocus;

	// Cache this off in case the input handler deletes the panel
	CPanelPtr< IUIPanel > hMouseInput = pMouseInput;

	// always send input. If pMouseDown is NULL, will only be sent to any input hooks	
	bool bEventHandled = BSendInput( pMouseInput, mouseData, pCaptureFunc, pPanelFunc, false );

	if ( mouseData.m_MouseCode == MOUSE_INVALID )
	{
		Assert( false );
		return bEventHandled;
	}

	MouseDownState_t &mouseDownState = m_MouseDownStates[ mouseData.m_MouseCode ];
	mouseDownState.m_bMouseDownHandled = bEventHandled;

	// remember the panel where the mouse button went down so we can generate a click event if necessary
	pMouseInput = hMouseInput.Get();
	SetMouseDownPtr( pMouseInput, mouseData.m_MouseCode );

	if( bEventHandled )
		return true;

	// Only the main left/middle/right buttons change focus, 4/5/etc do not ever do that
	bool bPrimaryMouseButton = (mouseData.m_MouseCode == MOUSE_LEFT || mouseData.m_MouseCode == MOUSE_MIDDLE || mouseData.m_MouseCode == MOUSE_RIGHT);
	if ( pMouseInput && bPrimaryMouseButton )
	{
		if ( ( origFocus.m_pFocus == m_ActionFocus.m_pFocus && origFocus.m_pTopmost == m_ActionFocus.m_pTopmost ) || !bEventHandled )
		{
			// always set input focus if clicked panel accepts focus and doesn't currently have it. If the panel doesn't accept focus,
			// skip setting focus if the panel already has descendant focus so the code below doesn't move focus back to the default focus child
			if ( !pMouseInput->BHasKeyFocus() && (!pMouseInput->BHasDescendantKeyFocus() || pMouseInput->BAcceptsFocus()) )
			{
				bool bFocusChange = false;
				if ( pMouseInput->BFocusOnMouseDown() )
				{
					bFocusChange = pMouseInput->SetFocus();

					// don't chain input to parents on mouse down if panel can activate on up (accepts input, not focus and has on mouse activate set)
					// don't so will most likely move focus and won't receive proper mouse button up call that turns into activate
					if ( !bFocusChange && ( !mouseDownState.m_bCanActivateOnMouseUp || (!pMouseInput->BHasOnActivateEvent() && !pMouseInput->BHasOnMouseActivateEvent() ) ) )
					{
						IUIPanel *pParent = pMouseInput->GetParent();
						while( pParent )
						{
							if ( pParent->BFocusOnMouseDown() )
							{
								if ( pParent->SetFocus() )
								{
									bFocusChange = true;
									break;
								}
							}

							pParent = pParent->GetParent();
						}
					}

					// this sound commented out since it's now driven from js or css
//					if ( bFocusChange )
//						UISoundSystem()->PlaySound( "focus_change", (IUIPanel*)NULL, k_ESoundType_Effects, 0.85f );
				}
			}
			else
			{
				// if we skip setting focus on click because the panel already has descendant focus, still need to switch input context.
				SetInputFocusContext( pMouseInput );
			}
		}
		else
		{
			// If the event has been handled, then it may have done something like show a modal, show a context menu, etc, 
			// and we should not do the above code that tries to set focus in response to the click to the target if that's true.
		}
	}	

	return bEventHandled;
}


//-----------------------------------------------------------------------------
// Purpose: Called when we receive a mouse wheel event
//-----------------------------------------------------------------------------
bool CUIWindowInput::OnMouseWheel( const MouseData_t &mouseData )
{
	return BSendInput( m_pMouseOverInternal.Get(), mouseData, &IInputCapture::OnCapturedMouseWheel, &IUIPanelClient::OnMouseWheel, false );
}


//-----------------------------------------------------------------------------
// Purpose: Our top level window just got kb focus
//-----------------------------------------------------------------------------
void CUIWindowInput::GotWindowFocus()
{
	m_pParent->SetWindowInputFocus( this );
}


//-----------------------------------------------------------------------------
// Purpose: Our top level window just got kb focus
//-----------------------------------------------------------------------------
void CUIWindowInput::LostWindowFocus()
{
	m_pParent->LostWindowInputFocus( this );
}


//-----------------------------------------------------------------------------
// Purpose: Do we have kb focus?
//-----------------------------------------------------------------------------
bool CUIWindowInput::BHasWindowFocus()
{
	return m_pParent->BHasWindowFocus( this );
}


//-----------------------------------------------------------------------------
// Purpose: Fires event to panel to show a tooltip if configured
//-----------------------------------------------------------------------------
void CUIWindowInput::DispatchShowTooltip()
{
	IUIPanel *pTarget = m_pMouseOverInternal.Get();
	if ( pTarget )
		DispatchEvent( ShowTooltip(), pTarget );
}


//-----------------------------------------------------------------------------
// Purpose: Registers an interface to forward input events to for a specified panel
//-----------------------------------------------------------------------------
void CUIWindowInput::HookPanelInput( IUIPanel *pPanel, IInputCapture *pInputCapture )
{
	CPanelPtr< IUIPanel > ptrPanel( pPanel );
	int iMap = m_mapHookPanelInput.Find( ptrPanel );
	if ( iMap == m_mapHookPanelInput.InvalidIndex() )
		iMap = m_mapHookPanelInput.Insert( ptrPanel, new CUtlVector< IInputCapture * >() );

	CUtlVector< IInputCapture * > *pvec = m_mapHookPanelInput.Element( iMap );
	if ( pvec->HasElement( pInputCapture ) )
	{
		AssertMsg( false, "Capture interface already registered for this panel" );
		return;
	}

	pvec->AddToTail( pInputCapture );
}


//-----------------------------------------------------------------------------
// Purpose: deregisters an interface to forward input events to for a specified panel
//-----------------------------------------------------------------------------
void CUIWindowInput::RemovePanelInputHook( IUIPanel *pPanel, IInputCapture *pInputCapture )
{
	CPanelPtr< IUIPanel > ptrPanel( pPanel );
	int iMap = m_mapHookPanelInput.Find( ptrPanel );
	if ( iMap == m_mapHookPanelInput.InvalidIndex() )
	{
		AssertMsg( false, "Capture interface not registered for this panel" );
		return;
	}
	
	CUtlVector< IInputCapture * > *pvec = m_mapHookPanelInput.Element( iMap );
	int iVec = pvec->Find( pInputCapture );
	if ( iVec == pvec->InvalidIndex() )
	{
		AssertMsg( false, "Capture interface not registered for this panel" );
		return;
	}

	pvec->Remove( iVec );

	// if last hook, clean up map
	if ( pvec->Count() == 0 )
	{
		delete pvec;
		m_mapHookPanelInput.RemoveAt( iMap );
	}
}


//-----------------------------------------------------------------------------
// Purpose: return true if the mouse had input (moved, clicked, scrolled) since the last time you asked
//-----------------------------------------------------------------------------
bool CUIWindowInput::BWasMouseClickedOrScrolled()
{
	bool bMouseActive = m_bMouseActive;
	bMouseActive |= m_bMouseDown;
	m_bMouseActive = false;
	return bMouseActive;

}


//-----------------------------------------------------------------------------
// Purpose: return true if the mouse had input (moved, clicked, scrolled) since the last time you asked
//-----------------------------------------------------------------------------
bool CUIWindowInput::BWasKeyboardUsed()
{
	bool bKeyboardActive = m_bKeyboardActive;
	bKeyboardActive |= m_bKeyboardActive;
	m_bKeyboardActive = false;
	return bKeyboardActive;
}


//-----------------------------------------------------------------------------
// Purpose: return true if the mouse had input (moved, clicked, scrolled) since the last time you asked
//-----------------------------------------------------------------------------
bool CUIWindowInput::BWasMouseMoving()
{
	// require more than 1 move event to count as active due to moves along
	// this helps with warping of the mouse cursor due to games exiting and with noisy mouse input
	if ( m_cMouseMoveCount > MOUSE_MOVE_ACTIVE_COUNT )
		return true;
	return false;

}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
class CDragStartCallbacks: public IDragStartCallbacks
{
public:
	CDragStartCallbacks()
	{
		m_pPanel = NULL;
		m_nOffsetX = INT_MAX;
		m_nOffsetY = INT_MAX;
		m_bRemovePositionBeforeDrop = true;
	}

	void RegisterJS()
	{
		// Only register the type once
		if ( !UIEngine()->IsObjectTypeExposedToJavaScript( GetJSTypeName() ) )
		{
			CUtlDelegate< void() > del( this, &CDragStartCallbacks::JSRegisterFunc );
			CUtlAbstractDelegate absDel = del.GetAbstractDelegate();
			panorama::UIEngine()->ExposeObjectTypeToJavaScript( GetJSTypeName(), absDel );
		}
	}

	virtual void SetDisplayPanel( IUIPanel *pPanel ) OVERRIDE
	{
		m_pPanel = pPanel;
	}

	virtual IUIPanel *GetDisplayPanel() const OVERRIDE
	{
		return m_pPanel;
	}

	virtual const char *GetJSTypeName() OVERRIDE
	{
		return "DragCallbacks";
	}

	void JSRegisterFunc()
	{
		RegisterJSAccessor( "displayPanel", PANORAMA_DELEGATE( &CDragStartCallbacks::GetDisplayPanel ), PANORAMA_DELEGATE( &CDragStartCallbacks::SetDisplayPanel ) );
		RegisterJSAccessor( "offsetX", PANORAMA_DELEGATE( &CDragStartCallbacks::GetOffsetX ), PANORAMA_DELEGATE( &CDragStartCallbacks::SetOffsetX ) );
		RegisterJSAccessor( "offsetY", PANORAMA_DELEGATE( &CDragStartCallbacks::GetOffsetY ), PANORAMA_DELEGATE( &CDragStartCallbacks::SetOffsetY ) );
		RegisterJSAccessor( "removePositionBeforeDrop", PANORAMA_DELEGATE( &CDragStartCallbacks::ShouldRemovePositionBeforeDrop ), PANORAMA_DELEGATE( &CDragStartCallbacks::SetRemovePositionBeforeDrop ) );
	}

	virtual void SetOffsetX( int nOffset ) OVERRIDE { m_nOffsetX = nOffset; }
	virtual int GetOffsetX() const { return m_nOffsetX; }
	virtual void SetOffsetY( int nOffset ) OVERRIDE { m_nOffsetY = nOffset; }
	virtual int GetOffsetY() const { return m_nOffsetY; }

	virtual void SetRemovePositionBeforeDrop( bool bRemovePositionBeforeDrop ) { m_bRemovePositionBeforeDrop = bRemovePositionBeforeDrop; }
	virtual bool ShouldRemovePositionBeforeDrop() const { return m_bRemovePositionBeforeDrop; }

private:
	IUIPanel *m_pPanel;
	int m_nOffsetX;
	int m_nOffsetY;
	bool m_bRemovePositionBeforeDrop;
};


//-----------------------------------------------------------------------------
// Purpose: enable/disable dragging with a specific mouse button
//-----------------------------------------------------------------------------
void CUIWindowInput::SetDragDropEnabled( MouseCode code, bool bEnabled )
{
	if ( !bEnabled )
	{
		m_dragDropMouseCodes.FindAndRemove( code );
	}
	else if ( m_dragDropMouseCodes.Find( code ) == m_dragDropMouseCodes.InvalidIndex() )
	{
		m_dragDropMouseCodes.AddToTail( code );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Updates drag and drop state
//-----------------------------------------------------------------------------
void CUIWindowInput::UpdateDragDrop()
{
	if ( !BDragInProgress() )
	{
		for ( MouseCode mouseCode : m_dragDropMouseCodes )
		{
			MouseDownState_t &mouseDownState = m_MouseDownStates[ mouseCode ];
			IUIPanel *pDown = mouseDownState.m_ptrMouseDown.Get();

			// check if we need to start drag/drop
			if ( pDown != nullptr && !m_bCheckedForDrag )
			{
				// check if mouse has moved far enough to start drag/drop
				if ( abs( m_nMouseX - mouseDownState.m_nMouseDownX ) < 5 && abs( m_nMouseY - mouseDownState.m_nMouseDownY ) < 5 )
					continue;

				// start drag
				m_bCheckedForDrag = true;
				IUIPanel *pDrag = GetDraggablePanel( pDown );
				if ( pDrag )
				{
					// unfortunate to have to use GetPositionWithinAncestor
					float flXOffset = 0.0f;
					float flYOffset = 0.0f;
					pDrag->ClientPtr()->GetPositionWithinAncestor( NULL, &flXOffset, &flYOffset );
					m_nMouseDragOffsetX = m_nMouseX - ( int )flXOffset;
					m_nMouseDragOffsetY = m_nMouseY - ( int )flYOffset;

					CDragStartCallbacks dragStartCallbacks;
					dragStartCallbacks.RegisterJS();

					DispatchEvent( DragStart(), pDrag, &dragStartCallbacks );
					m_bDispatchDragEnter = true;

					if ( dragStartCallbacks.GetDisplayPanel() )
					{
						// make sure panel is top most panel so it can render outside original parents
						dragStartCallbacks.GetDisplayPanel()->SetParent( NULL );
						dragStartCallbacks.GetDisplayPanel()->SetHitTestEnabledTraverse( false );			// would prefer to not have to change hit test here
						m_ptrMouseDragDisplay = dragStartCallbacks.GetDisplayPanel();
						m_ptrMouseDragInitiate = pDrag;
					}

					if ( dragStartCallbacks.GetOffsetX() != INT_MAX )
					{
						m_nMouseDragOffsetX = dragStartCallbacks.GetOffsetX();
					}

					if ( dragStartCallbacks.GetOffsetY() != INT_MAX )
					{
						m_nMouseDragOffsetY = dragStartCallbacks.GetOffsetY();
					}

					m_bRemovePositionBeforeDragDrop = dragStartCallbacks.ShouldRemovePositionBeforeDrop();
					m_dragMouseCode = mouseCode;
				}
			}
		}
	}

	// move drag panel if set
	IUIPanel *pDragDisplay = m_ptrMouseDragDisplay.Get();
	if ( pDragDisplay )
	{
		float flLeft, flTop, flRight, flBottom;
		pDragDisplay->AccessIUIStyle()->GetMargin( pDragDisplay->GetActualLayoutWidth(), pDragDisplay->GetActualLayoutHeight(), flLeft, flTop, flRight, flBottom );

		CUILength x( m_nMouseX - flLeft - m_nMouseDragOffsetX, CUILength::k_EUILengthLength );
		CUILength y( m_nMouseY - flTop - m_nMouseDragOffsetY, CUILength::k_EUILengthLength );
		CUILength z( 0, CUILength::k_EUILengthLength );		
		pDragDisplay->AccessIUIStyle()->SetPositionWithoutTransition( x, y, z, true );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Track velocity of mouse in X and Y
//-----------------------------------------------------------------------------
CMouseVelocityTracker::CMouseVelocityTracker()
{
	m_flLastSample = 0;
	m_velocityX = m_velocityY = 0;
	m_lastX = 0;
	m_lastY = 0;
	m_flCurrentDeltaX = m_flCurrentDeltaY = 0.0f;
}


void CMouseVelocityTracker::StartSampling( int x, int y, double flStartTime )
{
	m_flLastSample = flStartTime;
	m_lastX = x;
	m_lastY = y;
	m_velocityX = m_velocityY = 0.0;
	m_flCurrentDeltaX = m_flCurrentDeltaY = 0.0f;

#if defined(DEBUG_MOUSE_VELOCITY)
	m_nSamples = 1;
	m_flFirstSample = m_flLastSample;
	m_startX = x;
	m_startY = y;

	Msg( "========================================================\n" );
	Msg( "Started Sampling (%d,%d) at %0.2f\n", m_startX, m_startY, m_flFirstSample );
#endif
}


void CMouseVelocityTracker::AddSample( int x, int y )
{
	m_flCurrentDeltaX += (x - m_lastX);
	m_flCurrentDeltaY += (y - m_lastY);
	m_lastX = x;
	m_lastY = y;

#if defined(DEBUG_MOUSE_VELOCITY)
	Msg( "( %i, %i ) => ( %0.2f, %0.2f )\n", m_lastX, m_lastY, m_flCurrentDeltaX, m_flCurrentDeltaY );
#endif

	UpdateVelocitySamples();
}


void CMouseVelocityTracker::UpdateVelocitySamples( bool bForce )
{
	double flNow = UIEngine()->GetCurrentFrameTime();
	double elapsedTime = flNow - m_flLastSample;

	if ( (fabs(m_flCurrentDeltaX) > 0 || fabs(m_flCurrentDeltaY) > 0) && ( elapsedTime > 0.020 || bForce ) )
	{
		if ( elapsedTime == 0.0f )
		{
			elapsedTime = 0.02;
		}

		m_flLastSample = flNow;

		float vX = m_flCurrentDeltaX / elapsedTime;
		float vY = m_flCurrentDeltaY / elapsedTime;
		m_flCurrentDeltaX = m_flCurrentDeltaY = 0.0f;

		// Keep rolling average across samples
		const double k_flSampleWeight = 0.6;
		m_velocityX = k_flSampleWeight * vX + (1 - k_flSampleWeight) * m_velocityX;
		m_velocityY = k_flSampleWeight * vY + (1 - k_flSampleWeight) * m_velocityY;

#if defined(DEBUG_MOUSE_VELOCITY)
		Msg( "\t vX = %f, vY = %f over %0.2fms\n", m_velocityX, m_velocityY, elapsedTime );
		m_nSamples++;
#endif
	}
}


void CMouseVelocityTracker::GetVelocities( float &flVelocityX, float &flVelocityY )
{
	UpdateVelocitySamples( true );

	flVelocityX = m_velocityX;
	flVelocityY = m_velocityY;

#if defined(DEBUG_MOUSE_VELOCITY)
	double flTotalElapsed = m_flLastSample - m_flFirstSample;
	Msg( "---------------------------\n" );
	Msg( "Fetching velocities, x = %f, y = %f\n", flVelocityX, flVelocityY );
	Msg( "Total samples: %i\n", m_nSamples );
	Msg( "Start position: %i, %i\n", m_startX, m_startY );
	Msg( "End position: %i, %i\n", m_lastX, m_lastY );
	Msg( "Sampling interval: %fms\n", 1000 * flTotalElapsed );
	float flNetVX = ( m_lastX - m_startX ) / flTotalElapsed;
	float flNetVY = ( m_lastY - m_startY ) / flTotalElapsed;
	Msg( "Net vX = %f, vY = %f\n", flNetVX, flNetVY );
	Msg( "---------------------------\n" );
	Msg( "========================================================\n" );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Updates drag scrolling state
//-----------------------------------------------------------------------------
void CUIWindowInput::UpdateDragScroll()
{
	if ( m_dragScrollMouseCode == MOUSE_INVALID )
		return;

	MouseDownState_t &mouseDownState = m_MouseDownStates[ m_dragScrollMouseCode ];
	IUIPanel *pDown = mouseDownState.m_ptrMouseDown.Get();
	if ( !pDown )
		return;

	// If someone handled the mouse down event, don't also
	// start a drag scroll.
	if ( mouseDownState.m_bMouseDownHandled )
		return;

	// If we're doing drag/drop, that wins
	if ( m_ptrMouseDragDisplay.Get() )
		return;

	bool bStarted = false;

	if ( !m_bCheckedForDragScroll )
	{
		// Check for a scrollable parent
		IUIPanel *pScrollTarget = GetDragScrollablePanel( pDown );
		if ( pScrollTarget )
		{
			// Allow a small affordance for mouse wiggle, in space and time.
			//
			// Any mouse down inside of a currently scrolling panel should stop
			// the scrolling even before the mouse moves
			bool bIsVRMode = pScrollTarget->GetParentWindow()->BIsVROverlay() || UIInputEngine()->GetActiveControllerType() == k_EActiveControllerType_VR;

			int nAffordancePixels = bIsVRMode ? g_ConVarDragScrollAffordanceVR.GetInt() : g_ConVarDragScrollAffordance.GetInt();
			double flMouseDragDelayS = bIsVRMode ? g_ConVarDragScrollMinTimeVR.GetFloat() : g_ConVarDragScrollMinTimeVR.GetFloat();
			double flNow = UIEngine()->GetCurrentFrameTime();
			if ( !m_bScrollInProgressOnMouseDown &&
				( flNow - mouseDownState.m_flMouseDownTime < flMouseDragDelayS || ( abs( m_nMouseX - mouseDownState.m_nMouseDownX ) < nAffordancePixels && abs( m_nMouseY - mouseDownState.m_nMouseDownY ) < nAffordancePixels ) ) )
			{
				return;
			}

			m_bCheckedForDragScroll = true;

			float flXOffset = 0.0f;
			float flYOffset = 0.0f;
			pScrollTarget->ClientPtr()->GetPositionWithinAncestor( NULL, &flXOffset, &flYOffset );
			m_nMouseDragOffsetX = m_nMouseX - ( int )flXOffset;
			m_nMouseDragOffsetY = m_nMouseY - ( int )flYOffset;

			if ( DispatchEvent( DragScrollStart(), pScrollTarget ) )
			{
				m_ptrDragScroll = pScrollTarget;
				m_velocityTracker.StartSampling( mouseDownState.m_nMouseDownX - flXOffset, mouseDownState.m_nMouseDownY - flYOffset, mouseDownState.m_flMouseDownTime );
				bStarted = true;
			}
		}
		else
		{
			m_bCheckedForDragScroll = true;
		}
	}

	IUIPanel *pScroll = m_ptrDragScroll.Get();
	if ( pScroll )
	{
		float flXOffset = 0.0f;
		float flYOffset = 0.0f;
		pScroll->ClientPtr()->GetPositionWithinAncestor( NULL, &flXOffset, &flYOffset );
		int nMouseDragOffsetX = m_nMouseX - ( int )flXOffset;
		int nMouseDragOffsetY = m_nMouseY - ( int )flYOffset;
		
		if ( nMouseDragOffsetX < 0 || nMouseDragOffsetX > pScroll->GetActualLayoutWidth() ||
			 nMouseDragOffsetY < 0 || nMouseDragOffsetY > pScroll->GetActualLayoutHeight() )
		{
			// Out of bounds, end drag scroll (treat as a mouse up)
			MouseData_t mouseData;
			mouseData.m_MouseCode = m_dragScrollMouseCode;
			mouseData.m_XPos = m_nMouseX;
			mouseData.m_YPos = m_nMouseY;
			OnMouseButtonUp( mouseData, true );
		}
		else
		{
			if ( bStarted || m_nMouseDragOffsetX != nMouseDragOffsetX || m_nMouseDragOffsetY != nMouseDragOffsetY )
			{
				DispatchEvent( DragScrollMouseMove(), pScroll, m_nMouseDragOffsetX, m_nMouseDragOffsetY, nMouseDragOffsetX, nMouseDragOffsetY );
		
				m_nMouseDragOffsetX = nMouseDragOffsetX;
				m_nMouseDragOffsetY = nMouseDragOffsetY;
			}

			m_velocityTracker.AddSample( m_nMouseDragOffsetX, m_nMouseDragOffsetY );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Complete a drag scroll
//-----------------------------------------------------------------------------
void CUIWindowInput::DragScrollEnd()
{
	// If we were drag scrolling, then we're done with that.
	if ( m_ptrDragScroll.Get() )
	{
		IUIPanel *pScroll = m_ptrDragScroll.Get();
		float flXOffset = 0.0f;
		float flYOffset = 0.0f;
		pScroll->ClientPtr()->GetPositionWithinAncestor( NULL, &flXOffset, &flYOffset );
		int nMouseDragOffsetX = clamp( m_nMouseX - ( int )flXOffset, 0, pScroll->GetActualLayoutWidth() );
		int nMouseDragOffsetY = clamp( m_nMouseY - ( int )flYOffset, 0, pScroll->GetActualLayoutHeight() );
		m_velocityTracker.AddSample( nMouseDragOffsetX, nMouseDragOffsetY );

		float flVelocityX, flVelocityY;
		float flVelocityMultiplier = pScroll->GetParentWindow()->BIsVROverlay() ? g_ConVarDragScrollVelocityMultiplierVR.GetFloat() : g_ConVarDragScrollVelocityMultiplier.GetFloat();

		m_velocityTracker.GetVelocities( flVelocityX, flVelocityY );

		DispatchEvent( panorama::DragScrollEnd(), m_ptrDragScroll.Get(), nMouseDragOffsetX, nMouseDragOffsetY, flVelocityX * flVelocityMultiplier , flVelocityY * flVelocityMultiplier  );
		m_ptrDragScroll = NULL;
		m_dragScrollMouseCode = MOUSE_INVALID;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Draw debug information regarding the top level window on the screen
//-----------------------------------------------------------------------------
void CUIWindowInput::DrawInputDebugText( const char *pchText, float x0, float y0 )
{
	CUIRenderEngine * pRenderEngine = m_pWindow->GetUIRenderEngine();
	if ( pRenderEngine )
	{
		const Color redColor( 255, 0, 0, 255 );
		const float flWidth = 300.0f;
		const float flHeight = 50.0f;
		const float flFontSize = 18.0f;

		pRenderEngine->DrawSolidColorTextRegion( pchText, "Arial Unicode MS", redColor.AsUint32(), flFontSize, k_flFloatNotSet,
			k_EFontWeightBold, k_EFontStyleNormal, k_ETextAlignLeft, k_ETextDecorationNone,
			false, false, 0, x0, y0, x0 + flWidth, y0 + flHeight );
	}
}

void CUIWindowInput::DrawInputTopMostFocus( const TopMostFocus_t &topmost, int nStackIndex, float x0, float y0 )
{
	static CFmtStr1024 s_fmtStr;
	const float flCol1 = x0 + 20.0f;
	const float flCol2 = flCol1 + 30.0f;
	const float flCol3 = flCol2 + 600.0f;

	// stack index
	s_fmtStr.sprintf( "%d:", nStackIndex );
	DrawInputDebugText( s_fmtStr.Get(), flCol1, y0 );

	// Top of input context
	IUIPanel *pTopPanel = topmost.m_pTopmost.Get();
	s_fmtStr.sprintf( "ctx: %s (0x%p)", ( ( pTopPanel && pTopPanel->BHasID() ) ? pTopPanel->GetID() : "--" ), pTopPanel );
	DrawInputDebugText( s_fmtStr.Get(), flCol2, y0 );

	// Focus
	IUIPanel *pFocusPanel = topmost.m_pFocus.Get();
	s_fmtStr.sprintf( "focus: %s (0x%p)", ( ( pFocusPanel && pFocusPanel->BHasID() ) ? pFocusPanel->GetID() : "--" ), pFocusPanel );
	DrawInputDebugText( s_fmtStr.Get(), flCol3, y0 );
}

void CUIWindowInput::DrawInputDebugInfo()
{
	if ( !s_convarPanoramaInputDebugInfo.GetBool() )
	{
		return;
	}

	static CFmtStr1024 s_fmtStr;
	const float flLineOffset = 20.0f;
	float flX = 100.0f;
	float flY = 100.0f;

	CUIRenderEngine * pRenderEngine = m_pWindow->GetUIRenderEngine();

	pRenderEngine->PushAnimationAndTransformContext( 0, 0, 0, 1000, 1000, NULL, true, false, k_EPanelRepaintFull, false, false, false, false, true, false, nullptr, false, false, false, false, k_EFractionalPixelPositionsDefault );

	DrawInputDebugText( "Input Context Stack:", flX, flY );
	flY += 10.0f;
	DrawInputDebugText( "--------------------------", flX, flY );
	flY += flLineOffset;

	int nStackIndex = 0;
	DrawInputTopMostFocus( m_ActionFocus, nStackIndex++, flX, flY );
	flY += flLineOffset;

	FOR_EACH_LL_BACK( m_stackPriorFocus, i )
	{
		DrawInputTopMostFocus( m_stackPriorFocus[i], nStackIndex++, flX, flY );
		flY += flLineOffset;
	}

	pRenderEngine->PopAnimationAndTransformContext( 0 );
}


#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: validate mem
//-----------------------------------------------------------------------------
void CUIWindowInput::Validate( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE();

	ValidateObj( m_vecptrMouseTrackingPanels );
	ValidateObj( m_vecTrackingPanelHandles );
	ValidateObj( m_vecTrackingResults );
	ValidateObj( m_mapActionRepeats );
	ValidateObj( m_stackPriorFocus );

	ValidateObj( m_mapHookPanelInput );
	FOR_EACH_MAP_FAST( m_mapHookPanelInput, i )
	{
		CUtlVector< IInputCapture * > *pVec = m_mapHookPanelInput.Element( i );
		ValidatePtr( pVec );
	}
}
#endif


#ifdef POSIX
struct vgui_to_virtual_t
{
	const KeyCode vguiKeyCode;
	const uint16 windowsKeyCode;
	const uint16 macKeyCode;
	const uint16 linuxKeyCode;
};

// Simulated scan code for modifier keys
#define OSX_LWINCMD_VSCAN	200
#define OSX_LSHIFT_VSCAN	201
#define OSX_LALT_VSCAN		202
#define OSX_LCONTROL_VSCAN	203
#define OSX_RWINCMD_VSCAN	210
#define OSX_RSHIFT_VSCAN	211
#define OSX_RALT_VSCAN		212
#define OSX_RCONTROL_VSCAN	213

static vgui_to_virtual_t keyMap[] =
{	
	{ panorama::KeyCode::KEY_NONE, 0, (uint16)-1, 0 }, // Map KEY_NONE to an invalid code -- 0 is valid on OSX
	{ panorama::KeyCode::KEY_0, '0', 29, 0x30 },
	{ panorama::KeyCode::KEY_1, '1', 18, 0x31 },
	{ panorama::KeyCode::KEY_2, '2', 19, 0x32 },
	{ panorama::KeyCode::KEY_3, '3', 20, 0x33 },
	{ panorama::KeyCode::KEY_4, '4', 21, 0x34 },
	{ panorama::KeyCode::KEY_5, '5', 23, 0x35 },
	{ panorama::KeyCode::KEY_6, '6', 22, 0x36 },
	{ panorama::KeyCode::KEY_7, '7', 26, 0x37 },
	{ panorama::KeyCode::KEY_8, '8', 28, 0x38 },
	{ panorama::KeyCode::KEY_9, '9', 25, 0x39 },
	{ panorama::KeyCode::KEY_A, 'A', 0,  0x61 },
	{ panorama::KeyCode::KEY_B, 'B', 11, 0x62 },
	{ panorama::KeyCode::KEY_C, 'C', 8,  0x63},
	{ panorama::KeyCode::KEY_D, 'D', 2,  0x64 },
	{ panorama::KeyCode::KEY_E, 'E', 14, 0x65 },
	{ panorama::KeyCode::KEY_F, 'F', 3,  0x66 },
	{ panorama::KeyCode::KEY_G, 'G', 5,  0x67 },
	{ panorama::KeyCode::KEY_H, 'H', 4,  0x68 },
	{ panorama::KeyCode::KEY_I, 'I', 34, 0x69 },
	{ panorama::KeyCode::KEY_J, 'J', 38, 0x6a },
	{ panorama::KeyCode::KEY_K, 'K', 40, 0x6b },
	{ panorama::KeyCode::KEY_L, 'L', 37, 0x6c },
	{ panorama::KeyCode::KEY_M, 'M', 46, 0x6d },
	{ panorama::KeyCode::KEY_N, 'N', 45, 0x6e },
	{ panorama::KeyCode::KEY_O, 'O', 31, 0x6f },
	{ panorama::KeyCode::KEY_P, 'P', 35, 0x70 },
	{ panorama::KeyCode::KEY_Q, 'Q', 12, 0x71 },
	{ panorama::KeyCode::KEY_R, 'R', 15, 0x72 },
	{ panorama::KeyCode::KEY_S, 'S', 1,  0x73 },
	{ panorama::KeyCode::KEY_T, 'T', 17, 0x74 },
	{ panorama::KeyCode::KEY_U, 'U', 32, 0x75 },
	{ panorama::KeyCode::KEY_V, 'V', 9,  0x76 },
	{ panorama::KeyCode::KEY_W, 'W', 13, 0x77 },
	{ panorama::KeyCode::KEY_X, 'X', 7,  0x78 },
	{ panorama::KeyCode::KEY_Y, 'Y', 16, 0x79 },
	{ panorama::KeyCode::KEY_Z, 'Z', 6,  0x7a },
	{ panorama::KeyCode::KEY_PAD_0,		0,		82,		0xff9e },
	{ panorama::KeyCode::KEY_PAD_1,		0,		83,		0xff9c },
	{ panorama::KeyCode::KEY_PAD_2,		0,		84,		0xff99 },
	{ panorama::KeyCode::KEY_PAD_3,		0,		85,		0xff9b },
	{ panorama::KeyCode::KEY_PAD_4,		0,		86,		0xff96 },
	{ panorama::KeyCode::KEY_PAD_5,		0,		87,		0xff9d },
	{ panorama::KeyCode::KEY_PAD_6,		0,		88,		0xff98 },
	{ panorama::KeyCode::KEY_PAD_7,		0,		89,		0xff95 },
	{ panorama::KeyCode::KEY_PAD_8,		0,		91,		0xff97 },
	{ panorama::KeyCode::KEY_PAD_9,		0,		92,		0xffa9 },
	{ panorama::KeyCode::KEY_PAD_DIVIDE,	0,		75,		0xffaf },
	{ panorama::KeyCode::KEY_PAD_MULTIPLY,	0,		67,		0xffaa },
	{ panorama::KeyCode::KEY_PAD_MINUS,	0,		78,		0xffad },
	{ panorama::KeyCode::KEY_PAD_PLUS,		0,		69,		0xffab },
	{ panorama::KeyCode::KEY_PAD_ENTER,	0,		76,		0xff8d },
	{ panorama::KeyCode::KEY_PAD_DECIMAL,	0,		65,		0xff9f },
	{ panorama::KeyCode::KEY_LBRACKET,		0,		33,		0x5b },
	{ panorama::KeyCode::KEY_RBRACKET,		0,		30,		0x5d },
	{ panorama::KeyCode::KEY_SEMICOLON,	0,		41,		0x3b },
	{ panorama::KeyCode::KEY_APOSTROPHE,	0,		39,		0x27 },
	{ panorama::KeyCode::KEY_BACKQUOTE,	0,		50,		0x60 },
	{ panorama::KeyCode::KEY_COMMA,		0,		43,		0x2c },
	{ panorama::KeyCode::KEY_PERIOD,		0,		47,		0x2e },
	{ panorama::KeyCode::KEY_SLASH,		0,		44,		0x2f },
	{ panorama::KeyCode::KEY_BACKSLASH,	0,		42,		0x5c },
	{ panorama::KeyCode::KEY_MINUS,		0,		27,		0x2d },
	{ panorama::KeyCode::KEY_EQUAL,		0,		81,		0x3d },
	{ panorama::KeyCode::KEY_ENTER,		0,		36,		0xff0d },
	{ panorama::KeyCode::KEY_SPACE,		0,		49,		0x20 },
	{ panorama::KeyCode::KEY_BACKSPACE,	0,		51,		0xff08 },
	{ panorama::KeyCode::KEY_TAB,			0,		48,		0xff09 },
	{ panorama::KeyCode::KEY_CAPSLOCK,		0,		57,		0xffe5 },
	{ panorama::KeyCode::KEY_NUMLOCK,		0,		71,		0xff7f },
	{ panorama::KeyCode::KEY_ESCAPE,		0,		53,		0xff1b },
	{ panorama::KeyCode::KEY_SCROLLLOCK,	0,		0xff,	0xff14 },
	{ panorama::KeyCode::KEY_INSERT,		0,		114,	0xff63},
	{ panorama::KeyCode::KEY_DELETE,		0,		117,	0xffff },
	{ panorama::KeyCode::KEY_HOME,			0,		115,	0xff50 },
	{ panorama::KeyCode::KEY_END,			0,		119,	0xff57 },
	{ panorama::KeyCode::KEY_PAGEUP,		0,		116,	0xff55 },
	{ panorama::KeyCode::KEY_PAGEDOWN,		0,		121,	0xff56 },
	{ panorama::KeyCode::KEY_BREAK,		0,		0xff,	0xff95 },
	{ panorama::KeyCode::KEY_RSHIFT,		0,		OSX_RSHIFT_VSCAN,	0xffe2 },
	{ panorama::KeyCode::KEY_LSHIFT,		0,		OSX_LSHIFT_VSCAN,	0xffe1 },
	{ panorama::KeyCode::KEY_RALT,			0,		OSX_RALT_VSCAN,		0xffea },
	{ panorama::KeyCode::KEY_LALT,			0,		OSX_LALT_VSCAN,		0xffe9 },
	{ panorama::KeyCode::KEY_RCONTROL,		0,		OSX_RCONTROL_VSCAN,	0xffe4 },
	{ panorama::KeyCode::KEY_LCONTROL,		0,		OSX_LCONTROL_VSCAN,	0xffe3 },
	{ panorama::KeyCode::KEY_LWIN,			0,		OSX_LWINCMD_VSCAN,	0xffeb },
	{ panorama::KeyCode::KEY_RWIN,			0,		OSX_RWINCMD_VSCAN,	0xffec },
	{ panorama::KeyCode::KEY_APP,			0,		110,	0 },
	{ panorama::KeyCode::KEY_UP,			0,		126,	0xff52 },
	{ panorama::KeyCode::KEY_LEFT,			0,		123,	0xff51 },
	{ panorama::KeyCode::KEY_DOWN,			0,		125,	0xff54 },
	{ panorama::KeyCode::KEY_RIGHT,		0,		124,	0xff53 },
	{ panorama::KeyCode::KEY_F1,			0,		122,	0xffbe },
	{ panorama::KeyCode::KEY_F2,			0,		120,	0xffbf },
	{ panorama::KeyCode::KEY_F3,			0,		99,		0xffc0 },
	{ panorama::KeyCode::KEY_F4,			0,		118,	0xffc1 },
	{ panorama::KeyCode::KEY_F5,			0,		96,		0xffc2 },
	{ panorama::KeyCode::KEY_F6,			0,		97,		0xffc3 },
	{ panorama::KeyCode::KEY_F7,			0,		98,		0xffc4 },
	{ panorama::KeyCode::KEY_F8,			0,		100,	0xffc5 },
	{ panorama::KeyCode::KEY_F9,			0,		101,	0xffc6 },
	{ panorama::KeyCode::KEY_F10,			0,		109,	0xffc7 },
	{ panorama::KeyCode::KEY_F11,			0,		103,	0xffc8 },
	{ panorama::KeyCode::KEY_F12,			0,		111,	0xffc9 }
	// bugbug stefan - insert MEDIA keys for Mac and Linux here
	// { KEY_VOLUME_MUTE, 0, 0, 0 },
	// { KEY_VOLUME_DOWN, 0, 0, 0 },
	// { KEY_VOLUME_UP,	0, 0, 0 },
	// { KEY_MEDIA_NEXT_TRACK, 0, 0, 0 },
	// { KEY_MEDIA_PREV_TRACK, 0, 0, 0 },
	// { KEY_MEDIA_STOP, 0, 0, 0 },
	// { KEY_MEDIA_PLAY_PAUSE, 0, 0, 0 },
};

static uint16 s_unKeyCodeToKey[ panorama::KeyCode::KEY_LAST ];
static KeyCode s_unVirtualKeyToKeyCode[ 256 ];
static bool s_bInitializedKeyMap = false;

void InitializeKeyMapIfNeeded()
{
	if ( !s_bInitializedKeyMap )
	{
		s_bInitializedKeyMap = true;
		V_memset( s_unKeyCodeToKey, 0x0, sizeof(s_unKeyCodeToKey) );
		
		for ( int i = 0; i < V_ARRAYSIZE(keyMap); i++ )
		{
#if defined(OSX)
			s_unKeyCodeToKey[ keyMap[i].vguiKeyCode ] = keyMap[i].macKeyCode;
			if ( keyMap[i].macKeyCode < V_ARRAYSIZE( s_unVirtualKeyToKeyCode ) )
				s_unVirtualKeyToKeyCode[ keyMap[i].macKeyCode ] = keyMap[i].vguiKeyCode;
#elif defined(LINUX)
			s_unKeyCodeToKey[ keyMap[i].vguiKeyCode ] = keyMap[i].linuxKeyCode;
			if ( ( keyMap[i].linuxKeyCode & 0x00ff ) < V_ARRAYSIZE( s_unVirtualKeyToKeyCode ) )
				s_unVirtualKeyToKeyCode[ keyMap[i].linuxKeyCode&0x00ff ] = keyMap[i].vguiKeyCode;
#else
#error
#endif
			
		}
	}
}
#endif


//-----------------------------------------------------------------------------
// Purpose: convert a windows wparam argument from key down/up to our internal codes
//-----------------------------------------------------------------------------
KeyCode CUIInputEngine::WindowsVKeyToKeyCode( uint16 inKey )
{
#ifdef WIN32
	if ( inKey >= 'A' && inKey <= 'Z' )
		return (KeyCode)(KEY_A + (inKey - 'A'));

	if ( inKey >= '0' && inKey <= '9' )
		return (KeyCode)(KEY_0 + (inKey - '0'));

	if ( inKey >= VK_F1 && inKey <= VK_F12 )
		return (KeyCode)(KEY_F1 + (inKey - VK_F1));

	if ( inKey >= VK_NUMPAD0 && inKey <= VK_NUMPAD9 )
		return (KeyCode)(KEY_PAD_0 + (inKey - VK_NUMPAD0));

	switch( inKey )
	{
	case VK_UP:
		return KEY_UP;
	case VK_DOWN:
		return KEY_DOWN;
	case VK_LEFT:
		return KEY_LEFT;
	case VK_RIGHT:
		return KEY_RIGHT;
	case VK_RETURN:
		return KEY_ENTER;
	case VK_TAB:
		return KEY_TAB;
	case VK_SHIFT:
		return KEY_RSHIFT;
	case VK_CONTROL:
		return KEY_LCONTROL;
	case VK_MENU:
		return KEY_LALT;
	case VK_PAUSE:
		return KEY_BREAK;
	case VK_CAPITAL:
		return KEY_CAPSLOCK;
	case VK_ESCAPE:
		return KEY_ESCAPE;
	case VK_SPACE:
		return KEY_SPACE;
	case VK_LWIN:
		return KEY_LWIN;
	case VK_RWIN:
		return KEY_RWIN;
	case VK_APPS:
		return KEY_APP;
	case VK_LSHIFT:
		return KEY_LSHIFT;
	case VK_RSHIFT:
		return KEY_RSHIFT;
	case VK_RCONTROL:
		return KEY_RCONTROL;
	case VK_LCONTROL:
		return KEY_LCONTROL;
	case VK_MULTIPLY:
		return KEY_PAD_MULTIPLY;
	case VK_ADD:
		return KEY_PAD_PLUS;
	case VK_SUBTRACT:
		return KEY_PAD_MINUS;
	case VK_DECIMAL:
		return KEY_PAD_DECIMAL;
	case VK_DIVIDE:
		return KEY_PAD_DIVIDE;
	case VK_NUMLOCK:
		return KEY_NUMLOCK;
	case VK_SCROLL:
		return KEY_SCROLLLOCK;
	case VK_PRIOR:
		return KEY_PAGEUP;
	case VK_NEXT:
		return KEY_PAGEDOWN;
	case VK_END:
		return KEY_END;
	case VK_HOME:
		return KEY_HOME;
	case VK_PRINT:
		return KEY_PRINTSCREEN;
	case VK_INSERT:
		return KEY_INSERT;
	case VK_DELETE:
		return KEY_DELETE;
	case VK_SNAPSHOT:
		return KEY_PRINTSCREEN;
	case VK_OEM_MINUS:
		return KEY_MINUS;
	case VK_OEM_3:
		return KEY_BACKQUOTE;
	case VK_BACK:
		return KEY_BACKSPACE;
	case VK_OEM_4:
		return KEY_LBRACKET;
	case VK_OEM_6:
		return KEY_RBRACKET;
	case VK_OEM_1:
		return KEY_SEMICOLON;
	case VK_OEM_7:
		return KEY_APOSTROPHE;
	case VK_OEM_COMMA:
		return KEY_COMMA;
	case VK_OEM_PERIOD:
		return KEY_PERIOD;
	case VK_OEM_2:
		return KEY_SLASH;
	case VK_OEM_5:
		return KEY_BACKSLASH;
	case VK_OEM_PLUS:
		return KEY_EQUAL;
	case VK_VOLUME_MUTE:
		return KEY_VOLUME_MUTE;
	case VK_VOLUME_DOWN:
		return KEY_VOLUME_DOWN;
	case VK_VOLUME_UP:
		return KEY_VOLUME_UP;
	case VK_MEDIA_NEXT_TRACK:
		return KEY_MEDIA_NEXT_TRACK;
	case VK_MEDIA_PREV_TRACK:
		return KEY_MEDIA_PREV_TRACK;
	case VK_MEDIA_STOP:
		return KEY_MEDIA_STOP;
	case VK_MEDIA_PLAY_PAUSE:
		return KEY_MEDIA_PLAY_PAUSE;
	default:
		return KEY_NONE;
	}
#elif defined(POSIX)
	InitializeKeyMapIfNeeded();
	return s_unVirtualKeyToKeyCode[ inKey&0x00ff ];
#endif	
}



//-----------------------------------------------------------------------------
// Purpose: convert a our internal codes to a windows VKEY argument
//-----------------------------------------------------------------------------
uint16 CUIInputEngine::KeyCodeToWindowsVKey( const KeyCode inKey )
{
#ifdef WIN32
	if ( inKey >= KEY_A && inKey <= KEY_Z )
		return (KeyCode)('A' + (inKey - KEY_A));
	
	if ( inKey >= KEY_0 && inKey <= KEY_9 )
		return (KeyCode)('0' + (inKey - KEY_0));

	if ( inKey >= KEY_F1 && inKey <= KEY_F12 )
		return (KeyCode)(VK_F1 + (inKey - KEY_F1));

	if ( inKey >= KEY_PAD_0 && inKey <= KEY_PAD_9 )
		return (KeyCode)(VK_NUMPAD0 + (inKey - KEY_PAD_0));

	switch( inKey )
	{
	case KEY_UP:
		return VK_UP;
	case KEY_DOWN:
		return VK_DOWN;
	case KEY_LEFT:
		return VK_LEFT;
	case KEY_RIGHT:
		return VK_RIGHT;
	case KEY_ENTER:
		return VK_RETURN;
	case KEY_TAB:
		return VK_TAB;
	case KEY_RSHIFT:
		return VK_RSHIFT;
	case KEY_RCONTROL:
		return VK_RCONTROL;
	case KEY_RALT:
		return VK_RMENU;
	case KEY_LALT:
		return VK_LMENU;
	case KEY_BREAK:
		return VK_PAUSE;
	case KEY_CAPSLOCK:
		return VK_CAPITAL;
	case KEY_ESCAPE:
		return VK_ESCAPE;
	case KEY_SPACE:
		return VK_SPACE;
	case KEY_LWIN:
		return VK_LWIN;
	case KEY_RWIN:
		return VK_RWIN;
	case KEY_APP:
		return VK_APPS;
	case KEY_LSHIFT:
		return VK_LSHIFT;
	case KEY_LCONTROL:
		return VK_LCONTROL;
	case KEY_PAD_MULTIPLY:
		return VK_MULTIPLY;
	case KEY_PAD_PLUS:
		return VK_ADD;
	case KEY_PAD_MINUS:
		return VK_SUBTRACT;
	case KEY_PAD_DECIMAL:
		return VK_DECIMAL;
	case KEY_PAD_DIVIDE:
		return VK_DIVIDE;
	case KEY_NUMLOCK:
		return VK_NUMLOCK;
	case KEY_SCROLLLOCK:
		return VK_SCROLL;
	case KEY_PAGEUP:
		return VK_PRIOR;
	case KEY_PAGEDOWN:
		return VK_NEXT;
	case KEY_END:
		return VK_END;
	case KEY_HOME:
		return VK_HOME;
	case KEY_PRINTSCREEN:
		return VK_PRINT;
	case KEY_INSERT:
		return VK_INSERT;
	case KEY_DELETE:
		return VK_DELETE;
	case KEY_BACKQUOTE:
		return VK_OEM_3;
	case KEY_BACKSPACE:
		return VK_BACK;
	case KEY_LBRACKET:
		return 0xdb;
	case KEY_RBRACKET:
		return 0xdd;
	case KEY_SEMICOLON:
		return 0xba;
	case KEY_APOSTROPHE:
		return 0xde;
	case KEY_COMMA:
		return 0xbc;
	case KEY_PERIOD:
		return 0xbe;
	case KEY_SLASH:
		return 0xbf;
	case KEY_BACKSLASH:
		return 0xdc;
	case KEY_EQUAL:
		return 0xbb;
	case KEY_VOLUME_MUTE:
		return VK_VOLUME_MUTE;
	case KEY_VOLUME_DOWN:
		return VK_VOLUME_DOWN;
	case KEY_VOLUME_UP:
		return VK_VOLUME_UP;
	case KEY_MEDIA_NEXT_TRACK:
		return VK_MEDIA_NEXT_TRACK;
	case KEY_MEDIA_PREV_TRACK:
		return VK_MEDIA_PREV_TRACK;
	case KEY_MEDIA_STOP:
		return VK_MEDIA_STOP;
	case KEY_MEDIA_PLAY_PAUSE:
		return VK_MEDIA_PLAY_PAUSE;
	case KEY_NONE:
	default:
		return 0;
	}
	
#elif defined(POSIX)
	InitializeKeyMapIfNeeded();
	return s_unKeyCodeToKey[ inKey ];
#endif
}

#ifdef SOURCE2_PANORAMA
ButtonCode_t CUIInputEngine::KeyCodeToButtonCode( const KeyCode inKey )
{
	if ( inKey >= KEY_A && inKey <= KEY_Z )
		return ( ButtonCode_t )( ButtonCode_t::KEY_A - KeyCode::KEY_A + inKey );
	
	if ( inKey >= KEY_0 && inKey <= KEY_9 )
		return ( ButtonCode_t )( ButtonCode_t::KEY_0 - KeyCode::KEY_0 + inKey );

	if ( inKey >= KEY_F1 && inKey <= KEY_F12 )
		return ( ButtonCode_t )( ButtonCode_t::KEY_F1 - KeyCode::KEY_F1 + inKey );

	if ( inKey >= KEY_PAD_0 && inKey <= KEY_PAD_9 )
		return ( ButtonCode_t )( ButtonCode_t::KEY_PAD_0 - KeyCode::KEY_PAD_0 + inKey );
	
	switch ( inKey )
	{
	case KeyCode::KEY_UP:
		return ButtonCode_t::KEY_UP;
	case KeyCode::KEY_DOWN:
		return ButtonCode_t::KEY_DOWN;
	case KeyCode::KEY_LEFT:
		return ButtonCode_t::KEY_LEFT;
	case KeyCode::KEY_RIGHT:
		return ButtonCode_t::KEY_RIGHT;
	case KeyCode::KEY_ENTER:
		return ButtonCode_t::KEY_ENTER;
	case KeyCode::KEY_TAB:
		return ButtonCode_t::KEY_TAB;
	case KeyCode::KEY_RSHIFT:
		return ButtonCode_t::KEY_RSHIFT;
	case KeyCode::KEY_RCONTROL:
		return ButtonCode_t::KEY_RCONTROL;
	case KeyCode::KEY_RALT:
		return ButtonCode_t::KEY_RALT;
	case KeyCode::KEY_LALT:
		return ButtonCode_t::KEY_LALT;
	case KeyCode::KEY_BREAK:
		return ButtonCode_t::KEY_BREAK;
	case KeyCode::KEY_CAPSLOCK:
		return ButtonCode_t::KEY_CAPSLOCK;
	case KeyCode::KEY_ESCAPE:
		return ButtonCode_t::KEY_ESCAPE;
	case KeyCode::KEY_SPACE:
		return ButtonCode_t::KEY_SPACE;
	case KeyCode::KEY_LWIN:
		return ButtonCode_t::KEY_LWIN;
	case KeyCode::KEY_RWIN:
		return ButtonCode_t::KEY_RWIN;
	case KeyCode::KEY_APP:
		return ButtonCode_t::KEY_APP;
	case KeyCode::KEY_LSHIFT:
		return ButtonCode_t::KEY_LSHIFT;
	case KeyCode::KEY_LCONTROL:
		return ButtonCode_t::KEY_LCONTROL;
	case KeyCode::KEY_PAD_MULTIPLY:
		return ButtonCode_t::KEY_PAD_MULTIPLY;
	case KeyCode::KEY_PAD_PLUS:
		return ButtonCode_t::KEY_PAD_PLUS;
	case KeyCode::KEY_PAD_MINUS:
		return ButtonCode_t::KEY_PAD_MINUS;
	case KeyCode::KEY_PAD_DECIMAL:
		return ButtonCode_t::KEY_PAD_DECIMAL;
	case KeyCode::KEY_PAD_DIVIDE:
		return ButtonCode_t::KEY_PAD_DIVIDE;
	case KeyCode::KEY_NUMLOCK:
		return ButtonCode_t::KEY_NUMLOCK;
	case KeyCode::KEY_SCROLLLOCK:
		return ButtonCode_t::KEY_SCROLLLOCK;
	case KeyCode::KEY_PAGEUP:
		return ButtonCode_t::KEY_PAGEUP;
	case KeyCode::KEY_PAGEDOWN:
		return ButtonCode_t::KEY_PAGEDOWN;
	case KeyCode::KEY_END:
		return ButtonCode_t::KEY_END;
	case KeyCode::KEY_HOME:
		return ButtonCode_t::KEY_HOME;
	case KeyCode::KEY_INSERT:
		return ButtonCode_t::KEY_INSERT;
	case KeyCode::KEY_DELETE:
		return ButtonCode_t::KEY_DELETE;
	case KeyCode::KEY_BACKQUOTE:
		return ButtonCode_t::KEY_BACKQUOTE;
	case KeyCode::KEY_BACKSPACE:
		return ButtonCode_t::KEY_BACKSPACE;
	case KeyCode::KEY_LBRACKET:
		return ButtonCode_t::KEY_LBRACKET;
	case KeyCode::KEY_RBRACKET:
		return ButtonCode_t::KEY_RBRACKET;
	case KeyCode::KEY_SEMICOLON:
		return ButtonCode_t::KEY_SEMICOLON;
	case KeyCode::KEY_APOSTROPHE:
		return ButtonCode_t::KEY_APOSTROPHE;
	case KeyCode::KEY_COMMA:
		return ButtonCode_t::KEY_COMMA;
	case KeyCode::KEY_PERIOD:
		return ButtonCode_t::KEY_PERIOD;
	case KeyCode::KEY_SLASH:
		return ButtonCode_t::KEY_SLASH;
	case KeyCode::KEY_BACKSLASH:
		return ButtonCode_t::KEY_BACKSLASH;
	case KeyCode::KEY_EQUAL:
		return ButtonCode_t::KEY_EQUAL;
	case KeyCode::KEY_NONE:
	default:
		return BUTTON_CODE_NONE;
	}
}

ButtonCode_t CUIInputEngine::MouseCodeToButtonCode( const MouseCode inKey )
{
	if ( inKey >= MOUSE_LAST || inKey <= MOUSE_INVALID )
	{
		return ButtonCode_t::BUTTON_CODE_INVALID;
	}
	return ( ButtonCode_t )( ButtonCode_t::MOUSE_FIRST + inKey );
}
#endif

//-----------------------------------------------------------------------------
// Purpose: enum defines for our input enums
//-----------------------------------------------------------------------------
ENUMSTRINGS_START( GamePadCode )
{ XK_BUTTON_UP,				"pad_up" },
{ XK_BUTTON_DOWN,			"pad_down" },
{ XK_BUTTON_LEFT,			"pad_left" },
{ XK_BUTTON_RIGHT,			"pad_right" },
{ XK_BUTTON_START,			"pad_start" },
{ XK_BUTTON_BACK,			"pad_back" },
{ XK_BUTTON_STICK1,			"pad_stick1" },
{ XK_BUTTON_STICK2,			"pad_stick2" },
{ XK_BUTTON_A,				"pad_a" },
{ XK_BUTTON_B,				"pad_b" },
{ XK_BUTTON_X,				"pad_x" },
{ XK_BUTTON_Y,				"pad_y" },
{ XK_BUTTON_LEFT_SHOULDER,	"pad_left_shoulder" },
{ XK_BUTTON_RIGHT_SHOULDER,	"pad_right_shoulder" },
{ XK_BUTTON_LTRIGGER,		"pad_ltrigger" },
{ XK_BUTTON_RTRIGGER,		"pad_rtrigger" },
{ XK_STICK1_UP,				"pad_stick1_up" },
{ XK_STICK1_DOWN,			"pad_stick1_down" },
{ XK_STICK1_LEFT,			"pad_stick1_left" },
{ XK_STICK1_RIGHT,			"pad_stick1_right" },
{ XK_STICK2_UP,				"pad_stick2_up" },
{ XK_STICK2_DOWN,			"pad_stick2_down" },
{ XK_STICK2_LEFT,			"pad_stick2_left" },
{ XK_STICK2_RIGHT,			"pad_stick2_right" },
{ XK_BUTTON_GUIDE,			"pad_guide" },
{ XK_BUTTON_LAST,			"pad_last" },
{ STEAM_LEFTPAD_UP,			"steampad_lpad_up" },
{ STEAM_LEFTPAD_DOWN,		"steampad_lpad_down" },
{ STEAM_LEFTPAD_LEFT,		"steampad_lpad_left" },
{ STEAM_LEFTPAD_RIGHT,		"steampad_lpad_right" },
{ STEAM_RIGHTPAD_UP,		"steampad_rpad_up" },
{ STEAM_RIGHTPAD_DOWN,		"steampad_rpad_down" },
{ STEAM_RIGHTPAD_LEFT,		"steampad_rpad_left" },
{ STEAM_RIGHTPAD_RIGHT,		"steampad_rpad_right" },
{ STEAM_BUTTON_LTRIGGER,	"steampad_ltrigger" },
{ STEAM_BUTTON_RTRIGGER,	"steampad_rtrigger" },
{ STEAM_BUTTON_LSHOULDER,	"steampad_lshoulder" },
{ STEAM_BUTTON_RSHOULDER,	"steampad_rshoulder" },
{ STEAM_BUTTON_GUIDE,		"steampad_guide" },
{ STEAM_BUTTON_LPAD_CLICKED,	"steampad_lpad_clicked" },
{ STEAM_BUTTON_RPAD_CLICKED,	"steampad_rpad_clicked" },
{ STEAM_BUTTON_LPAD_DBLTAPPED,	"steampad_lpad_doubletap" },
{ STEAM_BUTTON_RPAD_DBLTAPPED,	"steampad_rpad_doubletap" },
{ STEAM_BUTTON_A,			"steampad_a" },
{ STEAM_BUTTON_B,			"steampad_b" },
{ STEAM_BUTTON_X,			"steampad_x" },
{ STEAM_BUTTON_Y,			"steampad_y" },
{ STEAM_BUTTON_SELECT,		"steampad_select" },
{ STEAM_BUTTON_START,		"steampad_start" },
{ STEAM_BUTTON_LBACK,		"steampad_lback" },
{ STEAM_BUTTON_RBACK,		"steampad_rback" },
{ STEAM_BUTTON_DPAD_UP,		"steampad_dpad_up" },
{ STEAM_BUTTON_DPAD_RIGHT,	"steampad_dpad_right" },
{ STEAM_BUTTON_DPAD_DOWN,	"steampad_dpad_down" },
{ STEAM_BUTTON_DPAD_LEFT,	"steampad_dpad_left" },
{ STEAM_LEFTSTICK_UP,		"steampad_stick1_up" },
{ STEAM_LEFTSTICK_DOWN,		"steampad_stick1_down" },
{ STEAM_LEFTSTICK_LEFT,		"steampad_stick1_left" },
{ STEAM_LEFTSTICK_RIGHT,	"steampad_stick1_right" },
{ STEAM_BUTTON_LEFTSTICK_CLICKED,	"steampad_stick1_clicked" },
{ VR_BUTTON_PRIMARY_APP,	"vrpad_primary_guide" },
{ VR_BUTTON_PRIMARY_UP,		"vrpad_primary_up" },
{ VR_BUTTON_PRIMARY_DOWN,	"vrpad_primary_down" },
{ VR_BUTTON_PRIMARY_LEFT,	"vrpad_primary_left" },
{ VR_BUTTON_PRIMARY_RIGHT,	"vrpad_primary_right" },
{ VR_BUTTON_PRIMARY_TRIGGER, "vrpad_primary_trigger" },
{ VR_BUTTON_PRIMARY_GRIP,	"vrpad_primary_grip" },
{ VR_BUTTON_SECONDARY_APP,	"vrpad_secondary_guide" },
{ VR_BUTTON_SECONDARY_UP, "	vrpad_secondary_up" },
{ VR_BUTTON_SECONDARY_DOWN,	"vrpad_secondary_down" },
{ VR_BUTTON_SECONDARY_LEFT, "vrpad_secondary_left" },
{ VR_BUTTON_SECONDARY_RIGHT, "vrpad_secondary_right" },
{ VR_BUTTON_SECONDARY_TRIGGER, "vrpad_secondary_trigger" },
{ VR_BUTTON_SECONDARY_GRIP, "vrpad_secondary_grip" },

ENUMSTRINGS_REVERSE( GamePadCode, XK_NULL )

ENUMSTRINGS_START( MouseCode )
{ panorama::MouseCode::MOUSE_INVALID,		"mouse_invalid" },
{ panorama::MouseCode::MOUSE_LEFT,			"mouse_left" },
{ panorama::MouseCode::MOUSE_RIGHT,			"mouse_right" },
{ panorama::MouseCode::MOUSE_MIDDLE,			"mouse_middle" },
{ panorama::MouseCode::MOUSE_4,				"mouse_4" },
{ panorama::MouseCode::MOUSE_5,				"mouse_5" },
ENUMSTRINGS_REVERSE( MouseCode, MOUSE_INVALID )


ENUMSTRINGS_START( KeyCode )
{ panorama::KeyCode::KEY_DOWN,				"key_down" },
{ panorama::KeyCode::KEY_UP,				"key_up" },
{ panorama::KeyCode::KEY_LEFT,				"key_left" },
{ panorama::KeyCode::KEY_RIGHT,			"key_right" },
{ panorama::KeyCode::KEY_1,				"key_1" },
{ panorama::KeyCode::KEY_2,				"key_2" },
{ panorama::KeyCode::KEY_3,				"key_3" },
{ panorama::KeyCode::KEY_4,				"key_4" },
{ panorama::KeyCode::KEY_5,				"key_5" },
{ panorama::KeyCode::KEY_6,				"key_6" },
{ panorama::KeyCode::KEY_7,				"key_7" },
{ panorama::KeyCode::KEY_8,				"key_8" },
{ panorama::KeyCode::KEY_9,				"key_9" },
{ panorama::KeyCode::KEY_0,				"key_0" },
{ panorama::KeyCode::KEY_A,				"key_a" },
{ panorama::KeyCode::KEY_B,				"key_b" },
{ panorama::KeyCode::KEY_C,				"key_c" },
{ panorama::KeyCode::KEY_D,				"key_d" },
{ panorama::KeyCode::KEY_E,				"key_e" },
{ panorama::KeyCode::KEY_F,				"key_f" },
{ panorama::KeyCode::KEY_G,				"key_g" },
{ panorama::KeyCode::KEY_H,				"key_h" },
{ panorama::KeyCode::KEY_I,				"key_i" },
{ panorama::KeyCode::KEY_J,				"key_j" },
{ panorama::KeyCode::KEY_K,				"key_k" },
{ panorama::KeyCode::KEY_L,				"key_l" },
{ panorama::KeyCode::KEY_M,				"key_m" },
{ panorama::KeyCode::KEY_N,				"key_n" },
{ panorama::KeyCode::KEY_O,				"key_o" },
{ panorama::KeyCode::KEY_P,				"key_p" },
{ panorama::KeyCode::KEY_Q,				"key_q" },
{ panorama::KeyCode::KEY_R,				"key_r" },
{ panorama::KeyCode::KEY_S,				"key_s" },
{ panorama::KeyCode::KEY_T,				"key_t" },
{ panorama::KeyCode::KEY_U,				"key_u" },
{ panorama::KeyCode::KEY_V,				"key_v" },
{ panorama::KeyCode::KEY_W,				"key_w" },
{ panorama::KeyCode::KEY_X,				"key_x" },
{ panorama::KeyCode::KEY_Y,				"key_y" },
{ panorama::KeyCode::KEY_Z,				"key_z" },
{ panorama::KeyCode::KEY_LALT,				"key_lalt" },
{ panorama::KeyCode::KEY_RALT,				"key_ralt" },
{ panorama::KeyCode::KEY_LWIN,				"key_lwin" },
{ panorama::KeyCode::KEY_RWIN,				"key_rwin" },
{ panorama::KeyCode::KEY_HOME,				"key_home" },
{ panorama::KeyCode::KEY_LSHIFT,			"key_lshift" },
{ panorama::KeyCode::KEY_RSHIFT,			"key_rshift" },
{ panorama::KeyCode::KEY_LCONTROL,			"key_lcontrol" },
{ panorama::KeyCode::KEY_RCONTROL,			"key_rcontrol" },
{ panorama::KeyCode::KEY_F1,				"key_f1" },
{ panorama::KeyCode::KEY_F2,				"key_f2" },
{ panorama::KeyCode::KEY_F3,				"key_f3" },
{ panorama::KeyCode::KEY_F4,				"key_f4" },
{ panorama::KeyCode::KEY_F5,				"key_f5" },
{ panorama::KeyCode::KEY_F6,				"key_f6" },
{ panorama::KeyCode::KEY_F7,				"key_f7" },
{ panorama::KeyCode::KEY_F8,				"key_f8" },
{ panorama::KeyCode::KEY_F9,				"key_f9" },
{ panorama::KeyCode::KEY_F10,				"key_f10" },
{ panorama::KeyCode::KEY_F11,				"key_f11" },
{ panorama::KeyCode::KEY_F12,				"key_f12" },
{ panorama::KeyCode::KEY_PAD_1,			"key_pad_1" },
{ panorama::KeyCode::KEY_PAD_2,			"key_pad_2" },
{ panorama::KeyCode::KEY_PAD_3,			"key_pad_3" },
{ panorama::KeyCode::KEY_PAD_4,			"key_pad_4" },
{ panorama::KeyCode::KEY_PAD_5,			"key_pad_5" },
{ panorama::KeyCode::KEY_PAD_6,			"key_pad_6" },
{ panorama::KeyCode::KEY_PAD_7,			"key_pad_7" },
{ panorama::KeyCode::KEY_PAD_8,			"key_pad_8" },
{ panorama::KeyCode::KEY_PAD_9,			"key_pad_9" },
{ panorama::KeyCode::KEY_PAD_0,			"key_pad_0" },
{ panorama::KeyCode::KEY_BACKQUOTE,		"key_backquote" },
{ panorama::KeyCode::KEY_PAD_ENTER,		"key_pad_enter" },
{ panorama::KeyCode::KEY_PAGEUP,			"key_pageup" },
{ panorama::KeyCode::KEY_PAGEDOWN,			"key_pagedown" },
{ panorama::KeyCode::KEY_PAD_DIVIDE,		"key_pad_divide" },
{ panorama::KeyCode::KEY_PAD_MULTIPLY,		"key_pad_multiply" },
{ panorama::KeyCode::KEY_PAD_MINUS,		"key_pad_minus" },
{ panorama::KeyCode::KEY_PAD_PLUS,			"key_pad_plus" },
{ panorama::KeyCode::KEY_PAD_DECIMAL,		"key_pad_decimal" },
{ panorama::KeyCode::KEY_LBRACKET,			"key_lbracket" },
{ panorama::KeyCode::KEY_RBRACKET,			"key_rbracket" },
{ panorama::KeyCode::KEY_SEMICOLON,		"key_semicolon" },
{ panorama::KeyCode::KEY_APOSTROPHE,		"key_apostrophe" },
{ panorama::KeyCode::KEY_COMMA,			"key_comma" },
{ panorama::KeyCode::KEY_PERIOD,			"key_period" },
{ panorama::KeyCode::KEY_SLASH,			"key_slash" },
{ panorama::KeyCode::KEY_BACKSLASH,		"key_backslash" },
{ panorama::KeyCode::KEY_MINUS,			"key_minus" },
{ panorama::KeyCode::KEY_EQUAL,			"key_equal" },
{ panorama::KeyCode::KEY_ENTER,			"key_enter" },
{ panorama::KeyCode::KEY_SPACE,			"key_space" },
{ panorama::KeyCode::KEY_BACKSPACE,		"key_backspace" },
{ panorama::KeyCode::KEY_TAB,				"key_tab" },
{ panorama::KeyCode::KEY_CAPSLOCK,			"key_capslock" },
{ panorama::KeyCode::KEY_NUMLOCK,			"key_numlock" },
{ panorama::KeyCode::KEY_ESCAPE,			"key_escape" },
{ panorama::KeyCode::KEY_SCROLLLOCK,		"key_scrolllock" },
{ panorama::KeyCode::KEY_INSERT,			"key_insert" },
{ panorama::KeyCode::KEY_DELETE,			"key_delete" },
{ panorama::KeyCode::KEY_END,				"key_end" },
{ panorama::KeyCode::KEY_BREAK,			"key_break" },
{ panorama::KeyCode::KEY_APP,				"key_app" },
{ panorama::KeyCode::KEY_CAPSLOCKTOGGLE,	"key_capslocktoggle" },
{ panorama::KeyCode::KEY_NUMLOCKTOGGLE,	"key_numlocktoggle" },
{ panorama::KeyCode::KEY_SCROLLLOCKTOGGLE, "key_scrolllocktoggle" },
{ panorama::KeyCode::KEY_VOLUME_MUTE,		"key_volume_mute" },
{ panorama::KeyCode::KEY_VOLUME_DOWN,		"key_volume_down" },
{ panorama::KeyCode::KEY_VOLUME_UP,		"key_volume_up" },
{ panorama::KeyCode::KEY_MEDIA_NEXT_TRACK,	"key_media_next_track" },
{ panorama::KeyCode::KEY_MEDIA_PREV_TRACK,	"key_media_prev_track" },
{ panorama::KeyCode::KEY_MEDIA_STOP,		"key_media_stop" },
{ panorama::KeyCode::KEY_MEDIA_PLAY_PAUSE,	"key_media_play_pause" },
ENUMSTRINGS_REVERSE( KeyCode, panorama::KeyCode::KEY_NONE )


// Get gamepad code value from textual name for config files, event code, etc
panorama::GamePadCode CUIInputEngine::GamePadCodeFromName( const char * pchGamePadCode )
{
	return ::GamePadCodeFromName( pchGamePadCode );
}
