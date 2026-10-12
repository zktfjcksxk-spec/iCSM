//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx.h"
#include "uitoplevelwindowopenvroverlay.h"
#ifdef WIN32
#include "uitoplevelwindowwin32.h"
#include "renderer/d3d10d2dsurface.h"
#elif defined(POSIX)
#include "renderer/sdlopenglsurface.h"
#endif

#include "panorama/vrevents.h"
#include "panorama/controls/textentry.h"

#ifndef STEAMCONTROLLER_KALMAN_FILTER_ENABLED
#error "We need Kalman filtering, see trackpad_kalman.h"
#endif

#include <vrapi.h>

using namespace panorama;

DEFINE_PANORAMA_EVENT( TextEntryUpdate );

//-----------------------------------------------------------------------------
// Purpose: Class is a 'headless' top level window supporting render targets
//			that are just a surface.  More or less like CTopLevelWindowWin32
//			management without the actual hwnd final target.
//-----------------------------------------------------------------------------
CTopLevelWindowOpenVROverlay::CTopLevelWindowOpenVROverlay( CUIEngine *pUIEngineParent )
: CTopLevelWindow( pUIEngineParent )
{
	m_p3DSurface = NULL;
	m_bMouseOverWindow = false;
	m_unSurfaceWidth = 1920;
	m_unSurfaceHeight = 1080;
	m_unWindowWidth = 1920;
	m_unWindowHeight = 1080;
	m_bVisibleThisFrame = false;
	m_bVisibleLastFrame = false;
	m_bFullScreen = false;
	m_bFixedSurfaceSize = false;
	m_bUseCustomMouseCursor = true;
	m_bOculusHMD = false;
	m_bFocus = true; // BUGBUG: do we need more nuance here?
	m_ulOverlayHandle = vr::k_ulOverlayHandleInvalid;
	m_pRenderEngine = NULL;
	m_bKeepInputFocusOnGamepadFocusLost = false;
	m_ulLastKeyboardHandle = 0;
	m_unLastKeyboardEvent = 0;
	COMPILE_TIME_ASSERT( V_ARRAYSIZE( m_TouchPadData ) == vr::k_unMaxTrackedDeviceCount );
}

//-----------------------------------------------------------------------------
// Purpose: Cleanup
//-----------------------------------------------------------------------------
CTopLevelWindowOpenVROverlay::~CTopLevelWindowOpenVROverlay()
{
	Shutdown();
}


