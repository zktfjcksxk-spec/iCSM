//================ Copyright (c) Valve Corporation. All Rights Reserved. ===========================
//
//
//
//==================================================================================================

#include "s1wrapper.h"
#include "../thirdparty/sdl/include/SDL.h"
#include "vgui/ISystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

//--------------------------------------------------------------------------------------------------
// SDL stubs
//--------------------------------------------------------------------------------------------------

static SDL_JoystickGUID s_guidNULL = { 0, };

DECLSPEC SDL_Joystick *SDLCALL SDL_JoystickOpen( int device_index )
{
	return (SDL_Joystick*)0;
}

DECLSPEC int SDLCALL SDL_NumJoysticks( void ) { return 0; }

DECLSPEC const char *SDLCALL SDL_JoystickNameForIndex( int device_index ) { return 0; }

DECLSPEC const char *SDLCALL SDL_JoystickName( SDL_Joystick * joystick ) { return 0; }

DECLSPEC SDL_JoystickGUID SDLCALL SDL_JoystickGetDeviceGUID( int device_index ) { return (SDL_JoystickGUID)s_guidNULL; }

DECLSPEC SDL_JoystickGUID SDLCALL SDL_JoystickGetGUID( SDL_Joystick * joystick ) { return (SDL_JoystickGUID)s_guidNULL; }

DECLSPEC void SDLCALL SDL_JoystickGetGUIDString( SDL_JoystickGUID guid, char *pszGUID, int cbGUID ) { return; }

DECLSPEC SDL_JoystickGUID SDLCALL SDL_JoystickGetGUIDFromString( const char *pchGUID ) { return (SDL_JoystickGUID)s_guidNULL; }

DECLSPEC SDL_bool SDLCALL SDL_JoystickGetAttached( SDL_Joystick * joystick ) { return SDL_bool( 0 ); }

DECLSPEC SDL_JoystickID SDLCALL SDL_JoystickInstanceID( SDL_Joystick * joystick ) { return 0; }

DECLSPEC int SDLCALL SDL_JoystickNumAxes( SDL_Joystick * joystick ) { return 0; }

DECLSPEC int SDLCALL SDL_JoystickNumBalls( SDL_Joystick * joystick ) { return 0; }

DECLSPEC int SDLCALL SDL_JoystickNumHats( SDL_Joystick * joystick ) { return 0; }

DECLSPEC int SDLCALL SDL_JoystickNumButtons( SDL_Joystick * joystick ) { return 0; }

DECLSPEC void SDLCALL SDL_JoystickUpdate( void ) { return; }

DECLSPEC int SDLCALL SDL_JoystickEventState( int state ) { return 0; }

DECLSPEC Sint16 SDLCALL SDL_JoystickGetAxis( SDL_Joystick * joystick,
											 int axis ) {
	return 0;
}

DECLSPEC Uint8 SDLCALL SDL_JoystickGetHat( SDL_Joystick * joystick,
										   int hat ) {
	return 0;
}

DECLSPEC int SDLCALL SDL_JoystickGetBall( SDL_Joystick * joystick,
										  int ball, int *dx, int *dy ) {
	return 0;
}

DECLSPEC Uint8 SDLCALL SDL_JoystickGetButton( SDL_Joystick * joystick,
											  int button ) {
	return 0;
}

DECLSPEC void SDLCALL SDL_JoystickClose( SDL_Joystick * joystick ) { return; }

DECLSPEC SDL_JoystickPowerLevel SDLCALL SDL_JoystickCurrentPowerLevel( SDL_Joystick * joystick ) { return SDL_JOYSTICK_POWER_UNKNOWN; }

DECLSPEC int SDLCALL SDL_SetClipboardText( const char *text )
{ 
	uint32 nMaxBytes = (V_strlen( text ) + 1) * sizeof( wchar_t );
	wchar_t *pwchTemp = (wchar_t*)g_pMemAlloc->Alloc( nMaxBytes );

	if( V_UTF8ToUnicode( text, pwchTemp, nMaxBytes ) )
	{
		g_pVGuiSystem->SetClipboardText( pwchTemp, V_wcslen( pwchTemp ) );
	}

	g_pMemAlloc->Free( pwchTemp );
	return 0;
}

