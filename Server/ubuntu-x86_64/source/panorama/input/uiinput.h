//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef UIINPUT_H
#define UIINPUT_H

#ifdef _WIN32
#pragma once
#endif

#include "panorama/input/iuiinput.h"
#include "panorama/input/keycodes.h"
#include "panorama/iuipanel.h"
#include "panorama/controls/panelptr.h"
#include "tier1/utlhashmap.h"
#include "controller.h"

#if !defined(NO_STEAM)
#if !defined( PANORAMA_PUBLIC_STEAM_SDK )
#include "steamcontroller.h"
#else
#include "steamcontroller_new.h"
#endif
#endif

#include "tier1/utldelegate.h"
#if !defined( SOURCE2_PANORAMA )
#include "steamcommon.h"
#endif

#if _GNUC
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#endif
#if defined( SOURCE2_PANORAMA )
#include "../thirdparty/v8/include/v8.h"
#else
#include "tier0/memdbgoff.h"
#include "../external/v8/include/v8.h"
#include "tier0/memdbgon.h"
#endif
#if _GNUC
#pragma GCC diagnostic pop
#endif

panorama::KeyCode KeyCodeFromName( const char * pchKeyPadCode );
panorama::MouseCode MouseCodeFromName( const char * pchMousePadCode );
uint32 ParseModifier( const char * pchModifier, const char **pchRemaining );

namespace panorama
{

class CTopLevelWindow;
class CUIInputEngine;
class CPanel2D;

struct InputAction_t
{
	InputAction_t() { }
	InputAction_t( const InputAction_t &rhs );
	InputAction_t &operator =( const InputAction_t rhs);
		

	
	~InputAction_t();

	// Only one of the two should be set
	CPanoramaSymbol m_symUIEventName;
	v8::Persistent<v8::Function> m_pJSAction;
	CPanelPtr< IUIPanel > m_pJSActionContextPanel;
	CUtlString m_strParams;
};


//#define DEBUG_MOUSE_VELOCITY
class CMouseVelocityTracker
{
public: 
	CMouseVelocityTracker();
	void StartSampling( int x, int y, double flStartTime );
	void AddSample( int x, int y);
	void GetVelocities( float &flVelocityX, float &flVelocityY );

private:
	void UpdateVelocitySamples( bool bForce = false );
	double m_flLastSample;
	float m_velocityX;
	float m_velocityY;
	int m_lastX;
	int m_lastY;
	float m_flCurrentDeltaX;
	float m_flCurrentDeltaY;

#if defined(DEBUG_MOUSE_VELOCITY)
	int m_nSamples;
	double m_flFirstSample;

	int m_startX;
	int m_startY;
#endif
};



class CUIWindowInput : public IUIWindowInput
{
public:
	CUIWindowInput( CTopLevelWindow *pWindow, CUIInputEngine *pParent );
	~CUIWindowInput();

	void RunFrame();

	virtual void SetInputFocus( IUIPanel *pPanel, bool bScrollParentToFit, bool bChangeContextIfNeeded )  OVERRIDE;
	virtual bool SetInputFocusContext( IUIPanel *pPanelInContext ) OVERRIDE;
	virtual void PopInputContext() OVERRIDE;
	virtual void RemoveInputContext( IUIPanel *pPanel ) OVERRIDE;
	virtual IUIPanel *GetInputFocusContext() OVERRIDE;
	virtual IUIPanel *GetInputFocus() OVERRIDE { return m_ActionFocus.m_pFocus.Get(); }
	virtual IUIPanel *GetMouseHover() OVERRIDE { return m_pMouseOverInternal.Get(); }
	virtual void PanelDeleted( IUIPanel *pPanel, IUIPanel *pParent ) OVERRIDE;

	virtual bool InputEvent( InputMessage_t &msg, bool bNewEvent = true );
	bool ActionEvent( InputAction_t code, EPanelEventSource_t eSource, int nRepeats );
	virtual void OnMouseMove( float flMouseX, float flMouseY );
	virtual void OnMouseMoveSurfaceCoords( float flMouseX, float flMouseY );

	virtual void GotWindowFocus();
	virtual void LostWindowFocus();
	virtual bool BHasWindowFocus();

	virtual bool BAllowInput( InputMessage_t &msg ); 

	virtual void GetSurfaceMousePosition( float &x, float &y );
	virtual bool BCursorVisible();
	virtual void WakeupMouseCursor();
	virtual void FadeOutCursorNow();
	virtual void ResetMouseMoveCount() { m_cMouseMoveCount = 0; }

