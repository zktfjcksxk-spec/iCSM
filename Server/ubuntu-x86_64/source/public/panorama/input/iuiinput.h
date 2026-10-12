//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//
#ifndef IUIINPUT_H
#define IUIINPUT_H

#if defined(_WIN32) || defined(SOURCE2_PANORAMA)
#pragma once
#endif

#include "keycodes.h"
#include "mousecodes.h"
#include "gamepadcodes.h"
#include "tier0/platform.h"
#include "tier1/utldelegate.h"
#include "../controls/panelhandle.h"
#include "../uieventcodes.h"
#include "../iuiengine.h"

#ifdef SOURCE2_PANORAMA
#include "inputsystem/ButtonCode.h"
#endif

const int k_flInvalidSurfaceMouseCoord = -32000;

namespace panorama
{

class CPanel2D;
class IImageSource;
class IUISettings;

// the classes of input we understand
enum EInputType
{
	k_eInputNone		= 0,
	k_eKeyDown			= 1, // raw down press
	k_eKeyUp			= 2, // raw release
	k_eKeyChar			= 3, // composited text entry from the OS
	k_eMouseDown		= 4,
	k_eMouseUp			= 5,
	k_eMouseMove		= 6,
	k_eMouseDoubleClick	= 7,
	k_eMouseTripleClick	= 8,
	k_eMouseWheel		= 9,
	k_eMouseEnter		= 10,
	k_eMouseLeave		= 11,
	k_eGamePadDown		= 12,
	k_eGamePadUp		= 13,
	k_eGamePadAnalog	= 14,
	k_eVRTouchPad		= 15,

	k_eOverlayCommand	= 5000,

	k_eInputTypeMaxRange = 0xFFFFFFFF // force size to 32 bits
};


enum EActiveControllerType
{
	k_EActiveControllerType_None,		// we haven't seen any input yet
	k_EActiveControllerType_KBMouse,
	k_EActiveControllerType_XInput,
	k_EActiveControllerType_Steam,
	k_EActiveControllerType_VR,
};

inline bool BIsControllerActiveControllerType( EActiveControllerType eActiveControllerType )
{
	return eActiveControllerType == k_EActiveControllerType_XInput
		|| eActiveControllerType == k_EActiveControllerType_Steam;
}

//
// structs container input type specific data
//
struct KeyData_t
{
	EPanelEventSource_t m_eSource; // this needs to be the first field of the struct, since it's unioned in InputMessage_t

	KeyCode m_KeyCode;
	uint8 m_RepeatCount;
	bool m_bFirstDown; // is this the first time this key was pressed
	uchar32 m_UniChar; // unicode equivalent of this key
	uint32 m_Modifiers; // alt, ctrl, etc held down?
};


struct MouseData_t
{
	EPanelEventSource_t m_eSource; // this needs to be the first field of the struct, since it's unioned in InputMessage_t

	MouseCode m_MouseCode;
	uint32 m_Modifiers;
	uint8 m_RepeatCount;
	int m_Delta;
	float m_XPos;
	float m_YPos;
};

struct GamePadData_t
{
	EPanelEventSource_t m_eSource; // this needs to be the first field of the struct, since it's unioned in InputMessage_t

	GamePadCode m_GamePadCode;
	uint8 m_RepeatCount; // home many times in a row we have had this button down
	float m_fValue; // the analog value of the deflection if an axis

	// if an analog event then these two are set for each axis value
	float m_fValueX; // the analog value of the deflection along horizontal
	float m_fValueY; // the analog value of the deflection along vertical

	// For Steam controller analog events this value will indicate the time the users finger went down
	float m_flFingerDown;

	// For Steam controller analog events these values indicate the starting finger position (so you can do some basic swipe stuff)
	float m_fValueXFirst;
	float m_fValueYFirst;
	
	// For Steam controller analog events this is the raw sampled coordinate without deadzoning
	float m_fValueXRaw;
	float m_fValueYRaw;
};

struct VRTouchEvent_t
{
	EPanelEventSource_t m_eSource; // this needs to be the first field of the struct, since it's unioned in InputMessage_t

	// is this the main (i.e laser pointer) controller or another one
	bool m_bPrimaryController;
	// the VR api device index for this controller
	uint32 m_nDeviceIndex;

	// is the pad touched right now, you will get one scroll after release
	bool m_bFingerDown;
	// how many seconds the pad has been touched
	float m_flFingerDown;