DECLSPEC char * SDLCALL SDL_GetClipboardText( void ) 
{
	// GetClipboardTextCount() gives a maximum number of chars including the null terminator.
	int nMaxSize = g_pVGuiSystem->GetClipboardTextCount(); 
	char* pUtf8 = NULL;

	if( nMaxSize )
	{
		int nMaxBytes = nMaxSize * sizeof( wchar_t );
		wchar_t *pwchTemp = (wchar_t*)g_pMemAlloc->Alloc( nMaxBytes );
		g_pVGuiSystem->GetClipboardText( 0, pwchTemp, nMaxBytes );

		// each Unicode code point can expand to as many as four bytes in UTF-8; 
		uint32 nUtf8MaxBytes = 4 * V_wcslen( pwchTemp );
		char *pUtf8Temp = (char*)g_pMemAlloc->Alloc( nUtf8MaxBytes );
		if( V_UnicodeToUTF8( pwchTemp, pUtf8Temp, nUtf8MaxBytes ) )
		{
			uint32 nUtf8Bytes = V_strlen( pUtf8Temp ) + 1;
			pUtf8 = (char*)g_pMemAlloc->Alloc( nUtf8Bytes );
			memcpy( pUtf8, pUtf8Temp, nUtf8Bytes );
		}
		g_pMemAlloc->Free( pwchTemp );
		g_pMemAlloc->Free( pUtf8Temp );
	}

	return pUtf8;
}

DECLSPEC char * SDLCALL SDL_GameControllerMapping( SDL_GameController * gamecontroller )
{
	return 0;
}

DECLSPEC SDL_GameController *SDLCALL SDL_GameControllerOpen( int joystick_index )
{
	return (SDL_GameController*)0;
}

DECLSPEC const char *SDLCALL SDL_GameControllerName( SDL_GameController *gamecontroller )
{
	return 0;
}

DECLSPEC Sint16 SDLCALL SDL_GameControllerGetAxis( SDL_GameController *gamecontroller,
												   SDL_GameControllerAxis axis )
{
	return 0;
}

DECLSPEC Uint8 SDLCALL SDL_GameControllerGetButton( SDL_GameController *gamecontroller,
													SDL_GameControllerButton button )
{
	return 0;
}

DECLSPEC void SDLCALL SDL_GameControllerClose( SDL_GameController *gamecontroller )
{

}

DECLSPEC SDL_Joystick *SDLCALL SDL_GameControllerGetJoystick( SDL_GameController *gamecontroller )
{
	return (SDL_Joystick*)0;
}

DECLSPEC SDL_Haptic *SDLCALL SDL_HapticOpenFromJoystick( SDL_Joystick *
														 joystick );

DECLSPEC void SDLCALL SDL_HapticClose( SDL_Haptic * haptic )
{
	return;
}

DECLSPEC int SDLCALL SDL_HapticRumbleSupported( SDL_Haptic * haptic )
{
	return 0;
}

DECLSPEC int SDLCALL SDL_HapticRumbleInit( SDL_Haptic * haptic )
{
	return 0;
}

DECLSPEC int SDLCALL SDL_HapticRumblePlay( SDL_Haptic * haptic, float strength, Uint32 length )
{
	return 0;
}

DECLSPEC int SDLCALL SDL_HapticRumbleStop( SDL_Haptic * haptic )
{
	return 0;
}

void SDLCALL SDL_free( void *mem )
{
	if( mem )
	{
		g_pMemAlloc->Free( mem );
	}
	return;
}

SDL_bool SDLCALL SDL_HasClipboardText( void )
{
	if( g_pVGuiSystem->GetClipboardTextCount() > 0 )
	{
		return SDL_bool( true );
	}
	return SDL_bool( false );
}

int SDLCALL SDL_ShowMessageBox( const SDL_MessageBoxData *messageboxdata, int *buttonid )
{
	// Assume Yes/No box, return button ID 1 for Yes
	int nResult = ::MessageBox(NULL, messageboxdata->message, messageboxdata->title, MB_YESNO);
	*buttonid = (nResult == IDYES) ? 1 : 0;
	return 0;
}
int SDLCALL SDL_ShowSimpleMessageBox( Uint32 flags, const char *title, const char *message, SDL_Window *window )
{
	::MessageBox(NULL, message, title, 0);
	Msg("%s %s\n", title, message);
	return 0;
}
void SDLCALL SDL_AddEventWatch( SDL_EventFilter filter, void *userdata )
{
	return;
}

void SDLCALL SDL_DelEventWatch( SDL_EventFilter filter, void *userdata )
{
	return;
}

SDL_Haptic *SDLCALL SDL_HapticOpenFromJoystick( SDL_Joystick *joystick )
{
	return (SDL_Haptic *)0;
}

SDL_bool SDLCALL SDL_SetHint( const char *name, const char *value )
{
	return SDL_bool( true );
}
int SDLCALL SDL_InitSubSystem( Uint32 flags )
{
	return 0;
}
void SDLCALL SDL_QuitSubSystem( Uint32 flags )
{
	return;
}