	virtual int GetNumGamepadsConnected() OVERRIDE;
	virtual bool BWasGamepadConnectedThisSession() OVERRIDE;
	virtual bool BWasGamepadUsedThisSession() OVERRIDE;
	virtual bool BWasSteamControllerConnectedThisSession() OVERRIDE;
	virtual bool BWasSteamControllerUsedThisSession() OVERRIDE;
	virtual bool BWasMouseOrKeyboardUsedThisSession() const OVERRIDE { return m_bHasReceivedMouseOrKeyboardInput; }

	virtual void RereadControllerState() OVERRIDE;

	virtual bool BWasGamepadLastInputSource() { return m_eLastInputSource == k_ePanelEventSourceGamepad; }
	virtual bool BWasMouseLastInputSource() { return m_eLastInputSource == k_ePanelEventSourceMouse; }
	virtual bool BWasKeyboardOrMouseLastInputSource() { return ( m_eLastInputSource == k_ePanelEventSourceKeyboard || m_eLastInputSource == k_ePanelEventSourceMouse ); }

	virtual EPanelEventSource_t GetLastPanelEventSource() { return m_eLastInputSource; }

	// manage the panels we are explicitly asking for mouse move messages from
	void AddMouseTrackingPanel( IUIPanel *pPanel );
	void RemoveMouseTrackingPanel( IUIPanel *pPanel );

	// get the handles to track moves and the results from that
	CCopyableUtlVector<uint64> GetMouseTrackingHandles();
	void SetMouseTrackingResults( CCopyableUtlVector<MouseTrackingResults_t> &vec );

	virtual void HookPanelInput( IUIPanel *pPanel, IInputCapture *pInputCapture );
	virtual void RemovePanelInputHook( IUIPanel *pPanel, IInputCapture *pInputCapture );

	// Callback event func on hover of a panel being detected
	void SetLastHover( double flFrameTime, uint64 ulPanelSafePtrValue, float flMouseX, float flMouseY );
	bool OnHoverPanel( uint64 ulPanelSafePtrValue, float flMouseX, float flMouseY );
	void ChangeHoverState( IUIPanel *pTo, IUIPanel *pFrom );

	// Callback from animation thread on mouse coordinate updates for the current mouse down panel
	void SetLastMouseDownPanelCoords( uint64 ulPanelSafePtrValue, float flMouseX, float flMouseY );

	// returns a panel that contains the mouse image
	IImageSource *GetMouseCursorTexture();

	CTopLevelWindow *GetTopLevelWindow() { return m_pWindow; }

	// true if the mouse scrolled/clicked since you last asked
	bool BWasMouseClickedOrScrolled();
	// true if the mouse moved since last reset, where moved may have some filtering based on OS behavior
	bool BWasMouseMoving();

	bool BWasKeyboardUsed();

	// Are we currently inside a set input focus call
	bool BInSetInputFocusTraverse();

	// Queue a panel focus event to occur once we finish with setting input focus
	void QueuePanelFocusEvent( IUIPanel *pPanel, CPanoramaSymbol symPanelEvent );

	IUIPanel *GetFocusOnLastMouseDown() { return m_ptrFocusOnMouseDown.Get(); }
	
	void SetInputForwarding( IUIWindowInput *pWindowInputForwarding ) { m_pWindowInputForwarding = pWindowInputForwarding; }

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName );
#endif

	virtual bool BDragInProgress() { return m_ptrMouseDragDisplay.Get() != nullptr; }

	virtual void SetDragDropEnabled( MouseCode code, bool bEnabled );

	// Draw debug information regarding the top level window on the screen
	virtual void DrawInputDebugInfo() OVERRIDE;

private:

	bool OnMouseButtonDown( const MouseData_t &mouseData );
	bool OnMouseButtonUp( const MouseData_t &mouseData, bool bSuppressActivateOnMouseUp );
	bool OnMouseDoubleClick( const MouseData_t &mouseData );
	bool OnMouseTripleClick( const MouseData_t &mouseData );
	bool OnMouseWheel( const MouseData_t &mouseData );
	void OnMouseLeave();

	bool SetInputFocusContextInternal( IUIPanel *pPanelInContext, bool bDontClearHoverAndMouseDown, bool bFireEvents );
	void FireInputFocusTopLevelChangedEvents( IUIPanel *pCurrentTopMost, IUIPanel *pCurrentFocus, IUIPanel *pNewTopMost, IUIPanel *pNewFocus );

