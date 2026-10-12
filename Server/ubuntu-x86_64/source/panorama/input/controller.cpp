//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx.h"
#include "controller.h"
#include "panorama/panoramacurves.h"
#if !defined( SOURCE2_PANORAMA )
#include "usb_ids.h"
#endif
#include <SDL.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

#define JOYSTICK_ANALOG_BUTTON_THRESHOLD	XBX_MAX_STICKSAMPLE_LEFT * 0.4f
#define JOYSTICK_ANALOG_TRIGGER_THRESHOLD	XBX_MAX_STICKSAMPLE_TRIGGER * 0.7f

ConVar g_ConVarPanoramaJoystickAxisRepeatIntervalStart( "@panorama_joystick_axis_repeat_interval_start", "0.22" );
ConVar g_ConVarPanoramaJoystickAxisRepeatIntervalEnd( "@panorama_joystick_axis_repeat_interval_end", "0.05" );
ConVar g_ConVarPanoramaJoystickAxisRepeatCurveTime( "@panorama_joystick_axis_repeat_curve_time", "1.0" );

ConVar g_ConVarPanoramaJoystickButtonRepeatIntervalStart( "@panorama_joystick_button_repeat_interval_start", "0.48" );
ConVar g_ConVarPanoramaJoystickButtonRepeatIntervalEnd( "@panorama_joystick_button_repeat_interval_end", "0.10" );
ConVar g_ConVarPanoramaJoystickButtonRepeatCurveTime( "@panorama_joystick_button_repeat_curve_time", "1.2" );

const int XINPUT_GAMEPAD_TRIGGER_THRESHOLD = 300;

static const int s_nSDLJoyButtonMapping[] =
{
	XK_BUTTON_A, //SDL_CONTROLLER_BUTTON_A,
	XK_BUTTON_B, //	SDL_CONTROLLER_BUTTON_B,
	XK_BUTTON_X, //	SDL_CONTROLLER_BUTTON_X,
	XK_BUTTON_Y, //	SDL_CONTROLLER_BUTTON_Y,
	XK_BUTTON_BACK, //	SDL_CONTROLLER_BUTTON_Back,
	XK_BUTTON_GUIDE, //	SDL_CONTROLLER_BUTTON_Guide,
	XK_BUTTON_START, //	SDL_CONTROLLER_BUTTON_Start,
	XK_BUTTON_STICK1, //	SDL_CONTROLLER_BUTTON_LeftStick,
	XK_BUTTON_STICK2, //	SDL_CONTROLLER_BUTTON_RightStick,
	XK_BUTTON_LEFT_SHOULDER,//	SDL_CONTROLLER_BUTTON_LeftShoulder,
	XK_BUTTON_RIGHT_SHOULDER, //	SDL_CONTROLLER_BUTTON_RightShoulder,
	XK_BUTTON_UP, //	SDL_CONTROLLER_BUTTON_DPAD_Up,
	XK_BUTTON_DOWN, //	SDL_CONTROLLER_BUTTON_DPAD_Down,
	XK_BUTTON_LEFT,// 	SDL_CONTROLLER_BUTTON_DPAD_Left,
	XK_BUTTON_RIGHT, //	SDL_CONTROLLER_BUTTON_DPAD_Right,
};

