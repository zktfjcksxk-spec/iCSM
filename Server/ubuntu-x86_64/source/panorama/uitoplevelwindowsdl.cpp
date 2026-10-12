//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#include "stdafx.h"
#include "uitoplevelwindowsdl.h"
#include <SDL_syswm.h>
#include "renderer/uirenderengine.h"
#include "renderer/sdlopenglsurface.h"
#include "uienginesdl.h"
#include "panorama/uievents.h"
#include "input/keycodes.h"
#ifdef OSX
#include "osxhelpers.h"
#endif
#ifdef LINUX
#include "X11/Xatom.h"
#endif
#include "tgaloader.h"
#include "openvr.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

#ifdef WIN32
extern CInterlockedInt g_nInFullscreenSwitch;
#else
CInterlockedInt g_nInFullscreenSwitch;
#endif

// Use an existing panorama helper rather than this
//#define LOG_SDL_EVENT( fmt, ... ) printf( fmt,  ## __VA_ARGS__ );
#define LOG_SDL_EVENT( fmt, ... )

CUtlMap< SDL_Window *, CTopLevelWindowSDL *, int, CDefLess< SDL_Window * > > CTopLevelWindowSDL::s_MapWindowInstances;

#define SDL_WHEEL_DELTA					1
static int32 g_nMouseDeltaLeftOvers = 0;

namespace panorama
{
	
#ifdef WIN32
extern uint32 GetWheelScrollLines(); // in uitoplevelwindowwin32.cpp
#else
uint32 GetWheelScrollLines()
{
	return 3;
}
#endif	
}

//-----------------------------------------------------------------------------
// Purpose: Translates win32 mouse message modifiers into our modifiers
//-----------------------------------------------------------------------------
uint32 GetInputModifiers()
{
	uint32 unState = SDL_GetModState();
	uint32 unModifiers = MODIFIER_NONE;
	if ( unState & KMOD_LSHIFT )
		unModifiers |= MODIFIER_LSHIFT;
	if ( unState & KMOD_RSHIFT )
		unModifiers |= MODIFIER_RSHIFT;
	if ( unState & KMOD_LCTRL )
		unModifiers |= MODIFIER_LCONTROL;
	if ( unState & KMOD_RCTRL )
		unModifiers |= MODIFIER_RCONTROL;
	if ( unState & KMOD_LALT )
		unModifiers |= MODIFIER_LALT;
	if ( unState & KMOD_RALT )
		unModifiers |= MODIFIER_RALT;
	if ( unState & KMOD_LGUI )
		unModifiers |= MODIFIER_LWIN;
	if ( unState & KMOD_RGUI )
		unModifiers |= MODIFIER_RWIN;

	return unModifiers;
}