	template < class T >
	bool OnMouseClickInternal( const MouseData_t &mouseData, bool (IInputCapture::*pCaptureFunc)(IUIPanel *pPanel, const T &data), bool (IUIPanelClient::*pPanelFunc)(const T &data) );

	void ProcessHoverData();
	void ProcessMouseTrackingResults();
	void DispatchShowTooltip();
	void ClearMouseDown();
	void SetMouseDownPtr( IUIPanel *pPanel, panorama::MouseCode mouseButton );
	void UpdateDragDrop();
	void UpdateDragScroll();
	void DragScrollEnd();

	void ClearHoverData( double flSecondsToSuppress );

	// hooks related
	template < class T >
	bool BSendInput( IUIPanel *pPanel, const T &data, bool (IInputCapture::*pCaptureFunc)(IUIPanel *pPanel, const T &data), bool (IUIPanelClient::*pPanelFunc)(const T &data), bool bIgnoreAcceptsInput );

	CUtlVector< IInputCapture * > *GetHooksForPanel( IUIPanel *pPanel );


	CUIInputEngine *m_pParent;	
	CTopLevelWindow *m_pWindow;		// top level window we are managing input for


	struct TopMostFocus_t
	{
		CPanelPtr< IUIPanel > m_pTopmost;
		CPanelPtr< IUIPanel > m_pFocus;
	};
	TopMostFocus_t m_ActionFocus;
	CUtlLinkedList< TopMostFocus_t > m_stackPriorFocus;

	bool m_bInSetInputFocusTraverse;
	struct QueuedPanelEvent_t
	{
		CPanelPtr< IUIPanel > m_pPanel;
		CPanoramaSymbol m_symPanelEvent;
	};
	CUtlVector<QueuedPanelEvent_t> m_vecQueuedPanelFocusEvents;

	// This whole set is protected by the mutex below
	CThreadMutex m_MutexMousePos;
	int m_nMouseX;
	int m_nMouseY;
	float m_flSurfaceMouseX;
	float m_flSurfaceMouseY;
	// End mutex protected


	// This whole set is protected by the mutex below
	CThreadMutex m_HoverPanelMutex;
	bool m_bHoverDirty;
	uint64 m_ulLastHoverPanelPtrValue;
	float m_flLastHoverPanelMouseX;
	float m_flLastHoverPanelMouseY;
	double m_flLastHoverPanelFrameTime;
	// End mutex protected

	CPanelPtr< IUIPanel > m_ptrMouseOver;			// panel the mouse is currently over (this lies if mouse is held down, so styles don't change)
	CPanelPtr< IUIPanel> m_pMouseOverInternal;		// actual panel the mouse is currently over regardless of down or not 
	bool m_bHasReceivedMouseOrKeyboardInput;

	struct MouseDownState_t
	{
		MouseDownState_t()
			: m_bMouseDownHandled( false )
			, m_bCanActivateOnMouseUp( false )
			, m_bSuppressActivateOnMouseUp( false )
			, m_nMouseDownX( 0 )
			, m_nMouseDownY( 0 )
			, m_flMouseDownTime( 0.0 )
		{
		}

		CPanelPtr< IUIPanel > m_ptrMouseDown;			// panel mouse was over on down event
		bool m_bMouseDownHandled;
		bool m_bCanActivateOnMouseUp;					// true if the m_ptrMouseDown panel had focus when we received mousedown
		bool m_bSuppressActivateOnMouseUp;				// true if we got a handled double or triple click. Don't activate on MouseUp in that case.
		int m_nMouseDownX;
		int m_nMouseDownY;
		double m_flMouseDownTime;
	};
	MouseDownState_t m_MouseDownStates[ MOUSE_LAST ];

	// drag and drop
	bool m_bCheckedForDrag;							// set to true if mouse moved enough to check for drag/drop with mouse down (might not have found parent that was draggable; don't check again)
	MouseCode m_dragMouseCode;
	bool m_bDispatchDragEnter;
	CPanelPtr< IUIPanel > m_ptrMouseDragInitiate;	// panel that was originally clicked that started drag/drop events
	CPanelPtr< IUIPanel > m_ptrMouseDragDisplay;	// panel that is shown next to mouse cursor when dragging. Returned in DragStart event
	int m_nMouseDragOffsetX;
	int m_nMouseDragOffsetY;
	bool m_bRemovePositionBeforeDragDrop;
	CUtlVector<MouseCode> m_dragDropMouseCodes;

