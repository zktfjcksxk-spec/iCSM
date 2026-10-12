//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx.h"
#include "steamcontroller_new.h"
#include "panorama/input/gamepadcodes.h" 
#if !defined( SOURCE2_PANORAMA_FIXME ) && !defined ( PANORAMA_PUBLIC_STEAM_SDK )
#include "steam/client_api.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

// For now, only build this in vrpanorama
#if defined(PANORAMA_PUBLIC_STEAM_SDK)

extern ConVar g_ConVarPanoramaSteampadButtonRepeatIntervalStart;
extern ConVar g_ConVarPanoramaSteampadButtonRepeatIntervalEnd;
extern ConVar g_ConVarPanoramaSteampadButtonRepeatCurveTime;


struct buttonToAction_t
{
	GamePadCode xKey;
	const char *pchAction;
	bool bAllowRepeat;
};


const buttonToAction_t k_buttonToAction[] =
{
	// BUttons that don't allow repeat
	{ STEAM_BUTTON_A, "A", false },
	{ STEAM_BUTTON_B, "B", false },
	{ STEAM_BUTTON_X, "X", false },
	{ STEAM_BUTTON_Y, "Y", false },
	{ STEAM_BUTTON_LBACK, "LBACK", false },
	{ STEAM_BUTTON_RBACK, "RBACK", false },
	{ STEAM_BUTTON_SELECT, "SELECT", false },
	{ STEAM_BUTTON_START, "START", false },
	{ STEAM_BUTTON_LSHOULDER, "LSHOULDER", false },
	{ STEAM_BUTTON_RSHOULDER, "RSHOULDER", false },
	{ STEAM_BUTTON_LTRIGGER, "LTRIGGER", false },
	{ STEAM_BUTTON_RTRIGGER, "RTRIGGER", false },
	{ STEAM_BUTTON_LPAD_CLICKED, "LPAD_CLICKED", false },
	{ STEAM_BUTTON_RPAD_CLICKED, "RPAD_CLICKED", false },
	{ STEAM_BUTTON_GUIDE, "GUIDE", false },

	// Buttons that allow repeat
	{ STEAM_BUTTON_DPAD_UP, "UP", true },
	{ STEAM_BUTTON_DPAD_RIGHT, "RIGHT", true },
	{ STEAM_BUTTON_DPAD_DOWN, "DOWN", true },
	{ STEAM_BUTTON_DPAD_LEFT, "LEFT", true },
	{ STEAM_BUTTON_LEFTSTICK_CLICKED, "LEFTSTICK_CLICKED", true },
	{ STEAM_LEFTSTICK_UP, "LEFTSTICK_UP", true },
	{ STEAM_LEFTSTICK_RIGHT, "LEFTSTICK_RIGHT", true },
	{ STEAM_LEFTSTICK_DOWN, "LEFTSTICK_DOWN", true },
	{ STEAM_LEFTSTICK_LEFT, "LEFTSTICK_LEFT", true },
};

struct analogToAction_t
{
	GamePadCode xKey;
	const char *pchAction;
};

const analogToAction_t k_analogToAction[] =
{
	{ STEAM_LEFTSTICK_ANALOG, "LEFTSTICK_ANALOG" },
	{ STEAM_LEFTPAD_ANALOG, "LEFTPAD_ANALOG" },
	{ STEAM_RIGHTPAD_ANALOG, "RIGHTPAD_ANALOG" },
	{ STEAM_LTRIGGER_ANALOG, "LTRIGGER_ANALOG" },
	{ STEAM_RTRIGGER_ANALOG, "RTRIGGER_ANALOG" }
};

extern const char *PchNameFromGamePadCode( int );