static panorama::KeyCode SDLKeysymToKeyCode( SDL_Keysym keysym )
{
	switch ( keysym.sym )
	{
	case SDLK_RETURN:                                  return KEY_ENTER;
	case SDLK_ESCAPE:                                  return KEY_ESCAPE;
	case SDLK_BACKSPACE:                               return KEY_BACKSPACE;
	case SDLK_TAB:                                     return KEY_TAB;
	case SDLK_SPACE:                                   return KEY_SPACE;
//	case SDLK_EXCLAIM:                                 return KEY_EXCLAIM;
//	case SDLK_QUOTEDBL:                                return KEY_QUOTEDBL;
//	case SDLK_HASH:                                    return KEY_HASH;
//	case SDLK_PERCENT:                                 return KEY_PERCENT;
//	case SDLK_DOLLAR:                                  return KEY_DOLLAR;
//	case SDLK_AMPERSAND:                               return KEY_AMPERSAND;
	case SDLK_QUOTE:                                   return KEY_APOSTROPHE;
//	case SDLK_LEFTPAREN:                               return KEY_LPAREN;
//	case SDLK_RIGHTPAREN:                              return KEY_RPAREN;
//	case SDLK_ASTERISK:                                return KEY_ASTERISK;
//	case SDLK_PLUS:                                    return KEY_PLUS;
	case SDLK_COMMA:                                   return KEY_COMMA;
	case SDLK_MINUS:                                   return KEY_MINUS;
	case SDLK_PERIOD:                                  return KEY_PERIOD;
	case SDLK_SLASH:                                   return KEY_SLASH;
	case SDLK_0:                                       return KEY_0;
	case SDLK_1:                                       return KEY_1;
	case SDLK_2:                                       return KEY_2;
	case SDLK_3:                                       return KEY_3;
	case SDLK_4:                                       return KEY_4;
	case SDLK_5:                                       return KEY_5;
	case SDLK_6:                                       return KEY_6;
	case SDLK_7:                                       return KEY_7;
	case SDLK_8:                                       return KEY_8;
	case SDLK_9:                                       return KEY_9;
//	case SDLK_COLON:                                   return KEY_COLON;
	case SDLK_SEMICOLON:                               return KEY_SEMICOLON;
//	case SDLK_LESS:                                    return KEY_LESS;
	case SDLK_EQUALS:                                  return KEY_EQUAL;
//	case SDLK_GREATER:                                 return KEY_GREATER;
//	case SDLK_QUESTION:                                return KEY_QUESTION;
//	case SDLK_AT:                                      return KEY_AT;
	case SDLK_LEFTBRACKET:                             return KEY_LBRACKET;
	case SDLK_BACKSLASH:                               return KEY_BACKSLASH;
	case SDLK_RIGHTBRACKET:                            return KEY_RBRACKET;
//	case SDLK_CARET:                                   return KEY_CARET;
//	case SDLK_UNDERSCORE:                              return KEY_UNDERSCORE;
	case SDLK_BACKQUOTE:                               return KEY_BACKQUOTE;
	case SDLK_a:                                       return KEY_A;
	case SDLK_b:                                       return KEY_B;
	case SDLK_c:                                       return KEY_C;
	case SDLK_d:                                       return KEY_D;
	case SDLK_e:                                       return KEY_E;
	case SDLK_f:                                       return KEY_F;
	case SDLK_g:                                       return KEY_G;
	case SDLK_h:                                       return KEY_H;
	case SDLK_i:                                       return KEY_I;
	case SDLK_j:                                       return KEY_J;
	case SDLK_k:                                       return KEY_K;
	case SDLK_l:                                       return KEY_L;
	case SDLK_m:                                       return KEY_M;
	case SDLK_n:                                       return KEY_N;
	case SDLK_o:                                       return KEY_O;
	case SDLK_p:                                       return KEY_P;
	case SDLK_q:                                       return KEY_Q;
	case SDLK_r:                                       return KEY_R;
	case SDLK_s:                                       return KEY_S;
	case SDLK_t:                                       return KEY_T;
	case SDLK_u:                                       return KEY_U;
	case SDLK_v:                                       return KEY_V;
	case SDLK_w:                                       return KEY_W;
	case SDLK_x:                                       return KEY_X;
	case SDLK_y:                                       return KEY_Y;
	case SDLK_z:                                       return KEY_Z;
	case SDLK_CAPSLOCK:                                return KEY_CAPSLOCK;
	case SDLK_F1:                                      return KEY_F1;
	case SDLK_F2:                                      return KEY_F2;
	case SDLK_F3:                                      return KEY_F3;
	case SDLK_F4:                                      return KEY_F4;
	case SDLK_F5:                                      return KEY_F5;
	case SDLK_F6:                                      return KEY_F6;
	case SDLK_F7:                                      return KEY_F7;
	case SDLK_F8:                                      return KEY_F8;
	case SDLK_F9:                                      return KEY_F9;
	case SDLK_F10:                                     return KEY_F10;
	case SDLK_F11:                                     return KEY_F11;
	case SDLK_F12:                                     return KEY_F12;
	case SDLK_PRINTSCREEN:                             return KEY_PRINTSCREEN;
	case SDLK_SCROLLLOCK:                              return KEY_SCROLLLOCK;
	case SDLK_PAUSE:                                   return KEY_BREAK;
	case SDLK_INSERT:                                  return KEY_INSERT;
	case SDLK_HOME:                                    return KEY_HOME;
	case SDLK_PAGEUP:                                  return KEY_PAGEUP;
	case SDLK_DELETE:                                  return KEY_DELETE;
	case SDLK_END:                                     return KEY_END;
	case SDLK_PAGEDOWN:                                return KEY_PAGEDOWN;
	case SDLK_RIGHT:                                   return KEY_RIGHT;
	case SDLK_LEFT:                                    return KEY_LEFT;
	case SDLK_DOWN:                                    return KEY_DOWN;
	case SDLK_UP:                                      return KEY_UP;
	case SDLK_NUMLOCKCLEAR:                            return KEY_NUMLOCK;
	case SDLK_KP_DIVIDE:                               return KEY_PAD_DIVIDE;
	case SDLK_KP_MULTIPLY:                             return KEY_PAD_MULTIPLY;
	case SDLK_KP_MINUS:                                return KEY_PAD_MINUS;
	case SDLK_KP_PLUS:                                 return KEY_PAD_PLUS;
	case SDLK_KP_ENTER:                                return KEY_PAD_ENTER;
	case SDLK_KP_1:                                    return KEY_PAD_1;
	case SDLK_KP_2:                                    return KEY_PAD_2;
	case SDLK_KP_3:                                    return KEY_PAD_3;
	case SDLK_KP_4:                                    return KEY_PAD_4;
	case SDLK_KP_5:                                    return KEY_PAD_5;
	case SDLK_KP_6:                                    return KEY_PAD_6;
	case SDLK_KP_7:                                    return KEY_PAD_7;
	case SDLK_KP_8:                                    return KEY_PAD_8;
	case SDLK_KP_9:                                    return KEY_PAD_9;
	case SDLK_KP_0:                                    return KEY_PAD_0;
	case SDLK_KP_PERIOD:                               return KEY_PAD_DECIMAL;
//	case SDLK_APPLICATION:                             return KEY_APPLICATION;
//	case SDLK_POWER:                                   return KEY_POWER;
//	case SDLK_KP_EQUALS:                               return KEY_KP_EQUALS;
	case SDLK_F13:                                     return KEY_F13;
	case SDLK_F14:                                     return KEY_F14;
	case SDLK_F15:                                     return KEY_F15;
	case SDLK_F16:                                     return KEY_F16;
	case SDLK_F17:                                     return KEY_F17;
	case SDLK_F18:                                     return KEY_F18;
	case SDLK_F19:                                     return KEY_F19;
//	case SDLK_F20:                                     return KEY_F20;
//	case SDLK_F21:                                     return KEY_F21;
//	case SDLK_F22:                                     return KEY_F22;
//	case SDLK_F23:                                     return KEY_F23;
//	case SDLK_F24:                                     return KEY_F24;
//	case SDLK_EXECUTE:                                 return KEY_EXECUTE;
//	case SDLK_HELP:                                    return KEY_HELP;
//	case SDLK_MENU:                                    return KEY_MENU;
//	case SDLK_SELECT:                                  return KEY_SELECT;
//	case SDLK_STOP:                                    return KEY_STOP;
//	case SDLK_AGAIN:                                   return KEY_AGAIN;
//	case SDLK_UNDO:                                    return KEY_UNDO;
//	case SDLK_CUT:                                     return KEY_CUT;
//	case SDLK_COPY:                                    return KEY_COPY;
//	case SDLK_PASTE:                                   return KEY_PASTE;
//	case SDLK_FIND:                                    return KEY_FIND;
//	case SDLK_MUTE:                                    return KEY_MUTE;
	case SDLK_VOLUMEUP:                                return KEY_VOLUME_UP;
	case SDLK_VOLUMEDOWN:                              return KEY_VOLUME_DOWN;
//	case SDLK_KP_COMMA:                                return KEY_KP_COMMA;
//	case SDLK_KP_EQUALSAS400:                          return KEY_KP_EQUALSAS400;
//	case SDLK_ALTERASE:                                return KEY_ALTERASE;
//	case SDLK_SYSREQ:                                  return KEY_SYSREQ;
//	case SDLK_CANCEL:                                  return KEY_CANCEL;
//	case SDLK_CLEAR:                                   return KEY_CLEAR;
//	case SDLK_PRIOR:                                   return KEY_PRIOR;
//	case SDLK_RETURN2:                                 return KEY_RETURN2;
//	case SDLK_SEPARATOR:                               return KEY_SEPARATOR;
//	case SDLK_OUT:                                     return KEY_OUT;
//	case SDLK_OPER:                                    return KEY_OPER;
//	case SDLK_CLEARAGAIN:                              return KEY_CLEARAGAIN;
//	case SDLK_CRSEL:                                   return KEY_CRSEL;
//	case SDLK_EXSEL:                                   return KEY_EXSEL;
//	case SDLK_KP_00:                                   return KEY_KP_00;
//	case SDLK_KP_000:                                  return KEY_KP_000;
//	case SDLK_THOUSANDSSEPARATOR:                      return KEY_THOUSANDSSEPAR;
//	case SDLK_DECIMALSEPARATOR:                        return KEY_DECIMALSEPARAT;
//	case SDLK_CURRENCYUNIT:                            return KEY_CURRENCYUNIT;
//	case SDLK_CURRENCYSUBUNIT:                         return KEY_CURRENCYSUBUNI;
//	case SDLK_KP_LEFTPAREN:                            return KEY_KP_LEFTPAREN;
//	case SDLK_KP_RIGHTPAREN:                           return KEY_KP_RIGHTPAREN;
//	case SDLK_KP_LEFTBRACE:                            return KEY_KP_LEFTBRACE;
//	case SDLK_KP_RIGHTBRACE:                           return KEY_KP_RIGHTBRACE;
//	case SDLK_KP_TAB:                                  return KEY_KP_TAB;
//	case SDLK_KP_BACKSPACE:                            return KEY_KP_BACKSPACE;
//	case SDLK_KP_A:                                    return KEY_KP_A;
//	case SDLK_KP_B:                                    return KEY_KP_B;
//	case SDLK_KP_C:                                    return KEY_KP_C;
//	case SDLK_KP_D:                                    return KEY_KP_D;
//	case SDLK_KP_E:                                    return KEY_KP_E;
//	case SDLK_KP_F:                                    return KEY_KP_F;
//	case SDLK_KP_XOR:                                  return KEY_KP_XOR;
//	case SDLK_KP_POWER:                                return KEY_KP_POWER;
//	case SDLK_KP_PERCENT:                              return KEY_KP_PERCENT;
//	case SDLK_KP_LESS:                                 return KEY_KP_LESS;
//	case SDLK_KP_GREATER:                              return KEY_KP_GREATER;
//	case SDLK_KP_AMPERSAND:                            return KEY_KP_AMPERSAND;
//	case SDLK_KP_DBLAMPERSAND:                         return KEY_KP_DBLAMPERSAN;
//	case SDLK_KP_VERTICALBAR:                          return KEY_KP_VERTICALBAR;
//	case SDLK_KP_DBLVERTICALBAR:                       return KEY_KP_DBLVERTICAL;
//	case SDLK_KP_COLON:                                return KEY_KP_COLON;
//	case SDLK_KP_HASH:                                 return KEY_KP_HASH;
//	case SDLK_KP_SPACE:                                return KEY_KP_SPACE;
//	case SDLK_KP_AT:                                   return KEY_KP_AT;
//	case SDLK_KP_EXCLAM:                               return KEY_KP_EXCLAM;
//	case SDLK_KP_MEMSTORE:                             return KEY_KP_MEMSTORE;
//	case SDLK_KP_MEMRECALL:                            return KEY_KP_MEMRECALL;
//	case SDLK_KP_MEMCLEAR:                             return KEY_KP_MEMCLEAR;
//	case SDLK_KP_MEMADD:                               return KEY_KP_MEMADD;
//	case SDLK_KP_MEMSUBTRACT:                          return KEY_KP_MEMSUBTRACT;
//	case SDLK_KP_MEMMULTIPLY:                          return KEY_KP_MEMMULTIPLY;
//	case SDLK_KP_MEMDIVIDE:                            return KEY_KP_MEMDIVIDE;
//	case SDLK_KP_PLUSMINUS:                            return KEY_KP_PLUSMINUS;
//	case SDLK_KP_CLEAR:                                return KEY_KP_CLEAR;
//	case SDLK_KP_CLEARENTRY:                           return KEY_KP_CLEARENTRY;
//	case SDLK_KP_BINARY:                               return KEY_KP_BINARY;
//	case SDLK_KP_OCTAL:                                return KEY_KP_OCTAL;
//	case SDLK_KP_DECIMAL:                              return KEY_KP_DECIMAL;
//	case SDLK_KP_HEXADECIMAL:                          return KEY_KP_HEXADECIMAL;
	case SDLK_LCTRL:                                   return KEY_LCONTROL;
	case SDLK_LSHIFT:                                  return KEY_LSHIFT;
	case SDLK_LALT:                                    return KEY_LALT;
	case SDLK_LGUI:                                    return KEY_LWIN;
	case SDLK_RCTRL:                                   return KEY_RCONTROL;
	case SDLK_RSHIFT:                                  return KEY_RSHIFT;
	case SDLK_RALT:                                    return KEY_RALT;
	case SDLK_RGUI:                                    return KEY_RWIN;
//	case SDLK_MODE:                                    return KEY_MODE;
	case SDLK_AUDIONEXT:                               return KEY_MEDIA_NEXT_TRACK;
	case SDLK_AUDIOPREV:                               return KEY_MEDIA_PREV_TRACK;
	case SDLK_AUDIOSTOP:                               return KEY_MEDIA_STOP;
	case SDLK_AUDIOPLAY:                               return KEY_MEDIA_PLAY_PAUSE;
	case SDLK_AUDIOMUTE:                               return KEY_VOLUME_MUTE;
//	case SDLK_MEDIASELECT:                             return KEY_MEDIASELECT;
//	case SDLK_WWW:                                     return KEY_WWW;
//	case SDLK_MAIL:                                    return KEY_MAIL;
//	case SDLK_CALCULATOR:                              return KEY_CALCULATOR;
//	case SDLK_COMPUTER:                                return KEY_COMPUTER;
//	case SDLK_AC_SEARCH:                               return KEY_AC_SEARCH;
//	case SDLK_AC_HOME:                                 return KEY_AC_HOME;
//	case SDLK_AC_BACK:                                 return KEY_AC_BACK;
//	case SDLK_AC_FORWARD:                              return KEY_AC_FORWARD;
//	case SDLK_AC_STOP:                                 return KEY_AC_STOP;
//	case SDLK_AC_REFRESH:                              return KEY_AC_REFRESH;
//	case SDLK_AC_BOOKMARKS:                            return KEY_AC_BOOKMARKS;
//	case SDLK_BRIGHTNESSDOWN:                          return KEY_BRIGHTNESSDOWN;
//	case SDLK_BRIGHTNESSUP:                            return KEY_BRIGHTNESSUP;
//	case SDLK_DISPLAYSWITCH:                           return KEY_DISPLAYSWITCH;
//	case SDLK_KBDILLUMTOGGLE:                          return KEY_KBDILLUMTOGGLE;
//	case SDLK_KBDILLUMDOWN:                            return KEY_KBDILLUMDOWN;
//	case SDLK_KBDILLUMUP:                              return KEY_KBDILLUMUP;
//	case SDLK_EJECT:                                   return KEY_EJECT;
//	case SDLK_SLEEP:                                   return KEY_SLEEP;
	default:                                           return KEY_NONE;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Initializes the surface, creating a window and D3D device
//-----------------------------------------------------------------------------
CTopLevelWindowSDL * CTopLevelWindowSDL::FindWindowForSDLWindow( SDL_Window *hWindow )
{
	int iIndex = s_MapWindowInstances.Find( hWindow );
	if ( iIndex == s_MapWindowInstances.InvalidIndex() )
		return NULL;

	return s_MapWindowInstances[iIndex];
}


//-----------------------------------------------------------------------------
// Purpose: Clear key repeats
//-----------------------------------------------------------------------------
void CTopLevelWindowSDL::ClearRepeats( KeyCode code )
{
	m_mapKeyRepeats.Remove( code );
}


//-----------------------------------------------------------------------------
// Purpose: Track key repeats
//-----------------------------------------------------------------------------
int CTopLevelWindowSDL::IncrementRepeats( KeyCode code )
{
	int iMap = m_mapKeyRepeats.Find( code );
	if ( iMap == m_mapKeyRepeats.InvalidIndex() )
	{
		m_mapKeyRepeats.Insert( code, 1 );
		return 1;
	}
	else
	{
		return (++m_mapKeyRepeats[ iMap ]);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CTopLevelWindowSDL::CTopLevelWindowSDL( CUIEngine *pUIEngineParent ) :
		CTopLevelWindow( pUIEngineParent )
{
	m_p3DSurface = NULL;
	m_pRenderEngine = NULL;
	m_bFixedSurfaceSize = false;
	m_bEnforceWindowAspectRatio = false;
	m_unSurfaceWidth = 1920;
	m_unSurfaceHeight = 1280;
	m_unWindowWidth = 1920;
	m_unWindowHeight = 1280;
	m_bMouseOverWindow = false;
	m_eRenderTarget = IUIEngine::k_ERenderTargetUnset;
	m_bHidden = false;
	m_unSDLTickCreateTime = SDL_GetTicks();
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CTopLevelWindowSDL::~CTopLevelWindowSDL()
{
	Shutdown();
}


//-----------------------------------------------------------------------------
// Purpose: Run a frame, pumps input, paints, whatever.
//-----------------------------------------------------------------------------
void CTopLevelWindowSDL::PumpMessageLoop()
{
	VPROF_BUDGET( "CTopLevelWindowSDL::PumpMessageLoop", VPROF_BUDGETGROUP_TENFOOT );
	// Do this first, because message loop could delete us

	InputMessage_t input;
	CTopLevelWindowSDL *pWindow = NULL;
	KeyCode code = KEY_NONE;
	int nEventsProcessed = 0;

	bool bQuitApp = false;
	SDL_Event ev;
	// normally this is done by SDL_GL_SwapBuffers but we're doing glXSwapBuffers instead because render is on another thread
	SDL_PumpEvents();

#if defined( OSX )
	// Once a second, check if windows are onscreen or offscreen (in an inactive Space)
	static CRTime s_rtimeLastActiveSpaceCheck;
	if ( s_rtimeLastActiveSpaceCheck.CSecsPassed() >= 1 )
	{
		s_rtimeLastActiveSpaceCheck.SetToCurrentTime();
		FOR_EACH_MAP_FAST( s_MapWindowInstances, iMap )
		{
			pWindow = s_MapWindowInstances[iMap];
			bool bIsOnActiveSpace = OSXHelpers::BIsNSWindowOnActiveSpace( pWindow->m_hWindowRef );
			if ( pWindow->m_bIsOnActiveSpace != bIsOnActiveSpace )
			{
				pWindow->m_bIsOnActiveSpace = bIsOnActiveSpace;
				if ( !bIsOnActiveSpace )
				{
					UIEngine()->DispatchEventAsync( 0.0f, WindowOffScreen::MakeEvent( NULL, (CTopLevelWindow *)pWindow ) );
				}
				else
				{
					UIEngine()->DispatchEventAsync( 0.0f, WindowOnScreen::MakeEvent( NULL, (CTopLevelWindow *)pWindow ) );
				}
			}
		}
	}
#endif

//	while ( SDL_PollEvent( &ev ) > 0 && nEventsProcessed < 5 )
	while ( !bQuitApp && SDL_PollEvent( &ev ) > 0 )
	{
		nEventsProcessed++;
		pWindow = NULL;
		code = KEY_NONE;

		switch ( ev.type )
		{
		case SDL_QUIT:
			FOR_EACH_MAP_FAST( s_MapWindowInstances, i )
			{
				if ( s_MapWindowInstances[i]->GetSDLTickCreateTime() <= ev.common.timestamp )
				{
					// Will remove itself from map in the delete
					delete s_MapWindowInstances[i];
				}
			}
			if( s_MapWindowInstances.Count() == 0 )
				bQuitApp = true;

			LOG_SDL_EVENT("SDL_QUIT\n");
			break;

		case SDL_WINDOWEVENT:
			pWindow = FindWindowForSDLWindow( SDL_GetWindowFromID( ev.window.windowID ) );
			if ( pWindow == NULL )
				break;
			switch(ev.window.event)
			{
			case SDL_WINDOWEVENT_SHOWN:
				UIEngine()->DispatchEventAsync( 0.0f, WindowShown::MakeEvent( NULL, (CTopLevelWindow *)pWindow ) );
				pWindow->m_bHidden = false;
				LOG_SDL_EVENT( "SDL_WINDOWEVENT_SHOWN in %i\n", ev.window.windowID );
				break;
			case SDL_WINDOWEVENT_HIDDEN:
				UIEngine()->DispatchEventAsync( 0.0f, WindowHidden::MakeEvent( NULL, (CTopLevelWindow *)pWindow ) );
				pWindow->m_bHidden = true;
				LOG_SDL_EVENT( "SDL_WINDOWEVENT_HIDDEN in %i\n", ev.window.windowID );
				break;
			case SDL_WINDOWEVENT_EXPOSED:
				LOG_SDL_EVENT( "SDL_WINDOWEVENT_EXPOSED in %i\n", ev.window.windowID );
				break;
			case SDL_WINDOWEVENT_MOVED:
				LOG_SDL_EVENT( "SDL_WINDOWEVENT_MOVED in %i\n", ev.window.windowID );
				break;
			case SDL_WINDOWEVENT_RESIZED:
				LOG_SDL_EVENT( "SDL_WINDOWEVENT_RESIZED in %i\n", ev.window.windowID );
				pWindow->OnWindowResize(ev.window.data1, ev.window.data2);
				break;
			case SDL_WINDOWEVENT_MINIMIZED:
				LOG_SDL_EVENT( "SDL_WINDOWEVENT_MINIMIZED in %i\n", ev.window.windowID );
				break;
			case SDL_WINDOWEVENT_MAXIMIZED:
				LOG_SDL_EVENT( "SDL_WINDOWEVENT_MAXIMIZED in %i\n", ev.window.windowID );
				break;
			case SDL_WINDOWEVENT_RESTORED:
				LOG_SDL_EVENT( "SDL_WINDOWEVENT_RESTORED in %i\n", ev.window.windowID );
				break;
			case SDL_WINDOWEVENT_ENTER:
				pWindow->OnMouseEnter();
				input.m_eSource = k_ePanelEventSourceMouse;
				input.m_eInputType = k_eMouseEnter;
				pWindow->UIWindowInput()->InputEvent( input );
				break;
			case SDL_WINDOWEVENT_LEAVE:
				pWindow->OnMouseLeave();
				input.m_eSource = k_ePanelEventSourceMouse;
				input.m_eInputType = k_eMouseLeave;
				pWindow->UIWindowInput()->InputEvent( input );
				break;
			case SDL_WINDOWEVENT_FOCUS_GAINED:
				if ( pWindow->BUseSystemCursor() )
					SDL_ShowCursor( SDL_ENABLE );
				else
					SDL_ShowCursor( SDL_DISABLE );

				pWindow->UIWindowInput()->GotWindowFocus();
				pWindow->OnGotFocus();
				LOG_SDL_EVENT( "SDL_WINDOWEVENT_FOCUS_GAINED in %i\n", ev.window.windowID );
				break;
			case SDL_WINDOWEVENT_FOCUS_LOST:
				SDL_ShowCursor( SDL_ENABLE );
				pWindow->UIWindowInput()->LostWindowFocus();
				pWindow->OnLostFocus();

				LOG_SDL_EVENT( "SDL_WINDOWEVENT_FOCUS_LOST in %i\n", ev.window.windowID );
				break;
			case SDL_WINDOWEVENT_CLOSE:
				//LOG_SDL_EVENT( "SDL_WINDOWEVENT_CLOSE in %i\n", ev.window.windowID );
				// forestw: we shut down by deleting the top level window
				delete pWindow;
				return;
			default:
				LOG_SDL_EVENT( "SDL_WINDOWEVENT type %i (unknown) in %i\n", ev.window.event, ev.window.windowID );
				break;
			}
			break;
		case SDL_SYSWMEVENT:
			LOG_SDL_EVENT( "SDL_SYSWMEVENT\n" );
			break;
	
		case SDL_KEYDOWN:
			pWindow = FindWindowForSDLWindow( SDL_GetWindowFromID( ev.key.windowID ) );
			if ( pWindow == NULL )
				break;
			code = SDLKeysymToKeyCode( ev.key.keysym );
			if ( code == KEY_NONE )
				break;

			if ( pWindow->UIWindowInput()->BHasWindowFocus() )
			{
#ifdef OSX
				// OSX standard fullscreen key is Ctrl-Cmd-F, alternate key that we also use is Cmd-/
				if ( ( code == KEY_F && ( GetInputModifiers() & ( MODIFIER_LCONTROL | MODIFIER_RCONTROL ) ) && ( GetInputModifiers() & ( MODIFIER_LWIN | MODIFIER_RWIN ) ) ) ||
					 ( code == KEY_SLASH && ( GetInputModifiers() & ( MODIFIER_LWIN | MODIFIER_RWIN ) ) ) )
#else
				if ( code == KEY_ENTER && ( GetInputModifiers() & ( MODIFIER_LALT | MODIFIER_RALT ) ) )
#endif
				{
					if( !DispatchEvent( ToggleFullscreen(), (IUIPanel *)NULL, pWindow, !pWindow->BIsFullscreen() ) )
						pWindow->SetFullscreen( !pWindow->BIsFullscreen() );
				}
			}

			input.m_eSource = k_ePanelEventSourceKeyboard;
			input.m_eInputType = k_eKeyDown;
			input.m_flInputTime = UIEngine()->GetCurrentFrameTime();
			input.m_KeyData.m_KeyCode = code;
			input.m_KeyData.m_UniChar = 0;
			input.m_KeyData.m_bFirstDown = ev.key.repeat == 0;
			input.m_KeyData.m_RepeatCount = 0;
			if ( !input.m_KeyData.m_bFirstDown )
				input.m_KeyData.m_RepeatCount = pWindow->IncrementRepeats( code );
			else
				pWindow->ClearRepeats( code );

			input.m_KeyData.m_Modifiers = GetInputModifiers();

			if ( code != KEY_NONE )
				pWindow->UIWindowInput()->InputEvent( input );

			if ( code == KEY_ENTER || code == KEY_BACKSPACE || code == KEY_PAD_ENTER  )
			{
				input.m_eSource = k_ePanelEventSourceKeyboard;
				input.m_eInputType = k_eKeyChar;
				input.m_flInputTime = UIEngine()->GetCurrentFrameTime();
				input.m_KeyData.m_KeyCode = KEY_NONE;
				input.m_KeyData.m_bFirstDown = true;
				input.m_KeyData.m_RepeatCount = 0;
				input.m_KeyData.m_Modifiers = GetInputModifiers();
				if ( code == KEY_ENTER || code == KEY_PAD_ENTER  )
					input.m_KeyData.m_UniChar = L'\r';
				else if ( code == KEY_BACKSPACE )
					input.m_KeyData.m_UniChar = L'\b';

				pWindow->UIWindowInput()->InputEvent( input );
			}
			break;
		case SDL_KEYUP:
			pWindow = FindWindowForSDLWindow( SDL_GetWindowFromID( ev.window.windowID ) );
			if ( pWindow == NULL )
				break;
			code = SDLKeysymToKeyCode( ev.key.keysym );
			input.m_eSource = k_ePanelEventSourceKeyboard;
			input.m_eInputType = k_eKeyUp;
			input.m_flInputTime = UIEngine()->GetCurrentFrameTime();
			input.m_KeyData.m_KeyCode = code;
			input.m_KeyData.m_bFirstDown = true;
			input.m_KeyData.m_Modifiers = GetInputModifiers();
			input.m_KeyData.m_RepeatCount = 0;

			if ( code != KEY_NONE )
				pWindow->UIWindowInput()->InputEvent( input );
			break;
		case SDL_TEXTEDITING:
			pWindow = FindWindowForSDLWindow( SDL_GetWindowFromID( ev.edit.windowID ) );
			if ( pWindow == NULL )
				break;
			LOG_SDL_EVENT( "SDL_TEXTEDITING in %i\n", ev.edit.windowID );
			break;
		case SDL_TEXTINPUT:
			pWindow = FindWindowForSDLWindow( SDL_GetWindowFromID( ev.text.windowID ) );
			if ( pWindow == NULL )
				break;
			input.m_eSource = k_ePanelEventSourceKeyboard;
			input.m_eInputType = k_eKeyChar;
			input.m_flInputTime = UIEngine()->GetCurrentFrameTime();
			input.m_KeyData.m_KeyCode = KEY_NONE;
			input.m_KeyData.m_bFirstDown = true;
			input.m_KeyData.m_RepeatCount = 0;
			input.m_KeyData.m_Modifiers = GetInputModifiers();
			{
				char rgchTextWithNull[sizeof( ev.text.text ) + 1];
				V_memcpy( rgchTextWithNull, ev.text.text, sizeof( ev.text.text ) );
				rgchTextWithNull[sizeof( ev.text.text )] = 0;

				uchar32 rgch32Text[V_ARRAYSIZE( rgchTextWithNull )];
				V_UTF8ToUTF32( rgchTextWithNull, rgch32Text, sizeof( rgch32Text ), STRINGCONVERT_ASSERT_REPLACE );
				for ( int i = 0; i < V_ARRAYSIZE( rgch32Text ) && rgch32Text[i]; ++i )
				{
					input.m_KeyData.m_UniChar = rgch32Text[i];
					pWindow->UIWindowInput()->InputEvent( input );
				}
			}
			break;
		case SDL_MOUSEMOTION:
			pWindow = FindWindowForSDLWindow( SDL_GetWindowFromID( ev.motion.windowID ) );
			if ( pWindow )
				pWindow->UIWindowInput()->OnMouseMove( ( float )ev.motion.x, ( float )ev.motion.y );
			break;
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			pWindow = FindWindowForSDLWindow( SDL_GetWindowFromID( ev.button.windowID ) );
			if ( pWindow == NULL )
				break;
			{
				EInputType eInputType = k_eInputNone;
				switch ( ev.type )
				{
				case SDL_MOUSEBUTTONDOWN:
				{
					switch ( ev.button.clicks )
					{
					case 1:
						eInputType = k_eMouseDown;
						break;
					case 2:
						eInputType = k_eMouseDoubleClick;
						break;
					case 3:
						eInputType = k_eMouseTripleClick;
						break;
					default:
						eInputType = k_eInputNone;
						break;
					}
					break;
				}
				case SDL_MOUSEBUTTONUP:
					eInputType = k_eMouseUp;
					break;
				}

				MouseCode mouseCode = MOUSE_INVALID;
				switch ( ev.button.button )
				{
				case 1:
					mouseCode = MOUSE_LEFT;
					break;
				case 2:
					mouseCode = MOUSE_MIDDLE;
					break;
				case 3:
					mouseCode = MOUSE_RIGHT;
					break;
#ifdef LINUX
				case 8:
					mouseCode = MOUSE_4;
					break;
				case 9:
					mouseCode = MOUSE_5;
					break;
#else
				case 4:
					mouseCode = MOUSE_4;
					break;
				case 5:
					mouseCode = MOUSE_5;
					break;
#endif
				}

				if ( eInputType == k_eInputNone || mouseCode == MOUSE_INVALID )
				{
					LOG_SDL_EVENT( "Unknown SDL_MOUSEBUTTONDOWN/SDL_MOUSEBUTTONUP %d in %i\n", ev.button.button, ev.button.windowID );
					break;
				}

				// update mouse location before handling the button event
				pWindow->UIWindowInput()->OnMouseMove( ( float )ev.button.x, ( float )ev.button.y );

				input.m_eSource = k_ePanelEventSourceMouse;
				input.m_eInputType = eInputType;
				input.m_MouseData.m_MouseCode = mouseCode;
				input.m_MouseData.m_Modifiers = GetInputModifiers();
				input.m_MouseData.m_Delta = 0;
				input.m_MouseData.m_RepeatCount = 0;

				float xMouse = ev.button.x;
				float yMouse = ev.button.y;

				pWindow->ConvertClientToSurfaceCoord( &xMouse, &yMouse );
				input.m_MouseData.m_XPos = xMouse;
				input.m_MouseData.m_YPos = yMouse;

				pWindow->UIWindowInput()->InputEvent( input );
			}
			break;
		case SDL_MOUSEWHEEL:
			{
			pWindow = FindWindowForSDLWindow( SDL_GetWindowFromID( ev.wheel.windowID ) );
			if ( pWindow == NULL )
				break;
			LOG_SDL_EVENT( "SDL_MOUSEWHEEL in window %i\n", ev.wheel.windowID );

			int rawscroll = (ev.wheel.y) + g_nMouseDeltaLeftOvers;
			int delta = rawscroll/SDL_WHEEL_DELTA;
			g_nMouseDeltaLeftOvers = rawscroll % SDL_WHEEL_DELTA;
				
			// account for windows scroll lines setting
			delta *= UIEngine()->GetWheelScrollLines();
				
			input.m_eSource = k_ePanelEventSourceMouse;
			input.m_eInputType = k_eMouseWheel;
			input.m_MouseData.m_MouseCode = MOUSE_LEFT;
			input.m_MouseData.m_Modifiers = GetInputModifiers();
			input.m_MouseData.m_Delta = delta;
			pWindow->GetMouseWheelRepeats( delta < 0, abs(delta), input.m_MouseData.m_RepeatCount );
			pWindow->UIWindowInput()->InputEvent( input );
			}
			break;

		case SDL_JOYAXISMOTION:
			// we can't actually process axis motions because we deliver two-axis values
//			LOG_SDL_EVENT( "SDL_JOYAXISMOTION %i %i %i\n", ev.jaxis.which, ev.jaxis.axis, ev.jaxis.value );
			break;
		case SDL_JOYBALLMOTION:
//			LOG_SDL_EVENT( "SDL_JOYBALLMOTION %i %i delta %i,%i\n", ev.jball.which, ev.jball.ball, ev.jball.xrel, ev.jball.yrel );
			break;
		case SDL_JOYHATMOTION:
//			LOG_SDL_EVENT( "SDL_JOYHATMOTION %i %i %i\n", ev.jhat.which, ev.jhat.hat, ev.jhat.value );
			break;
		case SDL_JOYBUTTONDOWN:
//			LOG_SDL_EVENT( "SDL_JOYBUTTONDOWN %i %i = %i\n", ev.jbutton.which, ev.jbutton.button, ev.jbutton.state );
			break;
		case SDL_JOYBUTTONUP:
//			LOG_SDL_EVENT( "SDL_JOYBUTTONUP   %i %i = %i\n", ev.jbutton.which, ev.jbutton.button, ev.jbutton.state );
			break;

		case SDL_FINGERDOWN:
//			LOG_SDL_EVENT( "SDL_FINGERDOWN in %i\n", ev.tfinger.windowID );
			break;
		case SDL_FINGERUP:
//			LOG_SDL_EVENT( "SDL_FINGERUP in %i\n", ev.tfinger.windowID );
			break;
		case SDL_FINGERMOTION:
//			LOG_SDL_EVENT( "SDL_FINGERMOTION in %i\n", ev.tfinger.windowID );
			break;
		default:
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set window position
//-----------------------------------------------------------------------------
void CTopLevelWindowSDL::SetWindowPosition( float x, float y )
{
	SDL_SetWindowPosition( m_hSDLWindow, x, y );
}


//-----------------------------------------------------------------------------
// Purpose: Get window position and size
//-----------------------------------------------------------------------------
void CTopLevelWindowSDL::GetWindowBounds( float &left, float &top, float &right, float &bottom )
{
	GetWindowPosition( left, top );
	right = left + m_unWindowWidth;
	bottom = left + m_unWindowHeight;
}


//-----------------------------------------------------------------------------
// Purpose: Get position and size of window's client area
//-----------------------------------------------------------------------------
void CTopLevelWindowSDL::GetClientDimensions( float &width, float &height )
{
	int wide, tall;
	SDL_GetWindowSize( m_hSDLWindow, &wide, &tall );
	width = wide;
	height = tall;
}


//-----------------------------------------------------------------------------
// Purpose: Get window position
//-----------------------------------------------------------------------------
void CTopLevelWindowSDL::GetWindowPosition( float &x, float &y )
{
	int iX, iY;
	SDL_GetWindowPosition( m_hSDLWindow, &iX, &iY );
	x = iX;
	y = iY;
}


//-----------------------------------------------------------------------------
// Purpose: Activates window, bringing to foreground
//-----------------------------------------------------------------------------
void CTopLevelWindowSDL::Activate( bool bForceful )
{
	REFERENCE( bForceful );
	SDL_ShowWindow( m_hSDLWindow );
#ifdef OSX
	if ( bForceful )
	{
		OSXHelpers::ForceActivateThisAppAndRaiseNSWindow( m_hWindowRef );
	}
#endif
	SDL_RaiseWindow( m_hSDLWindow );
}


//-----------------------------------------------------------------------------
// Purpose: Minimizes window
//-----------------------------------------------------------------------------
void CTopLevelWindowSDL::Minimize()
{
	SDL_MinimizeWindow( m_hSDLWindow );
}


//-----------------------------------------------------------------------------
// Purpose: Shutdown/close this window
//-----------------------------------------------------------------------------
void CTopLevelWindowSDL::Shutdown()
{
	if ( m_hSDLWindow
#ifdef OSX	// Note: HideWindow on active fullscreen window triggers an awful double-transition
		&& m_eRenderTarget != IUIEngine::k_ERenderBorderlessFullScreenWindow
#endif
		)
	{
		SDL_HideWindow( m_hSDLWindow );
	}
	
	CTopLevelWindow::Shutdown();

	if ( m_pRenderEngine )
	{
		delete m_pRenderEngine;
		m_pRenderEngine = NULL;
	}
	
	s_MapWindowInstances.Remove( m_hSDLWindow );

	// unlike the D3D implementation, we must destroy the surface before the window or it winds up trying to destroy resources on the wrong context
	if ( m_p3DSurface )
	{
		delete m_p3DSurface;
		m_p3DSurface = NULL;
	}

	if ( m_hGLContext )
		SDL_GL_DeleteContext( m_hGLContext );

	FOR_EACH_MAP_FAST( m_MapCursors, i )
	{
		SDL_FreeCursor( m_MapCursors[i] );
	}
	m_MapCursors.RemoveAll();

	SDL_ShowCursor( SDL_ENABLE );

	if ( m_hSDLWindow )
	{
		SDL_DestroyWindow( m_hSDLWindow );
		m_hSDLWindow = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Initializes the surface, creating a window and D3D device
//-----------------------------------------------------------------------------
bool CTopLevelWindowSDL::BInitializeSurface( const char *pchWindowTitle, int nWidth, int nHeight, IUIEngine::ERenderTarget eRenderTarget, bool bFixedSurfaceSize, bool bEnforceWindowAspectRatio, bool bUseCustomMouseCursor, const char *pchTargetMonitor )
{
	m_strTargetMonitor = pchTargetMonitor;
	m_bFixedSurfaceSize = bFixedSurfaceSize;
	m_bEnforceWindowAspectRatio = bEnforceWindowAspectRatio;
	m_bUseCustomMouseCursor = bUseCustomMouseCursor;
	m_unWindowWidth = nWidth;
	m_unWindowHeight = nHeight;
	m_unSurfaceWidth = nWidth;
	m_unSurfaceHeight = nHeight;
	m_eRenderTarget = eRenderTarget;
	
	SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
	SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 8 );
	SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 8 );
	SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 8 );
	SDL_GL_SetAttribute( SDL_GL_ALPHA_SIZE, 8 );
	SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, 24 );
#if GL_DEBUG
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG );
#endif

	int nWindowX = SDL_WINDOWPOS_UNDEFINED;
	int nWindowY = SDL_WINDOWPOS_UNDEFINED;

	SDL_Rect rect;
	int windowflags = SDL_WINDOW_OPENGL;
	rect.x = nWindowX;
	rect.y = nWindowY;
	rect.w = m_unWindowWidth;
	rect.h = m_unWindowHeight;
	if ( eRenderTarget == IUIEngine::k_ERenderFullScreen )
		windowflags |= SDL_WINDOW_FULLSCREEN;

	if ( eRenderTarget == IUIEngine::k_ERenderBorderlessFullScreenWindow )
	{
#ifdef OSX
		// Create as borderless but not fullscreen now, and set fullscreen after creation.
		// Make sure that we take up the full screen rectangle initially so that the OS
		// animation to enter fullscreen mode is much less jarring.
		windowflags |= SDL_WINDOW_BORDERLESS;
		SDL_GetDisplayBounds( 0, &rect );
#else
		windowflags |= ( SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_BORDERLESS );
#endif
	}
	else if ( eRenderTarget == IUIEngine::k_ERenderToWindow )
	{
		windowflags |= SDL_WINDOW_RESIZABLE;
	}

	m_unWindowWidth = rect.w;
	m_unWindowHeight = rect.h;
	m_hSDLWindow = SDL_CreateWindow( pchWindowTitle, rect.x, rect.y, rect.w, rect.h, windowflags );
	if ( !m_hSDLWindow )
	{
		Warning( "SDL_CreateWindow failed" );
		return false;
	}
	
	SetIcon();

	// SDL's GL context type is just a regular GLXContext
	m_hGLContext = SDL_GL_CreateContext( m_hSDLWindow );
	if ( !m_hGLContext )
	{
		Warning( "SDL_GL_CreateContext failed" );
		return false;
	}

#if defined(WIN32) && !defined(SOURCE2_PANORAMA)
	InitGLFunctionPointers();
#endif

	// restore the old context because we're managing contexts with GLX directly
	// get the X11 variables out of SDL because we use them ourselves...
	SDL_SysWMinfo SDLWMinfo;
	memset( &SDLWMinfo, 0, sizeof( SDLWMinfo ) );
	SDL_VERSION( &SDLWMinfo.version );
	if ( !SDL_GetWindowWMInfo( m_hSDLWindow, &SDLWMinfo ) )
		Warning(" SDL_GetWindowWMInfo failed" );
	
	// blank the window to black
	glClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
	glClear( GL_COLOR_BUFFER_BIT );
	SDL_GL_SwapWindow( m_hSDLWindow );

#ifdef LINUX
	if ( SDLWMinfo.subsystem != SDL_SYSWM_X11 )
		Error(" SDL_GetWindowWMInfo returned an unsupported subsystem (we require X11)" );
	Display *pXDisplay = SDLWMinfo.info.x11.display;
	m_XWindow = SDLWMinfo.info.x11.window;
	m_pXDisplay = pXDisplay;

	XColor black;
	Colormap cmap = DefaultColormap( m_pXDisplay, DefaultScreen( m_pXDisplay ) );
	XColor dummy;
	XAllocNamedColor( m_pXDisplay, cmap, "black", &black, &dummy );
	
	if ( bEnforceWindowAspectRatio )
	{
		XSizeHints *pSizeHints = XAllocSizeHints();
		
		if ( pSizeHints )
		{
			pSizeHints->flags = PAspect;
			pSizeHints->min_aspect.x = m_unSurfaceWidth;
			pSizeHints->min_aspect.y = m_unSurfaceHeight;
			pSizeHints->max_aspect = pSizeHints->min_aspect;
			
			XSetWMNormalHints( m_pXDisplay, m_XWindow, pSizeHints );
			
			XFree( pSizeHints );
		}
	}
	
	XRaiseWindow( m_pXDisplay, m_XWindow );
	
#elif defined(OSX)
	if ( SDLWMinfo.subsystem != SDL_SYSWM_COCOA )
		Warning(" SDL_GetWindowWMInfo returned an unsupported subsystem (we require Cocoa)" );

	m_hWindowRef = (void *)SDLWMinfo.info.cocoa.window;
	m_bIsOnActiveSpace = true;
	
	OSXHelpers::RaiseNSWindow( m_hWindowRef );
	
#elif defined(WIN32)
	if ( SDLWMinfo.subsystem != SDL_SYSWM_WINDOWS )
		Warning(" SDL_GetWindowWMInfo returned an unsupported subsystem (we require Cocoa)" );

	m_hWindowRef = SDLWMinfo.info.win.window;
#else
#error
#endif

	SDL_GL_MakeCurrent( m_hSDLWindow, NULL );

	// print the SDL versions we're using, just as an informative hint
	SDL_version SDLCompiledVersion;
	SDL_VERSION( &SDLCompiledVersion );
	SDL_version SDLLinkedVersion;
	SDL_GetVersion( &SDLLinkedVersion );
	Msg( "Compiled against SDL version %d.%d.%d and linked against SDL version %d.%d.%d\n", SDLCompiledVersion.major, SDLCompiledVersion.minor, SDLCompiledVersion.patch, SDLLinkedVersion.major, SDLLinkedVersion.minor, SDLLinkedVersion.patch );

	// Other housekeeping
	s_MapWindowInstances.Insert( m_hSDLWindow, this );

	// Turn off the system cursor, since there is a custom app level cursor
	if ( m_bUseCustomMouseCursor )
		SDL_ShowCursor( SDL_DISABLE );

	// Now initialize the 3d surface
#if !defined(SOURCE2_PANORAMA)
	COpenGLSurface *pSurface = new COpenGLSurface();
	if ( !pSurface->BInitialize( m_hSDLWindow, m_hGLContext, m_unSurfaceWidth, m_unSurfaceHeight, m_unWindowWidth, m_unWindowHeight, eRenderTarget, bEnforceWindowAspectRatio, bFixedSurfaceSize, m_pCursorRender ) )
	{
		delete pSurface;
		// TODO need more cleanup than this!
		return false;
	}

	m_pSurfaceInterface = pSurface;
	m_pSurfaceInterface->SetWindowScaleFactor( GetWindowScaleFactor() );

	m_p3DSurface = pSurface;
	m_pRenderEngine = new CUIRenderEngine( m_pUIEngineParent, m_p3DSurface, m_pInputEngine, this, m_unSurfaceWidth, m_unSurfaceHeight );
	
	if ( eRenderTarget == IUIEngine::k_ERenderFullScreen )
	{
		SDL_SetWindowFullscreen( m_hSDLWindow, SDL_WINDOW_FULLSCREEN_DESKTOP );
	}
	
#ifdef OSX
	if ( eRenderTarget == IUIEngine::k_ERenderBorderlessFullScreenWindow )
	{
		// Window must be shown, raised, and key-focused before going fullscreen
		// or things can go wonky on OSX and you get a black screen -henryg 11/7/2015
		Activate( true );
		SDL_PumpEvents();
		SDL_SetWindowFullscreen( m_hSDLWindow, SDL_WINDOW_FULLSCREEN_DESKTOP );
	}
#endif
	
	return true;

#else
	AssertMsg( false, "Source2 needs surface implementation!" );
	return false;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Load the window icon from disk and set it
//-----------------------------------------------------------------------------
void
CTopLevelWindowSDL::SetIcon()
{
	CUtlBuffer bufIcon;
	CUtlString iconPath = UIEngine()->GetLocalPathForNamedPath( "{images}" );
	iconPath += "steam_tray_48.tga";

	UIEngine()->UIFileSystem()->LoadFileIntoBuffer(iconPath.Get(), bufIcon, false);
	if ( bufIcon.TellPut() != 0 )
	{
		byte *rawImage;
		int rawImageBytes, width, height;
		if ( LoadTGA(bufIcon.TellPut(), 
					 (char *)bufIcon.Base(),
					 &rawImage,
					 &rawImageBytes,
					 &width,
					 &height) )
		{
			SDL_Surface *pIcon = SDL_CreateRGBSurfaceFrom(rawImage,width,height,32,width*4,0xff,0xff00,0xff0000,0xff000000);
			if ( pIcon )
			{
				SDL_SetWindowIcon(m_hSDLWindow, pIcon);
				SDL_FreeSurface(pIcon);
			}
			delete[] rawImage;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Resize 3D surface
//-----------------------------------------------------------------------------
void CTopLevelWindowSDL::OnWindowResize( uint32 width, uint32 height )
{
	if ( m_p3DSurface )
	{
		m_unWindowWidth = width;
		m_unWindowHeight = height;

		if ( !m_bFixedSurfaceSize )
		{
			if ( !m_bEnforceWindowAspectRatio )
			{
				m_unSurfaceWidth = width;
				m_unSurfaceHeight = height;
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
}


//-----------------------------------------------------------------------------
// Purpose: Called when the mouse is moving over our window
//-----------------------------------------------------------------------------
void CTopLevelWindowSDL::OnMouseEnter()
{
	if ( m_bMouseOverWindow )
		return;
	
	m_bMouseOverWindow = true;
}


//-----------------------------------------------------------------------------
// Purpose: set the mouse cursor to this cursor please
//-----------------------------------------------------------------------------
void CTopLevelWindowSDL::SetMouseCursor( EMouseCursors eCursor )
{
	if ( m_eCursorCurrent == eCursor )
		return;

	VPROF_BUDGET( "CTopLevelWindowSDL::SetMouseCursor", VPROF_BUDGETGROUP_TENFOOT );
	
	m_eCursorCurrent = eCursor; // store off the current cursor, the renderer may want to use it

	if ( m_bUseCustomMouseCursor )
		return; // manually pushed by the renderer instead of windows
	
	SDL_ShowCursor( SDL_ENABLE );

	SDL_Cursor *pCursor = NULL;
	int iMap = m_MapCursors.Find( eCursor );
	if ( iMap != m_MapCursors.InvalidIndex() )
		pCursor = m_MapCursors[iMap];
	else
	{
		switch ( eCursor )
		{
		default:
		case eMouseCursor_Arrow:
			pCursor = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_ARROW );
			break;
		case eMouseCursor_IBeam:
			pCursor = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_IBEAM );
			break;
		case eMouseCursor_SizeWE:
			pCursor = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_SIZEWE );
			break;
		case eMouseCursor_SizeNS:
			pCursor = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_SIZENS );
			break;
		case eMouseCursor_Hand:
			pCursor = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_HAND );
			break;
		}

		if ( pCursor )
			m_MapCursors.Insert( eCursor, pCursor );
	}

	SDL_SetCursor( pCursor );	
}


//-----------------------------------------------------------------------------
// Purpose: return true if this window has key focus
//-----------------------------------------------------------------------------
bool CTopLevelWindowSDL::BHasFocus()
{
	return ( SDL_GetWindowFlags( m_hSDLWindow ) & SDL_WINDOW_INPUT_FOCUS ) ? true : false ;
}


//-----------------------------------------------------------------------------
// Purpose: return true any part of the window is being rendered
//-----------------------------------------------------------------------------
bool CTopLevelWindowSDL::BIsVisible()
{
	if ( !m_p3DSurface )
		return false;
	
	if ( m_bHidden )
		return false;

#if defined( OSX )
	// m_bIsOnActiveSpace can lag behind by a second; combining it with focus is more accurate
	if ( !m_bIsOnActiveSpace && !BHasFocus() )
		return false;
#endif

	return ! m_p3DSurface->BSurfaceOccluded();
}


//-----------------------------------------------------------------------------
// Purpose: return true any part of the window is being rendered
//-----------------------------------------------------------------------------
void* CTopLevelWindowSDL::GetNativeWindowHandle()
{
#ifdef LINUX
	return (void*)m_XWindow;
#elif defined(OSX)
	return m_hWindowRef;
#elif defined(WIN32)
	return m_hWindowRef;
#else
#error Not yet implemented
#endif
}


void CTopLevelWindowSDL::ForceHideWindow()
{
	if ( m_hSDLWindow
#ifdef OSX	// Note: HideWindow on active fullscreen window triggers an awful double-transition
		&& m_eRenderTarget != IUIEngine::k_ERenderBorderlessFullScreenWindow
#endif
		)
	{
		SDL_HideWindow( m_hSDLWindow );
	}
}

//-----------------------------------------------------------------------------
// Purpose: force back topmost
//-----------------------------------------------------------------------------
void CTopLevelWindowSDL::SetPreventForceWindowOnTop( bool bPreventForceTopLevel )
{
	CTopLevelWindow::SetPreventForceWindowOnTop( bPreventForceTopLevel );

	if ( !bPreventForceTopLevel )
	{
		SDL_RestoreWindow( m_hSDLWindow );
		SDL_RaiseWindow( m_hSDLWindow ); // and force it top
	}
}


//-----------------------------------------------------------------------------
// Purpose: should we disable input for this window right now
//-----------------------------------------------------------------------------
bool CTopLevelWindowSDL::BAllowInput( InputMessage_t &msg )
{
	// Always allow guide button input
	if ( BIsGuideButton( msg ) )
		return true;

	COpenGLSurface *pSDLOpenGLSurface = assert_cast<COpenGLSurface *>( m_p3DSurface );
	if ( pSDLOpenGLSurface && !pSDLOpenGLSurface->BHasVTFocus() )
		return false;
	return CTopLevelWindow::BAllowInput( msg );
}


//-----------------------------------------------------------------------------
// Purpose: user tabbed away, stop being top most
//-----------------------------------------------------------------------------
void CTopLevelWindowSDL::OnLostFocus()
{
}


//-----------------------------------------------------------------------------
// Purpose: got focus back, come on top
//-----------------------------------------------------------------------------
void CTopLevelWindowSDL::OnGotFocus()
{
}



#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: validate mem
//-----------------------------------------------------------------------------
void CTopLevelWindowSDL::Validate( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE();

	// ValidatePtr( (CD3D10D2DSurface *)m_p3DSurface );
	CTopLevelWindow::Validate( validator, pchName );
}

//-----------------------------------------------------------------------------
// Purpose: validate mem
//-----------------------------------------------------------------------------
void CTopLevelWindowSDL::ValidateStatics( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE_STATIC( "CTopLevelWindowSDL" );
	ValidateObj( s_MapWindowInstances );
}

#endif