namespace panorama
{
bool BIsGamePadCodeEquivalentIgnoringVendor( GamePadCode a, GamePadCode b )
{
	if( a == b )
		return true;

	switch( a )
	{
	case XK_BUTTON_START:
		if( b == STEAM_BUTTON_START )
			return true;
		return false;
	case XK_BUTTON_BACK:
		if( b == STEAM_BUTTON_SELECT )
			return true;
		return false;
	case XK_BUTTON_STICK1:
		if( b == STEAM_BUTTON_LPAD_CLICKED )
			return true;
		return false;
	case XK_BUTTON_STICK2:
		if( b == STEAM_BUTTON_RPAD_CLICKED )
			return true;
		return false;
	case XK_BUTTON_A:
		if( b == STEAM_BUTTON_A )
			return true;
		return false;
	case XK_BUTTON_B:
		if( b == STEAM_BUTTON_B )
			return true;
		return false;
	case XK_BUTTON_X:
		if( b == STEAM_BUTTON_X )
			return true;
		return false;
	case XK_BUTTON_Y:
		if( b == STEAM_BUTTON_Y )
			return true;
		return false;
	case XK_BUTTON_LEFT_SHOULDER:
		if( b == STEAM_BUTTON_LSHOULDER )
			return true;
		return false;
	case XK_BUTTON_RIGHT_SHOULDER:
		if( b == STEAM_BUTTON_RSHOULDER )
			return true;
		return false;
	case XK_BUTTON_LTRIGGER:
		if( b == STEAM_BUTTON_LTRIGGER )
			return true;
		return false;
	case XK_BUTTON_RTRIGGER:
		if( b == STEAM_BUTTON_RTRIGGER )
			return true;
		return false;
	case XK_STICK1_UP:
		if( b == STEAM_LEFTPAD_UP )
			return true;
		return false;
	case XK_STICK1_DOWN:
		if( b == STEAM_LEFTPAD_DOWN )
			return true;
		return false;
	case XK_STICK1_LEFT:
		if( b == STEAM_LEFTPAD_LEFT )
			return true;
		return false;
	case XK_STICK1_RIGHT:
		if( b == STEAM_LEFTPAD_RIGHT )
			return true;
		return false;
	case XK_STICK2_UP:
		if( b == STEAM_RIGHTPAD_UP )
			return true;
		return false;
	case XK_STICK2_DOWN:
		if( b == STEAM_RIGHTPAD_DOWN )
			return true;
		return false;
	case XK_STICK2_LEFT:
		if( b == STEAM_RIGHTPAD_LEFT )
			return true;
		return false;
	case XK_STICK2_RIGHT:
		if( b == STEAM_RIGHTPAD_RIGHT )
			return true;
		return false;
	case XK_BUTTON_GUIDE:
		if( b == STEAM_BUTTON_GUIDE )
			return true;
		return false;
	case STEAM_LEFTPAD_UP:
		if( b == XK_STICK1_UP )
			return true;
		return false;
	case STEAM_LEFTPAD_DOWN:
		if( b == XK_STICK1_DOWN )
			return true;
		return false;
	case STEAM_LEFTPAD_LEFT:
		if( b == XK_STICK1_LEFT )
			return true;
		return false;
	case STEAM_LEFTPAD_RIGHT:
		if( b == XK_STICK1_RIGHT )
			return true;
		return false;
	case STEAM_RIGHTPAD_UP:
		if( b == XK_STICK2_UP )
			return true;
		return false;
	case STEAM_RIGHTPAD_DOWN:
		if( b == XK_STICK2_DOWN )
			return true;
		return false;
	case STEAM_RIGHTPAD_LEFT:
		if( b == XK_STICK2_LEFT )
			return true;
		return false;
	case STEAM_RIGHTPAD_RIGHT:
		if( b == XK_STICK2_RIGHT )
			return true;
		return false;
	case STEAM_BUTTON_LTRIGGER:
		if( b == XK_BUTTON_LTRIGGER )
			return true;
		return false;
	case STEAM_BUTTON_RTRIGGER:
		if( b == XK_BUTTON_RTRIGGER )
			return true;
		return false;
	case STEAM_BUTTON_LSHOULDER:
		if( b == XK_BUTTON_LEFT_SHOULDER )
			return true;
		return false;
	case STEAM_BUTTON_RSHOULDER:
		if( b == XK_BUTTON_RIGHT_SHOULDER )
			return true;
		return false;
	case STEAM_BUTTON_LBACK:
		return false;
	case STEAM_BUTTON_RBACK:
		return false;
	case STEAM_BUTTON_GUIDE:
		if( b == XK_BUTTON_GUIDE )
			return true;
		return false;
	case STEAM_BUTTON_SELECT:
		if( b == XK_BUTTON_BACK )
			return true;
		return false;
	case STEAM_BUTTON_START:
		if( b == XK_BUTTON_START )
			return true;
		return false;
	case STEAM_BUTTON_LPAD_CLICKED:
		if( b == XK_BUTTON_STICK1 )
			return true;
		return false;
	case STEAM_BUTTON_RPAD_CLICKED:
		if( b == XK_BUTTON_STICK2 )
			return true;
		return false;
	case STEAM_BUTTON_A:
		if( b == XK_BUTTON_A )
			return true;
		return false;
	case STEAM_BUTTON_B:
		if( b == XK_BUTTON_B )
			return true;
		return false;
	case STEAM_BUTTON_X:
		if( b == XK_BUTTON_X )
			return true;
		return false;
	case STEAM_BUTTON_Y:
		if( b == XK_BUTTON_Y )
			return true;
		return false;

	default:
		return false;
	}
}
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CGamepadController::CGamepadController( IUIInput *pInputParent )
{
	m_pInputParent = pInputParent;
	m_bHadGamepadInput = false;
	m_bGamepadConnectedThisSession = false;
	m_bGamepadUsedThisSession = false;
	m_pXDeviceForLastButtonPress = NULL;
	m_flLastUserIDAssignment = 0.0f;
#if defined( SOURCE2_PANORAMA )
	m_bUserDisabled = CommandLine()->HasParm( "-nojoy" ) && !CommandLine()->HasParm( "-panoramajoy" );
#else
	m_bUserDisabled = false;
#endif

	// copied from k_EAnimationEaseIn
	Vector2D vecPoints[4];
	panorama::GetAnimationCurveControlPoints( panorama::k_EAnimationEaseIn, vecPoints );
	m_AxisRepeatCurve.SetControlPoints( vecPoints );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CGamepadController::~CGamepadController()
{
	Shutdown();
}


//-----------------------------------------------------------------------------
// Purpose: return what we think reasonable deadzone values are for this stick
//-----------------------------------------------------------------------------
float CGamepadController::GetDeadZoneValue( GamePadCode code )
{
	switch( code )
	{
	default:
	case XK_STICK1_UP:
	case XK_STICK1_DOWN:
	case XK_STICK1_LEFT:
	case XK_STICK1_RIGHT:
	case XK_STICK1_ANALOG:
	case STEAM_LEFTPAD_ANALOG:
		return 	(((float)XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE)/XBX_MAX_STICKSAMPLE_LEFT)*1000;
	case XK_STICK2_UP:
	case XK_STICK2_DOWN:
	case XK_STICK2_LEFT:
	case XK_STICK2_RIGHT:
	case XK_STICK2_ANALOG:
	case STEAM_RIGHTPAD_ANALOG:
		return 	(((float)XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE)/XBX_MAX_STICKSAMPLE_RIGHT)*1000;
	}
}


//-----------------------------------------------------------------------------
// Counts the number of active gamepads connected
//-----------------------------------------------------------------------------
int CGamepadController::GetNumGamepadsConnected() const
{
	return m_XDevices.Count();
}


//-----------------------------------------------------------------------------
//	Purpose: Post Xbox events, ignoring key repeats
//-----------------------------------------------------------------------------
void CGamepadController::PostXKeyEvent( xdevice_t *pDevice, GamePadCode xKey, int nSample )
{
	int nSampleThreshold = 0; 
	float	value	= 0.f;

	bool bAnalogTrigger = false;
	bool bAnalogMove = false;
	bool bLeftStick = false;

	// Look for changes on the analog axes
	switch( xKey )
	{
	case XK_STICK1_LEFT:
	case XK_STICK1_RIGHT:
		{
			bLeftStick = true;
			bAnalogMove = true;
			value = ( xKey == XK_STICK1_LEFT ) ? -nSample : nSample;
			nSampleThreshold = ( int )( JOYSTICK_ANALOG_BUTTON_THRESHOLD );
		}
		break;

	case XK_STICK1_UP:
	case XK_STICK1_DOWN:
		{
			bLeftStick = true;
			bAnalogMove = true;
			value = ( xKey == XK_STICK1_UP ) ? -nSample : nSample;
			nSampleThreshold = ( int )( JOYSTICK_ANALOG_BUTTON_THRESHOLD );
		}
		break;

	case XK_STICK2_LEFT:
	case XK_STICK2_RIGHT:
		{
			bAnalogMove = true;
			value = ( xKey == XK_STICK2_LEFT ) ? -nSample : nSample;
			nSampleThreshold = ( int )( JOYSTICK_ANALOG_BUTTON_THRESHOLD );
		}
		break;

	case XK_STICK2_UP:
	case XK_STICK2_DOWN:
		{
			bAnalogMove = true;
			value = ( xKey == XK_STICK2_UP ) ? -nSample : nSample;
			nSampleThreshold = ( int )( JOYSTICK_ANALOG_BUTTON_THRESHOLD );
		}
		break;

	case XK_BUTTON_LTRIGGER:
	case XK_BUTTON_RTRIGGER:
		{
			bAnalogTrigger = true;
			nSampleThreshold = ( int )( JOYSTICK_ANALOG_TRIGGER_THRESHOLD );
		}
		break;

	default:
		break;
	}

	NOTE_UNUSED( value );

	bool bKeyUpThisFrame = false; // keep track of if we released this frame

	// store the key
	pDevice->m_appXKeys[xKey].sample = nSample;
	int nRepeats = pDevice->m_appXKeys[xKey].repeats;
	if ( nSample > nSampleThreshold && pDevice->m_appXKeys[xKey].flLastInput < UIEngine()->GetCurrentFrameTime() )
	{
		if (pDevice->m_appXKeys[xKey].repeats == 0 )
			pDevice->m_appXKeys[xKey].flFirstInput = UIEngine()->GetCurrentFrameTime(); // first down since release, start tracking when this started
		pDevice->m_appXKeys[xKey].repeats++;

		double flDelayTime = 0;
		switch ( xKey )
		{
		case XK_STICK1_LEFT:
		case XK_STICK1_RIGHT:
		case XK_STICK1_UP:
		case XK_STICK1_DOWN:
		case XK_STICK2_LEFT:
		case XK_STICK2_RIGHT:
		case XK_STICK2_UP:
		case XK_STICK2_DOWN:
		case XK_BUTTON_UP:
		case XK_BUTTON_DOWN:
		case XK_BUTTON_LEFT:
		case XK_BUTTON_RIGHT:
			{
				Vector2D vRes;
				m_AxisRepeatCurve.Evaluate( clamp( ( UIEngine( )->GetCurrentFrameTime( ) - pDevice->m_appXKeys[xKey].flFirstInput ) / g_ConVarPanoramaJoystickAxisRepeatCurveTime.GetFloat(), 0.0f, 1.0f ), vRes );
				flDelayTime = Lerp( vRes.y, g_ConVarPanoramaJoystickAxisRepeatIntervalStart.GetFloat(), g_ConVarPanoramaJoystickAxisRepeatIntervalEnd.GetFloat() );
				break;
			}
		default:
			// all the other buttons
			{
				Vector2D vRes;
				m_AxisRepeatCurve.Evaluate( clamp( ( UIEngine( )->GetCurrentFrameTime( ) - pDevice->m_appXKeys[xKey].flFirstInput ) / g_ConVarPanoramaJoystickButtonRepeatCurveTime.GetFloat(), 0.0f, 1.0f ), vRes );
				flDelayTime = Lerp( vRes.y, g_ConVarPanoramaJoystickButtonRepeatIntervalStart.GetFloat(), g_ConVarPanoramaJoystickButtonRepeatIntervalEnd.GetFloat() );
			}
		}

		pDevice->m_appXKeys[xKey].flLastInput = UIEngine()->GetCurrentFrameTime() + flDelayTime;  // fire the next down at this time
	}
	else if ( nSample < nSampleThreshold || nSample == 0 )
	{
		if ( pDevice->m_appXKeys[xKey].repeats )
			bKeyUpThisFrame = true; // had down events, now up

		pDevice->m_appXKeys[xKey].repeats = 0;
		nSample = 0;
		nRepeats = 0;
		pDevice->m_appXKeys[xKey].flLastInput = 0.0f;
		pDevice->m_appXKeys[xKey].flFirstInput = 0.0f;
	}
	else
	{
		return; // not time for a key repeat yet
	}

	if ( m_pXDeviceForLastButtonPress == NULL )
	{
		m_pXDeviceForLastButtonPress = pDevice;
		m_flLastUserIDAssignment = UIEngine()->GetCurrentFrameTime();
	}

	if ( bAnalogMove && m_pXDeviceForLastButtonPress != pDevice )
		return; // don't fire the analog move event as this isn't the primary controller

	if ( nSample == 0 && !bKeyUpThisFrame )
		return; // only fire 1 key up for a button

	if ( nSample != 0 )
	{
		m_pXDeviceForLastButtonPress = pDevice;
		m_flLastUserIDAssignment = UIEngine()->GetCurrentFrameTime();
	}

	m_bHadGamepadInput = true;

	InputMessage_t input;
	input.m_eSource = k_ePanelEventSourceGamepad;
	input.m_eInputType = nSample? k_eGamePadDown : k_eGamePadUp;
	input.m_GamePadData.m_GamePadCode = xKey;
	input.m_GamePadData.m_RepeatCount = nRepeats;
	if ( bAnalogTrigger )
	{
		input.m_GamePadData.m_fValue = ((float)nSample/XBX_MAX_STICKSAMPLE_TRIGGER)*1000;
	}
	else if ( bAnalogMove )
	{
		if ( bLeftStick )
			input.m_GamePadData.m_fValue = ((float)nSample/XBX_MAX_STICKSAMPLE_LEFT)*1000;
		else
			input.m_GamePadData.m_fValue = ((float)nSample/XBX_MAX_STICKSAMPLE_RIGHT)*1000;
	}

//	Msg( "Input event: %d %d \n", xKey, nRepeats );

	m_pInputParent->InputEvent( input );
}



//-----------------------------------------------------------------------------
//	Purpose: turn analog axis movement into keypresses
//-----------------------------------------------------------------------------
void CGamepadController::HandleXDeviceAxis( xdevice_t *pXDevice, int nAxisValue, GamePadCode negativeKey, GamePadCode positiveKey, int axisID )
{
	GamePadCode key = XK_NULL;

	int nThreshold = JOYSTICK_ANALOG_BUTTON_THRESHOLD;
	if ( ( axisID == LEFT_TRIGGER_AXIS ) || ( axisID == RIGHT_TRIGGER_AXIS ) )
		nThreshold = JOYSTICK_ANALOG_TRIGGER_THRESHOLD;

	if ( abs(nAxisValue) > nThreshold )
	{
		// Queue stick axis push response
		if ( nAxisValue < 0 )
		{
			key = negativeKey;
			PostXKeyEvent( pXDevice, negativeKey, -nAxisValue );
		}
		else if ( nAxisValue > 0 )
		{
			key = positiveKey;
			PostXKeyEvent( pXDevice, positiveKey, nAxisValue );
		}
	}

	if ( pXDevice->lastStickKeys[axisID] != XK_NULL && pXDevice->lastStickKeys[axisID] != key )
	{
		// Queue stick axis release response
		PostXKeyEvent( pXDevice, pXDevice->lastStickKeys[axisID], 0 );
	}
	pXDevice->lastStickKeys[axisID] = key;


}


//-----------------------------------------------------------------------------
// Event filter for SDL to see controller insertion/removal
//-----------------------------------------------------------------------------
int CGamepadController::SDL_GameControllerEventWatcher(void *userdata, SDL_Event * event)
{
	CGamepadController *pControllerManager = (CGamepadController *)userdata;
	
	bool bNeedMappingsUpdate = true;

	switch( event->type )
	{
	case SDL_CONTROLLERDEVICEADDED:
		pControllerManager->OpenXDevice( event->cdevice.which, false );
		break;
	case SDL_JOYDEVICEADDED:
		pControllerManager->OpenXDevice( event->jdevice.which, false );
		break;
	case SDL_CONTROLLERDEVICEREMOVED:
		pControllerManager->CloseXDevice( event->cdevice.which, false );
		break;
	case SDL_JOYDEVICEREMOVED:
		pControllerManager->CloseXDevice( event->jdevice.which, false );
		break;
	default:
		bNeedMappingsUpdate = false;
		break;
	}
	
	if ( bNeedMappingsUpdate && pControllerManager->m_pSettings )
	{
		CUtlString sExtraMappings;
		pControllerManager->GetSDLGameControllerMappings( &sExtraMappings );
		pControllerManager->m_pSettings->UpdateGamepadMappingHints( sExtraMappings.String() );
	}

	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: setup any controllers
//-----------------------------------------------------------------------------
void CGamepadController::Initialize( IUISettings *pSettings )
{
	m_pSettings = pSettings;

	if ( !m_bUserDisabled )
	{
		SDL_SetHint( SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1" );

		SDL_InitSubSystem( SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC );
		int nJoystickCount = SDL_NumJoysticks();	
		if ( nJoystickCount )
		{
			for ( int i = 0; i < nJoystickCount; ++i )
			{
				OpenXDevice( i, false );
			}
		}
	}

	UIEngine()->RegisterForUnhandledEvent( GameControllerMappingChanged::symbol, UtlMakeDelegate( this, &CGamepadController::OnActionGamepadMappingsReload ).GetAbstractDelegate() );

	if ( !m_bUserDisabled )
	{
		SDL_AddEventWatch( CGamepadController::SDL_GameControllerEventWatcher, this );
	}
	
	if ( pSettings )
	{
		CUtlString sExtraMappings;
		GetSDLGameControllerMappings( &sExtraMappings );
		pSettings->UpdateGamepadMappingHints( sExtraMappings.String() );
	}
}

//-----------------------------------------------------------------------------
//	Purpose: shutdown joystick support
//-----------------------------------------------------------------------------
void CGamepadController::Shutdown()
{
	UIEngine()->UnregisterForUnhandledEvent( GameControllerMappingChanged::symbol, UtlMakeDelegate( this, &CGamepadController::OnActionGamepadMappingsReload ).GetAbstractDelegate() );

	FOR_EACH_VEC( m_XDevices, i )
	{
		CloseXDevice( m_XDevices[i].instance_id, false );
	}
	m_XDevices.RemoveAll();
	m_pXDeviceForLastButtonPress = NULL;

	if ( !m_bUserDisabled )
	{
		SDL_DelEventWatch( CGamepadController::SDL_GameControllerEventWatcher, this );
		SDL_QuitSubSystem( SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC );
	}
}


//-----------------------------------------------------------------------------
//	Purpose: Open an Xbox controller
//-----------------------------------------------------------------------------
void CGamepadController::OpenXDevice( int device_index, bool bRemoteInput )
{
	
	// See if this device is steam controller emulating a gamepad
#if !defined( SOURCE2_PANORAMA )
	if ( !bRemoteInput )
	{
		SDL_JoystickGUID guid = SDL_JoystickGetDeviceGUID( device_index );
		uint8_t guidSdlVirtualGamepad[] = k_SdlGuidVirtualGamepad;
		if ( V_memcmp( &guid.data, &guidSdlVirtualGamepad, sizeof( guid.data ) ) == 0 )
			return;
	}
#endif

	int iXDevice = m_XDevices.AddToTail();
	xdevice_t	*pXDevice = &m_XDevices[iXDevice];
	pXDevice->active = false;
	pXDevice->m_pGameController = NULL;
	pXDevice->m_pJoystick = NULL;
	pXDevice->m_bRemoteInput = bRemoteInput;
	V_memset( pXDevice->m_arrAxisValues, 0, sizeof( pXDevice->m_arrAxisValues ) );
	V_memset( pXDevice->m_arrButtonValues, 0, sizeof( pXDevice->m_arrButtonValues ) );
	V_memset( pXDevice->m_appXKeys, 0x0, sizeof(pXDevice->m_appXKeys) );
	V_memset( pXDevice->lastStickKeys, 0x0, sizeof(pXDevice->lastStickKeys) );

	SDL_Joystick *pSDLJoystick = NULL;
	SDL_GameController *pSDLGameController = NULL;
	if ( bRemoteInput )
	{
		pXDevice->nAxes = SDL_CONTROLLER_AXIS_MAX;
		pXDevice->nButtons = SDL_CONTROLLER_BUTTON_MAX;
		pXDevice->instance_id = device_index;
	}
	else
	{
		pSDLGameController = SDL_GameControllerOpen( device_index );
		if ( !pSDLGameController )
		{
			pSDLJoystick = SDL_JoystickOpen( device_index );
			if ( pSDLJoystick )
			{
				int nAxes = SDL_JoystickNumAxes( pSDLJoystick );
				int nButtons = SDL_JoystickNumButtons( pSDLJoystick );

				if ( nAxes < 2 || nButtons < 2 ) // we need at least 2 buttons and a X/Y axis
				{
					SDL_JoystickClose( pSDLJoystick );
					pSDLJoystick = NULL;
				}
				
				pXDevice->nAxes = nAxes;
				pXDevice->nButtons = nButtons;
				pXDevice->instance_id = SDL_JoystickInstanceID(  pSDLJoystick );

			}
		}
		else
		{
			pXDevice->nAxes = SDL_CONTROLLER_AXIS_MAX;
			pXDevice->nButtons = SDL_CONTROLLER_BUTTON_MAX;
			pXDevice->instance_id = SDL_JoystickInstanceID( SDL_GameControllerGetJoystick( pSDLGameController ) );
		}
	}

	FOR_EACH_VEC( m_XDevices, i )
	{
		if ( i != iXDevice && m_XDevices[i].instance_id == pXDevice->instance_id && m_XDevices[i].m_bRemoteInput == pXDevice->m_bRemoteInput )
		{
			if ( pSDLGameController )
				SDL_GameControllerClose( pSDLGameController );
			if ( pSDLJoystick )
				SDL_JoystickClose( pSDLJoystick );

			m_XDevices.Remove( iXDevice ); // already have this device open, don't do it again
			return;

		}
	}

	if ( pSDLGameController || pSDLJoystick || bRemoteInput )
	{
		const char *Name = NULL;
		if ( pSDLGameController )
			Name = SDL_GameControllerName( pSDLGameController );
		else if ( pSDLJoystick )
			Name = SDL_JoystickName( pSDLJoystick );
		else
			Name = "Streaming Gamepad";
		
		
		if ( pSDLGameController )
		{
			char ControllerGUID[256];
			SDL_JoystickGetGUIDString( SDL_JoystickGetDeviceGUID( device_index ), ControllerGUID, 256 );


			// Strip off the GUID provided by SDL in the mapping string and
			// provide the real GUID, as generic Xinput devices don't have one in the mapping string
			pXDevice->sMappingString.Format( "%s%s\n", ControllerGUID,
											 V_strstr( SDL_GameControllerMapping( pSDLGameController ), "," ) );
		}

		SDL_Haptic *pHaptic = nullptr;
		if ( pSDLGameController )
			pHaptic = SDL_HapticOpenFromJoystick( SDL_GameControllerGetJoystick( pSDLGameController ) );
		else if ( pSDLJoystick )
			pHaptic = SDL_HapticOpenFromJoystick( pSDLJoystick );

		if ( pHaptic && ( !SDL_HapticRumbleSupported(pHaptic) || SDL_HapticRumbleInit( pHaptic ) != 0 ) )
		{
			SDL_HapticClose( pHaptic );
			pHaptic = nullptr;
		}

		Msg( "Opening joystick %i : %s\n", device_index, Name );

		pXDevice->active	  = true;
		pXDevice->quitTimeout = 0;
		pXDevice->m_pGameController = pSDLGameController;
		pXDevice->m_pJoystick = pSDLJoystick;
		pXDevice->m_pHaptic = pHaptic;

		// left stick, default to narrow zone
		pXDevice->stickThreshold[STICK1_AXIS_X]  = XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE;
		pXDevice->stickScale[STICK1_AXIS_X]      = XBX_STICK_SCALE_LEFT( XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE );
		pXDevice->stickZeroed[STICK1_AXIS_X] = false;
		pXDevice->stickThreshold[STICK1_AXIS_Y] = XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE;
		pXDevice->stickScale[STICK1_AXIS_Y]      = XBX_STICK_SCALE_DOWN( XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE );
		pXDevice->stickZeroed[STICK1_AXIS_Y] = false;

		// right stick, default to narrow zone
		pXDevice->stickThreshold[STICK2_AXIS_X]  = XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE;
		pXDevice->stickScale[STICK2_AXIS_X]      = XBX_STICK_SCALE_LEFT( XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE );
		pXDevice->stickZeroed[STICK2_AXIS_X] = false;
		pXDevice->stickThreshold[STICK2_AXIS_Y] = XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE;
		pXDevice->stickScale[STICK2_AXIS_Y]      = XBX_STICK_SCALE_DOWN( XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE );
		pXDevice->stickZeroed[STICK2_AXIS_Y] = false;

		// triggers, default to trigger zone
		pXDevice->stickThreshold[LEFT_TRIGGER_AXIS]  = XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
		pXDevice->stickScale[LEFT_TRIGGER_AXIS]      = XBX_STICK_SCALE_TRIGGER( XINPUT_GAMEPAD_TRIGGER_THRESHOLD );
		pXDevice->stickThreshold[RIGHT_TRIGGER_AXIS]  = XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
		pXDevice->stickScale[RIGHT_TRIGGER_AXIS]      = XBX_STICK_SCALE_TRIGGER( XINPUT_GAMEPAD_TRIGGER_THRESHOLD );

		for ( int i = 0; i < MAX_STICKAXIS; i++ )
		{
			pXDevice->lastStickKeys[i] = XK_NULL;
		}

		m_bGamepadConnectedThisSession = true;
	}
	else
	{
		m_XDevices.Remove( iXDevice ); // failed to open, remove it
	}
}


//-----------------------------------------------------------------------------
//	Purpose: Close an Xbox controller
//-----------------------------------------------------------------------------
void CGamepadController::CloseXDevice( int instance_id, bool bRemoteInput )
{
	xdevice_t *pXDevice = NULL;
	int iDevice = 0;
	FOR_EACH_VEC( m_XDevices, i )
	{
		if ( m_XDevices[i].instance_id == instance_id && m_XDevices[i].m_bRemoteInput == bRemoteInput )
		{
			iDevice = i;
			pXDevice = &m_XDevices[i];
			break;
		}
	}

	if ( !pXDevice )
		return;

	pXDevice->active = false;
	Msg( "Closing joystick %i\n", instance_id );

	// Controller unplugged, release buttons of the specific joystick that was unplugged
	for ( int j = 0; j < XK_MAX_KEYS; j++ ) 
	{
		if ( pXDevice->m_appXKeys[j].repeats )
		{
			InputMessage_t input;
			input.m_eSource = k_ePanelEventSourceGamepad;
			input.m_eInputType = k_eGamePadUp;
			input.m_GamePadData.m_GamePadCode = (GamePadCode)j;
			input.m_GamePadData.m_RepeatCount = 0;
			m_pInputParent->InputEvent( input );
		}
	}

	if ( pXDevice->m_pHaptic )
		SDL_HapticClose( pXDevice->m_pHaptic );
	if ( pXDevice->m_pGameController )
		SDL_GameControllerClose( pXDevice->m_pGameController );
	if ( pXDevice->m_pJoystick )
		SDL_JoystickClose( pXDevice->m_pJoystick );
	pXDevice->m_pGameController = NULL;
	pXDevice->m_pJoystick = NULL;
	if ( m_pXDeviceForLastButtonPress == pXDevice )
		m_pXDeviceForLastButtonPress = NULL;
	m_XDevices.Remove( iDevice );
}


//-----------------------------------------------------------------------------
//	Purpose: Process any remote gamepad input
//-----------------------------------------------------------------------------
void CGamepadController::ProcessRemoteInput()
{
	VPROF_BUDGET( "CGamepadController::ProcessRemoteInput", VPROF_BUDGETGROUP_TENFOOT );

	xdevice_t *pXDevice;

	RemoteGamepadInput_t Input;
	while ( m_RemoteGamepadInput.PopItem( &Input ) )
	{
		switch ( Input.m_eGamepadInput )
		{
		case k_ERemoteGamepadAttached:
			pXDevice = NULL;
			FOR_EACH_VEC( m_XDevices, i )
			{
				if ( m_XDevices[i].instance_id == Input.m_nInstanceID && m_XDevices[i].m_bRemoteInput )
				{
					pXDevice = &m_XDevices[i];
					break;
				}
			}

			// Don't duplicate a new entry if it was already added.
			if ( pXDevice )
				break;

			OpenXDevice( Input.m_nInstanceID, true );
			break;
		case k_ERemoteGamepadDetached:
			CloseXDevice( Input.m_nInstanceID, true );
			break;
		case k_ERemoteGamepadAxisChanged:
		case k_ERemoteGamepadButtonChanged:
			pXDevice = NULL;
			FOR_EACH_VEC( m_XDevices, i )
			{
				if ( m_XDevices[i].instance_id == Input.m_nInstanceID && m_XDevices[i].m_bRemoteInput )
				{
					pXDevice = &m_XDevices[i];
					break;
				}
			}

			if ( !pXDevice )
			{
				// This device must have existed before Big Picture started
				OpenXDevice( Input.m_nInstanceID, true );
				pXDevice = &m_XDevices[ m_XDevices.Count() - 1 ];
			}

			if ( Input.m_eGamepadInput == k_ERemoteGamepadAxisChanged )
				pXDevice->m_arrAxisValues[ Input.m_nAxis ] = Input.m_nAxisValue;
			else
				pXDevice->m_arrButtonValues[ Input.m_nButton ] = Input.m_nButtonValue;
			break;
		}
	}
}

int NormalizeStickValue_Cross( int nValue, int nThreshold, float flScale )
{
	if ( nValue >= -nThreshold )
	{
		if ( nValue <= nThreshold )
		{
			return 0;
		}
		else
		{
			return ( int )( ( nValue - nThreshold ) * flScale );
		}
	}
	else
	{
		return ( int )( ( nValue + nThreshold ) * flScale );
	}
}

//-----------------------------------------------------------------------------
//	Purpose: Sample the Xbox controllers.
//-----------------------------------------------------------------------------
void CGamepadController::RunFrame( void )
{
	VPROF_BUDGET( "CGamepadController::RunFrame", VPROF_BUDGETGROUP_TENFOOT );

#ifdef WIN32
	// We need to manually pump the joystick on win32 as we aren't using the normal SDL frameloop for it, for all
	// our other supported platforms we use the full SDL frame loop so this isn't needed
	if ( !m_bUserDisabled )
	{
		SDL_JoystickUpdate();
	}
#endif

	ProcessRemoteInput();

	FOR_EACH_VEC( m_XDevices, i )
	{
		xdevice_t *pXDevice = &m_XDevices[i];
		if ( !pXDevice->m_pGameController && !pXDevice->m_pJoystick && !pXDevice->m_bRemoteInput )
			continue;

		ReadXDevice( pXDevice );
	}
}


//-----------------------------------------------------------------------------
//	Purpose: get the value of a gamecontroller or joystick axis
//-----------------------------------------------------------------------------
int16 CGamepadController::GetAxisValue( xdevice_t* pXDevice, SDL_GameControllerAxis axis )
{
	if ( pXDevice->m_pGameController )
		return SDL_GameControllerGetAxis( pXDevice->m_pGameController, axis );
	else if ( pXDevice->m_pJoystick )
	{
		// use a generic mapping for joysticks, if it works then great
		int nJoystickAxis = -1;
		switch ( axis )
		{
		case SDL_CONTROLLER_AXIS_LEFTX:
			nJoystickAxis = 0;
			break;
		case SDL_CONTROLLER_AXIS_LEFTY:
			nJoystickAxis = 1;
			break;
		case SDL_CONTROLLER_AXIS_RIGHTX:
			nJoystickAxis = 2;
			break;
		case SDL_CONTROLLER_AXIS_RIGHTY:
			nJoystickAxis = 3;
			break;
		case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
		case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
			// no mapping for triggers
		default:
			break;
		}

		if ( nJoystickAxis >= 0 && nJoystickAxis < pXDevice->nAxes )
			return SDL_JoystickGetAxis( pXDevice->m_pJoystick, nJoystickAxis );

		return 0;
	}
	else if ( pXDevice->m_bRemoteInput )
		return pXDevice->m_arrAxisValues[ axis ];
	else
		return 0;
}


//-----------------------------------------------------------------------------
//	Purpose: get the value of a gamecontroller or joystick button
//-----------------------------------------------------------------------------
uint8 CGamepadController::GetButtonValue( xdevice_t* pXDevice, SDL_GameControllerButton button )
{
	if ( pXDevice->m_pGameController )
		return SDL_GameControllerGetButton( pXDevice->m_pGameController, button );
	else if ( pXDevice->m_pJoystick )
	{
		// use a generic mapping for joysticks, if it works then great
		int nJoystickButton = -1;
		switch ( button )
		{
		case SDL_CONTROLLER_BUTTON_A:
			nJoystickButton = 1;
			break;
		case SDL_CONTROLLER_BUTTON_B:
			nJoystickButton = 2;
			break;
		case SDL_CONTROLLER_BUTTON_X:
			nJoystickButton = 0;
			break;
		case SDL_CONTROLLER_BUTTON_Y:
			nJoystickButton = 3;
			break;
		default:
			break;
		}

		if ( nJoystickButton >= 0 && nJoystickButton < pXDevice->nButtons )
			return SDL_JoystickGetButton( pXDevice->m_pJoystick, nJoystickButton );

		return 0;
	}
	else if ( pXDevice->m_bRemoteInput )
		return pXDevice->m_arrButtonValues[ button ];
	else
		return 0;
}


//-----------------------------------------------------------------------------
//	Purpose: Check for the 2 stick axii hitting a zero point, so we start considering input from them
//-----------------------------------------------------------------------------
void CGamepadController::CheckStickZeroed( xdevice_t *pxDevice, int nX1, int nY1, int nX2, int nY2 )
{
	if ( !pxDevice->stickZeroed[STICK1_AXIS_X] && abs( nX1 ) < XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE )
	{
		pxDevice->stickZeroed[STICK1_AXIS_X] = true;
	}

	if ( !pxDevice->stickZeroed[STICK1_AXIS_Y] && abs( nY1 ) < XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE )
	{
		pxDevice->stickZeroed[STICK1_AXIS_Y] = true;
	}

	if ( !pxDevice->stickZeroed[STICK2_AXIS_X] && abs( nX2 ) < XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE )
	{
		pxDevice->stickZeroed[STICK2_AXIS_X] = true;
	}

	if ( !pxDevice->stickZeroed[STICK2_AXIS_Y] && abs( nY2 ) < XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE )
	{
		pxDevice->stickZeroed[STICK2_AXIS_Y] = true;
	}
}


//-----------------------------------------------------------------------------
//	Purpose: Queue input key events for a device
//-----------------------------------------------------------------------------
void CGamepadController::ReadXDevice( xdevice_t* pXDevice )
{
	int nX1 = GetAxisValue( pXDevice, SDL_CONTROLLER_AXIS_LEFTX );
	int nY1 = -1*GetAxisValue( pXDevice, SDL_CONTROLLER_AXIS_LEFTY );
	int nX2 = GetAxisValue( pXDevice, SDL_CONTROLLER_AXIS_RIGHTX );
	int nY2 = -1*GetAxisValue( pXDevice, SDL_CONTROLLER_AXIS_RIGHTY );

	//Msg( "%d,%d %d,%d\n", nX1, nY1, nX2, nY2 );

	CheckStickZeroed( pXDevice, nX1, nY1, nX2, nY2);

	nX1 = pXDevice->stickZeroed[STICK1_AXIS_X] ? NormalizeStickValue_Cross( nX1, pXDevice->stickThreshold[STICK1_AXIS_X], pXDevice->stickScale[STICK1_AXIS_X] ) : 0;
	nY1 = pXDevice->stickZeroed[STICK1_AXIS_Y] ? NormalizeStickValue_Cross( nY1, pXDevice->stickThreshold[STICK1_AXIS_Y], pXDevice->stickScale[STICK1_AXIS_Y] ) : 0;
	nX2 = pXDevice->stickZeroed[STICK2_AXIS_X] ? NormalizeStickValue_Cross( nX2, pXDevice->stickThreshold[STICK2_AXIS_X], pXDevice->stickScale[STICK2_AXIS_X] ) : 0;
	nY2 = pXDevice->stickZeroed[STICK2_AXIS_Y] ? NormalizeStickValue_Cross( nY2, pXDevice->stickThreshold[STICK2_AXIS_Y], pXDevice->stickScale[STICK2_AXIS_Y] ) : 0;

	// if this is a different controller and we haven't seen input from the others for a while AND the analog sticks are deflected on this, let it take over
	if ( m_pXDeviceForLastButtonPress != pXDevice &&  ( ( UIEngine()->GetCurrentFrameTime() - m_flLastUserIDAssignment ) > k_flAnalogUserInputTimeout || m_pXDeviceForLastButtonPress == NULL )
		&& ( abs(nX1) > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE || abs(nY1) > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE || abs(nX2) > XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE || abs(nY2) > XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE ) )
	{
		m_pXDeviceForLastButtonPress = pXDevice;
		m_flLastUserIDAssignment = UIEngine()->GetCurrentFrameTime(); // stick was deflected, lets assume they are still using this controller
	}

	// only fire analog events for the last controller that pressed a button
	if ( m_pXDeviceForLastButtonPress == pXDevice &&
#if !defined(NO_STEAM)
		( m_flLastUserIDAssignment > m_pInputParent->GetLastSteamControllerActiveTime() || m_pInputParent->GetLastSteamControllerActiveTime() < 0.001f ) )
#else
		( m_flLastUserIDAssignment > m_pInputParent->GetLastGamePadControllerActiveTime() || m_pInputParent->GetLastGamePadControllerActiveTime() < 0.001f ) )
#endif
	{
		// raw analog stick position
		InputMessage_t input;
		input.m_eSource = k_ePanelEventSourceGamepad;
		input.m_eInputType = k_eGamePadAnalog;

		input.m_GamePadData.m_GamePadCode = XK_STICK1_ANALOG;
		input.m_GamePadData.m_RepeatCount = pXDevice->m_appXKeys[XK_STICK1_ANALOG].repeats;
		input.m_GamePadData.m_fValueX = ((float)nX1/XBX_MAX_STICKSAMPLE_LEFT)*1000;
		input.m_GamePadData.m_fValueY = ((float)nY1/XBX_MAX_STICKSAMPLE_RIGHT)*1000;
		m_pInputParent->InputEvent( input );

		input.m_GamePadData.m_GamePadCode = XK_STICK2_ANALOG;
		input.m_GamePadData.m_RepeatCount = pXDevice->m_appXKeys[XK_STICK2_ANALOG].repeats;
		input.m_GamePadData.m_fValueX = ((float)nX2/XBX_MAX_STICKSAMPLE_LEFT)*1000;
		input.m_GamePadData.m_fValueY = ((float)nY2/XBX_MAX_STICKSAMPLE_RIGHT)*1000;
		m_pInputParent->InputEvent( input );

	}

	// stick axis "pressed" events
	HandleXDeviceAxis( pXDevice, nX1, XK_STICK1_LEFT, XK_STICK1_RIGHT, STICK1_AXIS_X );
	HandleXDeviceAxis( pXDevice, nY1, XK_STICK1_DOWN, XK_STICK1_UP, STICK1_AXIS_Y );
	HandleXDeviceAxis( pXDevice, nX2, XK_STICK2_LEFT, XK_STICK2_RIGHT, STICK2_AXIS_X );
	HandleXDeviceAxis( pXDevice, nY2, XK_STICK2_DOWN, XK_STICK2_UP, STICK2_AXIS_Y );

	// Trigger events
	int nLT = 0, nRT = 0;
	nLT = GetAxisValue( pXDevice, SDL_CONTROLLER_AXIS_TRIGGERLEFT );
	if ( nLT < pXDevice->stickThreshold[LEFT_TRIGGER_AXIS] )
		nLT = 0;
	else
		nLT *= pXDevice->stickScale[LEFT_TRIGGER_AXIS];

	nRT =GetAxisValue( pXDevice, SDL_CONTROLLER_AXIS_TRIGGERRIGHT );
	if ( nRT < pXDevice->stickThreshold[RIGHT_TRIGGER_AXIS] )
		nRT = 0;
	else
		nRT *= pXDevice->stickScale[RIGHT_TRIGGER_AXIS];

	// only fire analog events for the last controller that pressed a button
	if ( m_pXDeviceForLastButtonPress == pXDevice &&
#if !defined(NO_STEAM)
		( m_flLastUserIDAssignment > m_pInputParent->GetLastSteamControllerActiveTime() || m_pInputParent->GetLastSteamControllerActiveTime() < 0.001f ) )
#else
		( m_flLastUserIDAssignment > m_pInputParent->GetLastGamePadControllerActiveTime() || m_pInputParent->GetLastGamePadControllerActiveTime() < 0.001f ) )
#endif
	{
		// raw analog trigger position
		InputMessage_t input;
		input.m_eSource = k_ePanelEventSourceGamepad;
		input.m_eInputType = k_eGamePadAnalog;

		input.m_GamePadData.m_GamePadCode = XK_BUTTON_LTRIGGER;
		input.m_GamePadData.m_RepeatCount = pXDevice->m_appXKeys[XK_BUTTON_LTRIGGER].repeats;
		input.m_GamePadData.m_fValueX = ((float)nLT/XBX_MAX_STICKSAMPLE_TRIGGER)*1000;
		input.m_GamePadData.m_fValueY = input.m_GamePadData.m_fValueX;
		m_pInputParent->InputEvent( input );

		input.m_GamePadData.m_GamePadCode = XK_BUTTON_RTRIGGER;
		input.m_GamePadData.m_RepeatCount = pXDevice->m_appXKeys[XK_BUTTON_RTRIGGER].repeats;
		input.m_GamePadData.m_fValueX = ((float)nRT/XBX_MAX_STICKSAMPLE_TRIGGER)*1000;
		input.m_GamePadData.m_fValueY = input.m_GamePadData.m_fValueX;
		m_pInputParent->InputEvent( input );
	}

	// Trigger "pressed" events
	HandleXDeviceAxis( pXDevice, nLT, XK_BUTTON_LTRIGGER, XK_BUTTON_LTRIGGER, LEFT_TRIGGER_AXIS );
	HandleXDeviceAxis( pXDevice, nRT, XK_BUTTON_RTRIGGER, XK_BUTTON_RTRIGGER, RIGHT_TRIGGER_AXIS );	

	for ( int sdlbutton = 0; sdlbutton < SDL_CONTROLLER_BUTTON_MAX; sdlbutton++ )
	{
		int i = s_nSDLJoyButtonMapping[sdlbutton];
		bool bButtonPressed = GetButtonValue( pXDevice, (SDL_GameControllerButton)sdlbutton ) != 0;
		if ( !bButtonPressed )
		{
			// button that we saw down on getting window focus is now up, allow future key presses from it
			pXDevice->m_appXKeys[ i ].m_bSuppressUntilUp = false;
		}

		// ignore the key if it is not down AND we don't have repeats stored for it (if we have repeats we need to KEYUP it)
		if ( !bButtonPressed && pXDevice->m_appXKeys[i].repeats == 0 )
			continue;

		if ( bButtonPressed && pXDevice->m_appXKeys[ i ].m_bSuppressUntilUp )
			continue; 

		int				sample;
		if ( bButtonPressed )
		{
			// down event
			sample = XBX_MAX_BUTTONSAMPLE;
		}
		else
		{
			// up event
			sample = 0;
		}

		// No changes if packet numbers match
		if ( bButtonPressed )
			m_bHadGamepadInput = true;

		if ( bButtonPressed )
			m_bGamepadUsedThisSession = true;


		// we already did key pressed for the analog axii above, only do real buttons here
		if ( i != XK_STICK1_UP && i != XK_STICK1_DOWN && i != XK_STICK1_LEFT && i != XK_STICK1_RIGHT
			&& i != XK_STICK2_UP && i !=  XK_STICK2_DOWN && i != XK_STICK2_LEFT && i != XK_STICK2_RIGHT
			&& i != XK_STICK1_ANALOG && i != XK_STICK2_ANALOG && i != XK_BUTTON_LTRIGGER && i != XK_BUTTON_RTRIGGER )
			PostXKeyEvent( pXDevice, (GamePadCode)i, sample );
	}
}


//-----------------------------------------------------------------------------
//	Purpose: get the name of the connected controller
//-----------------------------------------------------------------------------
const char *CGamepadController::PchGamePadName( int iDevice )
{
	xdevice_t *pDevice = nullptr;
	if ( iDevice >= 0 && iDevice < m_XDevices.Count() )
		pDevice = &m_XDevices[iDevice];

	if ( pDevice )
	{
		if ( pDevice->m_pGameController )
			return SDL_GameControllerName( pDevice->m_pGameController );
		else if ( pDevice->m_pJoystick )
			return SDL_JoystickName( pDevice->m_pJoystick );
		else if ( pDevice->m_bRemoteInput )
			return "XInput Controller";
		else
			return NULL;
	}
	return NULL;
}


//-----------------------------------------------------------------------------
//	Purpose: reload any SDL bindings and re-init the joystick
//-----------------------------------------------------------------------------
bool CGamepadController::OnActionGamepadMappingsReload()
{
	IUISettings *pSettings = m_pSettings;
	Shutdown();
	Initialize( pSettings );

	return true;
}


//-----------------------------------------------------------------------------
//	Purpose: queue a remote gamepad attached event
//-----------------------------------------------------------------------------
void CGamepadController::RemoteGamepadAttached( int nGamepadID )
{
	RemoteGamepadInput_t Input;
	Input.m_eGamepadInput = k_ERemoteGamepadAttached;
	Input.m_nInstanceID = nGamepadID;
	m_RemoteGamepadInput.PushItem( Input );
}


//-----------------------------------------------------------------------------
//	Purpose: queue a remote gamepad detached event
//-----------------------------------------------------------------------------
void CGamepadController::RemoteGamepadDetached( int nGamepadID )
{
	RemoteGamepadInput_t Input;
	Input.m_eGamepadInput = k_ERemoteGamepadDetached;
	Input.m_nInstanceID = nGamepadID;
	m_RemoteGamepadInput.PushItem( Input );
}


//-----------------------------------------------------------------------------
//	Purpose: queue a remote gamepad axis event
//-----------------------------------------------------------------------------
void CGamepadController::SetRemoteGamepadAxis( int nGamepadID, int nAxis, int nValue )
{
	RemoteGamepadInput_t Input;
	Input.m_eGamepadInput = k_ERemoteGamepadAxisChanged;
	Input.m_nInstanceID = nGamepadID;
	Input.m_nAxis = nAxis;
	Input.m_nAxisValue = static_cast<int16>( nValue );
	m_RemoteGamepadInput.PushItem( Input );
}


//-----------------------------------------------------------------------------
//	Purpose: queue a remote gamepad button event
//-----------------------------------------------------------------------------
void CGamepadController::SetRemoteGamepadButton( int nGamepadID, int nButton, int nValue )
{
	RemoteGamepadInput_t Input;
	Input.m_eGamepadInput = k_ERemoteGamepadButtonChanged;
	Input.m_nInstanceID = nGamepadID;
	Input.m_nButton = nButton;
	Input.m_nButtonValue = static_cast<uint8>( nValue );
	m_RemoteGamepadInput.PushItem( Input );
}


//-----------------------------------------------------------------------------
//	Purpose: we just got focus, reset button state without firing events
//-----------------------------------------------------------------------------
void CGamepadController::GotWindowFocus()
{
#ifdef WIN32
	// We need to manually pump the joystick on win32 as we aren't using the normal SDL frameloop for it, for all
	// our other supported platforms we use the full SDL frame loop so this isn't needed
	if ( !m_bUserDisabled )
	{
		SDL_JoystickUpdate();
	}
#endif

	FOR_EACH_VEC( m_XDevices, i )
	{
		xdevice_t *pXDevice = &m_XDevices[ i ];
		if ( !pXDevice->m_pGameController && !pXDevice->m_pJoystick && !pXDevice->m_bRemoteInput )
			continue;

		// scan all the button and any that are down keep held down
		for ( int sdlbutton = 0; sdlbutton < SDL_CONTROLLER_BUTTON_MAX; sdlbutton++ )
		{
			int iXKey = s_nSDLJoyButtonMapping[ sdlbutton ];
			bool bButtonPressed = GetButtonValue( pXDevice, (SDL_GameControllerButton)sdlbutton ) != 0;
			// ignore the key if it is not down AND we don't have repeats stored for it (if we have repeats we need to KEYUP it)
			if ( !bButtonPressed  )
			{
				pXDevice->m_appXKeys[ iXKey ].m_bSuppressUntilUp = false;
			}
			else
			{
				pXDevice->m_appXKeys[ iXKey ].m_bSuppressUntilUp = true;
			}
		}
	}
}

//-----------------------------------------------------------------------------
//	Purpose: Collate a list of all of our mappings in string form to pass to SDL as a hint
//-----------------------------------------------------------------------------
void CGamepadController::GetSDLGameControllerMappings( CUtlString *pMappingsReturn /* out */ )
{
	FOR_EACH_VEC( m_XDevices, i )
	{
		xdevice_t *pXDevice = &m_XDevices[ i ];
		
		if ( pXDevice->m_pGameController )
			pMappingsReturn->Append( pXDevice->sMappingString );
	}
}


//-----------------------------------------------------------------------------
//	Purpose: return the battery level of this gamepad
//-----------------------------------------------------------------------------
IUIInput::EControllerPowerLevel CGamepadController::GetConnectGamePadPowerLevel( int iGamePad )
{
	if ( m_XDevices.IsValidIndex(iGamePad) )
	{ 
		xdevice_t *pXDevice = &m_XDevices[iGamePad];
		SDL_JoystickPowerLevel ePowerLevel = SDL_JOYSTICK_POWER_UNKNOWN;
		if ( pXDevice->m_pGameController )
			ePowerLevel = SDL_JoystickCurrentPowerLevel( SDL_GameControllerGetJoystick( pXDevice->m_pGameController ) );
		else if ( pXDevice->m_pJoystick )
			ePowerLevel = SDL_JoystickCurrentPowerLevel( pXDevice->m_pJoystick );

		IUIInput::EControllerPowerLevel eReturnLevel = IUIInput::ePowerLevel_Unknown;
		switch ( ePowerLevel )
		{
		default:
		case SDL_JOYSTICK_POWER_UNKNOWN:
			eReturnLevel = IUIInput::ePowerLevel_Unknown;
			break;
		case SDL_JOYSTICK_POWER_EMPTY:
			eReturnLevel = IUIInput::ePowerLevel_Empty;
			break;
		case SDL_JOYSTICK_POWER_LOW:
			eReturnLevel = IUIInput::ePowerLevel_Low;
			break;
		case SDL_JOYSTICK_POWER_MEDIUM:
			eReturnLevel = IUIInput::ePowerLevel_Medium;
			break;
		case SDL_JOYSTICK_POWER_FULL:
			eReturnLevel = IUIInput::ePowerLevel_Full;
			break;
		case SDL_JOYSTICK_POWER_WIRED:
			eReturnLevel = IUIInput::ePowerLevel_Wired;
			break;
		}
		return eReturnLevel;
	}
	return IUIInput::ePowerLevel_Unknown;
}


//-----------------------------------------------------------------------------
//	Purpose: rumble this pad if supported
//-----------------------------------------------------------------------------
void CGamepadController::PulseGamePadHaptics( int iGamePad, float flStrength, int uEffectTimeMS )
{
	if ( uEffectTimeMS == 0 )
		return;

	if ( m_XDevices.IsValidIndex( iGamePad ) )
	{
		xdevice_t *pXDevice = &m_XDevices[iGamePad];
		if ( pXDevice->m_pHaptic )
		{
			SDL_HapticRumbleStop( pXDevice->m_pHaptic );
			SDL_HapticRumblePlay( pXDevice->m_pHaptic, flStrength, uEffectTimeMS );
		}
	}
}


#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: validate mem
//-----------------------------------------------------------------------------
void CGamepadController::Validate( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE();
	ValidateObj( m_XDevices );
	FOR_EACH_VEC( m_XDevices, i )
	{
		ValidateObj( m_XDevices[i].sMappingString );
	}
	m_RemoteGamepadInput.ValidateDataStructureOnly( validator, "m_RemoteGamepadInput" );
}
#endif

