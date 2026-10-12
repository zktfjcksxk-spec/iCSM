//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx.h"
#include "steamcontroller.h"
#include "panorama/panoramacurves.h"
#include "panorama/input/gamepadcodes.h" 
#if !defined( SOURCE2_PANORAMA_FIXME ) && !defined ( PANORAMA_PUBLIC_STEAM_SDK )
#include "steam/client_api.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

#define STEAMPAD_MAX_ANALOGSAMPLE_TRIGGER	32768
#define STEAMPAD_MAX_ANALOGSAMPLE_HORIZONTAL	32766		// intentionally undersized -- actual production range goes from -32768 to +32766; we clamp before escaping this level
#define STEAMPAD_MAX_ANALOGSAMPLE_VERTICAL		32766

#define STEAMPAD_ANALOG_SCALE_HORIZONTAL(x) 	( ( float )STEAMPAD_MAX_ANALOGSAMPLE_HORIZONTAL/( float )( STEAMPAD_MAX_ANALOGSAMPLE_HORIZONTAL-(x) ) )
#define STEAMPAD_ANALOG_SCALE_VERTICAL(x) 	( ( float )STEAMPAD_MAX_ANALOGSAMPLE_VERTICAL/( float )( STEAMPAD_MAX_ANALOGSAMPLE_VERTICAL-(x) ) )

#define STEAMPAD_ANALOG_TRIGGER_THRESHOLD	( ( int )(STEAMPAD_MAX_ANALOGSAMPLE_TRIGGER * 0.45f) )
#define STEAMPAD_ANALOG_PAD_THRESHOLD		( ( int )(STEAMPAD_MAX_ANALOGSAMPLE_HORIZONTAL * 0.2f) )
#define STEAMPAD_ANALOG_STICK_HORIZ_THRESHOLD		( ( int )(STEAMPAD_MAX_ANALOGSAMPLE_HORIZONTAL * 0.5f) )
#define STEAMPAD_ANALOG_STICK_VERT_THRESHOLD		( ( int )(STEAMPAD_MAX_ANALOGSAMPLE_HORIZONTAL * 0.25f) )

ConVar g_ConVarPanoramaSteampadButtonRepeatIntervalStart( "@panorama_steampad_button_repeat_interval_start", ".7" );
ConVar g_ConVarPanoramaSteampadButtonRepeatIntervalEnd( "@panorama_steampad_button_repeat_interval_end", "0.035" );
ConVar g_ConVarPanoramaSteampadButtonRepeatCurveTime( "@panorama_steampad_button_repeat_curve_time", ".5" );

extern int NormalizeStickValue_Cross( int nValue, int nThreshold, float flScale );

#define PAD_ANALOG_BUTTON_THRESHOLD	(STEAMPAD_MAX_ANALOGSAMPLE_TRIGGER * 0.285f)
#define PAD_ANALOG_BUTTON_THRESHOLD_STRONG	(STEAMPAD_MAX_ANALOGSAMPLE_TRIGGER * 0.68f)

ConVar debug_dead_pad( "@panoram_debug_dead_pad", "0", FCVAR_NONE, "" );

ConVar steamcontroller_flow_sensitivity( "steamcontroller_flow_sensitivity", "0.75" );
ConVar steamcontroller_flow_interval( "steamcontroller_flow_interval", "7000" );