#if defined( PANORAMA_PUBLIC_STEAM_SDK )
#define ClientControllerLocal() SteamController()
#endif


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CSteamGameControllerNew::CSteamGameControllerNew( IUIInput *pInputParent )
{
	m_pInputParent = pInputParent;
	m_bHadGamepadInput = false;
	m_bInit = false;

#if !defined( SOURCE2_PANORAMA_FIXME ) && !defined( PANORAMA_PUBLIC_STEAM_SDK )
	ClientControllerLocal()->Init( false );
	m_bInit = true;
#endif

	m_bConfigLoaded = false;

	m_iIndexForLastPress = -1;
	m_flLastUserIDAssignment = 0;

	m_unConnectedControllers = 0;

	m_iExclusiveControllerIndex = -1;


	for ( int i = 0; i < XK_MAX_KEYS; i++ )
	{
		m_buttonState[i].bDown = false;
		m_buttonState[i].nRepeats = 0;
		m_buttonState[i].flFirstInput = 0;
		m_buttonState[i].flLastInput = 0;
		m_buttonState[i].bSuppressUntilUp = false;
	}

	for ( int i = 0; i < MAX_ANALOG; i++ )
	{
		m_analogState[i].nRepeats = 0;
		m_analogState[i].flFirstInput = 0;
		m_analogState[i].flLastInput = 0;
	}

	SetActiveActionSet( "Default" );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CSteamGameControllerNew::~CSteamGameControllerNew()
{
	Shutdown();
}


//-----------------------------------------------------------------------------
// Purpose: Helper function for creating a new action set
//-----------------------------------------------------------------------------
int CSteamGameControllerNew::InsertEmptyActionSet( const char *pchActionSet )
{
	ActionSetData_t action;
	action.m_hActionSet = 0;
	buttonData_t *pButtonData = action.m_buttonData;
	for ( int i  = 0; i < XK_MAX_KEYS; i++ )
	{
		pButtonData[i].iAction = 0;
		pButtonData[i].hAction = 0;
	}

	analogData_t *pAnalogData = action.m_analogData;
	for ( int i = 0; i < MAX_ANALOG; i++ )
	{
		pAnalogData[i].hAction = 0;
		pAnalogData[i].iAction = 0;
	}

	Assert( m_mapNameToActionSetData.Find( pchActionSet ) == m_mapNameToActionSetData.InvalidIndex() );
	return m_mapNameToActionSetData.Insert( pchActionSet, action );
}


//-----------------------------------------------------------------------------
// Purpose: Counts the number of active gamepads connected
//-----------------------------------------------------------------------------
uint32 CSteamGameControllerNew::GetNumGamepadsConnected() const
{
	return m_unConnectedControllers;
}


//-----------------------------------------------------------------------------
//	Purpose: shutdown joystick support
//-----------------------------------------------------------------------------
void CSteamGameControllerNew::Shutdown()
{
	if ( !m_bInit )
	{
		return;
	}
	ClientControllerLocal()->Shutdown();
}


//-----------------------------------------------------------------------------
//	Purpose: Return a pointer to the current button data, NULL if the action
//			set hasn't been loaded yet
//-----------------------------------------------------------------------------
CSteamGameControllerNew::buttonData_t * CSteamGameControllerNew::GetButtonDataForCurrentActionSet()
{
	if ( m_mapNameToActionSetData[m_iCurrentActionSet].m_hActionSet == 0 )
	{
		return NULL;
	}
	return m_mapNameToActionSetData[m_iCurrentActionSet].m_buttonData;
}


//-----------------------------------------------------------------------------
//	Purpose: Set the current active action set
//-----------------------------------------------------------------------------
void CSteamGameControllerNew::SetActiveActionSet( const char *pchActionSet )
{
	int iActionSet = m_mapNameToActionSetData.Find( pchActionSet );
	if ( iActionSet == m_mapNameToActionSetData.InvalidIndex() )
	{
		// Msg("SetActiveActionSet( %s ) - empty \n", pchActionSet);
		m_iCurrentActionSet = InsertEmptyActionSet( pchActionSet );
	}
	else
	{
		// Msg( "SetActiveActionSet( %s )\n", pchActionSet );
		m_iCurrentActionSet = iActionSet;
		if ( m_mapNameToActionSetData[m_iCurrentActionSet].m_hActionSet != 0 )
		{
#if defined( PANORAMA_PUBLIC_STEAM_SDK )
			SteamController()->ActivateActionSet( STEAM_CONTROLLER_HANDLE_ALL_CONTROLLERS, m_mapNameToActionSetData[m_iCurrentActionSet].m_hActionSet );
#else
			ClientController()->ActivateActionSet( k_nGameIDControllerConfigs_BigPicture, STEAM_CONTROLLER_HANDLE_ALL_CONTROLLERS, m_mapNameToActionSetData[m_iCurrentActionSet].m_hActionSet );
#endif
		}
	}
}


//-----------------------------------------------------------------------------
//	Purpose: Return the name of the current active action set
//-----------------------------------------------------------------------------
const char * CSteamGameControllerNew::GetCurrentActionSet() const
{
	return m_mapNameToActionSetData.Key( m_iCurrentActionSet );
}


//-----------------------------------------------------------------------------
//	Purpose: Convert actions from the controller API into panorama input
//			events
//-----------------------------------------------------------------------------
void CSteamGameControllerNew::RunFrame( void )
{
#if defined( PANORAMA_PUBLIC_STEAM_SDK )
	if ( !m_bInit )
	{
		if ( SteamController() )
		{
			SteamController()->Init();
			m_bInit = true;
		}
	}

	if ( !m_bInit )
	{
		return;
	}
#endif

	if ( !ClientControllerLocal() )
	{
		return;
	}

	ClientControllerLocal()->RunFrame();

	buttonData_t *pButtonData = GetButtonDataForCurrentActionSet();

	// Keep trying to get handles until the config is loaded and we're actually successful.
	if ( pButtonData == NULL )
	{
		ActionSetData_t &actionData = m_mapNameToActionSetData.Element( m_iCurrentActionSet );

		Assert( actionData.m_hActionSet == 0 );
#if defined( PANORAMA_PUBLIC_STEAM_SDK )
		ControllerActionSetHandle_t h = SteamController()->GetActionSetHandle( m_mapNameToActionSetData.Key( m_iCurrentActionSet ) );
#else
		ControllerActionSetHandle_t h = ClientController()->GetActionSetHandle( k_nGameIDControllerConfigs_BigPicture, m_mapNameToActionSetData.Key( m_iCurrentActionSet ) );
#endif

		if ( h == 0 )
		{
			return;
		}

#if defined( PANORAMA_PUBLIC_STEAM_SDK )
		SteamController()->ActivateActionSet( STEAM_CONTROLLER_HANDLE_ALL_CONTROLLERS, h );
#else
		ClientController()->ActivateActionSet( k_nGameIDControllerConfigs_BigPicture, STEAM_CONTROLLER_HANDLE_ALL_CONTROLLERS, h );
#endif
		actionData.m_hActionSet = h;

		pButtonData = actionData.m_buttonData;

		for ( int i = 0; i < V_ARRAYSIZE( k_buttonToAction ); i++ )
		{
			pButtonData[ k_buttonToAction[i].xKey ].iAction = i;
#if defined( PANORAMA_PUBLIC_STEAM_SDK )
			pButtonData[ k_buttonToAction[i].xKey ].hAction = SteamController()->GetDigitalActionHandle( k_buttonToAction[ i ].pchAction );
#else
			pButtonData[ k_buttonToAction[i].xKey ].hAction = ClientController()->GetDigitalActionHandle( k_nGameIDControllerConfigs_BigPicture, k_buttonToAction[ i ].pchAction );
#endif
			if ( pButtonData[ k_buttonToAction[i].xKey ].hAction != 0 )
			{
				m_bConfigLoaded = true;
			}
		}

		analogData_t *pAnalog = actionData.m_analogData;
		for ( int i = 0; i < V_ARRAYSIZE( k_analogToAction ); i++ )
		{
			Assert( i < MAX_ANALOG );
			pAnalog[i].iAction = i;
#if defined( PANORAMA_PUBLIC_STEAM_SDK )
			pAnalog[i].hAction = SteamController()->GetAnalogActionHandle( k_analogToAction[i].pchAction );
#else
			pAnalog[i].hAction = ClientController()->GetAnalogActionHandle( k_nGameIDControllerConfigs_BigPicture, k_analogToAction[i].pchAction );
#endif
		}

		return;
	}

	ControllerHandle_t pHandles[ STEAM_CONTROLLER_MAX_COUNT ];
	m_unConnectedControllers = ClientControllerLocal()->GetConnectedControllers( pHandles );

	if ( m_unConnectedControllers == 0 )
		return;

	m_bGamepadConnectedThisSession = true;

	for ( int iController = 0; iController < m_unConnectedControllers; iController++ )
	{
		if ( m_iExclusiveControllerIndex != -1 && m_iExclusiveControllerIndex != iController )
		{
			continue;
		}


		// Buttons/digital events
		for ( int iButton = 0; iButton < XK_MAX_KEYS; iButton++ )
		{
			if ( pButtonData[iButton].hAction != 0 )
			{
				ControllerDigitalActionData_t data = ClientControllerLocal()->GetDigitalActionData( pHandles[iController], pButtonData[iButton].hAction );

				if ( data.bActive )
				{
					int nRepeats = m_buttonState[iButton].nRepeats;
					if ( data.bState && m_buttonState[iButton].flLastInput < UIEngine()->GetCurrentFrameTime() ) // Button down, check for repeats
					{
						if ( m_buttonState[iButton].bSuppressUntilUp )
							continue;

						m_bHadGamepadInput = true;
						m_bGamepadUsedThisSession = true;

						if ( nRepeats == 0 )
						{
							m_buttonState[iButton].flFirstInput = UIEngine()->GetCurrentFrameTime();
						}
						m_buttonState[iButton].nRepeats++;

						float flDelayTime = SimpleSplineRemapValClamped(( UIEngine()->GetCurrentFrameTime() - m_buttonState[iButton].flFirstInput ) / g_ConVarPanoramaSteampadButtonRepeatCurveTime.GetFloat(), 0.0f, 1.0f,
							g_ConVarPanoramaSteampadButtonRepeatIntervalStart.GetFloat(), g_ConVarPanoramaSteampadButtonRepeatIntervalEnd.GetFloat());

						m_buttonState[iButton].flLastInput = UIEngine()->GetCurrentFrameTime() + flDelayTime;
					}
					else if ( !data.bState ) // Button up
					{
						m_buttonState[iButton].nRepeats = 0;
						m_buttonState[iButton].flLastInput = 0;
						m_buttonState[iButton].bSuppressUntilUp = false;
						nRepeats = 0;
					}
					else
					{
						// Msg( "Skipping due to delay time (%f, waiting for %f)\n", UIEngine()->GetCurrentFrameTime(), m_buttonState[iButton].flLastInput );
						continue;
					}

					if ( data.bState != m_buttonState[iButton].bDown || nRepeats > 0 )
					{
						InputMessage_t input;
						input.m_eSource = k_ePanelEventSourceGamepad;

						input.m_eInputType = data.bState ? k_eGamePadDown : k_eGamePadUp;
						input.m_GamePadData.m_GamePadCode = (GamePadCode)iButton;
						input.m_GamePadData.m_RepeatCount = nRepeats;

						if ( !k_buttonToAction[pButtonData[iButton].iAction].bAllowRepeat && nRepeats > 0 )
						{
							// Msg("ActionSet %s, %s: %s %s %i - skipping\n", m_mapNameToActionSetData.Key(m_iCurrentActionSet), k_buttonToAction[pButtonData[iButton].iAction].pchAction, PchNameFromGamePadCode(input.m_GamePadData.m_GamePadCode), input.m_eInputType == k_eGamePadDown ? "Down" : "Up", input.m_GamePadData.m_RepeatCount); 
							continue;
						}
						else
						{
							// Msg("ActionSet %s, %s: %s %s %i - sending\n", m_mapNameToActionSetData.Key(m_iCurrentActionSet), k_buttonToAction[pButtonData[iButton].iAction].pchAction, PchNameFromGamePadCode(input.m_GamePadData.m_GamePadCode), input.m_eInputType == k_eGamePadDown ? "Down" : "Up", input.m_GamePadData.m_RepeatCount);
							m_pInputParent->InputEvent(input);
						}

						m_iIndexForLastPress = iController;
						m_flLastUserIDAssignment = UIEngine()->GetCurrentFrameTime();
					}

					m_buttonState[ iButton ].bDown = data.bState;
				}
			}
		}


		// Analog
		if ( m_iIndexForLastPress == iController &&
			( m_flLastUserIDAssignment > m_pInputParent->GetLastGamePadControllerActiveTime() || m_pInputParent->GetLastGamePadControllerActiveTime() < 0.0001f ) )
		{
			// Only fire analog events for the current active controller
			analogData_t *pAnalog = m_mapNameToActionSetData[m_iCurrentActionSet].m_analogData;
			for ( int iAnalog = 0; iAnalog < MAX_ANALOG; iAnalog++ )
			{
				if ( pAnalog[iAnalog].hAction != 0 )
				{
					ControllerAnalogActionData_t actionData = ClientControllerLocal()->GetAnalogActionData(pHandles[iController], pAnalog[iAnalog].hAction );

					if ( actionData.bActive )
					{
						InputMessage_t input;
						input.m_eSource = k_ePanelEventSourceGamepad;

						input.m_eInputType = k_eGamePadAnalog;
						input.m_GamePadData.m_GamePadCode = k_analogToAction[pAnalog[iAnalog].iAction].xKey;
						input.m_GamePadData.m_fValueX = actionData.x * 1000.0f;
						input.m_GamePadData.m_fValueY = actionData.y * 1000.0f;
						input.m_GamePadData.m_RepeatCount = 0; // Even though we're tracking repeats, analog events always expect a 0 here apparently.
						input.m_GamePadData.m_fValueXRaw = actionData.x * 1000.0f; 
						input.m_GamePadData.m_fValueYRaw = actionData.y * 1000.0f;
						input.m_GamePadData.m_flFingerDown = 0;

						int nRepeats = m_analogState[ iAnalog ].nRepeats;

						if ( ( actionData.x != 0 || actionData.y != 0 ) )
						{
							if ( nRepeats == 0 )
							{
								m_analogState[iAnalog].flFirstInput = UIEngine()->GetCurrentFrameTime();
							}
							m_analogState[iAnalog].nRepeats++;

							float flDelayTime = SimpleSplineRemapValClamped(( UIEngine()->GetCurrentFrameTime() - m_analogState[iAnalog].flFirstInput ) / g_ConVarPanoramaSteampadButtonRepeatCurveTime.GetFloat(), 0.0f, 1.0f,
								g_ConVarPanoramaSteampadButtonRepeatIntervalStart.GetFloat(), g_ConVarPanoramaSteampadButtonRepeatIntervalEnd.GetFloat());

							m_analogState[iAnalog].flLastInput = UIEngine()->GetCurrentFrameTime() + flDelayTime;

							if ( nRepeats == 0 )
							{
								m_analogState[iAnalog].flFirstInput = UIEngine()->GetCurrentFrameTime();
							}
							m_analogState[iAnalog].nRepeats++;

							//Msg( "ActionSet %s, %s: %f %f %i - sending\n", m_mapNameToActionSetData.Key( m_iCurrentActionSet ), k_analogToAction[ pAnalog[iAnalog].iAction ].pchAction, input.m_GamePadData.m_fValueX, input.m_GamePadData.m_fValueY, input.m_GamePadData.m_RepeatCount );
					
							if ( input.m_GamePadData.m_GamePadCode == STEAM_LEFTPAD_ANALOG || input.m_GamePadData.m_GamePadCode == STEAM_RIGHTPAD_ANALOG )
							{
								// Synthesize pad down events
								if ( nRepeats == 0 )
								{
									InputMessage_t inputPadDown;
									inputPadDown.m_eSource = k_ePanelEventSourceGamepad;
									inputPadDown.m_eInputType = k_eGamePadDown;
									inputPadDown.m_GamePadData.m_GamePadCode = (input.m_GamePadData.m_GamePadCode == STEAM_LEFTPAD_ANALOG ) ? STEAM_BUTTON_LPAD_TOUCH : STEAM_BUTTON_RPAD_TOUCH;
									inputPadDown.m_GamePadData.m_RepeatCount = 0;
									m_pInputParent->InputEvent( inputPadDown );
								}
							}

#if 0
							if ( input.m_GamePadData.m_GamePadCode == STEAM_LEFTPAD_ANALOG )
							{
								Msg( "New LEFTPAD_ANALOG: %i %f %f %f %f\n", input.m_GamePadData.m_RepeatCount, input.m_GamePadData.m_fValueX, input.m_GamePadData.m_fValueY, input.m_GamePadData.m_fValueXRaw, input.m_GamePadData.m_fValueYRaw );
							}
#endif
						}
						else if ( actionData.x == 0 && actionData.y == 0 )
						{
							m_analogState[ iAnalog ].nRepeats = 0;
							m_analogState[ iAnalog ].flFirstInput = 0;
							m_analogState[ iAnalog ].flLastInput = 0;

							if ( input.m_GamePadData.m_GamePadCode == STEAM_LEFTPAD_ANALOG || input.m_GamePadData.m_GamePadCode == STEAM_RIGHTPAD_ANALOG )
							{
								// Synthesize pad up events
								if ( nRepeats != 0 )
								{
									InputMessage_t inputPadUp;
									inputPadUp.m_eSource = k_ePanelEventSourceGamepad;
									inputPadUp.m_eInputType = k_eGamePadUp;
									inputPadUp.m_GamePadData.m_GamePadCode = ( input.m_GamePadData.m_GamePadCode == STEAM_LEFTPAD_ANALOG ) ? STEAM_BUTTON_LPAD_TOUCH : STEAM_BUTTON_RPAD_TOUCH;
									inputPadUp.m_GamePadData.m_RepeatCount = 0;
									m_pInputParent->InputEvent(inputPadUp);
								}
							}

							nRepeats = 0;
						}

						m_pInputParent->InputEvent( input );
					}
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
//	Purpose: we just got focus, reset button state without firing events
//-----------------------------------------------------------------------------
void CSteamGameControllerNew::GotWindowFocus()
{
	VPROF_BUDGET( "CSteamGameController::GotWindowFocus", VPROF_BUDGETGROUP_TENFOOT );

	if ( !m_bInit )
	{
		return;
	}

	if ( ClientControllerLocal() )
	{
		ControllerHandle_t pHandles[STEAM_CONTROLLER_MAX_COUNT];
		m_unConnectedControllers = ClientControllerLocal()->GetConnectedControllers(pHandles);

		if ( m_unConnectedControllers == 0 )
			return;

		m_bGamepadConnectedThisSession = true;

		for ( int iController = 0; iController < m_unConnectedControllers; iController++ )
		{
			buttonData_t *pButtonData = GetButtonDataForCurrentActionSet();
			if ( pButtonData )
			{
				for ( int iButton = 0; iButton < XK_MAX_KEYS; iButton++ )
				{
					if ( pButtonData[iButton].hAction != 0 )
					{
						ControllerDigitalActionData_t data = ClientControllerLocal()->GetDigitalActionData(pHandles[iController], pButtonData[iButton].hAction);
						if ( data.bActive )
						{
							m_buttonState[iButton].bDown = data.bState;
							m_buttonState[iButton].nRepeats = 1;
							m_buttonState[iButton].bSuppressUntilUp = data.bState;
							m_buttonState[iButton].flFirstInput = UIEngine()->GetCurrentFrameTime();
							m_buttonState[iButton].flLastInput = UIEngine()->GetCurrentFrameTime();
						}
					}
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
//	Purpose: Pulse haptic feedback
//-----------------------------------------------------------------------------
void CSteamGameControllerNew::PulseHapticOnActiveController( ESteamControllerPad ePad, unsigned short durationMicroSec )
{
	if ( m_iIndexForLastPress != -1 )
	{
#if defined( PANORAMA_PUBLIC_STEAM_SDK )
		ControllerHandle_t pHandles[STEAM_CONTROLLER_MAX_COUNT];
		ClientControllerLocal()->GetConnectedControllers(pHandles);

		SteamController()->TriggerHapticPulse( pHandles[m_iIndexForLastPress], ePad, durationMicroSec );
#else
		ClientControllerLocal()->TriggerHapticPulse( m_iIndexForLastPress, ePad, durationMicroSec, 0, 1, 0 );
#endif
	}
}


//-----------------------------------------------------------------------------
// Purpose: Turn off the controller (typically just pressed "A" to choose to do that)
//-----------------------------------------------------------------------------
void panorama::CSteamGameControllerNew::TurnOffActiveController()
{
	if ( m_iIndexForLastPress != -1 )
	{
#if defined( PANORAMA_PUBLIC_STEAM_SDK )
#else
		ClientController()->TurnOffController( m_iIndexForLastPress );
#endif
	}
}


//-----------------------------------------------------------------------------
// Purpose: Check whether the thumb is down on an analog pad.  This is stored
//			internally as having a positive repeat count.
//-----------------------------------------------------------------------------
bool CSteamGameControllerNew::BIsFingerDownAnalog( GamePadCode code ) const
{
	for ( int i = 0; i < MAX_ANALOG; i++ )
	{
		if ( k_analogToAction[i].xKey == code )
		{
			return m_analogState[i].nRepeats > 0;
		}
	}
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Check if the thumb is down on the left pad
//-----------------------------------------------------------------------------
bool CSteamGameControllerNew::BIsFingerDownOnSteamControllerLeftPad() const
{
	return BIsFingerDownAnalog( STEAM_LEFTPAD_ANALOG );
}


//-----------------------------------------------------------------------------
// Purpose: Check if the thumb is down on the right pad
//-----------------------------------------------------------------------------
bool CSteamGameControllerNew::BIsFingerDownOnSteamControllerRightPad() const
{
	return BIsFingerDownAnalog( STEAM_RIGHTPAD_ANALOG );
}


#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: validate mem
//-----------------------------------------------------------------------------
void CSteamGameControllerNew::Validate(CValidator &validator, const char *pchName)
{
	VALIDATE_SCOPE();
	ValidateObj( m_mapNameToActionSetData );
}
#endif

#endif