	// Drag scrolling
	bool m_bEnableDragScroll;
	bool m_bCheckedForDragScroll;
	MouseCode m_dragScrollMouseCode;
	CPanelPtr< IUIPanel > m_ptrDragScroll;
	CMouseVelocityTracker m_velocityTracker;
	bool m_bScrollInProgressOnMouseDown;
	VRTouchEvent_t m_vrTouchEventLast;
	float m_flScrollGrabTime; 
	float m_flVRTouchLinearMoveDistanceForHaptics;

	// This whole set is protected by the mutex below
	CThreadMutex m_MouseTrackingMutex;
	bool m_bMouseTrackingDataDirty;
	CUtlVector< CPanelPtr< IUIPanel > > m_vecptrMouseTrackingPanels; // vec of panels that want OnMouseMove events sent to them independent of them being hover's or down panel
	CCopyableUtlVector< uint64 > m_vecTrackingPanelHandles; // vec of tracking results we have for a panel
	CCopyableUtlVector< MouseTrackingResults_t > m_vecTrackingResults; // vec of tracking results we have for a panel
	// End mutex protected

	double m_flTooltipDispatch;
	CUtlHashMap< CPanelPtr< IUIPanel >, CUtlVector< IInputCapture * > *, CDefEquals< CPanelPtr< IUIPanel > > > m_mapHookPanelInput;

	CCubicBezierCurve< Vector2D > m_AxisRepeatCurve;

	struct ActionRepeatData_t
	{
		double flFirst;
		double flLast;
		int nRepeats;
	};
	CUtlHashMap< CPanoramaSymbol, ActionRepeatData_t, CDefEquals< CPanoramaSymbol > > m_mapActionRepeats;

	bool m_bMouseActive; // true if we had a mouse input message since the surface last asked
	bool m_bMouseDown; // true if we got a mouse down event and yet to have the up event
	bool m_bMouseVisible; // track what the cursor visibility was last time we looked
	int m_cMouseMoveCount; //  number of mouse move events we have seen since last reset
	bool m_bKeyboardActive;
	
	EPanelEventSource_t m_eLastInputSource;

	CPanelPtr< IUIPanel > m_ptrFocusOnMouseDown;
	
	IUIWindowInput *m_pWindowInputForwarding;


	// Debug drawing helper functions
	void DrawInputDebugText( const char *pchText, float x0, float y0 );
	void DrawInputTopMostFocus( const TopMostFocus_t &topmost, int nStackIndex, float x0, float y0 );
};

} // namespace panorama

//
// Custom hashing function for ActionInput_t so we IGNORE the namespace param for bucketing
//
#if defined( SOURCE2_PANORAMA )
template<>
struct HashMapFunctor_t<panorama::ActionInput_t>
#else
template<>
struct HashFunctor < panorama::ActionInput_t >
#endif
{
	typedef	uint32 TargetType ; 
	TargetType	operator()(const panorama::ActionInput_t &key) const
	{
		uint32 byte_one =	key.m_InputType;
		uint32 byte_two =	key.m_unModifiers;
		uint32 byte_three =	key.m_Data.m_KeyCode;
		return	( byte_three << 16 ) | ( byte_two << 8 ) | byte_one; 
	}
};

namespace panorama
{

//
// Handles key/mouse/gamepad input and dispatches to appropriate panels
//
class CUIInputEngine : public IUIInput
{
public:
	CUIInputEngine();
	~CUIInputEngine();
	void RunFrame();

	// IUIInput methods
	virtual void Initialize( IUISettings *pSettings );

	// Not ifdef'd or specific to windows. v_key ended up as a common
	// denominator in lots of code (overlay as an example)
	// 0x00 for error in mapping. There is no 0x00 VKEY
	virtual uint16 KeyCodeToWindowsVKey( const KeyCode inKey );

	// KEY_NONE will come back on error.
	virtual KeyCode WindowsVKeyToKeyCode( uint16 inKey );

#ifdef SOURCE2_PANORAMA
	virtual ButtonCode_t KeyCodeToButtonCode( const KeyCode inKey );
	virtual ButtonCode_t MouseCodeToButtonCode( const MouseCode inKey );
#endif

