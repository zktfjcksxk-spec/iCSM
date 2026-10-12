//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef PANORAMASTEAMCONTROLLER_H
#define PANORAMASTEAMCONTROLLER_H

#include "tier0/platform.h"
#include "tier1/interface.h"
#include "tier1/utlflags.h"
#include "panorama/input/gamepadcodes.h"
#include "panorama/input/iuiinput.h"
#include "mathlib/mathlib.h"
#include "mathlib/beziercurve.h"

#include "steam/isteamcontroller.h"

#if defined( PANORAMA_PUBLIC_STEAM_SDK )
#include "steam/steam_api.h"
#elif !defined( SOURCE2_PANORAMA_FIXME ) 
#include "steam/client_api.h"
#endif
#include "winlite.h"

#if ( defined( PANORAMA_PUBLIC_STEAM_SDK ) || defined( SOURCE2_PANORAMA ) ) && !defined( MAX_STEAM_CONTROLLERS )
	#define MAX_STEAM_CONTROLLERS STEAM_CONTROLLER_MAX_COUNT
#endif

namespace panorama
{
//
// Handles getting data from a connected controller like a gamepad
//
class CSteamGameController
{
public:
	CSteamGameController( IUIInput *pInputParent );
	~CSteamGameController();
	void RunFrame();
	void Shutdown();

	uint32 GetNumGamepadsConnected() const;

	// return true if the gamepad had input since you last asked
	bool BHadGamepadInput()
	{
		bool bInput = m_bHadGamepadInput;
		m_bHadGamepadInput = false;
		return bInput;
	}

	bool BWasSteamControllerUsedThisSession()
	{
		return m_bGamepadUsedThisSession;
	}

	bool BWasSteamControllerConnectedThisSession()
	{
		return m_bGamepadConnectedThisSession;
	}

	bool BIsFingerDownOnSteamControllerLeftPad() const
	{
		if( m_iIndexForLastPress != -1 )
		{
			return m_Device[m_iIndexForLastPress].m_flLeftFingerDownTime > 0.000001f;
		}
		return false;
	}

	bool BIsFingerDownOnSteamControllerRightPad() const
	{
		if( m_iIndexForLastPress != -1 )
		{
			return m_Device[m_iIndexForLastPress].m_flRightFingerDownTime > 0.000001f;
		}
		return false;
	}

	float GetLastUserIDAssignment() const { return m_flLastUserIDAssignment; }
	
	int GetLastActiveControllerIndex() const { return m_iIndexForLastPress; }

	void PulseHapticOnActiveController( ESteamControllerPad ePad, unsigned short durationMicroSec );
	
	// Disables every Steam Controller except the requested index; -1 to re-enable all
	void SetControllerExclusiveEnabledIndex( int iIndex ) { m_iExclusiveControllerIndex = iIndex; }

	void TurnOffActiveController();

	const char *PchGamePadName( int iDevice )
	{
		return "Steam Controller";
	}

	void GotWindowFocus();

#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, const tchar *pchName );
#endif
private:
	enum EStickAxis
	{
		LEFTPAD_AXIS_X,
		LEFTPAD_AXIS_Y,
		RIGHTPAD_AXIS_X,
		RIGHTPAD_AXIS_Y,
		LEFTSTICK_AXIS_X,
		LEFTSTICK_AXIS_Y,
		LEFT_TRIGGER_AXIS,
		RIGHT_TRIGGER_AXIS,
		MAX_STICKAXIS
	};

#if !defined( SOURCE2_PANORAMA_FIXME ) && !defined( PANORAMA_PUBLIC_STEAM_SDK )
	void ReadSteamController( int iIndex, SteamControllerStateInternal_t &state );
#endif
	void HandleDeviceAxis( int iIndex, int nAxisValue, GamePadCode negativeKey, GamePadCode positiveKey, EStickAxis axisID );
	void PostKeyEvent( int iIndex, GamePadCode xKey, int nSample );
	const int GetSteamPadDeadZone( EStickAxis axis );

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
		xdevice_t() : m_vecRightDoubleTapStartPos( 0.0f, 0.0f ), m_vecLeftDoubleTapStartPos( 0.0f, 0.0f )
		{ 
			V_memset( lastStickKeys, 0, sizeof( lastStickKeys ) );
			m_vecInitialLeftPos.x = 0;
			m_vecInitialLeftPos.y = 0;
			m_vecInitialRightPos.x = 0;
			m_vecInitialRightPos.y = 0;
			m_flLeftFingerDownTime = 0.0f;
			m_flRightFingerDownTime = 0.0f;
			m_flLeftFingerDoubleTapStart = 0.0f;
			m_flRightFingerDoubleTapStart = 0.0f;
			m_unLeftTapCount = 0;
			m_unRightTapCount = 0;

			m_iLastUpdatedRightPadPos = -1;
			for( int i=0; i<V_ARRAYSIZE(m_vecRecentRightPadPos); ++i )
			{
				m_vecRecentRightPadPos[i].m_bIsValid = false;
				m_vecRecentRightPadPos[i].m_vec.x = 0.0f;
				m_vecRecentRightPadPos[i].m_vec.y = 0.0f;
			}

		}	
		double				m_flLeftFingerDownTime;
		double				m_flRightFingerDownTime;
		Vector2D			m_vecInitialLeftPos;
		Vector2D			m_vecInitialRightPos;

		double				m_flLeftFingerDoubleTapStart;
		double				m_flRightFingerDoubleTapStart;
		Vector2D			m_vecLeftDoubleTapStartPos;
		Vector2D			m_vecRightDoubleTapStartPos;
		uint32				m_unLeftTapCount;
		uint32				m_unRightTapCount;

		int					m_iLastUpdatedRightPadPos;

		struct LastPadPosData_t
		{
			bool m_bIsValid;
			Vector2D m_vec;
		};
		LastPadPosData_t			m_vecRecentRightPadPos[6];

		GamePadCode			lastStickKeys[MAX_STICKAXIS];
		appKey_t			m_appXKeys[ XK_MAX_KEYS ];
	};

	xdevice_t m_Device[MAX_STEAM_CONTROLLERS];

	Vector m_vSteamControllerLeftPadPos;
	Vector m_vSteamControllerLeftPadPosPrev;
	Vector m_vSteamControllerMomemtum;
	Vector m_vSteamControllerCurrentPos;
	double m_flSteamControllerPrevTime;
	double m_flSteamControllerLastBoundaryTime;

	IUIInput *m_pInputParent;

	uint32 m_unNumConnected;

	bool m_bHadGamepadInput;
	bool m_bGamepadConnectedThisSession; // true if we detected a gamepad at all during execution
	bool m_bGamepadUsedThisSession; // true if a gamepad was used for input for any period during this execution

	int m_iIndexForLastPress;
	double m_flLastUserIDAssignment;
	
	int m_iExclusiveControllerIndex;
};

} // namespace panorama

#endif // PANORAMASTEAMCONTROLLER_H
