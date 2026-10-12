//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "tier0/platform.h"
#include "tier0/tslist.h"
#include "tier1/interface.h"
#include "tier1/utlflags.h"
#include "panorama/input/gamepadcodes.h"
#include "panorama/input/iuiinput.h"
#include "mathlib/mathlib.h"
#include "mathlib/beziercurve.h"
#include "winlite.h"

#define HAVE_M_PI
#include <SDL_gamecontroller.h>
#include <SDL_events.h>
#include <SDL_haptic.h>

#define XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE  7849
#define XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE 8689

#define MAX_JOYSTICK_BUTTONS 32

namespace panorama
{

class IUISettings;

typedef struct _XINPUT_VIBRATION
{
	WORD                                wLeftMotorSpeed;
	WORD                                wRightMotorSpeed;
} XINPUT_VIBRATION;

} // namespace panorama

namespace panorama
{
#define INVALID_USER_ID -1
#define XBX_STICK_SCALE_TRIGGER(x) 	( ( float )XBX_MAX_STICKSAMPLE_TRIGGER/( float )( XBX_MAX_STICKSAMPLE_TRIGGER-(x) ) )
#define	XBX_MAX_MOTOR_SPEED			500
	
#define XBX_MAX_BUTTONSAMPLE		32768
#define XBX_MAX_ANALOGSAMPLE		32000.0f
	
#define XBX_MAX_STICKSAMPLE_TRIGGER	32768
#define XBX_MAX_STICKSAMPLE_LEFT	32768
#define XBX_MAX_STICKSAMPLE_RIGHT	32767
#define XBX_MAX_STICKSAMPLE_DOWN	32768
#define XBX_MAX_STICKSAMPLE_UP		32767
#define XBX_STICK_SCALE_LEFT(x) 	( ( float )XBX_MAX_STICKSAMPLE_LEFT/( float )( XBX_MAX_STICKSAMPLE_LEFT-(x) ) )
#define XBX_STICK_SCALE_RIGHT(x) 	( ( float )XBX_MAX_STICKSAMPLE_RIGHT/( float )( XBX_MAX_STICKSAMPLE_RIGHT-(x) ) )
#define XBX_STICK_SCALE_DOWN(x) 	( ( float )XBX_MAX_STICKSAMPLE_DOWN/( float )( XBX_MAX_STICKSAMPLE_DOWN-(x) ) )
#define XBX_STICK_SCALE_UP(x)	 	( ( float )XBX_MAX_STICKSAMPLE_UP/( float )( XBX_MAX_STICKSAMPLE_UP-(x) ) )

const float k_flAnalogUserInputTimeout = 1.5f; // after 1.5 seconds of no activity on the chosen controller allow it to change

//
// Handles getting data from a connected controller like a gamepad
//
class CGamepadController
{
public:
	CGamepadController( IUIInput *pInputParent );
	~CGamepadController();
	void Initialize( IUISettings *pSettings );
	void RunFrame();
	void Shutdown();

	int GetNumGamepadsConnected() const;
	float GetDeadZoneValue( GamePadCode code );

	// return true if the gamepad had input since you last asked
	bool BHadGamepadInput()
	{
		bool bInput = m_bHadGamepadInput;
		m_bHadGamepadInput = false;
		return bInput;
	}

	bool BWasGamepadUsedThisSession()
	{
		return m_bGamepadUsedThisSession;
	}

	bool BWasGamepadConnectedThisSession()
	{
		return m_bGamepadConnectedThisSession;
	}

	const char *PchGamePadName( int iDevice  );

	bool OnActionGamepadMappingsReload();

	float GetLastUserIDAssignment() const { return m_flLastUserIDAssignment; }

	void RemoteGamepadAttached( int nGamepadID );
	void RemoteGamepadDetached( int nGamepadID );
	void SetRemoteGamepadAxis( int nGamepadID, int nAxis, int nValue );
	void SetRemoteGamepadButton( int nGamepadID, int nButton, int nValue );

	void GotWindowFocus();

	// from 0 to GetNumGamepadsConnected()
	IUIInput::EControllerPowerLevel GetConnectGamePadPowerLevel( int iGamePad );
	void PulseGamePadHaptics( int iGamePad, float flStrength, int uEffectTimeMS );

#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, const tchar *pchName );
#endif
private:

	static int SDL_GameControllerEventWatcher( void *userdata, SDL_Event * event );
	void OpenXDevice( int device_index, bool bRemoteInput );
	void CloseXDevice( int instance_id, bool bRemoteInput );
	void ProcessRemoteInput();

	enum EStickAxis
	{
		STICK1_AXIS_X,
		STICK1_AXIS_Y,
		STICK2_AXIS_X,
		STICK2_AXIS_Y,
		LEFT_TRIGGER_AXIS,
		RIGHT_TRIGGER_AXIS,
		MAX_STICKAXIS
	};

	struct appKey_t
	{
		appKey_t() { repeats = sample = 0; flLastInput = 0.0f; flFirstInput = 0.0f; m_bSuppressUntilUp = false; }
		int repeats;
		int	sample;
		double flLastInput;
		double flFirstInput;
		bool m_bSuppressUntilUp;
	};

	struct xdevice_t
	{
		bool				active;
		SDL_GameController	*m_pGameController;
		SDL_Joystick 		*m_pJoystick;
		SDL_Haptic			*m_pHaptic;
		bool				m_bRemoteInput;
		int					instance_id;
		uint8				nAxes;
		uint8				nButtons;

		// state for remote gamepads
		int16				m_arrAxisValues[ SDL_CONTROLLER_AXIS_MAX ];
		uint8				m_arrButtonValues[ SDL_CONTROLLER_BUTTON_MAX ];

		// track Xbox stick keys from previous frame
		GamePadCode			lastStickKeys[MAX_STICKAXIS];
		int					stickThreshold[MAX_STICKAXIS];
		bool				stickZeroed[MAX_STICKAXIS];

		float				stickScale[MAX_STICKAXIS];
		int					quitTimeout;
		appKey_t			m_appXKeys[ XK_MAX_KEYS ];
		
		CUtlString			sMappingString;
	};

	void CheckStickZeroed( xdevice_t *pDevice, int nX1, int nY1, int nX2, int nY2 );

	enum ERemoteGamepadInput
	{
		k_ERemoteGamepadAttached,
		k_ERemoteGamepadDetached,
		k_ERemoteGamepadAxisChanged,
		k_ERemoteGamepadButtonChanged,
	};

	struct RemoteGamepadInput_t
	{
		ERemoteGamepadInput m_eGamepadInput;
		int m_nInstanceID;
		union
		{
			int m_nAxis;
			int m_nButton;
		};
		union
		{
			int16 m_nAxisValue;
			uint8 m_nButtonValue;
		};
	};

	void SetXDeviceRumble( float fLeftMotor, float fRightMotor, int instance_id );
	void HandleXDeviceAxis( xdevice_t *pXDevice, int nAxisValue, GamePadCode negativeKey, GamePadCode positiveKey, int axisID );
	void ReadXDevice( xdevice_t* pXDevice );
	void PostXKeyEvent( xdevice_t *pDevice, GamePadCode xKey, int nSample );

	int16 GetAxisValue( xdevice_t* pXDevice, SDL_GameControllerAxis axis );
	uint8 GetButtonValue( xdevice_t* pXDevice, SDL_GameControllerButton button );
	
	void GetSDLGameControllerMappings( CUtlString* );

	// Xbox controller info
	CUtlVector<xdevice_t>	m_XDevices;

	IUIInput *m_pInputParent;

	CCubicBezierCurve< Vector2D > m_AxisRepeatCurve;

	bool m_bHadGamepadInput;
	bool m_bGamepadConnectedThisSession; // true if we detected a gamepad at all during execution
	bool m_bGamepadUsedThisSession; // true if a gamepad was used for input for any period during this execution
	bool m_bUserDisabled; // true if -nojoy was given on the command line to explicitly turn off support

	xdevice_t *m_pXDeviceForLastButtonPress; // userid of the last controller to push a button
	double m_flLastUserIDAssignment;

	IUISettings *m_pSettings; // cache of the settings object so we can reload SDL gamepad bindings

	CTSQueue<RemoteGamepadInput_t> m_RemoteGamepadInput;
};

} // namespace panorama

#endif // CONTROLLER_H