	virtual bool InputEvent( InputMessage_t &msg );
	bool ActionEvent( InputAction_t code, EPanelEventSource_t eSource, int nRepeats );
	virtual void SetInputCapture( IInputCapture *pCapture );
	virtual void ReleaseInputCapture( IInputCapture *pCapture );
	virtual CUtlVector< IInputCapture * > &GetInputCapture() { return m_vecInputCapture; }
	virtual bool BGetDebugHitTesting( void ) const { return m_bDebugHitTest; }
	virtual void SetDebugHitTesting( bool bDebugHitTest ) { m_bDebugHitTest = bDebugHitTest; }
	virtual int GetNumGamepadsConnected() const OVERRIDE { return m_GamePadController.GetNumGamepadsConnected(); }
	virtual bool BWasGamepadOrSteamControllerActive() { return m_bSawControllerInputThisFrame; }
	virtual const char *PchGamePadName( int iDevice );
	virtual float GetDeadZoneValue( GamePadCode code ) { return m_GamePadController.GetDeadZoneValue( code ); }
	virtual const GamePadCode GetGamePadBindForEvent( const char *pchEvent, const IUIPanel *pFromPanel );
	virtual bool BIsCapsLockOn();
	virtual void RegisterKeyBindingsFile( const char *pszFilePath );
	virtual void ReloadKeyBindings();
	virtual void RemoteGamepadAttached( int nGamepadID ) { m_GamePadController.RemoteGamepadAttached( nGamepadID ); }
	virtual void RemoteGamepadDetached( int nGamepadID ) { m_GamePadController.RemoteGamepadDetached( nGamepadID ); }
	virtual void SetRemoteGamepadAxis( int nGamepadID, int nAxis, int nValue ) { m_GamePadController.SetRemoteGamepadAxis( nGamepadID, nAxis, nValue ); }
	virtual void SetRemoteGamepadButton( int nGamepadID, int nButton, int nValue ) { m_GamePadController.SetRemoteGamepadButton( nGamepadID, nButton, nValue ); }
#if !defined(NO_STEAM)
	virtual void TurnOffActiveController() { m_SteamController.TurnOffActiveController(); }  // could also support X360 pads
#else
	virtual void TurnOffActiveController() {};
#endif

	// Implementation specific methods
	void TranslateInputEvent( const InputMessage_t &msg, InputAction_t &actionOut );
	void SetWindowInputFocus( CUIWindowInput *pFocus );
	void LostWindowInputFocus( CUIWindowInput *pLost );
	bool BHasWindowFocus( CUIWindowInput *pWindow );
	void OnWindowShutdown( CTopLevelWindow *pWindow );
	void ReloadChangedFile( const char *pchFile );
	bool BWasGamepadConnectedThisSession() { return m_GamePadController.BWasGamepadConnectedThisSession(); }
	bool BWasGamepadUsedThisSession() { return m_GamePadController.BWasGamepadUsedThisSession(); }
#if !defined(NO_STEAM)
	bool BWasSteamControllerConnectedThisSession() { return m_SteamController.BWasSteamControllerConnectedThisSession(); }
	bool BWasSteamControllerUsedThisSession() { return m_SteamController.BWasSteamControllerUsedThisSession(); }
#else
	bool BWasSteamControllerConnectedThisSession() { return false; }
	bool BWasSteamControllerUsedThisSession() { return false; }
#endif

	void RereadControllerState();

	EActiveControllerType GetActiveControllerType() const OVERRIDE;
	void ForceActiveControllerType( EActiveControllerType uControllerType ) OVERRIDE;

#if !defined(NO_STEAM)
	uint32 GetSteamControllerCount() const OVERRIDE { return m_SteamController.GetNumGamepadsConnected(); }
	float GetLastSteamControllerActiveTime() const OVERRIDE { return m_SteamController.GetLastUserIDAssignment(); }
	int GetLastSteamControllerActiveIndex() const OVERRIDE { return m_SteamController.GetLastActiveControllerIndex(); }
	bool BIsFingerDownOnSteamControllerLeftPad() const OVERRIDE { return m_SteamController.BIsFingerDownOnSteamControllerLeftPad(); }
	bool BIsFingerDownOnSteamControllerRightPad() const OVERRIDE { return m_SteamController.BIsFingerDownOnSteamControllerRightPad(); }
#else

#endif
	float GetLastGamePadControllerActiveTime() const OVERRIDE { return m_GamePadController.GetLastUserIDAssignment(); }
	// Add GetLastVRControllerActiveTime() and GetLastVRControllerActiveIndex() here