	// the starting finger position (so you can do some basic swipe stuff)
	float m_fValueXFirst;
	float m_fValueYFirst;

	// the raw sampled coordinate without deadzoning
	float m_fValueXRaw;
	float m_fValueYRaw;
};

struct InputMessage_t
{
	EInputType m_eInputType;
	float m_flInputTime;
	union 
	{
		EPanelEventSource_t m_eSource; // where did this event originate? this is the first field of the other *Data_t structs below.
		KeyData_t m_KeyData;
		MouseData_t m_MouseData;
		GamePadData_t m_GamePadData;
		VRTouchEvent_t m_VRTouchData;
	};
};


//
// A combination of event type and specific code data to mean an input event, like on key down of the A key
//
struct ActionInput_t
{
	ActionInput_t() 
	{ 
		m_InputType = k_eInputNone; 
		m_Data.m_KeyCode = KEY_NONE; 
		m_unModifiers = MODIFIER_NONE;
	}
	
	explicit ActionInput_t( EInputType type, KeyCode code, uint32 unModifiers, const char *pchNamespace ) 
	{ 
		m_InputType = type; 
		m_Data.m_KeyCode = code; 
		m_unModifiers = unModifiers;
		if ( pchNamespace && pchNamespace[0] )
			m_symNameSpace = pchNamespace;
	}
	
	explicit ActionInput_t( EInputType type, GamePadCode code, const char *pchNamespace ) 
	{ 
		m_InputType = type; 
		m_Data.m_GamePadCode = code; 
		m_unModifiers = MODIFIER_NONE;
		if ( pchNamespace && pchNamespace[0] )
			m_symNameSpace = pchNamespace;
	}
	
	explicit ActionInput_t( EInputType type, MouseCode code, const char *pchNamespace ) 
	{ 
		m_InputType = type; 
		m_Data.m_MouseCode = code; 
		m_unModifiers = MODIFIER_NONE;
		if ( pchNamespace && pchNamespace[0] )
			m_symNameSpace = pchNamespace;
	}
	
	ActionInput_t( InputMessage_t &msg, const char *pchNamespace )
	{
		m_InputType = msg.m_eInputType;
		if ( pchNamespace && pchNamespace[0] )
			m_symNameSpace = pchNamespace;
		m_unModifiers = MODIFIER_NONE;
		switch ( msg.m_eInputType )
		{
		default:
			AssertMsg( false, "Unknown input type to ActionInput_t( InputMessage_t )" );
			break;
		case k_eKeyDown:
		case k_eKeyUp:
		case k_eKeyChar:
			m_Data.m_KeyCode = msg.m_KeyData.m_KeyCode;
			m_unModifiers = msg.m_KeyData.m_Modifiers;
			break;
		case k_eMouseDown:
		case k_eMouseDoubleClick:
		case k_eMouseTripleClick:
		case k_eMouseUp:
		case k_eMouseMove:
		case k_eMouseWheel:
			m_Data.m_MouseCode = msg.m_MouseData.m_MouseCode;
			m_unModifiers = msg.m_MouseData.m_Modifiers;
			break;
		case k_eMouseEnter:
		case k_eMouseLeave:
			break;
		case k_eGamePadDown:
		case k_eGamePadUp:
		case k_eGamePadAnalog:
			m_Data.m_GamePadCode = msg.m_GamePadData.m_GamePadCode;
			break;
		case k_eVRTouchPad: // this is an analog event
			break;
		}
	}


	// Can't ever make this not 32bits in size, see crazy comparison operators below.  Also, each
	// member must be exactly 32bits, not less with uninitialized data.
	EInputType m_InputType;
	union
	{
		KeyCode m_KeyCode;
		GamePadCode m_GamePadCode;
		MouseCode m_MouseCode;
	} m_Data;

	uint32 m_unModifiers;
	CPanoramaSymbol m_symNameSpace;