ConVar steamcontroller_haptic_intensity( "steamcontroller_haptic_intensity", "320" );

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
static float ScaleControllerAnalogSample( float in, float fRangeScale, float fMaxAnalogSample )
{
	const float f = in / (float)fMaxAnalogSample;

	return clamp( f, -1.0f, 1.0f ) * fRangeScale;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CSteamGameController::CSteamGameController( IUIInput *pInputParent )
{
	m_iIndexForLastPress = -1;
	m_flLastUserIDAssignment = 0.0f;
	m_pInputParent = pInputParent;
	m_bHadGamepadInput = false;
	m_bGamepadConnectedThisSession = false;
	m_bGamepadUsedThisSession = false;
	
	m_unNumConnected = 0;

	//Swipe
	m_vSteamControllerLeftPadPos = vec3_origin;
	m_vSteamControllerLeftPadPosPrev = vec3_origin;
	m_vSteamControllerMomemtum = vec3_origin;
	m_vSteamControllerCurrentPos = vec3_origin;
	m_flSteamControllerPrevTime = 0.0f;
	m_flSteamControllerLastBoundaryTime = 0.0f;
	
	m_iExclusiveControllerIndex = -1;

#if !defined( SOURCE2_PANORAMA_FIXME ) && !defined( PANORAMA_PUBLIC_STEAM_SDK )
	ClientControllerLocal()->Init( false );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CSteamGameController::~CSteamGameController()
{
	Shutdown();
}


//-----------------------------------------------------------------------------
// Purpose: Counts the number of active gamepads connected
//-----------------------------------------------------------------------------
uint32 CSteamGameController::GetNumGamepadsConnected() const
{
	return m_unNumConnected;
}


//-----------------------------------------------------------------------------
//	Purpose: shutdown joystick support
//-----------------------------------------------------------------------------
void CSteamGameController::Shutdown()
{
#if !defined( SOURCE2_PANORAMA_FIXME ) && !defined( PANORAMA_PUBLIC_STEAM_SDK )
	ClientControllerLocal()->Shutdown();
#endif
}


//-----------------------------------------------------------------------------
//	Purpose: Sample the controller state
//-----------------------------------------------------------------------------
void CSteamGameController::RunFrame( void )
{
	VPROF_BUDGET( "CSteamGameController::RunFrame", VPROF_BUDGETGROUP_TENFOOT );
	uint32 unNumConnected = 0;

#if !defined( SOURCE2_PANORAMA_FIXME ) && !defined( PANORAMA_PUBLIC_STEAM_SDK )
	SteamControllerStateInternal_t state;
	for( int i=0; i < MAX_STEAM_CONTROLLERS; ++i )
	{
		if ( ClientControllerLocal()->GetControllerState( i, &state ) )
		{
			m_bGamepadConnectedThisSession = true;
			++unNumConnected;

			ReadSteamController( i, state );
		}
	}
#endif

	m_unNumConnected = unNumConnected;
}

static const int s_nSteamPadDeadZoneTable[] =
{
	STEAMPAD_ANALOG_PAD_THRESHOLD,	// LEFTPAD_AXIS_X
	STEAMPAD_ANALOG_PAD_THRESHOLD,	// LEFTPAD_AXIS_Y
	STEAMPAD_ANALOG_PAD_THRESHOLD,	// RIGHTPAD_AXIS_X
	STEAMPAD_ANALOG_PAD_THRESHOLD,	// RIGHTPAD_AXIS_Y
	STEAMPAD_ANALOG_STICK_HORIZ_THRESHOLD,	// LEFTSTICK_AXIS_X
	STEAMPAD_ANALOG_STICK_VERT_THRESHOLD,	// LEFTSTICK_AXIS_Y
	STEAMPAD_ANALOG_TRIGGER_THRESHOLD,  //LEFT_TRIGGER_AXIS
	STEAMPAD_ANALOG_TRIGGER_THRESHOLD,  //RIGHT_TRIGGER_AXIS
};

const int CSteamGameController::GetSteamPadDeadZone( EStickAxis axis )
{
  int nDeadzone = s_nSteamPadDeadZoneTable[ axis ];

  // Do per mode fixup here
  if ( axis == LEFTPAD_AXIS_Y )
	  nDeadzone *= 2.0f;

  return nDeadzone;
}

//-----------------------------------------------------------------------------
// Purpose: Processes data for controller
//-----------------------------------------------------------------------------
#if !defined( SOURCE2_PANORAMA_FIXME ) && !defined( PANORAMA_PUBLIC_STEAM_SDK )
void CSteamGameController::ReadSteamController( int iIndex, SteamControllerStateInternal_t &state )
{
	VPROF_BUDGET( "ReadSteamController", VPROF_BUDGETGROUP_TENFOOT );
	
	if ( m_iExclusiveControllerIndex != -1 && m_iExclusiveControllerIndex != iIndex)
		return;

	int nX1 = 0, nY1 = 0, nX2 = 0, nY2 = 0;
	
	const double flDoubleTapDistance = 7500.0f;
	const double flDoubleTapTime = 0.4f;

	double flNow = UIEngine()->GetCurrentFrameTime();

	bool bLeftJustWentDown = false;
	if ( state.ulButtons & STEAM_LEFTPAD_FINGERDOWN_MASK )
	{
		if ( m_Device[iIndex].m_flLeftFingerDownTime  < 0.000000001f )
		{
			bLeftJustWentDown = true;
			m_Device[iIndex].m_flLeftFingerDownTime = flNow;

			PostKeyEvent( iIndex, STEAM_BUTTON_LPAD_TOUCH, 32768 );
		}

		if ( bLeftJustWentDown )
		{
			//PulseHapticOnActiveController( k_ESteamControllerPad_Left, 250 );
			Vector2D pos( state.sLeftPadX, state.sLeftPadY );
			float flDistance = pos.DistTo( m_Device[iIndex].m_vecLeftDoubleTapStartPos );

			if ( m_Device[iIndex].m_flLeftFingerDoubleTapStart < 0.0000001f || flNow - m_Device[iIndex].m_flLeftFingerDoubleTapStart > flDoubleTapTime 
				|| ( m_Device[iIndex].m_unLeftTapCount > 0 &&  flDistance > flDoubleTapDistance )
				)
			{
				m_Device[iIndex].m_flLeftFingerDoubleTapStart = flNow;
				m_Device[iIndex].m_vecLeftDoubleTapStartPos.x = state.sLeftPadX;
				m_Device[iIndex].m_vecLeftDoubleTapStartPos.y = state.sLeftPadY;
				m_Device[iIndex].m_unLeftTapCount = 1;
			}
			else 
			{
				m_Device[iIndex].m_flLeftFingerDoubleTapStart = flNow;
				++m_Device[iIndex].m_unLeftTapCount;
				if ( m_Device[iIndex].m_unLeftTapCount == 2 )
				{
					PostKeyEvent( iIndex, STEAM_BUTTON_LPAD_DBLTAPPED, 32768 );
					PostKeyEvent( iIndex, STEAM_BUTTON_LPAD_DBLTAPPED, 0 );
				}
			}
		}
		else
		{
			if ( flNow - m_Device[iIndex].m_flLeftFingerDownTime < flDoubleTapTime )
				m_Device[iIndex].m_flLeftFingerDoubleTapStart = flNow;

			Vector2D pos( state.sLeftPadX, state.sLeftPadY );
			if ( pos.DistTo( m_Device[iIndex].m_vecLeftDoubleTapStartPos ) > flDoubleTapDistance )
			{
				m_Device[iIndex].m_flLeftFingerDoubleTapStart = 0.0f;
				m_Device[iIndex].m_unLeftTapCount = 0;
			}
		}
	}
	else
	{
		m_Device[iIndex].m_flLeftFingerDownTime = 0.0f;

		if ( flNow - m_Device[iIndex].m_flLeftFingerDoubleTapStart > flDoubleTapTime )
		{
			m_Device[iIndex].m_flLeftFingerDoubleTapStart = 0.0f;
			m_Device[iIndex].m_unLeftTapCount = 0;
		}
		
		PostKeyEvent( iIndex, STEAM_BUTTON_LPAD_TOUCH, 0 );
	}

	

	bool bRightJustWentDown = false;
	if ( state.ulButtons & STEAM_RIGHTPAD_FINGERDOWN_MASK )
	{
		if ( m_Device[iIndex].m_flRightFingerDownTime  < 0.000000001f )
		{
			bRightJustWentDown = true;
			m_Device[iIndex].m_flRightFingerDownTime = flNow;
			
			PostKeyEvent( iIndex, STEAM_BUTTON_RPAD_TOUCH, 32768 );
		}

		if ( bRightJustWentDown )
		{
			//PulseHapticOnActiveController( k_ESteamControllerPad_Right, 250 );
			Vector2D pos( state.sRightPadX, state.sRightPadY );
			float flDistance = pos.DistTo( m_Device[iIndex].m_vecRightDoubleTapStartPos );
			if ( m_Device[iIndex].m_flRightFingerDoubleTapStart < 0.0000001f || flNow - m_Device[iIndex].m_flRightFingerDoubleTapStart > flDoubleTapTime 
				|| ( m_Device[iIndex].m_unRightTapCount > 0 &&  flDistance > flDoubleTapDistance )
				)
			{
				m_Device[iIndex].m_flRightFingerDoubleTapStart = flNow;
				m_Device[iIndex].m_vecRightDoubleTapStartPos.x = state.sRightPadX;
				m_Device[iIndex].m_vecRightDoubleTapStartPos.y = state.sRightPadY;
				m_Device[iIndex].m_unRightTapCount = 1;
			}
			else 
			{
				m_Device[iIndex].m_flRightFingerDoubleTapStart = flNow;
				++m_Device[iIndex].m_unRightTapCount;
				if ( m_Device[iIndex].m_unRightTapCount == 2 )
				{
					PostKeyEvent( iIndex, STEAM_BUTTON_RPAD_DBLTAPPED, 32768 );
					PostKeyEvent( iIndex, STEAM_BUTTON_RPAD_DBLTAPPED, 0 );
				}
			}
		}
		else
		{
			if ( flNow - m_Device[iIndex].m_flRightFingerDownTime < flDoubleTapTime )
				m_Device[iIndex].m_flRightFingerDoubleTapStart = flNow;

			Vector2D pos( state.sRightPadX, state.sRightPadY );
			if ( pos.DistTo( m_Device[iIndex].m_vecRightDoubleTapStartPos ) > flDoubleTapDistance )
			{
				m_Device[iIndex].m_flRightFingerDoubleTapStart = 0.0f;
				m_Device[iIndex].m_unRightTapCount = 0;
			}
		}

		++m_Device[iIndex].m_iLastUpdatedRightPadPos;
		if ( m_Device[iIndex].m_iLastUpdatedRightPadPos >= V_ARRAYSIZE( m_Device[iIndex].m_vecRecentRightPadPos ) )
			m_Device[iIndex].m_iLastUpdatedRightPadPos = 0;

		m_Device[iIndex].m_vecRecentRightPadPos[ m_Device[iIndex].m_iLastUpdatedRightPadPos ].m_bIsValid = true;
		m_Device[iIndex].m_vecRecentRightPadPos[ m_Device[iIndex].m_iLastUpdatedRightPadPos ].m_vec.x = state.sRightPadX;
		m_Device[iIndex].m_vecRecentRightPadPos[ m_Device[iIndex].m_iLastUpdatedRightPadPos ].m_vec.y = state.sRightPadY;
	}
	else
	{
		for( int i=1; i < V_ARRAYSIZE( m_Device[iIndex].m_vecRecentRightPadPos ); ++i )
		{
			m_Device[iIndex].m_vecRecentRightPadPos[i].m_bIsValid = false;
		}

		m_Device[iIndex].m_flRightFingerDownTime = 0.0f;

		if ( flNow - m_Device[iIndex].m_flRightFingerDoubleTapStart > flDoubleTapTime )
		{
			m_Device[iIndex].m_flRightFingerDoubleTapStart = 0.0f;
			m_Device[iIndex].m_unRightTapCount = 0;
		}
		
		PostKeyEvent( iIndex, STEAM_BUTTON_RPAD_TOUCH, 0 );
	}

	// Don't do stick events in mode 2.0f   do stick events if we're pressing the button in Mode 1.
	nX1 = NormalizeStickValue_Cross( state.sLeftPadX, GetSteamPadDeadZone( LEFTPAD_AXIS_X ), STEAMPAD_ANALOG_SCALE_HORIZONTAL( GetSteamPadDeadZone( LEFTPAD_AXIS_X ) ) );
	nY1 = NormalizeStickValue_Cross( state.sLeftPadY, GetSteamPadDeadZone( LEFTPAD_AXIS_Y ), STEAMPAD_ANALOG_SCALE_VERTICAL( GetSteamPadDeadZone( LEFTPAD_AXIS_Y ) ) );
	nX2 = NormalizeStickValue_Cross( state.sRightPadX, GetSteamPadDeadZone( RIGHTPAD_AXIS_X ), STEAMPAD_ANALOG_SCALE_HORIZONTAL( GetSteamPadDeadZone( RIGHTPAD_AXIS_X ) ) );
	nY2 = NormalizeStickValue_Cross( state.sRightPadY, GetSteamPadDeadZone( RIGHTPAD_AXIS_Y ), STEAMPAD_ANALOG_SCALE_VERTICAL( GetSteamPadDeadZone( RIGHTPAD_AXIS_Y ) ) );
	
	int stickX = NormalizeStickValue_Cross( state.sLeftStickX, GetSteamPadDeadZone( LEFTSTICK_AXIS_X ), STEAMPAD_ANALOG_SCALE_HORIZONTAL( GetSteamPadDeadZone( LEFTSTICK_AXIS_X ) ) );
	int stickY = NormalizeStickValue_Cross( state.sLeftStickY, GetSteamPadDeadZone( LEFTSTICK_AXIS_Y ), STEAMPAD_ANALOG_SCALE_VERTICAL( GetSteamPadDeadZone( LEFTSTICK_AXIS_Y ) ) );

	// if this is a different controller and we haven't seen input from the others for a while AND the analog sticks are deflected on this, let it take over
	if ( m_iIndexForLastPress != iIndex &&  ( ( UIEngine()->GetCurrentFrameTime() - m_flLastUserIDAssignment ) > k_flAnalogUserInputTimeout || m_iIndexForLastPress == -1 )
		&& ( abs(nX1) > GetSteamPadDeadZone( LEFTPAD_AXIS_X ) || abs(nY1) > GetSteamPadDeadZone( LEFTPAD_AXIS_Y ) || abs(nX2) > GetSteamPadDeadZone( RIGHTPAD_AXIS_X ) || abs(nY2) > GetSteamPadDeadZone( RIGHTPAD_AXIS_Y ) ) )
	{
		m_iIndexForLastPress = iIndex;
		m_flLastUserIDAssignment = UIEngine()->GetCurrentFrameTime(); // stick was deflected, lets assume they are still using this controller
	}

	// only fire analog events for the last controller that pressed a button
	if ( m_iIndexForLastPress == iIndex 
		&& ( m_flLastUserIDAssignment >= m_pInputParent->GetLastGamePadControllerActiveTime() || m_pInputParent->GetLastGamePadControllerActiveTime() < 0.0001f ) )
	{
		// raw analog stick position
		InputMessage_t input;
		input.m_eSource = k_ePanelEventSourceGamepad;
		input.m_eInputType = k_eGamePadAnalog;
		input.m_GamePadData.m_GamePadCode = STEAM_LEFTPAD_ANALOG;
		input.m_GamePadData.m_RepeatCount = m_Device[iIndex].m_appXKeys[STEAM_LEFTPAD_ANALOG].repeats;
		input.m_GamePadData.m_fValueX = ScaleControllerAnalogSample( nX1, 1000.0f, STEAMPAD_MAX_ANALOGSAMPLE_HORIZONTAL );
		input.m_GamePadData.m_fValueY = ScaleControllerAnalogSample( nY1, 1000.0f, STEAMPAD_MAX_ANALOGSAMPLE_VERTICAL );
		input.m_GamePadData.m_fValueXRaw = ScaleControllerAnalogSample( state.sLeftPadX, 1000.0f, STEAMPAD_MAX_ANALOGSAMPLE_HORIZONTAL );
		input.m_GamePadData.m_fValueYRaw = ScaleControllerAnalogSample( state.sLeftPadY, 1000.0f, STEAMPAD_MAX_ANALOGSAMPLE_VERTICAL );
		input.m_GamePadData.m_flFingerDown = m_Device[iIndex].m_flLeftFingerDownTime;
		if ( input.m_GamePadData.m_flFingerDown > 0.0000001f )
		{
			if ( bLeftJustWentDown )
			{
				m_Device[iIndex].m_vecInitialLeftPos.x = input.m_GamePadData.m_fValueXRaw;
				m_Device[iIndex].m_vecInitialLeftPos.y = input.m_GamePadData.m_fValueYRaw;
			}
			input.m_GamePadData.m_fValueXFirst = m_Device[iIndex].m_vecInitialLeftPos.x;
			input.m_GamePadData.m_fValueYFirst = m_Device[iIndex].m_vecInitialLeftPos.y;
		}
		else
		{
			input.m_GamePadData.m_fValueXFirst = 0.0f;
			input.m_GamePadData.m_fValueXFirst = 0.0f;
		}
		m_pInputParent->InputEvent( input );
	
		input.m_eInputType = k_eGamePadAnalog;
		input.m_GamePadData.m_GamePadCode = STEAM_RIGHTPAD_ANALOG;
		input.m_GamePadData.m_RepeatCount = m_Device[iIndex].m_appXKeys[STEAM_RIGHTPAD_ANALOG].repeats;
		input.m_GamePadData.m_fValueX = ScaleControllerAnalogSample( nX2, 1000.0f, STEAMPAD_MAX_ANALOGSAMPLE_HORIZONTAL );
		input.m_GamePadData.m_fValueY = ScaleControllerAnalogSample( nY2, 1000.0f, STEAMPAD_MAX_ANALOGSAMPLE_VERTICAL );
		input.m_GamePadData.m_fValueXRaw = ScaleControllerAnalogSample( state.sRightPadX, 1000.0f, STEAMPAD_MAX_ANALOGSAMPLE_HORIZONTAL );
		input.m_GamePadData.m_fValueYRaw = ScaleControllerAnalogSample( state.sRightPadY, 1000.0f, STEAMPAD_MAX_ANALOGSAMPLE_VERTICAL );
		input.m_GamePadData.m_flFingerDown = m_Device[iIndex].m_flRightFingerDownTime;
		if ( input.m_GamePadData.m_flFingerDown > 0.0000001f )
		{
			if ( bRightJustWentDown )
			{
				m_Device[iIndex].m_vecInitialRightPos.x = input.m_GamePadData.m_fValueXRaw;
				m_Device[iIndex].m_vecInitialRightPos.y = input.m_GamePadData.m_fValueYRaw;
			}
			input.m_GamePadData.m_fValueXFirst = m_Device[iIndex].m_vecInitialRightPos.x;
			input.m_GamePadData.m_fValueYFirst = m_Device[iIndex].m_vecInitialRightPos.y;
		}
		else
		{
			input.m_GamePadData.m_fValueXFirst = 0.0f;
			input.m_GamePadData.m_fValueXFirst = 0.0f;
		}
		m_pInputParent->InputEvent( input );

		input.m_eInputType = k_eGamePadAnalog;
		input.m_GamePadData.m_GamePadCode = STEAM_LEFTSTICK_ANALOG;
		input.m_GamePadData.m_RepeatCount = m_Device[iIndex].m_appXKeys[STEAM_LEFTSTICK_ANALOG].repeats;
		input.m_GamePadData.m_fValueX = ScaleControllerAnalogSample( stickX, 1000.0f, STEAMPAD_MAX_ANALOGSAMPLE_HORIZONTAL );
		input.m_GamePadData.m_fValueY = ScaleControllerAnalogSample( stickY, 1000.0f, STEAMPAD_MAX_ANALOGSAMPLE_VERTICAL );
		input.m_GamePadData.m_fValueXRaw = ScaleControllerAnalogSample( state.sLeftStickX, 1000.0f, STEAMPAD_MAX_ANALOGSAMPLE_HORIZONTAL );
		input.m_GamePadData.m_fValueYRaw = ScaleControllerAnalogSample( state.sLeftStickY, 1000.0f, STEAMPAD_MAX_ANALOGSAMPLE_VERTICAL );
		m_pInputParent->InputEvent( input );
		
		input.m_eInputType = k_eGamePadAnalog;
		input.m_GamePadData.m_GamePadCode = STEAM_LTRIGGER_ANALOG;
		input.m_GamePadData.m_RepeatCount = m_Device[iIndex].m_appXKeys[STEAM_LTRIGGER_ANALOG].repeats;
		input.m_GamePadData.m_fValueX = ScaleControllerAnalogSample( state.sTriggerL, 1000.0f, STEAMPAD_MAX_ANALOGSAMPLE_TRIGGER );
		input.m_GamePadData.m_fValueY = 0.0f;
		input.m_GamePadData.m_fValueXRaw = 0.0f;
		input.m_GamePadData.m_fValueYRaw = 0.0f;
		m_pInputParent->InputEvent( input );
		
		input.m_eInputType = k_eGamePadAnalog;
		input.m_GamePadData.m_GamePadCode = STEAM_RTRIGGER_ANALOG;
		input.m_GamePadData.m_RepeatCount = m_Device[iIndex].m_appXKeys[STEAM_RTRIGGER_ANALOG].repeats;
		input.m_GamePadData.m_fValueX = ScaleControllerAnalogSample( state.sTriggerR, 1000.0f, STEAMPAD_MAX_ANALOGSAMPLE_TRIGGER );
		input.m_GamePadData.m_fValueY = 0.0f;
		input.m_GamePadData.m_fValueXRaw = 0.0f;
		input.m_GamePadData.m_fValueYRaw = 0.0f;
		m_pInputParent->InputEvent( input );
	}

	// stick axis "pressed" events, for the pad we try to not emit diagonal (both right and up for instance)
	// unless you've strongly pressed in both axis, if one axis is very strong and the other relatively weak then
	// we'll just emit the dominate one.  This helps avoid false up/down when trying to do the more typical left/right 
	// in our UI
	int x = abs( nX1 );
	int y = abs( nY1 );
	if ( x > PAD_ANALOG_BUTTON_THRESHOLD && y > PAD_ANALOG_BUTTON_THRESHOLD )
	{
		if ( x > y && y < PAD_ANALOG_BUTTON_THRESHOLD_STRONG )
			nY1 = 0;
		else if ( y > x && x < PAD_ANALOG_BUTTON_THRESHOLD_STRONG )
			nX1 = 0;
	}
	x = abs( nX2 );
	y = abs( nY2 );
	if ( x > PAD_ANALOG_BUTTON_THRESHOLD && y > PAD_ANALOG_BUTTON_THRESHOLD )
	{
		if ( x > y && y < PAD_ANALOG_BUTTON_THRESHOLD_STRONG )
			nY2 = 0;
		else if ( y > x && x < PAD_ANALOG_BUTTON_THRESHOLD_STRONG )
			nX2 = 0;
	}
	HandleDeviceAxis( iIndex, nX1, STEAM_LEFTPAD_LEFT, STEAM_LEFTPAD_RIGHT, LEFTPAD_AXIS_X );
	HandleDeviceAxis( iIndex, nY1, STEAM_LEFTPAD_DOWN, STEAM_LEFTPAD_UP, LEFTPAD_AXIS_Y );
	HandleDeviceAxis( iIndex, nX2, STEAM_RIGHTPAD_LEFT, STEAM_RIGHTPAD_RIGHT, RIGHTPAD_AXIS_X );
	HandleDeviceAxis( iIndex, nY2, STEAM_RIGHTPAD_DOWN, STEAM_RIGHTPAD_UP, RIGHTPAD_AXIS_Y );

	HandleDeviceAxis( iIndex, stickX, STEAM_LEFTSTICK_LEFT, STEAM_LEFTSTICK_RIGHT, LEFTSTICK_AXIS_X );
	HandleDeviceAxis( iIndex, stickY, STEAM_LEFTSTICK_DOWN, STEAM_LEFTSTICK_UP, LEFTSTICK_AXIS_Y );
	
	HandleDeviceAxis( iIndex, state.sTriggerL, STEAM_LTRIGGER_ANALOG, STEAM_LTRIGGER_ANALOG, LEFT_TRIGGER_AXIS );
	HandleDeviceAxis( iIndex, state.sTriggerR, STEAM_RTRIGGER_ANALOG, STEAM_RTRIGGER_ANALOG, RIGHT_TRIGGER_AXIS );

	if ( state.ulButtons != 0 )
	{
		m_bHadGamepadInput = true;
		m_bGamepadUsedThisSession = true;
	}

	PostKeyEvent( iIndex, STEAM_BUTTON_RTRIGGER, (state.ulButtons & STEAM_RIGHT_TRIGGER_MASK) ? 32768 : 0 );
	PostKeyEvent( iIndex, STEAM_BUTTON_LSHOULDER, (state.ulButtons & STEAM_LEFT_BUMPER_MASK) ? 32768 : 0 );
	PostKeyEvent( iIndex, STEAM_BUTTON_RSHOULDER, (state.ulButtons & STEAM_RIGHT_BUMPER_MASK) ? 32768 : 0 );
	PostKeyEvent( iIndex, STEAM_BUTTON_LBACK, (state.ulButtons & STEAM_BUTTON_BACK_LEFT_MASK) ? 32768 : 0 );
	PostKeyEvent( iIndex, STEAM_BUTTON_RBACK, (state.ulButtons & STEAM_BUTTON_BACK_RIGHT_MASK) ? 32768 : 0 );
	PostKeyEvent( iIndex, STEAM_BUTTON_SELECT, (state.ulButtons & STEAM_BUTTON_MENU_MASK) ? 32768 : 0 );
	PostKeyEvent( iIndex, STEAM_BUTTON_START, (state.ulButtons & STEAM_BUTTON_ESCAPE_MASK) ? 32768 : 0 );
	PostKeyEvent( iIndex, STEAM_BUTTON_LTRIGGER, (state.ulButtons & STEAM_LEFT_TRIGGER_MASK) ? 32768 : 0 );
	PostKeyEvent( iIndex, STEAM_BUTTON_GUIDE, (state.ulButtons & STEAM_BUTTON_STEAM_MASK) ? 32768 : 0 );
	PostKeyEvent( iIndex, STEAM_BUTTON_LPAD_CLICKED, (state.ulButtons & STEAM_BUTTON_LEFTPAD_CLICKED_MASK) ? 32768 : 0 );
	PostKeyEvent( iIndex, STEAM_BUTTON_LEFTSTICK_CLICKED, (state.ulButtons & STEAM_JOYSTICK_BUTTON_MASK) ? 32768 : 0 );
	PostKeyEvent( iIndex, STEAM_BUTTON_X, (state.ulButtons & STEAM_BUTTON_2_MASK) ? 32768 : 0 );
	PostKeyEvent( iIndex, STEAM_BUTTON_A, (state.ulButtons & STEAM_BUTTON_3_MASK) ? 32768 : 0 );
	PostKeyEvent( iIndex, STEAM_BUTTON_Y, (state.ulButtons & STEAM_BUTTON_0_MASK) ? 32768 : 0 );
	PostKeyEvent( iIndex, STEAM_BUTTON_B, (state.ulButtons & STEAM_BUTTON_1_MASK) ? 32768 : 0 );
	PostKeyEvent( iIndex, STEAM_BUTTON_RPAD_CLICKED, (state.ulButtons & STEAM_BUTTON_RIGHTPAD_CLICKED_MASK) ? 32768 : 0 );
	PostKeyEvent( iIndex, STEAM_BUTTON_DPAD_UP, (state.ulButtons & STEAM_TOUCH_0_MASK) ? 32768 : 0 );
	PostKeyEvent( iIndex, STEAM_BUTTON_DPAD_RIGHT, (state.ulButtons & STEAM_TOUCH_1_MASK) ? 32768 : 0 );
	PostKeyEvent( iIndex, STEAM_BUTTON_DPAD_LEFT, (state.ulButtons & STEAM_TOUCH_2_MASK) ? 32768 : 0 );
	PostKeyEvent( iIndex, STEAM_BUTTON_DPAD_DOWN, (state.ulButtons & STEAM_TOUCH_3_MASK) ? 32768 : 0 );
}
#endif

//-----------------------------------------------------------------------------
//	Purpose: turn analog axis movement into keypresses
//-----------------------------------------------------------------------------
void CSteamGameController::HandleDeviceAxis( int iIndex, int nAxisValue, GamePadCode negativeKey, GamePadCode positiveKey, EStickAxis axisID )
{
	GamePadCode key = XK_NULL;
	int nThreshold = GetSteamPadDeadZone( axisID );

	// Simple hysteresis to prevent noise related rapid on/off events
	int nHysteresisAmount = 500;
	if ( m_Device[iIndex].m_appXKeys[nAxisValue<0?negativeKey:positiveKey].flFirstInput != 0.0f )
	{
		nThreshold -= nHysteresisAmount;
	}
	else
	{
		nThreshold += nHysteresisAmount;
	}
	clamp( nThreshold, 0, SHRT_MAX );


	if ( abs(nAxisValue) > nThreshold )
	{
		// Queue stick axis push response, don't track right pad since it's emulating mouse most the time
		if ( axisID != RIGHTPAD_AXIS_X && axisID != RIGHTPAD_AXIS_Y )
			m_bHadGamepadInput = true;

		m_bGamepadUsedThisSession = true;
		if ( nAxisValue < 0 )
		{
			key = negativeKey;
			PostKeyEvent( iIndex, negativeKey, -nAxisValue );
		}
		else if ( nAxisValue > 0 )
		{
			key = positiveKey;
			PostKeyEvent( iIndex, positiveKey, nAxisValue );
		}
	}

	if ( m_Device[iIndex].lastStickKeys[axisID] != XK_NULL && m_Device[iIndex].lastStickKeys[axisID] != key )
	{
		// Queue stick axis release response
		PostKeyEvent( iIndex, m_Device[iIndex].lastStickKeys[axisID], 0 );
	}
	m_Device[iIndex].lastStickKeys[axisID] = key;
}


//-----------------------------------------------------------------------------
//	Purpose: Pulse haptic feedback
//-----------------------------------------------------------------------------
void CSteamGameController::PulseHapticOnActiveController( ESteamControllerPad ePad, unsigned short durationMicroSec )
{
	if ( m_iIndexForLastPress != -1 )
	{
#if !defined( SOURCE2_PANORAMA_FIXME ) && !defined( PANORAMA_PUBLIC_STEAM_SDK )
		ClientControllerLocal()->TriggerHapticPulse( m_iIndexForLastPress, ePad, durationMicroSec, 0, 1, 0 );
#endif
	}
}


//-----------------------------------------------------------------------------
// Purpose: Turn off the controller (typically just pressed "A" to choose to do that)
//-----------------------------------------------------------------------------
void panorama::CSteamGameController::TurnOffActiveController()
{
	if ( m_iIndexForLastPress != -1 )
	{
#if !defined( SOURCE2_PANORAMA_FIXME ) && !defined( PANORAMA_PUBLIC_STEAM_SDK )
		ClientController()->TurnOffController( m_iIndexForLastPress );
#endif
	}
}


//-----------------------------------------------------------------------------
//	Purpose: Post events, ignoring key repeats
//-----------------------------------------------------------------------------
void CSteamGameController::PostKeyEvent( int iIndex, GamePadCode xKey, int nSample )
{
	int nSampleThreshold = 1; 
	float	value	= 0.f;


	bool bAnalogMove = false;

	// Look for changes on the analog axes
	switch( xKey )
	{
	case STEAM_LEFTPAD_LEFT:
	case STEAM_LEFTPAD_RIGHT:
		{
			bAnalogMove = true;
			value = ( xKey == STEAM_LEFTPAD_LEFT ) ? -nSample : nSample;
			nSampleThreshold = ( int )( GetSteamPadDeadZone( LEFTPAD_AXIS_X ) );
		}
		break;

	case STEAM_LEFTPAD_UP:
	case STEAM_LEFTPAD_DOWN:
		{
			bAnalogMove = true;
			value = ( xKey == STEAM_LEFTPAD_UP ) ? -nSample : nSample;
			nSampleThreshold = ( int )( GetSteamPadDeadZone( LEFTPAD_AXIS_Y ) );
		}
		break;

	case STEAM_RIGHTPAD_LEFT:
	case STEAM_RIGHTPAD_RIGHT:
		{
			bAnalogMove = true;
			value = ( xKey == STEAM_RIGHTPAD_LEFT ) ? -nSample : nSample;
			nSampleThreshold = ( int )( GetSteamPadDeadZone( RIGHTPAD_AXIS_X ) );
		}
		break;

	case STEAM_RIGHTPAD_UP:
	case STEAM_RIGHTPAD_DOWN:
		{
			bAnalogMove = true;
			value = ( xKey == STEAM_RIGHTPAD_UP ) ? -nSample : nSample;
			nSampleThreshold = ( int )(  GetSteamPadDeadZone( RIGHTPAD_AXIS_Y ) );
		}
		break;

	default:
		break;
	}

	NOTE_UNUSED( value );

	if ( nSample < nSampleThreshold )
	{
		// button is released, clear the suppress until up
		m_Device[iIndex].m_appXKeys[xKey].m_bSuppressUntilUp = false;
	}
	else
	{
		if ( m_Device[iIndex].m_appXKeys[xKey].m_bSuppressUntilUp )
		{
			return;
		}

	}
	bool bKeyUpThisFrame = false; // keep track of if we released this frame

	int nRepeats = m_Device[iIndex].m_appXKeys[xKey].repeats;
	if ( nSample > nSampleThreshold && m_Device[iIndex].m_appXKeys[xKey].flLastInput < UIEngine()->GetCurrentFrameTime() )
	{
		if (m_Device[iIndex].m_appXKeys[xKey].repeats == 0 )
			m_Device[iIndex].m_appXKeys[xKey].flFirstInput = UIEngine()->GetCurrentFrameTime(); // first down since release, start tracking when this started
		m_Device[iIndex].m_appXKeys[xKey].repeats++;

		float flDelayTime = SimpleSplineRemapValClamped( ( UIEngine( )->GetCurrentFrameTime( ) - m_Device[iIndex].m_appXKeys[xKey].flFirstInput ) / g_ConVarPanoramaSteampadButtonRepeatCurveTime.GetFloat(), 0.0f, 1.0f, 
														 g_ConVarPanoramaSteampadButtonRepeatIntervalStart.GetFloat(), g_ConVarPanoramaSteampadButtonRepeatIntervalEnd.GetFloat() );

		m_Device[iIndex].m_appXKeys[xKey].flLastInput = UIEngine()->GetCurrentFrameTime() + flDelayTime;  // fire the next down at this time

		if ( xKey == STEAM_LEFTPAD_UP && debug_dead_pad.GetBool() )
			Msg( "left pad up triggered\n" );
	}
	else if ( nSample < nSampleThreshold )
	{
		if (m_Device[iIndex].m_appXKeys[xKey].repeats )
			bKeyUpThisFrame = true; // had down events, now up
		m_Device[iIndex].m_appXKeys[xKey].repeats = 0;
		nRepeats = 0;
		m_Device[iIndex].m_appXKeys[xKey].flFirstInput = 0.0f;
		// Don't reset flLastInput unless we're truly getting no input
		// because we might just be in the deadzone.
		// Prevents multiple fires of a key if we're right on the border and noise is
		// bouncing us back and forth
		if ( nSample == 0 )
		{
			m_Device[iIndex].m_appXKeys[xKey].flLastInput = 0.0f;
		}

		if ( xKey == STEAM_LEFTPAD_UP && debug_dead_pad.GetBool() )
			Msg( "left pad no longer up\n" );
	}
	else
	{
		if ( xKey == STEAM_LEFTPAD_UP && debug_dead_pad.GetBool() )
			Msg( "Not time for lefpad up repeat\n" );
		return; // not time for a key repeat yet
	}

	// store the key
	m_Device[iIndex].m_appXKeys[xKey].sample = nSample;

	if ( m_iIndexForLastPress == -1 )
	{
		m_iIndexForLastPress = iIndex;
		m_flLastUserIDAssignment = UIEngine()->GetCurrentFrameTime();
	}

	if ( bAnalogMove && m_iIndexForLastPress != iIndex )
		return; // don't fire the analog move event as this isn't the primary controller

	// filter out key repeats for anything but the analog sticks and dpad, 
	// BUGBUG: undo this when we work out how/if the higher level wants these repeats
	if ( nSample > 0 && nRepeats >= 1 && 
		( xKey == STEAM_BUTTON_LTRIGGER ||
		xKey == STEAM_BUTTON_RTRIGGER ||
		xKey == STEAM_BUTTON_LSHOULDER ||
		xKey == STEAM_BUTTON_RSHOULDER ||
		xKey == STEAM_BUTTON_LBACK ||
		xKey == STEAM_BUTTON_RBACK ||
		xKey == STEAM_BUTTON_GUIDE || 
		xKey == STEAM_BUTTON_SELECT ||
		xKey == STEAM_BUTTON_START ||
		xKey == STEAM_BUTTON_LPAD_CLICKED ||
		xKey == STEAM_BUTTON_RPAD_CLICKED ||
		xKey == STEAM_BUTTON_A ||
		xKey == STEAM_BUTTON_B ||
		xKey == STEAM_BUTTON_X ||
		xKey == STEAM_BUTTON_Y 
		) )
	{
		return; // ignore key repeats for these buttons
	}

	if ( nSample == 0 && !bKeyUpThisFrame )
		return; // only fire 1 key up for a button

	if ( nSample != 0 )
	{
		m_iIndexForLastPress = iIndex;
		m_flLastUserIDAssignment = UIEngine()->GetCurrentFrameTime();
	}


	InputMessage_t input;
	input.m_eSource = k_ePanelEventSourceGamepad;

	input.m_eInputType = nSample? k_eGamePadDown : k_eGamePadUp;
	input.m_GamePadData.m_GamePadCode = xKey;
	input.m_GamePadData.m_RepeatCount = nRepeats;
	
	if ( bAnalogMove )
	{
		input.m_GamePadData.m_fValue = ScaleControllerAnalogSample( nSample, 1000.0f, STEAMPAD_MAX_ANALOGSAMPLE_HORIZONTAL );
	}

	if ( xKey == STEAM_LEFTPAD_UP && debug_dead_pad.GetBool() )
		Msg( "Leftpad up fired %d repeats\n", nRepeats );

	m_pInputParent->InputEvent( input );
}


//-----------------------------------------------------------------------------
//	Purpose: we just got focus, reset button state without firing events
//-----------------------------------------------------------------------------
void CSteamGameController::GotWindowFocus()
{
	VPROF_BUDGET( "CSteamGameController::GotWindowFocus", VPROF_BUDGETGROUP_TENFOOT );
#if !defined( SOURCE2_PANORAMA_FIXME ) && !defined( PANORAMA_PUBLIC_STEAM_SDK )

	for ( int i = 0; i < MAX_STEAM_CONTROLLERS; ++i )
	{
		SteamControllerStateInternal_t state;
		if ( ClientControllerLocal()->GetControllerState( i, &state ) )
		{
			xdevice_t *pXDevice = &m_Device[i];

#define CHECK_KEY( button, mask ) { if ( state.ulButtons & mask) { \
			pXDevice->m_appXKeys[button].m_bSuppressUntilUp = true; \
		} else { \
			pXDevice->m_appXKeys[button].m_bSuppressUntilUp = false; }  }

			CHECK_KEY( STEAM_BUTTON_RTRIGGER, STEAM_RIGHT_TRIGGER_MASK );
			CHECK_KEY( STEAM_BUTTON_LSHOULDER, STEAM_LEFT_BUMPER_MASK );
			CHECK_KEY( STEAM_BUTTON_RSHOULDER, STEAM_RIGHT_BUMPER_MASK );
			CHECK_KEY( STEAM_BUTTON_LBACK, STEAM_BUTTON_BACK_LEFT_MASK );
			CHECK_KEY( STEAM_BUTTON_RBACK, STEAM_BUTTON_BACK_RIGHT_MASK );
			CHECK_KEY( STEAM_BUTTON_SELECT, STEAM_BUTTON_MENU_MASK );
			CHECK_KEY( STEAM_BUTTON_START, STEAM_BUTTON_ESCAPE_MASK );
			CHECK_KEY( STEAM_BUTTON_LTRIGGER, STEAM_LEFT_TRIGGER_MASK );
			CHECK_KEY( STEAM_BUTTON_GUIDE, STEAM_BUTTON_STEAM_MASK );
			CHECK_KEY( STEAM_BUTTON_LPAD_CLICKED, STEAM_BUTTON_LEFTPAD_CLICKED_MASK );
			CHECK_KEY( STEAM_BUTTON_LEFTSTICK_CLICKED, STEAM_JOYSTICK_BUTTON_MASK );
			CHECK_KEY( STEAM_BUTTON_X, STEAM_BUTTON_2_MASK );
			CHECK_KEY( STEAM_BUTTON_A, STEAM_BUTTON_3_MASK );
			CHECK_KEY( STEAM_BUTTON_Y, STEAM_BUTTON_0_MASK );
			CHECK_KEY( STEAM_BUTTON_B, STEAM_BUTTON_1_MASK );
			CHECK_KEY( STEAM_BUTTON_RPAD_CLICKED, STEAM_BUTTON_RIGHTPAD_CLICKED_MASK );
			CHECK_KEY( STEAM_BUTTON_DPAD_UP, STEAM_TOUCH_0_MASK );
			CHECK_KEY( STEAM_BUTTON_DPAD_RIGHT, STEAM_TOUCH_1_MASK );
			CHECK_KEY( STEAM_BUTTON_DPAD_LEFT, STEAM_TOUCH_2_MASK );
			CHECK_KEY( STEAM_BUTTON_DPAD_DOWN, STEAM_TOUCH_3_MASK );
		}
	}
#endif
}


#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: validate mem
//-----------------------------------------------------------------------------
void CSteamGameController::Validate( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE();
}
#endif