	// Pulse haptic feedback on active gamepad/steam controller if supported
	virtual void PulseActiveControllerHaptic( IUIEngine::EHapticFeedbackPosition ePosition, IUIEngine::EHapticFeedbackStrength eStrength );
	virtual IUIEngine::EHapticFeedbackPosition GetHapticFeedbackPositionForInteraction();

	// Disables every Steam Controller except the requested index; -1 to re-enable all
	void SetControllerExclusiveEnabledIndex( int iIndex ) OVERRIDE;

	bool BRegisterKeyBind( const char *pchNamespace, const char *pchKeyToBind, const char *pchUIEvent, v8::Persistent<v8::Function> *pFunc, IUIPanel *pFuncContextPanel );
	bool BRegisterKeyBind( IUIPanel *pPanel, const char *pchKeyToBind, const char *pchUIEvent, v8::Persistent<v8::Function> *pFunc, IUIPanel *pFuncContextPanel );

	void OnPanelDeleted( IUIPanel *pPanel );

	// Get gamepad code value from textual name for config files, event code, etc
	virtual panorama::GamePadCode GamePadCodeFromName( const char * pchGamePadCode ) OVERRIDE;

	// Check if two gamepad codes are the 'same' button but on different vendor devices
	virtual bool BIsGamePadCodeEquivalentIgnoringVendor( GamePadCode a, GamePadCode b ) OVERRIDE;

	virtual IUIInput::EControllerPowerLevel GetConnectGamePadPowerLevel( int iGamePad ) OVERRIDE { return m_GamePadController.GetConnectGamePadPowerLevel( iGamePad ); }
	virtual void PulseGamePadHaptics( int iGamePad, float flStrength, uint32 uEffectMS ) OVERRIDE { return m_GamePadController.PulseGamePadHaptics( iGamePad, flStrength, uEffectMS ); }

#if !defined( SOURCE2_PANORAMA )
	virtual CUtlVector<EAttachedHardwareDevice> GetAttachedHardwareDevices() const OVERRIDE;
#endif

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName );
#endif
	void UpdateInputTime( InputMessage_t &msg );
	void SetVRControllerActivityTime() { m_flVRControllerLastInputTime = UIEngine()->GetCurrentFrameTime(); }

#ifdef SOURCE2_PANORAMA
	virtual bool IsIMEAllowed() const OVERRIDE;
	virtual void SetIMEAllowed( bool bAllowed ) OVERRIDE;
#endif

private:
	void ParseKeyConfig( const char *pchFileName );
	bool FindActionBinding( CUtlHashMap< ActionInput_t, InputAction_t > *pHashMap, InputMessage_t msg, InputAction_t &actionOut, const char *pchNameSpace );
	bool BRegisterKeyBindInternal( IUIPanel *pPanel, const char *pchNamespace, const char *pchKeyToBind, const char *pchUIEvent, v8::Persistent<v8::Function> *pFunc, IUIPanel *pFuncContextPanel );
	

	struct AnalogData_t
	{
		float x;
		float y;
	};

	CUtlMap<int, AnalogData_t, int, CDefLess< int > > m_MapAnalogValues;

	CUtlVector< CUtlString > m_vecKeyBindingsFilePaths;
	CUtlHashMap< ActionInput_t, InputAction_t > m_ActionBinds;

	CUtlHashMap< CPanelPtr<IUIPanel>, CUtlHashMap< ActionInput_t, InputAction_t > *, CDefEquals< CPanelPtr<IUIPanel> > > m_MapPanelBindings;
	CGamepadController m_GamePadController;
#if !defined(NO_STEAM)
#if !defined( PANORAMA_PUBLIC_STEAM_SDK )
	CSteamGameController m_SteamController;
#else
	CSteamGameControllerNew m_SteamController;
#endif
#endif

	float m_flVRControllerLastInputTime;

	CUtlVector< CUIWindowInput * > m_vecFocusedInputWindows;
	CUtlVector< IInputCapture*> m_vecInputCapture;

	EActiveControllerType m_eLastActiveControllerType;
	EActiveControllerType m_eForceActiveControllerType;
	bool m_bSawControllerInputThisFrame;
	bool m_bDebugHitTest;

#ifdef SOURCE2_PANORAMA
	bool m_bIMEAllowed;
#endif
};

} // namespace panorama

#endif // UIINPUT_H