//-----------------------------------------------------------------------------
// Purpose: Post constructor startup
//-----------------------------------------------------------------------------
bool CTopLevelWindowOpenVROverlay::BInitializeSurface( int nWidth, int nHeight, vr::VROverlayHandle_t ulOverlayHandle, bool bKeepInputFocusOnGamepadFocusLost, bool bIgnoreGamepadFocus )
{
	// linux64: error: format '%llX' expects argument of type 'long long unsigned int', but argument 3 has type 'vr::VROverlayHandle_t {aka long unsigned int}' [-Werror=format]
	//m_strTargetMonitor.Format( "OpenVR Overlay %llX", ulOverlayHandle );
	m_unSurfaceWidth = nWidth;
	m_unSurfaceHeight = nHeight;
	m_unWindowWidth = nWidth;
	m_unWindowHeight = nHeight;
	m_eRenderTarget = IUIEngine::k_ERenderToOpenVROverlay;
	m_bFixedSurfaceSize = true;
	m_bEnforceWindowAspectRatio = true;
	m_bKeepInputFocusOnGamepadFocusLost = bKeepInputFocusOnGamepadFocusLost;
	m_bIgnoreGamepadFocus = bIgnoreGamepadFocus;

	if ( !vrapi::VROverlay() )
		return false;

	if ( ulOverlayHandle == vr::k_ulOverlayHandleInvalid )
		return false;

	m_ulOverlayHandle = ulOverlayHandle;

	//vrapi::VROverlay()->SetOverlayFlag( ulOverlayHandle, vr::VROverlayFlags_SendVRScrollEvents , true);
	vrapi::VROverlay()->SetOverlayFlag( ulOverlayHandle, vr::VROverlayFlags_SendVRTouchpadEvents, true );
	vrapi::VROverlay()->SetOverlayFlag( ulOverlayHandle, vr::VROverlayFlags_ShowTouchPadScrollWheel, true );

	//m_pCursorRender->SetOverlayMouseData( -32000.0f, - 32000.0f, m_unSurfaceWidth, m_unSurfaceHeight );
	m_pCursorRender->UseHardwareCursorPositionForRendering( false );
	m_pCursorRender->FadeOutCursorNow();

#ifdef WIN32
	// Now initialize the 3d surface
	CD3D10D2DSurface *pSurface = new CD3D10D2DSurface();
	if ( !pSurface->BInitialize( NULL, m_unSurfaceWidth, m_unSurfaceHeight, m_unSurfaceWidth, m_unSurfaceHeight, m_eRenderTarget, true, false, m_pCursorRender ) )
	{
		delete pSurface;
		return false;
	}

	pSurface->SetVROverlayHandle( m_ulOverlayHandle );
#elif defined(POSIX)
	COpenGLSurface *pSurface = new COpenGLSurface();
	if ( !pSurface->BInitialize( NULL, 0, m_unSurfaceWidth, m_unSurfaceHeight, m_unSurfaceWidth, m_unSurfaceHeight, m_eRenderTarget, true, false, NULL ) )
	{
		delete pSurface;
		return false;
	}
#else
#error "Surface impl please"
#endif
	m_p3DSurface = pSurface;
	m_pSurfaceInterface = pSurface;
	m_pSurfaceInterface->SetWindowScaleFactor( GetWindowScaleFactor() );

	m_pRenderEngine = new CUIRenderEngine( m_pUIEngineParent, m_p3DSurface, m_pInputEngine, this, nWidth, nHeight );

	CUtlVector< EAttachedHardwareDevice > vecDevices = UIInputEngine()->GetAttachedHardwareDevices();
	FOR_EACH_VEC( vecDevices, i )
	{
		if ( BIsOculusVRHMDHardwareDevice( vecDevices[i] ) )
		{
			m_bOculusHMD = true;
			break;
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Gets the mouse code for a vr input event
//-----------------------------------------------------------------------------
MouseCode GetMouseCodeForVREvent( const vr::VREvent_t & vrEvent )
{
	switch ( vrEvent.data.mouse.button )
	{
	case vr::VRMouseButton_Left:
		return MOUSE_LEFT;
	case vr::VRMouseButton_Right:
		// disable the auto-generated right button from the VR controller, it is touchpad click and not a good experience in BP
		return MOUSE_INVALID;
//		return MOUSE_RIGHT;
		break;
	case vr::VRMouseButton_Middle:
		return MOUSE_MIDDLE;
	}
	return MOUSE_INVALID;
}


//-----------------------------------------------------------------------------
// Purpose: map from an openvr key code to a panorama VR controller one
//-----------------------------------------------------------------------------
GamePadCode VRButtonPressToGamepadButton( vr::EVRButtonId eButton, vr::TrackedDeviceIndex_t eDeviceIndex )
{
	bool bPrimaryController =  vrapi::VROverlay()->GetPrimaryDashboardDevice() == eDeviceIndex;

	switch ( eButton )
	{
	case vr::k_EButton_A:
		return XK_NULL; // BUGBUG - ignore A button presses for now from the trackpad until we work out if/how we want to generate this on the controller
//		return eControllerRoll == vr::TrackedControllerRole_RightHand ? VR_BUTTON_PRIMARY_TRIGGER : VR_BUTTON_SECONDARY_TRIGGER;
	case vr::k_EButton_ApplicationMenu:
		return bPrimaryController ? VR_BUTTON_PRIMARY_APP : VR_BUTTON_SECONDARY_APP;
	case vr::k_EButton_Grip:
		return bPrimaryController ? VR_BUTTON_PRIMARY_GRIP : VR_BUTTON_SECONDARY_GRIP;
	case vr::k_EButton_DPad_Left:
		return bPrimaryController ? VR_BUTTON_PRIMARY_LEFT : VR_BUTTON_SECONDARY_LEFT;
	case vr::k_EButton_DPad_Up:
		return bPrimaryController ? VR_BUTTON_PRIMARY_UP : VR_BUTTON_SECONDARY_UP;
	case vr::k_EButton_DPad_Right:
		return bPrimaryController ? VR_BUTTON_PRIMARY_RIGHT : VR_BUTTON_SECONDARY_RIGHT;
	case vr::k_EButton_DPad_Down:
		return bPrimaryController ? VR_BUTTON_PRIMARY_DOWN : VR_BUTTON_SECONDARY_DOWN;
	case vr::k_EButton_SteamVR_Trigger:
		return bPrimaryController ? VR_BUTTON_PRIMARY_TRIGGER : VR_BUTTON_SECONDARY_TRIGGER;
	default:
		return XK_NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Add a VR overlay window to process events for
//-----------------------------------------------------------------------------
bool CTopLevelWindowOpenVROverlay::AddVROverlayHandleToProcess( uint64_t ulOverlayHandle )
{
	auto idx = m_ulVecAdditionalOverlayHandles.Find( ulOverlayHandle );
	if ( idx == m_ulVecAdditionalOverlayHandles.InvalidIndex() ) 
	{
		m_ulVecAdditionalOverlayHandles.AddToTail( ulOverlayHandle );
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Remove a VR overlay window to process events for
//-----------------------------------------------------------------------------
bool CTopLevelWindowOpenVROverlay::RemoveVROverlayHandleToProcess( uint64_t ulOverlayHandle )
{
	auto idx = m_ulVecAdditionalOverlayHandles.Find( ulOverlayHandle );
	if ( idx != m_ulVecAdditionalOverlayHandles.InvalidIndex() ) 
	{
		m_ulVecAdditionalOverlayHandles.Remove( idx );
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: process VR events from the main window and any additional VR overlays
//-----------------------------------------------------------------------------
void CTopLevelWindowOpenVROverlay::ProcessVROverlayEvents( vr::VROverlayHandle_t ulOverlayHandle )
{
	vr::VREvent_t vrEvent;
	while ( vrapi::VROverlay()->PollNextOverlayEvent( ulOverlayHandle, &vrEvent, sizeof(vrEvent) ) )
	{
		InputMessage_t input;
		switch ( vrEvent.eventType )
		{
		case vr::VREvent_MouseMove:
		{
			if ( !IsMouseOver() )
			{
				OnMouseEnter();

				input.m_eSource = k_ePanelEventSourceMouse;
				input.m_eInputType = k_eMouseEnter;
				UIWindowInput()->InputEvent( input );
			}

			UIWindowInput()->OnMouseMove( vrEvent.data.mouse.x, m_unWindowHeight - vrEvent.data.mouse.y ); //!!
		}
		break;
		case vr::VREvent_MouseButtonDown:
		{
			MouseCode mouseCode = GetMouseCodeForVREvent( vrEvent );
			if ( mouseCode != MOUSE_INVALID )
			{
				input.m_eSource = k_ePanelEventSourceMouse;
				input.m_eInputType = k_eMouseDown;

				input.m_MouseData.m_MouseCode = mouseCode;
				input.m_MouseData.m_Modifiers = 0;
				input.m_MouseData.m_Delta = 0;
				input.m_MouseData.m_RepeatCount = 0;
				input.m_MouseData.m_XPos = vrEvent.data.mouse.x;
				input.m_MouseData.m_YPos = m_unWindowHeight - vrEvent.data.mouse.y;

				UIWindowInput()->InputEvent( input );
				UIInputEngine()->SetVRControllerActivityTime();
			}
		}
		break;
		case vr::VREvent_MouseButtonUp:
		{
			MouseCode mouseCode = GetMouseCodeForVREvent( vrEvent );
			if ( mouseCode != MOUSE_INVALID )
			{
				input.m_eSource = k_ePanelEventSourceMouse;
				input.m_eInputType = k_eMouseUp;

				input.m_MouseData.m_MouseCode = mouseCode;
				input.m_MouseData.m_Modifiers = 0;
				input.m_MouseData.m_Delta = 0;
				input.m_MouseData.m_RepeatCount = 0;
				input.m_MouseData.m_XPos = vrEvent.data.mouse.x;
				input.m_MouseData.m_YPos = m_unWindowHeight - vrEvent.data.mouse.y;

				UIWindowInput()->InputEvent( input );
				UIInputEngine()->SetVRControllerActivityTime();
			}
		}
		break;
		case vr::VREvent_TouchPadMove:
		{
			UIInputEngine()->SetVRControllerActivityTime();

			Vector2D vecPos = Vector2D( vrEvent.data.touchPadMove.fValueXRaw, vrEvent.data.touchPadMove.fValueYRaw );

			KalmanFilter &filter = m_TouchPadData[vrEvent.trackedDeviceIndex].m_TrackpadFilter;
			WeightedMovingAverageFilter &momentum = m_TouchPadData[vrEvent.trackedDeviceIndex].m_MomentumVelFilter;
			Vector2D &vecLastFingerPos = m_TouchPadData[vrEvent.trackedDeviceIndex].m_vecLastFingerPos;
			Vector2D &vecFingerVel = m_TouchPadData[vrEvent.trackedDeviceIndex].m_vecFingerVel;
			bool bSendInputEvent = false;
			if ( vrEvent.data.touchPadMove.bFingerDown )
			{
				if ( !m_TouchPadData[vrEvent.trackedDeviceIndex].m_bLastFingerOnTouchpad )
				{
					// finger just went down, reset our state
					filter.Reset();
					momentum.Reset();
					vecLastFingerPos.Init();
					vecFingerVel.Init();
					m_TouchPadData[vrEvent.trackedDeviceIndex].m_flFingerDownTime = UIEngine()->GetCurrentFrameTime();
					float flSmoothing = 0.5f; // BUGBUG - configurable?
					filter.SetSmoothing( flSmoothing );
					float flSmoothFactor = RemapValClamped( flSmoothing, 0.0f, 1.0f, 10000.0f, 7500.0f );
					filter.SetFastQValue( Vector2D( flSmoothFactor, flSmoothFactor ) );
				}
				// update the kalman filter with the new finger pos
				filter.Update( vecPos );
				vecPos = filter.GetFilteredPos();

				Assert( vecPos.IsValid() );

				// update the event with our filtered sample
				vrEvent.data.touchPadMove.fValueXRaw = vecPos.x;
				vrEvent.data.touchPadMove.fValueYRaw = vecPos.y;

				// update the weighted avg sample vector
				momentum.AddSample( vecPos - vecLastFingerPos );

				vecLastFingerPos = vecPos;

				bSendInputEvent = true;

			}
			else //  !vrEvent.data.touchPadMove.bFingerDown
			{
				if ( m_TouchPadData[vrEvent.trackedDeviceIndex].m_bLastFingerOnTouchpad )
				{
					// finger was down and now is up, should we keep scrolling for a bit?
					Vector2D vecAverageFingerVel = momentum.GetAverage();
					float fVelMin = 0.02f;
					if ( vecAverageFingerVel.Length() > fVelMin )
					{
						// Initiate momentum
						vecFingerVel = vecAverageFingerVel;
						m_vecMomentumPads.AddToTail( vrEvent.trackedDeviceIndex );
					}
					else
					{
						bSendInputEvent = true;
					}
				}
				else
				{
					bSendInputEvent = true;
				}
			}

			if ( bSendInputEvent )
			{
				input.m_eSource = k_ePanelEventSourceMouse;
				input.m_eInputType = k_eVRTouchPad;
				input.m_VRTouchData.m_bFingerDown = vrEvent.data.touchPadMove.bFingerDown;
				input.m_VRTouchData.m_bPrimaryController = vrapi::VROverlay()->GetPrimaryDashboardDevice() == vrEvent.trackedDeviceIndex;
				input.m_VRTouchData.m_nDeviceIndex = vrEvent.trackedDeviceIndex;
				input.m_VRTouchData.m_flFingerDown = vrEvent.data.touchPadMove.flSecondsFingerDown;
				input.m_VRTouchData.m_fValueXFirst = vrEvent.data.touchPadMove.fValueXFirst;
				input.m_VRTouchData.m_fValueYFirst = vrEvent.data.touchPadMove.fValueYFirst;
				input.m_VRTouchData.m_fValueXRaw = vrEvent.data.touchPadMove.fValueXRaw;
				input.m_VRTouchData.m_fValueYRaw = vrEvent.data.touchPadMove.fValueYRaw;
				UIWindowInput()->InputEvent( input );
			}

			m_TouchPadData[vrEvent.trackedDeviceIndex].m_bLastFingerOnTouchpad = vrEvent.data.touchPadMove.bFingerDown;

		}
		break;
		case vr::VREvent_Scroll:
		{
			input.m_eSource = k_ePanelEventSourceMouse;
			input.m_eInputType = k_eMouseWheel;

			input.m_MouseData.m_MouseCode = MOUSE_LEFT;
			input.m_MouseData.m_Modifiers = 0;

			float delta = vrEvent.data.scroll.ydelta;
			delta *= (UIEngine()->GetWheelScrollLines() * 20);
			input.m_MouseData.m_Delta = delta;

			if ( input.m_MouseData.m_Delta != 0.0f )
			{
				//Msg( "%0.2f\n", vrEvent.data.scroll.ydelta );
				UIWindowInput()->InputEvent( input );
			}
		}
		break;
		
		case vr::VREvent_KeyboardCharInput:
		{
			m_ulLastKeyboardHandle = vrEvent.data.keyboard.uUserValue;
			m_unLastKeyboardEvent = vr::VREvent_KeyboardCharInput;

			if ( vrEvent.data.keyboard.cNewInput[0] == 0 && vrEvent.data.keyboard.uUserValue != 0 )
			{
				// Non-minimal mode, just get the whole buffer

				// The uUserValue is a CPanelPtr handle so we don't crash if the panel goes away while the keyboard is up
				CPanelPtr<CPanel2D> panelPtr;
				panelPtr.SetFromUInt64( vrEvent.data.keyboard.uUserValue );
				CPanel2D *pTextEntry = panelPtr.Get();
				if ( pTextEntry != NULL )
				{

					// This TextEntryUpdate event only exists to pass through to the TextEntry and have it refresh its text
					DispatchEvent( TextEntryUpdate(), pTextEntry );
				}
				break;
			}


			input.m_eSource = k_ePanelEventSourceKeyboard;
			input.m_KeyData.m_Modifiers = 0;
			input.m_KeyData.m_RepeatCount = 0;


			// Some special keys need to be sent as a keydown instead of a keychar
			for ( int i = 0; i < sizeof( vrEvent.data.keyboard.cNewInput ) && vrEvent.data.keyboard.cNewInput[i]; i++ )
			{
				if ( vrEvent.data.keyboard.cNewInput[i] == '\b' )
				{
					input.m_eInputType = k_eKeyDown;
					input.m_KeyData.m_KeyCode = KEY_BACKSPACE;
					input.m_KeyData.m_UniChar = 0;
				}
				else if ( vrEvent.data.keyboard.cNewInput[i] == '\n' )
				{
					input.m_eInputType = k_eKeyDown;
					input.m_KeyData.m_KeyCode = KEY_ENTER;
					input.m_KeyData.m_UniChar = 0;
				}
				else
				{
					// Need to send a keydown for the web browser to work correctly
					// but don't send a keyup because that breaks it???
					// TODO- convert character to a keycode and provide correct keycode for
					// the web browser engine
					input.m_eInputType = k_eKeyDown;
					input.m_KeyData.m_KeyCode = KEY_NONE;
					input.m_KeyData.m_UniChar = vrEvent.data.keyboard.cNewInput[i];
					UIWindowInput()->InputEvent( input );

					input.m_eInputType = k_eKeyChar;
					input.m_KeyData.m_KeyCode = KEY_NONE;
					input.m_KeyData.m_UniChar = vrEvent.data.keyboard.cNewInput[i];
				}

				UIWindowInput()->InputEvent( input );
			}
		}
		break;

		case vr::VREvent_KeyboardDone:
		{
			input.m_eSource = k_ePanelEventSourceKeyboard;
			input.m_KeyData.m_Modifiers = 0;
			input.m_KeyData.m_RepeatCount = 0;
			input.m_eInputType = k_eKeyDown;
			input.m_KeyData.m_KeyCode = KEY_ENTER;
			input.m_KeyData.m_UniChar = 0;
			UIWindowInput()->InputEvent(input);

			m_ulLastKeyboardHandle = vrEvent.data.keyboard.uUserValue;
			m_unLastKeyboardEvent = vr::VREvent_KeyboardDone;
		}
		break;

		case vr::VREvent_KeyboardClosed:
		{
			if ( vrEvent.data.keyboard.uUserValue != 0 )
			{
				// The uUserValue is a CPanelPtr handle so we don't crash if the panel goes away while the keyboard is up
				CPanelPtr<CPanel2D> panelPtr;
				panelPtr.SetFromUInt64( vrEvent.data.keyboard.uUserValue );
				CPanel2D *pTextEntry = panelPtr.Get();
				if ( pTextEntry != NULL )
				{
					char buffer[1024];
					vrapi::VROverlay()->GetKeyboardText( buffer, sizeof( buffer ) );
					panorama::DispatchEvent( panorama::TextInputFinished(), pTextEntry, (vrEvent.data.keyboard.uUserValue == m_ulLastKeyboardHandle && m_unLastKeyboardEvent == vr::VREvent_KeyboardDone), buffer );

					m_ulLastKeyboardHandle = vrEvent.data.keyboard.uUserValue;
					m_unLastKeyboardEvent = vr::VREvent_KeyboardClosed;
				}
			}
		}
		break;

		case vr::VREvent_FocusEnter:
		{
			input.m_eSource = k_ePanelEventSourceMouse;
			input.m_eInputType = k_eMouseEnter;

			UIWindowInput()->InputEvent( input );
			OnMouseEnter();
		}
		break;

		case vr::VREvent_FocusLeave:
		{
			input.m_eSource = k_ePanelEventSourceMouse;
			input.m_eInputType = k_eMouseLeave;

			UIWindowInput()->InputEvent( input );
			OnMouseLeave();
		}
		break;

		case vr::VREvent_ShowKeyboard:
		{
			DispatchEvent( VRShowKeyboard(), (IUIPanel*) NULL );
			//pDebugHandler->SetDebugTargetPanel( pVRKeyboard->UIPanel() );
			break;
		}
		case vr::VREvent_HideKeyboard:
		{
			DispatchEvent( VRHideKeyboard(), (IUIPanel*) NULL );
			break;
		}


		case vr::VREvent_RenderToast:
		{
			DispatchEvent( VRRenderToast(), (IUIPanel*)NULL, vrEvent.data.notification.notificationId );
		}
		break;

		case vr::VREvent_DashboardRequested:
		{
			DispatchEvent( VRDashboardRequested(), (IUIPanel*)NULL, vrEvent.trackedDeviceIndex, vrEvent.data.overlay.overlayHandle );
		}
		break;

		case vr::VREvent_DashboardThumbSelected:
		{
			DispatchEvent( VRDashboardThumbSelected(), (IUIPanel*)NULL, vrEvent.data.overlay.overlayHandle );
		}
		break;

		case vr::VREvent_ResetDashboard:
		{
			DispatchEvent( VRResetDashboard(), (IUIPanel*)NULL );
		}
		break;

		case vr::VREvent_StatusUpdate:
		{
			DispatchEvent( VRStatusUpdate(), (IUIPanel*)NULL, (vr::EVRState)vrEvent.data.status.statusState);
		}
		break;

		case vr::VREvent_OverlayShown:
		{
			DispatchEvent( VROverlayShown(), (IUIPanel*)NULL, ulOverlayHandle );
		}
		break;

		case vr::VREvent_OverlayHidden:
		{
			DispatchEvent( VROverlayHidden(), (IUIPanel*)NULL, ulOverlayHandle );
		}
		break;

		case vr::VREvent_ImageLoaded:
		{
			DispatchEvent( VROverlayImageLoaded(), (IUIPanel*)NULL, ulOverlayHandle );
		}
		break;

		case vr::VREvent_Notification_Shown:
		{
			DispatchEvent( VRNotificationShown(), ( IUIPanel* )NULL, ulOverlayHandle, vrEvent.data.notification.notificationId, vrEvent.data.notification.ulUserValue );
		}
		break;

		case vr::VREvent_Notification_Hidden:
		{
			DispatchEvent( VRNotificationHidden(), ( IUIPanel* )NULL, ulOverlayHandle, vrEvent.data.notification.notificationId, vrEvent.data.notification.ulUserValue );
		}
		break;

		case vr::VREvent_Notification_BeginInteraction:
		{
			DispatchEvent( VRNotificationBeginInteraction(), ( IUIPanel* )NULL, ulOverlayHandle, vrEvent.data.notification.notificationId, vrEvent.data.notification.ulUserValue );
		}
		break;

		case vr::VREvent_Notification_Destroyed:
		{
			DispatchEvent( VRNotificationDestroyed(), ( IUIPanel* )NULL, ulOverlayHandle, vrEvent.data.notification.notificationId, vrEvent.data.notification.ulUserValue );
		}
		break;

		case vr::VREvent_ButtonPress:
		case vr::VREvent_ButtonUnpress:
			// we'll only get these if the compositor re-forwarded the button to us
			switch ( vrEvent.data.controller.button )
			{
			case vr::k_EButton_ApplicationMenu:
				if ( vrEvent.eventType == vr::VREvent_ButtonPress )
				{
					DispatchEvent( VRApplicationMenuButtonDown(), UIWindowInput()->GetInputFocus() );
				}
				else
				{
					DispatchEvent( VRApplicationMenuButtonUp(), UIWindowInput()->GetInputFocus() );
				}
				break;
			default:
			{
				input.m_eSource = k_ePanelEventSourceGamepad;
				input.m_eInputType = vrEvent.eventType == vr::VREvent_ButtonPress ? k_eGamePadDown : k_eGamePadUp;
				input.m_GamePadData.m_GamePadCode = VRButtonPressToGamepadButton( (vr::EVRButtonId)vrEvent.data.controller.button, vrEvent.trackedDeviceIndex);
				input.m_GamePadData.m_RepeatCount = 0;
				input.m_GamePadData.m_fValue = 0.0f;

				//	Msg( "Input event: %d %d \n", xKey, nRepeats );
				if ( input.m_GamePadData.m_GamePadCode != XK_NULL )
				{
					UIWindowInput()->InputEvent( input );
				}
			}
				break;
			}
			break;

		case vr::VREvent_OverlayGamepadFocusGained:
		{
			// set this window to have input focus so gamepad events will flow through Panorama
			UIWindowInput()->GotWindowFocus();
			UIWindowInput()->RereadControllerState();

			// Send an event so the panel can do whatever else it needs to do
			DispatchEvent( VRGamepadFocusGained(), (IUIPanel*)NULL, ulOverlayHandle );
		}
		break;

		case vr::VREvent_OverlayGamepadFocusLost:
		{
			if ( !m_bKeepInputFocusOnGamepadFocusLost )
			{
				UIWindowInput()->LostWindowFocus();
			}

			// Send an event so the panel can do whatever else it needs to do
			DispatchEvent( VRGamepadFocusLost(), (IUIPanel*)NULL, ulOverlayHandle );
		}
		break;

		case vr::VREvent_DashboardActivated:
		case vr::VREvent_DashboardDeactivated:
		{
			DispatchEvent( VRDashboardVisibilityChanged(), (IUIPanel*)NULL, vrEvent.eventType == vr::VREvent_DashboardActivated );
		}
		break;

		case vr::VREvent_ChaperoneDataHasChanged:
		case vr::VREvent_ChaperoneUniverseHasChanged:
		{
			DispatchEvent( VRChaperoneChanged(), (IUIPanel*)NULL );
		}
		break;

		case vr::VREvent_TrackedDeviceActivated:
		case vr::VREvent_TrackedDeviceDeactivated:
		case vr::VREvent_TrackedDeviceUpdated:
		case vr::VREvent_ButtonTouch:
		case vr::VREvent_ButtonUntouch:
		case vr::VREvent_InputFocusCaptured:
		case vr::VREvent_InputFocusReleased:
		case vr::VREvent_SceneFocusLost:
		case vr::VREvent_SceneFocusGained:
		case vr::VREvent_Quit: // This one we do care about, we just handle it globally in vrmanager.cpp
		case vr::VREvent_ProcessQuit:
		{
			// swallow these known events we don't want to turn into Panorama events
			// If you have cause to use one of these feel free to add a new event. Theses cases are just to
			// reduce the generic event spam.
		}
		break;

		default:
		{
			// send the unknown event for everything else so apps can use new events while they're still waiting for 
			// an OpenVR SDK to get to Steam. This lets the apps thats build in //vr/steamvr iterate more quickly
			// in many cases.
			DispatchEvent( VRUnknownEvent(), (IUIPanel*)NULL, vrEvent );

		}
		break;

		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: redo buffers/surfaces
//-----------------------------------------------------------------------------
void CTopLevelWindowOpenVROverlay::RunPlatformFrame()
{
	BaseClass::RunPlatformFrame();

	ProcessVROverlayEvents( m_ulOverlayHandle );
	FOR_EACH_VEC( m_ulVecAdditionalOverlayHandles, i )
	{
		ProcessVROverlayEvents( m_ulVecAdditionalOverlayHandles[i] );
	}

	FOR_EACH_VEC_BACK( m_vecMomentumPads, iVec )
	{
		int iPad = m_vecMomentumPads[iVec];
		// check to see if we have momentum still running on a pad
		if ( !m_TouchPadData[iPad].m_bLastFingerOnTouchpad && m_TouchPadData[iPad].m_vecFingerVel.IsValid() 
				&& !m_TouchPadData[iPad].m_vecFingerVel.IsZero() )
		{
			// Decay momentum {slow, fast}
			const float k_fMomentumDecayLevel = 0.9f;

			// smooth-step interpolate each axis' decay ratio based on the velocity
			float fMomentumDecayX = MapRange( m_TouchPadData[iPad].m_vecFingerVel.Length(), -1.0f, 1.0f, k_fMomentumDecayLevel, k_fMomentumDecayLevel );
			float fMomentumDecayY = MapRange( m_TouchPadData[iPad].m_vecFingerVel.Length(), -1.0f, 1.0f, k_fMomentumDecayLevel, k_fMomentumDecayLevel );

			// now slow the velocity of the pad
			m_TouchPadData[iPad].m_vecFingerVel *= Vector2D( fMomentumDecayX, fMomentumDecayY );

			// Once the cursor has slowed below a threshold turn off momentum
			float fMomentumThresh = 0.001f;
			if ( m_TouchPadData[iPad].m_vecFingerVel.Length() < fMomentumThresh )
			{
				m_TouchPadData[iPad].m_vecFingerVel.Init();
				m_vecMomentumPads.Remove( iVec );
			}

			m_TouchPadData[iPad].m_vecLastFingerPos += m_TouchPadData[iPad].m_vecFingerVel; // the movement if the current finger pos plus the velocity from this frame
			InputMessage_t input;
			input.m_eSource = k_ePanelEventSourceMouse;
			input.m_eInputType = k_eVRTouchPad;
			input.m_VRTouchData.m_bFingerDown = false;
			input.m_VRTouchData.m_bPrimaryController = vrapi::VROverlay()->GetPrimaryDashboardDevice() == (vr::TrackedDeviceIndex_t)iPad;
			input.m_VRTouchData.m_nDeviceIndex = iPad;
			input.m_VRTouchData.m_flFingerDown = UIEngine()->GetCurrentFrameTime() - m_TouchPadData[iPad].m_flFingerDownTime;
			input.m_VRTouchData.m_fValueXFirst = 0;
			input.m_VRTouchData.m_fValueYFirst = 0;
			input.m_VRTouchData.m_fValueXRaw = m_TouchPadData[iPad].m_vecLastFingerPos.x;
			input.m_VRTouchData.m_fValueYRaw = m_TouchPadData[iPad].m_vecLastFingerPos.y;
			UIWindowInput()->InputEvent( input );
		}
		else
		{
			m_vecMomentumPads.Remove( iVec );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Determine whether to allow an input message currently
//-----------------------------------------------------------------------------
bool CTopLevelWindowOpenVROverlay::BAllowInput( InputMessage_t &msg )
{
	if ( BIsGuideButton( msg ) )
		return true;

	if ( msg.m_eSource == k_ePanelEventSourceGamepad )
	{
		if ( m_bOculusHMD && msg.m_GamePadData.m_GamePadCode == XK_BUTTON_BACK )
			return true;

		// the start button switches between overlays, so it always needs to go through
		if ( m_bIgnoreGamepadFocus || msg.m_GamePadData.m_GamePadCode == XK_BUTTON_START || msg.m_GamePadData.m_GamePadCode == STEAM_BUTTON_START )
			return true;

		if ( vrapi::VROverlay()->GetGamepadFocusOverlay() != m_ulOverlayHandle &&  !vrapi::VROverlay()->IsHoverTargetOverlay( m_ulOverlayHandle ) )
		{
			return false;
		}
	}
	return BIsVisible();
}


//-----------------------------------------------------------------------------
// Purpose: Return true if Steam has gamepad focus in the VR dashboard
//-----------------------------------------------------------------------------
bool CTopLevelWindowOpenVROverlay::BIsVROverlayFocused()
{
	if ( vrapi::VROverlay()->GetGamepadFocusOverlay() == m_ulOverlayHandle ||  vrapi::VROverlay()->IsHoverTargetOverlay(m_ulOverlayHandle) )
	{
		return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Called when mouse moves
//-----------------------------------------------------------------------------
void CTopLevelWindowOpenVROverlay::OnMouseMove( float x, float y )
{
}

//-----------------------------------------------------------------------------
// Purpose: Called when focus is set or lost
//-----------------------------------------------------------------------------
void CTopLevelWindowOpenVROverlay::SetFocus( bool bFocus )
{
	m_bFocus = bFocus;
}

//-----------------------------------------------------------------------------
// Purpose: directly set target game window size
//-----------------------------------------------------------------------------
void CTopLevelWindowOpenVROverlay::SetGameWindowSize( uint32 nWidth, uint32 nHeight )
{
	if ( nWidth == m_unGameWidth && nHeight == m_unGameHeight )
		return;

	m_unGameWidth = nWidth;
	m_unGameHeight = nHeight;
}


//-----------------------------------------------------------------------------
// Purpose: directly set fixed surface size bypassing normal resize/scaling logic
//-----------------------------------------------------------------------------
void CTopLevelWindowOpenVROverlay::SetFixedSurfaceSize( uint32 unSurfaceWidth, uint32 unSurfaceHeight )
{
	Assert( m_bFixedSurfaceSize );

	if ( m_unWindowWidth != unSurfaceWidth || m_unWindowHeight != unSurfaceHeight 
		|| m_unSurfaceWidth != unSurfaceWidth || m_unSurfaceHeight != unSurfaceHeight )
	{
		m_unWindowWidth = unSurfaceWidth;
		m_unWindowHeight = unSurfaceHeight;
		m_unSurfaceWidth = unSurfaceWidth;
		m_unSurfaceHeight = unSurfaceHeight;

		FOR_EACH_LL( m_listVisiblePanels, i )
		{
			m_listVisiblePanels[i]->InvalidateSizeAndPosition();
		}

		FOR_EACH_LL( m_listInvisiblePanels, i )
		{
			m_listInvisiblePanels[i]->InvalidateSizeAndPosition();
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: redo buffers/surfaces
//-----------------------------------------------------------------------------
void CTopLevelWindowOpenVROverlay::OnWindowResize( uint32 nWidth, uint32 nHeight )
{
	// BUGBUG Joe - Assert false and nuke this code?
	if ( nWidth == m_unGameWidth && nHeight == m_unGameHeight )
		return;

	m_unGameWidth = nWidth;
	m_unGameHeight = nHeight;

	if ( !m_bFixedSurfaceSize )
	{
		if ( !m_bEnforceWindowAspectRatio )
		{
			m_unWindowWidth = nWidth;
			m_unWindowHeight = nHeight;
		}
		else
		{
			int dx = 0, dy = 0;
			uint32 unSurfaceWidth = 0;
			uint32 unSurfaceHeight = 0;
			UIEngine()->UISettings()->GetPreferredResolution( dx, dy );
			unSurfaceHeight = dy;
			unSurfaceWidth = dx;

			// Our max overlay res is 1080p, or 720p if specified on the command line. enforce this
			// by rounding down to the next size based on the game's window size and the preferred
			// size from the settings.

			if ( unSurfaceHeight >= 1080 && m_unGameWidth >= 1920 && m_unGameHeight >= 1080 )
			{
				unSurfaceWidth = 1920;
				unSurfaceHeight = 1080;
			}
			else if ( unSurfaceHeight >= 720 && m_unGameWidth >= 1280 && m_unGameHeight >= 720 )
			{
				unSurfaceWidth = 1280;
				unSurfaceHeight = 720;
			}
			else 
			{
				// small game window, or preferred size is very small
				unSurfaceWidth = MIN( m_unGameWidth, unSurfaceWidth );
				unSurfaceHeight = RoundFloatToInt( unSurfaceWidth * 0.5625f );

				// perhaps the game has a 2.35:1 window or something and we are now too tall
				if ( unSurfaceHeight > m_unGameHeight + 1 )
				{
					unSurfaceHeight = m_unGameHeight;
					unSurfaceWidth = RoundFloatToInt( unSurfaceHeight / 0.5625f );
				}
			}

			m_unWindowWidth = unSurfaceWidth;
			m_unWindowHeight = unSurfaceHeight;
			m_unSurfaceWidth = unSurfaceWidth;
			m_unSurfaceHeight = unSurfaceHeight;

			SetWindowScaleFactor( (float)m_unWindowHeight / 1080.0f );
		}

		FOR_EACH_LL( m_listVisiblePanels, i )
		{
			m_listVisiblePanels[i]->InvalidateSizeAndPosition();
		}

		FOR_EACH_LL( m_listInvisiblePanels, i )
		{
			m_listInvisiblePanels[i]->InvalidateSizeAndPosition();
		}
	}		
}


//-----------------------------------------------------------------------------
// Purpose: Return window visibility based on its underlying overlay
//-----------------------------------------------------------------------------
bool CTopLevelWindowOpenVROverlay::BIsVisible()
{
	return vrapi::VROverlay() && vrapi::VROverlay()->IsOverlayVisible( m_ulOverlayHandle );
}


//-----------------------------------------------------------------------------
// Purpose: Should be a no-op
//-----------------------------------------------------------------------------
void CTopLevelWindowOpenVROverlay::SetWindowPosition( float x, float y )
{

}


//-----------------------------------------------------------------------------
// Purpose: always origin
//-----------------------------------------------------------------------------
void CTopLevelWindowOpenVROverlay::GetWindowPosition( float &x, float &y )
{
	x = y = 0.0;
}


//-----------------------------------------------------------------------------
// Purpose: always origin,size
//-----------------------------------------------------------------------------
void CTopLevelWindowOpenVROverlay::GetWindowBounds( float &left, float &top, float &right, float &bottom )
{
	left = top = 0.0;
	right = m_unSurfaceWidth;
	bottom = m_unSurfaceHeight;
}

//-----------------------------------------------------------------------------
// Purpose: same as texture/surface size
//-----------------------------------------------------------------------------
void CTopLevelWindowOpenVROverlay::GetClientDimensions( float &width, float &height )
{
	width = m_unSurfaceWidth;
	height = m_unSurfaceHeight;
}


//-----------------------------------------------------------------------------
// Purpose: no op
//-----------------------------------------------------------------------------
void CTopLevelWindowOpenVROverlay::Activate( bool bForceful )
{
	REFERENCE( bForceful );
}


//-----------------------------------------------------------------------------
// Purpose: no op
//-----------------------------------------------------------------------------
void CTopLevelWindowOpenVROverlay::Minimize()
{
}


//-----------------------------------------------------------------------------
// Purpose: Called when mouse enters our window
//-----------------------------------------------------------------------------
void CTopLevelWindowOpenVROverlay::OnMouseEnter()
{
	DispatchEvent( WindowCursorShown(), (IUIPanel*)NULL, this );
	m_bMouseOverWindow = true;
}


//-----------------------------------------------------------------------------
// Purpose: Called when mouse leaves our window
//-----------------------------------------------------------------------------
void CTopLevelWindowOpenVROverlay::OnMouseLeave()
{
	DispatchEvent( WindowCursorHidden(), (IUIPanel*)NULL, this );
	m_bMouseOverWindow = false;
}


//-----------------------------------------------------------------------------
// Purpose: Set mouse cursor
//-----------------------------------------------------------------------------
void CTopLevelWindowOpenVROverlay::SetMouseCursor( EMouseCursors eCursor )
{
	m_eCursorCurrent = eCursor;
}


//-----------------------------------------------------------------------------
// Purpose: Enable/disable the cursor for movements with the controller
//-----------------------------------------------------------------------------
void CTopLevelWindowOpenVROverlay::EnableControllerCursor( bool bEnable )
{
	if ( m_pCursorRender )
	{
		if ( bEnable )
		{
			m_pCursorRender->SetHideOnGamepadActivity( false );
			m_pCursorRender->WakeupMouseCursor();
		}
		else
		{
			m_pCursorRender->SetHideOnGamepadActivity( true );
			m_pCursorRender->FadeOutCursorNow();
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Return an image source for the current mouse cursor
//-----------------------------------------------------------------------------
IImageSource *CTopLevelWindowOpenVROverlay::GetMouseCursorTexture( Vector2D *pptHotspot )
{
	// If we're using the laser pointer, and we don't want to drag a cursor around the screen, 
	// so return the invisible cursor for this case.
	if ( UIInputEngine()->GetActiveControllerType() == k_EActiveControllerType_VR )
	{
		return m_pMouseCursor->GetTexture( eMouseCursor_None, pptHotspot );
	}
	else
	{
		return m_pMouseCursor->GetTexture( m_eCursorCurrent, pptHotspot );
	}
}


//-----------------------------------------------------------------------------
// Purpose: cleanup
//-----------------------------------------------------------------------------
void CTopLevelWindowOpenVROverlay::Shutdown()
{
	CTopLevelWindow::Shutdown();

	SAFE_DELETE( m_pRenderEngine );
	SAFE_DELETE( m_p3DSurface );
}


//-----------------------------------------------------------------------------
// Purpose: Handle pushing against an edge with a gamepad
//-----------------------------------------------------------------------------
bool CTopLevelWindowOpenVROverlay::BOnMoveEdge( panorama::EFocusMoveDirection moveType )
{
	vr::EOverlayDirection eDirection;
	switch ( moveType )
	{
	case k_ENextByXPosition:
		eDirection = vr::OverlayDirection_Right;
		break;
	case k_EPrevByXPosition:
		eDirection = vr::OverlayDirection_Left;
		break;
	case k_ENextByYPosition:
		eDirection = vr::OverlayDirection_Down;
		break;
	case k_EPrevByYPosition:
		eDirection = vr::OverlayDirection_Up;
		break;
	default:
		return false;
	}

	vr::VROverlayError error = vrapi::VROverlay()->MoveGamepadFocusToNeighbor( eDirection, m_ulOverlayHandle );
	return error == vr::VROverlayError_None;
}


#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: validate mem
//-----------------------------------------------------------------------------
void CTopLevelWindowOpenVROverlay::Validate( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE();

#ifdef WIN32
	ValidatePtr( (CD3D10D2DSurface *)m_p3DSurface );
#elif defined(POSIX)
	ValidatePtr( (COpenGLSurface *)m_p3DSurface );
#endif
	CTopLevelWindow::Validate( validator, pchName );
}
#endif