	bool operator<( const ActionInput_t &that ) const
	{
		if ( m_InputType != that.m_InputType  )
			return m_InputType < that.m_InputType;

		if ( m_unModifiers != that.m_unModifiers )
			return m_unModifiers < that.m_unModifiers;

		// I have a namespace and you don't
		if ( m_symNameSpace.IsValid() && !that.m_symNameSpace.IsValid() )
			return true;

		// You have a namespace and I don't
		if ( that.m_symNameSpace.IsValid() && !m_symNameSpace.IsValid() )
			return false;

		// compare namespace if both are valid but not equal
		if ( m_symNameSpace.IsValid() && that.m_symNameSpace.IsValid() && m_symNameSpace != that.m_symNameSpace )
			return m_symNameSpace < that.m_symNameSpace;

		// lets compare the raw code now
		switch( m_InputType )
		{
		default:
		case k_eKeyDown:
		case k_eKeyUp:
		case k_eKeyChar:
			return m_Data.m_KeyCode < that.m_Data.m_KeyCode;
		case k_eMouseDown:
		case k_eMouseUp:
		case k_eMouseMove:
		case k_eMouseWheel:
		case k_eMouseDoubleClick:
		case k_eMouseTripleClick:
			return m_Data.m_MouseCode < that.m_Data.m_MouseCode;
		case k_eGamePadDown:
		case k_eGamePadUp:
		case k_eGamePadAnalog:
			return m_Data.m_GamePadCode < that.m_Data.m_GamePadCode;
		case k_eVRTouchPad:
			return that.m_InputType < m_InputType;
		}
	}

	bool operator==( const ActionInput_t &that ) const
	{
		if ( m_InputType == that.m_InputType && m_unModifiers == that.m_unModifiers &&  
			// also if both namespace are valid and equal OR either namespace is unset (i.e "global")
			( ( m_symNameSpace.IsValid() && that.m_symNameSpace.IsValid() && m_symNameSpace == that.m_symNameSpace ) || ( !m_symNameSpace.IsValid() && !that.m_symNameSpace.IsValid() ) )  )
		{
			switch( m_InputType )
			{
			default:
			case k_eKeyDown:
			case k_eKeyUp:
			case k_eKeyChar:
				return m_Data.m_KeyCode == that.m_Data.m_KeyCode;
			case k_eMouseDoubleClick:
			case k_eMouseTripleClick:
			case k_eMouseDown:
			case k_eMouseUp:
			case k_eMouseMove:
			case k_eMouseWheel:
				return m_Data.m_MouseCode == that.m_Data.m_MouseCode;
			case k_eGamePadDown:
			case k_eGamePadUp:
			case k_eGamePadAnalog:
				return m_Data.m_GamePadCode == that.m_Data.m_GamePadCode;
			case k_eVRTouchPad:
				return that.m_InputType == m_InputType;
			}
		}
		return false;
	}

};

// Helpers for checking modifier state
inline bool IsControlPressed( uint32 unModifiers ) { return unModifiers & MODIFIER_LCONTROL || unModifiers & MODIFIER_RCONTROL; }
inline bool IsAltPressed( uint32 unModifiers ) { return unModifiers & MODIFIER_LALT || unModifiers & MODIFIER_RALT; }
inline bool IsShiftPressed( uint32 unModifiers ) { return unModifiers & MODIFIER_LSHIFT || unModifiers & MODIFIER_RSHIFT; }
inline bool IsWinPressed( uint32 unModifiers ) { return unModifiers & MODIFIER_LWIN || unModifiers & MODIFIER_RWIN; }

// Helper for checking if shortcuts like Ctrl+C/Ctrl+V
inline bool IsControlShortcutPressed( uint32 unModifiers )
{
	// Is control or command on mac pressed?
	if ( !IsControlPressed( unModifiers ) && !( IsOSX() && IsWinPressed( unModifiers ) ) )
		return false;

	// Is alt or shift also pressed? If so, don't claim the shortcut is pressed. This is important because
	// when things like polish keyboard layouts try to type AltGr, they end up with both Alt+Ctrl pressed.
	// So instead of typing an accented "a", they end up first doing a "select all" followed by the accented "a",
	// which ends up deleting all their text.
	if ( IsAltPressed( unModifiers ) || IsShiftPressed( unModifiers ) )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: check if game pad code belongs to active controller 
//-----------------------------------------------------------------------------
inline bool BIsGamePadCodeForController( GamePadCode eCode, EActiveControllerType eController )
{
	if( eCode < XK_BUTTON_LAST && eController == k_EActiveControllerType_XInput )
		return true;

	if( eCode > XK_BUTTON_LAST && eController == k_EActiveControllerType_Steam )
		return true;

	return false;
}


// 
// struct to wrap mouse move events for mouse tracked panels
//
struct MouseTrackingResults_t
{
	MouseTrackingResults_t()
	{
		m_hPanel = k_ulInvalidPanelHandle64;
		m_flX = 0.0f;
		m_flY = 0.0f;

	}
	MouseTrackingResults_t( uint64 handle, float x, float y )
	{
		m_hPanel = handle;
		m_flX = x;
		m_flY = y;
	}

	uint64 m_hPanel;
	float m_flX;
	float m_flY;
};

//
// An interface to receive captured input
//
class IInputCapture
{
public:
	// keyboard
	virtual bool OnCapturedKeyDown( IUIPanel *pPanel, const KeyData_t &code ) = 0;
	virtual bool OnCapturedKeyUp( IUIPanel *pPanel, const KeyData_t &code ) = 0;
	virtual bool OnCapturedKeyTyped( IUIPanel *pPanel, const KeyData_t &unichar ) = 0;

	// mouse
	virtual bool OnCapturedMouseHover( IUIPanel *pPanel ) = 0;
	virtual bool OnCapturedMouseMove( IUIPanel *pPanel, float flMouseX, float flMouseY ) = 0;
	virtual bool OnCapturedMouseButtonDown( IUIPanel *pPanel, const MouseData_t &code ) = 0;
	virtual bool OnCapturedMouseButtonUp( IUIPanel *pPanel, const MouseData_t &code ) = 0;
	virtual bool OnCapturedMouseButtonDoubleClick( IUIPanel *pPanel, const MouseData_t &code ) = 0;
	virtual bool OnCapturedMouseButtonTripleClick( IUIPanel *pPanel, const MouseData_t &code ) = 0;
	virtual bool OnCapturedMouseWheel( IUIPanel *pPanel, const MouseData_t &code ) = 0;
	virtual bool OnCapturedVRTouchPad( IUIPanel *pPanel, const VRTouchEvent_t &code ) = 0;

	// gamepad
	virtual bool OnCapturedGamePadDown( IUIPanel *pPanel, const GamePadData_t &code ) = 0;
	virtual bool OnCapturedGamePadUp( IUIPanel *pPanel, const GamePadData_t &code ) = 0;
	virtual bool OnCapturedGamePadAnalog( IUIPanel *pPanel, const GamePadData_t &code ) = 0;
};

class CDefaultInputCapture : public IInputCapture
{
public:
	// keyboard
	virtual bool OnCapturedKeyDown( panorama::IUIPanel *pPanel, const panorama::KeyData_t &code ) OVERRIDE { return false; }
	virtual bool OnCapturedKeyUp( panorama::IUIPanel *pPanel, const panorama::KeyData_t &code ) OVERRIDE { return false; }
	virtual bool OnCapturedKeyTyped( panorama::IUIPanel *pPanel, const panorama::KeyData_t &unichar ) OVERRIDE { return false; }

	// mouse
	virtual bool OnCapturedMouseHover( panorama::IUIPanel *pPanel ) OVERRIDE { return false; }
	virtual bool OnCapturedMouseMove( IUIPanel *pPanel, float flMouseX, float flMouseY ) OVERRIDE { return false; }
	virtual bool OnCapturedMouseButtonDown( panorama::IUIPanel *pPanel, const panorama::MouseData_t &code ) OVERRIDE { return false; }
	virtual bool OnCapturedMouseButtonUp( panorama::IUIPanel *pPanel, const panorama::MouseData_t &code ) OVERRIDE { return false; }
	virtual bool OnCapturedMouseButtonDoubleClick( panorama::IUIPanel *pPanel, const panorama::MouseData_t &code ) OVERRIDE { return false; }
	virtual bool OnCapturedMouseButtonTripleClick( panorama::IUIPanel *pPanel, const panorama::MouseData_t &code ) OVERRIDE { return false; }
	virtual bool OnCapturedMouseWheel( panorama::IUIPanel *pPanel, const panorama::MouseData_t &code ) OVERRIDE { return false; }
	virtual bool OnCapturedVRTouchPad( IUIPanel *pPanel, const VRTouchEvent_t &code ) OVERRIDE{ return false; }

	// gamepad
	virtual bool OnCapturedGamePadDown( panorama::IUIPanel *pPanel, const panorama::GamePadData_t &code ) OVERRIDE { return false; }
	virtual bool OnCapturedGamePadUp( panorama::IUIPanel *pPanel, const panorama::GamePadData_t &code ) OVERRIDE { return false; }
	virtual bool OnCapturedGamePadAnalog( panorama::IUIPanel *pPanel, const panorama::GamePadData_t &code ) OVERRIDE { return false; }
};


class IDragStartCallbacks: public panorama::IUIJSObject
{
public:
	virtual void SetDisplayPanel( IUIPanel *pPanel ) = 0;
	virtual IUIPanel *GetDisplayPanel() const = 0;

	// Allows override of the anchor point
	virtual void SetOffsetX( int nOffset ) = 0;
	virtual int GetOffsetX() const = 0;
	virtual void SetOffsetY( int nOffset ) = 0;
	virtual int GetOffsetY() const = 0;

	// By default, the drag system will remove the position property it applied before sending DragDrop().
	// If you want to control that manually (for example to run an animation on the element being dragged),
	// you can turn that off here
	virtual void SetRemovePositionBeforeDrop( bool bRemovePositionBeforeDrop ) = 0;
	virtual bool ShouldRemovePositionBeforeDrop() const = 0;
};

//
// Handles per top level window focus
//
class IUIWindowInput
{
public:
	virtual bool InputEvent( InputMessage_t &msg, bool bNewEvent = true ) = 0;

	// Receive mouse move events, in window coordinate space
	virtual void OnMouseMove( float flMouseX, float flMouseY ) = 0;
	virtual void OnMouseMoveSurfaceCoords( float flMouseX, float flMouseY ) = 0;

	// current mouse coordinates and visibility
	virtual void GetSurfaceMousePosition( float &x, float &y ) = 0;
	virtual bool BCursorVisible() = 0;
	virtual void WakeupMouseCursor() = 0;
	virtual void FadeOutCursorNow() = 0;

	// gamepad state
	virtual int GetNumGamepadsConnected() = 0;
	virtual bool BWasGamepadConnectedThisSession() = 0;
	virtual bool BWasGamepadUsedThisSession() = 0;
	virtual bool BWasSteamControllerConnectedThisSession() = 0;
	virtual bool BWasSteamControllerUsedThisSession() = 0;
	// Do we want to act like we have a mouse/keyboard connected? Note that some platforms don't have accurate data here
	// but we do the best we can. If we've seen mouse/keyboard input this session, we assume that you still have it
	// connected.
	virtual bool BWasMouseOrKeyboardUsedThisSession() const = 0;

	virtual void RereadControllerState() = 0;

	// tracking of last input type
	virtual bool BWasGamepadLastInputSource() = 0;
	virtual bool BWasMouseLastInputSource() = 0;
	virtual bool BWasKeyboardOrMouseLastInputSource() = 0;

	// Get the last input source
	virtual EPanelEventSource_t GetLastPanelEventSource() = 0;

	// top level OS window support
	virtual void GotWindowFocus() = 0;
	virtual void LostWindowFocus() = 0;
	virtual bool BHasWindowFocus() = 0;

	// Window can temporarily disable all input, used for overlay when the game is focused, but overlay inactive
	virtual bool BAllowInput(  InputMessage_t &msg ) = 0;

	// panel management
	virtual void SetInputFocus( IUIPanel *pPanel, bool bScrollParentToFit, bool bChangeContextIfNeeded ) = 0;
	virtual bool SetInputFocusContext( IUIPanel *pPanelInContext ) = 0;
	virtual void PopInputContext() = 0;
	virtual void RemoveInputContext( IUIPanel *pPanel ) = 0;
	virtual IUIPanel *GetInputFocusContext() = 0;
	virtual IUIPanel *GetInputFocus() = 0;
	virtual IUIPanel *GetMouseHover() = 0;
	virtual void PanelDeleted( IUIPanel *pPanel, IUIPanel *pParent ) = 0;

	// input hooks
	virtual void HookPanelInput( IUIPanel *pPanel, IInputCapture *pInputCapture ) = 0;
	virtual void RemovePanelInputHook( IUIPanel *pPanel, IInputCapture *pInputCapture ) = 0;

	// set this panel to always (or stop) getting MouseMove events
	virtual void AddMouseTrackingPanel( IUIPanel *pPanel ) = 0;
	virtual void RemoveMouseTrackingPanel( IUIPanel *pPanel ) = 0;

	// Are we currently inside a set input focus call
	virtual bool BInSetInputFocusTraverse() = 0;

	// Queue a panel focus event to occur once we finish with setting input focus
	virtual void QueuePanelFocusEvent( IUIPanel *pPanel, CPanoramaSymbol symPanelEvent ) = 0;

	// reset any mouse movement count as we just hid the cursor
	virtual void ResetMouseMoveCount() = 0;

	// Get focus panel at time of last mouse down
	virtual IUIPanel *GetFocusOnLastMouseDown() = 0;

	// Copy any input events to target window
	virtual void SetInputForwarding( IUIWindowInput *pWindowInputForwarding ) = 0;
	
	// Is a drag+drop operation in progress?
	virtual bool BDragInProgress() = 0;

	// enable/disable dragging with a specific mouse button
	virtual void SetDragDropEnabled( MouseCode code, bool bEnabled ) = 0;

	// Draw debug information regarding the top level window on the screen
	virtual void DrawInputDebugInfo() = 0;
};

////////////////////////////////////
// DO NOT RENUMBER - These values are used by the web
////////////////////////////////////
enum EAttachedHardwareDevice
{
	k_AttachedHardwareDevice_MouseKeyboard = 1,
	k_AttachedHardwareDevice_SteamController = 2,
	k_AttachedHardwareDevice_XInput = 3,
	k_AttachedHardwareDevice_HTCVive = 4,
	k_AttachedHardwareDevice_OculusDK1 = 5,
	k_AttachedHardwareDevice_OculusDK2 = 6,
	k_AttachedHardwareDevice_OculusCV1 = 7,
	k_AttachedHardwareDevice_HTCMotionController = 8,
	k_AttachedHardwareDevice_OculusTouchController = 9,

	k_Dummy_AttachedHardwareDeviceCount,
	k_AttachedHardwareDeviceCount = k_Dummy_AttachedHardwareDeviceCount - 1			// we're 1-indexed in this list
};

inline bool BIsVRHMDHardwareDevice( EAttachedHardwareDevice eDevice )
{
	return (eDevice == k_AttachedHardwareDevice_HTCVive || eDevice == k_AttachedHardwareDevice_OculusDK1 || eDevice == k_AttachedHardwareDevice_OculusDK2 || eDevice == k_AttachedHardwareDevice_OculusCV1);
}

inline bool BIsOculusVRHMDHardwareDevice( EAttachedHardwareDevice eDevice )
{
	return (eDevice == k_AttachedHardwareDevice_OculusDK1 || eDevice == k_AttachedHardwareDevice_OculusDK2 || eDevice == k_AttachedHardwareDevice_OculusCV1);
}

inline bool BIsViveVRHMDHardwareDevice( EAttachedHardwareDevice eDevice )
{
	return (eDevice == k_AttachedHardwareDevice_HTCVive);
}


//
// Handles key/mouse/gamepad input and dispatches to appropriate panels
//
class IUIInput
{
public:
	virtual void Initialize( IUISettings *pSettings ) = 0;

	// Not ifdef'd or specific to windows. v_key ended up as a common
	// denominator in lots of code (overlay as an example)
	// 0x00 for error in mapping. There is no 0x00 VKEY
	virtual uint16 KeyCodeToWindowsVKey( const KeyCode inKey ) = 0;

	// KEY_NONE will come back on error.
	virtual KeyCode WindowsVKeyToKeyCode( uint16 inKey ) = 0;

#ifdef SOURCE2_PANORAMA
	virtual ButtonCode_t KeyCodeToButtonCode( const KeyCode inKey ) = 0;
	virtual ButtonCode_t MouseCodeToButtonCode( const MouseCode inKey ) = 0;
#endif

	// kb/mouse input
	virtual bool InputEvent( InputMessage_t &msg ) = 0;

	// used to capture all input
	virtual void SetInputCapture( IInputCapture *pCapture ) = 0;
	virtual void ReleaseInputCapture( IInputCapture *pCapture ) = 0;
	virtual CUtlVector< IInputCapture * > &GetInputCapture() = 0;
	virtual bool BGetDebugHitTesting( void ) const = 0;
	virtual void SetDebugHitTesting( bool bDebugHitTest ) = 0;

	// Checks whether gamepads are connected
	virtual int GetNumGamepadsConnected() const = 0;
	
	// did any gamepad have input since we last asked
	virtual bool BWasGamepadOrSteamControllerActive() = 0;

	// if a gamepad is connected then its friendly name
	virtual const char *PchGamePadName( int iDevice ) = 0;

	// helper for gamepad codes, returns values that are inside the deadzone for this joystick
	virtual float GetDeadZoneValue( GamePadCode code ) = 0;

	// translate an event into the gamepad key bound to it, XK_NULL if not bound
	virtual const GamePadCode GetGamePadBindForEvent( const char *pchEvent, const IUIPanel *pFromPanel ) = 0;

	// is capslock on
	virtual bool BIsCapsLockOn() = 0;

	// If we're trying to show help text for a controller, which type of controller is most relevant (ie., most recently
	// used, exists, etc.)? You probably don't want to call this directly but instead listen for ActiveControllerTypeChanged.
	virtual EActiveControllerType GetActiveControllerType() const = 0;

#if !defined(NO_STEAM)
	// Get count of actively connected Steam controllers
	virtual uint32 GetSteamControllerCount() const = 0;

	// Get the time a steam controller was last assigned/used
	virtual float GetLastSteamControllerActiveTime() const = 0;
#endif

	// Get the time a non-steam controller was last assigned/used
	virtual float GetLastGamePadControllerActiveTime() const = 0;

#if !defined(NO_STEAM)
	// Get the ID of the steam controller currently sending events to the window
	virtual int GetLastSteamControllerActiveIndex() const = 0;
#endif
	
	// Pulse haptic feedback on active gamepad/steam controller if supported
	virtual void PulseActiveControllerHaptic( IUIEngine::EHapticFeedbackPosition ePosition, IUIEngine::EHapticFeedbackStrength eStrength ) = 0;
	
	// Determine the correct position for haptics
	virtual IUIEngine::EHapticFeedbackPosition GetHapticFeedbackPositionForInteraction() = 0;
	
	// Disables every Steam Controller except the requested index; -1 to re-enable all
	virtual void SetControllerExclusiveEnabledIndex( int iIndex ) = 0;

#if !defined(NO_STEAM)
	// Is finger actively down on steam controller left/right pad?
	virtual bool BIsFingerDownOnSteamControllerLeftPad() const = 0;
	virtual bool BIsFingerDownOnSteamControllerRightPad() const = 0;
#endif

	// Register a file path to look for keybindings
	virtual void RegisterKeyBindingsFile( const char *pszFilePath ) = 0;

	// Force a reload of the keybindings
	virtual void ReloadKeyBindings() = 0;

	// Private APIs for remote gamepad input, called on a separate network thread
	virtual void RemoteGamepadAttached( int nGamepadID ) = 0;
	virtual void RemoteGamepadDetached( int nGamepadID ) = 0;
	virtual void SetRemoteGamepadAxis( int nGamepadID, int nAxis, int nValue ) = 0;
	virtual void SetRemoteGamepadButton( int nGamepadID, int nButton, int nValue ) = 0;

	// Turn off whatever (wireless) controller was last active
	virtual void TurnOffActiveController() = 0;

	// Get gamepad code value from textual name for config files, event code, etc
	virtual panorama::GamePadCode GamePadCodeFromName( const char * pchGamePadCode ) = 0;

	// Check if two gamepad codes are the 'same' button but on different vendor devices
	virtual bool BIsGamePadCodeEquivalentIgnoringVendor( GamePadCode a, GamePadCode b ) = 0;

	enum EControllerPowerLevel
	{
		ePowerLevel_Unknown,
		ePowerLevel_Empty,
		ePowerLevel_Low,
		ePowerLevel_Medium,
		ePowerLevel_Full,
		ePowerLevel_Wired,
	};

	// get the battery level for a controller, from 0 to GetNumGamepadsConnected()
	virtual EControllerPowerLevel GetConnectGamePadPowerLevel( int iGamePad ) = 0;

	// pulse this gamepads haptics
	virtual void PulseGamePadHaptics( int iGamePad, float flStrength, uint32 uEffectMS ) = 0;

#if !defined( SOURCE2_PANORAMA )
	// Get a list of all known attached/usable input devices. This may return different results based on the
	// user's platform/configuration.
	virtual CUtlVector<EAttachedHardwareDevice> GetAttachedHardwareDevices() const = 0;
#endif

	virtual void ForceActiveControllerType( EActiveControllerType uControllerType ) = 0;
	virtual void SetVRControllerActivityTime() = 0;

#ifdef SOURCE2_PANORAMA
	virtual bool IsIMEAllowed() const = 0;
	virtual void SetIMEAllowed( bool bAllowed ) = 0;
#endif
};

} // namespace panorama

#endif // IUIINPUT_H
